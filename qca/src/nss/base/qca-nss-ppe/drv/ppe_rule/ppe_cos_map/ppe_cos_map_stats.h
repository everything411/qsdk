/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_COS_MAP_STATS_H
#define __PPE_COS_MAP_STATS_H

#define PPE_COS_MAP_STATS_NODE_NAME		"PPE_COS_MAP"

/*
 * ppe_cos_map_stats
 *	Message structure for ppe general stats
 */
struct ppe_cos_map_stats {
	atomic64_t set_cos_map_port_group_success;	/* CoS map port group set success */
	atomic64_t set_cos_map_port_group_failed;	/* CoS map port group set fail */
	atomic64_t cos_map_rule_not_found_destroy_fail;	/* CoS map rule not found, destrot fail */
	atomic64_t cos_map_rule_already_exists;		/* CoS map rule ID already exists */
	atomic64_t create_cos_map_rule_success;		/* CoS map rule create success */
	atomic64_t create_cos_map_rule_failed;		/* CoS map rule create fail */
	atomic64_t destroy_cos_map_rule_success;	/* CoS map destroy rule success */
	atomic64_t destroy_cos_map_rule_failed;		/* CoS map destroy rule fail */
	atomic64_t cos_map_rules_flush_req;		/* CoS map flush rule success */
	atomic64_t cos_map_rule_invalid_dscp_val;	/* Cos map inavlid DSCP value */
	atomic64_t cos_map_rule_invalid_pcp_dei_val;	/* Cos map invalid PCP or DEI value */
	atomic64_t cos_map_rule_mem_alloc_failed;	/* Cos map rule memory allocation failed */
};

/*
 * ppe_cos_map_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_cos_map_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_cos_map_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_cos_map_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_cos_map_stats_debugfs_init(struct dentry *dentry);
void ppe_cos_map_stats_debugfs_exit(void);

#endif
