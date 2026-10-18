/*
 * hostapd / Hardware feature query and different modes
 * Copyright 2002-2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2008-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/hw_features_common.h"
#include "hostapd.h"
#include "ap_config.h"
#include "ap_drv_ops.h"
#include "acs.h"
#include "ieee802_11.h"
#include "beacon.h"
#include "hw_features.h"
#include "dfs.h"
#ifdef CONFIG_QCN_EXTN
#include "ucode.h"
#include "../../qcn_extns/cmn.h"
#endif

void hostapd_free_6ghz_channels(struct hostapd_hw_modes *mode)
{
	struct hostapd_channel_data **chan_6ghz;
	int i;

	if (!mode)
		return;

	chan_6ghz = mode->channels_6ghz.chans_6ghz;
	if (!chan_6ghz)
		return;

	for (i = 0; i < NL80211_REG_NUM_POWER_MODES; i++)
		os_free(chan_6ghz[i]);
}

void hostapd_free_hw_features(struct hostapd_hw_modes *hw_features,
			      size_t num_hw_features)
{
	size_t i;

	if (hw_features == NULL)
		return;

#ifdef CONFIG_QCN_EXTN
	hostapd_free_hw_features_extn(hw_features, num_hw_features);
#endif

	for (i = 0; i < num_hw_features; i++) {
		os_free(hw_features[i].channels);
		os_free(hw_features[i].rates);
		hostapd_free_6ghz_channels(&hw_features[i]);
	}

	os_free(hw_features);
}


#ifndef CONFIG_NO_STDOUT_DEBUG
static char * dfs_info(struct hostapd_channel_data *chan)
{
	static char info[256];
	char *state;

	switch (chan->flag & HOSTAPD_CHAN_DFS_MASK) {
	case HOSTAPD_CHAN_DFS_UNKNOWN:
		state = "unknown";
		break;
	case HOSTAPD_CHAN_DFS_USABLE:
		state = "usable";
		break;
	case HOSTAPD_CHAN_DFS_UNAVAILABLE:
		state = "unavailable";
		break;
	case HOSTAPD_CHAN_DFS_AVAILABLE:
		state = "available";
		break;
	default:
		return "";
	}
	os_snprintf(info, sizeof(info), " (DFS state = %s)", state);
	info[sizeof(info) - 1] = '\0';

	return info;
}
#endif /* CONFIG_NO_STDOUT_DEBUG */


int hostapd_get_hw_features(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i, j;
	unsigned int k;
	u16 num_modes, flags;
	struct hostapd_hw_modes *modes;
	u8 dfs_domain;
	enum hostapd_hw_mode mode = HOSTAPD_MODE_IEEE80211ANY;
	bool is_6ghz = false;
	bool orig_mode_valid = false;
	struct hostapd_multi_hw_info *multi_hw_info;
	unsigned int num_multi_hws;

	if (hostapd_drv_none(hapd))
		return -1;
	modes = hostapd_get_hw_feature_data(hapd, &num_modes, &flags,
					    &dfs_domain);
	if (modes == NULL) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Fetching hardware channel/rate support not "
			       "supported.");
		return -1;
	}

	iface->hw_flags = flags;
	iface->dfs_domain = dfs_domain;

	if (iface->current_mode) {
		/*
		 * Received driver event CHANNEL_LIST_CHANGED when the current
		 * hw mode is valid. Clear iface->current_mode temporarily as
		 * the mode instance will be replaced with a new instance and
		 * the current pointer would be pointing to freed memory.
		 */
		orig_mode_valid = true;
		mode = iface->current_mode->mode;
		is_6ghz = iface->current_mode->is_6ghz;
		iface->current_mode = NULL;
	}

	/**
	 * This function frees survey results, and initializes the chan
	 * survey list. That is fine as the modes are freed anyway.
	 */
	for (i = 0; i < iface->num_hw_features; i++)
		acs_cleanup_mode(&iface->hw_features[i]);
	iface->chans_surveyed = 0;

	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = modes;
	iface->num_hw_features = num_modes;

	/**
	 * This is done to (re)initialize the survey list for new channels as
	 * the previous modes are freed.
	 */
	for (i = 0; i < iface->num_hw_features; i++)
		acs_cleanup_mode(&iface->hw_features[i]);

	for (i = 0; i < num_modes; i++) {
		struct hostapd_hw_modes *feature = &modes[i];
		int dfs_enabled = hapd->iconf->ieee80211h &&
			(iface->drv_flags & WPA_DRIVER_FLAGS_RADAR);

		/* Restore orignal mode if possible */
		if (orig_mode_valid && feature->mode == mode &&
		    feature->num_channels > 0 &&
		    is_6ghz == is_6ghz_freq(feature->channels[0].freq))
			iface->current_mode = feature;

		/* set flag for channels we can use in current regulatory
		 * domain */
		for (j = 0; j < feature->num_channels; j++) {
			int dfs = 0;

			/*
			 * Disable all channels that are marked not to allow
			 * to initiate radiation (a.k.a. passive scan and no
			 * IBSS).
			 * Use radar channels only if the driver supports DFS.
			 */
			if ((feature->channels[j].flag &
			     HOSTAPD_CHAN_RADAR) && dfs_enabled) {
				dfs = 1;
			} else if (((feature->channels[j].flag &
				     HOSTAPD_CHAN_RADAR) &&
				    !(iface->drv_flags &
				      WPA_DRIVER_FLAGS_DFS_OFFLOAD)) ||
				   (feature->channels[j].flag &
				    HOSTAPD_CHAN_NO_IR)) {
				feature->channels[j].flag |=
					HOSTAPD_CHAN_DISABLED;
			}

			if (feature->channels[j].flag & HOSTAPD_CHAN_DISABLED)
				continue;

			wpa_printf(MSG_MSGDUMP, "Allowed channel: mode=%d "
				   "chan=%d freq=%d MHz max_tx_power=%d dBm%s",
				   feature->mode,
				   feature->channels[j].chan,
				   feature->channels[j].freq,
				   feature->channels[j].max_tx_power,
				   dfs ? dfs_info(&feature->channels[j]) : "");
		}
	}

	if (orig_mode_valid && !iface->current_mode) {
		wpa_printf(MSG_ERROR,
			   "%s: Could not update iface->current_mode",
			   __func__);
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_update_primary_chanlist_flags(iface->bss[0]);
#endif /* CONFIG_QCN_EXTN */

	multi_hw_info = hostapd_get_multi_hw_info(hapd, &num_multi_hws);
	if (!multi_hw_info) {
		hostapd_check_get_afc_details(hapd);
#ifdef CONFIG_QCN_EXTN
		hostapd_query_hw_blocklist_extn(iface, hapd);
#endif
		return 0;
	}

	hostapd_free_multi_hw_info(iface->multi_hw_info);
	iface->current_hw_info = NULL;
	iface->multi_hw_info = multi_hw_info;
	iface->num_multi_hws = num_multi_hws;

	wpa_printf(MSG_DEBUG, "Multiple underlying hardwares info:");

	for (k = 0; k < num_multi_hws; k++) {
		struct hostapd_multi_hw_info *hw_info = &multi_hw_info[k];

		wpa_printf(MSG_DEBUG,
			   "  %d. hw_idx=%u, frequency range: %d-%d MHz",
			   k + 1, hw_info->hw_idx, hw_info->start_freq,
			   hw_info->end_freq);
	}

	hostapd_check_get_afc_details(hapd);
#ifdef CONFIG_QCN_EXTN
	hostapd_query_hw_blocklist_extn(iface, hapd);
	if (iface->conf && is_6ghz_op_class(iface->conf->op_class))
		hostapd_get_6ghz_thresh_priority_freq_extn(iface);
	hostapd_get_agile_capable_extn(iface);
#endif /* CONFIG_QCN_EXTN */

	return 0;
}


static int ieee80211n_allowed_ht40_channel_pair(struct hostapd_iface *iface)
{
	int pri_freq, sec_freq;
	struct hostapd_channel_data *p_chan, *s_chan;

	pri_freq = iface->freq;
	sec_freq = pri_freq + iface->conf->secondary_channel * 20;

	if (!iface->current_mode)
		return 0;

	p_chan = hw_get_channel_freq(iface->current_mode->mode, pri_freq, NULL,
				     iface->hw_features,
				     iface->num_hw_features);

	s_chan = hw_get_channel_freq(iface->current_mode->mode, sec_freq, NULL,
				     iface->hw_features,
				     iface->num_hw_features);

	return allowed_ht40_channel_pair(iface->current_mode->mode,
					 p_chan, s_chan);
}


static void ieee80211n_switch_pri_sec(struct hostapd_iface *iface)
{
	if (iface->conf->secondary_channel > 0) {
		iface->conf->channel += 4;
		iface->freq += 20;
		iface->conf->secondary_channel = -1;
	} else {
		iface->conf->channel -= 4;
		iface->freq -= 20;
		iface->conf->secondary_channel = 1;
	}
}


static int ieee80211n_check_40mhz_5g(struct hostapd_iface *iface,
				     struct wpa_scan_results *scan_res)
{
	unsigned int pri_freq, sec_freq;
	int res;
	struct hostapd_channel_data *pri_chan, *sec_chan;

	pri_freq = iface->freq;
	sec_freq = pri_freq + iface->conf->secondary_channel * 20;

	if (!iface->current_mode)
		return 0;
	pri_chan = hw_get_channel_freq(iface->current_mode->mode, pri_freq,
				       NULL, iface->hw_features,
				       iface->num_hw_features);
	sec_chan = hw_get_channel_freq(iface->current_mode->mode, sec_freq,
				       NULL, iface->hw_features,
				       iface->num_hw_features);

	res = check_40mhz_5g(scan_res, pri_chan, sec_chan);

	if (res == 2) {
		if (iface->conf->no_pri_sec_switch) {
			wpa_printf(MSG_DEBUG,
				   "Cannot switch PRI/SEC channels due to local constraint");
		} else {
			ieee80211n_switch_pri_sec(iface);
		}
	}

	return !!res;
}


static int ieee80211n_check_40mhz_2g4(struct hostapd_iface *iface,
				      struct wpa_scan_results *scan_res)
{
	int pri_chan, sec_chan;
#ifdef CONFIG_QCN_EXTN
	struct check_40mhz_2g4_extn_args extn_args = {
		.threshold = iface->conf->conf_extn.obss_snr_threshold,
	};
#endif /* CONFIG_QCN_EXTN */

	pri_chan = iface->conf->channel;
	sec_chan = pri_chan + iface->conf->secondary_channel * 4;

	return check_40mhz_2g4(iface->current_mode, scan_res, pri_chan,
			       sec_chan
#ifdef CONFIG_QCN_EXTN
			       , &extn_args
#endif /* CONFIG_QCN_EXTN */
			       );
}


static int
hostapd_setup_interface_complete_or_defer(struct hostapd_iface *iface, int err)
{
#ifdef CONFIG_IEEE80211BE
	if (!err && iface->bss[0]->conf->mld_ap && iface->interfaces &&
	    (iface->drv_flags2 & WPA_DRIVER_FLAGS2_PARALLEL_HW_SCAN)) {
		unsigned int i;
		bool partner_in_acs = false;

		for (i = 0; i < iface->interfaces->count; i++) {
			struct hostapd_iface *h = iface->interfaces->iface[i];
			struct hostapd_data *h_hapd;

			if (!h || h == iface || !h->num_bss)
				continue;
			h_hapd = h->bss[0];
			if (!hostapd_is_ml_partner(iface->bss[0], h_hapd))
				continue;
			if (h->state == HAPD_IFACE_ACS &&
			    !h->acs_setup_deferred) {
				partner_in_acs = true;
				break;
			}
		}

		if (partner_in_acs) {
			wpa_printf(MSG_DEBUG,
				   "ACS: deferring interface setup for %d MHz, "
				   "partner link still in ACS",
				   iface->freq);
			iface->acs_setup_deferred = 1;
			return 0;
		}

		/* Re-trigger any partner whose setup was deferred */
		for (i = 0; i < iface->interfaces->count; i++) {
			struct hostapd_iface *h = iface->interfaces->iface[i];
			struct hostapd_data *h_hapd;

			if (!h || h == iface || !h->num_bss)
				continue;
			h_hapd = h->bss[0];
			if (!hostapd_is_ml_partner(iface->bss[0], h_hapd))
				continue;
			if (!h->acs_setup_deferred)
				continue;
			h->acs_setup_deferred = 0;
			wpa_printf(MSG_DEBUG,
				   "ACS: re-triggering deferred interface "
				   "setup for %d MHz", h->freq);
			hostapd_setup_interface_complete(h, 0);
		}
	}
#endif /* CONFIG_IEEE80211BE */
	return hostapd_setup_interface_complete(iface, err);
}

static void ieee80211n_check_scan(struct hostapd_iface *iface)
{
	struct wpa_scan_results *scan_res;
	int oper40;
	int res = 0;

	/* Check list of neighboring BSSes (from scan) to see whether 40 MHz is
	 * allowed per IEEE Std 802.11-2012, 10.15.3.2 */

	iface->scan_cb = NULL;
	if (iface->state != HAPD_IFACE_HT_SCAN) {
		wpa_printf(MSG_DEBUG, "state %s, Ignore scan event",
			   hostapd_state_text(iface->state));
		return;
	}

	scan_res = hostapd_driver_get_scan_results(iface->bss[0]);
	if (scan_res == NULL) {
		if (hostapd_check_reenable_bss(iface, REENABLE_HT_SCAN))
			hostapd_enable_pending_bss(iface, REENABLE_HT_SCAN,
						   false);
		else if (!hostapd_check_reenable_bss(iface, REENABLE_NONE))
			hostapd_setup_interface_complete(iface, 1);
		return;
	}

	if (iface->current_mode->mode == HOSTAPD_MODE_IEEE80211A)
		oper40 = ieee80211n_check_40mhz_5g(iface, scan_res);
	else
		oper40 = ieee80211n_check_40mhz_2g4(iface, scan_res);
	wpa_scan_results_free(scan_res);

	iface->secondary_ch = iface->conf->secondary_channel;
	if (!oper40) {
		wpa_printf(MSG_INFO, "20/40 MHz operation not permitted on "
			   "channel pri=%d sec=%d based on overlapping BSSes",
			   iface->conf->channel,
			   iface->conf->channel +
			   iface->conf->secondary_channel * 4);
		iface->conf->secondary_channel = 0;
		if (iface->drv_flags & WPA_DRIVER_FLAGS_HT_2040_COEX) {
			/*
			 * TODO: Could consider scheduling another scan to check
			 * if channel width can be changed if no coex reports
			 * are received from associating stations.
			 */
		}
	}

#ifdef CONFIG_IEEE80211AX
	if (iface->conf->secondary_channel &&
	    iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    iface->conf->ieee80211ax) {
		struct he_capabilities *he_cap;

		he_cap = &iface->current_mode->he_capab[IEEE80211_MODE_AP];
		if (!(he_cap->phy_cap[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
		      HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_IN_2G)) {
			wpa_printf(MSG_DEBUG,
				   "HE: 40 MHz channel width is not supported in 2.4 GHz; clear secondary channel configuration");
			iface->conf->secondary_channel = 0;
		}
	}
#endif /* CONFIG_IEEE80211AX */

	if (iface->conf->secondary_channel)
		res = ieee80211n_allowed_ht40_channel_pair(iface);
	if (!res) {
		iface->conf->secondary_channel = 0;
		hostapd_set_oper_centr_freq_seg0_idx(iface->conf, 0);
		hostapd_set_oper_centr_freq_seg1_idx(iface->conf, 0);
		hostapd_set_oper_chwidth(iface->conf, CONF_OPER_CHWIDTH_USE_HT);
		res = 1;
		wpa_printf(MSG_INFO, "Fallback to 20 MHz");
	}

	if (hostapd_check_reenable_bss(iface, REENABLE_HT_SCAN))
		hostapd_enable_pending_bss(iface, REENABLE_HT_SCAN, false);
	else
		hostapd_setup_interface_complete(iface, !res);
}


static void ieee80211n_scan_channels_2g4(struct hostapd_iface *iface,
					 struct wpa_driver_scan_params *params)
{
	/* Scan only the affected frequency range */
	int pri_freq, sec_freq;
	int affected_start, affected_end;
	int i, pos;
	struct hostapd_hw_modes *mode;

	if (iface->current_mode == NULL)
		return;

	pri_freq = iface->freq;
	if (iface->conf->secondary_channel > 0)
		sec_freq = pri_freq + 20;
	else
		sec_freq = pri_freq - 20;
	/*
	 * Note: Need to find the PRI channel also in cases where the affected
	 * channel is the SEC channel of a 40 MHz BSS, so need to include the
	 * scanning coverage here to be 40 MHz from the center frequency.
	 */
	affected_start = (pri_freq + sec_freq) / 2 - 40;
	affected_end = (pri_freq + sec_freq) / 2 + 40;
	wpa_printf(MSG_DEBUG, "40 MHz affected channel range: [%d,%d] MHz",
		   affected_start, affected_end);

	mode = iface->current_mode;
	params->freqs = os_calloc(mode->num_channels + 1, sizeof(int));
	if (params->freqs == NULL)
		return;
	pos = 0;

	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (chan->freq < affected_start ||
		    chan->freq > affected_end)
			continue;
		params->freqs[pos++] = chan->freq;
	}
}


static void ieee80211n_scan_channels_5g(struct hostapd_iface *iface,
					struct wpa_driver_scan_params *params)
{
	/* Scan only the affected frequency range */
	int pri_freq;
	int affected_start, affected_end;
	int i, pos;
	struct hostapd_hw_modes *mode;

	if (iface->current_mode == NULL)
		return;

	pri_freq = iface->freq;
	if (iface->conf->secondary_channel > 0) {
		affected_start = pri_freq - 10;
		affected_end = pri_freq + 30;
	} else {
		affected_start = pri_freq - 30;
		affected_end = pri_freq + 10;
	}
	wpa_printf(MSG_DEBUG, "40 MHz affected channel range: [%d,%d] MHz",
		   affected_start, affected_end);

	mode = iface->current_mode;
	params->freqs = os_calloc(mode->num_channels + 1, sizeof(int));
	if (params->freqs == NULL)
		return;
	pos = 0;

	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (chan->freq < affected_start ||
		    chan->freq > affected_end)
			continue;
		params->freqs[pos++] = chan->freq;
	}
}


static void ap_ht40_scan_retry(void *eloop_data, void *user_data)
{
#define HT2040_COEX_SCAN_RETRY 15
	struct hostapd_iface *iface = eloop_data;
	struct wpa_driver_scan_params params;
	int ret;

	os_memset(&params, 0, sizeof(params));
	if (iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		ieee80211n_scan_channels_2g4(iface, &params);
	else
		ieee80211n_scan_channels_5g(iface, &params);

	if (iface->num_multi_hws) {
		params.bssid = iface->bss[0]->conf->bssid;
		wpa_printf(MSG_DEBUG, "HT40 scan triggered with bssid" MACSTR "\n",
			   MAC2STR(params.bssid));
	}

	ret = hostapd_driver_scan(iface->bss[0], &params);
	iface->num_ht40_scan_tries++;
	os_free(params.freqs);

	if (ret == -EBUSY &&
	    iface->num_ht40_scan_tries < HT2040_COEX_SCAN_RETRY) {
		wpa_printf(MSG_ERROR,
			   "Failed to request a scan of neighboring BSSes ret=%d (%s) - try to scan again (attempt %d)",
			   ret, strerror(-ret), iface->num_ht40_scan_tries);
		eloop_register_timeout(5, 0, ap_ht40_scan_retry, iface, NULL);
		return;
	}

	if (ret == 0) {
		iface->scan_cb = ieee80211n_check_scan;
		iface->bss[0]->scan_cookie = params.scan_cookie;
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "Failed to request a scan in device, bringing up in HT20 mode");
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, 0);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, 0);
	hostapd_set_oper_chwidth(iface->conf, CONF_OPER_CHWIDTH_USE_HT);
	iface->conf->secondary_channel = 0;
	iface->conf->ht_capab &= ~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
	if (hostapd_check_reenable_bss(iface, REENABLE_HT_SCAN))
		hostapd_enable_pending_bss(iface, REENABLE_HT_SCAN, false);
	else
		hostapd_setup_interface_complete(iface, 0);
}


void hostapd_stop_setup_timers(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(ap_ht40_scan_retry, iface, NULL);
}


static int ieee80211n_check_40mhz(struct hostapd_iface *iface)
{
	struct wpa_driver_scan_params params;
	struct hostapd_channel_data *pri_chan;
	int ret;

	/* Check that HT40 is used and PRI / SEC switch is allowed */
	if (!iface->conf->secondary_channel || iface->conf->no_pri_sec_switch ||
		iface->conf->noscan)
		return 0;

	/*
	 * Skip the 40 MHz scan when the primary channel is a DFS channel.
	 * A neighbor scan could trigger ieee80211n_switch_pri_sec() and
	 * move the primary off the configured DFS channel, violating
	 * operator intent and breaking CAC continuity. Secondary-channel
	 * availability is still enforced by
	 * ieee80211n_allowed_ht40_channel_pair().
	 */
	pri_chan = hw_get_channel_freq(iface->current_mode->mode,
				      iface->freq, NULL,
				      iface->hw_features,
				      iface->num_hw_features);
	if (pri_chan && (pri_chan->flag & HOSTAPD_CHAN_RADAR)) {
		wpa_printf(MSG_DEBUG,
			   "Skip 40 MHz scan: primary channel %d is DFS",
			   iface->conf->channel);
		return 0;
	}

	hostapd_set_state(iface, HAPD_IFACE_HT_SCAN);
	wpa_printf(MSG_DEBUG, "Scan for neighboring BSSes prior to enabling "
		   "40 MHz channel");
	os_memset(&params, 0, sizeof(params));
	if (iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		ieee80211n_scan_channels_2g4(iface, &params);
	else
		ieee80211n_scan_channels_5g(iface, &params);

	if (iface->num_multi_hws) {
		params.bssid = iface->bss[0]->conf->bssid;
		wpa_printf(MSG_DEBUG, "scan triggered with bssid" MACSTR "\n",
			   MAC2STR(params.bssid));
	}
	eloop_cancel_timeout(ap_ht40_scan_retry, iface, NULL);

	ret = hostapd_driver_scan(iface->bss[0], &params);
	os_free(params.freqs);

	if (ret == -EBUSY) {
		wpa_printf(MSG_ERROR,
			   "Failed to request a scan of neighboring BSSes ret=%d (%s) - try to scan again",
			   ret, strerror(-ret));
		iface->num_ht40_scan_tries = 1;
		eloop_register_timeout(1, 0, ap_ht40_scan_retry, iface, NULL);
		return 1;
	}

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "Failed to request a scan of neighboring BSSes ret=%d (%s)",
			   ret, strerror(-ret));
		return -1;
	}

	iface->scan_cb = ieee80211n_check_scan;
	iface->bss[0]->scan_cookie = params.scan_cookie;
	return 1;
}


static int ieee80211n_supported_ht_capab(struct hostapd_iface *iface)
{
	u16 hw = iface->current_mode->ht_capab;
	u16 conf = iface->conf->ht_capab;

	if ((conf & HT_CAP_INFO_LDPC_CODING_CAP) &&
	    !(hw & HT_CAP_INFO_LDPC_CODING_CAP)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [LDPC]");
		return 0;
	}

	/*
	 * Driver ACS chosen channel may not be HT40 due to internal driver
	 * restrictions.
	 */
	if (!iface->conf->acs && (conf & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET) &&
	    !(hw & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [HT40*]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_GREEN_FIELD) &&
	    !(hw & HT_CAP_INFO_GREEN_FIELD)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [GF]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_SHORT_GI20MHZ) &&
	    !(hw & HT_CAP_INFO_SHORT_GI20MHZ)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [SHORT-GI-20]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_SHORT_GI40MHZ) &&
	    !(hw & HT_CAP_INFO_SHORT_GI40MHZ)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [SHORT-GI-40]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_TX_STBC) && !(hw & HT_CAP_INFO_TX_STBC)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [TX-STBC]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_RX_STBC_MASK) >
	    (hw & HT_CAP_INFO_RX_STBC_MASK)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [RX-STBC*]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_DELAYED_BA) &&
	    !(hw & HT_CAP_INFO_DELAYED_BA)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [DELAYED-BA]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_MAX_AMSDU_SIZE) &&
	    !(hw & HT_CAP_INFO_MAX_AMSDU_SIZE)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [MAX-AMSDU-7935]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_DSSS_CCK40MHZ) &&
	    !(hw & HT_CAP_INFO_DSSS_CCK40MHZ)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [DSSS_CCK-40]");
		return 0;
	}

	if ((conf & HT_CAP_INFO_LSIG_TXOP_PROTECT_SUPPORT) &&
	    !(hw & HT_CAP_INFO_LSIG_TXOP_PROTECT_SUPPORT)) {
		wpa_printf(MSG_ERROR, "Driver does not support configured "
			   "HT capability [LSIG-TXOP-PROT]");
		return 0;
	}

	return 1;
}


#ifdef CONFIG_IEEE80211AC
static int ieee80211ac_supported_vht_capab(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	u32 hw = mode->vht_capab;
	u32 conf = iface->conf->vht_capab;

	wpa_printf(MSG_DEBUG, "hw vht capab: 0x%x, conf vht capab: 0x%x",
		   hw, conf);

	if (mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    iface->conf->bss[0]->vendor_vht &&
	    mode->vht_capab == 0 && iface->hw_features) {
		int i;

		for (i = 0; i < iface->num_hw_features; i++) {
			if (iface->hw_features[i].mode ==
			    HOSTAPD_MODE_IEEE80211A) {
				mode = &iface->hw_features[i];
				hw = mode->vht_capab;
				wpa_printf(MSG_DEBUG,
					   "update hw vht capab based on 5 GHz band: 0x%x",
					   hw);
				break;
			}
		}
	}

	return ieee80211ac_cap_check(hw, conf);
}
#endif /* CONFIG_IEEE80211AC */


#ifdef CONFIG_IEEE80211BE
static int ieee80211be_supported_eht_capab(struct hostapd_iface *iface)
{
	return iface->current_mode->eht_capab[IEEE80211_MODE_AP].eht_supported;
}

static int _ieee80211eht_cap_check(const u8 *hw, u32 offset, u8 bits)
{
	if (bits & hw[offset])
		return 1;

	return 0;
}
#endif /* CONFIG_IEEE80211BE */


#ifdef CONFIG_IEEE80211BN
static int ieee80211bn_supported_uhr_capab(struct hostapd_iface *iface)
{
	return iface->current_mode->uhr_capab[IEEE80211_MODE_AP].uhr_supported;
}

static int _ieee80211uhr_cap_check(const u8 *hw, u32 offset, u8 bits)
{
	if (bits & hw[offset])
		return 1;

	return 0;
}
#endif /* CONFIG_IEEE80211BN */


#ifdef CONFIG_IEEE80211AX
static int _ieee80211he_cap_check(u8 *hw, u32 offset, u8 bits)
{
	if (bits & hw[offset])
		return 1;

	return 0;
}

static int ieee80211ax_supported_he_capab(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct he_capabilities *hw = &mode->he_capab[IEEE80211_MODE_AP];
	struct hostapd_config *conf = iface->conf;

#define HE_CAP_CHECK(hw_cap, cap, bytes, conf) \
	do { \
		if (conf && !_ieee80211he_cap_check(hw_cap, bytes, cap)) { \
			wpa_printf(MSG_ERROR, "Driver does not support configured" \
				   " HE capability [%s]", #cap); \
			return 0; \
		} \
	} while (0)

	if (conf->he_phy_capab.he_ul_mumimo != -1)
		HE_CAP_CHECK(hw->phy_cap, HE_PHYCAP_UL_MUMIMO_CAPB,
			     HE_PHYCAP_UL_MUMIMO_CAPB_IDX,
			     conf->he_phy_capab.he_ul_mumimo);

	return 1;
}

#endif /* CONFIG_IEEE80211AX */

/**
 * hostapd_validate_bss_vht_capab - Validate BSS-level VHT capability overrides
 * @hapd: Pointer to hostapd_data
 * Returns: 0 on success, -1 on failure
 *
 * Validates that BSS-level VHT capability overrides don't exceed what the
 * driver/hardware supports.
 */
#ifdef CONFIG_IEEE80211AC
static int hostapd_validate_bss_vht_capab(struct hostapd_data *hapd)
{
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	u32 hw_vht, bss_vht, mask;

	if (!mode || !hapd->conf->vht_capab_mask)
		return 0;

	hw_vht = mode->vht_capab;
	bss_vht = hapd->conf->vht_capab;
	mask = hapd->conf->vht_capab_mask;

	/* Check SU Beamformer */
	if (mask & VHT_CAP_BSS_OVR_SU_BEAMFORMER) {
		if ((bss_vht & VHT_CAP_SU_BEAMFORMER_CAPABLE) &&
		    !(hw_vht & VHT_CAP_SU_BEAMFORMER_CAPABLE)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_vht_su_beamformer");
			return -1;
		}
	}

	/* Check SU Beamformee */
	if (mask & VHT_CAP_BSS_OVR_SU_BEAMFORMEE) {
		if ((bss_vht & VHT_CAP_SU_BEAMFORMEE_CAPABLE) &&
		    !(hw_vht & VHT_CAP_SU_BEAMFORMEE_CAPABLE)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_vht_su_beamformee");
			return -1;
		}
	}

	/* Check MU Beamformer */
	if (mask & VHT_CAP_BSS_OVR_MU_BEAMFORMER) {
		if ((bss_vht & VHT_CAP_MU_BEAMFORMER_CAPABLE) &&
		    !(hw_vht & VHT_CAP_MU_BEAMFORMER_CAPABLE)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_vht_mu_beamformer");
			return -1;
		}
	}

	/* Check MU Beamformee */
	/* Check Sounding Dimension */
	if (mask & VHT_CAP_BSS_OVR_SOUNDING_DIMENSION) {
		u32 hw_snd = (hw_vht & VHT_CAP_SOUNDING_DIMENSION_MAX) >>
			     VHT_CAP_SOUNDING_DIMENSION_OFFSET;
		u32 bss_snd = (bss_vht & VHT_CAP_SOUNDING_DIMENSION_MAX) >>
			      VHT_CAP_SOUNDING_DIMENSION_OFFSET;

		if (bss_snd > hw_snd) {
			wpa_printf(MSG_ERROR,
				   "Configured bss_vht_sounding_dimension (%u) exceeds driver max (%u)",
				   bss_snd, hw_snd);
			return -1;
		}
	}

	/* Check Beamformee STS */
	if (mask & VHT_CAP_BSS_OVR_STS_CAPABILITY) {
		u32 hw_sts = (hw_vht & VHT_CAP_BEAMFORMEE_STS_MAX) >>
			     VHT_CAP_BEAMFORMEE_STS_OFFSET;
		u32 bss_sts = (bss_vht & VHT_CAP_BEAMFORMEE_STS_MAX) >>
			      VHT_CAP_BEAMFORMEE_STS_OFFSET;

		if (bss_sts > hw_sts) {
			wpa_printf(MSG_ERROR,
				   "Configured bss_vht_beamformee_sts (%u) exceeds driver max (%u)",
				   bss_sts, hw_sts);
			return -1;
		}
	}

	return 0;
}
#endif /* CONFIG_IEEE80211AC */


/**
 * hostapd_validate_bss_he_capab - Validate BSS-level HE capability overrides
 * @hapd: Pointer to hostapd_data
 * Returns: 0 on success, -1 on failure
 *
 * Validates that BSS-level HE capability overrides don't exceed what the
 * driver/hardware supports.
 */
#ifdef CONFIG_IEEE80211AX
static int hostapd_validate_bss_he_capab(struct hostapd_data *hapd)
{
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct he_capabilities *hw_he;
	u64 mask;
	bool su_beamformee;
	bool supports_gt80;
	u8 hw_bfee_sts_lteq80;
	u8 hw_bfee_sts_gt80;
	u8 hw_multi_tid_aggr;
	u8 hw_multi_tid_aggr_tx;
	u8 hw_max_ampdu_len_exp;
	u8 hw_fragmentation;
	u8 hw_max_frag_msdu;
	u8 hw_min_frag_size;
	u8 hw_max_nc;

	if (!mode || !hapd->conf->he_phy_capab_mask)
		return 0;

	hw_he = &mode->he_capab[IEEE80211_MODE_AP];
	mask = hapd->conf->he_phy_capab_mask;

	if (!hw_he->he_supported) {
		wpa_printf(MSG_ERROR,
			   "Driver does not support HE but BSS HE params configured");
		return -1;
	}

	hw_multi_tid_aggr =
		(hw_he->mac_cap[HE_MACCAP_MULTI_TID_AGGR_RX_IDX] &
		 HE_MACCAP_MULTI_TID_AGGR_RX_MASK) >>
		HE_MACCAP_MULTI_TID_AGGR_RX_SHIFT;
	hw_max_ampdu_len_exp =
		(hw_he->mac_cap[HE_MACCAP_MAX_AMPDU_LEN_EXP_IDX] &
		 HE_MACCAP_MAX_AMPDU_LEN_EXP_MASK) >>
		HE_MACCAP_MAX_AMPDU_LEN_EXP_SHIFT;
	hw_fragmentation =
		(hw_he->mac_cap[HE_MACCAP_FRAGMENTATION_IDX] &
		 HE_MACCAP_FRAGMENTATION_MASK) >>
		HE_MACCAP_FRAGMENTATION_SHIFT;
	hw_max_frag_msdu =
		(hw_he->mac_cap[HE_MACCAP_MAX_FRAG_MSDU_IDX] &
		 HE_MACCAP_MAX_FRAG_MSDU_MASK) >>
		HE_MACCAP_MAX_FRAG_MSDU_SHIFT;
	hw_min_frag_size =
		(hw_he->mac_cap[HE_MACCAP_MIN_FRAG_SIZE_IDX] &
		 HE_MACCAP_MIN_FRAG_SIZE_MASK) >>
		HE_MACCAP_MIN_FRAG_SIZE_SHIFT;
	hw_multi_tid_aggr_tx =
		((hw_he->mac_cap[HE_MACCAP_MULTI_TID_AGGR_TX_LO_IDX] &
		  HE_MACCAP_MULTI_TID_AGGR_TX_LO_MASK) ? 1 : 0) |
		((hw_he->mac_cap[HE_MACCAP_MULTI_TID_AGGR_TX_HI_IDX] &
		  HE_MACCAP_MULTI_TID_AGGR_TX_HI_MASK) << 1);
	hw_max_nc = (hw_he->phy_cap[HE_PHYCAP_MAX_NC_IDX] &
		     HE_PHYCAP_MAX_NC_MASK) >>
		    HE_PHYCAP_MAX_NC_SHIFT;

	if (mask & HE_PHY_BSS_OVR_MULTI_TID_AGGR) {
		if (hapd->conf->he_phy_capab.he_multi_tid_aggr >
		    hw_multi_tid_aggr) {
			wpa_printf(MSG_ERROR,
				   "bss_he_multi_tid_aggr exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_multi_tid_aggr,
				   hw_multi_tid_aggr);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_MULTI_TID_AGGR_TX) {
		if (hapd->conf->he_phy_capab.he_multi_tid_aggr_tx >
		    hw_multi_tid_aggr_tx) {
			wpa_printf(MSG_ERROR,
				   "bss_he_multi_tid_aggr_tx exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_multi_tid_aggr_tx,
				   hw_multi_tid_aggr_tx);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_MAX_AMPDU_LEN_EXP) {
		if (hapd->conf->he_phy_capab.he_max_ampdu_len_exp >
		    hw_max_ampdu_len_exp) {
			wpa_printf(MSG_ERROR,
				   "bss_he_max_ampdu_len_exp exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_max_ampdu_len_exp,
				   hw_max_ampdu_len_exp);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_MAX_FRAG_MSDU) {
		if (hapd->conf->he_phy_capab.he_max_frag_msdu >
		    hw_max_frag_msdu) {
			wpa_printf(MSG_ERROR,
				   "bss_he_max_frag_msdu exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_max_frag_msdu,
				   hw_max_frag_msdu);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_FRAGMENTATION) {
		if (hapd->conf->he_phy_capab.he_fragmentation > 3) {
			wpa_printf(MSG_ERROR,
				   "bss_he_fragmentation has invalid value %u (expected 0..3)",
				   hapd->conf->he_phy_capab.he_fragmentation);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_fragmentation >
		    hw_fragmentation) {
			wpa_printf(MSG_ERROR,
				   "bss_he_fragmentation exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_fragmentation,
				   hw_fragmentation);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_AMSDU_IN_AMPDU_SUPRT) {
		if (hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_amsdu_in_ampdu_suprt has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt &&
		    !_ieee80211he_cap_check(hw_he->mac_cap,
					    HE_MACCAP_AMSDU_IN_AMPDU_IDX,
					    HE_MACCAP_AMSDU_IN_AMPDU)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_amsdu_in_ampdu_suprt");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_BSR_SUPPORT) {
		if (hapd->conf->he_phy_capab.he_bsr_support > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bsr_support has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_bsr_support);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_bsr_support &&
		    !_ieee80211he_cap_check(hw_he->mac_cap,
					    HE_MACCAP_BSR_IDX,
					    HE_MACCAP_BSR)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_bsr_support");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_MIN_FRAG_SIZE) {
		if (hapd->conf->he_phy_capab.he_min_frag_size > 3) {
			wpa_printf(MSG_ERROR,
				   "bss_he_min_frag_size has invalid value %u (expected 0..3)",
				   hapd->conf->he_phy_capab.he_min_frag_size);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_min_frag_size <
		    hw_min_frag_size) {
			wpa_printf(MSG_ERROR,
				   "bss_he_min_frag_size is below driver minimum capability (%u < %u)",
				   hapd->conf->he_phy_capab.he_min_frag_size,
				   hw_min_frag_size);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_OMI) {
		if (hapd->conf->he_phy_capab.he_omi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_omi has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_omi);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_omi &&
		    !_ieee80211he_cap_check(hw_he->mac_cap,
					    HE_MACCAP_OMI_IDX,
					    HE_MACCAP_OMI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_omi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_SU_PPDU_1X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_su_ppdu_1x_ltf_800ns_gi &&
		    !_ieee80211he_cap_check(
			    hw_he->phy_cap,
			    HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI_IDX,
			    HE_PHYCAP_SU_PPDU_1X_LTF_800NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_su_ppdu_1x_ltf_800ns_gi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_SU_MU_PPDU_4X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_su_mu_ppdu_4x_ltf_800ns_gi has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi &&
		    !_ieee80211he_cap_check(
			    hw_he->phy_cap,
			    HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI_IDX,
			    HE_PHYCAP_SU_MU_PPDU_4X_LTF_800NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_su_mu_ppdu_4x_ltf_800ns_gi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_NDP_4X_LTF_3200NS_GI) {
		if (hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_ndp_4x_ltf_3200ns_gi has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi &&
		    !_ieee80211he_cap_check(
			    hw_he->phy_cap,
			    HE_PHYCAP_NDP_4X_LTF_3200NS_GI_IDX,
			    HE_PHYCAP_NDP_4X_LTF_3200NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_ndp_4x_ltf_3200ns_gi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_ER_SU_PPDU_1X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_er_su_ppdu_1x_ltf_800ns_gi has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi &&
		    !_ieee80211he_cap_check(
			    hw_he->phy_cap,
			    HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI_IDX,
			    HE_PHYCAP_ER_SU_PPDU_1X_LTF_800NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_er_su_ppdu_1x_ltf_800ns_gi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_ER_SU_PPDU_4X_LTF_800NS_GI) {
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_er_su_ppdu_4x_ltf_800ns_gi has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi &&
		    !_ieee80211he_cap_check(
			    hw_he->phy_cap,
			    HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI_IDX,
			    HE_PHYCAP_ER_SU_PPDU_4X_LTF_800NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_er_su_ppdu_4x_ltf_800ns_gi");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_1024QAM_LT242RU_RX_ENABLE) {
		if (hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_he_1024qam_lt242ru_rx_enable has invalid value %u (expected 0..1)",
				   hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable);
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable &&
		    !_ieee80211he_cap_check(hw_he->phy_cap,
					    HE_PHYCAP_RX_1024QAM_LT242RU_IDX,
					    HE_PHYCAP_RX_1024QAM_LT242RU)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_1024qam_lt242ru_rx_enable");
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_ER_SU_DISABLE &&
	    hapd->conf->he_phy_capab.he_er_su_disable > 1) {
		wpa_printf(MSG_ERROR,
			   "bss_he_er_su_disable has invalid value %u (expected 0..1)",
			   hapd->conf->he_phy_capab.he_er_su_disable);
		return -1;
	}

	/* Check SU Beamformer */
	if (mask & HE_PHY_BSS_OVR_SU_BEAMFORMER) {
		if (hapd->conf->he_phy_capab.he_su_beamformer &&
		    !_ieee80211he_cap_check(hw_he->phy_cap,
					    HE_PHYCAP_SU_BEAMFORMER_CAPAB_IDX,
					    HE_PHYCAP_SU_BEAMFORMER_CAPAB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_su_beamformer");
			return -1;
		}
	}

	/* Check SU Beamformee */
	if (mask & HE_PHY_BSS_OVR_SU_BEAMFORMEE) {
		if (hapd->conf->he_phy_capab.he_su_beamformee &&
		    !_ieee80211he_cap_check(hw_he->phy_cap,
					    HE_PHYCAP_SU_BEAMFORMEE_CAPAB_IDX,
					    HE_PHYCAP_SU_BEAMFORMEE_CAPAB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_su_beamformee");
			return -1;
		}
	}

	su_beamformee =
		((mask & HE_PHY_BSS_OVR_SU_BEAMFORMEE) ?
		 hapd->conf->he_phy_capab.he_su_beamformee :
		 hapd->iface->conf->he_phy_capab.he_su_beamformee);

	supports_gt80 = !!(hw_he->phy_cap[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &
			   (HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
			    HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G));

	hw_bfee_sts_lteq80 =
		(hw_he->phy_cap[HE_PHYCAP_BFEE_STS_LTEQ80_IDX] &
		 HE_PHYCAP_BFEE_STS_LTEQ80_MASK) >>
		HE_PHYCAP_BFEE_STS_LTEQ80_SHIFT;
	hw_bfee_sts_gt80 =
		(hw_he->phy_cap[HE_PHYCAP_BFEE_STS_GT80_IDX] &
		 HE_PHYCAP_BFEE_STS_GT80_MASK) >>
		HE_PHYCAP_BFEE_STS_GT80_SHIFT;

	if (mask & HE_PHY_BSS_OVR_BFEE_STS_LTEQ80) {
		if (!su_beamformee &&
		    hapd->conf->he_phy_capab.he_bfee_sts_lteq80) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bfee_sts_lteq80 requires bss_he_su_beamformee");
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_bfee_sts_lteq80 >
		    hw_bfee_sts_lteq80) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bfee_sts_lteq80 exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_bfee_sts_lteq80,
				   hw_bfee_sts_lteq80);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_BFEE_STS_GT80) {
		if (!su_beamformee &&
		    hapd->conf->he_phy_capab.he_bfee_sts_gt80) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bfee_sts_gt80 requires bss_he_su_beamformee");
			return -1;
		}
		if (!supports_gt80 &&
		    hapd->conf->he_phy_capab.he_bfee_sts_gt80) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bfee_sts_gt80 requires >80MHz channel width support");
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_bfee_sts_gt80 >
		    hw_bfee_sts_gt80) {
			wpa_printf(MSG_ERROR,
				   "bss_he_bfee_sts_gt80 exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_bfee_sts_gt80,
				   hw_bfee_sts_gt80);
			return -1;
		}
	}

	if (mask & HE_PHY_BSS_OVR_MAX_NC_SUPRT) {
		if (!su_beamformee && hapd->conf->he_phy_capab.he_max_nc) {
			wpa_printf(MSG_ERROR,
				   "bss_he_max_nc_suprt requires bss_he_su_beamformee");
			return -1;
		}
		if (hapd->conf->he_phy_capab.he_max_nc > hw_max_nc) {
			wpa_printf(MSG_ERROR,
				   "bss_he_max_nc_suprt exceeds driver capability (%u > %u)",
				   hapd->conf->he_phy_capab.he_max_nc,
				   hw_max_nc);
			return -1;
		}
	}

	/* Check MU Beamformer */
	if (mask & HE_PHY_BSS_OVR_MU_BEAMFORMER) {
		if (hapd->conf->he_phy_capab.he_mu_beamformer &&
		    !_ieee80211he_cap_check(hw_he->phy_cap,
					    HE_PHYCAP_MU_BEAMFORMER_CAPAB_IDX,
					    HE_PHYCAP_MU_BEAMFORMER_CAPAB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_mu_beamformer");
			return -1;
		}
	}

	/* Check UL MU-MIMO */
	if (mask & HE_PHY_BSS_OVR_UL_MUMIMO) {
		if (hapd->conf->he_phy_capab.he_ul_mumimo == 1 &&
		    !_ieee80211he_cap_check(hw_he->phy_cap,
					    HE_PHYCAP_UL_MUMIMO_CAPB_IDX,
					    HE_PHYCAP_UL_MUMIMO_CAPB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_he_ul_mumimo");
			return -1;
		}
	}

	return 0;
}
#endif /* CONFIG_IEEE80211AX */


/**
 * hostapd_validate_bss_eht_capab - Validate BSS-level EHT capability overrides
 * @hapd: Pointer to hostapd_data
 * Returns: 0 on success, -1 on failure
 *
 * Validates that BSS-level EHT capability overrides don't exceed what the
 * driver/hardware supports.
 */
#ifdef CONFIG_IEEE80211BE
static int hostapd_validate_bss_eht_capab(struct hostapd_data *hapd)
{
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct eht_capabilities *hw_eht;
	u64 mask;
	bool su_bfmr;
	u8 hw_num_sd_lt80;
	u8 hw_num_sd_160;
	u8 hw_num_sd_320;
	u8 hw_sup_mcs15_in_mru;

	if (!mode || !hapd->conf->eht_phy_capab_mask)
		return 0;

	hw_eht = &mode->eht_capab[IEEE80211_MODE_AP];
	mask = hapd->conf->eht_phy_capab_mask;

	if (!hw_eht->eht_supported) {
		wpa_printf(MSG_ERROR,
			   "Driver does not support EHT but BSS EHT params configured");
		return -1;
	}

	/* Check SU Beamformer */
	if (mask & EHT_PHY_BSS_OVR_SU_BEAMFORMER) {
		if (hapd->conf->eht_phy_capab.su_beamformer) {
			/* Validate against driver EHT SU beamformer capability */
			wpa_printf(MSG_DEBUG,
				   "bss_eht_su_beamformer configured");
		}
	}

	/* Check SU Beamformee */
	if (mask & EHT_PHY_BSS_OVR_SU_BEAMFORMEE) {
		if (hapd->conf->eht_phy_capab.su_beamformee) {
			wpa_printf(MSG_DEBUG,
				   "bss_eht_su_beamformee configured");
		}
	}

	/* Check MU Beamformer */
	if (mask & EHT_PHY_BSS_OVR_MU_BEAMFORMER) {
		if (hapd->conf->eht_phy_capab.mu_beamformer) {
			wpa_printf(MSG_DEBUG,
				   "bss_eht_mu_beamformer configured");
		}
	}

	/* Validate beamformee spatial streams */
	if (hapd->conf->eht_phy_capab.eht_bfme_ss_80 > 7 ||
	    hapd->conf->eht_phy_capab.eht_bfme_ss_160 > 7 ||
	    hapd->conf->eht_phy_capab.eht_bfme_ss_320 > 7) {
		wpa_printf(MSG_ERROR,
			   "Invalid EHT beamformee spatial streams (max 7)");
		return -1;
	}
	if (hapd->conf->eht_phy_capab.eht_bfme_ss_80 ||
	    hapd->conf->eht_phy_capab.eht_bfme_ss_160 ||
	    hapd->conf->eht_phy_capab.eht_bfme_ss_320) {
		bool su_bfmee =
			(mask & EHT_PHY_BSS_OVR_SU_BEAMFORMEE) ?
			hapd->conf->eht_phy_capab.su_beamformee :
			hapd->iface->conf->eht_phy_capab.su_beamformee;

		if (!su_bfmee) {
			wpa_printf(MSG_ERROR,
				   "EHT BFME SS configured while SU beamformee disabled");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_NDP_4X_EHT_LTF_AND_320NSGI) {
		if (hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_ndp_4x_eht_ltf_and_320nsgi has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi);
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI_IDX,
			    EHT_PHYCAP_NDP_4X_EHT_LTF_AND_320NSGI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_ndp_4x_eht_ltf_and_320nsgi");
			return -1;
		}
	}

	su_bfmr = (mask & EHT_PHY_BSS_OVR_SU_BEAMFORMER) ?
		  hapd->conf->eht_phy_capab.su_beamformer :
		  hapd->iface->conf->eht_phy_capab.su_beamformer;

	hw_num_sd_lt80 =
		(hw_eht->phy_cap[EHT_PHYCAP_NUM_SD_LT80_IDX] &
		 EHT_PHYCAP_NUM_SD_LT80_MASK) >>
		EHT_PHYCAP_NUM_SD_LT80_SHIFT;
	hw_num_sd_160 =
		(hw_eht->phy_cap[EHT_PHYCAP_NUM_SD_160_IDX] &
		 EHT_PHYCAP_NUM_SD_160_MASK) >>
		EHT_PHYCAP_NUM_SD_160_SHIFT;
	hw_num_sd_320 =
		((hw_eht->phy_cap[EHT_PHYCAP_NUM_SD_320_LOW_IDX] &
		  EHT_PHYCAP_NUM_SD_320_LOW_MASK) >>
		 EHT_PHYCAP_NUM_SD_320_LOW_SHIFT) |
		(((hw_eht->phy_cap[EHT_PHYCAP_NUM_SD_320_HIGH_IDX] &
		   EHT_PHYCAP_NUM_SD_320_HIGH_MASK) >>
		  EHT_PHYCAP_NUM_SD_320_HIGH_SHIFT) << 2);

	if (mask & EHT_PHY_BSS_OVR_NUM_SD_LT80) {
		if (hapd->conf->eht_phy_capab.eht_num_sd_lt80 > 7) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_lt80 has invalid value %u (expected 0..7)",
				   hapd->conf->eht_phy_capab.eht_num_sd_lt80);
			return -1;
		}
		if (!su_bfmr && hapd->conf->eht_phy_capab.eht_num_sd_lt80) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_lt80 requires bss_eht_su_beamformer");
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_num_sd_lt80 > hw_num_sd_lt80) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_lt80 exceeds driver capability (%u > %u)",
				   hapd->conf->eht_phy_capab.eht_num_sd_lt80,
				   hw_num_sd_lt80);
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_NUM_SD_160) {
		if (hapd->conf->eht_phy_capab.eht_num_sd_160 > 7) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_160 has invalid value %u (expected 0..7)",
				   hapd->conf->eht_phy_capab.eht_num_sd_160);
			return -1;
		}
		if (!su_bfmr && hapd->conf->eht_phy_capab.eht_num_sd_160) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_160 requires bss_eht_su_beamformer");
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_num_sd_160 > hw_num_sd_160) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_160 exceeds driver capability (%u > %u)",
				   hapd->conf->eht_phy_capab.eht_num_sd_160,
				   hw_num_sd_160);
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_NUM_SD_320) {
		if (hapd->conf->eht_phy_capab.eht_num_sd_320 > 7) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_320 has invalid value %u (expected 0..7)",
				   hapd->conf->eht_phy_capab.eht_num_sd_320);
			return -1;
		}
		if (!su_bfmr && hapd->conf->eht_phy_capab.eht_num_sd_320) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_320 requires bss_eht_su_beamformer");
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_num_sd_320 > hw_num_sd_320) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_num_sd_320 exceeds driver capability (%u > %u)",
				   hapd->conf->eht_phy_capab.eht_num_sd_320,
				   hw_num_sd_320);
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_4X_EHT_LTF_AND_800NS_GI) {
		if (hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_4x_eht_ltf_and_800ns_gi has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi);
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI_IDX,
			    EHT_PHYCAP_4X_EHT_LTF_AND_800NS_GI)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_4x_eht_ltf_and_800ns_gi");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_RX_1024_AND_4096_QAM_LS_242_TONE_RU) {
		if (hapd->conf->eht_phy_capab
			    .eht_rx_1024_and_4096_qam_ls_242_tone_ru > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab
					   .eht_rx_1024_and_4096_qam_ls_242_tone_ru);
			return -1;
		}
		if (hapd->conf->eht_phy_capab
			    .eht_rx_1024_and_4096_qam_ls_242_tone_ru &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU_IDX,
			    EHT_PHYCAP_RX_1024_AND_4096_QAM_LS_242_TONE_RU)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_DL_OFDMA_TXBF) {
		if (hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_dl_ofdma_txbf has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf);
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB_IDX,
			    EHT_PHYCAP_TRIG_MU_BF_PART_BW_FB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_dl_ofdma_txbf");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_TRIG_SU_BF_FB) {
		if (hapd->conf->eht_phy_capab.eht_trig_su_bf_fb &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_TRIG_SU_BF_FB_IDX,
			    EHT_PHYCAP_TRIG_SU_BF_FB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_trig_su_bf_fb");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_TRIG_CQI_FB) {
		if (hapd->conf->eht_phy_capab.eht_trig_cqi_fb &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_TRIG_CQI_FB_IDX,
			    EHT_PHYCAP_TRIG_CQI_FB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_trig_cqi_fb");
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_NON_TRIG_CQI_FB) {
		if (hapd->conf->eht_phy_capab.eht_non_trig_cqi_fb &&
		    !_ieee80211eht_cap_check(
			    hw_eht->phy_cap,
			    EHT_PHYCAP_NON_TRIG_CQI_FB_IDX,
			    EHT_PHYCAP_NON_TRIG_CQI_FB)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_non_trig_cqi_fb");
			return -1;
		}
	}

	hw_sup_mcs15_in_mru =
		(hw_eht->phy_cap[EHT_PHYCAP_SUP_MCS15_IN_MRU_IDX] &
		 EHT_PHYCAP_SUP_MCS15_IN_MRU_MASK) >>
		EHT_PHYCAP_SUP_MCS15_IN_MRU_SHIFT;

	if (mask & EHT_PHY_BSS_OVR_SUP_MCS15_IN_MRU) {
		if (hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_sup_mcs15_in_mru has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru);
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru >
		    hw_sup_mcs15_in_mru) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_sup_mcs15_in_mru exceeds driver capability (%u > %u)",
				   hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru,
				   hw_sup_mcs15_in_mru);
			return -1;
		}
	}

	if (mask & EHT_PHY_BSS_OVR_MCS14_DUP_IN_6GHZ) {
		if (hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz > 1) {
			wpa_printf(MSG_ERROR,
				   "bss_eht_mcs14_dup_in_6ghz has invalid value %u (expected 0..1)",
				   hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz);
			return -1;
		}
		if (hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz &&
		    !_ieee80211eht_cap_check(hw_eht->phy_cap,
					     EHT_PHYCAP_MCS14_DUP_IN_6GHZ_IDX,
					     EHT_PHYCAP_MCS14_DUP_IN_6GHZ)) {
			wpa_printf(MSG_ERROR,
				   "Driver does not support bss_eht_mcs14_dup_in_6ghz");
			return -1;
		}
	}

	return 0;
}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
static int hostapd_validate_bss_uhr_capab(struct hostapd_data *hapd)
{
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct uhr_capabilities *hw_uhr;
	u32 mask;

	if (!mode || !hapd->conf->uhr_phy_capab_mask)
		return 0;

	hw_uhr = &mode->uhr_capab[IEEE80211_MODE_AP];
	mask = hapd->conf->uhr_phy_capab_mask;

	if (!hw_uhr->uhr_supported) {
		wpa_printf(MSG_ERROR,
			   "Driver does not support UHR but BSS UHR params configured");
		return -1;
	}

	if ((mask & UHR_PHY_BSS_OVR_2XLDPC_TX) &&
	    hapd->conf->uhr_phy_capab.uhr_2xldpc_tx &&
	    !_ieee80211uhr_cap_check(hw_uhr->phy_cap,
				UHR_PHYCAP_2XLDPC_TX_SUPP_IDX,
				UHR_PHYCAP_2XLDPC_TX_SUPP)) {
		wpa_printf(MSG_ERROR,
			   "Driver does not support uhr_2xldpc_tx");
		return -1;
	}

	if ((mask & UHR_PHY_BSS_OVR_2XLDPC_RX) &&
	    hapd->conf->uhr_phy_capab.uhr_2xldpc_rx &&
	    !_ieee80211uhr_cap_check(hw_uhr->phy_cap,
				UHR_PHYCAP_2XLDPC_RX_SUPP_IDX,
				UHR_PHYCAP_2XLDPC_RX_SUPP)) {
		wpa_printf(MSG_ERROR,
			   "Driver does not support uhr_2xldpc_rx");
		return -1;
	}

	return 0;
}
#endif /* CONFIG_IEEE80211BN */

/**
 * hostapd_validate_bss_capab - Validate all BSS-level capability overrides
 * @hapd: Pointer to hostapd_data
 * Returns: 0 on success, -1 on failure
 *
 * Main validation function that checks all BSS-level VHT/HE/EHT capability
 * overrides against driver/hardware capabilities.
 */
int hostapd_validate_bss_capab(struct hostapd_data *hapd)
{
	if (!hapd || !hapd->iface || !hapd->iface->current_mode)
		return 0;

#ifdef CONFIG_IEEE80211AC
	if (hostapd_validate_bss_vht_capab(hapd) < 0)
		return -1;
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_validate_bss_he_capab(hapd) < 0)
		return -1;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_validate_bss_eht_capab(hapd) < 0)
		return -1;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_validate_bss_uhr_capab(hapd) < 0)
		return -1;
#endif /* CONFIG_IEEE80211BN */

	return 0;
}

static int hostapd_check_phy_capab(struct hostapd_iface *iface)
{

#ifdef CONFIG_IEEE80211AX
	if (iface->conf->ieee80211ax &&
	    !ieee80211ax_supported_he_capab(iface)) {
		wpa_printf(MSG_ERROR, "Driver does not support HE");
		return -1;
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	if (iface->conf->ieee80211be &&
	    !ieee80211be_supported_eht_capab(iface)) {
		wpa_printf(MSG_ERROR, "Driver does not support EHT");
		return -1;
	}
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211BN
	if (iface->conf->ieee80211bn &&
	    !ieee80211bn_supported_uhr_capab(iface)) {
		wpa_printf(MSG_ERROR, "Driver does not support UHR");
		return -1;
	}
#endif /* CONFIG_IEEE80211BN */
	return 0;
}



int hostapd_check_ht_capab(struct hostapd_iface *iface)
{
	int ret;

	if (is_6ghz_freq(iface->freq))
		return hostapd_check_phy_capab(iface);

	if (!iface->conf->ieee80211n)
		return 0;

	if (iface->current_mode->mode != HOSTAPD_MODE_IEEE80211B &&
	    iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G &&
	    (iface->conf->ht_capab & HT_CAP_INFO_DSSS_CCK40MHZ)) {
		wpa_printf(MSG_DEBUG,
			   "Disable HT capability [DSSS_CCK-40] on 5 GHz band");
		iface->conf->ht_capab &= ~HT_CAP_INFO_DSSS_CCK40MHZ;
	}

	if (!ieee80211n_supported_ht_capab(iface))
		return -1;
#ifdef CONFIG_IEEE80211AC
	if (iface->conf->ieee80211ac &&
	    !ieee80211ac_supported_vht_capab(iface))
		return -1;
#endif /* CONFIG_IEEE80211AC */

	if (hostapd_check_phy_capab(iface))
		return -1;

	 if (!iface->conf->disable_40mhz_scan) {
		 ret = ieee80211n_check_40mhz(iface);
		 if (ret)
			 return ret;
	 } else {
		 wpa_printf(MSG_INFO, "%s:40mhz scan disabled",
		 iface->conf->bss[0]->iface);
	 }
	if (!ieee80211n_allowed_ht40_channel_pair(iface))
		return -1;

	return 0;
}


int hostapd_check_edmg_capab(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode = iface->hw_features;
	struct ieee80211_edmg_config edmg;

	if (!iface->conf->enable_edmg)
		return 0;

	hostapd_encode_edmg_chan(iface->conf->enable_edmg,
				 iface->conf->edmg_channel,
				 iface->conf->channel,
				 &edmg);

	if (mode->edmg.channels && ieee802_edmg_is_allowed(mode->edmg, edmg))
		return 0;

	wpa_printf(MSG_WARNING, "Requested EDMG configuration is not valid");
	wpa_printf(MSG_INFO, "EDMG capab: channels 0x%x, bw_config %d",
		   mode->edmg.channels, mode->edmg.bw_config);
	wpa_printf(MSG_INFO,
		   "Requested EDMG configuration: channels 0x%x, bw_config %d",
		   edmg.channels, edmg.bw_config);
	return -1;
}


int hostapd_check_he_6ghz_capab(struct hostapd_iface *iface)
{
#ifdef CONFIG_IEEE80211AX
	struct he_capabilities *he_cap;
	u16 hw;

	if (!iface->current_mode || !is_6ghz_freq(iface->freq))
		return 0;

	he_cap = &iface->current_mode->he_capab[IEEE80211_MODE_AP];
	hw = he_cap->he_6ghz_capa;
	if (iface->conf->he_6ghz_max_mpdu >
	    ((hw & HE_6GHZ_BAND_CAP_MAX_MPDU_LEN_MASK) >>
	     HE_6GHZ_BAND_CAP_MAX_MPDU_LEN_SHIFT)) {
		wpa_printf(MSG_ERROR,
			   "The driver does not support the configured HE 6 GHz Max MPDU length");
		return -1;
	}

	if (iface->conf->he_6ghz_max_ampdu_len_exp >
	    ((hw & HE_6GHZ_BAND_CAP_MAX_AMPDU_LEN_EXP_MASK) >>
	     HE_6GHZ_BAND_CAP_MAX_AMPDU_LEN_EXP_SHIFT)) {
		wpa_printf(MSG_ERROR,
			   "The driver does not support the configured HE 6 GHz Max AMPDU Length Exponent");
		return -1;
	}

	if (iface->conf->he_6ghz_rx_ant_pat &&
	    !(hw & HE_6GHZ_BAND_CAP_RX_ANTPAT_CONS)) {
		wpa_printf(MSG_ERROR,
			   "The driver does not support the configured HE 6 GHz Rx Antenna Pattern");
		return -1;
	}

	if (iface->conf->he_6ghz_tx_ant_pat &&
	    !(hw & HE_6GHZ_BAND_CAP_TX_ANTPAT_CONS)) {
		wpa_printf(MSG_ERROR,
			   "The driver does not support the configured HE 6 GHz Tx Antenna Pattern");
		return -1;
	}
#endif /* CONFIG_IEEE80211AX */
	return 0;
}


/* Returns:
 * 1 = usable
 * 0 = not usable
 * -1 = not currently usable due to 6 GHz NO-IR
 */
#ifndef CONFIG_QCN_EXTN
static
#endif
int hostapd_is_usable_chan(struct hostapd_iface *iface,
				  int frequency, int primary)
{
	struct hostapd_channel_data *chan;

	if (!iface->current_mode)
		return 0;

	chan = hw_get_channel_freq(iface->current_mode->mode, frequency, NULL,
				   iface->hw_features, iface->num_hw_features);
	if (!chan)
		return 0;

	if ((primary && chan_pri_allowed(chan)) ||
	    (!primary && !(chan->flag & HOSTAPD_CHAN_DISABLED)))
		return 1;

	wpa_printf(MSG_INFO,
		   "Frequency %d (%s) not allowed for AP mode, flags: 0x%x%s%s",
		   frequency, primary ? "primary" : "secondary",
		   chan->flag,
		   chan->flag & HOSTAPD_CHAN_NO_IR ? " NO-IR" : "",
		   chan->flag & HOSTAPD_CHAN_RADAR ? " RADAR" : "");

	if (is_6ghz_freq(chan->freq) && (chan->flag & HOSTAPD_CHAN_NO_IR))
		return -1;

	return 0;
}


static int hostapd_is_usable_edmg(struct hostapd_iface *iface)
{
	int i, contiguous = 0;
	int num_of_enabled = 0;
	int max_contiguous = 0;
	int err;
	struct ieee80211_edmg_config edmg;
	struct hostapd_channel_data *pri_chan;

	if (!iface->conf->enable_edmg)
		return 1;

	if (!iface->current_mode)
		return 0;
	pri_chan = hw_get_channel_freq(iface->current_mode->mode,
				       iface->freq, NULL,
				       iface->hw_features,
				       iface->num_hw_features);
	if (!pri_chan)
		return 0;
	hostapd_encode_edmg_chan(iface->conf->enable_edmg,
				 iface->conf->edmg_channel,
				 pri_chan->chan,
				 &edmg);
	if (!(edmg.channels & BIT(pri_chan->chan - 1)))
		return 0;

	/* 60 GHz channels 1..6 */
	for (i = 0; i < 6; i++) {
		int freq = 56160 + 2160 * (i + 1);

		if (edmg.channels & BIT(i)) {
			contiguous++;
			num_of_enabled++;
		} else {
			contiguous = 0;
			continue;
		}

		/* P802.11ay defines that the total number of subfields
		 * set to one does not exceed 4.
		 */
		if (num_of_enabled > 4)
			return 0;

		err = hostapd_is_usable_chan(iface, freq, 1);
		if (err <= 0)
			return err;

		if (contiguous > max_contiguous)
			max_contiguous = contiguous;
	}

	/* Check if the EDMG configuration is valid under the limitations
	 * of P802.11ay.
	 */
	/* check bw_config against contiguous EDMG channels */
	switch (edmg.bw_config) {
	case EDMG_BW_CONFIG_4:
		if (!max_contiguous)
			return 0;
		break;
	case EDMG_BW_CONFIG_5:
		if (max_contiguous < 2)
			return 0;
		break;
	default:
		return 0;
	}

	return 1;
}


bool hostapd_is_usable_punct_bitmap(struct hostapd_iface *iface)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_config *conf = iface->conf;
	u16 bw;
	u8 start_chan;

	if (!conf->punct_bitmap)
		return true;

	if (!conf->ieee80211be) {
		wpa_printf(MSG_ERROR,
			   "Currently RU puncturing is supported only if ieee80211be is enabled");
		return false;
	}

	if (iface->freq >= 2412 && iface->freq <= 2484) {
		wpa_printf(MSG_ERROR,
			   "RU puncturing not supported in 2.4 GHz");
		return false;
	}

#ifdef CONFIG_MESH
	if (iface->mconf != NULL) {
		wpa_printf(MSG_DEBUG,
			   "%s: Mesh puncturing bitmap will be validated in kernel while joining the mesh network",
			   iface->bss[0]->conf->iface);
		return true;
	}
#endif
	/*
	 * In the 6 GHz band, eht_oper_chwidth is ignored. Use operating class
	 * to determine channel width.
	 */
	if (conf->op_class == 137) {
		bw = 320;
		start_chan = conf->eht_oper_centr_freq_seg0_idx - 30;
	} else {
		switch (conf->eht_oper_chwidth) {
		case 0:
			wpa_printf(MSG_ERROR,
				   "RU puncturing is supported only in 80 MHz and 160 MHz");
			return false;
		case 1:
			bw = 80;
			start_chan = conf->eht_oper_centr_freq_seg0_idx - 6;
			break;
		case 2:
			bw = 160;
			start_chan = conf->eht_oper_centr_freq_seg0_idx - 14;
			break;
		default:
#ifdef CONFIG_QCN_EXTN
			if (hostapd_get_bw_and_startchan_for_240mhz_extn(
				conf->eht_oper_chwidth,
				conf->eht_oper_centr_freq_seg0_idx,
				&bw, &start_chan))

			return false;
#endif /* CONFIG_QCN_EXTN */
		}
	}

	if (!is_punct_bitmap_valid(bw, (conf->channel - start_chan) / 4,
				   conf->punct_bitmap)) {
		wpa_printf(MSG_ERROR, "Invalid puncturing bitmap");
		return false;
	}
#endif /* CONFIG_IEEE80211BE */

	return true;
}


/* Returns:
 * 1 = usable
 * 0 = not usable
 * -1 = not currently usable due to 6 GHz NO-IR
 */
static int hostapd_is_usable_chans(struct hostapd_iface *iface)
{
	int secondary_freq;
	struct hostapd_channel_data *pri_chan;
	int err, err2;

	if (!iface->current_mode)
		return 0;
	pri_chan = hw_get_channel_freq(iface->current_mode->mode,
				       iface->freq, NULL,
				       iface->hw_features,
				       iface->num_hw_features);
	if (!pri_chan) {
		wpa_printf(MSG_ERROR, "Primary frequency not present");
		return 0;
	}

	err = hostapd_is_usable_chan(iface, pri_chan->freq, 1);
	if (err <= 0) {
		wpa_printf(MSG_ERROR, "Primary frequency not allowed");
		return err;
	}
	err = hostapd_is_usable_edmg(iface);
	if (err <= 0)
		return err;

	if (!hostapd_is_usable_punct_bitmap(iface))
		return 0;

	if (!iface->conf->secondary_channel)
		return 1;

	err = hostapd_is_usable_chan(iface, iface->freq +
				     iface->conf->secondary_channel * 20, 0);
	if (err > 0) {
		if (iface->conf->secondary_channel == 1 &&
		    (pri_chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40P))
			return 1;
		if (iface->conf->secondary_channel == -1 &&
		    (pri_chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40M))
			return 1;
	}
	if (!iface->conf->ht40_plus_minus_allowed)
		return err;

	/* Both HT40+ and HT40- are set, pick a valid secondary channel */
	secondary_freq = iface->freq + 20;
	err2 = hostapd_is_usable_chan(iface, secondary_freq, 0);
	if (err2 > 0 && (pri_chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40P)) {
		iface->conf->secondary_channel = 1;
		return 1;
	}

	secondary_freq = iface->freq - 20;
	err2 = hostapd_is_usable_chan(iface, secondary_freq, 0);
	if (err2 > 0 && (pri_chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40M)) {
		iface->conf->secondary_channel = -1;
		return 1;
	}

	return err;
}


static bool skip_mode(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode)
{
	int chan;

	if (iface->freq > 0 && !hw_mode_get_channel(mode, iface->freq, &chan))
		return true;

	if (is_6ghz_op_class(iface->conf->op_class) && iface->freq == 0 &&
	    !mode->is_6ghz)
		return true;

	return false;
}


int hostapd_determine_mode(struct hostapd_iface *iface)
{
	int i;
	enum hostapd_hw_mode target_mode;

	if (iface->current_mode ||
	    iface->conf->hw_mode != HOSTAPD_MODE_IEEE80211ANY)
		return 0;

	if (iface->freq < 4000)
		target_mode = HOSTAPD_MODE_IEEE80211G;
	else if (iface->freq > 50000)
		target_mode = HOSTAPD_MODE_IEEE80211AD;
	else
		target_mode = HOSTAPD_MODE_IEEE80211A;

	for (i = 0; i < iface->num_hw_features; i++) {
		struct hostapd_hw_modes *mode;

		mode = &iface->hw_features[i];
		if (mode->mode == target_mode) {
			if (skip_mode(iface, mode))
				continue;

			iface->current_mode = mode;
			iface->conf->hw_mode = mode->mode;
			break;
		}
	}

	if (!iface->current_mode) {
		wpa_printf(MSG_ERROR, "ACS/CSA: Cannot decide mode");
		return -1;
	}
	return 0;
}


static enum hostapd_chan_status
hostapd_check_chans(struct hostapd_iface *iface)
{
	if (iface->freq) {
		int err;

		hostapd_determine_mode(iface);

		err = hostapd_is_usable_chans(iface);
		if (err <= 0) {
			if (!err)
				return HOSTAPD_CHAN_INVALID;
			return HOSTAPD_CHAN_INVALID_NO_IR;
		}
		return HOSTAPD_CHAN_VALID;
	}

	/*
	 * The user set channel=0 or channel=acs_survey
	 * which is used to trigger ACS.
	 */

	switch (acs_init(iface)) {
	case HOSTAPD_CHAN_ACS:
		return HOSTAPD_CHAN_ACS;
	case HOSTAPD_CHAN_INVALID_NO_IR:
		return HOSTAPD_CHAN_INVALID_NO_IR;
	case HOSTAPD_CHAN_VALID:
	case HOSTAPD_CHAN_INVALID:
	default:
		return HOSTAPD_CHAN_INVALID;
	}
}


static void hostapd_notify_bad_chans(struct hostapd_iface *iface)
{
	if (!iface->current_mode) {
		hostapd_logger(iface->bss[0], NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "Hardware does not support configured mode");
		return;
	}
	hostapd_logger(iface->bss[0], NULL,
		       HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_WARNING,
		       "Configured channel (%d) or frequency (%d) (secondary_channel=%d) not found from the channel list of the current mode (%d) %s",
		       iface->conf->channel,
		       iface->freq, iface->conf->secondary_channel,
		       iface->current_mode->mode,
		       hostapd_hw_mode_txt(iface->current_mode->mode));
	hostapd_logger(iface->bss[0], NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_WARNING,
		       "Hardware does not support configured channel");
}


int hostapd_acs_completed(struct hostapd_iface *iface, int err)
{
	int ret = -1;

	if (err)
		goto out;

	switch (hostapd_check_chans(iface)) {
	case HOSTAPD_CHAN_VALID:
		iface->is_no_ir = false;
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO,
			ACS_EVENT_COMPLETED "freq=%d channel=%d",
			iface->freq, iface->conf->channel);
		/*
		 * - Non-DFS channels, or DFS where all subchannels are
		 *   DFS_AVAILABLE (including pre-CAC/background CAC),
		 *   notify wpa_supp to start STA scan.
		 * - DFS channels/BW where any subchannel is not
		 *   DFS_AVAILABLE require CAC, we set the pending flag
		 *   and notify post CAC.
		 */
		if (hostapd_is_dfs_required(iface)) {
			if (hostapd_is_cac_required(iface)) {
				wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO,
					"ACS: channel/BW with CAC still required,"
					" set the flag and notify post CAC");
#ifdef CONFIG_QCN_EXTN
				iface->iface_extn.acs_dfs_cac_pending = true;
#endif
			} else {
				wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO,
					"ACS: with CAC already done, notify now");
#ifdef CONFIG_QCN_EXTN
				hostapd_ml_acs_check_and_notify(iface, true);
#endif /* CONFIG_QCN_EXTN */
			}
		} else {
			wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO,
				"ACS: non-DFS channel/BW: AP will start immediately,"
				" notify now");
#ifdef CONFIG_QCN_EXTN
			hostapd_ml_acs_check_and_notify(iface, true);
#endif /* CONFIG_QCN_EXTN */
		}
		break;
	case HOSTAPD_CHAN_ACS:
		wpa_printf(MSG_ERROR, "ACS error - reported complete, but no result available");
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, ACS_EVENT_FAILED);
		hostapd_notify_bad_chans(iface);
		goto out;
	case HOSTAPD_CHAN_INVALID_NO_IR:
		iface->is_no_ir = true;
		/* fall through */
	case HOSTAPD_CHAN_INVALID:
	default:
		wpa_printf(MSG_ERROR, "ACS picked unusable channels");
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, ACS_EVENT_FAILED);
		hostapd_notify_bad_chans(iface);
		goto out;
	}

	ret = hostapd_check_ht_capab(iface);
	if (ret < 0)
		goto out;
	if (ret == 1) {
		wpa_printf(MSG_DEBUG, "Interface initialization will be completed in a callback");
		return 0;
	}

	ret = 0;
out:
	return hostapd_setup_interface_complete_or_defer(iface, ret);
}


/**
 * hostapd_csa_update_hwmode - Update hardware mode
 * @iface: Pointer to interface data.
 * Returns: 0 on success, < 0 on failure
 *
 * Update hardware mode when the operating channel changed because of CSA.
 */
int hostapd_csa_update_hwmode(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return -1;

	iface->current_mode = NULL;
	iface->conf->hw_mode = HOSTAPD_MODE_IEEE80211ANY;

	return hostapd_determine_mode(iface);
}


/**
 * hostapd_select_hw_mode - Select the hardware mode
 * @iface: Pointer to interface data.
 * Returns: 0 on success, < 0 on failure
 *
 * Sets up the hardware mode, channel, rates, and passive scanning
 * based on the configuration.
 */
int hostapd_select_hw_mode(struct hostapd_iface *iface)
{
	int i;

	if (iface->num_hw_features < 1)
		return -1;

	if ((iface->conf->hw_mode == HOSTAPD_MODE_IEEE80211G ||
	     iface->conf->ieee80211n || iface->conf->ieee80211ac ||
	     iface->conf->ieee80211ax || iface->conf->ieee80211be ||
	     iface->conf->ieee80211bn) &&
	    iface->conf->channel == 14) {
		wpa_printf(MSG_INFO, "Disable OFDM/HT/VHT/HE/EHT/UHR on channel 14");
		iface->conf->hw_mode = HOSTAPD_MODE_IEEE80211B;
		iface->conf->ieee80211n = 0;
		iface->conf->ieee80211ac = 0;
		iface->conf->ieee80211ax = 0;
		iface->conf->ieee80211be = 0;
		iface->conf->ieee80211bn = 0;
	}

	iface->current_mode = NULL;
	for (i = 0; i < iface->num_hw_features; i++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[i];

		if (mode->mode == iface->conf->hw_mode) {
			if (skip_mode(iface, mode))
				continue;

			iface->current_mode = mode;
			break;
		}
	}

	if (iface->current_mode == NULL) {
		if ((iface->drv_flags & WPA_DRIVER_FLAGS_ACS_OFFLOAD) &&
		    (iface->drv_flags & WPA_DRIVER_FLAGS_SUPPORT_HW_MODE_ANY)) {
			wpa_printf(MSG_DEBUG,
				   "Using offloaded hw_mode=any ACS");
		} else if (!(iface->drv_flags & WPA_DRIVER_FLAGS_ACS_OFFLOAD) &&
			   iface->conf->hw_mode == HOSTAPD_MODE_IEEE80211ANY) {
			wpa_printf(MSG_DEBUG,
				   "Using internal ACS for hw_mode=any");
		} else {
			wpa_printf(MSG_ERROR,
				   "Hardware does not support configured mode");
			hostapd_logger(iface->bss[0], NULL,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "Hardware does not support configured mode (%d) (hw_mode in hostapd.conf)",
				       (int) iface->conf->hw_mode);
			return -2;
		}
	}

	switch (hostapd_check_chans(iface)) {
	case HOSTAPD_CHAN_VALID:
		iface->is_no_ir = false;

		if (iface->conf->use_ru_puncture_dfs && iface->conf->punct_bitmap) {
			enum chan_width ch_width;

			ch_width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
			wpa_printf(MSG_DEBUG,
				   "DFS: Update puncture source for User puncture bitmap=0x%04x",
				   iface->conf->punct_bitmap);
			dfs_update_puncture_source(iface, 0, ch_width,
						   iface->conf->punct_bitmap,
						   DFS_CHAN_PUNC_USER);
		}
		return 0;
	case HOSTAPD_CHAN_ACS: /* ACS will run and later complete */
		return 1;
	case HOSTAPD_CHAN_INVALID_NO_IR:
		iface->is_no_ir = true;
		/* fall through */
	case HOSTAPD_CHAN_INVALID:
	default:
		hostapd_notify_bad_chans(iface);
		return -3;
	}
}


const char * hostapd_hw_mode_txt(int mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211A:
		return "IEEE 802.11a";
	case HOSTAPD_MODE_IEEE80211B:
		return "IEEE 802.11b";
	case HOSTAPD_MODE_IEEE80211G:
		return "IEEE 802.11g";
	case HOSTAPD_MODE_IEEE80211AD:
		return "IEEE 802.11ad";
	default:
		return "UNKNOWN";
	}
}


int hostapd_hw_get_freq(struct hostapd_data *hapd, int chan)
{
	return hw_get_freq(hapd->iface->current_mode, chan);
}


int hostapd_hw_get_channel(struct hostapd_data *hapd, int freq)
{
	int i, channel;
	struct hostapd_hw_modes *mode;

	if (hapd->iface->current_mode) {
		channel = hw_get_chan(hapd->iface->current_mode->mode, freq,
				      hapd->iface->hw_features,
				      hapd->iface->num_hw_features);
		if (channel)
			return channel;
	}

	/* Check other available modes since the channel list for the current
	 * mode did not include the specified frequency. */
	if (!hapd->iface->hw_features)
		return 0;
	for (i = 0; i < hapd->iface->num_hw_features; i++) {
		mode = &hapd->iface->hw_features[i];
		channel = hw_get_chan(mode->mode, freq,
				      hapd->iface->hw_features,
				      hapd->iface->num_hw_features);
		if (channel)
			return channel;
	}
	return 0;
}


int hostapd_hw_skip_mode(struct hostapd_iface *iface,
			 struct hostapd_hw_modes *mode)
{
	int i;

	if (iface->current_mode)
		return mode != iface->current_mode;
	if (mode->mode != HOSTAPD_MODE_IEEE80211B)
		return 0;
	for (i = 0; i < iface->num_hw_features; i++) {
		if (iface->hw_features[i].mode == HOSTAPD_MODE_IEEE80211G)
			return 1;
	}
	return 0;
}


void hostapd_free_multi_hw_info(struct hostapd_multi_hw_info *multi_hw_info)
{
	os_free(multi_hw_info);
}


int hostapd_set_current_hw_info(struct hostapd_iface *iface, int oper_freq)
{
	struct hostapd_multi_hw_info *hw_info;
	unsigned int i;

	if (!iface->num_multi_hws)
		return 0;

	for (i = 0; i < iface->num_multi_hws; i++) {
		hw_info = &iface->multi_hw_info[i];

		if (hw_info->start_freq <= oper_freq &&
		    hw_info->end_freq >= oper_freq) {
			iface->current_hw_info = hw_info;
			wpa_printf(MSG_DEBUG,
				   "Mode: Selected underlying hardware: hw_idx=%u",
				   iface->current_hw_info->hw_idx);
			return 0;
		}
	}

	return -1;
}


struct hostapd_multi_hw_info *hostapd_get_current_hw_info(struct hostapd_iface *iface,
							  int oper_freq)
{
	struct hostapd_multi_hw_info *hw_info;
	unsigned int i;

	if (!iface->num_multi_hws)
		return NULL;

	for (i = 0; i < iface->num_multi_hws; i++) {
		hw_info = &iface->multi_hw_info[i];

		if (hw_info->start_freq <= oper_freq &&
		    hw_info->end_freq >= oper_freq) {
			return hw_info;
		}
	}

	return NULL;
}
