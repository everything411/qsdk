// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include <linux/if_arp.h>
#include "../ieee80211_i.h"
#include "../driver-ops.h"


void ieee80211_scan_radio_do_open_extn(struct ieee80211_sub_if_data *sdata,
				       struct wireless_dev *wdev,
				       struct net_device *dev,
				       u32 *hw_reconf_flags)
{
	struct ieee80211_local *local = sdata->local;

	if (!wdev || !wdev_is_scan_radio(wdev))
		return;

	sdata->bss = &sdata->u.ap;
	local->monitors++;
	if (local->monitors == 1) {
		local->hw.conf.flags |= IEEE80211_CONF_MONITOR;
		*hw_reconf_flags |= IEEE80211_CONF_CHANGE_MONITOR;
	}

	ieee80211_adjust_monitor_flags(sdata, 1);
	ieee80211_configure_filter(local);
	ieee80211_recalc_offload(local);
	ieee80211_recalc_idle(local);

	if (dev)
		netif_carrier_on(dev);

	INIT_LIST_HEAD(&sdata->u.mntr.list);
	list_add_tail_rcu(&sdata->u.mntr.list, &local->mon_list);
	if (sdata->dev)
		sdata->dev->type = ARPHRD_IEEE80211_RADIOTAP;
}

void ieee80211_scan_radio_do_stop_extn(struct ieee80211_sub_if_data *sdata,
				       u32 *hw_reconf_flags)
{
	struct ieee80211_local *local = sdata->local;
	struct wireless_dev *wdev = &sdata->wdev;

	if (!wdev || !wdev_is_scan_radio(wdev) || local->monitors == 0)
		return;

	local->monitors--;
	if (local->monitors == 0) {
		local->hw.conf.flags &= ~IEEE80211_CONF_MONITOR;
		*hw_reconf_flags |= IEEE80211_CONF_CHANGE_MONITOR;
	}

	ieee80211_configure_filter(local);
	ieee80211_recalc_offload(local);
	ieee80211_recalc_idle(local);
	list_del_rcu(&sdata->u.mntr.list);
}



/*
 * ieee80211_set_allow_scan_on_dfs_chan_extn - Enable or disable scanning on DFS channels
 * @local: mac80211 local state
 * @enable: true to enable the fix, false to disable
 *
 * When enabled, AP interfaces are allowed to leave a DFS channel for an
 * ACS-triggered scan even when radar detection is required (i.e. when DCS
 * WLAN-interference mode is active).  This prevents the channel-switch from
 * being blocked by the pre-CAC check in __ieee80211_can_leave_ch().
 *
 * The off-channel scan and off-channel management-frame checks in nl80211
 * are also relaxed so that the ACS scan request and any associated action
 * frames are not rejected with -EBUSY/-EINVAL.
 */
void ieee80211_set_allow_scan_on_dfs_chan_extn(struct ieee80211_local *local, bool enable)
{
	if (!local)
		return;

	local->hw.wiphy->allow_scan_on_dfs_chan = enable;
	wiphy_dbg(local->hw.wiphy, "ALLOW SCAN ON DFS CHAN %s\n",
		  enable ? "enabled" : "disabled");
}
EXPORT_SYMBOL(ieee80211_set_allow_scan_on_dfs_chan_extn);
