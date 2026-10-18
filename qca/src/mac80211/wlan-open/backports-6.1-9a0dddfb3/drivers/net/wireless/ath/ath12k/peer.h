/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_PEER_H
#define ATH12K_PEER_H

#include "debug.h"
#include "dp_peer.h"

#define ATH12K_PEER_DELETE_TIMEOUT_MS 10000
#define ATH12K_PEER_DELETE_MGMT_TX_WAIT_MS 1000
/* Timeout for waiting on peer deletion tracker during peer create (in ms) */
#define ATH12K_PEER_DEL_TRACKER_TIMEOUT_MS 3000
/*
 * Max entries processed per vdev clear in deletion tracker.
 * Use the maximum supported clients per pdev to ensure we can
 * collect all pending deletions in one pass. Keep this aligned
 * with the highest supported client count in hw params/module
 * param (e.g., ath12k_max_clients default: 512).
 */
#define ATH12K_PEER_DEL_TRACKER_MAX_ENTRIES 512

#define ATH12K_CP_PEER_HASHTABLE_DEFAULT_ENTRIES 128

/* Hash key structure for peer deletion tracking */
struct ath12k_peer_del_key {
	u8 addr[ETH_ALEN];
};

/* Structure to track peers pending deletion */
struct ath12k_peer_del_entry {
	u32 vdev_id;
	u8 addr[ETH_ALEN];
	u8 mld_addr[ETH_ALEN];
	struct rhash_head rhash_node;
	struct rhash_head mld_rhash_node;
	struct rcu_head rcu_head;
	struct timer_list timer;
	struct ath12k_pdev *pdev;
};

/* Structure to manage peer deletion tracking at pdev level */
struct ath12k_peer_del_tracker {
	struct rhashtable peer_del_hash;
	struct rhashtable mld_del_hash;
	/* Protects rhashtable ops on peer_del_hash, mld_del_hash and entry lifecycle */
	spinlock_t lock;
	struct rhashtable_params hash_params;
	struct rhashtable_params mld_hash_params;
	wait_queue_head_t hash_delete_queue;
};

void ath12k_peer_cleanup(struct ath12k *ar, u32 vdev_id);
int ath12k_peer_delete(struct ath12k *ar, struct ath12k_link_vif *arvif, u8 *addr,
		       bool skip_peer_del, u32 mlo_hw_link_id_bitmap,
		       bool peer_delete_send_mlo_hw_bitmap, struct ieee80211_sta *sta);
int ath12k_peer_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
		       struct ieee80211_sta *sta,
		       struct ath12k_wmi_peer_create_arg *arg);
int ath12k_wait_for_peer_delete_done(struct ath12k *ar, u32 vdev_id,
				     const u8 *addr);
int ath12k_peer_mlo_link_peers_delete(struct ath12k_vif *ahvif, struct ath12k_sta *ahsta);
int ath12k_peer_send_assoc_vendor_response(const struct ath12k_dp_link_peer *peer,
					   const bool is_assoc);

int ath12k_link_sta_hlist_init(struct ath12k *ar);
void ath12k_link_sta_hlist_head_destroy(struct ath12k *ar);
void ath12k_link_sta_hlist_destroy(struct ath12k *ar);
bool ath12k_link_sta_hlist_empty(struct ath12k *ar);
void ath12k_link_sta_hlist_delete(struct ath12k *ar, struct ath12k_link_sta *arsta);
int ath12k_link_sta_hlist_add(struct ath12k *ar, struct ath12k_link_sta *arsta);
struct ath12k_link_sta *ath12k_link_sta_find_by_addr(struct ath12k *ar, const u8 *addr);
struct ath12k_link_sta *ath12k_link_sta_find_by_addr_vdev_id(struct ath12k *ar,
							     const u8 *addr,
							     u32 vdev_id);
bool ath12k_peer_deauth_disassoc_tx_inc(struct ath12k *ar, const u8 *addr);
void ath12k_peer_deauth_disassoc_tx_dec(struct ath12k *ar, const u8 *addr);
int ath12k_wait_pending_deauth_disassoc_tx(struct ath12k *ar, const u8 *addr,
					   unsigned long timeout_ms);

/**
 * ath12k_link_sta_for_each - iterate over all link STAs on a radio
 * @_ar:    struct ath12k * whose arsta_list is walked
 * @_bkt:   u32 bucket counter declared by the caller
 * @_arsta: loop cursor (struct ath12k_link_sta *)
 *
 * Caller must hold @_ar->arsta_lock.
 */
#define ath12k_link_sta_for_each(_ar, _bkt, _arsta)			\
	for ((_bkt) = 0;						\
	     (_ar)->arsta_list && (_bkt) < BIT((_ar)->arsta_hash_bits);	\
	     (_bkt)++)							\
		hlist_for_each_entry((_arsta),				\
				 &(_ar)->arsta_list[(_bkt)], hlist_addr)

/**
 * ath12k_link_sta_for_each_safe - iterate over all link STAs, safe against removal
 * @_ar:    struct ath12k * whose arsta_list is walked
 * @_bkt:   u32 bucket counter declared by the caller
 * @_arsta: loop cursor (struct ath12k_link_sta *)
 * @_tmp:   struct hlist_node * for safe-iteration temporary storage
 *
 * Caller must hold @_ar->arsta_lock.  Saves the next-node pointer so the lock
 * may be dropped inside the body and re-acquired before the next iteration.
 */
#define ath12k_link_sta_for_each_safe(_ar, _bkt, _arsta, _tmp)		\
	for ((_bkt) = 0;						\
	     (_ar)->arsta_list && (_bkt) < BIT((_ar)->arsta_hash_bits);	\
	     (_bkt)++)							\
		hlist_for_each_entry_safe((_arsta), (_tmp),		\
				 &(_ar)->arsta_list[(_bkt)], hlist_addr)

/**
 * ath12k_ahsta_for_each - iterate over all MLD/STA entries in ag->ahsta_list
 * @_ag:    struct ath12k_hw_group * whose ahsta_list is walked
 * @_bkt:   u32 bucket counter declared by the caller
 * @_ahsta: loop cursor (struct ath12k_sta *)
 *
 * Caller must hold @_ag->ahsta_lock.
 */
#define ath12k_ahsta_for_each(_ag, _bkt, _ahsta)			\
	for ((_bkt) = 0;						\
	     (_ag)->ahsta_list && (_bkt) < BIT((_ag)->ahsta_hash_bits);	\
	     (_bkt)++)							\
		hlist_for_each_entry((_ahsta),				\
				 &(_ag)->ahsta_list[(_bkt)], hlist_addr)

void ath12k_mac_peer_disassoc(struct ath12k_base *ab, struct ieee80211_sta *sta,
			      struct ath12k_sta *ahsta,
			      enum ath12k_debug_mask debug_mask);
int ath12k_peer_dp_cp_link_peer_delete(struct ath12k_link_vif *arvif,
				       struct ath12k_sta *ahsta, u8 link_id,
				       u8 *addr, u32 mlo_hw_link_id_bitmap,
				       bool peer_delete_send_mlo_hw_bitmap,
				       bool update_bmap);

/* Peer deletion tracking functions */
int ath12k_peer_del_tracker_init(struct ath12k_pdev *pdev);
void ath12k_peer_del_tracker_destroy(struct ath12k_pdev *pdev);
int ath12k_peer_del_tracker_add(struct ath12k_pdev *pdev, u32 vdev_id,
				const u8 *addr, const u8 *mld_addr);
void ath12k_peer_del_tracker_remove(struct ath12k_pdev *pdev, u32 vdev_id,
				    const u8 *addr);
bool ath12k_peer_del_tracker_is_mlo(struct ath12k_pdev *pdev, u32 vdev_id,
				    const u8 *addr);
int ath12k_peer_del_tracker_check(struct ath12k_pdev *pdev, const u8 *addr,
				  const u8 *mld_addr);
int ath12k_peer_del_tracker_clear_vdev(struct ath12k_pdev *pdev, u32 vdev_id);
int ath12k_peer_del_tracker_clear_pdev(struct ath12k_pdev *pdev);
int ath12k_peer_del_tracker_wait(struct ath12k_pdev *pdev, const u8 *addr,
				 unsigned long timeout_ms, const u8 *mld_addr);
int ath12k_get_peer_telemetry_stats(struct ath12k_vif *ahvif,
				    struct ath12k_telemetry_dp_peer *telemetry_peer,
				    u8 *addr, u8 link_id);
void ath12k_vif_iterate_peer(struct ath12k_link_vif *arvif,
			     struct ath12k_dp_aggr_vif_stats *aggr_vif_stats);

static inline
struct ath12k_link_sta *ath12k_peer_get_link_sta(struct ath12k_base *ab,
						 struct ath12k_dp_link_peer *peer)
{
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return NULL;

	ahsta = ath12k_sta_to_ahsta(ath12k_dp_link_peer_get_sta(peer));
	if (peer->ml_id & ATH12K_PEER_ML_ID_VALID) {
		if (!(ahsta->links_map & BIT(peer->link_id))) {
			ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
					 "peer %pM id %d link_id %d can't found in STA link_map 0x%x\n",
					 peer->addr, peer->peer_id, peer->link_id,
					 ahsta->links_map);
			return NULL;
		}
		arsta = rcu_dereference(ahsta->link[peer->link_id]);
		if (!arsta)
			return NULL;
		} else {
			arsta =  &ahsta->deflink;
		}
	return arsta;
}

/**
 * typedef ath12k_arsta_iter_cb - callback for ath12k_arsta_itr_on_ab_by_addr()
 * @ar:    radio on which @arsta was found
 * @arsta: the matching link STA (found under @ar->arsta_lock)
 * @data:  caller-supplied opaque context
 *
 * Called with @ar->arsta_lock held (BH-disabled).
 */
typedef void (*ath12k_arsta_iter_cb)(struct ath12k *ar,
				     struct ath12k_link_sta *arsta,
				     void *data);

/**
 * typedef ath12k_arsta_vdev_iter_cb - callback for ath12k_arsta_itr_on_ar_by_vdev_id()
 * @ar:    radio on which @arsta was found
 * @arsta: the matching link STA (found under @ar->arsta_lock)
 * @data:  caller-supplied opaque context
 *
 * Called with @ar->arsta_lock held (BH-disabled). Return 0 to continue
 * iterating, negative errno to stop and propagate the error.
 */
typedef int (*ath12k_arsta_vdev_iter_cb)(struct ath12k *ar,
					 struct ath12k_link_sta *arsta,
					 void *data);

bool ath12k_arsta_itr_on_ab_by_addr(struct ath12k_base *ab, const u8 *addr,
				    ath12k_arsta_iter_cb cb, void *data);
int ath12k_arsta_itr_on_ar_by_vdev_id(struct ath12k *ar, u32 vdev_id,
				      ath12k_arsta_vdev_iter_cb cb, void *data);
/* ahsta (ath12k_sta) group-level hashtable */
int ath12k_sta_hlist_init(struct ath12k_hw_group *ag);
void ath12k_sta_hlist_head_destroy(struct ath12k_hw_group *ag);
void ath12k_sta_hlist_destroy(struct ath12k_hw_group *ag);
void ath12k_sta_hlist_destroy_with_no_ar(struct ath12k_hw_group *ag);
int ath12k_sta_hlist_add(struct ath12k_hw_group *ag, struct ath12k_sta *ahsta);
int ath12k_sta_hlist_delete(struct ath12k_hw_group *ag, struct ath12k_sta *ahsta);
struct ath12k_sta *ath12k_sta_find_by_addr(struct ath12k_hw_group *ag,
					   const u8 *addr);
struct ath12k_sta *ath12k_sta_find_by_addr_and_ahvif(struct ath12k_hw_group *ag,
						     const u8 *addr,
						     const struct ath12k_vif *ahvif);
/* CP-level pre-emptive duplicate peer sanity check */
int ath12k_mac_addr_collision_check(struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    const u8 *link_mac,
				    bool is_self_peer,
				    struct ath12k_sta *ahsta);
struct ath12k_link_sta *ath12k_link_sta_find_by_vdev_id(struct ath12k *ar,
							u32 vdev_id);
void ath12k_sta_update_primary_link(struct wiphy *wiphy,
				    struct ath12k_sta *ahsta, u8 link_id);
int ath12k_wait_for_peer_create_done(struct ath12k *ar, u32 vdev_id,
				     const u8 *addr);
void ath12k_bcast_probe_rl_init(struct ath12k *ar);
void ath12k_bcast_probe_rl_flush(struct ath12k *ar);
#endif /* _PEER_H_ */
