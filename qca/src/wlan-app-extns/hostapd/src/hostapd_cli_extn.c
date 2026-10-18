// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include <dirent.h>

#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#include "hostapd_cli_extn.h"
#include "utils/os.h"

extern struct wpa_ctrl *ctrl_conn;

/* Add your cli handling for extensions here */
int hostapd_cli_cmd_set_esp_extn(struct wpa_ctrl *ctrl, int argc,
				 char *argv[])
{
	char buf[128] = {'\0'};
	int res;

	if (argc != 2) {
		printf("Invalid 'set_esp' command - usage: set_esp <param> <value>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_ESP %s=%s", argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long SET_ESP command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_esp_extn(struct wpa_ctrl *ctrl, int argc,
				 char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_ESP");
}

int hostapd_cli_cmd_set_non_prior_penalty_extn(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	char buf[64] = {'\0'};
	int res;

	if (argc != 1) {
		printf("Usage: set_vlp_non_prior_penalty <0-100>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_VLP_NON_PRIOR_PENALTY %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long SET_VLP_NON_PRIOR_PENALTY command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_non_prior_penalty_extn(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_VLP_NON_PRIOR_PENALTY");
}

int hostapd_cli_cmd_set_rnr_6ghz_colocated_extn(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "RNR_6GHZ_COLOCATED", 2, argc, argv);
}

int hostapd_cli_cmd_get_rnr_6ghz_colocated_extn(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_RNR_6GHZ_COLOCATED");
}

int hostapd_cli_cmd_set_channel_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid set_channel command: needs exactly one argument (channel)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_CHANNEL %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET_CHANNEL command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_set_opclass_tbl_idx_extn(struct wpa_ctrl *ctrl, int argc,
					      char *argv[])
{
	char cmd[64];
	char *end;
	long idx;
	int res;

	if (argc != 1) {
		printf("Usage: set_opclass_tbl <0-6>\n");
		return -1;
	}

	errno = 0;
	idx = strtol(argv[0], &end, 10);
	if (errno != 0 || end == argv[0] || *end != '\0' || idx < 0 ||
	    idx > 6) {
		printf("Invalid opclass table index '%s' (expected 0-6)\n",
		       argv[0]);
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_OPCLASS_TBL_IDX %ld", idx);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("set_opclass_tbl cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_get_opclass_tbl_idx_extn(struct wpa_ctrl *ctrl, int argc,
					      char *argv[])
{
	if (argc != 0) {
		printf("Invalid get_opclass_tbl command: no arguments expected\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "GET_OPCLASS_TBL_IDX");
}

int hostapd_cli_cmd_countryie_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[32];
	int ret;

	if (argc != 1) {
		printf("Invalid 'countryie' (0-disable or 1-enable) is needed\n");
		return -1;
	}

	if (os_strcmp(argv[0], "0") != 0 && os_strcmp(argv[0], "1") != 0) {
		printf("Invalid 'countryie' value '%s' - valid values are 0 or 1\n",
		       argv[0]);
		return -1;
	}

	ret = os_snprintf(cmd, sizeof(cmd), "COUNTRY_IE %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), ret))
		return -1;

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_get_countryie_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid 'get_countryie' command - no argument needed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "GET_COUNTRY_IE");
}

int hostapd_cli_acs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid ACS command: needs 1 argument atleast\n");
	}

	return hostapd_cli_cmd(ctrl, "ACS", 1, argc, argv);
}

int hostapd_cli_cmd_ignorecac_extn(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	/* With no argument, return current configured value. */
	return hostapd_cli_cmd(ctrl, "IGNORECAC", 0, argc, argv);
}

int hostapd_cli_cbs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid CBS command: needs at least 1 argument\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "CBS", 1, argc, argv);
}

#ifdef CONFIG_IEEE80211AC
int hostapd_cli_cmd_get_mu_cap_war_extn(struct wpa_ctrl *ctrl,
					     int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_MU_CAP_WAR");
}

int hostapd_cli_cmd_mu_cap_war_extn(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	char buf[32];
	int res;

	if (argc != 1) {
		printf("Usage: mu_cap_war <1/0>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "MU_CAP_WAR %s", argv[0]);

	if (os_snprintf_error(sizeof(buf), res)) {
		printf("mu_cap_war cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}
#endif /* CONFIG_IEEE80211AC */

int hostapd_cli_cmd_set_obss_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	char buf[64] = {'\0'};
	int res;

	if (argc != 1) {
		printf("Usage: set_obss_snr_threshold <snr>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_OBSS_SNR_THRESHOLD %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long SET_OBSS_SNR_THRESHOLD command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_obss_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_OBSS_SNR_THRESHOLD");
}

int hostapd_cli_cmd_set_obss_rx_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						   char *argv[])
{
	char buf[64] = {'\0'};
	int res;

	if (argc != 1) {
		printf("Usage: set_obss_rx_snr_threshold <snr>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_OBSS_RX_SNR_THRESHOLD %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long SET_OBSS_RX_SNR_THRESHOLD command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_obss_rx_snr_threshold_extn(struct wpa_ctrl *ctrl, int argc,
						   char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_OBSS_RX_SNR_THRESHOLD");
}

int hostapd_cli_cmd_dfs_no_wradar_extn(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Invalid dfs_no_wradar command: needs one argument (0|1)\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "DFS_NO_WRADAR %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long DFS_NO_WRADAR command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_dfs_no_wradar_extn(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	if (argc != 0) {
		printf("Invalid get_dfs_no_wradar command: no arguments expected\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "GET_DFS_NO_WRADAR");
}

int hostapd_cli_cmd_set_allow_scan_on_dfs_chan_extn(struct wpa_ctrl *ctrl, int argc,
						    char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Invalid allow_scan_on_dfs_chan command: needs one argument (0|1)\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "ALLOW_SCAN_ON_DFS_CHAN %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long ALLOW_SCAN_ON_DFS_CHAN command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_allow_scan_on_dfs_chan_extn(struct wpa_ctrl *ctrl, int argc,
						    char *argv[])
{
	if (argc != 0) {
		printf("Invalid get_allow_scan_on_dfs_chan command: no arguments expected\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "GET_ALLOW_SCAN_ON_DFS_CHAN");
}

int hostapd_cli_cmd_dcs_extn(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid dcs_enable command: need atleast 1 argument\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "DCS", 1, argc, argv);
}

int hostapd_cli_cmd_sync_iface_freq_extn(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return wpa_ctrl_command(ctrl, "SYNC_IFACE_FREQ");
}

int hostapd_cli_cmd_ecsa_opclass_extn(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: ecsa_opclass <opclass>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "ECSA_OPCLASS %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("ecsa_opclass cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_g_ecsa_opclass_extn(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_ECSA_OPCLASS");
}

static int hostapd_cli_send_dcs_param_values(struct wpa_ctrl *ctrl,
					     const char *base,
					     int argc, char *argv[])
{
	char buf[1024];
	int pos, i, r;

	pos = os_snprintf(buf, sizeof(buf), "%s", base);

	if (os_snprintf_error(sizeof(buf), pos))
		return -1;

	for (i = 0; i < argc; i++) {
		r = os_snprintf(buf + pos, sizeof(buf) - pos, "%s%s",
				    (pos > 0 ? " " : ""), argv[i]);
		if (os_snprintf_error(sizeof(buf), pos + r))
			return -1;
		pos += r;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_set_dcs_wlan_intr_params(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	if (argc < 2) {
		printf("Invalid dcs_params: need <key> <val> \n");
		return -1;
	}

	/* Allow odd argc so users can pass tokens like 'phyerr_penalty
	 * 10' etc.
	 * We'll send through as provided; ctrl side will validate.
	 */
	return hostapd_cli_send_dcs_param_values(ctrl, "DCS_PARAMS", argc,
						 argv);
}

int hostapd_cli_cmd_set_primary_chans(struct wpa_ctrl *ctrl,
				      int argc, char *argv[])
{
	char cmd[512];
	int res, i, pos;

	if (argc == 0) {
		/* No channels → clear the primary channel list */
		return wpa_ctrl_command(ctrl, "SET_PRIMARY_CHANS");
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_PRIMARY_CHANS");
	if (res < 0 || res >= (int)sizeof(cmd))
		return -1;
	pos = res;

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + pos, sizeof(cmd) - pos, " %s", argv[i]);
		if (res < 0 || res >= (int)(sizeof(cmd) - pos))
			return -1;
		pos += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_get_primary_chans(struct wpa_ctrl *ctrl,
				      int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_PRIMARY_CHANS");
}

int hostapd_cli_cmd_disable_opclass_chans(struct wpa_ctrl *ctrl,
					  int argc, char *argv[])
{
	char cmd[512];
	int i, pos, res;

	if (argc < 3) {
		printf("Usage: disable_opclass_chans <0|1> <opclass> <ch1> [<ch2> ...]\n");
		return -1;
	}

	if (os_strcmp(argv[0], "0") != 0 && os_strcmp(argv[0], "1") != 0) {
		printf("Invalid disable flag '%s' (expected 0 or 1)\n", argv[0]);
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "DISABLE_OPCLASS_CHANS %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;

	pos = res;
	for (i = 2; i < argc; i++) {
		res = os_snprintf(cmd + pos, sizeof(cmd) - pos, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - pos, res))
			return -1;
		pos += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_set_ht40intol(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_ht40intol <value>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "HT40INTOL %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_ht40intol cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_ht40intol(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_HT40INTOL");
}

int hostapd_cli_cmd_set_autorecovery_after_nol_vapdown(struct wpa_ctrl *ctrl,
						       int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid set_autorecovery_after_nol_vapdown command\n"
		       "usage: set_autorecovery_after_nol_vapdown <0|1>\n"
		       "  0: Disable VAP auto-recovery after NOL expiry\n"
		       "  1: Enable VAP auto-recovery after NOL expiry (default)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "SET_AUTORECOVERY_AFTER_NOL_VAPDOWN %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;

	return wpa_ctrl_command(ctrl, cmd);
}

int hostapd_cli_cmd_set_eht_config_ccfs0(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_eht_config_ccfs0 <value>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_EHT_CONFIG_CCFS0 %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_eht_config_ccfs0 cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_eht_config_ccfs0(struct wpa_ctrl *ctrl,
					 int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_EHT_CONFIG_CCFS0");
}

int hostapd_cli_cmd_set_tpe_common_psd(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_tpe_common_psd <value>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_TPE_COMMON_PSD %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_tpe_common_psd cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_tpe_common_psd(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_TPE_COMMON_PSD");
}

int hostapd_cli_cmd_set_tpe_tx_pwr_interp(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_tpe_tx_pwr_interp <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_TPE_TX_PWR_INTERP %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_tpe_tx_pwr_interp cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_tpe_tx_pwr_interp(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_TPE_TX_PWR_INTERP");
}

int hostapd_cli_cmd_set_tpe_punct_channel_tx_pwr(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_tpe_punct_channel_tx_pwr <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_TPE_PUNCT_CHANNEL_TX_PWR %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_tpe_punct_channel_tx_pwr cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_tpe_punct_channel_tx_pwr(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_TPE_PUNCT_CHANNEL_TX_PWR");
}

int hostapd_cli_cmd_set_pureg(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_pureg <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_PUREG %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_pureg cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_pureg(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_PUREG");
}

int hostapd_cli_cmd_set_puren(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_puren <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_PUREN %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_puren cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_puren(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_PUREN");
}

int hostapd_cli_cmd_set_disablecoexist(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_disablecoexist <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_DISABLECOEXIST %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_disablecoexist cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_disablecoexist(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_DISABLECOEXIST");
}

int hostapd_cli_cmd_set_pure11ac(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_pure11ac <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_PURE11AC %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_pure11ac cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_pure11ac(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_PURE11AC");
}

int hostapd_cli_cmd_set_pure11ax(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	int res;

	if (argc != 1) {
		printf("Usage: set_pure11ax <0|1>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_PURE11AX %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("set_pure11ax cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

int hostapd_cli_cmd_get_pure11ax(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_PURE11AX");
}

int hostapd_cli_cmd_get_mapc_peer_params_extn(struct wpa_ctrl *ctrl,
					      int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid 'get_mapc_peer_params' command - usage:\n"
		       "  get_mapc_peer_params <bssid>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "GET_MAPC_PEER_PARAMS %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command error (GET_MAPC_PEER_PARAMS argument too long)\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

int _wpa_ctrl_command_large(struct wpa_ctrl *ctrl, const char *cmd, int print)
{
	char *buf;
	size_t len;
	int ret;

	if (!ctrl_conn) {
		printf("Not connected to hostapd - command dropped.\n");
		return -1;
	}
	if (hostapd_cli_recovery_in_progress()) {
		printf("'%s' command dropped: ath12k recovery in progress.\n", cmd);
		return -1;
	}

	buf = os_malloc(MAX_REPLY_EXTN_BUF);
	if (!buf)
		return -1;
	len = MAX_REPLY_EXTN_BUF - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len, hostapd_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		os_free(buf);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		os_free(buf);
		return -1;
	}
	if (print) {
		buf[len] = '\0';
		printf("%s", buf);
	}
	os_free(buf);
	return 0;
}

