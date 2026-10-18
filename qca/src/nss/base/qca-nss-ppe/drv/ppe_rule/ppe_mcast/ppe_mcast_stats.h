/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_MCAST_STATS_H
#define __PPE_MCAST_STATS_H

#define PPE_MCAST_STATS_NODE_NAME		"PPE_MCAST"

/*
 * ppe_mcast_stats
 *	Message structure for ppe general stats
 */
struct ppe_mcast_stats {
	atomic64_t mcast_create_entry_fail;		/* Multicast create entry fail */
	atomic64_t mcast_create_entry_success;		/* Multicast create entry success */
	atomic64_t mcast_delete_entry_fail;		/* Multicast delete entry fail */
	atomic64_t mcast_delete_entry_success;		/* Multicast create entry success */
};

/*
 * ppe_mcast_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_mcast_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_mcast_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_mcast_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_mcast_stats_debugfs_init(struct dentry *dentry);
void ppe_mcast_stats_debugfs_exit(void);

#endif
