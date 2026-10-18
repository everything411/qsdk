/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "edma.h"
#include "edma_debug.h"
#include "edma_debugfs.h"
#include "nss_dp_dev.h"

#define EDMA_DEBUGFS_RINGS_PER_SECTION	6
#define EDMA_DEBUGFS_FIELD_WIDTH	30
#define EDMA_DEBUGFS_RING_COL_WIDTH	11

/*
 * edma_debugfs_ring_usage_dump
 * 	Format to print the EDMA ring utilization (full)
 */
const char *edma_debugfs_ring_usage_dump[EDMA_RING_USAGE_MAX_FULL] = {
	"100_percentage_full",
	"90_to_100_percentage_full",
	"70_to_90_percentage_full",
	"50_to_70_percentage_full",
	"Less_than_50_percentage_full"
};

/*
 * edma_debugfs_ring_usage_rx_fill_dump
 * 	Format to print EDMA Rx fill ring empty
 */
const char *edma_debugfs_ring_usage_rx_fill_dump[EDMA_RING_USAGE_MAX_FULL] = {
	"100_percentage_empty",
	"90_to_100_percentage_empty",
	"70_to_90_percentage_empty",
	"50_to_70_percentage_empty",
	"Less_than_50_percentage_empty"
};

/*
 * edma_txcmpl_err_string
 *	EDMA Tx complete error string.
 */
const char *edma_txcmpl_err_string[EDMA_TX_CMPL_ERR_MAX] = {
	"IP_header_length",
	"TSO",
	"IPv6_data_length",
	"TCP_header",
	"TCP_header_offset",
	"TCP_data_offset",
	"UDP_header",
	"UDP_header_offset",
	"UDP_data_offset",
	"UDPLite_header",
	"UDPLite_header_offset",
	"UDPLite_csum_cov",
	"IP_version",
	"L4_offset_<_L3_offset",
	"L4_offset_oob",
	"L3_offset_oob",
	"Payload_offset_oob",
	"Custom_Chksum_offset_oob",
	"Reserved",
	"Reserved",
	"Reserved",
	"TSO_MSS",
	"TSO_TCP_packet"
};

/*
 * edma_debugfs_print_banner()
 *	API to print the banner for a node
 */
static void edma_debugfs_print_banner(struct seq_file *m, char *node)
{
	uint32_t banner_char_len, i;

	for (i = 0; i < EDMA_STATS_BANNER_MAX_LEN; i++) {
		seq_printf(m, "_");
	}

	banner_char_len = (EDMA_STATS_BANNER_MAX_LEN - (strlen(node) + 2)) / 2;

	seq_printf(m, "\n\n");

	for (i = 0; i < banner_char_len; i++) {
		seq_printf(m, "<");
	}

	seq_printf(m, " %s ", node);

	for (i = 0; i < banner_char_len; i++) {
		seq_printf(m, ">");
	}
	seq_printf(m, "\n");

	for (i = 0; i < EDMA_STATS_BANNER_MAX_LEN; i++) {
		seq_printf(m, "_");
	}

	seq_printf(m, "\n\n");
}

/*
 * edma_debugfs_print_rx_desc_section()
 *	Print RX descriptor ring stats
 */
static void edma_debugfs_print_rx_desc_section(struct seq_file *m,
				   struct edma_rx_desc_stats *stats,
				   uint32_t start_idx,
				   uint32_t count,
				   uint32_t ring_id_offset)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nRX_DESC_RING_STATS (Rings %d-%d):\n",
		   start_idx + ring_id_offset,
		   start_idx + count - 1 + ring_id_offset);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 start_idx + i + ring_id_offset);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].src_port_inval);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval_type");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].src_port_inval_type);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "src_port_inval_netdev");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].src_port_inval_netdev);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "rx_napi_sched");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].rx_napi_sched);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "payload_buf_alloc_failed");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].payload_buf_alloc_failed);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (i = 0; i < count; i++) {
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[start_idx + i].ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_print_rx_fill_section()
 *	Print RX fill ring stats
 */
static void edma_debugfs_print_rx_fill_section(struct seq_file *m,
				   struct edma_rx_fill_stats *stats,
				   uint32_t start_idx,
				   uint32_t count,
				   uint32_t ring_id_offset)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nRX_FILL_RING_STATS (Rings %d-%d):\n",
		   start_idx + ring_id_offset,
		   start_idx + count - 1 + ring_id_offset);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 start_idx + i + ring_id_offset);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "alloc_failed");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].alloc_failed);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "page_alloc_failed");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].page_alloc_failed);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_rx_fill_dump[j]);
		for (i = 0; i < count; i++) {
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[start_idx + i].ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_print_tx_cmpl_section()
 *	Print TX completion ring stats
 */
static void edma_debugfs_print_tx_cmpl_section(struct seq_file *m,
				   struct edma_tx_cmpl_stats *stats,
				   uint32_t start_idx,
				   uint32_t count,
				   uint32_t ring_id_offset)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nTX_CMPL_RING_STATS (Rings %d-%d):\n",
		   start_idx + ring_id_offset,
		   start_idx + count - 1 + ring_id_offset);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field/Errors");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 start_idx + i + ring_id_offset);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "invalid_buffer");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].invalid_buffer);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_TX_CMPL_ERR_MAX; j++) {
		if (strcmp(edma_txcmpl_err_string[j], "Reserved") == 0)
			continue;

		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_txcmpl_err_string[j]);
		for (i = 0; i < count; i++) {
			seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[start_idx + i].errors[j]);
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "desc_with_more_bit");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].desc_with_more_bit);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "no_pending_desc");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].no_pending_desc);
	}
	seq_printf(m, "\n");
}

/*
 * edma_debugfs_print_tx_desc_section()
 *	Print TX descriptor ring stats
 */
static void edma_debugfs_print_tx_desc_section(struct seq_file *m,
				   struct edma_tx_desc_stats *stats,
				   uint32_t start_idx,
				   uint32_t count,
				   uint32_t ring_id_offset)
{
	uint32_t i, j;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];

	seq_printf(m, "\nTX_DESC_RING_STATS (Rings %d-%d):\n",
		   start_idx + ring_id_offset,
		   start_idx + count - 1 + ring_id_offset);

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (i = 0; i < count; i++) {
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 start_idx + i + ring_id_offset);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "no_desc_avail");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].no_desc_avail);
	}
	seq_printf(m, "\n");

	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "tso_max_seg_exceed");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].tso_max_seg_exceed);
	}
	seq_printf(m, "\n");

#ifdef NSS_DP_TX_SMALL_PACKET_WAR
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "tx_pad_fail");
	for (i = 0; i < count; i++) {
		seq_printf(m, " %*llu", EDMA_DEBUGFS_RING_COL_WIDTH,
			   stats[start_idx + i].tx_pad_fail);
	}
	seq_printf(m, "\n");
#endif

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (i = 0; i < count; i++) {
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				   stats[start_idx + i].ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
}

/*
 * edma_debugfs_rx_rings_stats_show()
 *	EDMA debugfs rx rings stats show API
 */
static int edma_debugfs_rx_rings_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_rx_fill_stats *rx_fill_stats;
	struct edma_rx_desc_stats *rx_desc_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_rxfill_ring *rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring;
	struct edma_rx_fill_stats *fill_stats;
	struct edma_rx_desc_stats *desc_stats;
	uint32_t i, ring_idx, rings_in_section;
	unsigned int start;
#ifdef NSS_DP_PPEDS_SUPPORT
	uint32_t j, ppeds_idx, valid_count;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];
	struct edma_ppeds_wifi7_cfg *ppeds_node_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	rx_fill_stats = kzalloc(egc->num_rxfill_rings * sizeof(struct edma_rx_fill_stats),
				 GFP_KERNEL);
	if (!rx_fill_stats) {
		edma_err("Error in allocating the Rx fill stats buffer\n");
		return -ENOMEM;
	}

	rx_desc_stats = kzalloc(egc->num_rxdesc_rings * sizeof(struct edma_rx_desc_stats),
				 GFP_KERNEL);
	if (!rx_desc_stats) {
		edma_err("Error in allocating the Rx descriptor stats buffer\n");
		kfree(rx_fill_stats);
		return -ENOMEM;
	}

	/*
	 * Get stats for Rx fill rings
	 */
	for (i = 0; i < egc->num_rxfill_rings; i++) {
		rxfill_ring = &egc->rxfill_rings[i];
		fill_stats = &rxfill_ring->rx_fill_stats;
		do {
			start = edma_dp_stats_fetch_begin(&fill_stats->syncp);
			rx_fill_stats[i].alloc_failed = fill_stats->alloc_failed;
			rx_fill_stats[i].page_alloc_failed = fill_stats->page_alloc_failed;
			memcpy(&rx_fill_stats[i].ring_stats, &fill_stats->ring_stats,
					sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&fill_stats->syncp, start));
	}

	/*
	 * Get stats for Rx Desc rings
	 */
	for (i = 0; i < edma_gbl_ctx->num_rxdesc_rings; i++) {
		rxdesc_ring = &egc->rxdesc_rings[i];
		desc_stats = &rxdesc_ring->rx_desc_stats;
		do {
			start = edma_dp_stats_fetch_begin(&desc_stats->syncp);
			rx_desc_stats[i].src_port_inval = desc_stats->src_port_inval;
			rx_desc_stats[i].src_port_inval_type = desc_stats->src_port_inval_type;
			rx_desc_stats[i].src_port_inval_netdev = desc_stats->src_port_inval_netdev;
			rx_desc_stats[i].rx_napi_sched = desc_stats->rx_napi_sched;
			rx_desc_stats[i].payload_buf_alloc_failed = desc_stats->payload_buf_alloc_failed;
			memcpy(&rx_desc_stats[i].ring_stats, &desc_stats->ring_stats,
					sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&desc_stats->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_RX_RING_STATS_NODE_NAME);

	for (ring_idx = 0;
	     ring_idx < egc->num_rxdesc_rings;
	     ring_idx += EDMA_DEBUGFS_RINGS_PER_SECTION) {

		rings_in_section = min_t(uint32_t,
					 EDMA_DEBUGFS_RINGS_PER_SECTION,
					 egc->num_rxdesc_rings - ring_idx);

		edma_debugfs_print_rx_desc_section(m, rx_desc_stats, ring_idx,
				     rings_in_section, egc->rxdesc_ring_start);
	}

	for (ring_idx = 0;
	     ring_idx < egc->num_rxfill_rings;
	     ring_idx += EDMA_DEBUGFS_RINGS_PER_SECTION) {

		rings_in_section = min_t(uint32_t,
					 EDMA_DEBUGFS_RINGS_PER_SECTION,
					 egc->num_rxfill_rings - ring_idx);

		edma_debugfs_print_rx_fill_section(m, rx_fill_stats, ring_idx,
				     rings_in_section, egc->rxfill_ring_start);
	}

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_debugfs_print_banner(m, EDMA_RX_RING_PPEDS_STATS_NODE_NAME);

	/* Count valid PPE-DS nodes */
	valid_count = 0;
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		if (drv->ppeds_node_cfg[ppeds_idx].ppeds_db)
			valid_count++;
	}

	if (!valid_count)
		goto ppeds_rx_done;

	seq_printf(m, "\nPPE-DS RX_FILL_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		rxfill_ring = &ppeds_node_cfg->rxfill_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 rxfill_ring->ring_id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_rx_fill_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			rxfill_ring = &ppeds_node_cfg->rxfill_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				rxfill_ring->rx_fill_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nPPE-DS RX_DESC_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		rxdesc_ring = &ppeds_node_cfg->rx_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d",
			 rxdesc_ring->ring_id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			rxdesc_ring = &ppeds_node_cfg->rx_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				rxdesc_ring->rx_desc_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
ppeds_rx_done:
#endif
	kfree(rx_fill_stats);
	kfree(rx_desc_stats);
	return 0;
}

/*
 * edma_debugfs_tx_rings_stats_show()
 *	EDMA debugfs Tx rings stats show API
 */
static int edma_debugfs_tx_rings_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_tx_cmpl_stats *tx_cmpl_stats;
	struct edma_tx_desc_stats *tx_desc_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *txdesc_ring;
	struct edma_txcmpl_ring *txcmpl_ring;
	struct edma_tx_desc_stats *tx_desc_stats_ptr;
	struct edma_tx_cmpl_stats *tx_cmpl_stats_ptr;
	uint32_t i, ring_idx, rings_in_section;
	unsigned int start;
#ifdef NSS_DP_PPEDS_SUPPORT
	uint32_t j, ppeds_idx, valid_count;
	char ring_header[EDMA_DEBUGFS_RING_COL_WIDTH + 1];
	struct edma_ppeds_wifi7_cfg *ppeds_node_cfg;
	struct edma_txdesc_ring *tx_ring;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	tx_cmpl_stats = kzalloc(egc->num_txcmpl_rings * sizeof(struct edma_tx_cmpl_stats), GFP_KERNEL);
	if (!tx_cmpl_stats) {
		edma_err("Error in allocating the Tx complete stats buffer\n");
		return -ENOMEM;
	}

	tx_desc_stats = kzalloc(egc->num_txdesc_rings * sizeof(struct edma_tx_desc_stats), GFP_KERNEL);
	if (!tx_desc_stats) {
		edma_err("Error in allocating the Tx descriptor stats buffer\n");
		kfree(tx_cmpl_stats);
		return -ENOMEM;
	}

	/*
	 * Get stats for Tx desc rings
	 */
	for (i = 0; i < egc->num_txdesc_rings; i++) {
		txdesc_ring = &egc->txdesc_rings[i];
		tx_desc_stats_ptr = &txdesc_ring->tx_desc_stats;
		do {
			start = edma_dp_stats_fetch_begin(&tx_desc_stats_ptr->syncp);
			tx_desc_stats[i].no_desc_avail = tx_desc_stats_ptr->no_desc_avail;
			tx_desc_stats[i].tso_max_seg_exceed = tx_desc_stats_ptr->tso_max_seg_exceed;
#ifdef NSS_DP_TX_SMALL_PACKET_WAR
			tx_desc_stats[i].tx_pad_fail = tx_desc_stats_ptr->tx_pad_fail;
#endif
			memcpy(&tx_desc_stats[i].ring_stats, &tx_desc_stats_ptr->ring_stats,
			       sizeof(struct edma_ring_util_stats));
		} while (edma_dp_stats_fetch_retry(&tx_desc_stats_ptr->syncp, start));
	}

	/*
	 * Get stats for Tx Complete rings
	 */
	for (i = 0; i < egc->num_txcmpl_rings; i++) {
		txcmpl_ring = &egc->txcmpl_rings[i];
		tx_cmpl_stats_ptr = &txcmpl_ring->tx_cmpl_stats;
		do {
			start = edma_dp_stats_fetch_begin(&tx_cmpl_stats_ptr->syncp);
			tx_cmpl_stats[i].invalid_buffer = tx_cmpl_stats_ptr->invalid_buffer;
			memcpy(tx_cmpl_stats[i].errors, tx_cmpl_stats_ptr->errors, sizeof(uint64_t) * EDMA_TX_CMPL_ERR_MAX);
			tx_cmpl_stats[i].desc_with_more_bit = tx_cmpl_stats_ptr->desc_with_more_bit;
			tx_cmpl_stats[i].no_pending_desc = tx_cmpl_stats_ptr->no_pending_desc;
		} while (edma_dp_stats_fetch_retry(&tx_cmpl_stats_ptr->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_TX_RING_STATS_NODE_NAME);

	for (ring_idx = 0;
	     ring_idx < egc->num_txcmpl_rings;
	     ring_idx += EDMA_DEBUGFS_RINGS_PER_SECTION) {

		rings_in_section = min_t(uint32_t,
					 EDMA_DEBUGFS_RINGS_PER_SECTION,
					 egc->num_txcmpl_rings - ring_idx);

		edma_debugfs_print_tx_cmpl_section(m, tx_cmpl_stats, ring_idx,
				     rings_in_section, egc->txcmpl_ring_start);
	}

	for (ring_idx = 0;
	     ring_idx < egc->num_txdesc_rings;
	     ring_idx += EDMA_DEBUGFS_RINGS_PER_SECTION) {

		rings_in_section = min_t(uint32_t,
					 EDMA_DEBUGFS_RINGS_PER_SECTION,
					 egc->num_txdesc_rings - ring_idx);

		edma_debugfs_print_tx_desc_section(m, tx_desc_stats, ring_idx,
				     rings_in_section, egc->txdesc_ring_start);
	}

#ifdef NSS_DP_PPEDS_SUPPORT
	edma_debugfs_print_banner(m, EDMA_TX_RING_PPEDS_STATS_NODE_NAME);

	/* Count valid PPE-DS nodes */
	valid_count = 0;
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		if (drv->ppeds_node_cfg[ppeds_idx].ppeds_db)
			valid_count++;
	}

	if (!valid_count)
		goto ppeds_tx_done;

	seq_printf(m, "\nPPE-DS TX_DESC_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		tx_ring = &ppeds_node_cfg->tx_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", tx_ring->id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			tx_ring = &ppeds_node_cfg->tx_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				tx_ring->tx_desc_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nPPE-DS TX_CMPL_RING_STATS:\n");
	seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH, "Field");
	for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
		ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
		if (!ppeds_node)
			continue;
		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		txcmpl_ring = &ppeds_node_cfg->txcmpl_ring;
		snprintf(ring_header, sizeof(ring_header), "Ring_%d", txcmpl_ring->id);
		seq_printf(m, " %*s", EDMA_DEBUGFS_RING_COL_WIDTH, ring_header);
	}
	seq_printf(m, "\n");

	for (j = 0; j < EDMA_RING_USAGE_MAX_FULL; j++) {
		seq_printf(m, "%-*s", EDMA_DEBUGFS_FIELD_WIDTH,
			   edma_debugfs_ring_usage_dump[j]);
		for (ppeds_idx = 0; ppeds_idx < drv->num_nodes; ppeds_idx++) {
			ppeds_node = drv->ppeds_node_cfg[ppeds_idx].ppeds_db;
			if (!ppeds_node)
				continue;
			ppeds_node_cfg = &ppeds_node->wifi7_cfg;
			txcmpl_ring = &ppeds_node_cfg->txcmpl_ring;
			seq_printf(m, " %*d", EDMA_DEBUGFS_RING_COL_WIDTH,
				txcmpl_ring->tx_cmpl_stats.ring_stats.util[j]);
		}
		seq_printf(m, "\n");
	}
ppeds_tx_done:
#endif

	kfree(tx_cmpl_stats);
	kfree(tx_desc_stats);
	return 0;
}

/*
 * edma_debugfs_misc_stats_show()
 *	EDMA debugfs miscellaneous stats show API
 */
static int edma_debugfs_misc_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_misc_stats *misc_stats, *pcpu_misc_stats;
	uint32_t cpu;
	unsigned int start;

	misc_stats = kzalloc(sizeof(struct edma_misc_stats), GFP_KERNEL);
	if (!misc_stats) {
		edma_err("Error in allocating the miscellaneous stats buffer\n");
		return -ENOMEM;
	}

	/*
	 * Get percpu EDMA miscellaneous stats
	 */
	for_each_possible_cpu(cpu) {
		pcpu_misc_stats = per_cpu_ptr(edma_gbl_ctx->misc_stats, cpu);
		do {
			start = edma_dp_stats_fetch_begin(&pcpu_misc_stats->syncp);
			misc_stats->edma_misc_axi_read_err +=
				pcpu_misc_stats->edma_misc_axi_read_err;
			misc_stats->edma_misc_axi_write_err +=
				pcpu_misc_stats->edma_misc_axi_write_err;
			misc_stats->edma_misc_rx_desc_fifo_full +=
				pcpu_misc_stats->edma_misc_rx_desc_fifo_full;
			misc_stats->edma_misc_rx_buf_size_err +=
				pcpu_misc_stats->edma_misc_rx_buf_size_err;
			misc_stats->edma_misc_tx_sram_full +=
				pcpu_misc_stats->edma_misc_tx_sram_full;
			misc_stats->edma_misc_tx_data_len_err +=
				pcpu_misc_stats->edma_misc_tx_data_len_err;
			misc_stats->edma_misc_tx_timeout +=
				pcpu_misc_stats->edma_misc_tx_timeout;
			misc_stats->edma_misc_tx_cmpl_buf_full +=
				pcpu_misc_stats->edma_misc_tx_cmpl_buf_full;
		} while (edma_dp_stats_fetch_retry(&pcpu_misc_stats->syncp, start));
	}

	edma_debugfs_print_banner(m, EDMA_MISC_STATS_NODE_NAME);

	seq_printf(m, "\n#EDMA miscellaneous stats:\n\n");
	seq_printf(m, "\t\t miscellaneous axi read error = %llu\n",
			misc_stats->edma_misc_axi_read_err);
	seq_printf(m, "\t\t miscellaneous axi write error = %llu\n",
			misc_stats->edma_misc_axi_write_err);
	seq_printf(m, "\t\t miscellaneous Rx descriptor fifo full = %llu\n",
			misc_stats->edma_misc_rx_desc_fifo_full);
	seq_printf(m, "\t\t miscellaneous Rx buffer size error = %llu\n",
			misc_stats->edma_misc_rx_buf_size_err);
	seq_printf(m, "\t\t miscellaneous Tx SRAM full = %llu\n",
			misc_stats->edma_misc_tx_sram_full);
	seq_printf(m, "\t\t miscellaneous Tx data length error = %llu\n",
			misc_stats->edma_misc_tx_data_len_err);
	seq_printf(m, "\t\t miscellaneous Tx timeout = %llu\n",
			misc_stats->edma_misc_tx_timeout);
	seq_printf(m, "\t\t miscellaneous Tx completion buffer full = %llu\n",
			misc_stats->edma_misc_tx_cmpl_buf_full);

	kfree(misc_stats);
	return 0;
}

/*
 * edma_debugfs_clear_ring_stats()
 *      EDMA debugfs clearing the ring stats
 */
static int edma_debugfs_clear_ring_stats(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t i;
#ifdef NSS_DP_PPEDS_SUPPORT
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds *ppeds_node;
#endif

	for (i = 0; i < egc->num_rxfill_rings; i++) {
		memset(&egc->rxfill_rings[i].rx_fill_stats, 0, sizeof(struct edma_rx_fill_stats));
	}

	for (i = 0; i < edma_gbl_ctx->num_rxdesc_rings; i++) {
		memset(&egc->rxdesc_rings[i].rx_desc_stats, 0, sizeof(struct edma_rx_desc_stats));
	}


	for (i = 0; i < egc->num_txdesc_rings; i++) {
		memset(&egc->txdesc_rings[i].tx_desc_stats, 0, sizeof(struct edma_tx_desc_stats));
	}

#ifdef NSS_DP_PPEDS_SUPPORT
	for (i = 0; i < drv->num_nodes; i++) {
		struct edma_ppeds_wifi7_cfg *ppeds_node_cfg;
		ppeds_node = drv->ppeds_node_cfg[i].ppeds_db;
		if (!ppeds_node) {
			continue;
		}

		ppeds_node_cfg = &ppeds_node->wifi7_cfg;
		memset(&ppeds_node_cfg->rxfill_ring.rx_fill_stats, 0, sizeof(struct edma_rx_fill_stats));
		memset(&ppeds_node_cfg->rx_ring.rx_desc_stats, 0, sizeof(struct edma_rx_desc_stats));
		memset(&ppeds_node_cfg->tx_ring.tx_desc_stats, 0, sizeof(struct edma_tx_desc_stats));
		memset(&ppeds_node_cfg->txcmpl_ring.tx_cmpl_stats, 0, sizeof(struct edma_tx_cmpl_stats));
	}
#endif

	seq_printf(m, "Resetting the EDMA Ring stats\n");
	return 0;
}

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * edma_debugfs_loopback_stats_show()
 *	EDMA debugfs loopback stats show API
 */
static int edma_debugfs_loopback_stats_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int i = 0;

	seq_printf(m, "\n#EDMA loopback configuration stats:\n\n");
	seq_printf(m, "\t\t Loopback enabled = %d\n", egc->loopback_en);
	seq_printf(m, "\t\t Loopback ring size = %d\n", egc->loopback_ring_size);
	seq_printf(m, "\t\t Loopback buffer size = %d\n",egc->loopback_buf_size);
	seq_printf(m, "\t\t Number of loopback rings = %d\n", egc->num_loopback_rings);
	seq_printf(m, "\t\t Loopback queue base = %d\n", egc->loopback_queue_base);
	seq_printf(m, "\t\t Number of loopback queues = %d\n", egc->loopback_num_queues);

	for (i = 0; i < egc->num_loopback_rings; i++) {
		seq_printf(m, "\t\t#EDMA loopback ring info: %d\n\n", i);
		seq_printf(m, "\t\t TX descriptor loopback_ring_id = %d\n", egc->txdesc_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t TX completion loopback_ring_id = %d\n", egc->txcmpl_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t RX descriptor loopback_ring_id = %d\n", egc->rxdesc_loopback_ring_id_arr[i]);
		seq_printf(m, "\t\t RX fill loopback_ring_id = %d\n", egc->rxfill_loopback_ring_id_arr[i]);
	}

	return 0;
}

/*
 * edma_debugs_loopback_stats_open()
 *	EDMA debugfs loopback stats open callback API
 */
static int edma_debugs_loopback_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_loopback_stats_show, inode->i_private);
}

/*
 * edma_debugfs_misc_file_ops
 *	File operations for EDMA miscellaneous stats
 */
const struct file_operations edma_debugfs_loopback_file_ops = {
	.open = edma_debugs_loopback_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
#endif

#ifdef NSS_DP_MHT_SW_PORT_MAP
/*
 * edma_debugfs_mht_tx_fcgrp_show()
 *	EDMA debugfs tx ring fcgrp on mht ports show API
 */
static int edma_debugfs_mht_tx_fcgrp_show(struct seq_file *m, void __attribute__((unused))*p)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct net_device *netdev = NULL;
	struct nss_dp_dev *dp_dev = NULL;
	struct edma_txdesc_ring *tx_ring = NULL;
	uint32_t i = 0, sw_port = 0;

	seq_printf(m, "\n#EDMA MHT tx ring fc_group info (port_id : group_id):\n\n");

	for (i = 0; i < EDMA_MAX_PORTS; i++) {
		netdev = egc->netdev_arr[i];

		if (!netdev)
			continue;

		dp_dev = (struct nss_dp_dev *)netdev_priv(netdev);
		if (!dp_dev || !dp_dev->nss_dp_mht_dev)
			continue;

		for (sw_port = 0; sw_port < NSS_DP_HAL_SW_MAX_TX_PORT; sw_port++) {
			/* one port's tx rings maps with same fc_grp_id, print once */
			tx_ring = dp_dev->dp_info.txr_sw_port_map[sw_port][0];
			if (tx_ring)
				seq_printf(m, "\t\t%d:%d", (sw_port + 1), tx_ring->fc_grp_id);
		}

		seq_printf(m, "\n\n#switch_netdev:%s, macid:%d", netdev->name, dp_dev->macid);

		seq_printf(m, "\n\n");
	}

	return 0;
}

/*
 * edma_debugs_mht_tx_fcgrp_open()
 *	EDMA debugfs tx ring fcgrp on mht ports open callback API
 */
static int edma_debugs_mht_tx_fcgrp_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_mht_tx_fcgrp_show, inode->i_private);
}

/*
 * edma_debugfs_mht_tx_fcgrp_file_ops
 *	File operations for EDMA tx ring fcgrp on mht ports
 */
const struct file_operations edma_debugfs_mht_tx_fcgrp_file_ops = {
	.open = edma_debugs_mht_tx_fcgrp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
#endif

/*
 * edma_debugs_rx_rings_stats_open()
 *	EDMA debugfs Rx rings open callback API
 */
static int edma_debugs_rx_rings_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_rx_rings_stats_show, inode->i_private);
}

/*
 * edma_debugfs_rx_rings_file_ops
 *	File operations for EDMA Rx rings stats
 */
const struct file_operations edma_debugfs_rx_rings_file_ops = {
	.open = edma_debugs_rx_rings_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_tx_rings_stats_open()
 *	EDMA debugfs Tx rings open callback API
 */
static int edma_debugs_tx_rings_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_tx_rings_stats_show, inode->i_private);
}

/*
 * edma_debugfs_tx_rings_file_ops
 *	File operations for EDMA Tx rings stats
 */
const struct file_operations edma_debugfs_tx_rings_file_ops = {
	.open = edma_debugs_tx_rings_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugs_misc_stats_open()
 *	EDMA debugfs miscellaneous stats open callback API
 */
static int edma_debugs_misc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_misc_stats_show, inode->i_private);
}

/*
 * edma_debugfs_misc_file_ops
 *	File operations for EDMA miscellaneous stats
 */
const struct file_operations edma_debugfs_misc_file_ops = {
	.open = edma_debugs_misc_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/* edma_debugs_clear_ring_stats()
 * 	EDMA debugfs clear stats open callback API
 */
static int edma_debugs_clear_ring_stats(struct inode *inode, struct file *file)
{
	return single_open(file, edma_debugfs_clear_ring_stats, inode->i_private);
}

/*
 * edma_debugfs_clear_ring_stats_ops
 * 	File operations for clearing ring stats
 */
const struct file_operations edma_debugfs_clear_ring_stats_ops = {
	.open = edma_debugs_clear_ring_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * edma_debugfs_init()
 *	EDMA debugfs init API
 */
int edma_debugfs_init(void)
{
	edma_gbl_ctx->root_dentry = debugfs_create_dir("qca-nss-dp", NULL);
	if (!edma_gbl_ctx->root_dentry) {
		edma_err("Unable to create debugfs qca-nss-dp directory in debugfs\n");
		return -1;
	}

	edma_gbl_ctx->stats_dentry = debugfs_create_dir("stats", edma_gbl_ctx->root_dentry);
	if (!edma_gbl_ctx->stats_dentry) {
		edma_err("Unable to create debugfs stats directory in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("rx_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_rx_rings_file_ops)) {
		edma_err("Unable to create Rx rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("tx_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_tx_rings_file_ops)) {
		edma_err("Unable to create Tx rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("clear_ring_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_clear_ring_stats_ops)) {
		edma_err("Unable to create clear rings statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

	/*
	 * Allocate memory for EDMA miscellaneous stats
	 */
	if (edma_misc_stats_alloc() < 0) {
		edma_err("Unable to allocate miscellaneous percpu stats\n");
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("misc_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_misc_file_ops)) {
		edma_err("Unable to create EDMA miscellaneous statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
	if (!debugfs_create_file("loopback_stats", S_IRUGO, edma_gbl_ctx->stats_dentry,
			NULL, &edma_debugfs_loopback_file_ops)) {
		edma_err("Unable to create EDMA loopback statistics file entry in debugfs\n");
		goto debugfs_dir_failed;
	}

#endif

#ifdef NSS_DP_MHT_SW_PORT_MAP
	if (!debugfs_create_file("mht_tx_fcgrp", S_IRUGO, edma_gbl_ctx->root_dentry,
			NULL, &edma_debugfs_mht_tx_fcgrp_file_ops)) {
		edma_err("Unable to create EDMA tx fcgrp on MHT ports file entry in debugfs\n");
		goto debugfs_dir_failed;
	}
#endif

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(edma_gbl_ctx->root_dentry);
	edma_gbl_ctx->root_dentry = NULL;
	edma_gbl_ctx->stats_dentry = NULL;
	return -1;
}

/*
 * edma_debugfs_exit()
 *	EDMA debugfs exit API
 */
void edma_debugfs_exit(void)
{
	/*
	 * Free EDMA miscellaneous stats memory
	 */
	edma_misc_stats_free();

	if (edma_gbl_ctx->root_dentry) {
		debugfs_remove_recursive(edma_gbl_ctx->root_dentry);
		edma_gbl_ctx->root_dentry = NULL;
		edma_gbl_ctx->stats_dentry = NULL;
	}
}
