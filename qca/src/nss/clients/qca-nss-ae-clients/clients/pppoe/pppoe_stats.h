/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * pppoe_stats.h
 *	PPPoE stats definitions
 */

#ifndef _PPPOE_STATS_H_
#define _PPPOE_STATS_H_

#include <linux/seq_file.h>

/*
 * pppoe_stats
 *      PPPoE statistics.
 */
struct pppoe_stats {
	atomic64_t pppoe_iface_alloc_failure;			/* Iface allocation failure */
	atomic64_t pppoe_iface_mtu_set_failure;			/* Iface mtu set failure */
	atomic64_t pppoe_invalid_lag_group_id;			/* Invalid LAG group ID */
	atomic64_t pppoe_add_session_failure;			/* PPPoE session add failure */
	atomic64_t pppoe_get_session_failure;                   /* PPPoE session get failure */
	atomic64_t pppoe_get_session_multilink_ppp;		/* Multilink PPP failure */
	atomic64_t pppoe_get_session_hold_channel_failed;	/* Hold channel failures */
	atomic64_t pppoe_get_session_proto_get_failed;		/* Protocol get failures */
	atomic64_t pppoe_get_session_addres_get_failed;		/* PPPoE channel addressing info failures */
	atomic64_t pppoe_session_alloc_failure;			/* PPPoE session allocation failure */
	atomic64_t pppoe_session_not_found;			/* PPPoE session not found */
	atomic64_t pppoe_session_deinit_failure;		/* PPPoE session deinit failure */
	atomic64_t pppoe_session_init_failure;                  /* PPPoE session init failure */

	/*
	 * Successful Events.
	 */
	atomic64_t pppoe_session_remove_success;		/* Successful session removals */
	atomic64_t pppoe_session_add_success;			/* Successful session addition */
	atomic64_t pppoe_disconnect_event_success;		/* Successful pppoe disconnect event */
	atomic64_t pppoe_connect_event_success;			/* Successful pppoe connect event */
};

/*
 * pppoe_stats_ctx
 *      Global pppoe stats context.
 */
struct pppoe_stats_ctx {
	struct pppoe_stats stats;		/* PPPoE statistics */
	struct dentry *dentry;			/* Root dentry for GRE client */
};

struct pppoe_stat_entry {
	const char *name;
	atomic64_t *counter;
};

extern struct pppoe_stats_ctx ctx_gbl ;

/*
 * pppoe_stats_inc()
 *	Increment stats counter.
 */
static inline void pppoe_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

bool pppoe_stats_init(struct pppoe_stats_ctx *ctx);
void pppoe_stats_deinit(struct pppoe_stats_ctx *ctx);

#endif /* _PPPOE_STATS_H_ */

