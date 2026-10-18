// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "utils/os.h"
#include "common/ieee802_11_defs.h"
#include "ap/ap_config.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/ap_drv_ops.h"
#include "ap/acs.h"
#include "ap/hw_features.h"
#include "utils/eloop.h"
#include "cbs.h"
#include "dcs.h"
#include "cmn.h"
#include "acs_extn.h"

#define HOSTAPD_CBS_BEST_CHAN_MAX_AGE_SEC 600
#define HOSTAPD_CBS_RETRIGGER_MIN_AGE_SEC 300

static void
hostapd_cbs_find_acs_ideal_chan(struct hostapd_data *link_hapd,
				struct hostapd_iface *iface,
				struct hostapd_config_extn *conf_extn);

static int
cbs_print_usage_extn(char *reply, int reply_size)
{
	int ret;

	ret = os_snprintf(
		reply, reply_size,
		"cbs commands:\n"
		"  enable <0|1|2>       : enable/disable cbs scan (0:disable | 1:enable CBS scan once | 2:enable cbs scan to run continuously)\n"
		"  g_enable             : get continuous background scan enable state\n"
		"  resttime <ms>        : set rest time in milliseconds\n"
		"  g_resttime           : get rest time in milliseconds\n"
		"  dwellrest <ms>       : set dwell rest time in milliseconds\n"
		"  g_dwellrest          : get dwell rest time in milliseconds\n"
		"  waittime <ms>        : set wait time in milliseconds\n"
		"  g_waittime           : get wait time in milliseconds\n"
		"  dwellsplit <value>   : set dwell split value\n"
		"  g_dwellsplit         : get dwell split value\n"
		"  totaldwell <value>   : set total dwell value\n"
		"  g_totaldwell         : get total dwell value\n"
		);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_get_enable(struct hostapd_config_extn *conf_extn,
					const char *pos,
					char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS enable: %d\n",
			      conf_extn->cbs_params.cbs_enable);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_set_enable(struct hostapd_data *hapd,
				  struct hostapd_config_extn *conf_extn,
				  const char *pos, char *reply,
				  size_t reply_size)
{
	int val = atoi(pos);
	int acs_ch_list_all = 0;
	int *freq_list = NULL;
	int ret = 0;
	struct hostapd_hw_modes *mode;
	struct cbs_params_extn *cbs_params = &conf_extn->cbs_params;
	struct os_reltime now, age;
	bool skip_cbs_scan = false;
	u64 age_ms;

	if (!(val == 0 || val == 1 || val == 2)) {
		wpa_printf(MSG_ERROR, "CBS: Invalid input: %d", val);
		return -1;
	}

	if (!hapd->driver || !hapd->driver->set_cbs)
		return -1;

	if (!val) {
		cbs_params->cbs_enable = 0;
		ret = hapd->driver->set_cbs(hapd->drv_priv,
					    cbs_params, NULL,
					    hapd->mld_link_id);
		if (!ret)
			wpa_msg(hapd->msg_ctx, MSG_INFO, CBS_EVENT_ABORTED);
		return ret;
	}

	if (!hapd->iface->current_mode)
		return -1;

	mode = hapd->iface->current_mode;

	/*
	 * If no chanlist config parameter is provided, include all
	 * enabled channels of the selected hw_mode.
	 */
	if (hapd->iface->conf->acs_freq_list_present)
		acs_ch_list_all = !hapd->iface->conf->acs_freq_list.num;
	else
		acs_ch_list_all = !hapd->iface->conf->acs_ch_list.num;

	hostapd_get_hw_mode_any_channels(hapd, mode,
					 acs_ch_list_all,
					 false, &freq_list);
	if (!freq_list) {
		wpa_printf(MSG_ERROR, "CBS: freq_list is empty. Failing CBS trigger.");
		return -1;
	}

	if (val == 1 &&
	    (cbs_params->cbs_scan_complete_ts.sec || cbs_params->cbs_scan_complete_ts.usec ||
	     cbs_params->acs_scan_complete_ts.sec || cbs_params->acs_scan_complete_ts.usec)) {
		struct os_reltime *last_ts;

		os_get_reltime(&now);
		if (os_reltime_before(&cbs_params->cbs_scan_complete_ts,
				      &cbs_params->acs_scan_complete_ts))
			last_ts = &cbs_params->acs_scan_complete_ts;
		else
			last_ts = &cbs_params->cbs_scan_complete_ts;
		os_reltime_sub(&now, last_ts, &age);
		age_ms = (u64) age.sec * 1000 + (u64) age.usec / 1000;
		if (age_ms < cbs_params->retrigger_time)
			skip_cbs_scan = true;
	}

	cbs_params->cbs_enable = val;
	cbs_params->best_chan = NULL;
	if (!skip_cbs_scan) {
		acs_cleanup(hapd->iface);
		qacs_reset_scan_stats(hapd->iface, mode);
	}

	wpa_msg(hapd->msg_ctx, MSG_INFO, CBS_EVENT_STARTED);
	acs_init_extn(hapd->iface, CBS_SCAN_TRIGGER);

	if (skip_cbs_scan) {
		wpa_printf(MSG_INFO,
			   "CBS: skip cbs scan trigger as elapsed time is less than 5 mins, and directly trigger ACS to find ideal channel");
		hostapd_cbs_find_acs_ideal_chan(hapd, hapd->iface,
						conf_extn);
	} else {
		ret = hapd->driver->set_cbs(hapd->drv_priv,
					    cbs_params, freq_list,
					    hapd->mld_link_id);
		if (ret) {
			cbs_params->cbs_enable = 0;
			wpa_msg(hapd->msg_ctx, MSG_INFO, CBS_EVENT_ABORTED);
		}
	}

	os_free(freq_list);
	return ret;
}

static int hostapd_cbs_set_resttime(struct hostapd_config_extn *conf_extn,
				    const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0) {
		conf_extn->cbs_params.resttime = val;
		return 0;
	}

	return -1;
}

static int hostapd_cbs_get_resttime(struct hostapd_config_extn *conf_extn,
				    const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS resttime: %d\n",
			      conf_extn->cbs_params.resttime);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_set_dwellrest(struct hostapd_config_extn *conf_extn,
				     const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0) {
		conf_extn->cbs_params.dwellrest = val;
		return 0;
	}

	return -1;
}

static int hostapd_cbs_get_dwellrest(struct hostapd_config_extn *conf_extn,
				     const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS dwellrest: %d\n",
			      conf_extn->cbs_params.dwellrest);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_set_waittime(struct hostapd_config_extn *conf_extn,
				    const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0) {
		conf_extn->cbs_params.waittime = val;
		return 0;
	}

	return -1;
}

static int hostapd_cbs_get_waittime(struct hostapd_config_extn *conf_extn,
				    const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS waittime: %d\n",
			      conf_extn->cbs_params.waittime);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_set_dwellsplit(struct hostapd_config_extn *conf_extn,
				   const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0) {
		conf_extn->cbs_params.dwellsplit = val;
		return 0;
	}

	return -1;
}

static int hostapd_cbs_get_dwellsplit(struct hostapd_config_extn *conf_extn,
				   const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS dwellsplit: %d\n",
			      conf_extn->cbs_params.dwellsplit);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_cbs_set_totaldwell(struct hostapd_config_extn *conf_extn,
				      const char *pos, char *reply, size_t reply_size)
{
	int val = atoi(pos);

	if (val >= 0) {
		conf_extn->cbs_params.totaldwell = val;
		return 0;
	}

	return -1;
}

static int hostapd_cbs_get_totaldwell(struct hostapd_config_extn *conf_extn,
				      const char *pos, char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "CBS totaldwell: %d\n",
			      conf_extn->cbs_params.totaldwell);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

int hostapd_cbs_handle_single_channel_survey(struct hostapd_iface *iface,
					     struct hostapd_channel_data *chan,
					     struct freq_survey *survey)
{
	if (iface->conf->conf_extn.cbs_params.cbs_enable) {
		dl_list_del(&survey->list);
		dl_list_add_tail(&chan->survey_list, &survey->list);
		hostapd_update_nf(iface, chan, survey);
		iface->chans_surveyed++;
		return 0;
	}

	return -1;
}

static void
hostapd_cbs_find_acs_ideal_chan(struct hostapd_data *link_hapd,
				struct hostapd_iface *iface,
				struct hostapd_config_extn *conf_extn)
{
	if (conf_extn->qacs_enable) {
		conf_extn->cbs_params.best_chan = qacs_find_ideal_chan(iface);
	} else {
		acs_study_options(iface);
		conf_extn->cbs_params.best_chan = acs_find_ideal_chan(iface);
	}

	if (conf_extn->cbs_params.best_chan)
		acs_fill_timestamp(iface, CBS_SCAN_TRIGGER, false);

	if (conf_extn->cbs_params.cbs_enable == 1)
		conf_extn->cbs_params.cbs_enable = 0;

	wpa_msg(link_hapd->msg_ctx, MSG_INFO, CBS_EVENT_COMPLETED);
}

int hostapd_cbs_handle_scan_complete(struct hostapd_data *hapd,
				     union wpa_event_data *data)
{
	struct cbs_event *cbs_evt;
	struct hostapd_data *link_hapd;
	struct hostapd_iface *iface;
	struct hostapd_config_extn *conf_extn;

	if (!data) {
		wpa_printf(MSG_ERROR, "WPA event with NULL data");
		return -1;
	}

	cbs_evt = &data->event_data_extn.scan_results_event.cbs_event;
	link_hapd = switch_link_hapd(hapd, cbs_evt->link_id);
	if (!link_hapd)
		return -1;

	iface = link_hapd->iface;
	conf_extn = &iface->conf->conf_extn;

	wpa_printf(MSG_DEBUG,
		   "CBS scan complete: frequency=%u scan_complete=%u link_id=%u",
		   cbs_evt->scan_complete_freq, cbs_evt->status,
		   cbs_evt->link_id);

	if (!cbs_evt->scan_complete_freq)
		return -1;

	/* Pre processing of scan results per channel */
	if (cbs_evt->status == VENDOR_SCAN_STATUS_NEW_RESULTS ||
	    cbs_evt->status == VENDOR_SPLIT_SCAN_COMPLETE_PER_CHANNEL) {
		if (hostapd_drv_get_survey(link_hapd,
					   cbs_evt->scan_complete_freq))
			wpa_printf(MSG_ERROR, "CBS: survey results failed");
		if (conf_extn->qacs_enable)
			acs_process_hostapd_scan_data_per_freq(
				iface, cbs_evt->scan_complete_freq);
	}

	if (cbs_evt->status == VENDOR_SCAN_STATUS_NEW_RESULTS) {
		os_get_reltime(&conf_extn->cbs_params.cbs_scan_complete_ts);
		wpa_printf(MSG_DEBUG,
			   "Scan complete ts is recorded for CBS: %ld.%06ld (freq=%u link_id=%u)",
			   conf_extn->cbs_params.cbs_scan_complete_ts.sec,
			   conf_extn->cbs_params.cbs_scan_complete_ts.usec,
			   cbs_evt->scan_complete_freq, cbs_evt->link_id);
		hostapd_cbs_find_acs_ideal_chan(link_hapd, iface, conf_extn);
	}

	return 0;
}

int hostapd_cbs_trigger_csa(struct hostapd_data *hapd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iface->conf->conf_extn;
	struct hostapd_channel_data *cbs_chan;
	int current_freq = hapd->iface->freq;
	struct os_reltime now, age;
	u64 age_ms;

	os_get_reltime(&now);
	if (os_reltime_before(&conf_extn->cbs_params.cbs_scan_complete_ts,
			      &conf_extn->cbs_params.acs_scan_complete_ts))
		os_reltime_sub(&now, &conf_extn->cbs_params.acs_scan_complete_ts, &age);
	else
		os_reltime_sub(&now, &conf_extn->cbs_params.cbs_scan_complete_ts, &age);

	wpa_printf(MSG_DEBUG,
		   "CBS CSA timestamp check: now=%ld.%06ld cbs_scan_complete_ts=%ld.%06ld acs_scan_complete_ts=%ld.%06ld age=%ld.%06ld",
		   now.sec, now.usec,
		   conf_extn->cbs_params.cbs_scan_complete_ts.sec,
		   conf_extn->cbs_params.cbs_scan_complete_ts.usec,
		   conf_extn->cbs_params.acs_scan_complete_ts.sec,
		   conf_extn->cbs_params.acs_scan_complete_ts.usec,
		   age.sec, age.usec);

	if (!( conf_extn->cbs_params.acs_scan_complete_ts.sec ||
	     conf_extn->cbs_params.acs_scan_complete_ts.usec ||
	     conf_extn->cbs_params.cbs_scan_complete_ts.sec ||
	     conf_extn->cbs_params.cbs_scan_complete_ts.usec))
		return -1;

	age_ms = (u64) age.sec * 1000 + (u64) age.usec / 1000;
	if (age_ms >= conf_extn->cbs_params.retrigger_time) {
		wpa_printf(MSG_INFO,
			   "CBS best channel age %llu ms exceeds max %u ms. Skipping CSA for CBS channel",
			   (unsigned long long) age_ms,
			   conf_extn->cbs_params.retrigger_time);
		conf_extn->cbs_params.best_chan = NULL;
		return -1;
	}

	wpa_msg(hapd->msg_ctx, MSG_INFO, CBS_EVENT_STARTED);
	acs_init_extn(hapd->iface, CBS_SCAN_TRIGGER);
	hostapd_set_channel_penalty_extn(hapd->iface, current_freq,
		conf_extn->qacs_conf.dcs_channel_penalty);

	hostapd_cbs_find_acs_ideal_chan(hapd, hapd->iface, conf_extn);
	hostapd_set_channel_penalty_extn(hapd->iface, current_freq, 0);

	cbs_chan = conf_extn->cbs_params.best_chan;
	if (!cbs_chan) {
		wpa_printf(MSG_DEBUG, "CBS CSA attempt failed. CBS chan: %p",
			   cbs_chan);
		return -1;
	}

	if (hapd->iface->freq == cbs_chan->freq) {
		wpa_printf(MSG_ERROR,
			   "CBS CSA attempt failed. Current channel is same as CBS Best channel: (%d)",
			   cbs_chan->freq);
		return -1;
	}

	wpa_printf(MSG_INFO, "CBS attempting CSA for freq: %d", cbs_chan->freq);

	return hostapd_trigger_channel_switch_extn(hapd->iface, cbs_chan);
}


int hostapd_handle_cli_cbs_extn(struct hostapd_data *hapd,
				char *pos, char *buf,
				size_t buflen)
{
	struct hostapd_config_extn *conf_extn;

	if (!hapd->iface || !hapd->iface->conf)
		return -1;

	conf_extn  = &hapd->iface->conf->conf_extn;

	if (os_strncmp(pos, "g_enable", 8) == 0) {
		return hostapd_cbs_get_enable(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "enable ", 7) == 0) {
		return hostapd_cbs_set_enable(hapd,
					      conf_extn,
					      pos + 7, buf, buflen);

	} else if (os_strncmp(pos, "g_resttime", 10) == 0) {
		return hostapd_cbs_get_resttime(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "resttime ", 9) == 0) {
		return hostapd_cbs_set_resttime(conf_extn, pos + 9, buf, buflen);

	} else if (os_strncmp(pos, "g_dwellrest", 11) == 0) {
		return hostapd_cbs_get_dwellrest(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "dwellrest ", 10) == 0) {
		return hostapd_cbs_set_dwellrest(conf_extn, pos + 10, buf, buflen);

	} else if (os_strncmp(pos, "g_waittime", 10) == 0) {
		return hostapd_cbs_get_waittime(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "waittime ", 9) == 0) {
		return hostapd_cbs_set_waittime(conf_extn, pos + 9, buf, buflen);

	} else if (os_strncmp(pos, "g_dwellsplit", 12) == 0) {
		return hostapd_cbs_get_dwellsplit(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "dwellsplit ", 11) == 0) {
		return hostapd_cbs_set_dwellsplit(conf_extn, pos + 11, buf, buflen);

	} else if (os_strncmp(pos, "g_totaldwell", 12) == 0) {
		return hostapd_cbs_get_totaldwell(conf_extn, pos, buf, buflen);

	} else if (os_strncmp(pos, "totaldwell ", 11) == 0) {
		return hostapd_cbs_set_totaldwell(conf_extn, pos + 11, buf, buflen);

	} else {
		return cbs_print_usage_extn(buf, buflen);
	}
}
