/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_gre_mgr.h"
#include "ppe_gre_mgr_stats.h"

/*
 * ppe_gre_mgr_stats_show()
 * 	Read ppe tunnel statistics.
 */
static int ppe_gre_mgr_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct net_device *dev = (struct net_device *)m->private;
	uint64_t exception_packet;
	uint64_t exception_bytes;

	ppe_tun_exception_packet_get(dev, &exception_packet, &exception_bytes);
	seq_printf(m, "\n################ PPE Client gre Statistics Start ################\n");
	seq_printf(m, "dev: %s\n", dev->name);
	seq_printf(m, "  Exception:\n");
	seq_printf(m, "\t exception packet: %llu\n", exception_packet);
	seq_printf(m, "\t exception bytes: %llu\n", exception_bytes);
	seq_printf(m, "\n################ PPE Client gre Statistics End ################\n");

	return 0;
}

/*
 * ppe_gre_mgr_stats_open()
 * 	Display PPE tunnel statistics
 */
static int ppe_gre_mgr_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_gre_mgr_stats_show, inode->i_private);
}

/*
 * ppe_gre_mgr_stats_ops
 * 	File operations for gre tunnel stats
 */
static const struct file_operations ppe_gre_mgr_stats_ops = {
	.open = ppe_gre_mgr_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * ppe_gre_mgr_client_stats_show()
 * 	Read GRE client statistics
 */
static int ppe_gre_mgr_client_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_gre_mgr_ctx *ctx = (struct ppe_gre_mgr_ctx *)m->private;

	seq_printf(m, "\n################ GRE client statistics Start################\n");
	seq_printf(m, "\tTunnel create request with iflag Sequence number: %llu\n", atomic64_read(&ctx->stats.iflag_seq_err));
	seq_printf(m, "\tTunnel create request with oflag Sequence number: %llu\n", atomic64_read(&ctx->stats.oflag_seq_err));
	seq_printf(m, "\tV6 tunnel create requests with non null encap limit: %llu\n", atomic64_read(&ctx->stats.enc_lim_err));
	seq_printf(m, "\tGRETAP source exception packet drop counter: %llu\n", atomic64_read(&ctx->stats.gretap_src_excep_drop_count));
	seq_printf(m, "\tGRETUN source exception packet drop counter: %llu\n", atomic64_read(&ctx->stats.gretun_src_excep_drop_count));
	seq_printf(m, "\tGRETUN key flags set failure: %llu\n", atomic64_read(&ctx->stats.gretun_key_flag_failure));
	seq_printf(m, "\tGRETUN csum flags set failure: %llu\n", atomic64_read(&ctx->stats.gretun_csum_flag_failure));
	seq_printf(m, "\n################ GRE Client Statistics End ################\n");

	return 0;
}

/*
 * ppe_gre_mgr_client_stats_open()
 * 	Display GRE client statistics
 */
static int ppe_gre_mgr_client_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_gre_mgr_client_stats_show, inode->i_private);
}

/*
 * ppe_gre_mgr_client_stats_ops()
 * 	File operations for GRE client stats
 */
static const struct file_operations ppe_gre_mgr_client_stats_ops = {
	.open = ppe_gre_mgr_client_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * ppe_gre_mgr_dev_stats_update()
 * 	Update gre dev statistics
 */
bool ppe_gre_mgr_dev_stats_update(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_data)
{
	struct pcpu_sw_netstats *tstats;

	if (!dev) {
		return false;
	}

	tstats = this_cpu_ptr(dev->tstats);
	u64_stats_update_begin(&tstats->syncp);
	u64_stats_add(&tstats->tx_bytes, stats->tx_byte_cnt);
	u64_stats_add(&tstats->tx_packets,  stats->tx_pkt_cnt);
	u64_stats_add(&tstats->rx_bytes, stats->rx_byte_cnt);
	u64_stats_add(&tstats->rx_packets,  stats->rx_pkt_cnt);
	u64_stats_update_end(&tstats->syncp);

	return true;
}

/*
 * ppe_gre_mgr_stats_dentry_create()
 * 	Create dentry for a given netdevice.
 */
bool ppe_gre_mgr_stats_dentry_create(struct ppe_gre_mgr_ctx *ppe_ctx, struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;

	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);
	dentry = debugfs_create_file(dentry_name, S_IRUGO,
			ppe_ctx->dentry, dev, &ppe_gre_mgr_stats_ops);
	if (!dentry) {
		gre_mgr_warning("%px: Debugfs file creation failed for device %s\n", dev, dev->name);
		return false;
	}

	return true;
}

/*
 * ppe_gre_mgr_stats_dentry_free()
 * 	Remove dentry for a given netdevice.
 */
bool ppe_gre_mgr_stats_dentry_free(struct ppe_gre_mgr_ctx *ctx, struct net_device *dev)
{
	char dentry_name[IFNAMSIZ];
	struct dentry *dentry;

	scnprintf(dentry_name, sizeof(dentry_name), "%s", dev->name);
	dentry = debugfs_lookup(dentry_name, ctx->dentry);
	if (dentry) {
		debugfs_remove(dentry);
		gre_mgr_trace("%px: removed stats debugfs entry for dev %s", dev, dentry_name);
		return true;
	}

	gre_mgr_trace("%px: Could not find stats debugfs entry for dev %s", dev, dentry_name);
	return false;
}

/*
 * ppe_gre_mgr_stats_debugfs_init()
 */
struct dentry *ppe_gre_mgr_stats_debugfs_init(struct ppe_gre_mgr_ctx *ppe_ctx)
{
	return debugfs_create_file("client", S_IRUGO, ppe_ctx->dentry, ppe_ctx, &ppe_gre_mgr_client_stats_ops);
}
