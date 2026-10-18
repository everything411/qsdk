/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include "ppe_vlan.h"

/*
 * ppe_vlan_stats_cmn_str
 *	PPE VLAN common statistics
 */
static const char *ppe_vlan_stats_cmn_str[] = {
	"vlan_create_req",			/* VLAN create requests. */
	"vlan_destroy_req",			/* VLAN destroy requests. */
	"vlan_flush_req",			/* VLAN flush requests. */
	"vlan_free_req",			/* VLAN rule free. */
	"rule_not_found",			/* VLAN rule not found based on rule ID. */
	"vlan_create_fail",			/* VLAN create request failure. */
	"vlan_create_fail_oom",			/* Not able to allocate rule memory. */
	"vlan_create_fail_alloc",		/* VLAN rule allocation failure. */
	"vlan_create_fail_rule_config",		/* VLAN rule configuration failure. */
	"vlan_create_fail_pm_ctx_alloc",	/* VLAN rule create failure due to PM counter context allocation. */
	"vlan_create_fail_rule_exist",		/* VLAN rule create failure due to collision. */
	"vlan_create_fail_action_config",	/* VLAN rule create failure due to invalid action. */
	"vlan_create_fail_invalid_id",		/* VLAN rule create failure due to invalid rule-ID. */
	"vlan_create_fail_counter_mode",	/* VLAN rule create failure due to invalid counter_mode. */
	"vlan_destroy_fail_invalid_id",		/* VLAN destroy failure due to invalid rule ID. */
};

/*
 * ppe_vlan_stats_cmn_show()
 *	Read ppe rfs connection statistics
 */
static int ppe_vlan_stats_cmn_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	uint64_t *stats, *stats_shadow;
	uint32_t stats_size;
	int i;

	stats_size = sizeof(struct ppe_vlan_stats_cmn);

	stats = kzalloc(stats_size, GFP_KERNEL);
	if (!stats) {
		ppe_vlan_warn("Error in allocating common stats\n");
		return -ENOMEM;
	}

	spin_lock_bh(&vlan_g->lock);
	memcpy(stats, &vlan_g->stats.cmn, sizeof(struct ppe_vlan_stats_cmn));
	spin_unlock_bh(&vlan_g->lock);

	seq_puts(m, "\nVLAN stats:\n\n");
	stats_shadow = stats;
	for (i = 0; i < sizeof(struct ppe_vlan_stats) / sizeof(uint64_t); i++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_vlan_stats_cmn_str[i], stats_shadow[i]);
	}

	kfree(stats);
	return 0;
}

/*
 * ppe_vlan_stats_cmn_open()
 *	PPE VLAN common stats callback
 */
static int ppe_vlan_stats_cmn_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_vlan_stats_cmn_show, inode->i_private);
}

/*
 * ppe_vlan_stats_cmn_file_ops
 *	File operations for VLAN common stats
 */
const struct file_operations ppe_vlan_stats_cmn_file_ops = {
	.open = ppe_vlan_stats_cmn_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_vlan_stats_debugfs_exit()
 *	VLAN debugfs exit api
 */
void ppe_vlan_stats_debugfs_exit(void)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	if (vlan_g->dentry) {
		debugfs_remove_recursive(vlan_g->dentry);
		vlan_g->dentry = NULL;
	}
}

/*
 * ppe_vlan_stats_debugfs_init()
 *	Create VLAN statistics debugfs entries.
 */
int ppe_vlan_stats_debugfs_init(struct dentry *root)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	if (!debugfs_create_file("cmn_stats", S_IRUGO, vlan_g->dentry,
				NULL, &ppe_vlan_stats_cmn_file_ops)) {
		ppe_vlan_warn("%p: Unable to create cmn stats file in debugfs\n", vlan_g);
		debugfs_remove_recursive(vlan_g->dentry);
		vlan_g->dentry = NULL;
		return -1;
	}

	return 0;
}
