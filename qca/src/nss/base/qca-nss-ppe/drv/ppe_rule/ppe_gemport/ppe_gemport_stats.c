/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include "ppe_gemport.h"

/*
 * ppe_gem_port_stats_cmn_str
 *      PPE GEMPORT common statistics
 */
static const char *ppe_gem_port_stats_cmn_str[] = {
	"gemport_create_req",			/* GEMPORT create requests. */
	"gemport_destroy_req",			/* GEMPORT destroy requests. */
	"gemport_flush_req",			/* GEMPORT flush requests. */
	"gemport_free_req",			/* GEMPORT rule free. */
	"rule_not_found",			/* GEMPORT rule not found based on rule ID. */
	"gemport_create_fail",			/* GEMPORT create request failure. */
	"gemport_create_fail_oom",		/* Not able to allocate rule memory. */
	"gemport_create_fail_alloc",		/* GEMPORT rule allocation failure. */
	"gemport_create_fail_rule_config",	/* GEMPORT rule configuration failure. */
	"gemport_create_fail_rule_exist",	/* GEMPORT rule create failure due to collision. */
	"gemport_create_fail_action_config",	/* GEMPORT rule create failure due to invalid action. */
	"gemport_create_fail_invalid_id",	/* GEMPORT rule create failure due to invalid rule-ID. */
	"gemport_destroy_fail_invalid_id",	/* GEMPORT destroy failure due to invalid rule ID. */
};

/*
 * ppe_gemport_stats_cmn_show()
 */
static int ppe_gem_port_stats_cmn_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	int i;

	spin_lock_bh(&gem_port_g->lock);
	atomic64_t *stats = (atomic64_t *)&gem_port_g->stats.cmn;
	spin_unlock_bh(&gem_port_g->lock);

	seq_puts(m, "\nGEMPORT stats:\n\n");

	for (i = 0; i < sizeof(struct ppe_gem_port_stats) / sizeof(uint64_t); i++, stats++) {
		seq_printf(m, "\t\t [%s]: %llu\n", ppe_gem_port_stats_cmn_str[i], atomic64_read(stats));
	}

	return 0;
}

/*
 * ppe_gem_port_stats_cmn_open()
 *      PPE GEMPORT common stats callback
 */
static int ppe_gem_port_stats_cmn_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_gem_port_stats_cmn_show, inode->i_private);
}

/*
 * ppe_gem_port_stats_cmn_file_ops
 *      File operations for GEMPORT common stats
 */
const struct file_operations ppe_gem_port_stats_cmn_file_ops = {
	.open = ppe_gem_port_stats_cmn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_gem_port_stats_debugfs_exit()
 *      GEMPORT debugfs exit api
 */
void ppe_gem_port_stats_debugfs_exit(void)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	if (gem_port_g->dentry) {
		debugfs_remove_recursive(gem_port_g->dentry);
		gem_port_g->dentry = NULL;
	}
}

/*
 * ppe_gem_port_stats_debugfs_init()
 *      Create GEMPORT statistics debugfs entries.
 */
int ppe_gem_port_stats_debugfs_init(struct dentry *root)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	if (!debugfs_create_file("cmn_stats", S_IRUGO, gem_port_g->dentry,
				NULL, &ppe_gem_port_stats_cmn_file_ops)) {
		ppe_gem_port_warn("%p: Unable to create cmn stats file in debugfs\n", gem_port_g);
		debugfs_remove_recursive(gem_port_g->dentry);
		gem_port_g->dentry = NULL;
		return -1;
	}

	return 0;
}

