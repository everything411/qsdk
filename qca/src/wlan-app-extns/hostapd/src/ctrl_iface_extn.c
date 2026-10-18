// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "esp.h"
#include "dcs.h"
#include "cmn.h"
#include "utils/os.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "ap/ap_config.h"
#include "ap/beacon.h"
#include "ap/dfs.h"
#include "ap/hw_features.h"
#include "ap/ap_drv_ops.h"
#include "hostapd_rptr_extn.h"
#include "block_channel.h"
#include "cmn.h"
#include "cbs.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "mapc_extn.h"

#define DEF_VLP_NON_PRIOR_PENALTY	30
#define DEF_OBSS_INTERVAL		300

/**
 * hostapd_ctrl_get_hw_info_extn - Return current hardware info
 * @hapd: Pointer to the hostapd instance
 * @buf: Caller-provided output buffer to receive stringified hw info
 * @buflen: Size of @buf in bytes
 *
 * Return: Number of bytes written to @buf (excluding null terminator) on success,
 *         or -1 on failure (e.g., invalid arguments or buffer too small).
 */
int hostapd_ctrl_get_hw_info_extn(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	int ret = -1;
	if (!hapd || !hapd->iface)
		return ret;

	if (hapd->iface->current_hw_info) {
		ret = os_snprintf(buf, buflen,
				  "hw_idx = %d start_freq = %d end_freq =%d\n",
				  hapd->iface->current_hw_info->hw_idx,
				  hapd->iface->current_hw_info->start_freq,
				  hapd->iface->current_hw_info->end_freq);
	}
	return ret;
}

static int hostapd_ctrl_iface_set_ht40intol_extn(struct hostapd_data *hapd, char *pos)
{
	int ret = -1;
	char *end;
	long value;
	bool user_ht40intol, current_ht40intol;

	if (!hapd || !hapd->iconf || !hapd->conf || !pos)
		return ret;

	value = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || value < 0 || value > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for ht40intol (expected 0 or 1)\n");
		return ret;
	}

	user_ht40intol = (value == 1);
	current_ht40intol = hapd->conf->bss_extn.ht40_intol.is_overridden ?
			    hapd->conf->bss_extn.ht40_intol.value :
			    !!(hapd->iconf->ht_capab & HT_CAP_INFO_40MHZ_INTOLERANT);

	hapd->conf->bss_extn.ht40_intol.is_overridden = true;
	hapd->conf->bss_extn.ht40_intol.value = user_ht40intol;

	if (current_ht40intol == user_ht40intol) {
		wpa_printf(MSG_DEBUG, "Intolerance is already %d\n", current_ht40intol);
		return 0;
	}

	ret = ieee802_11_update_beacons(hapd->iface);
	if (ret)
		wpa_printf(MSG_ERROR, "Failed to update beacons.\n");

	return ret;
}

static int hostapd_ctrl_iface_get_ht40intol_extn(struct hostapd_data *hapd, char *buf,
						 size_t buflen)
{
	int ret = -1;
	bool ht40intol;

	if (!hapd || !hapd->iconf || !hapd->conf || !buf)
		return ret;

	ht40intol = hapd->conf->bss_extn.ht40_intol.is_overridden ?
		    hapd->conf->bss_extn.ht40_intol.value :
		    !!(hapd->iconf->ht_capab & HT_CAP_INFO_40MHZ_INTOLERANT);

	ret = os_snprintf(buf, buflen, "ht40intol: %d\n",
			  ht40intol);

	return ret;
}

static int hostapd_ctrl_iface_set_eht_config_ccfs0_extn(struct hostapd_data *hapd,
							char *pos)
{
	char *end;
	long user_input;
	bool eht_config_ccfs0;

	if (!hapd || !hapd->iconf)
		return -1;

	if (!hostapd_is_eht_enabled(hapd)) {
		wpa_printf(MSG_ERROR, "EHT CONFIG CCFS0 is not allowed in current mode");
		return -1;
	}

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_eht_config_ccfs0\n");
		return -1;
	}

	eht_config_ccfs0 = (user_input == 1);
	hapd->iconf->conf_extn.eht_config_ccfs0 = eht_config_ccfs0;

	return 0;
}

static int hostapd_ctrl_iface_get_eht_config_ccfs0_extn(struct hostapd_data *hapd,
							char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->iconf || !buf)
		return ret;

	if (!hostapd_is_eht_enabled(hapd)) {
		wpa_printf(MSG_ERROR, "Not supported in current mode");
		return -1;
	}

	ret = os_snprintf(buf, buflen, "eht_config_ccfs0 %d\n",
			  hapd->iconf->conf_extn.eht_config_ccfs0);

	return ret;
}

static int hostapd_ctrl_iface_set_tpe_common_psd_extn(struct hostapd_data *hapd,
						      char *pos)
{
	char *end;
	long user_input;
	bool tpe_common_psd;

	if (!hapd || !hapd->conf)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_tpe_common_psd\n");
		return -1;
	}

	tpe_common_psd = (user_input == 1);
	hapd->conf->bss_extn.tpe_common_psd = tpe_common_psd;

	return 0;
}

static int hostapd_ctrl_iface_get_tpe_common_psd_extn(struct hostapd_data *hapd,
						      char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "tpe_common_psd %d\n",
			  hapd->conf->bss_extn.tpe_common_psd);

	return ret;
}

static int hostapd_ctrl_iface_set_tpe_tx_pwr_interp_extn(struct hostapd_data *hapd,
							 char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->conf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' ||
	    (user_input != TPE_REG_EIRP_PSD && user_input != TPE_REG_EIRP)) {
		wpa_printf(MSG_ERROR,
			   "Invalid input for set_tpe_tx_pwr_interp "
			   "(expected 0(TPE_REG_EIRP_PSD) or 1(TPE_REG_EIRP))\n");
		return -1;
	}

	hapd->conf->bss_extn.tpe_tx_pwr_interp = (enum tpe_tx_pwr_interp_unit)user_input;

	return 0;
}

static int hostapd_ctrl_iface_get_tpe_tx_pwr_interp_extn(struct hostapd_data *hapd,
							 char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	if (hapd->conf->bss_extn.tpe_tx_pwr_interp < TPE_REG_EIRP_PSD ||
	    hapd->conf->bss_extn.tpe_tx_pwr_interp > TPE_REG_EIRP) {
		wpa_printf(MSG_ERROR, "Invalid tpe_tx_pwr_interp value %d\n",
			   hapd->conf->bss_extn.tpe_tx_pwr_interp);
		return ret;
	}

	ret = os_snprintf(buf, buflen, "tpe_tx_pwr_interp %d\n",
			  hapd->conf->bss_extn.tpe_tx_pwr_interp);

	return ret;
}

static int hostapd_ctrl_iface_set_tpe_punct_channel_tx_pwr_extn(struct hostapd_data *hapd,
								char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->conf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_tpe_punct_channel_tx_pwr\n");
		return -1;
	}

	hapd->conf->bss_extn.tpe_punct_channel_tx_pwr = (user_input == 1);

	return 0;
}

static int hostapd_ctrl_iface_get_tpe_punct_channel_tx_pwr_extn(struct hostapd_data *hapd,
								char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "tpe_punct_channel_tx_pwr %d\n",
			  hapd->conf->bss_extn.tpe_punct_channel_tx_pwr);

	return ret;
}

static int hostapd_ctrl_iface_set_pureg_extn(struct hostapd_data *hapd, char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->conf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_pureg\n");
		return -1;
	}

	hapd->conf->bss_extn.pureg_bss = (user_input == 1);

	return 0;
}

static int hostapd_ctrl_iface_get_pureg_extn(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "pureg %d\n",
			  hapd->conf->bss_extn.pureg_bss);

	return ret;
}

static int hostapd_ctrl_iface_set_puren_extn(struct hostapd_data *hapd, char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->iconf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_puren.\n");
		return -1;
	}

	hapd->conf->bss_extn.puren_bss.is_overridden = true;
	hapd->conf->bss_extn.puren_bss.value = (user_input == 1);

	return 0;
}

static int hostapd_ctrl_iface_get_puren_extn(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->iconf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "puren %d\n",
			  hapd->conf->bss_extn.puren_bss.is_overridden ?
			  hapd->conf->bss_extn.puren_bss.value :
			  hapd->iconf->require_ht);

	return ret;
}

static int hostapd_ctrl_iface_set_disablecoexist_extn(struct hostapd_data *hapd,
						      char *pos)
{
	int ret = -1;
	char *end;
	long user_input;
	int obss_interval, no_ht_coex;

	if (!hapd || !hapd->iconf || !pos)
		return ret;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_disablecoexist.\n");
		return ret;
	}

	no_ht_coex = user_input;
	obss_interval = user_input ? 0 :
	   (hapd->iconf->obss_interval ? hapd->iconf->obss_interval : DEF_OBSS_INTERVAL);

	if (hapd->iconf->no_ht_coex == no_ht_coex &&
	    hapd->iconf->obss_interval == obss_interval)
		return 0;

	hapd->iconf->no_ht_coex = no_ht_coex;
	hapd->iconf->obss_interval = obss_interval;

	ret = ieee802_11_update_beacons(hapd->iface);
	if (ret)
		wpa_printf(MSG_ERROR, "Failed to update beacons.\n");

	return ret;
}

static int hostapd_ctrl_iface_get_disablecoexist_extn(struct hostapd_data *hapd,
						      char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->iconf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "disablecoexist %d\n",
			  !!(hapd->iconf->no_ht_coex ||
			     !hapd->iconf->obss_interval));

	return ret;
}

static int hostapd_ctrl_iface_set_pure11ac_extn(struct hostapd_data *hapd, char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->iconf || !hapd->conf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_pure11ac.\n");
		return -1;
	}

	hapd->conf->bss_extn.pure11ac_bss.is_overridden = true;
	hapd->conf->bss_extn.pure11ac_bss.value = (user_input == 1);

	return 0;
}

static int hostapd_ctrl_iface_get_pure11ac_extn(struct hostapd_data *hapd,
						char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "pure11ac %d\n",
			  hapd->conf->bss_extn.pure11ac_bss.is_overridden ?
			  hapd->conf->bss_extn.pure11ac_bss.value :
			  hapd->iconf->require_vht);

	return ret;
}

static int hostapd_ctrl_iface_set_pure11ax_extn(struct hostapd_data *hapd, char *pos)
{
	char *end;
	long user_input;

	if (!hapd || !hapd->iconf || !hapd->conf || !pos)
		return -1;

	user_input = strtol(pos, &end, 10);
	if (pos == end || *end != '\0' || user_input < 0 || user_input > 1) {
		wpa_printf(MSG_ERROR, "Invalid input for set_pure11ax.\n");
		return -1;
	}

	hapd->conf->bss_extn.pure11ax_bss.is_overridden = true;
	hapd->conf->bss_extn.pure11ax_bss.value = (user_input == 1);

	return 0;
}

static int hostapd_ctrl_iface_get_pure11ax_extn(struct hostapd_data *hapd,
						char *buf, size_t buflen)
{
	int ret = -1;

	if (!hapd || !hapd->conf || !buf)
		return ret;

	ret = os_snprintf(buf, buflen, "pure11ax %d\n",
			  hapd->conf->bss_extn.pure11ax_bss.is_overridden ?
			  hapd->conf->bss_extn.pure11ax_bss.value :
			  hapd->iconf->require_he);

	return ret;
}

static int hostapd_ctrl_iface_set_esp_extn(struct hostapd_data *hapd, char *cmd)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	char *param;
	long val_long;
	char *end = NULL;

	if (!iface_extn)
		return -1;

	if (!hapd->started || hapd->disabled) {
		wpa_printf(MSG_ERROR, "ESP: BSS is disabled");
		return -1;
	}

	param = strsep(&cmd, "=");
	if (!param || !cmd)
		return -1;

	if (strcmp(param, "enable_esp") != 0 && !iface_extn->esp.enable) {
		wpa_printf(MSG_ERROR, "ESP feature is not enabled");
		return -1;
	}

	while (*cmd == ' ') cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "ESP: Invalid value for %s: empty", param);
		return -1;
	}

	errno = 0;
	val_long = strtol(cmd, &end, 10);
	if (val_long < 0) {
		wpa_printf(MSG_ERROR, "ESP: Negative value not allowed for %s: %ld", param, val_long);
		return -1;
	}

	if (errno != 0 || end == cmd) {
		wpa_printf(MSG_ERROR, "ESP: Invalid numeric value for %s: '%s'", param, cmd);
		return -1;
	}

	while (end && *end == ' ') end++;
	if (end && *end != '\0') {
		wpa_printf(MSG_ERROR, "ESP: Invalid characters in value for %s: '%s'", param, cmd);
		return -1;
	}
	if (os_strcmp(param, "esp_airtime") == 0) {
		if (val_long > 255) {
			wpa_printf(MSG_ERROR, "ESP: Invalid airtime value. Permissible range: 0-255");
			return -1;
		}
	} else if (os_strcmp(param, "esp_ppdu_dur") == 0) {
		if (val_long > 255) {
			wpa_printf(MSG_ERROR, "ESP: Invalid PPDU duration value. Permissible range: 0-255");
			return -1;
		}
	} else if (os_strcmp(param, "esp_ba_window") == 0) {
		if (val_long > 7) {
			wpa_printf(MSG_ERROR, "ESP: Invalid BA window value. Permissible range: 0-7");
			return -1;
		}
	} else if (os_strcmp(param, "enable_esp") == 0) {
		if (val_long != 0 && val_long != 1) {
			wpa_printf(MSG_ERROR, "ESP: Invalid enable_esp value. Permissible values: 0 or 1");
			return -1;
		}
	}

	return hostapd_drv_set_esp_param_extn(hapd, param, (int)val_long);
}


static int hostapd_ctrl_iface_get_esp_extn(struct hostapd_data *hapd,
					   const char *cmd, char *reply,
					   int reply_size)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	int ret;
	u8 airtime, ppdu_dur, ba_window;

	if (!iface_extn)
		return -1;

	if (!hapd->started || hapd->disabled) {
		wpa_printf(MSG_ERROR, "ESP: BSS is disabled");
		return -1;
	}

	if (iface_extn->esp.enable) {
		if (iface_extn->esp.airtime)
			airtime = iface_extn->esp.airtime;
		else
			airtime = iface_extn->esp.computed_airtime;

		if (iface_extn->esp.ppdu_dur)
			ppdu_dur = iface_extn->esp.ppdu_dur;
		else
			ppdu_dur = ESP_DEFAULT_PPDU_DURATION;

		if (iface_extn->esp.ba_window)
			ba_window = iface_extn->esp.ba_window;
		else
			ba_window = 5;
	} else {
		airtime = 0;
		ppdu_dur = 0;
		ba_window = 0;
	}


	ret = os_snprintf(reply, reply_size,
			  "airtime=%u "
			  "ppdu_dur=%u "
			  "ba_window=%u "
			  "enable_esp=%u\n",
			  airtime,
			  ppdu_dur,
			  ba_window,
			  iface_extn->esp.enable);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int
hostapd_ctrl_iface_set_non_prior_penalty_extn(struct hostapd_data *hapd,
					      const char *cmd)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	char *end = NULL;
	long val;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0')
		return -1;

	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd)
		return -1;

	while (end && *end == ' ')
		end++;
	if (end && *end != '\0')
		return -1;

	if (val < 0 || val > 100) {
		wpa_printf(MSG_ERROR,
			   "VLP Non-priority penalty out of range (0-100): %ld",
			   val);
		return -1;
	}

	iface_extn->vlp_non_prior_penalty = (u8)val;
	wpa_printf(MSG_DEBUG, "VLP Non-priority penalty set to %u%%",
		   iface_extn->vlp_non_prior_penalty);
	return 0;
}

static int
hostapd_ctrl_iface_get_non_prior_penalty_extn(struct hostapd_data *hapd,
					      char *reply,
					      int reply_size)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	u8 non_prior_penalty = DEF_VLP_NON_PRIOR_PENALTY;
	int ret;

	if (iface_extn->vlp_non_prior_penalty)
		non_prior_penalty = iface_extn->vlp_non_prior_penalty;
	ret = os_snprintf(reply, reply_size, "%u\n", non_prior_penalty);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_country_ie_extn(struct hostapd_data *hapd,
					      const char *value)
{
	char *end;
	long enabled;
	int old_enabled;

	enabled = strtol(value, &end, 10);
	if (value == end || *end != '\0' || (enabled != 0 && enabled != 1)) {
		wpa_printf(MSG_ERROR,
			   "CTRL_IFACE COUNTRY_IE: invalid value '%s' (expected 0 or 1)",
			   value);
		return -1;
	}

	old_enabled = hapd->iconf->ieee80211d;
	if ((int) enabled == old_enabled)
		return 0;

	hapd->iconf->ieee80211d = enabled;
	if (ieee802_11_update_beacons(hapd->iface) < 0) {
		hapd->iconf->ieee80211d = old_enabled;
		return -1;
	}

	return 0;
}


static int hostapd_ctrl_iface_get_country_ie_extn(struct hostapd_data *hapd,
						  char *reply,
						  size_t reply_size)
{
	int ret;

	ret = os_snprintf(reply, reply_size, "%d\n", hapd->iconf->ieee80211d);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_opclass_tbl_idx_extn(struct hostapd_data *hapd,
							const char *cmd)
{
	char *end;
	unsigned long idx;
	u8 old_idx;

	if (!hapd || !hapd->iconf || !cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "CTRL: SET_OPCLASS_TBL_IDX: empty value");
		return -1;
	}

	errno = 0;
	idx = strtoul(cmd, &end, 10);
	while (end && *end == ' ')
		end++;
	if (errno != 0 || end == cmd || (end && *end != '\0') ||
	    idx > OPCLS_TAB_IDX_MAX) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_OPCLASS_TBL_IDX: invalid value '%s' (expected 0-%d)",
			   cmd, OPCLS_TAB_IDX_MAX);
		return -1;
	}

	old_idx = hapd->iconf->conf_extn.opclass_tbl_idx;
	if (old_idx == (u8) idx)
		return 0;

	hapd->iconf->conf_extn.opclass_tbl_idx = (u8) idx;
	if (ieee802_11_update_beacons(hapd->iface) < 0) {
		hapd->iconf->conf_extn.opclass_tbl_idx = old_idx;
		return -1;
	}

	wpa_printf(MSG_DEBUG, "CTRL: SET_OPCLASS_TBL_IDX set to %lu", idx);
	return 0;
}

static int hostapd_ctrl_iface_get_opclass_tbl_idx_extn(struct hostapd_data *hapd,
							char *reply,
							size_t reply_size)
{
	int ret;

	if (!hapd || !hapd->iconf)
		return -1;

	ret = os_snprintf(reply, reply_size, "%u\n",
			  hapd->iconf->conf_extn.opclass_tbl_idx);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int hostapd_ctrl_iface_set_ecsa_opclass_extn(struct hostapd_data *hapd,
						    const char *cmd)
{
	char *end;
	unsigned long v;

	if (!hapd || !hapd->conf || !cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0') {
		wpa_printf(MSG_ERROR, "CTRL: ECSA_OPCLASS: empty value");
		return -1;
	}

	errno = 0;
	v = strtoul(cmd, &end, 0);
	while (end && *end == ' ')
		end++;
	if (errno != 0 || end == cmd || (end && *end != '\0') || v > 255) {
		wpa_printf(MSG_ERROR, "CTRL: ECSA_OPCLASS: invalid value '%s'",
			   cmd);
		return -1;
	}

	hapd->conf->bss_extn.ecsa_opclass = (u8)v;
	wpa_printf(MSG_DEBUG, "CTRL: ECSA_OPCLASS override set to %lu", v);

	return 0;
}

static int hostapd_ctrl_iface_get_ecsa_opclass_extn(struct hostapd_data *hapd,
						    char *reply,
						    int reply_size)
{
	u8 opclass;
	int res;

	if (!hapd || !hapd->conf)
		return -1;

	opclass = hapd->conf->bss_extn.ecsa_opclass;
	res = os_snprintf(reply, reply_size, "%u\n", opclass);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

/* List of 40 MHz bonded channel frequencies */
static const struct bonded_channel_freq_extn bonded_chan_40mhz_list_freq_extn[] = {
	{5180, 5200},
	{5220, 5240},
	{5260, 5280},
	{5300, 5320},
	{5500, 5520},
	{5540, 5560},
	{5580, 5600},
	{5620, 5640},
	{5660, 5680},
	{5700, 5720},
	{5745, 5765},
	{5785, 5805},
	{5825, 5845},
	{5865, 5885},
	{5955, 5975},
	{5995, 6015},
	{6035, 6055},
	{6075, 6095},
	{6115, 6135},
	{6155, 6175},
	{6195, 6215},
	{6235, 6255},
	{6275, 6295},
	{6315, 6335},
	{6355, 6375},
	{6395, 6415},
	{6435, 6455},
	{6475, 6495},
	{6515, 6535},
	{6555, 6575},
	{6595, 6615},
	{6635, 6655},
	{6675, 6695},
	{6715, 6735},
	{6755, 6775},
	{6795, 6815},
	{6835, 6855},
	{6875, 6895},
	{6915, 6935},
	{6955, 6975},
	{6995, 7015},
	{7035, 7055},
	{7075, 7095}
};

/**
 * hostapd_ctrl_iface_get_sec_offset_from_bonded_40mhz_freq_extn - Get HT40 secondary
 * offset from bonded pair table
 *
 * Looks up @freq in the bonded 40 MHz pair table
 * (bonded_chan_40mhz_list_freq_extn).
 *
 * Return: 1 if @freq matches the pair start frequency (HT40+),
 * -1 if @freq matches the pair end frequency (HT40-),
 * 0 if @freq is not present in the bonded 40 MHz list.
 */
static int
hostapd_ctrl_iface_get_sec_offset_from_bonded_40mhz_freq_extn(int freq)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(bonded_chan_40mhz_list_freq_extn); i++) {
		if (freq == bonded_chan_40mhz_list_freq_extn[i].start_freq)
			return 1;
		if (freq == bonded_chan_40mhz_list_freq_extn[i].end_freq)
			return -1;
	}

	return 0;
}

/*
 * Keep SET_CHANNEL center frequency calculations aligned with
 * mac80211.sh:mac80211_get_seg0().
 */
static int
hostapd_ctrl_iface_get_seg0_idx_extn(int freq, int channel,
				     enum oper_chan_width oper_chwidth,
				     int secondary_channel)
{
	bool is_6ghz = is_6ghz_freq(freq);
	bool is_6ghz_chan2 = is_6ghz && freq == FREQ_6GHZ_CHAN2;

	switch (oper_chwidth) {
	case CONF_OPER_CHWIDTH_USE_HT:
		/* 20 MHz does not use seg0 index; derive only for 40 MHz HT. */
		if (!secondary_channel)
			break;

		if (is_6ghz && !is_6ghz_chan2)
			return ((channel / 4) % 2) ? channel - 2 : channel + 2;
		if (is_24ghz_freq(freq) && freq != FREQ_2GHZ_CHAN14)
			return channel < 7 ? channel + 2 : channel - 2;
		if (!is_6ghz_chan2)
			return ((channel / 4) % 2) ? channel + 2 : channel - 2;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
	case CONF_OPER_CHWIDTH_80P80MHZ:
		if (is_6ghz && !is_6ghz_chan2) {
			switch ((channel / 4) % 4) {
			case 0:
				return channel + 6;
			case 1:
				return channel + 2;
			case 2:
				return channel - 2;
			case 3:
				return channel - 6;
			}
		} else if (!is_6ghz_chan2) {
			switch ((channel / 4) % 4) {
			case 1:
				return channel + 6;
			case 2:
				return channel + 2;
			case 3:
				return channel - 2;
			case 0:
				return channel - 6;
			}
		}
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		if (is_6ghz && !is_6ghz_chan2) {
			switch (channel) {
			case 1: case 5: case 9: case 13: case 17: case 21: case 25: case 29:
				return 15;
			case 33: case 37: case 41: case 45: case 49: case 53: case 57: case 61:
				return 47;
			case 65: case 69: case 73: case 77: case 81: case 85: case 89: case 93:
				return 79;
			case 97: case 101: case 105: case 109: case 113: case 117: case 121: case 125:
				return 111;
			case 129: case 133: case 137: case 141: case 145: case 149: case 153: case 157:
				return 143;
			case 161: case 165: case 169: case 173: case 177: case 181: case 185: case 189:
				return 175;
			case 193: case 197: case 201: case 205: case 209: case 213: case 217: case 221:
				return 207;
			}
		} else if (!is_6ghz_chan2) {
			switch (channel) {
			case 36: case 40: case 44: case 48: case 52: case 56: case 60: case 64:
				return 50;
			case 100: case 104: case 108: case 112: case 116: case 120: case 124: case 128:
				return 114;
			case 149: case 153: case 157: case 161: case 165: case 169: case 173: case 177:
				return 163;
			}
		}
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		if (is_6ghz && !is_6ghz_chan2) {
			switch (channel) {
			case 1: case 5: case 9: case 13: case 17: case 21: case 25: case 29:
			case 33: case 37: case 41: case 45:
				return 31;
			case 49: case 53: case 57: case 61: case 65: case 69: case 73: case 77:
				return 63;
			case 81: case 85: case 89: case 93: case 97: case 101: case 105: case 109:
				return 95;
			case 113: case 117: case 121: case 125: case 129: case 133: case 137: case 141:
				return 127;
			case 145: case 149: case 153: case 157: case 161: case 165: case 169: case 173:
				return 159;
			case 177: case 181: case 185: case 189: case 193: case 197: case 201: case 205:
			case 209: case 213: case 217: case 221:
				return 191;
			}
		} else if (is_5ghz_freq(freq) &&
			   freq >= FREQ_5GHZ_240MHZ_START &&
			   freq <= FREQ_5GHZ_240MHZ_END) {
			return 130;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int
hostapd_ctrl_iface_get_sec_offset_extn(int freq, int channel)
{
	if (is_5ghz_freq(freq) || is_6ghz_freq(freq))
		return hostapd_ctrl_iface_get_sec_offset_from_bonded_40mhz_freq_extn(freq);

	if (is_24ghz_freq(freq)) {
		if (channel < IEEE_2GHZ_CHAN_MIN ||
		    channel > IEEE_2GHZ_CHAN_MAX)
			return 0;

		/* Keep deterministic HT40 split used in set-channel flow. */
		return channel < IEEE_2GHZ_CHAN_SEC_SPLIT ? 1 : -1;
	}

	return 0;
}

static int hostapd_ctrl_iface_set_channel_extn(struct hostapd_data *hapd,
					       const char *cmd)
{
#ifdef NEED_AP_MLME
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	struct hostapd_freq_params freq_params;
	enum oper_chan_width oper_chwidth;
	int sec_offset, old_sec_offset, bandwidth, seg0_calc;
	u8 seg0_idx, seg1_idx;
	u16 punct_bitmap;
	char *end;
	long channel;
	int freq;
	int ret;

	channel = strtol(cmd, &end, 10);
	if (cmd == end || channel <= 0 || channel > 255) {
		wpa_printf(MSG_ERROR, "CTRL: SET_CHANNEL: Invalid channel number");
		return -1;
	}

	while (*end == ' ' || *end == '\t')
		end++;
	if (*end != '\0') {
		wpa_printf(MSG_ERROR, "CTRL: SET_CHANNEL: Unexpected extra arguments");
		return -1;
	}

	if (!iface || !iface->current_mode) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_CHANNEL: Interface mode not initialized");
		return -1;
	}

	mode = iface->current_mode;
	chan = hw_get_channel_chan(mode, channel, &freq);
	if (!chan || !chan_pri_allowed(chan)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_CHANNEL: Channel %ld is not supported", channel);
		return -1;
	}

	if (!chan_in_current_hw_info(iface->current_hw_info, chan)) {
		if (iface->current_hw_info) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_CHANNEL: Channel %ld (freq=%d) is outside current radio[%d] range [%d, %d]",
				   channel, freq, iface->current_hw_info->hw_idx,
				   iface->current_hw_info->start_freq,
				   iface->current_hw_info->end_freq);
		} else {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_CHANNEL: Channel %ld (freq=%d) is not supported by current radio",
				   channel, freq);
		}
		return -1;
	}

	old_sec_offset = iface->conf->secondary_channel;
	oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	bandwidth = hostapd_get_width_from_oper_chwidth_extn(oper_chwidth,
							     old_sec_offset);
	seg0_calc = hostapd_ctrl_iface_get_seg0_idx_extn(freq, channel,
							 oper_chwidth,
							 iface->conf->secondary_channel);
	if (bandwidth > 20) {
		sec_offset = hostapd_ctrl_iface_get_sec_offset_extn(freq, channel);
		if (bandwidth == 40 && (!sec_offset)) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_CHANNEL: Failed to derive sec_offset");
			return -1;
		}

		if (!seg0_calc) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_CHANNEL: Failed to derive seg0");
			return -1;
		}
	} else {
		sec_offset = 0;
	}
	seg0_idx = (u8) seg0_calc;
	seg1_idx = oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ ?
		   hostapd_get_oper_centr_freq_seg1_idx(iface->conf) : 0;
	punct_bitmap = hostapd_get_punct_bitmap(hapd);
	ret = hostapd_set_freq_params(&freq_params,
				      iface->conf->hw_mode,
				      freq, channel,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      iface->conf->ieee80211bn,
				      sec_offset,
				      oper_chwidth,
				      seg0_idx,
				      seg1_idx,
				      iface->conf->vht_capab,
				      mode ? &mode->he_capab[IEEE80211_MODE_AP] : NULL,
				      mode ? &mode->eht_capab[IEEE80211_MODE_AP] : NULL,
				      mode ? &mode->uhr_capab[IEEE80211_MODE_AP] : NULL,
				      punct_bitmap,
				      hapd->iconf->he_6ghz_reg_pwr_type,
				      0, 0,
				      iface->conf->bandwidth_device,
				      iface->conf->center_freq_device);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_CHANNEL: Failed to build freq parameters");
		return -1;
	}

	ret = hostapd_disable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "CTRL: SET_CHANNEL: Failed to disable interface");
		return -1;
	}

	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &freq_params, NULL);
	if (ret) {
		iface->conf->secondary_channel = old_sec_offset;
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_CHANNEL: Failed to set channel %ld in config",
			   channel);
		return -1;
	}

	/*
	 * Keep user-requested primary channel intact. 5 GHz coexistence scan
	 * flow can otherwise swap primary/secondary HT40 pair and implicitly
	 * move the operating primary channel after SET_CHANNEL succeeds.
	 */
	iface->conf->no_pri_sec_switch = 1;
	ret = hostapd_enable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "CTRL: SET_CHANNEL: Failed to enable interface");
		return -1;
	}

	if (iface->conf->channel != channel ||
	    hostapd_get_oper_chwidth(iface->conf) != oper_chwidth) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_CHANNEL: Requested channel/width not retained after restart (requested channel=%ld width=%d, current channel=%u width=%d)",
			   channel, bandwidth, iface->conf->channel,
			   hostapd_get_width_from_oper_chwidth_extn(
				   hostapd_get_oper_chwidth(iface->conf),
				   iface->conf->secondary_channel));
	}

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}

static int hostapd_ctrl_set_rnr_6ghz_colocated_extn(struct hostapd_data *hapd, char *cmd)
{
#ifdef NEED_AP_MLME
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	uint8_t rnr_mode, frm_val;
	char *ptr, *endptr;

	if (!hapd->started) {
		wpa_printf(MSG_ERROR, "Interface is not UP.\n");
		return -1;
	}

	rnr_mode = (uint8_t)strtol(cmd, &ptr, 10);
	if (ptr == cmd || rnr_mode > 1) {
		wpa_printf(MSG_ERROR, "Invalid mode. Use 1:Enable 0:Disable");
		return -1;
	}

	frm_val = (uint8_t)strtol(ptr, &endptr, 10);
	if (ptr == endptr || frm_val > 7) {
		wpa_printf(MSG_ERROR, "Invalid frm_val. Valid values:0 to 7");
		return -1;
	}

	if (rnr_mode == 1 && frm_val == 0) {
		wpa_printf(MSG_ERROR, "Mode is enable But frm is not selected. Invalid frm_val");
		return -1;
	}

	if (rnr_mode == 1) {
		/* User rnr mode enable: set frame mask  */
		conf_extn->rnr_6ghz_colocated_enable |= (frm_val & 0x7);
	} else {
		/* User rnr mode disable: clear frame mask */
		conf_extn->rnr_6ghz_colocated_enable &= ~(frm_val & 0x7);
	}
	wpa_printf(MSG_INFO, "rnr_mode:%d frm_val:%d rnr_6ghz_colocated_enable %d",
			rnr_mode, frm_val, conf_extn->rnr_6ghz_colocated_enable);

	/* Update beacon to reflect the config */
	ieee802_11_update_beacons(hapd->iface);

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}

static int hostapd_ctrl_get_rnr_6ghz_colocated_extn(struct hostapd_data *hapd,
						    const char *cmd, char *reply,
						    int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int ret;

	ret = os_snprintf(reply, reply_size,
			"rnr_6ghz_colocated=%d\n",
			conf_extn->rnr_6ghz_colocated_enable);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

#ifdef CONFIG_TESTING_OPTIONS
static int hostapd_ctrl_iface_sync_iface_freq_extn(struct hostapd_data *hapd)
{
#ifdef NEED_AP_MLME
	unsigned int freq;

	if (hapd->started) {
		wpa_printf(MSG_ERROR, "SYNC_IFACE_FREQ: Cannot sync frequency on already started interface");
		return -1;
	}

	freq = hostapd_hw_get_freq(hapd, hapd->iface->conf->channel);
	if (!freq)
		return -1;

	hapd->iface->freq = freq;
	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}
#endif /* CONFIG_TESTING_OPTIONS */

/**
 * hostapd_iface_rep_ap_enable_extn - Handle REP_AP_ENABLE control command
 * @iface: Pointer to hostapd interface on which repeater AP is enabled
 * @pos: Pointer to control command arguments string (e.g., freq/width/etc.)
 *
 * Parse repeater AP enable parameters (frequency, bandwidth, puncturing
 * bitmap, center frequencies, and secondary offset), update interface
 * configuration, and restart or enable the AP with appropriate DFS/CAC
 * handling.
 *
 * Return: 0 on success or -1 on invalid parameters or configuration errors.
 */
int hostapd_iface_rep_ap_enable_extn(struct hostapd_iface *iface, char *pos)
{
	int freq = 0, width = 0, sec_off = 0;
	int cf1 = 0, cf2 = 0;
	u16 punct_bitmap = 0;
	enum oper_chan_width oper_chwidth = CONF_OPER_CHWIDTH_USE_HT;
	u8 op_class = 0, channel = 0;
	enum hostapd_hw_mode hw_mode;
	int i, ret;
	bool skip_cac_rep = false;
	struct hostapd_config *conf;
	char *param;

	if (!iface || !iface->conf || !pos)
		return -1;

	conf = iface->conf;

	/* Parse fields */
	param = os_strstr(pos, "freq=");
	if (param)
		freq = atoi(param + 5);

	param = os_strstr(pos, " width=");
	if (param)
		width = atoi(param + 7);

	param = os_strstr(pos, " punct_bitmap=");
	if (param)
		punct_bitmap = (u16) atoi(param + 14);

	param = os_strstr(pos, " c_freq1=");
	if (param)
		cf1 = atoi(param + 9);

	param = os_strstr(pos, " c_freq2=");
	if (param)
		cf2 = atoi(param + 9);

	param = os_strstr(pos, " sec_off=");
	if (param)
		sec_off = atoi(param + 9);

	if (!freq) {
		wpa_printf(MSG_ERROR, "REP_AP_ENABLE: missing freq");
		return -1;
	}

	/* Map provided bandwidth to oper_chwidth */
	switch (width) {
	case 80:
		oper_chwidth = (cf2 ? CONF_OPER_CHWIDTH_80P80MHZ :
			       CONF_OPER_CHWIDTH_80MHZ);
		break;
	case 160:
		oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case 320:
		oper_chwidth = CONF_OPER_CHWIDTH_320MHZ;
		break;
	case 40:
	case 20:
	default:
		oper_chwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	}

	/* Determine channel/opclass and HW mode from given freq */
	hw_mode = ieee80211_freq_to_channel_ext(freq, sec_off, oper_chwidth,
						&op_class, &channel);
	if (hw_mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_ERROR, "REP_AP_ENABLE: invalid frequency %d", freq);
		return -1;
	}

	/* Set center segment indices */
	ieee80211_freq_to_chan(freq, &channel);
	hostapd_set_oper_centr_freq_seg0_idx(conf, channel);
	if (cf1 == 5935)
		hostapd_set_oper_centr_freq_seg0_idx(conf, (cf1 - 5925) / 5);
	else if (cf1 > 5950)
		hostapd_set_oper_centr_freq_seg0_idx(conf, (cf1 - 5950) / 5);
	else if (cf1 > 5000)
		hostapd_set_oper_centr_freq_seg0_idx(conf, (cf1 - 5000) / 5);
	else if (cf1 > 0)
		ieee80211_freq_to_chan(cf1, (u8 *) &channel);

	hostapd_set_oper_chwidth(conf, oper_chwidth);

	if (cf2 > 0 && oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ) {
		int seg1_idx = 0;
		if (cf2 == 5935)
			seg1_idx = (cf2 - 5925) / 5;
		else if (cf2 > 5950)
			seg1_idx = (cf2 - 5950) / 5;
		else if (cf2 > 5000)
			seg1_idx = (cf2 - 5000) / 5;
		else
			ieee80211_freq_to_chan(cf2, (u8 *) &seg1_idx);
		hostapd_set_oper_centr_freq_seg1_idx(conf, seg1_idx);
	} else {
		hostapd_set_oper_centr_freq_seg1_idx(conf, 0);
	}

	conf->punct_bitmap = punct_bitmap;
	conf->hw_mode = hw_mode;
	conf->channel = channel;
	conf->acs = 0;

	/* Decide DFS/CAC skip if configured to skip */
	if (conf->conf_extn.skip_cac) {
		skip_cac_rep = ieee80211_is_dfs(freq, iface->hw_features,
						iface->num_hw_features);
	}
	wpa_printf(MSG_INFO, "REP_AP_ENABLE: skip_cac_rep = %d", skip_cac_rep);

	iface->freq = freq;

	/* Restart/Enable iface if needed around DFS CAC requirement */
	switch (iface->state) {
	case HAPD_IFACE_ENABLED:
		if (!skip_cac_rep && (!hostapd_is_dfs_required(iface) ||
		    hostapd_is_dfs_chan_available(iface)))
			break;
		wpa_printf(MSG_INFO,
			   "DFS CAC required on new channel, restart interface");
		/* fallthrough */
	default:
		hostapd_disable_iface(iface);
		break;
	}

	if (conf->channel && !iface->freq)
		iface->freq = hostapd_hw_get_freq(iface->bss[0], conf->channel);

	if (iface->state != HAPD_IFACE_ENABLED)
		hostapd_enable_iface(iface);

	hostapd_apply_6ghz_dynamic_puncturing(iface);
	if (is_6ghz_freq(iface->freq) && iface->conf->enable_best_power_mode) {
		u8 best_power_mode;
		best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
		if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
			iface->conf->he_6ghz_reg_pwr_type = best_power_mode;
			wpa_printf(MSG_INFO,
				   "%s: Best power mode for Freq %d is %d",
				   __func__, iface->freq, best_power_mode);
		}
	}

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		hapd->conf->start_disabled = 0;
#ifdef CONFIG_HOSTAPD_SRC_DIR
		ret = hostapd_set_freq(hapd, conf->hw_mode, iface->freq,
				       conf->channel,
				       conf->enable_edmg,
				       conf->edmg_channel,
				       conf->ieee80211n,
				       conf->ieee80211ac,
				       conf->ieee80211ax,
				       conf->ieee80211be,
				       conf->ieee80211bn,
				       conf->secondary_channel,
				       hostapd_get_oper_chwidth(conf),
				       hostapd_get_oper_centr_freq_seg0_idx(conf),
				       hostapd_get_oper_centr_freq_seg1_idx(conf),
				       skip_cac_rep,
				       conf->bandwidth_device,
				       conf->center_freq_device);
#endif
		wpa_printf(MSG_INFO,
			   "REP_AP_ENABLE: set_freq for bssid " MACSTR
			   " ret %d ifname %s",
			   MAC2STR(hapd->own_addr), ret, hapd->conf->iface);
		ret = ieee802_11_set_beacon(hapd);
		wpa_printf(MSG_DEBUG,
			   "REP_AP_ENABLE: set beacon for bssid " MACSTR
			   " ret %d",
			   MAC2STR(hapd->own_addr), ret);
	}

	return 0;
}

static int
hostapd_ctrl_iface_set_obss_snr_threshold_extn(struct hostapd_data *hapd,
					       const char *cmd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	char *end = NULL;
	long val;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0')
		return -1;

	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd)
		return -1;

	while (end && *end == ' ')
		end++;
	if (end && *end != '\0')
		return -1;

	if (val < OBSS_SNR_MIN || val > OBSS_SNR_MAX) {
		wpa_printf(MSG_ERROR,
			   "OBSS SNR threshold out of range (%d-%d): %ld",
			   OBSS_SNR_MIN, OBSS_SNR_MAX, val);
		return -1;
	}

	conf_extn->obss_snr_threshold = (u8)val;
	wpa_printf(MSG_DEBUG, "OBSS SNR threshold set to %u dB",
		   conf_extn->obss_snr_threshold);
	return 0;
}

static int
hostapd_ctrl_iface_get_obss_snr_threshold_extn(struct hostapd_data *hapd,
					       char *reply, int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int ret;

	ret = os_snprintf(reply, reply_size, "%u\n",
			  conf_extn->obss_snr_threshold);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int
hostapd_ctrl_iface_set_obss_rx_snr_threshold_extn(struct hostapd_data *hapd,
						   const char *cmd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	char *end = NULL;
	long val;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0')
		return -1;

	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd)
		return -1;

	while (end && *end == ' ')
		end++;
	if (end && *end != '\0')
		return -1;

	if (val < OBSS_SNR_MIN || val > OBSS_SNR_MAX) {
		wpa_printf(MSG_ERROR,
			   "OBSS RX SNR threshold out of range (%d-%d): %ld",
			   OBSS_SNR_MIN, OBSS_SNR_MAX, val);
		return -1;
	}

	conf_extn->obss_rx_snr_threshold = (u8)val;
	wpa_printf(MSG_DEBUG, "OBSS RX SNR threshold set to %u dB",
		   conf_extn->obss_rx_snr_threshold);
	return 0;
}

static int
hostapd_ctrl_iface_get_obss_rx_snr_threshold_extn(struct hostapd_data *hapd,
						   char *reply, int reply_size)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int ret;

	ret = os_snprintf(reply, reply_size, "%u\n",
			  conf_extn->obss_rx_snr_threshold);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	return ret;
}

static int
hostapd_ctrl_iface_set_autorecovery_after_nol_vapdown_extn(struct hostapd_data *hapd,
							   const char *cmd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	char *end = NULL;
	long val;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;
	if (*cmd == '\0')
		return -1;

	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd)
		return -1;

	while (end && *end == ' ')
		end++;
	if (end && *end != '\0')
		return -1;

	if (val < 0 || val > 1) {
		wpa_printf(MSG_ERROR,
			   "SET_AUTORECOVERY_AFTER_NOL_VAPDOWN: invalid value %d",
			   (int)val);
		return -1;
	}

	conf_extn->autorecovery_after_nol_vapdown = (int)val;
	wpa_printf(MSG_INFO,
		   "SET_AUTORECOVERY_AFTER_NOL_VAPDOWN: set to %d",
		   (int)val);
	return 0;
}

#ifdef CONFIG_QCA_LAB_TEST_FEATURES
static int hostapd_ctrl_iface_ignorecac_extn(struct hostapd_data *hapd,
					     const char *cmd)
{
	struct hostapd_config_extn *conf_extn;
	char *end;
	long val;

	if (!hapd || !hapd->iconf || !cmd)
		return -1;

	conf_extn = &hapd->iconf->conf_extn;

	while (*cmd == ' ')
		cmd++;

	val = strtol(cmd, &end, 10);
	if (cmd == end)
		return -1;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		return -1;

	if (val != 0 && val != 1)
		return -1;

	conf_extn->ignorecac = val;
	if (hapd->iface)
		hapd->iface->iface_extn.ignorecac = val;
	if (hapd->iface && hapd->iface->conf)
		hapd->iface->conf->conf_extn.ignorecac = val;
	wpa_printf(MSG_INFO, "IGNORECAC=%ld on interface %s",
		   val, hapd->conf->iface);

	return 0;
}

static int hostapd_ctrl_iface_get_ignorecac_extn(struct hostapd_data *hapd,
						 char *reply,
						 size_t reply_size)
{
	int res;

	if (!hapd || !hapd->iface)
		return -1;

	res = os_snprintf(reply, reply_size, "%d\n",
			  hapd->iconf->conf_extn.ignorecac ? 1 : 0);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}
#endif /* CONFIG_QCA_LAB_TEST_FEATURES */

#ifdef CONFIG_IEEE80211AC
static int hostapd_ctrl_iface_mu_cap_war_extn(struct hostapd_data_extn *hapd_extn,
					    const char *cmd)
{
	int val;

	if (!cmd || sscanf(cmd, "%d", &val) != 1 || (val != 0 && val != 1))
		return -1;

	hapd_extn->mu_cap_war = val ? 1 : 0;

	wpa_printf(MSG_DEBUG, "MU_CAP_WAR state: %s", val ? "enabled":"disabled");

	return 0;
}

static int hostapd_ctrl_iface_get_mu_cap_war_extn(struct hostapd_data_extn *hapd_extn,
						char *reply,
					        size_t reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "%s\n",
			  hapd_extn->mu_cap_war ? "Enabled" : "Disabled");

	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}
#endif /* CONFIG_IEEE80211AC */

static int hostapd_ctrl_iface_dfs_no_wradar_extn(struct hostapd_data *hapd,
						 const char *value)
{
	char *end = NULL;
	long val;
	struct hostapd_iface_extn *iface_extn;

	if (!hapd || !hapd->iface || !value) {
		wpa_printf(MSG_ERROR, "DFS_NO_WRADAR: Invalid parameters");
		return -1;
	}

	iface_extn = &hapd->iface->iface_extn;

	val = strtol(value, &end, 10);
	if (end == value) {
		wpa_printf(MSG_ERROR, "DFS_NO_WRADAR: Invalid value format");
		return -1;
	}
	while (end && (*end == ' ' || *end == '\n' || *end == '\r' ||
		       *end == '\t'))
		end++;
	if (end && *end != '\0') {
		wpa_printf(MSG_ERROR, "DFS_NO_WRADAR: Trailing characters after value");
		return -1;
	}

	if (val != 0 && val != 1) {
		wpa_printf(MSG_ERROR, "DFS_NO_WRADAR: Value must be 0 or 1");
		return -1;
	}

	iface_extn->dfs_no_wradar = !!val;

	return 0;
}

static int hostapd_ctrl_iface_get_dfs_no_wradar_extn(struct hostapd_data *hapd,
						     char *reply,
						     size_t reply_size)
{
	return os_snprintf(reply, reply_size, "%d\n",
			   hapd && hapd->iface &&
			   hapd->iface->iface_extn.dfs_no_wradar);
}

static bool
hostapd_ctrl_iface_list_has_current_chan_extn(struct hostapd_data *hapd,
					      const u8 *chan_list,
					      size_t chan_count)
{
	u8 current_chan = 0;
	size_t idx;
	int mode;

	if (!hapd || !hapd->iface || !chan_list || !chan_count ||
	    hapd->iface->freq <= 0)
		return false;

	mode = ieee80211_freq_to_chan(hapd->iface->freq, &current_chan);
	if (mode == NUM_HOSTAPD_MODES || !current_chan)
		return false;

	for (idx = 0; idx < chan_count; idx++) {
		if (chan_list[idx] == current_chan)
			return true;
	}

	return false;
}

static bool
hostapd_ctrl_iface_chan_in_list_extn(const u8 *chan_list, size_t chan_count,
				     u8 channel)
{
	size_t idx;

	if (!chan_list || !chan_count || !channel)
		return false;

	for (idx = 0; idx < chan_count; idx++) {
		if (chan_list[idx] == channel)
			return true;
	}

	return false;
}

static struct hostapd_channel_data *
hostapd_ctrl_iface_first_enabled_chan_extn(struct hostapd_iface *iface,
					   const u8 *blocked_chan_list,
					   size_t blocked_chan_count,
					   u8 current_chan)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	bool select_5ghz;
	bool select_6ghz;
	bool select_24ghz;
	int i;

	if (!iface || !iface->current_mode)
		return NULL;

	mode = iface->current_mode;
	select_5ghz = is_5ghz_freq(iface->freq);
	select_6ghz = is_6ghz_freq(iface->freq);
	select_24ghz = is_24ghz_freq(iface->freq);

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (select_5ghz && !is_5ghz_freq(chan->freq))
			continue;

		if (select_6ghz && !is_6ghz_freq(chan->freq))
			continue;

		if (select_24ghz && !is_24ghz_freq(chan->freq))
			continue;

		if (current_chan && chan->chan == current_chan)
			continue;

		if (hostapd_ctrl_iface_chan_in_list_extn(blocked_chan_list,
							 blocked_chan_count,
							 chan->chan))
			continue;

		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;

		if (!chan_pri_allowed(chan))
			continue;

		if (!chan_in_current_hw_info(iface->current_hw_info, chan))
			continue;

		return chan;
	}

	return NULL;
}

static int
hostapd_ctrl_iface_build_target_freq_params_extn(struct hostapd_data *hapd,
						 struct hostapd_channel_data *chan,
						 struct hostapd_freq_params *freq_params)
{
	struct hostapd_iface *iface;
	struct hostapd_hw_modes *mode;

	if (!hapd || !hapd->iface || !chan || !freq_params)
		return -1;

	iface = hapd->iface;
	mode = iface->current_mode;

	os_memset(freq_params, 0, sizeof(*freq_params));
	return hostapd_set_freq_params(freq_params,
				       iface->conf->hw_mode,
				       chan->freq, chan->chan,
				       iface->conf->enable_edmg,
				       iface->conf->edmg_channel,
				       iface->conf->ieee80211n,
				       iface->conf->ieee80211ac,
				       iface->conf->ieee80211ax,
				       iface->conf->ieee80211be,
				       iface->conf->ieee80211bn,
				       0,
				       CONF_OPER_CHWIDTH_USE_HT,
				       0,
				       0,
				       iface->conf->vht_capab,
				       mode ? &mode->he_capab[IEEE80211_MODE_AP] : NULL,
				       mode ? &mode->eht_capab[IEEE80211_MODE_AP] : NULL,
				       mode ? &mode->uhr_capab[IEEE80211_MODE_AP] : NULL,
				       0,
                                       hapd->iconf->he_6ghz_reg_pwr_type,
                                       0, 0,
                                       iface->conf->bandwidth_device,
				       iface->conf->center_freq_device);
}

/**
 * hostapd_ctrl_iface_move_off_disabled_channel_extn - Move AP off disabled channel
 * @hapd: BSS context receiving DISABLE_OPCLASS_CHANS
 * @blocked_chan_list: List of channels disabled by the control command
 * @blocked_chan_count: Number of entries in @blocked_chan_list
 *
 * If the current operating channel is disabled, select the first enabled
 * replacement channel in the same band and reconfigure the interface to that
 * channel. The fallback frequency parameters are built with 20 MHz bandwidth.
 *
 * Return: 0 on success, -1 on failure.
 */
static int
hostapd_ctrl_iface_move_off_disabled_channel_extn(struct hostapd_data *hapd,
						  const u8 *blocked_chan_list,
						  size_t blocked_chan_count)
{
#ifdef NEED_AP_MLME
	struct hostapd_iface *iface;
	struct hostapd_channel_data *target_chan;
	struct hostapd_freq_params freq_params;
	int ret;
	int i;
	u8 current_chan = 0;

	if (!hapd || !hapd->iface)
		return -1;

	iface = hapd->iface;
	ret = hostapd_get_hw_features(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: failed to refresh hw features (%d)",
			   ret);
		return -1;
	}

	ret = hostapd_select_hw_mode(iface);
	if (ret < 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: DISABLE_OPCLASS_CHANS: select hw mode returned %d; continuing fallback recovery",
			   ret);
		/*
		 * hostapd_select_hw_mode() can fail while the current
		 * configured channel is being disabled. If a matching
		 * mode is present, continue with best-effort fallback.
		 */
		if (!iface->current_mode) {
			for (i = 0; i < iface->num_hw_features; i++) {
				if (iface->hw_features[i].mode ==
				    iface->conf->hw_mode) {
					iface->current_mode =
						&iface->hw_features[i];
					break;
				}
			}
		}

		if (!iface->current_mode) {
			wpa_printf(MSG_ERROR,
				   "CTRL: DISABLE_OPCLASS_CHANS: no usable hw mode available for fallback");
			return -1;
		}
	}

	if (iface->freq > 0 && hostapd_set_current_hw_info(iface, iface->freq))
		wpa_printf(MSG_DEBUG,
			   "CTRL: DISABLE_OPCLASS_CHANS: could not refresh current_hw_info for freq=%d",
			   iface->freq);

	if (ieee80211_freq_to_chan(iface->freq, &current_chan) ==
	    NUM_HOSTAPD_MODES)
		current_chan = 0;

	target_chan = hostapd_ctrl_iface_first_enabled_chan_extn(iface, blocked_chan_list,
								 blocked_chan_count,
								 current_chan);
	if (!target_chan) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: no enabled fallback channel available");
		return -1;
	}

	if (target_chan->freq == iface->freq) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: current channel %u is disabled, but no alternate channel was selected",
			   current_chan);
		return -1;
	}

	wpa_printf(MSG_INFO,
		   "CTRL: DISABLE_OPCLASS_CHANS: moving from disabled channel %u to channel %u (%d MHz) with fallback restart%s",
		   current_chan, target_chan->chan, target_chan->freq,
		   iface->cac_started ? " (stopping DFS CAC)" : "");

	if (hostapd_ctrl_iface_build_target_freq_params_extn(hapd, target_chan,
							     &freq_params)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: failed to build fallback params for channel %u",
			   target_chan->chan);
		return -1;
	}
	wpa_printf(MSG_DEBUG,
		   "CTRL: DISABLE_OPCLASS_CHANS: fallback params freq=%d chan=%d bw=%d sec_off=%d cf1=%d cf2=%d",
		   freq_params.freq, freq_params.channel, freq_params.bandwidth,
		   freq_params.sec_channel_offset, freq_params.center_freq1,
		   freq_params.center_freq2);

	ret = hostapd_disable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: Failed to disable interface");
		return -1;
	}

	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &freq_params, NULL);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: Failed to set channel");
		return -1;
	}

	iface->conf->no_pri_sec_switch = 1;
	ret = hostapd_enable_iface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: Failed to enable interface");
		return -1;
	}
	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}

static int
hostapd_ctrl_iface_disable_opclass_chans_extn(struct hostapd_data *hapd, char *cmd)
{
	u8 chan_list[256], is_disable, opclass, link_id;
	char *end = cmd;
	long val;
	size_t chan_count = 0;
	int ret;
	bool current_chan_requested = false;

	if (!hapd || !cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd || (val != 0 && val != 1)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: invalid disable flag '%s'",
			   cmd);
		return -1;
	}

	is_disable = (u8) val;
	while (*end == ' ')
		end++;
	if (*end == '\0') {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: missing opclass");
		return -1;
	}

	cmd = end;
	errno = 0;
	val = strtol(cmd, &end, 10);
	if (errno != 0 || end == cmd || val <= 0 || val > 255) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: invalid opclass '%s'",
			   cmd);
		return -1;
	}

	opclass = (u8) val;
	while (*end == ' ')
		end++;
	if (*end == '\0') {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: at least one channel required");
		return -1;
	}

	while (*end != '\0') {
		if (chan_count >= ARRAY_SIZE(chan_list)) {
			wpa_printf(MSG_ERROR,
			"CTRL: DISABLE_OPCLASS_CHANS: channel cnt %zu exceeds %zu",
			chan_count, ARRAY_SIZE(chan_list));
			return -1;
		}

		errno = 0;
		val = strtol(end, &cmd, 10);
		if (errno != 0 || cmd == end || val <= 0 || val > 255) {
			wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: invalid channel near '%s'",
			   end);
			return -1;
		}

		chan_list[chan_count++] = (u8) val;
		while (*cmd == ' ')
			cmd++;
		end = cmd;
	}

	link_id = hapd->mld_link_id;
	ret = nl80211_disable_opclass_chans_extn(hapd->drv_priv, link_id, is_disable,
						 opclass, chan_list, chan_count);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: driver command failed ret=%d",
			   ret);
		if (ret == -EINVAL)
			wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: backend support is not available in driver/open-extns");
		return -1;
	}

	current_chan_requested = is_disable &&
				 hostapd_ctrl_iface_list_has_current_chan_extn(
					hapd, chan_list, chan_count);
	if (current_chan_requested && hapd->iface &&
	    hostapd_ctrl_iface_move_off_disabled_channel_extn(hapd, chan_list,
							      chan_count)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: DISABLE_OPCLASS_CHANS: failed to move interface away from disabled channel");
	}

	wpa_printf(MSG_DEBUG,
		   "CTRL: DISABLE_OPCLASS_CHANS: disable=%u opclass=%u channels=%zu",
		   is_disable, opclass, chan_count);
	return 0;
}

static int hostapd_ctrl_iface_set_allow_scan_on_dfs_chan_extn(struct hostapd_data *hapd,
							      const char *value)
{
	struct hostapd_iface_extn *iface_extn;
	char *end = NULL;
	long val;

	if (!hapd || !hapd->iface || !value) {
		wpa_printf(MSG_ERROR, "ALLOW_SCAN_ON_DFS: Invalid parameters");
		return -1;
	}

	iface_extn = &hapd->iface->iface_extn;

	val = strtol(value, &end, 10);
	if (end == value) {
		wpa_printf(MSG_ERROR, "ALLOW_SCAN_ON_DFS: Invalid value format");
		return -1;
	}

	while (end && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
		end++;

	if (end && *end != '\0') {
		wpa_printf(MSG_ERROR, "ALLOW_SCAN_ON_DFS: Trailing characters after value");
		return -1;
	}

	if (val != 0 && val != 1) {
		wpa_printf(MSG_ERROR, "ALLOW_SCAN_ON_DFS: Value must be 0 or 1");
		return -1;
	}

	iface_extn->allow_scan_on_dfs_chan = !!val;

	if (nl80211_set_allow_scan_on_dfs_chan_extn(hapd->drv_priv,
	    iface_extn->allow_scan_on_dfs_chan)) {
		wpa_printf(MSG_ERROR, "ALLOW_SCAN_ON_DFS: Failed to set in driver");
		return -1;
	}

	return 0;
}

static int hostapd_ctrl_iface_get_allow_scan_on_dfs_chan_extn(struct hostapd_data *hapd,
							      char *reply,
							      size_t reply_size)
{
	return os_snprintf(reply, reply_size, "%d\n",
			   hapd && hapd->iface &&
			   hapd->iface->iface_extn.allow_scan_on_dfs_chan);
}

int
hostapd_ctrl_iface_receive_process_extn(struct hostapd_data *hapd,
					char *buf, char *reply,
					int reply_size,
					struct sockaddr_storage *from,
					socklen_t fromlen, int *reply_len)
{
	int reply_len_extn = *reply_len;

	if (os_strncmp(buf, "SET_ESP", 7) == 0) {
		if (hostapd_ctrl_iface_set_esp_extn(hapd, buf + 8))
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "GET_ESP", 7) == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_esp_extn(hapd, buf + 7, reply,
								 reply_size);
	} else if (os_strncmp(buf, "ECSA_OPCLASS ", 13) == 0) {
		if (hostapd_ctrl_iface_set_ecsa_opclass_extn(hapd, buf + 13))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_ECSA_OPCLASS") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_ecsa_opclass_extn(hapd, reply,
									  reply_size);
	} else if (os_strncmp(buf, "RNR_6GHZ_COLOCATED ", 19) == 0) {
		if (hostapd_ctrl_set_rnr_6ghz_colocated_extn(hapd, buf + 19))
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "GET_RNR_6GHZ_COLOCATED", 22) == 0) {
		reply_len_extn = hostapd_ctrl_get_rnr_6ghz_colocated_extn(hapd, buf + 22, reply,
									  reply_size);
	} else if (os_strcmp(buf, "GET_HW_INFO") == 0) {
		reply_len_extn = hostapd_ctrl_get_hw_info_extn(hapd, reply, reply_size);
	} else if (os_strncmp(buf, "REP_AP_ENABLE ", 14) == 0) {
		if (hostapd_iface_rep_ap_enable_extn(hapd->iface, buf + 14))
			reply_len_extn = -1;
#ifdef CONFIG_ACS
	} else if (os_strncmp(buf, "ACS ", 4) == 0) {
		reply_len_extn = hostapd_handle_cli_acs_extn(hapd, buf + 4,
							     reply, reply_size);
#endif
#ifdef CONFIG_QCA_LAB_TEST_FEATURES
	} else if (os_strcmp(buf, "IGNORECAC") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_ignorecac_extn(hapd, reply,
								       reply_size);
	} else if (os_strncmp(buf, "IGNORECAC ", 10) == 0) {
		if (hostapd_ctrl_iface_ignorecac_extn(hapd, buf + 10))
			reply_len_extn = -1;
#endif /* CONFIG_QCA_LAB_TEST_FEATURES */
#ifdef CONFIG_IEEE80211AC
	} else if (os_strncmp(buf, "MU_CAP_WAR ", 11) == 0) {
		if (hostapd_ctrl_iface_mu_cap_war_extn(&hapd->hapd_extn, buf + 11))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_MU_CAP_WAR") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_mu_cap_war_extn(&hapd->hapd_extn, reply,
								      reply_size);
#endif /* CONFIG_IEEE80211AC */
	} else if (os_strncmp(buf, "DFS_NO_WRADAR ", 14) == 0) {
		if (hostapd_ctrl_iface_dfs_no_wradar_extn(hapd, buf + 14))
			reply_len_extn = -1;
		else
			reply_len_extn = os_snprintf(reply, reply_size, "OK\n");
	} else if (os_strcmp(buf, "GET_DFS_NO_WRADAR") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_dfs_no_wradar_extn(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "ALLOW_SCAN_ON_DFS_CHAN ", 23) == 0) {
		if (hostapd_ctrl_iface_set_allow_scan_on_dfs_chan_extn(hapd, buf + 23))
			reply_len_extn = -1;
		else
			reply_len_extn = os_snprintf(reply, reply_size, "OK\n");
	} else if (os_strcmp(buf, "GET_ALLOW_SCAN_ON_DFS_CHAN") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_allow_scan_on_dfs_chan_extn(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "DCS ", 4) == 0) {
		reply_len_extn = hostapd_ctrl_iface_dcs_extn(hapd, buf + 4, reply,
							     reply_size);
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strcmp(buf, "SYNC_IFACE_FREQ") == 0) {
		if (hostapd_ctrl_iface_sync_iface_freq_extn(hapd))
			reply_len_extn = -1;
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strncmp(buf, "SET_VLP_NON_PRIOR_PENALTY ", 26) == 0) {
		if (hostapd_ctrl_iface_set_non_prior_penalty_extn(hapd, buf + 26))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_VLP_NON_PRIOR_PENALTY") == 0) {
		reply_len_extn =
			hostapd_ctrl_iface_get_non_prior_penalty_extn(hapd, reply,
								      reply_size);
	} else if (os_strncmp(buf, "CBS ", 4) == 0) {
		reply_len_extn = hostapd_handle_cli_cbs_extn(hapd, buf + 4,
							     reply, reply_size);
	} else if (os_strncmp(buf, "SET_PRIMARY_CHANS ", 18) == 0) {
		/*
		 * SET_PRIMARY_CHANS <ch1> [<ch2> ...]
		 * Restrict ACS/DFS/scan to the listed primary channels.
		 * An empty argument list clears the restriction and reverts
		 * to the full regulatory channel set (ChanSel-001..003,011).
		 */
		if (hostapd_set_primary_chanlist(hapd, buf + 18) < 0)
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_PRIMARY_CHANS") == 0) {
		/*
		 * GET_PRIMARY_CHANS
		 * Returns the currently configured primary channel list.
		 */
		reply_len_extn = hostapd_get_primary_chanlist(hapd->iface,
							      reply, reply_size);
		if (reply_len_extn < 0)
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "DISABLE_OPCLASS_CHANS ", 22) == 0) {
		if (hostapd_ctrl_iface_disable_opclass_chans_extn(hapd, buf + 22))
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "SET_OBSS_SNR_THRESHOLD ", 23) == 0) {
		if (hostapd_ctrl_iface_set_obss_snr_threshold_extn(hapd,
								   buf + 23))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_OBSS_SNR_THRESHOLD") == 0) {
		reply_len_extn =
			hostapd_ctrl_iface_get_obss_snr_threshold_extn(hapd,
									reply,
									reply_size);
	} else if (os_strncmp(buf, "SET_OBSS_RX_SNR_THRESHOLD ", 26) == 0) {
		if (hostapd_ctrl_iface_set_obss_rx_snr_threshold_extn(hapd,
								      buf + 26))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_OBSS_RX_SNR_THRESHOLD") == 0) {
		reply_len_extn =
			hostapd_ctrl_iface_get_obss_rx_snr_threshold_extn(hapd,
									   reply,
									   reply_size);
	} else if (os_strncmp(buf, "COUNTRY_IE ", 11) == 0) {
		if (hostapd_ctrl_iface_country_ie_extn(hapd, buf + 11))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_COUNTRY_IE") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_country_ie_extn(hapd,
								      reply,
								      reply_size);
	} else if (os_strncmp(buf, "SET_OPCLASS_TBL_IDX ", 20) == 0) {
		if (hostapd_ctrl_iface_set_opclass_tbl_idx_extn(hapd, buf + 20))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_OPCLASS_TBL_IDX") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_opclass_tbl_idx_extn(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "HT40INTOL ", 10) == 0) {
		if (hostapd_ctrl_iface_set_ht40intol_extn(hapd, buf + 10))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_HT40INTOL") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_ht40intol_extn(hapd, reply,
								       reply_size);
	} else if (os_strncmp(buf, "SET_CHANNEL ", 12) == 0) {
		if (hostapd_ctrl_iface_set_channel_extn(hapd, buf + 12))
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "SET_EHT_CONFIG_CCFS0 ", 21) == 0) {
		if (hostapd_ctrl_iface_set_eht_config_ccfs0_extn(hapd, buf + 20))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_EHT_CONFIG_CCFS0") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_eht_config_ccfs0_extn(hapd,
									      reply,
									      reply_size);
	} else if (os_strncmp(buf, "SET_TPE_COMMON_PSD ", 19) == 0) {
		if (hostapd_ctrl_iface_set_tpe_common_psd_extn(hapd, buf + 19))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_TPE_COMMON_PSD") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_tpe_common_psd_extn(hapd,
									    reply,
									    reply_size);
	} else if (os_strncmp(buf, "SET_AUTORECOVERY_AFTER_NOL_VAPDOWN ", 35) == 0) {
		if (hostapd_ctrl_iface_set_autorecovery_after_nol_vapdown_extn(hapd,
									       buf + 35))
			reply_len_extn = -1;
	} else if (os_strncmp(buf, "SET_TPE_TX_PWR_INTERP ", 22) == 0) {
		if (hostapd_ctrl_iface_set_tpe_tx_pwr_interp_extn(hapd, buf + 22))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_TPE_TX_PWR_INTERP") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_tpe_tx_pwr_interp_extn(hapd,
									       reply,
									       reply_size);
	} else if (os_strncmp(buf, "SET_TPE_PUNCT_CHANNEL_TX_PWR ", 29) == 0) {
		if (hostapd_ctrl_iface_set_tpe_punct_channel_tx_pwr_extn(hapd, buf + 29))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_TPE_PUNCT_CHANNEL_TX_PWR") == 0) {
		reply_len_extn =
			hostapd_ctrl_iface_get_tpe_punct_channel_tx_pwr_extn(hapd,
									     reply,
									     reply_size);
	} else if (os_strncmp(buf, "SET_PUREG ", 10) == 0) {
		if (hostapd_ctrl_iface_set_pureg_extn(hapd, buf + 10))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_PUREG") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_pureg_extn(hapd, reply,
								   reply_size);
	} else if (os_strncmp(buf, "SET_PUREN ", 10) == 0) {
		if (hostapd_ctrl_iface_set_puren_extn(hapd, buf + 10))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_PUREN") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_puren_extn(hapd, reply,
								   reply_size);
	} else if (os_strncmp(buf, "SET_DISABLECOEXIST ", 19) == 0) {
		if (hostapd_ctrl_iface_set_disablecoexist_extn(hapd, buf + 19))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_DISABLECOEXIST") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_disablecoexist_extn(hapd,
									    reply,
									    reply_size);
	} else if (os_strncmp(buf, "SET_PURE11AC ", 13) == 0) {
		if (hostapd_ctrl_iface_set_pure11ac_extn(hapd, buf + 13))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_PURE11AC") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_pure11ac_extn(hapd, reply,
								      reply_size);
	} else if (os_strncmp(buf, "SET_PURE11AX ", 13) == 0) {
		if (hostapd_ctrl_iface_set_pure11ax_extn(hapd, buf + 13))
			reply_len_extn = -1;
	} else if (os_strcmp(buf, "GET_PURE11AX") == 0) {
		reply_len_extn = hostapd_ctrl_iface_get_pure11ax_extn(hapd, reply,
								      reply_size);
	} else if (os_strncmp(buf, "GET_MAPC_PEER_PARAMS ", 21) == 0) {
		u8 peer_addr[ETH_ALEN];

		if (hwaddr_aton(buf + 21, peer_addr)) {
			reply_len_extn = os_snprintf(reply, reply_size,
						     "FAIL: invalid MAC address\n");
		} else {
			reply_len_extn = mapc_get_peer_params(hapd, peer_addr,
							      reply, reply_size);
			if (reply_len_extn < 0)
				reply_len_extn = os_snprintf(reply, reply_size,
							     "FAIL\n");
		}
	} else {
		return -1;
	}

	*reply_len = reply_len_extn;

	if (*reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		*reply_len = 5;
	}

	return 0;
}

int hostapd_set_nontx_optional_vendor_elem_size_extn(struct hostapd_data *hapd,
						     struct hostapd_bss_config *conf,
						     char *value)
{
	char *end;
	unsigned long v;
	u8 optional_elem_size, vendor_elem_size;

	if (hapd && !hapd->disabled) {
		wpa_printf(MSG_ERROR, "CTRL: BSS should be in disabled state");
		return -1;
	}

	v = strtoul(value, &end, 0); /* accepts 0x-prefixed hex or decimal */
	if (end == value || *end != '\0') {
		wpa_printf(MSG_ERROR, "CTRL: nontx_profile_ie_size: invalid value '%s'", value);
		return -1;
	}

	if (v > 0xFFFF) {
		wpa_printf(MSG_ERROR, "CTRL: nontx_profile_ie_size: out of range '%s'", value);
		return -1;
	}

	optional_elem_size = (v >> 8) & 0xFF;
	vendor_elem_size = v & 0xFF;

	wpa_printf(MSG_INFO,
		   "CTRL: Optional elem size: %u max limit: %d vendor elem size: %u max limit: %d",
		   optional_elem_size, MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE, vendor_elem_size,
		   MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE);

	if (optional_elem_size > MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE) {
		wpa_printf(MSG_ERROR, "CTRL: Optional elem size %u bytes exceeds max limit %d",
			   optional_elem_size, MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE);
		return -1;
	}

	if (vendor_elem_size > MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE) {
		wpa_printf(MSG_ERROR, "CTRL: Vendor elem size %u bytes exceeds max limit %d",
			   vendor_elem_size, MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE);
		return -1;
	}

	conf->bss_extn.nontx_optional_elem_size = optional_elem_size;
	conf->bss_extn.nontx_vendor_elem_size = vendor_elem_size;
	conf->available_vendor_elem_size = vendor_elem_size;

	return 0;
}

/**
 * hostapd_set_cswopts_extn - Validate and apply a CSwOpts value
 * @conf_extn: Hostapd extension config structure
 * @value: String value to parse (decimal or hex with 0x prefix)
 *
 * Return: 0 on success, -1 on invalid value.
 */
int hostapd_set_cswopts_extn(struct hostapd_config_extn *conf_extn,
			     const char *value)
{
	unsigned int *cswopts = &conf_extn->cswopts;
	long int val = strtol(value, NULL, 0);

	if (!cswopts_validate(val)) {
		wpa_printf(MSG_ERROR,
			   "Invalid CSwOpts value 0x%lx: valid mask is 0x%x",
			   val, CSH_OPT_VALID_MASK);
		return -1;
	}

	if (val == 0)
		*cswopts = 0;
	else {
		if ((IS_CSH_RCSA_TO_UPLINK_ENABLED(*value) ||
		     IS_CSH_PROCESS_RCSA_ENABLED(*value)) &&
		    conf_extn->uplink_csa) {
			wpa_printf(MSG_INFO, "RCSA and Uplink CSA cann't co-exists");
			return -1;
		}
		*cswopts |= (unsigned int)val;
	}
	wpa_printf(MSG_INFO, "CSwOpts updated to 0x%x", *cswopts);

	if (IS_CSH_APRIORI_NEXT_CHANNEL_ENABLED(*cswopts))
		wpa_printf(MSG_INFO,
			   "CSwOpts %s: Apriori channel selection feature is not yet supported",
			   convert_cswopts_to_str(CSH_OPT_APRIORI_NEXT_CHANNEL));

	if (IS_CSH_CAC_APUP_BYSTA_ENABLED(*cswopts)) {
		wpa_printf(MSG_INFO,
			   "CSwOpts %s: Setting skip_cac to 0",
			   convert_cswopts_to_str(CSH_OPT_CAC_APUP_BYSTA));
		conf_extn->skip_cac = 0;
	}

	return 0;
}

int hostapd_set_he_mcs_12_13_peer_cap_extn(struct hostapd_data *hapd)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	u8 radio_idx;

	if (hapd->iface->current_hw_info)
		radio_idx = hapd->iface->current_hw_info->hw_idx;
	else
		return -1;

	if (nl80211_set_he_mcs_12_13_peer_cap_extn(hapd->drv_priv, radio_idx,
						   iface_extn->he_mcs_12_13_peer_cap))
		return -1;

	wpa_printf(MSG_INFO,
		   "he_mcs_12_13 peer capability = 0x%04x set to driver successfully",
		   iface_extn->he_mcs_12_13_peer_cap);

	return 0;
}

static int hostapd_he_mcs_12_13_deauth_sta(struct hostapd_data *hapd,
					   struct sta_info *sta, void *ctx)
{
	ap_sta_disconnect(hapd, sta, sta->addr, WLAN_REASON_PREV_AUTH_NOT_VALID);

	return 0;
}

int hostapd_set_he_mcs_12_13_cap_extn(struct hostapd_data *hapd)
{
	struct hostapd_iface_extn *iface_extn = &hapd->iface->iface_extn;
	u16 radio_cap = 0;
	u8 radio_idx;

	if (hapd->iconf->conf_extn.he_mcs_12_13_enabled) {
		if (hapd->iface->current_hw_info) {
			radio_idx = hapd->iface->current_hw_info->hw_idx;
		} else {
			wpa_printf(MSG_ERROR, "HE MCS 12/13: current hw_info is not available to determine radio idx");
			return -1;
		}

		if (nl80211_get_he_mcs_12_13_extn(hapd->drv_priv, radio_idx, &radio_cap))
			return -1;
	}

	iface_extn->he_mcs_12_13_radio_cap = radio_cap;
	wpa_printf(MSG_INFO,
		   "he_mcs_12_13_supp set to %d radio_capabilities = 0x%04x",
		   hapd->iconf->conf_extn.he_mcs_12_13_enabled,
		   iface_extn->he_mcs_12_13_radio_cap);

	return 0;
}

static int hostapd_he_mcs_12_13_supp_extn(struct hostapd_data *hapd, bool val)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;

	conf_extn->he_mcs_12_13_enabled = val;
	if (hostapd_set_he_mcs_12_13_cap_extn(hapd)) {
		wpa_printf(MSG_ERROR, "Failed to set HE MCS 12 13 support");
		return -1;
	}

	if (ieee802_11_update_beacons(hapd->iface) < 0)
		wpa_printf(MSG_WARNING, "he_mcs_12_13_supp: beacon update failed");

	ap_for_each_sta(hapd, hostapd_he_mcs_12_13_deauth_sta, NULL);

	return 0;
}

int hostapd_ctrl_iface_set_extn(struct hostapd_data *hapd, char *cmd, char *value)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int val, ret = -1;

	if (os_strcasecmp(cmd, "rnr_member_ess_colocated_en") == 0) {
		val = atoi(value);
		if (val < 0 || val > 1) {
			wpa_printf(MSG_ERROR,
				"rnr_member_ess_colocated_en: Invalid value (expected 0 or 1)");
			return -1;
		}
		if (is_6ghz_freq(hapd->iface->freq)) {
			conf_extn->rnr_ess_colocated_en = val;
			ieee802_11_update_beacons(hapd->iface);
		} else {
			wpa_printf(MSG_ERROR, "rnr_member_ess_colocated_en is valid only for 6 GHz");
			return -1;
		}

	} else if (os_strcasecmp(cmd, "rnr_6ghz_override") == 0) {
		val = atoi(value);
		if (val < 0 || val > 1) {
			wpa_printf(MSG_ERROR, "rnr_6ghz_override: Invalid value (expected 0 or 1)");
			return -1;
		}
		if (!is_6ghz_freq(hapd->iface->freq)) {
			wpa_printf(MSG_ERROR, "rnr_6ghz_override is valid only for 6 GHz");
			return -1;
		}

		conf_extn->rnr_6ghz_override = val;

		ret = ieee802_11_update_beacons(hapd->iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Failed to update beacon");
			return -1;
		}

	} else if (os_strcasecmp(cmd, "nontx_profile_elem_size") == 0) {
		ret = hostapd_set_nontx_optional_vendor_elem_size_extn(hapd, hapd->conf, value);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Failed to set nontx_profile_elem_size");
			return -1;
		}
		return ret;
	} else if (os_strcasecmp(cmd, "he_mcs_12_13_supp") == 0) {
		val = !!atoi(value);
		return hostapd_he_mcs_12_13_supp_extn(hapd, val);
	} else if (os_strcasecmp(cmd, "CSwOpts") == 0) {
		return hostapd_set_cswopts_extn(conf_extn, value);
	} else if (os_strcasecmp(cmd, "wds_ie") == 0) {
		val = atoi(value);
		if (val < 0 || val > 1) {
			wpa_printf(MSG_ERROR,
				   "wds_ie: Invalid value %d (expected 0 or 1)",
				   val);
			return -1;
		}
		hapd->conf->bss_extn.wds_ie = val;
		wpa_printf(MSG_INFO, "WDS IE: %s - wds_ie set to %d",
			   hapd->conf->iface, val);
		ret = ieee802_11_update_beacons(hapd->iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "WDS IE: Failed to update beacons after wds_ie change");
			return -1;
		}
	} else if (os_strcmp(cmd, "allow_3addr_mc") == 0) {
		val = atoi(value);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR,
				   "invalid allow_3addr_mc value %d "
				   "(expected 0 or 1)",  val);
			return -1;
		}
		hapd->conf->bss_extn.allow_3addr_mc = val;
		if (nl80211_set_allow_3addr_mc_extn(hapd->drv_priv, (u8)val))
			return -1;
		wpa_printf(MSG_DEBUG, "allow_3addr_mc: %s allow_3addr_mc=%d",
			   hapd->conf->iface, val);
	}

	return 0;
}

int hostapd_ctrl_iface_get_extn(struct hostapd_data *hapd, char *cmd,
				char *buf, size_t buflen)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	int res = -1;

	if (os_strcasecmp(cmd, "nontx_profile_elem_size") == 0) {
		res = os_snprintf(buf, buflen, "Optional elem size = %u\nVendor elem size = %u\n",
				  hapd->conf->bss_extn.nontx_optional_elem_size,
				  hapd->conf->bss_extn.nontx_vendor_elem_size);
		if (os_snprintf_error(buflen, res))
			return -1;
	} else if (os_strcasecmp(cmd, "he_mcs_12_13_supp") == 0) {
		res = os_snprintf(buf, buflen, "he_mcs_12_13_supp = %u\n",
				  conf_extn->he_mcs_12_13_enabled);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "CSwOpts") == 0) {
		res = os_snprintf(buf, buflen, "CSwOpts=0x%x\n", conf_extn->cswopts);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "wds_ie") == 0) {
		res = os_snprintf(buf, buflen, "wds_ie=%d\n",
				  hapd->conf->bss_extn.wds_ie);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "allow_3addr_mc") == 0) {
		res = os_snprintf(buf, buflen, "allow_3addr_mc=%d\n",
				  hapd->conf->bss_extn.allow_3addr_mc);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}

	return res;
}


int hostapd_ctrl_iface_status_extn(struct hostapd_data *hapd, char *buf,
				   size_t buflen, size_t curr_len)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	size_t len = curr_len;
	int ret;

	if (is_6ghz_freq(hapd->iface->freq)) {
		ret = os_snprintf(buf + len, buflen - len,
				"rnr_member_ess_colocated_en=%d\n",
				conf_extn->rnr_ess_colocated_en);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				"rnr_6ghz_override=%d\n",
				conf_extn->rnr_6ghz_override);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	/* WDS IE status - always reported */
	ret = os_snprintf(buf + len, buflen - len,
			  "wds_ie=%d\n",
			  hapd->conf->bss_extn.wds_ie);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}

#ifdef HOSTAPD
bool hostapd_5ghz_eht_320_channel_bw_extn(struct hostapd_hw_modes *mode,
					  int channel_idx)
{
	static const int allowed[] = { 100, 104, 108, 112, 116, 120,
				       124, 128, 132, 136, 140, 144 };
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];
	bool is_allowed_primary = false;
	size_t k;

	if (!(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_320))
		return false;

	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		int c = allowed[k];
		int idx;

		if (chan->chan == c)
			is_allowed_primary = true;

		idx = hostapd_get_channel_idx(mode, c);
		if (idx == -1 ||
		    (mode->channels[idx].flag & HOSTAPD_CHAN_DISABLED))
			return false;
	}

	return is_allowed_primary;
}
#endif /* HOSTAPD */
