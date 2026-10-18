// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include <netlink/genl/genl.h>
#include "utils/common.h"
#include "common/qca-vendor.h"
#include "drivers/driver.h"
#include "drivers/driver_nl80211.h"
#include "common/ieee802_11_common.h"
#include "ap/hostapd.h"
#include "ap/beacon.h"
#include "common/wpa_ctrl.h"
#include "cmn.h"
#include "esp.h"


/**
 * hostapd_drv_set_esp_param_extn - Set ESP parameter via vendor command
 * @hapd: Pointer to hostapd_data
 * @param: Parameter name (esp_airtime, esp_ppdu_dur, esp_ba_window, enable_esp)
 * @val: Parameter value
 * Returns: 0 on success, -1 on failure
 *
 * Uses QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION (generic framework)
 */
int hostapd_drv_set_esp_param_extn(struct hostapd_data *hapd, const char *param,
				   const int val)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	struct wpa_driver_nl80211_data *drv;
	struct nlattr *params, *esp_params;
	struct i802_bss *bss;
	struct nl_msg *msg;
	int i, ret;

	if (!param) {
		wpa_printf(MSG_ERROR, "ESP: Invalid parameters");
		return -1;
	}

	if (!hapd->driver || !hapd->driver->send_mlme) {
		wpa_printf(MSG_ERROR, "ESP: Driver not initialized");
		return -1;
	}

	bss = hapd->drv_priv;
	if (!bss) {
		wpa_printf(MSG_ERROR, "ESP: BSS not initialized");
		return -1;
	}
	drv = bss->drv;
	if (!drv || !drv->global || !drv->global->nl) {
		wpa_printf(MSG_ERROR, "ESP: Driver netlink context is not available");
		return -ENODEV;
	}

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (hapd->mld_link_id != NL80211_DRV_LINK_ID_NA &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID,
		       hapd->mld_link_id))
		goto fail;

	esp_params = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS);
	if (!esp_params)
		goto fail;

	if (os_strcmp(param, "esp_airtime") == 0) {
		if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE,
				val))
			goto fail;
		wpa_printf(MSG_DEBUG, "ESP: Setting airtime=%d", val);
	} else if (os_strcmp(param, "esp_ppdu_dur") == 0) {
		if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PPDU_DUR_BE,
			       val))
			goto fail;
		wpa_printf(MSG_DEBUG, "ESP: Setting PPDU duration=%d", val);
	} else if (os_strcmp(param, "esp_ba_window") == 0) {
		if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_BA_WINDOW_BE,
			       val))
			goto fail;
		wpa_printf(MSG_DEBUG, "ESP: Setting BA window=%d", val);
	} else if (os_strcmp(param, "enable_esp") == 0) {
		if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_ENABLE,
			       val))
			goto fail;
		wpa_printf(MSG_DEBUG, "ESP: Setting enable=%d", val);
	} else {
		wpa_printf(MSG_ERROR, "ESP: Unknown parameter '%s'", param);
		goto fail;
	}

	nla_nest_end(msg, esp_params);
	nla_nest_end(msg, params);

	ret = send_and_recv_cmd(drv, msg);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "ESP: Failed to send vendor command: %s (%d)",
			   strerror(-ret), ret);
		return ret;
	}

	/* Store values in iface structure after successful driver update */
	if (os_strcmp(param, "esp_airtime") == 0)
		iface_extn->esp.airtime = val;
	else if (os_strcmp(param, "esp_ppdu_dur") == 0)
		iface_extn->esp.ppdu_dur = val;
	else if (os_strcmp(param, "esp_ba_window") == 0)
		iface_extn->esp.ba_window = val;
	else if (os_strcmp(param, "enable_esp") == 0)
		iface_extn->esp.enable = val;

	for (i = 0; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *bss = hapd->iface->bss[i];

		if (!bss || !bss->started || bss->disabled)
			continue;
		ieee802_11_set_beacon_per_bss_only(bss);
#ifdef CONFIG_IEEE80211BE
		if (bss->conf && bss->conf->mld_ap)
			hostapd_gen_per_sta_profiles(bss);
#endif /* CONFIG_IEEE80211BE */
	}

	return 0;

fail:
	wpa_printf(MSG_ERROR, "ESP: Failed to build vendor command");
	nlmsg_free(msg);
	return -1;
}


/**
 * nl80211_parse_esp_params_extn - Parse and handle ESP parameters from vendor event
 * @bss: Pointer to struct i802_bss
 * @esp_params_attr: Nested ESP parameters attribute
 * @link_id: MLD link id
 * Returns: 0 on success, -1 on failure
 *
 * Parses nested ESP parameters from QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS
 * and updates the corresponding values in hostapd_iface structure.
 */
int nl80211_parse_esp_params_extn(struct i802_bss *bss,
				  struct nlattr *esp_params_attr,
				  u8 link_id)
{
	struct nlattr *esp_params[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_MAX + 1] = {0};
	struct esp_update_event *esp_event;
	union wpa_event_data event;
	uint32_t esp_airtime;
	struct nlattr *attr;
	int rem;

	if (!esp_params_attr) {
		wpa_printf(MSG_ERROR, "ESP: esp_params_attr is NULL - vendor event handler bug!");
		return -1;
	}

	os_memset(&event, 0, sizeof(event));
	esp_event = &event.event_data_extn.esp_update_event;
	esp_event->link_id = link_id;

	nla_for_each_nested(attr, esp_params_attr, rem) {
		int type = nla_type(attr);

		if (type <= QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_MAX)
			esp_params[type] = attr;
	}

	if (!esp_params[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE]) {
		wpa_printf(MSG_ERROR,
			   "ESP: QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE not present");
		return -EINVAL;
	}

	esp_airtime = nla_get_u32(esp_params[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_AIRTIME_BE]);
	wpa_printf(MSG_INFO, "ESP: Successfully parsed! Airtime=%u", esp_airtime);
	esp_event->airtime = esp_airtime;

	wpa_supplicant_event(bss->ctx, EVENT_ESP_UPDATE, &event);

	return 0;
}


/**
 * hostapd_update_esp_params_extn - update airtime fraction for the hapd interface
 * @hapd: Pointer to hostapd_data
 * @data: WPA event data
 */
void hostapd_update_esp_params_extn(struct hostapd_data *hapd,
				    union wpa_event_data *data)
{
	struct esp_update_event *esp_event;
	struct hostapd_data *link_hapd;

	if (!data) {
		wpa_printf(MSG_ERROR, "WPA event with NULL data");
		return;
	}

	esp_event = &data->event_data_extn.esp_update_event;

	link_hapd = switch_link_hapd(hapd, esp_event->link_id);
	if (!link_hapd)
		return;

	link_hapd->iface->iface_extn.esp.computed_airtime = esp_event->airtime;
	wpa_printf(MSG_INFO, "ESP: Updated computed_airtime=%u",
		   esp_event->airtime);
}


/**
 * hostapd_esp_ie_len_extn - Get ESP IE length
 * @hapd: Pointer to hostapd_data
 * Returns: ESP IE length
 */
size_t hostapd_esp_ie_len_extn(struct hostapd_data *hapd)
{
	if (!hapd || !hapd->iface || !hapd->iface->iface_extn.esp.enable)
		return 0;

	return 2 /* IE header */ + 1 /* ext id */ + 3 /* esp info field */;
}


/**
 * hostapd_eid_esp_extn - Adds Estimated Service Parameters (ESP) IE
 * @hapd: Pointer to hostapd_data
 * @eid: Position in buffer
 * @len: Length of buffer
 * Returns: Updated pointer of the position in buffer
 *
 * Writes a single-AC ESP IE (EID 255, ext 11) and returns updated pointer.
 */
u8 * hostapd_eid_esp_extn(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	struct hostapd_iface_extn *iface_extn;
	u8 *pos = eid;
	u8 head;

	if (!hapd || !hapd->iface || !hapd->iface->iface_extn.esp.enable)
		return pos;

	iface_extn = &hapd->iface->iface_extn;

	if (len < (size_t)(2 + 1 + 3))
		return pos;

	/* Element ID Extension */
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + 3; /* length: ext id + esp info field */
	*pos++ = WLAN_EID_EXT_ESTIMATED_SERVICE_PARAMS;

	head = 0x1; /* Access Category: BE */
	head |= (0x3 << 3); /* Data Format: AMPDU and AMSDU enabled */
	if (iface_extn->esp.ba_window)
		head |= (iface_extn->esp.ba_window & 0x7) << 5; /* BA Window Size as entered by user */
	else
		head |= (0x5 << 5); /* Default BA Window Size of 16 */
	*pos++ = head;

	/* Airtime fraction */
	if (iface_extn->esp.airtime)
		*pos++ = iface_extn->esp.airtime;
	else
		*pos++ = iface_extn->esp.computed_airtime;

	/* PPDU duration target */
	if (iface_extn->esp.ppdu_dur)
		*pos++ = iface_extn->esp.ppdu_dur;
	else
		*pos++ = ESP_DEFAULT_PPDU_DURATION;

	return pos;
}
