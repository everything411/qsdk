// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include <math.h>
#include <arpa/inet.h>
#include <limits.h>

#include "utils/common.h"
#include "utils/list.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "common/ieee802_11_defs.h"
#include "ap/ap_config.h"
#include "cmn.h"
#include "ap/ieee802_11.h"
#include "dcs.h"
#include "block_channel.h"

static char *hostapd_config_trim_extn(char *str)
{
	char *end;

	if (!str)
		return NULL;

	while (*str == ' ' || *str == '\t' || *str == '\r')
		str++;

	end = str + os_strlen(str);
	while (end > str &&
	       (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		*--end = '\0';

	return str;
}

static int hostapd_config_set_udbg_ip(struct hostapd_config_extn *conf_extn,
				      const char *value, int line)
{
	struct in_addr a4;
	struct in6_addr a6;
	char *tmp;
	char *trimmed;

	tmp = os_strdup(value ? value : "");
	if (!tmp)
		return -1;

	trimmed = hostapd_config_trim_extn(tmp);
	if (!trimmed[0]) {
		wpa_printf(MSG_ERROR,
			   "Line %d: invalid udbg_enh_server_ip (empty value)",
			   line);
		os_free(tmp);
		return -1;
	}

	if (inet_pton(AF_INET, trimmed, &a4) != 1 &&
	    inet_pton(AF_INET6, trimmed, &a6) != 1) {
		wpa_printf(MSG_ERROR,
			   "Line %d: invalid udbg_enh_server_ip '%s' (expected numeric IPv4/IPv6)",
			   line, trimmed);
		os_free(tmp);
		return -1;
	}

	if (os_strlen(trimmed) >= sizeof(conf_extn->udbg_enh_server_ip)) {
		wpa_printf(MSG_ERROR,
			   "Line %d: invalid udbg_enh_server_ip '%s' (maximum %zu characters)",
			   line, trimmed,
			   sizeof(conf_extn->udbg_enh_server_ip) - 1);
		os_free(tmp);
		return -1;
	}

	os_strlcpy(conf_extn->udbg_enh_server_ip, trimmed,
		   sizeof(conf_extn->udbg_enh_server_ip));
	os_free(tmp);

	return 0;
}

static int hostapd_config_set_udbg_app_id(struct hostapd_config_extn *conf_extn,
					  const char *value, int line)
{
	char *tmp;
	char *trimmed;

	tmp = os_strdup(value ? value : "");
	if (!tmp)
		return -1;

	trimmed = hostapd_config_trim_extn(tmp);
	if (!trimmed[0]) {
		wpa_printf(MSG_ERROR,
			   "Line %d: invalid udbg_enh_app_id (empty value)",
			   line);
		os_free(tmp);
		return -1;
	}

	if (os_strlen(trimmed) >= sizeof(conf_extn->udbg_enh_app_id)) {
		wpa_printf(MSG_ERROR,
			   "Line %d: invalid udbg_enh_app_id '%s' (maximum %zu characters)",
			   line, trimmed,
			   sizeof(conf_extn->udbg_enh_app_id) - 1);
		os_free(tmp);
		return -1;
	}

	os_strlcpy(conf_extn->udbg_enh_app_id, trimmed,
		   sizeof(conf_extn->udbg_enh_app_id));
	os_free(tmp);

	return 0;
}

void
hostapd_config_defaults_extn(struct hostapd_config *conf)
{
	struct hostapd_config_extn *conf_extn = &conf->conf_extn;

	/* Configure defaults for extensions */
	conf_extn->rnr_6ghz_colocated_enable = 0;
	conf_extn->rnr_ess_colocated_en = false;
	conf_extn->rnr_6ghz_override = true;

	/* Repeater defaults */
	conf_extn->skip_cac = 0;
	conf_extn->ignorecac = 0;
	conf_extn->ind_rptr = 0;
	conf_extn->cswopts = 0;
	conf_extn->same_ssid = 0;
	conf_extn->repeater = 0;
	conf_extn->disable_iface_during_cac = false; /* Boot-up CAC is enabled by default */

	/*configure qacs_default here*/
	conf_extn->qacs_enable = 1;                 /* QACS enabled */
	conf_extn->qacs_conf.wradar = 1;            /* wradar reject enabled */
	conf_extn->qacs_conf.rep_txpower_policy = 0;/* Option pwr disabled */
	conf_extn->qacs_conf.rank_en = 1;           /* rank enabled */
	conf_extn->qacs_conf.acs_pri_sec20_penalty = ICM_PRI_SEC20_PENALIZATION_FACTOR;
	conf_extn->qacs_conf.min_dwell = 50;        /* msec */
	conf_extn->qacs_conf.max_dwell = 250;       /* msec */
	conf_extn->qacs_conf.dwelltime = 200;   /* msec */
	conf_extn->qacs_conf.dcs_channel_penalty = 0;
        conf_extn->qacs_conf.dbg_module_bitmap = 0x00; /* bitmap of QACS debug modules id */
	conf_extn->qacs_conf.dbg_level = 0; /* QACS debug level */
	conf_extn->block_chan_list.n_chan = 0;

	/* DCS defaults: initialize values; valid_mask reflects only user overrides */
	os_memset(&conf_extn->dcs_conf, 0, sizeof(conf_extn->dcs_conf));
	conf_extn->dcs_conf.intr_detection_threshold = DCS_INTR_DETECTION_THR;
	conf_extn->dcs_conf.phyerr_penalty = DCS_PHYERR_PENALTY;
	conf_extn->dcs_conf.phyerr_threshold = DCS_PHYERR_THRESHOLD;
	conf_extn->dcs_conf.radarerr_threshold = DCS_RADARERR_THRESHOLD;
	conf_extn->dcs_conf.txerr_threshold = DCS_TXERR_THRESHOLD;
	conf_extn->dcs_conf.sample_size = DCS_SAMPLE_SIZE;
	conf_extn->dcs_conf.coch_intr_threshold = DCS_COCH_INTR_THRESHOLD;
	conf_extn->dcs_conf.user_max_cu = DCS_USER_MAX_CU;

	conf_extn->dcs_conf.enable_bitmap = 0;   /* DCS disabled */
	conf_extn->dcs_conf.dcs_event_action = ALLOWED_DCS_EVENT_ACTION_MASK;
	conf_extn->dcs_conf.dcs_event_notify = 0;
#ifdef RDK_ONEWIFI
	conf_extn->dcs_conf.dcs_csa_tbtt = DCS_CSA_TBTT_DEFAULT;
#endif
	/*
	 * Enable random channel selection for AWGN by default; users can
	 * explicitly disable random channel selection via hostapd.conf/CLI by
	 * setting the bitmap to 0.
	 */
	conf_extn->dcs_conf.dcs_random_chan_bitmap = DCS_AWGN_INTF;

	/* OBSS SNR threshold defaults */
	conf_extn->obss_snr_threshold = 0;
	conf_extn->obss_rx_snr_threshold = 0;
	conf_extn->opclass_tbl_idx = OPCLS_TAB_IDX_GLOBAL;

	/* DCS BW reduction control default */
	conf_extn->dcs_conf.bw_reduction_ctrl = 0;

	/* Configure CBS defaults here */
	conf_extn->cbs_params.cbs_enable = 0;
	conf_extn->cbs_params.resttime = 500;
	conf_extn->cbs_params.dwellrest = 500;
	conf_extn->cbs_params.waittime = 1000;
	conf_extn->cbs_params.dwellsplit = 50;
	conf_extn->cbs_params.totaldwell = 200;
	conf_extn->cbs_params.cbs_scan_complete_ts.sec = 0;
	conf_extn->cbs_params.cbs_scan_complete_ts.usec = 0;
	conf_extn->cbs_params.acs_scan_complete_ts.sec = 0;
	conf_extn->cbs_params.acs_scan_complete_ts.usec = 0;
	conf_extn->cbs_params.retrigger_time =
		HOSTAPD_CBS_RETRIGGER_TIME;

	conf_extn->eht_config_ccfs0 = false;

	/* No primary frequencies configured */
	conf_extn->num_primary_freq = 0;

	/* Auto-recovery after NOL VAP down */
	conf_extn->autorecovery_after_nol_vapdown = 1;

	conf_extn->he_mcs_12_13_enabled = DEFAULT_HE_MCS_12_13_SUPPORT;

	conf_extn->udbg_enh_enable = false;
	os_strlcpy(conf_extn->udbg_enh_server_ip, "127.0.0.1",
		   sizeof(conf_extn->udbg_enh_server_ip));
	conf_extn->udbg_enh_server_port = 55555;
	os_strlcpy(conf_extn->udbg_enh_app_id, "hostapd",
		   sizeof(conf_extn->udbg_enh_app_id));
	conf_extn->udbg_enh_records = 512;
	conf_extn->udbg_enh_ring_max_bytes = 2097152;
	conf_extn->udbg_enh_service_delay_ms = 1000;

}

void
hostapd_config_defaults_bss_extn(struct hostapd_bss_config *bss)
{
	struct hostapd_bss_config_extn *bss_extn = &bss->bss_extn;

	/* Configure bss defaults for extensions */
	bss_extn->nontx_optional_elem_size =
		MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE;
	bss_extn->nontx_vendor_elem_size =
		MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE;

	memset(&bss_extn->ht40_intol, 0, sizeof(bss_extn->ht40_intol));
	bss_extn->tpe_common_psd = true;
	bss_extn->tpe_tx_pwr_interp = TPE_REG_EIRP_PSD;
	bss_extn->tpe_punct_channel_tx_pwr = false;
	bss_extn->ecsa_opclass = 0;
	bss_extn->pureg_bss = false;
	memset(&bss_extn->puren_bss, 0, sizeof(bss_extn->puren_bss));
	memset(&bss_extn->pure11ac_bss, 0, sizeof(bss_extn->pure11ac_bss));
	memset(&bss_extn->pure11ax_bss, 0, sizeof(bss_extn->pure11ax_bss));
	/* WDS vendor IE: disabled by default */
	bss_extn->wds_ie = 0;
	bss_extn->vht_mcs_10_11_supp = DEFAULT_VHT_MCS_10_11_SUPPORT;
	bss_extn->vht_mcs_10_11_nq2q_peer_supp = DEFAULT_VHT_MCS_10_11_NQ2Q_PEER_SUPPORT;
	bss_extn->he_400ns_sgi_supp = DEFAULT_HE_400NS_SGI_SUPPORT;
	bss_extn->he_2xltf_160_80p80_supp = DEFAULT_HE_2XLTF_160_80P80_SUPPORT;
}

int
hostapd_config_fill_extn(struct hostapd_config *conf,
			 struct hostapd_bss_config *bss,
			 const char *buf, char *pos, int line)
{
	struct hostapd_config_extn *conf_extn = &conf->conf_extn;
	int val;

	if (!conf_extn)
		return -1;

	if (os_strcmp(buf, "rnr_member_ess_colocated_en") == 0) {
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR, "Line %d: invalid value for rnr_member_ess_colocated_en %d (expected 0 or 1)",
				line, val);
			return -1;
		}
		conf_extn->rnr_ess_colocated_en = val;
	} else if (os_strcmp(buf, "rnr_6ghz_override") == 0) {
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR, "Line %d: invalid value for rnr_6ghz_override %d (expected 0 or 1)", line, val);
			return -1;
		}
		conf_extn->rnr_6ghz_override = val;
	} else if (os_strcmp(buf, "athnewind") == 0) {
		conf_extn->ind_rptr = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "repeater") == 0) {
		conf_extn->repeater = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "same_ssid") == 0) {
		conf_extn->same_ssid = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "skip_cac") == 0) {
		conf_extn->skip_cac = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "ignorecac") == 0) {
		conf_extn->ignorecac = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "uplink_csa") == 0) {
		if ((IS_CSH_RCSA_TO_UPLINK_ENABLED(conf_extn->cswopts) ||
		     IS_CSH_PROCESS_RCSA_ENABLED(conf_extn->cswopts)) &&
		     atoi(pos)) {
			wpa_printf(MSG_ERROR,
				   "Rejecting uplink_csa config, RCSA is already enabled");
			return 0;
		}
		conf_extn->uplink_csa = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "rpt_max_phy") == 0) {
		conf_extn->rpt_max_phy = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "rptr_allow_chan_sw") == 0) {
		conf_extn->rptr_allow_chan_sw = atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "disable_iface_during_cac") == 0) {
		conf_extn->disable_iface_during_cac = !!atoi(pos);
		return 0;
	} else if (os_strcmp(buf, "qacs_enable") == 0) {
		conf_extn->qacs_enable = atoi(pos);
	} else if (os_strcasecmp(buf, "nontx_profile_elem_size") == 0) {
		/* Values are sanitized and set in hostapd_ctrl_iface_set_extn */
		return 0;
	} else if (os_strcmp(buf, "repurpose_mode") == 0) {
		int mode = atoi(pos);

		if (!hostapd_is_valid_repurpose_mode_extn(mode)) {
			wpa_printf(MSG_ERROR,
				   "Line %d: Invalid repurpose_mode %d (allowed 1..3)",
				   line, mode);
			return -1;
		}
		bss->bss_extn.repurpose_mode = (u8) mode;
	} else if (os_strcmp(buf, "repurpose_he_width") == 0) {
		u16 val = (u16) atoi(pos);

		if (val != 20 && val != 40 && val != 80 && val != 160) {
			wpa_printf(MSG_ERROR,
				   "Repurpose: invalid repurpose he width configured");
			return -1;
		}
		conf_extn->repurpose_he_width = val;
		conf_extn->user_repurpose_he_width = val;
		return 0;
	} else if (os_strcmp(buf, "repurpose_vht_width") == 0) {
		u16 val = (u16) atoi(pos);

		if (val != 20 && val != 40 && val != 80 && val != 160) {
			wpa_printf(MSG_ERROR,
				   "Repurpose: invalid repurpose vht width configured");
			return -1;
		}
		conf_extn->repurpose_vht_width = val;
		conf_extn->user_repurpose_vht_width = val;
		return 0;
	} else if (os_strcmp(buf, "acs_wradar") == 0) {
		conf_extn->qacs_conf.wradar = atoi(pos);
	} else if (os_strcmp(buf, "acs_txpwr_opt") == 0) {
		int val = atoi(pos);
		if (val != 1 && val != 2)
			val = 0;
		conf_extn->qacs_conf.rep_txpower_policy = val;
	} else if (os_strcmp(buf, "acs_rank_en") == 0) {
		conf_extn->qacs_conf.rank_en = atoi(pos);
	} else if (os_strcmp(buf, "acs_pri_sec20_penalty") == 0) {
		int val = atoi(pos);

		if (val < 0 || val > 100) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid acs_pri_sec20_penalty %d (allowed 0..100)",
				   line, val);
			return -1;
		}

		conf_extn->qacs_conf.acs_pri_sec20_penalty = val;

	} else if (os_strcmp(buf, "acs_dbgtrace") == 0) {
		/* Expected format: <value>
		 * Lower 0x00FF bits -> debug level
		 * Upper 0xFF00 bits -> module bitmap
		 * Example: "0x0201" means module_bitmap=0x02, dbg_level=0x01
		 */
		char *endptr;
		unsigned long val = strtoul(pos, &endptr, 0);
		if (*endptr) {
			wpa_printf(MSG_ERROR, "%s: Invalid acs_dbgtrace value '%s' (0xFF00=module mask, 0x00FF=debug level)", __func__, pos);
			return -1;
		}

		conf_extn->qacs_conf.dbg_module_bitmap = (u_int16_t)((val & 0xFF00) >> 8);
		conf_extn->qacs_conf.dbg_level = (int)(val & 0x00FF);

	} else if (os_strcmp(buf, "acsmin_dwell") == 0) {
		conf_extn->qacs_conf.min_dwell = atoi(pos);
		if (conf_extn->qacs_conf.min_dwell < 50)
			conf_extn->qacs_conf.min_dwell = 50;
	} else if (os_strcmp(buf, "acsmax_dwell") == 0) {
		conf_extn->qacs_conf.max_dwell = atoi(pos);
	} else if (os_strcmp(buf, "dcs_channel_penalty") == 0) {
		int val = atoi(pos);

		if (val < 0 || val > 100) {
			wpa_printf(MSG_ERROR,
				   "Line %d: Invalid dcs_channel_penalty %d (expected 0..100)",
				   line, val);
			return -1;
		}
		conf_extn->qacs_conf.dcs_channel_penalty = val;
	} else if (os_strcmp(buf, "dwelltime") == 0) {
		int dt = atoi(pos);
		/* Enforce dwelltime bounds: must be between min_dwell and max_dwell */
		if (conf_extn->qacs_conf.min_dwell &&
		    dt <= conf_extn->qacs_conf.min_dwell)
			dt = conf_extn->qacs_conf.min_dwell;
		if (conf_extn->qacs_conf.max_dwell &&
		    dt >= conf_extn->qacs_conf.max_dwell)
			dt = conf_extn->qacs_conf.max_dwell;

		conf_extn->qacs_conf.dwelltime = dt;

	} else if (os_strcmp(buf, "acs_2g_scan_all") == 0) {
		/*
		 * Control 2.4 GHz ACS selection set:
		 * 0 = restrict to non-overlapping primaries (1/6/11) for 20 MHz
		 * 1 = allow all available 2.4 GHz channels for 20 MHz
		 * 40 MHz constraints (1/6 for 40+, 6/11 for 40−) stay enforced.
		 */
		int v = atoi(pos);
		conf_extn->qacs_conf.acs_2g_scan_all = (v != 0);

	} else if (os_strcmp(buf, "dcs_enable") == 0) {
		char *endptr;
		unsigned long v;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		errno = 0;
		v = strtoul(pos, &endptr, 0);
		while (*endptr == ' ' || *endptr == '\t')
			endptr++;
		if (errno != 0 || endptr == pos || *endptr != '\0' ||
		    v > 0xFFFF) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_enable '%s' (expected 16-bit value)",
				   line, pos);
			conf_extn->dcs_conf.enable_bitmap = 0;
			return 0;
		}

		if (v & ~ALLOWED_DCS_MASK) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_enable '%s' (allowed bits mask: 0x%04x)",
				   line, pos, ALLOWED_DCS_MASK);
			conf_extn->dcs_conf.enable_bitmap = 0;
			return 0;
		}

		conf_extn->dcs_conf.enable_bitmap = (u16) v;
	} else if (os_strcmp(buf, "dcs_event_action") == 0) {
		char *endptr;
		unsigned long v;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		errno = 0;
		v = strtoul(pos, &endptr, 0);
		while (*endptr == ' ' || *endptr == '\t')
			endptr++;
		if (errno != 0 || endptr == pos || *endptr != '\0' || v > 0xFFFF) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_event_action '%s' (expected 16-bit value)",
				   line, pos);
			conf_extn->dcs_conf.dcs_event_action = 0;
			return 0;
		}

		if (v & ~ALLOWED_DCS_EVENT_ACTION_MASK) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_event_action '%s' (allowed bits mask: 0x%04x)",
				   line, pos, ALLOWED_DCS_EVENT_ACTION_MASK);
			conf_extn->dcs_conf.dcs_event_action = 0;
			return 0;
		}

		conf_extn->dcs_conf.dcs_event_action = (u16) v;
	} else if (os_strcmp(buf, "dcs_event_notify") == 0) {
		char *endptr;
		unsigned long v;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		errno = 0;
		v = strtoul(pos, &endptr, 0);
		while (*endptr == ' ' || *endptr == '\t')
			endptr++;
		if (errno != 0 || endptr == pos || *endptr != '\0' || v > 0xFFFF) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_event_notify '%s' (expected 16-bit value)",
				   line, pos);
			conf_extn->dcs_conf.dcs_event_notify = 0;
			return 0;
		}

		if (v & ~ALLOWED_DCS_EVENT_ACTION_MASK) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_event_notify '%s' (allowed bits mask: 0x%04x)",
				   line, pos, ALLOWED_DCS_EVENT_ACTION_MASK);
			conf_extn->dcs_conf.dcs_event_notify = 0;
			return 0;
		}

		conf_extn->dcs_conf.dcs_event_notify = (u16) v;
	} else if (os_strcmp(buf, "dcs_random_chan_bitmap") == 0) {
		/*
		 * Parse and set DCS random channel enable bitmap from hostapd.conf.
		 * Accept numeric values (decimal or hex like 0x1F). Validate range
		 * and allowed bits (0..4 i.e., 0x001F) to align with CLI handling.
		 */
		unsigned long tmp;
		char *endptr = NULL;
		u16 val;

		errno = 0;
		tmp = strtoul(pos, &endptr, 0);
		if (errno != 0 || endptr == pos) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid dcs random_chan_bitmap value '%s'",
				   line, pos);
			return -1;
		}

		while (endptr && *endptr == ' ')
			endptr++;

		if (endptr && *endptr != '\0') {
			wpa_printf(MSG_ERROR,
				   "Line %d: trailing characters in dcs random_chan_bitmap '%s'",
				   line, pos);
			return -1;
		}

		if (tmp > 0xFFFFUL) {
			wpa_printf(MSG_ERROR,
				   "Line %d: dcs random_chan_bitmap out of range '%s'",
				   line, pos);
			return -1;
		}

		val = (u16) tmp;
		/* Allow only bits 0..4; update mask if needed. */
		if (val & ~0x001Fu) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid dcs random_chan_bitmap 0x%04x (only bits 0..4 allowed)",
				   line, val);
			return -1;
		}

		conf_extn->dcs_conf.dcs_random_chan_bitmap = val;
		wpa_printf(MSG_DEBUG,
			   "DCS: dcs_random_chan_bitmap set to 0x%04x (%u)",
			   conf_extn->dcs_conf.dcs_random_chan_bitmap,
			   conf_extn->dcs_conf.dcs_random_chan_bitmap);
	} else if (os_strcmp(buf, "obss_snr_threshold") == 0) {
		val = atoi(pos);
		if (val < OBSS_SNR_MIN || val > OBSS_SNR_MAX) {
			wpa_printf(MSG_ERROR,
				   "Line %d: obss_snr_threshold out of range (%d-%d): %d",
				   line, OBSS_SNR_MIN, OBSS_SNR_MAX, val);
			return -1;
		}
		conf_extn->obss_snr_threshold = (u8)val;
	} else if (os_strcmp(buf, "obss_rx_snr_threshold") == 0) {
		val = atoi(pos);
		if (val < OBSS_SNR_MIN || val > OBSS_SNR_MAX) {
			wpa_printf(MSG_ERROR,
				   "Line %d: obss_rx_snr_threshold out of range (%d-%d): %d",
				   line, OBSS_SNR_MIN, OBSS_SNR_MAX, val);
			return -1;
		}
		conf_extn->obss_rx_snr_threshold = (u8)val;
	} else if (os_strcmp(buf, "dcs_bw_reduction_ctrl") == 0) {
		/* Parse and set DCS BW reduction control mask from hostapd.conf.
		 * Accept decimal or hex (e.g., 0x3). Validate 16-bit range to
		 * align with CLI handling
		 */
		unsigned long tmp;
		u16 bw_ctrl_val;
		char *endptr = NULL;
		errno = 0;

		tmp = strtoul(pos, &endptr, 0);
		if (errno != 0 || endptr == pos) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid value for dcs_bw_reduction_ctrl '%s'",
				   line, pos);
			return -1;
		}
		while (endptr && *endptr == ' ')
			endptr++;
		if (endptr && *endptr != '\0') {
			wpa_printf(MSG_ERROR,
				   "Line %d: trailing characters in dcs_bw_reduction_ctrl '%s'",
				   line, pos);
			return -1;
		}
		bw_ctrl_val = (u16) tmp;
		if (bw_ctrl_val & ~0x001Fu) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid dcs_bw_reduction_ctrl 0x%04x",
				   line, bw_ctrl_val);
			return -1;
		}
		conf_extn->dcs_conf.bw_reduction_ctrl = bw_ctrl_val;
		wpa_printf(MSG_DEBUG, "DCS: bw_reduction_ctrl set to 0x%04x",
			   conf_extn->dcs_conf.bw_reduction_ctrl);

	} else if (os_strcmp(buf, "cbs_enable") == 0) {
		conf_extn->cbs_params.cbs_enable = atoi(pos);
	} else if (os_strcmp(buf, "cbs_resttime") == 0) {
		conf_extn->cbs_params.resttime = atoi(pos);
	} else if (os_strcmp(buf, "cbs_dwellrest") == 0) {
		conf_extn->cbs_params.dwellrest = atoi(pos);
	} else if (os_strcmp(buf, "cbs_waittime") == 0) {
		conf_extn->cbs_params.waittime = atoi(pos);
	} else if (os_strcmp(buf, "cbs_dwellsplit") == 0) {
		conf_extn->cbs_params.dwellsplit = atoi(pos);
	} else if (os_strcmp(buf, "cbs_totaldwell") == 0) {
		conf_extn->cbs_params.totaldwell = atoi(pos);
	} else if (os_strcmp(buf, "cbs_retrigger_time") == 0) {
		conf_extn->cbs_params.retrigger_time = atoi(pos);

	} else if (os_strcmp(buf, "vap_submode") == 0) {
		u8 val = atoi(pos);
		if (val > QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MAX) {
			wpa_printf(MSG_ERROR, "Line %d: Invalid vap_submode: %d",
				   line, val);
			return 1;
		}
		bss->bss_extn.vap_submode = val;
	} else if (os_strcmp(buf, "he_mcs_12_13_supp") == 0) {
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR, "Line %d: invalid he_mcs_12_13_supp %d (expected 0 or 1)",
				   line, val);
			return -1;
		}
		conf->conf_extn.he_mcs_12_13_enabled = val;
	} else if (os_strcasecmp(buf, "CSwOpts") == 0) {
		if (hostapd_set_cswopts_extn(conf_extn, pos) < 0)
			return 1;
	} else if (os_strcmp(buf, "acs_block_chan_list") == 0) {
		return hostapd_config_parse_block_chanlist(conf_extn, pos);
	} else if (os_strcmp(buf, "acs_periodic_interval") == 0) {
		val = atoi(pos);
		if (val != 0 && (val < 60 || val > 86400)) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid acs_periodic_interval %d (expected 0 or 60..86400)",
				   line, val);
			return -1;
		}
		conf_extn->acs_periodic_interval = val;
	} else if (os_strcmp(buf, "udbg_enh_enable") == 0) {
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid udbg_enh_enable %d (expected 0 or 1)",
				   line, val);
			return -1;
		}
		conf_extn->udbg_enh_enable = val;
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_server_ip") == 0) {
		if (hostapd_config_set_udbg_ip(conf_extn, pos, line) < 0)
			return -1;
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_server_port") == 0) {
		conf_extn->udbg_enh_server_port = (int) atol(pos);
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_app_id") == 0) {
		if (hostapd_config_set_udbg_app_id(conf_extn, pos, line) < 0)
			return -1;
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_records") == 0) {
		conf_extn->udbg_enh_records = (size_t) atol(pos);
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_ring_max_bytes") == 0) {
		conf_extn->udbg_enh_ring_max_bytes = (size_t) atol(pos);
		return 0;
	} else if (os_strcmp(buf, "udbg_enh_service_delay_ms") == 0) {
		conf_extn->udbg_enh_service_delay_ms = (uint32_t) atol(pos);
		if (!conf_extn->udbg_enh_service_delay_ms) {
			wpa_printf(MSG_ERROR,
				   "Line %d: invalid udbg_enh_service_delay_ms 0",
				   line);
			return -1;
		}
		return 0;
	} else if (os_strcmp(buf, "wds_ie") == 0) {
		/*
		 * wds_ie - WDS vendor IE advertisement control
		 *
		 * 0 = disabled (default)
		 * 1 = enabled: AP advertises WDS vendor IE (OUI 00:13:84,
		 *     type 0x01) in beacon, probe response, and association
		 *     response frames.  Enables WDS mode for stations that
		 *     mutually advertise WDS_IE_CAP_STA.
		 */
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR,
				   "Invalid wds_ie value %d (expected 0 or 1)", val);
			return -1;
		}
		bss->bss_extn.wds_ie = val;
		wpa_printf(MSG_DEBUG, "WDS IE: %s wds_ie=%d", bss->iface, val);
	} else if (os_strcmp(buf, "allow_3addr_mc") == 0) {
		val = atoi(pos);
		if (val != 0 && val != 1) {
			wpa_printf(MSG_ERROR,
				   "Invalid allow_3addr_mc value %d (expected 0 or 1)",
				   val);
			return -1;
		}
		bss->bss_extn.allow_3addr_mc = val;
		wpa_printf(MSG_DEBUG, "allow_3addr_mc: %s allow_3addr_mc=%d",
			   bss->iface, val);
	} else {
		wpa_printf(MSG_INFO, "%s:%d> error buf %s =====> Not found", __func__, __LINE__, buf);
		return -1;
	}

	return 0;
}
