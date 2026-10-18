/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_VLAN_STATS_H
#define __PPE_VLAN_STATS_H

#define PPE_VLAN_STATS_NODE_NAME         "PPE_VLAN"

/*
 * ppe_vlan_stats_cmn
 *	Structure for VLAN common stats
 */
struct ppe_vlan_stats_cmn {
	atomic64_t vlan_create_req;			/* VLAN create requests. */
	atomic64_t vlan_destroy_req;			/* VLAN destroy requests. */
	atomic64_t vlan_flush_req;			/* VLAN flush requests. */
	atomic64_t vlan_free_req;			/* VLAN rule free. */
	atomic64_t rule_not_found;			/* VLAN rule not found. */

	/*
	 * Create failures.
	 */
	atomic64_t vlan_create_fail;			/* VLAN create request failure. */
	atomic64_t vlan_create_fail_oom;		/* Not able to allocate rule memory. */
	atomic64_t vlan_create_fail_alloc;		/* VLAN rule allocation failure. */
	atomic64_t vlan_create_fail_rule_config;	/* VLAN rule configuration failure. */
	atomic64_t vlan_create_fail_pm_ctx_alloc;	/* VLAN	rule create failure due to rule. */
	atomic64_t vlan_create_fail_rule_exist;		/* VLAN rule create failure due to collision. */
	atomic64_t vlan_create_fail_action_config;	/* VLAN rule create failure due to invalid action. */
	atomic64_t vlan_create_fail_invalid_id;		/* VLAN rule create failure due to invalid rule-ID. */
	atomic64_t vlan_create_fail_counter_mode;	/* VLAN rule create failure due to invalid counter_mode. */

	/*
	 * Destroy failures.
	 */
	atomic64_t vlan_destroy_fail_invalid_id;	/* VLAN destroy failure due to invalid rule ID. */
};

/*
 * ppe_vlan_stats
 *	Structure for VLAN statistics
 */
struct ppe_vlan_stats {
	struct ppe_vlan_stats_cmn cmn;           /* Common ACL stats */
};

/*
 * ppe_vlan_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_vlan_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_vlan_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_vlan_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_vlan_stats_debugfs_init(struct dentry *dentry);
void ppe_vlan_stats_debugfs_exit(void);

#endif
