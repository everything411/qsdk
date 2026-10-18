/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2026, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/reset.h>
#include "edma.h"
#include "edma_cfg_rx.h"
#include "edma_cfg_rx_loopback.h"
#include "edma_regs.h"
#include "edma_tx.h"
#include "edma_debug.h"
#include "nss_dp_dev.h"

uint32_t edma_cfg_rx_loopback_fc_enable = EDMA_RX_FC_ENABLE;

/*
 * edma_cfg_rx_fill_loopback_ring_cleanup()
 *	Cleanup resources for one RxFill ring
 *
 * API expects ring to be disabled by caller
 */
static void edma_cfg_rx_fill_loopback_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_rxfill_ring *rxfill_ring)
{
	rxfill_ring->desc = NULL;
	rxfill_ring->dma = (dma_addr_t)0;
}

/*
 * edma_cfg_rx_fill_loopback_ring_setup()
 *	Setup resources for one RxFill ring
 */
static int edma_cfg_rx_fill_loopback_ring_setup(struct edma_rxfill_ring *rxfill_ring)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txcmpl_ring *txcmpl_ring = &egc->txcmpl_loopback_rings[rxfill_ring->ring_id];

	/*
	 * Allocate RxFill ring descriptors
	 */
	rxfill_ring->desc = (struct edma_rxfill_desc *)txcmpl_ring->desc;
	rxfill_ring->dma  = txcmpl_ring->dma;
	return 0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_setup()
 *	Setup resources for one RxDesc ring
 */
static int edma_cfg_rx_desc_loopback_ring_setup(struct edma_rxdesc_ring *rxdesc_ring)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txdesc_ring *txdesc_ring = &egc->txdesc_loopback_rings[rxdesc_ring->ring_id];

	/*
	 * Allocate RxDesc ring descriptors
	 */
	rxdesc_ring->pdesc = (struct edma_rxdesc_desc *)txdesc_ring->pdesc;
	rxdesc_ring->pdma = txdesc_ring->pdma;

	/*
	 * Allocate secondary RxDesc ring descriptors
	 */
	rxdesc_ring->sdesc = (struct edma_rxdesc_sec_desc *)txdesc_ring->sdesc;
	rxdesc_ring->sdma = txdesc_ring->sdma;

	return 0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_cleanup()
 *	Cleanup resources for RxDesc ring
 *
 * API expects ring to be disabled by caller
 */
static void edma_cfg_rx_desc_loopback_ring_cleanup(struct edma_gbl_ctx *egc,
				struct edma_rxdesc_ring *rxdesc_ring)
{
	uint16_t prod_idx, cons_idx;

	/*
	 * Get Rxdesc consumer & producer indices
	 */
	cons_idx = rxdesc_ring->cons_idx & EDMA_RXDESC_CONS_IDX_MASK;

	prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id))
					& EDMA_RXDESC_PROD_IDX_MASK;

	/*
	 * Update the consumer index to keep hardware upto date with latest state.
	 */
	edma_reg_write(EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->ring_id), cons_idx);

	/*
	 * Free RXDESC ring descriptors
	 */
	rxdesc_ring->pdesc = NULL;
	rxdesc_ring->pdma = (dma_addr_t)0;

	rxdesc_ring->sdesc = NULL;
	rxdesc_ring->sdma = (dma_addr_t)0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_reset_queue_mapping()
 *	API to reset Rx descriptor rings to PPE queues mapping
 */
static int32_t edma_cfg_rx_desc_loopback_ring_reset_queue_mapping(struct edma_gbl_ctx *egc)
{
	fal_queue_bmp_t queue_bmp = {0};
	int32_t i;

	for (i = 0; i < egc->num_loopback_rings; i++) {
		int index = egc->rxdesc_loopback_ring_id_arr[i];
		if (fal_edma_ring_queue_map_set(EDMA_SWITCH_DEV_ID, index, &queue_bmp) != SW_OK) {
			edma_err("Error in unmapping rxdesc loopback ring %d to PPE queue mapping to"
				" disable its backpressure configuration\n", i);
			return -1;
		}
	}

	return 0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_reset_queue_priority()
 *	API to reset the priority for PPE queues mapped to Rx rings
 */
static int32_t edma_cfg_rx_desc_loopback_ring_reset_queue_priority(struct edma_gbl_ctx *egc)
{
	fal_qos_scheduler_cfg_t qsch;
	unsigned int queue_base = egc->loopback_queue_base;
	unsigned int num_queues = egc->loopback_num_queues;
	uint32_t i, queue_id, port_id;

	for (i = 0; i < num_queues; i++) {
		queue_id = queue_base + i;

		memset(&qsch, 0, sizeof(fal_qos_scheduler_cfg_t));
		if (fal_queue_scheduler_get(EDMA_SWITCH_DEV_ID, queue_id,
					EDMA_PPE_QUEUE_LEVEL, &port_id, &qsch) != SW_OK) {
			edma_err("Error in getting %u queue's priority information\n", queue_id);
			return -1;
		}

		qsch.e_pri = i;
		qsch.c_pri = i;

		if (fal_queue_scheduler_set(EDMA_SWITCH_DEV_ID, queue_id,
					EDMA_PPE_QUEUE_LEVEL, port_id, &qsch) != SW_OK) {
			edma_err("Error in resetting %u queue's priority\n", queue_id);
			return -1;
		}
	}

	return 0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_reset_queue_config()
 *	API to reset the Rx descriptor rings configurations
 */
static int32_t edma_cfg_rx_desc_loopback_ring_reset_queue_config(struct edma_gbl_ctx *egc)
{
	/*
	 * Unmap Rxdesc ring to PPE queue mapping to reset its backpressure configuration
	 */
	if (edma_cfg_rx_desc_loopback_ring_reset_queue_mapping(egc)) {
		edma_err("Error in resetting Rx desc loopback ring backpressure configurations\n");
		return -1;
	}

	/*
	 * Reset the priority for PPE queues mapped to Rx rings
	 */
	if(edma_cfg_rx_desc_loopback_ring_reset_queue_priority(egc)) {
		return -1;
	}

	return 0;
}

/*
 * edma_cfg_rx_desc_loopback_ring_to_queue_mapping()
 *	API to map Rx descriptor rings to PPE queue for backpressure
 */
static void edma_cfg_rx_desc_loopback_ring_to_queue_mapping(struct edma_gbl_ctx *egc, int index)
{
	sw_error_t ret;
	fal_queue_bmp_t queue_bmp = {0};
	uint32_t word_idx = 0, bit_idx = 0;
	uint32_t local_bmp[EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT] = {0};
	unsigned int queue_base = egc->loopback_queue_base;
	unsigned int num_queues = egc->loopback_num_queues;
	int queue_id = 0, id = 0;

	/*
	 * Rxdesc ring to PPE queue mapping
	 */
	for (id = 0; id  < num_queues; id++) {
		queue_id = queue_base + id;
		word_idx = (queue_id / EDMA_BITS_IN_WORD);
		bit_idx = (queue_id % EDMA_BITS_IN_WORD);
		local_bmp[word_idx] |= 1 << bit_idx;
		edma_debug("Queue_id: %d, word_idx: %d\n", queue_id, word_idx);
	}

	memcpy(queue_bmp.bmp, local_bmp, sizeof(uint32_t) * EDMA_RING_MAPPED_QUEUE_BM_WORD_COUNT);

	ret = fal_edma_ring_queue_map_set(0, egc->rxdesc_loopback_ring_id_arr[index], &queue_bmp);
	if (ret != SW_OK) {
		if (edma_cfg_rx_desc_loopback_ring_reset_queue_mapping(egc)) {
			edma_err("Error in resetting Rx desc loopback ring backpressure configurations\n");
		}
		return;
	}
}

/*
 * edma_cfg_rx_fill_loopback_ring_flow_control()
 *	Configure Rx fill loopback ring ring flow control configuration
 */
static void edma_cfg_rx_fill_loopback_ring_flow_control(struct edma_gbl_ctx *egc, int ring_id, uint32_t threshold_xoff, uint32_t threshold_xon)
{
	uint32_t data;

	data = (threshold_xoff & EDMA_RXFILL_FC_XOFF_THRE_MASK) << EDMA_RXFILL_FC_XOFF_THRE_SHIFT;
	data |= ((threshold_xon & EDMA_RXFILL_FC_XON_THRE_MASK) << EDMA_RXFILL_FC_XON_THRE_SHIFT);
	edma_reg_write(EDMA_REG_RXFILL_FC_THRE(ring_id), data);
}

/*
 * edma_cfg_rx_desc_loopback_ring_flow_control()
 *	Configure Rx descriptor ring flow control configuration
 */
static void edma_cfg_rx_desc_loopback_ring_flow_control(struct edma_gbl_ctx *egc, int ring_id, uint32_t threshold_xoff, uint32_t threshold_xon)
{
	uint32_t data;

	data = (threshold_xoff & EDMA_RXDESC_FC_XOFF_THRE_MASK) << EDMA_RXDESC_FC_XOFF_THRE_SHIFT;
	data |= ((threshold_xon & EDMA_RXDESC_FC_XON_THRE_MASK) << EDMA_RXDESC_FC_XON_THRE_SHIFT);
	edma_reg_write(EDMA_REG_RXDESC_FC_THRE(ring_id), data);
}

/*
 * edma_cfg_rx_desc_loopback_ring_configure()
 *	Configure one RxDesc ring in EDMA HW
 */
static void edma_cfg_rx_desc_loopback_ring_configure(struct edma_rxdesc_ring *rxdesc_ring)
{
	uint32_t data;
	uint32_t paddr, saddr;

	paddr = (uint32_t)(rxdesc_ring->pdma & EDMA_RXDESC_BA_MASK);
	edma_reg_write(EDMA_REG_RXDESC_BA(rxdesc_ring->ring_id), paddr);

	saddr = (uint32_t)(rxdesc_ring->sdma & EDMA_RXDESC_PREHEADER_BA_MASK);
	edma_reg_write(EDMA_REG_RXDESC_PREHEADER_BA(rxdesc_ring->ring_id), saddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
        paddr = (uint32_t)((rxdesc_ring->pdma >> 32) & EDMA_RXDESC_BA_HIGHER_MASK);
        edma_reg_write(EDMA_REG_RXDESC_BA_HIGH(rxdesc_ring->ring_id), paddr);

        saddr = (uint32_t)((rxdesc_ring->sdma >> 32) & EDMA_RXDESC_PREHEADER_BA_HIGHER_MASK);
        edma_reg_write(EDMA_REG_RXDESC_PREHEADER_BA_HIGH(rxdesc_ring->ring_id), saddr);
#endif

	data = rxdesc_ring->count & EDMA_RXDESC_RING_SIZE_MASK;

	/*
         * For SOC's where Rxdesc ring register do not contain PL offset
         * fields, skip writing that data into the Register.
         */
#if !defined(NSS_DP_EDMA_SKIP_PL_OFFSET)
	data |= (EDMA_RXDESC_PL_DEFAULT_VALUE & EDMA_RXDESC_PL_OFFSET_MASK)
		 << EDMA_RXDESC_PL_OFFSET_SHIFT;
#endif

	edma_reg_write(EDMA_REG_RXDESC_RING_SIZE(rxdesc_ring->ring_id), data);
}

/*
 * edma_cfg_rx_fill_loopback_ring_configure()
 *	Configure one RxFill ring in EDMA HW
 */
static void edma_cfg_rx_fill_loopback_ring_configure(struct edma_rxfill_ring *rxfill_ring)
{
	uint32_t ring_sz;
	uint32_t paddr;

	paddr = (uint32_t)(rxfill_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_RXFILL_BA(rxfill_ring->ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
        paddr = (uint32_t)((rxfill_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
        edma_reg_write(EDMA_REG_RXFILL_BA_HIGH(rxfill_ring->ring_id), paddr);
#endif

	ring_sz = rxfill_ring->count & EDMA_RXFILL_RING_SIZE_MASK;
	edma_reg_write(EDMA_RXFILL_RING_SIZE(rxfill_ring->ring_id), ring_sz);

	/*
	 * Alloc Rx buffers
	 */
	edma_rx_alloc_buffer_loopback(rxfill_ring, rxfill_ring->count);
}

/*
 * edma_cfg_rx_loopback_qid2rx_desc_mapping()
 *	Configure PPE queue id to Rx ring mapping
 */
static void edma_cfg_rx_loopback_qid2rx_desc_mapping(struct edma_gbl_ctx *egc)
{
	uint32_t desc_index;
	uint32_t reg_index, data;
	uint16_t start = 0;
        uint16_t end = 0;
	int i = 0, j = 0;

	/*
	 * Set PPE QID to EDMA Rx ring mapping.
	 * Each entry can hold mapping for 4 PPE queues and
	 * entry size is 4 bytes.
	 */
	for (j = 0; j < egc->num_loopback_rings; j++) {
		start = egc->loopback_queue_base;
		end = start + egc->loopback_num_queues;
		desc_index = (egc->rxdesc_loopback_ring_id_arr[j] & EDMA_RX_RING_ID_MASK);

		for (i = start; i <= end; i += EDMA_QID2RID_NUM_PER_REG) {
			reg_index = i/EDMA_QID2RID_NUM_PER_REG;
			data = EDMA_RX_RING_ID_QUEUE0_SET(desc_index) |
				EDMA_RX_RING_ID_QUEUE1_SET(desc_index) |
				EDMA_RX_RING_ID_QUEUE2_SET(desc_index) |
				EDMA_RX_RING_ID_QUEUE3_SET(desc_index);

			edma_reg_write(EDMA_QID2RID_TABLE_MEM(reg_index), data);

			edma_debug("Configure QID2RID(%d) reg:0x%x to 0x%x\n",
				i, EDMA_QID2RID_TABLE_MEM(reg_index), data);
		}
	}
}

/*
 * edma_cfg_rx_loopback_rings_to_rx_fill_mapping()
 *	Configure Rx rings to Rx fill mapping
 */
static void edma_cfg_rx_loopback_rings_to_rx_fill_mapping(struct edma_gbl_ctx *egc)
{
	struct edma_rxdesc_ring *rxdesc_ring;
	uint32_t data, reg, ring_id;
	int i = 0;

	for (i = 0; i < egc->num_loopback_rings; i++) {
		rxdesc_ring = &egc->rxdesc_loopback_rings[i];
		ring_id = rxdesc_ring->ring_id;

		if ((ring_id >= 0) && (ring_id <= 9)) {
			reg = EDMA_REG_RXDESC2FILL_MAP_0;
		} else if ((ring_id >= 10) && (ring_id <= 19)) {
			reg = EDMA_REG_RXDESC2FILL_MAP_1;
		} else {
			reg = EDMA_REG_RXDESC2FILL_MAP_2;
		}

		edma_debug("Configure RXDESC loopback ring:%u to use RXFILL loopback ring:%u\n", ring_id, egc->rxfill_loopback_ring_id_arr[i]);

		/*
		 * Set the Rx fill ring number in the
		 * mapping register.
		 */
		data = edma_reg_read(reg);
		data |= (egc->rxfill_loopback_ring_id_arr[i] & EDMA_RXDESC2FILL_MAP_RXDESC_MASK) << ((ring_id % 10) * 3);
		edma_reg_write(reg, data);
	}

	edma_debug("EDMA_REG_RXDESC2FILL_MAP_0: 0x%x\n", edma_reg_read(EDMA_REG_RXDESC2FILL_MAP_0));
	edma_debug("EDMA_REG_RXDESC2FILL_MAP_1: 0x%x\n", edma_reg_read(EDMA_REG_RXDESC2FILL_MAP_1));
	edma_debug("EDMA_REG_RXDESC2FILL_MAP_2: 0x%x\n", edma_reg_read(EDMA_REG_RXDESC2FILL_MAP_2));
}

/*
 * edma_cfg_rx_loopback_rings_enable()
 *	API to enable Rx and Rxfill rings
 */
void edma_cfg_rx_loopback_rings_enable(struct edma_gbl_ctx *egc)
{
	uint32_t data;
	struct edma_rxdesc_ring *rxdesc_ring;
	struct edma_rxfill_ring *rxfill_ring;
	int i = 0, j = 0;

	/*
	 * Enable Rx rings
	 */
	for (j = 0; j < egc->num_loopback_rings; j++) {
		rxdesc_ring = &egc->rxdesc_loopback_rings[j];
		i = rxdesc_ring->ring_id;
		data = edma_reg_read(EDMA_REG_RXDESC_CTRL(i));
		data |= EDMA_RXDESC_RX_EN;
		edma_reg_write(EDMA_REG_RXDESC_CTRL(i), data);

		rxfill_ring = &egc->rxfill_loopback_rings[j];
		i = rxfill_ring->ring_id;
		data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(i));
		data |= EDMA_RXFILL_RING_EN;
		edma_reg_write(EDMA_REG_RXFILL_RING_EN(i), data);
	}
}

/*
 * edma_cfg_rx_loopback_rings_disable()
 *	API to disable Rx and Rxfill rings
 */
void edma_cfg_rx_loopback_rings_disable(struct edma_gbl_ctx *egc)
{
	struct edma_rxdesc_ring *rxdesc_ring;
	struct edma_rxfill_ring *rxfill_ring;
	uint32_t data;
	int i;

	/*
	 * Disable Rx rings
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		rxdesc_ring = &egc->rxdesc_loopback_rings[i];
		data = edma_reg_read(EDMA_REG_RXDESC_CTRL(rxdesc_ring->ring_id));
		data &= ~EDMA_RXDESC_RX_EN;
		edma_reg_write(EDMA_REG_RXDESC_CTRL(rxdesc_ring->ring_id), data);

		/*
		 * Disable RxFill Rings
		 */
		rxfill_ring = &egc->rxfill_loopback_rings[i];
		data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(rxfill_ring->ring_id));
		data &= ~EDMA_RXFILL_RING_EN;
		edma_reg_write(EDMA_REG_RXFILL_RING_EN(rxfill_ring->ring_id), data);
	}
}

/*
 * edma_cfg_rx_loopback_mapping()
 *	API to setup RX ring mapping
 */
void edma_cfg_rx_loopback_mapping(struct edma_gbl_ctx *egc)
{
	uint32_t queue_base = 0, queue_id = 0;
	uint32_t word_idx, bit_idx;
	uint32_t num_queues;
	int i = 0;

	/*
	 * MAP Rx descriptor ring to PPE queues.
	 */
	queue_base = egc->loopback_queue_base;
	num_queues = egc->loopback_num_queues;

	for (i = 0; i < num_queues; i++) {
		queue_id = queue_base + i;

		word_idx = (queue_id / EDMA_BITS_IN_WORD);
		bit_idx = (queue_id % EDMA_BITS_IN_WORD);
		egc->rxdesc_loopback_ring_to_queue_bm[word_idx] = 1 << bit_idx;
	}

	/*
	 * Reset Rx descriptor ring mapped queue's configurations
	 */
	edma_cfg_rx_desc_loopback_ring_reset_queue_config(egc);
	edma_cfg_rx_loopback_qid2rx_desc_mapping(egc);
	edma_cfg_rx_loopback_rings_to_rx_fill_mapping(egc);
}

/*
 * edma_cfg_rx_loopback_rings_setup()
 *	Allocate/setup resources for EDMA rings
 */
static int edma_cfg_rx_loopback_rings_setup(struct edma_gbl_ctx *egc)
{
	struct edma_rxdesc_ring *rxdesc_ring = NULL;
	struct edma_rxfill_ring *rxfill_ring = NULL;
	int32_t ring_idx, ret;
	int i = 0;

	/*
	 * Allocate Rx fill ring descriptors
	 */
	for(i = 0; i < egc->num_loopback_rings; i++) {
		rxfill_ring = &egc->rxfill_loopback_rings[i];
		rxfill_ring->count = egc->loopback_ring_size;
		rxfill_ring->ring_id = egc->rxfill_loopback_ring_id_arr[i];
		rxfill_ring->alloc_size = egc->loopback_buf_size;
		rxfill_ring->buf_len = egc->loopback_buf_size;

		ret = edma_cfg_rx_fill_loopback_ring_setup(rxfill_ring);
		if (ret != 0) {
			edma_err("Error in setting up %d rxfill ring. ret: %d", rxfill_ring->ring_id, ret);
			return -ENOMEM;
		}

		/*
		 * Allocate RxDesc ring descriptors
		 */
		rxdesc_ring = &egc->rxdesc_loopback_rings[i];
		rxdesc_ring->count = egc->loopback_ring_size;
		rxdesc_ring->ring_id = egc->rxdesc_loopback_ring_id_arr[i];
		ring_idx = rxdesc_ring->ring_id;

		/*
		 * Create a mapping between RX Desc ring and Rx fill ring.
		 * Number of fill rings are lesser than the descriptor rings
		 * Share the fill rings across descriptor rings.
		 */
		rxdesc_ring->rxfill = &egc->rxfill_loopback_rings[i];

		ret = edma_cfg_rx_desc_loopback_ring_setup(rxdesc_ring);
		if (ret != 0) {
			edma_err("Error in setting up %d rxdesc ring. ret: %d", rxdesc_ring->ring_id, ret);
			goto rxdesc_mem_alloc_fail;
		}
	}

	edma_info("Rx descriptor count for Rx desc and Rx fill rings : %d\n", EDMA_RX_RING_SIZE);
	return 0;

rxdesc_mem_alloc_fail:
	for(i = 0; i < egc->num_loopback_rings; i++) {
		edma_cfg_rx_fill_loopback_ring_cleanup(egc, &egc->rxfill_loopback_rings[i]);
	}

	return -ENOMEM;
}

/*
 * edma_cfg_rx_loopback_rings_alloc()
 *	Allocate EDMA Rx rings
 */
int32_t edma_cfg_rx_loopback_rings_alloc(struct edma_gbl_ctx *egc)
{
	egc->rxfill_loopback_rings = kzalloc(sizeof(struct edma_rxfill_ring) * egc->num_loopback_rings, GFP_KERNEL);
	if (!egc->rxfill_loopback_rings) {
		edma_warn("Error in allocating rxfill ring\n");
		return -ENOMEM;
	}

	nss_dp_minidump_log(egc->rxfill_loopback_rings, sizeof(struct edma_rxfill_ring) * egc->num_loopback_rings,
						"edma_rxfill_ring");

	egc->rxdesc_loopback_rings = kzalloc(sizeof(struct edma_rxdesc_ring) * egc->num_loopback_rings, GFP_KERNEL);
	if (!egc->rxdesc_loopback_rings) {
		edma_warn("Error in allocating rxdesc ring\n");
		goto rxdesc_ring_alloc_fail;
	}

	nss_dp_minidump_log(egc->rxdesc_loopback_rings, sizeof(struct edma_rxdesc_ring) * egc->num_loopback_rings,
						"edma_rxdesc_ring");

	if (edma_cfg_rx_loopback_rings_setup(egc)) {
		edma_warn("Error in setting up rx rings\n");
		goto rx_rings_setup_fail;
	}

	/*
	 * Reset Rx descriptor ring mapped queue's configurations
	 */
	if (edma_cfg_rx_desc_loopback_ring_reset_queue_config(egc)) {
		edma_err("Error in resetting the Rx descriptor rings configurations\n");
		edma_cfg_rx_loopback_rings_cleanup(egc);
		return -EINVAL;
	}

	return 0;

rx_rings_setup_fail:
	nss_dp_minidump_free(egc->rxdesc_loopback_rings, "edma_rxdesc_ring");
	kfree(egc->rxdesc_loopback_rings);
	egc->rxdesc_loopback_rings = NULL;
rxdesc_ring_alloc_fail:
	nss_dp_minidump_free(egc->rxfill_loopback_rings, "edma_rxfill_ring");
	kfree(egc->rxfill_loopback_rings);
	egc->rxfill_loopback_rings = NULL;
	return -ENOMEM;
}

/*
 * edma_cfg_rx_loopback_ring_cleanup()
 *	Cleanup EDMA rings
 */
void edma_cfg_rx_loopback_rings_cleanup(struct edma_gbl_ctx *egc)
{
	struct edma_rxfill_ring *rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring;
	int i = 0;

	/*
	 * Free Rx fill ring descriptors
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		rxfill_ring = &egc->rxfill_loopback_rings[i];
		edma_cfg_rx_fill_loopback_ring_cleanup(egc, rxfill_ring);
	}

	/*
	 * Free Rx ring descriptors
	 */
	for (i = 0; i < egc->num_loopback_rings; i++) {
		rxdesc_ring = &egc->rxdesc_loopback_rings[i];
		edma_cfg_rx_desc_loopback_ring_cleanup(egc, rxdesc_ring);
	}

	nss_dp_minidump_free(egc->rxfill_loopback_rings, "edma_rxfill_ring");
	nss_dp_minidump_free(egc->rxdesc_loopback_rings, "edma_rxdesc_ring");

	kfree(egc->rxfill_loopback_rings);
	kfree(egc->rxdesc_loopback_rings);
	egc->rxfill_loopback_rings = NULL;
	egc->rxdesc_loopback_rings = NULL;
}

/*
 * edma_cfg_rx_loopback_rings()
 *	Configure EDMA Receive loopback rings
 */
void edma_cfg_rx_loopback_rings(struct edma_gbl_ctx *egc)
{
	struct edma_rxfill_ring *rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring;
	int i = 0;

	for (i = 0; i < egc->num_loopback_rings; i++) {
		rxfill_ring = &egc->rxfill_loopback_rings[i];
		rxdesc_ring = &egc->rxdesc_loopback_rings[i];

		edma_cfg_rx_fill_loopback_ring_configure(rxfill_ring);
		edma_cfg_rx_desc_loopback_ring_configure(rxdesc_ring);
	}

	if (edma_cfg_rx_loopback_fc_enable) {
		/*
		 * Validate flow control X-OFF and X-ON configurations
		 */
		if ((nss_dp_rx_fc_xoff < EDMA_RX_FC_XOFF_THRE_MIN) ||
				(nss_dp_rx_fc_xoff > EDMA_RX_RING_SIZE)) {
			edma_err("Incorrect Rx Xoff flow control value: %d. Setting\n"
					" it to default value: %d", nss_dp_rx_fc_xoff,
					NSS_DP_RX_FC_XOFF_DEF);
			nss_dp_rx_fc_xoff = NSS_DP_RX_FC_XOFF_DEF;
		}

		if ((nss_dp_rx_fc_xon < EDMA_RX_FC_XON_THRE_MIN) ||
				(nss_dp_rx_fc_xon > EDMA_RX_RING_SIZE) ||
				(nss_dp_rx_fc_xon < nss_dp_rx_fc_xoff)) {
			edma_err("Incorrect Rx Xon flow control value: %d. Setting\n"
					" it to default value: %d", nss_dp_rx_fc_xon,
					NSS_DP_RX_FC_XON_DEF);
			nss_dp_rx_fc_xon = NSS_DP_RX_FC_XON_DEF;
		}

		/*
		 * Configure Rx flow control configurations
		 */
		for (i = 0; i < egc->num_loopback_rings; i++) {
			rxfill_ring = &egc->rxfill_loopback_rings[i];
			rxdesc_ring = &egc->rxdesc_loopback_rings[i];
			edma_cfg_rx_desc_loopback_ring_flow_control(egc, rxfill_ring->ring_id, nss_dp_rx_fc_xoff, nss_dp_rx_fc_xon);
			edma_cfg_rx_fill_loopback_ring_flow_control(egc, rxdesc_ring->ring_id, nss_dp_rx_fc_xoff, nss_dp_rx_fc_xon);
			edma_cfg_rx_desc_loopback_ring_to_queue_mapping(egc, i);
		}
	}
}
