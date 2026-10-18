/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WIFI8_DP_EXTN_H
#define WIFI8_DP_EXTN_H

#include "../../core.h"
#include "../dp_tx.h"
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#include "ipa/dp_ipa.h"
#endif

int ath12k_wifi8_tx_setup_link(struct ieee80211_vif *vif,
                               struct ieee80211_sta *sta,
                               struct ieee80211_tx_info *info,
                               struct sk_buff *skb,
                               u8 *link_id);
/**
 * ath12k_wifi8_dp_tx_fetch_wds_link_id() - Recover link_id for WDS/3-addr ETH
 *					    offload frames missing the MLO
 *					    assist tag
 * @ahvif: VIF context, used to walk the multi-SoC hw group for peer lookup
 * @sta: Station pointer; non-NULL only for WDS/4-addr frames
 * @eth: Ethernet header of @skb; h_dest used for peer address lookup
 * @link_id: Output link_id on success
 *
 * WDS/4-addr: sta is valid; h_dest is the client behind the WDS peer.
 * Recover link_id from primary_link_id.
 * Regular unicast: sta is NULL; h_dest is the peer's own link MAC — look it
 * up in dp->link_peer_htbl.
 *
 * Return: true on success with @link_id populated, false otherwise
 */
static inline
bool ath12k_wifi8_dp_tx_fetch_link_id(struct ath12k_vif *ahvif,
				      struct sk_buff *skb, u8 *link_id)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_hw_group *ag = ahvif->ah->ag;
	struct ieee80211_tx_info info_tx = {0};
	bool found = false;
	u8 soc;

	for (soc = 0; soc < ATH12K_MAX_SOCS; soc++) {
		struct ath12k_dp *dp = ag->dp_hw_grp->dp[soc];

		if (!dp)
			continue;

		spin_lock_bh(&dp->dp_lock);
		link_peer = ath12k_dp_link_peer_htbl_find_by_addr(dp, eth->h_dest);
		if (link_peer && link_peer->dp_peer) {
			memcpy(&info_tx, info, sizeof(*info));
			if (!ath12k_wifi8_tx_setup_link(ahvif->vif, link_peer->dp_peer->sta,
						   &info_tx, skb, link_id))
				found = true;

			spin_unlock_bh(&dp->dp_lock);
			return found;
		}
		spin_unlock_bh(&dp->dp_lock);
	}

	return false;
}

/**
 * ath12k_wifi8_dp_tx_check_fast_path() - Handle fast-path TX frames
 * @skb: Socket buffer to be transmitted
 * @info_flags: ieee80211 TX control flags
 * @dp_vif: Datapath VIF context
 * @skb_ctrl: Per-skb control structure to set offload/fast-path flags
 * @qos_nw_delay: QoS network delay metadata used by fast-path handler
 * @vlan_ahvif: VLAN VIF context for fast-path handler
 * @sta: Station pointer; non-NULL only for WDS/4-addr frames
 *
 * Checks if @skb carries fast-path markers (fast_xmit + MLO assist tag) and is
 * eligible for the architecture-specific unicast fast path. When eligible, it
 * sets skb control features/flags (e.g. DP_ETH_OFFLOAD, DP_SKB_RECYCLED) and
 * calls ath12k_dp_arch_ucast_handler().
 *
 * For ETH offload frames where the MLO assist tag is missing, link_id is
 * recovered via sta->primary_link_id (WDS) or h_dest lookup in
 * dp->link_peer_htbl (regular unicast).
 *
 * Return: true if the skb was consumed by the fast path, false otherwise
 */
static inline
bool ath12k_wifi8_dp_tx_check_fast_path(struct sk_buff *skb, u32 info_flags,
					struct ath12k_dp_vif *dp_vif,
					struct ath12k_dp_skb_ctrl *skb_ctrl,
					u32 qos_nw_delay,
					struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = NULL;
	bool is_recycler = false;
	u8 link_id;

	if (likely(skb->fast_xmit)) {
		/* Primary: extract link_id from MLO assist tag in skb->mark */
		if (likely((skb->mark & ATH12K_MLO_METADATA_MLO_ASSIST_TAG_MASK) ==
			   ATH12K_MLO_METADATA_MLO_ASSIST_TAG)) {
			link_id = u32_get_bits(skb->mark,
					       ATH12K_MLO_METADATA_LINKID_MASK);

		} else if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
			if (unlikely(!ath12k_wifi8_dp_tx_fetch_link_id(ahvif,
								       skb, &link_id)))
				goto drop;
		} else {
			goto drop;
		}

		/* Validate link_id bounds before array access */
		if (unlikely(link_id >= ATH12K_NUM_MAX_LINKS ||
			     (ATH12K_SCAN_LINKS_MASK & BIT(link_id))))
			goto drop;

		arvif = rcu_dereference(ahvif->link[link_id]);
		if (unlikely(!arvif || !arvif->ar))
			goto drop;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
		/* ULSO: try IPA before TCL */
		struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);

		memset(skb_cb, 0, sizeof(*skb_cb));
		skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
		if (ath12k_wifi8_dp_ipa_xmit(arvif->ar, skb, skb_cb,
					      ahvif, NULL, arvif,
					      false) == 0)
			return true;
		/* IPA not eligible or error — fall through to TCL */
#endif

		skb_ctrl->flags |= DP_SKB_FAST_TX;
		is_recycler = (skb->fast_recycled = !!skb->is_from_recycler);
		if (likely(is_recycler))
			skb_ctrl->flags |= DP_SKB_RECYCLED;
		ath12k_wifi8_ucast_handler(dp_vif, link_id, NULL, skb, skb_ctrl,
					   qos_nw_delay, vlan_ahvif, NULL);
		return true;

drop:
		ath12k_mac_ieee80211_free_txskb(ahvif->ah->hw, skb, NULL, NULL, dp_vif,
						DP_TX_ENQ_DROP_INV_ENCAP_FAST,
						0, false);
		return true;
	}

	return false;
}

#endif // WIFI8_DP_EXTN_H
