/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <ppe_drv_public.h>
#include "ppe_qos.h"

/*
 * ppe_qos_stats_str
 *	PPE qos statistics
 */
static const char *ppe_qos_stats_str[] = {
	"qos_create_interface_queues_fail",		/* Qos create interface queues fail */
	"qos_create_interface_queues_success",		/* Qos create interface queues success */
	"qos_flush_interface_queues_fail",	/* Qos flush interface queues fail */
	"qos_flush_interface_queues_success",		/* Qos flush interface queues success */
	"qos_set_interface_shaper_fail",		/* Qos det interface shaper fail */
	"qos_set_interface_shaper_success",		/* Qos set interface shaper success */
	"qos_pq_to_tcont_mapping_fail",	/* Qos priority queue mapping to Tcont fail */
	"qos_pq_to_tcont_mapping_success",	/* Qos priority queue mapping to Tcont fail */
	"qos_tcont_stats_get_fail",		/* QoS Tcont statistics fetch failed */
	"qos_tcont_stats_get_success",		/* QoS Tcont statistics fetch success */
	"qos_reset_tcont_credit_fail",	/* QoS Tcont credit reset fail */
	"qos_reset_tcont_credit_success",	/* QoS Tcont credit reset success */
	"qos_set_queue_tm_fail",	/* Qos set queue traffic management fail */
	"qos_set_queue_tm_success",		/* Qos set queue traffic management success */
	"qos_set_queue_limit_fail",			/* QoS set queue limit fail */
	"qos_set_queue_limit_success",			/* QoS set queue limit success */
	"qos_create_shaper_fail",		/* Qos create shaper fail */
	"qos_create_shaper_success",		/* Qos create shaper fail */
	"qos_delete_shaper_fail",		/* Qos delete shaper fail */
	"qos_delete_shaper_success",		/* Qos delete shaper success */
	"qos_set_interface_queue_ctrl_fail",	/* Qos set interface queue control fail */
	"qos_set_interface_queue_ctrl_success",	/* Qos set interface queue control success */
	"qos_create_mcast_queues_fail",		/* Qos create multicast queues fail */
	"qos_create_mcast_queues_success",	/* Qos create multicast queues success */
	"qos_flush_mcast_queues_fail",		/* Qos flush multicast queues fail */
	"qos_flush_mcast_queues_success",	/* Qos flush multicast queues success */
	"qos_set_mcast_queue_tm_fail",		/* Qos set multicast queue TM fail */
	"qos_set_mcast_queue_tm_success",	/* Qos set multicast queue TM success */
	"qos_set_mcast_queue_limit_fail",	/* Qos set multicast queue limit fail */
	"qos_set_mcast_queue_limit_success",	/* Qos set multicast queue limit success */
	"qos_set_ucast_prio_map_fail",		/* Unicast priority map config fail */
	"qos_set_ucast_prio_map_success",	/* Unicast priority map config success */
	"qos_set_mcast_prio_map_fail",		/* Multicast priority map config fail */
	"qos_set_mcast_prio_map_success",	/* Multicast priority map config success */
};

/*
 * ppe_qos_stats_show()
 *	Read ppe qos connection statistics
 */
static int ppe_qos_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	uint64_t *stats, *stats_shadow;
	uint32_t stats_size;
	int i;

	stats_size = sizeof(g_qos->stats);

	stats = kzalloc(stats_size, GFP_KERNEL);
	if (!stats) {
		ppe_qos_warn("Error in allocating gen stats\n");
		return -ENOMEM;
	}

	spin_lock_bh(&g_qos->lock);
	memcpy(stats, &g_qos->stats, sizeof(struct ppe_qos_stats));
	spin_unlock_bh(&g_qos->lock);

	seq_puts(m, "\nPPE stats:\n\n");
	stats_shadow = stats;
	for (i = 0; i < sizeof(struct ppe_qos_stats) / sizeof(uint64_t); i++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", ppe_qos_stats_str[i], stats_shadow[i]);
	}

	kfree(stats);
	return 0;
}

/*
 * ppe_qos_stats_general_open()
 *	PPE qos gen open callback API
 */
static int ppe_qos_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_qos_stats_show, inode->i_private);
}

/*
 * ppe_qos_stats_general_file_ops
 *	File operations for EDMA common stats
 */
const struct file_operations ppe_qos_stats_file_ops = {
	.open = ppe_qos_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_qos_stats_debugfs_init()
 *	Create PPE statistics debug entry.
 */
int ppe_qos_stats_debugfs_init(struct dentry *root)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	g_qos->dentry = debugfs_create_dir("ppe-qos", root);
	if (!g_qos->dentry) {
		ppe_qos_warn("%p: Unable to create debugfs stats directory in debugfs\n", g_qos);
		return -1;
	}

	if (!debugfs_create_file("stats", S_IRUGO, g_qos->dentry,
			NULL, &ppe_qos_stats_file_ops)) {
		ppe_qos_warn("%p: Unable to create common statistics file entry in debugfs\n", g_qos);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(g_qos->dentry);
	g_qos->dentry = NULL;
	return -1;
}

/*
 * ppe_qos_stats_debugfs_exit()
 *	PPE debugfs exit api
 */
void ppe_qos_stats_debugfs_exit(void)
{
	struct ppe_qos_base *g_qos = &gbl_ppe_qos;
	if (g_qos->dentry) {
		debugfs_remove_recursive(g_qos->dentry);
		g_qos->dentry = NULL;
	}
}
