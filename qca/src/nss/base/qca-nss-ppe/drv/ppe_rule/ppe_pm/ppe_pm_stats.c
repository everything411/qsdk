/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include "ppe_pm.h"

/*
 * ppe_pm_stats_cmn_str
 *	PPE PM common statistics
 */
static const char *ppe_pm_stats_cmn_str[] = {
	"pm_create_req",			/* PM create requests. */
	"pm_destroy_req",			/* PM destroy requests. */
	"pm_free_req",				/* PM rule free. */
	"pm_ctx_get_req",			/* PM context allocation request. */
	"pm_ctx_destroy_req",			/* PM context destroy request. */
	"pm_get_req",				/* PM counter get request. */
	"pm_counter_invalid_id",		/* Invalid counter ID. */
	"pm_get_ctx_fail_alloc",		/* PM counter context allocation failed. */
	"pm_get_fail_invalid_ctx",		/* PM counter get failed due to invalid context. */
	"pm_create_fail",			/* PM create request failure. */
	"pm_create_fail_oom",			/* Not able to allocate rule memory. */
	"pm_create_fail_alloc",			/* PM rule allocation failure. */
	"pm_create_fail_rule_config",		/* PM rule configuration failure. */
	"pm_create_fail_pm_ctx_alloc",		/* PM rule create failure due to PM counter context allocation. */
	"pm_create_fail_rule_exist",		/* PM rule create failure due to collision. */
	"pm_destroy_fail_no_rule_found",	/* PM destroy failure due to no rule found. */
};

/*
 * ppe_pm_stats_cmn_show()
 *	Read ppe rfs connection statistics
 */
static int ppe_pm_stats_cmn_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;
	uint64_t *stats, *stats_shadow;
	uint32_t stats_size;
	int i;

	stats_size = sizeof(struct ppe_pm_stats_cmn);

	stats = kzalloc(stats_size, GFP_KERNEL);
	if (!stats) {
		ppe_pm_warn("Error in allocating common stats\n");
		return -ENOMEM;
	}

	spin_lock_bh(&pm_g->lock);
	memcpy(stats, &pm_g->stats.cmn, sizeof(struct ppe_pm_stats_cmn));
	spin_unlock_bh(&pm_g->lock);

	seq_puts(m, "\nPM stats:\n\n");
	stats_shadow = stats;
	for (i = 0; i < sizeof(struct ppe_pm_stats) / sizeof(uint64_t); i++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_pm_stats_cmn_str[i], stats_shadow[i]);
	}

	kfree(stats);
	return 0;
}

/*
 * ppe_pm_stats_cmn_open()
 *	PPE PM common stats callback
 */
static int ppe_pm_stats_cmn_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_pm_stats_cmn_show, inode->i_private);
}

/*
 * ppe_pm_stats_cmn_file_ops
 *      File operations for PM common stats
 */
const struct file_operations ppe_pm_stats_cmn_file_ops = {
	.open = ppe_pm_stats_cmn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_pm_stats_debugfs_exit()
 *	PM debugfs exit api
 */
void ppe_pm_stats_debugfs_exit(void)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	if (pm_g->dentry) {
		debugfs_remove_recursive(pm_g->dentry);
		pm_g->dentry = NULL;
	}
}

/*
 * ppe_pm_stats_debugfs_init()
 *	Create PM statistics debugfs entries.
 */
int ppe_pm_stats_debugfs_init(struct dentry *root)
{
	struct ppe_pm_base *pm_g = &ppe_pm_gbl;

	pm_g->dentry = debugfs_create_dir("ppe-pm", root);
	if (!pm_g->dentry) {
		ppe_pm_warn("%p: Unable to create PM debugfs directory\n", pm_g);
		return -1;
	}

	if (!debugfs_create_file("cmn_stats", S_IRUGO, pm_g->dentry,
				NULL, &ppe_pm_stats_cmn_file_ops)) {
		ppe_pm_warn("%p: Unable to create cmn stats file in debugfs\n", pm_g);
		debugfs_remove_recursive(pm_g->dentry);
		pm_g->dentry = NULL;
		return -1;
	}

	return 0;
}
