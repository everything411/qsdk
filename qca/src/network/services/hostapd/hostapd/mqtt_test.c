/*
 * mqtt_test.c - Standalone MQTT test client for hostapd MQTT messaging
 *
 * Connects to an MQTT broker, subscribes to all hostapd/transmit/# topics,
 * and provides an interactive command-line interface to test the MQTT
 * messaging pipeline with hostapd.
 *
 * The primary test command is 'ping':
 *   - Sends CMD_ID_SYS_PING (6 bytes, no parameters) on hostapd/receive/SYS
 *   - hostapd responds with EVT_ID_SYS_HOSTAPD_STARTED on hostapd/transmit/SYS
 *     containing the version string and BSS count
 *   - No WiFi setup, BSSes, or stations are needed — the exchange works
 *     as long as hostapd is running with mqtt_enabled=1
 *
 * Build:
 *   gcc -Wall -I../src -DCONFIG_MQTT -o mqtt_test mqtt_test.c -lmosquitto
 *
 * The -I../src flag puts the hostapd src/ tree on the include path so that
 * #include "utils/mqtt_feature_map.h" resolves correctly.
 * DCONFIG_MQTT enables the MQTT interface definitions in that header.
 *
 * Usage:
 *   ./mqtt_test [broker_host [broker_port]]
 *
 * Interactive commands at the '>' prompt:
 *   ping          - Send CMD_ID_SYS_PING; hostapd replies with version info
 *   ndb_set       - NeighborDB SET request (single/multiple tuples)
 *   ndb_get       - NeighborDB GET request
 *   ndb_clear     - NeighborDB CLEAR request
 *   stats         - Print TX/RX message counters
 *   help          - Show this list
 *   quit / q      - Disconnect and exit
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <mosquitto.h>

/*
 * Include the two public MQTT interface headers.  They are self-contained
 * (only standard C headers required) and define every constant, enum, macro,
 * and policy used by this application — demonstrating the intended usage
 * pattern for any external application communicating with hostapd over MQTT.
 */
#define CONFIG_MQTT 1          /* enable the MQTT interface definitions    */
#include "utils/mqtt_feature_map.h"  /* MSG IDs, topics, feature routing, policy */
/* mqtt_tlv_map.h is included automatically by mqtt_feature_map.h           */

/* Wire-format predicates and helpers come directly from the header macros:
 *   MQTT_TLV_IS_GLOBAL(type)            — true if bit 15 (GF) is set
 *   MQTT_TLV_IS_CONTAINER(wire_type)    — true if bit 14 (CF) is set
 *   MQTT_TLV_IS_INNER(stored_type)      — true if bit 13 (IF) is set
 *   MQTT_TLV_DECODE_TYPE(wire_type)     — strip bit 14 (CF) to get stored constant
 *   MQTT_TLV_ENCODE_CONTAINER(type)     — set bit 14 (CF) before writing to wire
 *
 * Global TLV TLV_GLOBAL_MSG_ID (0x8001) is always the first TLV on the wire.
 * Payload TLVs follow at byte offset 6.
 */

/* 802.11 Element ID used by this test */
#define WLAN_EID_NEIGHBOR_REPORT  52u
#define MQTT_TEST_CMD_BUF_LEN     1536

/* ── Application state ──────────────────────────────────────────────────── */

struct test_ctx {
	struct mosquitto *mosq;
	bool              connected;
	unsigned long     tx_count;
	unsigned long     rx_count;
	const char       *broker_host;
	int               broker_port;
};

/* Forward declarations for helpers used before their definitions */
static int put_tlv_mac(uint8_t *buf, size_t cap, size_t *off,
		       uint16_t type, const uint8_t *mac);
static int put_tlv_u8(uint8_t *buf, size_t cap, size_t *off,
		      uint16_t type, uint8_t val);
static int put_tlv_u16(uint8_t *buf, size_t cap, size_t *off,
		       uint16_t type, uint16_t val);
static int put_tlv_bytes(uint8_t *buf, size_t cap, size_t *off,
			 uint16_t type, const uint8_t *data, uint16_t dlen);

/* ── Big-endian helpers ─────────────────────────────────────────────────── */

static inline void put_u16(uint8_t *b, uint16_t v)
{
	b[0] = (uint8_t)(v >> 8);
	b[1] = (uint8_t)(v & 0xffu);
}

static inline void put_u32(uint8_t *b, uint32_t v)
{
	b[0] = (uint8_t)(v >> 24);
	b[1] = (uint8_t)(v >> 16);
	b[2] = (uint8_t)(v >>  8);
	b[3] = (uint8_t)(v & 0xffu);
}

static inline uint16_t get_u16(const uint8_t *b)
{
	return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static inline uint32_t get_u32(const uint8_t *b)
{
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
	       ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
}

/* ── Minimal ping message builder ───────────────────────────────────────── */

/*
 * build_ping_msg - serialise CMD_ID_SYS_PING into buf.
 *
 * Wire layout: [TLV_GLOBAL_MSG_ID:2B][0x0002:2B][CMD_ID_SYS_PING:2B]
 * MSG_ID global TLV only — no payload TLVs.
 * Returns the byte count (always 6).
 */
static int build_ping_msg(uint8_t *buf, size_t buflen)
{
	if (buflen < 6)
		return -1;
	put_u16(buf,     (uint16_t)TLV_GLOBAL_MSG_ID);  /* MSG_ID TLV type  */
	put_u16(buf + 2, 2);                           /* MSG_ID TLV length */
	put_u16(buf + 4, CMD_ID_SYS_PING);             /* MSG_ID TLV value  */
	return 6;
}

/* ── TLV decoder ────────────────────────────────────────────────────────── */

static const char *msg_id_name(uint16_t id)
{
	switch (id) {
	case CMD_ID_SYS_PING:            return "CMD_SYS_PING";
	case EVT_ID_SYS_HOSTAPD_STARTED: return "EVT_SYS_HOSTAPD_STARTED";
	case EVT_ID_SYS_HOSTAPD_STOPPED: return "EVT_SYS_HOSTAPD_STOPPED";
	case EVT_ID_CONFIG_RELOADED:     return "EVT_CONFIG_RELOADED";
	case CMD_ID_NEIGHBOR_DB_SET:      return "CMD_NEIGHBOR_DB_SET";
	case CMD_ID_NEIGHBOR_DB_GET:      return "CMD_NEIGHBOR_DB_GET";
	case CMD_ID_NEIGHBOR_DB_CLEAR:    return "CMD_NEIGHBOR_DB_CLEAR";
	case EVT_ID_NEIGHBOR_DB_SET_RESP: return "EVT_NEIGHBOR_DB_SET_RESP";
	case EVT_ID_NEIGHBOR_DB_GET_RESP: return "EVT_NEIGHBOR_DB_GET_RESP";
	case EVT_ID_NEIGHBOR_DB_CLEAR_RESP:return "EVT_NEIGHBOR_DB_CLEAR_RESP";
	default:                         return NULL;
	}
}

/*
 * TLV display helpers.
 *
 * TLV IDs are now per-message sequential indices (1, 2, 3...).  The same
 * index value means different things in different messages, so the name and
 * format are resolved from (msg_id, tlv_index) pairs below.  For messages
 * not listed here the decoder falls back to raw hex.
 */

typedef enum { FMT_HEX, FMT_MAC, FMT_U8, FMT_U16, FMT_U32, FMT_STR } tlv_fmt_t;

struct tlv_desc { uint16_t idx; const char *name; tlv_fmt_t fmt; };

/* Per-message TLV descriptor tables */
static const struct tlv_desc tlv_hostapd_started[] = {
	{ TLV_SYS_HOSTAPD_STARTED_VERSION,    "VERSION",    FMT_STR },
	{ TLV_SYS_HOSTAPD_STARTED_NUM_IFACES, "NUM_IFACES", FMT_U16 },
	{ 0, NULL, FMT_HEX }
};

static const struct tlv_desc tlv_neighbor_db_set_resp[] = {
	{ TLV_NEIGHBOR_DB_SET_RESP_STATUS, "STATUS", FMT_U8 },
	{ 0, NULL, FMT_HEX }
};

static const struct tlv_desc tlv_neighbor_db_get_resp[] = {
	{ TLV_NEIGHBOR_DB_GET_RESP_STATUS,    "STATUS",    FMT_U8  },
	{ TLV_NEIGHBOR_DB_GET_RESP_NRE_COUNT, "NRE_COUNT", FMT_U16 },
	{ TLV_NEIGHBOR_DB_GET_RESP_NRE_ENTRY, "NRE_ENTRY", FMT_HEX },
	{ TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE_LEN, "NRE_LEN", FMT_U16 },
	{ TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE,     "NRE",     FMT_HEX },
	{ 0, NULL, FMT_HEX }
};

static const struct tlv_desc tlv_neighbor_db_clear_resp[] = {
	{ TLV_NEIGHBOR_DB_CLEAR_RESP_STATUS, "STATUS", FMT_U8 },
	{ 0, NULL, FMT_HEX }
};


static const struct tlv_desc *tlv_desc_for_msg(uint16_t msg_id)
{
	switch (msg_id) {
	case EVT_ID_SYS_HOSTAPD_STARTED:      return tlv_hostapd_started;
	case EVT_ID_NEIGHBOR_DB_SET_RESP:     return tlv_neighbor_db_set_resp;
	case EVT_ID_NEIGHBOR_DB_GET_RESP:     return tlv_neighbor_db_get_resp;
	case EVT_ID_NEIGHBOR_DB_CLEAR_RESP:   return tlv_neighbor_db_clear_resp;
	default:                              return NULL;
	}
}

static const struct tlv_desc *find_tlv_desc(const struct tlv_desc *tbl,
					    uint16_t idx)
{
	if (!tbl)
		return NULL;
	for (; tbl->name; tbl++) {
		if (tbl->idx == idx)
			return tbl;
	}
	return NULL;
}

/* Length-based format fallback when no descriptor is available */
static tlv_fmt_t fmt_by_len(uint16_t vlen)
{
	switch (vlen) {
	case 1:  return FMT_U8;
	case 2:  return FMT_U16;
	case 4:  return FMT_U32;
	case 6:  return FMT_MAC;
	default: return FMT_HEX;
	}
}

static void decode_tlv_list(uint16_t msg_id, const uint8_t *buf, size_t len,
			    const char *indent)
{
	size_t                    off    = 0;
	uint32_t                  idx    = 0;
	const struct tlv_desc    *dtbl   = tlv_desc_for_msg(msg_id);
	char                      sub[32];
	char                      str[256];

	snprintf(sub, sizeof(sub), "%s  ", indent);

	while (off + 4 <= len) {
		uint16_t              type   = get_u16(buf + off); off += 2;
		uint16_t              vlen   = get_u16(buf + off); off += 2;
		bool                  nested = MQTT_TLV_IS_CONTAINER(type);
		uint16_t              bare   = MQTT_TLV_DECODE_TYPE(type);
		bool                  inner  = MQTT_TLV_IS_INNER(bare);
		const struct tlv_desc *d     = find_tlv_desc(dtbl, bare);
		const char            *name  = d ? d->name : (inner ? "INNER" : "TLV");
		const uint8_t         *v     = buf + off;
		tlv_fmt_t              fmt   = d ? d->fmt : fmt_by_len(vlen);

		idx++;

		if (off + vlen > len) {
			printf("%s[%u] TRUNCATED type=0x%04x\n", indent, idx, type);
			break;
		}

		if (nested) {
			printf("%s[%u] Container %-20s (0x%04x) inner=%u bytes\n",
			       indent, idx, name, bare, vlen);
			if (vlen > 0)
				decode_tlv_list(msg_id, v, vlen, sub);
		} else {
			printf("%s[%u] %-22s (0x%04x) = ",
			       indent, idx, name, bare);
			switch (fmt) {
			case FMT_MAC:
				if (vlen == 6)
					printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
					       v[0], v[1], v[2],
					       v[3], v[4], v[5]);
				else
					printf("<bad len %u>\n", vlen);
				break;
			case FMT_U8:
				printf(vlen == 1 ? "%u\n" : "<bad len %u>\n",
				       vlen == 1 ? (unsigned)v[0] : vlen);
				break;
			case FMT_U16:
				printf(vlen == 2 ? "%u (0x%04x)\n"
					         : "<bad len %u>\n",
				       vlen == 2 ? (unsigned)get_u16(v) : vlen,
				       vlen == 2 ? (unsigned)get_u16(v) : 0);
				break;
			case FMT_U32:
				printf(vlen == 4 ? "%u\n" : "<bad len %u>\n",
				       vlen == 4 ? get_u32(v) : vlen);
				break;
			case FMT_STR: {
				size_t slen = vlen < sizeof(str) - 1 ?
					      vlen : sizeof(str) - 1;
				memcpy(str, v, slen);
				str[slen] = '\0';
				printf("'%s'\n", str);
				break;
			}
			default: {
				uint16_t i;
				printf("0x");
				for (i = 0; i < vlen && i < 16; i++)
					printf("%02x", v[i]);
				if (vlen > 16) printf("...");
				printf("\n");
				break;
			}
			}
		}
		off += vlen;
	}
}

static int parse_mac(const char *s, uint8_t mac[6])
{
	unsigned int b[6];
	int i;

	if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
				&b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
		return -1;

	for (i = 0; i < 6; i++) {
		if (b[i] > 0xff)
			return -1;
		mac[i] = (uint8_t)b[i];
	}
	return 0;
}

static int parse_hex_blob(const char *hex, uint8_t **out, uint16_t *out_len)
{
	size_t hlen, i;
	uint8_t *buf;

	if (!hex || !out || !out_len)
		return -1;

	hlen = strlen(hex);
	if (hlen == 0 || (hlen % 2) != 0 || hlen / 2 > 0xffff)
		return -1;

	buf = malloc(hlen / 2);
	if (!buf)
		return -1;

	for (i = 0; i < hlen / 2; i++) {
		unsigned int byte;
		if (sscanf(hex + 2 * i, "%02x", &byte) != 1) {
			free(buf);
			return -1;
		}
		buf[i] = (uint8_t)byte;
	}

	*out = buf;
	*out_len = (uint16_t)(hlen / 2);
	return 0;
}

struct ndb_set_tuple {
	uint8_t has_mld_addr;
	const uint8_t *mld_addr;
	const uint8_t *bssid;
	const uint8_t *nre;
	uint16_t nre_len;
};

static int cmd_neighbor_db_set(struct test_ctx *ctx,
		const uint8_t ap_alid[6],
		uint8_t has_smd_id,
		const uint8_t smd_id[6],
		const struct ndb_set_tuple *tuples,
		size_t num_tuples)
{
	uint8_t buf[4096];
	size_t off = 6;
	size_t i;
	int mlen;

	if (!ctx || !ctx->connected || !tuples || num_tuples == 0)
		return -1;

	put_u16(buf, TLV_GLOBAL_MSG_ID);
	put_u16(buf + 2, 2);
	put_u16(buf + 4, CMD_ID_NEIGHBOR_DB_SET);

	put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_AP_ALID, ap_alid);
	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_HAS_SMD_ID, has_smd_id);
	if (has_smd_id)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_SMD_ID, smd_id);
	put_tlv_u16(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_NUM_NRE,
		    (uint16_t)num_tuples);

	for (i = 0; i < num_tuples; i++) {
		if (!tuples[i].bssid || !tuples[i].nre ||
		    tuples[i].nre_len < 13 || tuples[i].nre_len > 255) {
			fprintf(stderr, "[ERR] invalid tuple[%zu]\n", i);
			return -1;
		}

		if (memcmp(tuples[i].nre, tuples[i].bssid, 6) != 0) {
			fprintf(stderr, "[ERR] tuple[%zu] NRE BSSID mismatch\n", i);
			return -1;
		}

		put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_HAS_MLD_ADDR,
			   tuples[i].has_mld_addr ? 1 : 0);
		if (tuples[i].has_mld_addr) {
			if (!tuples[i].mld_addr) {
				fprintf(stderr, "[ERR] tuple[%zu] has_mld_addr=1 but no mld_addr\n", i);
				return -1;
			}
			put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_MLD_ADDR,
				    tuples[i].mld_addr);
		}
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_BSSID,
			    tuples[i].bssid);
		put_tlv_u16(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_NRE_LEN,
			    tuples[i].nre_len);
		put_tlv_bytes(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_SET_NRE,
			      tuples[i].nre, tuples[i].nre_len);
	}

	mlen = (int) off;
	if (mosquitto_publish(ctx->mosq, NULL, MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD,
				mlen, buf, 0, false) != MOSQ_ERR_SUCCESS)
		return -1;

	ctx->tx_count++;
	printf("[TX ] CMD_NEIGHBOR_DB_SET tuples=%zu -> "
	       MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD " (%d bytes)\n",
	       num_tuples, mlen);
	return 0;
}

static int cmd_neighbor_db_get(struct test_ctx *ctx,
		const uint8_t ap_alid[6],
		uint8_t has_smd_id,
		const uint8_t smd_id[6],
		uint8_t get_global_entries,
		uint8_t has_mld_addr,
		const uint8_t mld_addr[6],
		uint8_t has_bssid,
		const uint8_t bssid[6])
{
	uint8_t buf[256];
	size_t off = 6;
	int mlen;

	printf("inside CMD_ID_NEIGHBOR_DB_GET func()");

	if (!ctx->connected) return -1;

	put_u16(buf, TLV_GLOBAL_MSG_ID);
	put_u16(buf + 2, 2);
	put_u16(buf + 4, CMD_ID_NEIGHBOR_DB_GET);

	if(put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_AP_ALID, ap_alid) < 0 ||
			put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_HAS_SMD_ID, has_smd_id) < 0) {
		fprintf(stderr, "[ERR] buffer overflow building GET request\n");
		return -1;
	}
	if (has_smd_id) {
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_SMD_ID, smd_id);
		put_tlv_u8(buf, sizeof(buf), &off,
				TLV_NEIGHBOR_DB_GET_GLOBAL_ENTRIES, get_global_entries);
	}

	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_HAS_MLD_ADDR, has_mld_addr);
	if (has_mld_addr)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_MLD_ADDR, mld_addr);
	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_HAS_BSSID, has_bssid);
	if (has_bssid)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_GET_BSSID, bssid);

	mlen = (int) off;
	if (mosquitto_publish(ctx->mosq, NULL, MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD,
				mlen, buf, 0, false) != MOSQ_ERR_SUCCESS)
		return -1;

	ctx->tx_count++;
	printf("[TX ] CMD_NEIGHBOR_DB_GET -> " MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD
			" (%d bytes)\n", mlen);
	return 0;
}


static int cmd_neighbor_db_clear(struct test_ctx *ctx,
		const uint8_t ap_alid[6],
		uint8_t has_smd_id,
		const uint8_t smd_id[6],
		uint8_t has_mld_addr,
		const uint8_t mld_addr[6],
		uint8_t has_bssid,
		const uint8_t bssid[6])
{
	uint8_t buf[256];
	size_t off = 6;
	int mlen;

	printf("inside CMD_ID_NEIGHBOR_DB_CLEAR func()");
	if (!ctx->connected) return -1;

	put_u16(buf, TLV_GLOBAL_MSG_ID);
	put_u16(buf + 2, 2);
	put_u16(buf + 4, CMD_ID_NEIGHBOR_DB_CLEAR);

	put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_AP_ALID, ap_alid);
	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_HAS_SMD_ID, has_smd_id);
	if (has_smd_id)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_SMD_ID, smd_id);
	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_HAS_MLD_ADDR, has_mld_addr);
	if (has_mld_addr)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_MLD_ADDR, mld_addr);
	put_tlv_u8(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_HAS_BSSID, has_bssid);
	if (has_bssid)
		put_tlv_mac(buf, sizeof(buf), &off, TLV_NEIGHBOR_DB_CLEAR_BSSID, bssid);

	mlen = (int) off;
	if (mosquitto_publish(ctx->mosq, NULL, MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD,
				mlen, buf, 0, false) != MOSQ_ERR_SUCCESS)
		return -1;

	ctx->tx_count++;
	printf("[TX ] CMD_NEIGHBOR_DB_CLEAR -> " MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD
			" (%d bytes)\n", mlen);
	return 0;
}

static void decode_and_print(const char *topic,
		const uint8_t *data, int data_len)
{
	uint16_t    msg_type = 0;
	time_t      now    = time(NULL);
	struct tm  *tm_inf = localtime(&now);
	char        ts[16];
	const char *id_name;
	char        id_str[32];

	strftime(ts, sizeof(ts), "%H:%M:%S", tm_inf);

	if (data_len < 6) {
		printf("\n[%s] RX topic='%s' payload too short (%d B)\n",
				ts, topic, data_len);
		goto prompt;
	}

	/* Extract msg_type from MSG_ID global TLV (bytes 0-5) */
	if (data_len >= 6 &&
			get_u16(data) == (uint16_t)TLV_GLOBAL_MSG_ID &&
			get_u16(data + 2) == 2)
		msg_type = get_u16(data + 4);

	id_name = msg_id_name(msg_type);
	if (id_name)
		snprintf(id_str, sizeof(id_str), "%s", id_name);
	else
		snprintf(id_str, sizeof(id_str), "0x%04x", msg_type);

	printf("\n[%s] RX  topic='%s'\n", ts, topic);
	printf("     msg_type=%-22s  payload=%d bytes\n", id_str, data_len);

	if (data_len > 6)
		decode_tlv_list(msg_type, data + 6, (size_t)(data_len - 6),
				"     ");

prompt:
	printf("> ");
	fflush(stdout);
}

static volatile bool g_running = true;

static void on_signal(int sig)
{
	(void)sig;
	g_running = false;
}

/* ── mosquitto callbacks ────────────────────────────────────────────────── */

/* TLV serialization helpers for the manual response builder */

static int put_tlv_mac(uint8_t *buf, size_t cap, size_t *off,
		       uint16_t type, const uint8_t *mac)
{
	if (*off + 4 + 6 > cap) return -1;
	put_u16(buf + *off, type); *off += 2;
	put_u16(buf + *off, 6);    *off += 2;
	memcpy(buf + *off, mac, 6); *off += 6;
	return 0;
}

static int put_tlv_u8(uint8_t *buf, size_t cap, size_t *off,
		      uint16_t type, uint8_t val)
{
	if (*off + 4 + 1 > cap) return -1;
	put_u16(buf + *off, type); *off += 2;
	put_u16(buf + *off, 1);    *off += 2;
	buf[(*off)++] = val;
	return 0;
}

static int put_tlv_u16(uint8_t *buf, size_t cap, size_t *off,
		       uint16_t type, uint16_t val)
{
	if (*off + 4 + 2 > cap) return -1;
	put_u16(buf + *off, type); *off += 2;
	put_u16(buf + *off, 2);    *off += 2;
	put_u16(buf + *off, val);  *off += 2;
	return 0;
}

static int put_tlv_bytes(uint8_t *buf, size_t cap, size_t *off,
			 uint16_t type, const uint8_t *data, uint16_t dlen)
{
	if (*off + 4 + dlen > cap) return -1;
	put_u16(buf + *off, type);  *off += 2;
	put_u16(buf + *off, dlen);  *off += 2;
	memcpy(buf + *off, data, dlen); *off += dlen;
	return 0;
}

static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
	struct test_ctx *ctx = obj;
	uint8_t         buf[6];
	int             len;

	if (rc != 0) {
		fprintf(stderr, "[MQTT] Connection refused by broker (rc=%d)\n",
			rc);
		return;
	}

	ctx->connected = true;
	printf("\n[MQTT] Connected to broker %s:%d\n",
	       ctx->broker_host, ctx->broker_port);

	mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_TRANSMIT "/#", 0);
	printf("[MQTT] Subscribed to " MQTT_TOPIC_TRANSMIT "/#\n");

	/* Auto-send initial ping so the user sees a response immediately. */
	len = build_ping_msg(buf, sizeof(buf));
	if (len > 0 &&
	    mosquitto_publish(mosq, NULL,
			      MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SYS,
			      len, buf, 0, false) == MOSQ_ERR_SUCCESS) {
		ctx->tx_count++;
		printf("[TX ] CMD_SYS_PING -> "
		       MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SYS
		       " (%d bytes)  [auto-sent on connect]\n", len);
	}

	printf("Type 'ping' to re-send, 'help' for all commands.\n> ");
	fflush(stdout);
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
	struct test_ctx *ctx = obj;

	(void)mosq;
	ctx->connected = false;
	if (rc != 0)
		fprintf(stderr, "\n[MQTT] Unexpected disconnect (rc=%d)\n", rc);
	else
		printf("\n[MQTT] Disconnected\n");
}

static void on_message(struct mosquitto *mosq, void *obj,
		       const struct mosquitto_message *msg)
{
	struct test_ctx *ctx = obj;
	const uint8_t   *data;
	int              data_len;

	(void)mosq;
	if (!msg || !msg->topic)
		return;

	ctx->rx_count++;
	data     = (const uint8_t *)msg->payload;
	data_len = msg->payloadlen;

	decode_and_print(msg->topic, data, data_len);
}

static void on_subscribe(struct mosquitto *mosq, void *obj, int mid,
			 int qos_count, const int *granted_qos)
{
	(void)mosq; (void)obj;
	printf("[MQTT] SUBACK mid=%d granted_qos=%d\n",
	       mid, qos_count > 0 ? granted_qos[0] : -1);
}

/* ── Command line ───────────────────────────────────────────────────────── */

static void print_help(void)
{
	printf("Commands:\n"
			"  ping              Send CMD_ID_SYS_PING; hostapd replies with version info\n"
			"  stats             Print TX/RX counters and connection status\n"
			"  help              Show this message\n"
			"\n"
			"  NeighborDB Commands:\n"
			"  ndb_set <ap_alid> <has_smd_id> [smd_id] <mld_or_0> <bssid1> <nre1_hex> [<mld_or_0> <bssid2> <nre2_hex> ...]\n"
			"                    Set multiple neighbor entries; each NRE is tagged with its own mld_addr\n"
			"                    Use 0/-/none for tuple(s) without mld_addr\n"
			"                    (repeat same mld_addr per tuple if needed)\n"
			"                    Example: ndb_set 00:00:00:00:00:00 0 \\\n"
			"                             11:22:33:44:55:01 11:22:33:44:55:02 <nre1_hex> \\\n"
			"                             0                 11:22:33:44:55:03 <nre2_hex>\n"
			"\n"
				"  ndb_get <ap_alid> <has_smd_id> [smd_id get_global_entries] <has_mld_addr> [mld_addr] <has_bssid> [bssid]\n"
				"                    Scope priority:\n"
				"                      1. SMD_ID (has_smd_id=1): get_global_entries=0 -> self only, 1 -> non-self only\n"
				"                      2. MLD_ADDR (has_smd_id=0 and has_mld_addr=1)\n"
				"                      3. BSSID (has_smd_id=0, has_mld_addr=0, has_bssid=1)\n"
				"                    Example (SMD self):   ndb_get 00:00:00:00:00:00 1 aa:bb:cc:dd:ee:01 0 0 0\n"
				"                    Example (SMD global): ndb_get 00:00:00:00:00:00 1 aa:bb:cc:dd:ee:01 1 0 0\n"
				"                    Example (MLD):        ndb_get 00:00:00:00:00:00 0 1 11:22:33:44:55:01 0\n"
				"                    Example (BSSID):      ndb_get 00:00:00:00:00:00 0 0 1 11:22:33:44:55:02\n"
			"\n"
			"  ndb_clear <ap_alid> <has_smd_id> [smd_id] <has_mld_addr> [mld_addr] <has_bssid> [bssid]\n"
			"                    Scope priority (non-self entries only):\n"
			"                      1. SMD_ID (has_smd_id=1)\n"
			"                      2. MLD_ADDR (has_smd_id=0 and has_mld_addr=1)\n"
			"                      3. BSSID (has_smd_id=0, has_mld_addr=0 and has_bssid=1)\n"
			"                      4. none set -> clear all non-self entries\n"
			"                    Example (SMD):   ndb_clear 00:00:00:00:00:00 1 aa:bb:cc:dd:ee:01 0 0\n"
			"                    Example (MLD):   ndb_clear 00:00:00:00:00:00 0 1 11:22:33:44:55:01 0\n"
			"                    Example (BSSID): ndb_clear 00:00:00:00:00:00 0 0 1 11:22:33:44:55:02\n"
			"                    Example (all):   ndb_clear 00:00:00:00:00:00 0 0 0\n"
			"\n"
			"  quit              Disconnect and exit  (also: q)\n");
}


static int cmd_ping(struct test_ctx *ctx)
{
	uint8_t buf[6];
	int     len;

	if (!ctx->connected) {
		fprintf(stderr, "[ERR] Not connected to broker\n");
		return -1;
	}

	len = build_ping_msg(buf, sizeof(buf));
	if (len < 0)
		return -1;

	if (mosquitto_publish(ctx->mosq, NULL,
				MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SYS,
				len, buf, 0, false) != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[ERR] Publish failed\n");
		return -1;
	}
	ctx->tx_count++;
	printf("[TX ] CMD_SYS_PING -> "
			MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SYS
			" (%d bytes)\n", len);
	return 0;
}

static void process_line(struct test_ctx *ctx, char *line)
{
	char *p = line;

	while (*p == ' ' || *p == '\t') p++;

	/* Strip trailing newline / CR */
	char *e = p + strlen(p);
	while (e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
		*--e = '\0';

	if (*p == '\0') {
		printf("> ");
		fflush(stdout);
		return;
	}

	if (strcmp(p, "quit") == 0 || strcmp(p, "q") == 0) {
		g_running = false;
		return;
	}

	if (strcmp(p, "help") == 0) {
		print_help();
	} else if (strcmp(p, "ping") == 0) {
		cmd_ping(ctx);
	} else if (strcmp(p, "stats") == 0) {
		printf("[STATS] connected=%s  TX=%lu  RX=%lu\n",
		       ctx->connected ? "yes" : "no",
		       ctx->tx_count, ctx->rx_count);
	} else if (strncmp(p, "ndb_", 4) == 0) {
		char *av[64], *tok;
		char *saveptr = NULL;
		int ac = 0;
		char tmp[MQTT_TEST_CMD_BUF_LEN];
		uint8_t ap_alid[6], smd_id[6], mld_addr[6], bssid[6];
		uint8_t has_smd = 0;
		const uint8_t zero_mac[6] = {0};

		snprintf(tmp, sizeof(tmp), "%s", p);
		tok = strtok_r(tmp, " \t", &saveptr);
		while (tok && ac < (int) (sizeof(av) / sizeof(av[0]))) {
			av[ac++] = tok;
			tok = strtok_r(NULL, " \t", &saveptr);
		}

		if (ac >= 3)
			has_smd = (uint8_t) atoi(av[2]);

		if (strcmp(av[0], "ndb_set") == 0) {
			int idx;
			int fixed_args;
			int rem;
			size_t tuple_cnt = 0;
			size_t i;
			struct ndb_set_tuple *tuples = NULL;
			uint8_t *tuple_mlds = NULL;
			uint8_t *tuple_bssids = NULL;
			uint8_t **tuple_nres = NULL;
			uint16_t *tuple_lens = NULL;
			uint8_t *tmp_blob = NULL;

			if (parse_mac(av[1], ap_alid) < 0) {
				fprintf(stderr, "[ERR] bad ap_alid\n");
				goto ndb_set_out;
			}

			idx = 3;
			if (has_smd) {
				if (ac < 7) {
					fprintf(stderr, "[ERR] usage: ndb_set <ap_alid> <has_smd_id> [smd_id] <mld1> <bssid1> <nre1_hex> [<mld2> <bssid2> <nre2_hex> ...]\n");
					goto ndb_set_out;
				}
				if (parse_mac(av[idx++], smd_id) < 0) {
					fprintf(stderr, "[ERR] bad smd_id\n");
					goto ndb_set_out;
				}
				fixed_args = 4;
			} else {
				if (ac < 6) {
					fprintf(stderr, "[ERR] usage: ndb_set <ap_alid> <has_smd_id> <mld1> <bssid1> <nre1_hex> [<mld2> <bssid2> <nre2_hex> ...]\n");
					goto ndb_set_out;
				}
				memcpy(smd_id, zero_mac, 6);
				fixed_args = 3;
			}

			rem = ac - fixed_args;
			if (rem < 3 || (rem % 3) != 0) {
				fprintf(stderr, "[ERR] provide <mld_addr> <bssid> <nre_hex> tuples\n");
				goto ndb_set_out;
			}

			tuple_cnt = (size_t) (rem / 3);
			tuples = calloc(tuple_cnt, sizeof(*tuples));
			tuple_mlds = calloc(tuple_cnt, 6);
			tuple_bssids = calloc(tuple_cnt, 6);
			tuple_nres = calloc(tuple_cnt, sizeof(*tuple_nres));
			tuple_lens = calloc(tuple_cnt, sizeof(*tuple_lens));
			if (!tuples || !tuple_mlds || !tuple_bssids || !tuple_nres || !tuple_lens) {
				fprintf(stderr, "[ERR] alloc failed\n");
				goto ndb_set_out;
			}

			for (i = 0; i < tuple_cnt; i++) {
				{
					const char *mld_tok = av[idx++];
					if (strcmp(mld_tok, "0") == 0 ||
					    strcmp(mld_tok, "-") == 0 ||
					    strcmp(mld_tok, "none") == 0) {
						memset(&tuple_mlds[i * 6], 0, 6);
						tuples[i].has_mld_addr = 0;
						tuples[i].mld_addr = NULL;
					} else if (parse_mac(mld_tok, &tuple_mlds[i * 6]) < 0) {
						fprintf(stderr, "[ERR] bad mld_addr at tuple %zu\n", i);
						goto ndb_set_out;
					} else {
						tuples[i].has_mld_addr = 1;
						tuples[i].mld_addr = &tuple_mlds[i * 6];
					}
				}
				if (parse_mac(av[idx++], &tuple_bssids[i * 6]) < 0) {
					fprintf(stderr, "[ERR] bad bssid at tuple %zu\n", i);
					goto ndb_set_out;
				}
				if (parse_hex_blob(av[idx++], &tmp_blob, &tuple_lens[i]) < 0) {
					fprintf(stderr, "[ERR] bad nre_hex at tuple %zu\n", i);
					goto ndb_set_out;
				}
					tuple_nres[i] = tmp_blob;
					tmp_blob = NULL;
					tuples[i].bssid = &tuple_bssids[i * 6];
					tuples[i].nre = tuple_nres[i];
					tuples[i].nre_len = tuple_lens[i];
			}

			cmd_neighbor_db_set(ctx, ap_alid, has_smd, smd_id,
					    tuples, tuple_cnt);

ndb_set_out:
			free(tmp_blob);
			if (tuple_nres) {
				for (i = 0; i < tuple_cnt; i++)
					free(tuple_nres[i]);
			}
			free(tuple_nres);
			free(tuple_lens);
			free(tuple_mlds);
			free(tuple_bssids);
			free(tuples);
		}
			else if (strcmp(av[0], "ndb_get") == 0) {
	                        int idx;
	                        uint8_t get_global_entries = 0;
	                        uint8_t has_mld_addr = 0;
	                        uint8_t has_bssid = 0;

	                        printf("inside if block of ndb_get");
	                        /* ndb_get ap_alid has_smd_id [smd_id get_global_entries] has_mld_addr [mld_addr] has_bssid [bssid] */
	                        if ((has_smd && (ac < 7 || ac > 9)) || (!has_smd && (ac < 5 || ac > 7))) {
	                                fprintf(stderr, "[ERR] usage: ndb_get <ap_alid> <has_smd_id> [smd_id get_global_entries] <has_mld_addr> [mld_addr] <has_bssid> [bssid]\n");
	                        } else if (parse_mac(av[1], ap_alid) < 0) {
	                                fprintf(stderr, "[ERR] bad ap_alid\n");
	                        } else {
	                                idx = 3;
	                                if (has_smd) {
	                                        if (parse_mac(av[idx++], smd_id) < 0) {
	                                                fprintf(stderr, "[ERR] bad smd_id\n");
	                                                goto ndb_get_out;
	                                        }
	                                        get_global_entries = (uint8_t) atoi(av[idx++]);
	                                } else {
	                                        memcpy(smd_id, zero_mac, 6);
	                                }

	                                has_mld_addr = (uint8_t) atoi(av[idx++]);
	                                if (has_mld_addr) {
	                                        if (idx >= ac || parse_mac(av[idx++], mld_addr) < 0) {
	                                                fprintf(stderr, "[ERR] bad mld_addr\n");
	                                                goto ndb_get_out;
	                                        }
	                                } else {
	                                        memcpy(mld_addr, zero_mac, 6);
	                                }

	                                has_bssid = (uint8_t) atoi(av[idx++]);
	                                if (has_bssid) {
	                                        if (idx >= ac || parse_mac(av[idx++], bssid) < 0) {
	                                                fprintf(stderr, "[ERR] bad bssid\n");
	                                                goto ndb_get_out;
	                                        }
	                                } else {
	                                        memcpy(bssid, zero_mac, 6);
	                                }

	                                if (idx != ac) {
	                                        fprintf(stderr, "[ERR] unexpected extra args\n");
	                                        goto ndb_get_out;
	                                }

	                                printf("invoking ndb_get func()");
	                                cmd_neighbor_db_get(ctx, ap_alid, has_smd, smd_id,
	                                                    get_global_entries,
	                                                    has_mld_addr, mld_addr,
	                                                    has_bssid, bssid);
	  ndb_get_out: ;
	                        }
	                }
		else if (strcmp(av[0], "ndb_clear") == 0) {
			int idx;
			uint8_t has_mld_addr = 0;
			uint8_t has_bssid = 0;
			/* ndb_clear ap_alid has_smd_id [smd_id] has_mld_addr [mld_addr] has_bssid [bssid] */
			printf("inside ndb_clear if block");
			if ((has_smd && (ac < 6 || ac > 8)) || (!has_smd && (ac < 5 || ac > 7))) {
				fprintf(stderr, "[ERR] usage: ndb_clear <ap_alid> <has_smd_id> [smd_id] <has_mld_addr> [mld_addr] <has_bssid> [bssid]\n");
			} else if (parse_mac(av[1], ap_alid) < 0) {
				fprintf(stderr, "[ERR] bad ap_alid\n");
			} else {
				idx = 3;
				if (has_smd) {
					if (parse_mac(av[idx++], smd_id) < 0) {
						fprintf(stderr, "[ERR] bad smd_id\n");
						goto ndb_clear_out;
					}
				} else {
					memcpy(smd_id, zero_mac, 6);
				}

				has_mld_addr = (uint8_t) atoi(av[idx++]);
				if (has_mld_addr) {
					if (idx >= ac || parse_mac(av[idx++], mld_addr) < 0) {
						fprintf(stderr, "[ERR] bad mld_addr\n");
						goto ndb_clear_out;
					}
				} else {
					memcpy(mld_addr, zero_mac, 6);
				}

				has_bssid = (uint8_t) atoi(av[idx++]);
				if (has_bssid) {
					if (idx >= ac || parse_mac(av[idx++], bssid) < 0) {
						fprintf(stderr, "[ERR] bad bssid\n");
						goto ndb_clear_out;
					}
				} else {
					memcpy(bssid, zero_mac, 6);
				}

				if (idx != ac) {
					fprintf(stderr, "[ERR] unexpected extra args\n");
					goto ndb_clear_out;
				}

				printf("invoking ndb_clear func()");
				cmd_neighbor_db_clear(ctx, ap_alid, has_smd, smd_id,
						      has_mld_addr, mld_addr,
						      has_bssid, bssid);
ndb_clear_out: ;
			}
		} else {
			fprintf(stderr, "[ERR] Unknown command '%s'. "
					"Type 'help' for usage.\n", p);
		}

		printf("> ");
		fflush(stdout);
}

}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	const char      *broker_host = "localhost";
	int              broker_port = 1883;
	struct test_ctx  ctx;
	int              mosq_fd;
	int              rc;

	if (argc > 1) broker_host = argv[1];
	if (argc > 2) broker_port = atoi(argv[2]);

	memset(&ctx, 0, sizeof(ctx));
	ctx.broker_host         = broker_host;
	ctx.broker_port         = broker_port;

	signal(SIGINT,  on_signal);
	signal(SIGTERM, on_signal);

	/* Non-blocking stdin so mosquitto_loop() drives the network. */
	fcntl(STDIN_FILENO, F_SETFL,
	      fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);

	mosquitto_lib_init();

	ctx.mosq = mosquitto_new("hostapd-mqtt-test", true, &ctx);
	if (!ctx.mosq) {
		fprintf(stderr, "[ERR] mosquitto_new() failed\n");
		return 1;
	}

	mosquitto_connect_callback_set  (ctx.mosq, on_connect);
	mosquitto_disconnect_callback_set(ctx.mosq, on_disconnect);
	mosquitto_message_callback_set  (ctx.mosq, on_message);
	mosquitto_subscribe_callback_set(ctx.mosq, on_subscribe);

	printf("[MQTT] Connecting to broker %s:%d ...\n",
	       broker_host, broker_port);

	rc = mosquitto_connect(ctx.mosq, broker_host, broker_port, 60);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "[ERR] mosquitto_connect: %s\n",
			mosquitto_strerror(rc));
		mosquitto_destroy(ctx.mosq);
		mosquitto_lib_cleanup();
		return 1;
	}

	mosq_fd = mosquitto_socket(ctx.mosq);

	while (g_running) {
		fd_set         rfds;
		struct timeval tv = { 0, 100000 }; /* 100 ms */
		int            nfds;

		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);
		if (mosq_fd >= 0)
			FD_SET(mosq_fd, &rfds);

		nfds = (mosq_fd > STDIN_FILENO ? mosq_fd : STDIN_FILENO) + 1;

		if (select(nfds, &rfds, NULL, NULL, &tv) < 0) {
			if (errno == EINTR) continue;
			perror("select");
			break;
		}

		mosquitto_loop(ctx.mosq, 0, 1);
		mosq_fd = mosquitto_socket(ctx.mosq);

		if (FD_ISSET(STDIN_FILENO, &rfds)) {
			char line[256];

			if (fgets(line, sizeof(line), stdin))
				process_line(&ctx, line);
			else if (feof(stdin))
				g_running = false;
		}
	}

	mosquitto_disconnect(ctx.mosq);
	mosquitto_loop(ctx.mosq, 200, 1);
	mosquitto_destroy(ctx.mosq);
	mosquitto_lib_cleanup();

	printf("[MQTT] Exited. TX=%lu  RX=%lu\n",
	       ctx.tx_count, ctx.rx_count);
	return 0;
}
