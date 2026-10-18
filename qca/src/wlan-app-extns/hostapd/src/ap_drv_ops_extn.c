// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef RDK_ONEWIFI

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "wps/wps.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "ap/sta_info.h"
#include "drivers/driver.h"

int hostapd_drv_wps_event_notify_cb(struct hostapd_data *hapd, enum wps_event event,
				    union wps_event_data *data)
{
	if (!hapd->driver || !hapd->driver->wps_event_notify_cb || !hapd->drv_priv) {
		return 0;
	}

	return hapd->driver->wps_event_notify_cb(hapd->drv_priv, event, (union wps_event_data *)data);
}

int hostapd_drv_sta_notify_deauth(struct hostapd_data *hapd,
				  const u8 *addr, int reason)
{
	if (!hapd->driver || !hapd->driver->sta_notify_deauth || !hapd->drv_priv)
		return 0;
	return hapd->driver->sta_notify_deauth(hapd->drv_priv, hapd->own_addr, addr,
					       reason);
}

size_t hostapd_drv_eid_rnr_colocation_len(struct hostapd_data *hapd,
					  size_t *current_len)
{
	if (!hapd->driver || !hapd->driver->get_rnr_colocation_len || !hapd->drv_priv)
		return 0;

	return hapd->driver->get_rnr_colocation_len(hapd->drv_priv, current_len);
}

u8 * hostapd_drv_eid_rnr_colocation(struct hostapd_data *hapd, u8 *eid,
				    size_t *current_len)
{
	if (!hapd->driver || !hapd->driver->get_rnr_colocation_ie || !hapd->drv_priv)
		return eid;

	return hapd->driver->get_rnr_colocation_ie(hapd->drv_priv, eid, current_len);
}

int hostapd_drv_get_sta_auth_type(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *ies, size_t ies_len, int frame_type)
{
	struct ieee802_11_elems elems;
	const u8 *wpa_ie;
	int res;
	size_t wpa_ie_len;
	struct wpa_ie_data data;

	if (!hapd->driver || !hapd->driver->get_sta_auth_type)
		return -1;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR, "%s Failed to parse elements\n", __func__);
		return -1;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && elems.rsn_ie) {
		wpa_ie = elems.rsn_ie;
		wpa_ie_len = elems.rsn_ie_len;
	} else if ((hapd->conf->wpa & WPA_PROTO_WPA) && elems.wpa_ie) {
		wpa_ie = elems.wpa_ie;
		wpa_ie_len = elems.wpa_ie_len;
	} else {
		wpa_ie = NULL;
		wpa_ie_len = 0;
	}

	if (wpa_ie == NULL) {
		wpa_printf(MSG_ERROR, "wpa ie is NULL in %s\n", __func__);
		return -1;
	}

	if (hapd->conf->wpa && wpa_ie) {
		wpa_ie -= 2;
		wpa_ie_len += 2;
	}

	res = wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, &data);
	if (res) {
		wpa_printf(MSG_ERROR, "RSN_IE %s Failed to parse wpa IE \n", __func__);
		return -1;
	}

	return hapd->driver->get_sta_auth_type(hapd->drv_priv, addr, data.key_mgmt, frame_type);
}

struct hostapd_data * hostapd_drv_mbssid_get_tx_bss(struct hostapd_data *hapd)
{
	if (!hapd->driver || !hapd->driver->get_mbssid_tx_bss || !hapd->drv_priv)
		return hapd;

	return hapd->driver->get_mbssid_tx_bss(hapd->drv_priv);
}

int hostapd_drv_mbssid_get_bss_index(struct hostapd_data *hapd)
{
	if (!hapd->driver || !hapd->driver->get_mbssid_bss_index || !hapd->drv_priv)
		return 0;

	return hapd->driver->get_mbssid_bss_index(hapd->drv_priv);
}

size_t hostapd_drv_eid_mbssid_len(struct hostapd_data *hapd, u32 frame_type,
				  u8 *elem_count, const u8 *known_bss,
				  size_t known_bss_len, size_t *rnr_len)
{
	if (!hapd->driver || !hapd->driver->get_mbssid_len || !hapd->drv_priv)
		return 0;

	return hapd->driver->get_mbssid_len(hapd->drv_priv, frame_type, elem_count);
}

u8 * hostapd_drv_eid_mbssid(struct hostapd_data *hapd, u8 *eid, u8 *end,
			    unsigned int frame_stype, u8 elem_count,
			    u8 **elem_offset,
			    const u8 *known_bss, size_t known_bss_len, u8 *rnr_eid,
			    u8 *rnr_count, u8 **rnr_offset, size_t rnr_len)
{
	if (!hapd->driver || !hapd->driver->get_mbssid_ie || !hapd->drv_priv)
		return eid;

	return hapd->driver->get_mbssid_ie(hapd->drv_priv, eid, end, frame_stype,
					   elem_count, elem_offset);
}

u8 * hostapd_drv_mbssid_config(struct hostapd_data *hapd, u8 *eid)
{
	if (!hapd->driver || !hapd->driver->get_mbssid_config || !hapd->drv_priv)
		return eid;

	return hapd->driver->get_mbssid_config(hapd->drv_priv, eid);
}

#endif /* RDK_ONEWIFI */
