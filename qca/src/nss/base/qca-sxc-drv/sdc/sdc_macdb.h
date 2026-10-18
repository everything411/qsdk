/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#ifndef SDC_MACDB_H
#define SDC_MACDB_H

/*
 * 4k buffer per device.
 */
#define SDC_MACDB_BUF_SZ 4096
#define SDC_MACDB_HASH_BITS 10
#define SDC_MACDB_TIMER_TIMEOUT_SECS 10

struct sdc_ctx;

/*
 * sdc_mabdb_mac_state_t
 *	SDC MAC DB state
 */
typedef enum {
	SDC_MACDB_MAC_STATE_INPROGRESS,
	SDC_MACDB_MAC_STATE_COLLECTED
} sdc_macdb_mac_state_t;

/*
 * sdc_mabdb_entry
 *	SDC MAC database entry structure
 */
struct sdc_macdb_entry {
	struct hlist_node hnode;	/* Hash list node for DB entry.*/
	uint8_t mac[ETH_ALEN];		/* MAC address.*/
	spinlock_t lock;		/* Spinlock for entry update. TODO: Check if this can be removed.*/
	sdc_macdb_mac_state_t state;	/* Entry state denoting data is collected or not.*/
	struct sdc_telemetry_buf *data;	/* Telemetry data.*/

	refcount_t ref;			/* Reference count for entry*/
	struct timer_list timer;	/* Timer to control time out for collecting data.*/

	/* Per-device packet counters — incremented lock-free from ingress and egress hooks.
	 * Only counters not derivable from raw TLV counts are tracked here. */
	atomic_t dhcp_discover_count;
	atomic_t dhcp_offer_count;
	atomic_t dhcp_ack_count;
	atomic_t dhcp_nak_count;
	atomic_t dns_response_count;
	atomic_t dns_nxdomain_count;
	atomic_t dns_servfail_count;
	atomic_t ssdp_pkt_count;
	atomic_t tls_ch_count;
	atomic_t quic_initial_count;
	atomic_t quic_version_bitmap;	/* SDC_QUIC_VERSION_BIT_* OR'd in as versions are seen. */

	/* Per-type TLV drop counters — incremented when buffer full for each protocol. */
	atomic_t drop_dhcp;
	atomic_t drop_dns;
	atomic_t drop_mdns;
	atomic_t drop_ssdp;
	atomic_t drop_tls;
	atomic_t drop_quic;
};

/*
 * sdc_mabdb_ctx
 *	SDC MAC database context structure
 */
struct sdc_macdb_ctx {
	DECLARE_HASHTABLE(htable, SDC_MACDB_HASH_BITS);
};

void sdc_macdb_deref(struct sdc_macdb_ctx *ctx, struct sdc_macdb_entry *entry);
struct sdc_macdb_entry *sdc_macdb_get(struct sdc_macdb_ctx *ctx, const uint8_t *mac);
struct sdc_macdb_entry *sdc_macdb_get_or_create(struct sdc_macdb_ctx *ctx, const uint8_t *mac);
void sdc_macdb_init(struct sdc_macdb_ctx *ctx);
void sdc_macdb_deinit(struct sdc_macdb_ctx *ctx);
void sdc_macdb_clear_collected(struct sdc_macdb_ctx *ctx);
int sdc_macdb_del_by_mac(struct sdc_macdb_ctx *ctx, const uint8_t *mac);
#endif
