/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <fal/fal_tunnel.h>
#include <ppe_drv_port.h>
#include <ppe_drv_iface.h>
#include "ppe_vp_base.h"

#define RX_STATS_COUNT		7
#define TX_STATS_COUNT		4

extern struct ppe_vp_base vp_base;

static const char *ppe_vp_stats_base_str[] = {
	"VP_Allocation_fails",			/* Total VP allocation failures */
	"VP_Table_full_errors",			/* VP allocation fails due to table full */
	"MTU_assign_fails",			/* MTU assign fails */
	"MAC_assign_fails",			/* MAC assign fails */
	"Rx_Destination_VP_Inactive",		/* Packet received from PPE with inactive destinaton VP */
	"Rx_Source_VP_Inactive",		/* Packet received from PPE with inactive Source VP */
	"Rx_Destination_VP_Invalid",		/* Packet received from PPE without valid SVP */
	"Rx_Source_VP_Invalid",			/* Packet received from PPE without valid DVP */
	"Tx_VP_Inactive",			/* VP of Packet forwarded by VP user is inactive */
	"Rx_Fast_tramist_failed",		/* Rx packet fast transmit failed */
	"Rx_Qdisc_Fast_tramist_failed",		/* Rx packet Qdisc fast transmit failed */
	"Rx_Destination_VP_no_listcb",		/* Packet received from PPE with inactive destinaton VP */
	"Rx_Destination_VP_no_xdpcb"		/* XDP handler not registered for XDP destination VP */
};

static const char *ppe_vp_stats_rx_str[] = {
	"Rx_packets",				/* Total rx packets */
	"Rx_bytes",				/* Total rx bytes */
	"Rx_exceptioned packets",		/* Total exceptioned VP packets */
	"Rx_exceptioned bytes",			/* Total exceptioned VP bytes */
	"Rx_errors",				/* Total rx errors */
	"Rx_drops",				/* Total rx drops */
	"Rx_dev_not_up"				/* Received packets before dev IFF_UP */
};

static const char *ppe_vp_stats_tx_str[] = {
	"Tx_packets",				/* Total tx packets */
	"Tx_bytes",				/* Total tx bytes */
	"Tx_errors",				/* Total tx errors */
	"Tx_drops"				/* Total tx drops */
};

static const char *vp_user_mode[] = {
	"none",
	"passive",
	"active",
	"ds",
	"max"
};

/*
 * ppe_vp_stats_hw_port_stats_sync()
 *	Sync PPE HW stats
 */
static void ppe_vp_stats_hw_port_stats_sync(struct timer_list *tm)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_vp *vp;
	struct net_device *netdev;
	struct ppe_drv_port_hw_stats port_stats;
        ppe_vp_hw_stats_t *vp_hw_stats;
	ppe_vp_hw_stats_t delta_stats;
	ppe_vp_stats_callback_t stats_cb;
	int i;

	for (i = 0; i < PPE_DRV_VIRTUAL_MAX; i++) {
		vp = ppe_vp_base_get_vp_by_idx(i);
		if (!vp) {
			continue;
		}

		spin_lock(&vp->lock);

		/*
		 * Get the current PPE port stats for the VP port from PPE HW.
		 */
		if (!ppe_drv_port_get_vp_stats(vp->port_num, &port_stats)) {
			spin_unlock(&vp->lock);
			ppe_vp_warn("%p: failed to get port stats for the vp_num: %u", vp, vp->port_num);
			continue;
		}

		/*
		 * Get the stats snapshot from the previous timer interrupt.
		 */
		vp_hw_stats = &vp->vp_stats.vp_hw_stats;

		/*
		 * Calculate RX delta stats.
		 * This will handle hw stats counter overflow case as well.
		 */
		delta_stats.rx_pkt_cnt = (port_stats.rx_pkt_cnt - vp_hw_stats->rx_pkt_cnt + FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_PKT_CNT_MASK;
		delta_stats.rx_byte_cnt = (port_stats.rx_byte_cnt - vp_hw_stats->rx_byte_cnt + FAL_TUNNEL_DECAP_BYTE_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_BYTE_CNT_MASK;
		delta_stats.rx_drop_pkt_cnt = (port_stats.rx_drop_pkt_cnt - vp_hw_stats->rx_drop_pkt_cnt + FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_PKT_CNT_MASK;
		delta_stats.rx_drop_byte_cnt = (port_stats.rx_drop_byte_cnt - vp_hw_stats->rx_drop_byte_cnt + FAL_TUNNEL_DECAP_BYTE_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_BYTE_CNT_MASK;

		/*
		 * Calculate Tx delta stats.
		 */
		delta_stats.tx_pkt_cnt = (port_stats.tx_pkt_cnt - vp_hw_stats->tx_pkt_cnt + FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_PKT_CNT_MASK;
		delta_stats.tx_byte_cnt = (port_stats.tx_byte_cnt - vp_hw_stats->tx_byte_cnt + FAL_TUNNEL_DECAP_BYTE_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_BYTE_CNT_MASK;
		delta_stats.tx_drop_pkt_cnt = (port_stats.tx_drop_pkt_cnt - vp_hw_stats->tx_drop_pkt_cnt + FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_PKT_CNT_MASK;
		delta_stats.tx_drop_byte_cnt = (port_stats.tx_drop_byte_cnt - vp_hw_stats->tx_drop_byte_cnt + FAL_TUNNEL_DECAP_BYTE_CNT_MASK + 1)
				& FAL_TUNNEL_DECAP_BYTE_CNT_MASK;

		/*
		 * Take a snapshot of PPE HW stats for delta calculation in the next
		 * timer.
		 */
		memcpy(vp_hw_stats, &port_stats, sizeof(ppe_vp_hw_stats_t));
		stats_cb = vp->stats_cb;
		netdev = vp->netdev;
		spin_unlock(&vp->lock);

		/*
		 * Send the delta stats to VP callback function.
		 */
		if (stats_cb) {
			stats_cb(netdev, &delta_stats);
		}
		ppe_vp_trace("%px: Sync VP port %u statistics", vp, vp->port_num);

	}

	/*
	 * Re arm the hardware stats timer
	 */
	mod_timer(&pvb->hw_port_stats_timer, jiffies + pvb->hw_port_stats_ticks);
}

/*
 * ppe_vp_stats_reset_per_cpu_stats()
 *	Reset VP's per CPU stats.
 */
static void ppe_vp_stats_reset_per_cpu_stats(struct ppe_vp_stats *vp_stats)
{
	struct ppe_vp_rx_stats *rx_pcpu_stats;
	struct ppe_vp_tx_stats *tx_pcpu_stats;
	int i;

	for_each_possible_cpu(i) {
		unsigned int start;

		rx_pcpu_stats = per_cpu_ptr(vp_stats->rx_stats, i);

		do {
			start = ppe_vp_stats_fetch_begin(&rx_pcpu_stats->syncp);
			memset(rx_pcpu_stats, 0, sizeof(*rx_pcpu_stats));
		} while (ppe_vp_stats_fetch_retry(&rx_pcpu_stats->syncp, start));

		tx_pcpu_stats = per_cpu_ptr(vp_stats->tx_stats, i);

		do {
			start = ppe_vp_stats_fetch_begin(&tx_pcpu_stats->syncp);
			memset(tx_pcpu_stats, 0, sizeof(*tx_pcpu_stats));
		} while (ppe_vp_stats_fetch_retry(&tx_pcpu_stats->syncp, start));
	}
}

static int ppe_vp_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_vp *vp;
	struct ppe_vp_base_stats *pvb_stats;
	uint64_t *stats_shadow;
	struct ppe_vp_stats *vp_stats;
	struct ppe_vp_rx_stats *rx_pcpu_stats, rx_stats;
	struct ppe_vp_tx_stats *tx_pcpu_stats, tx_stats;
	uint64_t rx_aggr[RX_STATS_COUNT], tx_aggr[TX_STATS_COUNT];
	uint32_t active_vp_counter = 0;
	int16_t idx;
	uint32_t i;
	int mode = 0;

	/*
	 * Read the statistics from the main structure for
	 * 1. PPE-VP base, 2. PPE-VP
	 */
	pvb_stats = kmalloc(sizeof(struct ppe_vp_base_stats), GFP_KERNEL);
	if (!pvb_stats) {
		ppe_vp_warn("Failed to allocate memory for pvb\n");
		return -ENOMEM;
	}

	memcpy(pvb_stats, &pvb->base_stats, sizeof(struct ppe_vp_base_stats));

	/*
	 * Start displaying the Base VP stats
	 */
	seq_printf(m, "\n################ Virtual Port Statistics Start ################\n");
	seq_printf(m, "\nBase_VP_Statistics:\n");
	stats_shadow = (uint64_t *)pvb_stats;
	seq_printf(m, "\tActive_VPs: %u\n", pvb->vp_table.active_vp);
	for (i = 0; i < (sizeof(struct ppe_vp_base_stats) / sizeof(uint64_t)); i++) {
		seq_printf(m, "\t%s:  %llu\n", ppe_vp_stats_base_str[i], stats_shadow[i]);
	}

	/*
	 * Start displaying all the active VP stats.
	 */
	active_vp_counter = ppe_vp_base_get_active_vp_count(pvb);
	seq_printf(m, "\nVP_Statistics:\n\tActive_Count: %u\n", active_vp_counter);

	for (idx = 0; idx < PPE_DRV_VIRTUAL_MAX; idx++) {

		memset(rx_aggr, 0, sizeof(rx_aggr));
		memset(tx_aggr, 0, sizeof(tx_aggr));

		rcu_read_lock();
		vp = ppe_vp_base_get_vp_by_idx(idx);
		if (vp) {
			vp_stats = &vp->vp_stats;

			seq_printf(m, "\tVP_Port: %u\n", vp_stats->misc_info.ppe_port_num);
			seq_printf(m, "\t\tNetdev_if_num: %u\n", vp_stats->misc_info.netdev_if_num);
			seq_printf(m, "\t\tNetdev_name: %s\n", vp->netdev->name);
			mode = vp->vp_user_mode;

			/*
			 * Print only for wifi type.
			 */
			if (mode) {
				seq_printf(m, "\t\tVP_user_mode: %s\n", vp_user_mode[mode]);
			}

			/*
			 * Active VP: Accumulate stats from all CPUs.
			 */
			for_each_possible_cpu(i) {

				unsigned int start;
				rx_pcpu_stats = per_cpu_ptr(vp_stats->rx_stats, i);

				do {
					start = ppe_vp_stats_fetch_begin(&rx_pcpu_stats->syncp);
					memcpy(&rx_stats, rx_pcpu_stats, sizeof(*rx_pcpu_stats));
				} while (ppe_vp_stats_fetch_retry(&rx_pcpu_stats->syncp, start));

				rx_aggr[0] += rx_stats.rx_pkts;
				rx_aggr[1] += rx_stats.rx_bytes;
				rx_aggr[2] += rx_stats.rx_excp_pkts;
				rx_aggr[3] += rx_stats.rx_excp_bytes;
				rx_aggr[4] += rx_stats.rx_errors;
				rx_aggr[5] += rx_stats.rx_drops;
				rx_aggr[6] += rx_stats.rx_dev_not_up;

				tx_pcpu_stats = per_cpu_ptr(vp_stats->tx_stats, i);

				do {
					start = ppe_vp_stats_fetch_begin(&tx_pcpu_stats->syncp);
					memcpy(&tx_stats, tx_pcpu_stats, sizeof(*tx_pcpu_stats));
				} while (ppe_vp_stats_fetch_retry(&tx_pcpu_stats->syncp, start));

				tx_aggr[0] += tx_stats.tx_pkts;
				tx_aggr[1] += tx_stats.tx_bytes;
				tx_aggr[2] += tx_stats.tx_errors;
				tx_aggr[3] += tx_stats.tx_drops;
			}

			seq_printf(m, "\n\t\tVP_Rx_Stats:\n");
			stats_shadow = (uint64_t *)rx_aggr;
			for (i = 0; i < RX_STATS_COUNT; i++) {
				seq_printf(m, "\t\t\t%s:  %llu\n", ppe_vp_stats_rx_str[i], stats_shadow[i]);
			}

			seq_printf(m, "\n\t\tVP_Tx_Stats:\n");
			stats_shadow = (uint64_t *)tx_aggr;
			for (i = 0; i < TX_STATS_COUNT; i++) {
				seq_printf(m, "\t\t\t%s:  %llu\n", ppe_vp_stats_tx_str[i], stats_shadow[i]);
			}

			seq_printf(m, "\n\t\tHW_port_counters:\n");
			seq_printf(m, "\t\t\trx_pkts: %u\n", vp_stats->vp_hw_stats.rx_pkt_cnt);
			seq_printf(m, "\t\t\trx_bytes: %llu\n", vp_stats->vp_hw_stats.rx_byte_cnt);
			seq_printf(m, "\t\t\ttx_pkts: %u\n", vp_stats->vp_hw_stats.tx_pkt_cnt);
			seq_printf(m, "\t\t\ttx_bytes: %llu\n\n", vp_stats->vp_hw_stats.tx_byte_cnt);
		}

		rcu_read_unlock();
	}

	seq_printf(m, "\n################ Virtual Port Statistics End ################\n\n");

	kfree(pvb_stats);
	return 0;
}

/*
 * ppe_vp_stats_open()
 *	Read IPV4 stats
 */
static int ppe_vp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_vp_stats_show, inode->i_private);
}

/*
 * ppe_vp_stats_ops
 *	File operations for PPE-VP (also base) stats
 */
static const struct file_operations ppe_vp_stats_ops = {
		.open = ppe_vp_stats_open,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = seq_release,
};

/*
 * ppe_vp_stats_reset_vp_stats()
 *	Reset VP statistics.
 */
void ppe_vp_stats_reset_vp_stats(struct ppe_vp_stats *vp_stats)
{
	memset(vp_stats, 0, (sizeof(ppe_vp_hw_stats_t) + sizeof(struct ppe_vp_misc_info)));
	ppe_vp_stats_reset_per_cpu_stats(vp_stats);
}

/*
 * ppe_vp_stats_deinit()
 *	Free VP statistics.
 */
ppe_vp_status_t ppe_vp_stats_deinit(struct ppe_vp *vp)
{
	struct ppe_vp_stats *vp_stats = &vp->vp_stats;

	if (!vp_stats->rx_stats) {
		ppe_vp_warn("%px: Percpu Rx stats not allocated\n", vp);
		return PPE_VP_STATUS_FAILURE;
	}

	free_percpu(vp_stats->rx_stats);
	vp_stats->rx_stats = NULL;

	if (!vp_stats->tx_stats) {
		ppe_vp_warn("%px: Percpu Rx stats not allocated\n", vp);
		return PPE_VP_STATUS_FAILURE;
	}

	free_percpu(vp_stats->tx_stats);
	vp_stats->tx_stats = NULL;

	return PPE_VP_STATUS_SUCCESS;
}

/*
 * ppe_vp_stats_init()
 *	Initialize VP statistics.
 */
ppe_vp_status_t ppe_vp_stats_init(struct ppe_vp *vp)
{
	struct ppe_vp_stats *vp_stats = &vp->vp_stats;

	/*
	 * Allocate per-cpu stats memory
	 */
	vp_stats->rx_stats = netdev_alloc_pcpu_stats(struct ppe_vp_rx_stats);
	if (!vp_stats->rx_stats) {
		ppe_vp_warn("Percpu Rx stats alloc failed for VP %px\n", vp);
		return PPE_VP_STATUS_FAILURE;
	}

	vp_stats->tx_stats = netdev_alloc_pcpu_stats(struct ppe_vp_tx_stats);
	if (!vp_stats->tx_stats) {
		ppe_vp_warn("Percpu Tx stats alloc failed for VP %px\n", vp);
		free_percpu(vp_stats->rx_stats);
		vp_stats->rx_stats = NULL;
		return PPE_VP_STATUS_FAILURE;
	}

	return PPE_VP_STATUS_SUCCESS;
}

/*
 * ppe_vp_base_stats_init()
 *	Initialize VP base statistics.
 *
 * Note: No need to remove the file. The upper level directory removes this file
 * recursively.
 */
ppe_vp_status_t ppe_vp_base_stats_init(struct ppe_vp_base *pvb)
{
	struct dentry *vp_dentry;
	vp_dentry = debugfs_create_dir("ppe_vp", pvb->dentry);
	if (!debugfs_create_file("vp_stats", S_IRUGO, vp_dentry, pvb, &ppe_vp_stats_ops)) {
		ppe_vp_warn("%px: Failed to create debug entry for all VP stats\n", pvb);
		return PPE_VP_STATUS_FAILURE;
	}

	pvb->hw_port_stats_ticks = msecs_to_jiffies(PPE_VP_HW_PORT_STATS_MS);
	timer_setup(&pvb->hw_port_stats_timer, ppe_vp_stats_hw_port_stats_sync, 0);

	/*
	* Start sync timer for hardware stats collection.
	*/
	mod_timer(&pvb->hw_port_stats_timer, jiffies + pvb->hw_port_stats_ticks);

	return PPE_VP_STATUS_SUCCESS;
}
