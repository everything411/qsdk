/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_policer_stats.h"
#include "ppe_policer_dump.h"
#include <ppe_drv_public.h>
#include <ppe_policer.h>
#include <linux/version.h>

#ifdef NSS_PPE_IPQ53XX
#define PPE_ACL_POLICER_FLOW_RULE_MAX 128
#else
#define PPE_ACL_POLICER_FLOW_RULE_MAX 512
#endif
#define PPE_POLICER_PORT_RULE_MAX 8

/*
 * PPE RFS macros
 */
#if (PPE_POLICER_DEBUG_LEVEL == 3)
#define ppe_policer_assert(c, s, ...)
#else
#define ppe_policer_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_policer_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_policer_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_policer_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_POLICER_DEBUG_LEVEL < 2)
#define ppe_policer_warn(s, ...)
#else
#define ppe_policer_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_POLICER_DEBUG_LEVEL < 3)
#define ppe_policer_info(s, ...)
#else
#define ppe_policer_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_POLICER_DEBUG_LEVEL < 4)
#define ppe_policer_trace(s, ...)
#else
#define ppe_policer_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

struct ppe_drv_policer_acl;
struct ppe_drv_policer_port;

/*
 * struct ppe_policer
 *	Policer structure
 */
struct ppe_policer {
	bool userspace_rule;				/* Flag indicating userspace rule */
	struct list_head list;				/* List of active Policer rules */
	struct kref kref_cnt;				/* Reference count */
	struct net_device *dev;				/* Device associated with port policer */
	uint32_t rule_id;				/* Associated Policer rule id for ACL/FLOW policer */
	uint32_t acl_rule_id;				/* ACL rule id for Policer + FLOW case */
	bool is_flow_policer;				/* Flag for flow policer */
	union {
		struct ppe_drv_policer_acl *acl_ctx;     	  	/* PPE driver context */
		struct ppe_drv_policer_port *port_ctx;     	  	/* PPE driver context */
	} drv_ctx;

	uint32_t num_dir;		/* number of directions configured */
	struct ppe_policer_create_info policer_info;		/* policer create information */
	struct ppe_policer_create_info other_dir_policer_info;		/* policer create information */
};

/*
 * ppe_policer
 *	Global ppe policer global instance
 */
struct ppe_policer_base {
	spinlock_t lock;                                	/* PPE lock */
	struct ppe_policer_stats stats;                     	/* PPE RFS statistics */
	struct dentry *dentry;					/* Debugfs entry */

	struct kref ref;					/* Reference count */

	struct list_head port_active_rules;			/* List of active Policer rules */
	struct list_head acl_active_rules;			/* List of active Policer rules */

	int policer_dump_major_id;		/* Device ID for Policer dump */
};

int ppe_policer_id_to_hwidx(uint16_t policer_id);
void ppe_policer_deinit(void);
void ppe_policer_init(struct dentry *dentry);

extern struct ppe_policer_base gbl_ppe_policer;
