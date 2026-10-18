// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"
#include "../dp.h"
#include "dp.h"
#include "../telemetry_agent_if.h"
#include "../dp_stats.h"

static u16 ath12k_wifi7_peer_ml_id_alloc(struct ath12k_hw *ah)
{
	u16 ml_peer_id;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (ah->num_ml_peers >= ah->max_ml_peers_supported) {
		ath12k_err(NULL, "Failed to create ML peer limit %d[%d]\n",
			   ah->max_ml_peers_supported, ah->num_ml_peers);
		return ATH12K_MLO_PEER_ID_INVALID;
	}

	ml_peer_id = ah->last_ml_peer_id;
	for (i = 0; i <= ah->max_ml_peer_ids; i++) {
		ml_peer_id = (ml_peer_id + 1) % ah->max_ml_peer_ids;

		if (!ml_peer_id)
			continue;

		if (test_bit(ml_peer_id, ah->free_ml_peer_id_map))
			continue;

		set_bit(ml_peer_id, ah->free_ml_peer_id_map);
		break;
	}

	ah->last_ml_peer_id = ml_peer_id;
	if (i == ah->max_ml_peer_ids)
		ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "Allocated ml_peer_id:%d", ml_peer_id);

	return ml_peer_id;
}

static void ath12k_wifi7_peer_ml_id_free(struct ath12k_hw *ah,
					 struct ath12k_sta *ahsta)
{
	if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID)
		return;

	clear_bit(ahsta->ml_peer_id, ah->free_ml_peer_id_map);
	ahsta->ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;
	ah->num_ml_peers--;
}

int ath12k_wifi7_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	u8 i = 0, tid;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_sta *sta = NULL;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct wireless_dev *wdev;
	struct ath12k_pdev_dp *dp_pdev;
	int ret;
	struct ath12k_dp_rx_tid *rx_tid;

	if (params->sta) {
		sta = params->sta;
		ahsta = ath12k_sta_to_ahsta(sta);
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_check(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		ath12k_hw_warn(ah, "wifi7: dp peer already exists %pM vdev_peer %d\n",
			       addr, dp_peer->is_vdev_peer);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (sta && sta->mlo) {
		ahsta->ml_peer_id = ath12k_wifi7_peer_ml_id_alloc(ah);

		if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID) {
			ath12k_hw_warn(ah, "unable to allocate ML peer id for sta %pM",
				       sta->addr);
			return -ENOMEM;
		}
		params->peer_id = ahsta->ml_peer_id | ATH12K_PEER_ML_ID_VALID;
		ah->num_ml_peers++;
	}

	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer) {
		if (sta && sta->mlo)
			ath12k_wifi7_peer_ml_id_free(ah, ahsta);
		return -ENOMEM;
	}

	for (tid = 0; tid < ATH12K_MAX_TIDS; tid++) {
		rx_tid = &dp_peer->rx_tid[tid];
		spin_lock_init(&rx_tid->tid_lock);
	}

	dp_peer->qos = kzalloc(sizeof(*dp_peer->qos), GFP_KERNEL);
	if (!dp_peer->qos) {
		if (sta && sta->mlo)
			ath12k_wifi7_peer_ml_id_free(ah, ahsta);
		kfree(dp_peer);
		return -ENOMEM;
	}

	spin_lock_init(&dp_peer->qos->lock);
	spin_lock_init(&dp_peer->keys_lock);
	INIT_LIST_HEAD(&dp_peer->tqm_cleanup_node);
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->vif = vif;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = params->is_mlo ? params->peer_id : ATH12K_DP_PEER_ID_INVALID;
	dp_peer->is_epp_peer = params->is_epp_peer;
	dp_peer->sta_id = ATH12K_DP_PEER_ID_INVALID;
	dp_peer->hw_link_id = ATH12K_INVALID_HW_LINKID;
	dp_peer->is_vdev_peer = params->is_vdev_peer;
	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer)
		dp_peer->hw_link_id = params->hw_link_id;
	else
		dp_peer->assoc_hw_link_id = params->hw_link_id;
	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->tx_encap_type = ahvif->dp_vif.tx_encap_type;
	dp_peer->rx_decap_type = ahvif->dp_vif.rx_decap_type;

	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++)
		dp_peer->l2h_link_map[i] = ATH12K_DP_HW_LINK_ID_INVALID;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev)
		dp_peer->dev = wdev->netdev;

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(ah->ag->dp_hw_grp, params->hw_link_id);
	ret = ath12k_dp_peer_stats_alloc(dp_peer, dp_pdev);
	if (ret) {
		rcu_read_unlock();
		if (sta && sta->mlo)
			ath12k_wifi7_peer_ml_id_free(ah, ahsta);
		kfree(dp_peer->qos);
		kfree(dp_peer);
		return ret;
	}

	rcu_read_unlock();

	spin_lock_bh(&dp_hw->peer_hash_lock);

	/* Add ath12k_dp_peer to the linked list taking peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_add(&dp_peer->list, &dp_hw->peers);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	ath12k_dp_peer_hash_table_add(dp_hw, dp_peer);

	if (dp_peer->is_mlo && dp_peer->peer_id < MAX_DP_PEER_LIST_SIZE)
		rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	if (ahsta)
		rcu_assign_pointer(ahsta->dp_peer, dp_peer);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	params->peer_id = ATH12K_DP_PEER_ID_INVALID;
	params->sta_id = ATH12K_DP_PEER_ID_INVALID;

	return 0;
}

void ath12k_wifi7_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id,
				 struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	u16 peerid_index;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	bool pre_rcu_remove_done = false;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		/*
		 * ahsta->dp_peer was already cleared in
		 * sta_pre_rcu_remove() before mac80211's synchronize_net().
		 * This rcu_assign_pointer() is a no-op in that case but is
		 * kept as a safety net for paths where sta_pre_rcu_remove()
		 * was not called.
		 */
		rcu_assign_pointer(ahsta->dp_peer, NULL);
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	} else {
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	}

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L0,
				 "Failed to find peer:%pM", addr);
		return;
	}

	if (dp_peer->is_mlo) {
		peerid_index = dp_peer->peer_id;
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
		ahsta = ath12k_sta_to_ahsta(ath12k_dp_peer_get_sta(dp_peer));
		ath12k_wifi7_peer_ml_id_free(ah, ahsta);
	}

	ath12k_dp_peer_hash_table_delete(dp_hw, dp_peer);

	/* Remove ath12k_dp_peer from linked list holding peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_del(&dp_peer->list);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	/*
	 * pre_rcu_remove mutate under wiphy->mtx
	 * since this function is under wiphy lock reading this flag here is safe
	 */
	pre_rcu_remove_done = dp_peer->pre_rcu_remove_done;

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (!pre_rcu_remove_done) {
		/*
		 * sta_pre_rcu_remove() was not called (unlikely).
		 * Wait for all RCU readers that may hold a reference to dp_peer
		 * (via dp_peer_list[] or ahsta->dp_peer) before freeing it.
		 */

		synchronize_net();
	}

	if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
		ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

	kfree(dp_peer->qos);
	ath12k_dp_peer_stats_free(dp_peer);
	kfree(dp_peer);
}

int ath12k_wifi7_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr,
			       struct ath12k_dp_peer_create_params *params)
{
	return 0;
}
