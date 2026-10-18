/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/inetdevice.h>
#include <net/ip.h>
#include "nss_ppe_lag_private.h"

extern struct nss_ppe_lag_ctx gbl;

/*
 * nss_ppe_lag_stats_show()
 *	Read PPE LAG statistics
 */
static int nss_ppe_lag_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
    struct nss_ppe_lag_ctx *ctx = &gbl;

	seq_puts(m, "\nPPE LAG stats:\n\n");
	seq_printf(m, "\tPPE LAG no upper dev failure: %llu\n", atomic64_read(&ctx->stats.ppe_lag_no_upper_dev));
	seq_printf(m, "\tPPE LAG no bond slave: %llu\n", atomic64_read(&ctx->stats.ppe_lag_no_bond_slave));
	seq_printf(m, "\tPPE LAG no bond master: %llu\n", atomic64_read(&ctx->stats.ppe_lag_no_bond_master));
	seq_printf(m, "\tPPE LAG unknown slave dev: %llu\n", atomic64_read(&ctx->stats.ppe_lag_unknown_slave_dev));
	seq_printf(m, "\tPPE LAG invalid lag group ID: %llu\n", atomic64_read(&ctx->stats.ppe_lag_invalid_lag_group_id));
	seq_printf(m, "\tPPE LAG slave full: %llu\n", atomic64_read(&ctx->stats.ppe_lag_slave_full));
	seq_printf(m, "\tPPE LAG invalid PPE interface: %llu\n", atomic64_read(&ctx->stats.ppe_lag_invalid_ppe_interface));
	seq_printf(m, "\tPPE LAG slave join fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_slave_join_fail));
	seq_printf(m, "\tPPE LAG session init fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_init_fail));
	seq_printf(m, "\tPPE LAG session deinit fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_deinit_fail));
	seq_printf(m, "\tPPE LAG VLAN add fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_vlan_add_fail));
	seq_printf(m, "\tPPE LAG VLAN delete fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_vlan_del_fail));
	seq_printf(m, "\tPPE LAG bond interface destroyed: %llu\n", atomic64_read(&ctx->stats.ppe_lag_bond_interface_destoyed));
	seq_printf(m, "\tPPE LAG bond interface exists: %llu\n", atomic64_read(&ctx->stats.ppe_lag_bond_interface_exist));
	seq_printf(m, "\tPPE LAG bond ID invalid: %llu\n", atomic64_read(&ctx->stats.ppe_lag_bond_id_invalid));
	seq_printf(m, "\tPPE LAG bond ID full: %llu\n", atomic64_read(&ctx->stats.ppe_lag_bond_id_full));
	seq_printf(m, "\tPPE LAG interface alloc fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_iface_alloc_fail));
	seq_printf(m, "\tPPE LAG MAC set fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_mac_set_fail));
	seq_printf(m, "\tPPE LAG MAC clear fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_mac_clear_fail));
	seq_printf(m, "\tPPE LAG MTU set fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_mtu_set_fail));
	seq_printf(m, "\tPPE LAG unregister event success: %llu\n", atomic64_read(&ctx->stats.ppe_lag_unregister_event_success));
	seq_printf(m, "\tPPE LAG register event success: %llu\n", atomic64_read(&ctx->stats.ppe_lag_register_event_success));
	seq_printf(m, "\tPPE LAG change MTU event success: %llu\n", atomic64_read(&ctx->stats.ppe_lag_change_mtu_event_success));
	seq_printf(m, "\tPPE LAG change address event success: %llu\n", atomic64_read(&ctx->stats.ppe_lag_change_addr_event_success));
	seq_printf(m, "\tPPE LAG event fail: %llu\n", atomic64_read(&ctx->stats.ppe_lag_event_fail));

	return 0;
}

/*
 * nss_ppe_lag_stats_open()
 */
static int nss_ppe_lag_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_lag_stats_show, inode->i_private);
}

/*
 * nss_ppe_lag_stats_ops
 *	File operations for PPE LAG stats
 */
static const struct file_operations nss_ppe_lag_stats_ops = {
	.open = nss_ppe_lag_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_ppe_lag_stats_deinit()
 *	Cleanup the debugfs tree.
 */
void nss_ppe_lag_stats_deinit(struct nss_ppe_lag_ctx *ctx)
{
	if (ctx->dentry) {
		debugfs_remove_recursive(ctx->dentry);
		ctx->dentry = NULL;
	}
}

/*
 * nss_ppe_lag_stats_init()
 *	Create lag statistics debugfs entry.
 */
bool nss_ppe_lag_stats_init(struct nss_ppe_lag_ctx *ctx)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;
	struct dentry *dentry;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		nss_ppe_lag_warn("parent debugfs entry for qca-nss-ppe not present\n");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		nss_ppe_lag_warn("clients debugfs entry inside qca-nss-ppe not present\n");
		return false;
	}

	ctx->dentry = debugfs_create_dir("lag", clients);
	if (!ctx->dentry) {
		nss_ppe_lag_warn("lag debugfs entry inside qca-nss-ppe/clients could not be created\n");
		return false;
	}

	dentry = debugfs_create_file("stats", S_IRUGO,
			ctx->dentry, NULL, &nss_ppe_lag_stats_ops);
	if (!dentry) {
		nss_ppe_lag_warn("Debugfs file creation failed\n");
		return false;
	}

	return true;
}