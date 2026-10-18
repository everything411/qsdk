// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include <netlink/genl/genl.h>
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/qca-vendor.h"
#include "drivers/driver.h"
#include "drivers/driver_nl80211.h"
#include "common/ieee802_11_common.h"
#include "ap/hostapd.h"
#include "ap/ieee802_11.h"
#include "ap/beacon.h"
#include "ap/hw_features.h"
#include "ap/interference.h"
#include "common/wpa_ctrl.h"
#include "cmn.h"
#include "dcs.h"
#include "common/hw_features_common.h"
#include "cbs.h"

static void hostapd_dcs_reenable_timeout(void *eloop_ctx, void *timeout_ctx);
static void hostapd_dcs_apply_enable_bitmap(struct hostapd_iface *iface,
					    u16 enable_bitmap);
#ifdef RDK_ONEWIFI
static void hostapd_dcs_in_progress_timeout(void *eloop_ctx, void *timeout_ctx);

/* Maximum time (seconds) dcs_in_progress may be held before auto-clear.
 * Use 30s: covers 5 ACS scans * ~1s each + CSA time. Faster retry on
 * firmware CSA completion delay. */
#define DCS_IN_PROGRESS_TIMEOUT_SEC 30
#endif

static void hostapd_dcs_disable_fw(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd;
	struct driver_dcs_config drv_dcs_conf;

	if (!iface || !iface->conf)
		return;

	hapd = iface->bss[0];
	if (!hapd)
		return;

	os_memset(&drv_dcs_conf, 0, sizeof(drv_dcs_conf));
	drv_dcs_conf.dcs_enable = 0;
	drv_dcs_conf.cmd_type = SET_DCS_CONFIG;

	if (hostapd_drv_dcs_config(hapd, hapd->mld_link_id,
				   &drv_dcs_conf) < 0)
		wpa_printf(MSG_ERROR,
			   "DCS: failed to disable");
}

static bool hostapd_dcs_set_in_progress(struct hostapd_iface *iface, u16 type)
{
	struct hostapd_iface_extn *iface_extn;

	if (!iface)
		return false;

	iface_extn = &iface->iface_extn;
	if (iface_extn->dcs_in_progress) {
		wpa_printf(MSG_ERROR,
			   "DCS: interference event ignored (dcs in progress)");
		return false;
	}

	iface_extn->dcs_in_progress = true;
	hostapd_dcs_disable_fw(iface);
	wpa_printf(MSG_DEBUG,
		   "DCS: dcs in progress set, firmware DCS disabled (type=0x%04x)",
		   type);
#ifdef RDK_ONEWIFI
	eloop_cancel_timeout(hostapd_dcs_in_progress_timeout, iface, NULL);
	eloop_register_timeout(DCS_IN_PROGRESS_TIMEOUT_SEC, 0,
			       hostapd_dcs_in_progress_timeout, iface, NULL);
#endif
	return true;
}

void hostapd_dcs_restore_extn(struct hostapd_iface *iface, const char *reason)
{
	struct hostapd_iface_extn *iface_extn;
	u16 enable_bitmap;

	if (!iface || !iface->conf)
		return;

	iface_extn = &iface->iface_extn;
	if (!iface_extn->dcs_in_progress)
		return;

	enable_bitmap = iface->conf->conf_extn.dcs_conf.enable_bitmap;
	wpa_printf(MSG_DEBUG,
		   "DCS: restoring after %s (enable_bitmap=0x%04x)",
		   reason ? reason : "CSA",
		   enable_bitmap);
	iface_extn->dcs_in_progress = false;
	wpa_printf(MSG_ERROR, "DCS: dcs in progress cleared");
#ifdef RDK_ONEWIFI
	eloop_cancel_timeout(hostapd_dcs_in_progress_timeout, iface, NULL);
	/* Clear csa_in_progress on all BSS so the next CSA attempt is not
	 * blocked by a stale flag from a previous CSA that never completed
	 * (e.g. NL80211_CMD_CH_SWITCH_NOTIFY was never received). */
	for (int _i = 0; _i < iface->num_bss; _i++) {
		if (iface->bss[_i] && iface->bss[_i]->csa_in_progress)
			hostapd_cleanup_cs_params(iface->bss[_i]);
	}
	if (iface_extn->dcs_disabled_excessive_triggers) {
		wpa_printf(MSG_ERROR,
			   "DCS: restoring with rate-limit bitmap (0x%04x) instead of full bitmap (0x%04x)",
			   iface_extn->dcs_excess_trigger_enable_bitmap,
			   enable_bitmap);
		hostapd_dcs_apply_enable_bitmap(iface,
			iface_extn->dcs_excess_trigger_enable_bitmap);
	} else {
		hostapd_dcs_apply_enable_bitmap(iface, enable_bitmap);
	}
#else
	hostapd_dcs_apply_enable_bitmap(iface, enable_bitmap);
#endif
}

#ifdef RDK_ONEWIFI
static void hostapd_dcs_in_progress_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;

	wpa_printf(MSG_ERROR,
		   "DCS: dcs_in_progress safety timeout fired (%d s) — force-clearing",
		   DCS_IN_PROGRESS_TIMEOUT_SEC);
	hostapd_dcs_restore_extn(iface, "safety timeout");
}
#endif

static unsigned int hostapd_dcs_get_reenable_time_sec(struct hostapd_iface *iface)
{
	struct hostapd_iface_extn *iface_extn;
	u32 secs;

	if (!iface)
		return HOSTAPD_DCS_REENABLE_TIME_SEC;

	iface_extn = &iface->iface_extn;
	secs = iface_extn->dcs_re_enable_time;

	if (secs < DCS_ENABLE_TIME_MIN || secs > DCS_ENABLE_TIME_MAX)
		secs = DCS_ENABLE_TIME;

	return secs;
}

static void hostapd_dcs_apply_enable_bitmap(struct hostapd_iface *iface,
						    u16 enable_bitmap)
{
	struct hostapd_data *hapd;
	struct hostapd_iface_extn *iface_extn;

	if (!iface || !iface->conf || !iface->bss || iface->num_bss < 1)
		return;

	hapd = iface->bss[0];
	if (!hapd)
		return;

	iface_extn = &iface->iface_extn;

	if (iface_extn->dcs_in_progress) {
		wpa_printf(MSG_ERROR,
			   "DCS: deferring enable bitmap apply (dcs in progress)");
		return;
	}

	dcs_enable_init(hapd, enable_bitmap);
}

static void hostapd_dcs_rate_limit_reset(struct hostapd_iface *iface)
{
	struct hostapd_iface_extn *iface_extn;

	if (!iface)
		return;

	iface_extn = &iface->iface_extn;
	if (!iface_extn->dcs_re_enable_time)
		iface_extn->dcs_re_enable_time = DCS_ENABLE_TIME;
	iface_extn->dcs_trigger_count = 0;
	os_memset(iface_extn->dcs_trigger_ts, 0,
		  sizeof(iface_extn->dcs_trigger_ts));
	iface_extn->dcs_disabled_excessive_triggers = false;
	iface_extn->dcs_excess_trigger_enable_bitmap = 0;
	iface_extn->dcs_excess_trigger_restore_bitmap = 0;

	if (iface_extn->dcs_reenable_timer_set) {
		wpa_printf(MSG_ERROR,
			   "DCS: reenable timer cancelled (dcs_re_enable_time=%u sec)",
			   hostapd_dcs_get_reenable_time_sec(iface));
		eloop_cancel_timeout(hostapd_dcs_reenable_timeout, iface, NULL);
		iface_extn->dcs_reenable_timer_set = false;
	}
}

static void hostapd_dcs_rate_limit_update(struct hostapd_iface *iface, u16 type)
{
	struct hostapd_iface_extn *iface_extn;
	struct os_reltime now, diff;
	unsigned int ix;
	bool disable_dcs = false;
	u16 cur_enable, new_enable;
	unsigned int reenable_time;

	if (!iface || !iface->conf)
		return;

	if (type != DCS_WLAN_INTF)
		return;

	iface_extn = &iface->iface_extn;
	if (iface_extn->dcs_disabled_excessive_triggers) {
		wpa_printf(MSG_ERROR,
			   "DCS: WLAN interference event ignored (already auto-disabled; trigger_count=%u)",
			   iface_extn->dcs_trigger_count);
		if (iface_extn->dcs_reenable_timer_set)
			wpa_printf(MSG_DEBUG,
				   "DCS: reenable timer in progress (dcs_re_enable_time=%u sec)",
				   hostapd_dcs_get_reenable_time_sec(iface));
		return;
	}

	cur_enable = iface->conf->conf_extn.dcs_conf.enable_bitmap;
	if (!(cur_enable & DCS_WLAN_INTF)) {
		wpa_printf(MSG_ERROR,
			   "DCS: WLAN interference rate-limit skipped (DCS_WLAN_INTF not enabled; enable_bitmap=0x%04x)",
			   cur_enable);
		return;
	}

	os_get_reltime(&now);

	if (iface_extn->dcs_trigger_count >= (HOSTAPD_DCS_MAX_TRIGGERS - 1)) {
		os_reltime_sub(&now, &iface_extn->dcs_trigger_ts[0], &diff);
		wpa_printf(MSG_ERROR,
			   "DCS: WLAN interference trigger window check: count=%u oldest_age=%lu sec (limit=%u sec)",
			   iface_extn->dcs_trigger_count, (unsigned long) diff.sec,
			   HOSTAPD_DCS_AGING_TIME_SEC);
		if (diff.sec < HOSTAPD_DCS_AGING_TIME_SEC) {
			disable_dcs = true;
		} else {
			for (ix = 0; ix < (iface_extn->dcs_trigger_count - 1);
			     ix++) {
				iface_extn->dcs_trigger_ts[ix] =
					iface_extn->dcs_trigger_ts[ix + 1];
			}
			iface_extn->dcs_trigger_count--;
		}
	}

	if (disable_dcs) {
		new_enable = cur_enable & ~DCS_WLAN_INTF;
		iface_extn->dcs_disabled_excessive_triggers = true;
		iface_extn->dcs_excess_trigger_restore_bitmap = cur_enable;
		iface_extn->dcs_excess_trigger_enable_bitmap = new_enable;
		iface_extn->dcs_trigger_count = 0;
		reenable_time = hostapd_dcs_get_reenable_time_sec(iface);

		wpa_printf(MSG_ERROR,
			   "DCS: disabling WLAN interference handling (0x%04x -> 0x%04x); too many triggers within %u sec",
			   cur_enable, new_enable,
			   HOSTAPD_DCS_AGING_TIME_SEC);

		hostapd_dcs_apply_enable_bitmap(iface, new_enable);

		if (!iface_extn->dcs_reenable_timer_set) {
			wpa_printf(MSG_ERROR,
				   "DCS: reenable timer started time %u sec (dcs_re_enable_time=%u sec)",
				   reenable_time, reenable_time);
			eloop_register_timeout(reenable_time, 0,
					       hostapd_dcs_reenable_timeout,
					       iface, NULL);
			iface_extn->dcs_reenable_timer_set = true;
		} else {
			wpa_printf(MSG_ERROR,
				   "DCS: reenable timer already running (dcs_re_enable_time=%u sec)",
				   reenable_time);
		}
		wpa_printf(MSG_ERROR,
			   "DCS: WLAN interference handling auto-disabled (for %u sec)",
			   reenable_time);
		return;
	}

	if (iface_extn->dcs_trigger_count < HOSTAPD_DCS_MAX_TRIGGERS) {
		iface_extn->dcs_trigger_ts[iface_extn->dcs_trigger_count] = now;
		iface_extn->dcs_trigger_count++;
	}
	wpa_printf(MSG_ERROR,
		   "DCS: WLAN interference trigger recorded (new_count=%u)",
		   iface_extn->dcs_trigger_count);
}

static void hostapd_dcs_reenable_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	struct hostapd_iface_extn *iface_extn;

	(void) timeout_ctx;

	if (!iface || !iface->conf || !iface->bss ||
	    iface->num_bss < 1 || !iface->bss[0])
		return;

	iface_extn = &iface->iface_extn;
	iface_extn->dcs_reenable_timer_set = false;
	iface_extn->dcs_trigger_count = 0;

	wpa_printf(MSG_ERROR,
		   "DCS: reenable timer expired (dcs_re_enable_time=%u sec)",
		   hostapd_dcs_get_reenable_time_sec(iface));

	if (!iface_extn->dcs_disabled_excessive_triggers)
		return;

	wpa_printf(MSG_ERROR,
		   "DCS: re-enabling WLAN interference handling (0x%04x -> 0x%04x) after timeout",
		   iface_extn->dcs_excess_trigger_enable_bitmap,
		   iface_extn->dcs_excess_trigger_restore_bitmap);
	hostapd_dcs_apply_enable_bitmap(
		iface, iface_extn->dcs_excess_trigger_restore_bitmap);

	iface_extn->dcs_disabled_excessive_triggers = false;
	iface_extn->dcs_excess_trigger_enable_bitmap = 0;
	iface_extn->dcs_excess_trigger_restore_bitmap = 0;
}

void hostapd_dcs_iface_deinit_extn(struct hostapd_iface *iface)
{
	struct hostapd_iface_extn *iface_extn;

	if (!iface)
		return;

	iface_extn = &iface->iface_extn;
	eloop_cancel_timeout(hostapd_dcs_reenable_timeout, iface, NULL);
	iface_extn->dcs_reenable_timer_set = false;
	iface_extn->dcs_trigger_count = 0;
	os_memset(iface_extn->dcs_trigger_ts, 0,
		  sizeof(iface_extn->dcs_trigger_ts));
	iface_extn->dcs_disabled_excessive_triggers = false;
	iface_extn->dcs_excess_trigger_enable_bitmap = 0;
	iface_extn->dcs_excess_trigger_restore_bitmap = 0;
	iface_extn->dcs_in_progress = false;
}

static int
dcs_print_usage_extn(char *reply, int reply_size)
{
	int ret;

	ret = os_snprintf(
		reply, reply_size,
		"dcs extn commands:\n"
		"  dcs enable           : set DCS configuration\n"
		"  dcs bw_reduction_ctrl : DCS bw reduction control\n"
		"  dcs csa_tbtt		:  set DCS CSA TBTT value\n"
		"  dcs set_wlan_intr_params: set DCS WLAN INTR PARAMS\n"
		"  dcs get_wlan_intr_params: get DCS WLAN INTR PARAMS\n"
		"  dcs random_chan_bitmap : set random channel bitmask\n"
		"  (Bit(0)=CW, Bit(1)=WLAN, Bit(2)=AWGN, Bit(4)=OBSS, 0=Disabled; AWGN enabled by default during init)"
		"  It is advised for user to keep it enabled for AWGN through CLI\n"
		"  dcs event_action : set DCS event action bitmap\n"
		"  (Bit(0)=process CW, Bit(1)=process WLAN, Bit(4)=process OBSS; bit not set => discard)\n"
		"  dcs event_notify : <mask> control INTERFERENCE_DETECTED notification bitmap\n"
		"  (Bit(0)=notify CW, Bit(1)=notify WLAN, Bit(4)=notify OBSS)\n"
		"  dcs get_enable   : get DCS enable value\n"
		"  dcs get_event_action : get DCS event action bitmap\n"
		"  dcs get_event_notify : get DCS event notification state\n"
		"  dcs set_dcs_enable_timer : <sec> = set DCS re-enable time\n"
		"  dcs get_dcs_enable_timer : get DCS re-enable time\n"
		"  dcs sim              : DCS simulator\n"
		"  dcs get_random_chan_en	: get DCS random channel enable state\n"
		"  dcs get_csa_tbtt		: get DCS CSA TBTT value\n"
		"  dcs get_bw_reduction_ctrl  : get DCS BW reduction control value\n"
		);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

int hostapd_drv_dcs_config(struct hostapd_data *hapd, u8 link_id,
			   struct driver_dcs_config *params)
{
	if (!hapd->driver || !hapd->driver->dcs_config)
		return -1;

	return hapd->driver->dcs_config(hapd->drv_priv, link_id, params);
}

int hostapd_drv_dcs_sim_trigger(struct hostapd_data *hapd, u8 link_id,
				struct driver_dcs_sim *params)
{

	if (!hapd->driver || !hapd->driver->dcs_sim)
		return -1;

	return hapd->driver->dcs_sim(hapd->drv_priv, link_id, params);
}

static int hostapd_ctrl_iface_dcs_config(struct hostapd_data *hapd,
					 const char *cmd, char *reply,
					 int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	struct driver_dcs_config drv_dcs_conf;
	char *end;
	unsigned long v;
	u16 cur_enable;
	u16 req_enable;
	u16 apply_enable;
	u16 req_non_wlan;
	u16 restore_wlan_bit;
	bool wlan_bit_changed;
	bool cooldown_active;

	(void) reply;
	(void) reply_size;

	if (!hapd || !hapd->iface || !cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "CTRL: DCS_ENABLE: empty value");
		return -1;
	}

	errno = 0;
	v = strtoul(cmd, &end, 0);
	while (*end == ' ')
		end++;
	if (errno != 0 || end == cmd || *end != '\0' || v > 0xFFFF) {
		wpa_printf(MSG_ERROR, "CTRL: DCS_ENABLE: invalid value '%s'", cmd);
		return -1;
	}

	if (v & ~ALLOWED_DCS_MASK) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DCS_ENABLE: invalid value 0x%lx (allowed bits: 0(CW IM), 1(WLAN IM), 4(OBSS IM))",
			   v);
		return -1;
	}

	cur_enable = conf_extn->dcs_conf.enable_bitmap;
	req_enable = (u16) v;
	apply_enable = req_enable;
	req_non_wlan = req_enable & ~DCS_WLAN_INTF;
	cooldown_active = iface_extn->dcs_disabled_excessive_triggers;
	wlan_bit_changed = !!((cur_enable ^ req_enable) & DCS_WLAN_INTF);

	if (wlan_bit_changed) {
		wpa_printf(MSG_ERROR,
			   "DCS: reset rate-limit state due to WLAN bit change (0x%04x -> 0x%04x)",
			   cur_enable, req_enable);
		hostapd_dcs_rate_limit_reset(hapd->iface);
	} else if (cooldown_active) {
		restore_wlan_bit =
			iface_extn->dcs_excess_trigger_restore_bitmap & DCS_WLAN_INTF;
		apply_enable = req_non_wlan;
		iface_extn->dcs_excess_trigger_enable_bitmap = apply_enable;
		iface_extn->dcs_excess_trigger_restore_bitmap =
			req_non_wlan | restore_wlan_bit;
		wpa_printf(MSG_ERROR,
			   "DCS: preserving WLAN cooldown timer; active=0x%04x restore=0x%04x",
			   iface_extn->dcs_excess_trigger_enable_bitmap,
			   iface_extn->dcs_excess_trigger_restore_bitmap);
	}

	os_memset(&drv_dcs_conf, 0, sizeof(drv_dcs_conf));
	drv_dcs_conf.dcs_enable = apply_enable;
	drv_dcs_conf.cmd_type = SET_DCS_CONFIG;
	conf_extn->dcs_conf.enable_bitmap = req_enable;

	/* Send current defaults to driver on enable */
	drv_dcs_conf.valid_mask =
		DCS_VALID_INTR_DET_THR |
		DCS_VALID_PHYERR_PENALTY |
		DCS_VALID_PHYERR_THR |
		DCS_VALID_RADARERR_THR |
		DCS_VALID_TXERR_THR |
		DCS_VALID_SAMPLE_SIZE |
		DCS_VALID_COCH_THR |
		DCS_VALID_USER_MAX_CU;

	drv_dcs_conf.intr_detection_threshold = conf_extn->dcs_conf.intr_detection_threshold;
	drv_dcs_conf.phyerr_penalty = conf_extn->dcs_conf.phyerr_penalty;
	drv_dcs_conf.phyerr_threshold = conf_extn->dcs_conf.phyerr_threshold;
	drv_dcs_conf.radarerr_threshold = conf_extn->dcs_conf.radarerr_threshold;
	drv_dcs_conf.txerr_threshold = conf_extn->dcs_conf.txerr_threshold;
	drv_dcs_conf.sample_size = conf_extn->dcs_conf.sample_size;
	drv_dcs_conf.coch_intr_threshold = conf_extn->dcs_conf.coch_intr_threshold;
	drv_dcs_conf.user_max_cu = conf_extn->dcs_conf.user_max_cu;

	return hostapd_drv_dcs_config(hapd, hapd->mld_link_id, &drv_dcs_conf);
}

static int hostapd_ctrl_iface_set_dcs_bw_reduction_ctrl(struct hostapd_data *hapd,
							const char *cmd, char *reply,
							int reply_size)
{
	struct hostapd_config_extn *config_extn = &hapd->iconf->conf_extn;
	unsigned long val_ul;
	u16 val;
	char *end = NULL;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "DCS_BW_REDUCTION: empty value");
		return -1;
	}

	errno = 0;
	val_ul = strtoul(cmd, &end, 0);
	if (errno != 0 || end == cmd) {
		wpa_printf(MSG_ERROR,
			   "DCS_BW_REDUCTION: invalid value '%s'", cmd);
		return -1;
	}
	while (end && *end == ' ')
		end++;
	if (end && *end != '\0') {
		wpa_printf(MSG_ERROR,
			   "DCS_BW_REDUCTION: trailing characters in value '%s'",
			   cmd);
		return -1;
	}

	val = (u16) val_ul;
	if (val & ~0x001Fu) {
		wpa_printf(MSG_ERROR,
			   "DCS_BW_REDUCTION: only bits 0-4 allowed 0x%04x", val);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "DCS_BW_REDUCTION: bw_reduction_ctrl=%u (0x%04x)", val, val);
	config_extn->dcs_conf.bw_reduction_ctrl = val;

	return 0;
}

static int hostapd_ctrl_iface_set_dcs_csa_tbtt(struct hostapd_data *hapd,
					       const char *cmd, char *reply,
					       int reply_size)
{
	struct hostapd_config_extn *config_extn = &hapd->iconf->conf_extn;
	u32 val;

	if (!config_extn)
		return -1;

	val = atoi(cmd);
	if (!val || val > DCS_CSA_TBTT_MAX || val < DCS_CSA_TBTT_MIN) {
		wpa_printf(MSG_ERROR, "Invalid value of DCS CSA TBTT");
		return -1;
	}

	config_extn->dcs_conf.dcs_csa_tbtt = val;

	return 0;
}

static int hostapd_ctrl_iface_set_wlan_intr_params(struct hostapd_data *hapd,
						   const char *cmd, char *reply,
						   int reply_size)
{
	/* Parse space-separated key value pairs */
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	struct driver_dcs_config conf;
	char *dup, *token, *saveptr;
	int err = 0;

	os_memset(&conf, 0, sizeof(conf));
	conf.cmd_type = SET_DCS_CONFIG;
	conf.dcs_enable = conf_extn->dcs_conf.enable_bitmap;

	/* No defaults here; only send user-configured overrides via valid_mask */

	dup = os_strdup(cmd);
	if (!dup)
		return -1;

	token = strtok_r(dup, " ", &saveptr);
	while (token) {
		const char *key = token;
		const char *valstr = strtok_r(NULL, " ", &saveptr);
		long v;
		char *end = NULL;

		if (!valstr) {
			err = -1;
			break;
		}

		errno = 0;
		v = strtol(valstr, &end, 10);
		if (errno || end == valstr) {
			err = -1;
			break;
		}

		if (os_strcmp(key, "phyerr_penalty") == 0) {
			conf.phyerr_penalty = (u32) v;
			conf.valid_mask |= DCS_VALID_PHYERR_PENALTY;
			conf_extn->dcs_conf.phyerr_penalty = conf.phyerr_penalty;
		} else if (os_strcmp(key, "phyerr_threshold") == 0) {
			conf.phyerr_threshold = (u32) v;
			conf.valid_mask |= DCS_VALID_PHYERR_THR;
			conf_extn->dcs_conf.phyerr_threshold = conf.phyerr_threshold;
		} else if (os_strcmp(key, "radarerr_threshold") == 0) {
			conf.radarerr_threshold = (u32) v;
			conf.valid_mask |= DCS_VALID_RADARERR_THR;
			conf_extn->dcs_conf.radarerr_threshold = conf.radarerr_threshold;
		} else if (os_strcmp(key, "coch_intr_threshold") == 0) {
			conf.coch_intr_threshold = (u8) v;
			conf.valid_mask |= DCS_VALID_COCH_THR;
			conf_extn->dcs_conf.coch_intr_threshold = conf.coch_intr_threshold;
		} else if (os_strcmp(key, "txerr_threshold") == 0) {
			conf.txerr_threshold = (u32) v;
			conf.valid_mask |= DCS_VALID_TXERR_THR;
			conf_extn->dcs_conf.txerr_threshold = conf.txerr_threshold;
		} else if (os_strcmp(key, "user_max_cu") == 0) {
			conf.user_max_cu = (u8) v;
			conf.valid_mask |= DCS_VALID_USER_MAX_CU;
			conf_extn->dcs_conf.user_max_cu = conf.user_max_cu;
		} else if (os_strcmp(key,
			   "intr_detection_threshold") == 0) {
			conf.intr_detection_threshold = (u32) v;
			conf.valid_mask |= DCS_VALID_INTR_DET_THR;
			conf_extn->dcs_conf.intr_detection_threshold = conf.intr_detection_threshold;
		} else if (os_strcmp(key, "sample_size") == 0) {
			conf.sample_size = (u32) v;
			conf.valid_mask |= DCS_VALID_SAMPLE_SIZE;
			conf_extn->dcs_conf.sample_size = conf.sample_size;
		} else {
			wpa_printf(MSG_ERROR, "Unknown DCS param: %s", key);
			err = -1;
			break;
		}

		token = strtok_r(NULL, " ", &saveptr);
	}

	os_free(dup);
	if (err)
		return -1;

	return hostapd_drv_dcs_config(hapd, hapd->mld_link_id, &conf);
}

static int hostapd_ctrl_iface_get_wlan_intr_params(struct hostapd_data *hapd,
						   const char *cmd, char *reply,
						   int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int ret;

	ret = os_snprintf(reply, reply_size,
			  "phyerr_penalty=%u phyerr_threshold=%u radarerr_threshold=%u coch_intr_threshold=%u\ntxerr_threshold=%u user_max_cu=%u intr_detection_threshold=%u\nsample_size=%u\n",
			  conf_extn->dcs_conf.phyerr_penalty,
			  conf_extn->dcs_conf.phyerr_threshold,
			  conf_extn->dcs_conf.radarerr_threshold,
			  conf_extn->dcs_conf.coch_intr_threshold,
			  conf_extn->dcs_conf.txerr_threshold,
			  conf_extn->dcs_conf.user_max_cu,
			  conf_extn->dcs_conf.intr_detection_threshold,
			  conf_extn->dcs_conf.sample_size);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_random_chan_en(struct hostapd_data *hapd,
						 const char *cmd, char *reply,
						 int reply_size)
{
	struct hostapd_config_extn *config_extn;
	char *end = NULL;
	unsigned long tmp;
	u16 val;

	if (!hapd || !hapd->iconf) {
		wpa_printf(MSG_ERROR, "hapd/iconf is NULL");
		return -1;
	}

	config_extn = &hapd->iconf->conf_extn;
	if (!config_extn)
		return -1;

	while (cmd && isspace((unsigned char)*cmd))
		cmd++;

	if (!cmd || *cmd == '\0') {
		wpa_printf(MSG_ERROR, "Empty bitmap value");
		return -1;
	}

	errno = 0;
	tmp = strtoul(cmd, &end, 0);

	if (errno != 0 || end == cmd) {
		wpa_printf(MSG_ERROR, "Invalid bitmap value: '%s'", cmd);
		return -1;
	}

	while (*end && isspace((unsigned char)*end))
		end++;
	if (*end != '\0') {
		wpa_printf(MSG_ERROR, "Trailing garbage in bitmap value: '%s'", cmd);
		return -1;
	}

	if (tmp > 0xFFFFUL) {
		wpa_printf(MSG_ERROR, "Bitmap out of range (0..0xFFFF): 0x%lx", tmp);
		return -1;
	}

	val = (u16) tmp;

	/*
	 * Update this mask based on what bits are actually valid.
	 * Example below allows only bits 0..4 (i.e., 0x001F).
	 * For a full 16-bit bitmap with all bits valid, remove this check.
	 */
	if (val & ~0x001Fu) {
		wpa_printf(MSG_ERROR,
			   "Invalid bitmask 0x%04x (only bits 0..4 allowed)",
			   val);
		return -1;
	}

	config_extn->dcs_conf.dcs_random_chan_bitmap = val;

	wpa_printf(MSG_DEBUG, "Bitmap set to 0x%04x (%u)",
		   config_extn->dcs_conf.dcs_random_chan_bitmap,
		   config_extn->dcs_conf.dcs_random_chan_bitmap);

	return 0;
}

static int hostapd_ctrl_iface_get_dcs_enable(struct hostapd_data *hapd,
					     const char *cmd, char *reply,
					     int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int ret;

	ret = os_snprintf(reply, reply_size, "dcs_enable=%u\n",
			  conf_extn->dcs_conf.enable_bitmap);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_dcs_event_action(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_config_extn *conf_extn;
	char *end;
	unsigned long v;

	(void) reply;
	(void) reply_size;

	if (!hapd || !hapd->iconf || !cmd)
		return -1;

	while (*cmd == ' ' || *cmd == '\t')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "CTRL: DCS_EVENT_ACTION: empty value");
		return -1;
	}

	errno = 0;
	v = strtoul(cmd, &end, 0);
	while (*end == ' ' || *end == '\t')
		end++;
	if (errno != 0 || end == cmd || *end != '\0' || v > 0xFFFF) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DCS_EVENT_ACTION: invalid value '%s'", cmd);
		return -1;
	}

	if (v & ~ALLOWED_DCS_EVENT_ACTION_MASK) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DCS_EVENT_ACTION: invalid value 0x%lx (allowed bits mask: 0x%04x)",
			   v, ALLOWED_DCS_EVENT_ACTION_MASK);
		return -1;
	}

	conf_extn = &hapd->iconf->conf_extn;
	conf_extn->dcs_conf.dcs_event_action = (u16) v;

	wpa_printf(MSG_DEBUG,
		   "CTRL: DCS_EVENT_ACTION set to 0x%04x",
		   conf_extn->dcs_conf.dcs_event_action);

	return 0;
}

static int hostapd_ctrl_iface_get_dcs_event_action(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_config_extn *conf_extn;
	int ret;

	(void) cmd;

	if (!hapd || !hapd->iconf)
		return -1;

	conf_extn = &hapd->iconf->conf_extn;
	ret = os_snprintf(reply, reply_size, "dcs_event_action=0x%04x\n",
			  conf_extn->dcs_conf.dcs_event_action);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_dcs_event_notify(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_config_extn *conf_extn;
	char *end;
	unsigned long v;

	(void) reply;
	(void) reply_size;

	if (!hapd || !hapd->iconf || !cmd)
		return -1;

	while (*cmd == ' ' || *cmd == '\t')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "CTRL: DCS_EVENT_NOTIFY: empty value");
		return -1;
	}

	errno = 0;
	v = strtoul(cmd, &end, 0);
	while (*end == ' ' || *end == '\t')
		end++;
	if (errno != 0 || end == cmd || *end != '\0' || v > 0xFFFF) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DCS_EVENT_NOTIFY: invalid value '%s' (expected 16-bit value)",
			   cmd);
		return -1;
	}

	if (v & ~ALLOWED_DCS_EVENT_ACTION_MASK) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DCS_EVENT_NOTIFY: invalid value 0x%lx (allowed bits mask: 0x%04x)",
			   v, ALLOWED_DCS_EVENT_ACTION_MASK);
		return -1;
	}

	conf_extn = &hapd->iconf->conf_extn;
	conf_extn->dcs_conf.dcs_event_notify = (u16) v;

	wpa_printf(MSG_DEBUG,
		   "CTRL: DCS_EVENT_NOTIFY set to 0x%04x",
		   conf_extn->dcs_conf.dcs_event_notify);

	return 0;
}

static int hostapd_ctrl_iface_get_dcs_event_notify(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_config_extn *conf_extn;
	int ret;

	(void) cmd;

	if (!hapd || !hapd->iconf)
		return -1;

	conf_extn = &hapd->iconf->conf_extn;
	ret = os_snprintf(reply, reply_size, "dcs_event_notify=0x%04x\n",
			  conf_extn->dcs_conf.dcs_event_notify);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_dcs_reenable_time(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_iface_extn *iface_extn;
	unsigned long val_ul;
	u32 secs;
	char *end = NULL;

	(void) reply;
	(void) reply_size;

	if (!hapd || !hapd->iface)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "DCS: reenable_time: empty value");
		return -1;
	}

	errno = 0;
	val_ul = strtoul(cmd, &end, 0);
	while (end && *end == ' ')
		end++;
	if (errno != 0 || end == cmd || (end && *end != '\0') ||
	    val_ul > 0xFFFFFFFFUL) {
		wpa_printf(MSG_ERROR, "DCS: reenable_time: invalid value '%s'",
			   cmd);
		return -1;
	}

	secs = (u32) val_ul;
	if (secs < DCS_ENABLE_TIME_MIN || secs > DCS_ENABLE_TIME_MAX) {
		wpa_printf(MSG_ERROR,
			   "DCS: reenable_time: value out of range (%u..%u): %u",
			   DCS_ENABLE_TIME_MIN, DCS_ENABLE_TIME_MAX, secs);
		return -1;
	}

	iface_extn = &hapd->iface->iface_extn;
	iface_extn->dcs_re_enable_time = secs;

	wpa_printf(MSG_DEBUG, "DCS: reenable_time set to %u sec", secs);

	return 0;
}

static int hostapd_ctrl_iface_get_dcs_reenable_time(struct hostapd_data *hapd,
							    const char *cmd, char *reply,
							    int reply_size)
{
	int ret;
	unsigned int secs;

	(void) cmd;

	if (!hapd || !hapd->iface)
		return -1;

	secs = hostapd_dcs_get_reenable_time_sec(hapd->iface);

	ret = os_snprintf(reply, reply_size, "dcs_re_enable_time=%u\n", secs);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_dcs_sim(struct hostapd_data *hapd,
				      const char *cmd, char *reply,
				      int reply_size)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	struct driver_dcs_sim drv_dcs_sim;
	unsigned long val_ul;
	unsigned long intf_bitmap_ul;
	u32 intf_bitmap = 0x0;
	u16 val;
	char *end = NULL;
	char *input = NULL, *token, *saveptr = NULL;

	if (!iface_extn)
		return -1;

	(void) reply;
	(void) reply_size;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "DCS_SIM: empty value");
		return -1;
	}

	errno = 0;
	val_ul = strtoul(cmd, &end, 0);
	if (errno != 0 || end == cmd) {
		wpa_printf(MSG_ERROR, "DCS_SIM: invalid value '%s'", cmd);
		return -1;
	}
	while (end && *end == ' ')
		end++;
	if (val_ul > 0xFFFF) {
		wpa_printf(MSG_ERROR, "DCS_SIM: value out of range '%s'", cmd);
		return -1;
	}

	val = (u16) val_ul;
	if (val != DCS_CW_INTF && val != DCS_WLAN_INTF &&
	    val != DCS_AWGN_INTF && val != DCS_OBSS_INTF) {
		wpa_printf(MSG_ERROR, "DCS_SIM: invalid value 0x%04x", val);
		return -1;
	}

	if (end && *end != '\0') {
		input = os_strdup(end);
		if (!input)
			return -1;

		token = strtok_r(input, " ", &saveptr);
		while (token) {
			if (os_strncmp(token, "intf_bitmap=", 12) == 0) {
				char *bitmap_str = token + 12;
				char *bitmap_end = NULL;

				errno = 0;
				intf_bitmap_ul = strtoul(bitmap_str, &bitmap_end, 0);
				if (errno != 0 || bitmap_end == bitmap_str ||
				    *bitmap_end != '\0' ||
				    intf_bitmap_ul > 0xFFFFFFFFUL) {
					wpa_printf(MSG_ERROR,
						   "DCS_SIM: invalid intf_bitmap '%s'",
						   bitmap_str);
					os_free(input);
					return -1;
				}
				intf_bitmap = (u32) intf_bitmap_ul;
			} else {
				intf_bitmap = 0xFFFF;
			}

			token = strtok_r(NULL, " ", &saveptr);
		}
		os_free(input);
	}

	os_memset(&drv_dcs_sim, 0, sizeof(drv_dcs_sim));
	drv_dcs_sim.type = val;
	drv_dcs_sim.intf_bitmap = intf_bitmap;
	wpa_printf(MSG_DEBUG, "DCS_SIM: val=%u (0x%04x), intf_bitmap=0x%x",
		   val, val, intf_bitmap);

	return hostapd_drv_dcs_sim_trigger(hapd, hapd->mld_link_id, &drv_dcs_sim);
}

static int hostapd_ctrl_iface_g_dcs_random_chan_en(struct hostapd_config_extn
						   *conf_extn, const char *pos,
						   char *reply, size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "DCS get_random_chan_en: 0x%x\n",
			      conf_extn->dcs_conf.dcs_random_chan_bitmap);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_g_dcs_csa_tbtt(struct hostapd_config_extn *conf_extn,
					     const char *pos, char *reply,
					     size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "DCS get_csa_tbtt: %d\n",
			      conf_extn->dcs_conf.dcs_csa_tbtt);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_g_dcs_bw_reduction_ctrl(struct hostapd_config_extn
							*conf_extn, const char *pos,
							char *reply,size_t reply_size)
{
	int ret = os_snprintf(reply, reply_size,
			      "DCS get_bw_reduction_ctrl: 0x%x\n",
			      conf_extn->dcs_conf.bw_reduction_ctrl);

	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

int hostapd_ctrl_iface_dcs_extn(struct hostapd_data *hapd,
				const char *cmd, char *reply,
				int reply_size)
{
	struct hostapd_config_extn *conf_extn;

	if (!hapd->iface || !hapd->iface->conf)
		return -1;

	conf_extn  = &hapd->iface->conf->conf_extn;

	if (os_strncmp(cmd, "enable ", 7) == 0) {
		return hostapd_ctrl_iface_dcs_config(hapd, cmd + 7,
						     reply, reply_size);
	} else if (os_strncmp(cmd, "bw_reduction_ctrl ", 18) == 0) {
		return hostapd_ctrl_iface_set_dcs_bw_reduction_ctrl(hapd,
								    cmd + 18,
								    reply, reply_size);
	} else if (os_strncmp(cmd, "csa_tbtt ", 9) == 0) {
		return hostapd_ctrl_iface_set_dcs_csa_tbtt(hapd, cmd + 9,
							   reply, reply_size);
	} else if (os_strncmp(cmd, "set_wlan_intr_params ", 21) == 0) {
		return hostapd_ctrl_iface_set_wlan_intr_params(hapd, cmd + 21,
							       reply,
							       reply_size);
	} else if (os_strcmp(cmd, "get_wlan_intr_params") == 0) {
		return hostapd_ctrl_iface_get_wlan_intr_params(hapd, cmd, reply,
							       reply_size);
	} else if (os_strncmp(cmd, "random_chan_bitmap ", 19) == 0) {
		return hostapd_ctrl_iface_set_random_chan_en(hapd, cmd + 19,
							     reply, reply_size);
	} else if (os_strncmp(cmd, "event_action ", 13) == 0) {
		return hostapd_ctrl_iface_set_dcs_event_action(hapd, cmd + 13,
							       reply, reply_size);
	} else if (os_strncmp(cmd, "event_notify ", 13) == 0) {
		return hostapd_ctrl_iface_set_dcs_event_notify(hapd, cmd + 13,
							       reply, reply_size);
	} else if (os_strcmp(cmd, "get_enable") == 0) {
		return hostapd_ctrl_iface_get_dcs_enable(hapd, cmd, reply,
							 reply_size);
	} else if (os_strcmp(cmd, "get_event_action") == 0) {
		return hostapd_ctrl_iface_get_dcs_event_action(hapd, cmd,
							       reply, reply_size);
	} else if (os_strcmp(cmd, "get_event_notify") == 0) {
		return hostapd_ctrl_iface_get_dcs_event_notify(hapd, cmd,
							       reply, reply_size);
	} else if (os_strncmp(cmd, "set_dcs_enable_timer ", 21) == 0) {
		return hostapd_ctrl_iface_set_dcs_reenable_time(hapd, cmd + 21,
								reply,
								reply_size);
	} else if (os_strcmp(cmd, "get_dcs_enable_timer") == 0) {
		return hostapd_ctrl_iface_get_dcs_reenable_time(hapd, cmd, reply,
								reply_size);
	} else if (os_strncmp(cmd, "sim ", 4) == 0) {
		return hostapd_ctrl_iface_dcs_sim(hapd, cmd + 4,
						  reply, reply_size);
	} else if (os_strcmp(cmd, "get_random_chan_en") == 0) {
		return hostapd_ctrl_iface_g_dcs_random_chan_en(conf_extn, cmd,
							       reply, reply_size);
	} else if (os_strcmp(cmd, "get_csa_tbtt") == 0) {
		return hostapd_ctrl_iface_g_dcs_csa_tbtt(conf_extn, cmd, reply,
							 reply_size);
	} else if (os_strcmp(cmd, "get_bw_reduction_ctrl") == 0) {
		return hostapd_ctrl_iface_g_dcs_bw_reduction_ctrl(conf_extn, cmd,
								  reply,
								  reply_size);
	} else {
		return dcs_print_usage_extn(reply, reply_size);
	}
}

int find_random_channel(struct hostapd_iface *iface, int *new_freq,
			u32 freq, u32 cf1, u32 cf2, u32 chan_bw_interference_bitmap,
			enum chan_width chan_width, int *new_chan_width,
			int *new_centre_freq)
{
	int ret;
	u32 _rand;
	u32 chan_idx;
	unsigned int i;
	int temp_width;
	int current_start_freq;
	int num_available_chandefs = 0;
	struct hostapd_channel_data *chan_data = NULL;
	struct hostapd_hw_modes *mode = iface->current_mode;
	int cw_interference_freqs[BW_INTERFERENCE_MAXBITS] = {};
	struct hostapd_channel_data **available_chandef_list = NULL;

	available_chandef_list = os_zalloc(sizeof(struct hostapd_channel_data *) *
					   mode->num_channels);

	if (!available_chandef_list) {
		wpa_printf(MSG_ERROR, "available_chandef_list memory allocation failed");
		return -1;
	}

	current_start_freq = (cf1 - channel_width_to_int(chan_width) / 2) + 10;
	for (i = 0; i < BW_INTERFERENCE_MAXBITS; i++) {
		if ((1 << i) & chan_bw_interference_bitmap) {
			wpa_printf(MSG_DEBUG,
				   "CW: found cw interference in frequency %d",
				   current_start_freq + (20 * i));
			cw_interference_freqs[i] = current_start_freq + (20 * i);
		}
	}

	temp_width = chan_width;

	while (temp_width > CHAN_WIDTH_20_NOHT) {
		num_available_chandefs =
			intf_awgn_find_channel_list(iface, temp_width,
						    &available_chandef_list,
						    cw_interference_freqs);
		if (num_available_chandefs > 0)
			break;
		temp_width = get_next_max_width(temp_width);
	}

	if (num_available_chandefs == 0) {
		wpa_printf(MSG_ERROR, "CW Intf: no available_chandefs");
		goto exit;
	}


	if (os_get_random((u8 *)&_rand, sizeof(_rand)) < 0) {
		wpa_printf(MSG_ERROR, "CW Intf: couldn't get random number");
		goto exit;
	}

	chan_idx = _rand % num_available_chandefs;

	chan_data = available_chandef_list[chan_idx];

	if (!chan_data) {
		wpa_printf(MSG_ERROR, "CW Intf: channel info not available for chan_idx : %d",
				chan_idx);
		goto exit;
	}

	*new_chan_width = temp_width;

	wpa_printf(MSG_DEBUG, "CW Interference: got random channel %d (%d)",
		   chan_data->freq, chan_data->chan);

	if (*new_chan_width > CHAN_WIDTH_20) {
		ret = get_centre_freq(chan_data, *new_chan_width,
				      new_centre_freq);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "CW : couldn't find centre freq for chan : %d"
				   " chan_width : %d", chan_data->chan, *new_chan_width);
			goto exit;
		}
	} else {
		*new_centre_freq = chan_data->freq;
	}

	*new_freq = chan_data->freq;

	return 0;

exit:
	os_free(available_chandef_list);
	return -1;
}

int hostapd_dcs_channel_change(struct csa_settings *settings,
                               struct hostapd_iface *iface, int new_chan_width,
                               int new_centre_freq)
{
	int i, ret = 0;
	u16 prev_punct_bitmap;

	wpa_printf(MSG_DEBUG,
		   "DCS: channel_change req freq=%d cf1=%d width=%d cur_freq=%d cur_cf1=%d cur_width=%d",
		   settings->freq_params.freq, new_centre_freq, new_chan_width,
		   iface->freq, iface->conf->conf_extn.cur_chan_params.cf1,
		   iface->conf->conf_extn.cur_chan_params.chan_width);

	settings->cs_count = iface->conf->conf_extn.dcs_conf.dcs_csa_tbtt;
#ifdef RDK_ONEWIFI
	if (!settings->cs_count)
		settings->cs_count = DCS_CSA_TBTT_DEFAULT;
#endif

	switch (new_chan_width) {
	case CHAN_WIDTH_40:
		settings->freq_params.bandwidth = 40;
		break;
	case CHAN_WIDTH_80P80:
	case CHAN_WIDTH_80:
		settings->freq_params.bandwidth = 80;
		break;
	case CHAN_WIDTH_160:
		settings->freq_params.bandwidth = 160;
		break;
	case CHAN_WIDTH_320:
		settings->freq_params.bandwidth = 320;
		break;
	default:
		settings->freq_params.bandwidth = 20;
		break;
	}

	settings->freq_params.center_freq1 = new_centre_freq;
	settings->freq_params.ht_enabled = iface->conf->ieee80211n;
	settings->freq_params.vht_enabled = iface->conf->ieee80211ac;
	settings->freq_params.he_enabled = iface->conf->ieee80211ax;
	settings->freq_params.eht_enabled= iface->conf->ieee80211be;
	settings->power_mode = -1;

	if (settings->freq_params.freq == iface->freq &&
	    new_chan_width == iface->conf->conf_extn.cur_chan_params.chan_width &&
	    settings->freq_params.center_freq1 ==
		    iface->conf->conf_extn.cur_chan_params.cf1 &&
	    settings->freq_params.punct_bitmap == iface->conf->punct_bitmap) {
		wpa_printf(MSG_ERROR, "DCS: CSA skipped (same channel)");
		hostapd_dcs_restore_extn(iface, "CSA skipped (same channel)");
		return 0;
	}

	if (is_6ghz_freq(settings->freq_params.freq) &&
			 iface->conf->enable_best_power_mode) {
		int best_power_mode;

		best_power_mode =
			hostapd_get_best_ap_6ghz_power_mode(iface,
					settings->freq_params.freq,
					settings->freq_params.center_freq1,
					settings->freq_params.bandwidth,
					settings->freq_params.punct_bitmap);
		if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
			settings->power_mode = best_power_mode;
			wpa_printf(MSG_DEBUG, "%s: Best power mode for Freq %d is %d",
					__func__,
					settings->freq_params.freq,
					settings->power_mode);
		} else {
			wpa_printf(MSG_DEBUG, "%s: Failed to get BPM for Freq %d",
					__func__, settings->freq_params.freq);
			return -1;
		}
	}

	prev_punct_bitmap = iface->conf->conf_extn.cur_chan_params.punct_bitmap;
	iface->conf->conf_extn.cur_chan_params.punct_bitmap =
		settings->freq_params.punct_bitmap;

	for (i = 0; i < iface->num_bss; i++) {
		hostapd_chan_switch_config(iface->bss[i],
				&settings->freq_params);

		wpa_printf(MSG_DEBUG,
			   "channel=%u, freq=%d, bw=%d, center_freq1=%d puncture_bitmap=0x%x",
			   settings->freq_params.channel,
			   settings->freq_params.freq,
			   settings->freq_params.bandwidth,
			   settings->freq_params.center_freq1,
			   settings->freq_params.punct_bitmap);

		ret = hostapd_switch_channel(iface->bss[i], settings);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "DCS: hostapd_switch_channel failed for bss[%d] (ret=%d)",
				   i, ret);
			break;
		}
	}

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "DCS: channel switch failed; restoring puncture bitmap 0x%x -> 0x%x",
			   settings->freq_params.punct_bitmap, prev_punct_bitmap);
		iface->conf->conf_extn.cur_chan_params.punct_bitmap =
			prev_punct_bitmap;
	}

	return ret;
}

static u16 hostapd_dcs_find_legitimate_puncture_pattern(u16 pp, int freq,
							int center_freq, u16 bw)
{
	const u16 *pp_arr;
	u16 num_pp = 0;
	u16 pp_mask = 0;
	u16 pri_chan_pos = 0;
	u16 max_subch;
	u16 i;
	int start_freq;

	pp_arr = hostapd_get_valid_puncture_pattern_arr(bw, &num_pp, &pp_mask);
	pp &= pp_mask;

	if (!pp_arr || !num_pp)
		return 0;

	start_freq = (bw == 20) ? freq : center_freq - (bw / 2) + 10;
	if (freq < start_freq)
		return PUNCTURE_INVALID;

	pri_chan_pos = (freq - start_freq) / 20;
	max_subch = bw / 20;
	if (!max_subch || pri_chan_pos >= max_subch)
		return PUNCTURE_INVALID;

	if (is_punct_bitmap_valid(bw, pri_chan_pos, pp))
		return pp;

	for (i = 0; i < num_pp; i++) {
		if (pp_arr[i] == ((pp | pp_arr[i]) & pp_mask) &&
		    is_punct_bitmap_valid(bw, pri_chan_pos, pp_arr[i]))
			return pp_arr[i];
	}

	return PUNCTURE_INVALID;
}

static const char *hostapd_dcs_intf_type_str(u16 type)
{
	switch (type) {
	case DCS_CW_INTF:
		return "CW";
	case DCS_WLAN_INTF:
		return "WLAN";
	case DCS_AWGN_INTF:
		return "AWGN";
	case DCS_OBSS_INTF:
		return "OBSS";
	default:
		return NULL;
	}
}

void hostapd_dcs_intf_event_extn(struct hostapd_data *hapd,
				 union wpa_event_data *data)
{
	enum chan_width ch_width;
	u16 type;
	u32 freq, cf1, cf2, intf_bitmap, bw;
	struct dcs_intf_event *dcs_intf_event = &data->event_data_extn.dcs_intf_event;
	struct hostapd_iface *iface;
	struct hostapd_data *link_hapd;
	struct csa_settings settings ={};
	int new_chan_width, new_centre_freq, new_freq, ret;
	u8 rand_chan_bitmap;
	u16 event_notify;
	struct hostapd_iface_extn *iface_extn;
	u16 enable_bitmap;
	u16 event_action;
	const char *intf_type_str;
	u32 report_freq, report_cf1, report_cf2, report_bitmap;
	enum chan_width report_chan_width;

	link_hapd = switch_link_hapd(hapd, dcs_intf_event->link_id);
	if (!link_hapd)
		return;

	iface = link_hapd->iface;
	wpa_printf(MSG_ERROR,
		   "DCS: event rx type=0x%04x link_id=%u",
		   dcs_intf_event->type, dcs_intf_event->link_id);
	iface_extn = &iface->iface_extn;
	freq = iface->freq;
	cf1 = iface->conf->conf_extn.cur_chan_params.cf1;
	cf2 = iface->conf->conf_extn.cur_chan_params.cf2;
	ch_width = iface->conf->conf_extn.cur_chan_params.chan_width;
	type = dcs_intf_event->type;
	intf_type_str = hostapd_dcs_intf_type_str(type);
	report_freq = dcs_intf_event->freq ? dcs_intf_event->freq : freq;
	report_chan_width = dcs_intf_event->chan_width ?
		dcs_intf_event->chan_width : ch_width;
	report_cf1 = dcs_intf_event->cf1 ? dcs_intf_event->cf1 : cf1;
	report_cf2 = dcs_intf_event->cf2 ? dcs_intf_event->cf2 : cf2;
	report_bitmap = dcs_intf_event->chan_bw_interference_bitmap;


	event_notify = iface->conf->conf_extn.dcs_conf.dcs_event_notify;

	rand_chan_bitmap = iface->conf->conf_extn.dcs_conf.dcs_random_chan_bitmap;
	enable_bitmap = iface->conf->conf_extn.dcs_conf.enable_bitmap;
	event_action = iface->conf->conf_extn.dcs_conf.dcs_event_action;
	wpa_printf(MSG_ERROR,
		   "DCS: enable_bitmap=0x%04x event_action=0x%04x rand_chan_bitmap=0x%02x dcs_in_progress=%d",
		   enable_bitmap, event_action, rand_chan_bitmap,
		   iface_extn->dcs_in_progress);

	if ((event_notify & type) && intf_type_str && iface->bss && iface->bss[0] &&
	    iface->bss[0]->msg_ctx)
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, INTERFERENCE_DETECTED
			"type=%s freq=%u chan_width=%d cf1=%u cf2=%u bitmap=0x%x",
			intf_type_str, report_freq, report_chan_width,
			report_cf1, report_cf2, report_bitmap);

	if (!(event_action & type)) {
		wpa_printf(MSG_ERROR,
			   "DCS: discarding interference event type=0x%04x since dcs_event_action bit is not set (dcs_event_action=0x%04x)",
			   type, event_action);
		return;
	}

	if (type == DCS_WLAN_INTF &&
	    iface_extn->dcs_disabled_excessive_triggers) {
		wpa_printf(MSG_ERROR,
			   "DCS: ignoring WLAN intf event (auto-disabled)");
		return;
	}

	if (!enable_bitmap) {
		wpa_printf(MSG_ERROR,
			   "DCS: ignoring interference event (DCS disabled)");
		return;
	}

	if (!(enable_bitmap & type)) {
		wpa_printf(MSG_ERROR,
			   "DCS: ignoring interference event, not enabled; enable_bitmap=0x%04x)",
			   enable_bitmap);
		return;
	}

	if (iface_extn->dcs_in_progress) {
		wpa_printf(MSG_ERROR,
			   "DCS: interference event ignored (dcs in progress)");
		return;
	}

	if (type == DCS_OBSS_INTF &&
	    hostapd_is_bh_sta_connecting_or_connected_extn(iface)) {
		wpa_printf(MSG_ERROR,
			   "DCS: dropping OBSS event in repeater mode");
		return;
	}

	if ((type == DCS_CW_INTF || type == DCS_WLAN_INTF) &&
	    !(rand_chan_bitmap & type)) {
		int acs_ret;
#ifdef RDK_ONEWIFI
		if (type == DCS_WLAN_INTF)
			hostapd_dcs_rate_limit_update(link_hapd->iface, type);

		/* If rate-limit just disabled WLAN interference handling,
		 * do not start ACS */
		if (type == DCS_WLAN_INTF &&
		    iface_extn->dcs_disabled_excessive_triggers)
			return;
#endif

		if (!hostapd_dcs_set_in_progress(iface, type))
			return;

		acs_ret = hostapd_cbs_trigger_csa(link_hapd);
		if (acs_ret)
			acs_ret = hostapd_trigger_dynamic_acs(link_hapd,
				CHANNEL_CHANGE_CSA);

		if (acs_ret) {
			hostapd_dcs_restore_extn(iface,
						 "dynamic ACS start failed");
			return;
		}
#ifndef RDK_ONEWIFI
		if (type == DCS_WLAN_INTF)
			hostapd_dcs_rate_limit_update(link_hapd->iface, type);
#endif
		return;
	}

	if (type != DCS_OBSS_INTF) {
		intf_bitmap = dcs_intf_event->chan_bw_interference_bitmap ?
			      dcs_intf_event->chan_bw_interference_bitmap :
			      DCS_SEG_PRI20;

		ret = find_random_channel(iface, &new_freq, freq, cf1, cf2,
					  intf_bitmap, ch_width, &new_chan_width,
					  &new_centre_freq);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "finding random channel failed, dropping event");
			return;
		}

		settings.freq_params.freq = new_freq;
	} else {
		u16 obss_pp = 0;
		u16 op_pp = 0;
		u16 leg_pp;
		intf_bitmap = dcs_intf_event->chan_bw_interference_bitmap;
		settings.freq_params.freq = freq;
		new_chan_width = ch_width;
		new_centre_freq = cf1;

		bw = channel_width_to_int(new_chan_width);

		obss_pp = intf_bitmap;

		leg_pp = hostapd_dcs_find_legitimate_puncture_pattern(
					    obss_pp, freq, cf1, bw);
		if (leg_pp == PUNCTURE_INVALID) {
			wpa_printf(MSG_DEBUG,
				   "DCS: OBSS bitmap 0x%x invalid, retry with intersection",
				   obss_pp);
			op_pp = iface->conf->conf_extn.cur_chan_params.punct_bitmap;
			if (!op_pp)
				op_pp = iface->conf->punct_bitmap;
			obss_pp &= op_pp;
			leg_pp = hostapd_dcs_find_legitimate_puncture_pattern(
					    obss_pp, freq, cf1, bw);
			if (leg_pp == PUNCTURE_INVALID) {
				wpa_printf(MSG_ERROR,
					   "Puncture bitmap is invalid, dropping this event");
				return;
			}
		}
		if (leg_pp != obss_pp) {
			wpa_printf(MSG_DEBUG,
				   "DCS: adjusted puncture bitmap 0x%x -> 0x%x",
				   obss_pp, leg_pp);
		}
		intf_bitmap = obss_pp;
		wpa_printf(MSG_DEBUG,
			   "DCS: OBSS final puncture bitmap 0x%x (obss=0x%x)",
			   leg_pp, obss_pp);

		settings.freq_params.punct_bitmap = leg_pp;
	}

	wpa_printf(MSG_ERROR, "type=%d, input freq=%d, ch_width=%d, cf1=%d cf2=%d intf_bitmap:0x%x", type, freq, ch_width, cf1, cf2, intf_bitmap);

	if (!hostapd_dcs_set_in_progress(iface, type))
		return;

	if (hostapd_is_bh_sta_connecting_or_connected_extn(iface)) {
		if (iface->conf->conf_extn.rptr_allow_chan_sw) {
			wpa_printf(MSG_DEBUG, "Rptr BH STA is connected, "
				   "disconnect BH STA and allow DCS channel switch");
			hostapd_ucode_trigger_bhsta_disconnect(iface);
		} else {
			wpa_printf(MSG_ERROR, "Rptr BH STA is connected, "
				   "discard DCS channel switch");
			return;
		}
	}

#ifdef RDK_ONEWIFI
	/* For MLO: set link_id so the driver sends NL80211_ATTR_MLO_LINK_ID
	 * for the correct link. Use link_hapd->mld_link_id (set by rdk-wifi-hal
	 * to radio->rdk_radio_index) rather than iface->bss[0]->mld_link_id. */
	settings.freq_params.link_id = link_hapd->mld_link_id;
#endif
	ret = hostapd_dcs_channel_change(&settings, link_hapd->iface, new_chan_width, new_centre_freq);
	if (ret) {
		hostapd_dcs_restore_extn(iface, "CSA trigger failed");
		return;
	}

	if (type == DCS_WLAN_INTF)
		hostapd_dcs_rate_limit_update(link_hapd->iface, type);

	return;
}

void update_chan_params(struct hostapd_data *hapd, int cf1, int cf2, enum chan_width chwidth)
{
	hapd->iface->conf->conf_extn.cur_chan_params.cf1 = cf1;
        hapd->iface->conf->conf_extn.cur_chan_params.cf2 = cf2;
        hapd->iface->conf->conf_extn.cur_chan_params.chan_width = chwidth;
	hapd->iface->conf->conf_extn.cur_chan_params.punct_bitmap =
		hapd->iface->conf->punct_bitmap;
}

bool dcs_get_bw_reduction_ctrl_extn(struct hostapd_config *conf,
				   u16 dcs_intf_type)
{
	struct hostapd_config_extn *conf_extn;

	if (!conf)
		return true;

	conf_extn = &conf->conf_extn;

	wpa_printf(MSG_DEBUG, "bw_reduction_ctrl=%u (0x%04x)",
		   conf_extn->dcs_conf.bw_reduction_ctrl,
		   conf_extn->dcs_conf.bw_reduction_ctrl);

	if (conf_extn->dcs_conf.bw_reduction_ctrl & dcs_intf_type)
		return true;

	return false;
}

bool hostapd_dcs_awgn_handle_rand_chan_disabled_extn(struct hostapd_iface *iface)
{
	struct hostapd_data *link_hapd;
	int acs_ret;

	if (!iface || !iface->conf || !iface->bss || iface->num_bss < 1)
		return false;

	if (iface->conf->conf_extn.dcs_conf.dcs_random_chan_bitmap & DCS_AWGN_INTF)
		return false;

	link_hapd = iface->bss[0];

	if (!link_hapd) {
		wpa_printf(MSG_ERROR,
			   "AWGN: random channel selection disabled but no link hapd available");
		return false;
	}

	wpa_printf(MSG_DEBUG, "AWGN: triggering CSA/ACS");

	acs_ret = hostapd_cbs_trigger_csa(link_hapd);
	if (acs_ret)
		acs_ret = hostapd_trigger_dynamic_acs(link_hapd,
					     CHANNEL_CHANGE_CSA);

	if (acs_ret) {
		wpa_printf(MSG_ERROR,
			   "AWGN: failed to start CSA/ACS when random channel selection disabled");
		return false;
	}

	return true;
}

struct hostapd_channel_data *
get_chan_data_by_freq(struct hostapd_hw_modes *mode, int freq)
{
	int i;

	if (!mode)
		return NULL;

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == freq)
			return &mode->channels[i];
	}

	return NULL;
}

bool awgn_bw_range_available(struct hostapd_hw_modes *mode,
		struct hostapd_channel_data *primary,
		int chan_width)
{
	int centre_freq, start_freq, end_freq, freq;

	if (!mode || !primary)
		return false;

	if (chan_width <= CHAN_WIDTH_20)
		return chan_pri_allowed(primary);

	if (get_centre_freq_6g(primary->chan, chan_width, &centre_freq))
		return false;

	start_freq = (centre_freq - channel_width_to_int(chan_width) / 2) + 10;
	end_freq = (centre_freq + channel_width_to_int(chan_width) / 2) - 10;

	for (freq = start_freq; freq <= end_freq; freq += 20) {
		struct hostapd_channel_data *ch;

		ch = get_chan_data_by_freq(mode, freq);
		if (!ch || (ch->flag & HOSTAPD_CHAN_DISABLED))
			return false;
	}

	return true;
}

static bool
awgn_bw_hwbl_available(struct hostapd_iface *iface,
		       struct hostapd_channel_data *chan_data,
		       int chan_width)
{
	int center_freq;
	int bw;
	int target_pwr_mode;

	if (!iface || !chan_data)
		return false;

	if (!is_6ghz_freq(chan_data->freq))
		return true;

	if (get_centre_freq(chan_data, chan_width, &center_freq))
		return false;

	bw = channel_width_to_int(chan_width == CHAN_WIDTH_20_NOHT ?
				  CHAN_WIDTH_20 : chan_width);
	if (bw <= 0)
		return false;

	target_pwr_mode = iface->conf->he_6ghz_reg_pwr_type;
	if (iface->conf->enable_best_power_mode) {
		int best_power_mode;

		best_power_mode =
			hostapd_get_best_ap_6ghz_power_mode(iface,
							    chan_data->freq,
							    center_freq, bw, 0);
		if (best_power_mode < NL80211_REG_NUM_POWER_MODES)
			target_pwr_mode = best_power_mode;
	}

	if (target_pwr_mode >= NL80211_REG_NUM_POWER_MODES)
		return false;

	return hostapd_validate_chan_bw_in_pwr_mode(iface, chan_data->freq,
						     center_freq, bw, 0,
						     target_pwr_mode);
}

void reduced_chan_width(struct hostapd_iface *iface, int *new_chan_width,
			int chan_width, int freq,
			struct hostapd_hw_modes *mode,
			u32 chan_bw_interference_bitmap)
{
	struct hostapd_channel_data *chan_data;

	if (!new_chan_width) {
		wpa_printf(MSG_ERROR, "AWGN: reduced_chan_width: invalid args");
		return;
	}

	chan_data = get_chan_data_by_freq(mode, freq);
	if (!chan_data) {
		wpa_printf(MSG_ERROR,
			   "AWGN: reduced_chan_width: no channel found for freq %d",
			   freq);
		*new_chan_width = chan_width;
		return;
	}

	if ((chan_width > CHAN_WIDTH_160) &&
	    !(chan_bw_interference_bitmap & DCS_SEG_SEC80) &&
	    !(chan_bw_interference_bitmap & DCS_SEG_SEC40) &&
	    !(chan_bw_interference_bitmap & DCS_SEG_SEC20)) {
		*new_chan_width = CHAN_WIDTH_160;
	} else if ((chan_width > CHAN_WIDTH_80) &&
		   !(chan_bw_interference_bitmap & DCS_SEG_SEC40) &&
		   !(chan_bw_interference_bitmap & DCS_SEG_SEC20)) {
		*new_chan_width = CHAN_WIDTH_80;
	} else if (chan_width > CHAN_WIDTH_40 &&
		   !(chan_bw_interference_bitmap & DCS_SEG_SEC20)) {
		*new_chan_width = CHAN_WIDTH_40;
	} else {
		*new_chan_width = CHAN_WIDTH_20;
	}

	while (*new_chan_width > CHAN_WIDTH_20 &&
	       (!awgn_bw_range_available(mode, chan_data, *new_chan_width) ||
		!awgn_bw_hwbl_available(iface, chan_data, *new_chan_width)))
		*new_chan_width = get_next_max_width(*new_chan_width);

	if (!awgn_bw_hwbl_available(iface, chan_data, *new_chan_width))
		*new_chan_width = chan_width;

	wpa_printf(MSG_DEBUG,
		   "AWGN: reduced bandwidth %d -> %d on channel %d (%d) bitmap=0x%x",
		   chan_width, *new_chan_width, chan_data->freq, chan_data->chan,
		   chan_bw_interference_bitmap);
}

/*
 * intf_chan_range_available_5g - check whether the channel can operate
 * in the given bandwidth in 5 GHz.
 * @first_chan_idx - channel index of the first 20 MHz channel in a segment
 * @num_chans - number of 20 MHz channels needed for the operating bandwidth
 */
int intf_chan_range_available_5g(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans)
{
	struct hostapd_channel_data *first_chan;
	int allowed_40_5g[] = { 36, 44, 52, 60, 100, 108, 116, 124, 132, 140,
		149, 157, 165, 173 };
	int allowed_80_5g[] = { 36, 52, 100, 116, 132, 149, 165 };
	int allowed_160_5g[] = { 36, 100, 149 };
	int *allowed_arr = NULL;
	int allowed_arr_size = 0;
	int i;

	if (!mode || first_chan_idx < 0 || first_chan_idx >= mode->num_channels)
		return 0;

	first_chan = &mode->channels[first_chan_idx];
	if (!is_5ghz_freq(first_chan->freq))
		return 0;

	if (!chan_pri_allowed(first_chan)) {
		wpa_printf(MSG_DEBUG,
				"INTF: 5 GHz primary channel not allowed");
		return 0;
	}

	/* 20 MHz channel, so no need to check the range */
	if (num_chans == 1)
		return 1;

	switch (num_chans) {
		case 2:
			allowed_arr_size = ARRAY_SIZE(allowed_40_5g);
			allowed_arr = allowed_40_5g;
			break;
		case 4:
			allowed_arr_size = ARRAY_SIZE(allowed_80_5g);
			allowed_arr = allowed_80_5g;
			break;
		case 8:
			allowed_arr_size = ARRAY_SIZE(allowed_160_5g);
			allowed_arr = allowed_160_5g;
			break;
		default:
			return 0;
	}

	for (i = 0; i < allowed_arr_size; i++) {
		if (first_chan->chan == allowed_arr[i])
			break;
	}

	if (i == allowed_arr_size)
		return 0;

	/* Check whether all the 20 MHz channels in the given operating range are enabled */
	for (i = 1; i <= num_chans - 1; i++) {
		if (is_chan_disabled(mode, first_chan->chan + i * 4))
			return 0;
	}

	return 1;
}

/*
 * intf_chan_range_available_2g - check whether the channel can operate
 * in the given bandwidth in 2.4 GHz.
 * @first_chan_idx - channel index of the first 20 MHz channel in a segment
 * @num_chans - number of 20 MHz channels needed for the operating bandwidth
 *
 * Note: For 2.4 GHz, only 20 MHz and 40 MHz (HT40+) are considered here. The
 * first channel is treated as the primary 20 MHz channel and the 40 MHz range
 * is assumed to extend "above" (i.e., secondary channel at +4).
 */
int intf_chan_range_available_2g(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans)
{
	struct hostapd_channel_data *first_chan;
	int i;

	if (!mode || first_chan_idx < 0 || first_chan_idx >= mode->num_channels)
		return 0;

	first_chan = &mode->channels[first_chan_idx];
	if (!is_24ghz_freq(first_chan->freq))
		return 0;

	if (!chan_pri_allowed(first_chan)) {
		wpa_printf(MSG_DEBUG,
			   "INTF: 2.4 GHz primary channel not allowed");
		return 0;
	}

	/* 20 MHz channel, so no need to check the range */
	if (num_chans == 1)
		return 1;

	/* 2.4 GHz does not support > 40 MHz */
	if (num_chans != 2)
		return 0;

	/* HT40+ is possible only for primary channels 1..9 (secondary at +4). */
	if (first_chan->chan < 1 || first_chan->chan > 9)
		return 0;

	/* Check whether the secondary 20 MHz channel is enabled */
	for (i = 1; i <= num_chans - 1; i++) {
		if (is_chan_disabled(mode, first_chan->chan + i * 4))
			return 0;
	}

	return 1;
}

static int get_centre_freq_from_first_freq(int first_freq, int chan_width,
					   int *centre_freq)
{
	int bw;

	if (!centre_freq)
		return -1;

	*centre_freq = 0;

	if (chan_width == CHAN_WIDTH_80P80)
		return -1;

	bw = channel_width_to_int(chan_width);
	if (bw <= 0)
		return -1;

	/* Centre frequency from first 20 MHz channel centre frequency */
	*centre_freq = first_freq + (bw / 2) - 10;
	return 0;
}

int is_chan_range_available(struct hostapd_hw_modes *mode,
				   int first_chan_idx, int num_chans)
{
	struct hostapd_channel_data *first_chan;

	if (!mode || first_chan_idx < 0 || first_chan_idx >= mode->num_channels)
		return 0;

	first_chan = &mode->channels[first_chan_idx];
	if (is_6ghz_freq(first_chan->freq))
		return intf_chan_range_available_6g(mode, first_chan_idx, num_chans);
	if (is_5ghz_freq(first_chan->freq))
		return intf_chan_range_available_5g(mode, first_chan_idx, num_chans);
	if (is_24ghz_freq(first_chan->freq))
		return intf_chan_range_available_2g(mode, first_chan_idx, num_chans);

	return 0;
}

int get_centre_freq(struct hostapd_channel_data *first_chan,
		    int chan_width, int *centre_freq)
{
	if (chan_width == CHAN_WIDTH_20_NOHT)
		chan_width = CHAN_WIDTH_20;

	return get_centre_freq_from_first_freq(first_chan->freq, chan_width,
					       centre_freq);
}

void dcs_enable_init(struct hostapd_data *hapd, u16 enable_bitmap)
{
	struct driver_dcs_config drv_dcs_conf;

#ifdef RDK_ONEWIFI
	struct hostapd_config_extn *conf_extn;
#endif
	os_memset(&drv_dcs_conf, 0, sizeof(drv_dcs_conf));
	drv_dcs_conf.dcs_enable = enable_bitmap;
	drv_dcs_conf.cmd_type = SET_DCS_CONFIG;

#ifdef RDK_ONEWIFI
	/* Send current threshold defaults to firmware on enable */
	if (hapd && hapd->iconf) {
		conf_extn = &hapd->iconf->conf_extn;
		drv_dcs_conf.valid_mask =
			DCS_VALID_INTR_DET_THR |
			DCS_VALID_PHYERR_PENALTY |
			DCS_VALID_PHYERR_THR |
			DCS_VALID_RADARERR_THR |
			DCS_VALID_TXERR_THR |
			DCS_VALID_SAMPLE_SIZE |
			DCS_VALID_COCH_THR |
			DCS_VALID_USER_MAX_CU;
		drv_dcs_conf.intr_detection_threshold =
			conf_extn->dcs_conf.intr_detection_threshold;
		drv_dcs_conf.phyerr_penalty = conf_extn->dcs_conf.phyerr_penalty;
		drv_dcs_conf.phyerr_threshold = conf_extn->dcs_conf.phyerr_threshold;
		drv_dcs_conf.radarerr_threshold =
			conf_extn->dcs_conf.radarerr_threshold;
		drv_dcs_conf.txerr_threshold = conf_extn->dcs_conf.txerr_threshold;
		drv_dcs_conf.sample_size = conf_extn->dcs_conf.sample_size;
		drv_dcs_conf.coch_intr_threshold =
			conf_extn->dcs_conf.coch_intr_threshold;
		drv_dcs_conf.user_max_cu = conf_extn->dcs_conf.user_max_cu;
	}
#endif
	wpa_printf(MSG_DEBUG, "Setting DCS enable value: 0x%04x",
		   drv_dcs_conf.dcs_enable);

	if (hostapd_drv_dcs_config(hapd, hapd->mld_link_id, &drv_dcs_conf) < 0)
		wpa_printf(MSG_ERROR, "DCS enable configuration failed");
}
