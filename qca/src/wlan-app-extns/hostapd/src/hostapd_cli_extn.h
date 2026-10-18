/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef HOSTAPD_CLI_EXTN
#define HOSTAPD_CLI_EXTN

struct wpa_ctrl;

int hostapd_cli_cmd_set_esp_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_esp_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_non_prior_penalty_extn(struct wpa_ctrl *ctrl, int argc,
					       char *argv[]);
int hostapd_cli_cmd_get_non_prior_penalty_extn(struct wpa_ctrl *ctrl, int argc,
					       char *argv[]);
int hostapd_cli_cmd_set_rnr_6ghz_colocated_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_rnr_6ghz_colocated_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_channel_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_opclass_tbl_idx_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_opclass_tbl_idx_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_countryie_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_countryie_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_ecsa_opclass_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_g_ecsa_opclass_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);

#ifdef CONFIG_IEEE80211AC
int hostapd_cli_cmd_get_mu_cap_war_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_mu_cap_war_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
#endif /* CONFIG_IEEE80211AC */

int hostapd_cli_cmd_sync_iface_freq_extn(struct wpa_ctrl *ctrl, int argc,
					 char *argv[]);
int hostapd_cli_cmd_dfs_no_wradar_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_dfs_no_wradar_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_allow_scan_on_dfs_chan_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_allow_scan_on_dfs_chan_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_dcs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_dcs_wlan_intr_params(struct wpa_ctrl *ctrl, int argc,
					    char *argv[]);
int hostapd_cli_cmd_set_primary_chans(struct wpa_ctrl *ctrl, int argc,
				      char *argv[]);
int hostapd_cli_cmd_get_primary_chans(struct wpa_ctrl *ctrl, int argc,
				      char *argv[]);
int hostapd_cli_cmd_disable_opclass_chans(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_obss_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						char *argv[]);
int hostapd_cli_cmd_get_obss_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						char *argv[]);
int hostapd_cli_cmd_set_obss_rx_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						   char *argv[]);
int hostapd_cli_cmd_get_obss_rx_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						   char *argv[]);
int hostapd_cli_cmd_set_autorecovery_after_nol_vapdown(struct wpa_ctrl *ctrl,
						       int argc, char *argv[]);
int hostapd_cli_cmd_ignorecac_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);

int hostapd_cli_cmd_get_mapc_peer_params_extn(struct wpa_ctrl *ctrl, int argc,
					      char *argv[]);

#ifdef CONFIG_QCN_EXTN
int hostapd_cli_cmd(struct wpa_ctrl *ctrl, const char *cmd,
		    int min_args, int argc, char *argv[]);

int wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd);
int hostapd_cli_acs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_ht40intol(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_ht40intol(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cbs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[]);

int hostapd_cli_cmd_set_eht_config_ccfs0(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_eht_config_ccfs0(struct wpa_ctrl *ctrl, int argc, char *argv[]);

int hostapd_cli_cmd_set_tpe_common_psd(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_tpe_common_psd(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_tpe_punct_channel_tx_pwr(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_tpe_punct_channel_tx_pwr(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_tpe_tx_pwr_interp(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_tpe_tx_pwr_interp(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_pureg(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_pureg(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_puren(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_puren(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_disablecoexist(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_disablecoexist(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_pure11ac(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_pure11ac(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_set_pure11ax(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int hostapd_cli_cmd_get_pure11ax(struct wpa_ctrl *ctrl, int argc, char *argv[]);
int _wpa_ctrl_command_large(struct wpa_ctrl *ctrl, const char *cmd, int print);

#define HOSTAPD_CLI_CMDS_EXTN \
	{ "set_esp", hostapd_cli_cmd_set_esp_extn, NULL, \
		"<param> <value> = set ESP param (esp_airtime 0-255, " \
		"esp_ppdu_dur 0-255, esp_ba_window 0-7, enable_esp 0-1)" }, \
	{ "get_esp", hostapd_cli_cmd_get_esp_extn, NULL, \
		"= get ESP parameters from driver" }, \
	{ "set_vlp_non_prior_penalty", hostapd_cli_cmd_set_non_prior_penalty_extn, NULL, \
		"<0-100> = set 6 GHz VLP non-priority channel penalty (%)" }, \
	{ "get_vlp_non_prior_penalty", hostapd_cli_cmd_get_non_prior_penalty_extn, NULL, \
		"= get 6 GHz VLP non-priority channel penalty (%)" }, \
	{ "rnr_6ghz_colocated", hostapd_cli_cmd_set_rnr_6ghz_colocated_extn, NULL, \
		"<rnr_mode> <frm_val> = config rnr 6ghz colocated" }, \
	{ "get_rnr_6ghz_colocated", hostapd_cli_cmd_get_rnr_6ghz_colocated_extn, NULL, \
		"= get rnr 6ghz colocated" }, \
	{ "set_channel", hostapd_cli_cmd_set_channel_extn, NULL, \
		"<channel> = set channel by restarting interface" }, \
	{ "set_opclass_tbl", hostapd_cli_cmd_set_opclass_tbl_idx_extn, NULL, \
		"<0-6> = set operating class table index (0:none,1:US,2:EUROPE,3:JAPAN,4:GLOBAL,6:CHINA)" }, \
	{ "get_opclass_tbl", hostapd_cli_cmd_get_opclass_tbl_idx_extn, NULL, \
		"= get current operating class table index" }, \
	{ "countryie", hostapd_cli_cmd_countryie_extn, NULL, \
		"<0|1> = disable/enable Country IE in beacon" }, \
	{ "get_countryie", hostapd_cli_cmd_get_countryie_extn, NULL, \
		"= get Country IE advertisement status" }, \
	{ "acs", hostapd_cli_acs_extn, NULL, \
		"get_status           : get ACS current status\n" \
		"rank_en <1|0>        : enable/disable channel ranking\n" \
		"get_rank_en          : get channel ranking enable state\n" \
		"pri_sec20_penalty <0-100> : set PRI_SEC20 penalty\n" \
		"get_pri_sec20_penalty : get PRI_SEC20 penalty\n" \
		"qacs_enable <1|0>    : enable/disable QACS extension\n" \
		"get_qacs_enable      : get QACS extension enable state\n" \
		"noscan <1|0>         : enable/disable noscan mode\n" \
		"get_noscan           : get noscan mode state\n" \
		"dfs_exclude <1|0>    : enable/disable DFS channel exclusion\n" \
		"get_dfs_exclude      : get DFS channel exclusion state\n" \
		"dwelltime <ms>       : set dwell time in milliseconds\n" \
		"get_dwell            : get dwell time in milliseconds\n" \
		"dbgtrace <value>     : set debug (0x00FF=level, 0xFF00=module mask)\n" \
		"get_dbgtrace         : get debug/trace mask\n" \
		"wradar <0|1>         : enable/disable excluding weather radar channels\n" \
		"get_wradar           : get weather radar handling state\n" \
		"txpwr_opt <0|1|2>    : set the tx pwr optimization state(0 = disable, 1 = optimize throughput, 2 = optimize range)\n" \
		"get_txpwr_opt        : get tx power optimization state\n" \
		"6g_only_psc <1|0>    : restrict 6 GHz to PSC channels only\n" \
		"get_6g_only_psc      : get the state of restricting 6 GHz to PSC channels only\n" \
		"acs invoke <0|1>     : invoke ACS (0=dynamicACS+CSA)|(1=DynamicACS)\n" \
		"show_neighbor_report : print ACS neighbor report\n" \
		"periodic_interval <sec> : set periodic ACS interval (0 disables, max 86400)\n" \
		"get_periodic_interval : get periodic ACS interval\n" \
	}, \
	{ "cbs", hostapd_cli_cbs_extn, NULL, \
		"enable <0|1|2>       : enable/disable cbs scan (0:disable | 1:enable CBS scan once | 2:enable cbs scan to run continuously)\n" \
		"g_enable             : get continuous background scan enable state\n" \
		"resttime <ms>        : set rest time in milliseconds\n" \
		"g_resttime           : get rest time in milliseconds\n" \
		"dwellrest <ms>       : set dwell rest time in milliseconds\n" \
		"g_dwellrest          : get dwell rest time in milliseconds\n" \
		"waittime <ms>        : set wait time in milliseconds\n" \
		"g_waittime           : get wait time in milliseconds\n" \
		"dwellsplit <value>   : set dwell split value\n" \
		"g_dwellsplit         : get dwell split value\n" \
		"totaldwell <value>   : set total dwell value\n" \
		"g_totaldwell         : get total dwell value\n" \
		}, \
	{ "mu_cap_war", hostapd_cli_cmd_mu_cap_war_extn, NULL, \
		"enable/disable VHT MU-MIMO capability for MU_CAP_WAR clients" }, \
	{ "sync_iface_freq", hostapd_cli_cmd_sync_iface_freq_extn, NULL, \
		"= sync runtime interface frequency from current config" }, \
	{ "get_mu_cap_war", hostapd_cli_cmd_get_mu_cap_war_extn, NULL, \
		"get MU_CAP_WAR status" }, \
	{ "set_obss_snr_threshold", hostapd_cli_cmd_set_obss_snr_threshold_extn, NULL, \
		"<snr> = set OBSS SNR threshold in dB (per-radio)" }, \
	{ "get_obss_snr_threshold", hostapd_cli_cmd_get_obss_snr_threshold_extn, NULL, \
		"= get current OBSS SNR threshold in dB" }, \
	{ "set_obss_rx_snr_threshold", hostapd_cli_cmd_set_obss_rx_snr_threshold_extn, NULL, \
		"<snr> = set OBSS RX SNR threshold in dB (per-radio)" }, \
	{ "get_obss_rx_snr_threshold", hostapd_cli_cmd_get_obss_rx_snr_threshold_extn, NULL, \
		"= get current OBSS RX SNR threshold in dB" }, \
	{ "dfs_no_wradar", hostapd_cli_cmd_dfs_no_wradar_extn, NULL, \
		"<0|1> = avoid ETSI weather radar channels in DFS random channel selection" }, \
	{ "get_dfs_no_wradar", hostapd_cli_cmd_get_dfs_no_wradar_extn, NULL, \
		"= get dfs_no_wradar state (0|1)" }, \
	{ "allow_scan_on_dfs_chan", hostapd_cli_cmd_set_allow_scan_on_dfs_chan_extn, NULL, \
		"<0|1> = enable/disable DFS channel switch on ACS trigger during DCS WLAN interference" }, \
	{ "get_allow_scan_on_dfs_chan", hostapd_cli_cmd_get_allow_scan_on_dfs_chan_extn, NULL, \
		"= get allow_scan_on_dfs_chan state (0|1)" }, \
	{ "ignorecac", hostapd_cli_cmd_ignorecac_extn, NULL, \
		"[0|1] = get/set whether to ignore DFS CAC" },\
	{ "dcs", hostapd_cli_cmd_dcs_extn, NULL, \
		"enable		: enable DCS configuration\n" \
		"bw_reduction_ctrl	: <mask> = set DCS bw reduction control\n" \
		"csa_tbtt	: CSA TBTT value for DCS\n" \
		"event_action	: <mask> = set DCS event action bitmap (bit set = process, bit clear = discard)\n" \
		"event_notify	: <mask> set DCS event notification bitmap\n" \
		"wlan_intr_params	: phyerr_penalty <val> phyerr_threshold <val> radarerr_threshold <val> coch_intr_threshold <val> txerr_threshold <val> user_max_cu <val> intr_detection_threshold <val> sample_size <val> = set DCS WLAN INTR params\n" \
		"get_event_action	: get DCS event action bitmap\n" \
		"get_event_notify	: get DCS event notification\n" \
		"set_dcs_enable_timer	: <sec> = set DCS re-enable time\n" \
		"get_dcs_enable_timer	: get DCS re-enable time\n" \
		"sim            : simulate DCS interference\n" \
	}, \
	{ "set_primary_chans", hostapd_cli_cmd_set_primary_chans, NULL, \
	  "[<ch1> <ch2> ...]\n" \
	  "  = set primary channel list (space-separated channel numbers);" \
	  " no args clears list" }, \
	{ "get_primary_chans", hostapd_cli_cmd_get_primary_chans, NULL, \
	  "= get current primary channel list" }, \
	{ "disable_opclass_chans", hostapd_cli_cmd_disable_opclass_chans, NULL, \
	  "<0|1> <opclass> <ch1> [<ch2> ...] = enable/disable channels in opclass" }, \
	{ "set_ht40intol", hostapd_cli_cmd_set_ht40intol, NULL, \
		"<value 0|1> = set ht40 intolerant bit" }, \
	{ "get_ht40intol", hostapd_cli_cmd_get_ht40intol, NULL, \
		" = get current ht40 intolerant bit value" }, \
	{ "set_eht_config_ccfs0", hostapd_cli_cmd_set_eht_config_ccfs0, NULL, \
		"<value 0|1> :Set EHT OP ccfs0 field config" }, \
	{ "get_eht_config_ccfs0", hostapd_cli_cmd_get_eht_config_ccfs0, NULL, \
		"Get EHT OP ccfs0 field config" }, \
	{ "set_tpe_common_psd", hostapd_cli_cmd_set_tpe_common_psd, NULL, \
		"<value 0|1> :Set config for common psd power in TPE IE" }, \
	{ "get_tpe_common_psd", hostapd_cli_cmd_get_tpe_common_psd, NULL, \
		"Get config for common psd power in TPE IE"}, \
	{ "set_autorecovery_after_nol_vapdown", \
	  hostapd_cli_cmd_set_autorecovery_after_nol_vapdown, NULL, \
	  "<1|0> = enable/disable VAP auto-recovery after NOL expiry" }, \
	{ "set_tpe_pwr_unit", hostapd_cli_cmd_set_tpe_tx_pwr_interp, NULL, \
		"<value 0|1> :Set tx power interpretation unit in TPE IE (0:TPE_REG_EIRP_PSD, 1:TPE_REG_EIRP)" }, \
	{ "get_tpe_pwr_unit", hostapd_cli_cmd_get_tpe_tx_pwr_interp, NULL, \
		"Get tx power interpretation unit in TPE IE (0:TPE_REG_EIRP_PSD, 1:TPE_REG_EIRP)"}, \
	{ "set_tpe_punct_channel_tx_pwr", hostapd_cli_cmd_set_tpe_punct_channel_tx_pwr, NULL, \
		"<value 0|1> :Enable/disable punctured channel min power in TPE IE" }, \
	{ "get_tpe_punct_channel_tx_pwr", hostapd_cli_cmd_get_tpe_punct_channel_tx_pwr, NULL, \
		"Get punctured channel min power config in TPE IE" }, \
	{ "ecsa_opclass", hostapd_cli_cmd_ecsa_opclass_extn, NULL, \
		"<opclass> = override operating class in ECSA IE" }, \
	{ "g_ecsa_opclass", hostapd_cli_cmd_g_ecsa_opclass_extn, NULL, \
		"= get configured ECSA IE operating class override" }, \
	{ "set_pureg", hostapd_cli_cmd_set_pureg, NULL, \
		"<value 0|1> :Enable/disable pureg" }, \
	{ "get_pureg", hostapd_cli_cmd_get_pureg, NULL, \
		"Get pureg config" }, \
	{ "set_puren", hostapd_cli_cmd_set_puren, NULL, \
		"<value 0|1> :Enable/disable puren mode" }, \
	{ "get_puren", hostapd_cli_cmd_get_puren, NULL, \
		"Get puren mode config" }, \
	{ "set_disablecoexist", hostapd_cli_cmd_set_disablecoexist, NULL, \
		"<value 0|1> :1-disable coexistence, 0-enable coexistence" }, \
	{ "get_disablecoexist", hostapd_cli_cmd_get_disablecoexist, NULL, \
		"Get disable coexistence config" }, \
	{ "set_pure11ac", hostapd_cli_cmd_set_pure11ac, NULL, \
		"<value 0|1> :Enable/disable pure11ac mode" }, \
	{ "get_pure11ac", hostapd_cli_cmd_get_pure11ac, NULL, \
		"Get pure11ac mode config" }, \
	{ "set_pure11ax", hostapd_cli_cmd_set_pure11ax, NULL, \
		"<value 0|1> :Enable/disable pure11ax mode" }, \
	{ "get_pure11ax", hostapd_cli_cmd_get_pure11ax, NULL, \
		"Get pure11ax mode config" }, \
	{ "get_mapc_peer_params", hostapd_cli_cmd_get_mapc_peer_params_extn, NULL, \
		"<bssid> = retrieve cached MAPC params for the named peer" }, \

#define HOSTAPD_CLI_CMD_FIELDS_EXTN \
	"he_mcs_12_13_supp",
#else
#define HOSTAPD_CLI_CMDS_EXTN
#define HOSTAPD_CLI_CMD_FIELDS_EXTN

#endif /* CONFIG_QCN_EXTN */

#endif /* HOSTAPD_CLI_EXTN */
