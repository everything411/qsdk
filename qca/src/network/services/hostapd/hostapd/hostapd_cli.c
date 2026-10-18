/*
 * hostapd - command line interface for hostapd daemon
 * Copyright (c) 2004-2022, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <glob.h>

#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/hostapd_cli_extn.h"
#endif /* CONFIG_QCN_EXTN */
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/edit.h"
#include "common/version.h"
#include "common/cli.h"

#ifndef CONFIG_NO_CTRL_IFACE

static const char *const hostapd_cli_version =
"hostapd_cli v" VERSION_STR "\n"
"Copyright (c) 2004-2024, Jouni Malinen <j@w1.fi> and contributors";

struct wpa_ctrl *ctrl_conn;
static int hostapd_cli_quit = 0;
static int hostapd_cli_attached = 0;

#ifndef CONFIG_CTRL_IFACE_DIR
#define CONFIG_CTRL_IFACE_DIR "/var/run/hostapd"
#endif /* CONFIG_CTRL_IFACE_DIR */
static const char *ctrl_iface_dir = CONFIG_CTRL_IFACE_DIR;
static const char *client_socket_dir = NULL;

static char *ctrl_ifname = NULL;
static const char *pid_file = NULL;
static const char *action_file = NULL;
static int ping_interval = 5;
static int interactive = 0;
static int event_handler_registered = 0;

static DEFINE_DL_LIST(stations); /* struct cli_txt_entry */

static void print_help(FILE *stream, const char *cmd);
static char ** list_cmd_list(void);
static void hostapd_cli_receive(int sock, void *eloop_ctx, void *sock_ctx);
static void update_stations(struct wpa_ctrl *ctrl);
static void cli_event(const char *str);


static void usage(void)
{
	fprintf(stderr, "%s\n", hostapd_cli_version);
	fprintf(stderr,
		"\n"
		"usage: hostapd_cli [-p<path>] [-i<ifname>] "
#ifdef CONFIG_IEEE80211BE
		"[-l<link_id>] "
#endif /* CONFIG_IEEE80211BE */
		"[-hvBr] "
		"[-a<path>] \\\n"
		"                   [-P<pid file>] [-G<ping interval>] [command..]\n"
		"\n"
		"Options:\n"
		"   -h           help (show this usage text)\n"
		"   -v           shown version information\n"
		"   -p<path>     path to find control sockets (default: "
		"/var/run/hostapd)\n"
		"   -s<dir_path> dir path to open client sockets (default: "
		CONFIG_CTRL_IFACE_DIR ")\n"
		"   -a<file>     run in daemon mode executing the action file "
		"based on events\n"
		"                from hostapd\n"
		"   -r           try to reconnect when client socket is "
		"disconnected.\n"
		"                This is useful only when used with -a.\n"
		"   -B           run a daemon in the background\n"
		"   -i<ifname>   Interface to listen on (default: first "
		"interface found in the\n"
		"                socket path)\n"
#ifdef CONFIG_IEEE80211BE
		"   -l<link_id>  Link ID of the interface in case of Multi-Link Operation\n"
#endif /* CONFIG_IEEE80211BE */
		"\n");
	print_help(stderr, NULL);
}


static void register_event_handler(struct wpa_ctrl *ctrl)
{
	if (!ctrl_conn)
		return;
	if (interactive) {
		event_handler_registered =
			!eloop_register_read_sock(wpa_ctrl_get_fd(ctrl),
						  hostapd_cli_receive,
						  NULL, NULL);
	}
}


static void unregister_event_handler(struct wpa_ctrl *ctrl)
{
	if (!ctrl_conn)
		return;
	if (interactive && event_handler_registered) {
		eloop_unregister_read_sock(wpa_ctrl_get_fd(ctrl));
		event_handler_registered = 0;
	}
}


static struct wpa_ctrl * hostapd_cli_open_connection(const char *ifname)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	char *cfile;
	int flen;
#endif /* !CONFIG_CTRL_IFACE_UDP */

	if (ifname == NULL)
		return NULL;

#ifdef CONFIG_CTRL_IFACE_UDP
	ctrl_conn = wpa_ctrl_open(ifname);
	return ctrl_conn;
#else /* CONFIG_CTRL_IFACE_UDP */
	flen = strlen(ctrl_iface_dir) + strlen(ifname) + 2;
	cfile = malloc(flen);
	if (cfile == NULL)
		return NULL;
	snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ifname);

	if (client_socket_dir && client_socket_dir[0] &&
	    access(client_socket_dir, F_OK) < 0) {
		perror(client_socket_dir);
		free(cfile);
		return NULL;
	}

	ctrl_conn = wpa_ctrl_open2(cfile, client_socket_dir);
	free(cfile);
	return ctrl_conn;
#endif /* CONFIG_CTRL_IFACE_UDP */
}


static void hostapd_cli_close_connection(void)
{
	if (ctrl_conn == NULL)
		return;

	unregister_event_handler(ctrl_conn);
	if (hostapd_cli_attached) {
		wpa_ctrl_detach(ctrl_conn);
		hostapd_cli_attached = 0;
	}
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
}


static int hostapd_cli_reconnect(const char *ifname)
{
	char *next_ctrl_ifname;

	hostapd_cli_close_connection();

	if (!ifname)
		return -1;

	next_ctrl_ifname = os_strdup(ifname);
	os_free(ctrl_ifname);
	ctrl_ifname = next_ctrl_ifname;
	if (!ctrl_ifname)
		return -1;

	ctrl_conn = hostapd_cli_open_connection(ctrl_ifname);
	if (!ctrl_conn)
		return -1;
	if (!interactive && !action_file)
		return 0;
	if (wpa_ctrl_attach(ctrl_conn) == 0) {
		hostapd_cli_attached = 1;
		register_event_handler(ctrl_conn);
		update_stations(ctrl_conn);
	} else {
		printf("Warning: Failed to attach to hostapd.\n");
	}
	return 0;
}


void hostapd_cli_msg_cb(char *msg, size_t len)
{
	cli_event(msg);
	printf("%s\n", msg);
}


int hostapd_cli_recovery_in_progress(void)
{
	glob_t g;
	size_t i;
	FILE *f;
	char val;
	int found = 0;

	if (glob("/sys/kernel/debug/ath12k/*/recovery_in_progress",
		 0, NULL, &g) != 0)
		return 0;

	for (i = 0; i < g.gl_pathc; i++) {
		f = fopen(g.gl_pathv[i], "r");
		if (!f)
			continue;
		if (fread(&val, 1, 1, f) == 1 && val == '1')
			found = 1;
		fclose(f);
		if (found)
			break;
	}

	globfree(&g);
	return found;
}


static int _wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, int print)
{
#ifdef CONFIG_QCN_EXTN
	char buf[16384];
#else
	char buf[4096];
#endif /* CONFIG_QCN_EXTN */
	size_t len;
	int ret;

	if (ctrl_conn == NULL) {
		printf("Not connected to hostapd - command dropped.\n");
		return -1;
	}
	if (hostapd_cli_recovery_in_progress()) {
		printf("'%s' command dropped: ath12k recovery in progress.\n",
		       cmd);
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len,
			       hostapd_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		return -1;
	}
	if (print) {
		buf[len] = '\0';
		printf("%s", buf);
	}
	return 0;
}

#ifndef CONFIG_QCN_EXTN
static inline
#endif
int wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd)
{
	return _wpa_ctrl_command(ctrl, cmd, 1);
}

#ifndef CONFIG_QCN_EXTN
static
#endif
int hostapd_cli_cmd(struct wpa_ctrl *ctrl, const char *cmd,
		    int min_args, int argc, char *argv[])
{
#ifdef CONFIG_QCN_EXTN
	char buf[16384];
#else
	char buf[4096];
#endif /* CONFIG_QCN_EXTN */

	if (argc < min_args) {
		printf("Invalid %s command - at least %d argument%s required.\n",
		       cmd, min_args, min_args > 1 ? "s are" : " is");
		return -1;
	}
	if (write_cmd(buf, sizeof(buf), cmd, argc, argv) < 0)
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_ping(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PING");
}


static int hostapd_cli_cmd_relog(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOG");
}


static int hostapd_cli_cmd_close_log(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return wpa_ctrl_command(ctrl, "CLOSE_LOG");
}


static int hostapd_cli_cmd_status(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc > 0 && os_strcmp(argv[0], "driver") == 0)
		return wpa_ctrl_command(ctrl, "STATUS-DRIVER");
	return wpa_ctrl_command(ctrl, "STATUS");
}


static int hostapd_cli_cmd_mib(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc > 0) {
		char buf[100];
		os_snprintf(buf, sizeof(buf), "MIB %s", argv[0]);
		return wpa_ctrl_command(ctrl, buf);
	}
	return wpa_ctrl_command(ctrl, "MIB");
}


static int hostapd_cli_exec(const char *program, const char *arg1,
			    const char *arg2)
{
	char *arg;
	size_t len;
	int res;

	len = os_strlen(arg1) + os_strlen(arg2) + 2;
	arg = os_malloc(len);
	if (arg == NULL)
		return -1;
	os_snprintf(arg, len, "%s %s", arg1, arg2);
	res = os_exec(program, arg, 1);
	os_free(arg);

	return res;
}


static void hostapd_cli_action_process(char *msg, size_t len)
{
	const char *pos;

	pos = msg;
	if (*pos == '<') {
		pos = os_strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}

	hostapd_cli_exec(action_file, ctrl_ifname, pos);
}


static void hostapd_cli_action_cb(char *msg, size_t len)
{
	hostapd_cli_action_process(msg, len);
}


static int hostapd_cli_cmd_sta(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'sta' command - at least one argument, STA "
		       "address, is required.\n");
		return -1;
	}
	if (argc > 1)
		snprintf(buf, sizeof(buf), "STA %s %s", argv[0], argv[1]);
	else
		snprintf(buf, sizeof(buf), "STA %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static char ** hostapd_complete_stations(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = cli_txt_list_array(&stations);
		break;
	}

	return res;
}


static int hostapd_cli_cmd_new_sta(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char buf[64];
	if (argc != 1) {
		printf("Invalid 'new_sta' command - exactly one argument, STA "
		       "address, is required.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), "NEW_STA %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_deauthenticate(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'deauthenticate' command - exactly one "
		       "argument, STA address, is required.\n");
		return -1;
	}
	if (argc > 1)
		os_snprintf(buf, sizeof(buf), "DEAUTHENTICATE %s %s",
			    argv[0], argv[1]);
	else
		os_snprintf(buf, sizeof(buf), "DEAUTHENTICATE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_disassociate(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'disassociate' command - exactly one "
		       "argument, STA address, is required.\n");
		return -1;
	}
	if (argc > 1)
		os_snprintf(buf, sizeof(buf), "DISASSOCIATE %s %s",
			    argv[0], argv[1]);
	else
		os_snprintf(buf, sizeof(buf), "DISASSOCIATE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_signature(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	char buf[64];

	if (argc != 1) {
		printf("Invalid 'signature' command - exactly one argument, STA address, is required.\n");
		return -1;
	}
	os_snprintf(buf, sizeof(buf), "SIGNATURE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_sa_query(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char buf[64];
	if (argc != 1) {
		printf("Invalid 'sa_query' command - exactly one argument, "
		       "STA address, is required.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), "SA_QUERY %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_wps_pin(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char buf[256];
	if (argc < 2) {
		printf("Invalid 'wps_pin' command - at least two arguments, "
		       "UUID and PIN, are required.\n");
		return -1;
	}
	if (argc > 3)
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s %s %s",
			 argv[0], argv[1], argv[2], argv[3]);
	else if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s %s",
			 argv[0], argv[1], argv[2]);
	else
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s", argv[0], argv[1]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_wps_check_pin(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1 && argc != 2) {
		printf("Invalid WPS_CHECK_PIN command: needs one argument:\n"
		       "- PIN to be verified\n");
		return -1;
	}

	if (argc == 2)
		res = os_snprintf(cmd, sizeof(cmd), "WPS_CHECK_PIN %s %s",
				  argv[0], argv[1]);
	else
		res = os_snprintf(cmd, sizeof(cmd), "WPS_CHECK_PIN %s",
				  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_CHECK_PIN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_wps_pbc(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_PBC");
}


static int hostapd_cli_cmd_wps_cancel(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_CANCEL");
}


#ifdef CONFIG_WPS_NFC
static int hostapd_cli_cmd_wps_nfc_tag_read(struct wpa_ctrl *ctrl, int argc,
					    char *argv[])
{
	int ret;
	char *buf;
	size_t buflen;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_tag_read' command - one argument "
		       "is required.\n");
		return -1;
	}

	buflen = 18 + os_strlen(argv[0]);
	buf = os_malloc(buflen);
	if (buf == NULL)
		return -1;
	os_snprintf(buf, buflen, "WPS_NFC_TAG_READ %s", argv[0]);

	ret = wpa_ctrl_command(ctrl, buf);
	os_free(buf);

	return ret;
}


static int hostapd_cli_cmd_wps_nfc_config_token(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_config_token' command - one argument "
		       "is required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "WPS_NFC_CONFIG_TOKEN %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_NFC_CONFIG_TOKEN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_wps_nfc_token(struct wpa_ctrl *ctrl,
					 int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_token' command - one argument is "
		       "required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "WPS_NFC_TOKEN %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_NFC_TOKEN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_nfc_get_handover_sel(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 2) {
		printf("Invalid 'nfc_get_handover_sel' command - two arguments "
		       "are required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "NFC_GET_HANDOVER_SEL %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long NFC_GET_HANDOVER_SEL command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}

#endif /* CONFIG_WPS_NFC */


static int hostapd_cli_cmd_wps_ap_pin(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'wps_ap_pin' command - at least one argument "
		       "is required.\n");
		return -1;
	}
	if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s %s %s",
			 argv[0], argv[1], argv[2]);
	else if (argc > 1)
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s %s",
			 argv[0], argv[1]);
	else
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_wps_get_status(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_GET_STATUS");
}


static int hostapd_cli_cmd_wps_config(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[256];
	char ssid_hex[2 * SSID_MAX_LEN + 1];
	char key_hex[2 * 64 + 1];
	int i;

	if (argc < 1) {
		printf("Invalid 'wps_config' command - at least two arguments "
		       "are required.\n");
		return -1;
	}

	ssid_hex[0] = '\0';
	for (i = 0; i < SSID_MAX_LEN; i++) {
		if (argv[0][i] == '\0')
			break;
		os_snprintf(&ssid_hex[i * 2], 3, "%02x", argv[0][i]);
	}

	key_hex[0] = '\0';
	if (argc > 3) {
		for (i = 0; i < 64; i++) {
			if (argv[3][i] == '\0')
				break;
			os_snprintf(&key_hex[i * 2], 3, "%02x",
				    argv[3][i]);
		}
	}

	if (argc > 3)
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s %s %s",
			 ssid_hex, argv[1], argv[2], key_hex);
	else if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s %s",
			 ssid_hex, argv[1], argv[2]);
	else
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s",
			 ssid_hex, argv[1]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_link_remove(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	char buf[256];

	if (argc < 1) {
		printf("Invalid 'link_removal' command  - atleast 2 args required\n");
		return -1;
	}

	snprintf(buf, sizeof(buf), "LINK_REMOVE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}

#ifdef CONFIG_IEEE80211BE
static int hostapd_cli_cmd_ml_max_rec_links(struct wpa_ctrl *ctrl, int argc,
					    char *argv[])
{
	char buf[256];

	if (argc < 1) {
		printf("Invalid 'ml_max_rec_links' command  - atleast 2 args required\n");
		return -1;
	}

	snprintf(buf, sizeof(buf), "ML_MAX_REC_LINKS %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}
#endif /* CONFIG_IEEE80211BE */

static int hostapd_cli_cmd_disassoc_imminent(struct wpa_ctrl *ctrl, int argc,
					     char *argv[])
{
	char buf[300];
	int res;

	if (argc < 2) {
		printf("Invalid 'disassoc_imminent' command - two arguments "
		       "(STA addr and Disassociation Timer) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "DISASSOC_IMMINENT %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_ess_disassoc(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char buf[300];
	int res;

	if (argc < 3) {
		printf("Invalid 'ess_disassoc' command - three arguments (STA "
		       "addr, disassoc timer, and URL) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "ESS_DISASSOC %s %s %s",
			  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_bss_tm_req(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[2000], *tmp;
	int res, i, total;

	if (argc < 1) {
		printf("Invalid 'bss_tm_req' command - at least one argument (STA addr) is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "BSS_TM_REQ %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	total = res;
	for (i = 1; i < argc; i++) {
		tmp = &buf[total];
		res = os_snprintf(tmp, sizeof(buf) - total, " %s", argv[i]);
		if (os_snprintf_error(sizeof(buf) - total, res))
			return -1;
		total += res;
	}
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_get_config(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_CONFIG");
}

static int hostapd_cli_cmd_dump_tk(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char buf[300];
	int res;

	if (argc != 2) {
		printf("Invalid DUMP_TK command: needs two arguments "
		       "(STA address and key index)\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "DUMP_TK %s %s", argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Too long DUMP_TK command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}


static int wpa_ctrl_command_sta(struct wpa_ctrl *ctrl, const char *cmd,
				char *addr, size_t addr_len, int print)
{
#ifdef CONFIG_QCN_EXTN
	char buf[16384];
#else
	char buf[4096];
#endif /* CONFIG_QCN_EXTN */
	char *pos;
	size_t len;
	int ret;

	if (ctrl_conn == NULL) {
		printf("Not connected to hostapd - command dropped.\n");
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len,
			       hostapd_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		return -1;
	}

	buf[len] = '\0';
	if (memcmp(buf, "FAIL", 4) == 0 || memcmp(buf, "UNKNOWN COMMAND", 15) == 0)
		return -1;
	if (print)
		printf("%s", buf);

	pos = buf;
	while (*pos != '\0' && *pos != '\n')
		pos++;
	*pos = '\0';
	os_strlcpy(addr, buf, addr_len);
	return 0;
}


static int hostapd_cli_cmd_all_sta(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char addr[32], cmd[64];

	if (wpa_ctrl_command_sta(ctrl, "STA-FIRST", addr, sizeof(addr), 1))
		return 0;
	do {
		snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);
	} while (wpa_ctrl_command_sta(ctrl, cmd, addr, sizeof(addr), 1) == 0);

	return -1;
}


static int hostapd_cli_cmd_list_sta(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char addr[32], cmd[64];

	if (wpa_ctrl_command_sta(ctrl, "STA-FIRST", addr, sizeof(addr), 0))
		return 0;
	do {
		if (os_strcmp(addr, "") != 0)
			printf("%s\n", addr);
		os_snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);
	} while (wpa_ctrl_command_sta(ctrl, cmd, addr, sizeof(addr), 0) == 0);

	return 0;
}


static int hostapd_cli_cmd_help(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	print_help(stdout, argc > 0 ? argv[0] : NULL);
	return 0;
}


static char ** hostapd_cli_complete_help(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = list_cmd_list();
		break;
	}

	return res;
}


static int hostapd_cli_cmd_license(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	printf("%s\n\n%s\n", hostapd_cli_version, cli_full_license);
	return 0;
}


static int hostapd_cli_cmd_set_qos_map_set(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	char buf[200];
	int res;

	if (argc != 1) {
		printf("Invalid 'set_qos_map_set' command - "
		       "one argument (comma delimited QoS map set) "
		       "is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_QOS_MAP_SET %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


#ifdef CONFIG_INTERWORKING
static int hostapd_cli_cmd_set_bss_priority(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 1) {
		printf("Invalid 'bss_priority' command - "
				"one argument <0..3> is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_BSS_PRIORITY %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_set_bss_priority_status(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 1) {
		printf("Invalid 'set_bss_priority_status' command - "
			"one argument (0-disable 1-enable) is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf),
			"SET_BSS_PRIORITY_STATUS %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_get_bss_priority(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 0) {
		printf("Invalid 'get_bss_priority' command - "
				"no argument needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "GET_BSS_PRIORITY");

	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	return wpa_ctrl_command(ctrl, buf);
}

static int hostapd_cli_cmd_get_bss_priority_status(struct wpa_ctrl *ctrl,
		int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 0) {
		printf("Invalid 'get_bss_priority_status' command - "
				"no argument needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "GET_BSS_PRIORITY_STATUS");

	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	return wpa_ctrl_command(ctrl, buf);
}

#endif /* CONFIG_INTERWORKING */


static int hostapd_cli_cmd_send_qos_map_conf(struct wpa_ctrl *ctrl,
		int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 1) {
		printf("Invalid 'send_qos_map_conf' command - "
		       "one argument (STA addr) is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SEND_QOS_MAP_CONF %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}

static int hostapd_cli_cmd_set_dscp_policy(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	char cmd[512];
	int res;
	int i;
	int total = 0;

	if (argc < 2) {
		printf("Invalid set_dscp_policy command\n"
		       "usage: set_dscp_policy <sta_addr> policy_id=<id> request_type=<Add/Remove> dscp=<val>\n"
		       "[classifier_mask=] [ip_version=] [dst_ip=] [domain_name=] [reset=]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_DSCP_POLICY");
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;

	total = res;

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + total, sizeof(cmd) - total, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Too long SET_DSCP_POLICY command.\n");
			return -1;
		}
		total += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_send_unsolicited_dscp_req(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	char cmd[512];
	int res;
	int i;

	if (argc < 3) {
		printf("Invalid 'send_unsolicited_dscp_req' command - "
		       "usage: sta_addr=<addr> [reset=] [policy_id_list=]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SEND_UNSOLICITED_DSCP_REQ");
	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - strlen(cmd), res))
			return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_hs20_wnm_notif(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char buf[300];
	int res;

	if (argc < 2) {
		printf("Invalid 'hs20_wnm_notif' command - two arguments (STA "
		       "addr and URL) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "HS20_WNM_NOTIF %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_hs20_deauth_req(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	char buf[300];
	int res;

	if (argc < 3) {
		printf("Invalid 'hs20_deauth_req' command - at least three arguments (STA addr, Code, Re-auth Delay) are needed\n");
		return -1;
	}

	if (argc > 3)
		res = os_snprintf(buf, sizeof(buf),
				  "HS20_DEAUTH_REQ %s %s %s %s",
				  argv[0], argv[1], argv[2], argv[3]);
	else
		res = os_snprintf(buf, sizeof(buf),
				  "HS20_DEAUTH_REQ %s %s %s",
				  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_quit(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	hostapd_cli_quit = 1;
	if (interactive)
		eloop_terminate();
	return 0;
}


static int hostapd_cli_cmd_level(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	if (argc != 1) {
		printf("Invalid LEVEL command: needs one argument (debug "
		       "level)\n");
		return 0;
	}
	snprintf(cmd, sizeof(cmd), "LEVEL %s", argv[0]);
	return wpa_ctrl_command(ctrl, cmd);
}


static void update_stations(struct wpa_ctrl *ctrl)
{
	char addr[32], cmd[64];

	if (!ctrl || !interactive)
		return;

	cli_txt_list_flush(&stations);

	if (wpa_ctrl_command_sta(ctrl, "STA-FIRST", addr, sizeof(addr), 0))
		return;
	do {
		if (os_strcmp(addr, "") != 0)
			cli_txt_list_add(&stations, addr);
		os_snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);
	} while (wpa_ctrl_command_sta(ctrl, cmd, addr, sizeof(addr), 0) == 0);
}


static void hostapd_cli_get_interfaces(struct wpa_ctrl *ctrl,
				       struct dl_list *interfaces)
{
	struct dirent *dent;
	DIR *dir;

	if (!ctrl || !interfaces)
		return;
	dir = opendir(ctrl_iface_dir);
	if (dir == NULL)
		return;

	while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		cli_txt_list_add(interfaces, dent->d_name);
	}
	closedir(dir);
}


static void hostapd_cli_list_interfaces(struct wpa_ctrl *ctrl)
{
	struct dirent *dent;
	DIR *dir;

	dir = opendir(ctrl_iface_dir);
	if (dir == NULL) {
		printf("Control interface directory '%s' could not be "
		       "opened.\n", ctrl_iface_dir);
		return;
	}

	printf("Available interfaces:\n");
	while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		printf("%s\n", dent->d_name);
	}
	closedir(dir);
}


static int hostapd_cli_cmd_interface(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	if (argc < 1) {
		hostapd_cli_list_interfaces(ctrl);
		return 0;
	}
	if (hostapd_cli_reconnect(argv[0]) != 0) {
		printf("Could not connect to interface '%s' - re-trying\n",
			ctrl_ifname);
	}
	return 0;
}


static char ** hostapd_complete_interface(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	DEFINE_DL_LIST(interfaces);

	switch (arg) {
	case 1:
		hostapd_cli_get_interfaces(ctrl_conn, &interfaces);
		res = cli_txt_list_array(&interfaces);
		cli_txt_list_flush(&interfaces);
		break;
	}

	return res;
}


static int hostapd_cli_cmd_set(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[2048];
	int res;

	if (argc != 2) {
		printf("Invalid SET command: needs two arguments (variable "
		       "name and value)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET %s %s", argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static char ** hostapd_complete_set(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	const char *fields[] = {
#ifdef CONFIG_WPS_TESTING
		"wps_version_number", "wps_testing_stub_cred",
		"wps_corrupt_pkhash",
#endif /* CONFIG_WPS_TESTING */
#ifdef CONFIG_INTERWORKING
		"gas_frag_limit",
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_TESTING_OPTIONS
		"ext_mgmt_frame_handling", "ext_eapol_frame_io",
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_MBO
		"mbo_assoc_disallow", "mbo_trans_reason", "mbo_assoc_retry",
#endif /* CONFIG_MBO */
		"deny_mac_file", "accept_mac_file",
		"post_nol_freq", "post_nol_width", "post_nol_bgcac_en",
		"next_radar_freq", "next_radar_width",
#ifdef CONFIG_QCN_EXTN
		HOSTAPD_CLI_CMD_FIELDS_EXTN
#endif /* CONFIG_QCN_EXTN */
	};
	int i, num_fields = ARRAY_SIZE(fields);

	if (arg == 1) {
		char **res;

		res = os_calloc(num_fields + 1, sizeof(char *));
		if (!res)
			return NULL;
		for (i = 0; i < num_fields; i++) {
			res[i] = os_strdup(fields[i]);
			if (!res[i])
				return res;
		}
		return res;
	}
	return NULL;
}


static int hostapd_cli_cmd_get(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid GET command: needs one argument (variable "
		       "name)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "GET %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long GET command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_puncture_sources(struct wpa_ctrl *ctrl, int argc,
					    char *argv[])
{
	if (argc != 0) {
		printf("Invalid puncture_sources command: no arguments expected\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "GET_PUNCTURE_SOURCES");
}

static char ** hostapd_complete_get(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	const char *fields[] = {
		"version", "tls_library",
#ifdef CONFIG_QCN_EXTN
		HOSTAPD_CLI_CMD_FIELDS_EXTN
#endif /* CONFIG_QCN_EXTN */
	};
	int i, num_fields = ARRAY_SIZE(fields);

	if (arg == 1) {
		char **res;

		res = os_calloc(num_fields + 1, sizeof(char *));
		if (!res)
			return NULL;
		for (i = 0; i < num_fields; i++) {
			res[i] = os_strdup(fields[i]);
			if (!res[i])
				return res;
		}
		return res;
	}
	return NULL;
}


#ifdef CONFIG_FST
static int hostapd_cli_cmd_fst(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;
	int i;
	int total;

	if (argc <= 0) {
		printf("FST command: parameters are required.\n");
		return -1;
	}

	total = os_snprintf(cmd, sizeof(cmd), "FST-MANAGER");

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + total, sizeof(cmd) - total, " %s",
				  argv[i]);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Too long fst command.\n");
			return -1;
		}
		total += res;
	}
	return wpa_ctrl_command(ctrl, cmd);
}
#endif /* CONFIG_FST */


#ifdef CONFIG_IEEE80211AX
static int hostapd_cli_cmd_color_change(struct wpa_ctrl *ctrl,
					int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "COLOR_CHANGE", 1, argc, argv);
}

static int hostapd_cli_cmd_color_collision_ap_period(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "COLOR_COLLISION_AP_PERIOD", 1, argc, argv);
}

static int hostapd_cli_cmd_color_cca_count(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "COLOR_CCA_COUNT", 1, argc, argv);
}
#endif /* CONFIG_IEEE80211AX */


static int hostapd_cli_cmd_set_pwr_mode(struct wpa_ctrl *ctrl,
					int argc, char *argv[])
{
	char cmd[256];
	int res;
	int ret;

	if (argc < 1) {
		printf("Invalid power mode command: no argument given\n"
		       "usage: <pwr_mode-0/1/2>\n"
		       "0 - LPI; 1 - SP; 2 - VLP\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_6GHZ_PWR_MODE %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET_PWR_MODE command\n");
		return -1;
	}

	ret = wpa_ctrl_command(ctrl, cmd);
	return ret;
}


static int hostapd_cli_cmd_chan_switch(struct wpa_ctrl *ctrl,
				       int argc, char *argv[])
{
	char cmd[256];
	int res;
	int i;
	char *tmp;
	int total;

	if (argc < 2) {
		printf("Invalid chan_switch command: needs at least two "
		       "arguments (count and freq)\n"
		       "usage: <cs_count> <freq> [sec_channel_offset=] "
		       "[center_freq1=] [center_freq2=] [bandwidth=] "
		       "[bandwidth_device =] [center_freq_device=] "
		       "[blocktx] [ht|vht|he|eht]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "CHAN_SWITCH %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long CHAN_SWITCH command.\n");
		return -1;
	}

	total = res;
	for (i = 2; i < argc; i++) {
		tmp = cmd + total;
		res = os_snprintf(tmp, sizeof(cmd) - total, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Too long CHAN_SWITCH command.\n");
			return -1;
		}
		total += res;
	}
	return wpa_ctrl_command(ctrl, cmd);
}

/**
 * hostapd_cli_cmd_set_channel_usage_element - CLI Handler to construct and
 * send the SET_CHANNEL_USAGE_ELEMENT command to the hostapd interface
 * @ctrl: Control Interface Handle
 * @argc: Number of arguments provided
 * @argv: List of input arguments
 *
 * Returns: 0 on success, -1 on failure
 */

static int hostapd_cli_cmd_set_channel_usage_element(struct wpa_ctrl *ctrl,
						      int argc, char *argv[])
{
	char cmd[1024]; /* Buffer to hold final command string */
	int res;
	int i, j;
	int total;
	char *tmp;
	int num_channel_usage_elements;
	int mode_count, mode, num_entry, op_class, chan;

	/* At least 1 argument must exist: number of channel usage elements */
	if (argc < 1) {
		printf("Invalid set_channel_usage_element command: number of channel "
		       "usage elements is a required field."
		       "usage: <0-6> [mode <mode> num_entry <1-10> "
		       "<op0> <channel0> [<op1> <channel1> ...]] [mode <mode> "
		       "num_entry <1-10> ...] (up to 6 modes allowed)\n");
		return -1;
	}

	num_channel_usage_elements = atoi(argv[0]);
	if (num_channel_usage_elements < 0 || num_channel_usage_elements > 6) {
		printf("Error: num_channel_usage_elements needs to be in range [0-6]\n");
		return -1;
	}

	/* Initialize command string with base command and number of IEs */
	res = os_snprintf(cmd, sizeof(cmd), "SET_CHANNEL_USAGE_ELEMENT %d",
			  num_channel_usage_elements);

	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Error: command too long\n");
		return -1;
	}
	total = res;

	/* If there are 0 channel usage elements, command is already complete */
	if (num_channel_usage_elements == 0)
		return wpa_ctrl_command(ctrl, cmd);

	/* Parsing the mode blocks */
	i = 1;
	mode_count = 0;
	while (i < argc && mode_count < num_channel_usage_elements) {
		if (i + 3 >= argc || os_strcmp(argv[i], "mode") != 0 ||
		    os_strcmp(argv[i + 2], "num_entry") != 0) {
			printf("Error: Expected format 'mode <mode> num_entry "
			       "<num_entry> at mode block %d\n", mode_count+1);
			return -1;
		}
		mode = atoi(argv[i+1]);
		if (mode < 0 || mode > 5) {
			printf("Error: Mode value must be in range [0-5]\n");
			return -1;
		}
		num_entry = atoi(argv[i+3]);

		if (num_entry <= 0 || num_entry > 10) {
			printf("Error: num_entry value must be in range [1-10]\n");
			return -1;
		}

		tmp = cmd + total;
		res = os_snprintf(tmp, sizeof(cmd) - total,
			          " mode %d num_entry %d", mode, num_entry);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Error: command too long while adding mode block\n");
			return -1;
		}

		total += res;

		/* Move past "mode", mode_ID, "num_entry", num_entry_val */
		i += 4;

		/* Append channel entries (op_class and channel fields) */
		for (j = 0; j < num_entry; j++) {

			if (i + 1 >= argc) {
				printf("Error: Missing op_class/channel field entry "
				       "for entry %d for mode %d\n", j+1, mode);
				return -1;
			}

			op_class = atoi(argv[i]);
			if (op_class < 0 || op_class > 255) {
				printf("Error: op_class needs to be in range [0-255]\n");
				return -1;
			}
			chan = atoi(argv[i + 1]);
			if (chan < 0 || chan > 255) {
				printf("Error: chan needs to be in range [0-255]\n");
				return -1;
			}

			tmp = cmd + total;
			res = os_snprintf(tmp, sizeof(cmd) - total, " %d %d",
					  op_class, chan);
			if (os_snprintf_error(sizeof(cmd) - total, res)) {
				printf("Error: command too long while adding channel entry\n");
				return -1;
			}
			total += res;
			if (total >= sizeof(cmd) - 1) {
				printf("Error: Command buffer full\n");
				return -1;
			}

			/* Move past op_class and channel fields */
			i += 2;
		}
		mode_count++;
	}

	if (mode_count != num_channel_usage_elements) {
		printf("Error: expected %d mode blocks but only parsed %d\n",
			num_channel_usage_elements, mode_count);
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_notify_cw_change(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "NOTIFY_CW_CHANGE", 1, argc, argv);
}


static int hostapd_cli_cmd_set_bw(struct wpa_ctrl *ctrl,
				  int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "SET_BW", 0, argc, argv);
}


static int hostapd_cli_cmd_enable(struct wpa_ctrl *ctrl, int argc,
				  char *argv[])
{
	return wpa_ctrl_command(ctrl, "ENABLE");
}


static int hostapd_cli_cmd_switch_to_rcac(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	return wpa_ctrl_command(ctrl, "switch_to_rcac");
}


static int hostapd_cli_cmd_bgcac_start(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return wpa_ctrl_command(ctrl, "bgcac_start");
}


static int hostapd_cli_cmd_reload(struct wpa_ctrl *ctrl, int argc,
				  char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD");
}


static int hostapd_cli_cmd_reload_bss(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD_BSS");
}

static int hostapd_cli_cmd_reload_config_bss(struct wpa_ctrl *ctrl,
					     int argc, char *argv[])
{
        char cmd[256];
	int res;

	if (argc < 1) {
		printf("Missing BSS Config file\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "RELOAD_CONFIG_BSS %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long RELOAD_CONFIG_BSS command.\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_reload_config(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD_CONFIG");
}


static int hostapd_cli_cmd_disable(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "DISABLE");
}


static int hostapd_cli_cmd_disable_bss(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];
	int res;
	long tbtt;
	char *end;

	if (argc == 0)
		return wpa_ctrl_command(ctrl, "DISABLE_BSS");

	/* Single argument: tbtt number only */
	if (argc == 1) {
		errno = 0;
		end = NULL;
		tbtt = strtol(argv[0], &end, 10);
		if (!argv[0][0] || !end || *end != '\0' || tbtt < 0 ||
		    tbtt > INT_MAX || errno) {
			printf("Invalid TBTT value '%s'. Must be a non-negative integer.\n",
			       argv[0]);
			return -1;
		}

		res = os_snprintf(cmd, sizeof(cmd), "DISABLE_BSS %ld", tbtt);
		if (os_snprintf_error(sizeof(cmd), res)) {
			printf("Too long DISABLE_BSS command.\n");
			return -1;
		}
		return wpa_ctrl_command(ctrl, cmd);
	}

	printf("Invalid DISABLE_BSS usage. Expect: disable_bss [tbtt]\n");
	return -1;
}


static int hostapd_cli_cmd_enable_bss(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "ENABLE_BSS");
}


static int hostapd_cli_cmd_enable_mld(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "ENABLE_MLD");
}


static int hostapd_cli_cmd_disable_mld(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return wpa_ctrl_command(ctrl, "DISABLE_MLD");
}


static int hostapd_cli_cmd_stop_mld(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	return wpa_ctrl_command(ctrl, "STOP_MLD");
}


static int hostapd_cli_cmd_update_beacon(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return wpa_ctrl_command(ctrl, "UPDATE_BEACON");
}

#ifdef CONFIG_IEEE80211BN
/*
 * hostapd_cli_cmd_update_uhr_features - Send UPDATE_UHR_FEATURES command
 *
 * Usage:
 *   hostapd_cli -i <intf> [-l <link_id>] update_uhr_features \
 *       [NPCA enable=<0|1>
 *             (enable=1: primary_chan=<chan|freq_mhz> min_dur=<0-15>
 *                        switch_delay=<0-63> switch_back=<0-63>
 *                        [init_qsrc=<0-3>] [moplen=<0|1>] [bitmap=<0xHHHH>])
 *             (enable=0: no additional parameters allowed)]
 *
 * When enable=1, primary_chan, min_dur, switch_delay, and switch_back are
 * mandatory.  When enable=0, no additional parameters are allowed.
 */
static int hostapd_cli_cmd_update_uhr_features(struct wpa_ctrl *ctrl,
					       int argc, char *argv[])
{
	char cmd[512];
	int res, i;
	size_t used;

	if (argc < 1) {
		printf("Usage: update_uhr_features "
		       "[NPCA enable=<0|1>\n"
		       "  enable=1: primary_chan=<chan|freq_mhz> min_dur=<0-15>\n"
		       "           switch_delay=<0-63> switch_back=<0-63>\n"
		       "           [init_qsrc=<0-3>] [moplen=<0|1>] [bitmap=<0xHHHH>]\n"
		       "  enable=0: no additional parameters allowed\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "UPDATE_UHR_FEATURES");
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;
	used = res;

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + used, sizeof(cmd) - used, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - used, res)) {
			printf("Too long UPDATE_UHR_FEATURES command.\n");
			return -1;
		}
		used += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}
#endif /* CONFIG_IEEE80211BN */

static int hostapd_cli_cmd_add_tpe(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res, i;
	size_t used;

	if (argc < 4) {
		printf("Usage: add_tpe <tx_pwr_intrpt> <tx_pwr_cnt> <tx_pwr_cat> <tx_pwr …>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "ADD_TPE %s %s %s",
			  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;

	for (i = 3; i < argc; i++) {
		used = os_strlen(cmd);

		if (sizeof(cmd) - used <= os_strlen(argv[i]) + 2) {
			printf("Error: Too many arguments, command buffer full\n");
			return -1;
		}

		res = os_snprintf(cmd + used, sizeof(cmd) - used, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - used, res))
			return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_del_tpe(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc < 2) {
		printf("Usage: del_tpe <tx_pwr_intrpt> <tx_pwr_cat>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "DEL_TPE %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_stop_ap(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "STOP_AP");
}


static int hostapd_cli_cmd_vendor(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc < 2 || argc > 4) {
		printf("Invalid vendor command\n"
		       "usage: <vendor id> <command id> [<hex formatted command argument>] [nested=<0|1>]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "VENDOR %s %s %s%s%s", argv[0],
			  argv[1], argc >= 3 ? argv[2] : "",
			  argc == 4 ? " " : "", argc == 4 ? argv[3] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long VENDOR command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_erp_flush(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return wpa_ctrl_command(ctrl, "ERP_FLUSH");
}


static int hostapd_cli_cmd_log_level(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	char cmd[256];
	int res;

	res = os_snprintf(cmd, sizeof(cmd), "LOG_LEVEL%s%s%s%s",
			  argc >= 1 ? " " : "",
			  argc >= 1 ? argv[0] : "",
			  argc == 2 ? " " : "",
			  argc == 2 ? argv[1] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long option\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_log_peer(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char cmd[64];
	int res;

	if (argc < 1) {
		printf("Usage: log_peer <addr>|clear\n");
		return -1;
	}
	res = os_snprintf(cmd, sizeof(cmd), "LOG_PEER %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_raw(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc == 0)
		return -1;
	return hostapd_cli_cmd(ctrl, argv[0], 0, argc - 1, &argv[1]);
}


static int hostapd_cli_cmd_pmksa(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PMKSA");
}


static int hostapd_cli_cmd_pmksa_flush(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return wpa_ctrl_command(ctrl, "PMKSA_FLUSH");
}


static int hostapd_cli_cmd_set_neighbor(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char cmd[2048];
	int res;

	if (argc < 1 || argc > 6) {
		printf("Invalid set_neighbor command: needs 1-6 arguments\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_NEIGHBOR %s %s %s %s %s %s",
			  argv[0], argc >= 2 ? argv[1] : "",
			  argc >= 3 ? argv[2] : "", argc >= 4 ? argv[3] : "",
			  argc >= 5 ? argv[4] : "", argc == 6 ? argv[5] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET_NEIGHBOR command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_show_neighbor(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return wpa_ctrl_command(ctrl, "SHOW_NEIGHBOR");
}


static int hostapd_cli_cmd_remove_neighbor(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	return hostapd_cli_cmd(ctrl, "REMOVE_NEIGHBOR", 1, argc, argv);
}

static int hostapd_cli_cmd_send_neighbor(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	return hostapd_cli_cmd(ctrl, "SEND_NEIGHBOR", 2, argc, argv);
}

static int hostapd_cli_cmd_req_lci(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid req_lci command - requires destination address\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "REQ_LCI %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long REQ_LCI command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_req_range(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	if (argc < 4) {
		printf("Invalid req_range command: needs at least 4 arguments - dest address, randomization interval, min AP count, and 1 to 16 AP addresses\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "REQ_RANGE", 4, argc, argv);
}


static int hostapd_cli_cmd_set_rtt_responder_role(struct wpa_ctrl *ctrl,
						  int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "SET_RTT_RESPONDER_ROLE", 1, argc, argv);
}

static int hostapd_cli_cmd_get_rtt_responder_role(struct wpa_ctrl *ctrl,
						  int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid get_rtt_responder_role command: no arguments expected\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, "GET_RTT_RESPONDER_ROLE");
}


#ifdef CONFIG_IEEE80211BE
static int hostapd_cli_cmd_mld_add_link(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return hostapd_cli_cmd(ctrl, "MLD_ADD_LINK", 1, argc, argv);
}
#endif
static int hostapd_cli_cmd_chain_mask(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return hostapd_cli_cmd(ctrl, "CHAIN_MASK", 2, argc, argv);
}

static int hostapd_cli_cmd_get_chain_mask(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_CHAIN_MASK");
}

static int hostapd_cli_cmd_driver_flags(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return wpa_ctrl_command(ctrl, "DRIVER_FLAGS");
}


static int hostapd_cli_cmd_driver_flags2(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return wpa_ctrl_command(ctrl, "DRIVER_FLAGS2");
}

static int hostapd_cli_cmd_channel_bw(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "CHANNEL_BW");
}

#ifdef CONFIG_DPP

static int hostapd_cli_cmd_dpp_qr_code(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_QR_CODE", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_set_key_pair(struct wpa_ctrl *ctrl, int argc,
					     char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_SET_KEYPAIR", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_gen(struct wpa_ctrl *ctrl, int argc,
					     char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_GEN", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_remove(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_REMOVE", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_get_uri(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_GET_URI", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_info(struct wpa_ctrl *ctrl, int argc,
					      char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_INFO", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_bootstrap_set(struct wpa_ctrl *ctrl, int argc,
					     char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_BOOTSTRAP_SET", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_auth_init(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_AUTH_INIT", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_listen(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_LISTEN", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_stop_listen(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return wpa_ctrl_command(ctrl, "DPP_STOP_LISTEN");
}


static int hostapd_cli_cmd_dpp_configurator_add(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_CONFIGURATOR_ADD", 0, argc, argv);
}


static int hostapd_cli_cmd_dpp_configurator_remove(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_CONFIGURATOR_REMOVE", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_configurator_get_key(struct wpa_ctrl *ctrl,
						    int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_CONFIGURATOR_GET_KEY", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_configurator_sign(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
       return hostapd_cli_cmd(ctrl, "DPP_CONFIGURATOR_SIGN", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_pkex_add(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_PKEX_ADD", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_pkex_remove(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_PKEX_REMOVE", 1, argc, argv);
}


#ifdef CONFIG_DPP2

static int hostapd_cli_cmd_dpp_controller_start(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_CONTROLLER_START", 0, argc, argv);
}


static int hostapd_cli_cmd_dpp_controller_stop(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	return wpa_ctrl_command(ctrl, "DPP_CONTROLLER_STOP");
}


static int hostapd_cli_cmd_dpp_chirp(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_CHIRP", 1, argc, argv);
}


static int hostapd_cli_cmd_dpp_stop_chirp(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	return wpa_ctrl_command(ctrl, "DPP_STOP_CHIRP");
}

#endif /* CONFIG_DPP2 */


#ifdef CONFIG_DPP3
static int hostapd_cli_cmd_dpp_push_button(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DPP_PUSH_BUTTON", 0, argc, argv);
}
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */


static int hostapd_cli_cmd_accept_macacl(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	return hostapd_cli_cmd(ctrl, "ACCEPT_ACL", 1, argc, argv);
}


static int hostapd_cli_cmd_deny_macacl(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DENY_ACL", 1, argc, argv);
}


static int hostapd_cli_cmd_poll_sta(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	return hostapd_cli_cmd(ctrl, "POLL_STA", 1, argc, argv);
}


static int hostapd_cli_cmd_req_beacon(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return hostapd_cli_cmd(ctrl, "REQ_BEACON", 2, argc, argv);
}

static int hostapd_cli_cmd_show_rrm_bcn_report(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	return wpa_ctrl_command(ctrl, "SHOW_RRM_BEACON_REPORT");
}

static int hostapd_cli_cmd_req_link_measurement(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	return hostapd_cli_cmd(ctrl, "REQ_LINK_MEASUREMENT", 1, argc, argv);
}


static int hostapd_cli_cmd_req_channel_load(struct wpa_ctrl *ctrl, int argc,
					    char *argv[])
{
	return hostapd_cli_cmd(ctrl, "REQ_CHANNEL_LOAD", 5, argc, argv);
}


static int hostapd_cli_cmd_req_noise_histogram(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	return hostapd_cli_cmd(ctrl, "REQ_NOISE_HISTOGRAM", 5, argc, argv);
}


static int hostapd_cli_cmd_reload_wpa_psk(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD_WPA_PSK");
}

static int hostapd_cli_cmd_dump_mscs_ctxt(struct wpa_ctrl *ctrl,
		int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "DUMP_MSCS_CTXT");
}

static int hostapd_cli_cmd_send_unsolicited_mscs_resp(struct wpa_ctrl *ctrl,
		int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid 'send_unsolicited_mscs_resp' command - "
				"one argument (STA addr) is needed\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "SEND_UNSOLICITED_MSCS_RESP" ,1, argc, argv);
}

#ifdef CONFIG_IEEE80211R_AP

static int hostapd_cli_cmd_get_rxkhs(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_RXKHS");
}


static int hostapd_cli_cmd_reload_rxkhs(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD_RXKHS");
}

#endif /* CONFIG_IEEE80211R_AP */


#ifdef ANDROID
static int hostapd_cli_cmd_driver(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DRIVER", 1, argc, argv);
}
#endif /* ANDROID */

#ifdef CONFIG_IEEE80211BE
static int hostapd_cli_cmd_epcs(struct wpa_ctrl *ctrl, int argc,
				char *argv[])
{
	if (argc < 1) {
		printf("Invalid EPCS command: needs 1 argument atleast\n");
		return -1;
	}
	return hostapd_cli_cmd(ctrl, "EPCS", 1, argc, argv);
}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BE
static int hostapd_cli_cmd_negotiated_ttlm(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	if (argc < 2) {
		printf("Invalid negotiated_ttlm command: needs at least 2 arguments\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "NEGOTIATED_TTLM", 1, argc, argv);
}
static int hostapd_cli_cmd_advertised_ttlm(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "ADVERTISED_TTLM", 3, argc, argv);
}
#endif /* CONFIG_IEEE80211BE */

static int hostapd_cli_cmd_afc(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid AFC command: needs 1 argument atleast\n");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (os_strcmp(argv[0], "get_afc_6g_chan_list") == 0)
		return _wpa_ctrl_command_large(ctrl, "AFC get_afc_6g_chan_list", 1);
#endif /* CONFIG_QCN_EXTN */
	return hostapd_cli_cmd(ctrl, "AFC", 1, argc, argv);
}


#ifdef CONFIG_ATF_OFFLOAD
static int hostapd_cli_cmd_atf_offload(struct wpa_ctrl *ctrl,
				       int argc, char *argv[])
{
	if (argc < 1) {
		printf("Invalid ATF offload command: needs 1 argument atleast\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "ATF_OFFLOAD", 1, argc, argv);
}
#endif /* CONFIG_ATF_OFFLOAD */


static int hostapd_cli_cmd_clear_afc_payload(struct wpa_ctrl *ctrl,
					     int argc, char *argv[])
{
	char cmd[256];
	int res;
	int ret;

	res = os_snprintf(cmd, sizeof(cmd), "CLEAR_AFC_PAYLOAD");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command error\n");
		return -1;
	}

	ret = wpa_ctrl_command(ctrl, cmd);
	return ret;
}


static int hostapd_cli_cmd_reset_afc(struct wpa_ctrl *ctrl,
				     int argc, char *argv[])
{
	char cmd[256];
	int res;
	int ret;

	res = os_snprintf(cmd, sizeof(cmd), "RESET_AFC");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command error\n");
		return -1;
	}

	ret = wpa_ctrl_command(ctrl, cmd);
	return ret;
}


#ifdef CONFIG_IEEE80211BN
static int hostapd_cli_cmd_get_mapc_configs(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_MAPC_CONFIGS");
}


static int hostapd_cli_cmd_set_mapc_configs(struct wpa_ctrl *ctrl,
					    int argc, char *argv[])
{
	char cmd[256];
	int res, i, len = 0;

	if (argc < 1) {
		printf("usage: set_mapc_configs <key>=<value>"
		       " [<key>=<value> ...]\n"
		       "keys: cotdma_en disc_interval disc_mode neg_mode\n"
		       "      max_co_ap max_ctdma max_disc_ap disc_timeout\n"
		       "      neg_timeout inact_timeout disc_cotdma_prof\n"
		       "note: session-only; update hostapd.conf to persist\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_MAPC_CONFIGS");
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;
	len = res;

	for (i = 0; i < argc; i++) {
		if (!os_strchr(argv[i], '=')) {
			printf("error: '%s' missing '='\n", argv[i]);
			return -1;
		}
		res = os_snprintf(cmd + len, sizeof(cmd) - len,
				  " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - len, res)) {
			printf("error: command too long\n");
			return -1;
		}
		len += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_get_mapc_valid_coap_list(struct wpa_ctrl *ctrl,
							int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_MAPC_VALID_COAP_LIST");
}


static int hostapd_cli_cmd_set_mapc_valid_coap_list(struct wpa_ctrl *ctrl,
							int argc, char *argv[])
{
	char cmd[256];
	int res, i, len = 0;

	if (argc < 1) {
		printf("Invalid 'set_mapc_valid_coap_list' command - usage:\n"
		       "  set_mapc_valid_coap_list <MAC0> [MAC1 ... MACN]\n"
		       "  Replaces the entire valid COAP peer list.\n"
		       "  At least one MAC address is required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_MAPC_VALID_COAP_LIST");
	if (os_snprintf_error(sizeof(cmd), res))
		return -1;
	len = res;

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + len, sizeof(cmd) - len, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - len, res)) {
			printf("Command too long\n");
			return -1;
		}
		len += res;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_set_mapc_sta(struct wpa_ctrl *ctrl,
					int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 2) {
		printf("Invalid 'set_mapc_sta' command - usage:\n"
		       "  set_mapc_sta <bssid> cotdma=<0|1>\n"
		       "  Requires mapc_negotiation_mode=1\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_MAPC_STA %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command error (SET_MAPC_STA too long)\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}
#endif /* CONFIG_IEEE80211BN */


#ifdef CONFIG_IEEE80211AX
static int hostapd_cli_cmd_dump_scs(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char buf[100];
	int res;

	if (argc < 2 || argc > 3) {
		printf("Invalid 'dump_scs' command - usage: dump_scs <addr> "
		       "scs_list | scs_info <scs_id> | qm_info <qm_id>\n");
		return -1;
	}

	if (os_strcmp(argv[1], "scs_list") == 0) {
		if (argc != 2) {
			printf("Invalid 'dump_scs <addr> scs_list' usage\n");
			return -1;
		}

		res = os_snprintf(buf, sizeof(buf), "DUMP_SCS_LIST %s",
				  argv[0]);

	} else if (os_strcmp(argv[1], "scs_info") == 0) {
		if (argc != 3) {
			printf("Invalid 'dump_scs <addr> scs_info <scs_id>' usage\n");
			return -1;
		}

		res = os_snprintf(buf, sizeof(buf), "DUMP_SCS_INFO %s %s",
				  argv[0], argv[2]);

	} else if (os_strcmp(argv[1], "qm_info") == 0) {
		if (argc != 3) {
			printf("Invalid 'dump_scs <addr> qm_info <qm_id>' usage\n");
			return -1;
		}
		res = os_snprintf(buf, sizeof(buf), "DUMP_SCS_QM_INFO %s %s",
				  argv[0], argv[2]);

	} else {
		printf("Unknown subcommand for 'dump_scs': %s\n", argv[1]);
		return -1;
	}

	if (os_snprintf_error(sizeof(buf), res)) {
		printf("dump_scs cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_send_unsolicited_scs_resp(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	char buf[100];
	int res;

	if (argc != 5 || os_strcmp(argv[1], "--scsid") != 0 ||
	    os_strcmp(argv[3], "--req_type") != 0) {
		printf("Invalid 'send_unsolicited_scs_resp' usage\n");
		printf("Usage: send_unsolicited_scs_resp <addr> --scsid "
		       "<scsid> --req_type <req_type>\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SEND_UNSOLICITED_SCS_RESP %s %s %s",
			  argv[0], argv[2], argv[4]);

	if (os_snprintf_error(sizeof(buf), res)) {
		printf("send_unsolicited_scs_resp cmd failed\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_scs_configure(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	char buf[1024];
	int res;

	if (argc < 4 || argc > 5) {
		printf("Usage: scs_configure <peer_mac> <scs_sta_mac> <qm_id> "
		       "<scs_desc_hex> [dedicated_queue]\n");
		return -1;
	}

	if (argc == 5)
		res = os_snprintf(buf, sizeof(buf),
				  "SCS_CONFIGURE %s %s %s %s %s",
				  argv[0], argv[1], argv[2],
				  argv[3], argv[4]);
	else
		res = os_snprintf(buf, sizeof(buf),
				  "SCS_CONFIGURE %s %s %s %s",
				  argv[0], argv[1], argv[2], argv[3]);
	if (os_snprintf_error(sizeof(buf), res)) {
		printf("scs_configure cmd failedw\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}


int hostapd_cli_cmd_set_mbssid_tx(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[48];
	int res, len = 0;

	if (argc > 2) {
		printf("Invalid command:\n"
		       "usage: set_mbssid_tx [auto_stop] [auto_start]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_MBSSID_TX");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET_MBSSID_TX command\n");
		return -1;
	}
	len += res;

	if (argc >= 1) {
		res = os_snprintf(cmd + len, sizeof(cmd) - len, " %s", argv[0]);
		if (os_snprintf_error(sizeof(cmd) - len, res)) {
			printf("Failed to add input parameter for SET_MBSSID_TX command\n");
			return -1;
		}
		len += res;

		if (argc == 2) {
			res = os_snprintf(cmd + len, sizeof(cmd) - len, " %s", argv[1]);
			if (os_snprintf_error(sizeof(cmd) - len, res)) {
				printf("Failed to add input parameter for SET_MBSSID_TX command\n");
				return -1;
			}
		}
	}

	return wpa_ctrl_command(ctrl, cmd);
}

#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211AX

static int hostapd_cli_cmd_set_he_bfee_sts(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 2) {
		printf("Invalid usage: set_he_bfee_sts <lteq80 0-7> <gt80 0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_bfee_sts %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_bfee_sts(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_bfee_sts\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_bfee_sts");
}

static int hostapd_cli_cmd_set_he_multi_tid_aggr(struct wpa_ctrl *ctrl,
						  int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_multi_tid_aggr <0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_multi_tid_aggr %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_multi_tid_aggr(struct wpa_ctrl *ctrl,
						  int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_multi_tid_aggr\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_multi_tid_aggr");
}

static int hostapd_cli_cmd_set_he_multi_tid_aggr_tx(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_multi_tid_aggr_tx <0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_multi_tid_aggr_tx %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_multi_tid_aggr_tx(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_multi_tid_aggr_tx\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_multi_tid_aggr_tx");
}

static int hostapd_cli_cmd_set_he_max_ampdu_len_exp(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_max_ampdu_len_exp <0-3>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_max_ampdu_len_exp %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_max_ampdu_len_exp(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_max_ampdu_len_exp\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_max_ampdu_len_exp");
}

static int hostapd_cli_cmd_set_he_su_ppdu_1x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_su_ppdu_1x_ltf_800ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_su_ppdu_1x_ltf_800ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_su_ppdu_1x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_su_ppdu_1x_ltf_800ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_su_ppdu_1x_ltf_800ns_gi");
}

static int hostapd_cli_cmd_set_he_su_mu_ppdu_4x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_su_mu_ppdu_4x_ltf_800ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_he_su_mu_ppdu_4x_ltf_800ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_su_mu_ppdu_4x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_su_mu_ppdu_4x_ltf_800ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_su_mu_ppdu_4x_ltf_800ns_gi");
}

static int hostapd_cli_cmd_set_he_max_frag_msdu(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_max_frag_msdu <0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_max_frag_msdu %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_max_frag_msdu(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_max_frag_msdu\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_max_frag_msdu");
}

static int hostapd_cli_cmd_set_he_min_frag_size(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_min_frag_size <0-3>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_min_frag_size %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_min_frag_size(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_min_frag_size\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_min_frag_size");
}

static int hostapd_cli_cmd_set_he_omi(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_omi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_omi %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_omi(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_omi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_omi");
}

static int hostapd_cli_cmd_set_he_ndp_4x_ltf_3200ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_ndp_4x_ltf_3200ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_ndp_4x_ltf_3200ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_ndp_4x_ltf_3200ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_ndp_4x_ltf_3200ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_ndp_4x_ltf_3200ns_gi");
}

static int hostapd_cli_cmd_set_he_fragmentation(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_fragmentation <0-3>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_fragmentation %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_fragmentation(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_fragmentation\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_fragmentation");
}

static int hostapd_cli_cmd_set_he_amsdu_in_ampdu_suprt(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_amsdu_in_ampdu_suprt <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_amsdu_in_ampdu_suprt %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_amsdu_in_ampdu_suprt(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_amsdu_in_ampdu_suprt\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_amsdu_in_ampdu_suprt");
}

static int hostapd_cli_cmd_set_he_subfee_sts_suprt(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 2) {
		printf("Invalid usage: set_he_subfee_sts_suprt <lteq80 0-7> <gt80 0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_subfee_sts_suprt %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_subfee_sts_suprt(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_subfee_sts_suprt\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_subfee_sts_suprt");
}

static int hostapd_cli_cmd_set_he_max_nc_suprt(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_max_nc_suprt <0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_max_nc_suprt %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_max_nc_suprt(struct wpa_ctrl *ctrl, int argc,
					       char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_max_nc_suprt\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_max_nc_suprt");
}

#ifdef CONFIG_QCN_EXTN
static int hostapd_cli_cmd_set_he_er_su_disable(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_er_su_disable <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_er_su_disable %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_er_su_disable(struct wpa_ctrl *ctrl, int argc,
						char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_er_su_disable\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_er_su_disable");
}

static int hostapd_cli_cmd_set_he_er_su_ppdu_1x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_er_su_ppdu_1x_ltf_800ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_he_er_su_ppdu_1x_ltf_800ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_er_su_ppdu_1x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_er_su_ppdu_1x_ltf_800ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_er_su_ppdu_1x_ltf_800ns_gi");
}

static int hostapd_cli_cmd_set_he_er_su_ppdu_4x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_er_su_ppdu_4x_ltf_800ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_he_er_su_ppdu_4x_ltf_800ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_er_su_ppdu_4x_ltf_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_er_su_ppdu_4x_ltf_800ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_er_su_ppdu_4x_ltf_800ns_gi");
}

static int hostapd_cli_cmd_set_he_1024qam_lt242ru_rx_enable(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_1024qam_lt242ru_rx_enable <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_he_1024qam_lt242ru_rx_enable %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_1024qam_lt242ru_rx_enable(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_1024qam_lt242ru_rx_enable\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_1024qam_lt242ru_rx_enable");
}

static int hostapd_cli_cmd_set_he_full_bw_ul_mumimo(struct wpa_ctrl *ctrl,
						    int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_full_bw_ul_mumimo <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_full_bw_ul_mumimo %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_full_bw_ul_mumimo(struct wpa_ctrl *ctrl,
						    int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_full_bw_ul_mumimo\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_full_bw_ul_mumimo");
}

static int hostapd_cli_cmd_set_he_bsr_support(struct wpa_ctrl *ctrl, int argc,
					      char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_he_bsr_support <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_he_bsr_support %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_bsr_support(struct wpa_ctrl *ctrl, int argc,
					      char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_he_bsr_support\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_he_bsr_support");
}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
static int hostapd_cli_cmd_set_eht_ndp_4x_eht_ltf_and_320nsgi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_ndp_4x_eht_ltf_and_320nsgi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_eht_ndp_4x_eht_ltf_and_320nsgi %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_ndp_4x_eht_ltf_and_320nsgi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_ndp_4x_eht_ltf_and_320nsgi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_ndp_4x_eht_ltf_and_320nsgi");
}

static int hostapd_cli_cmd_set_eht_num_sd(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 3) {
		printf("Invalid usage: set_eht_num_sd <lt80 0-7> <160 0-7> <320 0-7>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_num_sd %s %s %s",
			  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_num_sd(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_num_sd\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_num_sd");
}

static int hostapd_cli_cmd_set_eht_4x_eht_ltf_and_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[96];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_4x_eht_ltf_and_800ns_gi <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_4x_eht_ltf_and_800ns_gi %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_4x_eht_ltf_and_800ns_gi(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_4x_eht_ltf_and_800ns_gi\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_4x_eht_ltf_and_800ns_gi");
}

static int hostapd_cli_cmd_set_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[128];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_rx_1024_and_4096_qam_ls_242_tone_ru <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd),
			  "set_eht_rx_1024_and_4096_qam_ls_242_tone_ru %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
	struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_rx_1024_and_4096_qam_ls_242_tone_ru\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl,
				"get_eht_rx_1024_and_4096_qam_ls_242_tone_ru");
}

static int hostapd_cli_cmd_set_eht_dl_ofdma_txbf(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_dl_ofdma_txbf <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_dl_ofdma_txbf %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_dl_ofdma_txbf(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_dl_ofdma_txbf\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_dl_ofdma_txbf");
}

static int hostapd_cli_cmd_set_eht_trig_su_bf_fb(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_trig_su_bf_fb <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_trig_su_bf_fb %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_trig_su_bf_fb(struct wpa_ctrl *ctrl,
						 int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_trig_su_bf_fb\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_trig_su_bf_fb");
}

static int hostapd_cli_cmd_set_eht_trig_cqi_fb(struct wpa_ctrl *ctrl,
					       int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_trig_cqi_fb <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_trig_cqi_fb %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_trig_cqi_fb(struct wpa_ctrl *ctrl,
					       int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_trig_cqi_fb\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_trig_cqi_fb");
}

static int hostapd_cli_cmd_set_eht_non_trig_cqi_fb(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_non_trig_cqi_fb <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_non_trig_cqi_fb %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_non_trig_cqi_fb(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_non_trig_cqi_fb\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_non_trig_cqi_fb");
}

static int hostapd_cli_cmd_set_eht_sup_mcs15_in_mru(struct wpa_ctrl *ctrl,
						    int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_sup_mcs15_in_mru <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_sup_mcs15_in_mru %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_sup_mcs15_in_mru(struct wpa_ctrl *ctrl,
						    int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_sup_mcs15_in_mru\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_sup_mcs15_in_mru");
}

static int hostapd_cli_cmd_set_eht_mcs14_dup_in_6ghz(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	char cmd[80];
	int res;

	if (argc != 1) {
		printf("Invalid usage: set_eht_mcs14_dup_in_6ghz <0|1>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "set_eht_mcs14_dup_in_6ghz %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_eht_mcs14_dup_in_6ghz(struct wpa_ctrl *ctrl,
						     int argc, char *argv[])
{
	if (argc != 0) {
		printf("Invalid usage: get_eht_mcs14_dup_in_6ghz\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, "get_eht_mcs14_dup_in_6ghz");
}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_QCN_EXTN
static int hostapd_cli_cmd_set_muedca_mode(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	char buf[128];
	int res;

	if (argc != 1 && argc != 3) {
		printf("Invalid 'set_edca_mode' command - usage: <0|1|2> [radio <n>]\n");
		return -1;
	}

	if (argc == 3 && os_strcasecmp(argv[1], "radio") != 0) {
		printf("Invalid argument '%s' - expected 'radio'\n", argv[1]);
		return -1;
	}

	if (argc == 1) {
		res = os_snprintf(buf, sizeof(buf), "SET_EDCA_MODE %s", argv[0]);
	} else {
		res = os_snprintf(buf, sizeof(buf), "SET_EDCA_MODE %s %s %s",
				  argv[0], argv[1], argv[2]);
	}

	if (os_snprintf_error(sizeof(buf), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, buf);
}

static int hostapd_cli_cmd_set_he_mu_edca(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 3) {
		printf("Invalid usage: set_mu_edca <ac> <param> <value>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_MU_EDCA %s %s %s",
			  argv[0], argv[1], argv[2]);

	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}

static int hostapd_cli_cmd_get_he_mu_edca(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 2) {
		printf("Invalid usage: get_mu_edca <ac> <param>\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "GET_MU_EDCA %s %s",
			  argv[0], argv[1]);

	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Command too long\n");
		return -1;
	}

	return wpa_ctrl_command(ctrl, cmd);
}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_QCN_EXTN
/**
 * hostapd_cli_cmd_use_ru_puncture_dfs - Send runtime RU puncture DFS command
 * @ctrl: Pointer to the control interface connection
 * @argc: Number of command arguments
 * @argv: Command argument array
 *
 * Return: hostapd CLI command status.
 */
static int hostapd_cli_cmd_use_ru_puncture_dfs(struct wpa_ctrl *ctrl,
					       int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "USE_RU_PUNCTURE_DFS", 1, argc, argv);
}

/**
 * hostapd_cli_cmd_dfs_disable_auto_unpunc - Send auto-unpuncture CLI command
 * @ctrl: Pointer to the control interface connection
 * @argc: Number of command arguments
 * @argv: Command argument array
 *
 * Return: hostapd CLI command status.
 */
static int hostapd_cli_cmd_dfs_disable_auto_unpunc(struct wpa_ctrl *ctrl,
						   int argc, char *argv[])
{
	return hostapd_cli_cmd(ctrl, "DFS_DISABLE_AUTO_UNPUNC", 1, argc, argv);
}
#endif /* CONFIG_QCN_EXTN */

struct hostapd_cli_cmd {
	const char *cmd;
	int (*handler)(struct wpa_ctrl *ctrl, int argc, char *argv[]);
	char ** (*completion)(const char *str, int pos);
	const char *usage;
};

static const struct hostapd_cli_cmd hostapd_cli_commands[] = {
	{ "ping", hostapd_cli_cmd_ping, NULL,
	  "= pings hostapd" },
	{ "mib", hostapd_cli_cmd_mib, NULL,
	  "= get MIB variables (dot1x, dot11, radius)" },
	{ "relog", hostapd_cli_cmd_relog, NULL,
	  "= reload/truncate debug log output file" },
	{ "close_log", hostapd_cli_cmd_close_log, NULL,
	  "= disable debug log output file" },
	{ "status", hostapd_cli_cmd_status, NULL,
	  "= show interface status info" },
	{ "sta", hostapd_cli_cmd_sta, hostapd_complete_stations,
	  "<addr> = get MIB variables for one station" },
	{ "all_sta", hostapd_cli_cmd_all_sta, NULL,
	   "= get MIB variables for all stations" },
	{ "list_sta", hostapd_cli_cmd_list_sta, NULL,
	   "= list all stations" },
	{ "new_sta", hostapd_cli_cmd_new_sta, NULL,
	  "<addr> = add a new station" },
	{ "deauthenticate", hostapd_cli_cmd_deauthenticate,
	  hostapd_complete_stations,
	  "<addr> = deauthenticate a station" },
	{ "disassociate", hostapd_cli_cmd_disassociate,
	  hostapd_complete_stations,
	  "<addr> = disassociate a station" },
	{ "signature", hostapd_cli_cmd_signature, hostapd_complete_stations,
	  "<addr> = get taxonomy signature for a station" },
	{ "sa_query", hostapd_cli_cmd_sa_query, hostapd_complete_stations,
	  "<addr> = send SA Query to a station" },
	{ "wps_pin", hostapd_cli_cmd_wps_pin, NULL,
	  "<uuid> <pin> [timeout] [addr] = add WPS Enrollee PIN" },
	{ "wps_check_pin", hostapd_cli_cmd_wps_check_pin, NULL,
	  "<PIN> = verify PIN checksum" },
	{ "wps_pbc", hostapd_cli_cmd_wps_pbc, NULL,
	  "= indicate button pushed to initiate PBC" },
	{ "wps_cancel", hostapd_cli_cmd_wps_cancel, NULL,
	  "= cancel the pending WPS operation" },
#ifdef CONFIG_WPS_NFC
	{ "wps_nfc_tag_read", hostapd_cli_cmd_wps_nfc_tag_read, NULL,
	  "<hexdump> = report read NFC tag with WPS data" },
	{ "wps_nfc_config_token", hostapd_cli_cmd_wps_nfc_config_token, NULL,
	  "<WPS/NDEF> = build NFC configuration token" },
	{ "wps_nfc_token", hostapd_cli_cmd_wps_nfc_token, NULL,
	  "<WPS/NDEF/enable/disable> = manager NFC password token" },
	{ "nfc_get_handover_sel", hostapd_cli_cmd_nfc_get_handover_sel, NULL,
	  NULL },
#endif /* CONFIG_WPS_NFC */
	{ "wps_ap_pin", hostapd_cli_cmd_wps_ap_pin, NULL,
	  "<cmd> [params..] = enable/disable AP PIN" },
	{ "wps_config", hostapd_cli_cmd_wps_config, NULL,
	  "<SSID> <auth> <encr> <key> = configure AP" },
	{ "wps_get_status", hostapd_cli_cmd_wps_get_status, NULL,
	  "= show current WPS status" },
	{ "disassoc_imminent", hostapd_cli_cmd_disassoc_imminent, NULL,
	  "= send Disassociation Imminent notification" },
	{ "link_remove", hostapd_cli_cmd_link_remove, NULL,
	  "= remove the link after specified count" },
	{ "ess_disassoc", hostapd_cli_cmd_ess_disassoc, NULL,
	  "= send ESS Dissassociation Imminent notification" },
	{ "bss_tm_req", hostapd_cli_cmd_bss_tm_req, NULL,
	  "= send BSS Transition Management Request" },
	{ "get_config", hostapd_cli_cmd_get_config, NULL,
	  "= show current configuration" },
	{ "dump_tk", hostapd_cli_cmd_dump_tk, NULL,
	  "<addr> <key_idx> = dump TK for station (debug only)" },
	{ "help", hostapd_cli_cmd_help, hostapd_cli_complete_help,
	  "= show this usage help" },
	{ "interface", hostapd_cli_cmd_interface, hostapd_complete_interface,
	  "[ifname] = show interfaces/select interface" },
#ifdef CONFIG_IEEE80211BE
	{ "ml_max_rec_links", hostapd_cli_cmd_ml_max_rec_links, NULL,
	  "= Configures Max ML recommended links Ext MLD CAP" },
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_FST
	{ "fst", hostapd_cli_cmd_fst, NULL,
	  "<params...> = send FST-MANAGER control interface command" },
#endif /* CONFIG_FST */
	{ "raw", hostapd_cli_cmd_raw, NULL,
	  "<params..> = send unprocessed command" },
	{ "level", hostapd_cli_cmd_level, NULL,
	  "<debug level> = change debug level" },
	{ "license", hostapd_cli_cmd_license, NULL,
	  "= show full hostapd_cli license" },
	{ "quit", hostapd_cli_cmd_quit, NULL,
	  "= exit hostapd_cli" },
	{ "set", hostapd_cli_cmd_set, hostapd_complete_set,
	  "<name> <value> = set runtime variables" },
	{ "get", hostapd_cli_cmd_get, hostapd_complete_get,
	  "<name> = get runtime info" },
	{ "get_puncture_sources", hostapd_cli_cmd_puncture_sources, NULL,
	  "= show channels with puncture source USER/RADAR" },
	{ "set_qos_map_set", hostapd_cli_cmd_set_qos_map_set, NULL,
	  "<arg,arg,...> = set QoS Map set element" },
	{ "send_qos_map_conf", hostapd_cli_cmd_send_qos_map_conf,
	  hostapd_complete_stations,
	  "<addr> = send QoS Map Configure frame" },
	{ "bss_priority", hostapd_cli_cmd_set_bss_priority, NULL,
	  "set bss_priority 0-bk 1-be 2-vi 3-vo" },
	{ "set_bss_priority_status", hostapd_cli_cmd_set_bss_priority_status,
	  NULL, "set enable_bss_priority 0-disable 1-enable" },
	{ "get_bss_priority", hostapd_cli_cmd_get_bss_priority, NULL,
	  "get current bss_priority value"},
	{ "get_bss_priority_status", hostapd_cli_cmd_get_bss_priority_status, NULL,
	  "get current bss_priority value"},
	{ "chan_switch", hostapd_cli_cmd_chan_switch, NULL,
	  "<cs_count> <freq> [sec_channel_offset=] [center_freq1=]\n"
	  "  [center_freq2=] [bandwidth=] [bandwidth_device=] \n"
	  "  [center_freq_device=] [blocktx] [ht|vht|he|eht] \n"
	  "  = initiate channel switch announcement" },
	{ "set_channel_usage_element", hostapd_cli_cmd_set_channel_usage_element, NULL,
	  "<0-6> [mode <mode> num_entry <1-10> <op0> <channel0>\n"
	  "[<op1> <channel1>]] [mode <mode> num_entry <1-10> ...]\n"
	  "(up to 6 modes allowed)\n"
	  " = set Channel Usage element" },
	{ "set_6ghz_power_mode", hostapd_cli_cmd_set_pwr_mode, NULL,
	   "<pwr_mode> = 0 - LPI, 1 - SP, 2 - VLP\n"},
#ifdef CONFIG_IEEE80211AX
	{ "color_change", hostapd_cli_cmd_color_change, NULL,
	  "<color> = initiate BSS color change to set the specified color\n"
	  "Value 0 will disable the color.\n"},
	{ "color_collision_ap_period", hostapd_cli_cmd_color_collision_ap_period, NULL,
	  "<seconds> = set BSS color collision AP period" },
	{ "color_change_announcement_count", hostapd_cli_cmd_color_cca_count, NULL,
	  "<count> = set BSS color change announcement countdown" },
#endif /* CONFIG_IEEE80211AX */
	{ "notify_cw_change", hostapd_cli_cmd_notify_cw_change, NULL,
	  "<channel_width> = 0 - 20 MHz, 1 - 40 MHz, 2 - 80 MHz, 3 - 160 MHz" },
	{ "set_bw", hostapd_cli_cmd_set_bw, NULL,
	  "[sec_channel_offset=] [center_freq1=]\n"
	  "  [center_freq2=] [bandwidth=] [ht|vht]\n"
	  "  = change channel bandwidth" },
	{ "hs20_wnm_notif", hostapd_cli_cmd_hs20_wnm_notif, NULL,
	  "<addr> <url>\n"
	  "  = send WNM-Notification Subscription Remediation Request" },
	{ "hs20_deauth_req", hostapd_cli_cmd_hs20_deauth_req, NULL,
	  "<addr> <code (0/1)> <Re-auth-Delay(sec)> [url]\n"
	  "  = send WNM-Notification imminent deauthentication indication" },
	{ "vendor", hostapd_cli_cmd_vendor, NULL,
	  "<vendor id> <sub command id> [<hex formatted data>]\n"
	  "  = send vendor driver command" },
	{ "enable", hostapd_cli_cmd_enable, NULL,
	  "= enable hostapd on current interface" },
	{ "switch_to_rcac", hostapd_cli_cmd_switch_to_rcac, NULL,
	  "= switch AP to the pre-cleared RCAC channel" },
	{ "bgcac_start", hostapd_cli_cmd_bgcac_start, NULL,
	  "= start background CAC (Agile CAC)" },
	{ "reload", hostapd_cli_cmd_reload, NULL,
	  "= reload configuration for current interface" },
	{ "reload_bss", hostapd_cli_cmd_reload_bss, NULL,
	  "= reload configuration for current BSS" },
	{ "reload_config", hostapd_cli_cmd_reload_config, NULL,
	  "= reload configuration for current interface" },
	{"reload_config_bss", hostapd_cli_cmd_reload_config_bss, NULL,
	 "= reload current bss from configuration" },
	{ "disable", hostapd_cli_cmd_disable, NULL,
	  "= disable hostapd on current interface" },
	{ "disable_bss", hostapd_cli_cmd_disable_bss, NULL,
	  "= disable this BSS [tbtt]" },
	{ "enable_bss", hostapd_cli_cmd_enable_bss, NULL,
	  "= enable this BSS" },
	{ "enable_mld", hostapd_cli_cmd_enable_mld, NULL,
	  "= enable AP MLD to which the interface is affiliated" },
	{ "disable_mld", hostapd_cli_cmd_disable_mld, NULL,
	  "= disable AP MLD to which the interface is affiliated" },
	{ "stop_mld", hostapd_cli_cmd_stop_mld, NULL,
	  "= stop specified AP MLD without affecting other APs/MLDs" },
	{ "update_beacon", hostapd_cli_cmd_update_beacon, NULL,
	  "= update Beacon frame contents\n"},
#ifdef CONFIG_IEEE80211BN
	{ "update_uhr_features", hostapd_cli_cmd_update_uhr_features, NULL,
	  "[NPCA enable=<0|1> [primary_chan=chan]\n"
	  "  [min_dur=<0-15>] [switch_delay=<0-63>] [switch_back=<0-63>]\n"
	  "  [init_qsrc=<0-3>] [moplen=<0|1>] [disabled_subch_bitmap=<0xHHHH>]]\n"
	  "= Update UHR feature parameters that requires enhanced critical update"},
#endif /* CONFIG_IEEE80211BN */
	{ "add_tpe", hostapd_cli_cmd_add_tpe, NULL,
	  "add_tpe <tx_pwr_intrpt> <tx_pwr_cnt> <tx_pwr_cat> <tx_pwr …>"},
	{ "del_tpe", hostapd_cli_cmd_del_tpe, NULL,
	  "del_tpe <tx_pwr_intrpt> <tx_pwr_cat>" },
	{ "stop_ap", hostapd_cli_cmd_stop_ap, NULL,
	  "= stop AP\n"},
	{ "erp_flush", hostapd_cli_cmd_erp_flush, NULL,
	  "= drop all ERP keys"},
	{ "log_level", hostapd_cli_cmd_log_level, NULL,
	  "[level] = show/change log verbosity level" },
	{ "log_peer", hostapd_cli_cmd_log_peer, NULL,
	  "<addr>|clear = restrict/clear per-peer log filter" },
	{ "pmksa", hostapd_cli_cmd_pmksa, NULL,
	  " = show PMKSA cache entries" },
	{ "pmksa_flush", hostapd_cli_cmd_pmksa_flush, NULL,
	  " = flush PMKSA cache" },
	{ "set_neighbor", hostapd_cli_cmd_set_neighbor, NULL,
	  "<addr> <ssid=> <nr=> [lci=] [civic=] [stat]\n"
	  "  = add AP to neighbor database" },
	{ "show_neighbor", hostapd_cli_cmd_show_neighbor, NULL,
	  "  = show neighbor database entries" },
	{ "remove_neighbor", hostapd_cli_cmd_remove_neighbor, NULL,
	  "<addr> [ssid=<hex>] = remove AP from neighbor database" },
	{ "send_neighbor", hostapd_cli_cmd_send_neighbor, NULL,
	  "<addr> <ssid=> [dialog_token=] = send neighbor report to addr" },
	{ "req_lci", hostapd_cli_cmd_req_lci, hostapd_complete_stations,
	  "<addr> = send LCI request to a station"},
	{ "req_range", hostapd_cli_cmd_req_range, NULL,
	  " = send FTM range request"},
	{ "set_rtt_responder_role", hostapd_cli_cmd_set_rtt_responder_role, NULL,
	  "<0..0x7> = set RTT responder role bitmask (0=disable)" },
	{ "get_rtt_responder_role", hostapd_cli_cmd_get_rtt_responder_role, NULL,
	  "= get RTT responder role bitmask" },
	{ "driver_flags", hostapd_cli_cmd_driver_flags, NULL,
	  " = show supported driver flags"},
	{ "driver_flags2", hostapd_cli_cmd_driver_flags2, NULL,
	  " = show supported driver flags2"},
#ifdef CONFIG_DPP
	{ "dpp_qr_code", hostapd_cli_cmd_dpp_qr_code, NULL,
	  "report a scanned DPP URI from a QR Code" },
	{ "dpp_bootstrap_set_keypair", hostapd_cli_cmd_dpp_bootstrap_set_key_pair, NULL,
	  "type=<qrcode> [privkey=..] [pubkey=..] [chan=..] [mac=..] [info=..] [curve=..] = generate DPP bootstrap information" },
	{ "dpp_bootstrap_gen", hostapd_cli_cmd_dpp_bootstrap_gen, NULL,
	  "type=<qrcode> [chan=..] [mac=..] [info=..] [curve=..] [key=..] = generate DPP bootstrap information" },
	{ "dpp_bootstrap_remove", hostapd_cli_cmd_dpp_bootstrap_remove, NULL,
	  "*|<id> = remove DPP bootstrap information" },
	{ "dpp_bootstrap_get_uri", hostapd_cli_cmd_dpp_bootstrap_get_uri, NULL,
	  "<id> = get DPP bootstrap URI" },
	{ "dpp_bootstrap_info", hostapd_cli_cmd_dpp_bootstrap_info, NULL,
	  "<id> = show DPP bootstrap information" },
	{ "dpp_bootstrap_set", hostapd_cli_cmd_dpp_bootstrap_set, NULL,
	  "<id> [conf=..] [ssid=<SSID>] [ssid_charset=#] [psk=<PSK>] [pass=<passphrase>] [configurator=<id>] [conn_status=#] [akm_use_selector=<0|1>] [group_id=..] [expiry=#] [csrattrs=..] = set DPP configurator parameters" },
	{ "dpp_auth_init", hostapd_cli_cmd_dpp_auth_init, NULL,
	  "peer=<id> [own=<id>] = initiate DPP bootstrapping" },
	{ "dpp_listen", hostapd_cli_cmd_dpp_listen, NULL,
	  "<freq in MHz> = start DPP listen" },
	{ "dpp_stop_listen", hostapd_cli_cmd_dpp_stop_listen, NULL,
	  "= stop DPP listen" },
	{ "dpp_configurator_add", hostapd_cli_cmd_dpp_configurator_add, NULL,
	  "[curve=..] [key=..] = add DPP configurator" },
	{ "dpp_configurator_remove", hostapd_cli_cmd_dpp_configurator_remove,
	  NULL,
	  "*|<id> = remove DPP configurator" },
	{ "dpp_configurator_get_key", hostapd_cli_cmd_dpp_configurator_get_key,
	  NULL,
	  "<id> = Get DPP configurator's private key" },
	{ "dpp_configurator_sign", hostapd_cli_cmd_dpp_configurator_sign, NULL,
	  "conf=<role> configurator=<id> = generate self DPP configuration" },
	{ "dpp_pkex_add", hostapd_cli_cmd_dpp_pkex_add, NULL,
	  "add PKEX code" },
	{ "dpp_pkex_remove", hostapd_cli_cmd_dpp_pkex_remove, NULL,
	  "*|<id> = remove DPP pkex information" },
#ifdef CONFIG_DPP2
	{ "dpp_controller_start", hostapd_cli_cmd_dpp_controller_start, NULL,
	  "[tcp_port=<port>] [role=..] = start DPP controller" },
	{ "dpp_controller_stop", hostapd_cli_cmd_dpp_controller_stop, NULL,
	  "= stop DPP controller" },
	{ "dpp_chirp", hostapd_cli_cmd_dpp_chirp, NULL,
	  "own=<BI ID> iter=<count> = start DPP chirp" },
	{ "dpp_stop_chirp", hostapd_cli_cmd_dpp_stop_chirp, NULL,
	  "= stop DPP chirp" },
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_DPP3
	{ "dpp_push_button", hostapd_cli_cmd_dpp_push_button, NULL,
	  "= press DPP push button" },
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */
	{ "accept_acl", hostapd_cli_cmd_accept_macacl, NULL,
	  "=Add/Delete/Show/Clear accept MAC ACL" },
	{ "deny_acl", hostapd_cli_cmd_deny_macacl, NULL,
	  "=Add/Delete/Show/Clear deny MAC ACL" },

#ifdef CONFIG_QCN_EXTN
	HOSTAPD_CLI_CMDS_EXTN
#endif /* CONFIG_QCN_EXTN */

	{ "poll_sta", hostapd_cli_cmd_poll_sta, hostapd_complete_stations,
	  "<addr> = poll a STA to check connectivity with a QoS null frame" },
	{ "channel_bw", hostapd_cli_cmd_channel_bw, NULL,
	  "= show allowed bandwidth on each channel"},
	{ "req_beacon", hostapd_cli_cmd_req_beacon, NULL,
	  "<addr> [req_mode=] <measurement request hexdump>  = send a Beacon report request to a station" },
	{ "show_rrm_beacon_report", hostapd_cli_cmd_show_rrm_bcn_report, NULL,
	  "= show recent received RRM Beacon Report"},
	{ "req_link_measurement", hostapd_cli_cmd_req_link_measurement, NULL,
	  "<addr> = send a link measurement report request to a station"},
	{ "req_channel_load", hostapd_cli_cmd_req_channel_load, NULL,
	  "<addr> <op_class> <channel> <rand_interval> <duration> = send a channel load request to a station"},
	{ "req_noise_histogram", hostapd_cli_cmd_req_noise_histogram, NULL,
	  "<addr> <op_class> <channel> <random_interval> <duration> = send a noise histogram report request to a station" },
	{ "reload_wpa_psk", hostapd_cli_cmd_reload_wpa_psk, NULL,
	  "= reload wpa_psk_file only" },
#ifdef CONFIG_IEEE80211R_AP
	{ "reload_rxkhs", hostapd_cli_cmd_reload_rxkhs, NULL,
	  "= reload R0KHs and R1KHs" },
	{ "get_rxkhs", hostapd_cli_cmd_get_rxkhs, NULL,
	  "= get R0KHs and R1KHs" },
#endif /* CONFIG_IEEE80211R_AP */
#ifdef ANDROID
	{ "driver", hostapd_cli_cmd_driver, NULL,
	  "<driver sub command> [<hex formatted data>] = send driver command data" },
#endif /* ANDROID */
#ifdef CONFIG_ATF_OFFLOAD
	{"atf_offload", hostapd_cli_cmd_atf_offload, NULL, "= send atf commands" },
#endif /* CONFIG_ATF_OFFLOAD */
#ifdef CONFIG_IEEE80211BE
	{ "mld_add_link", hostapd_cli_cmd_mld_add_link, NULL,
	"<config_file_location>" },
	{ "epcs", hostapd_cli_cmd_epcs, NULL,
	  "[session_initiate|session_teardown|show] [<peer_mld_mac>|mu_edca_params|wmm_params]"},
	{ "negotiated_ttlm", hostapd_cli_cmd_negotiated_ttlm, NULL,
	  "= send ttlm test commands" },
	{ "advertised_ttlm", hostapd_cli_cmd_advertised_ttlm, NULL,
	  "ieee_link_map= map_switch_time= expected_dur= link_mapping_size=\n"
	  "  = Trigger advertised TTLM" },
	{ "dump_mscs_ctxt",
	  hostapd_cli_cmd_dump_mscs_ctxt, NULL,
	  "= dump mscs context for all the associated STAs"},
	{ "send_unsolicited_mscs_resp",
	  hostapd_cli_cmd_send_unsolicited_mscs_resp,
	  hostapd_complete_stations,
	  "<addr> = send unsolicited MSCS response frame" },
#endif
	{"set_dscp_policy", hostapd_cli_cmd_set_dscp_policy, NULL,
	 "[policy_id=] [request_type=] [dscp=] [classifier_mask=]\n"
	 "[ip_version=] [dst_ip=] = Set DSCP policy"},
	{"send_unsolicited_dscp_req", hostapd_cli_cmd_send_unsolicited_dscp_req, NULL,
	 "<addr>, [reset=], [policy_list=], Send unsolicited DSCP request"},
	{ "chain_mask", hostapd_cli_cmd_chain_mask, NULL,
	"<tx chain mask> <rx chain mask>" },
	{ "get_chain_mask", hostapd_cli_cmd_get_chain_mask, NULL,
	 "= Get chain mask value of selected interface/link" },
	{ "afc", hostapd_cli_cmd_afc, NULL,
	  "[set_afc_chan_sel_config|get_afc_chan_sel_config|get_afc_6g_chan_list] <afc_chan_sel_config_value>" },
	{ "clear_afc_payload", hostapd_cli_cmd_clear_afc_payload, NULL,
	  "= Clear AFC payload stored in driver and firmware\n"},
	{ "reset_afc", hostapd_cli_cmd_reset_afc, NULL,
	  "= Reset AFC in target\n"},
#ifdef CONFIG_IEEE80211AX
	{ "dump_scs", hostapd_cli_cmd_dump_scs, NULL,
	  "<addr> scs_list | scs_info <scs_id> | qm_info <qm_id> = "
	  "Dump SCS list or descriptor info by SCS ID or QM ID" },
	{ "send_unsolicited_scs_resp", hostapd_cli_cmd_send_unsolicited_scs_resp,
	  NULL, "<addr> --scsid <scsid> --req_type <req_type> = "
	  "Send unsolicited SCS response to the STA" },
	{ "scs_configure", hostapd_cli_cmd_scs_configure, NULL,
	  "<peer_mac> <scs_sta_mac> <qm_id> <scs_desc_hex> "
	  "[<dedicated_queue>] = Configure SCS session via application" },
	{ "set_mbssid_tx", hostapd_cli_cmd_set_mbssid_tx, NULL,
	  "[auto_stop] [auto_start]\n"
	  "= Stop all profiles from MBSSID group if auto_stop option is given, "
	  "set given link as the transmitted profile of the group. Restart all "
	  "profiles if auto_start is given"
	  "is provided\n"},
	{ "set_he_bfee_sts", hostapd_cli_cmd_set_he_bfee_sts, NULL,
	  "<lteq80 0-7> <gt80 0-7> = set HE SU beamformee STS support" },
	{ "get_he_bfee_sts", hostapd_cli_cmd_get_he_bfee_sts, NULL,
	  "= get HE SU beamformee STS support as: 0x<lteq80> 0x<gt80>" },
	{ "set_he_multi_tid_aggr", hostapd_cli_cmd_set_he_multi_tid_aggr, NULL,
	  "<0-7> = set HE Multi-TID aggregation RX support" },
	{ "get_he_multi_tid_aggr", hostapd_cli_cmd_get_he_multi_tid_aggr, NULL,
	  "= get HE Multi-TID aggregation RX support" },
	{ "set_he_multi_tid_aggr_rx", hostapd_cli_cmd_set_he_multi_tid_aggr,
	  NULL, "<0-7> = set HE Multi-TID aggregation RX support" },
	{ "get_he_multi_tid_aggr_rx", hostapd_cli_cmd_get_he_multi_tid_aggr,
	  NULL, "= get HE Multi-TID aggregation RX support" },
	{ "set_he_multi_tid_aggr_tx", hostapd_cli_cmd_set_he_multi_tid_aggr_tx,
	  NULL, "<0-7> = set HE Multi-TID aggregation TX support" },
	{ "get_he_multi_tid_aggr_tx", hostapd_cli_cmd_get_he_multi_tid_aggr_tx,
	  NULL, "= get HE Multi-TID aggregation TX support" },
	{ "set_he_max_ampdu_len_exp", hostapd_cli_cmd_set_he_max_ampdu_len_exp,
	  NULL, "<0-3> = set HE max AMPDU length exponent extension" },
	{ "get_he_max_ampdu_len_exp", hostapd_cli_cmd_get_he_max_ampdu_len_exp,
	  NULL, "= get HE max AMPDU length exponent extension" },
	{ "set_he_su_ppdu_1x_ltf_800ns_gi",
	  hostapd_cli_cmd_set_he_su_ppdu_1x_ltf_800ns_gi, NULL,
	  "<0|1> = set HE SU PPDU 1x LTF 800ns GI support" },
	{ "get_he_su_ppdu_1x_ltf_800ns_gi",
	  hostapd_cli_cmd_get_he_su_ppdu_1x_ltf_800ns_gi, NULL,
	  "= get HE SU PPDU 1x LTF 800ns GI support" },
	{ "set_he_su_mu_ppdu_4x_ltf_800ns_gi",
	  hostapd_cli_cmd_set_he_su_mu_ppdu_4x_ltf_800ns_gi, NULL,
	  "<0|1> = set HE SU/MU PPDU 4x LTF 800ns GI support" },
	{ "get_he_su_mu_ppdu_4x_ltf_800ns_gi",
	  hostapd_cli_cmd_get_he_su_mu_ppdu_4x_ltf_800ns_gi, NULL,
	  "= get HE SU/MU PPDU 4x LTF 800ns GI support" },
	{ "set_he_max_frag_msdu", hostapd_cli_cmd_set_he_max_frag_msdu, NULL,
	  "<0-7> = set HE max fragmented MSDUs" },
	{ "get_he_max_frag_msdu", hostapd_cli_cmd_get_he_max_frag_msdu, NULL,
	  "= get HE max fragmented MSDUs" },
	{ "set_he_min_frag_size", hostapd_cli_cmd_set_he_min_frag_size, NULL,
	  "<0-3> = set HE minimum fragment size" },
	{ "get_he_min_frag_size", hostapd_cli_cmd_get_he_min_frag_size, NULL,
	  "= get HE minimum fragment size" },
	{ "set_he_omi", hostapd_cli_cmd_set_he_omi, NULL,
	  "<0|1> = set HE OMI capability" },
	{ "get_he_omi", hostapd_cli_cmd_get_he_omi, NULL,
	  "= get HE OMI capability" },
	{ "set_he_ndp_4x_ltf_3200ns_gi",
	  hostapd_cli_cmd_set_he_ndp_4x_ltf_3200ns_gi, NULL,
	  "<0|1> = set HE NDP 4x LTF 3200ns GI support" },
	{ "get_he_ndp_4x_ltf_3200ns_gi",
	  hostapd_cli_cmd_get_he_ndp_4x_ltf_3200ns_gi, NULL,
	  "= get HE NDP 4x LTF 3200ns GI support" },
	{ "set_he_fragmentation", hostapd_cli_cmd_set_he_fragmentation, NULL,
	  "<0-3> = set HE fragmentation support level" },
	{ "get_he_fragmentation", hostapd_cli_cmd_get_he_fragmentation, NULL,
	  "= get HE fragmentation support level" },
	{ "set_he_amsdu_in_ampdu_suprt",
	  hostapd_cli_cmd_set_he_amsdu_in_ampdu_suprt, NULL,
	  "<0|1> = set HE AMSDU in AMPDU support" },
	{ "get_he_amsdu_in_ampdu_suprt",
	  hostapd_cli_cmd_get_he_amsdu_in_ampdu_suprt, NULL,
	  "= get HE AMSDU in AMPDU support" },
	{ "set_he_subfee_sts_suprt",
	  hostapd_cli_cmd_set_he_subfee_sts_suprt, NULL,
	  "<lteq80 0-7> <gt80 0-7> = set HE subfee STS support" },
	{ "get_he_subfee_sts_suprt",
	  hostapd_cli_cmd_get_he_subfee_sts_suprt, NULL,
	  "= get HE subfee STS support as: 0x<lteq80> 0x<gt80>" },
	{ "set_he_max_nc_suprt", hostapd_cli_cmd_set_he_max_nc_suprt, NULL,
	  "<0-7> = set HE max NC support" },
	{ "get_he_max_nc_suprt", hostapd_cli_cmd_get_he_max_nc_suprt, NULL,
	  "= get HE max NC support" },
#ifdef CONFIG_QCN_EXTN
	{ "set_he_er_su_disable", hostapd_cli_cmd_set_he_er_su_disable, NULL,
	  "<0|1> = set HE ER SU disable" },
	{ "get_he_er_su_disable", hostapd_cli_cmd_get_he_er_su_disable, NULL,
	  "= get HE ER SU disable" },
	{ "set_he_er_su_ppdu_1x_ltf_800ns_gi",
	  hostapd_cli_cmd_set_he_er_su_ppdu_1x_ltf_800ns_gi, NULL,
	  "<0|1> = set HE ER SU PPDU 1x LTF 800ns GI support" },
	{ "get_he_er_su_ppdu_1x_ltf_800ns_gi",
	  hostapd_cli_cmd_get_he_er_su_ppdu_1x_ltf_800ns_gi, NULL,
	  "= get HE ER SU PPDU 1x LTF 800ns GI support" },
	{ "set_he_er_su_ppdu_4x_ltf_800ns_gi",
	  hostapd_cli_cmd_set_he_er_su_ppdu_4x_ltf_800ns_gi, NULL,
	  "<0|1> = set HE ER SU PPDU 4x LTF 800ns GI support" },
	{ "get_he_er_su_ppdu_4x_ltf_800ns_gi",
	  hostapd_cli_cmd_get_he_er_su_ppdu_4x_ltf_800ns_gi, NULL,
	  "= get HE ER SU PPDU 4x LTF 800ns GI support" },
	{ "set_he_1024qam_lt242ru_rx_enable",
	  hostapd_cli_cmd_set_he_1024qam_lt242ru_rx_enable, NULL,
	  "<0|1> = set HE RX 1024QAM for <242-tone RU support" },
	{ "get_he_1024qam_lt242ru_rx_enable",
	  hostapd_cli_cmd_get_he_1024qam_lt242ru_rx_enable, NULL,
	  "= get HE RX 1024QAM for <242-tone RU support" },
	{ "set_he_full_bw_ul_mumimo",
	  hostapd_cli_cmd_set_he_full_bw_ul_mumimo, NULL,
	  "<0|1> = set HE full BW UL MU-MIMO support" },
	{ "get_he_full_bw_ul_mumimo",
	  hostapd_cli_cmd_get_he_full_bw_ul_mumimo, NULL,
	  "= get HE full BW UL MU-MIMO support" },
	{ "set_he_bsr_support", hostapd_cli_cmd_set_he_bsr_support, NULL,
	  "<0|1> = set HE BSR support" },
	{ "get_he_bsr_support", hostapd_cli_cmd_get_he_bsr_support, NULL,
	  "= get HE BSR support" },
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	{ "set_eht_ndp_4x_eht_ltf_and_320nsgi",
	  hostapd_cli_cmd_set_eht_ndp_4x_eht_ltf_and_320nsgi, NULL,
	  "<0|1> = set EHT NDP 4x EHT-LTF and 320ns GI support" },
	{ "get_eht_ndp_4x_eht_ltf_and_320nsgi",
	  hostapd_cli_cmd_get_eht_ndp_4x_eht_ltf_and_320nsgi, NULL,
	  "= get EHT NDP 4x EHT-LTF and 320ns GI support" },
	{ "set_eht_num_sd", hostapd_cli_cmd_set_eht_num_sd, NULL,
	  "<lt80 0-7> <160 0-7> <320 0-7> = set EHT number of sounding dimensions" },
	{ "get_eht_num_sd", hostapd_cli_cmd_get_eht_num_sd, NULL,
	  "= get EHT number of sounding dimensions as: 0x<lt80> 0x<160> 0x<320>" },
	{ "set_eht_4x_eht_ltf_and_800ns_gi",
	  hostapd_cli_cmd_set_eht_4x_eht_ltf_and_800ns_gi, NULL,
	  "<0|1> = set EHT 4x EHT-LTF and 800ns GI support" },
	{ "get_eht_4x_eht_ltf_and_800ns_gi",
	  hostapd_cli_cmd_get_eht_4x_eht_ltf_and_800ns_gi, NULL,
	  "= get EHT 4x EHT-LTF and 800ns GI support" },
	{ "set_eht_rx_1024_and_4096_qam_ls_242_tone_ru",
	  hostapd_cli_cmd_set_eht_rx_1024_and_4096_qam_ls_242_tone_ru, NULL,
	  "<0|1> = set EHT RX 1024/4096-QAM for <242-tone RU support" },
	{ "get_eht_rx_1024_and_4096_qam_ls_242_tone_ru",
	  hostapd_cli_cmd_get_eht_rx_1024_and_4096_qam_ls_242_tone_ru, NULL,
	  "= get EHT RX 1024/4096-QAM for <242-tone RU support" },
	{ "set_eht_dl_ofdma_txbf", hostapd_cli_cmd_set_eht_dl_ofdma_txbf, NULL,
	  "<0|1> = set EHT DL OFDMA TX beamforming support" },
	{ "get_eht_dl_ofdma_txbf", hostapd_cli_cmd_get_eht_dl_ofdma_txbf, NULL,
	  "= get EHT DL OFDMA TX beamforming support" },
	{ "set_eht_trig_su_bf_fb", hostapd_cli_cmd_set_eht_trig_su_bf_fb, NULL,
	  "<0|1> = set EHT triggered SU beamforming feedback support" },
	{ "get_eht_trig_su_bf_fb", hostapd_cli_cmd_get_eht_trig_su_bf_fb, NULL,
	  "= get EHT triggered SU beamforming feedback support" },
	{ "set_eht_trig_cqi_fb", hostapd_cli_cmd_set_eht_trig_cqi_fb, NULL,
	  "<0|1> = set EHT triggered CQI feedback support" },
	{ "get_eht_trig_cqi_fb", hostapd_cli_cmd_get_eht_trig_cqi_fb, NULL,
	  "= get EHT triggered CQI feedback support" },
	{ "set_eht_non_trig_cqi_fb", hostapd_cli_cmd_set_eht_non_trig_cqi_fb,
	  NULL,
	  "<0|1> = set EHT non-triggered CQI feedback support" },
	{ "get_eht_non_trig_cqi_fb", hostapd_cli_cmd_get_eht_non_trig_cqi_fb,
	  NULL,
	  "= get EHT non-triggered CQI feedback support" },
	{ "set_eht_sup_mcs15_in_mru",
	  hostapd_cli_cmd_set_eht_sup_mcs15_in_mru, NULL,
	  "<0|1> = set EHT MCS15 support in MRU" },
	{ "get_eht_sup_mcs15_in_mru",
	  hostapd_cli_cmd_get_eht_sup_mcs15_in_mru, NULL,
	  "= get EHT MCS15 support in MRU" },
	{ "set_eht_mcs14_dup_in_6ghz",
	  hostapd_cli_cmd_set_eht_mcs14_dup_in_6ghz, NULL,
	  "<0|1> = set EHT MCS14 duplicate support in 6GHz" },
	{ "get_eht_mcs14_dup_in_6ghz",
	  hostapd_cli_cmd_get_eht_mcs14_dup_in_6ghz, NULL,
	  "= get EHT MCS14 duplicate support in 6GHz" },
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_QCN_EXTN
	{ "set_edca_mode", hostapd_cli_cmd_set_muedca_mode, NULL,
	  "<mode> [radio <n>] = set MU-EDCA mode\n"
	  "mode: 0=user, 1=host, 2=firmware (default)\n"
	  "radio: optional radio index (omit for all radios)" },
	{ "set_mu_edca", hostapd_cli_cmd_set_he_mu_edca, NULL,
	  "<ac> <param> <value> = set HE MU EDCA parameters"
	  "<ac>: Access category (be, bk, vi, vo)\n"
	  "<param>: Parameter name (aifsn, ecwmin, ecwmax, timer, acm)\n"
	  "<value>: Parameter value\n" },
	{ "get_mu_edca", hostapd_cli_cmd_get_he_mu_edca, NULL,
	  "<ac> <param> = get HE MU EDCA parameter value\n"
	  "<ac>: Access category (be, bk, vi, vo)\n"
	  "<param>: Parameter name (aifsn, ecwmin, ecwmax, timer, acm)\n" },
	{ "use_ru_puncture_dfs", hostapd_cli_cmd_use_ru_puncture_dfs, NULL,
	  "<1/0> = enable/disable Puncturing feature for DFS channels" },
	{ "dfs_disable_auto_unpunc", hostapd_cli_cmd_dfs_disable_auto_unpunc, NULL,
	  "<0|1> = disable/enable automatic unpuncturing of DFS channels after CAC" },
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BN
	{ "get_mapc_configs", hostapd_cli_cmd_get_mapc_configs, NULL,
	  "= get MAPC config (set_mapc_configs keys) and runtime state (bitmaps/counters/timer)" },
	{ "set_mapc_configs", hostapd_cli_cmd_set_mapc_configs, NULL,
	  "<key>=<value> [<key>=<value> ...] = set MAPC config at runtime\n"
	  "  cotdma_en [0|1]        disc_interval [0..3600]\n"
	  "  disc_mode [0|1]        neg_mode      [0|1]\n"
	  "  max_co_ap [1..12]      max_ctdma     [0..12]\n"
	  "  max_disc_ap [1..12]    disc_timeout  [1..300]\n"
	  "  neg_timeout [1..300]   inact_timeout [0..86400]\n"
	  "  disc_cotdma_prof [0|1]" },
	{ "get_mapc_valid_coap_list", hostapd_cli_cmd_get_mapc_valid_coap_list, NULL,
	  "= get the MAPC valid COAP list" },
	{ "set_mapc_valid_coap_list", hostapd_cli_cmd_set_mapc_valid_coap_list, NULL,
	  "<MAC0> [MAC1 ... MACN] = replace the valid COAP peer list (full replace)" },
	{ "set_mapc_sta", hostapd_cli_cmd_set_mapc_sta, NULL,
	  "<bssid> cotdma=<0|1> = establish (1) or teardown (0) Co-TDMA agreement\n"
	  "  Requires mapc_negotiation_mode=1 (manual mode)" },
#endif /* CONFIG_IEEE80211BN */
	{ NULL, NULL, NULL, NULL }
};


/*
 * Prints command usage, lines are padded with the specified string.
 */
static void print_cmd_help(FILE *stream, const struct hostapd_cli_cmd *cmd,
			   const char *pad)
{
	char c;
	size_t n;

	if (cmd->usage == NULL)
		return;
	fprintf(stream, "%s%s ", pad, cmd->cmd);
	for (n = 0; (c = cmd->usage[n]); n++) {
		fprintf(stream, "%c", c);
		if (c == '\n')
			fprintf(stream, "%s", pad);
	}
	fprintf(stream, "\n");
}


static void print_help(FILE *stream, const char *cmd)
{
	int n;

	fprintf(stream, "commands:\n");
	for (n = 0; hostapd_cli_commands[n].cmd; n++) {
		if (cmd == NULL || str_starts(hostapd_cli_commands[n].cmd, cmd))
			print_cmd_help(stream, &hostapd_cli_commands[n], "  ");
	}
}


static void wpa_request(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	const struct hostapd_cli_cmd *cmd, *match = NULL;
	int count;

	count = 0;
	cmd = hostapd_cli_commands;
	while (cmd->cmd) {
		if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
			match = cmd;
			if (os_strcasecmp(cmd->cmd, argv[0]) == 0) {
				/* we have an exact match */
				count = 1;
				break;
			}
			count++;
		}
		cmd++;
	}

	if (count > 1) {
		printf("Ambiguous command '%s'; possible commands:", argv[0]);
		cmd = hostapd_cli_commands;
		while (cmd->cmd) {
			if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) ==
			    0) {
				printf(" %s", cmd->cmd);
			}
			cmd++;
		}
		printf("\n");
	} else if (count == 0) {
		printf("Unknown command '%s'\n", argv[0]);
	} else {
		match->handler(ctrl, argc - 1, &argv[1]);
	}
}


static void cli_event(const char *str)
{
	const char *start, *s;

	start = os_strchr(str, '>');
	if (start == NULL)
		return;

	start++;

	if (str_starts(start, AP_STA_CONNECTED)) {
		s = os_strchr(start, ' ');
		if (s == NULL)
			return;
		cli_txt_list_add(&stations, s + 1);
		return;
	}

	if (str_starts(start, AP_STA_DISCONNECTED)) {
		s = os_strchr(start, ' ');
		if (s == NULL)
			return;
		cli_txt_list_del_addr(&stations, s + 1);
		return;
	}
}


static void hostapd_cli_recv_pending(struct wpa_ctrl *ctrl, int in_read,
				     int action_monitor)
{
	int first = 1;
	if (ctrl_conn == NULL)
		return;
	while (wpa_ctrl_pending(ctrl)) {
#ifdef CONFIG_QCN_EXTN
		char buf[16384];
#else
		char buf[4096];
#endif /* CONFIG_QCN_EXTN */
		size_t len = sizeof(buf) - 1;
		if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
			buf[len] = '\0';
			if (action_monitor)
				hostapd_cli_action_process(buf, len);
			else {
				cli_event(buf);
				if (in_read && first)
					printf("\n");
				first = 0;
				printf("%s\n", buf);
			}
		} else {
			printf("Could not read pending message.\n");
			break;
		}
	}
}


static void hostapd_cli_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	hostapd_cli_recv_pending(ctrl_conn, 0, 0);
}


static void hostapd_cli_ping(void *eloop_ctx, void *timeout_ctx)
{
	if (ctrl_conn && _wpa_ctrl_command(ctrl_conn, "PING", 0)) {
		printf("Connection to hostapd lost - trying to reconnect\n");
		hostapd_cli_close_connection();
	}
	if (!ctrl_conn && hostapd_cli_reconnect(ctrl_ifname) == 0)
		printf("Connection to hostapd re-established\n");
	if (ctrl_conn)
		hostapd_cli_recv_pending(ctrl_conn, 1, 0);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_ping, NULL, NULL);
}


static void hostapd_cli_eloop_terminate(int sig, void *signal_ctx)
{
	eloop_terminate();
}


static void hostapd_cli_edit_cmd_cb(void *ctx, char *cmd)
{
	char *argv[max_args];
	int argc;
	argc = tokenize_cmd(cmd, argv);
	if (argc)
		wpa_request(ctrl_conn, argc, argv);
}


static void hostapd_cli_edit_eof_cb(void *ctx)
{
	eloop_terminate();
}


static char ** list_cmd_list(void)
{
	char **res;
	int i, count;

	count = ARRAY_SIZE(hostapd_cli_commands);
	res = os_calloc(count + 1, sizeof(char *));
	if (res == NULL)
		return NULL;

	for (i = 0; hostapd_cli_commands[i].cmd; i++) {
		res[i] = os_strdup(hostapd_cli_commands[i].cmd);
		if (res[i] == NULL)
			break;
	}

	return res;
}


static char ** hostapd_cli_cmd_completion(const char *cmd, const char *str,
				      int pos)
{
	int i;

	for (i = 0; hostapd_cli_commands[i].cmd; i++) {
		if (os_strcasecmp(hostapd_cli_commands[i].cmd, cmd) != 0)
			continue;
		if (hostapd_cli_commands[i].completion)
			return hostapd_cli_commands[i].completion(str, pos);
		if (!hostapd_cli_commands[i].usage)
			return NULL;
		edit_clear_line();
		printf("\r%s\n", hostapd_cli_commands[i].usage);
		edit_redraw();
		break;
	}

	return NULL;
}


static char ** hostapd_cli_edit_completion_cb(void *ctx, const char *str,
					      int pos)
{
	char **res;
	const char *end;
	char *cmd;

	end = os_strchr(str, ' ');
	if (end == NULL || str + pos < end)
		return list_cmd_list();

	cmd = os_malloc(pos + 1);
	if (cmd == NULL)
		return NULL;
	os_memcpy(cmd, str, pos);
	cmd[end - str] = '\0';
	res = hostapd_cli_cmd_completion(cmd, str, pos);
	os_free(cmd);
	return res;
}


static void hostapd_cli_interactive(void)
{
	char *hfile = NULL;
	char *home;

	printf("\nInteractive mode\n\n");

#ifdef CONFIG_HOSTAPD_CLI_HISTORY_DIR
	home = CONFIG_HOSTAPD_CLI_HISTORY_DIR;
#else /* CONFIG_HOSTAPD_CLI_HISTORY_DIR */
	home = getenv("HOME");
#endif /* CONFIG_HOSTAPD_CLI_HISTORY_DIR */
	if (home) {
		const char *fname = ".hostapd_cli_history";
		int hfile_len = os_strlen(home) + 1 + os_strlen(fname) + 1;
		hfile = os_malloc(hfile_len);
		if (hfile)
			os_snprintf(hfile, hfile_len, "%s/%s", home, fname);
	}

	edit_init(hostapd_cli_edit_cmd_cb, hostapd_cli_edit_eof_cb,
		  hostapd_cli_edit_completion_cb, NULL, hfile, NULL);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_ping, NULL, NULL);

	eloop_run();

	cli_txt_list_flush(&stations);
	edit_deinit(hfile, NULL);
	os_free(hfile);
	eloop_cancel_timeout(hostapd_cli_ping, NULL, NULL);
}


static void hostapd_cli_cleanup(void)
{
	hostapd_cli_close_connection();
	if (pid_file)
		os_daemonize_terminate(pid_file);

	os_program_deinit();
}


static void hostapd_cli_action_ping(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_ctrl *ctrl = eloop_ctx;
	char buf[256];
	size_t len;

	/* verify that connection is still working */
	len = sizeof(buf) - 1;
	if (wpa_ctrl_request(ctrl, "PING", 4, buf, &len,
			     hostapd_cli_action_cb) < 0 ||
	    len < 4 || os_memcmp(buf, "PONG", 4) != 0) {
		printf("hostapd did not reply to PING command - open a new connection\n");
		hostapd_cli_close_connection();
		if (hostapd_cli_reconnect(ctrl_ifname)) {
			printf("Failed to establish new connection - exit\n");
			eloop_terminate();
			return;
		}
	}
	eloop_register_timeout(ping_interval, 0, hostapd_cli_action_ping,
			       ctrl, NULL);
}


static void hostapd_cli_action_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct wpa_ctrl *ctrl = eloop_ctx;

	hostapd_cli_recv_pending(ctrl, 0, 1);
}


static void hostapd_cli_action(struct wpa_ctrl *ctrl)
{
	int fd;

	fd = wpa_ctrl_get_fd(ctrl);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_action_ping,
			       ctrl, NULL);
	eloop_register_read_sock(fd, hostapd_cli_action_receive, ctrl, NULL);
	eloop_run();
	eloop_cancel_timeout(hostapd_cli_action_ping, ctrl, NULL);
	eloop_unregister_read_sock(fd);
}


int main(int argc, char *argv[])
{
	int warning_displayed = 0;
	int c;
	int daemonize = 0;
	int reconnect = 0;
#ifdef CONFIG_IEEE80211BE
	int link_id = -1;
#endif /* CONFIG_IEEE80211BE */

	if (os_program_init())
		return -1;

	for (;;) {
		c = getopt(argc, argv, "a:BhG:i:l:p:P:rs:v");
		if (c < 0)
			break;
		switch (c) {
		case 'a':
			action_file = optarg;
			break;
		case 'B':
			daemonize = 1;
			break;
		case 'G':
			ping_interval = atoi(optarg);
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			printf("%s\n", hostapd_cli_version);
			return 0;
		case 'i':
			os_free(ctrl_ifname);
			ctrl_ifname = os_strdup(optarg);
			break;
		case 'p':
			ctrl_iface_dir = optarg;
			break;
		case 'P':
			pid_file = optarg;
			break;
		case 'r':
			reconnect = 1;
			break;
		case 's':
			client_socket_dir = optarg;
			break;
#ifdef CONFIG_IEEE80211BE
		case 'l':
			link_id = atoi(optarg);
			break;
#endif /* CONFIG_IEEE80211BE */
		default:
			usage();
			return -1;
		}
	}

	interactive = (argc == optind) && (action_file == NULL);

	if (interactive) {
		printf("%s\n\n%s\n\n", hostapd_cli_version, cli_license);
	}

	if (eloop_init())
		return -1;

	for (;;) {
		if (ctrl_ifname == NULL) {
			struct dirent *dent;
			DIR *dir = opendir(ctrl_iface_dir);
			if (dir) {
				while ((dent = readdir(dir))) {
					if (os_strcmp(dent->d_name, ".") == 0
					    ||
					    os_strcmp(dent->d_name, "..") == 0)
						continue;
					printf("Selected interface '%s'\n",
					       dent->d_name);
					ctrl_ifname = os_strdup(dent->d_name);
					break;
				}
				closedir(dir);
			}
		}

#ifdef CONFIG_IEEE80211BE
		if (link_id >= 0 && ctrl_ifname) {
			int ret;
			char buf[300];

			ret = os_snprintf(buf, sizeof(buf), "%s_%s%d",
					  ctrl_ifname, WPA_CTRL_IFACE_LINK_NAME,
					  link_id);
			if (os_snprintf_error(sizeof(buf), ret))
				return -1;

			os_free(ctrl_ifname);
			ctrl_ifname = os_strdup(buf);
			link_id = -1;
		}
#endif /* CONFIG_IEEE80211BE */

		hostapd_cli_reconnect(ctrl_ifname);
		if (ctrl_conn) {
			if (warning_displayed)
				printf("Connection established.\n");
			break;
		}
		if (!interactive && !reconnect) {
			perror("Failed to connect to hostapd - "
			       "wpa_ctrl_open");
			return -1;
		}

		if (!warning_displayed) {
			printf("Could not connect to hostapd - re-trying\n");
			warning_displayed = 1;
		}
		os_sleep(1, 0);
		continue;
	}

	eloop_register_signal_terminate(hostapd_cli_eloop_terminate, NULL);

	if (action_file && !hostapd_cli_attached)
		return -1;
	if (daemonize && os_daemonize(pid_file) && eloop_sock_requeue())
		return -1;
	if (reconnect && action_file && ctrl_ifname) {
		while (!hostapd_cli_quit) {
			if (ctrl_conn)
				hostapd_cli_action(ctrl_conn);
			os_sleep(1, 0);
			hostapd_cli_reconnect(ctrl_ifname);
		}
	} else if (interactive)
		hostapd_cli_interactive();
	else if (action_file)
		hostapd_cli_action(ctrl_conn);
	else
		wpa_request(ctrl_conn, argc - optind, &argv[optind]);

	unregister_event_handler(ctrl_conn);
	os_free(ctrl_ifname);
	eloop_destroy();
	hostapd_cli_cleanup();
	return 0;
}

#else /* CONFIG_NO_CTRL_IFACE */

int main(int argc, char *argv[])
{
	return -1;
}

#endif /* CONFIG_NO_CTRL_IFACE */
