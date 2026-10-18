/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_dot1p.h>
#include "ppe_dot1p_dump.h"
#include "ppe_dot1p_stats.h"

/*
 * PPE DOT1P debug macros
 */
#if (PPE_DOT1P_DEBUG_LEVEL == 3)
#define ppe_dot1p_assert(c, s, ...)
#else
#define ppe_dot1p_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_dot1p_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_dot1p_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_dot1p_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_DOT1P_DEBUG_LEVEL < 2)
#define ppe_dot1p_warn(s, ...)
#else
#define ppe_dot1p_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DOT1P_DEBUG_LEVEL < 3)
#define ppe_dot1p_info(s, ...)
#else
#define ppe_dot1p_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DOT1P_DEBUG_LEVEL < 4)
#define ppe_dot1p_trace(s, ...)
#else
#define ppe_dot1p_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_dot1p
 *	Structure for dot1p rule.
 */
struct ppe_dot1p {
	struct list_head list;			/* List head for active list */

	/*
	 * basic info
	 */
	uint32_t rule_valid_flag;		/* Rule valid flags */
	uint32_t rule_id;			/* Associated rule ID */
	uint16_t gem_port_id;			/* Associated gemport ID */
	struct ppe_drv_dot1p_ctx *ctx;		/* PPE driver rule context */

	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_dot1p_rule info;

	struct kref ref_cnt;			/* Reference count */
	struct ppe_dot1p_rule rule;		/* Copy of rule information. */
};

/*
 * ppe_dot1p_policer
 *	Structure for dot1p policer rule.
 */
struct ppe_dot1p_policer {
	struct list_head list;			/* List head for active list */

	/*
	 * basic info
	 */
	uint32_t rule_valid_flag;			/* Rule valid flags */
	uint16_t gemport_id;				/* Associated gemport ID */
	struct ppe_drv_dot1p_policer_ctx *ctx;		/* PPE driver policer rule context */

	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_dot1p_policer_rule info;

	struct kref ref_cnt;			/* Reference count */
	struct ppe_dot1p_policer_rule rule;	/* Copy of rule information. */
};

/*
 * ppe_dot1p_base
 *	PPE DOT1P base structure
 */
struct ppe_dot1p_base {
	spinlock_t lock;				/* DOT1P lock */

	struct ppe_dot1p_stats stats;			/* DOT1P statistics */
	struct dentry *dentry;				/* Debugfs entry */

	struct ppe_dot1p_def_rule def_rule;		/* Default rule dot1p */
	struct kref ref;				/* Reference count */

	struct list_head active_rules;			/* List of active DOT1P rules */

	struct list_head active_policer_rules;		/* List of active DOT1P policer rules */

	uint8_t rule_state;				/* state of active DOT1P rules. */

	/*
	 * Device ID for DOT1P dump character device.
	 */
	int dot1p_dump_major_id;
};

extern struct ppe_dot1p_base ppe_dot1p_gbl;

/*
 * DOT1P manager APIs.
 */
void ppe_dot1p_deinit(void);
void ppe_dot1p_init(struct dentry *d_rule);
