/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DFS_EXTN_H
#define DFS_EXTN_H

#include "cfg80211_dfs_extn.h"

struct ieee80211_link_data;
struct ieee80211_sub_if_data;
struct cfg80211_ap_settings;
struct ieee80211_chanctx;

#ifndef CPTCFG_QCN_EXTN

static inline void
ieee80211_bootup_cac_start_timer_extn(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_link_data *link,
				      const struct cfg80211_chan_def *chandef,
				      u32 residual_cac_ms)
{
}

static inline void
ieee80211_bootup_cac_mesh_vdev_up_extn(struct ieee80211_chanctx *ctx)
{
}

static inline void
ieee80211_bootup_cac_handle_skip_extn(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_link_data *link,
				      const struct cfg80211_chan_def *chandef,
				      u32 residual_cac_ms, bool skip_cac)
{
}

#else /* CPTCFG_QCN_EXTN */

/**
 * ieee80211_bootup_cac_start_timer_extn - start boot-up CAC timer for a
 * 5G DFS channel, called from both AP (start_ap) and mesh (join_mesh) paths.
 * @sdata: the sub-interface data
 * @link: the link data for the link being started
 * @chandef: the channel definition for the link
 * @residual_cac_ms: residual CAC time in milliseconds, or 0 for full CAC
 *
 * If this is the first BSS being started on a 5G DFS channel, starts the
 * per-chanctx dfs_cac_timer with the residual CAC time when valid, otherwise
 * with the regulatory CAC time. Subsequent calls on the same chanctx inherit
 * the timing state.
 */
void ieee80211_bootup_cac_start_timer_extn(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_link_data *link,
					   const struct cfg80211_chan_def *chandef,
					   u32 residual_cac_ms);

/**
 * ieee80211_bootup_cac_mesh_vdev_up_extn - trigger vdev_up for mesh vdevs
 * sharing a chanctx after boot-up CAC completes.
 * @ctx: the chanctx whose CAC timer just fired
 *
 * The CAC timer fires once per chanctx and cfg80211_cac_event() is sent only
 * for the first link found by the caller, scoped to that link's netdev
 * ifindex. cfg80211_clear_cac_started() leaves cac_started set for mesh
 * wdevs, so for every mesh vdev on the same chanctx that has already joined
 * (enable_beacon=true) and still has cac_started set, call
 * cfg80211_cac_event() to send it its own scoped CAC-finished notification
 * and clear cac_started.
 */
void ieee80211_bootup_cac_mesh_vdev_up_extn(struct ieee80211_chanctx *ctx);

/**
 * ieee80211_bootup_cac_mesh_carrier_on - bring up mesh carrier after boot-up CAC
 * @sdata: the mesh sub-interface
 *
 * Called from ieee80211_bootup_cac_mesh_vdev_up_extn() after vdev_up has been
 * sent to firmware (arvif->is_up is now true).  Calls netif_carrier_on() so
 * the bridge layer starts udhcpc and DHCP DISCOVERs are sent only when the
 * firmware can actually TX them.
 *
 * This is the counterpart to the deferred netif_carrier_on in
 * ieee80211_start_mesh(): during boot-up CAC that call is skipped to avoid
 * DHCP DISCOVERs being dropped by ath12k_wifi7_mcbc_handler (which gates
 * broadcast TX on arvif->is_up).
 */
void ieee80211_bootup_cac_mesh_carrier_on(struct ieee80211_sub_if_data *sdata);

/**
 * ieee80211_bootup_cac_handle_skip_extn - handle skip_cac flag during boot-up CAC.
 * @sdata: the sub-interface data
 * @link: the link data for the link being started
 * @chandef: the channel definition for the link
 * @skip_cac: whether the user space requested CAC to be skipped
 *
 * When @skip_cac is true, sends a dummy NL80211_RADAR_CAC_FINISHED event so
 * that hostapd resets its boot-up CAC state and continues as if CAC completed
 * immediately.  When @skip_cac is false, falls through to the normal
 * ieee80211_bootup_cac_start_timer_extn() path.
 */
void ieee80211_bootup_cac_handle_skip_extn(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_link_data *link,
					   const struct cfg80211_chan_def *chandef,
					   u32 residual_cac_ms, bool skip_cac);

#endif /* CPTCFG_QCN_EXTN */

#endif /* DFS_EXTN_H */
