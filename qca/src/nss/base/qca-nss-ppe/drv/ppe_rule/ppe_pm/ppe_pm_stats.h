/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_PM_STATS_H
#define __PPE_PM_STATS_H

#define PPE_PM_STATS_NODE_NAME         "PPE_PM"

/*
 * ppe_pm_stats_cmn
 *	Structure for PM common stats
 */
struct ppe_pm_stats_cmn {
	atomic64_t pm_create_req;			/* PM create requests. */
	atomic64_t pm_destroy_req;			/* PM destroy requests. */
	atomic64_t pm_free_req;				/* PM rule free. */
	atomic64_t pm_ctx_get_req;			/* PM context get requests. */
	atomic64_t pm_ctx_destroy_req;			/* PM context destroy requests. */
	atomic64_t pm_get_req;				/* PM counter get request. */
	atomic64_t pm_counter_invalid_id;		/* PM counter invalid id. */

	/*
	 * Get failures.
	 */
	atomic64_t pm_get_ctx_fail_alloc;		/* PM counter context get failure due to alloc. */
	atomic64_t pm_get_fail_invalid_ctx;		/* PM counter get failure due to invalid context. */

	/*
	 * Create failures.
	 */
	atomic64_t pm_create_fail;			/* PM create request failure. */
	atomic64_t pm_create_fail_oom;			/* Not able to allocate rule memory. */
	atomic64_t pm_create_fail_alloc;		/* PM rule allocation failure. */
	atomic64_t pm_create_fail_rule_config;		/* PM rule configuration failure. */
	atomic64_t pm_create_fail_pm_ctx_alloc;		/* PM rule create failure due to rule. */
	atomic64_t pm_create_fail_rule_exist;		/* PM rule create failure due to collision. */

	/*
	 * Destroy failures.
	 */
	atomic64_t pm_destroy_fail_no_rule_found;	/* PM destroy failure due to no rule found. */

};

/*
 * ppe_pm_stats
 *      Structure for PM statistics
 */
struct ppe_pm_stats {
	struct ppe_pm_stats_cmn cmn;           /* Common PM stats */
};

/*
 * ppe_pm_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_pm_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_pm_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_pm_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_pm_stats_debugfs_init(struct dentry *dentry);
void ppe_pm_stats_debugfs_exit(void);

#endif
