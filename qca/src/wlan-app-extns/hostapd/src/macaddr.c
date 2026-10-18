/*
 * Copyright (c) 2002-2021, Jouni Malinen <j@w1.fi>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "ap/ap_config.h"
#include "ap/hw_features.h"
#include "ap/acs.h"
#include "cmn.h"

int wpa_driver_nl80211_vendor_bss_addr(void *priv, u8 radio_idx, u8 bss_id,
				       u32 mbssid_enabled,
				       enum nl80211_iftype iftype, u32 flags,
				       u8 *addr, const char *ifname);

#define MAX_NUM_MAC_ADDRESS 16

static int hostapd_get_bss_index(struct hostapd_data *hapd)
{
	u32 used_mask = 0;
	int i;

	if (hapd->iface->num_bss == 0)
		return 0;

	if (hapd->conf->bss_index >= 0) {
		if (hapd->conf->bss_index >= MAX_NUM_MAC_ADDRESS) {
			wpa_printf(MSG_DEBUG,
				   "bss_index=%d out of range (max %d)",
				   hapd->conf->bss_index, MAX_NUM_MAC_ADDRESS);
			return -1;
		}
		return hapd->conf->bss_index;
	}

	/* Reuse previously derived index for this BSS if valid */
	if (hapd->vendor_bss_index_valid)
		return hapd->vendor_bss_index;

	/* Start with cached mask on the interface for efficiency */
	used_mask = hapd->iface->vendor_bssid_used_mask;

	/*
	 * Ensure we also respect per-BSS overrides and indices
	 * already taken by other BSSes.
	 */
	for (i = 0; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *other = hapd->iface->bss[i];
		int idx;

		if (!other || other == hapd)
			continue;
		if (other->conf->bss_index >= MAX_NUM_MAC_ADDRESS)
			continue;

		if (other->conf->bss_index >= 0)
			idx = other->conf->bss_index;
		else if (other->vendor_bss_index_valid)
			idx = other->vendor_bss_index;
		else
			continue;

		if (idx < MAX_NUM_MAC_ADDRESS)
			used_mask |= BIT(idx);
	}

	/* Pick the first unused index in [0..MAX_NUM_MAC_ADDRESS-1] */
	for (i = 0; i < MAX_NUM_MAC_ADDRESS; i++)
		if (!(used_mask & BIT(i)))
			break;

	if (i >= MAX_NUM_MAC_ADDRESS)
		return -1;

	hapd->vendor_bss_index = i;
	hapd->vendor_bss_index_valid = true;
	hapd->iface->vendor_bssid_used_mask |= BIT(i);

	return i;
}

static int iface_channel_to_freq(struct hostapd_iface *iface, int channel)
{
	int freq = 0;
	int i, j;

	if (!channel)
		return 0;
	if (iface->conf->op_class) {
		freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
					      channel);
		if (freq < 0) {
			wpa_printf(MSG_INFO,
				   "Convert op_class %u chan %u to freq failed",
				   iface->conf->op_class, channel);
			return 0;
		}
		return freq;
	}

	/*
	 * Old configurations using only 2.4/5/60 GHz bands may not specify the
	 * op_class parameter. Select a matching channel from the configured
	 * mode using the channel parameter for these cases.
	 */
	for (j = 0; j < iface->num_hw_features; j++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[j];

		if (iface->conf->hw_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    iface->conf->hw_mode != mode->mode)
			continue;
		for (i = 0; i < mode->num_channels; i++) {
			struct hostapd_channel_data *chan = &mode->channels[i];

			if (chan->chan == channel &&
			    !is_6ghz_freq(chan->freq)) {
				freq = chan->freq;
				return freq;
			}
		}
	}

	wpa_printf(MSG_INFO, "Could not determine operating frequency");
	return 0;
}

static int prepare_iface_for_vendor_bssid(struct hostapd_iface *iface)
{
	if (iface->current_hw_info)
		return 0;

	/* Fetch hardware features */
	if (!hostapd_get_hw_features(iface)) {
		/*
		 * If there is only a single underlying hardware,
		 * select it.
		 */
		if (iface->num_multi_hws == 1)
			iface->current_hw_info = &iface->multi_hw_info[0];
		else {
			int channel = iface->conf->channel;
			int freq = iface->freq;

			/*
			 * If operating frequency not yet known,
			 * try using chan/chanlist/freqlist from
			 * config to select the hw info.
			 */
			if (!channel && !freq) {
			    if (iface->conf->acs_freq_list.num)
				    freq =
					iface->conf->acs_freq_list.range->min;
			    else if (iface->conf->acs_ch_list.num)
				    channel =
					iface->conf->acs_ch_list.range->min;
			}

			if (!freq && channel)
				freq = iface_channel_to_freq(iface, channel);

			if (freq)
				hostapd_set_current_hw_info(iface,
							    freq);
		}
	}

	if (iface->current_hw_info)
		return 0;

	return -1;
}

static int hostapd_drv_vendor_bssid(struct hostapd_data *hapd,
				    u8 bss_index,
				    u8 *addr,
				    bool alloc)
{
	u32 mbssid_enabled;
	u8 radio_idx;
	const char *ifname = NULL;

	if (!hapd || !hapd->iface)
		return -1;

	if (!hapd->driver || !hapd->drv_priv)
		return -1;

	if(prepare_iface_for_vendor_bssid(hapd->iface))
		return -1;

	radio_idx = hapd->iface->current_hw_info->hw_idx;
	ifname = hapd->conf ? hapd->conf->iface : NULL;
	mbssid_enabled = hapd->iconf ? hapd->iconf->mbssid : 0;

	return wpa_driver_nl80211_vendor_bss_addr(hapd->drv_priv,
						  radio_idx,
						  bss_index,
						  mbssid_enabled,
						  NL80211_IFTYPE_AP,
						  alloc ? 0x1 : 0x2,
						  addr,
						  ifname);
}

static int hostapd_drv_set_mac_addr(struct hostapd_data *hapd,
				    const u8 *addr)
{
	if (!hapd->driver || !hapd->driver->set_mac_addr || !hapd->drv_priv)
		return -1;
	return hapd->driver->set_mac_addr(hapd->drv_priv, addr);
}

int hostapd_drv_fetch_and_set_vendor_bssid_extn(struct hostapd_data *hapd)
{
	u8 ven_bssid[ETH_ALEN];
	int drv_idx;

	if (!hapd->iconf)
		return -1;

	if (!hapd->iconf->use_driver_vendor_addr)
		return -1;

	drv_idx = hostapd_get_bss_index(hapd);

	if (drv_idx != -1 &&
	    !hostapd_drv_vendor_bssid(hapd, drv_idx, ven_bssid, true)) {
		wpa_printf(MSG_DEBUG,
			   "fetch vendor BSSID successful");
		os_memcpy(hapd->own_addr, ven_bssid, ETH_ALEN);
		if (!hapd->conf->mld_ap) {
			if (!hostapd_drv_set_mac_addr(hapd, hapd->own_addr)) {
				wpa_printf(MSG_DEBUG,
					   "updated vendor BSSID for %s to " MACSTR,
					   hapd->conf->iface,
					   MAC2STR(hapd->own_addr));
				return 0;
			} else {
				hostapd_drv_vendor_bssid(hapd, drv_idx,
							 ven_bssid, false);
				wpa_printf(MSG_DEBUG,
					   "update vendor BSSID failed for %s",
					   hapd->conf->iface);
				return -1;
			}
		}

		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "fetch vendor BSSID failed");
	return -1;
}

void hostapd_free_bss_index_extn(struct hostapd_data *hapd)
{
	u8 ven_bssid[ETH_ALEN];
	int drv_idx;

	if (!hapd)
		return;

	if (hapd->iface)
		hostapd_dcs_iface_deinit_extn(hapd->iface);

	if (!hapd->iconf)
		return;

	if (!hapd->iconf->use_driver_vendor_addr)
		return;

	if (hapd->vendor_bss_index_valid) {
		drv_idx = hapd->vendor_bss_index;
		hapd->iface->vendor_bssid_used_mask &= ~BIT(hapd->vendor_bss_index);
		hapd->vendor_bss_index_valid = false;
	} else if (hapd->conf->bss_index >= 0)
		drv_idx = hapd->conf->bss_index;
	else
		return;

	hostapd_drv_vendor_bssid(hapd, drv_idx,
				 ven_bssid, false);
}

