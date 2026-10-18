/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <ppe_drv_public.h>
#include "ppe_cos_map.h"

/*
 * ppe_cos_map_stats_str
 *	PPE cos map statistics
 */
static const char *ppe_cos_map_stats_str[] = {
	"set_cos_map_port_group_success",	/* CoS map port group set success */
	"set_cos_map_port_group_failed",	/* CoS map port group set fail */
	"cos_map_rule_not_found_destroy_fail",	/* CoS map rule ID not valid, destrot fail */
	"cos_map_rule_already_exists",		/* CoS map rule ID already exists */
	"create_cos_map_rule_success",		/* CoS map rule create success */
	"create_cos_map_rule_failed",		/* CoS map rule create fail */
	"destroy_cos_map_rule_success",		/* CoS map destroy rule success */
	"destroy_cos_map_rule_failed",		/* CoS map destroy rule fail */
	"cos_map_rules_flush_req",		/* CoS map flush rule success */
	"cos_map_rule_invalid_dscp_val",	/* Cos map inavlid DSCP value */
	"cos_map_rule_invalid_pcp_dei_val",	/* Cos map invalid PCP or DEI value */
	"cos_map_rule_mem_alloc_failed",	/* Cos map rule memory allocation failed */
};

/*
 * ppe_cos_map_stats_show()
 *	Read ppe cos map connection statistics
 */
static int ppe_cos_map_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	int i;

	atomic64_t *stats = (atomic64_t *)&g_cos_map->stats;

	seq_puts(m, "\nPPE CoS map stats:\n\n");
	for (i = 0; i < sizeof(struct ppe_cos_map_stats) / sizeof(uint64_t); i++, stats++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_cos_map_stats_str[i], atomic64_read(stats));
	}

	return 0;
}

/*
 * ppe_cos_map_stats_general_open()
 *	PPE cos map gen open callback API
 */
static int ppe_cos_map_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_cos_map_stats_show, inode->i_private);
}

/*
 * ppe_cos_map_stats_general_file_ops
 *	File operations for EDMA common stats
 */
const struct file_operations ppe_cos_map_stats_file_ops = {
	.open = ppe_cos_map_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_cos_map_stats_debugfs_init()
 *	Create PPE statistics debug entry.
 */
int ppe_cos_map_stats_debugfs_init(struct dentry *root)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;

	g_cos_map->dentry = debugfs_create_dir("ppe-cos-map", root);
	if (!g_cos_map->dentry) {
		ppe_cos_map_warn("%p: Unable to create debugfs stats directory in debugfs\n", g_cos_map);
		return -1;
	}

	if (!debugfs_create_file("stats", S_IRUGO, g_cos_map->dentry,
			NULL, &ppe_cos_map_stats_file_ops)) {
		ppe_cos_map_warn("%p: Unable to create common statistics file entry in debugfs\n", g_cos_map);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(g_cos_map->dentry);
	g_cos_map->dentry = NULL;
	return -1;
}

/*
 * ppe_cos_map_stats_debugfs_exit()
 *	PPE debugfs exit api
 */
void ppe_cos_map_stats_debugfs_exit(void)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	if (g_cos_map->dentry) {
		debugfs_remove_recursive(g_cos_map->dentry);
		g_cos_map->dentry = NULL;
	}
}
