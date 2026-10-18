/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * external_hif_mqtt_test_client.c - Minimal MQTT subscriber for HOSTAPD_IF events
 *
 * Connects to an MQTT broker, subscribes to the HOSTAPD_IF feature topic,
 * deserializes every incoming TLV message, and prints its contents.
 * No commands are sent; this is purely a passive listener.
 *
 * Build:
 *   gcc -Wall -I../src -DCONFIG_MQTT -o external_hif_mqtt_test_client \
 *       external_hif_mqtt_test_client.c -lmosquitto
 *
 * Usage:
 *   ./external_hif_mqtt_test_client [broker_host [broker_port]]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <mosquitto.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#define CONFIG_MQTT 1
#include "utils/mqtt_feature_map.h"
#include "../qcn_extns/hostapd_external_interface.h"

#define BROKER_DEFAULT_HOST  "localhost"
#define BROKER_DEFAULT_PORT  1883
#define BROKER_KEEPALIVE     60

/* Subscribe only to the HOSTAPD_IF transmit subtopic */
#define SUB_TOPIC  MQTT_TOPIC_TRANSMIT "/" MQTT_FEATURE_HOSTAPD_IF

/* Publish commands to the HOSTAPD_IF receive subtopic */
#define PUB_TOPIC     MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_HOSTAPD_IF
#define CMD_BUF_SIZE  64

/* -- TLV display --------------------------------------------------------------- */

static void print_value(const struct mqtt_tlv_entry *e,
			enum mqtt_tlv_val_type val_type)
{
	uint16_t i;

	switch (val_type) {
	case MQTT_TLV_VAL_MAC:
		if (e->length == 6) {
			printf("%02x:%02x:%02x:%02x:%02x:%02x",
			       e->value[0], e->value[1], e->value[2],
			       e->value[3], e->value[4], e->value[5]);
			return;
		}
		break;
	case MQTT_TLV_VAL_U8:
		if (e->length == 1) {
			printf("%u (0x%02x)", e->value[0], e->value[0]);
			return;
		}
		break;
	case MQTT_TLV_VAL_U16:
		if (e->length == 2) {
			uint16_t v = ((uint16_t)e->value[0] << 8) | e->value[1];
			printf("%u (0x%04x)", v, v);
			return;
		}
		break;
	case MQTT_TLV_VAL_U32:
		if (e->length == 4) {
			uint32_t v = ((uint32_t)e->value[0] << 24) |
				     ((uint32_t)e->value[1] << 16) |
				     ((uint32_t)e->value[2] <<  8) |
				     e->value[3];
			int32_t sv = (int32_t)v;
			if (sv < 0)
				printf("%d (0x%08x)", sv, v);
			else
				printf("%u (0x%08x)", v, v);
			return;
		}
		break;
	case MQTT_TLV_VAL_S32:
		if (e->length == 4) {
			uint32_t v = ((uint32_t)e->value[0] << 24) |
				     ((uint32_t)e->value[1] << 16) |
				     ((uint32_t)e->value[2] <<  8) |
				     e->value[3];
			printf("%d (0x%08x)", (int32_t)v, v);
			return;
		}
		break;
	case MQTT_TLV_VAL_STRING:
		if (e->length > 0) {
			char tmp[128];
			uint16_t n = e->length < 127 ? e->length : 127;
			memcpy(tmp, e->value, n);
			tmp[n] = '\0';
			printf("'%s'", tmp);
			return;
		}
		break;
	default:
		break;
	}

	/* Fallback: hex dump */
	printf("0x");
	for (i = 0; i < e->length && i < 16; i++)
		printf("%02x", e->value[i]);
	if (e->length > 16)
		printf("...");
	printf(" (%u B)", e->length);
}

static enum mqtt_tlv_val_type
get_hif_tlv_val_type(uint16_t msg_id, uint16_t bare)
{
	/* For CMD messages, use the existing policy table */
	const struct mqtt_tlv_policy *pol = mqtt_tlv_policy_lookup(msg_id, bare);
	if (pol)
		return pol->val_type;

	/* For EVT messages derive from the per-enum comments in mqtt_tlv_map.h */
	if (msg_id == EVT_ID_HIF_NOTIFY_AUTH) {
		switch (bare) {
		case TLV_HIF_AUTH_IND_IFACE:              return MQTT_TLV_VAL_STRING;
		case TLV_HIF_AUTH_IND_STA_MAC:            return MQTT_TLV_VAL_MAC;
		case TLV_HIF_AUTH_IND_STATUS_CODE:        return MQTT_TLV_VAL_U16;
		case TLV_HIF_AUTH_IND_AUTH_TRANSACTION:   return MQTT_TLV_VAL_U16;
		case TLV_HIF_AUTH_IND_ALLOW_REUSE:        return MQTT_TLV_VAL_U8;
		case TLV_HIF_AUTH_IND_AUTH_ALG:           return MQTT_TLV_VAL_U16;
		case TLV_HIF_AUTH_IND_RSSI_DBM:           return MQTT_TLV_VAL_U32;
		case TLV_HIF_AUTH_IND_RX_LINK_ID:         return MQTT_TLV_VAL_S32;
		case TLV_HIF_AUTH_IND_STA_ASSOC_LINK_MAC: return MQTT_TLV_VAL_MAC;
		case TLV_HIF_AUTH_IND_FRAME:              return MQTT_TLV_VAL_BINARY;
		case TLV_HIF_AUTH_IND_FRAME_LEN:          return MQTT_TLV_VAL_U16;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_NOTIFY_ASSOC) {
		switch (bare) {
		case TLV_HIF_ASSOC_IND_IFACE:              return MQTT_TLV_VAL_STRING;
		case TLV_HIF_ASSOC_IND_STA_MAC:            return MQTT_TLV_VAL_MAC;
		case TLV_HIF_ASSOC_IND_STATUS_CODE:        return MQTT_TLV_VAL_U16;
		case TLV_HIF_ASSOC_IND_IS_REASSOC:         return MQTT_TLV_VAL_U8;
		case TLV_HIF_ASSOC_IND_RSSI_DBM:           return MQTT_TLV_VAL_U32;
		case TLV_HIF_ASSOC_IND_RX_LINK_ID:         return MQTT_TLV_VAL_S32;
		case TLV_HIF_ASSOC_IND_VALID_LINK_BITMAP:  return MQTT_TLV_VAL_U32;
		case TLV_HIF_ASSOC_IND_STA_ASSOC_LINK_MAC: return MQTT_TLV_VAL_MAC;
		case TLV_HIF_ASSOC_IND_STA_LINK_MACS:      return MQTT_TLV_VAL_BINARY;
		case TLV_HIF_ASSOC_IND_FRAME:              return MQTT_TLV_VAL_BINARY;
		case TLV_HIF_ASSOC_IND_FRAME_LEN:          return MQTT_TLV_VAL_U16;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_NOTIFY_DEAUTH) {
		switch (bare) {
		case TLV_HIF_DEAUTH_IND_IFACE:       return MQTT_TLV_VAL_STRING;
		case TLV_HIF_DEAUTH_IND_STA_MAC:     return MQTT_TLV_VAL_MAC;
		case TLV_HIF_DEAUTH_IND_RX_LINK_ID:  return MQTT_TLV_VAL_S32;
		case TLV_HIF_DEAUTH_IND_REASON_CODE: return MQTT_TLV_VAL_U16;
		case TLV_HIF_DEAUTH_IND_FRAME:       return MQTT_TLV_VAL_BINARY;
		case TLV_HIF_DEAUTH_IND_FRAME_LEN:   return MQTT_TLV_VAL_U16;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_NOTIFY_DISASSOC) {
		switch (bare) {
		case TLV_HIF_DISASSOC_IND_IFACE:       return MQTT_TLV_VAL_STRING;
		case TLV_HIF_DISASSOC_IND_STA_MAC:     return MQTT_TLV_VAL_MAC;
		case TLV_HIF_DISASSOC_IND_RX_LINK_ID:  return MQTT_TLV_VAL_S32;
		case TLV_HIF_DISASSOC_IND_REASON_CODE: return MQTT_TLV_VAL_U16;
		case TLV_HIF_DISASSOC_IND_FRAME:       return MQTT_TLV_VAL_BINARY;
		case TLV_HIF_DISASSOC_IND_FRAME_LEN:   return MQTT_TLV_VAL_U16;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE) {
		switch (bare) {
		case TLV_HIF_AUTH_TX_COMPLETE_IFNAME:  return MQTT_TLV_VAL_STRING;
		case TLV_HIF_AUTH_TX_COMPLETE_STA_MAC: return MQTT_TLV_VAL_MAC;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE) {
		switch (bare) {
		case TLV_HIF_ASSOC_TX_COMPLETE_IFNAME:  return MQTT_TLV_VAL_STRING;
		case TLV_HIF_ASSOC_TX_COMPLETE_STA_MAC: return MQTT_TLV_VAL_MAC;
		case TLV_HIF_ASSOC_TX_COMPLETE_OK:      return MQTT_TLV_VAL_U8;
		case TLV_HIF_ASSOC_TX_COMPLETE_STATUS:  return MQTT_TLV_VAL_U16;
		case TLV_HIF_ASSOC_TX_COMPLETE_AID:     return MQTT_TLV_VAL_U16;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_EVENT_DEAUTH ||
		   msg_id == EVT_ID_HIF_EVENT_DISASSOC) {
		switch (bare) {
		case TLV_HIF_DEAUTH_DISASSOC_EVT_IFNAME:          return MQTT_TLV_VAL_STRING;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_STA_MAC:         return MQTT_TLV_VAL_MAC;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_ID:         return MQTT_TLV_VAL_S32;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_MAC:        return MQTT_TLV_VAL_MAC;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_REASON_CODE:     return MQTT_TLV_VAL_U16;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_DISCONNECT_TYPE: return MQTT_TLV_VAL_U32;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_IS_TX_STATUS:    return MQTT_TLV_VAL_U8;
		case TLV_HIF_DEAUTH_DISASSOC_EVT_TX_STATUS_OK:    return MQTT_TLV_VAL_U32;
		default: break;
		}
	} else if (msg_id == EVT_ID_HIF_INTERFACE_CREATE) {
		switch (bare) {
		case TLV_HIF_INTERFACE_CREATE_IFNAME: return MQTT_TLV_VAL_STRING;
		default: break;
		}
	}

	return MQTT_TLV_VAL_BINARY;
}

static void print_hif_auth_ind_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_AUTH_IND_IFACE:              printf("IFACE");              break;
	case TLV_HIF_AUTH_IND_STA_MAC:            printf("STA_MAC");            break;
	case TLV_HIF_AUTH_IND_STATUS_CODE:        printf("STATUS_CODE");        break;
	case TLV_HIF_AUTH_IND_AUTH_TRANSACTION:   printf("AUTH_TRANSACTION");   break;
	case TLV_HIF_AUTH_IND_ALLOW_REUSE:        printf("ALLOW_REUSE");        break;
	case TLV_HIF_AUTH_IND_AUTH_ALG:           printf("AUTH_ALG");           break;
	case TLV_HIF_AUTH_IND_RSSI_DBM:           printf("RSSI_DBM");           break;
	case TLV_HIF_AUTH_IND_RX_LINK_ID:         printf("RX_LINK_ID");         break;
	case TLV_HIF_AUTH_IND_STA_ASSOC_LINK_MAC: printf("STA_ASSOC_LINK_MAC"); break;
	case TLV_HIF_AUTH_IND_FRAME:              printf("FRAME");              break;
	case TLV_HIF_AUTH_IND_FRAME_LEN:          printf("FRAME_LEN");          break;
	default:                                   printf("TLV_0x%04x", bare);   break;
	}
}

static void print_hif_assoc_ind_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_ASSOC_IND_IFACE:              printf("IFACE");              break;
	case TLV_HIF_ASSOC_IND_STA_MAC:            printf("STA_MAC");            break;
	case TLV_HIF_ASSOC_IND_STATUS_CODE:        printf("STATUS_CODE");        break;
	case TLV_HIF_ASSOC_IND_IS_REASSOC:         printf("IS_REASSOC");         break;
	case TLV_HIF_ASSOC_IND_RSSI_DBM:           printf("RSSI_DBM");           break;
	case TLV_HIF_ASSOC_IND_RX_LINK_ID:         printf("RX_LINK_ID");         break;
	case TLV_HIF_ASSOC_IND_VALID_LINK_BITMAP:  printf("VALID_LINK_BITMAP");  break;
	case TLV_HIF_ASSOC_IND_STA_ASSOC_LINK_MAC: printf("STA_ASSOC_LINK_MAC"); break;
	case TLV_HIF_ASSOC_IND_STA_LINK_MACS:      printf("STA_LINK_MACS");      break;
	case TLV_HIF_ASSOC_IND_FRAME:              printf("FRAME");              break;
	case TLV_HIF_ASSOC_IND_FRAME_LEN:          printf("FRAME_LEN");          break;
	default:                                    printf("TLV_0x%04x", bare);   break;
	}
}

static void print_hif_deauth_ind_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_DEAUTH_IND_IFACE:       printf("IFACE");       break;
	case TLV_HIF_DEAUTH_IND_STA_MAC:     printf("STA_MAC");     break;
	case TLV_HIF_DEAUTH_IND_RX_LINK_ID:  printf("RX_LINK_ID");  break;
	case TLV_HIF_DEAUTH_IND_REASON_CODE: printf("REASON_CODE"); break;
	case TLV_HIF_DEAUTH_IND_FRAME:       printf("FRAME");        break;
	case TLV_HIF_DEAUTH_IND_FRAME_LEN:  printf("FRAME_LEN");    break;
	default:                              printf("TLV_0x%04x", bare); break;
	}
}

static void print_hif_disassoc_ind_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_DISASSOC_IND_IFACE:       printf("IFACE");       break;
	case TLV_HIF_DISASSOC_IND_STA_MAC:     printf("STA_MAC");     break;
	case TLV_HIF_DISASSOC_IND_RX_LINK_ID:  printf("RX_LINK_ID");  break;
	case TLV_HIF_DISASSOC_IND_REASON_CODE: printf("REASON_CODE"); break;
	case TLV_HIF_DISASSOC_IND_FRAME:       printf("FRAME");        break;
	case TLV_HIF_DISASSOC_IND_FRAME_LEN:   printf("FRAME_LEN");   break;
	default:                                printf("TLV_0x%04x", bare); break;
	}
}

static void print_hif_auth_tx_complete_event_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_AUTH_TX_COMPLETE_IFNAME:  printf("IFNAME");  break;
	case TLV_HIF_AUTH_TX_COMPLETE_STA_MAC: printf("STA_MAC"); break;
	default:                                printf("TLV_0x%04x", bare); break;
	}
}

static void print_hif_assoc_tx_complete_event_name(uint16_t bare)
{
	switch(bare) {
	case TLV_HIF_ASSOC_TX_COMPLETE_OK:      printf("OK"); break;
	case TLV_HIF_ASSOC_TX_COMPLETE_IFNAME:  printf("IFNAME");  break;
	case TLV_HIF_ASSOC_TX_COMPLETE_STA_MAC: printf("STA_MAC"); break;
	case TLV_HIF_ASSOC_TX_COMPLETE_STATUS:  printf("Complete status"); break;
	case TLV_HIF_ASSOC_TX_COMPLETE_AID:     printf("AID"); break;
	default:                                printf("TLV_0x%04x", bare); break;
	}

}
static void print_hif_deauth_disassoc_event_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_DEAUTH_DISASSOC_EVT_IFNAME:          printf("IFNAME");          break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_STA_MAC:         printf("STA_MAC");         break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_ID:         printf("LINK_ID");         break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_MAC:        printf("LINK_MAC");        break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_REASON_CODE:     printf("REASON_CODE");     break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_DISCONNECT_TYPE: printf("DISCONNECT_TYPE"); break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_IS_TX_STATUS:    printf("IS_TX_STATUS");    break;
	case TLV_HIF_DEAUTH_DISASSOC_EVT_TX_STATUS_OK:    printf("TX_STATUS_OK");    break;
	default:                                            printf("TLV_0x%04x", bare); break;
	}
}

static void print_hif_register_policy_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_REGISTER_FRAME_FRAME_TYPE: printf("FRAME_TYPE"); break;
	case TLV_HIF_REGISTER_FRAME_POLICY:     printf("POLICY");     break;
	default:                                  printf("TLV_0x%04x", bare); break;
	}
}

static void print_hif_interface_create_name(uint16_t bare)
{
	switch (bare) {
	case TLV_HIF_INTERFACE_CREATE_IFNAME: printf("IFNAME"); break;
	default:                               printf("TLV_0x%04x", bare); break;
	}
}

static void print_tlv(const struct mqtt_tlv_entry *e, uint16_t msg_id,
		      const char *indent, unsigned int *n)
{
	uint16_t bare = MQTT_TLV_DECODE_TYPE(e->type);
	enum mqtt_tlv_val_type val_type;

	printf("%s[%u] ", indent, (*n)++);

	if (MQTT_TLV_IS_GLOBAL(bare)) {
		printf("MSG_ID             (0x%04x) = ", bare);
		print_value(e, MQTT_TLV_VAL_U16);
		printf("\n");
		return;
	}

	if (MQTT_TLV_IS_CONTAINER(e->type)) {
		struct mqtt_tlv_entry *child;
		unsigned int ci = 0;
		char sub[32];

		printf("Container          (0x%04x)\n", bare);
		snprintf(sub, sizeof(sub), "%s  ", indent);
		dl_list_for_each(child, &e->children, struct mqtt_tlv_entry, list)
			print_tlv(child, msg_id, sub, &ci);
		return;
	}

	val_type = get_hif_tlv_val_type(msg_id, bare);

	if (msg_id == EVT_ID_HIF_NOTIFY_AUTH)
		print_hif_auth_ind_name(bare);
	else if (msg_id == EVT_ID_HIF_NOTIFY_ASSOC)
		print_hif_assoc_ind_name(bare);
	else if (msg_id == EVT_ID_HIF_NOTIFY_DEAUTH)
		print_hif_deauth_ind_name(bare);
	else if (msg_id == EVT_ID_HIF_NOTIFY_DISASSOC)
		print_hif_disassoc_ind_name(bare);
	else if (msg_id == EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE)
		print_hif_auth_tx_complete_event_name(bare);
	else if (msg_id == EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE)
		print_hif_assoc_tx_complete_event_name(bare);
	else if (msg_id == EVT_ID_HIF_EVENT_DEAUTH ||
		 msg_id == EVT_ID_HIF_EVENT_DISASSOC)
		print_hif_deauth_disassoc_event_name(bare);
	else if (msg_id == EVT_ID_HIF_INTERFACE_CREATE)
		print_hif_interface_create_name(bare);
	else if (msg_id == CMD_ID_HIF_REGISTER_FRAME)
		print_hif_register_policy_name(bare);
	else
		printf("TLV_0x%04x", bare);

	printf(" (0x%04x) = ", bare);
	print_value(e, val_type);
	printf("\n");
}

/* -- mosquitto callbacks ------------------------------------------------------- */

static volatile int g_stop = 0;

static void sig_handler(int sig) { (void)sig; g_stop = 1; }

static void send_register_policy(struct mosquitto *mosq, const char *ifname,
				 uint8_t frame_type, uint8_t policy)
{
	struct mqtt_tlv_message *msg;
	uint8_t buf[CMD_BUF_SIZE];
	int len;

	msg = mqtt_tlv_message_alloc(CMD_ID_HIF_REGISTER_FRAME);
	if (!msg) {
		fprintf(stderr, "[MQTT] Failed to alloc REGISTER_FRAME msg\n");
		return;
	}

	if (mqtt_tlv_add_u8(msg, TLV_HIF_REGISTER_FRAME_FRAME_TYPE, frame_type) < 0 ||
	    mqtt_tlv_add_u8(msg, TLV_HIF_REGISTER_FRAME_POLICY, policy) < 0 ||
	    mqtt_tlv_add_string(msg, TLV_HIF_REGISTER_FRAME_IFNAME, ifname) < 0) {
		fprintf(stderr, "[MQTT] Failed to add REGISTER_FRAME TLVs\n");
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, sizeof(buf));
	mqtt_tlv_message_free(msg);

	if (len <= 0) {
		fprintf(stderr, "[MQTT] Failed to serialize REGISTER_FRAME\n");
		return;
	}

	if (mosquitto_publish(mosq, NULL, PUB_TOPIC, len, buf, 1, false)
			!= MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[MQTT] Failed to publish REGISTER_FRAME\n");
		return;
	}

	printf("[MQTT] Sent REGISTER_FRAME to '%s' (ifname=%s frame_type=%u policy=%u)\n",
	       PUB_TOPIC, ifname, frame_type, policy);
}

static void send_register_event(struct mosquitto *mosq, const char *ifname,
				uint8_t event_type, uint8_t set)
{
	struct mqtt_tlv_message *msg;
	uint8_t buf[CMD_BUF_SIZE];
	int len;

	msg = mqtt_tlv_message_alloc(CMD_ID_HIF_REGISTER_EVENT);
	if (!msg) {
		fprintf(stderr, "[MQTT] Failed to alloc REGISTER_EVENT msg\n");
		return;
	}

	if (mqtt_tlv_add_u8(msg, TLV_HIF_REGISTER_EVENT_TYPE, event_type) < 0 ||
	    mqtt_tlv_add_u8(msg, TLV_HIF_REGISTER_EVENT_SET, set) < 0 ||
	    mqtt_tlv_add_string(msg, TLV_HIF_REGISTER_EVENT_IFNAME, ifname) < 0) {
		fprintf(stderr, "[MQTT] Failed to add REGISTER_EVENT TLVs\n");
		mqtt_tlv_message_free(msg);
		return;
	}

	len = mqtt_tlv_serialize(msg, buf, sizeof(buf));
	mqtt_tlv_message_free(msg);

	if (len <= 0) {
		fprintf(stderr, "[MQTT] Failed to serialize REGISTER_EVENT\n");
		return;
	}

	if (mosquitto_publish(mosq, NULL, PUB_TOPIC, len, buf, 1, false)
			!= MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[MQTT] Failed to publish REGISTER_EVENT\n");
		return;
	}

	printf("[MQTT] Sent REGISTER_EVENT to '%s' (ifname=%s event_type=%u set=%u)\n",
	       PUB_TOPIC, ifname, event_type, set);
}

static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
	(void)obj;
	if (rc != 0) {
		fprintf(stderr, "[MQTT] Connection refused (rc=%d)\n", rc);
		return;
	}
	printf("[MQTT] Connected. Subscribing to '%s'\n", SUB_TOPIC);
	mosquitto_subscribe(mosq, NULL, SUB_TOPIC, 1);
}

static void on_subscribe(struct mosquitto *mosq, void *obj, int mid,
			 int qos_count, const int *granted_qos)
{
	(void)mosq; (void)obj;
	printf("[MQTT] SUBACK mid=%d qos=%d  -- listening for HOSTAPD_IF events\n",
	       mid, qos_count > 0 ? granted_qos[0] : -1);
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
	(void)mosq; (void)obj;
	printf("[MQTT] Disconnected (rc=%d)\n", rc);
}

static void on_message(struct mosquitto *mosq, void *obj,
		       const struct mosquitto_message *m)
{
	struct mqtt_tlv_message *parsed;
	struct mqtt_tlv_entry *e;
	uint16_t msg_id;
	unsigned int n = 0;
	time_t now = time(NULL);
	struct tm *tm_inf = localtime(&now);
	char ts[16];

	(void)mosq; (void)obj;

	strftime(ts, sizeof(ts), "%H:%M:%S", tm_inf);

	if (!m->payload || m->payloadlen < 6) {
		printf("[%s] RX topic='%s' payload too short (%d B)\n",
		       ts, m->topic, m->payloadlen);
		return;
	}

	parsed = mqtt_tlv_deserialize((const uint8_t *)m->payload,
				      (size_t)m->payloadlen);
	if (!parsed) {
		printf("[%s] RX topic='%s' deserialization failed (%d B)\n",
		       ts, m->topic, m->payloadlen);
		return;
	}

	msg_id = mqtt_tlv_msg_type(parsed);
	printf("\n[%s] RX topic='%s'  msg_id=0x%04x", ts, m->topic, msg_id);

	switch (msg_id) {
	case EVT_ID_HIF_NOTIFY_AUTH:
		printf(" (EVT_HIF_NOTIFY_AUTH)");
		break;
	case EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE:
		printf(" (EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE)");
		break;
	case EVT_ID_HIF_NOTIFY_ASSOC:
		printf(" (EVT_ID_HIF_NOTIFY_ASSOC)");
		break;
	case EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE:
		printf(" (EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE)");
		break;
	case EVT_ID_HIF_NOTIFY_DEAUTH:
		printf(" (EVT_ID_HIF_NOTIFY_DEAUTH)");
		break;
	case EVT_ID_HIF_NOTIFY_DISASSOC:
		printf(" (EVT_ID_HIF_NOTIFY_DISASSOC)");
		break;
	case EVT_ID_HIF_EVENT_DEAUTH:
		printf(" (EVT_ID_HIF_EVENT_DEAUTH)");
		break;
	case EVT_ID_HIF_EVENT_DISASSOC:
		printf(" (EVT_ID_HIF_EVENT_DISASSOC)");
		break;
	case EVT_ID_HIF_INTERFACE_CREATE: {
		char ifname[64] = {0};

		printf(" (EVT_HIF_INTERFACE_CREATE)");
		if (mqtt_tlv_get_string(parsed, TLV_HIF_INTERFACE_CREATE_IFNAME,
					ifname, sizeof(ifname)) < 0) {
			fprintf(stderr, "[MQTT] INTERFACE_CREATE: missing IFNAME\n");
			break;
		}
		send_register_policy(mosq, ifname, HOSTAPD_IF_FRAME_TYPE_AUTH, HOSTAPD_IF_FRAME_NOTIFY);
		send_register_policy(mosq, ifname, HOSTAPD_IF_FRAME_TYPE_ASSOC, HOSTAPD_IF_FRAME_NOTIFY);
		send_register_policy(mosq, ifname, HOSTAPD_IF_FRAME_TYPE_DEAUTH, HOSTAPD_IF_FRAME_NOTIFY);
		send_register_policy(mosq, ifname, HOSTAPD_IF_FRAME_TYPE_DISASSOC, HOSTAPD_IF_FRAME_NOTIFY);
		send_register_event(mosq, ifname, HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE, 1);
		send_register_event(mosq, ifname, HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE, 1);
		send_register_event(mosq, ifname, HOSTAPD_IF_EVENT_DEAUTH, 1);
		send_register_event(mosq, ifname, HOSTAPD_IF_EVENT_DISASSOC, 1);
		break;
	}
	}

	printf("  %d B\n", m->payloadlen);

	dl_list_for_each(e, &parsed->tlvs, struct mqtt_tlv_entry, list)
		print_tlv(e, msg_id, "  ", &n);

	mqtt_tlv_message_free(parsed);
	fflush(stdout);
}

/* -- main ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	const char *host = (argc > 1) ? argv[1] : BROKER_DEFAULT_HOST;
	int         port = (argc > 2) ? atoi(argv[2]) : BROKER_DEFAULT_PORT;
	struct mosquitto *mosq;
	char client_id[64];
	int rc = MOSQ_ERR_SUCCESS;
	int saved_errno = 0;

	signal(SIGINT,  sig_handler);
	signal(SIGTERM, sig_handler);

	snprintf(client_id, sizeof(client_id), "hif-sub-%d", getpid());

	mosquitto_lib_init();

	/* Outer loop: connect/reconnect to broker */
	while (!g_stop) {
		struct ifreq ifr;
		int fd;
		int up = 0;

		fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd >= 0) {
			memset(&ifr, 0, sizeof(ifr));
			snprintf(ifr.ifr_name, IFNAMSIZ, "%s", "lo");
			if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0)
				up = !!(ifr.ifr_flags & IFF_UP);
			close(fd);
		}

		if (!up) {
			fprintf(stderr,
				"[MQTT] Loopback interface 'lo' is down; retrying in 1 second\n");
			sleep(1);
			continue;
		}

		mosq = mosquitto_new(client_id, true, NULL);
		if (!mosq) {
			fprintf(stderr, "[ERR] mosquitto_new: %s\n", strerror(errno));
			rc = MOSQ_ERR_NOMEM;
			break;
		}

		mosquitto_connect_callback_set(mosq, on_connect);
		mosquitto_disconnect_callback_set(mosq, on_disconnect);
		mosquitto_message_callback_set(mosq, on_message);
		mosquitto_subscribe_callback_set(mosq, on_subscribe);

		printf("[MQTT] Connecting to %s:%d as '%s'...\n", host, port,
		       client_id);
		rc = mosquitto_connect(mosq, host, port, BROKER_KEEPALIVE);
		if (rc != MOSQ_ERR_SUCCESS) {
			fprintf(stderr,
				"[ERR] connect failed: %s; retrying in 1 second\n",
				rc == MOSQ_ERR_ERRNO ? strerror(errno) :
				mosquitto_strerror(rc));
			mosquitto_destroy(mosq);
			sleep(1);
			continue;
		}

		/* Inner loop: drive MQTT I/O; break on connection error to reconnect */
		while (!g_stop) {
			rc = mosquitto_loop(mosq, 1000, 1);
			if (rc == MOSQ_ERR_SUCCESS)
				continue;

			if (rc == MOSQ_ERR_ERRNO &&
			    (errno == EINTR || errno == EAGAIN ||
			     errno == EWOULDBLOCK))
				continue;

			break;
		}

		saved_errno = errno;
		if (g_stop) {
			mosquitto_disconnect(mosq);
			mosquitto_loop(mosq, 100, 1);
		}
		mosquitto_destroy(mosq);

		if (g_stop)
			break;

		fprintf(stderr,
			"[MQTT] MQTT loop ended: %s; retrying in 1 second\n",
			rc == MOSQ_ERR_ERRNO ? strerror(saved_errno) :
			mosquitto_strerror(rc));
		sleep(1);
	}

	printf("\n[MQTT] Disconnecting...\n");
	mosquitto_lib_cleanup();
	return rc == MOSQ_ERR_NOMEM ? 1 : 0;
}
