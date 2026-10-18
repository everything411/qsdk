/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

/* DEBUG FS WLAN IPA */

#include "dp_ipa.h"
#include "../../wifi7/qcn_extns/ipa/dp_ipa.h"
#include <linux/seq_file.h>

/* Provide a readable alias for IPA WDI stats structure */
typedef struct IpaHwStatsWDIInfoData_t ath12k_ipa_wdi_stats_t;

/**
 * ath12k_debugfs_ipa_format_host_stats - append host-side IPA stats to seq_file
 * @ipa_ctx: IPA context to read counters from
 * @s: seq_file to print to
 *
 * Formats and prints wlan IPA counters maintained by the driver into the
 * provided seq_file. Does not modify driver state.
 */
static void ath12k_debugfs_ipa_format_host_stats(struct ath12k_ipa *ipa_ctx,
						 struct seq_file *s)
{
	/* Messaging counters */
	seq_printf(s, "Messaging/RM:\n");
	seq_printf(s, "  send_msg: %llu\n",
		   ipa_ctx->stats.send_msg);
	seq_printf(s, "  failed_msg: %llu\n",
		   ipa_ctx->stats.failed_msg);

	/* Pipe requests*/
	seq_printf(s, "  cons_perf_req: %llu\n",
		   ipa_ctx->stats.cons_perf_req);
	seq_printf(s, "  prod_perf_req: %llu\n",
		   ipa_ctx->stats.prod_perf_req);

	/* RX datapath counters */
	seq_printf(s, "RX:\n");
	seq_printf(s, "  rx_drop_excep: %llu\n",
		   ipa_ctx->stats.rx_drop_excep);
	seq_printf(s, "  rx_excep: %llu\n",
		   ipa_ctx->stats.rx_excep);
	seq_printf(s, "  rx_no_iface: %llu\n",
		   ipa_ctx->stats.rx_no_iface);
	seq_printf(s, "  rx_eapol: %llu\n",
		   ipa_ctx->stats.rx_eapol);

	/* TX datapath counters */
	seq_printf(s, "TX:\n");
	seq_printf(s, "  tx_desc_q_cnt: %llu\n",
		   ipa_ctx->stats.tx_desc_q_cnt);
	seq_printf(s, "  tx_desc_error: %llu\n",
		   ipa_ctx->stats.tx_desc_error);

	/* RX sent to MAC stats */
	seq_printf(s, "Sent to MAC:\n");
	seq_printf(s, "  sent_to_mac: %llu\n",
		   ipa_ctx->stats.sent_to_mac);
}

/**
 * ath12k_debugfs_ipa_collect_wdi_stats - fetch IPA driver WDI stats
 * @ipa_stat: output stats structure to fill
 *
 * Zeroes the stats structure and fetches WDI stats from the IPA driver.
 */
static void  ath12k_debugfs_ipa_collect_wdi_stats(ath12k_ipa_wdi_stats_t *ipa_stat)
{
	memset(ipa_stat, 0, sizeof(*ipa_stat));
	ipa_get_wdi_stats(ipa_stat);
}

/*
 * ath12k_debugfs_ipa_format_ipa_wdi_stats - append IPA driver WDI stats to buffer
 * @ipa_stat: pointer to collected stats
 * @s:        seq_file to print to
 *
 * This helper assumes stats were already collected successfully.
 * It only prints fields for readability.
 */
static void ath12k_debugfs_ipa_format_ipa_wdi_stats(const ath12k_ipa_wdi_stats_t *ipa_stat,
						    struct seq_file *s)
{
	/* TX channel stats from IPA driver */
	seq_printf(s, "IPA WDI TX:\n");
	seq_printf(s, "  num_pkts_processed: %d\n",
		   ipa_stat->tx_ch_stats.num_pkts_processed);
	seq_printf(s, "  copy_engine_doorbell_value: 0x%x\n",
		   ipa_stat->tx_ch_stats.copy_engine_doorbell_value);
	seq_printf(s, "  num_db_fired: %d\n",
		   ipa_stat->tx_ch_stats.num_db_fired);
	seq_printf(s, "  tx_comp_ring: full=%d empty=%d hi=%d lo=%d\n",
		   ipa_stat->tx_ch_stats.tx_comp_ring_stats.ringFull,
		   ipa_stat->tx_ch_stats.tx_comp_ring_stats.ringEmpty,
		   ipa_stat->tx_ch_stats.tx_comp_ring_stats.ringUsageHigh,
		   ipa_stat->tx_ch_stats.tx_comp_ring_stats.ringUsageLow);
	seq_printf(s, "  bam_fifo: full=%d empty=%d hi=%d lo=%d\n",
		   ipa_stat->tx_ch_stats.bam_stats.bamFifoFull,
		   ipa_stat->tx_ch_stats.bam_stats.bamFifoEmpty,
		   ipa_stat->tx_ch_stats.bam_stats.bamFifoUsageHigh,
		   ipa_stat->tx_ch_stats.bam_stats.bamFifoUsageLow);
	seq_printf(s, "  num_db: %d unexpected_db: %d\n",
		   ipa_stat->tx_ch_stats.num_db,
		   ipa_stat->tx_ch_stats.num_unexpected_db);
	seq_printf(s, "  num_bam_int_handled: 0x%x num_qmb_int_handled: 0x%x\n",
		   ipa_stat->tx_ch_stats.num_bam_int_handled,
		   ipa_stat->tx_ch_stats.num_qmb_int_handled);

	/* RX channel stats from IPA driver */
	seq_printf(s, "IPA WDI RX:\n");
	seq_printf(s, "  max_outstanding_pkts: %d\n",
		   ipa_stat->rx_ch_stats.max_outstanding_pkts);
	seq_printf(s, "  num_pkts_processed: %d\n",
		   ipa_stat->rx_ch_stats.num_pkts_processed);
	seq_printf(s, "  rx_ring_rp_value: 0x%x\n",
		   ipa_stat->rx_ch_stats.rx_ring_rp_value);
	seq_printf(s, "  rx_ind_ring: full=%d empty=%d hi=%d lo=%d\n",
		   ipa_stat->rx_ch_stats.rx_ind_ring_stats.ringFull,
		   ipa_stat->rx_ch_stats.rx_ind_ring_stats.ringEmpty,
		   ipa_stat->rx_ch_stats.rx_ind_ring_stats.ringUsageHigh,
		   ipa_stat->rx_ch_stats.rx_ind_ring_stats.ringUsageLow);
	seq_printf(s, "  bam_fifo: full=%d empty=%d hi=%d lo=%d\n",
		   ipa_stat->rx_ch_stats.bam_stats.bamFifoFull,
		   ipa_stat->rx_ch_stats.bam_stats.bamFifoEmpty,
		   ipa_stat->rx_ch_stats.bam_stats.bamFifoUsageHigh,
		   ipa_stat->rx_ch_stats.bam_stats.bamFifoUsageLow);
	seq_printf(s, "  num_db: %d unexpected_db: %d\n",
		   ipa_stat->rx_ch_stats.num_db,
		   ipa_stat->rx_ch_stats.num_unexpected_db);
	seq_printf(s, "  num_bam_int_handled: 0x%x\n",
		   ipa_stat->rx_ch_stats.num_bam_int_handled);
}

/**
 * ath12k_debugfs_ipa_append_wdi_stats - collect and append WDI stats
 * @s: seq_file to print to
 *
 * Collects WDI stats via ath12k_debugfs_ipa_collect_wdi_stats() and formats
 * them using ath12k_debugfs_ipa_format_ipa_wdi_stats(). Errors during
 * collection are ignored by this helper.
 */
static void ath12k_debugfs_ipa_append_wdi_stats(struct seq_file *s)
{
	ath12k_ipa_wdi_stats_t ipa_stat;

	ath12k_debugfs_ipa_collect_wdi_stats(&ipa_stat);
	ath12k_debugfs_ipa_format_ipa_wdi_stats(&ipa_stat, s);
}

/**
 * ath12k_debugfs_ipa_stats_show - seq_file show handler for IPA stats
 * @file: seq_file
 * @data: private data (unused)
 *
 * Prints host-side IPA counters and IPA driver WDI stats to debugfs.
 * Returns 0 on success, -EINVAL/-ENODEV if context is unavailable.
 */
static int ath12k_debugfs_ipa_stats_show(struct seq_file *file, void *data)
{
	struct ath12k *ar = file->private;
	struct ath12k_base *ab;
	struct ath12k_ipa *ipa_ctx;

	if (!ar)
		return -EINVAL;

	ab = ar->ab;
	if (!ab || !ab->ath12k_base_extn.ipa_ctx)
		return -ENODEV;

	ipa_ctx = ab->ath12k_base_extn.ipa_ctx;

	seq_printf(file, "IPA Debug Stats (pdev:%d radio:%d)\n",
		   ar->pdev_idx, ar->radio_idx);

	/* 1) host-side counters */
	ath12k_debugfs_ipa_format_host_stats(ipa_ctx, file);

	/* 2) IPA driver WDI stats */
	ath12k_debugfs_ipa_append_wdi_stats(file);

	return 0;
}

/**
 * ath12k_debugfs_ipa_stats_open - Open handler for IPA stats debugfs file
 * @inode: inode pointer
 * @file: file pointer
 *
 * Initializes a seq_file for reading IPA stats using single_open.
 * Return: 0 on success or a negative errno on failure.
 */
static int ath12k_debugfs_ipa_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ath12k_debugfs_ipa_stats_show, inode->i_private);
}


/*
 * ath12k_debugfs_ipa_write_wlan_ipa_stats - DebugFS write handler for IPA stats
 *
 * Supports simple maintenance commands:
 *   - "reset" : zero all counters in struct ath12k_ipa_stats
 *
 * Any other input returns -EINVAL.
 */
static ssize_t ath12k_debugfs_ipa_write_wlan_ipa_stats(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct ath12k_ipa *ipa_ctx;
	char buf[32] = {0};
	ssize_t len;

	if (!seq)
		return -EINVAL;

	ar = seq->private;
	if (!ar)
		return -EINVAL;

	ab = ar->ab;
	if (!ab || !ab->ath12k_base_extn.ipa_ctx)
		return -ENODEV;

	ipa_ctx = ab->ath12k_base_extn.ipa_ctx;

	if (count == 0 || count >= sizeof(buf))
		return -EINVAL;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, count);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (strstr(buf, "reset")) {
		memset(&ipa_ctx->stats, 0, sizeof(ipa_ctx->stats));
		return count;
	}

	return -EINVAL;
}

static const struct file_operations fops_wlan_ipa_stats = {
	.read = seq_read,
	.write = ath12k_debugfs_ipa_write_wlan_ipa_stats,
	.open = ath12k_debugfs_ipa_stats_open,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

/* Create 'ipa_stats' under the per-PDEV debugfs directory.
 * This exposes IPA counters for the corresponding radio (pdev).
 */
void ath12k_debugfs_register_ipa_extn(struct ath12k *ar)
{
	debugfs_create_file("ipa_stats", 0600,
			    ar->debug.debugfs_pdev, ar,
			    &fops_wlan_ipa_stats);
}

/*
 * wifi8 IPA TX offload stats — only compiled when building the wifi8 module.
 * __ATH12K_WIFI8_EXTNS_DP_IPA_H__ is defined by wifi8/src/ipa/dp_ipa.h which
 * is the dp_ipa.h seen in the wifi8 module build context (#include "dp_ipa.h"
 * above resolves to it).  In the arch module build, arch/dp_ipa.h is included
 * instead and this guard is not defined, so the block is skipped.
 */
#ifdef __ATH12K_WIFI8_EXTNS_DP_IPA_H__
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
#endif /* __ATH12K_WIFI8_EXTNS_DP_IPA_H__ */
