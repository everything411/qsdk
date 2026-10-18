// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <crypto/hash.h>
#include "core.h"
#include "dp_tx.h"
#include "hif.h"
#include "hal.h"
#include "debug.h"
#include "dp_rx.h"
#include "peer.h"
#include "dp_mon.h"
#include "dp_cmn.h"
#include "debugfs.h"
#include "dp_stats.h"
#include "dp_peer.h"
#include "hal.h"
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_QCN_EXTN */
#endif
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa_pub.h"
#endif /* CPTCFG_QCN_EXTN */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif
#include "dp.h"
#include <linux/vmalloc.h>

/* DSCP TID mapping as per RFC 8325
 * ===============================
 * DSCP         User Priority (UP)
 * ===============================
 * 56           7
 * 48           7
 * 46           6
 * 44           6
 * 40           5
 * 34, 36, 38   4
 * 32           4
 * 26, 28, 30   4
 * 24           4
 * 18, 20, 22   3
 * 16           0
 * 10, 12, 14   0
 * 0            0
 * 8            1
 */

u8 ath12k_default_dscp_tid_map[DSCP_TID_MAP_TBL_ENTRY_SIZE] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 1, 0, 1, 0, 1,
	0, 2, 3, 2, 3, 2, 3, 2,
	4, 3, 4, 3, 4, 3, 4, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 6, 5, 6, 5,
	7, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
};
EXPORT_SYMBOL(ath12k_default_dscp_tid_map);

const u8 ath12k_default_pcp_tid_map[PCP_TID_MAP_TBL_SIZE] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};
EXPORT_SYMBOL(ath12k_default_pcp_tid_map);

/**
 * ath12k_dp_pcp_tid_map() - Validate and program PCP-TID map to all SOCs.
 * @dp_hw_grp: Group-level DP structure; pcp_tid_map[] is read from here.
 *
 * This is the intelligent wrapper between vendor.c and the HAL.
 * The HAL function (ath12k_hal_tx_set_pcp_tid_map) is a dumb register writer
 * with no validation.  All sanity checks are performed here:
 *
 *   1. dp_hw_grp must not be NULL.
 *   2. Each pcp_tid_map[i] must be in range 0-7 (valid TID).
 *   3. At least one non-NULL SOC with a valid ab pointer must exist.
 *   4. Each SOC's dp->ab must not be NULL before calling HAL.
 *
 * On success, calls ath12k_hal_tx_set_pcp_tid_map(dp->ab, pcp_tid_map)
 * for every valid SOC in the group.
 *
 * Return: 0 on success, -EINVAL on bad arguments, -ENODEV if no SOC found.
 */
int ath12k_dp_pcp_tid_map(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp *dp;
	bool has_soc = false;
	int i;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "pcp_tid_map: dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < ATH12K_DP_PCP_TID_MAP_SIZE; i++) {
		if (dp_hw_grp->pcp_tid_map[i] > 7) {
			ath12k_err(NULL,
				   "pcp_tid_map: pcp_tid_map[%d]=%u out of range (0-7)\n",
				    i, dp_hw_grp->pcp_tid_map[i]);
			return -EINVAL;
		}
	}

	/* Program PCP-TID map to all valid SOCs */
	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, i);
		if (dp && dp->ab) {
			has_soc = true;
			ath12k_hal_tx_set_pcp_tid_map(dp->ab, dp_hw_grp->pcp_tid_map);
		}
	}

	if (!has_soc) {
		ath12k_err(NULL, "pcp_tid_map: no valid SOC found in group\n");
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_pcp_tid_map);

/**
 * ath12k_dp_tid_map_precedence() - Validate and program TID precedence to all SOCs.
 * @dp_hw_grp: Group-level DP structure; tid_map_precedence is read from here.
 *
 * This is the intelligent wrapper between vendor.c and the HAL.
 * The HAL function (ath12k_hal_tx_set_tid_map_precedence) is a dumb
 * register writer with no validation.  All sanity checks are performed here:
 *
 *   1. dp_hw_grp must not be NULL.
 *   2. tid_map_precedence must be 0 (DSCP) or 1 (PCP).
 *   3. At least one non-NULL SOC with a valid ab pointer must exist.
 *   4. Each SOC's dp->ab must not be NULL before calling HAL.
 *
 * On success, calls ath12k_hal_tx_set_tid_map_precedence(dp->ab, precedence)
 * for every valid SOC in the group.
 *
 * Return: 0 on success, -EINVAL on bad arguments, -ENODEV if no SOC found.
 */
int ath12k_dp_tid_map_precedence(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp *dp;
	bool has_soc = false;
	u8 i, precedence;

	if (!dp_hw_grp) {
		ath12k_err(NULL, "tid_map_prty: dp_hw_grp is NULL\n");
		return -EINVAL;
	}

	if (dp_hw_grp->tid_map_precedence > ATH12K_DP_MAX_TID_PRECEDENCE_VAL) {
		ath12k_err(NULL,
			   "tid_map_prty: precedence=%u out of range (0=DSCP, 1=PCP)\n",
			   dp_hw_grp->tid_map_precedence);
		return -EINVAL;
	}

	/* Program TID precedence for all valid SOCs */
	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, i);
		if (dp && dp->ab) {
			has_soc = true;
			precedence = dp_hw_grp->tid_map_precedence;
			ath12k_hal_tx_set_tid_map_precedence(dp->ab, precedence);
		}
	}

	if (!has_soc) {
		ath12k_err(NULL, "tid_map_prty: no valid SOC found in group\n");
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_tid_map_precedence);

u32 ath12k_dp_reo_dst_ring_size[DP_REO_DST_RING_MAX];
EXPORT_SYMBOL(ath12k_dp_reo_dst_ring_size);

u32 ath12k_dp_tcl_data_ring_size[DP_TCL_NUM_RING_MAX];
EXPORT_SYMBOL(ath12k_dp_tcl_data_ring_size);

u32 ath12k_dp_tx_comp_ring_size[DP_TCL_NUM_RING_MAX];
EXPORT_SYMBOL(ath12k_dp_tx_comp_ring_size);

u32 ath12k_dp_tx_desc_count[DP_TCL_NUM_RING_MAX];
EXPORT_SYMBOL(ath12k_dp_tx_desc_count);

u32 ath12k_dp_num_ppeds_tx_spt_pages;
EXPORT_SYMBOL(ath12k_dp_num_ppeds_tx_spt_pages);

enum ath12k_dp_desc_type {
	ATH12K_DP_TX_DESC,
	ATH12K_DP_RX_DESC,
	ATH12K_DP_PPEDS_TX_DESC,
};

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_dp_ppe_rxole_rxdma_cfg(struct ath12k_base *ab)
{
	struct ath12k_dp_htt_rxdma_ppe_cfg_param param = {0};
	int ret;

	param.override = 1;
	param.reo_dst_ind = HAL_REO2PPE_DST_IND;
	param.multi_buffer_msdu_override_en = 0;

	/* Override use_ppe to 0 in RxOLE for the following cases */
	param.intra_bss_override = 0;
	param.decap_raw_override = 1;
	param.decap_nwifi_override = 1;
	param.ip_frag_override = 1;

	ret = ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(ab, &param);
	if (ret)
		ath12k_err(ab, "RxOLE and RxDMA PPE config failed %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_ppe_rxole_rxdma_cfg);
#endif

void ath12k_dp_peer_cleanup(struct ath12k *ar, void *ptr, int vdev_id, const u8 *addr)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	/* TODO: Any other peer specific DP cleanup */
	if (!dp_peer)
		return;

	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, addr);
	if (!peer)
		peer = ath12k_dp_link_peer_find_by_addr_post_rcu_remove(dp_peer, addr);

	if (!peer) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "failed to lookup peer %pM on vdev %d\n",
				 addr, vdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&dp->dp_lock);

	if (!ath12k_dp_arch_mlo_peer_tid_teardown_ready(dp, peer->dp_peer, peer)) {
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
		return;
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_ipa_peer_notify(ar, peer, NULL, vdev_id, false);
#endif /* CPTCFG_QCN_EXTN */
	ath12k_dp_arch_smd_clear_old_peer_rx_lut(dp, peer->dp_peer);

	ath12k_dp_rx_peer_tid_cleanup(ar, peer);
	crypto_free_shash(peer->dp_peer->tfm_mmic);
	peer->dp_peer->tfm_mmic = NULL;
	peer->dp_peer->primary_link_frag_setup = false;
	spin_unlock_bh(&dp->dp_lock);
	rcu_read_unlock();
}

int ath12k_dp_peer_epp_setup_mgmt_tids(struct ath12k *ar, void *ptr,
				       struct ath12k_link_vif *arvif,
				       struct ath12k_link_sta *arsta)
{
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;
	struct ieee80211_sta *sta = ath12k_dp_peer_get_sta(dp_peer);
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_link_peer *link_peer;
	struct ieee80211_key_conf *key = NULL;
	struct ath12k_dp_rx_tid *rx_tid;
	u32 vdev_id = arvif->vdev_id;
	int i, ret, link_id;
	u32 ba_win_size;
	u16 ssn;
	u8 tid;

	if (!dp_peer || !sta)
		return -ENOENT;

	if (!ab->hw_params->hw_ops->is_mgmt_reoq_tid)
		return 0;

	/* only EPP peers need queue setup early */
	if (!sta->epp_peer)
		return 0;

	link_id = arsta->link_id;

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		ath12k_err(ab, "failed to find link peer (link=%d) to setup mgmt rx tid",
			   link_id);
		rcu_read_unlock();
		return -ENOLINK;
	}

	if (!link_peer->primary_link) {
		rcu_read_unlock();
		return 0;
	}

	ath12k_info(ab, "Setting up mgmt rx TIDs for EPP peer %pM on primary link: %u",
		    arsta->addr, link_id);

	for (tid = 0; tid < ab->hal.hal_params->num_tids; tid++) {
		if (!ath12k_hw_is_mgmt_reoq_tid(ab->hw_params, tid))
			continue;

		ath12k_dp_rx_peer_tid_ba_config(dp, tid, &ba_win_size, &ssn);
		ret = ath12k_dp_rx_peer_tid_setup(ar, dp_peer, arsta->addr, vdev_id, tid,
						  ba_win_size, ssn, HAL_PN_TYPE_NONE);
		if (ret) {
			ath12k_err(ab, "failed to setup mgmt rx tid queue for tid %u: %d",
				   tid, ret);
			goto tid_clean;
		}
	}

	spin_lock_bh(&dp_peer->keys_lock);
	for (i = 0; i < ARRAY_SIZE(dp_peer->keys); i++) {
		if (!dp_peer->keys[i] ||
		    !(dp_peer->keys[i]->flags & IEEE80211_KEY_FLAG_PAIRWISE))
			continue;

		key = dp_peer->keys[i];
		break;
	}

	if (key) {
		ret = ath12k_dp_rx_peer_pn_replay_config(arvif, arsta->addr, SET_KEY,
							 key, sta,
							 ATH12K_RXTID_PN_CHECK_MGMT_TIDS);
		if (ret) {
			spin_unlock_bh(&dp_peer->keys_lock);
			ath12k_err(ab, "failed to configure PN Replay check for mgmt rx tids: %d",
				   ret);
			goto tid_clean;
		}
	}
	spin_unlock_bh(&dp_peer->keys_lock);

	rcu_read_unlock();
	return 0;

tid_clean:
	for (tid--; (int)tid >= 0; tid--) {
		if (!ath12k_hw_is_mgmt_reoq_tid(ab->hw_params, tid))
			continue;
		rx_tid = &link_peer->dp_peer->rx_tid[tid];
		spin_lock_bh(&rx_tid->tid_lock);
		ath12k_dp_arch_rx_peer_tid_delete(ab->dp, ar, link_peer, tid);
		spin_unlock_bh(&rx_tid->tid_lock);
	}

	rcu_read_unlock();
	return ret;
}

int ath12k_dp_peer_set_default_routing(struct ath12k *ar,
				       struct ath12k_link_vif *arvif,
				       const u8 *addr)
{
	struct ath12k_base *ab = ar->ab;
	u32 reo_dest = ar->dp.mac_id + 1;
	int ret;

	if (ar->radio_cfg.pdev_to_reo_dest)
		reo_dest = ar->radio_cfg.pdev_to_reo_dest;

#ifdef CPTCFG_QCN_EXTN
	reo_dest = ath12k_dp_ipa_arch_op_rx_default_routing(ab, reo_dest);
#endif /* CPTCFG_QCN_EXTN */

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "WMI_PEER_SET_DEFAULT_ROUTING peer:%pM vdev_id:%u reo_dest:%u val:0x%x\n",
		   addr, arvif->vdev_id, reo_dest,
		   DP_RX_HASH_ENABLE | (reo_dest << 1));

	ret = ath12k_wmi_set_peer_param(ar, addr, arvif->vdev_id,
					WMI_PEER_SET_DEFAULT_ROUTING,
					DP_RX_HASH_ENABLE | (reo_dest << 1));
	if (ret)
		ath12k_warn(ab, "failed to set default routing %d peer :%pM vdev_id :%d\n",
			    ret, addr, arvif->vdev_id);
	return ret;
}

int ath12k_dp_peer_setup(struct ath12k *ar, void *ptr, struct ath12k_link_vif *arvif,
			 const u8 *addr, u8 link_id)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *link_peer;
	u32 vdev_id = arvif->vdev_id;
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	int ret = 0, tid, tid_start = 0;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct crypto_shash *tfm;
	u32 ba_win_size;
	u16 ssn;
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;
	bool is_mgmt;

	if (!dp_peer)
		return -ENOENT;

	ret = ath12k_dp_peer_set_default_routing(ar, arvif, addr);
	if (ret)
		return ret;

	tfm = crypto_alloc_shash("michael_mic", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		ath12k_warn(ab, "failed to find the peer to del rx tid\n");
		ret = -ENOENT;
		goto free_shash;
	}

	spin_lock_bh(&dp->dp_lock);

	sta = ath12k_dp_link_peer_get_sta(link_peer);
	ahsta = ath12k_sta_to_ahsta(sta);

	/* MAPC peers are management-only: skip data TIDs 0..N-2, set up
	 * only the management TID (num_tids - 1 = TID 16).
	 */
	if (sta->mapc)
		tid_start = ab->hal.hal_params->num_tids - 1;

	if (link_peer->mlo && link_peer->link_id != ahsta->primary_link_id) {
		link_peer->primary_link = false;
		arvif->primary_sta_link = false;
		if (ar->dp.dp_hw) {
			if (dp_peer->qos_stats_lvl == ATH12K_QOS_MULTI_LINK_STATS)
				ath12k_dp_qos_stats_alloc(ar, vif, link_peer);
		}
		spin_unlock_bh(&dp->dp_lock);
		goto free_shash;
	}

	link_peer->primary_link = true;
	arvif->primary_sta_link = true;

	/* Allocate qos stats for primary link alone */
	ath12k_dp_qos_stats_alloc(ar, vif, link_peer);

	spin_unlock_bh(&dp->dp_lock);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (vif->type == NL80211_IFTYPE_STATION)
		ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(ar->ab, arvif, true);
#endif

	for (tid = tid_start; tid < ab->hal.hal_params->num_tids; tid++) {
		is_mgmt = ath12k_hw_is_mgmt_reoq_tid(ab->hw_params, tid);
		if (is_mgmt && dp_peer->is_epp_peer)
			continue;

		ath12k_dp_rx_peer_tid_ba_config(dp, tid, &ba_win_size, &ssn);
		ret = ath12k_dp_rx_peer_tid_setup(ar, dp_peer, addr, vdev_id, tid,
						  ba_win_size, ssn, HAL_PN_TYPE_NONE);
		if (ret) {
			ath12k_warn(ab, "failed to setup rxd tid queue for tid %d: %d\n",
				    tid, ret);
			goto tid_clean;
		}
	}

	spin_lock_bh(&dp->dp_lock);

	ret = ath12k_dp_rx_peer_frag_setup(ar, link_peer, tfm);
	if (ret) {
		ath12k_warn(ab, "failed to setup rx defrag context\n");
		goto tid_clean;
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_ipa_peer_notify(ar, link_peer, arvif, vdev_id, true);
#endif /* CPTCFG_QCN_EXTN */
	spin_unlock_bh(&dp->dp_lock);

	/* TODO: Setup other peer specific resource used in data path */

	rcu_read_unlock();
	return 0;

tid_clean:
	for (tid--; tid >= tid_start; tid--) {
		is_mgmt = ath12k_hw_is_mgmt_reoq_tid(ab->hw_params, tid);
		if (is_mgmt && dp_peer->is_epp_peer)
			continue;

		rx_tid = &link_peer->dp_peer->rx_tid[tid];

		spin_lock_bh(&rx_tid->tid_lock);
		ath12k_dp_arch_rx_peer_tid_delete(ab->dp, ar, link_peer, tid);
		spin_unlock_bh(&rx_tid->tid_lock);
	}

free_shash:
	crypto_free_shash(tfm);
	rcu_read_unlock();
	return ret;
}

void ath12k_dp_srng_cleanup(struct ath12k_base *ab, struct dp_srng *ring)
{
	if (!ring->vaddr_unaligned)
		return;

	if (ring->cached)
		ath12k_hal_dma_free_noncoherent(ab->dev, ring->size,
						ring->vaddr_unaligned,
						ring->paddr_unaligned,
						DMA_FROM_DEVICE);
	else
		ath12k_hal_dma_free_coherent(ab->dev, ring->size, ring->vaddr_unaligned,
					     ring->paddr_unaligned);

	ring->vaddr_unaligned = NULL;
}
EXPORT_SYMBOL(ath12k_dp_srng_cleanup);

static int ath12k_dp_srng_find_ring_in_mask(int ring_num, const u8 *grp_mask)
{
	int ext_group_num;
	u8 mask = 1 << ring_num;

	for (ext_group_num = 0; ext_group_num < ATH12K_EXT_IRQ_NUM_MAX; ext_group_num++) {
		if (mask & grp_mask[ext_group_num])
			return ext_group_num;
	}

	return -ENOENT;
}

static int ath12k_dp_srng_calculate_msi_group(struct ath12k_base *ab,
					      enum hal_ring_type type, int ring_num)
{
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	struct ath12k_hw_ring_mask *ring_mask;
	const u8 *grp_mask;
	int i;

	ring_mask = ab->hw_params->ring_mask;
	switch (type) {
	case HAL_WBM2SW_RELEASE:
		if (ring_num == HAL_WBM2SW_REL_ERR_RING_NUM) {
			grp_mask = &ring_mask->rx_wbm_rel[0];
			ring_num = 0;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		} else if (ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
			grp_mask = &ring_mask->ppeds_tx_cmpln[0];
			ring_num = 0;
#endif
		} else {
			map = ab->hal.tcl_to_cmp_rbm_map;
			for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
				if (ring_num == map[i].cmp_ring_num) {
					ring_num = i;
					break;
				}
			}

			grp_mask = &ring_mask->tx[0];
		}
		break;
	case HAL_REO_EXCEPTION:
		grp_mask = &ring_mask->rx_err[0];
		break;
	case HAL_REO_DST:
	case HAL_REO_DST_ROAMING:
		grp_mask = &ring_mask->rx[0];
		break;
	case HAL_REO_STATUS:
		grp_mask = &ring_mask->reo_status[0];
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		grp_mask = &ab->hw_params->ring_mask->rx_mon_status[0];
		break;
	case HAL_RXDMA_MONITOR_DST:
		grp_mask = &ring_mask->rx_mon_dest[0];
		break;
	case HAL_TX_MONITOR_DST:
		grp_mask = &ring_mask->tx_mon_dest[0];
		break;
	case HAL_TX_MONITOR_BUF:
		grp_mask = &ring_mask->host2txmon[0];
		break;
	case HAL_RXDMA_BUF:
		grp_mask = &ring_mask->host2rxdma[0];
		break;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	case HAL_PPE2TCL:
		grp_mask = &ring_mask->ppe2tcl[0];
		break;
	case HAL_REO2PPE:
		grp_mask = &ring_mask->reo2ppe[0];
		break;
#endif
	case HAL_TX_EXCEPTION:
		grp_mask = &ab->hw_params->ring_mask->tx_exception[0];
		break;
	case HAL_TCL_STATUS:
		grp_mask = &ab->hw_params->ring_mask->tcl_status[0];
		break;
	case HAL_TQM_STATUS:
		grp_mask = &ab->hw_params->ring_mask->tqm_status[0];
		break;
	case HAL_RXDMA_MONITOR_BUF:
		grp_mask = &ab->hw_params->ring_mask->host2rxmon[0];
		break;

	case HAL_TX_COMPLETION:
		map = ab->hal.tcl_to_cmp_rbm_map;
		for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
			if (ring_num == map[i].cmp_ring_num) {
				ring_num = i;
				break;
			}
		}
		grp_mask = &ab->hw_params->ring_mask->tx[0];
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (ring_num == HAL_TQM2SW_PPEDS_TX_CMPLN_RING_NUM) {
			grp_mask = &ring_mask->ppeds_tx_cmpln[0];
			ring_num = 0;
		}
#endif
		break;
	case HAL_PEER_TX_TELEMETRY:
		grp_mask = &ab->hw_params->ring_mask->tx_peer_telemetry[0];
		break;
	case HAL_PEER_RX_TELEMETRY:
		grp_mask = &ab->hw_params->ring_mask->rx_peer_telemetry[0];
		break;
	case HAL_SAM_STATUS:
		grp_mask = &ab->hw_params->ring_mask->sam_status[0];
		break;
	case HAL_ASE_STATUS_RING:
		grp_mask = &ab->hw_params->ring_mask->ase_status[0];
		break;
	case HAL_TQM2SW_FW_COMPLETION:
		grp_mask = &ab->hw_params->ring_mask->tqm2sw_fw[0];
		break;
	case HAL_REO_FLUSH:
		grp_mask = &ab->hw_params->ring_mask->reo_flush[0];
		break;
	case HAL_REO_DST_MGMT:
	case HAL_REO_DST_CTDMA:
	case HAL_REO_EXCEPTION_DS:
	case HAL_REO_EXCEPTION_MGMT:
	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_REO_CMD:
	case HAL_SW2WBM_RELEASE:
	case HAL_WBM_IDLE_LINK:
	case HAL_REO_REINJECT:
	case HAL_CE_SRC:
	case HAL_CE_DST:
	case HAL_CE_DST_STATUS:
	case HAL_WBM_BUF:
	case HAL_WBM_IDLE_BUF:
	case HAL_PPE2WBM_BUF:
	case HAL_PPE2WBM_IDLE_BUF:
	default:
		return -ENOENT;
	}

	return ath12k_dp_srng_find_ring_in_mask(ring_num, grp_mask);
}

void ath12k_dp_srng_msi_setup(struct ath12k_base *ab,
			      struct hal_srng_params *ring_params,
			      enum hal_ring_type type, int ring_num)
{
	int msi_group_number, msi_data_count;
	u32 msi_data_start, msi_irq_start, addr_lo, addr_hi;
	int ret;
	int vector;

	ret = ath12k_hif_get_user_msi_vector(ab, "DP",
					     &msi_data_count, &msi_data_start,
					     &msi_irq_start);
	if (ret)
		return;

	msi_group_number = ath12k_dp_srng_calculate_msi_group(ab, type,
							      ring_num);
	if (msi_group_number < 0) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "ring not part of an ext_group; ring_type: %d,ring_num %d",
			   type, ring_num);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		return;
	}

	if (msi_group_number > msi_data_count) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "multiple msi_groups share one msi, msi_group_num %d",
			   msi_group_number);
	}

	ath12k_hif_get_msi_address(ab, &addr_lo, &addr_hi);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (dma_addr_t)(((uint64_t)addr_hi) << 32);
	if (ab->hif.bus == ATH12K_BUS_HYBRID)
		ring_params->msi_data = ab->ipci.dp_msi_data[msi_group_number];
	else
		ring_params->msi_data = (msi_group_number % msi_data_count)
			+ msi_data_start;
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;

	vector = msi_irq_start  + (msi_group_number % msi_data_count);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->hw_params->ds_support && !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);
#endif
}

int ath12k_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries)
{
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int ret;
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
		case HAL_REO_DST_ROAMING:
		case HAL_WBM2SW_RELEASE:
		case HAL_TX_COMPLETION:
#ifndef PLATFORM_SDX
			cached = true;
			break;
#endif
		default:
			cached = false;
		}
	}
#else
	cached = true;
#endif

	ret = ath12k_dp_srng_alloc_aligned(ab, ring, num_entries, entry_sz, cached);
	if (ret)
		return ret;

	ring->cached = cached;
	ring->num_entries = num_entries;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_alloc);

int ath12k_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
			enum hal_ring_type type, int ring_num, int mac_id)
{
	struct hal_srng_params params = {};
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	int vector = 0;
#endif
	int ret;

	memset(ring->vaddr_unaligned, 0, ring->size);

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = ring->num_entries;
	ath12k_dp_srng_msi_setup(ab, &params, type, ring_num + mac_id);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->hw_params->ds_support && ab->hif.bus == ATH12K_BUS_AHB &&
	    !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);
#endif

	switch (type) {
	case HAL_REO_DST:
	case HAL_REO_DST_ROAMING:
	case HAL_REO2PPE:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_RX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_BUF:
		params.low_threshold = ring->num_entries >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		if (ring->num_entries > DP_RXDMA_MONITOR_DEFAULT_RING_FILL_LVL) {
			params.low_threshold =
					DP_RXDMA_MONITOR_DEFAULT_RING_FILL_LVL >> 1;
		} else {
			params.low_threshold = ring->num_entries >> 1;
		}
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		params.low_threshold = ring->num_entries >> 3;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 1;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_TX_EXCEPTION:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_TX_EXCEPTION;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX_EXCEPTION;
		break;
	case HAL_TX_MONITOR_BUF:
		if (ring->num_entries > DP_TX_MON_BUF_RING_FILL_LVL(ab))
			params.low_threshold = DP_TX_MON_BUF_RING_FILL_LVL(ab) >> 1;
		else
			params.low_threshold = ring->num_entries >> 1;
		params.flags |= HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN;
		params.intr_batch_cntr_thres_entries = 0;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_WBM2SW_RELEASE:
		if (ab->hw_params->hw_ops->dp_srng_is_tx_comp_ring(ring_num)) {
			params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_TX;
			params.intr_timer_thres_us =
					HAL_SRNG_INT_TIMER_THRESHOLD_TX;
			break;
		} else if (ring_num == HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM) {
			params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_PPE_WBM2SW_REL;
			params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX;

				break;
		}
		/* follow through when ring_num != HAL_WBM2SW_REL_ERR_RING_NUM */
		fallthrough;
	case HAL_REO_EXCEPTION:
	case HAL_REO_REINJECT:
	case HAL_REO_CMD:
	case HAL_REO_STATUS:
	case HAL_RXOLE_FSE_CMD:
	case HAL_TCL_DATA:
	case HAL_TCL_CMD:
	case HAL_TCL_STATUS:
	case HAL_ASE_CMD_RING:
	case HAL_ASE_STATUS_RING:
	case HAL_TQM_CMD:
	case HAL_TQM_STATUS:
	case HAL_WBM_IDLE_LINK:
	case HAL_SW2WBM_RELEASE:
	case HAL_RXDMA_DST:
	case HAL_RXDMA_MONITOR_DST:
	case HAL_RXDMA_MONITOR_DESC:
	case HAL_WBM_BUF:
	case HAL_WBM_IDLE_BUF:
	case HAL_PPE2WBM_BUF:
	case HAL_PPE2WBM_IDLE_BUF:
	case HAL_TX_MONITOR_DST:
	case HAL_SAM_CMD:
	case HAL_SAM_STATUS:
	case HAL_REO_FLUSH:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	case HAL_RXDMA_DIR_BUF:
		break;
	case HAL_PPE2TCL:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL;
		break;
	case HAL_TX_COMPLETION:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_TX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX;
		break;
	case HAL_TQM2PPE:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_TX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX;
		break;
	case HAL_TQM2SW_FW_COMPLETION:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_TX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_TX;
		break;
	case HAL_PEER_TX_TELEMETRY:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_TX_TELEMETRY;
		params.intr_timer_thres_us =
			HAL_SRNG_INT_TIMER_THRESHOLD_TX_TELEMETRY;
		break;
	case HAL_PEER_RX_TELEMETRY:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_RX_TELEMETRY;
		params.intr_timer_thres_us =
			HAL_SRNG_INT_TIMER_THRESHOLD_RX_TELEMETRY;
		break;
	default:
		ath12k_warn(ab, "Not a valid ring type in dp :%d\n", type);
		return -EINVAL;
	}

	if (ring->cached)
		params.flags |= HAL_SRNG_FLAGS_CACHED;

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params, 0);
	if (ret < 0) {
		ath12k_warn(ab, "failed to setup srng: %d ring_id %d\n",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_init);

int ath12k_dp_srng_setup(struct ath12k_base *ab, struct dp_srng *ring,
			 enum hal_ring_type type, int ring_num,
			 int mac_id, int num_entries)
{
	int ret;

	if (ath12k_dp_umac_reset_in_progress(ab)) {
		/* For safety */
		ring->num_entries = num_entries;
		goto skip_dp_srng_alloc;
	}

	ret = ath12k_dp_srng_alloc(ab, ring, type, ring_num, mac_id, num_entries);
	if (ret)
		return ret;

skip_dp_srng_alloc:
	ret = ath12k_dp_srng_init(ab, ring, type, ring_num, mac_id);
	if (ret) {
		ath12k_dp_srng_cleanup(ab, ring);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_setup);

void ath12k_dp_increment_bank_num_users(struct ath12k_dp *dp,
					int bank_id)
{
	spin_lock_bh(&dp->tx_bank_lock);
	if (dp->bank_profiles)
		dp->bank_profiles[bank_id].num_users++;
	spin_unlock_bh(&dp->tx_bank_lock);
}

void ath12k_dp_tx_put_bank_profile(struct ath12k_dp *dp, u8 bank_id)
{
	spin_lock_bh(&dp->tx_bank_lock);
	if (dp->bank_profiles && dp->bank_profiles[bank_id].num_users)
		dp->bank_profiles[bank_id].num_users--;
	spin_unlock_bh(&dp->tx_bank_lock);
}
EXPORT_SYMBOL(ath12k_dp_tx_put_bank_profile);

u32 ath12k_dp_tx_get_bank_config_from_id(struct ath12k_dp *dp, u8 bank_id)
{
	u32 bank_config;

	spin_lock_bh(&dp->tx_bank_lock);
	if (dp->bank_profiles)
		bank_config = dp->bank_profiles[bank_id].bank_config;
	spin_unlock_bh(&dp->tx_bank_lock);

	return bank_config;
}
EXPORT_SYMBOL(ath12k_dp_tx_get_bank_config_from_id);

int ath12k_dp_tx_get_bank_profile(struct ath12k_dp *dp,
				  u32 bank_config)
{
	int bank_id = DP_INVALID_BANK_ID;
	int i;
	struct ath12k_base *ab = dp->ab;
	bool configure_register = false;

	spin_lock_bh(&dp->tx_bank_lock);
	/* TODO: implement using idr kernel framework*/
	for (i = 0; i < dp->num_bank_profiles; i++) {
		if (dp->bank_profiles[i].is_configured &&
		    (dp->bank_profiles[i].bank_config ^ bank_config) == 0) {
			bank_id = i;
			goto inc_ref_and_return;
		}
		if (!dp->bank_profiles[i].is_configured ||
		    !dp->bank_profiles[i].num_users) {
			bank_id = i;
			goto configure_and_return;
		}
	}

	if (bank_id == DP_INVALID_BANK_ID) {
		spin_unlock_bh(&dp->tx_bank_lock);
		ath12k_err(ab, "unable to find TX bank!");
		return bank_id;
	}

configure_and_return:
	dp->bank_profiles[bank_id].is_configured = true;
	dp->bank_profiles[bank_id].bank_config = bank_config;
	configure_register = true;
inc_ref_and_return:
	dp->bank_profiles[bank_id].num_users++;
	spin_unlock_bh(&dp->tx_bank_lock);

	if (configure_register)
		ath12k_hal_tx_configure_bank_register(ab,
						      bank_config, bank_id);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt tcl bank_id %d input 0x%x match 0x%x num_users %u",
		   bank_id, bank_config, dp->bank_profiles[bank_id].bank_config,
		   dp->bank_profiles[bank_id].num_users);

	return bank_id;
}
EXPORT_SYMBOL(ath12k_dp_tx_get_bank_profile);

int ath12k_dp_bank_profiles_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 num_tcl_banks = ab->hw_params->num_tcl_banks;

	dp->num_bank_profiles = num_tcl_banks;
	dp->bank_profiles = kmalloc_array(num_tcl_banks,
					  sizeof(struct ath12k_dp_tx_bank_profile),
					  GFP_KERNEL);
	if (!dp->bank_profiles)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_bank_profiles_alloc);

void ath12k_dp_bank_profiles_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	kfree(dp->bank_profiles);
	dp->bank_profiles = NULL;
}
EXPORT_SYMBOL(ath12k_dp_bank_profiles_free);

void ath12k_dp_deinit_bank_profiles(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	for (i = 0; i < dp->num_bank_profiles; i++) {
		dp->bank_profiles[i].is_configured = false;
		dp->bank_profiles[i].num_users = 0;
	}
}
EXPORT_SYMBOL(ath12k_dp_deinit_bank_profiles);

int ath12k_dp_init_bank_profiles(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!dp->bank_profiles)
		return -EINVAL;

	spin_lock_init(&dp->tx_bank_lock);

	memset(dp->bank_profiles, 0,
	       dp->num_bank_profiles * sizeof(struct ath12k_dp_tx_bank_profile));

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_init_bank_profiles);

void ath12k_dp_srng_common_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	ath12k_dp_srng_cleanup(ab, &dp->reo_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_except_ring);
	ath12k_dp_srng_cleanup(ab, &dp->rx_rel_ring);
	ath12k_dp_srng_cleanup(ab, &dp->reo_reinject_ring);
	ath12k_dp_srng_cleanup(ab, &dp->wbm_desc_rel_ring);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops && ab->dp->ppe.ppe_ops->ath12k_ppeds_srng_cleanup)
		ab->dp->ppe.ppe_ops->ath12k_ppeds_srng_cleanup(ab);
#endif
}
EXPORT_SYMBOL(ath12k_dp_srng_common_cleanup);

int ath12k_dp_srng_common_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	ret = ath12k_dp_srng_alloc(ab, &dp->wbm_desc_rel_ring,
				   HAL_SW2WBM_RELEASE, 0, 0,
				   DP_WBM_RELEASE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up wbm2sw_release ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->reo_reinject_ring, HAL_REO_REINJECT,
				   0, 0, DP_REO_REINJECT_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_reinject ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->reo_except_ring, HAL_REO_EXCEPTION,
				   0, 0, DP_REO_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_exception ring :%d\n",
			    ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->reo_cmd_ring, HAL_REO_CMD,
				   0, 0, DP_REO_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_cmd ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->reo_status_ring, HAL_REO_STATUS,
				   0, 0, DP_REO_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_status ring :%d\n", ret);
		goto err;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_srng_cmn_alloc) {
		ret = dp->ppe.ppe_ops->ath12k_ppeds_srng_cmn_alloc(ab);
		if (ret) {
			ath12k_err(ab, "failed to alloc ppe-ds srngs :%d\n", ret);
			goto err;
		}
	}
#endif

	return 0;
err:
	ath12k_dp_srng_common_cleanup(ab);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_srng_common_alloc);

int ath12k_dp_srng_common_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct hal_srng *srng;
	int ret;

	ret = ath12k_dp_srng_init(ab, &dp->wbm_desc_rel_ring,
				  HAL_SW2WBM_RELEASE, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up wbm2sw_release ring :%d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp->reo_reinject_ring, HAL_REO_REINJECT,
				  0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_reinject ring :%d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp->reo_except_ring, HAL_REO_EXCEPTION,
				  0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_exception ring :%d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp->reo_cmd_ring, HAL_REO_CMD,
				  0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_cmd ring :%d\n", ret);
		return ret;
	}

	srng = &ab->hal.srng_list[dp->reo_cmd_ring.ring_id];
	ath12k_hal_reo_init_cmd_ring(ab, srng);

	ret = ath12k_dp_srng_init(ab, &dp->reo_status_ring, HAL_REO_STATUS,
				  0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo_status ring :%d\n", ret);
		return ret;
	}

	ath12k_hal_reo_hw_setup(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_srng_cmn_init) {
		ret = dp->ppe.ppe_ops->ath12k_ppeds_srng_cmn_init(ab);
		if (ret) {
			ath12k_warn(ab, "failed to set up ppe-ds srngs :%d\n", ret);
			return ret;
		}
	}
#endif

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_common_init);

int ath12k_dp_srng_common_setup(struct ath12k_base *ab)
{
	int ret;

	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_dma_alloc;

	ret = ath12k_dp_srng_common_alloc(ab);
	if (ret)
		return ret;

skip_dma_alloc:
	ret = ath12k_dp_srng_common_init(ab);
	if (ret) {
		ath12k_dp_srng_common_cleanup(ab);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_common_setup);

static void ath12k_dp_scatter_idle_link_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct wbm_idle_scatter_list *slist = dp->scatter_list;
	int i;

	dp->num_scatter_buf = 0;

	for (i = 0; i < DP_IDLE_SCATTER_BUFS_MAX; i++) {
		if (!slist[i].vaddr)
			continue;

		ath12k_hal_dma_free_coherent(ab->dev, HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX,
					      slist[i].vaddr, slist[i].paddr);
		slist[i].vaddr = NULL;
	}
}

static int
ath12k_dp_scatter_idle_link_desc_alloc(struct ath12k_base *ab, int size)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct wbm_idle_scatter_list *slist = dp->scatter_list;
	int num_scatter_buf;
	int ret, i;

	num_scatter_buf = DIV_ROUND_UP(size, HAL_WBM_IDLE_SCATTER_BUF_SIZE);

	if (num_scatter_buf > DP_IDLE_SCATTER_BUFS_MAX)
		return -EINVAL;

	for (i = 0; i < num_scatter_buf; i++) {
		slist[i].vaddr
			= ath12k_hal_dma_alloc_coherent(ab->dev,
							HAL_WBM_IDLE_SCATTER_BUF_SIZE_MAX,
							&slist[i].paddr, GFP_KERNEL);
		if (!slist[i].vaddr) {
			ret = -ENOMEM;
			goto err;
		}
	}

	dp->num_scatter_buf = num_scatter_buf;

	return 0;
err:
	ath12k_dp_scatter_idle_link_desc_cleanup(ab);
	return ret;
}

static int
ath12k_dp_scatter_idle_link_desc_init(struct ath12k_base *ab,
				      u32 n_link_desc_bank, u32 n_link_desc)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_link_desc_bank *link_desc_banks = dp->link_desc_banks;
	u16 link_desc_size = ab->hal.hal_params->link_desc_size;
	struct wbm_idle_scatter_list *slist = dp->scatter_list;
	struct ath12k_hal *hal = dp->hal;
	u8 rbm = dp->idle_link_rbm;
	struct wbm_link_desc *scatter_buf;
	int align_bytes, n_entries;
	u32 end_offset, cookie;
	u32 n_entries_per_buf;
	dma_addr_t paddr;
	int scatter_idx;
	int rem_entries;
	int i;

	n_entries_per_buf = HAL_WBM_IDLE_SCATTER_BUF_SIZE /
		ath12k_hal_srng_get_entrysize(ab, HAL_WBM_IDLE_LINK);

	scatter_idx = 0;
	scatter_buf = slist[scatter_idx].vaddr;
	rem_entries = n_entries_per_buf;

	for (i = 0; i < n_link_desc_bank; i++) {
		align_bytes = link_desc_banks[i].vaddr -
			      link_desc_banks[i].vaddr_unaligned;
		n_entries = (DP_LINK_DESC_ALLOC_SIZE_THRESH - align_bytes) /
			     link_desc_size;
		paddr = link_desc_banks[i].paddr;
		while (n_entries) {
			cookie = DP_LINK_DESC_COOKIE_SET(n_entries, i);
			ath12k_hal_set_link_desc_addr(hal, scatter_buf, cookie,
						      paddr, rbm);
			n_entries--;
			paddr += link_desc_size;
			if (rem_entries) {
				rem_entries--;
				scatter_buf++;
				continue;
			}

			rem_entries = n_entries_per_buf;
			scatter_idx++;
			scatter_buf = slist[scatter_idx].vaddr;
		}
	}

	end_offset = (scatter_buf - slist[scatter_idx].vaddr) *
		     sizeof(struct wbm_link_desc);
	ath12k_hal_setup_link_idle_list(ab, slist, dp->num_scatter_buf,
					n_link_desc, end_offset);

	return 0;
}

static void
ath12k_dp_link_desc_bank_free(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks)
{
	int i;

	for (i = 0; i < DP_LINK_DESC_BANKS_MAX; i++) {
		if (link_desc_banks[i].vaddr_unaligned) {
			ath12k_hal_dma_free_coherent(ab->dev,
						      link_desc_banks[i].size,
						      link_desc_banks[i].vaddr_unaligned,
						      link_desc_banks[i].paddr_unaligned);
			link_desc_banks[i].vaddr_unaligned = NULL;
		}
	}
}

static int ath12k_dp_link_desc_bank_alloc(struct ath12k_base *ab,
					  struct dp_link_desc_bank *desc_bank,
					  int n_link_desc_bank,
					  int last_bank_sz)
{
	int i;
	int ret = 0;
	int desc_sz = DP_LINK_DESC_ALLOC_SIZE_THRESH;

	for (i = 0; i < n_link_desc_bank; i++) {
		if (i == (n_link_desc_bank - 1) && last_bank_sz)
			desc_sz = last_bank_sz;

		desc_bank[i].vaddr_unaligned =
				ath12k_hal_dma_alloc_coherent(ab->dev, desc_sz,
							       &desc_bank[i].paddr_unaligned,
							       GFP_KERNEL);
		if (!desc_bank[i].vaddr_unaligned) {
			ret = -ENOMEM;
			goto err;
		}

		desc_bank[i].vaddr = PTR_ALIGN(desc_bank[i].vaddr_unaligned,
					       HAL_LINK_DESC_ALIGN);
		desc_bank[i].paddr = desc_bank[i].paddr_unaligned +
				     ((unsigned long)desc_bank[i].vaddr -
				      (unsigned long)desc_bank[i].vaddr_unaligned);
		desc_bank[i].size = desc_sz;
	}

	return 0;

err:
	ath12k_dp_link_desc_bank_free(ab, desc_bank);

	return ret;
}

void ath12k_dp_link_desc_cleanup(struct ath12k_base *ab,
				 struct dp_link_desc_bank *desc_bank,
				 u32 ring_type, struct dp_srng *ring)
{
	ath12k_dp_link_desc_bank_free(ab, desc_bank);

	if (ring_type != HAL_RXDMA_MONITOR_DESC) {
		ath12k_dp_srng_cleanup(ab, ring);
		ath12k_dp_scatter_idle_link_desc_cleanup(ab);
	}
}
EXPORT_SYMBOL(ath12k_dp_link_desc_cleanup);

int ath12k_wbm_idle_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	ret = ath12k_dp_srng_init(ab, &dp->wbm_idle_ring,
				  HAL_WBM_IDLE_LINK, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm_idle_ring: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_wbm_idle_ring_init);

int ath12k_wbm_idle_ring_alloc(struct ath12k_base *ab, u32 *n_link_desc)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 n_mpdu_link_desc, n_mpdu_queue_desc;
	u32 n_tx_msdu_link_desc, n_rx_msdu_link_desc;
	int ret = 0;

	n_mpdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX) /
			   ab->hal.hal_params->num_mpdus_per_link_desc;

	n_mpdu_queue_desc = n_mpdu_link_desc /
			    ab->hal.hal_params->num_mpdu_links_per_queue_desc;

	n_tx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_FLOWS_PER_TID *
			       DP_AVG_MSDUS_PER_FLOW) /
			      ab->hal.hal_params->num_tx_msdus_per_link_desc;

	n_rx_msdu_link_desc = (DP_NUM_TIDS_MAX * DP_AVG_MPDUS_PER_TID_MAX *
			       DP_AVG_MSDUS_PER_MPDU) /
			      ab->hal.hal_params->num_rx_msdus_per_link_desc;

	*n_link_desc = n_mpdu_link_desc + n_mpdu_queue_desc +
		      n_tx_msdu_link_desc + n_rx_msdu_link_desc;

	if (*n_link_desc & (*n_link_desc - 1))
		*n_link_desc = 1 << fls(*n_link_desc);

	ret = ath12k_dp_srng_alloc(ab, &dp->wbm_idle_ring,
				   HAL_WBM_IDLE_LINK, 0, 0, *n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_wbm_idle_ring_alloc);

void ath12k_wbm_idle_ring_cleanup(struct ath12k_base *ab)
{
	ath12k_dp_srng_cleanup(ab, &ab->dp->wbm_idle_ring);
}
EXPORT_SYMBOL(ath12k_wbm_idle_ring_cleanup);

int ath12k_wbm_idle_ring_setup(struct ath12k_base *ab, u32 *n_link_desc)
{
	int ret;

	if (ath12k_dp_umac_reset_in_progress(ab)) {
		*n_link_desc = ab->dp->wbm_idle_ring.num_entries;
		goto skip_dma_alloc;
	}

	ret = ath12k_wbm_idle_ring_alloc(ab, n_link_desc);
	if (ret)
		return ret;

skip_dma_alloc:
	ret = ath12k_wbm_idle_ring_init(ab);
	if (ret) {
		ath12k_wbm_idle_ring_cleanup(ab);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_wbm_idle_ring_setup);

int ath12k_dp_link_desc_alloc(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc)
{
	u32 n_link_desc_bank, last_bank_sz;
	u16 link_desc_size;
	u32 tot_mem_sz;
	u32 entry_sz;
	int ret;

	link_desc_size = ab->hal.hal_params->link_desc_size;
	tot_mem_sz = n_link_desc * link_desc_size;
	tot_mem_sz += HAL_LINK_DESC_ALIGN;

	if (tot_mem_sz <= DP_LINK_DESC_ALLOC_SIZE_THRESH) {
		n_link_desc_bank = 1;
		last_bank_sz = tot_mem_sz;
	} else {
		n_link_desc_bank = tot_mem_sz /
				   (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				    HAL_LINK_DESC_ALIGN);
		last_bank_sz = tot_mem_sz %
			       (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				HAL_LINK_DESC_ALIGN);

		if (last_bank_sz)
			n_link_desc_bank += 1;
	}

	if (n_link_desc_bank > DP_LINK_DESC_BANKS_MAX)
		return -EINVAL;

	ret = ath12k_dp_link_desc_bank_alloc(ab, link_desc_banks,
					     n_link_desc_bank,
					     last_bank_sz);
	if (ret)
		return ret;

	/* Setup link desc idle list for HW internal usage */
	entry_sz = ath12k_hal_srng_get_entrysize(ab, ring_type);
	tot_mem_sz = entry_sz * n_link_desc;

	/* Setup scatter desc list when the total memory requirement is more */
	if (tot_mem_sz > DP_LINK_DESC_ALLOC_SIZE_THRESH &&
	    ring_type != HAL_RXDMA_MONITOR_DESC) {
		ret = ath12k_dp_scatter_idle_link_desc_alloc(ab, tot_mem_sz);
		if (ret) {
			ath12k_warn(ab, "failed to setup scatting idle list descriptor :%d\n",
				    ret);
			goto fail_desc_bank_free;
		}

		return 0;
	}

	return 0;

fail_desc_bank_free:
	ath12k_dp_link_desc_bank_free(ab, link_desc_banks);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_link_desc_alloc);

int ath12k_dp_link_desc_init(struct ath12k_base *ab,
			     struct dp_link_desc_bank *link_desc_banks,
			     u32 ring_type, struct hal_srng *srng,
			     u32 n_link_desc)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 n_link_desc_bank, last_bank_sz;
	u32 align_bytes, n_entries;
	u8 rbm = dp->idle_link_rbm;
	struct wbm_link_desc *desc;
	u16 link_desc_size;
	u32 tot_mem_sz;
	u64 paddr;
	int i, ret;
	u32 cookie;

	link_desc_size = ab->hal.hal_params->link_desc_size;
	tot_mem_sz = n_link_desc * link_desc_size;
	tot_mem_sz += HAL_LINK_DESC_ALIGN;

	if (tot_mem_sz <= DP_LINK_DESC_ALLOC_SIZE_THRESH) {
		n_link_desc_bank = 1;
		last_bank_sz = tot_mem_sz;
	} else {
		n_link_desc_bank = tot_mem_sz /
				   (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				    HAL_LINK_DESC_ALIGN);
		last_bank_sz = tot_mem_sz %
			       (DP_LINK_DESC_ALLOC_SIZE_THRESH -
				HAL_LINK_DESC_ALIGN);

		if (last_bank_sz)
			n_link_desc_bank += 1;
	}

	if (n_link_desc_bank > DP_LINK_DESC_BANKS_MAX)
		return -EINVAL;

	if (dp->num_scatter_buf && ring_type != HAL_RXDMA_MONITOR_DESC) {
		ret = ath12k_dp_scatter_idle_link_desc_init(ab,
							    n_link_desc_bank,
							    n_link_desc);
		if (ret) {
			ath12k_warn(ab, "failed to setup scatting idle list descriptor :%d\n",
				    ret);
			return ret;
		}

		return 0;
	}

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	for (i = 0; i < n_link_desc_bank; i++) {
		align_bytes = link_desc_banks[i].vaddr -
			      link_desc_banks[i].vaddr_unaligned;
		n_entries = (link_desc_banks[i].size - align_bytes) /
			    link_desc_size;
		paddr = link_desc_banks[i].paddr;
		while (n_entries &&
		       (desc = ath12k_hal_srng_src_get_next_entry(ab, srng))) {
			cookie = DP_LINK_DESC_COOKIE_SET(n_entries, i);
			ath12k_hal_set_link_desc_addr(dp->hal, desc, cookie, paddr,
						      rbm);
			n_entries--;
			paddr += link_desc_size;
		}
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_link_desc_init);

int ath12k_dp_link_desc_setup(struct ath12k_base *ab,
			      struct dp_link_desc_bank *link_desc_banks,
			      u32 ring_type, struct hal_srng *srng,
			      u32 n_link_desc)
{
	int ret;

	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_dma_alloc;

	ret = ath12k_dp_link_desc_alloc(ab, link_desc_banks,
					ring_type, srng, n_link_desc);
	if (ret)
		return ret;

skip_dma_alloc:
	ret = ath12k_dp_link_desc_init(ab, link_desc_banks,
				       ring_type, srng, n_link_desc);
	if (ret) {
		/* the caller must free the link desc in this case */
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_link_desc_setup);

int ath12k_dp_pdev_pre_alloc(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_pdev_dp *dp = &ar->dp;
	int ret;

	dp->hw = ar->ah->hw;
	dp->dp = ath12k_ab_to_dp(ar->ab);
	dp->mac_id = ar->pdev_idx;
	dp->ar = ar;
	dp->dp_hw = &ar->ah->dp_hw;
	dp->hw_link_id = ar->hw_link_id;

	atomic_set(&dp->num_tx_pending, 0);
	init_waitqueue_head(&dp->tx_empty_waitq);

	/* Initialize per-TID TX/RX delay histograms */
	ath12k_dp_tid_rx_stats_hist_init(dp);
	ath12k_dp_tid_tx_stats_hist_init(dp);

	if (!dp->dp_mon_pdev_configured) {
		ret = ath12k_dp_mon_pdev_init(dp);
		if (ret) {
			ath12k_warn(ab, "failed to initialize mon pdev for pdev with mac_id: %d\n", dp->mac_id);
			return ret;
		}

		ret = ath12k_dp_mon_pdev_rx_alloc(dp, dp->mac_id);
		if (ret) {
			ath12k_warn(ab, "failed to alloc rx filter for pdev with mac_id: %d\n", dp->mac_id);
			goto mon_pdev_deinit;
		}

		ret = ath12k_dp_mon_pdev_rx_htt_setup(dp, dp->mac_id);
		if (ret) {
			ath12k_warn(ab, "failed to setup rx htt for pdev with mac_id: %d\n", dp->mac_id);
			goto mon_pdev_rx_free;
		}

		ret = ath12k_dp_mon_tx_pdev_alloc(dp, dp->mac_id);
		if (ret) {
			ath12k_err(ab, "TX Monitor: Pdev alloc failed - mac_id=%d (%d)",
				   dp->mac_id, ret);
			goto mon_pdev_tx_free;
		}

		dp->dp_mon_pdev_configured = true;
	}

	/* TODO: Add any RXDMA setup required per pdev */

	return 0;

mon_pdev_tx_free:
	ath12k_dp_mon_tx_pdev_free(&ar->dp);

mon_pdev_rx_free:
	ath12k_dp_mon_pdev_rx_free(dp);

mon_pdev_deinit:
	ath12k_dp_mon_pdev_deinit(dp);

	return ret;
}

int ath12k_dp_get_pdev_telemetry_stats(struct ath12k_base *ab,
                                      int pdev_id,
                                      struct ath12k_pdev_telemetry_stats *stats)
{
       struct ath12k_pdev_dp *dp;
       struct ath12k *ar;
       u8 ac;

       if (!ab) {
               pr_warn("Failed to fetch pdev dp telemetry stats\n");
               return -EINVAL;
       }

       ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
       if (!ar)
               return -EINVAL;

       dp = &ar->dp;

       spin_lock_bh(&ar->data_lock);

       /* Convert *_link_airtime from telemetry stats to a percentage of
        * microseconds (us) */
       for (ac = 0; ac < WLAN_MAX_AC; ac++) {
               stats->tx_link_airtime[ac] =
                       ((dp->stats.telemetry_stats.tx_link_airtime[ac] * 100) / 1000000);
               stats->rx_link_airtime[ac] =
                       ((dp->stats.telemetry_stats.rx_link_airtime[ac] * 100) / 1000000);
               stats->link_airtime[ac] =
                       (((dp->stats.telemetry_stats.tx_link_airtime[ac] +
                          dp->stats.telemetry_stats.rx_link_airtime[ac]) * 100) / 1000000);
       }

	stats->rx_data_msdu_cnt = dp->stats.telemetry_stats.rx_data_msdu_cnt;
	stats->total_rx_data_bytes = dp->stats.telemetry_stats.total_rx_data_bytes;
	stats->tx_data_msdu_cnt = dp->stats.telemetry_stats.tx_data_msdu_cnt;
	stats->total_tx_data_bytes = dp->stats.telemetry_stats.total_tx_data_bytes;
	stats->sta_vap_exist = dp->stats.telemetry_stats.sta_vap_exist;
	stats->time_last_assoc = dp->stats.telemetry_stats.time_last_assoc;

       spin_unlock_bh(&ar->data_lock);

       return 0;
}

void ath12k_dp_update_vdev_search(struct ath12k_vif *ahvif)
{
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;

	switch (ahvif->vdev_type) {
	case WMI_VDEV_TYPE_STA:
		dp_vif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
		dp_vif->search_type = HAL_TX_ADDR_SEARCH_INDEX;
		break;
	case WMI_VDEV_TYPE_AP:
	case WMI_VDEV_TYPE_IBSS:
		dp_vif->hal_addr_search_flags = HAL_TX_ADDRX_EN;
		dp_vif->search_type = HAL_TX_ADDR_SEARCH_DEFAULT;
		break;
	case WMI_VDEV_TYPE_MONITOR:
	default:
		return;
	}
}
EXPORT_SYMBOL(ath12k_dp_update_vdev_search);

void ath12k_dp_tx_ext_desc_free(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc)
{
	/* Unmap extension descriptor DMA*/
	if (!tx_desc->paddr_ext_desc || !tx_desc->ext_desc)
		return;

	/* Unmap SG buffers */
	if (tx_desc->is_from_sg) {
		ath12k_dp_tx_sg_unmap_buf(dp, tx_desc->ext_desc,
					  tx_desc->skb);
		tx_desc->is_from_sg = 0;
	}
	ath12k_core_dma_unmap_single(dp->dev, tx_desc->paddr_ext_desc,
				     tx_desc->ext_desc_len, DMA_TO_DEVICE);
	kmem_cache_free(dp->ext_cache, tx_desc->ext_desc);

	tx_desc->ext_kmem = 0;
	tx_desc->ext_desc = NULL;
	tx_desc->ext_desc_len = 0;
	tx_desc->paddr_ext_desc = 0;
}
EXPORT_SYMBOL(ath12k_dp_tx_ext_desc_free);

void ath12k_dp_tx_cc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp *desc_dp;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_tx_desc_info *tx_desc_info;
	struct sk_buff *skb;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct sk_buff_head free_list;
	int j, k;
	u32 pool_id, tx_spt_page;

	if (!dp_hw_grp->tx_desc_initialized)
		return;

	skb_queue_head_init(&free_list);

	/* TX Descriptor cleanup */
	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
		for (j = 0; j < ath12k_dp_tx_spt_pages_per_pool(pool_id); j++) {
			tx_spt_page = j + ath12k_dp_tx_spt_page_offset(pool_id);
			tx_desc_info = dp_hw_grp->txbaddr[tx_spt_page];
			if (!tx_desc_info)
				continue;
			for (k = 0; k < ATH12K_MAX_SPT_ENTRIES; k++) {
				if (!tx_desc_info[k].in_use)
					continue;

				skb = tx_desc_info[k].skb;
				if (!skb)
					continue;

				/* reset per-iteration to avoid stale pointer */
				dp_pdev = NULL;
				if (tx_desc_info[k].hw_link_id < ATH12K_GROUP_MAX_RADIO) {
					u8 hw_link_id = tx_desc_info[k].hw_link_id;
					u8 device_id =
						dp_hw_grp->hw_links[hw_link_id].device_id;
					desc_dp = dp_hw_grp->dp[device_id];
					dp_pdev = ath12k_dp_hw_grp_to_dp_pdev
							(dp->dp_hw_grp, hw_link_id);
					if (desc_dp != dp)
						continue;
				}
				tx_desc_info[k].skb = NULL;

				/* Cleanup extension descriptor based on type */
				if (tx_desc_info[k].ext_kmem) {
					ath12k_dp_tx_ext_desc_free(dp, &tx_desc_info[k]);
				} else if (tx_desc_info[k].skb_ext_desc) {
					ath12k_core_dma_unmap_single(dp->dev,
								     tx_desc_info[k].paddr_ext_desc,
								     tx_desc_info[k].skb_ext_desc->len,
								     DMA_TO_DEVICE);
					skb_queue_tail(&free_list,
						       tx_desc_info[k].skb_ext_desc);
					tx_desc_info[k].skb_ext_desc = NULL;
				}

				/* if we are unregistering, hw would've been destroyed and
				 * pdev is no longer valid
				 */
				if (!(test_bit(ATH12K_FLAG_UNREGISTERING,
					       &ab->dev_flags))) {
					if (dp_pdev &&
					    atomic_dec_and_test(&dp_pdev->num_tx_pending))
						wake_up(&dp_pdev->tx_empty_waitq);
				}

				ath12k_core_dma_unmap_single(dp->dev,
							     tx_desc_info[k].paddr,
							     tx_desc_info[k].len,
							     DMA_TO_DEVICE);
				skb_queue_tail(&free_list, skb);
				ath12k_dp_tx_release_txbuf_nolock(dp, &tx_desc_info[k],
								  pool_id);
			}
		}
		spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	}

	__skb_queue_purge(&free_list);
	ath12k_dp_ext_desc_cache_deinit(dp);
}

static u32 ath12k_dp_cc_cookie_gen(u16 ppt_idx, u16 spt_idx)
{
	return (u32)ppt_idx << ATH12K_CC_PPT_SHIFT | spt_idx;
}

static inline
void *ath12k_dp_cc_get_desc_addr_ptr(struct ath12k_spt_info *spt_info_base,
				     u16 ppt_idx, u16 spt_idx)
{
	return spt_info_base[ppt_idx].vaddr + spt_idx;
}

struct ath12k_rx_desc_info *ath12k_dp_get_rx_desc(struct ath12k_dp *dp,
						  u32 cookie)
{
	struct ath12k_rx_desc_info **desc_addr_ptr;
	u16 start_ppt_idx, end_ppt_idx, ppt_idx, spt_idx, rx_spt_offset;
	struct ath12k_base *ab = dp->ab;

	if (!ab)
		return NULL;

	ppt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);
	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);

	start_ppt_idx = dp->rx_ppt_base + ath12k_dp_rx_spt_page_offset();
	end_ppt_idx = start_ppt_idx + ATH12K_NUM_RX_SPT_PAGES;

	if (ppt_idx < start_ppt_idx ||
	    ppt_idx >= end_ppt_idx ||
	    spt_idx > ATH12K_MAX_SPT_ENTRIES)
		return NULL;

	rx_spt_offset = ppt_idx - dp->rx_ppt_base - ath12k_dp_rx_spt_page_offset();
	desc_addr_ptr = ath12k_dp_cc_get_desc_addr_ptr(dp->spt_info,
						       rx_spt_offset, spt_idx);

	return *desc_addr_ptr;
}
EXPORT_SYMBOL(ath12k_dp_get_rx_desc);

struct ath12k_tx_desc_info *ath12k_dp_get_tx_desc(struct ath12k_dp *dp,
						  u32 cookie)
{
	struct ath12k_tx_desc_info **desc_addr_ptr;
	u16 start_ppt_idx, end_ppt_idx, ppt_idx, spt_idx, tx_spt_offset;
	struct ath12k_base *ab = dp->ab;

	if (!ab)
		return NULL;

	ppt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);
	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);

	start_ppt_idx = ATH12K_TX_SPT_PAGE_OFFSET;
	end_ppt_idx = start_ppt_idx + ath12k_dp_tx_spt_page_offset(ATH12K_HW_MAX_QUEUES);

	if (ppt_idx < start_ppt_idx ||
	    ppt_idx >= end_ppt_idx ||
	    spt_idx > ATH12K_MAX_SPT_ENTRIES)
		return NULL;

	tx_spt_offset = ppt_idx - ATH12K_TX_SPT_OFFSET;
	desc_addr_ptr = ath12k_dp_cc_get_desc_addr_ptr(dp->dp_hw_grp->spt_info,
						       tx_spt_offset, spt_idx);

	return *desc_addr_ptr;
}
EXPORT_SYMBOL(ath12k_dp_get_tx_desc);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static u8 *ath12k_dp_cc_find_desc(struct ath12k_dp_hw_group *dp_hw_grp, u32 cookie)
{
	u16 spt_page_id, spt_idx;
	u8 *spt_va;

	spt_idx = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_SPT);
	spt_page_id = u32_get_bits(cookie, ATH12K_DP_CC_COOKIE_PPT);

	/* PPEDS TX SPT pages are stored in dp_hw_grp->ppeds_spt_info */
	spt_va = (u8 *)dp_hw_grp->ppeds_spt_info[spt_page_id].vaddr;

	return (spt_va + spt_idx * sizeof(u64));
}

struct ath12k_ppeds_tx_desc_info *ath12k_dp_get_ppeds_tx_desc(struct ath12k_base *ab,
							      u32 desc_id)
{
	u8 *desc_addr_ptr;

	desc_addr_ptr = ath12k_dp_cc_find_desc(ab->dp->dp_hw_grp, desc_id);
	return *(struct ath12k_ppeds_tx_desc_info **)desc_addr_ptr;
}
EXPORT_SYMBOL(ath12k_dp_get_ppeds_tx_desc);

void ath12k_dp_ppeds_tx_cmem_init(struct ath12k_base *ab)
{
	struct ath12k_dp_hw_group *dp_hw_grp = ath12k_ab_to_dp(ab)->dp_hw_grp;
	u32 cmem_base;
	u32 ppt_val;
	int i;

	cmem_base = ab->qmi.dev_mem[ATH12K_QMI_DEVMEM_CMEM_INDEX].start;

	for (i = ATH12K_PPEDS_TX_SPT_PAGE_OFFSET;
	     i < (ATH12K_PPEDS_TX_SPT_PAGE_OFFSET + ATH12K_NUM_PPEDS_TX_SPT_PAGES); i++) {
		ppt_val = dp_hw_grp->ppeds_spt_info[i].paddr >>
			  ATH12K_SPT_4K_ALIGN_OFFSET;
		/* Write to PPT in CMEM */
		if (ab->hif.ops->cmem_write32)
			ath12k_hif_cmem_write32(ab,
						cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
						ppt_val);
		else
			ath12k_hif_write32(ab,
					   cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
					   ppt_val);
	}
}
EXPORT_SYMBOL(ath12k_dp_ppeds_tx_cmem_init);

void ath12k_dp_ppeds_tx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct ath12k_dp_hw_group *dp_hw_grp = ath12k_ab_to_dp(ab)->dp_hw_grp;
	u32 tx_desc_free_cnt = 0, *used_cnt;
	struct sk_buff *skb;
	int i, j;

	/* PPEDS TX Descriptor cleanup */
	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = dp_hw_grp->ppedstxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!ppeds_tx_descs[j].in_use)
				continue;

			skb = ppeds_tx_descs[j].skb;
			if (!skb) {
				WARN_ON(1);
				continue;
			}

			ppeds_tx_descs[j].skb = NULL;
			ppeds_tx_descs[j].in_use = false;
			ath12k_core_dma_unmap_single_attrs(ab->dev,
							   ppeds_tx_descs[j].paddr,
							   skb->len, DMA_TO_DEVICE,
							   DMA_ATTR_SKIP_CPU_SYNC);

			skb_queue_tail(&ab->dp_umac_reset.ppeds_tx_skb_queue, skb);

			list_add_tail(&ppeds_tx_descs[j].list,
				      &dp_hw_grp->ppeds_tx_desc_free_list);
			tx_desc_free_cnt++;
		}
	}

	used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
	(*used_cnt) -= tx_desc_free_cnt;

	dp_hw_grp->ppeds_tx_desc_reuse_list_len = 0;

	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);
}
EXPORT_SYMBOL(ath12k_dp_ppeds_tx_desc_cleanup);

int ath12k_dp_ppeds_cc_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct ath12k_dp_hw_group *dp_hw_grp = ath12k_ab_to_dp(ab)->dp_hw_grp;
	struct sk_buff *skb;
	int i, j;

	if (!dp_hw_grp->ppedstxbaddr) {
		ath12k_err(ab, "%s failed", __func__);
		return -EINVAL;
	}

	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = dp_hw_grp->ppedstxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!ppeds_tx_descs[j].in_use)
				continue;

			if (ppeds_tx_descs[j].device_id != ab->device_id)
				continue;

			skb = ppeds_tx_descs[j].skb;

			if (!skb)
				continue;

			ppeds_tx_descs[j].skb = NULL;
			ppeds_tx_descs[j].in_use = false;
			ppeds_tx_descs[j].paddr = (dma_addr_t)NULL;
			dev_kfree_skb_any(skb);
			list_add_tail(&ppeds_tx_descs[j].list,
				      &dp_hw_grp->ppeds_tx_desc_free_list);
		}
	}

	dp_hw_grp->ppeds_tx_desc_reuse_list_len = 0;
	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_ppeds_cc_desc_cleanup);

void ath12k_dp_ppeds_spt_free_and_deinit(struct ath12k_base *ab,
					 struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	struct sk_buff *skb;
	int i, j;

	if (!dp_hw_grp->ppeds_tx_desc_initialized)
		return;

	if (!dp_hw_grp->ppedstxbaddr)
		goto free_spt;

	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = dp_hw_grp->ppedstxbaddr[i];
		if (!ppeds_tx_descs)
			continue;

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			skb = ppeds_tx_descs[j].skb;

			if (!skb)
				continue;

			ppeds_tx_descs[j].skb = NULL;
			ppeds_tx_descs[j].in_use = false;
			ppeds_tx_descs[j].paddr = (dma_addr_t)NULL;
			dev_kfree_skb_any(skb);
		}
	}

	dp_hw_grp->ppeds_tx_desc_reuse_list_len = 0;

	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		if (!dp_hw_grp->ppedstxbaddr[i])
			continue;

		kfree(dp_hw_grp->ppedstxbaddr[i]);
		dp_hw_grp->ppedstxbaddr[i] = NULL;
	}

	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	kfree(dp_hw_grp->ppedstxbaddr);
	dp_hw_grp->ppedstxbaddr = NULL;

free_spt:
	if (dp_hw_grp->ppeds_spt_info) {
		for (i = 0; i < dp_hw_grp->ppeds_num_spt_pages; i++) {
			if (!dp_hw_grp->ppeds_spt_info[i].vaddr)
				continue;
			ath12k_hal_dma_free_coherent(dp_hw_grp->ppeds_tx_spt_dev,
						     ATH12K_PAGE_SIZE,
						     dp_hw_grp->ppeds_spt_info[i].vaddr,
						     dp_hw_grp->ppeds_spt_info[i].paddr);
			dp_hw_grp->ppeds_spt_info[i].vaddr = NULL;
		}
		kfree(dp_hw_grp->ppeds_spt_info);
		dp_hw_grp->ppeds_spt_info = NULL;
	}

	free_percpu(dp_hw_grp->ppeds_tx_desc_used_cnt);

	dp_hw_grp->ppeds_tx_spt_dev = NULL;
	dp_hw_grp->ppeds_tx_desc_initialized = false;
}

int ath12k_dp_ppeds_spt_alloc_and_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_ppeds_tx_desc_info *ppeds_tx_descs;
	u32 i, j, ret = 0;
	u32 ppt_idx;

	mutex_lock(&dp_hw_grp->ppeds_tx_init_lock);

	if (dp_hw_grp->ppeds_tx_desc_initialized)
		goto unlock;

	INIT_LIST_HEAD(&dp_hw_grp->ppeds_tx_desc_free_list);
	INIT_LIST_HEAD(&dp_hw_grp->ppeds_tx_desc_reuse_list);
	spin_lock_init(&dp_hw_grp->ppeds_tx_desc_lock);
	dp_hw_grp->ppeds_tx_desc_reuse_list_len = 0;

	dp_hw_grp->ppeds_tx_desc_used_cnt = alloc_percpu(u32);
	if (!dp_hw_grp->ppeds_tx_desc_used_cnt)
		goto unlock;

	dp_hw_grp->ppeds_num_spt_pages = ATH12K_NUM_PPEDS_TX_SPT_PAGES;

	dp_hw_grp->ppeds_spt_info = kcalloc(dp_hw_grp->ppeds_num_spt_pages,
					    sizeof(struct ath12k_spt_info),
					    GFP_KERNEL);
	if (!dp_hw_grp->ppeds_spt_info) {
		free_percpu(dp_hw_grp->ppeds_tx_desc_used_cnt);
		ret = -ENOMEM;
		goto unlock;
	}

	dp_hw_grp->ppeds_tx_spt_dev = ab->dev;

	for (i = 0; i < dp_hw_grp->ppeds_num_spt_pages; i++) {
		dp_hw_grp->ppeds_spt_info[i].vaddr =
			ath12k_hal_dma_alloc_coherent(ab->dev, ATH12K_PAGE_SIZE,
						      &dp_hw_grp->ppeds_spt_info[i].paddr,
						      GFP_KERNEL);
		if (!dp_hw_grp->ppeds_spt_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		if (dp_hw_grp->ppeds_spt_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "PPEDS SPT allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	dp_hw_grp->ppedstxbaddr =
		kmalloc_array(ATH12K_NUM_PPEDS_TX_SPT_PAGES,
			      sizeof(struct ath12k_ppeds_tx_desc_info *),
			      GFP_KERNEL);
	if (!dp_hw_grp->ppedstxbaddr) {
		ret = -ENOMEM;
		goto free;
	}

	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);
	for (i = 0; i < ATH12K_NUM_PPEDS_TX_SPT_PAGES; i++) {
		ppeds_tx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*ppeds_tx_descs),
					 GFP_ATOMIC);
		if (!ppeds_tx_descs) {
			spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);
			ret = -ENOMEM;
			goto free;
		}
		dp_hw_grp->ppedstxbaddr[i] = &ppeds_tx_descs[0];
		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			ppt_idx = ATH12K_PPEDS_TX_SPT_PAGE_OFFSET + i;
			ppeds_tx_descs[j].desc_id = ath12k_dp_cc_cookie_gen(ppt_idx, j);
			ppeds_tx_descs[j].in_use = false;
			list_add_tail(&ppeds_tx_descs[j].list,
				      &dp_hw_grp->ppeds_tx_desc_free_list);
			/* Update descriptor VA in SPT */
			*(struct ath12k_ppeds_tx_desc_info **)
				((u8 *)dp_hw_grp->ppeds_spt_info[i].vaddr +
				 (j * sizeof(u64))) = &ppeds_tx_descs[j];
		}
	}
	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	dp_hw_grp->ppeds_tx_desc_initialized = true;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);

unlock:
	mutex_unlock(&dp_hw_grp->ppeds_tx_init_lock);
	return ret;

free:
	mutex_unlock(&dp_hw_grp->ppeds_tx_init_lock);
	ath12k_dp_ppeds_spt_free_and_deinit(ab, dp_hw_grp);
	return ret;
}
#endif

static int ath12k_dp_cmem_init(struct ath12k_base *ab, struct ath12k_dp *dp,
			       enum ath12k_dp_desc_type type)
{
	struct ath12k_spt_info *spt_info;
	u32 cmem_base, ppt_val;
	int i, j, start, end;

	cmem_base = ab->qmi.dev_mem[ATH12K_QMI_DEVMEM_CMEM_INDEX].start;

	switch (type) {
	case ATH12K_DP_TX_DESC:
		spt_info = dp->dp_hw_grp->spt_info;
		start = ATH12K_TX_SPT_PAGE_OFFSET;
		end = start + ath12k_dp_tx_spt_page_offset(ATH12K_HW_MAX_QUEUES);
		break;
	case ATH12K_DP_RX_DESC:
		spt_info = dp->spt_info;
		cmem_base += ATH12K_PPT_ADDR_OFFSET(dp->rx_ppt_base);
		start = ath12k_dp_rx_spt_page_offset();
		end = start + ATH12K_NUM_RX_SPT_PAGES;
		break;
	default:
		ath12k_err(ab, "invalid descriptor type %d in cmem init\n", type);
		return -EINVAL;
	}

	/* Write to PPT in CMEM */
	for (i = start, j = 0; i < end; i++, j++) {
		ppt_val = spt_info[j].paddr >> ATH12K_SPT_4K_ALIGN_OFFSET;

		if (ab->hif.ops->cmem_write32 && ab->hif.bus == ATH12K_BUS_HYBRID)
			ath12k_hif_cmem_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
						ppt_val);
		else
			ath12k_hif_write32(ab, cmem_base + ATH12K_PPT_ADDR_OFFSET(i),
					   ppt_val);
	}

	return 0;
}

static int ath12k_dp_cc_tx_desc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_tx_desc_info *tx_descs, **tx_desc_addr;
	struct list_head *free_list;
	u32 i, j, pool_id, tx_spt_page;
	u32 ppt_idx;
	bool spl_desc;

	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
		for (i = 0; i < ath12k_dp_tx_spt_pages_per_pool(pool_id); i++) {
			tx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*tx_descs),
					   GFP_ATOMIC);

			if (!tx_descs) {
				spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
				/* Caller takes care of TX pending and RX desc cleanup */
				return -ENOMEM;
			}

			tx_spt_page = i + ath12k_dp_tx_spt_page_offset(pool_id);
			ppt_idx = ATH12K_TX_SPT_PAGE_OFFSET + tx_spt_page;

			dp_hw_grp->txbaddr[tx_spt_page] = &tx_descs[0];

			if (i == ath12k_dp_tx_spt_pages_per_pool(pool_id) - 1) {
				free_list = &dp_hw_grp->tx_spl_desc_free_list[pool_id];
				spl_desc = true;
			} else {
				free_list = &dp_hw_grp->tx_desc_free_list[pool_id];
				spl_desc = false;
			}

			for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
				tx_descs[j].desc_id = ath12k_dp_cc_cookie_gen(ppt_idx, j);
				tx_descs[j].pool_id = pool_id;
				tx_descs[j].spl_desc = spl_desc;
				list_add_tail(&tx_descs[j].list, free_list);

				/* Update descriptor VA in SPT */
				tx_desc_addr =
				ath12k_dp_cc_get_desc_addr_ptr(dp->dp_hw_grp->spt_info,
							       tx_spt_page, j);
				*tx_desc_addr = &tx_descs[j];
			}
		}
		spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	}
	return 0;
}

static void ath12k_dp_cc_rx_desc_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *rx_descs;
	u32 i;

	if (!dp->rxbaddr)
		return;

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		rx_descs = dp->rxbaddr[i];
		if (!rx_descs)
			continue;
		kfree(rx_descs);
		dp->rxbaddr[i] = NULL;
	}

	kfree(dp->rxbaddr);
	dp->rxbaddr = NULL;
}

static int ath12k_dp_cc_rx_desc_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *rx_descs;
	int ret;
	u32 i, j;

	dp->rxbaddr = kcalloc(ATH12K_NUM_RX_SPT_PAGES,
			      sizeof(*dp->rxbaddr), GFP_KERNEL);
	if (!dp->rxbaddr)
		return -ENOMEM;

	/* First num_rx_spt_pages of allocated SPT pages are used for RX */
	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		rx_descs = kcalloc(ATH12K_MAX_SPT_ENTRIES, sizeof(*rx_descs),
				   GFP_ATOMIC);
		if (!rx_descs) {
			ret = -ENOMEM;
			goto err;
		}

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++)
			INIT_LIST_HEAD(&rx_descs[j].list);

		dp->rxbaddr[i] = &rx_descs[0];
	}

	return 0;
err:
	ath12k_dp_cc_rx_desc_free(ab);

	return ret;
}

static void ath12k_dp_cc_rx_desc_deinit(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *desc_info;
	struct sk_buff *skb;
	int i, j;

	if (!dp->rxbaddr)
		return;

	spin_lock_bh(&dp->rx_desc_lock);

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		const void *end;

		desc_info = dp->rxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!desc_info[j].in_use) {
				list_del_init(&desc_info[j].list);
				continue;
			}

			skb = desc_info[j].skb;
			if (!skb)
				continue;

			end = desc_info[j].vaddr + DP_RX_BUFFER_SIZE;
			ath12k_core_dmac_inv_range(desc_info[j].vaddr, end);
			dev_kfree_skb_any(skb);
			desc_info[j].skb = NULL;
		}
	}

	spin_unlock_bh(&dp->rx_desc_lock);
}

static int ath12k_dp_cc_rx_desc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *rx_descs, **rx_desc_addr;
	u32 i, j;
	u32 cookie_ppt_idx;

	if (!dp->rxbaddr)
		return -EINVAL;

	spin_lock_bh(&dp->rx_desc_lock);

	/* First num_rx_spt_pages of allocated SPT pages are used for RX */
	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		rx_descs = dp->rxbaddr[i];
		if (!rx_descs) {
			spin_unlock_bh(&dp->rx_desc_lock);
			return -EINVAL;
		}

		memset(rx_descs, 0, ATH12K_MAX_SPT_ENTRIES * sizeof(*rx_descs));

		cookie_ppt_idx = dp->rx_ppt_base + ath12k_dp_rx_spt_page_offset() + i;

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			rx_descs[j].cookie = ath12k_dp_cc_cookie_gen(cookie_ppt_idx, j);
			rx_descs[j].magic = ATH12K_DP_RX_DESC_MAGIC;
			rx_descs[j].device_id = ab->device_id;
			list_add_tail(&rx_descs[j].list, &dp->rx_desc_free_list);

			/* Update descriptor VA in SPT */
			rx_desc_addr = ath12k_dp_cc_get_desc_addr_ptr(dp->spt_info, i, j);
			*rx_desc_addr = &rx_descs[j];
		}
	}

	spin_unlock_bh(&dp->rx_desc_lock);
	return 0;
}

void ath12k_dp_partner_cc_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		if (ag->ab[i]->is_bypassed || ag->ab[i] == ab)
			continue;

		ath12k_dp_cmem_init(ab, ag->ab[i]->dp, ATH12K_DP_RX_DESC);
	}
}
EXPORT_SYMBOL(ath12k_dp_partner_cc_init);

void ath12k_dp_cc_rx_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	ath12k_dp_cc_rx_desc_free(ab);

	if (!dp->spt_info)
		return;

	for (i = 0; i < dp->num_spt_pages; i++) {
		if (!dp->spt_info[i].vaddr)
			continue;

		ath12k_hal_dma_free_coherent(ab->dev, ATH12K_PAGE_SIZE,
					     dp->spt_info[i].vaddr,
					     dp->spt_info[i].paddr);
		dp->spt_info[i].vaddr = NULL;
	}

	kfree(dp->spt_info);
	dp->spt_info = NULL;
}
EXPORT_SYMBOL(ath12k_dp_cc_rx_free);

int ath12k_dp_cc_rx_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret = 0;

	dp->num_spt_pages = ATH12K_NUM_RX_SPT_PAGES;

	if (dp->num_spt_pages > ATH12K_MAX_PPT_ENTRIES)
		dp->num_spt_pages = ATH12K_MAX_PPT_ENTRIES;

	dp->spt_info = kcalloc(dp->num_spt_pages, sizeof(struct ath12k_spt_info),
			       GFP_KERNEL);

	if (!dp->spt_info) {
		ath12k_warn(ab, "SPT page allocation failure");
		return -ENOMEM;
	}

	for (i = 0; i < dp->num_spt_pages; i++) {
		dp->spt_info[i].vaddr =
				ath12k_hal_dma_alloc_coherent(ab->dev,
							      ATH12K_PAGE_SIZE,
							      &dp->spt_info[i].paddr,
							      GFP_KERNEL);

		if (!dp->spt_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		if (dp->spt_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "SPT allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	ret = ath12k_dp_cc_rx_desc_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "rx desc alloc failed, ret %d\n", ret);
		goto free;
	}

	return 0;
free:
	ath12k_dp_cc_rx_free(ab);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_cc_rx_alloc);

int ath12k_dp_cc_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret = 0;

	INIT_LIST_HEAD(&dp->rx_desc_free_list);
	spin_lock_init(&dp->rx_desc_lock);

	if (!dp->spt_info) {
		ath12k_warn(ab, "SPT page for Rx is NULL");
		return -EINVAL;
	}

	dp->rx_ppt_base = ab->device_id * ATH12K_NUM_RX_SPT_PAGES;

	ret = ath12k_dp_cmem_init(ab, dp, ATH12K_DP_RX_DESC);
	if (ret) {
		ath12k_warn(ab, "HW CC Rx cmem init failed %d", ret);
		goto free;
	}

	ret = ath12k_dp_cc_rx_desc_init(ab);
	if (ret) {
		ath12k_warn(ab, "HW CC desc init failed %d", ret);
		goto free;
	}

	ret = ath12k_dp_cmem_init(ab, dp, ATH12K_DP_TX_DESC);
	if (ret) {
		ath12k_warn(ab, "HW CC Tx cmem init failed %d", ret);
		goto free;
	}

	/* Initialize extension descriptor cache */
	ret = ath12k_dp_ext_desc_cache_init(dp);
	if (ret) {
		ath12k_err(ab, "Failed to initialize ext descriptor cache: %d\n", ret);
		goto free;
	}

	return 0;
free:
	ath12k_dp_cc_deinit(ab);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_cc_init);

void ath12k_dp_cc_deinit(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!dp->spt_info)
		return;

	/* RX Descriptor cleanup */
	ath12k_dp_cc_rx_desc_deinit(ab);

	/* TX Descriptor cleanup */
	ath12k_dp_tx_cc_cleanup(ab);
}
EXPORT_SYMBOL(ath12k_dp_cc_deinit);

enum ath12k_dp_eapol_key_type ath12k_dp_get_eapol_subtype(u8 *data)
{
	u8 pkt_type = *(data + EAPOL_PACKET_TYPE_OFFSET);
	u16 key_info, key_data_length;
	enum ath12k_dp_eapol_key_type subtype = DP_EAPOL_KEY_TYPE_MAX;
	u64 *key_nonce;
	bool pairwise;

	if (pkt_type != EAPOL_PACKET_TYPE_KEY)
		return DP_EAPOL_KEY_TYPE_MAX;

	key_info = be16_to_cpu(*(u16 *)(data + EAPOL_KEY_INFO_OFFSET));

	key_data_length = be16_to_cpu(*(u16 *)(data + EAPOL_KEY_DATA_LENGTH_OFFSET));
	key_nonce = (u64 *)(data + EAPOL_WPA_KEY_NONCE_OFFSET);
	pairwise = key_info & EAPOL_WPA_KEY_INFO_KEY_TYPE;

	if (key_info & EAPOL_WPA_KEY_INFO_ACK) {
		if (key_info &
		   (EAPOL_WPA_KEY_INFO_MIC | EAPOL_WPA_KEY_INFO_ENCR_KEY_DATA))
			subtype = pairwise ?
				DP_EAPOL_KEY_TYPE_M3 :  DP_EAPOL_KEY_TYPE_G1;
		else
			subtype =  DP_EAPOL_KEY_TYPE_M1;
	} else {
		if (key_data_length == 0 ||
		    !((*key_nonce) || (*(key_nonce + 1)) ||
		      (*(key_nonce + 2)) || (*(key_nonce + 3))))
			subtype = pairwise ?
				DP_EAPOL_KEY_TYPE_M4 :  DP_EAPOL_KEY_TYPE_G2;
		else
			subtype =  DP_EAPOL_KEY_TYPE_M2;
	}
	return subtype;
}
EXPORT_SYMBOL(ath12k_dp_get_eapol_subtype);

int ath12k_dp_alloc_reoq_lut(struct ath12k_base *ab,
			     struct ath12k_reo_q_addr_lut *lut)
{
	lut->size =  ab->hal.hal_params->reoq_lut_size + HAL_REO_QLUT_ADDR_ALIGN - 1;
	lut->vaddr_unaligned = ath12k_hal_dma_alloc_coherent(ab->dev, lut->size,
							      &lut->paddr_unaligned,
							      GFP_KERNEL | __GFP_ZERO);
	if (!lut->vaddr_unaligned)
		return -ENOMEM;

	lut->vaddr = PTR_ALIGN(lut->vaddr_unaligned, HAL_REO_QLUT_ADDR_ALIGN);
	lut->paddr = lut->paddr_unaligned +
		     ((unsigned long)lut->vaddr - (unsigned long)lut->vaddr_unaligned);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_alloc_reoq_lut);

static int ath12k_dp_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	dp->ab = ab;

	spin_lock_init(&dp->dp_lock);
	INIT_LIST_HEAD(&dp->neighbor_peers);
	hash_init(dp->link_peer_htbl);

	return 0;
}

void ath12k_dp_cmn_device_deinit(struct ath12k_dp *dp)
{
	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &dp->ab->dev_flags))
		return;

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &dp->ab->dev_flags) ||
	    dp->ab->umcmn_fatal_received) {
		ath12k_info(dp->ab, "Skip DP deinit during Q6 BCR RESET\n");
		return;
	}

	ath12k_dp_arch_op_mlo_deinit(dp);
	ath12k_dp_arch_op_device_deinit(dp);
}

void ath12k_dp_init_ring_size(struct ath12k_base *ab)
{
	ath12k_dp_reo_dst_ring_size[0] = DP_REO_DST_RING0_SIZE;
	ath12k_dp_reo_dst_ring_size[1] = DP_REO_DST_RING1_SIZE;
	ath12k_dp_reo_dst_ring_size[2] = DP_REO_DST_RING2_SIZE;
	ath12k_dp_reo_dst_ring_size[3] = DP_REO_DST_RING3_SIZE;
	ath12k_dp_reo_dst_ring_size[4] = DP_REO_DST_RING4_SIZE;

	ath12k_dp_tcl_data_ring_size[0] = DP_TCL_DATA_RING0_SIZE;
	ath12k_dp_tcl_data_ring_size[1] = DP_TCL_DATA_RING1_SIZE;
	ath12k_dp_tcl_data_ring_size[2] = DP_TCL_DATA_RING2_SIZE;
	ath12k_dp_tcl_data_ring_size[3] = DP_TCL_DATA_RING3_SIZE;
	ath12k_dp_tcl_data_ring_size[4] = DP_TCL_DATA_RING4_SIZE;

	ath12k_dp_tx_comp_ring_size[0] = DP_TX_COMP_RING0_SIZE;
	ath12k_dp_tx_comp_ring_size[1] = DP_TX_COMP_RING1_SIZE;
	ath12k_dp_tx_comp_ring_size[2] = DP_TX_COMP_RING2_SIZE;
	ath12k_dp_tx_comp_ring_size[3] = DP_TX_COMP_RING3_SIZE;
	ath12k_dp_tx_comp_ring_size[4] = DP_TX_COMP_RING4_SIZE;
}
EXPORT_SYMBOL(ath12k_dp_init_ring_size);

int ath12k_dp_cmn_device_init(struct ath12k_dp *dp)
{
	int ret;

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &dp->ab->dev_flags) ||
	    dp->ab->umcmn_fatal_received) {
		ath12k_info(dp->ab, "Skip DP re-init during Q6 BCR RESET\n");
		return 0;
	}

	ret = ath12k_dp_arch_op_device_init(dp);
	if (ret)
		return ret;

	ret = ath12k_dp_setup(dp->ab);
	if (ret)
		return ret;

	return 0;
}

static void ath12k_dp_tx_spt_free_and_deinit(struct ath12k_base *ab,
					     struct ath12k_dp_hw_group *dp_hw_grp)
{
	int i, j;
	u32 pool_id, tx_spt_page;

	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
		for (j = 0; j < ath12k_dp_tx_spt_pages_per_pool(pool_id); j++) {
			tx_spt_page = j + ath12k_dp_tx_spt_page_offset(pool_id);
			if (!dp_hw_grp->txbaddr || !dp_hw_grp->txbaddr[tx_spt_page])
				continue;
			kfree(dp_hw_grp->txbaddr[tx_spt_page]);
			dp_hw_grp->txbaddr[tx_spt_page] = NULL;
		}
		spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	}

	kfree(dp_hw_grp->txbaddr);
	dp_hw_grp->txbaddr = NULL;

	if (dp_hw_grp->spt_info) {
		for (i = 0; i < dp_hw_grp->num_spt_pages; i++) {
			if (!dp_hw_grp->spt_info[i].vaddr)
				continue;
			ath12k_hal_dma_free_coherent(dp_hw_grp->tx_spt_dev,
						     ATH12K_PAGE_SIZE,
						     dp_hw_grp->spt_info[i].vaddr,
						     dp_hw_grp->spt_info[i].paddr);
			dp_hw_grp->spt_info[i].vaddr = NULL;
		}
		kfree(dp_hw_grp->spt_info);
		dp_hw_grp->spt_info = NULL;
	}

	dp_hw_grp->tx_spt_dev = NULL;
	dp_hw_grp->tx_desc_initialized = false;
}

static int ath12k_dp_tx_spt_alloc_and_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	int i, ret = 0;

	mutex_lock(&dp_hw_grp->tx_init_lock);

	if (dp_hw_grp->tx_desc_initialized)
		goto unlock;

	ath12k_dp_tx_desc_count[0] = DP_TX_DESC_COUNT_POOL0;
	ath12k_dp_tx_desc_count[1] = DP_TX_DESC_COUNT_POOL1;
	ath12k_dp_tx_desc_count[2] = DP_TX_DESC_COUNT_POOL2;
	ath12k_dp_tx_desc_count[3] = DP_TX_DESC_COUNT_POOL3;
	ath12k_dp_tx_desc_count[4] = DP_TX_DESC_COUNT_POOL4;
	ath12k_dp_num_ppeds_tx_spt_pages = ATH12K_NUM_PPEDS_TX_SPT_PAGES;

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		INIT_LIST_HEAD(&dp_hw_grp->tx_desc_free_list[i]);
		INIT_LIST_HEAD(&dp_hw_grp->tx_spl_desc_free_list[i]);
		spin_lock_init(&dp_hw_grp->tx_desc_lock[i]);
	}

	dp_hw_grp->num_spt_pages = ath12k_dp_tx_spt_page_offset(ATH12K_HW_MAX_QUEUES);

	if (dp_hw_grp->num_spt_pages > ATH12K_MAX_PPT_ENTRIES)
		dp_hw_grp->num_spt_pages = ATH12K_MAX_PPT_ENTRIES;

	dp_hw_grp->txbaddr = kcalloc(dp_hw_grp->num_spt_pages,
				     sizeof(*dp_hw_grp->txbaddr), GFP_KERNEL);
	if (!dp_hw_grp->txbaddr) {
		ret = -ENOMEM;
		goto unlock;
	}

	dp_hw_grp->spt_info = kcalloc(dp_hw_grp->num_spt_pages,
				      sizeof(struct ath12k_spt_info), GFP_KERNEL);
	if (!dp_hw_grp->spt_info) {
		ret = -ENOMEM;
		kfree(dp_hw_grp->txbaddr);
		dp_hw_grp->txbaddr = NULL;
		goto free;
	}

	dp_hw_grp->tx_spt_dev = ab->dev;

	for (i = 0; i < dp_hw_grp->num_spt_pages; i++) {
		dp_hw_grp->spt_info[i].vaddr =
			ath12k_hal_dma_alloc_coherent(ab->dev,
						      ATH12K_PAGE_SIZE,
						      &dp_hw_grp->spt_info[i].paddr,
						      GFP_KERNEL);
		if (!dp_hw_grp->spt_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		if (dp_hw_grp->spt_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "SPT allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	ret = ath12k_dp_cc_tx_desc_init(ab);
	if (ret) {
		ath12k_warn(ab, "HW CC TX desc init failed %d", ret);
		goto free;
	}

	dp_hw_grp->tx_desc_initialized = true;

unlock:
	mutex_unlock(&dp_hw_grp->tx_init_lock);
	return ret;

free:
	mutex_unlock(&dp_hw_grp->tx_init_lock);
	ath12k_dp_tx_spt_free_and_deinit(ab, dp_hw_grp);
	return ret;
}

void ath12k_dp_cmn_hw_group_unassign(struct ath12k_dp *dp,
				     struct ath12k_hw_group *ag)
{
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < DP_TOTAL_REO_DST_RINGS; i++) {
		if (!dp_hw_grp->rx_status_buf[i])
			continue;
		kfree(dp_hw_grp->rx_status_buf[i]);
		dp_hw_grp->rx_status_buf[i] = NULL;
	}

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		if (!dp_hw_grp->tx_status_buf[i])
			continue;
		kfree(dp_hw_grp->tx_status_buf[i]);
		dp_hw_grp->tx_status_buf[i] = NULL;
	}

	if (dp_hw_grp->fst) {
		ath12k_dp_rx_fst_detach(dp->ab, dp_hw_grp->fst);
		dp_hw_grp->fst = NULL;
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_ipa_hw_group_deinit(dp_hw_grp);
#endif /* CPTCFG_QCN_EXTN */

	if (dp_hw_grp->tx_desc_initialized && dp->dev == dp_hw_grp->tx_spt_dev)
		ath12k_dp_tx_spt_free_and_deinit(dp->ab, dp_hw_grp);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (dp_hw_grp->ppeds_tx_desc_initialized &&
	    dp->dev == dp_hw_grp->ppeds_tx_spt_dev)
		ath12k_dp_ppeds_spt_free_and_deinit(dp->ab, dp_hw_grp);
#endif
	dp_hw_grp->dp[dp->device_id] = NULL;
	dp->dp_hw_grp = NULL;
	dp->device_id = ATH12K_INVALID_DEVICE_ID;
}

void ath12k_dp_cmn_hw_group_assign(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	int i, ret;

	dp->dp_hw_grp = dp_hw_grp;
	dp->device_id = ab->device_id;
	dp_hw_grp->dp[dp->device_id] = dp;

	if (!dp_hw_grp->fst)
		dp_hw_grp->fst = ath12k_dp_rx_fst_attach(ab);

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++) {
		if (dp_hw_grp->tx_status_buf[i])
			continue;

		/* Each arch tx completion handler can use this buffer by typecasting its own
		 * entry struct (aligned to 32 or 64 bytes).
		 */
		dp_hw_grp->tx_status_buf[i] = kzalloc(TX_STATUS_BUFFER_SIZE, GFP_KERNEL);
	}

	for (i = 0; i < DP_TOTAL_REO_DST_RINGS; i++) {
		if (dp_hw_grp->rx_status_buf[i])
			continue;

		/* Each arch rx process handler can use this buffer by typecasting its own
		 * entry struct (aligned to 32 bytes).
		 */
		dp_hw_grp->rx_status_buf[i] = kzalloc(RX_STATUS_BUFFER_SIZE, GFP_KERNEL);
		if (!dp_hw_grp->rx_status_buf[i]) {
			ath12k_err(ab, "Failed to allocate rx_status_buf[%d]", i);
			BUG_ON(1);
		}
	}

	ret = ath12k_dp_tx_spt_alloc_and_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc and init TX SPT pages %d\n", ret);
		for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
			if (!dp_hw_grp->rx_status_buf[i])
				continue;

			kfree(dp_hw_grp->rx_status_buf[i]);
			dp_hw_grp->rx_status_buf[i] = NULL;
		}
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_tx_hw_group_extn_init(dp_hw_grp);
#endif
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_dp_ppeds_spt_alloc_and_init(ab);
	if (ret)
		ath12k_warn(ab, "failed to alloc and init PPEDS SPT pages %d\n", ret);
#endif
#ifdef CPTCFG_QCN_EXTN
	ret = ath12k_dp_ipa_hw_group_init(ab, dp_hw_grp);
	if (ret) {
		ath12k_err(ab, "Failed to allocate IPA global context: %d\n", ret);
	}
#endif /* CPTCFG_QCN_EXTN */
}

int ath12k_dp_srng_alloc_aligned(struct ath12k_base *ab,
				 struct dp_srng *ring,
				 int num_entries,
				 int entry_sz,
				 bool cached)
{
	size_t ring_sz = num_entries * entry_sz;
	void *addr;

	ring->cached = cached;
	ring->size = ring_sz;
	ring->vaddr = NULL;
	ring->paddr = 0;

	if (cached)
		addr = ath12k_hal_dma_alloc_noncoherent(ab->dev, ring->size,
							&ring->paddr_unaligned,
							DMA_FROM_DEVICE,
							GFP_KERNEL);
	else
		addr = ath12k_hal_dma_alloc_coherent(ab->dev, ring->size,
						     &ring->paddr_unaligned,
						     GFP_KERNEL);

	if (!addr)
		return -ENOMEM;

	ring->vaddr_unaligned = addr;

	if (IS_ALIGNED((unsigned long)ring->vaddr_unaligned, HAL_RING_BASE_ALIGN)) {
		ring->vaddr = ring->vaddr_unaligned;
		ring->paddr = ring->paddr_unaligned;
	} else {
		size_t unaligned_sz = ring->size;

		ring->size = ring_sz + HAL_RING_BASE_ALIGN - 1;
		if (cached) {
			ath12k_hal_dma_free_noncoherent(ab->dev, unaligned_sz,
							ring->vaddr_unaligned,
							ring->paddr_unaligned,
							DMA_FROM_DEVICE);
			addr = ath12k_hal_dma_alloc_noncoherent(ab->dev, ring->size,
								&ring->paddr_unaligned,
								DMA_FROM_DEVICE,
								GFP_KERNEL);
		} else {
			ath12k_hal_dma_free_coherent(ab->dev, unaligned_sz,
						     ring->vaddr_unaligned,
						     ring->paddr_unaligned);
			addr = ath12k_hal_dma_alloc_coherent(ab->dev, ring->size,
							     &ring->paddr_unaligned,
							     GFP_KERNEL);
		}

		ring->vaddr_unaligned = addr;

		if (!ring->vaddr_unaligned) {
			ring->paddr_unaligned = 0;
			return -ENOMEM;
		}

		ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
		ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
						(unsigned long)ring->vaddr_unaligned);
	}

	return 0;
}

void ath12k_dp_srng_hw_disable(struct ath12k_base *ab, struct dp_srng *ring)
{
        struct hal_srng *srng = &ab->hal.srng_list[ring->ring_id];

	ath12k_hal_srng_hw_disable(ab, srng);
}
EXPORT_SYMBOL(ath12k_dp_srng_hw_disable);

void ath12k_dp_srng_hw_ring_disable(struct ath12k_base *ab)
{
        struct ath12k_dp *dp;
        int i;

        dp = ath12k_ab_to_dp(ab);
	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		if (!dp->reo_dst_ring[i].vaddr_unaligned)
			continue;

		ath12k_dp_srng_hw_disable(ab, &dp->reo_dst_ring[i]);
	}
        ath12k_dp_srng_hw_disable(ab, &dp->wbm_desc_rel_ring);

        for(i = 0; i < ab->hw_params->max_tx_ring; i++) {
                ath12k_dp_srng_hw_disable(ab, &dp->tx_ring[i].tcl_data_ring);
                ath12k_dp_srng_hw_disable(ab, &dp->tx_ring[i].tcl_comp_ring);
        }
        ath12k_dp_srng_hw_disable(ab, &dp->reo_reinject_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->rx_rel_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_except_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_cmd_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->reo_status_ring);
        ath12k_dp_srng_hw_disable(ab, &dp->wbm_idle_ring);
}
EXPORT_SYMBOL(ath12k_dp_srng_hw_ring_disable);

void ath12k_dp_umac_rx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_dp *dp;
	struct sk_buff *skb;
	LIST_HEAD(used_list);
	int i, j;

	dp = ath12k_ab_to_dp(ab);

	/* RX Descriptor cleanup */
	spin_lock_bh(&dp->rx_desc_lock);

	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		desc_info = dp->rxbaddr[i];

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			if (!desc_info[j].in_use)
				continue;

			skb = desc_info[j].skb;
			if (!skb)
				continue;

			/* Add to replenish list - keep everything intact */
			list_add_tail(&desc_info[j].list, &used_list);
		}
	}

	spin_unlock_bh(&dp->rx_desc_lock);

	/* Feed descriptors to replenish */
	if (!list_empty(&used_list)) {
		struct hal_srng *refill_srng;
		int ring_id = ath12k_dp_arch_fetch_rx_desc_replenish_ring_id(dp);

		refill_srng = &ab->hal.srng_list[ring_id];
		ath12k_dp_rx_bufs_replenish(dp, refill_srng, &used_list, true);
	}
}
EXPORT_SYMBOL(ath12k_dp_umac_rx_desc_cleanup);

void ath12k_dp_umac_tx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_tx_desc_info *tx_desc_info;
	struct ath12k_dp *dp;
	struct sk_buff *skb;
	int i, j, k, pool_id;
	u32 tx_spt_page;
	dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	u32 *tx_desc_used_cnt;
	int cpu;

	/* TX Descriptor cleanup */
	for (pool_id = 0; pool_id < ATH12K_HW_MAX_QUEUES; pool_id++) {
		spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);

		for (j = 0; j < ath12k_dp_tx_spt_pages_per_pool(pool_id); j++) {
			tx_spt_page = j + ath12k_dp_tx_spt_page_offset(pool_id);
			if (!dp_hw_grp->txbaddr || !dp_hw_grp->txbaddr[tx_spt_page])
				continue;
			tx_desc_info = dp_hw_grp->txbaddr[tx_spt_page];
			for (k = 0; k < ATH12K_MAX_SPT_ENTRIES; k++) {
				if (!tx_desc_info[k].in_use)
					continue;

				skb = tx_desc_info[k].skb;
				if (!skb)
					continue;

				tx_desc_info[k].skb = NULL;

				/* Cleanup extension descriptor based on type */
				if (tx_desc_info[k].ext_kmem) {
					ath12k_dp_tx_ext_desc_free(dp, &tx_desc_info[k]);
				} else if (tx_desc_info[k].skb_ext_desc) {
					ath12k_core_dma_unmap_single(ab->dev,
							tx_desc_info[k].paddr_ext_desc,
							tx_desc_info[k].skb_ext_desc->len,
							DMA_TO_DEVICE);
					skb_queue_tail(&ab->dp_umac_reset.tx_skb_queue,
						       tx_desc_info[k].skb_ext_desc);
				}

				ath12k_core_dma_unmap_single(ab->dev,
							     tx_desc_info[k].paddr,
							     tx_desc_info[k].len,
							     DMA_TO_DEVICE);

				/* Save SKB to queue instead of freeing */
				skb_queue_tail(&ab->dp_umac_reset.tx_skb_queue, skb);
				ath12k_dp_tx_release_txbuf_nolock(dp, &tx_desc_info[k],
								  pool_id);
			}
		}

		spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	}

	rcu_read_lock();

	for (j = 0; j < ATH12K_MAX_SOCS; j++) {
		dp = dp_hw_grp->dp[j];
		if (!dp)
			continue;

		for (i = 0; i < ab->num_radios; i++) {
			struct ath12k_pdev_dp *dp_pdev = ath12k_dp_to_dp_pdev(dp, i);

			if (dp_pdev)
				atomic_set(&dp_pdev->num_tx_pending, 0);
		}
	}

	rcu_read_unlock();

	for_each_possible_cpu(cpu) {
		tx_desc_used_cnt = per_cpu_ptr(dp_hw_grp->tx_desc_used_cnt, cpu);
		*tx_desc_used_cnt = 0;
	}

}
EXPORT_SYMBOL(ath12k_dp_umac_tx_desc_cleanup);

size_t ath12k_dp_get_req_entries_from_buf_ring(struct ath12k_base *ab,
		struct hal_srng *srng,
		struct list_head *list, uint8_t pool_type)
{
        struct ath12k_dp *dp;
        size_t num_free, req_entries;

        dp = ath12k_ab_to_dp(ab);
        spin_lock_bh(&srng->lock);
        ath12k_hal_srng_access_begin(ab, srng);
        num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
        if (!num_free) {
                ath12k_hal_srng_access_end(ab, srng);
                spin_unlock_bh(&srng->lock);
                return 0;
        }

        spin_lock_bh(&dp->rx_desc_lock);
        req_entries = ath12k_dp_list_cut_nodes(list,
					&dp->rx_desc_free_list,
					num_free, pool_type);
        spin_unlock_bh(&dp->rx_desc_lock);

        ath12k_hal_srng_access_end(ab, srng);
        spin_unlock_bh(&srng->lock);

        return req_entries;
}
EXPORT_SYMBOL(ath12k_dp_get_req_entries_from_buf_ring);

void ath12k_dp_cmn_update_hw_links(struct ath12k_dp *dp,
				   struct ath12k_hw_group *ag,
				   struct ath12k *ar)
{
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;

	lockdep_assert_held(&ag->mutex);

	dp_hw_grp->hw_links[ar->hw_link_id].device_id = dp->device_id;
	dp_hw_grp->hw_links[ar->hw_link_id].pdev_idx = ar->pdev_idx;
}

void ath12k_dp_reoq_lut_addr_reset(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags)){
		if (dp->reoq_lut.vaddr_unaligned)
			ath12k_hal_write_reoq_lut_addr(ab, 0);

		if (dp->ml_reoq_lut.vaddr_unaligned)
			ath12k_hal_write_ml_reoq_lut_addr(ab, 0);
	}
}

static void
ath12k_dp_update_proto_peer_tx_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
				     struct ath12k_dp_peer_stats *src_peer_stats,
				     u8 index)
{
	u8 j, k;

	if (!dst_peer_stats->proto || !src_peer_stats->proto)
		return;

	for (j = 0; j < TX_COMP_MAX; j++) {
		for (k = 0; k < DP_PKT_TYPE_L3_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l3[k] =
				src_peer_stats->proto->tx[index][j].l3[k];

		for (k = 0; k < DP_PKT_TYPE_L4_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l4[k] =
				src_peer_stats->proto->tx[index][j].l4[k];

		for (k = 0; k < DP_PKT_TYPE_L5_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l5[k] =
				src_peer_stats->proto->tx[index][j].l5[k];
	}
}

static void
ath12k_dp_update_proto_peer_rx_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
				     struct ath12k_dp_peer_stats *src_peer_stats,
				     u8 index)
{
	u8 j, k;

	if (!dst_peer_stats->proto || !src_peer_stats->proto)
		return;

	for (j = 0; j < RX_RECV_MAX; j++) {
		for (k = 0; k < DP_PKT_TYPE_L3_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l3[k] =
				src_peer_stats->proto->rx[index][j].l3[k];

		for (k = 0; k < DP_PKT_TYPE_L4_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l4[k] =
				src_peer_stats->proto->rx[index][j].l4[k];

		for (k = 0; k < DP_PKT_TYPE_L5_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l5[k] =
				src_peer_stats->proto->rx[index][j].l5[k];
	}
}

static void
ath12k_dp_aggr_proto_peer_tx_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
				   struct ath12k_dp_peer_stats *src_peer_stats,
				   u8 index)
{
	u8 j, k;

	if (!dst_peer_stats->proto || !src_peer_stats->proto)
		return;

	for (j = 0; j < TX_COMP_MAX; j++) {
		for (k = 0; k < DP_PKT_TYPE_L3_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l3[k] +=
				src_peer_stats->proto->tx[index][j].l3[k];

		for (k = 0; k < DP_PKT_TYPE_L4_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l4[k] +=
				src_peer_stats->proto->tx[index][j].l4[k];

		for (k = 0; k < DP_PKT_TYPE_L5_MAX; k++)
			dst_peer_stats->proto->tx[index][j].l5[k] +=
				src_peer_stats->proto->tx[index][j].l5[k];
	}
}

static void
ath12k_dp_aggr_proto_peer_rx_stats(struct ath12k_dp_peer_stats *dst_peer_stats,
				   struct ath12k_dp_peer_stats *src_peer_stats,
				   u8 index)
{
	u8 j, k;

	if (!dst_peer_stats->proto || !src_peer_stats->proto)
		return;

	for (j = 0; j < RX_RECV_MAX; j++) {
		for (k = 0; k < DP_PKT_TYPE_L3_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l3[k] +=
				src_peer_stats->proto->rx[index][j].l3[k];

		for (k = 0; k < DP_PKT_TYPE_L4_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l4[k] +=
				src_peer_stats->proto->rx[index][j].l4[k];

		for (k = 0; k < DP_PKT_TYPE_L5_MAX; k++)
			dst_peer_stats->proto->rx[index][j].l5[k] +=
				src_peer_stats->proto->rx[index][j].l5[k];
	}
}

static void ath12k_dp_aggr_per_pkt_peer_stats(struct ath12k_pdev_dp *dp_pdev,
					      struct ath12k_dp_peer_stats *dst_peer_stats,
					      struct ath12k_dp_peer_stats *src_peer_stats,
					      bool is_vdev_peer)
{
	int i, j;

	/*tx peer stats*/
	for(i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		dst_peer_stats->tx[i].comp_pkt.packets +=
			src_peer_stats->tx[i].comp_pkt.packets;
		dst_peer_stats->tx[i].comp_pkt.bytes +=
			src_peer_stats->tx[i].comp_pkt.bytes;
		dst_peer_stats->tx[i].tx_success.packets +=
			src_peer_stats->tx[i].tx_success.packets;
		dst_peer_stats->tx[i].tx_success.bytes +=
			src_peer_stats->tx[i].tx_success.bytes;
		dst_peer_stats->tx[i].tx_failed +=
			src_peer_stats->tx[i].tx_failed;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_proto_stats_enabled(dp_pdev))
				ath12k_dp_aggr_proto_peer_tx_stats(dst_peer_stats,
								   src_peer_stats, i);
		}
	}

	/* HW peer stats report these counters OOB without debug stats.
	 * Non-HW targets keep the existing debug-stats gated path below.
	 */
	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
		dst_peer_stats->tx_dbg.retry_count +=
			src_peer_stats->tx_dbg.retry_count;
		dst_peer_stats->tx_dbg.total_msdu_retries +=
			src_peer_stats->tx_dbg.total_msdu_retries;
		dst_peer_stats->tx_dbg.ucast.packets +=
			src_peer_stats->tx_dbg.ucast.packets;
		dst_peer_stats->tx_dbg.ucast.bytes +=
			src_peer_stats->tx_dbg.ucast.bytes;
	}

	if (ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
				dst_peer_stats->tx_dbg.wbm_rel_reason[j] +=
					src_peer_stats->tx_dbg.wbm_rel_reason[j];
			for (j = 0; j < HAL_WBM_TQM_REL_REASON_MAX; j++)
				dst_peer_stats->tx_dbg.tqm_rel_reason[j] +=
					src_peer_stats->tx_dbg.tqm_rel_reason[j];

			dst_peer_stats->tx_dbg.release_src_not_tqm +=
				src_peer_stats->tx_dbg.release_src_not_tqm;
			if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
				dst_peer_stats->tx_dbg.retry_count +=
					src_peer_stats->tx_dbg.retry_count;
				dst_peer_stats->tx_dbg.total_msdu_retries +=
					src_peer_stats->tx_dbg.total_msdu_retries;
				dst_peer_stats->tx_dbg.ucast.packets +=
					src_peer_stats->tx_dbg.ucast.packets;
				dst_peer_stats->tx_dbg.ucast.bytes +=
					src_peer_stats->tx_dbg.ucast.bytes;
			}
			dst_peer_stats->tx_dbg.multiple_retry_count +=
				src_peer_stats->tx_dbg.multiple_retry_count;
			dst_peer_stats->tx_dbg.ofdma +=
				src_peer_stats->tx_dbg.ofdma;
			dst_peer_stats->tx_dbg.amsdu_cnt +=
				src_peer_stats->tx_dbg.amsdu_cnt;
			dst_peer_stats->tx_dbg.non_amsdu_cnt +=
				src_peer_stats->tx_dbg.non_amsdu_cnt;
			dst_peer_stats->tx_dbg.inval_link_id_pkt_cnt +=
				src_peer_stats->tx_dbg.inval_link_id_pkt_cnt;
			if (is_vdev_peer) {
				dst_peer_stats->tx_dbg.mcast.packets +=
					src_peer_stats->tx_dbg.mcast.packets;
				dst_peer_stats->tx_dbg.mcast.bytes +=
					src_peer_stats->tx_dbg.mcast.bytes;
				dst_peer_stats->tx_dbg.bcast.packets +=
					src_peer_stats->tx_dbg.bcast.packets;
				dst_peer_stats->tx_dbg.bcast.bytes +=
					src_peer_stats->tx_dbg.bcast.bytes;
			}
		}
	}

	/*rx peer stats*/
	for(i = 0; i < DP_REO_DST_RING_MAX; i++) {
		dst_peer_stats->rx[i].recv_from_reo.packets +=
			src_peer_stats->rx[i].recv_from_reo.packets;
		dst_peer_stats->rx[i].recv_from_reo.bytes +=
			src_peer_stats->rx[i].recv_from_reo.bytes;

		dst_peer_stats->rx[i].sent_to_stack_fast.packets +=
			src_peer_stats->rx[i].sent_to_stack_fast.packets;
		dst_peer_stats->rx[i].sent_to_stack_fast.bytes +=
			src_peer_stats->rx[i].sent_to_stack_fast.bytes;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_proto_stats_enabled(dp_pdev))
				ath12k_dp_aggr_proto_peer_rx_stats(dst_peer_stats,
								   src_peer_stats, i);
		}
		dst_peer_stats->rx[i].sent_to_stack.packets +=
			src_peer_stats->rx[i].sent_to_stack.packets;
		dst_peer_stats->rx[i].sent_to_stack.bytes +=
			src_peer_stats->rx[i].sent_to_stack.bytes;
	}

	/* HW peer stats report these counters OOB without debug stats.
	 * Non-HW targets keep the existing debug-stats gated path below.
	 */
	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
		dst_peer_stats->rx_dbg.ucast.packets +=
			src_peer_stats->rx_dbg.ucast.packets;
		dst_peer_stats->rx_dbg.ucast.bytes +=
			src_peer_stats->rx_dbg.ucast.bytes;
	}

	if (ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			dst_peer_stats->rx_dbg.mcast.packets +=
				src_peer_stats->rx_dbg.mcast.packets;
			dst_peer_stats->rx_dbg.mcast.bytes +=
				src_peer_stats->rx_dbg.mcast.bytes;
			if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
				dst_peer_stats->rx_dbg.ucast.packets +=
					src_peer_stats->rx_dbg.ucast.packets;
				dst_peer_stats->rx_dbg.ucast.bytes +=
					src_peer_stats->rx_dbg.ucast.bytes;
			}
			dst_peer_stats->rx_dbg.non_amsdu +=
				src_peer_stats->rx_dbg.non_amsdu;
			dst_peer_stats->rx_dbg.msdu_part_of_amsdu +=
				src_peer_stats->rx_dbg.msdu_part_of_amsdu;
			dst_peer_stats->rx_dbg.mpdu_retry +=
				src_peer_stats->rx_dbg.mpdu_retry;
			dst_peer_stats->rx_dbg.sg.packets +=
				src_peer_stats->rx_dbg.sg.packets;
			dst_peer_stats->rx_dbg.sg.bytes +=
				src_peer_stats->rx_dbg.sg.bytes;
		}
	}

	/*rx error stats*/
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		dst_peer_stats->wbm_err.rxdma_error[i] +=
			src_peer_stats->wbm_err.rxdma_error[i];
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		dst_peer_stats->wbm_err.reo_error[i] +=
			src_peer_stats->wbm_err.reo_error[i];

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	dst_peer_stats->mmesh_stat.no_qos +=
		src_peer_stats->mmesh_stat.no_qos;

	dst_peer_stats->mmesh_stat.no_enc +=
		src_peer_stats->mmesh_stat.no_enc;

	dst_peer_stats->mmesh_stat.txinfo +=
		src_peer_stats->mmesh_stat.txinfo;

	dst_peer_stats->mmesh_stat.auto_rate +=
		src_peer_stats->mmesh_stat.auto_rate;

	dst_peer_stats->mmesh_stat.filter_drop +=
		src_peer_stats->mmesh_stat.filter_drop;

	dst_peer_stats->mmesh_stat.direct +=
		src_peer_stats->mmesh_stat.direct;

	dst_peer_stats->mmesh_stat.tofw +=
		src_peer_stats->mmesh_stat.tofw;

	dst_peer_stats->mmesh_stat.rxhdr_updt +=
		src_peer_stats->mmesh_stat.rxhdr_updt;

	dst_peer_stats->mmesh_stat.rxkey_lookp_up_fail +=
		src_peer_stats->mmesh_stat.rxkey_lookp_up_fail;

	dst_peer_stats->mmesh_stat.rxkey_lookp_up_succ +=
		src_peer_stats->mmesh_stat.rxkey_lookp_up_succ;

	dst_peer_stats->mmesh_stat.rxhdr_alloc_fail +=
		src_peer_stats->mmesh_stat.rxhdr_alloc_fail;
#endif
}

/**
 * ath12k_dp_update_ext_htt_stats - Copy tx_ppdu_stats into an aggregation buffer
 * @dst: Destination tx_ppdu_stats buffer
 * @src: Source link peer tx_ppdu_stats
 *
 * Copies the full tx_ppdu_stats structure for per-peer or pdev aggregation.
 * Called only when DP_ENABLE_TX_PPDU_STATS is enabled.
 */
static void
ath12k_dp_update_tx_ppdu_stats(struct ath12k_htt_tx_ppdu_stats *dst_tx_ppdu_stats,
			       struct ath12k_htt_tx_ppdu_stats *src_tx_ppdu_stats)
{
	if (!dst_tx_ppdu_stats || !src_tx_ppdu_stats)
		return;

	memcpy(dst_tx_ppdu_stats, src_tx_ppdu_stats,
	       sizeof(struct ath12k_htt_tx_ppdu_stats));
}

static void ath12k_dp_update_per_pkt_peer_stats(struct ath12k_pdev_dp *dp_pdev,
						struct ath12k_dp_peer_stats *dst_peer_stats,
						struct ath12k_dp_peer_stats *src_peer_stats,
						bool is_vdev_peer)
{
	int i, j;

	/*tx peer stats*/
	for(i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		dst_peer_stats->tx[i].comp_pkt.packets =
			src_peer_stats->tx[i].comp_pkt.packets;
		dst_peer_stats->tx[i].comp_pkt.bytes =
			src_peer_stats->tx[i].comp_pkt.bytes;
		dst_peer_stats->tx[i].tx_success.packets =
			src_peer_stats->tx[i].tx_success.packets;
		dst_peer_stats->tx[i].tx_success.bytes =
			src_peer_stats->tx[i].tx_success.bytes;
		dst_peer_stats->tx[i].tx_failed =
			src_peer_stats->tx[i].tx_failed;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_proto_stats_enabled(dp_pdev))
				ath12k_dp_update_proto_peer_tx_stats(dst_peer_stats,
								     src_peer_stats, i);
		}
	}

	/* HW peer stats report these counters OOB without debug stats.
	 * Non-HW targets keep the existing debug-stats gated path below.
	 */
	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
		dst_peer_stats->tx_dbg.retry_count =
			src_peer_stats->tx_dbg.retry_count;
		dst_peer_stats->tx_dbg.total_msdu_retries =
			src_peer_stats->tx_dbg.total_msdu_retries;
		dst_peer_stats->tx_dbg.ucast.packets =
			src_peer_stats->tx_dbg.ucast.packets;
		dst_peer_stats->tx_dbg.ucast.bytes =
			src_peer_stats->tx_dbg.ucast.bytes;
	}

	if (ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
				dst_peer_stats->tx_dbg.wbm_rel_reason[j] =
					src_peer_stats->tx_dbg.wbm_rel_reason[j];

			for (j = 0; j < HAL_WBM_TQM_REL_REASON_MAX; j++)
				dst_peer_stats->tx_dbg.tqm_rel_reason[j] =
					src_peer_stats->tx_dbg.tqm_rel_reason[j];

			dst_peer_stats->tx_dbg.release_src_not_tqm =
				src_peer_stats->tx_dbg.release_src_not_tqm;
			if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
				dst_peer_stats->tx_dbg.retry_count =
					src_peer_stats->tx_dbg.retry_count;
				dst_peer_stats->tx_dbg.total_msdu_retries =
					src_peer_stats->tx_dbg.total_msdu_retries;
				dst_peer_stats->tx_dbg.ucast.packets =
					src_peer_stats->tx_dbg.ucast.packets;
				dst_peer_stats->tx_dbg.ucast.bytes =
					src_peer_stats->tx_dbg.ucast.bytes;
			}
			dst_peer_stats->tx_dbg.multiple_retry_count =
				src_peer_stats->tx_dbg.multiple_retry_count;
			dst_peer_stats->tx_dbg.ofdma =
				src_peer_stats->tx_dbg.ofdma;
			dst_peer_stats->tx_dbg.amsdu_cnt =
				src_peer_stats->tx_dbg.amsdu_cnt;
			dst_peer_stats->tx_dbg.non_amsdu_cnt =
				src_peer_stats->tx_dbg.non_amsdu_cnt;
			dst_peer_stats->tx_dbg.inval_link_id_pkt_cnt =
				src_peer_stats->tx_dbg.inval_link_id_pkt_cnt;
			if (is_vdev_peer) {
				dst_peer_stats->tx_dbg.mcast.packets =
					src_peer_stats->tx_dbg.mcast.packets;
				dst_peer_stats->tx_dbg.mcast.bytes =
					src_peer_stats->tx_dbg.mcast.bytes;

				dst_peer_stats->tx_dbg.bcast.packets =
					src_peer_stats->tx_dbg.bcast.packets;
				dst_peer_stats->tx_dbg.bcast.bytes =
					src_peer_stats->tx_dbg.bcast.bytes;
			}
		}
	}

	/*rx peer stats*/
	for(i = 0; i < DP_REO_DST_RING_MAX; i++) {
		dst_peer_stats->rx[i].recv_from_reo.packets =
			src_peer_stats->rx[i].recv_from_reo.packets;
		dst_peer_stats->rx[i].recv_from_reo.bytes =
			src_peer_stats->rx[i].recv_from_reo.bytes;

		dst_peer_stats->rx[i].sent_to_stack_fast.packets =
			src_peer_stats->rx[i].sent_to_stack_fast.packets;
		dst_peer_stats->rx[i].sent_to_stack_fast.bytes =
			src_peer_stats->rx[i].sent_to_stack_fast.bytes;

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_proto_stats_enabled(dp_pdev))
				ath12k_dp_update_proto_peer_rx_stats(dst_peer_stats,
								     src_peer_stats, i);
		}
		dst_peer_stats->rx[i].sent_to_stack.packets =
			src_peer_stats->rx[i].sent_to_stack.packets;
		dst_peer_stats->rx[i].sent_to_stack.bytes =
			src_peer_stats->rx[i].sent_to_stack.bytes;
	}

	/* HW peer stats report these counters OOB without debug stats.
	 * Non-HW targets keep the existing debug-stats gated path below.
	 */
	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
		dst_peer_stats->rx_dbg.ucast.packets =
			src_peer_stats->rx_dbg.ucast.packets;
		dst_peer_stats->rx_dbg.ucast.bytes =
			src_peer_stats->rx_dbg.ucast.bytes;
	}

	if (ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
			dst_peer_stats->rx_dbg.mcast.packets =
				src_peer_stats->rx_dbg.mcast.packets;
			dst_peer_stats->rx_dbg.mcast.bytes =
				src_peer_stats->rx_dbg.mcast.bytes;

			if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev)) {
				dst_peer_stats->rx_dbg.ucast.packets =
					src_peer_stats->rx_dbg.ucast.packets;
				dst_peer_stats->rx_dbg.ucast.bytes =
					src_peer_stats->rx_dbg.ucast.bytes;
			}

			dst_peer_stats->rx_dbg.non_amsdu =
				src_peer_stats->rx_dbg.non_amsdu;
			dst_peer_stats->rx_dbg.msdu_part_of_amsdu =
				src_peer_stats->rx_dbg.msdu_part_of_amsdu;
			dst_peer_stats->rx_dbg.mpdu_retry =
				src_peer_stats->rx_dbg.mpdu_retry;
			dst_peer_stats->rx_dbg.sg.packets =
				src_peer_stats->rx_dbg.sg.packets;
			dst_peer_stats->rx_dbg.sg.bytes =
				src_peer_stats->rx_dbg.sg.bytes;
		}
	}

	/*rx error stats*/
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; i++)
		dst_peer_stats->wbm_err.rxdma_error[i] =
			src_peer_stats->wbm_err.rxdma_error[i];
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX; i++)
		dst_peer_stats->wbm_err.reo_error[i] =
			src_peer_stats->wbm_err.reo_error[i];
}

static void
ath12k_dp_aggregate_link_rx_ppdu_stats(struct ath12k_rx_ppdu_stats *dst,
				       const struct ath12k_rx_ppdu_stats *src)
{
	int i, j, k;

	if (!dst || !src)
		return;

	dst->num_mpdu_fcs_ok += src->num_mpdu_fcs_ok;
	dst->num_mpdu_fcs_err += src->num_mpdu_fcs_err;
	dst->ampdu_msdu_count += src->ampdu_msdu_count;
	dst->non_ampdu_msdu_count += src->non_ampdu_msdu_count;

	for (i = 0; i < HAL_RX_RECEPTION_TYPE_MAX; i++)
		dst->reception_type[i] += src->reception_type[i];

	dst->num_mpdus += src->num_mpdus;
	dst->num_ppdus += src->num_ppdus;
	dst->num_bar += src->num_bar;
	dst->num_ndpa += src->num_ndpa;
	dst->num_mpdu_retry_count += src->num_mpdu_retry_count;
	dst->num_msdu_retry_count += src->num_msdu_retry_count;

	for (i = 0; i < HAL_RX_RECEPTION_TYPE_MAX; i++)
		dst->ppdu_reception[i] += src->ppdu_reception[i];

	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		dst->ppdu_nss[i] += src->ppdu_nss[i];

	for (i = 0; i < MAX_PUNCTURED_MODE; i++)
		dst->punc_bw[i] += src->punc_bw[i];

	for (i = 0; i < HAL_RX_GI_MAX; i++)
		dst->gi_count[i] += src->gi_count[i];
	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		dst->nss_count[i] += src->nss_count[i];
	for (i = 0; i < HAL_RX_BW_MAX; i++)
		dst->bw_count[i] += src->bw_count[i];

	for (i = 0; i < WME_NUM_AC; i++) {
		dst->wme_ac_type_pkts[i] += src->wme_ac_type_pkts[i];
		dst->wme_ac_type_bytes[i] += src->wme_ac_type_bytes[i];
	}

	/* su_ppdu_count is sized ATH12K_RX_PPDU_PROTO_MAX (11AX/11BE/11BN) */
	for (i = 0; i < ATH12K_RX_PPDU_PROTO_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++)
			dst->su_ppdu_count[i].mcs_count[j] +=
				src->su_ppdu_count[i].mcs_count[j];
	}

	for (i = 0; i < DOT11_MAX; i++) {
		for (j = 0; j < MAX_MCS; j++)
			dst->proto_type[i].mcs_count[j] +=
				src->proto_type[i].mcs_count[j];
	}

	/* rx_mu first dimension is ATH12K_RX_PPDU_PROTO_MAX (11AX/11BE/11BN) */
	for (k = 0; k < ATH12K_RX_PPDU_PROTO_MAX; k++) {
		for (j = 0; j < TXRX_TYPE_MU_MAX; j++) {
			for (i = 0; i < HAL_RX_MAX_NSS; i++)
				dst->rx_mu[k][j].ppdu_nss[i] +=
					src->rx_mu[k][j].ppdu_nss[i];

			dst->rx_mu[k][j].mpdu_cnt_fcs_ok +=
				src->rx_mu[k][j].mpdu_cnt_fcs_ok;
			dst->rx_mu[k][j].mpdu_cnt_fcs_err +=
				src->rx_mu[k][j].mpdu_cnt_fcs_err;

			for (i = 0; i < MAX_MCS; i++)
				dst->rx_mu[k][j].ppdu.mcs_count[i] +=
					src->rx_mu[k][j].ppdu.mcs_count[i];
		}
	}
}

static void
ath12k_dp_aggregate_hw_link_tx_stats(struct ath12k_dp_link_peer_hw_tx_stats *dst,
				     const struct ath12k_dp_link_peer_hw_tx_stats *src)
{
	if (!dst || !src)
		return;

	/* Placeholder for HW link Tx stats aggregation */

	/**
	 * sum_ack_rssi, sum_phy_rate and acked_ppdu_count are link-specific,
	 * not aggregated.
	 */
}

/**
 * ath12k_dp_aggr_hw_link_tx_stats() - Aggregate HW offload TX link stats
 *				       across all links of an MLD peer.
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer
 * @link_peer_stats: destination telemetry link stats buffer
 *
 * Iterates over all data links of the MLD peer and accumulates the link level
 * HW Tx counters into the telmetry structure.
 * sum_ack_rssi and sum_phy_rate are link-specific and are NOT aggregated.
 */
static void
ath12k_dp_aggr_hw_link_tx_stats(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_dp_peer *peer,
				struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct ath12k_dp_link_peer_hw_tx_stats *dst, *src;
	u8 tmp_link_id;

	if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev))
		return;

	if (!link_peer_stats->hw_link_stats)
		return;

	dst = &link_peer_stats->hw_link_stats->hw_link_tx;

	rcu_read_lock();
	for (tmp_link_id = 0; tmp_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; tmp_link_id++) {
		tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, tmp_link_id);
		if (!tmp_peer || !tmp_peer->peer_stats.hw_link_stats)
			continue;

		src = &tmp_peer->peer_stats.hw_link_stats->hw_link_tx;

		if (dst && src)
			ath12k_dp_aggregate_hw_link_tx_stats(dst, src);
	}
	rcu_read_unlock();
}

static void
ath12k_dp_aggregate_hw_link_rx_stats(struct ath12k_dp_link_peer_hw_rx_stats *dst,
				     const struct ath12k_dp_link_peer_hw_rx_stats *src)
{
	if (!dst || !src)
		return;

	dst->success_gcast_bytes += src->success_gcast_bytes;
	dst->failed_mpdu_bytes   += src->failed_mpdu_bytes;
	dst->drop1_ucast_pkts    += src->drop1_ucast_pkts;
	dst->success_gcast_pkts  += src->success_gcast_pkts;
	dst->failed_mpdu         += src->failed_mpdu;
	/**
	 * success_ppdu_count, sum_rssi and sum_phy_rate are link-specific,
	 * not aggregated
	 */
}

/**
 * ath12k_dp_aggr_hw_link_rx_stats() - Aggregate HW RX link stats
 *				       across all links of an MLD peer.
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer
 * @link_peer_stats: destination telemetry link stats buffer
 *
 * Iterates over all data links of the MLD peer and accumulates the
 * link-level HW RX counters into telemetry structure.
 */
static void
ath12k_dp_aggr_hw_link_rx_stats(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_dp_peer *peer,
				struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct ath12k_dp_link_peer_hw_rx_stats *dst, *src;
	u8 tmp_link_id;

	if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev))
		return;

	if (!link_peer_stats->hw_link_stats)
		return;

	dst = &link_peer_stats->hw_link_stats->hw_link_rx;

	rcu_read_lock();
	for (tmp_link_id = 0; tmp_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; tmp_link_id++) {
		tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, tmp_link_id);
		if (!tmp_peer || !tmp_peer->peer_stats.hw_link_stats)
			continue;

		src = &tmp_peer->peer_stats.hw_link_stats->hw_link_rx;

		if (dst && src)
			ath12k_dp_aggregate_hw_link_rx_stats(dst, src);
	}
	rcu_read_unlock();
}

/**
 * ath12k_dp_aggr_hw_link_stats() - Aggregate HW TX and RX link stats
 *				    across all links of an MLD peer.
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer
 * @link_peer_stats: destination telemetry link stats buffer
 *
 * Iterates over all data links of the MLD peer and accumulates the
 * link-level HW TX and RX counters into telemetry structure.
 */
static void
ath12k_dp_aggr_hw_link_stats(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_dp_peer *peer,
			     struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	ath12k_dp_aggr_hw_link_tx_stats(dp_pdev, peer, link_peer_stats);
	ath12k_dp_aggr_hw_link_rx_stats(dp_pdev, peer, link_peer_stats);
}

/**
 * ath12k_dp_update_hw_peer_tx_stats() - Copy peer HW offload TX stats
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer (source)
 * @mld_stats: destination telemetry MLD stats buffer
 *
 * Copies the MLD-level HW TX drop counters into the telemetry structure.
 */
static void
ath12k_dp_update_hw_peer_tx_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_peer *peer,
				  struct ath12k_dp_mld_peer_stats *mld_stats)
{
	if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev))
		return;

	if (!mld_stats->hw_stats || !peer->mld_stats.hw_stats)
		return;

	memcpy(&mld_stats->hw_stats->hw_tx,
	       &peer->mld_stats.hw_stats->hw_tx,
	       sizeof(mld_stats->hw_stats->hw_tx));
}

/**
 * ath12k_dp_update_hw_peer_rx_stats() - Copy peer HW offload RX stats
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer (source)
 * @mld_stats: destination telemetry MLD stats buffer
 *
 * Copies the MLD-level HW RX drop counters into the telemetry query structure.
 */
static void
ath12k_dp_update_hw_peer_rx_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_peer *peer,
				  struct ath12k_dp_mld_peer_stats *mld_stats)
{
	if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev))
		return;

	if (!mld_stats->hw_stats || !peer->mld_stats.hw_stats)
		return;

	memcpy(&mld_stats->hw_stats->hw_rx,
	       &peer->mld_stats.hw_stats->hw_rx,
	       sizeof(mld_stats->hw_stats->hw_rx));
}

/**
 * ath12k_dp_update_hw_peer_stats() - Copy peer HW offload TX and RX stats
 * @dp_pdev: DP radio pointer
 * @peer: MLD dp_peer (source)
 * @mld_stats: destination telemetry MLD stats buffer
 *
 * Copies the MLD-level HW TX and RX counters into the telemetry query structure.
 */

static void
ath12k_dp_update_hw_peer_stats(struct ath12k_pdev_dp *dp_pdev,
			       struct ath12k_dp_peer *peer,
			       struct ath12k_dp_mld_peer_stats *mld_stats)
{
	ath12k_dp_update_hw_peer_tx_stats(dp_pdev, peer, mld_stats);
	ath12k_dp_update_hw_peer_rx_stats(dp_pdev, peer, mld_stats);
}

static void ath12k_dp_aggr_peer_stats(struct ath12k_link_vif *arvif,
				      struct ath12k_dp_link_peer *link_peer,
				      struct ath12k_dp_aggr_vif_stats *aggr_vif_stats)
{
	struct ath12k_dp_peer *peer;
	struct ath12k_pdev_dp *dp_pdev = &arvif->ar->dp;
	struct ath12k *ar = arvif->ar;
	struct ath12k_dp_link_peer_stats *link_peer_stats;
	int stats_link_id;
	struct ath12k_htt_tx_ppdu_stats *src_tx_ppdu_stats = NULL;
	struct ath12k_htt_tx_ppdu_stats *dst_tx_ppdu_stats = NULL;
	struct ath12k_rx_ppdu_stats *rx_peer_stats = NULL;
	struct ath12k_rx_ppdu_stats *dst = NULL;
	struct ath12k_dp_link_peer_hw_tx_stats *src_hw_link_tx = NULL;
	struct ath12k_dp_link_peer_hw_rx_stats *src_hw_link_rx = NULL;
	struct ath12k_dp_link_peer_hw_tx_stats *dst_hw_link_tx = NULL;
	struct ath12k_dp_link_peer_hw_rx_stats *dst_hw_link_rx = NULL;

	if (!link_peer->dp_peer)
		return;

	peer = link_peer->dp_peer;
	stats_link_id = ar->hw_link_id;
	link_peer_stats = &aggr_vif_stats->link_peer_stats;

	/* RX packets/bytes derived from PPDU, independent of extended RX
	 * stats knob (DP_ENABLE_EXT_RX_STATS)
	 */
	aggr_vif_stats->rx_pkt_ppdu_stats.rx_packets += link_peer->rx_packets;
	aggr_vif_stats->rx_pkt_ppdu_stats.rx_bytes += link_peer->rx_bytes;

	if (stats_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS) {
		ath12k_dp_aggr_per_pkt_peer_stats(dp_pdev, &aggr_vif_stats->peer_stats,
						  &peer->stats[stats_link_id],
						  peer->is_vdev_peer);
		dst_tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
		src_tx_ppdu_stats = link_peer->peer_stats.tx_ppdu_stats;
		rx_peer_stats =  link_peer->peer_stats.rx_ppdu_stats;
		if (ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
			ath12k_dp_update_tx_ppdu_stats_aggr(dp_pdev,
							    dst_tx_ppdu_stats,
							    src_tx_ppdu_stats);
		if (ath12k_dp_rx_ppdu_stats_enabled(dp_pdev) &&
		    link_peer_stats->rx_ppdu_stats &&
		    rx_peer_stats) {
			dst = link_peer_stats->rx_ppdu_stats;
			ath12k_dp_aggregate_link_rx_ppdu_stats(dst, rx_peer_stats);
		}

		if (ath12k_dp_hw_peer_stats_enabled(dp_pdev) &&
		    link_peer->peer_stats.hw_link_stats &&
		    link_peer_stats->hw_link_stats) {
			src_hw_link_tx = &link_peer->peer_stats.hw_link_stats->hw_link_tx;
			src_hw_link_rx = &link_peer->peer_stats.hw_link_stats->hw_link_rx;

			dst_hw_link_tx = &link_peer_stats->hw_link_stats->hw_link_tx;
			dst_hw_link_rx = &link_peer_stats->hw_link_stats->hw_link_rx;

			ath12k_dp_aggregate_hw_link_tx_stats(dst_hw_link_tx,
							     src_hw_link_tx);
			ath12k_dp_aggregate_hw_link_rx_stats(dst_hw_link_rx,
							     src_hw_link_rx);
		}
	}
}

/**
 * ath12k_dp_aggr_link_vif_del_stats - aggregate deleted link peer stats into VIF
 * @arvif: link VIF to aggregate stats for
 * @aggr_vif_stats: destination VIF stats structure
 *
 * Aggregates preserved statistics from deleted link peers into the current
 * VIF stats aggregation. When link peers disconnect, their stats are preserved
 * in the link VIF's link_peer_delete_stats. This function retrieves and merges
 * those preserved stats into the VIF's aggregated statistics.
 */
static void
ath12k_dp_aggr_link_vif_del_stats(struct ath12k_link_vif *arvif,
				  struct ath12k_dp_aggr_vif_stats *aggr_vif_stats)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];

	/* Aggregate preserved stats from deleted link peers of this VIF */
	ath12k_dp_aggr_del_stats(&aggr_vif_stats->peer_stats,
				 &aggr_vif_stats->rx_pkt_ppdu_stats,
				 &dp_link_vif->link_peer_delete_stats,
				 "link_peer_delete_stats");
}

/**
 * ath12k_dp_vif_peer_stats_update - DP-only: aggregate stats for one peer
 * @dp_hw:        group-level DP hardware context (DP object)
 * @dp_pdev:      per-radio DP context (DP object)
 * @dp_peer_addr: MAC address used to key dp_peer (MLD MAC or link MAC)
 * @hw_link_id:   hardware link ID of the radio
 * @arvif:        link VIF (bridge: needed by ath12k_dp_aggr_peer_stats)
 * @aggr_vif_stats: destination stats buffer
 *
 * Called with ar->arsta_lock held (BH-disabled).
 * Acquires dp_hw->peer_hash_lock internally (arsta_lock -> peer_hash_lock ordering).
 */
void
ath12k_dp_vif_peer_stats_update(struct ath12k_dp_hw *dp_hw,
				struct ath12k_pdev_dp *dp_pdev,
				const u8 *dp_peer_addr,
				u8 hw_link_id,
				struct ath12k_link_vif *arvif,
				struct ath12k_dp_aggr_vif_stats *aggr_vif_stats)
{
	struct ath12k_dp_peer      *dp_peer;
	struct ath12k_dp_link_peer *dp_link_peer;

	/* DP: find MLD/legacy peer by MAC address */
	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, dp_peer_addr, arvif->ahvif->vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	/* DP: find the link peer for this radio's hw_link_id (RCU-protected) */
	rcu_read_lock();
	dp_link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);
	if (!dp_link_peer) {
		rcu_read_unlock();
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	/* DP: aggregate per-link peer stats */
	ath12k_dp_aggr_peer_stats(arvif, dp_link_peer, aggr_vif_stats);

	/* DP: aggregate MLD-level HW stats exactly once via the primary link peer */
	if (dp_link_peer->primary_link) {
		ath12k_dp_update_hw_peer_stats(dp_pdev, dp_peer,
					       &aggr_vif_stats->mld_stats);
	}

	rcu_read_unlock();
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

static void
ath12k_dp_aggr_vif_ppeds_sync_stats(struct ath12k_dp_aggr_vif_stats *aggr_vif_stats,
				    struct ath12k_dp_vif *vif)
{
	if (!vif->rx_stats[DP_REO_PPEDS_RING_IDX].ppeds_rx.packets)
		return;

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY, "RX PPEDS stats aggregation on VIF");
	aggr_vif_stats->peer_stats.rx[DP_REO_PPEDS_RING_IDX].sent_to_stack.packets +=
			vif->rx_stats[DP_REO_PPEDS_RING_IDX].ppeds_rx.packets;
	aggr_vif_stats->peer_stats.rx[DP_REO_PPEDS_RING_IDX].sent_to_stack.bytes +=
			vif->rx_stats[DP_REO_PPEDS_RING_IDX].ppeds_rx.bytes;
}

static void ath12k_dp_aggr_vif_ingress_stats(struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_dp_aggr_vif_stats *aggr_vif_stats,
                                             struct ath12k_dp_vif *vif)
{
	int i, j;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++) {
		aggr_vif_stats->stats[i].tx_i.recv_from_stack.packets +=
			vif->stats[i].tx_i.recv_from_stack.packets;
		aggr_vif_stats->stats[i].tx_i.recv_from_stack.bytes +=
			vif->stats[i].tx_i.recv_from_stack.bytes;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw.packets +=
			vif->stats[i].tx_i.enque_to_hw.packets;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw.bytes +=
			vif->stats[i].tx_i.enque_to_hw.bytes;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw_fast.packets +=
			vif->stats[i].tx_i.enque_to_hw_fast.packets;
		aggr_vif_stats->stats[i].tx_i.enque_to_hw_fast.bytes +=
			vif->stats[i].tx_i.enque_to_hw_fast.bytes;
		aggr_vif_stats->stats[i].tx_i.sg_pkt.packets +=
			vif->stats[i].tx_i.sg_pkt.packets;
		aggr_vif_stats->stats[i].tx_i.sg_pkt.bytes +=
			vif->stats[i].tx_i.sg_pkt.bytes;

		aggr_vif_stats->stats[i].tx_i.sg_dma_map_err +=
			vif->stats[i].tx_i.sg_dma_map_err;

		for (j = 0; j < DP_TX_ENQ_ERR_MAX; j++)
			aggr_vif_stats->stats[i].tx_i.drop[j] +=
				vif->stats[i].tx_i.drop[j];

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
				aggr_vif_stats->stats[i].tx_i.mcast.packets +=
					vif->stats[i].tx_i.mcast.packets;
				aggr_vif_stats->stats[i].tx_i.mcast.bytes +=
					vif->stats[i].tx_i.mcast.bytes;

				for (j = 0; j < HAL_TCL_ENCAP_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.encap_type[j] +=
						vif->stats[i].tx_i.encap_type[j];

				for (j = 0; j < HAL_ENCRYPT_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.encrypt_type[j] +=
						vif->stats[i].tx_i.encrypt_type[j];
				for (j = 0; j < DP_TCL_DESC_TYPE_MAX; j++)
					aggr_vif_stats->stats[i].tx_i.desc_type[j] +=
						vif->stats[i].tx_i.desc_type[j];
			}
			if (ath12k_proto_stats_enabled(dp_pdev)) {
				if (!vif->stats[i].proto)
					return;

				memcpy(aggr_vif_stats->stats[i].proto,
				       vif->stats[i].proto,
				       sizeof(struct ath12k_dp_proto_stats_vif));
			}
		}
	}
}

void ath12k_dp_get_pdev_stats(struct ath12k_pdev_dp *pdev,
			      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	struct ath12k *ar = pdev->ar;
	struct ath12k_dp_aggr_vif_stats *aggr_vif_stats;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_aggr_pdev_stats *aggr_pdev_stats =
					&telemetry_radio->aggr_pdev_stats;

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    ath12k_dp_debug_stats_enabled(&ar->dp))
		telemetry_radio->is_extended = true;

	aggr_vif_stats = vzalloc(sizeof(*aggr_vif_stats));
	if (aggr_vif_stats) {
		aggr_vif_stats->link_peer_stats.tx_ppdu_stats =
					aggr_pdev_stats->link_peer_stats.tx_ppdu_stats;
		aggr_vif_stats->link_peer_stats.rx_ppdu_stats =
					aggr_pdev_stats->link_peer_stats.rx_ppdu_stats;
		aggr_vif_stats->link_peer_stats.rx_stats =
					aggr_pdev_stats->link_peer_stats.rx_stats;
		list_for_each_entry(arvif, &ar->arvifs, list) {
			ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
			/* Include deleted link peer stats stored at link VIF */
			ath12k_dp_aggr_link_vif_del_stats(arvif, aggr_vif_stats);
		}
		memcpy(&aggr_pdev_stats->peer_stats, &aggr_vif_stats->peer_stats,
		       sizeof(aggr_pdev_stats->peer_stats));
		aggr_pdev_stats->rx_pkt_ppdu_stats = aggr_vif_stats->rx_pkt_ppdu_stats;

		vfree(aggr_vif_stats);
	}
}

void ath12k_dp_get_vif_stats(struct ath12k_vif *ahvif,
			     struct ath12k_telemetry_dp_vif *telemetry_vif,
			     u8 link_id)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k *ar = &ahvif->ah->radio[0];
	unsigned long links_map = ahvif->links_map;
	struct ath12k_dp_aggr_vif_stats *aggr_vif_stats =
						&telemetry_vif->aggr_vif_stats;

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    ath12k_dp_debug_stats_enabled(&ar->dp))
		telemetry_vif->is_extended = true;

	if (ath12k_scan_radio_supported(ar->pdev)) {
		ath12k_dp_mon_rx_scan_radio_stats_update(ar, telemetry_vif);
		return;
	}

	/*Vif stats for requested link*/
	if (link_id < ATH12K_NUM_MAX_LINKS && (links_map & BIT(link_id))) {
		rcu_read_lock();
		arvif = rcu_dereference(ahvif->link[link_id]);

		if (arvif && arvif->is_started) {
			ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
			/* Include deleted link peer stats for specific link VIF */
			ath12k_dp_aggr_link_vif_del_stats(arvif, aggr_vif_stats);
		}
		rcu_read_unlock();
	} else {
		/*MLD vif stats*/
		ath12k_dp_aggr_vif_ingress_stats(&ar->dp, aggr_vif_stats, dp_vif);
		ath12k_dp_aggr_vif_ppeds_sync_stats(aggr_vif_stats, dp_vif);
		/*legacy vif stats handling*/
		if (hweight16(links_map) == 0) {
			arvif =  &ahvif->deflink;

			if (arvif && arvif->is_started) {
				ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
				/* Include deleted link peer stats for legacy VIF */
				ath12k_dp_aggr_link_vif_del_stats(arvif, aggr_vif_stats);
			}
		} else {
			/*Aggregate vif stats of all link in MLD vif*/
			for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
				rcu_read_lock();
				arvif = rcu_dereference(ahvif->link[link_id]);
				if (!arvif || !arvif->is_created) {
					rcu_read_unlock();
					continue;
				}
				ath12k_vif_iterate_peer(arvif, aggr_vif_stats);
				/* Include deleted link peer stats of each link VIF
				 * in MLD VIF
				 */
				ath12k_dp_aggr_link_vif_del_stats(arvif, aggr_vif_stats);
				rcu_read_unlock();
			}
			/* Aggregate stats from deleted link VIFs into MLD VIF */
			ath12k_dp_aggr_del_stats(&aggr_vif_stats->peer_stats,
						 &aggr_vif_stats->rx_pkt_ppdu_stats,
						 &dp_vif->link_vif_delete_stats,
						 "link_vif_delete_stats");
		}
	}
}

/**
 * ath12k_update_peer_rx_signal_stats() - Get RX signal stats from link peer
 * @link_peer: pointer to link peer structure
 * @signal_stats: output buffer for signal statistics (caller-allocated)
 *
 * Retrieves RX signal statistics from the specified link peer.
 * Caller must allocate signal_stats buffer.
 *
 *
 * Return: 0 on success, negative error code on failure
 *         -EINVAL if parameters are NULL or if RX stats not available in link peer
 */
int
ath12k_update_peer_rx_signal_stats(struct ath12k_dp_link_peer *link_peer,
				   struct ath12k_dp_link_peer_rx_signal_stats *sgnl_stats)
{
	const struct ath12k_dp_link_peer_rx_signal_stats *stats;

	if (!link_peer || !sgnl_stats)
		return -EINVAL;

	stats = &link_peer->signal_stats;

	/* copying values to dst structure */
	sgnl_stats->snr = stats->snr;
	sgnl_stats->snr_dp = stats->snr_dp;
	sgnl_stats->snr_avg = stats->snr_avg;
	sgnl_stats->snr_dp_avg = stats->snr_dp_avg;
	sgnl_stats->rssi = stats->rssi;
	sgnl_stats->rssi_dp = stats->rssi_dp;
	sgnl_stats->rssi_avg = stats->rssi_avg;
	sgnl_stats->rssi_dp_avg = stats->rssi_dp_avg;

	return 0;
}

/**
 * ath12k_update_peer_rx_mon_stats() - Get RX monitor stats from link peer
 * @link_peer: pointer to link peer structure
 * @rx_mon_stats: output buffer for RX statistics (caller-allocated)
 *
 * Retrieves RX monitor statistics from the specified link peer.
 * Caller must allocate rx_stats buffer.
 *
 *
 * Return: 0 on success, negative error code on failure
 *         -EINVAL if parameters are NULL or if RX stats not available in link peer
 */
int ath12k_update_peer_rx_mon_stats(struct ath12k_dp_link_peer *link_peer,
				    struct ath12k_rx_ppdu_stats *rx_mon_stats)
{
	const struct ath12k_rx_ppdu_stats *src;

	if (!link_peer || !rx_mon_stats)
		return -EINVAL;

	src = link_peer->peer_stats.rx_ppdu_stats;
	if (!src) {
		ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
			   "RX stats not available for link peer %pM\n",
			   link_peer->addr);
		return -EINVAL;
	}

	memcpy(rx_mon_stats, src, sizeof(*rx_mon_stats));

	ath12k_update_peer_rx_signal_stats(link_peer, &rx_mon_stats->signal_stats);
	return 0;
}

/**
 * ath12k_update_peer_hw_link_tx_stats() - Get HW offload TX link stats from a
 *					   link peer.
 * @dst: destination hw_link_stats buffer
 * @src: source hw_link_stats from the link peer
 *
 * Performs a memcpy of the hw_link_tx sub-structure.
 */
static inline void
ath12k_update_peer_hw_link_tx_stats(struct ath12k_dp_link_peer_hw_stats *dst,
				    const struct ath12k_dp_link_peer_hw_stats *src)
{
	if (!dst || !src)
		return;

	memcpy(&dst->hw_link_tx, &src->hw_link_tx, sizeof(dst->hw_link_tx));
}

/**
 * ath12k_update_peer_hw_rx_link_stats() - Get HW offload RX link stats from a
 *					   link peer.
 * @dst: destination hw_link_stats buffer
 * @src: source hw_link_stats from the link peer
 *
 * Performs a memcpy of the hw_link_rx sub-structure.
 */
static inline void
ath12k_update_peer_hw_link_rx_stats(struct ath12k_dp_link_peer_hw_stats *dst,
				    const struct ath12k_dp_link_peer_hw_stats *src)
{
	if (!dst || !src)
		return;

	memcpy(&dst->hw_link_rx, &src->hw_link_rx, sizeof(dst->hw_link_rx));
}

/**
 * ath12k_dp_update_hw_link_stats() - Copy HW link TX and RX stats from a link
 *				      peer into the telemetry buffer.
 * @dp_pdev: DP radio pointer
 * @dp_peer: MLD dp_peer
 * @link_id: link ID to look up the link peer
 * @link_peer_stats: destination telemetry link stats buffer
 *
 * Looks up the link peer for the given link_id and copies both hw_link_tx
 * and hw_link_rx sub-structures if both buffers are allocated.
 */
static void
ath12k_dp_update_hw_link_stats(struct ath12k_pdev_dp *dp_pdev,
			       struct ath12k_dp_peer *dp_peer,
			       u8 link_id,
			       struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer;

	if (!ath12k_dp_hw_peer_stats_enabled(dp_pdev))
		return;

	rcu_read_lock();
	tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_id);
	if (!tmp_peer || !tmp_peer->peer_stats.hw_link_stats)
		goto unlock;

	if (link_peer_stats->hw_link_stats) {
		ath12k_update_peer_hw_link_tx_stats(link_peer_stats->hw_link_stats,
						    tmp_peer->peer_stats.hw_link_stats);
		ath12k_update_peer_hw_link_rx_stats(link_peer_stats->hw_link_stats,
						    tmp_peer->peer_stats.hw_link_stats);
	}

unlock:
	rcu_read_unlock();
}

/*
 * ath12k_dp_get_link_peer_stats - Collect link peer statistics (DP objects only)
 * @dp_pdev: per-pdev DP structure
 * @dp_peer: DP peer structure
 * @hw_link_id: hardware link ID
 * @telemetry_peer: output telemetry peer stats structure
 *
 * Core stats collection logic operating only on DP objects.
 * Called by ath12k_get_link_peer_stats() after non-DP lookups.
 * Called with RCU lock held.
 */
int
ath12k_dp_get_link_peer_stats(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_dp_peer *dp_peer,
			      int hw_link_id,
			      struct ath12k_telemetry_dp_peer *telemetry_peer)
{
	struct ath12k_dp_peer_stats *peer_stats = &telemetry_peer->peer_stats;
	struct ath12k_dp_link_peer_stats *link_peer_stats =
					&telemetry_peer->link_peer_stats;
	struct ath12k_dp_link_peer *link_peer;

	struct ath12k_htt_tx_ppdu_stats *dsc_tx_ppdu_stats = NULL;
	struct ath12k_htt_tx_ppdu_stats *src_tx_ppdu_stats = NULL;
	struct ath12k_rx_ppdu_stats *dst_stats = link_peer_stats->rx_ppdu_stats;
	int ret = 0;

	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);
	if (!link_peer) {
		ath12k_err(NULL, "Error link peer not found");
		return -EINVAL;
	}

	/* RX packets/bytes derived from PPDU, independent of extended RX
	 * stats knob (DP_ENABLE_EXT_RX_STATS)
	 */
	telemetry_peer->rx_pkt_ppdu_stats.rx_packets += link_peer->rx_packets;
	telemetry_peer->rx_pkt_ppdu_stats.rx_bytes += link_peer->rx_bytes;

	if (hw_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS) {
		dsc_tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
		src_tx_ppdu_stats = link_peer->peer_stats.tx_ppdu_stats;
		/* Per packet Counters */
		ath12k_dp_update_per_pkt_peer_stats(dp_pdev, peer_stats,
						    &dp_peer->stats[hw_link_id],
						    dp_peer->is_vdev_peer);
		/* HW link stats */
		ath12k_dp_update_hw_link_stats(dp_pdev, dp_peer, hw_link_id,
					       link_peer_stats);

		/* Tx PPDU HTT stats */
		if (ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
			ath12k_dp_update_tx_ppdu_stats(dsc_tx_ppdu_stats,
						       src_tx_ppdu_stats);

		/* Rx Mon stats */
		if (ath12k_dp_rx_ppdu_stats_enabled(dp_pdev)) {
			ret = ath12k_update_peer_rx_mon_stats(link_peer,
							      dst_stats);
		}
	}
	return ret;
}

void ath12k_dp_update_rx_msdu_counters(struct ath12k_dp_peer *peer,
				       u8 link_id,
				       struct ath12k_dp_rx_pkt_ppdu_stats *rx_pkt_stats)
{
	struct ath12k_dp_link_peer *tmp_peer;

	rcu_read_lock();
	tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, link_id);
	if (!tmp_peer) {
		rcu_read_unlock();
		return;
	}

	rx_pkt_stats->rx_packets += tmp_peer->rx_packets;
	rx_pkt_stats->rx_bytes += tmp_peer->rx_bytes;
	rcu_read_unlock();
}

void ath12k_update_ext_stats(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_dp_peer *peer,
			     u8 link_id,
			     struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	int ret = 0;
	if (!ath12k_htt_tx_ppdu_stats_enabled(dp_pdev) &&
	    !ath12k_dp_rx_ppdu_stats_enabled(dp_pdev))
		return;

	rcu_read_lock();
	tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, link_id);
	if (!tmp_peer)
		goto unlock;

	/* TX PPDU stats */
	if (ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
		ath12k_dp_update_tx_ppdu_stats(link_peer_stats->tx_ppdu_stats,
					       tmp_peer->peer_stats.tx_ppdu_stats);

	if (ath12k_dp_rx_ppdu_stats_enabled(dp_pdev)) {
		ret = ath12k_update_peer_rx_mon_stats(tmp_peer,
						      link_peer_stats->rx_ppdu_stats);
		if (ret)
			ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
				   "Link peer RX stats fetch failed for link peer %pM.\n",
				   tmp_peer->addr);
	}
unlock:
	rcu_read_unlock();
}

static void ath12k_dp_aggr_htt_stats(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_peer *peer,
				     struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct ath12k_htt_tx_ppdu_stats *src_tx_ppdu_stats = NULL;
	struct ath12k_htt_tx_ppdu_stats *dst_tx_ppdu_stats = NULL;
	u8 tmp_link_id;

	if (!ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
		return;

	rcu_read_lock();
	for (tmp_link_id = 0; tmp_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; tmp_link_id++) {
		tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, tmp_link_id);
		if (!tmp_peer)
			continue;
		dst_tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
		src_tx_ppdu_stats = tmp_peer->peer_stats.tx_ppdu_stats;
		ath12k_dp_update_tx_ppdu_stats_aggr(dp_pdev,
						    dst_tx_ppdu_stats,
						    src_tx_ppdu_stats);
	}
	rcu_read_unlock();
}

void ath12k_dp_aggr_rx_mon_stats(struct ath12k_pdev_dp *dp_pdev,
				 struct ath12k_dp_peer *peer,
				 struct ath12k_dp_link_peer_stats *link_peer_stats)
{
	struct ath12k_dp_link_peer *tmp_peer = NULL;
	struct ath12k_rx_ppdu_stats *dst = NULL;
	struct ath12k_rx_ppdu_stats *rx_peer_stats = NULL;
	u8 tmp_link_id;

	if (!ath12k_dp_rx_ppdu_stats_enabled(dp_pdev))
		return;

	rcu_read_lock();
	for (tmp_link_id = 0; tmp_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; tmp_link_id++) {
		tmp_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, tmp_link_id);
		if (!tmp_peer)
			continue;

		dst = link_peer_stats->rx_ppdu_stats;
		rx_peer_stats = tmp_peer->peer_stats.rx_ppdu_stats;
		if (dst && rx_peer_stats)
			ath12k_dp_aggregate_link_rx_ppdu_stats(dst,
							       rx_peer_stats);
	}
	rcu_read_unlock();
}

/**
 * ath12k_dp_aggr_rx_pkt_ppdu_stats() - Aggregate RX packet/byte counters
 *				      derived from PPDU across all MLO
 *				      links of an MLD peer.
 * @peer: MLD dp_peer whose link peers are summed
 * @rx_pkt_stats: destination RX packet/byte counters derived from PPDU
 *
 * Independent of extended RX stats knob (DP_ENABLE_EXT_RX_STATS);
 * iterates every live link peer of the MLD peer and accumulates their raw
 * ath12k_dp_link_peer rx_packets/rx_bytes counters into rx_pkt_stats.
 */
static void
ath12k_dp_aggr_rx_pkt_ppdu_stats(struct ath12k_dp_peer *peer,
				 struct ath12k_dp_rx_pkt_ppdu_stats *rx_pkt_stats)
{
	u8 tmp_link_id;

	for (tmp_link_id = 0; tmp_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; tmp_link_id++)
		ath12k_dp_update_rx_msdu_counters(peer, tmp_link_id, rx_pkt_stats);
}

/**
 * ath12k_dp_accumulate_tx_delay_stats() - Aggregate TX delay stats from one ring
 * @src_tx_delay: Source TX delay statistics from a specific ring
 * @dst_tx_delay: Destination TX delay statistics (aggregated across rings)
 *
 * Aggregates TX delay statistics including software queue delay and hardware
 * transmission delay histograms.
 */
static void
ath12k_dp_accumulate_tx_delay_stats(struct ath12k_dp_peer_delay_tx_stats *src_tx_delay,
				    struct ath12k_dp_peer_delay_tx_stats *dst_tx_delay)
{
	if (!src_tx_delay || !dst_tx_delay)
		return;

	/* Aggregate software queue delay histogram */
	ath12k_dp_accumulate_hist_stats(&src_tx_delay->tx_swq_delay,
					&dst_tx_delay->tx_swq_delay);

	/* Aggregate hardware transmission delay histogram */
	ath12k_dp_accumulate_hist_stats(&src_tx_delay->hwtx_delay,
					&dst_tx_delay->hwtx_delay);
}

/**
 * ath12k_dp_accumulate_rx_delay_stats() - Aggregate RX delay stats from one ring
 * @src_rx_delay: Source RX delay statistics from a specific ring
 * @dst_rx_delay: Destination RX delay statistics (aggregated across rings)
 *
 * Aggregates RX delay statistics including the delay from hardware to stack.
 */
static void
ath12k_dp_accumulate_rx_delay_stats(struct ath12k_dp_peer_delay_rx_stats *src_rx_delay,
				    struct ath12k_dp_peer_delay_rx_stats *dst_rx_delay)
{
	if (!src_rx_delay || !dst_rx_delay)
		return;

	/* Aggregate to-stack delay histogram */
	ath12k_dp_accumulate_hist_stats(&src_rx_delay->to_stack_delay,
					&dst_rx_delay->to_stack_delay);
}

/**
 * ath12k_dp_accumulate_stats_per_tid() - Store delay stats per TID per ring
 * @per_ring: Source statistics organized per-TID per-ring
 * @all_rings: Destination statistics organized per-TID for all rings combined
 *
 * This function aggregates delay statistics from all rings into a single per-TID view.
 * It iterates through all TIDs and all rings, accumulating the statistics.
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_dp_accumulate_stats_per_tid(struct ath12k_dp_peer_delay_stats *per_ring,
				   struct ath12k_dp_peer_tid_agg_delay_stats *all_rings)
{
	int tid, ring;
	struct ath12k_dp_peer_delay_tid_stats *src_tid_stats;
	struct ath12k_dp_peer_delay_tid_stats *dst_tid_stats;

	if (!per_ring || !all_rings) {
		ath12k_err(NULL, "Invalid parameters: per_ring = %p, all_rings = %p\n",
			   per_ring, all_rings);
		return -EINVAL;
	}

	/* Initialize destination statistics to zero */
	memset(all_rings, 0, sizeof(*all_rings));

	/* Iterate through all TIDs */
	for (tid = 0; tid < DP_TID_MAX; tid++) {
		dst_tid_stats = &all_rings->tid_stats[tid];

		/* Initialize histogram types for destination */
		ath12k_dp_hist_init(&dst_tid_stats->tx_delay.tx_swq_delay,
				    HIST_TYPE_SW_ENQEUE_DELAY);
		ath12k_dp_hist_init(&dst_tid_stats->tx_delay.hwtx_delay,
				    HIST_TYPE_HW_TX_COMP_DELAY);
		ath12k_dp_hist_init(&dst_tid_stats->rx_delay.to_stack_delay,
				    HIST_TYPE_REAP_STACK);

		/* Aggregate statistics from all rings for this TID */
		for (ring = 0; ring < DP_REO_DST_RING_MAX; ring++) {
			src_tid_stats = &per_ring->delay_tid_stats[tid][ring];

			/* Aggregate TX delay statistics */
			ath12k_dp_accumulate_tx_delay_stats(&src_tid_stats->tx_delay,
							    &dst_tid_stats->tx_delay);

			/* Aggregate RX delay statistics */
			ath12k_dp_accumulate_rx_delay_stats(&src_tid_stats->rx_delay,
							    &dst_tid_stats->rx_delay);
		}
	}

	return 0;
}

int
ath12k_dp_accumulate_sojourn_stats(struct ath12k_dp_peer_sojourn_stats *per_ring,
				   struct ath12k_dp_peer_tid_agg_sojourn_stats *all_rings)
{
	int tid, ring_id;
	struct ath12k_dp_peer_tid_sojourn_stats *src_tid_stats;
	struct ath12k_dp_peer_tid_sojourn_stats *dst_tid_stats;
	struct ewma_avg_sojourn *avg_sojourn_msdu;

	if (!per_ring || !all_rings)
		return -EINVAL;

	memset(all_rings, 0, sizeof(*all_rings));

	for (tid = 0; tid < DP_TID_MAX; tid++) {
		dst_tid_stats = &all_rings->tid_stats[tid];
		for (ring_id = 0; ring_id < DP_REO_DST_RING_MAX; ring_id++) {
			src_tid_stats = &per_ring->tid_stats[tid][ring_id];
			avg_sojourn_msdu = &src_tid_stats->avg_sojourn_msdu;
			dst_tid_stats->sum_sojourn_msdu +=
				src_tid_stats->sum_sojourn_msdu;
			dst_tid_stats->num_msdus += src_tid_stats->num_msdus;
			ewma_avg_sojourn_add(&dst_tid_stats->avg_sojourn_msdu,
					     ewma_avg_sojourn_read(avg_sojourn_msdu));
		}
	}

	return 0;
}

int
ath12k_dp_accumulate_jitter_stats(struct ath12k_dp_peer_jitter_stats *per_ring,
				  struct ath12k_dp_peer_tid_agg_jitter_stats *all_rings)
{
	int tid, ring_id;
	struct ath12k_dp_peer_tid_jitter_stats *src_tid_stats;
	struct ath12k_dp_peer_tid_jitter_stats *dst_tid_stats;

	if (!per_ring || !all_rings)
		return -EINVAL;

	memset(all_rings, 0, sizeof(*all_rings));

	for (tid = 0; tid < DP_TID_MAX; tid++) {
		dst_tid_stats = &all_rings->tid_stats[tid];
		for (ring_id = 0; ring_id < DP_REO_DST_RING_MAX; ring_id++) {
			src_tid_stats = &per_ring->tid_stats[tid][ring_id];

			if (!src_tid_stats->tx_avg_jitter)
				goto skip_jitter;
			if (!dst_tid_stats->tx_avg_jitter)
				dst_tid_stats->tx_avg_jitter =
					src_tid_stats->tx_avg_jitter;
			else
				dst_tid_stats->tx_avg_jitter =
					(src_tid_stats->tx_avg_jitter +
					 dst_tid_stats->tx_avg_jitter) >> 1;
skip_jitter:
			if (!src_tid_stats->tx_avg_delay)
				goto skip_delay;
			if (!dst_tid_stats->tx_avg_delay)
				dst_tid_stats->tx_avg_delay =
					src_tid_stats->tx_avg_delay;
			else
				dst_tid_stats->tx_avg_delay =
					(src_tid_stats->tx_avg_delay +
					 dst_tid_stats->tx_avg_delay) >> 1;
skip_delay:
			dst_tid_stats->tx_avg_err += src_tid_stats->tx_avg_err;

			dst_tid_stats->tx_total_success +=
				src_tid_stats->tx_total_success;
			dst_tid_stats->tx_drop += src_tid_stats->tx_drop;
		}
	}

	return 0;
}

void ath12k_dp_get_sojourn_stats(struct ath12k_dp_peer *peer,
				 struct ath12k_dp_mld_peer_stats *stats)
{
	struct ath12k_dp_mld_peer_stats *mld_stats;
	struct ath12k_dp_peer_sojourn_stats *sojourn;
	struct ath12k_dp_peer_tid_agg_sojourn_stats *tid_sojourn;

	if (!peer || !stats)
		return;

	mld_stats = &peer->mld_stats;

	sojourn = mld_stats->sojourn_stats;
	tid_sojourn = &stats->agg_mld_stats->sojourn;

	ath12k_dp_accumulate_sojourn_stats(sojourn, tid_sojourn);
}

void ath12k_dp_get_jitter_stats(struct ath12k_dp_peer *peer,
				struct ath12k_dp_mld_peer_stats *stats)
{
	struct ath12k_dp_mld_peer_stats *mld_stats;
	struct ath12k_dp_peer_jitter_stats *jitter;
	struct ath12k_dp_peer_tid_agg_jitter_stats *tid_jitter;

	if (!peer || !stats)
		return;

	mld_stats = &peer->mld_stats;

	jitter = mld_stats->jitter_stats;
	tid_jitter = &stats->agg_mld_stats->jitter;

	ath12k_dp_accumulate_jitter_stats(jitter, tid_jitter);
}

void ath12k_dp_get_delay_stats(struct ath12k_dp_peer *peer,
			       struct ath12k_dp_mld_peer_stats *stats)
{
	struct ath12k_dp_mld_peer_stats *mld_stats;
	struct ath12k_dp_peer_delay_stats *per_ring_stats;
	struct ath12k_dp_peer_tid_agg_delay_stats *all_rings_stats;

	if (!peer || !stats)
		return;

	mld_stats = &peer->mld_stats;
	per_ring_stats = mld_stats->delay_stats;
	all_rings_stats = &stats->agg_mld_stats->delay;

	ath12k_dp_accumulate_stats_per_tid(per_ring_stats, all_rings_stats);
}

static void
ath12k_dp_update_legacy_peer_stats(struct ath12k_pdev_dp *dp_pdev,
				   struct ath12k_dp_peer *peer,
				   struct ath12k_telemetry_dp_peer *telemetry_peer,
				   struct ath12k_dp_peer_stats *peer_stats,
				   struct ath12k_dp_link_peer_stats *link_stats)
{
	u8 link_id;

	telemetry_peer->peer_type = ATH12K_LEGACY_PEER;

	ath12k_dp_update_hw_peer_stats(dp_pdev, peer, &telemetry_peer->mld_stats);

	for (link_id = 0; link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; link_id++) {
		ath12k_dp_aggr_per_pkt_peer_stats(dp_pdev,
						  peer_stats,
						  &peer->stats[link_id],
						  peer->is_vdev_peer);
		ath12k_dp_update_hw_link_stats(dp_pdev, peer, link_id,
					       link_stats);
		ath12k_dp_update_rx_msdu_counters(peer, link_id,
						  &telemetry_peer->rx_pkt_ppdu_stats);
		ath12k_update_ext_stats(dp_pdev, peer, link_id, link_stats);
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_dp_latency_stats_enabled(dp_pdev)) {
		ath12k_dp_get_delay_stats(peer, &telemetry_peer->mld_stats);
		ath12k_dp_get_jitter_stats(peer, &telemetry_peer->mld_stats);
		ath12k_dp_get_sojourn_stats(peer, &telemetry_peer->mld_stats);
	}
}

/*
 * ath12k_dp_get_peer_stats - Collect MLD/legacy peer statistics (DP objects only)
 * @dp_pdev: per-pdev DP structure (derived from ar->dp in wrapper)
 * @dp_peer: DP peer structure
 * @telemetry_peer: output telemetry peer stats structure
 * @link_id: link ID
 * @valid_link: whether the link ID is valid
 * @links_map: bitmap of active links
 * @stats_link_id: hw_link_id for the specific link (derived from arvif->ar->hw_link_id
 *                 in the wrapper); meaningful only when valid_link is true
 *
 * Core stats collection logic for the MLD/legacy peer path, operating only
 * on DP objects. Called by ath12k_get_peer_stats() after non-DP lookups.
 */
int
ath12k_dp_get_peer_stats(struct ath12k_pdev_dp *dp_pdev,
			 struct ath12k_dp_peer *dp_peer,
			 struct ath12k_telemetry_dp_peer *telemetry_peer,
			 u8 link_id,
			 bool valid_link,
			 unsigned long links_map,
			 int stats_link_id)
{
	struct ath12k_dp_peer_stats *peer_stats = &telemetry_peer->peer_stats;
	struct ath12k_dp_link_peer_stats *link_stats = &telemetry_peer->link_peer_stats;
	struct ath12k_dp_mld_peer_stats *mld_stats = &telemetry_peer->mld_stats;
	struct ath12k_dp_rx_pkt_ppdu_stats *rx_pkt_stats;
	int i, ret = 0;

	rx_pkt_stats = &telemetry_peer->rx_pkt_ppdu_stats;

	/* Error case handling for legacy peer */
	if (!dp_peer->is_mlo && valid_link) {
		ath12k_err(NULL, "Error legacy peer with valid link id");
		return -EINVAL;
	}

	/* Error case handling for non-associated links */
	if (valid_link && !(dp_peer->peer_links_map & BIT(stats_link_id))) {
		ath12k_err(NULL, "Error MLO peer with invalid link id");
		return -EINVAL;
	}

	/* Peer stats for requested link id */
	if (valid_link) {
		telemetry_peer->peer_type = ATH12K_LINK_PEER;
		if (stats_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS) {
			ath12k_dp_update_per_pkt_peer_stats(dp_pdev,
							    peer_stats,
							    &dp_peer->stats[stats_link_id],
							    dp_peer->is_vdev_peer);
			ath12k_dp_update_rx_msdu_counters(dp_peer, stats_link_id,
							  rx_pkt_stats);
			ath12k_update_ext_stats(dp_pdev, dp_peer, stats_link_id,
						link_stats);
			ath12k_dp_update_hw_link_stats(dp_pdev,
						       dp_peer,
						       stats_link_id,
						       link_stats);
		}
	} else {
		/*
		 * Non-MLO peer handling:
		 * For non-MLO (legacy/WDS) peers, find the link the peer is
		 * associated on by scanning peer_links_map. A non-MLO peer
		 * has exactly one bit set in peer_links_map; that bit index
		 * is the link_id, which is also the index into peer->stats[].
		 *
		 * Note: On a multi-link AP the deflink may be on a different
		 * radio than the peer. Using ahvif->deflink would read the
		 * wrong peer->stats[] slot and return zero counters.
		 */
		if (!dp_peer->is_mlo) {
			ath12k_dp_update_legacy_peer_stats(dp_pdev, dp_peer,
							   telemetry_peer,
							   peer_stats,
							   link_stats);
		} else {
			telemetry_peer->peer_type = ATH12K_MLD_PEER;
			/* Aggregated peer stats of all links in MLD peer */
			for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
				ath12k_dp_aggr_per_pkt_peer_stats(dp_pdev,
								  peer_stats,
								  &dp_peer->stats[i],
								  dp_peer->is_vdev_peer);
			}
			/* RX packets/bytes derived from PPDU, independent of
			 * extended RX stats knob
			 * (DP_ENABLE_EXT_RX_STATS)
			 */
			ath12k_dp_aggr_rx_pkt_ppdu_stats(dp_peer, rx_pkt_stats);
			/* Include preserved stats of deleted link peers
			 * when reporting MLD peer stats
			 */
			ath12k_dp_aggr_del_stats(peer_stats,
						 rx_pkt_stats,
						 &dp_peer->link_peer_delete_stats,
						 "link_peer_delete_stats");
			ath12k_dp_aggr_htt_stats(dp_pdev, dp_peer, link_stats);
			ath12k_dp_aggr_rx_mon_stats(dp_pdev, dp_peer, link_stats);
			ath12k_dp_update_hw_peer_stats(dp_pdev, dp_peer, mld_stats);
			ath12k_dp_aggr_hw_link_stats(dp_pdev, dp_peer, link_stats);

			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_dp_latency_stats_enabled(dp_pdev)) {
				ath12k_dp_get_delay_stats(dp_peer, mld_stats);
				ath12k_dp_get_jitter_stats(dp_peer, mld_stats);
				ath12k_dp_get_sojourn_stats(dp_peer, mld_stats);
			}
		}
	}
	return ret;
}

void ath12k_dp_get_device_stats(struct ath12k_dp *dp,
				struct ath12k_telemetry_dp_device *telemetry_device)
{
	memcpy(&telemetry_device->rxdma_error,
	       &dp->device_stats.wbm_err.rxdma_error,
	       sizeof(telemetry_device->rxdma_error));

	memcpy(&telemetry_device->reo_error,
	       &dp->device_stats.wbm_err.reo_error,
	       sizeof(telemetry_device->reo_error));

	memcpy(&telemetry_device->tx_comp_err,
	       &dp->device_stats.tx_err.tx_comp_err,
	       sizeof(telemetry_device->tx_comp_err));

	memcpy(&telemetry_device->rx_wbm_sw_drop_reason,
	       &dp->device_stats.wbm_err.drop,
	       sizeof(telemetry_device->rx_wbm_sw_drop_reason));

	memcpy(&telemetry_device->reo_sw_drop_reason,
	       &dp->device_stats.rx.rx_err,
	       sizeof(telemetry_device->reo_sw_drop_reason));

	memcpy(&telemetry_device->ppeds_stats,
	       &dp->ppe.ppeds_stats,
	       sizeof(telemetry_device->ppeds_stats));
}

void ath12k_dp_clear_link_desc_pool(struct ath12k_dp *dp)
{
	struct dp_link_desc_bank *link_desc_banks = dp->link_desc_banks;
	int i;

	for (i = 0; i < DP_LINK_DESC_BANKS_MAX; i++) {
		if (link_desc_banks[i].vaddr_unaligned) {
			memset(link_desc_banks[i].vaddr_unaligned, 0x0,
			       link_desc_banks[i].size);
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_clear_link_desc_pool);

int ath12k_dp_alloc_proto_stats_vif(struct ath12k_dp_vif *dp_vif)
{
	u32 size;
	u8 index;
	struct ath12k_dp_proto_stats_vif *proto_vif;

	if (dp_vif->stats[0].proto)
		return 0;

	size = sizeof(struct ath12k_dp_proto_stats_vif) * DP_TCL_NUM_RING_MAX;
	proto_vif = kzalloc(size, GFP_ATOMIC);
	if (!proto_vif)
		return -ENOMEM;

	for (index = 0; index < DP_TCL_NUM_RING_MAX; index++)
		dp_vif->stats[index].proto = (proto_vif + index);

	return 0;
}

void ath12k_dp_free_proto_stats_vif(struct ath12k_dp_tx_vif_stats *vif_stats)
{
	kfree(vif_stats->proto);
	vif_stats->proto = NULL;
}

int ath12k_dp_alloc_proto_stats_peer(struct ath12k_dp_peer *dp_peer)
{
	u8 index;

	for (index = 0; index < ATH12K_DP_PEER_MAX_MLO_LINKS; index++) {
		struct ath12k_dp_peer_stats *stats = &dp_peer->stats[index];

		if (stats->proto)
			continue;

		stats->proto = kzalloc(sizeof(struct ath12k_dp_proto_stats_peer),
				       GFP_ATOMIC);

		if (!stats->proto)
			return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_alloc_proto_stats_peer);

void ath12k_dp_free_proto_stats_peer(struct ath12k_dp_peer *dp_peer)
{
	u8 index;

	for (index = 0; index < ATH12K_DP_PEER_MAX_MLO_LINKS; index++) {
		struct ath12k_dp_peer_stats *peer_stats = &dp_peer->stats[index];

		kfree(peer_stats->proto);
		peer_stats->proto = NULL;
	}
}
EXPORT_SYMBOL(ath12k_dp_free_proto_stats_peer);

static inline u8
ath12k_dp_get_eapol_keytype(struct sk_buff *skb)
{
	enum ath12k_dp_eapol_key_type eapol_subtype;

	if (!skb || !skb->data || skb->len < ETH_HLEN)
		return 0;

	eapol_subtype = ath12k_dp_get_eapol_subtype(skb->data + ETH_HLEN);

	switch (eapol_subtype) {
	case DP_EAPOL_KEY_TYPE_M1:
		return DP_PKT_TYPE_EAPOL_M1;
	case DP_EAPOL_KEY_TYPE_M2:
		return DP_PKT_TYPE_EAPOL_M2;
	case DP_EAPOL_KEY_TYPE_M3:
		return DP_PKT_TYPE_EAPOL_M3;
	case DP_EAPOL_KEY_TYPE_M4:
		return DP_PKT_TYPE_EAPOL_M4;
	case DP_EAPOL_KEY_TYPE_G1:
		return DP_PKT_TYPE_EAPOL_G1;
	case DP_EAPOL_KEY_TYPE_G2:
		return DP_PKT_TYPE_EAPOL_G2;
	default:
		return 0;
	}
}

void
ath12k_dp_tx_peer_update_proto_stats(struct ath12k_dp_peer *dp_peer,
				     u8 link_id,
				     struct sk_buff *skb,
				     u8 level,
				     int ring_id)
{
	u8 field = 0;

	if (unlikely(!dp_peer->stats[link_id].proto))
		return;

	if (unlikely(skb_is_nonlinear(skb)))
		return;

	field = ath12k_dp_get_l3_protocol_type(skb);
	DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
				ring_id, level, l3[field], 1);

	if (field == DP_PKT_TYPE_IPV4) {
		field = ath12k_dp_get_l4_protocol_type(skb);
			DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
						ring_id, level, l4[field], 1);

		if (field == DP_PKT_TYPE_ICMP) {
			field = ath12k_dp_get_l4_protocol_subtype(skb);
				DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
							ring_id, level, l4[field], 1);
		}
		if (field == DP_PKT_TYPE_UDP) {
			field = ath12k_dp_get_l5_protocol_type(skb);
				DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
							ring_id, level, l5[field], 1);

			if (field == DP_PKT_TYPE_DHCP) {
				field = ath12k_dp_get_l5_protocol_subtype(skb);
					DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
								ring_id, level,
								l5[field], 1);
			}
		}
	}

	if (field == DP_PKT_TYPE_EAPOL) {
		field = ath12k_dp_get_eapol_keytype(skb);
		DP_PEER_PROTO_STATS_INC(dp_peer, link_id, tx,
					ring_id, level, l3[field], 1);
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_peer_update_proto_stats);

void ath12k_dp_update_proto_stats_vif(struct ath12k_dp_vif *dp_vif,
				      u8 link_id,
				      struct sk_buff *skb,
				      u8 level,
				      int ring_id)
{
	u8 field = 0;

	if (unlikely(skb_is_nonlinear(skb)))
		return;

	if (unlikely(!dp_vif->stats[ring_id].proto))
		return;

	field = ath12k_dp_get_l3_protocol_type(skb);

	DP_STATS_INC(dp_vif, proto->tx[level].l3[field], 1, ring_id);

	if (field == DP_PKT_TYPE_IPV4) {
		field = ath12k_dp_get_l4_protocol_type(skb);
		DP_STATS_INC(dp_vif, proto->tx[level].l4[field], 1, ring_id);

		if (field == DP_PKT_TYPE_ICMP) {
			field = ath12k_dp_get_l4_protocol_subtype(skb);
			DP_STATS_INC(dp_vif,
				     proto->tx[level].l4[field], 1, ring_id);
		}

		if (field == DP_PKT_TYPE_UDP) {
			field = ath12k_dp_get_l5_protocol_type(skb);
			DP_STATS_INC(dp_vif, proto->tx[level].l5[field],
				     1, ring_id);

			if (field == DP_PKT_TYPE_DHCP) {
				field = ath12k_dp_get_l5_protocol_subtype(skb);
				DP_STATS_INC(dp_vif, proto->tx[level].l5[field],
					     1, ring_id);
			}
		}
	}

	if (field == DP_PKT_TYPE_EAPOL) {
		field = ath12k_dp_get_eapol_keytype(skb);
		DP_STATS_INC(dp_vif, proto->tx[level].l3[field], 1, ring_id);
	}
}
EXPORT_SYMBOL(ath12k_dp_update_proto_stats_vif);

void ath12k_dp_rx_update_protocol_stats(struct ath12k_dp_peer *dp_peer,
					u8 link_id, struct sk_buff *skb, u8 level,
					int ring_id)
{
	u8 field = 0;

	if (!dp_peer || unlikely(!dp_peer->stats[link_id].proto))
		return;

	field = ath12k_dp_get_l3_protocol_type(skb);
	DP_PEER_PROTO_STATS_INC(dp_peer, link_id, rx,
				ring_id, level, l3[field], 1);

	if (field == DP_PKT_TYPE_IPV4) {
		field = ath12k_dp_get_l4_protocol_type(skb);
			DP_PEER_PROTO_STATS_INC(dp_peer, link_id, rx,
						ring_id, level, l4[field], 1);

		if (field == DP_PKT_TYPE_ICMP) {
			field = ath12k_dp_get_l4_protocol_subtype(skb);
				DP_PEER_PROTO_STATS_INC(dp_peer, link_id, rx,
							ring_id, level, l4[field], 1);
		}
		if (field == DP_PKT_TYPE_UDP) {
			field = ath12k_dp_get_l5_protocol_type(skb);
				DP_PEER_PROTO_STATS_INC(dp_peer, link_id, rx,
							ring_id, level, l5[field], 1);

			if (field == DP_PKT_TYPE_DHCP) {
				field = ath12k_dp_get_l5_protocol_subtype(skb);
					DP_PEER_PROTO_STATS_INC(dp_peer, link_id, rx,
								ring_id, level,
								l5[field], 1);
			}
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_rx_update_protocol_stats);
