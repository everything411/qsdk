/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_cos_map_stats.h"
#include "ppe_cos_map_dump.h"

/*
 * PPE CoS map macros
 */
#if (PPE_COS_MAP_DEBUG_LEVEL == 3)
#define ppe_cos_map_assert(c, s, ...)
#else
#define ppe_cos_map_assert(c, s, ...) if (!(c)) { printk(KERN_CRIT "%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG_ON(!(c)); }
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define ppe_cos_map_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_cos_map_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ppe_cos_map_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (PPE_COS_MAP_DEBUG_LEVEL < 2)
#define ppe_cos_map_warn(s, ...)
#else
#define ppe_cos_map_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_COS_MAP_DEBUG_LEVEL < 3)
#define ppe_cos_map_info(s, ...)
#else
#define ppe_cos_map_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (PPE_COS_MAP_DEBUG_LEVEL < 4)
#define ppe_cos_map_trace(s, ...)
#else
#define ppe_cos_map_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

#define PPE_COS_MAP_MAX_GROUP 2

/*
 * ppe_cos_map_rule
 *	Structure for CoS map rule.
 */
struct ppe_cos_map_rule {
	struct list_head list;		/* List of active CoS map rules */
	uint32_t rule_id;		/* Associated CoS map rule id for ACL/FLOW CoS map */
	uint32_t type_flag;		/* CoS rule type flag */
	uint8_t dscp_val;		/* DSCP value */
	uint8_t pcp_val;		/* PCP value */
	uint8_t dei_val;		/* DEI value */
	struct ppe_drv_cos_map_cfg cfg;	/* CoS map configuration */
};

/*
 * ppe_cos_map_port_group
 *	Structure for CoS port group.
 */
struct ppe_cos_map_port_group {
	struct list_head list;		/* List of ports */
	char dev[IFNAMSIZ];		/* Device name */
	uint32_t port_id;		/* Port ID */
	struct ppe_drv_cos_map_port_group_cfg cfg;	/* Port group info */
};

/*
 * ppe_cos_map
 *	Global ppe CoS map global instance
 */
struct ppe_cos_map_base {
	spinlock_t lock;                                	/* PPE lock */
	struct ppe_cos_map_stats stats;			/* CoS map statistics */
	struct dentry *dentry;				/* Debugfs entry */
	struct list_head port_list;			/* List of ports for port group maping */
	struct list_head active_rules;			/* List of active CoS map rules */
	int cos_map_dump_major_id;	/* Device ID for CoS map dump character device. */
};

extern struct ppe_cos_map_base gbl_ppe_cos_map;

void ppe_cos_map_deinit(void);
void ppe_cos_map_init(struct dentry *dentry);
