/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/list.h>
#include <ppe_tun_rps.h>
#include "ppe_tun_rps_stats.h"

#define PPE_TUN_RPS_RULES_MAX 8

/*
 * ppe_tun_rps_rule_entry
 *	RPS rule tracking list node entry
 */
struct ppe_tun_rps_rule_entry {
	struct list_head list;
	uint8_t ip_proto;
	union {
		struct ppe_tun_rps_ipv4_rule_create_msg v4;
		struct ppe_tun_rps_ipv6_rule_create_msg v6;
	} msg;
};

/*
 * PPE Tunnel RPS related macros
 */
#if (PPE_TUN_RPS_DEBUG_LEVEL == 3)
#define ppe_tun_rps_assert(c, s, ...)
#else
#define ppe_tun_rps_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_tun_rps_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_tun_rps_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_tun_rps_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_TUN_RPS_DEBUG_LEVEL < 2)
#define ppe_tun_rps_warn(s, ...)
#else
#define ppe_tun_rps_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_TUN_RPS_DEBUG_LEVEL < 3)
#define ppe_tun_rps_info(s, ...)
#else
#define ppe_tun_rps_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_TUN_RPS_DEBUG_LEVEL < 4)
#define ppe_tun_rps_trace(s, ...)
#else
#define ppe_tun_rps_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_tun_rps
 *	Global ppe tun_rps global instance
 */
struct ppe_tun_rps {
	spinlock_t lock;                                /* PPE lock */
	struct ppe_tun_rps_stats stats;                /* PPE tun rps statistics */
	struct dentry *dentry;				/* Debugfs entry */
	struct list_head rules_list;			/* Active rules list */
};

void ppe_tun_rps_deinit(void);
int ppe_tun_rps_init(struct dentry *dentry);
void ppe_tun_rps_flush_rules(void);

extern struct ppe_tun_rps gbl_tun_rps;
