/*
 * hostapd / Callback functions for driver wrappers
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "radius/radius.h"
#include "drivers/driver.h"
#include "common/qca-vendor.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/dpp.h"
#include "common/sae.h"
#include "common/hw_features_common.h"
#include "common/nan_de.h"
#include "crypto/random.h"
#include "p2p/p2p.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "wnm_ap.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "ieee802_11_auth.h"
#include "uhr_utils.h"
#include "sta_info.h"
#include "accounting.h"
#include "tkip_countermeasures.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "wpa_auth_glue.h"
#include "wps_hostapd.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "ap_mlme.h"
#include "hw_features.h"
#include "dfs.h"
#include "beacon.h"
#include "mbo_ap.h"
#include "dpp_hostapd.h"
#include "fils_hlp.h"
#include "neighbor_db.h"
#include "nan_usd_ap.h"
#include "interference.h"
#include "ttlm.h"
#include "robust_av.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#include "ap/uhr_utils.h"
#include "uhr_neighbor_update.h"

#ifdef CONFIG_IEEE80211BN
#include "mapc.h"
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_FILS
void hostapd_notify_assoc_fils_finish(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	u16 reply_res = WLAN_STATUS_SUCCESS;
	struct ieee802_11_elems elems;
	u8 buf[IEEE80211_MAX_MMPDU_SIZE], *p = buf;
	int new_assoc;
	bool updated;

	wpa_printf(MSG_DEBUG, "%s FILS: Finish association with " MACSTR,
		   __func__, MAC2STR(sta->addr));
	eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
	if (!sta->fils_pending_assoc_req)
		return;

	if (ieee802_11_parse_elems(sta->fils_pending_assoc_req,
				   sta->fils_pending_assoc_req_len, &elems,
				   0) == ParseFailed ||
	    !elems.fils_session) {
		wpa_printf(MSG_DEBUG, "%s failed to find FILS Session element",
			   __func__);
		return;
	}

	p = hostapd_eid_assoc_fils_session(sta->wpa_sm, p,
					   elems.fils_session,
					   sta->fils_hlp_resp);
	if (!p)
		return;

	reply_res = hostapd_sta_assoc(hapd, sta->addr,
				      sta->fils_pending_assoc_is_reassoc,
				      WLAN_STATUS_SUCCESS,
				      buf, p - buf);
	updated = ap_sta_set_authorized_flag(hapd, sta, 1);
	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;
	hostapd_set_sta_flags(hapd, sta);
	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);
	wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FILS);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);
	os_free(sta->fils_pending_assoc_req);
	sta->fils_pending_assoc_req = NULL;
	sta->fils_pending_assoc_req_len = 0;
	wpabuf_free(sta->fils_hlp_resp);
	sta->fils_hlp_resp = NULL;
	wpabuf_free(sta->hlp_dhcp_discover);
	sta->hlp_dhcp_discover = NULL;
	fils_hlp_deinit(hapd);

	/*
	 * Remove the station in case transmission of a success response fails
	 * (the STA was added associated to the driver) or if the station was
	 * previously added unassociated.
	 */
	if (reply_res != WLAN_STATUS_SUCCESS || sta->added_unassoc) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}
#endif /* CONFIG_FILS */


static bool check_sa_query_need(struct hostapd_data *hapd, struct sta_info *sta)
{
	if ((sta->flags &
	     (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED))
		return false;

	if (!sta->sa_query_timed_out && sta->sa_query_count > 0)
		ap_check_sa_query_timeout(hapd, sta);

	if (!sta->sa_query_timed_out && (sta->auth_alg != WLAN_AUTH_FT)) {
		/*
		 * STA has already been associated with MFP and SA Query timeout
		 * has not been reached. Reject the association attempt
		 * temporarily and start SA Query, if one is not pending.
		 */
		if (sta->sa_query_count == 0)
			ap_sta_start_sa_query(hapd, sta);

		return true;
	}

	return false;
}


#ifdef CONFIG_IEEE80211BE
static int hostapd_update_sta_links_status(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   const u8 *resp_ies,
					   size_t resp_ies_len)
{
	struct mld_info *info = &sta->mld_info;
	struct wpabuf *mlebuf;
	const u8 *mle, *pos;
	struct ieee802_11_elems elems;
	size_t mle_len, rem_len;
	int ret = 0;

	if (!resp_ies) {
		wpa_printf(MSG_DEBUG,
			   "MLO: (Re)Association Response frame elements not available");
		return -1;
	}

	if (ieee802_11_parse_elems(resp_ies, resp_ies_len, &elems, 0) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLO: Failed to parse (Re)Association Response frame elements");
		return -1;
	}

	mlebuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len, true);
	if (!mlebuf) {
		wpa_printf(MSG_ERROR,
			   "MLO: Basic Multi-Link element not found in (Re)Association Response frame");
		return -1;
	}

	mle = wpabuf_head(mlebuf);
	mle_len = wpabuf_len(mlebuf);
	if (mle_len < MULTI_LINK_CONTROL_LEN + 1 ||
	    mle_len - MULTI_LINK_CONTROL_LEN < mle[MULTI_LINK_CONTROL_LEN]) {
		wpa_printf(MSG_ERROR,
			   "MLO: Invalid Multi-Link element in (Re)Association Response frame");
		ret = -1;
		goto out;
	}

	/* Skip Common Info */
	pos = mle + MULTI_LINK_CONTROL_LEN + mle[MULTI_LINK_CONTROL_LEN];
	rem_len = mle_len -
		(MULTI_LINK_CONTROL_LEN + mle[MULTI_LINK_CONTROL_LEN]);

	/* Parse Subelements */
	while (rem_len > 2) {
		size_t ie_len, subelem_defrag_len;
		int num_frag_subelems;

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlebuf, pos,
						      &subelem_defrag_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Failed to parse MLE subelem");
			break;
		}

		ie_len = 2 + subelem_defrag_len;
		rem_len -= num_frag_subelems * 2;

		if (rem_len < ie_len)
			break;

		if (pos[0] == MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
			u8 link_id;
			const u8 *sta_profile;
			size_t sta_profile_len;
			u16 sta_ctrl;

			if (subelem_defrag_len < BASIC_MLE_STA_CTRL_LEN + 1) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid per-STA profile IE");
				goto next_subelem;
			}

			sta_profile_len = subelem_defrag_len;
			sta_profile = &pos[2];
			sta_ctrl = WPA_GET_LE16(sta_profile);
			link_id = sta_ctrl & BASIC_MLE_STA_CTRL_LINK_ID_MASK;
			if (link_id >= MAX_NUM_MLD_LINKS) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid link ID in per-STA profile IE");
				goto next_subelem;
			}

			/* Skip STA Control and STA Info */
			if (sta_profile_len - BASIC_MLE_STA_CTRL_LEN <
			    sta_profile[BASIC_MLE_STA_CTRL_LEN]) {
				wpa_printf(MSG_DEBUG,
					   "MLO: Invalid STA info in per-STA profile IE");
				goto next_subelem;
			}

			sta_profile_len = sta_profile_len -
				(BASIC_MLE_STA_CTRL_LEN +
				 sta_profile[BASIC_MLE_STA_CTRL_LEN]);
			sta_profile = sta_profile + BASIC_MLE_STA_CTRL_LEN +
				sta_profile[BASIC_MLE_STA_CTRL_LEN];

			/* Skip Capabilities Information field */
			if (sta_profile_len < 2)
				goto next_subelem;
			sta_profile_len -= 2;
			sta_profile += 2;

			/* Get status of the link */
			info->links[link_id].status = WPA_GET_LE16(sta_profile);
		}
next_subelem:
		pos += ie_len;
		rem_len -= ie_len;
	}

out:
	wpabuf_free(mlebuf);
	return ret;
}
#endif /* CONFIG_IEEE80211BE */


#if defined(HOSTAPD) || defined(CONFIG_IEEE80211BE)
static struct hostapd_data * hostapd_find_by_sta(struct hostapd_iface *iface,
						 const u8 *src, bool rsn,
						 struct sta_info **sta_ret)
{
	struct hostapd_data *hapd;
	struct sta_info *sta;
	unsigned int j;

	if (sta_ret)
		*sta_ret = NULL;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		sta = ap_get_sta(hapd, src);
		/*wpa_sm is not allocated if Assoc Request contains WPS_IE
		 *So Ignore checking for sta->wpa_sm when sta flags have
		 *WLAN_STA_WPS set
		 */
		if (sta && (sta->flags & WLAN_STA_ASSOC) &&
		    (!rsn|| (sta->flags & WLAN_STA_WPS) || sta->wpa_sm)) {
			if (sta_ret)
				*sta_ret = sta;
			return hapd;
		}
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			struct hostapd_data *p_hapd;

			for_each_mld_link(p_hapd, hapd) {
				if (p_hapd == hapd)
					continue;
				/*wpa_sm is not allocated if Assoc Request contains WPS_IE
				 *So Ignore checking for sta->wpa_sm when sta flags have
				 *WLAN_STA_WPS set
				 */

				sta = ap_get_sta(p_hapd, src);
				if (sta && (sta->flags & WLAN_STA_ASSOC) &&
				    (!rsn ||(sta->flags & WLAN_STA_WPS) || sta->wpa_sm)) {
					if (sta_ret)
						*sta_ret = sta;
					return p_hapd;
				}
			}
		}
#endif /* CONFIG_IEEE80211BE */
	}

	return NULL;
}
#endif /* HOSTAPD || CONFIG_IEEE80211BE */


int hostapd_notif_assoc(struct hostapd_data *hapd, const u8 *addr,
			const u8 *req_ies, size_t req_ies_len,
			const u8 *resp_ies, size_t resp_ies_len,
			const u8 *link_addr, int reassoc)
{
	struct sta_info *sta;
	int new_assoc;
	enum wpa_validate_result res;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ielen;
	u8 buf[sizeof(struct ieee80211_mgmt) + 1024];
	u8 *p = buf;
	u16 reason = WLAN_REASON_UNSPECIFIED;
	int status = WLAN_STATUS_SUCCESS;
	const u8 *p2p_dev_addr = NULL;
#ifdef CONFIG_OWE
	struct hostapd_iface *iface = hapd->iface;
#endif /* CONFIG_OWE */
	bool updated = false;
	bool driver_acl;
	struct hostapd_ubus_request req = {
		.type = HOSTAPD_UBUS_ASSOC_REQ,
		.addr = addr,
	};

#ifdef CONFIG_P2P
	if (hapd->p2p_group && (!hapd->started || hapd->disabled)) {
		wpa_printf(MSG_DEBUG,
			   "hostapd_notif_assoc: Ignore assoc event - P2P GO not started or disabled");
		return 0;
	}
#endif /* CONFIG_P2P */

	if (addr == NULL) {
		/*
		 * This could potentially happen with unexpected event from the
		 * driver wrapper. This was seen at least in one case where the
		 * driver ended up being set to station mode while hostapd was
		 * running, so better make sure we stop processing such an
		 * event here.
		 */
		wpa_printf(MSG_DEBUG,
			   "hostapd_notif_assoc: Skip event with no address");
		return -1;
	}

	if (is_multicast_ether_addr(addr) ||
	    is_zero_ether_addr(addr) ||
	    ether_addr_equal(addr, hapd->own_addr)) {
		/* Do not process any frames with unexpected/invalid SA so that
		 * we do not add any state for unexpected STA addresses or end
		 * up sending out frames to unexpected destination. */
		wpa_printf(MSG_DEBUG, "%s: Invalid SA=" MACSTR
			   " in received indication - ignore this indication silently",
			   __func__, MAC2STR(addr));
		return 0;
	}

	random_add_randomness(addr, ETH_ALEN);

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "associated");

	if (ieee802_11_parse_elems(req_ies, req_ies_len, &elems, 0) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG, "%s: Could not parse elements", __func__);
		return -1;
	}

	if (elems.wps_ie) {
		ie = elems.wps_ie - 2;
		ielen = elems.wps_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included WPS IE in (Re)AssocReq");
	} else if (elems.rsn_ie) {
		ie = elems.rsn_ie - 2;
		ielen = elems.rsn_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included RSN IE in (Re)AssocReq");
	} else if (elems.wpa_ie) {
		ie = elems.wpa_ie - 2;
		ielen = elems.wpa_ie_len + 2;
		wpa_printf(MSG_DEBUG, "STA included WPA IE in (Re)AssocReq");
	} else {
		ie = NULL;
		ielen = 0;
		wpa_printf(MSG_DEBUG,
			   "STA did not include WPS/RSN/WPA IE in (Re)AssocReq");
	}

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		ap_sta_no_session_timeout(hapd, sta);
		accounting_sta_stop(hapd, sta);

		/*
		 * Make sure that the previously registered inactivity timer
		 * will not remove the STA immediately.
		 */
		sta->timeout_next = STA_NULLFUNC;
	} else {
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL) {
			hostapd_drv_sta_disassoc(hapd, addr,
						 WLAN_REASON_DISASSOC_AP_BUSY);
			return -1;
		}
	}

	if (hapd->conf->wpa && check_sa_query_need(hapd, sta)) {
		status = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
		p = hostapd_eid_assoc_comeback_time(hapd, sta, p);
		hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);

		return 0;
	}

#ifdef CONFIG_IEEE80211BE
	if (link_addr) {
		struct mld_info *info = &sta->mld_info;
		int i, num_valid_links = 0;
		u8 link_id = hapd->mld_link_id;

		ap_sta_set_mld(sta, true);
		sta->mld_assoc_link_id = link_id;
		os_memcpy(info->common_info.mld_addr, addr, ETH_ALEN);
		info->links[link_id].valid = true;
		os_memcpy(info->links[link_id].peer_addr, link_addr, ETH_ALEN);
		os_memcpy(info->links[link_id].local_addr, hapd->own_addr,
			  ETH_ALEN);

		if (!elems.basic_mle ||
		    hostapd_process_ml_assoc_req(hapd, &elems, sta) !=
		    WLAN_STATUS_SUCCESS) {
			reason = WLAN_REASON_UNSPECIFIED;
			wpa_printf(MSG_DEBUG,
				   "Failed to get STA non-assoc links info");
			goto fail;
		}

		for (i = 0 ; i < MAX_NUM_MLD_LINKS; i++) {
			if (info->links[i].valid)
				num_valid_links++;
		}
		if (num_valid_links > 1 &&
		    hostapd_update_sta_links_status(hapd, sta, resp_ies,
						    resp_ies_len)) {
			wpa_printf(MSG_DEBUG,
				   "Failed to get STA non-assoc links status info");
			reason = WLAN_REASON_UNSPECIFIED;
			goto fail;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);

	/*
	 * ACL configurations to the drivers (implementing AP SME and ACL
	 * offload) without hostapd's knowledge, can result in a disconnection
	 * though the driver accepts the connection. Skip the hostapd check for
	 * ACL if the driver supports ACL offload to avoid potentially
	 * conflicting ACL rules.
	 */
	driver_acl = hapd->iface->drv_max_acl_mac_addrs > 0;
#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap)
		driver_acl = false;
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	if (!driver_acl &&
	    hostapd_check_acl(hapd, addr, NULL) != HOSTAPD_ACL_ACCEPT) {
		wpa_printf(MSG_INFO, "STA " MACSTR " not allowed to connect",
			   MAC2STR(addr));
		reason = WLAN_REASON_UNSPECIFIED;
		goto fail;
	}
#ifdef CONFIG_IEEE80211BE
	/*
	 * The idea is that ACL is per link. For MLO associations, check
	 * whether peer MLD MAC address is acceptable in all requested links.
	 * For each peer link address, check the corresponding association
	 * local link's ACL configuration whether it is acceptable.
	 */
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (!driver_acl && hapd->conf->mld_ap) {
		if (hostapd_check_ml_acl(hapd, sta) != HOSTAPD_ACL_ACCEPT) {
			reason = WLAN_REASON_UNSPECIFIED;
			goto fail;
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	if (hostapd_ubus_handle_event(hapd, &req)) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " assoc rejected by ubus handler.\n",
			   MAC2STR(req.addr));
		goto fail;
	}

#ifdef CONFIG_P2P
	if (elems.p2p) {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = ieee802_11_vendor_ie_concat(req_ies, req_ies_len,
							  P2P_IE_VENDOR_TYPE);
		if (sta->p2p_ie)
			p2p_dev_addr = p2p_get_go_dev_addr(sta->p2p_ie);
	}
#endif /* CONFIG_P2P */

#ifdef NEED_AP_MLME
	if (elems.ht_capabilities &&
	    (hapd->iface->conf->ht_capab &
	     HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)) {
		struct ieee80211_ht_capabilities *ht_cap =
			(struct ieee80211_ht_capabilities *)
			elems.ht_capabilities;

		if (le_to_host16(ht_cap->ht_capabilities_info) &
		    HT_CAP_INFO_40MHZ_INTOLERANT)
			ht40_intolerant_add(hapd->iface, sta);
	}
#endif /* NEED_AP_MLME */

	check_ext_capab(hapd, sta, elems.ext_capab, elems.ext_capab_len);

#ifdef CONFIG_HS20
	wpabuf_free(sta->hs20_ie);
	if (elems.hs20 && elems.hs20_len > 4) {
		sta->hs20_ie = wpabuf_alloc_copy(elems.hs20 + 4,
						 elems.hs20_len - 4);
	} else
		sta->hs20_ie = NULL;

	wpabuf_free(sta->roaming_consortium);
	if (elems.roaming_cons_sel)
		sta->roaming_consortium = wpabuf_alloc_copy(
			elems.roaming_cons_sel + 4,
			elems.roaming_cons_sel_len - 4);
	else
		sta->roaming_consortium = NULL;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	wpabuf_free(sta->mb_ies);
	if (hapd->iface->fst)
		sta->mb_ies = mb_ies_by_info(&elems.mb_ies);
	else
		sta->mb_ies = NULL;
#endif /* CONFIG_FST */

	mbo_ap_check_sta_assoc(hapd, sta, &elems);

	ap_copy_sta_supp_op_classes(sta, elems.supp_op_classes,
				    elems.supp_op_classes_len);

	if (hapd->conf->wpa) {
		if (ie == NULL || ielen == 0) {
#ifdef CONFIG_WPS
			if (hapd->conf->wps_state) {
				wpa_printf(MSG_DEBUG,
					   "STA did not include WPA/RSN IE in (Re)Association Request - possible WPS use");
				sta->flags |= WLAN_STA_MAYBE_WPS;
				goto skip_wpa_check;
			}
#endif /* CONFIG_WPS */

			wpa_printf(MSG_DEBUG, "No WPA/RSN IE from STA");
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			goto fail;
		}
#ifdef CONFIG_WPS
		if (hapd->conf->wps_state && ie[0] == 0xdd && ie[1] >= 4 &&
		    os_memcmp(ie + 2, "\x00\x50\xf2\x04", 4) == 0) {
			struct wpabuf *wps;

			sta->flags |= WLAN_STA_WPS;
			wps = ieee802_11_vendor_ie_concat(ie, ielen,
							  WPS_IE_VENDOR_TYPE);
			if (wps) {
				if (wps_is_20(wps)) {
					wpa_printf(MSG_DEBUG,
						   "WPS: STA supports WPS 2.0");
					sta->flags |= WLAN_STA_WPS2;
				}
				wpabuf_free(wps);
			}
			goto skip_wpa_check;
		}
#endif /* CONFIG_WPS */

		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr,
							p2p_dev_addr);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_ERROR,
				   "Failed to initialize WPA state machine");
			return -1;
		}
		wpa_auth_set_rsn_selection(sta->wpa_sm, elems.rsn_selection,
					   elems.rsn_selection_len);
#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Set ML info in RSN Authenticator");
			wpa_auth_set_ml_info(sta->wpa_sm,
					     sta->mld_assoc_link_id,
					     &sta->mld_info);
		}
#endif /* CONFIG_IEEE80211BE */
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  hapd->iface->freq,
					  ie, ielen,
					  elems.rsnxe ? elems.rsnxe - 2 : NULL,
					  elems.rsnxe ? elems.rsnxe_len + 2 : 0,
					  elems.mdie, elems.mdie_len,
					  elems.owe_dh, elems.owe_dh_len, NULL,
					  ap_sta_is_mld(hapd, sta), false, NULL, false);
		reason = WLAN_REASON_INVALID_IE;
		status = WLAN_STATUS_INVALID_IE;
		switch (res) {
		case WPA_IE_OK:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_SUCCESS;
			break;
		case WPA_INVALID_IE:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_GROUP:
			reason = WLAN_REASON_GROUP_CIPHER_NOT_VALID;
			status = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
			break;
		case WPA_INVALID_PAIRWISE:
			reason = WLAN_REASON_PAIRWISE_CIPHER_NOT_VALID;
			status = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
			break;
		case WPA_INVALID_AKMP:
			reason = WLAN_REASON_AKMP_NOT_VALID;
			status = WLAN_STATUS_AKMP_NOT_VALID;
			break;
		case WPA_NOT_ENABLED:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_ALLOC_FAIL:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			break;
		case WPA_MGMT_FRAME_PROTECTION_VIOLATION:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_MGMT_GROUP_CIPHER:
			reason = WLAN_REASON_CIPHER_SUITE_REJECTED;
			status = WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
			break;
		case WPA_INVALID_MDIE:
			reason = WLAN_REASON_INVALID_MDE;
			status = WLAN_STATUS_INVALID_MDIE;
			break;
		case WPA_INVALID_PROTO:
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			break;
		case WPA_INVALID_PMKID:
			reason = WLAN_REASON_INVALID_PMKID;
			status = WLAN_STATUS_INVALID_PMKID;
			break;
		case WPA_DENIED_OTHER_REASON:
			reason = WLAN_REASON_UNSPECIFIED;
			status = WLAN_STATUS_ASSOC_DENIED_UNSPEC;
			break;
		}
		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "WPA/RSN information element rejected? (res %u)",
				   res);
			wpa_hexdump(MSG_DEBUG, "IE", ie, ielen);
			goto fail;
		}

		if (wpa_auth_uses_mfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_MFP;
		else
			sta->flags &= ~WLAN_STA_MFP;

		if (wpa_auth_uses_spp_amsdu(sta->wpa_sm))
			sta->flags |= WLAN_STA_SPP_AMSDU;
		else
			sta->flags &= ~WLAN_STA_SPP_AMSDU;

		if (wpa_auth_uses_cfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_CFP;
		else
			sta->flags &= ~WLAN_STA_CFP;

#ifdef CONFIG_IEEE80211R_AP
		if (sta->auth_alg == WLAN_AUTH_FT) {
			status = wpa_ft_validate_reassoc(sta->wpa_sm, req_ies,
							 req_ies_len, NULL);
			if (status != WLAN_STATUS_SUCCESS) {
				if (status == WLAN_STATUS_INVALID_PMKID)
					reason = WLAN_REASON_INVALID_IE;
				if (status == WLAN_STATUS_INVALID_MDIE)
					reason = WLAN_REASON_INVALID_IE;
				if (status == WLAN_STATUS_INVALID_FTIE)
					reason = WLAN_REASON_INVALID_IE;
				goto fail;
			}
		}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
		if (hapd->conf->sae_pwe == SAE_PWE_BOTH &&
		    sta->auth_alg == WLAN_AUTH_SAE &&
		    sta->sae && !sta->sae->h2e &&
		    ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len,
					      WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_INFO, "SAE: " MACSTR
				   " indicates support for SAE H2E, but did not use it",
				   MAC2STR(sta->addr));
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			reason = WLAN_REASON_UNSPECIFIED;
			goto fail;
		}
#endif /* CONFIG_SAE */

		wpa_auth_set_ssid_protection(
			sta->wpa_sm,
			hapd->conf->ssid_protection &&
			ieee802_11_rsnx_capab_len(
				elems.rsnxe, elems.rsnxe_len,
				WLAN_RSNX_CAPAB_SSID_PROTECTION));
	} else if (hapd->conf->wps_state) {
#ifdef CONFIG_WPS
		struct wpabuf *wps;

		if (req_ies)
			wps = ieee802_11_vendor_ie_concat(req_ies, req_ies_len,
							  WPS_IE_VENDOR_TYPE);
		else
			wps = NULL;
#ifdef CONFIG_WPS_STRICT
		if (wps && wps_validate_assoc_req(wps) < 0) {
			reason = WLAN_REASON_INVALID_IE;
			status = WLAN_STATUS_INVALID_IE;
			wpabuf_free(wps);
			goto fail;
		}
#endif /* CONFIG_WPS_STRICT */
		if (wps) {
			sta->flags |= WLAN_STA_WPS;
			if (wps_is_20(wps)) {
				wpa_printf(MSG_DEBUG,
					   "WPS: STA supports WPS 2.0");
				sta->flags |= WLAN_STA_WPS2;
			}
		} else
			sta->flags |= WLAN_STA_MAYBE_WPS;
		wpabuf_free(wps);
#endif /* CONFIG_WPS */
	}
#ifdef CONFIG_WPS
skip_wpa_check:
#endif /* CONFIG_WPS */

#ifdef CONFIG_MBO
	if (hapd->conf->mbo_enabled && (hapd->conf->wpa & 2) &&
	    elems.mbo && sta->cell_capa && !(sta->flags & WLAN_STA_MFP) &&
	    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_INFO,
			   "MBO: Reject WPA2 association without PMF");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211R_AP
	p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, buf, sizeof(buf),
					sta->auth_alg, req_ies, req_ies_len,
					!elems.rsnxe, reassoc, sta->vlan_id);
	if (!p) {
		wpa_printf(MSG_DEBUG, "FT: Failed to write AssocResp IEs");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		int delay_assoc = 0;

		if (!req_ies)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;

		if (!wpa_fils_validate_fils_session(sta->wpa_sm, req_ies,
						    req_ies_len,
						    sta->fils_session)) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Session validation failed");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		res = wpa_fils_validate_key_confirm(sta->wpa_sm, req_ies,
						    req_ies_len);
		if (res < 0) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Key Confirm validation failed");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		if (fils_process_hlp(hapd, sta, req_ies, req_ies_len) > 0) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Delaying Assoc Response (HLP)");
			delay_assoc = 1;
		} else {
			wpa_printf(MSG_DEBUG,
				   "FILS: Going ahead with Assoc Response (no HLP)");
		}

		if (sta) {
			wpa_printf(MSG_DEBUG, "FILS: HLP callback cleanup");
			eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
			os_free(sta->fils_pending_assoc_req);
			sta->fils_pending_assoc_req = NULL;
			sta->fils_pending_assoc_req_len = 0;
			wpabuf_free(sta->fils_hlp_resp);
			sta->fils_hlp_resp = NULL;
			sta->fils_drv_assoc_finish = 0;
		}

		if (sta && delay_assoc && status == WLAN_STATUS_SUCCESS) {
			u8 *req_tmp;

			req_tmp = os_malloc(req_ies_len);
			if (!req_tmp) {
				wpa_printf(MSG_DEBUG,
					   "FILS: buffer allocation failed for assoc req");
				goto fail;
			}
			os_memcpy(req_tmp, req_ies, req_ies_len);
			sta->fils_pending_assoc_req = req_tmp;
			sta->fils_pending_assoc_req_len = req_ies_len;
			sta->fils_pending_assoc_is_reassoc = reassoc;
			sta->fils_drv_assoc_finish = 1;
			wpa_printf(MSG_DEBUG,
				   "FILS: Waiting for HLP processing before sending (Re)Association Response frame to "
				   MACSTR, MAC2STR(sta->addr));
			eloop_register_timeout(
				0, hapd->conf->fils_hlp_wait_time * 1024,
				fils_hlp_timeout, hapd, sta);
			return 0;
		}
		p = hostapd_eid_assoc_fils_session(sta->wpa_sm, p,
						   elems.fils_session,
						   sta->fils_hlp_resp);
		if (!p)
			goto fail;

		wpa_hexdump(MSG_DEBUG, "FILS Assoc Resp BUF (IEs)",
			    buf, p - buf);
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) &&
	    !(iface->drv_flags2 & WPA_DRIVER_FLAGS2_OWE_OFFLOAD_AP) &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
	    elems.owe_dh) {
		u8 *npos;
		u16 ret_status;

		npos = owe_assoc_req_process(hapd, sta,
					     elems.owe_dh, elems.owe_dh_len,
					     p, sizeof(buf) - (p - buf),
					     &ret_status);
		status = ret_status;
		if (npos)
			p = npos;

		if (!npos &&
		    status == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED) {
			hostapd_sta_assoc(hapd, addr, reassoc, ret_status, buf,
					  p - buf);
			return 0;
		}

		if (!npos || status != WLAN_STATUS_SUCCESS)
			goto fail;
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
		dpp_pfs_free(sta->dpp_pfs);
		sta->dpp_pfs = NULL;

		if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
		    hapd->conf->dpp_netaccesskey && sta->wpa_sm &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP &&
		    elems.owe_dh) {
			sta->dpp_pfs = dpp_pfs_init(
				wpabuf_head(hapd->conf->dpp_netaccesskey),
				wpabuf_len(hapd->conf->dpp_netaccesskey));
			if (!sta->dpp_pfs) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Could not initialize PFS");
				/* Try to continue without PFS */
				goto pfs_fail;
			}

			if (dpp_pfs_process(sta->dpp_pfs, elems.owe_dh,
					    elems.owe_dh_len) < 0) {
				dpp_pfs_free(sta->dpp_pfs);
				sta->dpp_pfs = NULL;
				reason = WLAN_REASON_UNSPECIFIED;
				goto fail;
			}
		}

		wpa_auth_set_dpp_z(sta->wpa_sm, sta->dpp_pfs ?
				   sta->dpp_pfs->secret : NULL);
	pfs_fail:
#endif /* CONFIG_DPP2 */

	if (elems.rrm_enabled &&
	    elems.rrm_enabled_len >= sizeof(sta->rrm_enabled_capa))
	    os_memcpy(sta->rrm_enabled_capa, elems.rrm_enabled,
		      sizeof(sta->rrm_enabled_capa));

#if defined(CONFIG_IEEE80211R_AP) || defined(CONFIG_FILS) || defined(CONFIG_OWE)
	hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);

	if (sta->auth_alg == WLAN_AUTH_FT ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK)
		updated = ap_sta_set_authorized_flag(hapd, sta, 1);
#else /* CONFIG_IEEE80211R_AP || CONFIG_FILS */
	/* Keep compiler silent about unused variables */
	if (status) {
	}
#endif /* CONFIG_IEEE80211R_AP || CONFIG_FILS */

	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;

	hostapd_set_sta_flags(hapd, sta);

#ifdef CONFIG_IEEE80211BE
	if (hostapd_process_assoc_ml_info(hapd, sta, req_ies, req_ies_len,
					  !!reassoc, WLAN_STATUS_SUCCESS,
					  true, NULL)) {
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		reason = WLAN_REASON_UNSPECIFIED;
		goto fail;
	}
#endif /* CONFIG_IEEE80211BE */

	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);

	if (reassoc && (sta->auth_alg == WLAN_AUTH_FT))
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
#ifdef CONFIG_FILS
	else if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
		 sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
		 sta->auth_alg == WLAN_AUTH_FILS_PK)
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FILS);
#endif /* CONFIG_FILS */
	else
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);

	hostapd_new_assoc_sta(hapd, sta, !new_assoc);

	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);

#ifdef CONFIG_P2P
	if (req_ies) {
		p2p_group_notif_assoc(hapd->p2p_group, sta->addr,
				      req_ies, req_ies_len);
	}
#endif /* CONFIG_P2P */

	if (elems.wfa_capab)
		hostapd_wfa_capab(hapd, sta, elems.wfa_capab,
				  elems.wfa_capab + elems.wfa_capab_len);

	return 0;

fail:
#ifdef CONFIG_IEEE80211R_AP
	if (status >= 0)
		hostapd_sta_assoc(hapd, addr, reassoc, status, buf, p - buf);
#endif /* CONFIG_IEEE80211R_AP */
	hostapd_drv_sta_disassoc(hapd, sta->addr, reason);
	ap_free_sta(hapd, sta);
	return -1;
}


static void hostapd_remove_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	ap_sta_set_authorized(hapd, sta, 0);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


#ifdef CONFIG_IEEE80211BE
void hostapd_notif_disassoc_mld(struct hostapd_data *assoc_hapd,
				struct sta_info *sta, const u8 *addr)
{
	unsigned int i;
	struct hostapd_data *tmp_hapd;
	struct hapd_interfaces *interfaces = assoc_hapd->iface->interfaces;

	/* Remove STA entry in non-assoc links */
	for (i = 0; i < interfaces->count; i++) {
		struct sta_info *tmp_sta;

		tmp_hapd = interfaces->iface[i]->bss[0];

		if (!tmp_hapd->conf->mld_ap ||
		    assoc_hapd == tmp_hapd ||
		    !hostapd_is_ml_partner(assoc_hapd, tmp_hapd))
			continue;

#ifdef CONFIG_QCN_EXTN
		if (hostapd_is_repurpose_disabled_11be_extn(tmp_hapd->conf))
			continue;
#endif

		tmp_sta = ap_get_sta(tmp_hapd, addr);
		if (tmp_sta)
			ap_free_sta(tmp_hapd, tmp_sta);
	}

	/* Remove STA in assoc link — issues single MLD-level sta_remove() */
	hostapd_remove_sta(assoc_hapd, sta);
}
#endif /* CONFIG_IEEE80211BE */


void hostapd_notif_disassoc(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;

	if (addr == NULL) {
		/*
		 * This could potentially happen with unexpected event from the
		 * driver wrapper. This was seen at least in one case where the
		 * driver ended up reporting a station mode event while hostapd
		 * was running, so better make sure we stop processing such an
		 * event here.
		 */
		wpa_printf(MSG_DEBUG,
			   "hostapd_notif_disassoc: Skip event with no address");
		return;
	}

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_multiple_link_mld(hapd)) {
		struct hostapd_data *assoc_hapd;
		unsigned int i;

		if (!sta) {
			/* Find non-MLO cases from any of the affiliated AP
			 * links. */
			for (i = 0; i < hapd->iface->interfaces->count; ++i) {
				struct hostapd_iface *h =
					hapd->iface->interfaces->iface[i];
				struct hostapd_data *h_hapd = h->bss[0];
				struct hostapd_bss_config *hconf = h_hapd->conf;

				if (!hconf->mld_ap ||
				    !hostapd_is_ml_partner(hapd, h_hapd))
					continue;

#ifdef CONFIG_QCN_EXTN
				if (hostapd_is_repurpose_disabled_11be_extn(h_hapd->conf))
					continue;
#endif /* CONFIG_QCN_EXTN */

				sta = ap_get_sta(h_hapd, addr);
				if (sta) {
					if (!sta->mld_info.mld_sta) {
						hapd = h_hapd;
						goto legacy;
					}
					break;
				}
			}
		} else if (!sta->mld_info.mld_sta) {
			goto legacy;
		}
		if (!sta) {
			wpa_printf(MSG_DEBUG,
			   "Disassociation notification for unknown STA "
			   MACSTR, MAC2STR(addr));
			return;
		}
		sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (sta)
			hostapd_notif_disassoc_mld(assoc_hapd, sta, addr);
		return;
	}

legacy:
#endif /* CONFIG_IEEE80211BE */
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG,
			   "Disassociation notification for unknown STA "
			   MACSTR, MAC2STR(addr));
		return;
	}

	hostapd_remove_sta(hapd, sta);

	/* expire acl caches and queries for the removed wired station */
	if (hapd->iface->drv_flags == WPA_DRIVER_FLAGS_WIRED)
	{
#ifndef CONFIG_NO_RADIUS
		hostapd_acl_expire_sta(hapd, addr);
#endif
	}
}


void hostapd_event_sta_low_ack(struct hostapd_data *hapd, const u8 *addr,
			       u32 num_packets)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	u32 reason = WLAN_REASON_DISASSOC_LOW_ACK;

#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *orig_hapd = hapd;

	if (!sta && hapd->conf->mld_ap) {
		hapd = hostapd_find_by_sta(hapd->iface, addr, true, &sta);
		if (!hapd) {
			wpa_printf(MSG_DEBUG,
				   "No partner link BSS found for STA " MACSTR
				   " - fallback to received context",
				   MAC2STR(addr));
			hapd = orig_hapd;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	if (!sta || (!hapd->conf->disassoc_low_ack && num_packets != 0xFFFF) ||
	    sta->agreed_to_steer ||
	    (sta->auth_alg == WLAN_AUTH_FT && sta->ft_re_add == true &&
	     num_packets == 0xFFFF))
		return;

	if (num_packets == 0xFFFF)
		reason = WLAN_REASON_UNSPECIFIED;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "disconnected due to excessive missing ACKs");
	hostapd_drv_sta_disassoc(hapd, addr, reason);
	ap_sta_disassociate(hapd, sta, reason);
}


void hostapd_event_sta_rssi_low(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	int rssi_threshold = 0;
	const char *source = "disabled";
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *orig_hapd = hapd;

	if (!sta && hapd->conf->mld_ap) {
		hapd = hostapd_find_by_sta(hapd->iface, addr, true, &sta);
		if (!hapd) {
			wpa_printf(MSG_DEBUG,
				   "No partner link BSS found for STA " MACSTR
				   " - fallback to received context",
				   MAC2STR(addr));
			hapd = orig_hapd;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	/* Get unified RSSI threshold configuration */
	if (hapd->conf->rssi_reject_assoc_rssi != 0) {
		rssi_threshold = hapd->conf->rssi_reject_assoc_rssi;
		source = "BSS override";
	} else if (hapd->iconf->rssi_reject_assoc_rssi != 0) {
		rssi_threshold = hapd->iconf->rssi_reject_assoc_rssi;
		source = "radio fallback";
	}

	if (!sta || rssi_threshold == 0)
		return;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "RSSI deauth: disconnecting " MACSTR " signal quality below threshold %d dBm (source: %s, SSID: %s)",
		       MAC2STR(addr), rssi_threshold, source,
		       wpa_ssid_txt(hapd->conf->ssid.ssid, hapd->conf->ssid.ssid_len));

	hostapd_drv_sta_deauth(hapd, addr, WLAN_REASON_UNSPECIFIED);
	ap_sta_deauthenticate(hapd, sta, WLAN_REASON_UNSPECIFIED);
}


void hostapd_event_sta_opmode_changed(struct hostapd_data *hapd, const u8 *addr,
				      enum smps_mode smps_mode,
				      enum chan_width chan_width, u8 rx_nss)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	const char *txt;

	if (!sta)
		return;

	switch (smps_mode) {
	case SMPS_AUTOMATIC:
		txt = "automatic";
		break;
	case SMPS_OFF:
		txt = "off";
		break;
	case SMPS_DYNAMIC:
		txt = "dynamic";
		break;
	case SMPS_STATIC:
		txt = "static";
		break;
	default:
		txt = NULL;
		break;
	}
	if (txt) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_SMPS_MODE_CHANGED
			MACSTR " %s", MAC2STR(addr), txt);
	}

	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
		txt = "20(no-HT)";
		break;
	case CHAN_WIDTH_20:
		txt = "20";
		break;
	case CHAN_WIDTH_40:
		txt = "40";
		break;
	case CHAN_WIDTH_80:
		txt = "80";
		break;
	case CHAN_WIDTH_80P80:
		txt = "80+80";
		break;
	case CHAN_WIDTH_160:
		txt = "160";
		break;
	case CHAN_WIDTH_320:
		txt = "320";
		break;
	default:
		txt = NULL;
		break;
	}
	if (txt) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_MAX_BW_CHANGED
			MACSTR " %s", MAC2STR(addr), txt);
	}

	if (rx_nss != 0xff) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, STA_OPMODE_N_SS_CHANGED
			MACSTR " %d", MAC2STR(addr), rx_nss);
	}
}

int hostapd_switch_power_mode(struct hostapd_data *hapd)
{
	struct he_6ghz_pwr_mode_settings settings;
	unsigned int i, num_err =  0;
	int ret = 0, err = 0;

	settings.pwr_mode = hapd->iface->power_mode_6ghz_before_change;
	settings.link_id = -1;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_validate_current_6ghz_hw_blocklist_extn(
		    hapd->iface, settings.pwr_mode, "power mode switch")) {
		hapd->iface->power_mode_6ghz_before_change = -1;
		return -1;
	}
#endif

	for (i = 0; i < hapd->iface->num_bss; i++) {
#ifdef CONFIG_IEEE80211BE
		if (hapd->iface->bss[i]->conf->mld_ap)
			settings.link_id = hapd->iface->bss[i]->mld_link_id;
		else
			settings.link_id = -1;
#endif /* CONFIG_IEEE80211BE */

		err = hostapd_drv_set_6ghz_pwr_mode(hapd->iface->bss[i], &settings);
		if (err) {
			ret = err;
			num_err++;
		}
	}

	if (hapd->iface->num_bss != num_err)
		return 0;

	hapd->iface->power_mode_6ghz_before_change = -1;

	wpa_printf(MSG_ERROR, "Power mode change failed");
	return ret;
}

/**
 * hostapd_chan_switch_complete - finalize home channel change
 * @hapd: hostapd BSS context
 *
 * Called when a home-channel change has completed. Drivers are expected
 * to stop background (Agile) CAC during the switch; this callback must
 * restart Agile CAC on the new home channel so that DFS monitoring
 * continues correctly after every channel change.
 */
void hostapd_chan_switch_complete(struct hostapd_data *hapd, u8 power_mode_6ghz,
				  int width, int width_device, int is_dfs0, int is_dfs)
{
	int freq = hapd->iface->freq;

	hostapd_clear_local_tpe(hapd->iface);

	/* Hostapd receives the CH_SWITCH_NOTIFY event for all links on the
	 * CSA-triggered radio. If the new channel requires CAC, only the first
	 * link that gets this event starts CAC, so don’t reset dfs_cac_ms when
	 * this event is received for the second link on the same radio and CAC
	 * is already in progress.
	 * For non-DFS channels, dfs_cac_ms is reset here.
	 */
	if (!hapd->iface->cac_started)
		hapd->iface->dfs_cac_ms = 0;

	if (hapd->csa_in_progress &&
	    freq == hapd->cs_freq_params.freq) {
		if ((is_dfs || is_dfs0) && hostapd_is_dfs_required(hapd->iface) &&
		    !hostapd_is_dfs_chan_available(hapd->iface) &&
		    !hapd->iface->cac_started) {
			if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_DFS_CHANNEL_SWITCH) {
#ifdef CONFIG_QCN_EXTN
				if (hostapd_ignorecac_chan_switch_complete_extn(
					    hapd, power_mode_6ghz, width,
					    width_device, is_dfs))
					return;
#endif /* CONFIG_QCN_EXTN */
				hostapd_cleanup_cs_params(hapd);
				hapd->disable_cu = 1;

				/*
				 * When CSA completed on the same channel and bandwidth
				 * with only the puncture pattern changed (punct_bitmap != 0),
				 * the channel remains operational with punched subchannels.
				 * Set state to HAPD_IFACE_ENABLED — no new CAC is needed.
				 * Calling hostapd_start_dfs_cac() on the already-active
				 * chanctx returns -EINVAL which corrupts the chanctx state
				 * and causes subsequent NL80211_CMD_RADAR_DETECT events to
				 * be dropped as "DFS event on unknown freq".
				 */
				if (hapd->iface->radar_bit_pattern) {
					wpa_printf(MSG_INFO,
						   "DFS: Same-channel radar puncture CSA completed — "
						   "setting HAPD_IFACE_ENABLED, skipping CAC "
						   "radar_bit_pattern=0x%04x punct_bitmap=0x%04x",
						   hapd->iface->radar_bit_pattern,
						   hapd->iface->conf->punct_bitmap);
					hostapd_set_state(hapd->iface, HAPD_IFACE_ENABLED);
					ieee802_11_set_beacon(hapd);
					return;
				}

				hostapd_set_state(hapd->iface, HAPD_IFACE_DFS);
				hapd->iface->cac_type = HAPD_CAC_COMPLETE_AFTER_CSA;
				if (!hapd->disabled)
					ieee802_11_set_beacon(hapd);

				if (hostapd_set_dfs_cac_time(hapd->iface))
					return;

				wpa_printf(MSG_DEBUG, "DFS:Starting CAC after CSA on freq=%d", freq);
				wpa_msg(hapd->iface->bss[0]->msg_ctx,
					MSG_INFO, DFS_EVENT_CAC_START
					"freq=%d chan=%d sec_chan=%d, width=%d,"
					"seg0=%d, seg1=%d, cac_time=%ds bitmap:0x%04x",
					hapd->iface->freq,
					hapd->iface->conf->channel,
					hapd->iface->conf->secondary_channel,
					hostapd_get_oper_chwidth(hapd->iface->conf),
					hostapd_get_oper_centr_freq_seg0_idx(hapd->iface->conf),
					hostapd_get_oper_centr_freq_seg1_idx(hapd->iface->conf),
					hapd->iface->dfs_cac_ms / 1000,
					hapd->iface->conf->punct_bitmap);

				if (hostapd_start_dfs_cac(hapd->iface, hapd->iface->conf->hw_mode,
						     hapd->iface->freq,
						     hapd->iconf->channel,
						     hapd->iface->conf->ieee80211n,
						     hapd->iface->conf->ieee80211ac,
						     hapd->iface->conf->ieee80211ax,
						     hapd->iface->conf->ieee80211be,
						     hapd->iface->conf->ieee80211bn,
						     hapd->iconf->secondary_channel,
						     convert_to_oper_chan_width(width),
						     hostapd_get_oper_centr_freq_seg0_idx(hapd->iface->conf),
						     hostapd_get_oper_centr_freq_seg1_idx(hapd->iface->conf),
						     false, width_device,
						     hapd->iconf->center_freq_device)) {
					wpa_printf(MSG_ERROR,
						   "DFS: Failed to start CAC after CSA on freq=%d, disabling interface",
						   freq);
					/*
					 * disable_cu was set above for the beacon
					 * update that never happened (that call
					 * skipped it, and CAC never reached
					 * completion to consume the flag). Clear it
					 * before re-enabling below: hostapd_enable_iface()
					 * itself sends a beacon for this hapd once
					 * cac_type/state no longer match the skipped-
					 * update check, so a stale 1 here would still
					 * be read by that beacon update and incorrectly
					 * suppress the critical-update bitmap on it.
					 */
					hapd->disable_cu = 0;
					/*
					 * hostapd_disable_iface() tears down every BSS
					 * on this iface in one call, including clearing
					 * csa_in_progress for all of them via
					 * hostapd_cleanup_cs_params(). Since hostapd is
					 * single-threaded, any other VAP's pending
					 * CH_SWITCH_NOTIFY for this same CSA is handled
					 * after this call returns and will see
					 * csa_in_progress already cleared, so it won't
					 * re-enter this branch and re-trigger disable
					 * and enable.
					 */
					hostapd_disable_iface(hapd->iface);
					hostapd_enable_iface(hapd->iface);
					return;
				}
				hostapd_schedule_agile_cac_restart(hapd->iface);
			} else {
				hostapd_disable_iface(hapd->iface);
				hostapd_enable_iface(hapd->iface);
			}
		} else {
			hapd->iconf->he_6ghz_reg_pwr_type = power_mode_6ghz;
			hostapd_cleanup_cs_params(hapd);
			hapd->disable_cu = 1;
			if (!hapd->disabled)
				ieee802_11_set_beacon(hapd);
			if (!hapd->iface->cac_started &&
			    hostapd_check_reenable_bss(hapd->iface, REENABLE_CAC))
				hostapd_enable_pending_bss(hapd->iface,
							   REENABLE_CAC, false);
			hostapd_start_device_cac_background(hapd->iface);
			hostapd_schedule_agile_cac_restart(hapd->iface);
			wpa_msg(hapd->msg_ctx, MSG_INFO, AP_CSA_FINISHED
				"freq=%d dfs=%d", freq, is_dfs);
		}
#ifdef CONFIG_QCN_EXTN
		if (hapd->iconf->conf_extn.ind_rptr &&
			((is_dfs && hapd->iconf->conf_extn.skip_cac) || !is_dfs)) {
				hostapd_csa_bitmap_update_extn(hapd->iface, freq);
		}
#endif
	} else {
		if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) {
		/* Complete AP configuration for the first bring up. */
			if (is_dfs0 > 0 &&
			    hostapd_is_dfs_required(hapd->iface) <= 0 &&
			    hapd->iface->state != HAPD_IFACE_ENABLED) {
				/* Fake a CAC start bit to skip setting channel */
				hapd->iface->cac_started = 1;
				hostapd_setup_interface_complete(hapd->iface, 0);
			}
			wpa_msg(hapd->msg_ctx, MSG_INFO, AP_CSA_FINISHED
				"freq=%d dfs=%d", freq, is_dfs);
		} else if (is_dfs &&
			   hostapd_is_dfs_required(hapd->iface) &&
			   !hostapd_is_dfs_chan_available(hapd->iface) &&
			   !hapd->iface->cac_started) {
			hostapd_disable_iface(hapd->iface);
			hostapd_enable_iface(hapd->iface);
		}
	}
}

void hostapd_event_ch_switch(struct hostapd_data *hapd, int freq, int ht,
			     int offset, int width, int cf1, int cf2,
			     u16 punct_bitmap, u8 power_mode_6ghz,
			     int width_device, int cf_device, int finished)
{
#ifdef NEED_AP_MLME
	int channel, chwidth, is_dfs0, is_dfs;
	u8 seg0_idx = 0, seg1_idx = 0, op_class, chan_no;
	size_t i;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "driver %s channel switch: iface->freq=%d, freq=%d, ht=%d, vht_ch=0x%x, he_ch=0x%x, eht_ch=0x%x, uhr_ch=0x%x, offset=%d, width=%d (%s), cf1=%d, cf2=%d, puncturing_bitmap=0x%x width_device=%d, cf_device=%d 6ghz power mode=%d",
		       finished ? "had" : "starting",
		       hapd->iface->freq,
		       freq, ht, hapd->iconf->ch_switch_vht_config,
		       hapd->iconf->ch_switch_he_config,
		       hapd->iconf->ch_switch_eht_config,
		       hapd->iconf->ch_switch_uhr_config, offset,
		       width, channel_width_to_string(width), cf1, cf2,
		       punct_bitmap, width_device, cf_device, power_mode_6ghz);

	if (!hapd->iface->current_mode) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "ignore channel switch since the interface is not yet ready");
#ifdef CONFIG_QCN_EXTN
		goto out;
#else
		return;
#endif
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_handle_csa_target_unavailable_extn(hapd, freq, finished))
		return;
#endif /* CONFIG_QCN_EXTN */

	/* Check if any of configured channels require DFS */
	is_dfs0 = hostapd_is_dfs_required(hapd->iface);
	if (finished)
		hapd->iface->freq = freq;

	channel = hostapd_hw_get_channel(hapd, freq);
	if (!channel) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "driver switched to bad channel!");
#ifdef CONFIG_QCN_EXTN
		goto out;
#else
		return;
#endif
	}

	switch (width) {
	case CHAN_WIDTH_80:
		chwidth = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case CHAN_WIDTH_80P80:
		chwidth = CONF_OPER_CHWIDTH_80P80MHZ;
		break;
	case CHAN_WIDTH_160:
		chwidth = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case CHAN_WIDTH_320:
		chwidth = CONF_OPER_CHWIDTH_320MHZ;
		break;
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_40:
	default:
		chwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	}

	is_dfs = ieee80211_is_dfs(freq, hapd->iface->hw_features,
				  hapd->iface->num_hw_features);

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		"%sfreq=%d ht_enabled=%d ch_offset=%d ch_width=%s cf1=%d cf2=%d is_dfs0=%d dfs=%d puncturing_bitmap=0x%04x width_device=%d, cf_device=%d 6ghz power mode=%d",
		finished ? WPA_EVENT_CHANNEL_SWITCH :
		WPA_EVENT_CHANNEL_SWITCH_STARTED,
		freq, ht, offset, channel_width_to_string(width),
		cf1, cf2, is_dfs0, is_dfs, punct_bitmap, width_device, cf_device, power_mode_6ghz);

	/* Defer iconf updates until CSA completes to avoid
	 * advertising the target channel prematurely.
	 */
	if (!finished)
#ifdef CONFIG_QCN_EXTN
		goto out;
#else
		return;
#endif

	/* The operating channel changed when CSA finished, so need to update
	 * hw_mode for all following operations to cover the cases where the
	 * driver changed the operating band. */
	if (hostapd_csa_update_hwmode(hapd->iface))
#ifdef CONFIG_QCN_EXTN
		goto out;
#else
		return;
#endif

	switch (hapd->iface->current_mode->mode) {
	case HOSTAPD_MODE_IEEE80211A:
		if (cf1 == 5935)
			seg0_idx = (cf1 - 5925) / 5;
		else if (cf1 > 5950)
			seg0_idx = (cf1 - 5950) / 5;
		else if (cf1 > 5000)
			seg0_idx = (cf1 - 5000) / 5;

		if (cf2 == 5935)
			seg1_idx = (cf2 - 5925) / 5;
		else if (cf2 > 5950)
			seg1_idx = (cf2 - 5950) / 5;
		else if (cf2 > 5000)
			seg1_idx = (cf2 - 5000) / 5;
		break;
	default:
		ieee80211_freq_to_chan(cf1, &seg0_idx);
		ieee80211_freq_to_chan(cf2, &seg1_idx);
		break;
	}

	hapd->iconf->channel = channel;
	hapd->iconf->ieee80211n = ht;
	if (!ht)
		hapd->iconf->ieee80211ac = 0;
	if (hapd->iconf->ch_switch_vht_config) {
		/* CHAN_SWITCH VHT config */
		if (hapd->iconf->ch_switch_vht_config &
		    CH_SWITCH_VHT_ENABLED)
			hapd->iconf->ieee80211ac = 1;
		else if (hapd->iconf->ch_switch_vht_config &
			 CH_SWITCH_VHT_DISABLED)
			hapd->iconf->ieee80211ac = 0;
	}
	if (hapd->iconf->ch_switch_he_config) {
		/* CHAN_SWITCH HE config */
		if (hapd->iconf->ch_switch_he_config &
		    CH_SWITCH_HE_ENABLED) {
			hapd->iconf->ieee80211ax = 1;
			if (hapd->iface->freq > 4000 &&
			    hapd->iface->freq < 5895)
				hapd->iconf->ieee80211ac = 1;
		}
		else if (hapd->iconf->ch_switch_he_config &
			 CH_SWITCH_HE_DISABLED)
			hapd->iconf->ieee80211ax = 0;
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ch_switch_eht_config) {
		/* CHAN_SWITCH EHT config */
		if (hapd->iconf->ch_switch_eht_config &
		    CH_SWITCH_EHT_ENABLED) {
			hapd->iconf->ieee80211be = 1;
			hapd->iconf->ieee80211ax = 1;
			if (!is_6ghz_freq(hapd->iface->freq) &&
			    hapd->iface->freq > 4000)
				hapd->iconf->ieee80211ac = 1;
		} else if (hapd->iconf->ch_switch_eht_config &
			   CH_SWITCH_EHT_DISABLED)
			hapd->iconf->ieee80211be = 0;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hapd->iconf->ch_switch_uhr_config) {
		/* CHAN_SWITCH UHR config */
		if (hapd->iconf->ch_switch_uhr_config &
		    CH_SWITCH_UHR_ENABLED) {
			hapd->iconf->ieee80211bn = 1;
			hapd->iconf->ieee80211be = 1;
			hapd->iconf->ieee80211ax = 1;
			if (!is_6ghz_freq(hapd->iface->freq) &&
			    hapd->iface->freq > 4000)
				hapd->iconf->ieee80211ac = 1;
		} else if (hapd->iconf->ch_switch_uhr_config &
			   CH_SWITCH_UHR_DISABLED)
			hapd->iconf->ieee80211bn = 0;
	}
#endif /* CONFIG_IEEE80211BN */
	hapd->iconf->ch_switch_vht_config = 0;
	hapd->iconf->ch_switch_he_config = 0;
	hapd->iconf->ch_switch_eht_config = 0;
	hapd->iconf->ch_switch_uhr_config = 0;
#ifdef CONFIG_IEEE80211BN
	hostapd_disable_npca(hapd->iconf);
#endif /* CONFIG_IEEE80211BN */

	if (width == CHAN_WIDTH_40 || width == CHAN_WIDTH_80 ||
	    width == CHAN_WIDTH_80P80 || width == CHAN_WIDTH_160 ||
	    width == CHAN_WIDTH_320)
		hapd->iconf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
	else if (width == CHAN_WIDTH_20 || width == CHAN_WIDTH_20_NOHT)
		hapd->iconf->ht_capab &= ~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;

	hapd->iconf->secondary_channel = offset;
	if (ieee80211_freq_to_channel_ext(freq, offset, chwidth,
					  &op_class, &chan_no) !=
	    NUM_HOSTAPD_MODES)
		hapd->iconf->op_class = op_class;
	hostapd_set_oper_chwidth(hapd->iconf, chwidth);
	hostapd_set_oper_centr_freq_seg0_idx(hapd->iconf, seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, seg1_idx);
	/* Auto-detect new bw320_offset */
	hostapd_set_and_check_bw320_offset(hapd->iconf, 0);
#ifdef CONFIG_IEEE80211BE
	hapd->iconf->punct_bitmap = punct_bitmap;
#endif /* CONFIG_IEEE80211BE */

	hapd->iconf->center_freq_device = cf_device;
	switch (width_device) {
		case CHAN_WIDTH_40:
			hapd->iconf->bandwidth_device = 40;
			break;
		case CHAN_WIDTH_80:
			hapd->iconf->bandwidth_device = 80;
			break;
		case CHAN_WIDTH_160:
			hapd->iconf->bandwidth_device = 160;
			break;
		case CHAN_WIDTH_320:
			hapd->iconf->bandwidth_device = 320;
			break;
		default:
			hapd->iconf->bandwidth_device = 0;
			hapd->iconf->center_freq_device = 0;
			break;
	}

#ifdef CONFIG_QCN_EXTN
	update_chan_params(hapd, cf1, cf2, hostapd_get_chan_width_from_oper_chan_width(hapd->iconf));
#endif

#ifdef CONFIG_IEEE80211BN
	if (hapd->conf->mapc_conf)
		mapc_handle_primary_channel_change(hapd);
#endif /* CONFIG_IEEE80211BN */

	hostapd_chan_switch_complete(hapd, power_mode_6ghz, width,
				     width_device, is_dfs0, is_dfs);

#ifdef CONFIG_IEEE80211BN
	if (hapd->conf->mapc_conf) {
		if (!mapc_handle_primary_channel_change(hapd))
			mapc_handle_negotiation_update(hapd, MAPC_SCHEME_CO_TDMA);
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_QCN_EXTN
	hostapd_periodic_acs_schedule(hapd->iface);
#endif

	for (i = 0; i < hapd->iface->num_bss; i++)
		hostapd_neighbor_set_own_report(hapd->iface->bss[i]);

#ifdef CONFIG_IEEE80211BN
	if (hapd->smd_neighbor_update_ctx)
		smd_neighbor_update_notify_own_report_changed(hapd);
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_OCV
	if (hapd->conf->ocv &&
	    !(hapd->iface->drv_flags2 &
	      WPA_DRIVER_FLAGS2_SA_QUERY_OFFLOAD_AP)) {
		struct sta_info *sta;
		bool check_sa_query = false;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (wpa_auth_uses_ocv(sta->wpa_sm) &&
			    !(sta->flags & WLAN_STA_WNM_SLEEP_MODE)) {
				sta->post_csa_sa_query = 1;
				check_sa_query = true;
			}
		}

		if (check_sa_query) {
			wpa_printf(MSG_DEBUG,
				   "OCV: Check post-CSA SA Query initiation in 15 seconds");
			eloop_register_timeout(15, 0,
					       hostapd_ocv_check_csa_sa_query,
					       hapd, NULL);
		}
	}
#endif /* CONFIG_OCV */
#endif /* NEED_AP_MLME */

#ifdef CONFIG_QCN_EXTN
out:
	if (hapd && hapd->iface && hapd->iface->iface_extn.dcs_in_progress) {
		const char *reason = finished ? "CSA finished" : "CSA error/early exit";
		hostapd_dcs_restore_extn(hapd->iface, reason);
		return;
	}
#endif
}


void hostapd_event_connect_failed_reason(struct hostapd_data *hapd,
					 const u8 *addr, int reason_code)
{
	switch (reason_code) {
	case MAX_CLIENT_REACHED:
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_REJECTED_MAX_STA MACSTR,
			MAC2STR(addr));
		break;
	case BLOCKED_CLIENT:
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_REJECTED_BLOCKED_STA MACSTR,
			MAC2STR(addr));
		break;
	}
}


#ifdef CONFIG_ACS
void hostapd_acs_channel_selected(struct hostapd_data *hapd,
				  struct acs_selected_channels *acs_res)
{
	int ret, i;
	int err = 0;
	struct hostapd_channel_data *pri_chan;

#ifdef CONFIG_IEEE80211BE
	if (acs_res->link_id != -1) {
		hapd = hostapd_mld_get_link_bss(hapd, acs_res->link_id);
		if (!hapd) {
			wpa_printf(MSG_ERROR,
				   "MLD: Failed to get link BSS for EVENT_ACS_CHANNEL_SELECTED link_id=%d",
				   acs_res->link_id);
			return;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->iconf->channel) {
		wpa_printf(MSG_INFO, "ACS: Channel was already set to %d",
			   hapd->iconf->channel);
		return;
	}

	hapd->iface->freq = acs_res->pri_freq;

	if (!hapd->iface->current_mode) {
		for (i = 0; i < hapd->iface->num_hw_features; i++) {
			struct hostapd_hw_modes *mode =
				&hapd->iface->hw_features[i];

			if (mode->mode == acs_res->hw_mode) {
				if (hapd->iface->freq > 0 &&
				    !hw_get_chan(mode->mode,
						 hapd->iface->freq,
						 hapd->iface->hw_features,
						 hapd->iface->num_hw_features))
					continue;
				hapd->iface->current_mode = mode;
				break;
			}
		}
		if (!hapd->iface->current_mode) {
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "driver selected to bad hw_mode");
			err = 1;
			goto out;
		}
	}

	if (!acs_res->pri_freq) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "driver switched to bad channel");
		err = 1;
		goto out;
	}
	pri_chan = hw_get_channel_freq(hapd->iface->current_mode->mode,
				       acs_res->pri_freq, NULL,
				       hapd->iface->hw_features,
				       hapd->iface->num_hw_features);
	if (!pri_chan) {
		wpa_printf(MSG_ERROR,
			   "ACS: Could not determine primary channel number from pri_freq %u",
			   acs_res->pri_freq);
		err = 1;
		goto out;
	}

	hapd->iconf->channel = pri_chan->chan;
	hapd->iconf->acs = 1;

	if (acs_res->sec_freq == 0)
		hapd->iconf->secondary_channel = 0;
	else if (acs_res->sec_freq < acs_res->pri_freq)
		hapd->iconf->secondary_channel = -1;
	else if (acs_res->sec_freq > acs_res->pri_freq)
		hapd->iconf->secondary_channel = 1;
	else {
		wpa_printf(MSG_ERROR, "Invalid secondary channel!");
		err = 1;
		goto out;
	}

	hapd->iconf->edmg_channel = acs_res->edmg_channel;

	if (hapd->iface->conf->ieee80211ac || hapd->iface->conf->ieee80211ax) {
		/* set defaults for backwards compatibility */
		hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, 0);
		hostapd_set_oper_centr_freq_seg0_idx(hapd->iconf, 0);
		hostapd_set_oper_chwidth(hapd->iconf, CONF_OPER_CHWIDTH_USE_HT);
		if (acs_res->ch_width == 40) {
			if (is_6ghz_freq(acs_res->pri_freq))
				hostapd_set_oper_centr_freq_seg0_idx(
					hapd->iconf,
					acs_res->vht_seg0_center_ch);
		} else if (acs_res->ch_width == 80) {
			hostapd_set_oper_centr_freq_seg0_idx(
				hapd->iconf, acs_res->vht_seg0_center_ch);
			if (acs_res->vht_seg1_center_ch == 0) {
				hostapd_set_oper_chwidth(
					hapd->iconf, CONF_OPER_CHWIDTH_80MHZ);
			} else {
				hostapd_set_oper_chwidth(
					hapd->iconf,
					CONF_OPER_CHWIDTH_80P80MHZ);
				hostapd_set_oper_centr_freq_seg1_idx(
					hapd->iconf,
					acs_res->vht_seg1_center_ch);
			}
		} else if (acs_res->ch_width == 160) {
			hostapd_set_oper_chwidth(hapd->iconf,
						 CONF_OPER_CHWIDTH_160MHZ);
			hostapd_set_oper_centr_freq_seg0_idx(
				hapd->iconf, acs_res->vht_seg1_center_ch);
		}
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->iface->conf->ieee80211be && acs_res->ch_width == 320) {
		hostapd_set_oper_chwidth(hapd->iconf, CONF_OPER_CHWIDTH_320MHZ);
		hostapd_set_oper_centr_freq_seg0_idx(
			hapd->iconf, acs_res->vht_seg1_center_ch);
		hostapd_set_oper_centr_freq_seg1_idx(hapd->iconf, 0);
	}

	if (hapd->iface->conf->ieee80211be && acs_res->puncture_bitmap)
		hapd->iconf->punct_bitmap = acs_res->puncture_bitmap;
#endif /* CONFIG_IEEE80211BE */

out:
	ret = hostapd_acs_completed(hapd->iface, err);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "ACS: Possibly channel configuration is invalid");
	}
}
#endif /* CONFIG_ACS */


int hostapd_probe_req_rx(struct hostapd_data *hapd, const u8 *sa, const u8 *da,
			 const u8 *bssid, const u8 *ie, size_t ie_len,
			 int ssi_signal)
{
	size_t i;
	int ret = 0;

	if (sa == NULL || ie == NULL)
		return -1;

	random_add_randomness(sa, ETH_ALEN);
	for (i = 0; hapd->probereq_cb && i < hapd->num_probereq_cb; i++) {
		if (hapd->probereq_cb[i].cb(hapd->probereq_cb[i].ctx,
					    sa, da, bssid, ie, ie_len,
					    ssi_signal) > 0) {
			ret = 1;
			break;
		}
	}
	return ret;
}


#ifdef HOSTAPD

#ifdef CONFIG_IEEE80211R_AP
static void hostapd_notify_auth_ft_finish(void *ctx, const u8 *dst,
					  u16 auth_transaction, u16 status,
					  const u8 *ies, size_t ies_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL)
		return;

	hostapd_logger(hapd, dst, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "authentication OK (FT)");
	sta->flags |= WLAN_STA_AUTH;

	hostapd_sta_auth(hapd, dst, auth_transaction, status, ies, ies_len);
}
#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_FILS
static void hostapd_notify_auth_fils_finish(struct hostapd_data *hapd,
					    struct sta_info *sta, u16 resp,
					    struct wpabuf *data, int pub)
{
	if (resp == WLAN_STATUS_SUCCESS) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "authentication OK (FILS)");
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_FILS_SK;
		mlme_authenticate_indication(hapd, sta);
	} else {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication failed (FILS)");
	}

	hostapd_sta_auth(hapd, sta->addr, 2, resp,
			 data ? wpabuf_head(data) : NULL,
			 data ? wpabuf_len(data) : 0);
	wpabuf_free(data);
}
#endif /* CONFIG_FILS */


static void hostapd_notif_auth(struct hostapd_data *hapd,
			       struct auth_info *rx_auth)
{
	struct sta_info *sta;
	u16 status = WLAN_STATUS_SUCCESS;

	sta = ap_get_sta(hapd, rx_auth->peer);
	if (!sta) {
		sta = ap_sta_add(hapd, rx_auth->peer);
		if (sta == NULL) {
			status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}
	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);
#ifdef CONFIG_IEEE80211R_AP
	if (rx_auth->auth_type == WLAN_AUTH_FT && hapd->wpa_auth) {
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_DEBUG,
				   "FT: Failed to initialize WPA state machine");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_ft_process_auth(sta->wpa_sm,
				    rx_auth->auth_transaction, rx_auth->ies,
				    rx_auth->ies_len,
				    hostapd_notify_auth_ft_finish, hapd, false);
		return;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_FILS
	if (rx_auth->auth_type == WLAN_AUTH_FILS_SK) {
		sta->auth_alg = WLAN_AUTH_FILS_SK;
		handle_auth_fils(hapd, sta, rx_auth->ies, rx_auth->ies_len,
				 rx_auth->auth_type, rx_auth->auth_transaction,
				 rx_auth->status_code,
				 hostapd_notify_auth_fils_finish);
		return;
	}
#endif /* CONFIG_FILS */

fail:
	hostapd_sta_auth(hapd, rx_auth->peer, rx_auth->auth_transaction + 1,
			 status, NULL, 0);
}


#ifndef NEED_AP_MLME
static void hostapd_action_rx(struct hostapd_data *hapd,
			      struct rx_mgmt *drv_mgmt)
{
	struct ieee80211_mgmt *mgmt;
	struct sta_info *sta;
	size_t plen __maybe_unused;
	u16 fc;
	u8 *action __maybe_unused;

	if (drv_mgmt->frame_len < IEEE80211_HDRLEN + 2 + 1)
		return;

	plen = drv_mgmt->frame_len - IEEE80211_HDRLEN;

	mgmt = (struct ieee80211_mgmt *) drv_mgmt->frame;
	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ACTION)
		return; /* handled by the driver */

	action = (u8 *) &mgmt->u.action.u;
	wpa_printf(MSG_DEBUG, "RX_ACTION category %u action %u sa " MACSTR
		   " da " MACSTR " plen %d",
		   mgmt->u.action.category, *action,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), (int) plen);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "%s: station not found", __func__);
		return;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (mgmt->u.action.category == WLAN_ACTION_FT) {
		wpa_ft_action_rx(sta->wpa_sm, (u8 *) &mgmt->u.action, mgmt->sa,
				 hapd->own_addr, plen);
		return;
	}
#endif /* CONFIG_IEEE80211R_AP */
	if (mgmt->u.action.category == WLAN_ACTION_SA_QUERY) {
		ieee802_11_sa_query_action(hapd, mgmt, drv_mgmt->frame_len);
		return;
	}
#ifdef CONFIG_WNM_AP
	if (mgmt->u.action.category == WLAN_ACTION_WNM) {
		ieee802_11_rx_wnm_action_ap(hapd, mgmt, drv_mgmt->frame_len);
		return;
	}
#endif /* CONFIG_WNM_AP */
#ifdef CONFIG_FST
	if (mgmt->u.action.category == WLAN_ACTION_FST && hapd->iface->fst) {
		fst_rx_action(hapd->iface->fst, mgmt, drv_mgmt->frame_len);
		return;
	}
#endif /* CONFIG_FST */
#ifdef CONFIG_DPP
	if (plen >= 2 + 4 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] ==
	    DPP_OUI_TYPE) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.vs_public_action.oui;
		end = drv_mgmt->frame + drv_mgmt->frame_len;
		hostapd_dpp_rx_action(hapd, mgmt->sa, pos, end - pos,
				      drv_mgmt->freq);
		return;
	}
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	if (mgmt->u.action.category == WLAN_ACTION_PUBLIC && plen >= 5 &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] == NAN_OUI_TYPE) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.vs_public_action.variable;
		end = drv_mgmt->frame + drv_mgmt->frame_len;
		pos++;
		hostapd_nan_usd_rx_sdf(hapd, mgmt->sa, mgmt->bssid,
				       drv_mgmt->freq, pos, end - pos);
		return;
	}
#endif /* CONFIG_NAN_USD */
}
#endif /* NEED_AP_MLME */


#ifdef NEED_AP_MLME

#ifndef CONFIG_QCN_EXTN
static
#endif
struct hostapd_data *
switch_link_hapd(struct hostapd_data *hapd, int link_id)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && link_id >= 0) {
		struct hostapd_data *link_bss;

		link_bss = hostapd_mld_get_link_bss(hapd, link_id);
		if (link_bss)
			return link_bss;
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd;
}

struct hostapd_data *
get_link_hapd(struct hostapd_data *hapd, const u8 *ies, size_t len,
	      int *link_id)
{
	struct hostapd_data *link_hapd;
	u16 link_id_bitmap = 0;
	int parsed_link_id;

	if (link_id)
		*link_id = -1;

	if (!hapd || !hapd->conf || !hapd->conf->mld_ap || !ies || len < 3)
		return NULL;

	if (ieee80211_parse_mlo_link_info_ie(ies, len, &link_id_bitmap) < 0)
		return NULL;

	parsed_link_id = ieee80211_get_link_id_from_bitmap(link_id_bitmap);
	if (parsed_link_id < 0)
		return NULL;

	link_hapd = switch_link_hapd(hapd, parsed_link_id);
	if (!link_hapd || !link_hapd->iface ||
			link_hapd->mld_link_id != parsed_link_id)
		return NULL;

	if (link_id)
		*link_id = parsed_link_id;

	return link_hapd;
}

static struct hostapd_data *
switch_link_scan(struct hostapd_data *hapd, u64 scan_cookie,
		 const union wpa_event_data *data)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && hapd->iface->interfaces) {
		unsigned int i;

		/* Primary: cookie-based match */
		if (scan_cookie != 0) {
			for (i = 0; i < hapd->iface->interfaces->count; i++) {
				struct hostapd_iface *h;
				struct hostapd_data *h_hapd;

				h = hapd->iface->interfaces->iface[i];
				h_hapd = h->bss[0];
				if (!hostapd_is_ml_partner(hapd, h_hapd))
					continue;

				if (h_hapd->scan_cookie == scan_cookie) {
					h_hapd->scan_cookie = 0;
					return h_hapd;
				}
			}
		}

		/*
		 * Fallback: frequency-based match.
		 *
		 * When the driver does not provide a scan cookie (cookie == 0)
		 * or no partner matched the cookie, try to identify the link
		 * that requested the scan by checking whether the first
		 * reported scan frequency belongs to a partner link's hardware
		 * mode channel list.
		 *
		 * This relies on the assumption that each MLD link scans a
		 * disjoint set of frequencies (e.g., link A → 2.4 GHz,
		 * link B → 5 GHz, link C → 6 GHz).
		 */
		if (data && data->scan_info.freqs &&
		    data->scan_info.num_freqs > 0) {
			int first_freq = data->scan_info.freqs[0];

			for (i = 0; i < hapd->iface->interfaces->count; i++) {
				struct hostapd_iface *h;
				struct hostapd_data *h_hapd;
				int j, k;

				h = hapd->iface->interfaces->iface[i];
				h_hapd = h->bss[0];
				if (!hostapd_is_ml_partner(hapd, h_hapd))
					continue;

				for (j = 0; j < h_hapd->iface->num_hw_features;
				     j++) {
					struct hostapd_hw_modes *mode =
						&h_hapd->iface->hw_features[j];

					if (hostapd_hw_skip_mode(h_hapd->iface,
								 mode))
						continue;
					for (k = 0; k < mode->num_channels;
					     k++) {
						if (mode->channels[k].freq ==
						    first_freq) {
							wpa_printf(MSG_DEBUG,
								   "switch_link_scan: freq-based match link_id=%d freq=%d",
								   h_hapd->mld_link_id,
								   first_freq);
							return h_hapd;
						}
					}
				}
			}
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd;
}


#define HAPD_BROADCAST ((struct hostapd_data *) -1)

static struct hostapd_data * get_hapd_bssid(struct hostapd_iface *iface,
					    const u8 *bssid, int link_id)
{
	size_t i;

	if (bssid == NULL)
		return NULL;
	if (bssid[0] == 0xff && bssid[1] == 0xff && bssid[2] == 0xff &&
	    bssid[3] == 0xff && bssid[4] == 0xff && bssid[5] == 0xff)
		return HAPD_BROADCAST;
#ifdef CONFIG_NAN_USD
	if (nan_de_is_nan_network_id(bssid))
		return HAPD_BROADCAST; /* Process NAN Network ID like broadcast
					*/
#endif /* CONFIG_NAN_USD */

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd;
#ifdef CONFIG_IEEE80211BE
		struct hostapd_data *p_hapd;
#endif /* CONFIG_IEEE80211BE */

		hapd = iface->bss[i];
		if (!hapd)
			continue;

		if (ether_addr_equal(bssid, hapd->own_addr))
			return hapd;

#ifdef CONFIG_IEEE80211BE
		if (ether_addr_equal(bssid, hapd->own_addr) ||
		    (hapd->conf->mld_ap &&
		     ether_addr_equal(bssid, hapd->mld->mld_addr) &&
		     link_id == hapd->mld_link_id))
			return hapd;

		if (!hapd->conf->mld_ap)
			continue;

		for_each_mld_link(p_hapd, hapd) {
			if (p_hapd == hapd)
				continue;

			if (ether_addr_equal(bssid, p_hapd->own_addr) ||
			    (p_hapd->conf->mld_ap &&
			     ether_addr_equal(bssid, p_hapd->mld->mld_addr) &&
			     link_id == p_hapd->mld_link_id))
				return p_hapd;
		}
#endif /* CONFIG_IEEE80211BE */
	}

	return NULL;
}


static void hostapd_rx_from_unknown_sta(struct hostapd_data *hapd,
					const u8 *bssid, const u8 *addr,
					int wds)
{
	hapd = get_hapd_bssid(hapd->iface, bssid, -1);
	if (hapd == NULL || hapd == HAPD_BROADCAST)
		return;

	ieee802_11_rx_from_unknown(hapd, addr, wds);
}


static int hostapd_mgmt_rx(struct hostapd_data *hapd, struct rx_mgmt *rx_mgmt)
{
	struct hostapd_iface *iface;
	const struct ieee80211_hdr *hdr;
	const u8 *bssid;
	struct hostapd_frame_info fi;
	int ret;

	if (rx_mgmt->ctx)
		hapd = rx_mgmt->ctx;
	hapd = switch_link_hapd(hapd, rx_mgmt->link_id);
	iface = hapd->iface;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_mgmt_frame_handling) {
		size_t hex_len = 2 * rx_mgmt->frame_len + 1;
		char *hex = os_malloc(hex_len);

		if (hex) {
			wpa_snprintf_hex(hex, hex_len, rx_mgmt->frame,
					 rx_mgmt->frame_len);
			wpa_msg(hapd->msg_ctx, MSG_INFO, "MGMT-RX %s", hex);
			os_free(hex);
		}
		return 1;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	hdr = (const struct ieee80211_hdr *) rx_mgmt->frame;
	bssid = get_hdr_bssid(hdr, rx_mgmt->frame_len);
	if (bssid == NULL)
		return 0;

	hapd = get_hapd_bssid(iface, bssid, rx_mgmt->link_id);

	if (!hapd) {
		u16 fc = le_to_host16(hdr->frame_control);

		/*
		 * Drop frames to unknown BSSIDs except for Beacon & MAPC frames which
		 * could be used to update neighbor information.
		 */
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON ||
#ifdef CONFIG_IEEE80211BN
		     (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION &&
		      hostapd_is_mapc_action((const struct ieee80211_mgmt *) rx_mgmt->frame,
			                      rx_mgmt->frame_len))
#else
		     0
#endif
		     )) {
			/*
			 * For MAPC frames, route to the hapd whose own_addr
			 * matches the DA (A1). This ensures the request/response
			 * reaches the intended hapd/BSS.
			 */
			if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION) {
				size_t i;

				for (i = 0; i < iface->num_bss; i++) {
					if (iface->bss[i] &&
					    ether_addr_equal(hdr->addr1,
							    iface->bss[i]->own_addr)) {
						hapd = iface->bss[i];
						break;
					}
				}
			}
			if (!hapd) {
#ifdef CONFIG_IEEE80211BN
				if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION &&
				    is_broadcast_ether_addr(hdr->addr1)) {
					size_t j;

					for (j = 0; j < iface->num_bss; j++) {
						struct hostapd_data *b = iface->bss[j];

						if (b && b->conf &&
						    b->conf->mapc_conf &&
						    (b->conf->mapc_conf->mapc_capability_bitmap &
						     MAPC_SCHEME_CAP_MASK)) {
							hapd = b;
							break;
						}
					}
				}
				if (!hapd)
#endif /* CONFIG_IEEE80211BN */
					hapd = iface->bss[0];
			}
		} else
			return 0;
	}

	os_memset(&fi, 0, sizeof(fi));
	fi.freq = rx_mgmt->freq;
	fi.datarate = rx_mgmt->datarate;
	fi.ssi_signal = rx_mgmt->ssi_signal;
	fi.smd_ctx = rx_mgmt->smd_ctx;

	if (hapd == HAPD_BROADCAST) {
		size_t i;

		ret = 0;
		for (i = 0; i < iface->num_bss; i++) {
			/* if bss is set, driver will call this function for
			 * each bss individually. */
			if (rx_mgmt->drv_priv &&
			    (iface->bss[i]->drv_priv != rx_mgmt->drv_priv))
				continue;

			if (ieee802_11_mgmt(iface->bss[i], rx_mgmt->frame,
					    rx_mgmt->frame_len, &fi) > 0)
				ret = 1;
		}
	} else
		ret = ieee802_11_mgmt(hapd, rx_mgmt->frame, rx_mgmt->frame_len,
				      &fi);

	random_add_randomness(&fi, sizeof(fi));

	return ret;
}


static void hostapd_mgmt_tx_cb(struct hostapd_data *hapd, const u8 *buf,
			       size_t len, u16 stype, int ok, int link_id)
{
	struct ieee80211_hdr *hdr;
	struct hostapd_data *orig_hapd, *tmp_hapd;
	const u8 *bssid;

	orig_hapd = hapd;

	hdr = (struct ieee80211_hdr *) buf;
	hapd = switch_link_hapd(hapd, link_id);
	bssid = get_hdr_bssid(hdr, len);
	tmp_hapd = get_hapd_bssid(hapd->iface, bssid, link_id);
	if (tmp_hapd) {
		hapd = tmp_hapd;
#ifdef CONFIG_IEEE80211BE
	} else if (hapd->conf->mld_ap && bssid &&
		   ether_addr_equal(hapd->mld->mld_addr, bssid)) {
		/* AP MLD address match - use hapd pointer as-is */
#endif /* CONFIG_IEEE80211BE */
	} else {
		return;
	}

	if (hapd == HAPD_BROADCAST) {
		if (stype != WLAN_FC_STYPE_ACTION || len <= 25 ||
		    buf[24] != WLAN_ACTION_PUBLIC)
			return;
		hapd = get_hapd_bssid(orig_hapd->iface, hdr->addr2, link_id);
		if (!hapd || hapd == HAPD_BROADCAST)
			return;
		/*
		 * Allow processing of TX status for a Public Action frame that
		 * used wildcard BBSID.
		 */
	}

	ieee802_11_mgmt_cb(hapd, buf, len, stype, ok);
}

#endif /* NEED_AP_MLME */


static int hostapd_event_new_sta(struct hostapd_data *hapd, const u8 *addr, u32 flags)
{
	struct radius_sta out;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Data frame from unknown STA " MACSTR
			  " - adding a new STA, flags %d", MAC2STR(addr), flags);
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL) {
			wpa_printf(MSG_DEBUG, "Failed to add STA entry for " MACSTR,
				  MAC2STR(addr));
			return -1;
		}
	} else {
		if (!(sta->flags & WIRED_STA_MAB)) {
			return 0;
		}
	}

	if (flags & WIRED_STA_MAB) {
		/* for MAB non-802.1x wired station
		 * try mac authentication with external radius server */
		sta->flags |= WIRED_STA_MAB;
		wpa_printf(MSG_DEBUG, "try mac authentication with externel radius for QCA STA "
				MACSTR, MAC2STR(addr));
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		hostapd_allowed_address(hapd, addr, NULL, 0, &out, false);
		eloop_register_timeout(6, 0, hostapd_mac_auth_timeout, hapd, sta);
	} else {
		sta->flags &= (~WIRED_STA_MAB);
		hostapd_new_assoc_sta(hapd, sta, 0);
	}
	return 0;
}


static void hostapd_event_eapol_rx(struct hostapd_data *hapd, const u8 *src,
				   const u8 *data, size_t data_len,
				   enum frame_encryption encrypted,
				   int link_id)
{
	struct hostapd_data *orig_hapd = hapd;

#ifdef CONFIG_IEEE80211BE
	hapd = switch_link_hapd(hapd, link_id);
	hapd = hostapd_find_by_sta(hapd->iface, src, true, NULL);
#else /* CONFIG_IEEE80211BE */
	hapd = hostapd_find_by_sta(hapd->iface, src, false, NULL);
#endif /* CONFIG_IEEE80211BE */

	if (!hapd) {
		/* WLAN cases need to have an existing association, but non-WLAN
		 * cases (mainly, wired IEEE 802.1X) need to be able to process
		 * EAPOL frames from new devices that do not yet have a STA
		 * entry and as such, do not get a match in
		 * hostapd_find_by_sta(). */
		wpa_printf(MSG_DEBUG,
			   "No STA-specific hostapd instance for EAPOL RX found - fall back to initial context");
		hapd = orig_hapd;
	}

	ieee802_1x_receive(hapd, src, data, data_len, encrypted);
}

#endif /* HOSTAPD */


static struct hostapd_channel_data *
hostapd_get_mode_chan(struct hostapd_hw_modes *mode, unsigned int freq)
{
	int i;
	struct hostapd_channel_data *chan;

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if ((unsigned int) chan->freq == freq)
			return chan;
	}

	return NULL;
}


static struct hostapd_channel_data * hostapd_get_mode_channel(
	struct hostapd_iface *iface, unsigned int freq)
{
	int i;
	struct hostapd_channel_data *chan;

	for (i = 0; i < iface->num_hw_features; i++) {
		if (hostapd_hw_skip_mode(iface, &iface->hw_features[i]))
			continue;
		chan = hostapd_get_mode_chan(&iface->hw_features[i], freq);
		if (chan)
			return chan;
	}

	return NULL;
}


#ifndef CONFIG_QCN_EXTN
static
#endif
void hostapd_update_nf(struct hostapd_iface *iface,
		       struct hostapd_channel_data *chan,
		       struct freq_survey *survey)
{
	if (!iface->chans_surveyed) {
		chan->min_nf = survey->nf;
		iface->lowest_nf = survey->nf;
	} else {
		if (dl_list_empty(&chan->survey_list))
			chan->min_nf = survey->nf;
		else if (survey->nf < chan->min_nf)
			chan->min_nf = survey->nf;
		if (survey->nf < iface->lowest_nf)
			iface->lowest_nf = survey->nf;
	}
}


static void hostapd_single_channel_get_survey(struct hostapd_iface *iface,
					      struct survey_results *survey_res)
{
	struct hostapd_channel_data *chan;
	struct freq_survey *survey;
	u64 divisor, dividend;

	survey = dl_list_first(&survey_res->survey_list, struct freq_survey,
			       list);
	if (!survey || !survey->freq)
		return;

	chan = hostapd_get_mode_channel(iface, survey->freq);
	if (!chan || chan->flag & HOSTAPD_CHAN_DISABLED)
		return;

	wpa_printf(MSG_DEBUG,
		   "Single Channel Survey: (freq=%d channel_time=%ld channel_time_busy=%ld)",
		   survey->freq,
		   (unsigned long int) survey->channel_time,
		   (unsigned long int) survey->channel_time_busy);

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_cbs_handle_single_channel_survey(iface, chan, survey))
		return;
#endif

	if (survey->channel_time > iface->last_channel_time &&
	    survey->channel_time > survey->channel_time_busy) {
		dividend = survey->channel_time_busy -
			iface->last_channel_time_busy;
		divisor = survey->channel_time - iface->last_channel_time;

		iface->channel_utilization = dividend * 255 / divisor;
		wpa_printf(MSG_DEBUG, "Channel Utilization: %d",
			   iface->channel_utilization);
		os_get_reltime(&iface->ch_util_update_time);
	}
	iface->last_channel_time = survey->channel_time;
	iface->last_channel_time_busy = survey->channel_time_busy;
}


void hostapd_event_get_survey(struct hostapd_iface *iface,
			      struct survey_results *survey_results)
{
	struct freq_survey *survey, *tmp;
	struct hostapd_channel_data *chan;

	if (dl_list_empty(&survey_results->survey_list)) {
		wpa_printf(MSG_DEBUG, "No survey data received");
		return;
	}

	if (survey_results->freq_filter) {
		hostapd_single_channel_get_survey(iface, survey_results);
		return;
	}

	dl_list_for_each_safe(survey, tmp, &survey_results->survey_list,
			      struct freq_survey, list) {
		chan = hostapd_get_mode_channel(iface, survey->freq);
		if (!chan)
			continue;
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (!(chan->flag & HOSTAPD_CHAN_SURVEY_LIST_INITIALIZED))
			continue;

		dl_list_del(&survey->list);
		dl_list_add_tail(&chan->survey_list, &survey->list);

		hostapd_update_nf(iface, chan, survey);

		iface->chans_surveyed++;
	}
}


#ifdef HOSTAPD
#ifdef NEED_AP_MLME

static void hostapd_event_iface_unavailable(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "Interface %s is unavailable -- stopped",
		   hapd->conf->iface);

	if (hapd->csa_in_progress) {
		wpa_printf(MSG_INFO, "CSA failed (%s was stopped)",
			   hapd->conf->iface);
		hostapd_switch_channel_fallback(hapd->iface,
						&hapd->cs_freq_params);
	}

	/* Set beacon_set_done to false so the RNR and other beacon params are properly
	 * updated */
	hapd->beacon_set_done = 0;
}


static void hostapd_event_dfs_radar_detected(struct hostapd_data *hapd,
					     struct dfs_event *radar)
{

	wpa_printf(MSG_DEBUG, "DFS radar detected on %d MHz", radar->freq);
	if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
		wpa_printf(MSG_INFO,
			   "Ignoring since freq %d is out of own range",
			   radar->freq);
		return;
	}

	hostapd_dfs_radar_detected(hapd->iface, radar->freq, radar->ht_enabled,
				   radar->chan_offset, radar->chan_width,
				   radar->cf1, radar->cf2, radar->radar_bitmap,
				   radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}

#ifdef CONFIG_QCN_EXTN
static void hostapd_event_awgn_detected(struct hostapd_data *hapd,
					 struct awgn_event *awgn_info)
{
	hostapd_intf_awgn_detected(hapd->iface, awgn_info->freq, awgn_info->chan_width,
				   awgn_info->cf1, awgn_info->cf2,
				   awgn_info->chan_bw_interference_bitmap);
}

static void hostapd_event_afc_received(struct hostapd_data *hapd)
{
	hostapd_intf_afc_received(hapd->iface);
}
#endif /* CONFIG_QCN_EXTN */

static void hostapd_event_dfs_pre_cac_expired(struct hostapd_data *hapd,
					      struct dfs_event *radar)
{
    if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
        wpa_msg(hapd->iface->bss[0]->msg_ctx,
                MSG_INFO, DFS_EVENT_CAC_COMPLETED
                "Ignoring since freq info is out of own range");
        return;
    }

	wpa_printf(MSG_DEBUG, "DFS Pre-CAC expired on %d MHz", radar->freq);
	hostapd_dfs_pre_cac_expired(hapd->iface, radar->freq, radar->ht_enabled,
				    radar->chan_offset, radar->chan_width,
				    radar->cf1, radar->cf2,
				    radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}


static void hostapd_event_dfs_cac_finished(struct hostapd_data *hapd,
					   struct dfs_event *radar)
{
    if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
        wpa_msg(hapd->iface->bss[0]->msg_ctx,
                MSG_INFO, DFS_EVENT_CAC_COMPLETED
                "Ignoring since freq info is out of own range");
        return;
    }

	wpa_printf(MSG_DEBUG, "DFS CAC finished on %d MHz", radar->freq);
	hostapd_dfs_complete_cac(hapd->iface, 1, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2, radar->radar_bitmap,
				 radar->is_background,
				 radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}


static void hostapd_event_dfs_cac_aborted(struct hostapd_data *hapd,
					  struct dfs_event *radar)
{
    if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
        wpa_msg(hapd->iface->bss[0]->msg_ctx,
                MSG_INFO, DFS_EVENT_CAC_COMPLETED
                "Ignoring since freq info is out of own range");
        return;
    }

	wpa_printf(MSG_DEBUG, "DFS CAC aborted on %d MHz", radar->freq);
	hostapd_dfs_complete_cac(hapd->iface, 0, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2, radar->radar_bitmap,
				 radar->is_background,
				 radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}


static void hostapd_event_dfs_nop_finished(struct hostapd_data *hapd,
					   struct dfs_event *radar)
{
    if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
        wpa_msg(hapd->iface->bss[0]->msg_ctx,
                MSG_INFO, DFS_EVENT_NOP_FINISHED
                "Ignoring since freq info is out of own range");
        return;
    }

	wpa_printf(MSG_DEBUG, "DFS NOP finished on %d MHz", radar->freq);
	hostapd_dfs_nop_finished(hapd->iface, radar->freq, radar->ht_enabled,
				 radar->chan_offset, radar->chan_width,
				 radar->cf1, radar->cf2,
				 radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}


static void hostapd_event_dfs_cac_started(struct hostapd_data *hapd,
					  struct dfs_event *radar)
{
    if (!hostapd_is_freq_in_current_hw_info(hapd->iface, radar->freq)) {
        wpa_msg(hapd->iface->bss[0]->msg_ctx,
                MSG_INFO, DFS_EVENT_CAC_START
                "Ignoring since freq info is out of own range");
        return;
    }

	wpa_printf(MSG_DEBUG, "DFS offload CAC started on %d MHz", radar->freq);
	hostapd_dfs_start_cac(hapd->iface, radar->freq, radar->ht_enabled,
			      radar->chan_offset, radar->chan_width,
			      radar->cf1, radar->cf2, radar->is_background,
			      radar->chan_width_device, radar->cf_device);
    radar->is_dfs_event_on_curr_hw = true;
}

#endif /* NEED_AP_MLME */


static void hostapd_event_wds_sta_interface_status(struct hostapd_data *hapd,
						   int istatus,
						   const char *ifname,
						   const u8 *addr)
{
	struct sta_info *sta = ap_get_sta(hapd, addr);
	char chain[32];

	if (sta) {
		os_free(sta->ifname_wds);
		if (istatus == INTERFACE_ADDED) {
			sta->ifname_wds = os_strdup(ifname);
		} else {
			sta->ifname_wds = NULL;
		}
	}

#ifdef CONFIG_IEEE80211AX
	if (hapd->conf->scs) {
		os_snprintf(chain, sizeof(chain), "%s_%s", CHAIN_NAME, ifname);
		if (istatus == INTERFACE_ADDED)
			hostapd_config_nft_chain_for_dev(TABLE_NAME, chain,
							 (char *)ifname, true);
		else
			hostapd_config_nft_chain_for_dev(TABLE_NAME, chain,
							 (char *)ifname, false);
	}
#endif

	wpa_msg(hapd->msg_ctx, MSG_INFO, "%sifname=%s sta_addr=" MACSTR,
		istatus == INTERFACE_ADDED ?
		WDS_STA_INTERFACE_ADDED : WDS_STA_INTERFACE_REMOVED,
		ifname, MAC2STR(addr));
}

static void hostapd_event_update_muedca_params(struct hostapd_data *hapd,
					       struct update_muedca *params)
{
	struct hostapd_data *selected_hapd;
	bool radio_matched = false;
	int i;

	if (hapd->conf->mld_ap) {
#ifdef CONFIG_QCN_EXTN
		for_each_mld_link_include_repurposed(selected_hapd, hapd) {
#else
		for_each_mld_link(selected_hapd, hapd) {
#endif /* CONFIG_QCN_EXTN */
			if (!selected_hapd->iface ||
			    !selected_hapd->iface->current_hw_info)
				continue;

			if (selected_hapd->iface->current_hw_info->hw_idx ==
							    params->radio_idx) {
				radio_matched = true;
				break;
			}
		}

		if (!radio_matched) {
			wpa_printf(MSG_DEBUG,
				   "Radio index %u does not match HW index for any interface",
				   params->radio_idx);
			return;
		}

		hapd = selected_hapd;
	}

	/* Update current MU-EDCA parameters */
	for (i = 0; i < 3; i++) {
		hapd->iface->conf->he_mu_edca.he_mu_ac_be_param[i] =
						params->he_mu_ac_be_param[i];
		hapd->iface->conf->he_mu_edca.he_mu_ac_bk_param[i] =
						params->he_mu_ac_bk_param[i];
		hapd->iface->conf->he_mu_edca.he_mu_ac_vo_param[i] =
						params->he_mu_ac_vo_param[i];
		hapd->iface->conf->he_mu_edca.he_mu_ac_vi_param[i] =
						params->he_mu_ac_vi_param[i];
	}

	/* Increment Parameter Set Update Count for MU-EDCA and WME EDCA only
	 * if any STA is connected
	 */
	if (hapd->num_sta)
		hapd->parameter_set_count++;

	/* Update beacon with updated MU-EDCA parameters */
	if (ieee802_11_set_beacon(hapd))
		wpa_printf(MSG_DEBUG,
			   "Failed to update beacons with MU-EDCA parameters");
}

static void hostapd_event_6ghz_power_mode(struct hostapd_data *hapd,
					  u8 he_6ghz_power_mode)
{
	struct hostapd_config *conf = hapd->iconf;
	struct hostapd_iface *iface = hapd->iface;
	int ret;

	if (iface->power_mode_6ghz_before_change > -1 &&
	    he_6ghz_power_mode != iface->power_mode_6ghz_before_change) {
		wpa_printf(MSG_ERROR, "Invalid power mode sent by the target");
		return;
	}

	conf->he_6ghz_reg_pwr_type = he_6ghz_power_mode;
	hostapd_get_hw_features(iface);
	ret = hostapd_select_hw_mode(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "Could not change 6GHZ power mode(%d)",
			   ret);
		hostapd_disable_iface(iface);
	}

	if (hostapd_set_current_hw_info(iface, iface->freq)) {
		wpa_printf(MSG_ERROR, "Failed to get operating hw mac id");
		hostapd_disable_iface(iface);
	}

	hostapd_clear_local_tpe(hapd->iface);

	if (ieee802_11_update_beacons(hapd->iface))
		wpa_printf(MSG_DEBUG,
			   "Failed to update beacons with new pwr_mode");

	iface->power_mode_6ghz_before_change = -1;
}

static int hostapd_allocate_afc_rsp_info(struct hostapd_iface *iface,
					 struct afc_sp_reg_info *afc_rsp_info)
{
	struct afc_sp_reg_info *afc_response = NULL;
	struct afc_freq_obj *afc_freq_info = NULL;
	struct afc_chan_obj *afc_chan_info = NULL;
	struct chan_eirp_obj *chan_eirp_info = NULL;
	u8 i;

	afc_response = os_malloc(sizeof(*afc_response));
	if (!afc_response) {
		wpa_printf(MSG_DEBUG, "afc_response allocation failed");
		return -ENOMEM;
	}

	os_memcpy(afc_response, afc_rsp_info, sizeof(*afc_response));
	afc_response->afc_freq_info = NULL;
	afc_response->afc_chan_info = NULL;
	iface->afc_rsp_info = afc_response;

	if (!afc_rsp_info->num_freq_objs || !afc_rsp_info->num_chan_objs) {
		wpa_printf(MSG_DEBUG, "No AFC Freq / Chan objects");
		return 0;
	}

	afc_freq_info = os_malloc(afc_rsp_info->num_freq_objs *
				  sizeof(*afc_freq_info));
	if (!afc_freq_info) {
		wpa_printf(MSG_DEBUG, "afc_freq_info allocation failed");
		os_free(afc_response);
		iface->afc_rsp_info = NULL;
		return -ENOMEM;
	}

	os_memcpy(afc_freq_info, afc_rsp_info->afc_freq_info,
		  (afc_rsp_info->num_freq_objs *
		   sizeof(*afc_freq_info)));
	iface->afc_rsp_info->afc_freq_info = afc_freq_info;
	afc_chan_info = os_malloc(afc_rsp_info->num_chan_objs *
				  sizeof(*afc_chan_info));
	if (!afc_chan_info) {
		wpa_printf(MSG_DEBUG, "afc_chan_info allocation failed");
		os_free(afc_freq_info);
		iface->afc_rsp_info->afc_freq_info = NULL;
		os_free(afc_response);
		iface->afc_rsp_info = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < afc_response->num_chan_objs; i++) {
		afc_chan_info[i].global_opclass =
				afc_rsp_info->afc_chan_info[i].global_opclass;
		afc_chan_info[i].num_chans =
				afc_rsp_info->afc_chan_info[i].num_chans;
		chan_eirp_info =  os_malloc(afc_chan_info[i].num_chans *
					    sizeof(*chan_eirp_info));
		if (!chan_eirp_info) {
			wpa_printf(MSG_DEBUG, "chan_eirp_info allocation failed");
			os_free(afc_chan_info);
			afc_chan_info = NULL;
			os_free(afc_freq_info);
			iface->afc_rsp_info->afc_freq_info = NULL;
			os_free(afc_response);
			iface->afc_rsp_info = NULL;
			return -ENOMEM;
		}

		os_memcpy(chan_eirp_info,
			  afc_rsp_info->afc_chan_info[i].chan_eirp_info,
			  afc_chan_info[i].num_chans * sizeof(*chan_eirp_info));
		afc_chan_info[i].chan_eirp_info = chan_eirp_info;
	}

	iface->afc_rsp_info->afc_chan_info = afc_chan_info;

	return 0;
}

void
afc_channel_change_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;

	if (!iface->is_afc_channel_change_pending) {
		wpa_printf(MSG_DEBUG, "AFC channel change already completed");
		return;
	}
	wpa_printf(MSG_DEBUG, "AFC channel change timeout, try channel change once");
	hostapd_handle_afc_channel_change(iface);
}

/**
 * hostapd_afc_find_iface - Find the hostapd iface matching an AFC event
 *
 * Three-tier hw_idx matching:
 * 1. No multi-radio info: single-radio phy — match phy name only.
 * 2. multi_hw_info present, current_hw_info set: match current_hw_info->hw_idx.
 * 3. multi_hw_info present, current_hw_info NULL: match conf->radio_idx.
 */
#ifdef CONFIG_QCN_EXTN
struct hostapd_iface *
#else
static struct hostapd_iface *
#endif /* CONFIG_QCN_EXTN */
hostapd_afc_find_iface(struct hostapd_data *hapd, int hw_idx)
{
	const char *phy_name = hostapd_drv_get_radio_name(hapd);
	int i;

	if (!phy_name)
		return NULL;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		struct hostapd_iface *h = hapd->iface->interfaces->iface[i];
		const char *h_phy;

		/* Skip ifaces that are being torn down */
		if (!h->num_bss || !h->bss[0] || !h->conf)
			continue;

		h_phy = hostapd_drv_get_radio_name(h->bss[0]);
		if (!h_phy || os_strcmp(h_phy, phy_name) != 0)
			continue;

		/* Single-radio phy: no multi_hw_info — match by phy name only */
		if (!h->num_multi_hws) {
			wpa_printf(MSG_INFO,
				   "AFC: Found Target iface for single radio phy");
			return h;
		}

		/* Multi-radio: current_hw_info already set — match hw_idx */
		if (h->current_hw_info) {
			if (h->current_hw_info->hw_idx == (u8)hw_idx) {
				wpa_printf(MSG_INFO,
					   "AFC: Found Target iface - cur HW idx match");
				return h;
			}
			continue;
		}

		/* Multi-radio: freq=0 (ACS) — match via conf->radio_idx */
		if (h->conf->radio_idx >= 0 &&
		    h->conf->radio_idx == hw_idx) {
			wpa_printf(MSG_INFO,
				   "AFC: Found Target iface - conf radio_idx match");
			return h;
		}
	}
	return NULL;
}

static void hostapd_event_afc_update_complete(
		struct hostapd_data *hapd,
		struct afc_info *afc_info)
{
	struct afc_sp_reg_info *afc_rsp_info = &afc_info->afc_rsp_info;
	struct hostapd_iface *iface = NULL;
	bool is_connected_repeater;

	wpa_printf(MSG_DEBUG, "AFC response event received through %s",
		   hapd->iface->phy);
	iface = hostapd_afc_find_iface(hapd, afc_info->hw_idx);
	if (!iface) {
		wpa_printf(MSG_ERROR, "No matching hostapd interface found for AFC update");
		return;
	}

	hostapd_free_afc_data(iface);
	if (hostapd_allocate_afc_rsp_info(iface, afc_rsp_info)) {
		wpa_printf(MSG_DEBUG, "AFC response memory allocation failed");
		/* Cannot store AFC data - nothing to act on */
		return;
	}
	iface->is_afc_power_event_received =
		afc_rsp_info->target_status_code ==
		QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS;

#ifdef CONFIG_QCN_EXTN
	hostapd_set_afc_regd_wait(iface);
#endif /* CONFIG_QCN_EXTN */

	hapd = iface->bss[0];
	if (!iface->is_afc_power_event_received) {
		enum hostapd_afc_power_sync_result sync_result;

		wpa_printf(MSG_INFO,
			   "AFC power update failed: iface=%s status=%u server_resp=%d",
			   iface->phy, afc_rsp_info->target_status_code,
			   afc_rsp_info->serv_resp_code);
		iface->is_afc_channel_change_pending = false;
		iface->is_afc_repeater_power_sync_pending = false;

		if (!hostapd_drv_is_retail_afc_supported(iface->bss[0]))
			return;

		if (hostapd_iface_has_connected_backhaul_sta(iface)) {
			/*
			 * Connected repeater: channel switch is unsafe while
			 * the backhaul STA is active.  Try same-channel non-SP
			 * fallback; disconnect backhaul if no mode is valid.
			 */
			sync_result = hostapd_force_afc_non_sp_power_mode(iface);
			if (sync_result == HOSTAPD_AFC_PWR_SYNC_UPDATED ||
			    sync_result == HOSTAPD_AFC_PWR_SYNC_NOOP ||
			    sync_result == HOSTAPD_AFC_PWR_SYNC_DEFERRED)
				return;

			/* INVALID_CURRENT or ERROR: no valid fallback */
			wpa_printf(MSG_ERROR,
				   "AFC power update failed: fallback unavailable, disconnect backhaul STA iface=%s result=%d",
				   iface->phy, sync_result);
			if (hostapd_disconnect_backhaul_sta(iface))
				wpa_printf(MSG_ERROR,
					   "AFC power update failed: backhaul disconnect failed iface=%s",
					   iface->phy);
			return;
		}

		/*
		 * Root AP path: mirror payload reset by gating on SP mode.
		 * A non-SP root AP has no regulatory urgency — return.
		 */
		if (!he_reg_is_sp(iface->conf->he_6ghz_reg_pwr_type))
			return;

		if (iface->conf->enable_best_power_mode) {
			/*
			 * Root SP AP + BPM enabled: run channel-selection
			 * recovery directly; do not attempt same-channel
			 * fallback first.
			 */
			wpa_printf(MSG_INFO,
				   "AFC power update failed: BPM channel recovery iface=%s",
				   iface->phy);
			iface->is_afc_channel_change_pending = true;
			/* Wait for NL80211_WIPHY_REG_CHANGE to get updated channel list */
			eloop_register_timeout(5, 0,
					       afc_channel_change_timeout,
					       iface, NULL);
			return;
		}

		/* Root SP AP + BPM disabled: same-channel fallback then NO_IR */
		sync_result = hostapd_force_afc_non_sp_power_mode(iface);
		if (sync_result == HOSTAPD_AFC_PWR_SYNC_UPDATED ||
		    sync_result == HOSTAPD_AFC_PWR_SYNC_NOOP)
			return;

		/*
		 * DEFERRED, INVALID_CURRENT, or ERROR: no retry path exists
		 * for root AP + BPM disabled. Clear the pending flag before
		 * setting NO_IR to keep the repeater-pending invariant clean.
		 */
		iface->is_afc_repeater_power_sync_pending = false;
		wpa_printf(MSG_ERROR,
			   "AFC power update failed: no valid non-SP fallback iface=%s result=%d",
			   iface->phy, sync_result);
		hostapd_set_no_ir_state(iface);
		return;
	}

	is_connected_repeater = hostapd_iface_has_connected_backhaul_sta(iface);
	if (is_connected_repeater) {
		/*
		 * Do not trigger a channel change while the backhaul STA is
		 * connected. AFC response data is stored; defer power mode
		 * evaluation to hostapd_run_pending_repeater_afc_power_sync()
		 * which runs after the REGDOM_SET_BY_DRIVER event delivers
		 * the updated channel list. This ensures SP validity is
		 * checked against fresh regulatory data, not stale flags.
		 */
		wpa_printf(MSG_INFO,
			   "AFC repeater power sync pending: iface=%s freq=%d",
			   iface->phy, iface->freq);
		iface->is_afc_repeater_power_sync_pending = true;
		return;
	}

	if (hapd->driver && hapd->driver->is_only_afc_power_fetch &&
	    hapd->drv_priv) {
		bool is_only_afc_power_fetch =
			hapd->driver->is_only_afc_power_fetch(hapd->drv_priv);

		if (is_only_afc_power_fetch) {
			enum hostapd_afc_power_sync_result power_sync_result =
				hostapd_sync_current_afc_power_mode(iface, false);

			/*
			 * Result intentionally ignored: this path is advisory
			 * (retail mode, power-fetch only). Log any non-NOOP
			 * result for diagnosability.
			 */
			if (power_sync_result != HOSTAPD_AFC_PWR_SYNC_NOOP)
				wpa_printf(MSG_DEBUG,
					   "AFC power fetch sync result %d iface=%s",
					   power_sync_result, iface->phy);
			return;
		}
	}

	if (hostapd_drv_is_retail_afc_supported(hapd)) {
		if (!iface->conf->enable_best_power_mode) {
			wpa_printf(MSG_ERROR,
				   "Skip power event action for retail AFC use cases - BPM not enabled");
			return;
		}
		iface->is_afc_channel_change_pending = true;
		/* Wait for NL8011_WIPHY_REG_CHANGE event to get the updated channel list */
		eloop_register_timeout(5, 0, afc_channel_change_timeout, iface,
				       NULL);
	}
}

/*
  * The TBTT count to be passed as an argument to a function
  * to disable a BSS when it is part of an MLD (Multi Link Device)
  */
#define MLO_DIS_BSS_TBTT_COUNT 5

/*
 * The TBTT count to be passed as an argument to a function
 *  to disable a BSS when it is a single link BSS.
 */
#define SLO_DIS_BSS_TBTT_COUNT 0

/**
 * hostapd_get_disabling_tbtt_count - Get TBTT grace count before BSS disable
 * @hapd: Pointer to hostapd BSS context
 *
 * Determine the number of Target Beacon Transmission Time (TBTT) intervals
 * to wait before disabling the given BSS.
 *
 * This helper is used by NO-IR and regulatory handling paths that need to
 * disable a BSS gracefully instead of immediately tearing it down.
 * The returned TBTT count is passed to hostapd_disable_bss() to delay the
 * actual disable operation by the specified number of beacon intervals.
 *
 * For Single-Link Operation (SLO) or legacy BSSes, the BSS can be disabled
 * immediately since there is no multi-link dependency. In this case,
 * SLO_DIS_BSS_TBTT_COUNT (typically 0) is returned, resulting in an
 * immediate disable.
 *
 * For Multi-Link Operation (MLO) APs, a non-zero TBTT grace
 * count (MLO_DIS_BSS_TBTT_COUNT) is returned. This allows:
 *   - Associated MLO stations to receive beacon updates indicating link or
 *     BSS disable,
 *   - Proper propagation of link disable information across the MLD, and
 *   - Orderly teardown of links before the transmitting BSS is disabled.
 *
 * Returns the number of Target Beacon Transmission Time (TBTT) intervals
 * to wait before disabling a BSS.
 */
static int hostapd_get_disabling_tbtt_count(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		return MLO_DIS_BSS_TBTT_COUNT;
#endif
	return SLO_DIS_BSS_TBTT_COUNT;
}


/**
 * hostapd_disable_single_bss_no_ir - Disable a single BSS in NO-IR condition
 * @hapd: Pointer to hostapd BSS context
 */
static void hostapd_disable_single_bss_no_ir(struct hostapd_data *hapd)
{
	int ret, tbtt_count;

	if (!hapd || !hapd->started)
		return;

	tbtt_count = hostapd_get_disabling_tbtt_count(hapd);
	wpa_printf(MSG_DEBUG,
			"%s: Disabling %s BSS %s with TBTT=%d",
			__func__,
#ifdef CONFIG_IEEE80211BE
			hapd->conf->mld_ap ? "MLO" : "SLO/legacy",
#else
			"SLO/legacy",
#endif
			hapd->conf->iface, tbtt_count);
	hostapd_cleanup_cs_params(hapd);

	ret = hostapd_disable_bss(hapd, tbtt_count, AP_EVENT_NO_IR);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to disable %s",
				hapd->conf->iface);
		hostapd_drv_stop_ap(hapd);
		hostapd_no_ir_cleanup(hapd);
	}
}

/**
 * hostapd_disable_no_ir_bss_members - Disable selected BSS members under NO-IR
 * @iface: Pointer to hostapd interface context
 * @cat: BSS disable category selector
 */
static void hostapd_disable_no_ir_bss_members(struct hostapd_iface *iface,
					      enum hostapd_bss_category cat)
{
	int i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		if (!hapd || !hapd->started)
			continue;

		if (!hostapd_is_bss_in_category(hapd, cat))
			continue;

		hostapd_disable_single_bss_no_ir(hapd);
	}
}


/**
 * hostapd_disable_no_ir_mbssids - Disable MBSSID BSSes under
 * when all channels in the iface have become NO_IR
 * @iface: Pointer to hostapd interface context
 */
static void hostapd_disable_no_ir_mbssids(struct hostapd_iface *iface)
{
	/*
	 * Order of disabling BSSes is important:
	 * first disable non‑TX BSSes, then disable the TX BSS last.
	 */
	hostapd_disable_no_ir_bss_members(iface, CAT_NON_TX_BSS);
	hostapd_disable_no_ir_bss_members(iface, CAT_TX_BSS);
}


/**
 * hostapd_disable_no_ir_non_mbssids - Disable all BSSes
 * when all channels in the iface have become NO_IR
 * @iface: Pointer to hostapd interface context
 *
 */
static void hostapd_disable_no_ir_non_mbssids(struct hostapd_iface *iface)
{
	hostapd_disable_no_ir_bss_members(iface, CAT_ALL_BSS);
}

void
hostapd_set_no_ir_state(struct hostapd_iface *iface)
{
	hostapd_set_state(iface, HAPD_IFACE_NO_IR);
	hostapd_interface_update_fils_ubpr(iface, false);
	iface->is_no_ir = true;

	wpa_printf(MSG_DEBUG, "%s: AFC NO_IR", __func__);

	if (iface->conf->mbssid != MBSSID_DISABLED)
		hostapd_disable_no_ir_mbssids(iface);
	else
		hostapd_disable_no_ir_non_mbssids(iface);

	hostapd_cleanup_iface_partial(iface);
	hostapd_refresh_other_iface_beacons(iface);
}

/**
 * hostapd_event_afc_payload_reset - Reset AFC payload of the hostapd.
 *
 * Mark a flag to signal that an AFC channel switch is pending. Once
 * the NL8011_WIPHY_REG_CHANGE event is received, the channel change
 * is attempted to best power mode available. If the  NL8011_WIPHY_REG_CHANGE
 * is not received within the timeout period, afc_channel_change_timeout()
 * is invoked to take further action.
 *
 * @hapd: Pointer to hostapd_data structure
 * @afc_rsp_info: Pointer to afc_sp_reg_info structure
 *
 * Returns: None
 */
static void
hostapd_event_afc_payload_reset(struct hostapd_data *hapd,
				struct afc_info *afc_info)
{
	struct hostapd_iface *iface = NULL;

	wpa_printf(MSG_DEBUG, "AFC Reset event received through %s",
		   hapd->iface->phy);
	iface = hostapd_afc_find_iface(hapd, afc_info->hw_idx);
	if (!iface) {
		wpa_printf(MSG_DEBUG, "No matching hostapd interface found for AFC reset");
		return;
	}

	wpa_printf(MSG_DEBUG, "Processing AFC payload reset event");
	if (!iface->afc_rsp_info) {
		wpa_printf(MSG_INFO,
			   "AFC payload reset: no cached payload iface=%s",
			   iface->phy);
	} else {
		/* Clear AFC payload */
		hostapd_free_afc_data(iface);
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_set_afc_regd_wait(iface);
#endif /* CONFIG_QCN_EXTN */

	iface->is_afc_power_event_received = false;
	iface->is_afc_repeater_power_sync_pending = false;
	if (!hostapd_drv_is_retail_afc_supported(iface->bss[0])) {
		wpa_printf(MSG_DEBUG, "AFC payload reset not supported");
		return;
	}

	/*
	 * A payload reset means AFC data is no longer valid. For a
	 * connected repeater, do not trigger a channel change as that
	 * would tear down the dual-channel state while the backhaul
	 * STA is still active and cause a crash. Attempt same-channel
	 * non-SP fallback first; disconnect the backhaul STA only if
	 * no valid non-SP mode exists (INVALID_CURRENT or hard ERROR).
	 */
	if (hostapd_iface_has_connected_backhaul_sta(iface)) {
		enum hostapd_afc_power_sync_result sync_result;

		wpa_printf(MSG_DEBUG,
			   "AFC payload reset: connected repeater, attempt non-SP fallback iface=%s",
			   iface->phy);
		iface->is_afc_channel_change_pending = false;
		sync_result = hostapd_force_afc_non_sp_power_mode(iface);
		if (sync_result == HOSTAPD_AFC_PWR_SYNC_UPDATED ||
		    sync_result == HOSTAPD_AFC_PWR_SYNC_NOOP ||
		    sync_result == HOSTAPD_AFC_PWR_SYNC_DEFERRED) {
			/*
			 * UPDATED  - CSA to non-SP mode started
			 * NOOP     - already in a valid non-SP mode
			 * DEFERRED - transient (CSA/pending switch); retry armed
			 */
			return;
		}

		/* INVALID_CURRENT or ERROR: no valid fallback */
		wpa_printf(MSG_ERROR,
			   "AFC payload reset: fallback unavailable, disconnect backhaul STA iface=%s result=%d",
			   iface->phy, sync_result);
		if (hostapd_disconnect_backhaul_sta(iface))
			wpa_printf(MSG_ERROR,
				   "AFC payload reset: backhaul disconnect failed iface=%s",
				   iface->phy);
		return;
	}

	if (he_reg_is_sp(iface->conf->he_6ghz_reg_pwr_type)) {
		enum hostapd_afc_power_sync_result sync_result;

		if (iface->conf->enable_best_power_mode) {
			/*
			 * BPM enabled: run channel-selection recovery directly.
			 * Do not do same-channel fallback first; let the BPM
			 * path pick the best available channel and mode.
			 */
			wpa_printf(MSG_INFO,
				   "AFC payload reset: BPM channel recovery iface=%s",
				   iface->phy);
			iface->is_afc_channel_change_pending = true;
			/* Wait for NL80211_WIPHY_REG_CHANGE to get updated channel list */
			eloop_register_timeout(5, 0,
					       afc_channel_change_timeout,
					       iface, NULL);
			return;
		}

		/* BPM disabled: try same-channel non-SP fallback */
		wpa_printf(MSG_INFO,
			   "AFC payload reset: BPM disabled, same-channel fallback iface=%s",
			   iface->phy);
		sync_result = hostapd_force_afc_non_sp_power_mode(iface);
		if (sync_result == HOSTAPD_AFC_PWR_SYNC_UPDATED ||
		    sync_result == HOSTAPD_AFC_PWR_SYNC_NOOP)
			return;

		/*
		 * DEFERRED, INVALID_CURRENT, or ERROR: no retry path exists
		 * for root AP + BPM disabled after CSA completes.
		 * Clear the pending flag before setting NO_IR to keep the
		 * repeater-pending invariant clean.
		 */
		iface->is_afc_repeater_power_sync_pending = false;
		wpa_printf(MSG_ERROR,
			   "AFC payload reset: no valid non-SP fallback iface=%s result=%d",
			   iface->phy, sync_result);
		hostapd_set_no_ir_state(iface);
	}
}

#ifdef CONFIG_OWE
static int hostapd_notif_update_dh_ie(struct hostapd_data *hapd,
				      const u8 *peer, const u8 *ie,
				      size_t ie_len, const u8 *link_addr)
{
	u16 status;
	struct sta_info *sta;
	struct ieee802_11_elems elems;

	if (!hapd || !hapd->wpa_auth) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid hapd context");
		return -1;
	}
	if (!peer) {
		wpa_printf(MSG_DEBUG, "OWE: Peer unknown");
		return -1;
	}
	if (!(hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE)) {
		wpa_printf(MSG_DEBUG, "OWE: No OWE AKM configured");
		status = WLAN_STATUS_AKMP_NOT_VALID;
		goto err;
	}
	if (ieee802_11_parse_elems(ie, ie_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "OWE: Failed to parse OWE IE for "
			   MACSTR, MAC2STR(peer));
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto err;
	}
	status = owe_validate_request(hapd, peer, elems.rsn_ie,
				      elems.rsn_ie_len,
				      elems.owe_dh, elems.owe_dh_len);
	if (status != WLAN_STATUS_SUCCESS)
		goto err;

	sta = ap_get_sta(hapd, peer);
	if (sta) {
		ap_sta_no_session_timeout(hapd, sta);
		accounting_sta_stop(hapd, sta);

		/*
		 * Make sure that the previously registered inactivity timer
		 * will not remove the STA immediately.
		 */
		sta->timeout_next = STA_NULLFUNC;
	} else {
		sta = ap_sta_add(hapd, peer);
		if (!sta) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto err;
		}
	}
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);

#ifdef CONFIG_IEEE80211BE
	if (link_addr) {
		struct mld_info *info = &sta->mld_info;
		u8 link_id = hapd->mld_link_id;

		ap_sta_set_mld(sta, true);
		sta->mld_assoc_link_id = link_id;
		os_memcpy(info->common_info.mld_addr, peer, ETH_ALEN);
		info->links[link_id].valid = true;
		os_memcpy(info->links[link_id].local_addr, hapd->own_addr,
			  ETH_ALEN);
		os_memcpy(info->links[link_id].peer_addr, link_addr, ETH_ALEN);
	}
#endif /* CONFIG_IEEE80211BE */

	status = owe_process_rsn_ie(hapd, sta, elems.rsn_ie,
				    elems.rsn_ie_len, elems.owe_dh,
				    elems.owe_dh_len, link_addr);
	if (status != WLAN_STATUS_SUCCESS)
		ap_free_sta(hapd, sta);

	return 0;
err:
	hostapd_drv_update_dh_ie(hapd, link_addr ? link_addr : peer, status,
				 NULL, 0);
	return 0;
}
#endif /* CONFIG_OWE */


#ifdef NEED_AP_MLME
static void hostapd_eapol_tx_status(struct hostapd_data *hapd, const u8 *dst,
				    const u8 *data, size_t len, int ack,
				    int link_id)
{
	struct sta_info *sta;

	hapd = switch_link_hapd(hapd, link_id);
	hapd = hostapd_find_by_sta(hapd->iface, dst, false, &sta);

	if (!sta) {
		wpa_printf(MSG_DEBUG, "Ignore TX status for Data frame to STA "
			   MACSTR " that is not currently associated",
			   MAC2STR(dst));
		return;
	}

	ieee802_1x_eapol_tx_status(hapd, sta, data, len, ack);
}
#endif /* NEED_AP_MLME */


#ifdef CONFIG_IEEE80211AX
static void hostapd_event_color_change(struct hostapd_data *hapd, bool success)
{
	struct hostapd_data *bss;
	size_t i;

	for (i = 0; i < hapd->iface->num_bss; i++) {
		bss = hapd->iface->bss[i];
		if (bss->cca_color == 0)
			continue;

		if (success)
			hapd->iface->conf->he_op.he_bss_color = bss->cca_color;

		bss->cca_in_progress = 0;
		if (ieee802_11_set_beacon(bss)) {
			wpa_printf(MSG_ERROR, "Failed to remove BCCA element");
			bss->cca_in_progress = 1;
		} else {
			hostapd_cleanup_cca_params(bss);
#ifdef CONFIG_IEEE80211BN
			/*
			 * Trigger MAPC Co-TDMA Agreement Update so that
			 * coordinating peers updated with new BSS color.
			 */
			if (success && bss->conf->mapc_conf)
				mapc_handle_negotiation_update(
					bss, MAPC_SCHEME_CO_TDMA);
#endif /* CONFIG_IEEE80211BN */
		}
	}
}
#endif  /* CONFIG_IEEE80211AX */


static void hostapd_iface_enable(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	int ret;

	ret = hostapd_get_hw_features(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to get hardware features (%d)",
			   ret);
		return;
	}

	ret = hostapd_select_hw_mode(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "Could not change 6GHZ power mode(%d)",
			   ret);
		hostapd_disable_iface(iface);
	}

	if (hostapd_set_current_hw_info(iface, iface->freq)) {
		wpa_printf(MSG_ERROR, "Failed to get operating hw mac id");
		hostapd_disable_iface(iface);
	}

	wpa_msg(hapd->msg_ctx, MSG_INFO, INTERFACE_ENABLED);
	if (hapd->disabled && hapd->started) {
		hapd->disabled = 0;
		/*
		 * Try to re-enable interface if the driver stopped it
		 * when the interface got disabled.
		 */
		hostapd_reconfig_wpa(hapd);
		if (hapd->wpa_auth)
			wpa_auth_reconfig_group_keys(hapd->wpa_auth);
		else
			hostapd_reconfig_encryption(hapd);
		hapd->reenable_beacon = 1;

#ifdef CONFIG_QCN_EXTN
		hostapd_bootup_cac_start_extn(iface);
#endif
		ieee802_11_set_beacon(hapd);
#ifdef NEED_AP_MLME
	} else if (hapd->disabled && hostapd_is_cac_required(iface)) {
		wpa_printf(MSG_DEBUG, "DFS: restarting pending CAC");
		hostapd_handle_dfs(iface);
#endif /* NEED_AP_MLME */
	}
}


static void hostapd_iface_disable(struct hostapd_data *hapd)
{
	hostapd_free_stas(hapd);
	wpa_msg(hapd->msg_ctx, MSG_INFO, INTERFACE_DISABLED);
	hapd->disabled = 1;
}


#ifdef CONFIG_IEEE80211BE
static int hostapd_sm_link_reconfigure(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       void *ctx)
{
	struct hostapd_data *phapd = (struct hostapd_data *)ctx;

	if (!phapd || !sta || !sta->mld_info.mld_sta)
		/* No action needed for legacy station */
		return 0;

	if (sta->mld_assoc_link_id == hapd->mld_link_id) {
		eloop_cancel_timeout(wpa_send_eapol_timeout, hapd->wpa_auth, sta->wpa_sm);
		set_for_each_partner_link_sta(hapd, sta, phapd->wpa_auth,
					      wpa_auth_reconfig_wpa_auth_sm);

		set_link_id_for_each_partner_link_sta(hapd, sta, phapd->mld_link_id);
		sta->mld_assoc_link_id = phapd->mld_link_id;
	}

	set_valid_for_each_partner_link_sta(hapd, sta, false);

	return 0;
}


static void hostapd_update_link_removal_field(struct hostapd_data *hapd,
					      struct link_removal_event *ev,
					      enum wpa_event_type event)
{
	struct hostapd_data *phapd = NULL, *tx_hapd = NULL, *thapd = NULL;
	struct hostapd_iface *iface, **tmp;
	unsigned int i;
	struct hapd_interfaces *interfaces;
#ifdef CONFIG_WNM_AP
	u8 bss_term_dur[12];
	u8 req_mode;
	u32 total_us;
#endif
	bool is_6g = false;

	if (event == EVENT_LINK_REMOVAL_STARTED) {
#ifdef CONFIG_WNM_AP
		req_mode = WNM_BSS_TM_REQ_DISASSOC_IMMINENT |
			   WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED |
			   WNM_BSS_TM_REQ_LINK_REMOVAL_IMMINENT;

		bss_term_dur[0] = 4; /* Subelement ID */
		bss_term_dur[1] = 10; /* Length */
		/* TSF timer when corresponding BSS will be removed
		 * link_removal_count * beacon_interval will give total number of
		 * beacons ML reconfiguration element will be present after which the
		 * BSS will be removed
		 * Adding this with the TSF value of first beacon with ML reconfiguration
		 * element is sent will be equal/greater than the last beacon with ML
		 * reconfiguration element will be sent.
		 */
		total_us = host_to_le16(ev->link_removal_count) *
			   TU_TO_USEC(hapd->iconf->beacon_int);
		bss_term_dur[2] = ev->tsf + total_us;
		os_memset(&bss_term_dur[3], 0, 7); /* Optional */

		wnm_send_bss_tm_req(hapd, NULL, req_mode, ev->link_removal_count,
				    0x01, &bss_term_dur[0], 0x01, NULL, NULL, 0,
				    NULL, 0);
#endif
		hapd->eht_mld_link_removal_count = ev->link_removal_count;
		hapd->eht_mld_link_removal_inprogress = true;
	} else if (event == EVENT_LINK_REMOVAL_COMPLETED) {
		hapd->eht_mld_link_removal_inprogress = false;
		hapd->eht_mld_link_removal_count = 0;

		iface = hapd->iface;
		interfaces = iface->interfaces;
		/* check this to verify if the removed BSS is 6 GHz,
		 * otherwise updating only the partner beacon,
		 * irrespective of the band is suffice.
		 */
		is_6g = is_6ghz_op_class(iface->conf->op_class);

		/* Only disable the link instead of removing */
		if (hapd->removal_type == HAPD_LINK_DISABLE) {
			hostapd_free_link_stas(hapd);
			hostapd_disable_bss(hapd, 0, AP_EVENT_DISABLED);
			return;
		}

		/* Save one of the partner bss to update the beacon */
		if (hapd->mld->num_links > 1) {
			for_each_mld_link(phapd, hapd) {
				if (phapd != hapd)
					break;
			}
		}

		if (iface->num_bss == 1) {

			ap_for_each_sta(hapd, hostapd_sm_link_reconfigure, phapd);
			hostapd_free_link_stas(hapd);

			for (i = 0; i < interfaces->count; i++) {
				if (interfaces->iface[i] == iface) {
					hostapd_interface_deinit_free(iface);
					iface = NULL;
					os_remove_in_array(interfaces->iface, interfaces->count, sizeof(struct hostapd_iface *), i);
					interfaces->count--;
					tmp = os_realloc_array(interfaces->iface,
							       interfaces->count,
							       sizeof(struct hostapd_iface *));
					if (!tmp)
						return;
					interfaces->iface = tmp;
					break;
				}
			}
		} else {
			for (i = 0; i < iface->conf->num_bss; i++) {
				if (iface->bss[i] == hapd)
					break;
			}

			/* Shouldn't happen */
			if (i >= iface->conf->num_bss) {
				wpa_printf(MSG_ERROR, "Wrong hapd is provided\n");
				return;
			}

			ap_for_each_sta(hapd, hostapd_sm_link_reconfigure, phapd);
			hostapd_free_link_stas(hapd);

			/* Store tx_hapd to update MBSSID beacon as hapd will be
			 * freed by hostapd_remove_bss() */
			tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
			if (tx_hapd == hapd)
				tx_hapd = NULL;

			hostapd_remove_bss(iface, i);

			if (tx_hapd)
				ieee802_11_update_beacon_mbssid(tx_hapd);
		}

		/* For 6G, OOB advertisement also needs to be refreshed in all
		 * enabled lower bands, hence invoke other iface beacon refresh
		 * to update all the entries properly
		 */
		if (is_6g) {
			if (!iface && interfaces->count > 0)
				hostapd_refresh_all_iface_beacons(interfaces->iface[0]);
			else
				hostapd_refresh_other_iface_beacons(iface);
		} else if (phapd && phapd != hapd) {
			/* Refresh all partner beacons.
			 * Skip when phapd == hapd: that means hapd was the only
			 * remaining MLD link and hostapd_remove_bss() already freed
			 * hapd, making phapd a dangling pointer.
			 */
			for_each_mld_link(thapd, phapd) {
				ieee802_11_set_beacon_per_bss_only(thapd);
				hostapd_gen_per_sta_profiles(thapd);
			}
		}
	}
}
#endif /* CONFIG_IEEE80211BE */


static void hostapd_event_update_cu_param(struct hostapd_data *hapd,
					  struct cu_event *cu_event)
{
	/* Update critical update parameters */
	hapd->rx_cu_param.critical_flag = cu_event->critical_flag;
	hapd->rx_cu_param.bpcc = cu_event->bpcc;
	hapd->rx_cu_param.switch_count = cu_event->switch_count;
}

#ifdef CONFIG_IEEE80211BN
static void hostapd_event_update_ecu_param(struct hostapd_data *hapd,
					   struct cu_event *cu_event)
{
	/* Update Enhanced Critical Update (ECU) parameters.
	 * The enhanced_bpcc is a 4-bit value (modulo 16) received from the
	 * driver via EVENT_RX_CRITICAL_UPDATE when the kernel reports an
	 * updated Enhanced BSS Parameter Change Count for this link.
	 */
	hapd->rx_ecu_param.ebpcc = cu_event->enhanced_bpcc & 0x0F;
	hapd->rx_ecu_param.critical_update = cu_event->enhanced_critical_update;
	hapd->uhr_ecu.countdown_timer = cu_event->ecu_countdown;
}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_IEEE80211BE

static void hostapd_mld_iface_enable(struct hostapd_data *hapd)
{
	struct hostapd_data *first_link, *link_bss;

	first_link = hostapd_mld_is_first_bss(hapd) ? hapd :
		hostapd_mld_get_first_bss(hapd);

	/* Links have been removed. Re-add all links and enable them, but
	 * enable the first link BSS before doing that. */
	if (hostapd_drv_link_add(first_link, first_link->mld_link_id,
				 first_link->own_addr)) {
		wpa_printf(MSG_ERROR, "MLD: Failed to re-add link %d in MLD %s",
			   first_link->mld_link_id, first_link->conf->iface);
		return;
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_notify_link_repurpose(first_link, "hostapd_mld_iface_enable");
#endif /* CONFIG_QCN_EXTN */

	hostapd_iface_enable(first_link);

	/* Add other affiliated links */
#ifdef CONFIG_QCN_EXTN
	for_each_mld_link_include_repurposed(link_bss, first_link) {
#else
	for_each_mld_link(link_bss, first_link) {
#endif /* CONFIG_QCN_EXTN */
		if (link_bss == first_link)
			continue;

		if (hostapd_drv_link_add(link_bss, link_bss->mld_link_id,
					 link_bss->own_addr)) {
			wpa_printf(MSG_ERROR,
				   "MLD: Failed to re-add link %d in MLD %s",
				   link_bss->mld_link_id,
				   link_bss->conf->iface);
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		hostapd_notify_link_repurpose(link_bss,
					      "hostapd_mld_iface_enable");
#endif /* CONFIG_QCN_EXTN */
		hostapd_iface_enable(link_bss);
	}
}


static void hostapd_mld_iface_disable(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;

#ifdef CONFIG_QCN_EXTN
	for_each_mld_link_include_repurposed(link_bss, hapd)
#else
	for_each_mld_link(link_bss, hapd)
#endif /* CONFIG_QCN_EXTN */
		hostapd_iface_disable(link_bss);
}

static int hostapd_update_sta_negotiated_info(struct hostapd_data *hapd,
					      struct sta_info *sta,
					      void *ctx)
{
	struct ttlm_prev_negotiated_info *neg_info;
	int dir;

	/* no operation needed on legacy sta, continue iterator on remaining clients */
	if (!ap_sta_is_mld(hapd, sta))
		return 0;

	neg_info = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;

	neg_info->dialog_token = 0;

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++)
		neg_info->ttlm_info[dir].direction = TTLM_DIRECTION_INVALID;

	memcpy(&neg_info->ttlm_info[TTLM_DIRECTION_BIDI],
	       &hapd->mld->ttlm_ctx.established_ttlm.ttlm,
	       sizeof(struct ttlm_info));

	/* check further on remaining clients if present without breaking the iterator */
	return 0;
}

static void hostapd_update_sta_negotiated_info_across_partners(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;

	for_each_mld_link(link_bss, hapd)
		ap_for_each_sta(link_bss, hostapd_update_sta_negotiated_info, NULL);
}

static void hostapd_event_update_ttlm_status(struct hostapd_data *hapd,
					     struct ttlm_update_event *ttlm_update_event)
{
	struct ttlm_context *ttlm_ctx = &hapd->mld->ttlm_ctx;
	struct hostapd_data *link_bss;

	wpa_printf(MSG_INFO, "TTLM: link id = %d status = %d",
		   hapd->mld_link_id, ttlm_update_event->status);

	switch (ttlm_update_event->status) {
	case TTLM_MAP_SWITCH_TIMER_TSF:
		if (ttlm_ctx->upcoming_ttlm.ttlm.mapping_switch_time_present)
			hapd->mapping_switch_time =
				ttlm_update_event->mapping_switch_tsf;
		wpa_printf(MSG_INFO, "TTLM: Updated mapping switch time to %d",
			   ttlm_update_event->mapping_switch_tsf);
		break;
	case TTLM_MAP_SWITCH_TIMER_EXPIRED:
		for_each_mld_link(link_bss, hapd)
			link_bss->mapping_switch_time = 0;

		hostapd_ttlm_handle_mapping_switch_time_expiry(ttlm_ctx,
							       hapd->mld_link_id);
		/* TO-DO: Send event to user-space to notify link update.*/

		/* Go over all MLO peers on this MLD and clear the
		 * peer-to-peer level mapping.
		 */
		hostapd_update_sta_negotiated_info_across_partners(hapd);
		break;
	case TTLM_EXPECTED_DUR_EXPIRED:
		hostapd_ttlm_handle_expected_duration_expiry(ttlm_ctx,
							     hapd->mld_link_id);
		/* TO-DO: Move P2P negotiated mapping to default */

		/* Go over all MLO peers on this MLD and reset the
		 * peer-to-peer level mapping to default mapping.
		 */
		hostapd_update_sta_negotiated_info_across_partners(hapd);
		break;
	default:
		wpa_printf(MSG_ERROR, "TTLM: Invalid status");

	}
}
#endif /* CONFIG_IEEE80211BE */

static void hostapd_event_update_expec_dur(struct hostapd_data *hapd,
					  struct ttlm_expec_dur_event *ttlm_expec_dur_event)
{
	struct ttlm_context *ttlm_ctx = &hapd->mld->ttlm_ctx;

	if (ttlm_ctx->established_ttlm.ttlm.expected_duration_present) {
#ifdef CONFIG_QCN_EXTN
		if (!ttlm_ctx->established_t2lm_ed_modified_in_case_of_cac)
#endif /* CONFIG_QCN_EXTN */
			ttlm_ctx->established_ttlm.ttlm.expected_duration =
				ttlm_expec_dur_event->expec_dur;
	} else if (ttlm_ctx->upcoming_ttlm.ttlm.expected_duration_present) {
		ttlm_ctx->upcoming_ttlm.ttlm.expected_duration =
			ttlm_expec_dur_event->expec_dur;
	}
}


#ifdef CONFIG_IEEE80211BN
static void hostapd_update_ap_powersave(struct hostapd_data *hapd,
					struct ap_powersave_event *ap_ps_event)
{
	if (ap_ps_event->dps_assist_updated) {
		struct hostapd_hw_modes *mode = hapd->iface->current_mode;
		struct uhr_capabilities *uhr_cap;

		if (!mode) {
			wpa_printf(MSG_ERROR,
				   "Failed to %s DPS Assist feature due to feature not supported by HW",
				   ap_ps_event->dps_assist ? "enable" : "disable");
			return;
		}

		uhr_cap = &mode->uhr_capab[IEEE80211_MODE_AP];
		if (!uhr_cap->uhr_supported ||
		    !(uhr_cap->mac_cap[0] & UHR_MACCAP_DPS_ASSIST)) {
			wpa_printf(MSG_ERROR,
				   "Failed to %s DPS Assist feature due to feature not supported by HW",
				   ap_ps_event->dps_assist ? "enable" : "disable");
			return;
		}

		hapd->conf->dps_assist = ap_ps_event->dps_assist;
		/* Update beacon with updated DPS Assist Support Bit */
		if (ieee802_11_update_beacons(hapd->iface))
			wpa_printf(MSG_ERROR,
				   "Failed to update beacons with DPS Assist Support Bit");
	}
}

/**
 * hostapd_handle_critical_update_notify - Process EVENT_CRITICAL_UPDATE_NOTIFY
 * @hapd: hostapd BSS data (already resolved to the correct link)
 * @ev:   Event data carrying link_id and cu_state
 *
 * Maps the kernel-reported nl80211_cu_state onto the hostapd-internal
 * uhr_ecu_state and triggers a beacon update so that the UHR Params Update
 * element is included/excluded as required by IEEE 802.11bn 37.30.2.2:
 *
 * STARTED		-> UHR_ECU_ADVANCE_NOTIFY      (begin adv-notification;
 * 			   element mandatory in Beacon/ProbeResp/Assoc/LinkReconf)
 * ADV_NOTIFICATION_END -> UHR_ECU_POST_ADVANCE_NOTIFY (update has taken effect;
 * 			    element optional in Beacon/ProbeResp only)
 * POST_NOTIFICATION_END -> UHR_ECU_IDLE                (post window elapsed;
 *  			   element must not be included in any frame)
 * ECU_END		  -> UHR_ECU_IDLE                (session complete)
 * ECU_ABORT		-> UHR_ECU_IDLE 		(session aborted)
 *
 */
static void
hostapd_handle_critical_update_notify(struct hostapd_data *hapd,
				      const struct cu_notify_event *ev)
{
	enum uhr_ecu_state new_state;

	if (!hostapd_is_uhr_enabled(hapd)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: CRITICAL_UPDATE_NOTIFY ignored - UHR not enabled on %s",
			   hapd->conf->iface);
		return;
	}

	switch (ev->cu_state) {
	case NL80211_CU_STATE_STARTED:
		new_state = UHR_ECU_ADVANCE_NOTIFY;
		wpa_printf(MSG_DEBUG,
			   "nl80211: ECU STARTED on %s link %u - entering adv-notification phase",
			   hapd->conf->iface, ev->link_id);
		break;

	case NL80211_CU_STATE_ADV_NOTIFICATION_END:
		/*
		 * Advance-notification window has elapsed.  The element stays
		 * in beacons (state remains IN_PROGRESS) but the beacon must
		 * be refreshed so the countdown timer field is updated.
		 */
		new_state = UHR_ECU_POST_ADVANCE_NOTIFY;
		wpa_printf(MSG_DEBUG,
			   "nl80211: ECU ADV_NOTIFICATION_END on %s link %u",
			   hapd->conf->iface, ev->link_id);

		/*
		 * The update has taken effect.  Commit the new NPCA parameters
		 * into iconf so that hostapd_eid_uhr_operation() reflects the
		 * updated values in Probe Response and (Re)Association Response
		 * frames from this point on.
		 */
		hostapd_update_ecu_params(hapd);

		break;

	case NL80211_CU_STATE_POST_NOTIFICATION_END:
		new_state = UHR_ECU_UPDATE_IND_IN_TIM;
		wpa_printf(MSG_DEBUG,
			   "nl80211: ECU POST_NOTIFICATION_END on %s link %u",
			   hapd->conf->iface, ev->link_id);
		break;

	case NL80211_CU_STATE_ECU_END:
		new_state = UHR_ECU_IDLE;
		wpa_printf(MSG_DEBUG,
			   "nl80211: ECU_END on %s link %u - session complete",
			   hapd->conf->iface, ev->link_id);
		break;
	case NL80211_CU_STATE_ABORT:
		new_state = UHR_ECU_IDLE;
		wpa_printf(MSG_DEBUG,
			   "nl80211: ECU ABORT on %s link %u - session aborted",
			   hapd->conf->iface, ev->link_id);
		hostapd_reset_uhr_cu_params(hapd);
		break;
	default:
		wpa_printf(MSG_WARNING,
			   "nl80211: CRITICAL_UPDATE_NOTIFY unknown cu_state=%u on %s link %u",
			   ev->cu_state, hapd->conf->iface, ev->link_id);
		return;
	}

	hapd->uhr_ecu.state = new_state;
	/*
	 * Enhanced All Updates Included (B7 of the ECUI byte) must be 1
	 * while the UHR Parameters Update element is present in the frame
	 * (advance- and post-notification phases), and 0 once the element
	 * is no longer included (UHR_ECU_UPDATE_IND_IN_TIM / IDLE).
	 */
	hapd->rx_ecu_param.all_updates_included =
		(new_state == UHR_ECU_ADVANCE_NOTIFY ||
		 new_state == UHR_ECU_POST_ADVANCE_NOTIFY);

	/* TODO: Refresh beacon so the UHR Params Update element is added, updated,
	 * or removed according to the new ECU state. */
}
#endif /* CONFIG_IEEE80211BN */

static void hostapd_restart_agile_cac_all_ifaces(struct hostapd_data *hapd)
{
	unsigned int i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		struct hostapd_iface *h = hapd->iface->interfaces->iface[i];

		if (h->state == HAPD_IFACE_ENABLED &&
		    dfs_use_radar_background(h) &&
		    !h->radar_background.cac_started)
			hostapd_restart_agile_cac_after_ch_switch(h);
	}
}

void hostapd_rx_free_smd_ctx(struct sta_smd_ctx_info *smd_ctx)
{
	int i;

	if (!smd_ctx)
		return;

	for (i = 0; i < SMD_NUM_SCSID; i++)
		os_free(smd_ctx->qos.scs_descriptors[i]);
	os_free(smd_ctx->qos.mscs_descriptor);
	os_free(smd_ctx);
}

static bool hostapd_event_tpc_eirp_notify(struct hostapd_data *hapd,
					  s32 tpc_dbm, int link_id)
{
	size_t i;
	bool changed = false;

	if (!hapd->iface)
		return false;

	for (i = 0; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *bss = hapd->iface->bss[i];

		if (!bss || !bss->started)
			continue;

#ifdef CONFIG_IEEE80211BE
		if (link_id >= 0 && bss->conf->mld_ap &&
		    bss->mld_link_id != link_id)
			continue;
#endif /* CONFIG_IEEE80211BE */

		if (!bss->tpc_eirp_valid || bss->tpc_eirp_dbm != tpc_dbm) {
			bss->tpc_eirp_dbm = tpc_dbm;
			bss->tpc_eirp_valid = true;
			changed = true;
		}
	}

	return changed;
}

void hostapd_wpa_event(void *ctx, enum wpa_event_type event,
		       union wpa_event_data *data)
{
	struct hostapd_data *hapd = ctx, *phapd;
	struct sta_info *sta;
	struct hostapd_data *link_hapd;
	bool found = false;
#ifndef CONFIG_NO_STDOUT_DEBUG
	int level = MSG_DEBUG;

	if (event == EVENT_RX_MGMT && data->rx_mgmt.frame &&
	    data->rx_mgmt.frame_len >= 24) {
		const struct ieee80211_hdr *hdr;
		u16 fc;

		hdr = (const struct ieee80211_hdr *) data->rx_mgmt.frame;
		fc = le_to_host16(hdr->frame_control);
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
			level = MSG_EXCESSIVE;
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PROBE_REQ)
			level = MSG_EXCESSIVE;
	}

	wpa_dbg(hapd->msg_ctx, level, "Event %s (%d) received",
		event_to_string(event), event);
#endif /* CONFIG_NO_STDOUT_DEBUG */

	switch (event) {
	case EVENT_MICHAEL_MIC_FAILURE:
		michael_mic_failure(hapd, data->michael_mic_failure.src, 1);
		break;
	case EVENT_SCAN_RESULTS:
#ifdef NEED_AP_MLME
		if (data)
			hapd = switch_link_scan(hapd,
						data->scan_info.scan_cookie,
						data);
#endif /* NEED_AP_MLME */
		/* Latch whether the last scan was aborted to allow ACS logic to react */
		if (data)
			hapd->iface->last_scan_aborted = data->scan_info.aborted;

		if (hapd->iface->scan_cb)
			hapd->iface->scan_cb(hapd->iface);
#ifdef CONFIG_IEEE80211BE
		if (!hapd->iface->scan_cb && hapd->conf->mld_ap) {
			/* Other links may be waiting for HT scan result */
			unsigned int i;

			for (i = 0; i < hapd->iface->interfaces->count; i++) {
				struct hostapd_iface *h =
					hapd->iface->interfaces->iface[i];
				struct hostapd_data *h_hapd = h->bss[0];

				if (hostapd_is_ml_partner(hapd, h_hapd) &&
				    h_hapd->iface->scan_cb &&
				    h_hapd->iface->state != HAPD_IFACE_ACS)
					h_hapd->iface->scan_cb(h_hapd->iface);
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_restart_agile_cac_all_ifaces(hapd);
		break;
	case EVENT_WPS_BUTTON_PUSHED:
		hostapd_wps_button_pushed(hapd, NULL);
		break;
#ifdef RDK_ONEWIFI
	case EVENT_WPS_CANCEL:
		hostapd_wps_cancel(hapd);
		break;
#endif /* RDK_ONEWIFI */
#ifdef NEED_AP_MLME
	case EVENT_TX_STATUS:
		switch (data->tx_status.type) {
		case WLAN_FC_TYPE_MGMT:
			hostapd_mgmt_tx_cb(hapd, data->tx_status.data,
					   data->tx_status.data_len,
					   data->tx_status.stype,
					   data->tx_status.ack,
					   data->tx_status.link_id);
			break;
		case WLAN_FC_TYPE_DATA:
			hostapd_tx_status(hapd, data->tx_status.dst,
					  data->tx_status.data,
					  data->tx_status.data_len,
					  data->tx_status.ack);
			break;
		}
		break;
	case EVENT_EAPOL_TX_STATUS:
		hostapd_eapol_tx_status(hapd, data->eapol_tx_status.dst,
					data->eapol_tx_status.data,
					data->eapol_tx_status.data_len,
					data->eapol_tx_status.ack,
					data->eapol_tx_status.link_id);
		break;
	case EVENT_DRIVER_CLIENT_POLL_OK:
		hostapd_client_poll_ok(hapd, data->client_poll.addr);
		break;
	case EVENT_RX_FROM_UNKNOWN:
		hapd = switch_link_hapd(hapd, data->rx_from_unknown.link_id);
		hostapd_rx_from_unknown_sta(hapd, data->rx_from_unknown.bssid,
					    data->rx_from_unknown.addr,
					    data->rx_from_unknown.wds);
		break;
#endif /* NEED_AP_MLME */
	case EVENT_RX_MGMT:
		if (data->rx_mgmt.frame) {
#ifdef NEED_AP_MLME
			hostapd_mgmt_rx(hapd, &data->rx_mgmt);
#else /* NEED_AP_MLME */
			hostapd_action_rx(hapd, &data->rx_mgmt);
#endif /* NEED_AP_MLME */
		}
		hostapd_rx_free_smd_ctx(data->rx_mgmt.smd_ctx);
		break;
	case EVENT_RX_PROBE_REQ:
		if (data->rx_probe_req.sa == NULL ||
		    data->rx_probe_req.ie == NULL)
			break;
		hostapd_probe_req_rx(hapd, data->rx_probe_req.sa,
				     data->rx_probe_req.da,
				     data->rx_probe_req.bssid,
				     data->rx_probe_req.ie,
				     data->rx_probe_req.ie_len,
				     data->rx_probe_req.ssi_signal);
		break;
	case EVENT_NEW_STA:
		hostapd_event_new_sta(hapd, data->new_sta.addr, data->new_sta.flags);
		break;
	case EVENT_EAPOL_RX:
		hostapd_event_eapol_rx(hapd, data->eapol_rx.src,
				       data->eapol_rx.data,
				       data->eapol_rx.data_len,
				       data->eapol_rx.encrypted,
				       data->eapol_rx.link_id);
		break;
	case EVENT_ASSOC:
		if (!data)
			return;
#ifdef CONFIG_IEEE80211BE
		if (data->assoc_info.assoc_link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->assoc_info.assoc_link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for EVENT_ASSOC");
				return;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_notif_assoc(hapd, data->assoc_info.addr,
				    data->assoc_info.req_ies,
				    data->assoc_info.req_ies_len,
				    data->assoc_info.resp_ies,
				    data->assoc_info.resp_ies_len,
				    data->assoc_info.link_addr,
				    data->assoc_info.reassoc);
		break;
	case EVENT_PORT_AUTHORIZED:
		/* Port authorized event for an associated STA */
		sta = ap_get_sta(hapd, data->port_authorized.sta_addr);
		if (sta)
			ap_sta_set_authorized(hapd, sta, 1);
		else
			wpa_printf(MSG_DEBUG,
				   "No STA info matching port authorized event found");
		break;
#ifdef CONFIG_OWE
	case EVENT_UPDATE_DH:
		if (!data)
			return;
#ifdef CONFIG_IEEE80211BE
		if (data->update_dh.assoc_link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->update_dh.assoc_link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for EVENT_UPDATE_DH assoc_link_id=%d",
					   data->update_dh.assoc_link_id);
				return;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_notif_update_dh_ie(hapd, data->update_dh.peer,
					   data->update_dh.ie,
					   data->update_dh.ie_len,
					   data->update_dh.link_addr);
		break;
#endif /* CONFIG_OWE */
	case EVENT_DISASSOC:
		if (data)
			hostapd_notif_disassoc(hapd, data->disassoc_info.addr);
		break;
	case EVENT_DEAUTH:
		if (data)
			hostapd_notif_disassoc(hapd, data->deauth_info.addr);
		break;
	case EVENT_STATION_LOW_ACK:
		if (!data)
			break;

		switch (data->low_ack.num_packets) {
		case HOSTAPD_DEAUTH_ALL:
			hostapd_event_sta_rssi_low(hapd, data->low_ack.addr);
			break;
		default:
			/* Generic low ACK (actual packet loss)
			 * or STA kickout event with num_packets = 10
			 */
			hostapd_event_sta_low_ack(hapd, data->low_ack.addr,
						  data->low_ack.num_packets);
			break;
		}
		break;
	case EVENT_AUTH:
		hostapd_notif_auth(hapd, &data->auth);
		break;
	case EVENT_CH_SWITCH_STARTED:
	case EVENT_CH_SWITCH:
		if (!data)
			break;
#ifdef CONFIG_IEEE80211BE
		if (data->ch_switch.link_id != -1) {
			hapd = hostapd_mld_get_link_bss(
				hapd, data->ch_switch.link_id);
			if (!hapd) {
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link (ID %d) BSS for EVENT_CH_SWITCH/EVENT_CH_SWITCH_STARTED",
					   data->ch_switch.link_id);
				break;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_event_ch_switch(hapd, data->ch_switch.freq,
					data->ch_switch.ht_enabled,
					data->ch_switch.ch_offset,
					data->ch_switch.ch_width,
					data->ch_switch.cf1,
					data->ch_switch.cf2,
					data->ch_switch.punct_bitmap,
					data->ch_switch.power_mode_6ghz,
					data->ch_switch.ch_width_device,
					data->ch_switch.cf_device,
					event == EVENT_CH_SWITCH);
		break;
	case EVENT_CONNECT_FAILED_REASON:
		if (!data)
			break;
		hostapd_event_connect_failed_reason(
			hapd, data->connect_failed_reason.addr,
			data->connect_failed_reason.code);
		break;
	case EVENT_SURVEY:
		hostapd_event_get_survey(hapd->iface, &data->survey_results);
		break;
#ifdef NEED_AP_MLME
	case EVENT_INTERFACE_UNAVAILABLE:
		hostapd_event_iface_unavailable(hapd);
		ieee802_11_update_beacon_mbssid(hapd);
		/* Update beacon to all the interfaces about the
		 * removal/disable of one of the BSS.
		 */
		hostapd_refresh_other_iface_beacons(hapd->iface);
		break;
	case EVENT_DFS_RADAR_DETECTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_radar_detected(hapd, &data->dfs_event);
		break;
#ifdef CONFIG_QCN_EXTN
	case EVENT_AWGN_DETECTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->awgn_event.link_id);
		hostapd_event_awgn_detected(hapd, &data->awgn_event);
		break;
	case EVENT_AFC_RECEIVED:
		hostapd_event_afc_received(hapd);
		break;
#endif /* CONFIG_QCN_EXTN */
	case EVENT_DFS_PRE_CAC_EXPIRED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_pre_cac_expired(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_CAC_FINISHED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_finished(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_CAC_ABORTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_aborted(hapd, &data->dfs_event);
		break;
	case EVENT_DFS_NOP_FINISHED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_nop_finished(hapd, &data->dfs_event);
		break;
	case EVENT_CHANNEL_LIST_CHANGED:
		/* channel list changed (regulatory?), update channel list */
		/* TODO: check this. hostapd_get_hw_features() initializes
		 * too much stuff. */
		/* hostapd_get_hw_features(hapd->iface); */
		hostapd_channel_list_updated(
			hapd->iface, data->channel_list_changed.initiator);
		break;
	case EVENT_DFS_CAC_STARTED:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->dfs_event.link_id);
		hostapd_event_dfs_cac_started(hapd, &data->dfs_event);
		break;
#endif /* NEED_AP_MLME */
	case EVENT_INTERFACE_ENABLED:
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			hostapd_mld_iface_enable(hapd);
			break;
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_iface_enable(hapd);
		break;
	case EVENT_INTERFACE_DISABLED:
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			hostapd_mld_iface_disable(hapd);
			break;
		}
#endif /* CONFIG_IEEE80211BE */
		hostapd_iface_disable(hapd);
		break;
#ifdef CONFIG_ACS
	case EVENT_ACS_CHANNEL_SELECTED:
		hostapd_acs_channel_selected(hapd,
					     &data->acs_selected_channels);
		break;
#endif /* CONFIG_ACS */
	case EVENT_STATION_OPMODE_CHANGED:
		hostapd_event_sta_opmode_changed(hapd, data->sta_opmode.addr,
						 data->sta_opmode.smps_mode,
						 data->sta_opmode.chan_width,
						 data->sta_opmode.rx_nss);
		break;
	case EVENT_WDS_STA_INTERFACE_STATUS:
		hostapd_event_wds_sta_interface_status(
			hapd, data->wds_sta_interface.istatus,
			data->wds_sta_interface.ifname,
			data->wds_sta_interface.sta_addr);
		break;
#ifdef CONFIG_IEEE80211AX
	case EVENT_BSS_COLOR_COLLISION:
		/* The BSS color is shared amongst all BBSs on a specific phy.
		 * Therefore we always start the color change on the primary
		 * BSS. */
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "BSS color collision on %s",
			   hapd->conf->iface);
		hostapd_switch_color(hapd->iface->bss[0],
				     data->bss_color_collision.bitmap);
		break;
	case EVENT_CCA_STARTED_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA started on %s",
			   hapd->conf->iface);
		break;
	case EVENT_CCA_ABORTED_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA aborted on %s",
			   hapd->conf->iface);
		hostapd_event_color_change(hapd, false);
		break;
	case EVENT_CCA_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->bss_color_collision.link_id);
		wpa_printf(MSG_DEBUG, "CCA finished on %s",
			   hapd->conf->iface);
		hostapd_event_color_change(hapd, true);
		break;
	case EVENT_6GHZ_POWER_MODE_NOTIFY:
		hapd = switch_link_hapd(hapd,
					data->ap_6ghz_pwr_mode_event.link_id);
		wpa_printf(MSG_DEBUG, "6GHz power mode changed on %s",
			   hapd->conf->iface);
		hostapd_event_6ghz_power_mode(hapd,
					      data->ap_6ghz_pwr_mode_event.pwr_mode);
		break;
#endif /* CONFIG_IEEE80211AX */
	case EVENT_TPC_EIRP_NOTIFY:
		if (!data)
			break;
		hapd = switch_link_hapd(hapd, data->tpc_eirp_event.link_id);
		if (!hapd || !hapd->iface)
			break;
		if (!hostapd_event_tpc_eirp_notify(hapd,
					data->tpc_eirp_event.tpc_dbm,
					data->tpc_eirp_event.link_id))
			break;
		if (ieee802_11_update_beacons(hapd->iface))
			break;
		break;
#ifdef CONFIG_IEEE80211BE
	case EVENT_TTLM_UPDATE:
		if (!data)
			break;
		hostapd_event_update_ttlm_status(hapd, &data->ttlm_update_event);
		break;
#endif /* CONFIG_IEEE80211BE */
	 case EVENT_UPDATE_MUEDCA_PARAMS:
		 hostapd_event_update_muedca_params(hapd, &data->update_muedca);
		 break;
	case EVENT_RX_CRITICAL_UPDATE:
		link_hapd = switch_link_hapd(hapd, data->cu_event.link_id);
		if (link_hapd) {
			hostapd_event_update_cu_param(link_hapd, &data->cu_event);
#ifdef CONFIG_IEEE80211BN
			hostapd_event_update_ecu_param(link_hapd, &data->cu_event);
#endif /* CONFIG_IEEE80211BN */
		}
		break;
	case EVENT_AFC_POWER_UPDATE_COMPLETE_NOTIFY:
		hostapd_event_afc_update_complete(hapd,
						  &data->afc_info);
		break;
	case EVENT_AFC_PAYLOAD_RESET:
		hostapd_event_afc_payload_reset(hapd, &data->afc_info);
		break;
#ifdef CONFIG_IEEE80211BE
	case EVENT_LINK_REMOVAL_STARTED:
		hostapd_update_link_removal_field(hapd,
						  &data->link_removal_event,
						  EVENT_LINK_REMOVAL_STARTED);
		break;
	case EVENT_LINK_REMOVAL_COMPLETED:
		hostapd_update_link_removal_field(hapd, 0,
						  EVENT_LINK_REMOVAL_COMPLETED);
		break;
	case EVENT_LINK_RECONFIG:
		link_hapd = switch_link_hapd(hapd, data->link_removal_event.link_id);
		if (link_hapd->eht_mld_link_removal_inprogress)
			link_hapd->eht_mld_link_removal_count = data->link_removal_event.link_removal_count;
		break;
	case EVENT_TTLM_EXPEC_DUR_UPDATE:
		link_hapd = switch_link_hapd(hapd, data->ttlm_expec_dur_event.link_id);
		if (link_hapd)
			hostapd_event_update_expec_dur(hapd, &data->ttlm_expec_dur_event);
		break;
#endif
	case EVENT_IFACE_RELOAD:
		if (!hapd->conf->mld_ap) {
			if (hostapd_reload_bss_only(hapd) < 0)
				wpa_printf(MSG_ERROR, "Reloading of BSS failed");
			break;
		}
		if (data->iface_reload.link_id == 0xFF) {
			/* If link id is invalid reload all bss of the mld interface */
#ifdef CONFIG_QCN_EXTN
			for_each_mld_link_include_repurposed(phapd, hapd) {
#else
			for_each_mld_link(phapd, hapd) {
#endif /* CONFIG_QCN_EXTN */
				if (hostapd_reload_bss_only(phapd) < 0) {
					wpa_printf(MSG_ERROR, "Reloading of BSS failed");
					continue;
				}
			}
		} else {
			for_each_mld_link(phapd, hapd) {
				if (phapd->mld_link_id == data->iface_reload.link_id) {
					found = true;
					break;
				}
			}
			if (!found)
				return;
			if (hostapd_reload_bss_only(phapd) < 0) {
				wpa_printf(MSG_ERROR, "Reloading of BSS failed");
				return;
			}
		}
		break;
	case EVENT_MSCS_FLOW_RECEIVED:
		hostapd_process_mscs_flow(hapd, data->tclas_flow_event.tclas,
				data->tclas_flow_event.addr, data->tclas_flow_event.tid);
		break;
#ifdef CONFIG_IEEE80211BN
	case EVENT_UPDATE_AP_POWERSAVE:
		hostapd_update_ap_powersave(hapd, &data->ap_powersave_event);
		break;
	case EVENT_CRITICAL_UPDATE_NOTIFY:
		link_hapd = switch_link_hapd(hapd, data->cu_notify_event.link_id);
		if (link_hapd)
			hostapd_handle_critical_update_notify(link_hapd,
							      &data->cu_notify_event);
		break;
	case EVENT_GET_SMD_CTX_DONE:
		if (data)
			uhr_handle_get_smd_ctx_done(hapd,
						    data->get_smd_ctx_done.sta_addr,
						    data->get_smd_ctx_done.ctx);
		break;
#endif /* CONFIG_IEEE80211BN */
	default:
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_wpa_event_extn(ctx, event, data))
			break;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_DEBUG, "Unknown event %d", event);
		break;
	}
}

static void hostapd_monitor_event_handler(struct hapd_interfaces *interfaces,
					  union wpa_event_data *data)
{
	size_t i;

	if (!data)
		return;

	/* Clear state when the monitor interface is removed */
	if (data->interface_status.ievent == EVENT_INTERFACE_REMOVED) {
		for (i = 0; i < interfaces->count; i++) {
			struct hostapd_iface *iface = interfaces->iface[i];

			if (!iface || os_strcmp(iface->monitor_iface,
					data->interface_status.ifname) != 0)
				continue;
			wpa_printf(MSG_INFO,
				   "Monitor iface: Interface %s removed for radio %s",
				   iface->monitor_iface, iface->phy);
			iface->monitor_iface[0] = '\0';
			iface->monitor_ifindex = 0;
			iface->monitor_iface_configured = false;
		}
	/* Deferred setup: retry when the monitor interface appears */
	} else if (data->interface_status.ievent == EVENT_INTERFACE_ADDED) {
		for (i = 0; i < interfaces->count; i++) {
			struct hostapd_iface *iface = interfaces->iface[i];

			if (!iface || iface->monitor_iface_configured ||
			    !iface->conf->monitor_iface_name[0] ||
			    os_strcmp(iface->conf->monitor_iface_name,
				      data->interface_status.ifname) != 0)
				continue;

			wpa_printf(MSG_INFO,
				   "Monitor iface: Interface %s appeared, retrying setup for radio %s",
				   data->interface_status.ifname, iface->phy);
			if (hostapd_setup_monitor_iface(iface) < 0)
				wpa_printf(MSG_WARNING,
					   "Monitor iface: Deferred setup failed for %s on radio %s",
					   iface->conf->monitor_iface_name,
					   iface->phy);
		}
	}
}

void hostapd_wpa_event_global(void *ctx, enum wpa_event_type event,
				 union wpa_event_data *data)
{
	struct hapd_interfaces *interfaces = ctx;
	struct hostapd_data *hapd;

	if (event != EVENT_INTERFACE_STATUS)
		return;

	hapd = hostapd_get_iface(interfaces, data->interface_status.ifname);
	if (hapd && hapd->driver && hapd->driver->get_ifindex &&
	    hapd->drv_priv) {
		unsigned int ifindex;

		ifindex = hapd->driver->get_ifindex(hapd->drv_priv);
		if (ifindex != data->interface_status.ifindex) {
			wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
				"interface status ifindex %d mismatch (%d)",
				ifindex, data->interface_status.ifindex);
			return;
		}
	}
	if (hapd)
		wpa_supplicant_event(hapd, event, data);
	hostapd_monitor_event_handler(interfaces, data);
}

#endif /* HOSTAPD */
