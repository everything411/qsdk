/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/debugfs.h>
#include "ppe_ds_stats.h"
#include <ppe_drv.h>

static struct dentry *dbgfs;
struct ppe_ds_stats ppe_ds_node_stats[PPE_DS_MAX_NODE];
struct ppe_ds_idx_hys ppe_ds_idx_hyst[PPE_DS_MAX_NODE];
struct ppe_ds_ts_hys ppe_ds_ts_hyst[PPE_DS_MAX_NODE];
uint32_t enable_ring_hyst_stats;

struct ppe_ds_ts_hys_range_list ppe_ds_ts_hys_range = {
	{
		{ INT_MAX, 900, "Gt  900us"},
		{ 900, 800, "800-900us"},
		{ 800, 700, "700-800us"},
		{ 700, 600, "600-700us"},
		{ 600, 500, "500-600us"},
		{ 500, 400, "400-500us"},
		{ 400, 300, "300-400us"},
		{ 300, 200, "200-300us"},
		{ 200, 100, "100-200us"},
		{ 100, 90, "90-100us"},
		{ 90, 80, "80-90us"},
		{ 80, 70, "70-80us"},
		{ 70, 60, "60-70us"},
		{ 60, 50, "50-60us"},
		{ 50, 40, "40-50us"},
		{ 40, 30, "30-40us"},
		{ 30, 20, "20-30us"},
		{ 20, 10, "10-20us"},
		{ 10, 0, "0-10us" },
	}
};

struct ppe_ds_idx_hys_range_list ppe_ds_idx_hys_range = {
	{
		{ INT_MAX, 7000, "7k-8k pkts"},
		{ 7000, 6000, "6k-7k pkts"},
		{ 6000, 5000, "5k-6k pkts"},
		{ 5000, 4000, "4k-5k pkts"},
		{ 4000, 3000, "3k-4k pkts"},
		{ 3000, 2000, "2k-3k pkts"},
		{ 2000, 1000, "1k-2k pkts"},
		{ 1000, 1, "1-1k pkts"}
	}, {
		{ INT_MAX, 7000, "7k-8k pkts"},
		{ 7000, 6000, "6k-7k pkts"},
		{ 6000, 5000, "5k-6k pkts"},
		{ 5000, 4000, "4k-5k pkts"},
		{ 4000, 3000, "3k-4k pkts"},
		{ 3000, 2000, "2k-3k pkts"},
		{ 2000, 1000, "1k-2k pkts"},
		{ 1000, 192, "1k-192 pkts"},
		{ 192, 128, "192-128 pkts"},
		{ 128, 100, "128-100 pkts"},
		{ 100, 50, "50-100 pkts"},
		{ 50, 1, "1-50 pkts"}
	}
};

/*
 * ppe_ds_node_info_show()
 *	Show the node evp information
 */
static int ppe_ds_node_info_show(struct seq_file *m,
		void __attribute__((unused))*ptr)
{
	int i = 0;

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		seq_printf(m, "Node: %d\n", i);
		seq_printf(m, "Node_Enqueue_VP: %d\n", ppe_drv_port_metadata_to_enq_vp(i));
		seq_printf(m, "Node_pri_profile: %d\n", ppe_drv_port_metadata_to_pri_prof(i));
		seq_printf(m, "Node_low_freq: %u MHz\n", ppe_ds_node_cfg[i].freq.low_freq);
		seq_printf(m, "Node_high_freq: %u MHz\n\n", ppe_ds_node_cfg[i].freq.high_freq);
	}
	return 0;
}

/*
 * ppe_ds_node_idx_hyst_show()
 * 	Show the per node hysteresis stats of index movements
 * 	for PPE2TCL, REO2PPE, and EDMA interrupts.
 */
void ppe_ds_node_idx_hyst_show(struct seq_file *m)
{
	int i = 0, j = 0;

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		seq_printf(m, "Hysteresis - Num pkts ppe2tcl per interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_VLD_RING_ENTR_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.vld_ring_entr_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].ppe2tcl.vld_ring_entr[j]));

		seq_printf(m, "Hysteresis - Num pkts ppe2tcl per prod interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_IDX_MV_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.idx_mv_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].ppe2tcl.prod_idx_mv[j]));

		seq_printf(m, "Hysteresis - Num pkts ppe2tcl per cons interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_IDX_MV_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.idx_mv_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].ppe2tcl.cons_idx_mv[j]));

		seq_printf(m, "Hysteresis - Num pkts per edma interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_IDX_MV_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.idx_mv_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].edma_prod_mv[j]));

		seq_printf(m, "Hysteresis - Num pkts reo2ppe per interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_VLD_RING_ENTR_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.vld_ring_entr_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].reo2ppe.vld_ring_entr[j]));

		seq_printf(m, "Hysteresis - Num pkts reo2ppe per cons interrupt - Node: %d\n", i);
		for (j = 0; j < PPE_DS_IDX_MV_HYS_MAX ; j++)
			seq_printf(m, "%s :%u\n", ppe_ds_idx_hys_range.idx_mv_range[j].tag, atomic_read(&ppe_ds_idx_hyst[i].reo2ppe.cons_idx_mv[j]));
	}
}

/*
 * ppe_ds_node_ts_intr_hyst_show()
 *      Show the per node hysterisis stats of wlan interrupts
 *      timestamp for ppe2tcl and reo2ppe rings.
 */
void ppe_ds_node_ts_intr_hyst_show(struct seq_file *m)
{
	int i = 0, j = 0;

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		seq_printf(m, "Hysteresis - Timestamp ppe2tcl diff between interrupt - Node %d\n", i);
		for (j = 0; j < PPE_DS_TS_HYS_MAX; j++)
			seq_printf(m, "%s  :%llu     :%llu\n", ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[j].tag, atomic64_read(&ppe_ds_ts_hyst[i].ppe2tcl.ts_bt_intr[j]),
					atomic64_read(&ppe_ds_ts_hyst[i].ppe2tcl.waste_intr[j]));

		seq_printf(m, "Hysteresis - Timestamp reo2ppe diff between interrupt - Node %d\n", i);
		for (j = 0; j < PPE_DS_TS_HYS_MAX; j++)
			seq_printf(m, "%s  :%llu     :%llu\n", ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[j].tag, atomic64_read(&ppe_ds_ts_hyst[i].reo2ppe.ts_bt_intr[j]),
					atomic64_read(&ppe_ds_ts_hyst[i].reo2ppe.waste_intr[j]));
	}
}

/*
 * ppe_ds_node_stats_show()
 *	Show the stats of per node
 */
static int ppe_ds_node_stats_show(struct seq_file *m,
		void __attribute__((unused))*ptr)
{
	int i = 0;

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		seq_printf(m, "\nNode_%d:\n", i);
		seq_printf(m, "Tx_pkts: %llu\n", atomic64_read(&ppe_ds_node_stats[i].tx_pkts));
		seq_printf(m, "Rx_pkts: %llu\n", atomic64_read(&ppe_ds_node_stats[i].rx_pkts));
		seq_printf(m, "still_prod: %llu\n", atomic64_read(&ppe_ds_node_stats[i].ppe2tcl_still_prod));
		seq_printf(m, "still_cons: %llu\n", atomic64_read(&ppe_ds_node_stats[i].ppe2tcl_still_cons));
		seq_printf(m, "wifi_intr_count: %llu\n", atomic64_read(&ppe_ds_node_stats[i].wlan_ppe2tcl_intr_cnt));
		seq_printf(m, "prod_n_con_same: %llu\n", atomic64_read(&ppe_ds_node_stats[i].prod_n_con_same));
		seq_printf(m, "reo_still_cons: %llu\n", atomic64_read(&ppe_ds_node_stats[i].reo2ppe_still_cons));
		seq_printf(m, "reo_wifi_intr_count: %llu\n", atomic64_read(&ppe_ds_node_stats[i].wlan_reo2ppe_intr_cnt));

	}

	if (enable_ring_hyst_stats) {
		ppe_ds_node_idx_hyst_show(m);
		ppe_ds_node_ts_intr_hyst_show(m);
	}
	return 0;
}

/*
 * ppe_ds_node_info_open()
 *	PPE DS node infromation open callback API
 */
static int ppe_ds_node_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_ds_node_info_show, inode->i_private);
}

/*
 * ppe_ds_node_stats_open()
 *	PPE DS STATS open callback API
 */
static int ppe_ds_node_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_ds_node_stats_show, inode->i_private);
}

/*
 * ppe_ds_node_info_file_ops
 *	File operations for DS Node information
 */
const struct file_operations ppe_ds_node_info_file_ops = {
	.open = ppe_ds_node_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_ds_node_stats_general_file_ops
 *	File operations for DS Node stats
 */
const struct file_operations ppe_ds_node_stats_file_ops = {
	.open = ppe_ds_node_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*
 * ppe_ds_node_ring_stats_hyst_show()
 * 	Print the hysteresis ring stats.
 */
static int ppe_ds_node_ring_stats_hyst_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%d\n", enable_ring_hyst_stats);
	return 0;
}

/*
 * proc_ds_node_ring_stats_hyst_open()
 *  	File open operation for extended hysterisis stats.
 */
static int proc_ds_node_ring_stats_hyst_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			ppe_ds_node_ring_stats_hyst_show,
			inode->i_private);
}

/*
 * proc_ds_node_ring_stats_hyst_write()
 *      File write operation for extended hysterisis stats.
 */

static ssize_t proc_ds_node_ring_stats_hyst_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	int ret;
	int enable;
	char buffer[13];

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count) != 0)
		return -EFAULT;
	ret = kstrtoint(strstrip(buffer), 10, &enable);
	if (ret == 0 && enable >= 0)
		enable_ring_hyst_stats = enable;

	return count;
}

/*
 * proc_ds_node_ring_stats_hyst_write()
 *      File write operation for ds ring hysterisis stats.
 */
const struct file_operations ppe_ds_node_ring_stats_file_ops = {
	.open    = proc_ds_node_ring_stats_hyst_open,
	.read    = seq_read,
	.write   = proc_ds_node_ring_stats_hyst_write,
	.release = seq_release,
};

/*
 * ppe_ds_node_stats_debugfs_exit()
 *	PPE debugfs exit api
 */
void ppe_ds_node_stats_debugfs_exit(void)
{
	debugfs_remove_recursive(dbgfs);
	dbgfs = NULL;
}

/*
 * ppe_ds_node_stats_debugfs_init
 *	Create PPE statistics debug entry.
 */
int ppe_ds_node_stats_debugfs_init(void)
{
	struct dentry *root = ppe_drv_get_dentry();

	if (!root) {
		ppe_ds_warn("Unable to get root stats directory\n");
		return -1;
	}

	dbgfs = debugfs_create_dir("ppe_ds", root);
	if (!dbgfs) {
		ppe_ds_warn("%p: Unable to create debugfs stats directory in debugfs\n", dbgfs);
		return -1;
	}

	if (!debugfs_create_file("ppe_ds_node_stats", S_IRUGO, dbgfs, NULL, &ppe_ds_node_stats_file_ops)) {
		ppe_ds_warn("%p: Unable to create common statistics file entry in debugfs\n", dbgfs);
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("ppe_ds_node_info", S_IRUGO, dbgfs, NULL, &ppe_ds_node_info_file_ops)) {
		ppe_ds_warn("%p: Unable to create common statistics file entry in debugfs\n", dbgfs);
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("enable_ring_hyst_stats", S_IRUGO | S_IWUSR, dbgfs, NULL, &ppe_ds_node_ring_stats_file_ops)) {
		ppe_ds_warn("%p: Unable to create common statistics file entry in debugfs\n", dbgfs);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(dbgfs);
	dbgfs = NULL;
	return -1;
}
