/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/inetdevice.h>
#include <net/ip.h>
#include <linux/if_pppox.h>
#include "pppoe_stats.h"
#include "pppoe_mgr.h"


#define NUM_FAILURE_STATS 13
#define NUM_SUCCESS_STATS 4

/*
 * pppoe_stats_fill()
 * 	Fill PPPoE generic statistics.
 */
static void pppoe_stats_fill(struct pppoe_stats_ctx *ctx, struct pppoe_stat_entry *failure_stats,
		struct pppoe_stat_entry *success_stats)
{
	/*
	 * Failure stats
	 */
	failure_stats[0]  = (struct pppoe_stat_entry){ "PPPoE interface allocation failures",
		&ctx->stats.pppoe_iface_alloc_failure };
	failure_stats[1]  = (struct pppoe_stat_entry){ "PPPoE interface MTU set failures",
		&ctx->stats.pppoe_iface_mtu_set_failure };
	failure_stats[2]  = (struct pppoe_stat_entry){ "PPPoE invalid lag group id failures",
		&ctx->stats.pppoe_invalid_lag_group_id };
	failure_stats[3]  = (struct pppoe_stat_entry){ "PPPoE session add failures",
		&ctx->stats.pppoe_add_session_failure };
	failure_stats[4]  = (struct pppoe_stat_entry){ "PPPoE session get failures",
		&ctx->stats.pppoe_get_session_failure };
	failure_stats[5]  = (struct pppoe_stat_entry){ "PPPoE multilink PPP failures",
		&ctx->stats.pppoe_get_session_multilink_ppp };
	failure_stats[6]  = (struct pppoe_stat_entry){ "PPPoE hold channel failures",
		&ctx->stats.pppoe_get_session_hold_channel_failed };
	failure_stats[7]  = (struct pppoe_stat_entry){ "PPPoE protocol get failures",
		&ctx->stats.pppoe_get_session_proto_get_failed };
	failure_stats[8]  = (struct pppoe_stat_entry){ "PPPoE channel addressing info failures",
		&ctx->stats.pppoe_get_session_addres_get_failed };
	failure_stats[9]  = (struct pppoe_stat_entry){ "PPPoE session allocation failures",
		&ctx->stats.pppoe_session_alloc_failure };
	failure_stats[10] = (struct pppoe_stat_entry){ "PPPoE session not found",
		&ctx->stats.pppoe_session_not_found };
	failure_stats[11] = (struct pppoe_stat_entry){ "PPPoE session deinitialization failures",
		&ctx->stats.pppoe_session_deinit_failure };
	failure_stats[12] = (struct pppoe_stat_entry){ "PPPoE session initialization failures",
		&ctx->stats.pppoe_session_init_failure };

	/*
	 * Success stats
	 */
	success_stats[0] = (struct pppoe_stat_entry){ "PPPoE session add successes",
		&ctx->stats.pppoe_session_add_success };
	success_stats[1] = (struct pppoe_stat_entry){ "PPPoE session remove successes",
		&ctx->stats.pppoe_session_remove_success };
	success_stats[2] = (struct pppoe_stat_entry){ "PPPoE disconnect event successes",
		&ctx->stats.pppoe_disconnect_event_success };
	success_stats[3] = (struct pppoe_stat_entry){ "PPPoE connect event successes",
		&ctx->stats.pppoe_connect_event_success };
}

/*
 * pppoe_stats_show()
 *      Read PPPoE generic statistics.
 */
static int pppoe_stats_show(struct seq_file *m, void *unused)
{
	struct pppoe_stats_ctx *ctx = (struct pppoe_stats_ctx *)m->private;

	struct pppoe_stat_entry failure_stats[NUM_FAILURE_STATS];
	struct pppoe_stat_entry success_stats[NUM_SUCCESS_STATS];

	pppoe_stats_fill(ctx, failure_stats, success_stats);

	seq_puts(m, "\n################ PPPoE Client Statistics Start ################\n");

	seq_puts(m, "\n--- Failure Statistics ---\n");
	for (int i = 0; i < ARRAY_SIZE(failure_stats); i++) {
		seq_printf(m, "\t%-50s : %llu\n", failure_stats[i].name, atomic64_read(failure_stats[i].counter));
	}

	seq_puts(m, "\n--- Success Statistics ---\n");
	for (int i = 0; i < ARRAY_SIZE(success_stats); i++) {
		seq_printf(m, "\t%-50s : %llu\n", success_stats[i].name, atomic64_read(success_stats[i].counter));
	}

	seq_puts(m, "\n################ PPPoE Client Statistics End ################\n");

	return 0;
}

/*
 * pppoe_stats_open()
 *	Open handler for the PPPoE statistics debugfs file
 */
static int pppoe_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, pppoe_stats_show, inode->i_private);
}

/*
 * pppoe_stats_cmn_file_ops
 *      File operations for PPPoE stats
 */
static const struct file_operations pppoe_stats_cmn_file_ops = {
	.open = pppoe_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * pppoe_stats_deinit()
 *      Cleanup the debugfs tree.
 */
void pppoe_stats_deinit(struct pppoe_stats_ctx *ctx)
{
	debugfs_remove_recursive(ctx->dentry);
	ctx->dentry = NULL;
}

/*
 * pppoe_stats_init()
 *      Create pppoe tunnel statistics debugfs entry.
 */
bool pppoe_stats_init(struct pppoe_stats_ctx *ctx)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;
	struct dentry *stats;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		pppoe_mgr_warn("parent debugfs entry for qca-nss-ppe not present");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		pppoe_mgr_warn("clients debugfs entry inside qca-nss-ppe not present");
		return false;
	}

	ctx->dentry = debugfs_create_dir("pppoe", clients);
	if (!ctx->dentry) {
		pppoe_mgr_warn("pppoe debugfs entry inside qca-nss-ppe/clients could not be created");
		return false;
	}

	stats = debugfs_create_file("stats", S_IRUGO,
			ctx->dentry, ctx, &pppoe_stats_cmn_file_ops);
	if (!stats) {
		pppoe_mgr_warn("PPPoE stats debugfs create failed\n");
		debugfs_remove(ctx->dentry);
		return false;
	}

	return true;
}
