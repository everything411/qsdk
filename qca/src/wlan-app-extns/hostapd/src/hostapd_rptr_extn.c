/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "includes.h"
#include "utils/common.h"
#include "utils/os.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ap_drv_ops.h"
#include "ap/dfs.h"
#include "ap/hw_features.h"
#include "ap/beacon.h"
#include "drivers/driver.h"
#include "common/ieee802_11_defs.h"
#include "common/defs.h"
#include "common/wpa_ctrl.h"
#include "hostapd_rptr_extn.h"

/**
 * hostapd_csa_bitmap_update_extn - Track CSA completion and notify supplicant
 * @iface: Hostapd interface whose links are undergoing CSA/CAC
 * @freq: Operating frequency used when all links complete the CSA/CAC
 *
 * Update the per-link CSA bitmap in the extended configuration and, when
 * all bits are cleared, emit a completion event towards wpa_supplicant.
 */
void hostapd_csa_bitmap_update_extn(struct hostapd_iface *iface, int freq)
{
	int j;
	if (!iface || !iface->conf)
		return;

	if (iface->iface_extn.csa_bitmap) {
		for (j = 0; j < MAX_NUM_MLD_LINKS; j++) {
			if (iface->iface_extn.csa_bitmap & (1 << j)) {
				iface->iface_extn.csa_bitmap &= ~(1 << j);
				break;
			}
		}
		if (!iface->iface_extn.csa_bitmap) {
			iface->mcst = 0;
			wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO,
				"Revd CSA/CAC completion, notify wpa_supplicant");
#ifdef CONFIG_HOSTAPD_SRC_DIR
			hostapd_ucode_chsw_result_ev_notify(iface->bss[0], freq, 0);
#endif
		}
	}
}

/**
 * hostapd_ml_acs_check_and_notify - Track ACS completion and notify supplicant
 * @iface: Hostapd interface whose links are undergoing CSA/CAC
 * @status: ACS status
 *
 * Check if all ML partner links have completed ACS, and if so sends
 * notification to wpa_supplicant to start Repeater STA scan.
 */
void hostapd_ml_acs_check_and_notify(struct hostapd_iface *iface, bool status)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i, num_ap_with_acs_done = 0;

	wpa_printf(MSG_DEBUG, "ACS: Checking ACS status across all ML partner links");

	if (status)
		iface->iface_extn.acs_success = true;
	else
		iface->iface_extn.acs_failed = true;

	num_ap_with_acs_done++;

	/* Check all other ML partner interfaces */
	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		struct hostapd_iface *h =
			hapd->iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h->bss[0];

		if (h == hapd->iface) {
			wpa_printf(MSG_DEBUG, "ACS: Continue as same iface");
			continue;
		}

		if (hostapd_is_ml_partner(hapd, h_hapd)) {
			/* Count interfaces that have status (success or failure) */
			if (h_hapd->iface->iface_extn.acs_success ||
			    h_hapd->iface->iface_extn.acs_failed) {
				num_ap_with_acs_done++;
				wpa_printf(MSG_DEBUG, "ACS: ML partner %s has ACS done (status=%d)",
					   h_hapd->conf->iface,
				           h_hapd->iface->iface_extn.acs_success
					   ? h_hapd->iface->iface_extn.acs_success
					   : h_hapd->iface->iface_extn.acs_failed);
			}
		}
	}

	wpa_printf(MSG_DEBUG, "ACS: Total ML partners with ACS done: %d, Total interfaces: %zu",
			num_ap_with_acs_done, hapd->iface->interfaces->count);

	/* If all ML partner links have completed ACS, send notification */
	if (num_ap_with_acs_done == hapd->iface->interfaces->count) {
		wpa_printf(MSG_DEBUG, "ACS: All ML partner links have completed ACS, sending notification");
#ifdef CONFIG_HOSTAPD_SRC_DIR
		if (!hostapd_is_bh_sta_connecting_or_connected_extn(iface))
			hostapd_ucode_notify_acs_completed(iface, 1);
#endif
	}
}

/**
 * uc_hostapd_compare_channel_params_extn - Compare desired vs current channel
 * @conf: Pointer to hostapd configuration
 * @freq_params: Desired frequency and channel width parameters
 * @freq: Primary operating frequency
 *
 * Compare current channel configuration (frequency, bandwidth, center
 * frequencies and puncturing bitmap) against the desired parameters.
 * When they match, the caller can skip channel switch announcement (CSA)
 * and directly notify completion.
 *
 * Return: true if the configurations are equivalent, false otherwise.
 */
bool
uc_hostapd_compare_channel_params_extn(struct hostapd_config *conf,
				       struct hostapd_freq_params freq_params,
				       int freq)
{
	enum oper_chan_width oper_chwidth;
	int bandwidth;
	int centr_freq_seg0_idx;
	int centr_freq_seg1_idx;
	int center_freq1;
	int center_freq2;
	u8 op_class, channel;

	oper_chwidth = hostapd_get_oper_chwidth(conf);
	if (oper_chwidth == CONF_OPER_CHWIDTH_80MHZ)
		bandwidth = 80;
	else if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ ||
		oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ)
		bandwidth = 160;
	else if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
		bandwidth = 320;
	else if (conf->secondary_channel)
		bandwidth = 40;
	else
		bandwidth = 20;

	centr_freq_seg0_idx = hostapd_get_oper_centr_freq_seg0_idx(conf);
	centr_freq_seg1_idx = hostapd_get_oper_centr_freq_seg1_idx(conf);
	ieee80211_freq_to_channel_ext(freq, 0, 1, &op_class, &channel);

	/* Handle 2.4GHz band differently */
	if (freq >= 2412 && freq <= 2472) {
		if (bandwidth == 40) {
			int offset_mhz = (channel <= 7) ? 10 : -10;
			center_freq1 = freq + offset_mhz;
		} else {
			center_freq1 = freq;
		}
		center_freq2 = 0;
	} else {
		center_freq1 = ieee80211_chan_to_freq(NULL, op_class, centr_freq_seg0_idx);
		center_freq2 = ieee80211_chan_to_freq(NULL, op_class, centr_freq_seg1_idx);
		center_freq1 = (center_freq1 == -1) ? 0 : center_freq1;
		center_freq2 = (center_freq2 == -1) ? 0 : center_freq2;
	}

	if ((freq_params.freq == freq) &&
	    (freq_params.bandwidth == bandwidth) &&
	    (freq_params.center_freq1 == center_freq1) &&
	    (freq_params.center_freq2 == center_freq2) &&
	    (freq_params.punct_bitmap == conf->punct_bitmap)) {
		return true;
	} else {
		return false;
	}
}

enum chan_width
hostapd_oper_chwidth_to_chanwidth_extn(int oper_chwidth,
				       int sec_channel_offset)
{
	if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
		return CHAN_WIDTH_320;
	if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ)
		return CHAN_WIDTH_160;
	if (oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ)
		return CHAN_WIDTH_80P80;
	if (oper_chwidth == CONF_OPER_CHWIDTH_80MHZ)
		return CHAN_WIDTH_80;
	if (sec_channel_offset)
		return CHAN_WIDTH_40;

	return CHAN_WIDTH_20;
}

u8 uc_hostapd_bandwidth_to_oper_chwidth_extn(int bandwidth)
{
	if (bandwidth == 320)
		return CONF_OPER_CHWIDTH_320MHZ;
	if (bandwidth == 160)
		return CONF_OPER_CHWIDTH_160MHZ;
	if (bandwidth == 80)
		return CONF_OPER_CHWIDTH_80MHZ;

	return CONF_OPER_CHWIDTH_USE_HT;
}

static int uc_hostapd_handle_csa_during_cac_extn(struct hostapd_iface *iface,
						 struct csa_settings *csa,
						 bool pre_connect)
{
	int i, ret;
	u8 oper_chwidth;
	u8 op_class;
	u8 seg0 = 0;
	u8 seg1 = 0;
	u8 channel = 0;
	u8 chan;

	if (!iface->cac_started && !iface->bootup_cac_in_progress)
		return 1;

	if (!csa->cs_count)
		csa->cs_count = 5;
	csa->block_tx = 1;

	ieee80211_freq_to_chan(csa->freq_params.freq, &channel);
	oper_chwidth = uc_hostapd_bandwidth_to_oper_chwidth_extn(
		csa->freq_params.bandwidth);

	if (csa->freq_params.center_freq1)
		ieee80211_freq_to_chan(csa->freq_params.center_freq1, &seg0);
	if (csa->freq_params.center_freq2)
		ieee80211_freq_to_chan(csa->freq_params.center_freq2, &seg1);

	wpa_printf(MSG_INFO,
		   "CAC active on iface %s - abort CAC and queue deferred CSA "
		   "for freq=%d chan=%d bw=%d oper_chwidth=%d sec_chan=%d "
		   "seg0=%u seg1=%u cf1=%d cf2=%d cac_started=%d bootup_cac=%d",
		   iface->bss[0]->conf->iface, csa->freq_params.freq,
		   channel, csa->freq_params.bandwidth, oper_chwidth,
		   csa->freq_params.sec_channel_offset, seg0, seg1,
		   csa->freq_params.center_freq1, csa->freq_params.center_freq2,
		   iface->cac_started, iface->bootup_cac_in_progress);

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NEW_CHANNEL
		"freq=%d chan=%d sec_chan=%d", csa->freq_params.freq,
		channel, csa->freq_params.sec_channel_offset);

	if (ieee80211_freq_to_channel_ext(csa->freq_params.freq,
					  csa->freq_params.sec_channel_offset,
					  oper_chwidth, &op_class, &chan) !=
					  NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG, "Update op_class %d->%d",
			   iface->conf->op_class, op_class);
		iface->conf->op_class = op_class;
	}

	ret = hostapd_abort_cac_for_channel_switch(iface, csa);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CAC abort channel switch request failed ret=%d", ret);
		return ret;
	}

	if (pre_connect)
		for (i = 0; i < iface->num_bss; i++)
			iface->iface_extn.csa_bitmap |= BIT(i);

	return 0;
}

static void hostapd_get_channel_switch_time_extn(struct hostapd_iface *iface,
						 struct hostapd_freq_params *freq_params)
{
	struct hostapd_data *hapd = NULL;
	unsigned int i;
	int ret;

	if (!iface->bss || !iface->num_bss)
		return;

	iface->cs_time = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (!iface->bss[i]->driver || !iface->bss[i]->drv_priv)
			continue;

		hapd = iface->bss[i];
		break;
	}

	if (!hapd || !hapd->driver->get_channel_switch_time)
		return;

	ret = hapd->driver->get_channel_switch_time(hapd->drv_priv,
						      freq_params,
						      &iface->cs_time);
	if (!ret) {
		wpa_printf(MSG_DEBUG,
			   "channel switch time from driver: %u ms",
			   iface->cs_time);
		return;
	}

	wpa_printf(MSG_WARNING,
		   "Failed to get channel switch time from driver: %d", ret);
	iface->cs_time = 0;
}

/**
 * uc_hostapd_iface_switch_channel_extn - Perform CSA with repeater extensions
 * @iface: Hostapd interface on which to switch channel
 * @is_dfs: Whether the target channel is DFS and may require CAC
 * @wpa_state: Optional pointer to current wpa_supplicant state string
 * @csa: Pointer to CSA settings describing requested channel parameters
 *
 * Decide whether a channel switch announcement (CSA) is needed based on
 * current vs requested channel settings. When needed, perform CSA on all
 * BSSes of the interface and track completion in PRE_CONNECT using a bitmap.
 * If no change is required, optionally notify wpa_supplicant of completion.
 *
 * Return: 0 on success, or negative error code on failure from
 *         hostapd_switch_channel().
 */
int uc_hostapd_iface_switch_channel_extn(struct hostapd_iface *iface,
                                        bool is_dfs, char *wpa_state,
                                        struct csa_settings *csa)
{
	struct hostapd_config *conf = iface->conf;
	int i, ret = 0;
	bool pre_connect = false;

	if (wpa_state)
		wpa_printf(MSG_INFO, "wpa_state: %s", wpa_state);

	pre_connect = (wpa_state && os_strcmp(wpa_state, "PRE_CONNECT") == 0);

#ifdef CONFIG_HOSTAPD_SRC_DIR
	/* Apply skip_cac only in PRE_CONNECT when conf->skip_cac is enabled */
	if (conf->conf_extn.skip_cac && pre_connect)
		csa->freq_params.skip_cac = is_dfs && !csa->mcst;
#endif

	/*
	 * Based on the MCST received from kernel channel switch notification,
	 * derive cs_time (MCST to advertised in Repeater FH beacon).
	 *
	 * For a DFS target that can skip CAC, either because skip_cac was already
	 * enabled or because the received MCST fits within the non-CAC switch-time
	 * threshold, advertise the larger of the minimum non-CAC switch time and the
	 * received MCST. For a non-DFS target, or a DFS target that still requires
	 * CAC, use the driver-provided channel switch time.
	 */
	iface->mcst = csa->mcst;

	if (is_dfs && csa->mcst)
		csa->freq_params.skip_cac =
			hostapd_mcst_allows_skip_cac_extn(csa->mcst,
							  conf->beacon_int,
							  is_dfs);

	if (csa->freq_params.skip_cac) {
		iface->cs_time =
			MAX(HOSTAPD_NON_CAC_SWITCH_TIME_MSEC_EXTN(conf->beacon_int),
			    IEEE80211_TU_TO_MS(csa->mcst));
	} else if (is_dfs && csa->mcst) {
		iface->cs_time = IEEE80211_TU_TO_MS(csa->mcst);
		if (pre_connect) {
			iface->cs_time += 2 * conf->beacon_int;
			wpa_printf(MSG_INFO,
				   "CSA: residual CAC using phy=%s mcst=%u TU cs_time=%u ms num_bss=%zu",
				   iface->phy, csa->mcst, iface->cs_time, iface->num_bss);
		}
	} else
		hostapd_get_channel_switch_time_extn(iface, &csa->freq_params);

	csa->freq_params.mcst = iface->cs_time ?
		IEEE80211_MS_TO_TU(iface->cs_time) : 0;

	/* CSwOpts 0x4: AP must perform CAC on STA-triggered bring-up.
	 * Override any skip_cac set above (conf skip_cac or MCST-based);
	 * cswopts is runtime-only and not overwritten by config reloads. */
	if (IS_CSH_CAC_APUP_BYSTA_ENABLED(conf->conf_extn.cswopts))
		csa->freq_params.skip_cac = 0;

	if (csa->freq_params.skip_cac)
		iface->mcst = csa->freq_params.mcst;

	wpa_printf(MSG_INFO,
		   "CSA: phy=%s is_dfs=%d pre_connect=%d skip_cac=%d mcst=%u TU "
		   "cs_time=%u ms freq=%d channel=%d cf1=%d cf2=%d link_id=%d",
		   iface->phy, is_dfs, pre_connect, csa->freq_params.skip_cac,
		   csa->freq_params.mcst, iface->cs_time, csa->freq_params.freq,
		   csa->freq_params.channel, csa->freq_params.center_freq1,
		   csa->freq_params.center_freq2, csa->link_id);

	if (is_dfs && IS_CSH_IGNORE_CSA_DFS_ENABLED(conf->conf_extn.cswopts) &&
	    !pre_connect) {
		wpa_printf(MSG_INFO,
			   "CSwOpts %s: uplink CSA target freq=%d is DFS;"
			   " selecting local non-DFS channel for repeater AP vap",
			   convert_cswopts_to_str(CSH_OPT_IGNORE_CSA_DFS),
			   csa->freq_params.freq);
#ifdef CONFIG_HOSTAPD_SRC_DIR
		for (i = 0; i < iface->num_bss; i++) {
			ret = hostapd_dfs_start_channel_switch(iface);
			if (ret) {
				wpa_printf(MSG_ERROR, "Repeater AP Channel switch"
					   " to Non-DFS channel failed ret = %d", ret);
				hostapd_ucode_chsw_result_ev_notify(
							iface->bss[i],
							csa->freq_params.freq,
							ret);
				return ret;
			}
			iface->iface_extn.csa_bitmap |= BIT(i);
		}
#endif
		return ret;
	}

	wpa_printf(MSG_INFO,
		   "CSA: cac_started=%d bootup_cac=%d is_dfs=%d skip_cac=%d",
		   iface->cac_started, iface->bootup_cac_in_progress, is_dfs,
		   csa->freq_params.skip_cac);

	/* If channel params differ, perform CSA and track per-BSS completion */
	if (!uc_hostapd_compare_channel_params_extn(conf, csa->freq_params, iface->freq) ||
	     iface->conf->conf_extn.rpt_max_phy) {
		if (iface->cac_started || iface->bootup_cac_in_progress) {
			ret = uc_hostapd_handle_csa_during_cac_extn(iface, csa,
								    pre_connect);
			if (ret <= 0)
				return ret;
		}

		for (i = 0; i < iface->num_bss; i++) {
			wpa_printf(MSG_DEBUG,
				   "CSA: switch_channel bss[%d]=%s mld_link_id=%d beacon_set_done=%d",
				   i, iface->bss[i]->conf->iface,
				   iface->bss[i]->mld_link_id,
				   iface->bss[i]->beacon_set_done);

			/*
			 * NL80211_CMD_CHANNEL_SWITCH returns -ENOTCONN on a
			 * non-beaconing BSS (see the identical WAR in
			 * hostapd_deferred_csa_dispatch() in dfs.c). A
			 * BSS sharing this radio that is not currently
			 * beaconing is unrelated to the CSA target and must
			 * not abort CSA for every other BSS on the radio -
			 * one of which may be the fronthaul BSS the
			 * reconnecting bSTA actually depends on.
			 */
			if (!iface->bss[i]->beacon_set_done) {
				wpa_printf(MSG_DEBUG,
					   "CSA skip bss[%d]=%s mld_link_id=%d: not beaconing",
					   i, iface->bss[i]->conf->iface,
					   iface->bss[i]->mld_link_id);
				continue;
			}

			ret = hostapd_switch_channel(iface->bss[i], csa);
			if (ret) {
				/*
				 * hostapd_switch_channel() returns -1 when
				 * csa_in_progress is already set.  This can
				 * happen on an MLD STA where both
				 * EVENT_LINK_CH_SWITCH_STARTED and
				 * EVENT_CH_SWITCH_STARTED fire for the same
				 * physical channel switch, causing this
				 * function to be called twice.  If the BSS
				 * already has a CSA in progress toward the
				 * same target frequency, treat it as a no-op
				 * rather than a fatal error so the ongoing
				 * channel switch can complete normally.
				 */
				if (iface->bss[i] && iface->bss[i]->csa_in_progress &&
				    iface->bss[i]->cs_freq_params.freq ==
				    csa->freq_params.freq) {
					wpa_printf(MSG_DEBUG,
						   "CSA already in progress to freq=%d on bss[%d], skipping duplicate",
						   csa->freq_params.freq, i);
					continue;
				}
				wpa_printf(MSG_ERROR,
					   "Channel switch failed on bss[%d]=%s mld_link_id=%d"
					   " ret = %d", i, iface->bss[i]->conf->iface,
					   iface->bss[i]->mld_link_id, ret);
#ifdef CONFIG_HOSTAPD_SRC_DIR
				if (pre_connect) {
					hostapd_ucode_chsw_result_ev_notify(
							iface->bss[0],
							csa->freq_params.freq,
							ret);
				}
#endif
				return ret;
			}
			/* Track CSA per link using bitmap in PRE_CONNECT */
			if (pre_connect)
				iface->iface_extn.csa_bitmap |= BIT(i);
		}
	} else if (is_dfs &&
		   (iface->cac_started || iface->bootup_cac_in_progress) &&
		   csa->freq_params.skip_cac) {
		wpa_printf(MSG_INFO,
			   "AP is already on requested DFS channel, abort CAC and honor skip_cac CSA");
		ret = uc_hostapd_handle_csa_during_cac_extn(iface, csa,
							    pre_connect);
		if (ret <= 0)
			return ret;
	} else {
		/* No CSA needed; notify supplicant only in PRE_CONNECT */
		wpa_printf(MSG_INFO, "AP is already UP in same channel");
		if (pre_connect) {
#ifdef CONFIG_HOSTAPD_SRC_DIR
			hostapd_ucode_chsw_result_ev_notify(iface->bss[0],
							    csa->freq_params.freq,
							    0);
#endif
		}
	}

	/* Return 0 on success path, negative on failure consistent with caller */
	return ret;
}

bool hostapd_radio_has_ap_bss_extn(struct hostapd_iface *iface)
{
	struct hapd_interfaces *interfaces;
	struct hostapd_iface *other;
	struct hostapd_data *bss;
	size_t i, j;

	if (!iface) {
		wpa_printf(MSG_DEBUG, "%s: iface is NULL", __func__);
		return false;
	}

	interfaces = iface->interfaces;
	if (!interfaces) {
		wpa_printf(MSG_DEBUG, "%s: interfaces is NULL", __func__);
		return false;
	}

	for (i = 0; i < interfaces->count; i++) {
		other = interfaces->iface[i];
		if (!other || other->phy[0] == '\0')
			continue;

		if (os_strcmp(other->phy, iface->phy) != 0)
			continue;

		for (j = 0; j < other->num_bss; j++) {
			bss = other->bss[j];
			if (!bss || !bss->conf)
				continue;

			if (!bss->conf->mld_ap &&
			    bss->conf->ssid.ssid_len &&
			    !bss->conf->start_disabled) {
				return true;
			}
		}
	}

	return false;
}

void hostapd_iface_set_supplicant_channel_extn(struct hostapd_iface *hapd_iface)
{
	struct hostapd_freq_params sta_freq;
	int ret = 0;
	int band = -1;
	int freq;
	int cfg_chan;
	u8 chan = 0, seg0_chan = 0, seg1_chan = 0;

	if (!hapd_iface) {
		wpa_printf(MSG_DEBUG, "%s: iface is NULL", __func__);
		return;
	}
	freq = hapd_iface->freq;

	/*
	 * Only try STA channel when there is no existing AP BSS on
	 * this radio. If any AP VAP already exists (on this or another
	 * MLD) for the same phy, we keep using the configured channel
	 * to avoid dual-channel configurations.
	 */
	if (hostapd_radio_has_ap_bss_extn(hapd_iface)) {
		wpa_printf(MSG_DEBUG, "%s: Skip STA channel follow; AP BSS exists on phy %s",
			   __func__, hapd_iface->phy);
		return;
	}

	/* Derive freq from config if not set yet */
	if (!freq && hapd_iface->conf && hapd_iface->conf->channel) {
#ifdef CONFIG_HOSTAPD_SRC_DIR
		if (configured_fixed_chan_to_freq_helper(hapd_iface) < 0)
			freq = 0;
		else
			freq = hapd_iface->freq;
#else
	freq = hapd_iface->freq;
#endif
    }

	wpa_printf(MSG_DEBUG,
		   "%s: pre-band freq=%d (iface->freq=%d, chan=%d, op_class=%d)",
		   __func__, freq, hapd_iface->freq,
		   hapd_iface->conf ? hapd_iface->conf->channel : -1,
		   hapd_iface->conf ? hapd_iface->conf->op_class : -1);

	if (freq && is_24ghz_freq(freq)) {
		band = 0; /* 2G */
	} else if (freq && is_5ghz_freq(freq)) {
		band = 1; /* 5G */
	} else if (freq && is_6ghz_freq(freq)) {
		band = 2; /* 6G */
	} else if (hapd_iface->conf && hapd_iface->conf->channel) {
		cfg_chan = hapd_iface->conf->channel;

		/*
		 * Fallback: infer the operating band from the channel number when the
		 * frequency is invalid. This situation has been observed only on
		 * 2.4/5 GHz and so the respective handling.
		 */
		if (cfg_chan >= 1 && cfg_chan <= 14) {
			band = 0; /* 2G */
		}
		else if ((cfg_chan >= 36 && cfg_chan <= 64) ||
			 (cfg_chan >= 100 && cfg_chan <= 144) ||
			 (cfg_chan >= 149 && cfg_chan <= 177)) {
			band = 1; /* 5G */
                }
		else {
			wpa_printf(MSG_DEBUG,
				   "%s: No freq; channel=%d not in 5G ranges; "
				   "skipping fallback", __func__, cfg_chan);
			return;
		}
	} else {
		wpa_printf(MSG_DEBUG, "%s: Unable to infer band (no freq/channel)",
			   __func__);
		return;
	}

#ifdef CONFIG_HOSTAPD_SRC_DIR
	ret = hostapd_ucode_get_sta_channel_per_band(hapd_iface,
						     band,
						     &sta_freq);
#endif

	wpa_printf(MSG_DEBUG,
		   "%s: get_sta_channel ret=%d (freq=%d bw=%d cf1=%d cf2=%d sec=%d punct=0x%04x)",
		   __func__, ret, sta_freq.freq,
		   sta_freq.bandwidth, sta_freq.center_freq1,
		   sta_freq.center_freq2, sta_freq.sec_channel_offset,
		   sta_freq.punct_bitmap);

	if (ret) {
		wpa_printf(MSG_DEBUG, "%s: STA channel fetch/validate failed: %d",
			   __func__, ret);
		return;
	}

	/* Mark DFS availability via STA if this is a DFS channel/sub-channel */
	hapd_iface->iface_extn.dfs_available_from_sta =
		(ieee80211_is_dfs(sta_freq.freq, hapd_iface->hw_features,
				  hapd_iface->num_hw_features) ||
		 ieee80211_is_dfs(sta_freq.center_freq1, hapd_iface->hw_features,
			 	  hapd_iface->num_hw_features) ||
		 ieee80211_is_dfs(sta_freq.center_freq2, hapd_iface->hw_features,
			 	  hapd_iface->num_hw_features));

	wpa_printf(MSG_INFO,
		   "Using STA connected channel: freq=%d bw=%d cf1=%d cf2=%d sec=%d punct=0x%04x",
		   sta_freq.freq, sta_freq.bandwidth,
		   sta_freq.center_freq1, sta_freq.center_freq2,
		   sta_freq.sec_channel_offset,
		   sta_freq.punct_bitmap);

	if (sta_freq.freq) {
		chan = 0;

		hapd_iface->freq = sta_freq.freq;
		ieee80211_freq_to_chan(sta_freq.freq, &chan);
		if (chan > 0) {
			hapd_iface->conf->channel = chan;
			wpa_printf(MSG_INFO,
				   "STA primary freq=%d -> channel=%u",
				   sta_freq.freq, chan);
		}
	}

	if (sta_freq.sec_channel_offset)
		hapd_iface->conf->secondary_channel =
			sta_freq.sec_channel_offset;

	if (sta_freq.center_freq1) {
		seg0_chan = 0;

		ieee80211_freq_to_chan(sta_freq.center_freq1,
				       &seg0_chan);
		if (seg0_chan > 0) {
			wpa_printf(MSG_INFO,
				   "STA seg0 center_freq1=%d seg0_chan=%u",
				   sta_freq.center_freq1, seg0_chan);
			hostapd_set_oper_centr_freq_seg0_idx(hapd_iface->conf,
							     seg0_chan);
		}
	}

	if (sta_freq.center_freq2) {
		seg1_chan = 0;

		ieee80211_freq_to_chan(sta_freq.center_freq2,
				       &seg1_chan);
		if (seg1_chan > 0) {
			wpa_printf(MSG_INFO,
				   "STA seg1 center_freq2=%d seg1_chan=%u",
				   sta_freq.center_freq2, seg1_chan);
			hostapd_set_oper_centr_freq_seg1_idx(hapd_iface->conf,
							     seg1_chan);
		}
	}

	if (sta_freq.bandwidth) {
		if (sta_freq.bandwidth == 320)
			hostapd_set_oper_chwidth(hapd_iface->conf,
						 CONF_OPER_CHWIDTH_320MHZ);
		else if (sta_freq.bandwidth == 160)
			hostapd_set_oper_chwidth(hapd_iface->conf,
						 CONF_OPER_CHWIDTH_160MHZ);
		else if (sta_freq.bandwidth == 80)
			hostapd_set_oper_chwidth(hapd_iface->conf,
						 CONF_OPER_CHWIDTH_80MHZ);
		else
			hostapd_set_oper_chwidth(hapd_iface->conf,
						 CONF_OPER_CHWIDTH_USE_HT);
	}

	if (sta_freq.punct_bitmap)
		hapd_iface->conf->punct_bitmap =
			sta_freq.punct_bitmap;

	return;
}

/**
 * hostapd_is_bh_sta_connecting_or_connected_extn - Check and update whether STA
 * is connecting or connected
 *
 * @iface: Hostapd interface
 *
 * True: if STA is in connecting or connected state
 * False: if STA is not in connecting or connected state
 */
bool hostapd_is_bh_sta_connecting_or_connected_extn(struct hostapd_iface *iface)
{
	if (!iface)
		return false;

#ifdef UCODE_SUPPORT
	wpa_printf(MSG_INFO, "sta_wpa_state = %s vap_type = %s",
			iface->iface_extn.sta_wpa_state,
			vap_type_to_string(iface->iface_extn.vap_type));

	if (iface->iface_extn.vap_type == VAP_TYPE_MESH)
		return false;
#endif

	if (!os_strncmp(iface->iface_extn.sta_wpa_state, "COMPLETED", 9) ||
	    !os_strncmp(iface->iface_extn.sta_wpa_state, "AUTHENTICATING", 14))
		return true;

	return false;
}

bool hostapd_is_bh_sta_link_connected_extn(struct hostapd_iface *iface)
{
	if (!iface)
		return false;
	wpa_printf(MSG_DEBUG, "sta_wpa_state = %s",
		   iface->iface_extn.sta_wpa_state);

	return !os_strncmp(iface->iface_extn.sta_wpa_state, "COMPLETED", 9);
}

void hostapd_update_bh_sta_connected_extn(struct hostapd_iface *iface,
					  const char *wpa_state)
{
	if (!iface || !iface->interfaces || !wpa_state)
		return;

	iface->interfaces->interfaces_extn.bh_sta_connected =
		os_strncmp(wpa_state, "COMPLETED", 9) == 0;
}

/**
 * create_hop_count_vendor_ie - Create a vendor IE with hop count
 * @hop_count: The hop count value to include (0-255)
 * Returns: wpabuf containing the vendor IE, or NULL on failure
 */
struct wpabuf * hostapd_create_hop_count_vendor_ie_extn(u8 hop_count)
{
	struct wpabuf *buf;
	u8 *len_pos;

	/* Allocate buffer: EID(1) + Length(1) + OUI(3) + Type(1) + HopCount(1) = 7 bytes */
	buf = wpabuf_alloc(7);
	if (!buf) {
		wpa_printf(MSG_ERROR, "Failed to allocate buffer for hop count vendor IE");
		return NULL;
	}

	/* Element ID: Vendor Specific */
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);

	/* Length field - will be filled at the end */
	len_pos = wpabuf_put(buf, 1);

	/* OUI: 3 bytes */
	wpabuf_put_be24(buf, OUI_QCA);

	/* OUI Type: 1 byte */
	wpabuf_put_u8(buf, MULTI_AP_OUI_TYPE);

	/* Hop Count: 1 byte */
	wpabuf_put_u8(buf, hop_count);

	/* Set the length field (OUI + Type + HopCount = 5 bytes) */
	*len_pos = 5;

	wpa_hexdump_buf(MSG_DEBUG, "same_ssid: Created hop count vendor IE", buf);

	return buf;
}

/**
 * update_assoc_resp_with_hop_count -  Update association response elements with hop count
 * @hapd: hostapd_data
 * @hop_count: The hop count value to include (0-255)
 */
int hostapd_update_assoc_resp_with_hop_count_extn(struct hostapd_data *hapd)
{
	struct wpabuf *hop_ie;

	if (!hapd->iface->conf->conf_extn.same_ssid)
		return -1;

	if (!os_strncmp(hapd->iface->iface_extn.sta_wpa_state, "COMPLETED", 9))
		hop_ie = hostapd_create_hop_count_vendor_ie_extn(QCN_HOP_COUNT_CONNECTED);
	else
		hop_ie = hostapd_create_hop_count_vendor_ie_extn(QCN_HOP_COUNT_UNKNOWN);

	if (!hop_ie)
		return -1;

	/* Free old assocresp_elements before replacing to avoid memory leak */
	wpabuf_free(hapd->conf->assocresp_elements);

	/* Update the association response elements (hop_ie ownership transferred) */
	hapd->conf->assocresp_elements = hop_ie;

	wpa_hexdump_buf(MSG_DEBUG, "same_ssid: assoc_resp_elem",
	hapd->conf->assocresp_elements);

	return 0;
}

/**
 * hostapd_beacon_set_skip_cac_extn - Set skip_cac in freq params for beacon setup
 * @iface: Pointer to hostapd interface
 * @freq_params: Pointer to frequency parameters to update
 *
 * Determines whether CAC can be skipped for a dependent repeater operating on
 * a 5 GHz channel.
 */
void hostapd_beacon_set_skip_cac_extn(struct hostapd_iface *iface,
				      struct hostapd_freq_params *freq_params)
{
	struct hostapd_config *iconf;
	int chan_width;
	int start_chan_idx, start_chan_idx1;
	int n_chans, n_chans1;
	bool is_dfs;
	int res;

	if (!iface || !iface->conf || !freq_params)
		return;

	if (!is_5ghz_freq(iface->freq))
		return;

	iconf = iface->conf;
	if (!iconf->conf_extn.repeater || iconf->conf_extn.ind_rptr)
		return;

	chan_width = hostapd_get_oper_chwidth(iconf);
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
					       chan_width,
					       iconf->channel, false);
	if (start_chan_idx == -1)
		return;

	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

	res = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
	wpa_printf(MSG_DEBUG,
		   "DFS %d channels required radar detection", res);
	is_dfs = (res != 0);

	if (!is_dfs) {
		wpa_printf(MSG_DEBUG, "None of the channels are DFS");
		return;
	}

	res = dfs_check_chans_available(iface, start_chan_idx, n_chans);
	if (res) {
		wpa_printf(MSG_DEBUG, "All channels already available");
		return;
	}

	freq_params->skip_cac =
		(iface->cac_type != HAPD_CAC_COMPLETE_AFTER_CSA) &&
		iface->iface_extn.dfs_available_from_sta &&
		iconf->conf_extn.skip_cac;
	if (!freq_params->skip_cac &&
	    hostapd_mcst_allows_skip_cac_extn(iface->mcst,
					      iconf->beacon_int,
					      is_dfs))
		freq_params->skip_cac = true;

	/* CSwOpts 0x4: AP must perform CAC on STA-triggered bring-up.
	 * Override any skip_cac set above; the config-file skip_cac=1 may
	 * have been restored by a config reload after the runtime CSwOpts
	 * command cleared it to 0. */
	if (IS_CSH_CAC_APUP_BYSTA_ENABLED(iconf->conf_extn.cswopts))
		freq_params->skip_cac = false;

	wpa_printf(MSG_INFO,
		   "Dep Rptr: skip_cac = %d cac_type = %d"
		   " conf_extn.skip_cac = %d mcst = %u",
		   freq_params->skip_cac,
		   iface->cac_type,
		   iconf->conf_extn.skip_cac,
		   iface->mcst);
}
