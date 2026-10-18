// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * WDS Vendor IE - Generic WDS capability advertisement framework
 *
 * Implements IE building, parsing, AP-side association handling, and
 * wpa_supplicant-side association request/response processing for the
 * WDS vendor IE defined in wds_ie.h.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/sta_info.h"
#include "ap/ap_drv_ops.h"
#include "ap/ieee802_11.h"
#include "cmn.h"
#include "wds_ie.h"

/* Pull in wpa_supplicant types only when building the supplicant binary */
#ifndef HOSTAPD
#include "../wpa_supplicant/wpa_supplicant_i.h"
#include "../wpa_supplicant/config_ssid.h"
#include "../wpa_supplicant/driver_i.h"
#endif /* !HOSTAPD */


/* ================================================================== */
/* Low-level IE build / parse helpers                                   */
/* ================================================================== */

/**
 * wds_ie_build - Serialise a WDS vendor IE into @buf
 *
 * Writes the full 8-byte WDS vendor IE:
 *   EID(1) | Len(1) | OUI(3) | Type(1) | Cap(1) | Ver(1)
 *
 * @buf:        Destination buffer
 * @len:        Available bytes in @buf
 * @capability: WDS capability flags (WDS_IE_CAP_AP / WDS_IE_CAP_STA)
 *
 * Returns WDS_IE_TOTAL_LEN on success, 0 if the buffer is too small.
 */
size_t wds_ie_build(u8 *buf, size_t len, u8 capability)
{
	u8 *pos = buf;

	if (!buf || len < WDS_IE_TOTAL_LEN) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: build failed - buffer too small "
			   "(need %d, have %zu)", WDS_IE_TOTAL_LEN, len);
		return 0;
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = WDS_IE_PAYLOAD_LEN;   /* Length field covers OUI..Version */
	*pos++ = WDS_IE_OUI_0;
	*pos++ = WDS_IE_OUI_1;
	*pos++ = WDS_IE_OUI_2;
	*pos++ = WDS_IE_OUI_TYPE;
	*pos++ = capability;
	*pos++ = WDS_IE_VERSION;

	wpa_printf(MSG_DEBUG,
		   "WDS IE: built IE cap=0x%02x ver=%u (%d bytes)",
		   capability, WDS_IE_VERSION, WDS_IE_TOTAL_LEN);

	return WDS_IE_TOTAL_LEN;
}


/**
 * wds_ie_parse - Deserialise a WDS vendor IE
 *
 * @ie points to the OUI byte (i.e. the byte immediately after the
 * standard EID and Length octets).  @ie_len is the value of the Length
 * field, which must be >= WDS_IE_PAYLOAD_LEN.
 *
 * @ie:     Pointer to OUI byte of the IE payload
 * @ie_len: Payload length (Length field value)
 * @params: Output structure; filled on success
 *
 * Returns 0 on success, -1 on any validation error.
 */
int wds_ie_parse(const u8 *ie, size_t ie_len, struct wds_ie_params *params)
{
	if (!ie || !params) {
		wpa_printf(MSG_DEBUG, "WDS IE: parse - NULL argument");
		return -1;
	}

	os_memset(params, 0, sizeof(*params));

	/* Need at least OUI(3) + Type(1) + Cap(1) + Ver(1) */
	if (ie_len < WDS_IE_PAYLOAD_LEN) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: parse - payload too short (%zu < %d)",
			   ie_len, WDS_IE_PAYLOAD_LEN);
		return -1;
	}

	/* Verify OUI */
	if (ie[0] != WDS_IE_OUI_0 || ie[1] != WDS_IE_OUI_1 ||
	    ie[2] != WDS_IE_OUI_2) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: parse - OUI mismatch %02x:%02x:%02x",
			   ie[0], ie[1], ie[2]);
		return -1;
	}

	/* Verify OUI type */
	if (ie[3] != WDS_IE_OUI_TYPE) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: parse - OUI type mismatch 0x%02x", ie[3]);
		return -1;
	}

	params->capability = ie[4];
	params->version = ie[5];

	wpa_printf(MSG_DEBUG,
		   "WDS IE: parsed cap=0x%02x ver=%u",
		   params->capability, params->version);

	return 0;
}


/* ================================================================== */
/* AP (hostapd) mode - IE building                                      */
/* ================================================================== */

/**
 * hostapd_wds_ie_len_extn - Return WDS IE wire length for AP frames
 *
 * Called by beacon.c / ieee802_11.c when pre-calculating buffer sizes.
 */
size_t hostapd_wds_ie_len_extn(struct hostapd_data *hapd)
{
	if (!hapd || !hapd->conf)
		return 0;

	if (!hapd->conf->bss_extn.wds_ie)
		return 0;

	return WDS_IE_TOTAL_LEN;
}


/**
 * hostapd_eid_wds_ie_extn - Append WDS vendor IE to an AP frame buffer
 *
 * Writes the WDS IE advertising WDS_IE_CAP_AP when wds_ie is enabled.
 * Used for beacon, probe response, and association response frames.
 *
 * @hapd: hostapd BSS data
 * @eid:  Current write position in the frame buffer
 * @len:  Remaining bytes available at @eid
 *
 * Returns the updated write pointer.
 */
u8 *hostapd_eid_wds_ie_extn(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	size_t wds_ie_len;

	if (!hapd || !hapd->conf || !eid)
		return eid;

	if (!hapd->conf->bss_extn.wds_ie)
		return eid;

	wds_ie_len = wds_ie_build(eid, len, WDS_IE_CAP_AP);
	if (wds_ie_len < WDS_IE_TOTAL_LEN) {
		wpa_printf(MSG_WARNING,
			   "WDS IE: %s - failed to build AP IE (buf too small)",
			   hapd->conf->iface);
		return eid;
	}

	wpa_printf(MSG_DEBUG, "WDS IE: %s - added AP capability IE",
		   hapd->conf->iface);

	return eid + wds_ie_len;
}


/* ================================================================== */
/* AP (hostapd) mode - IE parsing and WDS STA management               */
/* ================================================================== */

/**
 * check_wds_ie_extn - Validate WDS IE in an association request
 *
 * Parses the WDS vendor IE carried in the association request and sets
 * sta->sta_extn.wds_ie_peer when the peer advertises WDS_IE_CAP_STA.
 * The WDS IE is entirely optional; its absence is not an error.
 *
 * @hapd:       hostapd BSS data
 * @sta:        Associating station
 * @wds_ie:     Pointer to the OUI byte of the WDS IE payload
 *              (i.e. after the EID and Length octets), as stored in
 *              elems->elems_extn.wds_ie.  May be NULL.
 * @wds_ie_len: Length field value (OUI + Type + Cap + Ver = 6 minimum),
 *              as stored in elems->elems_extn.wds_ie_len.
 *
 * Returns WLAN_STATUS_SUCCESS in all cases.
 */
u16 check_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *wds_ie, size_t wds_ie_len)
{
	struct wds_ie_params params;
	struct sta_info_extn *sta_extn;

	if (!hapd || !sta)
		return WLAN_STATUS_SUCCESS;

	sta_extn = &sta->sta_extn;
	sta_extn->wds_ie_peer = false;

	/* Peer did not include a WDS IE - WDS is optional, allow association */
	if (!wds_ie || wds_ie_len < WDS_IE_PAYLOAD_LEN) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: " MACSTR " - no WDS IE in assoc req "
			   "(WDS mode will not be activated)",
			   MAC2STR(sta->addr));
		return WLAN_STATUS_SUCCESS;
	}

	/*
	 * wds_ie points to the OUI byte (payload after EID and Length).
	 * wds_ie_parse() expects the payload starting at the OUI byte.
	 */
	if (wds_ie_parse(wds_ie, wds_ie_len, &params) < 0) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: " MACSTR " - malformed WDS IE, ignoring",
			   MAC2STR(sta->addr));
		return WLAN_STATUS_SUCCESS;
	}

	if (params.capability & WDS_IE_CAP_STA) {
		sta_extn->wds_ie_peer = true;
		wpa_printf(MSG_INFO,
			   "WDS IE: " MACSTR
			   " advertises WDS STA capability "
			   "(cap=0x%02x ver=%u) - WDS mode eligible",
			   MAC2STR(sta->addr),
			   params.capability, params.version);
	} else {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: " MACSTR
			   " WDS IE present but WDS_IE_CAP_STA not set "
			   "(cap=0x%02x) - WDS mode not activated",
			   MAC2STR(sta->addr), params.capability);
	}

	return WLAN_STATUS_SUCCESS;
}


/**
 * handle_assoc_cb_wds_ie_extn - Enable WDS mode after successful association
 *
 * Called from handle_assoc_cb() after the driver confirms the association.
 * Activates WDS mode via hostapd_set_wds_sta() only when BOTH conditions
 * are satisfied:
 *   1. The AP BSS has wds_ie=1 configured.
 *   2. The peer advertised WDS_IE_CAP_STA in its association request.
 *
 * @hapd: hostapd BSS data
 * @sta:  Newly associated station
 */
void handle_assoc_cb_wds_ie_extn(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct sta_info_extn *sta_extn;
	char ifname_wds[IFNAMSIZ + 1];
	int ret, aid;

	if (!hapd || !sta)
		return;

	sta_extn = &sta->sta_extn;

	/*
	 * Determine AID once — needed for both the teardown and enable paths.
	 * For MLD the wds_mld_uid is used; for non-MLD, sta->aid.
	 */
	if (hapd->conf->mld_ap) {
		if (hostapd_get_wds_mld_sta_uid(hapd, sta) < 0) {
			wpa_printf(MSG_DEBUG, "WDS IE: No room for uid "
				   "to enable 4-address WDS mode for STA "
				   MACSTR, MAC2STR(sta->addr));
			return;
		}
		aid = sta->wds_mld_uid;
	} else {
		aid = sta->aid;
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		aid = sta->aid;
#endif /* CONFIG_QCN_EXTN */

	/*
	 * Stale AP_VLAN teardown: the STA had an active WDS_IE interface from
	 * a previous association but has returned without advertising WDS_IE.
	 * Tear it down now (deferred from disassoc, mirroring normal WDS) so
	 * the STA is accepted as a plain 3-address client with no stale VLAN.
	 */
	if (sta_extn->wds_ie_active && !sta_extn->wds_ie_peer) {
		wpa_printf(MSG_INFO,
			   "WDS IE: %s - tearing down stale WDS interface for "
			   MACSTR " (reassociated without WDS_IE, aid=%d)",
			   hapd->conf->iface, MAC2STR(sta->addr), aid);
		hostapd_set_wds_sta(hapd, NULL, sta->addr, aid, 0);
		sta_extn->wds_ie_active = false;
		return;
	}

	/* Mutual capability check: both AP and STA must advertise WDS */
	if (!hapd->conf->bss_extn.wds_ie || !sta_extn->wds_ie_peer)
		return;

	wpa_printf(MSG_INFO,
		   "WDS IE: %s - enabling WDS mode for " MACSTR
		   " (mutual WDS IE advertisement confirmed, aid=%d)",
		   hapd->conf->iface, MAC2STR(sta->addr), aid);

	/* Set WDS/Multi-AP flags on partner links if needed */
	hostapd_set_sta_flag_to_partner_links(hapd, sta);

	os_memset(ifname_wds, 0, sizeof(ifname_wds));
	ret = hostapd_set_wds_sta(hapd, ifname_wds, sta->addr, aid, 1);
	if (ret) {
		wpa_printf(MSG_WARNING,
			   "WDS IE: %s - hostapd_set_wds_sta enable failed "
			   "for " MACSTR " (ret=%d)",
			   hapd->conf->iface, MAC2STR(sta->addr), ret);
	} else {
		sta_extn->wds_ie_active = true;
		wpa_printf(MSG_INFO,
			   "WDS IE: %s - WDS mode enabled for " MACSTR
			   " wds_ifname=%s",
			   hapd->conf->iface, MAC2STR(sta->addr), ifname_wds);
	}
}


/**
 * ap_free_sta_wds_ie_extn - Disable WDS mode when a WDS-IE station leaves
 *
 * Called from ap_free_sta() before the station entry is freed.
 * Invokes hostapd_set_wds_sta(..., 0) to tear down the WDS interface
 * when the departing station had previously been put into WDS mode.
 *
 * @hapd: hostapd BSS data
 * @sta:  Departing station
 */
void ap_free_sta_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sta_info_extn *sta_extn;
	struct hostapd_data *phapd;
	struct sta_info *psta;
	int aid;

	if (!hapd || !sta)
		return;

	sta_extn = &sta->sta_extn;

	if (!hapd->conf->bss_extn.wds_ie || !sta_extn->wds_ie_active)
		return;

	/*
	 * For MLD STAs, only tear down the WDS interface when the last link
	 * is gone.  Per-link AP-STA-DISCONNECTED fires for each removed link
	 * individually; tearing down wlan0.staX on the first one breaks the
	 * bridge path for the remaining links (wds_sta mode avoids this by
	 * doing the same check in ap_free_sta()).
	 */
	if (ap_sta_is_mld(hapd, sta)) {
		for_each_mld_link(phapd, hapd) {
			if (phapd == hapd)
				continue;
			psta = ap_get_sta(phapd, sta->addr);
			if (psta) {
				wpa_printf(MSG_DEBUG,
					   "WDS IE: %s - skipping WDS teardown for "
					    MACSTR " (still connected on partner link)",
					    hapd->conf->iface, MAC2STR(sta->addr));
				return;
			}
		}
	}

	if (hapd->conf->mld_ap)
		aid = sta->wds_mld_uid;
	else
		aid = sta->aid;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		aid = sta->aid;
#endif /* CONFIG_QCN_EXTN */

	wpa_printf(MSG_DEBUG,
		   "WDS IE: %s - disabling WDS mode for " MACSTR
		   " (station disconnected)",
		   hapd->conf->iface, MAC2STR(sta->addr));

	hostapd_set_wds_sta(hapd, NULL, sta->addr, aid, 0);
	sta_extn->wds_ie_active = false;
	sta_extn->wds_ie_peer = false;
}
