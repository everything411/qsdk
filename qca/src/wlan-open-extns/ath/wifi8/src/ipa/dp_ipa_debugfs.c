/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ipa.h"
#include <linux/seq_file.h>

static int ipa_tx_stats_show(struct seq_file *m, void *v)
{
	struct ath12k_ipa *ctx = m->private;
	int outstanding = ctx ? atomic_read(&ctx->tx_outstanding) : -1;
	int max_pkts    = ctx ? (int)ctx->offload_max_pkts : -1;

	seq_printf(m, "ipa_tx:      %lld\n", atomic64_read(&ipa_tx_ipa_count));
	seq_printf(m, "sw_tx:       %lld\n", atomic64_read(&ipa_tx_sw_count));
	seq_printf(m, "total_sw_tx: %lld\n", atomic64_read(&ipa_tx_total_sw_count));
	seq_printf(m, "qstop:       %lld\n", atomic64_read(&ipa_tx_qstop_count));
	seq_printf(m, "qwake:       %lld\n", atomic64_read(&ipa_tx_qwake_count));
	seq_printf(m, "pm_wake:     %lld\n", atomic64_read(&ipa_tx_pm_wake));
	seq_printf(m, "ipa_busy:    %lld\n", atomic64_read(&ipa_tx_busy_count));
	seq_printf(m, "outstanding: %d / %d\n", outstanding, max_pkts);
	seq_printf(m, "\ntx_queue[0/VO]: %lld\n", atomic64_read(&ipa_tx_queue_count[0]));
	seq_printf(m, "tx_queue[1/VI]: %lld\n", atomic64_read(&ipa_tx_queue_count[1]));
	seq_printf(m, "tx_queue[2/BE]: %lld\n", atomic64_read(&ipa_tx_queue_count[2]));
	seq_printf(m, "tx_queue[3/BK]: %lld\n", atomic64_read(&ipa_tx_queue_count[3]));
	seq_printf(m, "\nqueue_stuck: %s\n",
		   atomic64_read(&ipa_tx_qstop_count) >
		   atomic64_read(&ipa_tx_qwake_count) ? "YES" : "no");
	return 0;
}

static int ipa_tx_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ipa_tx_stats_show, inode->i_private);
}

static const struct file_operations ipa_tx_stats_fops = {
	.open    = ipa_tx_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

void ath12k_wifi8_dp_ipa_debugfs_init(struct dentry *root,
				      struct ath12k_ipa *ipa_ctx)
{
	debugfs_create_file("ipa_tx_stats", 0444, root, ipa_ctx,
			    &ipa_tx_stats_fops);
}
EXPORT_SYMBOL(ath12k_wifi8_dp_ipa_debugfs_init);
