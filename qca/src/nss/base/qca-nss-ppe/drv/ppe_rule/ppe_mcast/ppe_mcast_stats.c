/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <ppe_drv_public.h>
#include "ppe_mcast.h"

/*
 * ppe_mcast_stats_str
 *	PPE qos statistics
 */
static const char *ppe_mcast_stats_str[] = {
	"mcast_create_entry_fail",	/* Multicast create entry fail */
	"mcast_create_entry_success",	/* Multicast create entry success */
	"mcast_delete_entry_fail",	/* Multicast delete entry fail */
	"mcast_delete_entry_success",	/* Multicast create entry success */
};

/*
 * ppe_mcast_stats_show()
 *	Read ppe multicast connection statistics
 */
static int ppe_mcast_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	int i;

	atomic64_t *stats = (atomic64_t *)&g_mcast->stats;

	seq_puts(m, "\nPPE Multicast stats:\n\n");
	for (i = 0; i < sizeof(struct ppe_mcast_stats) / sizeof(uint64_t); i++, stats++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_mcast_stats_str[i], atomic64_read(stats));
	}

	return 0;
}

/*
 * ppe_mcast_stats_general_open()
 *	PPE qos gen open callback API
 */
static int ppe_mcast_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_mcast_stats_show, inode->i_private);
}

/*
 * ppe_mcast_stats_general_file_ops
 *	File operations for EDMA common stats
 */
const struct file_operations ppe_mcast_stats_file_ops = {
	.open = ppe_mcast_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_mcast_stats_debugfs_init()
 *	Create PPE statistics debug entry.
 */
int ppe_mcast_stats_debugfs_init(struct dentry *root)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	g_mcast->dentry = debugfs_create_dir("ppe-mcast", root);
	if (!g_mcast->dentry) {
		ppe_mcast_warn("%p: Unable to create debugfs stats directory in debugfs\n", g_mcast);
		return -1;
	}

	if (!debugfs_create_file("stats", S_IRUGO, g_mcast->dentry,
			NULL, &ppe_mcast_stats_file_ops)) {
		ppe_mcast_warn("%p: Unable to create common statistics file entry in debugfs\n", g_mcast);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(g_mcast->dentry);
	g_mcast->dentry = NULL;
	return -1;
}

/*
 * ppe_mcast_stats_debugfs_exit()
 *	PPE debugfs exit api
 */
void ppe_mcast_stats_debugfs_exit(void)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	if (g_mcast->dentry) {
		debugfs_remove_recursive(g_mcast->dentry);
		g_mcast->dentry = NULL;
	}
}
