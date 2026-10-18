// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/slab.h>
#include "core.h"
#include "wmi.h"
#include "mac.h"
#include "debug.h"
#include "vendor.h"
#include "peer.h"
#include "ranging.h"

u32 ath12k_supported_rtt_responder_roles(struct ath12k_base *ab)
{
	u32 role = ATH12K_RTT_11MC_RESPONDER_ROLE;

	if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT,
		     ab->wmi_ab.svc_map))
		role |= ATH12K_RTT_11AZ_NTB_RESPONDER_ROLE;

	if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT,
		     ab->wmi_ab.svc_map))
		role |= ATH12K_RTT_11AZ_TB_RESPONDER_ROLE;

	return role;
}

static bool ath12k_11az_rsta_supported(struct ath12k_base *ab)
{
	return test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT, ab->wmi_ab.svc_map) ||
		test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT, ab->wmi_ab.svc_map);
}

static bool ath12k_11az_mac_sec_supported(struct ath12k_base *ab)
{
	return ath12k_11az_rsta_supported(ab) &&
		(test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_SEC_SUPPORT, ab->wmi_ab.svc_map) ||
		 test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_PHY_SEC_SUPPORT,
			  ab->wmi_ab.svc_map));
}

static bool ath12k_11az_mac_phy_sec_supported(struct ath12k_base *ab)
{
	return ath12k_11az_rsta_supported(ab) &&
		test_bit(WMI_TLV_SERVICE_RTT_11AZ_MAC_PHY_SEC_SUPPORT,
			 ab->wmi_ab.svc_map);
}

void ath12k_mac_set_ranging_ext_features(struct wiphy *wiphy, struct ath12k *ar)
{
	if (ath12k_11az_mac_sec_supported(ar->ab)) {
		wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SECURE_RTT);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE);
	}

	if (ath12k_11az_mac_phy_sec_supported(ar->ab))
		wiphy_ext_feature_set(wiphy, NL80211_EXT_FEATURE_SECURE_LTF);
}

/**
 * ath12k_mac_setup_iftype_11az_ranging() - Advertise AP 11az ext-caps.
 * @ah: ath12k hardware group.
 *
 * The static ath12k iftype capability table is shared by all hardware
 * instances and also carries immutable template data used by registration.
 * 11az NTB/TB responder bits depend on firmware service bits, so update a
 * per-wiphy copy instead of modifying the global template in place.
 *
 * Return: 0 on success or a negative errno on allocation failure.
 */
int ath12k_mac_setup_iftype_11az_ranging(struct ath12k_hw *ah)
{
	const struct wiphy_iftype_ext_capab *tmpl, *ap_tmpl;
	struct wiphy_iftype_ext_capab *iftype_ext_capa;
	struct ath12k *ar, *tmp_ar;
	struct wiphy *wiphy;
	u8 *ap_ext_capa;
	unsigned int n_capab;
	u8 ranging = 0;
	u8 ap_len;
	int i, ap_idx = -1;

	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar || ar->rtt_capab.rtt_iftype_ext_capab)
		return 0;

	for_each_ar(ah, tmp_ar, i) {
		if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_NTB_SUPPORT,
			     tmp_ar->ab->wmi_ab.svc_map))
			ranging |= ATH12K_EXT_CAPA12_NTB_RANGING_RESPONDER;

		if (test_bit(WMI_TLV_SERVICE_RTT_11AZ_TB_RSTA_SUPPORT,
			     tmp_ar->ab->wmi_ab.svc_map))
			ranging |= ATH12K_EXT_CAPA12_TB_RANGING_RESPONDER;
	}

	if (!ranging)
		return 0;

	wiphy = ah->hw->wiphy;
	tmpl = wiphy->iftype_ext_capab;
	n_capab = wiphy->num_iftype_ext_capab;
	if (!tmpl || !n_capab)
		return 0;

	for (i = 0; i < n_capab; i++) {
		if (tmpl[i].iftype == NL80211_IFTYPE_AP) {
			ap_idx = i;
			break;
		}
	}

	if (ap_idx < 0)
		return 0;

	ap_tmpl = &tmpl[ap_idx];
	ap_len = max_t(u8, ap_tmpl->extended_capabilities_len,
		       ATH12K_EXT_CAPA_11AZ_RANGING_IDX + 1);
	iftype_ext_capa = kmemdup(tmpl, sizeof(*tmpl) * n_capab, GFP_KERNEL);
	if (!iftype_ext_capa)
		return -ENOMEM;

	ap_ext_capa = kzalloc(ap_len, GFP_KERNEL);
	if (!ap_ext_capa) {
		kfree(iftype_ext_capa);
		return -ENOMEM;
	}

	if (ap_tmpl->extended_capabilities)
		memcpy(ap_ext_capa, ap_tmpl->extended_capabilities,
		       ap_tmpl->extended_capabilities_len);

	ap_ext_capa[ATH12K_EXT_CAPA_11AZ_RANGING_IDX] |= ranging;
	iftype_ext_capa[ap_idx].extended_capabilities = ap_ext_capa;
	iftype_ext_capa[ap_idx].extended_capabilities_mask = ap_ext_capa;
	iftype_ext_capa[ap_idx].extended_capabilities_len = ap_len;

	ar->rtt_capab.rtt_iftype_ext_capab_orig = tmpl;
	ar->rtt_capab.rtt_num_iftype_ext_capab_orig = n_capab;
	ar->rtt_capab.rtt_iftype_ext_capab = iftype_ext_capa;
	ar->rtt_capab.rtt_ap_ext_capab = ap_ext_capa;
	wiphy->iftype_ext_capab = iftype_ext_capa;
	wiphy->num_iftype_ext_capab = n_capab;

	return 0;
}

/**
 * ath12k_mac_cleanup_iftype_11az_ranging() - Release RTT ext-cap copy.
 * @ah: ath12k hardware group.
 *
 * Restore the wiphy back to the original iftype capability table before
 * freeing the dynamically allocated AP extended capability buffer.
 */
void ath12k_mac_cleanup_iftype_11az_ranging(struct ath12k_hw *ah)
{
	struct ath12k *ar = ath12k_ah_to_ar(ah, 0);
	struct wiphy *wiphy = ah->hw->wiphy;

	if (!ar || !ar->rtt_capab.rtt_iftype_ext_capab)
		return;

	if (wiphy->iftype_ext_capab == ar->rtt_capab.rtt_iftype_ext_capab) {
		wiphy->iftype_ext_capab = ar->rtt_capab.rtt_iftype_ext_capab_orig;
		wiphy->num_iftype_ext_capab =
			ar->rtt_capab.rtt_num_iftype_ext_capab_orig;
	}

	kfree(ar->rtt_capab.rtt_ap_ext_capab);
	kfree(ar->rtt_capab.rtt_iftype_ext_capab);
	ar->rtt_capab.rtt_ap_ext_capab = NULL;
	ar->rtt_capab.rtt_iftype_ext_capab = NULL;
	ar->rtt_capab.rtt_iftype_ext_capab_orig = NULL;
	ar->rtt_capab.rtt_num_iftype_ext_capab_orig = 0;
}

void ath12k_pasn_peer_delete(struct ath12k_link_vif *arvif,
			     const u8 *peer_addr)
{
	struct ath12k_rtt_pasn_peer *peer;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	peer = ath12k_pasn_peer_find(arvif, peer_addr);
	if (peer) {
		list_del(&peer->list);
		kfree(peer);
	}
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);
}

void ath12k_pasn_peer_set_auth_status(struct ath12k_link_vif *arvif,
				      const u8 *src_addr, const u8 *peer_addr,
				      bool auth_success)
{
	struct ath12k_rtt_pasn_peer *peer;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	peer = ath12k_pasn_peer_find(arvif, peer_addr);
	if (peer) {
		if (auth_success && src_addr)
			ether_addr_copy(peer->src_addr, src_addr);
		if (auth_success)
			peer->flags |= ATH12K_PASN_F_AUTH_SUCCESS;
		else
			peer->flags &= ~ATH12K_PASN_F_AUTH_SUCCESS;
	}
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);
}

bool ath12k_pasn_peer_auth_status_sent(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr)
{
	return !!(ath12k_pasn_peer_update_flags(arvif, peer_addr, 0, 0) &
		  ATH12K_PASN_F_AUTH_STATUS_SENT);
}

void ath12k_pasn_peer_set_auth_status_sent(struct ath12k_link_vif *arvif,
					   const u8 *peer_addr)
{
	ath12k_pasn_peer_update_flags(arvif, peer_addr,
				      ATH12K_PASN_F_AUTH_STATUS_SENT, 0);
}

struct ath12k_rtt_pasn_peer *
ath12k_pasn_peer_find(struct ath12k_link_vif *arvif, const u8 *peer_addr)
{
	struct ath12k_rtt_pasn_peer *peer;

	lockdep_assert_held(&arvif->rtt_ctx.pasn_peer_lock);

	list_for_each_entry(peer, &arvif->rtt_ctx.pasn_peer_list, list) {
		if (ether_addr_equal(peer->peer_addr, peer_addr))
			return peer;
	}

	return NULL;
}

/**
 * ath12k_pasn_peer_update_flags() - Atomically set/clear peer flag bits.
 * @arvif: link vif owning the PASN peer list.
 * @peer_addr: MAC address of the peer to update.
 * @set_mask: flag bits to set.
 * @clr_mask: flag bits to clear.
 *
 * Returns the updated flags value, or 0 if the peer was not found.
 */
u8 ath12k_pasn_peer_update_flags(struct ath12k_link_vif *arvif,
				 const u8 *peer_addr,
				 u8 set_mask, u8 clr_mask)
{
	struct ath12k_rtt_pasn_peer *peer;
	u8 result = 0;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	peer = ath12k_pasn_peer_find(arvif, peer_addr);
	if (peer) {
		peer->flags = (peer->flags | set_mask) & ~clr_mask;
		result = peer->flags;
	}
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);

	return result;
}

bool ath12k_pasn_peer_is_fw_created(struct ath12k_link_vif *arvif,
				    const u8 *peer_addr)
{
	return !!(ath12k_pasn_peer_update_flags(arvif, peer_addr, 0, 0) &
		  ATH12K_PASN_F_FW_CREATED);
}

static unsigned int ath12k_pasn_peer_count(struct ath12k_link_vif *arvif)
{
	struct ath12k_rtt_pasn_peer *peer;
	unsigned int count = 0;

	lockdep_assert_held(&arvif->rtt_ctx.pasn_peer_lock);

	list_for_each_entry(peer, &arvif->rtt_ctx.pasn_peer_list, list)
		count++;

	return count;
}

/**
 * ath12k_pasn_peer_create_or_update() - Add or update a SW PASN peer entry.
 * @arvif: link vif owning the PASN peer list.
 * @src_addr: local MAC address for this session, or NULL to keep existing.
 * @peer_addr: remote MAC address of the PASN peer (key for lookup).
 * @ltf_keyseed_required: true if the initiator requires LTF key seed.
 * @security_mode: PASN security mode (none / MAC / MAC+PHY).
 *
 * Creates a new tracking entry if the peer is not yet known, or updates
 * the negotiation parameters of an existing entry. Flags are preserved
 * across updates so auth/create progress is not reset.
 *
 * Returns 0 on success, -EINVAL if @peer_addr is invalid, -ENOMEM on
 * allocation failure.
 */
int ath12k_pasn_peer_create_or_update(struct ath12k_link_vif *arvif,
				      const u8 *src_addr, const u8 *peer_addr,
				      bool ltf_keyseed_required, u8 security_mode)
{
	struct ath12k_rtt_pasn_peer *peer;

	if (!peer_addr || is_zero_ether_addr(peer_addr))
		return -EINVAL;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	peer = ath12k_pasn_peer_find(arvif, peer_addr);
	if (!peer) {
		if (ath12k_pasn_peer_count(arvif) >=
		    ATH12K_MAX_PASN_PEERS_PER_VAP) {
			spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);
			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN peer limit reached vdev %u max %u peer %pM\n",
				   arvif->vdev_id, ATH12K_MAX_PASN_PEERS_PER_VAP,
				   peer_addr);
			return -ENOSPC;
		}

		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer) {
			spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);
			return -ENOMEM;
		}
		ether_addr_copy(peer->peer_addr, peer_addr);
		list_add_tail(&peer->list, &arvif->rtt_ctx.pasn_peer_list);
	}
	if (src_addr)
		ether_addr_copy(peer->src_addr, src_addr);
	peer->ltf_keyseed_required = ltf_keyseed_required;
	peer->security_mode = security_mode;
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);

	return 0;
}

/**
 * ath12k_pasn_fw_peer_create() - Create a PASN firmware peer for ranging.
 * @arvif: link vif on which to create the peer.
 * @peer_addr: MAC address of the PASN peer.
 *
 * Sends WMI_PEER_CREATE with peer_type WMI_PEER_TYPE_PASN and waits for
 * firmware confirmation via the peer-map event. Skips silently if the
 * peer is already created (ATH12K_PASN_F_FW_CREATED is set).
 *
 * Must be called with wiphy_lock held.
 *
 * Returns 0 on success or a negative errno on failure.
 */
int ath12k_pasn_fw_peer_create(struct ath12k_link_vif *arvif,
			       const u8 *peer_addr)
{
	struct ath12k_wmi_peer_create_arg peer_param = {};
	struct ath12k *ar = arvif->ar;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_create: peer=%pM vdev=%u\n",
		   peer_addr, arvif->vdev_id);

	if (ath12k_pasn_peer_is_fw_created(arvif, peer_addr)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN fw_peer_create: %pM already FW-created, skip\n",
			   peer_addr);
		return 0;
	}

	peer_param.vdev_id = arvif->vdev_id;
	peer_param.peer_addr = (u8 *)peer_addr;
	peer_param.peer_type = WMI_PEER_TYPE_PASN;
	peer_param.peer_id = ATH12K_MLO_PEER_ID_INVALID;
	peer_param.sta_id = ATH12K_STA_ID_INVALID;

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_create: sending WMI peer create %pM vdev=%u type=PASN\n",
		   peer_addr, arvif->vdev_id);

	reinit_completion(&ar->peer_create_conf);
	ar->peer_create_status = ATH12K_WMI_PEER_CREATE_STATUS_MAX;

	memset(&ar->peer_map_event, 0, sizeof(ar->peer_map_event));
	ether_addr_copy(ar->peer_map_event.pending_peer_addr, peer_addr);
	ar->peer_map_event.pending_peer_vdev_id = arvif->vdev_id;
	ar->peer_map_event.received = false;
	ar->peer_map_event.peer_id = ATH12K_MLO_PEER_ID_INVALID;

	ret = ath12k_wmi_send_peer_create_cmd(ar, &peer_param);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send RTT PASN peer create WMI %pM vdev %u: %d\n",
			    peer_addr, arvif->vdev_id, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_create: WMI sent, waiting for FW confirm %pM\n",
		   peer_addr);

	ret = ath12k_wait_for_peer_create_done(ar, arvif->vdev_id, peer_addr);
	if (ret) {
		ath12k_warn(ar->ab,
			    "RTT PASN peer create conf timed out %pM vdev %u: %d\n",
			    peer_addr, arvif->vdev_id, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_create: FW confirmed peer %pM vdev=%u\n",
		   peer_addr, arvif->vdev_id);

	ath12k_pasn_peer_update_flags(arvif, peer_addr,
				      ATH12K_PASN_F_FW_CREATED, 0);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_create: done %pM vdev=%u\n",
		   peer_addr, arvif->vdev_id);

	return 0;
}

/**
 * ath12k_pasn_fw_peer_delete() - Delete a PASN firmware peer.
 * @arvif: link vif owning the peer.
 * @peer_addr: remote peer MAC address.
 * @skip_peer_del: true if firmware peer delete was already handled.
 *
 * Skips WMI peer delete during firmware recovery (crash flush) or when
 * @skip_peer_del is true.
 * Clears ATH12K_PASN_F_FW_CREATED.
 *
 * Returns 0 on success or a negative errno on failure.
 */
int ath12k_pasn_fw_peer_delete(struct ath12k_link_vif *arvif,
			       const u8 *peer_addr, bool skip_peer_del)
{
	struct ath12k *ar = arvif->ar;
	int ret = 0;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_delete: peer=%pM\n", peer_addr);

	if (!peer_addr || !ath12k_pasn_peer_is_fw_created(arvif, peer_addr)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN fw_peer_delete: %pM not FW-created, skip\n",
			   peer_addr);
		return 0;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_delete: %pM vdev=%u crash_flush=%d skip=%u\n",
		   peer_addr, arvif->vdev_id,
		   test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags),
		   skip_peer_del);

	if (!test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags) &&
	    !skip_peer_del) {
		if (ar->pdev->peer_del_tracker) {
			ret = ath12k_peer_del_tracker_add(ar->pdev, arvif->vdev_id,
							  peer_addr, NULL);
			if (ret) {
				ath12k_warn(ar->ab,
					    "failed to track RTT PASN peer delete %pM vdev %u: %d\n",
					    peer_addr, arvif->vdev_id, ret);
				return ret;
			}
		}

		ret = ath12k_wmi_send_peer_delete_cmd(ar, peer_addr,
						      arvif->vdev_id, 0, false);
		if (ret) {
			ath12k_warn(ar->ab,
				    "failed to delete RTT PASN fw peer %pM vdev %u: %d\n",
				    peer_addr, arvif->vdev_id, ret);
			if (ar->pdev->peer_del_tracker)
				ath12k_peer_del_tracker_remove(ar->pdev, arvif->vdev_id,
							       peer_addr);
			/*
			 * WMI delete failed — leave FW_CREATED set so
			 * ath12k_rtt_pasn_peer_delete_all() can still
			 * issue WMI_VDEV_DELETE_ALL_PEER_CMDID on vif teardown
			 * and avoid leaving an orphaned PASN peer in firmware.
			 */
			return ret;
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN fw_peer_delete: WMI delete sent %pM vdev=%u\n",
			   peer_addr, arvif->vdev_id);
	}

	ath12k_pasn_peer_update_flags(arvif, peer_addr,
				      0, ATH12K_PASN_F_FW_CREATED);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN fw_peer_delete: done %pM vdev=%u\n",
		   peer_addr, arvif->vdev_id);
	return ret;
}

/**
 * ath12k_pasn_fw_peer_create_work() - wiphy work to create PASN FW peers.
 * @wiphy: wiphy the work is running under (wiphy_lock held).
 * @work: embedded wiphy_work inside ath12k_rtt_context.
 *
 * Runs in the wiphy workqueue to create firmware peers for all SW-tracked
 * PASN peers that are pending creation. For secured peers, sends a
 * QCA_WLAN_VENDOR_PASN_ACTION_AUTH vendor event to hostapd after the FW
 * peer is confirmed. Tears down the FW peer immediately if the vendor
 * event fails.
 */
void ath12k_pasn_fw_peer_create_work(struct wiphy *wiphy,
				     struct wiphy_work *work)
{
	struct ath12k_rtt_context *rtt_ctx =
		container_of(work, struct ath12k_rtt_context,
			     pasn_fw_peer_create_work);
	struct ath12k_link_vif *arvif =
		container_of(rtt_ctx, struct ath12k_link_vif, rtt_ctx);
	struct ath12k_rtt_pasn_peer *peer, *next_peer;
	spinlock_t *lock = &rtt_ctx->pasn_peer_lock;
	const u8 *src_addr;
	u8 security_mode;
	int ret;

	lockdep_assert_wiphy(wiphy);

	if (!arvif->ar)
		return;

	/* Snapshot all peers pending FW creation under the spinlock. */
	spin_lock_bh(lock);
	list_for_each_entry(peer, &rtt_ctx->pasn_peer_list, list) {
		if (!(peer->flags & ATH12K_PASN_F_FW_CREATED))
			peer->flags |= ATH12K_PASN_F_FW_CREATE_PENDING;
	}
	spin_unlock_bh(lock);

	/* Now create FW peers outside the spinlock (WMI can sleep).
	 * Use list_for_each_entry_safe so a concurrent peer-delete that runs
	 * between spin_unlock_bh / spin_lock_bh cannot invalidate the iterator.
	 */
	spin_lock_bh(lock);
	list_for_each_entry_safe(peer, next_peer, &rtt_ctx->pasn_peer_list, list) {
		if (!(peer->flags & ATH12K_PASN_F_FW_CREATE_PENDING))
			continue;
		peer->flags &= ~ATH12K_PASN_F_FW_CREATE_PENDING;
		spin_unlock_bh(lock);

		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN work: creating FW peer %pM vdev=%u\n",
			   peer->peer_addr, arvif->vdev_id);

		security_mode = peer->security_mode;
		src_addr = peer->src_addr;

		ret = ath12k_pasn_fw_peer_create(arvif, peer->peer_addr);
		if (ret) {
			ath12k_warn(arvif->ar->ab,
				    "RTT PASN work: failed FW peer create %pM: %d\n",
				    peer->peer_addr, ret);
			spin_lock_bh(lock);
			continue;
		}

		if (security_mode != ATH12K_WMI_RTT_PASN_SECURITY_MODE_NONE) {
			int vevent_ret;

			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN work: FW peer ready, sending auth vendor event %pM sec=%u ltf=%d\n",
				   peer->peer_addr, security_mode,
				   peer->ltf_keyseed_required);
			vevent_ret = ath12k_vendor_send_pasn_event(
					ath12k_ar_to_hw(arvif->ar)->wiphy,
					ieee80211_vif_to_wdev(arvif->ahvif->vif),
					arvif->link_id,
					QCA_WLAN_VENDOR_PASN_ACTION_AUTH,
					src_addr, peer->peer_addr,
					peer->ltf_keyseed_required);
			if (vevent_ret) {
				ath12k_warn(arvif->ar->ab,
					    "RTT PASN work: failed vendor event %pM: %d, deleting FW peer\n",
					    peer->peer_addr, vevent_ret);
				/* hostapd will never run PASN; tear down FW peer now. */
				ath12k_pasn_fw_peer_delete(arvif, peer->peer_addr, false);
			} else {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN work: auth vendor event sent %pM\n",
					   peer->peer_addr);
			}
		} else {
			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN work: %pM is open/unsecure, FW peer created, no vendor event\n",
				   peer->peer_addr);
		}

		spin_lock_bh(lock);
	}
	spin_unlock_bh(lock);
}

void ath12k_pasn_peers_cleanup(struct ath12k_link_vif *arvif)
{
	struct ath12k_rtt_context *rtt_ctx = &arvif->rtt_ctx;
	struct ath12k_rtt_pasn_peer *peer, *tmp;

	ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN cleanup: freeing SW peer list vdev=%u\n",
		   arvif->vdev_id);

	spin_lock_bh(&rtt_ctx->pasn_peer_lock);
	list_for_each_entry_safe(peer, tmp, &rtt_ctx->pasn_peer_list, list) {
		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN cleanup: freeing peer %pM vdev=%u\n",
			   peer->peer_addr, arvif->vdev_id);
		list_del(&peer->list);
		kfree(peer);
	}
	spin_unlock_bh(&rtt_ctx->pasn_peer_lock);
}

/**
 * ath12k_rtt_pasn_peer_delete_all() - Delete all PASN FW peers and free SW list.
 * @arvif: link vif whose PASN peers to tear down.
 *
 * Sends WMI_VDEV_DELETE_ALL_PEER_CMDID scoped to WMI_PEER_TYPE_PASN and
 * waits up to three seconds for firmware confirmation. On timeout, calls
 * ath12k_peer_cleanup() to recover the local peer table. Finally frees
 * all SW peer tracking entries via ath12k_pasn_peers_cleanup().
 *
 * Called during vif teardown with wiphy_lock held.
 */
static void ath12k_rtt_pasn_peer_delete_all(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_rtt_pasn_peer *peer;
	bool pasn_fw_peer_found = false;
	int ret;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	list_for_each_entry(peer, &arvif->rtt_ctx.pasn_peer_list, list) {
		if (peer->flags & ATH12K_PASN_F_FW_CREATED) {
			pasn_fw_peer_found = true;
			break;
		}
	}
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN delete_all: arvif=%p vdev=%u pasn_fw_peer_found=%d\n",
		   arvif, arvif->vdev_id, pasn_fw_peer_found);
	if (!pasn_fw_peer_found) {
		ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN delete_all: no peers on vdev=%u, skip\n",
			   arvif->vdev_id);
		goto cleanup;
	}

	reinit_completion(&ar->delete_all_peer_done);
	ret = ath12k_wmi_pasn_peer_delete_all(arvif);
	if (ret) {
		ath12k_warn(ar->ab,
			    "RTT PASN delete_all: WMI send failed vdev=%u: %d\n",
			    arvif->vdev_id, ret);
		goto cleanup;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN delete_all: WMI sent vdev=%u, waiting for FW confirm\n",
		   arvif->vdev_id);

	if (!wait_for_completion_timeout(&ar->delete_all_peer_done, 3 * HZ)) {
		ath12k_warn(ar->ab,
			    "RTT PASN delete_all: timeout vdev=%u, cleaning up\n",
			    arvif->vdev_id);
		ath12k_peer_cleanup(ar, arvif->vdev_id);
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN delete_all: FW confirmed vdev=%u\n",
			   arvif->vdev_id);
	}

cleanup:
	ath12k_pasn_peers_cleanup(arvif);

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN delete_all: done vdev=%u", arvif->vdev_id);
}

void ath12k_rtt_init_link_vif(struct ath12k_link_vif *arvif)
{
	INIT_LIST_HEAD(&arvif->rtt_ctx.pasn_peer_list);
	spin_lock_init(&arvif->rtt_ctx.pasn_peer_lock);
	wiphy_work_init(&arvif->rtt_ctx.pasn_fw_peer_create_work,
			ath12k_pasn_fw_peer_create_work);
}

void ath12k_rtt_deinit_link_vif(struct ath12k_link_vif *arvif)
{
	if (!arvif->ar)
		return;
	wiphy_work_cancel(ath12k_ar_to_hw(arvif->ar)->wiphy,
			  &arvif->rtt_ctx.pasn_fw_peer_create_work);
	ath12k_rtt_pasn_peer_delete_all(arvif);
}

void ath12k_rtt_reconfig_responder_role(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;
	u32 role;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		struct ath12k_vif *ahvif = arvif->ahvif;

		if (!arvif->is_started || !ahvif)
			continue;

		if (ahvif->vdev_type != WMI_VDEV_TYPE_AP)
			continue;

		if (!arvif->ftm_responder)
			continue;

		role = arvif->rtt_ctx.rtt_responder_role;
		if (!role)
			continue;

		ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
			WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_RESPONDER_ROLE,
			role);
		if (ret)
			ath12k_warn(ar->ab,
				    "failed to restore RTT responder role 0x%x on vdev %i after FW recovery: %d\n",
				    role, arvif->vdev_id, ret);
		else
			ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
				   "restored RTT responder role 0x%x on vdev %i link %u after FW recovery\n",
				   role, arvif->vdev_id, arvif->link_id);
	}
}

/**
 * ath12k_pasn_peer_set_secure_ctx() - Update the secure-context-installed
 *	flag for a PASN peer.
 * @arvif: link vif owning the PASN peer list.
 * @src_addr: local MAC address for the session, or NULL to keep existing.
 * @peer_addr: remote peer MAC address.
 * @installed: true if the TK has been installed, false to clear.
 */
void ath12k_pasn_peer_set_secure_ctx(struct ath12k_link_vif *arvif,
				     const u8 *src_addr, const u8 *peer_addr,
				     bool installed)
{
	struct ath12k_rtt_pasn_peer *peer;

	if (!peer_addr)
		return;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	peer = ath12k_pasn_peer_find(arvif, peer_addr);
	if (peer) {
		if (src_addr)
			ether_addr_copy(peer->src_addr, src_addr);
		if (installed)
			peer->flags |= ATH12K_PASN_F_SECURE_CTX;
		else
			peer->flags &= ~ATH12K_PASN_F_SECURE_CTX;
	}
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);
}

/**
 * ath12k_pasn_peer_set_ltf_keyseed() - Update the LTF-keyseed-installed
 *	flag for a PASN peer.
 * @arvif: link vif owning the PASN peer list.
 * @peer_addr: remote peer MAC address.
 * @installed: true if the LTF key seed has been installed, false to clear.
 */
void ath12k_pasn_peer_set_ltf_keyseed(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr, bool installed)
{
	u8 mask = installed ? ATH12K_PASN_F_LTF_KEYSEED : 0;
	u8 clr  = installed ? 0 : ATH12K_PASN_F_LTF_KEYSEED;

	ath12k_pasn_peer_update_flags(arvif, peer_addr, mask, clr);
}
