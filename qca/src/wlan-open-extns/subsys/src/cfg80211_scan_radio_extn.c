// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include <net/genetlink.h>
#include "core.h"
#include "cfg80211_scan_radio_extn.h"

/*
 * Select the best 6 GHz power mode for a scan radio channel.
 *
 * Try SP → LPI → VLP; accept the first mode where every 20 MHz
 * sub-channel is present and has neither DISABLED nor NO_IR set.
 * If none qualify, fall back to SP if all sub-channels are present and not DISABLED,
 * even if some carry NO_IR (pre-AFC state).
 *
 * Returns the selected nl80211_regulatory_power_modes value, or -1 if no
 * usable mode is found.
 */
static int scan_radio_best_power_mode(struct wiphy *wiphy,
				      u32 freq, u32 center_freq, u32 bw)
{
	struct ieee80211_channel *chan;
	struct ieee80211_supported_band *sband6 = wiphy->bands[NL80211_BAND_6GHZ];
	u32 start_freq;
	u8 num_sub, i, j;
	bool sp_mode_valid, has_no_ir;
	static const enum nl80211_regulatory_power_modes pwr_prio[] = {
		NL80211_REG_AP_SP,
		NL80211_REG_AP_LPI,
		NL80211_REG_AP_VLP,
	};

	num_sub = bw / 20;
	start_freq = (bw == 20) ? freq : center_freq - bw / 2 + 10;
	if (start_freq < 5925 || start_freq > 7125)
		return -1;

	wiphy_dbg(wiphy, "scan_radio_best_pwr: freq=%u center=%u bw=%u num_sub=%u\n",
		  freq, center_freq, bw, num_sub);

	/* Try SP → LPI → VLP; require no DISABLED and no NO_IR on all sub-channels.
	 * For SP, also track whether it failed solely due to NO_IR in the same loop.
	 */
	sp_mode_valid = false;

	for (i = 0; i < ARRAY_SIZE(pwr_prio); i++) {
		enum nl80211_regulatory_power_modes mode = pwr_prio[i];

		has_no_ir = false;

		if (!sband6->chan_6g[mode])
			continue;

		for (j = 0; j < num_sub; j++) {
			u32 sub_freq = start_freq + j * 20;

			chan = ieee80211_get_6g_channel_khz(wiphy,
							    MHZ_TO_KHZ(sub_freq), mode);
			if (!chan || (chan->flags & IEEE80211_CHAN_DISABLED))
				break;
			if (chan->flags & IEEE80211_CHAN_NO_IR)
				has_no_ir = true;
		}

		if (j < num_sub)
			continue;

		/* All sub-channels present and not DISABLED. */
		if (!has_no_ir) {
			wiphy_dbg(wiphy, "scan_radio_best_pwr: -> mode=%u\n", mode);
			return mode;
		}
		if (mode == NL80211_REG_AP_SP)
			sp_mode_valid = true;
	}

	/* SP channels all present and not DISABLED, but some carry NO_IR (pre-AFC state). */
	if (sp_mode_valid) {
		wiphy_dbg(wiphy, "scan_radio_best_pwr: -> SP (NO_IR)\n");
		return NL80211_REG_AP_SP;
	}
	return -1;
}

void cfg80211_scan_radio_inject_power_mode(struct cfg80211_registered_device *rdev,
					   struct wireless_dev *wdev,
					   struct genl_info *info,
					   struct cfg80211_scan_radio_pwr_nla *attr_buf)
{
	u32 freq, center_freq;
	int bw, mode;

	if (!wdev_is_scan_radio(wdev))
		return;

	if (!info->attrs[NL80211_ATTR_WIPHY_FREQ])
		return;

	if (info->attrs[NL80211_ATTR_6G_REG_POWER_MODE])
		return;

	if (!rdev->wiphy.bands[NL80211_BAND_6GHZ])
		return;

	freq = MHZ_TO_KHZ(nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ]));
	if (info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET])
		freq += nla_get_u32(info->attrs[NL80211_ATTR_WIPHY_FREQ_OFFSET]);
	freq /= 1000; /* convert to MHz after KHz offset is applied */

	center_freq = info->attrs[NL80211_ATTR_CENTER_FREQ1]
		? nla_get_u32(info->attrs[NL80211_ATTR_CENTER_FREQ1])
		: freq;

	bw = info->attrs[NL80211_ATTR_CHANNEL_WIDTH]
		? nl80211_chan_width_to_mhz(
			nla_get_u32(info->attrs[NL80211_ATTR_CHANNEL_WIDTH]))
		: 20;
	if (bw < 20)
		return;

	mode = scan_radio_best_power_mode(&rdev->wiphy, freq, center_freq, bw);
	if (mode < 0)
		return;

	attr_buf->hdr.nla_len  = nla_attr_size(sizeof(u8));
	attr_buf->hdr.nla_type = NL80211_ATTR_6G_REG_POWER_MODE;
	attr_buf->mode = (u8)mode;
	info->attrs[NL80211_ATTR_6G_REG_POWER_MODE] = &attr_buf->hdr;
}
