/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2017, Jouni Malinen <j@w1.fi>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/crc32.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "crypto/random.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/sae.h"
#include "common/dpp.h"
#include "common/ocv.h"
#include "common/wpa_common.h"
#include "common/wpa_ctrl.h"
#include "common/ptksa_cache.h"
#include "common/nan_de.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "p2p/p2p.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "hostapd.h"
#include "hostapd_log.h"
#include "beacon.h"
#include "ieee802_11_auth.h"
#include "sta_info.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "pmksa_cache_auth.h"
#include "wmm.h"
#include "ap_list.h"
#include "accounting.h"
#include "ap_config.h"
#include "ap_mlme.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "wnm_ap.h"
#include "hw_features.h"
#include "ieee802_11.h"
#include "hostapd_if/hostapd_if.h"
#include "dfs.h"
#include "mbo_ap.h"
#include "rrm.h"
#include "taxonomy.h"
#include "fils_hlp.h"
#include "dpp_hostapd.h"
#include "gas_query_ap.h"
#include "comeback_token.h"
#include "nan_usd_ap.h"
#include "pasn/pasn_common.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#include "wpa_auth_i.h"
#include "ttlm.h"
#include "dscp_policy.h"
#include "ap/ctrl_iface_ap.h"

#ifdef CONFIG_IEEE80211BN
#include "mapc.h"
#endif /* CONFIG_IEEE80211BN */

#define CIPIE_ELEMENT_ID 0
#define CIPIE_LENGTH 1
#define CIPIE_ELEMENT_ID_EXTENSION 2
#define CIPIE_PADDING_DELAY 3
#define CIP_CAPAB_LEN 4

/* Buffer slack over the mgmt header for assoc-resp variable elements */
#define ASSOC_RESP_BUF_SLACK	2048

#define SECURITY_PROFILE_INDICATION 2
#define SECURITY_PROFILE_BITMAP 3

#ifdef CONFIG_IEEE80211AX
#include "robust_av.h"
#endif

#ifdef CONFIG_IEEE80211BN
#include "uhr_utils.h"
#endif /* CONFIG_IEEE80211BN */


#ifdef CONFIG_FILS
static struct wpabuf *
prepare_auth_resp_fils(struct hostapd_data *hapd,
		       struct sta_info *sta, u16 *resp,
		       struct rsn_pmksa_cache_entry *pmksa,
		       struct wpabuf *erp_resp,
		       const u8 *msk, size_t msk_len,
		       int *is_pub);
#endif /* CONFIG_FILS */

#ifdef CONFIG_PASN
#ifdef CONFIG_FILS

static void pasn_fils_auth_resp(struct hostapd_data *hapd,
				struct sta_info *sta, u16 status,
				struct wpabuf *erp_resp,
				const u8 *msk, size_t msk_len);

#endif /* CONFIG_FILS */
#endif /* CONFIG_PASN */

#ifdef CONFIG_IEEE80211BN

/**
 * hostapd_parse_smd_ie - Parse SMD Information Element from authentication
 * @hapd: hostapd data
 * @sta: Station info
 * @ies: IEs buffer
 * @ies_len: Length of IEs buffer
 *
 * Parse and store SMD IE information from station's authentication request.
 */
void hostapd_parse_smd_ie(struct hostapd_data * hapd, struct sta_info *sta, const u8 *ies,
				 size_t ies_len)
{
	const u8 *smd_ie;
	const u8 *pos;
	u8 smd_cap_byte;

	if (!sta || !ies)
		return;

	/* Check if AP supports SMD and SMD is enabled in configuration */
	if (!hapd->conf->smd.enabled) {
		wpa_printf(MSG_DEBUG, "SMD IE Parse: SMD not enabled in AP configuration for " MACSTR,
			   MAC2STR(sta->addr));
		return;
	}

	if (!hapd->mld) {
		wpa_printf(MSG_DEBUG, "SMD IE Parse: MLD not enabled in AP configuration for " MACSTR,
			   MAC2STR(sta->addr));
		return;
	}

	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SMD)) {
		wpa_printf(MSG_DEBUG, "SMD IE Parse: Driver does not support SMD for " MACSTR,
			   MAC2STR(sta->addr));
		return;
	}

	/* Initialize SMD info */
	os_memset(&sta->smd_info, 0, sizeof(sta->smd_info));

	/* Look for SMD Information Element (Extension element) */
	smd_ie = get_ie_ext(ies, ies_len, WLAN_EID_EXT_SMD);
	if (!smd_ie) {
		wpa_printf(MSG_DEBUG, "SMD IE: Not found in auth request from "
			   MACSTR, MAC2STR(sta->addr));
		return;
	}

	/* SMD IE format after Element ID Extension:
	 * SMD Identifier: 6 octets
	 * SMD Capabilities: 1 octet
	 * Timeout Value: 1 octet (units of 64 TUs)
	 * Total minimum length: 8 octets (+ 2 for EID and Len, + 1 for Ext ID)
	 */
	if (smd_ie[1] < 1 + 6 + 1 + 1) {
		wpa_printf(MSG_DEBUG,
			   "SMD IE: Invalid length %u from " MACSTR,
			   smd_ie[1], MAC2STR(sta->addr));
		return;
	}

	pos = smd_ie + 3; /* Skip Element ID, Length, and Extension ID */

	/* SMD Identifier (6 octets) */
	os_memcpy(sta->smd_info.smd_identifier, pos, ETH_ALEN);
	pos += ETH_ALEN;

	/* SMD Capabilities (1 octet)
	 * B0: DL Data Forwarding
	 * B1-B3: Max Number Of Prepared Target AP MLDs
	 * B4: SMD Type
	 * B5: PTK Mode
	 * B6: Neighboring AP Probing Support
	 * B7: Reserved
	 */
	smd_cap_byte = *pos++;
	sta->smd_info.caps.dl_data_fwd = !!(smd_cap_byte & BIT(0));
	sta->smd_info.caps.max_prep_target_apmlds = (smd_cap_byte >> 1) & 0x07;
	sta->smd_info.caps.smd_type = !!(smd_cap_byte & BIT(4));
	sta->smd_info.caps.ptk_mode = !!(smd_cap_byte & BIT(5));

	/* Timeout Value (1 octet, units of 64 TUs) */
	sta->smd_info.smd_timeout = hapd->conf->smd.smd_prep_timeout;

	sta->smd_info.smd_sta = true;

	os_memcpy(sta->smd_info.current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);

	wpa_printf(MSG_INFO,
		   "SMD IE: Parsed from " MACSTR " - ID: " MACSTR
		   ", DL Fwd: %d, Max Peer AP MLDs: %u, Type: %d, PTK Mode: %d, Timeout: %u TU",
		   MAC2STR(sta->addr),
		   MAC2STR(sta->smd_info.smd_identifier),
		   sta->smd_info.caps.dl_data_fwd,
		   sta->smd_info.caps.max_prep_target_apmlds,
		   sta->smd_info.caps.smd_type,
		   sta->smd_info.caps.ptk_mode,
		   sta->smd_info.smd_timeout);
	return;
}


/**
 * hostapd_eid_smd_ie_response - Add SMD IE to authentication or association response
 * @hapd: BSS data
 * @sta: Station info
 * @eid: Pointer to current position in buffer
 * Returns: Pointer to next position in buffer
 */
static u8 * hostapd_eid_smd_ie_response(struct hostapd_data *hapd,
					struct sta_info *sta, u8 *eid)
{
	if (!hapd->conf->smd.enabled || !sta || !sta->smd_info.smd_sta)
		return eid;

	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SMD))
		return eid;

	return hostapd_eid_smd(hapd, eid);
}
#endif /* CONFIG_IEEE80211BN */

static void handle_auth(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt, size_t len,
			int rssi, int from_queue);
static struct wpabuf *cip_build_assoc_resp_ie(u8 padding_delay);

#ifdef CONFIG_IEEE80211BE
static u8 * hostapd_eid_mcst(struct hostapd_data *hapd, u8 *eid,
			     size_t len)
{
	u32 switch_time;

	if (len < 6)
		return eid;

	if (!hapd->iface->cac_started || !is_5ghz_freq(hapd->iface->freq) ||
	    !hapd->iface->dfs_cac_ms)
		return eid;

	switch_time = hostapd_get_remaining_cac_tu(hapd->iface);

	return hostapd_eid_add_max_cs_time(eid, switch_time);
}
#endif /* CONFIG_IEEE80211BE */

static u16 check_rssi_association(struct hostapd_data *hapd,
				  const struct ieee80211_mgmt *mgmt,
				  int rssi, struct sta_info *sta);

#ifdef CONFIG_IEEE8021X_AUTH
static struct rsn_pmksa_cache_entry *
pmksa_cache_search(void *ctx, const u8 *spa, const u8 *pmkid, bool is_ml);
#endif /* CONFIG_IEEE8021X_AUTH */

static u8 * hostapd_eid_multi_ap(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	struct multi_ap_params multi_ap = { 0 };

	if (!hapd->conf->multi_ap)
		return eid;

	if (hapd->conf->multi_ap & BACKHAUL_BSS)
		multi_ap.capability |= MULTI_AP_BACKHAUL_BSS;
	if (hapd->conf->multi_ap & FRONTHAUL_BSS)
		multi_ap.capability |= MULTI_AP_FRONTHAUL_BSS;

	if (hapd->conf->multi_ap_client_disallow &
	    PROFILE1_CLIENT_ASSOC_DISALLOW)
		multi_ap.capability |=
			MULTI_AP_PROFILE1_BACKHAUL_STA_DISALLOWED;
	if (hapd->conf->multi_ap_client_disallow &
	    PROFILE2_CLIENT_ASSOC_DISALLOW)
		multi_ap.capability |=
			MULTI_AP_PROFILE2_BACKHAUL_STA_DISALLOWED;

	multi_ap.profile = hapd->conf->multi_ap_profile;
	multi_ap.vlanid = hapd->conf->multi_ap_vlanid;

	return eid + add_multi_ap_ie(eid, len, &multi_ap);
}


static size_t hostapd_supp_rates(struct hostapd_data *hapd, u8 *buf)
{
	u8 *pos = buf;
	int i;
#ifdef CONFIG_IEEE80211BE
	bool require_eht;
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->current_rates)
		return 0;

	for (i = 0; i < hapd->num_rates; i++) {
		*pos = hapd->current_rates[i].rate / 5;
		if (hapd->current_rates[i].flags & HOSTAPD_RATE_BASIC)
			*pos |= 0x80;
		pos++;
	}

	if (hapd->iconf->ieee80211n &&
	    (hapd->iconf->require_ht || hapd->conf->bss_require_ht))
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HT_PHY;

	if (hapd->iconf->ieee80211ac &&
	    (hapd->iconf->require_vht || hapd->conf->bss_require_vht))
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_VHT_PHY;

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax &&
	    (hapd->iconf->require_he || hapd->conf->bss_require_he))
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HE_PHY;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	require_eht = hapd->iconf->require_eht || hapd->conf->bss_require_eht;
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be &&
	    require_eht)
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_EHT_PHY;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_SAE
	if ((hapd->conf->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
	     hostapd_sae_pw_id_in_use(hapd->conf) == 2) &&
	    hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK &&
	    wpa_key_mgmt_only_sae(hapd->conf->wpa_key_mgmt))
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_SAE_H2E_ONLY;
#endif /* CONFIG_SAE */

	return pos - buf;
}


#define MAX_SECURITY_PROFILE_NUM 16

static const struct security_profile_entry_ap security_profile_table[MAX_SECURITY_PROFILE_NUM] = {
    /* profile_num, key_mgmt, pairwise_cipher, mfpr, mfpc,
     * ieee8021x_auth_frame, assoc_frame_encrypt,
     * pmksa_caching_privacy, kek_in_pasn, unauth_eppke
     */

     /* 0: EPPKE (AKM 29), unauth‑EPPKE allowed */
	[0]  = { 0,  WPA_KEY_MGMT_EPPKE,			  WPA_CIPHER_GCMP_256,
		true, false, false,  true,  true,  true,  true },

    /* 1: EPPKE + SAE (AKM 29+24) */
	[1]  = { 1,  WPA_KEY_MGMT_EPPKE | WPA_KEY_MGMT_SAE_EXT_KEY,
		WPA_CIPHER_GCMP_256, true, false, false,  true,	true,  true,  false },

    /* 2: EPPKE + FT‑SAE (AKM 29+25) */
	[2]  = { 2,  WPA_KEY_MGMT_EPPKE | WPA_KEY_MGMT_FT_SAE_EXT_KEY,
		WPA_CIPHER_GCMP_256, true, false, true,  true,	true,  true,  false },

    /* 3: 802.1X/5 + assoc‑encrypt */
	[3]  = { 3,  WPA_KEY_MGMT_IEEE8021X_SHA256,  WPA_CIPHER_GCMP_256,
		true, true,  true,  true,  true,  false, false },

    /* 4: FT‑802.1X/3 + assoc‑encrypt */
	[4]  = { 4,  WPA_KEY_MGMT_FT_IEEE8021X,      WPA_CIPHER_GCMP_256,
		true, true,  true,  true,  true,  false, false },

    /* 5: FILS‑SHA256/23 + assoc‑encrypt */
	[5]  = { 5,  WPA_KEY_MGMT_IEEE8021X_SHA384,	     WPA_CIPHER_GCMP_256,
		true, true,  true,  true,  true,  false, false },

    /* 6: FT‑FILS‑SHA256/22 + assoc‑encrypt */
	[6]  = { 6,  WPA_KEY_MGMT_FT_FILS_SHA256,    WPA_CIPHER_GCMP_256,
		true, true,  true,  true,  true,  false, false },

    /* 7: IEEE8021X‑SHA384/12 + assoc‑encrypt */
	[7]  = { 7,  WPA_KEY_MGMT_IEEE8021X_SUITE_B_192,  WPA_CIPHER_GCMP_256,
		true, true,  true,  true,  true,  false, false },

    /* 8: OWE/18 */
	[8]  = { 8,  WPA_KEY_MGMT_OWE,		     WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 9: SAE/24 */
	[9]  = { 9,  WPA_KEY_MGMT_SAE_EXT_KEY,	     WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 10: FT‑SAE/25 */
	[10] = { 10, WPA_KEY_MGMT_FT_SAE_EXT_KEY,    WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 11: 802.1X/5 (no assoc‑encrypt) */
	[11] = { 11, WPA_KEY_MGMT_IEEE8021X_SHA256,  WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 12: FT‑802.1X/3 (no assoc‑encrypt) */
	[12] = { 12, WPA_KEY_MGMT_FT_IEEE8021X,      WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 13: FILS‑SHA256/23 (no assoc‑encrypt) */
	[13] = { 13, WPA_KEY_MGMT_IEEE8021X_SHA384,	     WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 14: FT‑FILS‑SHA256/22 (no assoc‑encrypt) */
	[14] = { 14, WPA_KEY_MGMT_FT_FILS_SHA256,    WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },

    /* 15: IEEE8021X‑SHA384/12 (no assoc‑encrypt) */
	[15] = { 15, WPA_KEY_MGMT_IEEE8021X_SUITE_B_192,  WPA_CIPHER_GCMP_256,
		true, false, false, false, false, false, false },
};

static int hostapd_get_sta_num_sec_prof(const u8 *sec_prof_ie, size_t len)
{
	u8 indication, bitmap_len;
	const u8 *bitmap;
	size_t i;

	if (!sec_prof_ie || len < 2) {
		wpa_printf(MSG_DEBUG, "UHR: Security Profile element too short");
		return -1;
	}

	/* Parse Security Profile Indication field (1 byte):
	 * Bits 0-3: bitmap_len (number of octets in bitmap)
	 * Bits 4-7: vendor profile count
	 * IEEE 802.11bn D1.4, Figure 9-aa70
	 */
	indication = sec_prof_ie[SECURITY_PROFILE_INDICATION];
	bitmap_len = indication & SECURITY_PROFILE_INDICATION_BITMAP_LEN_MASK;

	if (len < (size_t)(3 + bitmap_len)) {
		wpa_printf(MSG_DEBUG, "UHR: Truncated Security Profile bitmap");
		return -1;
	}

	if (bitmap_len == 0) {
		wpa_printf(MSG_DEBUG, "UHR: Empty Security Profile bitmap");
		return -1;
	}

	bitmap = &sec_prof_ie[SECURITY_PROFILE_BITMAP];

	/* first set bit is the profile number */
	for (i = 0; i < bitmap_len * 8; i++) {
		if (bitmap[i / 8] & BIT(i % 8)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: STA profile number extracted: %zu", i);
			return (int) i;
		}
	}

	wpa_printf(MSG_DEBUG, "UHR: No profile set in bitmap");
	return -1;
}

/**
 * is_valid_profile - Check if a profile number is valid
 * @profile_num: Profile number to validate (0-15)
 * Returns: true if valid, false otherwise
 */
static bool is_valid_profile(int profile_num)
{
	if (profile_num < 0 || profile_num >= MAX_SECURITY_PROFILE_NUM)
		return false;

	return security_profile_table[profile_num].key_mgmt != 0;
}


/**
 * hostapd_find_validate_profile - Find and validate a matching security profile
 * @hapd: AP data
 * @addr: STA MAC address (for logging)
 * @sta_profile_num: Profile number from STA's Security Profile element
 * @rsne_data: Parsed RSNE data from STA
 * @rsnxe: STA's RSNXE.
 * @rsnxe_len: STA's RSNXE data len
 * @profile_matched: On success, filled with the matched profile entry
 * Returns: true if a matching profile is found and validated, false otherwise
 */
static bool hostapd_find_validate_profile(
	struct hostapd_data *hapd,
	const u8 *addr,
	int sta_profile_num,
	const struct wpa_ie_data *rsne_data,
	const u8 *rsnxe,
	size_t rsnxe_len,
	struct security_profile_entry_ap *profile_matched)
{
	const struct security_profile_entry_ap *entry;
	size_t i;

	/* Validate the profile number is in the table before searching */
	if (!is_valid_profile(sta_profile_num)) {
		wpa_printf(MSG_ERROR,
			   "UHR: Profile %d not in table",
			   sta_profile_num);
		return false;
	}

	/* Check if this profile is in the AP's advertised list */
	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		if (hapd->conf->security_profiles[i] != sta_profile_num)
			continue;

		entry = &security_profile_table[sta_profile_num];

		/* Validate RSNE fields */
		if (!(entry->key_mgmt & rsne_data->key_mgmt)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d key_mgmt mismatch: required=0x%x got=0x%x",
				   sta_profile_num, entry->key_mgmt,
				   rsne_data->key_mgmt);
			return false;
		}
		if (!(entry->pairwise_cipher & rsne_data->pairwise_cipher)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d pairwise cipher mismatch: required=0x%x got=0x%x",
				   sta_profile_num, entry->pairwise_cipher,
				   rsne_data->pairwise_cipher);
			return false;
		}
		if (entry->mfpc &&
		    !(rsne_data->capabilities & WPA_CAPABILITY_MFPC)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d MFPC required but not set",
				   sta_profile_num);
			return false;
		}
		if (entry->mfpr &&
		    !(rsne_data->capabilities & WPA_CAPABILITY_MFPR)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d MFPR required but not set",
				   sta_profile_num);
			return false;
		}

		/* Validate RSNXE fields */
		if (entry->ieee8021x_auth_frame &&
		    !ieee802_11_rsnx_capab_len(rsnxe, rsnxe_len,
					   WLAN_RSNX_CAPAB_802_1X_IN_AUTH_FRAMES)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d 8021x_auth_frame capab missing",
				   sta_profile_num);
			return false;
		}
		if (entry->assoc_frame_encrypt &&
		    !ieee802_11_rsnx_capab_len(rsnxe, rsnxe_len,
					   WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d assoc_frame_encrypt capab missing",
				   sta_profile_num);
			return false;
		}
		if (entry->pmksa_caching_privacy &&
		    !ieee802_11_rsnx_capab_len(rsnxe, rsnxe_len,
					   WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY)) {
			wpa_printf(MSG_DEBUG,
				   "UHR: Profile %d pmksa_caching_privacy capab missing",
				   sta_profile_num);
			return false;
		}

		wpa_printf(MSG_DEBUG,
			   "UHR: Station " MACSTR
			   " matches security profile %d (key_mgmt=0x%x cipher=0x%x)",
			   MAC2STR(addr), sta_profile_num,
			   entry->key_mgmt, entry->pairwise_cipher);
		if (profile_matched)
			*profile_matched = *entry;
		return true;
	}

	wpa_printf(MSG_INFO,
		   "UHR: Station " MACSTR
		   " profile %d not in AP's advertised list",
		   MAC2STR(addr), sta_profile_num);
	return false;
}

/*
 * validate_sta_security_profile - Validate STA's Security Profile element
 *
 * Validates that the STA's indicated security profile matches one of the
 * AP's advertised profiles and that the RSNE/RSNXE fields are consistent.
 *
 * @hapd: AP data
 * @addr: STA MAC address (for logging)
 * @wpa_ie: STA's RSNE (including EID and Length)
 * @wpa_ie_len: Length of wpa_ie
 * @rsnxe: STA's RSNXE (including EID and Length), or NULL
 * @rsnxe_len: Length of rsnxe (unused; rsnxe already includes EID+Len)
 * @sta_sec_ie: Body of STA's Security Profile element (after EID_Ext byte)
 * @sta_sec_ie_len: Length of sta_sec_ie
 * @profile_matched: On success, filled with the matched profile entry
 * Returns: true if validation passes, false otherwise
 */
static bool validate_sta_security_profile(
	struct hostapd_data *hapd,
	const u8 *addr,
	const u8 *wpa_ie, size_t wpa_ie_len,
	const u8 *rsnxe, size_t rsnxe_len,
	const u8 *sta_sec_ie, size_t sta_sec_ie_len,
	struct security_profile_entry_ap *profile_matched)
{
	struct wpa_ie_data data;
	int sta_profile_num;

	if (!hapd->conf->security_profiles)
		return true; /* No profiles configured, allow */

	wpa_printf(MSG_DEBUG, "UHR: Validating security profile for STA " MACSTR,
		   MAC2STR(addr));

	/* Parse station's RSNE */
	if (!wpa_ie || wpa_ie_len < 2) {
		wpa_printf(MSG_INFO, "UHR: Station " MACSTR " missing RSNE",
			   MAC2STR(addr));
		return false;
	}

	if (wpa_parse_wpa_ie_rsn(wpa_ie-2, wpa_ie_len+2, &data) < 0) {
		wpa_printf(MSG_INFO, "UHR: Station " MACSTR " invalid RSNE",
			   MAC2STR(addr));
		return false;
	}

	/* Extract STA's profile number from Security Profile element */
	sta_profile_num = hostapd_get_sta_num_sec_prof(sta_sec_ie, sta_sec_ie_len);
	if (sta_profile_num < 0) {
		wpa_printf(MSG_INFO,
			   "UHR: Station " MACSTR " has invalid Security Profile element",
			   MAC2STR(addr));
		return false;
	}

	wpa_printf(MSG_DEBUG, "UHR: STA " MACSTR " profile number: %d",
		   MAC2STR(addr), sta_profile_num);

	/* Use the helper function to find and validate the profile */
	return hostapd_find_validate_profile(hapd, addr, sta_profile_num,
					 &data, rsnxe, rsnxe_len ,profile_matched);
}
/**
 * validate_security_profile_common - Common UHR Security Profile validation
 * @hapd: hostapd BSS data structure
 * @sta_info: Station address.
 * @ies: Pointer to the start of IEs
 * @ies_len: Length of IEs
 * @auth_context: Authentication context string (e.g., "SAE", "PASN", "802.1X")
 *
 * This helper function consolidates the common Security Profile validation logic
 * used across different authentication methods (SAE, PASN, 802.1X).
 *
 * Returns: WLAN_STATUS_SUCCESS on success or validation not needed,
 *	    WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE on validation failure
 */
static u16 validate_security_profile_common(
	struct hostapd_data *hapd,
	struct sta_info *sta,
	struct ieee802_11_elems *elems,
	const char *auth_context,
	struct security_profile_entry_ap *profile_matched)
{
	u8 *addr;


	addr = sta->addr;

	/* Skip validation if no Security Profiles are configured */
	if (!hapd->conf->security_profiles)
		return WLAN_STATUS_SUCCESS;

	if (!elems->security_profile_ie) {
		/* No Security Profile element present - validation not needed */
		return WLAN_STATUS_SUCCESS;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR: Validating Security Profile from " MACSTR
		   " in %s auth (body_len=%zu)",
		   MAC2STR(addr), auth_context, elems->security_profile_ie_len);

	/* Perform validation */
	if (!validate_sta_security_profile(
		    hapd, addr,
		    elems->rsn_ie, elems->rsn_ie_len,
		    elems->rsnxe, elems->rsnxe_len,
		    elems->security_profile_ie, elems->security_profile_ie_len,
		    profile_matched)) {
		wpa_printf(MSG_INFO,
			   "UHR: Rejecting %s auth from " MACSTR
			   " - Security Profile mismatch",
			   auth_context, MAC2STR(addr));
		return WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR: Security Profile validated for " MACSTR
		   " in %s auth",
		   MAC2STR(addr), auth_context);

	sta->sp_ie_validated = 1;

	/* UHR Security Profile validated successfully */
	if (sta->wpa_sm) {
		sta->wpa_sm->security_profile_indication = 1;
		wpa_printf(MSG_ERROR,
				   "UHR: STA " MACSTR
				   " profile validated, security_profile=1",
				   MAC2STR(sta->addr));
	}

	return WLAN_STATUS_SUCCESS;
}


u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 buf[100];
	size_t len;

	len = hostapd_supp_rates(hapd, buf);
	if (len == 0)
		return eid;
	/* Only up to first eight values in this element */
	if (len > 8)
		len = 8;

	*pos++ = WLAN_EID_SUPP_RATES;
	*pos++ = len;
	os_memcpy(pos, buf, len);
	pos += len;

	return pos;
}


u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 buf[100];
	size_t len;

	len = hostapd_supp_rates(hapd, buf);
	/* Starting from the 9th value for this element */
	if (len <= 8)
		return eid;

	*pos++ = WLAN_EID_EXT_SUPP_RATES;
	*pos++ = len - 8;
	os_memcpy(pos, &buf[8], len - 8);
	pos += len - 8;

	return pos;
}


u8 * hostapd_eid_rm_enabled_capab(struct hostapd_data *hapd, u8 *eid,
				  size_t len)
{
	size_t i;

	for (i = 0; i < RRM_CAPABILITIES_IE_LEN; i++) {
		if (hapd->conf->radio_measurements[i])
			break;
	}

	if (i == RRM_CAPABILITIES_IE_LEN || len < 2 + RRM_CAPABILITIES_IE_LEN)
		return eid;

	*eid++ = WLAN_EID_RRM_ENABLED_CAPABILITIES;
	*eid++ = RRM_CAPABILITIES_IE_LEN;
	os_memcpy(eid, hapd->conf->radio_measurements, RRM_CAPABILITIES_IE_LEN);

	return eid + RRM_CAPABILITIES_IE_LEN;
}


u16 hostapd_own_capab_info(struct hostapd_data *hapd)
{
	int capab = WLAN_CAPABILITY_ESS;
	int privacy = 0;
	int dfs;
	int i;

	/* Check if any of configured channels require DFS */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0) {
		wpa_printf(MSG_WARNING, "Failed to check if DFS is required; ret=%d",
			   dfs);
		dfs = 0;
	}

	if (hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		capab |= WLAN_CAPABILITY_SHORT_PREAMBLE;

#ifdef CONFIG_WEP
	privacy = hapd->conf->ssid.wep.keys_set;

	if (hapd->conf->ieee802_1x &&
	    (hapd->conf->default_wep_key_len ||
	     hapd->conf->individual_wep_key_len))
		privacy = 1;
#endif /* CONFIG_WEP */

	if (hapd->conf->wpa)
		privacy = 1;

	if (privacy)
		capab |= WLAN_CAPABILITY_PRIVACY;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hapd->iface->num_sta_no_short_slot_time == 0)
		capab |= WLAN_CAPABILITY_SHORT_SLOT_TIME;

	/*
    * Currently, Spectrum Management capability bit is set when directly
    * requested in configuration by spectrum_mgmt_required or when AP is
    * running on DFS channel.
    * TODO: Also consider driver support for TPC to set Spectrum Mgmt bit
	*/
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211A &&
	    (hapd->iconf->spectrum_mgmt_required || dfs ||
	     hapd->iconf->ieee80211h))
		capab |= WLAN_CAPABILITY_SPECTRUM_MGMT;

	for (i = 0; i < RRM_CAPABILITIES_IE_LEN; i++) {
		if (hapd->conf->radio_measurements[i]) {
			capab |= IEEE80211_CAP_RRM;
			break;
		}
	}

	return capab;
}


u16 hostapd_critical_update_capab(struct hostapd_data *hapd)
{
	int capab = 0;
	struct hostapd_data *bss;
	size_t i;

	if (!hapd)
		return capab;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && hapd->rx_cu_param.critical_flag)
		capab |= WLAN_CAPABILITY_PBCC;
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */

	if (hostapd_is_uhr_enabled(hapd) &&
	    hapd->rx_ecu_param.critical_update)
	       capab |= WLAN_CAPABILITY_ECU;

	if (hapd->iconf && hapd->iconf->mbssid) {
		for (i = 1; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];
#ifdef CONFIG_QCN_EXTN
			if (bss && hostapd_is_repurpose_disabled_11be_extn(bss->conf))
				continue;
#endif /* CONFIG_QCN_EXTN */
			if (bss && bss->conf->mld_ap && bss->rx_cu_param.critical_flag)
				capab |= WLAN_CAPABILITY_CHANNEL_AGILITY;
		}
	}
	return capab;
}


size_t hostapd_wfa_cap_ie_len(struct hostapd_data *hapd, struct sta_info *sta)
{
	bool scs_enabled = false;
	bool dscp_enabled = false;

#ifdef CONFIG_IEEE80211AX
	scs_enabled = hapd->conf->scs;
#endif /* CONFIG_IEEE80211AX */

	if (hapd->conf->enable_dscp_policy_capa)
		dscp_enabled = !sta || (sta && sta->dscp_policy_capable);

	if (scs_enabled || dscp_enabled)
		return WFA_IE_LEN;

	return 0;
}


u8 *hostapd_add_wfa_cap_ie(struct hostapd_data *hapd,
			    struct sta_info *sta,
			    u8 *eid)
{
	u8 cap = 0;
	bool scs_enabled = false;
	bool dscp_enabled = false;

#ifdef CONFIG_IEEE80211AX
	scs_enabled = hapd->conf->scs;
#endif /* CONFIG_IEEE80211AX */

	if (hapd->conf->enable_dscp_policy_capa)
		dscp_enabled = !sta || (sta && sta->dscp_policy_capable);

	if (!scs_enabled && !dscp_enabled)
		return eid;

	*eid++ = WLAN_EID_VENDOR_SPECIFIC;
	*eid++ = WFA_IE_LEN - 2;
	*eid++ = (OUI_WFA >> 16) & 0xFF;
	*eid++ = (OUI_WFA >> 8) & 0xFF;
	*eid++ = OUI_WFA & 0xFF;
	*eid++ = WFA_CAPA_OUI_TYPE;
	*eid++ = 1;

	if (scs_enabled)
		cap |= WFA_CAPA_QM_NON_EHT_SCS_TRAFFIC_DESC;

	if (dscp_enabled)
		cap |= WFA_CAPA_QM_DSCP_POLICY | WFA_CAPA_QM_UNSOLIC_DSCP;

	*eid++ = cap;
	return eid;
}


#ifdef CONFIG_WEP
#ifndef CONFIG_NO_RC4
static u16 auth_shared_key(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 auth_transaction, const u8 *challenge,
			   int iswep)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication (shared key, transaction %d)",
		       auth_transaction);

	if (auth_transaction == 1) {
		if (!sta->challenge) {
			/* Generate a pseudo-random challenge */
			u8 key[8];

			sta->challenge = os_zalloc(WLAN_AUTH_CHALLENGE_LEN);
			if (sta->challenge == NULL)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			if (os_get_random(key, sizeof(key)) < 0) {
				os_free(sta->challenge);
				sta->challenge = NULL;
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			}

			rc4_skip(key, sizeof(key), 0,
				 sta->challenge, WLAN_AUTH_CHALLENGE_LEN);
		}
		return 0;
	}

	if (auth_transaction != 3)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* Transaction 3 */
	if (!iswep || !sta->challenge || !challenge ||
	    os_memcmp_const(sta->challenge, challenge,
			    WLAN_AUTH_CHALLENGE_LEN)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "shared key authentication - invalid "
			       "challenge-response");
		return WLAN_STATUS_CHALLENGE_FAIL;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication OK (shared key)");
	sta->flags |= WLAN_STA_AUTH;
	wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
	os_free(sta->challenge);
	sta->challenge = NULL;

	return 0;
}
#endif /* CONFIG_NO_RC4 */
#endif /* CONFIG_WEP */

int send_auth_reply(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *dst,
			   u16 auth_alg, u16 auth_transaction, u16 resp,
			   const u8 *ies, size_t ies_len, const char *dbg)
{
	struct ieee80211_mgmt *reply;
	u8 *buf;
	size_t rlen;
	size_t tail_len = 0;
	size_t ml_len = 0;
	int reply_res = WLAN_STATUS_UNSPECIFIED_FAILURE;
	const u8 *sa = hapd->own_addr;
	struct wpabuf *ml_resp = NULL;
	struct wpabuf *smd_resp = NULL;
	size_t ml_resp_len = 0;
	size_t smd_resp_len = 0;
#ifdef CONFIG_IEEE8021X_AUTH
	size_t mic_len = 0;
#endif /* CONFIG_IEEE8021X_AUTH */

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) && sta->mld_auth) {
		ml_resp = hostapd_ml_auth_resp(hapd);
		if (!ml_resp)
			return -1;
		ml_resp_len = wpabuf_len(ml_resp);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
        /* Add SMD IE if both AP and STA support SMD */
        if (hapd->conf->smd.enabled && sta && sta->smd_info.smd_sta) {
                u8 smd_buf[SMD_IE_LEN];
                u8 *smd_end = hostapd_eid_smd(hapd, smd_buf);
                size_t smd_len = smd_end - smd_buf;

                if (smd_len > 0) {
                        smd_resp = wpabuf_alloc(smd_len);
                        if (!smd_resp) {
                                wpabuf_free(ml_resp);
                                return -1;
                        }
                        wpabuf_put_data(smd_resp, smd_buf, smd_len);
			smd_resp_len = wpabuf_len(smd_resp);
                }
        }
#endif /* CONFIG_IEEE80211BN */

	rlen = IEEE80211_HDRLEN + sizeof(reply->u.auth) + ies_len +
	       ml_resp_len + smd_resp_len;
#ifdef CONFIG_IEEE8021X_AUTH
	/* Add MIC element for an Authentication frame carrying an EAP-Success
	 * message and for an Authentication frame with transaction sequence
	 * frame 2, if PMKSA caching was used.
	 */
	if (auth_alg == WLAN_AUTH_802_1X &&
	    ((resp == WLAN_STATUS_802_1_X_AUTH_SUCCESS) ||
	     (sta && sta->eap_auth_data.add_mic))) {
		mic_len = wpa_mic_len(sta->eap_auth_data.akm,
				      sta->eap_auth_data.pmk_len,
				      RSN_HASH_NOT_SPECIFIED);
		rlen += 2 + mic_len;
	}
#endif /* CONFIG_IEEE8021X_AUTH */

#ifdef CONFIG_HOSTAPD_IF
	tail_len = hostapd_if_auth_reply_tail_len(sta, rlen);
#endif
	rlen += tail_len;

	buf = os_zalloc(rlen);
	if (!buf) {
		wpabuf_free(ml_resp);
		wpabuf_free(smd_resp);
		return -1;
	}

	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					    WLAN_FC_STYPE_AUTH);
	os_memcpy(reply->da, dst, ETH_ALEN);
	os_memcpy(reply->sa, sa, ETH_ALEN);
	os_memcpy(reply->bssid, sa, ETH_ALEN);

	reply->u.auth.auth_alg = host_to_le16(auth_alg);
	reply->u.auth.auth_transaction = host_to_le16(auth_transaction);
	reply->u.auth.status_code = host_to_le16(resp);

	if (ies && ies_len)
		os_memcpy(reply->u.auth.variable, ies, ies_len);
#ifdef CONFIG_IEEE80211BE
	if (ml_resp) {
		ml_len = wpabuf_len(ml_resp);
		os_memcpy(reply->u.auth.variable + ies_len,
				wpabuf_head(ml_resp), ml_len);
#ifdef CONFIG_IEEE80211BN
		/* SMD requires ML as mandatory */
		if (smd_resp)
			os_memcpy(reply->u.auth.variable + ies_len + wpabuf_len(ml_resp),
				  wpabuf_head(smd_resp), wpabuf_len(smd_resp));
#endif /* CONFIG_IEEE80211BN */
	}
#endif /* CONFIG_IEEE80211BE */

        wpabuf_free(ml_resp);
        wpabuf_free(smd_resp);

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_auth_reply_add_tail(sta, ies_len + ml_len, tail_len, reply);
#endif

	wpa_printf(MSG_DEBUG, "authentication reply: STA=" MACSTR
		   " auth_alg=%d auth_transaction=%d resp=%d (IE len=%lu) (dbg=%s)",
		   MAC2STR(dst), auth_alg, auth_transaction,
		   resp, (unsigned long) ies_len, dbg);
#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_SAE
	if (hapd->conf->sae_confirm_immediate == 2 &&
	    auth_alg == WLAN_AUTH_SAE) {
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT && sta &&
		    (resp == WLAN_STATUS_SUCCESS ||
		     resp == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		     resp == WLAN_STATUS_SAE_PK)) {
			wpa_printf(MSG_DEBUG,
				   "TESTING: Postpone SAE Commit transmission until Confirm is ready");
			os_free(sta->sae_postponed_commit);
			sta->sae_postponed_commit = buf;
			sta->sae_postponed_commit_len = rlen;
			return WLAN_STATUS_SUCCESS;
		}

		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM &&
		    sta && sta->sae_postponed_commit) {
			wpa_printf(MSG_DEBUG,
				   "TESTING: Send postponed SAE Commit first, immediately followed by SAE Confirm");
			if (hostapd_drv_send_mlme(hapd,
						  sta->sae_postponed_commit,
						  sta->sae_postponed_commit_len,
						  0, NULL, 0, 0, 0, 0) < 0)
				wpa_printf(MSG_INFO, "send_auth_reply: send failed");
			os_free(sta->sae_postponed_commit);
			sta->sae_postponed_commit = NULL;
			sta->sae_postponed_commit_len = 0;
		}
	}
#endif /* CONFIG_SAE */
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE8021X_AUTH
	if (auth_alg == WLAN_AUTH_802_1X &&
	    ((resp == WLAN_STATUS_802_1_X_AUTH_SUCCESS) ||
	     (sta && sta->eap_auth_data.add_mic))) {
		const u8 *frame, *data, *rsne, *rsnxe;
		u8 data_buf[500], mic[WPA_1X_MAX_MIC_LEN];
		size_t frame_len, data_len;
		const u8 *aa = sa;
		u8 *ptr = reply->u.auth.variable + ies_len + ml_resp_len;

#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta))
			aa = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

		rsne = hostapd_wpa_ie(hapd, WLAN_EID_RSN);
		if (!rsne) {
			wpa_printf(MSG_INFO, "No AP RSNE");
			return -1;
		}
		os_memcpy(data_buf, rsne, 2 + rsne[1]);
		data_len = 2 + rsne[1];

		rsnxe = hostapd_wpa_ie(hapd, WLAN_EID_RSNX);
		if (rsnxe) {
			wpa_printf(MSG_DEBUG, "Found AP RSNXE");
			os_memcpy(&data_buf[data_len], rsnxe, 2 + rsnxe[1]);
			data_len += 2 + rsnxe[1];
		} else {
			wpa_printf(MSG_DEBUG, "No AP RSNXE");
		}
		data = data_buf;

		/* MIC element */
		ptr[0] = WLAN_EID_MIC;
		ptr[1] = mic_len;
		os_memset(ptr + 2, 0, mic_len);

		frame = (const u8 *) &reply->u.auth.auth_alg;
		frame_len =  rlen - IEEE80211_HDRLEN;
		if (wpa_auth_8021x_mic(sta->eap_auth_data.akm,
				       sta->eap_auth_data.ptk.kck,
				       sta->eap_auth_data.ptk.kck_len,
				       aa, sta->addr, data, data_len,
				       frame, frame_len, mic)) {
			wpa_printf(MSG_INFO, "Failed to derive MIC");
			return -1;
		}
		os_memcpy(ptr + 2, mic, mic_len);
	}
#endif /* CONFIF_IEEE8021X_AUTH */

	if (hostapd_drv_send_mlme(hapd, reply, rlen, 0, NULL, 0, 0, 0, 0) < 0)
		wpa_printf(MSG_INFO, "send_auth_reply: send failed");
	else
		reply_res = WLAN_STATUS_SUCCESS;

	os_free(buf);

#ifdef CONFIG_QCN_EXTN
	if (resp != WLAN_STATUS_SUCCESS && sta) {
		wpa_printf(MSG_DEBUG, "auth_reject: STA " MACSTR " status=%u",
			   MAC2STR(sta->addr), resp);
		hostapd_log_trigger_emit(hapd, sta->addr,
					 HOSTAPD_LOG_TRIG_AUTH_REJECT);
	}
#endif /* CONFIG_QCN_EXTN */

	return reply_res;
}

#ifdef CONFIG_IEEE8021X_AUTH
static void send_8021x_auth_reply(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  u16 auth_transaction, u16 resp,
				  struct wpabuf *ies)
{
	const u8 *dst = sta->addr;

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211BE)
	if (ap_sta_is_mld(hapd, sta))
		dst = sta->reply_addr;
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211BE */

	send_auth_reply(hapd, sta, dst, WLAN_AUTH_802_1X,
			auth_transaction, resp, wpabuf_head(ies),
			wpabuf_len(ies), "send-8021x-auth-reply");

	if (sta->added_unassoc && (resp != WLAN_STATUS_SUCCESS &&
				   resp != WLAN_STATUS_802_1_X_AUTH_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
	wpabuf_free(ies);
}
#endif /* IEEE8021X_AUTH */


#ifdef CONFIG_IEEE80211R_AP
static void handle_auth_ft_finish(void *ctx, const u8 *dst,
				  u16 auth_transaction, u16 status,
				  const u8 *ies, size_t ies_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int reply_res;

	sta = ap_get_sta(hapd, dst);
	if (!sta) {
		reply_res = send_auth_reply(hapd, NULL, dst, WLAN_AUTH_FT,
					    auth_transaction, status, ies, ies_len,
					    "auth-ft-finish");
		return;
	}

	reply_res = send_auth_reply(hapd, NULL, sta->mld_auth ? sta->reply_addr : dst,
				    WLAN_AUTH_FT, auth_transaction, status, ies, ies_len,
				    "auth-ft-finish");


	if (sta->added_unassoc && (reply_res != WLAN_STATUS_SUCCESS ||
				   status != WLAN_STATUS_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
		return;
	}

	if (status != WLAN_STATUS_SUCCESS)
		return;

	hostapd_logger(hapd, dst, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "authentication OK (FT)");
	sta->flags |= WLAN_STA_AUTH;
	mlme_authenticate_indication(hapd, sta);
}
#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_SAE

static void sae_set_state(struct sta_info *sta, enum sae_state state,
			  const char *reason)
{
	wpa_printf(MSG_DEBUG, "SAE: State %s -> %s for peer " MACSTR " (%s)",
		   sae_state_txt(sta->sae->state), sae_state_txt(state),
		   MAC2STR(sta->addr), reason);
	sta->sae->state = state;
}


static bool in_mac_addr_list(const u8 *list, unsigned int num, const u8 *addr)
{
	unsigned int i;

	for (i = 0; list && i < num; i++) {
		if (ether_addr_equal(&list[i * ETH_ALEN], addr))
			return true;
	}

	return false;
}


static struct sae_password_entry *
sae_password_find_pw(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sae_password_entry *pw = NULL;

	if (!sta->sae || !sta->sae->tmp || !sta->sae->tmp->used_pw)
		return NULL;


	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (pw == sta->sae->tmp->used_pw)
			return pw;
	}

	return NULL;
}


static bool is_other_sae_password(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  struct sae_password_entry *used_pw)
{
	struct sae_password_entry *pw;

	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (pw == used_pw ||
		    pw->identifier ||
		    !is_broadcast_ether_addr(pw->peer_addr))
			continue;

		if (in_mac_addr_list(pw->success_mac,
				     pw->num_success_mac,
				     sta->addr))
			return true;

		if (!in_mac_addr_list(pw->fail_mac, pw->num_fail_mac,
				      sta->addr))
			return true;
	}

	return false;
}


static bool has_sae_success_seen(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct sae_password_entry *pw;

	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (pw->identifier ||
		    !is_broadcast_ether_addr(pw->peer_addr))
			continue;

		if (in_mac_addr_list(pw->success_mac,
				     pw->num_success_mac,
				     sta->addr))
			return true;
	}

	return false;
}


static int sae_password_mark_success(struct hostapd_data *hapd,
				      struct sae_password_entry *pw,
				      const u8 *addr)
{
	if (in_mac_addr_list(pw->success_mac, pw->num_success_mac, addr))
		return 0;

	if (!pw->success_mac) {
		pw->success_mac = os_zalloc(hapd->conf->sae_track_password *
					    ETH_ALEN);
		if (!pw->success_mac)
			return -1;
		pw->num_success_mac = hapd->conf->sae_track_password;
	}

	os_memcpy(&pw->success_mac[pw->next_success_mac * ETH_ALEN], addr,
		  ETH_ALEN);
	pw->next_success_mac = (pw->next_success_mac + 1) % pw->num_success_mac;
	return 0;
}


static void sae_password_track_success(struct hostapd_data *hapd,
				       struct sta_info *sta)
{
	struct sae_password_entry *pw;

	if (!hapd->conf->sae_track_password)
		return;

	pw = sae_password_find_pw(hapd, sta);
	if (!pw)
		return;

	sae_password_mark_success(hapd, pw, sta->addr);
}


static bool sae_password_track_fail(struct hostapd_data *hapd,
				    struct sta_info *sta)
{
	struct sae_password_entry *pw;

	if (!hapd->conf->sae_track_password)
		return false;

	pw = sae_password_find_pw(hapd, sta);
	if (!pw)
		return false;

	if (in_mac_addr_list(pw->fail_mac,
			     pw->num_fail_mac,
			     sta->addr))
		return is_other_sae_password(hapd, sta, pw);

	if (!pw->fail_mac) {
		pw->fail_mac = os_zalloc(hapd->conf->sae_track_password *
					 ETH_ALEN);
		if (!pw->fail_mac)
			return false;
		pw->num_fail_mac = hapd->conf->sae_track_password;
	}

	os_memcpy(&pw->fail_mac[pw->next_fail_mac * ETH_ALEN], sta->addr,
		  ETH_ALEN);
	pw->next_fail_mac = (pw->next_fail_mac + 1) % pw->num_fail_mac;

	return is_other_sae_password(hapd, sta, pw);
}


int sae_password_bind(struct hostapd_data *hapd, const u8 *addr,
		      const char *password)
{
	struct sae_password_entry *pw;

	if (!hapd->conf->sae_track_password)
		return -1;

	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (pw->identifier ||
		    !is_broadcast_ether_addr(pw->peer_addr) ||
		    os_strcmp(password, pw->password) != 0)
			continue;

		return sae_password_mark_success(hapd, pw, addr);
	}

	return -1;
}


const char * sae_get_password(struct hostapd_data *hapd,
			      struct sta_info *sta,
			      const u8 *rx_id, size_t rx_id_len,
			      struct sae_password_entry **pw_entry,
			      struct sae_pt **s_pt,
			      const struct sae_pk **s_pk)
{
	const char *password = NULL;
	struct sae_password_entry *pw;
	struct sae_pt *pt = NULL;
	const struct sae_pk *pk = NULL;
	struct hostapd_sta_wpa_psk_short *psk = NULL;

	/* With sae_track_password functionality enabled, try to first find the
	 * next viable wildcard-address password if a password identifier was
	 * not used. Select an wildcard-addr entry if the STA is known to have
	 * used it successfully before. If no such entry exists, pick a
	 * wildcard-addr entry that does not have a failed entry tracked for the
	 * STA. */
	if (!rx_id && sta && hapd->conf->sae_track_password) {
		struct sae_password_entry *success = NULL, *no_fail = NULL;

		for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
			if (pw->identifier ||
			    !is_broadcast_ether_addr(pw->peer_addr))
				continue;
			if (in_mac_addr_list(pw->success_mac,
					     pw->num_success_mac,
					     sta->addr)) {
				success = pw;
				break;
			}

			if (!no_fail &&
			    !in_mac_addr_list(pw->fail_mac, pw->num_fail_mac,
					      sta->addr))
				no_fail = pw;
		}

		pw = success ? success : no_fail;
		if (pw) {
			password = pw->password;
			pt = pw->pt;
			if (!(hapd->conf->mesh & MESH_ENABLED))
				pk = pw->pk;
			goto found;
		}
	}

	/* If sae_track_password functionality is not enabled or no suitable
	 * password entry was found with it, pick the first entry that matches
	 * the STA MAC address and password identifier (if used). */
	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (!is_broadcast_ether_addr(pw->peer_addr) &&
		    (!sta ||
		     !ether_addr_equal(pw->peer_addr, sta->addr)))
			continue;
		if ((rx_id && !pw->identifier) || (!rx_id && pw->identifier))
			continue;
		if (rx_id && pw->identifier &&
		    (rx_id_len != os_strlen(pw->identifier) ||
		     os_memcmp(rx_id, pw->identifier, rx_id_len) != 0))
			continue;
		password = pw->password;
		pt = pw->pt;
		if (!(hapd->conf->mesh & MESH_ENABLED))
			pk = pw->pk;
		break;
	}
	if (!password && !rx_id && !hapd->conf->sae_password_psk) {
		password = hapd->conf->ssid.wpa_passphrase;
		pt = hapd->conf->ssid.pt;
	}

	if (!password && sta && !rx_id) {
		for (psk = sta->psk; psk; psk = psk->next) {
			if (psk->is_passphrase) {
				password = psk->passphrase;
				break;
			}
		}
	}

	/* Try to decrypt the received password identifier if no plaintext
	 * identifier match was found. */
	if (!password && rx_id && rx_id_len > 4 + 4 + AES_BLOCK_SIZE &&
	    hapd->conf->sae_pw_id_key) {
		u8 *plain, *pos, *counter;
		size_t plain_len;
		const u8 *id;
		size_t id_len;

		plain = os_malloc(rx_id_len);
		if (!plain)
			goto fail;
		if (aes_siv_decrypt(
			    wpabuf_head(hapd->conf->sae_pw_id_key),
			    wpabuf_len(hapd->conf->sae_pw_id_key),
			    rx_id, rx_id_len, 0, NULL, NULL, plain) < 0)
			goto fail;
		plain_len = rx_id_len - AES_BLOCK_SIZE;
		wpa_hexdump_ascii(MSG_DEBUG,
				  "SAE: Decrypted password identifier info",
				  plain, plain_len);
		/* 4 octet date | Password ID | <padding> | 4 octet counter */
		counter = plain + plain_len - 4;
		wpa_printf(MSG_DEBUG, "SAE: Generation time %u counter %u",
			   WPA_GET_BE32(plain), WPA_GET_BE32(counter));
		id = pos = plain + 4;
		while (pos < counter) {
			if (*pos == 0x00)
				break;
			pos++;
		}
		id_len = pos - id;
		wpa_hexdump_ascii(MSG_DEBUG,
				  "SAE: Decrypted password identifier",
				  id, id_len);
		for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
			if (!is_broadcast_ether_addr(pw->peer_addr) &&
			    (!sta ||
			     !ether_addr_equal(pw->peer_addr, sta->addr)))
				continue;
			if (!pw->identifier ||
			    os_strlen(pw->identifier) != id_len ||
			    os_memcmp(id, pw->identifier, id_len) != 0)
				continue;
			password = pw->password;
			if (!(hapd->conf->mesh & MESH_ENABLED))
				pk = pw->pk;
			if (sta && sta->sae && sta->sae->tmp) {
				os_free(sta->sae->tmp->dec_pw_id);
				sta->sae->tmp->dec_pw_id =
					os_zalloc(id_len + 1);
				if (sta->sae->tmp->dec_pw_id) {
					os_memcpy(sta->sae->tmp->dec_pw_id,
						  id, id_len);
					sta->sae->tmp->dec_pw_id_len = id_len;
					sta->sae->tmp->pw_id_counter =
						WPA_GET_BE32(counter);
					wpa_printf(MSG_DEBUG,
						   "SAE: Bound decrypted password identifier to STA");
				}
			}
			break;
		}
	fail:
		os_free(plain);
	}

found:
	if (pw_entry)
		*pw_entry = pw;
	if (s_pt)
		*s_pt = pt;
	if (s_pk)
		*s_pk = pk;

	return password;
}


static struct wpabuf * auth_build_sae_commit(struct hostapd_data *hapd,
					     struct sta_info *sta, int update,
					     int status_code)
{
	struct wpabuf *buf;
	const char *password = NULL;
	struct sae_password_entry *pw;
	const u8 *rx_id = NULL;
	size_t rx_id_len = 0;
	int use_pt = 0;
	struct sae_pt *pt = NULL;
	const struct sae_pk *pk = NULL;
	const u8 *own_addr = hapd->own_addr;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) && sta->mld_auth)
		own_addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	if (sta->sae->tmp) {
		rx_id = sta->sae->tmp->parsed_pw_id ?
			sta->sae->tmp->parsed_pw_id : sta->sae->tmp->pw_id;
		rx_id_len = sta->sae->tmp->parsed_pw_id ?
			sta->sae->tmp->parsed_pw_id_len :
			sta->sae->tmp->pw_id_len;
		use_pt = sta->sae->h2e;
#ifdef CONFIG_SAE_PK
		os_memcpy(sta->sae->tmp->own_addr, own_addr, ETH_ALEN);
		os_memcpy(sta->sae->tmp->peer_addr, sta->addr, ETH_ALEN);
#endif /* CONFIG_SAE_PK */
	}

	if (rx_id && hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		use_pt = 1;
	else if (status_code == WLAN_STATUS_SUCCESS)
		use_pt = 0;
	else if (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		 status_code == WLAN_STATUS_SAE_PK)
		use_pt = 1;

	password = sae_get_password(hapd, sta, rx_id, rx_id_len, &pw, &pt, &pk);
	if (!password) {
		wpa_printf(MSG_DEBUG, "SAE: No password available");
		return NULL;
	}

	if (use_pt) {
		struct sae_pt *tmp_pt = NULL;
		bool failed = false;

		if (!pt && pw) {
			int groups[2] = { sta->sae->group, 0 };

			wpa_printf(MSG_DEBUG,
				   "SAE: Derive PT for encrypted PW ID");
			tmp_pt = sae_derive_pt(groups, hapd->conf->ssid.ssid,
					       hapd->conf->ssid.ssid_len,
					       (const u8 *) pw->password,
					       os_strlen(pw->password),
					       rx_id, rx_id_len);
			if (!tmp_pt) {
				wpa_printf(MSG_DEBUG,
					   "SAE: Could not derive PT");
				return NULL;
			}
			pt = tmp_pt;
			update = 1;
		}

		if (!pt) {
			wpa_printf(MSG_DEBUG, "SAE: No PT available");
			return NULL;
		}

		if (update &&
		    sae_prepare_commit_pt(sta->sae, pt, own_addr, sta->addr,
					  NULL, pk) < 0)
			failed = true;

		sae_deinit_pt(tmp_pt);
		if (failed)
			return NULL;
	}

	if (update && !use_pt &&
	    sae_prepare_commit(own_addr, sta->addr,
			       (u8 *) password, os_strlen(password),
			       sta->sae) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not pick PWE");
		return NULL;
	}

	if (pw && sta->sae->tmp)
		sta->sae->tmp->used_pw = pw;

	if (pw && pw->vlan_id) {
		if (!sta->sae->tmp) {
			wpa_printf(MSG_INFO,
				   "SAE: No temporary data allocated - cannot store VLAN ID");
			return NULL;
		}
		sta->sae->tmp->vlan_id = pw->vlan_id;
	}

	buf = wpabuf_alloc(SAE_COMMIT_MAX_LEN +
			   (rx_id ? 3 + rx_id_len : 0));
	if (buf &&
	    sae_write_commit(sta->sae, buf, sta->sae->tmp ?
			     sta->sae->tmp->anti_clogging_token : NULL,
			     rx_id, rx_id_len) < 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


static struct wpabuf * auth_build_sae_confirm(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(SAE_CONFIRM_MAX_LEN);
	if (buf == NULL)
		return NULL;

#ifdef CONFIG_SAE_PK
#ifdef CONFIG_TESTING_OPTIONS
	if (sta->sae->tmp)
		sta->sae->tmp->omit_pk_elem = hapd->conf->sae_pk_omit;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_SAE_PK */

	if (sae_write_confirm(sta->sae, buf) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}


static int auth_sae_send_commit(struct hostapd_data *hapd,
				struct sta_info *sta,
				int update, int status_code)
{
	struct wpabuf *data;
	int reply_res;
	u16 status;
	u8 *dst = sta->addr;
#ifdef CONFIG_IEEE80211BE
	u8 link_id = hapd->mld_link_id;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && sta && sta->mld_info.mld_sta && sta->mld_auth) {
		dst = sta->reply_addr;
		/*
		 * dst address should be partner link peer address,
		 * if receive auth frame from sta partner link with same mld address
		 * before disconnect with assoc link,
		 */
		if (sta->unadded_sta)
			dst = sta->mld_info.links[link_id].peer_addr;
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */

#endif /* CONFIG_IEEE80211BE */

	data = auth_build_sae_commit(hapd, sta, update, status_code);
	if (!data && sta->sae->tmp &&
	    (sta->sae->tmp->pw_id || sta->sae->tmp->parsed_pw_id))
		return WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER;
	if (data == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (sta->sae->tmp && sta->sae->pk)
		status = WLAN_STATUS_SAE_PK;
	else if (sta->sae->tmp && sta->sae->h2e)
		status = WLAN_STATUS_SAE_HASH_TO_ELEMENT;
	else
		status = WLAN_STATUS_SUCCESS;
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->sae_commit_status >= 0 &&
	    hapd->conf->sae_commit_status != status) {
		wpa_printf(MSG_INFO,
			   "TESTING: Override SAE commit status code %u --> %d",
			   status, hapd->conf->sae_commit_status);
		status = hapd->conf->sae_commit_status;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	reply_res = send_auth_reply(hapd, sta, dst,
				    WLAN_AUTH_SAE, 1,
				    status, wpabuf_head(data),
				    wpabuf_len(data), "sae-send-commit");

	wpabuf_free(data);

	return reply_res;
}


static int auth_sae_send_confirm(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct wpabuf *data;
	int reply_res;
	u8 *dst = sta->addr;

#ifdef CONFIG_IEEE80211BE
	u8 link_id = hapd->mld_link_id;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && sta && sta->mld_info.mld_sta && sta->mld_auth) {
		dst = sta->reply_addr;
		if (sta->unadded_sta) {

			/*
			 * dst address should be partner link peer address,
			 * if receive auth frame from sta partner link with same mld address
			 * before disconnect with assoc link,
			 */
			dst = sta->mld_info.links[link_id].peer_addr;
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	data = auth_build_sae_confirm(hapd, sta);
	if (data == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	reply_res = send_auth_reply(hapd, sta, dst,
				    WLAN_AUTH_SAE, 2,
				    WLAN_STATUS_SUCCESS, wpabuf_head(data),
				    wpabuf_len(data), "sae-send-confirm");

	wpabuf_free(data);

	return reply_res;
}

#endif /* CONFIG_SAE */


#if defined(CONFIG_SAE) || defined(CONFIG_PASN)

static int use_anti_clogging(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	unsigned int open = 0;

	if (hapd->conf->anti_clogging_threshold == 0)
		return 1;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
#ifdef CONFIG_SAE
		if (sta->sae &&
		    (sta->sae->state == SAE_COMMITTED ||
		     sta->sae->state == SAE_CONFIRMED))
			open++;
#endif /* CONFIG_SAE */
#ifdef CONFIG_PASN
		if (sta->pasn && sta->pasn->ecdh)
			open++;
#endif /* CONFIG_PASN */
		if (open >= hapd->conf->anti_clogging_threshold)
			return 1;
	}

#ifdef CONFIG_SAE
	/* In addition to already existing open SAE sessions, check whether
	 * there are enough pending commit messages in the processing queue to
	 * potentially result in too many open sessions. */
	if (open + dl_list_len(&hapd->sae_commit_queue) >=
	    hapd->conf->anti_clogging_threshold)
		return 1;
#endif /* CONFIG_SAE */

	return 0;
}

#endif /* defined(CONFIG_SAE) || defined(CONFIG_PASN) */


#ifdef CONFIG_SAE

static int sae_check_big_sync(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->sae->sync > hapd->conf->sae_sync) {
		sae_set_state(sta, SAE_NOTHING, "Sync > dot11RSNASAESync");
		sta->sae->sync = 0;
		if (sta->sae->tmp) {
			/* Disable this SAE instance for 10 seconds to avoid
			 * unnecessary flood of multiple SAE commits in
			 * unexpected mesh cases. */
			if (os_get_reltime(&sta->sae->tmp->disabled_until) == 0)
				sta->sae->tmp->disabled_until.sec += 10;
		}
		return -1;
	}
	return 0;
}


static bool sae_proto_instance_disabled(struct sta_info *sta)
{
	struct sae_temporary_data *tmp;

	if (!sta->sae)
		return false;
	tmp = sta->sae->tmp;
	if (!tmp)
		return false;

	if (os_reltime_initialized(&tmp->disabled_until)) {
		struct os_reltime now;

		os_get_reltime(&now);
		if (os_reltime_before(&now, &tmp->disabled_until))
			return true;
	}

	return false;
}


static void auth_sae_retransmit_timer(void *eloop_ctx, void *eloop_data)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = eloop_data;
	int ret;

	if (sae_check_big_sync(hapd, sta))
		return;
	sta->sae->sync++;
	wpa_printf(MSG_DEBUG, "SAE: Auth SAE retransmit timer for " MACSTR
		   " (sync=%d state=%s)",
		   MAC2STR(sta->addr), sta->sae->sync,
		   sae_state_txt(sta->sae->state));

	switch (sta->sae->state) {
	case SAE_COMMITTED:
		ret = auth_sae_send_commit(hapd, sta, 0, -1);
		eloop_register_timeout(0,
				       hapd->dot11RSNASAERetransPeriod * 1000,
				       auth_sae_retransmit_timer, hapd, sta);
		break;
	case SAE_CONFIRMED:
		ret = auth_sae_send_confirm(hapd, sta);
		eloop_register_timeout(0,
				       hapd->dot11RSNASAERetransPeriod * 1000,
				       auth_sae_retransmit_timer, hapd, sta);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret != WLAN_STATUS_SUCCESS)
		wpa_printf(MSG_INFO, "SAE: Failed to retransmit: ret=%d", ret);
}


void sae_clear_retransmit_timer(struct hostapd_data *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(auth_sae_retransmit_timer, hapd, sta);
}


static void sae_set_retransmit_timer(struct hostapd_data *hapd,
				     struct sta_info *sta)
{
	if (!(hapd->conf->mesh & MESH_ENABLED))
		return;

	eloop_cancel_timeout(auth_sae_retransmit_timer, hapd, sta);
	eloop_register_timeout(0, hapd->dot11RSNASAERetransPeriod * 1000,
			       auth_sae_retransmit_timer, hapd, sta);
}

void sae_sme_send_external_auth_status(struct hostapd_data *hapd,
				       struct sta_info *sta, u16 status)
{
	struct external_auth params;

	os_memset(&params, 0, sizeof(params));
	params.status = status;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		params.bssid =
			sta->mld_info.links[sta->mld_assoc_link_id].peer_addr;
#endif /* CONFIG_IEEE80211BE */
	if (!params.bssid)
		params.bssid = sta->addr;

	if (status == WLAN_STATUS_SUCCESS && sta->sae &&
	    !hapd->conf->disable_pmksa_caching)
		params.pmkid = sta->sae->pmkid;

	hostapd_drv_send_external_auth_status(hapd, &params);
}


static int sae_assign_vlan(struct hostapd_data *hapd, struct sta_info *sta,
			   int vlan_id)
{
#ifndef CONFIG_NO_VLAN
	struct vlan_description vlan_desc;

	if (vlan_id > 0) {
		wpa_printf(MSG_DEBUG, "SAE: Assign STA " MACSTR
			   " to VLAN ID %d",
			   MAC2STR(sta->addr), vlan_id);

		if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
			os_memset(&vlan_desc, 0, sizeof(vlan_desc));
			vlan_desc.notempty = 1;
			vlan_desc.untagged = vlan_id;
			if (!hostapd_vlan_valid(hapd->conf->vlan, &vlan_desc)) {
				wpa_printf(MSG_INFO,
					   "Invalid VLAN ID %d in sae_password",
					   vlan_id);
				return -1;
			}

			if (ap_sta_set_vlan(hapd, sta, &vlan_desc) < 0 ||
			    ap_sta_bind_vlan(hapd, sta) < 0) {
				wpa_printf(MSG_INFO,
					   "Failed to assign VLAN ID %d from sae_password to "
					   MACSTR, vlan_id,
					   MAC2STR(sta->addr));
				return -1;
			}
		} else {
			sta->vlan_id = vlan_id;
		}
	}
#endif /* CONFIG_NO_VLAN */

	return 0;
}


void sae_accept_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->sae->tmp &&
	    sae_assign_vlan(hapd, sta, sta->sae->tmp->vlan_id) < 0)
		return;

	sta->flags |= WLAN_STA_AUTH;
	sta->auth_alg = WLAN_AUTH_SAE;
	mlme_authenticate_indication(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
	sae_set_state(sta, SAE_ACCEPTED, "Accept Confirm");
	crypto_bignum_deinit(sta->sae->peer_commit_scalar_accepted, 0);
	sta->sae->peer_commit_scalar_accepted = sta->sae->peer_commit_scalar;
	sta->sae->peer_commit_scalar = NULL;
	wpa_auth_pmksa_add_sae(hapd->wpa_auth, sta->addr,
			       sta->sae->pmk, sta->sae->pmk_len,
			       sta->sae->pmkid, sta->sae->akmp,
			       ap_sta_is_mld(hapd, sta), sta->vlan_id);
	sae_sme_send_external_auth_status(hapd, sta, WLAN_STATUS_SUCCESS);
	if (sta->sae->tmp) {
		struct sae_temporary_data *tmp = sta->sae->tmp;

		wpabuf_free(sta->sae_pw_id);
		sta->sae_pw_id = NULL;
		if (tmp->dec_pw_id) {
			sta->sae_pw_id = wpabuf_alloc_copy(
				tmp->dec_pw_id, tmp->dec_pw_id_len);
			sta->sae_pw_id_counter = tmp->pw_id_counter;
		} else if (tmp->pw_id) {
			sta->sae_pw_id = wpabuf_alloc_copy(
				tmp->pw_id, tmp->pw_id_len);
		}
	}
}


int sae_sm_step(struct hostapd_data *hapd, struct sta_info *sta,
		       u16 auth_transaction, u16 status_code,
		       int allow_reuse, int *sta_removed)
{
	int ret;

	*sta_removed = 0;

	if (auth_transaction != WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
	    auth_transaction != WLAN_AUTH_TR_SEQ_SAE_CONFIRM)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	wpa_printf(MSG_DEBUG, "SAE: Peer " MACSTR " state=%s auth_trans=%u",
		   MAC2STR(sta->addr), sae_state_txt(sta->sae->state),
		   auth_transaction);

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
	    sae_proto_instance_disabled(sta)) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Protocol instance temporarily disabled - discard received SAE commit");
		return WLAN_STATUS_SUCCESS;
	}

	switch (sta->sae->state) {
	case SAE_NOTHING:
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
			struct sae_temporary_data *tmp = sta->sae->tmp;
			bool immediate_confirm;

			if (tmp) {
				sta->sae->h2e =
					(status_code ==
					 WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
					 status_code == WLAN_STATUS_SAE_PK);
				sta->sae->pk =
					status_code == WLAN_STATUS_SAE_PK;
			}
			ret = auth_sae_send_commit(hapd, sta,
						   !allow_reuse, status_code);
			if (ret == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER)
				wpa_msg(hapd->msg_ctx, MSG_INFO,
					WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER
					MACSTR, MAC2STR(sta->addr));
			if (ret)
				return ret;

			if (tmp && tmp->parsed_pw_id && !tmp->pw_id) {
				tmp->pw_id = tmp->parsed_pw_id;
				tmp->pw_id_len = tmp->parsed_pw_id_len;
				tmp->parsed_pw_id = NULL;
				tmp->parsed_pw_id_len = 0;
				wpa_hexdump_ascii(MSG_DEBUG,
						  "SAE: Known Password Identifier bound to this STA",
						  tmp->pw_id, tmp->pw_id_len);
			}

			sae_set_state(sta, SAE_COMMITTED, "Sent Commit");

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			/*
			 * In mesh case, both Commit and Confirm are sent
			 * immediately. In infrastructure BSS, by default, only
			 * a single Authentication frame (Commit) is expected
			 * from the AP here and the second one (Confirm) will
			 * be sent once the STA has sent its second
			 * Authentication frame (Confirm). This behavior can be
			 * overridden with explicit configuration so that the
			 * infrastructure BSS case sends both frames together.
			 */
			immediate_confirm = (hapd->conf->mesh & MESH_ENABLED) ||
				hapd->conf->sae_confirm_immediate;

			/* If sae_track_password is enabled and the STA has not
			 * yet been tracked to having successfully completed
			 * SAE authentication with the password that the AP
			 * tries to use, do not send Confirm immediately to
			 * avoid an explicit indication on the STA side on
			 * password mismatch. */
			if (immediate_confirm &&
			    hapd->conf->sae_track_password &&
			    (!sta->sae->tmp || !sta->sae->tmp->parsed_pw_id) &&
			    !has_sae_success_seen(hapd, sta))
				immediate_confirm = false;

			if (immediate_confirm) {
				/*
				 * Send both Commit and Confirm immediately
				 * based on SAE finite state machine
				 * Nothing -> Confirm transition.
				 */
				ret = auth_sae_send_confirm(hapd, sta);
				if (ret)
					return ret;
				sae_set_state(sta, SAE_CONFIRMED,
					      "Sent Confirm (mesh)");
			} else {
				/*
				 * For infrastructure BSS, send only the Commit
				 * message now to get alternating sequence of
				 * Authentication frames between the AP and STA.
				 * Confirm will be sent in
				 * Committed -> Confirmed/Accepted transition
				 * when receiving Confirm from STA.
				 */
			}
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "SAE confirm before commit");
		}
		break;
	case SAE_COMMITTED:
		sae_clear_retransmit_timer(hapd, sta);
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;
			sae_set_state(sta, SAE_CONFIRMED, "Sent Confirm");
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else if (hapd->conf->mesh & MESH_ENABLED) {
			/*
			 * In mesh case, follow SAE finite state machine and
			 * send Commit now, if sync count allows.
			 */
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_commit(hapd, sta, 0, status_code);
			if (ret)
				return ret;

			sae_set_retransmit_timer(hapd, sta);
		} else {
			/*
			 * For instructure BSS, send the postponed Confirm from
			 * Nothing -> Confirmed transition that was reduced to
			 * Nothing -> Committed above.
			 */
			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;

			sae_set_state(sta, SAE_CONFIRMED, "Sent Confirm");

			/*
			 * Since this was triggered on Confirm RX, run another
			 * step to get to Accepted without waiting for
			 * additional events.
			 */
			return sae_sm_step(hapd, sta, auth_transaction,
					   WLAN_STATUS_SUCCESS, 0, sta_removed);
		}
		break;
	case SAE_CONFIRMED:
		sae_clear_retransmit_timer(hapd, sta);
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_commit(hapd, sta, 1, status_code);
			if (ret)
				return ret;

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;

			sae_set_retransmit_timer(hapd, sta);
		} else {
			sta->sae->send_confirm = 0xffff;
			sae_accept_sta(hapd, sta);
		}
		break;
	case SAE_ACCEPTED:
		if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
		    (hapd->conf->mesh & MESH_ENABLED)) {
			wpa_printf(MSG_DEBUG, "SAE: remove the STA (" MACSTR
				   ") doing reauthentication",
				   MAC2STR(sta->addr));
			wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);
			ap_free_sta(hapd, sta);
			*sta_removed = 1;
		} else if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
			wpa_printf(MSG_DEBUG, "SAE: Start reauthentication");
			ret = auth_sae_send_commit(hapd, sta, 1, status_code);
			if (ret)
				return ret;
			sae_set_state(sta, SAE_COMMITTED, "Sent Commit");

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else {
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_confirm(hapd, sta);
			sae_clear_temp_data(sta->sae);
			if (ret)
				return ret;
		}
		break;
	default:
		wpa_printf(MSG_ERROR, "SAE: invalid state %d",
			   sta->sae->state);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	return WLAN_STATUS_SUCCESS;
}


/*
 * hostapd_sp_implied_key_mgmt - Derive implied key_mgmt from security profiles
 *
 * Returns the union of all AKMs defined by the configured security profiles.
 * Allows the AP to accept connections whose AKM is implied by a Security
 * Profile IE even when that AKM is not explicitly listed in wpa_key_mgmt.
 * Covers all profile families: EPPKE (0-2), 802.1X (3-7/11-15), OWE (8),
 * SAE (9), FT-SAE (10).
 */
int hostapd_sp_implied_key_mgmt(const struct hostapd_bss_config *conf)
{
	int i, implied = 0;

	if (!conf->security_profiles)
		return 0;

	for (i = 0; conf->security_profiles[i] >= 0; i++) {
		int p = conf->security_profiles[i];

		if (p >= 0 && p < MAX_SECURITY_PROFILE_NUM)
			implied |= security_profile_table[p].key_mgmt;
	}
	return implied;
}

static void sae_pick_next_group(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sae_data *sae = sta->sae;
	struct hostapd_bss_config *conf = hapd->conf;
	int i, *groups = conf->sae_groups;
	int default_groups[] = { 19, 0, 0 };

	if (sae->state != SAE_COMMITTED)
		return;

	wpa_printf(MSG_DEBUG, "SAE: Previously selected group: %d", sae->group);

	if (!groups) {
		groups = default_groups;
		if (wpa_key_mgmt_sae_ext_key(conf->wpa_key_mgmt |
					     conf->rsn_override_key_mgmt |
					     conf->rsn_override_key_mgmt_2 |
					     hostapd_sp_implied_key_mgmt(conf)))
			default_groups[1] = 20;
	}

	for (i = 0; groups[i] > 0; i++) {
		if (sae->group == groups[i])
			break;
	}

	if (groups[i] <= 0) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Previously selected group not found from the current configuration");
		return;
	}

	for (;;) {
		i++;
		if (groups[i] <= 0) {
			wpa_printf(MSG_DEBUG,
				   "SAE: No alternative group enabled");
			return;
		}

		if (sae_set_group(sae, groups[i]) < 0)
			continue;

		break;
	}
	wpa_printf(MSG_DEBUG, "SAE: Selected new group: %d", groups[i]);
}


int sae_status_success(struct hostapd_data *hapd, u16 status_code)
{
	enum sae_pwe sae_pwe = hapd->conf->sae_pwe;
	int id_in_use;
	bool sae_pk = false;

	id_in_use = hostapd_sae_pw_id_in_use(hapd->conf);
	if (id_in_use == 2 && sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		sae_pwe = SAE_PWE_HASH_TO_ELEMENT;
	else if (id_in_use == 1 && sae_pwe == SAE_PWE_HUNT_AND_PECK)
		sae_pwe = SAE_PWE_BOTH;
#ifdef CONFIG_SAE_PK
	sae_pk = hostapd_sae_pk_in_use(hapd->conf);
	if (sae_pwe == SAE_PWE_HUNT_AND_PECK && sae_pk)
		sae_pwe = SAE_PWE_BOTH;
#endif /* CONFIG_SAE_PK */
	if (sae_pwe == SAE_PWE_HUNT_AND_PECK &&
	    (hapd->conf->wpa_key_mgmt &
	     (WPA_KEY_MGMT_SAE_EXT_KEY | WPA_KEY_MGMT_FT_SAE_EXT_KEY)))
		sae_pwe = SAE_PWE_BOTH;

	return ((sae_pwe == SAE_PWE_HUNT_AND_PECK ||
		 sae_pwe == SAE_PWE_FORCE_HUNT_AND_PECK) &&
		status_code == WLAN_STATUS_SUCCESS) ||
		(sae_pwe == SAE_PWE_HASH_TO_ELEMENT &&
		 (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		  (sae_pk && status_code == WLAN_STATUS_SAE_PK))) ||
		(sae_pwe == SAE_PWE_BOTH &&
		 (status_code == WLAN_STATUS_SUCCESS ||
		  status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		  (sae_pk && status_code == WLAN_STATUS_SAE_PK)));
}


static int sae_is_group_enabled(struct hostapd_data *hapd, int group)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int *groups = conf->sae_groups;
	int default_groups[] = { 19, 0, 0 };
	int i;

	if (!groups) {
		groups = default_groups;
		if (wpa_key_mgmt_sae_ext_key(conf->wpa_key_mgmt |
					     conf->rsn_override_key_mgmt |
					     conf->rsn_override_key_mgmt_2 |
					     hostapd_sp_implied_key_mgmt(conf)))
			default_groups[1] = 20;
	}

	for (i = 0; groups[i] > 0; i++) {
		if (groups[i] == group)
			return 1;
	}

	return 0;
}


static int check_sae_rejected_groups(struct hostapd_data *hapd,
				     struct sae_data *sae)
{
	const struct wpabuf *groups;
	size_t i, count, len;
	const u8 *pos;

	if (!sae->tmp)
		return 0;
	groups = sae->tmp->peer_rejected_groups;
	if (!groups)
		return 0;

	pos = wpabuf_head(groups);
	len = wpabuf_len(groups);
	if (len & 1) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Invalid length of the Rejected Groups element payload: %zu",
			   len);
		return 1;
	}

	count = len / 2;
	for (i = 0; i < count; i++) {
		int enabled;
		u16 group;

		group = WPA_GET_LE16(pos);
		pos += 2;
		enabled = sae_is_group_enabled(hapd, group);
		wpa_printf(MSG_DEBUG, "SAE: Rejected group %u is %s",
			   group, enabled ? "enabled" : "disabled");
		if (enabled)
			return 1;
	}

	return 0;
}
static void handle_auth_sae(struct hostapd_data *hapd, struct sta_info *sta,
			    const struct ieee80211_mgmt *mgmt, size_t len,
			    u16 auth_transaction, u16 status_code,
			    int rssi)
{
	int resp = WLAN_STATUS_SUCCESS;
	struct wpabuf *data = NULL;
	struct hostapd_bss_config *conf = hapd->conf;
	int *groups = conf->sae_groups;
	int default_groups[] = { 19, 0, 0 };
	const u8 *pos, *end;
	int sta_removed = 0;
	bool success_status;
	const u8 *dst = mgmt->sa;
#ifdef CONFIG_IEEE80211BE
	u8 link_id = hapd->mld_link_id;
#endif

	if (!groups) {
		groups = default_groups;
		if (wpa_key_mgmt_sae_ext_key(conf->wpa_key_mgmt |
					     conf->rsn_override_key_mgmt |
					     conf->rsn_override_key_mgmt_2 |
					     hostapd_sp_implied_key_mgmt(conf)))
			default_groups[1] = 20;
	}

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && sta && sta->mld_info.mld_sta) {
		if (sta->unadded_sta) {
			dst = sta->mld_info.links[link_id].peer_addr;
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */


#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->sae_reflection_attack &&
	    auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
		wpa_printf(MSG_DEBUG, "SAE: TESTING - reflection attack");
		pos = mgmt->u.auth.variable;
		end = ((const u8 *) mgmt) + len;
		resp = status_code;
		send_auth_reply(hapd, sta, dst,
				WLAN_AUTH_SAE,
				auth_transaction, resp, pos, end - pos,
				"auth-sae-reflection-attack");
		goto remove_sta;
	}

	if (hapd->conf->sae_commit_override &&
	    auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
		wpa_printf(MSG_DEBUG, "SAE: TESTING - commit override");
		send_auth_reply(hapd, sta, dst,
				WLAN_AUTH_SAE,
				auth_transaction, resp,
				wpabuf_head(hapd->conf->sae_commit_override),
				wpabuf_len(hapd->conf->sae_commit_override),
				"sae-commit-override");
		goto remove_sta;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (!sta->sae) {
		if (auth_transaction != WLAN_AUTH_TR_SEQ_SAE_COMMIT ||
		    !sae_status_success(hapd, status_code)) {
			wpa_printf(MSG_DEBUG, "SAE: Unexpected Status Code %u",
				   status_code);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto reply;
		}
		sta->sae = os_zalloc(sizeof(*sta->sae));
		if (!sta->sae) {
			resp = -1;
			goto remove_sta;
		}
		if (!hostapd_sae_pw_id_in_use(hapd->conf))
			sta->sae->no_pw_id = 1;
		sae_set_state(sta, SAE_NOTHING, "Init");
		sta->sae->sync = 0;
	}

	if (sta->mesh_sae_pmksa_caching) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Cancel use of mesh PMKSA caching because peer starts SAE authentication");
		wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);
		sta->mesh_sae_pmksa_caching = 0;
	}

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
		const u8 *token = NULL;
		size_t token_len = 0;
		int allow_reuse = 0;

		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "start SAE authentication (RX commit, status=%u (%s))",
			       status_code, status2str(status_code));

		if ((hapd->conf->mesh & MESH_ENABLED) &&
		    status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ &&
		    sta->sae->tmp) {
			pos = mgmt->u.auth.variable;
			end = ((const u8 *) mgmt) + len;
			if (pos + sizeof(le16) > end) {
				wpa_printf(MSG_ERROR,
					   "SAE: Too short anti-clogging token request");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}
			resp = sae_group_allowed(sta->sae, groups,
						 WPA_GET_LE16(pos));
			if (resp != WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_ERROR,
					   "SAE: Invalid group in anti-clogging token request");
				goto reply;
			}
			pos += sizeof(le16);

			wpabuf_free(sta->sae->tmp->anti_clogging_token);
			sta->sae->tmp->anti_clogging_token =
				wpabuf_alloc_copy(pos, end - pos);
			if (sta->sae->tmp->anti_clogging_token == NULL) {
				wpa_printf(MSG_ERROR,
					   "SAE: Failed to alloc for anti-clogging token");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto remove_sta;
			}

			/*
			 * IEEE Std 802.11-2012, 11.3.8.6.4: If the Status code
			 * is 76, a new Commit Message shall be constructed
			 * with the Anti-Clogging Token from the received
			 * Authentication frame, and the commit-scalar and
			 * COMMIT-ELEMENT previously sent.
			 */
			resp = auth_sae_send_commit(hapd, sta, 0, status_code);
			if (resp != WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_ERROR,
					   "SAE: Failed to send commit message");
				goto remove_sta;
			}
			sae_set_state(sta, SAE_COMMITTED,
				      "Sent Commit (anti-clogging token case in mesh)");
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
			return;
		}

		if ((hapd->conf->mesh & MESH_ENABLED) &&
		    status_code ==
		    WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
		    sta->sae->tmp) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Peer did not accept our SAE group");
			sae_pick_next_group(hapd, sta);
			goto remove_sta;
		}

		if (!sae_status_success(hapd, status_code))
			goto remove_sta;

		if (sae_proto_instance_disabled(sta)) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Protocol instance temporarily disabled - discard received SAE commit");
			return;
		}

		if (!(hapd->conf->mesh & MESH_ENABLED) &&
		    sta->sae->state == SAE_COMMITTED) {
			/* This is needed in the infrastructure BSS case to
			 * address a sequence where a STA entry may remain in
			 * hostapd across two attempts to do SAE authentication
			 * by the same STA. The second attempt may end up trying
			 * to use a different group and that would not be
			 * allowed if we remain in Committed state with the
			 * previously set parameters. */
			pos = mgmt->u.auth.variable;
			end = ((const u8 *) mgmt) + len;
			if ((!sta->sae->tmp ||
			     !sta->sae->tmp->try_other_password) &&
			    end - pos >= (int) sizeof(le16) &&
			    sae_group_allowed(sta->sae, groups,
					      WPA_GET_LE16(pos)) ==
			    WLAN_STATUS_SUCCESS) {
				/* Do not waste resources deriving the same PWE
				 * again since the same group is reused. */
				sae_set_state(sta, SAE_NOTHING,
					      "Allow previous PWE to be reused");
				allow_reuse = 1;
			} else {
				sae_set_state(sta, SAE_NOTHING,
					      "Clear existing state to allow restart");
				sae_clear_data(sta->sae);
			}
		}

		resp = sae_parse_commit(sta->sae, mgmt->u.auth.variable,
					((const u8 *) mgmt) + len -
					mgmt->u.auth.variable, &token,
					&token_len, groups, status_code ==
					WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
					status_code == WLAN_STATUS_SAE_PK,
					NULL);
		if (resp == SAE_SILENTLY_DISCARD) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Drop commit message from " MACSTR " due to reflection attack",
				   MAC2STR(sta->addr));
			goto remove_sta;
		}

		if (resp == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER) {
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER
				MACSTR, MAC2STR(sta->addr));
			sae_clear_retransmit_timer(hapd, sta);
			sae_set_state(sta, SAE_NOTHING,
				      "Unknown Password Identifier");
			if (sta->sae->state == SAE_NOTHING)
				goto reply;
			goto remove_sta;
		}

		if (token &&
		    check_comeback_token(hapd->comeback_key,
					 hapd->comeback_pending_idx, sta->addr,
					 token, token_len)
		    < 0) {
			wpa_printf(MSG_DEBUG, "SAE: Drop commit message with "
				   "incorrect token from " MACSTR,
				   MAC2STR(sta->addr));
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto remove_sta;
		}

		if (resp != WLAN_STATUS_SUCCESS)
			goto reply;

		if (check_sae_rejected_groups(hapd, sta->sae)) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto reply;
		}

		if (hapd->conf->security_profiles) {
			const u8 *pos;
			size_t remaining_len, ies_len;

			remaining_len = len - offsetof(struct ieee80211_mgmt, u.auth.variable);
			pos = skip_ml_auth_fixed_fields(hapd, mgmt, len);

			if (pos) {
				ies_len = remaining_len - (pos - mgmt->u.auth.variable);
				struct ieee802_11_elems elems;
				if (ieee802_11_parse_elems(pos, ies_len, &elems, 1) == ParseFailed) {
					wpa_printf(MSG_DEBUG, "Failed to parse IEs");
					resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
					goto reply;
				}
				resp = validate_security_profile_common(hapd, sta, &elems,
									"SAE", NULL);

				if (resp != WLAN_STATUS_SUCCESS)
					goto reply;
			}
		}

		if (!token && use_anti_clogging(hapd) && !allow_reuse) {
			int h2e = 0;

			wpa_printf(MSG_DEBUG,
				   "SAE: Request anti-clogging token from "
				   MACSTR, MAC2STR(sta->addr));
			if (sta->sae->tmp)
				h2e = sta->sae->h2e;
			if (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
			    status_code == WLAN_STATUS_SAE_PK)
				h2e = 1;
			data = auth_build_token_req(
				&hapd->last_comeback_key_update,
				hapd->comeback_key,
				hapd->comeback_idx,
				hapd->comeback_pending_idx,
				sizeof(hapd->comeback_pending_idx),
				sta->sae->group,
				sta->addr, h2e);
			resp = WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ;
			if (hapd->conf->mesh & MESH_ENABLED)
				sae_set_state(sta, SAE_NOTHING,
					      "Request anti-clogging token case in mesh");
			goto reply;
		}

#ifdef CONFIG_HOSTAPD_IF
		/* Link context will be computed inside hostapd_if_notify_auth() */
		if (hostapd_if_notify_auth(hapd, sta, (const u8 *) mgmt, len,
				rssi, status_code, auth_transaction,
				allow_reuse, WLAN_AUTH_SAE, dst) ==
				HOSTAPD_IF_FRAME_PROCESSING_WAIT)
			return;
#endif

		resp = sae_sm_step(hapd, sta, auth_transaction,
				   status_code, allow_reuse, &sta_removed);
	} else if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "SAE authentication (RX confirm, status=%u (%s))",
			       status_code, status2str(status_code));
		if (status_code != WLAN_STATUS_SUCCESS)
			goto remove_sta;
		if (sta->sae->state >= SAE_CONFIRMED ||
		    !(hapd->conf->mesh & MESH_ENABLED)) {
			const u8 *var;
			size_t var_len;
			u16 peer_send_confirm;

			var = mgmt->u.auth.variable;
			var_len = ((u8 *) mgmt) + len - mgmt->u.auth.variable;
			if (var_len < 2) {
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}

			peer_send_confirm = WPA_GET_LE16(var);

			if (sta->sae->state == SAE_ACCEPTED &&
			    (peer_send_confirm <= sta->sae->rc ||
			     peer_send_confirm == 0xffff)) {
				wpa_printf(MSG_DEBUG,
					   "SAE: Silently ignore unexpected Confirm from peer "
					   MACSTR
					   " (peer-send-confirm=%u Rc=%u)",
					   MAC2STR(sta->addr),
					   peer_send_confirm, sta->sae->rc);
				return;
			}

			if (sae_check_confirm(sta->sae, var, var_len,
					      NULL) < 0) {
				if (sae_password_track_fail(hapd, sta)) {
					wpa_printf(MSG_DEBUG,
						   "SAE: Reject mismatching Confirm so that another password can be attempted by "
						   MACSTR,
						   MAC2STR(sta->addr));
					if (sta->sae->tmp)
						sta->sae->tmp->
							try_other_password = 1;
					resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
					goto reply;
				}
				resp = WLAN_STATUS_CHALLENGE_FAIL;
				goto reply;
			}
			sae_password_track_success(hapd, sta);
			sta->sae->rc = peer_send_confirm;
		}
#ifdef CONFIG_HOSTAPD_IF
		if (hostapd_if_notify_auth(hapd, sta, (const u8 *) mgmt, len,
					rssi, status_code, auth_transaction, 0,
					WLAN_AUTH_SAE, dst) ==
					HOSTAPD_IF_FRAME_PROCESSING_WAIT)
			return;
#endif

		resp = sae_sm_step(hapd, sta, auth_transaction,
				   status_code, 0, &sta_removed);
	} else {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "unexpected SAE authentication transaction %u (status=%u (%s))",
			       auth_transaction, status_code,
			       status2str(status_code));
		if (status_code != WLAN_STATUS_SUCCESS)
			goto remove_sta;
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
	}

reply:
	if (!sta_removed && resp != WLAN_STATUS_SUCCESS) {
		pos = mgmt->u.auth.variable;
		end = ((const u8 *) mgmt) + len;

		/* Copy the Finite Cyclic Group field from the request if we
		 * rejected it as unsupported group. */
		if (resp == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
		    !data && end - pos >= 2)
			data = wpabuf_alloc_copy(pos, 2);

		send_auth_reply(hapd, sta, dst,
				WLAN_AUTH_SAE,
				auth_transaction, resp,
				data ? wpabuf_head(data) : (u8 *) "",
				data ? wpabuf_len(data) : 0, "auth-sae");
		sae_sme_send_external_auth_status(hapd, sta, resp);
		wpa_auth_connection_fail_event(hapd->wpa_auth,
					       len, "Authentication_frame",
					       (u8 *)mgmt, sta->addr,
					       hapd->own_addr, resp,
					       true);
	}

remove_sta:
	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT)
		success_status = sae_status_success(hapd, status_code);
	else
		success_status = status_code == WLAN_STATUS_SUCCESS;
	if (!sta_removed && sta->added_unassoc &&
	    (resp != WLAN_STATUS_SUCCESS || !success_status)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
	wpabuf_free(data);
}


/**
 * auth_sae_init_committed - Send COMMIT and start SAE in committed state
 * @hapd: BSS data for the device initiating the authentication
 * @sta: the peer to which commit authentication frame is sent
 *
 * This function implements Init event handling (IEEE Std 802.11-2012,
 * 11.3.8.6.3) in which initial COMMIT message is sent. Prior to calling, the
 * sta->sae structure should be initialized appropriately via a call to
 * sae_prepare_commit().
 */
int auth_sae_init_committed(struct hostapd_data *hapd, struct sta_info *sta)
{
	int ret;

	if (!sta->sae || !sta->sae->tmp)
		return -1;

	if (sta->sae->state != SAE_NOTHING)
		return -1;

	ret = auth_sae_send_commit(hapd, sta, 0, -1);
	if (ret)
		return -1;

	sae_set_state(sta, SAE_COMMITTED, "Init and sent commit");
	sta->sae->sync = 0;
	sae_set_retransmit_timer(hapd, sta);

	return 0;
}


void auth_sae_process_commit(void *eloop_ctx, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct hostapd_sae_commit_queue *q;
	unsigned int queue_len;

	q = dl_list_first(&hapd->sae_commit_queue,
			  struct hostapd_sae_commit_queue, list);
	if (!q)
		return;
	wpa_printf(MSG_DEBUG,
		   "SAE: Process next available message from queue");
	dl_list_del(&q->list);
	handle_auth(hapd, (const struct ieee80211_mgmt *) q->msg, q->len,
		    q->rssi, 1);
	os_free(q);

	if (eloop_is_timeout_registered(auth_sae_process_commit, hapd, NULL))
		return;
	queue_len = dl_list_len(&hapd->sae_commit_queue);
	eloop_register_timeout(0, queue_len * 10000, auth_sae_process_commit,
			       hapd, NULL);
}


static void auth_sae_queue(struct hostapd_data *hapd,
			   const struct ieee80211_mgmt *mgmt, size_t len,
			   int rssi)
{
	struct hostapd_sae_commit_queue *q, *q2;
	unsigned int queue_len;
	const struct ieee80211_mgmt *mgmt2;

	queue_len = dl_list_len(&hapd->sae_commit_queue);
	if (queue_len >= 15) {
		wpa_printf(MSG_DEBUG,
			   "SAE: No more room in message queue - drop the new frame from "
			   MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "SAE: Queue Authentication message from "
		   MACSTR " for processing (queue_len %u)", MAC2STR(mgmt->sa),
		   queue_len);
	q = os_zalloc(sizeof(*q) + len);
	if (!q)
		return;
	q->rssi = rssi;
	q->len = len;
	os_memcpy(q->msg, mgmt, len);

	/* Check whether there is already a queued Authentication frame from the
	 * same station with the same transaction number and if so, replace that
	 * queue entry with the new one. This avoids issues with a peer that
	 * sends multiple times (e.g., due to frequent SAE retries). There is no
	 * point in us trying to process the old attempts after a new one has
	 * obsoleted them. */
	dl_list_for_each(q2, &hapd->sae_commit_queue,
			 struct hostapd_sae_commit_queue, list) {
		mgmt2 = (const struct ieee80211_mgmt *) q2->msg;
		if (ether_addr_equal(mgmt->sa, mgmt2->sa) &&
		    mgmt->u.auth.auth_transaction ==
		    mgmt2->u.auth.auth_transaction) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Replace queued message from same STA with same transaction number");
			dl_list_add(&q2->list, &q->list);
			dl_list_del(&q2->list);
			os_free(q2);
			goto queued;
		}
	}

	/* No pending identical entry, so add to the end of the queue */
	dl_list_add_tail(&hapd->sae_commit_queue, &q->list);

queued:
	if (eloop_is_timeout_registered(auth_sae_process_commit, hapd, NULL))
		return;
	eloop_register_timeout(0, queue_len * 10000, auth_sae_process_commit,
			       hapd, NULL);
}


static int auth_sae_queued_addr(struct hostapd_data *hapd, const u8 *addr)
{
	struct hostapd_sae_commit_queue *q;
	const struct ieee80211_mgmt *mgmt;

	dl_list_for_each(q, &hapd->sae_commit_queue,
			 struct hostapd_sae_commit_queue, list) {
		mgmt = (const struct ieee80211_mgmt *) q->msg;
		if (ether_addr_equal(addr, mgmt->sa))
			return 1;
	}

	return 0;
}

#endif /* CONFIG_SAE */


static u16 wpa_res_to_status_code(enum wpa_validate_result res)
{
	switch (res) {
	case WPA_IE_OK:
		return WLAN_STATUS_SUCCESS;
	case WPA_INVALID_IE:
		return WLAN_STATUS_INVALID_IE;
	case WPA_INVALID_GROUP:
		return WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
	case WPA_INVALID_PAIRWISE:
		return WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
	case WPA_INVALID_AKMP:
		return WLAN_STATUS_AKMP_NOT_VALID;
	case WPA_NOT_ENABLED:
		return WLAN_STATUS_INVALID_IE;
	case WPA_ALLOC_FAIL:
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	case WPA_MGMT_FRAME_PROTECTION_VIOLATION:
		return WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
	case WPA_INVALID_MGMT_GROUP_CIPHER:
		return WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
	case WPA_INVALID_MDIE:
		return WLAN_STATUS_INVALID_MDIE;
	case WPA_INVALID_PROTO:
		return WLAN_STATUS_INVALID_IE;
	case WPA_INVALID_PMKID:
		return WLAN_STATUS_INVALID_PMKID;
	case WPA_DENIED_OTHER_REASON:
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}
	return WLAN_STATUS_INVALID_IE;
}


#ifdef CONFIG_IEEE8021X_AUTH

static struct wpabuf *
prepare_802_1x_auth_resp(struct hostapd_data *hapd, struct sta_info *sta,
			 u16 auth_transaction, u16 status,
			 struct rsn_pmksa_cache_entry *cached_pmk,
			 const u8 *eap_req, size_t eap_req_len)
{
	struct wpabuf *pub = NULL, *data;
	bool enc_assoc = ap_sta_support_enc_assoc(hapd,
						  sta->eap_auth_data.rsnxe,
						  sta->eap_auth_data.rsnxe_len);

	data = wpabuf_alloc(1000 + eap_req_len);
	if (!data) {
		wpa_printf(MSG_INFO,
			   "Authentication frame buffer allocation failed");
		return NULL;
	}

	/* Encapsulation Length field */
	wpabuf_put_le16(data, eap_req_len);
	/* Encapsulation field */
	wpabuf_put_data(data, eap_req, eap_req_len);

	if (status != WLAN_STATUS_SUCCESS &&
	    status != WLAN_STATUS_802_1_X_AUTH_SUCCESS)
		goto reply;

	/* Authentication frames with transaction sequence greater than or
	 * equal to 3 contain Authentication fields only.
	 */
	if (auth_transaction == 2) {
		/* Per IEEE 802.11bi/D4.0, 12.16.8.3 (IEEE 802.1X), a responder
		 * that sets
		 * dot11EPPReAssociationFrameEncryptionSupportActivated
		 * to false or does not receive the RSNXE in the first
		 * Authentication frame with the (Re)Association Frame
		 * Encryption Support field set to 1 shall not include
		 * a Diffie-Hellman Parameter element nor a Nonce element
		 * nor an RSNE in the second Authentication frame for
		 * IEEE 802.1X authentication.
		 */
		if (enc_assoc) {
			u8 a_nonce[WPA_NONCE_LEN];
			struct hostapd_bss_config *conf = hapd->conf;
			int res;

			/* Derive own public key */
			if (sta->eap_auth_data.ecdh) {
				pub = crypto_ecdh_get_pubkey(
					sta->eap_auth_data.ecdh, 1);
				if (!pub) {
					status =
						WLAN_STATUS_UNSPECIFIED_FAILURE;
					goto reply;
				}
			}

			/* ANonce generation */
			if (random_get_bytes(a_nonce, WPA_NONCE_LEN) < 0) {
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}
			os_memcpy(sta->eap_auth_data.anonce, a_nonce,
				  WPA_NONCE_LEN);


			if (pub && wpabuf_resize(&data, wpabuf_len(pub)) < 0) {
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}

			/* Per IEEE 802.11bi/D4.0, 12.16.8.3 (IEEE 802.1X),
			 * responder shall include an RSNE with the AKM and
			 * pairwise cipher suite as indicated in the first
			 * Authentication frame.
			 */
			res = wpa_write_802_1x_rsne(
				hapd->wpa_auth,
				wpabuf_mhead_u8(data) + wpabuf_len(data),
				(wpabuf_size(data) - wpabuf_len(data)),
				cached_pmk ? cached_pmk->pmkid : NULL,
				sta->eap_auth_data.akm,
				sta->eap_auth_data.cipher,
				conf->wpa_group,
				conf->group_mgmt_cipher,
				conf->ieee80211w);
			if (res < 0) {
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}
			wpabuf_put(data, res);

			/* DH Parameter element */
			wpabuf_put_u8(data, WLAN_EID_EXTENSION);
			wpabuf_put_u8(data, 1 + 2 + wpabuf_len(pub));
			wpabuf_put_u8(data, WLAN_EID_EXT_OWE_DH_PARAM);
			wpabuf_put_le16(data, sta->eap_auth_data.group);
			wpabuf_put_buf(data, pub);

			/* ANonce in Nonce element */
			wpabuf_put_u8(data, WLAN_EID_EXTENSION);
			wpabuf_put_u8(data, 1 + WPA_NONCE_LEN);
			wpabuf_put_u8(data, WLAN_EID_EXT_NONCE);
			wpabuf_put_data(data, a_nonce, WPA_NONCE_LEN);
		} else {
			/* Per IEEE 802.11bi/D4.0, 12.16.5 (IEEE 802.1X
			 * authentication utilizing Authentication frames), the
			 * Responser shall construct the second Authentication
			 * frame with an AKM Suite Selector element indicating
			 * the same IEEE 802.1X AKM indicated in the first
			 * Authentication frame.
			 */
			wpabuf_put_u8(data, WLAN_EID_EXTENSION);
			wpabuf_put_u8(data, 1 + RSN_SELECTOR_LEN);
			wpabuf_put_u8(data, WLAN_EID_EXT_AKM_SUITE_SELECTOR);
			RSN_SELECTOR_PUT(wpabuf_put(data, RSN_SELECTOR_LEN),
					 wpa_akm_to_suite(
						 sta->eap_auth_data.akm));
		}
	} /* if (auth_transaction == 2) */
reply:
	wpabuf_free(pub);
	return data;
}


u16 wpa_auth_validate_802_1x_frame(struct hostapd_data *hapd,
				   struct sta_info *sta,
				   struct ieee802_11_elems *elems)
{
	struct wpa_ie_data rsn;
	const int default_groups[] = { 19, 0 };
	bool enc_assoc = ap_sta_support_enc_assoc(hapd,
						  elems->rsnxe,
						  elems->rsnxe_len);

	/* Per IEEE P802.11bi/D4.0, 12.16.8.3 (IEEE 802.1X), an originator that
	 * sets dot11EPPReAssociationFrameEncryptionSupportActivated to false or
	 * does not receive the RSNXE from the responder with the
	 * (Re)Association Frame Encryption Support field set to 1 shall not
	 * include a Diffie-Hellman Parameter element nor an RSNE nor an RSNXE
	 * nor a Nonce element in the first Authentication frame for IEEE 802.1X
	 * authentication.
	 */
	if (!enc_assoc &&
	    (elems->rsn_ie || elems->nonce || elems->owe_dh)) {
		wpa_printf(MSG_INFO,
			   "Invalid inclusion of RSNE/Nonce/DHE when (Re)Association frame encryption is not supported");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	if (enc_assoc &&
	    (!elems->rsn_ie || !elems->nonce ||
	     elems->nonce_len != WPA_NONCE_LEN || !elems->owe_dh)) {
		wpa_printf(MSG_ERROR, "Missing RSNE/DHIE/Nonce");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* Both RSNE and AKM Suite Selector element shall not be present at the
	 * same time. */
	if (elems->rsn_ie && elems->akm_suite_selector) {
		wpa_printf(MSG_INFO,
			   "Incorrect inclusion of both RSNE and AKM Suite Selector element");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (enc_assoc &&
	    (!elems->rsn_ie ||
	     wpa_parse_wpa_ie_rsn(elems->rsn_ie - 2, elems->rsn_ie_len + 2,
				  &rsn) < 0)) {
		wpa_printf(MSG_INFO, "No valid RSNE");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (enc_assoc && elems->rsn_ie) {
		if (!(rsn.pairwise_cipher & hapd->conf->rsn_pairwise)) {
			wpa_printf(MSG_INFO,
				   "Invalid pairwise cipher (0x%x) in RSNE",
				   rsn.pairwise_cipher);
			return WPA_INVALID_PAIRWISE;
		}
		sta->eap_auth_data.cipher = rsn.pairwise_cipher;
		wpa_printf(MSG_DEBUG, "Received pairwise cipher (0x%x) in RSNE",
			   rsn.pairwise_cipher);

		if (!(rsn.key_mgmt & hapd->conf->wpa_key_mgmt)) {
			wpa_printf(MSG_INFO, "Invalid key mgmt (0x%x) in RSNE",
				   rsn.key_mgmt);
			return WPA_INVALID_AKMP;
		}
		sta->eap_auth_data.akm = rsn.key_mgmt;
		wpa_printf(MSG_DEBUG, "Received keymgmt (0x%x) in RSNE",
			   rsn.key_mgmt);
	}

	/* Validate AKM Suite Selector element */
	if (elems->akm_suite_selector) {
		sta->eap_auth_data.akm = rsn_key_mgmt_to_wpa_akm(
			RSN_SELECTOR_GET(elems->akm_suite_selector));
		if (!(sta->eap_auth_data.akm & hapd->conf->wpa_key_mgmt)) {
			wpa_printf(MSG_INFO,
				   "Invalid key mgmt (0x%x) in AKM Suite Selector element",
				   sta->eap_auth_data.akm);
			return WPA_INVALID_AKMP;
		}
		wpa_printf(MSG_DEBUG,
			   "Received keymgmt (0x%x) in AKM Suite Selector element",
			   sta->eap_auth_data.akm);
	}

	if (elems->rsnxe) {
		os_free(sta->eap_auth_data.rsnxe);
		sta->eap_auth_data.rsnxe =
			os_memdup(elems->rsnxe, elems->rsnxe_len);
		sta->eap_auth_data.rsnxe_len = elems->rsnxe_len;
	}

	/* Store SNonce */
	if (elems->nonce && elems->nonce_len == WPA_NONCE_LEN) {
		os_memcpy(sta->eap_auth_data.snonce, elems->nonce,
			  WPA_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "SNonce", elems->nonce, WPA_NONCE_LEN);
	}

	/* Validate DH Parameter element */
	if (elems->owe_dh) {
		u16 group;
		u8 pubkey_len;
		const u8 *pubkey;
		struct wpabuf *secret;

		group = WPA_GET_LE16(elems->owe_dh);
		if (!int_array_includes(default_groups, group)) {
			wpa_printf(MSG_INFO,
				   "Received unsupported group value %u",
				   group);
			return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
		}
		sta->eap_auth_data.group = group;
		pubkey = elems->owe_dh + 2;
		pubkey_len = elems->owe_dh_len - 2;

		/* TODO: Any more validation of peer public key needed? */
		if (!pubkey_len) {
			wpa_printf(MSG_INFO, "Missing DH public key");
			return WLAN_STATUS_INVALID_PUBLIC_KEY;
		}

		/* Setup ECDH context */
		crypto_ecdh_deinit(sta->eap_auth_data.ecdh);
		sta->eap_auth_data.ecdh = crypto_ecdh_init(group);
		if (!sta->eap_auth_data.ecdh) {
			wpa_printf(MSG_INFO, "Failed to setup ECDH context");
			return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
		}

		/* Generate shared secret */
		wpabuf_clear_free(sta->eap_auth_data.dhss);
		sta->eap_auth_data.dhss = NULL;
		secret = crypto_ecdh_set_peerkey(sta->eap_auth_data.ecdh, 0,
						 pubkey, pubkey_len);
		if (!secret) {
			wpa_printf(MSG_INFO, "Invalid peer public key");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
		wpa_hexdump_buf_key(MSG_DEBUG, "DH shared secret", secret);
		sta->eap_auth_data.dhss = secret;
	}

	return WLAN_STATUS_SUCCESS;
}


/**
 * ieee80211_send_eap_req - Callback function to send EAP-Request message in an
 *	Authentication frame
 *
 * This function is called from ieee802_1x_eapol_send() using the
 * hapd->send_eap_req callback. Its main purpose is to prepend the EAP-Request
 * data with an IEEE 802.1X header and call prepare_802_1x_auth_resp() to send
 * out the next IEEE 802.1X Authentication frame to the station. If this is an
 * EAP-Success frame, it also fetches the MSK derived from the successful EAP
 * authentication to derive PMK and PTK and configure the TK to the driver.
 */
void ieee80211_send_eap_req(struct hostapd_data *hapd, struct sta_info *sta,
			    u8 type, u16 auth_transaction, u16 status,
			    struct rsn_pmksa_cache_entry *cached_pmk,
			    const u8 *eap_req, size_t eap_req_len)
{
	bool enc_assoc = ap_sta_support_enc_assoc(hapd,
						  sta->eap_auth_data.rsnxe,
						  sta->eap_auth_data.rsnxe_len);
	struct ieee802_1x_hdr *xhdr;
	u8 *data;
	size_t len = eap_req_len + sizeof(struct ieee802_1x_hdr);
	struct wpabuf *reply;

	wpa_printf(MSG_DEBUG,
		   "Process EAP-Request data for TX using an Authentication frame");

	data = os_malloc(len);
	if (!data) {
		wpa_printf(MSG_ERROR, "malloc() failed for %s", __func__);
		return;
	}

	xhdr = (struct ieee802_1x_hdr *) data;
	xhdr->version = hapd->conf->eapol_version;
	xhdr->type = type;
	xhdr->length = host_to_be16(eap_req_len);

	if (eap_req && eap_req_len > 0)
		os_memcpy(xhdr + 1, eap_req, eap_req_len);

	wpa_hexdump(MSG_MSGDUMP, "EAP-Request", eap_req, eap_req_len);

	/* EAP-Success */
	if (enc_assoc && eap_req_len > 0 && eap_req[0] == 3) {
		u8 msk[2 * PMK_LEN] = { 0 };
		size_t _len = 2 * PMK_LEN;
		size_t pmk_len, kdk_len;
		bool is_ml = ap_sta_is_mld(hapd, sta);
		enum wpa_alg alg =
			wpa_cipher_to_alg(sta->eap_auth_data.cipher);
		size_t key_len =
			wpa_cipher_key_len(sta->eap_auth_data.cipher);
		const u8 *aa = hapd->own_addr;
		struct rsn_pmksa_cache *pmksa =
			wpa_auth_get_pmksa_cache(hapd->wpa_auth, is_ml);
		struct rsn_pmksa_cache_entry *entry;

#ifdef CONFIG_IEEE80211BE
		if (is_ml)
			aa = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

		/* Per IEEE 802.11bi/D4.0, 12.16.5 (IEEE 802.1X authentication
		 * utilizing Authentication frames), if the IEEE 802.1X
		 * authentication is successful, the Status Code field
		 * is set to 802_1_X_AUTH_SUCCESS. */
		status = WLAN_STATUS_802_1_X_AUTH_SUCCESS;
		/* TODO: If the IEEE 802.1X authentication fails,
		 * the status code is set to 802_1_X_AUTH_FAILED. */
		os_memset(&sta->eap_auth_data.ptk, 0, sizeof(struct wpa_ptk));
		if (wpa_auth_802_1x_get_msk(hapd->wpa_auth, sta->addr,
					    msk, &_len)) {
			wpa_printf(MSG_INFO, "Failed to get MSK");
			os_free(data);
			return;
		}

		if (wpa_key_mgmt_sha384(sta->eap_auth_data.akm))
			pmk_len = PMK_LEN_SUITE_B_192;
		else
			pmk_len = PMK_LEN;

		sta->eap_auth_data.pmk_len = pmk_len;
		os_memcpy(sta->eap_auth_data.pmk, msk, pmk_len);

		if (hapd->conf->force_kdk_derivation ||
		    (wpa_auth_ap_support_secure_ltf(hapd->wpa_auth) &&
		     ieee802_11_rsnx_capab(sta->eap_auth_data.rsnxe,
					   WLAN_RSNX_CAPAB_SECURE_LTF)))
			kdk_len = WPA_KDK_MAX_LEN;
		else
			kdk_len = 0;
		if (wpa_auth_802_1x_pmk_to_ptk(
			    msk, sta->eap_auth_data.pmk_len,
			    sta->addr, aa,
			    sta->eap_auth_data.snonce,
			    sta->eap_auth_data.anonce,
			    sta->eap_auth_data.akm,
			    sta->eap_auth_data.cipher,
			    wpabuf_head_u8(sta->eap_auth_data.dhss),
			    wpabuf_len(sta->eap_auth_data.dhss),
			    &sta->eap_auth_data.ptk, kdk_len)) {
			wpa_printf(MSG_INFO, "Failed to derive the PTK");
			os_free(data);
			return;
		}
		wpa_printf(MSG_DEBUG, "PTK derived successfully");

		if (wpa_auth_802_1x_set_key(hapd->wpa_auth,
					    alg, sta->addr,
					    sta->eap_auth_data.ptk.tk,
					    key_len)) {
			wpa_printf(MSG_INFO,
				   "Failed to set the TK to the driver");
			os_free(data);
			return;
		}

		/* Delete DHss after successful PTK derivation */
		wpabuf_clear_free(sta->eap_auth_data.dhss);
		sta->eap_auth_data.dhss = NULL;

		/* TODO: Fill session_timeout? */
		wpa_hexdump_key(MSG_DEBUG, "IEEE802.1X: Cache PMK",
				msk, pmk_len);

		entry = pmksa_cache_auth_add(pmksa, msk, pmk_len, NULL,
					     sta->eap_auth_data.ptk.kck,
					     sta->eap_auth_data.ptk.kck_len,
					     aa, sta->addr, 0, sta->eapol_sm,
					     sta->eap_auth_data.akm);
		if (!entry) {
			wpa_printf(MSG_INFO, "Failed to add PMKSA entry");
			return;
		}
		os_memcpy(sta->eap_auth_data.epp_pmkid_cur, entry->pmkid,
			  PMKID_LEN);
	}

	reply = prepare_802_1x_auth_resp(hapd, sta, auth_transaction, status,
					 cached_pmk, data, len);
	if (reply)
		send_8021x_auth_reply(hapd, sta, auth_transaction, status,
				      reply);
	os_free(data);
}


static void handle_auth_802_1x(struct hostapd_data *hapd, struct sta_info *sta,
			       const u8 *pos, size_t len, u16 auth_alg,
			       u16 auth_transaction)
{
	struct ieee802_1x_hdr *eapol_pdu;
	u16 encap_len, resp = WLAN_STATUS_SUCCESS;
	const u8 *end;
	struct wpabuf *reply;

	if (len < 2) {
		wpa_printf(MSG_INFO, "Missing Encapsulation Length field");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	end = pos + len;
	encap_len = WPA_GET_LE16(pos);
	pos += 2;
	if (encap_len > end - pos) {
		wpa_printf(MSG_INFO, "Truncated Encapsulation field");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* Start of Encapsulation field */
	eapol_pdu = (struct ieee802_1x_hdr *) pos;

	if (auth_transaction == 1 &&
	    eapol_pdu->type != IEEE802_1X_TYPE_EAPOL_START) {
		wpa_printf(MSG_INFO,
			   "Received unexpected EAPOL PDU type %u in the first Authentication frame",
			   eapol_pdu->type);
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	pos += encap_len;
	sta->eap_auth_data.auth_transaction = auth_transaction;

	/* Process Authentication frame elements
	 * Authentication frames with transaction sequence greater
	 * than or equal to 3 contain Authentication fields only.
	 */
	if (auth_transaction == 1) {
		struct wpa_ie_data data;
		struct ieee802_11_elems elems;
		struct rsn_pmksa_cache_entry *cached_pmk = NULL;
		bool is_ml = ap_sta_is_mld(hapd, sta);
		bool enc_assoc, pmkid_privacy;
		size_t i;

		if (ieee802_11_parse_elems(pos, end - pos, &elems, 1) ==
		    ParseFailed) {
			wpa_printf(MSG_INFO, "Could not parse elements");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		resp = wpa_auth_validate_802_1x_frame(hapd, sta, &elems);
		if (resp)
			goto fail;

		/* UHR: Validate Security Profile if present */
		if (elems.security_profile_ie && elems.security_profile_ie_len > 0) {
			size_t security_profile_body_len = elems.security_profile_ie_len > 1 ?
				elems.security_profile_ie_len - 1 : 0;

			wpa_printf(MSG_DEBUG,
				   "UHR: Found Security Profile element from STA "
				   MACSTR " in 802.1X auth (body_len=%zu)",
				   MAC2STR(sta->addr), security_profile_body_len);
			wpa_hexdump(MSG_DEBUG,
				    "UHR: Security Profile element body",
				    elems.security_profile_ie + 1, security_profile_body_len);

			if (!validate_sta_security_profile(
				    hapd, sta->addr,
				    elems.rsn_ie, elems.rsn_ie_len,
				    elems.rsnxe, elems.rsnxe_len,
				    elems.security_profile_ie, elems.security_profile_ie_len,
				    NULL)) {
				wpa_printf(MSG_INFO,
					   "UHR: Rejecting 802.1X auth from "
					   MACSTR " - Security Profile mismatch",
					   MAC2STR(sta->addr));
				resp = WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE;
				goto fail;
			}
			/* UHR Security Profile validated successfully */
			if (sta->wpa_sm) {
				sta->wpa_sm->security_profile_indication = 1;
				wpa_printf(MSG_DEBUG,
					   "UHR: STA " MACSTR
					   " profile validated in 802.1X auth, security_profile=1",
					   MAC2STR(sta->addr));
			}
		}

		enc_assoc = ap_sta_support_enc_assoc(hapd, elems.rsnxe,
						     elems.rsnxe_len);

		pmkid_privacy = hapd->conf->pmksa_caching_privacy &&
			ieee802_11_rsnx_capab_len(
				elems.rsnxe, elems.rsnxe_len,
				WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY);

		os_memset(&data, 0, sizeof(data));
		if (enc_assoc &&
		    (!elems.rsn_ie ||
		     wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2,
					  elems.rsn_ie_len + 2, &data) < 0)) {
			wpa_printf(MSG_INFO, "No valid RSNE");
			goto fail;
		}

		for (i = 0; i < data.num_pmkid; i++) {
			const u8 *aa;
			enum wpa_alg alg;
			size_t key_len, kdk_len;

			wpa_hexdump(MSG_DEBUG, "RSNE: STA PMKID",
				    &data.pmkid[i * PMKID_LEN], PMKID_LEN);

			cached_pmk = pmksa_cache_search(
				hapd, pmkid_privacy ? NULL : sta->addr,
				&data.pmkid[i * PMKID_LEN], is_ml);
			if (!cached_pmk)
				continue;

			aa = hapd->own_addr;
			alg = wpa_cipher_to_alg(sta->eap_auth_data.cipher);
			key_len = wpa_cipher_key_len(sta->eap_auth_data.cipher);

#ifdef CONFIG_IEEE80211BE
			if (ap_sta_is_mld(hapd, sta))
				aa = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */
			wpa_printf(MSG_DEBUG,
				   "Found a matching PMKSA cache entry");
			os_memcpy(sta->eap_auth_data.epp_pmkid_cur,
				  cached_pmk->pmkid, PMKID_LEN);
			reply = prepare_802_1x_auth_resp(
				hapd, sta, auth_transaction + 1,
				WLAN_STATUS_SUCCESS, cached_pmk, NULL, 0);
			if (!reply) {
				wpa_printf(MSG_INFO,
					   "Failed to prepare IEEE 802.1X Authentication frame");
				return;
			}

			if (hapd->conf->force_kdk_derivation ||
			    (wpa_auth_ap_support_secure_ltf(hapd->wpa_auth) &&
			     ieee802_11_rsnx_capab(sta->eap_auth_data.rsnxe,
						   WLAN_RSNX_CAPAB_SECURE_LTF)))
				kdk_len = WPA_KDK_MAX_LEN;
			else
				kdk_len = 0;

			if (wpa_auth_802_1x_pmk_to_ptk(
				    cached_pmk->pmk, cached_pmk->pmk_len,
				    sta->addr, aa,
				    sta->eap_auth_data.snonce,
				    sta->eap_auth_data.anonce,
				    sta->eap_auth_data.akm,
				    sta->eap_auth_data.cipher,
				    wpabuf_head_u8(sta->eap_auth_data.dhss),
				    wpabuf_len(sta->eap_auth_data.dhss),
				    &sta->eap_auth_data.ptk, kdk_len)) {
				wpa_printf(MSG_INFO, "Failed to derive PTK");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "PTK derived successfully");

			if (wpa_auth_802_1x_set_key(hapd->wpa_auth, alg,
						    sta->addr,
						    sta->eap_auth_data.ptk.tk,
						    key_len)) {
				wpa_printf(MSG_INFO,
					   "Failed to set TK to driver");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			sta->flags |= WLAN_STA_AUTH;
			sta->auth_alg = WLAN_AUTH_802_1X;
			sta->eap_auth_data.add_mic = true;
			send_8021x_auth_reply(hapd, sta, auth_transaction + 1,
					      WLAN_STATUS_SUCCESS, reply);
			/* Delete DHss after successful PTK derivation */
			wpabuf_clear_free(sta->eap_auth_data.dhss);
			sta->eap_auth_data.dhss = NULL;
			return;
		}

		/* Start EAPOL SM to process EAPOL PDU */
		if (!sta->eapol_sm) {
			sta->eapol_sm = ieee802_1x_alloc_eapol_sm(hapd, sta);
			if (!sta->eapol_sm)
				return;
		}

		ieee802_1x_eapol_sm_set_port_enabled(sta->eapol_sm, true);
	}

	/* Forward the extracted EAP PDU to AS */
	ieee802_1x_receive(hapd, sta->addr, (const u8 *) eapol_pdu,
			   encap_len, FRAME_NOT_ENCRYPTED);
	return;

fail:
	reply = prepare_802_1x_auth_resp(hapd, sta, auth_transaction + 1,
					 resp, NULL, NULL, 0);
	if (reply)
		send_8021x_auth_reply(hapd, sta, auth_transaction + 1, resp,
				      reply);
}

#endif /* CONFIG_IEEE8021X_AUTH */


#ifdef CONFIG_FILS

static void handle_auth_fils_finish(struct hostapd_data *hapd,
				    struct sta_info *sta, u16 resp,
				    struct wpabuf *data, int pub);

void handle_auth_fils(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *pos, size_t len, u16 auth_alg,
		      u16 auth_transaction, u16 status_code,
		      void (*cb)(struct hostapd_data *hapd,
				 struct sta_info *sta, u16 resp,
				 struct wpabuf *data, int pub))
{
	u16 resp = WLAN_STATUS_SUCCESS;
	const u8 *end;
	struct ieee802_11_elems elems;
	enum wpa_validate_result res;
	struct wpa_ie_data rsn;
	struct rsn_pmksa_cache_entry *pmksa = NULL;

	if (auth_transaction != WLAN_AUTH_TR_SEQ_SAE_COMMIT ||
	    status_code != WLAN_STATUS_SUCCESS)
		return;

	end = pos + len;

	wpa_hexdump(MSG_DEBUG, "FILS: Authentication frame fields",
		    pos, end - pos);

	/* TODO: FILS PK */
#ifdef CONFIG_FILS_SK_PFS
	if (auth_alg == WLAN_AUTH_FILS_SK_PFS) {
		u16 group;
		struct wpabuf *pub;
		size_t elem_len;

		/* Using FILS PFS */

		/* Finite Cyclic Group */
		if (end - pos < 2) {
			wpa_printf(MSG_DEBUG,
				   "FILS: No room for Finite Cyclic Group");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		group = WPA_GET_LE16(pos);
		pos += 2;
		if (group != hapd->conf->fils_dh_group) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Unsupported Finite Cyclic Group: %u (expected %u)",
				   group, hapd->conf->fils_dh_group);
			resp = WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
			goto fail;
		}

		crypto_ecdh_deinit(sta->fils_ecdh);
		sta->fils_ecdh = crypto_ecdh_init(group);
		if (!sta->fils_ecdh) {
			wpa_printf(MSG_INFO,
				   "FILS: Could not initialize ECDH with group %d",
				   group);
			resp = WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
			goto fail;
		}

		pub = crypto_ecdh_get_pubkey(sta->fils_ecdh, 1);
		if (!pub) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Failed to derive ECDH public key");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		elem_len = wpabuf_len(pub);
		wpabuf_free(pub);

		/* Element */
		if ((size_t) (end - pos) < elem_len) {
			wpa_printf(MSG_DEBUG, "FILS: No room for Element");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		wpabuf_free(sta->fils_g_sta);
		sta->fils_g_sta = wpabuf_alloc_copy(pos, elem_len);
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = crypto_ecdh_set_peerkey(sta->fils_ecdh, 1,
							  pos, elem_len);
		if (!sta->fils_dh_ss) {
			wpa_printf(MSG_DEBUG, "FILS: ECDH operation failed");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_hexdump_buf_key(MSG_DEBUG, "FILS: DH_SS", sta->fils_dh_ss);
		pos += elem_len;
	} else {
		crypto_ecdh_deinit(sta->fils_ecdh);
		sta->fils_ecdh = NULL;
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = NULL;
	}
#endif /* CONFIG_FILS_SK_PFS */

	wpa_hexdump(MSG_DEBUG, "FILS: Remaining IEs", pos, end - pos);
	if (ieee802_11_parse_elems(pos, end - pos, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "FILS: Could not parse elements");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* RSNE */
	wpa_hexdump(MSG_DEBUG, "FILS: RSN element",
		    elems.rsn_ie, elems.rsn_ie_len);
	if (!elems.rsn_ie ||
	    wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				 &rsn) < 0) {
		wpa_printf(MSG_DEBUG, "FILS: No valid RSN element");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (!sta->wpa_sm)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr,
						NULL);
	if (!sta->wpa_sm) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Failed to initialize RSN state machine");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	wpa_auth_set_rsn_selection(sta->wpa_sm, elems.rsn_selection,
				   elems.rsn_selection_len);
	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  hapd->iface->freq,
				  elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				  elems.rsnxe ? elems.rsnxe - 2 : NULL,
				  elems.rsnxe ? elems.rsnxe_len + 2 : 0,
				  elems.mdie, elems.mdie_len, NULL, 0, NULL,
				  ap_sta_is_mld(hapd, sta), false, NULL, false);
	resp = wpa_res_to_status_code(res);
	if (resp != WLAN_STATUS_SUCCESS)
		goto fail;

	if (!elems.nonce) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Nonce field");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "FILS: SNonce", elems.nonce, NONCE_LEN);
	os_memcpy(sta->fils_snonce, elems.nonce, NONCE_LEN);

	/* PMKID List */
	if (rsn.pmkid && rsn.num_pmkid > 0) {
		u8 num;
		const u8 *pmkid;

		wpa_hexdump(MSG_DEBUG, "FILS: PMKID List",
			    rsn.pmkid, rsn.num_pmkid * PMKID_LEN);

		pmkid = rsn.pmkid;
		num = rsn.num_pmkid;
		while (num) {
			wpa_hexdump(MSG_DEBUG, "FILS: PMKID", pmkid, PMKID_LEN);
			pmksa = wpa_auth_pmksa_get(hapd->wpa_auth, sta->addr,
						   pmkid);
			if (pmksa)
				break;
			pmksa = wpa_auth_pmksa_get_fils_cache_id(hapd->wpa_auth,
								 sta->addr,
								 pmkid);
			if (pmksa)
				break;
			pmkid += PMKID_LEN;
			num--;
		}
	}
	if (pmksa && wpa_auth_sta_key_mgmt(sta->wpa_sm) != pmksa->akmp) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Matching PMKSA cache entry has different AKMP (0x%x != 0x%x) - ignore",
			   wpa_auth_sta_key_mgmt(sta->wpa_sm), pmksa->akmp);
		pmksa = NULL;
	}
	if (pmksa)
		wpa_printf(MSG_DEBUG, "FILS: Found matching PMKSA cache entry");

	/* FILS Session */
	if (!elems.fils_session) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Session element");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "FILS: FILS Session", elems.fils_session,
		    FILS_SESSION_LEN);
	os_memcpy(sta->fils_session, elems.fils_session, FILS_SESSION_LEN);

	/* Wrapped Data */
	if (elems.wrapped_data) {
		wpa_hexdump(MSG_DEBUG, "FILS: Wrapped Data",
			    elems.wrapped_data,
			    elems.wrapped_data_len);
		if (!pmksa) {
#ifndef CONFIG_NO_RADIUS
			if (!sta->eapol_sm) {
				sta->eapol_sm =
					ieee802_1x_alloc_eapol_sm(hapd, sta);
			}
			wpa_printf(MSG_DEBUG,
				   "FILS: Forward EAP-Initiate/Re-auth to authentication server");
			ieee802_1x_encapsulate_radius(
				hapd, sta, elems.wrapped_data,
				elems.wrapped_data_len);
			sta->fils_pending_cb = cb;
			wpa_printf(MSG_DEBUG,
				   "FILS: Will send Authentication frame once the response from authentication server is available");
			sta->flags |= WLAN_STA_PENDING_FILS_ERP;
			/* Calculate pending PMKID here so that we do not need
			 * to maintain a copy of the EAP-Initiate/Reauth
			 * message. */
			if (fils_pmkid_erp(wpa_auth_sta_key_mgmt(sta->wpa_sm),
					   elems.wrapped_data,
					   elems.wrapped_data_len,
					   sta->fils_erp_pmkid) == 0)
				sta->fils_erp_pmkid_set = 1;
			return;
#else /* CONFIG_NO_RADIUS */
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
#endif /* CONFIG_NO_RADIUS */
		}
	}

	if (hapd->conf->security_profiles) {
		resp = validate_security_profile_common(hapd, sta, &elems, "FILS", NULL);
		if (resp)
			goto fail;
	}

fail:
	if (cb) {
		struct wpabuf *data;
		int pub = 0;

		data = prepare_auth_resp_fils(hapd, sta, &resp, pmksa, NULL,
					      NULL, 0, &pub);
		if (!data) {
			wpa_printf(MSG_DEBUG,
				   "%s: prepare_auth_resp_fils() returned failure",
				   __func__);
		}

		cb(hapd, sta, resp, data, pub);
	}
}


static struct wpabuf *
prepare_auth_resp_fils(struct hostapd_data *hapd,
		       struct sta_info *sta, u16 *resp,
		       struct rsn_pmksa_cache_entry *pmksa,
		       struct wpabuf *erp_resp,
		       const u8 *msk, size_t msk_len,
		       int *is_pub)
{
	u8 fils_nonce[NONCE_LEN];
	size_t ielen;
	struct wpabuf *data = NULL;
	const u8 *ie;
	u8 *ie_buf = NULL;
	const u8 *pmk = NULL;
	size_t pmk_len = 0;
	u8 pmk_buf[PMK_LEN_MAX];
	struct wpabuf *pub = NULL;

	if (*resp != WLAN_STATUS_SUCCESS)
		goto fail;

	ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ielen);
	if (!ie) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (pmksa) {
		/* Add PMKID of the selected PMKSA into RSNE */
		ie_buf = os_malloc(ielen + 2 + 2 + PMKID_LEN);
		if (!ie_buf) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		os_memcpy(ie_buf, ie, ielen);
		if (wpa_insert_pmkid(ie_buf, &ielen, pmksa->pmkid, true) < 0) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		ie = ie_buf;
	}

	if (random_get_bytes(fils_nonce, NONCE_LEN) < 0) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "RSN: Generated FILS Nonce",
		    fils_nonce, NONCE_LEN);

#ifdef CONFIG_FILS_SK_PFS
	if (sta->fils_dh_ss && sta->fils_ecdh) {
		pub = crypto_ecdh_get_pubkey(sta->fils_ecdh, 1);
		if (!pub) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}
#endif /* CONFIG_FILS_SK_PFS */

	data = wpabuf_alloc(1000 + ielen + (pub ? wpabuf_len(pub) : 0));
	if (!data) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* TODO: FILS PK */
#ifdef CONFIG_FILS_SK_PFS
	if (pub) {
		/* Finite Cyclic Group */
		wpabuf_put_le16(data, hapd->conf->fils_dh_group);

		/* Element */
		wpabuf_put_buf(data, pub);
	}
#endif /* CONFIG_FILS_SK_PFS */

	/* RSNE */
	wpabuf_put_data(data, ie, ielen);

	/* MDE when using FILS+FT (already included in ie,ielen with RSNE) */

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(wpa_auth_sta_key_mgmt(sta->wpa_sm))) {
		/* FTE[R1KH-ID,R0KH-ID] when using FILS+FT */
		int res;

		res = wpa_auth_write_fte(hapd->wpa_auth, sta->wpa_sm,
					 wpabuf_put(data, 0),
					 wpabuf_tailroom(data));
		if (res < 0) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpabuf_put(data, res);
	}
#endif /* CONFIG_IEEE80211R_AP */

	/* FILS Nonce */
	wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
	wpabuf_put_u8(data, 1 + NONCE_LEN); /* Length */
	/* Element ID Extension */
	wpabuf_put_u8(data, WLAN_EID_EXT_NONCE);
	wpabuf_put_data(data, fils_nonce, NONCE_LEN);

	/* FILS Session */
	wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
	wpabuf_put_u8(data, 1 + FILS_SESSION_LEN); /* Length */
	/* Element ID Extension */
	wpabuf_put_u8(data, WLAN_EID_EXT_FILS_SESSION);
	wpabuf_put_data(data, sta->fils_session, FILS_SESSION_LEN);

	/* Wrapped Data */
	if (!pmksa && erp_resp) {
		wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
		wpabuf_put_u8(data, 1 + wpabuf_len(erp_resp)); /* Length */
		/* Element ID Extension */
		wpabuf_put_u8(data, WLAN_EID_EXT_WRAPPED_DATA);
		wpabuf_put_buf(data, erp_resp);

		if (fils_rmsk_to_pmk(wpa_auth_sta_key_mgmt(sta->wpa_sm),
				     msk, msk_len, sta->fils_snonce, fils_nonce,
				     sta->fils_dh_ss ?
				     wpabuf_head(sta->fils_dh_ss) : NULL,
				     sta->fils_dh_ss ?
				     wpabuf_len(sta->fils_dh_ss) : 0,
				     pmk_buf, &pmk_len)) {
			wpa_printf(MSG_DEBUG, "FILS: Failed to derive PMK");
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			wpabuf_free(data);
			data = NULL;
			goto fail;
		}
		pmk = pmk_buf;

		/* Don't use DHss in PTK derivation if PMKSA caching is not
		 * used. */
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = NULL;

		if (sta->fils_erp_pmkid_set) {
			/* TODO: get PMKLifetime from WPA parameters */
			unsigned int dot11RSNAConfigPMKLifetime = 43200;
			int session_timeout;

			session_timeout = dot11RSNAConfigPMKLifetime;
			if (sta->session_timeout_set) {
				struct os_reltime now, diff;

				os_get_reltime(&now);
				os_reltime_sub(&sta->session_timeout, &now,
					       &diff);
				session_timeout = diff.sec;
			}

			sta->fils_erp_pmkid_set = 0;
			wpa_auth_add_fils_pmk_pmkid(sta->wpa_sm, pmk, pmk_len,
						    sta->fils_erp_pmkid);
			if (!hapd->conf->disable_pmksa_caching &&
			    wpa_auth_pmksa_add2(
				    hapd->wpa_auth, sta->addr,
				    pmk, pmk_len,
				    sta->fils_erp_pmkid,
				    session_timeout,
				    wpa_auth_sta_key_mgmt(sta->wpa_sm),
				    NULL, ap_sta_is_mld(hapd, sta)) < 0) {
				wpa_printf(MSG_ERROR,
					   "FILS: Failed to add PMKSA cache entry based on ERP");
			}
		}
	} else if (pmksa) {
		pmk = pmksa->pmk;
		pmk_len = pmksa->pmk_len;
	}

	if (!pmk) {
		wpa_printf(MSG_DEBUG, "FILS: No PMK available");
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		wpabuf_free(data);
		data = NULL;
		goto fail;
	}

	if (fils_auth_pmk_to_ptk(sta->wpa_sm, pmk, pmk_len,
				 sta->fils_snonce, fils_nonce,
				 sta->fils_dh_ss ?
				 wpabuf_head(sta->fils_dh_ss) : NULL,
				 sta->fils_dh_ss ?
				 wpabuf_len(sta->fils_dh_ss) : 0,
				 sta->fils_g_sta, pub) < 0) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		wpabuf_free(data);
		data = NULL;
		goto fail;
	}

fail:
	if (is_pub)
		*is_pub = pub != NULL;
	os_free(ie_buf);
	wpabuf_free(pub);
	wpabuf_clear_free(sta->fils_dh_ss);
	sta->fils_dh_ss = NULL;
#ifdef CONFIG_FILS_SK_PFS
	crypto_ecdh_deinit(sta->fils_ecdh);
	sta->fils_ecdh = NULL;
#endif /* CONFIG_FILS_SK_PFS */
	return data;
}


static void handle_auth_fils_finish(struct hostapd_data *hapd,
				    struct sta_info *sta, u16 resp,
				    struct wpabuf *data, int pub)
{
	u16 auth_alg;

	auth_alg = (pub ||
		    resp == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED) ?
		WLAN_AUTH_FILS_SK_PFS : WLAN_AUTH_FILS_SK;
	send_auth_reply(hapd, sta, sta->mld_auth ? sta->reply_addr : sta->addr, auth_alg,
			2, resp, data ? wpabuf_head(data) : (u8 *) "",
			data ? wpabuf_len(data) : 0, "auth-fils-finish");
	wpabuf_free(data);

	if (resp == WLAN_STATUS_SUCCESS) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication OK (FILS)");
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = pub ? WLAN_AUTH_FILS_SK_PFS : WLAN_AUTH_FILS_SK;
		mlme_authenticate_indication(hapd, sta);
	}
}


void ieee802_11_finish_fils_auth(struct hostapd_data *hapd,
				 struct sta_info *sta, int success,
				 struct wpabuf *erp_resp,
				 const u8 *msk, size_t msk_len)
{
	u16 resp;
	u32 flags = sta->flags;

	sta->flags &= ~(WLAN_STA_PENDING_FILS_ERP |
			WLAN_STA_PENDING_PASN_FILS_ERP);

	resp = success ? WLAN_STATUS_SUCCESS : WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (flags & WLAN_STA_PENDING_FILS_ERP) {
		struct wpabuf *data;
		int pub = 0;

		if (!sta->fils_pending_cb)
			return;

		data = prepare_auth_resp_fils(hapd, sta, &resp, NULL, erp_resp,
					      msk, msk_len, &pub);
		if (!data) {
			wpa_printf(MSG_DEBUG,
				   "%s: prepare_auth_resp_fils() failure",
				   __func__);
		}
		sta->fils_pending_cb(hapd, sta, resp, data, pub);
#ifdef CONFIG_PASN
	} else if (flags & WLAN_STA_PENDING_PASN_FILS_ERP) {
		pasn_fils_auth_resp(hapd, sta, resp, erp_resp,
				    msk, msk_len);
#endif /* CONFIG_PASN */
	}
}

#endif /* CONFIG_FILS */


static int ieee802_11_allowed_address(struct hostapd_data *hapd, const u8 *addr,
				      const u8 *msg, size_t len,
				      struct radius_sta *info)
{
	int res;

	res = hostapd_allowed_address(hapd, addr, msg, len, info, 0);

	if (res == HOSTAPD_ACL_REJECT) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not allowed to authenticate",
			   MAC2STR(addr));
		return HOSTAPD_ACL_REJECT;
	}

	if (res == HOSTAPD_ACL_PENDING) {
		wpa_printf(MSG_DEBUG, "Authentication frame from " MACSTR
			   " waiting for an external authentication",
			   MAC2STR(addr));
		/* Authentication code will re-send the authentication frame
		 * after it has received (and cached) information from the
		 * external source. */
		return HOSTAPD_ACL_PENDING;
	}

	return res;
}


int ieee802_11_set_radius_info(struct hostapd_data *hapd, struct sta_info *sta,
			       int res, struct radius_sta *info)
{
	u32 session_timeout = info->session_timeout;
	u32 acct_interim_interval = info->acct_interim_interval;
	struct vlan_description *vlan_id = &info->vlan_id;
	struct hostapd_sta_wpa_psk_short *psk = info->psk;
	char *identity = info->identity;
	char *radius_cui = info->radius_cui;

	if (vlan_id->notempty &&
	    !hostapd_vlan_valid(hapd->conf->vlan, vlan_id)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO,
			       "Invalid VLAN %d%s received from RADIUS server",
			       vlan_id->untagged,
			       vlan_id->tagged[0] ? "+" : "");
		return -1;
	}
	if (ap_sta_set_vlan(hapd, sta, vlan_id) < 0)
		return -1;
	if (sta->vlan_id)
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO, "VLAN ID %d", sta->vlan_id);

	hostapd_free_psk_list(sta->psk);
	if (hapd->conf->wpa_psk_radius != PSK_RADIUS_IGNORED)
		hostapd_copy_psk_list(&sta->psk, psk);
	else
		sta->psk = NULL;

	os_free(sta->identity);
	if (identity)
		sta->identity = os_strdup(identity);
	else
		sta->identity = NULL;

	os_free(sta->radius_cui);
	if (radius_cui)
		sta->radius_cui = os_strdup(radius_cui);
	else
		sta->radius_cui = NULL;

	if (hapd->conf->acct_interim_interval == 0 && acct_interim_interval)
		sta->acct_interim_interval = acct_interim_interval;
	if (res == HOSTAPD_ACL_ACCEPT_TIMEOUT) {
		sta->session_timeout_set = 1;
		os_get_reltime(&sta->session_timeout);
		sta->session_timeout.sec += session_timeout;
		ap_sta_session_timeout(hapd, sta, session_timeout);
	} else {
		sta->session_timeout_set = 0;
		ap_sta_no_session_timeout(hapd, sta);
	}

	return 0;
}


#ifdef CONFIG_PASN
#ifdef CONFIG_FILS

static void pasn_fils_auth_resp(struct hostapd_data *hapd,
				struct sta_info *sta, u16 status,
				struct wpabuf *erp_resp,
				const u8 *msk, size_t msk_len)
{
	struct pasn_data *pasn = sta->pasn;
	struct pasn_fils *fils = &pasn->fils;
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	int ret;

	wpa_printf(MSG_DEBUG, "PASN: FILS: Handle AS response - status=%u",
		   status);

	if (status != WLAN_STATUS_SUCCESS)
		goto fail;

	if (!pasn->secret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing secret");
		goto fail;
	}

	if (random_get_bytes(fils->anonce, NONCE_LEN) < 0) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to get ANonce");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "RSN: Generated FILS ANonce",
		    fils->anonce, NONCE_LEN);

	ret = fils_rmsk_to_pmk(pasn_get_akmp(pasn), msk, msk_len, fils->nonce,
			       fils->anonce, NULL, 0, pmk, &pmk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG, "FILS: Failed to derive PMK");
		goto fail;
	}

	ret = pasn_pmk_to_ptk(pmk, pmk_len, sta->addr, hapd->own_addr,
			      wpabuf_head(pasn->secret),
			      wpabuf_len(pasn->secret),
			      pasn_get_ptk(sta->pasn), pasn_get_akmp(sta->pasn),
			      pasn_get_cipher(sta->pasn), sta->pasn->kdk_len,
			      sta->pasn->kek_len, &sta->pasn->hash_alg,
			      pasn->auth_alg == WLAN_AUTH_EPPKE);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to derive PTK");
		goto fail;
	}

	if (pasn->secure_ltf) {
		ret = wpa_ltf_keyseed(pasn_get_ptk(pasn), pasn_get_akmp(pasn),
				      pasn_get_cipher(pasn));
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: FILS: Failed to derive LTF keyseed");
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "PASN: PTK successfully derived");

	wpabuf_free(pasn->secret);
	pasn->secret = NULL;

	fils->erp_resp = erp_resp;
	ret = handle_auth_pasn_resp(sta->pasn, hapd->own_addr, sta->addr, NULL,
				    WLAN_STATUS_SUCCESS);
	wpabuf_free(pasn->frame);
	pasn->frame = NULL;
	fils->erp_resp = NULL;

	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to send response");
		goto fail;
	}

	fils->state = PASN_FILS_STATE_COMPLETE;
	return;
fail:
	ap_free_sta(hapd, sta);
}


static int pasn_wd_handle_fils(struct hostapd_data *hapd, struct sta_info *sta,
			       struct wpabuf *wd)
{
#ifdef CONFIG_NO_RADIUS
	wpa_printf(MSG_DEBUG, "PASN: FILS: RADIUS is not configured. Fail");
	return -1;
#else /* CONFIG_NO_RADIUS */
	struct pasn_data *pasn = sta->pasn;
	struct pasn_fils *fils = &pasn->fils;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsne_data;
	struct wpabuf *fils_wd;
	const u8 *data;
	size_t buf_len;
	u16 alg, seq, status;
	int ret;

	if (fils->state != PASN_FILS_STATE_NONE) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Not expecting wrapped data");
		return -1;
	}

	if (!wd) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: No wrapped data");
		return -1;
	}

	data = wpabuf_head_u8(wd);
	buf_len = wpabuf_len(wd);

	if (buf_len < 6) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Buffer too short. len=%zu",
			   buf_len);
		return -1;
	}

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: FILS: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_FILS_SK || seq != 1 ||
	    status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: Dropping peer authentication");
		return -1;
	}

	data += 6;
	buf_len -= 6;

	if (ieee802_11_parse_elems(data, buf_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Could not parse elements");
		return -1;
	}

	if (!elems.rsn_ie || !elems.nonce || !elems.nonce ||
	    !elems.wrapped_data || !elems.fils_session) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing IEs");
		return -1;
	}

	ret = wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				   &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed parsing RSNE");
		return -1;
	}

	ret = wpa_pasn_validate_rsne(&rsne_data, false);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed validating RSNE");
		return -1;
	}

	if (rsne_data.num_pmkid) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: Not expecting PMKID in RSNE");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Nonce", elems.nonce, NONCE_LEN);
	os_memcpy(fils->nonce, elems.nonce, NONCE_LEN);

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Session", elems.fils_session,
		    FILS_SESSION_LEN);
	os_memcpy(fils->session, elems.fils_session, FILS_SESSION_LEN);

	fils_wd = ieee802_11_defrag(elems.wrapped_data, elems.wrapped_data_len,
				    true);

	if (!fils_wd) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing wrapped data");
		return -1;
	}

	if (!sta->eapol_sm)
		sta->eapol_sm = ieee802_1x_alloc_eapol_sm(hapd, sta);

	wpa_printf(MSG_DEBUG,
		   "PASN: FILS: Forward EAP-Initiate/Re-auth to AS");

	ieee802_1x_encapsulate_radius(hapd, sta, wpabuf_head(fils_wd),
				      wpabuf_len(fils_wd));

	sta->flags |= WLAN_STA_PENDING_PASN_FILS_ERP;

	fils->state = PASN_FILS_STATE_PENDING_AS;

	/*
	 * Calculate pending PMKID here so that we do not need to maintain a
	 * copy of the EAP-Initiate/Reautt message.
	 */
	fils_pmkid_erp(pasn_get_akmp(pasn),
		       wpabuf_head(fils_wd), wpabuf_len(fils_wd),
		       fils->erp_pmkid);

	wpabuf_free(fils_wd);
	return 0;
#endif /* CONFIG_NO_RADIUS */
}

#endif /* CONFIG_FILS */


static int hapd_pasn_send_mlme(void *ctx, const u8 *data, size_t data_len,
			       int noack, unsigned int freq, unsigned int wait)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_send_mlme(hapd, data, data_len, 0, NULL, 0, 0, 0, 0);
}


static struct rsn_pmksa_cache_entry *
pmksa_cache_search(void *ctx, const u8 *spa, const u8 *pmkid, bool is_ml)
{
	struct hostapd_data *hapd = ctx;
	struct rsn_pmksa_cache_entry *entry;
	struct rsn_pmksa_cache *pmksa = wpa_auth_get_pmksa_cache(hapd->wpa_auth,
								 is_ml);

	entry = pmksa_cache_auth_get(pmksa, spa, pmkid);
	if (entry)
		return entry;

#ifdef CONFIG_IEEE80211BE
	if (is_ml) {
		struct hostapd_data *tmp_hapd;

		/* Search in link caches of each affiliated AP MLD link */
		for_each_mld_link(tmp_hapd, hapd) {
			pmksa = wpa_auth_get_pmksa_cache(tmp_hapd->wpa_auth,
							 false);
			entry = pmksa_cache_auth_get(pmksa, spa, pmkid);
			if (entry)
				return entry;
		}
	} else if (hapd->conf->mld_ap) {
		/* Search in the MLD cache */
		pmksa = wpa_auth_get_pmksa_cache(hapd->wpa_auth, true);
		entry = pmksa_cache_auth_get(pmksa, spa, pmkid);
		if (entry)
			return entry;
	}
#endif /* CONFIG_IEEE80211BE */

	return NULL;
}


#ifdef CONFIG_ENC_ASSOC
static int eppke_set_key(void *ctx, enum wpa_alg alg, const u8 *addr,
			 int vlan_id, const u8 *key, size_t key_len)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_set_key(hapd->conf->iface, hapd, alg, addr,
				   0, vlan_id, 1, NULL, 0, key, key_len,
				   KEY_FLAG_PAIRWISE_RX_TX);
}
#else /* CONFIG_ENC_ASSOC */
#define eppke_set_key NULL
#endif /* CONFIG_ENC_ASSOC */


#ifdef CONFIG_SAE
/**
 * hapd_pasn_get_pt_for_pw_id - Look up SAE PT for a password identifier
 *
 * Called by the PASN responder when an SAE commit frame contains a password
 * identifier that was not known at PASN-setup time (e.g., for EPPKE where
 * the PT cannot be pre-selected before the commit is received).
 *
 * For plaintext identifiers sae_get_password() returns the pre-computed PT
 * (pw_entry->pt). We must NOT return that pointer directly because the
 * caller will free it; clone it so the caller always owns the returned PT.
 * counter and dec_pw_id are set to 0/NULL for plaintext identifiers.
 *
 * For encrypted password identifiers the PT is not pre-computed (the
 * pre-computed PT is keyed to the decrypted identifier, not the encrypted
 * one). Derive the PT on-the-fly using the raw (encrypted) identifier as
 * the salt, mirroring what auth_build_sae_commit() does for regular SAE.
 * The decrypted blob is parsed to extract the real password identifier
 * (dec_pw_id) and the counter, both returned to the caller. dec_pw_id is
 * heap-allocated and the caller takes ownership (must free with os_free()).
 */
static struct sae_pt *
hapd_pasn_get_pt_for_pw_id(void *ctx, const u8 *pw_id, size_t pw_id_len,
			    int group, const char **password,
			    unsigned int *counter,
			    u8 **dec_pw_id, size_t *dec_pw_id_len)
{
	struct hostapd_data *hapd = ctx;
	struct sae_password_entry *pw_entry = NULL;
	struct sae_pt *pt = NULL;
	int groups[2] = { group, 0 };

	*counter = 0;
	*dec_pw_id = NULL;
	*dec_pw_id_len = 0;

	*password = sae_get_password(hapd, NULL, pw_id, pw_id_len, &pw_entry,
				     &pt, NULL);
	if (!*password)
		return NULL;

	if (pt) {
		/* Plaintext identifier: sae_get_password() found a
		 * pre-computed PT.  Clone it so the caller can free it
		 * without affecting the password entry's own PT.
		 * counter and dec_pw_id stay 0/NULL for plaintext. */
		return sae_derive_pt(groups, hapd->conf->ssid.ssid,
				     hapd->conf->ssid.ssid_len,
				     (const u8 *) pw_entry->password,
				     os_strlen(pw_entry->password),
				     pw_id, pw_id_len);
	}

	if (pw_entry) {
		/* Encrypted identifier: no pre-computed PT exists for the
		 * raw (encrypted) identifier. Decrypt the blob once to
		 * extract the real password identifier (dec_pw_id) and the
		 * counter, then derive the PT using the encrypted bytes as
		 * the salt (matching auth_build_sae_commit()).
		 *
		 * Decrypted format:
		 *   4-byte date | Password ID | NUL padding | 4-byte counter
		 */
		if (hapd->conf->sae_pw_id_key &&
		    pw_id_len > 4 + 4 + AES_BLOCK_SIZE) {
			u8 *plain;
			size_t plain_len;

			plain = os_malloc(pw_id_len);
			if (plain &&
			    aes_siv_decrypt(
				    wpabuf_head(hapd->conf->sae_pw_id_key),
				    wpabuf_len(hapd->conf->sae_pw_id_key),
				    pw_id, pw_id_len,
				    0, NULL, NULL, plain) == 0) {
				const u8 *id, *pos;

				plain_len = pw_id_len - AES_BLOCK_SIZE;
				/* Counter is the last 4 bytes */
				*counter = WPA_GET_BE32(plain + plain_len - 4);
				wpa_printf(MSG_DEBUG,
					   "SAE: Generation time %u counter %u",
					   WPA_GET_BE32(plain), *counter);
				/* Real password ID starts at byte 4,
				 * NUL-terminated before the counter */
				id = plain + 4;
				pos = id;
				while (pos < plain + plain_len - 4) {
					if (*pos == 0x00)
						break;
					pos++;
				}
				*dec_pw_id_len = pos - id;
				wpa_hexdump_ascii(
					MSG_DEBUG,
					"SAE: Decrypted password identifier",
					id, *dec_pw_id_len);
				*dec_pw_id = os_memdup(id, *dec_pw_id_len);
				if (!*dec_pw_id)
					*dec_pw_id_len = 0;
			}
			os_free(plain);
		}

		pt = sae_derive_pt(groups, hapd->conf->ssid.ssid,
				   hapd->conf->ssid.ssid_len,
				   (const u8 *) pw_entry->password,
				   os_strlen(pw_entry->password),
				   pw_id, pw_id_len);
		if (!pt)
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed to derive PT for encrypted password identifier");
		return pt;
	}

	return NULL;
}
#endif /* CONFIG_SAE */


static void hapd_initialize_pasn(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct pasn_data *pasn = sta->pasn;

	pasn_register_callbacks(pasn, hapd, hapd_pasn_send_mlme,
				NULL, eppke_set_key, pmksa_cache_search);
	pasn_set_bssid(pasn, hapd->own_addr);
	pasn_set_own_addr(pasn, hapd->own_addr);
#ifdef CONFIG_PMKSA_PRIVACY
	pasn->pmksa_caching_privacy = hapd->conf->pmksa_caching_privacy;
#endif /* CONFIG_PMKSA_PRIVACY */
#if defined(CONFIG_IEEE80211BE) && defined(CONFIG_ENC_ASSOC)
	/* Per IEEE802.11bi/D4.0, 12.16.9 (Enhanced privacy
	 * protection key exchange), if (Re)Association frame
	 * Encryption is activated, KEK in PASN shall be true.
	 */
	if (hapd->conf->assoc_frame_encryption)
		pasn->derive_kek = true;
	if (hapd->conf->mld_ap)
		pasn_set_own_mld_addr(pasn, hapd->mld->mld_addr);
#endif /* CONFIG_IEEE80211BE && CONFIG_ENC_ASSOC */
	pasn_set_peer_addr(pasn, sta->addr);
	pasn_set_wpa_key_mgmt(pasn, hapd->conf->wpa_key_mgmt |
			      hostapd_sp_implied_key_mgmt(hapd->conf));
	pasn_set_rsn_pairwise(pasn, hapd->conf->rsn_pairwise);
	pasn_set_mfp(pasn, hapd->conf->ieee80211w);
	os_free(pasn->pasn_groups);
	pasn->pasn_groups = int_array_dup(hapd->conf->pasn_groups);
	pasn->noauth = hapd->conf->pasn_noauth;
#ifdef CONFIG_ENC_ASSOC
	pasn->eppke_unauth = hapd->conf->eppke_unauth;
#endif /* CONFIG_ENC_ASSOC */
	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_AP)
		pasn_enable_kdk_derivation(pasn);

#ifdef CONFIG_TESTING_OPTIONS
	pasn->corrupt_mic = hapd->conf->pasn_corrupt_mic;
	if (hapd->conf->force_kdk_derivation)
		pasn_enable_kdk_derivation(pasn);
#endif /* CONFIG_TESTING_OPTIONS */
	pasn->use_anti_clogging = use_anti_clogging(hapd);
	pasn_set_password(pasn, sae_get_password(hapd, sta, NULL, 0, NULL,
						 &pasn->pt, NULL));
#ifdef CONFIG_SAE
	/* Register a callback so the PASN responder can look up the correct
	 * SAE PT when the STA's commit frame contains a password identifier
	 * that was not known at setup time (EPPKE cases).
	 */
	pasn->get_pt_for_pw_id = hapd_pasn_get_pt_for_pw_id;
#endif /* CONFIG_SAE */
	pasn_set_rsne(pasn, wpa_auth_get_wpa_ie(hapd->wpa_auth,
						&pasn->rsn_ie_len));
	pasn_set_rsnxe_ie(pasn, hostapd_wpa_ie(hapd, WLAN_EID_RSNX));
	pasn->disable_pmksa_caching = hapd->conf->disable_pmksa_caching;
#ifdef CONFIG_ENC_ASSOC
	pasn->tk_configured = false;
#endif /* CONFIG_ENC_ASSOC */
	pasn_set_responder_pmksa(
		pasn,
		wpa_auth_get_pmksa_cache(hapd->wpa_auth,
					 ap_sta_is_epp(sta) ?
					 ap_sta_is_mld(hapd, sta) : false));

	pasn->comeback_after = hapd->conf->pasn_comeback_after;
	pasn->comeback_idx = hapd->comeback_idx;
	pasn->comeback_key =  hapd->comeback_key;
	pasn->comeback_pending_idx = hapd->comeback_pending_idx;
}


static int pasn_set_keys_from_cache(struct hostapd_data *hapd,
				    const u8 *own_addr, const u8 *sta_addr,
				    int cipher, int akmp)
{
	struct ptksa_cache_entry *entry;

	entry = ptksa_cache_get(hapd->ptksa, sta_addr, cipher);
	if (!entry) {
		wpa_printf(MSG_DEBUG, "PASN: peer " MACSTR
			   " not present in PTKSA cache", MAC2STR(sta_addr));
		return -1;
	}

	if (!ether_addr_equal(entry->own_addr, own_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: own addr " MACSTR " and PTKSA entry own addr "
			   MACSTR " differ",
			   MAC2STR(own_addr), MAC2STR(entry->own_addr));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: " MACSTR " present in PTKSA cache",
		   MAC2STR(sta_addr));
	hostapd_drv_set_secure_ranging_ctx(hapd, own_addr, sta_addr, cipher,
					   entry->ptk.tk_len, entry->ptk.tk,
					   entry->ptk.ltf_keyseed_len,
					   entry->ptk.ltf_keyseed, 0);

	return 0;
}


static void hapd_pasn_update_params(struct hostapd_data *hapd,
				    struct sta_info *sta,
				    const struct ieee80211_mgmt *mgmt,
				    size_t len)
{
	struct pasn_data *pasn = sta->pasn;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsn_data;
#ifdef CONFIG_FILS
	struct wpa_pasn_params_data pasn_params;
	struct wpabuf *wrapped_data = NULL;
#endif /* CONFIG_FILS */
	int akmp;
	int has_security_profiles = hapd->conf->security_profiles ? 1 : 0;

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		return;
	}

	if (!elems.rsn_ie ||
	    wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				 &rsn_data)) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE");
		return;
	}

	if (!(rsn_data.key_mgmt & pasn->wpa_key_mgmt)) {
		wpa_printf(MSG_DEBUG, "PASN: Mismatch in AKMP");
		return;
	}

	if (has_security_profiles && sta->sp_ie_validated) {
		if (!(rsn_data.pairwise_cipher & (WPA_CIPHER_GCMP_256))) {
			wpa_printf(MSG_DEBUG, "PASN: Pairwise cipher not allowed for Security Profile");
			return;
		}
	} else {
		if (!(rsn_data.pairwise_cipher & pasn->rsn_pairwise)) {
			wpa_printf(MSG_DEBUG, "PASN: Mismatch in cipher");
			return;
		}
	}

#ifdef CONFIG_QCN_EXTN
	os_memcpy(pasn->reply_addr, mgmt->sa, ETH_ALEN);
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_ENC_ASSOC
	pasn->auth_alg = mgmt->u.auth.auth_alg;
	pasn->authorized = ap_sta_is_authorized(sta);
#ifdef CONFIG_IEEE80211BE
	pasn->is_ml_peer = sta->mld_info.mld_sta;
#endif /* CONFIG_IEEE80211BE */
#endif /* CONFIG_ENC_ASSOC */

	pasn_set_akmp(pasn, rsn_data.key_mgmt);
	pasn_set_cipher(pasn, rsn_data.pairwise_cipher);

	if (pasn->derive_kdk &&
	    !ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len,
				       WLAN_RSNX_CAPAB_SECURE_LTF))
		pasn_disable_kdk_derivation(pasn);
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->force_kdk_derivation)
		pasn_enable_kdk_derivation(pasn);
#endif /* CONFIG_TESTING_OPTIONS */
	akmp = pasn_get_akmp(pasn);

	if (wpa_key_mgmt_ft(akmp) && rsn_data.num_pmkid) {
#ifdef CONFIG_IEEE80211R_AP
		pasn->pmk_r1_len = 0;
		wpa_ft_fetch_pmk_r1(hapd->wpa_auth, sta->addr,
				    rsn_data.pmkid,
				    pasn->pmk_r1, &pasn->pmk_r1_len, NULL,
				    NULL, NULL, NULL,
				    NULL, NULL, NULL);
#endif /* CONFIG_IEEE80211R_AP */
	}
#ifdef CONFIG_FILS
	if (akmp != WPA_KEY_MGMT_FILS_SHA256 &&
	    akmp != WPA_KEY_MGMT_FILS_SHA384)
		return;
	if (!elems.pasn_params ||
	    wpa_pasn_parse_parameter_ie(elems.pasn_params - 3,
					elems.pasn_params_len + 3,
					false, &pasn_params)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed validation of PASN Parameters element");
		return;
	}
	if (pasn_params.wrapped_data_format != WPA_PASN_WRAPPED_DATA_NO) {
		wrapped_data = ieee802_11_defrag(elems.wrapped_data,
						 elems.wrapped_data_len, true);
		if (!wrapped_data) {
			wpa_printf(MSG_DEBUG, "PASN: Missing wrapped data");
			return;
		}
		if (pasn_wd_handle_fils(hapd, sta, wrapped_data))
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed processing FILS wrapped data");
		else
			pasn->fils_wd_valid = true;
	}
	wpabuf_free(wrapped_data);
#endif /* CONFIG_FILS */
}


static void handle_auth_pasn(struct hostapd_data *hapd, struct sta_info *sta,
			     const struct ieee80211_mgmt *mgmt, size_t len,
			     u16 trans_seq, u16 status)
{
	int ret;
#ifdef CONFIG_P2P
	struct ieee802_11_elems elems;

	if (len < 24) {
		wpa_printf(MSG_DEBUG, "PASN: Too short Management frame");
		return;
	}

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		return;
	}

	if ((hapd->conf->p2p & (P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    (P2P_ENABLED | P2P_GROUP_OWNER) &&
	    hapd->p2p && elems.p2p2_ie && elems.p2p2_ie_len) {
		p2p_pasn_auth_rx(hapd->p2p, mgmt, len, hapd->iface->freq);
		return;
	}
#endif /* CONFIG_P2P */

	if (hapd->conf->wpa != WPA_PROTO_RSN) {
		wpa_printf(MSG_INFO, "PASN: RSN is not configured");
		return;
	}

	wpa_printf(MSG_INFO, "PASN authentication: sta=" MACSTR,
		   MAC2STR(sta->addr));

	if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH1) {
		if (sta->pasn && sta->auth_alg != WLAN_AUTH_EPPKE) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Not expecting transaction == 1");
			return;
		}

		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failure status in transaction == 1");
			return;
		}

		if (!sta->pasn)
			sta->pasn = pasn_data_init();
		if (!sta->pasn) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed to allocate PASN context");
			return;
		}

		hapd_initialize_pasn(hapd, sta);

		/* UHR Security Profile validation during PASN frame 1.
		 * Spec: if the first Authentication frame includes RSNE, RSNXE,
		 * and a Security Profile element the AP must verify it matches
		 * an advertised profile; reject with
		 * REJECTED_INVALID_SECURITY_PROFILE on mismatch.
		 */
		if (hapd->conf->security_profiles) {
			const u8 *var = mgmt->u.auth.variable;
			size_t var_len = ((const u8 *) mgmt) + len - var;

			struct ieee802_11_elems elems;
			if (ieee802_11_parse_elems(var, var_len, &elems, 1) == ParseFailed) {
				wpa_printf(MSG_DEBUG, "Failed to parse IEs");
				return;
			}
			if (validate_security_profile_common(hapd, sta, &elems, "PASN", NULL)) {

				wpa_printf(MSG_INFO,
						"UHR: Rejecting PASN auth from "
						MACSTR
						" - Security Profile mismatch",
						MAC2STR(sta->addr));
				send_auth_reply(hapd, sta, sta->addr,
						le_to_host16(mgmt->u.auth.auth_alg),
						WLAN_AUTH_TR_SEQ_PASN_AUTH2,
						WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE,
						NULL, 0,
						"pasn-sec-profile-reject");
				ap_free_sta(hapd, sta);
				return;
			}
		}

		if (sta->sp_ie_validated)
			sta->pasn->sp_ie_in_pasn_activated = 1;

		hapd_pasn_update_params(hapd, sta, mgmt, len);

		ret = handle_auth_pasn_1(sta->pasn, hapd->own_addr, sta->addr,
					 mgmt, len, false);
		wpabuf_free(sta->pasn->frame);
		sta->pasn->frame = NULL;
		if (ret < 0) {
			hostapd_drv_set_secure_ranging_ctx(hapd, hapd->own_addr,
							   sta->addr, 0, 0,
							   NULL, 0, NULL, 1);
			ap_free_sta(hapd, sta);
		}
	} else if (trans_seq == WLAN_AUTH_TR_SEQ_PASN_AUTH3) {
		if (!sta->pasn) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Not expecting transaction == 3");
			return;
		}

		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failure status in transaction == 3");
			ap_free_sta_pasn(hapd, sta);
			return;
		}

		ret = handle_auth_pasn_3(sta->pasn, hapd->own_addr, sta->addr,
					 mgmt, len);
		if (ret == 0) {
#ifdef CONFIG_ENC_ASSOC
			if (ap_sta_is_epp(sta)) {
				sta->auth_alg = WLAN_AUTH_EPPKE;
				sta->flags |= WLAN_STA_AUTH;
			}
#endif /* CONFIG_ENC_ASSOC */
			ptksa_cache_add(hapd->ptksa, hapd->own_addr, sta->addr,
					pasn_get_cipher(sta->pasn), 43200,
					pasn_get_ptk(sta->pasn), NULL, NULL,
					pasn_get_akmp(sta->pasn),
					sta->pasn->auth_alg);
#ifdef CONFIG_ENC_ASSOC
			/* TODO: Support VLAN ID assignment based on configured
			 * SAE passwords. */
			if (ap_sta_is_epp(sta) && !sta->pasn->tk_configured &&
			    sta->pasn->eppke_set_key)
				sta->pasn->eppke_set_key(
					sta->pasn->cb_ctx,
					wpa_cipher_to_alg(sta->pasn->cipher),
					sta->addr, 0,
					sta->pasn->ptk.tk,
					sta->pasn->ptk.tk_len);
#endif /* CONFIG_ENC_ASSOC */
			if (!ap_sta_is_epp(sta))
				pasn_set_keys_from_cache(
					hapd, hapd->own_addr,
					sta->addr,
					pasn_get_cipher(sta->pasn),
					pasn_get_akmp(sta->pasn));
		}
		if (!ap_sta_is_epp(sta) ||
		    (ret < 0 &&
		     ap_sta_is_epp(sta) && !ap_sta_is_authorized(sta)))
			ap_free_sta(hapd, sta);

	} else {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid transaction %u - ignore", trans_seq);
	}
}

#endif /* CONFIG_PASN */


static void handle_auth(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt, size_t len,
			int rssi, int from_queue)
{
	u16 auth_alg, auth_transaction, status_code;
	u16 resp = WLAN_STATUS_SUCCESS;
	struct sta_info *sta = NULL, *osta = NULL;
	struct hostapd_data *ohapd;
	int res, reply_res, ubus_resp;
	u16 fc;
	const u8 *challenge = NULL;
	u8 resp_ies[2 + WLAN_AUTH_CHALLENGE_LEN +
		    WLAN_PASN_COMEBACK_INFO_BUF_LEN];
	size_t resp_ies_len = 0;
	u16 seq_ctrl;
	struct radius_sta rad_info;
	const u8 *dst, *sa;
	bool skip_acl = false;
#ifdef CONFIG_IEEE80211BE
	bool mld_sta = false;
#endif /* CONFIG_IEEE80211BE */
	int ft_auth_resp;
	bool deferred_auth_response = false;
#ifdef CONFIG_HOSTAPD_IF
	bool hapd_if_notified = false;
#endif
	struct hostapd_ubus_request req = {
		.type = HOSTAPD_UBUS_AUTH_REQ,
		.mgmt_frame = mgmt,
		.ssi_signal = rssi,
	};

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		wpa_printf(MSG_INFO, "handle_auth - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iconf->ignore_auth_probability > 0.0 &&
	    drand48() < hapd->iconf->ignore_auth_probability) {
		wpa_printf(MSG_INFO,
			   "TESTING: ignoring auth frame from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	sa = mgmt->sa;
#ifdef CONFIG_IEEE80211BE
	/*
	 * Handle MLO authentication before the station is added to hostapd and
	 * the driver so that the station MLD MAC address would be used in both
	 * hostapd and the driver.
	 */
	sa = hostapd_process_ml_auth(hapd, mgmt, len);
	if (sa)
		mld_sta = true;
	else
		sa = mgmt->sa;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */

	/* For MLO APs with DENY_UNLESS_ACCEPTED or
	 * ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST modes, skip the initial
	 * single-link ACL check and defer to hostapd_check_ml_acl()
	 * which properly checks all partner links.
	 */
	if (hapd->conf->mld_ap &&
	    (hapd->conf->macaddr_acl == DENY_UNLESS_ACCEPTED ||
	     hapd->conf->macaddr_acl == ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST))
		skip_acl = true;
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);
	fc = le_to_host16(mgmt->frame_control);
	seq_ctrl = le_to_host16(mgmt->seq_ctrl);

	if (len >= IEEE80211_HDRLEN + sizeof(mgmt->u.auth) +
	    2 + WLAN_AUTH_CHALLENGE_LEN &&
	    mgmt->u.auth.variable[0] == WLAN_EID_CHALLENGE &&
	    mgmt->u.auth.variable[1] == WLAN_AUTH_CHALLENGE_LEN)
		challenge = &mgmt->u.auth.variable[2];

	wpa_printf(MSG_DEBUG, "authentication: STA=" MACSTR " auth_alg=%d "
		   "auth_transaction=%d status_code=%d protected=%d%s "
		   "seq_ctrl=0x%x%s%s ml sta %d",
		   MAC2STR(sa), auth_alg, auth_transaction,
		   status_code, !!(fc & WLAN_FC_PROTECTED),
		   challenge ? " challenge" : "",
		   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "",
		   from_queue ? " (from queue)" : "", mld_sta);

#ifdef CONFIG_NO_RC4
	if (auth_alg == WLAN_AUTH_SHARED_KEY) {
		wpa_printf(MSG_INFO,
			   "Unsupported authentication algorithm (%d)",
			   auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}
#endif /* CONFIG_NO_RC4 */

#ifdef CONFIG_IEEE8021X_AUTH
	if (auth_alg == WLAN_AUTH_802_1X &&
	    !hapd->conf->eap_using_authentication_frames) {
		wpa_printf(MSG_INFO,
			   "Unsupported authentication algorithm (%d)",
			   auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}
#endif /* CONFIG_IEEE8021X_AUTH */

	if (hapd->tkip_countermeasures) {
		wpa_printf(MSG_DEBUG,
			   "Ongoing TKIP countermeasures (Michael MIC failure) - reject authentication");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (!(((hapd->conf->auth_algs & WPA_AUTH_ALG_OPEN) &&
	       auth_alg == WLAN_AUTH_OPEN) ||
#ifdef CONFIG_IEEE80211R_AP
	      (hapd->conf->wpa && wpa_key_mgmt_ft(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_FT) ||
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	      (hapd->conf->wpa &&
	       wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt |
				hapd->conf->rsn_override_key_mgmt |
				hapd->conf->rsn_override_key_mgmt_2 |
				hostapd_sp_implied_key_mgmt(hapd->conf)) &&
	       auth_alg == WLAN_AUTH_SAE) ||
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	      (hapd->conf->wpa && wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_FILS_SK) ||
	      (hapd->conf->wpa && wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt) &&
	       hapd->conf->fils_dh_group &&
	       auth_alg == WLAN_AUTH_FILS_SK_PFS) ||
#endif /* CONFIG_FILS */
#ifdef CONFIG_PASN
	      (hapd->conf->wpa &&
	       (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PASN) &&
	       auth_alg == WLAN_AUTH_PASN) ||
#endif /* CONFIG_PASN */
#ifdef CONFIG_ENC_ASSOC
	      (hapd->conf->wpa &&
	       ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_EPPKE) ||
		(hostapd_sp_implied_key_mgmt(hapd->conf) & WPA_KEY_MGMT_EPPKE)) &&
	       hapd->conf->assoc_frame_encryption &&
	       auth_alg == WLAN_AUTH_EPPKE) ||
#endif /* CONFIG_ENC_ASSOC */
#ifdef CONFIG_IEEE8021X_AUTH
	      (hapd->conf->wpa &&
	       wpa_key_mgmt_wpa_ieee8021x(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_802_1X) ||
#endif /* CONFIG_IEEE8021X_AUTH */
	      ((hapd->conf->auth_algs & WPA_AUTH_ALG_SHARED) &&
	       auth_alg == WLAN_AUTH_SHARED_KEY))) {
		wpa_printf(MSG_INFO, "Unsupported authentication algorithm (%d)",
			   auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}

	if (!(auth_transaction == 1 ||
#ifdef CONFIG_SAE
	      (auth_alg == WLAN_AUTH_SAE &&
	       auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM) ||
#endif /* CONFIG_SAE */
#ifdef CONFIG_PASN
	      (auth_alg == WLAN_AUTH_PASN &&
	       auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH3) ||
#endif /* CONFIG_PASN */
#ifdef CONFIG_ENC_ASSOC
	      (auth_alg == WLAN_AUTH_EPPKE &&
	       auth_transaction == WLAN_AUTH_TR_SEQ_PASN_AUTH3) ||
#endif /* CONFIG_ENC_ASSOC */
#ifdef CONFIG_IEEE8021X_AUTH
	      /* EAP over Auth involves variable number of frames depending
	       * on the EAP authentication method */
	      auth_alg == WLAN_AUTH_802_1X ||
#endif /* CONFIG_IEEE8021X_AUTH */
	      (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 3))) {
		wpa_printf(MSG_INFO, "Unknown authentication transaction number (%d)",
			   auth_transaction);
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
		goto fail;
	}

	if (ether_addr_equal(mgmt->sa, hapd->own_addr)) {
		wpa_printf(MSG_INFO, "Station " MACSTR " not allowed to authenticate",
			   MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

#ifdef CONFIG_IEEE80211BE
	if (mld_sta &&
	    (ether_addr_equal(sa, hapd->own_addr) ||
	     ether_addr_equal(sa, hapd->mld->mld_addr))) {
		wpa_printf(MSG_INFO,
			   "Station " MACSTR " not allowed to authenticate",
			   MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->conf->no_auth_if_seen_on) {
		struct hostapd_data *other;

		other = sta_track_seen_on(hapd->iface, sa,
					  hapd->conf->no_auth_if_seen_on);
		if (other) {
			u8 *pos;
			u32 info;
			u8 op_class, channel, phytype;

			wpa_printf(MSG_DEBUG, "%s: Reject authentication from "
				   MACSTR " since STA has been seen on %s",
				   hapd->conf->iface, MAC2STR(sa),
				   hapd->conf->no_auth_if_seen_on);

			resp = WLAN_STATUS_REJECTED_WITH_SUGGESTED_BSS_TRANSITION;
			pos = &resp_ies[0];
			*pos++ = WLAN_EID_NEIGHBOR_REPORT;
			*pos++ = 13;
			os_memcpy(pos, other->own_addr, ETH_ALEN);
			pos += ETH_ALEN;
			info = 0; /* TODO: BSSID Information */
			WPA_PUT_LE32(pos, info);
			pos += 4;
			if (other->iconf->hw_mode == HOSTAPD_MODE_IEEE80211AD)
				phytype = 8; /* dmg */
			else if (other->iconf->ieee80211ac)
				phytype = 9; /* vht */
			else if (other->iconf->ieee80211n)
				phytype = 7; /* ht */
			else if (other->iconf->hw_mode ==
				 HOSTAPD_MODE_IEEE80211A)
				phytype = 4; /* ofdm */
			else if (other->iconf->hw_mode ==
				 HOSTAPD_MODE_IEEE80211G)
				phytype = 6; /* erp */
			else
				phytype = 5; /* hrdsss */
			if (ieee80211_freq_to_channel_ext(
				    hostapd_hw_get_freq(other,
							other->iconf->channel),
				    other->iconf->secondary_channel,
				    other->iconf->ieee80211ac,
				    &op_class, &channel) == NUM_HOSTAPD_MODES) {
				op_class = 0;
				channel = other->iconf->channel;
			}
			*pos++ = op_class;
			*pos++ = channel;
			*pos++ = phytype;
			resp_ies_len = pos - &resp_ies[0];
			goto fail;
		}
	}


	res = ieee802_11_allowed_address(hapd, sa, (const u8 *) mgmt, len,
					 &rad_info);
	if (res == HOSTAPD_ACL_REJECT && !skip_acl) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"Ignore Authentication frame from " MACSTR
			" due to ACL reject", MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	if (res == HOSTAPD_ACL_PENDING)
		return;

	ubus_resp = hostapd_ubus_handle_event(hapd, &req);
	if (ubus_resp) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " rejected by ubus handler.\n",
			MAC2STR(mgmt->sa));
		resp = ubus_resp > 0 ? (u16) ubus_resp : WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

#ifdef CONFIG_IEEE80211BE
	/* In case of ACCEPT_UNLESS_DENIED, check both mld address and
	 * source address
	 */
	if (mld_sta) {

		res = ieee802_11_allowed_address(hapd, sa,
						 (const u8 *) mgmt, len,
						 &rad_info);
		if (res == HOSTAPD_ACL_REJECT && !skip_acl) {
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Ignore Authentication frame from " MACSTR
				" due to ACL reject", MAC2STR(mgmt->sa));
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		if (res == HOSTAPD_ACL_PENDING)
			return;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_SAE
	if (auth_alg == WLAN_AUTH_SAE && !from_queue &&
	    (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT ||
	     (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM &&
	      auth_sae_queued_addr(hapd, sa)))) {
		/* Handle SAE Authentication commit message through a queue to
		 * provide more control for postponing the needed heavy
		 * processing under a possible DoS attack scenario. In addition,
		 * queue SAE Authentication confirm message if there happens to
		 * be a queued commit message from the same peer. This is needed
		 * to avoid reordering Authentication frames within the same
		 * SAE exchange. */
		auth_sae_queue(hapd, mgmt, len, rssi);
		return;
	}
#endif /* CONFIG_SAE */
	if (hapd->conf->mld_ap) {
		struct hostapd_ft_over_ds_ml_sta_entry *entry;

		entry = ap_get_ft_ds_ml_sta(hapd, sa);
		if (entry) {
			wpa_printf(MSG_ERROR,
				   "handle_auth: Auth frame received with SA = FT-OVER-DS list MLD mac "MACSTR"\n",
				   MAC2STR(sa));
			return;
		}
	}

	if (auth_alg != WLAN_AUTH_FT) {
		osta = ap_sta_get_from_obss(hapd, sa, mgmt->sa, &ohapd);
		/* Delete the station from other BSS immediately if its not MFP or not authorized yet */
		if (osta && (!(osta->flags & WLAN_STA_MFP) || !ap_sta_is_authorized(osta))) {
			wpa_printf(MSG_DEBUG, "Delete STA "MACSTR" from driver on %s as STA "
				   "is not authorized and trying to associate in new bss %s",
				   MAC2STR(osta->addr), ohapd->conf->iface, hapd->conf->iface);
			if (osta->flags & WLAN_STA_ASSOC)
				hostapd_drv_sta_deauth(ohapd, osta->addr,
						       WLAN_REASON_PREV_AUTH_NOT_VALID);
			mlme_deauthenticate_indication(ohapd, osta,
						       WLAN_REASON_PREV_AUTH_NOT_VALID);
			if (ap_sta_is_mld(ohapd, osta)) {
				hostapd_drv_sta_remove(ohapd, osta->addr);
				osta->skip_kernel_delete = true;
			}

			ap_sta_remove_link_sta(ohapd, osta, false, true);
			ap_free_sta(ohapd, osta);
			osta = NULL;
		}

	}

#if defined(CONFIG_ENC_ASSOC) || defined(CONFIG_IEEE8021X_AUTH)
	if (auth_transaction == 1 &&
	    (auth_alg == WLAN_AUTH_EPPKE || auth_alg == WLAN_AUTH_802_1X)) {
		struct hostapd_data *ohapd;
		struct sta_info *osta;
		u8 *pos;

		/*
		 * On an AP, determine if there exists a non-AP STA identified
		 * by the source STA MAC address that requires an Authentication
		 * comeback.
		 *
		 * On an AP MLD, determine if there exists a non-AP STA or
		 * non-AP link STA identified by the source STA MAC address or
		 * STA MLD address in the Authentication frame that requires an
		 * Authentication comeback.
		 */
		if (hostapd_find_auth_comeback_sta(hapd, mgmt->sa,
						   mgmt->u.auth.variable,
						   len - IEEE80211_HDRLEN -
						   sizeof(mgmt->u.auth),
						   &ohapd, &osta) ||
		    (mld_sta && !ether_addr_equal(mgmt->sa, sa) &&
		     hostapd_find_auth_comeback_sta(hapd, sa,
						    mgmt->u.auth.variable,
						    len - IEEE80211_HDRLEN -
						    sizeof(mgmt->u.auth),
						    &ohapd, &osta))) {
			resp = WLAN_STATUS_AUTH_REFUSED_TEMPORARILY;

			pos = hostapd_add_auth_comeback(ohapd, osta, resp_ies,
							sizeof(resp_ies), auth_alg);
			if (!pos) {
				wpa_printf(MSG_ERROR, "Failed to add "
					   "Authentication comeback info (auth_alg=%u)",
					   auth_alg);
				return;
			} else {
				resp_ies_len = pos - resp_ies;
			}
			goto fail;
		}
	}
#endif /* CONFIG_ENC_ASSOC || CONFIG_IEEE8021X_AUTH */

	sta = ap_get_sta(hapd, sa);
	if (sta) {
		sta->flags &= ~WLAN_STA_PENDING_FILS_ERP;
		sta->ft_over_ds = 0;
		if ((fc & WLAN_FC_RETRY) &&
		    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
		    sta->last_seq_ctrl == seq_ctrl &&
		    sta->last_subtype == WLAN_FC_STYPE_AUTH) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Drop repeated authentication frame seq_ctrl=0x%x",
				       seq_ctrl);
			return;
		}
#ifdef CONFIG_PASN
		if (auth_alg == WLAN_AUTH_PASN &&
		    (sta->flags & WLAN_STA_ASSOC)) {
			wpa_printf(MSG_DEBUG,
				   "PASN: auth: Existing station: " MACSTR,
				   MAC2STR(sta->addr));
			return;
		}
#endif /* CONFIG_PASN */
	} else {
#ifdef CONFIG_MESH
		if (hapd->conf->mesh & MESH_ENABLED) {
			/* if the mesh peer is not available, we don't do auth.
			 */
			wpa_printf(MSG_DEBUG, "Mesh peer " MACSTR
				   " not yet known - drop Authentication frame",
				   MAC2STR(sa));
			/*
			 * Save a copy of the frame so that it can be processed
			 * if a new peer entry is added shortly after this.
			 */
			wpabuf_free(hapd->mesh_pending_auth);
			hapd->mesh_pending_auth = wpabuf_alloc_copy(mgmt, len);
			os_get_reltime(&hapd->mesh_pending_auth_time);
			return;
		}
#endif /* CONFIG_MESH */

		if (auth_alg == WLAN_AUTH_SAE
			&& auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM
			&& !sta) {
			wpa_printf(MSG_INFO, " No STA for SAE AUTH COMMIT");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		sta = ap_sta_add(hapd, sa);
		if (!sta) {
			wpa_printf(MSG_DEBUG, "ap_sta_add() failed");
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}
#ifdef CONFIG_HOSTAPD_IF
#ifdef CONFIG_SAE
	/*
	 * If a plugin response is still pending for this STA, drop the frame
	 * immediately without forwarding it.
	 */
	if (sta && sta->sae && sta->sae->plugin_wait) {
		wpa_printf(MSG_DEBUG,
			   "%s: SAE " MACSTR " - dropping frame, plugin response pending",
			   __func__, MAC2STR(sta->addr));
		return;
	}
#endif /* CONFIG_SAE */
#endif

#if defined(CONFIG_ENC_ASSOC) || defined(CONFIG_IEEE8021X_AUTH)
	if (auth_alg == WLAN_AUTH_EPPKE || auth_alg == WLAN_AUTH_802_1X) {
		wpa_printf(MSG_DEBUG, "Mark the station as an EPP peer");
		sta->epp_sta = true;
	}
#endif /* CONFIG_ENC_ASSOC || CONFIG_IEEE8021X_AUTH */

#ifdef CONFIG_IEEE80211BE
	/* Set the non-AP MLD information based on the initial Authentication
	 * frame. Once the STA entry has been added to the driver, the driver
	 * will translate addresses in the frame and we need to avoid overriding
	 * peer_addr based on mgmt->sa which would have been translated to the
	 * MLD MAC address. */
	if ((!(sta->flags & WLAN_STA_MFP) || !ap_sta_is_authorized(sta)) &&
	    !sta->added_unassoc && auth_transaction == 1) {
		if (sta->wpa_sm) {
			wpa_auth_sta_deinit(sta->wpa_sm);
			sta->wpa_sm = NULL;
			clear_wpa_sm_for_each_partner_link(hapd, sta);
		}
		ap_sta_free_sta_profile(&sta->mld_info);
		os_memset(&sta->mld_info, 0, sizeof(sta->mld_info));

		if (mld_sta) {
			u8 link_id = hapd->mld_link_id;

			ap_sta_set_mld(sta, true);
			set_link_id_for_each_partner_link_sta(hapd,
							      sta,
							      link_id);
			sta->mld_assoc_link_id = link_id;
			/*
			 * Set the MLD address as the station address and the
			 * station addresses.
			 */
			os_memcpy(sta->mld_info.common_info.mld_addr, sa,
				  ETH_ALEN);
			os_memcpy(sta->mld_info.links[link_id].peer_addr,
				  mgmt->sa, ETH_ALEN);
			os_memcpy(sta->mld_info.links[link_id].local_addr,
				  hapd->own_addr, ETH_ALEN);
		}
	}
#endif /* CONFIG_IEEE80211BE */

	sta->last_seq_ctrl = seq_ctrl;
	sta->last_subtype = WLAN_FC_STYPE_AUTH;
#ifdef CONFIG_MBO
	sta->auth_rssi = rssi;
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211BN
	/* Parse SMD IE if present in authentication request */
	if (auth_transaction == 1) {
		const u8 *pos;

		pos = auth_skip_fixed_fields(hapd, mgmt,
					     len - offsetof(struct ieee80211_mgmt, u.auth.variable));
		if (pos)
			hostapd_parse_smd_ie(hapd, sta, pos,
					     (int)len - (pos - mgmt->u.auth.variable));

		/* Transfer SMD info to wpa_state_machine if it exists */
		if (sta->wpa_sm && sta->smd_info.smd_sta) {
			wpa_printf(MSG_DEBUG,
				   "SMD: Transferring SMD info to wpa_state_machine for "
				   MACSTR " after authentication",
				   MAC2STR(sta->addr));
			wpa_auth_set_smd_info(sta->wpa_sm, sta);
		}
	}
#endif /* CONFIG_IEEE80211BN */

	res = ieee802_11_set_radius_info(hapd, sta, res, &rad_info);
	if (res) {
		wpa_printf(MSG_DEBUG, "ieee802_11_set_radius_info() failed");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);

	os_memcpy(sta->reply_addr, mgmt->sa, ETH_ALEN);
	sta->mld_auth = mld_sta;
	/*
	 * If the driver supports full AP client state, add a station to the
	 * driver before sending authentication reply to make sure the driver
	 * has resources, and not to go through the entire authentication and
	 * association handshake, and fail it at the end.
	 *
	 * If this is not the first transaction, in a multi-step authentication
	 * algorithm, the station already exists in the driver
	 * (sta->added_unassoc = 1) so skip it.
	 *
	 * In mesh mode, the station was already added to the driver when the
	 * NEW_PEER_CANDIDATE event is received.
	 *
	 * If PMF was negotiated for the existing association, skip this to
	 * avoid dropping the STA entry and the associated keys. This is needed
	 * to allow the original connection work until the attempt can complete
	 * (re)association, so that unprotected Authentication frame cannot be
	 * used to bypass PMF protection.
	 *
	 * PASN authentication does not require adding/removing station to the
	 * driver so skip this flow in case of PASN authentication.
	 */
	if (FULL_AP_CLIENT_STATE_SUPP(hapd->iface->drv_flags) &&
	    (!(sta->flags & WLAN_STA_MFP) || !ap_sta_is_authorized(sta)) &&
	    !(hapd->conf->mesh & MESH_ENABLED) &&
	    !(sta->added_unassoc) && auth_alg != WLAN_AUTH_PASN) {
		res = ap_sta_check_link_sta(hapd, sta, mgmt->sa);
		if (!res && !osta) {
			if (ap_sta_re_add(hapd, sta, 1) < 0) {
				resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
				goto fail;
			}
		}
#ifdef CONFIG_IEEE80211BE
		else
			sta->unadded_sta = true;
#endif /* CONFIG_IEEE80211BE */
	}

	if ((sta = ap_get_sta(hapd, sa)) != NULL &&
	    ap_sta_is_authorized(sta) &&
	    !(sta->added_unassoc) &&
	    (sta->skip_kernel_delete)) {
		if (ap_sta_re_add(hapd, sta, 1) < 0) {
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}
	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication OK (open system)");
#ifdef CONFIG_HOSTAPD_IF
		if (hostapd_if_notify_auth(hapd, sta, (const u8 *) mgmt, len,
					rssi, WLAN_STATUS_SUCCESS, 2, 0,
					WLAN_AUTH_OPEN, mgmt->sa) ==
					HOSTAPD_IF_FRAME_PROCESSING_WAIT)
			return;
		hapd_if_notified = true;
#endif
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_OPEN;
		mlme_authenticate_indication(hapd, sta);
		break;
#ifdef CONFIG_WEP
#ifndef CONFIG_NO_RC4
	case WLAN_AUTH_SHARED_KEY:
		resp = auth_shared_key(hapd, sta, auth_transaction, challenge,
				       fc & WLAN_FC_PROTECTED);
		if (resp != 0)
			wpa_printf(MSG_DEBUG,
				   "auth_shared_key() failed: status=%d", resp);
		sta->auth_alg = WLAN_AUTH_SHARED_KEY;
		mlme_authenticate_indication(hapd, sta);
		if (sta->challenge && auth_transaction == 1) {
			resp_ies[0] = WLAN_EID_CHALLENGE;
			resp_ies[1] = WLAN_AUTH_CHALLENGE_LEN;
			os_memcpy(resp_ies + 2, sta->challenge,
				  WLAN_AUTH_CHALLENGE_LEN);
			resp_ies_len = 2 + WLAN_AUTH_CHALLENGE_LEN;
		}
		break;
#endif /* CONFIG_NO_RC4 */
#endif /* CONFIG_WEP */
#ifdef CONFIG_IEEE80211R_AP
	case WLAN_AUTH_FT:
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL) {
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
			if (sta->wpa_sm == NULL) {
				wpa_printf(MSG_DEBUG, "FT: Failed to initialize WPA "
					   "state machine");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
#ifdef CONFIG_IEEE80211BE
			struct mld_info *sta_mld_info = &sta->mld_info;
			if (ap_sta_is_mld(hapd, sta)) {
				wpa_printf(MSG_DEBUG,
					   "MLD: Set ML info in RSN Authenticator");
				wpa_auth_set_ml_info(sta->wpa_sm,
						     sta->mld_assoc_link_id,
						     sta_mld_info);
			}
#endif /* CONFIG_IEEE80211BE */
		}
#if defined(CONFIG_HOSTAPD_IF) && defined(CONFIG_QCN_EXTN)
		if (hostapd_if_frame_fwd_decision(hapd,auth_alg,
						  HOSTAPD_IF_FRAME_TYPE_AUTH)) {
			deferred_auth_response = true;
		}
#endif

		if (hapd->conf->security_profiles) {
			struct ieee802_11_elems elems;
			size_t auth_var_len = len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth);
			if (ieee802_11_parse_elems(mgmt->u.auth.variable, auth_var_len, &elems, 1) == ParseFailed) {
				wpa_printf(MSG_DEBUG, "Failed to parse IEs");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			if (validate_security_profile_common(hapd, sta, &elems, "FT", NULL)) {
				resp = WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE; 
				goto fail;
			}
		}
		ft_auth_resp = wpa_ft_process_auth(sta->wpa_sm,
						   auth_transaction,
						   mgmt->u.auth.variable,
						   (len - IEEE80211_HDRLEN -
						   sizeof(mgmt->u.auth)),
						   handle_auth_ft_finish, hapd,
						   deferred_auth_response);
		/* handle_auth_ft_finish() callback will complete auth. */
		if (ft_auth_resp >= 0) {
#ifdef CONFIG_HOSTAPD_IF
			hostapd_if_notify_auth(hapd, sta, (const u8 *) mgmt,
					       len, rssi, (u16) ft_auth_resp,
					       1, 0,
					       auth_alg, mgmt->sa);
#endif
		}
		return;
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	case WLAN_AUTH_SAE:
#ifdef CONFIG_MESH
		if (status_code == WLAN_STATUS_SUCCESS &&
		    hapd->conf->mesh & MESH_ENABLED) {
			if (sta->wpa_sm == NULL)
				sta->wpa_sm =
					wpa_auth_sta_init(hapd->wpa_auth,
							  sta->addr, NULL);
			if (sta->wpa_sm == NULL) {
				wpa_printf(MSG_DEBUG,
					   "SAE: Failed to initialize WPA state machine");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
		}
#endif /* CONFIG_MESH */
		handle_auth_sae(hapd, sta, mgmt, len, auth_transaction,
				status_code, rssi);
		return;
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	case WLAN_AUTH_FILS_SK:
	case WLAN_AUTH_FILS_SK_PFS:
		handle_auth_fils(hapd, sta, mgmt->u.auth.variable,
				 len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth),
				 auth_alg, auth_transaction, status_code,
				 handle_auth_fils_finish);
		return;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE8021X_AUTH
	case WLAN_AUTH_802_1X:
		handle_auth_802_1x(hapd, sta, mgmt->u.auth.variable,
				   len - IEEE80211_HDRLEN -
				   sizeof(mgmt->u.auth),
				   auth_alg, auth_transaction);
		return;
#endif /* CONFIG_IEEE8021X_AUTH */
#ifdef CONFIG_ENC_ASSOC
	case WLAN_AUTH_EPPKE:
#endif /* CONFIG_ENC_ASSOC */
#ifdef CONFIG_PASN
	case WLAN_AUTH_PASN:
		handle_auth_pasn(hapd, sta, mgmt, len, auth_transaction,
				 status_code);
		return;
#endif /* CONFIG_PASN */
	}

	if (hapd->conf->security_profiles) {
			struct ieee802_11_elems elems;
			size_t auth_var_len = len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth);
			if (ieee802_11_parse_elems(mgmt->u.auth.variable, auth_var_len, &elems, 1) == ParseFailed) {
				wpa_printf(MSG_DEBUG, "Failed to parse IEs");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			if (validate_security_profile_common(hapd, sta, &elems, "Open", NULL)) {
			resp = WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE;
			goto fail;
		}
	}

 fail:

#ifdef CONFIG_HOSTAPD_IF
	if (!hapd_if_notified &&
	    hostapd_if_notify_auth(hapd, sta, (const u8 *) mgmt, len, rssi,
				   resp, auth_transaction, 0,
				   auth_alg, mgmt->sa) == HOSTAPD_IF_FRAME_PROCESSING_WAIT)
		return;
#endif

	dst = mgmt->sa;

	reply_res = send_auth_reply(hapd, sta, dst, auth_alg,
				    auth_alg == WLAN_AUTH_SAE ?
				    auth_transaction : auth_transaction + 1,
				    resp, resp_ies, resp_ies_len,
				    "handle-auth");

	if (sta && sta->added_unassoc && (resp != WLAN_STATUS_SUCCESS ||
					  reply_res != WLAN_STATUS_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}

void hostap_ft_ds_ml_sta_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_ft_over_ds_ml_sta_entry *entry = eloop_ctx;

	if (!entry)
		return;

	wpa_printf(MSG_DEBUG, "%s: removing "MACSTR"\n", __func__,
		   MAC2STR(entry->mld_mac));
	if (entry->wpa_sm)
		wpa_auth_sta_deinit(entry->wpa_sm);

	dl_list_del(&entry->list);
	os_free(entry);
}


static u32 hostapd_get_aid_word(struct hostapd_data *hapd,
				struct sta_info *sta, int i)
{
#ifdef CONFIG_IEEE80211BE
	u32 aid_word = 0;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return hapd->sta_aid[i];
#endif /* CONFIG_QCN_EXTN */

	/* Do not assign an AID that is in use on any of the affiliated links
	 * when finding an AID for a non-AP MLD. */
	if (hapd->conf->mld_ap && sta->mld_info.mld_sta) {
		int j;

		for (j = 0; j < MAX_NUM_MLD_LINKS; j++) {
			struct hostapd_data *link_bss;

			if (!sta->mld_info.links[j].valid)
				continue;

			link_bss = hostapd_mld_get_link_bss(hapd, j);
			if (!link_bss) {
				/* This shouldn't happen, just skip */
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for AID");
				continue;
			}
#ifdef CONFIG_QCN_EXTN
			if (hostapd_is_repurpose_disabled_11be_extn(link_bss->conf)) {
				wpa_printf(MSG_DEBUG,
					   "MLD: dont consider aid word from repurposed link");
				continue;
			}
#endif /* CONFIG_QCN_EXTN */
			link_bss = hostapd_mbssid_get_tx_bss(link_bss);
			aid_word |= link_bss->sta_aid[i];
		}

		return aid_word;
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd->sta_aid[i];
}


int hostapd_get_wds_mld_sta_uid(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct hostapd_data *link_bss;
	int i, j = 32, aid;

	/* get a unique ID */
	if (sta->wds_mld_uid > 0) {
		wpa_printf(MSG_DEBUG, "  old UID %d", sta->wds_mld_uid);
		return 0;
	}

	if (!hapd->conf->mld_ap)
		return -1;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		int uid_offset = -1;
		u32 uid_base = WDS_STA_UID_REPURPOSED_BASE +
			       (hapd->mld_link_id *
				WDS_STA_UID_REPURPOSED_PER_LINK);

		for (i = 0; i < WDS_STA_UID_REPURPOSED_WORDS; i++) {
			if (hapd->wds_sta_uid_repurpose[i] == (u32) -1)
				continue;

			for (j = 0; j < 32; j++) {
				int idx = i * 32 + j;

				if (idx >=
				    WDS_STA_UID_REPURPOSED_PER_LINK)
					break;

				if (!(hapd->wds_sta_uid_repurpose[i] & BIT(j))) {
					uid_offset = idx;
					break;
				}
			}

			if (uid_offset >= 0)
				break;
		}
		if (uid_offset < 0)
			return -1;

		aid = uid_base + uid_offset;
		sta->wds_mld_uid = aid;
		hapd->wds_sta_uid_repurpose[i] |= BIT(j);

		wpa_printf(MSG_DEBUG, "  new UID %d", sta->wds_mld_uid);
		return 0;
	}
#endif

	link_bss = hostapd_mld_get_first_bss(hapd);
	if (!link_bss)
		link_bss = hapd;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(link_bss->conf))
		link_bss = hostapd_get_non_repurposed_link_of_mld_extn(hapd);

	if (!link_bss)
		return -1;
#endif

	for (i = 0; i < AID_WORDS; i++) {
		if (link_bss->wds_sta_uid[i] == (u32) -1)
			continue;

		for (j = 0; j < 32; j++) {
			if (!(link_bss->wds_sta_uid[i] & BIT(j))) {
				break;
			}
		}

		if (j < 32)
			break;
	}
	if (j == 32)
		return -1;

	aid = i * 32 + j + 1;

	if (aid > 2007)
		return -1;

	sta->wds_mld_uid = aid;
	link_bss->wds_sta_uid[i] |= BIT(j);

	wpa_printf(MSG_DEBUG, "  new UID %d", sta->wds_mld_uid);

	return 0;
}

void hostapd_free_wds_mld_sta_uid(struct hostapd_data *hapd,
				  struct sta_info *sta)
{
	if (sta->wds_mld_uid == 0)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		u32 uid_base = WDS_STA_UID_REPURPOSED_BASE +
			       (hapd->mld_link_id *
				WDS_STA_UID_REPURPOSED_PER_LINK);

		if (sta->wds_mld_uid >= uid_base &&
		    sta->wds_mld_uid <
		    uid_base + WDS_STA_UID_REPURPOSED_PER_LINK) {
			int uid_offset = sta->wds_mld_uid - uid_base;

			hapd->wds_sta_uid_repurpose[uid_offset / 32] &=
				~BIT(uid_offset % 32);
		}
	} else
#endif /* CONFIG_QCN_EXTN */
	hapd->wds_sta_uid[(sta->wds_mld_uid - 1) / 32] &=
		~BIT((sta->wds_mld_uid - 1) % 32);

	sta->wds_mld_uid = 0;
}

int hostapd_get_aid(struct hostapd_data *hapd, struct sta_info *sta)
{
	int i, j = 32, aid;

	/* Transmitted and non-transmitted BSSIDs share the same AID pool, so
	 * use the shared storage in the transmitted BSS to find the next
	 * available value. */
	hapd = hostapd_mbssid_get_tx_bss(hapd);

	/* get a unique AID */
	if (sta->aid > 0) {
		wpa_printf(MSG_DEBUG, "  old AID %d", sta->aid);
		return 0;
	}

	if (TEST_FAIL())
		return -1;

	for (i = 0; i < AID_WORDS; i++) {
		u32 aid_word = hostapd_get_aid_word(hapd, sta, i);

		if (aid_word == (u32) -1)
			continue;
		for (j = 0; j < 32; j++) {
			if (!(aid_word & BIT(j)))
				break;
		}
		if (j < 32)
			break;
	}
	if (j == 32)
		return -1;
	aid = i * 32 + j;
	if (aid <= 0 || aid > 2007)
		return -1;

	sta->aid = aid;
	hapd->sta_aid[i] |= BIT(j);
	wpa_printf(MSG_DEBUG, " new AID %d hapd:%s hapd->sta_aid[%d]:%u",
		   sta->aid, hapd->conf->iface, i, hapd->sta_aid[i]);
	return 0;
}


static u16 check_ssid(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *ssid_ie, size_t ssid_ie_len)
{
	if (ssid_ie == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (ssid_ie_len != hapd->conf->ssid.ssid_len ||
	    os_memcmp(ssid_ie, hapd->conf->ssid.ssid, ssid_ie_len) != 0) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Station tried to associate with unknown SSID "
			       "'%s'", wpa_ssid_txt(ssid_ie, ssid_ie_len));
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}

static u16 check_cip_padding_delay(struct hostapd_data *hapd, struct sta_info *sta,
			      const u8 *cip_pad, size_t cip_pad_len)
{
	if (!cip_pad || cip_pad_len != 1) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Missing or malformed CIP Padding");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	u8 control_mic_pad = *cip_pad;

	if ((hapd->conf->max_cip_padding_delay) &&
	    control_mic_pad > hapd->conf->max_cip_padding_delay) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "CIP Capabilities: MIC padding delay %u "
			       "exceeds max allowed %u",
			       control_mic_pad, hapd->conf->max_cip_padding_delay);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpa_printf(MSG_DEBUG, "CIP Capabilities: MIC padding delay %u accepted "
		   "for "MACSTR" ", control_mic_pad, MAC2STR(sta->addr));
	sta->control_mic_pad = control_mic_pad;
	return WLAN_STATUS_SUCCESS;
}

static u16 check_wmm(struct hostapd_data *hapd, struct sta_info *sta,
		     const u8 *wmm_ie, size_t wmm_ie_len)
{
	sta->flags &= ~WLAN_STA_WMM;
	sta->qosinfo = 0;
	if (wmm_ie && hapd->conf->wmm_enabled) {
		struct wmm_information_element *wmm;

		if (!hostapd_eid_wmm_valid(hapd, wmm_ie, wmm_ie_len)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG,
				       "invalid WMM element in association "
				       "request");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		sta->flags |= WLAN_STA_WMM;
		wmm = (struct wmm_information_element *) wmm_ie;
		sta->qosinfo = wmm->qos_info;
	}
	return WLAN_STATUS_SUCCESS;
}

static u16 check_multi_ap(struct hostapd_data *hapd, struct sta_info *sta,
			  const u8 *multi_ap_ie, size_t multi_ap_len)
{
	struct multi_ap_params multi_ap;
	u16 status;

	sta->flags &= ~WLAN_STA_MULTI_AP;

	if (!hapd->conf->multi_ap)
		return WLAN_STATUS_SUCCESS;

	if (!multi_ap_ie) {
		if (!(hapd->conf->multi_ap & FRONTHAUL_BSS)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Non-Multi-AP STA tries to associate with backhaul-only BSS");
			return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
		}

		return WLAN_STATUS_SUCCESS;
	}

	status = check_multi_ap_ie(multi_ap_ie + 4, multi_ap_len - 4,
				   &multi_ap);
	if (status != WLAN_STATUS_SUCCESS)
		return status;

	if (multi_ap.capability && multi_ap.capability != MULTI_AP_BACKHAUL_STA)
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP IE with unexpected value 0x%02x",
			       multi_ap.capability);

	if (multi_ap.profile == MULTI_AP_PROFILE_1 &&
	    (hapd->conf->multi_ap_client_disallow &
	     PROFILE1_CLIENT_ASSOC_DISALLOW)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP Profile-1 clients not allowed");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (multi_ap.profile >= MULTI_AP_PROFILE_2 &&
	    (hapd->conf->multi_ap_client_disallow &
	     PROFILE2_CLIENT_ASSOC_DISALLOW)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP Profile-2 clients not allowed");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (!(multi_ap.capability & MULTI_AP_BACKHAUL_STA)) {
		if (hapd->conf->multi_ap & FRONTHAUL_BSS)
			return WLAN_STATUS_SUCCESS;

		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Non-Multi-AP STA tries to associate with backhaul-only BSS");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (!(hapd->conf->multi_ap & BACKHAUL_BSS))
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Backhaul STA tries to associate with fronthaul-only BSS");

	sta->flags |= WLAN_STA_MULTI_AP;
	return WLAN_STATUS_SUCCESS;
}


static u16 copy_supp_rates(struct hostapd_data *hapd, struct sta_info *sta,
			   struct ieee802_11_elems *elems)
{
	/* Supported rates not used in IEEE 802.11ad/DMG */
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD)
		return WLAN_STATUS_SUCCESS;

	if (!elems->supp_rates) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "No supported rates element in AssocReq");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (elems->supp_rates_len + elems->ext_supp_rates_len >
	    sizeof(sta->supported_rates)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Invalid supported rates element length %d+%d",
			       elems->supp_rates_len,
			       elems->ext_supp_rates_len);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->supported_rates_len = merge_byte_arrays(
		sta->supported_rates, sizeof(sta->supported_rates),
		elems->supp_rates, elems->supp_rates_len,
		elems->ext_supp_rates, elems->ext_supp_rates_len);

	return WLAN_STATUS_SUCCESS;
}

/**
 * hostapd_check_assoc_pureg_rates - Validate station supported rates for pure-G BSS
 * @hapd: Pointer to the hostapd BSS context
 * @sta: Pointer to the sta_info structure
 *
 * The function checks if the @sta supports at least 24 Kbps rate
 * (this is the minimum 802.11g mode rate).
 * If the minimum 802.11g mode rate is supported then the function returns true.
 * Else false.
 *
 * Return: true if the station is allowed to associate, false if the
 *         association must be rejected due to insufficient supported rates.
 */
#ifdef CONFIG_QCN_EXTN

#define MIN_PUREG_RATE		24       /* In Kbps unit */
#define TWICE_MIN_PUREG_RATE	(MIN_PUREG_RATE * 2)

static bool hostapd_check_assoc_pureg_rates(struct hostapd_data *hapd,
					    struct sta_info *sta)
{
	int i;
	u8 max_rate;

	if (!hapd || !hapd->iconf || !hapd->iface || !hapd->iface->current_mode || !sta)
		return true;

	if (hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G ||
	    !hapd->conf->bss_extn.pureg_bss)
		return true;

	max_rate = 0;
	for (i = 0; i < sta->supported_rates_len; i++) {
		u8 rate;

		rate = sta->supported_rates[i] & IEEE80211_RATE_VAL_MASK;
		if (rate > max_rate)
			max_rate = rate;
	}

	if (max_rate >= TWICE_MIN_PUREG_RATE)
		return true;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "Station does not support pureg rates (max=%u.%u Mbps), "
		       "Reject association.", max_rate / 2, (max_rate & 1) ? 5 : 0);

	return false;
}
#else
static inline bool hostapd_check_assoc_pureg_rates(struct hostapd_data *hapd,
						   struct sta_info *sta)
{
	return true;
}
#endif /* CONFIG_QCN_EXTN */


#ifdef CONFIG_OWE

static int owe_group_supported(struct hostapd_data *hapd, u16 group)
{
	int i;
	int *groups = hapd->conf->owe_groups;

	if (group != 19 && group != 20 && group != 21)
		return 0;

	if (!groups)
		return 1;

	for (i = 0; groups[i] > 0; i++) {
		if (groups[i] == group)
			return 1;
	}

	return 0;
}


static u16 owe_process_assoc_req(struct hostapd_data *hapd,
				 struct sta_info *sta, const u8 *owe_dh,
				 u8 owe_dh_len)
{
	struct wpabuf *secret, *pub, *hkey;
	int res;
	u8 prk[SHA512_MAC_LEN], pmkid[SHA512_MAC_LEN];
	const char *info = "OWE Key Generation";
	const u8 *addr[2];
	size_t len[2];
	u16 group;
	size_t hash_len, prime_len;

	if (wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		wpa_printf(MSG_DEBUG, "OWE: Using PMKSA caching");
		return WLAN_STATUS_SUCCESS;
	}

	group = WPA_GET_LE16(owe_dh);
	if (!owe_group_supported(hapd, group)) {
		wpa_printf(MSG_DEBUG, "OWE: Unsupported DH group %u", group);
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	}
	if (group == 19)
		prime_len = 32;
	else if (group == 20)
		prime_len = 48;
	else if (group == 21)
		prime_len = 66;
	else
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;

	if (sta->owe_group == group && sta->owe_ecdh) {
		/* This is a workaround for mac80211 behavior of retransmitting
		 * the Association Request frames multiple times if the link
		 * layer retries (i.e., seq# remains same) fail. The mac80211
		 * initiated retransmission will use a different seq# and as
		 * such, will go through duplicate detection. If we were to
		 * change our DH key for that attempt, there would be two
		 * different DH shared secrets and the STA would likely select
		 * the wrong one. */
		wpa_printf(MSG_DEBUG,
			   "OWE: Try to reuse own previous DH key since the STA tried to go through OWE association again");
	} else {
		crypto_ecdh_deinit(sta->owe_ecdh);
		sta->owe_ecdh = crypto_ecdh_init(group);
	}
	if (!sta->owe_ecdh)
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	sta->owe_group = group;

	secret = crypto_ecdh_set_peerkey(sta->owe_ecdh, 0, owe_dh + 2,
					 owe_dh_len - 2);
	secret = wpabuf_zeropad(secret, prime_len);
	if (!secret) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid peer DH public key");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	wpa_hexdump_buf_key(MSG_DEBUG, "OWE: DH shared secret", secret);

	/* prk = HKDF-extract(C | A | group, z) */

	pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
	if (!pub) {
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* PMKID = Truncate-128(Hash(C | A)) */
	addr[0] = owe_dh + 2;
	len[0] = owe_dh_len - 2;
	addr[1] = wpabuf_head(pub);
	len[1] = wpabuf_len(pub);
	if (group == 19) {
		res = sha256_vector(2, addr, len, pmkid);
		hash_len = SHA256_MAC_LEN;
	} else if (group == 20) {
		res = sha384_vector(2, addr, len, pmkid);
		hash_len = SHA384_MAC_LEN;
	} else if (group == 21) {
		res = sha512_vector(2, addr, len, pmkid);
		hash_len = SHA512_MAC_LEN;
	} else {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	pub = wpabuf_zeropad(pub, prime_len);
	if (res < 0 || !pub) {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	hkey = wpabuf_alloc(owe_dh_len - 2 + wpabuf_len(pub) + 2);
	if (!hkey) {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpabuf_put_data(hkey, owe_dh + 2, owe_dh_len - 2); /* C */
	wpabuf_put_buf(hkey, pub); /* A */
	wpabuf_free(pub);
	wpabuf_put_le16(hkey, group); /* group */
	if (group == 19)
		res = hmac_sha256(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	else if (group == 20)
		res = hmac_sha384(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	else if (group == 21)
		res = hmac_sha512(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	wpabuf_clear_free(hkey);
	wpabuf_clear_free(secret);
	if (res < 0)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	wpa_hexdump_key(MSG_DEBUG, "OWE: prk", prk, hash_len);

	/* PMK = HKDF-expand(prk, "OWE Key Generation", n) */

	os_free(sta->owe_pmk);
	sta->owe_pmk = os_malloc(hash_len);
	if (!sta->owe_pmk) {
		os_memset(prk, 0, SHA512_MAC_LEN);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (group == 19)
		res = hmac_sha256_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	else if (group == 20)
		res = hmac_sha384_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	else if (group == 21)
		res = hmac_sha512_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	os_memset(prk, 0, SHA512_MAC_LEN);
	if (res < 0) {
		os_free(sta->owe_pmk);
		sta->owe_pmk = NULL;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	sta->owe_pmk_len = hash_len;
	os_free(sta->owe_pmkid);
	sta->owe_pmkid = os_memdup(pmkid, PMKID_LEN);
	if (!sta->owe_pmkid) {
		os_free(sta->owe_pmk);
		sta->owe_pmk = NULL;
		sta->owe_pmk_len = 0;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpa_hexdump_key(MSG_DEBUG, "OWE: PMK", sta->owe_pmk, sta->owe_pmk_len);
	wpa_hexdump(MSG_DEBUG, "OWE: PMKID", pmkid, PMKID_LEN);
	wpa_auth_pmksa_add2(hapd->wpa_auth, sta->addr, sta->owe_pmk,
			    sta->owe_pmk_len, pmkid, 0, WPA_KEY_MGMT_OWE,
			    NULL, ap_sta_is_mld(hapd, sta));

	return WLAN_STATUS_SUCCESS;
}


u16 owe_validate_request(struct hostapd_data *hapd, const u8 *peer,
			 const u8 *rsn_ie, size_t rsn_ie_len,
			 const u8 *owe_dh, size_t owe_dh_len)
{
	struct wpa_ie_data data;
	int res;

	if (!rsn_ie || rsn_ie_len < 2) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid RSNE from " MACSTR,
			   MAC2STR(peer));
		return WLAN_STATUS_INVALID_IE;
	}
	rsn_ie -= 2;
	rsn_ie_len += 2;

	res = wpa_parse_wpa_ie_rsn(rsn_ie, rsn_ie_len, &data);
	if (res) {
		wpa_printf(MSG_DEBUG, "Failed to parse RSNE from " MACSTR
			   " (res=%d)", MAC2STR(peer), res);
		wpa_hexdump(MSG_DEBUG, "RSNE", rsn_ie, rsn_ie_len);
		return wpa_res_to_status_code(res);
	}
	if (!(data.key_mgmt & WPA_KEY_MGMT_OWE)) {
		wpa_printf(MSG_DEBUG,
			   "OWE: Unexpected key mgmt 0x%x from " MACSTR,
			   (unsigned int) data.key_mgmt, MAC2STR(peer));
		return WLAN_STATUS_AKMP_NOT_VALID;
	}
	if (!owe_dh) {
		wpa_printf(MSG_DEBUG,
			   "OWE: No Diffie-Hellman Parameter element from "
			   MACSTR, MAC2STR(peer));
		return WLAN_STATUS_AKMP_NOT_VALID;
	}

	return WLAN_STATUS_SUCCESS;
}


u16 owe_process_rsn_ie(struct hostapd_data *hapd,
		       struct sta_info *sta,
		       const u8 *rsn_ie, size_t rsn_ie_len,
		       const u8 *owe_dh, size_t owe_dh_len,
		       const u8 *link_addr)
{
	u16 status;
	u8 *owe_buf, ie[256 * 2];
	size_t ie_len = 0;
	enum wpa_validate_result res;

	if (!rsn_ie || rsn_ie_len < 2) {
		wpa_printf(MSG_DEBUG, "OWE: No RSNE in (Re)AssocReq");
		status = WLAN_STATUS_INVALID_IE;
		goto end;
	}

	if (!sta->wpa_sm)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,	sta->addr,
						NULL);
	if (!sta->wpa_sm) {
		wpa_printf(MSG_WARNING,
			   "OWE: Failed to initialize WPA state machine");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		wpa_auth_set_ml_info(sta->wpa_sm,
				     sta->mld_assoc_link_id, &sta->mld_info);
#endif /* CONFIG_IEEE80211BE */
	rsn_ie -= 2;
	rsn_ie_len += 2;
	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  hapd->iface->freq, rsn_ie, rsn_ie_len,
				  NULL, 0, NULL, 0, owe_dh, owe_dh_len, NULL,
				  ap_sta_is_mld(hapd, sta), false, NULL, false);
	status = wpa_res_to_status_code(res);
	if (status != WLAN_STATUS_SUCCESS)
		goto end;
	status = owe_process_assoc_req(hapd, sta, owe_dh, owe_dh_len);
	if (status != WLAN_STATUS_SUCCESS)
		goto end;
	owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, ie, sizeof(ie));
	if (!owe_buf) {
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}

	if (sta->owe_ecdh) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto end;
		}

		/* OWE Diffie-Hellman Parameter element */
		*owe_buf++ = WLAN_EID_EXTENSION; /* Element ID */
		*owe_buf++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*owe_buf++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension
							 */
		WPA_PUT_LE16(owe_buf, sta->owe_group);
		owe_buf += 2;
		os_memcpy(owe_buf, wpabuf_head(pub), wpabuf_len(pub));
		owe_buf += wpabuf_len(pub);
		wpabuf_free(pub);
		sta->external_dh_updated = 1;
	}
	ie_len = owe_buf - ie;

end:
	wpa_printf(MSG_DEBUG, "OWE: Update status %d, ie len %d for peer "
			      MACSTR, status, (unsigned int) ie_len,
			      MAC2STR(link_addr ? link_addr : sta->addr));
	hostapd_drv_update_dh_ie(hapd, link_addr ? link_addr : sta->addr,
				 status,
				 status == WLAN_STATUS_SUCCESS ? ie : NULL,
				 ie_len);

	return status;
}

#endif /* CONFIG_OWE */


static bool hapd_is_known_sta(struct hostapd_data *hapd, struct sta_info *sta,
			      const u8 *ies, size_t ies_len)
{
	const u8 *ie, *pos, *end, *timestamp_pos, *mic;
	u64 timestamp;
	u8 mic_len;

	if (!hapd->conf->known_sta_identification)
		return false;

	ie = get_ie_ext(ies, ies_len, WLAN_EID_EXT_KNOWN_STA_IDENTIFICATION);
	if (!ie)
		return false;

	pos = ie + 3;
	end = &ie[2 + ie[1]];
	if (end - pos < 8 + 1)
		return false; /* truncated element */
	timestamp_pos = pos;
	timestamp = WPA_GET_LE64(pos);
	pos += 8;
	mic_len = *pos++;
	if (mic_len > end - pos)
		return false; /* truncated element */
	mic = pos;

	wpa_printf(MSG_DEBUG, "RSN: STA " MACSTR
		   " included Known STA Identification element: Timestamp=0x%llx mic_len=%u",
		   MAC2STR(sta->addr), (unsigned long long) timestamp, mic_len);

	if (timestamp <= sta->last_known_sta_id_timestamp) {
		wpa_printf(MSG_DEBUG,
			   "RSN: Ignore reused or old Known STA Identification");
		return false;
	}

	if (!wpa_auth_sm_known_sta_identification(sta->wpa_sm, timestamp_pos,
						  mic, mic_len)) {
		wpa_printf(MSG_DEBUG,
			   "RSN: Ignore Known STA Identification with invalid MIC or due to KCK not available");
		return false;
	}

	wpa_printf(MSG_DEBUG, "RSN: Valid Known STA Identification");
	sta->last_known_sta_id_timestamp = timestamp;

	return true;
}

static bool hostapd_skip_sa_query(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  struct sta_info *current_sta)
{

	if (!hapd->conf->disable_sa_query || !current_sta)
		return false;

	if (current_sta->auth_alg == WLAN_AUTH_SAE) {
		/*
		 * Skip SA Query for SAE only after authentication confirm
		 * completed, but force the cleanup path for already associated
		 * MFP STAs to avoid stale peer state on immediate reassociation.
		 */
		if (!current_sta->sae || current_sta->sae->state != SAE_ACCEPTED) {
			wpa_printf(MSG_DEBUG,
				   "SA Query disabled but SAE not accepted for STA "
				   MACSTR, MAC2STR(current_sta->addr));
			return false;
		}

		wpa_printf(MSG_DEBUG,
			   "SA Query skipped for SAE STA " MACSTR,
			    MAC2STR(current_sta->addr));
		current_sta->skip_sa_query = 1;
		sta->skip_sa_query = 1;

		return true;
	}

	return false;
}

static bool check_sa_query(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc, const u8 *ies, size_t ies_len,
			   struct sta_info *current_sta, bool enc_assoc)
{
	struct hostapd_data *assoc_hapd;
	struct sta_info *assoc_sta;

	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (assoc_sta) {
		sta = assoc_sta;
		hapd = assoc_hapd;
	}

	if ((sta->flags &
	     (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED))
		return false;

#ifdef CONFIG_ENC_ASSOC
	if (enc_assoc && sta->epp_sta) {
		/*
		 * Skip SA Query and association comeback since either:
		 * - For a protected (Re)Association without a new EPPKE
		 *   Authentication, the STA has already demonstrated
		 *   possession of the PTK that is in use by the existing
		 *   association; or
		 * - For a (Re)Association following a new EPPKE Authentication,
		 *   the authentication exchange has already authenticated the
		 *   STA, with the Authentication comeback procedures providing
		 *   protection for the existing association covering the MAC
	         *   addresses indicated in the Authentication frame. Any
		 *   additional addresses indicated in the (Re)Association
		 *   Request frame are covered by the AP MLD rejection rules for
		 *   ML (re)setup procedures in IEEE P802.11-REVmf/D2.1, 35.3.5.1.
		 */
		return false;
	}
#endif /* CONFIG_ENC_ASSOC */

	if (!sta->sa_query_timed_out && sta->sa_query_count > 0)
		ap_check_sa_query_timeout(hapd, sta);

	/*
	 * For an FT reassociation, the (Re)Association Request is already
	 * protected by the FT authentication that just completed, so an SA
	 * Query must not be initiated. Note that hostapd_ml_get_assoc_sta()
	 * above may have replaced 'sta' with the assoc-link STA whose auth_alg
	 * still reflects the previous (non-FT) association; check the auth_alg
	 * of the incoming (Re)Association attempt (current_sta) as well so a
	 * successful FT roam onto a different link of an AP MLD is not rejected
	 * temporarily with a spurious SA Query.
	 */
	if (!sta->sa_query_timed_out &&
	    (!reassoc ||
	     (sta->auth_alg != WLAN_AUTH_FT &&
	      (!current_sta || current_sta->auth_alg != WLAN_AUTH_FT)))) {
		if (hapd_is_known_sta(hapd, sta, ies, ies_len))
			return false;

		if (hostapd_skip_sa_query(hapd, sta, current_sta))
			return false;
		/*
		 * STA has already been associated with MFP and SA Query timeout
		 * has not been reached. Reject the association attempt
		 * temporarily and start SA Query, if one is not pending.
		 */
		if (sta->sa_query_count == 0)
			ap_sta_start_sa_query(hapd, sta);

		if (current_sta)
			current_sta->sa_query_triggered_sta = sta;

		return true;
	}

	return false;
}


#if defined(CONFIG_ENC_ASSOC) || defined(CONFIG_IEEE8021X_AUTH)
/* Return True if Authentication comeback is needed */
bool hostapd_find_auth_comeback_sta(struct hostapd_data *hapd, const u8 *addr,
				    const u8 *ies, size_t ies_len,
				    struct hostapd_data **o_hapd,
				    struct sta_info **o_sta)
{
	struct hostapd_data *t_hapd = NULL;
	struct sta_info *t_sta = NULL;
	bool sa_query_need = false;
	int i, j;

	if (!hapd || !hapd->iface || !hapd->iface->interfaces || !addr ||
	    is_zero_ether_addr(addr))
		return false;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		for (j = 0; j < hapd->iface->interfaces->iface[i]->num_bss; j++)
		{
			t_hapd = hapd->iface->interfaces->iface[i]->bss[j];
			if (!t_hapd || !t_hapd->started)
				continue;

			t_sta = ap_get_sta(t_hapd, addr);
			if (t_sta) {
				wpa_printf(MSG_DEBUG, "Found an existing STA "
					   MACSTR, MAC2STR(addr));
				sa_query_need = check_sa_query(t_hapd, t_sta, 0,
							       ies, ies_len, NULL,
							       false);
				if (!sa_query_need) {
#ifdef CONFIG_IEEE80211BE
					/* Remove older Association on all
					 * affiliated AP MLD links */
					if (t_hapd->conf->mld_ap)
						ap_sta_remove_link_sta(t_hapd,
								       t_sta, 0, false);
#endif /* CONFIG_IEEE80211BE */
					ap_free_sta(t_hapd, t_sta);
					return false;
				}

				*o_hapd = t_hapd;
				*o_sta = t_sta;
				return true;
			}
#ifdef CONFIG_IEEE80211BE
			/* Check for an existing link STA with ‘addr’ */
			else {
				for (t_sta = t_hapd->sta_list; t_sta;
				     t_sta = t_sta->next) {
					struct mld_info *mld_info =
							&t_sta->mld_info;
					int i;

					for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
						if (!mld_info->links[i].valid ||
						    !ether_addr_equal(
							mld_info->links[i].peer_addr, addr))
						continue;

						wpa_printf(MSG_DEBUG, "Found an "
							   "existing link STA " MACSTR,
							   MAC2STR(addr));
						sa_query_need = check_sa_query(t_hapd,
									       t_sta, 0,
									       ies,
									       ies_len,
									       NULL,
									       false);
						if (!sa_query_need) {
							/* Remove older Association
							 * on all affiliated AP MLD
							 * links */
							if (t_hapd->conf->mld_ap)
								ap_sta_remove_link_sta(
									t_hapd, t_sta, 0, false);
							ap_free_sta(t_hapd, t_sta);
							return false;
						}

						*o_hapd = t_hapd;
						*o_sta = t_sta;
						return true;
					}
				}
			}
#endif /* CONFIG_IEEE80211BE */
		}
	}
	return false;
}
#endif /* CONFIG_ENC_ASSOC || CONFIG_IEEE8021X_AUTH */


int start_unsolicited_sa_query(struct hostapd_data *hapd, struct sta_info *sta)
{
	if ((sta->flags &
	     (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_ERROR, "ERROR! SA Query request in improper state\n");
		return -1;
	}

	if (sta->sa_query_count != 0) {
		wpa_printf(MSG_INFO, "INFO! SA Query already in progress\n");
		return -1;
	}

	ap_sta_start_sa_query(hapd, sta);
	return 0;
}

#ifdef CONFIG_IEEE80211BE
static bool check_sa_query_partner_link(struct hostapd_data *hapd, struct sta_info *sta,
					 enum link_parse_type type, const u8 *ies, size_t ies_len)
{
	struct hostapd_data *bss;
	struct sta_info *lsta;
	int i, j, k;
	bool triggered = false, epp_sta = false;

	if (sta->auth_alg == WLAN_AUTH_FT)
		return false;

#ifdef CONFIG_ENC_ASSOC
	if (sta->epp_sta)
		epp_sta = true;
#endif /* CONFIG_ENC_ASSOC */

	if (sta->unadded_sta &&
	    (sta->flags & WLAN_STA_AUTH)) {
		for_each_mld_link(bss, hapd) {
			if (bss == hapd)
				continue;
			lsta = ap_get_sta(bss, sta->addr);
			if (lsta && check_sa_query(bss, lsta, type, ies, ies_len, sta,
						   /* enc_assoc -
						    * check_sa_query_partner_link is
						    * called from (Re)Association Request
						    * processing. For EPP peer, the driver
						    * ensures that all (Re)Asscoation
						    * Request frames received for an EPP
						    * peer are encrypted. Hence it would
						    * suffice to check if the frame was
						    * received from an EPP peer
						    */
						   epp_sta ? true : false))
				return true;
		}
	}

	/*
	 * Check per-link MAC addresses of the incoming station's partner
	 * links (all links except the assoc link) against all STAs across
	 * all BSSes on all interfaces.  This is independent of the
	 * unadded_sta state — a per-link address conflict can arise for any
	 * incoming MLD STA.
	 *
	 * All partner links are checked even after the first conflict is
	 * found, because each link address may conflict with a different
	 * existing station.  SA Query is initiated for every such station;
	 * the caller rejects the association temporarily if at least one
	 * SA Query was triggered.
	 *
	 * STA entries are keyed by MLD address, so for each BSS we walk its
	 * sta_list and compare the per-link peer_addr stored at
	 * hapd_ptr->mld_link_id — the link index that BSS owns.
	 */
	if (!ap_sta_is_mld(hapd, sta))
		return false;

	for (k = 0; k < MAX_NUM_MLD_LINKS; k++) {
		struct hostapd_data *hapd_ptr;
		struct sta_info *osta;
		const u8 *link_addr;
		bool found;

		/* Only check partner links, not the assoc link */
		if (k == hapd->mld_link_id)
			continue;
		if (!sta->mld_info.links[k].valid)
			continue;

		link_addr = sta->mld_info.links[k].peer_addr;
		found = false;

		for (i = 0; i < hapd->iface->interfaces->count && !found; i++) {
			for (j = 0; j < hapd->iface->interfaces->iface[i]->num_bss && !found; j++) {
				hapd_ptr = hapd->iface->interfaces->iface[i]->bss[j];
				if (!hapd_ptr || !hapd_ptr->started)
					continue;

				/* Fast path: link_addr is the station's
				 * primary address */
				osta = ap_get_sta(hapd_ptr, link_addr);

				/* STA entries are keyed by MLD address;
				 * if not found above, compare the per-link
				 * peer_addr at hapd_ptr's own link ID —
				 * no need to scan all link indices. */
				if (!osta && hapd_ptr->conf->mld_ap) {
					struct sta_info *tmp;

					for (tmp = hapd_ptr->sta_list;
					     tmp; tmp = tmp->next) {
						if (!tmp->mld_info.mld_sta)
							continue;
						if (!tmp->mld_info.links[hapd_ptr->mld_link_id].valid)
							continue;
						if (ether_addr_equal(
							tmp->mld_info.links[hapd_ptr->mld_link_id].peer_addr,
							link_addr)) {
							osta = tmp;
							break;
						}
					}
				}

				if (osta) {
					wpa_printf(MSG_DEBUG,
						   "MLD: partner link %d addr "
						   MACSTR " conflicts with STA "
						   MACSTR " on BSS %s",
						   k, MAC2STR(link_addr),
						   MAC2STR(osta->addr),
						   hapd_ptr->conf->iface);

					if (check_sa_query(hapd_ptr, osta,
							   type, ies, ies_len,
							   sta,
						/* enc_assoc -
						 * check_sa_query_partner_link is
						 * called from (Re)Association Request
						 * processing. For EPP peer, the driver
						 * ensures that all (Re)Asscoation
						 * Request frames received for an EPP
						 * peer are encrypted. Hence it would
						 * suffice to check if the frame was
						 * received from an EPP peer
						 */
						epp_sta ? true : false)) {
						triggered = true;
						sta->link_addr_conflict_bitmap |= BIT(k);
					}

					/* One conflicting STA per link address
					 * is sufficient; move on to the next
					 * partner link. */
					found = true;
				}
			}
		}
	}
	return triggered;
}
#endif /* CONFIG_IEEE80211BE */


/*
 * hostapd_security_profile_ie_len - Calculate length of UHR Security Info IE
 *
 * Returns the total byte length of the IE (including EID and Length fields),
 * or 0 if no profiles are configured.
 */
size_t hostapd_security_profile_ie_len(struct hostapd_data *hapd)
{
	int i, max_profile = -1;
	size_t bitmap_len, ext_rsn_capab_len = 0;
	u8 rsnxe_buf[2 + sizeof(u64)];
	u8 *rsnxe_end;

	if (!hapd || !hapd->conf || !hapd->conf->security_profiles)
		return 0;

	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		if (hapd->conf->security_profiles[i] > max_profile)
			max_profile = hapd->conf->security_profiles[i];
	}

	if (max_profile < 0)
		return 0;

	/* Bitmap size: ceil((max_profile + 1) / 8) */
	bitmap_len = (max_profile / 8) + 1;

	rsnxe_end = hostapd_eid_rsnxe(hapd, rsnxe_buf, sizeof(rsnxe_buf), ~0ULL);
	if (rsnxe_end > rsnxe_buf + 2)
		ext_rsn_capab_len = rsnxe_end - rsnxe_buf - 2;

	/* If any configured profile mandates SAE-H2E, ensure space for the bit */
	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		int p = hapd->conf->security_profiles[i];

		if (p == SECURITY_PROFILE_NUM_EPPKE_SAE ||
		    p == SECURITY_PROFILE_NUM_EPPKE_FT_SAE ||
		    p == SECURITY_PROFILE_NUM_SAE ||
		    p == SECURITY_PROFILE_NUM_FT_SAE) {
			if (ext_rsn_capab_len < 1)
				ext_rsn_capab_len = 1;
			break;
		}
	}

	/*
	 * D1.4 format:
	 * EID (1) + Length (1) + EID_Ext (1) + Reduced_RSN_Capab (1) +
	 * Security_Profile_Indication (1) + Security_Profile_Bitmap (bitmap_len) +
	 * Ext_RSN_Capab (ext_rsn_capab_len)
	 */
	return 2 + 1 + 1 + 1 + bitmap_len + ext_rsn_capab_len;
}


/*
 * hostapd_eid_security_profile - Build UHR Security Information element
 *
 * Writes the D1.4 bitmap-format Security Profile element into @eid and
 * returns a pointer past the last written byte.
 */
u8 *hostapd_eid_security_profile(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 *len_pos;
	u8 reduced_rsn_capab = 0;
	u8 ext_rsn_capab[256];
	size_t ext_rsn_capab_len = 0;
	u8 bitmap[16]; /* max 128 profiles */
	size_t bitmap_len = 0;
	int i, max_profile = -1;
	u8 rsnxe_buf[2 + sizeof(u64)];
	u8 *rsnxe_end;

	if (!hapd || !hapd->conf || !hapd->conf->security_profiles)
		return eid;

	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		if (hapd->conf->security_profiles[i] > max_profile)
			max_profile = hapd->conf->security_profiles[i];
	}

	if (max_profile < 0)
		return eid;

	/* Build bitmap */
	bitmap_len = (max_profile / 8) + 1;
	if (bitmap_len > sizeof(bitmap))
		bitmap_len = sizeof(bitmap);
	os_memset(bitmap, 0, bitmap_len);
	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		int p = hapd->conf->security_profiles[i];

		if (p / 8 < (int) bitmap_len)
			bitmap[p / 8] |= BIT(p % 8);
	}

	rsnxe_end = hostapd_eid_rsnxe(hapd, rsnxe_buf, sizeof(rsnxe_buf), ~0ULL);
	if (rsnxe_end > rsnxe_buf + 2) {
		ext_rsn_capab_len = rsnxe_end - rsnxe_buf - 2;
		if (ext_rsn_capab_len > sizeof(ext_rsn_capab))
			ext_rsn_capab_len = sizeof(ext_rsn_capab);
		os_memcpy(ext_rsn_capab, rsnxe_buf + 2, ext_rsn_capab_len);
	}

	/* If any configured profile mandates SAE-H2E, force the H2E bit */
	for (i = 0; hapd->conf->security_profiles[i] >= 0; i++) {
		int p = hapd->conf->security_profiles[i];

		if (p == SECURITY_PROFILE_NUM_EPPKE_SAE ||
		    p == SECURITY_PROFILE_NUM_EPPKE_FT_SAE ||
		    p == SECURITY_PROFILE_NUM_SAE ||
		    p == SECURITY_PROFILE_NUM_FT_SAE) {
			if (ext_rsn_capab_len < 1) {
				ext_rsn_capab_len = 1;
				os_memset(ext_rsn_capab, 0, 1);
			}
			ext_rsn_capab[0] |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
			break;
		}
	}

	/* Build Reduced RSN Capabilities */
	if (hapd->conf->security_profile_ext_key_id)
		reduced_rsn_capab |=
			WLAN_SEC_PROF_REDUCED_RSN_CAPA_EXTENDED_KEY_ID;
	if (hapd->conf->security_profile_ocvc)
		reduced_rsn_capab |= WLAN_SEC_PROF_REDUCED_RSN_CAPA_OCVC;

	/* Write IE */
	*pos++ = WLAN_EID_EXTENSION;
	len_pos = pos++;  /* Length field, filled later */
	*pos++ = WLAN_EID_EXT_SECURITY_PROFILE;

	/* Reduced RSN Capabilities (1 byte) */
	*pos++ = reduced_rsn_capab;

	/* Security Profile Indication (1 byte):
	 * B0-B3: Number of octets in Security Profile Bitmap
	 * B4-B7: Number of Vendor Specific Security Profiles (0)
	 */
	*pos++ = (u8) bitmap_len;

	/* Security Profile Bitmap */
	os_memcpy(pos, bitmap, bitmap_len);
	pos += bitmap_len;

	/* Extended RSN Capabilities */
	if (ext_rsn_capab_len > 0) {
		os_memcpy(pos, ext_rsn_capab, ext_rsn_capab_len);
		pos += ext_rsn_capab_len;
	}

	/* Fill length */
	*len_pos = (u8) (pos - len_pos - 1);
	return pos;
}

static bool hostapd_deny_non_ht_assoc(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	bool require_ht;

	if (!hapd->iconf->ieee80211n)
		return false;

	require_ht = hapd->iconf->require_ht || hapd->conf->bss_require_ht;
#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->bss_extn.puren_bss.is_overridden)
		require_ht |= hapd->conf->bss_extn.puren_bss.value;
#endif
	if (require_ht && !(sta->flags & WLAN_STA_HT))
		return true;

	return false;
}

static bool hostapd_deny_non_vht_assoc(struct hostapd_data *hapd,
				       struct sta_info *sta)
{
	bool require_vht;

	if (!hapd->iconf->ieee80211ac)
		return false;

	require_vht = hapd->iconf->require_vht || hapd->conf->bss_require_vht;
#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->bss_extn.pure11ac_bss.is_overridden)
		require_vht |= hapd->conf->bss_extn.pure11ac_bss.value;
#endif
	if (require_vht && !(sta->flags & WLAN_STA_VHT))
		return true;

	return false;
}

static bool hostapd_deny_non_he_assoc(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	bool require_he;

	if (!hostapd_is_he_enabled(hapd))
		return false;

	require_he = hapd->iconf->require_he || hapd->conf->bss_require_he;
#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->bss_extn.pure11ax_bss.is_overridden)
		require_he |= hapd->conf->bss_extn.pure11ax_bss.value;
#endif
	if (require_he && !(sta->flags & WLAN_STA_HE))
		return true;

	return false;
}

static int __check_assoc_ies(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *ies, size_t ies_len,
			     struct ieee802_11_elems *elems,
			     enum link_parse_type type,
			     struct wpa_state_machine *assoc_wpa_sm)
{
	int resp;
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	const u8 *p2p_dev_addr = NULL;
	const struct element *elem;
	bool pmk_cache_based_sae;
	struct security_profile_entry_ap matched_profile;
	bool security_profile_matched = false;
#ifdef CONFIG_SAE
	bool epp_sta = false;
#ifdef CONFIG_ENC_ASSOC
	epp_sta = sta->epp_sta;
#endif /* CONFIG_ENC_ASSOC */
#endif /* CONFIG_SAE */
#ifdef CONFIG_PMKSA_PRIVACY
	bool derive_next_pmkid = true;
#endif /* CONFIG_PMKSA_PRIVACY */
#ifdef CONFIG_IEEE8021X_AUTH
	bool mic_check = true;
#endif /* CONFIG_IEEE8021X_AUTH */
	bool smd_flag = false ;

	for_each_element(elem, ies, ies_len) {
		memcpy(sta->vendor_oui, elem->data, 3);
		if(elem->id == WLAN_EID_VENDOR_SPECIFIC &&
		   !WPA_GET_BE24(sta->vendor_oui))
			break;
	}

	os_memset(&matched_profile, 0, sizeof(matched_profile));

	if (type == LINK_PARSE_ASSOC || type == LINK_PARSE_REASSOC) {
		resp = check_ssid(hapd, sta, elems->ssid, elems->ssid_len);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;
	}

	resp = check_wmm(hapd, sta, elems->wmm, elems->wmm_len);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;
	resp = check_ext_capab(hapd, sta, elems->ext_capab,
			       elems->ext_capab_len);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;
	resp = copy_supp_rates(hapd, sta, elems);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;

	resp = check_multi_ap(hapd, sta, elems->multi_ap, elems->multi_ap_len);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;

#ifdef CONFIG_QCN_EXTN
	/* WDS vendor IE: parse from assoc request and set sta_extn.wds_ie_peer */
	resp = check_wds_ie_extn(hapd, sta,
				 elems->elems_extn.wds_ie,
				 elems->elems_extn.wds_ie_len);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;
#endif /* CONFIG_QCN_EXTN */

	resp = copy_sta_ht_capab(hapd, sta, elems->ht_capabilities);
	if (resp != WLAN_STATUS_SUCCESS)
		goto out;
	if (hostapd_deny_non_ht_assoc(hapd, sta)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "Station does not support "
			       "mandatory HT PHY - reject association");
		resp = WLAN_STATUS_ASSOC_DENIED_NO_HT;
		goto out;
	}

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac) {
		resp = copy_sta_vht_capab(hapd, sta, elems->vht_capabilities);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;

		resp = set_sta_vht_opmode(hapd, sta, elems->opmode_notif);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;
	}

	if (hostapd_deny_non_vht_assoc(hapd, sta)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "Station does not support "
			       "mandatory VHT PHY - reject association");
		resp = WLAN_STATUS_ASSOC_DENIED_NO_VHT;
		goto out;
	}

	if (hapd->conf->vendor_vht && !elems->vht_capabilities) {
		resp = copy_sta_vendor_vht(hapd, sta, elems->vendor_vht,
					   elems->vendor_vht_len);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;
	}

#ifdef CONFIG_QCN_EXTN
	if (is_mu_cap_war_active(hapd) && is_sta_vht_only(sta))
		hostapd_mu_cap_war_client_cap_extn(hapd, sta);

	hostapd_drv_set_vht_mcs_10_11_supp_extn(hapd, sta, &elems->elems_extn);
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		resp = copy_sta_he_capab(hapd, sta, IEEE80211_MODE_AP,
					 elems->he_capabilities,
					 elems->he_capabilities_len);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;

#ifdef CONFIG_QCN_EXTN
		hostapd_drv_set_peer_he_mcs_12_13_cap_extn(hapd, &elems->elems_extn);

		hostapd_drv_set_he_400ns_sig_2xltf_160_supp_extn(hapd, sta,
								 &elems->elems_extn);
#endif /* CONFIG_QCN_EXTN */
		if (hostapd_deny_non_he_assoc(hapd, sta)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station does not support mandatory HE PHY - reject association");
			resp = WLAN_STATUS_DENIED_HE_NOT_SUPPORTED;
			goto out;
		}

		if (is_6ghz_op_class(hapd->iconf->op_class)) {
			if (!(sta->flags & WLAN_STA_HE)) {
				hostapd_logger(hapd, sta->addr,
					       HOSTAPD_MODULE_IEEE80211,
					       HOSTAPD_LEVEL_INFO,
					       "Station does not support mandatory HE PHY - reject association");
				resp = WLAN_STATUS_DENIED_HE_NOT_SUPPORTED;
				goto out;
			}
			resp = copy_sta_he_6ghz_capab(hapd, sta,
						      elems->he_6ghz_band_cap);
			if (resp != WLAN_STATUS_SUCCESS)
				goto out;
		}
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		bool require_eht = hapd->iconf->require_eht ||
				   hapd->conf->bss_require_eht;
		resp = copy_sta_eht_capab(hapd, sta, IEEE80211_MODE_AP,
					  elems->he_capabilities,
					  elems->he_capabilities_len,
					  elems->eht_capabilities,
					  elems->eht_capabilities_len);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;

#ifdef CONFIG_QCN_EXTN
		resp = hostapd_copy_sta_eht_240mhz_cap_extn(hapd, sta,
							    IEEE80211_MODE_AP,
							    &elems->elems_extn);
#endif /* CONFIG_QCN_EXTN */
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;

		if (require_eht && !(sta->flags & WLAN_STA_EHT)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station does not support mandatory EHT PHY - reject association");
			return WLAN_STATUS_DENIED_EHT_NOT_SUPPORTED;
		}

		if (!assoc_wpa_sm) {
			resp = hostapd_process_ml_assoc_req(hapd, elems, sta);
			if (resp != WLAN_STATUS_SUCCESS)
				goto out;
		}
	}
#endif /* CONFIG_IEEE80211BE */


		if (hapd->conf->security_profiles) {

			resp = validate_security_profile_common(hapd, sta, elems,
								type == LINK_PARSE_REASSOC ?
								"Reassoc" : "Assoc", &matched_profile);
			if (resp != WLAN_STATUS_SUCCESS) {
				resp = WLAN_STATUS_REJECTED_INVALID_SECURITY_PROFILE;
				goto out;
			}
			wpa_printf(MSG_DEBUG,
				   "UHR: (Re)Assoc Security Profile validated "
				   "for " MACSTR, MAC2STR(sta->addr));

			if (matched_profile.profile_num != 0) {
				wpa_printf(MSG_ERROR," %d %s %d \n", matched_profile.profile_num, __func__, __LINE__);
				security_profile_matched = true;
			}
		}

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		resp = copy_sta_uhr_capab(hapd, sta,
				  elems->uhr_capabilities,
				  elems->uhr_capabilities_len);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_P2P
	if (elems->p2p && ies && ies_len) {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = ieee802_11_vendor_ie_concat(ies, ies_len,
							  P2P_IE_VENDOR_TYPE);
		if (sta->p2p_ie)
			p2p_dev_addr = p2p_get_go_dev_addr(sta->p2p_ie);
	} else {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = NULL;
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_IEEE8021X_AUTH
	/* Per IEEE 802.11bi/D4.0, 12.16.6 ((Re)Association Request/Response
	 * frame encryption), if IEEE 802.1X is used and FT protocol is not
	 * used, the EPP non-AP STA shall include a MIC element in the
	 * (Re)Association Request frame.
	 * Skip MIC validation on partner AP MLD links.
	 */
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    hapd->mld_link_id != sta->mld_assoc_link_id)
		mic_check = false;
#endif /* CONFIG_IEEE80211BE */
	if (ap_sta_is_epp(sta) && sta->auth_alg == WLAN_AUTH_802_1X &&
	    mic_check) {
		const u8 *data;
		u8 mic_len, data_buf[(255 + 2) * 2], mic[WPA_1X_MAX_MIC_LEN];
		const u8 *aa = hapd->own_addr;
		size_t data_len = 0;
		int ret;

		if (!elems->mic) {
			wpa_printf(MSG_DEBUG, "802.1X: Missing MIC element");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}

		if (wpa_key_mgmt_sha384(sta->eap_auth_data.akm))
			mic_len = SHA384_MAC_LEN / 2;
		else
			mic_len = SHA256_MAC_LEN / 2;

		if (mic_len != elems->mic_len) {
			wpa_printf(MSG_DEBUG, "802.1X: Invalid MIC len: %u",
				   elems->mic_len);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}

#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta))
			aa = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

		os_memcpy(data_buf, elems->rsn_ie - 2, elems->rsn_ie_len + 2);
		data_len += 2 + elems->rsn_ie_len;
		os_memcpy(data_buf + data_len, elems->rsnxe - 2,
			  elems->rsnxe_len + 2);
		data_len += 2 + elems->rsnxe_len;

		data = data_buf;
		ret = wpa_auth_8021x_mic(sta->eap_auth_data.akm,
					 sta->eap_auth_data.ptk.kck,
					 sta->eap_auth_data.ptk.kck_len, aa,
					 sta->addr, data, data_len,
					 NULL, 0, mic);
		wpa_hexdump_key(MSG_DEBUG, "802.1X: Frame MIC",
				elems->mic, elems->mic_len);
		if (ret || os_memcmp(mic, elems->mic, mic_len) != 0) {
			wpa_printf(MSG_INFO, "802.1X: Failed MIC verification");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
	}
#endif /* CONFIG_IEEE8021X_AUTH */

	/* Link Reconfiguration Request frame for add link operation will not
	 * have RSN and other security IEs. So, skip the checks.
	 */
	if (type == LINK_PARSE_RECONF || type == LINK_PARSE_UHR_RECONF_LINK) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Skip security IE checks for Link Reconfiguration request");
		goto skip_wpa_ies;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && elems->rsn_ie) {
		wpa_ie = elems->rsn_ie;
		wpa_ie_len = elems->rsn_ie_len;
	} else if ((hapd->conf->wpa & WPA_PROTO_WPA) &&
		   elems->wpa_ie) {
		wpa_ie = elems->wpa_ie;
		wpa_ie_len = elems->wpa_ie_len;
	} else {
		wpa_ie = NULL;
		wpa_ie_len = 0;
	}

#ifdef CONFIG_WPS
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);
	if (hapd->conf->wps_state && elems->wps_ie && ies && ies_len) {
		wpa_printf(MSG_DEBUG, "STA included WPS IE in (Re)Association "
			   "Request - assume WPS is used");
		sta->flags |= WLAN_STA_WPS;
		wpabuf_free(sta->wps_ie);
		sta->wps_ie = ieee802_11_vendor_ie_concat(ies, ies_len,
							  WPS_IE_VENDOR_TYPE);
		if (sta->wps_ie && wps_is_20(sta->wps_ie)) {
			wpa_printf(MSG_DEBUG, "WPS: STA supports WPS 2.0");
			sta->flags |= WLAN_STA_WPS2;
		}
		wpa_ie = NULL;
		wpa_ie_len = 0;
		if (sta->wps_ie && wps_validate_assoc_req(sta->wps_ie) < 0) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid WPS IE in "
				   "(Re)Association Request - reject");
			resp = WLAN_STATUS_INVALID_IE;
			goto out;
		}
	} else if (hapd->conf->wps_state && wpa_ie == NULL) {
		wpa_printf(MSG_DEBUG, "STA did not include WPA/RSN IE in "
			   "(Re)Association Request - possible WPS use");
		sta->flags |= WLAN_STA_MAYBE_WPS;
	} else
#endif /* CONFIG_WPS */
	if (hapd->conf->wpa && wpa_ie == NULL) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "No WPA/RSN IE in association request");
		resp = WLAN_STATUS_INVALID_IE;
		goto out;
	}
	sta->control_mic_pad = CONTROL_MIC_PAD_NOT_SET;
	if (hapd->conf->control_frame_prot &&
	    (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_CIGTK) &&
	    (elems->cip_pad && (elems->cip_pad_len >= 0))) {
		resp = check_cip_padding_delay(hapd, sta, elems->cip_pad, elems->cip_pad_len);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;
	}

	if (hapd->conf->wpa && wpa_ie) {
		enum wpa_validate_result res;
#ifdef CONFIG_IEEE80211BE
		struct mld_info *info = &sta->mld_info;
		bool init = !sta->wpa_sm;
		if (!assoc_wpa_sm && check_sa_query_partner_link(hapd, sta, type, ies, ies_len))
			return WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
#endif /* CONFIG_IEEE80211BE */

		wpa_ie -= 2;
		wpa_ie_len += 2;
#ifdef CONFIG_IEEE80211BE
		if (!assoc_wpa_sm && sta->mld_info.mld_sta && sta->mld_assoc_link_id != hapd->mld_link_id) {
			struct hostapd_data *bss;
			struct sta_info *lsta;
			u8 link_id = hapd->mld_link_id;
			wpa_printf(MSG_WARNING,
				   "Existing ML STA "MACSTR" on link %u is associating after "
				   "SA query timeout on a different link %u", MAC2STR(sta->addr),
				   sta->mld_assoc_link_id, link_id);
			wpa_auth_sta_deinit(sta->wpa_sm);
			sta->wpa_sm = NULL;
			sta->mld_assoc_link_id = link_id;
			for_each_mld_link(bss, hapd) {
				if (bss == hapd)
					continue;
				lsta = ap_get_sta(bss, sta->addr);
				if (lsta) {
					lsta->wpa_sm = NULL;
					lsta->mld_assoc_link_id = link_id;
				}
			}
		}
		/* Overwrite existing ml info only after SA query procedure */
		if (!assoc_wpa_sm && hostapd_is_eht_enabled(hapd)) {
			resp = hostapd_process_ml_assoc_req(hapd, elems, sta);
			if (resp != WLAN_STATUS_SUCCESS)
				return resp;
		}
#endif /* CONFIG_IEEE80211BE */

		if (!sta->wpa_sm) {
#ifdef CONFIG_IEEE80211BE
			info = &sta->mld_info;
#endif /* CONFIG_IEEE80211BE */

			/* NOTE: For links other than the assoc-link the
			 * separate wpa_sm is only allocated internally to this
			 * function.
			 */
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr,
							p2p_dev_addr);

			if (!sta->wpa_sm) {
				wpa_printf(MSG_WARNING,
					   "Failed to initialize RSN state machine");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto out;
			}
#ifdef CONFIG_SAE
			if (sta->sae && sta->sae->state == SAE_ACCEPTED &&
			    wpa_key_mgmt_sae_ext_key(sta->sae->akmp))
				wpa_auth_set_hash_alg_sae_ext_key(
					sta->wpa_sm, sta->sae->pmk_len);
#endif /* CONFIG_SAE */
		}

#ifdef CONFIG_IEEE80211BE
		if  (!assoc_wpa_sm && ap_sta_is_mld(hapd, sta)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: %s ML info in RSN Authenticator",
				   init ? "Set" : "Reset");
			sta->wpa_sm->n_mld_affiliated_links = 0;
			wpa_auth_reset_ml_link_info(sta->wpa_sm, sta->mld_assoc_link_id);
			wpa_auth_set_ml_info(sta->wpa_sm,
					     sta->mld_assoc_link_id,
					     info);
		}
#endif /* CONFIG_IEEE80211BE */

		wpa_auth_set_auth_alg(sta->wpa_sm, sta->auth_alg);
		if (sta->auth_alg == WLAN_AUTH_SAE)
			wpa_auth_set_sae_pw_id(sta->wpa_sm, sta->sae_pw_id,
					       sta->sae_pw_id_counter);
		wpa_auth_set_rsn_selection(sta->wpa_sm, elems->rsn_selection,
					   elems->rsn_selection_len);

		/*
		 * Security Profile validation for SAE and other non-802.1X
		 * authentication methods.  For 802.1X, this is done above in
		 * the CONFIG_IEEE8021X_AUTH block.  For SAE, the security
		 * profile was validated during the auth phase, but we need to
		 * set security_profile_matched here so that wpa_validate_wpa_ie()
		 * uses the profile's pairwise cipher (GCMP-256) instead of the
		 * AP's rsn_pairwise setting.
		 */
		if (hapd->conf->security_profiles && !security_profile_matched &&
		    elems->security_profile_ie) {
			u16 sp_resp = validate_security_profile_common(
				hapd, sta, elems,
				type == LINK_PARSE_REASSOC ? "Reassoc" : "Assoc",
				&matched_profile);

			if (sp_resp == WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_DEBUG,
					   "UHR: Security Profile validated for "
					   MACSTR " in (Re)Assoc",
					   MAC2STR(sta->addr));
				security_profile_matched = true;
			}
		}

		if (type == LINK_PARSE_UHR_RECONF_ASSOC || type == LINK_PARSE_UHR_RECONF_LINK)
			smd_flag = true;
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  hapd->iface->freq,
					  wpa_ie, wpa_ie_len,
					  elems->rsnxe ? elems->rsnxe - 2 :
					  NULL,
					  elems->rsnxe ? elems->rsnxe_len + 2 :
					  0,
					  elems->mdie, elems->mdie_len,
					  elems->owe_dh, elems->owe_dh_len,
					  assoc_wpa_sm,
					  ap_sta_is_mld(hapd, sta),
					  hapd->conf->external_pmk_cache,
					  security_profile_matched ?
					  &matched_profile : NULL, smd_flag);
		resp = wpa_res_to_status_code(res);
		if (resp != WLAN_STATUS_SUCCESS)
			goto out;

		if (hapd->conf->security_profiles)
			sta->wpa_sm->ap_security_profile_indication = 1;

		if (security_profile_matched)
			sta->wpa_sm->ap_security_profile_indication = 1;

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

#ifdef CONFIG_PMKSA_PRIVACY
	/* Per IEEE 802.11bi/D4.0, 12.16.7 (PMKSA caching privacy), when both
	 * the AP and non-AP STA support PMKSA caching privacy, the non-AP STA
	 * shall include a Nonce element in the (Re)Association Request frame.
	 * Skip Nonce element processing for partner AP MLD links. */
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    hapd->mld_link_id != sta->mld_assoc_link_id)
		derive_next_pmkid = false;
#endif /* CONFIG_IEEE80211BE */

	if (derive_next_pmkid && ap_sta_is_epp(sta) &&
	    hapd->conf->pmksa_caching_privacy &&
	    ieee802_11_rsnx_capab_len(elems->rsnxe, elems->rsnxe_len,
				      WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY)) {
		int akmp;
		size_t pmk_len;
		u8 *pmkid_next;

		if (!elems->nonce) {
			wpa_printf(MSG_DEBUG, "STA " MACSTR
				   " did not include Nonce element to compute next PMKID",
				   MAC2STR(sta->addr));
			goto skip_pmkid_update;
		}
		os_memcpy(sta->snonce, elems->nonce, NONCE_LEN);
		wpa_hexdump(MSG_DEBUG,
			    "RSN: Received SNonce to compute next PMKID",
			    sta->snonce, NONCE_LEN);

		switch (sta->auth_alg) {
		case WLAN_AUTH_EPPKE:
			if (!sta->pasn) {
				wpa_printf(MSG_INFO,
					   "EPPKE: Missing PASN data - cannot derive a new PMKID");
				goto skip_pmkid_update;
			}
			pmk_len = sta->pasn->pmk_len;
			pmkid_next = sta->epp_pmkid_next;
			break;
#ifdef CONFIG_IEEE8021X_AUTH
		case WLAN_AUTH_802_1X:
			pmk_len = sta->eap_auth_data.pmk_len;
			pmkid_next = sta->eap_auth_data.epp_pmkid_next;
			break;
#endif /* CONFIG_IEEE8021X_AUTH */
		default:
			wpa_printf(MSG_INFO,
				   "EPP: Unsupported auth alg %u for PMKID privacy",
				   sta->auth_alg);
			goto skip_pmkid_update;
		}

		if (random_get_bytes(sta->anonce, NONCE_LEN) < 0)
			goto skip_pmkid_update;
		wpa_hexdump_key(MSG_DEBUG,
				"EPP: Generated ANonce to compute next PMKID",
				sta->anonce, NONCE_LEN);

		akmp = wpa_auth_sta_key_mgmt(sta->wpa_sm);
		if (akmp < 0 ||
		    wpa_auth_epp_derive_new_pmkid(sta->anonce, sta->snonce,
						  akmp, pmk_len,
						  pmkid_next) < 0) {
			wpa_printf(MSG_INFO,
				   "EPP: Failed to generate new PMKID");
			goto skip_pmkid_update;
		}
		wpa_hexdump_key(MSG_DEBUG, "EPP: New PMKID",
				pmkid_next, PMKID_LEN);
	}
skip_pmkid_update:
#endif /* CONFIG_PMKSA_PRIVACY */

#ifdef CONFIG_IEEE80211R_AP
		if (sta->auth_alg == WLAN_AUTH_FT) {
			if (type != LINK_PARSE_REASSOC) {
				wpa_printf(MSG_DEBUG, "FT: " MACSTR " tried "
					   "to use association (not "
					   "re-association) with FT auth_alg",
					   MAC2STR(sta->addr));
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto out;
			}
			/* FT IEs appear only in the assoc-link frame body, not
			 * in per-STA ML sub-elements; skip re-validation for
			 * partner links whose wpa_sm is managed by assoc link.
			 */
			if (!assoc_wpa_sm) {
#ifdef CONFIG_IEEE80211BE
				resp = wpa_ft_validate_reassoc(sta->wpa_sm, ies,
							       ies_len, &sta->mld_info);
#else /* CONFIG_IEEE80211BE */
				resp = wpa_ft_validate_reassoc(sta->wpa_sm, ies,
							       ies_len, NULL);
#endif /* CONFIG_IEEE80211BE */
				if (resp != WLAN_STATUS_SUCCESS)
					goto out;
			}
		}
#endif /* CONFIG_IEEE80211R_AP */

		if (assoc_wpa_sm)
			goto skip_sae_owe;
#ifdef CONFIG_IEEE80211BE
	/*
	 * For FT, since the MIC has been validated, reset the affiliated
	 * links' auth references of previous association.
	 */
	if (info->mld_sta && sta->auth_alg == WLAN_AUTH_FT) {
		sta->wpa_sm->n_mld_affiliated_links = 0;
		wpa_auth_reset_ml_link_info(sta->wpa_sm, sta->mld_assoc_link_id);
		wpa_printf(MSG_DEBUG,
			   "MLD: Set ML info in RSN Authenticator");
		wpa_auth_set_ml_info(sta->wpa_sm,
				     sta->mld_assoc_link_id,
				     info);
	}
#endif /* CONFIG_IEEE80211BE */

		if (type == LINK_PARSE_UHR_RECONF_ASSOC || type == LINK_PARSE_UHR_RECONF_LINK)
			goto skip_sae_owe;

#ifdef CONFIG_SAE
		if (wpa_auth_uses_sae(sta->wpa_sm) && sta->sae &&
		    sta->sae->state == SAE_ACCEPTED)
			wpa_auth_add_sae_pmkid(sta->wpa_sm, sta->sae->pmkid);

		pmk_cache_based_sae = (wpa_auth_uses_sae(sta->wpa_sm) &&
				      sta->auth_alg == WLAN_AUTH_OPEN);

		if (pmk_cache_based_sae && !hapd->conf->external_pmk_cache) {
			struct rsn_pmksa_cache_entry *sa;
			sa = wpa_auth_sta_get_pmksa(sta->wpa_sm);
			if (!sa || !wpa_key_mgmt_sae(sa->akmp)) {
				wpa_printf(MSG_DEBUG,
					   "SAE: No PMKSA cache entry found for "
					   MACSTR, MAC2STR(sta->addr));
				resp = WLAN_STATUS_INVALID_PMKID;
				goto out;
			}
			wpa_printf(MSG_DEBUG, "SAE: " MACSTR
				   " using PMKSA caching", MAC2STR(sta->addr));
			sae_assign_vlan(hapd, sta, sa->sae_vlan_id);
			if (wpa_key_mgmt_sae_ext_key(sa->akmp))
				wpa_auth_set_hash_alg_sae_ext_key(
					sta->wpa_sm, sa->pmk_len);
		} else if (!pmk_cache_based_sae && !epp_sta &&
			   wpa_auth_uses_sae(sta->wpa_sm) &&
			   sta->auth_alg != WLAN_AUTH_SAE &&
			   !(sta->auth_alg == WLAN_AUTH_FT &&
			     wpa_auth_uses_ft_sae(sta->wpa_sm))) {
			wpa_printf(MSG_DEBUG, "SAE: " MACSTR " tried to use "
				   "SAE AKM after non-SAE auth_alg %u",
				   MAC2STR(sta->addr), sta->auth_alg);
			resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
			goto out;
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (hapd->conf->sae_pwe == SAE_PWE_BOTH &&
		    sta->auth_alg == WLAN_AUTH_SAE &&
		    sta->sae && !sta->sae->h2e &&
		    (hapd->conf->rsnxe_capab_mask & BIT_ULL(WLAN_RSNX_CAPAB_SAE_H2E)) &&
		    ieee802_11_rsnx_capab_len(elems->rsnxe, elems->rsnxe_len,
					      WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_INFO, "SAE: " MACSTR
				   " indicates support for SAE H2E, but did not use it",
				   MAC2STR(sta->addr));
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_SAE */

#ifdef CONFIG_OWE
		if (((hapd->conf->wpa_key_mgmt |
		      hapd->conf->rsn_override_key_mgmt |
		      hapd->conf->rsn_override_key_mgmt_2) &
		     WPA_KEY_MGMT_OWE) &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
		    elems->owe_dh) {
			resp = owe_process_assoc_req(hapd, sta, elems->owe_dh,
						     elems->owe_dh_len);
			if (resp != WLAN_STATUS_SUCCESS)
				goto out;
		}
#endif /* CONFIG_OWE */
	skip_sae_owe:

#ifdef CONFIG_DPP2
		dpp_pfs_free(sta->dpp_pfs);
		sta->dpp_pfs = NULL;

		if (DPP_VERSION > 1 &&
		    (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
		    hapd->conf->dpp_netaccesskey && sta->wpa_sm &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP &&
		    elems->owe_dh && !assoc_wpa_sm) {
			sta->dpp_pfs = dpp_pfs_init(
				wpabuf_head(hapd->conf->dpp_netaccesskey),
				wpabuf_len(hapd->conf->dpp_netaccesskey));
			if (!sta->dpp_pfs) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Could not initialize PFS");
				/* Try to continue without PFS */
				goto pfs_fail;
			}

			if (dpp_pfs_process(sta->dpp_pfs, elems->owe_dh,
					    elems->owe_dh_len) < 0) {
				dpp_pfs_free(sta->dpp_pfs);
				sta->dpp_pfs = NULL;
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto out;
			}
		}
		if (!assoc_wpa_sm)
			wpa_auth_set_dpp_z(sta->wpa_sm, sta->dpp_pfs ?
					   sta->dpp_pfs->secret : NULL);
	pfs_fail:
#endif /* CONFIG_DPP2 */

		if ((sta->flags & (WLAN_STA_HT | WLAN_STA_VHT)) &&
		    wpa_auth_get_pairwise(sta->wpa_sm) == WPA_CIPHER_TKIP) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station tried to use TKIP with HT "
				       "association");
			resp = WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
			goto out;
		}

		wpa_auth_set_ssid_protection(
			sta->wpa_sm,
			hapd->conf->ssid_protection &&
			ieee802_11_rsnx_capab_len(
				elems->rsnxe, elems->rsnxe_len,
				WLAN_RSNX_CAPAB_SSID_PROTECTION));
	} else
		wpa_auth_sta_no_wpa(sta->wpa_sm);

skip_wpa_ies:

#ifdef CONFIG_P2P
	if (ies && ies_len)
		p2p_group_notif_assoc(hapd->p2p_group, sta->addr, ies, ies_len);
#endif /* CONFIG_P2P */

#ifdef CONFIG_HS20
	wpabuf_free(sta->hs20_ie);
	if (elems->hs20 && elems->hs20_len > 4) {
		int release;

		sta->hs20_ie = wpabuf_alloc_copy(elems->hs20 + 4,
						 elems->hs20_len - 4);
		release = ((elems->hs20[4] >> 4) & 0x0f) + 1;
		if (release >= 2 && !wpa_auth_uses_mfp(sta->wpa_sm) &&
		    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
			wpa_printf(MSG_DEBUG,
				   "HS 2.0: PMF not negotiated by release %d station "
				   MACSTR, release, MAC2STR(sta->addr));
			resp = WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
			goto out;
		}
	} else {
		sta->hs20_ie = NULL;
	}

	wpabuf_free(sta->roaming_consortium);
	if (elems->roaming_cons_sel)
		sta->roaming_consortium = wpabuf_alloc_copy(
			elems->roaming_cons_sel + 4,
			elems->roaming_cons_sel_len - 4);
	else
		sta->roaming_consortium = NULL;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	wpabuf_free(sta->mb_ies);
	if (hapd->iface->fst)
		sta->mb_ies = mb_ies_by_info(&elems->mb_ies);
	else
		sta->mb_ies = NULL;
#endif /* CONFIG_FST */

#ifdef CONFIG_MBO
	mbo_ap_check_sta_assoc(hapd, sta, elems);

	if (hapd->conf->mbo_enabled && (hapd->conf->wpa & 2) &&
	    elems->mbo && sta->cell_capa && !(sta->flags & WLAN_STA_MFP) &&
	    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_INFO,
			   "MBO: Reject WPA2 association without PMF");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto out;
	}
#endif /* CONFIG_MBO */

#if defined(CONFIG_FILS) && defined(CONFIG_OCV)
	if (type != LINK_PARSE_RECONF && 
	    type != LINK_PARSE_UHR_RECONF_ASSOC &&
	    type != LINK_PARSE_UHR_RECONF_LINK &&
	    wpa_auth_uses_ocv(sta->wpa_sm) &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK)) {
		struct wpa_channel_info ci;
		int tx_chanwidth;
		int tx_seg1_idx;
		enum oci_verify_result res;

		if (hostapd_drv_channel_info(hapd, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info to validate received OCI in FILS (Re)Association Request frame");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}

		if (get_sta_tx_parameters(sta->wpa_sm,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx, &tx_chanwidth,
					  &tx_seg1_idx) < 0) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}

		res = ocv_verify_tx_params(elems->oci, elems->oci_len, &ci,
					   tx_chanwidth, tx_seg1_idx);
		if (wpa_auth_uses_ocv(sta->wpa_sm) == 2 &&
		    res == OCI_NOT_FOUND) {
			/* Work around misbehaving STAs */
			wpa_printf(MSG_INFO,
				   "FILS: Disable OCV with a STA that does not send OCI");
			wpa_auth_set_ocv(sta->wpa_sm, 0);
		} else if (res != OCI_SUCCESS) {
			wpa_printf(MSG_WARNING, "FILS: OCV failed: %s",
				   ocv_errorstr);
			wpa_msg(hapd->msg_ctx, MSG_INFO, OCV_FAILURE "addr="
				MACSTR " frame=fils-reassoc-req error=%s",
				MAC2STR(sta->addr), ocv_errorstr);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
	}
#endif /* CONFIG_FILS && CONFIG_OCV */

	ap_copy_sta_supp_op_classes(sta, elems->supp_op_classes,
				    elems->supp_op_classes_len);

	if ((sta->capability & WLAN_CAPABILITY_RADIO_MEASUREMENT) &&
	    elems->rrm_enabled &&
	    elems->rrm_enabled_len >= sizeof(sta->rrm_enabled_capa))
		os_memcpy(sta->rrm_enabled_capa, elems->rrm_enabled,
			  sizeof(sta->rrm_enabled_capa));

	if (elems->power_capab) {
		sta->min_tx_power = elems->power_capab[0];
		sta->max_tx_power = elems->power_capab[1];
		sta->power_capab = 1;
	} else {
		sta->power_capab = 0;
	}

	if (elems->bss_max_idle_period &&
	    hapd->conf->max_acceptable_idle_period) {
		u16 req;

		req = WPA_GET_LE16(elems->bss_max_idle_period);
		if (req <= hapd->conf->max_acceptable_idle_period)
			sta->max_idle_period = req;
		else if (hapd->conf->max_acceptable_idle_period >
			 hapd->conf->ap_max_inactivity)
			sta->max_idle_period =
				hapd->conf->max_acceptable_idle_period;
	}

	if (elems->wfa_capab)
		hostapd_wfa_capab(hapd, sta, elems->wfa_capab,
				  elems->wfa_capab + elems->wfa_capab_len);

	if (elems->mscs_desc && hapd->conf->mscs)
		hostapd_handle_mscs_ie_assoc(hapd, sta, elems->mscs_desc,
					     elems->mscs_desc_len);

out:
	if (resp != WLAN_STATUS_SUCCESS || assoc_wpa_sm) {
		wpa_auth_sta_deinit(sta->wpa_sm);

		/* Only keep a reference to the main wpa_sm and drop the
		 * per-link instance.
		 * This reference is needed during group rekey handling.
		 */
		if (resp == WLAN_STATUS_SUCCESS) {
			sta->wpa_sm = assoc_wpa_sm;
#ifdef CONFIG_IEEE80211BE
			if (hapd->mld_link_id == sta->mld_assoc_link_id)
				set_wpa_sm_for_each_partner_link(hapd, sta, assoc_wpa_sm);
#endif /* CONFIG_IEEE80211BE */
		} else {
			sta->wpa_sm = NULL;
#ifdef CONFIG_IEEE80211BE
			if (hapd->mld_link_id == sta->mld_assoc_link_id)
				clear_wpa_sm_for_each_partner_link(hapd, sta);
#endif /* CONFIG_IEEE80211BE */
		}
	}

	return resp;
}


int check_assoc_ies(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *ies, size_t ies_len,
			   enum link_parse_type type)
{
	struct ieee802_11_elems elems;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Station sent an invalid association request");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return __check_assoc_ies(hapd, sta, ies, ies_len, &elems, type, NULL);
}


#ifdef CONFIG_IEEE80211BE

void ieee80211_ml_build_assoc_resp(struct hostapd_data *hapd,
				   struct hostapd_data *phapd,
				   struct sta_info *sta,
				   struct mld_link_info *link)
{
	struct hostapd_data *assoc_hapd;
	struct sta_info *assoc_sta;
	u8 buf[EHT_ML_MAX_STA_PROF_LEN];
	u8 *p = buf;
	size_t buflen = sizeof(buf);
#ifdef CONFIG_IEEE80211R_AP
	u8 assoc_rsne[128];
	u8 link_rsne[128];
	size_t assoc_rsn_len, link_rsn_len;
#endif

	/* Capability Info */
	WPA_PUT_LE16(p, hostapd_own_capab_info(hapd));
	p += 2;

	/* Status Code */
	WPA_PUT_LE16(p, link->status);
	p += 2;

	if (link->status != WLAN_STATUS_SUCCESS)
		goto out;

	/* AID is not included */
	p = hostapd_eid_supp_rates(hapd, p);
	p = hostapd_eid_ext_supp_rates(hapd, p);
	p = hostapd_eid_rm_enabled_capab(hapd, p, buf + buflen - p);
	p = hostapd_eid_ht_capabilities(hapd, p);
	p = hostapd_eid_ht_operation(hapd, p);

#ifdef CONFIG_IEEE80211R_AP
	if (phapd && hapd && (sta->auth_alg == WLAN_AUTH_FT)) {
		assoc_rsn_len = wpa_write_rsn_ie(&phapd->wpa_auth->conf,
						 assoc_rsne, sizeof(assoc_rsne),
						 sta->wpa_sm->pmk_r1_name);
		link_rsn_len = wpa_write_rsn_ie(&hapd->wpa_auth->conf, link_rsne,
						sizeof(link_rsne),
						sta->wpa_sm->pmk_r1_name);
		if ((assoc_rsn_len != link_rsn_len) ||
		    (os_memcmp(assoc_rsne, link_rsne, assoc_rsn_len) != 0)) {
			os_memcpy(p, link_rsne, link_rsn_len);
			p += link_rsn_len;
		}
	}
#endif

	if (hostapd_is_vht_enabled(hapd)) {
		p = hostapd_eid_vht_capabilities(hapd, p, 0);
		p = hostapd_eid_vht_operation(hapd, p);
	}

	if (hostapd_is_he_enabled(hapd)) {
		p = hostapd_eid_he_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_he_operation(hapd, p);
		p = hostapd_eid_spatial_reuse(hapd, p);
		p = hostapd_eid_he_mu_edca_parameter_set(hapd, p, false);
		p = hostapd_eid_he_6ghz_band_cap(hapd, p);
		if (hostapd_is_eht_enabled(hapd)) {
			p = hostapd_eid_eht_capab(hapd, p, IEEE80211_MODE_AP);
			p = hostapd_eid_eht_operation(hapd, p);
		}
#ifdef CONFIG_IEEE80211BN
		if (hostapd_is_uhr_enabled(hapd)) {
			p = hostapd_eid_uhr_capab(hapd, p, IEEE80211_MODE_AP);
			p = hostapd_eid_uhr_operation(hapd, p, false);
			p = hostapd_eid_uhr_params_update(hapd, p, true, false);
		}
#endif /* CONFIG_IEEE80211BN */
	}

	p = hostapd_eid_ext_capab(hapd, p, false);

	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (assoc_sta && hapd != assoc_hapd) {
		const u8 *rsnxe, *assoc_rsnxe;
#ifdef CONFIG_ENC_ASSOC
		const u8 *rsne, *assoc_rsne;
#endif /* CONFIG_ENC_ASSOC */
		u8 ie_count = 0, non_inherit_ie[2];

		rsnxe = hostapd_wpa_ie(hapd, WLAN_EID_RSNX);
		assoc_rsnxe = hostapd_wpa_ie(assoc_hapd, WLAN_EID_RSNX);
#ifdef CONFIG_ENC_ASSOC
		rsne = hostapd_wpa_ie(hapd, WLAN_EID_RSN);
		assoc_rsne = hostapd_wpa_ie(assoc_hapd, WLAN_EID_RSN);
#endif /* CONFIG_ENC_ASSOC */

		if (assoc_hapd->conf->rsn_override_omit_rsnxe)
			goto omit_rsnxe;

		if (assoc_rsnxe && !rsnxe)
			non_inherit_ie[ie_count++] = WLAN_EID_RSNX;

		if ((!assoc_rsnxe && rsnxe) ||
		    (rsnxe && assoc_rsnxe &&
		     (rsnxe[1] != assoc_rsnxe[1] ||
		      os_memcmp(rsnxe, assoc_rsnxe, 2 + rsnxe[1]) != 0))) {
			size_t rsnx_ie_len;

			rsnx_ie_len = 2 + rsnxe[1];
			if ((size_t) (buf + buflen - p) >= rsnx_ie_len) {
				os_memcpy(p, rsnxe, rsnx_ie_len);
				p += rsnx_ie_len;
			}
		}
omit_rsnxe:
#ifdef CONFIG_ENC_ASSOC
		if (ap_sta_is_epp(sta) &&
		    wpa_auth_ap_sta_support_assoc_enc(sta->wpa_sm)) {
			if ((rsne && assoc_rsne &&
			     (rsne[1] != assoc_rsne[1] ||
			      os_memcmp(rsne, assoc_rsne, 2 + rsne[1]) != 0))) {
				size_t rsn_ie_len;

				rsn_ie_len = 2 + rsne[1];
				if ((size_t) (buf + buflen - p) >= rsn_ie_len) {
					os_memcpy(p, rsne, rsn_ie_len);
					p += rsn_ie_len;
				}
			}
		}
#endif /* CONFIG_ENC_ASSOC */
		if (ie_count) {
			*p++ = WLAN_EID_EXTENSION;
			*p++ = 2 + ie_count + 1;
			*p++ = WLAN_EID_EXT_NON_INHERITANCE;
			*p++ = ie_count;
			os_memcpy(p, non_inherit_ie, ie_count);
			p += ie_count;
			*p++ = 0; /* No Element ID Extension List */
		}
	}

#ifdef CONFIG_IEEE80211BE
	p = hostapd_eid_mcst(hapd, p, buf + buflen - p);
#endif /* CONFIG_IEEE80211BE */
	p = hostapd_eid_mbo(hapd, p, buf + buflen - p);
	p = hostapd_eid_wmm(hapd, p, false);
#ifdef CONFIG_QCN_EXTN
	p = hostapd_eid_qcn_vendor_ie_extn(hapd, p, IEEE80211_MODE_AP);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	if (hapd->conf->smd.enabled) {
		struct sta_info *sta = ap_get_sta(hapd, link->peer_addr);
		if (sta && sta->smd_info.smd_sta)
			p = hostapd_eid_smd_ie_response(hapd, sta, p);
	}
#endif

	if (hapd->conf->assocresp_elements &&
	    (size_t) (buf + buflen - p) >=
	    wpabuf_len(hapd->conf->assocresp_elements)) {
		os_memcpy(p, wpabuf_head(hapd->conf->assocresp_elements),
			  wpabuf_len(hapd->conf->assocresp_elements));
		p += wpabuf_len(hapd->conf->assocresp_elements);
	}

out:
	os_free(link->resp_sta_profile);
	link->resp_sta_profile = os_memdup(buf, p - buf);
	link->resp_sta_profile_len = link->resp_sta_profile ? p - buf : 0;
}


int ieee80211_ml_process_link(struct hostapd_data *hapd,
			      struct hostapd_data *phapd,
			      struct sta_info *origin_sta,
			      struct mld_link_info *link,
			      const u8 *ies, size_t ies_len,
			      enum link_parse_type type, bool offload,
			      bool *set_beacon)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf = NULL;
	struct sta_info *sta = NULL;
	u16 status = WLAN_STATUS_SUCCESS;
	int i;

	wpa_printf(MSG_DEBUG, "MLD: link: link_id=%u, peer=" MACSTR,
		   hapd->mld_link_id, MAC2STR(link->peer_addr));

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLD: link: Element parsing failed");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto out;
	}

	sta = ap_get_sta(hapd, origin_sta->addr);
	if (sta || TEST_FAIL()) {
		wpa_printf(MSG_INFO, "MLD: link: Station already exists");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		sta = NULL;
		goto out;
	}

	sta = ap_sta_add(hapd, origin_sta->addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "MLD: link: ap_sta_add() failed");
		status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto out;
	}

	if (type != LINK_PARSE_RECONF  && type != LINK_PARSE_UHR_RECONF_LINK) {
		mlbuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len,
					  true);
		if (!mlbuf)
			goto out;

		if (ieee802_11_parse_link_assoc_req(&elems, mlbuf,
						    hapd->mld_link_id, true) ==
		    ParseFailed) {
			wpa_printf(MSG_DEBUG,
				   "MLD: link: Failed to parse association request Multi-Link element");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
	}

	sta->flags = origin_sta->flags & WLAN_STA_AUTH;

	if (type == LINK_PARSE_RECONF || type == LINK_PARSE_UHR_RECONF_LINK)
		sta->flags |= (origin_sta->flags & (WLAN_STA_ASSOC | WLAN_STA_MFP));

	sta->mld_assoc_link_id = origin_sta->mld_assoc_link_id;
	sta->sa_query_timed_out = origin_sta->sa_query_timed_out;
	ap_sta_set_mld(sta, true);
	sta->auth_alg = origin_sta->auth_alg;
#ifdef CONFIG_ENC_ASSOC
	sta->epp_sta = origin_sta->epp_sta;
#endif /* CONFIG_ENC_ASSOC */

	sta->capability = elems.per_link_sta_capability;

	status = __check_assoc_ies(hapd, sta, NULL, 0, &elems, type,
				   origin_sta->wpa_sm);
	if (status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "MLD: link: Element check failed");
		goto out;
	}

	os_memcpy(&sta->mld_info, &origin_sta->mld_info, sizeof(sta->mld_info));
	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		struct mld_link_info *li = &sta->mld_info.links[i];

		li->resp_sta_profile = NULL;
		li->resp_sta_profile_len = 0;

		if ((type == LINK_PARSE_RECONF || type == LINK_PARSE_UHR_RECONF_LINK) && i == hapd->mld_link_id) {
			os_memcpy(li->local_addr, hapd->own_addr, ETH_ALEN);
			os_memcpy(li->peer_addr, link->peer_addr, ETH_ALEN);
		}
	}

#ifdef CONFIG_IEEE80211BN
	if (type == LINK_PARSE_UHR_RECONF_LINK)
		sta->smd_info.roam_sta = origin_sta->smd_info.roam_sta;
#endif /* CONFIG_IEEE80211BN */

	if (!offload) {
		/*
		 * Get the AID from the station on which the association was
		 * performed, and mark it as used.
		 */
		sta->aid = origin_sta->aid;
		if (sta->aid == 0) {
			wpa_printf(MSG_DEBUG, "MLD: link: No AID assigned");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
		hapd->sta_aid[sta->aid / 32] |= BIT(sta->aid % 32);
		sta->listen_interval = origin_sta->listen_interval;
		if (update_ht_state(hapd, sta) > 0 && set_beacon)
			*set_beacon = true;
	}

	/*
	 * Do not initialize the EAPOL state machine.
	 * TODO: Maybe it is needed?
	 */
	sta->eapol_sm = NULL;

	wpa_printf(MSG_DEBUG, "MLD: link=%u, association OK (aid=%u)",
		   hapd->mld_link_id, sta->aid);
	sta->flags |= WLAN_STA_ASSOC_REQ_OK;
	sta->vlan_id = origin_sta->vlan_id;
	sta->auth_alg = origin_sta->auth_alg;

	/* TODO: What other processing is required? */

	if (!offload &&
	    add_associated_sta(hapd, sta, type))
		status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
out:
	wpabuf_free(mlbuf);
	link->status = status;

	if (!offload && (type == LINK_PARSE_ASSOC || type == LINK_PARSE_REASSOC))
		ieee80211_ml_build_assoc_resp(hapd, phapd, sta, link);

	wpa_printf(MSG_DEBUG, "MLD: link: status=%u", status);
	if (status != WLAN_STATUS_SUCCESS) {
		/*
		 * Per-link failure (capability mismatch, capacity, policy).
		 * Free the partially-initialised partner STA, mark the link as
		 * rejected in mld_link_info so the caller can continue the
		 * association on the remaining links, and return -1 so the
		 * caller knows this specific link was not accepted.
		 */
		link->rejected = true;
		wpa_release_link_auth_ref(origin_sta->wpa_sm, hapd->mld_link_id, true);
		if (sta)
			ap_free_sta(hapd, sta);
		return -1;
	}
	/* if link sta removed and re-added again in reassoc,
	 * link valid flag set to false during link removal in
	 * ml_info in sta's sm. if links added successfully set
	 * link valid true again in sta's wpa_sm for all valid links.
	 */
	wpa_auth_set_ml_info_link(origin_sta->wpa_sm,  &origin_sta->mld_info, hapd->mld_link_id);
	wpa_printf(MSG_DEBUG, "MLD: States set");
	return 0;
}


bool hostapd_is_multiple_link_mld(struct hostapd_data *hapd)
{
	struct hostapd_data *bss;

	if (!hapd->conf->mld_ap)
		return false;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return false;
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->iface || !hapd->iface->interfaces ||
	    hapd->iface->interfaces->count <= 1)
		return false;

	/*
	 * Checking for interfaces count above is not sufficient as there
	 * could be non-MLD interfaces or MLD interface that are not affiliated
	 * with the same MLD as the currently processing one. So need to check
	 * if other partner links exist for this the same AP MLD.
	 */
	for_each_mld_link(bss, hapd) {
		if (bss != hapd)
			return true;
	}

	return false;
}

#endif /* CONFIG_IEEE80211BE */


/*
 * hostapd_mlo_link_rej_prio - Return band rejection priority for a link.
 *
 * Two rejection modes are supported:
 *
 * Mode 0 — default band-priority:
 *   6G (3) > 5G (2) > 2G (1)
 *   When the mld_max_links_per_sta cap is hit, 2G is rejected first.
 *
 * Mode 1 — range mode (mirrors qca-wifi MLO_LINK_REJ_RANGE):
 *   6G (3) > 2G (2) > 5G (1)
 *   2G is never rejected; 5G is rejected first when the cap is hit.
 *
 * Higher priority = kept longer; lowest priority = rejected first.
 * Returns 2 (5G default) if the link BSS frequency cannot be determined.
 */
static u8 hostapd_mlo_link_rej_prio(struct hostapd_data *hapd,
				     struct hostapd_data *link_bss)
{
	u8 mode = hapd->conf->mld_link_rej_mode;
	int freq;
	u8 prio;

	if (!link_bss || !link_bss->iface)
		return 2;

	freq = link_bss->iface->freq;

	if (mode == 1) {
		/* Range mode: never reject 2G, reject 5G first */
		if (IS_2P4GHZ(freq))
			prio = 2;
		else if (is_6ghz_freq(freq))
			prio = 3;
		else
			prio = 1; /* 5G: lowest priority, rejected first */
		return prio;
	}

	/* Default mode: reject 2G first */
	if (IS_2P4GHZ(freq))
		prio = 1;
	else if (is_6ghz_freq(freq))
		prio = 3;
	else
		prio = 2;
	return prio;
}


int hostapd_process_assoc_ml_info(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  const u8 *ies, size_t ies_len,
				  bool reassoc, int tx_link_status,
				  bool offload,
				  bool *set_beacon)
{
	int ret = 0;
#ifdef CONFIG_IEEE80211BE
	unsigned int i;
	const u8 *mld_link_addr = NULL;
	bool mld_link_sta = false;
	u16 eml_cap = 0;
	unsigned int accepted_links = 0;

	if (!hapd->conf->mld_ap)
		return 0;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return 0;
#endif /* CONFIG_QCN_EXTN */

	if (tx_link_status == WLAN_STATUS_SUCCESS && sta->mld_info.mld_sta) {
		u8 mld_link_id = hapd->mld_link_id;
		bool epp_sta = false;

#ifdef CONFIG_ENC_ASSOC
	epp_sta = sta->epp_sta;
#endif /* CONFIG_ENC_ASSOC */
		mld_link_sta = sta->mld_assoc_link_id != mld_link_id;
		mld_link_addr = sta->mld_info.links[mld_link_id].peer_addr;
		eml_cap = sta->mld_info.common_info.eml_capa;
		wpa_printf(MSG_DEBUG, "Add associated ML STA " MACSTR
			   " (added_unassoc=%d auth_alg=%u ft_over_ds=%u reassoc=%d authorized=%d ft_tk=%d fils_tk=%d)",
			   MAC2STR(sta->addr), sta->added_unassoc, sta->auth_alg,
			   sta->ft_over_ds, reassoc,
			   !!(sta->flags & WLAN_STA_AUTHORIZED),
			   wpa_auth_sta_ft_tk_already_set(sta->wpa_sm),
			   wpa_auth_sta_fils_tk_already_set(sta->wpa_sm));

		if (!sta->added_unassoc && (!(sta->flags & WLAN_STA_AUTHORIZED) ||
		    (reassoc && sta->ft_over_ds && sta->auth_alg == WLAN_AUTH_FT) ||
		    (!wpa_auth_sta_ft_tk_already_set(sta->wpa_sm) &&
		     !wpa_auth_sta_fils_tk_already_set(sta->wpa_sm)))) {
			wpa_printf(MSG_DEBUG,
				   "ML STA was already created and we received assoc resp again (reassoc: %d)",
				   reassoc);
			/* cleanup all link sta in kernel and add later on ml processing */
			hostapd_drv_sta_remove(hapd, sta->addr);
			ap_sta_remove_link_sta(hapd, sta, 0, true);
			sta->flags &= ~(WLAN_STA_ASSOC | WLAN_STA_AUTHORIZED);
			sta->unadded_sta = false;

			if (hostapd_sta_add(hapd, sta->addr, 0, 0,
					    sta->supported_rates,
					    sta->supported_rates_len,
					    0, NULL, NULL, NULL, 0, NULL, 0,
				 	    NULL, 0,
#ifdef CONFIG_QCN_EXTN
					    NULL,
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BN
					    sta->smd_info.smd_sta,
					    sta->smd_info.caps.dl_data_fwd,
					    sta->smd_info.smd_identifier,
					    false,
#else
					    false, false, NULL, false,
#endif /* CONFIG_IEEE80211BN */
					    NULL, sta->flags, 0, 0, 0, 0,
					    mld_link_addr, mld_link_sta,
					    eml_cap, reassoc, CONTROL_MIC_PAD_NOT_SET,
					    epp_sta)) {
				hostapd_logger(hapd, sta->addr,HOSTAPD_MODULE_IEEE80211,HOSTAPD_LEVEL_NOTICE,
					       "Could not add STA to kernel driver");
				return -1;
			}
		}

		accepted_links = 1;
	}

	/*
	 * Reject the whole association if the STA requests fewer links than
	 * the configured minimum. The TX link counts as 1, so total requested
	 * links = partner_link_count + 1.
	 */
	if (hapd->conf->mld_min_links_per_sta &&
	    tx_link_status == WLAN_STATUS_SUCCESS) {
		unsigned int partner_link_count = 0;
		int pi;

		for (pi = 0; pi < MAX_NUM_MLD_LINKS; pi++) {
			if (sta->mld_info.links[pi].valid &&
			    pi != sta->mld_assoc_link_id)
				partner_link_count++;
		}
		if ((partner_link_count + 1) < hapd->conf->mld_min_links_per_sta)
			return -1;
	}

	/*
	 * Process each partner link independently.  A failure on one link
	 * must not abort the association on the remaining links (IEEE
	 * 802.11be-2024 §35.3.6).  For every rejected link we:
	 *   1. Set the per-link Status Code in mld_link_info.
	 *   2. Mark mld_link_info.rejected so downstream code (assoc-cb,
	 *      4-way HS KDE processing) can skip it cleanly.
	 *   3. Build the per-STA profile response with the error status.
	 *   4. Release the RSN authenticator reference for that link.
	 * Only if the TX (assoc) link itself fails do we propagate -1 to
	 * the caller and abort the whole association.
	 */
	{
		u8 sorted_links[MAX_NUM_MLD_LINKS];
		u8 sorted_prio[MAX_NUM_MLD_LINKS];
		unsigned int n_sorted = 0, si;
		struct hostapd_data *bss = NULL;
		struct mld_link_info *link = NULL;
		bool link_bss_found = false;

		for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
			struct mld_link_info *slink = &sta->mld_info.links[i];
			struct hostapd_data *bss2 = NULL;
			u8 prio = 2; /* default: 5G */

			if (!slink->valid || i == sta->mld_assoc_link_id)
				continue;

			for_each_mld_link(bss2, hapd) {
				if (bss2 == hapd || bss2->mld_link_id != i ||
				    !bss2->started)
					continue;
				break;
			}

			if (bss2 && bss2->mld_link_id == i && bss2->started)
				prio = hostapd_mlo_link_rej_prio(hapd, bss2);

			/* insertion sort by descending priority (highest first) */
			si = n_sorted;
			while (si > 0 && sorted_prio[si - 1] < prio) {
				sorted_links[si] = sorted_links[si - 1];
				sorted_prio[si] = sorted_prio[si - 1];
				si--;
			}
			sorted_links[si] = i;
			sorted_prio[si] = prio;
			n_sorted++;
		}

	for (si = 0; si < n_sorted; si++) {
		i = sorted_links[si];
		link = &sta->mld_info.links[i];
		bss = NULL;
		link_bss_found = false;

		/* Clear stale rejected flag from a previous association attempt
		 * so a retry starts with a clean state. */
		if (link->rejected) {
			link->rejected = false;
			if (sta->wpa_sm)
				wpa_auth_set_ml_link_rejected(sta->wpa_sm, i, false);
		}

		if (!link->valid || i == sta->mld_assoc_link_id)
			continue;

		for_each_mld_link(bss, hapd) {
			if (bss == hapd)
				continue;
			if (bss->mld_link_id != i)
				continue;
			if (!bss->started || !bss->beacon_set_done)
				continue;
			link_bss_found = true;
			break;
		}

		if (!link_bss_found || TEST_FAIL()) {
			/* Requested link ID not present in this AP MLD */
			wpa_printf(MSG_DEBUG,
				   "MLD: No BSS for link_id=%u, rejecting", i);
			link->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			link->rejected = true;
			if (!offload)
				ieee80211_ml_build_assoc_resp(hapd, NULL, sta, link);
		} else if (tx_link_status != WLAN_STATUS_SUCCESS) {
			/*
			 * TX link was rejected; per spec all partner links must
			 * also be rejected with DENIED_TX_LINK_NOT_ACCEPTED.
			 */
			link->status = WLAN_STATUS_DENIED_TX_LINK_NOT_ACCEPTED;
			link->rejected = true;
			if (!offload)
				ieee80211_ml_build_assoc_resp(hapd, NULL, sta, link);
		} else {
			/*
			 * Operator policy: cap the number of links per STA.
			 * The TX link always counts as one accepted link, so
			 * accepted_links is initialised to 1 above.
			 */
			if (hapd->conf->mld_max_links_per_sta &&
			    accepted_links >= hapd->conf->mld_max_links_per_sta) {
				wpa_printf(MSG_DEBUG,
					   "MLD: link_id=%u rejected: max links (%u) reached",
					   i, hapd->conf->mld_max_links_per_sta);
				link->status = WLAN_STATUS_DENIED_TX_LINK_NOT_ACCEPTED;
				link->rejected = true;
				if (!offload)
					ieee80211_ml_build_assoc_resp(hapd, NULL, sta, link);
				goto release_ref;
			}

			/*
			 * Capacity check: reject only this link if the partner
			 * BSS is full; the association on other links continues.
			 */
			if (hostapd_check_max_sta(bss)) {
				wpa_printf(MSG_DEBUG,
					   "MLD: link_id=%u rejected: BSS at capacity",
					   i);
				link->status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
				link->rejected = true;
				if (!offload)
					ieee80211_ml_build_assoc_resp(hapd, NULL, sta, link);
				goto release_ref;
			}

			/*
			 * Attempt to set up the partner link.  On failure the
			 * link is already marked rejected inside
			 * ieee802_11_ml_process_link() and -1 is returned.
			 * Treat this as a soft per-link rejection: continue
			 * processing remaining links and do NOT propagate the
			 * error to the caller (the TX-link association stands).
			 */
			if (ieee80211_ml_process_link(bss, hapd, sta, link,
						      ies, ies_len,
						      reassoc ? LINK_PARSE_REASSOC :
						      LINK_PARSE_ASSOC,
						      offload, set_beacon)) {
				/* link->rejected already set by callee */
				wpa_printf(MSG_DEBUG,
					   "MLD: link_id=%u process failed, rejected",
					   i);
			} else {
				accepted_links++;
			}
		}

release_ref:
		/*
		 * Sync the rejected flag into the RSN state machine so the
		 * 4-way handshake KDE processing can skip rejected links.
		 * Must be called before wpa_release_link_auth_ref() because
		 * that call clears link->wpa_auth.
		 */
		if (sta->wpa_sm && link->rejected)
			wpa_auth_set_ml_link_rejected(sta->wpa_sm, i, true);

		if (link->rejected)
			wpa_release_link_auth_ref(sta->wpa_sm, i, true);
	}
	} /* end band-priority block */
#endif /* CONFIG_IEEE80211BE */

	return ret;
}


static void send_deauth(struct hostapd_data *hapd, const u8 *addr,
			u16 reason_code)
{
	int send_len;
	struct ieee80211_mgmt reply;

	os_memset(&reply, 0, sizeof(reply));
	reply.frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH);
	os_memcpy(reply.da, addr, ETH_ALEN);
	os_memcpy(reply.sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply.bssid, hapd->own_addr, ETH_ALEN);

	send_len = IEEE80211_HDRLEN + sizeof(reply.u.deauth);
	reply.u.deauth.reason_code = host_to_le16(reason_code);

	if (hostapd_drv_send_mlme(hapd, &reply, send_len, 0, NULL, 0, 0, 0, 0) < 0)
		wpa_printf(MSG_INFO, "Failed to send deauth: %s",
			   strerror(errno));
}


int add_associated_sta(struct hostapd_data *hapd,
			      struct sta_info *sta, int type)
{
	struct ieee80211_ht_capabilities ht_cap;
	struct ieee80211_vht_capabilities vht_cap;
	struct ieee80211_he_capabilities he_cap;
	struct ieee80211_eht_capabilities eht_cap;
	struct ieee80211_uhr_capabilities uhr_cap;
	int set = 1;
	const u8 *mld_link_addr = NULL;
	bool mld_link_sta = false, epp_sta = false;
	u16 eml_cap = 0;
	bool reassoc = (type == LINK_PARSE_REASSOC);

#ifdef CONFIG_ENC_ASSOC
	epp_sta = sta->epp_sta;
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_IEEE80211BE
	wpa_printf(MSG_DEBUG, "Associated link: %d, current link: %d, %d, %p, %d", sta->mld_assoc_link_id, hapd->mld_link_id, hapd->conf->mld_ap, sta, sta->mld_info.mld_sta);
	if (ap_sta_is_mld(hapd, sta)) {
		
		u8 mld_link_id = hapd->mld_link_id;

		mld_link_sta = (sta->mld_assoc_link_id != mld_link_id);
		mld_link_addr = sta->mld_info.links[mld_link_id].peer_addr;

		if (hapd->mld_link_id != sta->mld_assoc_link_id)
			set = 0;
		eml_cap = sta->mld_info.common_info.eml_capa;
	}

	if (sta->unadded_sta) {
		ap_sta_remove_link_sta(hapd, sta, 0, false);
		sta->unadded_sta = false;
	}
#endif /* CONFIG_IEEE80211BE */
	/*
	 * Remove the STA entry to ensure the STA PS state gets cleared and
	 * configuration gets updated. This is relevant for cases, such as
	 * FT-over-the-DS, where a station re-associates back to the same AP but
	 * skips the authentication flow, or if working with a driver that
	 * does not support full AP client state.
	 *
	 * Skip this if the STA has already completed FT reassociation and the
	 * TK has been configured since the TX/RX PN must not be reset to 0 for
	 * the same key.
	 *
	 * FT-over-the-DS has a special case where the STA entry (and as such,
	 * the TK) has not yet been configured to the driver depending on which
	 * driver interface is used. For that case, allow add-STA operation to
	 * be used (instead of set-STA). This is needed to allow mac80211-based
	 * drivers to accept the STA parameter configuration. Since this is
	 * after a new FT-over-DS exchange, a new TK has been derived, so key
	 * reinstallation is not a concern for this case.
	 *
	 * If the STA was associated and authorized earlier, but came for a new
	 * connection (!added_unassoc + !reassoc), remove the existing STA entry
	 * so that it can be re-added. This case is rarely seen when the AP could
	 * not receive the deauth/disassoc frame from the STA. And the STA comes
	 * back with new connection within a short period or before the inactive
	 * STA entry is removed from the list.
	 */
	wpa_printf(MSG_DEBUG, "Add associated STA " MACSTR
		   " (added_unassoc=%d auth_alg=%u ft_over_ds=%u reassoc=%d authorized=%d ft_tk=%d fils_tk=%d, type=%d)",
		   MAC2STR(sta->addr), sta->added_unassoc, sta->auth_alg,
		   sta->ft_over_ds, reassoc,
		   !!(sta->flags & WLAN_STA_AUTHORIZED),
		   wpa_auth_sta_ft_tk_already_set(sta->wpa_sm),
		   wpa_auth_sta_fils_tk_already_set(sta->wpa_sm), type);

	if (!ap_sta_is_mld(hapd, sta) && !sta->added_unassoc &&
	    (!(sta->flags & WLAN_STA_AUTHORIZED) ||
	     (reassoc && sta->ft_over_ds && sta->auth_alg == WLAN_AUTH_FT) ||
	     (!wpa_auth_sta_ft_tk_already_set(sta->wpa_sm) &&
	      !wpa_auth_sta_fils_tk_already_set(sta->wpa_sm)) ||
	     (!reassoc && (sta->flags & WLAN_STA_AUTHORIZED)))) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		wpa_auth_sm_event(sta->wpa_sm, WPA_DRV_STA_REMOVED);
		set = 0;
		sta->skip_kernel_delete = false;

		 /* Do not allow the FT-over-DS exception to be used more than
		  * once per authentication exchange to guarantee a new TK is
		  * used here */
		sta->ft_over_ds = 0;
	}

	if (sta->flags & WLAN_STA_HT)
		hostapd_get_ht_capab(hapd, sta->ht_capabilities, &ht_cap);
#ifdef CONFIG_IEEE80211AC
	if (sta->flags & WLAN_STA_VHT)
		hostapd_get_vht_capab(hapd, sta->vht_capabilities, &vht_cap);
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	if (sta->flags & WLAN_STA_HE) {
		hostapd_get_he_capab(hapd, sta->he_capab, &he_cap,
				     sta->he_capab_len);
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	if (sta->flags & WLAN_STA_EHT) {
		hostapd_get_eht_capab(hapd, sta->eht_capab, &eht_cap,
				      sta->eht_capab_len);
		hostapd_get_epcs_capab(hapd, sta);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (sta->flags & WLAN_STA_UHR)
		hostapd_get_uhr_capab(sta->uhr_capab, &uhr_cap,
				      sta->uhr_capab_len);
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_QCN_EXTN
	/* Notify the driver of the negotiated non-standard VHT MCS10/11
	 * capability and HE cap BEFORE hostapd_sta_add() so the driver has
	 * the value when mac80211 triggers the AUTH->ASSOC sta_state
	 * transition and WMI peer assoc is sent. This cannot go with
	 * hostapd_sta_add().
	 */
	hostapd_set_sta_vht_mcs10_11_and_he_cap_internal_extn(hapd, sta);
#endif /* CONFIG_QCN_EXTN */

	/*
	 * Add the station with forced WLAN_STA_ASSOC flag. The sta->flags
	 * will be set when the ACK frame for the (Re)Association Response frame
	 * is processed (TX status driver event).
	 */
	if (hostapd_sta_add(hapd, sta->addr, sta->aid, sta->capability,
			    sta->supported_rates, sta->supported_rates_len,
			    sta->listen_interval,
			    sta->flags & WLAN_STA_HT ? &ht_cap : NULL,
			    sta->flags & WLAN_STA_VHT ? &vht_cap : NULL,
			    sta->flags & WLAN_STA_HE ? &he_cap : NULL,
			    sta->flags & WLAN_STA_HE ? sta->he_capab_len : 0,
			    sta->flags & WLAN_STA_EHT ? &eht_cap : NULL,
			    sta->flags & WLAN_STA_EHT ? sta->eht_capab_len : 0,
			    sta->flags & WLAN_STA_UHR ? &uhr_cap : NULL,
			    sta->flags & WLAN_STA_UHR ? sta->uhr_capab_len : 0,

#ifdef CONFIG_QCN_EXTN
			    (struct sta_info_extn *)&sta->sta_extn,
#endif
#ifdef CONFIG_IEEE80211BN
                           sta->smd_info.smd_sta, sta->smd_info.caps.dl_data_fwd, sta->smd_info.smd_identifier,
                           sta->smd_info.roam_sta,
#else
                           false, false, NULL, false,
#endif /*CONFIG_IEEE80211 */
			    sta->he_6ghz_capab,
			    sta->flags | WLAN_STA_ASSOC, sta->qosinfo,
			    sta->vht_opmode, sta->p2p_ie ? 1 : 0,
			    set, mld_link_addr, mld_link_sta, eml_cap,
			    type, sta->control_mic_pad, epp_sta)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211, HOSTAPD_LEVEL_NOTICE,
			       "Could not %s STA to kernel driver",
			       set ? "set" : "add");

		if (!ap_sta_is_mld(hapd, sta) && sta->added_unassoc) {
			hostapd_drv_sta_remove(hapd, sta->addr);
			sta->added_unassoc = 0;
		}

		return -1;
	}

	sta->added_unassoc = 0;

	return 0;
}

#ifdef RDK_ONEWIFI
u16 send_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta, const u8 *addr, u16 status_code, int reassoc,
                           const u8 *ies, size_t ies_len, int rssi,
                           int omit_rsnxe)
#else
static u16 send_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *addr, u16 status_code, int reassoc,
			   const u8 *ies, size_t ies_len, int rssi,
			   int omit_rsnxe)
#endif
{
	int send_len;
	u8 *buf;
	size_t buflen;
	struct ieee80211_mgmt *reply;
	u8 *p;
	u16 res = WLAN_STATUS_SUCCESS;

	buflen = sizeof(struct ieee80211_mgmt) + ASSOC_RESP_BUF_SLACK;
#ifdef CONFIG_FILS
	if (sta && sta->fils_hlp_resp)
		buflen += wpabuf_len(sta->fils_hlp_resp);
	if (sta)
		buflen += 150;
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if (sta && ((hapd->conf->wpa_key_mgmt | hostapd_sp_implied_key_mgmt(hapd->conf)) & WPA_KEY_MGMT_OWE))
		buflen += 150;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP2
	if (sta && sta->dpp_pfs)
		buflen += 5 + sta->dpp_pfs->curve->prime_len;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		buflen += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		buflen += 3 + sizeof(struct ieee80211_eht_operation);
		if (hapd->chan_usage_config.num_elems > 0) {
			/* Add Country element if Channel Usage is added */
			buflen += hostapd_eid_country_len(hapd);
			buflen += hostapd_eid_channel_usage_len(hapd);
		}
		if (hapd->iconf->punct_bitmap)
			buflen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		buflen += 3 + sizeof(struct ieee80211_uhr_capabilities);
		buflen += 3 + sizeof(struct ieee80211_uhr_operation);
		if (hapd->iconf->npca_enable) {
			buflen += IEEE80211_UHR_NPCA_OPER_BASE_SIZE;
			if (hapd->iconf->npca_punct_bitmap)
				buflen += IEEE80211_UHR_NPCA_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;
		}
		buflen += hostapd_eid_uhr_params_update_len(hapd, true, false);
	}
	/* Add SMD IE if both AP and STA support SMD */
	if (hapd->conf->smd.enabled && sta && sta->smd_info.smd_sta) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Len: %d of SMD IEs", SMD_IE_LEN);
		buflen += SMD_IE_LEN;
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_HOSTAPD_IF
	buflen += hostapd_if_assoc_resp_tail_len(sta);
#endif
#ifdef CONFIG_QCN_EXTN
	buflen += hostapd_modify_buflen_for_qcn_ie_extn(hapd);
#endif /* CONFIG_QCN_EXTN */
	/* Security Profile IE is appended unconditionally when configured;
	 * always reserve space regardless of CONFIG_IEEE80211BN. */
	if (hapd->conf->security_profiles)
		buflen += hostapd_security_profile_ie_len(hapd);

	buf = os_zalloc(buflen);
	if (!buf) {
		res = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto done;
	}
	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT,
			     (reassoc ? WLAN_FC_STYPE_REASSOC_RESP :
			      WLAN_FC_STYPE_ASSOC_RESP));

	os_memcpy(reply->da, addr, ETH_ALEN);
	os_memcpy(reply->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply->bssid, hapd->own_addr, ETH_ALEN);

	send_len = IEEE80211_HDRLEN;
	send_len += sizeof(reply->u.assoc_resp);
	reply->u.assoc_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd));
	reply->u.assoc_resp.status_code = host_to_le16(status_code);

	reply->u.assoc_resp.aid = host_to_le16((sta ? sta->aid : 0) |
					       BIT(14) | BIT(15));
	/* Supported rates */
	p = hostapd_eid_supp_rates(hapd, reply->u.assoc_resp.variable);
	/* Extended supported rates */
	p = hostapd_eid_ext_supp_rates(hapd, p);

	/* Radio measurement capabilities */
	p = hostapd_eid_rm_enabled_capab(hapd, p, buf + buflen - p);

#ifdef CONFIG_MBO
	if (status_code == WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS &&
	    rssi != 0) {
		int threshold;
		int delta;

		threshold = hapd->conf->rssi_reject_assoc_rssi ?
			hapd->conf->rssi_reject_assoc_rssi :
			hapd->iconf->rssi_reject_assoc_rssi;
		delta = threshold - rssi;
		if (delta < 0)
			delta = 0;
		if (delta > 127)
			delta = 127;

		p = hostapd_eid_mbo_rssi_assoc_rej(hapd, p, buf + buflen - p,
						   delta);
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211R_AP
	if (sta && status_code == WLAN_STATUS_SUCCESS) {
		/* IEEE 802.11r: Mobility Domain Information, Fast BSS
		 * Transition Information, RSN, [RIC Response] */
		p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, p,
						buf + buflen - p,
						sta->auth_alg, ies, ies_len,
						omit_rsnxe, reassoc,
						sta->vlan_id);
		if (!p) {
			wpa_printf(MSG_DEBUG,
				   "FT: Failed to write AssocResp IEs");
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
	}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_FILS
	if (sta && status_code == WLAN_STATUS_SUCCESS &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK))
		p = wpa_auth_write_assoc_resp_fils(sta->wpa_sm, p,
						   buf + buflen - p);
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if (sta && status_code == WLAN_STATUS_SUCCESS &&
	    ((hapd->conf->wpa_key_mgmt |
	      hapd->conf->rsn_override_key_mgmt |
	      hapd->conf->rsn_override_key_mgmt_2) &
	     WPA_KEY_MGMT_OWE))
		p = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, p,
						  buf + buflen - p);
#endif /* CONFIG_OWE */

	if (sta && status_code == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY)
		p = hostapd_eid_assoc_comeback_time(hapd, sta, p);

	p = hostapd_eid_ht_capabilities(hapd, p);
	p = hostapd_eid_ht_operation(hapd, p);

#ifdef CONFIG_IEEE80211AC
	if (hostapd_is_vht_enabled(hapd) &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		u32 nsts = 0, sta_nsts;

		if (sta && hapd->conf->use_sta_nsts && sta->vht_capabilities) {
			struct ieee80211_vht_capabilities *capa;

			nsts = (hapd->iface->conf->vht_capab >>
				VHT_CAP_BEAMFORMEE_STS_OFFSET) & 7;
			capa = sta->vht_capabilities;
			sta_nsts = (le_to_host32(capa->vht_capabilities_info) >>
				    VHT_CAP_BEAMFORMEE_STS_OFFSET) & 7;

			if (nsts < sta_nsts)
				nsts = 0;
			else
				nsts = sta_nsts;
		}
		p = hostapd_eid_vht_capabilities(hapd, p, nsts);
		p = hostapd_eid_vht_operation(hapd, p);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		p = hostapd_eid_he_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_he_operation(hapd, p);
		p = hostapd_eid_cca(hapd, p);
		p = hostapd_eid_spatial_reuse(hapd, p);
		p = hostapd_eid_he_mu_edca_parameter_set(hapd, p, false);
		p = hostapd_eid_he_6ghz_band_cap(hapd, p);
	}

	if (hapd->conf->mscs && sta && sta->mscs_ctxt)
		p = hostapd_add_mscs_desc(hapd, p, sta);

#endif /* CONFIG_IEEE80211AX */

	p = hostapd_eid_ext_capab(hapd, p, false);
	p = hostapd_eid_bss_max_idle_period(hapd, p,
					    sta ? sta->max_idle_period : 0);
	if (sta && sta->qos_map_enabled)
		p = hostapd_eid_qos_map_set(hapd, p);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(p, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		p += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->rsnxe_override_ft &&
	    buf + buflen - p >=
	    (long int) wpabuf_len(hapd->conf->rsnxe_override_ft) &&
	    sta && sta->auth_alg == WLAN_AUTH_FT) {
		wpa_printf(MSG_DEBUG, "TESTING: RSNXE FT override");
		os_memcpy(p, wpabuf_head(hapd->conf->rsnxe_override_ft),
			  wpabuf_len(hapd->conf->rsnxe_override_ft));
		p += wpabuf_len(hapd->conf->rsnxe_override_ft);
		goto rsnxe_done;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (!omit_rsnxe) {
		hapd->sp_ie_activated_sta = (sta && sta->sp_ie_validated);
#ifdef CONFIG_TESTING_OPTIONS
		p = hostapd_eid_rsnxe(hapd, p, buf + buflen - p,
				      hapd->conf->rsnxe_capab_mask);
#else /* CONFIG_TESTING_OPTIONS */
		p = hostapd_eid_rsnxe(hapd, p, buf + buflen - p, ~0ULL);
#endif /* CONFIG_TESTING_OPTIONS */
		hapd->sp_ie_activated_sta = false;
	}
#ifdef CONFIG_TESTING_OPTIONS
rsnxe_done:
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		u8 ext_cap = 0;

		if (hapd->conf->single_link_emlsr &&
		    (hapd->iface->mld_ext_mld_capa &
		     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
			ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

		if (hapd->conf->mld_ap)
			p = hostapd_eid_eht_ml_assoc(hapd, sta, p, ext_cap);
		p = hostapd_eid_eht_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_eht_operation(hapd, p);
		hostapd_get_epcs_capab(hapd, sta);

		/* Add Country element if Channel Usage element is present */
		if (hapd->chan_usage_config.num_elems > 0 &&
		    hapd->iconf->ieee80211d &&
		    hapd->iface->current_mode != NULL) {
			p = hostapd_eid_country(hapd, p, buf + buflen - p);
			p = hostapd_eid_channel_usage(hapd, p, buf + buflen - p);
		}
	}

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap &&
	    hapd->conf->ttlm_enable && sta &&
	    sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info.ttlm_resp_type ==
	    WLAN_STATUS_SUCCESS)
		hostapd_apply_ttlm_mapping_to_driver(hapd, sta);
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		p = hostapd_eid_uhr_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_uhr_operation(hapd, p, false);
		p = hostapd_eid_uhr_params_update(hapd, p, true, false);
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_QCN_EXTN
	p = hostapd_eid_qcn_vendor_ie_extn(hapd, p, IEEE80211_MODE_AP);
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_OWE
	if (((hapd->conf->wpa_key_mgmt | hapd->conf->rsn_override_key_mgmt |
	      hapd->conf->rsn_override_key_mgmt_2) & WPA_KEY_MGMT_OWE) &&
	    sta && sta->owe_ecdh && status_code == WLAN_STATUS_SUCCESS &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
	    !wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
		/* OWE Diffie-Hellman Parameter element */
		*p++ = WLAN_EID_EXTENSION; /* Element ID */
		*p++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*p++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension */
		WPA_PUT_LE16(p, sta->owe_group);
		p += 2;
		os_memcpy(p, wpabuf_head(pub), wpabuf_len(pub));
		p += wpabuf_len(pub);
		wpabuf_free(pub);
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_ENC_ASSOC
	if (sta &&
	    (sta->auth_alg == WLAN_AUTH_EPPKE ||
	     sta->auth_alg == WLAN_AUTH_802_1X) &&
	    wpa_auth_ap_sta_support_assoc_enc(sta->wpa_sm) &&
	    status_code == WLAN_STATUS_SUCCESS) {
		/* Ensure GMK/Counter are initialized before the Key Delivery
		 * element is built. For auth-frame STAs the normal 4-way
		 * handshake path (SM_STATE AUTHENTICATION2) is skipped, so
		 * wpa_group_ensure_init() must be called here instead.
		 * Without this, the first non-auth-frame STA that connects
		 * later triggers a GTK rotation that is unknown to this STA,
		 * breaking broadcast frame reception. */
		wpa_auth_ensure_group_init(sta->wpa_sm);
		reply->frame_control |= WLAN_FC_PROTECTED;

#ifdef CONFIG_PMKSA_PRIVACY
		/* Include a Nonce element (ANonce) to compute next PMKID */
		if (wpa_auth_ap_sta_support_pmkid_privacy(sta->wpa_sm)) {
			switch (sta->auth_alg) {
			case WLAN_AUTH_EPPKE:
#ifdef CONFIG_IEEE8021X_AUTH
			case WLAN_AUTH_802_1X:
#endif /* CONFIG_IEEE8021X_AUTH */
				break;
			default:
				wpa_printf(MSG_INFO,
					   "EPP: Unsupported auth alg %u for PMKID privacy support",
					   sta->auth_alg);
				goto skip_nonce;
			}

			*p++ = WLAN_EID_EXTENSION; /* Element ID */
			*p++ = 1 + NONCE_LEN; /* Length */
			*p++ = WLAN_EID_EXT_NONCE; /* Element ID Extension */
			os_memcpy(p, sta->anonce, NONCE_LEN);
			p += NONCE_LEN;
		}
	skip_nonce:
#ifdef CONFIG_SAE
		/* For EPPKE with SAE, propagate the password identifier from
		 * the PASN wrapped SAE commit to the WPA state machine so that
		 * wpa_auth_eid_key_delivery() can include the SAE PW IDs KDE
		 * in the encrypted (Re)Association Response frame.
		 */
		if (sta->auth_alg == WLAN_AUTH_EPPKE && sta->pasn &&
		    wpa_key_mgmt_sae(wpa_auth_sta_key_mgmt(sta->wpa_sm))) {
			const u8 *pw_id = NULL;
			size_t pw_id_len = 0;

			/* Use the decrypted (real) identifier when the STA
			 * presented an encrypted alternative identifier.
			 * Fall back to the raw parsed identifier for the
			 * plaintext case (first connection). */
			if (sta->pasn->dec_pw_id &&
			    sta->pasn->dec_pw_id_len) {
				pw_id = sta->pasn->dec_pw_id;
				pw_id_len = sta->pasn->dec_pw_id_len;
			} else if (sta->pasn->sae.tmp) {
				if (sta->pasn->sae.tmp->parsed_pw_id) {
					pw_id = sta->pasn->sae.tmp->parsed_pw_id;
					pw_id_len = sta->pasn->sae.tmp->parsed_pw_id_len;
				} else if (sta->pasn->sae.tmp->pw_id) {
					pw_id = sta->pasn->sae.tmp->pw_id;
					pw_id_len =
					sta->pasn->sae.tmp->pw_id_len;
				}
			}
			if (pw_id && pw_id_len) {
				struct wpabuf *pw_id_buf;

				pw_id_buf = wpabuf_alloc_copy(pw_id, pw_id_len);
				if (pw_id_buf) {
					wpa_auth_set_sae_pw_id(
						sta->wpa_sm, pw_id_buf,
						sta->pasn->sae_pw_id_counter);
					wpabuf_free(pw_id_buf);
				}
			}
		}
#endif /* CONFIG_SAE */
#endif /* CONFIG_PMKSA_PRIVACY */

		p = wpa_auth_write_assoc_resp_eppke(sta->wpa_sm, p,
						    buf + buflen - p,
						    ap_sta_is_mld(hapd, sta));
	}
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1 && (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
	    sta && sta->dpp_pfs && status_code == WLAN_STATUS_SUCCESS &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP) {
		os_memcpy(p, wpabuf_head(sta->dpp_pfs->ie),
			  wpabuf_len(sta->dpp_pfs->ie));
		p += wpabuf_len(sta->dpp_pfs->ie);
	}
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_IEEE80211AC
	if (sta && hapd->conf->vendor_vht && (sta->flags & WLAN_STA_VENDOR_VHT))
		p = hostapd_eid_vendor_vht(hapd, p);
#endif /* CONFIG_IEEE80211AC */

	if (sta && (sta->flags & WLAN_STA_WMM))
		p = hostapd_eid_wmm(hapd, p, false);

	p = hostapd_add_wfa_cap_ie(hapd, sta, p);

#ifdef CONFIG_WPS
	if (sta &&
	    ((sta->flags & WLAN_STA_WPS) ||
	     ((sta->flags & WLAN_STA_MAYBE_WPS) && hapd->conf->wpa))) {
		struct wpabuf *wps = wps_build_assoc_resp_ie();
		if (wps) {
			os_memcpy(p, wpabuf_head(wps), wpabuf_len(wps));
			p += wpabuf_len(wps);
			wpabuf_free(wps);
		}
	}
#endif /* CONFIG_WPS */

	if (sta && (sta->flags & WLAN_STA_MULTI_AP))
		p = hostapd_eid_multi_ap(hapd, p, buf + buflen - p);

#ifdef CONFIG_QCN_EXTN
	/* WDS vendor IE in association response */
	if (sta && sta->sta_extn.wds_ie_peer)
		p = hostapd_eid_wds_ie_extn(hapd, p, buf + buflen - p);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_P2P
	if (sta && sta->p2p_ie && hapd->p2p_group) {
		struct wpabuf *p2p_resp_ie;
		enum p2p_status_code status;
		switch (status_code) {
		case WLAN_STATUS_SUCCESS:
			status = P2P_SC_SUCCESS;
			break;
		case WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA:
			status = P2P_SC_FAIL_LIMIT_REACHED;
			break;
		default:
			status = P2P_SC_FAIL_INVALID_PARAMS;
			break;
		}
		p2p_resp_ie = p2p_group_assoc_resp_ie(hapd->p2p_group, status);
		if (p2p_resp_ie) {
			os_memcpy(p, wpabuf_head(p2p_resp_ie),
				  wpabuf_len(p2p_resp_ie));
			p += wpabuf_len(p2p_resp_ie);
			wpabuf_free(p2p_resp_ie);
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_P2P_MANAGER
	if (hapd->conf->p2p & P2P_MANAGE)
		p = hostapd_eid_p2p_manage(hapd, p);
#endif /* CONFIG_P2P_MANAGER */

	p = hostapd_eid_mbo(hapd, p, buf + buflen - p);

#ifdef CONFIG_QCN_EXTN
	hostapd_update_assoc_resp_with_hop_count_extn(hapd);
#endif

	if (hapd->conf->assocresp_elements &&
	    (size_t) (buf + buflen - p) >=
	    wpabuf_len(hapd->conf->assocresp_elements)) {
		os_memcpy(p, wpabuf_head(hapd->conf->assocresp_elements),
			  wpabuf_len(hapd->conf->assocresp_elements));
		p += wpabuf_len(hapd->conf->assocresp_elements);
	}

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_assoc_resp_tail(sta, buflen, p - buf, &p);
#endif

	send_len += p - reply->u.assoc_resp.variable;

#ifdef CONFIG_FILS
	if (sta &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK) &&
	    status_code == WLAN_STATUS_SUCCESS) {
		struct ieee802_11_elems elems;

		if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) ==
		    ParseFailed || !elems.fils_session) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}

		/* FILS Session */
		*p++ = WLAN_EID_EXTENSION; /* Element ID */
		*p++ = 1 + FILS_SESSION_LEN; /* Length */
		*p++ = WLAN_EID_EXT_FILS_SESSION; /* Element ID Extension */
		os_memcpy(p, elems.fils_session, FILS_SESSION_LEN);
		send_len += 2 + 1 + FILS_SESSION_LEN;

		send_len = fils_encrypt_assoc(sta->wpa_sm, buf, send_len,
					      buflen, sta->fils_hlp_resp);
		if (send_len < 0) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->ttlm_enable &&
	    (sta && hapd->mld_link_id == sta->mld_assoc_link_id &&
	     sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info.ttlm_resp_type ==
	     TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING)) {
		struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
		size_t ttlm_elem_len;
		u8 *ttlm_elem = NULL;
		int err;

		ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;
		err = hostapd_build_ttlm_elem(ongoing_ttlm, &ttlm_elem, &ttlm_elem_len);

		if (!err) {
			os_memcpy(p, ttlm_elem, ttlm_elem_len);
			p = p + ttlm_elem_len;
			send_len += ttlm_elem_len;
		}
		os_free(ttlm_elem);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	/* Add SMD IE to association response if both AP and STA support SMD */
	if (hapd->conf->smd.enabled && sta && sta->smd_info.smd_sta) {
		p = hostapd_eid_smd_ie_response(hapd, sta, p);
		send_len += SMD_IE_LEN;
	}
#endif /* CONFIG_IEEE80211BN */

	if (hapd->conf->control_frame_prot &&
	    (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_CIGTK) &&
	    (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_CIP_PADDING_SUPPORT) &&
	    (sta->control_mic_pad != CONTROL_MIC_PAD_NOT_SET)) {
		u8 padding_delay = sta->control_mic_pad;
		struct wpabuf *cip_ie = cip_build_assoc_resp_ie(padding_delay);

		if (cip_ie) {
			os_memcpy(p, wpabuf_head(cip_ie), wpabuf_len(cip_ie));
			p += wpabuf_len(cip_ie);
			send_len += wpabuf_len(cip_ie);
			wpabuf_free(cip_ie);
			wpa_printf(MSG_DEBUG, "CIP: Added CIP Capability IE"
				   "with Padding Delay = %u", padding_delay);
		}
	}

	if (hapd->conf->security_profiles) {
		u8 *sec_prof_start = p;
		p = hostapd_eid_security_profile(hapd, p);
		send_len += (p - sec_prof_start);
		wpa_printf(MSG_ERROR,
			   "UHR: Added Security Profile IE to Association Response (len=%zu)",
			   (size_t)(p - sec_prof_start));
	}


	if (hostapd_drv_send_mlme(hapd, reply, send_len, 0, NULL, 0, 0, 0, 0) < 0) {
		wpa_printf(MSG_INFO, "Failed to send assoc resp: %s",
			   strerror(errno));
		res = WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

done:
	os_free(buf);
	return res;
}

#ifdef CONFIG_OWE
u8 * owe_assoc_req_process(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *owe_dh, u8 owe_dh_len,
			   u8 *owe_buf, size_t owe_buf_len, u16 *status)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->own_ie_override) {
		wpa_printf(MSG_DEBUG, "OWE: Using IE override");
		*status = WLAN_STATUS_SUCCESS;
		return wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
						     owe_buf_len);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		wpa_printf(MSG_DEBUG, "OWE: Using PMKSA caching");
		owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
							owe_buf_len);
		*status = WLAN_STATUS_SUCCESS;
		return owe_buf;
	}

	if (sta->owe_pmk && sta->external_dh_updated) {
		wpa_printf(MSG_DEBUG, "OWE: Using previously derived PMK");
		*status = WLAN_STATUS_SUCCESS;
		return owe_buf;
	}

	*status = owe_process_assoc_req(hapd, sta, owe_dh, owe_dh_len);
	if (*status != WLAN_STATUS_SUCCESS)
		return NULL;

	owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
						owe_buf_len);

	if (sta->owe_ecdh && owe_buf) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			*status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			return owe_buf;
		}

		/* OWE Diffie-Hellman Parameter element */
		*owe_buf++ = WLAN_EID_EXTENSION; /* Element ID */
		*owe_buf++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*owe_buf++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension
							 */
		WPA_PUT_LE16(owe_buf, sta->owe_group);
		owe_buf += 2;
		os_memcpy(owe_buf, wpabuf_head(pub), wpabuf_len(pub));
		owe_buf += wpabuf_len(pub);
		wpabuf_free(pub);
	}

	return owe_buf;
}
#endif /* CONFIG_OWE */


#ifdef CONFIG_FILS

void fils_hlp_finish_assoc(struct hostapd_data *hapd, struct sta_info *sta)
{
	u16 reply_res;

	wpa_printf(MSG_DEBUG, "FILS: Finish association with " MACSTR,
		   MAC2STR(sta->addr));
	eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
	if (!sta->fils_pending_assoc_req)
		return;
	reply_res = send_assoc_resp(hapd, sta, sta->addr, WLAN_STATUS_SUCCESS,
				    sta->fils_pending_assoc_is_reassoc,
				    sta->fils_pending_assoc_req,
				    sta->fils_pending_assoc_req_len, 0, 0);
	os_free(sta->fils_pending_assoc_req);
	sta->fils_pending_assoc_req = NULL;
	sta->fils_pending_assoc_req_len = 0;
	wpabuf_free(sta->fils_hlp_resp);
	sta->fils_hlp_resp = NULL;
	wpabuf_free(sta->hlp_dhcp_discover);
	sta->hlp_dhcp_discover = NULL;

	/*
	 * Remove the station in case transmission of a success response fails.
	 * At this point the station was already added associated to the driver.
	 */
	if (reply_res != WLAN_STATUS_SUCCESS)
		hostapd_drv_sta_remove(hapd, sta->addr);
}


void fils_hlp_timeout(void *eloop_ctx, void *eloop_data)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = eloop_data;

	wpa_printf(MSG_DEBUG,
		   "FILS: HLP response timeout - continue with association response for "
		   MACSTR, MAC2STR(sta->addr));
	if (sta->fils_drv_assoc_finish)
		hostapd_notify_assoc_fils_finish(hapd, sta);
	else
		fils_hlp_finish_assoc(hapd, sta);
}

#endif /* CONFIG_FILS */


#ifdef CONFIG_IEEE80211BE
static struct sta_info * handle_mlo_translate(struct hostapd_data *hapd,
					      const struct ieee80211_mgmt *mgmt,
					      size_t len, bool reassoc,
					      struct hostapd_data **assoc_hapd,
					      u8 *assoc_mld_addr)
{
	struct sta_info *sta;
	struct ieee802_11_elems elems;
	u8 mld_addr[ETH_ALEN];
	const u8 *pos;

	if (!hostapd_is_eht_enabled(hapd))
		return NULL;

	if (reassoc) {
		len -= IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req);
		pos = mgmt->u.reassoc_req.variable;
	} else {
		len -= IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req);
		pos = mgmt->u.assoc_req.variable;
	}

	if (ieee802_11_parse_elems(pos, len, &elems, 1) == ParseFailed)
		return NULL;

	if (hostapd_process_ml_assoc_req_addr(hapd, elems.basic_mle,
					      elems.basic_mle_len,
					      mld_addr))
		return NULL;

	os_memcpy(assoc_mld_addr, mld_addr, ETH_ALEN);

	sta = ap_get_sta(hapd, mld_addr);
	if (!sta)
		return NULL;

	wpa_printf(MSG_DEBUG, "MLD: assoc: mld=" MACSTR ", link=" MACSTR,
		   MAC2STR(mld_addr), MAC2STR(mgmt->sa));

	return sta;
}
#endif /* CONFIG_IEEE80211BE */


#ifdef CONFIG_IEEE80211R_AP
static const u8 *
hostapd_mlie_to_get_mld_addr_from_assoc(struct hostapd_data *hapd,
					const struct ieee80211_mgmt *mgmt,
					size_t len, int reassoc)
{
	struct ieee802_11_elems elems;
	const u8 *pos;
	int assoc_ies_len;

	if (!hapd->mld)
		return NULL;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return NULL;
#endif /* CONFIG_QCN_EXTN */

	pos = reassoc ? mgmt->u.reassoc_req.variable : mgmt->u.assoc_req.variable;
	if (!pos)
		return NULL;

	if (reassoc) {
		len -= offsetof(struct ieee80211_mgmt, u.reassoc_req.variable);
		assoc_ies_len = (int)len - (pos - mgmt->u.reassoc_req.variable);
	} else {
		len -= offsetof(struct ieee80211_mgmt, u.assoc_req.variable);
		assoc_ies_len = (int)len - (pos - mgmt->u.assoc_req.variable);
	}

	if (ieee802_11_parse_elems(pos, assoc_ies_len,
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Failed parsing Authentication frame");
		return NULL;
	}

	if (!elems.basic_mle || !elems.basic_mle_len)
		return NULL;

	return get_basic_mle_mld_addr(elems.basic_mle, elems.basic_mle_len);
}

struct wpa_state_machine *get_wpa_sm_from_ft_ds_list(struct hostapd_data *hapd,
						     uint8_t *sta_mld_addr)
{
	struct hostapd_ft_over_ds_ml_sta_entry *entry;

	entry = ap_get_ft_ds_ml_sta(hapd, sta_mld_addr);
	if (!entry) {
		wpa_printf(MSG_ERROR, "FT: Entry not found for sta_mld " MACSTR,
			   MAC2STR(sta_mld_addr));
		return NULL;
	}

	return entry->wpa_sm;
}

static struct sta_info *
get_sta_from_ft_ds_list(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt,
			size_t len, int reassoc)
{
	struct hostapd_ft_over_ds_ml_sta_entry *entry;
	const u8 *sta_mld;
	struct wpa_state_machine *wpa_sm;
	struct sta_info *sta = NULL;

	if (!hapd->mld)
		return NULL;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return NULL;
#endif /* CONFIG_QCN_EXTN */

	sta_mld = hostapd_mlie_to_get_mld_addr_from_assoc(hapd, mgmt, len, reassoc);
	if (!sta_mld)
		return NULL;

	entry = ap_get_ft_ds_ml_sta(hapd, sta_mld);
	if (!entry) {
		wpa_printf(MSG_DEBUG, "FT: Entry not found for sta_mld " MACSTR,
			   MAC2STR(sta_mld));
		return NULL;
	}

	wpa_sm = entry->wpa_sm;

	if (hapd->mld_link_id != wpa_sm->wpa_auth->link_id) {
		wpa_printf(MSG_DEBUG, "FT: assoc link id different from"
			   " the MLD id hence changing wpa_auth of sm to assoc link");
		wpa_group_put_sm(wpa_sm);
		wpa_sm->wpa_auth = hapd->wpa_auth;
		wpa_sm->group = hapd->wpa_auth->group;
		wpa_group_get_sm(wpa_sm);
	}

	eloop_cancel_timeout(hostap_ft_ds_ml_sta_timeout, entry, NULL);

	if (wpa_sm) {
		if (!wpa_sm->group)
			wpa_sm->group = hapd->wpa_auth->group;
	}  else {
		wpa_printf(MSG_DEBUG, "%s : Temp reject the station as it is a existing entry",
			   __func__);
		goto free_entry;
	}

	sta = ap_sta_add(hapd, sta_mld);
	if (!sta) {
		if (wpa_sm)
			wpa_auth_sta_deinit(wpa_sm);
		goto free_entry;
	}

	sta->auth_alg = WLAN_AUTH_FT;
	sta->ft_over_ds = true;
	sta->wpa_sm = wpa_sm;
	if (sta_mld) {
		u8 link_id = hapd->mld_link_id;

		sta->mld_info.mld_sta = true;
		sta->mld_assoc_link_id = link_id;

		os_memcpy(sta->mld_info.common_info.mld_addr, sta_mld, ETH_ALEN);
		os_memcpy(sta->mld_info.links[link_id].peer_addr, mgmt->sa, ETH_ALEN);
		os_memcpy(sta->mld_info.links[link_id].local_addr, hapd->own_addr, ETH_ALEN);
	}

free_entry:
	dl_list_del(&entry->list);
	os_free(entry);
	//sta->ft_over_ds_saquery_status = sa_query_status;

	return sta;
}
#endif

static struct wpabuf *cip_build_assoc_resp_ie(u8 padding_delay)
{
	struct wpabuf *ie = wpabuf_alloc(CIP_CAPAB_LEN);

	if (!ie)
		return NULL;
	wpabuf_put_u8(ie, WLAN_EID_EXTENSION);
	wpabuf_put_u8(ie, 2);
	wpabuf_put_u8(ie, WLAN_EID_EXT_CIP_CAPAB);
	wpabuf_put_u8(ie, padding_delay);

	return ie;
}

static u16 check_rssi_rejection_timeout(struct sta_info *sta,
					 const struct ieee80211_mgmt *mgmt)
{
	/* Check if client is still in RSSI rejection timeout */
	if (sta && (sta->rssi_reject_timeout.sec != 0 || sta->rssi_reject_timeout.usec != 0)) {
		struct os_time now;
		os_get_time(&now);
		if (os_time_before(&now, &sta->rssi_reject_timeout)) {
			wpa_printf(MSG_INFO,
				   "Client " MACSTR " still in RSSI rejection timeout",
				   MAC2STR(mgmt->sa));
			return WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS;
		} else {
			/* Timeout expired, clear it */
			os_memset(&sta->rssi_reject_timeout, 0, sizeof(sta->rssi_reject_timeout));
		}
	}

	return WLAN_STATUS_SUCCESS;
}

static u16 check_rssi_association(struct hostapd_data *hapd,
				  const struct ieee80211_mgmt *mgmt,
				  int rssi, struct sta_info *sta)
{
	int rssi_threshold = 0;
	int rssi_timeout = 0;
	const char *source = "disabled";

	/* Try BSS-specific threshold first */
	if (hapd->conf->rssi_reject_assoc_rssi != 0) {
		rssi_threshold = hapd->conf->rssi_reject_assoc_rssi;
		rssi_timeout = hapd->conf->rssi_reject_assoc_timeout;
		source = "BSS override";
		wpa_printf(MSG_DEBUG,
			   "RSSI monitor: using BSS override threshold=%d dBm",
			   rssi_threshold);
	}
	/* Fall back to radio-wide threshold */
	else if (hapd->iconf->rssi_reject_assoc_rssi != 0) {
		rssi_threshold = hapd->iconf->rssi_reject_assoc_rssi;
		rssi_timeout = hapd->iconf->rssi_reject_assoc_timeout;
		source = "radio fallback";
		wpa_printf(MSG_DEBUG,
			   "RSSI monitor: using radio fallback threshold=%d dBm",
			   rssi_threshold);
	}

	/* Check threshold if enabled */
	if (rssi_threshold != 0 && rssi != 0) {
		if (rssi < rssi_threshold) {
			wpa_printf(MSG_INFO,
				   "RSSI %d dBm below threshold %d dBm - rejecting association from "
				   MACSTR " (source: %s, SSID: %s)",
				   rssi, rssi_threshold,
				   MAC2STR(mgmt->sa), source,
				   wpa_ssid_txt(hapd->conf->ssid.ssid,
						hapd->conf->ssid.ssid_len));

			/* Set timeout if configured */
			if (rssi_timeout > 0 && sta) {
				os_get_time(&sta->rssi_reject_timeout);
				sta->rssi_reject_timeout.sec += rssi_timeout;
				wpa_printf(MSG_DEBUG,
					   "Set RSSI rejection timeout for " MACSTR " (%d seconds)",
					   MAC2STR(mgmt->sa), rssi_timeout);
			}

			return WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS;
		} else {
			if (sta) {
				os_memset(&sta->rssi_reject_timeout, 0,
					  sizeof(sta->rssi_reject_timeout));
			}

			wpa_printf(MSG_DEBUG,
				   "RSSI %d dBm above threshold %d dBm - accepting association from "
				   MACSTR " (source: %s)",
				   rssi, rssi_threshold,
				   MAC2STR(mgmt->sa), source);
		}
	}

	return WLAN_STATUS_SUCCESS;
}

#ifdef CONFIG_IEEE80211BE
static void
handle_link_addr_conflict_sa_query_timeout(struct hostapd_data *hapd,
					   struct sta_info *sta)
{
	int i, j, k;

	if (!ap_sta_is_mld(hapd, sta)) {
		sta->link_addr_conflict_bitmap = 0;
		return;
	}

	/*
	 * Only re-walk the partner links whose bit is set in
	 * link_addr_conflict_bitmap — those are the links where a per-link
	 * address conflict was detected during the previous association
	 * attempt.  For each such link, find the conflicting station and, if
	 * its SA Query has now timed out, clean it up so the new association
	 * can proceed.  Partner links of the incoming STA are re-added
	 * automatically by check_assoc_ies() / ieee80211_ml_process_link().
	 */
	for (k = 0; k < MAX_NUM_MLD_LINKS; k++) {
		struct hostapd_data *hapd_ptr, *assoc_hapd;
		struct sta_info *osta, *assoc_sta;
		const u8 *link_addr;

		if (!(sta->link_addr_conflict_bitmap & BIT(k)))
			continue;

		/* Clear the bit regardless of outcome below */
		sta->link_addr_conflict_bitmap &= ~BIT(k);

		link_addr = sta->mld_info.links[k].peer_addr;

		for (i = 0; i < hapd->iface->interfaces->count; i++) {
			for (j = 0; j < hapd->iface->interfaces->iface[i]->num_bss; j++) {
				struct sta_info *tmp;

				hapd_ptr = hapd->iface->interfaces->iface[i]->bss[j];
				if (!hapd_ptr || !hapd_ptr->started)
					continue;

				osta = ap_get_sta(hapd_ptr, link_addr);
				if (!osta && hapd_ptr->conf->mld_ap) {
					for (tmp = hapd_ptr->sta_list;
					     tmp; tmp = tmp->next) {
						if (!tmp->mld_info.mld_sta)
							continue;
						if (!tmp->mld_info.links[hapd_ptr->mld_link_id].valid)
							continue;
						if (ether_addr_equal(
							tmp->mld_info.links[hapd_ptr->mld_link_id].peer_addr,
							link_addr)) {
							osta = tmp;
							break;
						}
					}
				}

				if (!osta)
					continue;

				/* Resolve to the assoc-link STA */
				assoc_sta = hostapd_ml_get_assoc_sta(
					hapd_ptr, osta, &assoc_hapd);
				if (assoc_sta) {
					osta = assoc_sta;
					hapd_ptr = assoc_hapd;
				}

				if (!osta->sa_query_timed_out)
					continue;

				wpa_printf(MSG_DEBUG,
					   "MLD: partner link %d addr " MACSTR
					   " conflict: SA Query timed out for STA "
					   MACSTR " on BSS %s, cleaning up",
					   k, MAC2STR(link_addr),
					   MAC2STR(osta->addr),
					   hapd_ptr->conf->iface);

				hostapd_drv_sta_deauth(hapd_ptr, osta->addr,
						       WLAN_REASON_PREV_AUTH_NOT_VALID);
				ap_sta_cleanup_all(hapd_ptr, osta, sta);
			}
		}
	}
}
#endif

static int
handle_assoc_sa_query_timeout_ml_setup(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       const struct ieee80211_mgmt *mgmt,
				       const u8 *pos, int left,
				       u8 *mld_addr,
				       bool do_drv_add, int reassoc)
{
	struct ieee802_11_elems elems;
	bool mld_link_sta = false, epp_sta = false;
	const u8 *mld_link_addr = NULL;
	u16 eml_cap = 0;

#ifdef CONFIG_ENC_ASSOC
	epp_sta = sta->epp_sta;
#endif /* CONFIG_ENC_ASSOC */
	if (ap_sta_is_authorized(sta))
		ap_sta_set_authorized(hapd, sta, 0);

	sta->flags &= ~WLAN_STA_ASSOC;
	sta->unadded_sta = false;
	ap_sta_set_sa_query_timeout(hapd, sta, 0);

	if (ieee802_11_parse_elems(pos, left, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "handle_assoc: failed to parse IEs");
		return -1;
	}

	if (!hostapd_process_ml_assoc_req_addr(hapd, elems.basic_mle,
					       elems.basic_mle_len, mld_addr)) {
		u8 link_id = hapd->mld_link_id;

		if (!sta->skip_sa_query)
			wpa_printf(MSG_DEBUG, "Allowing reassociation of MLD STA " MACSTR
			   " after SA Query timeout", MAC2STR(mld_addr));

		sta->mld_info.mld_sta = true;
		set_link_id_for_each_partner_link_sta(hapd, sta, link_id);
		sta->mld_assoc_link_id = link_id;
		os_memcpy(sta->mld_info.common_info.mld_addr, mld_addr, ETH_ALEN);
		os_memcpy(sta->mld_info.links[link_id].peer_addr, mgmt->sa, ETH_ALEN);
		os_memcpy(sta->mld_info.links[link_id].local_addr,
			  hapd->own_addr, ETH_ALEN);
		mld_link_sta = sta->mld_assoc_link_id != link_id;
		mld_link_addr = sta->mld_info.links[link_id].peer_addr;
		eml_cap = sta->mld_info.common_info.eml_capa;
	} else {
		if (!sta->skip_sa_query)
			wpa_printf(MSG_DEBUG, "Allowing reassociation of MLD STA as legacy"
				   " STA " MACSTR " after timed out SA Query procedure",
				   MAC2STR(mgmt->sa));
		memset(&sta->mld_info, 0x00, sizeof(sta->mld_info));
	}

	if (!do_drv_add)
		return 0;

	if (hostapd_sta_add(hapd, sta->addr, 0, 0,
			    sta->supported_rates, sta->supported_rates_len,
			    0, NULL, NULL, NULL, 0, NULL, 0, NULL, 0,
#ifdef CONFIG_QCN_EXTN
			    NULL,
#endif
#ifdef CONFIG_IEEE80211BN
                            sta->smd_info.smd_sta, sta->smd_info.caps.dl_data_fwd, sta->smd_info.smd_identifier,
                            false,
#else
                            0, 0, NULL, false,
#endif
			    NULL, sta->flags, 0, 0, 0, 0,
			    mld_link_addr, mld_link_sta,
			    eml_cap, reassoc, CONTROL_MIC_PAD_NOT_SET,
			    epp_sta)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "Could not add STA to kernel driver");
		return -1;
	}
	sta->pending_drv_add = false;
	sta->added_unassoc = 1;
	return 0;
}

static int hostapd_reset_sta_for_skip_sa_query(struct hostapd_data *hapd,
					       struct sta_info *sta,
					       const struct ieee80211_mgmt *mgmt,
					       const u8 *pos, int left,
					       int reassoc)
{
	u8 mld_addr[ETH_ALEN] = {0};
	wpa_printf(MSG_DEBUG, "Allowing reassocation sta " MACSTR
			      " without doing SA Query procedure",
		   MAC2STR(sta->addr));
	wpa_auth_sta_deinit(sta->wpa_sm);
	sta->wpa_sm = NULL;
	SET_EACH_PARTNER_STA_OBJ(hapd, sta, wpa_sm, NULL);
	hostapd_drv_sta_remove(hapd, sta->addr);
	ap_sta_remove_link_sta(hapd, sta, 0, true);

	if (handle_assoc_sa_query_timeout_ml_setup(
				hapd, sta, mgmt, pos, left, mld_addr,
				false, reassoc) < 0)
		return 0;

	sta->skip_sa_query = 0;
	return 1;
}

static void handle_assoc(struct hostapd_data *hapd,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int reassoc, int rssi)
{
	u16 capab_info, listen_interval, seq_ctrl, fc;
	int resp = WLAN_STATUS_SUCCESS;
	const u8 *pos;
	int left, i, ubus_resp;
	struct sta_info *sta, *tmp_sta;
	u8 *tmp = NULL;
	u8 *sa;
#ifdef CONFIG_HOSTAPD_IF
	int res;
#endif
#ifdef CONFIG_FILS
	int delay_assoc = 0;
#endif /* CONFIG_FILS */
	int omit_rsnxe = 0;
	bool set_beacon = false;
	u8 mld_addr[ETH_ALEN] = {0};
	struct hostapd_data *assoc_hapd;

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_req) :
				      sizeof(mgmt->u.assoc_req))) {
		wpa_printf(MSG_INFO, "handle_assoc(reassoc=%d) - too short payload (len=%lu)",
			   reassoc, (unsigned long) len);
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (reassoc) {
		if (hapd->iconf->ignore_reassoc_probability > 0.0 &&
		    drand48() < hapd->iconf->ignore_reassoc_probability) {
			wpa_printf(MSG_INFO,
				   "TESTING: ignoring reassoc request from "
				   MACSTR, MAC2STR(mgmt->sa));
			return;
		}
	} else {
		if (hapd->iconf->ignore_assoc_probability > 0.0 &&
		    drand48() < hapd->iconf->ignore_assoc_probability) {
			wpa_printf(MSG_INFO,
				   "TESTING: ignoring assoc request from "
				   MACSTR, MAC2STR(mgmt->sa));
			return;
		}
	}
#endif /* CONFIG_TESTING_OPTIONS */

	fc = le_to_host16(mgmt->frame_control);
	seq_ctrl = le_to_host16(mgmt->seq_ctrl);
	/* sa should always be MLD address for assoc req, except for
	 * ft_over_the_ds ml case.
	 */
	sa = (u8 *)mgmt->sa;

	if (reassoc) {
		capab_info = le_to_host16(mgmt->u.reassoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.reassoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "reassociation request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d current_ap="
			   MACSTR " seq_ctrl=0x%x%s",
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   MAC2STR(mgmt->u.reassoc_req.current_ap),
			   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "");
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
		pos = mgmt->u.reassoc_req.variable;
	} else {
		capab_info = le_to_host16(mgmt->u.assoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.assoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "association request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d "
			   "seq_ctrl=0x%x%s",
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "");
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
		pos = mgmt->u.assoc_req.variable;
	}

	sta = ap_get_sta(hapd, mgmt->sa);

	resp = check_rssi_rejection_timeout(sta, mgmt);
	if (resp != WLAN_STATUS_SUCCESS) {
		goto fail;
	}

#ifdef CONFIG_IEEE80211BE
	/*
	 * It is possible that the association frame is from an associated
	 * non-AP MLD station, that tries to re-associate using different link
	 * addresses. In such a case, try to find the station based on the AP
	 * MLD MAC address.
	 */

	/* STA should always be with MLD address, certain legacy sta can roam
 	 * back as MLO station with different MLD address, earlier legacy STA address
 	 * can now become link address. We need to fetch STA always with MLD address
 	 * in case of MLD. Hence fetch and override the old sta object
 	 */
	tmp_sta = handle_mlo_translate(hapd, mgmt, len, reassoc,
				   &assoc_hapd, mld_addr);
	if (tmp_sta)
		sta = tmp_sta;

#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211R_AP
	if (!sta) {
		sta = ap_get_unadded_sta(hapd, mgmt->sa);
		if (!sta) {
			wpa_printf(MSG_DEBUG,
				   "FT over DS: Check for STA entry with ML address");
			sta = get_sta_from_ft_ds_list(hapd, mgmt, len, reassoc);
		}
	}

	if (sta && sta->auth_alg == WLAN_AUTH_FT) {
		/*
		 * Mark station with WLAN_STA_FT_AUTH flag to open the port
		 * without waiting for EAPOL handshake in case of FT roaming.
		 */
		sta->flags |= WLAN_STA_FT_AUTH;
	}

	if (sta && sta->auth_alg == WLAN_AUTH_FT &&
	    (sta->flags & WLAN_STA_AUTH) == 0) {
		wpa_printf(MSG_DEBUG, "FT: Allow STA " MACSTR " to associate "
			   "prior to authentication since it is using "
			   "over-the-DS FT", MAC2STR(mgmt->sa));

		/*
		 * Mark station as authenticated, to avoid adding station
		 * entry in the driver as associated and not authenticated
		 */
		sta->flags |= WLAN_STA_AUTH;
	} else
#endif /* CONFIG_IEEE80211R_AP */
	if (sta == NULL || (sta->flags & WLAN_STA_AUTH) == 0) {
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode ==
			HOSTAPD_MODE_IEEE80211AD) {
			int acl_res;
			struct radius_sta info;

			if (hapd->conf->mld_ap &&
#ifdef CONFIG_QCN_EXTN
			    !hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
#endif /* CONFIG_QCN_EXTN */
			    sta && sta->mld_info.mld_sta) {
				acl_res = ieee802_11_allowed_address(hapd, sta->addr,
								     (const u8 *) mgmt,
								     len, &info);
			} else if (!is_zero_ether_addr(mld_addr)) {
				acl_res = ieee802_11_allowed_address(hapd, mld_addr,
								     (const u8 *) mgmt,
								     len, &info);
			} else {
				acl_res = ieee802_11_allowed_address(hapd, mgmt->sa,
								     (const u8 *) mgmt,
								     len, &info);
			}

			if (acl_res == HOSTAPD_ACL_REJECT) {
				wpa_msg(hapd->msg_ctx, MSG_DEBUG,
					"Ignore Association Request frame from "
					MACSTR " due to ACL reject",
					MAC2STR(mgmt->sa));
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			if (acl_res == HOSTAPD_ACL_PENDING)
				return;

			/* DMG/IEEE 802.11ad does not use authentication.
			 * Allocate sta entry upon association. */
			sta = ap_sta_add(hapd, mgmt->sa);
			if (!sta) {
				hostapd_logger(hapd, mgmt->sa,
					       HOSTAPD_MODULE_IEEE80211,
					       HOSTAPD_LEVEL_INFO,
					       "Failed to add STA");
				resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
				goto fail;
			}

			acl_res = ieee802_11_set_radius_info(
				hapd, sta, acl_res, &info);
			if (acl_res) {
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Skip authentication for DMG/IEEE 802.11ad");
			sta->flags |= WLAN_STA_AUTH;
			wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
			sta->auth_alg = WLAN_AUTH_OPEN;
		} else {
			hostapd_logger(hapd, mgmt->sa,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station tried to associate before authentication (aid=%d flags=0x%x)",
				       sta ? sta->aid : -1,
				       sta ? sta->flags : 0);
			send_deauth(hapd, mgmt->sa,
				    WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA);
			return;
		}
	}
#ifdef RDK_ONEWIFI
       os_free(sta->assoc_req);
       sta->assoc_req = os_malloc(len);
       os_memcpy(sta->assoc_req, (u8 *)mgmt, len);
       sta->assoc_req_len = len;
#endif
	if ((fc & WLAN_FC_RETRY) &&
	    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
	    sta->last_seq_ctrl == seq_ctrl &&
	    sta->last_subtype == (reassoc ? WLAN_FC_STYPE_REASSOC_REQ :
				  WLAN_FC_STYPE_ASSOC_REQ)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Drop repeated association frame seq_ctrl=0x%x",
			       seq_ctrl);
		return;
	}

	if (sta && !reassoc) {
		struct os_reltime now, age;

		os_get_reltime(&now);

		if (sta->last_assoc_req_rx_time.sec != 0 ||
		    sta->last_assoc_req_rx_time.usec != 0) {
			os_reltime_sub(&now, &sta->last_assoc_req_rx_time, &age);
			if (os_reltime_in_ms(&age) < WLAN_ASSOC_REQ_MIN_INTERVAL_MS) {
				wpa_printf(MSG_DEBUG,
					   "Dropping association from " MACSTR
					   " within %d ms of last (age=%d ms)",
					   MAC2STR(mgmt->sa), WLAN_ASSOC_REQ_MIN_INTERVAL_MS,
					   os_reltime_in_ms(&age));
				return;
			}
		}
	}

	sta->last_seq_ctrl = seq_ctrl;
	sta->last_subtype = reassoc ? WLAN_FC_STYPE_REASSOC_REQ :
		WLAN_FC_STYPE_ASSOC_REQ;

	if (hapd->tkip_countermeasures) {
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (listen_interval > hapd->conf->max_listen_interval) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Too large Listen Interval (%d)",
			       listen_interval);
		resp = WLAN_STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE;
		goto fail;
	}

	resp = check_rssi_association(hapd, mgmt, rssi, sta);
	if (resp != WLAN_STATUS_SUCCESS) {
		goto fail;
	}

#ifdef CONFIG_MBO
	if (hapd->conf->mbo_enabled && hapd->mbo_assoc_disallow) {
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto fail;
	}

	if (hapd->iconf->rssi_reject_assoc_rssi && rssi &&
	    rssi < hapd->iconf->rssi_reject_assoc_rssi &&
	    (sta->auth_rssi == 0 ||
	     sta->auth_rssi < hapd->iconf->rssi_reject_assoc_rssi)) {
		resp = WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS;
		goto fail;
	}
#endif /* CONFIG_MBO */
	if (sta && sta->auth_alg != WLAN_AUTH_FT) {
		struct hostapd_data *ohapd;
		struct sta_info *osta;

		osta = ap_sta_get_from_obss(hapd, sta->addr, mgmt->sa, &ohapd);
		if (!osta) {
			osta = ap_sta_get_by_link_addr(hapd, mgmt->sa, sta);
			ohapd = hapd;
		}
		if (osta && (osta->flags & WLAN_STA_MFP) && ap_sta_is_authorized(osta)) {
			wpa_printf(MSG_DEBUG, "Association request received from STA "MACSTR
				  " but sta is already associated in %s",
				  MAC2STR(sta->addr), ohapd->conf->iface);
			/* for an ML STA do SA procedure in Assoc link sta  */
			if (check_sa_query(ohapd, osta, reassoc, pos, left, sta,
					   fc & WLAN_FC_PROTECTED)) {
				wpa_printf(MSG_DEBUG, "SA query triggered for "MACSTR" on %s",
					   MAC2STR(osta->addr), ohapd->conf->iface);
				resp = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
				goto fail;
			} else if (osta->sa_query_timed_out || osta->skip_sa_query) {
				wpa_printf(MSG_DEBUG, "SA query timed out for " MACSTR " on %s, "
					   "delete it", MAC2STR(osta->addr), ohapd->conf->iface);
				ap_sta_cleanup_all(ohapd, osta, sta);
				sta->wpa_sm = NULL;

				if (handle_assoc_sa_query_timeout_ml_setup(
					    hapd, sta, mgmt, pos, left, mld_addr,
					    true, reassoc) < 0) {

					resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
					goto fail;
				}
				sta->skip_sa_query = 0;
			}
		}

	}

	if (hapd->conf->wpa && check_sa_query(hapd, sta, reassoc, pos, left, sta,
					      fc & WLAN_FC_PROTECTED)) {
		resp = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
		goto fail;
	}

	if (sta->skip_sa_query &&
	    !hostapd_reset_sta_for_skip_sa_query(hapd, sta, mgmt, pos, left, reassoc)) {
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto fail;
	}

	/*
	 * sta->capability is used in check_assoc_ies() for RRM enabled
	 * capability element.
	 */
	sta->capability = capab_info;

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		int res;

		/* The end of the payload is encrypted. Need to decrypt it
		 * before parsing. */

		tmp = os_memdup(pos, left);
		if (!tmp) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		res = fils_decrypt_assoc(sta->wpa_sm, sta->fils_session, mgmt,
					 len, tmp, left);
		if (res < 0) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		pos = tmp;
		left = res;
	}
#endif /* CONFIG_FILS */
	struct hostapd_ubus_request req = {
		.type = HOSTAPD_UBUS_ASSOC_REQ,
		.mgmt_frame = mgmt,
		.ssi_signal = rssi,
	};

	if ((sta->flags & WLAN_STA_MFP) &&
	     sta->sa_query_timed_out &&
	     sta->mld_info.mld_sta) {
		wpa_printf(MSG_DEBUG, "SA Query timed out for current STA "
			   MACSTR ", resetting ML info", MAC2STR(sta->addr));
		wpa_auth_sta_deinit(sta->wpa_sm);
		sta->wpa_sm = NULL;
		SET_EACH_PARTNER_STA_OBJ(hapd, sta, wpa_sm, NULL);
		hostapd_drv_sta_remove(hapd, sta->addr);
		ap_sta_remove_link_sta(hapd, sta, 0, true);

		if (handle_assoc_sa_query_timeout_ml_setup(
			    hapd, sta, mgmt, pos, left, mld_addr,
			    false, reassoc) < 0)
			goto fail;
	}

#ifdef CONFIG_IEEE80211BE
	if (sta->link_addr_conflict_bitmap)
		handle_link_addr_conflict_sa_query_timeout(hapd, sta);
#endif
	/* followed by SSID and Supported rates; and HT capabilities if 802.11n
	 * is used */
	resp = check_assoc_ies(hapd, sta, pos, left,
			       reassoc ? LINK_PARSE_REASSOC : LINK_PARSE_ASSOC);
	if (resp != WLAN_STATUS_SUCCESS)
		goto fail;

	if (!hostapd_check_assoc_pureg_rates(hapd, sta)) {
		resp = WLAN_STATUS_ASSOC_DENIED_RATES;
		goto fail;
	}

#ifdef CONFIG_IEEE80211R_AP
	if (reassoc && sta->auth_alg == WLAN_AUTH_FT)
		omit_rsnxe = !get_ie(pos, left, WLAN_EID_RSNX);
#endif /* CONFIG_IEEE80211R_AP */
	if (hapd->conf->rsn_override_omit_rsnxe)
		omit_rsnxe = 1;

#ifdef CONFIG_IEEE80211BN
	/* Parse SMD IE if present in association request */
	hostapd_parse_smd_ie(hapd, sta, pos, left);

	/*
	 * Transfer SMD info to wpa_state_machine after parsing.
	 * This ensures the WPA authenticator has access to SMD parameters
	 * for PTK derivation and other security operations.
	 */
	if (sta->wpa_sm && sta->smd_info.smd_sta) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Transferring SMD info to wpa_state_machine for "
			   MACSTR " after association",
			   MAC2STR(sta->addr));
		wpa_auth_set_smd_info(sta->wpa_sm, sta);
	}

#endif /* CONFIG_IEEE80211BN */

	if (hostapd_get_aid(hapd, sta) < 0) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "No room for more AIDs");
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto fail;
	}

	sta->listen_interval = listen_interval;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		sta->flags |= WLAN_STA_NONERP;
	for (i = 0; i < sta->supported_rates_len; i++) {
		if ((sta->supported_rates[i] & 0x7f) > 22) {
			sta->flags &= ~WLAN_STA_NONERP;
			break;
		}
	}
	if (sta->flags & WLAN_STA_NONERP && !sta->nonerp_set) {
		sta->nonerp_set = 1;
		hapd->iface->num_sta_non_erp++;
		if (hapd->iface->num_sta_non_erp == 1)
			set_beacon = true;
	}

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_SLOT_TIME) &&
	    !sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 1;
		hapd->iface->num_sta_no_short_slot_time++;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode ==
		    HOSTAPD_MODE_IEEE80211G &&
		    hapd->iface->num_sta_no_short_slot_time == 1)
			set_beacon = true;
	}

	if (sta)
		hostapd_check_dscp_policy_capability(sta, pos, left);

	if (sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sta->flags |= WLAN_STA_SHORT_PREAMBLE;
	else
		sta->flags &= ~WLAN_STA_SHORT_PREAMBLE;

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE) &&
	    !sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 1;
		hapd->iface->num_sta_no_short_preamble++;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 1)
			set_beacon = true;
	}

#ifdef CONFIG_QCN_EXTN
	/*
	 * Record the SNR of the (Re)Association Request frame so that
	 * update_sta_ht() can decide whether to honour the station's
	 * HT 40 MHz Intolerant indication.
	 */
	sta->sta_extn.assoc_snr = hostapd_rssi_to_snr_extn(hapd, rssi);
#endif /* CONFIG_QCN_EXTN */
	if (update_ht_state(hapd, sta) > 0)
		set_beacon = true;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "association OK (aid %d)", sta->aid);
	/* Station will be marked associated, after it acknowledges AssocResp
	 */
	sta->flags |= WLAN_STA_ASSOC_REQ_OK;

	if ((sta->flags & WLAN_STA_MFP) && sta->sa_query_timed_out) {
		wpa_printf(MSG_DEBUG, "Allowing %sassociation after timed out "
			   "SA Query procedure", reassoc ? "re" : "");
		/* TODO: Send a protected Disassociate frame to the STA using
		 * the old key and Reason Code "Previous Authentication no
		 * longer valid". Make sure this is only sent protected since
		 * unprotected frame would be received by the STA that is now
		 * trying to associate.
		 */
		sta->flags &= ~WLAN_STA_AUTHORIZED;
	}

	/* Make sure that the previously registered inactivity timer will not
	 * remove the STA immediately. */
	sta->timeout_next = STA_NULLFUNC;

#ifdef CONFIG_TAXONOMY
	taxonomy_sta_info_assoc_req(hapd, sta, pos, left);
#endif /* CONFIG_TAXONOMY */

	sta->pending_wds_enable = 0;

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		if (fils_process_hlp(hapd, sta, pos, left) > 0)
			delay_assoc = 1;
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		wpa_printf(MSG_INFO, "STA " MACSTR " for acl checking",
			   MAC2STR(sta->addr));
		if (hostapd_check_ml_acl(hapd, sta) == HOSTAPD_ACL_REJECT) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	ubus_resp = hostapd_ubus_handle_event(hapd, &req);
	if (ubus_resp) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " assoc rejected by ubus handler.\n",
		       MAC2STR(mgmt->sa));
		resp = ubus_resp > 0 ? (u16) ubus_resp : WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
 fail:

	/*
	 * In case of a successful response, add the station to the driver.
	 * Otherwise, the kernel may ignore Data frames before we process the
	 * ACK frame (TX status). In case of a failure, this station will be
	 * removed.
	 *
	 * Note that this is not compliant with the IEEE 802.11 standard that
	 * states that a non-AP station should transition into the
	 * authenticated/associated state only after the station acknowledges
	 * the (Re)Association Response frame. However, still do this as:
	 *
	 * 1. In case the station does not acknowledge the (Re)Association
	 *    Response frame, it will be removed.
	 * 2. Data frames will be dropped in the kernel until the station is
	 *    set into authorized state, and there are no significant known
	 *    issues with processing other non-Data Class 3 frames during this
	 *    window.
	 */
	if (sta)
		hostapd_process_assoc_ml_info(hapd, sta, pos, left, reassoc,
					      resp, false, &set_beacon);

	if (resp != WLAN_STATUS_SUCCESS)
		wpa_auth_connection_fail_event(hapd->wpa_auth,
					       len, "Association_frame",
					       (u8 *)mgmt, sta->addr,
					       hapd->own_addr, resp,
					       true);
#ifdef CONFIG_IEEE80211BE
	if (sta)
		hostapd_handle_ttlm_assoc_req(hapd, mgmt, len, sta, pos, left);
#endif /* CONFIG_IEEE80211BE */

	if (resp == WLAN_STATUS_SUCCESS && sta &&
	    add_associated_sta(hapd, sta, reassoc))
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;

#ifdef CONFIG_FILS
	if (sta && delay_assoc && resp == WLAN_STATUS_SUCCESS &&
	    eloop_is_timeout_registered(fils_hlp_timeout, hapd, sta) &&
	    sta->fils_pending_assoc_req) {
		/* Do not reschedule fils_hlp_timeout in case the station
		 * retransmits (Re)Association Request frame while waiting for
		 * the previously started FILS HLP wait, so that the timeout can
		 * be determined from the first pending attempt. */
		wpa_printf(MSG_DEBUG,
			   "FILS: Continue waiting for HLP processing before sending (Re)Association Response frame to "
			   MACSTR, MAC2STR(sta->addr));
		os_free(tmp);
		return;
	}
	if (sta) {
		eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
		os_free(sta->fils_pending_assoc_req);
		sta->fils_pending_assoc_req = NULL;
		sta->fils_pending_assoc_req_len = 0;
		wpabuf_free(sta->fils_hlp_resp);
		sta->fils_hlp_resp = NULL;
	}
	if (sta && delay_assoc && resp == WLAN_STATUS_SUCCESS) {
		sta->fils_pending_assoc_req = tmp;
		sta->fils_pending_assoc_req_len = left;
		sta->fils_pending_assoc_is_reassoc = reassoc;
		sta->fils_drv_assoc_finish = 0;
		wpa_printf(MSG_DEBUG,
			   "FILS: Waiting for HLP processing before sending (Re)Association Response frame to "
			   MACSTR, MAC2STR(sta->addr));
		eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
		eloop_register_timeout(0, hapd->conf->fils_hlp_wait_time * 1024,
				       fils_hlp_timeout, hapd, sta);
		return;
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_HOSTAPD_IF
	res = hostapd_if_notify_assoc(hapd, sta, (const u8 *)mgmt, len,
				      resp, reassoc, rssi, set_beacon,
				      sa);
#endif
	if (sta && !reassoc)
		os_get_reltime(&sta->last_assoc_req_rx_time);

#ifdef CONFIG_HOSTAPD_IF
	if (res == HOSTAPD_IF_FRAME_PROCESSING_WAIT)
		return;
#endif
	initiate_assoc_response(hapd, sta, resp, reassoc, tmp, pos, left,
			omit_rsnxe, sa, rssi, set_beacon);

}

void
initiate_assoc_response(struct hostapd_data *hapd, struct sta_info *sta,
			int resp, int reassoc,
			uint8_t *tmp, const u8 *pos, int left,
			int omit_rsnxe, uint8_t *sa, int rssi,
			bool set_beacon)
{
	u16 reply_res = WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (resp >= 0)
		reply_res = send_assoc_resp(hapd,
					    sta,
					    sa, resp, reassoc,
					    pos, left, rssi, omit_rsnxe);

	if (sta && (resp < 0 || reply_res != WLAN_STATUS_SUCCESS)) {
		ap_sta_reset_assoc_req_rx_times(sta);
#ifdef CONFIG_QCN_EXTN
		wpa_printf(MSG_DEBUG, "assoc_reject: STA " MACSTR " status=%u",
			   MAC2STR(sta->addr), reply_res);
		hostapd_log_trigger_emit(hapd, sta->addr,
					 HOSTAPD_LOG_TRIG_ASSOC_REJECT);
#endif /* CONFIG_QCN_EXTN */
	}

	if (set_beacon)
		ieee802_11_update_beacons(hapd->iface);

	os_free(tmp);

	/*
	 * Remove the station in case transmission of a success response fails
	 * (the STA was added associated to the driver) or if the station was
	 * previously added unassociated.
	 */
	if (sta && ((reply_res != WLAN_STATUS_SUCCESS &&
		     resp == WLAN_STATUS_SUCCESS) || sta->added_unassoc)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


static void hostapd_deauth_sta(struct hostapd_data *hapd,
			       struct sta_info *sta,
			       const struct ieee80211_mgmt *mgmt)
{
	wpa_msg(hapd->msg_ctx, MSG_DEBUG,
		"deauthentication: STA=" MACSTR " reason_code=%d",
		MAC2STR(mgmt->sa), le_to_host16(mgmt->u.deauth.reason_code));

	ap_sta_set_authorized(hapd, sta, 0);
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC |
			WLAN_STA_ASSOC_REQ_OK);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "deauthenticated");
	mlme_deauthenticate_indication(
		hapd, sta, le_to_host16(mgmt->u.deauth.reason_code));
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


static void hostapd_disassoc_sta(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 const struct ieee80211_mgmt *mgmt)
{
	wpa_msg(hapd->msg_ctx, MSG_DEBUG,
		"disassocation: STA=" MACSTR " reason_code=%d",
		MAC2STR(mgmt->sa), le_to_host16(mgmt->u.disassoc.reason_code));

	ap_sta_set_authorized(hapd, sta, 0);
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;
	sta->flags &= ~(WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	/* Stop Accounting and IEEE 802.1X sessions, but leave the STA
	 * authenticated. */
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(hapd, sta);
	if (sta->ipaddr)
		hostapd_drv_br_delete_ip_neigh(hapd, 4, (u8 *) &sta->ipaddr);
	ap_sta_ip6addr_del(hapd, sta);
	hostapd_drv_sta_remove(hapd, sta->addr);
	sta->added_unassoc = 0;

	if (sta->timeout_next == STA_NULLFUNC ||
	    sta->timeout_next == STA_DISASSOC) {
		sta->timeout_next = STA_DEAUTH;
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
	}

	mlme_disassociate_indication(
		hapd, sta, le_to_host16(mgmt->u.disassoc.reason_code));

	/* DMG/IEEE 802.11ad does not use deauthication. Deallocate sta upon
	 * disassociation. */
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD) {
		sta->flags &= ~WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "deauthenticated");
		ap_free_sta(hapd, sta);
	}
}


static bool hostapd_ml_handle_disconnect(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 const struct ieee80211_mgmt *mgmt,
					 bool disassoc)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *assoc_hapd, *tmp_hapd;
	struct sta_info *assoc_sta;
	struct sta_info *tmp_sta;

	if (!hostapd_is_multiple_link_mld(hapd))
		return false;

	/*
	 * Get the station on which the association was performed, as it holds
	 * the information about all the other links.
	 */
	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta)
		return false;

	for_each_mld_link(tmp_hapd, assoc_hapd) {
		if (tmp_hapd == assoc_hapd)
			continue;

		if (!assoc_sta->mld_info.links[tmp_hapd->mld_link_id].valid)
			continue;

		for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
		     tmp_sta = tmp_sta->next) {
			if (tmp_sta->mld_assoc_link_id !=
			    assoc_sta->mld_assoc_link_id ||
			    tmp_sta->aid != assoc_sta->aid)
				continue;

			if (!disassoc)
				hostapd_deauth_sta(tmp_hapd, tmp_sta, mgmt);
			else
				hostapd_disassoc_sta(tmp_hapd, tmp_sta, mgmt);
			break;
		}
	}

	/* Remove the station on which the association was performed. */
	if (!disassoc)
		hostapd_deauth_sta(assoc_hapd, assoc_sta, mgmt);
	else
		hostapd_disassoc_sta(assoc_hapd, assoc_sta, mgmt);

	return true;
#else /* CONFIG_IEEE80211BE */
	return false;
#endif /* CONFIG_IEEE80211BE */
}


static void handle_disassoc(struct hostapd_data *hapd,
			    const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.disassoc)) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			   "handle_disassoc - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}
	hostapd_ubus_notify(hapd, "disassoc", mgmt->sa);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Station " MACSTR
			" trying to disassociate, but it is not associated",
			MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_event_disassoc(hapd, sta,
			HOSTAPD_IF_DISCONNECT_FROM_STA,
			le_to_host16(mgmt->u.disassoc.reason_code), false, 0);
	hostapd_if_notify_disassoc(hapd, sta, mgmt, len);
#endif

	if (hostapd_ml_handle_disconnect(hapd, sta, mgmt, true))
		return;

	hostapd_disassoc_sta(hapd, sta, mgmt);
}


static void handle_deauth(struct hostapd_data *hapd,
			  const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.deauth)) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"handle_deauth - too short payload (len=%lu)",
			(unsigned long) len);
		return;
	}

	/* Clear the PTKSA cache entries for PASN */
	ptksa_cache_flush(hapd->ptksa, mgmt->sa, WPA_CIPHER_NONE);

	hostapd_ubus_notify(hapd, "deauth", mgmt->sa);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Station " MACSTR
			" trying to deauthenticate, but it is not authenticated",
			MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_HOSTAPD_IF
	/* FROM_STA deauthentication event */
	hostapd_if_event_deauth(hapd, sta, HOSTAPD_IF_DISCONNECT_FROM_STA,
			le_to_host16(mgmt->u.deauth.reason_code), false, 0);
	hostapd_if_notify_deauth(hapd, sta, mgmt, len);
#endif

	if (hostapd_ml_handle_disconnect(hapd, sta, mgmt, false))
		return;

	hostapd_deauth_sta(hapd, sta, mgmt);
}


static void handle_beacon(struct hostapd_data *hapd,
			  const struct ieee80211_mgmt *mgmt, size_t len,
			  struct hostapd_frame_info *fi)
{
	struct ieee802_11_elems elems;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.beacon)) {
		wpa_printf(MSG_INFO, "handle_beacon - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}

	(void) ieee802_11_parse_elems(mgmt->u.beacon.variable,
				      len - (IEEE80211_HDRLEN +
					     sizeof(mgmt->u.beacon)), &elems,
				      0);

	ap_list_process_beacon(hapd->iface, mgmt, &elems, fi);

#ifdef CONFIG_IEEE80211BN
	mapc_handle_neighbor_beacon(hapd, mgmt->sa);
#endif /* CONFIG_IEEE80211BN */

}

static void hostapd_dscp_action(struct hostapd_data *hapd,
				struct sta_info *sta,
				const u8 *pos, const u8 *end,
				bool protected)
{
	struct hostapd_data *assoc_hapd = hapd;
	struct sta_info *assoc_sta = sta;
	u8 subtype;

	if (end - pos < 1) {
		wpa_printf(MSG_DEBUG, "DSCP Action: Frame too short");
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_DEBUG,
				   "DSCP Action: Assoc STA not found");
			return;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	subtype = *pos++;
	switch (subtype) {
	case QM_DSCP_POLICY_QUERY:
		hostapd_handle_dscp_policy_query(assoc_hapd, assoc_sta, pos,
						 end - pos);
		break;
	case QM_DSCP_POLICY_RESP:
		hostapd_handle_dscp_policy_response(assoc_hapd, assoc_sta, pos,
						    end - pos);
		break;
	default:
		wpa_printf(MSG_DEBUG, "QM Action: Unknown subtype %u", subtype);
		break;
	}
}

static int hostapd_action_vs(struct hostapd_data *hapd,
			     struct sta_info *sta,
			     const struct ieee80211_mgmt *mgmt, size_t len,
			     unsigned int freq, bool protected)
{
	const u8 *pos, *end;
	u32 oui_type;

	pos = (const u8 *) &mgmt->u.action;
	end = ((const u8 *) mgmt) + len;

	if (end - pos < 1 + 4)
		return -1;
	pos++;

	oui_type = WPA_GET_BE32(pos);
	pos += 4;

	switch (oui_type) {
	case WFA_CAPAB_VENDOR_TYPE:
		hostapd_wfa_capab(hapd, sta, pos, end);
		return 0;
	case QM_ACTION_VENDOR_TYPE:
		if (!protected) {
			wpa_printf(MSG_ERROR, "DSCP: Ignoring unprotected frame");
			return -1;
		}
		hostapd_dscp_action(hapd, sta, pos, end, protected);
		return 0;
	default:
#ifdef CONFIG_QCN_EXTN
		if (handle_action_vs_extn(hapd, sta, mgmt, len, freq,
					  protected) == 0)
			return 0;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_DEBUG,
			   "Ignore unknown Vendor Specific Action frame OUI/type %08x%s",
			   oui_type, protected ? " (protected)" : "");
		break;
	}

	return -1;
}

#ifdef CONFIG_IEEE80211BN
static void handle_uhr_link_reconfig(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     const struct ieee80211_mgmt *mgmt,
				     size_t len, struct sta_smd_ctx_info *smd_ctx)
{
	const u8 *frame = (const u8 *) mgmt;
	u8 type = frame[IEEE80211_HDRLEN + 3];  /* Skip header + category + action + dialog */
	
	if (len < IEEE80211_HDRLEN + 4) {
		wpa_printf(MSG_DEBUG, "UHR: Frame too short");
		return;
	}
	
	switch (type) {
	case UHR_LINK_RECONFIG_TYPE_PREP:
		uhr_handle_st_prep_req(hapd, sta, (const u8 *) mgmt, len, smd_ctx);
		break;
	case UHR_LINK_RECONFIG_TYPE_EXECUTE:
		/*
		 * Distinguish ST Execute via Target AP-MLD from the normal
		 * IAP-forwarded path: in the via-target path the STA sends
		 * Execute directly to us, so the STA record is not yet
		 * authorized (ST Prep created it but did not authorize it).
		 */
		if (!(sta->flags & WLAN_STA_AUTHORIZED)) {
			wpa_printf(MSG_INFO,
				   "UHR: ST Execute received at Target AP "
				   "for " MACSTR " — initiating CTX fetch",
				   MAC2STR(sta->addr));
			uhr_handle_st_exec_req_tgt(hapd, sta,
						   (const u8 *) mgmt, len);
			return;
		}
		uhr_handle_st_exec_req(hapd, sta, (const u8 *) mgmt, len, smd_ctx);
		break;
	default:
		wpa_printf(MSG_DEBUG, "UHR: Received type not resolved");
	}
}


static void ieee802_11_rx_protected_uhr_action(struct hostapd_data *hapd,
					       const struct ieee80211_mgmt *mgmt,
					       size_t len,
					       struct sta_smd_ctx_info *smd_ctx)
{
	u8 action;
	struct sta_info *sta;

	action =  *((u8 *)&mgmt->u.action + 1);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "UHR: Action frame from unknown STA " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "UHR: Received action=%u ",action);

	switch (action) {
	case WLAN_PROT_UHR_LINK_RECONFIG_REQUEST:
		handle_uhr_link_reconfig(hapd, sta, mgmt, len, smd_ctx);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			"MLD: Unsupported Protected UHR Action %u from " MACSTR
			" discarded", action, MAC2STR(mgmt->sa));
		break;
	}
}


#endif /* CONFIG_IEEE80211BN */

static int robust_action_frame(u8 category)
{
	return category != WLAN_ACTION_PUBLIC &&
		category != WLAN_ACTION_HT &&
		category != WLAN_ACTION_UNPROTECTED_WNM &&
		category != WLAN_ACTION_SELF_PROTECTED &&
		category != WLAN_ACTION_UNPROTECTED_DMG &&
		category != WLAN_ACTION_VHT &&
		category != WLAN_ACTION_UNPROTECTED_S1G &&
		category != WLAN_ACTION_HE &&
		category != WLAN_ACTION_EHT &&
		category != WLAN_ACTION_VENDOR_SPECIFIC;
}


static int handle_action(struct hostapd_data *hapd,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 unsigned int freq
#ifdef CONFIG_QCN_EXTN
			 , const struct handle_action_extn_args *extn_args
#endif /* CONFIG_QCN_EXTN */
			 , struct sta_smd_ctx_info *smd_ctx
			 )
{
#ifdef CONFIG_HOSTAPD_IF
	int rssi = 0;
#endif

	struct sta_info *sta;
	u8 *action __maybe_unused;

	if (!hostapd_verify_action_frame_has_min_length(mgmt, len)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "handle_action - too short payload (len=%lu)",
			       (unsigned long) len);
		return 0;
	}

	action = (u8 *) &mgmt->u.action.u;
	wpa_printf(MSG_DEBUG, "RX_ACTION category %u action %u sa " MACSTR
		   " da " MACSTR " len %d freq %u",
		   mgmt->u.action.category, *action,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), (int) len, freq);

	sta = ap_get_sta(hapd, mgmt->sa);

	if (mgmt->u.action.category != WLAN_ACTION_PUBLIC &&
	    (sta == NULL || !(sta->flags & WLAN_STA_ASSOC))) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Ignored Action "
			   "frame (category=%u) from unassociated STA " MACSTR,
			   mgmt->u.action.category, MAC2STR(mgmt->sa));
		return 0;
	}

	if (sta && (sta->flags & WLAN_STA_MFP) &&
	    !(mgmt->frame_control & host_to_le16(WLAN_FC_PROTECTED)) &&
	    robust_action_frame(mgmt->u.action.category)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Dropped unprotected Robust Action frame from "
			       "an MFP STA");
		return 0;
	}

	if (sta) {
		u16 fc = le_to_host16(mgmt->frame_control);
		u16 seq_ctrl = le_to_host16(mgmt->seq_ctrl);

		if ((fc & WLAN_FC_RETRY) &&
		    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
		    sta->last_seq_ctrl == seq_ctrl &&
		    sta->last_subtype == WLAN_FC_STYPE_ACTION) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Drop repeated action frame seq_ctrl=0x%x",
				       seq_ctrl);
			return 1;
		}

		sta->last_seq_ctrl = seq_ctrl;
		sta->last_subtype = WLAN_FC_STYPE_ACTION;
	}

#ifdef CONFIG_HOSTAPD_IF
#ifdef CONFIG_QCN_EXTN
	rssi = extn_args->rssi;
#endif
	if (hostapd_if_notify_action(hapd, sta, mgmt, len, rssi) ==
	    HOSTAPD_IF_FRAME_PROCESSING_OFFLOAD)
		return 1;
#endif

	switch (mgmt->u.action.category) {
#ifdef CONFIG_IEEE80211R_AP
	case WLAN_ACTION_FT:
		if (!sta ||
		    wpa_ft_action_rx(sta->wpa_sm, (u8 *) &mgmt->u.action,
				     mgmt->sa, hapd->own_addr, len - IEEE80211_HDRLEN))
			break;
		return 1;
#endif /* CONFIG_IEEE80211R_AP */
	case WLAN_ACTION_WMM:
		hostapd_wmm_action(hapd, mgmt, len);
		return 1;
	case WLAN_ACTION_SA_QUERY:
		ieee802_11_sa_query_action(hapd, mgmt, len);
		return 1;
#ifdef CONFIG_WNM_AP
	case WLAN_ACTION_WNM:
		ieee802_11_rx_wnm_action_ap(hapd, mgmt, len);
		return 1;
#endif /* CONFIG_WNM_AP */
#ifdef CONFIG_FST
	case WLAN_ACTION_FST:
		if (hapd->iface->fst)
			fst_rx_action(hapd->iface->fst, mgmt, len);
		else
			wpa_printf(MSG_DEBUG,
				   "FST: Ignore FST Action frame - no FST attached");
		return 1;
#endif /* CONFIG_FST */
	case WLAN_ACTION_PUBLIC:
	case WLAN_ACTION_PROTECTED_DUAL:
		if (len >= IEEE80211_HDRLEN + 2 &&
		    mgmt->u.action.u.public_action.action ==
		    WLAN_PA_20_40_BSS_COEX) {
#ifdef CONFIG_QCN_EXTN
			/*
			 * Skip 20/40 coex action frames from weak/distant
			 * stations whose signal level is below the configured
			 * obss_rx_snr_threshold.
			 */
			if (hostapd_2040_coex_action_snr_below_threshold_extn(
				    hapd, extn_args->rssi))
				return 1;
#endif /* CONFIG_QCN_EXTN */
			hostapd_2040_coex_action(hapd, mgmt, len);
			return 1;
		}
#ifdef CONFIG_DPP
		if (len >= IEEE80211_HDRLEN + 6 &&
		    mgmt->u.action.u.vs_public_action.action ==
		    WLAN_PA_VENDOR_SPECIFIC &&
		    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
		    OUI_WFA &&
		    mgmt->u.action.u.vs_public_action.variable[0] ==
		    DPP_OUI_TYPE) {
			const u8 *pos, *end;

			pos = mgmt->u.action.u.vs_public_action.oui;
			end = ((const u8 *) mgmt) + len;
			hostapd_dpp_rx_action(hapd, mgmt->sa, pos, end - pos,
					      freq);
			return 1;
		}
		if (len >= IEEE80211_HDRLEN + 2 &&
		    (mgmt->u.action.u.public_action.action ==
		     WLAN_PA_GAS_INITIAL_RESP ||
		     mgmt->u.action.u.public_action.action ==
		     WLAN_PA_GAS_COMEBACK_RESP)) {
			const u8 *pos, *end;

			pos = &mgmt->u.action.u.public_action.action;
			end = ((const u8 *) mgmt) + len;
			if (gas_query_ap_rx(hapd->gas, mgmt->sa,
					    mgmt->u.action.category,
					    pos, end - pos, freq) == 0)
				return 1;
		}
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
		if (mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
		    len >= IEEE80211_HDRLEN + 5 &&
		    mgmt->u.action.u.vs_public_action.action ==
		    WLAN_PA_VENDOR_SPECIFIC &&
		    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
		    OUI_WFA &&
		    mgmt->u.action.u.vs_public_action.variable[0] ==
		    NAN_OUI_TYPE) {
			const u8 *pos, *end;

			pos = mgmt->u.action.u.vs_public_action.variable;
			end = ((const u8 *) mgmt) + len;
			pos++;
			hostapd_nan_usd_rx_sdf(hapd, mgmt->sa, mgmt->bssid,
					       freq, pos, end - pos);
			return 1;
		}
#endif /* CONFIG_NAN_USD */
#ifdef CONFIG_IEEE80211BN
		if (mgmt->u.action.u.public_action.action ==
		    WLAN_PA_MAPC_DISCOVERY_REQ ||
		    mgmt->u.action.u.public_action.action ==
		    WLAN_PA_MAPC_DISCOVERY_RESP ||
                    mgmt->u.action.u.public_action.action ==
		    WLAN_PA_MAPC_NEGOTIATION_REQ ||
		    mgmt->u.action.u.public_action.action ==
		    WLAN_PA_MAPC_NEGOTIATION_RESP) {
			const u8 *pos, *end;
			pos = (const u8 *) &mgmt->u.action.category;
			end = ((const u8 *) mgmt) + len;
			hostapd_mapc_handle_action(hapd, mgmt->sa, pos, end - pos);
			return 1;
                }
#endif /* CONFIG_IEEE80211BN */
		if (hapd->public_action_cb) {
			hapd->public_action_cb(hapd->public_action_cb_ctx,
					       (u8 *) mgmt, len, freq);
		}
		if (hapd->public_action_cb2) {
			hapd->public_action_cb2(hapd->public_action_cb2_ctx,
						(u8 *) mgmt, len, freq);
		}
		if (hapd->public_action_cb || hapd->public_action_cb2)
			return 1;
		break;
	case WLAN_ACTION_VENDOR_SPECIFIC:
		if (hapd->vendor_action_cb) {
			if (hapd->vendor_action_cb(hapd->vendor_action_cb_ctx,
						   (u8 *) mgmt, len, freq) == 0)
				return 1;
		}
		if (sta &&
		    hostapd_action_vs(hapd, sta, mgmt, len, freq, false) == 0)
			return 1;
		break;
	case WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED:
		if (sta &&
		    hostapd_action_vs(hapd, sta, mgmt, len, freq, true) == 0)
			return 1;
		break;
#ifdef CONFIG_IEEE80211AX
	case WLAN_ACTION_ROBUST_AV_STREAMING:
		hostapd_handle_robust_av(hapd, (const u8 *)mgmt, len);
		return 1;
#endif /* CONFIG_IEEE80211AX */
#ifndef CONFIG_NO_RRM
	case WLAN_ACTION_RADIO_MEASUREMENT:
		hostapd_handle_radio_measurement(hapd, (const u8 *) mgmt, len);
		return 1;
#endif /* CONFIG_NO_RRM */
#ifdef CONFIG_IEEE80211BE
	case WLAN_ACTION_PROTECTED_EHT:
		ieee802_11_rx_protected_eht_action(hapd, sta, mgmt, len);
		return 1;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211BN
	case WLAN_ACTION_PROTECTED_UHR:
		wpa_printf(MSG_DEBUG, "Received protected UHR action frame");
		ieee802_11_rx_protected_uhr_action(hapd, mgmt, len, smd_ctx);
		return 1;
#endif /* CONFIG_IEEE80211BN */
	default:
#ifdef CONFIG_QCN_EXTN
		if (handle_action_extn(hapd, mgmt, len, freq))
			return 1;
#endif /* CONFIG_QCN_EXTN */
	}

	hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "handle_action - unknown action category %d or invalid "
		       "frame",
		       mgmt->u.action.category);

	return 1;
}


/**
 * notify_mgmt_frame - Notify of Management frames on the control interface
 * @hapd: hostapd BSS data structure (the BSS to which the Management frame was
 * sent to)
 * @buf: Management frame data (starting from the IEEE 802.11 header)
 * @len: Length of frame data in octets
 *
 * Notify the control interface of any received Management frame.
 */
static void notify_mgmt_frame(struct hostapd_data *hapd, const u8 *buf,
			      size_t len)
{

	int hex_len = len * 2 + 1;
	char *hex = os_malloc(hex_len);

	if (hex) {
		wpa_snprintf_hex(hex, hex_len, buf, len);
		wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO,
			     AP_MGMT_FRAME_RECEIVED "buf=%s", hex);
		os_free(hex);
	}
}


/**
 * ieee80211_clear_critical_flag - clear critical flags on mbssid profile and MLD links
 * @hapd: hostapd BSS data structure (the BSS to which the management frame was
 * sent to)
 *
 * Clear critical flags after sending probe /assoc response frame because driver
 * will update critical flags for each of these frames through NL80211_CMD_FRAME event
 */

static void ieee80211_clear_critical_flag(struct hostapd_data *hapd)
{
	struct hostapd_data *bss, *link_bss;
	size_t i;

	if (!hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */
	/*clear mbssid bss critical flags*/
	if (hapd->iconf->mbssid) {
		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];
			if (bss)
				bss->rx_cu_param.critical_flag  = 0;
		}
	} else {
		/*clear bss critical flag*/
		hapd->rx_cu_param.critical_flag  = 0;
	}

	/*clear MLO partner link bss critical flags*/
	for_each_mld_link(link_bss, hapd) {
		if (hapd == link_bss)
			continue;
		link_bss->rx_cu_param.critical_flag  = 0;
	}
}

#ifdef CONFIG_IEEE80211BN

/**
 * uhr_send_st_prep_response - Send ST Preparation Response
 * @hapd: hostapd data
 * @sta: Station info
 * @req_mgmt: Original request management frame
 * @req_len: Request frame length
 * Returns: 0 on success, -1 on error
 *
 * Sends an automatic ST Preparation Response with success status.
 */
int uhr_send_st_prep_response(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     const struct ieee80211_mgmt *req_mgmt,
				     size_t req_len)
{
	struct ieee80211_mgmt *mgmt;
	u8 *buf, *pos;
	size_t len;
	u8 dialog_token;
	int ret;

	if (!hapd || !sta || !req_mgmt) {
		wpa_printf(MSG_ERROR, "UHR ST Prep Response: Invalid parameters");
		return -1;
	}

	/* Extract dialog token from request */
	if (req_len < IEEE80211_HDRLEN + 4) {
		wpa_printf(MSG_ERROR, "UHR ST Prep Response: Request too short");
		return -1;
	}
	/* Dialog token is at offset 2 after category and action */
	dialog_token = *((u8 *)&req_mgmt->u.action + 2);

	wpa_printf(MSG_DEBUG,
		   "UHR ST Prep Response: Sending response to STA " MACSTR " (dialog_token=%u)",
		   MAC2STR(sta->addr), dialog_token);

	/* Calculate frame length with mandatory IEs */
	/* Basic frame: MAC header + action fields + status + type */
	/* category + action + dialog + count + (link_id + status) per link */
	len = IEEE80211_HDRLEN + 1 + 1 + 1 + 1 + 1 + 3; /* category + action + dialog + type + count + one link entry */

	/* Add SMD BSS Transition Parameters IE */
	/* IE Header (2) + Extension ID (1) + Target AID (2) + Status Code (2) + BA Info (1) */
	size_t smd_bss_ie_len = 2 + 1 + 2 + 2 + 1; /* SMD BSS Transition IE */
	len += smd_bss_ie_len;
	
	/* Add Basic Multi-Link Element (simplified) */
	/* IE Header (2) + Extension ID (1) + ML Control (2) + Common Info Length (1) + MLD MAC (6) */
	size_t basic_mle_len = 2 + 1 + 2 + 1 + 6; /* Basic MLE */
	len += basic_mle_len;

	wpa_printf(MSG_DEBUG,
		   "UHR ST Prep Response: Frame length=%zu (basic=%zu, smd_ie=%zu, mle=%zu)",
		   len, (size_t)(IEEE80211_HDRLEN + 6), smd_bss_ie_len, basic_mle_len);

	buf = os_zalloc(len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "UHR ST Prep Response: Memory allocation failed");
		return -1;
	}

	mgmt = (struct ieee80211_mgmt *) buf;

	/* Fill MAC header - use MLD addresses for proper encryption */
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->mld->mld_addr, ETH_ALEN);

	/* Fill action frame */
	pos = (u8 *) &mgmt->u.action;
	*pos++ = WLAN_ACTION_PROTECTED_UHR;
	*pos++ = 1; /* WLAN_PROTECTED_UHR_ACTION_LINK_RECONFIG_RES */
	*pos++ = dialog_token;
	*pos++ = 0; /* Type: 0 = ST Preparation */
	*pos++ = 1; /* Count: number of link status entries */

	/* Add link status entry for the current link */
	u8 link_id = hapd->mld_link_id;
	*pos++ = link_id;
	WPA_PUT_LE16(pos, WLAN_STATUS_SUCCESS); /* Status code for this link */
	pos += 2;

	wpa_printf(MSG_DEBUG, "UHR ST Prep Response: Added link status (link_id=%u, status=SUCCESS)", link_id);

	/*
	 * Append mandatory IEs expected by STA-side parsing:
	 *  - SMD BSS Transition Parameters IE (Extension element, Ext ID = WLAN_EID_EXT_SMD)
	 *  - Basic Multi-Link element (Extension element, Ext ID = WLAN_EID_EXT_MULTI_LINK)
	 *
	 * Note: len was already calculated to include these IEs.
	 */
	{
		u16 target_aid = sta->aid ? sta->aid : 1;
		u16 bss_trans_status = WLAN_STATUS_SUCCESS;
		u8 ba_info = 0x00;

		/* SMD BSS Transition Parameters IE */
		*pos++ = WLAN_EID_EXTENSION;
		*pos++ = 1 + 2 + 2 + 1; /* ExtID + TargetAID + Status + BAInfo */
		*pos++ = WLAN_EID_EXT_SMD;
		WPA_PUT_LE16(pos, target_aid);
		pos += 2;
		WPA_PUT_LE16(pos, bss_trans_status);
		pos += 2;
		*pos++ = ba_info;

		/* Basic Multi-Link element (minimal common info: MLD MAC only) */
		*pos++ = WLAN_EID_EXTENSION;
		*pos++ = 1 + 2 + 1 + ETH_ALEN; /* ExtID + ML Control + CommonLen + MLD MAC */
		*pos++ = WLAN_EID_EXT_MULTI_LINK;

		/* ML Control: Basic (type=0) and no presence bitmap */
		WPA_PUT_LE16(pos, 0x0000);
		pos += 2;

		/* Common Info Length + Common Info (MLD MAC only in this minimal form) */
		*pos++ = ETH_ALEN;
		os_memcpy(pos, hapd->own_addr, ETH_ALEN);
		pos += ETH_ALEN;

		wpa_printf(MSG_DEBUG,
			   "UHR ST Prep Response: Appended SMD BSS Trans IE (AID=%u) + Basic MLE (MLD=" MACSTR ")",
			   target_aid, MAC2STR(hapd->own_addr));
	}

	wpa_printf(MSG_DEBUG,
		   "UHR ST Prep Response: Sending frame (len=%zu)", len);
	wpa_printf(MSG_DEBUG,
		   "UHR ST Prep Response: Frame will be encrypted by driver (Protected UHR Action, category=%u)",
		   WLAN_ACTION_PROTECTED_UHR);

	/* Send the frame - no_encrypt=0 allows driver to encrypt Protected UHR frames */
	ret = hostapd_drv_send_mlme(hapd, buf, len, 0, NULL, 0, 0, 0, 0);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR ST Prep Response: Failed to send frame");
	}

	os_free(buf);
	return ret;
}

#endif /* CONFIG_IEEE80211BN */


/**
 * ieee802_11_mgmt - process incoming IEEE 802.11 management frames
 * @hapd: hostapd BSS data structure (the BSS to which the management frame was
 * sent to)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @fi: meta data about received frame (signal level, etc.)
 *
 * Process all incoming IEEE 802.11 management frames. This will be called for
 * each frame received from the kernel driver through wlan#ap interface. In
 * addition, it can be called to re-inserted pending frames (e.g., when using
 * external RADIUS server as an MAC ACL).
 */
int ieee802_11_mgmt(struct hostapd_data *hapd, const u8 *buf, size_t len,
		    struct hostapd_frame_info *fi)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	int ret = 0;
	unsigned int freq;
	int ssi_signal = fi ? fi->ssi_signal : 0;
#ifdef CONFIG_NAN_USD
	static const u8 nan_network_id[ETH_ALEN] =
		{ 0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00 };
	static const u8 p2p_network_id[ETH_ALEN] =
		{ 0x51, 0x6f, 0x9a, 0x02, 0x00, 0x00 };
#endif /* CONFIG_NAN_USD */
	struct sta_smd_ctx_info *smd_ctx = fi ? fi->smd_ctx : NULL;

	if (len < 24)
		return 0;

	if (fi && fi->freq)
		freq = fi->freq;
	else
		freq = hapd->iface->freq;

	mgmt = (struct ieee80211_mgmt *) buf;
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (is_multicast_ether_addr(mgmt->sa) ||
	    is_zero_ether_addr(mgmt->sa) ||
	    ether_addr_equal(mgmt->sa, hapd->own_addr)) {
		/* Do not process any frames with unexpected/invalid SA so that
		 * we do not add any state for unexpected STA addresses or end
		 * up sending out frames to unexpected destination. */
		wpa_printf(MSG_DEBUG, "MGMT: Invalid SA=" MACSTR
			   " in received frame - ignore this frame silently",
			   MAC2STR(mgmt->sa));
		return 0;
	}

	if (stype == WLAN_FC_STYPE_BEACON) {
		handle_beacon(hapd, mgmt, len, fi);
		return 1;
	}

	if (fi && fi->datarate) {
		struct sta_info *rx_sta = ap_get_sta(hapd, mgmt->sa);
		if (rx_sta)
			rx_sta->last_rx_mgmt_rate = fi->datarate;
	}

	if (!is_broadcast_ether_addr(mgmt->bssid) &&
#ifdef CONFIG_NAN_USD
	    !nan_de_is_nan_network_id(mgmt->bssid) &&
	    !nan_de_is_p2p_network_id(mgmt->bssid) &&
#endif /* CONFIG_NAN_USD */
#ifdef CONFIG_P2P
	    /* Invitation responses can be sent with the peer MAC as BSSID */
	    !((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	      stype == WLAN_FC_STYPE_ACTION) &&
#endif /* CONFIG_P2P */
#ifdef CONFIG_MESH
	    !(hapd->conf->mesh & MESH_ENABLED) &&
#endif /* CONFIG_MESH */
#ifdef CONFIG_IEEE80211BE
	    !(hapd->conf->mld_ap &&
#ifdef CONFIG_QCN_EXTN
	      !hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
#endif /* CONFIG_QCN_EXTN */
	      ether_addr_equal(hapd->mld->mld_addr, mgmt->bssid)) &&
#endif /* CONFIG_IEEE80211BE */
	    !ether_addr_equal(mgmt->bssid, hapd->own_addr)
#ifdef CONFIG_IEEE80211BN
	    && !(stype == WLAN_FC_STYPE_ACTION &&
		 hostapd_is_mapc_action(mgmt, len))
#endif /* CONFIG_IEEE80211BN */
	    ) {
		wpa_printf(MSG_INFO, "MGMT: BSSID=" MACSTR " not our address",
			   MAC2STR(mgmt->bssid));
		return 0;
	}

	if (hapd->iface->state != HAPD_IFACE_ENABLED) {
		wpa_printf(MSG_DEBUG, "MGMT: Ignore management frame while interface is not enabled (SA=" MACSTR " DA=" MACSTR " subtype=%u)",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), stype);
		return 1;
	}

	if (stype == WLAN_FC_STYPE_PROBE_REQ) {
		handle_probe_req(hapd, mgmt, len, fi);
		ieee80211_clear_critical_flag(hapd);
		return 1;
	}

	if ((!is_broadcast_ether_addr(mgmt->da) ||
	     stype != WLAN_FC_STYPE_ACTION) &&
#ifdef CONFIG_IEEE80211BE
	    !(hapd->conf->mld_ap &&
#ifdef CONFIG_QCN_EXTN
	      !hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
#endif /* CONFIG_QCN_EXTN */
	      ether_addr_equal(hapd->mld->mld_addr, mgmt->bssid)) &&
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_NAN_USD
	    !ether_addr_equal(mgmt->da, nan_network_id) &&
	    !ether_addr_equal(mgmt->da, p2p_network_id) &&
#endif /* CONFIG_NAN_USD */
	    !ether_addr_equal(mgmt->da, hapd->own_addr)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "MGMT: DA=" MACSTR " not our address",
			       MAC2STR(mgmt->da));
		return 0;
	}

	if (hapd->iconf->track_sta_max_num)
		sta_track_add(hapd->iface, mgmt->sa, ssi_signal);

	if (hapd->conf->notify_mgmt_frames)
		notify_mgmt_frame(hapd, buf, len);

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth");
		handle_auth(hapd, mgmt, len, ssi_signal, 0);
		ret = 1;
		break;
	case WLAN_FC_STYPE_ASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_req");
		handle_assoc(hapd, mgmt, len, 0, ssi_signal);
		ieee80211_clear_critical_flag(hapd);
		ret = 1;
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_req");
		handle_assoc(hapd, mgmt, len, 1, ssi_signal);
		ieee80211_clear_critical_flag(hapd);
		ret = 1;
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wpa_printf(MSG_DEBUG, "mgmt::disassoc");
		handle_disassoc(hapd, mgmt, len);
		ret = 1;
		break;
	case WLAN_FC_STYPE_DEAUTH:
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "mgmt::deauth");
		handle_deauth(hapd, mgmt, len);
		ret = 1;
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action");
#ifdef CONFIG_QCN_EXTN
		{
			struct handle_action_extn_args extn_args = {
				.rssi = ssi_signal,
			};
			ret = handle_action(hapd, mgmt, len, freq, &extn_args, smd_ctx);
		}
#else
		ret = handle_action(hapd, mgmt, len, freq, smd_ctx);
#endif /* CONFIG_QCN_EXTN */
		break;
	default:
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "unknown mgmt frame subtype %d", stype);
		break;
	}

	return ret;
}


static void handle_auth_cb(struct hostapd_data *hapd,
			   const struct ieee80211_mgmt *mgmt,
			   size_t len, int ok)
{
	u16 auth_alg, auth_transaction, status_code;
	struct sta_info *sta;
	bool success_status;

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		sta = ap_get_link_sta(hapd, mgmt->da);
		if (!sta) {
			sta = ap_get_unadded_sta(hapd, mgmt->da);
			if (!sta) {
				wpa_printf(MSG_DEBUG, "handle_auth_cb: sta " MACSTR
						" not found",
						MAC2STR(mgmt->da));
				return;
			}
		} else {
			wpa_printf(MSG_DEBUG, "handle_auth_cb: STA " MACSTR
				   " identified as link STA", MAC2STR(mgmt->da));
		}
	}

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		wpa_printf(MSG_INFO, "handle_auth_cb - too short payload (len=%lu)",
			   (unsigned long) len);
		auth_alg = 0;
		auth_transaction = 0;
		status_code = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_event_auth_tx_complete(hapd, sta->addr);
#endif
	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "did not acknowledge authentication response");
		goto fail;
	}

	if (status_code == WLAN_STATUS_SUCCESS &&
	    ((auth_alg == WLAN_AUTH_OPEN && auth_transaction == 2) ||
	     (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 4))) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "authenticated");
		sta->flags |= WLAN_STA_AUTH;
		if (sta->added_unassoc)
			hostapd_set_sta_flags(hapd, sta);
		return;
	}

fail:
	success_status = status_code == WLAN_STATUS_SUCCESS;
#ifdef CONFIG_SAE
	if (auth_alg == WLAN_AUTH_SAE &&
	    auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT)
		success_status = sae_status_success(hapd, status_code);
#endif /* CONFIG_SAE */
#ifdef CONFIG_IEEE8021X_AUTH
	if (auth_alg == WLAN_AUTH_802_1X &&
	    status_code == WLAN_STATUS_802_1_X_AUTH_SUCCESS) {
		sta->flags |= WLAN_STA_AUTH;
		sta->auth_alg = WLAN_AUTH_802_1X;
		success_status = true;
	}
#endif  /* CONFIG_IEEE8021X_AUTH */
	if (!success_status && sta->added_unassoc) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


static void hostapd_set_wds_encryption(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       char *ifname_wds)
{
#ifdef CONFIG_WEP
	int i;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		return;

	for (i = 0; i < 4; i++) {
		if (ssid->wep.key[i] &&
		    hostapd_drv_set_key(ifname_wds, hapd, WPA_ALG_WEP, NULL, i,
					0, i == ssid->wep.idx, NULL, 0,
					ssid->wep.key[i], ssid->wep.len[i],
					i == ssid->wep.idx ?
					KEY_FLAG_GROUP_RX_TX_DEFAULT :
					KEY_FLAG_GROUP_RX_TX)) {
			wpa_printf(MSG_WARNING,
				   "Could not set WEP keys for WDS interface; %s",
				   ifname_wds);
			break;
		}
	}
#endif /* CONFIG_WEP */
}


#ifdef CONFIG_IEEE80211BE
static void ieee80211_ml_link_sta_assoc_cb(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   struct mld_link_info *link,
					   bool ok)
{
	bool updated = false;

	if (!ok) {
		hostapd_logger(hapd, link->peer_addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "did not acknowledge association response");
		sta->flags &= ~WLAN_STA_ASSOC_REQ_OK;

		/* The STA is added only in case of SUCCESS */
		if (link->status == WLAN_STATUS_SUCCESS)
			hostapd_drv_sta_remove(hapd, sta->addr);

		return;
	}

	if (link->status != WLAN_STATUS_SUCCESS)
		return;

	sta->flags |= WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;

	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa)
		updated = ap_sta_set_authorized_flag(hapd, sta, 1);

	hostapd_set_sta_flags(hapd, sta);
	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);

	/*
	 * TODOs:
	 * - IEEE 802.1X port enablement is not needed as done on the station
	 *     doing the connection.
	 * - Not handling accounting
	 * - Need to handle VLAN configuration
	 */
}
#endif /* CONFIG_IEEE80211BE */


static void hostapd_ml_handle_assoc_cb(struct hostapd_data *hapd,
				       struct sta_info *sta, bool ok)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *tmp_hapd;

	if (!hostapd_is_multiple_link_mld(hapd))
		return;

	for_each_mld_link(tmp_hapd, hapd) {
		struct mld_link_info *link;
		struct sta_info *tmp_sta;

		if (tmp_hapd == hapd)
			continue;

		link = &sta->mld_info.links[tmp_hapd->mld_link_id];
		if (!link->valid || link->rejected)
			continue;

		for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
		     tmp_sta = tmp_sta->next) {
			if (tmp_sta == sta ||
			    tmp_sta->mld_assoc_link_id !=
			    sta->mld_assoc_link_id ||
			    tmp_sta->aid != sta->aid)
				continue;

			/* To-do: WLAN_STA_AUTHORIZED flag to be set to partner
			 * links in other places as well.
			 */
			tmp_sta->flags |= (sta->flags & WLAN_STA_AUTHORIZED);
			tmp_sta->flags |= (sta->flags & WLAN_STA_FT_AUTH);

			ieee80211_ml_link_sta_assoc_cb(tmp_hapd, tmp_sta, link,
						       ok);
			break;
		}
	}
#endif /* CONFIG_IEEE80211BE */
}

static void set_sta_flag_to_partner_links(struct hostapd_data *hapd, struct
			     sta_info *sta)
{
	struct sta_info *psta;
	struct hostapd_data *phapd;
	u16 aid = sta->wds_mld_uid;

	if (ap_sta_is_mld(hapd, sta)) {
		for_each_mld_link(phapd, hapd) {
			if (phapd == hapd)
				continue;
			psta = ap_get_sta(phapd, sta->addr);
			if (psta) {
				psta->wds_mld_uid = aid;
				psta->flags = ((psta->flags & ~WLAN_STA_WDS) |
						(sta->flags & WLAN_STA_WDS));
				psta->flags = ((psta->flags & ~WLAN_STA_MULTI_AP) |
						(sta->flags & WLAN_STA_MULTI_AP));
			}
		}
	}
}


/**
 * hostapd_set_sta_flag_to_partner_links - Propagate STA flags to partner links
 * This is a non-static wrapper over set_sta_flag_to_partner_links(), exposed
 * for use by qcn_extns modules.
 * The function propagates relevant STA flags (e.g., WDS, Multi-AP) from the
 * given station context to all corresponding partner links, ensuring
 * consistency across multi-link or multi-BSS configurations.
 *
 * @hapd: Pointer to the hostapd BSS context
 * @sta: Station context for which WDS/Multi-AP flags need to be propagate
 */
void hostapd_set_sta_flag_to_partner_links(struct hostapd_data *hapd,
					   struct sta_info *sta)
{
	set_sta_flag_to_partner_links(hapd, sta);
}

static void handle_assoc_cb(struct hostapd_data *hapd,
			    const struct ieee80211_mgmt *mgmt,
			    size_t len, int reassoc, int ok)
{
	u16 status;
	struct sta_info *sta;
	int new_assoc = 1;

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		sta = ap_get_link_sta(hapd, mgmt->da);
		if (!sta) {
			sta = ap_get_unadded_sta(hapd, mgmt->da);
			if (!sta) {
				wpa_printf(MSG_INFO, "handle_assoc_cb: STA " MACSTR " not found",
					   MAC2STR(mgmt->da));
				return;
			}
		} else {
			wpa_printf(MSG_DEBUG, "handle_assoc_cb: STA " MACSTR
				   " identified as link STA", MAC2STR(mgmt->da));
		}
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    hapd->mld_link_id != sta->mld_assoc_link_id) {
		/* See ieee80211_ml_link_sta_assoc_cb() for the MLD case */
		wpa_printf(MSG_DEBUG,
			   "%s: MLD: ignore on link station (%d != %d)",
			   __func__, hapd->mld_link_id, sta->mld_assoc_link_id);
		return;
	}
#endif /* CONFIG_IEEE80211BE */

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_resp) :
				      sizeof(mgmt->u.assoc_resp))) {
		wpa_printf(MSG_INFO,
			   "handle_assoc_cb(reassoc=%d) - too short payload (len=%lu)",
			   reassoc, (unsigned long) len);
		hostapd_drv_sta_remove(hapd, sta->addr);
		return;
	}

	if (reassoc)
		status = le_to_host16(mgmt->u.reassoc_resp.status_code);
	else
		status = le_to_host16(mgmt->u.assoc_resp.status_code);
#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_event_assoc_tx_complete(hapd, sta->addr, ok, status,
					   sta->aid);
#endif

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "did not acknowledge association response");
		sta->flags &= ~WLAN_STA_ASSOC_REQ_OK;
		/* The STA is added only in case of SUCCESS */
		if (status == WLAN_STATUS_SUCCESS) {
			hostapd_drv_sta_remove(hapd, sta->addr);
			sta->added_unassoc = 0;
		}

		ap_sta_reset_assoc_req_rx_times(sta);

		goto handle_ml;
	}

	if (status != WLAN_STATUS_SUCCESS)
		goto handle_ml;

	/* Stop previous accounting session, if one is started, and allocate
	 * new session id for the new session. */
	accounting_sta_stop(hapd, sta);

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "associated (aid %d)",
		       sta->aid);

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
	if (is_mu_cap_war_active(hapd) && is_sta_vht_only(sta))
		hostapd_mu_cap_war_mu_state_changed_extn(hapd);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */

	if (sta->flags & WLAN_STA_ASSOC)
		new_assoc = 0;
	sta->flags |= WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;
	if ((!hapd->conf->ieee802_1x && !hapd->conf->wpa) ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK ||
	    sta->auth_alg == WLAN_AUTH_FT ||
	    sta->auth_alg == WLAN_AUTH_EPPKE ||
	    sta->auth_alg == WLAN_AUTH_802_1X) {
		/*
		 * Open, static WEP, FT protocol, or FILS; no separate
		 * authorization step.
		 */
		ap_sta_set_authorized(hapd, sta, 1);
	}

	if (sta->auth_alg == WLAN_AUTH_FT) {
		sta->ft_re_add = false;
#ifdef CONFIG_IEEE80211R_AP
		if (reassoc)
			wpa_ft_push_roam_notification(hapd->wpa_auth, sta->addr);
#endif
	}

	if (reassoc)
		mlme_reassociate_indication(hapd, sta);
	else
		mlme_associate_indication(hapd, sta);

	ap_sta_set_sa_query_timeout(hapd, sta, 0);

#ifdef CONFIG_PMKSA_PRIVACY
	if (ok && status == WLAN_STATUS_SUCCESS && sta->epp_sta &&
	    wpa_auth_ap_sta_support_pmkid_privacy(sta->wpa_sm)) {
		bool is_ml = ap_sta_is_mld(hapd, sta);
		struct rsn_pmksa_cache_entry *entry, *next;
		struct rsn_pmksa_cache *pmksa, *t_pmksa;
		const u8 *pmkid_cur, *pmkid_next;

		switch (sta->auth_alg) {
		case WLAN_AUTH_EPPKE:
			if (!sta->pasn) {
				wpa_printf(MSG_INFO, "EPP: Missing PASN data");
				goto skip_update;
			}
			pmkid_cur = sta->pasn->epp_pmkid_cur;
			pmkid_next = sta->epp_pmkid_next;
			break;
#ifdef CONFIG_IEEE8021X_AUTH
		case WLAN_AUTH_802_1X:
			pmkid_cur = sta->eap_auth_data.epp_pmkid_cur;
			pmkid_next = sta->eap_auth_data.epp_pmkid_next;
			break;
#endif /* CONFIG_IEEE8021X_AUTH */
		default:
			wpa_printf(MSG_INFO,
				   "EPP: Unsupported auth alg %u for PMKID privacy support",
				   sta->auth_alg);
			goto skip_update;
		}

		pmksa = t_pmksa = wpa_auth_get_pmksa_cache(hapd->wpa_auth,
							   is_ml);

		entry = pmksa_cache_auth_get(t_pmksa, NULL, pmkid_cur);
		if (entry)
			goto update_pmksa_entry;

#ifdef CONFIG_IEEE80211BE
		if (!entry && is_ml) {
			struct hostapd_data *tmp_hapd;

			/* Search in link caches of each AP MLD link */
			for_each_mld_link(tmp_hapd, hapd) {
				t_pmksa = wpa_auth_get_pmksa_cache(
					tmp_hapd->wpa_auth, false);
				entry = pmksa_cache_auth_get(t_pmksa, NULL,
							     pmkid_cur);
				if (entry)
					break;
			}
		} else if (!entry && !is_ml && hapd->conf->mld_ap) {
			/* Search in the MLD cache */
			t_pmksa = wpa_auth_get_pmksa_cache(hapd->wpa_auth,
							   true);
			entry = pmksa_cache_auth_get(t_pmksa, NULL, pmkid_cur);
		}
#endif /* CONFIG_IEEE80211BE */

update_pmksa_entry:
		if (entry) {
			wpa_printf(MSG_DEBUG,
				   "EPP: PMKSA caching privacy on - update PMKSA cache entry");
			next = os_memdup(entry, sizeof(*entry));
			if (!next)
				goto skip_update;
			os_memcpy(next->pmkid, pmkid_next, PMKID_LEN);
			os_memcpy(next->spa, sta->addr, ETH_ALEN);
			next->vlan_desc = NULL;
			next->identity = NULL;
			next->dpp_pkhash = NULL;
			next->cui = NULL;
			pmksa_cache_from_eapol_data(next, sta->eapol_sm);
			pmksa_cache_free_entry(t_pmksa, entry);
			pmksa_cache_auth_add_entry(pmksa, next);
		}
	}
skip_update:
#endif /* CONFIG_PMKSA_PRIVACY */

	if (sta->eapol_sm == NULL) {
		/*
		 * This STA does not use RADIUS server for EAP authentication,
		 * so bind it to the selected VLAN interface now, since the
		 * interface selection is not going to change anymore.
		 */
		if (ap_sta_bind_vlan(hapd, sta) < 0)
			goto handle_ml;
	} else if (sta->vlan_id) {
		/* VLAN ID already set (e.g., by PMKSA caching), so bind STA */
		if (ap_sta_bind_vlan(hapd, sta) < 0)
			goto handle_ml;
	}

	hostapd_set_sta_flags(hapd, sta);
	ap_sta_set_sa_query_timeout(hapd, sta, 0);

	if (!(sta->flags & WLAN_STA_WDS) && sta->pending_wds_enable) {
		wpa_printf(MSG_DEBUG, "Enable 4-address WDS mode for STA "
			   MACSTR " based on pending request",
			   MAC2STR(sta->addr));
		sta->pending_wds_enable = 0;
		sta->flags |= WLAN_STA_WDS;
		set_sta_flag_to_partner_links(hapd, sta);
		if (hapd->conf->mld_ap &&
		    hostapd_get_wds_mld_sta_uid(hapd, sta) < 0) {
			wpa_printf(MSG_DEBUG, "No room for uid"
				   "to enable 4-address WDS mode for STA "
				   MACSTR, MAC2STR(sta->addr));
			return;
		}
	}

	/* WPS not supported on backhaul BSS. Disable 4addr mode on fronthaul */
	if (((sta->flags & WLAN_STA_WDS) && hapd->conf->wds_sta)||
	    (sta->flags & WLAN_STA_MULTI_AP &&
	     (hapd->conf->multi_ap & BACKHAUL_BSS) &&
	     !(sta->flags & WLAN_STA_WPS))) {
		int ret;
		char ifname_wds[IFNAMSIZ + 1];
		int aid;

		if (hapd->conf->mld_ap) {
			if (hostapd_get_wds_mld_sta_uid(hapd, sta) < 0) {
				wpa_printf(MSG_DEBUG, "No room for uid"
						"to enable 4-address WDS mode for STA "
						MACSTR, MAC2STR(sta->addr));
				return;
			}
			aid = sta->wds_mld_uid;
		} else {
			aid = sta->aid;
		}

#ifdef CONFIG_QCN_EXTN
		/*
		 * Stale WDS teardown: STA had a general-WDS interface from a
		 * prior association but has returned with WDS_IE enabled while
		 * this AP only supports general WDS (wds_ie=0). The mutual
		 * WDS_IE path will not run for this AP so the AP_VLAN must be
		 * torn down here rather than silently reused.
		 */
		if (sta->sta_extn.wds_ie_peer && !hapd->conf->bss_extn.wds_ie) {
			wpa_printf(MSG_INFO,
				   "WDS: %s - STA " MACSTR " returned with "
				   "WDS_IE but AP is not WDS_IE capable; "
				   "tearing down stale WDS interface (aid=%d)",
				   hapd->conf->iface, MAC2STR(sta->addr), aid);
			hostapd_set_wds_sta(hapd, NULL, sta->addr, aid, 0);
			hostapd_free_wds_mld_sta_uid(hapd, sta);
			sta->flags &= ~WLAN_STA_WDS;
			set_sta_flag_to_partner_links(hapd, sta);
		} else {
#endif /* CONFIG_QCN_EXTN */
		wpa_printf(MSG_DEBUG, "Reenable 4-address WDS mode for STA "
			   MACSTR " (aid %u)",
			   MAC2STR(sta->addr), aid);
		set_sta_flag_to_partner_links(hapd, sta);
		ret = hostapd_set_wds_sta(hapd, ifname_wds, sta->addr,
					  aid, 1);
		if (!ret)
			hostapd_set_wds_encryption(hapd, sta, ifname_wds);
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */
	}

	if (sta->auth_alg == WLAN_AUTH_FT)
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
	else
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hapd->new_assoc_sta_cb(hapd, sta, !new_assoc);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);

#ifdef CONFIG_FILS
	if ((sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK) &&
	    fils_set_tk(sta->wpa_sm) < 0) {
		wpa_printf(MSG_DEBUG, "FILS: TK configuration failed");
		ap_sta_disconnect(hapd, sta, sta->addr,
				  WLAN_REASON_UNSPECIFIED);
		return;
	}
#endif /* CONFIG_FILS */

	if (sta->pending_eapol_rx) {
		struct os_reltime now, age;

		os_get_reltime(&now);
		os_reltime_sub(&now, &sta->pending_eapol_rx->rx_time, &age);
		if (age.sec == 0 && age.usec < 200000) {
			wpa_printf(MSG_DEBUG,
				   "Process pending EAPOL frame that was received from " MACSTR " just before association notification",
				   MAC2STR(sta->addr));
			ieee802_1x_receive(
				hapd, mgmt->da,
				wpabuf_head(sta->pending_eapol_rx->buf),
				wpabuf_len(sta->pending_eapol_rx->buf),
				sta->pending_eapol_rx->encrypted);
		}
		wpabuf_free(sta->pending_eapol_rx->buf);
		os_free(sta->pending_eapol_rx);
		sta->pending_eapol_rx = NULL;
	}

	if (sta && hapd->conf->enable_dscp_policy_capa && sta->dscp_policy_capable) {
		int *policy_ids = NULL;
		size_t num_policies = 0;

		if (sta->num_dscp_policies > 0) {
			policy_ids = os_malloc(sizeof(int) * sta->num_dscp_policies);
			if (!policy_ids)
				return;

			for (size_t i = 0; i < sta->num_dscp_policies; i++)
				policy_ids[num_policies++] = sta->policies[i]->policy_id;

			hostapd_send_unsolicited_dscp_policy_request(hapd, sta, 0, policy_ids,
								     num_policies);
			os_free(policy_ids);
		} else {
			wpa_printf(MSG_DEBUG, "DSCP: No DSCP policies available");
		}
	}


#ifdef CONFIG_QCN_EXTN
	/* WDS IE: enable WDS mode if both AP and STA advertised WDS IE */
	handle_assoc_cb_wds_ie_extn(hapd, sta);
#endif /* CONFIG_QCN_EXTN */

handle_ml:
	hostapd_ml_handle_assoc_cb(hapd, sta, ok);
}


static void handle_deauth_cb(struct hostapd_data *hapd,
			     const struct ieee80211_mgmt *mgmt,
			     size_t len, int ok)
{
	struct sta_info *sta;
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_deauth_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}
	if (ok)
		wpa_printf(MSG_DEBUG, "STA " MACSTR " acknowledged deauth",
			   MAC2STR(sta->addr));
	else
		wpa_printf(MSG_DEBUG, "STA " MACSTR " did not acknowledge "
			   "deauth", MAC2STR(sta->addr));

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_event_deauth(hapd, sta, HOSTAPD_IF_DISCONNECT_TO_STA,
			le_to_host16(mgmt->u.deauth.reason_code), true, ok);
#endif
	ap_sta_deauth_cb(hapd, sta);
}


static void handle_disassoc_cb(struct hostapd_data *hapd,
			       const struct ieee80211_mgmt *mgmt,
			       size_t len, int ok)
{
	struct sta_info *sta;
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_disassoc_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}
	if (ok)
		wpa_printf(MSG_DEBUG, "STA " MACSTR " acknowledged disassoc",
			   MAC2STR(sta->addr));
	else
		wpa_printf(MSG_DEBUG, "STA " MACSTR " did not acknowledge "
			   "disassoc", MAC2STR(sta->addr));

#ifdef CONFIG_HOSTAPD_IF
	hostapd_if_event_disassoc(hapd, sta, HOSTAPD_IF_DISCONNECT_TO_STA,
				  le_to_host16(mgmt->u.disassoc.reason_code),
				  true, ok);
#endif
	ap_sta_disassoc_cb(hapd, sta);
}


static void handle_action_cb(struct hostapd_data *hapd,
			     const struct ieee80211_mgmt *mgmt,
			     size_t len, int ok)
{
	struct sta_info *sta;
#ifndef CONFIG_NO_RRM
	const struct rrm_measurement_report_element *report;
#endif /* CONFIG_NO_RRM */

#ifdef CONFIG_DPP
	if (len >= IEEE80211_HDRLEN + 6 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] ==
	    DPP_OUI_TYPE) {
		const u8 *pos, *end;

		pos = &mgmt->u.action.u.vs_public_action.variable[1];
		end = ((const u8 *) mgmt) + len;
		hostapd_dpp_tx_status(hapd, mgmt->da, pos, end - pos, ok);
		return;
	}
	if (len >= IEEE80211_HDRLEN + 2 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    (mgmt->u.action.u.public_action.action ==
	     WLAN_PA_GAS_INITIAL_REQ ||
	     mgmt->u.action.u.public_action.action ==
	     WLAN_PA_GAS_COMEBACK_REQ)) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.public_action.variable;
		end = ((const u8 *) mgmt) + len;
		gas_query_ap_tx_status(hapd->gas, mgmt->da, pos, end - pos, ok);
		return;
	}
#endif /* CONFIG_DPP */
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_action_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}

#ifdef CONFIG_HS20
	if (ok && len >= IEEE80211_HDRLEN + 2 &&
	    mgmt->u.action.category == WLAN_ACTION_WNM &&
	    mgmt->u.action.u.vs_public_action.action == WNM_NOTIFICATION_REQ &&
	    sta->hs20_deauth_on_ack) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Deauthenticate STA " MACSTR
			   " on acknowledging the WNM-Notification",
			   MAC2STR(sta->addr));
		ap_sta_session_timeout(hapd, sta, 0);
		return;
	}
#endif /* CONFIG_HS20 */

#ifdef CONFIG_IEEE80211BE
	/* Frame header (24B) + Category (1B) + Action code (1B) +
	 * Dialog token (1B) + Count (1B) + Status list (count * 3B)
	 */
	if (len >= IEEE80211_HDRLEN + 3 + 1 + 3 &&
	    mgmt->u.action.category == WLAN_ACTION_PROTECTED_EHT &&
	    mgmt->u.action.u.link_reconf_resp.action ==
	    WLAN_PROT_EHT_LINK_RECONFIG_RESPONSE) {
		hostapd_link_reconf_resp_tx_status(hapd, sta, mgmt, len, ok);
		return;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BE
	if (mgmt->u.action.category == WLAN_ACTION_PROTECTED_EHT &&
	    mgmt->u.action.u.ttlm_resp.action == WLAN_PROT_EHT_T2L_MAPPING_RESPONSE)
		hostapd_ttlm_resp_tx_status(hapd, sta, ok);

	if (mgmt->u.action.category == WLAN_ACTION_PROTECTED_EHT &&
	    mgmt->u.action.u.ttlm_teardown.action == WLAN_PROT_EHT_T2L_MAPPING_TEARDOWN)
		hostapd_ttlm_teardown_tx_status(hapd, sta, ok);
#endif /* CONFIG_IEEE80211BE */

#ifndef CONFIG_NO_RRM
	if (len < 24 + 5 + sizeof(*report))
		return;
	report = (const struct rrm_measurement_report_element *)
		&mgmt->u.action.u.rrm.variable[2];
	if (mgmt->u.action.category == WLAN_ACTION_RADIO_MEASUREMENT &&
	    mgmt->u.action.u.rrm.action == WLAN_RRM_RADIO_MEASUREMENT_REQUEST &&
	    report->eid == WLAN_EID_MEASURE_REQUEST &&
	    report->len >= 3 &&
	    report->type == MEASURE_TYPE_BEACON)
		hostapd_rrm_beacon_req_tx_status(hapd, mgmt, len, ok);
#endif /* CONFIG_NO_RRM */
}


/**
 * ieee802_11_mgmt_cb - Process management frame TX status callback
 * @hapd: hostapd BSS data structure (the BSS from which the management frame
 * was sent from)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @stype: management frame subtype from frame control field
 * @ok: Whether the frame was ACK'ed
 */
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, const u8 *buf, size_t len,
			u16 stype, int ok)
{
	const struct ieee80211_mgmt *mgmt;
	mgmt = (const struct ieee80211_mgmt *) buf;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_mgmt_frame_handling) {
		size_t hex_len = 2 * len + 1;
		char *hex = os_malloc(hex_len);

		if (hex) {
			wpa_snprintf_hex(hex, hex_len, buf, len);
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				"MGMT-TX-STATUS stype=%u ok=%d buf=%s",
				stype, ok, hex);
			os_free(hex);
		}
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth cb");
		handle_auth_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 0, ok);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 1, ok);
		break;
	case WLAN_FC_STYPE_PROBE_RESP:
		wpa_printf(MSG_EXCESSIVE, "mgmt::proberesp cb ok=%d", ok);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		wpa_printf(MSG_DEBUG, "mgmt::deauth cb");
		handle_deauth_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wpa_printf(MSG_DEBUG, "mgmt::disassoc cb");
		handle_disassoc_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action cb ok=%d", ok);
		handle_action_cb(hapd, mgmt, len, ok);
		break;
	default:
		wpa_printf(MSG_INFO, "unknown mgmt cb frame subtype %d", stype);
		break;
	}
}


int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}


int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen)
{
	int len = 0, ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "auth_alg=%d\n",
			  sta->auth_alg);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}


void hostapd_tx_status(struct hostapd_data *hapd, const u8 *addr,
		       const u8 *buf, size_t len, int ack)
{
	struct sta_info *sta;
	struct hostapd_iface *iface = hapd->iface;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL && iface->num_bss > 1) {
		size_t j;
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			sta = ap_get_sta(hapd, addr);
			if (sta)
				break;
		}
	}
	if (sta == NULL || !(sta->flags & WLAN_STA_ASSOC))
		return;
	if (sta->flags & WLAN_STA_PENDING_POLL) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " %s pending "
			   "activity poll", MAC2STR(sta->addr),
			   ack ? "ACKed" : "did not ACK");
		if (ack)
			sta->flags &= ~WLAN_STA_PENDING_POLL;
	}

	ieee802_1x_tx_status(hapd, sta, buf, len, ack);
}


void hostapd_client_poll_ok(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;
	struct hostapd_iface *iface = hapd->iface;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL && iface->num_bss > 1) {
		size_t j;
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			sta = ap_get_sta(hapd, addr);
			if (sta)
				break;
		}
	}
	if (sta == NULL)
		return;
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_STA_POLL_OK MACSTR,
		MAC2STR(sta->addr));
	if (!(sta->flags & WLAN_STA_PENDING_POLL))
		return;

	wpa_printf(MSG_DEBUG, "STA " MACSTR " ACKed pending "
		   "activity poll", MAC2STR(sta->addr));
	sta->flags &= ~WLAN_STA_PENDING_POLL;
}


void ieee802_11_rx_from_unknown(struct hostapd_data *hapd, const u8 *src,
				int wds)
{
	struct sta_info *sta;
	int aid = 0;

	sta = ap_get_sta(hapd, src);
	if (sta &&
	    ((sta->flags & WLAN_STA_ASSOC) ||
	     ((sta->flags & WLAN_STA_ASSOC_REQ_OK) && wds))) {
		if (!hapd->conf->wds_sta)
			return;

#ifdef CONFIG_QCN_EXTN
		if ((hapd->conf->bss_extn.wds_ie && !sta->sta_extn.wds_ie_peer) ||
		    (!hapd->conf->bss_extn.wds_ie && sta->sta_extn.wds_ie_peer)) {
			wpa_printf(MSG_DEBUG, "AP or Sta is not WDS_IE enabled peer\n");
			return;
		}
#endif /* CONFIG_QCN_EXTN */

		if (hapd->conf->mld_ap && wds) {
			if (hostapd_get_wds_mld_sta_uid(hapd, sta) < 0) {
				wpa_printf(MSG_DEBUG, "No room for uid"
					   "to enable 4-address WDS mode for STA "
					    MACSTR, MAC2STR(sta->addr));
				return;
			}
			aid = sta->wds_mld_uid;
		}
		else {
			aid = sta->aid;
		}

		if ((sta->flags & (WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK)) ==
		    WLAN_STA_ASSOC_REQ_OK) {
			wpa_printf(MSG_DEBUG,
				   "Postpone 4-address WDS mode enabling for STA "
				   MACSTR " since TX status for AssocResp is not yet known",
				   MAC2STR(sta->addr));
			sta->pending_wds_enable = 1;
			return;
		}

		if (wds && !(sta->flags & WLAN_STA_WDS)) {
			int ret;
			char ifname_wds[IFNAMSIZ + 1];

			wpa_printf(MSG_DEBUG, "Enable 4-address WDS mode for "
				   "STA " MACSTR " (aid %u)",
				   MAC2STR(sta->addr), aid);

			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO, "Enable 4-address WDS"
					"mode for STA with id %u flags 0x%x", aid, sta->flags);

			sta->flags |= WLAN_STA_WDS;
			set_sta_flag_to_partner_links(hapd, sta);
			ret = hostapd_set_wds_sta(hapd, ifname_wds,
						  sta->addr, aid, 1);
			if (!ret)
				hostapd_set_wds_encryption(hapd, sta,
							   ifname_wds);
		}
		return;
	}

	wpa_printf(MSG_DEBUG, "Data/PS-poll frame from not associated STA "
		   MACSTR, MAC2STR(src));
	if (is_multicast_ether_addr(src) || is_zero_ether_addr(src) ||
	    ether_addr_equal(src, hapd->own_addr)) {
		/* Broadcast bit set in SA or unexpected SA?! Ignore the frame
		 * silently. */
		return;
	}

	if (sta && (sta->flags & WLAN_STA_ASSOC_REQ_OK)) {
		wpa_printf(MSG_DEBUG, "Association Response to the STA has "
			   "already been sent, but no TX status yet known - "
			   "ignore Class 3 frame issue with " MACSTR,
			   MAC2STR(src));
		return;
	}

	if (sta && (sta->flags & WLAN_STA_AUTH))
		hostapd_drv_sta_disassoc(
			hapd, src,
			WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
	else
		hostapd_drv_sta_deauth(
			hapd, src,
			WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
}

static u8 *hostapd_add_tpe_info(u8 *eid, enum max_tx_pwr_interpretation tx_pwr_intrpn,
				u8 tx_pwr_count, s8 *tx_pwr_array,
				u8 tx_pwr_ext_count, s8 *tx_pwr_ext_array,
				u8 tx_pwr_cat)
{
	u8 *length, total_tx_pwr_count;
	int i;

	if (!tx_pwr_array || (tx_pwr_ext_count && !tx_pwr_ext_array))
		return eid;

	if (tx_pwr_intrpn == LOCAL_EIRP_PSD ||
	    tx_pwr_intrpn == REGULATORY_CLIENT_EIRP_PSD ||
	    tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD) {
		total_tx_pwr_count = tx_pwr_count ? 1 << (tx_pwr_count - 1) : 1;
	} else if (tx_pwr_intrpn == LOCAL_EIRP ||
		   tx_pwr_intrpn == REGULATORY_CLIENT_EIRP ||
		   tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP) {
		total_tx_pwr_count = tx_pwr_count + 1;
	} else {
		wpa_printf(MSG_ERROR, "Invalid tx power interpretation:%d", tx_pwr_intrpn);
		return eid;
	}

	/* Maximum Transmit Power field */
	*eid++ = WLAN_EID_TRANSMIT_POWER_ENVELOPE; /* Element ID */
	length = eid;
	*eid++ = 1 + total_tx_pwr_count; /* Length */

	/*
	 * Transmit Power Information field
	 *	bits 0-2 : Maximum Transmit Power Count
	 *	bits 3-5 : Maximum Transmit Power Interpretation
	 *	bits 6-7 : Maximum Transmit Power Category
	 */
	*eid++ = tx_pwr_count | (tx_pwr_intrpn << 3) | (tx_pwr_cat << 6);

	for (i = 0; i < total_tx_pwr_count; i++)
		*eid++ = tx_pwr_array[i];

#ifdef CONFIG_IEEE80211BE
	if (tx_pwr_intrpn == LOCAL_EIRP ||
	    tx_pwr_intrpn == REGULATORY_CLIENT_EIRP ||
	    tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP) {
		if (tx_pwr_ext_count) {
			if (tx_pwr_ext_count > MAX_EIRP_TPE_POWER_EXT_COUNT) {
				wpa_printf(MSG_WARNING, "Invalid EIRP tx power extension count:%d",
					   tx_pwr_ext_count);
				return eid;
			}
			*eid++ = tx_pwr_ext_array[0];
			*length += tx_pwr_ext_count;
		}
	} else if (tx_pwr_intrpn == LOCAL_EIRP_PSD ||
		   tx_pwr_intrpn == REGULATORY_CLIENT_EIRP_PSD ||
		   tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD) {
		if (tx_pwr_ext_count) {
			if (tx_pwr_ext_count > MAX_PSD_TPE_EXT_POWER_COUNT) {
				wpa_printf(MSG_WARNING, "Invalid PSD tx power extension count:%d",
					   tx_pwr_ext_count);
				return eid;
			}
			*eid++ = tx_pwr_ext_count;
			for (i = 0; i < tx_pwr_ext_count; i++)
				*eid++ = tx_pwr_ext_array[i];
			*length += 1 + tx_pwr_ext_count;
		}
	} else {
		wpa_printf(MSG_ERROR, "Invalid TPE power interpretation");
	}
#endif
	return eid;
}


int get_chan_list(struct hostapd_data *hapd, int *non_11be_start_idx,
		  int *chan_start_idx, int *non_11be_chan_count,
		  int *total_chan_count, struct ieee_chan_data chan_data)
{
	u8 seg0 = hostapd_get_oper_centr_freq_seg0_idx(hapd->iconf);
	u8 seg1 = hostapd_get_oper_centr_freq_seg1_idx(hapd->iconf);
	struct hostapd_iface *iface = hapd->iface;
	enum oper_chan_width chan_width = hostapd_get_oper_chwidth(iface->conf);
	int start_chan = -1, non11be_start_chan = -1, i = 0, res = -1;
	int sec_chan = iface->conf->secondary_channel;
	u8 pri_chan = iface->conf->channel;
	struct hostapd_channel_data *chan;

	switch (chan_width) {
	case CONF_OPER_CHWIDTH_320MHZ:
		non11be_start_chan = start_chan = seg0 - 30;
		*total_chan_count = 16;
		if (pri_chan > seg0)
			non11be_start_chan = seg0 + 2;
		*non_11be_chan_count = 8;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		non11be_start_chan = start_chan = seg0 - 14;
		*non_11be_chan_count = *total_chan_count = 8;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		non11be_start_chan = start_chan = seg0 - 6;
		*non_11be_chan_count = *total_chan_count = 4;
		break;
	case CONF_OPER_CHWIDTH_USE_HT:
		*non_11be_chan_count = *total_chan_count = 1;
		non11be_start_chan = start_chan = pri_chan;
		if (sec_chan) {
			if (sec_chan == -1)
				non11be_start_chan = start_chan = pri_chan - 4;
			*non_11be_chan_count = *total_chan_count = 2;
		}
		break;
	default:
		wpa_printf(MSG_ERROR, "unsupported BW :%d", chan_width);
		break;
	}

	if (hapd->iconf->punct_bitmap) {
		punct_update_legacy_bw(hapd->iconf->punct_bitmap, pri_chan,
				       &chan_width, &seg0, &seg1);
		switch (chan_width) {
		case CONF_OPER_CHWIDTH_160MHZ:
			non11be_start_chan = seg0 - 14;
			*non_11be_chan_count = 8;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			non11be_start_chan = seg0 - 6;
			*non_11be_chan_count = 4;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			if (!seg0 || (seg0 == pri_chan)) {
				non11be_start_chan = pri_chan;
				*non_11be_chan_count = 1;
			} else {
				non11be_start_chan = seg0 - 2;
				*non_11be_chan_count = 2;
			}
			break;
		default:
			wpa_printf(MSG_ERROR,
				   "unsupported BW for puncturing:%d",
				   chan_width);
			break;
		}
	}
	for (i = 0; i < chan_data.num_channels; i++) {
		chan = &chan_data.channels[i];
		if (chan->chan == non11be_start_chan) {
			*non_11be_start_idx = i;
			res++;
		}
		if (chan->chan == start_chan) {
			*chan_start_idx = i;
			res++;
		}
		if (res > 0)
			break;
	}
	if (res > 0)
		return 0;

	wpa_printf(MSG_ERROR, "Invalid channel and bw");
	return -1;
}

static u8 num_psd_values_to_psd_count(int n_chans)
{
	switch (n_chans) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 4:
		return 3;
	case 8:
		return 4;
	default:
		return 0;
	}
}

int set_ieee_order_chan_list(struct hostapd_hw_modes *mode,
			     struct ieee_chan_data *chan_data,
			     enum nl80211_regulatory_power_modes client_mode)
{
	static const int ieee_6g_chan[] =  {2,
					    1, 5, 9, 13, 17, 21, 25, 29,
					    33, 37, 41, 45, 49, 53, 57, 61,
					    65, 69, 73, 77, 81, 85, 89, 93,
					    97, 101, 105, 109, 113, 117, 121, 125,
					    129, 133, 137, 141, 145, 149, 153, 157,
					    161, 165, 169, 173, 177, 181, 185, 189,
					    193, 197, 201, 205, 209, 213, 217, 221,
					    225, 229, 233};
	struct hostapd_channel_data *channels =  NULL, tmp_chan;
	int i, j;
	int num_6ghz_chans = mode->channels_6ghz.num_channels_6ghz[client_mode];
	int chan_data_size = num_6ghz_chans * sizeof(struct hostapd_channel_data);
	struct hostapd_channel_data *channels_6ghz = mode->channels_6ghz.chans_6ghz[client_mode];
	int n_6g_arr_elems = ARRAY_SIZE(ieee_6g_chan);

	if (num_6ghz_chans == 0) {
		wpa_printf(MSG_ERROR, "Invalid num channels or chan data");
		return -1;
	}
	if (!channels_6ghz) {
		wpa_printf(MSG_WARNING, "Invalid channel data for 6GHz of client_mode: %d", client_mode);
		return -1;
	}

	channels = os_malloc(chan_data_size);
	if (channels ==  NULL) {
		wpa_printf(MSG_ERROR, "Failed to alloc memory");
		return -1;
	}

	os_memcpy(channels, channels_6ghz, chan_data_size);

	for (i = 0; i < MIN(num_6ghz_chans,n_6g_arr_elems); i++) {
		if (ieee_6g_chan[i] != channels[i].chan) {
			for (j = 0; j < num_6ghz_chans; j++) {
				if (ieee_6g_chan[i] == channels[j].chan) {
					os_memcpy(&tmp_chan, &channels[j],
						  sizeof(struct hostapd_channel_data));
					os_memcpy(&channels[j], &channels[i],
						  sizeof(struct hostapd_channel_data));
					os_memcpy(&channels[i], &tmp_chan,
						  sizeof(struct hostapd_channel_data));
				}
			}
		}
	}
	chan_data->channels = channels;
	chan_data->num_channels = num_6ghz_chans;

	wpa_printf(MSG_DEBUG, "Set IEEE 6GHz channel list with %d channels",
		   chan_data->num_channels);

	return 0;
}

static void free_ieee_ordered_chan_list(struct ieee_chan_data *chan_data)
{
	os_free(chan_data->channels);
}

static inline bool
hostapd_is_additional_tpe(enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
       return (tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD ||
               tx_pwr_intrpn == REGULATORY_CLIENT_ADDITIONAL_EIRP);
}

/**
 * get_mask_details - Derive puncture mask and weight based on bandwidth.
 * @bw: Bandwidth in MHz (e.g., 40, 80, 160).
 *
 * The mapping is as follows:
 *   - 40 MHz  → mask = 0x03  (2 bits)
 *   - 80 MHz  → mask = 0x0F  (4 bits)
 *   - 160 MHz → mask = 0xFF  (8 bits)
 *
 * For unsupported bandwidths, mask is 0.
 *
 * This function is typically used in puncture pattern calculations
 * where the mask is needed to extract or align subchannel information for a
 * given bandwidth.
 *
 * Return: A bitmask with the number of bits set equal to the number of 20 MHz
 * sub-channels.
 */
static u16 get_mask_details(u16 bw)
{
	/* nchans : number of 20 Mhz bands */
	u8 nchans;
	u16 mask;

	switch (bw) {
	case CHWIDTH_20:
	case CHWIDTH_40:
	case CHWIDTH_80:
	case CHWIDTH_160:
	case CHWIDTH_320:
		nchans = bw / CHWIDTH_20;
		break;
	default:
		nchans = 0;
		break;
	}

	mask = (1 << nchans) - 1;
	return mask;
}

/**
 * get_lower_bandwidth_puncture_pattern - Extract puncture pattern for a target
 * bandwidth.
 * @prifreq: Primary channel center frequency in MHz.
 * @cur_pat: Current puncture bitmap representing inactive sub-channels.
 * @cur_cenfreq: Center frequency of the current bandwidth in MHz.
 * @cur_bw: Current bandwidth in MHz.
 * @target_bw: Target bandwidth in MHz for which the puncture pattern is needed.
 *
 * This function computes the puncture bitmap for a lower target bandwidth
 * based on the current puncture pattern and channel configuration.
 *
 * It determines the location of the target bandwidth segment within the current
 * bandwidth by calculating the offset of the primary channel from the left-most
 * 20 MHz sub-channel. It then extracts the relevant bits from the current
 * puncture bitmap corresponding to the target bandwidth.
 *
 * An example of converting a 160MHz BW puncture pattern to a 80MHz BW puncture
 * pattern is shown below:

 * |-----------------|pu|--| (current pattern = 0b0100_0000 = 0x40)
 * |-----------|pf|--------| (primary channel = 49 Current bandwidth = 160)
 * |33|37|41|45|49|53|57|61|
 * |-----0-----|-----1-----| (location of target BW in the current bw = 1)

 *             |-----|pu|--| (target pattern = 0b0100 = 0x4)
 *             |pf|--------| (primary channel = 49 target bandwidth = 80)
 *             |49|53|57|61|
 *
 * Example (6 GHz band):
 *   - Current bandwidth: 160 MHz
 *   - Current center frequency: 6185 MHz (Channel 47)
 *   - Primary channel frequency: 6245 MHz (Channel 49)
 *   - Current puncture pattern: 0x40 (binary: 0b0100_0000)
 *
 *   Computation:
 *     start_20mhz_freq = 6185 - 80 + 10 = 6115 MHz (Channel 33)
 *     target_bw_loc_in_curbw = (6245 - 6115) / 80 = 130 / 80 = 1
 *     mask = 0xF (4-bit mask for 80 MHz)
 *     n_20chans_in_target_bw = 80 / 20 = 4
 *     nbits_to_right_shift = 1 * 4 = 4
 *     target_pat = (0x40 >> 4) & 0xF = 0x4
 *
 *   Result:
 *     Extracted 80 MHz puncture pattern = 0x4
 *
 * Return: Puncture bitmap representing inactive sub-channels for the given
 * target bandwidth.
 */

#ifndef CONFIG_QCN_EXTN
static
#endif
u16 get_lower_bandwidth_puncture_pattern(u16 prifreq, u16 cur_pat,
					 u16 cur_cenfreq, u16 cur_bw,
					 u16 target_bw)
{
	/* @start_20mhz_freq :- Center frequency of the left-most/first 20 MHz
	 * channel.
	 * @target_bw_loc_in_curbw :- Location of the target bandwidth in
	 * current bandwidth.
	 * Target bandwidth is where the primary channel is.
	 * If the target bandwidth is the left-most/first band, then its location
	 * is 0.
	 * If the target bandwidth is the second left-most channel, then its
	 * location is 1,
	 * and so on.
	 * @n_20chans_in_target_bw :- Number of 20 MHz channels in the target
	 * bandwidth.
	 * @nbits_to_right_shift :- Number of bits for the right shift.
	 * @target_pat :- Puncture pattern in the target bandwidth.
	 */
	u16 start_20mhz_freq;
	u8 target_bw_loc_in_curbw;
	u16 mask;
	u8 n_20chans_in_target_bw;
	u8 nbits_to_right_shift;
	u16 target_pat;

	if (cur_bw < target_bw)
		return (u16)0xFFFF;

	start_20mhz_freq = cur_cenfreq - (cur_bw / 2) + (CHWIDTH_20 / 2);
	target_bw_loc_in_curbw = (prifreq - start_20mhz_freq) / target_bw;

	n_20chans_in_target_bw = target_bw / CHWIDTH_20;
	nbits_to_right_shift = target_bw_loc_in_curbw * n_20chans_in_target_bw;
	mask = get_mask_details(target_bw);

	target_pat = (cur_pat >> nbits_to_right_shift) & mask;
	return target_pat;
}

/**
 * hostapd_get_eirp_arr_for_bw - Get EIRP array for the current operating
 * bandwidth.
 * @iface: hostapd interface data structure.
 * @freq: Primary channel frequency in MHz.
 * @cf_320: Center frequency for 320 MHz operation, if applicable.
 * @cur_bw: Current operating bandwidth in MHz.
 * @client_type: Client type for which EIRP is being calculated.
 * @max_eirp_arr: Output array to store maximum EIRP values for each
 * bandwidth.
 * @pwr_type: Power type for EIRP calculation.
 * @is_chan_punctured: Indicates if channel puncturing is enabled.
 */
static void
hostapd_get_eirp_arr_for_bw(struct hostapd_iface *iface, u16 freq,
			    u16 cf_320, u16 cur_bw, u8 client_type,
			    s8 *max_eirp_arr, u8 pwr_type,
			    bool is_chan_punctured)
{
	u16 bw;
	u8 i;
	u8 seg0 = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	u16 seg0_cfreq = ieee80211_chan_to_freq(NULL, iface->conf->op_class, seg0);

	for (i = 0, bw = 20; bw <= cur_bw; i++, bw *= 2) {
		u16 cen_freq, pp = PUNCTURE_NONE;
		bool is_punc_valid = false;

		cen_freq = hostapd_get_bonded_chan_center_freq(freq, bw,
							       cf_320, 0);
		u16 start_freq = (bw == 20) ? freq : cen_freq - (bw / 2) + 10;
		u16 pri_chan_pos = (freq - start_freq) / 20;

		if (is_chan_punctured) {
			pp = get_lower_bandwidth_puncture_pattern(freq,
								  iface->conf->punct_bitmap,
								  seg0_cfreq,
								  cur_bw, bw);
			is_punc_valid = is_punct_bitmap_valid(bw, pri_chan_pos, pp);
		}
		if (is_punc_valid || !is_chan_punctured) {
			max_eirp_arr[i] = hostapd_get_eirp_pwr(iface, freq, cen_freq,
							       bw, pp,
							       pwr_type,
							       true, client_type, true);
		} else {
			max_eirp_arr[i] = RNR_20_MHZ_PSD_NO_POWER;
		}
		wpa_printf(MSG_DEBUG, "EIRP for BW %d MHz, freq %d MHz, "
			   "center freq %d MHz is %d dBm, is_punc_valid %d, pp 0x%x, is_chan_punctured %d",
			   bw, freq, cen_freq, max_eirp_arr[i], is_punc_valid, pp, is_chan_punctured);
	}
}

void
hostapd_get_eirp_arr_for_6ghz(struct hostapd_iface *iface,
			      u16 freq,
			      u8 cen320,
			      enum chan_width chanwidth,
			      u8 client_type,
			      s8 *max_eirp_arr,
			      u8 pwr_mode,
			      enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	u16 bw, max_bw = channel_width_to_int(chanwidth);
	u8 i, op_class = iface->conf->op_class;
	u16 cf_320 = 0;
	u8 pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	bool is_chan_punctured = iface->conf->punct_bitmap;

	if (is_320_opclass(op_class))
		cf_320 = ieee80211_chan_to_freq(NULL, op_class, cen320);

	hostapd_get_eirp_arr_for_bw(iface, freq, cf_320, max_bw, client_type,
				    max_eirp_arr, pwr_type, is_chan_punctured);

	if (pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
	    !hostapd_is_additional_tpe(tx_pwr_intrpn)) {
		s8 max_eirp_arr_lpi[TPE_NUM_POWER_SUPP_IN_11BE] = {0};

		if (client_type == NL80211_REG_REGULAR_CLIENT_SP)
			client_type = NL80211_REG_REGULAR_CLIENT_LPI;
		else
			client_type = NL80211_REG_SUBORDINATE_CLIENT_LPI;
		hostapd_get_eirp_arr_for_bw(iface, freq, cf_320, max_bw,
					    client_type, max_eirp_arr_lpi,
					    NL80211_REG_NUM_POWER_MODES,
					    is_chan_punctured);
		for (i = 0, bw = 20; bw <= max_bw; i++, bw *= 2) {
			max_eirp_arr[i] = MAX(max_eirp_arr[i], max_eirp_arr_lpi[i]);
			wpa_printf(MSG_DEBUG, "Max EIRP for BW %d MHz, freq %d MHz, "
				   "center freq %d MHz is %d dBm",
				   bw, freq,
				   hostapd_get_bonded_chan_center_freq(freq, bw,
								       cf_320, 0),
				   max_eirp_arr[i]);
		}
	}
}

static u8 hostapd_get_num_pwr_levels(struct hostapd_config *iconf)
{
	u8 num_eirp_pwr_levels;

	switch (hostapd_get_oper_chwidth(iconf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iconf->secondary_channel == 0)
			num_eirp_pwr_levels = 1;
		else
			num_eirp_pwr_levels = 2;

		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		num_eirp_pwr_levels = 3;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
		num_eirp_pwr_levels = 4;
		break;
#ifdef CONFIG_IEEE80211BE
	case CONF_OPER_CHWIDTH_320MHZ:
		num_eirp_pwr_levels = 5;
		break;
#endif
	default:
		return 1;
	}

	return num_eirp_pwr_levels;
}

static int hostapd_assign_tx_pwr_count(struct hostapd_config *iconf,
				       u8 *tx_pwr_count)
{
	switch (hostapd_get_oper_chwidth(iconf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iconf->secondary_channel == 0) {
			/* Max Transmit Power count = 0 (20 MHz) */
			*tx_pwr_count = 0;
		} else {
			/* Max Transmit Power count = 1 (20, 40 MHz) */
			*tx_pwr_count = 1;
		}
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		/* Max Transmit Power count = 2 (20, 40, and 80 MHz) */
		*tx_pwr_count = 2;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
#ifdef CONFIG_IEEE80211BE
	case CONF_OPER_CHWIDTH_320MHZ:
#endif
		/* Max Transmit Power count = 3 (20, 40, 80, 160/80+80 MHz) */
		*tx_pwr_count = 3;
		break;
	default:
		*tx_pwr_count = 0;
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_IEEE80211BE
static void hostapd_fill_eirp_for_ext_tpe(struct hostapd_config *iconf,
					  s8 *tx_pwr_ext_array,
					  s8 *tx_pwr_array,
					  u8 *tx_pwr_ext_count,
					  u8 num_pwr_levels)
{
	if  (hostapd_get_oper_chwidth(iconf) == CONF_OPER_CHWIDTH_320MHZ)
		*tx_pwr_ext_count = 1;

	if (*tx_pwr_ext_count)
		tx_pwr_ext_array[0] = tx_pwr_array[num_pwr_levels - 1];
}
#else
static void hostapd_fill_eirp_for_ext_tpe(struct hostapd_config *iconf,
					  u8 *tx_pwr_ext_array,
					  s8 *tx_pwr_array,
					  u8 *tx_pwr_ext_count,
					  u8 num_pwr_levels)
{
}
#endif

static u8 *hostapd_add_eirp_tpe(struct hostapd_data *hapd, u8 client_type,
				u8 *eid, u8 tx_pwr_cat,
				enum max_tx_pwr_interpretation tx_pwr_intrpn,
				u8 pwr_mode)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	s8 tx_pwr_ext_array[TPE_NUM_EIRP_POWER_EXT_SUPPORTED] = {0};
	u8 cen320, tx_pwr_count = 0, tx_pwr_ext_count = 0, num_pwr_levels = 0;
	enum chan_width ch_width;
	s8 tx_pwr_array[TPE_NUM_POWER_SUPP_IN_11BE] = {0};
	u16 freq;

	if (hostapd_assign_tx_pwr_count(iconf, &tx_pwr_count)) {
		wpa_printf(MSG_DEBUG, "Error in fetching tx_pwr_count");
		return eid;
	}

	ch_width =
		hostapd_get_chan_width_from_oper_chan_width(iconf);
	num_pwr_levels = hostapd_get_num_pwr_levels(iconf);
	freq = ieee80211_chan_to_freq(NULL, iconf->op_class, iconf->channel);
	cen320 = hostapd_get_oper_centr_freq_seg0_idx(iconf);
	hostapd_get_eirp_arr_for_6ghz(iface,
				      freq,
				      cen320,
				      ch_width,
				      client_type,
				      tx_pwr_array,
				      pwr_mode,
				      tx_pwr_intrpn);
	hostapd_fill_eirp_for_ext_tpe(iconf, tx_pwr_ext_array, tx_pwr_array,
				      &tx_pwr_ext_count, num_pwr_levels);

	return hostapd_add_tpe_info(eid, tx_pwr_intrpn, tx_pwr_count,
				    tx_pwr_array, tx_pwr_ext_count,
				    tx_pwr_ext_array, tx_pwr_cat);
}

static s16
get_max_psd_for_composite_ap(struct hostapd_iface *iface,
			     u16 chan_freq, u8 ap_pwr_type,
			     u8 client_mode,
			     s16 sp_psd)
{
	s16 lpi_psd, max_psd;
	int ret;

	if (client_mode == NL80211_REG_REGULAR_CLIENT_SP)
		client_mode = NL80211_REG_REGULAR_CLIENT_LPI;
	else
		client_mode = NL80211_REG_SUBORDINATE_CLIENT_LPI;

	ret = hostapd_reg_get_psd_from_chan_list(iface,
						 chan_freq, chan_freq, CHWIDTH_20, 0,
						 ap_pwr_type, client_mode,
						 true, false, &lpi_psd);
	if (ret) {
	    wpa_printf(MSG_WARNING, "Failed to calculate reg PSD for channel %d",
		       chan_freq);
	    return CHAN_MIN_TX_POWER;
	}
	max_psd = MAX(sp_psd, lpi_psd);
	wpa_printf(MSG_DEBUG, "Composite AP channel PSD for %d MHz channel %d is %d dBm, sp_psd: %d, lpi_psd: %d",
		   20, chan_freq, max_psd, sp_psd, lpi_psd);
	return max_psd;
}

static s16
get_sp_psd_for_non_punctured_chan(struct hostapd_data *hapd,
				  u16 chan_freq,
				  u8 client_mode,
				  u8 pwr_mode,
				  enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 ap_pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	s16 sp_psd;
	s8 eirp_20mhz;

	eirp_20mhz = hostapd_get_eirp_pwr(iface, chan_freq, 0, CHWIDTH_20, 0,
					  ap_pwr_type, true, client_mode, true);
	if (eirp_20mhz == CHAN_MIN_TWICE_TX_POWER) {
		wpa_printf(MSG_DEBUG,
			   "Failed to calculate EIRP in TPE for channel %d",
			   chan_freq);
	    sp_psd = CHAN_MIN_TX_POWER;
	} else {
	    sp_psd = (eirp_20mhz - (CONV_20MHZ_EIRP_TO_PSD_IN_DBM * 2)) / 2;
	    wpa_printf(MSG_DEBUG, "SP channel PSD for %d MHz channel %d is %d dBm, eirp: %d",
		       20, chan_freq, sp_psd, eirp_20mhz);
	}

	if (pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
	    !hostapd_is_additional_tpe(tx_pwr_intrpn)) {
	    return get_max_psd_for_composite_ap(iface, chan_freq,
						ap_pwr_type, client_mode,
						sp_psd);
	}
	return sp_psd;
}

static s16 get_sp_psd_for_punctured_chan(struct hostapd_data *hapd,
					 u16 chan_freq,
					 u8 client_mode,
					 u8 pwr_mode,
					 enum max_tx_pwr_interpretation tx_pwr_intrpn,
					 s16 *primary_20_mhz_psd)
{
	struct hostapd_iface *iface = hapd->iface;
	s16 oobe_psd, reg_psd;
	u8 ap_pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	int ret;

	get_min_psd_values(iface->afc_rsp_info, chan_freq, chan_freq, 0,
			   CHWIDTH_20, &oobe_psd);
	if (oobe_psd == CHAN_MAX_TWICE_TX_POWER * PSD_SCALE) {
	    wpa_printf(MSG_WARNING, "Failed to calculate OOBE PSD in TPE");
	    return CHAN_MIN_TX_POWER;
	}

	ret = hostapd_reg_get_psd_from_chan_list(iface, chan_freq, chan_freq,
						 CHWIDTH_20, 0,
						 ap_pwr_type, client_mode,
						 true,
						 false, &reg_psd);
	if (ret) {
	    wpa_printf(MSG_WARNING, "Failed to calculate reg PSD for channel %d",
		       chan_freq);
	    reg_psd = CHAN_MIN_TX_POWER;
	}
	oobe_psd -= SP_AP_AND_CLIENT_POWER_DIFF_IN_SCALE;
	reg_psd *= PSD_SCALE;
	oobe_psd = MIN(oobe_psd, reg_psd);
	oobe_psd /= PSD_SCALE;
    	/* Primary 20 MHz PSD shouldnt consider the mandatory power difference
	 * of SP AP and client SP as per regulatory guidelines.
	 */
	if (chan_freq == iface->freq && primary_20_mhz_psd)
	    *primary_20_mhz_psd = oobe_psd + SP_AP_AND_CLIENT_POWER_DIFF;

	wpa_printf(MSG_DEBUG, "OOBE PSD for %d MHz channel %d is %d dBm, primary_20_mhz_psd = %d",
		   20, chan_freq, oobe_psd, *primary_20_mhz_psd);

	if (pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
	    !hostapd_is_additional_tpe(tx_pwr_intrpn)) {
	    return get_max_psd_for_composite_ap(iface, chan_freq,
						ap_pwr_type, client_mode,
						oobe_psd);
	}
	return oobe_psd;
}

static s8 get_psd_for_chan_idx(struct hostapd_data *hapd,
			       int non_11be_start_idx,
			       struct ieee_chan_data chan_data,
			       u8 client_mode,
			       u8 pwr_mode,
			       enum max_tx_pwr_interpretation tx_pwr_intrpn,
			       s16 *primary_20mhz_psd)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 ap_pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	u16 chan_freq;

	if (ap_pwr_type != HE_REG_INFO_6GHZ_AP_TYPE_SP) {
		bool is_psd = chan_data.channels[non_11be_start_idx].flag & HOSTAPD_CHAN_PSD;
		s8 reg_psd;

		if (is_psd) {
			reg_psd = chan_data.channels[non_11be_start_idx].psd_power;
		} else {
			s8 reg_eirp_pwr = chan_data.channels[non_11be_start_idx].eirp_power;

			reg_psd = reg_eirp_pwr - CONV_20MHZ_EIRP_TO_PSD_IN_DBM;
		}

		return reg_psd;
	}

	chan_freq = chan_data.channels[non_11be_start_idx].freq;
	if (!iface->conf->punct_bitmap)
		return get_sp_psd_for_non_punctured_chan(hapd, chan_freq,
							 client_mode, pwr_mode,
							 tx_pwr_intrpn);
	return get_sp_psd_for_punctured_chan(hapd, chan_freq, client_mode,
					     pwr_mode, tx_pwr_intrpn,
					     primary_20mhz_psd);
}

/**
 * get_psd_limit - Get the minimum PSD limit for a given frequency
 * @freq: Frequency for which the PSD limit is to be determined
 * @num_freq_obj: Number of frequency objects in the AFC response
 * @afc_freq_info: Pointer to the array of AFC frequency objects
 *
 * This function calculates the minimum PSD (Power Spectral Density) limit for
 * a given frequency by iterating through the AFC frequency objects. It returns
 * the minimum PSD limit found within the range of the frequency objects.
 *
 * Return: Minimum PSD limit for the given frequency, or INVALID_PSD if the
 * frequency is not found within the AFC frequency objects.
 */
static
s16 get_psd_limit(u16 freq, u8 num_freq_obj, struct afc_freq_obj *afc_freq_info)
{
	u8 i;
	s16 min_psd = CHAN_MAX_PSD_POWER * EIRP_PWR_SCALE;
	bool chan_freq_found = false;

	for (i = 0; i < num_freq_obj; i++) {
		if (freq >= afc_freq_info[i].low_freq &&
		    freq <= afc_freq_info[i].high_freq) {
			chan_freq_found = true;
			if (afc_freq_info[i].max_psd < min_psd)
				min_psd = afc_freq_info[i].max_psd;

			/* Even though the frequency object is found, there may
			 * be more matching frequency-object following it.
			 * Continue search until the input frequency is out of
			 * range.
			 */
			continue;
		}

		/* Assuming AFC payload is sorted in increasing order of
		 * frequencies, stop and return here
		 */
		if (chan_freq_found)
			return min_psd;
	}

	/* Handle for last frequency object */
	if (chan_freq_found)
		return min_psd;

	return INVALID_PSD;
}

/**
 * get_y_val - Calculate the interpolated y-value for a given x-value
 * @x1: First x-coordinate
 * @x2: Second x-coordinate
 * @y1: y-coordinate corresponding to x1
 * @y2: y-coordinate corresponding to x2
 * @x: x-coordinate for which the interpolated y-value is to be calculated
 *
 * This function calculates the interpolated y-value for a given x-value using
 * linear interpolation between two points (x1, y1) and (x2, y2). The function
 * returns the interpolated y-value based on the input x-coordinate.
 *
 * Return: The interpolated y-value for the given x-coordinate.
 */
static
s16 get_y_val(s16 x1, s16 x2, s16 y1, s16 y2, s16 x)
{
	s16 den = x2 - x1;

	if (!den)
		return INVALID_DBR;

	return y1 + ((x - x1) * (y2 - y1)) / (x2 - x1);
}

/**
 * get_regmask_non_puncture - Calculate the regulatory mask for non-punctured
 * channels.
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 *
 * This function calculates the regulatory mask for non-punctured channels based
 * on the given offset and bandwidth. The mask value is determined by the offset
 * relative to the bandwidth and predefined thresholds.
 *
 * Return: The calculated regulatory mask value.
 */
static
s16 get_regmask_non_puncture(s16 offset, u16 bw)
{
	u16 hbw = bw >> 1;
	s16 mask;

	if (offset < 0)
		offset = 0 - offset;

	if (offset >= ((bw * 3) / 2))
		mask = -400;
	else if (offset >= bw)
		mask = -280 - ((120 * (offset - bw)) / hbw);
	else if (offset >= (hbw + 1))
		mask = -200 - ((80 * (offset - (hbw + 1))) / (hbw - 1));
	else
		mask = 0;

	return mask;
}

/**
 * handle_edge_puncture - Populate puncture mask values for edge puncture type
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 * @pu_l_edge: Offset value for the left edge of the puncture
 * region (in 0.01 MHz units)
 * @pu_r_edge: Offset value for the right edge of the puncture
 * region (in 0.01 MHz units)
 * @pdbm1: Pointer to an array of dB reduction values used to populate the mask
 *
 * This function sets the offset and dB reduction (dbr) values in the left and
 * right edge puncture mask structures for the PUNCTURE_TYPE_EDGE case. It uses
 * the provided edge offsets and a predefined dB mask array (typically pdbm1) to
 * define the regulatory mask shape on both sides of the punctured region.
 *
 * The mask is symmetric and ensures a smooth transition from the edge of the
 * punctured region to the adjacent usable spectrum.
 */
static void
handle_edge_puncture(struct punct_mask *pu_mask_l_edge,
		     struct punct_mask *pu_mask_r_edge, s16 pu_l_edge,
		     s16 pu_r_edge, const s16 *pdbm1)
{
	wpa_printf(MSG_DEBUG, "EDGE puncture");
	pu_mask_l_edge->offset[0] = pu_l_edge - ((pu_r_edge - pu_l_edge) / 2);
	pu_mask_l_edge->dbr[0] = pdbm1[2];

	pu_mask_l_edge->offset[1] = pu_l_edge - 5;
	pu_mask_l_edge->dbr[1] = pdbm1[1];

	pu_mask_l_edge->offset[2] = pu_l_edge;
	pu_mask_l_edge->dbr[2] = pdbm1[0];

	pu_mask_r_edge->offset[0] = pu_r_edge;
	pu_mask_r_edge->dbr[0] = pdbm1[0];

	pu_mask_r_edge->offset[1] = pu_r_edge + 5;
	pu_mask_r_edge->dbr[1] = pdbm1[1];

	pu_mask_r_edge->offset[2] = pu_r_edge + ((pu_r_edge - pu_l_edge) / 2);
	pu_mask_r_edge->dbr[2] = pdbm1[2];
}

/**
 * handle_interim_20_plus - Populate puncture mask values for INTERIM_20_PLUS
 * type.
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_l_edge: Offset value for the left edge of the puncture
 * region (in 0.01 MHz units)
 * @pu_r_edge: Offset value for the right edge of the puncture
 * region (in 0.01 MHz units)
 * @l_edge: Logical left edge of the channel (in 0.01 MHz units)
 * @r_edge: Logical right edge of the channel (in 0.01 MHz units)
 * @pu_edge1: Start offset of the interim puncture region (in 0.01 MHz units)
 * @pu_edge2: End offset of the interim puncture region (in 0.01 MHz units)
 * @pdbm1: Pointer to dB reduction values for edge shaping
 * @pdbm2: Pointer to dB reduction values for interim shaping
 *
 * This function sets the offset and dB reduction (dbr) values in the puncture
 * mask structures for the PUNCTURE_TYPE_INTERIM_20_PLUS case. It handles both
 * edge and interim puncture shaping, ensuring smooth transitions in the
 * regulatory mask across the punctured and adjacent usable spectrum.
 *
 * The function uses predefined dB masks (pdbm1 and pdbm2) to shape the
 * attenuation profile for both edge and interim regions.
 */
static void
handle_interim_20_plus(struct punct_mask *pu_mask_l_edge,
		       struct punct_mask *pu_mask_r_edge,
		       struct punct_mask *pu_mask_l,
		       struct punct_mask *pu_mask_r,
		       s16 pu_l_edge, s16 pu_r_edge, s16 l_edge, s16 r_edge,
		       s16 pu_edge1, s16 pu_edge2, const s16 *pdbm1,
		       const s16 *pdbm2)
{
	/* type 2 mask */
	wpa_printf(MSG_DEBUG, "INTERIM 20 PLUS puncture");
	/* Edge concurrent puncture */
	if (l_edge != pu_l_edge || r_edge != pu_r_edge) {
		pu_mask_l_edge->offset[0] =
				pu_l_edge - ((pu_edge1 - pu_l_edge) / 2);
		pu_mask_l_edge->dbr[0] = pdbm1[2];

		pu_mask_l_edge->offset[1] = pu_l_edge - 5;
		pu_mask_l_edge->dbr[1] = pdbm1[1];

		pu_mask_l_edge->offset[2] = pu_l_edge;
		pu_mask_l_edge->dbr[2] = pdbm1[0];

		pu_mask_r_edge->offset[0] = pu_r_edge;
		pu_mask_r_edge->dbr[0] = pdbm1[0];

		pu_mask_r_edge->offset[1] = pu_r_edge + 5;
		pu_mask_r_edge->dbr[1] = pdbm1[1];

		pu_mask_r_edge->offset[2] =
				pu_r_edge + ((pu_r_edge - pu_edge2) / 2);
		pu_mask_r_edge->dbr[2] = pdbm1[2];
	}

	pu_mask_l->offset[0] = pu_edge1;
	pu_mask_l->dbr[0] = pdbm2[0];

	pu_mask_l->offset[1] = pu_edge1 + 5;
	pu_mask_l->dbr[1] = pdbm2[1];

	pu_mask_l->offset[2] = pu_edge1 + ((pu_edge1 - pu_l_edge) >> 1);
	pu_mask_l->dbr[2] = pdbm2[2];

	pu_mask_r->offset[0] = pu_edge2 - ((pu_r_edge - pu_edge2) >> 1);
	pu_mask_r->dbr[0] = pdbm2[2];

	pu_mask_r->offset[1] = pu_edge2 - 5;
	pu_mask_r->dbr[1] = pdbm2[1];

	pu_mask_r->offset[2] = pu_edge2;
	pu_mask_r->dbr[2] = pdbm2[0];
}

/**
 * handle_interim_20 - Populate puncture mask values for INTERIM_20 type
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_edge1: Start offset of the interim puncture region (in 0.01 MHz units)
 * @pu_edge2: End offset of the interim puncture region (in 0.01 MHz units)
 * @pdbm3: Pointer to dB reduction values used to shape the interim mask
 *
 * This function sets the offset and dB reduction (dbr) values in the left and
 * right interim puncture mask structures for the PUNCTURE_TYPE_INTERIM_20 case.
 * It defines a symmetric attenuation profile across the punctured region using
 * the provided dB mask array (typically pdbm3).
 *
 * The mask ensures a smooth regulatory transition across the 20 MHz interim
 * puncture region, helping to meet spectral emission constraints.
 */
static void
handle_interim_20(struct punct_mask *pu_mask_l, struct punct_mask *pu_mask_r,
		  s16 pu_edge1, s16 pu_edge2, const s16 *pdbm3)
{
	wpa_printf(MSG_DEBUG, "INTERIM 20 puncture");
	pu_mask_l->offset[0] = pu_edge1;
	pu_mask_l->dbr[0] = pdbm3[0];

	pu_mask_l->offset[1] = pu_edge1 + 5;
	pu_mask_l->dbr[1] = pdbm3[1];

	pu_mask_l->offset[2] = pu_edge1 + 100;
	pu_mask_l->dbr[2] = pdbm3[2];

	pu_mask_r->offset[0] = pu_edge2 - 100;
	pu_mask_r->dbr[0] = pdbm3[2];

	pu_mask_r->offset[1] = pu_edge2 - 5;
	pu_mask_r->dbr[1] = pdbm3[1];

	pu_mask_r->offset[2] = pu_edge2;
	pu_mask_r->dbr[2] = pdbm3[0];
}

/**
 * get_puncture_type_and_masks - Determine the puncture mask limits for a given
 * bandwidth and puncture bitmap
 * @bw: Bandwidth for which the puncture mask limits are to be determined
 * @puncture_bitmap: Bitmap indicating the punctured sub-channels
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 *
 * This function calculates the puncture mask limits for a given bandwidth and
 * puncture bitmap. It determines the type of puncture (edge, interim 20 MHz,
 * interim 20 MHz plus, or invalid) and sets the appropriate offset and dbr
 * values in the provided pmask structures.
 *
 * Return: The type of puncture determined (enum puncture_type).
 */
static enum puncture_type
get_puncture_type_and_masks(u16 bw, u16 puncture_bitmap,
			    struct punct_mask *pu_mask_l_edge,
			    struct punct_mask *pu_mask_l,
			    struct punct_mask *pu_mask_r,
			    struct punct_mask *pu_mask_r_edge)
{
	u16 punc_mask, pp;
	s16 i, num_valid_bits = 0;
	s16 l_edge = (bw >> 1) - bw, r_edge = bw - (bw >> 1);
	s16 pu_l_edge = INVALID_EDGE, pu_r_edge = INVALID_EDGE;
	s16 pu_edge1 = INVALID_EDGE, pu_edge2 = INVALID_EDGE,
	punc_start = INVALID_EDGE;
	enum puncture_type punc_type;

	switch (bw) {
	case 80:
		punc_mask = PUNCTURE_80MHZ_MASK;
		num_valid_bits = 4;
		break;
	case 160:
		punc_mask = PUNCTURE_160MHZ_MASK;
		num_valid_bits = 8;
		break;
	case 320:
		punc_mask = PUNCTURE_320MHZ_MASK;
		num_valid_bits = 16;
		break;
	default:
		punc_mask = 0;
		wpa_printf(MSG_ERROR, "Bandwidth input invalid");
		return PUNCTURE_TYPE_INVALID;
	}

	pp = puncture_bitmap & punc_mask;
	if (!pp)
		return PUNCTURE_TYPE_INVALID;

	for (i = 0; i < num_valid_bits; i++) {
		if (!((1 << i) & pp) && pu_l_edge == INVALID_EDGE)
			pu_l_edge = l_edge + (i * 20);

		if (!((1 << (num_valid_bits - 1 - i)) & pp) &&
		    pu_r_edge == INVALID_EDGE)
			pu_r_edge = r_edge - (i * 20);

		if (punc_start != INVALID_EDGE &&
		    pu_edge1 == INVALID_EDGE &&
		    !((1 << i) & pp)) {
			/* End of interim puncture */
			pu_edge1 = punc_start;
			pu_edge2 = l_edge + (i * 20);
			punc_start = INVALID_EDGE;
		}

		if (((1 << i) & pp) && punc_start == INVALID_EDGE &&
		    ((l_edge + (i * 20)) > pu_l_edge)) {
			/* Start of interim puncture */
			punc_start = l_edge + (i * 20);
		}
	}

	/* Find the puncture type */
	if (pu_edge1 == INVALID_EDGE && (l_edge != pu_l_edge ||
	    r_edge != pu_r_edge))
		punc_type = PUNCTURE_TYPE_EDGE;
	else if ((pu_edge2 - pu_edge1) >= 40)
		punc_type = PUNCTURE_TYPE_INTERIM_20_PLUS;
	else if ((pu_edge2 - pu_edge1) == 20)
		punc_type = PUNCTURE_TYPE_INTERIM_20;
	else
		punc_type = PUNCTURE_TYPE_INVALID;

	pu_l_edge *= 10;
	pu_r_edge *= 10;
	pu_edge1 *= 10;
	pu_edge2 *= 10;
	l_edge  *= 10;
	r_edge  *= 10;

	switch (punc_type) {
	case PUNCTURE_TYPE_EDGE:
		handle_edge_puncture(pu_mask_l_edge, pu_mask_r_edge, pu_l_edge,
				     pu_r_edge, pdbm1);
		break;
	case PUNCTURE_TYPE_INTERIM_20_PLUS:
		handle_interim_20_plus(pu_mask_l_edge, pu_mask_r_edge,
				       pu_mask_l, pu_mask_r, pu_l_edge,
				       pu_r_edge, l_edge, r_edge, pu_edge1,
				       pu_edge2, pdbm1, pdbm2);
		break;
	case PUNCTURE_TYPE_INTERIM_20:
		handle_interim_20(pu_mask_l, pu_mask_r, pu_edge1, pu_edge2,
				  pdbm3);
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "Investigate - Invalid puncture type!!!");
		break;
	}

	return punc_type;
}

/**
 * get_regmask_puncture - Calculate the regulatory mask for punctured channels
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 * @pmask: Pointer to the pmask structure containing puncture mask limits
 *
 * This function calculates the regulatory mask for punctured channels based
 * on the given offset, bandwidth, and puncture mask limits. The mask value is
 * determined by the offset relative to the puncture mask limits defined in the
 * pmask structure.
 *
 * Return: The calculated regulatory mask value, or INVALID_DBR if the offset
 * does not fall within the defined puncture mask limits.
 */

static s16 get_regmask_puncture(s16 offset, u16 bw, struct punct_mask *pmask)
{
	s16 mask;

	offset *= 10;

	if (offset <= pmask->offset[0]) {
		mask = pmask->dbr[0];
	} else if ((offset > pmask->offset[0]) && (offset < pmask->offset[1])) {
		mask = get_y_val(pmask->offset[0], pmask->offset[1],
				 pmask->dbr[0], pmask->dbr[1], offset);
	} else if (offset == pmask->offset[1]) {
		mask = pmask->dbr[1];
	} else if ((offset > pmask->offset[1]) && (offset < pmask->offset[2])) {
		mask = get_y_val(pmask->offset[1], pmask->offset[2],
				 pmask->dbr[1], pmask->dbr[2], offset);
	} else if (offset >= pmask->offset[2]) {
		mask = pmask->dbr[2];
	} else {
		mask = INVALID_DBR;
	}

	return mask;
}

/**
 * get_regmask - Calculate the regulatory mask for a given offset and bandwidth
 * @offset: Offset value for the frequency
 * @bw: Bandwidth of the channel
 * @punc_type: Type of puncture (enum puncture_type)
 * @pu_mask_l_edge: Pointer to the left edge puncture mask structure
 * @pu_mask_l: Pointer to the left interim puncture mask structure
 * @pu_mask_r: Pointer to the right interim puncture mask structure
 * @pu_mask_r_edge: Pointer to the right edge puncture mask structure
 *
 * This function calculates the regulatory mask for a given offset and bandwidth
 * based on the puncture type and the puncture mask limits defined in the pmask
 * structures. It determines the appropriate mask value by comparing the
 * non-puncture mask and puncture mask values.
 *
 * Return: The calculated regulatory mask value.
 */
static
s16 get_regmask(s16 offset, u16 bw, enum puncture_type punc_type,
		struct punct_mask *pu_mask_l_edge, struct punct_mask *pu_mask_l,
		struct punct_mask *pu_mask_r,
		struct punct_mask *pu_mask_r_edge)
{
	s16 mask, mask_def, mask_le, mask_re, mask_l, mask_r, mask_punc;

	mask_def = get_regmask_non_puncture(offset, bw);
	if (punc_type == PUNCTURE_TYPE_INVALID)
		return mask_def;

	mask_le = get_regmask_puncture(offset, bw, pu_mask_l_edge);
	mask_re = get_regmask_puncture(offset, bw, pu_mask_r_edge);
	mask_l  = get_regmask_puncture(offset, bw, pu_mask_l);
	mask_r  = get_regmask_puncture(offset, bw, pu_mask_r);

	switch (punc_type) {
	case PUNCTURE_TYPE_EDGE:
		mask_punc = MIN(mask_le, mask_re);
		break;
	case PUNCTURE_TYPE_INTERIM_20_PLUS:
		if ((pu_mask_l->offset[0] <= (offset * 10)) &&
		    ((offset * 10) <= pu_mask_r->offset[2]))
			mask_punc = MAX(mask_l, mask_r);
		else
			mask_punc = MIN(mask_le, mask_re);
		break;
	case PUNCTURE_TYPE_INTERIM_20:
		mask_punc = MAX(mask_l, mask_r);
		break;
	default:
		return mask_def;
	}

	mask = MIN(mask_punc, mask_def);

	return mask;
}

void
get_min_psd_values(struct afc_sp_reg_info *afc_rsp_info, u16 freq, u16 cfreq,
		   u16 punc_bitmap, u16 bw, s16 *min_psd)
{
	u16 freq_start, freq_end;
	u16 adj_freq_start, adj_freq_end;
	s16 offset;
	u16 modoffset;
	u16 hbw = bw >> 1;
	enum puncture_type punc_type;
	struct punct_mask pu_mask_l = {0}, pu_mask_r = {0},
			  pu_mask_l_edge = {0}, pu_mask_r_edge = {0};
	struct afc_freq_obj *afc_freq_info;
	u8 num_freq_obj;
	int i;
	s16 mask, psd_limit = 0;

	*min_psd = CHAN_MAX_PSD_POWER * PSD_SCALE;
	if (!is_6ghz_freq(cfreq))
		return;

	freq_start = cfreq - hbw;
	freq_end   = cfreq + hbw;
	adj_freq_start = MAX(DEFAULT_LOW_6GFREQ, (cfreq - (3 * hbw)));
	adj_freq_end   = MIN((cfreq + (3 * hbw)), DEFAULT_HIGH_6GFREQ);

	num_freq_obj = afc_rsp_info->num_freq_objs;
	if (!num_freq_obj) {
		wpa_printf(MSG_ERROR, "No frequency objects found!");
		return;
	}

	afc_freq_info = afc_rsp_info->afc_freq_info;
	if (!afc_freq_info) {
		wpa_printf(MSG_ERROR, "freq info is NULL!");
		return;
	}

	punc_type = get_puncture_type_and_masks(bw, punc_bitmap,
						&pu_mask_l_edge,
						&pu_mask_l, &pu_mask_r,
						&pu_mask_r_edge);

	for (i = adj_freq_start; i <= adj_freq_end; i++) {
		offset = i - cfreq;
		modoffset = abs(offset);

		psd_limit = get_psd_limit(i, num_freq_obj, afc_freq_info);
		if (psd_limit == INVALID_PSD) {
			/* If PSD limit is invalid for usable freq and not
			 * punctured, return here. Other adjacent freq can be
			 * ignored.
			 */
			if (i >= freq_start && i < freq_end) {
				if (!(punc_bitmap &
				      (1 << ((i - freq_start) / CHWIDTH_20))))
					return;
			}
			continue;
		}

		if (modoffset <= ((bw * 3) >> 1)) {
			mask = get_regmask(offset, bw, punc_type,
					   &pu_mask_l_edge, &pu_mask_l,
					   &pu_mask_r, &pu_mask_r_edge);
			*min_psd = MIN((s16)(*min_psd),
				       (s16)(psd_limit - (mask * 10)));
		}
	}
}

static s16
fill_psd_power_for_punctured_freq(struct hostapd_data *hapd, u16 freq,
				  u8 client_mode, u8 pwr_mode,
				  enum max_tx_pwr_interpretation tx_pwr_intrpn,
				  s16 primary_20_mhz_psd)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 num_channels_6ghz, chan_idx;
	struct hostapd_channel_data *ch_6g_lst;
	u8 pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	s16 psd_power;

	ch_6g_lst = hostapd_iface_get_6ghz_chan_list(iface,
						     freq,
						     pwr_type,
						     &num_channels_6ghz,
						     &chan_idx);
	if (!ch_6g_lst || (ch_6g_lst->flag & HOSTAPD_CHAN_DISABLED)) {
		int ret;

		/* The given freq is not found in SP power mode channel list, so it is
		 * an LPI/VLP channel. Fetch the LPI reg power from regulatory
		 * database
		 */
		wpa_printf(MSG_WARNING,
			   "Error getting 6 GHz chan: power mode: %d freq: %d",
			   pwr_type, freq);
		ret = hostapd_reg_get_psd_from_chan_list(iface, freq, freq, CHWIDTH_20, 0,
							 NL80211_REG_AP_LPI,
							 NL80211_REG_REGULAR_CLIENT_LPI, true, false,
							 &psd_power);
		if (ret)
			psd_power = CHAN_MIN_TX_POWER;
		wpa_printf(MSG_DEBUG, "LPI/VLP channel %d MHz, psd_power: %d",
			   freq, psd_power);

	} else {
		/* For SP punctured channel, psd power is primary 20 MHZ PSD - 16 */
		psd_power = (primary_20_mhz_psd - PUNCTURED_SP_CHAN_POWER_DIFF);
		wpa_printf(MSG_DEBUG, "SP channel %d MHz, psd_power: %d",
			   freq, psd_power);
	}

	if (pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
	    !hostapd_is_additional_tpe(tx_pwr_intrpn)) {
		return get_max_psd_for_composite_ap(iface, freq,
						    pwr_type, client_mode,
						    psd_power);
	}
	return psd_power;
}

static int compute_chan_psd_common(struct hostapd_data *hapd, int i,
				   struct ieee_chan_data chan_data,
				   u16 punct_bitmap, int j, bool fill_psd_for_sp,
				   int client_mode, int pwr_mode,
				   enum max_tx_pwr_interpretation tx_pwr_intrpn,
				   s16 *primary_20mhz_psd)
{
	if ((punct_bitmap & BIT(j)) && fill_psd_for_sp) {
		u16 punc_freq = chan_data.channels[i].freq;

		return fill_psd_power_for_punctured_freq(hapd, punc_freq,
							 client_mode, pwr_mode,
							 tx_pwr_intrpn,
							 *primary_20mhz_psd);
	}

	return get_psd_for_chan_idx(hapd, i, chan_data, client_mode,
				    pwr_mode, tx_pwr_intrpn, primary_20mhz_psd);
}

#ifdef CONFIG_QCN_EXTN
static bool set_punct_psd_override(struct hostapd_data *hapd, u16 punct_bitmap,
				   int j, s16 *chan_psd)
{
	bool is_punc_chn_txpwr;

	is_punc_chn_txpwr = hapd->conf->bss_extn.tpe_punct_channel_tx_pwr;
	if ((punct_bitmap & BIT(j)) && is_punc_chn_txpwr) {
		*chan_psd = RNR_20_MHZ_PSD_NO_POWER / 2;

		return true;
	}

	return false;
}
#else
static inline bool set_punct_psd_override(struct hostapd_data *hapd,
					  u16 punct_bitmap, int j, s16 *chan_psd)
{
	return false;
}
#endif


int get_psd_values(struct hostapd_data *hapd, int non_11be_start_idx,
			  int chan_start_idx, int non_11be_chan_count,
			  int total_chan_count, u8 *tx_pwr_count,
			  s8 *tx_pwr_array, u8 *tx_pwr_ext_count,
			  s8 *tx_pwr_ext_array, u8 client_mode, struct ieee_chan_data chan_data,
			  u8 pwr_mode, enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	struct hostapd_iface *iface = hapd->iface;
	u16 punct_bitmap = iface->conf->punct_bitmap;
	u16 non_be_chan_index_map = 0;
	int non11be_chan_pos = non_11be_start_idx - chan_start_idx;
	s16 start_chan_psd = RNR_20_MHZ_PSD_NO_POWER, chan_psd = RNR_20_MHZ_PSD_NO_POWER;
	int i = 0, j = 0;
	s16 primary_20mhz_psd = RNR_20_MHZ_PSD_NO_POWER;
	u8 pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	bool is_composite_ap_sp = false;
	bool is_different_psd = false;
	bool fill_psd_for_sp = false;

	if (!tx_pwr_array || ((total_chan_count - non_11be_chan_count) && !tx_pwr_ext_array))
		return -1;
	if (chan_start_idx >= chan_data.num_channels ||
	    chan_start_idx + total_chan_count >= chan_data.num_channels) {
		wpa_printf(MSG_ERROR, "Invalid start index: %d, num_chan:%d",
			   chan_start_idx, total_chan_count);
		return -1;
	}

	start_chan_psd = get_psd_for_chan_idx(hapd, non_11be_start_idx, chan_data,
					      client_mode, pwr_mode, tx_pwr_intrpn,
					      &primary_20mhz_psd);

	for (i = non_11be_start_idx; i < non_11be_start_idx + non_11be_chan_count;
	     i++, non11be_chan_pos++) {
		if (i >= chan_data.num_channels) {
			wpa_printf(MSG_ERROR, "Invalid channel index :%d", i);
			return -1;
		}
		non_be_chan_index_map |= BIT(non11be_chan_pos);
		chan_psd = get_psd_for_chan_idx(hapd, i, chan_data, client_mode,
						pwr_mode, tx_pwr_intrpn,
						&primary_20mhz_psd);
		*tx_pwr_array = chan_psd * 2;
		tx_pwr_array++;
		if (start_chan_psd != chan_psd)
			is_different_psd = true;
	}
#ifdef CONFIG_IEEE80211BE
	/* For 11be the TPE extension parameter added if the bw is 320MHZ or if
	 * any channel is punctured in 320MHZ/160MHZ/80MHZ
	 */
	if (pwr_type == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP &&
		hostapd_is_additional_tpe(tx_pwr_intrpn))
		is_composite_ap_sp = true;

	if (is_composite_ap_sp || pwr_type == HE_REG_INFO_6GHZ_AP_TYPE_SP)
		fill_psd_for_sp = true;

	for (i = chan_start_idx, j = 0; i < chan_start_idx + total_chan_count; i++, j++) {
		if (i >= chan_data.num_channels) {
			wpa_printf(MSG_ERROR, "Invalid channel index :%d", i);
			return -1;
		}
		if (non_be_chan_index_map & BIT(j)) { /* filled in 11ax TPE*/
			continue;
		}
		if (!set_punct_psd_override(hapd, punct_bitmap, j, &chan_psd))
			chan_psd = compute_chan_psd_common(hapd, i, chan_data,
							   punct_bitmap, j,
							   fill_psd_for_sp,
							   client_mode,
							   pwr_mode,
							   tx_pwr_intrpn,
							   &primary_20mhz_psd);

		*tx_pwr_ext_array = chan_psd * 2;
		tx_pwr_ext_array++;
		*tx_pwr_ext_count += 1;
		if (start_chan_psd != chan_psd)
			is_different_psd = true;
	}
#endif
#ifdef CONFIG_QCN_EXTN
	if (!hapd->conf->bss_extn.tpe_common_psd)
		is_different_psd = true;
#endif
	if (!is_different_psd) {
		*tx_pwr_count = 0;
		*tx_pwr_ext_count = 0;
	} else {
		*tx_pwr_count = num_psd_values_to_psd_count(non_11be_chan_count);
		if (*tx_pwr_count == 0) {
			wpa_printf(MSG_ERROR, "Invalid channel count:%d", non_11be_chan_count);
			return -1;
		}
	}

	return 0;
}

static u8 *hostapd_add_psd_tpe(struct hostapd_data *hapd, u8 client_mode,
			       u8 *eid, u8 tx_pwr_cat,
			       enum max_tx_pwr_interpretation tx_pwr_intrpn,
			       u8 pwr_mode)
{
	s8 tx_pwr_ext_array[MAX_PSD_TPE_EXT_POWER_COUNT] = {0};
	int non_11be_chan_count = 0, total_chan_count = 0;
	int non_11be_start_idx = 0, chan_start_idx = 0;
	s8 tx_pwr_array[MAX_PSD_TPE_POWER_COUNT] = {0};
	u8 tx_pwr_count = 0, tx_pwr_ext_count = 0;
 	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct ieee_chan_data chan_data;

	wpa_printf(MSG_DEBUG, "Adding TPE for client mode: %d, pwr_mode: %d",
		   client_mode, pwr_mode);
	if (set_ieee_order_chan_list(mode, &chan_data, client_mode))
		return eid;

	if (get_chan_list(hapd, &non_11be_start_idx, &chan_start_idx,
			  &non_11be_chan_count, &total_chan_count, chan_data)) {
		wpa_printf(MSG_ERROR, "Unable to get chan list");
		goto free;
	}
	wpa_printf(MSG_DEBUG, "non_11be_start_idx: %d, chan_start_idx: %d, "
		   "non_11be_chan_count: %d, total_chan_count: %d, pwr_mode: %d, client_mode: %d, tx_pwr_in: %d\n",
		   non_11be_start_idx, chan_start_idx,
		   non_11be_chan_count, total_chan_count, pwr_mode, client_mode, tx_pwr_intrpn);

	if (get_psd_values(hapd, non_11be_start_idx, chan_start_idx,
			   non_11be_chan_count, total_chan_count, &tx_pwr_count,
			   tx_pwr_array, &tx_pwr_ext_count, tx_pwr_ext_array,
			   client_mode, chan_data, pwr_mode, tx_pwr_intrpn)) {
		wpa_printf(MSG_ERROR, "failed to get the PSD values");
		goto free;
	}

	eid = hostapd_add_tpe_info(eid, tx_pwr_intrpn, tx_pwr_count,
				   tx_pwr_array, tx_pwr_ext_count, tx_pwr_ext_array,
				   tx_pwr_cat);

free:
	free_ieee_ordered_chan_list(&chan_data);

	return eid;
}

static u8 *hostapd_append_local_tpe(struct hostapd_data *hapd,
				 u8 *eid)
{
	ieee80211_tpe_config_user_params *tpe_conf = &hapd->conf->tpe_ie_config;
	struct ieee80211_tpe_payload tpe_entry;
	u8 num_tpe_ext_elem;
	s8 tpe_11ax_count;
	u8 index;

	if (!tpe_conf->local_tpe_config)
		return eid;

	for (index = 0; index < IEEE80211_TPE_LOCAL_CONFIG_MAX; index++) {
		if (tpe_conf->local_tpe_config & (1 << index)) {

			num_tpe_ext_elem = tpe_conf->tpe_config[index].num_tpe_ext_elem;
			tpe_entry = tpe_conf->tpe_config[index].tpe_payload;
			tpe_11ax_count = hostapd_get_tpe_11ax_count(tpe_entry.tpe_info_intrpt,
								    tpe_entry.tpe_info_cnt);

			if (tpe_11ax_count < 0) {
				wpa_printf(MSG_ERROR, "Failed to get TPE 11ax count for index %d",
					   index);
				continue;
			}
			eid = hostapd_add_tpe_info(eid,
						   tpe_entry.tpe_info_intrpt,
						   tpe_entry.tpe_info_cnt,
						   tpe_entry.local_max_txpwr,
						   num_tpe_ext_elem,
						   &tpe_entry.local_max_txpwr[tpe_11ax_count],
						   tpe_entry.tpe_info_cat);
		}
	}
	return eid;
}

/**
 * hostapd_add_6g_tpe() - For the given power mode, add the required TPE IEs
 * @hapd: hostapd BSS data structure
 * @eid: Pointer to position at which TPE IE is to be added
 * @pwr_mode: Current power mode of the AP
 *
 * Return: void
 *
 * For an AP, operating in LPI, the TPEs to be advertised
 * in the beacon are
 * TPE1: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation = Regulatory Client EIRP PSD
 * TPE2: Max Tx Pwr Category = Subordinate,
 *	 Max Tx Pwr Interpretation = Regulatory Client EIRP PSD
 *
 * For an AP operating in SP / VLP power mode, Subordinate client category is
 * not valid. So, the TPEs to be advertised in the beacon are
 * TPE1: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation = Regulatory Client EIRP PSD
 * TPE2: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation = Regulatory Client EIRP
 *
 * For a composite AP, the TPEs to be advertised in the beacon are
 * TPE1: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation =
 *		Regulatory Client EIRP PSD i.e. max(LPI client power, SP client power)
 * TPE2: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation =
 *		Regulatory Client EIRP i.e. max(LPI client power, SP client power)
 * TPE3: Max Tx Pwr Category = Subordinate,
 *	 Max Tx Pwr Interpretation =
 *		Regulatory Client EIRP PSD i.e. max(LPI client power, SP client power)
 * TPE4: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation =
 *		Additional Regulatory Client EIRP PSD i.e. min(AFC - 6, Reg SP client power)
 * TPE5: Max Tx Pwr Category = Default,
 *	 Max Tx Pwr Interpretation =
 *		Additional Regulatory Client EIRP i.e. SP client power
 *
 */
static void hostapd_add_6g_tpe(struct hostapd_data *hapd, u8 **eid, u8 pwr_mode)
{
#ifdef CONFIG_QCN_EXTN
	enum tpe_tx_pwr_interp_unit pwr_interp_conf;

	pwr_interp_conf = hapd->conf->bss_extn.tpe_tx_pwr_interp;
	if (pwr_interp_conf < TPE_REG_EIRP_PSD || pwr_interp_conf > TPE_REG_EIRP)
		pwr_interp_conf = TPE_REG_EIRP_PSD;
#endif

	if (hostapd_is_indoor_sp_pwr_mode(hapd))
		pwr_mode = HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP;

	switch(pwr_mode) {
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR:
#ifdef CONFIG_QCN_EXTN
		if (pwr_interp_conf == TPE_REG_EIRP_PSD) {
#endif
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_REGULAR_CLIENT_LPI,
						   *eid, REG_DEFAULT_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_SUBORDINATE_CLIENT_LPI,
						   *eid, REG_SUBORDINATE_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
#ifdef CONFIG_QCN_EXTN
		} else if (pwr_interp_conf == TPE_REG_EIRP) {
			*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_REGULAR_CLIENT_LPI,
						    *eid, REG_DEFAULT_CLIENT,
						    REGULATORY_CLIENT_EIRP, pwr_mode);
			*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_SUBORDINATE_CLIENT_LPI,
						    *eid, REG_SUBORDINATE_CLIENT,
						    REGULATORY_CLIENT_EIRP, pwr_mode);
		}
#endif
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_VLP:
#ifdef CONFIG_QCN_EXTN
		if (pwr_interp_conf == TPE_REG_EIRP_PSD)
#endif
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_REGULAR_CLIENT_VLP,
						   *eid, REG_DEFAULT_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
		*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_REGULAR_CLIENT_VLP,
					    *eid, REG_DEFAULT_CLIENT,
					    REGULATORY_CLIENT_EIRP, pwr_mode);
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_SP:
#ifdef CONFIG_QCN_EXTN
		if (pwr_interp_conf == TPE_REG_EIRP_PSD)
#endif
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
						   *eid, REG_DEFAULT_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
		*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
					    *eid, REG_DEFAULT_CLIENT,
					    REGULATORY_CLIENT_EIRP, pwr_mode);
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP:
#ifdef CONFIG_QCN_EXTN
		if (pwr_interp_conf == TPE_REG_EIRP_PSD)
#endif
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
						   *eid, REG_DEFAULT_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
		*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
					    *eid, REG_DEFAULT_CLIENT,
					    REGULATORY_CLIENT_EIRP, pwr_mode);
#ifdef CONFIG_QCN_EXTN
		if (pwr_interp_conf == TPE_REG_EIRP_PSD) {
#endif
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_SUBORDINATE_CLIENT_SP,
						   *eid, REG_SUBORDINATE_CLIENT,
						   REGULATORY_CLIENT_EIRP_PSD, pwr_mode);
			*eid = hostapd_add_psd_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
						   *eid, REG_DEFAULT_CLIENT,
						   REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD,
						   pwr_mode);
#ifdef CONFIG_QCN_EXTN
		} else if (pwr_interp_conf == TPE_REG_EIRP) {
			*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_SUBORDINATE_CLIENT_SP,
						    *eid, REG_SUBORDINATE_CLIENT,
						    REGULATORY_CLIENT_EIRP, pwr_mode);
		}
#endif
		*eid = hostapd_add_eirp_tpe(hapd, NL80211_REG_REGULAR_CLIENT_SP,
					    *eid, REG_DEFAULT_CLIENT,
					    REGULATORY_CLIENT_ADDITIONAL_EIRP,
					    pwr_mode);
		break;
	}
	*eid = hostapd_append_local_tpe(hapd, *eid);
}

u8 * hostapd_eid_txpower_envelope(struct hostapd_data *hapd, u8 *eid)
{
	u8 channel, tx_pwr_count, local_pwr_constraint, tx_pwr_ext_count = 0;
	s8 tx_pwr_ext_array[TPE_NUM_EIRP_POWER_EXT_SUPPORTED] = {0};
	s8 eirp_tx_pwr_array[MAX_TPE_EIRP_NUM_POWER_SUPPORTED];
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct hostapd_config *iconf = iface->conf;
	struct hostapd_channel_data *chan;
	s8 max_tx_power, tx_pwr;
	int dfs, i;

	if (!mode)
		return eid;

	if (ieee80211_freq_to_chan(iface->freq, &channel) == NUM_HOSTAPD_MODES)
		return eid;

#ifdef CONFIG_IEEE80211AX
	if (is_6ghz_op_class(iconf->op_class)) {
		hostapd_add_6g_tpe(hapd, &eid, iconf->he_6ghz_reg_pwr_type);
		return eid;
	}
#endif /* CONFIG_IEEE80211AX */

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == iface->freq)
			break;
	}

	if (i == mode->num_channels)
		return eid;

	chan = &mode->channels[i];

	switch (hostapd_get_oper_chwidth(iconf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iconf->secondary_channel == 0) {
			/* Max Transmit Power count = 0 (20 MHz) */
			tx_pwr_count = 0;
		} else {
			/* Max Transmit Power count = 1 (20, 40 MHz) */
			tx_pwr_count = 1;
		}
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		/* Max Transmit Power count = 2 (20, 40, and 80 MHz) */
		tx_pwr_count = 2;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
	case CONF_OPER_CHWIDTH_320MHZ:
		/* Max Transmit Power count = 3 (20, 40, 80, 160/80+80, 320 MHz) */
		tx_pwr_count = 3;
		break;
	default:
		return eid;
	}

	/*
	 * Below local_pwr_constraint logic is referred from
	 * hostapd_eid_pwr_constraint.
	 *
	 * Check if DFS is required by regulatory.
	 */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0)
		dfs = 0;

	/*
	 * In order to meet regulations when TPC is not implemented using
	 * a transmit power that is below the legal maximum (including any
	 * mitigation factor) should help. In this case, indicate 3 dB below
	 * maximum allowed transmit power.
	 */
	if (hapd->iconf->local_pwr_constraint == -1)
		local_pwr_constraint = (dfs == 0) ? 0 : 3;
	else
		local_pwr_constraint = hapd->iconf->local_pwr_constraint;

	/*
	 * A STA that is not an AP shall use a transmit power less than or
	 * equal to the local maximum transmit power level for the channel.
	 * The local maximum transmit power can be calculated from the formula:
	 * local max TX pwr = max TX pwr - local pwr constraint
	 * Where max TX pwr is maximum transmit power level specified for
	 * channel in Country element and local pwr constraint is specified
	 * for channel in this Power Constraint element.
	 */
	chan = &mode->channels[i];
	max_tx_power = chan->max_tx_power - local_pwr_constraint;

	/*
	 * Local Maximum Transmit power is encoded as two's complement
	 * with a 0.5 dB step.
	 */
	max_tx_power *= 2; /* in 0.5 dB steps */
	if (max_tx_power > 127) {
		/* 63.5 has special meaning of 63.5 dBm or higher */
		max_tx_power = 127;
	}
	if (max_tx_power < -128)
		max_tx_power = -128;
	if (max_tx_power < 0)
		tx_pwr = 0x80 + max_tx_power + 128;
	else
		tx_pwr = max_tx_power;

	memset(eirp_tx_pwr_array, tx_pwr, tx_pwr_count + 1);
	hostapd_fill_eirp_for_ext_tpe(iconf, tx_pwr_ext_array,
				      eirp_tx_pwr_array, &tx_pwr_ext_count,
				      tx_pwr_count + 1);
	eid = hostapd_add_tpe_info(eid, LOCAL_EIRP, tx_pwr_count, eirp_tx_pwr_array,
				   tx_pwr_ext_count, tx_pwr_ext_array,
				    REG_MAX_CLIENT_TYPE);
	return eid;
}


/* Wide Bandwidth Channel Switch subelement */
static u8 * hostapd_eid_wb_channel_switch(struct hostapd_data *hapd, u8 *eid,
					  u8 chan1, u8 chan2)
{
	u8 bw;
	enum oper_chan_width chan_width;

	switch (hapd->cs_freq_params.bandwidth) {
	case 320:
		chan_width = CONF_OPER_CHWIDTH_320MHZ;
		break;
	case 160:
		chan_width = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case 80:
		chan_width = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case 40:
		chan_width = CONF_OPER_CHWIDTH_USE_HT;
		break;
	default:
		return eid;
	}

	/* check max bandwidth without any disabled channels */
	punct_update_legacy_bw(hapd->cs_freq_params.punct_bitmap,
			       hapd->cs_freq_params.channel, &chan_width,
			       &chan1, &chan2);

	/* bandwidth: 0: 40, 1: 80, 160, 80+80, 4 to 255 reserved as per
	 * IEEE Std 802.11-2024, 9.4.2.156 and Table 9-316 (VHT Operation
	 * Information subfields).
	 */
	switch (chan_width) {
	case CONF_OPER_CHWIDTH_320MHZ:
		/* As per IEEE Std 802.11be-2024, 35.15.3 (Channel switching
		 * methods for an EHT BSS), for EHT BSS operating channel width
		 * wider than 160 MHz, the announced BSS bandwidth in the Wide
		 * Bandwidth Channel Switch element is less than the BSS
		 * bandwidth in the Bandwidth Indication element
		 */

		/* Modifying the center frequency to 160 MHz */
		if (hapd->cs_freq_params.channel < chan1)
			chan1 -= 16;
		else
			chan1 += 16;

		/* fallthrough */
	case CONF_OPER_CHWIDTH_160MHZ:
		/* Update the CCFS0 and CCFS1 values in the element based on
		 * IEEE Std 802.11-2024, Table 9-316 (VHT Operation
		 * Information subfields).
		 */

		/* CCFS1 - The channel center frequency index of the 160 MHz
		 * channel. */
		chan2 = chan1;

		/* CCFS0 - The channel center frequency index of the 80 MHz
		 * channel segment that contains the primary channel. */
		if (hapd->cs_freq_params.channel < chan1)
			chan1 -= 8;
		else
			chan1 += 8;

		bw = 1;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		bw = 1;
		break;
	case CONF_OPER_CHWIDTH_USE_HT:
		/* Wide Bandwidth Channel Switch element is present only
		 * when the new channel width is wider than 20 MHz
		 */
		if (chan1 == hapd->cs_freq_params.channel)
			return eid;
		bw = 0;
		break;
	default:
		/* not valid VHT bandwidth or not in CSA */
		return eid;
	}

	*eid++ = WLAN_EID_WIDE_BW_CHSWITCH;
	*eid++ = 3; /* Length of Wide Bandwidth Channel Switch element */
	*eid++ = bw; /* New Channel Width */
	*eid++ = chan1; /* New Channel Center Frequency Segment 0 */
	*eid++ = chan2; /* New Channel Center Frequency Segment 1 */

	return eid;
}


#ifdef CONFIG_IEEE80211BE
/*
 * Bandwidth Indication element that is also used as the Bandwidth Indication
 * For Channel Switch subelement within a Channel Switch Wrapper element.
 */
static u8 * hostapd_eid_bw_indication(struct hostapd_data *hapd, u8 *eid,
				      u8 chan1, u8 chan2)
{
	u16 punct_bitmap = hapd->cs_freq_params.punct_bitmap;
	struct ieee80211_bw_ind_element *bw_ind_elem;
	size_t elen = 4;
	int bandwidth = hapd->cs_freq_params.bandwidth;

	/*
	 * Special case: non-standard 5 GHz 320 MHz operation. If puncturing
	 * reduces the effective width to < 160 MHz, skip BW Indication.
	 * For 160 MHz, update the puncture bitmap to the effective width.
	 */
	if (bandwidth == CHWIDTH_320 &&
	    is_5ghz_freq(hapd->cs_freq_params.freq) &&
	    punct_bitmap) {
#ifdef CONFIG_QCN_EXTN
		if (hostapd_handle_5ghz_320mhz_bw_indication_extn(
			    hapd, &chan1, &chan2, &punct_bitmap,
			    &bandwidth) < 0)
			return eid;
#else /* CONFIG_QCN_EXTN */
		return eid;
#endif /* CONFIG_QCN_EXTN */
	}

	if (bandwidth <= CHWIDTH_160 && !punct_bitmap)
		return eid;

	if (punct_bitmap)
		elen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

	*eid++ = WLAN_EID_EXTENSION;
	*eid++ = 1 + elen;
	*eid++ = WLAN_EID_EXT_BANDWIDTH_INDICATION;

	bw_ind_elem = (struct ieee80211_bw_ind_element *) eid;
	os_memset(bw_ind_elem, 0, sizeof(struct ieee80211_bw_ind_element));

	switch (bandwidth) {
	case CHWIDTH_320:
		bw_ind_elem->bw_ind_info.control |= BW_IND_CHANNEL_WIDTH_320MHZ;
		chan2 = chan1;
		if (hapd->cs_freq_params.channel < chan1)
			chan1 -= 16;
		else
			chan1 += 16;
		break;
	case CHWIDTH_160:
		bw_ind_elem->bw_ind_info.control |= BW_IND_CHANNEL_WIDTH_160MHZ;
		chan2 = chan1;
		if (hapd->cs_freq_params.channel < chan1)
			chan1 -= 8;
		else
			chan1 += 8;
		break;
	case CHWIDTH_80:
		bw_ind_elem->bw_ind_info.control |= BW_IND_CHANNEL_WIDTH_80MHZ;
		break;
	case CHWIDTH_40:
		if (hapd->cs_freq_params.sec_channel_offset == 1)
			bw_ind_elem->bw_ind_info.control |=
				BW_IND_CHANNEL_WIDTH_40MHZ;
		else
			bw_ind_elem->bw_ind_info.control |=
				BW_IND_CHANNEL_WIDTH_20MHZ;
		break;
	default:
		bw_ind_elem->bw_ind_info.control |= BW_IND_CHANNEL_WIDTH_20MHZ;
		break;
	}

	bw_ind_elem->bw_ind_info.ccfs0 = chan1;
	bw_ind_elem->bw_ind_info.ccfs1 = chan2;

	if (punct_bitmap) {
		bw_ind_elem->bw_ind_params |=
			BW_IND_PARAMETER_DISABLED_SUBCHAN_BITMAP_PRESENT;
		bw_ind_elem->bw_ind_info.disabled_chan_bitmap =
			host_to_le16(punct_bitmap);
	}

	return eid + elen;
}
#endif /* CONFIG_IEEE80211BE */


u8 * hostapd_eid_chsw_wrapper(struct hostapd_data *hapd, u8 *eid)
{
	u8 chan1 = 0, chan2 = 0;
	u8 *eid_len_offset, *start_pos;
	int freq1;

	if (!(hostapd_is_vht_enabled(hapd)) &&
	    !(hostapd_is_he_enabled(hapd)) &&
	    !(hostapd_is_eht_enabled(hapd)))
		return eid;

	if (!hapd->cs_freq_params.channel ||
	    (!hapd->cs_freq_params.vht_enabled &&
	     !hapd->cs_freq_params.he_enabled &&
	     !hapd->cs_freq_params.eht_enabled))
		return eid;

	freq1 = hapd->cs_freq_params.center_freq1 ?
		hapd->cs_freq_params.center_freq1 :
		hapd->cs_freq_params.freq;
	if (ieee80211_freq_to_chan(freq1, &chan1) !=
	    HOSTAPD_MODE_IEEE80211A)
		return eid;

	if (hapd->cs_freq_params.center_freq2 &&
	    ieee80211_freq_to_chan(hapd->cs_freq_params.center_freq2,
				   &chan2) != HOSTAPD_MODE_IEEE80211A)
		return eid;

	start_pos = eid;
	*eid++ = WLAN_EID_CHANNEL_SWITCH_WRAPPER;
	eid_len_offset = eid++; /* Length of Channel Switch Wrapper element */

	eid = hostapd_eid_wb_channel_switch(hapd, eid, chan1, chan2);

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		/* Bandwidth Indication For Channel Switch subelement */
		eid = hostapd_eid_bw_indication(hapd, eid, chan1, chan2);
	}
#endif /* CONFIG_IEEE80211BE */

	if (eid == start_pos + 2)
		return start_pos;

	*eid_len_offset = (eid - eid_len_offset) - 1;

	return eid;
}

int hostapd_config_read_maclist(const char *fname,
				struct mac_acl_entry **acl, int *num)
{
	FILE *f;
	char buf[128], *pos, *mask_pos;
	int line = 0;
	u8 addr[ETH_ALEN];
	u8 mask[ETH_ALEN];
	int vlan_id;
	bool has_mask;
	int mask_idx;
	const char *mask_start;
	static const u8 exact_mask[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	char mask_str[18];

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "MAC list file '%s' not found.", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		int rem = 0;

		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;
		pos = buf;
		if (buf[0] == '-') {
			rem = 1;
			pos++;
		}

		if (hwaddr_aton(pos, addr)) {
			wpa_printf(MSG_ERROR, "Invalid MAC address '%s", pos);
			fclose(f);
			return -1;
		}

		if (rem) {
			hostapd_remove_acl_mac(acl, num, addr);
			continue;
		}

		/* Initialize mask to exact match by default */
		os_memcpy(mask, exact_mask, ETH_ALEN);
		has_mask = false;
		vlan_id = 0;

		/* Skip past the MAC address */
		pos = buf;
		while (*pos != '\0' && *pos != ' ' && *pos != '\t')
			pos++;

		/* Skip whitespace */
		while (*pos == ' ' || *pos == '\t')
			pos++;

		/* Check if next token is a mask (contains colons) or VLAN ID (numeric) */
		if (*pos != '\0') {
			mask_pos = pos;
			/* Check if this looks like a MAC address (has colons) */
			if (os_strchr(mask_pos, ':') != NULL) {
				/* Try to parse as mask */
				mask_idx = 0;
				mask_start = mask_pos;
				/* Extract mask string */
				while (*mask_pos != '\0' && *mask_pos != ' ' &&
				       *mask_pos != '\t' && mask_idx < 17) {
					mask_str[mask_idx++] = *mask_pos++;
				}
				mask_str[mask_idx] = '\0';

				/* Validate that we stopped at a delimiter, not buffer limit */
				if (mask_idx >= 17 && *mask_pos != '\0' &&
				    *mask_pos != ' ' && *mask_pos != '\t') {
					wpa_printf(MSG_ERROR,
						   "MAC mask too long (exceeds 17 chars) ");
					fclose(f);
					return -1;
				}

				/* Validate mask has correct length for MAC address */
				if (mask_idx != 17) {
					wpa_printf(MSG_ERROR,
						   "MAC mask has incorrect length (%d, expected 17) ",
						    mask_idx);
					fclose(f);
					return -1;
				}
				if (hwaddr_aton(mask_str, mask) == 0) {
					has_mask = true;
					pos = mask_pos;
					/* Skip whitespace after mask */
					while (*pos == ' ' || *pos == '\t')
						pos++;
				} else {
					wpa_printf(MSG_ERROR,
						   "Invalid MAC mask '%s' (from position '%s')",
						   mask_str, mask_start);
					fclose(f);
					return -1;
				}
			}
			/* Parse VLAN ID if present */
			if (*pos != '\0')
				vlan_id = atoi(pos);
		}

		if (hostapd_add_acl_maclist(acl, num, vlan_id, addr) < 0) {
			fclose(f);
			return -1;
		}

		/* Set the mask for the newly added entry */
		if (has_mask && *acl)
			os_memcpy((*acl)[*num - 1].mask, mask, ETH_ALEN);
	}

	fclose(f);

	if (*acl)
		qsort(*acl, *num, sizeof(**acl), hostapd_acl_comp);

	return 0;
}



static bool hostapd_nr_bssid_is_colocated(struct hostapd_data *hapd,
					  const u8 *bssid)
{
	struct hapd_interfaces *ifaces = hapd->iface->interfaces;
	size_t i, b;

	if (!ifaces)
		return false;

	for (i = 0; i < ifaces->count; i++) {
		struct hostapd_iface *other = ifaces->iface[i];

		if (!other)
			continue;
		for (b = 0; b < other->num_bss; b++) {
			if (other->bss[b] &&
			    os_memcmp(bssid, other->bss[b]->own_addr,
				      ETH_ALEN) == 0)
				return true;
		}
	}
	return false;
}

static size_t hostapd_eid_nr_db_len(struct hostapd_data *hapd,
				    size_t *current_len, u8 *num_rnr)
{
	struct hostapd_neighbor_entry *nr;
	size_t total_len = 0, len = *current_len;
	u8 max_rnr = hapd->conf->rnr_ie_allowed;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (!nr->nr || wpabuf_len(nr->nr) < 12)
			continue;

		if (nr->short_ssid == hapd->conf->ssid.short_ssid)
			continue;

		/* Skip co-located BSSes — counted by hostapd_eid_rnr_colocation_len() */
		if (hostapd_nr_bssid_is_colocated(hapd, nr->bssid))
			continue;

		/* Start a new element */
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN > 255) {
			if (max_rnr > 0 && num_rnr && *num_rnr >= max_rnr)
				break;

			len = RNR_HEADER_LEN;
			total_len += RNR_HEADER_LEN;
			if (num_rnr)
				(*num_rnr)++;
		}

		len += RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN;
		total_len += RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN;
	}

	*current_len = len;
	return total_len;
}


#ifdef CONFIG_IEEE80211BE
static bool hostapd_mbssid_mld_match(struct hostapd_data *tx_hapd,
				     struct hostapd_data *ml_hapd,
				     u8 *match_idx)
{
	size_t bss_idx;
	struct hostapd_data *bss;
	size_t num_bss;

	if (!ml_hapd->conf->mld_ap)
		return false;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(ml_hapd->conf))
		return false;
#endif /* CONFIG_QCN_EXTN */

	if (!tx_hapd->iconf->mbssid || tx_hapd->iface->num_bss <= 1) {
		if (hostapd_is_ml_partner(tx_hapd, ml_hapd)) {
			if (match_idx)
				*match_idx = 0;
			return true;
		}

		return false;
	}

	num_bss = hostapd_get_mbssid_max_num_bss(tx_hapd);

	for (bss_idx = 0; bss_idx < num_bss; bss_idx++) {
		if (tx_hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(tx_hapd->mbssid_group,
							  bss_idx);
		else
			bss = tx_hapd->iface->bss[bss_idx];

		if (!bss)
			continue;

		if (hostapd_is_ml_partner(bss, ml_hapd)) {
#ifdef CONFIG_QCN_EXTN
			if (hostapd_is_repurpose_disabled_11be_extn(bss->conf))
				continue;
#endif /* CONFIG_QCN_EXTN */
			if (match_idx)
				*match_idx = bss->mbssid_idx;
			return true;
		}
	}

	return false;
}
#endif /* CONFIG_IEEE80211BE */


struct mbssid_ie_profiles {
	u8 start;
	u8 end;
};

static bool hostapd_skip_rnr(size_t i, struct mbssid_ie_profiles *skip_profiles,
			     bool ap_mld, u8 tbtt_info_len, bool mld_update,
			     struct hostapd_data *reporting_hapd,
			     struct hostapd_data *bss, u8 *match_idx)
{
	bool reporting_ap_mld = false;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	reporting_ap_mld =
		(reporting_hapd->conf->mld_ap &&
		 !hostapd_is_repurpose_disabled_11be_extn(reporting_hapd->conf));
#else
	reporting_ap_mld = !!reporting_hapd->conf->mld_ap;
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	if (!mld_update && skip_profiles &&
	    i >= skip_profiles->start && i < skip_profiles->end)
		return true;

	/* No need to report if length is for normal TBTT and both the reporting
	 * AP and neighbor AP are affiliated with an AP MLD. MLD TBTT will
	 * include this. */
	if (tbtt_info_len == RNR_TBTT_INFO_LEN && ap_mld && reporting_ap_mld)
		return true;

	/* No need to report if length is for MLD TBTT and the BSS is not
	 * affiliated with an aP MLD. Normal TBTT will include this. */
	if ((tbtt_info_len == RNR_TBTT_INFO_MLD_LEN ||
	     tbtt_info_len == RNR_TBTT_INFO_MLD_ECU_LEN) && !ap_mld)
		return true;

#ifdef CONFIG_IEEE80211BE
	/* If building for co-location and they are ML partners, no need to
	 * include since the ML RNR will carry this. */
	if (!mld_update &&
#ifdef CONFIG_QCN_EXTN
	    !hostapd_is_repurpose_disabled_11be_extn(reporting_hapd->conf) &&
	    !hostapd_is_repurpose_disabled_11be_extn(bss->conf) &&
#endif /* CONFIG_QCN_EXTN */
	    hostapd_is_ml_partner(reporting_hapd, bss))
		return true;

	/* If building for ML RNR and they are not ML partners, don't include.
	 */
	if (mld_update &&
	    !hostapd_mbssid_mld_match(reporting_hapd, bss, match_idx))
		return true;

	/* When MLD parameters are added to beacon RNR and in case of EMA
	 * beacons we report only affiliated APs belonging to the reported
	 * non Tx profiles and TX profile will be reported in every EMA beacon.
	 */
	if (mld_update && skip_profiles && match_idx &&
	    (*match_idx < skip_profiles->start ||
	     *match_idx >= skip_profiles->end))
		return true;
	/* When 6GHz is in STANDALONE MODE with MULTI_MBSSID_GROUPING enabled
	 * only the group's RNR info should be reported in FILS discovery, so
	 * don't include the other group's info in RNR.
	 */
	if (!mld_update && (get_colocation_mode(reporting_hapd) == STANDALONE_6GHZ) &&
	    (hostapd_mbssid_get_tx_bss(reporting_hapd) != hostapd_mbssid_get_tx_bss(bss)))
		return true;
#endif /* CONFIG_IEEE80211BE */

	return false;
}


static bool hostapd_rnr_get_bss_info(struct hostapd_data *hapd,
				     struct hostapd_data *reporting_hapd,
				     struct mbssid_ie_profiles *skip_profiles,
				     size_t i, u8 tbtt_info_len,
				     bool mld_update,
				     u8 *op_class, u8 *channel,
				     u8 *match_idx)
{
	struct hostapd_data *bss;
	bool ap_mld = false;
	u8 tmp_match_idx = 255;
	enum oper_chan_width bss_chwidth;
	int secondary_channel;
	u8 seg0, seg1;

	if (!hapd->iface || i >= hapd->iface->num_bss || !op_class || !channel)
		return false;

	bss = hapd->iface->bss[i];
	if (!bss || !bss->conf || !bss->started || bss == reporting_hapd)
		return false;
	if (!bss->beacon_set_done) {
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_bss_rnr_eligible_extn(bss))
#endif /* CONFIG_QCN_EXTN */
			return false;
	}

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	ap_mld = (bss->conf->mld_ap &&
		  !hostapd_is_repurpose_disabled_11be_extn(bss->conf));
#else
	ap_mld = !!bss->conf->mld_ap;
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	/* MLD RNR has to be included for the parameter change count */
	if (bss->conf->ignore_broadcast_ssid && !(ap_mld && mld_update))
		return false;

	if (!match_idx)
		match_idx = &tmp_match_idx;

	if (hostapd_skip_rnr(i, skip_profiles, ap_mld, tbtt_info_len,
			     mld_update, reporting_hapd, bss, match_idx))
		return false;

	hostapd_get_oper_chan_info_of_bss(bss, &bss_chwidth, &seg0, &seg1);
	secondary_channel = bss->iconf->secondary_channel;

	if (seg0 == bss->iconf->channel &&
	    bss_chwidth == CONF_OPER_CHWIDTH_USE_HT)
		secondary_channel = 0;

#ifdef CONFIG_QCN_EXTN
	if ((bss_chwidth == CONF_OPER_CHWIDTH_320MHZ) &&
	    hapd->iconf->downgrade_320mhz_opclass) {
		hostapd_modify_supported_op_class_for_320mhz_extn(
			bss->iface->freq, op_class);
		*channel = bss->iconf->channel;
	} else
#endif /* CONFIG_QCN_EXTN */
	if (ieee80211_freq_to_channel_ext(
		    bss->iface->freq,
		    secondary_channel,
		    bss_chwidth,
		    op_class, channel) == NUM_HOSTAPD_MODES)
		return false;

	return true;
}



static size_t
hostapd_eid_rnr_iface_len(struct hostapd_data *hapd,
			  struct hostapd_data *reporting_hapd,
			  size_t *current_len,
			  struct mbssid_ie_profiles *skip_profiles,
			  bool mld_update, u8 *num_rnr)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t total_len = 0, len = *current_len;
	int total_tbtt_count = 0;
	size_t i;
	u8 tbtt_info_len;
	bool reporting_ap_mld = false;
	bool have_pending_group;
	u8 pending_op_class = 0, pending_channel = 0;
	bool *tbtt_added = NULL;
	u8 max_rnr = reporting_hapd->conf->rnr_ie_allowed;

	if (mld_update)
		tbtt_info_len = hostapd_is_uhr_enabled(hapd) &&
			!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_RNR) ?
			RNR_TBTT_INFO_MLD_ECU_LEN : RNR_TBTT_INFO_MLD_LEN;
	else
		tbtt_info_len = RNR_TBTT_INFO_LEN;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	reporting_ap_mld = (reporting_hapd->conf->mld_ap &&
			    !hostapd_is_repurpose_disabled_11be_extn(reporting_hapd->conf));
#else
	reporting_ap_mld = !!reporting_hapd->conf->mld_ap;
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

repeat_rnr_len:
	os_free(tbtt_added);
	tbtt_added = os_zalloc(iface->num_bss);
	if (!tbtt_added)
		return total_len;

	have_pending_group = false;
	for (;;) {
		int tbtt_count = 0;
		bool group_found = false, group_pending = false;
		u8 rnr_op_class = 0, rnr_channel = 0;

		if (have_pending_group) {
			rnr_op_class = pending_op_class;
			rnr_channel = pending_channel;
			group_found = true;
		}

		if (!group_found) {
			for (i = 0; i < iface->num_bss; i++) {
				if (tbtt_added[i])
					continue;
				if (!hostapd_rnr_get_bss_info(
					hapd, reporting_hapd, skip_profiles,
					i, tbtt_info_len, mld_update,
					&rnr_op_class, &rnr_channel, NULL))
					continue;
				group_found = true;
				break;
			}
		}

		if (!group_found)
			break;

		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + tbtt_info_len > 255) {
			if (max_rnr > 0 && num_rnr && *num_rnr >= max_rnr) {
				*current_len = len;
				return total_len;
			}

			len = RNR_HEADER_LEN;
			total_len += RNR_HEADER_LEN;
			tbtt_count = 0;
			if (num_rnr)
				(*num_rnr)++;
		}

		len += RNR_TBTT_HEADER_LEN;
		total_len += RNR_TBTT_HEADER_LEN;

		for (i = 0; i < iface->num_bss; i++) {
			u8 bss_op_class, bss_channel;

			if (tbtt_added[i])
				continue;

			if (!hostapd_rnr_get_bss_info(
					hapd, reporting_hapd, skip_profiles,
					i, tbtt_info_len, mld_update,
					&bss_op_class, &bss_channel, NULL))
				continue;

			if (rnr_op_class != bss_op_class ||
			    rnr_channel != bss_channel)
				continue;

			if (len + tbtt_info_len > 255 ||
			    tbtt_count >= RNR_TBTT_INFO_COUNT_MAX) {
				group_pending = true;
				break;
			}

			len += tbtt_info_len;
			total_len += tbtt_info_len;
			tbtt_count++;
			tbtt_added[i] = true;
		}

		if (!tbtt_count) {
			len -= RNR_TBTT_HEADER_LEN;
			total_len -= RNR_TBTT_HEADER_LEN;
			break;
		}

		total_tbtt_count += tbtt_count;
		if (group_pending) {
			have_pending_group = true;
			pending_op_class = rnr_op_class;
			pending_channel = rnr_channel;
		} else {
			have_pending_group = false;
		}
	}

	/* If building for co-location, re-build again but this time include
	 * ML TBTTs if the reporting AP is affiliated with an AP MLD.
	 */
	if (!mld_update && tbtt_info_len == RNR_TBTT_INFO_LEN &&
	    reporting_ap_mld) {
		tbtt_info_len = hostapd_is_uhr_enabled(hapd) &&
			!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_RNR) ?
			RNR_TBTT_INFO_MLD_ECU_LEN : RNR_TBTT_INFO_MLD_LEN;
		goto repeat_rnr_len;
	}

	os_free(tbtt_added);

	if (!total_tbtt_count)
		total_len = 0;
	else
		*current_len = len;

	return total_len;
}

static bool hostapd_iface_has_started_bss(struct hostapd_iface *iface)
{
	size_t i;

	if (!iface || !iface->conf || iface->state == HAPD_IFACE_DISABLED)
		return false;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i] && iface->bss[i]->started)
			return true;
	}

	return false;
}

enum colocation_mode get_colocation_mode(struct hostapd_data *hapd)
{
	u8 i;
	bool is_6ghz = is_6ghz_op_class(hapd->iconf->op_class);

	if (!hapd->iface || !hapd->iface->interfaces)
		return NO_COLOCATED_6GHZ;

	if (is_6ghz && hapd->iface->interfaces->count == 1)
		return STANDALONE_6GHZ;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		struct hostapd_iface *iface;
		bool is_colocated_6ghz;

		iface = hapd->iface->interfaces->iface[i];
		if (iface == hapd->iface ||
		    !hostapd_iface_has_started_bss(iface))
			continue;

		is_colocated_6ghz = is_6ghz_op_class(iface->conf->op_class);
		if (!is_6ghz && is_colocated_6ghz)
			return COLOCATED_LOWER_BAND;
		if (is_6ghz && !is_colocated_6ghz)
			return COLOCATED_6GHZ;
	}

	if (is_6ghz)
		return STANDALONE_6GHZ;

	/*
	 * OCE: treat 5G AP with co-located 2.4G BSS as COLOCATED_LOWER_BAND
	 * so that hostapd_eid_rnr_colocation_len/colocation includes them.
	 */
	if (!is_6ghz && OCE_AP_ENABLED(hapd)) {
		for (i = 0; i < hapd->iface->interfaces->count; i++) {
			struct hostapd_iface *iface =
				hapd->iface->interfaces->iface[i];

			if (iface == hapd->iface ||
			    !hostapd_iface_has_started_bss(iface))
				continue;

			/* Include all co-located BSSes (same or different channel) */
			if (!is_6ghz_op_class(iface->conf->op_class))
				return COLOCATED_LOWER_BAND;
		}
	}

	return NO_COLOCATED_6GHZ;
}


static size_t hostapd_eid_rnr_colocation_len(struct hostapd_data *hapd,
					     size_t *current_len,
					     u8 *num_rnr)
{
	struct hostapd_iface *iface;
	size_t len = 0;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return 0;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    iface->state != HAPD_IFACE_ENABLED)
			continue;

		/* Standard: 6GHz co-location. OCE: ALL co-located BSSes */
		if (!is_6ghz_op_class(iface->conf->op_class) &&
		    !OCE_AP_ENABLED(hapd))
			continue;

		if (!iface->num_bss || !iface->bss[0] || !iface->bss[0]->started)
			continue;

		len += hostapd_eid_rnr_iface_len(iface->bss[0], hapd,
						 current_len, NULL, false,
						 num_rnr);
	}

	return len;
}


static size_t hostapd_eid_rnr_mlo_len(struct hostapd_data *hapd, u32 type,
				      struct mbssid_ie_profiles *skip_profiles,
				      size_t *current_len, u8 *num_rnr)
{
	size_t len = 0;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return 0;

	/* TODO: Allow for FILS/Action as well */
	if (type != WLAN_FC_STYPE_BEACON && type != WLAN_FC_STYPE_PROBE_RESP)
		return 0;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    hapd->iface->freq == iface->freq)
			continue;

		len += hostapd_eid_rnr_iface_len(iface->bss[0], hapd,
						 current_len, skip_profiles,
						 true, num_rnr);
	}
#endif /* CONFIG_IEEE80211BE */

	return len;
}


static bool hostapd_add_rnr_non_colocated(struct hostapd_data *hapd, u32 type)
{
	if (!hapd->conf->rnr)
		return false;

	switch (type) {
		case WLAN_FC_STYPE_BEACON:
			return (hapd->conf->rnr & INCLUDE_ELEMENT_IN_BEACON);
		case WLAN_FC_STYPE_PROBE_RESP:
			return (hapd->conf->rnr & INCLUDE_ELEMENT_IN_PROBE_RESP);
		default:
			return false;
	}
}


size_t hostapd_eid_rnr_len(struct hostapd_data *hapd, u32 type,
			   bool include_mld_params)
{
	size_t total_len = 0, current_len = 0;
	enum colocation_mode mode = get_colocation_mode(hapd);
	u8 num_rnr = 0;
#ifdef CONFIG_QCN_EXTN
	bool skip_rnr = hostapd_skip_rnr_6ghz_colocated_extn(hapd, type);

	if (skip_rnr && (type == WLAN_FC_STYPE_ACTION))
		return total_len;
#endif /* CONFIG_QCN_EXTN */

	switch (type) {
	case WLAN_FC_STYPE_BEACON:
		/* fallthrough */
	case WLAN_FC_STYPE_PROBE_RESP:
#ifdef CONFIG_QCN_EXTN
		if (skip_rnr)
			break;
#endif /* CONFIG_QCN_EXTN */
#ifdef RDK_ONEWIFI
        total_len += hostapd_drv_eid_rnr_colocation_len(hapd,
                                                   &current_len);
#endif
		if (mode == COLOCATED_LOWER_BAND)
			total_len +=
				hostapd_eid_rnr_colocation_len(hapd,
							       &current_len,
							       &num_rnr);

		if (hapd->conf->rnr && hapd->iface->num_bss > 1 &&
		    !hapd->iconf->mbssid)
			total_len += hostapd_eid_rnr_iface_len(hapd, hapd,
							       &current_len,
							       NULL, false,
							       &num_rnr);
		break;
	case WLAN_FC_STYPE_ACTION:
		if (hapd->iface->num_bss > 1 && mode == STANDALONE_6GHZ)
			total_len += hostapd_eid_rnr_iface_len(hapd, hapd,
							       &current_len,
							       NULL, false,
							       &num_rnr);
		break;
	}

	/* For EMA Beacons, MLD neighbor repoting is added as part of
	 * MBSSID RNR. For repurposed link under MLD, skip adding ML TBTT.
	 */
	if (include_mld_params &&
#ifdef CONFIG_QCN_EXTN
	    !hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
#endif /* CONFIG_QCN_EXTN */
	    (type != WLAN_FC_STYPE_BEACON ||
	     hapd->iconf->mbssid != ENHANCED_MBSSID_ENABLED))
		total_len += hostapd_eid_rnr_mlo_len(hapd, type, NULL,
						     &current_len,
						     &num_rnr);

	if (hostapd_add_rnr_non_colocated(hapd, type))
		total_len += hostapd_eid_nr_db_len(hapd, &current_len, &num_rnr);

	return total_len;
}

s8 hostapd_get_20mhz_psd_for_rnr(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 ap_pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	u16 freq = iface->freq;
	u8 client_mode;
	s8 result;

	if (!is_6ghz_freq(freq))
		return CHAN_MIN_TX_POWER;

	switch (ap_pwr_type) {
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR:
		client_mode = NL80211_REG_REGULAR_CLIENT_LPI;
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_SP:
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP:
		client_mode = NL80211_REG_REGULAR_CLIENT_SP;
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_VLP:
		client_mode = NL80211_REG_REGULAR_CLIENT_VLP;
		break;
	default:
		return CHAN_MIN_TX_POWER;
	}

	if (ap_pwr_type != HE_REG_INFO_6GHZ_AP_TYPE_SP) {
		s16 reg_psd;
		int ret;

		ret = hostapd_reg_get_psd_from_chan_list(iface, freq,
							 freq, CHWIDTH_20,
							 0, ap_pwr_type,
							 client_mode, true,
							 false, &reg_psd);
		if (ret) {
			wpa_printf(MSG_WARNING, "Failed to calculate reg PSD for frequency %d",
				   freq);
			return CHAN_MIN_TX_POWER;
		}

		return reg_psd * 2;
	}

	result = get_sp_psd_for_non_punctured_chan(hapd, freq, client_mode,
						   ap_pwr_type, REGULATORY_CLIENT_EIRP_PSD);

	if (result != CHAN_MIN_TX_POWER)
		return result * 2;

	return CHAN_MIN_TX_POWER;
}

static u8 * hostapd_eid_nr_db(struct hostapd_data *hapd, u8 *eid,
			      size_t *current_len, u8 *num_rnr)
{
	struct hostapd_neighbor_entry *nr;
	size_t len = *current_len;
	u8 *size_offset = (eid - len) + 1;
	u8 max_rnr = hapd->conf->rnr_ie_allowed;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (!nr->nr || wpabuf_len(nr->nr) < 12)
			continue;

		if (nr->short_ssid == hapd->conf->ssid.short_ssid)
			continue;

		/* Skip co-located BSSes — included by hostapd_eid_rnr_colocation() */
		if (hostapd_nr_bssid_is_colocated(hapd, nr->bssid))
			continue;

		/* Start a new element */
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN > 255) {
			if (max_rnr > 0 && num_rnr && *num_rnr >= max_rnr)
				break;

			*eid++ = WLAN_EID_REDUCED_NEIGHBOR_REPORT;
			size_offset = eid++;
			len = RNR_HEADER_LEN;
			if (num_rnr)
				(*num_rnr)++;
		}

		/* TBTT Information Header subfield (2 octets) */
		*eid++ = 0;
		/* TBTT Information Length */
		*eid++ = RNR_TBTT_INFO_LEN;
		/* Operating Class */
		*eid++ = wpabuf_head_u8(nr->nr)[10];
		/* Channel Number */
		*eid++ = wpabuf_head_u8(nr->nr)[11];
		len += RNR_TBTT_HEADER_LEN;
		/* TBTT Information Set */
		/* TBTT Information field */
		/* Neighbor AP TBTT Offset */
		*eid++ = RNR_NEIGHBOR_AP_OFFSET_UNKNOWN;
		/* BSSID */
		os_memcpy(eid, nr->bssid, ETH_ALEN);
		eid += ETH_ALEN;
		/* Short SSID */
		os_memcpy(eid, &nr->short_ssid, 4);
		eid += 4;
		/* BSS parameters */
		*eid++ = nr->bss_parameters;
		/* 20 MHz PSD */
		if (is_6ghz_op_class(hapd->iface->conf->op_class))
			*eid++ = hapd->iface->rnr_psd;
		else
			*eid++ = RNR_20_MHZ_PSD_MAX_TXPOWER;

		len += RNR_TBTT_INFO_LEN;
		*size_offset = (eid - size_offset) - 1;
	}

	*current_len = len;
	return eid;
}


static bool hostapd_eid_rnr_bss(struct hostapd_data *hapd,
				struct hostapd_data *reporting_hapd,
				size_t i, u8 *tbtt_count, size_t *len,
				u8 **pos, u8 **tbtt_count_pos, u8 tbtt_info_len,
				u8 op_class, u8 channel, u8 match_idx, u32 type)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data *bss = iface->bss[i];
	u8 bss_param = 0;
	bool ap_mld = false;
	u8 *eid = *pos;
	int bss_wiphy_idx, reporting_wiphy_idx;

	if (!bss || !bss->conf || bss == reporting_hapd)
		return false;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	ap_mld = (bss->conf->mld_ap &&
		  !hostapd_is_repurpose_disabled_11be_extn(bss->conf));
#else
	ap_mld = !!bss->conf->mld_ap;
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	if (*len + tbtt_info_len > 255 ||
	    *tbtt_count >= RNR_TBTT_INFO_COUNT_MAX)
		return true;

	if (!(*tbtt_count)) {
		/* Add neighbor report header info only if there is at least
		 * one TBTT info available. */
		*tbtt_count_pos = eid++;
		*eid++ = tbtt_info_len;
		*eid++ = op_class;
		*eid++ = channel;
		*len += RNR_TBTT_HEADER_LEN;
	}

	bss_wiphy_idx = hostapd_drv_get_wiphy_idx(bss);
	reporting_wiphy_idx = hostapd_drv_get_wiphy_idx(reporting_hapd);

	if ((bss->iface->drv_flags2 & WPA_DRIVER_FLAGS2_BEACON_TX_SYNC) &&
	    (bss->iconf->beacon_int == reporting_hapd->iconf->beacon_int) &&
	    (bss_wiphy_idx >= 0 && bss_wiphy_idx == reporting_wiphy_idx)) {
		*eid++ = 0;
	} else {
		*eid++ = RNR_NEIGHBOR_AP_OFFSET_UNKNOWN;
	}

	os_memcpy(eid, bss->own_addr, ETH_ALEN);
	eid += ETH_ALEN;
	os_memcpy(eid, &bss->conf->ssid.short_ssid, 4);
	eid += 4;
	if (bss->conf->ssid.short_ssid == reporting_hapd->conf->ssid.short_ssid)
		bss_param |= RNR_BSS_PARAM_SAME_SSID;

	if (iface->conf->mbssid != MBSSID_DISABLED && iface->num_bss > 1) {
		bss_param |= RNR_BSS_PARAM_MULTIPLE_BSSID;
		if (bss == hostapd_mbssid_get_tx_bss(bss))
			bss_param |= RNR_BSS_PARAM_TRANSMITTED_BSSID;
	}

	if (is_6ghz_op_class(op_class) &&
	    bss->conf->unsol_bcast_probe_resp_interval)
		bss_param |= RNR_BSS_PARAM_UNSOLIC_PROBE_RESP_ACTIVE;

	bss_param |= RNR_BSS_PARAM_CO_LOCATED;

#ifdef CONFIG_QCN_EXTN
	/* RNR memeber ess colocated indication in bss param */
	if (hapd->iconf->rnr_colocated_ess &&
	    is_6ghz_op_class(op_class))
		bss_param |= RNR_BSS_PARAM_MEMBER_CO_LOCATED_ESS;
#endif /* CONFIG_QCN_EXTN */

	*eid++ = bss_param;
	/* 20 MHz PSD */
	if (is_6ghz_op_class(op_class))
		*eid++ = iface->rnr_psd;
	else
		*eid++ = RNR_20_MHZ_PSD_MAX_TXPOWER;


#ifdef CONFIG_IEEE80211BE
	/* Include the MLD parameters only when TBTT length is for ML RNR */
	if (ap_mld && (tbtt_info_len == RNR_TBTT_INFO_MLD_LEN ||
		       tbtt_info_len == RNR_TBTT_INFO_MLD_ECU_LEN)) {
		u8 param_ch = 0;
		/* If BSS is not a partner of the reporting_hapd or
		 * it is one of the nontransmitted hapd,
		 *  a) MLD ID advertised shall be 255.
		 *  b) Link ID advertised shall be 15.
		 *  c) BPCC advertised shall be 255 */

  		/* If atleast one of the MLD params is Unknown, set Unknown for all
		 * mld params.
		 */
		if (type != WLAN_FC_STYPE_BEACON)
			param_ch = bss->rx_cu_param.bpcc;

		if ((match_idx == 0xff) || (bss->mld_link_id == 0xf) ||
		    (param_ch == 0xff)) {
			*eid++ = 0xff;
			*eid++ = 0xff;
			*eid = 0xf;
		} else {
			/* MLD ID */
			*eid++ = match_idx;
			/* TODO colocated bss match + MBSSID + MLO case */
			/* Link ID */
			*eid++ = (bss->mld_link_id & 0xf) |
				 (param_ch & 0xf) << 4;
			/* BPCC */
			*eid = (param_ch & 0xf0) >> 4;
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (bss->conf->mld_indicate_disabled)
			*eid |= RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
#endif /* CONFIG_TESTING_OPTIONS */
		if (type == WLAN_FC_STYPE_PROBE_RESP &&
		    bss->mld &&
		    BIT(bss->mld_link_id) &
		    bss->mld->ttlm_ctx.established_ttlm.disabled_link_bitmap)
			*eid |= RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
		eid++;

		/* Enhanced Critical Updates Information (IEEE P802.11bn/D2.0, Figure 9-803a) */
		if (tbtt_info_len == RNR_TBTT_INFO_MLD_ECU_LEN) {
			u8 ecu_info = (type != WLAN_FC_STYPE_BEACON) ?
				hostapd_get_ecu_info(&bss->rx_ecu_param) : 0;

			*eid++ = ecu_info;
		}

	}
#endif /* CONFIG_IEEE80211BE */

	*len += tbtt_info_len;
	(*tbtt_count)++;
	*pos = eid;

	return false;
}


static u8 * hostapd_eid_rnr_iface(struct hostapd_data *hapd,
				  struct hostapd_data *reporting_hapd,
				  u8 *eid, size_t *current_len,
				  struct mbssid_ie_profiles *skip_profiles,
				  bool mld_update, u32 type,
				  u8 *num_rnr)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t i;
	size_t len = *current_len;
	u8 *eid_start = eid, *size_offset = (eid - len) + 1;
	u8 *tbtt_count_pos = size_offset + 1;
	u8 total_tbtt_count = 0;
	u8 tbtt_info_len;
	bool reporting_ap_mld = false;
	bool have_pending_group;
	u8 pending_op_class = 0, pending_channel = 0;
	bool *tbtt_added = NULL;
	u8 max_rnr = reporting_hapd->conf->rnr_ie_allowed;

	if (mld_update)
		tbtt_info_len = hostapd_is_uhr_enabled(hapd) &&
			!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_RNR) ?
			RNR_TBTT_INFO_MLD_ECU_LEN : RNR_TBTT_INFO_MLD_LEN;
	else
		tbtt_info_len = RNR_TBTT_INFO_LEN;

	if (!(iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA) || !iface->freq)
		return eid;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	reporting_ap_mld =
		(reporting_hapd->conf->mld_ap &&
		 !hostapd_is_repurpose_disabled_11be_extn(reporting_hapd->conf));
#else
	reporting_ap_mld = !!reporting_hapd->conf->mld_ap;
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

repeat_rnr:
	os_free(tbtt_added);
	tbtt_added = os_zalloc(iface->num_bss);
	if (!tbtt_added)
		return eid;

	have_pending_group = false;
	for (;;) {
		u8 tbtt_count = 0;
		bool group_found = false, group_pending = false;
		u8 rnr_op_class = 0, rnr_channel = 0;

		if (have_pending_group) {
			rnr_op_class = pending_op_class;
			rnr_channel = pending_channel;
			group_found = true;
		}

		if (!group_found) {
			for (i = 0; i < iface->num_bss; i++) {
				if (tbtt_added[i])
					continue;
				if (!hostapd_rnr_get_bss_info(
					hapd, reporting_hapd, skip_profiles,
					i, tbtt_info_len, mld_update,
					&rnr_op_class, &rnr_channel, NULL))
					continue;
				group_found = true;
				break;
			}
		}

		if (!group_found)
			break;

		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + tbtt_info_len > 255) {
			if (max_rnr > 0 && num_rnr && *num_rnr >= max_rnr) {
				*current_len = len;
				return eid;
			}

			eid_start = eid;
			*eid++ = WLAN_EID_REDUCED_NEIGHBOR_REPORT;
			size_offset = eid++;
			len = RNR_HEADER_LEN;
			tbtt_count = 0;
			if (num_rnr)
				(*num_rnr)++;
		}

		for (i = 0; i < iface->num_bss; i++) {
			u8 op_class, channel, match_idx = 255;

			if (tbtt_added[i])
				continue;

			if (!hostapd_rnr_get_bss_info(
					hapd, reporting_hapd, skip_profiles,
					i, tbtt_info_len, mld_update,
					&op_class, &channel, &match_idx))
				continue;

			if (rnr_op_class != op_class ||
			    rnr_channel != channel)
				continue;

			if (hostapd_eid_rnr_bss(hapd, reporting_hapd, i,
						&tbtt_count, &len, &eid,
						&tbtt_count_pos, tbtt_info_len,
						op_class, channel, match_idx,
						type)) {
				group_pending = true;
				break;
			}

			tbtt_added[i] = true;
		}

		if (tbtt_count) {
			*tbtt_count_pos = RNR_TBTT_INFO_COUNT(tbtt_count - 1);
			*size_offset = (eid - size_offset) - 1;
		} else {
			break;
		}

		total_tbtt_count += tbtt_count;

		if (group_pending) {
			have_pending_group = true;
			pending_op_class = rnr_op_class;
			pending_channel = rnr_channel;
		} else {
			have_pending_group = false;
		}
	}

	/* If building for co-location, re-build again but this time include
	 * ML TBTTs if the reporting AP is affiliated with an AP MLD.
	 */
	if (!mld_update && tbtt_info_len == RNR_TBTT_INFO_LEN &&
	    reporting_ap_mld) {
		tbtt_info_len = hostapd_is_uhr_enabled(hapd) &&
			!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_RNR) ?
			RNR_TBTT_INFO_MLD_ECU_LEN : RNR_TBTT_INFO_MLD_LEN;
		goto repeat_rnr;
	}

	os_free(tbtt_added);

	if (!total_tbtt_count)
		return eid_start;

	*current_len = len;
	return eid;
}


static u8 * hostapd_eid_rnr_colocation(struct hostapd_data *hapd, u8 *eid,
				       size_t *current_len, u32 type,
				       u8 *num_rnr)
{
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return eid;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    iface->state != HAPD_IFACE_ENABLED)
			continue;

		/* Standard: 6GHz. OCE: ALL co-located BSSes */
		if (!is_6ghz_op_class(iface->conf->op_class) &&
		    !OCE_AP_ENABLED(hapd))
			continue;

		/*
		 * When bss[0] is disabled using disable_bss command,
		 * it is not destroyed it just down and it shouldn't skip
		 * updating other colocated BSSs in this iface which are
		 * up and beaconing.
		 */
		if (!iface->num_bss || !iface->bss[0])
			continue;

		eid = hostapd_eid_rnr_iface(iface->bss[0], hapd, eid,
					    current_len, NULL, false, type,
					    num_rnr);
	}

	return eid;
}


static u8 * hostapd_eid_rnr_mlo(struct hostapd_data *hapd, u32 type,
				u8 *eid,
				struct mbssid_ie_profiles *skip_profiles,
				size_t *current_len, u8 *num_rnr)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return eid;

	/* TODO: Allow for FILS/Action as well */
	if (type != WLAN_FC_STYPE_BEACON && type != WLAN_FC_STYPE_PROBE_RESP)
		return eid;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    hapd->iface->freq == iface->freq)
			continue;

		if (!iface->num_bss)
			continue;

		eid = hostapd_eid_rnr_iface(iface->bss[0], hapd, eid,
					    current_len, skip_profiles, true,
					    type, num_rnr);
	}
#endif /* CONFIG_IEEE80211BE */

	return eid;
}


u8 * hostapd_eid_rnr(struct hostapd_data *hapd, u8 *eid, u32 type,
		     bool include_mld_params)
{
	u8 *eid_start = eid, num_rnr = 0;
	size_t current_len = 0;
	enum colocation_mode mode = get_colocation_mode(hapd);
#ifdef CONFIG_QCN_EXTN
	bool skip_rnr = hostapd_skip_rnr_6ghz_colocated_extn(hapd, type);

	if (skip_rnr && (type == WLAN_FC_STYPE_ACTION))
		return eid_start;
#endif /* CONFIG_QCN_EXTN */

	switch (type) {
	case WLAN_FC_STYPE_BEACON:
		/* fallthrough */
	case WLAN_FC_STYPE_PROBE_RESP:
#ifdef CONFIG_QCN_EXTN
		if (skip_rnr)
			break;
#endif /* CONFIG_QCN_EXTN */

#ifdef RDK_ONEWIFI
        eid = hostapd_drv_eid_rnr_colocation(hapd, eid, &current_len);
#endif
		if (mode == COLOCATED_LOWER_BAND)
			eid = hostapd_eid_rnr_colocation(hapd, eid,
							 &current_len, type,
							 &num_rnr);

		if (hapd->conf->rnr && hapd->iface->num_bss > 1 &&
		    !hapd->iconf->mbssid)
			eid = hostapd_eid_rnr_iface(hapd, hapd, eid,
						    &current_len, NULL, false,
						    type, &num_rnr);
		break;
	case WLAN_FC_STYPE_ACTION:
		if (hapd->iface->num_bss > 1 && mode == STANDALONE_6GHZ)
			eid = hostapd_eid_rnr_iface(hapd, hapd, eid,
						    &current_len, NULL, false,
						    type, &num_rnr);
		break;
	default:
		return eid_start;
	}

	/* For EMA Beacons, MLD neighbor repoting is added as part of
	 * MBSSID RNR. Skip including MLD TBTT if repurposed to lower mode
	 */
	if (include_mld_params &&
#ifdef CONFIG_QCN_EXTN
	    !hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
#endif /* CONFIG_QCN_EXTN */
	    (type != WLAN_FC_STYPE_BEACON ||
	     hapd->iconf->mbssid != ENHANCED_MBSSID_ENABLED))
		eid = hostapd_eid_rnr_mlo(hapd, type, eid, NULL, &current_len,
					  &num_rnr);

	if (hostapd_add_rnr_non_colocated(hapd, type))
		eid = hostapd_eid_nr_db(hapd, eid, &current_len, &num_rnr);

	if (eid == eid_start + 2)
		return eid_start;

	return eid;
}


static bool mbssid_known_bss(unsigned int i, const u8 *known_bss,
			     size_t known_bss_len)
{
	if (!known_bss || known_bss_len <= i / 8)
		return false;
	known_bss = &known_bss[i / 8];
	return *known_bss & (u8) (BIT(i % 8));
}

static bool ieee802_11_mbssid_is_elem_inherited(u8 id, u8 ext_id, bool is_non_tx)
{
	switch (id) {
	case WLAN_EID_EXTENSION:
		switch (ext_id) {
		case WLAN_EID_EXT_HE_CAPABILITIES:
		case WLAN_EID_EXT_HE_OPERATION:
		case WLAN_EID_EXT_HE_6GHZ_BAND_CAP:
		case WLAN_EID_EXT_COLOR_CHANGE_ANNOUNCEMENT:
		case WLAN_EID_EXT_SPATIAL_REUSE:
		case WLAN_EID_EXT_MAX_CHANNEL_SWITCH_TIME:
		case WLAN_EID_EXT_MULTIPLE_BSSID_CONFIGURATION:
		case WLAN_EID_EXT_MULTI_LINK:
		case WLAN_EID_EXT_EHT_CAPABILITIES:
		case WLAN_EID_EXT_EHT_OPERATION:
		case WLAN_EID_EXT_UHR_CAPABILITIES:
		case WLAN_EID_EXT_UHR_OPERATION:
			break;
		default:
			return false;
		}
		break;
	case WLAN_EID_SSID:
	case WLAN_EID_MULTIPLE_BSSID:
	case WLAN_EID_TIM:
	case WLAN_EID_DS_PARAMS:
	case WLAN_EID_IBSS_PARAMS:
	case WLAN_EID_COUNTRY:
	case WLAN_EID_CHANNEL_SWITCH:
	case WLAN_PA_EXT_CHANNEL_SWITCH_ANNOUNCE:
	case WLAN_EID_WIDE_BW_CHSWITCH:
	case WLAN_EID_TRANSMIT_POWER_ENVELOPE:
	case WLAN_EID_SUPPORTED_OPERATING_CLASSES:
	case WLAN_EID_IBSS_DFS:
	case WLAN_EID_ERP_INFO:
	case WLAN_EID_REDUCED_NEIGHBOR_REPORT:
	case WLAN_EID_HT_CAP:
	case WLAN_EID_HT_OPERATION:
	case WLAN_EID_VHT_CAP:
	case WLAN_EID_VHT_OPERATION:
	case WLAN_EID_S1G_BCN_COMPAT:
	case WLAN_EID_S1G_OPERATION:
	case WLAN_EID_S1G_CAPABILITIES:
	case WLAN_EID_QUIET:
	case WLAN_EID_QUIET_CHANNEL:
	case WLAN_EID_TPC_REPORT:
	case WLAN_EID_VENDOR_SPECIFIC:
		if (is_non_tx == true)
			return false;
	case WLAN_EID_MMIE:
		break;
	default:
		return false;
	}

	return true;
}

static u8 * ieee802_11_inheritance_txbss_params(u8 *tx_elem, size_t tx_elem_len, u8 *tx_head,
						size_t tx_head_len, u8 *nontx_elem,
						size_t nontx_elem_len, u8 *eid,
						struct non_inheritance_elem *non_inherit_ie,
						ssize_t *optional_ie_len, u32 frame_type,
						struct hostapd_data *bss)
{
	const struct element *tx_ie, *nontx_ie;
	const u8 *data, *nontx_data;
	u8 id, len, nontx_id, nontx_len, ext_id, nontx_ext_id;
	bool tx_vendor_ie = false;
	u8 *pos = eid, parsed_eid_bmap[32] = { 0 }, parsed_ext_eid_bmap[32] = {0};
	size_t nontx_prof_len = 0, total_non_inherit_ie_len = 0;
	bool found_in_nontx_bss;

	if (nontx_elem_len < 2 || tx_elem_len < 2) {
		wpa_printf(MSG_ERROR, "Invalid length Non_tx:%zu, Tx:%zu",
			   nontx_elem_len, tx_elem_len);
		goto fail;
	}

	/*
	 * Compare Supported Rates element in the Tx BSS's head and Non‑Tx
	 * BSS for beacon frames to decide whether to include this element
	 * in the Non‑Tx MBSSID profile or let it inherit from the Tx BSS.
	 */
	if (frame_type == WLAN_FC_STYPE_BEACON) {
		if (tx_head_len < 2) {
			wpa_printf(MSG_ERROR, "Invalid length tx_head_len:%zu",
				   tx_head_len);
			goto fail;
		}

		for_each_element(tx_ie, tx_head, tx_head_len) {
			id = tx_ie->id;
			len = tx_ie->datalen;
			data = tx_ie->data;
			found_in_nontx_bss = false;

			if (2 + len > tx_head_len) {
				wpa_printf(MSG_ERROR,
					   "Truncated TX BSS head element len:%u tx_head_len:%zu",
					    len + 2, tx_head_len);
				goto fail;
			}

			if (len <= 0)
				continue;

			if (id != WLAN_EID_SUPP_RATES)
				continue;

			for_each_element(nontx_ie, nontx_elem, nontx_elem_len) {
				nontx_id  = nontx_ie->id;
				nontx_len = nontx_ie->datalen;
				nontx_data = nontx_ie->data;

				if (nontx_len <= 0)
					continue;

				if (2 + nontx_len > nontx_elem_len) {
					wpa_printf(MSG_ERROR,
						   "Truncated Non-Tx BSS element len:%u nontx_head_len:%zu",
						   nontx_len + 2, nontx_elem_len);
					goto fail;
				}

				if (id == nontx_id) {
					parsed_eid_bmap[id / 8] |= BIT(id % 8);

					found_in_nontx_bss = true;

					if (nontx_len == len &&
					    os_memcmp(data, nontx_data, nontx_len) == 0) {
						wpa_printf(MSG_DEBUG,
							   "Element (%u) data matches with Tx BSS",
							   nontx_id);
						break;
					}

					wpa_printf(MSG_DEBUG,
						   "Element (%u) data doesn't match with Tx BSS",
						   nontx_id);
					/*
					 * The length is computed before constructing the MBSSID
					 * elements. The allocated memory accounts for optional
					 * elements where the Non‑Tx BSS has the elements not Tx
					 * BSS. As a result, the length is not validated during
					 * the actual MBSSID element construction.
					 */
					if (!pos) {
						if (nontx_len + IEEE80211_ELEM_HEADER_LEN >
						    MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss) -
						    nontx_prof_len) {
							wpa_printf(MSG_ERROR,
								   "Inheritance: Unable to add Element (%u) "
								   "exceeds max limit (%d)",
								   nontx_id,
								   MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss));
							goto fail;
						}
						nontx_prof_len += nontx_len + IEEE80211_ELEM_HEADER_LEN;
					} else {
						/* Append in proper order as found in non-Tx tail */
						os_memcpy(pos, nontx_data - IEEE80211_ELEM_HEADER_LEN,
							  nontx_len + IEEE80211_ELEM_HEADER_LEN);
						pos += nontx_len + IEEE80211_ELEM_HEADER_LEN;
					}
					break;
				}
			}

			if (!found_in_nontx_bss) {
				if (non_inherit_ie->elem_len + 1 >= MAX_MBSSID_NONINHERIT_ELEM_SIZE) {
					wpa_printf(MSG_ERROR, "Unable to add Non-inheritance element:%u, reached max limit:%d",
						   id, MAX_MBSSID_NONINHERIT_ELEM_SIZE);
					goto fail;
				}
				non_inherit_ie->elem_list[non_inherit_ie->elem_len++] = id;
			}
			break;
		}
	}

	/*
	 * Compare all elements in the Non‑Tx BSS with those in the Tx BSS's
	 * tail BSS for beacon frames and with the Tx BSS probe response
	 * frames to determine whether the element should be included in the
	 * Non‑Tx MBSSID profile or inherited from the Tx BSS.
	 */
	for_each_element(tx_ie, tx_elem, tx_elem_len) {
		id = tx_ie->id;
		len = tx_ie->datalen;
		data = tx_ie->data;
		found_in_nontx_bss = false;

		if (2 + len > tx_elem_len) {
			wpa_printf(MSG_ERROR,
				   "Truncated TX BSS element len:%u tx_elem_len:%zu",
				   len + 2, tx_elem_len);
			goto fail;
		}

		if (len <= 0)
			continue;

		if (id == WLAN_EID_EXTENSION) {
			ext_id = *(data);
		} else if (id == WLAN_EID_VENDOR_SPECIFIC) {
			/* vendor IEs from tx-vap non-inheritable. so skip
			 * checking entirely.
			 */
			tx_vendor_ie = true;
			continue;
		}

		if (ieee802_11_mbssid_is_elem_inherited(id, ext_id, false) ||
		    (id == WLAN_EID_EXT_CAPAB))
			continue;

		/* Check for duplicates in Non-Tx BSS elements */
		for_each_element(nontx_ie, nontx_elem, nontx_elem_len) {
			nontx_id  = nontx_ie->id;
			nontx_len = nontx_ie->datalen;
			nontx_data = nontx_ie->data;

			if (nontx_len <= 0)
				continue;

			if (2 + nontx_len > nontx_elem_len) {
				wpa_printf(MSG_ERROR,
					   "Truncated Non-Tx BSS element len:%u nontx_elem_len:%zu",
					   nontx_len + 2, nontx_elem_len);
				goto fail;
			}

			if (id == nontx_id) {
				if (id == WLAN_EID_EXTENSION) {
					nontx_ext_id = *(nontx_data);
					if (ext_id != nontx_ext_id)
						continue;
					parsed_ext_eid_bmap[ext_id / 8] |= BIT(ext_id % 8);

				} else {
					parsed_eid_bmap[id / 8] |= BIT(id % 8);
				}

				found_in_nontx_bss = true;

				if (nontx_len == len &&
				    os_memcmp(data, nontx_data, nontx_len) == 0) {
					wpa_printf(MSG_DEBUG, "Element (%u) data matches with Tx BSS",
						   nontx_id);
					break;
				}

				wpa_printf(MSG_DEBUG, "Element:%u data doesn't match with Tx BSS, "
					   "include in Non-Tx BSS profile", nontx_id);

				 /* Boundary is validated only during length calculation */
				if (!pos) {
					if (nontx_len + IEEE80211_ELEM_HEADER_LEN >
					    MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss) -
					    nontx_prof_len) {
						wpa_printf(MSG_ERROR,
							   "Inheritance: Unable to add Element (%u) "
							   "exceeds max limit (%d)",
							   nontx_id,
							   MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss));
						goto fail;
					}
					nontx_prof_len += nontx_len + IEEE80211_ELEM_HEADER_LEN;
				} else {
					/* Append in proper order as found in non-Tx tail */
					os_memcpy(pos, nontx_data - IEEE80211_ELEM_HEADER_LEN,
						  nontx_len + IEEE80211_ELEM_HEADER_LEN);
					pos += nontx_len + IEEE80211_ELEM_HEADER_LEN;
				}
				break;
			}
		}

		if (!found_in_nontx_bss) {
			if (((id == WLAN_EID_EXTENSION) && (non_inherit_ie->ext_elem_len + 1 >=
							    MAX_MBSSID_NONINHERIT_ELEM_SIZE)) ||
			    ((id != WLAN_EID_EXTENSION) && (non_inherit_ie->elem_len + 1 >=
							    MAX_MBSSID_NONINHERIT_ELEM_SIZE))) {
				wpa_printf(MSG_ERROR,
					   "Failed to add Non-inheritance for id:%u, ext_id:%u "
					   "exceeds max limit(%d)",
					   id, ext_id, MAX_MBSSID_NONINHERIT_ELEM_SIZE);
				goto fail;
			}

			if (id == WLAN_EID_EXTENSION)
				non_inherit_ie->ext_elem_list[non_inherit_ie->ext_elem_len++] = ext_id;
			else
				non_inherit_ie->elem_list[non_inherit_ie->elem_len++] = id;
		}
	}

	/* Check for remaining Element in Non-Tx BSS */
	for_each_element(nontx_ie, nontx_elem, nontx_elem_len) {
		nontx_id  = nontx_ie->id;
		nontx_len = nontx_ie->datalen;

		if (2 + nontx_len > nontx_elem_len) {
			wpa_printf(MSG_ERROR,
				   "Truncated Non-Tx BSS element len:%u nontx_elem_len:%zu",
				   nontx_len + 2, nontx_elem_len);
			goto fail;
		}

		if (nontx_len <= 0)
			continue;

		nontx_data = nontx_ie->data;

		if (nontx_id == WLAN_EID_EXTENSION) {
			nontx_ext_id = *(nontx_data);
			if (parsed_ext_eid_bmap[nontx_ext_id / 8] & BIT(nontx_ext_id % 8))
				continue;
			/* Vendors IEs are non-inheritable, So Add all non-tx BSS
			 * vendor IEs into non-tx MBSSID profile
			 */
		} else if (nontx_id != WLAN_EID_VENDOR_SPECIFIC) {
			if (parsed_eid_bmap[nontx_id / 8] & BIT(nontx_id % 8))
				continue;
		}

		if (ieee802_11_mbssid_is_elem_inherited(nontx_id, nontx_ext_id, true))
			continue;

		 /* Boundary is validated only during length calculation */
		if (!pos) {
			if (nontx_len + IEEE80211_ELEM_HEADER_LEN >
			    MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss) - nontx_prof_len) {
				wpa_printf(MSG_ERROR,
					   "Inheritance: Failed to add element:%u to Non-Tx BSS, "
					   "exceeds max limit (%d)",
					   nontx_id, MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss));
				goto fail;
			}
			nontx_prof_len += nontx_len + IEEE80211_ELEM_HEADER_LEN;
		} else {
			os_memcpy(pos, nontx_data - IEEE80211_ELEM_HEADER_LEN,
				  nontx_len + IEEE80211_ELEM_HEADER_LEN);
			pos += nontx_len + IEEE80211_ELEM_HEADER_LEN;
		}
	}

	/*
	 * Vendor elements are not inherited from the TX BSS.
	 * They are always added to the non-inheritance list
	 * to prevent inheritance if there is space.
	 */
	if (non_inherit_ie->elem_len + 1 >= MAX_MBSSID_NONINHERIT_ELEM_SIZE) {
		wpa_printf(MSG_ERROR,
			   "Failed to add Non-inheritance element:%d to Non-Tx BSS, "
			   "exceeds max limit (%d)",
			   WLAN_EID_VENDOR_SPECIFIC, MAX_MBSSID_NONINHERIT_ELEM_SIZE);
		goto fail;

	}
	if (tx_vendor_ie == true)
		non_inherit_ie->elem_list[non_inherit_ie->elem_len++] = WLAN_EID_VENDOR_SPECIFIC;

	/*
	 * Non-inheritance Element length
	 * IEEE80211_ELEM_HEADER_LEN: 2
	 * Ext tag number: 1
	 * Length of Element ID list: 1
	 * Element ID list: Variable
	 * Length of Element ID Extension list: 1
	 * Element ID Extension List: Variable
	 */
	total_non_inherit_ie_len = IEEE80211_ELEM_HEADER_LEN + 1 +
				   1 + non_inherit_ie->elem_len +
				   1 + non_inherit_ie->ext_elem_len;

	if (total_non_inherit_ie_len >
	    MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss) - nontx_prof_len) {
		wpa_printf(MSG_ERROR,
			   "Unable to add non-inheritance elements in frame type:%u, "
			   "non_inherit_ie_len:%zu exceeds max limit:%d",
			   frame_type, total_non_inherit_ie_len,
			   MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss));
		os_memset(non_inherit_ie, 0, sizeof(struct non_inheritance_elem));
		goto fail;
	}

	nontx_prof_len += total_non_inherit_ie_len;

	*optional_ie_len = (ssize_t) nontx_prof_len;

	return pos;

fail:
	wpa_printf(MSG_ERROR, "Inheritance: Insuffient length, frame_type:%u",
		   frame_type);
	*optional_ie_len = -1;
	return NULL;
}

u8 * hostapd_eid_mbssid_nontx_optional_ie(struct hostapd_data *bss, void *tx_params,
					  struct non_inheritance_elem *non_inherit_ie,
					  u8 *eid, ssize_t *nontx_prof_len, u8 frame_type)
{
	struct wpa_driver_ap_params nontx_params;
	struct probe_resp_params nontx_probe_params;
	u8 *tx_elem, *nontx_elem, *tx_head;
	size_t tx_elem_len, nontx_elem_len, tx_head_len;
	size_t fixed_param_len;

	if (!tx_params) {
		wpa_printf(MSG_ERROR, "Tx params is NULL");
		goto fail;
	}

	if (frame_type == WLAN_FC_STYPE_BEACON) {
		struct wpa_driver_ap_params *params =
			(struct wpa_driver_ap_params *) tx_params;
		struct ieee80211_mgmt *mgmt =
			(struct ieee80211_mgmt *) params->head;

		os_memset(&nontx_params, 0, sizeof(nontx_params));
		if (ieee802_11_build_nontx_bss_params(bss, &nontx_params) < 0) {
			wpa_printf(MSG_ERROR, "Failed to build optional elements for Non-Tx BSS %s",
				   bss->conf->iface);
			goto fail;
		}

		/*
		 * head_len = IEEE80211 header + fixed fields + variable-sized elements
		 * Extract fixed params length to derive variable elements length.
		 */
		fixed_param_len = (size_t) ((u8 *) mgmt->u.beacon.variable -
					    (u8 *) mgmt);
		tx_elem = params->tail;
		tx_elem_len = params->tail_len;
		tx_head = mgmt->u.beacon.variable;
		tx_head_len = params->head_len - fixed_param_len;
		nontx_elem = nontx_params.tail;
		nontx_elem_len = nontx_params.tail_len;
	} else {
		struct probe_resp_params *probe_params =
			(struct probe_resp_params *) tx_params;

		os_memset(&nontx_probe_params, 0, sizeof(nontx_probe_params));
		if (ieee802_11_build_nontx_bss_probe_params(bss, &nontx_probe_params) < 0) {
			wpa_printf(MSG_ERROR, "Failed to build optional elements for Non-Tx BSS %s",
				   bss->conf->iface);
			goto fail;

		}

		/*
		 * resp_len = IEE80211 header + fixed fields + variable-sized elements
		 * Extract fixed params length to derive variable elements length.
		 */
		fixed_param_len = (size_t) ((u8 *) probe_params->resp->u.probe_resp.variable -
					    (u8 *) probe_params->resp);
		tx_elem = probe_params->resp->u.probe_resp.variable;
		tx_elem_len = probe_params->resp_len - fixed_param_len;
		nontx_elem = nontx_probe_params.resp->u.probe_resp.variable;
		nontx_elem_len = nontx_probe_params.resp_len - fixed_param_len;
	}

	eid = ieee802_11_inheritance_txbss_params(tx_elem, tx_elem_len,
						  tx_head, tx_head_len,
						  nontx_elem, nontx_elem_len,
						  eid, non_inherit_ie, nontx_prof_len,
						  frame_type, bss);

	if (frame_type == WLAN_FC_STYPE_BEACON)
		os_free(nontx_params.tail);
	else
		os_free(nontx_probe_params.resp);

	return eid;

fail:
	wpa_printf(MSG_ERROR,
		   "Failed to build optional elements for Non-Tx BSS %s, frame_type:%u",
		   bss->conf->iface, frame_type);
	*nontx_prof_len = -1;
	return NULL;
}


static size_t hostapd_eid_mbssid_elem_len(struct hostapd_data *hapd,
					  u32 frame_type, size_t *bss_index,
					  const u8 *known_bss,
					  size_t known_bss_len, size_t num_bss,
					  bool bcast_prb_resp, void *params)
{
	struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	struct probe_resp_params *probe_params = NULL;
	bool is_ml_probe = false;

	u8 ext_cap;
	size_t len, i;

	/* Element ID: 1 octet
	 * Length: 1 octet
	 * MaxBSSID Indicator: 1 octet
	 * Optional Subelements: variable
	 *
	 * Total fixed length: 3 octets
	 *
	 * 1 octet in len for the MaxBSSID Indicator field.
	 */
	len = 1;

	if (frame_type == WLAN_FC_STYPE_PROBE_RESP && params) {
		probe_params = (struct probe_resp_params *) params;
		is_ml_probe = probe_params->is_ml_probe;
	}

	for (i = *bss_index; i < num_bss; i++) {
		struct hostapd_data *bss;
		struct non_inheritance_elem non_inherit_ie;
		size_t rem_vendor_elem_size;
		size_t nontx_profile_len, wmm_len, j;
		ssize_t optional_ie_len = 0;

		if (tx_bss->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(tx_bss->mbssid_group, i);
		else
			bss = tx_bss->iface->bss[i];

		if (!bss || !bss->conf || !bss->started || !bss->beacon_set_done ||
		    mbssid_known_bss(i, known_bss, known_bss_len))
			continue;

		/*
		 * Sublement ID: 1 octet
		 * Length: 1 octet
		 * Nontransmitted capabilities: 4 octets
		 * SSID element: 2 + variable (except for hidden BSS)
		 * Multiple BSSID Index Element: 3 octets (+2 octets in beacons)
		 * Fixed length = 1 + 1 + 4 + 2 + 3 = 11
		 */
		nontx_profile_len = 11;

		if (!bss->conf->ignore_broadcast_ssid ||
		    bss->conf->ignore_broadcast_ssid == 2 ||
		    (frame_type == WLAN_FC_STYPE_PROBE_RESP && bss == hapd))
			nontx_profile_len += bss->conf->ssid.ssid_len;

		/* DTIM period and DTIM Count*/
		if (frame_type == WLAN_FC_STYPE_BEACON)
			nontx_profile_len += 2;

		/* Optional IE and Non-inheritance IE len after applying inheritence logic*/
		os_memset(&non_inherit_ie, 0, sizeof(non_inherit_ie));
		hostapd_eid_mbssid_nontx_optional_ie(bss, params, &non_inherit_ie,
						     NULL, &optional_ie_len, frame_type);
		if (optional_ie_len < 0) {
			wpa_printf(MSG_ERROR,
				   "Failed to calculate the length for optional elements:%s, frame_type:%u",
				   bss->conf->iface, frame_type);
			return 0;
		}

		nontx_profile_len += optional_ie_len;

#ifdef CONFIG_IEEE80211BE
		/* For ML Probe Response frame, the solicited hapd's MLE will
		 * be in the frame body */
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(bss->conf)) {
#endif /* CONFIG_QCN_EXTN */
		if (bss->conf->mld_ap &&
		    (bss != hapd || frame_type != WLAN_FC_STYPE_PROBE_RESP ||
		     !is_ml_probe)) {
			ext_cap = 0;

			if (((frame_type == WLAN_FC_STYPE_PROBE_RESP) &&
			     bcast_prb_resp) || (frame_type == WLAN_FC_STYPE_BEACON)) {
				/* RMSL value sent in broadcast Probe response case and beacon */
				if (bss->conf->enable_aal)
					ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN);

				if (bss->conf->single_link_emlsr &&
				    (bss->iface->mld_ext_mld_capa &
				     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
					ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);
			}

			nontx_profile_len += hostapd_eid_eht_basic_ml_len(
				bss, NULL, true, false, ext_cap, true,
				false, !!(bss->conf->skip_uhr_extn & SKIP_UHR_EXTN_BCN));
			if (bss->eht_mld_link_removal_inprogress)
				nontx_profile_len += hostapd_eid_eht_ml_reconfig_len(bss);
		}
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

		rem_vendor_elem_size = MBSSID_NON_TX_VENDOR_ELEM_SIZE(bss);

		/* WMM IE */
		wmm_len = hostapd_eid_wmm_len(bss);
		if (wmm_len > rem_vendor_elem_size) {
			wpa_printf(MSG_DEBUG,
				   "WMM vendor element size: %zu exceeds available space: %zu",
				   wmm_len, rem_vendor_elem_size);
			return 0;
		}

		nontx_profile_len += wmm_len;
		rem_vendor_elem_size -= wmm_len;

		/* User configured vendor elements */
		for (j = 0; j < bss->conf->vendor_elements_count; j++) {
			struct wpabuf *entry = bss->conf->vendor_elements[j];

			if (wpabuf_len(entry) <= rem_vendor_elem_size) {
				nontx_profile_len += wpabuf_len(entry);
				rem_vendor_elem_size -= wpabuf_len(entry);
			} else {
				wpa_printf(MSG_DEBUG,
					   "Vendor element size:%zu exceeds available space:%zu ",
					   wpabuf_len(entry), rem_vendor_elem_size);
				wpa_hexdump(MSG_DEBUG, "Vendor element: ",
					    wpabuf_head_u8(entry), wpabuf_len(entry));
				return 0;
			}
		}

		if (len + nontx_profile_len > 255)
			break;

		len += nontx_profile_len;
	}

	*bss_index = i;

	/* Add 2 octets to get the full size of the element */
	return len + 2;
}


size_t hostapd_eid_mbssid_len(struct hostapd_data *hapd_probed, u32 frame_type,
			      u8 *elem_count, const u8 *known_bss,
			      size_t known_bss_len, size_t *rnr_len,
			      bool bcast_prb_resp, void *params,
			      bool *is_len_calc_failed)
{
	struct hostapd_data *hapd = hostapd_mbssid_get_tx_bss(hapd_probed);
	size_t len = 0, bss_index = 1;
	bool skip_rnr = false;
	bool rnr_override = true;
	size_t num_bss, elem_len = 0;

#ifdef CONFIG_QCN_EXTN
	skip_rnr = hostapd_skip_rnr_6ghz_colocated_extn(hapd, frame_type);
	rnr_override = hostapd_rnr_6ghz_override_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->iconf->mbssid ||
	    (frame_type != WLAN_FC_STYPE_BEACON &&
	     frame_type != WLAN_FC_STYPE_PROBE_RESP))
		return 0;

	num_bss = hostapd_get_mbssid_max_num_bss(hapd);

	/*
	 * Include the Multiple BSSID element whenever MBSSID is enabled. The
	 * element may include zero or more nontransmitted BSSID profiles.
	 */
	if (num_bss == 1) {
		if (frame_type == WLAN_FC_STYPE_BEACON) {
			if (!elem_count) {
				wpa_printf(MSG_INFO,
					   "MBSSID: Insufficient data for Beacon frames");
				return 0;
			}
			*elem_count = 1;
		}
		return 3;
	}

	if (frame_type == WLAN_FC_STYPE_BEACON) {
		if (!elem_count) {
			wpa_printf(MSG_INFO,
				   "MBSSID: Insufficient data for Beacon frames");
			return 0;
		}
		*elem_count = 0;
	}

	while (bss_index < num_bss) {
		size_t rnr_count = bss_index;

		elem_len = hostapd_eid_mbssid_elem_len(hapd_probed, frame_type,
						   &bss_index, known_bss,
						   known_bss_len, num_bss,
						   bcast_prb_resp, params);
		if (!elem_len) {
			wpa_printf(MSG_ERROR,
				   "MBSSID: Unable to calculate the length for:%s, frame_type:%u",
				   hapd_probed->conf->iface, frame_type);
			*is_len_calc_failed = true;
			return 0;
		}

		len += elem_len;

		if (frame_type == WLAN_FC_STYPE_BEACON)
			*elem_count += 1;

		if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED && rnr_len) {
			size_t rnr_cur_len = 0;
			struct mbssid_ie_profiles skip_profiles = {
				rnr_count, bss_index
			};

			if (!skip_rnr && rnr_override) {
			    *rnr_len += hostapd_eid_rnr_iface_len(
					hapd, hostapd_mbssid_get_tx_bss(hapd),
					&rnr_cur_len, &skip_profiles, false,
					NULL);
			}

			*rnr_len += hostapd_eid_rnr_mlo_len(
					hostapd_mbssid_get_tx_bss(hapd), frame_type,
					&skip_profiles, &rnr_cur_len, NULL);
		}
	}

	if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED && rnr_len)
		*rnr_len += hostapd_eid_rnr_len(hapd, frame_type, false);

	return len;
}

#ifdef CONFIG_IEEE80211BE
void hostapd_eid_update_cu_info(struct hostapd_data *hapd, u16 *elemid_modified,
				const u8 *eid_pos, size_t eid_len,
				enum elemid_cu eid_cu)
{
	u32 hash;

	if (!hapd->conf->mld_ap)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */

	if (!eid_pos || (eid_len == 0) || (eid_len > 255))
		return;
	if (eid_cu >= ELEMID_CU_PARAM_MAX)
		return;

	hash = ieee80211_crc32(eid_pos, eid_len);
	if ((hapd->cu_eid[eid_cu].eid_len != eid_len) ||
	    (hapd->cu_eid[eid_cu].hash != hash)) {
		hapd->cu_eid[eid_cu].eid_len = eid_len;
		hapd->cu_eid[eid_cu].hash = hash;
		*elemid_modified |= BIT(eid_cu);
	}
}
#endif

static u8 * hostapd_eid_mbssid_elem(struct hostapd_data *hapd, u8 *eid, u8 *end,
				    u32 frame_type, u8 max_bssid_indicator,
				    size_t *bss_index, u8 elem_count,
				    const u8 *known_bss, size_t known_bss_len,
				    u32 *elemid_modified_bmap, size_t num_bss,
				    bool bcast_prb_resp, void *params)
{
	struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	struct probe_resp_params *probe_params = NULL;
	bool is_ml_probe = false;
	u8 *eid_len_offset, *max_bssid_indicator_offset, *startpos;
	u8 ext_cap;
	size_t i;

	*eid++ = WLAN_EID_MULTIPLE_BSSID;
	eid_len_offset = eid++;
	max_bssid_indicator_offset = eid++;

	if (frame_type == WLAN_FC_STYPE_PROBE_RESP && params) {
		probe_params = (struct probe_resp_params *) params;
		is_ml_probe = probe_params->is_ml_probe;
	}

	for (i = *bss_index; i < num_bss; i++) {
		struct hostapd_data *bss;
		struct hostapd_bss_config *conf;
		struct non_inheritance_elem non_inherit_ie;
		u8 *eid_len_pos, *nontx_bss_start = eid;
		u16 capab_info, modified_flag = 0;
		size_t rem_vendor_elem_size;
		size_t j, wmm_len;
		ssize_t optional_ie_len = 0;

		if (tx_bss->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(tx_bss->mbssid_group, i);
		else
			bss = tx_bss->iface->bss[i];

		if (!bss || !bss->conf || !bss->started || !bss->beacon_set_done ||
		    mbssid_known_bss(i, known_bss, known_bss_len))
			continue;

		conf = bss->conf;

		*eid++ = WLAN_MBSSID_SUBELEMENT_NONTRANSMITTED_BSSID_PROFILE;
		eid_len_pos = eid++;

		capab_info = hostapd_own_capab_info(bss);
		*eid++ = WLAN_EID_NONTRANSMITTED_BSSID_CAPA;
		*eid++ = sizeof(capab_info);
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(bss->conf))
#endif /* CONFIG_QCN_EXTN */
		if (bss->conf->mld_ap && bss->rx_cu_param.critical_flag)
			capab_info |= WLAN_CAPABILITY_PBCC;

		if (hostapd_is_uhr_enabled(bss) &&
		    bss->rx_ecu_param.critical_update)
			capab_info |= WLAN_CAPABILITY_ECU;

		WPA_PUT_LE16(eid, capab_info);
		eid += sizeof(capab_info);

		*eid++ = WLAN_EID_SSID;
		if (!conf->ignore_broadcast_ssid ||
		    (frame_type == WLAN_FC_STYPE_PROBE_RESP && bss == hapd)) {
			*eid++ = conf->ssid.ssid_len;
			os_memcpy(eid, conf->ssid.ssid, conf->ssid.ssid_len);
			eid += conf->ssid.ssid_len;
		} else if (conf->ignore_broadcast_ssid == 2) {
			*eid++ = conf->ssid.ssid_len;
			os_memset(eid, 0, conf->ssid.ssid_len);
			eid += conf->ssid.ssid_len;
		} else {
			*eid++ = 0;
		}

		*eid++ = WLAN_EID_MULTIPLE_BSSID_INDEX;
		if (frame_type == WLAN_FC_STYPE_BEACON) {
			*eid++ = 3;
			*eid++ = bss->mbssid_idx; /* BSSID Index */
			if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
			    (conf->dtim_period % elem_count))
				conf->dtim_period = elem_count;
			*eid++ = conf->dtim_period;
			/* The driver is expected to update the DTIM Count
			 * field for each BSS that corresponds to a
			 * nontransmitted BSSID. The value is initialized to
			 * 0 here so that the DTIM count would be somewhat
			 * functional even if the driver were not to update
			 * this. */
			*eid++ = 0; /* DTIM Count */
		} else {
			/* Probe Request frame does not include DTIM Period and
			 * DTIM Count fields. */
			*eid++ = 1;
			*eid++ = bss->mbssid_idx; /* BSSID Index */
		}

		os_memset(&non_inherit_ie, 0, sizeof(non_inherit_ie));
		eid = hostapd_eid_mbssid_nontx_optional_ie(bss, params, &non_inherit_ie,
							   eid, &optional_ie_len, frame_type);

#ifdef CONFIG_IEEE80211BE
		/* For ML Probe Response frame, the solicited hapd's MLE will
		 * be in the frame body */
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(bss->conf)) {
#endif /* CONFIG_QCN_EXTN */

		if (bss->conf->mld_ap &&
		    (bss != hapd || frame_type != WLAN_FC_STYPE_PROBE_RESP ||
		     !is_ml_probe)) {
			ext_cap = 0;

			if (((frame_type == WLAN_FC_STYPE_PROBE_RESP) &&
			     bcast_prb_resp) || (frame_type == WLAN_FC_STYPE_BEACON)) {
				/* RMSL value sent in broadcast Probe response case and beacon */
				if (bss->conf->enable_aal)
					ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN);

				if (bss->conf->single_link_emlsr &&
				    (bss->iface->mld_ext_mld_capa &
				     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
					ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);
			}

			eid = hostapd_eid_eht_basic_ml_common(bss, eid, NULL,
							      true, false, ext_cap, false,
							      true, false,
							      !!(bss->conf->skip_uhr_extn & SKIP_UHR_EXTN_BCN));
			if (bss->eht_mld_link_removal_inprogress)
				eid = hostapd_eid_eht_reconf_ml(bss, eid);
		}
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

		rem_vendor_elem_size = MBSSID_NON_TX_VENDOR_ELEM_SIZE(bss);

		/* WMM IE */
		wmm_len = hostapd_eid_wmm_len(bss);
		if (wmm_len) {
			startpos = eid;
			eid = hostapd_eid_wmm(bss, eid, false);
			hostapd_eid_update_cu_info(bss, &modified_flag, startpos,
						   eid-startpos, ELEMID_CU_PARAM_WMM);
			if (modified_flag && elemid_modified_bmap)
				*elemid_modified_bmap |= BIT(i);
			rem_vendor_elem_size -= wmm_len;
		}

		/* User configured vendor elements */
		for (j = 0; j < bss->conf->vendor_elements_count; j++) {
			struct wpabuf *entry = bss->conf->vendor_elements[j];

			os_memcpy(eid, wpabuf_head(entry), wpabuf_len(entry));
			eid += wpabuf_len(entry);
			rem_vendor_elem_size -= wpabuf_len(entry);
		}
		conf->available_vendor_elem_size = rem_vendor_elem_size;

		/*
	 	 * Non-inheritance Element
	 	 * IEEE80211_ELEM_HEADER_LEN - 2
	 	 * Ext tag number: 1
	 	 * Length of Element ID list: 1
	 	 * Element ID list - Variable
	 	 * Length of Element ID Extension list: 1
	 	 * Element ID Extension List - Variable
	 	 */
		if (non_inherit_ie.ext_elem_len || non_inherit_ie.elem_len) {
			*eid++ = WLAN_EID_EXTENSION;
			*eid++ = 1 + 1 + non_inherit_ie.elem_len +
				 1 + non_inherit_ie.ext_elem_len;
			*eid++ = WLAN_EID_EXT_NON_INHERITANCE;
			*eid++ = non_inherit_ie.elem_len;
			os_memcpy(eid, non_inherit_ie.elem_list,
				  non_inherit_ie.elem_len);
			eid += non_inherit_ie.elem_len;
			*eid++ = non_inherit_ie.ext_elem_len;
			os_memcpy(eid, non_inherit_ie.ext_elem_list,
				  non_inherit_ie.ext_elem_len);
			eid += non_inherit_ie.ext_elem_len;
		}

		*eid_len_pos = (eid - eid_len_pos) - 1;

		if (((eid - eid_len_offset) - 1) > 255) {
			eid = nontx_bss_start;
			break;
		}
	}

	*bss_index = i;
	*max_bssid_indicator_offset = max_bssid_indicator;
	if (*max_bssid_indicator_offset < 1)
		*max_bssid_indicator_offset = 1;
	*eid_len_offset = (eid - eid_len_offset) - 1;
	return eid;
}


u8 * hostapd_eid_mbssid(struct hostapd_data *hapd_probed, u8 *eid, u8 *end,
			unsigned int frame_stype, u8 elem_count,
			u8 **elem_offset,
			const u8 *known_bss, size_t known_bss_len, u8 *rnr_eid,
			u8 *rnr_count, u8 **rnr_offset, size_t rnr_len,
			u32 *elemid_modified_bmap, bool bcast_prb_resp,
			void *params)
{
	struct hostapd_data *hapd = hostapd_mbssid_get_tx_bss(hapd_probed);
	size_t bss_index = 1, cur_len = 0;
	u8 elem_index = 0, *rnr_start_eid = rnr_eid;
	bool skip_rnr = false, rnr_override = true;
	bool add_rnr;
	size_t num_bss;

#ifdef CONFIG_QCN_EXTN
	skip_rnr = hostapd_skip_rnr_6ghz_colocated_extn(hapd, frame_stype);
	rnr_override = hostapd_rnr_6ghz_override_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->iconf->mbssid ||
	    (frame_stype != WLAN_FC_STYPE_BEACON &&
	     frame_stype != WLAN_FC_STYPE_PROBE_RESP))
		return eid;

	if (frame_stype == WLAN_FC_STYPE_BEACON && !elem_offset) {
		wpa_printf(MSG_INFO,
			   "MBSSID: Insufficient data for Beacon frames");
		return eid;
	}

	add_rnr = hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
		frame_stype == WLAN_FC_STYPE_BEACON &&
		rnr_eid && rnr_count && rnr_offset && rnr_len;

	num_bss = hostapd_get_mbssid_max_num_bss(hapd);

	/*
	 * Include the Multiple BSSID element whenever MBSSID is enabled. The
	 * element may include zero or more nontransmitted BSSID profiles.
	 */
	if (num_bss == 1) {
		if (frame_stype == WLAN_FC_STYPE_BEACON)
			elem_offset[0] = eid;

		return hostapd_eid_mbssid_elem(
				hapd_probed, eid, end, frame_stype,
				hostapd_max_bssid_indicator(hapd),
				&bss_index, elem_count, known_bss, known_bss_len,
				elemid_modified_bmap, num_bss, bcast_prb_resp,
				params);
	}

	while (bss_index < num_bss) {
		unsigned int rnr_start_count = bss_index;

		if (frame_stype == WLAN_FC_STYPE_BEACON) {
			if (elem_index == elem_count) {
				wpa_printf(MSG_WARNING,
					   "MBSSID: Larger number of elements than there is room in the provided array");
				break;
			}

			elem_offset[elem_index] = eid;
			elem_index = elem_index + 1;
		}
		eid = hostapd_eid_mbssid_elem(hapd_probed, eid, end,
					      frame_stype,
					      hostapd_max_bssid_indicator(hapd),
					      &bss_index, elem_count,
					      known_bss, known_bss_len,
					      elemid_modified_bmap, num_bss,
					      bcast_prb_resp, params);

		if (add_rnr) {
			struct mbssid_ie_profiles skip_profiles = {
				rnr_start_count, bss_index
			};

			rnr_offset[*rnr_count] = rnr_eid;
			*rnr_count = *rnr_count + 1;
			cur_len = 0;
			if (!skip_rnr && rnr_override) {
			    rnr_eid = hostapd_eid_rnr_iface(
				      hapd, hostapd_mbssid_get_tx_bss(hapd),
				      rnr_eid, &cur_len, &skip_profiles, false,
				      frame_stype, NULL);
			}
			rnr_eid = hostapd_eid_rnr_mlo(
				hostapd_mbssid_get_tx_bss(hapd), frame_stype,
				rnr_eid, &skip_profiles, &cur_len, NULL);
		}
	}

	if (add_rnr && (size_t) (rnr_eid - rnr_start_eid) < rnr_len) {
		rnr_offset[*rnr_count] = rnr_eid;
		*rnr_count = *rnr_count + 1;
		cur_len = 0;

		if (hapd->conf->rnr)
			rnr_eid = hostapd_eid_nr_db(hapd, rnr_eid, &cur_len, NULL);
		if (get_colocation_mode(hapd) == COLOCATED_LOWER_BAND)
			rnr_eid = hostapd_eid_rnr_colocation(hapd, rnr_eid,
							     &cur_len, frame_stype,
							     NULL);
	}

	return eid;
}

#endif /* CONFIG_NATIVE_WINDOWS */
