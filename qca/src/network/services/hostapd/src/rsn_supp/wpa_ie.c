/*
 * wpa_supplicant - WPA/RSN IE and KDE processing
 * Copyright (c) 2003-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "wpa.h"
#include "pmksa_cache.h"
#include "common/ieee802_11_defs.h"
#include "wpa_i.h"
#include "wpa_ie.h"


/**
 * wpa_parse_wpa_ie - Parse WPA/RSN IE
 * @wpa_ie: Pointer to WPA or RSN IE
 * @wpa_ie_len: Length of the WPA/RSN IE
 * @data: Pointer to data area for parsing results
 * Returns: 0 on success, -1 on failure
 *
 * Parse the contents of WPA or RSN IE and write the parsed data into data.
 */
int wpa_parse_wpa_ie(const u8 *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == WLAN_EID_RSN)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	if (wpa_ie_len >= 6 && wpa_ie[0] == WLAN_EID_VENDOR_SPECIFIC &&
	    wpa_ie[1] >= 4 &&
	    WPA_GET_BE32(&wpa_ie[2]) == RSNE_OVERRIDE_IE_VENDOR_TYPE)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	if (wpa_ie_len >= 6 && wpa_ie[0] == WLAN_EID_VENDOR_SPECIFIC &&
	    wpa_ie[1] >= 4 &&
	    WPA_GET_BE32(&wpa_ie[2]) == RSNE_OVERRIDE_2_IE_VENDOR_TYPE)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}


static int wpa_gen_wpa_ie_wpa(u8 *wpa_ie, size_t wpa_ie_len,
			      int pairwise_cipher, int group_cipher,
			      int key_mgmt)
{
	u8 *pos;
	struct wpa_ie_hdr *hdr;
	u32 suite;

	if (wpa_ie_len < sizeof(*hdr) + WPA_SELECTOR_LEN +
	    2 + WPA_SELECTOR_LEN + 2 + WPA_SELECTOR_LEN)
		return -1;

	hdr = (struct wpa_ie_hdr *) wpa_ie;
	hdr->elem_id = WLAN_EID_VENDOR_SPECIFIC;
	RSN_SELECTOR_PUT(hdr->oui, WPA_OUI_TYPE);
	WPA_PUT_LE16(hdr->version, WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_WPA, group_cipher);
	if (suite == 0) {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	suite = wpa_cipher_to_suite(WPA_PROTO_WPA, pairwise_cipher);
	if (suite == 0 ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_NONE);
	} else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_CCKM);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - wpa_ie) - 2;

	WPA_ASSERT((size_t) (pos - wpa_ie) <= wpa_ie_len);

	return pos - wpa_ie;
}


u16 rsn_supp_capab(struct wpa_sm *sm)
{
	u16 capab = 0;

	if (sm->wmm_enabled) {
		/* Advertise 16 PTKSA replay counters when using WMM */
		capab |= RSN_NUM_REPLAY_COUNTERS_16 << 2;
	}
	if (sm->mfp)
		capab |= WPA_CAPABILITY_MFPC;
	if (sm->mfp == 2)
		capab |= WPA_CAPABILITY_MFPR;
	if (sm->ocv)
		capab |= WPA_CAPABILITY_OCVC;
	if (sm->ext_key_id)
		capab |= WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST;

	return capab;
}


static int wpa_gen_wpa_ie_rsn(u8 *rsn_ie, size_t rsn_ie_len,
			      int pairwise_cipher, int group_cipher,
			      int key_mgmt, int mgmt_group_cipher,
			      struct wpa_sm *sm)
{
	u8 *pos;
	struct rsn_ie_hdr *hdr;
	u32 suite;

	if (rsn_ie_len < sizeof(*hdr) + RSN_SELECTOR_LEN +
	    2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN + 2 +
	    (sm->cur_pmksa ? 2 + PMKID_LEN : 0)) {
		wpa_printf(MSG_DEBUG, "RSN: Too short IE buffer (%lu bytes)",
			   (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (struct rsn_ie_hdr *) rsn_ie;
	hdr->elem_id = WLAN_EID_RSN;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, group_cipher);
	if (suite == 0) {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, pairwise_cipher);
	if (suite == 0 ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_CCKM);
#ifdef CONFIG_IEEE80211R
	} else if (key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X);
#ifdef CONFIG_SHA384
	} else if (key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384);
#endif /* CONFIG_SHA384 */
	} else if (key_mgmt == WPA_KEY_MGMT_FT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_PSK);
#endif /* CONFIG_IEEE80211R */
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SHA256);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_SHA256);
#ifdef CONFIG_SAE
	} else if (key_mgmt == WPA_KEY_MGMT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
	} else if (key_mgmt == WPA_KEY_MGMT_SAE_EXT_KEY) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE_EXT_KEY);
	} else if (key_mgmt == WPA_KEY_MGMT_FT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE);
	} else if (key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY);
#endif /* CONFIG_SAE */
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192);
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B);
#ifdef CONFIG_FILS
	} else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA256);
	} else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA384);
#ifdef CONFIG_IEEE80211R
	} else if (key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA256);
	} else if (key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA384);
#endif /* CONFIG_IEEE80211R */
#endif /* CONFIG_FILS */
#ifdef CONFIG_OWE
	} else if (key_mgmt & WPA_KEY_MGMT_OWE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OWE);
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	} else if (key_mgmt & WPA_KEY_MGMT_DPP) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_DPP);
#endif /* CONFIG_DPP */
#ifdef CONFIG_SHA384
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SHA384);
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_ENC_ASSOC
	} else if (key_mgmt == WPA_KEY_MGMT_EPPKE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_EPPKE);
#endif /* CONFIG_ENC_ASSOC */
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	WPA_PUT_LE16(pos, rsn_supp_capab(sm));
	pos += 2;

	if (sm->cur_pmksa) {
		/* PMKID Count (2 octets, little endian) */
		*pos++ = 1;
		*pos++ = 0;
		/* PMKID */
		os_memcpy(pos, sm->cur_pmksa->pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	if (wpa_cipher_valid_mgmt_group(mgmt_group_cipher)) {
		if (!sm->cur_pmksa) {
			/* PMKID Count */
			WPA_PUT_LE16(pos, 0);
			pos += 2;
		}

		/* Management Group Cipher Suite */
		RSN_SELECTOR_PUT(pos, wpa_cipher_to_suite(WPA_PROTO_RSN,
							  mgmt_group_cipher));
		pos += RSN_SELECTOR_LEN;
	}

	hdr->len = (pos - rsn_ie) - 2;

	WPA_ASSERT((size_t) (pos - rsn_ie) <= rsn_ie_len);

	return pos - rsn_ie;
}


/**
 * wpa_gen_wpa_ie - Generate WPA/RSN IE based on current security policy
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @wpa_ie: Pointer to memory area for the generated WPA/RSN IE
 * @wpa_ie_len: Maximum length of the generated WPA/RSN IE
 * Returns: Length of the generated WPA/RSN IE or -1 on failure
 */
int wpa_gen_wpa_ie(struct wpa_sm *sm, u8 *wpa_ie, size_t wpa_ie_len)
{
	if (sm->proto == WPA_PROTO_RSN)
		return wpa_gen_wpa_ie_rsn(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt, sm->mgmt_group_cipher,
					  sm);
	else
		return wpa_gen_wpa_ie_wpa(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt);
}


/*
 * security_profile_akm_matches - Check if a profile's AKM matches key_mgmt
 * @profile_num: Security Profile number (Table 9-bb14, 802.11bn D1.4)
 * @key_mgmt: Negotiated WPA_KEY_MGMT_* value
 *
 * Returns 1 if the profile's AKM (Table 9-bb14) matches key_mgmt, 0 otherwise.
 * Used during profile selection in wpa_supplicant_set_suites().
 */
int security_profile_akm_matches(int profile_num, int key_mgmt)
{
	switch (profile_num) {
	case SECURITY_PROFILE_NUM_EPPKE_NO_AUTH:
	case SECURITY_PROFILE_NUM_EPPKE_SAE:
	case SECURITY_PROFILE_NUM_EPPKE_FT_SAE:
		/* EPPKE AKM (00-0F-AC:29) */
		return !!(key_mgmt & WPA_KEY_MGMT_EPPKE);
	case SECURITY_PROFILE_NUM_8021X_AUTH:
	case SECURITY_PROFILE_NUM_8021X:
		/* 802.1X AKM (00-0F-AC:5) */
		return key_mgmt == WPA_KEY_MGMT_IEEE8021X ||
		       key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA256;
	case SECURITY_PROFILE_NUM_8021X_FT_AUTH:
	case SECURITY_PROFILE_NUM_8021X_FT:
		/* 802.1X+FT AKM (00-0F-AC:3) */
		return key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X;
	case SECURITY_PROFILE_NUM_8021X_SHA384_AUTH:
	case SECURITY_PROFILE_NUM_8021X_SHA384:
		/* 802.1X SHA384 AKM (00-0F-AC:23) */
		return key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA384;
	case SECURITY_PROFILE_NUM_8021X_FT384_AUTH:
	case SECURITY_PROFILE_NUM_8021X_FT384:
		/* 802.1X+FT SHA384 AKM (00-0F-AC:22) */
		return key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X_SHA384;
	case SECURITY_PROFILE_NUM_8021X_SUITEB_AUTH:
	case SECURITY_PROFILE_NUM_8021X_SUITEB:
		/* 802.1X Suite-B-192 AKM (00-0F-AC:12) */
		return key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192;
	case SECURITY_PROFILE_NUM_OWE:
		/* OWE/None AKM (00-0F-AC:18) */
		return key_mgmt == WPA_KEY_MGMT_OWE;
	case SECURITY_PROFILE_NUM_SAE:
		/* SAE AKM (00-0F-AC:24) */
		return key_mgmt == WPA_KEY_MGMT_SAE_EXT_KEY ||
		       key_mgmt == WPA_KEY_MGMT_SAE;
	case SECURITY_PROFILE_NUM_FT_SAE:
		/* FT/SAE AKM (00-0F-AC:25) */
		return key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY ||
		       key_mgmt == WPA_KEY_MGMT_FT_SAE;
	default:
		return 0;
	}
}


/*
 * wpa_external_auth_add_rsne - Build an RSNE for external authentication
 * @rsne: Buffer in which the RSNE will be written
 * @rsne_len: Length of the RSNE buffer
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @akmp: Authentication and key management protocol
 * @pairwise_cipher: The pairwise cipher suite
 * @group_cipher: The group addressed data cipher suite
 * @group_mgmt_cipher: The group addressed management cipher suite
 * @rsn_capab: RSN capabilities field
 * Returns: Length of the RSNE or -1 on failure
 */
int wpa_external_auth_add_rsne(u8 *rsne, size_t rsne_len, struct wpa_sm *sm,
			       int akmp, int pairwise_cipher, int group_cipher,
			       int group_mgmt_cipher, u16 rsn_capab)
{
	struct rsn_ie_hdr *hdr;
	u32 suite;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "RSN: Ext-Auth: Build RSNE");

	if (rsne_len < sizeof(*hdr) + RSN_SELECTOR_LEN +
	    2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN + 2 +
	    (sm->cur_pmksa ? 2 + PMKID_LEN : 0)) {
		wpa_printf(MSG_DEBUG, "Ext-Auth: Too short RSNE buffer (%lu bytes)",
			   (unsigned long) rsne_len);
		return -1;
	}

	hdr = (struct rsn_ie_hdr *) rsne;
	hdr->elem_id = WLAN_EID_RSN;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	/* Group cipher */
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, group_cipher);
	if (!suite || !wpa_cipher_valid_group(group_cipher)) {
		wpa_printf(MSG_INFO,
			   "RSN: Ext-Auth: Invalid group cipher 0x%x",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* Pairwise cipher */
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, pairwise_cipher);
	if (!suite ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_INFO,
			   "RSN: Ext-Auth: Invalid pairwise cipher 0x%x",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* AKM suite */
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	suite = wpa_akm_to_suite(akmp);
	if (!suite) {
		wpa_printf(MSG_INFO, "RSN: Ext-Auth: Invalid AKMP 0x%x", akmp);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	WPA_PUT_LE16(pos, rsn_capab);
	pos += 2;

	if (sm->cur_pmksa) {
		wpa_printf(MSG_DEBUG, "RSN: Ext-Auth: Adding PMKID");
		/* PMKID Count (2 octets, little endian) */
		WPA_PUT_LE16(pos, 1);
		pos += 2;
		/* PMKID */
		os_memcpy(pos, sm->cur_pmksa->pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	/* Group Management Cipher Suite */
	if (wpa_cipher_valid_mgmt_group(group_mgmt_cipher)) {
		if (!sm->cur_pmksa) {
			/* PMKID Count */
			WPA_PUT_LE16(pos, 0);
			pos += 2;
		}

		/* Management Group Cipher Suite */
		RSN_SELECTOR_PUT(pos, wpa_cipher_to_suite(WPA_PROTO_RSN,
							  group_mgmt_cipher));
		pos += RSN_SELECTOR_LEN;
	}

	hdr->len = (pos - rsne) - 2;

	WPA_ASSERT((size_t) (pos - rsne) <= rsne_len);

	return pos - rsne;
}


int wpa_gen_rsnxe(struct wpa_sm *sm, u8 *rsnxe, size_t rsnxe_len)
{
	u8 *pos = rsnxe;
	u64 capab, tmp;
	size_t flen;

	capab = wpa_sm_get_rsnxe_capab(sm);

	if (!capab)
		return 0; /* no supported extended RSN capabilities */
	tmp = capab;
	flen = 0;
	while (tmp) {
		flen++;
		tmp >>= 8;
	}
	if (rsnxe_len < 2 + flen)
		return -1;
	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	*pos++ = WLAN_EID_RSNX;
	*pos++ = flen;
	while (capab) {
		*pos++ = capab & 0xff;
		capab >>= 8;
	}

	return pos - rsnxe;
}


/*
 * wpa_sm_get_rsnxe_capab - Compute RSNXE capability word from wpa_sm state
 * @sm: WPA state machine
 *
 * Returns the u64 capability word that wpa_gen_rsnxe() encodes into the RSNXE
 * wire format, WITHOUT the length prefix bits (bits 0-3). The caller is
 * responsible for inserting the length prefix before encoding.
 *
 * This is the single source of truth for RSNXE capabilities. Both
 * wpa_gen_rsnxe() and security_profile_build_sta_ie() call this function to
 * guarantee that the RSNXE and the Security Profile element's Extended RSN
 * Capabilities field always carry identical values (802.11bn D1.4, 9.4.2.364,
 * p.206: "field values must be configured to match the value specified in the
 * corresponding Security Profile").
 */
u64 wpa_sm_get_rsnxe_capab(struct wpa_sm *sm)
{
	u64 capab = 0;

	if (wpa_key_mgmt_sae(sm->key_mgmt) &&
	    (sm->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
	     sm->sae_pwe == SAE_PWE_BOTH || sm->sae_pk)) {
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
#ifdef CONFIG_SAE_PK
		if (sm->sae_pk)
			capab |= BIT(WLAN_RSNX_CAPAB_SAE_PK);
#endif /* CONFIG_SAE_PK */
	}

	if (sm->secure_ltf)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (sm->secure_rtt)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (sm->prot_range_neg)
		capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
	if (sm->prot_range_neg_x20)
		capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR_X20);
	if (sm->ssid_protection)
		capab |= BIT(WLAN_RSNX_CAPAB_SSID_PROTECTION);
	if (sm->spp_amsdu)
		capab |= BIT(WLAN_RSNX_CAPAB_SPP_A_MSDU);
	if (sm->sae_pw_id_change)
		capab |= BIT_ULL(WLAN_RSNX_CAPAB_SAE_PW_ID_CHANGE);
#ifdef CONFIG_ENC_ASSOC
	if (sm->assoc_encryption)
		capab |= BIT(WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION) |
			BIT(WLAN_RSNX_CAPAB_KEK_IN_PASN);
#endif /* CONFIG_ENC_ASSOC */
#ifdef CONFIG_PMKSA_PRIVACY
	if (sm->pmksa_privacy)
		capab |= BIT(WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY);
#endif /* CONFIG_PMKSA_PRIVACY */
	if (sm->control_frame_prot)
		capab |= BIT_ULL(WLAN_RSNX_CAPAB_CIGTK);

	return capab;
}


/*
 * security_profile_build_sta_ie - Build Security Profile element for STA TX
 * @sm: WPA state machine — same instance used by wpa_gen_wpa_ie_rsn() and
 *      wpa_gen_rsnxe(). All capability fields are derived from sm, NOT from
 *      the AP's advertised Security Profile element.
 * @selected_profile_num: Single profile number the STA has selected (0-119).
 *      Only this profile's bit is set in the Security Profile Bitmap (37.32).
 * @buf: Output buffer
 * @buf_len: Size of output buffer
 *
 * Returns: Number of bytes written, or -1 on error.
 *
 * Spec: 9.4.2.364 (Figure 9-aa70), 37.32, Table 9-bb14 (802.11bn D1.4).
 * Element ID = 255, Element ID Extension = 162 (Table 9-164).
 *
 * Consistency guarantee:
 *   Reduced RSN Capabilities <- rsn_supp_capab(sm)  [same as RSNE builder]
 *   Extended RSN Capabilities <- wpa_sm_get_rsnxe_capab(sm) [same as RSNXE]
 * This ensures RSNE RSN Capabilities, RSNXE, and Security Profile element
 * always carry identical capability values (9.4.2.364 p.206).
 */
int security_profile_build_sta_ie(struct wpa_sm *sm,
				  int selected_profile_num,
				  u8 *buf, size_t buf_len)
{
	u8 *pos = buf;
	u8 *len_pos;
	u8 reduced_rsn_caps = 0;
	u16 rsn_caps;
	u64 ext_rsn_capab;
	u64 tmp;
	size_t ext_rsn_len;
	size_t bitmap_len;
	size_t total;

	if (!sm || selected_profile_num < 0 ||
	    selected_profile_num > SECURITY_PROFILE_NUM_MAX)
		return -1;

	/*
	 * Reduced RSN Capabilities (Figure 9-aa71): B0=ExtKeyID, B1=OCVC.
	 * Derived from rsn_supp_capab(sm) — the same function used by
	 * wpa_gen_wpa_ie_rsn() — so this field always matches the RSN
	 * Capabilities word in the STA's RSNE.
	 */
	rsn_caps = rsn_supp_capab(sm);

	if (rsn_caps & WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST)
		reduced_rsn_caps |= REDUCED_RSN_CAPS_EXT_KEY_ID;
	if (rsn_caps & WPA_CAPABILITY_OCVC)
		reduced_rsn_caps |= REDUCED_RSN_CAPS_OCVC;

	/*
	 * Extended RSN Capabilities (Table 9-408): derived from
	 * wpa_sm_get_rsnxe_capab(sm) — the same helper used by
	 * wpa_gen_rsnxe() — so this field always matches the STA's RSNXE.
	 * Length prefix (bits 0-3) is added below after computing byte length.
	 */
	ext_rsn_capab = wpa_sm_get_rsnxe_capab(sm);
	tmp = ext_rsn_capab;
	ext_rsn_len = 0;
	while (tmp) {
		ext_rsn_len++;
		tmp >>= 8;
	}
	if (ext_rsn_len == 0)
		ext_rsn_len = 1; /* minimum 1 octet */
	/* Encode length in bits 0-3 of first byte (Table 9-408) */
	ext_rsn_capab |= (u64)(ext_rsn_len - 1);

	/*
	 * Security Profile Bitmap: one bit per profile number.
	 * Only the selected profile's bit is set (37.32).
	 * Profile numbers 0-7 fit in 1 octet, 8-15 in 2 octets, etc.
	 */
	bitmap_len = (size_t)(selected_profile_num / 8) + 1;

	/* EID(1) + Len(1) + EID_EXT(1) + ReducedRSNCaps(1) +
	 * SecProfInd(1) + bitmap(bitmap_len) + ext_rsn(ext_rsn_len) */
	total = 2 + 1 + 1 + 1 + bitmap_len + ext_rsn_len;
	if (buf_len < total)
		return -1;

	/* Element ID = 255 (WLAN_EID_EXTENSION) */
	*pos++ = WLAN_EID_EXTENSION;
	len_pos = pos++; /* Length — filled in at end */

	/* Element ID Extension = 162 (Table 9-164, 802.11bn D1.4 p.129) */
	*pos++ = WLAN_EID_EXT_SECURITY_PROFILE;

	/* Reduced RSN Capabilities (1 octet, Figure 9-aa71) */
	*pos++ = reduced_rsn_caps;

	/*
	 * Security Profile Indication (1 octet, Figure 9-aa72):
	 * B0-B3 = Number Of Octets Of Security Profile Bitmap
	 * B4-B7 = Number Of Vendor Specific Security Profiles (0 here)
	 */
	*pos++ = (u8)(bitmap_len & 0x0F);

	/* Security Profile Bitmap: bit X = 1 for selected_profile_num */
	os_memset(pos, 0, bitmap_len);
	pos[selected_profile_num / 8] |= BIT(selected_profile_num % 8);
	pos += bitmap_len;

	/* Vendor Specific Security Profile List: empty (0 vendor profiles) */

	/* Extended RSN Capabilities (variable, Table 9-408) */
	tmp = ext_rsn_capab;
	while (ext_rsn_len--) {
		*pos++ = (u8)(tmp & 0xff);
		tmp >>= 8;
	}

	/* Fill Length field (excludes EID and Length bytes, includes EID_EXT) */
	*len_pos = (u8)(pos - len_pos - 1);

	return (int)(pos - buf);
}
