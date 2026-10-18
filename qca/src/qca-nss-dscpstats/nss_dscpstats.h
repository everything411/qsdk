/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * nss_dscpstats.h
 */
#ifndef __NSSSTATDSCP__H
#define __NSSSTATDSCP__H

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define nss_dscpstats_warn(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_dscpstats_info(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_dscpstats_trace(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (nss_dscpstats_DEBUG_LEVEL < 2)
#define nss_dscpstats_warn(s, ...)
#else
#define nss_dscpstats_warn(s, ...) pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (nss_dscpstats_DEBUG_LEVEL < 3)
#define nss_dscpstats_info(s, ...)
#else
#define nss_dscpstats_info(s, ...) pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (nss_dscpstats_DEBUG_LEVEL < 4)
#define nss_dscpstats_trace(s, ...)
#else
#define nss_dscpstats_trace(s, ...) pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define nss_dscpstats_info_always(s, ...) printk(KERN_INFO"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#define NSS_DSCPSTATS_CLIENT_HASH_RATIO 4

/**
 * nss_dscpstats_wanif
 *	NSS DSCPSTATS strcture to WAN interface
 */
struct nss_dscpstats_wanif {
	struct hlist_node wan_node;			/* hlist node to store next wan node */
	uint8_t dev_name[16];				/* name of the wan interface */
	uint64_t dscp_mask;				/* bits for which dscp values are enabled */
};

/**
 * nss_dscpstats_dscp_node
 *	NSS DSCPSTATS strcture to represent tx and rx bytes of a dscp value
 */
struct nss_dscpstats_dscp_node {
	uint32_t dscp;					/* dscp value of the dscp node */
	uint64_t rxBytes;				/* rx bytes of dscp node */
	uint64_t txBytes;				/* tx bytes of dscp node */
	uint64_t total_rxBytes;				/* total rx bytes of dscp node */
	uint64_t total_txBytes;				/* total tx bytes of dscp node */
	struct hlist_node dscp_node;			/* hlist node to store next dscp node */
};

/**
 * nss_dscpstats_mac_node
 *	NSS DSCPSTATS strcture to represent dscp data of a client
 */
struct nss_dscpstats_mac_node {
	uint64_t mac_addr;				/* mac address of the client */
	struct hlist_head dscp_list;			/* hlist to store dscp nodes of client */
	struct hlist_node mac_node;			/* hlist_node to store next mac node */
};

/**
 * nss_dscpstats_common_stats
 *	NSS DSCPSTATS strcture to represent general statistics
 */
struct nss_dscpstats_common_stats {
	uint32_t tot_mac_add;		/* Total number of mac node added */
	uint32_t tot_mac_del;		/* Total number of mac node added */
	uint32_t mac_node_add_fail;	/* Total number of mac node addition failed */
	uint32_t tot_read_req;		/* Total number of times userspace read the statistics */
	atomic_t tot_act_mac;		/* Total number of active mac node */
};

/**
 * nss_dscpstats
 *	NSS DSCPSTATS strcture to represent storage data
 */
struct nss_dscpstats {
	uint32_t hash_size;				/* size of the hash table */
	spinlock_t lock;				/* spin lock */
	struct dentry *dentry;			/* base debugfs */
	struct dentry *conf_dentry;		/* configuration debugfs */
	struct dentry *stats_dentry;		/* statistics debugfs */
	struct hlist_head wan_list;			/* wan interface list */
	struct hlist_head *mac_hash_table;		/* hash table to store client mac node */
	struct nss_dscpstats_common_stats common_stats;	/* common statistics */
};

/**
 * nss_dscpstats_dbg_read_ctx
 *	Store the current read context to be used in next call.
 */
struct nss_dscpstats_dbg_read_ctx {
	uint32_t hash_index;	/* hash index */
	bool read_done;		/* completed reading entire table*/
	bool mac_addr_written;	/* Mac address written to buffer */
	struct nss_dscpstats_mac_node *mac_obj;		/* mac node */
	struct nss_dscpstats_dscp_node *dscp_obj;	/* dscp  node */
};

/**
 * nss_dscpstats_ret
 *	dscp stats return code
 */
enum nss_dscpstats_ret {
	DSCP_STATS_RET_SUCCESS = 0,			/* Success */
	DSCP_STATS_RET_FAIL,				/* Failure */
	DSCP_STATS_RET_ALREADY_ENABLED,			/* DSCP is already enabled */
	DSCP_STATS_RET_ENABLE_FAIL_OOM,			/* DSCP enable failed due to out of memory */
	DSCP_STATS_RET_ALREADY_DISABLED,		/* DSCP is already disabled */
	DSCP_STATS_RET_INVALID_VALUE,			/* Invalid DSCP value */
	DSCP_STATS_RET_ENABLE_FAIL_MAX_CLIENT_LIMIT,	/* DSCP enabled failed due to max number of clients reached */
};

extern struct nss_dscpstats nss_dscpstats_ctx;
extern uint32_t num_clients;

enum nss_dscpstats_ret nss_dscpstats_db_disable_dscp(uint64_t dscp_mask, uint8_t dev_name[]);
enum nss_dscpstats_ret nss_dscpstats_db_enable_dscp(uint64_t dscp_mask, uint8_t dev_name[]);
void nss_dscpstats_db_reset(void);
uint32_t nss_dscpstats_db_init_table(void);
void nss_dscpstats_db_free_table(void);
void nss_dscpstats_db_stats_process(struct net_device *from_dev, struct net_device *to_dev, uint8_t *smac,
		uint8_t *dmac, uint8_t dscp_marked, uint8_t flow_mark, uint8_t reply_mark, uint8_t flow_dscp,
		uint8_t reply_dscp, uint32_t tx_bytes, uint32_t rx_bytes, bool is_ipv6);
void nss_dscpstats_db_stop(void);
#endif /* __NSSSTATDSCP__H */
