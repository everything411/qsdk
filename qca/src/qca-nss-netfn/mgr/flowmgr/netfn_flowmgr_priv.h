/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/**
 * @file netfn_flowmgr_priv.h
 *      Netfn flow manager private header file.
 */
#ifndef __NETFN_FLOWMGR_PRIV_H
#define __NETFN_FLOWMGR_PRIV_H

#include <netfn_flowmgr.h>
#include "netfn_flowmgr_stats.h"

extern struct netfn_flowmgr netfn_flowmgr_gbl;
extern uint32_t static_dbg_level;
extern uint16_t ipv4_stats_sync_period;
extern uint16_t ipv6_stats_sync_period;
extern bool enable_stats_sync;

/*
 * netfn_flowmgr_static_dbg_level
 *      Netfn flowmgr static debug level
 */
enum netfn_flowmgr_static_dbg_level {
        NETFN_FLOWMGR_STATIC_DBG_LEVEL_NONE,
        NETFN_FLOWMGR_STATIC_DBG_LEVEL_WARN,
        NETFN_FLOWMGR_STATIC_DBG_LEVEL_INFO,
        NETFN_FLOWMGR_STATIC_DBG_LEVEL_TRACE,
};

/* Define a macro to set  netfn flowmgr status code */
#define NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(netfn_flowmgr_error, ae_error) \
		((netfn_flowmgr_ret_t)((netfn_flowmgr_error) | (ae_error << 8)))

#if (NETFN_FLOWMGR_DEBUG_LEVEL == 3)
#define netfn_flowmgr_assert(c, s, ...)
#else
#define netfn_flowmgr_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif /* NETFN_FLOWMGR_DEBUG_LEVEL */

/*
 * If dynamic debug is enabled, use pr_debug.
 */
#if defined(CONFIG_DYNAMIC_DEBUG)
#define netfn_flowmgr_warn(s, ...)	pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_flowmgr_info(s, ...)	pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_flowmgr_trace(s, ...)	pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (NETFN_FLOWMGR_DEBUG_LEVEL < 2)
#define netfn_flowmgr_warn(s, ...)
#else
#define netfn_flowmgr_warn(s, ...) \
	if (static_dbg_level >= NETFN_FLOWMGR_STATIC_DBG_LEVEL_WARN) \
		pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETFN_FLOWMGR_DEBUG_LEVEL < 3)
#define netfn_flowmgr_info(s, ...)
#else
#define netfn_flowmgr_info(s, ...) \
	if (static_dbg_level >= NETFN_FLOWMGR_STATIC_DBG_LEVEL_INFO) \
		pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETFN_FLOWMGR_DEBUG_LEVEL < 4)
#define netfn_flowmgr_trace(s, ...)
#else
#define netfn_flowmgr_trace(s, ...) \
	if (static_dbg_level >= NETFN_FLOWMGR_STATIC_DBG_LEVEL_TRACE) \
		pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * netfn_flowmgr_ae_ops
 *	AE operations
 */
struct netfn_flowmgr_ae_ops {
	netfn_flowmgr_ret_t (*create_rule)(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply);   /* Function pointer to create flow rule */
	netfn_flowmgr_ret_t (*destroy_rule)(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply);  /* Function pointer to destroy flow rule */
	netfn_flowmgr_ret_t (*get_stats)(struct netfn_flowmgr_flow_conn_stats *conn_stats);  /* Function pointer to get stats of a single flow based on 5 tuple */
	netfn_flowmgr_ret_t (*add_dscp_priority)(struct netfn_flowmgr_dscp_priority *dscpinfo);  /* Function pointer to add dscp priority */
	netfn_flowmgr_ret_t (*del_dscp_priority)(int16_t rule_id);  /* Function pointer to delete dscp priority */
};

/*
 * netfn_flowmgr
 *	Netfn flowmgr base structure
 */
struct netfn_flowmgr {
	spinlock_t lock;							/* Netfn flowmgr lock */
	struct netfn_flowmgr_ae_ops *ae_ops[NETFN_FLOWMGR_AE_TYPE_MAX];		/* AE operations */
	struct ctl_table_header *netfn_flowmgr_header;				/* Netfn flowmgr sysctl */
	netfn_flowmgr_ipv4_stats_callback_t stats_sync_v4_cb;			/* Callback to call to sync IPv4 statistics */
        void *stats_sync_v4_data;						/* Argument of IPv4 callback: stats_sync_v4_cb */
	netfn_flowmgr_ipv6_stats_callback_t stats_sync_v6_cb;			/* Callback to call to IPv6 sync statistics */
        void *stats_sync_v6_data;						/* Argument of IPv6 callback: stats_syncv6__cb */
	struct dentry *dentry;							/* Debugfs entry */
        struct dentry *stats_dentry;						/* Debugfs sub entry */
	struct netfn_flowmgr_debug_stats stats[NETFN_FLOWMGR_AE_TYPE_MAX];	/* Debuggability stats */
};

extern struct netfn_flowmgr netfn_flowmgr_gbl;
bool netfn_flowmgr_ae_ops_register(netfn_flowmgr_ae_type_t ae_type, struct netfn_flowmgr_ae_ops *ops);
void netfn_flowmgr_ae_ops_unregister(netfn_flowmgr_ae_type_t ae_type);

#endif /*__NETFN_FLOWMGR_PRIV_H */
