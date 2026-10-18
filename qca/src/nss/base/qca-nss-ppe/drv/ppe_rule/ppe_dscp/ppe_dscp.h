/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_RULE_DSCP_H_
#define _PPE_RULE_DSCP_H_

#include <linux/module.h>
#include <linux/version.h>
#include <ppe_drv_public.h>
#include <ppe_dscp.h>
#include <ppe_drv_dscp.h>
#include "ppe_drv.h"

/*
 * PPE DSCP debug macros
 */
#if (PPE_DSCP_DEBUG_LEVEL == 3)
#define ppe_dscp_assert(c, s, ...)
#else
#define ppe_dscp_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_dscp_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_dscp_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_dscp_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_DSCP_DEBUG_LEVEL < 2)
#define ppe_dscp_warn(s, ...)
#else
#define ppe_dscp_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DSCP_DEBUG_LEVEL < 3)
#define ppe_dscp_info(s, ...)
#else
#define ppe_dscp_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_DSCP_DEBUG_LEVEL < 4)
#define ppe_dscp_trace(s, ...)
#else
#define ppe_dscp_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

#define PPE_DSCP_SHIFT			2	/* DSCP occupies bits 7..2 */
#define PPE_DSCP_MAX_VALUE		63	/* DSCP maximum value is 63 */
#define PPE_DSCP_MASK			0xFC	/* Mask for DSCP value from tos value */
#define PPE_DCP_PCP_MAX_VALUE		7	/* PCP maximum value is 7 */

/*
 * ECN (bits 1..0)
 */
#define PPE_DSCP_ECN_NON		0x0	/* Not-ECT */
#define PPE_DSCP_ECN_ECT1		0x1	/* ECT(1) */
#define PPE_DSCP_ECN_ECT0		0x2	/* ECT(0) */
#define PPE_DSCP_ECN_CE			0x3	/* Congestion Experienced */

/*
 * ppe_dscp_db_entry
 *	DSCP database entry structure
 */
struct ppe_dscp_db_entry {
	struct list_head list;		/* List node */
	uint8_t tos;			/* TOS value (DSCP + ECN) */
	uint8_t pcp0_upstream;		/* PCP0 value for upstream */
	uint8_t pcp1_upstream;		/* PCP1 value for upstream */
	uint8_t pcp0_downstream;	/* PCP0 value for downstream */
	uint8_t pcp1_downstream;	/* PCP1 value for downstream */
	bool valid;			/* Entry is valid/configured */
};

/*
 * ppe_dscp
 *	Structure for dscp to p bit table rule.
 */
struct ppe_dscp {
	/*
	 * Rule information extracted from user rule.
	 */
	struct ppe_drv_dscp_pcp_rule info;
};

/*
 * ppe_dscp_base
 *	PPE dscp to p bit table base structure
 */
struct ppe_dscp_base {
	struct list_head dscp_map;		/* DSCP database list */
	spinlock_t lock;			/* DSCP lock */
	struct dentry *dentry;			/* Debugfs dentry */
	int dscp_dump_major_id;			/* DSCP dump major ID */
};

extern struct ppe_dscp_base ppe_dscp_gbl;

/*
 * DSCP manager APIs.
 */
void ppe_dscp_deinit(void);
void ppe_dscp_init(struct dentry *d_rule);

#endif /* _PPE_RULE_DSCP_H_ */
