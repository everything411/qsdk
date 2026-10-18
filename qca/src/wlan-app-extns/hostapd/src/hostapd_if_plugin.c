/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause
*/


/*
 * Test plugin stub for hostapd external interface
 *
 * This shared library simulates a customer-implemented plugin.
 * It registers itself with hostapd by calling hostapd_plugin_register()
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "hostapd_external_interface.h"
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
#include "hostapd_if_plugin.h"

#define QUEUE_MAX_SIZE 100

/* frame invoke policy related configuration options */
struct hostapd_if_invoke_frame_config {
	int out_of_order;        /* Number of auth frames to collect before processing */
	bool process_frames;          /* Whether to process auth frames */
	bool send_response;           /* Whether to send auth response */
	int delay;                    /* Delay in ms before sending auth response */
	int status_code;              /* Status code to send in auth response (0 = success) */
};


struct hostapd_if_eapol_config {
	bool send_m3;                  /* Whether to send EAPOL M3 response */
	int delay_m3_ms;               /* Delay in ms before sending M3 (0 = no delay) */
	uint8_t override_m3_mac[6];    /* Override MAC address for EAPOL M3 response */
	int override_m3_mac_set;       /* Flag: 1 if override_m3_mac should be used */
};

struct {
	/* Authentication configuration */
	struct hostapd_if_invoke_frame_config auth;

	/* Association configuration */
	struct hostapd_if_invoke_frame_config assoc;

	/* EAPOL configuration */
	struct hostapd_if_eapol_config eapol;
} global_conf;

static struct hostapd_external_app_object test_plugin;

struct hostapd_if_action_policy_entry {
	enum hostapd_if_action_frame_type action_type;
	const char *conf_name;
};

static const struct hostapd_if_action_policy_entry hostapd_if_action_policy_map[] = {
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO,
	  "external_plugin_action_policy_radio" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO_NEIGHBOUR_REQ,
	  "external_plugin_action_policy_radio_neighbour_req" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WNM,
	  "external_plugin_action_policy_wnm" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_QUERY,
	  "external_plugin_action_policy_wnm_btm_query" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_RESP,
	  "external_plugin_action_policy_wnm_btm_resp" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WMM,
	  "external_plugin_action_policy_wmm" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_ADDTS_REQ,
	  "external_plugin_action_policy_wmm_addts_req" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_DELTS,
	  "external_plugin_action_policy_wmm_delts" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_REQ,
	  "external_plugin_action_policy_wnm_dms_req" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_RESP,
	  "external_plugin_action_policy_wnm_dms_resp" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_FT,
	  "external_plugin_action_policy_ft" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_FT_REQ,
	  "external_plugin_action_policy_ft_req" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_FT_RESP,
	  "external_plugin_action_policy_ft_resp" },
	{ HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR,
	  "external_plugin_action_policy_vendor" },
};

static struct dl_list plugin_hapd_iface_list = {
	.next = &plugin_hapd_iface_list,
	.prev = &plugin_hapd_iface_list,
};

/* Defining the deque structure */
struct deque {
    struct invoke_plugin_datablock *datablocks[QUEUE_MAX_SIZE];
    uint32_t front;
    uint32_t rear;
};

static struct deque *datablock_deque;
static pthread_mutex_t datablock_deque_mutex;
static pthread_mutex_t plugin_iface_mutex;

static volatile bool test_harness_thread_running = false;
static volatile bool test_harness_thread_done = false;

struct invoke_plugin_datablock {
	char ifname[IFNAMSIZ + 1];
	uint8_t sta_mac[ETH_ALEN];
	union {
		struct {
			const uint8_t *frame;
			uint16_t frame_len;
			struct hostapd_if_frame_ctx *ctx;
			enum {
				HOSTAPD_IF_EVENT_AUTH_REQ,
				HOSTAPD_IF_EVENT_ASSOC_REQ,
				HOSTAPD_IF_EVENT_M2_NOTIFY,
				HOSTAPD_IF_EVENT_ACTION_REQ,
				HOSTAPD_IF_EVENT_REMOTE_AUTH_REQ,
			} req_type;
		} request;
	} data;
};

/* Storage for association response IEs */
static uint8_t *assoc_resp_ies = NULL;
static size_t assoc_resp_ies_len = 0;

/* Storage for authentication response IEs */
static uint8_t *auth_resp_ies = NULL;
static size_t auth_resp_ies_len = 0;


/* Function to initialize the deque */
void initialize_deque(struct deque* q)
{
	q->front = 0;
	q->rear = 1;
}


/* Function to check if the deque is full */
bool is_full(struct deque* q)
{
	return (q->rear == q->front);
}

int deque_size(struct deque* q)
{
	uint32_t rear;
	if (q->rear > q->front)
		rear = q->rear;
	else
		rear = QUEUE_MAX_SIZE + q->rear;

	return ((rear - q->front) - 1);
}

/* Function to check if the deque is empty */
bool is_empty(struct deque* q)
{
	return (deque_size(q) == 0);
}

/*
 * Function to add an element to the deque
 * at rear (enqueue_rear operation)
 */
int enqueue_rear(struct deque* q, struct invoke_plugin_datablock *datablock)
{
	if (is_full(q)) {
		printf("Deque is full\n");
		return -1;
	}
	q->datablocks[q->rear] = datablock;
	q->rear = (q->rear + 1) % QUEUE_MAX_SIZE;
	return 0;
}

/*
 * Function to remove an element from the deque at rear
 * (dequeue_rear operation)
 */
void dequeue_rear(struct deque* q)
{
	if (is_empty(q)) {
		printf("Deque is empty\n");
		return;
	}
	if (q->rear == 0)
		q->rear = QUEUE_MAX_SIZE - 1;
	else
		q->rear = q->rear - 1;
}

/*
 * Function to remove an element from the deque at front
 * (dequeue_front operation)
 */
void dequeue_front(struct deque* q)
{
	if (is_empty(q)) {
		printf("Deque is empty\n");
		return;
	}
	q->front = (q->front + 1) % QUEUE_MAX_SIZE;
}

/*
 * Function to get the element at the front of the deque
 * (peek_front operation)
 */
struct invoke_plugin_datablock* peek_front(struct deque* q)
{
	if (is_empty(q)) {
		printf("Deque is empty\n");
		/* return some default value or handle error differently */
		return NULL;
	}
	return q->datablocks[(q->front + 1) % QUEUE_MAX_SIZE];
}

/*
 * Function to get the element at the rear of the deque (peek_rear operation)
 */
struct invoke_plugin_datablock* peek_rear(struct deque* q)
{
	uint32_t rear;
	if (is_empty(q)) {
		printf("Deque is empty\n");
		/* return some default value or handle error differently */
		return NULL;
	}

	if (q->rear == 0)
		rear = QUEUE_MAX_SIZE;
	else
		rear = q->rear;

	return q->datablocks[rear - 1];
}


const char *hostapd_if_event_string(enum hostapd_if_event_type type)
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

static void notify_event(void *hapd_ctx, struct hostapd_if_event *event)
{
	struct invoke_plugin_datablock *datablock;

	wpa_printf(MSG_DEBUG,
		   "notifying event of type %s\n ifname = %s\n STA MAC = " MACSTR "\n",
		   hostapd_if_event_string(event->type),
		   event->ifname,  MAC2STR(event->sta_mac));
	switch (event->type) {

		case HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED:
			datablock = os_zalloc(sizeof(*datablock));
			if (!datablock)
				break;
		    	os_strlcpy(datablock->ifname, event->ifname,
				   sizeof(datablock->ifname));
			os_memcpy(datablock->sta_mac, event->sta_mac,
				  sizeof(datablock->sta_mac));
			datablock->data.request.req_type =
				HOSTAPD_IF_EVENT_M2_NOTIFY;
			pthread_mutex_lock(&datablock_deque_mutex);
			if (enqueue_rear(datablock_deque, datablock) < 0) {
				pthread_mutex_unlock(&datablock_deque_mutex);
				os_free(datablock);
				break;
			}
			pthread_mutex_unlock(&datablock_deque_mutex);
			wpa_printf(MSG_DEBUG,
				   "queued m3 trigger for STA " MACSTR "\n",
				   MAC2STR(datablock->sta_mac));
			break;

		case HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE:
			wpa_printf(MSG_DEBUG,
				   "ok = %d\nstatus = %d\n",
				   event->data.assoc_resp_completion.ok,
				   event->data.assoc_resp_completion.status);
			break;

		case HOSTAPD_IF_EVENT_DEAUTH:
		case HOSTAPD_IF_EVENT_DISASSOC:
			wpa_printf(MSG_DEBUG, "link_id = %d",
				   event->data.deauth_disassoc.link_id);
			wpa_printf(MSG_DEBUG, "is_tx_status = %d",
				   event->data.deauth_disassoc.is_tx_status);
			wpa_printf(MSG_DEBUG, "tx_status_ok = %d",
				   event->data.deauth_disassoc.tx_status_ok);
			wpa_printf(MSG_DEBUG, "type = %d",
				   event->data.deauth_disassoc.type);
			wpa_printf(MSG_DEBUG, "reason_code = %d",
				   event->data.deauth_disassoc.reason_code);
			break;

		case HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION:
			wpa_printf(MSG_DEBUG, "status = %d",
				   event->data.sa_query.status);
			break;

		case HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION:
			wpa_printf(MSG_DEBUG, "authorized = %d",
				   event->data.authorize_completion.authorized);
			break;

		default:
			break;
	}
}

/* Northbound (implemented by plugin) callbacks */
static void invoke_assoc(char *ifname, uint8_t *sta_mac, const uint8_t *frame,
			 uint16_t frame_len, struct hostapd_if_frame_ctx *ctx)
{
	/* prepare datablock to push to deque for secondary thread */
	struct invoke_plugin_datablock *datablock = os_zalloc(sizeof(*datablock));
	struct hostapd_if_frame_ctx *ctx_copy;

	ctx_copy = os_zalloc(sizeof(*ctx_copy));
	*ctx_copy = *ctx;
	os_strlcpy(datablock->ifname, ifname, sizeof(datablock->ifname));
	os_memcpy(datablock->sta_mac, sta_mac, sizeof(datablock->sta_mac));
	datablock->data.request.frame = frame;
	datablock->data.request.frame_len = frame_len;
	datablock->data.request.ctx = ctx_copy;
	datablock->data.request.req_type = HOSTAPD_IF_EVENT_ASSOC_REQ;

	/* push datablock to deque */
	pthread_mutex_lock(&datablock_deque_mutex);
	if (enqueue_rear(datablock_deque, datablock) < 0) {
		pthread_mutex_unlock(&datablock_deque_mutex);
		wpa_printf(MSG_ERROR,
			   "datablock queue full, dropping assoc for STA " MACSTR,
			   MAC2STR(datablock->sta_mac));
		os_free(ctx_copy);
		os_free(datablock);
		return;
	}
	wpa_printf(MSG_DEBUG,
		   "queued assoc request for STA " MACSTR "\n",
		   MAC2STR(datablock->sta_mac));
	if (global_conf.assoc.out_of_order &&
	    (deque_size(datablock_deque) <= 1))
		global_conf.assoc.process_frames = false;
	else
		global_conf.assoc.process_frames = true;
	wpa_printf(MSG_DEBUG, "datablock queue has %d elements",
		   deque_size(datablock_deque));
	pthread_mutex_unlock(&datablock_deque_mutex);
	return;
}

static void invoke_auth(char *ifname, uint8_t *sta_mac, const uint8_t *frame,
			uint16_t frame_len, struct hostapd_if_frame_ctx *ctx)
{
	/* prepare datablock to push to deque for secondary thread */
	struct invoke_plugin_datablock *datablock;
	struct hostapd_if_frame_ctx *ctx_copy;

	datablock = os_zalloc(sizeof(*datablock));
	ctx_copy = os_zalloc(sizeof(*ctx_copy));
	*ctx_copy = *ctx;

	os_strlcpy(datablock->ifname, ifname, sizeof(datablock->ifname));
	os_memcpy(datablock->sta_mac, sta_mac, sizeof(datablock->sta_mac));
	datablock->data.request.frame = frame;
	datablock->data.request.frame_len = frame_len;
	datablock->data.request.ctx = ctx_copy;
	datablock->data.request.req_type = HOSTAPD_IF_EVENT_AUTH_REQ;

	/* push datablock to deque */
	pthread_mutex_lock(&datablock_deque_mutex);
	if (enqueue_rear(datablock_deque, datablock) < 0) {
		pthread_mutex_unlock(&datablock_deque_mutex);
		wpa_printf(MSG_ERROR,
			   "datablock queue full, dropping auth for STA " MACSTR,
			   MAC2STR(datablock->sta_mac));
		os_free(ctx_copy);
		os_free(datablock);
		return;
	}
	wpa_printf(MSG_DEBUG, "queued auth request for STA " MACSTR "\n",
		   MAC2STR(datablock->sta_mac));
	if (global_conf.auth.out_of_order && (deque_size(datablock_deque) <= 1))
		global_conf.auth.process_frames = false;
	else
		global_conf.auth.process_frames = true;
	wpa_printf(MSG_DEBUG, "datablock queue has %d elements",
		   deque_size(datablock_deque));
	pthread_mutex_unlock(&datablock_deque_mutex);
	return;
}

static void invoke_remote_auth(char *ifname, uint8_t *sta_mac,
			       const uint8_t *ies, uint16_t ies_len,
			       struct hostapd_if_frame_ctx *ctx)
{
	/* prepare datablock to push to deque for secondary thread */
	struct invoke_plugin_datablock *datablock;
	struct hostapd_if_frame_ctx *ctx_copy;

	datablock = os_zalloc(sizeof(*datablock));
	ctx_copy = os_zalloc(sizeof(*ctx_copy));
	*ctx_copy = *ctx;

	os_strlcpy(datablock->ifname, ifname, sizeof(datablock->ifname));
	os_memcpy(datablock->sta_mac, sta_mac, sizeof(datablock->sta_mac));
	datablock->data.request.frame = ies;
	datablock->data.request.frame_len = ies_len;
	datablock->data.request.ctx = ctx_copy;
	datablock->data.request.req_type = HOSTAPD_IF_EVENT_REMOTE_AUTH_REQ;

	/* push datablock to deque */
	pthread_mutex_lock(&datablock_deque_mutex);
	if (enqueue_rear(datablock_deque, datablock) < 0) {
		pthread_mutex_unlock(&datablock_deque_mutex);
		wpa_printf(MSG_ERROR,
			   "datablock queue full, dropping remote auth for STA " MACSTR,
			   MAC2STR(datablock->sta_mac));
		os_free(ctx_copy);
		os_free(datablock);
		return;
	}
	pthread_mutex_unlock(&datablock_deque_mutex);
	wpa_printf(MSG_DEBUG, "queued remote auth request for STA " MACSTR "\n",
		   MAC2STR(datablock->sta_mac));
	return;
}

static void notify_assoc(void *hapd_ctx, char *ifname, uint8_t *sta_mac, const uint8_t *frame,
			 uint16_t frame_len, struct hostapd_if_frame_ctx *ctx)
{
	uint16_t notify_status_code;
	wpa_printf(MSG_DEBUG,
		   "Notified Assoc for STA " MACSTR " on link_id=%d, frame_len=%u\n",
		   MAC2STR(sta_mac), ctx->rx_link_id,
		   frame_len);
	/*
	 * Parse status code from assoc response frame if present
	 * Management frame format: 24-byte header + frame body
	 * Assoc response body: 2-byte capability + 2-byte status +
	 * 2-byte AID + IEs. Status code is at offset 26-27
	 * (24-byte header + 2-byte capability)
	 */
	if (frame_len >= 28) {
		notify_status_code = frame[26] | (frame[27] << 8);
		/*
		 * Use configured notification status code if set,
		 * otherwise use actual status code
		 */
		wpa_printf(MSG_DEBUG, "Assoc notification status_code=%u (%s)\n",
			   notify_status_code, notify_status_code == 0 ?
			   "SUCCESS" : "REJECTED");
	}
}

static void notify_action(char *ifname, const uint8_t *sta_mac, const uint8_t *frame,
			  uint16_t frame_len, int link_id, struct hostapd_if_frame_ctx *ctx)
{
	int category = 0;
	int action_code = 0;
	int rssi = 0;

	if (ctx) {
		category = ctx->data.action.category;
		action_code = ctx->data.action.action_code;
		rssi = ctx->data.action.rssi;
	}

	wpa_printf(MSG_DEBUG,
		   "Notified Action for STA " MACSTR " on link_id=%d, frame_len=%u%s category=%d action_code=%d RSSI %d\n",
		   MAC2STR(sta_mac), link_id, frame_len, ctx ? "" : " Frame ctx null",
		   category, action_code, rssi);
}

static void offload_action(char *ifname, const uint8_t *sta_mac, const uint8_t *frame,
			   uint16_t frame_len, uint8_t link_id, struct hostapd_if_frame_ctx *ctx)
{
	int category = 0;
	int action_code = 0;
	int rssi = 0;

	if (ctx) {
		category = ctx->data.action.category;
		action_code = ctx->data.action.action_code;
		rssi = ctx->data.action.rssi;
	}

	wpa_printf(MSG_DEBUG,
		   "Offload Action for STA " MACSTR " on link_id=%d, frame_len=%u%s category=%d action_code=%d RSSI %d\n",
		   MAC2STR(sta_mac), link_id, frame_len, ctx ? "" : " Frame ctx null",
		   category, action_code, rssi);
}

static void notify_auth(void *hapd_ctx, char *ifname, uint8_t *sta_mac, const uint8_t *frame,
			uint16_t frame_len, struct hostapd_if_frame_ctx *ctx)
{
	uint16_t auth_alg = 0;
	uint16_t auth_transaction = 0;
	uint16_t notify_status_code = 0;
	int rx_link_id = -1;
	int rssi = 0;

	if (ctx) {
		rx_link_id = ctx->rx_link_id;
		rssi = ctx->data.auth_req.rssi;
	}

	wpa_printf(MSG_DEBUG,
		   "Notified Auth for STA " MACSTR " on link_id=%d, frame_len=%u%s RSSI %d\n",
		   MAC2STR(sta_mac), rx_link_id, frame_len,
		   ctx ? "" : " Frame ctx null", rssi);

	/*
	 * Parse status code from auth response frame if present
	 * Management frame format: 24-byte header + frame body
	 * Auth frame body: 2-byte auth_alg + 2-byte auth_transaction +
	 * 2-byte status + challenge. Status code is at offset 28-29
	 * (24-byte header + 2-byte alg + 2-byte transaction)
	 */
	if (frame_len >= 30) {
		auth_alg = frame[24] | (frame[25] << 8);
		auth_transaction = frame[26] | (frame[27] << 8);
		notify_status_code = frame[28] | (frame[29] << 8);
		wpa_printf(MSG_DEBUG,
			   "Auth notification auth_alg=%u, auth_transaction=%u, status_code=%u (%s)\n",
			   auth_alg, auth_transaction,
			   notify_status_code,
			   notify_status_code == 0 ? "SUCCESS" : "REJECTED");
	}
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

static void notify_deauth(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
			  const void *frame, size_t frame_len,
			  struct hostapd_if_frame_ctx *ctx)
{
	uint16_t reason = 0xFFFF;
	const char *dir = "UNKNOWN";
	int rx_link_id = -1;
	const uint8_t *f;

	if (ctx) {
		rx_link_id = ctx->rx_link_id;
	}
	if (frame && frame_len >= 26) {
		/* Deauth body: 2-byte reason at offset 24 after 24B header */
		f = (const uint8_t *) frame;
		reason = (uint16_t)(f[24] | (f[25] << 8));
	}

	wpa_printf(MSG_DEBUG,
		   "Notified Deauth (%s) for STA " MACSTR " link_id=%d reason=%u frame_len=%zu\n",
		   dir, MAC2STR(sta_mac), rx_link_id,
		   reason, frame_len);
}

static void notify_disassoc(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
			    const void *frame, size_t frame_len,
			    struct hostapd_if_frame_ctx *ctx)
{
	uint16_t reason = 0xFFFF;
	const char *dir = "UNKNOWN";
	int rx_link_id = -1;
	const uint8_t *f;

	if (ctx) {
		rx_link_id = ctx->rx_link_id;
	}
	if (frame && frame_len >= 26) {
		/* Disassoc body: 2-byte reason at offset 24 after 24B header */
		f = (const uint8_t *) frame;
		reason = (uint16_t)(f[24] | (f[25] << 8));
	}

	wpa_printf(MSG_DEBUG,
		   "Notified Disassoc (%s) for STA " MACSTR " link_id=%d reason=%u frame_len=%zu\n",
		   dir, MAC2STR(sta_mac), rx_link_id,
		   reason, frame_len);
}

struct wpa_ft_pmk_r1_sa {
        struct dl_list list;
        u8 pmk_r1[PMK_LEN_MAX];
        size_t pmk_r1_len;
        u8 pmk_r1_name[WPA_PMK_NAME_LEN];
        u8 spa[ETH_ALEN];
        int pairwise; /* Pairwise cipher suite, WPA_CIPHER_* */
        struct vlan_description *vlan;
        u8 *identity;
        size_t identity_len;
        u8 *radius_cui;
        size_t radius_cui_len;
        os_time_t session_timeout; /* 0 for no expiration */
        /* TODO: radius_class, EAP type */
};

struct wpa_ft_pmk_cache {
        struct dl_list pmk_r1; /* struct wpa_ft_pmk_r1_sa */
        unsigned int ref_count;
};


struct plugin_hapd_iface
{
	struct dl_list list;
	char ifname[IFNAMSIZ+1];
	struct hostapd_data *hapd;
	struct wpa_ft_pmk_cache ft_pmk_cache;
};

static struct plugin_hapd_iface *plugin_hapd_iface_get(const char *ifname)
{
	struct plugin_hapd_iface *iface;

	dl_list_for_each(iface, &plugin_hapd_iface_list,
			 struct plugin_hapd_iface, list) {
		if (os_strcmp(iface->ifname, ifname) == 0)
			return iface;
	}
	return NULL;
}

static int pull_pmk_r1(char *ifname, uint8_t *sta_mac,
		       uint8_t pmk_r1_name[WPA_PMK_NAME_LEN],
		       uint8_t pmk_r1[PMK_LEN_MAX], size_t *pmk_r1_len,
		       int *pairwise, int *session_timeout,
		       uint8_t identity[MAX_RADIUS_CUI_LEN],
		       size_t *identity_len,
		       uint8_t radius_cui[MAX_RADIUS_CUI_LEN],
		       size_t *radius_cui_len)
{
	struct plugin_hapd_iface *iface;
	struct wpa_ft_pmk_r1_sa *r1;
	struct os_reltime now;

	if (!ifname || !sta_mac || !pmk_r1_name)
		return -1;

	pthread_mutex_lock(&plugin_iface_mutex);

	iface = plugin_hapd_iface_get(ifname);
	if (!iface) {
		wpa_printf(MSG_DEBUG,
			   "plugin: pull_pmk_r1: no iface found for %s\n",
			   ifname);
		pthread_mutex_unlock(&plugin_iface_mutex);
		return -1;
	}

	os_get_reltime(&now);

	dl_list_for_each(r1, &iface->ft_pmk_cache.pmk_r1,
			 struct wpa_ft_pmk_r1_sa, list) {
		if (os_memcmp(r1->spa, sta_mac, ETH_ALEN) != 0)
			continue;
		if (os_memcmp(r1->pmk_r1_name, pmk_r1_name,
			      WPA_PMK_NAME_LEN) != 0)
			continue;

		/* Skip expired entries */
		if (r1->session_timeout > 0 &&
		    r1->session_timeout < now.sec) {
			wpa_printf(MSG_DEBUG,
				   "plugin: pull_pmk_r1: entry expired for "
				   MACSTR "\n", MAC2STR(sta_mac));
			continue;
		}

		os_memcpy(pmk_r1, r1->pmk_r1, r1->pmk_r1_len);
		*pmk_r1_len = r1->pmk_r1_len;
		*pairwise = r1->pairwise;

		if (r1->session_timeout > 0)
			*session_timeout = (int)(r1->session_timeout - now.sec);
		else
			*session_timeout = 0;

		if (identity && identity_len) {
			if (r1->identity && r1->identity_len > 0) {
				size_t copy_len = r1->identity_len;

				if (copy_len > MAX_RADIUS_CUI_LEN)
					copy_len = MAX_RADIUS_CUI_LEN;
				os_memcpy(identity, r1->identity, copy_len);
				*identity_len = copy_len;
			} else {
				*identity_len = 0;
			}
		}

		if (radius_cui && radius_cui_len) {
			if (r1->radius_cui && r1->radius_cui_len > 0) {
				size_t copy_len = r1->radius_cui_len;

				if (copy_len > MAX_RADIUS_CUI_LEN)
					copy_len = MAX_RADIUS_CUI_LEN;
				os_memcpy(radius_cui, r1->radius_cui, copy_len);
				*radius_cui_len = copy_len;
			} else {
				*radius_cui_len = 0;
			}
		}

		wpa_printf(MSG_DEBUG,
			   "plugin: pull_pmk_r1: found entry for " MACSTR
			   " on iface %s\n", MAC2STR(sta_mac), ifname);
		pthread_mutex_unlock(&plugin_iface_mutex);
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "plugin: pull_pmk_r1: no entry found for " MACSTR
		   " on iface %s\n", MAC2STR(sta_mac), ifname);
	pthread_mutex_unlock(&plugin_iface_mutex);
	return -1;
}

static int interface_create(char *ifname, void *ctx)
{
	struct plugin_hapd_iface *iface;


	size_t i;
	struct hostapd_data *hapd;
	struct hostapd_if_frame_category cat;
	enum hostapd_if_frame_policy auth_policy, deauth_policy,
				     disassoc_policy, assoc_policy,
				     action_policy, remote_auth_policy;

	hapd = ctx;
	if (!ifname)
		return -1;

	if (!test_plugin.register_frame)
		return -1;

	pthread_mutex_lock(&plugin_iface_mutex);

	iface = plugin_hapd_iface_get(ifname);
	if (!iface) {
		iface = os_zalloc(sizeof(*iface));
		if (!iface) {
			wpa_printf(MSG_ERROR,
				   "plugin: failed to alloc iface for %s\n",
				   ifname);
			pthread_mutex_unlock(&plugin_iface_mutex);
			return -1;
		}
		os_strlcpy(iface->ifname, ifname, sizeof(iface->ifname));
		iface->hapd = hapd;
		dl_list_init(&iface->ft_pmk_cache.pmk_r1);
		dl_list_add_tail(&plugin_hapd_iface_list, &iface->list);
		wpa_printf(MSG_DEBUG,
			   "plugin: created plugin_hapd_iface for %s\n",
			   ifname);
	} else {
		wpa_printf(MSG_DEBUG,
			   "plugin: plugin_hapd_iface for %s already exists\n",
			   ifname);
	}


	auth_policy = hapd->conf->plugin.external_plugin_auth_policy;
	remote_auth_policy = hapd->conf->plugin.external_plugin_remote_auth_policy;
	deauth_policy = hapd->conf->plugin.external_plugin_deauth_policy;
	disassoc_policy = hapd->conf->plugin.external_plugin_disassoc_policy;
	assoc_policy = hapd->conf->plugin.external_plugin_assoc_policy;

	if (((uint32_t)auth_policy) > HOSTAPD_IF_FRAME_INVOKE) {
		wpa_printf(MSG_ERROR, "ERROR!! auth_policy error %d\n",
				auth_policy);
	} else {
		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_AUTH;
		test_plugin.register_frame(ctx, &cat, auth_policy);
		wpa_printf(MSG_DEBUG, "registered AUTH policy %d\n",
			   auth_policy);
	}

	if (((uint32_t)deauth_policy) > HOSTAPD_IF_FRAME_NOTIFY) {
		wpa_printf(MSG_ERROR, "ERROR!! deauth_policy error %d\n",
			   deauth_policy);
	} else {
		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_DEAUTH;
		test_plugin.register_frame(ctx, &cat, deauth_policy);
		wpa_printf(MSG_DEBUG, "registered DEAUTH policy %d\n",
			   deauth_policy);
	}

	if (((uint32_t)assoc_policy) > HOSTAPD_IF_FRAME_INVOKE) {
		wpa_printf(MSG_ERROR, "ERROR!! assoc_policy error %d\n",
			   assoc_policy);
	} else {
		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_ASSOC;
		test_plugin.register_frame(ctx, &cat, assoc_policy);
		wpa_printf(MSG_DEBUG, "registered ASSOC policy %d\n",
			   assoc_policy);
	}

	if (((uint32_t)disassoc_policy) > HOSTAPD_IF_FRAME_NOTIFY) {
		wpa_printf(MSG_ERROR, "ERROR!! disassoc_policy error %d\n",
			   disassoc_policy);
	} else {
		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_DISASSOC;
		test_plugin.register_frame(ctx, &cat, disassoc_policy);
		wpa_printf(MSG_DEBUG, "registered DISASSOC policy %d\n",
			   disassoc_policy);
	}

	if (((uint32_t)remote_auth_policy) > HOSTAPD_IF_FRAME_INVOKE) {
		wpa_printf(MSG_ERROR, "ERROR!! remote_auth_policy error %d\n",
			   remote_auth_policy);
	} else {
		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_REMOTE_AUTH;
		test_plugin.register_frame(ctx, &cat, remote_auth_policy);
		wpa_printf(MSG_ERROR, "registered REMOTE-AUTH policy %d\n",
			   remote_auth_policy);
	}

	for (i = 0; i < ARRAY_SIZE(hostapd_if_action_policy_map); i++) {

		action_policy = hapd->conf->plugin.external_plugin_action_policy[
				hostapd_if_action_policy_map[i].action_type];
		if (action_policy > HOSTAPD_IF_FRAME_OFFLOAD ||
		    action_policy == HOSTAPD_IF_FRAME_INVOKE) {
			wpa_printf(MSG_ERROR, "%s: invalid action policy %d "
				   "(allowed: DO_NOTHING, NOTIFY, OFFLOAD)",
				    hostapd_if_action_policy_map[i].conf_name,
				    action_policy);
			continue;
		}

		if (action_policy == HOSTAPD_IF_FRAME_DO_NOTHING)
			continue;

		memset(&cat, 0, sizeof(cat));
		cat.type = HOSTAPD_IF_FRAME_TYPE_ACTION;
		cat.u.action_type = hostapd_if_action_policy_map[i].action_type;
		test_plugin.register_frame(ctx, &cat, action_policy);
		wpa_printf(MSG_DEBUG, "registered ACTION %s policy %d\n",
			   hostapd_if_action_policy_map[i].conf_name,
			   action_policy);
	}

	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_DEAUTH, true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_DISASSOC, true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_GTK_COMPLETION,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED,
				     true);
	test_plugin.register_event(ctx, HOSTAPD_IF_EVENT_ACTION_COMPLETION,
				     true);

	pthread_mutex_unlock(&plugin_iface_mutex);
	return 0;
}

static void interface_remove(char *ifname, void *ctx)
{
	struct plugin_hapd_iface *iface;
	struct hostapd_data *hapd;

	hapd = ctx;

	if (!ifname)
		return;

	pthread_mutex_lock(&plugin_iface_mutex);

	iface = plugin_hapd_iface_get(ifname);
	wpa_printf(MSG_DEBUG,
		   "plugin: removed plugin_hapd_iface for %s %d\n", ifname,
		   hapd->iface->freq);

	if (!iface) {
		wpa_printf(MSG_DEBUG,
			   "plugin: interface_remove: no iface found for %s\n",
			   ifname);
		pthread_mutex_unlock(&plugin_iface_mutex);
		return;
	}

	dl_list_del(&iface->list);
	os_free(iface);

	pthread_mutex_unlock(&plugin_iface_mutex);
}

static
void process_assoc_request(struct invoke_plugin_datablock *datablock,
			   struct hostapd_if_frame_ctx *ctx,
			   struct hostapd_if_frame_ctx *resp_ctx)
{
	/*
	 * assign fields one-by-one to prevent silent errors
	 * elsewhere if struct definition changes
	 */
	resp_ctx->rx_link_id = ctx->rx_link_id;
	resp_ctx->status_code = ctx->status_code;
	resp_ctx->data.assoc_resp.is_reassoc =
		ctx->data.assoc_req.is_reassoc;
	os_memcpy(resp_ctx->data.assoc_resp.sta_assoc_link_mac,
			ctx->data.assoc_req.sta_assoc_link_mac,
			sizeof(resp_ctx->data.assoc_resp.sta_assoc_link_mac));

	/* Set assoc response frame IEs if configured */
	if (global_conf.assoc.delay > 0) {
		usleep(global_conf.assoc.delay * 1000);
	}

	/*
	 * Use configured status code if set,
	 * otherwise use the original status code from request
	 */
	if (global_conf.assoc.status_code != -1) {
		wpa_printf(MSG_DEBUG,
			   "overwrote assoc response status code with %d for STA " MACSTR "\n",
			   global_conf.assoc.status_code,
			   MAC2STR(datablock->sta_mac));
		resp_ctx->status_code = global_conf.assoc.status_code;
	}

	/* Add stored association response IEs if available */
	if (assoc_resp_ies && assoc_resp_ies_len > 0) {

		resp_ctx->data.assoc_resp.additional_ies =
			os_memdup(assoc_resp_ies, assoc_resp_ies_len);
		resp_ctx->data.assoc_resp.additional_ies_len =
			assoc_resp_ies_len;
		wpa_printf(MSG_DEBUG,
			   "added %zu bytes of additional IEs to assoc response for STA " MACSTR "\n",
			   assoc_resp_ies_len,
			   MAC2STR(datablock->sta_mac));
	}
	if (global_conf.assoc.send_response) {
		wpa_printf(MSG_DEBUG,
			   "sending assoc response for STA " MACSTR " with status_code=%d (%s)\n",
			   MAC2STR(datablock->sta_mac),
			   resp_ctx->status_code,
			   resp_ctx->status_code == 0 ? "SUCCESS" : "REJECTED");
		test_plugin.assoc_response(datablock->ifname,
					     datablock->sta_mac, resp_ctx);
	}
}

/*
 * process_ft_pmk_r1 - Given raw IEs, derive PMK-R1-Name from the PMKID
 * (PMK-R0-Name) using wpa_derive_pmk_r1_name, look up the matching PMK-R1
 * in the plugin cache, and return an allocated hostapd_if_pmk_r1 on success
 * (caller must assign and eventually free), or NULL if not found.
 *
 * Used by both FT-over-Air (process_auth_request) and FT-over-DS
 * (process_remote_auth_request).  The caller is responsible for extracting
 * the IEs from the frame (if needed) and for writing the returned pointer
 * into the appropriate resp_ctx field.
 */
static struct hostapd_if_pmk_r1 *
process_ft_pmk_r1(const char *ifname, const u8 *sta_mac,
		  const u8 *ies, size_t ies_len)
{
	const u8 *rsn_ie, *pmk_r0_name;
	size_t pmk_len;
	struct wpa_ie_data ie_data;
	struct plugin_hapd_iface *iface;
	struct wpa_ft_pmk_r1_sa *r1 = NULL, *tmp;
	struct hostapd_if_pmk_r1 *pmk_r1_info;
	u8 pmk_r1_name[WPA_PMK_NAME_LEN];
	const u8 *r1kh_id;

	rsn_ie = get_ie(ies, ies_len, WLAN_EID_RSN);
	if (!rsn_ie)
		return NULL;

	if (wpa_parse_wpa_ie_rsn(rsn_ie, 2 + rsn_ie[1], &ie_data) != 0)
		return NULL;

	if (!ie_data.num_pmkid || !ie_data.pmkid)
		return NULL;

	/* ie_data.pmkid is the PMK-R0-Name, not the PMK-R1-Name */
	pmk_r0_name = ie_data.pmkid;
	wpa_printf(MSG_DEBUG, "FT: RSN IE contains PMKID (PMK-R0-Name) for STA "
		   MACSTR, MAC2STR(sta_mac));

	pthread_mutex_lock(&plugin_iface_mutex);

	iface = plugin_hapd_iface_get(ifname);
	if (!iface || !iface->hapd || !iface->hapd->wpa_auth) {
		pthread_mutex_unlock(&plugin_iface_mutex);
		return NULL;
	}

	r1kh_id = iface->hapd->wpa_auth->conf.r1_key_holder;

	/*
	 * Derive PMK-R1-Name from PMK-R0-Name, R1KH-ID and STA MAC.
	 * Try all possible PMK lengths (SHA-256/384/512) as done in
	 * wpa_ft_process_auth_req.
	 */
	for (pmk_len = SHA256_MAC_LEN;
	     pmk_len <= SHA512_MAC_LEN;
	     pmk_len += 16) {
		if (wpa_derive_pmk_r1_name(pmk_r0_name, r1kh_id,
					   sta_mac,
					   pmk_r1_name, pmk_len) < 0)
			continue;

		dl_list_for_each(tmp, &iface->ft_pmk_cache.pmk_r1,
				 struct wpa_ft_pmk_r1_sa, list) {
			if (os_memcmp(tmp->pmk_r1_name, pmk_r1_name,
				      WPA_PMK_NAME_LEN) == 0) {
				r1 = tmp;
				break;
			}
		}

		if (r1)
			break;
	}

	if (!r1) {
		pthread_mutex_unlock(&plugin_iface_mutex);
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "FT: found PMK-R1 in plugin cache for STA " MACSTR,
		   MAC2STR(sta_mac));

	pmk_r1_info = os_zalloc(sizeof(*pmk_r1_info));
	if (!pmk_r1_info) {
		pthread_mutex_unlock(&plugin_iface_mutex);
		return NULL;
	}

	os_memcpy(pmk_r1_info->pmk_r1, r1->pmk_r1, r1->pmk_r1_len);
	pmk_r1_info->pmk_r1_len = r1->pmk_r1_len;
	os_memcpy(pmk_r1_info->pmk_r1_name, r1->pmk_r1_name, PMK_R1_NAME_LEN);
	pmk_r1_info->pairwise = r1->pairwise;

	if (r1->identity && r1->identity_len > 0) {
		size_t len = MIN(r1->identity_len, MAX_IDENTITY_LEN);

		os_memcpy(pmk_r1_info->identity, r1->identity, len);
		pmk_r1_info->identity_len = len;
	}

	if (r1->radius_cui && r1->radius_cui_len > 0) {
		size_t len = MIN(r1->radius_cui_len, MAX_RADIUS_CUI_LEN);

		os_memcpy(pmk_r1_info->radius_cui, r1->radius_cui, len);
		pmk_r1_info->radius_cui_len = len;
	}

	pthread_mutex_unlock(&plugin_iface_mutex);
	return pmk_r1_info;
}


static
void process_auth_request(struct invoke_plugin_datablock *datablock,
			  struct hostapd_if_frame_ctx *ctx,
			  struct hostapd_if_frame_ctx *resp_ctx)
{
	memset(resp_ctx, 0, sizeof(*resp_ctx));
	/*
	 * assign fields one-by-one to prevent silent errors
	 * elsewhere if struct definition changes
	 */
	resp_ctx->rx_link_id = ctx->rx_link_id;
	resp_ctx->status_code = ctx->status_code;
	resp_ctx->data.auth_resp.allow_reuse =
		ctx->data.auth_req.allow_reuse;
	resp_ctx->data.auth_resp.auth_transaction =
		ctx->data.auth_req.auth_transaction;
	resp_ctx->data.auth_resp.auth_alg =
		ctx->data.auth_req.auth_alg;
	os_memcpy(resp_ctx->data.auth_resp.sta_assoc_link_mac,
			ctx->data.auth_req.sta_assoc_link_mac,
			sizeof(resp_ctx->data.auth_resp.sta_assoc_link_mac));

	if (global_conf.auth.delay > 0) {
		usleep(global_conf.auth.delay * 1000);
	}
	if (global_conf.auth.status_code != -1) {
		wpa_printf(MSG_DEBUG,
			   "overwrote auth response status code with %d for STA " MACSTR "\n",
			   global_conf.auth.status_code,
			   MAC2STR(datablock->sta_mac));
		resp_ctx->status_code = global_conf.auth.status_code;
	}

	/* Add stored authentication response IEs if available */
	if (auth_resp_ies && auth_resp_ies_len > 0) {
		resp_ctx->data.auth_resp.additional_ies =
			os_memdup(auth_resp_ies, auth_resp_ies_len);
		resp_ctx->data.auth_resp.additional_ies_len =
			auth_resp_ies_len;
		wpa_printf(MSG_DEBUG,
			   "added %zu bytes of additional IEs to auth response for STA " MACSTR "\n",
			   auth_resp_ies_len,
			   MAC2STR(datablock->sta_mac));
	}

	/* FT-over-Air: extract IEs from the 802.11 auth frame and look up
	 * PMK-R1 in the plugin cache; write result into auth_resp. */
	if (ctx->data.auth_req.auth_alg == WLAN_AUTH_FT &&
	    datablock->data.request.frame &&
	    datablock->data.request.frame_len >
	    offsetof(struct ieee80211_mgmt, u.auth.variable)) {
		const struct ieee80211_mgmt *mgmt =
			(const struct ieee80211_mgmt *)
			datablock->data.request.frame;
		const u8 *ies = mgmt->u.auth.variable;
		size_t ies_len = datablock->data.request.frame_len -
			offsetof(struct ieee80211_mgmt, u.auth.variable);

		resp_ctx->data.auth_resp.pmk_r1 =
			process_ft_pmk_r1(datablock->ifname,
					   datablock->sta_mac,
					   ies, ies_len);
	}

	if (global_conf.auth.send_response) {
		wpa_printf(MSG_DEBUG,
			   "sending auth response for STA " MACSTR " with status_code=%d (%s)\n",
			   MAC2STR(datablock->sta_mac),
			   resp_ctx->status_code,
			   resp_ctx->status_code == 0 ? "SUCCESS" : "REJECTED");
		test_plugin.auth_response(datablock->ifname,
					    datablock->sta_mac, resp_ctx);
	}
}

static void
process_remote_auth_request(struct invoke_plugin_datablock *datablock,
			     struct hostapd_if_frame_ctx *ctx,
			     struct hostapd_if_frame_ctx *resp_ctx)
{
	memset(resp_ctx, 0, sizeof(*resp_ctx));
	resp_ctx->rx_link_id = ctx->rx_link_id;
	resp_ctx->status_code = ctx->status_code;
	resp_ctx->data.remote_auth_resp.is_ml_sta =
		ctx->data.remote_auth_req.is_ml_sta;

	/* FT-over-DS: IEs are passed directly (no 802.11 frame header).
	 * Reuse process_ft_pmk_r1 and write result into remote_auth_resp. */
	if (datablock->data.request.frame &&
	    datablock->data.request.frame_len > 0) {
		resp_ctx->data.remote_auth_resp.pmk_r1 =
			process_ft_pmk_r1(datablock->ifname,
					   datablock->sta_mac,
					   datablock->data.request.frame,
					   datablock->data.request.frame_len);
	}

	wpa_printf(MSG_DEBUG,
		   "sending remote auth response for STA " MACSTR "\n",
		   MAC2STR(datablock->sta_mac));
	test_plugin.remote_auth_response(datablock->ifname,
					 datablock->sta_mac, resp_ctx);
}

static
void process_request(struct invoke_plugin_datablock *datablock,
		     struct hostapd_if_frame_ctx *ctx,
		     struct hostapd_if_frame_ctx *resp_ctx)
{
	switch(datablock->data.request.req_type) {

		case HOSTAPD_IF_EVENT_ASSOC_REQ:
			process_assoc_request(datablock, ctx, resp_ctx);
			break;

		case HOSTAPD_IF_EVENT_AUTH_REQ:
			process_auth_request(datablock, ctx, resp_ctx);
			break;

		case HOSTAPD_IF_EVENT_M2_NOTIFY:
			wpa_printf(MSG_DEBUG,
				   "processing m3 trigger for STA " MACSTR "\n",
				   MAC2STR(datablock->sta_mac));
			test_plugin.trigger_eapol_m3(datablock->ifname,
						     datablock->sta_mac);
			break;

		case HOSTAPD_IF_EVENT_REMOTE_AUTH_REQ:
			process_remote_auth_request(datablock, ctx, resp_ctx);
			break;

		default:
			break;
	}
}

void *invoke_loop()
{
	struct invoke_plugin_datablock *datablock;
	struct hostapd_if_frame_ctx *ctx;
	struct hostapd_if_frame_ctx *resp_ctx;
	bool should_process = true;
	bool is_out_of_order_case = false;

	wpa_printf(MSG_DEBUG, "Started pthread loop for Hostapd Plugin");

	while(test_harness_thread_running) {

		usleep(4000);
		pthread_mutex_lock(&datablock_deque_mutex);
		if ((is_empty(datablock_deque))) {
			pthread_mutex_unlock(&datablock_deque_mutex);
			continue;
		}
		/*
		 * Determine if we're in an out-of-order case for the
		 * current frame at the front
		 */
		is_out_of_order_case = false;

		/*
		 * First peek at the front to determine
		 * the frame type
		 */
		datablock = peek_front(datablock_deque);

		if ((datablock->data.request.req_type ==
		     HOSTAPD_IF_EVENT_AUTH_REQ) &&
		    (global_conf.auth.out_of_order > 0)) {

			is_out_of_order_case = true;
		} else if ((datablock->data.request.req_type ==
			   HOSTAPD_IF_EVENT_ASSOC_REQ) &&
			   (global_conf.assoc.out_of_order > 0)) {

			is_out_of_order_case = true;
		}

		/*
		 * Get the frame from the appropriate end of the queue
		 * based on whether we're in an out-of-order case
		 */
		if (is_out_of_order_case) {
			datablock = peek_rear(datablock_deque);
			wpa_printf(MSG_DEBUG,
				   "pulled datablock from REAR in invoke_loop of type %d for STA " MACSTR "\n",
				   datablock->data.request.req_type,
				   MAC2STR(datablock->sta_mac));
		} else {
			datablock = peek_front(datablock_deque);
			wpa_printf(MSG_DEBUG,
				   "pulled datablock from FRONT in invoke_loop of type %d for STA " MACSTR "\n",
				   datablock->data.request.req_type,
				   MAC2STR(datablock->sta_mac));
		}

		/*
		 * Check if we should process this frame type
		 * based on configuration
		 */
		should_process = true;

		if ((datablock->data.request.req_type ==
		     HOSTAPD_IF_EVENT_AUTH_REQ) &&
		    (!global_conf.auth.process_frames)) {

			wpa_printf(MSG_DEBUG,
				   "skipping auth frame processing - waiting for more frames (current: %d, needed: %d)",
				   deque_size(datablock_deque),
				   global_conf.auth.out_of_order);
			should_process = false;
		} else if ((datablock->data.request.req_type ==
			    HOSTAPD_IF_EVENT_ASSOC_REQ) &&
			   (!global_conf.assoc.process_frames)) {

			wpa_printf(MSG_DEBUG,
				   "skipping assoc frame processing - waiting for more frames (current: %d, needed: %d)",
				   deque_size(datablock_deque),
				   global_conf.assoc.out_of_order);
			should_process = false;
		}

		/*
		 * If we shouldn't process this frame yet,
		 * just skip to the next iteration without modifying the queue
		 */
		if (!should_process) {
			pthread_mutex_unlock(&datablock_deque_mutex);
			continue;
		}

		/* Remove the frame from the appropriate end of the queue */
		if (is_out_of_order_case) {
			dequeue_rear(datablock_deque);
			wpa_printf(MSG_DEBUG,
				   "processing frame from REAR (out-of-order case)");
		} else {
			dequeue_front(datablock_deque);
			wpa_printf(MSG_DEBUG,
				   "processing frame from FRONT (normal case)");
		}
		pthread_mutex_unlock(&datablock_deque_mutex);

		switch (datablock->data.request.req_type) {
		case HOSTAPD_IF_EVENT_ASSOC_REQ:
		case HOSTAPD_IF_EVENT_AUTH_REQ:
		case HOSTAPD_IF_EVENT_REMOTE_AUTH_REQ:
			resp_ctx = os_zalloc(sizeof(*resp_ctx));
			ctx = datablock->data.request.ctx;
			process_request(datablock, ctx, resp_ctx);
			os_free(ctx);
			break;
		case HOSTAPD_IF_EVENT_M2_NOTIFY:
			process_request(datablock, NULL, NULL);
			break;
		default:
			break;
		}
		os_free(datablock);
	}
	test_harness_thread_done = true;
	return NULL;
}

void hostapd_if_plugin_deinit()
{
	test_harness_thread_running = false;
	while (!test_harness_thread_done)
		usleep(1000);

	/* Destroy the mutex for datablock_deque */
	pthread_mutex_destroy(&datablock_deque_mutex);
	pthread_mutex_destroy(&plugin_iface_mutex);
}

static pthread_t invoke_thread;
/* Constructor: called when the shared library is loaded */
void hostapd_if_plugin_init(void *arg)
{
	global_conf.assoc.status_code = -1;
	global_conf.auth.status_code = -1;
	global_conf.auth.send_response = 1;
	global_conf.assoc.send_response = 1;

	/* Initialize the mutex for datablock_deque */
	pthread_mutex_init(&datablock_deque_mutex, NULL);
	pthread_mutex_init(&plugin_iface_mutex, NULL);

	datablock_deque = os_zalloc(sizeof(*datablock_deque));
	test_harness_thread_running = true;
	test_harness_thread_done = false;
	initialize_deque(datablock_deque);
	pthread_create(&invoke_thread, NULL, invoke_loop, NULL);

	/*
	 * Global plugin instance.
	 * Note: Southbound function pointers are to be filled by hostapd core
	 * after registration (as per integration contract).
	 * We only provide northbound handlers here.
	 */
	test_plugin.invoke_assoc         = invoke_assoc,
	test_plugin.invoke_auth          = invoke_auth,
	test_plugin.invoke_remote_auth   = invoke_remote_auth,
	test_plugin.notify_assoc         = notify_assoc,
	test_plugin.notify_auth          = notify_auth,
	test_plugin.notify_remote_auth   = notify_remote_auth,
	test_plugin.notify_disassoc      = notify_disassoc,
	test_plugin.notify_deauth        = notify_deauth,
	test_plugin.notify_action        = notify_action,
	test_plugin.offload_action       = offload_action,
	test_plugin.notify_event         = notify_event,
	test_plugin.interface_create     = interface_create,
	test_plugin.interface_remove     = interface_remove,
	test_plugin.pull_pmk_r1          = pull_pmk_r1;

	hostapd_plugin_register(&test_plugin);
}

static void hostapd_if_plugin_set_assoc_resp_ies(uint8_t *buf, size_t buf_len)
{
	/* Free any previously stored IEs */
	if (assoc_resp_ies) {
		os_free(assoc_resp_ies);
		assoc_resp_ies = NULL;
		assoc_resp_ies_len = 0;
	}

	/* Store new IEs if provided */
	if (buf && buf_len > 0) {
		assoc_resp_ies = os_malloc(buf_len);
		if (assoc_resp_ies) {
			os_memcpy(assoc_resp_ies, buf, buf_len);
			assoc_resp_ies_len = buf_len;
			wpa_printf(MSG_DEBUG,
				   "Stored %zu bytes of association response IEs\n",
				   buf_len);
		} else {
			wpa_printf(MSG_ERROR,
				   "Failed to allocate memory for association response IEs\n");
		}
	} else {
		wpa_printf(MSG_DEBUG, "Cleared association response IEs\n");
	}
}

static void hostapd_if_plugin_set_auth_resp_ies(uint8_t *buf, size_t buf_len)
{
	/* Free any previously stored IEs */
	if (auth_resp_ies) {
		os_free(auth_resp_ies);
		auth_resp_ies = NULL;
		auth_resp_ies_len = 0;
	}

	/* Store new IEs if provided */
	if (buf && buf_len > 0) {
		auth_resp_ies = os_malloc(buf_len);
		if (auth_resp_ies) {
			os_memcpy(auth_resp_ies, buf, buf_len);
			auth_resp_ies_len = buf_len;
			wpa_printf(MSG_DEBUG,
				   "Stored %zu bytes of authentication response IEs\n",
				   buf_len);
		} else {
			wpa_printf(MSG_ERROR,
				   "Failed to allocate memory for authentication response IEs\n");
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "Cleared authentication response IEs\n");
	}
}

/* Functions for hostapd_cli */

static int hostapd_ctrl_iface_remove_additional_ies(struct hostapd_data *hapd,
						    const char *cmd)
{
	/* usage: SET_ADDITIONAL_IES <beacon|probe|both> <hex> */
	const char *pos = cmd;
	int link_id;
	uint8_t *buf = NULL;

	link_id = atoi(pos);
	test_plugin.set_beacon_probe_vendor_ies(hapd->conf->iface, NULL, 0,
						link_id);

	os_free(buf);
	return 0;
}

static int hostapd_ctrl_iface_set_additional_ies(struct hostapd_data *hapd,
						 const char *cmd)
{
	/* usage: SET_ADDITIONAL_IES <beacon|probe|both> <hex> */
	const char *hex = cmd;
	const char *pos = cmd;
	char *bcn_buf_hex_end_ptr;
	char *hex_copy;
	int bcn_buf_len;
	int link_id;
	uint8_t *buf = NULL;

	if (!hex)
		return -1;

	bcn_buf_hex_end_ptr = os_strchr(hex, ' ');

	if (!bcn_buf_hex_end_ptr) {
		wpa_printf(MSG_ERROR, "Incorrect format for set-additional-IEs");
		wpa_printf(MSG_ERROR, "SET-ADDITIONAL-IES <buffer> <link_id>");
		return -1;
	}
	bcn_buf_len = (bcn_buf_hex_end_ptr - hex) / 2;
	/* step over whitespace also */
	pos += (2 * bcn_buf_len) + 1;

	link_id = atoi(pos);
	buf = os_malloc(bcn_buf_len);
	hex_copy = os_malloc(bcn_buf_len*2+1);
	if (!buf || !hex_copy) {
		os_free(buf);
		os_free(hex_copy);
		return -1;
	}
	os_memcpy(hex_copy, hex, bcn_buf_len*2);
	hex_copy[bcn_buf_len*2] = 0;

	if (hexstr2bin(hex_copy, buf, bcn_buf_len) < 0) {
		os_free(hex_copy);
		os_free(buf);
		return -1;
	}
	os_free(hex_copy);

	test_plugin.set_beacon_probe_vendor_ies((char *)hapd->conf->iface, buf,
						bcn_buf_len, link_id);
	return 0;
}

/* Public API: Set association response IEs */
static int hostapd_ctrl_iface_set_assoc_ies(struct hostapd_data *hapd,
					    const char *cmd)
{
	/* usage: SET_ASSOC_IES <hex> */
	const char *hex = cmd;
	size_t hex_len;
	size_t buf_len;
	uint8_t *buf = NULL;
	int ret = -1;

	if (!hex)
		return -1;

	hex_len = os_strlen(hex);
	if (hex_len % 2) {
		wpa_printf(MSG_ERROR,
			   "SET_ASSOC_IES: Invalid hex string length");
		return -1;
	}

	buf_len = hex_len / 2;
	buf = os_malloc(buf_len);
	if (!buf)
		return -1;

	if (hexstr2bin(hex, buf, buf_len) < 0) {
		wpa_printf(MSG_ERROR, "SET_ASSOC_IES: Invalid hex string");
		os_free(buf);
		return -1;
	}

	hostapd_if_plugin_set_assoc_resp_ies(buf, buf_len);
	ret = 0;

	os_free(buf);
	return ret;
}

/* Public API: Set authentication response IEs */
static int hostapd_ctrl_iface_set_auth_ies(struct hostapd_data *hapd,
					   const char *cmd)
{
	/* usage: SET_AUTH_IES <hex> */
	const char *hex = cmd;
	size_t hex_len;
	size_t buf_len;
	uint8_t *buf = NULL;
	int ret = -1;

	if (!hex)
		return -1;

	hex_len = os_strlen(hex);
	if (hex_len % 2) {
		wpa_printf(MSG_ERROR,
			   "SET_AUTH_IES: Invalid hex string length");
		return -1;
	}

	buf_len = hex_len / 2;
	buf = os_malloc(buf_len);
	if (!buf)
		return -1;

	if (hexstr2bin(hex, buf, buf_len) < 0) {
		wpa_printf(MSG_ERROR, "SET_AUTH_IES: Invalid hex string");
		os_free(buf);
		return -1;
	}

	hostapd_if_plugin_set_auth_resp_ies(buf, buf_len);
	ret = 0;

	os_free(buf);
	return ret;
}

static int hostapd_ctrl_iface_send_frame(struct hostapd_data *hapd,
					 const char *cmd)
{
	const char *hex = cmd;
	u8 *buf = NULL;
	int link_id = -1;
	size_t hex_len, len;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SEND_FRAME %s", cmd);

	if (!hex)
		return -1;

	if (!test_plugin.send_frame) {
		wpa_printf(MSG_ERROR, "SEND_FRAME: send_frame callback not registered");
		return -1;
	}

	while (*hex && isspace((unsigned char)*hex))
		hex++;

	if (*hex == '\0') {
		wpa_printf(MSG_ERROR, "SEND_FRAME: Missing frame data");
		return -1;
	}

	hex_len = os_strlen(hex);
	if (hex_len & 1) {
		wpa_printf(MSG_ERROR, "SEND_FRAME: Invalid hex string length");
		return -1;
	}

	len = hex_len / 2;
	if (!len || len > 0xffff) {
		wpa_printf(MSG_ERROR,
			   "SEND_FRAME: Invalid frame length %u",
			   (unsigned int) len);
		return -1;
	}

	buf = os_malloc(len);
	if (!buf)
		return -1;

	if (hexstr2bin(hex, buf, len) < 0) {
		wpa_printf(MSG_ERROR, "SEND_FRAME: Invalid hex string");
		goto fail;
	}

	test_plugin.send_frame((char *)hapd->conf->iface, link_id, buf,
			       (uint16_t) len);
	return 0;

fail:
	os_free(buf);
	return -1;
}

static int hostapd_ctrl_iface_start_sa_query_plugin(struct hostapd_data *hapd,
						    const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	int link_id = -1;
	const char *pos;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE START_SA_QUERY_PLUGIN %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " link_id=");
	if (pos)
		link_id = atoi(pos + 9);

	test_plugin.start_sa_query((char *)hapd->conf->iface, addr, link_id);

	return 0;
}


static int hostapd_ctrl_iface_disassociate_plugin(struct hostapd_data *hapd,
						  const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;
	const u8 *added_data = NULL;
	size_t added_data_len = 0;
	u8 *data_buf = NULL;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DISASSOCIATE-PLUGIN %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	/* Parse added_data parameter */
	pos = os_strstr(txtaddr, " added_data=");
	if (pos) {
		pos += 12;
		/* Find the length of the hex string */
		const char *end = pos;
		while (*end && *end != ' ')
			end++;

		added_data_len = (end - pos) / 2;
		if (added_data_len > 0) {
			data_buf = os_malloc(added_data_len);
			if (!data_buf)
				return -1;

			if (hexstr2bin(pos, data_buf, added_data_len) < 0) {
				os_free(data_buf);
				return -1;
			}
			added_data = data_buf;
		}
	}

	/*
	 * Call external plugin to send disassociation via
	 * test->invoke_disassoc
	 */
	test_plugin.send_disassoc((char *)hapd->conf->iface, addr, reason, -1,
				  (uint8_t *)added_data,
				  (uint8_t)added_data_len);

	return 0;
}

static int hostapd_ctrl_iface_deauthenticate_plugin(struct hostapd_data *hapd,
						    const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;
	const u8 *added_data = NULL;
	size_t added_data_len = 0;
	u8 *data_buf = NULL;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
		"CTRL_IFACE DEAUTHENTICATE-PLUGIN %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	/* Parse added_data parameter */
	pos = os_strstr(txtaddr, " added_data=");
	if (pos) {
		pos += 12;
		/* Find the length of the hex string */
		const char *end = pos;
		while (*end && *end != ' ')
			end++;

		added_data_len = (end - pos) / 2;
		if (added_data_len > 0) {
			data_buf = os_malloc(added_data_len);
			if (!data_buf)
				return -1;

			if (hexstr2bin(pos, data_buf, added_data_len) < 0) {
				os_free(data_buf);
				return -1;
			}
			added_data = data_buf;
		}
	}

	/*
	 * Call external plugin to send deauthentication
	 * via test->invoke_deauth
	 */
	test_plugin.send_deauth((char *)hapd->conf->iface, addr, reason, -1,
				(uint8_t *) added_data,
				(uint8_t) added_data_len);

	return 0;
}

static int hostapd_ctrl_iface_get_pmk_plugin(struct hostapd_data *hapd,
					     const char *txtaddr,
					     char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	uint8_t pmk[PMK_LEN_MAX];
	size_t pmk_len = 0;
	uint8_t pmkid[PMKID_LEN];
	int ret, len = 0;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET_PMK_PLUGIN %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr)) {
		wpa_printf(MSG_ERROR, "GET_PMK_PLUGIN: Invalid MAC address");
		return -1;
	}

	/* Initialize the PMK data */
	os_memset(pmk, 0, sizeof(pmk));
	os_memset(pmkid, 0, sizeof(pmkid));

	/* Call the plugin function to get PMK */
	test_plugin.get_pmk((char *)hapd->conf->iface, addr, pmk, &pmk_len,
			    pmkid);

	/* Check if PMK was retrieved */
	if (pmk_len == 0) {
		wpa_printf(MSG_ERROR, "GET_PMK_PLUGIN: No PMK available for "
			   MACSTR, MAC2STR(addr));
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return -1;
		return ret;
	}

	/* Format the output: PMK in hex */
	ret = os_snprintf(buf + len, buflen - len, "pmk=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	len += wpa_snprintf_hex(buf + len, buflen - len, pmk, pmk_len);

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* Add PMKID if available */
	ret = os_snprintf(buf + len, buflen - len, "pmkid=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	len += wpa_snprintf_hex(buf + len, buflen - len, pmkid, PMKID_LEN);

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}


static int hostapd_ctrl_iface_get_ptk_plugin(struct hostapd_data *hapd,
					     const char *txtaddr,
					     char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	uint8_t kck[MAX_KCK_LEN];
	size_t kck_len = 0;
	uint8_t kek[MAX_KEK_LEN];
	size_t kek_len = 0;
	uint8_t tk[MAX_TK_LEN];
	size_t tk_len = 0;
	int ret, len = 0;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET_PTK_PLUGIN %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr)) {
		wpa_printf(MSG_ERROR, "GET_PTK_PLUGIN: Invalid MAC address");
		return -1;
	}

	/* Initialize the PTK data */
	os_memset(kck, 0, sizeof(kck));
	os_memset(kek, 0, sizeof(kek));
	os_memset(tk, 0, sizeof(tk));

	/* Call the plugin function to get PTK */
	test_plugin.get_ptk((char *)hapd->conf->iface, addr, kck, &kck_len, kek,
			    &kek_len, tk, &tk_len);

	/* Check if PTK was retrieved */
	if (kck_len == 0) {
		wpa_printf(MSG_ERROR, "GET_PTK_PLUGIN: No PTK available for "
			   MACSTR, MAC2STR(addr));
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return -1;
		return ret;
	}

	/* Format the output: KCK */
	if (kck_len > 0) {
		ret = os_snprintf(buf + len, buflen - len, "kck=");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		len += wpa_snprintf_hex(buf + len, buflen - len, kck, kck_len);

		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	/* Format the output: KEK */
	if (kek_len > 0) {
		ret = os_snprintf(buf + len, buflen - len, "kek=");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		len += wpa_snprintf_hex(buf + len, buflen - len, kek, kek_len);

		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	/* Format the output: TK */
	if (tk_len > 0) {
		ret = os_snprintf(buf + len, buflen - len, "tk=");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		len += wpa_snprintf_hex(buf + len, buflen - len, tk, tk_len);

		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	return len;
}


static int hostapd_ctrl_iface_get_gtk_plugin(struct hostapd_data *hapd,
					     const char *txtaddr,
					     char *buf, size_t buflen)
{
	int gtk_idx = 0;
	uint8_t gtk[MAX_GTK_LEN];
	size_t gtk_len = 0;
	int link_id = 0;
	char* pos;
	int ret, len = 0;

	pos = os_strstr(txtaddr, "link_id=");
	if (pos) {
		pos += 8;
		link_id = atoi(pos);
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET_GTK_PLUGIN %s", txtaddr);


	/* Initialize the GTK data */
	os_memset(gtk, 0, sizeof(gtk));

	/* Call the plugin function to get GTK */
	test_plugin.get_gtk((char *)hapd->conf->iface, link_id, &gtk_idx, gtk,
			    &gtk_len);

	/* Check if GTK was retrieved */
	if (gtk_len == 0) {
		wpa_printf(MSG_ERROR, "GET_GTK_PLUGIN: No GTK available\n");
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return -1;
		return ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "gtk=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	len += wpa_snprintf_hex(buf + len, buflen - len, gtk, gtk_len);

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* Add GTK index */
	ret = os_snprintf(buf + len, buflen - len, "gtk_idx=%u\n", gtk_idx);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}

static int hostapd_ctrl_iface_configure_plugin_config(struct hostapd_data *hapd,
						      const char *cmd)
{
	char *pos;
	int val;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE CONFIGURE_PLUGIN CONFIG %s", cmd);

	/* Initialize the global_configuration structure with default values */
	os_memset(&global_conf, 0, sizeof(global_conf));

	/*
	 * Set default values for status codes to -1 (use original status code)
	 */
	global_conf.auth.status_code = -1;
	global_conf.assoc.status_code = -1;
	global_conf.assoc.send_response = 1;
	global_conf.auth.send_response = 1;

	/* Parse out_of_order_auth parameter */
	pos = os_strstr(cmd, "out_of_order_auth=");
	if (pos) {
		pos += 18;
		val = atoi(pos);

		global_conf.auth.out_of_order = val;
	}

	/* Parse out_of_order_assoc parameter */
	pos = os_strstr(cmd, "out_of_order_assoc=");
	if (pos) {
		pos += 19;
		val = atoi(pos);
		global_conf.assoc.out_of_order = val;
	}

	/* Parse process_frames_auth parameter */
	pos = os_strstr(cmd, "process_frames_auth=");
	if (pos) {
		pos += 20;
		global_conf.auth.process_frames = os_strcmp(pos, "1") == 0 ||
		                           os_strncmp(pos, "true", 4) == 0;
	}

	/* Parse process_frames_assoc parameter */
	pos = os_strstr(cmd, "process_frames_assoc=");
	if (pos) {
		pos += 21;
		global_conf.assoc.process_frames = os_strcmp(pos, "1") == 0 ||
		                            os_strncmp(pos, "true", 4) == 0;
	}

	/* Parse send_auth parameter */
	pos = os_strstr(cmd, "send_auth=");
	if (pos) {
		pos += 10;
		global_conf.auth.send_response = os_strcmp(pos, "1") == 0 ||
		                          os_strncmp(pos, "true", 4) == 0;
	}

	/* Parse send_assoc parameter */
	pos = os_strstr(cmd, "send_assoc=");
	if (pos) {
		pos += 11;
		global_conf.assoc.send_response = os_strcmp(pos, "1") == 0 ||
		                           os_strncmp(pos, "true", 4) == 0;
	}

	/* Parse delay_auth parameter */
	pos = os_strstr(cmd, "delay_auth=");
	if (pos) {
		pos += 11;
		val = atoi(pos);
		global_conf.auth.delay = val;
	}

	/* Parse delay_assoc parameter */
	pos = os_strstr(cmd, "delay_assoc=");
	if (pos) {
		pos += 12;
		val = atoi(pos);
		global_conf.assoc.delay = val;
	}

	/* Parse auth_status_code parameter */
	pos = os_strstr(cmd, "auth_status_code=");
	if (pos) {
		pos += 17;
		val = atoi(pos);
		global_conf.auth.status_code = val;
	}

	/* Parse assoc_status_code parameter */
	pos = os_strstr(cmd, "assoc_status_code=");
	if (pos) {
		pos += 18;
		val = atoi(pos);
		global_conf.assoc.status_code = val;
	}

	return 0;
}

/*
 * Helper: return a human-readable band name for wpa_printf output.
 */
static const char *sta_info_band_str(enum hostapd_if_band band)
{
	switch (band) {
	case HOSTAPD_IF_BAND_2GHZ:    return "2.4 GHz";
	case HOSTAPD_IF_BAND_5GHZ:    return "5 GHz";
	case HOSTAPD_IF_BAND_6GHZ:    return "6 GHz";
	case HOSTAPD_IF_BAND_60GHZ:   return "60 GHz";
	default:                       return "unknown";
	}
}

/*
 * Helper: dump a hostapd_if_radio_info block via wpa_printf.
 * @prefix is prepended to each line (e.g. "  " or "  link[0] ").
 */
static void dump_radio_info(const char *prefix,
			    const struct hostapd_if_radio_info *r)
{
	wpa_printf(MSG_INFO, "%sfreq=%d MHz  channel=%u  band=%s",
		   prefix, r->freq, r->channel,
		   sta_info_band_str(r->band));
	wpa_printf(MSG_INFO, "%srssi=%d dBm", prefix, r->rssi);
	wpa_printf(MSG_INFO,
		   "%scap_flags=0x%08x [%s%s%s%s%s%s%s%s%s%s]",
		   prefix, r->cap_flags,
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_OFDM) ? "OFDM "  : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_11G)  ? "11G "   : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_11N)  ? "11N "   : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_HT)   ? "HT "    : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_HT40) ? "HT40 "  : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_VHT)  ? "VHT "   : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_HE)   ? "HE "    : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_EHT)  ? "EHT "   : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_MLD)  ? "MLD "   : "",
		   (r->cap_flags & HOSTAPD_IF_STA_CAP_6GHZ) ? "6GHZ "  : "");
}

/*
 * TEST: exercise test_plugin.get_sta_info for a given STA MAC address and
 * dump all returned fields via wpa_printf.
 *
 * Command syntax (as passed from the CONFIGURE-PLUGIN dispatcher):
 *   GET_STA_INFO <MAC>
 * e.g.:
 *   hostapd_cli -i wlan0 CONFIGURE-PLUGIN GET_STA_INFO aa:bb:cc:dd:ee:ff
 */
static int hostapd_ctrl_iface_get_sta_info_plugin(struct hostapd_data *hapd,
						  const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct hostapd_if_sta_info info;
	int ret;
	int i;

	wpa_printf(MSG_INFO, "TEST get_sta_info: ifname=%s sta=%s",
		   hapd->conf->iface, txtaddr);

	if (hwaddr_aton(txtaddr, addr)) {
		wpa_printf(MSG_ERROR,
			   "TEST get_sta_info: invalid MAC address '%s'",
			   txtaddr);
		return -1;
	}

	if (!test_plugin.get_sta_info) {
		wpa_printf(MSG_ERROR,
			   "TEST get_sta_info: southbound get_sta_info not registered");
		return -1;
	}

	os_memset(&info, 0, sizeof(info));

	ret = test_plugin.get_sta_info((char *)hapd->conf->iface, addr, &info);
	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "TEST get_sta_info: get_sta_info(" MACSTR ") failed: %d",
			   MAC2STR(addr), ret);
		return ret;
	}

	wpa_printf(MSG_INFO,
		   "TEST get_sta_info: result for " MACSTR,
		   MAC2STR(addr));

	/* ---- HT capabilities ---- */
	if (info.ht_caps_len > 0) {
		char hex[HOSTAPD_IF_HT_CAP_MAX_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 info.ht_caps, info.ht_caps_len);
		wpa_printf(MSG_INFO, "  ht_caps(%u bytes): %s",
			   info.ht_caps_len, hex);
	} else {
		wpa_printf(MSG_INFO, "  ht_caps: not present");
	}

	/* ---- VHT capabilities ---- */
	if (info.vht_caps_len > 0) {
		char hex[HOSTAPD_IF_VHT_CAP_MAX_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 info.vht_caps, info.vht_caps_len);
		wpa_printf(MSG_INFO, "  vht_caps(%u bytes): %s",
			   info.vht_caps_len, hex);
	} else {
		wpa_printf(MSG_INFO, "  vht_caps: not present");
	}

	/* ---- HE capabilities ---- */
	if (info.he_caps_len > 0) {
		char hex[HOSTAPD_IF_HE_CAP_MAX_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 info.he_caps, info.he_caps_len);
		wpa_printf(MSG_INFO, "  he_caps(%u bytes): %s",
			   info.he_caps_len, hex);
	} else {
		wpa_printf(MSG_INFO, "  he_caps: not present");
	}

	/* ---- EHT capabilities ---- */
	if (info.eht_caps_len > 0) {
		char hex[HOSTAPD_IF_EHT_CAP_MAX_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 info.eht_caps, info.eht_caps_len);
		wpa_printf(MSG_INFO, "  eht_caps(%u bytes): %s",
			   info.eht_caps_len, hex);
	} else {
		wpa_printf(MSG_INFO, "  eht_caps: not present");
	}

	/* ---- Radio / MLD information ---- */
	if (info.is_mld_sta) {
		const struct hostapd_if_mld_info *m = &info.u.mld_info;

		wpa_printf(MSG_INFO,
			   "  MLD station: mld_addr=" MACSTR
			   "  eml_capa=0x%04x  mld_capa=0x%04x  num_links=%u",
			   MAC2STR(m->mld_addr),
			   m->eml_capa, m->mld_capa, m->num_links);

		for (i = 0; i < MAX_MLO_LINKS; i++) {
			const struct hostapd_if_mld_link_info *lnk =
				&m->links[i];
			char pfx[32];

			if (!lnk->valid)
				continue;

			wpa_printf(MSG_INFO,
				   "  link[%d]: local=" MACSTR
				   "  peer=" MACSTR,
				   i,
				   MAC2STR(lnk->local_addr),
				   MAC2STR(lnk->peer_addr));

			os_snprintf(pfx, sizeof(pfx), "  link[%d] ", i);
			dump_radio_info(pfx, &lnk->radio);
		}
	} else {
		wpa_printf(MSG_INFO, "  non-MLD station radio info:");
		dump_radio_info("  ", &info.u.non_mld);
	}

	return 0;
}

/*
 * Main dispatcher function for CONFIGURE-PLUGIN command
 * Routes to appropriate subcommand handler
 */
int hostapd_ctrl_iface_configure_plugin(struct hostapd_data *hapd,
					const char *cmd,
					char *buf, size_t buflen)
{
	const char *pos = cmd;
	int reply_len = 0;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE CONFIGURE-PLUGIN %s", cmd);

	/* Skip leading whitespace */
	while (*pos == ' ')
		pos++;

	/* Parse subcommand */
	if (os_strncmp(pos, "CONFIG ", 7) == 0) {
		return hostapd_ctrl_iface_configure_plugin_config(hapd, pos + 7);
	} else if (os_strncmp(pos, "DISASSOCIATE ", 13) == 0) {
		return hostapd_ctrl_iface_disassociate_plugin(hapd, pos + 13);
	} else if (os_strncmp(pos, "DEAUTHENTICATE ", 15) == 0) {
		return hostapd_ctrl_iface_deauthenticate_plugin(hapd, pos + 15);
	} else if (os_strncmp(pos, "START_SA_QUERY ", 15) == 0) {
		return hostapd_ctrl_iface_start_sa_query_plugin(hapd, pos + 15);
	} else if (os_strncmp(pos, "GET_PMK ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_get_pmk_plugin(hapd, pos + 8,
							       buf, buflen);
		return reply_len;
	} else if (os_strncmp(pos, "GET_PTK ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_get_ptk_plugin(hapd, pos + 8,
							       buf, buflen);
		return reply_len;
	} else if (os_strncmp(pos, "GET_GTK ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_get_gtk_plugin(hapd, pos + 8,
							       buf, buflen);
		return reply_len;
	} else if (os_strncmp(pos, "SET_ADDITIONAL_IES ", 19) == 0) {
		return hostapd_ctrl_iface_set_additional_ies(hapd, pos + 19);
	} else if (os_strncmp(pos, "REMOVE_ADDITIONAL_IES ", 22) == 0) {
		return hostapd_ctrl_iface_remove_additional_ies(hapd, pos + 22);
	} else if (os_strncmp(pos, "SET_ASSOC_IES ", 14) == 0) {
		return hostapd_ctrl_iface_set_assoc_ies(hapd, pos + 14);
	} else if (os_strncmp(pos, "SET_AUTH_IES ", 13) == 0) {
		return hostapd_ctrl_iface_set_auth_ies(hapd, pos + 13);
	} else if (os_strncmp(pos, "GET_STA_INFO ", 13) == 0) {
		return hostapd_ctrl_iface_get_sta_info_plugin(hapd, pos + 13);
	} else if (os_strncmp(pos, "SEND_FRAME ",11) == 0) {
		return hostapd_ctrl_iface_send_frame(hapd, pos + 11);
	} else {
		wpa_printf(MSG_ERROR,
			   "CONFIGURE-PLUGIN: Unknown subcommand '%s'", pos);
		return -1;
	}
}

void hostapd_if_plugin_store_pmk_r1(const char *ifname, const u8 *spa,
				    const u8 *pmk_r1, size_t pmk_r1_len,
				    const u8 *pmk_r1_name, int pairwise,
				    int expires_in, int session_timeout,
				    const u8 *identity, size_t identity_len,
				    const u8 *radius_cui,
				    size_t radius_cui_len)
{
	struct plugin_hapd_iface *iface;
	struct wpa_ft_pmk_r1_sa *r1;
	struct os_reltime now;

	if (!test_harness_thread_running)
		return;

	if (!ifname) {
		wpa_printf(MSG_ERROR, "plugin: store_pmk_r1 called with NULL ifname\n");
		return;
	}

	pthread_mutex_lock(&plugin_iface_mutex);

	iface = plugin_hapd_iface_get(ifname);
	if (!iface) {
		wpa_printf(MSG_ERROR,
			   "plugin: store_pmk_r1: no iface found for %s\n",
			   ifname);
		pthread_mutex_unlock(&plugin_iface_mutex);
		return;
	}

	os_get_reltime(&now);

	r1 = os_zalloc(sizeof(*r1));
	if (!r1) {
		wpa_printf(MSG_ERROR,
			   "plugin: failed to alloc PMK-R1 entry for %s\n",
			   ifname);
		pthread_mutex_unlock(&plugin_iface_mutex);
		return;
	}

	os_memcpy(r1->pmk_r1, pmk_r1, pmk_r1_len);
	r1->pmk_r1_len = pmk_r1_len;
	os_memcpy(r1->pmk_r1_name, pmk_r1_name, WPA_PMK_NAME_LEN);
	r1->pairwise = pairwise;
	os_memcpy(r1->spa, spa, ETH_ALEN);

	if (identity && identity_len > 0) {
		r1->identity = os_malloc(identity_len);
		if (r1->identity) {
			os_memcpy(r1->identity, identity, identity_len);
			r1->identity_len = identity_len;
		}
	}

	if (radius_cui && radius_cui_len > 0) {
		r1->radius_cui = os_malloc(radius_cui_len);
		if (r1->radius_cui) {
			os_memcpy(r1->radius_cui, radius_cui, radius_cui_len);
			r1->radius_cui_len = radius_cui_len;
		}
	}

	if (session_timeout > 0)
		r1->session_timeout = now.sec + session_timeout;

	dl_list_add(&iface->ft_pmk_cache.pmk_r1, &r1->list);
	wpa_ft_clear_pmk_r1(iface->hapd->wpa_auth, spa, pmk_r1_name);

	wpa_printf(MSG_DEBUG,
		   "plugin: stored PMK-R1 in cache for iface %s (pairwise=%d expires_in=%d)\n",
		   ifname, pairwise, expires_in);

	pthread_mutex_unlock(&plugin_iface_mutex);
}

int hostapd_config_fill_plugin(struct hostapd_bss_config *bss, const char *buf,
			       char *pos)
{
	size_t i;

	if (os_strcmp(buf, "external_plugin_auth_policy") == 0) {
		bss->plugin.external_plugin_auth_policy = atoi(pos);
	} else if (os_strcmp(buf, "external_plugin_remote_auth_policy") == 0) {
		bss->plugin.external_plugin_remote_auth_policy = atoi(pos);
	} else if (os_strcmp(buf, "external_plugin_assoc_policy") == 0) {
		bss->plugin.external_plugin_assoc_policy = atoi(pos);
	} else if (os_strcmp(buf, "external_plugin_deauth_policy") == 0) {
		bss->plugin.external_plugin_deauth_policy = atoi(pos);
	} else if (os_strcmp(buf, "external_plugin_disassoc_policy") == 0) {
		bss->plugin.external_plugin_disassoc_policy = atoi(pos);
	} else {
		for (i = 0; i < ARRAY_SIZE(hostapd_if_action_policy_map); i++) {
			if (os_strcmp(buf,
				      hostapd_if_action_policy_map[i].conf_name) != 0)
				continue;

			bss->plugin.external_plugin_action_policy[
				hostapd_if_action_policy_map[i].action_type] = atoi(pos);
			return 0;
		}

		return -1;
	}
	return 0;
}
