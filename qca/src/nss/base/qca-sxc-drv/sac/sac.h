/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#ifndef SAC_H
#define SAC_H

#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/atomic.h>

#define SAC_MAX_PRINT_OUTPUT	512
#define SAC_DNS_SPORT		53
#define SAC_DNS_DPORT		53
#define SAC_IFACE_NAME_MAX_SIZE	16
#define SAC_MAX_IFACE		2

#define DEBUG_ERROR(fmt, ...) \
	pr_err("[ERROR][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define DEBUG_WARN(fmt, ...) \
	pr_warn("[WARN ][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define DEBUG_INFO(fmt, ...) \
	pr_info("[INFO ][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define DEBUG_TRACE(fmt, ...) \
	pr_debug("[TRACE][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

/*
 * sac_tracked_iface
 *	Interface to be tracked for desired flow.
 */
struct sac_tracked_iface {
	struct net_device *dev;		/* Net device associated with the interface name */
	struct nf_hook_ops nf_hook;	/* Netfilter hook */
};

/*
 * sac_iface_data
 *	Interface configuration data
 */
struct sac_iface_data {
	uint8_t count;			/* Number of interfaces */
	char name[SAC_MAX_IFACE][SAC_IFACE_NAME_MAX_SIZE];
						/* Name of the interfaces */
};

/*
 * sac_stats
 *	SAC driver stats
 *
 * After adding a new member here, make sure to update the sac_stats_string
 * array in sac.c - the two arrays must stay in sync.
 */
struct sac_stats {
	atomic64_t sac_enable;			/* SAC driver enable status */
	atomic64_t enable_count;		/* Driver enable count */
	atomic64_t disable_count;		/* Driver disable count */
	atomic64_t iface_cfg_rx;		/* Interface configuration received */
	atomic64_t mmap_offset;			/* Current memory offset for m-mapped region */
	atomic64_t invalid_l4;			/* Invalid L4 payload or data */

	atomic64_t dns_parse_success;		/* DNS parsing success */
	atomic64_t dns_parse_fail;		/* DNS parsing failure */
	atomic64_t dns_no_valid_ip4;		/* No valid IPv4 address extracted */
	atomic64_t dns_no_valid_ip6;		/* No valid IPv6 address extracted */
	atomic64_t dns_tx_success;		/* Success in sending DNS response to telemetry */
	atomic64_t dns_tx_fail;			/* Failure in sending DNS response to telemetry */
	atomic64_t dns_tx_limit_reached;	/* Reached max outstanding DNS responses */
	atomic64_t dns_rx_ack;			/* Ack received from user space */
	atomic64_t dns_write_dis;		/* Response send disabled (telemetry full) */
	atomic64_t dns_unsupp_ans_type;		/* Unsupported DNS answer type */
	atomic64_t dns_ans_noip;		/* No IP address in DNS response */

	atomic64_t sni_tcp443_seen;		/* TCP/443 packets examined */
	atomic64_t sni_parse_attempts;		/* ClientHello parse attempts */
	atomic64_t sni_found;			/* SNI successfully extracted */
	atomic64_t sni_parse_fail;		/* Parse errors or non-TLS payload */
	atomic64_t sni_giveup;			/* Flows that hit max attempt limit */
	atomic64_t sni_flow_cache_full;		/* Flow entry alloc failed */
	atomic64_t sni_enqueue_ok;		/* SNI frames written to mmap */
	atomic64_t sni_enqueue_fail;		/* SNI frames dropped */
};

/*
 * sac_ctx
 *	SAC driver context.
 */
struct sac_ctx {
	struct sac_tracked_iface *tif[SAC_MAX_IFACE];
					/* Array of tracked interface */
	struct dentry *dir;		/* Parent debugfs entry */
	uint8_t num_devs;		/* Number of interfaces */
	uint16_t mmap_size;		/* Memory mapped buffer size */
	uint32_t mmap_offset;		/* Memory mapped buffer offset */
	void *mmap_buf;			/* Memory mapped buffer pointer */
	bool enable;			/* Global enable flag for SAC driver */
	spinlock_t lock;		/* Global lock for SAC context.*/
	struct sac_stats stats;		/* Driver stats */

	bool sni_enable;		/* SNI extraction enabled */
	uint8_t sni_max_attempts;	/* Max ClientHello parse tries per flow */
};

void sac_enable(void);
void sac_disable(void);
void sac_handle_iface_config(struct sac_iface_data *data);

#endif /* SAC_H */
