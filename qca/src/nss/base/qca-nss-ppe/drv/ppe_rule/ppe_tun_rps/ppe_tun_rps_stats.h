/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_TUN_RPS_STATS_H
#define __PPE_TUN_RPS_STATS_H

#define PPE_TUN_RPS_STATS_NODE_NAME		"PPE_TUN_RPS"

/*
 * ppe_tun_rps_stats
 *	Message structure for ppe general stats
 */
struct ppe_tun_rps_stats {
	atomic64_t v4_create_ppe_tun_rps_rule;		/* v4 create tun RPS rule request */
	atomic64_t v4_create_ppe_tun_rps_rule_fail;	/* v4 create tun RPS ppe rule addition failed */
	atomic64_t v4_destroy_ppe_tun_rps_rule;		/* v4 destroy tun RPS rule request */
	atomic64_t v4_destroy_ppe_tun_rps_rule_fail;	/* v4 destroy tun RPS ppe rule addition failed */
	atomic64_t v6_create_ppe_tun_rps_rule;		/* v6 create tun RPS rule request */
	atomic64_t v6_create_ppe_tun_rps_rule_fail;	/* v6 create tun RPS ppe rule addition failed */
	atomic64_t v6_destroy_ppe_tun_rps_rule;		/* v6 destroy tun RPS rule request */
	atomic64_t v6_destroy_ppe_tun_rps_rule_fail;	/* v6 destroy tun RPS ppe rule addition failed */
};

/*
 * ppe_tun_rps_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_tun_rps_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_tun_rps_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_tun_rps_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_tun_rps_stats_debugfs_init(struct dentry *dentry);
void ppe_tun_rps_stats_debugfs_exit(void);
#endif
