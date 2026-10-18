/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause
*/


/*
 * Plugin stub for hostapd external interface
 *
 * This module registers the MQTT plugin callbacks with hostapd during
 * startup by calling hostapd_plugin_register().
 *
 */
#ifdef CONFIG_QCN_EXTN
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "../qcn_extns/hostapd_external_interface.h"
#include "common.h"
#include "utils/includes.h"

#include "utils/common.h"
#include "utils/bitfield.h"
#include "common/wpa_ctrl.h"
#include "ap/hostapd.h"
#include "ap/wpa_auth.h"
#include "ap/wpa_auth_i.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "../../qcn_extns/hostapd_if_plugin.h"
#include "utils/mqtt_feature_map.h"
#include "utils/mqtt_eloop.h"
#include "utils/mqtt_tlv_map.h"
#include "hostapd_if_mqtt.h"

#define QUEUE_MAX_SIZE 100

static struct hostapd_external_app_object mqtt_plugin;

static struct mqtt_eloop_ctx *hostapd_mqtt_ctx(const struct hostapd_data *hapd)
{
	if (!hapd || !hapd->iface || !hapd->iface->interfaces)
		return NULL;
	return hapd->iface->interfaces->mqtt_ctx;
}

#ifdef CONFIG_MQTT
const char *hostapd_if_event_mqtt_string(enum hostapd_if_event_type type)
{
	const char *event_str;
	switch (type) {
		case HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE:
			event_str = "AUTH_TX_COMPLETE";
			break;
		case HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE:
			event_str = "ASSOC_TX_COMPLETE";
			break;
		case HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION:
			event_str = "AUTHORIZE_COMPLETION";
			break;
		case HOSTAPD_IF_EVENT_DEAUTH:
			event_str = "DEAUTH";
			break;
		case HOSTAPD_IF_EVENT_DISASSOC:
			event_str = "DISASSOC";
			break;
		case HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION:
			event_str = "SA_QUERY_COMPLETION";
			break;
		case HOSTAPD_IF_EVENT_GTK_COMPLETION:
			event_str = "GTK_COMPLETION";
			break;
		case HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED:
			event_str = "EAPOL_M2_RECEIVED";
			break;
		case HOSTAPD_IF_EVENT_ACTION_COMPLETION:
			event_str = "ACTION_COMPLETION";
			break;
		case HOSTAPD_IF_EVENT_INBOUND_CALL_ERROR:
			event_str = "INBOUND_CALL_ERROR";
			break;
		default:
			event_str = "UNKNOWN_EVENT";
			break;
	}
	return event_str;
}

static void notify_assoc_tx_complete(void *hapd_ctx,
				      const struct hostapd_if_event *event)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_ASSOC_TX_COMPLETE_IFNAME,
				  event->ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_ASSOC_TX_COMPLETE_STA_MAC,
			       (uint8_t *) event->sta_mac);
	rc |= mqtt_tlv_add_u8(msg, TLV_HIF_ASSOC_TX_COMPLETE_OK,
			      (uint8_t) event->data.assoc_resp_completion.ok);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_ASSOC_TX_COMPLETE_STATUS,
			       event->data.assoc_resp_completion.status);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_ASSOC_TX_COMPLETE_AID,
			       event->data.assoc_resp_completion.aid);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE"
		   " iface=%s sta=" MACSTR " ok=%d status=%u aid=%u",
		   event->ifname, MAC2STR(event->sta_mac),
		   event->data.assoc_resp_completion.ok,
		   event->data.assoc_resp_completion.status,
		   event->data.assoc_resp_completion.aid);

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = 64;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *) hapd_ctx),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish"
			   " EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE iface=%s",
			   event->ifname);
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

static void notify_auth_tx_complete_event(void *hapd_ctx,
					   const struct hostapd_if_event *event)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_AUTH_TX_COMPLETE_IFNAME,
				  event->ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_AUTH_TX_COMPLETE_STA_MAC,
			       (uint8_t *) event->sta_mac);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE"
		   " iface=%s sta=" MACSTR,
		   event->ifname, MAC2STR(event->sta_mac));

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = 64;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *) hapd_ctx),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish"
			   " EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE iface=%s",
			   event->ifname);
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

static void notify_deauth_disassoc_event(void *hapd_ctx,
					  const struct hostapd_if_event *event)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;
	uint16_t evt_id;

	evt_id = (event->type == HOSTAPD_IF_EVENT_DEAUTH)
		? EVT_ID_HIF_EVENT_DEAUTH
		: EVT_ID_HIF_EVENT_DISASSOC;

	msg = mqtt_tlv_message_alloc(evt_id);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_IFNAME,
				  event->ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_STA_MAC,
			       (uint8_t *) event->sta_mac);
	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_ID,
			       (int32_t) event->data.deauth_disassoc.link_id);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_MAC,
			       (uint8_t *) event->data.deauth_disassoc.link_mac);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_REASON_CODE,
			       event->data.deauth_disassoc.reason_code);
	rc |= mqtt_tlv_add_u32(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_DISCONNECT_TYPE,
			       (uint32_t) event->data.deauth_disassoc.type);
	rc |= mqtt_tlv_add_u8(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_IS_TX_STATUS,
			      (uint8_t) event->data.deauth_disassoc.is_tx_status);
	rc |= mqtt_tlv_add_u32(msg, TLV_HIF_DEAUTH_DISASSOC_EVT_TX_STATUS_OK,
			       (uint32_t) event->data.deauth_disassoc.tx_status_ok);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing %s iface=%s sta=" MACSTR
		   " link_id=%d reason=%u disconnect_type=%u"
		   " is_tx_status=%d tx_status_ok=%d",
		   event->type == HOSTAPD_IF_EVENT_DEAUTH
			? "EVT_ID_HIF_EVENT_DEAUTH"
			: "EVT_ID_HIF_EVENT_DISASSOC",
		   event->ifname, MAC2STR(event->sta_mac),
		   event->data.deauth_disassoc.link_id,
		   event->data.deauth_disassoc.reason_code,
		   (uint32_t) event->data.deauth_disassoc.type,
		   event->data.deauth_disassoc.is_tx_status,
		   event->data.deauth_disassoc.tx_status_ok);

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = 128;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *) hapd_ctx),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish %s iface=%s",
			   event->type == HOSTAPD_IF_EVENT_DEAUTH
				? "EVT_ID_HIF_EVENT_DEAUTH"
				: "EVT_ID_HIF_EVENT_DISASSOC",
			   event->ifname);
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

static void notify_event(void *hapd_ctx, struct hostapd_if_event *event)
{

	wpa_printf(MSG_DEBUG,
		   "notifying event of type %s\n ifname = %s\n STA MAC = " MACSTR "\n",
		   hostapd_if_event_mqtt_string(event->type),
		   event->ifname,  MAC2STR(event->sta_mac));
	switch (event->type) {

		case HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE:
			notify_auth_tx_complete_event(hapd_ctx, event);
			break;

		case HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE:
			notify_assoc_tx_complete(hapd_ctx, event);
			break;

		case HOSTAPD_IF_EVENT_DEAUTH:
		case HOSTAPD_IF_EVENT_DISASSOC:
			notify_deauth_disassoc_event(hapd_ctx, event);
			break;

		default:
			break;
	}
}
static int interface_create(char *ifname, void *hapd)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;

	if (!ifname || !ifname[0])
		return -1;

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_INTERFACE_CREATE);
	if (!msg)
		return -1;

	if (mqtt_tlv_add_string(msg, TLV_HIF_INTERFACE_CREATE_IFNAME, ifname) < 0) {
		mqtt_tlv_message_free(msg);
		return -1;
	}

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = strlen(ifname) + 32;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return -1;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *)hapd),
			      topic, buf, len, 0, true) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish EVT_ID_HIF_INTERFACE_CREATE iface=%s",
			   ifname);
		return -1;
	} else {
		wpa_printf(MSG_DEBUG,
			   "MQTT HIF: published EVT_ID_HIF_INTERFACE_CREATE iface=%s",
			   ifname);
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
	return 0;
}
static void notify_auth(void *hapd, char *iface, uint8_t *sta_mac,
                       const uint8_t *frame, uint16_t frame_len,
		       struct hostapd_if_frame_ctx *ctx)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;
	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_NOTIFY_AUTH);

	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_AUTH_IND_IFACE, iface);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_AUTH_IND_STA_MAC, sta_mac);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_AUTH_IND_STATUS_CODE,
			       (uint16_t) ctx->status_code);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_AUTH_IND_AUTH_TRANSACTION,
			       ctx->data.auth_req.auth_transaction);
	rc |= mqtt_tlv_add_u8(msg, TLV_HIF_AUTH_IND_ALLOW_REUSE,
			      (uint8_t) ctx->data.auth_req.allow_reuse);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_AUTH_IND_AUTH_ALG,
			       ctx->data.auth_req.auth_alg);

	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_AUTH_IND_RSSI_DBM, ctx->data.auth_req.rssi);
	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_AUTH_IND_RX_LINK_ID,
			       (int32_t) ctx->rx_link_id);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_AUTH_IND_STA_ASSOC_LINK_MAC,
			       ctx->data.auth_req.sta_assoc_link_mac);
	rc |= mqtt_tlv_add_binary(msg, TLV_HIF_AUTH_IND_FRAME,
				  frame, frame_len);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_AUTH_IND_FRAME_LEN, frame_len);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}
       wpa_printf(MSG_DEBUG,
                  "MQTT HIF: publishing EVT_ID_HIF_NOTIFY_AUTH iface=%s sta="
                  MACSTR " status=%u auth_transaction=%u allow_reuse=%d "
                  "auth_alg=%u rssi=%d rx_link_id=%d",
                  iface && iface[0] ? iface : "?",
                  MAC2STR(sta_mac),
                  (unsigned int) (uint16_t) ctx->status_code,
                  (unsigned int) ctx->data.auth_req.auth_transaction,
                  ctx->data.auth_req.allow_reuse,
                  (unsigned int) ctx->data.auth_req.auth_alg,
                  ctx->data.auth_req.rssi, ctx->rx_link_id);

       mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
       buf_len = frame_len + 160; // Fix : MQTT_AUTH_IND_HDR_OVERHEAD;
       buf = os_malloc(buf_len);
       if (!buf) {
	       mqtt_tlv_message_free(msg);
	       return;
       }

       len = mqtt_tlv_serialize(msg, buf, buf_len);

       if (len <= 0 || mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *)hapd), topic, buf,
			       len, 0, false) < 0) {
	       os_free(buf);
	       mqtt_tlv_message_free(msg);
	       return;
       }

       os_free(buf);
       mqtt_tlv_message_free(msg);
}

static void notify_assoc(void *hapd, char *ifname, uint8_t *sta_mac,
			 const uint8_t *frame, uint16_t frame_len,
			 struct hostapd_if_frame_ctx *ctx)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_NOTIFY_ASSOC);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_ASSOC_IND_IFACE, ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_ASSOC_IND_STA_MAC, sta_mac);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_ASSOC_IND_STATUS_CODE,
			       (uint16_t) ctx->status_code);
	rc |= mqtt_tlv_add_u8(msg, TLV_HIF_ASSOC_IND_IS_REASSOC,
			      (uint8_t) !!ctx->data.assoc_req.is_reassoc);

	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_ASSOC_IND_RSSI_DBM, ctx->data.assoc_req.rssi);
	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_ASSOC_IND_RX_LINK_ID,
			       (int32_t) ctx->rx_link_id);
	rc |= mqtt_tlv_add_u32(msg, TLV_HIF_ASSOC_IND_VALID_LINK_BITMAP,
			       ctx->data.assoc_req.valid_link_bitmap);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_ASSOC_IND_STA_ASSOC_LINK_MAC,
			       ctx->data.assoc_req.sta_assoc_link_mac);

	/* Flat blob of MAX_MLO_LINKS * 6 bytes; zero-filled for invalid links */
	rc |= mqtt_tlv_add_binary(msg, TLV_HIF_ASSOC_IND_STA_LINK_MACS,
				  (const uint8_t *) ctx->data.assoc_req.sta_link_mac,
				  MAX_MLO_LINKS * 6);

	rc |= mqtt_tlv_add_binary(msg, TLV_HIF_ASSOC_IND_FRAME, frame, frame_len);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_ASSOC_IND_FRAME_LEN, frame_len);

	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing EVT_ID_HIF_NOTIFY_ASSOC iface=%s sta="
		   MACSTR " status=%u is_reassoc=%d rssi=%d rx_link_id=%d "
		   "valid_link_bitmap=0x%x",
		   ifname && ifname[0] ? ifname : "?",
		   MAC2STR(sta_mac),
		   (unsigned int)(uint16_t) ctx->status_code,
		   ctx->data.assoc_req.is_reassoc,
		   ctx->data.assoc_req.rssi, ctx->rx_link_id,
		   ctx->data.assoc_req.valid_link_bitmap);

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = frame_len + 160;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *)hapd),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish EVT_ID_HIF_NOTIFY_ASSOC iface=%s",
			   ifname && ifname[0] ? ifname : "?");
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

static void notify_remote_auth(char *ifname, uint8_t *sta_mac,
			       const uint8_t *ies, uint16_t ies_len,
			       struct hostapd_if_frame_ctx *ctx)
{
	wpa_printf(MSG_DEBUG,
		   "Notified Remote Auth for STA " MACSTR
		   " on link_id=%d, ies_len=%u\n",
		   MAC2STR(sta_mac), ctx->rx_link_id, ies_len);
	wpa_printf(MSG_DEBUG,
		   "Remote Auth status_code=%u (%s), is_ml_sta=%d\n",
		   ctx->status_code,
		   ctx->status_code == 0 ? "SUCCESS" : "REJECTED",
		   ctx->data.remote_auth_req.is_ml_sta);
}

static void notify_deauth(void *hapd, char *ifname, uint8_t *sta_mac,
			  const void *frame, size_t frame_len,
			  struct hostapd_if_frame_ctx *ctx)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;
	uint16_t reason = 0xFFFF;
	const uint8_t *f;

	if (frame && frame_len >= 26) {
		f = (const uint8_t *) frame;
		reason = (uint16_t)(f[24] | (f[25] << 8));
	}

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_NOTIFY_DEAUTH);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_DEAUTH_IND_IFACE, ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_DEAUTH_IND_STA_MAC, sta_mac);
	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_DEAUTH_IND_RX_LINK_ID,
			       (int32_t)(ctx ? ctx->rx_link_id : -1));
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_DEAUTH_IND_REASON_CODE, reason);
	if (frame && frame_len > 0)
		rc |= mqtt_tlv_add_binary(msg, TLV_HIF_DEAUTH_IND_FRAME,
					  (const uint8_t *) frame,
					  (uint16_t) frame_len);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_DEAUTH_IND_FRAME_LEN, (uint16_t) frame_len);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing EVT_ID_HIF_NOTIFY_DEAUTH iface=%s sta="
		   MACSTR " rx_link_id=%d reason=%u",
		   ifname && ifname[0] ? ifname : "?",
		   MAC2STR(sta_mac),
		   ctx ? ctx->rx_link_id : -1, reason);

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = frame_len + 64;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *) hapd),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish EVT_ID_HIF_NOTIFY_DEAUTH iface=%s",
			   ifname && ifname[0] ? ifname : "?");
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

static void notify_disassoc(void *hapd, char *ifname, uint8_t *sta_mac,
			    const void *frame, size_t frame_len,
			    struct hostapd_if_frame_ctx *ctx)
{
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t *buf;
	size_t buf_len;
	int len;
	int rc = 0;
	uint16_t reason = 0xFFFF;
	const uint8_t *f;

	if (frame && frame_len >= 26) {
		f = (const uint8_t *) frame;
		reason = (uint16_t)(f[24] | (f[25] << 8));
	}

	msg = mqtt_tlv_message_alloc(EVT_ID_HIF_NOTIFY_DISASSOC);
	if (!msg)
		return;

	rc |= mqtt_tlv_add_string(msg, TLV_HIF_DISASSOC_IND_IFACE, ifname);
	rc |= mqtt_tlv_add_mac(msg, TLV_HIF_DISASSOC_IND_STA_MAC, sta_mac);
	rc |= mqtt_tlv_add_s32(msg, TLV_HIF_DISASSOC_IND_RX_LINK_ID,
			       (int32_t)(ctx ? ctx->rx_link_id : -1));
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_DISASSOC_IND_REASON_CODE, reason);
	if (frame && frame_len > 0)
		rc |= mqtt_tlv_add_binary(msg, TLV_HIF_DISASSOC_IND_FRAME,
					  (const uint8_t *) frame,
					  (uint16_t) frame_len);
	rc |= mqtt_tlv_add_u16(msg, TLV_HIF_DISASSOC_IND_FRAME_LEN, (uint16_t) frame_len);
	if (rc < 0) {
		mqtt_tlv_message_free(msg);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF: publishing EVT_ID_HIF_NOTIFY_DISASSOC iface=%s sta="
		   MACSTR " rx_link_id=%d reason=%u",
		   ifname && ifname[0] ? ifname : "?",
		   MAC2STR(sta_mac),
		   ctx ? ctx->rx_link_id : -1, reason);

	mqtt_build_transmit_topic(MQTT_FEATURE_HOSTAPD_IF, topic, sizeof(topic));
	buf_len = frame_len + 64;
	buf = os_malloc(buf_len);
	if (!buf) {
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, buf_len);
	if (len <= 0 ||
	    mqtt_eloop_publish(hostapd_mqtt_ctx((struct hostapd_data *) hapd),
			      topic, buf, len, 0, false) < 0) {
		wpa_printf(MSG_ERROR,
			   "MQTT HIF: failed to publish EVT_ID_HIF_NOTIFY_DISASSOC iface=%s",
			   ifname && ifname[0] ? ifname : "?");
	}

	os_free(buf);
	mqtt_tlv_message_free(msg);
}

/* Called during hostapd startup to register MQTT callbacks */
void hostapd_if_mqtt_init(void *arg)
{

	/*
	 * Global plugin instance.
	 * Note: Southbound function pointers are to be filled by hostapd core
	 * after registration (as per integration contract).
	 * We only provide northbound handlers here.
	 */
	mqtt_plugin.notify_assoc         = notify_assoc,
	mqtt_plugin.notify_auth          = notify_auth,
	mqtt_plugin.notify_remote_auth   = notify_remote_auth,
	mqtt_plugin.notify_disassoc      = notify_disassoc,
	mqtt_plugin.notify_deauth        = notify_deauth,
	mqtt_plugin.notify_event         = notify_event,
	mqtt_plugin.interface_create     = interface_create,

	hostapd_plugin_register(&mqtt_plugin);
}

void handle_register_frame(struct mqtt_tlv_message *msg,
			  struct hapd_interfaces *interfaces)
{
	uint8_t frame_type_val, policy_val;
	char ifname[IFNAMSIZ + 1] = {0};
	struct hostapd_if_frame_category cat;
	enum hostapd_if_frame_policy policy;
	struct hostapd_data *hapd;

	if (!interfaces || !mqtt_plugin.register_frame) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_FRAME: interfaces or register_frame not available");
		return;
	}

	if (mqtt_tlv_get_string(msg, TLV_HIF_REGISTER_FRAME_IFNAME, ifname, sizeof(ifname)) < 0 ||
	    ifname[0] == '\0') {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_FRAME: missing or empty IFNAME TLV");
		return;
	}

	hapd = hostapd_get_iface(interfaces, ifname);
	if (!hapd) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_FRAME: no hapd found for ifname=%s",
			   ifname);
		return;
	}

	if (mqtt_tlv_get_u8(msg, TLV_HIF_REGISTER_FRAME_FRAME_TYPE,
			    &frame_type_val) < 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_FRAME: missing FRAME_TYPE TLV");
		return;
	}

	if (mqtt_tlv_get_u8(msg, TLV_HIF_REGISTER_FRAME_POLICY,
			    &policy_val) < 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_FRAME: missing POLICY TLV");
		return;
	}

	policy = (enum hostapd_if_frame_policy)policy_val;

	os_memset(&cat, 0, sizeof(cat));
	cat.type = frame_type_val;

	mqtt_plugin.register_frame(hapd, &cat, policy);

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF REGISTER_FRAME: ifname=%s frame_type=%u policy=%u registered",
		   ifname, frame_type_val, policy_val);
}

void handle_register_event(struct mqtt_tlv_message *msg,
			   struct hapd_interfaces *interfaces)
{
	uint8_t event_type_val, set_val;
	char ifname[IFNAMSIZ + 1] = {0};
	enum hostapd_if_event_type event_type;
	struct hostapd_data *hapd;

	if (!interfaces || !mqtt_plugin.register_event) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: interfaces or register_event not available");
		return;
	}

	if (mqtt_tlv_get_string(msg, TLV_HIF_REGISTER_EVENT_IFNAME,
				ifname, sizeof(ifname)) < 0 ||
	    ifname[0] == '\0') {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: missing or empty IFNAME TLV");
		return;
	}

	hapd = hostapd_get_iface(interfaces, ifname);
	if (!hapd) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: no hapd found for ifname=%s",
			   ifname);
		return;
	}

	if (mqtt_tlv_get_u8(msg, TLV_HIF_REGISTER_EVENT_TYPE,
			    &event_type_val) < 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: missing EVENT_TYPE TLV");
		return;
	}

	if (mqtt_tlv_get_u8(msg, TLV_HIF_REGISTER_EVENT_SET, &set_val) < 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: missing SET TLV");
		return;
	}

	if (event_type_val > HOSTAPD_IF_EVENT_DISASSOC) {
		wpa_printf(MSG_WARNING,
			   "MQTT HIF REGISTER_EVENT: unsupported event_type=%u",
			   event_type_val);
		return;
	}

	event_type = (enum hostapd_if_event_type) event_type_val;

	mqtt_plugin.register_event(hapd, event_type, (bool) set_val);

	wpa_printf(MSG_DEBUG,
		   "MQTT HIF REGISTER_EVENT: ifname=%s event_type=%u set=%u registered",
		   ifname, event_type_val, set_val);
}

void hostapd_mqtt_hif_cmd(struct hapd_interfaces *interfaces,
			  uint16_t msg_type,
			  struct mqtt_tlv_message *msg)
{
	switch (msg_type) {
	case CMD_ID_HIF_REGISTER_FRAME:
		handle_register_frame(msg, interfaces);
		break;
	case CMD_ID_HIF_REGISTER_EVENT:
		handle_register_event(msg, interfaces);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "MQTT HIF: unhandled msg_type=0x%04x", msg_type);
		break;
	}
}

#ifdef CONFIG_MQTT_TEST_APP_FORK
static pid_t mqtt_sub_pid = -1;

int hostapd_if_start_mqtt_hif_client(void)
{
	int log_fd;

	mqtt_sub_pid = fork();
	if (mqtt_sub_pid < 0) {
		wpa_printf(MSG_ERROR, "MQTT: fork for external_hif_mqtt_test_client failed: %s",
			   strerror(errno));
		return -1;
	}
	if (mqtt_sub_pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGTERM);
		setsid();
		log_fd = open("/tmp/external_mqtt_hif_logs",
			      O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (log_fd >= 0) {
			dup2(log_fd, STDOUT_FILENO);
			dup2(log_fd, STDERR_FILENO);
			close(log_fd);
		}
		execl("/usr/sbin/external_hif_mqtt_test_client", "external_hif_mqtt_test_client", NULL);
		_exit(1);
	}
	wpa_printf(MSG_INFO, "MQTT: started external_hif_mqtt_test_client (pid %d)", mqtt_sub_pid);
	return 0;
}

void hostapd_if_stop_mqtt_hif_client(void)
{
	if (mqtt_sub_pid > 0) {
		kill(mqtt_sub_pid, SIGTERM);
		waitpid(mqtt_sub_pid, NULL, 0);
		wpa_printf(MSG_INFO, "MQTT: stopped external_hif_mqtt_test_client (pid %d)", mqtt_sub_pid);
		mqtt_sub_pid = -1;
	}
}
#endif /* CONFIG_MQTT_TEST_APP_FORK */

#endif
#endif /* CONFIG_QCN_EXTN */
