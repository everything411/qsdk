// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../../core.h"
#include "../../debug.h"
#include "../../dp_rx.h"
#include "../../hif.h"
#include "../../dp_cmn.h"
#include "../../telemetry_agent_if.h"
#include "dp_rx.h"
#include "dp.h"
#include "hal.h"
#include "dp_mon.h"

static int ath12k_wifi6_dp_service_srng(struct ath12k_dp *dp,
					struct ath12k_ext_irq_grp *irq_grp,
					int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0, j = 0;
	int tot_work_done = 0;
	struct hal_srng *refill_srng;
	u8 ring_mask, rx_mask;

	rx_mask = dp->hw_params->ring_mask->rx[grp_id];

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi6_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_wbm_rel[grp_id]) {
		work_done = ath12k_wifi6_dp_rx_process_wbm_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;

		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi6_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_mon_status[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_status[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_rx_mon_process_ring(dp, id,
								      napi, budget);
					budget -= work_done;
					tot_work_done += work_done;
					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->rx_mon_dest[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_rx_mon_process_ring(dp, id,
								      napi, budget);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->reo_status[grp_id]) {
		ath12k_wifi6_dp_rx_process_reo_status(dp);
	}

	if (dp->hw_params->ring_mask->host2rxdma[grp_id]) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		LIST_HEAD(list);
		size_t req_entries;

		refill_srng = &dp->ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
		req_entries = ath12k_dp_get_req_entries_from_buf_ring(dp->ab, refill_srng,
				&list, 0);
		if (req_entries)
			ath12k_dp_rx_bufs_replenish(dp, refill_srng, &list, false);
	}

done:
	return tot_work_done;
}

static int ath12k_wifi6_dp_rings_init(struct ath12k_dp *dp, u32 n_link_desc)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	int ret;

	ret = ath12k_dp_srng_init(ab, &dp->wbm_idle_ring,
				  HAL_WBM_IDLE_LINK, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm_idle_ring: %d\n", ret);
		goto fail_out;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	ret = ath12k_dp_link_desc_init(ab, dp->link_desc_banks,
				       HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to init link desc: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_dp_srng_init(ab, &dp->wbm_desc_rel_ring,
				  HAL_SW2WBM_RELEASE, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm2sw_release ring: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_wifi6_dp_rx_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "rx ring init failed ret = %d\n", ret);
		goto fail_dp_rx_buf_free;
	}

	ret = ath12k_dp_mon_rx_init(dp);
	if (ret) {
		ath12k_warn(ab, "failed to init rxdma rings ret = %d\n", ret);
		goto fail_dp_mon_rx_deinit;
	}

	return 0;

fail_dp_mon_rx_deinit:
	ath12k_dp_mon_rx_deinit(dp);

fail_dp_rx_buf_free:
	ath12k_wifi6_dp_rx_buf_free(dp);

fail_out:
	return ret;
}

static int ath12k_wifi6_dp_op_device_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	u32 n_link_desc;
	int ret;

	ret = ath12k_hif_ext_irq_setup(dp->ab, ath12k_wifi6_dp_service_srng, dp);
	if (ret)
		return ret;

	dp->idle_link_rbm =
			ath12k_hal_get_idle_link_rbm(&ab->hal, ab->device_id);

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		/* For QCN9074, monitor link desc ring will be used,
		 * setting n_link_desc to minimum value */
		n_link_desc = HAL_LINK_DESC_SIZE;
	} else {
		n_link_desc = HAL_RX_MON_DESC_RING_SIZE;
	}

	ret = ath12k_wifi6_dp_rings_init(dp, n_link_desc);
	if (ret)
		goto fail_irq_cleanup;

	return 0;

fail_irq_cleanup:
	ath12k_hif_ext_irq_cleanup(dp->ab);

	return ret;
}

static void ath12k_wifi6_dp_op_device_deinit(struct ath12k_dp *dp)
{
	if (!dp->ab)
		return;

	ath12k_hif_ext_irq_cleanup(dp->ab);
	ath12k_dp_mon_rx_deinit(dp);
	ath12k_wifi6_dp_rx_buf_free(dp);
}

static struct ath12k_dp_hw_group *ath12k_wifi6_dp_hw_group_alloc(void)
{
	struct ath12k_dp_hw_group *dp_hw_grp;

	dp_hw_grp = kzalloc(sizeof(*dp_hw_grp), GFP_KERNEL);
	if (!dp_hw_grp)
		return NULL;
	return dp_hw_grp;
}

int ath12k_wifi6_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct wireless_dev *wdev;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (!params->is_vdev_peer) {
		ath12k_err(NULL, "Peer is not vdev peer for scan radio\n");
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer)
		return -ENOMEM;
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->vif = vif;
	dp_peer->sta = params->sta;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = params->is_mlo ? params->peer_id : ATH12K_DP_PEER_ID_INVALID;
	dp_peer->is_vdev_peer = params->is_vdev_peer;
	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer)
		dp_peer->hw_link_id = params->hw_link_id;
	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev)
		dp_peer->dev = wdev->netdev;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	/* Add ath12k_dp_peer to the linked list taking peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_add(&dp_peer->list, &dp_hw->peers);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	ath12k_dp_peer_hash_table_add(dp_hw, dp_peer);

	if (dp_peer->is_mlo && dp_peer->peer_id < MAX_DP_PEER_LIST_SIZE)
		rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	params->peer_id = ATH12K_DP_PEER_ID_INVALID;

	return 0;
}

void ath12k_wifi6_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id,
				 struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	u16 peerid_index;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	if (dp_peer->is_mlo) {
		peerid_index = dp_peer->peer_id;
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
	}

	ath12k_dp_peer_hash_table_delete(dp_hw, dp_peer);

	/* Remove ath12k_dp_peer from linked list holding peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_del(&dp_peer->list);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	synchronize_rcu();

	if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
		ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

	if (dp_peer->qos)
		kfree(dp_peer->qos);

	kfree(dp_peer);
}

int ath12k_wifi6_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr,
			       struct ath12k_dp_peer_create_params *params)
{
	return 0;
}


static struct ath12k_dp_arch_ops ath12k_wifi6_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi6_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi6_dp_op_device_deinit,
	.rx_link_desc_return = ath12k_wifi6_dp_rx_link_desc_return,
	.dp_hw_group_alloc = ath12k_wifi6_dp_hw_group_alloc,
	.dp_pdev_alloc = ath12k_wifi6_dp_pdev_alloc,
	.dp_pdev_free = ath12k_wifi6_dp_pdev_free,
	.dp_peer_create = ath12k_wifi6_dp_peer_create,
	.dp_peer_delete = ath12k_wifi6_dp_peer_delete,
	.dp_peer_assoc = ath12k_wifi6_dp_peer_assoc,
};

static int ath12k_wifi6_dp_rings_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	int ret;

	/* For QCN9074, monitor link desc ring will be used,
	 * setting n_link_desc to minimum value */
	ret = ath12k_dp_srng_alloc(ab, &dp->wbm_idle_ring,
				   HAL_WBM_IDLE_LINK, 0, 0,
				   ath12k_dp_mon_rxdma1_enable(dp) ?
				   HAL_LINK_DESC_SIZE : HAL_RX_MON_DESC_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to alloc wbm_idle_ring: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_dp_link_desc_alloc(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, NULL,
					ath12k_dp_mon_rxdma1_enable(dp) ?
					HAL_LINK_DESC_SIZE : HAL_RX_MON_DESC_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to alloc link desc: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->wbm_desc_rel_ring,
				   HAL_SW2WBM_RELEASE, 0, 0,
				   DP_WBM_RELEASE_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to alloc wbm_desc_rel_ring: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_wifi6_dp_rx_ring_alloc(ab);
	if (ret) {
		ath12k_warn(dp, "failed to alloc rx rings: %d\n", ret);
		goto fail_out;
	}

	ret = ath12k_dp_mon_rx_alloc(dp);
	if (ret) {
		ath12k_warn(dp, "failed to alloc mon rx rings: %d\n", ret);
		goto fail_out;
	}

	return 0;

fail_out:
	return ret;
}

/* TODO: remove export once this file is built with wifi6 ko */
struct ath12k_dp *ath12k_wifi6_dp_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_wifi6 *dp_wifi6;
	int ret;

	dp = kzalloc(sizeof(*dp) + sizeof(*dp_wifi6), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp_wifi6 = ath12k_get_dp_wifi6(dp);
	dp_wifi6->dp = dp;

	dp->arch_ops = &ath12k_wifi6_dp_arch_ops;

	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hal = &ab->hal;
	dp->global_peer_id_supported = false;

	/* Set ab->dp early so that ring alloc helpers that call
	 * ath12k_ab_to_dp(ab) during probe-time allocation find a
	 * valid pointer.  The caller overwrites ab->dp with our return
	 * value; if we fail and return NULL it will be set to NULL. */
	ab->dp = dp;

	ret = ath12k_dp_mon_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp_mon_init failed %d\n", ret);
		goto dp_err;
	}

	ath12k_wifi6_dp_mon_ops_register(dp);

	ret = ath12k_wifi6_dp_rings_alloc(dp);
	if (ret)
		goto dp_err;

	return dp;
dp_err:
	ath12k_wifi6_dp_deinit(dp);
	return NULL;
}

void ath12k_wifi6_dp_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab;

	if (!dp)
		return;

	ab = dp->ab;

	ath12k_dp_mon_rx_free(dp);

	if (ab) {
		ath12k_wifi6_dp_rx_ring_free(ab);
		ath12k_dp_srng_cleanup(ab, &dp->wbm_desc_rel_ring);
		ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
					    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);
	}

	ath12k_dp_mon_deinit(dp);
	kfree(dp);
}
