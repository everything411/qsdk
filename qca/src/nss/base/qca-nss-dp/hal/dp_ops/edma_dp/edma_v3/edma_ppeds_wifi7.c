/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <asm/cacheflush.h>
#include <linux/irq.h>
#include "edma.h"
#include "edma_regs.h"
#include "edma_debug.h"
#include "nss_dp_dev.h"
#include "edma_cfg_rx.h"
#include "edma_cfg_tx.h"
#include <ppe_drv.h>

#ifdef NSS_DP_PON_SUPPORT
#include <nss_dp_gem.h>
#endif

/*
 * edma_ppeds_tx_complete()
 *	PPE-DS EDMA Tx complete processing API
 */
static uint32_t edma_ppeds_tx_complete(uint32_t work_to_do, struct edma_txcmpl_ring *txcmpl_ring)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = container_of(txcmpl_ring, struct edma_ppeds_node_wifi7, txcmpl_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi7_cfg, struct edma_ppeds, wifi7_cfg);
	nss_dp_ppeds_handle_t *ppeds_handle = &ppeds_node->ppeds_handle;
	uint32_t cons_idx, prod_idx, data;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_txcmpl_desc *txcmpl;
	uint32_t avail_in_ring;
	uint16_t chnk_of_reap;
	int16_t avail_cnt, idx;
	uint16_t count;

	cons_idx = txcmpl_ring->cons_idx;

	/*
	 * Get TXCMPL ring producer index
	 */
	data = edma_reg_read(EDMA_REG_TXCMPL_PROD_IDX(txcmpl_ring->id));
	prod_idx = data & EDMA_TXCMPL_PROD_IDX_MASK;

	avail_in_ring = EDMA_DESC_AVAIL_COUNT(prod_idx, cons_idx, txcmpl_ring->count);
	if (!avail_in_ring) {
		return 0;
	}

	if (unlikely(egc->enable_ring_util_stats)) {
		edma_update_ring_stats(avail_in_ring, wifi7_cfg->txcmpl_ring.count,
				       &wifi7_cfg->txcmpl_ring.tx_cmpl_stats.ring_stats);
	}

	avail_in_ring = min(avail_in_ring, work_to_do);
	count = avail_in_ring;

	do {
		chnk_of_reap = min(avail_in_ring, ppeds_handle->wifi7_hdl.eth_txcomp_chnk_of_reap);
		avail_cnt = idx = chnk_of_reap;

		txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);

		while (likely(idx--)) {
			ppeds_handle->wifi7_hdl.tx_cmpl_arr[avail_cnt - idx - 1].cookie = EDMA_TXCMPL_OPAQUE_GET(txcmpl);

			cons_idx = ((cons_idx + 1) & (txcmpl_ring->count - 1));
			txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);
		}

		/* Update Tx comp consumer index. */
		txcmpl_ring->cons_idx = cons_idx;
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id), cons_idx);

		/* Complete/replenish all the buffers. */
		ppeds_node->ops->tx_cmpl(ppeds_handle, avail_cnt);

		avail_in_ring -= chnk_of_reap;
	} while(avail_in_ring > 0);

	return count;
}

/*
 * edma_ppeds_txcomp_napi_poll()
 *	PPE-DS EDMA TX NAPI handler
 */
static int edma_ppeds_txcomp_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_txcmpl_ring *txcmpl_ring = (struct edma_txcmpl_ring *)napi;
	struct edma_ppeds_node_wifi7 *wifi7_cfg = container_of(txcmpl_ring, struct edma_ppeds_node_wifi7, txcmpl_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi7_cfg, struct edma_ppeds, wifi7_cfg);
	uint32_t txcmpl_intr_status;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t reg_data;
	int work_done = 0;

	set_bit(EDMA_PPEDS_TXCOMP_NAPI_BIT, &ppeds_node->service_running);
	do {
		work_done += edma_ppeds_tx_complete(budget - work_done, txcmpl_ring);
		if (work_done >= budget) {
			return work_done;
		}

		reg_data = edma_reg_read(EDMA_REG_TX_INT_STAT(txcmpl_ring->id));
		txcmpl_intr_status = reg_data & EDMA_TXCMPL_RING_INT_STATUS_MASK;
	} while (txcmpl_intr_status);

	/*
	 * No more packets to process. Finish NAPI processing.
	 */
	napi_complete(napi);

	/*
	 * Set TXCMPL ring interrupt mask
	 */
	clear_bit(EDMA_PPEDS_TXCOMP_NAPI_BIT, &ppeds_node->service_running);
	if (!ppeds_node->umac_reset_inprogress) {
		edma_reg_write(EDMA_REG_TX_INT_MASK(txcmpl_ring->id),
				egc->txcmpl_intr_mask);
	} else {
		if ((!ppeds_node->service_running) &&
			 (ppeds_node->ops->notify_napi_done)) {
			ppeds_node->ops->notify_napi_done(&ppeds_node->ppeds_handle);
		}
	}

	return work_done;
}

/*
 * edma_ppeds_rx_alloc_buffer()
 *	Alloc Rx buffers for RxFill ring
 */
static void edma_ppeds_rx_alloc_buffer(struct edma_rxfill_ring *rxfill_ring, int alloc_count, struct nss_dp_ppeds_rx_fill_elem *rx_fill_arr,
		uint32_t headroom)
{
	uint32_t ring_size_mask = rxfill_ring->count - 1;
	uint32_t rx_alloc_size = rxfill_ring->alloc_size;
	uint16_t prod_idx, start_idx;
	struct edma_rxfill_desc *rxfill_desc;
	uint16_t num_alloc = 0;

	/*
	 * Get RXFILL ring producer index
	 */
	prod_idx = rxfill_ring->prod_idx;
	start_idx = prod_idx;

	while (likely(alloc_count--)) {
		/*
		 * Get RXFILL descriptor
		 */
		rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, prod_idx);

		EDMA_RXFILL_BUFFER_ADDR_SET(rxfill_desc, rx_fill_arr[num_alloc].buff_addr);

#if defined(NSS_DP_HIGHMEM_SUPP)
		EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc, rx_fill_arr[num_alloc].buff_addr);
#endif

		rxfill_desc->word2 = rx_fill_arr[num_alloc].opaque_lo;
		rxfill_desc->word3 = rx_fill_arr[num_alloc].opaque_hi;
		EDMA_RXFILL_PACKET_LEN_SET(rxfill_desc,
			((uint32_t)(rx_alloc_size - headroom)
			& EDMA_RXFILL_BUF_SIZE_MASK));

		prod_idx = (prod_idx + 1) & ring_size_mask;
		EDMA_RXFILL_ENDIAN_SET(rxfill_desc);
		num_alloc++;
	}

	if (likely(num_alloc)) {
		edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id),
								prod_idx);
		rxfill_ring->prod_idx = prod_idx;
	}
}

/*
 * edma_ppeds_rxfill_napi_poll()
 *	EDMA RXFill NAPI handler
 */
static int edma_ppeds_rxfill_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxfill_ring *rxfill_ring = (struct edma_rxfill_ring *)napi;
	struct edma_ppeds_node_wifi7 *wifi7_cfg = container_of(rxfill_ring, struct edma_ppeds_node_wifi7, rxfill_ring);
	uint32_t headroom = EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
	struct edma_ppeds *ppeds_node = container_of(wifi7_cfg, struct edma_ppeds, wifi7_cfg);
	uint32_t alloc_size = rxfill_ring->alloc_size;
	uint32_t cons_idx, work_to_do;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t num_avail = 0;

	cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) &
				EDMA_RXFILL_CONS_IDX_MASK;
	work_to_do = (cons_idx - rxfill_ring->prod_idx + rxfill_ring->count - 1) & (rxfill_ring->count - 1);

	if (unlikely(egc->enable_ring_util_stats)) {
		edma_update_ring_stats(work_to_do, rxfill_ring->count,
				       &rxfill_ring->rx_fill_stats.ring_stats);
	}

	if (work_to_do > budget) {
		work_to_do = budget;
	}

	if (unlikely(!work_to_do)) {
		goto napi_complete;
	}

	num_avail = ppeds_node->ops->rx_fill(&ppeds_node->ppeds_handle, work_to_do,
						alloc_size, headroom);
	if (likely(num_avail))
		edma_ppeds_rx_alloc_buffer(rxfill_ring, num_avail,
				ppeds_node->ppeds_handle.wifi7_hdl.rx_fill_arr, headroom);

	edma_reg_read(EDMA_REG_RXFILL_INT_STAT(rxfill_ring->ring_id));

	if (work_to_do < budget) {
		goto napi_complete;
	}

	return budget;

napi_complete:
	napi_complete(napi);
	if (!ppeds_node->umac_reset_inprogress) {
		edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id),
				EDMA_RXFILL_INT_MASK);
	}
	return 0;
}

/*
 * edma_ppeds_rx_napi_poll()
 *	EDMA RX NAPI handler
 */
static int edma_ppeds_rx_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)napi;
	struct edma_ppeds_node_wifi7 *wifi7_cfg = container_of(rxdesc_ring, struct edma_ppeds_node_wifi7, rx_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi7_cfg, struct edma_ppeds, wifi7_cfg);
	uint32_t status;
	uint16_t prod_idx;

	/*
	 * Read EDMA Prod Idx.
	 */
	prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id)) &
		EDMA_RXDESC_PROD_IDX_MASK;

	/*
	 * Update Prod idx to DS interface
	 */
	ppeds_node->ops->rx(&ppeds_node->ppeds_handle, prod_idx);


	/*
	 * Clear on read
	 */
	status = EDMA_RXDESC_RING_INT_STATUS_MASK &
		edma_reg_read(EDMA_REG_RXDESC_INT_STAT(rxdesc_ring->ring_id));

	napi_complete(napi);

	/*
	 * Set RXDESC ring interrupt mask
	 */
	if (!ppeds_node->umac_reset_inprogress) {
		edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
				EDMA_RXDESC_INT_MASK_PKT_INT);
	}

	return 0;
}

/*
 * edma_ppeds_set_rx_mapping()
 *	API for PPE-DS EDMA Rx ring mapping
 */
static void edma_ppeds_set_rx_mapping(uint32_t rxfill_ring_id, uint32_t rx_ring_id, uint32_t ppe_qid,
		uint32_t num_ppe_queues)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	/*
	 * Setup RxFill to Rx mapping.
	 */
	edma_cfg_rxdesc_to_rxfill_mapping(egc, rxfill_ring_id, rx_ring_id);

	/*
	 * Setup PPE Queue to Rx Ring mapping.
	 */
	edma_cfg_rx_qid_to_rid_mapping(ppe_qid, ppe_qid + num_ppe_queues, rx_ring_id);

	edma_debug("PPE Queue %d mapped to EDMA ring %d", ppe_qid, rx_ring_id);
}


/*
 * edma_ppeds_cfg_tx()
 *	API to configure PPE-DS EDMA Tx ring
 */
static void edma_ppeds_cfg_tx(struct edma_ppeds *ppeds_node)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_txdesc_ring *txdesc_ring = &wifi7_cfg->tx_ring;
	struct edma_txcmpl_ring *txcmpl_ring = &wifi7_cfg->txcmpl_ring;
	uint32_t paddr, saddr;
	uint32_t tx_mod_timer;

	/*
	 * Configure TXDESC ring
	 */
	paddr = (uint32_t)(txdesc_ring->pdma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA(txdesc_ring->id), paddr);

	saddr = (uint32_t)(txdesc_ring->sdma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA2(txdesc_ring->id), saddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((txdesc_ring->pdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA_HIGH(txdesc_ring->id), paddr);

	saddr = (uint32_t)((txdesc_ring->sdma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXDESC_BA2_HIGH(txdesc_ring->id), saddr);
#endif

	edma_reg_write(EDMA_REG_TXDESC_RING_SIZE(txdesc_ring->id),
			(uint32_t)(txdesc_ring->count &
				EDMA_TXDESC_RING_SIZE_MASK));

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id), EDMA_TX_INITIAL_PROD_IDX);

	/*
	 * Configure TxCmpl ring base address
	 */
	paddr = (uint32_t)(txcmpl_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA(txcmpl_ring->id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((txcmpl_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA_HIGH(txcmpl_ring->id), paddr);
#endif

	edma_reg_write(EDMA_REG_TXCMPL_RING_SIZE(txcmpl_ring->id),
			(uint32_t)(txcmpl_ring->count & EDMA_TXDESC_RING_SIZE_MASK));

	/*
	 * Set TxCmpl ret mode to opaque
	 */
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id),
			EDMA_TXCMPL_RETMODE_OPAQUE);

	/*
	 * Configure the default timer mitigation value
	 */
	tx_mod_timer = (EDMA_TX_MOD_TIMER & EDMA_TX_MOD_TIMER_INIT_MASK)
			<< EDMA_TX_MOD_TIMER_INIT_SHIFT;
	edma_reg_write(EDMA_REG_TX_MOD_TIMER(txcmpl_ring->id),
				tx_mod_timer);

	txcmpl_ring->cons_idx = edma_reg_read(EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id));

	edma_reg_write(EDMA_REG_TX_INT_CTRL(txcmpl_ring->id), EDMA_TX_NE_INT_EN);
}

/*
 * edma_ppeds_rx_desc_ring_flow_control()
 *	PPE-DS Rx descriptor ring flow control configuration API
 */
static void edma_ppeds_rx_desc_ring_flow_control(struct edma_rxdesc_ring *rxdesc_ring)
{
	uint32_t data = 0;

	data = (EDMA_PPEDS_RX_FC_XOFF_DEF & EDMA_RXDESC_FC_XOFF_THRE_MASK) <<
			 EDMA_RXDESC_FC_XOFF_THRE_SHIFT;
	data |= ((EDMA_PPEDS_RX_FC_XON_DEF & EDMA_RXDESC_FC_XON_THRE_MASK) <<
			 EDMA_RXDESC_FC_XON_THRE_SHIFT);

	edma_debug("Rxdesc flow control threshold value is %d for ring: %d\n",
			data, rxdesc_ring->ring_id);
	edma_reg_write(EDMA_REG_RXDESC_FC_THRE(rxdesc_ring->ring_id), data);
}

/*
 * edma_ppeds_rx_fill_ring_flow_control()
 *	PPE-DS Rx fill ring flow control configuration API
 */
static void edma_ppeds_rx_fill_ring_flow_control(struct edma_rxfill_ring *rxfill_ring)
{
	uint32_t data;

	data = (EDMA_PPEDS_RX_FC_XOFF_DEF & EDMA_RXFILL_FC_XOFF_THRE_MASK) <<
			 EDMA_RXFILL_FC_XOFF_THRE_SHIFT;
	data |= ((EDMA_PPEDS_RX_FC_XON_DEF & EDMA_RXFILL_FC_XON_THRE_MASK) <<
			 EDMA_RXFILL_FC_XON_THRE_SHIFT);

	edma_debug("Rxfill flow control threshold value is %d for ring: %d\n",
			data, rxfill_ring->ring_id);
	edma_reg_write(EDMA_REG_RXFILL_FC_THRE(rxfill_ring->ring_id), data);
}

/*
 * edma_ppeds_cfg_rx()
 *	API to configure PPE-DS EDMA Rx ring
 */
static void edma_ppeds_cfg_rx(struct edma_ppeds *ppeds_node)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi7_cfg->rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring = &wifi7_cfg->rx_ring;
	uint32_t paddr, saddr;
	uint32_t ring_sz;
	uint32_t data;
	uint32_t reg_val;

	/*
	 * No pre-header mode
	 */
	reg_val = edma_reg_read(EDMA_REG_RXDESC_CTRL(rxdesc_ring->ring_id));
	reg_val &= ~EDMA_RXDESC_WR_PH;
	edma_reg_write(EDMA_REG_RXDESC_CTRL(rxdesc_ring->ring_id), reg_val);

	paddr = (uint32_t)(rxfill_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_RXFILL_BA(rxfill_ring->ring_id), paddr);

	/*
	 * Fill up the higher 8 bits in another register
	 */
#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((rxfill_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_RXFILL_BA_HIGH(rxfill_ring->ring_id), paddr);
#endif

	ring_sz = rxfill_ring->count & EDMA_RXFILL_RING_SIZE_MASK;
	edma_reg_write(EDMA_RXFILL_RING_SIZE(rxfill_ring->ring_id), ring_sz);

	rxfill_ring->prod_idx = edma_reg_read(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id));

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

	/*
	 * Configure the default timer mitigation value
	 */
	data = (EDMA_RX_MOD_TIMER_INIT & EDMA_RX_MOD_TIMER_INIT_MASK)
			<< EDMA_RX_MOD_TIMER_INIT_SHIFT;
	edma_reg_write(EDMA_REG_RX_MOD_TIMER(rxdesc_ring->ring_id), data);

	/*
	 * Enable ring. Set ret mode to 'opaque'.
	 */
	edma_reg_write(EDMA_REG_RX_INT_CTRL(rxdesc_ring->ring_id), EDMA_RX_NE_INT_EN);

	/*
	 * Configure flow control and Rx ring to queue mapping
	 */
	edma_cfg_rx_desc_ring_to_queue_mapping_conf(rxdesc_ring, true);
	edma_ppeds_rx_desc_ring_flow_control(rxdesc_ring);
	edma_ppeds_rx_fill_ring_flow_control(rxfill_ring);
}

/*
 * edma_ppeds_rx_handle_irq
 *	Disable edma interrupt and enable wlan interrupt
 */
static irqreturn_t edma_ppeds_rx_handle_irq(int irq, void *ctx)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)ctx;
	struct edma_ppeds_node_wifi7 *wifi7_cfg =
			container_of(rxdesc_ring, struct edma_ppeds_node_wifi7, rx_ring);
	struct edma_ppeds *ppeds_node =
			container_of(wifi7_cfg, struct edma_ppeds, wifi7_cfg);

	/*
	 * Clear RxDesc ring interrupt mask
	 */
	edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
			EDMA_MASK_INT_CLEAR);

	/*
	 * Enable wlan interrupt
	 */
	ppeds_node->ops->enable_wlan_intr(&ppeds_node->ppeds_handle, true);

	return IRQ_HANDLED;
}

/*
 * edma_ppeds_enable_rx_reap_intr()
 *	PPEDS enable edma interrupt
 */
static void edma_ppeds_enable_rx_reap_intr(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node =
		container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxdesc_ring *rx_ring = &wifi7_cfg->rx_ring;
	uint32_t status;

	/*
	 * Clear on Read
	 */
	status = edma_reg_read(EDMA_REG_RXDESC_INT_STAT(rx_ring->ring_id)) &
			EDMA_RXDESC_RING_INT_STATUS_MASK;

	/*
	 * Set RXDESC ring interrupt mask
	 */
	edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rx_ring->ring_id),
			EDMA_RXDESC_INT_MASK_PKT_INT);
}

/*
 * edma_ppeds_alloc_rings()
 *	Allocate RxFill and TxCmpl rings
 */
static int edma_ppeds_alloc_rings(struct edma_ppeds *ppeds_node,
					nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t alloc_size;
	int ret;

	/* Determine allocation size */
	if (egc->rx_jumbo_mru)
		alloc_size = egc->rx_jumbo_mru;
	else
		alloc_size = NSS_DP_RX_BUFFER_SIZE;

	/* Setup ring counts and addresses */
	wifi7_cfg->rxfill_ring.count = ppeds_handle->wifi7_hdl.ppe2tcl_rxfill_num_desc;
	wifi7_cfg->rxfill_ring.alloc_size = alloc_size;
	wifi7_cfg->rx_ring.count = ppeds_handle->wifi7_hdl.ppe2tcl_num_desc;
	wifi7_cfg->rx_ring.pdma = (dma_addr_t)ppeds_handle->wifi7_hdl.ppe2tcl_ba;

	/* Allocate RxFill ring */
	ret = edma_ppeds_rx_fill_ring_alloc(&wifi7_cfg->rxfill_ring, false);
	if (ret != 0) {
		return ret;
	}

	/* Setup Tx ring */
	wifi7_cfg->txcmpl_ring.count = ppeds_handle->wifi7_hdl.reo2ppe_txcmpl_num_desc;
	wifi7_cfg->tx_ring.count = ppeds_handle->wifi7_hdl.reo2ppe_num_desc;
	wifi7_cfg->tx_ring.pdma = (dma_addr_t)ppeds_handle->wifi7_hdl.reo2ppe_ba;
	wifi7_cfg->tx_ring.pdesc = phys_to_virt(wifi7_cfg->tx_ring.pdma);
	memset(wifi7_cfg->tx_ring.pdesc, 0, 32 * wifi7_cfg->tx_ring.count);

	/* Allocate TxCmpl ring */
	ret = edma_ppeds_tx_cmpl_ring_alloc(&wifi7_cfg->txcmpl_ring, false);
	if (ret != 0) {
		edma_ppeds_rx_fill_ring_free(&wifi7_cfg->rxfill_ring);
		return ret;
	}

	return 0;
}

/*
 * edma_ppeds_alloc_arrays()
 *	Allocate rx_fill_arr and tx_cmpl_arr
 */
static int edma_ppeds_alloc_arrays(struct edma_ppeds *ppeds_node,
					 nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;

	ppeds_handle->wifi7_hdl.rx_fill_arr = (struct nss_dp_ppeds_rx_fill_elem *)kzalloc(
		sizeof(struct nss_dp_ppeds_rx_fill_elem) * wifi7_cfg->rxfill_ring.count,
		GFP_KERNEL);
	if (!ppeds_handle->wifi7_hdl.rx_fill_arr) {
		return -ENOMEM;
	}

	ppeds_handle->wifi7_hdl.tx_cmpl_arr = (struct nss_dp_ppeds_tx_cmpl_elem *)kzalloc(
		sizeof(struct nss_dp_ppeds_tx_cmpl_elem) * wifi7_cfg->txcmpl_ring.count,
		GFP_KERNEL);
	if (!ppeds_handle->wifi7_hdl.tx_cmpl_arr) {
		kfree(ppeds_handle->wifi7_hdl.rx_fill_arr);
		ppeds_handle->wifi7_hdl.rx_fill_arr = NULL;
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_ppeds_setup_irq_napi()
 *	Setup IRQ and NAPI for all rings
 */
static int edma_ppeds_setup_irq_napi(struct edma_ppeds *ppeds_node,
					   nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	int ret;

	/* Setup TxComp IRQ and NAPI */
	irq_set_status_flags(wifi7_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	snprintf(wifi7_cfg->txcmpl_irq_name, EDMA_IRQ_NAME_SIZE,
		 "edma_ppeds_txcmpl_%d", ppeds_node->db_idx);
	ret = request_irq(wifi7_cfg->txcmpl_intr, edma_tx_handle_irq, IRQF_SHARED,
			  wifi7_cfg->txcmpl_irq_name, (void *)&wifi7_cfg->txcmpl_ring);
	if (ret) {
		edma_err("PPEDS TXCMPL ring IRQ:%d request failed for node %d\n",
			 wifi7_cfg->txcmpl_intr, ppeds_node->db_idx);
		return ret;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	netif_napi_add(&ppeds_node->napi_ndev, &wifi7_cfg->txcmpl_ring.napi,
		       edma_ppeds_txcomp_napi_poll, ppeds_handle->wifi7_hdl.eth_txcomp_budget);
#else
	netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi7_cfg->txcmpl_ring.napi,
			      edma_ppeds_txcomp_napi_poll, ppeds_handle->wifi7_hdl.eth_txcomp_budget);
#endif

	/* Setup RxDesc IRQ and NAPI */
	irq_set_status_flags(wifi7_cfg->rxdesc_intr, IRQ_DISABLE_UNLAZY);
	snprintf(wifi7_cfg->rxdesc_irq_name, EDMA_IRQ_NAME_SIZE,
		 "edma_ppeds_rxdesc_%d", ppeds_node->db_idx);

	if (ppeds_handle->wifi7_hdl.polling_for_idx_update) {
		ret = request_irq(wifi7_cfg->rxdesc_intr, edma_rx_handle_irq, IRQF_SHARED,
				  wifi7_cfg->rxdesc_irq_name, (void *)&wifi7_cfg->rx_ring);
	} else {
		ret = request_irq(wifi7_cfg->rxdesc_intr, edma_ppeds_rx_handle_irq, IRQF_SHARED,
				  wifi7_cfg->rxdesc_irq_name, (void *)&wifi7_cfg->rx_ring);
	}

	if (ret) {
		edma_err("PPEDS RXDESC ring IRQ:%d request failed for node %d\n",
			 wifi7_cfg->rxdesc_intr, ppeds_node->db_idx);
		goto rxdesc_irq_fail;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	netif_napi_add(&ppeds_node->napi_ndev, &wifi7_cfg->rx_ring.napi,
		       edma_ppeds_rx_napi_poll, EDMA_PPEDS_RX_WEIGHT);
#else
	netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi7_cfg->rx_ring.napi,
			      edma_ppeds_rx_napi_poll, EDMA_PPEDS_RX_WEIGHT);
#endif

	/* Setup RxFill IRQ and NAPI */
	irq_set_status_flags(wifi7_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	snprintf(wifi7_cfg->rxfill_irq_name, EDMA_IRQ_NAME_SIZE,
		 "edma_ppeds_rxfill_%d", ppeds_node->db_idx);
	ret = request_irq(wifi7_cfg->rxfill_intr, edma_rxfill_handle_irq, IRQF_SHARED,
			  wifi7_cfg->rxfill_irq_name, (void *)&wifi7_cfg->rxfill_ring);
	if (ret) {
		edma_err("PPEDS RXFILL ring IRQ:%d request failed for node %d\n",
			 wifi7_cfg->rxfill_intr, ppeds_node->db_idx);
		goto rxfill_irq_fail;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	netif_napi_add(&ppeds_node->napi_ndev, &wifi7_cfg->rxfill_ring.napi,
		       edma_ppeds_rxfill_napi_poll, ppeds_handle->wifi7_hdl.eth_rxfill_budget);
#else
	netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi7_cfg->rxfill_ring.napi,
			      edma_ppeds_rxfill_napi_poll, ppeds_handle->wifi7_hdl.eth_rxfill_budget);
#endif

	return 0;

rxfill_irq_fail:
	irq_clear_status_flags(wifi7_cfg->rxdesc_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi7_cfg->rxdesc_intr);
	free_irq(wifi7_cfg->rxdesc_intr, (void *)&wifi7_cfg->rx_ring);
	netif_napi_del(&wifi7_cfg->rx_ring.napi);
rxdesc_irq_fail:
	irq_clear_status_flags(wifi7_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi7_cfg->txcmpl_intr);
	free_irq(wifi7_cfg->txcmpl_intr, (void *)&wifi7_cfg->txcmpl_ring);
	netif_napi_del(&wifi7_cfg->txcmpl_ring.napi);

	return ret;
}

/*
 * edma_ppeds_configure_rings()
 *	Configure secondary Tx rings
 */
static int edma_ppeds_configure_rings(struct edma_ppeds *ppeds_node)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	int ret;

	ret = edma_ppeds_tx_secondary_alloc(&wifi7_cfg->tx_ring);
	if (ret) {
		edma_err("Failed to setup secondary EDMA Tx Desc Ring\n");
		return ret;
	}

	return 0;
}

/*
 * edma_ppeds_setup_mappings()
 *	Setup Tx and Rx ring mappings
 */
static void edma_ppeds_setup_mappings(struct edma_ppeds *ppeds_node)
{
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;

	edma_cfg_tx_map_tx_ring_to_txcmpl(wifi7_cfg->tx_ring.id, wifi7_cfg->txcmpl_ring.id);
	edma_ppeds_set_rx_mapping(wifi7_cfg->rxfill_ring.ring_id, wifi7_cfg->rx_ring.ring_id,
				  wifi7_cfg->ppe_qid, wifi7_cfg->ppe_num_queues);
}

/*
 * edma_ppeds_inst_register()
 *	PPE-DS EDMA instance registration API
 */
static bool edma_ppeds_inst_register(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	int ret;

	/* Validate and update node state */
	write_lock_bh(&drv->lock);
	if (node_cfg->node_state != EDMA_PPEDS_NODE_STATE_ALLOC) {
		edma_err("%px: Invalid node state: %d, registration failed\n",
			 ppeds_node, node_cfg->node_state);
		write_unlock_bh(&drv->lock);
		return false;
	}
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_REG_IN_PROG;
	write_unlock_bh(&drv->lock);

	/* Allocate rings */
	ret = edma_ppeds_alloc_rings(ppeds_node, ppeds_handle);
	if (ret != 0) {
		return false;
	}

	/* Allocate arrays */
	ret = edma_ppeds_alloc_arrays(ppeds_node, ppeds_handle);
	if (ret != 0) {
		goto arrays_alloc_failed;
	}

	/* Setup dummy netdev for NAPIs */
	init_dummy_netdev(&ppeds_node->napi_ndev);

	/* Setup IRQ and NAPI */
	ret = edma_ppeds_setup_irq_napi(ppeds_node, ppeds_handle);
	if (ret != 0) {
		goto irq_napi_failed;
	}

	/* Configure rings */
	ret = edma_ppeds_configure_rings(ppeds_node);
	if (ret != 0) {
		goto configure_rings_failed;
	}

	/* Setup mappings */
	edma_ppeds_setup_mappings(ppeds_node);

	/* Configure Tx and Rx */
	edma_ppeds_cfg_tx(ppeds_node);
	edma_ppeds_cfg_rx(ppeds_node);

	edma_debug("EDMA PPE-DS registration successfull."
		   " PPE2TCL ring size: %d, REO2PPE ring size: %d,"
		   " Rxfill ring size: %d, Txcmpl ring size: %d\n",
		   ppeds_handle->wifi7_hdl.ppe2tcl_num_desc,
		   ppeds_handle->wifi7_hdl.reo2ppe_num_desc,
		   ppeds_handle->wifi7_hdl.ppe2tcl_rxfill_num_desc,
		   ppeds_handle->wifi7_hdl.reo2ppe_txcmpl_num_desc);

	/* Update node state to registered */
	write_lock_bh(&drv->lock);
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_REG_DONE;
	write_unlock_bh(&drv->lock);

	return true;

configure_rings_failed:
	/* Cleanup IRQ and NAPI */
	irq_clear_status_flags(wifi7_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi7_cfg->rxfill_intr);
	free_irq(wifi7_cfg->rxfill_intr, (void *)&wifi7_cfg->rxfill_ring);
	netif_napi_del(&wifi7_cfg->rxfill_ring.napi);

	irq_clear_status_flags(wifi7_cfg->rxdesc_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi7_cfg->rxdesc_intr);
	free_irq(wifi7_cfg->rxdesc_intr, (void *)&wifi7_cfg->rx_ring);
	netif_napi_del(&wifi7_cfg->rx_ring.napi);

	irq_clear_status_flags(wifi7_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi7_cfg->txcmpl_intr);
	free_irq(wifi7_cfg->txcmpl_intr, (void *)&wifi7_cfg->txcmpl_ring);
	netif_napi_del(&wifi7_cfg->txcmpl_ring.napi);

irq_napi_failed:
	kfree(ppeds_handle->wifi7_hdl.tx_cmpl_arr);
	ppeds_handle->wifi7_hdl.tx_cmpl_arr = NULL;
	kfree(ppeds_handle->wifi7_hdl.rx_fill_arr);
	ppeds_handle->wifi7_hdl.rx_fill_arr = NULL;

arrays_alloc_failed:
	edma_ppeds_rx_fill_ring_free(&wifi7_cfg->rxfill_ring);
	edma_ppeds_tx_cmpl_ring_free(&wifi7_cfg->txcmpl_ring);

	return false;
}

/*
 * edma_ppeds_inst_refill()
 *	API to fill PPE-DS EDMA RxFill ring
 */
static void edma_ppeds_inst_refill(nss_dp_ppeds_handle_t *ppeds_handle, int count)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	uint32_t headroom = EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi7_cfg->rxfill_ring;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	uint32_t num_avail;

	read_lock_bh(&drv->lock);
	if ((node_cfg->node_state != EDMA_PPEDS_NODE_STATE_REG_DONE) &&
		(node_cfg->node_state != EDMA_PPEDS_NODE_STATE_STOP_DONE)) {
		edma_err("%px: Invalid node state: %d, PPE-DS rxfill failed\n", ppeds_node,
				node_cfg->node_state);
		read_unlock_bh(&drv->lock);
		return;
	}
	read_unlock_bh(&drv->lock);

	num_avail = ppeds_node->ops->rx_fill(&ppeds_node->ppeds_handle, count, rxfill_ring->alloc_size, headroom);

	if(count != num_avail) {
		edma_warn("Got %d less than what is asked for %d\n", num_avail, count);
	}

	edma_ppeds_rx_alloc_buffer(rxfill_ring, num_avail,
			ppeds_node->ppeds_handle.wifi7_hdl.rx_fill_arr, headroom);
}

/*
 * edma_ppeds_get_ppe_queues()
 *	Get the associated PPE queues with the given instance
 */
static bool edma_ppeds_get_ppe_queues(nss_dp_ppeds_handle_t *ppeds_handle, uint32_t *ppe_queue_start)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);

	read_lock_bh(&drv->lock);
	if ((node_cfg->node_state != EDMA_PPEDS_NODE_STATE_START_DONE) &&
		(node_cfg->node_state != EDMA_PPEDS_NODE_STATE_ALLOC)) {
		edma_err("%px: Invalid node state: %d, PPE-DS get queues failed\n", ppeds_node,
				node_cfg->node_state);
		read_unlock_bh(&drv->lock);
		return false;
	}
	read_unlock_bh(&drv->lock);

	*ppe_queue_start = wifi7_cfg->ppe_qid;
	return true;
}

/*
 * edma_ppeds_set_tx_prod_idx()
 *	Set EDMA TX producer idx
 */
static void edma_ppeds_set_tx_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle, uint16_t tx_prod_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t work_to_do = 0;
	uint32_t cons_idx;

	if (unlikely(egc->enable_ring_util_stats)) {
		cons_idx = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(wifi7_cfg->tx_ring.id)) & EDMA_TXDESC_CONS_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(tx_prod_idx, cons_idx, wifi7_cfg->tx_ring.count);
		edma_update_ring_stats(work_to_do, wifi7_cfg->tx_ring.count,
				       &wifi7_cfg->tx_ring.tx_desc_stats.ring_stats);
	}

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(wifi7_cfg->tx_ring.id), tx_prod_idx);
}

/*
 * edma_ppeds_set_rx_cons_idx()
 *	Set EDMA RX consumer idx
 */
static void edma_ppeds_set_rx_cons_idx(nss_dp_ppeds_handle_t *ppeds_handle, uint16_t rx_cons_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t work_to_do = 0;
	uint32_t prod_idx;

	if (unlikely(egc->enable_ring_util_stats)) {
		prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(wifi7_cfg->rx_ring.ring_id)) & EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx, rx_cons_idx, wifi7_cfg->rx_ring.count);
		edma_update_ring_stats(work_to_do, wifi7_cfg->rx_ring.count,
				       &wifi7_cfg->rx_ring.rx_desc_stats.ring_stats);
	}

	edma_reg_write(EDMA_REG_RXDESC_CONS_IDX(wifi7_cfg->rx_ring.ring_id), rx_cons_idx);
}

/*
 * edma_ppeds_get_rx_prod_idx()
 *	Get EDMA RX producer idx
 */
static uint16_t edma_ppeds_get_rx_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	uint16_t prod_idx;
	uint32_t data;

	data = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(wifi7_cfg->rx_ring.ring_id));
	prod_idx = data & EDMA_RXDESC_PROD_IDX_MASK;

	return prod_idx;
}

/*
 * edma_ppeds_get_tx_cons_idx()
 *	Get EDMA TX consumer idx
 */
static uint16_t edma_ppeds_get_tx_cons_idx(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	uint16_t cons_idx;
	uint32_t data;

	data = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(wifi7_cfg->tx_ring.id));
	cons_idx = data & EDMA_TXDESC_CONS_IDX_MASK;

	return cons_idx;
}

/*
 * edma_ppeds_get_rxfill_cons_idx()
 *	Get rxfill ring consumer index
 */
static uint16_t edma_ppeds_get_rxfill_cons_idx(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi7_cfg->rxfill_ring;

	return edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) &
				EDMA_RXFILL_CONS_IDX_MASK;
}

/*
 * edma_ppeds_host_rings_configuration_enable()
 *	API to enable EDMA host rings configurations while WLAN reset
 */
static void edma_ppeds_host_rings_configuration_enable(struct edma_gbl_ctx *gbl_ctx)
{
	struct edma_host_info *host_info = &init_info.host_info;
	struct edma_rx_rings_info *rx_info = &host_info->sfe_info.rx_info;
	uint32_t num_queues_per_ring = rx_info->num_queues_per_ring;
	uint32_t i, queue_start = 0;

	edma_enable_rx_interrupts(gbl_ctx);
	edma_cfg_rx_rings_enable(gbl_ctx);
	edma_cfg_tx_rings_enable(gbl_ctx);

	for (i = 0; i < rx_info->num_rx_rings; i++) {
		queue_start = gbl_ctx->rx_queue_start + rx_info->rx_map[i].ppe_queue_base;

		/*
		 * Enable the PPE queues corresponding to host RX ring to stop the incoming
		 * traffic on the ring.
		 */
		if (!edma_cfg_rx_ring_en_mapped_queues(gbl_ctx, queue_start, num_queues_per_ring, true)) {
			edma_err("Failed to enable the host rx queue:%d\n", queue_start);
		}
	}
}

/*
 * edma_ppeds_host_rings_configuration_disable()
 *	API to disable EDMA host rings configurations while WLAN reset
 */
static void edma_ppeds_host_rings_configuration_disable(struct edma_gbl_ctx *gbl_ctx)
{
	struct edma_host_info *host_info = &init_info.host_info;
	struct edma_rx_rings_info *rx_info = &host_info->sfe_info.rx_info;
	uint32_t num_queues_per_ring = rx_info->num_queues_per_ring;
	uint32_t i, queue_start = 0;

	for (i = 0; i < rx_info->num_rx_rings; i++) {
		queue_start = gbl_ctx->rx_queue_start + rx_info->rx_map[i].ppe_queue_base;

		/*
		 * Disable the PPE queues corresponding to host RX ring to stop the incoming
		 * traffic on the ring.
		 */
		if (!edma_cfg_rx_ring_en_mapped_queues(gbl_ctx, queue_start, num_queues_per_ring, false)) {
			edma_err("Failed to disable the host rx queue:%d\n", queue_start);
		}
	}

	edma_cfg_rx_rings_disable(gbl_ctx);
	edma_cfg_tx_rings_disable(gbl_ctx);
	edma_disable_rx_interrupts(gbl_ctx);

}

/*
 * edma_ppeds_set_rxfill_prod_idx()
 *	Set rxfill ring producer index
 */
static void edma_ppeds_set_rxfill_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle,
					   uint16_t prod_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi7_cfg->rxfill_ring;

	edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id), prod_idx);
}

/*
 * edma_ppeds_get_rxfill_ring_info()
 *	Get WiFi7 RxFill ring filled-count information
 */
static void edma_ppeds_get_rxfill_ring_info(nss_dp_ppeds_handle_t *ppeds_handle,
					struct nss_dp_ppeds_rxfill_ring_info *info)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi7_cfg->rxfill_ring;
	struct nss_dp_ppeds_wifi7_rxfill_ring_info *wifi7 = &info->wifi7;

	if (info->arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7) {
		return;
	}

	wifi7->prim_prod_idx = rxfill_ring->prod_idx;
	wifi7->prim_cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) &
				EDMA_RXFILL_CONS_IDX_MASK;
	wifi7->prim_active_cnt = (wifi7->prim_prod_idx - wifi7->prim_cons_idx + rxfill_ring->count) &
				rxfill_ring->count_mask;
}

/*
 * edma_ppeds_inst_start()
 *	PPE-DS EDMA instance start API
 */
static int edma_ppeds_inst_start(nss_dp_ppeds_handle_t *ppeds_handle, uint8_t intr_enable,
				struct nss_ppe_ds_ctx_info_handle *info_hdl)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_ppeds_drv *drv = &egc->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	uint32_t data;

	write_lock_bh(&drv->lock);
	if ((node_cfg->node_state != EDMA_PPEDS_NODE_STATE_REG_DONE) &&
		(node_cfg->node_state != EDMA_PPEDS_NODE_STATE_STOP_DONE)) {
		edma_err("%px: Invalid node state: %d, PPE-DS start failed\n", ppeds_node,
				node_cfg->node_state);
		write_unlock_bh(&drv->lock);
		return -1;
	}
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_START_IN_PROG;
	write_unlock_bh(&drv->lock);

	if (ppeds_node->umac_reset_inprogress) {
		/*
		 * Clear dequeue drop on all the DDRQs
		 */
		edma_ddrq_dequeue_drop_disable_all();
		edma_debug("Cleared the DDRQs dequeue drop configuration\n");

		/*
		 * Reset enqueue disable on all the DDRQs
		 */
		edma_ddrq_enqueue_enable_all();
		edma_debug("DDRQs enqueue configurations are enabled\n");

		/*
		 * Enable enqueue for all the ISQs
		 */
		if (!edma_cfg_rx_ring_enq_en_mapped_queues(edma_ddrq_isq_base, EDMA_DDRQ_ISQ_NUM, true)) {
			edma_err("Error in enabling enqueue of ISQs (base: %d)\n", edma_ddrq_isq_base);
		}

		/*
		 * Enabling the host rings related configurations
		 */
		edma_ppeds_host_rings_configuration_enable(egc);
		edma_debug("The host ring configurations are enabled successfully\n");

#ifdef NSS_DP_PON_SUPPORT
		/*
		 * Re-enable enqueue and dequeue for all PON MAC PPE queues.
		 */
		edma_ppeds_pon_pq_enable();
#endif
	}

	/*
	 * Configure RxFill Low threshold value and
	 * enable RXFILL Low threshold interrupt along with the
	 * associated NAPI
	 */
	edma_debug("Setting low threshold to %d\n", ppeds_handle->wifi7_hdl.eth_rxfill_low_thr);

	edma_reg_write(EDMA_REG_RXFILL_UGT_THRE(wifi7_cfg->rxfill_ring.ring_id),
			EDMA_RXFILL_LOW_THRE_MASK & ppeds_handle->wifi7_hdl.eth_rxfill_low_thr);
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(wifi7_cfg->rxfill_ring.ring_id),
			EDMA_RXFILL_INT_MASK);
	if (!ppeds_node->umac_reset_inprogress) {
		napi_enable(&wifi7_cfg->rxfill_ring.napi);
	}

	/*
	 * Enable TxComp interrupt along with the associated NAPI
	 */
	edma_reg_write(EDMA_REG_TX_INT_MASK(wifi7_cfg->txcmpl_ring.id),
			EDMA_TX_INT_MASK_PKT_INT);
	if (!ppeds_node->umac_reset_inprogress) {
		napi_enable(&wifi7_cfg->txcmpl_ring.napi);
	}

	/*
	 * Enable RxDesc Ring.
	 */
	data = edma_reg_read(EDMA_REG_RXDESC_CTRL(wifi7_cfg->rx_ring.ring_id));
	data |= EDMA_RXDESC_RX_EN;
	edma_reg_write(EDMA_REG_RXDESC_CTRL(wifi7_cfg->rx_ring.ring_id), data);

	/*
	 * Reset RxDesc disable Reg.
	 */
	data = edma_reg_read(EDMA_REG_RXDESC_DISABLE(wifi7_cfg->rx_ring.ring_id));
	data &= ~EDMA_RXDESC_RX_DISABLE;
	edma_reg_write(EDMA_REG_RXDESC_DISABLE(wifi7_cfg->rx_ring.ring_id), data);

	/*
	 * Enable RxFill Ring.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi7_cfg->rxfill_ring.ring_id));
	data |= EDMA_RXFILL_RING_EN;
	edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi7_cfg->rxfill_ring.ring_id), data);

	/*
	 * Reset RxFill disable Reg.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi7_cfg->rxfill_ring.ring_id));
	data &= ~EDMA_RXFILL_RING_DISABLE;
	edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi7_cfg->rxfill_ring.ring_id), data);

	/*
	 * Enable Tx Ring.
	 */
	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi7_cfg->tx_ring.id));
	data |= EDMA_TXDESC_TX_ENABLE;
	edma_reg_write(EDMA_REG_TXDESC_CTRL(wifi7_cfg->tx_ring.id), data);

	/*
	 * If the ring reset is supported,
	 * Enable the PPE-DS node queues that are disabled at the time of inst stop.
	 */
	if (edma_dp_per_ring_reset_support()) {
		if (!edma_cfg_rx_ring_en_mapped_queues(egc, wifi7_cfg->ppe_qid, wifi7_cfg->ppe_num_queues, true)) {
			edma_err("%px: Failed to enable the queue in PPE-DS start%d qid\n", ppeds_node, wifi7_cfg->ppe_qid);
		}
	}

	write_lock_bh(&drv->lock);
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_START_DONE;
	write_unlock_bh(&drv->lock);
	ppeds_node->umac_reset_inprogress = info_hdl->umac_reset_inprogress;

	return 0;
}

/*
 * edma_ppeds_inst_stop()
 *	PPE-DS EDMA instance stop API
 */
static void edma_ppeds_inst_stop(nss_dp_ppeds_handle_t *ppeds_handle, uint8_t intr_enable,
				struct nss_ppe_ds_ctx_info_handle *info_hdl)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_gbl_ctx *gbl_ctx = edma_gbl_ctx;
	struct edma_ppeds_drv *drv = &gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	uint32_t data;
	uint32_t passthr_1st_pc, passthr_2nd_pc;
	uint32_t poll_timeout = EDMA_DDRQ_HW_CONSUME_LOOP_CNT;
	uint16_t prod_idx;

	ppeds_node->umac_reset_inprogress = info_hdl->umac_reset_inprogress;
	write_lock_bh(&drv->lock);
	if (node_cfg->node_state != EDMA_PPEDS_NODE_STATE_START_DONE) {
		edma_err("%px: Invalid node state: %d, PPE-DS stop failed\n", ppeds_node,
				node_cfg->node_state);
		write_unlock_bh(&drv->lock);
		return;
	}
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_STOP_IN_PROG;
	write_unlock_bh(&drv->lock);

	if (ppeds_node->umac_reset_inprogress) {
		/*
		 * Disabling enqueue for all the ISQs
		 */
		if (!edma_cfg_rx_ring_enq_en_mapped_queues(edma_ddrq_isq_base, EDMA_DDRQ_ISQ_NUM, false)) {
			edma_err("Error in disabling enqueue of ISQs (base: %d)\n", edma_ddrq_isq_base);
		}

		mdelay(5);
		/*
		 * Disable enqueue for all the DDRQs
		 */
		edma_ddrq_enqueue_disable_all();
	}

	/*
	 * Disable TxDesc rings.
	 */
	data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi7_cfg->tx_ring.id));
	data &= ~EDMA_TXDESC_TX_ENABLE;
	edma_reg_write(EDMA_REG_TXDESC_CTRL(wifi7_cfg->tx_ring.id), data);
	do {
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi7_cfg->tx_ring.id));
		data &= EDMA_TXDESC_TX_ENABLE;
	} while (data);

	/*
	 * Reset the TX ring if Hardware support is present.
	 */
	if (edma_dp_per_ring_reset_support()) {
		edma_cfg_tx_ring_reset(&wifi7_cfg->tx_ring);

		/*
		 * Disable the PPE queues corresponding to RX ring to stop the incoming
		 * traffic on the ring.
		 */
		if (!edma_cfg_rx_ring_en_mapped_queues(gbl_ctx, wifi7_cfg->ppe_qid, wifi7_cfg->ppe_num_queues, false)) {
			edma_err("%px: Failed to disable the queue in PPE-DS stop %d queue id", ppeds_node, wifi7_cfg->ppe_qid);
		}
	}

	/*
	 * Clear enable bit, set disable bit and wait untill Rx Desc ring is disabled.
	 */
	data = edma_reg_read(EDMA_REG_RXDESC_CTRL(wifi7_cfg->rx_ring.ring_id));
	data &= ~EDMA_RXDESC_RX_EN;
	edma_reg_write(EDMA_REG_RXDESC_CTRL(wifi7_cfg->rx_ring.ring_id), data);

	data = edma_reg_read(EDMA_REG_RXDESC_DISABLE(wifi7_cfg->rx_ring.ring_id));
	data |= EDMA_RXDESC_RX_DISABLE;
	edma_reg_write(EDMA_REG_RXDESC_DISABLE(wifi7_cfg->rx_ring.ring_id), data);

	do {
		data = edma_reg_read(EDMA_REG_RXDESC_DISABLE_DONE(wifi7_cfg->rx_ring.ring_id));
	} while (!data);

	/*
	 * Reset the ring if Hardware support is present.
	 */
	if (edma_dp_per_ring_reset_support())
		edma_cfg_rx_ring_reset(&wifi7_cfg->rx_ring);

	/*
	 * Disable Tx complete interrupt and NAPI
	 */
	edma_reg_write(EDMA_REG_TX_INT_MASK(wifi7_cfg->txcmpl_ring.id),
			EDMA_MASK_INT_CLEAR);
	if (!ppeds_node->umac_reset_inprogress) {
		synchronize_irq(wifi7_cfg->txcmpl_intr);
		napi_disable(&wifi7_cfg->txcmpl_ring.napi);
	}

	/*
	 * Clear enable bit, set the disable bit and wait until the RxFill ring is disabled.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi7_cfg->rxfill_ring.ring_id));
	data &= ~EDMA_RXFILL_RING_EN;
	edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi7_cfg->rxfill_ring.ring_id), data);

	data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi7_cfg->rxfill_ring.ring_id));
	data |= EDMA_RXFILL_RING_DISABLE;
	edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi7_cfg->rxfill_ring.ring_id), data);

	do {
		data = edma_reg_read(EDMA_REG_RXFILL_DISABLE_DONE(wifi7_cfg->rxfill_ring.ring_id));
	} while (!data);

	/*
	 * Disable Rxfill interrupt and NAPI
	 */
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(wifi7_cfg->rxfill_ring.ring_id),
			EDMA_MASK_INT_CLEAR);
	if (!ppeds_node->umac_reset_inprogress) {
		synchronize_irq(wifi7_cfg->rxfill_intr);
		napi_disable(&wifi7_cfg->rxfill_ring.napi);
	}

	if (ppeds_node->umac_reset_inprogress) {
		/*
		 * Disable EDMA host rings related configurations
		 */
		edma_ppeds_host_rings_configuration_disable(gbl_ctx);
		edma_debug("EDMA host ring configurations got disabled\n");

#ifdef NSS_DP_PON_SUPPORT
		/*
		 * Disable enqueue/dequeue and flush each PON MAC PPE queue.
		 */
		edma_ppeds_pon_pq_disable();

		/*
		 * Drain TCF SRAM and zero PPE TCONT credits for all 32 TCONTs.
		 * Calls pon_tcf_bwmrpt_drainout_tcont_by_bitmap directly via the
		 * registered callback — safe from process context.
		 */
		if (nss_dp_pon_drainout(NSS_DP_PON_TCONT_BITMAP_ALL, "umac_reset_wifi7") != 0)
			edma_warn("PON TCF drainout failed for wifi7\n");
#endif

		/*
		 * Enabling dequeue drop on all the DDRQs
		 */
		edma_ddrq_dequeue_drop_enable_all();

		/*
		 * Write cons_idx to max value in order for DDRQ to push all packets
		 * onto the completion ring without boundary.
		 */
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(wifi7_cfg->txcmpl_ring.id),
					wifi7_cfg->txcmpl_ring.count);

		/*
		 * Wait for 20ms and then clean the tx complete ring
		 */
		mdelay(19);
		do {
			mdelay(1);
			passthr_1st_pc = edma_reg_read(EDMA_REG_TXDESC_PASSTHR_UNREL(wifi7_cfg->tx_ring.id));
			passthr_2nd_pc = edma_reg_read((EDMA_REG_TXDESC_PASSTHR_UNREL(wifi7_cfg->tx_ring.id) + 4));
			edma_debug("passthr_1st_pc: %d, passthr_2nd_pc: %d", passthr_1st_pc, passthr_2nd_pc);
			if (--poll_timeout == 0) {
				edma_err("Timeout waiting for passthrough counters to stabilize\n");
				break;
			}
		} while (passthr_1st_pc != passthr_2nd_pc);

		/*
		 * Reset the PROD_IDX and CONS_IDX, once all packets are pushed to txcmpl
		 * ring.
		 */
		data = edma_reg_read(EDMA_REG_TXCMPL_PROD_IDX(wifi7_cfg->txcmpl_ring.id));
		prod_idx = data & EDMA_TXCMPL_PROD_IDX_MASK;
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(wifi7_cfg->txcmpl_ring.id), prod_idx);

		edma_debug("UMAC RESET:Txcmpl prod:%d cons:%d passthr_1st_pc=0x%x(%u) passthr_2nd_pc=0x%x(%u)\n",
				prod_idx,
				edma_reg_read(EDMA_REG_TXCMPL_CONS_IDX(wifi7_cfg->txcmpl_ring.id)),
				passthr_1st_pc, passthr_1st_pc,
				passthr_2nd_pc, passthr_2nd_pc);
	} else {
		/*
		 * Wait for 5ms and then clean the tx complete ring
		 */
		mdelay(5);
		edma_ppeds_tx_complete(wifi7_cfg->txcmpl_ring.count, &wifi7_cfg->txcmpl_ring);
	}

	write_lock_bh(&drv->lock);
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_STOP_DONE;
	write_unlock_bh(&drv->lock);
}

/*
 * edma_ppeds_inst_free()
 *	PPE-DS EDMA instance free API
 */
static void edma_ppeds_inst_free(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);

	write_lock_bh(&drv->lock);
	if (node_cfg->node_state != EDMA_PPEDS_NODE_STATE_STOP_DONE) {
		edma_err("%px: Invalid node state: %d, PPE-DS free failed\n", ppeds_node,
				node_cfg->node_state);
		write_unlock_bh(&drv->lock);
		return;
	}
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_FREE_IN_PROG;
	write_unlock_bh(&drv->lock);

	kfree(ppeds_handle->wifi7_hdl.rx_fill_arr);
	ppeds_handle->wifi7_hdl.rx_fill_arr = NULL;

	kfree(ppeds_handle->wifi7_hdl.tx_cmpl_arr);
	ppeds_handle->wifi7_hdl.tx_cmpl_arr = NULL;

	irq_clear_status_flags(wifi7_cfg->rxdesc_intr, IRQ_DISABLE_UNLAZY);
	free_irq(wifi7_cfg->rxdesc_intr,
			(void *)&wifi7_cfg->rx_ring);
	netif_napi_del(&wifi7_cfg->rx_ring.napi);

	irq_clear_status_flags(wifi7_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	free_irq(wifi7_cfg->txcmpl_intr,
			(void *)&wifi7_cfg->txcmpl_ring);
	netif_napi_del(&wifi7_cfg->txcmpl_ring.napi);

	irq_clear_status_flags(wifi7_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	free_irq(wifi7_cfg->rxfill_intr,
			(void *)&wifi7_cfg->rxfill_ring);
	netif_napi_del(&wifi7_cfg->rxfill_ring.napi);

	edma_ppeds_rx_fill_ring_free(&wifi7_cfg->rxfill_ring);
	edma_ppeds_tx_cmpl_ring_free(&wifi7_cfg->txcmpl_ring);

	kfree(ppeds_node);

	/*
	 * Clear the rings in global ring info context.
	 */
	edma_ppeds_reset_gbl_ds_ctx();

	/*
	 * Remove from DB
	 */
	write_lock_bh(&drv->lock);
	node_cfg->ppeds_db = NULL;
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_AVAIL;
	write_unlock_bh(&drv->lock);
}

/*
 * edma_ppeds_inst_alloc()
 *	PPE-DS EDMA instance allocation API
 */
static nss_dp_ppeds_handle_t *edma_ppeds_inst_alloc(const struct nss_dp_ppeds_cb *ops, size_t priv_size)
{
	int size = priv_size + sizeof(struct edma_ppeds);
	struct edma_ds_info *ds_info = &init_info.ds_info;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_rxdesc_ring_info *rxdesc_info;
	struct edma_rxfill_ring_info *rxfill_info;
	struct edma_txdesc_ring_info *txdesc_info;
	struct edma_txcmpl_ring_info *txcmpl_info;
	struct edma_ppeds_node_info *node_info;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t i, ring_id;
	struct edma_ppeds *ppeds_node;

	if (!ops || !ops->rx || !ops->rx_fill || !ops->rx_release
			|| !ops->tx_cmpl) {
		edma_err("Invalid PPE-DS operations\n");
		return NULL;
	}

	edma_debug("ppeds node size %lu total size %d\n", sizeof(struct edma_ppeds),  size);
	ppeds_node = (struct edma_ppeds *)kzalloc(size, GFP_KERNEL);
	if (!ppeds_node) {
		edma_err("Cannot allocate memory for ppeds node\n");
		return NULL;
	}

	ppeds_node->ops = ops;
	ppeds_node->wifi_arch_mode = EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7;
	ppeds_node->ppeds_handle.wifi_arch_mode = EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7;

	/*
	 * Add to Database
	 */
	write_lock_bh(&drv->lock);
	for (i = 0; i < drv->num_nodes; i++) {
		if (drv->ppeds_node_cfg[i].node_state == EDMA_PPEDS_NODE_STATE_AVAIL) {
			break;
		}
	}

	if (i == drv->num_nodes) {
		write_unlock_bh(&drv->lock);
		edma_err("Cannot get a free edma PPE-DS resource set\n");
		kfree(ppeds_node);
		return NULL;
	}

	ppeds_node->db_idx = i;
	drv->ppeds_node_cfg[i].ppeds_db = ppeds_node;

	struct edma_ppeds_node_wifi7 *wifi7_cfg = &ppeds_node->wifi7_cfg;
	node_info = &ds_info->ppeds_info.node_info[ppeds_node->db_idx];
	wifi7_cfg->rxfill_ring.ring_id = node_info->rx_map[0].rx_fill_ring_id;
	wifi7_cfg->txcmpl_ring.id = node_info->tx_map[0].tx_cmpl_ring_id;
	wifi7_cfg->rx_ring.ring_id = node_info->rx_map[0].rx_ring_id;
	wifi7_cfg->tx_ring.id = node_info->tx_map[0].tx_ring_id;
	wifi7_cfg->ppe_qid = node_info->rx_map[0].ppe_queue_base;
	wifi7_cfg->ppe_num_queues = node_info->num_queues_per_ring;
	wifi7_cfg->txcmpl_intr = edma_gbl_ctx->txcmpl_info[wifi7_cfg->txcmpl_ring.id].intr_num;
	wifi7_cfg->rxfill_intr = edma_gbl_ctx->rxfill_info[wifi7_cfg->rxfill_ring.ring_id].intr_num;
	wifi7_cfg->rxdesc_intr = edma_gbl_ctx->rxdesc_info[wifi7_cfg->rx_ring.ring_id].intr_num;

	if (wifi7_cfg->txcmpl_intr <= 0 || wifi7_cfg->rxfill_intr <= 0 || wifi7_cfg->rxdesc_intr <= 0) {
		edma_err("Invalid interrupt numbers for PPE-DS node %d: txcmpl=%d, rxfill=%d, rxdesc=%d\n",
				ppeds_node->db_idx, wifi7_cfg->txcmpl_intr,
				wifi7_cfg->rxfill_intr, wifi7_cfg->rxdesc_intr);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}

	rxdesc_info = egc->rxdesc_info;
	rxfill_info = egc->rxfill_info;
	txdesc_info = egc->txdesc_info;
	txcmpl_info = egc->txcmpl_info;

	ring_id = wifi7_cfg->rxfill_ring.ring_id;
	if (rxfill_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("RXfill ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	rxfill_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	rxfill_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	rxfill_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;

	ring_id = wifi7_cfg->rx_ring.ring_id;
	if (rxdesc_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("RX ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	rxdesc_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	rxdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	rxdesc_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;
	rxdesc_info[ring_id].ppe_queue_base = wifi7_cfg->ppe_qid;
	rxdesc_info[ring_id].ppe_num_queues = wifi7_cfg->ppe_num_queues;

	ring_id = wifi7_cfg->tx_ring.id;
	if (txdesc_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("TX ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	txdesc_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	txdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	txdesc_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;

	ring_id = wifi7_cfg->txcmpl_ring.id;
	if (txcmpl_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("TXCMPL ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	txcmpl_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	txcmpl_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	txcmpl_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;

	edma_debug("PPE-DS node(%d): Rxfill ring: %d, Tx complete ring: %d,"
			" Rx ring: %d, Tx ring: %d, Queue start id: %d,"
			" Queue count: %d, Tx complete interrupt: %d,"
			" Rxfill interrupt: %d, Rx interrupt: %d\n", i,
			wifi7_cfg->rxfill_ring.ring_id, wifi7_cfg->txcmpl_ring.id,
			wifi7_cfg->rx_ring.ring_id, wifi7_cfg->tx_ring.id,
			wifi7_cfg->ppe_qid, wifi7_cfg->ppe_num_queues,
			wifi7_cfg->txcmpl_intr, wifi7_cfg->rxfill_intr,
			wifi7_cfg->rxdesc_intr);
	drv->ppeds_node_cfg[i].node_state = EDMA_PPEDS_NODE_STATE_ALLOC;
	write_unlock_bh(&drv->lock);

	return &ppeds_node->ppeds_handle;
}

/*
 * edma_ppeds_ops
 *	PPE-DS operations
 */
struct nss_dp_ppeds_ops edma_ppeds_ops_wifi7 = {
	.alloc			=	edma_ppeds_inst_alloc,
	.reg			=	edma_ppeds_inst_register,
	.start 			=	edma_ppeds_inst_start,
	.refill			=	edma_ppeds_inst_refill,
	.stop			=	edma_ppeds_inst_stop,
	.free			=	edma_ppeds_inst_free,
	.get_queues		=	edma_ppeds_get_ppe_queues,
	.set_rx_cons_idx	=	edma_ppeds_set_rx_cons_idx,
	.set_tx_prod_idx	=	edma_ppeds_set_tx_prod_idx,
	.get_tx_cons_idx	=	edma_ppeds_get_tx_cons_idx,
	.get_rx_prod_idx	=	edma_ppeds_get_rx_prod_idx,
	.get_rxfill_cons_idx	=	edma_ppeds_get_rxfill_cons_idx,
	.set_rxfill_prod_idx	=	edma_ppeds_set_rxfill_prod_idx,
	.enable_rx_reap_intr	=	edma_ppeds_enable_rx_reap_intr,
	.service_status_update	=	edma_ppeds_service_status_update,
	.get_rxfill_ring_info	=	edma_ppeds_get_rxfill_ring_info,
};
