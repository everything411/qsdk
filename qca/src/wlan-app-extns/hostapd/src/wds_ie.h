/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * WDS Vendor IE - Generic WDS capability advertisement framework
 *
 * This module implements a vendor-neutral WDS (Wireless Distribution System)
 * Information Element for advertising WDS capability in beacon, probe response,
 * association request, and association response frames.
 *
 * IE Wire Format:
 *   Octet 0:   Element ID = 0xDD (WLAN_EID_VENDOR_SPECIFIC)
 *   Octet 1:   Length = 6
 *   Octet 2-4: OUI = 00:13:74  (WDS IE generic OUI)
 *   Octet 5:   OUI Type = 0x01
 *   Octet 6:   WDS Capability flags
 *                BIT(0) = WDS_IE_CAP_AP  - device operates as WDS AP
 *                BIT(1) = WDS_IE_CAP_STA - device operates as WDS STA
 *   Octet 7:   WDS IE Version = 0x01
 *
 * Total IE size on the wire: 8 bytes.
 *
 * Capability negotiation:
 *   WDS mode is activated only when BOTH peers mutually advertise WDS support:
 *   - AP must have wds_ie=1 in hostapd.conf and advertise WDS_IE_CAP_AP.
 *   - STA must have wds_ie=1 in wpa_supplicant.conf and advertise WDS_IE_CAP_STA.
 *   - AP calls hostapd_set_wds_sta(..., 1) when both AP and STA advertise/enable this support
 *   - AP calls hostapd_set_wds_sta(..., 0) when the WDS STA disconnects.
 */

#ifndef WDS_IE_H
#define WDS_IE_H

#include "utils/includes.h"
#include "utils/common.h"

/* ------------------------------------------------------------------ */
/* WDS IE OUI - generic vendor-neutral OUI for WDS capability          */
/* ------------------------------------------------------------------ */
#define WDS_IE_OUI_0    0x00
#define WDS_IE_OUI_1    0x13
#define WDS_IE_OUI_2    0x84

/* OUI type byte that identifies this as a WDS IE */
#define WDS_IE_OUI_TYPE 0x01

/* Current WDS IE version */
#define WDS_IE_VERSION  0x01

/* ------------------------------------------------------------------ */
/* WDS capability flags (Octet 6 of the IE)                            */
/* ------------------------------------------------------------------ */
#define WDS_IE_CAP_AP   BIT(0)  /* Sender operates as WDS AP  */
#define WDS_IE_CAP_STA  BIT(1)  /* Sender operates as WDS STA */

/* ------------------------------------------------------------------ */
/* IE size constants                                                    */
/* ------------------------------------------------------------------ */
/* Payload = OUI(3) + Type(1) + Cap(1) + Ver(1) = 6 bytes */
#define WDS_IE_PAYLOAD_LEN  6
/* Total wire size = EID(1) + Len(1) + Payload(6) = 8 bytes */
#define WDS_IE_TOTAL_LEN    8

/* ------------------------------------------------------------------ */
/* Parsed WDS IE parameters                                             */
/* ------------------------------------------------------------------ */
/**
 * struct wds_ie_params - Decoded WDS IE fields
 * @capability: WDS capability flags (WDS_IE_CAP_AP / WDS_IE_CAP_STA)
 * @version:    WDS IE version number
 */
struct wds_ie_params {
	u8 capability;
	u8 version;
};

/* Forward declarations (avoid pulling in full headers here) */
struct hostapd_data;
struct hostapd_bss_config;
struct sta_info;
struct wpa_supplicant;
struct wpa_ssid;

/* ------------------------------------------------------------------ */
/* Low-level IE build / parse helpers                                   */
/* ------------------------------------------------------------------ */

/**
 * wds_ie_build - Serialise a WDS vendor IE into @buf
 * @buf:        Output buffer
 * @len:        Available bytes in @buf
 * @capability: WDS capability flags to advertise
 *
 * Returns the number of bytes written (WDS_IE_TOTAL_LEN) or 0 on error.
 */
size_t wds_ie_build(u8 *buf, size_t len, u8 capability);

/**
 * wds_ie_parse - Deserialise a WDS vendor IE
 * @ie:     Pointer to the raw IE data starting at the OUI byte
 *          (i.e. after the EID and Length octets)
 * @ie_len: Number of bytes available at @ie (must be >= WDS_IE_PAYLOAD_LEN)
 * @params: Output structure filled on success
 *
 * Returns 0 on success, -1 on error.
 */
int wds_ie_parse(const u8 *ie, size_t ie_len, struct wds_ie_params *params);

/* ------------------------------------------------------------------ */
/* AP (hostapd) mode functions                                          */
/* ------------------------------------------------------------------ */

/**
 * hostapd_wds_ie_len_extn - Return the WDS IE wire length for AP frames
 * @hapd: hostapd BSS data
 *
 * Returns WDS_IE_TOTAL_LEN when wds_ie is enabled, 0 otherwise.
 * Used by callers that pre-calculate buffer sizes.
 */
size_t hostapd_wds_ie_len_extn(struct hostapd_data *hapd);

/**
 * hostapd_eid_wds_ie_extn - Append WDS vendor IE to a frame buffer (AP)
 * @hapd: hostapd BSS data
 * @eid:  Current write position in the frame buffer
 * @len:  Remaining bytes available at @eid
 *
 * Writes the WDS IE with WDS_IE_CAP_AP set when wds_ie is enabled.
 * Returns the updated write pointer (eid + WDS_IE_TOTAL_LEN on success,
 * or the original @eid if the IE was not written).
 */
u8 *hostapd_eid_wds_ie_extn(struct hostapd_data *hapd, u8 *eid, size_t len);

/**
 * check_wds_ie_extn - Validate WDS IE in an association request
 * @hapd:       hostapd BSS data
 * @sta:        Associating station
 * @wds_ie:     Pointer to the full WDS IE (EID + Length + payload), or NULL
 * @wds_ie_len: Total byte count of @wds_ie
 *
 * Sets sta->sta_extn.wds_ie_peer when the peer advertises WDS_IE_CAP_STA.
 * Returns WLAN_STATUS_SUCCESS always (WDS IE is optional).
 */
u16 check_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *wds_ie, size_t wds_ie_len);

/**
 * handle_assoc_cb_wds_ie_extn - Enable WDS mode after successful association
 * @hapd: hostapd BSS data
 * @sta:  Newly associated station
 *
 * Called from handle_assoc_cb().  Invokes hostapd_set_wds_sta(..., 1) when
 * both the AP has wds_ie enabled and the peer advertised WDS_IE_CAP_STA.
 */
void handle_assoc_cb_wds_ie_extn(struct hostapd_data *hapd,
				 struct sta_info *sta);

/**
 * ap_free_sta_wds_ie_extn - Disable WDS mode when a WDS-IE station leaves
 * @hapd: hostapd BSS data
 * @sta:  Departing station
 *
 * Called from ap_free_sta().  Invokes hostapd_set_wds_sta(..., 0) when the
 * station had wds_ie_peer set.
 */
void ap_free_sta_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta);

/* ------------------------------------------------------------------ */
/* wpa_supplicant (STA) mode functions                                  */
/* ------------------------------------------------------------------ */

/**
 * wds_ie_assoc_req_len_extn - Return WDS IE wire length for assoc request
 * @wpa_s: wpa_supplicant instance
 * @ssid:  Network profile
 *
 * Returns WDS_IE_TOTAL_LEN when ssid->wds_ie is enabled, 0 otherwise.
 */
size_t wds_ie_assoc_req_len_extn(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid);

/**
 * wds_ie_populate_assoc_req_extn - Append WDS IE to an association request
 * @wpa_s:  wpa_supplicant instance
 * @ssid:   Network profile
 * @pos:    Current write position in the IE buffer
 * @avail:  Remaining bytes available at @pos
 *
 * Writes the WDS IE with WDS_IE_CAP_STA set when ssid->wds_ie is enabled.
 * Returns the updated write pointer.
 */
u8 *wds_ie_populate_assoc_req_extn(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid,
				   u8 *pos, size_t avail);

/**
 * wds_ie_process_assoc_resp_extn - Parse WDS IE from an association response
 * @wpa_s:    wpa_supplicant instance
 * @ies:      IEs from the association response frame
 * @ies_len:  Length of @ies
 *
 * Searches for the WDS vendor IE and logs whether the AP advertises
 * WDS_IE_CAP_AP.  Sets wpa_s->wds_ie_ap accordingly.
 */
void wds_ie_process_assoc_resp_extn(struct wpa_supplicant *wpa_s,
				    const u8 *ies, size_t ies_len);

#endif /* WDS_IE_H */
