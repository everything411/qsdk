// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "utils/common.h"
#include "../wpa_supplicant/config.h"
#include "../wpa_supplicant/wpa_supplicant_i.h"
#include "../wpa_supplicant/driver_i.h"
#include "../wpa_supplicant/scan.h"
#include "cmn.h"

static int wpas_get_radio_idx_for_freq(struct wpa_supplicant *wpa_s, int freq,
				       u8 *radio_idx)
{
	struct hostapd_multi_hw_info *multi_hw_info;
	unsigned int num_multi_hws = 0;
	unsigned int i;

	multi_hw_info = wpa_get_multi_hw_info(wpa_s, &num_multi_hws);
	if (!multi_hw_info || !num_multi_hws || !freq)
		return -1;

	for (i = 0; i < num_multi_hws; i++) {
		if (freq >= multi_hw_info[i].start_freq &&
		    freq <= multi_hw_info[i].end_freq) {
			*radio_idx = multi_hw_info[i].hw_idx;
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "HE MCS 12/13: no radio_idx found for freq = %d", freq);

	return -1;
}

int wpas_set_he_mcs_12_13_peer_cap_extn(struct wpa_supplicant *wpa_s, int freq)
{
	struct wpa_supplicant_extn *wpas_extn = &wpa_s->wpas_extn;
	u8 radio_idx;

	if (wpas_get_radio_idx_for_freq(wpa_s, freq, &radio_idx))
		return -1;

	if (nl80211_set_he_mcs_12_13_peer_cap_extn(wpa_s->drv_priv, radio_idx,
						   wpas_extn->he_mcs_12_13_peer_cap))
		return -1;

	wpa_printf(MSG_DEBUG,
		   "he_mcs_12_13 peer capability = 0x%04x set to driver successfully for radio_idx = %u",
		   wpas_extn->he_mcs_12_13_peer_cap, radio_idx);

	return 0;
}

int wpas_set_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq)
{
	struct wpa_supplicant_extn *wpas_extn = &wpa_s->wpas_extn;
	u16 radio_cap = 0;
	u8 radio_idx;

	if (wpa_s->conf->conf_extn.he_mcs_12_13_enabled) {
		if (wpas_get_radio_idx_for_freq(wpa_s, freq, &radio_idx))
			return -1;

		if (nl80211_get_he_mcs_12_13_extn(wpa_s->drv_priv, radio_idx, &radio_cap))
			return -1;
	}

	wpas_extn->he_mcs_12_13_radio_cap = radio_cap;
	wpa_printf(MSG_INFO,
		   "he_mcs_12_13_supp set to %d radio_capabilities = 0x%04x for freq %d MHz",
		   wpa_s->conf->conf_extn.he_mcs_12_13_enabled,
		   wpas_extn->he_mcs_12_13_radio_cap, freq);

	return 0;
}

void wpas_drv_set_peer_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq,
					     const u8 *ies, size_t ies_len)
{
	struct wpa_supplicant_extn *wpas_extn = &wpa_s->wpas_extn;
	struct ieee802_11_elems elems;

	if (!ies || !ies_len || !wpa_s->conf->conf_extn.he_mcs_12_13_enabled)
		return;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) == ParseFailed)
		return;

	wpas_extn->he_mcs_12_13_peer_cap = elems.elems_extn.he_mcs_12_13_peer_cap;
	wpas_set_he_mcs_12_13_peer_cap_extn(wpa_s, freq);
}

static int wpas_he_mcs_12_13_supp(struct wpa_supplicant *wpa_s, bool val)
{
	if (wpa_s->conf->conf_extn.he_mcs_12_13_enabled == val)
		return 0;

	wpa_s->conf->conf_extn.he_mcs_12_13_enabled = val;
	if (wpas_set_he_mcs_12_13_cap_extn(wpa_s, wpa_s->assoc_freq)) {
		wpa_printf(MSG_ERROR, "Failed to set HE MCS 12 13 support");
		return -1;
	}

	if (wpa_s->wpa_state >= WPA_ASSOCIATED) {
		wpa_s->reassociate = 1;
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
	}

	return -1;
}

int wpas_ctrl_iface_set_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			     const char *value, bool *is_extn_cmd)
{
	int ret = -1, enable;
	bool val;

	if (!is_extn_cmd)
		return -1;

	*is_extn_cmd = true;
	if (os_strcasecmp(cmd, "he_mcs_12_13_supp") == 0) {
		val = !!atoi(value);
		ret = wpas_he_mcs_12_13_supp(wpa_s, val);
	} else if (os_strcasecmp(cmd, "vht_mcs_10_11_supp") == 0) {
		wpa_s->conf->conf_extn.vht_mcs_10_11_supp = !!atoi(value);
		ret = 0;
	} else if (os_strcasecmp(cmd, "he_400ns_sgi_supp") == 0) {
		wpa_s->conf->conf_extn.he_400ns_sgi_supp = !!atoi(value);
		ret = 0;
	} else if (os_strcasecmp(cmd, "he_2xltf_160_80p80_supp") == 0) {
		wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp = !!atoi(value);
		ret = 0;
	} else if (os_strcasecmp(cmd, "wds_ie") == 0) {
		enable = atoi(value);
		wpa_printf(MSG_ERROR, "ENABLE %d", enable);
		ret = wpa_ctrl_set_wds_ie_extn(wpa_s, enable);
	} else if (os_strcasecmp(cmd, "allow_3addr_mc") == 0) {
		enable = atoi(value);
		ret = wpa_ctrl_set_allow_3addr_mc_extn(wpa_s, enable);
	} else {
		*is_extn_cmd = false;
	}

	return ret;
}

int wpas_ctrl_iface_get_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			     char *buf, size_t buflen, bool *is_extn_cmd)
{
	int ret = -1;

	if (!is_extn_cmd)
		return -1;

	*is_extn_cmd = true;
	if (os_strcasecmp(cmd, "he_mcs_12_13_supp") == 0) {
		ret = os_snprintf(buf, buflen, "he_mcs_12_13_supp = %u\n",
				  wpa_s->conf->conf_extn.he_mcs_12_13_enabled);
		if (os_snprintf_error(buflen, ret))
			return -1;
	} else if (os_strcasecmp(cmd, "vht_mcs_10_11_supp") == 0) {
		ret = os_snprintf(buf, buflen, "vht_mcs_10_11_supp = %u\n",
				  wpa_s->conf->conf_extn.vht_mcs_10_11_supp);
		if (os_snprintf_error(buflen, ret))
			return -1;
	} else if (os_strcasecmp(cmd, "he_400ns_sgi_supp") == 0) {
		ret = os_snprintf(buf, buflen, "he_400ns_sgi_supp = %u\n",
				  wpa_s->conf->conf_extn.he_400ns_sgi_supp);
		if (os_snprintf_error(buflen, ret))
			return -1;
	} else if (os_strcasecmp(cmd, "he_2xltf_160_80p80_supp") == 0) {
		ret = os_snprintf(buf, buflen, "he_2xltf_160_80p80_supp = %u\n",
				  wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp);
		if (os_snprintf_error(buflen, ret))
			return -1;
	} else if (os_strcmp(cmd, "wds_ie") == 0) {
		ret = wpa_ctrl_get_wds_ie_extn(wpa_s, buf, buflen);
	} else if (os_strcasecmp(cmd, "allow_3addr_mc") == 0) {
		ret = os_snprintf(buf, buflen, "allow_3addr_mc = %u\n",
				  wpa_s->conf->conf_extn.allow_3addr_mc);
	} else {
		*is_extn_cmd = false;
	}

	return ret;
}

/**
 * wpas_drv_set_peer_vht_mcs10_11_and_he_cap_internal_extn - STA mode: notify
 * driver of negotiated VHT MCS 10/11 and HE 400ns SGI / 2xLTF 160/80+80
 * peer capabilities extracted from the AP's assoc response QCN IE.
 *
 * Parses the assoc response IEs, extracts QCN IE attributes 0x02/0x03/0x04,
 * computes he_cap_internal bitmap, and calls
 * nl80211_set_vht_mcs10_11_and_he_cap_internal_extn() to inform the driver.
 * Called alongside wpas_drv_set_peer_he_mcs_12_13_cap_extn() in events.c.
 *
 * @wpa_s:    wpa_supplicant instance
 * @ies:      assoc response IEs buffer
 * @ies_len:  assoc response IEs length
 */
void wpas_drv_set_peer_vht_mcs10_11_and_he_cap_internal_extn(
		struct wpa_supplicant *wpa_s,
		const u8 *ies, size_t ies_len)
{
	struct ieee802_11_elems elems;
	u8 vht_supp = 0;
	u32 he_cap_internal = 0;
	const u8 *bssid;

	if (!ies || !ies_len || !wpa_s || !wpa_s->conf)
		return;

	if (!wpa_s->conf->conf_extn.vht_mcs_10_11_supp &&
	    !wpa_s->conf->conf_extn.he_400ns_sgi_supp &&
	    !wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp)
		return;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) == ParseFailed)
		return;

	if (wpa_s->conf->conf_extn.vht_mcs_10_11_supp &&
	    elems.elems_extn.vht_mcs10_11_peer_cap)
		vht_supp = 1;

	if (wpa_s->conf->conf_extn.he_400ns_sgi_supp)
		he_cap_internal |= elems.elems_extn.he_400ns_sgi_peer_cap & 0x3;

	if (wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp &&
	    elems.elems_extn.he_2xltf_160_80p80_peer_cap)
		he_cap_internal |= BIT(2);

	if (!vht_supp && !he_cap_internal)
		return;

	bssid = wpa_s->bssid;
	wpa_printf(MSG_DEBUG,
		   "QCN: STA assoc VHT MCS10/11 supp=%u he_cap_internal=0x%08x for "
		   MACSTR, vht_supp, he_cap_internal, MAC2STR(bssid));

	nl80211_set_vht_mcs10_11_and_he_cap_internal_extn(wpa_s->drv_priv,
							  bssid,
							  vht_supp,
							  he_cap_internal);
}
