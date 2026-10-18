/*
 * mqtt_feature_map.h - MQTT message IDs and feature routing for hostapd
 *
 * Two-header public API:
 *   mqtt_feature_map.h  ← this file (message IDs, topics, routing, policy)
 *   mqtt_tlv_map.h      (TLV implementation, per-message enums — included below)
 *
 * Self-contained: only <stdint.h>, <stddef.h>, <string.h>, <stdio.h> required.
 * All functions are static / static inline — no link-time dependency.
 *
 *
 * ── Message ID Bit Layout (16-bit) ──────────────────────────────────────────
 *
 *  15  | 14  13  12  11  10   9   8 |  7   6   5   4   3   2   1   0
 * ─────┼───────────────────────────┼──────────────────────────────────
 * DIR  |       FEATURE  (7-bit)    |       OPERATION  (8-bit)
 *
 *  DIR [15]  0 = CMD (app→hostapd)    range 0x0001–0x7FFF
 *            1 = EVT (hostapd→app)    range 0x8001–0xFFFF
 *  FEATURE [14:8]  2=SMD  3=MLME  4=MAPC  15=SYS
 *  OPERATION [7:0] 0x00=INVALID  0x01–0xFF usable
 *
 *
 * ── Developer Guide ─────────────────────────────────────────────────────────
 *
 * Adding a new feature:
 *   1. Add MQTT_FEAT_* nibble and MQTT_FEATURE_* string.
 *   2. Add CMD_ID_<FEAT>_* / EVT_ID_<FEAT>_* to the enums below.
 *   3. In mqtt_tlv_map.h: add per-message TLV enums (TLV_<FEAT>_<MSG>_*).
 *   4. Add a static per-feature msg ID array and register it in
 *      mqtt_feature_table[].
 *   5. Add CMD policy entries to mqtt_cmd_policy_table[].
 *   6. Add a per-feature handler in hostapd.c and wire it into
 *      hostapd_mqtt_msg_cb() via MQTT_MSG_FEATURE(msg_type).
 *
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MQTT_FEATURE_MAP_H
#define MQTT_FEATURE_MAP_H

#ifdef CONFIG_MQTT

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <common.h>

/*
 * mqtt_tlv_map.h is self-contained and has no dependency on the message IDs
 * defined below — it can be included first safely.
 */
#include "mqtt_tlv_map.h"

/* ── Topic root prefixes ────────────────────────────────────────────────── */

#define MQTT_TOPIC_RECEIVE   "hostapd/receive"
#define MQTT_TOPIC_TRANSMIT  "hostapd/transmit"

/* ── Feature string constants ───────────────────────────────────────────── */

#define MQTT_FEATURE_SMD   "SMD"   /* Station Management & Discovery   */
#define MQTT_FEATURE_MLME  "MLME"  /* MAC Layer Management Entity      */
#define MQTT_FEATURE_MAPC  "MAPC"  /* Multi-AP Controller (reserved)   */
#define MQTT_FEATURE_HOSTAPD_IF  "HOSTAPD_IF"  /* hostapd southbound IF events     */
#define MQTT_FEATURE_SYS   "SYS"   /* hostapd system lifecycle         */

/* ── Feature nibble constants ───────────────────────────────────────────── */

enum mqtt_feat_nibble {
	MQTT_FEAT_SMD  = 2,   /* 0x02 */
	MQTT_FEAT_MLME = 3,   /* 0x03 */
	MQTT_FEAT_MAPC = 4,   /* 0x04 — reserved for future use */
	MQTT_FEAT_HOSTAPD_IF  = 5,   /* 0x05 */
	/* 6–14: available for new features */
	MQTT_FEAT_SYS = 15,  /* 0x0F */
};

/* ── Message ID constructors ────────────────────────────────────────────── */

#define MQTT_CMD_ID(feat, op) \
	((uint16_t)(((feat) << 8) | ((op) & 0xFFu)))

#define MQTT_EVT_ID(feat, op) \
	((uint16_t)(0x8000u | ((feat) << 8) | ((op) & 0xFFu)))

#define MQTT_MSG_IS_CMD(id)    (!((id) & 0x8000u))
#define MQTT_MSG_IS_EVT(id)    (!!((id) & 0x8000u))
#define MQTT_MSG_FEATURE(id)   (((id) >> 8) & 0x7Fu)
#define MQTT_MSG_OP(id)        ((id) & 0xFFu)

/* ── Command IDs  (external app → hostapd) ──────────────────────────────── */

enum mqtt_cmd_id {
	/* SYS — System lifecycle */
	CMD_ID_SYS_PING = MQTT_CMD_ID(MQTT_FEAT_SYS, 1), /* 0x0F01 */

	/* SMD NeighborDB commands */
	CMD_ID_NEIGHBOR_DB_SET   = MQTT_CMD_ID(MQTT_FEAT_SMD, 1), /* 0x0201 */
	CMD_ID_NEIGHBOR_DB_GET   = MQTT_CMD_ID(MQTT_FEAT_SMD, 2), /* 0x0202 */
	CMD_ID_NEIGHBOR_DB_CLEAR = MQTT_CMD_ID(MQTT_FEAT_SMD, 3), /* 0x0203 */

	/* HOSTAPD_IF — southbound interface commands */
	CMD_ID_HIF_REGISTER_FRAME = MQTT_CMD_ID(MQTT_FEAT_HOSTAPD_IF, 1), /* 0x0501 */
	CMD_ID_HIF_REGISTER_EVENT  = MQTT_CMD_ID(MQTT_FEAT_HOSTAPD_IF, 2), /* 0x0502 */
};

/* ── Event IDs  (hostapd → external app) ───────────────────────────────── */

enum mqtt_evt_id {
	/* SYS — System lifecycle */
	EVT_ID_SYS_HOSTAPD_STARTED = MQTT_EVT_ID(MQTT_FEAT_SYS, 1), /* 0x8F01 */
	EVT_ID_SYS_HOSTAPD_STOPPED = MQTT_EVT_ID(MQTT_FEAT_SYS, 2), /* 0x8F02 */
	EVT_ID_CONFIG_RELOADED     = MQTT_EVT_ID(MQTT_FEAT_SYS, 3), /* 0x8F03 */

	/* SMD NeighborDB responses (EZHIF low-16 values) */
	EVT_ID_NEIGHBOR_DB_SET_RESP   = 0x0081,
	EVT_ID_NEIGHBOR_DB_GET_RESP   = 0x0082,
	EVT_ID_NEIGHBOR_DB_CLEAR_RESP = 0x0083,

	/* HOSTAPD_IF — southbound interface events */
	EVT_ID_HIF_INTERFACE_CREATE        = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 1), /* 0x8501 */
	EVT_ID_HIF_NOTIFY_AUTH             = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 2), /* 0x8502 */
	EVT_ID_HIF_NOTIFY_ASSOC            = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 3), /* 0x8503 */
	EVT_ID_HIF_NOTIFY_DEAUTH           = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 4), /* 0x8504 */
	EVT_ID_HIF_NOTIFY_DISASSOC         = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 5), /* 0x8505 */
	EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 6), /* 0x8506 */
	EVT_ID_HIF_EVENT_DEAUTH            = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 7), /* 0x8507 */
	EVT_ID_HIF_EVENT_DISASSOC          = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 8), /* 0x8508 */
	EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE  = MQTT_EVT_ID(MQTT_FEAT_HOSTAPD_IF, 9), /* 0x8509 */
};

/* ── Feature routing table ──────────────────────────────────────────────── */

struct mqtt_feature_entry {
	const char     *feature;
	const uint16_t *msg_ids;
	size_t          num_msgs;
};

static const uint16_t mqtt_smd_msg_ids[] = {
	(uint16_t)CMD_ID_NEIGHBOR_DB_SET,
	(uint16_t)CMD_ID_NEIGHBOR_DB_GET,
	(uint16_t)CMD_ID_NEIGHBOR_DB_CLEAR,
	(uint16_t)EVT_ID_NEIGHBOR_DB_SET_RESP,
	(uint16_t)EVT_ID_NEIGHBOR_DB_GET_RESP,
	(uint16_t)EVT_ID_NEIGHBOR_DB_CLEAR_RESP,
};
static const uint16_t mqtt_mlme_msg_ids[] = { 0 };

/* MAPC: feature reserved for future use — no messages currently. */
static const uint16_t mqtt_mapc_msg_ids[] = { 0 };

static const uint16_t mqtt_sys_msg_ids[] = {
	(uint16_t)CMD_ID_SYS_PING,
	(uint16_t)EVT_ID_SYS_HOSTAPD_STARTED,
	(uint16_t)EVT_ID_SYS_HOSTAPD_STOPPED,
	(uint16_t)EVT_ID_CONFIG_RELOADED,
};

static const uint16_t mqtt_hostapd_if_msg_ids[] = {
	(uint16_t)EVT_ID_HIF_INTERFACE_CREATE,
	(uint16_t)EVT_ID_HIF_NOTIFY_AUTH,
	(uint16_t)EVT_ID_HIF_NOTIFY_ASSOC,
	(uint16_t)EVT_ID_HIF_NOTIFY_DEAUTH,
	(uint16_t)EVT_ID_HIF_NOTIFY_DISASSOC,
	(uint16_t)EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE,
	(uint16_t)EVT_ID_HIF_EVENT_DEAUTH,
	(uint16_t)EVT_ID_HIF_EVENT_DISASSOC,
	(uint16_t)EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE,
	(uint16_t)CMD_ID_HIF_REGISTER_FRAME,
	(uint16_t)CMD_ID_HIF_REGISTER_EVENT,
};

#define MQTT_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const struct mqtt_feature_entry mqtt_feature_table[] = {
	{ MQTT_FEATURE_SMD,  mqtt_smd_msg_ids,  MQTT_ARRAY_SIZE(mqtt_smd_msg_ids) },
	{ MQTT_FEATURE_MLME, mqtt_mlme_msg_ids, 0 /* reserved, no active msgs */ },
	{ MQTT_FEATURE_MAPC, mqtt_mapc_msg_ids, 0 /* reserved, no active msgs */ },
	{ MQTT_FEATURE_HOSTAPD_IF, mqtt_hostapd_if_msg_ids, MQTT_ARRAY_SIZE(mqtt_hostapd_if_msg_ids) },
	{ MQTT_FEATURE_SYS,  mqtt_sys_msg_ids,  MQTT_ARRAY_SIZE(mqtt_sys_msg_ids)  },
};

static const size_t mqtt_num_features = MQTT_ARRAY_SIZE(mqtt_feature_table);

/* ── Feature routing API (static inline) ───────────────────────────────── */

static inline const struct mqtt_feature_entry *
mqtt_feature_lookup(const char *feature)
{
	size_t i;

	if (!feature)
		return NULL;
	for (i = 0; i < mqtt_num_features; i++)
		if (strcmp(mqtt_feature_table[i].feature, feature) == 0)
			return &mqtt_feature_table[i];
	return NULL;
}

static inline const char *mqtt_feature_from_msg_id(uint16_t msg_id)
{
	size_t i, j;
	for (i = 0; i < mqtt_num_features; i++)
		for (j = 0; j < mqtt_feature_table[i].num_msgs; j++)
			if (mqtt_feature_table[i].msg_ids[j] == msg_id)
				return mqtt_feature_table[i].feature;
	return NULL;
}

static inline int
mqtt_feature_validate_cmd(const char *feature, uint16_t msg_id)
{
	const struct mqtt_feature_entry *fe;
	size_t j;

	fe = mqtt_feature_lookup(feature);
	if (!fe)
		return -1;
	for (j = 0; j < fe->num_msgs; j++)
		if (fe->msg_ids[j] == msg_id)
			return 0;
	return -1;
}

static inline const char *mqtt_feature_from_topic(const char *topic)
{
	const char *rx = MQTT_TOPIC_RECEIVE "/";
	const char *tx = MQTT_TOPIC_TRANSMIT "/";
	size_t rx_len = strlen(rx), tx_len = strlen(tx);

	if (!topic)
		return NULL;
	if (strncmp(topic, rx, rx_len) == 0 && topic[rx_len] != '\0')
		return topic + rx_len;
	if (strncmp(topic, tx, tx_len) == 0 && topic[tx_len] != '\0')
		return topic + tx_len;
	return NULL;
}

static inline int
mqtt_build_receive_topic(const char *feature, char *buf, size_t len)
{
	return snprintf(buf, len, "%s/%s", MQTT_TOPIC_RECEIVE, feature);
}

static inline int
mqtt_build_transmit_topic(const char *feature, char *buf, size_t len)
{
	return snprintf(buf, len, "%s/%s", MQTT_TOPIC_TRANSMIT, feature);
}

/* ── CMD policy tables (static const) ──────────────────────────────────── */

/* CMD_ID_SYS_PING: no payload TLVs */
static const struct mqtt_tlv_policy mqtt_pol_sys_ping[1] = {
	{ 0, MQTT_TLV_VAL_NONE, 0, 0, 0, 0 }
};

/* CMD_ID_HIF_REGISTER_POLICY */
static const struct mqtt_tlv_policy mqtt_pol_hif_register_frame[] = {
	{ TLV_HIF_REGISTER_FRAME_FRAME_TYPE, MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_HIF_REGISTER_FRAME_POLICY,     MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_HIF_REGISTER_FRAME_IFNAME,     MQTT_TLV_VAL_STRING, IFNAMSIZ - 1, 1, 0, 0 },
};

/* CMD_ID_HIF_REGISTER_EVENT */
static const struct mqtt_tlv_policy mqtt_pol_hif_register_event[] = {
	{ TLV_HIF_REGISTER_EVENT_TYPE,   MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_HIF_REGISTER_EVENT_SET,    MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_HIF_REGISTER_EVENT_IFNAME, MQTT_TLV_VAL_STRING, IFNAMSIZ - 1, 1, 0, 0 },
};

static const struct mqtt_tlv_policy mqtt_pol_neighbor_db_set[] = {
	{ TLV_NEIGHBOR_DB_SET_AP_ALID,      MQTT_TLV_VAL_MAC,    6, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_HAS_SMD_ID,   MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_SMD_ID,       MQTT_TLV_VAL_MAC,    6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_NUM_NRE,      MQTT_TLV_VAL_U16,    2, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_HAS_MLD_ADDR, MQTT_TLV_VAL_U8,     1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_MLD_ADDR,     MQTT_TLV_VAL_MAC,    6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_BSSID,        MQTT_TLV_VAL_MAC,    6, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_NRE_LEN,      MQTT_TLV_VAL_U16,    2, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_SET_NRE,          MQTT_TLV_VAL_BINARY, 0, 1, 0, 0 },
};

static const struct mqtt_tlv_policy mqtt_pol_neighbor_db_get[] = {
	{ TLV_NEIGHBOR_DB_GET_AP_ALID,         MQTT_TLV_VAL_MAC, 6, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_HAS_SMD_ID,      MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_SMD_ID,          MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_GLOBAL_ENTRIES,  MQTT_TLV_VAL_U8,  1, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_HAS_MLD_ADDR,    MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_MLD_ADDR,        MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_HAS_BSSID,       MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_GET_BSSID,           MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
};

static const struct mqtt_tlv_policy mqtt_pol_neighbor_db_clear[] = {
	{ TLV_NEIGHBOR_DB_CLEAR_AP_ALID,      MQTT_TLV_VAL_MAC, 6, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_HAS_SMD_ID,   MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_SMD_ID,       MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_HAS_MLD_ADDR, MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_MLD_ADDR,     MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_HAS_BSSID,    MQTT_TLV_VAL_U8,  1, 1, 0, 0 },
	{ TLV_NEIGHBOR_DB_CLEAR_BSSID,        MQTT_TLV_VAL_MAC, 6, 0, 0, 0 },
};

/* ── CMD policy registry ────────────────────────────────────────────────── */

static const struct mqtt_msg_policy mqtt_cmd_policy_table[] = {
	{ (uint16_t)CMD_ID_SYS_PING, mqtt_pol_sys_ping, 0 },
	{ (uint16_t)CMD_ID_NEIGHBOR_DB_SET, mqtt_pol_neighbor_db_set,
	  (uint8_t)MQTT_ARRAY_SIZE(mqtt_pol_neighbor_db_set) },
	{ (uint16_t)CMD_ID_NEIGHBOR_DB_GET, mqtt_pol_neighbor_db_get,
	  (uint8_t)MQTT_ARRAY_SIZE(mqtt_pol_neighbor_db_get) },
	{ (uint16_t)CMD_ID_NEIGHBOR_DB_CLEAR, mqtt_pol_neighbor_db_clear,
	  (uint8_t)MQTT_ARRAY_SIZE(mqtt_pol_neighbor_db_clear) },
	{ (uint16_t)CMD_ID_HIF_REGISTER_FRAME, mqtt_pol_hif_register_frame,
	  (uint8_t)MQTT_ARRAY_SIZE(mqtt_pol_hif_register_frame) },
	{ (uint16_t)CMD_ID_HIF_REGISTER_EVENT, mqtt_pol_hif_register_event,
	  (uint8_t)MQTT_ARRAY_SIZE(mqtt_pol_hif_register_event) },
};

static const size_t mqtt_num_cmd_policies =
	MQTT_ARRAY_SIZE(mqtt_cmd_policy_table);

/* ── Policy lookup (static inline) ─────────────────────────────────────── */

static inline const struct mqtt_tlv_policy *
mqtt_tlv_policy_lookup(uint16_t msg_id, uint16_t tlv_id)
{
	size_t i, j;
	for (i = 0; i < mqtt_num_cmd_policies; i++) {
		if (mqtt_cmd_policy_table[i].msg_id != msg_id)
			continue;
		for (j = 0; j < mqtt_cmd_policy_table[i].num_tlvs; j++)
			if (mqtt_cmd_policy_table[i].tlvs[j].tlv_id == tlv_id)
				return &mqtt_cmd_policy_table[i].tlvs[j];
		return NULL;
	}
	return NULL;
}

/* ── Policy validation (implemented in mqtt_feature_map.c) ─────────────── */
/*
 * Validates incoming CMD TLVs against the per-command policy table.
 * Returns 0 if valid (or no policy registered), -1 otherwise.
 * Global TLVs (MSG_ID) are skipped — they are frame metadata, not payload.
 * Only incoming CMDs are validated; EVTs are not.
 */
static inline int
mqtt_msg_validate_if_policy(uint16_t msg_id, struct mqtt_tlv_message *msg)
{
	const struct mqtt_msg_policy *mp = NULL;
	const struct mqtt_tlv_entry  *e;
	size_t i, j;
	int    found;

	for (i = 0; i < mqtt_num_cmd_policies; i++) {
		if (mqtt_cmd_policy_table[i].msg_id == msg_id) {
			mp = &mqtt_cmd_policy_table[i];
			break;
		}
	}
	if (!mp)
		return 0;

	if (mp->num_tlvs == 0) {
		dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list)
			if (!MQTT_TLV_IS_GLOBAL(MQTT_TLV_DECODE_TYPE(e->type)))
				return -1;
		return 0;
	}

	/* Pass 1: every non-global TLV must be in the policy */
	dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list) {
		const struct mqtt_tlv_policy *pol = NULL;
		uint16_t bare = MQTT_TLV_DECODE_TYPE(e->type);
		if (MQTT_TLV_IS_GLOBAL(bare))
			continue;
		for (j = 0; j < mp->num_tlvs; j++)
			if (mp->tlvs[j].tlv_id == bare) {
				pol = &mp->tlvs[j];
				break;
			}
		if (!pol)
			return -1;
		switch (pol->val_type) {
		case MQTT_TLV_VAL_U8:
			if (e->length != 1)
				return -1;
			break;
		case MQTT_TLV_VAL_U16:
			if (e->length != 2)
				return -1;
			break;
		case MQTT_TLV_VAL_U32:
			if (e->length != 4)
				return -1;
			break;
		case MQTT_TLV_VAL_S32:
			if (e->length != 4)
				return -1;
			break;
		case MQTT_TLV_VAL_MAC:
			if (pol->len && e->length != pol->len)
				return -1;
			break;
		case MQTT_TLV_VAL_BINARY:
		case MQTT_TLV_VAL_STRING:
			if (pol->len && e->length > pol->len)
				return -1;
			break;
		default:
			break;
		}
	}

	/* Pass 2: all mandatory TLVs must be present */
	for (j = 0; j < mp->num_tlvs; j++) {
		if (!mp->tlvs[j].mandatory)
			continue;
		found = 0;
		dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list)
			if (MQTT_TLV_DECODE_TYPE(e->type) == mp->tlvs[j].tlv_id) {
				found = 1;
				break;
			}
		if (!found)
			return -1;
	}
	return 0;
}

#endif /* CONFIG_MQTT */
#endif /* MQTT_FEATURE_MAP_H */
