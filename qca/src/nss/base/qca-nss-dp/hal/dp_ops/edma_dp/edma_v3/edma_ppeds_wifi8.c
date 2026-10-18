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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = container_of(txcmpl_ring, struct edma_ppeds_node_wifi8, txcmpl_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);
	nss_dp_ppeds_handle_t *ppeds_handle = &ppeds_node->ppeds_handle;
	bool hw_buff_mgmt = ppeds_handle->wifi8_hdl.hw_buff_mgmt_en;
	struct edma_txcmpl_desc_8B_mode *txcmpl_ds;
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
		edma_update_ring_stats(avail_in_ring, txcmpl_ring->count,
				       &txcmpl_ring->tx_cmpl_stats.ring_stats);
	}

	avail_in_ring = min(avail_in_ring, work_to_do);
	count = avail_in_ring;

	do {
		chnk_of_reap = min(avail_in_ring, ppeds_handle->wifi8_hdl.eth_txcomp_chnk_of_reap);
		avail_cnt = idx = chnk_of_reap;

		if (likely(hw_buff_mgmt)) {
			txcmpl_ds = EDMA_TXCMPL_DESC_8B_MODE(txcmpl_ring, cons_idx);

			while (likely(idx--)) {
				ppeds_handle->wifi8_hdl.tx_cmpl_arr[avail_cnt - idx - 1].cookie = EDMA_TXCMPL_DS_OPAQUE_GET(txcmpl_ds);

				cons_idx = ((cons_idx + 1) & (txcmpl_ring->count - 1));
				txcmpl_ds = EDMA_TXCMPL_DESC_8B_MODE(txcmpl_ring, cons_idx);
			}
		} else {
			txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);

			while (likely(idx--)) {
				ppeds_handle->wifi8_hdl.tx_cmpl_arr[avail_cnt - idx - 1].cookie = EDMA_TXCMPL_OPAQUE_GET(txcmpl);

				cons_idx = ((cons_idx + 1) & (txcmpl_ring->count - 1));
				txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);
			}
		}

		/* Update Tx comp consumer index. */
		txcmpl_ring->cons_idx = cons_idx;
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id), cons_idx);

		/* Complete/replenish all the buffers. */
		ppeds_node->ops->tx_cmpl(ppeds_handle, avail_cnt);

		avail_in_ring -= chnk_of_reap;
	} while (avail_in_ring > 0);

	return count;
}

/*
 * edma_ppeds_txcomp_napi_poll()
 *	PPE-DS EDMA TX NAPI handler
 */
static int edma_ppeds_txcomp_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_txcmpl_ring *txcmpl_ring = (struct edma_txcmpl_ring *)napi;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = container_of(txcmpl_ring, struct edma_ppeds_node_wifi8, txcmpl_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t txcmpl_intr_status;
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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = container_of(rxfill_ring, struct edma_ppeds_node_wifi8, rxfill_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);
	bool hw_buff_mgmt = ppeds_node->ppeds_handle.wifi8_hdl.hw_buff_mgmt_en;
	struct edma_rxfill_desc_8B_mode *rxfill_desc_ds;
	uint32_t ring_size_mask = rxfill_ring->count - 1;
	uint32_t rx_alloc_size = rxfill_ring->alloc_size;
	struct edma_rxfill_desc *rxfill_desc;
	uint16_t num_alloc = 0;
	uint16_t prod_idx;

	/*
	 * Get RXFILL ring producer index
	 */
	prod_idx = rxfill_ring->prod_idx;

	if (hw_buff_mgmt) {
		while (likely(alloc_count--)) {
			rxfill_desc_ds = EDMA_RXFILL_DESC_8B_MODE(rxfill_ring, prod_idx);
			rxfill_desc_ds->word0 = (uint32_t)(rx_fill_arr[num_alloc].buff_addr);

#if defined(NSS_DP_HIGHMEM_SUPP)
			EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc_ds, rx_fill_arr[num_alloc].buff_addr);
#endif
			EDMA_RXFILL_DS_OPAQUE_SET(rxfill_desc_ds, rx_fill_arr[num_alloc].opaque_lo);
			EDMA_RXFILL_ENDIAN_SET(rxfill_desc_ds);
			prod_idx = (prod_idx + 1) & ring_size_mask;
			num_alloc++;
		}
	} else {
		while (likely(alloc_count--)) {
			rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, prod_idx);
			EDMA_RXFILL_BUFFER_ADDR_SET(rxfill_desc, rx_fill_arr[num_alloc].buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
			EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc, rx_fill_arr[num_alloc].buff_addr);
#endif
			rxfill_desc->word2 = rx_fill_arr[num_alloc].opaque_lo;
			rxfill_desc->word3 = rx_fill_arr[num_alloc].opaque_hi;
			EDMA_RXFILL_PACKET_LEN_SET(rxfill_desc,
					((uint32_t)(rx_alloc_size - headroom) & EDMA_RXFILL_BUF_SIZE_MASK));
			EDMA_RXFILL_ENDIAN_SET(rxfill_desc);
			prod_idx = (prod_idx + 1) & ring_size_mask;
			num_alloc++;
		}
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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = container_of(rxfill_ring, struct edma_ppeds_node_wifi8, rxfill_ring);
	struct edma_ppeds *ppeds_node = container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);
	uint32_t headroom = EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
	uint32_t alloc_size = rxfill_ring->alloc_size;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t cons_idx, work_to_do, work_done = 0;
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
				ppeds_node->ppeds_handle.wifi8_hdl.rx_fill_arr, headroom);

	/*
	 * Clear interrupt status
	 */
	edma_reg_read(EDMA_REG_RXFILL_INT_STAT(rxfill_ring->ring_id));

	work_done = num_avail;

	/*
	 * If no buffers were available, complete NAPI to avoid busy-looping
	 */
	if (!num_avail)
		goto napi_complete;

	/*
	 * Only complete NAPI if we processed less than budget
	 */
	if (work_done < budget) {
		goto napi_complete;
	}

	return work_done;

napi_complete:
	napi_complete(napi);
	if (!ppeds_node->umac_reset_inprogress) {
		edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id),
				EDMA_RXFILL_INT_MASK);
	}

	return work_done;
}

/*
 * edma_ppeds_rx_napi_poll()
 *	EDMA RX NAPI handler
 */
static int edma_ppeds_rx_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)napi;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = container_of(rxdesc_ring, struct edma_ppeds_node_wifi8, rx_ring[0]);
	struct edma_ppeds *ppeds_node = container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);
	uint16_t prod_idx;
	uint32_t status;

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


static void edma_ppeds_txcmpl_hw_buff_conf(uint32_t txcmpl_ring_id, dma_addr_t wlan_ppe2wbm_hp_addr)
{
	uint32_t reg_val = 0;
	dma_addr_t paddr;

	paddr = (uint32_t)(wlan_ppe2wbm_hp_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_UPLOAD_IDX_ADDR_L(txcmpl_ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((wlan_ppe2wbm_hp_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	reg_val = paddr;
#endif
	reg_val |= EDMA_REG_TXCMPL_UP_IDX_ENABLE;
	edma_reg_write(EDMA_REG_TXCMPL_UPLOAD_IDX_ADDR_H(txcmpl_ring_id), reg_val);

	reg_val = edma_reg_read(EDMA_REG_TXCMPL_CTRL(txcmpl_ring_id));
	reg_val |= EDMA_REG_TXCMPL_IDX_UNIT(1);
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(txcmpl_ring_id), reg_val);

	/*
	 * when HW buffer manager is enabled, Set secondary
	 * TXCMPL ring is valid for the corresponsing primary TXCMPL ring.
	 */
	reg_val = edma_reg_read(EDMA_REG_TXCMPL_SEC_RING_REG);
	reg_val |= (1 << txcmpl_ring_id);
	edma_reg_write(EDMA_REG_TXCMPL_SEC_RING_REG, reg_val);
}

static void edma_ppeds_txdesc_auto_index_conf(uint32_t tx_ring_id, dma_addr_t wlan_reo2ppe_tp_addr)
{
	uint32_t reg_val = 0;
	uint32_t paddr;

	paddr = (uint32_t)(wlan_reo2ppe_tp_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXDESC_UPLOAD_IDX_ADDR_L(tx_ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((wlan_reo2ppe_tp_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	reg_val = paddr;
#endif
	reg_val |= EDMA_REG_TXDESC_UP_IDX_ENABLE;
	edma_reg_write(EDMA_REG_TXDESC_UPLOAD_IDX_ADDR_H(tx_ring_id), reg_val);

	reg_val = edma_reg_read(EDMA_REG_TXDESC_CTRL(tx_ring_id));
	reg_val |= EDMA_REG_TXDESC_IDX_UNIT(3);
	edma_reg_write(EDMA_REG_TXDESC_CTRL(tx_ring_id), reg_val);
}

/*
 * edma_ppeds_cfg_tx()
 *	API to configure PPE-DS EDMA Tx ring
 */
static void edma_ppeds_cfg_tx(struct edma_ppeds *ppeds_node)
{
	nss_dp_ppeds_handle_t *ppeds_handle = &ppeds_node->ppeds_handle;
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_hbm_ring_hptp_cfg *hw_buf_mgmt_txrx_info = &wifi8_hdl->hw_buf_mgmt.txrx_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_hptp_cfg *txrx_info = &wifi8_hdl->data_ring.txrx_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_txcmpl_ring *txcmpl_ring;
	uint32_t paddr, saddr;
	uint8_t txdesc_entry_size;
	uint32_t tx_mod_timer;
	uint32_t data;
	int i;

	/*
	 * Configure TXDESC ring
	 */
	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		struct edma_txdesc_ring *txdesc_ring;
		txdesc_ring = &wifi8_cfg->tx_ring[i];

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

		if (wifi8_hdl->data_ring_auto_index_en) {
			edma_ppeds_txdesc_auto_index_conf(txdesc_ring->id,
				txrx_info->wlan_reo2ppe_tp_addr[i].paddr);
			txrx_info->edma_txdesc_prod_addr[i].paddr =
				(uint32_t)(edma_gbl_ctx->reg_resource->start +
						EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));
			txrx_info->edma_txdesc_prod_addr[i].vaddr =
				edma_gbl_ctx->reg_base +
				EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id);
		}

		txdesc_entry_size = wifi8_hdl->data_ring_auto_index_en ? 8 : 1;
		edma_reg_write(EDMA_REG_TXDESC_RING_SIZE(txdesc_ring->id),
				(uint32_t)((txdesc_ring->count * txdesc_entry_size) &
					EDMA_TXDESC_RING_SIZE_MASK));

		edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id), EDMA_TX_INITIAL_PROD_IDX);
	}

	/*
	 * Configure SW TxCmpl ring.
	 */
	txcmpl_ring = &wifi8_cfg->txcmpl_ring;
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

	/*
	 * Configure HW TxCmpl ring.
	 */
	if (!wifi8_hdl->hw_buff_mgmt_en) {
		return;
	}

	txcmpl_ring = &wifi8_cfg->hw_buf_mgmt.txcmpl_ring;

	paddr = (uint32_t)(txcmpl_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA(txcmpl_ring->id), paddr);
#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((txcmpl_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_TXCMPL_BA_HIGH(txcmpl_ring->id), paddr);
#endif
	edma_reg_write(EDMA_REG_TXCMPL_RING_SIZE(txcmpl_ring->id),
		(uint32_t)((txcmpl_ring->count * 2) & EDMA_TXDESC_RING_SIZE_MASK));
	/*
	 * Set TxCmpl ret mode to hwbuff mode
	 */
	data = edma_reg_read(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id));
	data |= EDMA_TXCMPL_DESC_MODE_8B;
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id), data);

	data = 0;
	data |= (nss_dp_txcmpl_fc_threshold_cnt & EDMA_TXCMPL_FC_THRE_MASK) << EDMA_TXCMPL_FC_THRE_SHIFT;
	edma_reg_write(EDMA_REG_TXCMPL_UGT_THRE(txcmpl_ring->id), data);
	edma_ppeds_txcmpl_hw_buff_conf(txcmpl_ring->id,
		hw_buf_mgmt_txrx_info->wlan_ppe2wbm_hp_addr.paddr);
	hw_buf_mgmt_txrx_info->edma_txcmpl_cons_addr.paddr =
		(uint32_t)(edma_gbl_ctx->reg_resource->start +
		EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id));
	hw_buf_mgmt_txrx_info->edma_txcmpl_cons_addr.vaddr =
		edma_gbl_ctx->reg_base +
		EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id);

	/*
	 * Configure SW TxCmpl ring to 8Byte mode if hw buffer manager is enabled.
	 */
	txcmpl_ring = &wifi8_cfg->txcmpl_ring;

	data = edma_reg_read(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id));
	data |= EDMA_TXCMPL_DESC_MODE_8B;
	edma_reg_write(EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id), data);
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

static void edma_ppeds_rxdesc_auto_index_conf(uint32_t rx_ring_id, dma_addr_t wlan_ppe2tcl_hp_addr)
{
	uint32_t reg_val = 0;
	dma_addr_t paddr;

	paddr = (uint32_t)(wlan_ppe2tcl_hp_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_RXDESC_UPLOAD_IDX_ADDR_L(rx_ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((wlan_ppe2tcl_hp_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	reg_val = paddr;
#endif
	reg_val |= EDMA_REG_RXDESC_UP_IDX_ENABLE;
	edma_reg_write(EDMA_REG_RXDESC_UPLOAD_IDX_ADDR_H(rx_ring_id), reg_val);

	reg_val = edma_reg_read(EDMA_REG_RXDESC_DISABLE(rx_ring_id));
	reg_val |= EDMA_REG_RXDESC_IDX_UNIT(3);
	edma_reg_write(EDMA_REG_RXDESC_DISABLE(rx_ring_id), reg_val);

	edma_reg_write(EDMA_REG_RXDESC_UPLOAD_IDX_TRIG(rx_ring_id),
			EDMA_REG_RXDESC_UP_IDX_TRG_VAL);
}

static void edma_ppeds_rxfill_hw_buff_conf(uint32_t rxfill_ring_id, dma_addr_t wlan_tqm2ppe_tp_addr)
{
	uint32_t reg_val = 0;
	dma_addr_t paddr;

	paddr = (uint32_t)(wlan_tqm2ppe_tp_addr & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_RXFILL_UPLOAD_IDX_ADDR_L(rxfill_ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((wlan_tqm2ppe_tp_addr >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	reg_val = paddr;
#endif
	reg_val |= EDMA_REG_RXFILL_UP_IDX_ENABLE;
	edma_reg_write(EDMA_REG_RXFILL_UPLOAD_IDX_ADDR_H(rxfill_ring_id), reg_val);

	reg_val = edma_reg_read(EDMA_REG_RXFILL_DISABLE(rxfill_ring_id));
	reg_val |= EDMA_REG_RXFILL_IDX_UNIT(1);
	edma_reg_write(EDMA_REG_RXFILL_DISABLE(rxfill_ring_id), reg_val);
}

static void edma_ppeds_rxdesc_type_ds(uint32_t rx_ring_id)
{
	uint32_t reg_val;

	reg_val = edma_reg_read(EDMA_REG_RXDESC_CTRL(rx_ring_id));
	reg_val |= EDMA_RXDESC_MODE_DS;
	edma_reg_write(EDMA_REG_RXDESC_CTRL(rx_ring_id), reg_val);
}


/*
 * edma_ppeds_cfg_rx()
 *	API to configure PPE-DS EDMA Rx ring
 */
static void edma_ppeds_cfg_rx(struct edma_ppeds *ppeds_node)
{
	nss_dp_ppeds_handle_t *ppeds_handle = &ppeds_node->ppeds_handle;
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_hbm_ring_hptp_cfg *hw_buf_mgmt_txrx_info = &wifi8_hdl->hw_buf_mgmt.txrx_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_hptp_cfg *txrx_info = &wifi8_hdl->data_ring.txrx_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	uint32_t headroom = EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *rxfill_ring;
	struct edma_rxdesc_ring *rxdesc_ring;
	uint8_t rxdesc_entry_size;
	uint32_t buffer_size;
	uint32_t ring_sz;
	uint32_t paddr;
	uint32_t data;
	int i;

	/*
	 * configure SW RXFILL ring.
	 */
	rxfill_ring = &wifi8_cfg->rxfill_ring;

	buffer_size = ((uint32_t)(rxfill_ring->alloc_size - headroom) & EDMA_RXFILL_BUF_SIZE_MASK);

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
	rxfill_ring->hw_ring_size = ring_sz;

	edma_reg_write(EDMA_REG_RXFILL_FORMAT(rxfill_ring->ring_id),
		EDMA_RXFILL_FORMAT_SET(EDMA_RXFILL_FORMAT_16B));

	rxfill_ring->prod_idx = edma_reg_read(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id));

	/*
	 * Configure flow control for SW ring
	 */
	edma_ppeds_rx_fill_ring_flow_control(rxfill_ring);

	/*
	 * Configure HW TxCmpl ring.
	 */
	if (!wifi8_hdl->hw_buff_mgmt_en) {
		goto rxdesc_cfg;
	}

	rxfill_ring = &wifi8_cfg->hw_buf_mgmt.rxfill_ring;
	paddr = (uint32_t)(rxfill_ring->dma & EDMA_RING_DMA_MASK);
	edma_reg_write(EDMA_REG_RXFILL_BA(rxfill_ring->ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
	paddr = (uint32_t)((rxfill_ring->dma >> 32) & EDMA_RING_DMA_HIGHER_MASK);
	edma_reg_write(EDMA_REG_RXFILL_BA_HIGH(rxfill_ring->ring_id), paddr);
#endif

	ring_sz = (rxfill_ring->count * 2) & EDMA_RXFILL_RING_SIZE_MASK;
	edma_reg_write(EDMA_RXFILL_RING_SIZE(rxfill_ring->ring_id), ring_sz);
	rxfill_ring->hw_ring_size = ring_sz;

	edma_reg_write(EDMA_REG_RXFILL_FORMAT(rxfill_ring->ring_id),
			EDMA_RXFILL_FORMAT_SET(EDMA_RXFILL_FORMAT_8B));

	edma_ppeds_rxfill_hw_buff_conf(rxfill_ring->ring_id,
		hw_buf_mgmt_txrx_info->wlan_tqm2ppe_tp_addr.paddr);

	/*
	 * Set Rx buffers size when rxfill ring descriptor size is 8Byte.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_SIZE(rxfill_ring->ring_id));
	data |= buffer_size;
	edma_reg_write(EDMA_REG_RXFILL_RING_SIZE(rxfill_ring->ring_id), data);

	/*
	 * Configure flow control for HW ring
	 */
	edma_ppeds_rx_fill_ring_flow_control(rxfill_ring);

	hw_buf_mgmt_txrx_info->edma_rxfill_prod_addr.paddr =
		(uint32_t)(edma_gbl_ctx->reg_resource->start +
		EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id));
	hw_buf_mgmt_txrx_info->edma_rxfill_prod_addr.vaddr =
		edma_gbl_ctx->reg_base +
		EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id);

	/*
	 * If HW buffer manager is enabled then both primary and secondary
	 * has to be in 8Byte descriptor mode.
	 */
	rxfill_ring = &wifi8_cfg->rxfill_ring;

	edma_reg_write(EDMA_REG_RXFILL_FORMAT(rxfill_ring->ring_id),
			EDMA_RXFILL_FORMAT_SET(EDMA_RXFILL_FORMAT_8B));

	/*
	 * Set Rx buffers size when rxfill ring descriptor size is 8Byte.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_SIZE(rxfill_ring->ring_id));
	data |= buffer_size;
	edma_reg_write(EDMA_REG_RXFILL_RING_SIZE(rxfill_ring->ring_id), data);


rxdesc_cfg:
	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		rxdesc_ring = &wifi8_cfg->rx_ring[i];
		paddr = (uint32_t)(rxdesc_ring->pdma & EDMA_RXDESC_BA_MASK);
		edma_reg_write(EDMA_REG_RXDESC_BA(rxdesc_ring->ring_id), paddr);

#if defined(NSS_DP_HIGHMEM_SUPP)
		paddr = (uint32_t)((rxdesc_ring->pdma >> 32) & EDMA_RXDESC_BA_HIGHER_MASK);
		edma_reg_write(EDMA_REG_RXDESC_BA_HIGH(rxdesc_ring->ring_id), paddr);
#endif

		rxdesc_entry_size = wifi8_hdl->data_ring_auto_index_en ? 8 : 1;
		data = (rxdesc_ring->count * rxdesc_entry_size) & EDMA_RXDESC_RING_SIZE_MASK;

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

		edma_ppeds_rxdesc_type_ds(rxdesc_ring->ring_id);

		if (wifi8_hdl->data_ring_auto_index_en) {
			edma_ppeds_rxdesc_auto_index_conf(rxdesc_ring->ring_id,
				txrx_info->wlan_ppe2tcl_hp_addr[i].paddr);

			txrx_info->edma_rxdesc_cons_addr[i].paddr =
				(uint32_t)(edma_gbl_ctx->reg_resource->start +
				EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->ring_id));
			txrx_info->edma_rxdesc_cons_addr[i].vaddr =
				edma_gbl_ctx->reg_base +
				EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->ring_id);
		}

		/*
		 * Enable ring. Set ret mode to 'opaque'.
		 */
		edma_reg_write(EDMA_REG_RX_INT_CTRL(rxdesc_ring->ring_id), EDMA_RX_NE_INT_EN);

		/*
		 * Configure flow control and Rx ring to queue mapping
		 */
		edma_cfg_rx_desc_ring_to_queue_mapping_conf(rxdesc_ring, true);
		edma_ppeds_rx_desc_ring_flow_control(rxdesc_ring);
	}
}

/*
 * edma_ppeds_rx_handle_irq
 *	Disable edma interrupt and enable wlan interrupt
 */
static irqreturn_t edma_ppeds_rx_handle_irq(int irq, void *ctx)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)ctx;
	struct edma_ppeds_node_wifi8 *wifi8_cfg =
			container_of(rxdesc_ring, struct edma_ppeds_node_wifi8, rx_ring[0]);
	struct edma_ppeds *ppeds_node =
			container_of(wifi8_cfg, struct edma_ppeds, wifi8_cfg);

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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxdesc_ring *rx_ring = &wifi8_cfg->rx_ring[0];
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
				  struct nss_dp_ppeds_wifi8_handle *wifi8_hdl)
{
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *sw_rxfill_ring = &wifi8_cfg->rxfill_ring;
	struct edma_txcmpl_ring *sw_txcmpl_ring = &wifi8_cfg->txcmpl_ring;
	int ret;

	ret = edma_ppeds_rx_fill_ring_alloc(sw_rxfill_ring, wifi8_hdl->hw_buff_mgmt_en);
	if (ret != 0) {
		return ret;
	}

	ret = edma_ppeds_tx_cmpl_ring_alloc(sw_txcmpl_ring, wifi8_hdl->hw_buff_mgmt_en);
	if (ret != 0) {
		edma_ppeds_rx_fill_ring_free(sw_rxfill_ring);
		return ret;
	}

	return 0;
}

/*
 * edma_ppeds_alloc_arrays()
 *	Allocate rx_fill_arr and tx_cmpl_arr
 */
static int edma_ppeds_alloc_arrays(struct edma_ppeds *ppeds_node,
				   struct nss_dp_ppeds_wifi8_handle *wifi8_hdl)
{
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	size_t rx_fill_size, tx_cmpl_size;

	rx_fill_size = sizeof(struct nss_dp_ppeds_rx_fill_elem) * wifi8_cfg->rxfill_ring.count;

	wifi8_hdl->rx_fill_arr = (struct nss_dp_ppeds_rx_fill_elem *)kzalloc(rx_fill_size, GFP_KERNEL);
	if (!wifi8_hdl->rx_fill_arr) {
		edma_err("PPEDS node[%d]: Failed to allocate rx_fill_arr\n", ppeds_node->db_idx);
		return -ENOMEM;
	}

	tx_cmpl_size = sizeof(struct nss_dp_ppeds_tx_cmpl_elem) * wifi8_cfg->txcmpl_ring.count;

	wifi8_hdl->tx_cmpl_arr = (struct nss_dp_ppeds_tx_cmpl_elem *)kzalloc(tx_cmpl_size, GFP_KERNEL);
	if (!wifi8_hdl->tx_cmpl_arr) {
		edma_err("PPEDS node[%d]: Failed to allocate tx_cmpl_arr\n", ppeds_node->db_idx);
		kfree(wifi8_hdl->rx_fill_arr);
		wifi8_hdl->rx_fill_arr = NULL;
		return -ENOMEM;
	}

	return 0;
}

/*
 * edma_ppeds_setup_irq_napi()
 *	Setup IRQ and NAPI for all rings
 */
static int edma_ppeds_setup_irq_napi(struct edma_ppeds *ppeds_node,
				     struct nss_dp_ppeds_wifi8_handle *wifi8_hdl,
				     bool data_ring_auto_index_en)
{
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	int ret, i;

	/* Setup TxComp IRQ and NAPI */
	irq_set_status_flags(wifi8_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	snprintf(wifi8_cfg->txcmpl_irq_name, 32, "edma_ppeds_txcmpl_%d", ppeds_node->db_idx);
	ret = request_irq(wifi8_cfg->txcmpl_intr, edma_tx_handle_irq, IRQF_SHARED,
			  wifi8_cfg->txcmpl_irq_name, (void *)&wifi8_cfg->txcmpl_ring);
	if (ret) {
		edma_err("PPEDS TXCMPL ring IRQ:%d request failed for node %d\n",
			 wifi8_cfg->txcmpl_intr, ppeds_node->db_idx);
		return ret;
	}

	netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi8_cfg->txcmpl_ring.napi,
			      edma_ppeds_txcomp_napi_poll, wifi8_hdl->eth_txcomp_budget);

	if (data_ring_auto_index_en)
		goto skip_rxdesc_irq;

	/* Setup RxDesc IRQ and NAPI */
	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		irq_set_status_flags(wifi8_cfg->rxdesc_intr[i], IRQ_DISABLE_UNLAZY);
		snprintf(wifi8_cfg->rxdesc_irq_name, 32, "edma_ppeds_rxdesc_%d", ppeds_node->db_idx);

		ret = request_irq(wifi8_cfg->rxdesc_intr[i], edma_ppeds_rx_handle_irq,
				IRQF_SHARED, wifi8_cfg->rxdesc_irq_name,
				(void *)&wifi8_cfg->rx_ring[i]);

		if (ret) {
			edma_err("PPEDS RXDESC ring IRQ:%d request failed for node %d\n",
				 wifi8_cfg->rxdesc_intr[i], ppeds_node->db_idx);
			goto rxdesc_irq_fail;
		}

		netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi8_cfg->rx_ring[i].napi,
				      edma_ppeds_rx_napi_poll, EDMA_PPEDS_RX_WEIGHT);
	}

skip_rxdesc_irq:
	/* Setup RxFill IRQ and NAPI */
	irq_set_status_flags(wifi8_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	snprintf(wifi8_cfg->rxfill_irq_name, EDMA_IRQ_NAME_SIZE,
		 "edma_ppeds_rxfill_%d", ppeds_node->db_idx);
	ret = request_irq(wifi8_cfg->rxfill_intr, edma_rxfill_handle_irq, IRQF_SHARED,
			  wifi8_cfg->rxfill_irq_name, (void *)&wifi8_cfg->rxfill_ring);
	if (ret) {
		edma_err("PPEDS RXFILL ring IRQ:%d request failed for node %d\n",
			 wifi8_cfg->rxfill_intr, ppeds_node->db_idx);
		goto rxfill_irq_fail;
	}

	netif_napi_add_weight(&ppeds_node->napi_ndev, &wifi8_cfg->rxfill_ring.napi,
			      edma_ppeds_rxfill_napi_poll, wifi8_hdl->eth_rxfill_budget);

	return 0;

rxfill_irq_fail:
	if (!data_ring_auto_index_en) {
		/* Clean up all previously allocated RxDesc rings */
		for (i = 0; i < ring_info->num_ppe2tcl; i++) {
			if (wifi8_cfg->rxdesc_intr[i] > 0) {
				irq_clear_status_flags(wifi8_cfg->rxdesc_intr[i], IRQ_DISABLE_UNLAZY);
				synchronize_irq(wifi8_cfg->rxdesc_intr[i]);
				free_irq(wifi8_cfg->rxdesc_intr[i], (void *)&wifi8_cfg->rx_ring[i]);
				netif_napi_del(&wifi8_cfg->rx_ring[i].napi);
			}
		}
	}
rxdesc_irq_fail:
	irq_clear_status_flags(wifi8_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi8_cfg->txcmpl_intr);
	free_irq(wifi8_cfg->txcmpl_intr, (void *)&wifi8_cfg->txcmpl_ring);
	netif_napi_del(&wifi8_cfg->txcmpl_ring.napi);

	return ret;
}

/*
 * edma_ppeds_configure_rings()
 *	Configure RxDesc mode and setup secondary Tx rings
 */
static int edma_ppeds_configure_rings(struct edma_ppeds *ppeds_node,
				      struct nss_dp_ppeds_wifi8_handle *wifi8_hdl)
{
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	int ret, i, j;

	/* Configure RxDesc mode */
	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		uint32_t rx_ring_id = wifi8_cfg->rx_ring[i].ring_id;
		uint32_t reg_val = edma_reg_read(EDMA_REG_RXDESC_CTRL(rx_ring_id));
		/* No pre-header mode */
		reg_val &= ~EDMA_RXDESC_WR_PH;
		edma_reg_write(EDMA_REG_RXDESC_CTRL(rx_ring_id), reg_val);
	}

	/* Setup secondary Tx rings */
	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		ret = edma_ppeds_tx_secondary_alloc(&wifi8_cfg->tx_ring[i]);
		if (ret) {
			edma_err("Failed to setup secondary EDMA Tx Desc Ring %d\n", i);
			/* Clean up previously allocated Tx secondary rings */
			for (j = 0; j < i; j++) {
				if (wifi8_cfg->tx_ring[j].sdesc) {
					/* Note: We don't free edma_ppeds_tx_ring_sec_mem as it's shared */
					wifi8_cfg->tx_ring[j].sdesc = NULL;
					wifi8_cfg->tx_ring[j].sdma = (dma_addr_t)0;
				}
			}
			return ret;
		}
	}

	return 0;
}

/*
 * edma_ppeds_setup_mappings()
 *	Setup Tx and Rx ring mappings
 */
static void edma_ppeds_setup_mappings(struct edma_ppeds *ppeds_node,
				      struct nss_dp_ppeds_wifi8_handle *wifi8_hdl)
{
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	int i;

	/* Setup Tx mappings */
	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		if (wifi8_hdl->hw_buff_mgmt_en) {
			edma_cfg_tx_map_tx_ring_to_txcmpl(wifi8_cfg->tx_ring[i].id,
						  wifi8_cfg->hw_buf_mgmt.txcmpl_ring.id);
		} else {
			edma_cfg_tx_map_tx_ring_to_txcmpl(wifi8_cfg->tx_ring[i].id,
						  wifi8_cfg->txcmpl_ring.id);
		}
	}

	/* Setup Rx mappings */
	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		if (wifi8_hdl->hw_buff_mgmt_en) {
			edma_ppeds_set_rx_mapping(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id,
						  wifi8_cfg->rx_ring[i].ring_id,
						  wifi8_cfg->ppe_qid[i],
						  wifi8_cfg->ppe_num_queues[i]);
		} else {
			edma_ppeds_set_rx_mapping(wifi8_cfg->rxfill_ring.ring_id,
						  wifi8_cfg->rx_ring[i].ring_id,
						  wifi8_cfg->ppe_qid[i],
						  wifi8_cfg->ppe_num_queues[i]);
		}
	}
}

static bool edma_ppeds_get_ring_info_to_node(struct edma_ppeds *ppeds_node, nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_hbm_ring_cfg *hw_buf_mgmt_ring_info = &wifi8_hdl->hw_buf_mgmt.ring_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_ds_info *ds_info = &init_info.ds_info;
	struct edma_ppeds_node_info *node_info;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t alloc_size;
	uint32_t ring_size;
	int ring_num;
	int i;

	node_info = &ds_info->ppeds_info.node_info[ppeds_node->db_idx];

	if (ring_info->num_reo2ppe > node_info->num_rx_rings) {
		edma_warn("num_rx_rings %d is less than required %d\n", node_info->num_rx_rings, ring_info->num_reo2ppe);
		return false;
	}

	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		ring_size = ring_info->ppe2tcl_num_desc[i];
		wifi8_cfg->rx_ring[i].count = ring_size;
		wifi8_cfg->rx_ring[i].count_mask = ring_size - 1;
		wifi8_cfg->rx_ring[i].pdma = (dma_addr_t)ring_info->ppe2tcl_ba[i];
	}

	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		ring_size = ring_info->reo2ppe_num_desc[i];
		wifi8_cfg->tx_ring[i].count = ring_size;
		wifi8_cfg->tx_ring[i].pdma = (dma_addr_t)ring_info->reo2ppe_ba[i];
		wifi8_cfg->tx_ring[i].pdesc = phys_to_virt(wifi8_cfg->tx_ring[i].pdma);
		memset(wifi8_cfg->tx_ring[i].pdesc, 0, 32 * ring_size);
	}

	/*
	 * if the HW buffer manager is enabled then the first ring has to given as primary ring (EDMA_PPEDS_BUFF_RING_HW)
	 * and immediate second ring for EDMA_PPEDS_BUFF_RING_SW.
	 */
	if (wifi8_hdl->hw_buff_mgmt_en) {
		struct edma_rxfill_ring_info *rxfill_info;
		struct edma_txcmpl_ring_info *txcmpl_info;

		/*
		 * Primary RXfill ring (HW managed)
		 */
		ring_num = node_info->rx_map[0].rx_fill_ring_id;
		wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id = ring_num;
		rxfill_info = &egc->rxfill_info[ring_num];
		rxfill_info->flags |= EDMA_RING_FLAGS_SEC_RING_VALID;

		/*
		 * Secondary RXfill ring (SW managed)
		 */
		ring_num = node_info->rx_map[0].rx_fill_ring_id + 1;
		wifi8_cfg->rxfill_ring.ring_id = ring_num;
		rxfill_info = &egc->rxfill_info[ring_num];
		rxfill_info->status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		rxfill_info->type_flags |= EDMA_RING_TYPE_FLAGS_DS;

		/*
		 * Primary txcmpl ring (HW managed)
		 */
		ring_num = node_info->tx_map[0].tx_cmpl_ring_id;
		wifi8_cfg->hw_buf_mgmt.txcmpl_ring.id = ring_num;
		txcmpl_info = &egc->txcmpl_info[ring_num];

		/*
		 * Secondary txcmpl ring (SW managed)
		 */
		ring_num = node_info->tx_map[0].tx_cmpl_ring_id + 1;
		wifi8_cfg->txcmpl_ring.id = ring_num;
		txcmpl_info = &egc->txcmpl_info[ring_num];
		txcmpl_info->status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		txcmpl_info->type_flags |= EDMA_RING_TYPE_FLAGS_DS;
	} else {
		ring_num = node_info->rx_map[0].rx_fill_ring_id;
		wifi8_cfg->rxfill_ring.ring_id = ring_num;

		ring_num = node_info->tx_map[0].tx_cmpl_ring_id;
		wifi8_cfg->txcmpl_ring.id = ring_num;
	}

	/*
	 * Get the rings information for HW buffer managed rings.
	 */
	ring_size = hw_buf_mgmt_ring_info->tqm2ppe_num_desc;
	wifi8_cfg->hw_buf_mgmt.rxfill_ring.count = ring_size;
	wifi8_cfg->hw_buf_mgmt.rxfill_ring.dma = hw_buf_mgmt_ring_info->tqm2ppe_ba;

	ring_size = hw_buf_mgmt_ring_info->ppe2wbm_num_desc;
	wifi8_cfg->hw_buf_mgmt.txcmpl_ring.count = ring_size;
	wifi8_cfg->hw_buf_mgmt.txcmpl_ring.dma = hw_buf_mgmt_ring_info->ppe2wbm_ba;

	/*
	 * allocate the interrupt and descrpitor only for SW managed rings.
	 */
	ring_num = wifi8_cfg->rxfill_ring.ring_id;
	wifi8_cfg->rxfill_intr = edma_gbl_ctx->rxfill_info[ring_num].intr_num;
	ring_num = wifi8_cfg->txcmpl_ring.id;
	wifi8_cfg->txcmpl_intr = edma_gbl_ctx->txcmpl_info[ring_num].intr_num;

	if (egc->rx_jumbo_mru)
		alloc_size = egc->rx_jumbo_mru;
	else
		alloc_size = NSS_DP_RX_BUFFER_SIZE;

	wifi8_cfg->rxfill_ring.count = wifi8_hdl->ppe2tcl_rxfill_num_desc;
	wifi8_cfg->rxfill_ring.count_mask = wifi8_cfg->rxfill_ring.count - 1;
	wifi8_cfg->rxfill_ring.alloc_size  = alloc_size;

	wifi8_cfg->txcmpl_ring.count = wifi8_hdl->reo2ppe_txcmpl_num_desc;

	return true;
}


/*
 * edma_ppeds_inst_register()
 *	PPE-DS EDMA instance registration API
 */
static bool edma_ppeds_inst_register(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	bool data_ring_auto_index_en;
	int ret;

	/*
	 * During umac_reset the memory dealloc/alloc will not be done.
	 * Initially instance stop is triggered to disable the rings and further
	 * re-registration happens for all the rings without memory allocation.
	 */
	if (ppeds_node->umac_reset_inprogress) {
		goto auto_idx_info_fill;
	}

	/* Get ring information from node */
	ret = edma_ppeds_get_ring_info_to_node(ppeds_node, ppeds_handle);
	if (!ret) {
		edma_err("edma_ppeds_get_ring_info failed\n");
		return false;
	}

	data_ring_auto_index_en = wifi8_hdl->data_ring_auto_index_en;

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
	ret = edma_ppeds_alloc_rings(ppeds_node, wifi8_hdl);
	if (ret != 0) {
		return false;
	}

	/* Allocate arrays */
	ret = edma_ppeds_alloc_arrays(ppeds_node, wifi8_hdl);
	if (ret != 0) {
		goto arrays_alloc_failed;
	}

	/* Setup dummy netdev for NAPIs */
	init_dummy_netdev(&ppeds_node->napi_ndev);

	/* Setup IRQ and NAPI */
	ret = edma_ppeds_setup_irq_napi(ppeds_node, wifi8_hdl, data_ring_auto_index_en);
	if (ret != 0) {
		goto irq_napi_failed;
	}

	/* Configure rings */
	ret = edma_ppeds_configure_rings(ppeds_node, wifi8_hdl);
	if (ret != 0) {
		goto configure_rings_failed;
	}

	/* Setup mappings */
	edma_ppeds_setup_mappings(ppeds_node, wifi8_hdl);

auto_idx_info_fill:
	/* Configure Tx and Rx */
	edma_ppeds_cfg_tx(ppeds_node);
	edma_ppeds_cfg_rx(ppeds_node);

	/* Update node state to registered */
	write_lock_bh(&drv->lock);
	node_cfg->node_state = EDMA_PPEDS_NODE_STATE_REG_DONE;
	write_unlock_bh(&drv->lock);

	return true;

configure_rings_failed:
	/* Cleanup IRQ and NAPI */
	irq_clear_status_flags(wifi8_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi8_cfg->rxfill_intr);
	free_irq(wifi8_cfg->rxfill_intr, (void *)&wifi8_cfg->rxfill_ring);
	netif_napi_del(&wifi8_cfg->rxfill_ring.napi);

	if (!data_ring_auto_index_en) {
		struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
		int i;
		/* Clean up all allocated RxDesc rings */
		for (i = 0; i < ring_info->num_ppe2tcl; i++) {
			if (wifi8_cfg->rxdesc_intr[i] > 0) {
				irq_clear_status_flags(wifi8_cfg->rxdesc_intr[i], IRQ_DISABLE_UNLAZY);
				synchronize_irq(wifi8_cfg->rxdesc_intr[i]);
				free_irq(wifi8_cfg->rxdesc_intr[i], (void *)&wifi8_cfg->rx_ring[i]);
				netif_napi_del(&wifi8_cfg->rx_ring[i].napi);
			}
		}
	}

	irq_clear_status_flags(wifi8_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	synchronize_irq(wifi8_cfg->txcmpl_intr);
	free_irq(wifi8_cfg->txcmpl_intr, (void *)&wifi8_cfg->txcmpl_ring);
	netif_napi_del(&wifi8_cfg->txcmpl_ring.napi);

irq_napi_failed:
	kfree(wifi8_hdl->tx_cmpl_arr);
	wifi8_hdl->tx_cmpl_arr = NULL;
	kfree(wifi8_hdl->rx_fill_arr);
	wifi8_hdl->rx_fill_arr = NULL;

arrays_alloc_failed:
	edma_ppeds_rx_fill_ring_free(&wifi8_cfg->rxfill_ring);
	edma_ppeds_tx_cmpl_ring_free(&wifi8_cfg->txcmpl_ring);

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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi8_cfg->rxfill_ring;
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

	if (count != num_avail) {
		edma_warn("Got %d less than what is asked for %d\n", num_avail, count);
	}

	edma_ppeds_rx_alloc_buffer(rxfill_ring, num_avail,
			ppeds_node->ppeds_handle.wifi8_hdl.rx_fill_arr, headroom);
}

/*
 * edma_ppeds_get_ppe_queues()
 *	Get the associated PPE queues with the given instance
 */
static bool edma_ppeds_get_ppe_queues(nss_dp_ppeds_handle_t *ppeds_handle, uint32_t *ppe_queue_start)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_ds_info *ds_info = &init_info.ds_info;
	struct edma_ppeds_drv *drv = &edma_gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	struct edma_ppeds_node_info *node_info;

	node_info = &ds_info->ppeds_info.node_info[ppeds_node->db_idx];


	read_lock_bh(&drv->lock);
	if ((node_cfg->node_state != EDMA_PPEDS_NODE_STATE_START_DONE) &&
		(node_cfg->node_state != EDMA_PPEDS_NODE_STATE_ALLOC)) {
		edma_err("%px: Invalid node state: %d, PPE-DS get queues failed\n", ppeds_node,
				node_cfg->node_state);
		read_unlock_bh(&drv->lock);
		return false;
	}
	read_unlock_bh(&drv->lock);

	*ppe_queue_start = wifi8_cfg->ppe_qid[0];

	return true;
}


/*
 * edma_ppeds_set_tx_prod_idx()
 *	Set EDMA RX producer idx
 */
static void edma_ppeds_set_tx_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle, uint16_t tx_prod_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t work_to_do = 0;
	uint32_t cons_idx;

	if (unlikely(egc->enable_ring_util_stats)) {
		cons_idx = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(wifi8_cfg->tx_ring[0].id)) & EDMA_TXDESC_CONS_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(tx_prod_idx, cons_idx, wifi8_cfg->tx_ring[0].count);
		edma_update_ring_stats(work_to_do, wifi8_cfg->tx_ring[0].count,
				       &wifi8_cfg->tx_ring[0].tx_desc_stats.ring_stats);
	}

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(wifi8_cfg->tx_ring[0].id), tx_prod_idx);
}

/*
 * edma_ppeds_set_rx_cons_idx()
 *	Set EDMA RX consumer idx
 */
static void edma_ppeds_set_rx_cons_idx(nss_dp_ppeds_handle_t *ppeds_handle, uint16_t rx_cons_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t work_to_do = 0;
	uint32_t prod_idx;

	if (unlikely(egc->enable_ring_util_stats)) {
		prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(wifi8_cfg->rx_ring[0].ring_id)) & EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx, rx_cons_idx, wifi8_cfg->rx_ring[0].count);
		edma_update_ring_stats(work_to_do, wifi8_cfg->rx_ring[0].count,
				       &wifi8_cfg->rx_ring[0].rx_desc_stats.ring_stats);
	}

	edma_reg_write(EDMA_REG_RXDESC_CONS_IDX(wifi8_cfg->rx_ring[0].ring_id), rx_cons_idx);
}

/*
 * edma_ppeds_get_rx_prod_idx()
 *	Get EDMA RX producer idx
 */
static uint16_t edma_ppeds_get_rx_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	uint16_t prod_idx;
	uint32_t data;

	data = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(wifi8_cfg->rx_ring[0].ring_id));
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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	uint16_t cons_idx;
	uint32_t data;

	data = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(wifi8_cfg->tx_ring[0].id));
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
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi8_cfg->rxfill_ring;

	return edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) &
				EDMA_RXFILL_CONS_IDX_MASK;
}

/*
 * edma_ppeds_set_rxfill_prod_idx()
 *	Set rxfill ring producer index
 */
static void edma_ppeds_set_rxfill_prod_idx(nss_dp_ppeds_handle_t *ppeds_handle,
					   uint16_t prod_idx)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *rxfill_ring = &wifi8_cfg->rxfill_ring;

	edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id), prod_idx);
}


/*
 * edma_ppeds_get_rxfill_ring_info()
 *	Get WiFi8 SW and HW RxFill ring filled-count information
 */
static void edma_ppeds_get_rxfill_ring_info(nss_dp_ppeds_handle_t *ppeds_handle,
					struct nss_dp_ppeds_rxfill_ring_info *info)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_rxfill_ring *sw_ring = &wifi8_cfg->rxfill_ring;
	struct edma_rxfill_ring *hw_ring = &wifi8_cfg->hw_buf_mgmt.rxfill_ring;
	struct nss_dp_ppeds_wifi8_rxfill_ring_info *wifi8 = &info->wifi8;

	if (info->arch_mode != EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8) {
		return;
	}

	wifi8->hw_buff_mgmt_en = !!wifi8_hdl->hw_buff_mgmt_en;

	if (!wifi8->hw_buff_mgmt_en) {
		wifi8->prim_prod_idx = sw_ring->prod_idx;
		wifi8->prim_cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(sw_ring->ring_id)) &
			EDMA_RXFILL_CONS_IDX_MASK;
		wifi8->prim_active_cnt = (wifi8->prim_prod_idx - wifi8->prim_cons_idx + sw_ring->count) &
			sw_ring->count_mask;

		wifi8->secd_prod_idx = 0;
		wifi8->secd_cons_idx = 0;
		wifi8->secd_active_cnt = 0;
		return;
	}

	/* HW RxFill ring */
	wifi8->prim_prod_idx = edma_reg_read(EDMA_REG_RXFILL_PROD_IDX(hw_ring->ring_id)) &
				EDMA_RXFILL_PROD_IDX_MASK;
	wifi8->prim_cons_idx = hw_ring->prod_idx;
	wifi8->prim_active_cnt = (wifi8->prim_prod_idx - wifi8->prim_cons_idx + hw_ring->count) &
				hw_ring->count_mask;

	/* SW RxFill ring */
	wifi8->secd_prod_idx = sw_ring->prod_idx;
	wifi8->secd_cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(sw_ring->ring_id)) &
				EDMA_RXFILL_CONS_IDX_MASK;
	wifi8->secd_active_cnt = (wifi8->secd_prod_idx - wifi8->secd_cons_idx + sw_ring->count) &
				sw_ring->count_mask;
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

	/*
	 * Due to HW WAR, 1ms of delay is done for all rings to go to DISABLE DONE.
	 * Count the rxdesc delay.
	 */
	gbl_ctx->umac_reset_delay_war_inc_cnt++;

	edma_cfg_tx_rings_disable(gbl_ctx);
	edma_disable_rx_interrupts(gbl_ctx);

}

/*
 * edma_ppeds_inst_start()
 *	PPE-DS EDMA instance start API
 */
static int edma_ppeds_inst_start(nss_dp_ppeds_handle_t *ppeds_handle, uint8_t intr_enable,
				struct nss_ppe_ds_ctx_info_handle *info_hdl)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct edma_ppeds_drv *drv = &egc->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	uint32_t data;
	int32_t i;
	struct edma_gbl_ctx *gbl_ctx = edma_gbl_ctx;

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

		/*
		 * Reset enqueue disable on all the DDRQs
		 */
                edma_ddrq_enqueue_enable_all();

		/*
		 * Enable enqueue for all the ISQs
		 */
		if (!edma_cfg_rx_ring_enq_en_mapped_queues(edma_ddrq_isq_base, EDMA_DDRQ_ISQ_NUM, true)) {
			edma_err("Error in disabling enqueue of ISQs (base: %d)\n", gbl_ctx->ddrq_def_cfg.ddrq_lp_cfg.queue_base);
		}

		/*
		 * Enabling the host rings related configurations
		 */
		edma_ppeds_host_rings_configuration_enable(gbl_ctx);

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
	edma_reg_write(EDMA_REG_RXFILL_UGT_THRE(wifi8_cfg->rxfill_ring.ring_id),
			EDMA_RXFILL_LOW_THRE_MASK & wifi8_hdl->eth_rxfill_low_thr);
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(wifi8_cfg->rxfill_ring.ring_id),
			EDMA_RXFILL_INT_MASK);
	if (!ppeds_node->umac_reset_inprogress) {
		napi_enable(&wifi8_cfg->rxfill_ring.napi);
	}

	/*
	 * Enable TxComp interrupt along with the associated NAPI
	 */
	edma_reg_write(EDMA_REG_TX_INT_MASK(wifi8_cfg->txcmpl_ring.id),
			EDMA_TX_INT_MASK_PKT_INT);
	if (!ppeds_node->umac_reset_inprogress) {
		napi_enable(&wifi8_cfg->txcmpl_ring.napi);
	}

	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		/*
		 * Enable RxDesc Ring.
		 */
		data = edma_reg_read(EDMA_REG_RXDESC_CTRL(wifi8_cfg->rx_ring[i].ring_id));
		data |= EDMA_RXDESC_RX_EN;
		edma_reg_write(EDMA_REG_RXDESC_CTRL(wifi8_cfg->rx_ring[i].ring_id), data);

		/*
		 * Reset RxDesc disable Reg.
		 */
		data = edma_reg_read(EDMA_REG_RXDESC_DISABLE(wifi8_cfg->rx_ring[i].ring_id));
		data &= ~EDMA_RXDESC_RX_DISABLE;
		edma_reg_write(EDMA_REG_RXDESC_DISABLE(wifi8_cfg->rx_ring[i].ring_id), data);
	}

	/*
	 * Enable SW RxFill Ring.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->rxfill_ring.ring_id));
	data |= EDMA_RXFILL_RING_EN;
	edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->rxfill_ring.ring_id), data);

	/*
	 * Reset SW RxFill disable Reg.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->rxfill_ring.ring_id));
	data &= ~EDMA_RXFILL_RING_DISABLE;
	edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->rxfill_ring.ring_id), data);

	if (wifi8_hdl->hw_buff_mgmt_en) {
		/*
		 * Enable HW RxFill Ring.
		 */
		data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id));
		data |= EDMA_RXFILL_RING_EN;
		edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id), data);

		/*
		 * Reset HW RxFill disable Reg.
		 */
		data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id));
		data &= ~EDMA_RXFILL_RING_DISABLE;
		edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id), data);
	}

	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		/*
		 * Enable Tx Ring.
		 */
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi8_cfg->tx_ring[i].id));
		data |= EDMA_TXDESC_TX_ENABLE;
		edma_reg_write(EDMA_REG_TXDESC_CTRL(wifi8_cfg->tx_ring[i].id), data);
	}

	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		/*
		 * If the ring reset is supported,
		 * Enable the PPE-DS node queues that are disabled at the time of inst stop.
		 */
		if (edma_dp_per_ring_reset_support()) {
			if (!edma_cfg_rx_ring_en_mapped_queues(egc, wifi8_cfg->ppe_qid[i], wifi8_cfg->ppe_num_queues[i], true)) {
				edma_err("%px: Failed to enable the queue in PPE-DS start%d qid\n", ppeds_node, wifi8_cfg->ppe_qid[i]);
			}
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
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	struct edma_gbl_ctx *gbl_ctx = edma_gbl_ctx;
	struct edma_ppeds_drv *drv = &gbl_ctx->ppeds_drv;
	struct edma_ppeds_node_cfg *node_cfg = &(drv->ppeds_node_cfg[ppeds_node->db_idx]);
	uint32_t data, i;
	uint32_t passthr_1st_pc, passthr_2nd_pc;
	uint16_t prod_idx;
	uint32_t poll_timeout = EDMA_DDRQ_HW_CONSUME_LOOP_CNT;

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
	for (i = 0; i < ring_info->num_reo2ppe; i++) {
		data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi8_cfg->tx_ring[i].id));
		data &= ~EDMA_TXDESC_TX_ENABLE;
		edma_reg_write(EDMA_REG_TXDESC_CTRL(wifi8_cfg->tx_ring[i].id), data);
		do {
			data = edma_reg_read(EDMA_REG_TXDESC_CTRL(wifi8_cfg->tx_ring[i].id));
			data &= EDMA_TXDESC_TX_ENABLE;
		} while (data);

		/*
		 * Reset the TX ring if Hardware support is present.
		 */
		if (edma_dp_per_ring_reset_support()) {
			edma_cfg_tx_ring_reset(&wifi8_cfg->tx_ring[i]);
		}
	}

	for (i = 0; i < ring_info->num_ppe2tcl; i++) {
		if (edma_dp_per_ring_reset_support()) {
			/*
			 * Disable the PPE queues corresponding to RX ring to stop the incoming
			 * traffic on the ring.
			 */
			if (!edma_cfg_rx_ring_en_mapped_queues(gbl_ctx, wifi8_cfg->ppe_qid[i], wifi8_cfg->ppe_num_queues[i], false)) {
				edma_err("%px: Failed to disable the queue in PPE-DS stop %d queue id", ppeds_node, wifi8_cfg->ppe_qid[i]);
			}
		}

		/*
		 * Clear enable bit, set disable bit and wait untill Rx Desc ring is disabled.
		 */
		data = edma_reg_read(EDMA_REG_RXDESC_CTRL(wifi8_cfg->rx_ring[i].ring_id));
		data &= ~EDMA_RXDESC_RX_EN;
		edma_reg_write(EDMA_REG_RXDESC_CTRL(wifi8_cfg->rx_ring[i].ring_id), data);

		data = edma_reg_read(EDMA_REG_RXDESC_DISABLE(wifi8_cfg->rx_ring[i].ring_id));
		data |= EDMA_RXDESC_RX_DISABLE;
		edma_reg_write(EDMA_REG_RXDESC_DISABLE(wifi8_cfg->rx_ring[i].ring_id), data);

		/*
		 * WAR: 1ms of delay is added for rxdesc rings to reach DISABLE DONE state.
		 *	DISABLE done register check is removed.
		 * TODO: need to remove after correct fix from EDMA<->DDRQ.
		 */
		mdelay(1);

		/*
		 * Reset the ring if Hardware support is present.
		 */
		if (edma_dp_per_ring_reset_support())
			edma_cfg_rx_ring_reset(&wifi8_cfg->rx_ring[i]);
	}

	/*
	 * Disable Tx complete interrupt and NAPI
	 */
	edma_reg_write(EDMA_REG_TX_INT_MASK(wifi8_cfg->txcmpl_ring.id),
			EDMA_MASK_INT_CLEAR);
	if (!ppeds_node->umac_reset_inprogress) {
		synchronize_irq(wifi8_cfg->txcmpl_intr);
		napi_disable(&wifi8_cfg->txcmpl_ring.napi);
	}

	/*
	 * Clear enable bit, set the disable bit and wait until the RxFill ring is disabled.
	 */
	data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->rxfill_ring.ring_id));
	data &= ~EDMA_RXFILL_RING_EN;
	edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->rxfill_ring.ring_id), data);

	data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->rxfill_ring.ring_id));
	data |= EDMA_RXFILL_RING_DISABLE;
	edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->rxfill_ring.ring_id), data);

	do {
		data = edma_reg_read(EDMA_REG_RXFILL_DISABLE_DONE(wifi8_cfg->rxfill_ring.ring_id));
	} while (!data);

	/*
	 * Disable Rxfill interrupt and NAPI
	 */
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(wifi8_cfg->rxfill_ring.ring_id),
			EDMA_MASK_INT_CLEAR);
	if (!ppeds_node->umac_reset_inprogress) {
		synchronize_irq(wifi8_cfg->rxfill_intr);
		napi_disable(&wifi8_cfg->rxfill_ring.napi);
	}

	if (wifi8_hdl->hw_buff_mgmt_en) {
		/*
		 * Clear enable bit, set the disable bit and wait until the HW RxFill ring is disabled.
		 */
		data = edma_reg_read(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id));
		data &= ~EDMA_RXFILL_RING_EN;
		edma_reg_write(EDMA_REG_RXFILL_RING_EN(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id), data);

		data = edma_reg_read(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id));
		data |= EDMA_RXFILL_RING_DISABLE;
		edma_reg_write(EDMA_REG_RXFILL_DISABLE(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id), data);

		do {
			data = edma_reg_read(EDMA_REG_RXFILL_DISABLE_DONE(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id));
		} while (!data);

		/*
		 * Disable Rxfill interrupt and NAPI
		 */
		edma_reg_write(EDMA_REG_RXFILL_INT_MASK(wifi8_cfg->hw_buf_mgmt.rxfill_ring.ring_id),
				EDMA_MASK_INT_CLEAR);

		/*
		 * Reset the Primary Rxfill and Tx completion ring indexes
		 * if Hardware support is present.
		 */
		if (edma_dp_per_ring_reset_support()) {
			edma_cfg_txcmpl_ring_reset(&wifi8_cfg->hw_buf_mgmt.txcmpl_ring);
			edma_cfg_rxfill_ring_reset(&wifi8_cfg->hw_buf_mgmt.rxfill_ring);
		}
	}

	if (ppeds_node->umac_reset_inprogress) {
		/*
		 * Disable EDMA host rings related configurations
		 */
		edma_ppeds_host_rings_configuration_disable(gbl_ctx);

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
		if (nss_dp_pon_drainout(NSS_DP_PON_TCONT_BITMAP_ALL, "umac_reset_wifi8") != 0)
			edma_warn("PON TCF drainout failed for wifi8\n");
#endif

		/*
		 * Enabling dequeue drop on all the DDRQs
		 */
                edma_ddrq_dequeue_drop_enable_all();

		/*
		 * Write cons_idx to max value in order for DDRQ to push all packets
		 * onto the completion ring without boundary.
		 */
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(wifi8_cfg->txcmpl_ring.id),
					wifi8_cfg->txcmpl_ring.count);

		/*
		 * Wait for 20ms and check if all packets are pushed to Tx completion ring.
		 */
		mdelay(19);
		do {
			mdelay(1);
			passthr_1st_pc = edma_reg_read(EDMA_REG_TXDESC_PASSTHR_UNREL(wifi8_cfg->tx_ring[0].id));
			passthr_2nd_pc = edma_reg_read((EDMA_REG_TXDESC_PASSTHR_UNREL(wifi8_cfg->tx_ring[0].id) + 4));
			if (--poll_timeout == 0) {
				edma_err("WARNING!!!Timeout reached - umac reset may not be gracefull\n");
				break;
			}
		} while (passthr_1st_pc != passthr_2nd_pc);

		/*
		 * Reset the PROD_IDX and CONS_IDX, once all packets are pushed to txcmpl
		 * ring.
		 */
		data = edma_reg_read(EDMA_REG_TXCMPL_PROD_IDX(wifi8_cfg->txcmpl_ring.id));
		prod_idx = data & EDMA_TXCMPL_PROD_IDX_MASK;
		edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(wifi8_cfg->txcmpl_ring.id),
					prod_idx);
		edma_debug("UMAC RESET:Txcmpl prod:%d cons:%d passthr_1st_pc=0x%x(%u) passthr_2nd_pc=0x%x(%u)\n",
				prod_idx,
				edma_reg_read(EDMA_REG_TXCMPL_CONS_IDX(wifi8_cfg->txcmpl_ring.id)),
				passthr_1st_pc, passthr_1st_pc,
				passthr_2nd_pc, passthr_2nd_pc);
	} else {
		/*
		 * Wait for 5ms and then clean the tx complete ring
		 */
		mdelay(5);
		edma_ppeds_tx_complete(wifi8_cfg->txcmpl_ring.count, &wifi8_cfg->txcmpl_ring);
	}

	edma_debug("%s: PPE-DS inst stop done: ppeds_node=%px, db_idx=%d, node_state=%d\n",
		 __func__, ppeds_node, ppeds_node->db_idx, node_cfg->node_state);

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
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &ppeds_handle->wifi8_hdl;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *ring_info = &wifi8_hdl->data_ring.ring_info;
	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
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

	kfree(wifi8_hdl->rx_fill_arr);
	wifi8_hdl->rx_fill_arr = NULL;

	kfree(wifi8_hdl->tx_cmpl_arr);
	wifi8_hdl->tx_cmpl_arr = NULL;

	if (!wifi8_hdl->data_ring_auto_index_en) {
		for (int i = 0; i < ring_info->num_ppe2tcl; i++) {
			irq_clear_status_flags(wifi8_cfg->rxdesc_intr[i], IRQ_DISABLE_UNLAZY);
			free_irq(wifi8_cfg->rxdesc_intr[i],
					(void *)&wifi8_cfg->rx_ring[i]);
			netif_napi_del(&wifi8_cfg->rx_ring[i].napi);
		}
	}

	irq_clear_status_flags(wifi8_cfg->txcmpl_intr, IRQ_DISABLE_UNLAZY);
	free_irq(wifi8_cfg->txcmpl_intr,
			(void *)&wifi8_cfg->txcmpl_ring);
	netif_napi_del(&wifi8_cfg->txcmpl_ring.napi);

	irq_clear_status_flags(wifi8_cfg->rxfill_intr, IRQ_DISABLE_UNLAZY);
	free_irq(wifi8_cfg->rxfill_intr,
			(void *)&wifi8_cfg->rxfill_ring);
	netif_napi_del(&wifi8_cfg->rxfill_ring.napi);

	if (wifi8_hdl->hw_buff_mgmt_en) {
               edma_ppeds_rx_fill_ring_free(&wifi8_cfg->hw_buf_mgmt.rxfill_ring);
               edma_ppeds_tx_cmpl_ring_free(&wifi8_cfg->hw_buf_mgmt.txcmpl_ring);
       }

	edma_ppeds_rx_fill_ring_free(&wifi8_cfg->rxfill_ring);
	edma_ppeds_tx_cmpl_ring_free(&wifi8_cfg->txcmpl_ring);

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
	uint32_t i, ring_id, j;
	struct edma_ppeds *ppeds_node;

	if (!ops || !ops->rx || !ops->rx_fill || !ops->rx_release
			|| !ops->tx_cmpl) {
		edma_err("Invalid PPE-DS operations\n");
		return NULL;
	}

	ppeds_node = (struct edma_ppeds *)kzalloc(size, GFP_KERNEL);
	if (!ppeds_node) {
		edma_err("Cannot allocate memory for ppeds node\n");
		return NULL;
	}

	ppeds_node->ops = ops;
	ppeds_node->wifi_arch_mode = EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8;
	ppeds_node->ppeds_handle.wifi_arch_mode = EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8;

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

	struct edma_ppeds_node_wifi8 *wifi8_cfg = &ppeds_node->wifi8_cfg;
	node_info = &ds_info->ppeds_info.node_info[ppeds_node->db_idx];
	wifi8_cfg->rxfill_ring.ring_id = node_info->rx_map[0].rx_fill_ring_id;
	wifi8_cfg->txcmpl_ring.id = node_info->tx_map[0].tx_cmpl_ring_id;

	for (j = 0; j < node_info->num_rx_rings; j++) {
		wifi8_cfg->rx_ring[j].ring_id = node_info->rx_map[j].rx_ring_id;
		wifi8_cfg->ppe_qid[j] = node_info->rx_map[j].ppe_queue_base;
		wifi8_cfg->ppe_num_queues[j] = node_info->num_queues_per_ring;
	}

	for (j = 0; j < node_info->num_tx_rings; j++) {
		wifi8_cfg->tx_ring[j].id = node_info->tx_map[j].tx_ring_id;
	}

	wifi8_cfg->txcmpl_intr = edma_gbl_ctx->txcmpl_info[wifi8_cfg->txcmpl_ring.id].intr_num;
	wifi8_cfg->rxfill_intr = edma_gbl_ctx->rxfill_info[wifi8_cfg->rxfill_ring.ring_id].intr_num;

	for (j = 0; j < node_info->num_rx_rings; j++) {
		wifi8_cfg->rxdesc_intr[j] = edma_gbl_ctx->rxdesc_info[wifi8_cfg->rx_ring[j].ring_id].intr_num;
		if (wifi8_cfg->rxdesc_intr[j] <= 0) {
			edma_err("Invalid interrupt numbers for PPE-DS node %d: rxdesc=%d\n",
				ppeds_node->db_idx, wifi8_cfg->rxdesc_intr[j]);
			write_unlock_bh(&drv->lock);
			kfree(ppeds_node);
			return NULL;
		}
	}

	if (wifi8_cfg->txcmpl_intr <= 0 || wifi8_cfg->rxfill_intr <= 0) {
		edma_err("Invalid interrupt numbers for PPE-DS node %d: txcmpl=%d, rxfill=%d\n",
				ppeds_node->db_idx, wifi8_cfg->txcmpl_intr,
				wifi8_cfg->rxfill_intr);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}

	rxdesc_info = egc->rxdesc_info;
	rxfill_info = egc->rxfill_info;
	txdesc_info = egc->txdesc_info;
	txcmpl_info = egc->txcmpl_info;

	ring_id = wifi8_cfg->rxfill_ring.ring_id;
	if (rxfill_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("RXfill ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	rxfill_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	rxfill_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	rxfill_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;

	for (j = 0; j < node_info->num_rx_rings; j++) {
		ring_id = wifi8_cfg->rx_ring[j].ring_id;
		if (rxdesc_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			edma_err("RX ring already in use %d\n", ring_id);
			write_unlock_bh(&drv->lock);
			kfree(ppeds_node);
			return NULL;
		}
		rxdesc_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
		rxdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		rxdesc_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;
		rxdesc_info[ring_id].ppe_queue_base = wifi8_cfg->ppe_qid[j];
		rxdesc_info[ring_id].ppe_num_queues = wifi8_cfg->ppe_num_queues[j];
	}

	for (j = 0; j < node_info->num_tx_rings; j++) {
		ring_id = wifi8_cfg->tx_ring[j].id;
		if (txdesc_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
			edma_err("TX ring already in use %d\n", ring_id);
			write_unlock_bh(&drv->lock);
			kfree(ppeds_node);
			return NULL;
		}
		txdesc_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
		txdesc_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
		txdesc_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;
	}

	ring_id = wifi8_cfg->txcmpl_ring.id;
	if (txcmpl_info[ring_id].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE) {
		edma_err("TXCMPL ring already in use %d\n", ring_id);
		write_unlock_bh(&drv->lock);
		kfree(ppeds_node);
		return NULL;
	}
	txcmpl_info[ring_id].ring_type = EDMA_RING_TYPE_DS;
	txcmpl_info[ring_id].status_flags |= EDMA_RING_STATUS_FLAGS_IN_USE;
	txcmpl_info[ring_id].type_flags |= EDMA_RING_TYPE_FLAGS_DS;

	drv->ppeds_node_cfg[i].node_state = EDMA_PPEDS_NODE_STATE_ALLOC;
	write_unlock_bh(&drv->lock);

	return &ppeds_node->ppeds_handle;
}

/*
 * edma_ppeds_ops
 *	PPE-DS operations
 */
struct nss_dp_ppeds_ops edma_ppeds_ops_wifi8 = {
	.alloc			=	edma_ppeds_inst_alloc,
	.reg			=	edma_ppeds_inst_register,
	.start			=	edma_ppeds_inst_start,
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
