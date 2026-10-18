/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef CFG80211_DFS_EXTN_H
#define CFG80211_DFS_EXTN_H

#include <net/cfg80211.h>

#ifndef CPTCFG_QCN_EXTN

static inline bool cfg80211_support_bootup_cac(struct wiphy *wiphy)
{
	return false;
}

static inline bool
cfg80211_bootup_cac_is_5g_dfs_chan_extn(struct wiphy *wiphy,
					 const struct cfg80211_chan_def *chandef)
{
	return false;
}

#else /* CPTCFG_QCN_EXTN */

/**
 * cfg80211_is_driver_mac80211_hwsim - check if wiphy belongs to mac80211_hwsim
 * @wiphy: the wiphy to check
 *
 * Returns true when the wiphy parent device is driven by the mac80211_hwsim
 * virtual driver, false otherwise.
 */
bool cfg80211_is_driver_mac80211_hwsim(struct wiphy *wiphy);

/**
 * cfg80211_support_bootup_cac - check if boot-up CAC is supported
 *
 * Returns true when the driver supports creating all 5G BSS interfaces
 * during DFS CAC (boot-up CAC feature).  When true, callers may skip
 * cfg80211_reg_can_beacon_relax() rejection for 5G DFS channels so that
 * all BSSes can be started before CAC completes.
 */
bool cfg80211_support_bootup_cac(struct wiphy *wiphy);

/**
 * cfg80211_bootup_cac_is_5g_dfs_chan_extn - check if boot-up CAC applies to
 * the given wiphy and channel
 * @wiphy: the wiphy to check boot-up CAC support on
 * @chandef: the channel definition to evaluate (all subchannels are checked)
 *
 * Returns true when boot-up CAC is supported and the chandef covers a 5 GHz
 * channel definition that requires DFS (radar detection) on any subchannel.
 */
bool cfg80211_bootup_cac_is_5g_dfs_chan_extn(struct wiphy *wiphy,
					      const struct cfg80211_chan_def *chandef);

#endif /* CPTCFG_QCN_EXTN */

#endif /* CFG80211_DFS_EXTN_H */
