/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __CFG80211_SCAN_RADIO_EXTN_H
#define __CFG80211_SCAN_RADIO_EXTN_H

#include <net/cfg80211.h>

/**
 * DOC: Scan radio 6 GHz power mode extensions
 *
 * A scan radio is a dedicated receive-only radio used for Spectral Analysis,
 * Monitoring, and Network Profiling, etc.
 * Channels marked with %IEEE80211_CHAN_NO_IR must be treated as
 * reception-only; transmission on them is strictly prohibited.
 *
 * For 6 GHz channels the kernel requires an explicit AP power mode
 * (%NL80211_ATTR_6G_REG_POWER_MODE) to be present in every channel-change
 * command.  User space tools targeting a normal AP or STA always supply this
 * attribute, but scan radio control paths typically do not.  The helpers in
 * this file fill that gap:
 *
 * - cfg80211_scan_radio_inject_power_mode() selects the best available power
 *   mode and synthesises a temporary &struct nlattr so that the standard
 *   nl80211 chandef parser sees it as if user space had provided it.
 */

struct cfg80211_registered_device;

/**
 * struct cfg80211_scan_radio_pwr_nla - nlattr wrapper for power mode
 * @hdr:  nlattr header; nla_len and nla_type are filled in by
 *        cfg80211_scan_radio_inject_power_mode() before injection.
 * @mode: The selected &enum nl80211_regulatory_power_modes value (u8).
 *
 * This structure can be cast to &struct nlattr and injected into
 * &struct genl_info.attrs[].
 */
struct cfg80211_scan_radio_pwr_nla {
	struct nlattr hdr;
	u8 mode;
};

/**
 * cfg80211_scan_radio_inject_power_mode - select and inject 6 GHz power mode
 *   for a scan radio channel-change command
 * @rdev:     The cfg80211 registered device whose wiphy holds the 6 GHz band
 *            channel tables.
 * @wdev:     The wireless device; the function is a no-op if this is not a
 *            scan radio (wdev_is_scan_radio() returns false).
 * @info:     The netlink command info; attrs[] is updated in-place when a
 *            suitable power mode is found.
 * @attr_buf: &struct cfg80211_scan_radio_pwr_nla provided by the caller;
 *            used to synthesise the %NL80211_ATTR_6G_REG_POWER_MODE attribute.
 *
 * Selects the best available 6 GHz AP power mode for the frequency indicated
 * by %NL80211_ATTR_WIPHY_FREQ and injects it into @info->attrs so that
 * _nl80211_parse_chandef() sees it without requiring user space to supply it.
 *
 * Selection order: SP → LPI → VLP.
 * Pass 1: First mode whose channel has neither %IEEE80211_CHAN_DISABLED nor
 *         %IEEE80211_CHAN_NO_IR set.
 * Pass 2: If no such mode is found, fall back to SP provided the channel is
 *         not %IEEE80211_CHAN_DISABLED (%IEEE80211_CHAN_NO_IR is accepted so
 *         that the scan radio can start operating before an AFC response
 *         arrives).
 *
 * The function does nothing if @info already contains
 * %NL80211_ATTR_6G_REG_POWER_MODE, if the frequency is not in the 6 GHz
 * band, or if the wiphy does not expose a 6 GHz band.
 */
void cfg80211_scan_radio_inject_power_mode(struct cfg80211_registered_device *rdev,
					   struct wireless_dev *wdev,
					   struct genl_info *info,
					   struct cfg80211_scan_radio_pwr_nla *attr_buf);

#endif /* __CFG80211_SCAN_RADIO_EXTN_H */
