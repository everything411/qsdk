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

void *edma_ppeds_tx_ring_sec_mem;
int edma_ppeds_tx_ring_entries;

/*
 * edma_ppeds_tx_secondary_alloc()
 *	API to allocate secondary Tx ring for PPE-DS node
 */
int edma_ppeds_tx_secondary_alloc(struct edma_txdesc_ring *txdesc_ring)
{
	if (edma_ppeds_tx_ring_sec_mem) {
		txdesc_ring->sdesc = edma_ppeds_tx_ring_sec_mem;
		if (edma_ppeds_tx_ring_entries < txdesc_ring->count) {
			edma_err("Num of descs needed (%d) for Tx secondary ring are more than available (%d)",
					txdesc_ring->count, edma_ppeds_tx_ring_entries);
			return -1;
		}
	} else {
		/*
		 * Allocate sencondary Tx ring descriptors
		 */
		txdesc_ring->sdesc = kmalloc(roundup((sizeof(struct edma_sec_txdesc) * txdesc_ring->count),
					SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);

		edma_ppeds_tx_ring_sec_mem = txdesc_ring->sdesc;
		edma_ppeds_tx_ring_entries = txdesc_ring->count;
	}

	if (!txdesc_ring->sdesc) {
		edma_err("Descriptor alloc for secondary TX ring %u failed\n",
				txdesc_ring->id);
		return -1;
	}

	txdesc_ring->sdma = (dma_addr_t)virt_to_phys(txdesc_ring->sdesc);
	edma_debug("tx sec desc got allocated for Tx ring %d\n", txdesc_ring->id);
	return 0;
}

/*
 * edma_ppeds_rx_fill_ring_alloc()
 *	API to allocate Rxfill ring for PPE-DS node
 */
int edma_ppeds_rx_fill_ring_alloc(struct edma_rxfill_ring *rxfill_ring, bool hw_buf_mgmt)
{
	uint32_t size;

	if (hw_buf_mgmt) {
		size = sizeof(struct edma_rxfill_desc_8B_mode);
	} else {
		size = sizeof(struct edma_rxfill_desc);
	}

	rxfill_ring->desc_size = size;
#ifdef CONFIG_IO_COHERENCY
	/*
	 * Allocate RxFill ring descriptors
	 */
	rxfill_ring->desc = kmalloc(roundup((size * rxfill_ring->count),
				SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);
	if (!rxfill_ring->desc) {
		edma_err("Cached descriptor alloc for RXFILL ring %u failed\n",
							rxfill_ring->ring_id);
		return -ENOMEM;
	}
	rxfill_ring->dma = (dma_addr_t)virt_to_phys(rxfill_ring->desc);
#else
	/*
	 * Allocate RxFill ring descriptors
	 */
	rxfill_ring->desc = dma_alloc_coherent(&edma_gbl_ctx->pdev->dev,
				(size * rxfill_ring->count),
				&rxfill_ring->dma, GFP_KERNEL | __GFP_ZERO);
	if (!rxfill_ring->desc) {
		edma_err("Descriptor alloc for RXFILL ring %u failed\n",
							rxfill_ring->ring_id);
		return -ENOMEM;
	}
#endif

	return 0;
}

/*
 * edma_ppeds_rx_fill_ring_free()
 *	API to free Rxfill ring for PPE-DS node
 */
void edma_ppeds_rx_fill_ring_free(struct edma_rxfill_ring *rxfill_ring)
{
	/*
	 * Free RXFILL ring descriptors
	 */
#ifdef CONFIG_IO_COHERENCY
	kfree(rxfill_ring->desc);
#else
	dma_free_coherent(&edma_gbl_ctx->pdev->dev,
			(rxfill_ring->desc_size * rxfill_ring->count),
			rxfill_ring->desc, rxfill_ring->dma);
#endif
	rxfill_ring->desc = NULL;
	rxfill_ring->dma = (dma_addr_t)0;
}

/*
 * edma_ppeds_tx_cmpl_ring_alloc()
 *	API to allocate Tx complete ring for PPE-DS node
 */
int edma_ppeds_tx_cmpl_ring_alloc(struct edma_txcmpl_ring *txcmpl_ring, bool hw_buf_mgmt)
{
	uint32_t size;

	if (hw_buf_mgmt) {
		size = sizeof(struct edma_txcmpl_desc_8B_mode);
	} else {
		size = sizeof(struct edma_txcmpl_desc);
	}

	txcmpl_ring->desc_size = size;
#ifdef CONFIG_IO_COHERENCY
	txcmpl_ring->desc = kmalloc(roundup((size * txcmpl_ring->count),
				SMP_CACHE_BYTES), GFP_KERNEL | __GFP_ZERO);
	if (!txcmpl_ring->desc) {
		edma_err("Cached descriptor alloc for TXCMPL ring %u failed\n",
				txcmpl_ring->id);
		return -ENOMEM;
	}
	txcmpl_ring->dma = (dma_addr_t)virt_to_phys(txcmpl_ring->desc);
#else
	txcmpl_ring->desc = dma_alloc_coherent(&edma_gbl_ctx->pdev->dev,
				(size * txcmpl_ring->count),
				&txcmpl_ring->dma, GFP_KERNEL | __GFP_ZERO);
	if (!txcmpl_ring->desc) {
		edma_err("Descriptor alloc for TXCMPL ring %u failed\n",
				txcmpl_ring->id);
		return -ENOMEM;
	}
#endif

	return 0;
}

/*
 * edma_ppeds_tx_cmpl_ring_free()
 *	API to free Tx complete ring for PPE-DS node
 */
void edma_ppeds_tx_cmpl_ring_free(struct edma_txcmpl_ring *txcmpl_ring)
{
#ifdef CONFIG_IO_COHERENCY
	kfree(txcmpl_ring->desc);
#else
	dma_free_coherent(&edma_gbl_ctx->pdev->dev,
			(txcmpl_ring->desc_size * txcmpl_ring->count),
			txcmpl_ring->desc, txcmpl_ring->dma);
#endif
	txcmpl_ring->desc = NULL;
	txcmpl_ring->dma = (dma_addr_t)0;
}

/*
 * edma_ppeds_service_status_update()
 *	Set/Unset EDMA ring usage service and notify NAPI done.
 */
void edma_ppeds_service_status_update(nss_dp_ppeds_handle_t *ppeds_handle, bool enable)
{
	struct edma_ppeds *ppeds_node = container_of(ppeds_handle, struct edma_ppeds, ppeds_handle);
	if (enable) {
		set_bit(EDMA_PPEDS_SERVICE_STOP_BIT, &ppeds_node->service_running);
	} else {
		clear_bit(EDMA_PPEDS_SERVICE_STOP_BIT, &ppeds_node->service_running);

		if ((!ppeds_node->service_running) &&
				(ppeds_node->ops->notify_napi_done)) {
			ppeds_node->ops->notify_napi_done(&ppeds_node->ppeds_handle);
		}
	}
}

/*
 * edma_ppeds_deinit()
 *	PPEDS deinit
 */
void edma_ppeds_deinit(struct edma_ppeds_drv *drv)
{
	uint32_t i;

	kfree(edma_ppeds_tx_ring_sec_mem);
	edma_ppeds_tx_ring_sec_mem = NULL;

	for (i = 0; i < EDMA_PPEDS_MAX_NODES; i++) {
		drv->ppeds_node_cfg[i].ppeds_db = NULL;
		drv->ppeds_node_cfg[i].node_state = EDMA_PPEDS_NODE_STATE_AVAIL;
	}
}

/*
 * edma_ppeds_reset_gbl_ds_ctx()
 *	Reset DS ring status and flags for all DS rings in the EDMA context
 *	This function iterates through all RX descriptor, RX fill, TX descriptor,
 *	and TX completion rings, and for those marked as DS rings with IN_USE status,
 *	it resets the IN_USE status flag and the DS type flag.
 */
void edma_ppeds_reset_gbl_ds_ctx()
{
	uint32_t i;

	struct edma_gbl_ctx *gbl_ctx = edma_gbl_ctx;

	/*
	 * Iterate through all RX descriptor rings
	 */
	for (i = 0; i < gbl_ctx->rxdesc_ring_max; i++) {
		if ((gbl_ctx->rxdesc_info[i].type_flags & EDMA_RING_TYPE_FLAGS_DS) &&
				(gbl_ctx->rxdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)) {
			gbl_ctx->rxdesc_info[i].status_flags = 0;
			gbl_ctx->rxdesc_info[i].type_flags = 0;
			edma_debug("Reset DS ring for RX descriptor ring %u\n", i);
		}
	}

	/*
	 * Iterate through all RX fill rings
	 */
	for (i = 0; i < gbl_ctx->rxfill_ring_max; i++) {
		if ((gbl_ctx->rxfill_info[i].type_flags & EDMA_RING_TYPE_FLAGS_DS) &&
				(gbl_ctx->rxfill_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)) {
			gbl_ctx->rxfill_info[i].status_flags = 0;
			gbl_ctx->rxfill_info[i].type_flags = 0;
			gbl_ctx->rxfill_info[i].flags = 0;
			edma_debug("Reset DS ring for RX fill ring %u\n", i);
		}
	}

	/*
	 * Iterate through all TX descriptor rings
	 */
	for (i = 0; i < gbl_ctx->txdesc_ring_max; i++) {
		if ((gbl_ctx->txdesc_info[i].type_flags & EDMA_RING_TYPE_FLAGS_DS) &&
				(gbl_ctx->txdesc_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)) {
			gbl_ctx->txdesc_info[i].status_flags = 0;
			gbl_ctx->txdesc_info[i].type_flags = 0;
			edma_debug("Reset DS ring for TX descriptor ring %u\n", i);
		}
	}

	/*
	 * Iterate through all TX completion rings
	 */
	for (i = 0; i < gbl_ctx->txcmpl_ring_max; i++) {
		if ((gbl_ctx->txcmpl_info[i].type_flags & EDMA_RING_TYPE_FLAGS_DS) &&
				(gbl_ctx->txcmpl_info[i].status_flags & EDMA_RING_STATUS_FLAGS_IN_USE)) {
			gbl_ctx->txcmpl_info[i].status_flags = 0;
			gbl_ctx->txcmpl_info[i].type_flags = 0;
			edma_debug("Reset DS ring for TX completion ring %u\n", i);
		}
	}
}

/*
 * edma_ppeds_init()
 *	PPEDS init
 */
int edma_ppeds_init(struct edma_ppeds_drv *drv)
{
	uint32_t i;

	rwlock_init(&drv->lock);

	for (i = 0; i < EDMA_PPEDS_MAX_NODES; i++) {
		drv->ppeds_node_cfg[i].ppeds_db = NULL;
		if (i < drv->num_nodes) {
			drv->ppeds_node_cfg[i].node_state = EDMA_PPEDS_NODE_STATE_AVAIL;
			continue;
		}
		drv->ppeds_node_cfg[i].node_state = EDMA_PPEDS_NODE_STATE_NOT_AVAIL;
	}

	return 0;
}

#ifdef NSS_DP_PON_SUPPORT
/*
 * edma_ppeds_pon_pq_disable()
 *	Disable enqueue/dequeue and flush all PON MAC PPE queues.
 *
 *	Called during UMAC reset (inst_stop) to stop PON upstream traffic
 *	from entering TCF before the TCF drainout.
 *	ppe_drv_qos_queue_disable handles enqueue disable, dequeue disable
 *	and PPE HW ESRAM flush per PQ atomically.
 */
void edma_ppeds_pon_pq_disable(void)
{
	int pon_start_pq = ppe_drv_pon_port_start_pq_get();
	int pon_max_pq = ppe_drv_pon_port_max_pq_get();
	int pq;

	/*
	 * pon_max_pq is 0 when the PON port has not been initialized
	 * (non-PON board variant). Skip silently in that case.
	 */
	if (!pon_max_pq) {
		edma_debug("PON port not initialized, skipping PQ disable\n");
		return;
	}

	for (pq = pon_start_pq; pq < (pon_start_pq + pon_max_pq); pq++) {
		if (ppe_drv_qos_queue_disable(0, pq) != PPE_DRV_RET_SUCCESS)
			edma_err("Failed to disable/flush PON PQ %d\n", pq);
	}
}

/*
 * edma_ppeds_pon_pq_enable()
 *	Re-enable enqueue and dequeue for all PON MAC PPE queues.
 *
 *	Called during UMAC reset (inst_start) after DDR queue flush
 *	is complete and normal operation resumes.
 */
void edma_ppeds_pon_pq_enable(void)
{
	int pon_start_pq = ppe_drv_pon_port_start_pq_get();
	int pon_max_pq = ppe_drv_pon_port_max_pq_get();
	int pq;

	/*
	 * pon_max_pq is 0 when the PON port has not been initialized
	 * (non-PON board variant). Skip silently in that case.
	 */
	if (!pon_max_pq) {
		edma_debug("PON port not initialized, skipping PQ enable\n");
		return;
	}

	for (pq = pon_start_pq; pq < (pon_start_pq + pon_max_pq); pq++) {
		if (ppe_drv_qos_queue_enable(pq) != PPE_DRV_RET_SUCCESS)
			edma_err("Failed to enable PON PQ %d\n", pq);
	}
}
#endif /* NSS_DP_PON_SUPPORT */

