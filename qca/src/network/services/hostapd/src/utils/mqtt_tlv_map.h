/*
 * mqtt_tlv_map.h - Complete TLV implementation and type definitions
 *                  for the hostapd MQTT interface
 *
 * Self-contained header; requires only <stdint.h>, <stdbool.h>, <stddef.h>,
 * <stdlib.h>, <string.h>.  No hostapd-internal includes needed.
 *
 * Include mqtt_feature_map.h (which includes this file automatically) to
 * obtain the complete protocol API.
 *
 *
 * ── Wire Format ─────────────────────────────────────────────────────────────
 *
 *   [TLV_GLOBAL_MSG_ID:2B][0x0002:2B][msg_type:2B] ← mandatory 6-byte header
 *   [payload TLV 1][payload TLV 2]...
 *
 * Leaf TLV:       [type:2B][length:2B][value:length bytes]
 * Container TLV:  [base_type|CF:2B][inner_length:2B][inner TLVs...]
 * All multi-byte fields are big-endian (network byte order, RFC 1700).
 * No alignment padding — each TLV is byte-contiguous; next TLV starts
 * immediately after the last value byte.
 *
 *
 * ── TLV Type ID Bit Layout (16-bit stored constants) ────────────────────────
 *
 *   15  |  14  |  13  |  12  11  10   9   8   7   6   5   4   3   2   1   0
 *  ─────┼───────┼───────┼──────────────────────────────────────────────────
 *   GF  |  CF  |  IF  |                   INDEX  (13-bit)
 *  ─────┴───────┴───────┴──────────────────────────────────────────────────
 *
 *  GF (bit 15 = 0x8000): Global flag — frame-level metadata, top-level only.
 *                         Present in constants AND wire.
 *  CF (bit 14 = 0x4000): Container wire flag — 0 in constants; encoder sets 1.
 *  IF (bit 13 = 0x2000): Inner flag — only valid inside a container.
 *                         Present in constants AND wire.
 *  INDEX [12:0]:           Per-message decimal index, 1-based (0 = INVALID).
 *
 *  Category table:
 *   Outer leaf:    0x0001–0x1FFF  (GF=0, CF=0, IF=0)
 *   Inner leaf:    0x2001–0x3FFF  (GF=0, CF=0, IF=1) via MQTT_TLV_INNER(n)
 *   Container wire: 0x4001–0x5FFF (GF=0, CF=1, wire only)
 *   Global TLV:    0x8001–0x9FFF  (GF=1, CF=0, IF=0) via MQTT_TLV_GLOBAL(n)
 *
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MQTT_TLV_MAP_H
#define MQTT_TLV_MAP_H

#ifdef CONFIG_MQTT

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ── Optional logging ───────────────────────────────────────────────────── */
#ifndef MQTT_TLV_LOG_WARN
#define MQTT_TLV_LOG_WARN(fmt, ...)  ((void)0)
#endif
#ifndef MQTT_TLV_LOG_DEBUG
#define MQTT_TLV_LOG_DEBUG(fmt, ...) ((void)0)
#endif

/* ── Portable doubly-linked list ────────────────────────────────────────── */
/* Skipped if hostapd's list.h is already loaded (it defines LIST_H). */
#ifndef LIST_H
#define LIST_H
struct dl_list { struct dl_list *next; struct dl_list *prev; };
static inline void dl_list_init(struct dl_list *l)
{
	l->next = l;
	l->prev = l;
}
static inline void dl_list_add(struct dl_list *list, struct dl_list *item)
{
	item->next = list->next;
	item->prev = list;
	list->next->prev = item;
	list->next = item;
}
static inline void dl_list_add_tail(struct dl_list *list, struct dl_list *item)
{
	dl_list_add(list->prev, item);
}
static inline void dl_list_del(struct dl_list *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	item->next = NULL;
	item->prev = NULL;
}
static inline int dl_list_empty(const struct dl_list *list)
{
	return list->next == list;
}
#define dl_list_entry(item, type, member) \
	((type *)((char *)(item) - offsetof(type, member)))
#define dl_list_for_each(item, list, type, member) \
	for ((item) = dl_list_entry((list)->next, type, member); \
	     &(item)->member != (list); \
	     (item) = dl_list_entry((item)->member.next, type, member))
#define dl_list_for_each_safe(item, n, list, type, member) \
	for ((item) = dl_list_entry((list)->next, type, member), \
	     (n)    = dl_list_entry((item)->member.next, type, member); \
	     &(item)->member != (list); \
	     (item) = (n), \
	     (n)    = dl_list_entry((n)->member.next, type, member))
#endif /* LIST_H */

/* ── TLV type-flag constants ────────────────────────────────────────────── */

#define MQTT_TLV_GLOBAL_FLAG     0x8000  /* bit 15 */
#define MQTT_TLV_CONTAINER_FLAG  0x4000  /* bit 14 — wire-only */
#define MQTT_TLV_INNER_FLAG      0x2000  /* bit 13 */

#define MQTT_TLV_GLOBAL(n)       (MQTT_TLV_GLOBAL_FLAG | (n))
#define MQTT_TLV_INNER(n)        (MQTT_TLV_INNER_FLAG  | (n))

#define MQTT_TLV_MAX_DEPTH  4
#define MQTT_TLV_MAC_LEN    6

#define MQTT_TLV_ENCODE_CONTAINER(t) \
	((uint16_t)((t) | MQTT_TLV_CONTAINER_FLAG))
#define MQTT_TLV_DECODE_TYPE(wt) \
	((uint16_t)((wt) & ~(uint16_t)MQTT_TLV_CONTAINER_FLAG))
#define MQTT_TLV_IS_GLOBAL(t)      (!!((t) & MQTT_TLV_GLOBAL_FLAG))
#define MQTT_TLV_IS_CONTAINER(wt)  (!!((wt) & MQTT_TLV_CONTAINER_FLAG))
#define MQTT_TLV_IS_INNER(t)       (!!((t)  & MQTT_TLV_INNER_FLAG))

/* ── Global TLV definitions ─────────────────────────────────────────────── */

enum mqtt_global_tlv_type {
	TLV_GLOBAL_MSG_ID = MQTT_TLV_GLOBAL(1),  /* 0x8001 — 2B message type */
	_TLV_GLOBAL_MAX   = MQTT_TLV_GLOBAL(2)
};

/* ── Data structures ────────────────────────────────────────────────────── */

struct mqtt_tlv_entry {
	uint16_t       type;
	uint16_t       length;
	uint8_t       *value;
	struct dl_list children;
	struct dl_list list;
};

struct mqtt_tlv_message {
	struct dl_list tlvs;   /* complete TLV list; TLV_GLOBAL_MSG_ID is first */
};

/* ── Big-endian byte I/O ────────────────────────────────────────────────── */

static inline void mqtt_put_be16(uint8_t *b, uint16_t v)
{
	b[0] = (uint8_t)((v >> 8) & 0xFFu);
	b[1] = (uint8_t)(v & 0xFFu);
}
static inline uint16_t mqtt_get_be16(const uint8_t *b)
{
	return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}
static inline void mqtt_put_be32(uint8_t *b, uint32_t v)
{
	b[0] = (uint8_t)((v >> 24) & 0xFFu);
	b[1] = (uint8_t)((v >> 16) & 0xFFu);
	b[2] = (uint8_t)((v >>  8) & 0xFFu);
	b[3] = (uint8_t)(v & 0xFFu);
}
static inline uint32_t mqtt_get_be32(const uint8_t *b)
{
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static inline void mqtt_put_be64(uint8_t *b, uint64_t v)
{
	b[0] = (uint8_t)((v >> 56) & 0xFFu);
	b[1] = (uint8_t)((v >> 48) & 0xFFu);
	b[2] = (uint8_t)((v >> 40) & 0xFFu);
	b[3] = (uint8_t)((v >> 32) & 0xFFu);
	b[4] = (uint8_t)((v >> 24) & 0xFFu);
	b[5] = (uint8_t)((v >> 16) & 0xFFu);
	b[6] = (uint8_t)((v >>  8) & 0xFFu);
	b[7] = (uint8_t)(v & 0xFFu);
}
static inline uint64_t mqtt_get_be64(const uint8_t *b)
{
	return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
	       ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
	       ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
	       ((uint64_t)b[6] <<  8) | (uint64_t)b[7];
}

/* ── Internal entry helpers ─────────────────────────────────────────────── */

static struct mqtt_tlv_entry *
mqtt_tlv_entry_alloc_leaf(uint16_t type, const uint8_t *value, uint16_t length)
{
	struct mqtt_tlv_entry *e;
	if (MQTT_TLV_IS_CONTAINER(type))
		return NULL;
	e = calloc(1, sizeof(*e));
	if (!e)
		return NULL;
	e->type = type;
	e->length = length;
	dl_list_init(&e->children);
	if (length > 0) {
		e->value = malloc(length);
		if (!e->value) {
			free(e);
			return NULL;
		}
		memcpy(e->value, value, length);
	}
	return e;
}
static struct mqtt_tlv_entry *
mqtt_tlv_entry_alloc_container(uint16_t base_type)
{
	struct mqtt_tlv_entry *e = calloc(1, sizeof(*e));

	if (!e)
		return NULL;
	e->type = MQTT_TLV_ENCODE_CONTAINER(MQTT_TLV_DECODE_TYPE(base_type));
	dl_list_init(&e->children);
	return e;
}
static void mqtt_tlv_entry_free(struct mqtt_tlv_entry *e)
{
	struct mqtt_tlv_entry *child, *tmp;
	if (!e)
		return;
	if (MQTT_TLV_IS_CONTAINER(e->type)) {
		dl_list_for_each_safe(child, tmp, &e->children, struct mqtt_tlv_entry, list) {
			dl_list_del(&child->list);
			mqtt_tlv_entry_free(child);
		}
	} else {
		free(e->value);
	}
	free(e);
}
static int mqtt_tlv_list_add_leaf(struct dl_list *list,
                                   uint16_t type, const uint8_t *value,
                                   uint16_t length)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_entry_alloc_leaf(type, value, length);

	if (!e)
		return -1;
	dl_list_add_tail(list, &e->list);
	return 0;
}
static struct mqtt_tlv_entry *
mqtt_tlv_list_add_container(struct dl_list *list, uint16_t base_type)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_entry_alloc_container(base_type);

	if (!e)
		return NULL;
	dl_list_add_tail(list, &e->list);
	return e;
}
static struct mqtt_tlv_entry *
mqtt_tlv_list_find_leaf(const struct dl_list *list, uint16_t type)
{
	struct mqtt_tlv_entry *e;
	dl_list_for_each(e, list, struct mqtt_tlv_entry, list)
		if (!MQTT_TLV_IS_CONTAINER(e->type) && e->type == type)
			return e;
	return NULL;
}

/* ── Message lifecycle ──────────────────────────────────────────────────── */

static inline struct mqtt_tlv_message *mqtt_tlv_message_alloc(uint16_t msg_type)
{
	struct mqtt_tlv_message *msg = calloc(1, sizeof(*msg));
	uint8_t id_buf[2];

	if (!msg)
		return NULL;
	dl_list_init(&msg->tlvs);
	mqtt_put_be16(id_buf, msg_type);
	if (mqtt_tlv_list_add_leaf(&msg->tlvs, TLV_GLOBAL_MSG_ID, id_buf, 2) != 0) {
		free(msg);
		return NULL;
	}
	return msg;
}
static inline void mqtt_tlv_message_free(struct mqtt_tlv_message *msg)
{
	struct mqtt_tlv_entry *e, *tmp;
	if (!msg)
		return;
	dl_list_for_each_safe(e, tmp, &msg->tlvs, struct mqtt_tlv_entry, list) {
		dl_list_del(&e->list);
		mqtt_tlv_entry_free(e);
	}
	free(msg);
}
static inline uint16_t mqtt_tlv_msg_type(const struct mqtt_tlv_message *msg)
{
	const struct mqtt_tlv_entry *first;
	if (!msg || dl_list_empty(&msg->tlvs))
		return 0;
	first = dl_list_entry(msg->tlvs.next, struct mqtt_tlv_entry, list);
	if (first->type != (uint16_t)TLV_GLOBAL_MSG_ID || first->length != 2 || !first->value)
		return 0;
	return mqtt_get_be16(first->value);
}

/* ── Leaf TLV — add ─────────────────────────────────────────────────────── */

static inline int mqtt_tlv_add_u8(struct mqtt_tlv_message *msg, uint16_t type, uint8_t v)
{
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, &v, 1);
}
static inline int mqtt_tlv_add_u16(struct mqtt_tlv_message *msg, uint16_t type, uint16_t v)
{
	uint8_t b[2];

	mqtt_put_be16(b, v);
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, b, 2);
}
static inline int mqtt_tlv_add_u32(struct mqtt_tlv_message *msg, uint16_t type, uint32_t v)
{
	uint8_t b[4];

	mqtt_put_be32(b, v);
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, b, 4);
}
static inline int mqtt_tlv_add_s32(struct mqtt_tlv_message *msg, uint16_t type, int32_t v)
{
	uint8_t b[4];

	mqtt_put_be32(b, (uint32_t)v);
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, b, 4);
}
static inline int mqtt_tlv_add_u64(struct mqtt_tlv_message *msg, uint16_t type, uint64_t v)
{
	uint8_t b[8];

	mqtt_put_be64(b, v);
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, b, 8);
}
static inline int mqtt_tlv_add_string(struct mqtt_tlv_message *msg, uint16_t type,
                                       const char *str)
{
	size_t len;

	if (!str)
		return -1;
	len = strlen(str);
	if (len > 0xFFFFu)
		return -1;
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, (const uint8_t *)str, (uint16_t)len);
}
static inline int mqtt_tlv_add_mac(struct mqtt_tlv_message *msg, uint16_t type,
                                    const uint8_t *mac)
{
	if (!mac)
		return -1;
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, mac, MQTT_TLV_MAC_LEN);
}
static inline int mqtt_tlv_add_binary(struct mqtt_tlv_message *msg, uint16_t type,
                                       const uint8_t *data, uint16_t len)
{
	return mqtt_tlv_list_add_leaf(&msg->tlvs, type, data, len);
}

/* ── Leaf TLV — get ─────────────────────────────────────────────────────── */

static inline int mqtt_tlv_get_u8(const struct mqtt_tlv_message *msg,
                                   uint16_t type, uint8_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != 1)
		return -1;
	*out = e->value[0];
	return 0;
}
static inline int mqtt_tlv_get_u16(const struct mqtt_tlv_message *msg,
                                    uint16_t type, uint16_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != 2)
		return -1;
	*out = mqtt_get_be16(e->value);
	return 0;
}
static inline int mqtt_tlv_get_u32(const struct mqtt_tlv_message *msg,
                                    uint16_t type, uint32_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != 4)
		return -1;
	*out = mqtt_get_be32(e->value);
	return 0;
}
static inline int mqtt_tlv_get_s32(const struct mqtt_tlv_message *msg,
                                    uint16_t type, int32_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != 4)
		return -1;
	*out = (int32_t)mqtt_get_be32(e->value);
	return 0;
}
static inline int mqtt_tlv_get_u64(const struct mqtt_tlv_message *msg,
                                    uint16_t type, uint64_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != 8)
		return -1;
	*out = mqtt_get_be64(e->value);
	return 0;
}
static inline int mqtt_tlv_get_string(const struct mqtt_tlv_message *msg,
                                       uint16_t type, char *str, size_t str_len)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	size_t n;
	if (!e || !str || str_len == 0)
		return -1;
	n = (e->length < str_len - 1) ? e->length : str_len - 1;
	memcpy(str, e->value, n);
	str[n] = '\0';
	return 0;
}
static inline int mqtt_tlv_get_mac(const struct mqtt_tlv_message *msg,
                                    uint16_t type, uint8_t *mac)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || e->length != MQTT_TLV_MAC_LEN)
		return -1;
	memcpy(mac, e->value, MQTT_TLV_MAC_LEN);
	return 0;
}
static inline int mqtt_tlv_get_binary(const struct mqtt_tlv_message *msg,
                                       uint16_t type, uint8_t *buf, uint16_t *len)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_list_find_leaf(&msg->tlvs, type);
	if (!e || !len)
		return -1;
	if (buf && *len >= e->length)
		memcpy(buf, e->value, e->length);
	*len = e->length;
	return 0;
}

/* ── Container TLV — building ───────────────────────────────────────────── */

static inline struct mqtt_tlv_entry *
mqtt_tlv_add_container(struct mqtt_tlv_message *msg, uint16_t type)
{
	return mqtt_tlv_list_add_container(&msg->tlvs, type);
}
static inline struct mqtt_tlv_entry *
mqtt_tlv_container_add_container(struct mqtt_tlv_entry *parent, uint16_t type)
{
	if (!parent || !MQTT_TLV_IS_CONTAINER(parent->type))
		return NULL;
	return mqtt_tlv_list_add_container(&parent->children, type);
}
static inline int mqtt_tlv_container_add_u8(struct mqtt_tlv_entry *c, uint16_t t, uint8_t v)
{
	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return -1;
	return mqtt_tlv_list_add_leaf(&c->children, t, &v, 1);
}
static inline int mqtt_tlv_container_add_u16(struct mqtt_tlv_entry *c, uint16_t t, uint16_t v)
{
	uint8_t b[2];

	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return -1;
	mqtt_put_be16(b, v);
	return mqtt_tlv_list_add_leaf(&c->children, t, b, 2);
}
static inline int mqtt_tlv_container_add_u32(struct mqtt_tlv_entry *c, uint16_t t, uint32_t v)
{
	uint8_t b[4];

	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return -1;
	mqtt_put_be32(b, v);
	return mqtt_tlv_list_add_leaf(&c->children, t, b, 4);
}
static inline int mqtt_tlv_container_add_s32(struct mqtt_tlv_entry *c, uint16_t t, int32_t v)
{
	uint8_t b[4];

	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return -1;
	mqtt_put_be32(b, (uint32_t)v);
	return mqtt_tlv_list_add_leaf(&c->children, t, b, 4);
}
static inline int mqtt_tlv_container_add_string(struct mqtt_tlv_entry *c, uint16_t t,
                                                  const char *str)
{
	size_t len;

	if (!c || !MQTT_TLV_IS_CONTAINER(c->type) || !str)
		return -1;
	len = strlen(str);
	if (len > 0xFFFFu)
		return -1;
	return mqtt_tlv_list_add_leaf(&c->children, t, (const uint8_t *)str, (uint16_t)len);
}
static inline int mqtt_tlv_container_add_mac(struct mqtt_tlv_entry *c, uint16_t t,
                                              const uint8_t *mac)
{
	if (!c || !MQTT_TLV_IS_CONTAINER(c->type) || !mac)
		return -1;
	return mqtt_tlv_list_add_leaf(&c->children, t, mac, MQTT_TLV_MAC_LEN);
}
static inline int mqtt_tlv_container_add_binary(struct mqtt_tlv_entry *c, uint16_t t,
                                                  const uint8_t *data, uint16_t len)
{
	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return -1;
	return mqtt_tlv_list_add_leaf(&c->children, t, data, len);
}

/* ── Container TLV — reading ────────────────────────────────────────────── */

static inline struct mqtt_tlv_entry *
mqtt_tlv_next_container(const struct mqtt_tlv_message *msg,
                         const struct mqtt_tlv_entry *prev, uint16_t type)
{
	struct mqtt_tlv_entry *e;
	uint16_t search = MQTT_TLV_ENCODE_CONTAINER(MQTT_TLV_DECODE_TYPE(type));
	int found_prev  = (prev == NULL);
	dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list) {
		if (!found_prev) {
			if (e == prev)
				found_prev = 1;
			continue;
		}
		if (e->type == search)
			return e;
	}
	return NULL;
}
static inline struct mqtt_tlv_entry *
mqtt_tlv_get_container(const struct mqtt_tlv_message *msg, uint16_t type)
{
	return mqtt_tlv_next_container(msg, NULL, type);
}
static inline struct mqtt_tlv_entry *
mqtt_tlv_container_find(const struct mqtt_tlv_entry *c, uint16_t type)
{
	if (!c || !MQTT_TLV_IS_CONTAINER(c->type))
		return NULL;
	return mqtt_tlv_list_find_leaf(&c->children, type);
}
static inline int mqtt_tlv_container_get_u8(const struct mqtt_tlv_entry *c,
                                             uint16_t t, uint8_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	if (!e || e->length != 1)
		return -1;
	*out = e->value[0];
	return 0;
}
static inline int mqtt_tlv_container_get_u16(const struct mqtt_tlv_entry *c,
                                              uint16_t t, uint16_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	if (!e || e->length != 2)
		return -1;
	*out = mqtt_get_be16(e->value);
	return 0;
}
static inline int mqtt_tlv_container_get_u32(const struct mqtt_tlv_entry *c,
                                              uint16_t t, uint32_t *out)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	if (!e || e->length != 4)
		return -1;
	*out = mqtt_get_be32(e->value);
	return 0;
}
static inline int mqtt_tlv_container_get_string(const struct mqtt_tlv_entry *c,
                                                  uint16_t t, char *str, size_t sl)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	size_t n;
	if (!e || !str || sl == 0)
		return -1;
	n = (e->length < sl - 1) ? e->length : sl - 1;
	memcpy(str, e->value, n);
	str[n] = '\0';
	return 0;
}
static inline int mqtt_tlv_container_get_mac(const struct mqtt_tlv_entry *c,
                                              uint16_t t, uint8_t *mac)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	if (!e || e->length != MQTT_TLV_MAC_LEN)
		return -1;
	memcpy(mac, e->value, MQTT_TLV_MAC_LEN);
	return 0;
}
static inline int mqtt_tlv_container_get_binary(const struct mqtt_tlv_entry *c,
                                                  uint16_t t, uint8_t *buf, uint16_t *len)
{
	struct mqtt_tlv_entry *e = mqtt_tlv_container_find(c, t);
	if (!e || !len)
		return -1;
	if (buf && *len >= e->length)
		memcpy(buf, e->value, e->length);
	*len = e->length;
	return 0;
}

/* ── Serialization ──────────────────────────────────────────────────────── */

static int mqtt_tlv_serialize_list(const struct dl_list *list,
                                    uint8_t *buf, size_t buf_len, int depth)
{
	struct mqtt_tlv_entry *e;
	size_t offset = 0;

	if (depth > MQTT_TLV_MAX_DEPTH)
		return -1;
	dl_list_for_each(e, list, struct mqtt_tlv_entry, list) {
		if (offset + 4 > buf_len)
			return -1;
		if (MQTT_TLV_IS_CONTAINER(e->type)) {
			size_t lp;
			int inner;

			mqtt_put_be16(buf + offset, e->type);
			offset += 2;
			lp = offset;
			offset += 2;
			inner = mqtt_tlv_serialize_list(&e->children, buf + offset,
			                               buf_len - offset, depth + 1);
			if (inner < 0)
				return -1;
			mqtt_put_be16(buf + lp, (uint16_t)inner);
			offset += (size_t)inner;
		} else {
			if (offset + 4 + e->length > buf_len)
				return -1;
			mqtt_put_be16(buf + offset, e->type);
			offset += 2;
			mqtt_put_be16(buf + offset, e->length);
			offset += 2;
			if (e->length > 0)
				memcpy(buf + offset, e->value, e->length);
			offset += e->length;
		}
	}
	return (int)offset;
}

static inline int mqtt_tlv_serialize(const struct mqtt_tlv_message *msg,
                                      uint8_t *buffer, size_t buffer_len)
{
	int n;

	if (!msg || !buffer || buffer_len < 6)
		return -1;
	n = mqtt_tlv_serialize_list(&msg->tlvs, buffer, buffer_len, 0);
	if (n < 0) {
		MQTT_TLV_LOG_WARN("serialize failed for msg_type=0x%04x",
		                  mqtt_tlv_msg_type(msg));
		return -1;
	}
	MQTT_TLV_LOG_DEBUG("serialized msg_type=0x%04x -> %d bytes",
	                   mqtt_tlv_msg_type(msg), n);
	return n;
}

/* ── Deserialization ────────────────────────────────────────────────────── */

static int mqtt_tlv_deserialize_list(struct dl_list *list,
                                      const uint8_t *buf, size_t buf_len, int depth)
{
	size_t offset = 0;

	if (depth > MQTT_TLV_MAX_DEPTH)
		return -1;
	while (offset + 4 <= buf_len) {
		uint16_t type = mqtt_get_be16(buf + offset);
		uint16_t length;
		struct mqtt_tlv_entry *e;

		offset += 2;
		length = mqtt_get_be16(buf + offset);
		offset += 2;
		if (offset + length > buf_len)
			return -1;
		e = calloc(1, sizeof(*e));
		if (!e)
			return -1;
		e->type = type;
		dl_list_init(&e->children);
		if (MQTT_TLV_IS_CONTAINER(type)) {
			if (mqtt_tlv_deserialize_list(&e->children, buf + offset,
			                             length, depth + 1) < 0) {
				free(e);
				return -1;
			}
		} else {
			e->length = length;
			if (length > 0) {
				e->value = malloc(length);
				if (!e->value) {
					free(e);
					return -1;
				}
				memcpy(e->value, buf + offset, length);
			}
		}
		dl_list_add_tail(list, &e->list);
		offset += length;
	}
	return (offset == buf_len) ? 0 : -1;
}

static inline struct mqtt_tlv_message *
mqtt_tlv_deserialize(const uint8_t *buffer, size_t buffer_len)
{
	struct mqtt_tlv_message *msg;
	uint16_t first_type, first_len;

	if (!buffer || buffer_len < 6)
		return NULL;
	first_type = mqtt_get_be16(buffer);
	first_len  = mqtt_get_be16(buffer + 2);
	if (first_type != (uint16_t)TLV_GLOBAL_MSG_ID || first_len != 2)
		return NULL;
	msg = calloc(1, sizeof(*msg));
	if (!msg)
		return NULL;
	dl_list_init(&msg->tlvs);
	if (mqtt_tlv_deserialize_list(&msg->tlvs, buffer, buffer_len, 0) < 0) {
		mqtt_tlv_message_free(msg);
		return NULL;
	}
	MQTT_TLV_LOG_DEBUG("deserialized msg_type=0x%04x from %zu bytes",
	                   mqtt_tlv_msg_type(msg), buffer_len);
	return msg;
}

/* ── Structural validation ──────────────────────────────────────────────── */

static int mqtt_tlv_validate_entry(const struct mqtt_tlv_entry *e, int depth)
{
	struct mqtt_tlv_entry *child;

	if (depth > MQTT_TLV_MAX_DEPTH)
		return -1;
	if (MQTT_TLV_IS_GLOBAL(e->type) && depth > 0)
		return -1;
	if (MQTT_TLV_IS_CONTAINER(e->type)) {
		if (dl_list_empty(&e->children))
			return -1;
		dl_list_for_each(child, &e->children, struct mqtt_tlv_entry, list)
			if (mqtt_tlv_validate_entry(child, depth + 1) < 0)
				return -1;
	} else {
		if (e->length > 0 && !e->value)
			return -1;
	}
	return 0;
}
static inline int mqtt_tlv_validate_message(const struct mqtt_tlv_message *msg)
{
	struct mqtt_tlv_entry *e;
	int first = 1;

	if (!msg)
		return -1;
	dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list) {
		if (first) {
			if (e->type != (uint16_t)TLV_GLOBAL_MSG_ID)
				return -1;
			first = 0;
			continue;
		}
		if (mqtt_tlv_validate_entry(e, 0) < 0)
			return -1;
	}
	return first ? -1 : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Per-message TLV enums
 *
 * Naming convention: TLV_<FEATURE>_<MESSAGE>_<FIELD>
 * Indices are 1-based decimal.  MQTT_TLV_INNER(n) sets bit 13 (IF) for
 * inner-leaf TLVs that are only valid inside a container.
 * ════════════════════════════════════════════════════════════════════════════ */

/* ── SMD: CMD_ID_NEIGHBOR_DB_SET ───────────────────────────────────────── */
enum mqtt_tlv_neighbor_db_set {
	TLV_NEIGHBOR_DB_SET_AP_ALID      = 1, /* 6 B */
	TLV_NEIGHBOR_DB_SET_HAS_SMD_ID   = 2, /* 1 B */
	TLV_NEIGHBOR_DB_SET_SMD_ID       = 3, /* 6 B optional if has_smd_id=1 */
	TLV_NEIGHBOR_DB_SET_NUM_NRE      = 4, /* 2 B */
	TLV_NEIGHBOR_DB_SET_HAS_MLD_ADDR = 5, /* 1 B per tuple */
	TLV_NEIGHBOR_DB_SET_MLD_ADDR     = 6, /* 6 B optional per tuple */
	TLV_NEIGHBOR_DB_SET_BSSID        = 7, /* 6 B per tuple */
	TLV_NEIGHBOR_DB_SET_NRE_LEN      = 8, /* 2 B per tuple */
	TLV_NEIGHBOR_DB_SET_NRE          = 9, /* binary per tuple */
	_TLV_NEIGHBOR_DB_SET_MAX
};

enum mqtt_tlv_neighbor_db_set_resp {
	TLV_NEIGHBOR_DB_SET_RESP_STATUS = 1, /* 1 B */
	_TLV_NEIGHBOR_DB_SET_RESP_MAX
};

/* ── SMD: CMD_ID_NEIGHBOR_DB_GET ───────────────────────────────────────── */
enum mqtt_tlv_neighbor_db_get {
	TLV_NEIGHBOR_DB_GET_AP_ALID         = 1, /* 6 B */
	TLV_NEIGHBOR_DB_GET_HAS_SMD_ID      = 2, /* 1 B */
	TLV_NEIGHBOR_DB_GET_SMD_ID          = 3, /* 6 B optional */
	TLV_NEIGHBOR_DB_GET_GLOBAL_ENTRIES  = 4, /* 1 B, valid only if has_smd_id=1 */
	TLV_NEIGHBOR_DB_GET_HAS_MLD_ADDR    = 5, /* 1 B */
	TLV_NEIGHBOR_DB_GET_MLD_ADDR        = 6, /* 6 B optional */
	TLV_NEIGHBOR_DB_GET_HAS_BSSID       = 7, /* 1 B */
	TLV_NEIGHBOR_DB_GET_BSSID           = 8, /* 6 B optional */
	_TLV_NEIGHBOR_DB_GET_MAX
};

enum mqtt_tlv_neighbor_db_get_resp {
	TLV_NEIGHBOR_DB_GET_RESP_STATUS    = 1, /* 1 B */
	TLV_NEIGHBOR_DB_GET_RESP_NRE_COUNT = 2, /* 2 B */
	TLV_NEIGHBOR_DB_GET_RESP_NRE_ENTRY = 3, /* container */
	_TLV_NEIGHBOR_DB_GET_RESP_MAX
};

enum mqtt_tlv_neighbor_db_get_resp_entry {
	TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE_LEN = MQTT_TLV_INNER(1), /* 2 B */
	TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE     = MQTT_TLV_INNER(2), /* binary */
	_TLV_NEIGHBOR_DB_GET_RESP_ENTRY_MAX    = MQTT_TLV_INNER(3)
};

/* ── SMD: CMD_ID_NEIGHBOR_DB_CLEAR ─────────────────────────────────────── */
enum mqtt_tlv_neighbor_db_clear {
	TLV_NEIGHBOR_DB_CLEAR_AP_ALID      = 1, /* 6 B */
	TLV_NEIGHBOR_DB_CLEAR_HAS_SMD_ID   = 2, /* 1 B */
	TLV_NEIGHBOR_DB_CLEAR_SMD_ID       = 3, /* 6 B optional */
	TLV_NEIGHBOR_DB_CLEAR_HAS_MLD_ADDR = 4, /* 1 B */
	TLV_NEIGHBOR_DB_CLEAR_MLD_ADDR     = 5, /* 6 B optional */
	TLV_NEIGHBOR_DB_CLEAR_HAS_BSSID    = 6, /* 1 B */
	TLV_NEIGHBOR_DB_CLEAR_BSSID        = 7, /* 6 B optional */
	_TLV_NEIGHBOR_DB_CLEAR_MAX
};

enum mqtt_tlv_neighbor_db_clear_resp {
	TLV_NEIGHBOR_DB_CLEAR_RESP_STATUS = 1, /* 1 B */
	_TLV_NEIGHBOR_DB_CLEAR_RESP_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_NOTIFY_AUTH ───────────────────────────────────── */
/*
 * Dedicated authentication indication for the hostapd_if notify path.
 * Carries the computed hostapd_if auth context that is not guaranteed to be
 * recoverable from the raw frame alone.
 *
 * TLV_HIF_AUTH_IND_RSSI_DBM is encoded on the wire as the two's-complement
 * bit pattern of a signed 32-bit dBm value.
 */
enum mqtt_tlv_hif_auth_ind {
	TLV_HIF_AUTH_IND_IFACE              = 1,  /* var — interface name string */
	TLV_HIF_AUTH_IND_STA_MAC            = 2,  /* 6 B — notify_auth() STA MAC */
	TLV_HIF_AUTH_IND_STATUS_CODE        = 3,  /* 2 B — ctx->status_code */
	TLV_HIF_AUTH_IND_AUTH_TRANSACTION   = 4,  /* 2 B — ctx->data.auth_req.auth_transaction */
	TLV_HIF_AUTH_IND_ALLOW_REUSE        = 5,  /* 1 B — ctx->data.auth_req.allow_reuse */
	TLV_HIF_AUTH_IND_AUTH_ALG           = 6,  /* 2 B — ctx->data.auth_req.auth_alg */
	TLV_HIF_AUTH_IND_RSSI_DBM           = 7,  /* 4 B — signed dBm, encoded as int32 bits */
	TLV_HIF_AUTH_IND_RX_LINK_ID         = 8,  /* 4 B — ctx->rx_link_id */
	TLV_HIF_AUTH_IND_STA_ASSOC_LINK_MAC = 9,  /* 6 B — ctx->data.auth_req.sta_assoc_link_mac */
	TLV_HIF_AUTH_IND_FRAME              = 10, /* var — full raw 802.11 auth frame */
	TLV_HIF_AUTH_IND_FRAME_LEN         = 11, /* 2 B — frame_len in bytes */
	_TLV_HIF_AUTH_IND_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_NOTIFY_ASSOC ──────────────────────────────────── */
/*
 * Association indication for the hostapd_if notify path.
 * Carries the computed hostapd_if assoc context including MLO per-link MACs.
 *
 * TLV_HIF_ASSOC_IND_RSSI_DBM is encoded as the two's-complement bit pattern
 * of a signed 32-bit dBm value (same convention as auth ind RSSI).
 * TLV_HIF_ASSOC_IND_STA_LINK_MACS is a flat blob of MAX_MLO_LINKS * 6 bytes,
 * only the links whose bit is set in valid_link_bitmap carry a real address;
 * the rest are zero-filled.
 */
enum mqtt_tlv_hif_assoc_ind {
	TLV_HIF_ASSOC_IND_IFACE             = 1,  /* var — interface name string */
	TLV_HIF_ASSOC_IND_STA_MAC           = 2,  /* 6 B — STA MAC (sta->addr) */
	TLV_HIF_ASSOC_IND_STATUS_CODE       = 3,  /* 2 B — ctx->status_code */
	TLV_HIF_ASSOC_IND_IS_REASSOC        = 4,  /* 1 B — ctx->data.assoc_req.is_reassoc */
	TLV_HIF_ASSOC_IND_RSSI_DBM          = 5,  /* 4 B — signed dBm, encoded as int32 bits */
	TLV_HIF_ASSOC_IND_RX_LINK_ID        = 6,  /* 4 B — ctx->rx_link_id */
	TLV_HIF_ASSOC_IND_VALID_LINK_BITMAP = 7,  /* 4 B — ctx->data.assoc_req.valid_link_bitmap */
	TLV_HIF_ASSOC_IND_STA_ASSOC_LINK_MAC = 8, /* 6 B — ctx->data.assoc_req.sta_assoc_link_mac */
	TLV_HIF_ASSOC_IND_STA_LINK_MACS    = 9,  /* MAX_MLO_LINKS*6 B — per-link MACs flat array */
	TLV_HIF_ASSOC_IND_FRAME             = 10, /* var — full raw 802.11 assoc/reassoc frame */
	TLV_HIF_ASSOC_IND_FRAME_LEN        = 11, /* 2 B — frame_len in bytes */
	_TLV_HIF_ASSOC_IND_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_NOTIFY_DEAUTH ─────────────────────────────── */
/*
 * Deauthentication indication for the hostapd_if notify path.
 * TLV_HIF_DEAUTH_IND_REASON_CODE is extracted from the raw 802.11 frame body
 * (2 bytes at offset 24, little-endian) when frame is available.
 */
enum mqtt_tlv_hif_deauth_ind {
	TLV_HIF_DEAUTH_IND_IFACE       = 1,  /* var — interface name string */
	TLV_HIF_DEAUTH_IND_STA_MAC     = 2,  /* 6 B — STA MAC address */
	TLV_HIF_DEAUTH_IND_RX_LINK_ID  = 3,  /* 4 B — ctx->rx_link_id */
	TLV_HIF_DEAUTH_IND_REASON_CODE = 4,  /* 2 B — reason code from frame body */
	TLV_HIF_DEAUTH_IND_FRAME       = 5,  /* var — full raw 802.11 deauth frame */
	TLV_HIF_DEAUTH_IND_FRAME_LEN  = 6,  /* 2 B — frame_len in bytes */
	_TLV_HIF_DEAUTH_IND_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_NOTIFY_DISASSOC ───────────────────────────── */
/*
 * Disassociation indication for the hostapd_if notify path.
 * TLV_HIF_DISASSOC_IND_REASON_CODE is extracted from the raw 802.11 frame body
 * (2 bytes at offset 24, little-endian) when frame is available.
 */
enum mqtt_tlv_hif_disassoc_ind {
	TLV_HIF_DISASSOC_IND_IFACE       = 1,  /* var — interface name string */
	TLV_HIF_DISASSOC_IND_STA_MAC     = 2,  /* 6 B — STA MAC address */
	TLV_HIF_DISASSOC_IND_RX_LINK_ID  = 3,  /* 4 B — ctx->rx_link_id */
	TLV_HIF_DISASSOC_IND_REASON_CODE = 4,  /* 2 B — reason code from frame body */
	TLV_HIF_DISASSOC_IND_FRAME       = 5,  /* var — full raw 802.11 disassoc frame */
	TLV_HIF_DISASSOC_IND_FRAME_LEN  = 6,  /* 2 B — frame_len in bytes */
	_TLV_HIF_DISASSOC_IND_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_INTERFACE_CREATE ───────────────────────────── */
enum mqtt_tlv_hif_interface_create {
	TLV_HIF_INTERFACE_CREATE_IFNAME = 1, /* var — interface name string */
	_TLV_HIF_INTERFACE_CREATE_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_EVENT_ASSOC_TX_COMPLETE ───────────────────── */
/*
 * Assoc response TX completion event.
 * Signals whether the assoc response frame was successfully transmitted.
 */
enum mqtt_tlv_hif_assoc_tx_complete {
	TLV_HIF_ASSOC_TX_COMPLETE_IFNAME  = 1, /* var — interface name string */
	TLV_HIF_ASSOC_TX_COMPLETE_STA_MAC = 2, /* 6 B — STA MAC address */
	TLV_HIF_ASSOC_TX_COMPLETE_OK      = 3, /* 1 B — 1=success, 0=failure */
	TLV_HIF_ASSOC_TX_COMPLETE_STATUS  = 4, /* 2 B — status code */
	TLV_HIF_ASSOC_TX_COMPLETE_AID     = 5, /* 2 B — association ID */
	_TLV_HIF_ASSOC_TX_COMPLETE_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_EVENT_DEAUTH / EVT_ID_HIF_EVENT_DISASSOC ──── */
/*
 * Shared TLV layout for deauth and disassoc event notifications.
 * Used by both hostapd_if_event_deauth() and hostapd_if_event_disassoc().
 * Carries deauth_disassoc struct fields: link_id, link_mac, reason_code,
 * disconnect type, and TX status when applicable.
 * The message ID (EVT_ID_HIF_EVENT_DEAUTH vs EVT_ID_HIF_EVENT_DISASSOC)
 * distinguishes the frame type on the wire.
 */
enum mqtt_tlv_hif_deauth_disassoc_event {
	TLV_HIF_DEAUTH_DISASSOC_EVT_IFNAME          = 1, /* var — interface name string */
	TLV_HIF_DEAUTH_DISASSOC_EVT_STA_MAC         = 2, /* 6 B — STA MAC (evt.sta_mac) */
	TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_ID         = 3, /* 4 B — deauth_disassoc.link_id */
	TLV_HIF_DEAUTH_DISASSOC_EVT_LINK_MAC        = 4, /* 6 B — deauth_disassoc.link_mac (MLO) */
	TLV_HIF_DEAUTH_DISASSOC_EVT_REASON_CODE     = 5, /* 2 B — deauth_disassoc.reason_code */
	TLV_HIF_DEAUTH_DISASSOC_EVT_DISCONNECT_TYPE = 6, /* 4 B — deauth_disassoc.type */
	TLV_HIF_DEAUTH_DISASSOC_EVT_IS_TX_STATUS    = 7, /* 1 B — deauth_disassoc.is_tx_status */
	TLV_HIF_DEAUTH_DISASSOC_EVT_TX_STATUS_OK    = 8, /* 4 B — deauth_disassoc.tx_status_ok */
	_TLV_HIF_DEAUTH_DISASSOC_EVT_MAX
};

/* ── HOSTAPD_IF: EVT_ID_HIF_EVENT_AUTH_TX_COMPLETE ────────────────────── */
/*
 * Auth response TX completion event from hostapd_if_event_auth_tx_complete().
 */
enum mqtt_tlv_hif_auth_tx_complete {
	TLV_HIF_AUTH_TX_COMPLETE_IFNAME  = 1, /* var — interface name string */
	TLV_HIF_AUTH_TX_COMPLETE_STA_MAC = 2, /* 6 B — STA MAC address */
	_TLV_HIF_AUTH_TX_COMPLETE_MAX
};

/*
 * ── HOSTAPD_IF: Common TLVs shared across multiple HIF messages ──────────
 *
 * These TLVs carry fields that appear in more than one HIF command or event
 * (e.g. interface name in REGISTER_POLICY_*, ASSOC_COMPLETED_EVENT, …).
 * Indices are independent of per-message enums above; the message ID
 * always determines how a TLV is interpreted.
 */
enum mqtt_tlv_hif_common {
	TLV_HIF_IFNAME = 1, /* var — interface name string */
	_TLV_HIF_COMMON_MAX
};

/* ── HOSTAPD_IF: CMD_ID_HIF_REGISTER_POLICY ────────────────────────────── */
/*
 * Maps to register_frame(ifname_ctx, cat, policy).
 * One message = one register_frame() call.
 * TLV_HIF_REGISTER_POLICY_IFNAME scopes the registration to a specific
 * interface.  Using a message-scoped ID (3) avoids collision with the
 * per-message TLVs at IDs 1 and 2.
 */
enum mqtt_hif_register_frame_tlv {
	TLV_HIF_REGISTER_FRAME_FRAME_TYPE = 1, /* 1 B — frame type (HIF_FRAME_TYPE_*) */
	TLV_HIF_REGISTER_FRAME_POLICY     = 2, /* 1 B — policy (HIF_POLICY_*)         */
	TLV_HIF_REGISTER_FRAME_IFNAME     = 3, /* var — interface name string          */
	_TLV_HIF_REGISTER_FRAME_MAX
};


/* ── HOSTAPD_IF: CMD_ID_HIF_REGISTER_EVENT ─────────────────────────────── */
/*
 * Maps to register_event(ifname_ctx, type, set).
 * One message = one register_event() call.
 */
enum mqtt_hif_register_event_tlv {
	TLV_HIF_REGISTER_EVENT_TYPE   = 1, /* 1 B — event type (HIF_EVENT_*) */
	TLV_HIF_REGISTER_EVENT_SET    = 2, /* 1 B — 1=register, 0=unregister */
	TLV_HIF_REGISTER_EVENT_IFNAME = 3, /* var — interface name string     */
	_TLV_HIF_REGISTER_EVENT_MAX
};

/* ── SYS: EVT_ID_SYS_HOSTAPD_STARTED ───────────────────────────────────── */
/* Also used as the response to CMD_ID_SYS_PING. */
enum mqtt_tlv_sys_hostapd_started {
	TLV_SYS_HOSTAPD_STARTED_VERSION    = 1,  /* var — version string       */
	TLV_SYS_HOSTAPD_STARTED_NUM_IFACES = 2,  /* 2 B — number of interfaces */
	_TLV_SYS_HOSTAPD_STARTED_MAX
};

/* ── SYS: EVT_ID_SYS_HOSTAPD_STOPPED ───────────────────────────────────── */
enum mqtt_tlv_sys_hostapd_stopped {
	TLV_SYS_HOSTAPD_STOPPED_REASON = 1,  /* 2 B — shutdown reason code */
	_TLV_SYS_HOSTAPD_STOPPED_MAX
};

/* ── Policy structures ──────────────────────────────────────────────────── */

enum mqtt_tlv_val_type {
	MQTT_TLV_VAL_NONE      = 0,
	MQTT_TLV_VAL_U8        = 1,
	MQTT_TLV_VAL_U16       = 2,
	MQTT_TLV_VAL_U32       = 3,
	MQTT_TLV_VAL_MAC       = 4,  /* 6-byte EUI-48 (MAC address / BSSID) */
	MQTT_TLV_VAL_BINARY    = 5,  /* variable-length binary blob          */
	MQTT_TLV_VAL_STRING    = 6,  /* variable-length, not NUL-terminated  */
	MQTT_TLV_VAL_CONTAINER = 7,
	MQTT_TLV_VAL_S32       = 8,  /* signed 32-bit integer, big-endian    */
};

struct mqtt_tlv_policy {
	uint16_t                      tlv_id;
	enum mqtt_tlv_val_type        val_type;
	uint16_t                      len;
	bool                          mandatory;
	const struct mqtt_tlv_policy *nested;
	uint8_t                       nested_count;
};

struct mqtt_msg_policy {
	uint16_t                      msg_id;
	const struct mqtt_tlv_policy *tlvs;
	uint8_t                       num_tlvs;
};

/* Policy validation API is defined as static inline in mqtt_feature_map.h,
 * which includes this file.  No separate declaration is needed here. */

#endif /* CONFIG_MQTT */
#endif /* MQTT_TLV_MAP_H */
