/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_GEMPORT_STATS_H
#define __PPE_GEMPORT_STATS_H

#define PPE_GEM_PORT_STATS_NODE_NAME         "PPE_GEM_PORT"

/*
 * ppe_gem_port_stats_cmn
 *      Structure for GEMPORT common stats
 */
struct ppe_gem_port_stats_cmn {
	atomic64_t gem_port_create_req;			/* gem_port create requests. */
	atomic64_t gem_port_destroy_req;		/* gem_port destroy requests. */
	atomic64_t gem_port_flush_req;			/* gem_port flush requests. */
	atomic64_t gem_port_free_req;			/* gem_port rule free. */
	atomic64_t gem_port_rule_not_found;		/* gem_port rule not found. */

	/*
	 * Create failures.
	 */
	atomic64_t gem_port_create_fail;		/* gem_port create request failure. */
	atomic64_t gem_port_create_fail_oom;		/* Not able to allocate rule memory. */
	atomic64_t gem_port_create_fail_alloc;		/* gem_port rule allocation failure. */
	atomic64_t gem_port_create_fail_rule_config;	/* gem_port rule configuration failure. */
	atomic64_t gem_port_create_fail_rule_exist;	/* gem_port rule create failure because rule exists. */
	atomic64_t gem_port_create_fail_action_config;	/* gem_port rule create failure due to invalid action. */
	atomic64_t gem_port_create_fail_invalid_id;	/* gem_port rule create failure due to invalid rule-ID. */

	/*
	 * Destroy failures.
	 */
	atomic64_t gem_port_destroy_fail_invalid_id;	/* gem_port destroy failure due to invalid rule ID. */
};

/*
 * ppe_gem_port_stats
 *      Structure for gem_port statistics
 */
struct ppe_gem_port_stats {
	struct ppe_gem_port_stats_cmn cmn;           /* Common gem_port stats */
};

/*
 * ppe_gem_port_stats_dec()
 *      Decrement gem_port counter.
 */
static inline void ppe_gem_port_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_gem_port_stats_inc()
 *      Increment stats counter.
 */
static inline void ppe_gem_port_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_gem_port_stats_debugfs_init(struct dentry *dentry);
void ppe_gem_port_stats_debugfs_exit(void);

#endif
