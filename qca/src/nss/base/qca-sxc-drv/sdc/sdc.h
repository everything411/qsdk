/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
*/
#ifndef SDC_H
#define SDC_H

#include <linux/etherdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_netdev.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>

#include "sdc_features_ext.h"
#include "sdc_macdb.h"
#include "sdc_telemetry.h"

#define DEBUG_ERR(fmt, ...) \
        pr_err("[ERROR][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#if defined(CONFIG_DYNAMIC_DEBUG)
#define DEBUG_WARN(fmt, ...) \
        pr_debug("[WARN][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG_INFO(fmt, ...) \
        pr_debug("[INFO][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_WARN(fmt, ...) \
        pr_warn("[WARN ][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define DEBUG_INFO(fmt, ...) \
        pr_info("[INFO ][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#endif

#define SDC_MAX_IF 10
#define SDC_MAX_PRINT_OUTPUT 512

/*
 * sdc_tracked_if
 *	Store tracked interface netdevice
 *	pointer and hooks.
 */
struct sdc_tracked_if {
	struct net_device *dev;			/** Netdev pointer*/
	struct nf_hook_ops nf_ingress_hook;	/** Ingress hook point*/
	struct nf_hook_ops nf_egress_hook;	/** Egress hook point*/
};

/*
 * sdc_ctx
 *	SDC context structure
 */
struct sdc_ctx {
	struct sdc_tracked_if *tif[SDC_MAX_IF];		/** list of tracked interface*/
	struct ctl_table_header *sysctl_header;		/** Sysctl table header */
	spinlock_t lock;				/** global lock for SDC context.*/
	uint8_t num_devs;				/** Number of devices.*/
	struct sdc_macdb_ctx macdb_ctx;			/** MAC DB context*/
	struct sdc_telemetry_ctx tele_ctx;		/** Telemetry context*/
	uint32_t hash_seed;				/** Hash seed for macdb */

	uint32_t buf_size;				/** Per-device telemetry buffer size in bytes (sysctl).*/
	uint32_t collection_timeout;			/** Collection window in seconds (sysctl).*/

	atomic_t egress_dhcp_miss;			/** Egress DHCP frame with no macdb entry for chaddr.*/
	atomic_t egress_dns_miss;			/** Egress DNS response with no macdb entry for dst MAC.*/
	atomic_t tele_queue_drop;			/** Device buffers dropped due to full telemetry queue.*/

	/* Global per-type TLV drop counters — exposed via procfs for operational monitoring. */
	atomic_t drop_dhcp;
	atomic_t drop_dns;
	atomic_t drop_mdns;
	atomic_t drop_ssdp;
	atomic_t drop_tls;
	atomic_t drop_quic;
};

extern int sdc_add_tracked_if_by_name(const char *ifname);
extern void sdc_del_tracked_if_by_name(const char *ifname);
extern void sdc_clear_tracked_if(void);

#endif
