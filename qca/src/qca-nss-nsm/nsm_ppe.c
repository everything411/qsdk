/*
 **************************************************************************
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#include "nsm_ppe.h"
#include "exports/nsm_nl_fam.h"
#include <linux/netdevice.h>
#include <nss_dp_api_if.h>

/*
 * nsm_ppe_get_service_stat_packets()
 *	Bundle the ppe service stat.
 */
int nsm_ppe_get_service_stats(nsm_ppe_service_stat_t *stats, uint8_t service_id)
{
	struct ppe_drv_nsm_stats ppe_nsm_stats_temp;

	if (!ppe_drv_nsm_sawf_sc_stats_read(&ppe_nsm_stats_temp, service_id)) {
		pr_err("ppe_drv_nsm_sawf_sc_stats_read() failed service_id:%d", service_id);
		return 0;
	}
	pr_info("PPE Service Stats Packets:%llu Bytes:%llu\n", ppe_nsm_stats_temp.sawf_sc_stats.rx_packets, ppe_nsm_stats_temp.sawf_sc_stats.rx_bytes);

	stats->packets = ppe_nsm_stats_temp.sawf_sc_stats.rx_packets;
	stats->bytes = ppe_nsm_stats_temp.sawf_sc_stats.rx_bytes;

	return 1;
}

/*
 * nsm_ppe_get_v4_flow_stats()
 *	Bundle the ppe flow stat.
 */
int nsm_ppe_get_v4_flow_stats(nsm_ppe_flow_stat_t *stats, struct ppe_drv_v4_5tuple *tuple)
{
	struct ppe_drv_nsm_stats nsm_stats;

	if(!ppe_drv_v4_nsm_stats_update(&nsm_stats, tuple)) {
		pr_err("nsm_ppe: Bad ipv4 flow stat lookup %pI4h:%d %pI4h:%d %d\n",
			&tuple->flow_ip, tuple->flow_ident, &tuple->return_ip, tuple->return_ident, tuple->protocol);
		return 0;
	}
	pr_info("Flow_Stats Match Found: Packets:%llu Bytes:%llu", nsm_stats.flow_stats.rx_packets, nsm_stats.flow_stats.rx_bytes);

	stats->packets = nsm_stats.flow_stats.rx_packets;
	stats->bytes = nsm_stats.flow_stats.rx_bytes;

	return 1;
}

/*
 * nsm_ppe_get_v6_flow_stats()
 *	Bundle the ppe flow stat.
 */
int nsm_ppe_get_v6_flow_stats(nsm_ppe_flow_stat_t *stats, struct ppe_drv_v6_5tuple *tuple)
{
	struct ppe_drv_nsm_stats nsm_stats;

	if(!ppe_drv_v6_nsm_stats_update(&nsm_stats, tuple)) {
		pr_err("nsm_ppe: Bad ipv6 flow stat lookup %pI6:%d %pI6:%d %d\n",
			&tuple->flow_ip[0], tuple->flow_ident, &tuple->return_ip[0],tuple->return_ident, tuple->protocol);
		return 0;
	}
	pr_info("Flow_Stats Match Found: Packets:%llu Bytes:%llu", nsm_stats.flow_stats.rx_packets, nsm_stats.flow_stats.rx_bytes);

	stats->packets = nsm_stats.flow_stats.rx_packets;
	stats->bytes = nsm_stats.flow_stats.rx_bytes;

	return 1;
}

/*
 * nsm_ppe_get_drop_stat()
 *	Get ppe drop stats by service id
 */
int nsm_ppe_get_drop_stat(nsm_ppe_drop_stat_t *stats, uint8_t service_id)
{
	struct ppe_drv_nsm_stats ppe_stat;
	struct nss_dp_hal_nsm_sawf_sc_stats edma_stat;

	if (!ppe_drv_nsm_sawf_sc_stats_read(&ppe_stat, service_id)) {
		pr_err("ppe_drv_nsm_sawf_sc_stats_read failed service_id:%d ", service_id);
		return 0;
	}
	pr_info("PPE Drop Packet:%llu Byte:%llu\n", ppe_stat.sawf_sc_stats.rx_packets, ppe_stat.sawf_sc_stats.rx_bytes);

	if (!nss_dp_nsm_sawf_sc_stats_read(&edma_stat, service_id)) {
		pr_err("nss_dp_nsm_sawf_sc_stats_read failed service_id:%d ", service_id);
		return 0;
	}
	pr_info("EDMA Drop Packet:%llu Byte:%llu\n", edma_stat.rx_packets, edma_stat.rx_bytes);

	/*
	 * Reset counter to prevent roll over
	 */
	if (ppe_stat.sawf_sc_stats.rx_packets < edma_stat.rx_packets) {
		stats->total_packets = 0;
		stats->total_bytes = 0;
	} else {
		stats->total_packets = ppe_stat.sawf_sc_stats.rx_packets - edma_stat.rx_packets;
		stats->total_bytes = ppe_stat.sawf_sc_stats.rx_bytes - edma_stat.rx_bytes;
	}

	return 1;
}

/*
 * nsm_ppe_get_queue_drop_stat()
 *	Get ppe queue drop stats
 *
 * Queue Index represents the number of queues in the PPE
 * Drop_Queue_ID identifies the index/reason for the drop in the array.
 */
int nsm_ppe_get_queue_drop_stat(nsm_ppe_queue_drop_stat_t *stats, uint32_t queue_index, uint32_t drop_queue_id)
{
	struct ppe_drv_nsm_stats ppe_nsm_stats_temp;

	if(!ppe_drv_nsm_queue_stats_update(&ppe_nsm_stats_temp, queue_index, drop_queue_id)) {
			pr_err("ppe_drv_nsm_queue_stats_update failed queue_index:%d drop_queue_id:%d\n", queue_index, drop_queue_id);
		return 0;
	}
	pr_info("Queue Drop Packet:%llu Byte:%llu", ppe_nsm_stats_temp.queue_stats.drop_packets, ppe_nsm_stats_temp.queue_stats.drop_bytes);

	/*
	 * This is safe to do since the packets will never be zero
	 */
	stats->queue_packets = ppe_nsm_stats_temp.queue_stats.drop_packets;
	stats->queue_bytes = ppe_nsm_stats_temp.queue_stats.drop_bytes;

	return 1;
}
