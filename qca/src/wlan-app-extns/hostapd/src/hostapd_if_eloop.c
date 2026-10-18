/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * ASYNC API scaffolding, for direct inclusion from hostapd_if.c
 */
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "hostapd_external_interface.h"

#define HOSTAPD_IF_ASYNC_SOCKET_PATH "/var/run/hostapd/hostapd_if_eloop"
static char hostapd_if_layer_socket_path[100];

/*
 * Server socket (bound, registered with eloop)
 */
static int hostapd_if_eloop_server_sock = -1;

/*
 * Client socket (used by eloop serializers to send)
 */
static int hostapd_if_eloop_sock = -1;

/*
 * These are defined in hostapd_if.c; declare here for standalone
 * compilation analysis
 */
#define HOSTAPD_IF_ASYNC_MAXMSG 2048

enum hostapd_if_eloop_opcode {
	HOSTAPD_IF_ASYNC_ASSOC_RESPONSE = 0,
	HOSTAPD_IF_ASYNC_AUTH_RESPONSE = 1,
	HOSTAPD_IF_ASYNC_SEND_DEAUTH = 2,
	HOSTAPD_IF_ASYNC_SEND_DISASSOC = 3,
	HOSTAPD_IF_ASYNC_SET_BEACON_PROBE_VENDOR_IES = 4,
	HOSTAPD_IF_ASYNC_SET_PMK = 5,
	HOSTAPD_IF_ASYNC_SET_PTK = 6,
	HOSTAPD_IF_ASYNC_SET_GTK = 7,
	HOSTAPD_IF_ASYNC_START_SA_QUERY = 8,
	HOSTAPD_IF_ASYNC_TRIGGER_EAPOL_M3 = 9,
	HOSTAPD_IF_ASYNC_EAPOL_TX = 10,
	HOSTAPD_IF_ASYNC_SEND_FRAME = 11,
	HOSTAPD_IF_ASYNC_EAPOL_KEY_TX = 12,
	HOSTAPD_IF_ASYNC_SET_AUTHORIZED = 13,
	HOSTAPD_IF_ASYNC_REMOTE_AUTH_RESPONSE = 14,
	HOSTAPD_IF_ASYNC_OP_MAX
};

struct hostapd_if_assoc_response_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	struct hostapd_if_frame_ctx *ctx;
};

struct hostapd_if_auth_response_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	struct hostapd_if_frame_ctx *ctx;
};

struct hostapd_if_send_deauth_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	uint16_t reason_code;
	int link_id;
	uint8_t *added_data;
	uint8_t added_data_len;
};

struct hostapd_if_send_disassoc_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	uint16_t reason_code;
	int link_id;
	uint8_t *added_data;
	uint8_t added_data_len;
};

struct hostapd_if_set_beacon_probe_vendor_ies_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t *buf;
	size_t buf_len;
	int link_id;
};

struct hostapd_if_set_pmk_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	uint8_t *pmk;
	size_t pmk_len;
	uint8_t *pmkid;
	int session_timeout;
	struct dot1x_ctx *ctx;
	bool dot1x_done;
};

struct hostapd_if_set_ptk_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	uint8_t *kck;
	size_t kck_len;
	uint8_t *kek;
	size_t kek_len;
	uint8_t *tk;
	size_t tk_len;
	bool authorized;
};

struct hostapd_if_set_gtk_msg {
	char ifname[IFNAMSIZ + 1];
	int link_id;
	int gtk_idx;
	uint8_t *gtk;
	size_t gtk_len;
};

struct hostapd_if_start_sa_query_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	int link_id;
};

struct hostapd_if_set_authorized_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	int authorized;
};

struct hostapd_if_trigger_eapol_m3_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
};

struct hostapd_if_eapol_tx_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	int link_id;
	uint8_t type;
	uint16_t data_len;
	uint8_t *data;
	bool with_header;
};

struct hostapd_if_send_frame_msg {
	char ifname[IFNAMSIZ + 1];
	int link_id;
	uint8_t *frame;
	uint16_t frame_len;
};

struct hostapd_if_eapol_key_tx_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	uint8_t link_id;
	uint8_t *frame;
	uint16_t frame_len;
};

struct hostapd_if_remote_auth_response_msg {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	struct hostapd_if_frame_ctx *ctx;
};

union hostapd_if_eloop_msg_union {
	struct hostapd_if_assoc_response_msg assoc_response;
	struct hostapd_if_auth_response_msg auth_response;
	struct hostapd_if_send_deauth_msg send_deauth;
	struct hostapd_if_send_disassoc_msg send_disassoc;
	struct hostapd_if_set_beacon_probe_vendor_ies_msg set_beacon_probe_vendor_ies;
	struct hostapd_if_set_pmk_msg set_pmk;
	struct hostapd_if_set_ptk_msg set_ptk;
	struct hostapd_if_set_gtk_msg set_gtk;
	struct hostapd_if_start_sa_query_msg start_sa_query;
	struct hostapd_if_set_authorized_msg set_authorized;
	struct hostapd_if_trigger_eapol_m3_msg trigger_eapol_m3;
	struct hostapd_if_eapol_key_tx_msg eapol_key_tx;
	struct hostapd_if_eapol_tx_msg eapol_tx;
	struct hostapd_if_send_frame_msg send_frame;
	struct hostapd_if_remote_auth_response_msg remote_auth_response;
};

struct hostapd_if_eloop_payload {
	enum hostapd_if_eloop_opcode opcode;
	union hostapd_if_eloop_msg_union msg;
};

int hostapd_if_assoc_response_validate_inputs(char *ifname, uint8_t *sta_mac,
					      struct hostapd_if_frame_ctx *ctx);
int hostapd_if_auth_response_validate_inputs(char *ifname, uint8_t *sta_mac,
					     struct hostapd_if_frame_ctx *ctx);
int hostapd_if_send_deauth_validate_inputs(char *ifname, uint8_t *sta_mac,
					   uint16_t reason_code,
					   uint8_t *added_data,
					   uint8_t added_data_len);
int hostapd_if_send_disassoc_validate_inputs(char *ifname, uint8_t *sta_mac,
					     uint16_t reason_code,
					     uint8_t *added_data,
					     uint8_t added_data_len);
int hostapd_if_set_beacon_probe_vendor_ies_validate_inputs(char *ifname,
							   uint8_t *buf,
							   size_t buf_len,
							   int link_id);
int hostapd_if_set_pmk_validate_inputs(char *ifname, uint8_t *sta_mac,
				       uint8_t *pmk, size_t pmk_len,
				       uint8_t *pmkid);
int hostapd_if_set_ptk_validate_inputs(char *ifname, uint8_t *sta_mac,
				       uint8_t *kck, size_t kck_len,
				       uint8_t *kek, size_t kek_len,
				       uint8_t *tk, size_t tk_len,
				       bool authorized);
int hostapd_if_set_gtk_validate_inputs(char *ifname, int link_id,
				       int gtk_idx, uint8_t *gtk,
				       size_t gtk_len);
int hostapd_if_start_sa_query_validate_inputs(char *ifname,
					      uint8_t *sta_mac, int link_id);
int hostapd_if_set_authorized_validate_inputs(char *ifname, uint8_t *sta_mac,
					      int authorized);
int hostapd_if_trigger_eapol_m3_validate_inputs(char *ifname,
						uint8_t *sta_mac);
int hostapd_if_eapol_key_tx_validate_inputs(char *ifname, uint8_t *sta_mac,
                                           uint8_t link_id,
                                           uint8_t *frame,
                                           uint16_t frame_len);
int hostapd_if_eapol_tx_validate_inputs(char *ifname, uint8_t *sta_mac,
					int link_id, uint8_t *data,
					uint16_t data_len);
int hostapd_if_send_frame_validate_inputs(char *ifname, int link_id,
					  uint8_t *frame, uint16_t frame_len);
int hostapd_if_remote_auth_response_validate_inputs(char *ifname,
						    uint8_t *sta_mac,
						    struct hostapd_if_frame_ctx *ctx);
void hostapd_if_assoc_response_dump_params(char *ifname, uint8_t *sta_mac,
					   struct hostapd_if_frame_ctx *ctx);
void hostapd_if_auth_response_dump_params(char *ifname, uint8_t *sta_mac,
					  struct hostapd_if_frame_ctx *ctx);
void hostapd_if_send_deauth_dump_params(char *ifname, uint8_t *sta_mac,
					uint16_t reason_code,
					int link_id,
					uint8_t *added_data,
					uint8_t added_data_len);
void hostapd_if_send_disassoc_dump_params(char *ifname, uint8_t *sta_mac,
					  uint16_t reason_code,
					  uint8_t *added_data,
					  uint8_t added_data_len);
void hostapd_if_set_beacon_probe_vendor_ies_dump_params(
	char *ifname, uint8_t *buf, size_t buf_len, int link_id);
void hostapd_if_set_pmk_dump_params(char *ifname, uint8_t *sta_mac,
				    uint8_t *pmk, size_t pmk_len,
				    uint8_t *pmkid);
void hostapd_if_set_ptk_dump_params(char *ifname, uint8_t *sta_mac,
				    uint8_t *kck, size_t kck_len,
				    uint8_t *kek, size_t kek_len,
				    uint8_t *tk, size_t tk_len,
				    bool authorized);
void hostapd_if_set_gtk_dump_params(char *ifname, int link_id,
				    int gtk_idx, uint8_t *gtk, size_t gtk_len);
void hostapd_if_start_sa_query_dump_params(char *ifname, uint8_t *sta_mac, int link_id);
void hostapd_if_set_authorized_dump_params(char *ifname, uint8_t *sta_mac,
					   int authorized);
void hostapd_if_trigger_eapol_m3_dump_params(char *ifname, uint8_t *sta_mac);
void hostapd_if_send_frame_dump_params(char *ifname, int link_id,
				       uint8_t *frame, uint16_t frame_len);
void hostapd_if_eapol_key_tx_dump_params(char *ifname, uint8_t *sta_mac,
                                        uint8_t link_id,
                                        uint8_t *frame,
                                        uint16_t frame_len);
void hostapd_if_remote_auth_response_dump_params(char *ifname, uint8_t *sta_mac,
						 struct hostapd_if_frame_ctx *ctx);
void __hostapd_if_trigger_eapol_m3(char *ifname, uint8_t *sta_mac);
void __hostapd_if_assoc_response(char *ifname, uint8_t *sta_mac,
				 struct hostapd_if_frame_ctx *ctx);
void __hostapd_if_auth_response(char *ifname, uint8_t *sta_mac,
				struct hostapd_if_frame_ctx *ctx);
void __hostapd_if_send_deauth(char *ifname, uint8_t *sta_mac,
			      uint16_t reason_code, int link_id,
			      uint8_t *added_data,
			      uint8_t added_data_len);
void __hostapd_if_send_disassoc(char *ifname, uint8_t *sta_mac,
				uint16_t reason_code, int link_id,
				uint8_t *added_data,
				uint8_t added_data_len);
void __hostapd_if_set_beacon_probe_vendor_ies(char *ifname, uint8_t *buf,
					      size_t buf_len, int link_id);
void __hostapd_if_set_pmk(char *ifname, uint8_t *sta_mac,
			  uint8_t *pmk, size_t pmk_len,
			  uint8_t *pmkid, int session_timeout,
			  struct dot1x_ctx *ctx, bool dot1x_done);
void __hostapd_if_set_ptk(char *ifname, uint8_t *sta_mac,
			  uint8_t *kck, size_t kck_len,
			  uint8_t *kek, size_t kek_len,
			  uint8_t *tk, size_t tk_len, bool authorized);
void __hostapd_if_set_gtk(char *ifname, int link_id,
			  int gtk_idx, uint8_t *gtk, size_t gtk_len);
void __hostapd_if_start_sa_query(char *ifname, uint8_t *sta_mac, int link_id);
void __hostapd_if_set_authorized(char *ifname, uint8_t *sta_mac, int authorized);
void __hostapd_if_eapol_key_tx(char *ifname, uint8_t *sta_mac, uint8_t link_id,
                              uint8_t *frame, uint16_t frame_len);
void __hostapd_if_eapol_tx(char *ifname, uint8_t *sta_mac, int link_id, uint8_t type,
			   uint8_t *data, uint16_t data_len, bool with_header);
void __hostapd_if_send_frame(char *ifname, int link_id,
			     uint8_t *frame, uint16_t frame_len);
void __hostapd_if_remote_auth_response(char *ifname, uint8_t *sta_mac,
				       struct hostapd_if_frame_ctx *ctx);


static int hostapd_if_assoc_response(char *ifname, uint8_t *sta_mac,
				     struct hostapd_if_frame_ctx *ctx)
{
	int __validate_ret =
		hostapd_if_assoc_response_validate_inputs(ifname, sta_mac, ctx);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_ASSOC_RESPONSE;
	os_strlcpy(payload.msg.assoc_response.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.assoc_response.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.assoc_response.ctx = ctx;

	hostapd_if_assoc_response_dump_params(ifname, sta_mac, ctx);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_assoc_response(ifname, sta_mac, ctx);
	return 0;
}

static int hostapd_if_auth_response(char *ifname, uint8_t *sta_mac,
				    struct hostapd_if_frame_ctx *ctx)
{
	int __validate_ret =
		hostapd_if_auth_response_validate_inputs(ifname, sta_mac, ctx);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_AUTH_RESPONSE;
	os_strlcpy(payload.msg.auth_response.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.auth_response.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.auth_response.ctx = ctx;

	hostapd_if_auth_response_dump_params(ifname, sta_mac, ctx);
	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_auth_response(ifname, sta_mac, ctx);
	return 0;
}

static int hostapd_if_send_deauth(char *ifname, uint8_t *sta_mac,
				  uint16_t reason_code, int link_id,
				  uint8_t *added_data,
				  uint8_t added_data_len)
{
	int __validate_ret =
		hostapd_if_send_deauth_validate_inputs(ifname, sta_mac,
						       reason_code,
						       added_data,
						       added_data_len);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SEND_DEAUTH;
	os_strlcpy(payload.msg.send_deauth.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.send_deauth.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.send_deauth.reason_code = reason_code;
	payload.msg.send_deauth.link_id = link_id;
	payload.msg.send_deauth.added_data = added_data;
	payload.msg.send_deauth.added_data_len = added_data_len;

	hostapd_if_send_deauth_dump_params(ifname, sta_mac, reason_code,
					   link_id,
					   added_data, added_data_len);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_send_deauth(ifname, sta_mac, reason_code, link_id, added_data, added_data_len);
	return 0;
}

static int hostapd_if_send_disassoc(char *ifname, uint8_t *sta_mac,
				    uint16_t reason_code, int link_id,
				    uint8_t *added_data,
				    uint8_t added_data_len)
{
	int __validate_ret =
		hostapd_if_send_disassoc_validate_inputs(ifname, sta_mac,
							 reason_code,
							 added_data,
							 added_data_len);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SEND_DISASSOC;
	os_strlcpy(payload.msg.send_disassoc.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.send_disassoc.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.send_disassoc.reason_code = reason_code;
	payload.msg.send_disassoc.link_id = link_id;
	payload.msg.send_disassoc.added_data = added_data;
	payload.msg.send_disassoc.added_data_len = added_data_len;

	hostapd_if_send_disassoc_dump_params(ifname, sta_mac, reason_code,
					     added_data, added_data_len);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_send_disassoc(ifname, sta_mac, reason_code, link_id, added_data, added_data_len);
	return 0;
}

int hostapd_if_set_beacon_probe_vendor_ies(char *ifname, uint8_t *buf,
					   size_t buf_len, int link_id)
{
	int __validate_ret =
		hostapd_if_set_beacon_probe_vendor_ies_validate_inputs(
			ifname, buf, buf_len, link_id);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SET_BEACON_PROBE_VENDOR_IES;
	os_strlcpy(payload.msg.set_beacon_probe_vendor_ies.ifname, ifname, IFNAMSIZ + 1);
	payload.msg.set_beacon_probe_vendor_ies.buf = buf;
	payload.msg.set_beacon_probe_vendor_ies.buf_len = buf_len;
	payload.msg.set_beacon_probe_vendor_ies.link_id = link_id;

	hostapd_if_set_beacon_probe_vendor_ies_dump_params(ifname, buf,
							   buf_len, link_id);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_set_beacon_probe_vendor_ies(ifname, buf, buf_len, link_id);
	return 0;
}

static int hostapd_if_set_pmk(char *ifname, uint8_t *sta_mac,
			      uint8_t *pmk, size_t pmk_len,
			      uint8_t *pmkid, int session_timeout,
			      struct dot1x_ctx *ctx, bool dot1x_done)
{
	int __validate_ret =
		hostapd_if_set_pmk_validate_inputs(ifname, sta_mac, pmk,
						   pmk_len, pmkid);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SET_PMK;
	os_strlcpy(payload.msg.set_pmk.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.set_pmk.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.set_pmk.pmk = pmk;
	payload.msg.set_pmk.pmk_len = pmk_len;
	payload.msg.set_pmk.pmkid = pmkid;
	payload.msg.set_pmk.session_timeout = session_timeout;
	payload.msg.set_pmk.ctx = ctx;
	payload.msg.set_pmk.dot1x_done = dot1x_done;

	hostapd_if_set_pmk_dump_params(ifname, sta_mac, pmk, pmk_len, pmkid);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_set_pmk(ifname, sta_mac, pmk, pmk_len, pmkid, session_timeout,
				     ctx, dot1x_done);
	return 0;
}

static int hostapd_if_set_ptk(char *ifname, uint8_t *sta_mac,
			      uint8_t *kck, size_t kck_len,
			      uint8_t *kek, size_t kek_len,
			      uint8_t *tk, size_t tk_len, bool authorized)
{
	int __validate_ret =
		hostapd_if_set_ptk_validate_inputs(ifname, sta_mac, kck,
						   kck_len, kek, kek_len,
						   tk, tk_len, authorized);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SET_PTK;
	os_strlcpy(payload.msg.set_ptk.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.set_ptk.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.set_ptk.kck = kck;
	payload.msg.set_ptk.kck_len = kck_len;
	payload.msg.set_ptk.kek = kek;
	payload.msg.set_ptk.kek_len = kek_len;
	payload.msg.set_ptk.tk = tk;
	payload.msg.set_ptk.tk_len = tk_len;
	payload.msg.set_ptk.authorized = authorized;

	hostapd_if_set_ptk_dump_params(ifname, sta_mac, kck, kck_len,
				       kek, kek_len, tk, tk_len, authorized);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_set_ptk(ifname, sta_mac, kck, kck_len, kek, kek_len, tk, tk_len,
				     authorized);
	return 0;
}

static int hostapd_if_set_gtk(char *ifname, int link_id,
			      int gtk_idx, uint8_t *gtk, size_t gtk_len)
{
	int __validate_ret =
		hostapd_if_set_gtk_validate_inputs(ifname, link_id, gtk_idx,
						   gtk, gtk_len);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_SET_GTK;
	os_strlcpy(payload.msg.set_gtk.ifname, ifname, IFNAMSIZ + 1);
	payload.msg.set_gtk.link_id = link_id;
	payload.msg.set_gtk.gtk_idx = gtk_idx;
	payload.msg.set_gtk.gtk = gtk;
	payload.msg.set_gtk.gtk_len = gtk_len;

	hostapd_if_set_gtk_dump_params(ifname, link_id, gtk_idx, gtk,
				       gtk_len);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_set_gtk(ifname, link_id, gtk_idx, gtk, gtk_len);
	return 0;
}

static int hostapd_if_trigger_eapol_m3(char *ifname, uint8_t *sta_mac)
{
	int __validate_ret =
		hostapd_if_trigger_eapol_m3_validate_inputs(ifname, sta_mac);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_TRIGGER_EAPOL_M3;
	os_strlcpy(payload.msg.trigger_eapol_m3.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.trigger_eapol_m3.sta_mac, sta_mac, ETH_ALEN);

	hostapd_if_trigger_eapol_m3_dump_params(ifname, sta_mac);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_trigger_eapol_m3(ifname, sta_mac);
	return 0;
}

static void hostapd_if_eapol_tx(char *ifname, uint8_t *sta_mac, int link_id,
				uint8_t type, uint8_t *data, uint16_t data_len,
				bool with_header)
{
	int __validate_ret =
		hostapd_if_eapol_tx_validate_inputs(ifname, sta_mac, link_id,
						    data, data_len);
	struct hostapd_if_eloop_payload payload = {0};

	if (__validate_ret < 0) {
		os_free(data);
		return;
	}

	payload.opcode = HOSTAPD_IF_ASYNC_EAPOL_TX;
	os_strlcpy(payload.msg.eapol_tx.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.eapol_tx.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.eapol_tx.link_id = link_id;
	payload.msg.eapol_tx.type = type;
	payload.msg.eapol_tx.data_len = data_len;
	payload.msg.eapol_tx.data = data;
	payload.msg.eapol_tx.with_header = with_header;

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_eapol_tx(ifname, sta_mac, link_id, type, data,
				      data_len, with_header);
}

static int hostapd_if_start_sa_query(char *ifname, uint8_t *sta_mac, int link_id)
{
	int __validate_ret =
		hostapd_if_start_sa_query_validate_inputs(ifname, sta_mac, link_id);
	struct hostapd_if_eloop_payload payload;

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_START_SA_QUERY;
	os_strlcpy(payload.msg.start_sa_query.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.start_sa_query.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.start_sa_query.link_id = link_id;

	hostapd_if_start_sa_query_dump_params(ifname, sta_mac, link_id);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_start_sa_query(ifname, sta_mac, link_id);
	return 0;
}

static void hostapd_if_set_authorized(char *ifname, uint8_t *sta_mac, int authorized)
{
	struct hostapd_if_eloop_payload payload;

	int __validate_ret =
		hostapd_if_set_authorized_validate_inputs(ifname, sta_mac, authorized);

	if (__validate_ret < 0)
		return;

	payload.opcode = HOSTAPD_IF_ASYNC_SET_AUTHORIZED;
	os_strlcpy(payload.msg.set_authorized.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.set_authorized.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.set_authorized.authorized = authorized;

	hostapd_if_set_authorized_dump_params(ifname, sta_mac, authorized);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_set_authorized(ifname, sta_mac, authorized);
}


static void hostapd_if_send_frame(char *ifname, int link_id,
				  uint8_t *frame, uint16_t frame_len)
{
	struct hostapd_if_eloop_payload payload;

	int __validate_ret = hostapd_if_send_frame_validate_inputs(ifname, link_id, frame,
								   frame_len);

	if (__validate_ret < 0)
		return;

	payload.opcode = HOSTAPD_IF_ASYNC_SEND_FRAME;
	os_strlcpy(payload.msg.send_frame.ifname, ifname, IFNAMSIZ + 1);
	payload.msg.send_frame.link_id = link_id;
	payload.msg.send_frame.frame = frame;
	payload.msg.send_frame.frame_len = frame_len;

	hostapd_if_send_frame_dump_params(ifname, link_id, frame, frame_len);
	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_send_frame(ifname, link_id, frame, frame_len);
}

static int hostapd_if_remote_auth_response(char *ifname, uint8_t *sta_mac,
					   struct hostapd_if_frame_ctx *ctx)
{
	struct hostapd_if_eloop_payload payload;
	int __validate_ret =
		hostapd_if_remote_auth_response_validate_inputs(ifname, sta_mac, ctx);

	if (__validate_ret < 0)
		return __validate_ret;

	payload.opcode = HOSTAPD_IF_ASYNC_REMOTE_AUTH_RESPONSE;
	os_strlcpy(payload.msg.remote_auth_response.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.remote_auth_response.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.remote_auth_response.ctx = ctx;

	hostapd_if_remote_auth_response_dump_params(ifname, sta_mac, ctx);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_remote_auth_response(ifname, sta_mac, ctx);
	return 0;
}

static void hostapd_if_eapol_key_tx(char *ifname, uint8_t *sta_mac,uint8_t link_id,
				    uint8_t *frame, uint16_t frame_len)
{
	struct hostapd_if_eloop_payload payload;

	int __validate_ret =
		hostapd_if_eapol_key_tx_validate_inputs(ifname, sta_mac, link_id,
							frame, frame_len);

	if (__validate_ret < 0)
		return;
	payload.opcode = HOSTAPD_IF_ASYNC_EAPOL_KEY_TX;
	os_strlcpy(payload.msg.eapol_key_tx.ifname, ifname, IFNAMSIZ + 1);
	os_memcpy(payload.msg.eapol_key_tx.sta_mac, sta_mac, ETH_ALEN);
	payload.msg.eapol_key_tx.link_id = link_id;
	payload.msg.eapol_key_tx.frame = frame;
	payload.msg.eapol_key_tx.frame_len = frame_len;

	hostapd_if_eapol_key_tx_dump_params(ifname, sta_mac, link_id, frame,
					    frame_len);

	if (hostapd_if_eloop_sock >= 0)
		send(hostapd_if_eloop_sock, &payload, sizeof(payload), 0);
	else
		__hostapd_if_eapol_key_tx(ifname, sta_mac, link_id, frame, frame_len);
}

static void hostapd_if_eloop_socket_read(int sock, void *eloop_ctx,
					 void *sock_ctx)
{
	uint8_t buf[HOSTAPD_IF_ASYNC_MAXMSG];
	ssize_t len = recv(sock, buf, sizeof(buf), 0);

	if (len < (ssize_t) sizeof(struct hostapd_if_eloop_payload))
		return;

	struct hostapd_if_eloop_payload *payload =
		(struct hostapd_if_eloop_payload *) buf;

	switch (payload->opcode) {
	case HOSTAPD_IF_ASYNC_ASSOC_RESPONSE: {
		struct hostapd_if_assoc_response_msg *msg =
			&payload->msg.assoc_response;

		__hostapd_if_assoc_response(msg->ifname, msg->sta_mac,
					    msg->ctx);
		break;
	}
	case HOSTAPD_IF_ASYNC_AUTH_RESPONSE: {
		struct hostapd_if_auth_response_msg *msg =
			&payload->msg.auth_response;

		__hostapd_if_auth_response(msg->ifname, msg->sta_mac,
					   msg->ctx);
		break;
	}
	case HOSTAPD_IF_ASYNC_SEND_DEAUTH: {
		struct hostapd_if_send_deauth_msg *msg =
			&payload->msg.send_deauth;

		__hostapd_if_send_deauth(msg->ifname, msg->sta_mac,
					 msg->reason_code, msg->link_id, msg->added_data,
					 msg->added_data_len);
		break;
	}
	case HOSTAPD_IF_ASYNC_SEND_DISASSOC: {
		struct hostapd_if_send_disassoc_msg *msg =
			&payload->msg.send_disassoc;

		__hostapd_if_send_disassoc(msg->ifname, msg->sta_mac,
					   msg->reason_code, msg->link_id, msg->added_data,
					   msg->added_data_len);
		break;
	}
	case HOSTAPD_IF_ASYNC_SET_BEACON_PROBE_VENDOR_IES: {
		struct hostapd_if_set_beacon_probe_vendor_ies_msg *msg =
			&payload->msg.set_beacon_probe_vendor_ies;

		__hostapd_if_set_beacon_probe_vendor_ies(
			msg->ifname, msg->buf, msg->buf_len, msg->link_id);
		break;
	}
	case HOSTAPD_IF_ASYNC_SET_PMK: {
		struct hostapd_if_set_pmk_msg *msg = &payload->msg.set_pmk;

		__hostapd_if_set_pmk(msg->ifname, msg->sta_mac, msg->pmk,
				     msg->pmk_len, msg->pmkid, msg->session_timeout,
				     msg->ctx, msg->dot1x_done);
		break;
	}
	case HOSTAPD_IF_ASYNC_SET_PTK: {
		struct hostapd_if_set_ptk_msg *msg = &payload->msg.set_ptk;

		__hostapd_if_set_ptk(msg->ifname, msg->sta_mac,
				     msg->kck, msg->kck_len,
				     msg->kek, msg->kek_len,
				     msg->tk, msg->tk_len,
				     msg->authorized);
		break;
	}
	case HOSTAPD_IF_ASYNC_SET_GTK: {
		struct hostapd_if_set_gtk_msg *msg = &payload->msg.set_gtk;

		__hostapd_if_set_gtk(msg->ifname, msg->link_id, msg->gtk_idx,
				     msg->gtk, msg->gtk_len);
		break;
	}
	case HOSTAPD_IF_ASYNC_START_SA_QUERY: {
		struct hostapd_if_start_sa_query_msg *msg =
			&payload->msg.start_sa_query;

		__hostapd_if_start_sa_query(msg->ifname, msg->sta_mac, msg->link_id);
		break;
	}
	case HOSTAPD_IF_ASYNC_SET_AUTHORIZED: {
		struct hostapd_if_set_authorized_msg *msg = &payload->msg.set_authorized;

		__hostapd_if_set_authorized(msg->ifname, msg->sta_mac, msg->authorized);
		break;
	}
	case HOSTAPD_IF_ASYNC_TRIGGER_EAPOL_M3: {
		struct hostapd_if_trigger_eapol_m3_msg *msg =
			&payload->msg.trigger_eapol_m3;

		__hostapd_if_trigger_eapol_m3(msg->ifname, msg->sta_mac);
		break;
	}
	case HOSTAPD_IF_ASYNC_EAPOL_TX: {
		struct hostapd_if_eapol_tx_msg *msg = &payload->msg.eapol_tx;

		__hostapd_if_eapol_tx(msg->ifname, msg->sta_mac,
				      msg->link_id, msg->type,
				      msg->data, msg->data_len, msg->with_header);
	}
	case HOSTAPD_IF_ASYNC_SEND_FRAME: {
		struct hostapd_if_send_frame_msg *msg = &payload->msg.send_frame;

		__hostapd_if_send_frame(msg->ifname, msg->link_id, msg->frame,
					msg->frame_len);
		break;
	}
	case HOSTAPD_IF_ASYNC_EAPOL_KEY_TX: {
		struct hostapd_if_eapol_key_tx_msg *msg = &payload->msg.eapol_key_tx;

		__hostapd_if_eapol_key_tx(msg->ifname, msg->sta_mac, msg->link_id,
					  msg->frame, msg->frame_len);
		break;
	}
	case HOSTAPD_IF_ASYNC_REMOTE_AUTH_RESPONSE: {
		struct hostapd_if_remote_auth_response_msg *msg =
			&payload->msg.remote_auth_response;

		__hostapd_if_remote_auth_response(msg->ifname, msg->sta_mac,
						  msg->ctx);
		break;
	}
	default:
		break;
	}
}

/*
 * Helper: initialize eloop socket infrastructure (server + client),
 * register callback in eloop
 */
int hostapd_if_eloop_init(enum hostapd_if_eloop_type type)
{
	struct sockaddr_un addr;

	if (type == HOSTAPD_IF_ELOOP_DIRECT_CALL) {
		wpa_printf(MSG_DEBUG,
			"hostapd_if: Direct-call mode selected, skipping eloop socket setup");
		hostapd_if_eloop_server_sock = -1;
		hostapd_if_eloop_sock = -1;
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: Initializing eloop socket infrastructure");

	/*
	 * Create SERVER socket
	 */
	hostapd_if_eloop_server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (hostapd_if_eloop_server_sock < 0) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: Failed to create server socket: %s",
			   strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_snprintf(hostapd_if_layer_socket_path, sizeof(hostapd_if_layer_socket_path),
		    "%s_%d.sock", HOSTAPD_IF_ASYNC_SOCKET_PATH, getpid());
	os_strlcpy(addr.sun_path, hostapd_if_layer_socket_path,
		sizeof(addr.sun_path));

	/*
	 * Try to bind server socket - only unlink if bind fails
	 * with EADDRINUSE
	 */
	if (bind(hostapd_if_eloop_server_sock, (struct sockaddr *) &addr,
		 sizeof(addr)) < 0) {
		if (errno == EADDRINUSE) {
			wpa_printf(MSG_WARNING,
				   "hostapd_if: Socket path in use, "
				   "removing stale socket: %s",
				   hostapd_if_layer_socket_path);

			unlink(hostapd_if_layer_socket_path);

			/*
			 * Retry bind after unlink
			 */
			if (bind(hostapd_if_eloop_server_sock,
				 (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				wpa_printf(MSG_ERROR,
					   "hostapd_if: Failed to bind "
					   "server socket after unlink: %s",
					   strerror(errno));
				close(hostapd_if_eloop_server_sock);
				hostapd_if_eloop_server_sock = -1;
				return -1;
			}
		} else {
			wpa_printf(MSG_ERROR,
				   "hostapd_if: Failed to bind server "
				   "socket: %s",
				   strerror(errno));
			close(hostapd_if_eloop_server_sock);
			hostapd_if_eloop_server_sock = -1;
			return -1;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: Server socket bound to %s (fd=%d)",
		   hostapd_if_layer_socket_path,
		   hostapd_if_eloop_server_sock);

	/*
	 * Register server socket with eloop for reading
	 */
	eloop_register_read_sock(hostapd_if_eloop_server_sock,
				 hostapd_if_eloop_socket_read, NULL, NULL);

	/*
	 * Create CLIENT socket
	 */
	hostapd_if_eloop_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (hostapd_if_eloop_sock < 0) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: Failed to create client socket: %s",
			   strerror(errno));
		eloop_unregister_read_sock(hostapd_if_eloop_server_sock);
		close(hostapd_if_eloop_server_sock);
		unlink(hostapd_if_layer_socket_path);
		hostapd_if_eloop_server_sock = -1;
		return -1;
	}

	/*
	 * Connect client socket to server socket
	 */
	if (connect(hostapd_if_eloop_sock, (struct sockaddr *) &addr,
		    sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR,
			   "hostapd_if: Failed to connect client socket: %s",
			   strerror(errno));
		close(hostapd_if_eloop_sock);
		eloop_unregister_read_sock(hostapd_if_eloop_server_sock);
		close(hostapd_if_eloop_server_sock);
		unlink(hostapd_if_layer_socket_path);
		hostapd_if_eloop_sock = -1;
		hostapd_if_eloop_server_sock = -1;
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: Client socket connected (fd=%d)",
		   hostapd_if_eloop_sock);

	wpa_printf(MSG_INFO,
		   "hostapd_if: Async socket infrastructure initialized "
		   "successfully");

	return 0;
}

/*
 * Deinitialize eloop socket infrastructure (client + server)
 */
void hostapd_if_eloop_deinit(void)
{
	/*
	 * Close client socket
	 */
	if (hostapd_if_eloop_sock >= 0) {
		close(hostapd_if_eloop_sock);
		hostapd_if_eloop_sock = -1;
	}

	/*
	 * Close server socket and cleanup
	 */
	if (hostapd_if_eloop_server_sock >= 0) {
		eloop_unregister_read_sock(hostapd_if_eloop_server_sock);
		close(hostapd_if_eloop_server_sock);
		unlink(hostapd_if_layer_socket_path);
		hostapd_if_eloop_server_sock = -1;
	}

	wpa_printf(MSG_DEBUG,
		   "hostapd_if: Async socket infrastructure deinitialized");
}

/*
 * Wire plugin inbound handlers to local implementations
 */
void hostapd_if_eloop_inbound_handlers(
	struct hostapd_external_app_object *plugin)
{
	plugin->assoc_response = hostapd_if_assoc_response;
	plugin->auth_response = hostapd_if_auth_response;
	plugin->send_deauth = hostapd_if_send_deauth;
	plugin->send_disassoc = hostapd_if_send_disassoc;
	plugin->set_beacon_probe_vendor_ies =
		hostapd_if_set_beacon_probe_vendor_ies;
	plugin->set_pmk = hostapd_if_set_pmk;
	plugin->set_ptk = hostapd_if_set_ptk;
	plugin->set_gtk = hostapd_if_set_gtk;
	plugin->start_sa_query = hostapd_if_start_sa_query;
	plugin->set_authorized = hostapd_if_set_authorized;
	plugin->trigger_eapol_m3 = hostapd_if_trigger_eapol_m3;
	plugin->eapol_key_tx = hostapd_if_eapol_key_tx;
	plugin->eapol_tx = hostapd_if_eapol_tx;
	plugin->send_frame = hostapd_if_send_frame;
	plugin->remote_auth_response = hostapd_if_remote_auth_response;
}
