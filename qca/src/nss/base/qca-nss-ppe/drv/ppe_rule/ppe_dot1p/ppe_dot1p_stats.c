/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include "ppe_dot1p.h"

/*
 * ppe_dot1p_stats_cmn_str
 *      PPE DOT1P common statistics
 */
static const char *ppe_dot1p_stats_cmn_str[] = {
	"dot1p_create_req",			/* DOT1P create requests. */
	"dot1p_destroy_req",			/* DOt1P destroy requests. */
	"dot1p_flush_req",			/* DOT1P flush requests. */
	"dot1p_free_req",			/* DOT1P rule free. */
	"rule_not_found",			/* DOT1P rule not found based on rule ID. */
	"dot1p_policer_rule_not_found", 	/* dot1p policer rule not found. */
	"dot1p_policer_free_req",		/* dot1p policer rule free. */
	"dot1p_policer_destroy_req",		/* dot1p destroy requests. */
	"dot1p_policer_flush_req",		/* dot1p flush requests. */
	"dot1p_policer_create_req",		/* dot1p create requests. */
	"dot1p_create_fail",			/* DOT1P create request failure. */
	"dot1p_create_fail_oom",		/* Not able to allocate rule memory. */
	"dot1p_create_fail_alloc",		/* DOT1P rule allocation failure. */
	"dot1p_create_fail_rule_config",	/* DOT1P rule configuration failure. */
	"dot1p_create_fail_rule_exist",		/* DOT1P rule create failure due to collision. */
	"dot1p_create_fail_action_config",	/* DOT1P rule create failure due to invalid action. */
	"dot1p_create_fail_invalid_id",		/* DOT1P rule create failure due to invalid rule-ID. */
	"dot1p_policer_create_fail_oom",	/* Not able to allocate policer rule memory. */
	"dot1p_policer_create_fail_alloc",	/* DOT1P policer rule allocation failure. */
	"dot1p_policer_create_fail_rule_config",	/* DOT1P policer rule configuration failure. */
	"dot1p_policer_create_fail_rule_exist",		/* DOT1P policer rule create failure due to collision. */
	"dot1p_policer_create_fail_action_config",	/* DOT1P policer rule create failure due to invalid action. */
	"dot1p_policer_create_fail_invalid_id",		/* DOT1P rule create failure due to invalid rule-ID. */
	"dot1p_destroy_fail_invalid_id",	/* DOT1P destroy failure due to invalid rule ID. */
	"dot1p_policer_destroy_fail_invalid_id"	/* DOT1P policer destroy failure due to invalid rule ID. */
};

/*
 * ppe_dot1p_stats_cmn_show()
 */
static int ppe_dot1p_stats_cmn_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	int i;

	spin_lock_bh(&dot1p_g->lock);
	atomic64_t *stats = (atomic64_t *)&dot1p_g->stats.cmn;
	spin_unlock_bh(&dot1p_g->lock);

	seq_puts(m, "\nDOT1P stats:\n\n");

	for (i = 0; i < sizeof(struct ppe_dot1p_stats) / sizeof(uint64_t); i++, stats++) {
		seq_printf(m, "\t\t [%s]: %llu\n", ppe_dot1p_stats_cmn_str[i], atomic64_read(stats));
	}

	return 0;
}

/*
 * ppe_dot1p_stats_cmn_open()
 *      PPE DOT1P common stats callback
 */
static int ppe_dot1p_stats_cmn_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_dot1p_stats_cmn_show, inode->i_private);
}

/*
 * ppe_dot1p_stats_cmn_file_ops
 *      File operations for DOT1P common stats
 */
const struct file_operations ppe_dot1p_stats_cmn_file_ops = {
	.open = ppe_dot1p_stats_cmn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_dot1p_stats_debugfs_exit()
 *      DOT1P debugfs exit api
 */
void ppe_dot1p_stats_debugfs_exit(void)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	if (dot1p_g->dentry) {
		debugfs_remove_recursive(dot1p_g->dentry);
		dot1p_g->dentry = NULL;
	}
}

/*
 * ppe_dot1p_stats_debugfs_init()
 *      Create DOT1P statistics debugfs entries.
 */
int ppe_dot1p_stats_debugfs_init(struct dentry *root)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	if (!debugfs_create_file("cmn_stats", S_IRUGO, dot1p_g->dentry,
				NULL, &ppe_dot1p_stats_cmn_file_ops)) {
		ppe_dot1p_warn("%p: Unable to create cmn stats file in debugfs\n", dot1p_g);
		debugfs_remove_recursive(dot1p_g->dentry);
		dot1p_g->dentry = NULL;
		return -1;
	}

	return 0;
}

