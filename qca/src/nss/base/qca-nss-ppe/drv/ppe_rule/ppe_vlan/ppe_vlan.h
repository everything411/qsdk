/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_vlan.h>
#include "ppe_drv_vlan.h"
#include "ppe_vlan_dump.h"
#include "ppe_vlan_stats.h"

/*
 * Maximum number of VLAN rules
 */
#if defined(NSS_PPE_IPQ96XX)
#define PPE_VLAN_RULE_ID_MAX 256	/* Maximum number of VLAN rules for Juhu */
#else
#define PPE_VLAN_RULE_ID_MAX 384	/* Maximum number of VLAN rules for Hermosa */
#endif

/*
 * PPE VLAN debug macros
 */
#if (PPE_VLAN_DEBUG_LEVEL == 3)
#define ppe_vlan_assert(c, s, ...)
#else
#define ppe_vlan_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_vlan_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_vlan_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_vlan_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_VLAN_DEBUG_LEVEL < 2)
#define ppe_vlan_warn(s, ...)
#else
#define ppe_vlan_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_VLAN_DEBUG_LEVEL < 3)
#define ppe_vlan_info(s, ...)
#else
#define ppe_vlan_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_VLAN_DEBUG_LEVEL < 4)
#define ppe_vlan_trace(s, ...)
#else
#define ppe_vlan_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_vlan
 *	Structure for VLAN rule
 */
struct ppe_vlan {
	/*
	 * list head for active list
	 */
	struct list_head list;			/* List head for active list */

	/*
	 * basic info
	 */
	uint32_t rule_id;			/* Associated rule ID */
	struct ppe_drv_vlan_ctx *ctx;		/* PPE driver rule context */
	uint8_t counter_id;			/* Associated counter ID */
	bool counter_valid_flag;		/* Valid counter flag */
	bool hgu_rule_valid;			/* Flag for HGU rule */

	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_vlan_cfg info;	/* VLAN rule configuration */

	/*
	 * References
	 */
	struct kref ref_cnt;			/* Reference count */

	struct ppe_vlan_rule rule;		/* Copy of rule information */
};

/*
 * ppe_vlan_base
 *	PPE VLAN base structure
 */
struct ppe_vlan_base {
	spinlock_t lock;			/* VLAN lock */

	struct kref ref;			/* Reference count */

	/*
	 * General stats
	 */
	struct ppe_vlan_stats stats;		/* VLAN statistics */
	struct dentry *dentry;			/* Debugfs entry */

	/*
	 * Active list of VLAN rules
	 */
	struct list_head active_rules;		/* List of active VLAN rules */

	/*
	 * Device ID for VLAN dump character device.
	 */
	int vlan_dump_major_id;			/* Major ID for VLAN dump device */
};

extern struct ppe_vlan_base ppe_vlan_gbl;

/*
 * VLAN manager APIs.
 */
void ppe_vlan_deinit(void);
void ppe_vlan_init(struct dentry *d_rule);
