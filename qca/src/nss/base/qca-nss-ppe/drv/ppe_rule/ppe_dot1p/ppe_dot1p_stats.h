/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_DOT1P_STATS_H
#define __PPE_DOT1P_STATS_H

#define PPE_DOT1P_STATS_NODE_NAME         "PPE_DOT1P"

/*
 * ppe_dot1p_stats_cmn
 *      Structure for DOT1P common stats
 */
struct ppe_dot1p_stats_cmn {
	atomic64_t dot1p_create_req;			/* dot1p create requests. */
	atomic64_t dot1p_destroy_req;			/* dot1p destroy requests. */
	atomic64_t dot1p_flush_req;			/* dot1p flush requests. */
	atomic64_t dot1p_free_req;			/* dot1p rule free. */
	atomic64_t dot1p_rule_not_found;		/* dot1p rule not found. */
	atomic64_t dot1p_policer_rule_not_found;	/* dot1p policer rule not found. */
	atomic64_t dot1p_policer_free_req;		/* dot1p policer rule free. */
	atomic64_t dot1p_policer_destroy_req;		/* dot1p destroy requests. */
	atomic64_t dot1p_policer_flush_req;		/* dot1p flush requests. */
	atomic64_t dot1p_policer_create_req;		/* dot1p create requests. */

	/*
	 * Create failures.
	 */
	atomic64_t dot1p_create_fail;			/* DOT1P create request failure. */
	atomic64_t dot1p_create_fail_oom;		/* Not able to allocate rule memory. */
	atomic64_t dot1p_create_fail_alloc;		/* DOT1P rule allocation failure. */
	atomic64_t dot1p_create_fail_rule_config;	/* DOT1P rule configuration failure. */
	atomic64_t dot1p_create_fail_rule_exist;	/* DOt1P rule create failure because rule exists. */
	atomic64_t dot1p_create_fail_action_config;	/* DOT1P rule create failure due to invalid action. */
	atomic64_t dot1p_create_fail_invalid_id;	/* DOT1P rule create failure due to invalid rule-ID. */
	atomic64_t dot1p_policer_create_fail_oom;	/* Not able to allocate policer rule memory. */
	atomic64_t dot1p_policer_create_fail_alloc;	/* DOT1P policer rule allocation failure. */
	atomic64_t dot1p_policer_create_fail_rule_config;	/* DOT1P policer rule configuration failure. */
	atomic64_t dot1p_policer_create_fail_rule_exist;	/* DOt1P policer rule create failure because rule exists. */
	atomic64_t dot1p_policer_create_fail_action_config;	/* DOT1P policer rule create failure due to invalid action. */
	atomic64_t dot1p_policer_create_fail_invalid_id;	/* DOT1P rule create failure due to invalid rule-ID. */

	/*
	 * Destroy failures.
	 */
	atomic64_t dot1p_destroy_fail_invalid_id;		/* DOT1P destroy failure due to invalid rule ID. */
	atomic64_t dot1p_policer_destroy_fail_invalid_id;	/* DOT1P policer destroy failure due to invalid rule ID. */
};

/*
 * ppe_dot1p_stats
 *      Structure for DOT1P statistics
 */
struct ppe_dot1p_stats {
	struct ppe_dot1p_stats_cmn cmn;           /* Common DOt1P stats */
};

/*
 * ppe_dot1p_stats_dec()
 *      Decrement stats counter.
 */
static inline void ppe_dot1p_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_dot1p_stats_inc()
 *      Increment stats counter.
 */
static inline void ppe_dot1p_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_dot1p_stats_debugfs_init(struct dentry *dentry);
void ppe_dot1p_stats_debugfs_exit(void);

#endif
