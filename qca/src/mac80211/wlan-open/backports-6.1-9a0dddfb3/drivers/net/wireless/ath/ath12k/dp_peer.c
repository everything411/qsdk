// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp_peer.h"
#include "debug.h"
#include "debugfs.h"
#include "dp_stats.h"
#include "telemetry_agent_if.h"
#include "mac.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "sdwf.h"
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_QCN_EXTN */
#endif
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa_pub.h"
#endif /* CPTCFG_QCN_EXTN */

u16 ath12k_dp_get_peer_based_tcl_metadata(struct ath12k_dp *dp, u16 peer_id,
					  u8 valid_htt_ext)
{
	u16 metadata = 0;

	if (dp->tcl_metadata_ver == HTT_OPTION_TCL_METADATA_VER_V3) {
		metadata = u32_encode_bits(0, HTT_TCL_META_DATA_TYPE_V3) |
			   u32_encode_bits(peer_id, HTT_TCL_META_DATA_PEER_ID_V3) |
			   u32_encode_bits(valid_htt_ext, HTT_TCL_META_DATA_VALID_HTT_V3);

		return metadata;
	}

	metadata = u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
		   u32_encode_bits(peer_id, HTT_TCL_META_DATA_PEER_ID) |
		   u32_encode_bits(valid_htt_ext, HTT_TCL_META_DATA_VALID_HTT);

	return metadata;
}

u16 ath12k_dp_link_peer_get_peer_id(struct ath12k_base *ab,
				    struct ath12k_dp_link_peer *link_peer)
{
	if (!ab || !ab->dp || !link_peer)
		return ATH12K_PEER_ID_INVALID;

	if (ab->dp->global_peer_id_supported) {
		if (!link_peer->dp_peer)
			return ATH12K_PEER_ID_INVALID;
		return link_peer->dp_peer->peer_id;
	}

	return link_peer->peer_id;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_get_peer_id);

static void __ath12k_link_peer_free(struct ath12k_dp_link_peer *peer)
{
	ath12k_dp_peer_link_stats_free(peer);

	kfree(peer);
}

void ath12k_link_peer_free(struct ath12k_dp_link_peer *peer)
{
	if (!peer)
		return;

	list_del(&peer->list);

	__ath12k_link_peer_free(peer);
}
EXPORT_SYMBOL(ath12k_link_peer_free);

void ath12k_peer_unmap_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			     u8 *mac_addr, bool is_wds)
{
	if (is_wds) {
		ath12k_dp_ipa_peer_unmap_event_wds(ab, vdev_id, peer_id, mac_addr);
		return;
	}
}

void ath12k_peer_map_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id, bool is_wds)
{
	struct ath12k *ar;
	struct ath12k_peer_map_pending_event *resp;

	if (is_wds) {
		ath12k_dp_ipa_peer_map_event_wds(ab, vdev_id, peer_id, mac_addr);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (ar) {
		if (ar->peer_map_event.pending_peer_vdev_id == vdev_id &&
		    ether_addr_equal(ar->peer_map_event.pending_peer_addr, mac_addr)) {
			/* Fill response structure - NO PEER ALLOCATION */
			resp = &ar->peer_map_event;
			resp->peer_id = peer_id;
			resp->ast_hash = ast_hash;
			resp->hw_peer_id = hw_peer_id;
			resp->received = true;

			wake_up(&ab->peer_mapping_wq);
			rcu_read_unlock();
			return;
		}
	}

	rcu_read_unlock();
	ath12k_warn(ab, "unexpected peer map event for %pM\n", mac_addr);
}

void ath12k_dp_link_peer_htbl_add(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_link_peer *peer)
{
	u32 hash;
	struct ath12k_dp *dp = dp_pdev->dp;

	lockdep_assert_held(&dp->dp_lock);

	hash = jhash(peer->addr, ETH_ALEN, 0);
	hash_add(dp->link_peer_htbl, &peer->hash_addr_node, hash);
}

void ath12k_dp_link_peer_htbl_delete(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_link_peer *peer)
{
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	hash_del(&peer->hash_addr_node);
}

/**
 * ath12k_dp_link_peer_htbl_find_by_addr - Find link peer by MAC address only
 * @dp: DP context whose link_peer_htbl is searched
 * @addr: MAC address to search for
 *
 * Unlike ath12k_dp_link_peer_find_by_mac_addr(), this does not require an
 * ath12k_dp_peer context and does not filter by hw_link_id. Caller must hold
 * dp->dp_lock.
 *
 * Returns: Pointer to link peer if found, NULL otherwise
 */
struct ath12k_dp_link_peer *
ath12k_dp_link_peer_htbl_find_by_addr(struct ath12k_dp *dp, const u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	u32 hash;

	lockdep_assert_held(&dp->dp_lock);

	hash = jhash(addr, ETH_ALEN, 0);

	hash_for_each_possible(dp->link_peer_htbl, peer, hash_addr_node, hash) {
		if (ether_addr_equal(peer->addr, addr))
			return peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_htbl_find_by_addr);

/**
 * ath12k_dp_peer_addr_hash - Compute hash value for MAC address
 * @addr: MAC address (6 bytes)
 *
 * Uses jhash to compute hash value from MAC address.
 *
 * Returns: 32-bit hash value
 */
static inline u32 ath12k_dp_peer_addr_hash(const u8 *addr)
{
	/* Use jhash for good distribution */
	return jhash(addr, ETH_ALEN, 0);
}

/**
 * ath12k_dp_peer_hash_table_add - Add peer to hash table
 * @dp_hw: DP hardware context
 * @dp_peer: Peer to add
 *
 * Adds peer to hash table using MAC address as key.
 * Caller must hold peer_hash_lock.
 */
void ath12k_dp_peer_hash_table_add(struct ath12k_dp_hw *dp_hw,
				   struct ath12k_dp_peer *dp_peer)
{
	u32 hash;

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash = ath12k_dp_peer_addr_hash(dp_peer->addr);
	hash_add(dp_hw->peer_hash, &dp_peer->hash_node, hash);
}
EXPORT_SYMBOL(ath12k_dp_peer_hash_table_add);

/**
 * ath12k_dp_peer_hash_table_delete - Remove peer from hash table
 * @dp_hw: DP hardware context
 * @dp_peer: Peer to remove
 *
 * Removes peer from hash table.
 * Caller must hold peer_hash_lock.
 */
void ath12k_dp_peer_hash_table_delete(struct ath12k_dp_hw *dp_hw,
				      struct ath12k_dp_peer *dp_peer)
{
	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash_del(&dp_peer->hash_node);
}
EXPORT_SYMBOL(ath12k_dp_peer_hash_table_delete);

/**
 * ath12k_dp_peer_find_by_addr - Find peer by MAC address (fast)
 * @dp_hw: DP hardware context
 * @addr: MAC address to search for
 * @vif: ieee80211_vif ptr
 *
 * Fast O(1) average-case hash table lookup for peer by MAC address.
 * Caller must hold peer_hash_lock.
 *
 * Returns: Pointer to peer if found, NULL otherwise
 */
struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr(struct ath12k_dp_hw *dp_hw,
						   const u8 *addr,
						   struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	u32 hash;

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash = ath12k_dp_peer_addr_hash(addr);

	hash_for_each_possible(dp_hw->peer_hash, dp_peer, hash_node, hash) {
		if (ether_addr_equal(dp_peer->addr, addr) &&
		    (ath12k_dp_peer_get_vif(dp_peer) == vif))
			return dp_peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_find_by_addr);

struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr_and_sta(struct ath12k_dp_hw *dp_hw, u8 *addr,
							   struct ieee80211_sta *sta)
{
	struct ath12k_dp_peer *dp_peer;
	u32 hash;

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash = ath12k_dp_peer_addr_hash(addr);

	hash_for_each_possible(dp_hw->peer_hash, dp_peer, hash_node, hash) {
		if (ether_addr_equal(dp_peer->addr, addr) &&
		    (ath12k_dp_peer_get_sta(dp_peer) == sta))
			return dp_peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_find_by_addr_and_sta);

struct ath12k_dp_peer *ath12k_dp_vdev_peer_check(struct ath12k_dp_hw *dp_hw,
						 u8 *addr, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	u32 hash;

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash = ath12k_dp_peer_addr_hash(addr);

	hash_for_each_possible(dp_hw->peer_hash, dp_peer, hash_node, hash) {
		if (ether_addr_equal(dp_peer->addr, addr) &&
		    (dp_peer->hw_link_id == hw_link_id ||
		     !dp_peer->is_vdev_peer))
			return dp_peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_vdev_peer_check);

struct ath12k_dp_peer *ath12k_dp_peer_create_find(struct ath12k_dp_hw *dp_hw, u8 *addr,
						  struct ieee80211_sta *sta,
						  bool mlo_peer)
{
	struct ath12k_dp_peer *dp_peer;
	u32 hash;

	lockdep_assert_held(&dp_hw->peer_hash_lock);

	hash = ath12k_dp_peer_addr_hash(addr);

	hash_for_each_possible(dp_hw->peer_hash, dp_peer, hash_node, hash) {
		if (ether_addr_equal(dp_peer->addr, addr)) {
			if (!sta || mlo_peer || dp_peer->is_mlo ||
			    ath12k_dp_peer_get_sta(dp_peer) == sta)
				return dp_peer;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_create_find);

struct ath12k_dp_peer *ath12k_dp_peer_find_by_peerid_index(struct ath12k_dp *dp,
							   struct ath12k_pdev_dp *dp_pdev,
							   u16 peer_id)
{
	struct ath12k_dp_peer *dp_peer = NULL;
	u16 index;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp peer find by peerid index called without rcu lock");

	if (peer_id >= ATH12K_PEER_ID_INVALID)
		return NULL;

	index = ath12k_dp_peer_get_peerid_index(dp, peer_id);

	dp_peer = rcu_dereference(dp_pdev->dp_hw->dp_peer_list[index]);

	if (dp_peer && (dp_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED))
		return dp_peer;

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_find_by_peerid_index);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_peerid_index(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev, u16 peer_id)
{
	u8 i;
	struct ath12k_pdev_dp *temp_dp_pdev;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp link peer find by peerid index called without rcu lock");

	/* Validate peer_id */
	if (peer_id == ATH12K_PEER_ID_INVALID)
		return NULL;

	if (dp_pdev && dp_pdev->hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS)
		return NULL;

	if (dp_pdev) {
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
		if (!dp_peer)
			return NULL;

		return rcu_dereference(dp_peer->link_peers[dp_pdev->hw_link_id]);
	} else {
		/* Iterate through all dp_pdevs and try to find the peer */
		for (i = 0; i < dp->num_radios; i++) {
			temp_dp_pdev = ath12k_dp_to_dp_pdev(dp, i);
			if (!temp_dp_pdev)
				continue;

			link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp,
									     temp_dp_pdev,
									     peer_id);

			/* Peer_id comparison is required for split-phy mode */
			if (link_peer && link_peer->peer_id == peer_id)
				return link_peer;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_peerid_index);

u16 ath12k_dp_peer_get_peer_id(struct ath12k_dp_hw *dp_hw, struct ieee80211_vif *vif,
			       u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	int peer_id = ATH12K_MLO_PEER_ID_INVALID;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return ATH12K_MLO_PEER_ID_INVALID;
	}

	peer_id = dp_peer->peer_id;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return peer_id;
}

u16 ath12k_dp_peer_get_sta_id(struct ath12k_dp_hw *dp_hw, struct ieee80211_vif *vif,
			      u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	int sta_id = ATH12K_STA_ID_INVALID;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return ATH12K_STA_ID_INVALID;
	}

	sta_id = dp_peer->sta_id;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return sta_id;
}

int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       struct ieee80211_sta *sta, u8 *addr, u8 link_id,
			       struct ieee80211_vif *vif, u8 vp_type, int vp_num,
			       bool mlo_bridge_peer)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_link_peer *peer;
	u16 peerid_index;
	int ret;
	u8 *dp_peer_mac = !sta ? addr : sta->addr;
	bool is_vdev_peer = false;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	u8 hw_link_id = ar->hw_link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_link_vif *arvif;

	peer = kzalloc(sizeof(*peer), GFP_KERNEL);
	if (!peer)
		return -ENOMEM;

	peer->dp = dp;
	peer->pdev_idx = ar->pdev_idx;
	peer->is_bridge_peer = mlo_bridge_peer;
	peer->vdev_id = vdev_id;
	peer->peer_id = ar->peer_map_event.peer_id;
	peer->ast_hash = ar->peer_map_event.ast_hash;
	peer->hw_peer_id = ar->peer_map_event.hw_peer_id;
	ether_addr_copy(peer->addr, addr);
	ewma_avg_rssi_init(&peer->avg_rssi);
	ewma_avg_ack_rssi_init(&peer->peer_stats.avg_ack_rssi);
	ewma_avg_snr_init(&peer->signal_stats.avg_snr);
	ewma_avg_snr_dp_init(&peer->signal_stats.avg_snr_dp);
	ewma_avg_rssi_init(&peer->signal_stats.avg_rssi);
	ewma_avg_rssi_dp_init(&peer->signal_stats.avg_rssi_dp);
	/* Initialize generic event mechanism (FR_RSSI)
	 * Note: llist_node does not need explicit initialization.
	 * The llist_add() operation will handle node linkage automatically.
	 */
	peer->event.common.callback = ath12k_mac_peer_event_callback;
	atomic_set(&peer->event.common.flags, 0);

	/* Initialize peer event context */
	peer->event.peer_id = peer->peer_id;

	/* Initialize RSSI monitoring structure */
	peer->rssi_mon.last_rssi = 0;
	peer->rssi_mon.low_rssi_count = 0;
	peer->rssi_mon.first_low_jiffies = 0;
	peer->rssi_mon.cfg = NULL;  /* Will be set during peer assignment */

	peer->max_rssi = S8_MIN;
	peer->min_rssi = S8_MAX;
	spin_lock_init(&peer->ppdu_stats_lock);

	if (vif->type == NL80211_IFTYPE_STATION) {
		dp_link_vif->ast_hash = peer->ast_hash;
		dp_link_vif->ast_idx = peer->hw_peer_id;
	}

	if (!sta)
		is_vdev_peer = true;


	spin_lock_bh(&dp_hw->peer_hash_lock);
	spin_lock_bh(&dp->dp_lock);

	if (!is_vdev_peer)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, dp_peer_mac, sta);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, dp_peer_mac, vif);

	if (!dp_peer) {
		ret = -ENOENT;
		goto err_dp_peer;
	}

	peer->link_id = link_id;
	peer->dp_peer = dp_peer;

	ath12k_dp_arch_link_peer_assign_id(dp, ar, peer);
	peer->hw_link_id = hw_link_id;
	peer->event.common.hw_link_id = hw_link_id;
	peer->tcl_metadata |= ath12k_dp_get_peer_based_tcl_metadata(dp, peer->peer_id, 0);

	ret = ath12k_dp_peer_link_stats_alloc(peer, dp_pdev);
	if (ret)
		goto err_dp_peer;

	dp_peer->qos_stats_lvl = (ar->dp.qos_stats &
				  ATH12K_QOS_STATS_COLLECTION_MASK) >> 2;

	/* Store logical link_id to hw_link_id mapping */
	dp_peer->l2h_link_map[peer->link_id] = peer->hw_link_id;

	/* Store hw_link_id to logical link id mapping */
	dp_peer->hw_links[peer->hw_link_id] = peer->link_id;

	if (vif->type == NL80211_IFTYPE_AP) {
		dp_peer->is_reset_mcbc = true;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
		if (QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(ahvif->vap_submode))
			dp_peer->is_mmesh_peer = true;
#endif
	} else if (vif->type == NL80211_IFTYPE_MESH_POINT) {
		dp_peer->is_11s_mesh_peer = true;
	}

	/* Do not deliver frames to PPE in fast rx incase of RFS
	 * RFS is supported only in SFE Mode
	 */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (vp_type == PPE_VP_USER_TYPE_ACTIVE || vp_type == PPE_VP_USER_TYPE_DS)
		dp_peer->ppe_vp_num = vp_num;
#endif

	if (peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
		peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], dp_peer);
		/* Set peer_id in dp_peer for non-mlo client, peer_id for mlo client is
		 * set during dp_peer create
		 */
		if (!dp_peer->is_mlo) {
			dp_peer->peer_id = peer->peer_id;
			if (!is_vdev_peer)
				ahsta->dp_peer_id = peer->peer_id;
		}
	}

	if (!dp_peer->is_vdev_peer) {
		dp_peer->peer_links_map |= BIT(peer->hw_link_id);
		/* if this is the first link, assign primary link early for EPP peers */
		if (dp_peer->is_epp_peer && hweight32(dp_peer->peer_links_map) == 1) {
			peer->primary_link = true;
			rcu_read_lock();
			arvif = rcu_dereference(ahvif->link[link_id]);
			arvif->primary_sta_link = true;
			rcu_read_unlock();
		}
	}

	if (ath12k_proto_stats_enabled(dp_pdev) && dp_peer->peer_links_map)
		ath12k_dp_alloc_proto_stats_vif(&ahvif->dp_vif);

	rcu_assign_pointer(dp_peer->link_peers[peer->hw_link_id], peer);

	/* Fill ML info into created peer */
	if (dp_peer->is_mlo) {
		peer->ml_id = dp_peer->peer_id;
		ether_addr_copy(peer->ml_addr, dp_peer->addr);
		peer->mlo = true;
	} else {
		peer->ml_id = ATH12K_MLO_PEER_ID_INVALID;
		peer->mlo = false;
	}

	/* Cache config pointer for fast data path access
	 * Prefer per-link configuration when available; fallback to deflink.
	 */
	if (ath12k_dp_link_peer_get_sta(peer)) {
		struct ath12k_sta *ahsta =
			ath12k_sta_to_ahsta(ath12k_dp_link_peer_get_sta(peer));
		struct ath12k_link_sta *arsta = NULL;
		struct ath12k_link_vif *arvif;

		rcu_read_lock();
		if (link_id < IEEE80211_MLD_MAX_NUM_LINKS)
			arsta = rcu_dereference(ahsta->link[link_id]);
		if (!arsta)
			arsta = &ahsta->deflink;
		arvif = arsta->arvif;
		rcu_read_unlock();

		if (arvif)
			peer->rssi_mon.cfg = &arvif->rssi_deauth_cfg;
	}

	ath12k_dp_link_peer_htbl_add(dp_pdev, peer);

	if (!peer->is_bridge_peer) {
		ret = ath12k_telemetry_peer_agent_create_handler(ar, peer);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
					 "TA peer create failed vdev_id:%d addr %pM ret %d\n",
					 vdev_id, addr, ret);
		}
	}

	spin_unlock_bh(&dp->dp_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return 0;

err_dp_peer:
	ath12k_dp_peer_link_stats_free(peer);
	kfree(peer);
	spin_unlock_bh(&dp->dp_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return ret;
}

/**
 * ath12k_dp_capture_link_peer_stats - Capture all stats from link peer
 * @aggr_stats: Aggregation structure to capture stats into
 * @peer: Link peer whose stats need to be captured
 * @dp_peer: MLD peer associated with the link peer
 * @stats_link_id: Link ID for stats array indexing
 *
 * Captures HTT TX stats, per-packet TX stats (all TCL rings), RX peer stats,
 * per-packet RX stats (all REO rings), and the always-maintained RX MSDU
 * totals from a link peer into the provided aggregation structure. This is
 * used to preserve statistics before link peer deletion.
 *
 */

static void
ath12k_dp_capture_link_peer_stats(struct ath12k *ar,
				  struct ath12k_dp_preserved_stats *aggr_stats,
				  struct ath12k_dp_link_peer *peer,
				  struct ath12k_dp_peer *dp_peer,
				  u8 stats_link_id)
{
	int i;

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_tx_stats(&aggr_stats->per_pkt_tx[i],
						&dp_peer->stats[stats_link_id].tx[i]);
	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_rx_stats(&aggr_stats->per_pkt_rx[i],
						&dp_peer->stats[stats_link_id].rx[i]);
	ath12k_dp_aggr_per_pkt_tx_dbg_stats(&aggr_stats->tx_dbg,
					    &dp_peer->stats[stats_link_id].tx_dbg);
	ath12k_dp_aggr_per_pkt_rx_dbg_stats(&aggr_stats->rx_dbg,
					    &dp_peer->stats[stats_link_id].rx_dbg);
	ath12k_dp_aggr_wbm_rx_stats(&aggr_stats->wbm_err,
				    &dp_peer->stats[stats_link_id].wbm_err);
	aggr_stats->rx_counters.packets += peer->rx_packets;
	aggr_stats->rx_counters.bytes += peer->rx_bytes;
}

/**
 * ath12k_dp_aggr_link_peer_to_mld_peer - Aggregate link peer stats to MLD peer
 * @peer: Link peer whose stats need to be aggregated
 * @dp_peer: MLD peer to aggregate stats into
 * @stats_link_id: Link ID for stats indexing
 *
 * This function aggregates statistics from a link peer to its corresponding
 * MLD peer before the link peer is deleted. Caller must hold the locks before
 * calling this.
 */
static void ath12k_dp_aggr_link_peer_to_mld_peer(struct ath12k *ar,
						 struct ath12k_dp_link_peer *peer,
						 struct ath12k_dp_peer *dp_peer,
						 u8 stats_link_id)
{
	if (!peer || !dp_peer)
		return;

	ath12k_dp_capture_link_peer_stats(ar, &dp_peer->link_peer_delete_stats,
					  peer, dp_peer, stats_link_id);
}

/**
 * ath12k_dp_aggr_link_peer_to_link_vif - Aggregate link peer stats to link VIF
 * @dp_link_vif: Link VIF to aggregate stats into
 * @peer: Link peer whose stats need to be aggregated
 * @dp_peer: MLD peer associated with the link peer
 * @stats_link_id: Link ID for stats array indexing
 *
 * Aggregates statistics from a link peer to its associated link VIF before
 * the link peer is deleted. This preserves per-VIF statistics across peer
 * lifecycle events. Returns early if any pointer is NULL.
 */

void ath12k_dp_aggr_link_peer_to_link_vif(struct ath12k *ar,
					  struct ath12k_dp_link_vif *dp_link_vif,
					  struct ath12k_dp_link_peer *peer,
					  struct ath12k_dp_peer *dp_peer,
					  u8 stats_link_id)
{
	if (!peer || !dp_peer || !dp_link_vif)
		return;

	ath12k_dp_capture_link_peer_stats(ar, &dp_link_vif->link_peer_delete_stats,
					  peer, dp_peer, stats_link_id);
}

/**
 * ath12k_dp_aggr_clear_per_pkt_stats - Clear per-packet stats after aggregation
 * @peer: MLD peer whose per-packet stats need to be cleared
 * @stats_link_id: Link ID for stats array indexing
 *
 * Clears per-packet TX stats (all TCL rings) and per-packet RX stats (all REO
 * rings) for the specified link ID to prevent double-counting when stats are
 * reused or aggregated multiple times. Should be called after stats have been
 * aggregated to MLD peer or link VIF.
 */

static void ath12k_dp_aggr_clear_per_pkt_stats(struct ath12k_dp_peer *peer,
					       u8 stats_link_id)
{
	ath12k_dp_clear_per_pkt_tx_stats(&peer->stats[stats_link_id]);
	ath12k_dp_clear_per_pkt_rx_stats(&peer->stats[stats_link_id]);
	ath12k_dp_clear_wbm_rx_stats(&peer->stats[stats_link_id].wbm_err);
}

static void __ath12k_dp_link_peer_unassign(struct ath12k *ar,
					   struct ath12k_dp *dp,
					   struct ath12k_dp_hw *dp_hw,
					   struct ath12k_dp_link_peer *peer,
					   struct ath12k_dp_link_vif *link_vif,
					   u8 *addr)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp_peer *dp_peer;
	int stats_link_id;
	u16 peerid_index;
	int ret;

	dp_peer = peer->dp_peer;
	stats_link_id = peer->hw_link_id;

	dp_peer->l2h_link_map[peer->link_id] = ATH12K_DP_HW_LINK_ID_INVALID;

	dp_peer->hw_links[peer->hw_link_id] = 0;

	ath12k_dp_arch_link_peer_unassign_id(dp, ar, peer);

	if (!dp_peer->is_vdev_peer) {
		dp_peer->peer_links_map &= ~BIT(peer->hw_link_id);
		if (stats_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS) {
			/* Preserve link peer stats to MLD peer before deletion */
			ath12k_dp_aggr_link_peer_to_mld_peer(ar, peer, dp_peer,
							     stats_link_id);
			/* Preserve link peer stats to link VIF before deletion */
			if (link_vif)
				ath12k_dp_aggr_link_peer_to_link_vif(ar, link_vif, peer,
								     dp_peer,
								     stats_link_id);
			/* Clear per-packet stats to prevent double-counting on reuse */
			ath12k_dp_aggr_clear_per_pkt_stats(dp_peer, stats_link_id);
		}
	}

	rcu_assign_pointer(dp_peer->link_peers[peer->hw_link_id], NULL);

	if (peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
		peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
	}

	ath12k_dp_link_peer_htbl_delete(dp_pdev, peer);

	if (!peer->is_bridge_peer && link_vif) {
		ret = ath12k_telemetry_peer_agent_delete_handler(ar, peer);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
					 "failed to delete peer reference in TA for vdev_id %d addr %pM ret %d\n",
					 link_vif->vdev_id, addr, ret);
		}
	}

	dp_peer->teardown_link_peers[peer->hw_link_id] = NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_addr_post_rcu_remove(struct ath12k_dp_peer *dp_peer,
						 const char *addr)
{
	struct ath12k_dp_link_peer *link_peer = NULL;
	u8 i;

	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		if (dp_peer->teardown_link_peers[i] &&
		    ether_addr_equal(dp_peer->teardown_link_peers[i]->addr, addr)) {
			link_peer = dp_peer->teardown_link_peers[i];
			break;
		}
	}

	return link_peer;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_addr_post_rcu_remove);

static struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_link_id_post_rcu_remove(struct ath12k_dp_peer *dp_peer,
						    u8 link_id)
{
	u8 hw_link_id;

	hw_link_id = ath12k_dp_peer_convert_logical_to_hw_link_id(dp_peer, link_id);

	if (hw_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS)
		return dp_peer->teardown_link_peers[hw_link_id];

	return NULL;
}

void ath12k_dp_cp_link_peer_unassign(struct ath12k *ar,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_sta *ahsta, u8 link_id,
				     u8 *addr, bool update_bmap)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_link_vif *dp_link_vif = NULL;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_sta *arsta;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_dp_peer *dp_peer;
	struct wiphy *wiphy = ah->hw->wiphy;
	bool pre_rcu_remove_done = false;

	lockdep_assert_wiphy(wiphy);

	if (WARN_ON(link_id >= ATH12K_NUM_MAX_LINKS))
		return;

	ahvif = arvif->ahvif;
	dp_vif = &ahvif->dp_vif;
	dp_link_vif = &dp_vif->dp_link_vif[link_id];

	/* Flush the pending events to be safe */
	ath12k_event_queue_flush(&ahvif->event_queue);

	/*
	 * Step 1: Find dp_peer by STA MAC address.
	 * Use peer_hash_lock to protect the hash table lookup.
	 * Note: ahsta->addr is the MLD MAC for MLO STA, link MAC for non-MLO.
	 */
	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, ahsta->addr, sta);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	pre_rcu_remove_done = dp_peer->pre_rcu_remove_done;

	/*
	 * Step 2: Find the link peer within dp_peer.
	 */
	if (!pre_rcu_remove_done) {
		rcu_read_lock();
		peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
		if (!peer) {
			rcu_read_unlock();
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return;
		}

		dp_peer->teardown_link_peers[peer->hw_link_id] = peer;

		rcu_read_unlock();
	}

	peer = ath12k_dp_link_peer_find_by_link_id_post_rcu_remove(dp_peer, link_id);
	if (!peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	ath12k_dp_peer_cleanup(ar, dp_peer, arvif->vdev_id, peer->addr);

	/*
	 * Step 3: Remove link peer from all DP data structures.
	 * dp_lock serializes concurrent modifications.
	 */
	spin_lock_bh(&dp->dp_lock);

	__ath12k_dp_link_peer_unassign(ar, dp, dp_hw, peer, dp_link_vif, addr);

	spin_unlock_bh(&dp->dp_lock);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	/*
	 * TODO: Move to CP — the following MAC-layer teardown (ahsta->link[],
	 * links_map, etc.) belongs in the control path, not the DP path.
	 */

	/*
	 * ahsta->link[link_id] may already be NULL if sta_pre_rcu_remove()
	 * cleared it before mac80211's synchronize_net(). Use saved_link_sta
	 * as fallback.
	 */
	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta)
		arsta = ahsta->saved_link_sta[link_id];
	if (WARN_ON(!arsta))
		return;

	ahsta->links_map &= ~BIT(link_id);
	ahsta->device_bitmap &= ~BIT(ab->wsi_info.index);

	if (update_bmap && arvif->update_skip_link)
		ahsta->mlo_hw_link_id_bitmap &= ~BIT(arvif->ar->pdev->hw_link_id);

	ahsta->free_logical_idx_map |= BIT(arsta->link_idx);
	ahsta->num_peer--;

	/*
	 * ahsta->link[link_id] is already NULL if sta_pre_rcu_remove() ran.
	 * This rcu_assign_pointer() is a no-op in that case but is kept as
	 * a safety net for paths where sta_pre_rcu_remove() was not called.
	 */
	rcu_assign_pointer(ahsta->link[link_id], NULL);

	if (!pre_rcu_remove_done)
		synchronize_net();

	/* Link peer is freed after the RCU grace period has passed */
	__ath12k_link_peer_free(peer);
}

void ath12k_dp_link_peer_unassign(struct ath12k *ar, struct ath12k_link_vif *arvif,
				  u8 *addr, struct ieee80211_sta *sta)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_vif *link_vif = NULL;
	struct ath12k_dp_peer *dp_peer;
	bool pre_rcu_remove_done = false;

	lockdep_assert_wiphy(ath12k_dp_pdev_to_hw(dp_pdev)->wiphy);

	ahvif = arvif->ahvif;
	link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];
	/* Flush the pending events to be safe */
	ath12k_event_queue_flush(&ahvif->event_queue);

	/*
	 * Step 1: Find dp_peer by MAC address.
	 * If sta is NULL this is a vdev peer (BSS self-peer).
	 */
	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, sta->addr, sta);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, ahvif->vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	pre_rcu_remove_done = dp_peer->pre_rcu_remove_done;

	/*
	 * Step 2: Find the link peer within dp_peer.
	 */
	if (!pre_rcu_remove_done) {
		rcu_read_lock();
		peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, addr);
		if (!peer) {
			rcu_read_unlock();
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return;
		}

		dp_peer->teardown_link_peers[peer->hw_link_id] = peer;

		rcu_read_unlock();
	}

	peer = ath12k_dp_link_peer_find_by_addr_post_rcu_remove(dp_peer, addr);
	if (!peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	/*
	 * Step 3: Remove link peer from all DP data structures.
	 *
	 * __ath12k_dp_link_peer_unassign() removes the link peer from:
	 *   - dp->rhead_peer_addr (per-card rhashtable, RCU B domain)
	 *   - dp_peer->link_peers[] (RCU array)
	 *   - dp->peers list and other bookkeeping
	 */
	spin_lock_bh(&dp->dp_lock);

	__ath12k_dp_link_peer_unassign(ar, dp, dp_hw, peer, link_vif, addr);

	spin_unlock_bh(&dp->dp_lock);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (!pre_rcu_remove_done) {
		/*
		 * sta_pre_rcu_remove() was not called.
		 * Wait for all RCU readers that may hold a reference to
		 * link_peer before freeing it.
		 */
		synchronize_net();
	}

	/*
	 * All lookup paths for link_peer have been removed and the RCU grace
	 * period has passed (either via synchronize_net() above or via
	 * mac80211's synchronize_net() in the pre-RCU path). Safe to free.
	 */
	__ath12k_link_peer_free(peer);
}

/**
 * struct ath12k_batch_cleanup_ctx - Context for the phase-1 peer-collection
 *                                   iterator used by
 *                                   ath12k_dp_link_peer_batch_cleanup.
 * @ar:           Radio instance; forwarded to __ath12k_dp_link_peer_unassign.
 * @dp:           Global ath12k_dp; forwarded to __ath12k_dp_link_peer_unassign.
 * @dp_hw:        dp_hw pointer; forwarded to __ath12k_dp_link_peer_unassign.
 * @peer_match:   Optional caller-supplied filter; NULL means match all peers.
 * @context:      Opaque data forwarded to @peer_match.
 * @cleanup_list: Local list onto which matched peers are collected; freed
 *                after synchronize_rcu() in phase 2.
 * @count:        Number of peers collected so far.
 */
struct ath12k_batch_cleanup_ctx {
	struct ath12k    *ar;
	struct ath12k_dp *dp;
	struct ath12k_dp_hw *dp_hw;
	bool (*peer_match)(struct ath12k_dp_link_peer *link_peer, void *data);
	void             *context;
	struct list_head  cleanup_list;
	int               count;
};

/**
 * ath12k_batch_cleanup_cb() - Phase-1 per-peer callback for
 *                             ath12k_dp_link_peer_batch_cleanup.
 * @peer: Current ath12k_dp_link_peer visited by the iterator.
 * @data: Pointer to struct ath12k_batch_cleanup_ctx.
 *
 * Mirrors the body of the old list_for_each_entry_safe loop:
 *   - skips peers that do not satisfy ctx->peer_match (if set)
 *   - calls __ath12k_dp_link_peer_unassign to detach the peer from all
 *     data-path tables
 *   - appends the peer to ctx->cleanup_list for phase-2 freeing
 *
 * Must NOT call synchronize_rcu() or sleep; that is done by the caller
 * after iteration completes.
 *
 * Return: always 0 (iteration is never aborted).
 */
static void ath12k_batch_cleanup_cb(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_link_peer *peer, void *data)
{
	struct ath12k_batch_cleanup_ctx *ctx = data;

	if (ctx->peer_match && !ctx->peer_match(peer, ctx->context))
		return;

	spin_lock_bh(&dp_pdev->dp->dp_lock);

	__ath12k_dp_link_peer_unassign(ctx->ar, ctx->dp, ctx->dp_hw,
				       peer, NULL, peer->addr);

	spin_unlock_bh(&dp_pdev->dp->dp_lock);

	list_add_tail(&peer->list, &ctx->cleanup_list);
	ctx->count++;
}

/**
 * ath12k_dp_link_peer_batch_cleanup()
 * @ar: ath12k radio instance
 * @peer_match: Callback to determine if peer should be cleaned up
 * @context: Context data for peer_match
 *
 * Must be called from process context (not atomic context).
 *
 * Returns: Number of peers cleaned up
 */
int
ath12k_dp_link_peer_batch_cleanup(struct ath12k *ar,
				  bool (*peer_match)(struct ath12k_dp_link_peer *,
						     void *),
				  void *context)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_batch_cleanup_ctx ctx = {
		.ar         = ar,
		.dp         = ar->dp.dp,
		.dp_hw      = &ar->ah->dp_hw,
		.peer_match = peer_match,
		.context    = context,
	};

	INIT_LIST_HEAD(&ctx.cleanup_list);

	/* Phase 1: collect and unlink matched peers via the RCU-safe iterator */
	ath12k_dp_link_peer_iterate_by_dp_pdev(&ar->dp,
					       ath12k_batch_cleanup_cb, &ctx);
	if (!ctx.count)
		return 0;

	synchronize_rcu();

	/* Phase 2: free peers after RCU grace period */
	list_for_each_entry_safe(peer, tmp, &ctx.cleanup_list, list) {
		list_del(&peer->list);
		__ath12k_link_peer_free(peer);
	}

	return ctx.count;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_batch_cleanup);

unsigned long ath12k_link_peer_last_active(struct ath12k_dp_link_peer *link_peer)
{
	unsigned long last_ack = READ_ONCE(link_peer->peer_stats.last_ack);
	unsigned long last_rx = READ_ONCE(link_peer->peer_stats.last_rx);

	if (!last_ack || time_after(last_rx, last_ack))
		return last_rx;

	return last_ack;
}

void
ath12k_link_peer_get_sta_rate_info_stats(struct ath12k_dp_link_peer *link_peer,
					 struct ath12k_dp_link_peer_rate_info *rate_info)
{
	spin_lock_bh(&link_peer->ppdu_stats_lock);
	rate_info->rx_duration = link_peer->rx_duration;
	rate_info->tx_duration = link_peer->tx_duration;
	/* Copy txrate and rxrate atomically under the caller's ppdu stats lock
	 * Field-by-field access races with dp_htt.c/dp_mon.c writers that
	 * do memset + multi-store sequences under stats lock on another CPU.
	 */
	memcpy(&rate_info->txrate, &link_peer->txrate, sizeof(struct rate_info));
	memcpy(&rate_info->rxrate, &link_peer->rxrate, sizeof(struct rate_info));
	rate_info->rssi_comb = link_peer->signal_stats.rssi;
	rate_info->signal_avg = (s8)link_peer->signal_stats.rssi_avg;
	rate_info->tx_retry_count = link_peer->tx_retry_count;
	rate_info->tx_retry_failed = link_peer->tx_retry_failed;
	rate_info->rx_retries = link_peer->peer_stats.rx_retries;
	spin_unlock_bh(&link_peer->ppdu_stats_lock);
}

struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_get(struct ath12k_dp *dp,
		       struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_peer_qos *qos = NULL;

	if (peer && peer->qos)
		qos = peer->qos;

	return qos;
}

bool ath12k_dp_qos_stats_alloc(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_link_peer_qos_stats *link_qos_stats = NULL;

	/* already allocated */
	if (peer->peer_stats.link_qos_stats)
		return true;

	if (vif->type != NL80211_IFTYPE_AP ||
	    peer->dp_peer->is_vdev_peer ||
	    !ath12k_debugfs_is_qos_stats_enabled(ar))
		return false;

	link_qos_stats = kzalloc(sizeof(*link_qos_stats), GFP_ATOMIC);
	if (!link_qos_stats) {
		ath12k_err(ar->ab, "Peer QoS stats allocation failed for peer: %pM link_id: %u\n",
			   peer->addr, peer->link_id);
		return false;
	}

	peer->peer_stats.link_qos_stats = link_qos_stats;

	ath12k_dbg(ar->ab, ATH12K_DBG_QOS, "Peer QoS stats allocated for peer: %pM link_id: %u\n",
		   peer->addr, peer->link_id);
	return true;
}

static u8 ath12k_get_tid_msduq(struct ath12k_base *ab,
			       struct ath12k_dp_peer_qos *qos,
			       struct ath12k_dp_peer *dp_peer,
			       struct ath12k_dp_hw *dp_hw,
			       u16 qos_id, u8 svc_id, u8 tid,
			       u8 qm_id, bool txop_intent)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	void *telemetry_peer_ctx;
	u8 q, msduq = QOS_INVALID_MSDUQ;

	/* Find matching msduq with qos_id in the reserved pool.
	 * For dedicated_queue (txop_intent=true), only reuse a slot
	 * that was allocated for the same qm_id — each dedicated
	 * session must have its own MSDUQ even if qos_id matches.
	 */
	for (q = 0; q < QOS_TID_MDSUQ_MAX; ++q) {
		if (!qos->msduq_map[tid][q].reserved)
			continue;
		if (qos->msduq_map[tid][q].qos_id != qos_id)
			continue;
		if (txop_intent &&
		    qos->msduq_map[tid][q].qm_id != qm_id)
			continue;
		msduq = qos->msduq_map[tid][q].msduq;
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Res:msduq 0x%x:tid %u usrdefq %u\n",
			   msduq, tid, q);
		break;
	}

	if (msduq != QOS_INVALID_MSDUQ)
		return msduq;

	/* Reserve a new one */
	for (q = 0; q < QOS_TID_MDSUQ_MAX; ++q) {
		if (qos->msduq_map[tid][q].reserved)
			continue;

		qos->msduq_map[tid][q].reserved = true;
		qos->msduq_map[tid][q].qos_id = qos_id;
		msduq = u8_encode_bits(q, MSDUQ_MASK) |
						u8_encode_bits(tid, MSDUQ_TID_MASK);
		msduq = msduq + MSDUQ_MAX_DEF;
		qos->msduq_map[tid][q].msduq = msduq;
		qos->msduq_map[tid][q].qm_id = qm_id;
		qos->msduq_map[tid][q].txop_intent = txop_intent;
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "New: msduq 0x%x:tid %u usrdefq %u",
			   msduq, tid, q);
		if (!qos->telemetry_peer_ctx && dp_peer && dp_hw) {
			struct ath12k_dp_link_peer *link_peer;
			u8 hostq_id = msduq - MSDUQ_MAX_DEF;

			link_peer = ath12k_dp_link_peer_find_by_logical_link_id
						(dp_peer, dp_peer->primary_link_id);
			if (link_peer) {
				telemetry_peer_ctx =
					ath12k_telemetry_peer_ctx_alloc(dp_hw,
									dp_peer,
									link_peer->addr,
									svc_id, hostq_id);
				if (telemetry_peer_ctx) {
					qos->telemetry_peer_ctx = telemetry_peer_ctx;

					ath12k_dbg(ab, ATH12K_DBG_QOS,
						   "telemetry peer ctx allocation with msduq_id:0x%x\n",
						   hostq_id);
				}
			}
		}

		if (ath12k_dp_qos_queue_setup(dp, dp->dp_hw_grp,
					      dp_peer, msduq, qos_id,
					      qm_id, txop_intent))
			msduq = QOS_INVALID_MSDUQ;
		break;
	}

	return msduq;
}

u8 ath12k_dp_peer_qos_msduq(struct ath12k_base *ab,
			    struct ath12k_dp_peer_qos *qos,
			    struct ath12k_dp_peer *dp_peer,
			    struct ath12k_dp_hw *dp_hw,
			    u16 qos_id, u8 svc_id,
			    u8 qm_id, bool txop_intent)
{
	u8 msduq;
	u8 qos_tid;

	lockdep_assert_held(&qos->lock);

	qos_tid = ath12k_qos_get_tid(ab, qos_id);
	if (qos_tid >= QOS_TID_MAX) {
		ath12k_err(ab, "Invalid TID: %d", qos_tid);
		return QOS_INVALID_MSDUQ;
	}

	/* Get MSDUQ for excat QoS profile TID */
	msduq = ath12k_get_tid_msduq(ab, qos, dp_peer, dp_hw, qos_id,
				     svc_id, qos_tid, qm_id, txop_intent);

	/* Get MSDUQ for lower QoS profile TID */
	if (msduq == QOS_INVALID_MSDUQ && qos_tid != 0) {
		u8 tid = qos_tid - 1;

		while (tid < qos_tid) {
			msduq = ath12k_get_tid_msduq(ab, qos, dp_peer, dp_hw,
						     qos_id, svc_id, tid,
						     qm_id, txop_intent);
			if (msduq != QOS_INVALID_MSDUQ)
				break;
			--tid;
		}
	}
	/* Get MSDUQ for higher QoS profile TID */
	if (msduq == QOS_INVALID_MSDUQ) {
		u8 tid = qos_tid + 1;

		while (tid < QOS_MAX_TID) {
			msduq = ath12k_get_tid_msduq(ab, qos, dp_peer, dp_hw,
						     qos_id, svc_id, tid,
						     qm_id, txop_intent);
			if (msduq != QOS_INVALID_MSDUQ)
				break;
			++tid;
		}
	}
	return msduq;
}

static int __ath12k_dp_peer_scs_add(struct ath12k_dp_peer_qos *qos,
				    u8 scs_id, u16 qos_id,
				    bool dedicated_queue)
{
	struct ath12k_dl_scs *scs;
	u16 qos_data;

	if (scs_id >= QOS_MAX_SCS_ID) {
		ath12k_err(NULL, "ath12k: Invalid SCS ID");
		return -EINVAL;
	}

	scs = &qos->scs_map[scs_id];
	qos_data = u16_encode_bits(QOS_INVALID_MSDUQ, SCS_MSDUQ_MASK) |
		   u16_encode_bits(qos_id, SCS_QOS_ID_MASK);
	scs->qos_id_msduq = qos_data;
	scs->dedicated_queue = dedicated_queue;

	ath12k_info(NULL, "Peer QoS add scs_id:%d | msduq:%d| qos_id:%d",
		    scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		    u16_get_bits(qos_data, SCS_QOS_ID_MASK));

	return 0;
}

int ath12k_dp_peer_scs_add(struct ath12k_dp_peer *dp_peer, u8 qm_id,
			   u16 qos_id, bool dedicated_queue)
{
	struct ath12k_dp_peer_qos *qos;
	int ret;

	qos = dp_peer->qos;
	if (!qos) {
		ath12k_err(NULL, "SCS QoS is NULL");
		return -EINVAL;
	}

	spin_lock_bh(&qos->lock);
	ret = __ath12k_dp_peer_scs_add(qos, qm_id, qos_id, dedicated_queue);
	spin_unlock_bh(&qos->lock);

	return ret;
}

static int __ath12k_dp_peer_scs_del(struct ath12k_dp_peer_qos *qos,
				    u8 scs_id)
{
	struct ath12k_dl_scs *scs;
	u16 qos_data;

	if (!qos) {
		ath12k_err(NULL, "Invalid QoS");
		return -EINVAL;
	}

	if (scs_id >= QOS_MAX_SCS_ID) {
		ath12k_err(NULL, "ath12k: Invalid SCS ID");
		return -EINVAL;
	}

	scs = &qos->scs_map[scs_id];
	qos_data = u16_encode_bits(QOS_INVALID_MSDUQ, SCS_MSDUQ_MASK) |
		   u16_encode_bits(QOS_ID_INVALID, SCS_QOS_ID_MASK);
	scs->qos_id_msduq = qos_data;

	ath12k_info(NULL, "Peer QoS del scs_id:%d | msduq:%d| qos_id:%d",
		    scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		    u16_get_bits(qos_data, SCS_QOS_ID_MASK));
	return 0;
}

int ath12k_dp_peer_scs_del(struct ath12k_dp_peer *dp_peer, u8 qm_id,
			   u16 *qos_id)
{
	struct ath12k_dp_peer_qos *qos;

	qos = dp_peer->qos;
	if (!qos) {
		ath12k_err(NULL, "SCS QoS is NULL");
		return -EINVAL;
	}

	spin_lock_bh(&qos->lock);
	*qos_id = ath12k_dp_peer_scs_get_qos_id(qos, qm_id);
	__ath12k_dp_peer_scs_del(qos, qm_id);
	spin_unlock_bh(&qos->lock);

	return 0;
}

u16 ath12k_dp_peer_scs_get_qos_id(struct ath12k_dp_peer_qos *qos, u8 scs_id)
{
	u16 qos_id = QOS_ID_INVALID;

	if (!qos)
		return qos_id;

	if (scs_id >= QOS_MAX_SCS_ID) {
		ath12k_err(NULL, "ath12k: Invalid SCS ID");
		return qos_id;
	}

	qos_id = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			      SCS_QOS_ID_MASK);

	ath12k_dbg(NULL, ATH12K_DBG_QOS,
		   "Peer QoS Get QoS ID: %d| qos_id:%d",
		   scs_id, u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
					SCS_QOS_ID_MASK));
	return qos_id;
}

int ath12k_dp_peer_scs_data(struct ath12k_dp *dp,
			    u8 scs_id,
			    struct ath12k_dp_hw *dp_hw,
			    u8 *queue, u16 *id,
			    struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_peer_qos *qos;
	u16 qos_data;
	u8 msduq = QOS_INVALID_MSDUQ;
	u16 qos_id = QOS_ID_INVALID;
	int ret = -EINVAL;

	if (!dp_peer || !dp_peer->qos)
		return ret;

	qos = dp_peer->qos;

	spin_lock_bh(&qos->lock);
	if (scs_id >= QOS_MAX_SCS_ID) {
		ath12k_err(dp->ab, "ath12k: Invalid SCS ID");
		goto ret;
	}

	msduq = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			     SCS_MSDUQ_MASK);
	qos_id = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			      SCS_QOS_ID_MASK);

	if (msduq != QOS_INVALID_MSDUQ && qos_id < QOS_ID_INVALID) {
		ret = 0;
		goto ret;
	}

	if (qos_id >= QOS_ID_INVALID) {
		ath12k_err(dp->ab, "Invalid QoS ID: %d", qos_id);
		goto ret;
	}

	if (qos_id > QOS_UL_ID_MAX && qos_id < QOS_ID_INVALID)
		msduq = qos_id - QOS_LEGACY_DL_ID_MIN;
	else
		msduq = ath12k_dp_peer_qos_msduq(dp->ab, qos, dp_peer,
						 dp_hw, qos_id, scs_id,
						 scs_id,
						 qos->scs_map[scs_id].dedicated_queue);

	if (msduq == QOS_INVALID_MSDUQ)
		goto ret;

	qos_data = u16_encode_bits(msduq, SCS_MSDUQ_MASK) |
		   u16_encode_bits(qos_id, SCS_QOS_ID_MASK);
	qos->scs_map[scs_id].qos_id_msduq = qos_data;

	ath12k_dbg(dp->ab, ATH12K_DBG_QOS,
		   "Peer QoS get scs_id:%d | msduq:%d| qos_id:%d",
		   scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		   u16_get_bits(qos_data, SCS_QOS_ID_MASK));

	ret = 0;
ret:
	spin_unlock_bh(&qos->lock);
	*queue = msduq;
	*id = qos_id;
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_peer_scs_data);

u16 dp_peer_msduq_qos_id(struct ath12k_base *ab,
			 struct ath12k_dp_peer_qos *qos,
			 u8 msduq)
{
	u16 qos_id = QOS_ID_INVALID;
	u8 tid, q;

	if (msduq < (ab->def_tid_msduq * SDWF_MAX_TID_SUPPORT)) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Invalid msduq: 0x%x in mark\n", msduq);
		return 0;
	}

	msduq = msduq - MSDUQ_MAX_DEF;
	q = u16_get_bits(msduq, MSDUQ_MASK);
	tid = u16_get_bits(msduq, MSDUQ_TID_MASK);

	spin_lock_bh(&qos->lock);
	if (qos->msduq_map[tid][q].reserved)
		qos_id = qos->msduq_map[tid][q].qos_id;
	spin_unlock_bh(&qos->lock);

	return qos_id;
}
EXPORT_SYMBOL(dp_peer_msduq_qos_id);

void ath12k_peer_qos_queue_ind_handler(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct htt_t2h_qos_info_ind *resp;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_dp_link_peer *link_peer = NULL;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_sdwf_msduq_evt_data evt_data = {0};
	struct ath12k *ar;
	u8 pdev_idx = 0;
#endif /* CPTCFG_QCN_EXTN */
	bool send_event = false;
	u32 htt_qtype, remapped_tid, peer_id;
	u32 def_tid_msduq, max_def_msduq, qos_tid_msduq;
	u32 hlos_tid, flow_or, ast_idx, who_cl, tgt_opaque_id;
	u32 max_qos_msduq;
	u8 msduq_index, q_id;
#ifdef CPTCFG_QCN_EXTN
	u8 msduq;
	u16 qos_id;
#endif /* CPTCFG_QCN_EXTN */

	resp = (struct htt_t2h_qos_info_ind *)skb->data;
	htt_qtype = u32_get_bits(__le32_to_cpu(resp->info0),
				 HTT_T2H_QOS_MSDUQ_INFO_0_IND_HTT_QTYPE_ID);
	peer_id = u32_get_bits(__le32_to_cpu(resp->info0),
			       HTT_T2H_QOS_MSDUQ_INFO_0_IND_PEER_ID);

	remapped_tid = u32_get_bits(__le32_to_cpu(resp->info1),
				    HTT_T2H_QOS_MSDUQ_INFO_1_IND_REMAP_TID_ID);
	hlos_tid = u32_get_bits(__le32_to_cpu(resp->info1),
				HTT_T2H_QOS_MSDUQ_INFO_1_IND_HLOS_TID_ID);
	who_cl = u32_get_bits(__le32_to_cpu(resp->info1),
			      HTT_T2H_QOS_MSDUQ_INFO_1_IND_WHO_CLSFY_INFO_SEL_ID);
	flow_or = u32_get_bits(__le32_to_cpu(resp->info1),
			       HTT_T2H_QOS_MSDUQ_INFO_1_IND_FLOW_OVERRIDE_ID);
	ast_idx = u32_get_bits(__le32_to_cpu(resp->info1),
			       HTT_T2H_QOS_MSDUQ_INFO_1_IND_AST_INDEX_ID);

	tgt_opaque_id = u32_get_bits(__le32_to_cpu(resp->info2),
				     HTT_T2H_QOS_MSDUQ_INFO_2_IND_TGT_OPAQUE_ID);

	ath12k_info(ab, "QoS MSDUQ Map Ind:\n");
	ath12k_info(ab,
		    "htt_qtype[0x%x]Peer_Id[0x%x]Remp_Tid[0x%x]Hlos_Tid[0x%x]",
		    htt_qtype, peer_id, remapped_tid, hlos_tid);
	ath12k_info(ab, "who_cl[0x%x]flow_or[0x%x]Ast[0x%x]Op[0x%x]",
		    who_cl, flow_or, ast_idx, tgt_opaque_id);

	spin_lock_bh(&ab->base_lock);
	def_tid_msduq = ab->def_tid_msduq;
	qos_tid_msduq = ab->max_tid_msduq - ab->def_tid_msduq;
	spin_unlock_bh(&ab->base_lock);

	max_def_msduq = def_tid_msduq * QOS_TID_MAX;
	max_qos_msduq = qos_tid_msduq * QOS_TID_MAX;
	msduq_index = ((who_cl * max_def_msduq) +
		      (flow_or * QOS_TID_MAX) + hlos_tid) -
		      max_def_msduq;

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(ab->dp, NULL, peer_id);
	if (!link_peer || !link_peer->dp_peer) {
		rcu_read_unlock();
		return;
	}

	qos = link_peer->dp_peer->qos;
	if (!qos) {
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&qos->lock);
	if (msduq_index < max_qos_msduq) {
		q_id = htt_qtype - def_tid_msduq;

		if ((hlos_tid < QOS_TID_MAX) &&
		    (q_id < (qos_tid_msduq)) && qos) {
			qos->msduq_map[hlos_tid][q_id].tgt_opaque_id =
							tgt_opaque_id;

			/* Collect data for SDWF MSDUQ vendor event */
#ifdef CPTCFG_QCN_EXTN
			if (qos->msduq_map[hlos_tid][q_id].reserved) {
				msduq = qos->msduq_map[hlos_tid][q_id].msduq;
				qos_id = qos->msduq_map[hlos_tid][q_id].qos_id;

				if (msduq != QOS_INVALID_MSDUQ) {
					ath12k_sdwf_fill_msduq_event_data(ab, link_peer,
									  msduq,
									  qos_id,
									  &evt_data);
					if (evt_data.peer_id != ATH12K_PEER_ID_INVALID) {
						pdev_idx = link_peer->pdev_idx;
						send_event = true;
					}
				}
			}
#endif /* CPTCFG_QCN_EXTN */
		}

		if (qos->telemetry_peer_ctx)
			ath12k_telemetry_update_tid_msduq(qos->telemetry_peer_ctx,
							  msduq_index, remapped_tid,
							  (htt_qtype - def_tid_msduq));
	}

	spin_unlock_bh(&qos->lock);
	rcu_read_unlock();

	if (send_event) {
#ifdef CPTCFG_QCN_EXTN
		ar = ab->pdevs[pdev_idx].ar;
		if (ar)
			ath12k_vendor_sdwf_msduq_send_event(ar, &evt_data);
#endif /* CPTCFG_QCN_EXTN */
	}
}

u8 ath12k_dp_validate_hw_link_id(u8 hw_link_id)
{
	/* Sanity check: ensure the HW link id is within bounds */
	if (unlikely(hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS)) {
		ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
			   "Invalid HW link id %u\n", hw_link_id);
		return 0;
	}

	return hw_link_id;
}
EXPORT_SYMBOL(ath12k_dp_validate_hw_link_id);

struct ath12k_dp_vif *ath12k_dp_peer_get_dp_vif(struct ath12k_dp_peer *dp_peer)
{
	struct ieee80211_vif *vif = ath12k_dp_peer_get_vif(dp_peer);

	if (vif)
		return (struct ath12k_dp_vif *)(&ath12k_vif_to_ahvif(vif)->dp_vif);
	else
		return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_get_dp_vif);

struct ath12k_dp_vif *
ath12k_dp_link_peer_get_dp_vif(struct ath12k_dp_link_peer *link_peer)
{
	return ath12k_dp_peer_get_dp_vif(link_peer->dp_peer);
}
EXPORT_SYMBOL(ath12k_dp_link_peer_get_dp_vif);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_hw_link_id(struct ath12k_dp_peer *dp_peer, u8 hw_link_id)
{
	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp link peer find by link id without rcu lock");

	if (hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS)
		return NULL;

	return rcu_dereference(dp_peer->link_peers[hw_link_id]);
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_hw_link_id);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_logical_link_id(struct ath12k_dp_peer *dp_peer, u8 link_id)
{
	u8 hw_link_id = ath12k_dp_peer_convert_logical_to_hw_link_id(dp_peer, link_id);

	return ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_logical_link_id);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_mac_addr(struct ath12k_dp_peer *dp_peer, const u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	u8 link_idx;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp link peer find by mac addr without rcu lock");

	for (link_idx = 0; link_idx < ATH12K_DP_PEER_MAX_MLO_LINKS; link_idx++) {
		peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_idx);
		if (peer && !memcmp(addr, peer->addr, ETH_ALEN))
			return peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_mac_addr);

/**
 * ath12k_dp_hw_peer_stats_enabled - Check if HW offload peer stats is enabled
 * @dp_pdev: Pointer to the pdev DP structure
 *
 * Returns true if hardware offload peer statistics collection is supported
 * and enabled for the hardware, false otherwise.
 */
bool ath12k_dp_hw_peer_stats_enabled(struct ath12k_pdev_dp *dp_pdev)
{
	if (!dp_pdev || !dp_pdev->dp)
		return false;

	return dp_pdev->dp->hw_peer_stats_support;
}
EXPORT_SYMBOL(ath12k_dp_hw_peer_stats_enabled);

int ath12k_dp_alloc_delay_stats_peer(struct ath12k_dp_peer *dp_peer)
{
	int tid, ring_id;
	struct ath12k_dp_mld_peer_stats *mld_stats;
	struct ath12k_dp_peer_delay_stats *delay_stats;
	struct ath12k_dp_peer_delay_tx_stats *tx_delay;
	struct ath12k_dp_peer_delay_rx_stats *rx_delay;

	if (!dp_peer)
		return -EINVAL;

	mld_stats = &dp_peer->mld_stats;

	if (mld_stats->delay_stats)
		return 0;

	mld_stats->delay_stats = kzalloc(sizeof(*mld_stats->delay_stats),
					 GFP_ATOMIC);

	if (!mld_stats->delay_stats)
		return -ENOMEM;

	delay_stats = mld_stats->delay_stats;

	for (tid = 0; tid < DP_TID_MAX; tid++) {
		for (ring_id = 0; ring_id < DP_REO_DST_RING_MAX; ring_id++) {
			tx_delay = &delay_stats->delay_tid_stats[tid][ring_id].tx_delay;
			rx_delay = &delay_stats->delay_tid_stats[tid][ring_id].rx_delay;

			ath12k_dp_hist_init(&tx_delay->tx_swq_delay,
					    HIST_TYPE_SW_ENQEUE_DELAY);
			ath12k_dp_hist_init(&tx_delay->hwtx_delay,
					    HIST_TYPE_HW_COMP_DELAY);
			ath12k_dp_hist_init(&rx_delay->to_stack_delay,
					    HIST_TYPE_REAP_STACK);
		}
	}

	return 0;
}

int ath12k_dp_alloc_sojourn_stats_peer(struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_mld_peer_stats *mld_stats;
	struct ath12k_dp_peer_sojourn_stats *sojourn_stats;
	struct ath12k_dp_peer_tid_sojourn_stats *tid_stats;
	int tid, ring;

	if (!dp_peer)
		return -EINVAL;

	mld_stats = &dp_peer->mld_stats;

	if (mld_stats->sojourn_stats)
		return 0;

	mld_stats->sojourn_stats =
		kzalloc(sizeof(*mld_stats->sojourn_stats), GFP_ATOMIC);

	if (!mld_stats->sojourn_stats)
		return -ENOMEM;

	sojourn_stats = mld_stats->sojourn_stats;

	for (tid = 0; tid < DP_TID_MAX; tid++) {
		for (ring = 0; ring < DP_REO_DST_RING_MAX; ring++) {
			tid_stats = &sojourn_stats->tid_stats[tid][ring];
			ewma_avg_sojourn_init(&tid_stats->avg_sojourn_msdu);
		}
	}

	return 0;
}

int ath12k_dp_alloc_jitter_stats_peer(struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_mld_peer_stats *mld_stats;

	if (!dp_peer)
		return -EINVAL;

	mld_stats = &dp_peer->mld_stats;

	if (mld_stats->jitter_stats)
		return 0;

	mld_stats->jitter_stats =
		kzalloc(sizeof(*mld_stats->jitter_stats), GFP_ATOMIC);

	if (!mld_stats->jitter_stats)
		return -ENOMEM;

	return 0;
}

static int ath12k_dp_peer_qos_stats_alloc(struct ath12k_dp_peer *dp_peer)
{
	dp_peer->mld_stats.mld_qos_stats =
		kzalloc(sizeof(*dp_peer->mld_stats.mld_qos_stats) *
			QOS_TID_MAX * QOS_TID_MDSUQ_MAX, GFP_ATOMIC);

	if (!dp_peer->mld_stats.mld_qos_stats) {
		ath12k_err(NULL,
			   "failed to alloc qos_stats for peer %pM\n",
			   dp_peer->addr);

		return -ENOMEM;
	}
	return 0;
}

void ath12k_dp_peer_qos_stats_free(struct ath12k_dp_peer *dp_peer)
{
	kfree(dp_peer->mld_stats.mld_qos_stats);
	dp_peer->mld_stats.mld_qos_stats = NULL;
}

int ath12k_dp_peer_stats_alloc(struct ath12k_dp_peer *dp_peer,
			       struct ath12k_pdev_dp *dp_pdev)
{
	int ret = 0;

	if (!dp_peer || !dp_pdev) {
		ath12k_err(NULL,
			   "Stats alloc NULL arg: dp_peer=%p dp_pdev=%p\n",
			   dp_peer, dp_pdev);
		return -EINVAL;
	}

	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev) &&
	    !dp_peer->mld_stats.hw_stats) {
		dp_peer->mld_stats.hw_stats =
			kzalloc(sizeof(*dp_peer->mld_stats.hw_stats),
				GFP_ATOMIC);
		if (!dp_peer->mld_stats.hw_stats) {
			ath12k_err(NULL,
				   "failed to alloc hw_stats for peer %pM\n",
				   dp_peer->addr);
			return -ENOMEM;
		}
	}

	if (ath12k_dp_latency_stats_enabled(dp_pdev)) {
		ret = ath12k_dp_alloc_delay_stats_peer(dp_peer);
		if (ret) {
			ath12k_warn(dp_pdev->ar->ab,
				    "Failed to allocate delay stats\n");
			goto err_free;
		}

		ret = ath12k_dp_alloc_jitter_stats_peer(dp_peer);
		if (ret) {
			ath12k_warn(dp_pdev->ar->ab,
				    "Failed to allocate jitter stats\n");
			goto err_free;
		}

		ret = ath12k_dp_alloc_sojourn_stats_peer(dp_peer);
		if (ret) {
			ath12k_warn(dp_pdev->ar->ab,
				    "Failed to allocate sojourn stats\n");
			goto err_free;
		}
	}

	if (ath12k_proto_stats_enabled(dp_pdev)) {
		ret = ath12k_dp_alloc_proto_stats_peer(dp_peer);
		if (ret) {
			ath12k_warn(dp_pdev->ar->ab, "Failed to alloc proto stats.\n");
			goto err_free;
		}
	}
	if (dp_pdev && (dp_pdev->dp_stats_mask & DP_ENABLE_QOS_STATS)) {
		ret = ath12k_dp_peer_qos_stats_alloc(dp_peer);
		if (ret) {
			ath12k_warn(dp_pdev->ar->ab,
				    "Failed to allocate Qos Stats\n");
			goto err_free;
		}
	}

	return ret;

err_free:
	ath12k_dp_peer_qos_stats_free(dp_peer);
	ath12k_dp_free_proto_stats_peer(dp_peer);
	kfree(dp_peer->mld_stats.delay_stats);
	dp_peer->mld_stats.delay_stats = NULL;
	kfree(dp_peer->mld_stats.jitter_stats);
	dp_peer->mld_stats.jitter_stats = NULL;
	kfree(dp_peer->mld_stats.sojourn_stats);
	dp_peer->mld_stats.sojourn_stats = NULL;
	kfree(dp_peer->mld_stats.hw_stats);
	dp_peer->mld_stats.hw_stats = NULL;

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_peer_stats_alloc);

void ath12k_dp_peer_stats_free(struct ath12k_dp_peer *dp_peer)
{
	if (!dp_peer)
		return;

	kfree(dp_peer->mld_stats.hw_stats);
	dp_peer->mld_stats.hw_stats = NULL;

	kfree(dp_peer->mld_stats.delay_stats);
	dp_peer->mld_stats.delay_stats = NULL;

	kfree(dp_peer->mld_stats.jitter_stats);
	dp_peer->mld_stats.jitter_stats = NULL;

	kfree(dp_peer->mld_stats.sojourn_stats);
	dp_peer->mld_stats.sojourn_stats = NULL;

	ath12k_dp_free_proto_stats_peer(dp_peer);

	ath12k_dp_peer_qos_stats_free(dp_peer);
}
EXPORT_SYMBOL(ath12k_dp_peer_stats_free);

static void ath12k_dp_qos_link_stats_alloc(struct ath12k_dp_link_peer *link_peer)
{
	link_peer->peer_stats.link_qos_stats =
		kzalloc(sizeof(*link_peer->peer_stats.link_qos_stats),
			GFP_ATOMIC);
}

static void ath12k_dp_tx_stats_free(struct ath12k_dp_link_peer *link_peer)
{
	kfree(link_peer->peer_stats.tx_stats);
	link_peer->peer_stats.tx_stats = NULL;
}

static void ath12k_dp_tx_ppdu_stats_free(struct ath12k_dp_link_peer *link_peer)
{
	kfree(link_peer->peer_stats.tx_ppdu_stats);
	link_peer->peer_stats.tx_ppdu_stats = NULL;
}

int ath12k_dp_peer_link_stats_alloc(struct ath12k_dp_link_peer *link_peer,
				    struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_link_peer_stats *peer_stats = NULL;

	if (!link_peer || !dp_pdev) {
		ath12k_err(NULL,
			   "Link stats alloc NULL arg: link_peer=%p dp_pdev=%p\n",
			   link_peer, dp_pdev);
		return -EINVAL;
	}

	peer_stats = &link_peer->peer_stats;

	if (ath12k_dp_hw_peer_stats_enabled(dp_pdev) &&
	    !peer_stats->hw_link_stats) {
		peer_stats->hw_link_stats =
			kzalloc(sizeof(*peer_stats->hw_link_stats),
				GFP_ATOMIC);
		if (!peer_stats->hw_link_stats) {
			ath12k_err(NULL,
				   "failed to alloc hw_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
	}

	if (ath12k_extd_rx_stats_enabled(dp_pdev) &&
	    !link_peer->peer_stats.rx_stats) {
		link_peer->peer_stats.rx_stats =
			kzalloc(sizeof(*link_peer->peer_stats.rx_stats),
				GFP_ATOMIC);
		if (!link_peer->peer_stats.rx_stats) {
			ath12k_err(NULL,
				   "failed to alloc rx_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
	}

	if (ath12k_dp_rx_ppdu_stats_enabled(dp_pdev) &&
	    !link_peer->peer_stats.rx_ppdu_stats) {
		link_peer->peer_stats.rx_ppdu_stats =
			kzalloc(sizeof(*link_peer->peer_stats.rx_ppdu_stats),
				GFP_ATOMIC);
		if (!link_peer->peer_stats.rx_ppdu_stats) {
			ath12k_err(NULL,
				   "failed to alloc rx_ppdu_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
	}

	if (ath12k_extd_tx_stats_enabled(dp_pdev) &&
	    !link_peer->peer_stats.tx_stats) {
		link_peer->peer_stats.tx_stats =
			kzalloc(sizeof(*link_peer->peer_stats.tx_stats),
				GFP_ATOMIC);
		if (!link_peer->peer_stats.tx_stats) {
			ath12k_err(NULL,
				   "failed to alloc tx_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
	}

	if (ath12k_htt_tx_ppdu_stats_enabled(dp_pdev) &&
	    !link_peer->peer_stats.tx_ppdu_stats) {
		link_peer->peer_stats.tx_ppdu_stats =
			kzalloc(sizeof(*link_peer->peer_stats.tx_ppdu_stats),
				GFP_ATOMIC);
		if (!link_peer->peer_stats.tx_ppdu_stats) {
			ath12k_err(NULL,
				   "failed to alloc tx_ppdu_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
		link_peer->peer_stats.tx_ppdu_stats->avg_ack_rssi = INVALID_RSSI;
		link_peer->peer_stats.tx_ppdu_stats->avg_tx_rate  = INVALID_RATE;
	}

	if (dp_pdev && (dp_pdev->dp_stats_mask & DP_ENABLE_QOS_STATS)) {
		ath12k_dp_qos_link_stats_alloc(link_peer);

		if (!peer_stats->link_qos_stats) {
			ath12k_err(NULL,
				   "failed to alloc link_qos_stats for link peer %pM\n",
				   link_peer->addr);
			return -ENOMEM;
		}
	}

	return 0;
}

static void ath12k_dp_qos_link_stats_free(struct ath12k_dp_link_peer *link_peer)
{
	kfree(link_peer->peer_stats.link_qos_stats);
	link_peer->peer_stats.link_qos_stats = NULL;
}

void ath12k_dp_peer_link_stats_free(struct ath12k_dp_link_peer *link_peer)
{
	if (!link_peer)
		return;

	kfree(link_peer->peer_stats.rx_ppdu_stats);
	link_peer->peer_stats.rx_ppdu_stats = NULL;

	kfree(link_peer->peer_stats.rx_stats);
	link_peer->peer_stats.rx_stats = NULL;

	kfree(link_peer->peer_stats.hw_link_stats);
	link_peer->peer_stats.hw_link_stats = NULL;

	ath12k_dp_qos_link_stats_free(link_peer);
	ath12k_dp_tx_stats_free(link_peer);
	ath12k_dp_tx_ppdu_stats_free(link_peer);
}

int ath12k_dp_peer_set_param_by_dp_peer(void *ptr, enum ath12k_dp_peer_param param,
					union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	switch (param) {
	case ATH12K_DP_PEER_MSCS_PARAM:
	{
		struct ath12k_dp_vif *dp_vif = ath12k_dp_peer_get_dp_vif(dp_peer);
		struct cfg80211_qm_req_desc_data *qm_req_desc = &val->mscs_params;
		u8 req_type = qm_req_desc->request_type;

		switch (qm_req_desc->request_type) {
		case IEEE80211_QM_ADD_REQ:
			if (dp_peer->mscs_session_exists)
				return -EINVAL;
			dp_peer->mscs_session_exists = true;
			dp_vif->mscs_hlos_tid_override++;
			dp_vif->dp_features |= DP_FEATURE_HLOS;
			fallthrough;
		case IEEE80211_QM_CHANGE_REQ:
			dp_peer->mscs_ctxt.user_priority_bitmap =
				qm_req_desc->user_priority_bitmap;
			dp_peer->mscs_ctxt.user_priority_limit =
				qm_req_desc->user_priority_limit;
			dp_peer->mscs_ctxt.tclas_mask =
				qm_req_desc->tclas_mask;

			ath12k_dbg(NULL, ATH12K_DBG_QOS,
				   "MSCS: %s: peer %pM, bmap 0x%x, limit %u, mask 0x%x",
				   (req_type == IEEE80211_QM_CHANGE_REQ) ? "CHANGE" :
				   "ADD",
				   dp_peer->addr,
				   dp_peer->mscs_ctxt.user_priority_bitmap,
				   dp_peer->mscs_ctxt.user_priority_limit,
				   dp_peer->mscs_ctxt.tclas_mask);

			ath12k_dbg(NULL, ATH12K_DBG_QOS,
				   "mscs_session_exists %u, mscs_tid_override %u",
				   dp_peer->mscs_session_exists,
				   dp_vif->mscs_hlos_tid_override);
		break;
		case IEEE80211_QM_REMOVE_REQ:
			dp_peer->mscs_session_exists = false;
			dp_vif->dp_features &= ~DP_FEATURE_HLOS;
			if (dp_vif->mscs_hlos_tid_override > 0) {
				dp_vif->mscs_hlos_tid_override--;
			} else {
				ath12k_dbg(NULL, ATH12K_DBG_QOS,
					   "MSCS: TID override counter underflow");
				return -EINVAL;
			}

			ath12k_dbg(NULL, ATH12K_DBG_QOS,
				   "MSCS: REMOVE peer %pM, mscs_session_exists %u",
				   dp_peer->addr, dp_peer->mscs_session_exists);

			ath12k_dbg(NULL, ATH12K_DBG_QOS, "mscs_tid_override %u",
				   dp_vif->mscs_hlos_tid_override);
		break;
		default:
			ret = -EINVAL;
		}
	}
	break;
	case ATH12K_DP_PEER_AUTHORIZE_PARAM:
		dp_peer->is_authorized = val->is_authorized;
	break;
	case ATH12K_DP_PEER_DMS_DISABLE_PARAM:
		dp_peer->dms_disable = val->dms_disable;
	break;
	case ATH12K_DP_PEER_CLEAR_KEYS_PARAM:
	{
		int i;

		ath12k_dp_peer_get_param_by_dp_peer(dp_peer, ATH12K_DP_PEER_KEYS_PARAM,
						    val);

		spin_lock_bh(&dp_peer->keys_lock);

		for (i = 0; i < val->keys_params.len; i++)
			dp_peer->keys[i] = NULL;

		spin_unlock_bh(&dp_peer->keys_lock);
	}
	break;
	case ATH12K_DP_PEER_PRIMARY_LINK_ID_PARAM:
		dp_peer->primary_link_id = val->primary_link_id;
	break;
	default:
		ath12k_err(NULL, "Invalid set param %d", param);
		ret = -EINVAL;
	}

	return ret;
}

int ath12k_dp_peer_get_param_by_dp_peer(void *ptr, enum ath12k_dp_peer_param param,
					union ath12k_config_param *val)
{
	int ret = 0, len, i;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	switch (param) {
	case ATH12K_DP_PEER_PEERID_PARAM:
		val->peer_id = dp_peer->peer_id;
	break;
	case ATH12K_DP_PEER_DMS_DISABLE_PARAM:
		val->dms_disable = dp_peer->dms_disable;
	break;
	case ATH12K_DP_PEER_KEYS_PARAM:
		spin_lock_bh(&dp_peer->keys_lock);

		len = ARRAY_SIZE(dp_peer->keys);

		for (i = 0; i < len; i++) {
			if (!dp_peer->keys[i])
				continue;

			val->keys_params.keys[i] = dp_peer->keys[i];
		}

		val->keys_params.len = len;

		spin_unlock_bh(&dp_peer->keys_lock);
	break;
	case ATH12K_DP_PEER_PN_PARAMS:
		spin_lock_bh(&dp_peer->keys_lock);

		val->pn_params.key = dp_peer->keys[val->pn_params.keyidx];

		spin_unlock_bh(&dp_peer->keys_lock);
	break;
	case ATH12K_DP_PEER_AUTHORIZE_PARAM:
		val->is_authorized = dp_peer->is_authorized;
	break;
	case ATH12K_DP_PEER_MAC_ADDR_PARAM:
		ether_addr_copy(val->addr, dp_peer->addr);
	break;
	default:
		ath12k_err(NULL, "Invalid set param %d", param);
		ret = -EINVAL;
	}

	return ret;
}

int ath12k_dp_peer_set_param_by_mac_addr(struct ath12k_dp_hw *dp_hw,
					 struct ieee80211_vif *vif,
					 const u8 *addr,
					 enum ath12k_dp_peer_param param,
					 union ath12k_config_param *val)
{
	int ret;
	struct ath12k_dp_peer *dp_peer = NULL;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	if (!dp_peer || dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	ret = ath12k_dp_peer_set_param_by_dp_peer(dp_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return ret;
}

int ath12k_dp_peer_get_param_by_mac_addr(struct ath12k_dp_hw *dp_hw,
					 struct ieee80211_vif *vif,
					 const u8 *addr,
					 enum ath12k_dp_peer_param param,
					 union ath12k_config_param *val)
{
	int ret;
	struct ath12k_dp_peer *dp_peer = NULL;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	ret = ath12k_dp_peer_get_param_by_dp_peer(dp_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return ret;
}

int ath12k_dp_peer_get_param_by_peer_id(struct ath12k_pdev_dp *dp_pdev, u16 peer_id,
					enum ath12k_dp_peer_param param,
					union ath12k_config_param *val)
{
	int ret;
	struct ath12k_dp_peer *dp_peer;

	rcu_read_lock();

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev, peer_id);
	if (!dp_peer) {
		rcu_read_unlock();
		return -ENOENT;
	}

	ret = ath12k_dp_peer_get_param_by_dp_peer(dp_peer, param, val);

	rcu_read_unlock();

	return ret;
}

static inline int ath12k_dp_link_peer_set_param(struct ath12k_dp_link_peer *link_peer,
						enum ath12k_dp_link_peer_param param,
						union ath12k_config_param *val)
{
	int ret = 0;

	switch (param) {
	case ATH12K_DP_LINK_PEER_ATF_PARAM:
		link_peer->atf_group_index = val->atf_params.atf_group_index;
		link_peer->atf_peer_conf_airtime = val->atf_params.atf_peer_conf_airtime;
	break;
	case ATH12K_DP_LINK_PEER_AUTHORIZE_PARAM:
		link_peer->is_authorized = val->is_authorized;
	break;
	case ATH12K_DP_LINK_PEER_ASSOC_PARAM:
		link_peer->assoc_success = val->assoc_success;
	break;
	case ATH12K_DP_LINK_PEER_TID_WEIGHT_PARAM: {
		u8 i;

		for (i = 0; i < ATH12K_DATA_TID_MAX; i++)
			link_peer->tid_weight[i] = val->tid_weight[i];
	break;
	}
	default:
		ath12k_err(NULL, "Invalid set param %d", param);
		ret = -EINVAL;
	}

	return ret;
}

static inline int ath12k_dp_link_peer_get_param(struct ath12k_dp_link_peer *link_peer,
						enum ath12k_dp_link_peer_param param,
						union ath12k_config_param *val)
{
	int ret = 0;

	switch (param) {
	case ATH12K_DP_LINK_PEER_PEERID_PARAM:
		val->peer_id = link_peer->peer_id;
	break;
	case ATH12K_DP_LINK_PEER_ASSOC_PARAM:
		val->assoc_success = link_peer->assoc_success;
	break;
	case ATH12K_DP_LINK_PEER_IS_PRIMARY:
		val->is_primary = link_peer->primary_link;
	break;
	case ATH12K_DP_LINK_PEER_MAC_ADDR_PARAM:
		ether_addr_copy(val->addr, link_peer->addr);
	break;
	case ATH12K_DP_LINK_PEER_TXRATE_PARAM:
		ath12k_link_peer_get_sta_rate_info_stats(link_peer,
							 &val->rate_info);
	break;
	default:
		ath12k_err(NULL, "Invalid set param %d", param);
		ret = -EINVAL;
	}

	return ret;
}

int ath12k_dp_link_peer_set_param_by_dp_peer_and_link_mac(void *ptr, const u8 *link_mac,
							  enum ath12k_dp_link_peer_param param,
							  union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, link_mac);
	if (!link_peer) {
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_set_param(link_peer, param, val);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_get_param_by_dp_peer_and_link_mac(void *ptr, const u8 *link_mac,
							  enum ath12k_dp_link_peer_param param,
							  union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, link_mac);
	if (!link_peer) {
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_get_param(link_peer, param, val);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_set_param_by_dp_peer_and_link_id(void *ptr, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_set_param(link_peer, param, val);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_get_param_by_dp_peer_and_link_id(void *ptr, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_get_param(link_peer, param, val);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_set_param_by_mld_and_link_mac(struct ath12k_dp_hw *dp_hw,
						      struct ieee80211_vif *vif,
						      const u8 *mld_mac,
						      const u8 *link_mac,
						      enum ath12k_dp_link_peer_param param,
						      union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;

	rcu_read_lock();

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, mld_mac, vif);
	if (!dp_peer || dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, link_mac);
	if (!link_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_set_param(link_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_get_param_by_mld_and_link_mac(struct ath12k_dp_hw *dp_hw,
						      struct ieee80211_vif *vif,
						      const u8 *mld_mac,
						      const u8 *link_mac,
						      enum ath12k_dp_link_peer_param param,
						      union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;

	rcu_read_lock();

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, mld_mac, vif);
	if (!dp_peer || dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, link_mac);
	if (!link_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_get_param(link_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_set_param_by_mld_mac_and_link_id(struct ath12k_dp_hw *dp_hw,
							 struct ieee80211_vif *vif,
							 const u8 *mld_mac, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;

	rcu_read_lock();

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, mld_mac, vif);
	if (!dp_peer || dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_set_param(link_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	rcu_read_unlock();

	return ret;
}

int ath12k_dp_link_peer_get_param_by_mld_mac_and_link_id(struct ath12k_dp_hw *dp_hw,
							 struct ieee80211_vif *vif,
							 const u8 *mld_mac, u8 link_id,
							 enum ath12k_dp_link_peer_param param,
							 union ath12k_config_param *val)
{
	int ret = 0;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;

	rcu_read_lock();

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, mld_mac, vif);
	if (!dp_peer || dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return -EINVAL;
	}

	ret = ath12k_dp_link_peer_get_param(link_peer, param, val);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	rcu_read_unlock();

	return ret;
}

/**
 * ath12k_sta_get_dp_peer_wiphy_locked() - Get dp_peer with wiphy lock held
 * @ahsta: pointer to ath12k_sta
 *
 * This function retrieves the dp_peer pointer in control path context
 * where wiphy lock is already held. Uses wiphy_dereference() which
 * provides proper RCU dereference with wiphy lock protection.
 *
 * Context: Must be called with wiphy lock held
 * Return: pointer to ath12k_dp_peer or NULL
 */
void *ath12k_sta_get_dp_peer_wiphy_locked(struct wiphy *wiphy, struct ath12k_sta *ahsta)
{
	/* Verify we're in the correct context (wiphy lock held) */
	lockdep_assert_wiphy(wiphy);

	/* Use wiphy_dereference for proper RCU access with wiphy lock */
	return wiphy_dereference(wiphy, ahsta->dp_peer);
}
EXPORT_SYMBOL(ath12k_sta_get_dp_peer_wiphy_locked);

/**
 * ath12k_sta_get_dp_peer_rcu() - Get dp_peer with RCU read lock held
 * @ahsta: pointer to ath12k_sta
 *
 * This function retrieves the dp_peer pointer in datapath/interrupt
 * context where RCU read lock must be held. This is typically used in
 * interrupt handlers, NAPI contexts, or other data path operations.
 *
 * Context: Must be called with rcu_read_lock held
 * Return: pointer to ath12k_dp_peer or NULL
 */
void *ath12k_sta_get_dp_peer_rcu(struct ath12k_sta *ahsta)
{
	/* Verify we're in the correct context (RCU read lock held) */
	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k sta get dp_peer without rcu lock");

	if (!ahsta)
		return NULL;

	return rcu_dereference(ahsta->dp_peer);
}
EXPORT_SYMBOL(ath12k_sta_get_dp_peer_rcu);

int ath12k_dp_peer_set_key_config(struct ath12k_pdev_dp *dp_pdev, const u8 *addr,
				  enum set_key_cmd cmd, struct ieee80211_key_conf *key,
				  struct ieee80211_sta *sta, struct ieee80211_vif *vif,
				  enum hal_encrypt_type *enctype)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = dp_pdev->dp_hw;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, sta->addr, vif);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr, vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	spin_lock_bh(&dp_peer->keys_lock);

	if (cmd == SET_KEY) {
		dp_peer->keys[key->keyidx] = key;
		if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE) {
			dp_peer->ucast_keyidx = key->keyidx;
			dp_peer->sec_type = ath12k_dp_tx_get_encrypt_type(key->cipher);
			*enctype = dp_peer->sec_type;
		} else {
			dp_peer->mcast_keyidx = key->keyidx;
			dp_peer->sec_type_grp =
					ath12k_dp_tx_get_encrypt_type(key->cipher);
			*enctype = dp_peer->sec_type_grp;
		}
	} else if (cmd == DISABLE_KEY) {
		dp_peer->keys[key->keyidx] = NULL;
		if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE)
			dp_peer->ucast_keyidx = 0;
		else
			dp_peer->mcast_keyidx = 0;
	}

	spin_unlock_bh(&dp_peer->keys_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return 0;
}

int ath12k_dp_link_peer_get_4addr_params(void *ptr, const u8 *addr,
					 struct ath12k_4addr_params *params)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, addr);
	if (!link_peer) {
		rcu_read_unlock();
		return -ENOENT;
	}

	params->tcl_metadata = link_peer->tcl_metadata;
	params->ast_hash = link_peer->ast_hash;
	params->hw_peer_id = link_peer->hw_peer_id;

	rcu_read_unlock();

	return 0;
}

bool ath12k_dp_peer_set_4addr_params(void *ptr, int ppe_vp_num)
{
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	dp_peer->vdev_type_4addr |= BIT(ath12k_dp_peer_get_vif_type(dp_peer));
	dp_peer->is_reset_mcbc = true;
	dp_peer->use_4addr = true;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	dp_peer->ppe_vp_num = ppe_vp_num;
#endif

	if (ath12k_dp_peer_get_vif_type(dp_peer) == NL80211_IFTYPE_AP)
		dp_peer->dev = ath12k_dp_peer_get_sta(dp_peer)->dev;

	return 0;
}

void
ath12k_dp_link_peer_iterate_by_dp_pdev(struct ath12k_pdev_dp *dp_pdev,
				       void (*iter_fn)(struct ath12k_pdev_dp *dp_pdev,
						       struct ath12k_dp_link_peer *peer,
						       void *context),
				       void *data)
{
	struct ath12k_dp_hw *dp_hw = dp_pdev->dp_hw;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	u32 start, end, i;

	if (dp->global_peer_id_supported) {
		start = 0;
		end = MAX_DP_PEER_LIST_SIZE;
	} else {
		start = dp->device_id << PEER_TABLE_SOC_ID_SHIFT;
		end = start + BIT(PEER_TABLE_SOC_ID_SHIFT);
	}

	rcu_read_lock();
	spin_lock_bh(&dp_hw->peer_hash_lock);

	for (i = start; i < end; i++) {
		dp_peer = rcu_dereference(dp_hw->dp_peer_list[i]);
		if (!dp_peer ||
		    dp_peer->dp_peer_state >= ATH12K_DP_PEER_LOGICALLY_DELETED)
			continue;

		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer,
								   dp_pdev->hw_link_id);
		if (!link_peer)
			continue;

		iter_fn(dp_pdev, link_peer, data);
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_dp_link_peer_iterate_by_dp_pdev);

void
ath12k_dp_link_peer_iterate_by_device(struct ath12k_dp *dp,
				      void (*iter_fn)(struct ath12k_pdev_dp *dp_pdev,
						      struct ath12k_dp_link_peer *peer,
						      void *context),
				      void *data)
{
	struct ath12k_pdev_dp *dp_pdev;
	int i;

	rcu_read_lock();

	for (i = 0; i < MAX_RADIOS; i++) {
		dp_pdev = rcu_dereference(dp->dp_pdevs[i]);
		if (!dp_pdev)
			continue;

		ath12k_dp_link_peer_iterate_by_dp_pdev(dp_pdev, iter_fn, data);
	}

	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_dp_link_peer_iterate_by_device);

void ath12k_dp_peer_iterate_by_vif(struct ath12k_dp_hw *dp_hw,
				   struct ieee80211_vif *vif,
				   void (*iter_fn)(struct ath12k_dp_peer *peer,
						   void *context),
				   void *data)
{
	struct ath12k_dp_peer *peer;

	if (!dp_hw || !vif || !iter_fn)
		return;

	spin_lock_bh(&dp_hw->peer_list_lock);

	list_for_each_entry(peer, &dp_hw->peers, list) {
		if (ath12k_dp_peer_get_vif(peer) != vif)
			continue;

		iter_fn(peer, data);
	}

	spin_unlock_bh(&dp_hw->peer_list_lock);
}
EXPORT_SYMBOL(ath12k_dp_peer_iterate_by_vif);

struct ath12k_mac_cleanup_ctx {
	struct ath12k    *ar;
	struct list_head  link_peers;
	struct list_head  dp_peers;
};

static void ath12k_mac_dp_peer_cleanup_cb(struct ath12k_pdev_dp *dp_pdev,
					  struct ath12k_dp_link_peer *link_peer,
					  void *data)
{
	struct ath12k_mac_cleanup_ctx *ctx = data;
	struct ath12k *ar = ctx->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_rx_tid *rx_tid;
	int i, num_tids = ab->hal.hal_params->num_tids;
	u16 peerid_index;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;

	spin_lock_bh(&dp->dp_lock);
#ifdef CPTCFG_QCN_EXTN
	ath12k_dp_ipa_peer_notify(ar, link_peer, NULL, link_peer->vdev_id, false);
#endif /* CPTCFG_QCN_EXTN */

	/* Mode3 (client-retaining) recovery: the asserted chip's MLO client
	 * peers are retained across the reload so the deferred link_readd phase
	 * can re-add their asserted links.  Only legacy/SLO client peers and
	 * self/vdev (AP) peers are torn down here; MLO client link_peers are
	 * left untouched.  is_vdev_peer identifies the AP self peer (always
	 * cleaned so the vdev can be recreated); is_mlo identifies an MLD
	 * client peer.
	 */
	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3 &&
	    link_peer->dp_peer &&
	    link_peer->dp_peer->is_mlo &&
	    !link_peer->dp_peer->is_vdev_peer) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	/*Skip this for non primary_links and vdev peers*/
	if (ath12k_dp_link_peer_get_sta(link_peer) && link_peer->dp_peer &&
	    ath12k_dp_arch_mlo_peer_tid_teardown_ready(dp, link_peer->dp_peer,
						       link_peer)) {
		for (i = 0; i < num_tids; i++) {
			rx_tid = &link_peer->dp_peer->rx_tid[i];

			spin_lock_bh(&rx_tid->tid_lock);
			ath12k_dp_arch_rx_peer_tid_delete(dp, ar, link_peer, i);
			ath12k_dp_rx_frags_cleanup(rx_tid, true);
			spin_unlock_bh(&rx_tid->tid_lock);
		}
	}

	/* cleanup dp peer */
	if (link_peer->dp_peer) {
		dp_peer = link_peer->dp_peer;

		if (!dp_peer->is_vdev_peer)
			dp_peer->peer_links_map &= ~BIT(link_peer->hw_link_id);

		rcu_assign_pointer(dp_peer->link_peers[link_peer->hw_link_id], NULL);

		if (link_peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
			peerid_index = ath12k_dp_peer_get_peerid_index(dp,
								       link_peer->peer_id);
			rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
		}

		/*vdev peer cleanup is taken care later*/
		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE2 &&
		    !dp_peer->is_vdev_peer && dp_peer->peer_links_map == 0) {
			ath12k_dbg_level(ab, ATH12K_DBG_MAC | ATH12K_DBG_PEER,
					 ATH12K_DBG_L0,
					 "dp_peer %pM dosent have active link_peers\n",
					 dp_peer->addr);
			if (dp_peer->is_mlo &&
			    dp_peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
				sta = ath12k_dp_peer_get_sta(dp_peer);
				ahsta = ath12k_sta_to_ahsta(sta);

				/* Clear bit for peer id in wifi8 case only */
				if (dp->global_peer_id_supported)
					clear_bit(dp_peer->peer_id,
						  dp_hw->free_peer_id_map);

				if (ahsta->ml_peer_id != ATH12K_MLO_PEER_ID_INVALID) {
					clear_bit(ahsta->ml_peer_id,
						  ah->free_ml_peer_id_map);
					ahsta->ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;
				}

				ah->num_ml_peers--;
			}

			/* NULL out ahsta->dp_peer before synchronize_rcu() */
			sta = ath12k_dp_peer_get_sta(dp_peer);
			if (sta) {
				ahsta = ath12k_sta_to_ahsta(sta);
				rcu_assign_pointer(ahsta->dp_peer, NULL);
			}

			if (dp_peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
				peerid_index = ath12k_dp_peer_get_peerid_index(dp,
									       dp_peer->peer_id);
				rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
			}

			ath12k_dp_peer_hash_table_delete(dp_hw, dp_peer);

			/* Remove ath12k_dp_peer from linked list */
			spin_lock_bh(&dp_hw->peer_list_lock);
			list_del(&dp_peer->list);
			list_del_init(&dp_peer->tqm_cleanup_node);
			list_add(&dp_peer->list, &ctx->dp_peers);
			spin_unlock_bh(&dp_hw->peer_list_lock);
		}
	}

	ath12k_dp_link_peer_htbl_delete(dp_pdev, link_peer);
	list_add(&link_peer->list, &ctx->link_peers);
	spin_unlock_bh(&dp->dp_lock);
}

void ath12k_dp_peer_cleanup_all(struct ath12k *ar)
{
	struct ath12k_dp_link_peer *link_peer, *tmp_link_peer;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_peer *dp_peer, *tmp_dp_peer;
	int i, num_tids;
	struct ath12k_hw *ah = ar->ah;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_mac_cleanup_ctx ctx = {.ar = ar};

	num_tids = ab->hal.hal_params->num_tids;

	INIT_LIST_HEAD(&ctx.link_peers);
	INIT_LIST_HEAD(&ctx.dp_peers);

	/* Phase 1: collect and unlink all peers via the RCU-safe iterator */
	ath12k_dp_link_peer_iterate_by_device(dp, ath12k_mac_dp_peer_cleanup_cb,
					      &ctx);
	synchronize_rcu();

	/*Link peer cleanup part*/
	list_for_each_entry_safe(link_peer, tmp_link_peer, &ctx.link_peers, list) {
		if (ath12k_dp_link_peer_get_sta(link_peer) && link_peer->dp_peer &&
		    ath12k_dp_arch_mlo_peer_tid_teardown_ready(dp, link_peer->dp_peer,
							       link_peer)) {
			for (i = 0; i < num_tids; i++) {
				rx_tid = &link_peer->dp_peer->rx_tid[i];

				del_timer_sync(&rx_tid->frag_timer);
			}
		}
		link_peer->dp_peer = NULL;
		ath12k_link_peer_free(link_peer);
	}

	/*Dp peer cleanup part*/
	list_for_each_entry_safe(dp_peer, tmp_dp_peer, &ctx.dp_peers, list) {
		list_del(&dp_peer->list);

		if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);
		if (dp_peer->sta_id != ATH12K_STA_ID_INVALID)
			clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);

		if (!dp_peer->peer_links_map) {
			kfree(dp_peer->qos);
			kfree(dp_peer);
		}
	}
}

void ath12k_dp_peer_pre_rcu_remove(struct ieee80211_hw *hw, struct ath12k_sta *ahsta)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	/*
	 * ── DP layer: clear dp_peer_list[] and link_peers[] ──
	 */
	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(hw->wiphy, ahsta);
	if (!dp_peer)
		return;

	if (dp_peer->is_mlo && dp_peer->peer_id < MAX_DP_PEER_LIST_SIZE) {
		rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], NULL);
		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "%s: sta=%pM cleared dp_peer_list[%u] mlo_peer_id\n",
				 __func__, dp_peer->addr, dp_peer->peer_id);
	}

	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		struct ath12k_dp_link_peer *link_peer;
		u16 peerid_index;

		link_peer = wiphy_dereference(hw->wiphy, dp_peer->link_peers[i]);
		if (!link_peer)
			continue;

		dp_peer->teardown_link_peers[i] = link_peer;
		rcu_assign_pointer(dp_peer->link_peers[i], NULL);

		if (link_peer->peer_id == ATH12K_MLO_PEER_ID_INVALID)
			continue;

		peerid_index = ath12k_dp_peer_get_peerid_index(link_peer->dp,
							       link_peer->peer_id);

		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);

		ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "%s: sta=%pM cleared dp_peer_list[%u] hw_link_id=%u peer_id=%u\n",
				 __func__, link_peer->addr, peerid_index, i,
				 link_peer->peer_id);
	}

	rcu_assign_pointer(ahsta->dp_peer, NULL);

	dp_peer->pre_rcu_remove_done = true;

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "%s: dp_peer=%pM done mlo=%d\n",
			 __func__, dp_peer->addr, dp_peer->is_mlo);
}

void ath12k_dp_peer_cleanup_by_addr(struct ath12k *ar, int vdev_id,
				    const u8 *addr, struct ieee80211_sta *sta)
{
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, sta->addr, sta);

	if (dp_peer)
		ath12k_dp_peer_cleanup(ar, dp_peer, vdev_id, addr);

	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

void ath12k_dp_peer_unauthorize(struct ath12k *ar, struct ieee80211_sta *sta,
				u8 link_id)
{
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	u8 hw_link_id;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, sta->addr, sta);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	dp_peer->is_authorized = false;

	hw_link_id = ath12k_dp_peer_convert_logical_to_hw_link_id(dp_peer, link_id);
	if (hw_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS) {
		link_peer = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
					      dp_peer->link_peers[hw_link_id]);
		if (!link_peer)
			link_peer = dp_peer->teardown_link_peers[hw_link_id];
		if (link_peer)
			link_peer->is_authorized = false;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

int ath12k_dp_peer_clear_keys(struct ath12k *ar, struct ieee80211_sta *sta,
			      union ath12k_config_param *key_params)
{
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, sta->addr, sta);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	ath12k_dp_peer_set_param_by_dp_peer(dp_peer,
					    ATH12K_DP_PEER_CLEAR_KEYS_PARAM,
					    key_params);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return 0;
}
