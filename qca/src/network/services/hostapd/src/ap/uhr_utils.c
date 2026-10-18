/*
 * hostapd / IEEE 802.11bn UHR
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "utils/includes.h"
#include "utils/includes.h"
#include "utils/common.h"
#include "utils/wpabuf.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "hostapd.h"
#include "sta_info.h"
#include "uhr_utils.h"
#include "ieee802_11.h"


static void uhr_st_iap_timeout_handler(void *eloop_ctx,
                                       void *timeout_ctx)
{
        struct sta_info *sta = eloop_ctx;
        struct smd_roam_ap_info *ap_info = timeout_ctx;

        ap_info->uhr_st_iap_timer_ongoing = false;

        wpa_printf(MSG_DEBUG,
                   "UHR: ST IAP timeout fired for AP " MACSTR,
                   MAC2STR(ap_info->ap_mld_addr));

        /*
         * If the IAP response never arrived during ST Execute, the target AP
         * may have already authorized the STA and cancelled its own prep timer.
         * Send a ROAM_CLEANUP so the target AP can undo that state before the
         * ap_info entry is freed.
         */
        if (ap_info->state == SMD_AP_STATE_ST_EXEC_IAP_PENDING &&
            ap_info->st_prep_hapd) {
                wpa_printf(MSG_DEBUG,
                           "UHR: ST IAP timeout in EXEC phase, sending ROAM CLEANUP to " MACSTR,
                           MAC2STR(ap_info->ap_mld_addr));
                uhr_iap_send_st_roam_cleanup(ap_info->st_prep_hapd,
                                             ap_info->ap_mld_addr,
                                             sta->addr);
        }

        uhr_remove_ap_from_list(sta, ap_info->ap_mld_addr);
}


int uhr_cur_start_iap_msg_timer(struct sta_info *sta,
                               const u8 *ap_mld_addr)
{
        struct smd_roam_ap_info *ap_info;

        if (!sta || !ap_mld_addr)
                return -1;

        ap_info = uhr_find_ap_in_list(sta, ap_mld_addr);
        if (!ap_info)
                return -1;

        if (ap_info->uhr_st_iap_timer_ongoing)
                return 0;

        ap_info->uhr_st_iap_timer_ongoing = true;

        if (eloop_register_timeout(0,
                                   UHR_ST_IAP_TIMEOUT_USEC,
                                   uhr_st_iap_timeout_handler,
                                   sta, ap_info) < 0) {
                ap_info->uhr_st_iap_timer_ongoing = false;
                return -1;
        }

        wpa_printf(MSG_DEBUG,
                   "UHR: Started ST IAP timeout (%u ms) for AP " MACSTR,
                   UHR_ST_IAP_TIMEOUT_USEC / 1000,
                   MAC2STR(ap_mld_addr));

        return 0;
}


void uhr_cancel_iap_timeout(struct sta_info *sta,
                            const u8 *ap_mld_addr)
{
        struct smd_roam_ap_info *ap_info;

        if (!sta || !ap_mld_addr)
                return;

        ap_info = uhr_find_ap_in_list(sta, ap_mld_addr);
        if (!ap_info || !ap_info->uhr_st_iap_timer_ongoing)
                return;

        eloop_cancel_timeout(uhr_st_iap_timeout_handler, sta, ap_info);

        ap_info->uhr_st_iap_timer_ongoing = false;
}

static void uhr_st_prep_timeout_handler(void *eloop_ctx, void *timeout_ctx)
{
	struct sta_info *sta = eloop_ctx;
	struct smd_roam_ap_info *ap_info = timeout_ctx;

	struct hostapd_data *prep_hapd = ap_info->st_prep_hapd;
	struct hostapd_data *partner_hapd;
	struct sta_info *partner_sta;
	u8 ap_mld_addr[ETH_ALEN];

	os_memcpy(ap_mld_addr, ap_info->ap_mld_addr, ETH_ALEN);

	wpa_printf(MSG_DEBUG, "UHR: ST prep timeout for AP " MACSTR,
		   MAC2STR(ap_mld_addr));

	ap_info->uhr_st_prep_timer_ongoing = false;

	/* Remove the original entry — frees ap_info */
	uhr_remove_ap_from_list(sta, ap_mld_addr);

	/* Remove stale clones from all partner link stations */
	if (prep_hapd) {
		for_each_mld_link(partner_hapd, prep_hapd) {
			if (partner_hapd == prep_hapd)
				continue;
			partner_sta = ap_get_sta(partner_hapd, sta->addr);
			if (partner_sta)
				uhr_remove_ap_from_list(partner_sta, ap_mld_addr);
		}
	}
}


int uhr_cur_start_st_prep_timer(struct sta_info *sta, const u8 *ap_mld_addr, u32 seconds)
{
	struct smd_roam_ap_info *ap_info;
	unsigned int timeout_sec = UHR_ST_PREP_TIMEOUT_SEC;

	if (!sta || !ap_mld_addr) {
		wpa_printf(MSG_ERROR, "UHR: Invalid parameters for ST prep timeout");
		return -1;
	}
	/* Find AP in the list */
	ap_info = uhr_find_ap_in_list(sta, ap_mld_addr);
	if (!ap_info) {
		wpa_printf(MSG_DEBUG, "UHR: AP " MACSTR " not found in candidate list",
			   MAC2STR(ap_mld_addr));
		return -1;
	}
	/* Cancel any existing timeout */
	eloop_cancel_timeout(uhr_st_prep_timeout_handler, sta, ap_info);
	/* Record start time */
	os_get_reltime(&ap_info->uhr_st_prep_start);
	ap_info->uhr_st_prep_timeout_occurred = false;
	ap_info->uhr_st_prep_timer_ongoing = true;


	/* Derive timeout from smd_prep_timeout conf (units of 64 TUs, 1 TU = 1024 us);
	 * fall back to UHR_ST_PREP_TIMEOUT_SEC when not configured. */
	if (ap_info->st_prep_hapd &&
	    ap_info->st_prep_hapd->conf->smd.smd_prep_timeout > 0) {
		timeout_sec = ((unsigned int)ap_info->st_prep_hapd->conf->smd.smd_prep_timeout
			       * 64 * 1024) / 1000000;
		if (timeout_sec == 0)
			timeout_sec = UHR_ST_PREP_TIMEOUT_SEC;
	}

	/* Start new timeout */
	if (eloop_register_timeout(timeout_sec, 0,
				   uhr_st_prep_timeout_handler, sta, ap_info) < 0) {
		wpa_printf(MSG_ERROR, "UHR: Failed to register ST prep timeout");
		ap_info->uhr_st_prep_timer_ongoing = false;
		return -1;
	}
	wpa_printf(MSG_DEBUG, "UHR: Started ST prep timeout (%d sec) for AP " MACSTR,
		   timeout_sec, MAC2STR(ap_mld_addr));
	
	return 0;
}


void uhr_cancel_st_prep_timeout(struct sta_info *sta, const u8 *ap_mld_addr)
{
	struct smd_roam_ap_info *ap_info;
	if (!sta || !ap_mld_addr)
		return;
	ap_info = uhr_find_ap_in_list(sta, ap_mld_addr);
	if (!ap_info || !ap_info->uhr_st_prep_timer_ongoing)
		return;
	eloop_cancel_timeout(uhr_st_prep_timeout_handler, sta, ap_info);
	ap_info->uhr_st_prep_timer_ongoing = false;
	wpa_printf(MSG_DEBUG, "UHR: Cancelled ST prep timeout for AP " MACSTR,
		   MAC2STR(ap_mld_addr));
}


struct smd_roam_ap_info *uhr_find_ap_in_list(struct sta_info *sta, const u8 *ap_mld_addr)
{
	struct smd_roam_ap_info *ap_info;
	
	if (!sta || !ap_mld_addr)
		return NULL;
	
	ap_info = sta->smd_info.ap_list;
	while (ap_info) {
		if (ether_addr_equal(ap_info->ap_mld_addr, ap_mld_addr))
			return ap_info;
		ap_info = ap_info->next;
	}
	
	return NULL;
}


/**
 * uhr_find_st_exec_ap - Find first AP in ST Execute state
 * @sta: Station info
 * Returns: AP info pointer if found, NULL otherwise
 *
 * This function searches the station's roaming candidate list for the first AP
 * that is in any ST Execute state (ST_EXEC_STARTED through DL_DRAIN_ACTIVE).
 * Used to identify which AP is currently undergoing ST Execute transition.
 */
struct smd_roam_ap_info *uhr_find_st_exec_ap(struct sta_info *sta)
{
	struct smd_roam_ap_info *ap_info;

	if (!sta)
		return NULL;

	ap_info = sta->smd_info.ap_list;
	while (ap_info) {
		if (ap_info->state >= SMD_AP_STATE_ST_EXEC_STARTED &&
		    ap_info->state <= SMD_AP_STATE_DL_DRAIN_ACTIVE)
			return ap_info;
		ap_info = ap_info->next;
	}

	return NULL;
}

/**
 * uhr_remove_ap_from_list - Remove AP from station's roaming candidate list
 * @sta: Station info
 * @ap_mld_addr: AP MLD MAC address to remove
 * Returns: 0 on success, -1 if not found
 */
int uhr_remove_ap_from_list(struct sta_info *sta, const u8 *ap_mld_addr)
{
	struct smd_roam_ap_info *ap_info, *prev = NULL;
	if (!sta || !ap_mld_addr)
		return -1;
	ap_info = sta->smd_info.ap_list;
	while (ap_info) {
		if (ether_addr_equal(ap_info->ap_mld_addr, ap_mld_addr)) {
			eloop_cancel_timeout(uhr_st_iap_timeout_handler, sta, ap_info);
			eloop_cancel_timeout(uhr_st_prep_timeout_handler, sta, ap_info);
			if (prev)
				prev->next = ap_info->next;
			else
				sta->smd_info.ap_list = ap_info->next;
			wpa_printf(MSG_DEBUG, "UHR: Removed AP " MACSTR " from candidate list",
				   MAC2STR(ap_mld_addr));
			os_free(ap_info);
			return 0;
		}
		prev = ap_info;
		ap_info = ap_info->next;
	}
	wpa_printf(MSG_DEBUG, "UHR: AP " MACSTR " not found in candidate list",
		   MAC2STR(ap_mld_addr));
	return -1;
}


/**
 * uhr_cleanup_sta_roam_contexts - Clean up all roaming contexts for a station
 * @hapd: Hostapd data
 * @sta: Station info
 *
 * Public cleanup function called from sta_info.c when freeing a station.
 * Cancels all pending timeouts and frees all AP entries in the roaming list.
 */
void uhr_cleanup_sta_roam_contexts(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct smd_roam_ap_info *ap_info, *next;

	if (!sta)
		return;

	eloop_cancel_timeout(uhr_cur_get_ctx_timeout, hapd, sta);

	ap_info = sta->smd_info.ap_list;
	while (ap_info) {
		next = ap_info->next;
		/* Cancel all pending timers before freeing */
		eloop_cancel_timeout(uhr_st_iap_timeout_handler, sta, ap_info);
		eloop_cancel_timeout(uhr_st_prep_timeout_handler, sta, ap_info);
		wpa_printf(MSG_DEBUG, "UHR: Freeing roam context for AP " MACSTR,
			   MAC2STR(ap_info->ap_mld_addr));
		os_free(ap_info);
		ap_info = next;
	}
	sta->smd_info.ap_list = NULL;
}

int uhr_parse_smd_bss_trans_elem(const struct ieee802_11_elems *elems,
				 u8 type,
				 struct uhr_smd_bss_transition_element *sbte)
{
	const u8 *pos;

	if (!elems || !sbte) {
		wpa_printf(MSG_ERROR, "UHR SBTE: NULL parameters");
		return -1;
	}

	os_memset(sbte, 0, sizeof(*sbte));

	if (!elems->smd_bsstransparams || !elems->smd_bsstransparams_len) {
		wpa_printf(MSG_ERROR, "UHR SBTE: NULL element list entry");
		return -1;
	}

	pos = elems->smd_bsstransparams;

	switch (type) {
	case UHR_SMD_ST_PREP_REQ:
		/*
		 * ST Info Format for PREP_REQ:
		 * (1) Common Information
		 * (1.1) Request DL SN Not Transferred
		 * (1.2) Request UL SN Not Transferred
		 * (2) Listen Interval
		 * (3) Presence Bitmap
		 * (3.1) SCS List Present
		 * (4) SCS List
		 */
		sbte->dl_sn_not_transferred = *pos & BIT(0);
		sbte->ul_sn_not_transferred = *pos & BIT(1);
		pos += 1;

		sbte->listen_interval = WPA_GET_LE16(pos);
		pos += 2;

		if (*pos & BIT(0)) {
			/* TODO: parse SCS list entries */
			wpa_printf(MSG_DEBUG,
				   "UHR SBTE: SCS list present but parsing not yet implemented");
		}
		pos += 1;

		/* TODO: pos must advance past count byte before skipping SCS IDs:
		 * pos += 1 + sbte->num_scs_ids */
		sbte->num_scs_ids = *pos;
		pos += sbte->num_scs_ids;
		break;
	case UHR_SMD_ST_PREP_RESP:
		break;
	case UHR_SMD_ST_EXEC_REQ:
		break;
	case UHR_SMD_ST_EXEC_RESP:
		break;
	}

	return 0;
}

size_t hostapd_uhr_eid_bmlie_from_rmlie(const struct wpabuf *mlbuf,
						u8 link_id,
						u8 *bmlie)
{
	const u8 *pos;
	u16 ml_control;
	u8 mac_addr[ETH_ALEN];
	u8 *out = bmlie;
	u16 bmlie_ml_control = 0;
	u8 bmlie_len = 0;
	u8 bmlie_common_info_len = 0;
	size_t len = 0;
	
	/* Optional fields from RMLIE */
	const u8 *eml_caps = NULL;
	const u8 *mld_caps = NULL;
	const u8 *ext_mld_caps = NULL;

	const struct ieee80211_eht_ml *ml;
	
	if (!mlbuf || !bmlie) {
		wpa_printf(MSG_ERROR, "SMD ST Prep: Invalid parameters");
		return 0;
	}
	ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	len = wpabuf_len(mlbuf);
	
	wpa_hexdump(MSG_DEBUG, "SMD ST Prep: Input RMLIE", ml, len);
	
	if (!ml)
		return 0;

	
	if (len < sizeof(*ml) + ETH_ALEN + 1UL)
		goto fail;
	
	const struct eht_ml_reconf_common_info *ml_common_info;

	ml_common_info = (const struct eht_ml_reconf_common_info *) ml->variable;

	pos = (const u8 *) ml_common_info->variable;

	ml_control = WPA_GET_LE16((const u8 *) ml) >> 4;

	if (!(ml_control & RECONF_MULTI_LINK_CTRL_PRES_MLD_MAC_ADDR))
		goto fail;

	os_memcpy(mac_addr, pos, ETH_ALEN);
	pos += ETH_ALEN;

	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_EML_CAPA) {
		eml_caps = pos;
		pos +=2;
	}
	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_MLD_CAPA) {
		mld_caps = pos;
		pos +=2;
	}

	if (ml_control & RECONF_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP) {
		ext_mld_caps = pos;
		pos +=2;
	}
	if (ml_control & RECONF_MULTI_LINK_CTRL_PRESS_TGT_MLD_ADDR) {
		pos += ETH_ALEN;
	}
	
	bmlie_ml_control = MULTI_LINK_CONTROL_TYPE_BASIC;
	bmlie_common_info_len = 1 + ETH_ALEN;
	
	if (eml_caps) {
		bmlie_ml_control |= BASIC_MULTI_LINK_CTRL_PRES_EML_CAPA;
		bmlie_common_info_len += 2;
	}
	if (mld_caps) {
		bmlie_ml_control |= BASIC_MULTI_LINK_CTRL_PRES_MLD_CAPA;
		bmlie_common_info_len += 2;
	}
	if (ext_mld_caps) {
		bmlie_ml_control |= BASIC_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP;
		bmlie_common_info_len += 2;
	}

	bmlie_len = 1 + 2 + bmlie_common_info_len;
	if (bmlie_len > 255)
		goto fail;
	
	*out++ = WLAN_EID_EXTENSION;
	*out++ = bmlie_len;
	*out++ = WLAN_EID_EXT_MULTI_LINK;

	WPA_PUT_LE16(out, bmlie_ml_control);
	out += 2;

	*out++ = bmlie_common_info_len;

	os_memcpy(out, mac_addr, ETH_ALEN);
	out += ETH_ALEN;

	if (eml_caps) {
		os_memcpy(out, eml_caps, 2);
		out += 2;
	}

	if (mld_caps) {
		os_memcpy(out, mld_caps, 2);
		out += 2;
	}

	if (ext_mld_caps) {
		os_memcpy(out, ext_mld_caps, 2);
		out += 2;
	}

	wpa_printf(MSG_DEBUG, "SMD ST Prep: Built BMLIE from RMLIE (%d bytes, link_id=%u)",
		   (int)(out - bmlie), link_id);
	wpa_printf(MSG_DEBUG, "  MLD MAC: " MACSTR, MAC2STR(mac_addr));
	wpa_printf(MSG_DEBUG, "  EML Caps: %s", eml_caps ? "present" : "absent");
	wpa_printf(MSG_DEBUG, "  MLD Caps: %s", mld_caps ? "present" : "absent");
	wpa_printf(MSG_DEBUG, "  Ext MLD Caps: %s", ext_mld_caps ? "present" : "absent");

	
	/* Debug: Dump output BMLIE */
	wpa_hexdump(MSG_DEBUG, "SMD ST Prep: Output BMLIE", bmlie, bmlie_len + 2);

	return (out - bmlie);
fail:
	return 0;
}


/**
 * uhr_parse_reconfig_mle - Parse UHR Reconfiguration Multi-Link element
 * @elems: Parsed 802.11 elements
 * @mle: Output structure for parsed ML-IE data
 * Returns: 0 on success, -1 on error
 *
 * Parses IEEE 802.11bn UHR Reconfiguration ML-IE with proper defragmentation.
 */
int uhr_parse_reconfig_mle(const struct ieee802_11_elems *elems,
			   struct uhr_reconfig_mle *mle)
{
	struct wpabuf *mlbuf = NULL;
	const struct ieee80211_eht_ml *ml;  /* Reuse EHT ML-IE structure */
	const struct eht_ml_reconf_common_info *common_info;
	size_t ml_len, common_info_len;
	u16 ml_control, presence_bitmap;
	const u8 *pos;
	int ret = -1;
	u8 bmlie[UHR_BMLIE_BUF_LEN];
	os_memset(bmlie, 0, sizeof(bmlie));

	/* NULL pointer checks */
	if (!elems || !mle) {
		wpa_printf(MSG_ERROR, "UHR ML-IE: NULL parameters");
		return -1;
	}
	os_memset(mle, 0, sizeof(*mle));

	/* Validate ML-IE presence and minimum length */
	if (!elems->reconf_mle || elems->reconf_mle_len < 4) {
		wpa_printf(MSG_DEBUG,
			   "UHR ML-IE: Not present or too short (%zu)",
			   elems->reconf_mle_len);
		return -1;
	}

	/* STEP 1: Defragment ML-IE */
	mlbuf = ieee802_11_defrag(elems->reconf_mle, elems->reconf_mle_len, true);
	if (!mlbuf) {
		wpa_printf(MSG_ERROR, "UHR: Failed to defrag Reconfiguration MLE");
		return -1;
	}

	ml = (const struct ieee80211_eht_ml *) wpabuf_head(mlbuf);
	ml_len = wpabuf_len(mlbuf);

	wpa_hexdump(MSG_DEBUG, "UHR: Defragged Reconfiguration MLE",
		    (const u8 *) ml, ml_len);

	/* STEP 2: Validate minimum length */
	if (ml_len < sizeof(*ml) + 1) {  /* ML Control + at least Common Info Length */
		wpa_printf(MSG_DEBUG, "UHR: ML-IE too short");
		goto out;
	}

	/* STEP 3: Parse ML Control field */
	ml_control = le_to_host16(ml->ml_control);
	
	/* Validate ML Type (bits 0-3) */
	if ((ml_control & UHR_RECONF_ML_CONTROL_TYPE_MASK) !=
	    UHR_RECONF_ML_CONTROL_TYPE_RECONF) {
		wpa_printf(MSG_DEBUG, "UHR: Invalid ML type=%u",
			   ml_control & UHR_RECONF_ML_CONTROL_TYPE_MASK);
		goto out;
	}

	/* Extract Presence Bitmap (bits 4-15) - shift right by 4 to reuse EHT defs */
	presence_bitmap = ml_control >> 4;

	wpa_printf(MSG_DEBUG, "UHR: ML Control=0x%04x, Presence Bitmap=0x%03x",
		   ml_control, presence_bitmap);

	/* STEP 4: Calculate expected Common Info length */
	common_info_len = 1;  /* Length octet */

	/* B0: MLD MAC Address (mandatory for UHR Reconfig) - Reuse EHT definition */
	if (!(presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_MLD_MAC_ADDR)) {
		wpa_printf(MSG_DEBUG, "UHR: MLD MAC Address not present (required)");
		goto out;
	}
	common_info_len += ETH_ALEN;

	/* B1: EML Capabilities (optional) - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_EML_CAPA)
		common_info_len += 2;

	/* B2: MLD Capabilities And Operations (optional) - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_MLD_CAPA)
		common_info_len += 2;

	/* B3: Extended MLD Capabilities And Operations (optional) - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP)
		common_info_len += 2;

	/* B4: Target AP MLD MAC Address (optional but critical for UHR) - UHR-specific */
	if (presence_bitmap & UHR_RECONF_ML_CTRL_PRES_TARGET_AP_MLD_ADDR)
		common_info_len += ETH_ALEN;

	wpa_printf(MSG_DEBUG, "UHR: Expected Common Info length=%zu",
		   common_info_len);

	/* STEP 5: Validate Common Info */
	if (ml_len < sizeof(*ml) + common_info_len) {
		wpa_printf(MSG_DEBUG, "UHR: Not enough bytes for Common Info");
		goto out;
	}

	common_info = (const struct eht_ml_reconf_common_info *) ml->variable;

	if (common_info->len < common_info_len) {
		wpa_printf(MSG_DEBUG,
			   "UHR: Invalid Common Info len=%u (expected >=%zu)",
			   common_info->len, common_info_len);
		goto out;
	}

	/* STEP 6: Extract Common Info fields */
	pos = common_info->variable;

	/* Extract MLD MAC Address (mandatory) */
	os_memcpy(mle->mld_mac_addr, pos, ETH_ALEN);
	pos += ETH_ALEN;
	wpa_printf(MSG_DEBUG, "UHR: STA MLD MAC=" MACSTR, MAC2STR(mle->mld_mac_addr));

	/* Skip optional EML Capabilities - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_EML_CAPA)
		pos += 2;

	/* Skip optional MLD Capabilities - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_MLD_CAPA)
		pos += 2;

	/* Skip optional Extended MLD Capabilities - Reuse EHT definition */
	if (presence_bitmap & RECONF_MULTI_LINK_CTRL_PRES_EXT_MLD_CAP)
		pos += 2;

	/* Extract Target AP MLD MAC Address (critical for UHR) - UHR-specific */
	if (presence_bitmap & UHR_RECONF_ML_CTRL_PRES_TARGET_AP_MLD_ADDR) {
		os_memcpy(mle->target_ap_mld_addr, pos, ETH_ALEN);
		mle->has_target_ap_mld_addr = true;
		pos += ETH_ALEN;
		wpa_printf(MSG_DEBUG, "UHR: Target AP MLD MAC=" MACSTR,
			   MAC2STR(mle->target_ap_mld_addr));
	} else {
		/* ST Prep Request: Target AP MLD MAC is carried in the MLD MAC field */
		os_memcpy(mle->target_ap_mld_addr, mle->mld_mac_addr, ETH_ALEN);
		mle->has_target_ap_mld_addr = true;
		wpa_printf(MSG_DEBUG, "UHR: Target AP MLD MAC from MLD MAC field=" MACSTR,
			   MAC2STR(mle->target_ap_mld_addr));
	}

	/* Validate Target AP MLD MAC Address */
	if (is_zero_ether_addr(mle->target_ap_mld_addr) ||
	    is_broadcast_ether_addr(mle->target_ap_mld_addr)) {
		wpa_printf(MSG_DEBUG, "UHR: Invalid Target AP MLD MAC");
		goto out;
	}

	ret = 0;

out:
	wpabuf_free(mlbuf);
	return ret;
}


/**
 * uhr_tgt_st_prep_timer_cleanup - Cleanup handler for prep timer timeout
 * @eloop_ctx: hostapd_data pointer
 * @timeout_ctx: Station MAC address
 *
 * Called when ST Exec doesn't arrive within timeout period.
 * Removes station entries created during ST Prep to free resources.
 */
void uhr_tgt_st_prep_timer_cleanup(void *eloop_ctx, void *timeout_ctx)
{
       struct hostapd_data *hapd = eloop_ctx;
       u8 *sta_addr = timeout_ctx;
       struct sta_info *sta;
       unsigned int i;

       wpa_printf(MSG_INFO,
                  "UHR Target AP: ST Prep timer expired for " MACSTR " - cleaning up",
                  MAC2STR(sta_addr));

       /* Remove station from all links in the MLD */
       for (i = 0; i < hapd->iface->interfaces->count; i++) {
               struct hostapd_iface *iface = hapd->iface->interfaces->iface[i];
               struct hostapd_data *bss;
               size_t j;

               if (!iface)
                       continue;

               for (j = 0; j < iface->num_bss; j++) {
                       bss = iface->bss[j];
                       if (!bss)
                               continue;

                       sta = ap_get_sta(bss, sta_addr);
                       if (sta) {
			       struct hostapd_data *assoc_hapd;
			       struct sta_info *assoc_sta = NULL;

			       assoc_sta = hostapd_ml_get_assoc_sta(bss, sta, &assoc_hapd);

			       /* Clear timer reference */
                               sta->smd_info.uhr_target_prep_timer = 0;
                               sta->smd_info.tgt_prep_timer_ctx = NULL;
                               sta->smd_info.tgt_prep_timer_hapd = NULL;

			       if (assoc_sta &&
				   assoc_sta->smd_info.state == SMD_STA_ST_EXEC_DONE)
				       continue;

                               wpa_printf(MSG_DEBUG,
                                          "UHR Target AP: Removing STA from link %u",
                                          bss->mld_link_id);

                               /* Remove station */
				ap_sta_disconnect(bss, sta, sta->addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
				ap_free_sta(bss, sta);
                       }
               }
       }

       os_free(sta_addr);
}


/**
 *_uhr_tgtart_st_prep_timer_start_st_prep_timer - Start target prep timer
 * @hapd: hostapd data
 * @sta_addr: Station MAC address
 *
 * Starts timer after successful ST Prep IAP RESPONSE.
 * Timer will be cancelled if ST Exec arrives, or will trigger
 * cleanup if timeout expires.
 */
void uhr_tgt_start_st_prep_timer(struct hostapd_data *hapd,
                                       const u8 *sta_addr)
{
       struct sta_info *sta;
       u8 *addr_copy;
       unsigned int timeout_sec;

       sta = ap_get_sta(hapd, sta_addr);
       if (!sta)
               return;


       /* Cancel any existing timer before registering a new one.
        * On re-prep the old eloop entry and its addr_copy must be
        * discarded, otherwise two timers fire against the same STA. */
       if (sta->smd_info.uhr_target_prep_timer) {
               struct hostapd_data *old_hapd =
                       sta->smd_info.tgt_prep_timer_hapd
                       ? sta->smd_info.tgt_prep_timer_hapd : hapd;
               eloop_cancel_timeout(uhr_tgt_st_prep_timer_cleanup, old_hapd,
                                    sta->smd_info.tgt_prep_timer_ctx);
               os_free(sta->smd_info.tgt_prep_timer_ctx);
               sta->smd_info.tgt_prep_timer_ctx = NULL;
               sta->smd_info.tgt_prep_timer_hapd = NULL;
               sta->smd_info.uhr_target_prep_timer = 0;
       }

       timeout_sec = UHR_ST_PREP_TIMEOUT_SEC;
       if (hapd->conf->smd.smd_prep_timeout > 0) {
               timeout_sec = ((unsigned int)hapd->conf->smd.smd_prep_timeout
                              * 64 * 1024) / 1000000;
               if (timeout_sec == 0)
                       timeout_sec = UHR_ST_PREP_TIMEOUT_SEC;
       }
       			
       addr_copy = os_memdup(sta_addr, ETH_ALEN);
       if (!addr_copy)
               return;

       eloop_register_timeout(timeout_sec, 0, uhr_tgt_st_prep_timer_cleanup,
                              hapd, addr_copy);
       sta->smd_info.uhr_target_prep_timer = 1;
       sta->smd_info.tgt_prep_timer_ctx = addr_copy;
       sta->smd_info.tgt_prep_timer_hapd = hapd;

       wpa_printf(MSG_DEBUG,
                  "UHR Target AP: Started prep timer for " MACSTR " (%u sec)",
                  MAC2STR(sta_addr), timeout_sec);
}


/**
 * uhr_tgt_cancel_st_prep_timer - Cancel target prep timer
 * @hapd: hostapd data
 * @sta_addr: Station MAC address
 *
 * Cancels timer when ST Exec arrives at target AP.
 */
void uhr_tgt_cancel_st_prep_timer(struct hostapd_data *hapd,
                                        const u8 *sta_addr)
{
       struct sta_info *sta;
       struct hostapd_data *timer_hapd;

       sta = ap_get_sta(hapd, sta_addr);
       if (!sta || !sta->smd_info.uhr_target_prep_timer)
               return;

       timer_hapd = sta->smd_info.tgt_prep_timer_hapd
		    ? sta->smd_info.tgt_prep_timer_hapd : hapd;
       eloop_cancel_timeout(uhr_tgt_st_prep_timer_cleanup, timer_hapd,
                            sta->smd_info.tgt_prep_timer_ctx);
       os_free(sta->smd_info.tgt_prep_timer_ctx);
       sta->smd_info.tgt_prep_timer_ctx = NULL;
       sta->smd_info.tgt_prep_timer_hapd = NULL;
       sta->smd_info.uhr_target_prep_timer = 0;

       wpa_printf(MSG_DEBUG,
                  "UHR Target AP: Cancelled prep timer for " MACSTR,
                  MAC2STR(sta_addr));
}

/**
 * uhr_cur_get_ctx_timeout - Cancels current AP GET_SMD_CTX timer and sends IAP
 * @eloop_ctx: ctx for eloop timer (@hapd)
 * @timeout_ctx: ctx for timeout (@sta)
 */
void uhr_cur_get_ctx_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	struct smd_roam_ap_info *ap_info;
	const u8 *target_ap_mld_addr =
		sta->smd_info.get_ctx_pending.target_ap_mld_addr;

	wpa_printf(MSG_ERROR,
		   "GET_SMD_CTX timeout for target " MACSTR " (STA " MACSTR ")",
		   MAC2STR(target_ap_mld_addr), MAC2STR(sta->addr));

	ap_info = uhr_find_ap_in_list(sta, target_ap_mld_addr);
	if (!ap_info) {
		wpa_printf(MSG_ERROR, "GET_SMD_CTX timeout: No ap_info for " MACSTR,
			   MAC2STR(target_ap_mld_addr));
		return;
	}

	if (ap_info->state != SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_WAIT ||
	    !sta->smd_info.get_ctx_pending.active) {
		wpa_printf(MSG_ERROR, "GET_SMD_CTX timeout: Invalid state %u (pending=%d)",
			   ap_info->state, sta->smd_info.get_ctx_pending.active);
		return;
	}

	sta->smd_info.get_ctx_pending.active = false;
	ap_info->state = SMD_AP_STATE_ST_PREP_COMPLETE;
	wpa_printf(MSG_DEBUG, "GET_SMD_CTX timeout: Rolling back to ST_PREP_COMPLETE");

	uhr_iap_send_st_ctx_response(hapd,
				     target_ap_mld_addr,
				     sta->addr,
				     sta->smd_info.get_ctx_pending.iap_transaction_id,
				     UHR_IAP_STATUS_FAILURE,
				     NULL);
}
