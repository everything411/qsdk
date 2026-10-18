/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_gemport.h>
#include "ppe_gemport_dump.h"
#include "ppe_gemport_stats.h"

/*
 * Max number of GEM PORT rules IDs.
 */
#define PPE_GEM_PORT_ID_MAX 128		/* Maximum number of gem port ID */

/*
 * PPE GEM PORT debug macros
 */
#if (PPE_GEM_PORT_DEBUG_LEVEL == 3)
#define ppe_gem_port_assert(c, s, ...)
#else
#define ppe_gem_port_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_gem_port_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_gem_port_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_gem_port_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_GEM_PORT_DEBUG_LEVEL < 2)
#define ppe_gem_port_warn(s, ...)
#else
#define ppe_gem_port_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_GEM_PORT_DEBUG_LEVEL < 3)
#define ppe_gem_port_info(s, ...)
#else
#define ppe_gem_port_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_GEM_PORT_DEBUG_LEVEL < 4)
#define ppe_gem_port_trace(s, ...)
#else
#define ppe_gem_port_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * ppe_gem_port
 *	Structure for gem port rule.
 */
struct ppe_gem_port {
	struct list_head list;			/* List head for active list */

	/*
	 * basic info
	 */
	uint32_t rule_valid_flag;		/* Rule valid flags */
	struct ppe_drv_gem_port_ctx *ctx;	/* PPE driver rule context */

	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_gem_port_rule info;

	struct kref ref_cnt;			/* Reference count */

	struct ppe_gem_port_rule rule;		/* Copy of rule information. */
};

/*
 * ppe_gem_port_base
 *	PPE GEM PORT base structure
 */
struct ppe_gem_port_base {
	spinlock_t lock;				/* GEM PORT lock */

	struct dentry *dentry;				/* Debugfs entry */

	struct kref ref;				/* Reference count */
	struct ppe_gem_port_stats stats;                /* gemport statistics */

	struct list_head active_rules;			/* List of active GEM PORT rules */

	/*
	 * Device ID for GEM PORT dump character device.
	 */
	int gem_port_dump_major_id;
};

extern struct ppe_gem_port_base ppe_gem_port_gbl;

/*
 * GEM PORT manager APIs.
 */
void ppe_gem_port_deinit(void);
void ppe_gem_port_init(struct dentry *d_rule);
