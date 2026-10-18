// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "esp.h"
#include "utils/os.h"
#include "common/ieee802_11_defs.h"
#include "ap/ap_config.h"
#include "cmn.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/ap_drv_ops.h"
#include "ap/hw_features.h"
#include "ap/acs.h"
#include "ap/dfs.h"
#include "block_channel.h"
#include "utils/eloop.h"
#include "cmn.h"
#include "acs_extn.h"

int hostapd_get_center_chan_extn(struct hostapd_iface *iface,
				 struct hostapd_channel_data *chan,
				 enum oper_chan_width oper_bw)
{
	int center = 0;

	switch (oper_bw) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (chan->extn.secondary_channel &&
		    chan->freq >= 2400 && chan->freq < 2500)
			center = chan->chan +
				2 * chan->extn.secondary_channel;
		else if (chan->extn.secondary_channel)
			center = acs_get_bw_center_chan(chan->freq, ACS_BW40);
		else
			center = chan->chan;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		center = acs_get_bw_center_chan(chan->freq, ACS_BW80);
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		center = acs_get_bw_center_chan(chan->freq, ACS_BW160);
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		switch (chan->extn.eht_bw320_offset) {
		case 0:
			if (acs_usable_bw_chan(chan, ACS_BW320_1))
				center = acs_get_bw_center_chan(chan->freq, ACS_BW320_1);
			else if (acs_usable_bw_chan(chan, ACS_BW320_2))
				center = acs_get_bw_center_chan(chan->freq, ACS_BW320_2);
			break;
		case 1:
			center = acs_get_bw_center_chan(chan->freq,
							ACS_BW320_1);
			break;
		case 2:
			center = acs_get_bw_center_chan(chan->freq,
							ACS_BW320_2);
			break;
		default:
			wpa_printf(MSG_INFO,
				   "ACS: BW320 offset is not selected");
			return -1;
		}

		break;
	default:
		wpa_printf(MSG_INFO,
			   "ACS: Only VHT20/40/80/160/320 is supported now");
		return -1;
	}

	return center;
}

static struct hostapd_channel_data *
hostapd_get_chan_from_freq_extn(struct hostapd_iface *iface, int freq)
{
	if (!iface || !iface->current_mode || freq <= 0)
		return NULL;

	return get_chan_data_by_freq(iface->current_mode, freq);
}

int hostapd_set_channel_penalty_extn(struct hostapd_iface *iface,
				     int freq,
				     int penalty_percent)
{
	struct hostapd_channel_data *chan;

	if (penalty_percent < 0 || penalty_percent > 100) {
		wpa_printf(MSG_ERROR, "%s: Invalid penalty %d (expected 0..100)",
			   __func__, penalty_percent);
		return -1;
	}

	chan = hostapd_get_chan_from_freq_extn(iface, freq);
	if (!chan) {
		wpa_printf(MSG_ERROR, "%s: Channel data not found for freq %d",
			   __func__, freq);
		return -1;
	}

	chan->extn.acs_channel_penalty = (u8) penalty_percent;

	wpa_printf(MSG_DEBUG,
		   "%s: Set penalty %u%% for channel %d (freq=%d)",
		   __func__, (u8) penalty_percent, chan->chan, chan->freq);
	return 0;
}

int hostapd_get_dcs_channel_penalty_extn(struct hostapd_iface *iface,
					 struct hostapd_channel_data *chan)
{
	int penalty_percent;

	if (!chan) {
		wpa_printf(MSG_ERROR, "%s: Channel not found", __func__);
		return 0;
	}

	penalty_percent = chan->extn.acs_channel_penalty;

	if (penalty_percent < 0 || penalty_percent > 100) {
		wpa_printf(MSG_ERROR, "%s: Invalid penalty %d (expected 0..100)",
			   __func__, penalty_percent);
		if (penalty_percent > 100)
			chan->extn.acs_channel_penalty = 100;
		else if(penalty_percent < 0)
			chan->extn.acs_channel_penalty = 0;
	}

	return chan->extn.acs_channel_penalty;
}

static void
hostapd_get_center_chanfreq1_from_channel(struct hostapd_iface *iface,
					   struct hostapd_channel_data *chan,
					   enum oper_chan_width oper_bw,
					   int *center_chan1,
					   int *center_freq1)
{
	int center_chan = 0, center_freq = 0;
	u8 op_class = 0, channel = 0;
	enum hostapd_hw_mode hw_mode;

	center_chan = hostapd_get_center_chan_extn(iface, chan, oper_bw);
	if (center_chan == -1)
		goto fail;

	hw_mode = ieee80211_freq_to_channel_ext(chan->freq,
						chan->extn.secondary_channel,
						oper_bw,
						&op_class, &channel);
	if (hw_mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_ERROR, "Failed to get channel for freq: %d, sec_channel_offset: %d, bw: %d",
			   chan->freq, chan->extn.secondary_channel, oper_bw);
		goto fail;
	}

	center_freq = ieee80211_chan_to_freq(NULL, op_class, center_chan);

	/* Calculate center freq for 5G 240MHz, as it doesnot have a valid op_class */
	if (center_freq <= 0 && is_5ghz_freq(chan->freq)
	    && oper_bw == CONF_OPER_CHWIDTH_320MHZ)
		center_freq = 5000 + center_chan * 5;

fail:
	wpa_printf(MSG_DEBUG, "%s: ACS: center_chan1: %d, center_freq1: %d, oper_bw %d",
		   __func__,
		   center_chan,
		   center_freq, oper_bw);

	if (center_chan1)
		*center_chan1 = center_chan;

	if (center_freq1)
		*center_freq1 = center_freq;
}


static int
acs_print_usage_extn(char *reply, int reply_size)
{
	int ret;

	ret = os_snprintf(
		reply, reply_size,
		"acs extn commands:\n"
		"  acs get_status           : get ACS current status\n"
		"  acs rank_en <1|0>        : enable/disable channel ranking\n"
		"  acs get_rank_en          : get channel ranking enable state\n"
		"  acs pri_sec20_penalty <0-100> : set PRI_SEC20 penalty\n"
		"  acs get_pri_sec20_penalty : get PRI_SEC20 penalty\n"
		"  acs qacs_enable <1|0>    : enable/disable QACS extension\n"
		"  acs get_qacs_enable      : get QACS extension enable state\n"
		"  acs noscan <1|0>         : enable/disable noscan mode\n"
		"  acs get_noscan           : get noscan mode state\n"
		"  acs dfs_exclude <1|0>    : enable/disable DFS channel exclusion\n"
		"  acs get_dfs_exclude      : get DFS channel exclusion state\n"
		"  acs dwelltime <ms>       : set dwell time in milliseconds\n"
		"  acs get_dwell            : get dwell time in milliseconds\n"
		"  acs dcs_channel_penalty <0-100> : set ACS channel penalty percentage\n"
		"  acs get_dcs_channel_penalty : get ACS channel penalty percentage\n"
		"  acs dbgtrace <value>     : set debug (0x00FF=level, 0xFF00=module mask)\n"
		"  acs get_dbgtrace         : get debug mask\n"
		"  acs wradar <0|1>         : enable/disable excluding weather radar channels\n"
		"  acs get_wradar           : get weather radar handling state\n"
		"  acs txpwr_opt <0|1|2>    : set the tx pwr optimization state(0 = disable, 1 = optimize throughput, 2 = optimize range)\n"
		"  acs get_txpwr_opt        : get tx power optimization state\n"
		"  acs 6g_only_psc <1|0>    : restrict 6 GHz to PSC channels only\n"
		"  acs get_6g_only_psc      : get the state of restricting 6 GHz to PSC channels only\n"
		"  acs invoke <0|1>         : invoke ACS (0=dynamicACS+CSA)|(1=DynamicACS)\n"
		"  acs show_report          : print last ACS report\n"
		"  acs set_block_chan_list  : set the channels to blocked state\n"
		"  acs get_block_chan_list  : get the blocked channels\n"
		"  acs clear_block_chan_list: clear the blocked channels \n"
		"  acs show_neighbor_report : print ACS neighbor report\n"
		"  acs set_nf_bond_penalty <0-100> : set NF penalty %% for bonded channel (default 75)\n"
		"  acs get_nf_bond_penalty          : get NF bond penalty percentage\n"
		"  acs periodic_interval <sec> : set periodic ACS interval (0 disables, max 86400)\n"
		"  acs get_periodic_interval : get periodic ACS interval\n"
		"  acs 2g_scan_all <1|0>    : 2.4 GHz ACS: 1=all channels, 0=only 1/6/11\n"
		"  acs get_2g_scan_all      : get 2.4 GHz ACS channel set selection\n"
		);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

#ifdef CONFIG_QCN_APP_EXTN

static enum trigger_type acs_get_valid_trigger_type(int trigger_type)
{
	if (trigger_type >= 0 && trigger_type < trigger_type_max)
		return (enum trigger_type) trigger_type;

	return normal_scan;
}

void acs_fill_timestamp(struct hostapd_iface *iface,
			int trigger_type, bool is_trigger_time)
{
	struct qacs_data_extn *data_extn;
	struct os_time now = { 0 };
	enum trigger_type trigger = acs_get_valid_trigger_type(trigger_type);

	if (!iface || !iface->current_mode)
		return;

	data_extn = ICM_GET_EXTN_DATA_PTR(iface->current_mode);
	if (!data_extn)
		return;

	if (!os_get_time(&now)) {
		if (is_trigger_time) {
			data_extn->chan_sel_trigger_time[trigger].tv_sec = now.sec;
			data_extn->chan_sel_trigger_time[trigger].tv_nsec =
				now.usec * 1000;
		} else {
			data_extn->chan_sel_completion_time[trigger].tv_sec = now.sec;
			data_extn->chan_sel_completion_time[trigger].tv_nsec =
				now.usec * 1000;
		}

	} else {
		if (is_trigger_time) {
			data_extn->chan_sel_trigger_time[trigger].tv_sec = 0;
			data_extn->chan_sel_trigger_time[trigger].tv_nsec = 0;
		} else {
			data_extn->chan_sel_completion_time[trigger].tv_sec = 0;
			data_extn->chan_sel_completion_time[trigger].tv_nsec = 0;
		}
	}

	/* Track the latest trigger path for both start and completion updates. */
	data_extn->trigger_type = trigger;
}

static void acs_fill_algo_type(struct hostapd_iface *iface)
{
	struct qacs_data_extn *data_extn;
	enum acs_type acs_algo = iface->conf->conf_extn.qacs_enable ?
				 qacs : standard_acs;

	if (!iface || !iface->current_mode)
		return;

	data_extn = ICM_GET_EXTN_DATA_PTR(iface->current_mode);
	if (!data_extn)
		return;

	data_extn->acs_algo = acs_algo;
}

static int print_acs_report_to_buf(const struct qacs_dbg_info_per_band *report,
				   struct hostapd_iface *iface, int nchans,
				   struct qacs_data_extn *data_extn,
				   char *reply, size_t reply_size)
{
	int ret;
	char *pos = reply;
	char *end = reply + reply_size;
	unsigned int best_center_freq = 0;

	/* ---- Single header ---- */
	ret = os_snprintf(pos, end - pos,
			" Freq(chan)          BSS    NF   Load  Sec   SRP  Grade  Radar    Eff   Power   Rank  MaxRSSI  MinRSSI\n");
	if (os_snprintf_error(end - pos, ret)) {
		return (int)(pos - reply);
	}
	pos += ret;

	ret = os_snprintf(pos, end - pos,
			"----------------------------------------------------------------------------------------------------------\n");
	if (os_snprintf_error(end - pos, ret)) {
		return (int)(pos - reply);
	}
	pos += ret;

	/* ---- Rows ---- */
	for (int i = 0; i < nchans; i++) {
		const struct qacs_dbg_info_per_band *r = &report[i];

		/* Skip uninitialized entries */
		              if (!r->chan_freq)
		                      continue;

		const int has_plus  = (r->center_freq1 != 0);
		const int has_minus = (r->center_freq2 != 0);

		if (has_plus || has_minus) {

			if (has_plus) {
				ret = os_snprintf(pos, end - pos,
						" %4u(%3u %4u)  %6u %5d %6u %4u %5d %6u %6u %7d %6d %5d  %7d  %7d\n",
						r->chan_freq, r->ieee_chan, r->center_freq1,
						r->chan_nbss, r->noisefloor, r->chan_load,
						r->sec_chan, r->chan_nbss_srp, r->chan_grade,
						(unsigned) r->chan_radar_noise,
						r->chan_efficiency_1, r->txpower, r->rank_1,
						r->max_rssi, r->min_rssi);

				if (os_snprintf_error(end - pos, ret))
					return (int)(pos - reply);
				pos += ret;
			}

			/* Row 2 : center_freq2 → seg1 / HT40− */
			if (has_minus) {
				ret = os_snprintf(pos, end - pos,
						" %4u(%3u %4u)  %6u %5d %6u %4u %5d %6u %6u %7d %6d %5d  %7d  %7d\n",
						r->chan_freq, r->ieee_chan, r->center_freq2,
						r->chan_nbss, r->noisefloor, r->chan_load,
						r->sec_chan, r->chan_nbss_srp, r->chan_grade,
						(unsigned) r->chan_radar_noise,
						r->chan_efficiency, r->txpower, r->rank,
						r->max_rssi, r->min_rssi);

				if (os_snprintf_error(end - pos, ret))
					return (int)(pos - reply);
				pos += ret;
			}

			continue;
		}

		else {
			ret = os_snprintf(pos, end - pos,
					" %4u(%3u)       %6u %5d %6u %4u %5d %6u %6u %7d %6d %5d  %7d  %7d\n",
					r->chan_freq, r->ieee_chan,
					r->chan_nbss, r->noisefloor, r->chan_load,
					r->sec_chan, r->chan_nbss_srp, r->chan_grade,
					(unsigned)r->chan_radar_noise, r->chan_efficiency, r->txpower, r->rank,
					r->max_rssi, r->min_rssi);
			if (os_snprintf_error(end - pos, ret))
				return (int)(pos - reply);
			pos += ret;

			continue;
		}

		if (!best_center_freq && r->ieee_chan == data_extn->best_chan) {
			bool is_2g = (r->chan_freq && r->chan_freq < 3000);
			bool is_6g = (r->chan_freq && r->chan_freq >= 5925);

			if (data_extn->bw == 40 && is_2g) {
				if (iface->conf->secondary_channel == 1 && r->center_freq1)
					best_center_freq = r->center_freq1;
				else if (iface->conf->secondary_channel == -1 && r->center_freq2)
					best_center_freq = r->center_freq2;
				else
					best_center_freq = r->center_freq1 ? r->center_freq1 :
						r->center_freq2 ? r->center_freq2 : 0;
			} else if (data_extn->bw == 320 && is_6g) {
				if (iface->conf->eht_bw320_offset == 1 && r->center_freq1)
					best_center_freq = r->center_freq1;
				else if (iface->conf->eht_bw320_offset == 2 && r->center_freq2)
					best_center_freq = r->center_freq2;
				else
					best_center_freq = r->center_freq1 ? r->center_freq1 :
						r->center_freq2 ? r->center_freq2 : 0;
			}
		}
	}

	if (data_extn->is_fallback_chan) {
		ret = os_snprintf(pos, end - pos,
				"ACS_SUCCESS: Current channel is selected by Random channel algorithm\n");
		if (os_snprintf_error(end - pos, ret)) {
			return (int)(pos - reply);
		}
		pos += ret;
	}
	else {
		ret = os_snprintf(pos, end - pos,
				"ACS_SUCCESS: Current channel is selected by ACS algorithm\n");
		if (os_snprintf_error(end - pos, ret)) {
			return (int)(pos - reply);
		}
		pos += ret;
	}

	if (best_center_freq) {
		ret = os_snprintf(pos, end - pos,
				"Best channel %d selected for Bandwidth %d (center_freq=%u)\n",
				data_extn->best_chan, data_extn->bw, best_center_freq);
	} else {
		ret = os_snprintf(pos, end - pos,
				"Best channel %d selected for Bandwidth %d\n",
				data_extn->best_chan, data_extn->bw);
	}

	pos += ret;

	static const char * const acs_algo_str[] = {
		[0] = "Standard ACS",
		[qacs] = "QACS",
	};

	static const char * const trigger_type_str[] = {
		[0] = "Normal scan",
		[cbs_vendor_scan] = "CBS Vendor scan",
	};
	const char *acs_algo = "Unknown ACS algo";
	const char *trigger = "Unknown scan trigger";
	enum trigger_type trig =
		acs_get_valid_trigger_type(data_extn->trigger_type);
	const struct timespec *trigger_time =
		&data_extn->chan_sel_trigger_time[trig];
	const struct timespec *completion_time =
		&data_extn->chan_sel_completion_time[trig];

	if (data_extn->acs_algo < ARRAY_SIZE(acs_algo_str) &&
	    acs_algo_str[data_extn->acs_algo])
		acs_algo = acs_algo_str[data_extn->acs_algo];

	if (trig < ARRAY_SIZE(trigger_type_str) && trigger_type_str[trig])
		trigger = trigger_type_str[trig];

	ret = os_snprintf(pos, end - pos,
			  "ACS Algo: %s  Trigger: %s\n",
			  acs_algo, trigger);

	if (os_snprintf_error(end - pos, ret))
		return (int)(pos - reply);
	pos += ret;

	{
		long diff_sec = (long)(completion_time->tv_sec -
				       trigger_time->tv_sec);
		long diff_usec = (long)((completion_time->tv_nsec -
					 trigger_time->tv_nsec) / 1000);

		if (diff_usec < 0) {
			diff_sec--;
			diff_usec += 1000000L;
		}

		ret = os_snprintf(pos, end - pos,
				  "Chan sel trigger time: %ld.%06ld  completion time: %ld.%06ld\n"
				  "Total Time taken for channel selection (sec): %ld.%06ld\n",
				  (long)trigger_time->tv_sec,
				  (long)(trigger_time->tv_nsec / 1000),
				  (long)completion_time->tv_sec,
				  (long)(completion_time->tv_nsec / 1000),
				  diff_sec, diff_usec);
	}

	if (os_snprintf_error(end - pos, ret))
		return (int)(pos - reply);
	pos += ret;

	return (int)(pos - reply);
}

static int hostapd_acs_show_report_extn(struct hostapd_data *hapd,
		const char *pos,
		char *reply, size_t reply_size)
{
	int len = 0;
	struct hostapd_iface *iface = hapd->iface;

	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	struct qacs_data_extn *data_extn = ICM_GET_EXTN_DATA_PTR(mode);
	if (!mode) {
		wpa_printf(MSG_ERROR,
				"No current mode selected (interface not initialized?)");
		return -1;
	}

	int nchans = mode->num_channels;
	struct qacs_dbg_info_per_band *acs_report =
		calloc(nchans, sizeof(*acs_report));
	if (!acs_report) {
		wpa_printf(MSG_ERROR, "Memory allocation failed");
		return -1;
	}

	/* Call the ICM/ACS API for the currently selected band/mode */
	int ret = qacs_scan_report(iface, mode, acs_report);
	if (ret <= 0) {
		wpa_printf(MSG_ERROR, "No ACS report available");
		free(acs_report);
		return -1;
	}

	/* Print the report to the reply buffer */
	len = print_acs_report_to_buf(acs_report, iface, nchans, data_extn ,reply, reply_size);

	free(acs_report);
	return len;
}

static int print_buf_neighbor_report(const struct qacs_neighbor_report *report,
				     u8 nentries,
				     char *reply, size_t reply_size)
{
	char *pos = reply;
	char *end = reply + reply_size;
	int ret;
	u8 i;

	ret = os_snprintf(pos, end - pos,
			  "Index | Band | Channel | NBSS | SSID             | BSSID             | SNR | PHYTYPE | CHANWIDTH | Power info\n"
			  "----------------------------------------------------------------------------------------------------------------\n");
	if (os_snprintf_error(end - pos, ret))
		return -1;
	pos += ret;

	for (i = 0; i < nentries; i++) {
		const struct qacs_neighbor_report *nr = &report[i];
		char ssid_buf[MAX_SSID_LEN + 1];
		char bssid_buf[18];
		const char *ssid;
		const char *band;
		const char *phytype = nr->phytype_disp ? nr->phytype_disp : "UNSPEC";
		const char *chanwidth = nr->chan_width_disp[0] ? nr->chan_width_disp : "UNSPEC";

		switch (nr->band) {
		case ICM_BAND_2_4G:
			band = "2.4G";
			break;
		case ICM_BAND_5G:
			band = "5G";
			break;
		case ICM_BAND_6G:
			band = "6G";
			break;
		default:
			band = "UNK";
			break;
		}

		os_memcpy(ssid_buf, nr->ssid, MAX_SSID_LEN);
		ssid_buf[MAX_SSID_LEN] = '\0';
		ssid = ssid_buf[0] ? ssid_buf : "<hidden>";

		os_snprintf(bssid_buf, sizeof(bssid_buf), "%02x:%02x:%02x:%02x:%02x:%02x",
			    nr->bssid[0], nr->bssid[1], nr->bssid[2],
			    nr->bssid[3], nr->bssid[4], nr->bssid[5]);

		ret = os_snprintf(pos, end - pos,
				  "%5u | %-4.4s | %7u | %4u | %-16.16s | %-17.17s | %3d | %-7.7s | %-9.9s | %10d\n",
				  (unsigned int) (i + 1), band,
				  (unsigned int) nr->chan, (unsigned int) nr->nbss,
				  ssid, bssid_buf, nr->snr, phytype, chanwidth,
				  (int) nr->power_info);
		if (os_snprintf_error(end - pos, ret))
			return -1;
		pos += ret;
	}

	if (nentries == 0)
		return os_snprintf(reply, reply_size, "No matching scan entries found\n");

	return (int) (pos - reply);
}

static int hostapd_acs_show_neighbor_report_extn(struct hostapd_data *hapd,
						 const char *pos,
						 char *reply, size_t reply_size)
{
	struct hostapd_hw_modes *mode;
	struct qacs_data_extn *data_extn;

	(void) pos;

	if (!hapd || !hapd->iface || !reply || !reply_size)
		return -1;

	mode = hapd->iface->current_mode;
	if (!mode)
		return os_snprintf(reply, reply_size,
				   "No current mode selected\n");

	data_extn = ICM_GET_EXTN_DATA_PTR(mode);
	if (!data_extn || !data_extn->neighbor_report ||
	    !data_extn->num_neighbor_entries)
		return os_snprintf(reply, reply_size,
				   "No neighbor report available\n");

	return print_buf_neighbor_report(data_extn->neighbor_report,
					 data_extn->num_neighbor_entries, reply, reply_size);
}

#else
static int hostapd_acs_show_report_extn(struct hostapd_data *hapd,
		const char *pos,
		char *reply, size_t reply_size)
{
	return -1;
}

static int hostapd_acs_show_neighbor_report_extn(struct hostapd_data *hapd,
						 const char *pos,
						 char *reply, size_t reply_size)
{
	return -1;
}

void acs_fill_timestamp(struct hostapd_iface *iface,
			int trigger_type, bool is_trigger_time)
{
}

static void acs_fill_algo_type(struct hostapd_iface *iface)
{
}
#endif

int hostapd_trigger_dynamic_acs(struct hostapd_data *hapd, enum dynamic_acs_action_extn acs_action)
{
	struct hostapd_iface *iface = hapd->iface;
	int status;

	if (!hapd->iface->current_mode)
		return -1;


	if (iface->iface_extn.dynamic_acs_action) {
		wpa_printf(MSG_ERROR, "Dynamic ACS is already in progress");
		return -1;
	}

	iface->iface_extn.dynamic_acs_action = acs_action;
	wpa_msg(hapd->msg_ctx, MSG_INFO, DYNAMIC_ACS_EVENT_STARTED);
	qacs_reset_scan_stats(iface, hapd->iface->current_mode);
	/* Notify wpa_supplicant to abort scans before starting ACS */
	if (!hostapd_is_bh_sta_connecting_or_connected_extn(iface))
		hostapd_ucode_notify_acs_start(iface);

	status = acs_init(iface);
	if (status != HOSTAPD_CHAN_ACS) {
		wpa_printf(MSG_ERROR, "Could not start ACS, error: %d", status);
		iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;
		/* Notify wpa_supplicant to resume scans on failure */
		hostapd_ml_acs_check_and_notify(iface, 0);
		return -1;
	}

	return 0;
}

static void hostapd_periodic_acs_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;

	iface->iface_extn.periodic_acs_timer_set = false;

	if (!iface->conf->conf_extn.acs_periodic_interval)
		return;

	if (iface->state != HAPD_IFACE_ENABLED) {
		eloop_register_timeout(iface->conf->conf_extn.acs_periodic_interval, 0,
				       hostapd_periodic_acs_timeout, iface, NULL);
		iface->iface_extn.periodic_acs_timer_set = true;
		return;
	}

	wpa_printf(MSG_INFO, "Periodic ACS: triggering ACS (interval=%u sec)",
		   iface->conf->conf_extn.acs_periodic_interval);

	if (hostapd_trigger_dynamic_acs(iface->bss[0], CHANNEL_CHANGE_CSA) < 0) {
		wpa_printf(MSG_ERROR, "Periodic ACS: trigger failed");
		hostapd_periodic_acs_schedule(iface);
	}
}

void hostapd_periodic_acs_schedule(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return;

	if (!iface->conf->conf_extn.acs_periodic_interval)
		return;

	eloop_cancel_timeout(hostapd_periodic_acs_timeout, iface, NULL);
	iface->iface_extn.periodic_acs_timer_set = false;

	eloop_register_timeout(iface->conf->conf_extn.acs_periodic_interval, 0,
			       hostapd_periodic_acs_timeout, iface, NULL);
	iface->iface_extn.periodic_acs_timer_set = true;
}

void hostapd_periodic_acs_start(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return;

	eloop_cancel_timeout(hostapd_periodic_acs_timeout, iface, NULL);
	iface->iface_extn.periodic_acs_timer_set = false;

	if (!iface->conf->conf_extn.acs_periodic_interval)
		return;

	hostapd_periodic_acs_schedule(iface);
}

void hostapd_periodic_acs_stop(struct hostapd_iface *iface)
{
	if (!iface)
		return;

	eloop_cancel_timeout(hostapd_periodic_acs_timeout, iface, NULL);
	iface->iface_extn.periodic_acs_timer_set = false;
}

bool acs_scan_event_expected_extn(struct hostapd_iface *iface)
{
	bool expected;

	expected = ((iface->state == HAPD_IFACE_ACS) ||
		    (iface->iface_extn.dynamic_acs_action &&
		     iface->state == HAPD_IFACE_ENABLED));

	if (expected) {
#if !defined(WPA_TRACE_BFD) || !defined(CONFIG_TESTING_OPTIONS)
		os_get_reltime(&iface->conf->conf_extn.cbs_params.acs_scan_complete_ts);
		wpa_printf(MSG_DEBUG,
			   "Scan complete ts is recorded for ACS: %ld.%06ld (source=acs_scan_event_expected_extn)",
			   iface->conf->conf_extn.cbs_params.acs_scan_complete_ts.sec,
			   iface->conf->conf_extn.cbs_params.acs_scan_complete_ts.usec);
#else
		wpa_printf(MSG_DEBUG,
			   "Skipping ACS scan complete timestamp update in testing build");
#endif
	}

	return expected;
}

static int hostapd_acs_run_extn(struct hostapd_data *hapd, const char *pos,
                                char *reply, size_t reply_size)
{
        int acs_run_op;

        acs_run_op = atoi(pos);

        if (acs_run_op != 0 && acs_run_op != 1)
                return -1;

        return hostapd_trigger_dynamic_acs(hapd, (acs_run_op == 1 ? NO_CHANNEL_CHANGE : CHANNEL_CHANGE_CSA));
}

static int hostapd_acs_get_status_extn(struct hostapd_iface *iface,
		const char *pos,
		char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS status: %s\n",
			      (iface->iface_extn.dynamic_acs_action != DYNAMIC_ACS_DISABLE
			       || iface->state == HAPD_IFACE_ACS) ?
			      "Inprogress" : "Idle");

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_rank_en_extn(struct hostapd_config_extn *conf_extn,
					const char *pos,
					char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf_extn->qacs_conf.rank_en = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_rank_en_extn(struct hostapd_config_extn *conf_extn,
					const char *pos,
					char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS rank_en: %d\n",
			      conf_extn->qacs_conf.rank_en);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int
hostapd_acs_set_nf_bond_penalty_extn(struct hostapd_config_extn *conf_extn,
				     const char *pos,
				     char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0 && val <= 100) {
		conf_extn->qacs_conf.nf_bond_penalty_pct = (u8)val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value (0-100)", __func__);
	return -1;
}

static int
hostapd_acs_get_nf_bond_penalty_extn(struct hostapd_config_extn *conf_extn,
				     const char *pos,
				     char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS nf_bond_penalty_pct: %u\n",
			      conf_extn->qacs_conf.nf_bond_penalty_pct);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_pri_sec20_penalty_extn(
		struct hostapd_config_extn *conf_extn,
		const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0 && val <= 100) {
		conf_extn->qacs_conf.acs_pri_sec20_penalty = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_pri_sec20_penalty_extn(
		struct hostapd_config_extn *conf_extn,
		const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS pri_sec20_penalty: %u\n",
			      conf_extn->qacs_conf.acs_pri_sec20_penalty);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_qacs_enable_extn(struct hostapd_config_extn *conf_extn,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf_extn->qacs_enable = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_qacs_enable_extn(struct hostapd_config_extn *conf_extn,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS qacs_enable: %d\n",
			      conf_extn->qacs_enable);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_noscan_extn(struct hostapd_config *conf,
				       const char *pos,
				       char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf->noscan = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_noscan_extn(struct hostapd_config *conf,
				       const char *pos,
				       char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS noscan: %d\n", conf->noscan);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_dfs_exclude_extn(struct hostapd_config *conf,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf->acs_exclude_dfs = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_dfs_exclude_extn(struct hostapd_config *conf,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS dfs_exclude: %d\n",
			      conf->acs_exclude_dfs);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_periodic_interval_extn(struct hostapd_iface *iface,
						  const char *pos,
						  char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val != 0 && (val < 60 || val > 86400)) {
		wpa_printf(MSG_ERROR,
			   "%s: Invalid acs_periodic_interval %d (expected 0 or 60..86400)",
			   __func__, val);
		return -1;
	}

	iface->conf->conf_extn.acs_periodic_interval = val;
	if (val)
		hostapd_periodic_acs_start(iface);
	else
		hostapd_periodic_acs_stop(iface);

	return 0;
}

static int hostapd_acs_get_periodic_interval_extn(struct hostapd_config_extn *conf_extn,
						  const char *pos,
						  char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS periodic_interval: %u\n",
			      conf_extn->acs_periodic_interval);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_2g_scan_all_extn(struct hostapd_config_extn *conf_extn,
                                            const char *pos,
                                            char *reply, size_t reply_size)
{
    int val = atoi(pos);
    conf_extn->qacs_conf.acs_2g_scan_all = (val != 0);
    return 0;
}

static int hostapd_acs_get_2g_scan_all_extn(struct hostapd_config_extn *conf_extn,
                                            const char *pos,
                                            char *reply, size_t reply_size)
{
    int ret = os_snprintf(reply, reply_size,
                          "ACS 2g_scan_all: %d\n",
                          conf_extn->qacs_conf.acs_2g_scan_all ? 1 : 0);
    if (os_snprintf_error(reply_size, ret))
        return -1;
    return ret;
}

static int hostapd_acs_set_dwelltime_extn(struct hostapd_config_extn *conf_extn,
					  const char *pos,
					  char *reply, size_t reply_size)
{
	u16 acs_dwell = atoi(pos);

	if (acs_dwell <= conf_extn->qacs_conf.max_dwell &&
	    acs_dwell >= conf_extn->qacs_conf.min_dwell) {
		conf_extn->qacs_conf.dwelltime = acs_dwell;
		return 0;
	}

	wpa_printf(MSG_ERROR, "Dwell time must be between %d milliseconds and %d milliseconds",
		   conf_extn->qacs_conf.min_dwell,
		   conf_extn->qacs_conf.max_dwell);
	return -1;
}

static int hostapd_acs_get_dwell_extn(struct hostapd_config_extn *conf_extn,
				      const char *pos,
				      char *reply, size_t reply_size)
{

	int ret = os_snprintf(reply, reply_size,
			      "ACS min_dwelltime: %d\n"
			      "ACS max_dwelltime: %d\n"
			      "ACS dwelltime: %d\n",
			      conf_extn->qacs_conf.min_dwell,
			      conf_extn->qacs_conf.max_dwell,
			      conf_extn->qacs_conf.dwelltime);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_dcs_channel_penalty_extn(
					struct hostapd_config_extn *conf_extn,
					const char *pos,
					char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val < 0 || val > 100) {
		wpa_printf(MSG_ERROR, "%s: Invalid value %d (expected 0..100)",
			   __func__, val);
		return -1;
	}

	conf_extn->qacs_conf.dcs_channel_penalty = val;
	return 0;
}

static int hostapd_acs_get_dcs_channel_penalty_extn(
					struct hostapd_config_extn *conf_extn,
					const char *pos,
					char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS dcs_channel_penalty: %u\n",
			      conf_extn->qacs_conf.dcs_channel_penalty);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_dbgtrace_extn(struct hostapd_config_extn *conf_extn,
					 const char *pos,
					 char *reply, size_t reply_size)
{
	/* Expected format: "<value>"
	 * Lower 0x00FF bits -> debug level
	 * Upper 0xFF00 bits -> module bitmap
	 * Example: "0x0201" means module_bitmap=0x02, dbg_level=0x01
	 */
	char *endptr;
	unsigned long val;

	if (!conf_extn || !pos)
		return -1;

	/* Parse combined value (hex or decimal) */
	val = strtoul(pos, &endptr, 0);
	if (endptr == pos)
		goto invalid;

	/* Extract fields */
	conf_extn->qacs_conf.dbg_module_bitmap = (u_int16_t)((val & 0xFF00) >> 8);
	conf_extn->qacs_conf.dbg_level = (int)(val & 0x00FF);

	return 0;

invalid:
	wpa_printf(MSG_ERROR, "%s: Invalid value. Usage: acs dbgtrace <value> (0xFF00=module mask, 0x00FF=debug level)", __func__);
	return -1;
}

static int hostapd_acs_get_dbgtrace_extn(struct hostapd_config_extn *conf_extn,
					 const char *pos,
					 char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS Debug Trace: module_bitmap=0x%04x, debug_level=%d\n",
			      conf_extn->qacs_conf.dbg_module_bitmap,
			      conf_extn->qacs_conf.dbg_level);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_wradar_extn(struct hostapd_config_extn *conf_extn,
				       const char *pos,
				       char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf_extn->qacs_conf.wradar = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_wradar_extn(struct hostapd_config_extn *conf_extn,
				       const char *pos,
				       char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS Weather Radar: %d\n",
			      conf_extn->qacs_conf.wradar);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_txpwr_opt_extn(struct hostapd_config_extn *conf_extn,
					  const char *pos,
					  char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1 || val == 2) {
		conf_extn->qacs_conf.rep_txpower_policy = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value: %d", __func__, val);
	return -1;
}

static int hostapd_acs_get_txpwr_opt_extn(struct hostapd_config_extn *conf_extn,
					  const char *pos,
					  char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS Tx power policy: %d\n",
			      conf_extn->qacs_conf.rep_txpower_policy);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_acs_set_6g_only_psc_extn(struct hostapd_config *conf,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val == 0 || val == 1) {
		conf->acs_exclude_6ghz_non_psc = val;
		return 0;
	}

	wpa_printf(MSG_ERROR, "%s: Invalid value", __func__);
	return -1;
}

static int hostapd_acs_get_6g_only_psc_extn(struct hostapd_config *conf,
					    const char *pos,
					    char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "ACS get_6g_only_psc: %d\n",
			      conf->acs_exclude_6ghz_non_psc);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}


bool
hostapd_acs_is_chan_blocked(struct hostapd_iface *iface, u8 chan)
{
	struct hostapd_config_extn *ce;
	int i;

	if (!iface || !iface->conf)
		return false;

	ce = &iface->conf->conf_extn;

	for (i = 0; i < ce->block_chan_list.n_chan; i++) {
		if (ce->block_chan_list.chans[i] == chan)
			return true;
	}

	return false;
}

static int hostapd_acs_set_block_chanlist(struct hostapd_data *hapd,
		const char *cmd)
{
	return hostapd_set_block_chanlist(hapd->iface, cmd);
}

static int hostapd_acs_clear_block_chanlist(struct hostapd_data *hapd,
		const char *cmd)
{
	return hostapd_clear_block_chanlist(hapd->iface, cmd);
}

static int hostapd_acs_get_block_chanlist(struct hostapd_data *hapd,
		char *reply, size_t reply_size)
{
	return hostapd_get_block_chanlist(hapd->iface, reply, reply_size);
}

int hostapd_handle_cli_acs_extn(struct hostapd_data *hapd,
				char *pos, char *buf,
				size_t buflen)
{
	struct hostapd_config_extn *conf_extn;
	struct hostapd_config *conf;

	if (!hapd->iface || !hapd->iface->conf)
		return -1;

	conf = hapd->iface->conf;
	conf_extn  = &hapd->iface->conf->conf_extn;

	if (os_strncmp(pos, "get_status", 10) == 0) {
		return hostapd_acs_get_status_extn(hapd->iface, pos,
						   buf, buflen);

	} else if (os_strncmp(pos, "rank_en ", 8) == 0) {
		return hostapd_acs_set_rank_en_extn(conf_extn, pos + 8,
						    buf, buflen);

	} else if (os_strncmp(pos, "get_rank_en", 11) == 0) {
		return hostapd_acs_get_rank_en_extn(conf_extn, pos,
						    buf, buflen);

	} else if (os_strncmp(pos, "pri_sec20_penalty ", 18) == 0) {
		return hostapd_acs_set_pri_sec20_penalty_extn(conf_extn,
							      pos + 18, buf, buflen);

	} else if (os_strncmp(pos, "get_pri_sec20_penalty", 21) == 0) {
		return hostapd_acs_get_pri_sec20_penalty_extn(conf_extn, pos,
							      buf, buflen);

	} else if (os_strncmp(pos, "qacs_enable ", 12) == 0) {
		return hostapd_acs_set_qacs_enable_extn(conf_extn, pos + 12,
							buf, buflen);

	} else if (os_strncmp(pos, "get_qacs_enable", 16) == 0) {
		return hostapd_acs_get_qacs_enable_extn(conf_extn, pos,
							buf, buflen);

	} else if (os_strncmp(pos, "noscan ", 7) == 0) {
		return hostapd_acs_set_noscan_extn(conf, pos + 7, buf, buflen);

	} else if (os_strncmp(pos, "get_noscan", 10) == 0) {
		return hostapd_acs_get_noscan_extn(conf, pos, buf, buflen);

	} else if (os_strncmp(pos, "dfs_exclude ", 12) == 0) {
		return hostapd_acs_set_dfs_exclude_extn(conf, pos + 12,
							buf, buflen);

	} else if (os_strncmp(pos, "get_dfs_exclude", 15) == 0) {
		return hostapd_acs_get_dfs_exclude_extn(conf, pos,
							buf, buflen);

	} else if (os_strncmp(pos, "periodic_interval ", 18) == 0) {
		return hostapd_acs_set_periodic_interval_extn(hapd->iface,
							     pos + 18, buf, buflen);

	} else if (os_strncmp(pos, "get_periodic_interval", 21) == 0) {
		return hostapd_acs_get_periodic_interval_extn(conf_extn, pos,
							     buf, buflen);

	} else if (os_strncmp(pos, "dwelltime ", 10) == 0) {
		return hostapd_acs_set_dwelltime_extn(conf_extn, pos + 9,
						      buf, buflen);

	} else if (os_strncmp(pos, "get_dwell", 9) == 0) {
		return hostapd_acs_get_dwell_extn(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "dcs_channel_penalty ", 20) == 0) {
		return hostapd_acs_set_dcs_channel_penalty_extn(
							conf_extn, pos + 20,
							buf, buflen);

	} else if (os_strncmp(pos, "get_dcs_channel_penalty", 23) == 0) {
		return hostapd_acs_get_dcs_channel_penalty_extn(
							conf_extn, pos,
							buf, buflen);

	} else if (os_strncmp(pos, "dbgtrace ", 9) == 0) {
		return hostapd_acs_set_dbgtrace_extn(conf_extn, pos + 9,
						     buf, buflen);

	} else if (os_strncmp(pos, "get_dbgtrace", 12) == 0) {
		return hostapd_acs_get_dbgtrace_extn(conf_extn, pos,
						     buf, buflen);

	} else if (os_strncmp(pos, "wradar ", 7) == 0) {
		return hostapd_acs_set_wradar_extn(conf_extn, pos + 7,
						   buf, buflen);

	} else if (os_strncmp(pos, "get_wradar", 10) == 0) {
		return hostapd_acs_get_wradar_extn(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "txpwr_opt ", 10) == 0) {
		return hostapd_acs_set_txpwr_opt_extn(conf_extn, pos + 10,
						      buf, buflen);

	} else if (os_strncmp(pos, "get_txpwr_opt", 13) == 0) {
		return hostapd_acs_get_txpwr_opt_extn(conf_extn, pos,
						      buf, buflen);

	} else if (os_strncmp(pos, "6g_only_psc ", 12) == 0) {
		return hostapd_acs_set_6g_only_psc_extn(conf, pos + 12,
							buf, buflen);

	} else if (os_strncmp(pos, "get_6g_only_psc", 15) == 0) {
		return hostapd_acs_get_6g_only_psc_extn(conf, pos,
							buf, buflen);

	} else if (os_strncmp(pos, "2g_scan_all ", 12) == 0) {
		return hostapd_acs_set_2g_scan_all_extn(conf_extn, pos + 12,
							buf, buflen);

	} else if (os_strncmp(pos, "get_2g_scan_all", 15) == 0) {
		return hostapd_acs_get_2g_scan_all_extn(conf_extn, pos,
							buf, buflen);

	} else if (os_strncmp(pos, "invoke ", 7) == 0) {
		return hostapd_acs_run_extn(hapd, pos + 7, buf, buflen);

	} else if (os_strncmp(pos, "show_report", 11) == 0) {
		return hostapd_acs_show_report_extn(hapd, pos, buf, buflen);

	} else if (os_strncmp(pos, "set_block_chan_list", 19) == 0) {
		return hostapd_acs_set_block_chanlist(hapd, pos + 19);

	} else if (os_strncmp(pos, "clear_block_chan_list", 21) == 0) {
		return hostapd_acs_clear_block_chanlist(hapd, pos + 21);

	} else if (os_strcmp(pos, "get_block_chan_list") == 0) {
		return hostapd_acs_get_block_chanlist(hapd, buf, buflen);

	} else if (os_strncmp(pos, "set_nf_bond_penalty ", 20) == 0) {
		return hostapd_acs_set_nf_bond_penalty_extn(conf_extn, pos + 20,
							 buf, buflen);

	} else if (os_strncmp(pos, "get_nf_bond_penalty", 19) == 0) {
		return hostapd_acs_get_nf_bond_penalty_extn(conf_extn, pos,
							 buf, buflen);

	} else if (os_strncmp(pos, "show_neighbor_report", 20) == 0) {
		return hostapd_acs_show_neighbor_report_extn(hapd, pos, buf, buflen);

	} else {
		return acs_print_usage_extn(buf, buflen);
	}
}

void acs_request_scan_add_freqs_extn(struct hostapd_channel_data *chan,
				    int **freq)
{
	if (!(chan->flag & HOSTAPD_CHAN_DISABLED)) {
		**freq = chan->freq;
		(*freq)++;
	}
}

void acs_modify_scan_params_extn(struct hostapd_iface *iface,
				 struct wpa_driver_scan_params *params)
{
	if (!params)
		return;

	if (iface->conf->conf_extn.qacs_conf.dwelltime) {
		params->duration =
			iface->conf->conf_extn.qacs_conf.dwelltime;
		params->duration_mandatory = 1;
	}
}

int
hostapd_trigger_channel_switch_extn(struct hostapd_iface *iface,
				    struct hostapd_channel_data *chan)
{
	struct csa_settings settings;
	int i;
	int dfs_range = 0;
	int bandwidth;
	u8 chan_no;

	os_memset(&settings, 0, sizeof(settings));
	settings.cs_count = 10;

	settings.freq_params.sec_channel_offset = chan->extn.secondary_channel;
	settings.freq_params.freq = chan->freq;
	settings.freq_params.channel = chan->chan;

	switch (hostapd_get_oper_chwidth(iface->conf)) {
		case CONF_OPER_CHWIDTH_80MHZ:
			settings.freq_params.bandwidth = 80;
			break;
		case CONF_OPER_CHWIDTH_80P80MHZ:
			settings.freq_params.bandwidth = 160;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			settings.freq_params.bandwidth = 160;
			break;
		case CONF_OPER_CHWIDTH_320MHZ:
			settings.freq_params.bandwidth = 320;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
		default:
			settings.freq_params.bandwidth = chan->extn.secondary_channel ? 40 : 20;
			break;
	}

	/* Get the center_freq1 for the chan->freq and operating bw*/
	hostapd_get_center_chanfreq1_from_channel(iface, chan,
		hostapd_get_oper_chwidth(iface->conf),
		NULL,
		&settings.freq_params.center_freq1);

	settings.freq_params.ht_enabled = iface->conf->ieee80211n;
	settings.freq_params.vht_enabled = iface->conf->ieee80211ac;
	settings.freq_params.he_enabled = iface->conf->ieee80211ax;
	settings.freq_params.eht_enabled= iface->conf->ieee80211be;
	settings.freq_params.uhr_enabled = iface->conf->ieee80211bn;
	settings.freq_params.punct_bitmap = chan->punct_bitmap;
	settings.power_mode = -1;

	if (is_6ghz_freq(settings.freq_params.freq) &&
	    iface->conf->enable_best_power_mode) {
		int best_power_mode;

		best_power_mode =
			hostapd_get_best_ap_6ghz_power_mode(iface,
				settings.freq_params.freq,
				settings.freq_params.center_freq1,
				settings.freq_params.bandwidth,
				settings.freq_params.punct_bitmap);
		if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
			settings.power_mode = best_power_mode;
			wpa_printf(MSG_DEBUG, "%s: Best power mode for Freq %d is %d",
				   __func__,
				   settings.freq_params.freq,
				   settings.power_mode);
		} else {
			wpa_printf(MSG_DEBUG, "%s: Failed to get BPM for Freq %d, Setting to LPI mode",
				   __func__, settings.freq_params.freq);
			settings.power_mode = NL80211_REG_AP_LPI;
		}
	}

	/* Determine chan_width enumeration from bandwidth int */
	switch (settings.freq_params.bandwidth) {
		case 40:
			bandwidth = CHAN_WIDTH_40;
			break;
		case 80:
			bandwidth = settings.freq_params.center_freq2 ?
					CHAN_WIDTH_80P80 : CHAN_WIDTH_80;
			break;

		case 160:
			bandwidth = CHAN_WIDTH_160;
			break;
		case 320:
			bandwidth = CHAN_WIDTH_320;
			break;
		default:
			bandwidth = CHAN_WIDTH_20;
			break;
	}

	if (iface->iface_extn.dcs_in_progress &&
	    settings.freq_params.freq == iface->freq &&
	    bandwidth == iface->conf->conf_extn.cur_chan_params.chan_width &&
	    settings.freq_params.center_freq1 ==
		    iface->conf->conf_extn.cur_chan_params.cf1 &&
	    settings.freq_params.punct_bitmap == iface->conf->punct_bitmap) {
		hostapd_dcs_restore_extn(iface, "ACS selected current channel");
		return 0;
	}

#ifdef CONFIG_QCN_EXTN
	dfs_range += hostapd_find_dfs_range_extn(iface, bandwidth,
						 &settings.freq_params);
#else
	if (settings.freq_params.center_freq1)
		dfs_range += hostapd_is_dfs_overlap(
				iface, bandwidth, settings.freq_params.center_freq1);
	else
		dfs_range += hostapd_is_dfs_overlap(
				iface, bandwidth, settings.freq_params.freq);

	if (settings.freq_params.center_freq2)
		dfs_range += hostapd_is_dfs_overlap(
				iface, bandwidth, settings.freq_params.center_freq2);
#endif
	if (dfs_range) {
		if (ieee80211_freq_to_chan(settings.freq_params.freq, &chan_no) ==
			NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_ERROR,
				   "ACS: Failed to get channel for (freq=%d, sec_channel_offset=%d, bw=%d)",
				   settings.freq_params.freq,
				   settings.freq_params.sec_channel_offset,
				   settings.freq_params.bandwidth);
			return -1;
		}


	if (iface->conf->disable_csa_dfs == 1) {
		wpa_printf(MSG_DEBUG, "ACS: cancel radar handling timer for %s",
				iface->conf->bss[0]->iface);
		eloop_cancel_timeout(hostapd_dfs_radar_handling_timeout, iface, NULL);
	}

	settings.freq_params.channel = chan_no;
        wpa_printf(MSG_DEBUG,
                   "ACS DFS/CAC to (channel=%u, freq=%d, sec_channel_offset=%d, bw=%d, center_freq1=%d)",
                   settings.freq_params.channel,
                   settings.freq_params.freq,
                   settings.freq_params.sec_channel_offset,
                   settings.freq_params.bandwidth,
                   settings.freq_params.center_freq1);

	if(iface->cac_started || iface->bootup_cac_in_progress) {
		wpa_printf(MSG_DEBUG, "ACS: CAC in progress and ACS invoked - Aborting CAC");
		return hostapd_abort_cac_for_channel_switch(iface, &settings);
	}

#ifdef RDK_ONEWIFI
        /* If DFS_CHANNEL_SWITCH supported proceed with CSA */
        if (!(iface->drv_flags2 & WPA_DRIVER_FLAGS2_DFS_CHANNEL_SWITCH))
        {
            /* Perform CAC and switch channel via fallback */
            iface->is_ch_switch_dfs = true;
            hostapd_switch_channel_fallback(iface, &settings.freq_params);
            return 0;
        }
#else
	/* Perform CAC and switch channel via fallback */
        iface->is_ch_switch_dfs = true;
        hostapd_switch_channel_fallback(iface, &settings.freq_params);
        return 0;
#endif
    }

	if (iface->cac_started || iface->bootup_cac_in_progress) {
		return hostapd_abort_cac_for_channel_switch(iface, &settings);
	}

    if (iface->conf->disable_csa_dfs == 1) {
        wpa_printf(MSG_DEBUG, "ACS: cancel radar handling timer for %s",
                   iface->conf->bss[0]->iface);
        eloop_cancel_timeout(hostapd_dfs_radar_handling_timeout, iface, NULL);
    }

	for (i = 0; i < iface->num_bss; i++) {
		/* Save CHAN_SWITCH VHT and HE config */
		hostapd_chan_switch_config(iface->bss[i],
					   &settings.freq_params);

		wpa_printf(MSG_DEBUG,
			   "channel=%u, freq=%d, bw=%d, center_freq1=%d",
			   settings.freq_params.channel,
			   settings.freq_params.freq,
			   settings.freq_params.bandwidth,
			   settings.freq_params.center_freq1);

		if (hostapd_switch_channel(iface->bss[i], &settings))
			return -1;
	}

	return 0;
}

void acs_init_extn(struct hostapd_iface *iface,
		   uint8_t trigger)
{
	acs_fill_timestamp(iface, trigger, true);
	acs_fill_algo_type(iface);
}

int
acs_handle_channel_change_extn(struct hostapd_iface *iface,
			       struct hostapd_channel_data *chan,
			       int err)
{
	int cs_err;

	acs_fill_timestamp(iface, NORMAL_SCAN_TRIGGER, false);

	if (!chan || err)
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DYNAMIC_ACS_EVENT_FAILED);
	else
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DYNAMIC_ACS_EVENT_COMPLETED);

	if (iface->iface_extn.dynamic_acs_action == NO_CHANNEL_CHANGE) {
		iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;
		/* Notify wpa_supplicant to resume scans on success but no channel change requested */
		hostapd_ml_acs_check_and_notify(iface, 1);
		return 0;
	}

	if (iface->iface_extn.dynamic_acs_action == DYNAMIC_ACS_DISABLE)
		return -1;

	if (err) {
		wpa_printf(MSG_ERROR, "ACS failed with error: %d, channel change is not possible",
			   err);
		hostapd_dcs_restore_extn(iface, "ACS failed");
		iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;
		/* Notify wpa_supplicant to resume scans on failure */
		hostapd_ml_acs_check_and_notify(iface, 0);
		return 0;
	}

	/* Notify wpa_supplicant to resume scans on success and channel change requested */
	hostapd_ml_acs_check_and_notify(iface, 1);

	if (hostapd_is_bh_sta_connecting_or_connected_extn(iface)) {
		if (iface->conf->conf_extn.rptr_allow_chan_sw) {
			wpa_printf(MSG_DEBUG, "Rptr BH STA is connected, "
				   "disconnect BH STA and allow ACS channel switch");
			hostapd_ucode_trigger_bhsta_disconnect(iface);
		} else {
			wpa_printf(MSG_ERROR, "Rptr BH STA is connected, "
				   "discard ACS channel switch");
			iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;
			return 0;
		}
	}

	cs_err = hostapd_trigger_channel_switch_extn(iface, chan);
	if (cs_err) {
		wpa_printf(MSG_ERROR, "ACS failed with error: %d, channel change is not possible",
			   cs_err);
		hostapd_dcs_restore_extn(iface, "ACS channel switch failed");
	}

	iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;
	return 0;
}

int
acs_handle_channel_change_failed_extn(struct hostapd_iface *iface,
				      struct hostapd_channel_data *ideal_chan,
				      int err)
{
	if (iface->iface_extn.dynamic_acs_action == DYNAMIC_ACS_DISABLE)
		return -1;

	wpa_printf(MSG_ERROR, "ACS failed with error: %d, channel change is not possible",
		   err);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DYNAMIC_ACS_EVENT_FAILED);
	/* Notify wpa_supplicant to resume scans on failure */
	hostapd_ml_acs_check_and_notify(iface, 0);
	iface->iface_extn.dynamic_acs_action = DYNAMIC_ACS_DISABLE;

	if (!ideal_chan) {
		wpa_printf(MSG_ERROR, "ACS: No ideal channel found, bringing down the VAP");
		hostapd_disable_iface(iface);
		return 0;
	}

	hostapd_dcs_restore_extn(iface, "ACS failed");

	return 0;
}

bool
hostapd_hwbl_validate_6ghz(struct hostapd_iface *iface,
			   struct hostapd_channel_data *chan,
			   u16 bw, u16 center_freq, u16 punct_bitmap,
			   u8 nl80211_pwr_mode)
{
	u8 target_pwr_mode = nl80211_pwr_mode;

	if (!is_6ghz_freq(chan->freq))
		return true;

	if (iface->conf->enable_best_power_mode) {
		u8 best_pwr_mode;

		best_pwr_mode = hostapd_get_best_ap_6ghz_power_mode(
			iface, chan->freq, center_freq, bw, punct_bitmap);
		if (best_pwr_mode < NL80211_REG_NUM_POWER_MODES)
			target_pwr_mode = best_pwr_mode;
	}

	if (target_pwr_mode >= NL80211_REG_NUM_POWER_MODES)
		return false;

	return hostapd_validate_chan_bw_in_pwr_mode(iface, chan->freq,
						    center_freq, bw,
						    punct_bitmap,
						    target_pwr_mode);
}

static u16
acs_get_chan_center_freq_extn(u16 freq, u32 bw, int bw320_offset)
{
	enum bw_type bw_type;
	int center_chan;

	if (bw == 20)
		return freq;

	switch (bw) {
	case 40:
		bw_type = ACS_BW40;
		break;
	case 80:
		bw_type = ACS_BW80;
		break;
	case 160:
		bw_type = ACS_BW160;
		break;
	case 320:
		bw_type = bw320_offset == ACS_BW320_2 ? ACS_BW320_2 : ACS_BW320_1;
		break;
	default:
		return 0;
	}

	center_chan = acs_get_bw_center_chan(freq, bw_type);
	if (!center_chan)
		return 0;

	return (center_chan == 2) ? 5935 : 5950 + center_chan * 5;
}

bool
acs_hwbl_candidate_ok(struct hostapd_iface *iface,
		      struct hostapd_channel_data *chan,
		      u32 bw, int bw320_offset, u16 punct_bitmap,
		      u8 nl80211_pwr_mode)
{
	u16 center_freq;

	if (!is_6ghz_freq(chan->freq))
		return true;

	center_freq = acs_get_chan_center_freq_extn(chan->freq, bw, bw320_offset);
	if (!center_freq)
		return false;

	return hostapd_hwbl_validate_6ghz(iface, chan, bw, center_freq,
					  punct_bitmap, nl80211_pwr_mode);
}

#ifdef CONFIG_IEEE80211BE
static int acs_get_primary_index_extn(u16 freq, u32 bw, int bw320_offset)
{
	u16 cen_freq, start_freq;

	if (bw == 20)
		return 0;

	cen_freq = acs_get_chan_center_freq_extn(freq, bw, bw320_offset);
	if (!cen_freq)
		return 0;

	start_freq = cen_freq - (u16)(bw / 2) + 10;
	return (freq - start_freq) / 20;
}
#endif /* CONFIG_IEEE80211BE */

bool
acs_hwbl_chan_ok_extn(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
		      int n_chans, struct hostapd_channel_data *chan,
		      long double factor)
{
	u16 punct_bitmap = iface->conf->punct_bitmap;
	u16 saved_punct_bitmap = chan->punct_bitmap;

	if (!is_6ghz_freq(chan->freq))
		return true;

	chan->punct_bitmap = punct_bitmap;
#ifdef CONFIG_IEEE80211BE
	if (iface->conf->ieee80211be) {
		int index_primary = acs_get_primary_index_extn(chan->freq, bw,
							       bw320_offset);

		acs_update_puncturing_bitmap(iface, mode, bw, n_chans, chan,
					     factor, index_primary);
	}
#endif /* CONFIG_IEEE80211BE */
	punct_bitmap = chan->punct_bitmap;
	chan->punct_bitmap = saved_punct_bitmap;

	return acs_hwbl_candidate_ok(iface, chan, bw, bw320_offset,
				     punct_bitmap,
				     iface->conf->he_6ghz_reg_pwr_type);
}
