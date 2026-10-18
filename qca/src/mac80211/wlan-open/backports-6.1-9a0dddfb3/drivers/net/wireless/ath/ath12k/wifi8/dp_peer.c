// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../core.h"
#include "../debug.h"
#include "../debugfs.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"
#include "dp.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"
#include "dp_msdu_queue.h"
#include "dp_tx_flow_info.h"
#include "../telemetry_agent_if.h"
#include "dp_tx.h"
#include "../dp_tx.h"
#include "dp_telemetry.h"
#include "hal_queue.h"
#include "../mgmt_rx.h"

#define ATH12K_DP_MAX_SEQ_NUM  0xFFF
#define ATH12K_DP_MAX_POSSIBLE_BA_WIN  0x400
#define ATH12K_DP_INVALID_MLSN_OFFSET  0xFFFF

void ath12k_dp_tqm_update_completion(struct ath12k_dp *dp, void *ctx,
				     struct hal_tqm_status *tqm_status)
{
	struct ath12k_dp_tx_queue *data = ctx;
	struct ath12k_base *ab = dp->ab;

	/* NULL tqm_status = forced cleanup from tqm_cmd_list_cleanup; no-op. */
	if (!tqm_status)
		return;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "dp tqm update: peer_id=%u hw_link_id=%u status=%d\n",
			 data ? data->peer_id : 0, data ? data->hw_link_id : 0,
			 tqm_status->status_hdr.cmd_execution_status);

	if (tqm_status->status_hdr.cmd_execution_status !=
	    HAL_TQM_SUCCESSFUL_EXECUTION)
		ath12k_warn(ab, "dp tqm update command failed with status %d",
			    tqm_status->status_hdr.cmd_execution_status);
	else
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "dp tqm update completion successful\n");
}

static u16 ath12k_wifi8_peer_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 peer_id;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	peer_id = dp_hw->last_peer_id;
	for (i = 0; i < ATH12K_MAX_PEER_ID; i++) {
		peer_id = (peer_id + 1) % ATH12K_MAX_PEER_ID;

		if (!peer_id)
			continue;

		if (test_bit(peer_id, dp_hw->free_peer_id_map))
			continue;

		set_bit(peer_id, dp_hw->free_peer_id_map);
		break;
	}

	dp_hw->last_peer_id = peer_id;
	if (i >= ATH12K_MAX_PEER_ID)
		peer_id = ATH12K_MLO_PEER_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1, "Allocated peer_id:%d",
			 peer_id);

	return peer_id;
}

static u16 ath12k_wifi8_sta_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 sta_id;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	sta_id = dp_hw->last_sta_id;
	for (i = 0; i < ATH12K_MAX_STA_ID; i++) {
		sta_id = (sta_id + 1) % ATH12K_MAX_STA_ID;

		if (test_bit(sta_id, dp_hw->free_sta_id_map))
			continue;

		set_bit(sta_id, dp_hw->free_sta_id_map);
		break;
	}

	dp_hw->last_sta_id = sta_id;
	if (i >= ATH12K_MAX_STA_ID)
		sta_id = ATH12K_STA_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1, "Allocated sta_id:%d",
			 sta_id);

	return sta_id;
}

static u16
ath12k_wifi8_ucast_stats_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				  u16 dp_peer_id, u8 tid, u8 hw_link_id,
				  unsigned int num_peer)
{
	u16 stats_id;
	u16 max_stats_id = ATH12K_DEFAULT_UCAST_STATS_ID;
	int i;

	if (num_peer >= ATH12K_DEFAULT_UCAST_STATS_ID)
		max_stats_id = ATH12K_MAX_EXT_UCAST_STATS_ID;

	stats_id = dp_hw_grp_wifi8->last_ucast_stats_id;

	for (i = 0; i < max_stats_id; i++) {
		stats_id++;

		if (stats_id >= max_stats_id)
			stats_id = 0;

		if (test_bit(stats_id, dp_hw_grp_wifi8->free_stats_id))
			continue;

		set_bit(stats_id, dp_hw_grp_wifi8->free_stats_id);
		dp_hw_grp_wifi8->last_ucast_stats_id = stats_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id = dp_peer_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].tid = tid;
		dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id = hw_link_id;

		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "allocated stats_id:%d", stats_id);
		return stats_id;
	}
	return ATH12K_MAX_STATS_ID;
}

static u16
ath12k_wifi8_downlink_gcast_stats_id(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     u16 dp_peer_id, u8 tid, u8 hw_link_id)
{
	u16 stats_id;
	int i;

	/* If last_gcast_stats_id is out of range, start with minimum value */
	if (dp_hw_grp_wifi8->last_gcast_stats_id < ATH12K_MIN_DGCAST_STATS_ID ||
	    dp_hw_grp_wifi8->last_gcast_stats_id >= ATH12K_MAX_DGCAST_STATS_ID - 1)
		stats_id = ATH12K_MIN_DGCAST_STATS_ID - 1;
	else
		stats_id = dp_hw_grp_wifi8->last_gcast_stats_id;

	for (i = 0; i < ATH12K_MAX_DGCAST_STATS_ID - ATH12K_MIN_DGCAST_STATS_ID; i++) {
		stats_id++;

		if (stats_id >= ATH12K_MAX_DGCAST_STATS_ID)
			stats_id = ATH12K_MIN_DGCAST_STATS_ID;

		if (test_bit(stats_id, dp_hw_grp_wifi8->free_stats_id))
			continue;

		set_bit(stats_id, dp_hw_grp_wifi8->free_stats_id);
		dp_hw_grp_wifi8->last_gcast_stats_id = stats_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id = dp_peer_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].tid = tid;
		dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id = hw_link_id;

		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "allocated stats_id:%d", stats_id);
		return stats_id;
	}
	return ATH12K_MAX_STATS_ID;
}

static void
ath12k_wifi8_stats_id_map_update_hw_link(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
						 struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *removed_peer)
{
	struct stats_to_peer_id_map *stats_map;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id = ATH12K_INVALID_LINK_ID;
	int i;

	if (!dp_peer || dp_peer->stats_id >= ATH12K_MAX_STATS_ID)
		return;

	stats_map = &dp_hw_grp_wifi8->stats_id_map[dp_peer->stats_id];
	if (stats_map->dp_peer_id != dp_peer->peer_id ||
	    stats_map->hw_link_id != removed_peer->hw_link_id)
		return;

	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = rcu_dereference(dp_peer->link_peers[i]);
		if (!link_peer || link_peer == removed_peer)
			continue;

		hw_link_id = link_peer->hw_link_id;
		break;
	}
	rcu_read_unlock();

	stats_map->hw_link_id = hw_link_id;
}

static u16
ath12k_wifi8_link_band_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8)
{
	u16 link_band_id;
	int i;

	for (i = 0; i < ATH12K_MAX_STATS_ID; i++) {
		link_band_id = i;

		if (test_bit(link_band_id, dp_hw_grp_wifi8->free_link_band_id))
			continue;

		set_bit(link_band_id, dp_hw_grp_wifi8->free_link_band_id);
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "allocated link_band_id:%d", link_band_id);
		return link_band_id;
	}

	return ATH12K_MAX_STATS_ID;
}

/**
 * ath12k_wifi8_stats_id_free() - Invalidate a stats_id map entry and free its
 *                                bitmap slot.
 * @dp_hw_grp_wifi8: wifi8 HW group context
 * @stats_id: the stats_id to release; must be < ATH12K_MAX_STATS_ID
 *
 * Centralises the three-field map invalidation + clear_bit
 */
static void
ath12k_wifi8_stats_id_free(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			   u16 stats_id)
{
	dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id  = ATH12K_MLO_PEER_ID_INVALID;
	dp_hw_grp_wifi8->stats_id_map[stats_id].tid         = ATH12K_INVALID_TID;
	dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id  = ATH12K_INVALID_LINK_ID;
	clear_bit(stats_id, dp_hw_grp_wifi8->free_stats_id);
}

/**
 * ath12k_wifi8_link_band_id_free() - Free a link_band_id bitmap slot.
 * @dp_hw_grp_wifi8: wifi8 HW group context
 * @band_id: the band_id to release; must be < ATH12K_MAX_STATS_ID
 */
static void
ath12k_wifi8_link_band_id_free(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			       u16 band_id)
{
	clear_bit(band_id, dp_hw_grp_wifi8->free_link_band_id);
}

/**
 * ath12k_wifi8_dp_vow_stats_id_alloc() - Allocate per-TID stats IDs for VoW
 * @dp_hw_grp_wifi8: wifi8 HW group context holding the stats_id pool
 * @dp_peer: DP peer structure
 * @addr: peer MAC address
 * @hw_link_id: HW link ID, stored in stats_id_map for link tracking
 * @num_peer: Number of peers connected
 *
 * Allocates one stats_id per data TID (0 to ATH12K_DATA_TID_MAX-1) from the
 * ucast pool and stores them in dp_peer->tid_stats_id[].
 *
 * On pool exhaustion mid-loop the already-allocated entries are rolled back,
 * all tid_stats_id[] are reset to invalid marker, and a warning is emitted.
 * Peer creation continues — only VoW HW telemetry is lost for this peer.
 */
static void
ath12k_wifi8_dp_vow_stats_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				   struct ath12k_dp_peer *dp_peer,
				   const u8 *addr, u8 hw_link_id,
				   unsigned int num_peer)
{
	u8 tid, i;

	for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++) {
		dp_peer->tid_stats_id[tid] =
			ath12k_wifi8_ucast_stats_id_alloc(dp_hw_grp_wifi8,
							  dp_peer->peer_id,
							  tid,
							  hw_link_id,
							  num_peer);
		if (dp_peer->tid_stats_id[tid] < ATH12K_MAX_STATS_ID)
			continue;

		ath12k_err(NULL,
			   "stats_id exhausted for %pM tid: %u\n",
			   addr, tid);
		for (i = 0; i < tid; i++) {
			ath12k_wifi8_stats_id_free(dp_hw_grp_wifi8,
						   dp_peer->tid_stats_id[i]);
			dp_peer->tid_stats_id[i] = ATH12K_MAX_STATS_ID;
		}
		break;
	}
}

/**
 * ath12k_wifi8_dp_vow_telemetry_peer_config() - Program TASC registers for VoW per-TID
 * @umac_dp: central DP context
 * @dp_peer: DP peer structure
 * @tid_band_id: 2D array [tid][hw_link_id] of pre-built per-TID band IDs,
 *               populated by the caller from each link peer's tid_band_id[tid]
 *
 * For each data TID whose tid_stats_id is valid, programs the TASC TX and RX
 * peer telemetry registers with the TID's stats_id and its per-link band array.
 */
static void
ath12k_wifi8_dp_vow_telemetry_peer_config(struct ath12k_dp *umac_dp,
					  struct ath12k_dp_peer *dp_peer,
					  u16 tid_band_id[][HAL_TASC_BAND_MAX])
{
	u8 tid;

	for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++) {
		if (dp_peer->tid_stats_id[tid] >= ATH12K_MAX_STATS_ID)
			continue;
		ath12k_wifi8_dp_telemetry_peer_config(umac_dp,
						      dp_peer->tid_stats_id[tid],
						      tid_band_id[tid]);
	}
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	u8 i = 0, tid;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct wireless_dev *wdev;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_pdev_dp *dp_pdev;
	int ret;
	struct ath12k_dp_rx_tid *rx_tid;
	unsigned int num_peers;
	bool vow_enabled;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(ah->ag->dp_hw_grp);

	if (params->sta)
		ahsta = ath12k_sta_to_ahsta(params->sta);

	spin_lock_bh(&dp_hw->peer_hash_lock);
	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_check(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		ath12k_hw_warn(ah, "wifi8: dp peer already exists %pM vdev_peer %d\n",
			       addr, dp_peer->is_vdev_peer);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer)
		return -ENOMEM;

	dp_peer->qos = kzalloc(sizeof(*dp_peer->qos), GFP_KERNEL);
	if (!dp_peer->qos) {
		kfree(dp_peer);
		return -ENOMEM;
	}

	dp_peer->dp_hw = dp_hw;
	dp_peer->dp_hw_grp = ah->ag->dp_hw_grp;

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(ah->ag->dp_hw_grp, params->hw_link_id);
	vow_enabled = ath12k_dp_vow_stats_enabled(dp_pdev);
	ret = ath12k_dp_peer_stats_alloc(dp_peer, dp_pdev);
	if (ret) {
		rcu_read_unlock();
		kfree(dp_peer->qos);
		kfree(dp_peer);
		return ret;
	}
	rcu_read_unlock();

	for (tid = 0; tid < ATH12K_MAX_TIDS; tid++) {
		rx_tid = &dp_peer->rx_tid[tid];
		spin_lock_init(&rx_tid->tid_lock);
	}

	spin_lock_init(&dp_peer->qos->lock);
	spin_lock_init(&dp_peer->keys_lock);
	dp_peer->sta_id = ATH12K_STA_ID_INVALID;
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->vif = vif;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->is_epp_peer = params->is_epp_peer;
	dp_peer->peer_id = ath12k_wifi8_peer_id_alloc(dp_hw);
	if (dp_peer->peer_id == ATH12K_MLO_PEER_ID_INVALID) {
		ath12k_dp_peer_stats_free(dp_peer);
		kfree(dp_peer->qos);
		kfree(dp_peer);
		return -ENOMEM;
	}

	if (!params->is_vdev_peer) {
		dp_peer->sta_id = ath12k_wifi8_sta_id_alloc(dp_hw);
		if (dp_peer->sta_id == ATH12K_STA_ID_INVALID) {
			spin_lock_bh(&dp_hw->peer_hash_lock);
			clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			kfree(dp_peer->qos);
			ath12k_dp_peer_stats_free(dp_peer);
			kfree(dp_peer);
			return -ENOMEM;
		}
	}

	dp_peer->is_vdev_peer = params->is_vdev_peer;
	dp_peer->is_sta_bss_peer = params->is_sta_bss_peer;
	dp_peer->hw_link_id = ATH12K_INVALID_HW_LINKID;

	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	dp_peer->tx_encap_type = ahvif->dp_vif.tx_encap_type;
	dp_peer->rx_decap_type = ahvif->dp_vif.rx_decap_type;

	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++)
		dp_peer->l2h_link_map[i] = ATH12K_DP_HW_LINK_ID_INVALID;

	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer) {
		dp_peer->hw_link_id = params->hw_link_id;
	} else {
		ahsta->dp_peer_id = dp_peer->peer_id;
		rcu_assign_pointer(ahsta->dp_peer, dp_peer);
		dp_peer->assoc_hw_link_id = params->hw_link_id;
	}

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev) {
		dp_peer->dev = wdev->netdev;
		if (params->is_sta_bss_peer)
			dp_peer->is_sta_bss_peer_4addr = wdev->use_4addr;
	}

	/*
	 * tid_stats_id[] and stats_id entries to be initialized to
	 * ATH12K_MAX_STATS_ID (invalid marker).
	 */
	for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++)
		dp_peer->tid_stats_id[tid] = ATH12K_MAX_STATS_ID;

	dp_peer->stats_id = ATH12K_MAX_STATS_ID;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	/* Assigning telemetry specific id's for stats update */
	if (dp_peer->is_vdev_peer) {
		dp_peer->stats_id =
			ath12k_wifi8_downlink_gcast_stats_id(dp_hw_grp_wifi8,
							     dp_peer->peer_id,
							     ATH12K_INVALID_TID,
							     params->hw_link_id);
	} else {
		num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);
		if (!vow_enabled) {
			/*
			 * VoW disabled: allocate a single peer-level stats_id.
			 * This covers all TIDs in one TASC descriptor per window.
			 */
			dp_peer->stats_id =
				ath12k_wifi8_ucast_stats_id_alloc(dp_hw_grp_wifi8,
								  dp_peer->peer_id,
								  ATH12K_INVALID_TID,
								  params->hw_link_id,
								  num_peers);
		} else {
			/*
			 * VoW enabled: skip peer-level stats_id; allocate one
			 * stats_id per data TID (0-7) so HW delivers per-TID
			 * telemetry descriptors.
			 */
			ath12k_wifi8_dp_vow_stats_id_alloc(dp_hw_grp_wifi8,
							   dp_peer, addr,
							   params->hw_link_id,
							   num_peers);
		}

	}

	/* For peer delete */
	kref_init(&dp_peer->refcount);

	/* Initialize TQM cleanup list node */
	INIT_LIST_HEAD(&dp_peer->tqm_cleanup_node);

	/* Add ath12k_dp_peer to the linked list holding peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_add(&dp_peer->list, &dp_hw->peers);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	ath12k_dp_peer_hash_table_add(dp_hw, dp_peer);

	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	params->peer_id = dp_peer->peer_id;
	params->sta_id = dp_peer->sta_id;
	dp_peer->dp_peer_state = ATH12K_DP_PEER_CREATED;

	return 0;
}

/*
 * Relase all dp_peer pointer fields
 */
static void ath12k_wifi8_dp_peer_cleanup(struct ath12k_dp_hw *dp_hw,
					 struct ath12k_dp_peer *dp_peer)
{
	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "dp-peer-cleanup: peer_id=%u\n", dp_peer->peer_id);

	if (dp_peer->peer_ext_ctx) {
		struct ath12k_dp_tx_flow_info *t = &dp_peer->peer_ext_ctx->tx_flow_info;
		dma_addr_t tx_classify_info_paddr;
		void *tx_classify_info_vaddr;

		tx_classify_info_paddr = t->hw_who_classify_info_paddr;
		tx_classify_info_vaddr = t->hw_who_classify_info_vaddr;

		ath12k_dp_peer_free_queues(dp_peer->dp_hw_grp, dp_peer);
		ath12k_dp_tx_classify_info_free(dp_peer->dp_hw_grp,
						tx_classify_info_paddr,
						tx_classify_info_vaddr);

		kfree(dp_peer->peer_ext_ctx);
		dp_peer->peer_ext_ctx = NULL;

		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "dp-peer-cleanup: free peer_ext_ctx, peer_id=%u\n",
				 dp_peer->peer_id);
	}

	if (dp_peer->qos) {
		if (dp_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

		kfree(dp_peer->qos);
	}

	ath12k_dp_peer_stats_free(dp_peer);
}

void ath12k_wifi8_dp_peer_release(struct kref *ref)
{
	struct ath12k_dp_peer *dp_peer = container_of(ref, struct ath12k_dp_peer,
						      refcount);
	struct ath12k_dp_hw_group *dp_hw_grp = dp_peer->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_dp_hw *dp_hw = dp_peer->dp_hw;
	struct ath12k_dp *umac_dp;
	unsigned int num_peers;

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "dp-peer-release: peer_id=%u\n", dp_peer->peer_id);

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	/*
	 * Guaranteed: synchronize_net() was called by peer delete context
	 * Hence safe to free the resources
	 */
	ath12k_wifi8_dp_peer_cleanup(dp_hw, dp_peer);

	spin_lock_bh(&dp_hw->peer_hash_lock);

	clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);

	num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);

	list_del_init(&dp_peer->tqm_cleanup_node);

	dp_peer->dp_peer_state = ATH12K_DP_PEER_DELETED;

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	umac_dp = dp_hw_grp_wifi8->cumac_dp;

	ath12k_wifi8_dp_telemetry_peer_count_update(umac_dp->ab, num_peers);

	kfree(dp_peer);
}

void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id,
				 struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_base *central_ab = NULL;
	struct ath12k_dp_wifi8 *dp_wifi8 = NULL;
	struct hal_srng *srng = NULL;
	u16 link_band_id[HAL_TASC_BAND_MAX];
	struct ath12k_dp *umac_dp;
	struct ath12k_sta *ahsta;
	int i, ret;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		rcu_assign_pointer(ahsta->dp_peer, NULL);

		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	} else {
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	}

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	ath12k_dp_peer_hash_table_delete(dp_hw, dp_peer);

	spin_lock_bh(&dp_hw->peer_list_lock);
	list_del(&dp_peer->list);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	/* reset the telemetry peer stats based configs */
	for (i = 0; i < HAL_TASC_BAND_MAX; i++)
		link_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;

	umac_dp = dp_hw_grp_wifi8->cumac_dp;

	/*
	 * Free peer-level stats_id if it was allocated (VoW disabled path).
	 * When VoW is enabled stats_id is set to ATH12K_MAX_STATS_ID
	 * (invalid marker) at create time and must not be freed.
	 */
	if (dp_peer->stats_id < ATH12K_MAX_STATS_ID) {
		ath12k_wifi8_dp_telemetry_peer_delete(umac_dp, dp_peer->stats_id,
						      link_band_id);
		ath12k_wifi8_stats_id_free(dp_hw_grp_wifi8, dp_peer->stats_id);
	}

	/*
	 * Free per-TID stats IDs if they were allocated (VoW enabled path).
	 * Entries left at ATH12K_MAX_STATS_ID (invalid marker) were never
	 * allocated and are skipped.
	 */
	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		if (dp_peer->tid_stats_id[i] >= ATH12K_MAX_STATS_ID)
			continue;
		ath12k_wifi8_dp_telemetry_peer_delete(umac_dp,
						      dp_peer->tid_stats_id[i],
						      link_band_id);
		ath12k_wifi8_stats_id_free(dp_hw_grp_wifi8, dp_peer->tid_stats_id[i]);
	}

	if (dp_peer->sta_id != ATH12K_STA_ID_INVALID)
		clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);

	/* In v1 hardware, send peer clear command over all links.
	 * In v2 hardware, a new link_mask field allows a single SAM command
	 * to be sent for all links, with bits set for each link.
	 */
	if (!dp_peer->is_vdev_peer && dp_hw_grp_wifi8 && dp_hw_grp_wifi8->cumac_dp) {
		central_ab = dp_hw_grp_wifi8->cumac_dp->ab;
		dp_wifi8 = ath12k_get_dp_wifi8(dp_hw_grp_wifi8->cumac_dp);
		if (central_ab && dp_wifi8)
			srng = &central_ab->hal.srng_list[dp_wifi8->sam_cmd_ring.ring_id];

		if (srng) {
			if (central_ab->hw_rev == ATH12K_HW_QCN9625_HW10) {
				for (i = 0; i < HAL_TX_NUM_MAX_LINKS; i++) {
					ret = ath12k_wifi8_hal_tx_sam_cmd_send
							(central_ab, srng, i,
							HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
							dp_peer->sta_id, false, false);

					if (ret < 0)
						ath12k_warn(central_ab,
							    "failed to send SAM peer clear command for link %d: %d\n",
							    i, ret);
				}
			} else {
				ret = ath12k_wifi8_hal_tx_sam_cmd_send
						(central_ab, srng, -1,
						 HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
						 dp_peer->sta_id, false, false);
				if (ret < 0)
					ath12k_warn(central_ab,
						    "failed to send SAM peer clear command: %d\n",
						    ret);
			}
		}
	}

	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], NULL);

	if (dp_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED &&
	    dp_peer->peer_ext_ctx) {
		ath12k_dp_ast_entry_delete(dp_peer->dp_hw_grp,
					   dp_peer->peer_ext_ctx->ast_index);
	}

	dp_peer->dp_peer_state = ATH12K_DP_PEER_LOGICALLY_DELETED;

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	synchronize_net();

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "dp-peer-delete: peer_id=%u\n", dp_peer->peer_id);

	kref_put(&dp_peer->refcount, ath12k_wifi8_dp_peer_release);
}

void ath12k_wifi8_dp_smd_reset_tx_queue_states(struct ath12k_base *ab,
					       struct ath12k_dp_tx_flow_info *tx_info,
					u16 new_peer_id)
{
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct ath12k_dp_msdu_q_info *msduq;
	int n_mpduq = 0, n_msduq = 0;
	int i, j;

	spin_lock_bh(&tx_info->tx_q_lock);

	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "smd tx-reset: peer_id=%u new_peer_id=%u assoc_links=0x%lx txq_links=0x%lx\n",
		   tx_info->tid_info[0].peer_id, new_peer_id,
		   tx_info->assoc_hw_links_bitmap,
		   tx_info->txq_hw_links_bitmap);

	/* Reset txq_hw_links_bitmap: queues must be re-registered with FW */
	tx_info->txq_hw_links_bitmap = 0;

	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		mpduq = tx_info->tid_info[i].mpduq;
		if (!mpduq)
			continue;

		if (mpduq->mpduq_state == ATH12K_TX_Q_INIT_DONE) {
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tx-reset: tid[%d] mpduq paddr=0x%llx INIT_DONE->CREATED peer_id %u->%u\n",
				   i, (u64)mpduq->mpdu_q_paddr,
				   mpduq->flow_info.peer_id, new_peer_id);
			mpduq->mpduq_state = ATH12K_TX_Q_CREATED;
			mpduq->flow_info.peer_id = new_peer_id;
			mpduq->queue_number = (mpduq->queue_number & 0xFFFFFF) |
					      (new_peer_id << 24);
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tx-reset: tid[%d] mpduq queue_number: 0x%08x\n",
				   i, mpduq->queue_number);
			n_mpduq++;
		}

		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			msduq = tx_info->tid_info[i].msduq[j];
			if (!msduq)
				continue;

			if (msduq->msduq_state == ATH12K_TX_Q_INIT_DONE) {
				ath12k_dbg(ab, ATH12K_DBG_SMD,
					   "smd tx-reset: tid[%d] msduq[%d] paddr=0x%llx INIT_DONE->CREATED peer_id %u->%u\n",
					   i, j, (u64)msduq->msdu_q_paddr,
					   msduq->flow_info.peer_id, new_peer_id);
				msduq->msduq_state = ATH12K_TX_Q_CREATED;
				msduq->flow_info.peer_id = new_peer_id;
				msduq->queue_number = (msduq->queue_number & 0xFFFFFF) |
						      (new_peer_id << 24);
				ath12k_dbg(ab, ATH12K_DBG_SMD,
					   "smd tx-reset: tid[%d] msduq[%d] queue_number: 0x%08x\n",
					   i, j, msduq->queue_number);
				n_msduq++;
			}
		}
	}

	if (tx_info->mcast_mpduq &&
	    tx_info->mcast_mpduq->mpduq_state == ATH12K_TX_Q_INIT_DONE) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mcast_mpduq paddr=0x%llx INIT_DONE->CREATED peer_id %u->%u\n",
			   (u64)tx_info->mcast_mpduq->mpdu_q_paddr,
			   tx_info->mcast_mpduq->flow_info.peer_id, new_peer_id);
		tx_info->mcast_mpduq->mpduq_state = ATH12K_TX_Q_CREATED;
		tx_info->mcast_mpduq->flow_info.peer_id = new_peer_id;
		tx_info->mcast_mpduq->queue_number =
				(tx_info->mcast_mpduq->queue_number & 0xFFFFFF) |
				(new_peer_id << 24);
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mcast_mpduq queue_number: 0x%08x\n",
			   tx_info->mcast_mpduq->queue_number);
		n_mpduq++;
	}

	if (tx_info->mcast_msduq &&
	    tx_info->mcast_msduq->msduq_state == ATH12K_TX_Q_INIT_DONE) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mcast_msduq paddr=0x%llx INIT_DONE->CREATED peer_id %u->%u\n",
			   (u64)tx_info->mcast_msduq->msdu_q_paddr,
			   tx_info->mcast_msduq->flow_info.peer_id, new_peer_id);
		tx_info->mcast_msduq->msduq_state = ATH12K_TX_Q_CREATED;
		tx_info->mcast_msduq->flow_info.peer_id = new_peer_id;
		tx_info->mcast_msduq->queue_number =
				(tx_info->mcast_msduq->queue_number & 0xFFFFFF) |
				(new_peer_id << 24);
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mcast_msduq queue_number: 0x%08x\n",
			   tx_info->mcast_msduq->queue_number);
		n_msduq++;
	}

	if (tx_info->mgmt_mpduq &&
	    tx_info->mgmt_mpduq->mpduq_state == ATH12K_TX_Q_CREATED) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mgmt_mpduq paddr=0x%llx CREATED->CREATED peer_id %u->%u\n",
			   (u64)tx_info->mgmt_mpduq->mpdu_q_paddr,
			   tx_info->mgmt_mpduq->flow_info.peer_id, new_peer_id);
		tx_info->mgmt_mpduq->mpduq_state = ATH12K_TX_Q_CREATED;
		tx_info->mgmt_mpduq->flow_info.peer_id = new_peer_id;
		tx_info->mgmt_mpduq->queue_number =
			(tx_info->mgmt_mpduq->queue_number & 0xFFFFFF) |
			(new_peer_id << 24);
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: mgmt_mpduq queue_number: 0x%08x\n",
			   tx_info->mgmt_mpduq->queue_number);
		n_mpduq++;
	}

	for (i = 0; i < MGMT_MSDUQ_TYPE_MAX; i++) {
		msduq = tx_info->mgmt_msduq[i];
		if (!msduq)
			continue;

		if (msduq->msduq_state == ATH12K_TX_Q_CREATED) {
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tx-reset: mgmt_msduq[%d] paddr=0x%llx CREATED->CREATED peer_id %u->%u\n",
				   i, (u64)msduq->msdu_q_paddr,
				   msduq->flow_info.peer_id, new_peer_id);
			msduq->msduq_state = ATH12K_TX_Q_CREATED;
			msduq->flow_info.peer_id = new_peer_id;
			msduq->queue_number = (msduq->queue_number & 0xFFFFFF) |
					      (new_peer_id << 24);
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tx-reset: mgmt_msduq queue_number: 0x%08x\n",
				   msduq->queue_number);
			n_msduq++;
		}
	}

	if (tx_info->hol_msduq &&
	    tx_info->hol_msduq->msduq_state == ATH12K_TX_Q_CREATED) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: hol_msduq paddr=0x%llx CREATED->CREATED peer_id %u->%u\n",
			   (u64)tx_info->hol_msduq->msdu_q_paddr,
			   tx_info->hol_msduq->flow_info.peer_id, new_peer_id);
		tx_info->hol_msduq->msduq_state = ATH12K_TX_Q_CREATED;
		tx_info->hol_msduq->flow_info.peer_id = new_peer_id;
		tx_info->hol_msduq->queue_number =
			(tx_info->hol_msduq->queue_number & 0xFFFFFF) |
			(new_peer_id << 24);
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tx-reset: hol_msduq queue_number: 0x%08x\n",
			   tx_info->hol_msduq->queue_number);
		n_msduq++;
	}

	spin_unlock_bh(&tx_info->tx_q_lock);

	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "smd tx-reset: done — %d mpduq + %d msduq reset to CREATED, txq_links cleared\n",
		   n_mpduq, n_msduq);
}

void ath12k_wifi8_dp_smd_update_queue_peer_id_via_tqm(struct ath12k_base *ab,
					struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_dp_tx_flow_info *tx_info,
					u16 new_peer_id,
					u16 old_peer_id,
					u8 bitmap)
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	int n_msduq = 0, n_mpduq = 0;
	int i, j;
	int ret;

	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "smd tqm-update: updating peer_id %u -> %u via TQM UPDATE command\n",
		   old_peer_id, new_peer_id);

	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		if (!tx_info->tid_info[i].mpduq)
			continue;
		memset(&cmd, 0, sizeof(cmd));
		cmd.std.peer_id = old_peer_id;
		cmd.update_mpduq.mpdu_q_paddr =
			tx_info->tid_info[i].mpduq->mpdu_q_paddr;
		cmd.update_mpduq.new_peer_id = new_peer_id;
		cmd.update_mpduq.new_queue_number =
				tx_info->tid_info[i].mpduq->queue_number;
		cmd.update_mpduq.update_peer_id = true;
		cmd.update_mpduq.update_queue_number = true;
		cmd.update_mpduq.update_queue_valid = false;
		cmd.update_mpduq.update_tid = false;

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO,
						   &cmd, &(struct ath12k_dp_tx_queue){
							   .peer_id = new_peer_id,
							   .hw_link_id = 0
						   }, ath12k_dp_tqm_update_completion);
		if (ret) {
			ath12k_err(ab, "smd tqm-update: failed tid=%d ret=%d\n", i, ret);
			continue;
		}
		n_mpduq++;

		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tqm-update: tid[%d] mpduq updated\n", i);
	}

	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			if (!tx_info->tid_info[i].msduq[j])
				continue;

			memset(&cmd, 0, sizeof(cmd));
			cmd.std.peer_id = old_peer_id;
			cmd.update_tx_msdu_params.msdu_q_paddr =
				tx_info->tid_info[i].msduq[j]->msdu_q_paddr;
			cmd.update_tx_msdu_params.new_peer_id = new_peer_id;
			cmd.update_tx_msdu_params.tx_flow_number =
				tx_info->tid_info[i].msduq[j]->queue_number;
			cmd.update_tx_msdu_params.update_peer_id = true;
			cmd.update_tx_msdu_params.update_flow_number = true;
			cmd.update_tx_msdu_params.update_flow_valid = false;
			cmd.update_tx_msdu_params.update_tid = false;

			ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MSDUQ_BO,
						&cmd, &(struct ath12k_dp_tx_queue){
							.peer_id = new_peer_id,
							.hw_link_id = 0
						}, ath12k_dp_tqm_update_completion);
			if (ret) {
				ath12k_err(ab, "smd tqm-update: failed tid=%d q=%d ret=%d\n",
					   i, j, ret);
				continue;
			}
			n_msduq++;

			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tqm-update: tid[%d] msduq[%d] updated\n", i, j);
		}
	}

	if (tx_info->mgmt_mpduq) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.std.peer_id = old_peer_id;
		cmd.update_mpduq.mpdu_q_paddr = tx_info->mgmt_mpduq->mpdu_q_paddr;
		cmd.update_mpduq.new_peer_id = new_peer_id;
		cmd.update_mpduq.new_queue_number = tx_info->mgmt_mpduq->queue_number;
		cmd.update_mpduq.update_peer_id = true;
		cmd.update_mpduq.update_queue_number = true;

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO,
						   &cmd, &(struct ath12k_dp_tx_queue){
							   .peer_id = new_peer_id,
							   .hw_link_id = 0
						   }, ath12k_dp_tqm_update_completion);
		if (!ret) {
			n_mpduq++;
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tqm-update: mgmt_mpduq updated\n");
		}
	}

	for (i = 0; i < MGMT_MSDUQ_TYPE_MAX; i++) {
		if (!tx_info->mgmt_msduq[i])
			continue;
		memset(&cmd, 0, sizeof(cmd));
		cmd.std.peer_id = old_peer_id;
		cmd.update_tx_msdu_params.msdu_q_paddr =
			tx_info->mgmt_msduq[i]->msdu_q_paddr;
		cmd.update_tx_msdu_params.new_peer_id = new_peer_id;
		cmd.update_tx_msdu_params.tx_flow_number =
			tx_info->mgmt_msduq[i]->queue_number;
		cmd.update_tx_msdu_params.update_peer_id = true;
		cmd.update_tx_msdu_params.update_flow_number = true;
		cmd.update_tx_msdu_params.bitmap = bitmap;

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MSDUQ_BO,
						   &cmd, &(struct ath12k_dp_tx_queue){
							   .peer_id = new_peer_id,
							   .hw_link_id = 0
						   }, ath12k_dp_tqm_update_completion);
		if (!ret) {
			n_msduq++;
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tqm-update: mgmt_msduq[%d] updated\n", i);
		}
	}

	if (tx_info->hol_msduq) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.std.peer_id = old_peer_id;
		cmd.update_tx_msdu_params.msdu_q_paddr = tx_info->hol_msduq->msdu_q_paddr;
		cmd.update_tx_msdu_params.new_peer_id = new_peer_id;
		cmd.update_tx_msdu_params.tx_flow_number =
			tx_info->hol_msduq->queue_number;
		cmd.update_tx_msdu_params.update_peer_id = true;
		cmd.update_tx_msdu_params.update_flow_number = true;
		cmd.update_tx_msdu_params.bitmap = bitmap;

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MSDUQ_BO,
						   &cmd, &(struct ath12k_dp_tx_queue){
							   .peer_id = new_peer_id,
							   .hw_link_id = 0
						   }, ath12k_dp_tqm_update_completion);
		if (!ret) {
			n_msduq++;
			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tqm-update: hol_msduq updated\n");
		}
	}

	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "smd tqm-update: completed — %d mpduq + %d msduq updated via TQM\n",
		   n_mpduq, n_msduq);
}

/**
 * enum ath12k_smd_assoc_action - result code from
 *     ath12k_wifi8_dp_peer_assoc_smd_transition().
 * @ATH12K_SMD_ASSOC_CONTINUE: not an SMD transition; caller proceeds with
 *     normal ext_ctx allocation.  @dp_hw->peer_hash_lock remains held.
 * @ATH12K_SMD_ASSOC_DONE: SMD transition handled (MLO PREP path); caller
 *     must release @dp_hw->peer_hash_lock and return 0.
 * @ATH12K_SMD_ASSOC_EXEC: SMD transition handled (SLO post-DL-drain path);
 *     caller must release @dp_hw->peer_hash_lock, call
 *     ath12k_wifi8_dp_smd_exec_activate_links(), then return 0.
 */
enum ath12k_smd_assoc_action {
	ATH12K_SMD_ASSOC_CONTINUE = 0,
	ATH12K_SMD_ASSOC_DONE,
	ATH12K_SMD_ASSOC_EXEC,
};

/**
 * ath12k_wifi8_dp_peer_assoc_smd_transition() - SMD BSS Transition fast path
 *     in peer_assoc.
 *
 * Checks whether this peer_assoc belongs to an in-progress SMD BSS
 * Transition and shares the parked ext_ctx with the target peer if so.
 *
 * Does NOT acquire or release @dp_hw->peer_hash_lock — that remains the
 * exclusive responsibility of the caller.
 *
 * Context: caller holds @dp_hw->peer_hash_lock (bh-disabled).
 * Return: action code indicating what the caller must do next.
 */
static enum ath12k_smd_assoc_action
ath12k_wifi8_dp_peer_assoc_smd_transition(struct ath12k_dp *dp,
					  struct ath12k_dp_peer *dp_peer,
					  const u8 *addr)
{
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx;
	bool exec_in_progress;

	/* SMD BSS Transition only applies to STA-mode BSS peers. */
	if (!dp_peer->is_sta_bss_peer)
		return ATH12K_SMD_ASSOC_CONTINUE;

	spin_lock_bh(&dp->dp_hw_grp->smd_transition_lock);

	if (!dp->dp_hw_grp->smd_parked_ext_ctx ||
	    !ether_addr_equal(dp->dp_hw_grp->smd_target_mld_addr, addr)) {
		spin_unlock_bh(&dp->dp_hw_grp->smd_transition_lock);
		return ATH12K_SMD_ASSOC_CONTINUE;
	}

	exec_in_progress = dp->dp_hw_grp->smd_exec_in_progress;
	peer_ext_ctx = dp->dp_hw_grp->smd_parked_ext_ctx;
	/* Assign under smd_transition_lock to prevent UAF if the transition
	 * is aborted and smd_parked_ext_ctx freed after the unlock.
	 * Keep smd_parked_ext_ctx and smd_target_mld_addr set for EXEC.
	 */
	dp_peer->peer_ext_ctx = peer_ext_ctx;
	spin_unlock_bh(&dp->dp_hw_grp->smd_transition_lock);

	if (exec_in_progress) {
		/* SLO post-DL-drain path: EXEC resp already happened but
		 * exec_activate_links was skipped because the target peer had
		 * no links yet.  Now that peer_assoc has been called and the
		 * peer is fully set up, signal the caller to drive the TQM
		 * UPDATE + AST transition immediately.
		 */
		ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
			   "smd assoc: shared ext_ctx %pM (SLO post-DL-drain, exec now)\n",
			   addr);
		return ATH12K_SMD_ASSOC_EXEC;
	}

	/* MLO PREP resp path: ext_ctx shared with target peer, TQM/AST
	 * transition deferred to exec_activate_links during EXEC resp.
	 */
	ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
		   "smd assoc: shared ext_ctx %pM (MLO PREP, AST deferred to EXEC)\n",
		   addr);
	return ATH12K_SMD_ASSOC_DONE;
}

int ath12k_wifi8_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr,
			       struct ath12k_dp_peer_create_params *params)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8;
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	u16 link_band_id[HAL_TASC_BAND_MAX];
	dma_addr_t pn_counter_paddr = 0;
	struct ath12k_dp_peer *dp_peer;
	dma_addr_t tx_classify_paddr;
	struct ath12k_dp *umac_dp;
	void *tx_classify_vaddr;
	unsigned int num_peers;
	bool is_qos = true;
	int ret, i;
	int vdev_peer_link_id;
	struct ath12k_dp_link_vif *dp_link_vif;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 ppeds_idx_map_val = 0;
#endif
	u16 tid_band_id[ATH12K_DATA_TID_MAX][HAL_TASC_BAND_MAX];
	struct ath12k_pdev_dp *dp_pdev;
	bool vow_enabled;
	u8 tid;
	int j;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, params->sta);
	else
		dp_peer = ath12k_dp_vdev_peer_check(dp_hw, addr, params->hw_link_id);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_err(NULL, "Unable to find dp_peer during assoc %pM\n", addr);
		return -ENOENT;
	}

	switch (ath12k_wifi8_dp_peer_assoc_smd_transition(dp, dp_peer, addr)) {
	case ATH12K_SMD_ASSOC_EXEC:
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_wifi8_dp_smd_exec_activate_links(dp, dp_hw, dp_vif,
							dp_peer->addr,
							BIT(dp_peer->hw_link_id));
		return 0;
	case ATH12K_SMD_ASSOC_DONE:
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return 0;
	case ATH12K_SMD_ASSOC_CONTINUE:
		break;
	}

	/* SMD BSS Transition: skip allocation if ext_ctx was already shared
	 * on a prior peer_assoc call for this peer (MLO multi-link case where
	 * addr is a link address that does not match smd_target_mld_addr).
	 * Only relevant for STA-mode BSS peers.
	 */
	if (dp_peer->is_sta_bss_peer && dp_peer->peer_ext_ctx) {
		ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
			   "smd assoc: peer_ext_ctx already exists for %pM, skipping allocation\n",
			   addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return 0;
	}

	if (dp_peer->peer_ext_ctx) {
		ath12k_err(dp->ab,
			   "peer ext is already allocated, wrong peer found %pM", addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	peer_ext_ctx = kzalloc(sizeof(*peer_ext_ctx), GFP_ATOMIC);
	if (!peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOMEM;
	}

	for (i = 0; i < HAL_TASC_BAND_MAX; i++)
		link_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;

	for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++)
		for (j = 0; j < HAL_TASC_BAND_MAX; j++)
			tid_band_id[tid][j] = DP_TELEMETRY_INVALID_LINK_BAND_ID;

	dp_hw_group_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	umac_dp = dp_hw_group_wifi8->cumac_dp;

	dp_peer->peer_ext_ctx = peer_ext_ctx;
	spin_lock_init(&peer_ext_ctx->tx_flow_info.tx_q_lock);
	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp,
					     dp_peer->assoc_hw_link_id);
	vow_enabled = dp_pdev ? ath12k_dp_vow_stats_enabled(dp_pdev) : false;

	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
		if (!link_peer)
			continue;

		set_bit(link_peer->hw_link_id,
			&peer_ext_ctx->tx_flow_info.assoc_hw_links_bitmap);

		if (!vow_enabled || dp_peer->is_vdev_peer) {
			link_band_id[link_peer->hw_link_id] = link_peer->link_band_id;
		} else {
			for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++) {
				if (link_peer->tid_band_id[tid] < ATH12K_MAX_STATS_ID)
					tid_band_id[tid][link_peer->hw_link_id] =
						link_peer->tid_band_id[tid];
			}
		}

		if (dp_peer->is_vdev_peer) {
			vdev_peer_link_id = dp_peer->hw_links[link_peer->hw_link_id];
			break;
		}
	}

	rcu_read_unlock();

	num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);
	ath12k_wifi8_dp_telemetry_peer_count_update(umac_dp->ab, num_peers);

	/*
	 * Configure HW peer telemetry registers.
	 *
	 * VoW disabled (or) Self peer : Program the single peer-level stats_id
	 * with the per-link band ID. One HW descriptor per window covers all TIDs.
	 *
	 * VoW enabled: Program one stats_id per data TID, each paired with that
	 * TID's dedicated per-link band ID so HW delivers isolated per-TID,
	 * per-band descriptors.  The peer-level stats_id is not programmed.
	 */
	if (!vow_enabled || dp_peer->is_vdev_peer) {
		ath12k_wifi8_dp_telemetry_peer_config(umac_dp, dp_peer->stats_id,
						      link_band_id);
	} else {
		ath12k_wifi8_dp_vow_telemetry_peer_config(umac_dp, dp_peer, tid_band_id);
	}

	ret = ath12k_dp_tx_classify_info_alloc(dp->dp_hw_grp,
					       &tx_classify_paddr,
					       &tx_classify_vaddr);
	if (ret)
		goto free_peer_ext_ctx;

	peer_ext_ctx->tx_flow_info.hw_who_classify_info_vaddr = tx_classify_vaddr;
	peer_ext_ctx->tx_flow_info.hw_who_classify_info_paddr = tx_classify_paddr;
	pn_counter_paddr = ath12k_dp_get_page_paddr(dp->dp_hw_grp, dp_peer->peer_id);
	if (!pn_counter_paddr) {
		ret = -ENOMEM;
		goto free_tx_classify_info;
	}

	if (dp_peer->is_vdev_peer) {
		ret = ath12k_peer_alloc_mcast_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	} else {
		if (ath12k_dp_peer_get_sta(dp_peer)->wme)
			is_qos = true;
		else
			is_qos = false;

		ret = ath12k_peer_alloc_default_queues(dp->dp_hw_grp, dp_peer,
						       dp_vif, is_qos);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_hol_queues(dp->dp_hw_grp, dp_peer,
						   dp_vif, is_qos);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_mgmt_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	}

	memcpy(ast_param.mac_addr, addr, ETH_ALEN);
	ast_param.peer_id = dp_peer->peer_id;
	ast_param.tx_classify_info_paddr = tx_classify_paddr;
	ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_USE_ADDRX;
	if (dp_peer->is_vdev_peer)
		ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_AP_BSS_ENTRY;
	ret = ath12k_dp_ast_entry_create(dp->dp_hw_grp, &ast_param);
	if (ret)
		goto free_queues_info;

	peer_ext_ctx->ast_index = ast_param.ast_index;
	peer_ext_ctx->ast_hash = ast_param.ast_hash;

	if (dp_peer->is_sta_bss_peer) {
		dp_vif->ast_idx = ast_param.ast_index;
		dp_vif->ast_hash = ast_param.ast_hash;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ppeds_idx_map_val |=
			u32_encode_bits(dp_vif->ast_idx, HAL_TX_PPEDS_CFG_SEARCH_IDX) |
			u32_encode_bits(dp_vif->ast_hash, HAL_TX_PPEDS_CFG_CACHE_SET);
		ath12k_wifi8_hal_ppeds_cfg_ast(dp->ab, dp_vif->ppe_vp_num,
						ppeds_idx_map_val);

		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "STA ast_idx:%d hash:%d ppe_vp:%d\n",
				 dp_vif->ast_idx,
				 dp_vif->ast_hash,
				 dp_vif->ppe_vp_num);
#endif
	} else if (dp_peer->is_vdev_peer) {
		dp_link_vif = &dp_vif->dp_link_vif[vdev_peer_link_id];
		dp_link_vif->ast_idx = ast_param.ast_index;
		dp_link_vif->ast_hash =	ast_param.ast_hash;
	}

	/*
	 * Take peer reference for TQM cleanup
	 */
	kref_get(&dp_peer->refcount);

	/* Register in TQM cleanup list */
	list_add_tail(&dp_peer->tqm_cleanup_node, &dp_hw->tqm_cleanup_list);

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return 0;

free_queues_info:
	ath12k_peer_free_static_queues(dp->dp_hw_grp, dp_peer);
free_tx_classify_info:
	ath12k_dp_tx_classify_info_free(dp->dp_hw_grp,
					tx_classify_paddr,
					tx_classify_vaddr);
free_peer_ext_ctx:
	kfree(peer_ext_ctx);
	dp_peer->peer_ext_ctx = NULL;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

void ath12k_wifi8_dp_link_peer_assign_id(struct ath12k_dp *dp, struct ath12k *ar,
					 struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	u8 tid, i;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	if (!ath12k_dp_vow_stats_enabled(&ar->dp) ||
	    peer->dp_peer->is_vdev_peer) {
		/*
		 * VoW disabled or vdev/self peer: one link_band_id per link
		 * peer covers all TIDs.
		 * tid_band_id[] entries should be set to ATH12K_MAX_STATS_ID
		 * (invalid marker).
		 */
		peer->link_band_id =
			ath12k_wifi8_link_band_id_alloc(dp_hw_grp_wifi8);
		for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++)
			peer->tid_band_id[tid] = ATH12K_MAX_STATS_ID;
	} else {
		/*
		 * VoW enabled: Allocate one band ID per data TID so each TID's
		 * HW descriptor carries isolated per-band stats.
		 * link_band_id is set to ATH12K_MAX_STATS_ID (invalid marker)
		 * to signal to the unassign path that per-TID slots are in use.
		 */
		peer->link_band_id = ATH12K_MAX_STATS_ID;
		for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++) {
			peer->tid_band_id[tid] =
				ath12k_wifi8_link_band_id_alloc(dp_hw_grp_wifi8);
			if (peer->tid_band_id[tid] < ATH12K_MAX_STATS_ID)
				continue;

			/*
			 * If pool is exhausted mid-loop, roll back already
			 * allocated tid_band_id entries, reset to invalid
			 * marker.
			 * The link peer is still created;
			 * VoW per-band telemetry will be absent for this link.
			 */
			ath12k_err(NULL, "link_band_id exhausted - tid %u\n", tid);

			for (i = 0; i < tid; i++) {
				ath12k_wifi8_link_band_id_free(dp_hw_grp_wifi8,
							       peer->tid_band_id[i]);
				peer->tid_band_id[i] = ATH12K_MAX_STATS_ID;
			}

			break;
		}
	}
}

void ath12k_wifi8_dp_link_peer_unassign_id(struct ath12k_dp *dp, struct ath12k *ar,
					   struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	u8 tid;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	ath12k_wifi8_stats_id_map_update_hw_link(dp_hw_grp_wifi8, peer->dp_peer,
						       peer);

	/* VoW disabled path: free the single shared link_band_id */
	if (peer->link_band_id < ATH12K_MAX_STATS_ID) {
		ath12k_wifi8_link_band_id_free(dp_hw_grp_wifi8, peer->link_band_id);
		return;
	}

	/* VoW enabled path: free per-TID band slots; skip invalid marker entries */
	for (tid = 0; tid < ATH12K_DATA_TID_MAX; tid++) {
		if (peer->tid_band_id[tid] >= ATH12K_MAX_STATS_ID)
			continue;
		ath12k_wifi8_link_band_id_free(dp_hw_grp_wifi8,
					       peer->tid_band_id[tid]);
	}
}

int ath12k_dp_tqm_update_mpduq_sn_pn(struct ath12k_base *ab,
				     struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				     struct ath12k_dp_peer *dp_peer,
				     u16 sn, u8 *pn)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(ab->dp->dp_hw_grp);
	struct ath12k_hal_tqm_cmd cmd = {0};
	int ret = 0;
	void *pn_vaddr = NULL;
	dma_addr_t pn_paddr;

	if (!sw_mpduq_ptr)
		return ret;

	if (sn >= ATH12K_DP_MAX_SEQ_NUM) {
		ath12k_err(ab, "Error: invalid sn peer = %d", dp_peer->peer_id);
		return -EINVAL;
	}

	pn_vaddr = ath12k_dp_get_page_vaddr(ab->dp->dp_hw_grp, dp_peer->peer_id);
	if (!pn_vaddr) {
		ath12k_err(ab, "Error: pn_addr is NULL peer = %d", dp_peer->peer_id);
		return -EINVAL;
	}
	memcpy(pn_vaddr, pn, ATH12K_DP_PN_COUNTER_SIZE);
	pn_paddr = ath12k_dp_get_page_paddr(ab->dp->dp_hw_grp, dp_peer->peer_id);
	ath12k_core_dma_sync_single_for_device(dev, pn_paddr,
					       ATH12K_DP_PN_COUNTER_SIZE,
					       DMA_BIDIRECTIONAL);

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.update_mpduq.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.update_mpduq.sn_num_valid = 1;
	cmd.update_mpduq.sn_num = sn;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO, &cmd,
					   NULL, NULL);
	return ret;
}

int ath12k_dp_tqm_update_mpduq_max_lsn(struct ath12k_base *ab,
				       struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				       struct ath12k_dp_peer *dp_peer,
				       u16 max_lsn)
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	int ret = 0;

	if (!sw_mpduq_ptr)
		return ret;

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.update_mpduq.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.update_mpduq.max_lsn_valid = 1;
	cmd.update_mpduq.max_lsn = max_lsn;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO, &cmd,
					   NULL, NULL);
	return ret;
}

/*
 * Reset only the SN to 0 in a single MPDUQ.  PN counter and LSN are
 * intentionally left untouched — used when UL SN is not transferred.
 */
static int ath12k_dp_tqm_reset_mpduq_sn(struct ath12k_base *ab,
					struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
					 struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_hal_tqm_cmd cmd = {0};

	if (!sw_mpduq_ptr)
		return 0;

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.update_mpduq.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.update_mpduq.sn_num_valid = 1;
	cmd.update_mpduq.sn_num = 0;

	return ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO,
					    &cmd, NULL, NULL);
}

int ath12k_wifi8_peer_tx_tid_update_for_smd(struct ath12k_base *ab,
					    struct ath12k_dp_hw *dp_hw,
					    struct ieee80211_vif *vif,
					    const u8 *peer_addr,
					    struct ath12k_tx_smd_ctx_per_tid *tx_tid_ctx)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_dp_peer *dp_peer;
	int ret;

	if (!dp_hw || !peer_addr || !tx_tid_ctx) {
		ath12k_err(ab, "invalid args for SMD TX update\n");
		return -EINVAL;
	}

	if (tx_tid_ctx->ssn > ATH12K_DP_MAX_SEQ_NUM) {
		ath12k_warn(ab, "Invalid TX SSN 0x%x for tid %d\n",
			    tx_tid_ctx->ssn, tx_tid_ctx->tid);
		return -EINVAL;
	}

	if (tx_tid_ctx->tid > ATH12K_SMD_TX_MGMT_TID) {
		ath12k_warn(ab, "SMD update Invalid TX tid %d\n", tx_tid_ctx->tid);
		return -EINVAL;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "failed to find peer %pM for SMD TX update\n",
			    peer_addr);
		return -ENOENT;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(ab, "SMD TX update invalid tx flow info peer %pM",
			   dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	if (tx_tid_ctx->tid < ATH12K_MAX_NUM_DATA_TIDS)
		sw_mpduq_ptr = tx_flow_info->tid_info[tx_tid_ctx->tid].mpduq;
	else if (tx_tid_ctx->tid == ATH12K_SMD_TX_MGMT_TID)
		sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;

	if (!sw_mpduq_ptr) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);

		/*
		 * for data TIDs the mpduq is allocated only when a BA session is
		 * established. If it is not yet set up on the new AP, skip the
		 * datapath update — the SSN/PN context is already cached in
		 * ahsta->smd_info and will be pushed to FW via WMI.
		 */
		if (tx_tid_ctx->tid != TQM_NON_DATA_TID) {
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "SMD TX update: no mpduq tid %d peer %pM, skip\n",
				   tx_tid_ctx->tid, dp_peer->addr);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return 0;
		}
		ath12k_err(ab, "SMD TX update invalid tx MPDUQ peer %pM",
			   dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	ret = ath12k_dp_tqm_update_mpduq_sn_pn(ab, sw_mpduq_ptr, dp_peer,
					       tx_tid_ctx->ssn,
					       tx_tid_ctx->pn_number);
	if (ret) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_err(ab, "SMD UPDATE MPDUQ failed tid %d peer %pM id %d\n",
			   tx_tid_ctx->tid, dp_peer->addr, dp_peer->peer_id);
		return ret;
	}

	if (tx_tid_ctx->lsn_offset > 0 &&
	    tx_tid_ctx->lsn_offset <= ATH12K_DP_MAX_POSSIBLE_BA_WIN) {
		ret = ath12k_dp_tqm_update_mpduq_max_lsn(ab, sw_mpduq_ptr,
							 dp_peer,
							 tx_tid_ctx->lsn_offset);
		if (ret) {
			ath12k_err(ab,
				   "SMD UPDATE LSN failed tid %d peer %pM id %d\n",
				   tx_tid_ctx->tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return ret;
		}
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	ath12k_dbg(ab, ATH12K_DBG_DP_TX,
		   "SMD TX update done for peer %pM tid %d: SSN=0x%x\n",
		   peer_addr, tx_tid_ctx->tid, tx_tid_ctx->ssn);

	return 0;
}

/*
 * Reset TQM SN to 0 for all data TIDs of a peer without touching PN or LSN.
 * Called when request_ul_sn_not_transferred is set in DYNAMIC_CONTEXT.
 */
int ath12k_wifi8_peer_tx_tid_sn_reset(struct ath12k_base *ab,
				      struct ath12k_dp_hw *dp_hw,
				      struct ieee80211_vif *vif,
				      const u8 *peer_addr)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr;
	struct ath12k_dp_peer *dp_peer;
	int ret = 0;
	u8 tid;

	if (!dp_hw || !peer_addr)
		return -EINVAL;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "failed to find peer %pM for SMD TX update\n",
			    peer_addr);
		return -ENOENT;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(ab, "SMD TX update invalid tx flow info peer %pM",
			   dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		sw_mpduq_ptr = tx_flow_info->tid_info[tid].mpduq;
		if (!sw_mpduq_ptr)
			continue;

		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "SMD SN reset: peer %pM tid %u TQM SN -> 0\n",
				 peer_addr, tid);

		ret = ath12k_dp_tqm_reset_mpduq_sn(ab, sw_mpduq_ptr, dp_peer);
		if (ret)
			ath12k_err(ab, "SMD SN reset failed tid %d peer %pM: %d\n",
				   tid, peer_addr, ret);
	}

	/* Management TID (ATH12K_SMD_TX_MGMT_TID = 15) uses a separate mpduq */
	sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
	if (sw_mpduq_ptr) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "SMD SN reset: peer %pM tid %u (mgmt) TQM SN -> 0\n",
				 peer_addr, 15);
		ret = ath12k_dp_tqm_reset_mpduq_sn(ab, sw_mpduq_ptr, dp_peer);
		if (ret)
			ath12k_err(ab, "SMD SN reset failed mgmt tid peer %pM: %d\n",
				   peer_addr, ret);
	}

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "SMD SN reset done for peer %pM\n", peer_addr);

	return ret;
}

void ath12k_dp_peer_get_mpdu_queues_stats_status(struct ath12k_dp *dp,
						 void *ctx,
						 struct hal_tqm_status *tqm_status)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_tx_queue *data = (struct ath12k_dp_tx_queue *)ctx;
	struct ath12k_smd_ctx_tx_cb_per_tid tx_ctx = {};

	if (!tqm_status) {
		ath12k_err(ab, "Error: TQM STATUS is not valid");
		return;
	}
	if (tqm_status->status_hdr.cmd_execution_status != HAL_TQM_SUCCESSFUL_EXECUTION) {
		ath12k_err(ab, "Error: TQM STATUS FAILED with reason %d",
			   tqm_status->status_hdr.cmd_execution_status);
		return;
	}

	tx_ctx.sn = tqm_status->mpduq_stats.max_lsn;
	tx_ctx.lsn_offset = data->cookie;
	tx_ctx.pn_len = 6;
	memcpy(&tx_ctx.pn[0], &tqm_status->mpduq_stats.pn_31_0, sizeof(u32));
	memcpy(&tx_ctx.pn[4], &tqm_status->mpduq_stats.pn_47_32, sizeof(u16));

	if (data->cb)
		data->cb(dp, &tx_ctx, data->addr, tqm_status->mpduq_stats.tid);
}

int ath12k_dp_tqm_get_mpdu_queue_stats(struct ath12k_base *ab,
				       struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				       struct ath12k_dp_peer *dp_peer,
				       bool clear_stats,
				       u16 cookie,
				       void (*cb)(struct ath12k_dp *dp, void *ctx,
						  u8 *addr, u8 tid))
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	struct ath12k_dp_tx_queue data =  {0};
	int ret = -EINVAL;

	if (!sw_mpduq_ptr)
		return ret;

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.get_mpduq_stats.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.get_mpduq_stats.clear_stats = clear_stats;

	data.peer_id = dp_peer->peer_id;
	data.cookie = cookie;
	data.cb = cb;
	memcpy(&data.addr, dp_peer->addr, ETH_ALEN);

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_GET_MPDUQ_STATS_BO, &cmd,
					   &data,
					   ath12k_dp_peer_get_mpdu_queues_stats_status);

	return ret;
}

u16 ath12k_dp_peer_compute_max_lsn(u16 ba_size)
{
	return ba_size / 2;
}

int ath12k_dp_peer_fetch_smd_tx_ctx(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u32 tx_tid_bitmap,
				    u16 *tx_tid_ba_win_size,
				    void (*cb)(struct ath12k_dp *dp, void *ctx,
					       u8 *addr, u8 tid))
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int tid;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(ab, "invalid tx flow info peer %pM", dp_peer->addr);
		return -EINVAL;
	}
	spin_lock_bh(&tx_flow_info->tx_q_lock);
	/* data tids */
	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		u16 lsn_offset_tap = ATH12K_DP_INVALID_MLSN_OFFSET;
		u16 lsn_offset_sap;

		if (!(tx_tid_bitmap & BIT(tid)))
			continue;

		sw_mpduq_ptr = tx_flow_info->tid_info[tid].mpduq;
		if (!sw_mpduq_ptr)
			continue;

		/* update the max sequence number */
		if (tx_tid_ba_win_size[tid] <= ATH12K_DP_MAX_POSSIBLE_BA_WIN) {
			lsn_offset_sap =
				ath12k_dp_peer_compute_max_lsn(tx_tid_ba_win_size[tid]);
			ret = ath12k_dp_tqm_update_mpduq_max_lsn(ab, sw_mpduq_ptr,
								 dp_peer,
								 lsn_offset_sap);
			if (ret) {
				ath12k_err(ab,
					   "UPDATE MPDUQ failed tid %d peer %pM id %d\n",
					   tid, dp_peer->addr, dp_peer->peer_id);
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				return ret;
			}
			lsn_offset_tap = tx_tid_ba_win_size[tid] - lsn_offset_sap;
		}
		/* fetch SN and PN */
		ret = ath12k_dp_tqm_get_mpdu_queue_stats(ab, sw_mpduq_ptr,
							 dp_peer, false,
							 lsn_offset_tap, cb);
		if (ret) {
			ath12k_err(ab, "GET MPDUQ failed tid %d peer %pM id %d\n",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
	}

	/* mgmt tid */
	if (tx_tid_bitmap & BIT(ATH12K_SMD_TX_MGMT_TID)) {
		sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
		ret = ath12k_dp_tqm_get_mpdu_queue_stats(ab, sw_mpduq_ptr,
							 dp_peer, false, 0, cb);
		if (ret) {
			ath12k_err(ab, "GET MPDUQ failed tid %d peer %pM id %d\n",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_msduq_send(struct ath12k_base *ab,
				    struct ath12k_dp_msdu_q_info *sw_msduq_ptr,
				    struct ath12k_dp_peer *dp_peer,
				    u16 count)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	if (!sw_msduq_ptr)
		return ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.remove_msdu_params.block_tx_notify_frame_removal = 0;
	cmd.remove_msdu_params.count = count;
	cmd.remove_msdu_params.qtype = sw_msduq_ptr->flow_info.flow_type;
	cmd.remove_msdu_params.msdu_q_paddr = sw_msduq_ptr->msdu_q_paddr;

	if (count == ATH12K_MAX_MSDU_COUNT)
		cmd.remove_msdu_params.type = HAL_WIFIREMOVE_MSDUS_AND_DISABLE_FLOW;
	else
		cmd.remove_msdu_params.type = HAL_WIFIREMOVE_HEAD_MSDUS;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_REMOVE_MSDU_BO, &cmd,
					   NULL, NULL);

	return ret;
}

int ath12k_dp_tqm_remove_mpduq_send(struct ath12k_base *ab,
				    struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				    struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	if (!sw_mpduq_ptr)
		return ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.remove_mpdu_params.type = HAL_WIFIREMOVE_MPDUS_AND_DISABLE_QUEUE;
	cmd.remove_mpdu_params.block_tx_notify_frame_removal = 0;
	cmd.remove_mpdu_params.count = 0xFFF; /* TODO: check if 0xFFFF needed */
	cmd.remove_mpdu_params.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_REMOVE_MPDU_BO, &cmd,
					   NULL, NULL);

	return ret;
}

int ath12k_dp_tqm_remove_mcast_queues(struct ath12k_base *ab,
				      struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	sw_msduq_ptr = tx_flow_info->mcast_msduq;
	ret = ath12k_dp_tqm_remove_msduq_send(ab,
					      sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM MSDUQ send failed for MCAST frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;

	sw_mpduq_ptr = tx_flow_info->mcast_mpduq;
	ret = ath12k_dp_tqm_remove_mpduq_send(ab,
					      sw_mpduq_ptr,
					      dp_peer);
	if (ret) {
		ath12k_err(ab,
			   "TQM MPDUQ send failed for MCAST frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret; /* TODO: retry on error? */
	}
	if (sw_mpduq_ptr)
		sw_mpduq_ptr->tqm_send = 1;

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_data_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int tid, q;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	sw_msduq_ptr = tx_flow_info->hol_msduq;
	ret = ath12k_dp_tqm_remove_msduq_send(ab,
					      sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM MSDUQ send failed for HOL frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;

	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		for (q = 0; q < ATH12K_MAX_DP_MSDUQ_PER_TID; q++) {
			sw_msduq_ptr = tx_flow_info->tid_info[tid].msduq[q];
			ret = ath12k_dp_tqm_remove_msduq_send(ab,
							      sw_msduq_ptr,
							      dp_peer,
							      ATH12K_MAX_MSDU_COUNT);
			if (ret) {
				ath12k_err(
				ab,
				"TQM MSDUQ fail: data frame %d tid %d peer %pM id %d",
				q, tid, dp_peer->addr, dp_peer->peer_id);
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				return ret; /* TODO: return or continue? */
			}
			if (sw_msduq_ptr)
				sw_msduq_ptr->tqm_send = 1;
		}
		sw_mpduq_ptr = tx_flow_info->tid_info[tid].mpduq;
		ret = ath12k_dp_tqm_remove_mpduq_send(ab,
						      sw_mpduq_ptr,
						      dp_peer);
		if (ret) {
			ath12k_err(ab,
				   "TQM MPDUQ fail: data frame tid %d peer %pM id %d",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret; /* TODO: return or continue? */
		}
		if (sw_mpduq_ptr)
			sw_mpduq_ptr->tqm_send = 1;
	}

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_mgmt_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int q;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	for (q = 0; q < MGMT_MSDUQ_TYPE_MAX; q++) {
		sw_msduq_ptr = tx_flow_info->mgmt_msduq[q];
		if (!sw_msduq_ptr || sw_msduq_ptr->tqm_send)
			continue;
		ret = ath12k_dp_tqm_remove_msduq_send(ab,
						      sw_msduq_ptr,
						      dp_peer,
						      ATH12K_MAX_MSDU_COUNT);
		if (ret) {
			ath12k_err(ab,
				   "TQM MSDUQ fail: MGMT frame %d peer %pM id %d",
				   q, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
		if (sw_msduq_ptr)
			sw_msduq_ptr->tqm_send = 1;
	}
	sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
	ret = ath12k_dp_tqm_remove_mpduq_send(ab,
					      sw_mpduq_ptr,
					      dp_peer);
	if (ret) {
		ath12k_err(ab,
			   "TQM MPDUQ fail: MGMT frame peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret; /* TODO: return or continue? */
	}
	if (sw_mpduq_ptr)
		sw_mpduq_ptr->tqm_send = 1;

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

static inline
int ath12k_dp_tqm_remove_link_mgmt_queues(struct ath12k_base *ab,
					  struct ath12k_dp_peer *dp_peer,
					  u8 hw_link_id)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	int ret = 0;
	u8 idx;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
	sw_msduq_ptr = tx_flow_info->mgmt_msduq[idx];
	ret = ath12k_dp_tqm_remove_msduq_send(ab, sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM Link mgmt MSDUQ fail: peer %pM linkid %d",
			   dp_peer->addr, hw_link_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

static inline
int ath12k_dp_tqm_sync_remove_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer,
				     struct ath12k_dp_tx_queue *data)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-sync: ENTRY peer %pM peer_id=%u hw_link_id=%u\n",
			 dp_peer->addr, data->peer_id, data->hw_link_id);

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = 0xFFFF;
	cmd.tqm_sync_params.cb_func = ath12k_dp_peer_cleanup_tqm_sync;
	cmd.tqm_sync_params.cb_ctxt = dp_peer;
	cmd.tqm_sync_params.cb_data = 0;
	if (!cmd.tqm_sync_params.cb_func)
		cmd.tqm_sync_params.data_only = 1;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "tqm-sync: sending TQM_SYNC_CMD with cb_func=%p cb_ctxt=%p\n",
			 cmd.tqm_sync_params.cb_func, cmd.tqm_sync_params.cb_ctxt);
	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_SYNC_CMD_BO, &cmd, data,
					   ath12k_dp_peer_cleanup_tqm_sync);
	if (ret)
		ath12k_err(ab, "tqm-sync: TQM_SYNC_CMD send failed ret=%d\n", ret);
	else
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "tqm-sync: TQM_SYNC_CMD sent successfully\n");

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-sync: EXIT peer_id=%u ret=%d\n", data->peer_id, ret);
	return ret;
}

static inline
int ath12k_dp_tqm_remove_mgmt_link_queues(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u8 hw_link_id)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(ab->dp);
	int ret_mgmt = 0;

	ab = central_dp->ab;

	if (dp_peer->is_vdev_peer) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "peer %d is mcast peer", dp_peer->peer_id);
		return 0;
	}
	ret_mgmt = ath12k_dp_tqm_remove_link_mgmt_queues(ab, dp_peer, hw_link_id);
	if (ret_mgmt) {
		ath12k_err(ab,
			   "Error: TQM Remove link MGMT queue peer %d link_id %d",
			   dp_peer->peer_id, hw_link_id);
		return ret_mgmt;
	}
	return 0;
}

static inline
int ath12k_dp_tqm_remove_queues_cmd(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u8 hw_link_id)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(ab->dp);
	struct ath12k_dp_tx_queue data;
	int ret_mcast = 0, ret_data = 0, ret_mgmt = 0, ret_sync = 0;

	ab = central_dp->ab;

	if (dp_peer->is_vdev_peer) {
		ret_mcast = ath12k_dp_tqm_remove_mcast_queues(ab, dp_peer);
		if (ret_mcast) {
			ath12k_err(ab,
				   "Error: TQM Remove MCAST queue peer %d ret=%d",
				   dp_peer->peer_id, ret_mcast);
			return ret_mcast;
		}
	} else {
		ret_data = ath12k_dp_tqm_remove_data_queues(ab, dp_peer);
		ret_mgmt = ath12k_dp_tqm_remove_mgmt_queues(ab, dp_peer);
		if (ret_data || ret_mgmt) {
			ath12k_err(
			ab,
			"Error: TQM Remove DATA/MGMT queue peer %d ret_data %d ret_mgmt %d",
			dp_peer->peer_id, ret_data, ret_mgmt);
			return ret_data ? ret_data : ret_mgmt;
		}
	}
	data.peer_id = dp_peer->peer_id;
	data.hw_link_id = hw_link_id;
	ret_sync = ath12k_dp_tqm_sync_remove_queues(ab, dp_peer, &data);
	if (ret_sync) {
		ath12k_err(ab, "Error: TQM SYNC peer %d ret=%d",
			   dp_peer->peer_id, ret_sync);
		return ret_sync;
	}
	return 0;
}

void ath12k_dp_peer_cleanup_indication(struct ath12k_dp *dp,
				       u16 peer_id,
				       u8 hw_link_id)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_dp_peer *dp_peer = NULL, *t;
	u8 pdev_id;
	int ret = 0;

	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "peer-cleanup-ind: ENTRY peer_id=%u hw_link_id=%u\n",
			 peer_id, hw_link_id);

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
					      dp_hw_grp->hw_links[hw_link_id].pdev_idx);
	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "peer-cleanup-ind: dp_pdev NULL for pdev_id=%u\n",
				 pdev_id);
		rcu_read_unlock();
		return;
	}

	dp_hw = dp_pdev->dp_hw;
	if (!dp_hw) {
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "peer-cleanup-ind: dp_hw NULL\n");
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	list_for_each_entry(t, &dp_hw->tqm_cleanup_list, tqm_cleanup_node) {
		if (t->peer_id == peer_id) {
			dp_peer = t;
			break;
		}
	}

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "peer-cleanup-ind: dp_peer NULL for peer_id=%u\n",
				 peer_id);
		rcu_read_unlock();
		return;
	}

	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "peer-cleanup-ind: found dp_peer %pM peer_id=%u peer_ext_ctx=%p\n",
			 dp_peer->addr, dp_peer->peer_id, dp_peer->peer_ext_ctx);

	tx_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_info) {
		if (dp_peer->is_sta_bss_peer && dp_peer->smd_exec_transferred) {
			/* SMD BSS Transition EXEC completed on this STA BSS
			 * peer: all TXQ queues were re-pointed to the target
			 * peer_id via tqm-update and peer_ext_ctx was NULLed.
			 * TQM no longer holds queues for this peer_id and will
			 * not generate a TQM_SYNC status response.
			 *
			 * Mark the peer as LOGICALLY_DELETED so that
			 * ath12k_wifi8_dp_peer_delete() frees it when the WMI
			 * peer delete response arrives.  Do NOT free here —
			 * dp_peer_delete() is the owner of the final kfree.
			 */
			ath12k_dbg(dp->ab, ATH12K_DBG_SMD | ATH12K_DBG_PEER,
				   "peer-cleanup-ind: SMD exec complete, skipping TQM_SYNC for dp_peer %pM id=%u\n",
				   dp_peer->addr, peer_id);
			dp_peer->dp_peer_state = ATH12K_DP_PEER_LOGICALLY_DELETED;
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			rcu_read_unlock();
			kref_put(&dp_peer->refcount,
				 ath12k_wifi8_dp_peer_tqm_sync_release);
			return;
		}

		/* SMD PREP in progress: peer_ext_ctx still set, EXEC has not
		 * yet run. Send TQM_SYNC to ensure all in-flight TQM
		 * completions drain before EXEC transfers ownership.
		 * tqm-cleanup-sync will find peer_ext_ctx NULL (NULLed by EXEC
		 * before it fires) and skip resource freeing.
		 */
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "peer-cleanup-ind: tx_info NULL (SMD case), sending TQM_SYNC only\n");

		ret = ath12k_dp_tqm_sync_remove_queues(dp->ab, dp_peer,
						       &(struct ath12k_dp_tx_queue){
							   .peer_id = dp_peer->peer_id,
							   .hw_link_id = hw_link_id
						       });
		if (ret) {
			ath12k_err(dp->ab,
				   "ERROR: TQM_SYNC failed for SMD peer %pM id=%d ret=%d\n",
				   dp_peer->addr, dp_peer->peer_id, ret);
		} else {
			ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
					 "peer-cleanup-ind: TQM_SYNC sent for SMD case\n");
		}

		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}

	/*
	 * SMD transition in progress — peer_ext_ctx still set on current peer.
	 *
	 * For SLO: ath12k_uhr_smd_activate_ext_ctx() returns early at EXEC
	 * resp time because target_ahsta->links_map == 0 (no links yet).
	 * exec_activate_links() is therefore deferred to the post-DL-drain
	 * peer_assoc call.  When the current AP peer is deleted before the
	 * target peer is added, cleanup_indication fires here with
	 * peer_ext_ctx still set — we must NOT free it.
	 *
	 * For MLO: exec_activate_links() runs during EXEC resp and NULLs
	 * peer_ext_ctx, so smd_parked_ext_ctx is already NULL by the time
	 * cleanup_indication fires.  This guard is a no-op for MLO.
	 *
	 * Detection: smd_parked_ext_ctx == dp_peer->peer_ext_ctx means the
	 * PREP-phase parking is still pending (exec_activate_links not yet
	 * called).  Detach peer_ext_ctx from the current peer without freeing
	 * it — the ext_ctx will be transferred to the target peer when
	 * exec_activate_links() runs post DL drain.  Send TQM_SYNC only.
	 */
	spin_lock_bh(&dp_hw_grp->smd_transition_lock);
	if (dp_hw_grp->smd_parked_ext_ctx &&
	    dp_hw_grp->smd_parked_ext_ctx == dp_peer->peer_ext_ctx) {
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "peer-cleanup-ind: SMD pending (SLO), detach ext_ctx %pM (id=%u)\n",
				 dp_peer->addr, dp_peer->peer_id);
		dp_peer->peer_ext_ctx = NULL;
		spin_unlock_bh(&dp_hw_grp->smd_transition_lock);

		ret = ath12k_dp_tqm_sync_remove_queues(dp->ab, dp_peer,
						       &(struct ath12k_dp_tx_queue){
							   .peer_id = dp_peer->peer_id,
							   .hw_link_id = hw_link_id
						       });
		if (ret)
			ath12k_err(dp->ab,
				   "ERROR: TQM_SYNC failed for SMD transition peer %pM id=%d ret=%d\n",
				   dp_peer->addr, dp_peer->peer_id, ret);
		else
			ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
					 "peer-cleanup-ind: TQM_SYNC sent for SMD transition (SLO) case\n");

		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}
	spin_unlock_bh(&dp_hw_grp->smd_transition_lock);

	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "peer-cleanup-ind: before clear_bit txq_links=0x%lx\n",
			 tx_info->txq_hw_links_bitmap);

	clear_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);

	ret = ath12k_dp_tqm_remove_mgmt_link_queues(dp->ab, dp_peer, hw_link_id);
	if (ret) {
		ath12k_err(dp->ab,
			   "ERROR: TQM REMOVE MGMT LINK QUEUE CMD peer %d link %d",
			   dp_peer->peer_id, hw_link_id);
	}
	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
			 "peer-cleanup-ind: after clear_bit txq_links=0x%lx\n",
			 tx_info->txq_hw_links_bitmap);

	/* Check whether event is for last link or not */
	if (tx_info->txq_hw_links_bitmap) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "peer-cleanup-ind: not last link, skipping TQM remove\n");
		rcu_read_unlock();
		return;
	}

	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "peer-cleanup-ind: last link, calling TQM remove queues\n");

	ret = ath12k_dp_tqm_remove_queues_cmd(dp->ab, dp_peer, hw_link_id);
	if (ret) {
		ath12k_err(dp->ab,
			   "ERROR: TQM REMOVE QUEUE CMD peer %d ret=%d",
			   dp_peer->peer_id, ret);
	} else {
		ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "peer-cleanup-ind: TQM remove queues cmd sent successfully\n");
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	rcu_read_unlock();
	ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "peer-cleanup-ind: EXIT peer_id=%u\n", peer_id);
}

void ath12k_wifi8_dp_link_peer_assoc(struct ath12k_dp_hw *dp_hw,
				     struct ath12k_dp *dp,
				     u8 *addr, u32 hw_link_id,
				     struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	u8 idx;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	/* During link addition if existing queues are re-used; reset tqm_send */
	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);
	idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
	sw_msduq_ptr = tx_flow_info->mgmt_msduq[idx];
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 0;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);

	ath12k_dp_tx_peer_msduq_mpduq_setup(dp->dp_hw_grp, dp_peer, hw_link_id);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

int ath12k_wifi8_dp_smd_prep_transfer_ext_ctx(struct ath12k_dp *dp,
					      struct ath12k_dp_peer *current_dp_peer,
					const u8 *target_mld_addr,
					u16 transitioning_links)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_peer_ext_ctx *ext_ctx;
	u16 old_ast_index;

	if (WARN_ON(!current_dp_peer || !current_dp_peer->peer_ext_ctx)) {
		ath12k_warn(dp->ab, "smd prep: current peer has no ext_ctx\n");
		return -EINVAL;
	}

	ext_ctx = current_dp_peer->peer_ext_ctx;
	old_ast_index = ext_ctx->ast_index;

	ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
		   "smd prep: parking ext_ctx %pM->%pM ast=%u links=0x%x\n",
		   current_dp_peer->addr, target_mld_addr,
		   old_ast_index, transitioning_links);

	/*
	 * Park ext_ctx reference in dp_hw_grp for EXEC phase.
	 *
	 * We do NOT detach ext_ctx from the current peer, delete the old AST
	 * entry, or clear ast_index/ast_hash here during PREP.  The STA still
	 * needs to communicate with the current AP (e.g. send ST Exec Request).
	 * These operations are deferred to EXEC phase (post ST Exec Response)
	 * in ath12k_wifi8_dp_smd_exec_activate_links():
	 *   - current_dp_peer->peer_ext_ctx is NULLed
	 *   - old AST entry (smd_parked_ext_ctx->ast_index) is deleted
	 *   - new AST entry is created for the target peer
	 *   - TQM UPDATE is sent to switch queues to the new peer_id
	 */
	spin_lock_bh(&dp_hw_grp->smd_transition_lock);
	dp_hw_grp->smd_parked_ext_ctx = ext_ctx;
	dp_hw_grp->smd_old_peer_id = current_dp_peer->peer_id;
	ether_addr_copy(dp_hw_grp->smd_target_mld_addr, target_mld_addr);
	spin_unlock_bh(&dp_hw_grp->smd_transition_lock);

	ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
		   "smd prep: ext_ctx parked %pM peer_id=%u ast_idx=%u (deferred to EXEC)\n",
		   target_mld_addr, current_dp_peer->peer_id, old_ast_index);

	return 0;
}

/**
 * ath12k_wifi8_dp_smd_abort_prep()
 *	Undo the ext_ctx sharing done during PREP.
 *
 * PREP phase did two things to ext_ctx:
 *   1. prep_transfer_ext_ctx: smd_parked_ext_ctx = SAP->peer_ext_ctx (parked ref)
 *   2. peer_assoc_smd_transition: TAP->peer_ext_ctx = smd_parked_ext_ctx (shared)
 *
 * The correct reverse:
 *   1. NULL the TAP peer's peer_ext_ctx: removes the shared reference.
 *   2. Clear smd_parked_ext_ctx and tracking state.
 *
 * After this, the SAP peer remains the sole owner of its peer_ext_ctx.
 * It will free ext_ctx naturally via peer-cleanup-ind -> tqm-cleanup-sync
 * when the SAP peer is deleted by sta_info_flush().
 *
 * Do NOT delete the AST, free TXQ memory, or kfree ext_ctx here, those
 * resources are still in active use by the SAP peer.
 */
void ath12k_wifi8_dp_smd_abort_prep(struct ath12k_dp *dp, struct ieee80211_vif *vif)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_peer_ext_ctx *ext_ctx;
	struct ath12k_dp_peer *target_dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	u8 target_mld_addr[ETH_ALEN];
	int pdev_idx;

	spin_lock_bh(&dp_hw_grp->smd_transition_lock);
	ext_ctx = dp_hw_grp->smd_parked_ext_ctx;
	if (!ext_ctx) {
		spin_unlock_bh(&dp_hw_grp->smd_transition_lock);
		return;
	}
	ether_addr_copy(target_mld_addr, dp_hw_grp->smd_target_mld_addr);

	/* Unpark: clear group-level tracking first. */
	dp_hw_grp->smd_parked_ext_ctx = NULL;
	dp_hw_grp->smd_old_peer_id = 0;
	eth_zero_addr(dp_hw_grp->smd_target_mld_addr);
	spin_unlock_bh(&dp_hw_grp->smd_transition_lock);

	ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
		   "smd abort prep: removing shared ext_ctx from target peer %pM\n",
		   target_mld_addr);

	/* Find the target (TAP) MLD BSS peer across all pdevs and NULL its
	 * peer_ext_ctx.  This removes the shared reference — the ext_ctx
	 * remains owned by the SAP peer which will free it on peer delete.
	 * The target peer may not exist yet (e.g. if PREP failed before
	 * peer_assoc ran) — silently skip in that case.
	 */
	rcu_read_lock();
	for (pdev_idx = 0; pdev_idx < dp->ab->num_radios; pdev_idx++) {
		dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_idx);
		if (!dp_pdev || !dp_pdev->dp_hw)
			continue;

		dp_hw = dp_pdev->dp_hw;
		spin_lock_bh(&dp_hw->peer_hash_lock);
		target_dp_peer = ath12k_dp_peer_find_by_addr(dp_hw,
							     target_mld_addr, vif);
		if (target_dp_peer) {
			if (WARN_ONCE(target_dp_peer->peer_ext_ctx != ext_ctx,
				      "smd abort prep: target peer %pM peer_ext_ctx mismatch (got %p, expected %p)\n",
				      target_mld_addr,
				      target_dp_peer->peer_ext_ctx, ext_ctx)) {
				spin_unlock_bh(&dp_hw->peer_hash_lock);
				break;
			}
			target_dp_peer->peer_ext_ctx = NULL;
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
				   "smd abort prep: NULLed peer_ext_ctx on target %pM pdev=%d\n",
				   target_mld_addr, pdev_idx);
			break;
		}
		spin_unlock_bh(&dp_hw->peer_hash_lock);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_wifi8_dp_smd_abort_prep);

void ath12k_wifi8_dp_smd_update_msduq_chip_status_via_tqm(struct ath12k_base *ab,
					struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_dp_tx_flow_info *tx_info,
					u16 peer_id,
					u8 bitmap)
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	int i, j, ret;
	int n_msduq = 0;

	if (!bitmap) {
		ath12k_dbg(ab, ATH12K_DBG_SMD,
			   "smd tqm-update-chip-req: bitmap=0 for peer_id=%u, skipping\n",
			   peer_id);
		return;
	}

	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			if (!tx_info->tid_info[i].msduq[j])
				continue;

			memset(&cmd, 0, sizeof(cmd));
			cmd.std.peer_id = peer_id;
			cmd.update_tx_msdu_params.msdu_q_paddr =
				tx_info->tid_info[i].msduq[j]->msdu_q_paddr;
			cmd.update_tx_msdu_params.bitmap = bitmap;

			ath12k_dbg(ab, ATH12K_DBG_SMD,
				   "smd tqm-update-chip-req msduq: tid=%d q=%d flow=0x%x bitmap=0x%x\n",
				   i, j,
				   tx_info->tid_info[i].msduq[j]->queue_number,
				   bitmap);

			ret = ath12k_wifi8_dp_tqm_cmd_send(ab,
							   HAL_TQM_UPDATE_MSDUQ_BO,
						&cmd, &(struct ath12k_dp_tx_queue){
							.peer_id = peer_id,
							.hw_link_id = 0
						}, ath12k_dp_tqm_update_completion);
			if (!ret)
				n_msduq++;
		}
	}
	ath12k_dbg(ab, ATH12K_DBG_SMD,
		   "smd tqm-update-chip-req: completed %d msduq updated bitmap=0x%x\n",
		   n_msduq, bitmap);
}

void ath12k_wifi8_dp_smd_exec_activate_links(struct ath12k_dp *dp,
					     struct ath12k_dp_hw *dp_hw,
					     struct ath12k_dp_vif *dp_vif,
					     const u8 *addr,
					     u16 active_links)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_peer *current_dp_peer = NULL;
	struct ath12k_dp_peer *target_dp_peer;
	struct ath12k_dp_peer_ext_ctx *ext_ctx;
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_dp *cumac_dp;
	struct ath12k_base *tqm_ab;
	bool target_is_sta_bss_peer;
	u8 target_addr[ETH_ALEN];
	u16 old_ast_index = 0;
	u16 old_peer_id = 0;
	u16 target_peer_id;
	u8 chip_bitmap;
	int ret;
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);

	spin_lock_bh(&dp_hw->peer_hash_lock);
	target_dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, ahvif->vif);
	if (!target_dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		WARN_ONCE(1,
			  "smd exec_activate_links: peer %pM not found, target peer deleted between PREP and EXEC\n",
			  addr);
		return;
	}

	if (!target_dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		/* peer_ext_ctx must be populated by PREP before EXEC fires.
		 * For MLO STA BSS peers it carries TX flow state required for
		 * AST/TQM migration. For SLO the DL drain phase handles the
		 * full transition so exec_activate_links is not expected to run
		 * with a NULL peer_ext_ctx in either case.
		 * If we reach here, PREP's ext_ctx transfer failed and the
		 * transition should have been aborted at that point.
		 */
		WARN_ONCE(1,
			  "smd exec_activate_links: peer %pM missing ext_ctx PREP transfer failure should have aborted transition\n",
			  addr);
		return;
	}

	/* ext_ctx and the scalar copies below are captured under peer_hash_lock.
	 * peer_hash_lock cannot span the smd_transition_lock section below
	 * because that section re-acquires peer_hash_lock to NULL the old
	 * peer's ext_ctx, which would self-deadlock.  The
	 * smd_parked_ext_ctx == ext_ctx check inside smd_transition_lock
	 * re-validates ext_ctx before any dereference.
	 */
	ext_ctx = target_dp_peer->peer_ext_ctx;
	tx_info = &ext_ctx->tx_flow_info;
	target_peer_id = target_dp_peer->peer_id;
	ether_addr_copy(target_addr, target_dp_peer->addr);
	target_is_sta_bss_peer = target_dp_peer->is_sta_bss_peer;
	chip_bitmap = ath12k_dp_get_chipid_bitmap(dp_hw_grp, target_dp_peer);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	/*
	 * EXEC phase: complete deferred cleanup from PREP phase.
	 *
	 * If smd_parked_ext_ctx matches this ext_ctx, a PREP-phase transfer
	 * is pending.  We now:
	 *   1. NULL current peer's peer_ext_ctx (ownership transferred)
	 *   2. Delete old AST entry (current AP's AST)
	 *   3. Create new AST entry for target peer
	 *   4. TQM UPDATE: old_peer_id → target peer_id
	 */
	spin_lock_bh(&dp_hw_grp->smd_transition_lock);
	if (dp_hw_grp->smd_parked_ext_ctx == ext_ctx) {
		old_peer_id = dp_hw_grp->smd_old_peer_id;
		old_ast_index = ext_ctx->ast_index;
		dp_hw_grp->smd_parked_ext_ctx = NULL;
		eth_zero_addr(dp_hw_grp->smd_target_mld_addr);
		dp_hw_grp->smd_old_peer_id = 0;
		dp_hw_grp->smd_exec_in_progress = false;
	}
	spin_unlock_bh(&dp_hw_grp->smd_transition_lock);

	if (old_peer_id) {
		/* 1. NULL current peer's peer_ext_ctx */
		spin_lock_bh(&dp_hw->peer_hash_lock);
		if (old_peer_id < MAX_DP_PEER_LIST_SIZE)
			current_dp_peer = rcu_dereference_protected(
				dp_hw->dp_peer_list[old_peer_id],
				lockdep_is_held(&dp_hw->peer_hash_lock));
		if (current_dp_peer &&
		    current_dp_peer->peer_ext_ctx == ext_ctx) {
			current_dp_peer->peer_ext_ctx = NULL;
			current_dp_peer->smd_exec_transferred = true;
			ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
				   "smd exec: NULLed ext_ctx on current peer id=%u\n",
				   old_peer_id);
		}
		spin_unlock_bh(&dp_hw->peer_hash_lock);

		/* 2. Delete old AST entry */
		if (old_ast_index) {
			ath12k_dp_ast_entry_delete(dp_hw_grp, old_ast_index);
			ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
				   "smd exec: deleted old AST index=%u\n",
				   old_ast_index);
		}

		/* 3. Create new AST entry for target peer */
		ether_addr_copy(ast_param.mac_addr, target_addr);
		ast_param.peer_id = target_peer_id;
		ast_param.tx_classify_info_paddr =
			ext_ctx->tx_flow_info.hw_who_classify_info_paddr;
		ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_USE_ADDRX;
		ret = ath12k_dp_ast_entry_create(dp_hw_grp, &ast_param);
		if (ret) {
			ath12k_warn(dp->ab,
				    "smd exec: AST create failed for %pM ret=%d\n",
				    target_addr, ret);
		} else {
			ext_ctx->ast_index = ast_param.ast_index;
			ext_ctx->ast_hash  = ast_param.ast_hash;
			if (dp_vif && target_is_sta_bss_peer) {
				dp_vif->ast_idx  = ast_param.ast_index;
				dp_vif->ast_hash = ast_param.ast_hash;
			}
			ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
				   "smd exec: new AST index=%u hash=%u for %pM\n",
				   ast_param.ast_index, ast_param.ast_hash,
				   target_addr);
		}

		/* 4. TQM UPDATE: old_peer_id → target peer_id */
		cumac_dp = ath12k_get_central_dp(dp);
		tqm_ab = cumac_dp ? cumac_dp->ab : dp->ab;
		ath12k_wifi8_dp_smd_reset_tx_queue_states(
			tqm_ab, tx_info, target_peer_id);
		ath12k_wifi8_dp_smd_update_queue_peer_id_via_tqm(
			tqm_ab, dp_hw_grp, tx_info,
			target_peer_id, old_peer_id, chip_bitmap);
	}

	/* Activate TX queues for the new links */
	spin_lock_bh(&tx_info->tx_q_lock);
	tx_info->assoc_hw_links_bitmap |= active_links;
	tx_info->txq_hw_links_bitmap   |= active_links;
	spin_unlock_bh(&tx_info->tx_q_lock);

	/* Refresh tqm_status_required_for_chipX on every exec_activate_links call,
	 * including partner-radio invocations (old_peer_id=0), so the chip status
	 * bits are always in sync with the peer's current link configuration.
	 */
	cumac_dp = ath12k_get_central_dp(dp);
	tqm_ab = cumac_dp ? cumac_dp->ab : dp->ab;
	ath12k_wifi8_dp_smd_update_msduq_chip_status_via_tqm(
		tqm_ab, dp_hw_grp, tx_info, target_peer_id, chip_bitmap);

	ath12k_dbg(dp->ab, ATH12K_DBG_SMD,
		   "smd exec: activated links=0x%x for %pM assoc=0x%lx txq=0x%lx\n",
		   active_links, target_addr,
		   tx_info->assoc_hw_links_bitmap,
		   tx_info->txq_hw_links_bitmap);
}

int ath12k_wifi8_get_mgmt_flowq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr,
				struct peer_assoc_flowq_params *flowq_params,
				struct ieee80211_vif *vif)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct peer_assoc_msduq_params *msduq_params;
	struct peer_assoc_mpduq_params *mpduq_params;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	enum htt_tx_tid_msduq_mpdu_type msduq_type;
	u64 dma_addr = 0;
	u8 hw_link_id;
	int ret = 0;
	u8 idx;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	flowq_params->num_links = 0;
	rcu_read_lock();
	if (dp_peer->is_mlo) {
		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (!tx_info->mgmt_msduq[ATH12K_LINK_TO_MGMT_TYPE(hw_link_id)])
				continue;
			idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
			msduq_type = ATH12K_LINK_TO_MSDUQ_TYPE(hw_link_id);
			dma_addr = (u64)tx_info->mgmt_msduq[idx]->msdu_q_paddr;
			msduq_params =
				&flowq_params->msduq_params[flowq_params->num_links];
			msduq_params->mgmt_msduq_address =
					(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
			msduq_params->flow_type = msduq_type;
			msduq_params->link_id = hw_link_id;
			flowq_params->num_links++;
		}

		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]) {
			ret = -ENOENT;
			goto exit;
		}
		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = HTT_TID_MSDUQ_MGMT_LINK_AGNOSTIC;
		flowq_params->num_links++;
	} else {
		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]) {
			ret = -ENOENT;
			goto exit;
		}

		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (link_peer)
				break;
		}

		if (!link_peer) {
			ret = -ENOENT;
			goto exit;
		}

		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_0;
		msduq_params->link_id = link_peer->hw_link_id;
		flowq_params->num_links++;
	}

	if (tx_info->mgmt_mpduq) {
		dma_addr = (u64)tx_info->mgmt_mpduq->mpdu_q_paddr;
		mpduq_params = &flowq_params->mpduq_params;
		mpduq_params->mgmt_mpduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		dma_addr = (u64)tx_info->mgmt_mpduq->pn_addr;
		mpduq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
		mpduq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	}
	flowq_params->enabled = 1;

exit:
	rcu_read_unlock();
	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

int ath12k_wifi8_get_holq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			  u8 *addr, struct peer_assoc_holq_params *holq_params,
			  struct ieee80211_vif *vif)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_dp_peer *dp_peer;
	u64 dma_addr = 0;
	int ret = 0;
	u8 holq_tid;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	holq_tid = tx_info->holq_tid;
	if (!tx_info->hol_msduq || !tx_info->tid_info[holq_tid].mpduq) {
		spin_unlock_bh(&tx_info->tx_q_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	holq_params->peer_id = tx_info->hol_msduq->flow_info.peer_id;
	holq_params->tid = tx_info->hol_msduq->flow_info.tid_num;
	holq_params->mpdu_type =
		tx_info->tid_info[holq_tid].mpduq->flow_info.flow_type;
	holq_params->msdu_type = tx_info->hol_msduq->flow_info.flow_type;

	dma_addr = (u64)tx_info->tid_info[holq_tid].mpduq->mpdu_q_paddr;
	holq_params->mpduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->hol_msduq->msdu_q_paddr;
	holq_params->msduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->tid_info[holq_tid].mpduq->pn_addr;
	holq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
	holq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	holq_params->enabled = 1;

	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

int ath12k_wifi8_dp_get_peer_init_status(struct ath12k_dp *dp,
					 struct ath12k_dp_hw *dp_hw,
					 u8 *addr)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!(ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp) ||
	      ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp)))
		return 0;

	if (!wait_for_completion_timeout(&dp_hw_grp_wifi8->peer_init_done, 1 * HZ)) {
		ath12k_wifi8_invalidate_peer_ase_cache_table(dp_hw_grp);
		ath12k_warn(dp->ab, "peer init is not completed for %pM", addr);
		return -ETIMEDOUT;
	}

	return 0;
}

void ath12k_wifi8_dp_vif_update_4addr(struct ath12k_dp_hw *dp_hw,
				      struct ath12k_dp_vif *dp_vif,
				      struct ieee80211_sta *sta)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, sta->addr, ahsta->ahvif->vif);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "unable for find peer/peer_ext_ctx for mac addr in set 4 addr %pM",
				 sta->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	dp_vif->is_wds_4addr = true;
	dp_vif->ast_idx = dp_peer->peer_ext_ctx->ast_index;
	dp_vif->ast_hash = dp_peer->peer_ext_ctx->ast_hash;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

void ath12k_wifi8_dp_assoc_link_update(struct ath12k_dp *dp,
				       struct ath12k_hw *ah,
				       struct ieee80211_sta *sta,
				       struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_sta *ahsta;
	u8 assoc_link_id;
	struct hal_txpt_classify_data ti = {0};
	struct ath12k_dp_msdu_q_info *msdu_flow_ptr = NULL;
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	dma_addr_t txpt_paddr;
	u64 msdu_flow_dma_ptr = 0;
	u8 tid_num, q;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, sta->addr, vif);

	if (!dp_peer || !dp_peer->sta) {
		ath12k_err(dp->ab, "peer or peer sta is null");
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}
	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(dp->ab, "tx_flow_info is null for %pM", dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	for (tid_num = 0; tid_num < ATH12K_MAX_NUM_DATA_TIDS; tid_num++) {
		for (q = 0; q < ATH12K_MAX_DP_MSDUQ_PER_TID; q++) {
			msdu_flow_ptr = tx_flow_info->tid_info[tid_num].msduq[q];
			if (!msdu_flow_ptr)
				continue;

			tx_tid_ptr = ath12k_get_txpt_info_ptr(
					dp_peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));
			txpt_paddr = ath12k_get_txpt_paddr(
					dp_peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));

			if (!tx_tid_ptr || txpt_paddr == 0)
				continue;

			msdu_flow_dma_ptr = (u64)msdu_flow_ptr->msdu_q_paddr;
			ti.msdu_paddr = (u32)((msdu_flow_dma_ptr >> 0x8) & 0xFFFFFFFF);
			ti.paddr = txpt_paddr;
			ti.flow_handler = HAL_WIFITXPT_TO_TQM;
			ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
			ti.msdu_drop = 0;
			ti.metadata = dp_peer ? dp_peer->peer_id : HAL_INVALID_PEERID;

			rcu_read_lock();
			ahsta = ath12k_sta_to_ahsta(dp_peer->sta);
			assoc_link_id = dp_peer->sta->mlo ?
					ahsta->assoc_link_id :
					ahsta->deflink.link_id;
			ti.assoc_link_id =
			ath12k_dp_peer_convert_logical_to_hw_link_id(dp_peer,
								     assoc_link_id);
			if (ti.assoc_link_id == ATH12K_INVALID_HW_LINKID) {
				rcu_read_unlock();
				goto end;
			}
			rcu_read_unlock();
			dp_peer->assoc_hw_link_id = ti.assoc_link_id;

			ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp,
								  tx_tid_ptr, &ti);
		}
	}

end:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
bool ath12k_wifi8_dp_peer_ast_param_get(struct ath12k_hw *ah, struct ieee80211_vif *vif,
					u16 *ast_info,
					u16 *hw_peer_id,
					u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "Invalid peer - ast param get failed\n");
		return false;
	}

	peer_ext_ctx = dp_peer->peer_ext_ctx;
	if (!peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "Invalid peer ext ctx for peer_id:%d\n",
				 dp_peer->peer_id);
		return false;
	}

	/*
	 * Populate AST information with index[15:4] and AST hash[3:0]
	 */
	*ast_info = (peer_ext_ctx->ast_index << ATH12K_AST_INDEX_SHIFT) |
		(peer_ext_ctx->ast_hash & ATH12K_AST_HASH_MASK);
	*hw_peer_id = dp_peer->peer_id;

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "Peer param pid:%u ast_idx:%u ast_hash:%u\n",
			 dp_peer->peer_id, peer_ext_ctx->ast_index,
			 peer_ext_ctx->ast_hash);

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return true;
}
#endif
