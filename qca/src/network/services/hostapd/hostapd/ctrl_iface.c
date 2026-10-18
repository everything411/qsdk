/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#ifdef CONFIG_TESTING_OPTIONS
#ifdef __NetBSD__
#include <net/if_ether.h>
#else
#include <net/ethernet.h>
#endif
#include <netinet/ip.h>
#endif /* CONFIG_TESTING_OPTIONS */

#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>

#ifdef CONFIG_CTRL_IFACE_UDP
#include <netdb.h>
#endif /* CONFIG_CTRL_IFACE_UDP */

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/module_tests.h"
#include "utils/trace.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#include "common/ctrl_iface_common.h"
#include "common/ieee802_11_common.h"
#ifdef CONFIG_DPP
#include "common/dpp.h"
#endif /* CONFIG_DPP */
#include "common/wpa_ctrl.h"
#include "common/ptksa_cache.h"
#include "common/hw_features_common.h"
#include "common/nan_de.h"
#include "crypto/tls.h"
#include "drivers/driver.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "radius/radius_client.h"
#include "radius/radius_server.h"
#include "l2_packet/l2_packet.h"
#include "ap/hostapd.h"
#include "ap/hw_features.h"
#include "ap/hostapd_log.h"
#include "ap/ap_config.h"
#include "ap/ieee802_1x.h"
#include "ap/wpa_auth.h"
#include "ap/pmksa_cache_auth.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "ap/wps_hostapd.h"
#include "ap/ctrl_iface_ap.h"
#include "ap/ap_drv_ops.h"
#include "ap/hs20.h"
#include "ap/wnm_ap.h"
#include "ap/wpa_auth.h"
#include "ap/beacon.h"
#include "ap/neighbor_db.h"
#include "ap/rrm.h"
#include "ap/dpp_hostapd.h"
#include "ap/dfs.h"
#include "ap/ubus.h"
#include "ap/nan_usd_ap.h"
#include "wps/wps_defs.h"
#include "wps/wps.h"
#include "fst/fst_ctrl_iface.h"
#include "config_file.h"
#include "ctrl_iface.h"
#include "ap/ttlm.h"
#include "../src/drivers/driver_nl80211.h"
#include "ap/dscp_policy.h"
#include "ap/interference.h"
#include <limits.h>

#ifdef CONFIG_IEEE80211BN
#include "ap/mapc.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/mapc_extn.h"
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_ATF_OFFLOAD
#include "atf/atf_offload_config.h"
#endif

#ifdef CONFIG_PROCESS_COORDINATION
#include "common/proc_coord.h"
#ifdef HOSTAPD_EXTERNAL_PLUGIN_TESTAPP
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/hostapd_if_plugin.h"
#endif /* CONFIG_QCN_EXTN */
#endif
#endif

#define HOSTAPD_CLI_DUP_VALUE_MAX_LEN 256

#ifdef CONFIG_CTRL_IFACE_UDP
#define HOSTAPD_CTRL_IFACE_PORT		8877
#define HOSTAPD_CTRL_IFACE_PORT_LIMIT	50
#define HOSTAPD_GLOBAL_CTRL_IFACE_PORT		8878
#define HOSTAPD_GLOBAL_CTRL_IFACE_PORT_LIMIT	50
#endif /* CONFIG_CTRL_IFACE_UDP */

#ifdef CONFIG_IEEE80211BE
#define MIN_ML_RECONF_COUNT 5
#define MAX_ML_RECONF_COUNT 50
#endif /* CONFIG_IEEE80211BE */

#define QOS_MAP_LEN 16

static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    enum wpa_msg_type type,
				    const char *buf, size_t len);


static int hostapd_ctrl_iface_add(struct hapd_interfaces *interfaces,
				  char *buf);


static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_storage *from,
				     socklen_t fromlen, const char *input)
{
	return ctrl_iface_attach(&hapd->ctrl_dst, from, fromlen, input);
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_storage *from,
				     socklen_t fromlen)
{
	return ctrl_iface_detach(&hapd->ctrl_dst, from, fromlen);
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_storage *from,
				    socklen_t fromlen,
				    char *level)
{
	return ctrl_iface_level(&hapd->ctrl_dst, from, fromlen, level);
}


static int hostapd_ctrl_iface_new_sta(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE NEW_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Add new STA " MACSTR " based on ctrl_iface "
		   "notification", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta == NULL)
		return -1;

	hostapd_new_assoc_sta(hapd, sta, 0);
	return 0;
}


#ifdef NEED_AP_MLME
static int hostapd_ctrl_iface_sa_query(struct hostapd_data *hapd,
				       const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	u8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SA_QUERY %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr) ||
	    os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0)
		return -1;

	ieee802_11_send_sa_query_req(hapd, addr, trans_id);

	return 0;
}
#endif /* NEED_AP_MLME */


#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_wps_pin(struct hostapd_data *hapd, char *txt)
{
	char *pin = os_strchr(txt, ' ');
	char *timeout_txt;
	int timeout;
	u8 addr_buf[ETH_ALEN], *addr = NULL;
	char *pos;

	if (pin == NULL)
		return -1;
	*pin++ = '\0';

	timeout_txt = os_strchr(pin, ' ');
	if (timeout_txt) {
		*timeout_txt++ = '\0';
		timeout = atoi(timeout_txt);
		pos = os_strchr(timeout_txt, ' ');
		if (pos) {
			*pos++ = '\0';
			if (hwaddr_aton(pos, addr_buf) == 0)
				addr = addr_buf;
		}
	} else
		timeout = 0;

	return hostapd_wps_add_pin(hapd, addr, txt, pin, timeout);
}


static int hostapd_ctrl_iface_wps_check_pin(
	struct hostapd_data *hapd, char *cmd, char *buf, size_t buflen)
{
	char pin[9];
	size_t len;
	char *pos;
	int ret;

	wpa_hexdump_ascii_key(MSG_DEBUG, "WPS_CHECK_PIN",
			      (u8 *) cmd, os_strlen(cmd));
	for (pos = cmd, len = 0; *pos != '\0'; pos++) {
		if (*pos < '0' || *pos > '9')
			continue;
		pin[len++] = *pos;
		if (len == 9) {
			wpa_printf(MSG_DEBUG, "WPS: Too long PIN");
			return -1;
		}
	}
	if (len != 4 && len != 8) {
		wpa_printf(MSG_DEBUG, "WPS: Invalid PIN length %d", (int) len);
		return -1;
	}
	pin[len] = '\0';

	if (len == 8) {
		unsigned int pin_val;
		pin_val = atoi(pin);
		if (!wps_pin_valid(pin_val)) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid checksum digit");
			ret = os_snprintf(buf, buflen, "FAIL-CHECKSUM\n");
			if (os_snprintf_error(buflen, ret))
				return -1;
			return ret;
		}
	}

	ret = os_snprintf(buf, buflen, "%s", pin);
	if (os_snprintf_error(buflen, ret))
		return -1;

	return ret;
}


#ifdef CONFIG_WPS_NFC
static int hostapd_ctrl_iface_wps_nfc_tag_read(struct hostapd_data *hapd,
					       char *pos)
{
	size_t len;
	struct wpabuf *buf;
	int ret;

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	buf = wpabuf_alloc(len);
	if (buf == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(buf, len), len) < 0) {
		wpabuf_free(buf);
		return -1;
	}

	ret = hostapd_wps_nfc_tag_read(hapd, buf);
	wpabuf_free(buf);

	return ret;
}


static int hostapd_ctrl_iface_wps_nfc_config_token(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	int ndef;
	struct wpabuf *buf;
	int res;

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	buf = hostapd_wps_nfc_config_token(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token_gen(struct hostapd_data *hapd,
						char *reply, size_t max_len,
						int ndef)
{
	struct wpabuf *buf;
	int res;

	buf = hostapd_wps_nfc_token_gen(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token(struct hostapd_data *hapd,
					    char *cmd, char *reply,
					    size_t max_len)
{
	if (os_strcmp(cmd, "WPS") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 0);

	if (os_strcmp(cmd, "NDEF") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 1);

	if (os_strcmp(cmd, "enable") == 0)
		return hostapd_wps_nfc_token_enable(hapd);

	if (os_strcmp(cmd, "disable") == 0) {
		hostapd_wps_nfc_token_disable(hapd);
		return 0;
	}

	return -1;
}


static int hostapd_ctrl_iface_nfc_get_handover_sel(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	struct wpabuf *buf;
	int res;
	char *pos;
	int ndef;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	if (os_strcmp(pos, "WPS-CR") == 0)
		buf = hostapd_wps_nfc_hs_cr(hapd, ndef);
	else
		buf = NULL;
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_nfc_report_handover(struct hostapd_data *hapd,
						  char *cmd)
{
	size_t len;
	struct wpabuf *req, *sel;
	int ret;
	char *pos, *role, *type, *pos2;

	role = cmd;
	pos = os_strchr(role, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	type = pos;
	pos = os_strchr(type, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	pos2 = os_strchr(pos, ' ');
	if (pos2 == NULL)
		return -1;
	*pos2++ = '\0';

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	req = wpabuf_alloc(len);
	if (req == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(req, len), len) < 0) {
		wpabuf_free(req);
		return -1;
	}

	len = os_strlen(pos2);
	if (len & 0x01) {
		wpabuf_free(req);
		return -1;
	}
	len /= 2;

	sel = wpabuf_alloc(len);
	if (sel == NULL) {
		wpabuf_free(req);
		return -1;
	}
	if (hexstr2bin(pos2, wpabuf_put(sel, len), len) < 0) {
		wpabuf_free(req);
		wpabuf_free(sel);
		return -1;
	}

	if (os_strcmp(role, "RESP") == 0 && os_strcmp(type, "WPS") == 0) {
		ret = hostapd_wps_nfc_report_handover(hapd, req, sel);
	} else {
		wpa_printf(MSG_DEBUG, "NFC: Unsupported connection handover "
			   "reported: role=%s type=%s", role, type);
		ret = -1;
	}
	wpabuf_free(req);
	wpabuf_free(sel);

	return ret;
}

#endif /* CONFIG_WPS_NFC */


static int hostapd_ctrl_iface_wps_ap_pin(struct hostapd_data *hapd, char *txt,
					 char *buf, size_t buflen)
{
	int timeout = 300;
	char *pos;
	const char *pin_txt;

	pos = os_strchr(txt, ' ');
	if (pos)
		*pos++ = '\0';

	if (os_strcmp(txt, "disable") == 0) {
		hostapd_wps_ap_pin_disable(hapd);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "random") == 0) {
		if (pos)
			timeout = atoi(pos);
		pin_txt = hostapd_wps_ap_pin_random(hapd, timeout);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "get") == 0) {
		pin_txt = hostapd_wps_ap_pin_get(hapd);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "set") == 0) {
		char *pin;
		if (pos == NULL)
			return -1;
		pin = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			timeout = atoi(pos);
		}
		if (os_strlen(pin) > buflen)
			return -1;
		if (hostapd_wps_ap_pin_set(hapd, pin, timeout) < 0)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin);
	}

	return -1;
}


static int hostapd_ctrl_iface_wps_config(struct hostapd_data *hapd, char *txt)
{
	char *pos;
	char *ssid, *auth, *encr = NULL, *key = NULL;

	ssid = txt;
	pos = os_strchr(txt, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	auth = pos;
	pos = os_strchr(pos, ' ');
	if (pos) {
		*pos++ = '\0';
		encr = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			key = pos;
		}
	}

	return hostapd_wps_config_ap(hapd, ssid, auth, encr, key);
}


static const char * pbc_status_str(enum pbc_status status)
{
	switch (status) {
	case WPS_PBC_STATUS_DISABLE:
		return "Disabled";
	case WPS_PBC_STATUS_ACTIVE:
		return "Active";
	case WPS_PBC_STATUS_TIMEOUT:
		return "Timed-out";
	case WPS_PBC_STATUS_OVERLAP:
		return "Overlap";
	default:
		return "Unknown";
	}
}


static int hostapd_ctrl_iface_wps_get_status(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "PBC Status: %s\n",
			  pbc_status_str(hapd->wps_stats.pbc_status));

	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	ret = os_snprintf(pos, end - pos, "Last WPS result: %s\n",
			  (hapd->wps_stats.status == WPS_STATUS_SUCCESS ?
			   "Success":
			   (hapd->wps_stats.status == WPS_STATUS_FAILURE ?
			    "Failed" : "None")));

	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	/* If status == Failure - Add possible Reasons */
	if(hapd->wps_stats.status == WPS_STATUS_FAILURE &&
	   hapd->wps_stats.failure_reason > 0) {
		ret = os_snprintf(pos, end - pos,
				  "Failure Reason: %s\n",
				  wps_ei_str(hapd->wps_stats.failure_reason));

		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->wps_stats.status) {
		ret = os_snprintf(pos, end - pos, "Peer Address: " MACSTR "\n",
				  MAC2STR(hapd->wps_stats.peer_addr));

		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}

#endif /* CONFIG_WPS */


#ifdef CONFIG_HS20
static int hostapd_ctrl_iface_hs20_deauth_req(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 addr[ETH_ALEN];
	int code, reauth_delay, ret;
	const char *pos;
	size_t url_len;
	struct wpabuf *req;

	/* <STA MAC Addr> <Code(0/1)> <Re-auth-Delay(sec)> [URL] */
	if (hwaddr_aton(cmd, addr))
		return -1;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	code = atoi(pos);

	pos = os_strchr(pos, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	reauth_delay = atoi(pos);

	url_len = 0;
	pos = os_strchr(pos, ' ');
	if (pos) {
		pos++;
		url_len = os_strlen(pos);
	}

	req = wpabuf_alloc(4 + url_len);
	if (req == NULL)
		return -1;
	wpabuf_put_u8(req, code);
	wpabuf_put_le16(req, reauth_delay);
	wpabuf_put_u8(req, url_len);
	if (pos)
		wpabuf_put_data(req, pos, url_len);

	wpa_printf(MSG_DEBUG, "HS 2.0: Send WNM-Notification to " MACSTR
		   " to indicate imminent deauthentication (code=%d "
		   "reauth_delay=%d)", MAC2STR(addr), code, reauth_delay);
	ret = hs20_send_wnm_notification_deauth_req(hapd, addr, req);
	wpabuf_free(req);
	return ret;
}
#endif /* CONFIG_HS20 */


#ifdef CONFIG_INTERWORKING

static int hostapd_ctrl_iface_set_qos_map_set(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 qos_map_set[16 + 2 * 21], count = 0;
	const char *pos = cmd;
	int val, ret;

	for (;;) {
		if (count == sizeof(qos_map_set)) {
			wpa_printf(MSG_ERROR, "Too many qos_map_set parameters");
			return -1;
		}

		val = atoi(pos);
		if (val < 0 || val > 255) {
			wpa_printf(MSG_INFO, "Invalid QoS Map Set");
			return -1;
		}

		qos_map_set[count++] = val;
		pos = os_strchr(pos, ',');
		if (!pos)
			break;
		pos++;
	}

	if (count < 16 || count & 1) {
		wpa_printf(MSG_INFO, "Invalid QoS Map Set");
		return -1;
	}

	ret = hostapd_drv_set_qos_map(hapd, qos_map_set, count);
	if (ret) {
		wpa_printf(MSG_INFO, "Failed to set QoS Map Set");
		return -1;
	}

	os_memcpy(hapd->conf->qos_map_set, qos_map_set, count);
	hapd->conf->qos_map_set_len = count;

	return 0;
}


static int hostapd_ctrl_iface_send_qos_map_conf(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	struct wpabuf *buf;
	u8 *qos_map_set = hapd->conf->qos_map_set;
	u8 qos_map_set_len = hapd->conf->qos_map_set_len;
	int ret;

	if (!qos_map_set_len) {
		wpa_printf(MSG_INFO, "QoS Map Set is not set");
		return -1;
	}

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " not found "
			   "for QoS Map Configuration message",
			   MAC2STR(addr));
		return -1;
	}

	if (!sta->qos_map_enabled) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " did not indicate "
			   "support for QoS Map", MAC2STR(addr));
		return -1;
	}

	buf = wpabuf_alloc(2 + 2 + qos_map_set_len);
	if (buf == NULL)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_QOS);
	wpabuf_put_u8(buf, QOS_QOS_MAP_CONFIG);

	/* QoS Map Set Element */
	wpabuf_put_u8(buf, WLAN_EID_QOS_MAP_SET);
	wpabuf_put_u8(buf, qos_map_set_len);
	wpabuf_put_data(buf, qos_map_set, qos_map_set_len);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);

	return ret;
}


static int hostapd_ctrl_iface_set_bss_priority(struct hostapd_data *hapd,
					       const char *cmd)
{
	const char *pos = cmd;
	int val = atoi(pos);
	int ret;
	static const u8 qos_map_for_bss_priority[][QOS_MAP_LEN] = {
		/* bss_priority = 0: DSCP 0–63 mapped via UP2 =>TID2 others unused */
		{ 255, 255, 255, 255, 0, 63, 255, 255,
		  255, 255, 255, 255, 255, 255, 255, 255 },
		/* bss_priority = 1: DSCP 0–63 mapped via UP0 => TID0 */
		{ 0, 63, 255, 255, 255, 255, 255, 255,
		  255, 255, 255, 255, 255, 255, 255, 255 },
		/* bss_priority = 2: DSCP 0–63 mapped via UP4 = TID4 */
		{ 255, 255, 255, 255, 255, 255, 255, 255,
		  0, 63, 255, 255, 255, 255, 255, 255 },
		/* bss_priority = 3: DSCP 0–63 mapped via UP6 = TID6 */
		{ 255, 255, 255, 255, 255, 255, 255, 255,
		  255, 255, 255, 255, 0, 63, 255, 255 },
	};

	if (val < 0 || val >= WMM_AC_NUM) {
		wpa_printf(MSG_INFO, "invalid value for bss_priority %d", val);
		return -1;
	}

	if (!hapd->conf->bss_priority_status) {
		wpa_printf(MSG_ERROR, "bss_priority_status not set");
		return -1;
	}

	ret = hostapd_drv_set_qos_map(hapd, qos_map_for_bss_priority[val],
				      QOS_MAP_LEN);
	if (ret) {
		wpa_printf(MSG_ERROR, "set_qos_map failed");
		return -1;
	}

	hapd->conf->bss_priority = val;

	wpa_printf(MSG_DEBUG, "BSS priority set to %d", val);

	return 0;
}


static int hostapd_ctrl_iface_set_bss_priority_status(struct hostapd_data *hapd,
						      const char *cmd)
{
	const char *pos = cmd;
	int val = atoi(pos);
	int ret = 0;

	if (val < 0 || val > 1) {
		wpa_printf(MSG_INFO,
			   "invalid value for bss_priority_status %d", val);
		return -1;
	}

	hapd->conf->bss_priority_status = val;

	if (!hapd->conf->bss_priority_status) {
		/* If the feature is disabled, restore configured QoS Map Set if any */
		if (hapd->conf->qos_map_set_len > 0) {
			ret = hostapd_drv_set_qos_map(hapd,
						      hapd->conf->qos_map_set,
						      hapd->conf->qos_map_set_len);
		} else {
			/* No custom QoS map configured, clear any existing mapping */
			ret = hostapd_drv_set_qos_map(hapd, NULL, 0);
		}
	}

	return ret;
}


static int hostapd_ctrl_iface_get_bss_priority(struct hostapd_data *hapd,
					       char *buf, size_t buflen)
{
	int ret;

	ret = os_snprintf(buf, buflen, "%d\n", hapd->conf->bss_priority);

	if (os_snprintf_error(buflen, ret)) {
		wpa_printf(MSG_ERROR,
			   "get_bss_priority: buffer too small (len=%zu)", buflen);
		return -1;
	}

	return ret;
}


static int hostapd_ctrl_iface_get_bss_priority_status(struct hostapd_data *hapd,
		char *buf, size_t buflen)
{
	int ret;

	ret = os_snprintf(buf, buflen, "%d\n", hapd->conf->bss_priority_status);

	if (os_snprintf_error(buflen, ret)) {
		wpa_printf(MSG_ERROR,
			   "get_bss_priority: buffer too small (len=%zu)", buflen);
		return -1;
	}

	return ret;
}


static int hostapd_ctrl_iface_set_dscp_policy(struct hostapd_data *hapd,
					       const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct hostapd_dscp_policy policy;
	struct sta_info *sta, *assoc_sta;
	struct hostapd_data *lhapd, *assoc_hapd = hapd;
	const char *params;
	char *reset_str;

	if (!hapd->conf->enable_dscp_policy_capa)
		return -1;

	if (!cmd || *cmd == '\0')
		return -1;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
#ifdef CONFIG_IEEE80211BE
	/* To find link STA when MLD addr is provided */
#ifdef CONFIG_QCN_EXTN
	if (!sta && hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(lhapd, hapd) {
			sta = ap_get_sta(lhapd, addr);
			if (sta) {
				assoc_hapd = lhapd;
				break;
			}
		}
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	if (!sta) {
		wpa_printf(MSG_DEBUG, "DSCP: STA " MACSTR " not capable", MAC2STR(addr));
		return -1;
	}

	assoc_sta = sta;
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(assoc_hapd, sta))
		assoc_sta = hostapd_ml_get_assoc_sta(assoc_hapd, sta, &lhapd);
#endif /* CONFIG_IEEE80211BE */

	if (!assoc_sta) {
		wpa_printf(MSG_DEBUG,
			   "DSCP: Assoc STA not found for " MACSTR,
			   MAC2STR(addr));
		return -1;
	}

	params = os_strchr(cmd, ' ');
	if (!params || *++params == '\0')
		return -1;

	reset_str = os_strstr(params, "reset=");
	if (reset_str) {
		assoc_sta->dscp_reset = atoi(reset_str + 6);
		wpa_printf(MSG_DEBUG, "DSCP: Reset flag set to %d for STA " MACSTR,
			   assoc_sta->dscp_reset, MAC2STR(addr));
		return 0;
	}

	if (parse_dscp_policy_string(assoc_sta, &policy, params) < 0)
		return -1;

	if (validate_dscp_policy(&policy) < 0)
		return -1;

	if (build_frame_classifier(&policy) < 0)
		return -1;

	if (add_dscp_policy_to_sta(assoc_sta, &policy) < 0) {
		wpa_printf(MSG_WARNING, "DSCP: Failed to add policy for STA " MACSTR,
			   MAC2STR(addr));
		return -1;
	}

	wpa_printf(MSG_INFO, "DSCP: Added policy ID %u to STA " MACSTR,
		   policy.policy_id, MAC2STR(addr));

	return 0;
}

#endif /* CONFIG_INTERWORKING */


static int hostapd_ctrl_send_unsolicited_dscp_req(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	struct hostapd_data *assoc_hapd = hapd;
#ifdef CONFIG_QCN_EXTN
	struct hostapd_data *lhapd;
#endif /* CONFIG_QCN_EXTN */
	u8 addr[ETH_ALEN];
	int reset = 0;
	int policy_ids[10];
	size_t num_policies = 0;
	char *buf, *p, *end;

	if (!hapd->conf->enable_dscp_policy_capa)
		return -1;

	buf = os_strdup(cmd);
	if (!buf)
		return -1;

	p = buf;
	end = os_strchr(p, ' ');
	if (!end || hwaddr_aton(p, addr)) {
		os_free(buf);
		return -1;
	}

	*end = '\0';
	p = end + 1;
	end = os_strchr(p, ' ');
	if (!end || os_strncmp(p, "reset=", 6) != 0) {
		os_free(buf);
		return -1;
	}

	*end = '\0';
	reset = atoi(p + 6);
	p = end + 1;

	if (os_strncmp(p, "policy_id_list=", 15) != 0) {
		os_free(buf);
		return -1;
	}
	p += 15;

	while (*p && num_policies < ARRAY_SIZE(policy_ids)) {
		int val = strtol(p, &end, 10);
		if (p == end)
			break;
		policy_ids[num_policies++] = val;
		if (*end == '_')
			p = end + 1;
		else
			break;
	}
	os_free(buf);

	if (num_policies == 0)
		return -1;

	sta = ap_get_sta(hapd, addr);
#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!sta && hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(lhapd, hapd) {
			sta = ap_get_sta(lhapd, addr);
			if (sta) {
				assoc_hapd = lhapd;
				break;
			}
		}
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	if (!sta) {
		wpa_printf(MSG_DEBUG, "DSCP: STA " MACSTR " not capable", MAC2STR(addr));
		return -1;
	}

	hostapd_send_unsolicited_dscp_policy_request(assoc_hapd, sta, reset,
						     policy_ids, num_policies);
	return 0;
}

static int hostapd_ctrl_iface_dump_mscs_ctxt(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	struct sta_info *sta;
	int ret;
	size_t len = 0;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (!sta->mscs_ctxt)
			continue;

		wpa_printf(MSG_DEBUG, "MSCS:Station " MACSTR "has an"
			   "active MSCS session",
			   MAC2STR(sta->addr));

		ret = os_snprintf(buf + len, buflen - len,
				  "MSCS context params for STA " MACSTR
				  "\n",MAC2STR(sta->addr));

		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				  "bitmap 0x%x limit 0x%x mask 0x%x\n",
				  sta->mscs_ctxt->user_priority_bitmap,
				  sta->mscs_ctxt->user_priority_limit,
				  sta->mscs_ctxt->tclas_mask);

		if (!os_snprintf_error(buflen - len, ret))
			len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				  "====\n");

		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	return len;
}

static int hostapd_ctrl_iface_send_mscs_resp(struct hostapd_data *hapd,
		const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	int ret = -EINVAL;
	const char *pos = cmd;
	int status;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_ERROR, "Station " MACSTR " not found "
				"for sending MSCS resp",
				MAC2STR(addr));
		return -1;
	}
	wpa_printf(MSG_DEBUG, "MSCS:Station " MACSTR " found "
			"for sending MSCS resp",
			MAC2STR(addr));

	if (!sta->mscs_ctxt)
		return -1;

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	if (strncmp(pos, "--status", 8) != 0)
		return -1;
	pos += 9;
	while (*pos == ' ')
		 pos++;
	status = (uint8_t)atoi(pos);
	if (status > WLAN_STATUS_TCLAS_PROCESSING_TERMINATED)
		return -1;

	if (hostapd_send_mscs_response(hapd, sta, sta->addr, 0, status))
		return -1;

	if (status == WLAN_STATUS_TCLAS_PROCESSING_TERMINATED) {
		ret = hostapd_copy_and_send_mscs_data(hapd, sta, QM_REMOVE_REQ,
						      0);
		sta->mscs_session_exists = false;
		hostapd_mscs_delete_nft_rules(hapd, sta);
		os_free(sta->mscs_ctxt);
		sta->mscs_ctxt = NULL;
	}
	return ret;
}

#ifdef CONFIG_WNM_AP

static int hostapd_ctrl_iface_coloc_intf_req(struct hostapd_data *hapd,
					     const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	unsigned int auto_report, timeout;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_DEBUG, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for Collocated Interference Request",
			   MAC2STR(addr));
		return -1;
	}

	pos = cmd + 17;
	if (*pos != ' ')
		return -1;
	pos++;
	auto_report = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	timeout = atoi(pos);

	return wnm_send_coloc_intf_req(hapd, sta, auto_report, timeout);
}

#endif /* CONFIG_WNM_AP */

static int hostapd_ctrl_iface_update_assocresp_elements(
	struct hostapd_data *hapd, const char *value)
{
	struct wpabuf *new_elems;

	if (!value)
		return -1;

	new_elems = wpabuf_parse_bin(value);
	if (!new_elems)
		return -1;

	wpabuf_free(hapd->conf->assocresp_elements);
	hapd->conf->assocresp_elements = new_elems;

	return 0;
}

static int hostapd_ctrl_iface_get_key_mgmt(struct hostapd_data *hapd,
					   char *buf, size_t buflen)
{
	int ret = 0;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	WPA_ASSERT(hapd->conf->wpa_key_mgmt);

	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		ret = os_snprintf(pos, end - pos, "WPA-PSK ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_PSK) {
		ret = os_snprintf(pos, end - pos, "FT-PSK ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) {
		ret = os_snprintf(pos, end - pos, "FT-EAP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_SHA384
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384) {
		ret = os_snprintf(pos, end - pos, "FT-EAP-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SAE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_SAE) {
		ret = os_snprintf(pos, end - pos, "FT-SAE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		ret = os_snprintf(pos, end - pos, "FT-SAE-EXT-KEY ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) {
		ret = os_snprintf(pos, end - pos, "FT-FILS-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) {
		ret = os_snprintf(pos, end - pos, "FT-FILS-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_FILS */
#endif /* CONFIG_IEEE80211R_AP */
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK_SHA256) {
		ret = os_snprintf(pos, end - pos, "WPA-PSK-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_SAE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_SAE) {
		ret = os_snprintf(pos, end - pos, "SAE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_SAE_EXT_KEY) {
		ret = os_snprintf(pos, end - pos, "SAE-EXT-KEY ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SAE */
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SUITE-B ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt &
	    WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		ret = os_snprintf(pos, end - pos,
				  "WPA-EAP-SUITE-B-192 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_FILS
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA256) {
		ret = os_snprintf(pos, end - pos, "FILS-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA384) {
		ret = os_snprintf(pos, end - pos, "FILS-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) {
		ret = os_snprintf(pos, end - pos, "OWE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) {
		ret = os_snprintf(pos, end - pos, "DPP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_DPP */
#ifdef CONFIG_SHA384
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA384) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SHA384 */

	if (pos > buf && *(pos - 1) == ' ') {
		*(pos - 1) = '\0';
		pos--;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_get_config(struct hostapd_data *hapd,
					 char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n"
			  "ssid=%s\n",
			  MAC2STR(hapd->own_addr),
			  wpa_ssid_txt(hapd->conf->ssid.ssid,
				       hapd->conf->ssid.ssid_len));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if ((hapd->conf->config_id)) {
		ret = os_snprintf(pos, end - pos, "config_id=%s\n",
				  hapd->conf->config_id);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

#ifdef CONFIG_WPS
	ret = os_snprintf(pos, end - pos, "wps_state=%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_passphrase) {
		ret = os_snprintf(pos, end - pos, "passphrase=%s\n",
				  hapd->conf->ssid.wpa_passphrase);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_psk &&
	    hapd->conf->ssid.wpa_psk->group) {
		char hex[PMK_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
		ret = os_snprintf(pos, end - pos, "psk=%s\n", hex);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->multi_ap) {
		struct hostapd_ssid *ssid = &hapd->conf->multi_ap_backhaul_ssid;

		ret = os_snprintf(pos, end - pos, "multi_ap=%d\n",
				  hapd->conf->multi_ap);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		if (ssid->ssid_len) {
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_ssid=%s\n",
					  wpa_ssid_txt(ssid->ssid,
						       ssid->ssid_len));
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}

		if (hapd->conf->wps_state && hapd->conf->wpa &&
			ssid->wpa_passphrase) {
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_wpa_passphrase=%s\n",
					  ssid->wpa_passphrase);
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}

		if (hapd->conf->wps_state && hapd->conf->wpa &&
		    ssid->wpa_psk &&
		    ssid->wpa_psk->group) {
			char hex[PMK_LEN * 2 + 1];

			wpa_snprintf_hex(hex, sizeof(hex), ssid->wpa_psk->psk,
					 PMK_LEN);
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_wpa_psk=%s\n",
					  hex);
			forced_memzero(hex, sizeof(hex));
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}
	}
#endif /* CONFIG_WPS */

	if (hapd->conf->wpa) {
		ret = os_snprintf(pos, end - pos, "wpa=%d\n", hapd->conf->wpa);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa && hapd->conf->wpa_key_mgmt) {
		ret = os_snprintf(pos, end - pos, "key_mgmt=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		pos += hostapd_ctrl_iface_get_key_mgmt(hapd, pos, end - pos);

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa) {
		ret = os_snprintf(pos, end - pos, "group_cipher=%s\n",
				  wpa_cipher_txt(hapd->conf->wpa_group));
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->rsn_pairwise) {
		ret = os_snprintf(pos, end - pos, "rsn_pairwise_cipher=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->rsn_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_WPA) && hapd->conf->wpa_pairwise) {
		ret = os_snprintf(pos, end - pos, "wpa_pairwise_cipher=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->wpa_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa && hapd->conf->wpa_deny_ptk0_rekey) {
		ret = os_snprintf(pos, end - pos, "wpa_deny_ptk0_rekey=%d\n",
				  hapd->conf->wpa_deny_ptk0_rekey);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->extended_key_id) {
		ret = os_snprintf(pos, end - pos, "extended_key_id=%d\n",
				  hapd->conf->extended_key_id);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && hapd->conf->enable_aal) {
		ret = os_snprintf(pos, end - pos, "ml_max_rec_links=%d\n",
				  hapd->conf->ml_max_rec_links);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */

	return pos - buf;
}


/* LOG_PEER restricts logging to a single STA at a time. Only one filter
 * address is stored; issuing LOG_PEER <new_addr> replaces the previous one.
 * LOG_PEER clear removes the restriction and restores per-BSS logging. */
static int hostapd_ctrl_iface_log_peer(struct hostapd_data *hapd,
				       const char *cmd)
{
	u8 addr[ETH_ALEN];

	if (os_strcmp(cmd, "clear") == 0) {
		hapd->log_peer_filter_set = 0;
		os_memset(hapd->log_peer_addr, 0, ETH_ALEN);
#ifdef CONFIG_QCN_EXTN
		hostapd_log_trigger_clear(hapd, NULL);
#endif /* CONFIG_QCN_EXTN */
		return 0;
	}
	if (hwaddr_aton(cmd, addr) < 0)
		return -1;
	if (is_multicast_ether_addr(addr))
		return -1;
	hapd->log_peer_filter_set = 1;
	os_memcpy(hapd->log_peer_addr, addr, ETH_ALEN);
	return 0;
}


static int hostapd_ctrl_iface_set_band(struct hostapd_data *hapd,
				       const char *bands)
{
	union wpa_event_data event;
	u32 setband_mask = WPA_SETBAND_AUTO;

	/*
	 * For example:
	 *  SET setband 2G,6G
	 *  SET setband 5G
	 *  SET setband AUTO
	 */
	if (!os_strstr(bands, "AUTO")) {
		if (os_strstr(bands, "5G"))
			setband_mask |= WPA_SETBAND_5G;
		if (os_strstr(bands, "6G"))
			setband_mask |= WPA_SETBAND_6G;
		if (os_strstr(bands, "2G"))
			setband_mask |= WPA_SETBAND_2G;
		if (setband_mask == WPA_SETBAND_AUTO)
			return -1;
	}

	if (hostapd_drv_set_band(hapd, setband_mask) == 0) {
		os_memset(&event, 0, sizeof(event));
		event.channel_list_changed.initiator = REGDOM_SET_BY_USER;
		event.channel_list_changed.type = REGDOM_TYPE_UNKNOWN;
		wpa_supplicant_event(hapd, EVENT_CHANNEL_LIST_CHANGED, &event);
	}

	return 0;
}


/**
 * hostapd_ctrl_iface_set_punc_strict - Set the puncture strict config
 * @iface: Pointer to hostapd interface data
 * @value: Pointer to the string containing the value to set
 *
 * Return: 0 on success, -1 on failure
 */
static int hostapd_ctrl_iface_set_punc_strict(struct hostapd_iface *iface,
					      char *value)
{
#ifdef NEED_AP_MLME
	int puncture_strict_config;
	char *end;

	puncture_strict_config = strtol(value, &end, 10);
	if (value == end || (puncture_strict_config != 0 &&
			   puncture_strict_config != 1)) {
		wpa_printf(MSG_ERROR, "Invalid value for puncture strict config");
		return -1;
	}

	if (!is_6ghz_freq(iface->freq)) {
		wpa_printf(MSG_ERROR, "set_punc_strict is valid only for 6 GHz");
		return -1;
	}

	if (iface->conf->puncture_strict_6ghz == puncture_strict_config) {
		wpa_printf(MSG_DEBUG,
			   "Puncture strict config already set to %d",
			   puncture_strict_config);
		return -1;
	}

	iface->conf->puncture_strict_6ghz = puncture_strict_config;
	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME  */
}


/**
 * hostapd_ctrl_iface_set_punc_thres - Set the puncture threshold
 * @iface: Pointer to hostapd interface data
 * @value: Pointer to the string containing the value to set
 *
 * Return: 0 on success, -1 on failure
 */
static int hostapd_ctrl_iface_set_punc_thres(struct hostapd_iface *iface,
					     char *value)
{
#ifdef NEED_AP_MLME
	char *end;
	int puncture_eirp_threshold;

	puncture_eirp_threshold = strtol(value, &end, 10);
	if (value == end || puncture_eirp_threshold < CHAN_MIN_TX_POWER ||
	    puncture_eirp_threshold > MAX_EIRP_THRESHOLD) {
		wpa_printf(MSG_ERROR, "Invalid puncture threshold: %s. Valid range: %d to %d dBm",
			   value, CHAN_MIN_TX_POWER, MAX_EIRP_THRESHOLD);
		return -1;
	}

	if (!is_6ghz_freq(iface->freq)) {
		wpa_printf(MSG_ERROR, "set_punc_threshold is valid only for 6 GHz");
		return -1;
	}

	if (iface->conf->punc_eirp_thres_6ghz == puncture_eirp_threshold) {
		wpa_printf(MSG_DEBUG,
			   "Puncture threshold already set to %d",
			   puncture_eirp_threshold);
		return -1;
	}

	iface->conf->punc_eirp_thres_6ghz = puncture_eirp_threshold;
	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME  */
}

static int hostapd_ctrl_iface_update_rssi_monitor(struct hostapd_data *hapd)
{
	int threshold, hysteresis, link_id;
	int ret;

	if (!hapd->started || !hapd->drv_priv || !hapd->driver ||
	    !hapd->driver->signal_monitor)
		return 0;

	threshold = hapd->conf->rssi_reject_assoc_rssi;
	hysteresis = hapd->conf->rssi_deauth_grace_samples;
	link_id = hapd->mld_link_id;

	wpa_printf(MSG_INFO, "Updating RSSI monitor: threshold=%d dBm hysteresis=%d link=%d",
		   threshold, hysteresis, link_id);

	ret = hapd->driver->signal_monitor(hapd->drv_priv, threshold, hysteresis, link_id);
	if (ret < 0)
		wpa_printf(MSG_WARNING, "Failed to update RSSI signal monitor, error: %d",
			   ret);

	return ret;
}

#ifdef NEED_AP_MLME
/*
 * hostapd_ctrl_iface_bgcac_status - Get current RCAC channel info
 *
 * Returns RCAC channel, frequency, bandwidth and status (ongoing/done/unavailable).
 * Only uses radar_background parameters — device-width params are not relevant
 * for RCAC which uses the operating bandwidth.
 * For PreCAC (ETSI): Returns channel currently being CACed and status
 * (ongoing/idle).
 */
static int hostapd_ctrl_iface_get_bgcac_status(struct hostapd_data *hapd,
					    char *buf, size_t buflen)
{
	const struct hostapd_iface *iface = hapd->iface;
	const char *status;
	int freq = 0, chan = 0, bw = 0, res;
	u8 chan_num;

	if (!iface->conf->bgcac_en) {
		return os_snprintf(buf, buflen, "bgcac_en=0\n");
	}

	if (iface->radar_background.cac_started) {
		status = "ongoing";
		freq = iface->radar_background.freq;
	} else if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI) {
		status = "idle";
	} else if (iface->radar_background.channel != -1 &&
		   iface->radar_background.freq > 0) {
		status = "done";
		freq = iface->radar_background.freq;
	} else {
		status = "unavailable";
	}

	if (freq > 0) {
		switch (iface->radar_background.chwidth) {
		case CONF_OPER_CHWIDTH_USE_HT:
			bw = iface->radar_background.secondary_channel ? 40 : 20;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			bw = 80;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			bw = 160;
			break;
		default:
			bw = 0;
			break;
		}
		if (ieee80211_freq_to_chan(freq, &chan_num) != NUM_HOSTAPD_MODES)
			chan = chan_num;
	}

	res = os_snprintf(buf, buflen,
			  "mode=%s status=%s channel=%d freq=%d bw=%d\n",
			  iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI ?
			  "PreCAC" : "RCAC", status, chan, freq, bw);
	if (os_snprintf_error(buflen, res))
		return -1;

	return res;
}
#endif /* NEED_AP_MLME */

#ifdef NEED_AP_MLME
/**
 * hostapd_ctrl_iface_set_rcac_freq - Parse and apply SET_RCAC_FREQ arguments
 * @hapd: Pointer to hostapd data
 * @pos: Argument string after "SET_RCAC_FREQ " prefix
 *
 * Format: set_rcac_freq channel <chan> [<bw>]
 * Returns: 0 on success, -1 on failure
 *
 * Validates that the requested BW is either the same as the home channel BW
 * or exactly half of it (e.g., home=80 → RCAC BW must be 80 or 40 MHz).
 */
static int hostapd_ctrl_iface_set_rcac_freq(struct hostapd_data *hapd,
					    const char *pos)
{
	int chan = 0, freq = 0;
	int home_bw;
	int home_center;
	int home_start;
	int home_end;

	chan = atoi(pos);

	if (chan <= 0) {
		wpa_printf(MSG_ERROR,
			   "SET rcac_freq: missing/invalid channel");
		return -1;
	}

	if (chan >= 36 && chan <= 177)
		freq = 5000 + 5 * chan;
	else
		freq = -1;

	if (freq < 0) {
		wpa_printf(MSG_ERROR,
			   "SET rcac_freq: cannot convert channel %d to freq",
			   chan);
		return -1;
	}

	switch (hostapd_get_oper_chwidth(hapd->iface->conf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		home_bw = hapd->iface->conf->secondary_channel ? 40 : 20;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		home_bw = 80;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		home_bw = 160;
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		home_bw = 320;
		break;
	default:
		home_bw = 80;
		break;
	}

	home_center = (hostapd_get_oper_centr_freq_seg0_idx(hapd->iface->conf) * 5) + 5000;
	home_start = home_center - home_bw / 2 + 10;
	home_end = home_center + home_bw / 2 - 10;

	wpa_printf(MSG_DEBUG,
		   "SET rcac_freq: overlap check freq=%d home_start=%d home_end=%d",
		   freq, home_start, home_end);

	if (freq >= home_start && freq <= home_end) {
		wpa_printf(MSG_ERROR,
			   "SET rcac_freq: channel %d overlaps with home channel block - rejected",
			   chan);
		return -1;
	}

	if (!hapd->iface->conf->enable_background_radar) {
		wpa_printf(MSG_ERROR,
			   "SET rcac_freq: background radar not enabled (enable_background_radar=0)");
		return -1;
	}

	wpa_printf(MSG_INFO, "RCAC config: channel=%d freq=%d MHz bw=%d (home BW)",
		   chan, freq, home_bw);

	hapd->iface->user_rcac_channel = chan;

	if (hapd->iface->radar_background.cac_started) {
		hostapd_abort_background_cac(hapd->iface);
		wpa_printf(MSG_INFO,
			   "SET rcac_freq: aborted RCAC on old channel; starting immediately on channel %d (%d MHz)",
			   chan, home_bw);
	}

	if (hostapd_start_rcac_on_channel(hapd->iface, chan, home_bw) < 0) {
		wpa_printf(MSG_ERROR,
			   "SET rcac_freq: failed to start RCAC on channel %d",
			   chan);
		hapd->iface->user_rcac_channel = 0;
		return -1;
	}
	return 0;
}
#endif /* NEED_AP_MLME */
static int hostapd_set_bw_reduce_en(struct hostapd_data *hapd, const char *value)
{
	int val;
	char *end = NULL;

	val = strtol(value, &end, 10);
	if (value == end || (val != 0 && val != 1)) {
		wpa_printf(MSG_ERROR,
			   "Invalid bw_reduce_en value: %s (use 0 or 1)",
			   value);
		return -1;
	}
	hapd->iface->conf->dfs_bw_reduce_en = val;
	wpa_printf(MSG_INFO, "DFS Bandwidth Reduction %s",
		   val ? "enabled" : "disabled");
	return 0;
}


/**
 * hostapd_ctrl_iface_set_post_nol_freq - Set the post_nol frequency
 * @hapd: Pointer to hostapd BSS data
 * @value: Frequency in MHz
 *
 * Parse and apply the user provided value for post_nol_freq. Rejected on
 * any non-5 GHz link.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_set_post_nol_freq(struct hostapd_data *hapd,
						const char *value)
{
	int val;
	char *end = NULL;

	if (!is_5ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR,
			   "post_nol_freq is only applicable to the 5 GHz link");
		return -1;
	}

	val = strtol(value, &end, 10);
	if (value == end) {
		wpa_printf(MSG_ERROR, "Invalid post_nol_freq value: %s", value);
		return -1;
	}

	if (val != -1 &&
	    (!hapd->iface->current_mode ||
	     !hw_mode_get_channel(hapd->iface->current_mode, val, NULL)))
		return -1;

	hapd->iface->conf->post_nol_freq = val;
	return 0;
}


/**
 * hostapd_ctrl_iface_set_post_nol_width - Set the post_nol bandwidth
 * @hapd: Pointer to hostapd BSS data
 * @value: Bandwidth in MHz,
 *
 * Parse and apply the user provided value for post_nol_width. Rejected on
 * any non-5 GHz link request.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_set_post_nol_width(struct hostapd_data *hapd,
						 const char *value)
{
	int val;
	char *end = NULL;

	if (!is_5ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR,
			   "post_nol_width is only applicable to the 5 GHz link");
		return -1;
	}

	val = strtol(value, &end, 10);
	if (value == end) {
		wpa_printf(MSG_ERROR, "Invalid post_nol_width value: %s", value);
		return -1;
	}

	hapd->iface->conf->post_nol_width = val;
	return 0;
}


/**
 * hostapd_ctrl_iface_set_post_nol_bgcac_en - Enable/disable Agile CAC for
 * post_nol channel switch
 * @hapd: Pointer to hostapd BSS data
 * @value: "0" to disable, "1" to enable
 *
 * Return: 0 on success, -1 on failure.
 */
static int
hostapd_ctrl_iface_set_post_nol_bgcac_en(struct hostapd_data *hapd,
					    const char *value)
{
	int val;
	char *end = NULL;

	val = strtol(value, &end, 10);
	if (value == end || *end != '\0' || (val != 0 && val != 1)) {
		wpa_printf(MSG_ERROR,
			   "Invalid post_nol_bgcac_en value: %s (use 0 or 1)",
			   value);
		return -1;
	}

	hapd->iface->conf->post_nol_bgcac_en = val;
	return 0;
}


/**
 * hostapd_ctrl_iface_set_next_radar_freq - Set the next-radar frequency
 * @hapd: Pointer to hostapd BSS data
 * @value: Frequency in MHz
 *
 * Parse and apply the user provided value for next_radar_freq.
 * Rejected on any non-5 GHz link, and if @value does not resolve to a
 * channel known in the current mode.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_set_next_radar_freq(struct hostapd_data *hapd,
						   const char *value)
{
	int val;
	char *end = NULL;

	if (!is_5ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR,
			   "next_radar_freq is only applicable to the 5 GHz link");
		return -1;
	}

	val = strtol(value, &end, 10);
	if (value == end) {
		wpa_printf(MSG_ERROR, "Invalid next_radar_freq value: %s",
			   value);
		return -1;
	}

	if (val != -1 &&
	    (!hapd->iface->current_mode ||
	     !hw_mode_get_channel(hapd->iface->current_mode, val, NULL)))
		return -1;

	hapd->iface->conf->next_radar_freq = val;
	return 0;
}


/**
 * hostapd_ctrl_iface_set_next_radar_width - Set the target next-radar
 * bandwidth
 * @hapd: Pointer to hostapd BSS data
 * @value: Bandwidth in MHz
 *
 * Parse and apply the user provided value for next_radar_width.
 * Rejected on any non-5 GHz link request.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_set_next_radar_width(struct hostapd_data *hapd,
						    const char *value)
{
	int val;
	char *end = NULL;

	if (!is_5ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR,
			   "next_radar_width is only applicable to the 5 GHz link");
		return -1;
	}

	val = strtol(value, &end, 10);
	if (value == end) {
		wpa_printf(MSG_ERROR, "Invalid next_radar_width value: %s",
			   value);
		return -1;
	}

	hapd->iface->conf->next_radar_width = val;
	return 0;
}


static int hostapd_ctrl_iface_set(struct hostapd_data *hapd, char *cmd)
{
	char *value;
	int ret = 0;
	struct hostapd_data *tx_hapd = hostapd_mbssid_get_tx_bss(hapd);

	value = os_strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (0) {
#ifdef CONFIG_WPS_TESTING
	} else if (os_strcasecmp(cmd, "wps_version_number") == 0) {
		long int val;
		val = strtol(value, NULL, 0);
		if (val < 0 || val > 0xff) {
			ret = -1;
			wpa_printf(MSG_DEBUG, "WPS: Invalid "
				   "wps_version_number %ld", val);
		} else {
			wps_version_number = val;
			wpa_printf(MSG_DEBUG, "WPS: Testing - force WPS "
				   "version %u.%u",
				   (wps_version_number & 0xf0) >> 4,
				   wps_version_number & 0x0f);
			hostapd_wps_update_ie(hapd);
		}
	} else if (os_strcasecmp(cmd, "wps_testing_stub_cred") == 0) {
		wps_testing_stub_cred = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - stub_cred=%d",
			   wps_testing_stub_cred);
	} else if (os_strcasecmp(cmd, "wps_corrupt_pkhash") == 0) {
		wps_corrupt_pkhash = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - wps_corrupt_pkhash=%d",
			   wps_corrupt_pkhash);
#endif /* CONFIG_WPS_TESTING */
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strcasecmp(cmd, "ext_mgmt_frame_handling") == 0) {
		hapd->ext_mgmt_frame_handling = atoi(value);
	} else if (os_strcasecmp(cmd, "ext_eapol_frame_io") == 0) {
		hapd->ext_eapol_frame_io = atoi(value);
	} else if (os_strcasecmp(cmd, "force_backlog_bytes") == 0) {
		hapd->force_backlog_bytes = atoi(value);
#ifdef CONFIG_DPP
	} else if (os_strcasecmp(cmd, "dpp_config_obj_override") == 0) {
		os_free(hapd->dpp_config_obj_override);
		hapd->dpp_config_obj_override = os_strdup(value);
	} else if (os_strcasecmp(cmd, "dpp_discovery_override") == 0) {
		os_free(hapd->dpp_discovery_override);
		hapd->dpp_discovery_override = os_strdup(value);
	} else if (os_strcasecmp(cmd, "dpp_groups_override") == 0) {
		os_free(hapd->dpp_groups_override);
		hapd->dpp_groups_override = os_strdup(value);
	} else if (os_strcasecmp(cmd,
				 "dpp_ignore_netaccesskey_mismatch") == 0) {
		hapd->dpp_ignore_netaccesskey_mismatch = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_test") == 0) {
		dpp_test = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_version_override") == 0) {
		dpp_version_override = atoi(value);
#endif /* CONFIG_DPP */
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_MBO
	} else if (os_strcasecmp(cmd, "mbo_assoc_disallow") == 0) {
		int val;

		if (!hapd->conf->mbo_enabled)
			return -1;

		val = atoi(value);
		if (val < 0 || val > MBO_ASSOC_DISALLOW_REASON_LOW_RSSI)
			return -1;

		hapd->mbo_assoc_disallow = val;
		ieee802_11_update_beacons(hapd->iface);

		/*
		 * TODO: Need to configure drivers that do AP MLME offload with
		 * disallowing station logic.
		 */
	} else if (os_strcmp(cmd, "mbo_trans_reason") == 0) {
		int val;

		if (!hapd->conf->mbo_enabled)
			return -1;

		val = atoi(value);
		if (val < 0 || val > MBO_TRANSITION_REASON_MAX) {
			wpa_printf(MSG_ERROR, "Invalid mbo_trans_reason %d (expected 0..%d)", val, MBO_TRANSITION_REASON_MAX);
			return -1;
		}

		hapd->mbo_trans_reason = (u8)val;
	} else if (os_strcmp(cmd, "mbo_assoc_retry") == 0) {
		int val;

		if (!hapd->conf->mbo_enabled)
			return -1;

		val = atoi(value);
		if (val < 0 || val > 65535) {
			wpa_printf(MSG_ERROR, "Invalid mbo_assoc_retry %d (expected 0..65535)", val);
			return -1;
		}

		hapd->mbo_assoc_retry = (u16)val;
#endif /* CONFIG_MBO */
#ifdef CONFIG_DPP
	} else if (os_strcasecmp(cmd, "dpp_configurator_params") == 0) {
		ret = dpp_global_configurations_remove(hapd->iface->interfaces->dpp);
		if (!ret)
			ret = dpp_global_configurations_add(hapd->iface->interfaces->dpp, value);
#ifdef CONFIG_DPP2
		dpp_controller_set_params(hapd->iface->interfaces->dpp, value);
#endif /* CONFIG_DPP2 */
	} else if (os_strcasecmp(cmd, "dpp_wps") == 0) {
		hapd->dpp_wps = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_init_max_tries") == 0) {
		hapd->dpp_init_max_tries = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_init_retry_time") == 0) {
		hapd->dpp_init_retry_time = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_wait_time") == 0) {
		hapd->dpp_resp_wait_time = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_max_tries") == 0) {
		hapd->dpp_resp_max_tries = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_retry_time") == 0) {
		hapd->dpp_resp_retry_time = atoi(value);
#endif /* CONFIG_DPP */
#ifdef NEED_AP_MLME
	} else if (os_strcasecmp(cmd, "bgcac_en") == 0) {
		int val;
		char *end = NULL;

		val = strtol(value, &end, 10);
		if (value == end || (val != 0 && val != 1)) {
			wpa_printf(MSG_ERROR, "Invalid bgcac_en value: %s (use 0 or 1)", value);
			return -1;
		}
		hapd->iface->conf->bgcac_en = val;
		/* bgcac_en requires enable_background_radar as prerequisite */
		if (val)
			hapd->iface->conf->enable_background_radar = 1;
		wpa_printf(MSG_INFO, "Background CAC %s",
			   val ? "enabled" : "disabled");
		if (!val) {
			hapd->iface->conf->enable_background_radar = 0;
			hostapd_abort_background_cac(hapd->iface);
			hostapd_cancel_agile_cac_restart(hapd->iface);
			hapd->iface->radar_background.channel = -1;
			hapd->iface->radar_background.cac_started = 0;
			hapd->iface->user_rcac_channel = 0;
			wpa_printf(MSG_INFO, "DFS: Agile CAC disabled - background CAC abort CSA issued");
		}
	} else if (os_strcasecmp(cmd, "rcac_freq") == 0) {
		if (hostapd_ctrl_iface_set_rcac_freq(hapd, value) < 0)
			return -1;
		return 0;
#endif /* NEED_AP_MLME */
	} else if (os_strcasecmp(cmd, "setband") == 0) {
		ret = hostapd_ctrl_iface_set_band(hapd, value);
	} else if (os_strcasecmp(cmd, "puncture_strict_6ghz") == 0) {
		ret = hostapd_ctrl_iface_set_punc_strict(hapd->iface, value);
	} else if (os_strcasecmp(cmd, "punc_eirp_thres_6ghz") == 0) {
		ret = hostapd_ctrl_iface_set_punc_thres(hapd->iface, value);
	} else if (os_strncmp(cmd, "vendor_elements_", 16) == 0) {
		ret = hostapd_handle_vendor_elements_update(hapd, hapd->conf, NULL,
							    cmd, value, true);
	} else if (os_strcasecmp(cmd, "assocresp_elements") == 0) {
		ret = hostapd_ctrl_iface_update_assocresp_elements(hapd, value);
	} else if (os_strcasecmp(cmd, "rssi_deauth_grace_samples") == 0) {
		int val = atoi(value);
		if (val < 1 || val > 100) {
			wpa_printf(MSG_ERROR, "Invalid grace samples %d (range: 1 to 100)", val);
			ret = -1;
		} else {
			hapd->conf->rssi_deauth_grace_samples = val;
			hapd->iconf->rssi_deauth_grace_samples = val;
			wpa_printf(MSG_INFO, "Updated RSSI deauth grace samples to %d", val);
			hostapd_ctrl_iface_update_rssi_monitor(hapd);
		}
#ifdef CONFIG_FILS
	} else if (os_strcasecmp(cmd, "fils_discovery_max_interval") == 0) {
		int val = atoi(value);

		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			wpa_printf(MSG_ERROR,
				   "Fils are not supported in current band!");
			hapd->conf->fils_discovery_max_int = 0;
			return -1;
		}

		if (hapd != tx_hapd) {
			wpa_printf(MSG_ERROR,
				   "Fils are allowed to enable only on Tx BSS");
			hapd->conf->fils_discovery_max_int = 0;
			return -1;
		}

		if (val < 0 || val > FD_MAX_INTERVAL_6GHZ) {
			wpa_printf(MSG_ERROR,
				   "Invalid fils_discovery_max_int value");
			return -1;
		}

		/* To force enable fils set force_disable_in_band_discovery to 0 */
		hapd->conf->fils_discovery_max_int = val;

		if (hapd->conf->fils_discovery_max_int) {
			/* Disable UBPR if its enabled during fils enablement */
			if (hapd->conf->ubpr_state == FILS_UBPR_ENABLED)
				hapd->conf->unsol_bcast_probe_resp_interval = 0;
		}

		/* Fils can be enabled even when there are lower band BSSs present */
		ieee802_11_set_beacon_per_bss_only(hapd);

		return ret;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211AX
	} else if (os_strcasecmp(cmd, "unsol_bcast_probe_resp_interval") == 0) {
		int val = atoi(value);

		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			wpa_printf(MSG_ERROR,
				   "UBPR is not supported in current band!");
			hapd->conf->unsol_bcast_probe_resp_interval = 0;
			return -1;
		}

		if (hapd != tx_hapd) {
			wpa_printf(MSG_ERROR,
				   "UBPRs are allowed to enable only on Tx BSS");
			hapd->conf->unsol_bcast_probe_resp_interval = 0;
			return -1;
		}

		if (val < 0 || val > 20) {
			wpa_printf(MSG_ERROR,
				   "Invalid  unsol_bcast_probe_resp_interval value");
			return -1;
		}

		hapd->conf->unsol_bcast_probe_resp_interval = val;

		/* To force enable UBPR set force_disable_in_band_discovery to 0 */
		if (hapd->conf->unsol_bcast_probe_resp_interval) {
			/* Disable fils if its enabled during UBPR enablement */
#ifdef CONFIG_FILS
			if (hapd->conf->fils_state == FILS_UBPR_ENABLED)
				hapd->conf->fils_discovery_max_int = 0;
#endif /* CONFIG_FILS */
		} else {
#ifdef CONFIG_FILS
			if (hapd->conf->fils_state != FILS_UBPR_ENABLED)
				hapd->conf->fils_discovery_max_int = FD_MAX_INTERVAL_6GHZ;
#endif /* CONFIG_FILS */
		}

		ieee802_11_set_beacon_per_bss_only(tx_hapd);

		return ret;

	} else if (os_strcasecmp(cmd, "he_6ghz_min_rate") == 0) {
		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			wpa_printf(MSG_ERROR,
				   "he_6ghz_min_rate is applicable for 6 GHz only");
			return -1;
		}
		ret = hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value);
		if (ret)
			return ret;
		if (!hapd->conf->is_cmn_param)
			ieee802_11_update_beacons(tx_hapd->iface);
		return 0;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211AC
	} else if (os_strcasecmp(cmd, "bss_vht_mu_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_vht_su_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_vht_su_beamformee") == 0 ||
		   os_strcasecmp(cmd, "bss_vht_sounding_dimension") == 0 ||
		   os_strcasecmp(cmd, "bss_vht_beamformee_sts") == 0) {

		/* Save old values for rollback on failure */
		u32 old_vht_capab = hapd->conf->vht_capab;
		u32 old_vht_capab_mask = hapd->conf->vht_capab_mask;

		if (hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value))
			goto vht_rollback;

		if (hostapd_validate_bss_capab(hapd) < 0)
			goto vht_rollback;

		if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
			goto vht_rollback;

		return 0;
vht_rollback:
		hapd->conf->vht_capab = old_vht_capab;
		hapd->conf->vht_capab_mask = old_vht_capab_mask;
		return -1;
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	} else if (os_strcasecmp(cmd, "bss_he_su_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_he_su_beamformee") == 0 ||
		   os_strcasecmp(cmd, "bss_he_mu_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_he_ul_mumimo") == 0 ||
		   os_strcasecmp(cmd, "bss_he_full_bw_ul_mumimo") == 0 ||
		   os_strcasecmp(cmd, "bss_he_bfee_sts_lteq80") == 0 ||
		   os_strcasecmp(cmd, "bss_he_bfee_sts_gt80") == 0 ||
		   os_strcasecmp(cmd, "bss_he_subfee_sts_lteq80") == 0 ||
		   os_strcasecmp(cmd, "bss_he_subfee_sts_gt80") == 0 ||
		   os_strcasecmp(cmd, "bss_he_multi_tid_aggr") == 0 ||
		   os_strcasecmp(cmd, "bss_he_multi_tid_aggr_tx") == 0 ||
		   os_strcasecmp(cmd, "bss_he_max_ampdu_len_exp") == 0 ||
		   os_strcasecmp(cmd, "bss_he_su_ppdu_1x_ltf_800ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_su_mu_ppdu_4x_ltf_800ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_max_frag_msdu") == 0 ||
		   os_strcasecmp(cmd, "bss_he_min_frag_size") == 0 ||
		   os_strcasecmp(cmd, "bss_he_omi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_ndp_4x_ltf_3200ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_fragmentation") == 0 ||
		   os_strcasecmp(cmd, "bss_he_amsdu_in_ampdu_suprt") == 0 ||
		   os_strcasecmp(cmd, "bss_he_amsdu_in_ampdu_supp") == 0 ||
		   os_strcasecmp(cmd, "bss_he_max_nc_suprt") == 0 ||
		   os_strcasecmp(cmd, "bss_he_er_su_disable") == 0 ||
		   os_strcasecmp(cmd, "bss_he_er_su_ppdu_1x_ltf_800ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_er_su_ppdu_4x_ltf_800ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_he_1024qam_lt242ru_rx_enable") == 0 ||
		   os_strcasecmp(cmd, "bss_he_bsr_support") == 0) {
		/* Save old values for rollback on failure */
		struct he_phy_capabilities_info old_he_phy_capab = hapd->conf->he_phy_capab;
		u32 old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

		if (hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value))
			goto he_rollback;

		if (hostapd_validate_bss_capab(hapd) < 0)
			goto he_rollback;

		if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
			goto he_rollback;

		return 0;
he_rollback:
		hapd->conf->he_phy_capab = old_he_phy_capab;
		hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
		return -1;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	} else if (os_strcasecmp(cmd, "bss_eht_su_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_su_beamformee") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_mu_beamformer") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_ndp_4x_eht_ltf_and_320nsgi") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_num_sd_lt80") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_num_sd_160") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_num_sd_320") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_4x_eht_ltf_and_800ns_gi") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_dl_ofdma_txbf") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_trig_su_bf_fb") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_trig_cqi_fb") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_non_trig_cqi_fb") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_sup_mcs15_in_mru") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_mcs15_supp") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_mcs14_dup_in_6ghz") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_ltf") == 0) {
		/* Save old values for rollback on failure */
		struct eht_phy_capabilities_info old_eht_generic_capab = hapd->conf->eht_phy_capab;
		u32 old_eht_generic_capab_mask = hapd->conf->eht_phy_capab_mask;

		if (hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value))
			goto eht_generic_rollback;

		if (hostapd_validate_bss_capab(hapd) < 0)
			goto eht_generic_rollback;

		if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
			goto eht_generic_rollback;

		return 0;
eht_generic_rollback:
		hapd->conf->eht_phy_capab = old_eht_generic_capab;
		hapd->conf->eht_phy_capab_mask = old_eht_generic_capab_mask;
		return -1;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211BN
	} else if (os_strcasecmp(cmd, "bss_uhr_2xldpc_tx") == 0 ||
		   os_strcasecmp(cmd, "bss_uhr_2xldpc_rx") == 0) {
		/* Save old values for rollback on failure */
		struct uhr_phy_capabilities_info old_uhr_phy_capab = hapd->conf->uhr_phy_capab;
		u32 old_uhr_phy_capab_mask = hapd->conf->uhr_phy_capab_mask;

		if (hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value))
			goto uhr_2xldpc_rollback;

		if (hostapd_validate_bss_capab(hapd) < 0)
			goto uhr_2xldpc_rollback;

		if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
			goto uhr_2xldpc_rollback;

		return 0;
uhr_2xldpc_rollback:
		hapd->conf->uhr_phy_capab = old_uhr_phy_capab;
		hapd->conf->uhr_phy_capab_mask = old_uhr_phy_capab_mask;
		return -1;
#endif /* CONFIG_IEEE80211BN */
	} else {
		if (hapd->iface->conf->disable_csa_dfs &&
		    ((os_strcmp(cmd, "channel") == 0) &&
		    ((os_strcmp(value, "acs_survey") != 0) &&
		    (atoi(value) != 0)))) {
			eloop_cancel_timeout(hostapd_dfs_radar_handling_timeout,
					     hapd->iface, NULL);
		}
		ret = hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value);
		if (ret)
			return ret;

		if (os_strcasecmp(cmd, "deny_mac_file") == 0) {
			hostapd_disassoc_deny_mac(hapd);
		} else if (os_strcasecmp(cmd, "accept_mac_file") == 0) {
			hostapd_disassoc_accept_mac(hapd);
		} else if (os_strcasecmp(cmd, "macaddr_acl") == 0) {
			/*
			 * ACL mode changed at runtime: re-evaluate all connected
			 * STAs against the new mode and the current accept/deny
			 * lists.
			 */
			if (hapd->conf->num_accept_mac > 0 ||
			    hapd->conf->num_accept_mac_masked > 0)
				hostapd_disassoc_accept_mac(hapd);
			hostapd_disassoc_deny_mac(hapd);
		} else if (os_strcasecmp(cmd, "rssi_reject_assoc_rssi") == 0) {
			hostapd_ctrl_iface_update_rssi_monitor(hapd);
		} else if (os_strcasecmp(cmd, "ssid") == 0) {
			hostapd_neighbor_sync_own_report(hapd);
		} else if (os_strcasecmp(cmd, "rnr") == 0) {
			ieee802_11_set_beacon(hapd);
		} else if (os_strcasecmp(cmd, "rnr_ie_allowed") == 0) {
			ieee802_11_set_beacon(hapd);
		} else if (os_strcasecmp(cmd, "dtim_period") == 0) {
			ieee802_11_set_beacon(hapd);
#ifdef CONFIG_IEEE80211AC
		} else if (os_strcasecmp(cmd, "vht_mcs_nss_set") == 0) {
			if (!hapd->conf->is_cmn_param)
				return hostapd_reload_bss_only(hapd);
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
		} else if (os_strcasecmp(cmd, "he_tx_mcs_nss_set") == 0 ||
			   os_strcasecmp(cmd, "he_rx_mcs_nss_set") == 0) {
			if (hostapd_tx_bss_only(hapd, cmd) < 0)
				return -1;
			return hostapd_reload_bss_only(hapd);
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
		} else if (os_strcasecmp(cmd, "bss_eht_mu_mimo") == 0) {
			/* Save old values for rollback on failure */
			struct eht_phy_capabilities_info old_eht_phy_capab =
				hapd->conf->eht_phy_capab;
			u32 old_eht_phy_capab_mask = hapd->conf->eht_phy_capab_mask;
			long val = strtol(value, NULL, 0);
			if (val < 0 || val > 0x7)
				return -1;

			if (!eht_mu_mask_valid((u8)val)) {
				wpa_printf(MSG_ERROR,
					   "Invalid bss_eht_mu_mimo 0x%lx: "
					   "higher BW requires lower BW support",
					   val);
				return -1;
			}

			hapd->conf->eht_phy_capab.eht_mu_mimo_mask = (u8)val;
			hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO;

			if (hostapd_validate_bss_capab(hapd) < 0)
				goto eht_mu_mimo_rollback;

			if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
				goto eht_mu_mimo_rollback;

			return 0;
eht_mu_mimo_rollback:
			hapd->conf->eht_phy_capab = old_eht_phy_capab;
			hapd->conf->eht_phy_capab_mask = old_eht_phy_capab_mask;
			return -1;
		} else if (os_strcasecmp(cmd, "bss_eht_mu_bfmr") == 0) {
			/* Save old values for rollback on failure */
			struct eht_phy_capabilities_info old_eht_phy_capab =
				hapd->conf->eht_phy_capab;
			u32 old_eht_phy_capab_mask = hapd->conf->eht_phy_capab_mask;
			long val = strtol(value, NULL, 0);

			if (val < 0 || val > 0x7)
				return -1;

			if (!eht_mu_mask_valid((u8)val)) {
				wpa_printf(MSG_ERROR,
					   "Invalid bss_eht_mu_bfmr 0x%lx: "
					   "higher BW requires lower BW support",
					   val);
				return -1;
			}

			hapd->conf->eht_phy_capab.eht_mu_bfmr_mask = (u8)val;
			hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_MU_BFMR_MASK;

			if (hostapd_validate_bss_capab(hapd) < 0)
				goto eht_mu_bfmr_rollback;

			if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
				goto eht_mu_bfmr_rollback;

			return 0;
eht_mu_bfmr_rollback:
			hapd->conf->eht_phy_capab = old_eht_phy_capab;
			hapd->conf->eht_phy_capab_mask = old_eht_phy_capab_mask;
			return -1;
		} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_80") == 0 ||
			   os_strcasecmp(cmd, "bss_eht_bfme_ss_160") == 0 ||
			   os_strcasecmp(cmd, "bss_eht_bfme_ss_320") == 0) {
			/* Save old values for rollback on failure */
			struct eht_phy_capabilities_info old_eht_phy_capab = hapd->conf->eht_phy_capab;
			u32 old_eht_phy_capab_mask = hapd->conf->eht_phy_capab_mask;
			int val = (int) strtol(value, NULL, 0);

			if (val < 0 || val > 7)
				return -1;

			if (os_strcasecmp(cmd, "bss_eht_bfme_ss_80") == 0) {
				hapd->conf->eht_phy_capab.eht_bfme_ss_80 = val;
				wpa_printf(MSG_INFO, "SET: eht_bfme_ss_80=%d at %p", val, &hapd->conf->eht_phy_capab.eht_bfme_ss_80);
				hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_BFME_SS_80;
				/* Enable SU beamformee when SS is set to non-zero */
				if (val > 0) {
					hapd->conf->eht_phy_capab.su_beamformee = 1;
					hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_SU_BEAMFORMEE;
				}
			} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_160") == 0) {
				hapd->conf->eht_phy_capab.eht_bfme_ss_160 = val;
				wpa_printf(MSG_INFO, "SET: eht_bfme_ss_160=%d at %p", val, &hapd->conf->eht_phy_capab.eht_bfme_ss_160);
				hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_BFME_SS_160;
				/* Enable SU beamformee when SS is set to non-zero */
				if (val > 0) {
					hapd->conf->eht_phy_capab.su_beamformee = 1;
					hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_SU_BEAMFORMEE;
				}
			} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_320") == 0) {
				hapd->conf->eht_phy_capab.eht_bfme_ss_320 = val;
				wpa_printf(MSG_INFO, "SET: eht_bfme_ss_320=%d at %p", val, &hapd->conf->eht_phy_capab.eht_bfme_ss_320);
				hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_BFME_SS_320;
				/* Enable SU beamformee when SS is set to non-zero */
				if (val > 0) {
					hapd->conf->eht_phy_capab.su_beamformee = 1;
					hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_SU_BEAMFORMEE;
				}
			}

			if (hostapd_validate_bss_capab(hapd) < 0)
				goto eht_bfme_ss_rollback;

			if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
				goto eht_bfme_ss_rollback;

			return 0;
eht_bfme_ss_rollback:
			hapd->conf->eht_phy_capab = old_eht_phy_capab;
			hapd->conf->eht_phy_capab_mask = old_eht_phy_capab_mask;
			return -1;
		} else if (os_strcasecmp(cmd, "eht_tx_mcs_nss_set") == 0 ||
			   os_strcasecmp(cmd, "eht_rx_mcs_nss_set") == 0) {
			if (hostapd_tx_bss_only(hapd, cmd) < 0)
				return -1;
			return hostapd_reload_bss_only(hapd);
#endif /* CONFIG_IEEE80211BE */
		} else if (os_strcasecmp(cmd, "ht_mcs_nss_set") == 0) {
			if (!hapd->conf->is_cmn_param)
				return hostapd_reload_bss_only(hapd);
		} else if (os_strncmp(cmd, "wme_ac_", 7) == 0 ||
			   os_strncmp(cmd, "wmm_ac_", 7) == 0) {
			hapd->parameter_set_count++;
			if (tx_hapd && ieee802_11_update_beacons(tx_hapd->iface))
				wpa_printf(MSG_DEBUG,
					   "Failed to update beacons with WMM parameters");
		} else if (os_strcmp(cmd, "wpa_passphrase") == 0 ||
			   os_strcmp(cmd, "sae_password") == 0 ||
			   os_strcmp(cmd, "sae_pwe") == 0) {
			if (hapd->started)
				hostapd_setup_sae_pt(hapd->conf);
		} else if (os_strcasecmp(cmd, "transition_disable") == 0) {
			wpa_auth_set_transition_disable(hapd->wpa_auth,
							hapd->conf->transition_disable);
		} else if (os_strcasecmp(cmd, "dfs_bw_reduce_en") == 0) {
			ret = hostapd_set_bw_reduce_en(hapd, value);
		} else if (os_strcasecmp(cmd, "post_nol_freq") == 0) {
			ret = hostapd_ctrl_iface_set_post_nol_freq(hapd, value);
		} else if (os_strcasecmp(cmd, "post_nol_width") == 0) {
			ret = hostapd_ctrl_iface_set_post_nol_width(hapd, value);
		} else if (os_strcasecmp(cmd, "post_nol_bgcac_en") == 0) {
			ret = hostapd_ctrl_iface_set_post_nol_bgcac_en(hapd, value);
		} else if (os_strcasecmp(cmd, "next_radar_freq") == 0) {
			ret = hostapd_ctrl_iface_set_next_radar_freq(hapd, value);
		} else if (os_strcasecmp(cmd, "next_radar_width") == 0) {
			ret = hostapd_ctrl_iface_set_next_radar_width(hapd, value);
#ifdef CONFIG_QCN_EXTN
		} else {
			ret = hostapd_ctrl_iface_set_extn(hapd, cmd, value);
#endif /* CONFIG_QCN_EXTN */
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (os_strcmp(cmd, "ft_rsnxe_used") == 0)
			wpa_auth_set_ft_rsnxe_used(hapd->wpa_auth,
						   hapd->conf->ft_rsnxe_used);
		else if (os_strcmp(cmd, "oci_freq_override_eapol_m3") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_EAPOL_M3,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_eapol_g1") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_EAPOL_G1,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_ft_assoc") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_FT_ASSOC,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_fils_assoc") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth,
				WPA_AUTH_OCV_OVERRIDE_FILS_ASSOC, atoi(value));
#endif /* CONFIG_TESTING_OPTIONS */
	}

	return ret;
}

static int hostapd_ctrl_iface_get_scan_status(struct hostapd_data *hapd,
					   char *buf, size_t buflen)
{
	struct i802_bss *bss = hapd->drv_priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int scanning;

	scanning = (drv->scan_state == SCAN_STARTED ||
		    drv->scan_state == SCAN_REQUESTED) ? 1 : 0;

	return os_snprintf(buf, buflen, "scanning: %d\n", scanning);
}

static int hostapd_get_vendor_elements(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_data *tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	char *pos = buf, *end = buf + buflen;
	size_t count, i;
	int ret;

	count = hapd->conf->vendor_elements_count;

	for (i = 0; i < count; i++) {
		struct wpabuf *entry = hapd->conf->vendor_elements[i];

		pos += wpa_snprintf_hex(pos, end - pos, wpabuf_head_u8(entry),
					wpabuf_len(entry));

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd != tx_hapd) {
		ret = os_snprintf(pos, end - pos, "Available vendor elements size: %zu\n",
				  hapd->conf->available_vendor_elem_size);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}

static int hostapd_get_wmm_params(struct hostapd_data *hapd, char *cmd,
				  char *buf, size_t buflen)
{
	const char *pos;
	int ret = 0, v = 0, ac = -1;
	struct hostapd_wmm_ac_params *current_wmm_param;

	if (!hapd->started) {
		wpa_printf(MSG_ERROR, "Interface is not UP");
		return -1;
	}

	/* skip 'wmm_ac_' prefix */
	pos = cmd + 7;

	if (os_strncmp(pos, "be_", 3) == 0) {
		ac = 0;
		pos += 3;
	} else if (os_strncmp(pos, "bk_", 3) == 0) {
		ac = 1;
		pos += 3;
	} else if (os_strncmp(pos, "vi_", 3) == 0) {
		ac = 2;
		pos += 3;
	} else if (os_strncmp(pos, "vo_", 3) == 0) {
		ac = 3;
		pos += 3;
	} else {
		wpa_printf(MSG_ERROR, "Unknown WMM Access Category '%s'", pos);
		return -1;
	}

	current_wmm_param = &hapd->conf->wmm_ac_params[ac];

	if (os_strcmp(pos, "aifs") == 0) {
		v = current_wmm_param->aifs;
	} else if (os_strcmp(pos, "cwmin") == 0) {
		v = current_wmm_param->cwmin;
	} else if (os_strcmp(pos, "cwmax") == 0) {
		v = current_wmm_param->cwmax;
	} else if (os_strcmp(pos, "txop_limit") == 0) {
		v = current_wmm_param->txop_limit;
	} else if (os_strcmp(pos, "acm") == 0) {
		v = current_wmm_param->admission_control_mandatory;
	} else {
		wpa_printf(MSG_ERROR, "Unknown WMM param '%s'", pos);
		return -1;
	}

	ret = os_snprintf(buf, buflen, "%d\n", v);

	return ret;
}


static int hostapd_get_tx_queue_params(struct hostapd_data *hapd, char *cmd,
				       char *buf, size_t buflen)
{
	const char *pos;
	int ret = 0, v = 0, ac = -1;
	struct hostapd_tx_queue_params *queue;

	if (!hapd->started) {
		wpa_printf(MSG_ERROR, "Interface is not UP");
		return -1;
	}

	/* skip 'tx_queue_' prefix */
	pos = cmd + 9;

	if (os_strncmp(pos, "data", 4) == 0 &&
	    pos[4] >= '0' && pos[4] <= '9' && pos[5] == '_') {
		ac = pos[4] - '0';
		pos += 6;
	} else {
		wpa_printf(MSG_ERROR, "Unknown tx_queue name '%s'", pos);
		return -1;
	}

	if (ac < 0 || ac >= NUM_TX_QUEUES) {
		/* for backwards compatibility, do not trigger failure */
		wpa_printf(MSG_INFO, "DEPRECATED: '%s' not used", cmd);
		return 0;
	}

	queue = &hapd->iface->conf->tx_queue[ac];

	if (os_strcmp(pos, "aifs") == 0) {
		v = queue->aifs;
	} else if (os_strcmp(pos, "cwmin") == 0) {
		v = queue->cwmin;
	} else if (os_strcmp(pos, "cwmax") == 0) {
		v = queue->cwmax;
	} else if (os_strcmp(pos, "burst") == 0) {
		v = queue->burst;
	} else if (os_strcmp(pos, "acm") == 0) {
		v = queue->acm;
	} else if (os_strcmp(pos, "noack") == 0) {
		v = queue->noack;
	} else {
		wpa_printf(MSG_ERROR, "Unknown tx_queue_ param '%s'", pos);
		return -1;
	}

	ret = os_snprintf(buf, buflen, "%d\n", v);

	return ret;
}

static int hostapd_ctrl_iface_get_mbssid_attributes(struct hostapd_data *hapd,
						    char *buf, size_t buflen)
{
	struct hostapd_data *bss, *tx_hapd;
	struct hostapd_multi_mbssid_group *mbssid_group;
	size_t active_group_cnt = 0;
	int res, i, j;
	char *pos, *end;

	if (!hapd) {
		wpa_printf(MSG_ERROR, "Invalid BSS");
		return -1;
	}

	if (hapd->iconf->mbssid == MBSSID_DISABLED) {
		wpa_printf(MSG_ERROR, "%s is not part of any MBSSID group",
			   hapd->conf->iface);
		return -1;
	}

	pos = buf;
	end = buf + buflen;

	if ((hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED) ||
	    (hapd->iconf->mbssid == MBSSID_ENABLED)) {
		res = os_snprintf(pos, end - pos, "IFACE  BSSID            BSSID_INDEX\n");
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;

		res = os_snprintf(pos, end - pos, "===================================\n");
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;

		tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];
			if (!bss)
				continue;

			res = os_snprintf(pos, end - pos,
					  "%s " MACSTR " " "%s     %zu\n",
					  bss->conf->iface,
					  MAC2STR(bss->own_addr),
					  (bss == tx_hapd) ? "*":" ",
					  bss->mbssid_idx);
			if (os_snprintf_error(end - pos, res))
				return pos - buf;
			pos += res;
		}

		if (hapd->iface->num_bss) {
			res = os_snprintf(pos, end - pos,
					  "mbssid_idx_bmap = 0x%x\n",
					  hapd->iface->mbssid_idx_bmap);
			if (os_snprintf_error(end - pos, res))
				return pos - buf;
			pos += res;
		}

		return pos - buf;
	}

	/* MULTI_MBSSID_GROUP_ENABLED */
	res = os_snprintf(pos, end - pos, "IFACE  BSSID            BSSID_INDEX GRP_ID\n");
	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	res = os_snprintf(pos, end - pos, "===========================================\n");
	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	for (i = 0; i < hapd->iface->multi_mbssid.num_mbssid_groups; i++) {
		mbssid_group = hapd->iface->multi_mbssid.group[i];

		if (!mbssid_group)
			continue;

		for (j = 0; j < mbssid_group->num_bss; j++) {
			bss = hostapd_get_multi_group_bss(mbssid_group, j);
			if (!bss)
				continue;

			res = os_snprintf(pos, end - pos,
					  "%s " MACSTR " " "%s     %zu        %u\n",
					  bss->conf->iface,
					  MAC2STR(bss->own_addr),
					  (bss->mbssid_group->txbss == bss) ? "*":" ",
					  bss->mbssid_idx,
					  bss->mbssid_group->group_id);
			if (os_snprintf_error(end - pos, res))
				return pos - buf;
			pos += res;

			if (j == 0)
				active_group_cnt++;
		}

		if (mbssid_group->num_bss) {
			res = os_snprintf(pos, end - pos,
					  "mbssid_idx_bmap = 0x%x\n",
					  mbssid_group->mbssid_idx_bmap);
			if (os_snprintf_error(end - pos, res))
				return pos - buf;
			pos += res;
		}

		res = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;
	}

	res = os_snprintf(pos, end - pos, "current_active_ngroups = %zu\n",
			  active_group_cnt);
	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	res = os_snprintf(pos, end - pos, "max_ngroups = %zu\nmbssid_group_size = %u\n",
			  hapd->iface->multi_mbssid.num_mbssid_groups, hapd->iconf->group_size);

	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	return pos - buf;
}

static const char * hostapd_fils_state_to_str(u8 state)
{
	switch(state) {
	case FILS_UBPR_USER_DISABLED:
		return "Disabled";
	case FILS_UBPR_FORCE_DISABLED:
		return "Forced_Disabled";
	case FILS_UBPR_ENABLED:
		return "Enabled";
	default:
		return "Unknown";
	}
}

static int hostapd_ctrl_iface_get(struct hostapd_data *hapd, char *cmd,
				  char *buf, size_t buflen)
{
	int res;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET '%s'", cmd);

	if (os_strcmp(cmd, "version") == 0) {
		res = os_snprintf(buf, buflen, "%s", VERSION_STR);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "tls_library") == 0) {
		res = tls_get_library_version(buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strncmp(cmd, "wmm_ac_", 7) == 0) {
		res = hostapd_get_wmm_params(hapd, cmd, buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strncmp(cmd, "tx_queue_", 9) == 0) {
		res = hostapd_get_tx_queue_params(hapd, cmd, buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#ifdef NEED_AP_MLME
	} else if (os_strcmp(cmd, "bgcac_en") == 0) {
		int enabled = hapd->iface->conf ?
			hapd->iface->conf->bgcac_en : 0;
		res = os_snprintf(buf, buflen, "%d\n", enabled);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "bgcac_status") == 0) {
		return hostapd_ctrl_iface_get_bgcac_status(hapd, buf, buflen);
#endif /* NEED_AP_MLME */
	} else if (os_strcmp(cmd, "macaddr_acl") == 0) {
		if (!hapd || !hapd->conf) {
			wpa_printf(MSG_ERROR, "Invalid hapd or hapd->conf pointer");
			return -1;
		}

		res = os_snprintf(buf, buflen, "%d\n", hapd->conf->macaddr_acl);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "scan_status") == 0) {
		return hostapd_ctrl_iface_get_scan_status(hapd, buf, buflen);
	} else if (os_strcmp(cmd, "acl_deny_wait_time") == 0) {
		res = os_snprintf(buf, buflen, "%u\n",
				  hapd->conf->acl_deny_wait_time);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "acl_deny_allow_time") == 0) {
		res = os_snprintf(buf, buflen, "%u\n",
				  hapd->conf->acl_deny_allow_time);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rnr") == 0) {
		res = os_snprintf(buf, buflen, "rnr = %u\n", hapd->conf->rnr);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rnr_ie_allowed") == 0) {
		res = os_snprintf(buf, buflen, "rnr_ie_allowed = %u\n",
				  hapd->conf->rnr_ie_allowed);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#ifdef CONFIG_MBO
	} else if (os_strcmp(cmd, "mbo_trans_reason") == 0) {
		res = os_snprintf(buf, buflen, "mbo_trans_reason = %d\n",
				  hapd->mbo_trans_reason);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "mbo_assoc_retry") == 0) {
		res = os_snprintf(buf, buflen, "mbo_assoc_retry = %d\n",
				  hapd->mbo_assoc_retry);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#endif /* CONFIG_MBO */
#ifdef CONFIG_IEEE80211AC
	} else if (os_strcmp(cmd, "vht_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen, "vht_mcs_nss_set = 0x%x\n",
				  hapd->conf->vht_mcs_nss_set);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_vht_mu_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_vht_mu_beamformer = %d\n",
				!!(hapd->conf->vht_capab & VHT_CAP_MU_BEAMFORMER_CAPABLE));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_vht_su_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_vht_su_beamformer = %d\n",
				!!(hapd->conf->vht_capab & VHT_CAP_SU_BEAMFORMER_CAPABLE));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_vht_su_beamformee") == 0) {
		res = os_snprintf(buf, buflen, "bss_vht_su_beamformee = %d\n",
				!!(hapd->conf->vht_capab & VHT_CAP_SU_BEAMFORMEE_CAPABLE));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_vht_sounding_dimension") == 0) {
		int val = (hapd->conf->vht_capab & VHT_CAP_SOUNDING_DIMENSION_MAX)
			>> VHT_CAP_SOUNDING_DIMENSION_OFFSET;
		res = os_snprintf(buf, buflen,
				"bss_vht_sounding_dimension = %d\n", val);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_vht_beamformee_sts") == 0) {
		int val = (hapd->conf->vht_capab & VHT_CAP_BEAMFORMEE_STS_MAX)
			>> VHT_CAP_BEAMFORMEE_STS_OFFSET;
		res = os_snprintf(buf, buflen,
				"bss_vht_beamformee_sts = %d\n", val);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	} else if (os_strcasecmp(cmd, "bss_he_su_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_su_beamformer = %d\n",
				hapd->conf->he_phy_capab.he_su_beamformer);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_su_beamformee") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_su_beamformee = %d\n",
				hapd->conf->he_phy_capab.he_su_beamformee);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_mu_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_mu_beamformer = %d\n",
				hapd->conf->he_phy_capab.he_mu_beamformer);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_ul_mumimo") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_ul_mumimo = %d\n",
				hapd->conf->he_phy_capab.he_ul_mumimo);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_bfee_sts_lteq80") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_bfee_sts_lteq80 = %u\n",
				  hapd->conf->he_phy_capab.he_bfee_sts_lteq80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_bfee_sts_gt80") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_bfee_sts_gt80 = %u\n",
				  hapd->conf->he_phy_capab.he_bfee_sts_gt80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_subfee_sts_lteq80") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_subfee_sts_lteq80 = %u\n",
				  hapd->conf->he_phy_capab.he_bfee_sts_lteq80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_subfee_sts_gt80") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_subfee_sts_gt80 = %u\n",
				  hapd->conf->he_phy_capab.he_bfee_sts_gt80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_fragmentation") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_fragmentation = %u\n",
				  hapd->conf->he_phy_capab.he_fragmentation);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_amsdu_in_ampdu_suprt") == 0 ||
		   os_strcasecmp(cmd, "bss_he_amsdu_in_ampdu_supp") == 0) {
		res = os_snprintf(buf, buflen,
				  "%s = %u\n", cmd,
				  hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_max_nc_suprt") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_max_nc_suprt = %u\n",
				  hapd->conf->he_phy_capab.he_max_nc);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_er_su_disable") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_er_su_disable = %u\n",
				  hapd->conf->he_phy_capab.he_er_su_disable);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_er_su_ppdu_1x_ltf_800ns_gi") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_er_su_ppdu_1x_ltf_800ns_gi = %u\n",
				  hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_er_su_ppdu_4x_ltf_800ns_gi") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_er_su_ppdu_4x_ltf_800ns_gi = %u\n",
				  hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_1024qam_lt242ru_rx_enable") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_1024qam_lt242ru_rx_enable = %u\n",
				  hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_full_bw_ul_mumimo") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_he_full_bw_ul_mumimo = %d\n",
				  hapd->conf->he_phy_capab.he_ul_mumimo);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_he_bsr_support") == 0) {
		res = os_snprintf(buf, buflen, "bss_he_bsr_support = %u\n",
				  hapd->conf->he_phy_capab.he_bsr_support);
	} else if (os_strcasecmp(cmd, "he_6ghz_min_rate") == 0) {
		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			wpa_printf(MSG_ERROR,
				   "he_6ghz_min_rate is applicable for 6 GHz only");
			return -1;
		}

		res = os_snprintf(buf, buflen, "he_6ghz_min_rate = %u\n",
				  hapd->iconf->he_6ghz_min_rate);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_tx_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen,
				  "he_tx_mcs_nss_set = 0x%04x(<=80 mhz) 0x%04x(160 mhz)\n",
				  hapd->conf->he_tx_mcs_nss_set[0],
				  hapd->conf->he_tx_mcs_nss_set[1]);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_rx_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen,
				  "he_rx_mcs_nss_set = 0x%04x(<=80 mhz) 0x%04x(160 mhz)\n",
				  hapd->conf->he_rx_mcs_nss_set[0],
				  hapd->conf->he_rx_mcs_nss_set[1]);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	} else if (os_strcasecmp(cmd, "bss_eht_su_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_su_beamformer = %d\n",
				hapd->conf->eht_phy_capab.su_beamformer);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_su_beamformee") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_su_beamformee = %d\n",
				hapd->conf->eht_phy_capab.su_beamformee);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_mu_beamformer") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_mu_beamformer = %d\n",
				hapd->conf->eht_phy_capab.mu_beamformer);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_mu_bfmr") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_mu_bfmr = 0x%x\n",
				  hapd->conf->eht_phy_capab.eht_mu_bfmr_mask);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_mu_mimo") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_mu_mimo = 0x%x\n",
				  hapd->conf->eht_phy_capab.eht_mu_mimo_mask);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "eht_ulmumimo_80mhz") == 0) {
		res = os_snprintf(buf, buflen, "eht_ulmumimo_80mhz = %d\n",
				  (hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO) ?
				  !!(hapd->conf->eht_phy_capab.eht_mu_mimo_mask & BIT(0)) :
				  hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_80mhz);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "eht_ulmumimo_160mhz") == 0) {
		res = os_snprintf(buf, buflen, "eht_ulmumimo_160mhz = %d\n",
				  (hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO) ?
				  !!(hapd->conf->eht_phy_capab.eht_mu_mimo_mask & BIT(1)) :
				  hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_160mhz);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "eht_ulmumimo_320mhz") == 0) {
		res = os_snprintf(buf, buflen, "eht_ulmumimo_320mhz = %d\n",
				  (hapd->conf->eht_phy_capab_mask &
				   EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO) ?
				  !!(hapd->conf->eht_phy_capab.eht_mu_mimo_mask & BIT(2)) :
				  hapd->iface->conf->eht_phy_capab.non_ofdma_ulmumimo_320mhz);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "eht_tx_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen,
				  "eht_tx_mcs_nss_set = 0x%04x(<=80 mhz) 0x%04x(160 mhz) 0x%04x(320 mhz)\n",
				  hapd->conf->eht_tx_mcs_nss_set[0],
				  hapd->conf->eht_tx_mcs_nss_set[1],
				  hapd->conf->eht_tx_mcs_nss_set[2]);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "eht_rx_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen,
				  "eht_rx_mcs_nss_set = 0x%04x(<=80 mhz) 0x%04x(160 mhz) 0x%04x(320 mhz)\n",
				  hapd->conf->eht_rx_mcs_nss_set[0],
				  hapd->conf->eht_rx_mcs_nss_set[1],
				  hapd->conf->eht_rx_mcs_nss_set[2]);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_80") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_bfme_ss_80 = %u\n",
				  hapd->conf->eht_phy_capab.eht_bfme_ss_80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_160") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_bfme_ss_160 = %u\n",
				  hapd->conf->eht_phy_capab.eht_bfme_ss_160);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_bfme_ss_320") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_bfme_ss_320 = %u\n",
				  hapd->conf->eht_phy_capab.eht_bfme_ss_320);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_ndp_4x_eht_ltf_and_320nsgi") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_eht_ndp_4x_eht_ltf_and_320nsgi = %u\n",
				  hapd->conf->eht_phy_capab
					  .eht_ndp_4x_eht_ltf_and_320nsgi);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_num_sd_lt80") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_num_sd_lt80 = %u\n",
				  hapd->conf->eht_phy_capab.eht_num_sd_lt80);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_num_sd_160") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_num_sd_160 = %u\n",
				  hapd->conf->eht_phy_capab.eht_num_sd_160);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_num_sd_320") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_num_sd_320 = %u\n",
				  hapd->conf->eht_phy_capab.eht_num_sd_320);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_4x_eht_ltf_and_800ns_gi") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_eht_4x_eht_ltf_and_800ns_gi = %u\n",
				  hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_eht_rx_1024_and_4096_qam_ls_242_tone_ru = %u\n",
				  hapd->conf->eht_phy_capab
					  .eht_rx_1024_and_4096_qam_ls_242_tone_ru);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_dl_ofdma_txbf") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_dl_ofdma_txbf = %u\n",
				  hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_trig_su_bf_fb") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_trig_su_bf_fb = %u\n",
				  hapd->conf->eht_phy_capab.eht_trig_su_bf_fb);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_trig_cqi_fb") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_trig_cqi_fb = %u\n",
				  hapd->conf->eht_phy_capab.eht_trig_cqi_fb);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_non_trig_cqi_fb") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_non_trig_cqi_fb = %u\n",
				  hapd->conf->eht_phy_capab.eht_non_trig_cqi_fb);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_sup_mcs15_in_mru") == 0 ||
		   os_strcasecmp(cmd, "bss_eht_mcs15_supp") == 0) {
		res = os_snprintf(buf, buflen, "%s = %u\n", cmd,
				  hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_mcs14_dup_in_6ghz") == 0) {
		res = os_snprintf(buf, buflen,
				  "bss_eht_mcs14_dup_in_6ghz = %u\n",
				  hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "bss_eht_ltf") == 0) {
		res = os_snprintf(buf, buflen, "bss_eht_ltf = %d\n",
				  hapd->conf->eht_ltf);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#endif /* CONFIG_IEEE80211BE */
	} else if (os_strcmp(cmd, "ht_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen, "ht_mcs_nss_set = 0x%x\n",
				  hapd->conf->ht_mcs_nss_set);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "vendor_elements") == 0) {
		res = hostapd_get_vendor_elements(hapd, buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "unsolicited_probe_resp_state") == 0) {
		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			res = os_snprintf(buf, buflen,
					  "unsolicited_probe_resp_state is applicable for 6 GHz only\n");
			if (os_snprintf_error(buflen, res))
				return -1;
			return res;
		}
		res = os_snprintf(buf, buflen, "state:%u (%s)\n", hapd->conf->ubpr_state,
				  hostapd_fils_state_to_str(hapd->conf->ubpr_state));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "fils_state") == 0) {
		if (!is_6ghz_op_class(hapd->iconf->op_class)) {
			res = os_snprintf(buf, buflen,
					  "fils_state is applicable for 6 GHz only\n");
			if (os_snprintf_error(buflen, res))
				return -1;
			return res;
		}
		res = os_snprintf(buf, buflen, "state:%u (%s)\n", hapd->conf->fils_state,
				  hostapd_fils_state_to_str(hapd->conf->fils_state));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "mbssid_attributes") == 0) {
		if (hapd->iconf->mbssid == MBSSID_DISABLED) {
			res = os_snprintf(buf, buflen, "MBSSID is disabled\n");
			if (os_snprintf_error(buflen, res))
				return -1;
			return res;
		}
		res = hostapd_ctrl_iface_get_mbssid_attributes(hapd, buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
	else if (os_strcasecmp(cmd, "rssi_reject_assoc_timeout" ) == 0) {
		res = os_snprintf(buf, buflen, "rssi_reject_assoc_timeout= %d\n",
				  hapd->iconf->rssi_reject_assoc_timeout);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
	else if (os_strcasecmp(cmd, "rssi_reject_assoc_rssi" ) == 0) {
		res = os_snprintf(buf, buflen, "rssi_reject_assoc_rssi= %d\n",
				  hapd->iconf->rssi_reject_assoc_rssi);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
	else if (os_strcasecmp(cmd, "rssi_ignore_probe_request") == 0) {
		res = os_snprintf(buf, buflen, "rssi_ignore_probe_request= %d\n",
				  hapd->iconf->rssi_ignore_probe_request);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "dfs_bw_reduce_en") == 0) {
		res = os_snprintf(buf, buflen, "%d\n", hapd->iface->conf->dfs_bw_reduce_en);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
	else if (os_strcasecmp(cmd, "rssi_probe_delay_time_window") == 0) {
		res = os_snprintf(buf, buflen, "rssi_probe_delay_time_window= %d\n",
				  hapd->iconf->rssi_probe_delay_time_window);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
	else if (os_strcasecmp(cmd, "rssi_probe_delay_req_count") == 0) {
		res = os_snprintf(buf, buflen, "rssi_probe_delay_req_count= %d\n",
				  hapd->iconf->rssi_probe_delay_req_count);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "use_ru_puncture_dfs") == 0) {
		if (!hapd || !hapd->iconf) {
			wpa_printf(MSG_ERROR, "Invalid hapd or hapd->iconf pointer");
			return -1;
		}

		res = os_snprintf(buf, buflen, "%d\n", hapd->iconf->use_ru_puncture_dfs);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "dfs_disable_auto_unpunc") == 0) {
		if (!hapd || !hapd->iconf) {
			wpa_printf(MSG_ERROR, "Invalid hapd or hapd->iconf pointer");
			return -1;
		}

		res = os_snprintf(buf, buflen, "%d\n", hapd->iconf->dfs_disable_auto_unpunc);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strcasecmp(cmd, "ecsa_ie_only") == 0) {
		res = os_snprintf(buf, buflen, "ecsa_ie_status = %d\n",
				  hapd->iconf->ecsa_ie_only);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#endif
	} else if (os_strcasecmp(cmd, "he_basic_mcs_nss_set") == 0) {
		res = os_snprintf(buf, buflen, "he_basic_mcs_nss_set = 0x%x\n",
				  hapd->iconf->he_op.he_basic_mcs_nss_set);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_rts_threshold") == 0) {
		res = os_snprintf(buf, buflen, "he_rts_threshold = 0x%x\n",
				  hapd->iconf->he_op.he_rts_threshold);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "spp_amsdu") == 0) {
		res = os_snprintf(buf, buflen, "spp_amsdu = %d\n",
				  hapd->conf->spp_amsdu);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_twt_responder") == 0) {
		res = os_snprintf(buf, buflen, "he_twt_responder = %u\n",
				  hapd->iconf->he_op.he_twt_responder);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_6ghz_max_ampdu_len_exp") == 0) {
		res = os_snprintf(buf, buflen, "he_6ghz_max_ampdu_len_exp = %u\n",
				  hapd->iconf->he_6ghz_max_ampdu_len_exp);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "he_er_su_disable") == 0) {
		res = os_snprintf(buf, buflen, "he_er_su_disable = %u\n",
				  hapd->iconf->he_op.he_er_su_disable);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "enable_mcs15") == 0) {
		res = os_snprintf(buf, buflen, "enable_mcs15 = %d\n",
				  hapd->iconf->enable_mcs15);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "short_gi_20") == 0) {
		res = os_snprintf(buf, buflen, "short_gi_20 = %u\n",
				  !!(hapd->iconf->ht_capab & HT_CAP_INFO_SHORT_GI20MHZ));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "short_gi_40") == 0) {
		res = os_snprintf(buf, buflen, "short_gi_40 = %u\n",
				  !!(hapd->iconf->ht_capab & HT_CAP_INFO_SHORT_GI40MHZ));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "max-amsdu-7935") == 0) {
		res = os_snprintf(buf, buflen, "max-amsdu-7935 = %u\n",
				  !!(hapd->iconf->ht_capab & HT_CAP_INFO_MAX_AMSDU_SIZE));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "short_gi_80") == 0) {
		res = os_snprintf(buf, buflen, "short_gi_80 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_SHORT_GI_80));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "short_gi_160") == 0) {
		res = os_snprintf(buf, buflen, "short_gi_160 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_SHORT_GI_160));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "max-mpdu-7991") == 0) {
		res = os_snprintf(buf, buflen, "max-mpdu-7991 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_MAX_MPDU_LENGTH_7991));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "mpdu-11454") == 0) {
		res = os_snprintf(buf, buflen, "mpdu-11454 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_MAX_MPDU_LENGTH_11454));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rxldpc") == 0) {
		res = os_snprintf(buf, buflen, "rxldpc = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_RXLDPC));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "tx-stbc-2by1") == 0) {
		res = os_snprintf(buf, buflen, "tx-stbc-2by1 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_TXSTBC));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rx-stbc-1") == 0) {
		res = os_snprintf(buf, buflen, "rx-stbc-1 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_RXSTBC_1));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rx-stbc-12") == 0) {
		res = os_snprintf(buf, buflen, "rx-stbc-12 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_RXSTBC_2));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rx-stbc-123") == 0) {
		res = os_snprintf(buf, buflen, "rx-stbc-123 = %u\n",
				  ((hapd->iconf->vht_capab & VHT_CAP_RXSTBC_3) >>
				    VHT_CAP_RXSTBC_MASK_SHIFT));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "rx-stbc-1234") == 0) {
		res = os_snprintf(buf, buflen, "rx-stbc-1234 = %u\n",
				  !!(hapd->iconf->vht_capab & VHT_CAP_RXSTBC_4));
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "beacon_int") == 0) {
		res = os_snprintf(buf, buflen, "beacon_int = %u\n",
				  hapd->iconf->beacon_int);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "post_nol_freq") == 0) {
		res = os_snprintf(buf, buflen, "%d\n", hapd->iface->conf->post_nol_freq);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "post_nol_width") == 0) {
		res = os_snprintf(buf, buflen, "%d\n", hapd->iface->conf->post_nol_width);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcasecmp(cmd, "post_nol_bgcac_en") == 0) {
		res = os_snprintf(buf, buflen, "%d\n",
				  hapd->iface->conf->post_nol_bgcac_en);
	} else if (os_strcmp(cmd, "next_radar_freq") == 0) {
		res = os_snprintf(buf, buflen, "%d\n",
				  hapd->iface->conf->next_radar_freq);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "next_radar_width") == 0) {
		res = os_snprintf(buf, buflen, "%d\n",
				  hapd->iface->conf->next_radar_width);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
#ifdef CONFIG_QCN_EXTN
	} else {
		res = hostapd_ctrl_iface_get_extn(hapd, cmd, buf, buflen);
		return res;
#endif /* CONFIG_QCN_EXTN */
	}

	return -1;
}


static int hostapd_ctrl_iface_enable(struct hostapd_iface *iface)
{
	if (hostapd_enable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Enabling of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_reload(struct hostapd_iface *iface)
{
	if (hostapd_reload_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Reloading of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_reload_bss(struct hostapd_data *bss)
{
	if (hostapd_reload_bss_only(bss) < 0) {
		wpa_printf(MSG_ERROR, "Reloading of BSS failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_disable(struct hostapd_iface *iface)
{
	if (hostapd_disable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Disabling of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_disable_bss(struct hostapd_data *hapd, int tbtt)
{
	size_t i;

	if (!hapd->started) {
		wpa_printf(MSG_INFO, "BSS %s already disabled",
			   hapd->conf->iface);
		return -1;
	}

	if (hapd->iface->conf->mbssid == MBSSID_DISABLED)
		goto disable_bss;

	if (hapd != hostapd_mbssid_get_tx_bss(hapd))
		goto disable_bss;

	/* If this is the TX-BSS of an MBSSID setup, disable all
	 * associated non-TX BSSes first.
	 */
	if (hapd->iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;
		struct hostapd_data *bss, *tmp;

		if (group) {
			dl_list_for_each_safe(bss, tmp, &group->bss_list,
					      struct hostapd_data, mbssid_bss) {
				if (bss == hapd)
					continue;

				hostapd_disable_bss(bss, tbtt, AP_EVENT_DISABLED);
			}
		}
	} else {
		for (i = 0; i < hapd->iface->num_bss; i++) {
			struct hostapd_data *bss = hapd->iface->bss[i];

			if (bss == hapd)
				continue;

			hostapd_disable_bss(bss, tbtt, AP_EVENT_DISABLED);
		}
	}

disable_bss:
	hostapd_disable_bss(hapd, tbtt, AP_EVENT_DISABLED);

	return 0;
}

static int hostapd_ctrl_iface_enable_bss(struct hostapd_data *hapd)
{
	if (hostapd_enable_bss(hapd) < 0) {
		wpa_printf(MSG_ERROR, "Enabling of BSS %s failed",
			   hapd->conf->iface);
		return -1;
	}
	return 0;
}

static int
hostapd_ctrl_iface_kick_mismatch_psk_sta_iter(struct hostapd_data *hapd,
					      struct sta_info *sta, void *ctx)
{
	struct hostapd_wpa_psk *psk;
	const u8 *pmk;
	int pmk_len;
	int pmk_match;
	int sta_match;
	int bss_match;
	int reason;

	pmk = wpa_auth_get_pmk(sta->wpa_sm, &pmk_len);

	for (psk = hapd->conf->ssid.wpa_psk; pmk && psk; psk = psk->next) {
		pmk_match = PMK_LEN == pmk_len &&
			os_memcmp(psk->psk, pmk, pmk_len) == 0;
		sta_match = psk->group == 0 &&
			ether_addr_equal(sta->addr, psk->addr);
		bss_match = psk->group == 1;

		if (pmk_match && (sta_match || bss_match))
			return 0;
	}

	wpa_printf(MSG_INFO, "STA " MACSTR
		   " PSK/passphrase no longer valid - disconnect",
		   MAC2STR(sta->addr));
	reason = WLAN_REASON_PREV_AUTH_NOT_VALID;
	hostapd_drv_sta_deauth(hapd, sta->addr, reason);
	ap_sta_deauthenticate(hapd, sta, reason);

	return 0;
}


static int hostapd_ctrl_iface_reload_wpa_psk(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int err;

	hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);

	err = hostapd_setup_wpa_psk(conf);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "Reloading WPA-PSK passwords failed: %d",
			   err);
		return -1;
	}

	ap_for_each_sta(hapd, hostapd_ctrl_iface_kick_mismatch_psk_sta_iter,
			NULL);

	return 0;
}


#ifdef CONFIG_IEEE80211R_AP

static int hostapd_ctrl_iface_get_rxkhs(struct hostapd_data *hapd,
					char *buf, size_t buflen)
{
	int ret, start_pos;
	char *pos, *end;
	struct ft_remote_r0kh *r0kh;
	struct ft_remote_r1kh *r1kh;
	struct hostapd_bss_config *conf = hapd->conf;

	pos = buf;
	end = buf + buflen;

	for (r0kh = conf->r0kh_list; r0kh; r0kh=r0kh->next) {
		start_pos = pos - buf;
		ret = os_snprintf(pos, end - pos, "r0kh=" MACSTR " ",
				  MAC2STR(r0kh->addr));
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
		if (r0kh->id_len + 1 >= (size_t) (end - pos))
			return start_pos;
		os_memcpy(pos, r0kh->id, r0kh->id_len);
		pos += r0kh->id_len;
		*pos++ = ' ';
		pos += wpa_snprintf_hex(pos, end - pos, r0kh->key,
					sizeof(r0kh->key));
		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
	}

	for (r1kh = conf->r1kh_list; r1kh; r1kh=r1kh->next) {
		start_pos = pos - buf;
		ret = os_snprintf(pos, end - pos, "r1kh=" MACSTR " " MACSTR " ",
			MAC2STR(r1kh->addr), MAC2STR(r1kh->id));
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
		pos += wpa_snprintf_hex(pos, end - pos, r1kh->key,
					sizeof(r1kh->key));
		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_reload_rxkhs(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int err;

	hostapd_config_clear_rxkhs(conf);

	err = hostapd_config_read_rxkh_file(conf, conf->rxkh_file);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "Reloading RxKHs failed: %d",
			   err);
		return -1;
	}

	return 0;
}

#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_TESTING_OPTIONS

static int hostapd_ctrl_iface_radar(struct hostapd_data *hapd, char *cmd)
{
	union wpa_event_data data;
	char *pos, *param;
	enum wpa_event_type event;

	wpa_printf(MSG_DEBUG, "RADAR TEST: %s", cmd);

	os_memset(&data, 0, sizeof(data));

	param = os_strchr(cmd, ' ');
	if (param == NULL)
		return -1;
	*param++ = '\0';

	if (os_strcmp(cmd, "DETECTED") == 0)
		event = EVENT_DFS_RADAR_DETECTED;
	else if (os_strcmp(cmd, "CAC-FINISHED") == 0)
		event = EVENT_DFS_CAC_FINISHED;
	else if (os_strcmp(cmd, "CAC-ABORTED") == 0)
		event = EVENT_DFS_CAC_ABORTED;
	else if (os_strcmp(cmd, "NOP-FINISHED") == 0)
		event = EVENT_DFS_NOP_FINISHED;
	else {
		wpa_printf(MSG_DEBUG, "Unsupported RADAR test command: %s",
			   cmd);
		return -1;
	}

	pos = os_strstr(param, "freq=");
	if (pos)
		data.dfs_event.freq = atoi(pos + 5);

	pos = os_strstr(param, "ht_enabled=1");
	if (pos)
		data.dfs_event.ht_enabled = 1;

	pos = os_strstr(param, "chan_offset=");
	if (pos)
		data.dfs_event.chan_offset = atoi(pos + 12);

	pos = os_strstr(param, "chan_width=");
	if (pos)
		data.dfs_event.chan_width = atoi(pos + 11);

	pos = os_strstr(param, "cf1=");
	if (pos)
		data.dfs_event.cf1 = atoi(pos + 4);

	pos = os_strstr(param, "cf2=");
	if (pos)
		data.dfs_event.cf2 = atoi(pos + 4);

	wpa_supplicant_event(hapd, event, &data);

	return 0;
}


static int hostapd_ctrl_iface_awgn(struct hostapd_data *hapd, char *cmd)
{
	union wpa_event_data data;
	char *pos, *param, *end;
	const char *val = NULL;

	wpa_printf(MSG_DEBUG, "AWGN TEST: %s", cmd);

	os_memset(&data, 0, sizeof(data));
	data.awgn_event.link_id = -1;

	param = os_strchr(cmd, ' ');
	if (!param)
		return -1;
	*param++ = '\0';

	if (os_strcmp(cmd, "DETECTED") != 0) {
		wpa_printf(MSG_DEBUG, "Unsupported AWGN test command: %s",
			   cmd);
		return -1;
	}

	pos = os_strstr(param, "freq=");
	if (!pos)
		return -1;
	data.awgn_event.freq = atoi(pos + 5);

	pos = os_strstr(param, "chan_width=");
	if (!pos)
		return -1;
	data.awgn_event.chan_width = atoi(pos + 11);

	pos = os_strstr(param, "cf1=");
	if (!pos)
		return -1;
	data.awgn_event.cf1 = atoi(pos + 4);

	pos = os_strstr(param, "cf2=");
	if (pos)
		data.awgn_event.cf2 = atoi(pos + 4);

	pos = os_strstr(param, "bitmap=");
	if (pos)
		val = pos + 7;
	else {
		pos = os_strstr(param, "chan_bw_interference_bitmap=");
		if (pos)
			val = pos + 28;
	}
	if (!val)
		return -1;
	data.awgn_event.chan_bw_interference_bitmap =
		strtoul(val, &end, 0);
	if (val == end)
		return -1;

	pos = os_strstr(param, "link_id=");
	if (pos)
		data.awgn_event.link_id = atoi(pos + 8);

	wpa_supplicant_event(hapd, EVENT_AWGN_DETECTED, &data);

	return 0;
}


static int hostapd_ctrl_iface_mgmt_tx(struct hostapd_data *hapd, char *cmd)
{
	size_t len;
	u8 *buf;
	int res;

	wpa_printf(MSG_DEBUG, "External MGMT TX: %s", cmd);

	len = os_strlen(cmd);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(cmd, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	res = hostapd_drv_send_mlme(hapd, buf, len, 0, NULL, 0, 0, 0, 0);
	os_free(buf);
	return res;
}


static int hostapd_ctrl_iface_mgmt_tx_status_process(struct hostapd_data *hapd,
						     char *cmd)
{
	char *pos, *param;
	size_t len;
	u8 *buf;
	int stype = 0, ok = 0;
	union wpa_event_data event;

	if (!hapd->ext_mgmt_frame_handling)
		return -1;

	/* stype=<val> ok=<0/1> buf=<frame hexdump> */

	wpa_printf(MSG_DEBUG, "External MGMT TX status process: %s", cmd);

	pos = cmd;
	param = os_strstr(pos, "stype=");
	if (param) {
		param += 6;
		stype = atoi(param);
	}

	param = os_strstr(pos, " ok=");
	if (param) {
		param += 4;
		ok = atoi(param);
	}

	param = os_strstr(pos, " buf=");
	if (!param)
		return -1;
	param += 5;

	len = os_strlen(param);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (!buf || hexstr2bin(param, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_TYPE_MGMT;
	event.tx_status.data = buf;
	event.tx_status.data_len = len;
	event.tx_status.stype = stype;
	event.tx_status.ack = ok;
	hapd->ext_mgmt_frame_handling = 0;
	wpa_supplicant_event(hapd, EVENT_TX_STATUS, &event);
	hapd->ext_mgmt_frame_handling = 1;

	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_mgmt_rx_process(struct hostapd_data *hapd,
					      char *cmd)
{
	char *pos, *param;
	size_t len;
	u8 *buf;
	int freq = 0, datarate = 0, ssi_signal = 0;
	union wpa_event_data event;

	if (!hapd->ext_mgmt_frame_handling)
		return -1;

	/* freq=<MHz> datarate=<val> ssi_signal=<val> frame=<frame hexdump> */

	wpa_printf(MSG_DEBUG, "External MGMT RX process: %s", cmd);

	pos = cmd;
	param = os_strstr(pos, "freq=");
	if (param) {
		param += 5;
		freq = atoi(param);
	}

	param = os_strstr(pos, " datarate=");
	if (param) {
		param += 10;
		datarate = atoi(param);
	}

	param = os_strstr(pos, " ssi_signal=");
	if (param) {
		param += 12;
		ssi_signal = atoi(param);
	}

	param = os_strstr(pos, " frame=");
	if (param == NULL)
		return -1;
	param += 7;

	len = os_strlen(param);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(param, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	os_memset(&event, 0, sizeof(event));
	event.rx_mgmt.freq = freq;
	event.rx_mgmt.frame = buf;
	event.rx_mgmt.frame_len = len;
	event.rx_mgmt.ssi_signal = ssi_signal;
	event.rx_mgmt.datarate = datarate;
	hapd->ext_mgmt_frame_handling = 0;
	wpa_supplicant_event(hapd, EVENT_RX_MGMT, &event);
	hapd->ext_mgmt_frame_handling = 1;

	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_eapol_rx(struct hostapd_data *hapd, char *cmd)
{
	char *pos;
	u8 src[ETH_ALEN], *buf;
	int used;
	size_t len;

	wpa_printf(MSG_DEBUG, "External EAPOL RX: %s", cmd);

	pos = cmd;
	used = hwaddr_aton2(pos, src);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;

	len = os_strlen(pos);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(pos, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	ieee802_1x_receive(hapd, src, buf, len, FRAME_ENCRYPTION_UNKNOWN);
	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_eapol_tx(struct hostapd_data *hapd, char *cmd)
{
	char *pos, *pos2;
	u8 dst[ETH_ALEN], *buf;
	int used, ret;
	size_t len;
	unsigned int prev;
	int encrypt = 0;

	wpa_printf(MSG_DEBUG, "External EAPOL TX: %s", cmd);

	pos = cmd;
	used = hwaddr_aton2(pos, dst);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;

	pos2 = os_strchr(pos, ' ');
	if (pos2) {
		len = pos2 - pos;
		encrypt = os_strstr(pos2, "encrypt=1") != NULL;
	} else {
		len = os_strlen(pos);
	}
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (!buf || hexstr2bin(pos, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	prev = hapd->ext_eapol_frame_io;
	hapd->ext_eapol_frame_io = 0;
	ret = hostapd_wpa_auth_send_eapol(hapd, dst, buf, len, encrypt);
	hapd->ext_eapol_frame_io = prev;
	os_free(buf);

	return ret;
}


static u16 ipv4_hdr_checksum(const void *buf, size_t len)
{
	size_t i;
	u32 sum = 0;
	const u16 *pos = buf;

	for (i = 0; i < len / 2; i++)
		sum += *pos++;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return sum ^ 0xffff;
}


#define HWSIM_PACKETLEN 1500
#define HWSIM_IP_LEN (HWSIM_PACKETLEN - sizeof(struct ether_header))

static void hostapd_data_test_rx(void *ctx, const u8 *src_addr, const u8 *buf,
				 size_t len)
{
	struct hostapd_data *hapd = ctx;
	const struct ether_header *eth;
	struct ip ip;
	const u8 *pos;
	unsigned int i;
	char extra[30];

	if (len < sizeof(*eth) + sizeof(ip) || len > HWSIM_PACKETLEN) {
		wpa_printf(MSG_DEBUG,
			   "test data: RX - ignore unexpected length %d",
			   (int) len);
		return;
	}

	eth = (const struct ether_header *) buf;
	os_memcpy(&ip, eth + 1, sizeof(ip));
	pos = &buf[sizeof(*eth) + sizeof(ip)];

	if (ip.ip_hl != 5 || ip.ip_v != 4 ||
	    ntohs(ip.ip_len) > HWSIM_IP_LEN) {
		wpa_printf(MSG_DEBUG,
			   "test data: RX - ignore unexpected IP header");
		return;
	}

	for (i = 0; i < ntohs(ip.ip_len) - sizeof(ip); i++) {
		if (*pos != (u8) i) {
			wpa_printf(MSG_DEBUG,
				   "test data: RX - ignore mismatching payload");
			return;
		}
		pos++;
	}

	extra[0] = '\0';
	if (ntohs(ip.ip_len) != HWSIM_IP_LEN)
		os_snprintf(extra, sizeof(extra), " len=%d", ntohs(ip.ip_len));
	wpa_msg(hapd->msg_ctx, MSG_INFO, "DATA-TEST-RX " MACSTR " " MACSTR "%s",
		MAC2STR(eth->ether_dhost), MAC2STR(eth->ether_shost), extra);
}


static int hostapd_ctrl_iface_data_test_config(struct hostapd_data *hapd,
					       char *cmd)
{
	int enabled = atoi(cmd);
	char *pos;
	const char *ifname;
	const u8 *addr = hapd->own_addr;

	if (!enabled) {
		if (hapd->l2_test) {
			l2_packet_deinit(hapd->l2_test);
			hapd->l2_test = NULL;
			wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
				"test data: Disabled");
		}
		return 0;
	}

	if (hapd->l2_test)
		return 0;

	pos = os_strstr(cmd, " ifname=");
	if (pos)
		ifname = pos + 8;
	else
		ifname = hapd->conf->iface;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */
	hapd->l2_test = l2_packet_init(ifname, addr,
					ETHERTYPE_IP, hostapd_data_test_rx,
					hapd, 1);
	if (hapd->l2_test == NULL)
		return -1;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: Enabled");

	return 0;
}


static int hostapd_ctrl_iface_data_test_tx(struct hostapd_data *hapd, char *cmd)
{
	u8 dst[ETH_ALEN], src[ETH_ALEN];
	char *pos, *pos2;
	int used;
	long int val;
	u8 tos;
	u8 buf[2 + HWSIM_PACKETLEN];
	struct ether_header *eth;
	struct ip *ip;
	u8 *dpos;
	unsigned int i;
	size_t send_len = HWSIM_IP_LEN;

	if (hapd->l2_test == NULL)
		return -1;

	/* format: <dst> <src> <tos> [len=<length>] */

	pos = cmd;
	used = hwaddr_aton2(pos, dst);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;
	used = hwaddr_aton2(pos, src);
	if (used < 0)
		return -1;
	pos += used;

	val = strtol(pos, &pos2, 0);
	if (val < 0 || val > 0xff)
		return -1;
	tos = val;

	pos = os_strstr(pos2, " len=");
	if (pos) {
		i = atoi(pos + 5);
		if (i < sizeof(*ip) || i > HWSIM_IP_LEN)
			return -1;
		send_len = i;
	}

	eth = (struct ether_header *) &buf[2];
	os_memcpy(eth->ether_dhost, dst, ETH_ALEN);
	os_memcpy(eth->ether_shost, src, ETH_ALEN);
	eth->ether_type = htons(ETHERTYPE_IP);
	ip = (struct ip *) (eth + 1);
	os_memset(ip, 0, sizeof(*ip));
	ip->ip_hl = 5;
	ip->ip_v = 4;
	ip->ip_ttl = 64;
	ip->ip_tos = tos;
	ip->ip_len = htons(send_len);
	ip->ip_p = 1;
	ip->ip_src.s_addr = htonl(192U << 24 | 168 << 16 | 1 << 8 | 1);
	ip->ip_dst.s_addr = htonl(192U << 24 | 168 << 16 | 1 << 8 | 2);
	ip->ip_sum = ipv4_hdr_checksum(ip, sizeof(*ip));
	dpos = (u8 *) (ip + 1);
	for (i = 0; i < send_len - sizeof(*ip); i++)
		*dpos++ = i;

	if (l2_packet_send(hapd->l2_test, dst, ETHERTYPE_IP, &buf[2],
			   sizeof(struct ether_header) + send_len) < 0)
		return -1;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: TX dst=" MACSTR
		" src=" MACSTR " tos=0x%x", MAC2STR(dst), MAC2STR(src), tos);

	return 0;
}


static int hostapd_ctrl_iface_data_test_frame(struct hostapd_data *hapd,
					      char *cmd)
{
	u8 *buf;
	struct ether_header *eth;
	struct l2_packet_data *l2 = NULL;
	size_t len;
	u16 ethertype;
	int res = -1;
	const char *ifname = hapd->conf->iface;

	if (os_strncmp(cmd, "ifname=", 7) == 0) {
		cmd += 7;
		ifname = cmd;
		cmd = os_strchr(cmd, ' ');
		if (cmd == NULL)
			return -1;
		*cmd++ = '\0';
	}

	len = os_strlen(cmd);
	if (len & 1 || len < ETH_HLEN * 2)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(cmd, buf, len) < 0)
		goto done;

	eth = (struct ether_header *) buf;
	ethertype = ntohs(eth->ether_type);

	l2 = l2_packet_init(ifname, hapd->own_addr, ethertype,
			    hostapd_data_test_rx, hapd, 1);
	if (l2 == NULL)
		goto done;

	res = l2_packet_send(l2, eth->ether_dhost, ethertype, buf, len);
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: TX frame res=%d", res);
done:
	if (l2)
		l2_packet_deinit(l2);
	os_free(buf);

	return res < 0 ? -1 : 0;
}


static int hostapd_ctrl_reset_pn(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	u8 zero[WPA_TK_MAX_LEN];

	os_memset(zero, 0, sizeof(zero));

	if (hwaddr_aton(cmd, addr))
		return -1;

	if (is_broadcast_ether_addr(addr) && os_strstr(cmd, " BIGTK")) {
		if (hapd->last_bigtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset BIPN for BIGTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_bigtk_alg,
					broadcast_ether_addr,
					hapd->last_bigtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_bigtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_bigtk_alg,
					   broadcast_ether_addr,
					   hapd->last_bigtk_key_idx, 0, 1, NULL,
					   0, hapd->last_bigtk,
					   hapd->last_bigtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	if (is_broadcast_ether_addr(addr) && os_strstr(cmd, "IGTK")) {
		if (hapd->last_igtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset IPN for IGTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_igtk_alg,
					broadcast_ether_addr,
					hapd->last_igtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_igtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_igtk_alg,
					   broadcast_ether_addr,
					   hapd->last_igtk_key_idx, 0, 1, NULL,
					   0, hapd->last_igtk,
					   hapd->last_igtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	if (is_broadcast_ether_addr(addr)) {
		if (hapd->last_gtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset PN for GTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_gtk_alg,
					broadcast_ether_addr,
					hapd->last_gtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_gtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_gtk_alg,
					   broadcast_ether_addr,
					   hapd->last_gtk_key_idx, 0, 1, NULL,
					   0, hapd->last_gtk,
					   hapd->last_gtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	if (sta->last_tk_alg == WPA_ALG_NONE)
		return -1;

	wpa_printf(MSG_INFO, "TESTING: Reset PN for " MACSTR,
		   MAC2STR(sta->addr));

	/* First, use a zero key to avoid any possible duplicate key avoidance
	 * in the driver. */
	if (hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
				sta->addr, sta->last_tk_key_idx, 0, 1, NULL, 0,
				zero, sta->last_tk_len,
				KEY_FLAG_PAIRWISE_RX_TX) < 0)
		return -1;

	/* Set the previously configured key to reset its TSC/RSC */
	return hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
				   sta->addr, sta->last_tk_key_idx, 0, 1, NULL,
				   0, sta->last_tk, sta->last_tk_len,
				   KEY_FLAG_PAIRWISE_RX_TX);
}
#endif /* CONFIG_TESTING_OPTIONS */


#if defined(CONFIG_QCN_EXTN) || defined(CONFIG_TESTING_OPTIONS)
static int hostapd_ctrl_set_key(struct hostapd_data *hapd, const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *pos = cmd;
	enum wpa_alg alg;
	enum key_flag key_flag;
	int idx, set_tx;
	u8 seq[6], key[WPA_TK_MAX_LEN];
	size_t key_len;

	/* parameters: alg addr idx set_tx seq key key_flag */

	alg = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	if (hwaddr_aton(pos, addr))
		return -1;
	pos += 17;
	if (*pos != ' ')
		return -1;
	pos++;
	idx = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	set_tx = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	if (hexstr2bin(pos, seq, sizeof(seq)) < 0)
		return -1;
	pos += 2 * 6;
	if (*pos != ' ')
		return -1;
	pos++;
	if (!os_strchr(pos, ' '))
		return -1;
	key_len = (os_strchr(pos, ' ') - pos) / 2;
	if (hexstr2bin(pos, key, key_len) < 0)
		return -1;
	pos += 2 * key_len;
	if (*pos != ' ')
		return -1;

	pos++;
	key_flag = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (pos)
		return -1;

	wpa_printf(MSG_INFO, "TESTING: Set key");
	return hostapd_drv_set_key(hapd->conf->iface, hapd, alg, addr, idx, 0,
				   set_tx, seq, 6, key, key_len, key_flag);
}
#endif /* CONFIG_QCN_EXTN || CONFIG_TESTING_OPTIONS */


#ifdef CONFIG_TESTING_OPTIONS
static void restore_tk(void *ctx1, void *ctx2)
{
	struct hostapd_data *hapd = ctx1;
	struct sta_info *sta = ctx2;

	wpa_printf(MSG_INFO, "TESTING: Restore TK for " MACSTR,
		   MAC2STR(sta->addr));
	/* This does not really restore the TSC properly, so this will result
	 * in replay protection issues for now since there is no clean way of
	 * preventing encryption of a single EAPOL frame. */
	hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
			    sta->addr, sta->last_tk_key_idx, 0, 1, NULL, 0,
			    sta->last_tk, sta->last_tk_len,
			    KEY_FLAG_PAIRWISE_RX_TX);
}


static int hostapd_ctrl_resend_m1(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO, "TESTING: Send M1 to " MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_m1(sta->wpa_sm,
				  os_strstr(cmd, "change-anonce") != NULL,
				  plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_resend_m3(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO, "TESTING: Send M3 to " MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_m3(sta->wpa_sm,
				  plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_resend_group_m1(struct hostapd_data *hapd,
					const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO,
		   "TESTING: Send group M1 for the same GTK and zero RSC to "
		   MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_group_m1(sta->wpa_sm,
					plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_rekey_ptk(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	return wpa_auth_rekey_ptk(hapd->wpa_auth, sta->wpa_sm);
}


static int hostapd_ctrl_get_pmksa_pmk(struct hostapd_data *hapd, const u8 *addr,
				      char *buf, size_t buflen)
{
	struct rsn_pmksa_cache_entry *pmksa;

	pmksa = wpa_auth_pmksa_get(hapd->wpa_auth, addr, NULL);
	if (!pmksa)
		return -1;

	return wpa_snprintf_hex(buf, buflen, pmksa->pmk, pmksa->pmk_len);
}


static int hostapd_ctrl_get_pmk(struct hostapd_data *hapd, const char *cmd,
				char *buf, size_t buflen)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	const u8 *pmk;
	int pmk_len;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm) {
		wpa_printf(MSG_DEBUG, "No STA WPA state machine for " MACSTR,
			   MAC2STR(addr));
		return hostapd_ctrl_get_pmksa_pmk(hapd, addr, buf, buflen);
	}
	pmk = wpa_auth_get_pmk(sta->wpa_sm, &pmk_len);
	if (!pmk || !pmk_len) {
		wpa_printf(MSG_DEBUG, "No PMK stored for " MACSTR,
			   MAC2STR(addr));
		return hostapd_ctrl_get_pmksa_pmk(hapd, addr, buf, buflen);
	}

	return wpa_snprintf_hex(buf, buflen, pmk, pmk_len);
}


static int hostapd_ctrl_register_frame(struct hostapd_data *hapd,
				       const char *cmd)
{
	u16 type;
	char *pos, *end;
	u8 match[10];
	size_t match_len;
	bool multicast = false;

	type = strtol(cmd, &pos, 16);
	if (*pos != ' ')
		return -1;
	pos++;
	end = os_strchr(pos, ' ');
	if (end) {
		match_len = end - pos;
		multicast = os_strstr(end, "multicast") != NULL;
	} else {
		match_len = os_strlen(pos) / 2;
	}
	if (hexstr2bin(pos, match, match_len))
		return -1;

	return hostapd_drv_register_frame(hapd, type, match, match_len,
					  multicast);
}

#endif /* CONFIG_TESTING_OPTIONS */

static int hostapd_check_validity_device_params(struct hostapd_freq_params *params)
{
	int freq_section = 0;

	if (params->bandwidth_device == 0 && params->center_freq_device == 0)
		return 0;
	else if (params->bandwidth_device == 0 || params->center_freq_device == 0)
		return -EINVAL;

	if ((params->center_freq1 == params->center_freq_device) &&
	    (params->bandwidth == params->bandwidth_device)) {
		params->bandwidth_device = 0;
		params->center_freq_device = 0;
		return 0;
	}

	if (params->bandwidth_device != 2*params->bandwidth) {
		wpa_printf(MSG_ERROR,
			   "Device bandwidth is not set to twice the operating bandwidth\n");
		return -EINVAL;
	}

	if (params->center_freq1 > params->center_freq_device)
		freq_section = 1;

	switch (params->bandwidth_device) {
	case 320:
		if (freq_section) {
			if (params->center_freq_device != params->center_freq1 - 80)
				return -EINVAL;
		} else {
			if (params->center_freq_device != params->center_freq1 + 80)
				return -EINVAL;
		}
		break;
	case 160:
		if (freq_section) {
			if (params->center_freq_device != params->center_freq1 - 40)
				return -EINVAL;
		} else {
			if (params->center_freq_device != params->center_freq1 + 40)
				return -EINVAL;
		}
		break;
	case 80:
		if (freq_section) {
			if (params->center_freq_device != params->center_freq1 - 20)
				return -EINVAL;
		} else {
			if (params->center_freq_device != params->center_freq1 + 20)
				return -EINVAL;
		}
		break;
	case 40:
		if (freq_section) {
			if (params->center_freq_device != params->center_freq1 - 10)
				return -EINVAL;
		} else {
			if (params->center_freq_device != params->center_freq1 + 10)
				return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int hostapd_ctrl_iface_set_pwr_mode(struct hostapd_iface *iface,
					   char *pos)
{
#ifdef NEED_AP_MLME
	struct he_6ghz_pwr_mode_settings settings;
	int ret = 0, he_6ghz_pwr_mode, err = 0;
	unsigned int i, num_err = 0;
	char *end;

	if (iface->power_mode_6ghz_before_change > -1) {
		wpa_printf(MSG_ERROR, "Power mode change in progress");
		return -1;
	}

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_ERROR, "Channel switch in progress");
		return -1;
	}

	os_memset(&settings, 0, sizeof(settings));
	he_6ghz_pwr_mode = strtol(pos, &end, 10);
	if (pos == end || he_6ghz_pwr_mode < 0 || he_6ghz_pwr_mode > 2) {
		wpa_printf(MSG_ERROR, "Invalid power mode provided");
		return -1;
	}

	if (!is_6ghz_freq(iface->freq)) {
		wpa_printf(MSG_ERROR, "set_pwr_mode is only for 6 GHz");
		return -1;
	}

	if (he_6ghz_pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_SP &&
	    !iface->is_afc_power_event_received) {
		wpa_printf(MSG_ERROR, "Standard Power mode cant be set without AFC");
		return -1;
	}

	if (he_6ghz_pwr_mode == HE_REG_INFO_6GHZ_AP_TYPE_SP &&
	    !iface->is_afc_power_event_received) {
		wpa_printf(MSG_ERROR, "Standard Power mode cant be set without AFC");
		return -1;
	}

	if (iface->conf->enable_best_power_mode) {
		u16 bw, center_freq;
		u8 best_power_mode = NL80211_REG_NUM_POWER_MODES;
		bool valid;
		enum chan_width ch_width = hostapd_get_chan_width_from_oper_chan_width(iface->conf);
		u8 center_chan_no = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);

		bw = channel_width_to_int(ch_width);
		center_freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
						     center_chan_no);
		valid = hostapd_validate_chan_bw_in_pwr_mode(iface,
							     iface->freq, center_freq,
							     bw, iface->conf->punct_bitmap,
							     he_6ghz_pwr_mode);
		wpa_printf(MSG_DEBUG,
			   "%s: Power mode %d for Freq %d is valid - %d",
			   __func__, he_6ghz_pwr_mode, iface->freq, valid);

		if (!valid) {
			wpa_printf(MSG_ERROR, "Fallback to best power mode");
			best_power_mode = hostapd_get_best_ap_6ghz_power_mode_for_iface(iface);
			if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
				wpa_printf(MSG_INFO,
						"%s: Best power mode for Freq %d is %d",
						__func__, iface->freq, best_power_mode);
				he_6ghz_pwr_mode = best_power_mode;
			} else {
				wpa_printf(MSG_ERROR, "Cannot find power mode");
				return -1;
			}
		}
	}

	settings.pwr_mode = he_6ghz_pwr_mode;
	settings.link_id = -1;

	iface->power_mode_6ghz_before_change = he_6ghz_pwr_mode;
	wpa_printf(MSG_DEBUG, "Setting user selected 6 GHz power mode: %d\n",
		   he_6ghz_pwr_mode);

	for (i = 0; i < iface->num_bss; i++) {
#ifdef CONFIG_IEEE80211BE
		if (iface->bss[i]->conf->mld_ap)
			settings.link_id = iface->bss[i]->mld_link_id;
		else
			settings.link_id = -1;
#endif /* CONFIG_IEEE80211BE */

		err = hostapd_drv_set_6ghz_pwr_mode(iface->bss[i], &settings);
		if (err) {
			ret = err;
			num_err++;
		}
	}

	if (iface->num_bss != num_err)
		return 0;

	iface->power_mode_6ghz_before_change = -1;

	return ret;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}

static int hostapd_ctrl_iface_chan_switch(struct hostapd_iface *iface,
					  char *pos)
{
#ifdef NEED_AP_MLME
	struct csa_settings settings;
	int ret;
	int dfs_range = 0;
	unsigned int i;
	int bandwidth;
	u8 chan;
	unsigned int num_err = 0;
	int err = 0;

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_ERROR, "CSA Request skipped, a Channel switch is already in progress");
		return -1;
	}

	ret = hostapd_parse_csa_settings(iface, pos, &settings);
	if (ret)
		return ret;

#ifdef CONFIG_QCN_EXTN
	if (!settings.freq_params.rptr_mgr &&
		hostapd_is_bh_sta_connecting_or_connected_extn(iface)) {
		if (iface->conf->conf_extn.rptr_allow_chan_sw) {
			wpa_printf(MSG_DEBUG,
				   "chanswitch: BH STA in connecting or connected"
				   " state, disconnect BH STA channel switch"
				   " and allow channel switch");
			hostapd_ucode_trigger_bhsta_disconnect(iface);
		} else {
			wpa_printf(MSG_ERROR,
				   "chanswitch: BH STA in connecting or"
				   " connected state, aborting channel switch");
			return -1;
		}
	}

	if (!hostapd_is_chan_in_primary_list(iface,
					     (u16)settings.freq_params.freq)) {
		wpa_printf(MSG_ERROR,
			   "chanswitch: freq %d not in primary channel list,"
			   " rejecting channel switch",
			   settings.freq_params.freq);
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

	settings.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	/* Reject if EHT is disabled in channel switch settings but the
	 * interface has a BSS affiliated with an AP MLD where EHT is mandatory
	 * to be enabled. */
	if (!settings.freq_params.eht_enabled) {
		for (i = 0; i < iface->num_bss; i++) {
			if (iface->bss[i]->conf->mld_ap) {
				wpa_printf(MSG_INFO,
					   "Do not allow EHT to be disabled when the interface has an ML BSS");
				return -1;
			}
		}
	}

	if (iface->num_bss && iface->bss[0]->conf->mld_ap)
		settings.link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211BN
	/* Reject if UHR is disabled in channel switch settings but the
	 * interface has a BSS with UHR enabled, since UHR is a superset of
	 * EHT and silently disabling it would downgrade the VAP after CSA. */
	if (!settings.freq_params.uhr_enabled) {
		for (i = 0; i < iface->num_bss; i++) {
			if (!iface->bss[i])
				continue;
			if (hostapd_is_uhr_enabled(iface->bss[i])) {
				wpa_printf(MSG_ERROR,
					   "chanswitch: Do not allow UHR to be disabled"
					   " when VAP %s is operating in UHR mode",
					   iface->bss[i]->conf->iface);
				return -1;
			}
		}
	}
#endif /* CONFIG_IEEE80211BN */
	if (settings.power_mode == HE_REG_INFO_6GHZ_AP_TYPE_SP &&
	    !iface->is_afc_power_event_received) {
		wpa_printf(MSG_ERROR, "Standard Power mode cant be set without AFC");
		return -1;
	}

	if (settings.power_mode == -1 || settings.power_mode == HE_REG_INFO_6GHZ_AP_TYPE_SP) {
		bool is_bpm_needed = iface->conf->enable_best_power_mode;

		if (settings.power_mode == HE_REG_INFO_6GHZ_AP_TYPE_SP)
			is_bpm_needed = false;

		if (hostapd_allow_6ghz_dynamic_puncture(iface, settings.freq_params.freq,
							settings.power_mode)) {
			u16 best_6ghz_pp = settings.freq_params.punct_bitmap;

			if (!hostapd_get_6ghz_best_pp(iface, settings.freq_params.freq,
						      settings.freq_params.center_freq1,
						      settings.freq_params.bandwidth,
						      &best_6ghz_pp,
						      is_bpm_needed)) {
				settings.freq_params.punct_bitmap = best_6ghz_pp;
			}
		}
	}

	if (iface->power_mode_6ghz_before_change > -1) {
		wpa_printf(MSG_ERROR, "Power mode change in progress");
		return -1;
	}

	if (is_6ghz_freq(settings.freq_params.freq) && iface->conf->enable_best_power_mode) {
		bool valid = false;
		u8 best_power_mode;

		if (settings.power_mode != -1) {
			valid =
			    hostapd_validate_chan_bw_in_pwr_mode(iface,
								 settings.freq_params.freq,
								 settings.freq_params.center_freq1,
								 settings.freq_params.bandwidth,
								 settings.freq_params.punct_bitmap,
								 settings.power_mode);
			wpa_printf(MSG_DEBUG,
				   "%s: Power mode %d for Freq %d is valid - %d",
				   __func__,
				   settings.power_mode,
				   settings.freq_params.freq,
				   valid);
		}

		if (!valid) {
			wpa_printf(MSG_ERROR, "Fallback to best power mode");
			best_power_mode =
			    hostapd_get_best_ap_6ghz_power_mode(iface,
								settings.freq_params.freq,
								settings.freq_params.center_freq1,
								settings.freq_params.bandwidth,
								settings.freq_params.punct_bitmap);
			if (best_power_mode != NL80211_REG_NUM_POWER_MODES) {
				wpa_printf(MSG_DEBUG,
					   "%s: Best power mode for Freq %d is %d",
					   __func__,
					   settings.freq_params.freq,
					   best_power_mode);
				settings.power_mode = best_power_mode;
			} else {
				wpa_printf(MSG_ERROR,
					   "No Valid power mode for Freq %d",
					   settings.freq_params.freq);
				return -1;
			}
		}
	} else if (is_6ghz_freq(settings.freq_params.freq) &&
		   !iface->conf->enable_best_power_mode) {
		hostapd_set_current_6ghz_pwr_type(iface, &settings.power_mode);
		wpa_printf(MSG_DEBUG,
			   "%s: Using configured power mode %d for Freq %d (BPM disabled)",
			   __func__, settings.power_mode,
			   settings.freq_params.freq);
	}

	ret = hostapd_check_validity_device_params(&settings.freq_params);
	if (ret) {
		wpa_printf(MSG_ERROR, "chanswitch: invalid device parameters provided %d %d",
			   settings.freq_params.bandwidth_device,
			   settings.freq_params.center_freq_device);
		return ret;
	}

	switch (settings.freq_params.bandwidth) {
	case 40:
		bandwidth = CHAN_WIDTH_40;
		break;
	case 80:
		if (settings.freq_params.center_freq2)
			bandwidth = CHAN_WIDTH_80P80;
		else
			bandwidth = CHAN_WIDTH_80;
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

	if (iface->conf->use_ru_puncture_dfs) {
		if (!hostapd_dfs_csa_target_has_unavailable_channel(iface,
								    &settings.freq_params,
								    bandwidth)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: Update puncture source for User puncture bitmap=0x%04x",
				   settings.freq_params.punct_bitmap);
			dfs_update_puncture_source(iface,
						   settings.freq_params.center_freq1,
						   bandwidth,
						   settings.freq_params.punct_bitmap,
						   DFS_CHAN_PUNC_USER);
		} else {
			wpa_printf(MSG_ERROR,
				   "DFS: Invalid puncture bitmap=0x%04x",
				   settings.freq_params.punct_bitmap);
			return -1;
		}
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
		ret = ieee80211_freq_to_chan(settings.freq_params.freq, &chan);
		if (ret == NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_ERROR,
				   "Failed to get channel for (freq=%d, sec_channel_offset=%d, bw=%d)",
				   settings.freq_params.freq,
				   settings.freq_params.sec_channel_offset,
				   settings.freq_params.bandwidth);
			return -1;
		}

		if (hostapd_dfs_csa_target_has_unavailable_channel(iface, &settings.freq_params,
								   bandwidth)) {
			wpa_printf(MSG_ERROR,
				   "chanswitch: target DFS channel(s) unavailable");
			return -1;
		}

		if (iface->conf->disable_csa_dfs == 1) {
			wpa_printf(MSG_DEBUG, "chanswitch interface %s : cancel radar handling timer",
				   iface->conf->bss[0]->iface);
			eloop_cancel_timeout(hostapd_dfs_radar_handling_timeout, iface, NULL);
		}

		settings.freq_params.channel = chan;

		wpa_printf(MSG_DEBUG,
			   "DFS/CAC to (channel=%u, freq=%d, sec_channel_offset=%d, bw=%d, center_freq1=%d)",
			   settings.freq_params.channel,
			   settings.freq_params.freq,
			   settings.freq_params.sec_channel_offset,
			   settings.freq_params.bandwidth,
			   settings.freq_params.center_freq1);

		/* Perform CAC and switch channel */
		iface->is_ch_switch_dfs = true;

		if (!(iface->drv_flags2 & WPA_DRIVER_FLAGS2_DFS_CHANNEL_SWITCH)) {
			hostapd_switch_channel_fallback(iface, &settings.freq_params);
			return 0;
		}
	}

	if (iface->conf->disable_csa_dfs == 1) {
		wpa_printf(MSG_DEBUG, "chanswitch interface %s : cancel radar handling timer",
			   iface->conf->bss[0]->iface);
		eloop_cancel_timeout(hostapd_dfs_radar_handling_timeout, iface, NULL);
	}

	hostapd_get_channel_switch_time(iface, &settings.freq_params);

#ifdef CONFIG_QCN_EXTN
	if (iface->cac_started || iface->bootup_cac_in_progress)
#else
	if (iface->cac_started)
#endif /* CONFIG_QCN_EXTN */
		return hostapd_abort_cac_for_channel_switch(iface, &settings);

	/*
	 * When radar fires while CAC is active, ieee80211_abort_cac() releases
	 * the link's channel context. A subsequent NL80211_CMD_CHANNEL_SWITCH
	 * is rejected with -ENOTCONN (-107). Use force_channel_switch only
	 * in that case, identified by state==HAPD_IFACE_DFS: the CAC_ABORTED
	 * event leaves state at DFS, whereas a successful CAC transitions state
	 * to ENABLED before the CHANSWITCH command arrives.
	 *
	 * When radar fires after CAC completes (state==HAPD_IFACE_ENABLED),
	 * the chanctx is intact and NL80211_CMD_CHANNEL_SWITCH works fine.
	 * force_channel_switch must NOT be used there: the disable/re-enable
	 * cycle triggers DFS_PRE_CAC_EXPIRED for all previously-cleared
	 * channels, resetting them to USABLE and causing a spurious CAC
	 * restart on the next DFS_NOP_FINISHED event.
	 */
	if (iface->conf->disable_csa_dfs == 1 &&
	    iface->state == HAPD_IFACE_DFS)
		return hostapd_force_channel_switch(iface, &settings);

	/* Trigger mesh CSA before AP channel switch if mesh VAP present */
	hostapd_ubus_mesh_switch_channel(iface, &settings);
	for (i = 0; i < iface->num_bss; i++) {

		/* Save CHAN_SWITCH VHT, HE, and EHT config */
		hostapd_chan_switch_config(iface->bss[i],
					   &settings.freq_params);

		err = hostapd_switch_channel(iface->bss[i], &settings);
		if (err) {
			ret = err;
			num_err++;
		}
	}

	return (iface->num_bss == num_err) ? ret : 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


#ifdef CONFIG_IEEE80211AX
extern void hostapd_switch_color_timeout_handler(void *eloop_data,
						 void *user_ctx);

static int hostapd_ctrl_iface_bsscolor_collision_ap_period(struct hostapd_iface *iface,
							   const char *pos)
{
#ifdef NEED_AP_MLME
	unsigned int i;
	long period;
	char *end;

	period = strtol(pos, &end, 10);
	if (pos == end || period < 50 || period > 255) {
		wpa_printf(MSG_ERROR,
			   "bsscolor_collision_ap_period: Invalid period (valid range 50..255)");
		return -1;
	}

	iface->conf->he_bss_color_collision_ap_period = (u16)period;

	wpa_printf(MSG_INFO, "COLOR_COLLISION_AP_PERIOD set to %ld",
		   period);

	for (i = 0; i < iface->num_bss; i++)
		eloop_replenish_timeout(period, 0, hostapd_switch_color_timeout_handler,
					iface->bss[i], NULL);

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}

static int hostapd_ctrl_iface_bsscolor_cca_count(struct hostapd_iface *iface,
						 const char *pos)
{
#ifdef NEED_AP_MLME
	long count;
	char *end;
	int i;

	count = strtol(pos, &end, 10);
	if (pos == end || count < 3 || count > 100) {
		wpa_printf(MSG_ERROR,
			   "bsscolor_cca_count: Invalid count (valid range 3..100)");
		return -1;
	}

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		/* Store configured CCA count directly */
		bss->cca_count = (u8) count;
	}

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


static int hostapd_ctrl_iface_color_change(struct hostapd_iface *iface,
					   const char *pos)
{
#ifdef NEED_AP_MLME
	struct cca_settings settings;
	int ret, color;
	unsigned int i;
	char *end;
	struct hostapd_data *link_bss;

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_ERROR, "Channel switch in progress");
		return -1;
	}

	os_memset(&settings, 0, sizeof(settings));

	color = strtol(pos, &end, 10);
	if (pos == end || color < 0 || color > 63) {
		wpa_printf(MSG_ERROR, "color_change: Invalid color provided");
		return -1;
	}

	/* Color value is expected to be [1-63]. If 0 comes, assumption is this
	 * is to disable the color. In this case no need to do CCA, just
	 * changing Beacon frames is sufficient. */
	if (color == 0) {
		if (iface->conf->he_op.he_bss_color_disabled) {
			wpa_printf(MSG_ERROR,
				   "color_change: Color is already disabled");
			return -1;
		}

		iface->conf->he_op.he_bss_color_disabled = 1;

		for (i = 0; i < iface->num_bss; i++)
			ieee802_11_set_beacon(iface->bss[i]);

		return 0;
	}

	if (color == iface->conf->he_op.he_bss_color) {
		if (!iface->conf->he_op.he_bss_color_disabled) {
			wpa_printf(MSG_ERROR,
				   "color_change: Provided color is already set");
			return -1;
		}

		iface->conf->he_op.he_bss_color_disabled = 0;

		for (i = 0; i < iface->num_bss; i++)
			ieee802_11_set_beacon(iface->bss[i]);

		return 0;
	}

	if (hostapd_is_cca_in_progress(iface)) {
		wpa_printf(MSG_ERROR,
			   "color_change: CCA is already in progress");
		return -1;
	}

	iface->conf->he_op.he_bss_color_disabled = 0;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];

		hostapd_cleanup_cca_params(bss);

		bss->cca_color = color;
		/* Use radio-level default CCA count unless overridden */
		bss->cca_count = bss->cca_count > 0 ?
				 bss->cca_count : HE_BSS_COLOR_CCA_COUNT_DEFAULT;

		if (hostapd_fill_cca_settings(bss, &settings)) {
			wpa_printf(MSG_DEBUG,
				   "color_change: Filling CCA settings failed for color: %d\n",
				   color);
			hostapd_cleanup_cca_params(bss);
			continue;
		}
		eloop_cancel_timeout(hostapd_switch_color_timeout_handler, bss, NULL);

		wpa_printf(MSG_DEBUG, "Setting user selected color: %d", color);
		ret = hostapd_drv_switch_color(bss, &settings);
		if (ret)
			hostapd_cleanup_cca_params(bss);

#ifdef CONFIG_QCN_EXTN
		if (!hostapd_is_repurpose_disabled_11be_extn(bss->conf)) {
#endif /* CONFIG_QCN_EXTN */
		if (!ret && bss->conf->mld_ap) {
			/* Generate per sta profiles for affiliated APs */
			for_each_mld_link(link_bss, bss) {
				if (bss == link_bss)
					continue;
				hostapd_gen_per_sta_profiles(link_bss);
			}
		}
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */

		free_beacon_data(&settings.beacon_cca);
		free_beacon_data(&settings.beacon_after);
	}

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}
#endif /* CONFIG_IEEE80211AX */


u8 hostapd_maxnss(struct hostapd_data *hapd, struct sta_info *sta)
{
	u8 nss = 0;
	u8 rx_nss = 1;
	u8 mcs_count;
	u16 rx_mcs_set;
	int i, j;
	const u16 *ap_mcs_set = NULL;
	const u8 *mcs_set = NULL;
	struct hostapd_config *conf = hapd->iface->conf;
	struct hostapd_hw_modes *mode = NULL;
	u8 support_check[MAXNSS_HTMODE_MAX] = {};
	u8 htmode = MAXNSS_HTMODE_UNSET;

	if (sta) {
		htmode =  (!!(sta->flags & WLAN_STA_HT)) |
			((!!(sta->flags & WLAN_STA_VHT)) << 1) |
			((!!(sta->flags & WLAN_STA_HE)) << 2) |
			((!!(sta->flags & WLAN_STA_EHT)) << 3);
		support_check[MAXNSS_HTMODE_HT_N] = !!sta->ht_capabilities;
		support_check[MAXNSS_HTMODE_VHT_AC] = !!sta->vht_capabilities;
		support_check[MAXNSS_HTMODE_EHT_BE] = !!sta->eht_capab;
		support_check[MAXNSS_HTMODE_HE_AX] = !!sta->he_capab;
	} else {
		htmode = (!!conf->ieee80211n) |
			((!!conf->ieee80211ac) << 1) |
			((!!conf->ieee80211ax) << 2) |
			((!!conf->ieee80211be) << 3);
		support_check[MAXNSS_HTMODE_HT_N] = hostapd_is_ht_enabled(hapd);
		support_check[MAXNSS_HTMODE_VHT_AC] = hostapd_is_vht_enabled(hapd);
		support_check[MAXNSS_HTMODE_EHT_BE] = hostapd_is_eht_enabled(hapd);
		support_check[MAXNSS_HTMODE_HE_AX] = hostapd_is_he_enabled(hapd);
		mode = hapd->iface->current_mode;
	}
	htmode |= htmode >> 1;
	htmode |= htmode >> 2;
	htmode |= htmode >> 4;
	htmode++;
	htmode = htmode >> 1;
	if (!(htmode < MAXNSS_HTMODE_MAX) || !support_check[htmode] || (!sta && !mode))
		return rx_nss;
	switch (htmode) {
		case MAXNSS_HTMODE_HT_N:
			mcs_set = (sta) ? sta->ht_capabilities->supported_mcs_set : mode->mcs_set;
			return (!!mcs_set[0])  + (!!mcs_set[1]) + (!!mcs_set[2]) + (!!mcs_set[3]);
		case MAXNSS_HTMODE_VHT_AC:
			rx_mcs_set = (sta) ?
				le_to_host16(sta->vht_capabilities->vht_supported_mcs_set.rx_map) :
				mode->vht_mcs_set[4] | (mode->vht_mcs_set[5] << 8);
			for (i = VHT_RX_NSS_MAX_STREAMS - 1; i >= 0; i--) {
				if (((rx_mcs_set >> (2 * i)) & 0x03) != 0x03)
					return i + 1;
			}
			return rx_nss;
		case MAXNSS_HTMODE_EHT_BE:
			mcs_count = 1;
			mcs_set = (sta) ? sta->eht_capab->optional : mode->eht_capab[IEEE80211_MODE_AP].mcs;
			switch (conf->eht_oper_chwidth) {
				case CONF_OPER_CHWIDTH_320MHZ:
					mcs_count++;
					/* fall through */
				case CONF_OPER_CHWIDTH_80P80MHZ:
				case CONF_OPER_CHWIDTH_160MHZ:
					mcs_count++;
					break;
				default:
					break;
			}
			for (i = 0; i < mcs_count * EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS; i++) {
				nss = (mcs_set[i] & 0x000F);
				if (nss > rx_nss)
					rx_nss = nss;
			}
			return rx_nss;
		case MAXNSS_HTMODE_HE_AX:
			mcs_count = 0;
			ap_mcs_set =   (sta) ?  (u16 *) sta->he_capab->optional :
				(u16 *) mode->he_capab[IEEE80211_MODE_AP].mcs;
			switch (conf->he_oper_chwidth) {
				case CONF_OPER_CHWIDTH_80P80MHZ:
					mcs_count = 3;
					break;
				case CONF_OPER_CHWIDTH_160MHZ:
					mcs_count = 2;
					break;
				default:
					mcs_count = 1;
					break;
			}
			for (i = 0; i < mcs_count; i++) {
				rx_mcs_set = WPA_GET_LE16((const u8 *)&ap_mcs_set[(i * 2)]);
				for (j = HE_NSS_MAX_STREAMS - 1; j >= 0; j--) {
					if (((rx_mcs_set >> (2 * j)) & 0x03) != 0x03)
						return j + 1;
				}
			}
		default:
			return rx_nss;
	}
	return rx_nss;
}


static char hostapd_ctrl_iface_notify_cw_htaction(struct hostapd_data *hapd,
						  const u8 *addr, u8 width)
{
	u8 buf[3];
	char ret;

	width = width >= 1 ? 1 : 0;

	buf[0] = WLAN_ACTION_HT;
	buf[1] = WLAN_HT_ACTION_NOTIFY_CHANWIDTH;
	buf[2] = width;

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      buf, sizeof(buf));
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "Failed to send Notify Channel Width frame to "
			   MACSTR, MAC2STR(addr));

	return ret;
}


static char hostapd_ctrl_iface_notify_cw_vhtaction(struct hostapd_data *hapd,
						   const u8 *addr, u8 width)
{
	u8 buf[3];
	char ret;

	buf[0] = WLAN_ACTION_VHT;
	buf[1] = WLAN_VHT_ACTION_OPMODE_NOTIF;
	buf[2] = width;

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      buf, sizeof(buf));
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "Failed to send Opeating Mode Notification frame to "
			   MACSTR, MAC2STR(addr));

	return ret;
}


static char hostapd_ctrl_iface_notify_cw_change(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 cw, operating_mode = 0, nss;
	struct sta_info *sta;
	enum hostapd_hw_mode hw_mode;

	if (is_6ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR, "20/40 BSS coex not supported in 6 GHz");
		return -1;
	}

	cw = atoi(cmd);
	hw_mode = hapd->iface->current_mode->mode;
	if ((hw_mode == HOSTAPD_MODE_IEEE80211G ||
	     hw_mode == HOSTAPD_MODE_IEEE80211B) &&
	    !(cw == 0 || cw == 1)) {
		wpa_printf(MSG_ERROR,
			   "Channel width should be either 20 MHz or 40 MHz for 2.4 GHz band");
		return -1;
	}

	switch (cw) {
	case 0:
		operating_mode = 0;
		break;
	case 1:
		operating_mode = VHT_OPMODE_CHANNEL_40MHZ;
		break;
	case 2:
		operating_mode = VHT_OPMODE_CHANNEL_80MHZ;
		break;
	case 3:
		operating_mode = VHT_OPMODE_CHANNEL_160MHZ;
		break;
	default:
		wpa_printf(MSG_ERROR, "Channel width should be between 0 to 3");
		return -1;
	}

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
			nss = hostapd_maxnss(hapd, sta) - 1;
			hostapd_ctrl_iface_notify_cw_vhtaction(hapd, sta->addr,
							       operating_mode |
							       (u8) (nss << 4));
			continue;
		}

		if ((sta->flags & (WLAN_STA_HT | WLAN_STA_VHT)) ==
		    WLAN_STA_HT && sta->ht_capabilities)
			hostapd_ctrl_iface_notify_cw_htaction(hapd, sta->addr,
							      cw);
	}

	return 0;
}


#ifdef CONFIG_IEEE80211BN
static bool is_uhr_section_keyword(const char *s)
{
	return os_strncmp(s, "DPS ", 4) == 0 ||
	       os_strncmp(s, "DUO ", 4) == 0 ||
	       os_strncmp(s, "P_EDCA ", 7) == 0 ||
	       os_strncmp(s, "AP_PUO ", 7) == 0 ||
	       os_strncmp(s, "ELR ", 4) == 0;
}

static void
npca_params_from_mode(struct hostapd_uhr_npca_params *npca,
		      const struct uhr_npca_info *info)
{
	u32 params = 0;

	params |= ((u32)info->npca_min_dur_threshold <<
		   UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH_SHIFT) &
		  UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH;
	params |= ((u32)info->npca_switch_delay <<
		   UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY_SHIFT) &
		  UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY;
	params |= ((u32)info->npca_switch_back_delay <<
		   UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY_SHIFT) &
		  UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY;
	params |= ((u32)info->npca_initial_qsrc <<
		   UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC_SHIFT) &
		  UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC;
	if (info->npca_moplen)
		params |= UHR_OPER_PARAMS_NPCA_MOPLEN_NPCA;

	os_memset(npca, 0, sizeof(*npca));
	npca->params = params;
}

static int
parse_npca_params(struct hostapd_data *hapd,
		  struct hostapd_uhr_npca_params *npca,
		  char **pos)
{
	char *cur = *pos;
	char *end, *token;
	bool primary_chan_set = false;
	bool enable_seen = false;

	while (*cur) {
		while (*cur == ' ')
			cur++;
		if (!*cur)
			break;
		/* Stop if we hit another top-level keyword */
		if (is_uhr_section_keyword(cur))
			break;

		/* Find end of this token */
		end = os_strchr(cur, ' ');
		if (end)
			*end = '\0';
		token = cur;
		cur = end ? end + 1 : cur + os_strlen(cur);

		if (os_strncmp(token, "enable=", 7) == 0) {
			npca->enable = atoi(token + 7) != 0;
			enable_seen = true;
			if (!npca->enable) {
				while (*cur == ' ')
					cur++;
				if (*cur && !is_uhr_section_keyword(cur)) {
					wpa_printf(MSG_ERROR,
						   "UPDATE_UHR_FEATURES: NPCA enable=0 must not be followed by additional parameters");
					return -1;
				}
				break;
			}
		} else if (os_strncmp(token, "primary_chan=", 13) == 0) {
			int npca_chan =
				hostapd_npca_primary_chan_validate(
					hapd, token + 13);

			if (npca_chan < 0)
				return -1;
			primary_chan_set = true;
			npca->params =
				(npca->params &
				 ~UHR_OPER_PARAMS_NPCA_PRIMARY_CHAN) |
				((u32) npca_chan <<
				 UHR_OPER_PARAMS_NPCA_PRIMARY_CHAN_SHIFT);
		} else if (os_strncmp(token, "min_dur=", 8) == 0) {
			u32 v = (u32) atoi(token + 8) &
				(UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH >>
				 UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH_SHIFT);
			npca->params =
				(npca->params &
				 ~UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH) |
				(v << UHR_OPER_PARAMS_NPCA_NPCA_MIN_DUR_THRESH_SHIFT);
		} else if (os_strncmp(token, "switch_delay=", 13) == 0) {
			/* User supplies microseconds; wire value = us / 4. */
			u32 us = (u32) atoi(token + 13);
			u32 v = (us / 4) &
				(UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY >>
				 UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY_SHIFT);
			npca->params =
				(npca->params &
				 ~UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY) |
				(v << UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_DELAY_SHIFT);
		} else if (os_strncmp(token, "switch_back=", 12) == 0) {
			/* User supplies microseconds; wire value = us / 4. */
			u32 us = (u32) atoi(token + 12);
			u32 v = (us / 4) &
				(UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY >>
				 UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY_SHIFT);
			npca->params =
				(npca->params &
				 ~UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY) |
				(v << UHR_OPER_PARAMS_NPCA_NPCA_SWITCH_BACK_DELAY_SHIFT);
		} else if (os_strncmp(token, "init_qsrc=", 10) == 0) {
			u32 v = (u32) atoi(token + 10) &
				(UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC >>
				 UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC_SHIFT);
			npca->params =
				(npca->params &
				 ~UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC) |
				(v << UHR_OPER_PARAMS_NPCA_INIT_NPCA_QRSC_SHIFT);
		} else if (os_strncmp(token, "moplen=", 7) == 0) {
			if (atoi(token + 7))
				npca->params |= UHR_OPER_PARAMS_NPCA_MOPLEN_NPCA;
			else
				npca->params &= ~UHR_OPER_PARAMS_NPCA_MOPLEN_NPCA;
		} else if (os_strncmp(token, "bitmap=", 7) == 0) {
			unsigned long bmap;
			bmap = strtoul(token + 7, NULL, 0);
			npca->disabled_subchan_bitmap = (u16)(bmap & 0xFFFF);
			npca->params |= UHR_OPER_PARAMS_NPCA_DIS_SUBCH_BITMAP_PRES;
		} else {
			wpa_printf(MSG_ERROR,
				   "UPDATE_UHR_FEATURES: unknown NPCA token '%s'",
				   token);
			return -1;
		}
	}

	if (enable_seen && npca->enable &&
	    !primary_chan_set) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: NPCA enable=1 requires "
			   "primary_chan to be specified");
		return -1;
	}

	*pos = cur;
	return 0;
}

static int
hostapd_validate_uhr_cu_state(struct hostapd_data *hapd) {

	if (!hostapd_is_uhr_enabled(hapd)) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: UHR not enabled on this BSS");
		return 0;
	}

	if (hapd->uhr_ecu.state != UHR_ECU_IDLE) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: UHR_CU already in progress, try again later");
		return -1;
	}

	return 0;
}

static int
hostapd_validate_mld_uhr_cu_state(struct hostapd_data *hapd)
{
	struct hostapd_data *link;

	for_each_mld_link(link, hapd) {
		if (!hostapd_is_uhr_enabled(link))
			continue;
		if (link->uhr_ecu.state != UHR_ECU_IDLE) {
			wpa_printf(MSG_ERROR,
				   "UPDATE_UHR_FEATURES: ECU already in progress on link %u (%s), try again later",
				   link->mld_link_id, link->conf->iface);
			return -1;
		}
	}
	return 0;
}

static int
hostapd_send_uhr_params_critical_update(struct hostapd_data *hapd)
{
	size_t elem_len;
	u8 *elem, *elem_end;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "UPDATE_UHR_FEATURES [%s]: Initiating UHR enhanced critical update",
		   hapd->conf->iface);

	elem_len = hostapd_eid_uhr_params_update_len(hapd, false, true);
	if (elem_len == 0) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES [%s]: UHR Params Update element is empty",
			   hapd->conf->iface);
		return -1;
	}

	elem = os_malloc(elem_len);
	if (!elem) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES [%s]: failed to allocate element buffer",
			   hapd->conf->iface);
		return -1;
	}

	elem_end = hostapd_eid_uhr_params_update(hapd, elem, false, true);
	elem_len = elem_end - elem;

	wpa_hexdump(MSG_DEBUG,
		    "UPDATE_UHR_FEATURES: UHR Params Update element",
		    elem, elem_len);

	ret = hostapd_drv_critical_update(hapd, hapd->mld_link_id,
					  NL80211_CU_TYPE_UHR_PARAMS,
					  elem, elem_len);
	os_free(elem);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES [%s] NL80211_CMD_CRITICAL_UPDATE failed: %d",
			   hapd->conf->iface,
			   ret);
	return ret;
}


static int
hostapd_ctrl_iface_update_uhr_features(struct hostapd_data *hapd, char *cmd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	struct uhr_params_update_config *upd = &conf->uhr_params_update;
	struct hostapd_uhr_npca_params new_npca;
	struct hostapd_data *bss;
	u16 mode_changed = 0;
	char *pos;
	int i;
	struct hostapd_hw_modes *mode;
	struct uhr_npca_info *npca_info;

	if (hostapd_validate_uhr_cu_state(hapd) < 0)
		return -1;

	if (hostapd_validate_mld_uhr_cu_state(hapd) < 0)
		return -1;

	pos = cmd;

	while (*pos) {
		/* Skip leading whitespace */
		while (*pos == ' ')
			pos++;
		if (!*pos)
			break;

		if (os_strncmp(pos, "NPCA ", 5) == 0) {
			pos += 5;
			struct hostapd_uhr_npca_params prev_npca = upd->npca;

			mode = hapd->iface->current_mode;
			npca_info = mode ?
				&mode->npca_info[IEEE80211_MODE_AP] : NULL;
			if (!npca_info || !npca_info->npca_supported) {
				wpa_printf(MSG_ERROR,
					   "UPDATE_UHR_FEATURES: NPCA not supported by the driver");
				return -1;
			}
			npca_params_from_mode(&new_npca, npca_info);
			if (parse_npca_params(hapd, &new_npca, &pos) < 0)
				return -1;
			if (!(upd->mode_changed & BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA)) ||
			    os_memcmp(&prev_npca, &new_npca, sizeof(new_npca)) != 0) {
				mode_changed |= BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA);
			}
		} else {
			wpa_printf(MSG_ERROR,
				   "UPDATE_UHR_FEATURES: unknown keyword at '%s'",
				   pos);
			return -1;
		}
	}

	if (!mode_changed) {
		wpa_printf(MSG_ERROR,
			   "UPDATE_UHR_FEATURES: no feature update.");
		return -1;
	}


	if (mode_changed & BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA)) {
		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];

			if (hostapd_validate_uhr_cu_state(bss) < 0)
				return -1;
		}

		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];

			if (!hostapd_is_uhr_enabled(bss))
				continue;

			bss->conf->uhr_params_update.mode_changed |= BIT(UHR_PARAMS_UPDATE_MODE_ID_NPCA);
			bss->conf->uhr_params_update.npca = new_npca;

			if (!bss->started)
				hostapd_update_ecu_params(bss);

			/* TODO: Add the changes for to update bss specific uhr_params_update features e.g DPS */


			bss->uhr_ecu.countdown_timer = upd->adv_notification_interval;
			if (hostapd_send_uhr_params_critical_update(bss)) {
				wpa_printf(MSG_ERROR,
					   "UPDATE_UHR_FEATURES: failed to send critical update command");

				/*TODO: how to reset the state of other BSS? */
				return -1;
			}
		}
	} else {
		/*TODO - Add support for BSS specific critical update params update */
	}

	wpa_printf(MSG_DEBUG,
		   "UPDATE_UHR_FEATURES: UHR Params Update window "
		   "countdown=%u modes=%d",
		   hapd->uhr_ecu.countdown_timer,
		   upd->mode_changed);

	return 0;
}
#endif /* CONFIG_IEEE80211BN */


#ifdef CONFIG_TESTING_OPTIONS
static int hostapd_ctrl_iface_set_bw(struct hostapd_iface *iface, char *pos)
{
#ifdef NEED_AP_MLME
	struct hostapd_freq_params freq_params;
	int ret;
	enum oper_chan_width chanwidth;
	u8 chan, oper_class;

	if (!(iface->drv_flags2 & WPA_DRIVER_FLAGS2_AP_CHANWIDTH_CHANGE))
		return -1;

	ret = hostapd_parse_freq_params(pos, &freq_params, iface->freq);
	if (ret)
		return ret;

	chanwidth = hostapd_chan_width_from_freq_params(&freq_params);

	if (ieee80211_freq_to_channel_ext(
		    freq_params.freq,
		    freq_params.sec_channel_offset,
		    chanwidth, &oper_class,
		    &chan) == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "invalid channel: (freq=%d, sec_channel_offset=%d, vht_enabled=%d, he_enabled=%d)",
			   freq_params.freq,
			   freq_params.sec_channel_offset,
			   freq_params.vht_enabled,
			   freq_params.he_enabled);
		return -1;
	}

	freq_params.channel = chan;

	/* FIXME: What if the newly extended channel overlaps radar ranges? */

	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &freq_params, NULL);
	if (ret)
		return ret;

	ieee802_11_set_beacons(iface);
	return 0;

#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}
#endif /* CONFIG_TESTING_OPTIONS */


#ifdef CONFIG_CTRL_IFACE_MIB
static int hostapd_ctrl_iface_mib(struct hostapd_data *hapd, char *reply,
				  int reply_size, const char *param)
{
#ifdef RADIUS_SERVER
	if (os_strcmp(param, "radius_server") == 0) {
		return radius_server_get_mib(hapd->radius_srv, reply,
					     reply_size);
	}
#endif /* RADIUS_SERVER */
	return -1;
}
#endif /* CONFIG_CTRL_IFACE_MIB */


static int hostapd_ctrl_iface_vendor(struct hostapd_data *hapd, char *cmd,
				     char *buf, size_t buflen)
{
	int ret;
	char *pos, *temp = NULL;
	u8 *data = NULL;
	unsigned int vendor_id, subcmd;
	enum nested_attr nested_attr_flag = NESTED_ATTR_UNSPECIFIED;
	struct wpabuf *reply;
	size_t data_len = 0;

	/**
	 * cmd: <vendor id> <subcommand id> [<hex formatted data>]
	 * [nested=<0|1>]
	 */
	vendor_id = strtoul(cmd, &pos, 16);
	if (!isblank((unsigned char) *pos))
		return -EINVAL;

	subcmd = strtoul(pos, &pos, 10);

	if (*pos != '\0') {
		if (!isblank((unsigned char) *pos++))
			return -EINVAL;

		temp = os_strchr(pos, ' ');
		data_len = temp ? (size_t) (temp - pos) : os_strlen(pos);
	}

	if (data_len) {
		data_len /= 2;
		data = os_malloc(data_len);
		if (!data)
			return -ENOBUFS;

		if (hexstr2bin(pos, data, data_len)) {
			wpa_printf(MSG_DEBUG,
				   "Vendor command: wrong parameter format");
			os_free(data);
			return -EINVAL;
		}
	}

	pos = os_strstr(cmd, "nested=");
	if (pos)
		nested_attr_flag = atoi(pos + 7) ? NESTED_ATTR_USED :
			NESTED_ATTR_NOT_USED;

	reply = wpabuf_alloc((buflen - 1) / 2);
	if (!reply) {
		os_free(data);
		return -ENOBUFS;
	}

	ret = hostapd_drv_vendor_cmd(hapd, vendor_id, subcmd, data, data_len,
				     nested_attr_flag, reply);

	if (ret == 0)
		ret = wpa_snprintf_hex(buf, buflen, wpabuf_head_u8(reply),
				       wpabuf_len(reply));

	wpabuf_free(reply);
	os_free(data);

	return ret;
}


static int hostapd_ctrl_iface_eapol_reauth(struct hostapd_data *hapd,
					   const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->eapol_sm)
		return -1;

	eapol_auth_reauthenticate(sta->eapol_sm);
	return 0;
}


static int hostapd_ctrl_iface_eapol_set(struct hostapd_data *hapd, char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	char *pos = cmd, *param;

	if (hwaddr_aton(pos, addr) || pos[17] != ' ')
		return -1;
	pos += 18;
	param = pos;
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->eapol_sm)
		return -1;

	return eapol_auth_set_conf(sta->eapol_sm, param, pos);
}


static int hostapd_ctrl_iface_log_level(struct hostapd_data *hapd, char *cmd,
					char *buf, size_t buflen)
{
	char *pos, *end, *stamp;
	int ret;

	/* cmd: "LOG_LEVEL [<level>]" */
	if (*cmd == '\0') {
		pos = buf;
		end = buf + buflen;
		ret = os_snprintf(pos, end - pos, "Current level: %s\n"
				  "Timestamp: %d\n",
				  debug_level_str(wpa_debug_level),
				  wpa_debug_timestamp);
		if (os_snprintf_error(end - pos, ret))
			ret = 0;

		return ret;
	}

	while (*cmd == ' ')
		cmd++;

	stamp = os_strchr(cmd, ' ');
	if (stamp) {
		*stamp++ = '\0';
		while (*stamp == ' ') {
			stamp++;
		}
	}

	if (os_strlen(cmd)) {
		int level = str_to_debug_level(cmd);
		if (level < 0)
			return -1;
		wpa_debug_level = level;
	}

	if (stamp && os_strlen(stamp))
		wpa_debug_timestamp = atoi(stamp);

	os_memcpy(buf, "OK\n", 3);
	return 3;
}


#ifdef NEED_AP_MLME

static int hostapd_ctrl_iface_track_sta_list(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	char *pos, *end;
	struct hostapd_sta_info *info;
	struct os_reltime now;

	if (!iface->num_sta_seen)
		return 0;

	sta_track_expire(iface, 0);

	pos = buf;
	end = buf + buflen;

	os_get_reltime(&now);
	dl_list_for_each_reverse(info, &iface->sta_seen,
				 struct hostapd_sta_info, list) {
		struct os_reltime age;
		int ret;

		os_reltime_sub(&now, &info->last_seen, &age);
		ret = os_snprintf(pos, end - pos, MACSTR " %u %d\n",
				  MAC2STR(info->addr), (unsigned int) age.sec,
				  info->ssi_signal);
		if (os_snprintf_error(end - pos, ret))
			break;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_dump_beacon(struct hostapd_data *hapd,
					  char *buf, size_t buflen)
{
	struct beacon_data beacon;
	char *pos, *end;
	int ret;

	if (hostapd_build_beacon_data(hapd, &beacon) < 0)
		return -1;

	if (2 * (beacon.head_len + beacon.tail_len) > buflen)
		return -1;

	pos = buf;
	end = buf + buflen;

	ret = wpa_snprintf_hex(pos, end - pos, beacon.head, beacon.head_len);
	pos += ret;

	ret = wpa_snprintf_hex(pos, end - pos, beacon.tail, beacon.tail_len);
	pos += ret;

	free_beacon_data(&beacon);

	return pos - buf;
}

#endif /* NEED_AP_MLME */


static int hostapd_ctrl_iface_req_lci(struct hostapd_data *hapd,
				      const char *cmd)
{
	u8 addr[ETH_ALEN];

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_INFO, "CTRL: REQ_LCI: Invalid MAC address");
		return -1;
	}

	return hostapd_send_lci_req(hapd, addr);
}


static int hostapd_ctrl_iface_req_range(struct hostapd_data *hapd, char *cmd)
{
	u8 addr[ETH_ALEN];
	char *token, *context = NULL;
	int random_interval, min_ap;
	u8 responders[ETH_ALEN * RRM_RANGE_REQ_MAX_RESPONDERS];
	unsigned int n_responders;

	token = str_token(cmd, " ", &context);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_INFO,
			   "CTRL: REQ_RANGE - Bad destination address");
		return -1;
	}

	token = str_token(cmd, " ", &context);
	if (!token)
		return -1;

	random_interval = atoi(token);
	if (random_interval < 0 || random_interval > 0xffff)
		return -1;

	token = str_token(cmd, " ", &context);
	if (!token)
		return -1;

	min_ap = atoi(token);
	if (min_ap <= 0 || min_ap > WLAN_RRM_RANGE_REQ_MAX_MIN_AP)
		return -1;

	n_responders = 0;
	while ((token = str_token(cmd, " ", &context))) {
		if (n_responders == RRM_RANGE_REQ_MAX_RESPONDERS) {
			wpa_printf(MSG_INFO,
				   "CTRL: REQ_RANGE: Too many responders");
			return -1;
		}

		if (hwaddr_aton(token, responders + n_responders * ETH_ALEN)) {
			wpa_printf(MSG_INFO,
				   "CTRL: REQ_RANGE: Bad responder address");
			return -1;
		}

		n_responders++;
	}

	if (!n_responders) {
		wpa_printf(MSG_INFO,
			   "CTRL: REQ_RANGE - No FTM responder address");
		return -1;
	}

	return hostapd_send_range_req(hapd, addr, random_interval, min_ap,
				      responders, n_responders);
}


static int hostapd_ctrl_iface_req_beacon(struct hostapd_data *hapd,
					 const char *cmd, char *reply,
					 size_t reply_size)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	struct wpabuf *req;
	int ret;
	u8 req_mode = 0;

	if (hwaddr_aton(cmd, addr))
		return -1;
	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;
	if (os_strncmp(pos, "req_mode=", 9) == 0) {
		int val = hex2byte(pos + 9);

		if (val < 0)
			return -1;
		req_mode = val;
		pos += 11;
		pos = os_strchr(pos, ' ');
		if (!pos)
			return -1;
		pos++;
	}
	req = wpabuf_parse_bin(pos);
	if (!req)
		return -1;

	ret = hostapd_send_beacon_req(hapd, addr, req_mode, req);
	wpabuf_free(req);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}

static int
hostapd_ctrl_iface_show_rrm_beacon_report(struct hostapd_data *hapd,
					  char *buf,
					  size_t buflen)
{
	return hostapd_show_rrm_bcn_report(hapd, buf, buflen);
}

static int hostapd_ctrl_iface_req_link_measurement(struct hostapd_data *hapd,
						   const char *cmd, char *reply,
						   size_t reply_size)
{
	u8 addr[ETH_ALEN];
	int ret;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: REQ_LINK_MEASUREMENT: Invalid MAC address");
		return -1;
	}

	ret = hostapd_send_link_measurement_req(hapd, addr);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}


static int hostapd_ctrl_iface_req_channel_load(struct hostapd_data *hapd,
					       const char *cmd, char *reply,
					       size_t reply_size)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	int op_class, channel, random_interval, duration;
	int ret;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: REQ_CHANNEL_LOAD: Invalid MAC address");
		return -1;
	}

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	op_class = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	channel = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	random_interval = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	duration = atoi(pos);

	ret = hostapd_send_channel_load_req(hapd, addr, op_class, channel,
					    random_interval, duration);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}


static int hostapd_ctrl_iface_req_noise_histogram(struct hostapd_data *hapd,
						  const char *cmd, char *reply,
						  size_t reply_size)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	int op_class, channel, random_interval, duration;
	int ret;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: REQ_NOISE_HISTOGRAM: Invalid MAC address");
		return -1;
	}

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	op_class = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	channel = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	random_interval = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	duration = atoi(pos);

	ret = hostapd_send_noise_histogram_req(hapd, addr, op_class, channel,
					       random_interval, duration);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}


static int hostapd_ctrl_iface_show_neighbor(struct hostapd_data *hapd,
					    char *buf, size_t buflen)
{
	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SHOW_NEIGHBOR: Neighbor report is not enabled");
		return -1;
	}

	return hostapd_neighbor_show(hapd, buf, buflen);
}


static int hostapd_ctrl_iface_set_neighbor(struct hostapd_data *hapd, char *buf)
{
	struct wpa_ssid_value ssid = {
		.ssid_len = 0
	};
	u8 bssid[ETH_ALEN];
	struct wpabuf *nr = NULL, *lci = NULL, *civic = NULL;
	int stationary = 0;
	int bss_parameters = 0;
	int scan = 0;
	u32 bands = 0;
	char *tmp;
	int ret = -1;

	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Neighbor report is not enabled");
		return -1;
	}

	/* Set neighbors from available scan results matching the SSID */
	tmp = os_strstr(buf, "scan");
	if (tmp) {
		scan = 1;
		tmp = os_strstr(buf, "ssid=");
		if (tmp) {
			if (ssid_parse(tmp + 5, &ssid)) {
				wpa_printf(MSG_ERROR,
					   "CTRL: SET_NEIGHBOR: Bad SSID");
				return -1;
			}
		}

		tmp = os_strstr(buf, "bands=");
		if (tmp) {
			buf = tmp + 6;
			if (os_strstr(buf, "5G"))
				bands |= WPA_SETBAND_5G;
			if (os_strstr(buf, "6G"))
				bands |= WPA_SETBAND_6G;
			if (os_strstr(buf, "2G"))
				bands |= WPA_SETBAND_2G;
		}
		goto set;
	}

	if (hwaddr_aton(buf, bssid)) {
		wpa_printf(MSG_ERROR, "CTRL: SET_NEIGHBOR: Bad BSSID");
		return -1;
	}

	tmp = os_strstr(buf, "ssid=");
	if (!tmp || ssid_parse(tmp + 5, &ssid)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Bad or missing SSID");
		return -1;
	}
	buf = os_strchr(tmp + 6, tmp[5] == '"' ? '"' : ' ');
	if (!buf)
		return -1;

	tmp = os_strstr(buf, "nr=");
	if (!tmp) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Missing Neighbor Report element");
		return -1;
	}

	buf = os_strchr(tmp, ' ');
	if (buf)
		*buf++ = '\0';

	nr = wpabuf_parse_bin(tmp + 3);
	if (!nr) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Bad Neighbor Report element");
		return -1;
	}

	if (!buf)
		goto set;

	tmp = os_strstr(buf, "lci=");
	if (tmp) {
		buf = os_strchr(tmp, ' ');
		if (buf)
			*buf++ = '\0';
		lci = wpabuf_parse_bin(tmp + 4);
		if (!lci) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad LCI subelement");
			goto fail;
		}
	}

	if (!buf)
		goto set;

	tmp = os_strstr(buf, "civic=");
	if (tmp) {
		buf = os_strchr(tmp, ' ');
		if (buf)
			*buf++ = '\0';
		civic = wpabuf_parse_bin(tmp + 6);
		if (!civic) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad civic subelement");
			goto fail;
		}
	}

	if (!buf)
		goto set;

	if (os_strstr(buf, "stat"))
		stationary = 1;

	tmp = os_strstr(buf, "bss_parameter=");
	if (tmp) {
		bss_parameters = atoi(tmp + 14);
		if (bss_parameters < 0 || bss_parameters > 0xff) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad bss_parameters subelement");
			goto fail;
		}
	}

set:
	if (scan)
		ret = hostapd_neighbor_set_ifaces_scan_report(hapd, &ssid,
							      bands);
	else {
		ret = hostapd_neighbor_set(hapd, bssid, &ssid, nr, lci, civic,
					   stationary, bss_parameters);
		if (ret)
			goto fail;

		/* Update beacon to include the new neighbor in RNR */
		if (ieee802_11_set_beacon(hapd))
			wpa_printf(MSG_WARNING,
				   "CTRL: SET_NEIGHBOR: set beacon for " MACSTR " failed",
				   MAC2STR(hapd->own_addr));
	}

fail:
	wpabuf_free(nr);
	wpabuf_free(lci);
	wpabuf_free(civic);

	return ret;
}


static int hostapd_2ghz_channel_bw(struct hostapd_hw_modes *mode, char *buf,
				    size_t buflen)
{
	int ch, ret, len = 0;
	char temp_buf[30] = {0};

	for (ch = 0; ch < mode->num_channels; ch++) {
		struct hostapd_channel_data *chan = &mode->channels[ch];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		ret = os_snprintf(temp_buf, 30, "Channel[%d]", chan->chan);
		if (os_snprintf_error(30, ret))
			return len;
		ret = os_snprintf(buf + len, buflen - len,
				  "%-13s : %d - 20MHz HT40%c%c\n",
				  temp_buf, chan->freq,
				  (chan->flag & HOSTAPD_CHAN_HT40MINUS)?'-':' ',
				  (chan->flag & HOSTAPD_CHAN_HT40PLUS)?'+':' ');
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}
	return len;
}

#ifndef CONFIG_QCN_EXTN
static
#endif
int hostapd_get_channel_idx(struct hostapd_hw_modes *mode,
			    int channel_num)
{
	int  j=0;

	for(j = 0; j < mode->num_channels; j++) {
	       struct hostapd_channel_data *chan = &mode->channels[j];
	       if(chan->chan == channel_num)
			return j;
	}
	return -1;
}

static int hostapd_5ghz_vht_80_channel_bw(struct hostapd_hw_modes *mode,
					  int channel_idx)
{
	int k, m, ok, allowed[][4] = {{36, 40, 44, 48}, {52, 56, 60, 64},
				      {100, 104, 108, 112}, {116, 120, 124, 128},
				      {132, 136, 140, 144}, {149, 153, 157, 161},
				      {165, 169, 173, 177}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		 }
		 if(ok == 1)
			 break;
	 }

	if (ok == 0)
		 return false;

	for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if((channel_idx == -1) || (mode->channels[channel_idx].flag &
		   HOSTAPD_CHAN_DISABLED))
			return false;
	}

	 return true;
}

static bool hostapd_5ghz_vht_160_channel_bw(struct hostapd_hw_modes *mode,
					    int channel_idx)
{
	int k, m, ok, allowed[][8] = {{36, 40, 44, 48, 52, 56, 60, 64},
				      {100, 104, 108, 112, 116, 120, 124, 128},
				      {149, 153, 157, 161, 165, 169, 173, 177}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		}

		if(ok == 1)
			break;
	}

	if (ok == 0)
		return false;

	for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if (channel_idx == -1 || (mode->channels[channel_idx].flag &
		    HOSTAPD_CHAN_DISABLED))
			return false;
	}
	return true;
}

static int hostapd_5ghz_ht_40_channel_bw(struct hostapd_hw_modes *mode,
					 int channel_idx)
{

	int  k, ok, allowed[] = {36, 44, 52, 60, 100, 108, 116, 124, 132, 140,
				 149, 157, 165, 173, 184, 192};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	if (!((chan->flag & HOSTAPD_CHAN_HT40MINUS) ||
	    (chan->flag & HOSTAPD_CHAN_HT40PLUS)))
		return 0;

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		if (chan->chan < allowed[k])
			break;
		if (chan->chan == allowed[k]) {
			ok = 1;
			break;
		}
	}

	if (!ok && chan->chan != (allowed[k - 1] + 4))
		ok = -1;

	if (ok == 1 && (mode->channels[channel_idx + 1].flag &
	    HOSTAPD_CHAN_DISABLED))
		ok = -1;

	if (ok != -1) {
		if( ok == 1)
			return HOSTAPD_CHAN_HT40PLUS;
		else
			return HOSTAPD_CHAN_HT40MINUS;
	}
	return 0;
}

static int hostapd_5ghz_channel_bw(struct hostapd_hw_modes *mode,
				   char *buf, size_t buflen)
{
	int j, ret, ret_val, len = 0;
	char temp_buf[30] = {0};

	for (j = 0; j < mode->num_channels; j++) {
		struct hostapd_channel_data *chan = &mode->channels[j];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;

		ret = os_snprintf(temp_buf, 30, "Channel[%d]", chan->chan);
		if (os_snprintf_error(30, ret))
			return len;

		ret = os_snprintf(buf + len, buflen - len, "%-13s : %d - 20MHz ",
				  temp_buf, chan->freq);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		ret_val = hostapd_5ghz_ht_40_channel_bw(mode, j);
		if(ret_val) {
			ret = os_snprintf(buf + len, buflen - len, "HT40%s ",
					  ret_val == HOSTAPD_CHAN_HT40PLUS? "+":"-");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if(hostapd_5ghz_vht_80_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "80MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if((mode->vht_capab & (VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
		   VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ)) &&
		   hostapd_5ghz_vht_160_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "160MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_5ghz_eht_320_channel_bw_extn(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "320MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
#endif

		ret = os_snprintf(buf + len, buflen - len,"\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}
	return len;
}

static int hostapd_6ghz_he_40_channel_bw(struct hostapd_hw_modes *mode,
					 int channel_idx)
{

	int  k, ok, allowed[] = {1, 9, 17, 25, 33, 41, 49, 57, 65, 73, 81, 89,
				 97, 105, 113, 121, 129, 137, 145, 153, 161, 169,
				 177, 185, 193, 201, 209, 217, 225};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		if (chan->chan < allowed[k])
			break;
		if (chan->chan == allowed[k]) {
			ok = 1;
			break;
		}
	}

	/* ok would be 0 for secondary channel offset -1 channels. If channel
	 * number is not equivalent to previous primary channel + 4, then 40 MHz
	 * bonding is not possible
	 */
	if (!ok && chan->chan != (allowed[k - 1] + 4))
		return false;

	/* ok would be 1 for secondary channel offset 1 channels. If next immediate
	 * channel is disabled, 40 MHz bonding can't happen
	 */
	if (ok == 1 && (mode->channels[channel_idx + 1].flag &
	    HOSTAPD_CHAN_DISABLED))
		return false;

	return true;
}

static int hostapd_6ghz_he_80_channel_bw(struct hostapd_hw_modes *mode,
					 int channel_idx)
{
	int k, m, ok, allowed[][4] = {{1, 5, 9, 13}, {17, 21, 25, 29},
				      {33, 37, 41, 45}, {49, 53, 57, 61},
				      {65, 69, 73, 77}, {81, 85, 89, 93},
				      {97, 101, 105, 109}, {113, 117, 121, 125},
				      {129, 133, 137, 141}, {145, 149, 153, 157},
				      {161, 165, 169, 173}, {177, 181, 185, 189},
				      {193, 197, 201, 205}, {209, 213, 217, 221}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		}
		if(ok == 1)
			break;
	}

	if (ok == 0)
		return false;

	for (m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if((channel_idx == -1) || (mode->channels[channel_idx].flag &
		   HOSTAPD_CHAN_DISABLED))
			return false;
	}

	return true;
}

static bool hostapd_6ghz_he_160_channel_bw(struct hostapd_hw_modes *mode,
					   int channel_idx)
{
	int k, m, ok, allowed[][8] = {{1, 5, 9, 13, 17, 21, 25, 29},
				      {33, 37, 41, 45, 49, 53, 57, 61},
				      {65, 69, 73, 77, 81, 85, 89, 93},
				      {97, 101, 105, 109, 113, 117, 121, 125},
				      {129, 133, 137, 141, 145, 149, 153, 157},
				      {161, 165, 169, 173, 177, 181, 185, 189},
				      {193, 197, 201, 205, 209, 213, 217, 221}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		}
		if(ok == 1)
			break;
	}

	if (ok == 0)
		return false;

	for (m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if (channel_idx == -1 || (mode->channels[channel_idx].flag &
		    HOSTAPD_CHAN_DISABLED))
			return false;
	}
	return true;
}

static bool hostapd_6ghz_he_320_channel_bw(struct hostapd_hw_modes *mode,
					   int channel_idx)
{
	int k, m, ok, allowed[][16] = {
		{1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61},
		{65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125},
		{129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		}
		if(ok == 1)
			break;
	}

	if (ok == 0)
		return false;

	for (m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if (channel_idx == -1 || (mode->channels[channel_idx].flag &
		    HOSTAPD_CHAN_DISABLED))
			return false;
	}
	return true;
}

/* 6 GHz band could have two different types of 320 MHz channels bonding. The above
 * one uses different cf1 and below uses different. */
static bool hostapd_6ghz_he_320_1_channel_bw(struct hostapd_hw_modes *mode,
					     int channel_idx)
{
	int k, m, ok, allowed[][16] = {
		{33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93},
		{97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157},
		{161, 165, 169, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221}};
	struct hostapd_channel_data *chan = &mode->channels[channel_idx];

	ok = 0;
	for (k = 0; k < ARRAY_SIZE(allowed); k++) {
		for(m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
			if (chan->chan == allowed[k][m]) {
				ok = 1;
				break;
			}
		}
		if(ok == 1)
			break;
	}

	if (ok == 0)
		return false;

	for (m = 0; m < ARRAY_SIZE(allowed[0]); m++) {
		channel_idx = hostapd_get_channel_idx(mode, allowed[k][m]);
		if (channel_idx == -1 || (mode->channels[channel_idx].flag &
		    HOSTAPD_CHAN_DISABLED))
			return false;
	}
	return true;
}

static int hostapd_6ghz_channel_bw(struct hostapd_hw_modes *mode,
				   char *buf, size_t buflen)
{
	struct hostapd_channel_data *chan = NULL;
	int j, ret, len = 0, total_valid_chnls = 0;
	char temp_buf[30] = {0};

	for (j = 0; j < mode->num_channels; j++) {
		chan = &mode->channels[j];

		if (!(chan->flag & HOSTAPD_CHAN_DISABLED))
			++total_valid_chnls;
	}

	for (j = 0; j < mode->num_channels; j++) {
		chan = &mode->channels[j];

		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;

		ret = os_snprintf(temp_buf, 30, "Channel[%d]", chan->chan);
		if (os_snprintf_error(30, ret))
			return len;

		ret = os_snprintf(buf + len, buflen - len, "%-13s : %d - 20MHz ",
				  temp_buf, chan->freq);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (hostapd_6ghz_he_40_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "40MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hostapd_6ghz_he_80_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "80MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hostapd_6ghz_he_160_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "160MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hostapd_6ghz_he_320_channel_bw(mode, j) ||
		    hostapd_6ghz_he_320_1_channel_bw(mode, j)) {
			ret = os_snprintf(buf + len, buflen - len, "320MHz ");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		ret = os_snprintf(buf + len, buflen - len,"\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	return len;
}

static int hostapd_ctrl_iface_channel_bw(struct hostapd_iface *iface,
					 char *buf, size_t buflen)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_hw_modes *mode;
	int len = 0;
	u16 num_modes, flags;
	u8 dfs_domain,i;

	mode = hostapd_get_hw_feature_data(hapd, &num_modes, &flags,
					   &dfs_domain);

	if (!mode) {
		wpa_printf(MSG_ERROR, "CTRL: CHAN_BW: get hw feature data failure");
		return len;
	}

	if (!iface->current_mode)
		return len;

	for (i=0; i < num_modes; ++i) {
		if (iface->current_mode->mode == mode[i].mode ) {
			if (mode[i].mode != HOSTAPD_MODE_IEEE80211A) {
				len = hostapd_2ghz_channel_bw(&mode[i],
							      buf,
							      buflen);
			} else {
				if ((is_5ghz_freq(iface->current_mode->channels->freq) &&
				    is_5ghz_freq(mode[i].channels->freq))) {
					len = hostapd_5ghz_channel_bw(&mode[i],
								      buf,
								      buflen);
				} else if ((is_6ghz_freq(iface->current_mode->channels->freq) &&
					    is_6ghz_freq(mode[i].channels->freq))) {
					len = hostapd_6ghz_channel_bw(&mode[i],
								      buf,
								      buflen);
				}
			}
		}
	}

	return len;
}

static int hostapd_ctrl_iface_remove_neighbor(struct hostapd_data *hapd,
					      char *buf)
{
	struct wpa_ssid_value ssid;
	struct wpa_ssid_value *ssidp = NULL;
	u8 bssid[ETH_ALEN];
	char *tmp;
	int ret;

	if (hwaddr_aton(buf, bssid)) {
		wpa_printf(MSG_ERROR, "CTRL: REMOVE_NEIGHBOR: Bad BSSID");
		return -1;
	}

	tmp = os_strstr(buf, "ssid=");
	if (tmp) {
		ssidp = &ssid;
		if (ssid_parse(tmp + 5, &ssid)) {
			wpa_printf(MSG_ERROR,
				   "CTRL: REMOVE_NEIGHBOR: Bad SSID");
			return -1;
		}
	}

	ret = hostapd_neighbor_remove(hapd, bssid, ssidp);
	if (ret)
		return ret;

	/* Update beacon to remove the neighbor in RNR */
	if (ieee802_11_set_beacon(hapd))
		wpa_printf(MSG_WARNING,
			   "CTRL: REMOVE_NEIGHBOR: set beacon for " MACSTR " failed",
			   MAC2STR(hapd->own_addr));

	return 0;
}

static int hostapd_ctrl_iface_send_neighbor(struct hostapd_data *hapd,
					    char *buf)
{
	struct sta_info *sta;
	struct wpa_ssid_value ssid;
	u8 addr[ETH_ALEN];
	char *tmp;
	u8 token = 1;

	if (hwaddr_aton(buf, addr)) {
		wpa_printf(MSG_ERROR, "CTRL: SEND_NEIGHBOR: Bad Address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta && hapd->mld)
		sta = ap_get_link_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_ERROR, "Station " MACSTR
			   " not found for neighbor report message",
			   MAC2STR(addr));
		return -1;
	}

	tmp = os_strstr(buf, "ssid=");
	if (!tmp || ssid_parse(tmp + 5, &ssid)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SEND_NEIGHBOR: Bad or missing SSID");
		return -1;
	}

	tmp = os_strstr(buf, "dialog_token=");
	if (tmp)
		token = atoi(tmp + 13);

	hostapd_send_nei_report_resp(hapd, sta->addr, token, &ssid, 0, 0, 0);

	return 0;
}

static int hostapd_ctrl_driver_flags(struct hostapd_iface *iface, char *buf,
				     size_t buflen)
{
	int ret, i;
	char *pos, *end;

	ret = os_snprintf(buf, buflen, "%016llX:\n",
			  (long long unsigned) iface->drv_flags);
	if (os_snprintf_error(buflen, ret))
		return -1;

	pos = buf + ret;
	end = buf + buflen;

	for (i = 0; i < 64; i++) {
		if (iface->drv_flags & (1LLU << i)) {
			ret = os_snprintf(pos, end - pos, "%s\n",
					  driver_flag_to_string(1LLU << i));
			if (os_snprintf_error(end - pos, ret))
				return -1;
			pos += ret;
		}
	}

	return pos - buf;
}


static int hostapd_ctrl_driver_flags2(struct hostapd_iface *iface, char *buf,
				      size_t buflen)
{
	int ret, i;
	char *pos, *end;

	ret = os_snprintf(buf, buflen, "%016llX:\n",
			  (long long unsigned) iface->drv_flags2);
	if (os_snprintf_error(buflen, ret))
		return -1;

	pos = buf + ret;
	end = buf + buflen;

	for (i = 0; i < 64; i++) {
		if (iface->drv_flags2 & (1LLU << i)) {
			ret = os_snprintf(pos, end - pos, "%s\n",
					  driver_flag2_to_string(1LLU << i));
			if (os_snprintf_error(end - pos, ret))
				return -1;
			pos += ret;
		}
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_get_capability(struct hostapd_data *hapd,
					     const char *field, char *buf,
					     size_t buflen)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_CAPABILITY '%s'", field);

#ifdef CONFIG_DPP
	if (os_strcmp(field, "dpp") == 0) {
		int res;

#ifdef CONFIG_DPP3
		res = os_snprintf(buf, buflen, "DPP=3");
#elif defined(CONFIG_DPP2)
		res = os_snprintf(buf, buflen, "DPP=2");
#else /* CONFIG_DPP2 */
		res = os_snprintf(buf, buflen, "DPP=1");
#endif /* CONFIG_DPP2 */
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
#endif /* CONFIG_DPP */

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown GET_CAPABILITY field '%s'",
		   field);

	return -1;
}


#ifdef ANDROID
static int hostapd_ctrl_iface_driver_cmd(struct hostapd_data *hapd, char *cmd,
					 char *buf, size_t buflen)
{
	int ret;

	ret = hostapd_drv_driver_cmd(hapd, cmd, buf, buflen);
	if (ret == 0) {
		ret = os_snprintf(buf, buflen, "%s\n", "OK");
		if (os_snprintf_error(buflen, ret))
			ret = -1;
	}
	return ret;
}
#endif /* ANDROID */


#ifdef CONFIG_IEEE80211BE

static int hostapd_ctrl_iface_enable_mld(struct hostapd_iface *iface)
{
	unsigned int i;

	if (!iface || !iface->bss[0]->conf->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "Trying to enable AP MLD on an interface that is not affiliated with an AP MLD");
		return -1;
	}

	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		if (hostapd_enable_iface(h_iface)) {
			wpa_printf(MSG_ERROR, "Enabling of AP MLD failed");
			return -1;
		}
	}
	return 0;
}


static void hostapd_disable_iface_bss(struct hostapd_iface *iface)
{
	unsigned int i;

	for (i = 0; i < iface->num_bss; i++)
		hostapd_bss_deinit_no_free(iface->bss[i]);
}


static int hostapd_ctrl_iface_disable_mld(struct hostapd_iface *iface)
{
	unsigned int i;

	if (!iface || !iface->bss[0]->conf->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "Trying to disable AP MLD on an interface that is not affiliated with an AP MLD.");
		return -1;
	}

	/* First, disable BSSs before stopping beaconing and doing driver
	 * deinit so that the broadcast Deauthentication frames go out. */

	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		hostapd_disable_iface_bss(iface);
	}

	/* Then, fully disable interfaces */
	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		if (hostapd_disable_iface(h_iface)) {
			wpa_printf(MSG_ERROR, "Disabling AP MLD failed");
			return -1;
		}
	}

	return 0;
}

static s8 get_client_mode_frm_pwr_type(struct hostapd_data *hapd,
				       u8 txpwr_cat,
				       enum max_tx_pwr_interpretation tx_pwr_intrpn,
				       u8 *client_mode)
{
	u8 pwr_mode = hapd->iconf->he_6ghz_reg_pwr_type;

	if (hostapd_is_indoor_sp_pwr_mode(hapd))
		pwr_mode = HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP;

	switch (pwr_mode) {
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR:
		*client_mode = (txpwr_cat == REG_DEFAULT_CLIENT)
			? NL80211_REG_REGULAR_CLIENT_LPI
			: NL80211_REG_SUBORDINATE_CLIENT_LPI;
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_VLP:
		if (txpwr_cat == REG_DEFAULT_CLIENT) {
			*client_mode = NL80211_REG_REGULAR_CLIENT_VLP;
		} else {
			wpa_printf(MSG_ERROR, "power mode %d is not supported for intepretation %d",
				   pwr_mode, tx_pwr_intrpn);
			return -1;
		}
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_SP:
		if (txpwr_cat == REG_DEFAULT_CLIENT) {
			*client_mode = NL80211_REG_REGULAR_CLIENT_SP;
		} else {
			wpa_printf(MSG_ERROR, "power mode %d is not supported for intepretation %d",
				   pwr_mode, tx_pwr_intrpn);
			return -1;
		}
		break;
	case HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP:
		if (txpwr_cat == REG_DEFAULT_CLIENT) {
			*client_mode = NL80211_REG_REGULAR_CLIENT_SP;
		} else if (tx_pwr_intrpn == LOCAL_EIRP_PSD) {
			*client_mode = NL80211_REG_SUBORDINATE_CLIENT_SP;
		} else {
			wpa_printf(MSG_ERROR, "power mode %d is not supported for intepretation %d",
				   pwr_mode, tx_pwr_intrpn);
			return -1;
		}
		break;
	default:
		wpa_printf(MSG_ERROR, "Invalid power mode: %d", pwr_mode);
		return -1;
	}
	return 0;
}

static s8 validate_user_eirp_tx_power(struct hostapd_data *hapd, s8 *local_tx_pwr,
				      u8 local_max_txpwr_count,
				      u8 ext_tx_pwr_val_count,
				      enum max_tx_pwr_interpretation tx_pwr_intrpn,
				      u8 client_mode, u8 tx_pwr_count)
{
	s8 max_eirp_pwr[TPE_NUM_POWER_SUPP_IN_11BE] = {0};
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	enum chan_width ch_width;
	u8 cen320, pwr_type, expected_tx_pwr_count = 0;
	u16 freq;
	u8 i = 0;

	pwr_type = iface->conf->he_6ghz_reg_pwr_type;
	ch_width = hostapd_get_chan_width_from_oper_chan_width(hapd->iconf);

	switch (ch_width) {
		case CHAN_WIDTH_20:
			expected_tx_pwr_count = 0;
			break;
		case CHAN_WIDTH_40:
			expected_tx_pwr_count = 1;
			break;
		case CHAN_WIDTH_80:
			expected_tx_pwr_count = 2;
			break;
		case CHAN_WIDTH_160:
			expected_tx_pwr_count = 3;
			break;
		case CHAN_WIDTH_320:
			expected_tx_pwr_count = 4;
			break;
		default:
			wpa_printf(MSG_ERROR,
					"TPE: Unsupported channel width %d for LOCAL_EIRP",
					ch_width);
			return -1;
	}

	if (tx_pwr_count != expected_tx_pwr_count) {
		wpa_printf(MSG_ERROR,
				"TPE: Invalid tx_pwr_count %d for current channel "
				"configuration, expected %d (ch_width=%d)",
				tx_pwr_count, expected_tx_pwr_count,
				ch_width);
		return -1;
	}

	if (ext_tx_pwr_val_count && ch_width < CHAN_WIDTH_320) {
		wpa_printf(MSG_ERROR, "Extended tx power is not applicable for current bw");
		return -1;
	}

	freq = ieee80211_chan_to_freq(NULL, iconf->op_class, iconf->channel);
	cen320 = hostapd_get_oper_centr_freq_seg0_idx(iconf);
	hostapd_get_eirp_arr_for_6ghz(iface,
				      freq,
				      cen320,
				      ch_width,
				      client_mode,
				      max_eirp_pwr,
				      pwr_type,
				      tx_pwr_intrpn);

	for (i = 0; i < local_max_txpwr_count; i++) {
		if (local_tx_pwr[i] > max_eirp_pwr[i]) {
			wpa_printf(MSG_ERROR, "%d is greater than Max EIRP %d",
				   local_tx_pwr[i], max_eirp_pwr[i]);
			return -1;
		}
	}
	return 0;
}

static s8 validate_user_psd_tx_power(struct hostapd_data *hapd,
				     s8 *local_tx_pwr,
				     u8 local_max_txpwr_count,
				     u8 ext_tx_pwr_val_count,
				     enum max_tx_pwr_interpretation tx_pwr_intrpn,
				     s8 client_mode)
{
	struct hostapd_iface *iface = hapd->iface;
	s8 max_tx_pwr_ext[MAX_PSD_TPE_EXT_POWER_COUNT] = {0};
	struct hostapd_hw_modes *mode = iface->current_mode;
	int non_11be_chan_count = 0, total_chan_count = 0;
	int non_11be_start_idx = 0, chan_start_idx = 0;
	u8 pwr_mode = iface->conf->he_6ghz_reg_pwr_type;
	s8 max_tx_pwr[MAX_PSD_TPE_POWER_COUNT] = {0};
	u8 computed_tx_pwr_count = 0, tx_pwr_ext_count = 0;
	struct ieee_chan_data chan_data;
	s8 i = 0, j = 0, ret;
	u8 tpe_11ax_count;

	tpe_11ax_count = local_max_txpwr_count - ext_tx_pwr_val_count;

	if (tpe_11ax_count > MAX_PSD_TPE_POWER_COUNT ||
	    ext_tx_pwr_val_count > MAX_PSD_TPE_EXT_POWER_COUNT) {
		wpa_printf(MSG_ERROR, "TPE count exceeds maximum allowed");
		return -1;
	}

	ret = set_ieee_order_chan_list(mode, &chan_data, client_mode);
	if (ret)
		return ret;

	ret = get_chan_list(hapd, &non_11be_start_idx, &chan_start_idx,
			    &non_11be_chan_count, &total_chan_count, chan_data);
	if (ret) {
		wpa_printf(MSG_ERROR, "Unable to get chan list");
		goto free;
	}

	ret = get_psd_values(hapd, non_11be_start_idx, chan_start_idx,
			     non_11be_chan_count, total_chan_count,
			     &computed_tx_pwr_count, max_tx_pwr,
			     &tx_pwr_ext_count, max_tx_pwr_ext,
			     client_mode, chan_data, pwr_mode, tx_pwr_intrpn);
	if (ret) {
		wpa_printf(MSG_ERROR, "failed to get the PSD values");
		goto free;
	}

	for (i = 0; i < tpe_11ax_count; i++) {
		if (local_tx_pwr[i] > max_tx_pwr[i]) {
			wpa_printf(MSG_ERROR, "%d is greater than Max PSD %d",
				   local_tx_pwr[i], max_tx_pwr[i]);
			ret = -1;
			goto free;
		}
	}
	if (ext_tx_pwr_val_count) {
		for (i = tpe_11ax_count; i < local_max_txpwr_count; i++) {
			if (local_tx_pwr[i] > max_tx_pwr_ext[j]) {
				wpa_printf(MSG_ERROR, "%d is greater than Max PSD %d",
					   local_tx_pwr[i], max_tx_pwr_ext[j]);
				ret = -1;
				goto free;
			}
			j++;
		}
	}
free:
	os_free(chan_data.channels);
	return ret;
}

static int hostapd_ctrl_iface_stop_mld(struct hostapd_data *hapd)
{
	struct hostapd_data *link;
	int ret, stop_err = 0;

	if (!hapd || !hapd->conf->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "Trying to stop AP MLD on an interface that is not affiliated with an AP MLD.");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	for_each_mld_link_include_repurposed(link, hapd) {
#else
	for_each_mld_link(link, hapd) {
#endif /* CONFIG_QCN_EXTN */
		ret = hostapd_drv_stop_ap(link);
		if (ret) {
			wpa_printf(MSG_ERROR, "Failed to stop %s link %u",
				   link->conf->iface, link->mld_link_id);
			stop_err = ret;
		} else {
			wpa_printf(MSG_DEBUG, "Stopped %s link %u",
				   link->conf->iface, link->mld_link_id);
		}
	}

	return stop_err;
}


static int hostapd_ctrl_set_tx_rx_chain_mask(struct hostapd_data *hapd, char *cmd,
					    char *buf, size_t buflen)
{
	int ret = -1;
	uint32_t tx_ant, rx_ant;
	uint8_t radio_idx = NL80211_WIPHY_RADIO_ID_MAX;
	char *ptr, *endptr;

	if ((!hapd->started) || (hapd->iface->state != HAPD_IFACE_ENABLED)) {
		wpa_printf(MSG_ERROR, "Interface is not UP.\n");
		return ret;
	}

	tx_ant = (uint32_t)strtol(cmd, &ptr, 10);
	if (ptr == cmd) {
		wpa_printf(MSG_ERROR, "Invalid argument given.\n");
		return ret;
	}

	rx_ant = (uint32_t)strtol(ptr, &endptr, 10);
	if (ptr == endptr) {
		wpa_printf(MSG_ERROR, "Invalid argument given.\n");
		return ret;
	}

	if (hapd->iface->num_multi_hws && hapd->iface->current_hw_info)
		radio_idx = hapd->iface->current_hw_info->hw_idx;

	/* Set tx_ant and rx_ant values to max so that driver
	 * can move tx_ant and rx_ant to max supported values
	 */

	if (tx_ant == 0)
		tx_ant = 0xffffffff;
	if (rx_ant == 0)
		rx_ant = 0xffffffff;

	if (hapd->driver == NULL || hapd->driver->set_chain_mask == NULL) {
		wpa_printf(MSG_ERROR, "Set chain mask not found.\n");
		return -1;
	}
	ret = hapd->driver->set_chain_mask(hapd->drv_priv, radio_idx, tx_ant, rx_ant);

	if (ret)
		wpa_printf(MSG_ERROR, "Failed to set chain mask.\n");

	return ret;
}


#ifndef CONFIG_DRIVER_NL80211
static int hostapd_ctrl_get_chain_mask(struct hostapd_data *hapd,
                                       char *buf, size_t buflen)
{
	wpa_printf(MSG_ERROR, "CONFIG_DRIVER_NL80211 is not set\n");
	return -1;
}

#else /*CONFIG_DRIVER_NL80211*/
static int hostapd_ctrl_get_chain_mask(struct hostapd_data *hapd,
                                       char *buf, size_t buflen)
{
	int ret;
	u8 radio_idx = NL80211_WIPHY_RADIO_ID_MAX;

	if (!hapd->driver || !hapd->drv_priv || !hapd->started){
		wpa_printf(MSG_ERROR, "Driver Data/Interface not found\n");
		return -1;
	}

	if (hapd->iface && hapd->iface->num_multi_hws && hapd->iface->current_hw_info)
		radio_idx = hapd->iface->current_hw_info->hw_idx;

	ret = nl80211_get_chain_mask(hapd->drv_priv, radio_idx, buf, buflen);
	if (ret < 0)
		return -1;

	return ret;
}
#endif /*CONFIG_DRIVER_NL80211*/

static int hostapd_ctrl_iface_link_remove(struct hostapd_data *hapd, char *cmd,
					  char *buf, size_t buflen)
{
	int ret;
	u32 count = atoi(cmd);

	if (!count) {
		count = MIN_ML_RECONF_COUNT;
	} else if (count < MIN_ML_RECONF_COUNT || count > MAX_ML_RECONF_COUNT) {
		wpa_printf(MSG_ERROR, "Invalid link removal count:%d allowed range %d-%d\n",
			   count, MIN_ML_RECONF_COUNT, MAX_ML_RECONF_COUNT);
		ret = os_snprintf(buf, buflen, "%s\n", "FAIL");
		if (os_snprintf_error(buflen, ret))
			return -1;
	} else if (!hapd->conf->mld_ap) {
		wpa_printf(MSG_ERROR, "ML reconfigure is not supported in non-MLO case\n");
		ret = os_snprintf(buf, buflen, "%s\n", "FAIL");
		if (os_snprintf_error(buflen, ret))
			return -1;
		return -1;
	}

	ret = hostapd_link_remove(hapd, count, HAPD_LINK_REMOVAL);
	if (ret == 0) {
		ret = os_snprintf(buf, buflen, "%s\n", "OK");
		if (os_snprintf_error(buflen, ret))
			ret = -1;
		else
			ret = 0;
	}

	return ret;
}


static int hostapd_ctrl_iface_negotiated_ttlm_request(struct hostapd_data *hapd, const char *cmd)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm;
	struct ttlm_of_direction *ttlm_dir = NULL;
	struct ttlm_config *ttlm_conf;
	struct sta_info *sta;
	char *input, *token, *saveptr, *tid_str, *map_str;
	int ret, i, num_tids, dir = -1;
	bool homogeneous_map;
	u8 addr[ETH_ALEN];
	u8 tid_num = 0;
	u16 link_map = 0;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf && hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		struct hostapd_data *link_hapd;

		link_hapd = hostapd_get_non_repurposed_link_of_mld_extn(hapd);
		if (!link_hapd) {
			wpa_printf(MSG_ERROR,
				   "TTLM: Failed to get non-repurposed link");
			return -1;
		}
		hapd = link_hapd;
	}
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf || !hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM negotiation support is disabled");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);
#endif /* CONFIG_QCN_EXTN */

	input = os_strdup(cmd);
	if (!input)
		return -1;

	token = strtok_r(input, " ", &saveptr);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_ERROR, "Invalid or missing STA MAC address");
		os_free(input);
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_ERROR, "Station " MACSTR " not found", MAC2STR(addr));
		os_free(input);
		return -1;
	}

	if (is_sta_ttlm_capable(sta) == false) {
		wpa_printf(MSG_ERROR, "%s: STA not TTLM capable", __func__);
		os_free(input);
		return -1;
	}

	ttlm_conf = os_zalloc(sizeof(*ttlm_conf));
	ongoing_ttlm = os_zalloc(sizeof(*ongoing_ttlm));
	if (!ttlm_conf || !ongoing_ttlm) {
		os_free(input);
		if (ttlm_conf)
			os_free(ttlm_conf);
		if (ongoing_ttlm)
			os_free(ongoing_ttlm);
		return -1;
	}

	for (i = 0; i < TTLM_DIRECTION_MAX; i++) {
		ttlm_conf->ttlm_direction[i].direction = TTLM_DIRECTION_INVALID;
		ongoing_ttlm->ttlm_info[i].direction = TTLM_DIRECTION_INVALID;
	}

	while ((token = strtok_r(NULL, " ", &saveptr))) {
		if (os_strncmp(token, "dir=", 4) == 0) {
			dir = atoi(token + 4);
			if (dir < 0 || dir > TTLM_DIRECTION_BIDI) {
				wpa_printf(MSG_DEBUG, "Invalid direction: %d", dir);
				goto fail;
			}

			if (dir == TTLM_DIRECTION_BIDI &&
			    (ttlm_conf->ttlm_direction[TTLM_DIRECTION_DL].direction !=
			     TTLM_DIRECTION_INVALID ||
			     ttlm_conf->ttlm_direction[TTLM_DIRECTION_UL].direction !=
			     TTLM_DIRECTION_INVALID)) {
				wpa_printf(MSG_DEBUG, "Cannot mix BIDI with UL/DL");
				goto fail;
			}

			ttlm_conf->ttlm_direction[dir].direction = dir;

		} else {
			if (dir < 0 || dir > TTLM_DIRECTION_BIDI) {
				wpa_printf(MSG_DEBUG, "Direction is not specified");
				goto fail;
			}

			ttlm_dir = &ttlm_conf->ttlm_direction[dir];
			if (os_strncmp(token, "def_link_map=", 13) == 0) {
				ttlm_dir->default_mapping = atoi(token + 13);

			} else if (os_strncmp(token, "link_map_size=", 14) == 0) {
				ttlm_dir->link_mapping_size = atoi(token + 14);

			} else if (os_strncmp(token, "num_tids=", 9) == 0) {
				num_tids = atoi(token + 9);
				ttlm_dir->num_tids = num_tids;

				for (i = 0; i < num_tids; i++) {
					tid_str = strtok_r(NULL, " ", &saveptr);
					map_str = strtok_r(NULL, " ", &saveptr);
					if (!tid_str || !map_str) {
						wpa_printf(MSG_DEBUG, "Missing TID or mapping");
						goto fail;
					}

					ttlm_dir->map_tid_to_links[i].tid = atoi(tid_str);
					ttlm_dir->map_tid_to_links[i].link_map =
						strtol(map_str, NULL, 0);
				}
			}
		}
	}

	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		ttlm_dir = &ttlm_conf->ttlm_direction[dir];

		if (ttlm_dir->direction > TTLM_DIRECTION_BIDI)
			continue;

		ongoing_ttlm->ttlm_info[ttlm_dir->direction].link_mapping_size =
			ttlm_dir->link_mapping_size;
		ongoing_ttlm->ttlm_info[ttlm_dir->direction].direction = ttlm_dir->direction;
		ongoing_ttlm->ttlm_info[ttlm_dir->direction].default_link_mapping =
			ttlm_dir->default_mapping;

		if (ttlm_dir->default_mapping)
			continue;

		for (i = 0; i < ttlm_dir->num_tids; i++) {
			tid_num = ttlm_dir->map_tid_to_links[i].tid;
			link_map = ttlm_dir->map_tid_to_links[i].link_map;
#ifdef CONFIG_QCN_EXTN
			if (repurposed_links & link_map) {
				wpa_printf(MSG_ERROR,
					   "TTLM: TID %u maps to repurposed link(s) 0x%x",
					   tid_num, link_map & repurposed_links);
				goto fail;
			}
#endif /* CONFIG_QCN_EXTN */
			ongoing_ttlm->ttlm_info[ttlm_dir->direction].ieee_link_map_tid[tid_num] =
				link_map;
		}
	}

	homogeneous_map = hostapd_is_mapping_homogeneous(ongoing_ttlm);
	if (homogeneous_map == false) {
		wpa_printf(MSG_DEBUG, "Mapping is not homogeneous");
		goto fail;
	}

	/* check if requested mapping conflicts with advertised ttlm mapping. If yes, ignore the
	 * request.
	 */
	if (!is_valid_negotiated_ttlm(&hapd->mld->ttlm_ctx.established_ttlm, ongoing_ttlm))
		goto fail;

	ret = hostapd_send_ttlm_req(hapd, ongoing_ttlm, sta);

	os_free(input);
	os_free(ttlm_conf);
	os_free(ongoing_ttlm);
	return ret;

fail:
	os_free(input);
	os_free(ttlm_conf);
	os_free(ongoing_ttlm);
	return -1;
}


static int hostapd_ctrl_iface_negotiated_ttlm_teardown(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf && hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		struct hostapd_data *link_hapd;

		link_hapd = hostapd_get_non_repurposed_link_of_mld_extn(hapd);
		if (!link_hapd) {
			wpa_printf(MSG_ERROR,
				   "TTLM failed to find non-repurposed link");
			return -1;
		}
		hapd = link_hapd;
	}
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf || !hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM negotiation support is disabled");
		return -1;
	}


	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_ERROR, "Station " MACSTR
			   " not found for Negotiated TTLM  Request message",
			   MAC2STR(addr));
		return -1;
	}

	return hostapd_send_ttlm_teardown(hapd, sta);
}


static int hostapd_ctrl_iface_negotiated_ttlm_response(struct hostapd_data *hapd, const char *cmd)
{
	struct ttlm_ongoing_negotiation_info *ongoing_ttlm, *partner_ttlm;
	struct tid_to_link_map_info *partner_tid_map;
	struct ttlm_of_direction *ttlm_dir = NULL;
	struct ttlm_config *ttlm_conf;
	struct sta_info *sta, *lsta;
	struct hostapd_data *lhapd;
	char *tid_str, *map_str;
	bool homogeneous_map;
	char *input, *token, *saveptr;
	int assoc_frame = 0;
	int resp_code = 0;
	int i, dir = -1;
	int tmp;
	u8 addr[ETH_ALEN];
	u8 tid_num = 0;
	u16 link_map = 0;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf && hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		lhapd = hostapd_get_non_repurposed_link_of_mld_extn(hapd);
		if (!lhapd) {
			wpa_printf(MSG_ERROR,
				   "TTLM failed to find non-repurposed link");
			return -1;
		}
		hapd = lhapd;
	}
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf || !hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM negotiation support is disabled");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);
#endif /* CONFIG_QCN_EXTN */

	input = os_strdup(cmd);
	if (!input)
		return -1;

	token = strtok_r(input, " ", &saveptr);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_ERROR, "Invalid STA MAC address");
		os_free(input);
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_ERROR, "Station " MACSTR " not found", MAC2STR(addr));
		os_free(input);
		return -1;
	}

	ttlm_conf = os_zalloc(sizeof(*ttlm_conf));
	if (!ttlm_conf) {
		os_free(input);
		return -1;
	}

	for (i = 0; i < TTLM_DIRECTION_MAX; i++)
		ttlm_conf->ttlm_direction[i].direction = TTLM_DIRECTION_INVALID;

	while ((token = strtok_r(NULL, " ", &saveptr))) {
		if (strncmp(token, "assoc_frame=", 12) == 0) {
			assoc_frame = atoi(token + 12);
		} else if (strncmp(token, "resp_code=", 10) == 0) {
			resp_code = atoi(token + 10);
		} else if (strncmp(token, "dir=", 4) == 0) {
			dir = atoi(token + 4);
			if (dir < 0 || dir > TTLM_DIRECTION_BIDI) {
				wpa_printf(MSG_DEBUG, "Invalid direction:%d", dir);
				goto fail;
			}

			if (dir == TTLM_DIRECTION_BIDI &&
			    (ttlm_conf->ttlm_direction[TTLM_DIRECTION_DL].direction !=
			     TTLM_DIRECTION_INVALID ||
			     ttlm_conf->ttlm_direction[TTLM_DIRECTION_UL].direction !=
			     TTLM_DIRECTION_INVALID)) {
				wpa_printf(MSG_DEBUG, "Cannot mix BIDI with UL/DL");
				goto fail;
			}

			ttlm_conf->ttlm_direction[dir].direction = dir;
		} else {
			if (dir < 0 || dir > TTLM_DIRECTION_BIDI) {
				wpa_printf(MSG_DEBUG, "Direction is not specified");
				goto fail;
			}

			ttlm_dir = &ttlm_conf->ttlm_direction[dir];
			if (strncmp(token, "def_link_map=", 13) == 0) {
				ttlm_dir->default_mapping = atoi(token + 13);
			} else if (strncmp(token, "link_map_size=", 14) == 0) {
				ttlm_dir->link_mapping_size = atoi(token + 14);
			} else if (strncmp(token, "num_tids=", 9) == 0) {
				ttlm_dir->num_tids = atoi(token + 9);
				for (i = 0; i < ttlm_dir->num_tids; i++) {
					tid_str = strtok_r(NULL, " ", &saveptr);
					map_str = strtok_r(NULL, " ", &saveptr);
					if (!tid_str || !map_str) {
						wpa_printf(MSG_DEBUG, "Missing TID or mapping");
						goto fail;
					}
					ttlm_dir->map_tid_to_links[i].tid = atoi(tid_str);
					ttlm_dir->map_tid_to_links[i].link_map =
						strtol(map_str, NULL, 0);
#ifdef CONFIG_QCN_EXTN
					if (ttlm_dir->map_tid_to_links[i].link_map &
					    repurposed_links) {
						wpa_printf(MSG_ERROR,
							   "TTLM: repurpose links enabled");
						goto fail;
					}
#endif /* CONFIG_QCN_EXTN */
				}
			}
		}
	}

	ongoing_ttlm = &sta->mld_info.tid_map_info.ttlm_ongoing_negotiation_info;

	for (i = 0; i < TTLM_DIRECTION_MAX; i++)
		ongoing_ttlm->ttlm_info[i].direction = TTLM_DIRECTION_INVALID;

	if (resp_code == TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING) {
		ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING;

		for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
			ttlm_dir = &ttlm_conf->ttlm_direction[dir];

			if (ttlm_dir->direction > TTLM_DIRECTION_BIDI)
				continue;

			ongoing_ttlm->ttlm_info[ttlm_dir->direction].link_mapping_size =
				ttlm_dir->link_mapping_size;
			ongoing_ttlm->ttlm_info[ttlm_dir->direction].direction =
				ttlm_dir->direction;
			ongoing_ttlm->ttlm_info[ttlm_dir->direction].default_link_mapping =
				ttlm_dir->default_mapping;

			if (ttlm_dir->default_mapping)
				continue;

			for (int tid = 0; tid < ttlm_dir->num_tids; tid++) {
				tid_num = ttlm_dir->map_tid_to_links[tid].tid;
				link_map = ttlm_dir->map_tid_to_links[tid].link_map;
				tmp = ttlm_dir->direction;
				ongoing_ttlm->ttlm_info[tmp].ieee_link_map_tid[tid_num] =
					link_map;
			}
		}

		homogeneous_map = hostapd_is_mapping_homogeneous(ongoing_ttlm);
		if (homogeneous_map == false) {
			wpa_printf(MSG_DEBUG, "Preferred Mapping is not homogeneous");
			goto fail;
		}

	} else if (resp_code == TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING) {
		ongoing_ttlm->ttlm_resp_type = TTLM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_BIDI].direction = TTLM_DIRECTION_BIDI;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_DL].direction = TTLM_DIRECTION_INVALID;
		ongoing_ttlm->ttlm_info[TTLM_DIRECTION_UL].direction = TTLM_DIRECTION_INVALID;
		ongoing_ttlm->ttlm_info->default_link_mapping = true;
	} else {
		wpa_printf(MSG_DEBUG, "Invalid response type, set 133/134");
		goto fail;
	}

	if (!assoc_frame) {
		for_each_mld_link(lhapd, hapd) {
			if (lhapd == hapd)
				continue;

			lsta = ap_get_sta(lhapd, sta->addr);
			if (lsta && lsta->mld_info.mld_sta) {
				partner_tid_map = &lsta->mld_info.tid_map_info;
				partner_ttlm = &partner_tid_map->ttlm_ongoing_negotiation_info;
				os_memcpy(partner_ttlm, ongoing_ttlm, sizeof(*partner_ttlm));
			}
		}
	}

	os_free(ttlm_conf);
	os_free(input);
	return 0;

fail:
	os_free(ttlm_conf);
	os_free(input);
	return -1;
}


static int hostapd_ctrl_iface_negotiated_ttlm(struct hostapd_data *hapd, const char *cmd,
					      char *buf, size_t buflen)
{
	if (os_strncmp(cmd, "request ", 8) == 0)
		return hostapd_ctrl_iface_negotiated_ttlm_request(hapd, cmd + 8);
	else if (os_strncmp(cmd, "response ", 9) == 0)
		return hostapd_ctrl_iface_negotiated_ttlm_response(hapd, cmd + 9);
	else if (os_strncmp(cmd, "teardown ", 9) == 0)
		return hostapd_ctrl_iface_negotiated_ttlm_teardown(hapd, cmd + 9);
	else if (os_strncmp(cmd, "show ", 5) == 0) {
		if (os_strncmp(cmd + 5, "ttlm_capability", 15) == 0)
			return hostapd_ctrl_iface_negotiated_ttlm_capabilities(hapd, buf, buflen);
		else if (os_strncmp(cmd + 5, "ttlm_config ", 12) == 0)
			return hostapd_ctrl_iface_negotiated_ttlm_config(hapd, cmd + 17, buf,
									 buflen);
	} else {
		wpa_printf(MSG_ERROR, "invalid negotiated ttlm command");
		return -1;
	}

	return 0;
}

static int hostapd_ctrl_iface_conf_ml_rec_links(struct hostapd_data *hapd,
						const char *links)
{
	char *endptr;
	long links_val;

	errno = 0;
	links_val = strtol(links, &endptr, 10);
	if (endptr == links || *endptr != '\0' || errno != 0) {
		wpa_printf(MSG_ERROR,
			   "ml_max_rec_links: invalid value '%s'", links);
		return -1;
	}

	if (!hapd->conf->mld_ap || !hapd->conf->enable_aal) {
		wpa_printf(MSG_ERROR,
			   "MLD or AAL is not enabled (MLD enable %d AAL enable %d)",
			   hapd->conf->mld_ap, hapd->conf->enable_aal);
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		wpa_printf(MSG_ERROR,
			   "MLD or AAL is not enabled (MLD enable %d AAL enable %d)",
			   hapd->conf->mld_ap, hapd->conf->enable_aal);
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

	if (links_val < 0 || links_val > ML_IE_MAX_SUPPORT_MAX_REC_LINKS ||
			links_val > hapd->mld->num_links) {
		wpa_printf(MSG_ERROR,
			   "configured max rec links is %ld greater than %d", links_val,
			   ML_IE_MAX_SUPPORT_MAX_REC_LINKS);
		return -1;
	}

	if (links_val == ML_IE_RSVD_MAX_REC_LINKS) {
		wpa_printf(MSG_ERROR,
			   "configured max rec links is %ld reserved value", links_val);
		return -1;
	}

	hostapd_set_ml_max_rec_links(hapd, (u8)links_val);

	return 0;
}

int hostapd_ctrl_iface_advertise_ttlm(struct hostapd_data *hapd, const char *cmd)
{
	struct mlo_ttlm_ie *ttlm_conf;
	struct ttlm_info *ttlm;
	struct hostapd_data *link_bss;
	u16 removal_links = 0;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */
	u16 ieee_link_map;
	const char *pos;
	int ret = -1;
	u8 i;

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf && hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		link_bss = hostapd_get_non_repurposed_link_of_mld_extn(hapd);
		if (!link_bss) {
			wpa_printf(MSG_ERROR,
				   "Failed to find the non-repurposed link");
			return -1;
		}
		hapd = link_bss;
	}
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->conf || !hapd->conf->ttlm_enable) {
		wpa_printf(MSG_ERROR, "TTLM support is not enabled");
		return -1;
	}

	if (!hostapd_is_multiple_link_mld(hapd)) {
		wpa_printf(MSG_ERROR, "TTLM: Command is applicable only for an MLD");
		return -1;
	}

	if (hapd->mld->num_links == 1) {
		wpa_printf(MSG_INFO, "T2TM: Skip TTLM advertisement on single link MLO");
		return 0;
	}

	ttlm_conf = os_zalloc(sizeof(struct mlo_ttlm_ie));
	if (!ttlm_conf) {
		wpa_printf(MSG_ERROR, "TTLM failed to allocate ttlm_conf");
		return -1;
	}

	ttlm = &ttlm_conf->ttlm;

	pos = os_strstr(cmd, "ieee_link_map=");
	if (!pos)
		return -1;
	pos += 14;
	ieee_link_map = strtol(pos, NULL, 16);
	if (!ieee_link_map) {
		wpa_printf(MSG_ERROR, "TTLM: ieee_link_map cannot be 0");
		return -1;
	}

	for_each_mld_link(link_bss, hapd) {
		if (link_bss->eht_mld_link_removal_count)
			removal_links |= BIT(link_bss->mld_link_id);
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);
#endif /* CONFIG_QCN_EXTN */

	if (removal_links & ieee_link_map) {
		wpa_printf(MSG_ERROR, "TTLM: Cannot map to link under removal process, "
			   "removal_links:%x provisioned_links:%x",
			   removal_links, ieee_link_map);
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (repurposed_links & ieee_link_map) {
		wpa_printf(MSG_ERROR,
			   "TTLM: Cannot map tid to repurposed links");
		os_free(ttlm_conf);
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

	pos = os_strstr(cmd, " map_switch_time=");
	if (!pos)
		goto free_conf;
	pos += 17;
	ttlm->mapping_switch_time = atoi(pos);
	if (ttlm->mapping_switch_time)
		ttlm->mapping_switch_time_present = true;

	if (ttlm->mapping_switch_time > 0xFFFF) {
		wpa_printf(MSG_ERROR, "TTLM: Mapping switch time cannot be greater than 0xFFFF");
		goto free_conf;
	}

	pos = os_strstr(cmd, " expected_dur=");
	if (!pos) {
		wpa_printf(MSG_ERROR, "TTLM: Expected duration cannot be NULL");
		goto free_conf;
	}
	pos += 14;
	ttlm->expected_duration = atoi(pos);
	if (ttlm->expected_duration) {
		ttlm->expected_duration_present = true;
	} else {
		wpa_printf(MSG_ERROR, "TTLM: Expected duration cannot be 0");
		goto free_conf;
	}
	if (ttlm->expected_duration > 0xFFFFFF) {
		wpa_printf(MSG_ERROR, "TTLM: Expected duration cannot be greater than 0xFFFFFF");
		goto free_conf;
	}

	pos = os_strstr(cmd, " link_mapping_size=");
	if (!pos)
		goto free_conf;
	pos += 19;
	ttlm->link_mapping_size = atoi(pos);

	ttlm->direction = TTLM_DIRECTION_BIDI;

	for (i = 0; i < NUM_MAX_TIDS; i++)
		ttlm->ieee_link_map_tid[i] = ieee_link_map;

	wpa_printf(MSG_INFO, "TTLM: ieee_link_map=%d map_switch_time=%d "
		   "expected_dur=%d link_mapping_size=%d",
		   ieee_link_map, ttlm->mapping_switch_time,
		   ttlm->expected_duration, ttlm->link_mapping_size);

	ret = hostapd_send_advertised_ttlm(hapd, ttlm_conf);

free_conf:
	os_free(ttlm_conf);

	return ret;
}

static int hostapd_ctrl_iface_set_channel_usage_element(struct hostapd_data *hapd,
							 char *pos)
{
	struct channel_usage_config cfg;
	if (hostapd_parse_channel_usage_settings(pos, &cfg) < 0) {
		wpa_printf(MSG_ERROR, "Failure in parsing SET_CHANNEL_USAGE_ELEMENT");
		return -1;
	}
	/* Save the config into hostapd_data structure and set beacon */
	os_memcpy(&hapd->chan_usage_config, &cfg, sizeof(cfg));
	wpa_printf(MSG_DEBUG, "Channel Usage element updated with %d elements",
		   hapd->chan_usage_config.num_elems);
	ieee802_11_set_beacon(hapd);
	return 0;
}


#endif /* CONFIG_IEEE80211BE */

const char *tx_pwr_intrpn_str(enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	switch (tx_pwr_intrpn) {
	case LOCAL_EIRP:
		return "LOCAL_EIRP";
	case LOCAL_EIRP_PSD:
		return "LOCAL_EIRP_PSD";
	case REGULATORY_CLIENT_EIRP:
		return "REGULATORY_CLIENT_EIRP";
	case REGULATORY_CLIENT_EIRP_PSD:
		return "REGULATORY_CLIENT_EIRP_PSD";
	case REGULATORY_CLIENT_ADDITIONAL_EIRP:
		return "REGULATORY_CLIENT_ADDITIONAL_EIRP";
	case REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD:
		return "REGULATORY_CLIENT_ADDITIONAL_EIRP_PSD";
	default:
		return "Unknown";
	}
}

static s8 tpe_config_val_assign(struct hostapd_data *hapd,
				u8 index,
				const s8 *local_max_txpwr,
				u8 local_max_txpwr_count,
				u8 tx_pwr_count,
				enum max_tx_pwr_interpretation tx_pwr_intrpn,
				u8 txpwr_cat,
				u8 ext_tx_pwr_val_count)
{
	ieee80211_tpe_config_user_params *tpe_conf = &hapd->conf->tpe_ie_config;

	if (local_max_txpwr_count > IEEE80211_TPE_NUM_POWER_SUPPORTED) {
		wpa_printf(MSG_ERROR, "Count of Local Tx power values should be within %d",
			   IEEE80211_TPE_NUM_POWER_SUPPORTED);
		return -1;
	}

	if (tpe_conf->local_tpe_config & (1 << index)) {
		wpa_printf(MSG_INFO,
			   "CTRL: Overwriting existing TPE config Interpretation: %s\nCategory: %s",
			   tx_pwr_intrpn_str(tx_pwr_intrpn),
			   txpwr_cat ? "Subordinate Device" : "Default Device");
	} else {
		tpe_conf->local_tpe_config |= 1 << index;
		wpa_printf(MSG_INFO,
			   "CTRL: Adding TPE IE with Interpretation: %s\nCategory: %s",
			   tx_pwr_intrpn_str(tx_pwr_intrpn),
			   txpwr_cat ? "Subordinate Device" : "Default Device");
	}
	tpe_conf->tpe_config[index].tpe_payload.tpe_info_cnt = tx_pwr_count;
	tpe_conf->tpe_config[index].tpe_payload.tpe_info_intrpt = tx_pwr_intrpn;
	tpe_conf->tpe_config[index].tpe_payload.tpe_info_cat = txpwr_cat;
	tpe_conf->tpe_config[index].num_tpe_ext_elem = ext_tx_pwr_val_count;

	/* Copy validated power values into selected config */
	os_memcpy(tpe_conf->tpe_config[index].tpe_payload.local_max_txpwr,
		  local_max_txpwr,
		  local_max_txpwr_count);
	return 0;
}

static s8 validate_user_max_tx_pwr(struct hostapd_data *hapd,
				   s8 *local_max_txpwr,
				   u8 local_max_txpwr_count,
				   u8 ext_tx_pwr_val_count,
				   u8 txpwr_cat,
				   enum max_tx_pwr_interpretation tx_pwr_intrpn,
				   u8 tx_pwr_count)
{
	u8 client_mode;
	s8 ret;

	ret = get_client_mode_frm_pwr_type(hapd,
					   txpwr_cat,
					   tx_pwr_intrpn,
					   &client_mode);
	if (ret < 0)
		return -1;

	if (tx_pwr_intrpn == LOCAL_EIRP)
		ret = validate_user_eirp_tx_power(hapd,
						  local_max_txpwr,
						  local_max_txpwr_count,
						  ext_tx_pwr_val_count,
						  tx_pwr_intrpn,
						  client_mode, tx_pwr_count);
	else if (tx_pwr_intrpn == LOCAL_EIRP_PSD)
		ret = validate_user_psd_tx_power(hapd,
						 local_max_txpwr,
						 local_max_txpwr_count,
						 ext_tx_pwr_val_count,
						 tx_pwr_intrpn,
						 client_mode);

	return ret;
}

static s8 validatate_bw_pwr_count(struct hostapd_data *hapd,
				  u8 local_max_txpwr_count,
				  enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	enum chan_width ch_width;
	u16 bw_step_count = 0;
	u16 max_bw = 0;

	ch_width = hostapd_get_chan_width_from_oper_chan_width(iconf);
	max_bw = channel_width_to_int(ch_width);

	if (tx_pwr_intrpn == LOCAL_EIRP) {
		switch (ch_width) {
		case CHAN_WIDTH_20:
			bw_step_count = 1;
			break;
		case CHAN_WIDTH_40:
			bw_step_count = 2;
			break;
		case CHAN_WIDTH_80:
			bw_step_count = 3;
			break;
		case CHAN_WIDTH_160:
			bw_step_count = 4;
			break;
		case CHAN_WIDTH_320:
			bw_step_count = 5;
			break;
		default:
			bw_step_count = 1;
			break;
		}
	} else if (tx_pwr_intrpn == LOCAL_EIRP_PSD) {
		bw_step_count = max_bw / 20;
	}

	if (local_max_txpwr_count != bw_step_count) {
		wpa_printf(MSG_ERROR, " Tx power values(%d) does not match BW channel count(%d) for %s",
			   local_max_txpwr_count, bw_step_count, tx_pwr_intrpn_str(tx_pwr_intrpn));
		return -1;
	}

	return 0;
}

static s8 validate_user_tpe_val(struct hostapd_data *hapd,
				u8 *ext_tx_pwr_val_count,
				u8 tx_pwr_count,
				u8 txpwr_cat,
				u8 local_max_txpwr_count,
				enum max_tx_pwr_interpretation tx_pwr_intrpn)
{
	s8 total_tx_pwr_count;
	s8 tmp_extn_count;

	if (tx_pwr_intrpn < LOCAL_EIRP ||
	    tx_pwr_intrpn > LOCAL_EIRP_PSD) {
		wpa_printf(MSG_ERROR,
			   "Invalid tx power interpretation %d, allowed(%d..%d)",
			   tx_pwr_intrpn, LOCAL_EIRP, LOCAL_EIRP_PSD);
		return -1;
	}

	if (txpwr_cat > REG_SUBORDINATE_CLIENT) {
		wpa_printf(MSG_ERROR, "Invalid Tx Power Category");
		return -1;
	}

	total_tx_pwr_count = hostapd_get_tpe_11ax_count(tx_pwr_intrpn, tx_pwr_count);
	if (total_tx_pwr_count < 0)
		return -1;

	tmp_extn_count = local_max_txpwr_count - total_tx_pwr_count;
	if (tmp_extn_count < 0) {
		wpa_printf(MSG_ERROR,
			   "Total Tx Power value count too short (%d, expected %d for %s)",
			   local_max_txpwr_count, total_tx_pwr_count,
			   tx_pwr_intrpn_str(tx_pwr_intrpn));
		return -1;
	}
	*ext_tx_pwr_val_count = tmp_extn_count;

	return validatate_bw_pwr_count(hapd, local_max_txpwr_count, tx_pwr_intrpn);
}

int hostapd_ctrl_iface_set_tpe(struct hostapd_data *hapd, char *cmd)
{
	u8 tx_pwr_count, txpwr_cat, index, ext_tx_pwr_val_count;
	s8 local_max_txpwr[IEEE80211_TPE_NUM_POWER_SUPPORTED];
	enum max_tx_pwr_interpretation tx_pwr_intrpn;
	s8 local_max_txpwr_count = 0, ret;
	char *saveptr, *token;
	int temp_val;

	if (!is_6ghz_freq(hapd->iface->freq) ||
	    hapd != hostapd_mbssid_get_tx_bss(hapd)) {
		wpa_printf(MSG_ERROR, "TPE addition/deletion is allowed only on 6 GHz Tx Vap");
		return -1;
	}

	token = strtok_r(cmd, " ", &saveptr);
	if (!token) {
		wpa_printf(MSG_ERROR, "Invalid Interpretation");
		return -1;
	}
	tx_pwr_intrpn = atoi(token);

	token = strtok_r(NULL, " ", &saveptr);
	if (!token) {
		wpa_printf(MSG_ERROR, "Invalid Tx power count");
		return -1;
	}
	tx_pwr_count = atoi(token);

	token = strtok_r(NULL, " ", &saveptr);
	if (!token) {
		wpa_printf(MSG_ERROR, "Invalid client category");
		return -1;
	}
	txpwr_cat = atoi(token);

	index = ((tx_pwr_intrpn << 1) | txpwr_cat);
	if (index >= IEEE80211_TPE_LOCAL_CONFIG_MAX) {
		wpa_printf(MSG_ERROR, "Invalid TPE config index: %d", index);
		return -1;
	}

	/* Remaining tokens: power values */
	while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
		if (local_max_txpwr_count > IEEE80211_TPE_NUM_POWER_SUPPORTED) {
			wpa_printf(MSG_ERROR, "Count of Local Tx power values should be within %d",
				   IEEE80211_TPE_NUM_POWER_SUPPORTED);
			return -1;
		}
		temp_val = atoi(token);

		if (temp_val < -128 || temp_val > 127) {
			wpa_printf(MSG_ERROR, "Tx Power value %d out of valid range (-128 to 127)",
				   temp_val);
			return -1;
		}
		local_max_txpwr[local_max_txpwr_count] = (s8)temp_val;
		local_max_txpwr_count++;
	}

	ret = validate_user_tpe_val(hapd,
				    &ext_tx_pwr_val_count,
				    tx_pwr_count,
				    txpwr_cat,
				    local_max_txpwr_count,
				    tx_pwr_intrpn);
	if (ret < 0)
		return -1;

	ret = validate_user_max_tx_pwr(hapd,
				       local_max_txpwr,
				       local_max_txpwr_count,
				       ext_tx_pwr_val_count,
				       txpwr_cat,
				       tx_pwr_intrpn, tx_pwr_count);
	if (ret < 0)
		return -1;

	ret = tpe_config_val_assign(hapd,
				    index,
				    local_max_txpwr,
				    local_max_txpwr_count,
				    tx_pwr_count,
				    tx_pwr_intrpn,
				    txpwr_cat,
				    ext_tx_pwr_val_count);
	if (ret < 0)
		return -1;

	/* Update Beacon to reflect new TPE settings */
	if (ieee802_11_set_beacon(hapd))
		return -1;

	return 0;
}

/**
 * hostapd_ctrl_iface_del_tpe - Remove the Transmit Power Envelope config
 * @hapd: Pointer to hostapd data structure
 * @cmd: Pointer to the string containing the interpretation and category
 *       value to remove
 *
 * Return: 0 on success, -1 on failure
 */
int hostapd_ctrl_iface_del_tpe(struct hostapd_data *hapd, char *cmd)
{
	ieee80211_tpe_config_user_params *tpe_conf = &hapd->conf->tpe_ie_config;
	enum max_tx_pwr_interpretation tx_pwr_intrpn;
	char *saveptr, *token;
	u8 txpwr_cat;
	u8 index;

	if (!is_6ghz_freq(hapd->iface->freq) ||
	    hapd != hostapd_mbssid_get_tx_bss(hapd)) {
		wpa_printf(MSG_ERROR, "TPE addition/deletion is allowed only on 6 GHz Tx Vap");
		return -1;
	}

	/* First token: interpretation */
	token = strtok_r(cmd, " ", &saveptr);
	if (!token) {
		wpa_printf(MSG_ERROR, "Invalid Interpretation");
		return -1;
	}
	tx_pwr_intrpn = atoi(token);

	token = strtok_r(NULL, " ", &saveptr);
	if (!token) {
		wpa_printf(MSG_ERROR, "Invalid client category");
		return -1;
	}
	txpwr_cat = atoi(token);

	index = ((tx_pwr_intrpn << 1) | txpwr_cat);

	if (!(tpe_conf->local_tpe_config & (1 << index))) {
		wpa_printf(MSG_ERROR, "TPE IE not present tx_pwr_intrpn: %d txpwr_cat: %d",
			   tx_pwr_intrpn, txpwr_cat);
		return -1;
	}
	tpe_conf->local_tpe_config &= ~(1 << (index));
	os_memset(&tpe_conf->tpe_config[index], 0,
		  sizeof(struct ieee80211_tpe_ie_config));

	/* Update Beacon to reflect new TPE settings */
	if (ieee802_11_set_beacon(hapd))
		return -1;

	return 0;
}

#ifdef CONFIG_NAN_USD

static int hostapd_ctrl_nan_publish(struct hostapd_data *hapd, char *cmd,
				    char *buf, size_t buflen)
{
	char *token, *context = NULL;
	int publish_id;
	struct nan_publish_params params;
	const char *service_name = NULL;
	struct wpabuf *ssi = NULL;
	int ret = -1;
	enum nan_service_protocol_type srv_proto_type = 0;
	bool p2p = false;

	os_memset(&params, 0, sizeof(params));
	/* USD shall use both solicited and unsolicited transmissions */
	params.unsolicited = true;
	params.solicited = true;
	/* USD shall require FSD without GAS */
	params.fsd = true;

	while ((token = str_token(cmd, " ", &context))) {
		if (os_strncmp(token, "service_name=", 13) == 0) {
			service_name = token + 13;
			continue;
		}

		if (os_strncmp(token, "ttl=", 4) == 0) {
			params.ttl = atoi(token + 4);
			continue;
		}

		if (os_strncmp(token, "srv_proto_type=", 15) == 0) {
			srv_proto_type = atoi(token + 15);
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		if (os_strcmp(token, "p2p=1") == 0) {
			p2p = true;
			continue;
		}

		if (os_strcmp(token, "solicited=0") == 0) {
			params.solicited = false;
			continue;
		}

		if (os_strcmp(token, "unsolicited=0") == 0) {
			params.unsolicited = false;
			continue;
		}

		if (os_strcmp(token, "fsd=0") == 0) {
			params.fsd = false;
			continue;
		}

		wpa_printf(MSG_INFO, "CTRL: Invalid NAN_PUBLISH parameter: %s",
			   token);
		goto fail;
	}

	publish_id = hostapd_nan_usd_publish(hapd, service_name, srv_proto_type,
					     ssi, &params, p2p);
	if (publish_id > 0)
		ret = os_snprintf(buf, buflen, "%d", publish_id);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_cancel_publish(struct hostapd_data *hapd,
					   char *cmd)
{
	char *token, *context = NULL;
	int publish_id = 0;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "publish_id=%i", &publish_id) == 1)
			continue;
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_CANCEL_PUBLISH parameter: %s",
			   token);
		return -1;
	}

	if (publish_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_CANCEL_PUBLISH publish_id");
		return -1;
	}

	hostapd_nan_usd_cancel_publish(hapd, publish_id);
	return 0;
}


static int hostapd_ctrl_nan_update_publish(struct hostapd_data *hapd,
					   char *cmd)
{
	char *token, *context = NULL;
	int publish_id = 0;
	struct wpabuf *ssi = NULL;
	int ret = -1;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "publish_id=%i", &publish_id) == 1)
			continue;
		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_UPDATE_PUBLISH parameter: %s",
			   token);
		goto fail;
	}

	if (publish_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_UPDATE_PUBLISH publish_id");
		goto fail;
	}

	ret = hostapd_nan_usd_update_publish(hapd, publish_id, ssi);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_subscribe(struct hostapd_data *hapd, char *cmd,
				      char *buf, size_t buflen)
{
	char *token, *context = NULL;
	int subscribe_id;
	struct nan_subscribe_params params;
	const char *service_name = NULL;
	struct wpabuf *ssi = NULL;
	int ret = -1;
	enum nan_service_protocol_type srv_proto_type = 0;
	bool p2p = false;

	os_memset(&params, 0, sizeof(params));

	while ((token = str_token(cmd, " ", &context))) {
		if (os_strncmp(token, "service_name=", 13) == 0) {
			service_name = token + 13;
			continue;
		}

		if (os_strcmp(token, "active=1") == 0) {
			params.active = true;
			continue;
		}

		if (os_strncmp(token, "ttl=", 4) == 0) {
			params.ttl = atoi(token + 4);
			continue;
		}

		if (os_strncmp(token, "srv_proto_type=", 15) == 0) {
			srv_proto_type = atoi(token + 15);
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		if (os_strcmp(token, "p2p=1") == 0) {
			p2p = true;
			continue;
		}

		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_SUBSCRIBE parameter: %s",
			   token);
		goto fail;
	}

	subscribe_id = hostapd_nan_usd_subscribe(hapd, service_name,
						 srv_proto_type, ssi,
						 &params, p2p);
	if (subscribe_id > 0)
		ret = os_snprintf(buf, buflen, "%d", subscribe_id);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_cancel_subscribe(struct hostapd_data *hapd,
					     char *cmd)
{
	char *token, *context = NULL;
	int subscribe_id = 0;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "subscribe_id=%i", &subscribe_id) == 1)
			continue;
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_CANCEL_SUBSCRIBE parameter: %s",
			   token);
		return -1;
	}

	if (subscribe_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_CANCEL_SUBSCRIBE subscribe_id");
		return -1;
	}

	hostapd_nan_usd_cancel_subscribe(hapd, subscribe_id);
	return 0;
}


static int hostapd_ctrl_nan_transmit(struct hostapd_data *hapd, char *cmd)
{
	char *token, *context = NULL;
	int handle = 0;
	int req_instance_id = 0;
	struct wpabuf *ssi = NULL;
	u8 peer_addr[ETH_ALEN];
	int ret = -1;

	os_memset(peer_addr, 0, ETH_ALEN);

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "handle=%i", &handle) == 1)
			continue;

		if (sscanf(token, "req_instance_id=%i", &req_instance_id) == 1)
			continue;

		if (os_strncmp(token, "address=", 8) == 0) {
			if (hwaddr_aton(token + 8, peer_addr) < 0)
				return -1;
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_TRANSMIT parameter: %s",
			   token);
		goto fail;
	}

	if (handle <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_TRANSMIT handle");
		goto fail;
	}

	if (is_zero_ether_addr(peer_addr)) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_TRANSMIT address");
		goto fail;
	}

	ret = hostapd_nan_usd_transmit(hapd, handle, ssi, NULL, peer_addr,
				       req_instance_id);
fail:
	wpabuf_free(ssi);
	return ret;
}

#endif /* CONFIG_NAN_USD */


#ifdef CONFIG_SAE
static int hostapd_ctrl_iface_sae_password_bind(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *password;

	if (hwaddr_aton(cmd, addr))
		return -1;
	password = os_strchr(cmd, ' ');
	if (!password)
		return -1;
	password++;

	return sae_password_bind(hapd, addr, password);
}
#endif /* CONFIG_SAE */


static int hostapd_ctrl_iface_clear_afc_payload(struct hostapd_data *hapd,
						char *pos)
{
#ifdef NEED_AP_MLME
	if (hostapd_drv_is_retail_afc_supported(hapd)) {
		wpa_printf(MSG_ERROR, "AFC payload clear cmd is allowed only in enterprise mode");
		return -1;
	}

	if (!is_6ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR, "AFC payload clear cmd is only for 6 GHz");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Clearing AFC payload\n");

	return hostapd_drv_clear_afc_payload(hapd);
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


static int hostapd_ctrl_iface_reset_afc(struct hostapd_data *hapd,
					char *pos)
{
#ifdef NEED_AP_MLME
	if (!hostapd_drv_is_retail_afc_supported(hapd)) {
		wpa_printf(MSG_ERROR, "AFC reset cmd is allowed only in retail mode");
		return -1;
	}

	if (!is_6ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR, "AFC reset is only for 6 GHz");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Resetting AFC\n");

	return  hostapd_drv_reset_afc(hapd);
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


#ifdef CONFIG_IEEE80211AX
static int hostapd_ctrl_iface_dump_scs_list(struct hostapd_data *hapd,
					    const char *cmd, char *buf,
					    size_t buflen)
{
	struct hostapd_data *temp_hapd = hapd;
	struct sta_info *sta = NULL;
	u8 addr[ETH_ALEN];

	if (!hapd->conf->scs) {
		wpa_printf(MSG_ERROR, "SCS feature is disabled");
		return -1;
	}

	if (hwaddr_aton(cmd, addr))
		return -1;

#ifdef CONFIG_QCN_EXTN
	/* Get STA by looping all links of the AP MLD including the
	 * repurposed links
	 */
	if (hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#else /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		for_each_mld_link(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#endif /* CONFIG_QCN_EXTN */

	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "Station " MACSTR " not found to dump SCS List",
			   MAC2STR(addr));
		return -1;
	}

	return hostapd_dump_scs_list(temp_hapd, sta, buf, buflen);
}


static int hostapd_ctrl_iface_dump_scs_info(struct hostapd_data *hapd,
					    const char *cmd, char *buf,
					    size_t buflen)
{
	struct hostapd_data *temp_hapd = hapd;
	char *token, *context = NULL;
	struct sta_info *sta = NULL;
	u8 addr[ETH_ALEN];
	int scs_id_temp;
	u8 scs_id;

	if (!hapd->conf->scs) {
		wpa_printf(MSG_ERROR, "SCS feature is disabled");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || hwaddr_aton(token, addr) != 0) {
		wpa_printf(MSG_ERROR, "Invalid or missing MAC address: %s",
			   token ? token : "NULL");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || sscanf(token, "%d", &scs_id_temp) != 1 ||
	    scs_id_temp < 0 || scs_id_temp > 255) {
		wpa_printf(MSG_ERROR, "Invalid or missing SCS ID: %s",
			   token ? token : "NULL");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	/* Get STA by looping all links of the AP MLD including the
	 * repurposed links
	 */
	if (hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#else /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		for_each_mld_link(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#endif /* CONFIG_QCN_EXTN */

	if (!sta) {
		wpa_printf(MSG_ERROR,
			   "Station " MACSTR " not found to dump SCS Info",
			   MAC2STR(addr));
		return -1;
	}

	scs_id = (u8) scs_id_temp;

	return hostapd_dump_scs_info(temp_hapd, sta, buf, buflen, scs_id);
}

static int hostapd_ctrl_iface_dump_scs_qm_info(struct hostapd_data *hapd,
					       const char *cmd, char *buf,
					       size_t buflen)
{
	struct hostapd_data *temp_hapd = hapd;
	struct sta_info *sta = NULL;
	char *token, *context = NULL;
	unsigned int qm_id_val;
	u8 addr[ETH_ALEN];

	if (!hapd->conf->scs || !hapd->conf->deferred_scs) {
		wpa_printf(MSG_ERROR, "SCS feature/Deferred SCS is disabled");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || hwaddr_aton(token, addr) != 0) {
		wpa_printf(MSG_ERROR, "Invalid MAC address");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || sscanf(token, "%u", &qm_id_val) != 1 ||
	    qm_id_val < 0 || qm_id_val > 0xFF) {
		wpa_printf(MSG_ERROR, "Invalid QM ID");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#else
	if (hapd->conf->mld_ap) {
		for_each_mld_link(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#endif

	if (!sta) {
		wpa_printf(MSG_ERROR, "STA not found");
		return -1;
	}

	return hostapd_dump_scs_qm_info(temp_hapd, sta, buf, buflen,
					(u16)qm_id_val);
}


static int hostapd_ctrl_iface_send_scs_resp(struct hostapd_data *hapd,
					    const char *cmd)
{
	struct hostapd_data *temp_hapd = hapd;
	int scs_id_temp, req_type_temp;
	char *token, *context = NULL;
	struct sta_info *sta = NULL;
	u8 scs_id, req_type;
	u8 addr[ETH_ALEN];

	if (!hapd->conf->scs) {
		wpa_printf(MSG_ERROR, "SCS feature is disabled");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || hwaddr_aton(token, addr) != 0) {
		wpa_printf(MSG_ERROR, "Invalid MAC address");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || sscanf(token, "%d", &scs_id_temp) != 1 ||
	    scs_id_temp < 0 || scs_id_temp > 255) {
		wpa_printf(MSG_ERROR, "Invalid SCS ID");
		return -1;
	}

	token = str_token((char *)cmd, " ", &context);
	if (!token || sscanf(token, "%d", &req_type_temp) != 1 ||
	    req_type_temp < 0 || req_type_temp > 255) {
		wpa_printf(MSG_ERROR, "Invalid Request Type");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->mld_ap) {
		for_each_mld_link_include_repurposed(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#else /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap) {
		for_each_mld_link(temp_hapd, hapd) {
			sta = ap_get_sta(temp_hapd, addr);
			if (sta)
				break;
		}
	} else
		sta = ap_get_sta(temp_hapd, addr);
#endif /* CONFIG_QCN_EXTN */

	if (!sta) {
		wpa_printf(MSG_ERROR, "STA not found for Unsolicited response");
		return -1;
	}

	scs_id = (u8) scs_id_temp;
	req_type = (u8) req_type_temp;

	if (hostapd_send_unsolicited_scs_resp(temp_hapd, sta, scs_id,
					      req_type) < 0) {
		wpa_printf(MSG_ERROR, "Failed to send unsolicited SCS response");
		return -1;
	}

	return 0;
}


static int hostapd_ctrl_iface_scs_configure(struct hostapd_data *hapd,
					    const char *cmd)
{
	u8 peer_mac[ETH_ALEN], scs_sta_mac[ETH_ALEN];
	char *token, *context = NULL;
	u16 qm_id;
	u8 desc_buf[256];
	size_t desc_len;
	bool dedicated_queue = false;

	/* peer_mac */
	token = str_token((char *)cmd, " ", &context);
	if (!token || hwaddr_aton(token, peer_mac) != 0) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: invalid peer_mac");
		return -1;
	}

	/* scs_sta_mac */
	token = str_token((char *)cmd, " ", &context);
	if (!token || hwaddr_aton(token, scs_sta_mac) != 0) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: invalid scs_sta_mac");
		return -1;
	}

	/* qm_id limited to 0xFF for now, to be enhanced later if required */
	token = str_token((char *)cmd, " ", &context);
	if (!token || sscanf(token, "%hu", &qm_id) != 1 ||
	    qm_id > 0xFF) {
		wpa_printf(MSG_ERROR,
			   "SCS_CONFIGURE: invalid QM ID");
		return -1;
	}

	/* scs_desc_hex */
	token = str_token((char *)cmd, " ", &context);
	if (!token) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: missing scs_desc_hex");
		return -1;
	}
	desc_len = os_strlen(token) / 2;
	if (desc_len == 0 || desc_len > sizeof(desc_buf) ||
	    hexstr2bin(token, desc_buf, desc_len) < 0) {
		wpa_printf(MSG_ERROR, "SCS_CONFIGURE: invalid scs_desc_hex");
		return -1;
	}

	/* dedicated_queue — optional, default false */
	token = str_token((char *)cmd, " ", &context);
	if (token)
		dedicated_queue = !!atoi(token);

	return hostapd_scs_configure(hapd, peer_mac, scs_sta_mac, qm_id,
				     desc_buf, (u8)desc_len, dedicated_queue);
}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_PROCESS_COORDINATION

static bool hapd_ctrl_proc_coord_cb(void *ctx, int src,
				    enum proc_coord_message_types msg_type,
				    enum proc_coord_commands cmd,
				    u32 seq, const struct wpabuf *msg)
{
	struct hostapd_data *hapd = ctx;

	if (cmd != PROC_COORD_CMD_TEST)
		return false;

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		"PROC-COORD-TEST RX src=%u msg_type=%d seq=%u msg_len=%zu",
		src, msg_type, seq, wpabuf_len(msg));

	if (msg_type == PROC_COORD_MSG_REQUEST)
		proc_coord_send_response(hapd->iface->interfaces->pc,
					 src, cmd, seq, msg);
	return false;
}


static void hapd_ctrl_proc_coord_test_cb(void *ctx, int pid,
					 const struct wpabuf *msg)
{
	struct hostapd_data *hapd = ctx;

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		"PROC-COORD-TEST RX-RESP src=%u msg_len=%d",
		pid, msg ? (int) wpabuf_len(msg) : -1);

}


static int hostapd_ctrl_iface_proc_coord_test(struct hostapd_data *hapd,
					      const char *cmd)
{
	int res, dst;
	struct wpabuf *msg;

	if (!hapd->iface->interfaces->pc)
		return -1;

	dst = atoi(cmd);

	msg = wpabuf_alloc(1);
	if (!msg)
		return -1;
	wpabuf_put_u8(msg, 123);

	res = proc_coord_send_request(hapd->iface->interfaces->pc,
				      dst, PROC_COORD_CMD_TEST, msg, 1000,
				      hapd_ctrl_proc_coord_test_cb, hapd);
	wpabuf_free(msg);
	return res < 0 ? -1 : 0;
}

#endif /* CONFIG_PROCESS_COORDINATION */
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211AX
static int hostapd_ctrl_iface_set_he_bfee_sts(struct hostapd_data *hapd,
					      char *cmd)
{
	char *pos, *end;
	long lteq80, gt80;
	bool su_beamformee;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	lteq80 = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || lteq80 < 0 || lteq80 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end == '\0')
		goto usage;

	pos = end;
	errno = 0;
	gt80 = strtol(pos, &end, 10);
	if (pos == end || errno == ERANGE || gt80 < 0 || gt80 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	su_beamformee =
		((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_SU_BEAMFORMEE) ?
		 hapd->conf->he_phy_capab.he_su_beamformee :
		 hapd->iface->conf->he_phy_capab.he_su_beamformee);

	if ((lteq80 || gt80) && !su_beamformee) {
		wpa_printf(MSG_ERROR,
			   "set_he_bfee_sts requires SU beamformee support");
		return -1;
	}

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_bfee_sts_lteq80 = (u8) lteq80;
	hapd->conf->he_phy_capab.he_bfee_sts_gt80 = (u8) gt80;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_BFEE_STS_LTEQ80 |
					 HE_PHY_BSS_OVR_BFEE_STS_GT80;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR,
		   "Usage: set_he_bfee_sts <lteq80 0-7> <gt80 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_bfee_sts(struct hostapd_data *hapd,
					      char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x 0x%x\n",
			  hapd->conf->he_phy_capab.he_bfee_sts_lteq80,
			  hapd->conf->he_phy_capab.he_bfee_sts_gt80);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_multi_tid_aggr(struct hostapd_data *hapd,
						     char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_multi_tid_aggr = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_MULTI_TID_AGGR;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_multi_tid_aggr <value 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_multi_tid_aggr(struct hostapd_data *hapd,
						     char *reply,
						     int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_multi_tid_aggr);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_multi_tid_aggr_tx(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_multi_tid_aggr_tx = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_MULTI_TID_AGGR_TX;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_multi_tid_aggr_tx <value 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_multi_tid_aggr_tx(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_multi_tid_aggr_tx);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_max_ampdu_len_exp(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 3)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_max_ampdu_len_exp = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_MAX_AMPDU_LEN_EXP;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_max_ampdu_len_exp <value 0-3>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_max_ampdu_len_exp(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_max_ampdu_len_exp);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_su_ppdu_1x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 1)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_su_ppdu_1x_ltf_800ns_gi = (u8) value;
	hapd->conf->he_phy_capab_mask |=
		HE_PHY_BSS_OVR_SU_PPDU_1X_LTF_800NS_GI;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR,
		   "Usage: set_he_su_ppdu_1x_ltf_800ns_gi <value 0|1>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_su_ppdu_1x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_su_ppdu_1x_ltf_800ns_gi);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_su_mu_ppdu_4x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 1)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi = (u8) value;
	hapd->conf->he_phy_capab_mask |=
		HE_PHY_BSS_OVR_SU_MU_PPDU_4X_LTF_800NS_GI;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR,
		   "Usage: set_he_su_mu_ppdu_4x_ltf_800ns_gi <value 0|1>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_su_mu_ppdu_4x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_su_mu_ppdu_4x_ltf_800ns_gi);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_max_frag_msdu(struct hostapd_data *hapd,
						   char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_max_frag_msdu = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_MAX_FRAG_MSDU;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_max_frag_msdu <value 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_max_frag_msdu(struct hostapd_data *hapd,
						   char *reply,
						   int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_max_frag_msdu);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_min_frag_size(struct hostapd_data *hapd,
						   char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 3)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_min_frag_size = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_MIN_FRAG_SIZE;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_min_frag_size <value 0-3>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_min_frag_size(struct hostapd_data *hapd,
						   char *reply,
						   int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_min_frag_size);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_omi(struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 1)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_omi = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_OMI;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_omi <value 0|1>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_omi(struct hostapd_data *hapd,
					 char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_omi);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_ndp_4x_ltf_3200ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		return -1;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 1)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi = (u8) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_NDP_4X_LTF_3200NS_GI;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR,
		   "Usage: set_he_ndp_4x_ltf_3200ns_gi <value 0|1>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_ndp_4x_ltf_3200ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_ndp_4x_ltf_3200ns_gi);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_u8_field(struct hostapd_data *hapd,
					      char *cmd, const char *op_name,
					      const char *usage, u8 *field,
					      u32 mask_bit, long min,
					      long max)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		goto usage_err;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < min || value > max)
		goto usage_err;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage_err;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	*field = (u8) value;
	hapd->conf->he_phy_capab_mask |= mask_bit;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage_err:
	wpa_printf(MSG_ERROR, "%s", usage);
	return -1;
}

static int hostapd_ctrl_iface_get_u8_hex(char *reply, int reply_size, u8 value)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n", value);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_fragmentation(struct hostapd_data *hapd,
						   char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_fragmentation",
		"Usage: set_he_fragmentation <value 0-3>",
		&hapd->conf->he_phy_capab.he_fragmentation,
		HE_PHY_BSS_OVR_FRAGMENTATION, 0, 3);
}

static int hostapd_ctrl_iface_get_he_fragmentation(struct hostapd_data *hapd,
						   char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size, hapd->conf->he_phy_capab.he_fragmentation);
}

static int hostapd_ctrl_iface_set_he_amsdu_in_ampdu_suprt(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_amsdu_in_ampdu_suprt",
		"Usage: set_he_amsdu_in_ampdu_suprt <value 0|1>",
		&hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt,
		HE_PHY_BSS_OVR_AMSDU_IN_AMPDU_SUPRT, 0, 1);
}

static int hostapd_ctrl_iface_get_he_amsdu_in_ampdu_suprt(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size,
		hapd->conf->he_phy_capab.he_amsdu_in_ampdu_suprt);
}

static int hostapd_ctrl_iface_set_he_subfee_sts_suprt(
	struct hostapd_data *hapd, char *cmd)
{
	char *pos, *end;
	long lteq80, gt80;
	bool su_beamformee;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		goto usage;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	lteq80 = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || lteq80 < 0 || lteq80 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end == '\0')
		goto usage;

	pos = end;
	errno = 0;
	gt80 = strtol(pos, &end, 10);
	if (pos == end || errno == ERANGE || gt80 < 0 || gt80 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	su_beamformee =
		((hapd->conf->he_phy_capab_mask & HE_PHY_BSS_OVR_SU_BEAMFORMEE) ?
		 hapd->conf->he_phy_capab.he_su_beamformee :
		 hapd->iface->conf->he_phy_capab.he_su_beamformee);

	if ((lteq80 || gt80) && !su_beamformee) {
		wpa_printf(MSG_ERROR,
			   "set_he_subfee_sts_suprt requires SU beamformee support");
		return -1;
	}

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_bfee_sts_lteq80 = (u8) lteq80;
	hapd->conf->he_phy_capab.he_bfee_sts_gt80 = (u8) gt80;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_BFEE_STS_LTEQ80 |
					 HE_PHY_BSS_OVR_BFEE_STS_GT80;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR,
		   "Usage: set_he_subfee_sts_suprt <lteq80 0-7> <gt80 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_subfee_sts_suprt(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_he_bfee_sts(hapd, reply, reply_size);
}

static int hostapd_ctrl_iface_set_he_max_nc_suprt(struct hostapd_data *hapd,
						  char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_max_nc_suprt",
		"Usage: set_he_max_nc_suprt <value 0-7>",
		&hapd->conf->he_phy_capab.he_max_nc, HE_PHY_BSS_OVR_MAX_NC_SUPRT,
		0, 7);
}

static int hostapd_ctrl_iface_get_he_max_nc_suprt(struct hostapd_data *hapd,
						  char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size, hapd->conf->he_phy_capab.he_max_nc);
}

static int hostapd_ctrl_iface_set_he_er_su_disable(struct hostapd_data *hapd,
						   char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_er_su_disable",
		"Usage: set_he_er_su_disable <value 0|1>",
		&hapd->conf->he_phy_capab.he_er_su_disable,
		HE_PHY_BSS_OVR_ER_SU_DISABLE, 0, 1);
}

static int hostapd_ctrl_iface_get_he_er_su_disable(struct hostapd_data *hapd,
						   char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size, hapd->conf->he_phy_capab.he_er_su_disable);
}

static int hostapd_ctrl_iface_set_he_er_su_ppdu_1x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_er_su_ppdu_1x_ltf_800ns_gi",
		"Usage: set_he_er_su_ppdu_1x_ltf_800ns_gi <value 0|1>",
		&hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi,
		HE_PHY_BSS_OVR_ER_SU_PPDU_1X_LTF_800NS_GI, 0, 1);
}

static int hostapd_ctrl_iface_get_he_er_su_ppdu_1x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size,
		hapd->conf->he_phy_capab.he_er_su_ppdu_1x_ltf_800ns_gi);
}

static int hostapd_ctrl_iface_set_he_er_su_ppdu_4x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_er_su_ppdu_4x_ltf_800ns_gi",
		"Usage: set_he_er_su_ppdu_4x_ltf_800ns_gi <value 0|1>",
		&hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi,
		HE_PHY_BSS_OVR_ER_SU_PPDU_4X_LTF_800NS_GI, 0, 1);
}

static int hostapd_ctrl_iface_get_he_er_su_ppdu_4x_ltf_800ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size,
		hapd->conf->he_phy_capab.he_er_su_ppdu_4x_ltf_800ns_gi);
}

static int hostapd_ctrl_iface_set_he_1024qam_lt242ru_rx_enable(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_1024qam_lt242ru_rx_enable",
		"Usage: set_he_1024qam_lt242ru_rx_enable <value 0|1>",
		&hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable,
		HE_PHY_BSS_OVR_1024QAM_LT242RU_RX_ENABLE, 0, 1);
}

static int hostapd_ctrl_iface_get_he_1024qam_lt242ru_rx_enable(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size,
		hapd->conf->he_phy_capab.he_1024qam_lt242ru_rx_enable);
}

static int hostapd_ctrl_iface_set_he_full_bw_ul_mumimo(
	struct hostapd_data *hapd, char *cmd)
{
	char *end;
	long value;
	struct he_phy_capabilities_info old_he_phy_capab;
	u32 old_he_phy_capab_mask;

	if (!cmd)
		goto usage;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < 0 || value > 1)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_he_phy_capab = hapd->conf->he_phy_capab;
	old_he_phy_capab_mask = hapd->conf->he_phy_capab_mask;

	hapd->conf->he_phy_capab.he_ul_mumimo = (int) value;
	hapd->conf->he_phy_capab_mask |= HE_PHY_BSS_OVR_UL_MUMIMO;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->he_phy_capab = old_he_phy_capab;
	hapd->conf->he_phy_capab_mask = old_he_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_he_full_bw_ul_mumimo <value 0|1>");
	return -1;
}

static int hostapd_ctrl_iface_get_he_full_bw_ul_mumimo(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n",
			  hapd->conf->he_phy_capab.he_ul_mumimo);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_he_bsr_support(struct hostapd_data *hapd,
						 char *cmd)
{
	return hostapd_ctrl_iface_set_he_u8_field(
		hapd, cmd, "set_he_bsr_support",
		"Usage: set_he_bsr_support <value 0|1>",
		&hapd->conf->he_phy_capab.he_bsr_support,
		HE_PHY_BSS_OVR_BSR_SUPPORT, 0, 1);
}

static int hostapd_ctrl_iface_get_he_bsr_support(struct hostapd_data *hapd,
						 char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_u8_hex(
		reply, reply_size, hapd->conf->he_phy_capab.he_bsr_support);
}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
static int hostapd_ctrl_iface_set_eht_u8_field(struct hostapd_data *hapd,
					       char *cmd,
					       const char *op_name,
					       const char *usage, u8 *field,
					       u32 mask_bit, long min,
					       long max)
{
	char *end;
	long value;
	struct eht_phy_capabilities_info old_eht_phy_capab;
	u32 old_eht_phy_capab_mask;

	if (!cmd)
		goto usage_err;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	value = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || value < min || value > max)
		goto usage_err;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage_err;

	old_eht_phy_capab = hapd->conf->eht_phy_capab;
	old_eht_phy_capab_mask = hapd->conf->eht_phy_capab_mask;

	*field = (u8) value;
	hapd->conf->eht_phy_capab_mask |= mask_bit;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->eht_phy_capab = old_eht_phy_capab;
	hapd->conf->eht_phy_capab_mask = old_eht_phy_capab_mask;
	return -1;

usage_err:
	wpa_printf(MSG_ERROR, "%s", usage);
	return -1;
}

static int hostapd_ctrl_iface_get_eht_u8_hex(char *reply, int reply_size,
					     u8 value)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x\n", value);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_eht_ndp_4x_eht_ltf_and_320nsgi(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_ndp_4x_eht_ltf_and_320nsgi",
		"Usage: set_eht_ndp_4x_eht_ltf_and_320nsgi <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi,
		EHT_PHY_BSS_OVR_NDP_4X_EHT_LTF_AND_320NSGI, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_ndp_4x_eht_ltf_and_320nsgi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size,
		hapd->conf->eht_phy_capab.eht_ndp_4x_eht_ltf_and_320nsgi);
}

static int hostapd_ctrl_iface_set_eht_num_sd(struct hostapd_data *hapd,
					     char *cmd)
{
	char *pos, *end;
	long lt80, bw160, bw320;
	struct eht_phy_capabilities_info old_eht_phy_capab;
	u32 old_eht_phy_capab_mask;

	if (!cmd)
		goto usage;

	while (*cmd == ' ')
		cmd++;

	errno = 0;
	lt80 = strtol(cmd, &end, 10);
	if (cmd == end || errno == ERANGE || lt80 < 0 || lt80 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end == '\0')
		goto usage;

	pos = end;
	errno = 0;
	bw160 = strtol(pos, &end, 10);
	if (pos == end || errno == ERANGE || bw160 < 0 || bw160 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end == '\0')
		goto usage;

	pos = end;
	errno = 0;
	bw320 = strtol(pos, &end, 10);
	if (pos == end || errno == ERANGE || bw320 < 0 || bw320 > 7)
		goto usage;

	while (*end == ' ')
		end++;
	if (*end != '\0')
		goto usage;

	old_eht_phy_capab = hapd->conf->eht_phy_capab;
	old_eht_phy_capab_mask = hapd->conf->eht_phy_capab_mask;

	hapd->conf->eht_phy_capab.eht_num_sd_lt80 = (u8) lt80;
	hapd->conf->eht_phy_capab.eht_num_sd_160 = (u8) bw160;
	hapd->conf->eht_phy_capab.eht_num_sd_320 = (u8) bw320;
	hapd->conf->eht_phy_capab_mask |= EHT_PHY_BSS_OVR_NUM_SD_LT80 |
					  EHT_PHY_BSS_OVR_NUM_SD_160 |
					  EHT_PHY_BSS_OVR_NUM_SD_320;

	if (hostapd_validate_bss_capab(hapd) < 0)
		goto rollback;

	if (!hapd->conf->is_cmn_param && hostapd_reload_bss_only(hapd) < 0)
		goto rollback;

	return 0;

rollback:
	hapd->conf->eht_phy_capab = old_eht_phy_capab;
	hapd->conf->eht_phy_capab_mask = old_eht_phy_capab_mask;
	return -1;

usage:
	wpa_printf(MSG_ERROR, "Usage: set_eht_num_sd <lt80 0-7> <160 0-7> <320 0-7>");
	return -1;
}

static int hostapd_ctrl_iface_get_eht_num_sd(struct hostapd_data *hapd,
					     char *reply, int reply_size)
{
	int res;

	res = os_snprintf(reply, reply_size, "0x%x 0x%x 0x%x\n",
			  hapd->conf->eht_phy_capab.eht_num_sd_lt80,
			  hapd->conf->eht_phy_capab.eht_num_sd_160,
			  hapd->conf->eht_phy_capab.eht_num_sd_320);
	if (os_snprintf_error(reply_size, res))
		return -1;

	return res;
}

static int hostapd_ctrl_iface_set_eht_4x_eht_ltf_and_800ns_gi(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_4x_eht_ltf_and_800ns_gi",
		"Usage: set_eht_4x_eht_ltf_and_800ns_gi <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi,
		EHT_PHY_BSS_OVR_4X_EHT_LTF_AND_800NS_GI, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_4x_eht_ltf_and_800ns_gi(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size,
		hapd->conf->eht_phy_capab.eht_4x_eht_ltf_and_800ns_gi);
}

static int hostapd_ctrl_iface_set_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_rx_1024_and_4096_qam_ls_242_tone_ru",
		"Usage: set_eht_rx_1024_and_4096_qam_ls_242_tone_ru <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_rx_1024_and_4096_qam_ls_242_tone_ru,
		EHT_PHY_BSS_OVR_RX_1024_AND_4096_QAM_LS_242_TONE_RU, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size,
		hapd->conf->eht_phy_capab
			.eht_rx_1024_and_4096_qam_ls_242_tone_ru);
}

static int hostapd_ctrl_iface_set_eht_dl_ofdma_txbf(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_dl_ofdma_txbf",
		"Usage: set_eht_dl_ofdma_txbf <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf,
		EHT_PHY_BSS_OVR_DL_OFDMA_TXBF, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_dl_ofdma_txbf(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_dl_ofdma_txbf);
}

static int hostapd_ctrl_iface_set_eht_trig_su_bf_fb(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_trig_su_bf_fb",
		"Usage: set_eht_trig_su_bf_fb <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_trig_su_bf_fb,
		EHT_PHY_BSS_OVR_TRIG_SU_BF_FB, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_trig_su_bf_fb(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_trig_su_bf_fb);
}

static int hostapd_ctrl_iface_set_eht_trig_cqi_fb(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_trig_cqi_fb",
		"Usage: set_eht_trig_cqi_fb <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_trig_cqi_fb,
		EHT_PHY_BSS_OVR_TRIG_CQI_FB, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_trig_cqi_fb(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_trig_cqi_fb);
}

static int hostapd_ctrl_iface_set_eht_non_trig_cqi_fb(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_non_trig_cqi_fb",
		"Usage: set_eht_non_trig_cqi_fb <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_non_trig_cqi_fb,
		EHT_PHY_BSS_OVR_NON_TRIG_CQI_FB, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_non_trig_cqi_fb(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_non_trig_cqi_fb);
}

static int hostapd_ctrl_iface_set_eht_sup_mcs15_in_mru(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_sup_mcs15_in_mru",
		"Usage: set_eht_sup_mcs15_in_mru <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru,
		EHT_PHY_BSS_OVR_SUP_MCS15_IN_MRU, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_sup_mcs15_in_mru(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_sup_mcs15_in_mru);
}

static int hostapd_ctrl_iface_set_eht_mcs14_dup_in_6ghz(
	struct hostapd_data *hapd, char *cmd)
{
	return hostapd_ctrl_iface_set_eht_u8_field(
		hapd, cmd, "set_eht_mcs14_dup_in_6ghz",
		"Usage: set_eht_mcs14_dup_in_6ghz <value 0|1>",
		&hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz,
		EHT_PHY_BSS_OVR_MCS14_DUP_IN_6GHZ, 0, 1);
}

static int hostapd_ctrl_iface_get_eht_mcs14_dup_in_6ghz(
	struct hostapd_data *hapd, char *reply, int reply_size)
{
	return hostapd_ctrl_iface_get_eht_u8_hex(
		reply, reply_size, hapd->conf->eht_phy_capab.eht_mcs14_dup_in_6ghz);
}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_QCN_EXTN
static int hapd_parse_int_edca(const char *name, const char *s,
			  int min, int max, int *out, char **next)
{
	long v;
	char *end;
	const char *arg = name ? name : "value";

	if (!s || !*s) {
		wpa_printf(MSG_ERROR, "EDCA: %s is empty", arg);
		return -1;
	}

	if (s[0] < '0' || s[0] > '9') {
		wpa_printf(MSG_ERROR,
			   "EDCA: %s must start with digit",
			   arg);
		return -1;
	}

	errno = 0;
	v = strtol(s, &end, 10);

	if (s == end || errno == ERANGE || v < min || v > max) {
		wpa_printf(MSG_ERROR,
			   "EDCA: %s is out of range",
			   arg);
		return -1;
	}

	if ((end - s) > 1 && s[0] == '0') {
		wpa_printf(MSG_ERROR,
			   "EDCA: Invalid %s '%s' (no leading zeros)",
			   arg, s);
		return -1;
	}

	*out = (int) v;
	if (next)
		*next = end;

	return 0;
}

static int hostapd_ctrl_iface_set_muedca_mode(struct hostapd_data *hapd, char *cmd)
{
	char *pos;
	int mode;
	int radio_idx = -1;

	if (hapd_parse_int_edca("mode", cmd, 0, 2, &mode, &pos) < 0) {
		wpa_printf(MSG_ERROR,
			   "EDCA: Invalid usage, expected: <mode 0|1|2>");
		return -1;
	}

	/* Optional part: " radio <n>" */
	if (*pos != '\0') {
		if (*pos != ' ' || os_strncmp(pos + 1, "radio ", 6) != 0) {
			wpa_printf(MSG_ERROR,
				   "EDCA: Invalid usage, expected: <mode 0|1|2> [radio <n>]");
			return -1;
		}

		if (hapd_parse_int_edca("radio", pos + 7, 0,
				   NL80211_WIPHY_RADIO_ID_MAX - 1,
				   &radio_idx, &pos) < 0) {
			wpa_printf(MSG_ERROR, "EDCA: Invalid radio index");
			return -1;
		}

		if (*pos != '\0') {
			wpa_printf(MSG_ERROR,
				   "EDCA: Invalid usage, expected: <mode 0|1|2> [radio <n>]");
			return -1;
		}
	}

	wpa_printf(MSG_INFO, "EDCA: set MU-EDCA mode=%d radio=%d", mode, radio_idx);

	if (hostapd_drv_set_muedca_mode(hapd, mode, radio_idx) < 0) {
		wpa_printf(MSG_ERROR, "EDCA: driver set MU-EDCA mode failed");
		return -1;
	}

	return 0;
}

int hostapd_ctrl_iface_set_he_muedca(struct hostapd_data *hapd, char *buf)
{
	char *ac = NULL, *param = NULL, *value = NULL, *token = NULL;
	char *saveptr = NULL;
	char combined_name[64];
	size_t max_len = 256;
	size_t buf_len;
	int res;

	if (!buf)
		return -1;

	buf_len = strnlen(buf, max_len);
	if (buf_len >= max_len || buf_len < 5) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Invalid input length");
		return -1;
	}

	/* Parse exactly: <ac> <param> <value> */
	ac = strtok_r(buf, " ", &saveptr);
	param = strtok_r(NULL, " ", &saveptr);
	value = strtok_r(NULL, " ", &saveptr);
	token = strtok_r(NULL, " ", &saveptr);

	if (!ac || !param || !value || token) {
		wpa_printf(MSG_ERROR, "MU-EDCA: usage <ac> <param> <value>");
		return -1;
	}

	if (os_strlen(ac) < 2 || os_strlen(ac) > 10) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Invalid AC format or name too long");
		return -1;
	}
	if (os_strlen(param) > 30) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Invalid param format or name too long");
		return -1;
	}

	/* Construct Combined Name (e.g., "vo_aifsn") */
	res = os_snprintf(combined_name, sizeof(combined_name), "%s_%s", ac, param);

	if (os_snprintf_error(sizeof(combined_name), res)) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Combined name buffer overflow");
		return -1;
	}

	if (hostapd_config_he_mu_edca(&hapd->iconf->he_mu_edca, combined_name, value) < 0) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Configuration failed (invalid value?)");
		return -1;
	}

	if (ieee802_11_update_beacons(hapd->iface) < 0) {
		wpa_printf(MSG_ERROR, "MU-EDCA: Failed to update beacons with new parameters");
		return -1;
	}

	return 0;
}

static int hostapd_ctrl_iface_get_he_muedca(struct hostapd_data *hapd, char *buf,
					    char *reply, int reply_size)
{
	char *ac = NULL, *saveptr = NULL, *param = NULL, *token = NULL;
	const u8 *ac_param = NULL;
	/* max_len must match local_buf size below */
	size_t max_len = 64, buf_len;
	/* local copy so strtok_r does not modify the caller's buffer */
	char local_buf[64];
	int val, res;

	if (!buf || !hapd || !hapd->iconf || !reply)
		return -1;

	buf_len = strnlen(buf, max_len);
	/* Minimum valid input is "<ac> <param>", e.g. "be acm" (6 chars) */
	if (buf_len == max_len || buf_len < 6) {
		wpa_printf(MSG_ERROR,
			   "MU-EDCA: Invalid input length %zu (expected 6..%zu)",
			   buf_len, max_len - 1);
		return -1;
	}

	os_strlcpy(local_buf, buf, sizeof(local_buf));

	/* Parse exactly: <ac> <param> */
	ac = strtok_r(local_buf, " ", &saveptr);
	param = strtok_r(NULL, " ", &saveptr);
	token = strtok_r(NULL, " ", &saveptr);

	if (!ac || !param || token) {
		wpa_printf(MSG_ERROR, "MU-EDCA: usage get_mu_edca <ac> <param>");
		return -1;
	}

	/* Select AC buffer */
	if (os_strcmp(ac, "be") == 0)
		ac_param = hapd->iconf->he_mu_edca.he_mu_ac_be_param;
	else if (os_strcmp(ac, "bk") == 0)
		ac_param = hapd->iconf->he_mu_edca.he_mu_ac_bk_param;
	else if (os_strcmp(ac, "vi") == 0)
		ac_param = hapd->iconf->he_mu_edca.he_mu_ac_vi_param;
	else if (os_strcmp(ac, "vo") == 0)
		ac_param = hapd->iconf->he_mu_edca.he_mu_ac_vo_param;
	else {
		wpa_printf(MSG_ERROR, "MU-EDCA: Unknown AC '%s'", ac);
		return -1;
	}

	/* Extract parameter value from packed 3-byte AC record:
	 * Byte 0 (HE_MU_AC_PARAM_ACI_IDX):   [ Reserved(1) | ACI(2) | ACM(1) | AIFSN(4) ]
	 * Byte 1 (HE_MU_AC_PARAM_ECW_IDX):   [ ECWmax(4) | ECWmin(4) ]
	 * Byte 2 (HE_MU_AC_PARAM_TIMER_IDX): MU EDCA Timer
	 */
	if (os_strcmp(param, "aifsn") == 0)
		val = ac_param[HE_MU_AC_PARAM_ACI_IDX] & HE_MU_AC_PARAM_AIFSN;
	else if (os_strcmp(param, "acm") == 0)
		val = !!(ac_param[HE_MU_AC_PARAM_ACI_IDX] & HE_MU_AC_PARAM_ACM);
	else if (os_strcmp(param, "ecwmin") == 0)
		val = ac_param[HE_MU_AC_PARAM_ECW_IDX] & HE_MU_AC_PARAM_ECWMIN;
	else if (os_strcmp(param, "ecwmax") == 0)
		val = (ac_param[HE_MU_AC_PARAM_ECW_IDX] & HE_MU_AC_PARAM_ECWMAX) >> 4;
	else if (os_strcmp(param, "timer") == 0)
		val = ac_param[HE_MU_AC_PARAM_TIMER_IDX];
	else {
		wpa_printf(MSG_ERROR, "MU-EDCA: Unknown parameter '%s'", param);
		return -1;
	}

	res = os_snprintf(reply, reply_size, "%d\n", val);
	if (os_snprintf_error(reply_size, res)) {
		wpa_printf(MSG_ERROR,
			   "MU-EDCA: Reply buffer too small (size=%d)",
			   reply_size);
		return -1;
	}

	return res;
}
#endif /* CONFIG_QCN_EXTN */

/**
 * hostapd_ctrl_iface_use_ru_puncture_dfs - Handle runtime RU puncture DFS set
 * @hapd: Pointer to hostapd BSS data
 * @cmd: NUL-terminated string containing the requested enable value
 *
 * Parse and apply the runtime control interface value for
 * use_ru_puncture_dfs.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_use_ru_puncture_dfs(struct hostapd_data *hapd,
						  char *cmd)
{
	int value;

	value = atoi(cmd);
	if (value < 0 || value > 1) {
		wpa_printf(MSG_ERROR,
			   "Invalid use_ru_puncture_dfs value: %d (must be 0 or 1)",
			   value);
		return -1;
	}

	hapd->iconf->use_ru_puncture_dfs = value;
	wpa_printf(MSG_INFO, "use_ru_puncture_dfs set to %d", value);

	return 0;
}

/**
 * hostapd_ctrl_iface_dfs_disable_auto_unpunc - Set auto-unpuncture runtime flag
 * @hapd: Pointer to hostapd BSS data
 * @cmd: NUL-terminated string containing the requested enable value
 *
 * Parse and apply the runtime control interface value for
 * dfs_disable_auto_unpunc.
 *
 * Return: 0 on success, -1 on failure.
 */
static int hostapd_ctrl_iface_dfs_disable_auto_unpunc(struct hostapd_data *hapd,
						      char *cmd)
{
	int value;

	value = atoi(cmd);
	if (value < 0 || value > 1) {
		wpa_printf(MSG_ERROR,
			   "Invalid dfs_disable_auto_unpunc value: %d (must be 0 or 1)",
			   value);
		return -1;
	}

	hapd->iconf->dfs_disable_auto_unpunc = value;
	wpa_printf(MSG_INFO, "dfs_disable_auto_unpunc set to %d", value);

	return 0;
}

/**
 * hostapd_ctrl_iface_puncture_sources - List channels with puncture sources
 * @hapd: Pointer to hostapd BSS data
 * @reply: Buffer to store the response text
 * @reply_size: Size of @reply buffer in bytes
 *
 * Build a textual list of channels whose puncture source is USER or RADAR
 * and return it through the control interface reply buffer.
 *
 * Return: Length of the generated reply on success, -1 on failure.
 */
static int hostapd_ctrl_iface_puncture_sources(struct hostapd_data *hapd,
					       char *reply, size_t reply_size)
{
	struct hostapd_iface *iface;
	struct hostapd_hw_modes *mode;
	int i;
	int res;
	char *pos;

	iface = hapd ? hapd->iface : NULL;
	mode = iface ? iface->current_mode : NULL;
	if (!mode) {
		wpa_printf(MSG_ERROR, "Invalid iface/current_mode pointer");
		return -1;
	}

	pos = reply;
	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan;

		chan = &mode->channels[i];
		if (chan->puncture_source == DFS_CHAN_PUNC_NONE)
			continue;

		res = os_snprintf(pos, reply + reply_size - pos, "%d(%s) ",
				  chan->chan,
				  chan->puncture_source == DFS_CHAN_PUNC_USER ?
				  "USER" : "RADAR");
		if (os_snprintf_error(reply + reply_size - pos, res))
			return -1;

		pos += res;
	}

	res = os_snprintf(pos, reply + reply_size - pos, "\n");
	if (os_snprintf_error(reply + reply_size - pos, res))
		return -1;

	pos += res;

	return pos - reply;
}

#ifdef CONFIG_IEEE80211BN
static int
hostapd_ctrl_iface_get_mapc_configs(struct hostapd_data *hapd,
				    char *buf, size_t buflen)
{
	const struct mapc_bss_config *mc;
	char *pos = buf, *end = buf + buflen;
	int ret;

	if (!hapd->conf || !hapd->conf->mapc_conf) {
		wpa_printf(MSG_DEBUG, "MAPC: mapc_conf not allocated");
		return -1;
	}
	mc = hapd->conf->mapc_conf;

	ret = os_snprintf(pos, end - pos,
			  "cotdma_en=%d\n"
			  "disc_interval=%u\n"
			  "disc_mode=%d\n"
			  "neg_mode=%d\n"
			  "max_co_ap=%d\n"
			  "max_ctdma=%u\n"
			  "max_disc_ap=%d\n"
			  "disc_timeout=%u\n"
			  "neg_timeout=%u\n"
			  "inact_timeout=%u\n"
			  "disc_cotdma_prof=%d\n"
			  "initialized=%d\n"
			  "usr_bitmap=0x%04x\n"
			  "cap_bitmap=0x%04x\n"
			  "param_bitmap=0x%04x\n"
			  "active_peers=%u\n"
			  "cotdma_active=%u\n"
			  "disc_timer=%s\n",
			  mc->mapc_cotdma_enable,
			  mc->mapc_disc_req_interval_sec,
			  mc->discovery_mode,
			  mc->negotiation_mode,
			  mc->max_mapc_co_ap_peer,
			  (unsigned int)mc->max_mapc_ctdma_peer,
			  mc->max_mapc_discovered_ap_peer,
			  mc->discovery_req_timeout,
			  mc->negotiation_req_timeout,
			  mc->max_mapc_ap_inactivity,
			  mc->enable_disc_cotdma_scheme_profile,
			  hapd->mapc_initialized ? 1 : 0,
			  mc->mapc_usr_enabled_bitmap,
			  mc->mapc_capability_bitmap,
			  mc->mapc_parameter_bitmap,
			  (unsigned int)hapd->bss_active_peer_count,
			  (unsigned int)hapd->bss_cotdma_active_count,
			  mapc_is_periodic_disc_running(hapd) ? "running" : "stopped");
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	return pos - buf;
}

static int
hostapd_ctrl_iface_set_mapc_configs(struct hostapd_data *hapd,
				    const char *cmd,
				    char *reply, size_t reply_size)
{
	struct mapc_cfg_req reqs[MAPC_CFG_PARAMS_MAX];
	int count = 0;
	const char *p = cmd;
	const char *tok, *eq;
	size_t toklen, klen, vlen, j;

	while (*p && count < MAPC_CFG_PARAMS_MAX) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		tok = p;
		while (*p && *p != ' ')
			p++;
		toklen = (size_t)(p - tok);
		eq = NULL;
		for (j = 0; j < toklen; j++) {
			if (tok[j] == '=') {
				eq = tok + j;
				break;
			}
		}
		if (!eq)
			return os_snprintf(reply, reply_size,
					   "FAIL: '%.*s' missing '='\n",
					   (int)toklen, tok);
		klen = (size_t)(eq - tok);
		vlen = toklen - klen - 1;
		if (klen == 0 || klen >= MAPC_CFG_KEY_MAX ||
		    vlen == 0 || vlen >= MAPC_CFG_VAL_MAX)
			return os_snprintf(reply, reply_size,
					   "FAIL: token too long or empty\n");
		os_memcpy(reqs[count].key, tok, klen);
		reqs[count].key[klen] = '\0';
		os_memcpy(reqs[count].val, eq + 1, vlen);
		reqs[count].val[vlen] = '\0';
		count++;
	}

	if (count == 0)
		return os_snprintf(reply, reply_size,
				   "FAIL: expected key=value\n");

	return mapc_set_config(hapd, reqs, count, reply, reply_size);
}

static int
hostapd_ctrl_iface_get_mapc_valid_coap_list(struct hostapd_data *hapd,
					    char *buf, size_t buflen)
{
	const struct mapc_bss_config *mc;
	char *pos = buf, *end = buf + buflen;
	int ret, i;

	if (!hapd->conf || !hapd->conf->mapc_conf) {
		wpa_printf(MSG_DEBUG, "MAPC: mapc_conf not allocated");
		return -1;
	}
	mc = hapd->conf->mapc_conf;

	ret = os_snprintf(pos, end - pos, "valid_ap_count=%d\n",
			  mc->valid_ap_count);
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if (mc->valid_ap_count == 0) {
		ret = os_snprintf(pos, end - pos, "valid_coap_list=(none)\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
		return pos - buf;
	}

	ret = os_snprintf(pos, end - pos, "valid_coap_list=");
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	for (i = 0; i < mc->valid_ap_count; i++) {
		ret = os_snprintf(pos, end - pos, "%s" MACSTR,
				  i ? "," : "",
				  MAC2STR(mc->valid_coap_list[i]));
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	ret = os_snprintf(pos, end - pos, "\n");
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	return pos - buf;
}

static int
hostapd_ctrl_iface_set_mapc_valid_coap_list(struct hostapd_data *hapd,
					    const char *cmd)
{
	u8 macs[MAPC_MAX_VALID_AP_LIST][ETH_ALEN];
	int count = 0;
	const char *pos = cmd;
	char mac_str[18];

	if (!hapd->conf || !hapd->conf->mapc_conf) {
		wpa_printf(MSG_DEBUG, "MAPC: mapc_conf not allocated");
		return -1;
	}

	while (*pos == ' ')
		pos++;

	if (*pos == '\0') {
		wpa_printf(MSG_ERROR,
			   "SET_MAPC_VALID_COAP_LIST: no arguments — "
			   "at least one MAC required");
		return -1;
	}

	while (*pos) {
		if (sscanf(pos, "%17s", mac_str) != 1)
			break;
		if (count >= MAPC_MAX_VALID_AP_LIST) {
			wpa_printf(MSG_ERROR,
				   "SET_MAPC_VALID_COAP_LIST: too many MACs "
				   "(max %d)", MAPC_MAX_VALID_AP_LIST);
			return -1;
		}
		if (hwaddr_aton(mac_str, macs[count])) {
			wpa_printf(MSG_ERROR,
				   "SET_MAPC_VALID_COAP_LIST: invalid MAC '%s'",
				   mac_str);
			return -1;
		}
		count++;
		pos += os_strlen(mac_str);
		while (*pos == ' ')
			pos++;
	}

	return mapc_set_valid_coap_list(hapd, macs, count);
}

static int
hostapd_ctrl_iface_set_mapc_sta(struct hostapd_data *hapd, const char *cmd,
				char *buf, size_t buflen)
{
	struct mapc_bss_config *mc;
	struct sta_info *sta;
	struct mapc_scheme_nego_req reqs[MAPC_SCHEME_MAX];
	u8 peer_addr[ETH_ALEN];
	const char *p;
	int cotdma, ret;

	/* ---- Parse BSSID ("XX:XX:XX:XX:XX:XX") ---- */
	if (hwaddr_aton(cmd, peer_addr)) {
		wpa_printf(MSG_ERROR, "SET_MAPC_STA: invalid BSSID");
		return os_snprintf(buf, buflen, "FAIL invalid BSSID\n");
	}

	/* ---- Parse " cotdma=<0|1>" ---- */
	if (os_strlen(cmd) < 18 || cmd[17] != ' ') {
		return os_snprintf(buf, buflen,
				   "FAIL usage: SET_MAPC_STA <bssid> cotdma=<0|1>\n");
	}
	p = os_strstr(cmd + 18, "cotdma=");
	if (!p) {
		return os_snprintf(buf, buflen,
				   "FAIL missing cotdma parameter\n");
	}
	cotdma = atoi(p + 7);
	if (cotdma != 0 && cotdma != 1) {
		return os_snprintf(buf, buflen,
				   "FAIL cotdma must be 0 or 1\n");
	}

	/* ---- Sanity checks ---- */
	if (!hapd->conf->mapc_conf) {
		return os_snprintf(buf, buflen, "FAIL MAPC not configured\n");
	}
	mc = hapd->conf->mapc_conf;

	if (mc->negotiation_mode != 1) {
		return os_snprintf(buf, buflen,
				   "FAIL mapc_negotiation_mode must be 1 for manual trigger\n");
	}

	if (os_memcmp(peer_addr, hapd->own_addr, ETH_ALEN) == 0) {
		return os_snprintf(buf, buflen,
				   "FAIL cannot target own BSSID\n");
	}

#ifdef CONFIG_IEEE80211BE
	/*
	 * MLO: if peer's sta_info lives on a different link (e.g. command
	 * arrived on MLD anchor socket without -l flag), resolve to that
	 * link hapd so all subsequent operations (ap_get_sta, discovery,
	 * negotiation) target the correct link and its sta_list.
	 * Non-MLO: mapc_find_link_hapd() returns hapd unchanged.
	 */
	hapd = mapc_find_link_hapd(hapd, peer_addr);
	mc   = hapd->conf->mapc_conf;
	if (!mc) {
		return os_snprintf(buf, buflen,
				   "FAIL MAPC not configured on resolved link\n");
	}
#endif /* CONFIG_IEEE80211BE */

	sta = ap_get_sta(hapd, peer_addr);

	if (cotdma == 1) {
		if (!mc->mapc_cotdma_enable) {
			return os_snprintf(buf, buflen,
					   "FAIL Co-TDMA disabled (mapc_cotdma_enable=0)\n");
		}

		if (!mapc_is_valid_coap_peer(mc, peer_addr)) {
			if (mc->valid_ap_count >= MAPC_MAX_VALID_AP_LIST) {
				wpa_printf(MSG_ERROR,
					   "SET_MAPC_STA: valid_coap_list full "
					   "(max %d) — remove an entry first",
					   MAPC_MAX_VALID_AP_LIST);
				return os_snprintf(buf, buflen,
						   "FAIL valid_coap_list full "
						   "(max %d) — remove an entry "
						   "first\n",
						   MAPC_MAX_VALID_AP_LIST);
			}
			os_memcpy(mc->valid_coap_list[mc->valid_ap_count],
				  peer_addr, ETH_ALEN);
			mc->valid_ap_count++;
			wpa_printf(MSG_INFO,
				   "SET_MAPC_STA: auto-added " MACSTR
				   " to valid_coap_list (count=%d)",
				   MAC2STR(peer_addr), mc->valid_ap_count);
		}

		if (!sta || !sta->is_mapc_peer) {
			wpa_printf(MSG_INFO,
				   "SET_MAPC_STA: " MACSTR " not discovered, "
				   "sending unicast discovery",
				   MAC2STR(peer_addr));
			ret = mapc_send_discovery_request(hapd, peer_addr);
			if (ret < 0) {
				wpa_printf(MSG_ERROR,
					   "SET_MAPC_STA: discovery request failed (ret=%d)",
					   ret);
				return os_snprintf(buf, buflen,
						   "FAIL discovery request failed (ret=%d)\n",
						   ret);
			}
			return os_snprintf(buf, buflen,
					   "OK discovery initiated - "
					   "reissue set_mapc_sta after MAPC-DISCOVERY-REPORT\n");
		}

		/* Peer exists — check Co-TDMA capability */
		if (!(sta->mapc_params.mapc_capability_bitmap &
		      BIT(MAPC_CAPABILITY_COTDMA_SUPPORT))) {
			return os_snprintf(buf, buflen,
					   "FAIL peer " MACSTR
					   " does not support Co-TDMA\n",
					   MAC2STR(peer_addr));
		}

		/* Idempotent: agreement already active */
		if (sta->mapc_params.agreement_cnt[MAPC_SCHEME_CO_TDMA] > 0) {
			wpa_printf(MSG_INFO,
				   "SET_MAPC_STA: Co-TDMA agreement with "
				   MACSTR " already active",
				   MAC2STR(peer_addr));
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				"MAPC-NEGOTIATION-STATUS negotiation_status=1 "
				"src_bssid=" MACSTR " dst_bssid=" MACSTR,
				MAC2STR(hapd->own_addr), MAC2STR(peer_addr));
			return os_snprintf(buf, buflen, "OK already established\n");
		}

		/* Trigger Co-TDMA ESTABLISHMENT negotiation */
		os_memset(reqs, 0, sizeof(reqs));
		reqs[MAPC_SCHEME_CO_TDMA].include = true;
		reqs[MAPC_SCHEME_CO_TDMA].op_type = MAPC_OP_AGREEMENT_ESTABLISHMENT;
		ret = mapc_send_negotiation_request(hapd, peer_addr, reqs);
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "SET_MAPC_STA: negotiation request failed (ret=%d)",
				   ret);
			return os_snprintf(buf, buflen,
					   "FAIL negotiation request failed (ret=%d)\n",
					   ret);
		}
		wpa_printf(MSG_INFO,
			   "SET_MAPC_STA: Co-TDMA ESTABLISHMENT initiated with "
			   MACSTR, MAC2STR(peer_addr));
		return os_snprintf(buf, buflen,
				   "OK Co-TDMA negotiation initiated\n");
	}

	if (!sta || !sta->is_mapc_peer ||
	    sta->mapc_params.peer_state != MAPC_PEER_STATE_ACTIVE ||
	    sta->mapc_params.agreement_cnt[MAPC_SCHEME_CO_TDMA] == 0) {
		wpa_printf(MSG_INFO,
			   "SET_MAPC_STA: no active Co-TDMA agreement with "
			   MACSTR " — nothing to do", MAC2STR(peer_addr));
		return os_snprintf(buf, buflen,
				   "OK no active Co-TDMA agreement\n");
	}

	/* Cannot teardown while another negotiation is in progress */
	if (sta->mapc_params.neg_state != MAPC_NEG_IDLE) {
		wpa_printf(MSG_INFO,
			   "SET_MAPC_STA: negotiation in progress for " MACSTR
			   " (state=%d) - retry later",
			   MAC2STR(peer_addr), sta->mapc_params.neg_state);
		return os_snprintf(buf, buflen,
				   "FAIL negotiation in progress, retry later\n");
	}

	os_memset(reqs, 0, sizeof(reqs));
	reqs[MAPC_SCHEME_CO_TDMA].include = true;
	reqs[MAPC_SCHEME_CO_TDMA].op_type = MAPC_OP_AGREEMENT_TEARDOWN;
	ret = mapc_send_negotiation_request(hapd, peer_addr, reqs);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "SET_MAPC_STA: teardown request failed (ret=%d)", ret);
		return os_snprintf(buf, buflen,
				   "FAIL teardown request failed (ret=%d)\n", ret);
	}

	return os_snprintf(buf, buflen, "OK Co-TDMA teardown initiated\n");
}
#endif /* CONFIG_IEEE80211BN */

static int hostapd_ctrl_iface_set_rtt_responder_role(struct hostapd_data *hapd,
						     char *value)
{
	char *end = NULL;
	long role;
	int ret;

	if (!hapd || !hapd->conf || !hapd->iface || !value)
		return -1;

	errno = 0;
	role = strtol(value, &end, 0);
	if (errno || end == value) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_RTT_RESPONDER_ROLE: invalid value");
		return -1;
	}
	while (end && (*end == ' ' || *end == '\n' || *end == '\r' ||
		       *end == '\t'))
		end++;
	if (end && *end != '\0') {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_RTT_RESPONDER_ROLE: trailing characters");
		return -1;
	}
	if (role < 0 || role > 0x7) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_RTT_RESPONDER_ROLE: value must be 0..0x7");
		return -1;
	}
	if (hapd->conf->rtt_responder_role == (int)role)
		return 0;

	ret = hostapd_drv_set_rtt_responder_role(hapd, (int)role);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_RTT_RESPONDER_ROLE: vendor cmd failed (%d)",
			   ret);
		return -1;
	}

	hapd->conf->rtt_responder_role = (int)role;
	hapd->rtt_role_fw_sent = true;
	if (ieee802_11_update_beacons(hapd->iface)) {
		hapd->rtt_role_fw_sent = false;
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_RTT_RESPONDER_ROLE: beacon update failed");
		return -1;
	}
	hapd->rtt_role_fw_sent = false;

	return 0;
}

static int hostapd_ctrl_iface_get_rtt_responder_role(struct hostapd_data *hapd,
						     char *reply,
						     size_t reply_size)
{
	int role;

	if (!hapd || !hapd->conf || !reply)
		return -1;

	role = hapd->conf->rtt_responder_role;

	return os_snprintf(reply, reply_size, "0x%x\n", role);
}

static int hostapd_ctrl_iface_receive_process(struct hostapd_data *hapd,
					      char *buf, char *reply,
					      int reply_size,
					      struct sockaddr_storage *from,
					      socklen_t fromlen)
{
	int reply_len;
#if defined(CONFIG_CTRL_IFACE_MIB) || defined(CONFIG_DPP)
	int res;
#endif /* CONFIG_CTRL_IFACE_MIB || CONFIG_DPP */

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "CLOSE_LOG") == 0) {
		wpa_debug_stop_log();
	} else if (os_strncmp(buf, "NOTE ", 5) == 0) {
		wpa_printf(MSG_INFO, "NOTE: %s", buf + 5);
		wpa_trace_set_context(buf + 5);
	} else if (os_strcmp(buf, "STATUS") == 0) {
		reply_len = hostapd_ctrl_iface_status(hapd, reply,
						      reply_size);
	} else if (os_strcmp(buf, "STATUS-DRIVER") == 0) {
		reply_len = hostapd_drv_status(hapd, reply, reply_size);
#ifdef CONFIG_CTRL_IFACE_MIB
	} else if (os_strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd->wpa_auth, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#ifndef CONFIG_NO_RADIUS
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#endif /* CONFIG_NO_RADIUS */
	} else if (os_strncmp(buf, "MIB ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_mib(hapd, reply, reply_size,
						   buf + 4);
	} else if (os_strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (os_strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
#endif
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, from, fromlen, NULL))
			reply_len = -1;
	} else if (os_strncmp(buf, "ATTACH ", 7) == 0) {
		if (hostapd_ctrl_iface_attach(hapd, from, fromlen, buf + 7))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, from, fromlen))
			reply_len = -1;
	} else if (os_strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "NEW_STA ", 8) == 0) {
		if (hostapd_ctrl_iface_new_sta(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "DEAUTHENTICATE ", 15) == 0) {
		if (hostapd_ctrl_iface_deauthenticate(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISASSOCIATE ", 13) == 0) {
		if (hostapd_ctrl_iface_disassociate(hapd, buf + 13))
			reply_len = -1;
#ifdef HOSTAPD_EXTERNAL_PLUGIN_TESTAPP
	} else if (os_strncmp(buf, "CONFIGURE-PLUGIN ", 17) == 0) {
		reply_len = hostapd_ctrl_iface_configure_plugin(hapd, buf + 17,
							       reply, reply_size);
#endif
#ifdef CONFIG_TAXONOMY
	} else if (os_strncmp(buf, "SIGNATURE ", 10) == 0) {
		reply_len = hostapd_ctrl_iface_signature(hapd, buf + 10,
							reply, reply_size);
#endif /* CONFIG_TAXONOMY */
	} else if (os_strncmp(buf, "DUMP_TK ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_dump_tk(hapd, buf + 8,
						       reply, reply_size);
	} else if (os_strncmp(buf, "POLL_STA ", 9) == 0) {
		if (hostapd_ctrl_iface_poll_sta(hapd, buf + 9))
			reply_len = -1;
	} else if (os_strcmp(buf, "STOP_AP") == 0) {
		if (hostapd_ctrl_iface_stop_ap(hapd))
			reply_len = -1;
#ifdef NEED_AP_MLME
	} else if (os_strncmp(buf, "SA_QUERY ", 9) == 0) {
		if (hostapd_ctrl_iface_sa_query(hapd, buf + 9))
			reply_len = -1;
#endif /* NEED_AP_MLME */
#ifdef CONFIG_WPS
	} else if (os_strncmp(buf, "WPS_PIN ", 8) == 0) {
		if (hostapd_ctrl_iface_wps_pin(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CHECK_PIN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_check_pin(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strcmp(buf, "WPS_PBC") == 0) {
		if (hostapd_wps_button_pushed(hapd, NULL))
			reply_len = -1;
	} else if (os_strcmp(buf, "WPS_CANCEL") == 0) {
		if (hostapd_wps_cancel(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_AP_PIN ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_wps_ap_pin(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CONFIG ", 11) == 0) {
		if (hostapd_ctrl_iface_wps_config(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_GET_STATUS", 13) == 0) {
		reply_len = hostapd_ctrl_iface_wps_get_status(hapd, reply,
							      reply_size);
#ifdef CONFIG_WPS_NFC
	} else if (os_strncmp(buf, "WPS_NFC_TAG_READ ", 17) == 0) {
		if (hostapd_ctrl_iface_wps_nfc_tag_read(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_NFC_CONFIG_TOKEN ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_config_token(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "WPS_NFC_TOKEN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_token(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_GET_HANDOVER_SEL ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_nfc_get_handover_sel(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_REPORT_HANDOVER ", 20) == 0) {
		if (hostapd_ctrl_iface_nfc_report_handover(hapd, buf + 20))
			reply_len = -1;
#endif /* CONFIG_WPS_NFC */
#endif /* CONFIG_WPS */
#ifdef CONFIG_INTERWORKING
	} else if (os_strncmp(buf, "SET_QOS_MAP_SET ", 16) == 0) {
		if (hostapd_ctrl_iface_set_qos_map_set(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_QOS_MAP_CONF ", 18) == 0) {
		if (hostapd_ctrl_iface_send_qos_map_conf(hapd, buf + 18))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_BSS_PRIORITY_STATUS ", 23) == 0) {
		if (hostapd_ctrl_iface_set_bss_priority_status(hapd, buf + 23))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_BSS_PRIORITY ", 16) == 0) {
		if (hostapd_ctrl_iface_set_bss_priority(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_BSS_PRIORITY_STATUS ", 23) == 0) {
		reply_len = hostapd_ctrl_iface_get_bss_priority_status(
				hapd, reply, reply_size);
	} else if (os_strncmp(buf, "GET_BSS_PRIORITY ", 16) == 0) {
		reply_len = hostapd_ctrl_iface_get_bss_priority(
				hapd, reply, reply_size);
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	} else if (os_strncmp(buf, "HS20_DEAUTH_REQ ", 16) == 0) {
		if (hostapd_ctrl_iface_hs20_deauth_req(hapd, buf + 16))
			reply_len = -1;
#endif /* CONFIG_HS20 */
#ifdef CONFIG_WNM_AP
	} else if (os_strncmp(buf, "DISASSOC_IMMINENT ", 18) == 0) {
		if (hostapd_ctrl_iface_disassoc_imminent(hapd, buf + 18))
			reply_len = -1;
	} else if (os_strncmp(buf, "ESS_DISASSOC ", 13) == 0) {
		if (hostapd_ctrl_iface_ess_disassoc(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "BSS_TM_REQ ", 11) == 0) {
		if (hostapd_ctrl_iface_bss_tm_req(hapd, buf + 11))
			reply_len = -1;
	} else if (os_strncmp(buf, "COLOC_INTF_REQ ", 15) == 0) {
		if (hostapd_ctrl_iface_coloc_intf_req(hapd, buf + 15))
			reply_len = -1;
#endif /* CONFIG_WNM_AP */
	} else if (os_strcmp(buf, "GET_CONFIG") == 0) {
		reply_len = hostapd_ctrl_iface_get_config(hapd, reply,
							  reply_size);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (hostapd_ctrl_iface_set(hapd, buf + 4))
			reply_len = -1;
	} else if (os_strncmp(buf, "LOG_PEER ", 9) == 0) {
		if (hostapd_ctrl_iface_log_peer(hapd, buf + 9))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_get(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strcmp(buf, "ENABLE") == 0) {
		if (hostapd_ctrl_iface_enable(hapd->iface))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISABLE_BSS", 11) == 0) {
		int tbtt = -1;
		const char *pos = buf + 11;

		/* Parse optional argument: DISABLE_BSS [tbtt] */
		if (*pos == ' ') {
			pos++;
			if (*pos)
				tbtt = atoi(pos);
		}
		if (hostapd_ctrl_iface_disable_bss(hapd, tbtt))
			reply_len = -1;
	} else if (os_strcmp(buf, "ENABLE_BSS") == 0) {
		if (hostapd_ctrl_iface_enable_bss(hapd))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD_WPA_PSK") == 0) {
		if (hostapd_ctrl_iface_reload_wpa_psk(hapd))
			reply_len = -1;
#ifdef CONFIG_IEEE80211R_AP
	} else if (os_strcmp(buf, "GET_RXKHS") == 0) {
		reply_len = hostapd_ctrl_iface_get_rxkhs(hapd, reply,
							 reply_size);
	} else if (os_strcmp(buf, "RELOAD_RXKHS") == 0) {
		if (hostapd_ctrl_iface_reload_rxkhs(hapd))
			reply_len = -1;
#endif /* CONFIG_IEEE80211R_AP */
	} else if (os_strcmp(buf, "RELOAD_BSS") == 0) {
		if (hostapd_ctrl_iface_reload_bss(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "RELOAD_CONFIG_BSS ", 18) == 0) {
		if (hostapd_reload_config_bss(hapd->iface, hapd->conf->iface, buf+18))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD_CONFIG") == 0) {
		if (hostapd_reload_config(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD") == 0) {
		if (hostapd_ctrl_iface_reload(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "DISABLE") == 0) {
		if (hostapd_ctrl_iface_disable(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "UPDATE_BEACON") == 0) {
		hapd->is_update_beacon = true;
		if (ieee802_11_set_beacon(hapd))
			reply_len = -1;
		hapd->is_update_beacon = false;
#ifdef CONFIG_IEEE80211BN
	} else if (os_strncmp(buf, "UPDATE_UHR_FEATURES ", 20) == 0) {
		if (hostapd_ctrl_iface_update_uhr_features(hapd, buf + 20))
			reply_len = -1;
#endif /* CONFIG_IEEE80211BN */
	} else if (os_strncmp(buf, "MLD_ADD_LINK ", 13) == 0) {
		if (hostapd_ctrl_iface_add(hapd->iface->interfaces, buf + 13))
			reply_len = -1;
		else
			hostapd_ucode_update_interfaces();
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strncmp(buf, "RADAR ", 6) == 0) {
		if (hostapd_ctrl_iface_radar(hapd, buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "AWGN ", 5) == 0) {
		if (hostapd_ctrl_iface_awgn(hapd, buf + 5))
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_TX ", 8) == 0) {
		if (hostapd_ctrl_iface_mgmt_tx(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_TX_STATUS_PROCESS ", 23) == 0) {
		if (hostapd_ctrl_iface_mgmt_tx_status_process(hapd,
							      buf + 23) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_RX_PROCESS ", 16) == 0) {
		if (hostapd_ctrl_iface_mgmt_rx_process(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_RX ", 9) == 0) {
		if (hostapd_ctrl_iface_eapol_rx(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_TX ", 9) == 0) {
		if (hostapd_ctrl_iface_eapol_tx(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_CONFIG ", 17) == 0) {
		if (hostapd_ctrl_iface_data_test_config(hapd, buf + 17) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_TX ", 13) == 0) {
		if (hostapd_ctrl_iface_data_test_tx(hapd, buf + 13) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_FRAME ", 16) == 0) {
		if (hostapd_ctrl_iface_data_test_frame(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "TEST_ALLOC_FAIL ", 16) == 0) {
		if (testing_set_fail_pattern(true, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_ALLOC_FAIL") == 0) {
		reply_len = testing_get_fail_pattern(true, reply, reply_size);
	} else if (os_strncmp(buf, "TEST_FAIL ", 10) == 0) {
		if (testing_set_fail_pattern(false, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_FAIL") == 0) {
		reply_len = testing_get_fail_pattern(false, reply, reply_size);
	} else if (os_strncmp(buf, "RESET_PN ", 9) == 0) {
		if (hostapd_ctrl_reset_pn(hapd, buf + 9) < 0)
			reply_len = -1;
#endif /* CONFIG_TESTING_OPTIONS */
#if defined(CONFIG_QCN_EXTN) || defined(CONFIG_TESTING_OPTIONS)
	} else if (os_strncmp(buf, "SET_KEY ", 8) == 0) {
		if (hostapd_ctrl_set_key(hapd, buf + 8) < 0)
			reply_len = -1;
#endif /* CONFIG_QCN_EXTN || CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strncmp(buf, "RESEND_M1 ", 10) == 0) {
		if (hostapd_ctrl_resend_m1(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "RESEND_M3 ", 10) == 0) {
		if (hostapd_ctrl_resend_m3(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "RESEND_GROUP_M1 ", 16) == 0) {
		if (hostapd_ctrl_resend_group_m1(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "REKEY_PTK ", 10) == 0) {
		if (hostapd_ctrl_rekey_ptk(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "REKEY_GTK") == 0) {
		if (wpa_auth_rekey_gtk(hapd->wpa_auth) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_PMK ", 8) == 0) {
		reply_len = hostapd_ctrl_get_pmk(hapd, buf + 8, reply,
						 reply_size);
	} else if (os_strncmp(buf, "REGISTER_FRAME ", 15) == 0) {
		if (hostapd_ctrl_register_frame(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_BW ", 7) == 0) {
		/* note: preserve the space for hostapd_parse_freq_params() */
		if (hostapd_ctrl_iface_set_bw(hapd->iface, buf + 6))
			reply_len = -1;
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strncmp(buf, "CHAN_SWITCH ", 12) == 0) {
		if (hostapd_ctrl_iface_chan_switch(hapd->iface, buf + 12))
			reply_len = -1;
#ifdef CONFIG_IEEE80211AX
	} else if (os_strncmp(buf, "COLOR_CHANGE ", 13) == 0) {
		if (hostapd_ctrl_iface_color_change(hapd->iface, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "COLOR_COLLISION_AP_PERIOD ", 26) == 0) {
		if (hostapd_ctrl_iface_bsscolor_collision_ap_period(hapd->iface, buf + 26))
			reply_len = -1;
	} else if (os_strncmp(buf, "COLOR_CCA_COUNT ", 16) == 0) {
		if (hostapd_ctrl_iface_bsscolor_cca_count(hapd->iface, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_6GHZ_PWR_MODE ", 18) == 0) {
		if (hostapd_ctrl_iface_set_pwr_mode(hapd->iface, buf + 18))
			reply_len = -1;
#endif /* CONFIG_IEEE80211AX */
	} else if (os_strncmp(buf, "NOTIFY_CW_CHANGE ", 17) == 0) {
		if (hostapd_ctrl_iface_notify_cw_change(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "VENDOR ", 7) == 0) {
		reply_len = hostapd_ctrl_iface_vendor(hapd, buf + 7, reply,
						      reply_size);
	} else if (os_strcmp(buf, "switch_to_rcac") == 0) {
#ifdef NEED_AP_MLME
		if (hostapd_dfs_agile_cac_switch(hapd->iface) < 0)
			reply_len = -1;
#else /* NEED_AP_MLME */
		reply_len = -1;
#endif /* NEED_AP_MLME */
	} else if (os_strcmp(buf, "bgcac_start") == 0) {
#ifdef NEED_AP_MLME
		if (hostapd_start_background_cac(hapd->iface) < 0)
			reply_len = -1;
#else /* NEED_AP_MLME */
		reply_len = -1;
#endif /* NEED_AP_MLME */
	} else if (os_strcmp(buf, "ERP_FLUSH") == 0) {
		ieee802_1x_erp_flush(hapd);
#ifdef RADIUS_SERVER
		radius_server_erp_flush(hapd->radius_srv);
#endif /* RADIUS_SERVER */
	} else if (os_strncmp(buf, "EAPOL_REAUTH ", 13) == 0) {
		if (hostapd_ctrl_iface_eapol_reauth(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_SET ", 10) == 0) {
		if (hostapd_ctrl_iface_eapol_set(hapd, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "LOG_LEVEL", 9) == 0) {
		reply_len = hostapd_ctrl_iface_log_level(
			hapd, buf + 9, reply, reply_size);
#ifdef NEED_AP_MLME
	} else if (os_strcmp(buf, "TRACK_STA_LIST") == 0) {
		reply_len = hostapd_ctrl_iface_track_sta_list(
			hapd, reply, reply_size);
	} else if (os_strcmp(buf, "DUMP_BEACON") == 0) {
		reply_len = hostapd_ctrl_iface_dump_beacon(hapd, reply,
							   reply_size);
#endif /* NEED_AP_MLME */
	} else if (os_strcmp(buf, "PMKSA") == 0) {
		reply_len = hostapd_ctrl_iface_pmksa_list(hapd, reply,
							  reply_size);
	} else if (os_strcmp(buf, "PMKSA_FLUSH") == 0) {
		hostapd_ctrl_iface_pmksa_flush(hapd);
	} else if (os_strncmp(buf, "PMKSA_ADD ", 10) == 0) {
		if (hostapd_ctrl_iface_pmksa_add(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_NEIGHBOR ", 13) == 0) {
		if (hostapd_ctrl_iface_set_neighbor(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strcmp(buf, "SHOW_NEIGHBOR") == 0) {
		reply_len = hostapd_ctrl_iface_show_neighbor(hapd, reply,
							     reply_size);
	} else if (os_strncmp(buf, "REMOVE_NEIGHBOR ", 16) == 0) {
		if (hostapd_ctrl_iface_remove_neighbor(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_NEIGHBOR ", 14) == 0) {
		if (hostapd_ctrl_iface_send_neighbor(hapd, buf + 14))
			reply_len = -1;
	} else if (os_strncmp(buf, "REQ_LCI ", 8) == 0) {
		if (hostapd_ctrl_iface_req_lci(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "REQ_RANGE ", 10) == 0) {
		if (hostapd_ctrl_iface_req_range(hapd, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_RTT_RESPONDER_ROLE ", 23) == 0) {
		if (hostapd_ctrl_iface_set_rtt_responder_role(hapd, buf + 23))
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_RTT_RESPONDER_ROLE") == 0) {
		reply_len = hostapd_ctrl_iface_get_rtt_responder_role(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "REQ_BEACON ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_req_beacon(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "SHOW_RRM_BEACON_REPORT", 22) == 0) {
		reply_len = hostapd_ctrl_iface_show_rrm_beacon_report(hapd,
								      reply,
								      reply_size);
	} else if (os_strncmp(buf, "REQ_LINK_MEASUREMENT ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_req_link_measurement(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "REQ_CHANNEL_LOAD ", 17) == 0) {
		reply_len = hostapd_ctrl_iface_req_channel_load(
			hapd, buf + 17, reply, reply_size);
	} else if (os_strncmp(buf, "REQ_NOISE_HISTOGRAM ", 20) == 0) {
		reply_len = hostapd_ctrl_iface_req_noise_histogram(
			hapd, buf + 20, reply, reply_size);
	} else if (os_strcmp(buf, "DRIVER_FLAGS") == 0) {
		reply_len = hostapd_ctrl_driver_flags(hapd->iface, reply,
						      reply_size);
	} else if (os_strcmp(buf, "DRIVER_FLAGS2") == 0) {
		reply_len = hostapd_ctrl_driver_flags2(hapd->iface, reply,
						       reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else if (os_strncmp(buf, "ACCEPT_ACL ", 11) == 0) {
		if (os_strncmp(buf + 11, "ADD_MAC ", 8) == 0) {
			/*
			 * When the first MAC is added to the accept list,
			 * re-evaluate all connected STAs to enforce the
			 * ACL. In DENY_UNLESS_ACCEPTED (mode 1) and
			 * ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST (mode 3),
			 * any STA not in the accept list must be
			 * disconnected. hostapd_disassoc_accept_mac() is
			 * a no-op for modes that do not require a
			 * whitelist check (0,2,4).
			 */
			int was_empty = (hapd->conf->num_accept_mac == 0 &&
					 hapd->conf->num_accept_mac_masked == 0);
			if (hostapd_ctrl_iface_acl_add_mac(
				    hapd->conf, true, buf + 19) ||
			    hostapd_set_acl(hapd))
				reply_len = -1;
			if (was_empty && reply_len != -1) {
				if (hostapd_disassoc_accept_mac(hapd) ||
				    hostapd_disassoc_deny_mac(hapd))
					reply_len = -1;
			}
		} else if (os_strncmp((buf + 11), "DEL_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_del_mac(
				    hapd->conf, true, buf + 19) ||
			    hostapd_set_acl(hapd) ||
			    hostapd_disassoc_accept_mac(hapd) ||
			    hostapd_disassoc_deny_mac(hapd))
				reply_len = -1;
		} else if (os_strcmp(buf + 11, "SHOW") == 0) {
			reply_len = hostapd_ctrl_iface_acl_show_mac(
				hapd->conf, true, reply, reply_size);
		} else if (os_strcmp(buf + 11, "CLEAR") == 0) {
			hostapd_ctrl_iface_acl_clear_list(hapd->conf, true);
			if (hostapd_set_acl(hapd) ||
			    hostapd_disassoc_accept_mac(hapd) ||
			    hostapd_disassoc_deny_mac(hapd))
				reply_len = -1;
		} else {
			reply_len = -1;
		}
	} else if (os_strncmp(buf, "DENY_ACL ", 9) == 0) {
		if (os_strncmp(buf + 9, "ADD_MAC ", 8) == 0) {
			/*
			 * When the first MAC is added to the deny list,
			 * re-evaluate all connected STAs to enforce the
			 * ACL. In DENY_UNLESS_ACCEPTED (mode 1) and
			 * ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST (mode 3),
			 * any STA not in the accept list must be
			 * disconnected. hostapd_disassoc_accept_mac() is
			 * a no-op for modes that do not require a
			 * whitelist check (0,2,4).
			 */
			int was_empty = (hapd->conf->num_deny_mac == 0 &&
					 hapd->conf->num_deny_mac_masked == 0);
			if (hostapd_ctrl_iface_acl_add_mac(
				    hapd->conf, false, buf + 17) ||
			    hostapd_set_acl(hapd) ||
			    hostapd_disassoc_deny_mac(hapd))
				reply_len = -1;
			if (was_empty && reply_len != -1) {
				if (hostapd_disassoc_accept_mac(hapd))
					reply_len = -1;
			}
		} else if (os_strncmp(buf + 9, "DEL_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_del_mac(
				    hapd->conf, false, buf + 17) ||
			    hostapd_set_acl(hapd))
				reply_len = -1;
		} else if (os_strcmp(buf + 9, "SHOW") == 0) {
			reply_len = hostapd_ctrl_iface_acl_show_mac(
				hapd->conf, false, reply, reply_size);
		} else if (os_strcmp(buf + 9, "CLEAR") == 0) {
			hostapd_ctrl_iface_acl_clear_list(hapd->conf, false);
			if (hostapd_set_acl(hapd))
				reply_len = -1;
		} else {
			reply_len = -1;
		}
#ifdef CONFIG_DPP
	} else if (os_strncmp(buf, "DPP_QR_CODE ", 12) == 0) {
		res = hostapd_dpp_qr_code(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_URI ", 12) == 0) {
		res = hostapd_dpp_nfc_uri(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_HANDOVER_REQ ", 21) == 0) {
		res = hostapd_dpp_nfc_handover_req(hapd, buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_HANDOVER_SEL ", 21) == 0) {
		res = hostapd_dpp_nfc_handover_sel(hapd, buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_SET_KEYPAIR ", 26) == 0) {
		res = dpp_bootstrap_set_keypair(hapd->iface->interfaces->dpp, buf + 26);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_GEN ", 18) == 0) {
		res = dpp_bootstrap_gen(hapd->iface->interfaces->dpp, buf + 18);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_REMOVE ", 21) == 0) {
		if (dpp_bootstrap_remove(hapd->iface->interfaces->dpp,
					 buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_GET_URI ", 22) == 0) {
		const char *uri;

		uri = dpp_bootstrap_get_uri(hapd->iface->interfaces->dpp,
					    atoi(buf + 22));
		if (!uri) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%s", uri);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_INFO ", 19) == 0) {
		reply_len = dpp_bootstrap_info(hapd->iface->interfaces->dpp,
					       atoi(buf + 19),
			reply, reply_size);
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_SET ", 18) == 0) {
		if (dpp_bootstrap_set(hapd->iface->interfaces->dpp,
				      atoi(buf + 18),
				      os_strchr(buf + 18, ' ')) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_AUTH_INIT ", 14) == 0) {
		if (hostapd_dpp_auth_init(hapd, buf + 13) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_LISTEN ", 11) == 0) {
		if (hostapd_dpp_listen(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_STOP_LISTEN") == 0) {
		hostapd_dpp_stop(hapd);
		hostapd_dpp_listen_stop(hapd);
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_ADD", 20) == 0) {
		res = dpp_configurator_add(hapd->iface->interfaces->dpp,
					   buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_SET ", 21) == 0) {
		if (dpp_configurator_set(hapd->iface->interfaces->dpp,
					 buf + 20) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_REMOVE ", 24) == 0) {
		if (dpp_configurator_remove(hapd->iface->interfaces->dpp,
					    buf + 24) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_SIGN ", 22) == 0) {
		if (hostapd_dpp_configurator_sign(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_GET_KEY ", 25) == 0) {
		reply_len = dpp_configurator_get_key_id(
			hapd->iface->interfaces->dpp,
			atoi(buf + 25),
			reply, reply_size);
	} else if (os_strncmp(buf, "DPP_PKEX_ADD ", 13) == 0) {
		res = hostapd_dpp_pkex_add(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_PKEX_REMOVE ", 16) == 0) {
		if (hostapd_dpp_pkex_remove(hapd, buf + 16) < 0)
			reply_len = -1;
#ifdef CONFIG_DPP2
	} else if (os_strncmp(buf, "DPP_CONTROLLER_START ", 21) == 0) {
		if (hostapd_dpp_controller_start(hapd, buf + 20) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_CONTROLLER_START") == 0) {
		if (hostapd_dpp_controller_start(hapd, NULL) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_CONTROLLER_STOP") == 0) {
		dpp_controller_stop(hapd->iface->interfaces->dpp);
	} else if (os_strncmp(buf, "DPP_CHIRP ", 10) == 0) {
		if (hostapd_dpp_chirp(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_STOP_CHIRP") == 0) {
		hostapd_dpp_chirp_stop(hapd);
	} else if (os_strncmp(buf, "DPP_RELAY_ADD_CONTROLLER ", 25) == 0) {
		if (hostapd_dpp_add_controller(hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_RELAY_REMOVE_CONTROLLER ", 28) == 0) {
		hostapd_dpp_remove_controller(hapd, buf + 28);
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_DPP3
	} else if (os_strcmp(buf, "DPP_PUSH_BUTTON") == 0) {
		if (hostapd_dpp_push_button(hapd, NULL) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_PUSH_BUTTON ", 16) == 0) {
		if (hostapd_dpp_push_button(hapd, buf + 15) < 0)
			reply_len = -1;
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	} else if (os_strncmp(buf, "NAN_PUBLISH ", 12) == 0) {
		reply_len = hostapd_ctrl_nan_publish(hapd, buf + 12, reply,
						     reply_size);
	} else if (os_strncmp(buf, "NAN_CANCEL_PUBLISH ", 19) == 0) {
		if (hostapd_ctrl_nan_cancel_publish(hapd, buf + 19) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_UPDATE_PUBLISH ", 19) == 0) {
		if (hostapd_ctrl_nan_update_publish(hapd, buf + 19) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_SUBSCRIBE ", 14) == 0) {
		reply_len = hostapd_ctrl_nan_subscribe(hapd, buf + 14, reply,
						       reply_size);
	} else if (os_strncmp(buf, "NAN_CANCEL_SUBSCRIBE ", 21) == 0) {
		if (hostapd_ctrl_nan_cancel_subscribe(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_TRANSMIT ", 13) == 0) {
		if (hostapd_ctrl_nan_transmit(hapd, buf + 13) < 0)
			reply_len = -1;
#endif /* CONFIG_NAN_USD */
#ifdef RADIUS_SERVER
	} else if (os_strncmp(buf, "DAC_REQUEST ", 12) == 0) {
		if (radius_server_dac_request(hapd->radius_srv, buf + 12) < 0)
			reply_len = -1;
#endif /* RADIUS_SERVER */
	} else if(os_strcmp(buf, "CHANNEL_BW") == 0) {
		reply_len = hostapd_ctrl_iface_channel_bw(hapd->iface, reply,
							  reply_size);

	} else if (os_strncmp(buf, "GET_CAPABILITY ", 15) == 0) {
		reply_len = hostapd_ctrl_iface_get_capability(
			hapd, buf + 15, reply, reply_size);
#ifdef CONFIG_PASN
	} else if (os_strcmp(buf, "PTKSA_CACHE_LIST") == 0) {
		reply_len = ptksa_cache_list(hapd->ptksa, reply, reply_size);
#endif /* CONFIG_PASN */
#ifdef ANDROID
	} else if (os_strncmp(buf, "DRIVER ", 7) == 0) {
		reply_len = hostapd_ctrl_iface_driver_cmd(hapd, buf + 7, reply,
							  reply_size);
#endif /* ANDROID */
#ifdef CONFIG_ATF_OFFLOAD
	} else if (os_strncmp(buf, "ATF_OFFLOAD ", 12) == 0) {
		reply_len = hostapd_ctrl_iface_config_atf_offload(hapd, buf + 12,
								  reply, reply_size);
#endif
#ifdef CONFIG_IEEE80211BE
	} else if (os_strcmp(buf, "ENABLE_MLD") == 0) {
		if (hostapd_ctrl_iface_enable_mld(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "DISABLE_MLD") == 0) {
		if (hostapd_ctrl_iface_disable_mld(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "STOP_MLD") == 0) {
		if (hostapd_ctrl_iface_stop_mld(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "LINK_REMOVE ", 12) == 0) {
		if (hostapd_ctrl_iface_link_remove(hapd, buf + 12,
						   reply, reply_size))
			reply_len = -1;
		else
			hostapd_ucode_update_interfaces();
	} else if (os_strncmp(buf, "EPCS ", 5) == 0) {
		reply_len = hostapd_epcs_handle_cli(hapd, buf + 5,
						    reply, reply_size);
	} else if (os_strncmp(buf, "NEGOTIATED_TTLM ", 16) == 0) {
		reply_len = hostapd_ctrl_iface_negotiated_ttlm(hapd, buf + 16,
							       reply, reply_size);
	} else if (os_strncmp(buf, "ML_MAX_REC_LINKS ", 17) == 0) {
		if (hostapd_ctrl_iface_conf_ml_rec_links(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "ADVERTISED_TTLM ", 16) == 0) {
		if (hostapd_ctrl_iface_advertise_ttlm(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_CHANNEL_USAGE_ELEMENT ", 26) == 0) {
		if (hostapd_ctrl_iface_set_channel_usage_element(hapd, buf + 26))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_UNSOLICITED_MSCS_RESP ", 27) == 0) {
		if (hostapd_ctrl_iface_send_mscs_resp(hapd, buf + 27))
			reply_len = -1;
	} else if (os_strcmp(buf, "DUMP_MSCS_CTXT") == 0) {
		reply_len = hostapd_ctrl_iface_dump_mscs_ctxt(hapd,
							      reply, reply_size);
#endif /* CONFIG_IEEE80211BE */
	} else if (os_strncmp(buf, "SET_DSCP_POLICY ", 16) == 0) {
		if (hostapd_ctrl_iface_set_dscp_policy(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "ADD_TPE ", 8) == 0) {
		if (hostapd_ctrl_iface_set_tpe(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "DEL_TPE ", 8) == 0) {
		if (hostapd_ctrl_iface_del_tpe(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_UNSOLICITED_DSCP_REQ ", 26) == 0) {
		if (hostapd_ctrl_send_unsolicited_dscp_req(hapd, buf + 26))
			reply_len = -1;
	} else if (os_strncmp(buf, "CHAIN_MASK ", 11) == 0) {
		if (hostapd_ctrl_set_tx_rx_chain_mask(hapd, buf+11,
						     reply, reply_size))
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_CHAIN_MASK") == 0) {
		reply_len = hostapd_ctrl_get_chain_mask(hapd, reply, reply_size);
#ifdef CONFIG_QCN_EXTN
	} else if (os_strncmp(buf, "AFC ", 4) == 0) {
		reply_len = hostapd_afc_handle_cli(hapd, buf + 4,
						   reply, reply_size);
#endif /* CONFIG_QCN_EXTN */
	} else if (os_strncmp(buf, "CLEAR_AFC_PAYLOAD", 17) == 0) {
		if (hostapd_ctrl_iface_clear_afc_payload(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "RESET_AFC", 9) == 0) {
		if (hostapd_ctrl_iface_reset_afc(hapd, buf + 9))
			reply_len = -1;
#ifdef CONFIG_IEEE80211AX
	} else if (os_strncasecmp(buf, "set_he_bfee_sts ", 16) == 0) {
		if (hostapd_ctrl_iface_set_he_bfee_sts(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_bfee_sts") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_bfee_sts(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_multi_tid_aggr ", 22) == 0) {
		if (hostapd_ctrl_iface_set_he_multi_tid_aggr(hapd, buf + 22) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_multi_tid_aggr") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_multi_tid_aggr(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_multi_tid_aggr_rx ", 25) == 0) {
		if (hostapd_ctrl_iface_set_he_multi_tid_aggr(hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_multi_tid_aggr_rx") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_multi_tid_aggr(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_multi_tid_aggr_tx ", 25) == 0) {
		if (hostapd_ctrl_iface_set_he_multi_tid_aggr_tx(hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_multi_tid_aggr_tx") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_multi_tid_aggr_tx(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_max_ampdu_len_exp ", 25) == 0) {
		if (hostapd_ctrl_iface_set_he_max_ampdu_len_exp(hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_max_ampdu_len_exp") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_max_ampdu_len_exp(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_su_ppdu_1x_ltf_800ns_gi ", 31) == 0) {
		if (hostapd_ctrl_iface_set_he_su_ppdu_1x_ltf_800ns_gi(
			    hapd, buf + 31) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_su_ppdu_1x_ltf_800ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_su_ppdu_1x_ltf_800ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_su_mu_ppdu_4x_ltf_800ns_gi ", 34) == 0) {
		if (hostapd_ctrl_iface_set_he_su_mu_ppdu_4x_ltf_800ns_gi(
			    hapd, buf + 34) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_su_mu_ppdu_4x_ltf_800ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_su_mu_ppdu_4x_ltf_800ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_max_frag_msdu ", 21) == 0) {
		if (hostapd_ctrl_iface_set_he_max_frag_msdu(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_max_frag_msdu") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_max_frag_msdu(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_min_frag_size ", 21) == 0) {
		if (hostapd_ctrl_iface_set_he_min_frag_size(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_min_frag_size") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_min_frag_size(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_omi ", 11) == 0) {
		if (hostapd_ctrl_iface_set_he_omi(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_omi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_omi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_ndp_4x_ltf_3200ns_gi ", 28) == 0) {
		if (hostapd_ctrl_iface_set_he_ndp_4x_ltf_3200ns_gi(
			    hapd, buf + 28) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_ndp_4x_ltf_3200ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_ndp_4x_ltf_3200ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_fragmentation ", 21) == 0) {
		if (hostapd_ctrl_iface_set_he_fragmentation(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_fragmentation") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_fragmentation(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_amsdu_in_ampdu_suprt ", 28) == 0) {
		if (hostapd_ctrl_iface_set_he_amsdu_in_ampdu_suprt(
			    hapd, buf + 28) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_amsdu_in_ampdu_suprt") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_amsdu_in_ampdu_suprt(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_subfee_sts_suprt ", 24) == 0) {
		if (hostapd_ctrl_iface_set_he_subfee_sts_suprt(
			    hapd, buf + 24) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_subfee_sts_suprt") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_subfee_sts_suprt(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_max_nc_suprt ", 20) == 0) {
		if (hostapd_ctrl_iface_set_he_max_nc_suprt(hapd, buf + 20) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_max_nc_suprt") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_max_nc_suprt(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_er_su_disable ", 21) == 0) {
		if (hostapd_ctrl_iface_set_he_er_su_disable(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_er_su_disable") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_er_su_disable(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_er_su_ppdu_1x_ltf_800ns_gi ", 34) == 0) {
		if (hostapd_ctrl_iface_set_he_er_su_ppdu_1x_ltf_800ns_gi(
			    hapd, buf + 34) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_er_su_ppdu_1x_ltf_800ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_er_su_ppdu_1x_ltf_800ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_er_su_ppdu_4x_ltf_800ns_gi ", 34) == 0) {
		if (hostapd_ctrl_iface_set_he_er_su_ppdu_4x_ltf_800ns_gi(
			    hapd, buf + 34) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_er_su_ppdu_4x_ltf_800ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_er_su_ppdu_4x_ltf_800ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_1024qam_lt242ru_rx_enable ", 33) == 0) {
		if (hostapd_ctrl_iface_set_he_1024qam_lt242ru_rx_enable(
			    hapd, buf + 33) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_1024qam_lt242ru_rx_enable") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_1024qam_lt242ru_rx_enable(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_full_bw_ul_mumimo ", 25) == 0) {
		if (hostapd_ctrl_iface_set_he_full_bw_ul_mumimo(
			    hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_full_bw_ul_mumimo") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_full_bw_ul_mumimo(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_he_bsr_support ", 19) == 0) {
		if (hostapd_ctrl_iface_set_he_bsr_support(hapd, buf + 19) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_he_bsr_support") == 0) {
		reply_len = hostapd_ctrl_iface_get_he_bsr_support(
			hapd, reply, reply_size);
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	} else if (os_strncasecmp(buf, "set_eht_ndp_4x_eht_ltf_and_320nsgi ", 35) == 0) {
		if (hostapd_ctrl_iface_set_eht_ndp_4x_eht_ltf_and_320nsgi(
			    hapd, buf + 35) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_ndp_4x_eht_ltf_and_320nsgi") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_ndp_4x_eht_ltf_and_320nsgi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_num_sd ", 15) == 0) {
		if (hostapd_ctrl_iface_set_eht_num_sd(hapd, buf + 15) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_num_sd") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_num_sd(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_4x_eht_ltf_and_800ns_gi ", 32) == 0) {
		if (hostapd_ctrl_iface_set_eht_4x_eht_ltf_and_800ns_gi(
			    hapd, buf + 32) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_4x_eht_ltf_and_800ns_gi") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_4x_eht_ltf_and_800ns_gi(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_rx_1024_and_4096_qam_ls_242_tone_ru ", 44) == 0) {
		if (hostapd_ctrl_iface_set_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
			    hapd, buf + 44) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_rx_1024_and_4096_qam_ls_242_tone_ru") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_rx_1024_and_4096_qam_ls_242_tone_ru(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_dl_ofdma_txbf ", 22) == 0) {
		if (hostapd_ctrl_iface_set_eht_dl_ofdma_txbf(hapd, buf + 22) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_dl_ofdma_txbf") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_dl_ofdma_txbf(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_trig_su_bf_fb ", 22) == 0) {
		if (hostapd_ctrl_iface_set_eht_trig_su_bf_fb(hapd, buf + 22) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_trig_su_bf_fb") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_trig_su_bf_fb(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_trig_cqi_fb ", 20) == 0) {
		if (hostapd_ctrl_iface_set_eht_trig_cqi_fb(hapd, buf + 20) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_trig_cqi_fb") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_trig_cqi_fb(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_non_trig_cqi_fb ", 24) == 0) {
		if (hostapd_ctrl_iface_set_eht_non_trig_cqi_fb(hapd, buf + 24) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_non_trig_cqi_fb") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_non_trig_cqi_fb(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_sup_mcs15_in_mru ", 25) == 0) {
		if (hostapd_ctrl_iface_set_eht_sup_mcs15_in_mru(
			    hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_sup_mcs15_in_mru") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_sup_mcs15_in_mru(
			hapd, reply, reply_size);
	} else if (os_strncasecmp(buf, "set_eht_mcs14_dup_in_6ghz ", 26) == 0) {
		if (hostapd_ctrl_iface_set_eht_mcs14_dup_in_6ghz(
			    hapd, buf + 26) < 0)
			reply_len = -1;
	} else if (os_strcasecmp(buf, "get_eht_mcs14_dup_in_6ghz") == 0) {
		reply_len = hostapd_ctrl_iface_get_eht_mcs14_dup_in_6ghz(
			hapd, reply, reply_size);
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_QCN_EXTN
	} else if (os_strncmp(buf, "SET_EDCA_MODE ", 14) == 0) {
		if (hostapd_ctrl_iface_set_muedca_mode(hapd, buf + 14) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_MU_EDCA ", 12) == 0) {
		if (hostapd_ctrl_iface_set_he_muedca(hapd, buf + 12) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_MU_EDCA ", 12) == 0) {
		reply_len = hostapd_ctrl_iface_get_he_muedca(hapd, buf + 12,
							     reply, reply_size);
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211AX
	} else if (os_strncmp(buf, "DUMP_SCS_LIST ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_dump_scs_list(hapd, buf + 14,
							     reply, reply_size);
	} else if (os_strncmp(buf, "DUMP_SCS_INFO ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_dump_scs_info(hapd, buf + 14,
							     reply, reply_size);
	} else if (os_strncmp(buf, "DUMP_SCS_QM_INFO ", 17) == 0) {
		reply_len = hostapd_ctrl_iface_dump_scs_qm_info(hapd, buf + 17,
								reply, reply_size);
	} else if (os_strncmp(buf, "SEND_UNSOLICITED_SCS_RESP ", 26) == 0) {
		if (hostapd_ctrl_iface_send_scs_resp(hapd, buf + 26))
			reply_len = -1;
	} else if (os_strncmp(buf, "SCS_CONFIGURE ", 14) == 0) {
		if (hostapd_ctrl_iface_scs_configure(hapd, buf + 14))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_MBSSID_TX", 13) == 0) {
		if (hostapd_ctrl_iface_set_mbssid_tx(hapd, buf + 13))
			reply_len = -1;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_SAE
	} else if (os_strncmp(buf, "SAE_PASSWORD_BIND ", 18) == 0) {
		if (hostapd_ctrl_iface_sae_password_bind(hapd, buf + 18))
			reply_len = -1;
#endif /* CONFIG_SAE */
#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_PROCESS_COORDINATION
	} else if (os_strncmp(buf, "PROC_COORD_TEST ", 16) == 0) {
		if (hostapd_ctrl_iface_proc_coord_test(hapd, buf + 16))
			reply_len = -1;
#endif /* CONFIG_PROCESS_COORDINATION */
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strncmp(buf, "USE_RU_PUNCTURE_DFS ", 20) == 0) {
		if (hostapd_ctrl_iface_use_ru_puncture_dfs(hapd, buf + 20))
			reply_len = -1;
	} else if (os_strncmp(buf, "DFS_DISABLE_AUTO_UNPUNC ", 24) == 0) {
		if (hostapd_ctrl_iface_dfs_disable_auto_unpunc(hapd, buf + 24))
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_PUNCTURE_SOURCES") == 0) {
		reply_len = hostapd_ctrl_iface_puncture_sources(hapd, reply, reply_size);
#ifdef CONFIG_IEEE80211BN
	} else if (os_strcmp(buf, "GET_MAPC_CONFIGS") == 0) {
		reply_len = hostapd_ctrl_iface_get_mapc_configs(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "SET_MAPC_CONFIGS ", 17) == 0) {
		reply_len = hostapd_ctrl_iface_set_mapc_configs(
			hapd, buf + 17, reply, reply_size);
	} else if (os_strcmp(buf, "GET_MAPC_VALID_COAP_LIST") == 0) {
		reply_len = hostapd_ctrl_iface_get_mapc_valid_coap_list(
			hapd, reply, reply_size);
	} else if (os_strncmp(buf, "SET_MAPC_VALID_COAP_LIST ", 25) == 0) {
		if (hostapd_ctrl_iface_set_mapc_valid_coap_list(hapd,
								buf + 25))
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_MAPC_STA ", 13) == 0) {
		reply_len = hostapd_ctrl_iface_set_mapc_sta(
			hapd, buf + 13, reply, reply_size);

#endif /* CONFIG_IEEE80211BN */
	} else {
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_ctrl_iface_receive_process_extn(hapd, buf, reply,
							     reply_size,
							     from, fromlen,
							     &reply_len))
			return reply_len;
#endif /* CONFIG_QCN_EXTN */

		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	return reply_len;
}

static int hostapd_ctrl_iface_parse_mbssid_cmn_param_cmd(char *str, char *get_str,
							 size_t get_str_len)
{
	int param_id;

	wpa_printf(MSG_DEBUG, "parse cmd: str ='%s'", str);

	if (os_strcasecmp(str, "SET beacon_int") == 0)
		param_id = CMD_BEACON_INT;
	else if (os_strcasecmp(str, "SET bss_vht_mu_beamformer") == 0)
		param_id = CMD_VHT_MU_BFMER;
	else if (os_strcasecmp(str, "SET bss_vht_su_beamformee") == 0)
		param_id = CMD_VHT_SU_BFMEE;
	else if (os_strcasecmp(str, "SET bss_vht_su_beamformer") == 0)
		param_id = CMD_VHT_SU_BFMER;
	else if (os_strcasecmp(str, "SET bss_vht_sounding_dimension") == 0)
		param_id = CMD_VHT_SOUNDING_DIM;
	else if (os_strcasecmp(str, "SET bss_vht_beamformee_sts") == 0)
		param_id = CMD_VHT_BFMEE_STS;
	else if (os_strcasecmp(str, "SET vht_mcs_nss_set") == 0)
		param_id = CMD_VHT_MCS_NSS_SET;
	else if (os_strcasecmp(str, "SET bss_he_su_beamformer") == 0)
		param_id = CMD_HE_SU_BFMER;
	else if (os_strcasecmp(str, "SET bss_he_su_beamformee") == 0)
		param_id = CMD_HE_SU_BFMEE;
	else if (os_strcasecmp(str, "SET bss_he_mu_beamformer") == 0)
		param_id = CMD_HE_MU_BEAMFORMER;
	else if (os_strcasecmp(str, "SET bss_he_ul_mumimo") == 0)
		param_id = CMD_HE_UL_MUMIMO;
	else if (os_strcasecmp(str, "SET he_basic_mcs_nss_set") == 0)
		param_id = CMD_HE_BASIC_MCS_NSS_SET;
	else if (os_strcasecmp(str, "SET he_rts_threshold") == 0)
		param_id = CMD_HE_RTS_THRESHOLD;
	else if (os_strcasecmp(str, "SET spp_amsdu") == 0)
		param_id = CMD_SPP_AMSDU;
	else if (os_strcasecmp(str, "SET he_twt_responder") == 0)
		param_id = CMD_HE_TWT_RESPONDER;
	else if (os_strcasecmp(str, "SET he_6ghz_max_ampdu_len_exp") == 0)
		param_id = CMD_HE_6GHZ_MAX_AMPDU_LEN_EXP;
	else if (os_strcasecmp(str, "SET he_er_su_disable") == 0)
		param_id = CMD_HE_ER_SU_DISABLE;
	else if (os_strcasecmp(str, "set_he_bfee_sts") == 0)
		param_id = CMD_HE_BFEE_STS;
	else if (os_strcasecmp(str, "set_he_multi_tid_aggr") == 0)
		param_id = CMD_HE_MULTI_TID_AGGR;
	else if (os_strcasecmp(str, "set_he_multi_tid_aggr_rx") == 0)
		param_id = CMD_HE_MULTI_TID_AGGR_RX;
	else if (os_strcasecmp(str, "set_he_multi_tid_aggr_tx") == 0)
		param_id = CMD_HE_MULTI_TID_AGGR_TX;
	else if (os_strcasecmp(str, "set_he_max_ampdu_len_exp") == 0)
		param_id = CMD_HE_MAX_AMPDU_LEN_EXP;
	else if (os_strcasecmp(str, "set_he_su_ppdu_1x_ltf_800ns_gi") == 0)
		param_id = CMD_HE_SU_PPDU_1X_LTF_800NS_GI;
	else if (os_strcasecmp(str, "set_he_su_mu_ppdu_4x_ltf_800ns_gi") == 0)
		param_id = CMD_HE_SU_MU_PPDU_4X_LTF_800NS_GI;
	else if (os_strcasecmp(str, "set_he_max_frag_msdu") == 0)
		param_id = CMD_HE_MAX_FRAG_MSDU;
	else if (os_strcasecmp(str, "set_he_min_frag_size") == 0)
		param_id = CMD_HE_MIN_FRAG_SIZE;
	else if (os_strcasecmp(str, "set_he_omi") == 0)
		param_id = CMD_HE_OMI;
	else if (os_strcasecmp(str, "set_he_ndp_4x_ltf_3200ns_gi") == 0)
		param_id = CMD_HE_NDP_4X_LTF_3200NS_GI;
	else if (os_strcasecmp(str, "set_he_fragmentation") == 0)
		param_id = CMD_HE_FRAGMENTATION;
	else if (os_strcasecmp(str, "set_he_amsdu_in_ampdu_suprt") == 0)
		param_id = CMD_HE_AMSDU_IN_AMPDU_SUPRT;
	else if (os_strcasecmp(str, "set_he_subfee_sts_suprt") == 0)
		param_id = CMD_HE_SUBFEE_STS_SUPRT;
	else if (os_strcasecmp(str, "set_he_max_nc_suprt") == 0)
		param_id = CMD_HE_MAX_NC_SUPRT;
	else if (os_strcasecmp(str, "set_he_er_su_disable") == 0)
		param_id = CMD_HE_ER_SU_DISABLE;
	else if (os_strcasecmp(str, "set_he_er_su_ppdu_1x_ltf_800ns_gi") == 0)
		param_id = CMD_HE_ER_SU_PPDU_1X_LTF_800NS_GI;
	else if (os_strcasecmp(str, "set_he_er_su_ppdu_4x_ltf_800ns_gi") == 0)
		param_id = CMD_HE_ER_SU_PPDU_4X_LTF_800NS_GI;
	else if (os_strcasecmp(str, "set_he_bsr_support") == 0)
		param_id = CMD_HE_BSR_SUPPORT;
	else if (os_strcasecmp(str, "SET he_6ghz_min_rate") == 0)
		param_id = CMD_HE_6GHZ_MIN_RATE;
	else if (os_strcasecmp(str, "SET bss_eht_su_beamformer") == 0)
		param_id = CMD_EHT_SU_BFMER;
	else if (os_strcasecmp(str, "SET bss_eht_su_beamformee") == 0)
		param_id = CMD_EHT_SU_BFMEE;
	else if (os_strcasecmp(str, "SET bss_eht_mu_beamformer") == 0)
		param_id = CMD_EHT_MU_BFMER;
	else if (os_strcasecmp(str, "SET bss_eht_bfme_ss_80") == 0)
		param_id = CMD_EHT_BFME_SS_80;
	else if (os_strcasecmp(str, "SET bss_eht_bfme_ss_160") == 0)
		param_id = CMD_EHT_BFME_SS_160;
	else if (os_strcasecmp(str, "SET bss_eht_bfme_ss_320") == 0)
		param_id = CMD_EHT_BFME_SS_320;
	else if (os_strcasecmp(str, "SET bss_eht_ltf") == 0)
		param_id = CMD_EHT_LTF;
	else if (os_strcasecmp(str, "SET enable_mcs15") == 0)
		param_id = CMD_ENABLE_MCS15;
	else if (os_strcasecmp(str, "set_eht_ndp_4x_eht_ltf_and_320nsgi") == 0)
		param_id = CMD_EHT_NDP_4X_EHT_LTF_AND_320NSGI;
	else if (os_strcasecmp(str, "set_eht_rx_1024_and_4096_qam_ls_242_tone_ru") == 0)
		param_id = CMD_EHT_RX_1024_AND_4096_QAM_LS_242_TONE_RU;
	else if (os_strcasecmp(str, "set_eht_dl_ofdma_txbf") == 0)
		param_id = CMD_EHT_DL_OFDMA_TXBF;
	else if (os_strcasecmp(str, "set_eht_sup_mcs15_in_mru") == 0)
		param_id = CMD_EHT_SUP_MCS15_IN_MRU;
	else if (os_strcasecmp(str, "set_eht_mcs14_dup_in_6ghz") == 0)
		param_id = CMD_EHT_MCS14_DUP_IN_6GHZ;
	else if (os_strcasecmp(str, "SET ecsa_ie_only") == 0)
		param_id = CMD_ECSA_IE_ONLY;
	else
		param_id = CMD_INVALID;

	if (param_id != CMD_INVALID) {
		if (os_strncasecmp(str, "SET ", 4) == 0)
			os_snprintf(get_str, get_str_len, "GET %s", str + 4);
		else if (os_strncasecmp(str, "set_", 4) == 0)
			os_snprintf(get_str, get_str_len, "get_%s", str + 4);
	}

	return param_id;
}

static bool hostapd_ctrl_iface_is_mbssid_cmn_param(struct hostapd_data *hapd,
						   char *cmd, char *get_str,
						   size_t get_str_len,
						   int *cmn_param_id)
{
	char *value = NULL, *str, *first, *second;
	size_t len;

	wpa_printf(MSG_DEBUG, "Common param:'%s'", cmd);

	if (!hapd->iconf->mbssid)  {
		wpa_printf(MSG_DEBUG, "MBSSID is not enabled");
		return false;
	}

	first = os_strchr(cmd, ' ');
	if (first) {
		second = os_strchr(first + 1, ' ');
		if (second)
			value = second;
		else
			value = first;
	}

	if (os_strncasecmp(cmd, "set_he_bfee_sts ", 16) == 0 ||
	    os_strncasecmp(cmd, "set_he_subfee_sts_suprt ", 24) == 0)
		value = first;

	if (!value) {
		wpa_printf(MSG_ERROR, "value is NULL for cmd:%s", cmd);
		return false;
	}

	len = value - cmd;

	str = os_malloc(len + 1);
	if (!str) {
		wpa_printf(MSG_ERROR, "Memory allocation failure");
		return false;
	}

	os_memcpy(str, cmd, len);
	str[len] = '\0';
	*cmn_param_id = hostapd_ctrl_iface_parse_mbssid_cmn_param_cmd(str, get_str, get_str_len);

	if (*cmn_param_id == CMD_INVALID) {
		wpa_printf(MSG_DEBUG, "Not a common param");
		os_free(str);
		return false;
	}

	wpa_printf(MSG_DEBUG, "Common param found:%d", *cmn_param_id);
	os_free(str);
	return true;
}

static int hostapd_ctrl_iface_get_cmn_param_val(struct hostapd_data *hapd, char *str,
						char *reply, int reply_size,
						struct sockaddr_storage *from,
						socklen_t fromlen, int *get_val)
{
	char *pos, *end;

	hostapd_ctrl_iface_receive_process(hapd, str, reply, reply_size,
					   from, fromlen);
	if (os_strncasecmp(reply, "FAIL\n", 5) == 0) {
		wpa_printf(MSG_ERROR, "Failed to get value for %s", str);
		return -1;
	}

	pos = os_strchr(reply, '=');
	get_val[0] = pos ? (int) strtol(pos + 1, NULL, 0) :
		     (int) strtol(reply, &end, 0);

	/* These params have 2 values */
	if (os_strcmp(str, "get_he_bfee_sts") == 0 ||
	    os_strcmp(str, "get_he_subfee_sts_suprt") == 0)
		get_val[1] = (int) strtol(end, NULL, 0);

	return 0;
}

static int hostapd_ctrl_iface_set_cmn_param(struct hostapd_data *hapd, char *buf,
					    char *reply, int reply_size,
					    struct sockaddr_storage *from,
					    socklen_t fromlen, char *get_str,
					    int param_id)
{
	struct hostapd_data *bss, *tx_hapd = NULL;
	char *cmd_bk = NULL, *value = NULL, *str = buf;
	char *first, *second, *end;
	int get_val[2] = {-1, -1}, val[2] = {-1, -1};
	int reply_len = 0, ret = -1;
	size_t num_bss, i, len;

	first = os_strchr(str, ' ');
	if (first) {
		second = os_strchr(first + 1, ' ');
		if (second)
			value = second;
		else
			value = first;
	}

	if (os_strncasecmp(str, "set_he_bfee_sts ", 16) == 0 ||
	    os_strncasecmp(str, "set_he_subfee_sts_suprt ", 24) == 0)
		value = first;

	if (!value) {
		wpa_printf(MSG_ERROR, "Value is NULL");
		goto end;
	}

	val[0] = (int) strtol(value, &end, 0);

	/* This param has 2 values */
	if (os_strncasecmp(str, "set_he_bfee_sts ", 16) == 0 ||
	    os_strncasecmp(str, "set_he_subfee_sts_suprt ", 24) == 0)
		val[1] = (int) strtol(end, NULL, 0);

	cmd_bk = os_malloc(os_strlen(buf) + 1);
	if (!cmd_bk) {
		wpa_printf(MSG_ERROR, "Memory allocation failed for cmd_bk");
		goto end;
	}

	len = value - str;
	os_memcpy(cmd_bk, str, len);
	cmd_bk[len] = '\0';

	num_bss = hostapd_get_mbssid_max_num_bss(hapd);

	ret = hostapd_ctrl_iface_get_cmn_param_val(hapd, get_str, reply,
						   reply_size, from, fromlen,
						   get_val);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "Fail to get param value for %d", param_id);
		goto end;
	}

	if (os_strncasecmp(str, "set_he_bfee_sts ", 16) == 0 ||
	    os_strncasecmp(str, "set_he_subfee_sts_suprt ", 24) == 0) {
		if (get_val[0] == val[0] && get_val[1] == val[1]) {
			wpa_printf(MSG_DEBUG,
				   "Values matches with previous configured get_val[0]:%d get_val[1]:%d val[0]:%d val[1]:%d",
				   get_val[0], get_val[1], val[0], val[1]);
			reply_len = 0;
			goto end;
		}
	} else {
		if (get_val[0] == val[0]) {
			wpa_printf(MSG_DEBUG,
				   "Values matches with previous configured get_val:%d value:%d",
				   get_val[0], val[0]);
			reply_len = 0;
			goto end;
		}
	}

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (!tx_hapd) {
		wpa_printf(MSG_ERROR, "Tx hapd is NULL");
		reply_len = 0;
		ret = -1;
		goto end;
	}

	os_memset(reply, 0, reply_size);
	for (i = 0; i < num_bss; i++) {
		bss = hostapd_mbssid_get_bss(hapd, i);
		if (!bss)
			continue;

		if (bss == tx_hapd) {
			bss->conf->cmn_param_id = param_id;
			bss->conf->cmn_param_val[0] = val[0];
			bss->conf->cmn_param_val[1] = val[1];
		}

		bss->conf->is_cmn_param = true;

		os_memset(cmd_bk, 0, os_strlen(buf) + 1);
		os_memcpy(cmd_bk, buf, os_strlen(buf) + 1);

		reply_len = hostapd_ctrl_iface_receive_process(bss, cmd_bk, reply, reply_size,
							       from, fromlen);
		if (os_strncasecmp(reply, "FAIL\n", 5) == 0) {
			wpa_printf(MSG_ERROR, "Failed to set param:%d val:%s for %s",
				   param_id, value, bss->conf->iface);
			goto end;
		}
	}

	for (i = 0; i < num_bss; i++) {
		bss = hostapd_mbssid_get_bss(hapd, i);
		if (!bss || bss == tx_hapd)
			continue;

		hostapd_clear_old_bss(bss);
	}

	if (hostapd_reload_bss_only(tx_hapd) < 0) {
		wpa_printf(MSG_ERROR, "Failed to reload Tx BSS %s",
			   tx_hapd->conf->iface);
		reply_len = 0;
		ret = -1;
		goto end;
	}

	wpa_printf(MSG_DEBUG, "cmn_param:%d value1:%d value2:%d set successfully %s",
		   tx_hapd->conf->cmn_param_id, tx_hapd->conf->cmn_param_val[0],
		   tx_hapd->conf->cmn_param_val[1],
		   hapd->conf->iface);

end:
	if (tx_hapd) {
		for (i = 0; i < num_bss; i++) {
			bss = hostapd_mbssid_get_bss(hapd, i);
			if (!bss || !bss->started)
				continue;

			if (bss == tx_hapd) {
				bss->conf->cmn_param_id = -1;
				bss->conf->cmn_param_val[0] = -1;
				bss->conf->cmn_param_val[1] = -1;
			}
			bss->conf->is_cmn_param = false;
		}
	}

	if (!reply_len) {
		if (ret < 0) {
			os_memcpy(reply, "FAIL\n", 5);
			reply_len = 5;
		} else {
			os_memcpy(reply, "OK\n", 3);
			reply_len = 3;
		}
	}

	if (cmd_bk)
		os_free(cmd_bk);

	return reply_len;
}

static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[4096];
	int res;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	char *reply, *pos = buf;
	int reply_len, cmn_param_id;
	int level = MSG_DEBUG;
	int reply_size;
#ifdef CONFIG_CTRL_IFACE_UDP
	unsigned char lcookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */
	char get_str[256];

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';
#ifdef CONFIG_QCN_EXTN
	reply_size = os_strstr(buf, "AFC get_afc_6g_chan_list") ?
		MAX_REPLY_EXTN_BUF : 16384;
#else
	reply_size = 4096;
#endif /* CONFIG_QCN_EXTN */

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		if (sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
				   strerror(errno));
		}
		return;
	}

#ifdef CONFIG_CTRL_IFACE_UDP
	if (os_strcmp(buf, "GET_COOKIE") == 0) {
		os_memcpy(reply, "COOKIE=", 7);
		wpa_snprintf_hex(reply + 7, 2 * CTRL_IFACE_COOKIE_LEN + 1,
				 hapd->ctrl_iface_cookie,
				 CTRL_IFACE_COOKIE_LEN);
		reply_len = 7 + 2 * CTRL_IFACE_COOKIE_LEN;
		goto done;
	}

	if (os_strncmp(buf, "COOKIE=", 7) != 0 ||
	    hexstr2bin(buf + 7, lcookie, CTRL_IFACE_COOKIE_LEN) < 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: No cookie in the request - drop request");
		os_free(reply);
		return;
	}

	if (os_memcmp(hapd->ctrl_iface_cookie, lcookie,
		      CTRL_IFACE_COOKIE_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: Invalid cookie in the request - drop request");
		os_free(reply);
		return;
	}

	pos = buf + 7 + 2 * CTRL_IFACE_COOKIE_LEN;
	while (*pos == ' ')
		pos++;
#endif /* CONFIG_CTRL_IFACE_UDP */

	if (os_strcmp(pos, "PING") == 0)
		level = MSG_EXCESSIVE;
	wpa_hexdump_ascii(level, "RX ctrl_iface", (u8 *)pos, res);

	if ((os_strncasecmp(pos, "SET", 3) == 0) &&
	    hostapd_ctrl_iface_is_mbssid_cmn_param(hapd, pos, get_str, sizeof(get_str),
						   &cmn_param_id)) {
		reply_len = hostapd_ctrl_iface_set_cmn_param(hapd, pos, reply, reply_size,
							     &from, fromlen, get_str,
							     cmn_param_id);
	} else {
		reply_len = hostapd_ctrl_iface_receive_process(hapd, pos, reply,
							       reply_size, &from,
							       fromlen);
	}

#ifdef CONFIG_CTRL_IFACE_UDP
done:
#endif /* CONFIG_CTRL_IFACE_UDP */
	if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		   fromlen) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
			   strerror(errno));
	}
	os_free(reply);
}


#ifdef CONFIG_IEEE80211BE
#ifndef CONFIG_CTRL_IFACE_UDP

static int hostapd_mld_ctrl_iface_receive_process(struct hostapd_mld *mld,
						  char *buf, char *reply,
						  size_t reply_size,
						  struct sockaddr_storage *from,
						  socklen_t fromlen)
{
	struct hostapd_data *link_hapd, *link_itr;
	int reply_len = -1, link_id = -1;
	char *cmd;
	bool found = false;

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	cmd = buf;

	/* Check whether the link ID is provided in the command */
	if (os_strncmp(cmd, "LINKID ", 7) == 0) {
		cmd += 7;
		link_id = atoi(cmd);
		if (link_id < 0 || link_id >= 15) {
			os_memcpy(reply, "INVALID LINK ID\n", 16);
			reply_len = 16;
			goto out;
		}

		cmd = os_strchr(cmd, ' ');
		if (!cmd)
			goto out;
		cmd++;
	}
	if (link_id >= 0) {
		link_hapd = mld->fbss;
		if (!link_hapd) {
			os_memcpy(reply, "NO LINKS ACTIVE\n", 16);
			reply_len = 16;
			goto out;
		}

#ifdef CONFIG_QCN_EXTN
		for_each_mld_link_include_repurposed(link_itr, link_hapd) {
#else
		for_each_mld_link(link_itr, link_hapd) {
#endif /* CONFIG_QCN_EXTN */
			if (link_itr->mld_link_id == link_id) {
				found = true;
				break;
			}
		}

		if (!found)
			goto out;

		link_hapd = link_itr;
	} else {
		link_hapd = mld->fbss;
	}

	if (os_strcmp(cmd, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strcmp(cmd, "ATTACH") == 0) {
		if (ctrl_iface_attach(&mld->ctrl_dst, from, fromlen, NULL))
			reply_len = -1;
	} else if (os_strncmp(cmd, "ATTACH ", 7) == 0) {
		if (ctrl_iface_attach(&mld->ctrl_dst, from, fromlen, cmd + 7))
			reply_len = -1;
	} else if (os_strcmp(cmd, "DETACH") == 0) {
		if (ctrl_iface_detach(&mld->ctrl_dst, from, fromlen))
			reply_len = -1;
	} else {
		if (link_id == -1)
			wpa_printf(MSG_DEBUG,
				   "Link ID not provided, using the first link BSS (if available)");

		if (!link_hapd)
			reply_len = -1;
		else
			reply_len =
				hostapd_ctrl_iface_receive_process(
					link_hapd, cmd, reply, reply_size,
					from, fromlen);
	}

out:
	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	return reply_len;
}


static void hostapd_mld_ctrl_iface_receive(int sock, void *eloop_ctx,
					   void *sock_ctx)
{
	struct hostapd_mld *mld = eloop_ctx;
	char buf[4096];
	int res;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	char *reply, *pos = buf;
	int reply_len;
	int level = MSG_DEBUG;
	size_t reply_size;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(mld ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';
#ifdef CONFIG_QCN_EXTN
	reply_size = os_strstr(buf, "AFC get_afc_6g_chan_list") ?
		MAX_REPLY_EXTN_BUF : 16384;
#else
	reply_size = 4096;
#endif /* CONFIG_QCN_EXTN */

	reply = os_malloc(reply_size);
	if (!reply) {
		if (sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "MLD CTRL: sendto failed: %s",
				   strerror(errno));
		}
		return;
	}

	if (os_strcmp(pos, "PING") == 0)
		level = MSG_EXCESSIVE;

	wpa_hexdump_ascii(level, "RX MLD ctrl_iface", (u8 *)pos, res);

	reply_len = hostapd_mld_ctrl_iface_receive_process(mld, pos,
							   reply, reply_size,
							   &from, fromlen);

	if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		   fromlen) < 0) {
		wpa_printf(MSG_DEBUG, "MLD CTRL: sendto failed: %s",
			   strerror(errno));
	}
	os_free(reply);
}


static char * hostapd_mld_ctrl_iface_path(struct hostapd_mld *mld)
{
	size_t len;
	char *buf;
	int ret;

	if (!mld->ctrl_interface)
		return NULL;

	len = os_strlen(mld->ctrl_interface) + os_strlen(mld->name) + 2;

	buf = os_malloc(len);
	if (!buf)
		return NULL;

	ret = os_snprintf(buf, len, "%s/%s", mld->ctrl_interface, mld->name);
	if (os_snprintf_error(len, ret)) {
		os_free(buf);
		return NULL;
	}

	return buf;
}

#endif /* !CONFIG_CTRL_IFACE_UDP */


int hostapd_mld_ctrl_iface_init(struct hostapd_mld *mld)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (!mld)
		return -1;

	if (mld->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "MLD %s ctrl_iface already exists!",
			   mld->name);
		return 0;
	}

	dl_list_init(&mld->ctrl_dst);

	if (!mld->ctrl_interface)
		return 0;

	if (mkdir(mld->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG,
				   "Using existing control interface directory.");
		} else {
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface]: %s",
				   strerror(errno));
			goto fail;
		}
	}

	if (os_strlen(mld->ctrl_interface) + 1 + os_strlen(mld->name) >=
	    sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;

	fname = hostapd_mld_ctrl_iface_path(mld);
	if (!fname)
		goto fail;

	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));

	wpa_printf(MSG_DEBUG, "Setting up MLD %s ctrl_iface", mld->name);

	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not allow connections - assuming it was left over from forced program termination");
			if (unlink(fname) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				wpa_printf(MSG_ERROR,
					   "hostapd-ctrl-iface: bind(PF_UNIX): %s",
					   strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG,
				   "Successfully replaced leftover ctrl_iface socket '%s'",
				   fname);
		} else {
			wpa_printf(MSG_INFO,
				   "ctrl_iface exists and seems to be in use - cannot override it");
			wpa_printf(MSG_INFO,
				   "Delete '%s' manually if it is not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}
	os_free(fname);

	mld->ctrl_sock = s;

	if (eloop_register_read_sock(s, hostapd_mld_ctrl_iface_receive, mld,
				     NULL) < 0)
		return -1;

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
#endif /* !CONFIG_CTRL_IFACE_UDP */
	return 0;
}


void hostapd_mld_ctrl_iface_deinit(struct hostapd_mld *mld)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	struct wpa_ctrl_dst *dst, *prev;

	if (mld->ctrl_sock > -1) {
		char *fname;

		eloop_unregister_read_sock(mld->ctrl_sock);
		close(mld->ctrl_sock);
		mld->ctrl_sock = -1;

		fname = hostapd_mld_ctrl_iface_path(mld);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		if (mld->ctrl_interface &&
		    rmdir(mld->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG,
					   "MLD control interface directory not empty - leaving it behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   mld->ctrl_interface,
					   strerror(errno));
			}
		}
	}

	dl_list_for_each_safe(dst, prev, &mld->ctrl_dst, struct wpa_ctrl_dst,
			      list)
		os_free(dst);
#endif /* !CONFIG_CTRL_IFACE_UDP */

	os_free(mld->ctrl_interface);
}

#endif /* CONFIG_IEEE80211BE */


#ifndef CONFIG_CTRL_IFACE_UDP
static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;
	const char *ctrl_sock_iface;

#ifdef CONFIG_IEEE80211BE
	ctrl_sock_iface = hapd->ctrl_sock_iface;
#else /* CONFIG_IEEE80211BE */
	ctrl_sock_iface = hapd->conf->iface;
#endif /* CONFIG_IEEE80211BE */

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = os_strlen(hapd->conf->ctrl_interface) +
		os_strlen(ctrl_sock_iface) + 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s",
		    hapd->conf->ctrl_interface, ctrl_sock_iface);
	buf[len - 1] = '\0';
	return buf;
}
#endif /* CONFIG_CTRL_IFACE_UDP */


static void hostapd_ctrl_iface_msg_cb(void *ctx, int level,
				      enum wpa_msg_type type,
				      const char *txt, size_t len)
{
	struct hostapd_data *hapd = ctx;
	if (hapd == NULL)
		return;
	hostapd_ctrl_iface_send(hapd, level, type, txt, len);
}


int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
#ifdef CONFIG_CTRL_IFACE_UDP
	int port = HOSTAPD_CTRL_IFACE_PORT;
	char p[32] = { 0 };
	char port_str[40], *tmp;
	char *pos;
	struct addrinfo hints = { 0 }, *res, *saveres;
	int n;

	if (hapd->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	pos = os_strstr(hapd->conf->ctrl_interface, "udp:");
	if (pos) {
		pos += 4;
		port = atoi(pos);
		if (port <= 0) {
			wpa_printf(MSG_ERROR, "Invalid ctrl_iface UDP port");
			goto fail;
		}
	}

	dl_list_init(&hapd->ctrl_dst);
	hapd->ctrl_sock = -1;
	os_get_random(hapd->ctrl_iface_cookie, CTRL_IFACE_COOKIE_LEN);

#ifdef CONFIG_CTRL_IFACE_UDP_REMOTE
	hints.ai_flags = AI_PASSIVE;
#endif /* CONFIG_CTRL_IFACE_UDP_REMOTE */

#ifdef CONFIG_CTRL_IFACE_UDP_IPV6
	hints.ai_family = AF_INET6;
#else /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_family = AF_INET;
#endif /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_socktype = SOCK_DGRAM;

try_again:
	os_snprintf(p, sizeof(p), "%d", port);
	n = getaddrinfo(NULL, p, &hints, &res);
	if (n) {
		wpa_printf(MSG_ERROR, "getaddrinfo(): %s", gai_strerror(n));
		goto fail;
	}

	saveres = res;
	hapd->ctrl_sock = socket(res->ai_family, res->ai_socktype,
				 res->ai_protocol);
	if (hapd->ctrl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_INET): %s", strerror(errno));
		goto fail;
	}

	if (bind(hapd->ctrl_sock, res->ai_addr, res->ai_addrlen) < 0) {
		port--;
		if ((HOSTAPD_CTRL_IFACE_PORT - port) <
		    HOSTAPD_CTRL_IFACE_PORT_LIMIT && !pos)
			goto try_again;
		wpa_printf(MSG_ERROR, "bind(AF_INET): %s", strerror(errno));
		goto fail;
	}

	freeaddrinfo(saveres);

	os_snprintf(port_str, sizeof(port_str), "udp:%d", port);
	tmp = os_strdup(port_str);
	if (tmp) {
		os_free(hapd->conf->ctrl_interface);
		hapd->conf->ctrl_interface = tmp;
	}
	wpa_printf(MSG_DEBUG, "ctrl_iface_init UDP port: %d", port);

	if (eloop_register_read_sock(hapd->ctrl_sock,
				     hostapd_ctrl_iface_receive, hapd, NULL) <
	    0) {
		hostapd_ctrl_iface_deinit(hapd);
		return -1;
	}

	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_PROCESS_COORDINATION
	if (hapd->iface->interfaces->pc)
		proc_coord_register_handler(hapd->iface->interfaces->pc,
					    hapd_ctrl_proc_coord_cb, hapd);
#endif /* CONFIG_PROCESS_COORDINATION */
#endif /* CONFIG_TESTING_OPTIONS */

	return 0;

fail:
	if (hapd->ctrl_sock >= 0)
		close(hapd->ctrl_sock);
	return -1;
#else /* CONFIG_CTRL_IFACE_UDP */
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;
	size_t iflen;

	if (hapd->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	dl_list_init(&hapd->ctrl_dst);

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface]: %s",
				   strerror(errno));
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    lchown(hapd->conf->ctrl_interface, -1,
		   hapd->conf->ctrl_interface_gid) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		return -1;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    lchown(hapd->conf->ctrl_interface, -1,
		   hapd->iface->interfaces->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		return -1;
	}

#ifdef ANDROID
	/*
	 * Android is using umask 0077 which would leave the control interface
	 * directory without group access. This breaks things since Wi-Fi
	 * framework assumes that this directory can be accessed by other
	 * applications in the wifi group. Fix this by adding group access even
	 * if umask value would prevent this.
	 */
	if (chmod(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "CTRL: Could not chmod directory: %s",
			   strerror(errno));
		/* Try to continue anyway */
	}
#endif /* ANDROID */

#ifdef CONFIG_IEEE80211BE
	iflen = os_strlen(hapd->ctrl_sock_iface);
#else /* CONFIG_IEEE80211BE */
	iflen = os_strlen(hapd->conf->iface);
#endif /* CONFIG_IEEE80211BE */
	if (os_strlen(hapd->conf->ctrl_interface) + 1 +
	    iflen >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				wpa_printf(MSG_ERROR,
					   "hostapd-ctrl-iface: bind(PF_UNIX): %s",
					   strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    lchown(fname, -1, hapd->conf->ctrl_interface_gid) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    lchown(fname, -1, hapd->iface->interfaces->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}
	os_free(fname);

	hapd->ctrl_sock = s;
	if (eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				     NULL) < 0) {
		hostapd_ctrl_iface_deinit(hapd);
		return -1;
	}
	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
#endif /* CONFIG_CTRL_IFACE_UDP */
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (hapd->ctrl_sock > -1) {
#ifndef CONFIG_CTRL_IFACE_UDP
		char *fname;
#endif /* !CONFIG_CTRL_IFACE_UDP */

		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		os_free(fname);

		if (hapd->conf->ctrl_interface &&
		    rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   hapd->conf->ctrl_interface,
					   strerror(errno));
			}
		}
#endif /* !CONFIG_CTRL_IFACE_UDP */
	}

	dl_list_for_each_safe(dst, prev, &hapd->ctrl_dst, struct wpa_ctrl_dst,
			      list)
		os_free(dst);

#ifdef CONFIG_TESTING_OPTIONS
	l2_packet_deinit(hapd->l2_test);
	hapd->l2_test = NULL;
#ifdef CONFIG_PROCESS_COORDINATION
	if (hapd->iface->interfaces->pc) {
		proc_coord_unregister_handler(hapd->iface->interfaces->pc,
					      hapd_ctrl_proc_coord_cb, hapd);
		proc_coord_cancel_wait(hapd->iface->interfaces->pc,
				       hapd_ctrl_proc_coord_test_cb, hapd);
	}
#endif /* CONFIG_PROCESS_COORDINATION */
#endif /* CONFIG_TESTING_OPTIONS */
}


static int hostapd_ctrl_iface_add(struct hapd_interfaces *interfaces,
				  char *buf)
{
	if (hostapd_add_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Adding interface %s failed", buf);
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_remove(struct hapd_interfaces *interfaces,
				     char *buf)
{
	if (hostapd_remove_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Removing interface %s failed", buf);
		return -1;
	}
	return 0;
}


static int hostapd_global_ctrl_iface_attach(struct hapd_interfaces *interfaces,
					    struct sockaddr_storage *from,
					    socklen_t fromlen, char *input)
{
	return ctrl_iface_attach(&interfaces->global_ctrl_dst, from, fromlen,
				 input);
}


static int hostapd_global_ctrl_iface_detach(struct hapd_interfaces *interfaces,
					    struct sockaddr_storage *from,
					    socklen_t fromlen)
{
	return ctrl_iface_detach(&interfaces->global_ctrl_dst, from, fromlen);
}


static void hostapd_ctrl_iface_flush(struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_WPS_TESTING
	wps_version_number = 0x20;
	wps_testing_stub_cred = 0;
	wps_corrupt_pkhash = 0;
#endif /* CONFIG_WPS_TESTING */

#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_DPP
	dpp_test = DPP_TEST_DISABLED;
#ifdef CONFIG_DPP3
	dpp_version_override = 3;
#elif defined(CONFIG_DPP2)
	dpp_version_override = 2;
#else /* CONFIG_DPP2 */
	dpp_version_override = 1;
#endif /* CONFIG_DPP2 */
#endif /* CONFIG_DPP */
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_DPP
	dpp_global_clear(interfaces->dpp);
#ifdef CONFIG_DPP3
	interfaces->dpp_pb_bi = NULL;
	{
		int i;

		for (i = 0; i < DPP_PB_INFO_COUNT; i++) {
			struct dpp_pb_info *info;

			info = &interfaces->dpp_pb[i];
			info->rx_time.sec = 0;
			info->rx_time.usec = 0;
		}
	}
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */
}


#ifdef CONFIG_FST

static int
hostapd_global_ctrl_iface_fst_attach(struct hapd_interfaces *interfaces,
				     const char *cmd)
{
	char ifname[IFNAMSIZ + 1];
	struct fst_iface_cfg cfg;
	struct hostapd_data *hapd;
	struct fst_wpa_obj iface_obj;

	if (!fst_parse_attach_command(cmd, ifname, sizeof(ifname), &cfg)) {
		hapd = hostapd_get_iface(interfaces, ifname);
		if (hapd) {
			if (hapd->iface->fst) {
				wpa_printf(MSG_INFO, "FST: Already attached");
				return -1;
			}
			fst_hostapd_fill_iface_obj(hapd, &iface_obj);
			hapd->iface->fst = fst_attach(ifname, hapd->own_addr,
						      &iface_obj, &cfg);
			if (hapd->iface->fst)
				return 0;
		}
	}

	return -EINVAL;
}


static int
hostapd_global_ctrl_iface_fst_detach(struct hapd_interfaces *interfaces,
				     const char *cmd)
{
	char ifname[IFNAMSIZ + 1];
	struct hostapd_data * hapd;

	if (!fst_parse_detach_command(cmd, ifname, sizeof(ifname))) {
		hapd = hostapd_get_iface(interfaces, ifname);
		if (hapd) {
			if (!fst_iface_detach(ifname)) {
				hapd->iface->fst = NULL;
				hapd->iface->fst_ies = NULL;
				return 0;
			}
		}
	}

	return -EINVAL;
}

#endif /* CONFIG_FST */


static int hostapd_ctrl_iface_dup_param(struct hostapd_data *src_hapd,
					struct hostapd_data *dst_hapd,
					const char *param)
{
	int res;
	char *value;

	value = os_zalloc(HOSTAPD_CLI_DUP_VALUE_MAX_LEN);
	if (!value) {
		wpa_printf(MSG_ERROR,
			   "DUP: cannot allocate buffer to stringify %s",
			   param);
		goto error_return;
	}

	if (os_strcmp(param, "wpa") == 0) {
		os_snprintf(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN, "%d",
			    src_hapd->conf->wpa);
	} else if (os_strcmp(param, "wpa_key_mgmt") == 0 &&
		   src_hapd->conf->wpa_key_mgmt) {
		res = hostapd_ctrl_iface_get_key_mgmt(
			src_hapd, value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN);
		if (os_snprintf_error(HOSTAPD_CLI_DUP_VALUE_MAX_LEN, res))
			goto error_stringify;
	} else if (os_strcmp(param, "wpa_pairwise") == 0 &&
		   src_hapd->conf->wpa_pairwise) {
		res = wpa_write_ciphers(value,
					value + HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
					src_hapd->conf->wpa_pairwise, " ");
		if (res < 0)
			goto error_stringify;
	} else if (os_strcmp(param, "rsn_pairwise") == 0 &&
		   src_hapd->conf->rsn_pairwise) {
		res = wpa_write_ciphers(value,
					value + HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
					src_hapd->conf->rsn_pairwise, " ");
		if (res < 0)
			goto error_stringify;
	} else if (os_strcmp(param, "wpa_passphrase") == 0 &&
		   src_hapd->conf->ssid.wpa_passphrase) {
		os_snprintf(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN, "%s",
			    src_hapd->conf->ssid.wpa_passphrase);
	} else if (os_strcmp(param, "wpa_psk") == 0 &&
		   src_hapd->conf->ssid.wpa_psk_set) {
		wpa_snprintf_hex(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
			src_hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
	} else {
		wpa_printf(MSG_WARNING, "DUP: %s cannot be duplicated", param);
		goto error_return;
	}

	res = hostapd_set_iface(dst_hapd->iconf, dst_hapd->conf, param, value);
	os_free(value);
	return res;

error_stringify:
	wpa_printf(MSG_ERROR, "DUP: cannot stringify %s", param);
error_return:
	os_free(value);
	return -1;
}


static int
hostapd_global_ctrl_iface_interfaces(struct hapd_interfaces *interfaces,
				     const char *input,
				     char *reply, int reply_size)
{
	size_t i, j;
	int res;
	char *pos, *end;
	struct hostapd_iface *iface;
	int show_ctrl = 0;

	if (input)
		show_ctrl = !!os_strstr(input, "ctrl");

	pos = reply;
	end = reply + reply_size;

	for (i = 0; i < interfaces->count; i++) {
		iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_bss_config *conf;

			conf = iface->conf->bss[j];
			if (show_ctrl)
				res = os_snprintf(pos, end - pos,
						  "%s ctrl_iface=%s\n",
						  conf->iface,
						  conf->ctrl_interface ?
						  conf->ctrl_interface : "N/A");
			else
				res = os_snprintf(pos, end - pos, "%s\n",
						  conf->iface);
			if (os_snprintf_error(end - pos, res)) {
				*pos = '\0';
				return pos - reply;
			}
			pos += res;
		}
	}

	return pos - reply;
}


static int
hostapd_global_ctrl_iface_dup_network(struct hapd_interfaces *interfaces,
				      char *cmd)
{
	char *p_start = cmd, *p_end;
	struct hostapd_data *src_hapd, *dst_hapd;

	/* cmd: "<src ifname> <dst ifname> <variable name> */

	p_end = os_strchr(p_start, ' ');
	if (!p_end) {
		wpa_printf(MSG_ERROR, "DUP: no src ifname found in cmd: '%s'",
			   cmd);
		return -1;
	}

	*p_end = '\0';
	src_hapd = hostapd_interfaces_get_hapd(interfaces, p_start);
	if (!src_hapd) {
		wpa_printf(MSG_ERROR, "DUP: no src ifname found: '%s'",
			   p_start);
		return -1;
	}

	p_start = p_end + 1;
	p_end = os_strchr(p_start, ' ');
	if (!p_end) {
		wpa_printf(MSG_ERROR, "DUP: no dst ifname found in cmd: '%s'",
			   cmd);
		return -1;
	}

	*p_end = '\0';
	dst_hapd = hostapd_interfaces_get_hapd(interfaces, p_start);
	if (!dst_hapd) {
		wpa_printf(MSG_ERROR, "DUP: no dst ifname found: '%s'",
			   p_start);
		return -1;
	}

	p_start = p_end + 1;
	return hostapd_ctrl_iface_dup_param(src_hapd, dst_hapd, p_start);
}


static int hostapd_global_ctrl_iface_ifname(struct hapd_interfaces *interfaces,
					    const char *ifname,
					    char *buf, char *reply,
					    int reply_size,
					    struct sockaddr_storage *from,
					    socklen_t fromlen)
{
	struct hostapd_data *hapd;

	hapd = hostapd_interfaces_get_hapd(interfaces, ifname);
	if (hapd == NULL) {
		int res;

		res = os_snprintf(reply, reply_size, "FAIL-NO-IFNAME-MATCH\n");
		if (os_snprintf_error(reply_size, res))
			return -1;
		return res;
	}

	return hostapd_ctrl_iface_receive_process(hapd, buf, reply,reply_size,
						  from, fromlen);
}


static void hostapd_global_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	struct hapd_interfaces *interfaces = eloop_ctx;
	char buffer[256], *buf = buffer;
	int res;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	int reply_len;
	int reply_size;
#ifdef CONFIG_CTRL_IFACE_UDP
	unsigned char lcookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */

	res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';
	wpa_printf(MSG_DEBUG, "Global ctrl_iface command: %s", buf);
#ifdef CONFIG_QCN_EXTN
	reply_size = os_strstr(buf, "AFC get_afc_6g_chan_list") ?
		MAX_REPLY_EXTN_BUF : 16384;
#else
	reply_size = 4096;
#endif /* CONFIG_QCN_EXTN */

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		if (sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
				   strerror(errno));
		}
		return;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

#ifdef CONFIG_CTRL_IFACE_UDP
	if (os_strcmp(buf, "GET_COOKIE") == 0) {
		os_memcpy(reply, "COOKIE=", 7);
		wpa_snprintf_hex(reply + 7, 2 * CTRL_IFACE_COOKIE_LEN + 1,
				 interfaces->ctrl_iface_cookie,
				 CTRL_IFACE_COOKIE_LEN);
		reply_len = 7 + 2 * CTRL_IFACE_COOKIE_LEN;
		goto send_reply;
	}

	if (os_strncmp(buf, "COOKIE=", 7) != 0 ||
	    hexstr2bin(buf + 7, lcookie, CTRL_IFACE_COOKIE_LEN) < 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: No cookie in the request - drop request");
		os_free(reply);
		return;
	}

	if (os_memcmp(interfaces->ctrl_iface_cookie, lcookie,
		      CTRL_IFACE_COOKIE_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: Invalid cookie in the request - drop request");
		os_free(reply);
		return;
	}

	buf += 7 + 2 * CTRL_IFACE_COOKIE_LEN;
	while (*buf == ' ')
		buf++;
#endif /* CONFIG_CTRL_IFACE_UDP */

	if (os_strncmp(buf, "IFNAME=", 7) == 0) {
		char *pos = os_strchr(buf + 7, ' ');

		if (pos) {
			*pos++ = '\0';
			reply_len = hostapd_global_ctrl_iface_ifname(
				interfaces, buf + 7, pos, reply, reply_size,
				&from, fromlen);
			goto send_reply;
		}
	}

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "FLUSH") == 0) {
		hostapd_ctrl_iface_flush(interfaces);
	} else if (os_strncmp(buf, "ADD ", 4) == 0) {
		if (hostapd_ctrl_iface_add(interfaces, buf + 4) < 0)
			reply_len = -1;
		else
			hostapd_ucode_update_interfaces();
	} else if (os_strncmp(buf, "REMOVE ", 7) == 0) {
		if (hostapd_ctrl_iface_remove(interfaces, buf + 7) < 0)
			reply_len = -1;
		else
			hostapd_ucode_update_interfaces();
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_global_ctrl_iface_attach(interfaces, &from,
						     fromlen, NULL))
			reply_len = -1;
	} else if (os_strncmp(buf, "ATTACH ", 7) == 0) {
		if (hostapd_global_ctrl_iface_attach(interfaces, &from,
						     fromlen, buf + 7))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_global_ctrl_iface_detach(interfaces, &from,
			fromlen))
			reply_len = -1;
#ifdef CONFIG_MODULE_TESTS
	} else if (os_strcmp(buf, "MODULE_TESTS") == 0) {
		if (hapd_module_tests() < 0)
			reply_len = -1;
#endif /* CONFIG_MODULE_TESTS */
#ifdef CONFIG_FST
	} else if (os_strncmp(buf, "FST-ATTACH ", 11) == 0) {
		if (!hostapd_global_ctrl_iface_fst_attach(interfaces, buf + 11))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "FST-DETACH ", 11) == 0) {
		if (!hostapd_global_ctrl_iface_fst_detach(interfaces, buf + 11))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "FST-MANAGER ", 12) == 0) {
		reply_len = fst_ctrl_iface_receive(buf + 12, reply, reply_size);
#endif /* CONFIG_FST */
	} else if (os_strncmp(buf, "DUP_NETWORK ", 12) == 0) {
		if (!hostapd_global_ctrl_iface_dup_network(interfaces,
							   buf + 12))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "INTERFACES", 10) == 0) {
		reply_len = hostapd_global_ctrl_iface_interfaces(
			interfaces, buf + 10, reply, reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else {
		wpa_printf(MSG_DEBUG, "Unrecognized global ctrl_iface command "
			   "ignored");
		reply_len = -1;
	}

send_reply:
	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		   fromlen) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
			   strerror(errno));
	}
	os_free(reply);
}


#ifndef CONFIG_CTRL_IFACE_UDP
static char * hostapd_global_ctrl_iface_path(struct hapd_interfaces *interface)
{
	char *buf;
	size_t len;

	if (interface->global_iface_path == NULL)
		return NULL;

	len = os_strlen(interface->global_iface_path) +
		os_strlen(interface->global_iface_name) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s", interface->global_iface_path,
		    interface->global_iface_name);
	buf[len - 1] = '\0';
	return buf;
}
#endif /* CONFIG_CTRL_IFACE_UDP */


int hostapd_global_ctrl_iface_init(struct hapd_interfaces *interface)
{
#ifdef CONFIG_CTRL_IFACE_UDP
	int port = HOSTAPD_GLOBAL_CTRL_IFACE_PORT;
	char p[32] = { 0 };
	char *pos;
	struct addrinfo hints = { 0 }, *res, *saveres;
	int n;

	if (interface->global_ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	if (interface->global_iface_path == NULL)
		return 0;

	pos = os_strstr(interface->global_iface_path, "udp:");
	if (pos) {
		pos += 4;
		port = atoi(pos);
		if (port <= 0) {
			wpa_printf(MSG_ERROR, "Invalid global ctrl UDP port");
			goto fail;
		}
	}

	os_get_random(interface->ctrl_iface_cookie, CTRL_IFACE_COOKIE_LEN);

#ifdef CONFIG_CTRL_IFACE_UDP_REMOTE
	hints.ai_flags = AI_PASSIVE;
#endif /* CONFIG_CTRL_IFACE_UDP_REMOTE */

#ifdef CONFIG_CTRL_IFACE_UDP_IPV6
	hints.ai_family = AF_INET6;
#else /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_family = AF_INET;
#endif /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_socktype = SOCK_DGRAM;

try_again:
	os_snprintf(p, sizeof(p), "%d", port);
	n = getaddrinfo(NULL, p, &hints, &res);
	if (n) {
		wpa_printf(MSG_ERROR, "getaddrinfo(): %s", gai_strerror(n));
		goto fail;
	}

	saveres = res;
	interface->global_ctrl_sock = socket(res->ai_family, res->ai_socktype,
					     res->ai_protocol);
	if (interface->global_ctrl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_INET): %s", strerror(errno));
		goto fail;
	}

	if (bind(interface->global_ctrl_sock, res->ai_addr, res->ai_addrlen) <
	    0) {
		port++;
		if ((port - HOSTAPD_GLOBAL_CTRL_IFACE_PORT) <
		    HOSTAPD_GLOBAL_CTRL_IFACE_PORT_LIMIT && !pos)
			goto try_again;
		wpa_printf(MSG_ERROR, "bind(AF_INET): %s", strerror(errno));
		goto fail;
	}

	freeaddrinfo(saveres);

	wpa_printf(MSG_DEBUG, "global ctrl_iface_init UDP port: %d", port);

	if (eloop_register_read_sock(interface->global_ctrl_sock,
				     hostapd_global_ctrl_iface_receive,
				     interface, NULL) < 0) {
		hostapd_global_ctrl_iface_deinit(interface);
		return -1;
	}

	interface->ctrl_iface_recv = hostapd_ctrl_iface_receive_process;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (interface->global_ctrl_sock >= 0)
		close(interface->global_ctrl_sock);
	return -1;
#else /* CONFIG_CTRL_IFACE_UDP */
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (interface->global_iface_path == NULL) {
		wpa_printf(MSG_DEBUG, "ctrl_iface not configured!");
		return 0;
	}

	if (mkdir(interface->global_iface_path, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface]: %s",
				   strerror(errno));
			goto fail;
		}
	} else if (interface->ctrl_iface_group &&
		   lchown(interface->global_iface_path, -1,
			  interface->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		goto fail;
	}

	if (os_strlen(interface->global_iface_path) + 1 +
	    os_strlen(interface->global_iface_name) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_global_ctrl_iface_path(interface);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				wpa_printf(MSG_ERROR, "bind(PF_UNIX): %s",
					   strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (interface->ctrl_iface_group &&
	    lchown(fname, -1, interface->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}
	os_free(fname);

	interface->global_ctrl_sock = s;
	interface->ctrl_iface_recv = hostapd_ctrl_iface_receive_process;
	eloop_register_read_sock(s, hostapd_global_ctrl_iface_receive,
				 interface, NULL);

	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
#endif /* CONFIG_CTRL_IFACE_UDP */
}


void hostapd_global_ctrl_iface_deinit(struct hapd_interfaces *interfaces)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	char *fname = NULL;
#endif /* CONFIG_CTRL_IFACE_UDP */
	struct wpa_ctrl_dst *dst, *prev;

	if (interfaces->global_ctrl_sock > -1) {
		eloop_unregister_read_sock(interfaces->global_ctrl_sock);
		close(interfaces->global_ctrl_sock);
		interfaces->global_ctrl_sock = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
		fname = hostapd_global_ctrl_iface_path(interfaces);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		if (interfaces->global_iface_path &&
		    rmdir(interfaces->global_iface_path) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   interfaces->global_iface_path,
					   strerror(errno));
			}
		}
#endif /* CONFIG_CTRL_IFACE_UDP */
	}

	os_free(interfaces->global_iface_path);
	interfaces->global_iface_path = NULL;

	dl_list_for_each_safe(dst, prev, &interfaces->global_ctrl_dst,
			      struct wpa_ctrl_dst, list)
		os_free(dst);
}


static int hostapd_ctrl_check_event_enabled(struct wpa_ctrl_dst *dst,
					    const char *buf)
{
	/* Enable Probe Request events based on explicit request.
	 * Other events are enabled by default.
	 */
	if (str_starts(buf, RX_PROBE_REQUEST))
		return !!(dst->events & WPA_EVENT_RX_PROBE_REQUEST);
	return 1;
}


static void hostapd_ctrl_iface_send_internal(int sock, struct dl_list *ctrl_dst,
					     const char *ifname,
					     int mld_link_id, int level,
					     const char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx, res;
	struct iovec io[8];
	char levelstr[10];
	char linkid_str[20];

	if (sock < 0 || dl_list_empty(ctrl_dst))
		return;

	res = os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	if (os_snprintf_error(sizeof(levelstr), res))
		return;
	idx = 0;
	if (ifname) {
		io[idx].iov_base = "IFNAME=";
		io[idx].iov_len = 7;
		idx++;
		io[idx].iov_base = (char *) ifname;
		io[idx].iov_len = os_strlen(ifname);
		idx++;
		io[idx].iov_base = " ";
		io[idx].iov_len = 1;
		idx++;
	}
	io[idx].iov_base = levelstr;
	io[idx].iov_len = os_strlen(levelstr);
	idx++;
	io[idx].iov_base = (char *) buf;
	io[idx].iov_len = len;
	idx++;
	if (mld_link_id >= 0) {
		res = os_snprintf(linkid_str, sizeof(linkid_str), " LINKID=%d",
				  mld_link_id);
		if (os_snprintf_error(sizeof(linkid_str), res))
			return;

		io[idx].iov_base = linkid_str;
		io[idx].iov_len = os_strlen(linkid_str);
		idx++;
	}
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = idx;

	idx = 0;
	dl_list_for_each_safe(dst, next, ctrl_dst, struct wpa_ctrl_dst, list) {
		if ((level >= dst->debug_level) &&
		     hostapd_ctrl_check_event_enabled(dst, buf)) {
			sockaddr_print(MSG_DEBUG, "CTRL_IFACE monitor send",
				       &dst->addr, dst->addrlen);
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(sock, &msg, MSG_DONTWAIT) < 0) {
				int _errno = errno;
				wpa_printf(MSG_INFO, "CTRL_IFACE monitor[%d]: "
					   "%d - %s",
					   idx, errno, strerror(errno));
				dst->errors++;
				if (dst->errors > 10 || _errno == ENOENT) {
					ctrl_iface_detach(ctrl_dst,
							  &dst->addr,
							  dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
	}
}


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    enum wpa_msg_type type,
				    const char *buf, size_t len)
{
	int mld_link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		mld_link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if (type != WPA_MSG_NO_GLOBAL) {
		hostapd_ctrl_iface_send_internal(
			hapd->iface->interfaces->global_ctrl_sock,
			&hapd->iface->interfaces->global_ctrl_dst,
			type != WPA_MSG_PER_INTERFACE ?
			NULL : hapd->conf->iface,
			type == WPA_MSG_PER_INTERFACE ?
			mld_link_id : -1,
			level, buf, len);
	}

	if (type != WPA_MSG_ONLY_GLOBAL) {
		hostapd_ctrl_iface_send_internal(
			hapd->ctrl_sock, &hapd->ctrl_dst,
			NULL, -1, level, buf, len);
	}
}
#endif /* CONFIG_NATIVE_WINDOWS */
