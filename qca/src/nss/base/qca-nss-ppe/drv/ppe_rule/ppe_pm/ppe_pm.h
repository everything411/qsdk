/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_pm.h>
#include "ppe_pm_stats.h"
#include "ppe_pm_dump.h"

/*
 * PPE PM debug macros
 */
#if (PPE_PM_DEBUG_LEVEL == 3)
#define ppe_pm_assert(c, s, ...)
#else
#define ppe_pm_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)

/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_pm_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_pm_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_pm_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_PM_DEBUG_LEVEL < 2)
#define ppe_pm_warn(s, ...)
#else
#define ppe_pm_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_PM_DEBUG_LEVEL < 3)
#define ppe_pm_info(s, ...)
#else
#define ppe_pm_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_PM_DEBUG_LEVEL < 4)
#define ppe_pm_trace(s, ...)
#else
#define ppe_pm_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_pm_counter_ref
 *	Structure for PM counter reference.
 *
 * This structure holds the opaque context pointer returned by the driver
 * and a kref for managing the lifecycle of the counter across multiple rules.
 */
struct ppe_pm_counter_ref {
	struct ppe_drv_pm_counter_ctx *pm_ctx;	/* Pointer to the opaque driver context */
	struct kref ref;				/* Reference count for rules sharing this counter */
};

/*
 * ppe_pm_gen
 *	Structure for ppe pm counter gen rule.
 */
struct ppe_pm_gen {
	/*
	 * list head for active list
	 */
	struct list_head list;				/* List head for active list */

	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_pm_counter_gen_rule info;	/* Rule information extracted from user rule */

	struct ppe_pm_counter_gen rule;			/* Copy of rule information */

	/*
	 * References
	 */
	struct kref ref_cnt;				/* Reference count */

	struct ppe_drv_pm_counter_gen_ctx *ctx;		/* PM counter gen context */

	struct ppe_drv_pm_counter_info *counter_info;	/* PM counter info */

	/*
	 * basic info
	 */
	uint8_t counter_id;				/* Associated counter ID */
};

/*
 * ppe_pm_base
 *	PPE PM base structure
 */
struct ppe_pm_base {
	/*
	 * PPE PM counter local reference.
	 */
	struct ppe_pm_counter_ref counter_array[PPE_DRV_PM_COUNTER_CTX_MAX];

	spinlock_t lock;			/* PM lock */

	/*
	 * General stats
	 */
	struct ppe_pm_stats stats;		/* PM statistics */
	struct dentry *dentry;			/* Debugfs entry */

	/*
	 * Active list of PM gen rules
	 */
	struct list_head active_rules;		/* List of active PM rules */

	/*
	 * Device ID for PM gen dump character device.
	 */
	int pm_dump_major_id;			/* PM dump major device ID */
};

extern struct ppe_pm_base ppe_pm_gbl;

/*
 * PM manager APIs.
 */
void ppe_pm_deinit(void);
void ppe_pm_init(struct dentry *d_counter);
