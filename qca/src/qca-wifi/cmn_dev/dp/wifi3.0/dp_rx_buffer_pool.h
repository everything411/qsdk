/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DP_RX_BUFFER_POOL_H_
#define _DP_RX_BUFFER_POOL_H_

#include "dp_types.h"
#include "qdf_nbuf.h"
#include "qdf_module.h"
#include "athdefs.h"
#include "wlan_cfg.h"
#include "dp_internal.h"
#include "dp_rx.h"

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
/**
 * dp_rx_buffer_pool_init() - Initialize emergency buffer pool
 * @soc: SoC handle
 * @mac_id: MAC ID
 *
 * Return: None
 */
void dp_rx_buffer_pool_init(struct dp_soc *soc, u8 mac_id);

/**
 * dp_rx_buffer_pool_deinit() - De-Initialize emergency buffer pool
 * @soc: SoC handle
 * @mac_id: MAC ID
 *
 * Return: None
 */
void dp_rx_buffer_pool_deinit(struct dp_soc *soc, u8 mac_id);

/**
 * dp_rx_buffer_pool_refill() - Process the rx nbuf list and
 * refill the emergency buffer pool
 * @soc: SoC handle
 * @nbuf: RX buffer
 * @mac_id: MAC ID
 *
 * Return: Whether the rx nbuf is consumed into the pool or not.
 */
bool dp_rx_buffer_pool_refill(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id);

/**
 * dp_rx_buffer_pool_nbuf_free() - Free the nbuf or queue it
 * back into the pool
 * @soc: SoC handle
 * @nbuf: RX buffer
 * @mac_id: MAC ID
 *
 * Return: None
 */
void dp_rx_buffer_pool_nbuf_free(struct dp_soc *soc, qdf_nbuf_t nbuf,
				 u8 mac_id);

/**
 * dp_rx_buffer_pool_nbuf_alloc() - Allocate nbuf for buffer replenish,
 * give nbuf from the pool if allocation fails
 * @soc: SoC handle
 * @mac_id: MAC ID
 * @rx_desc_pool: RX descriptor pool
 * @num_available_buffers: number of available buffers in the ring.
 *
 * Return: nbuf
 */
qdf_nbuf_t dp_rx_buffer_pool_nbuf_alloc(struct dp_soc *soc, uint32_t mac_id,
					struct rx_desc_pool *rx_desc_pool,
					uint32_t num_available_buffers);

/**
 * dp_rx_buffer_pool_nbuf_map() - Map nbuff for buffer replenish
 * @soc: SoC handle
 * @rx_desc_pool: RX descriptor pool
 * @nbuf_frag_info_t: nbuf frag info
 *
 * Return: nbuf
 */
QDF_STATUS
dp_rx_buffer_pool_nbuf_map(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t);

/**
 * dp_rx_schedule_refill_thread() - Schedule RX refill thread to enqueue
 * buffers in refill pool
 * @soc: SoC handle
 *
 */
static inline void dp_rx_schedule_refill_thread(struct dp_soc *soc)
{
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	uint16_t head = buff_pool->head;
	uint16_t tail = buff_pool->tail;
	uint16_t num_refill;

	if (!buff_pool->is_initialized)
		return;

	if (tail > head)
		num_refill = (tail - head - 1);
	else
		num_refill = (buff_pool->max_bufq_len - head + tail - 1);

	if (soc->cdp_soc.ol_ops->dp_rx_sched_refill_thread &&
	    num_refill >= DP_RX_REFILL_THRD_THRESHOLD)
		soc->cdp_soc.ol_ops->dp_rx_sched_refill_thread(
						dp_soc_to_cdp_soc_t(soc));
}
#else
/**
 * dp_rx_buffer_pool_init() - Initialize emergency buffer pool
 * @soc: SoC handle
 * @mac_id: MAC ID
 *
 * Return: None
 */
static inline
void dp_rx_buffer_pool_init(struct dp_soc *soc, u8 mac_id)
{
	soc->rx_buff_pool[mac_id].is_initialized = false;
}

/**
 * dp_rx_buffer_pool_deinit() - De-Initialize emergency buffer pool
 * @soc: SoC handle
 * @mac_id: MAC ID
 *
 * Return: None
 */
static inline
void dp_rx_buffer_pool_deinit(struct dp_soc *soc, u8 mac_id)
{
}

/**
 * dp_rx_buffer_pool_refill() - Process the rx nbuf list and
 * refill the emergency buffer pool
 * @soc: SoC handle
 * @nbuf: RX buffer
 * @mac_id: MAC ID
 *
 * Return: Whether the rx nbuf is consumed into the pool or not.
 */
static inline
bool dp_rx_buffer_pool_refill(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id)
{
	return false;
}

/**
 * dp_rx_buffer_pool_nbuf_free() - Free the nbuf or queue it
 * back into the pool
 * @soc: SoC handle
 * @nbuf: RX buffer
 * @mac_id: MAC ID
 *
 * Return: None
 */
static inline
void dp_rx_buffer_pool_nbuf_free(struct dp_soc *soc, qdf_nbuf_t nbuf,
				 u8 mac_id)
{
	qdf_nbuf_free(nbuf);
}

/**
 * dp_rx_buffer_pool_nbuf_alloc() - Allocate nbuf for buffer replenish,
 * give nbuf from the pool if allocation fails
 * @soc: SoC handle
 * @mac_id: MAC ID
 * @rx_desc_pool: RX descriptor pool
 * @num_available_buffers: number of available buffers in the ring.
 *
 * Return: nbuf
 */
static inline qdf_nbuf_t
dp_rx_buffer_pool_nbuf_alloc(struct dp_soc *soc, uint32_t mac_id,
			     struct rx_desc_pool *rx_desc_pool,
			     uint32_t num_available_buffers)
{
	return qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
			      RX_BUFFER_RESERVATION,
			      rx_desc_pool->buf_alignment, FALSE);
}

/**
 * dp_rx_buffer_pool_nbuf_map() - Map nbuff for buffer replenish
 * @soc: SoC handle
 * @rx_desc_pool: RX descriptor pool
 * @nbuf_frag_info_t: nbuf frag info
 *
 * Return: nbuf
 */
static inline QDF_STATUS
dp_rx_buffer_pool_nbuf_map(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	return qdf_nbuf_map_nbytes_single(soc->osdev,
					  (nbuf_frag_info_t->virt_addr).nbuf,
					  QDF_DMA_FROM_DEVICE,
					  rx_desc_pool->buf_size);
}

static inline void dp_rx_schedule_refill_thread(struct dp_soc *soc) { }

#endif /* WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL */

#ifdef DP_FEATURE_RX_BUFFER_RECYCLE
/**
 * dp_rx_page_pool_resize() - Resize page pool dynamically
 *
 * @soc: SoC handle
 * @pool_id: page pool id
 * @new_size: new size of the page pool
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_page_pool_resize(struct dp_soc *soc, uint32_t pool_id,
				  size_t new_size);

/**
 * dp_rx_page_pool_nbuf_alloc_and_map() - Allocate and map NBUF from page pool
 *
 * @soc: SoC handle
 * @nbuf_frag_info: NBUF frag info reference
 * @mac_id: Pool ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_rx_page_pool_nbuf_alloc_and_map(struct dp_soc *soc,
				   struct dp_rx_nbuf_frag_info *nbuf_frag_info,
				   uint32_t mac_id);
/**
 * dp_rx_page_pool_deinit() - Deinit page pool parameters for @pool_id
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: void
 */
void dp_rx_page_pool_deinit(struct dp_soc *soc, uint32_t pool_id);

/**
 * dp_rx_page_pool_init() - Init page pool parameters for @pool_id
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_page_pool_init(struct dp_soc *soc, uint32_t pool_id);

/**
 * dp_rx_page_pool_free() - Free RX Page Pools
 *
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: void
 */
void dp_rx_page_pool_free(struct dp_soc *soc, uint32_t pool_id);

/**
 * dp_rx_page_pool_alloc() - Allocate Page Pools for RX buffers
 *
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 * @pool_size: Size of the buffer pool, not to be confused with page pool size
 *
 * Return: QDF_STATUS_SUCCESS for successful page pool creation
 *	   QDF_STATUS_E_FAILURE for failed page pool creation
 */
QDF_STATUS dp_rx_page_pool_alloc(struct dp_soc *soc, uint32_t pool_id,
				 uint32_t pool_size);
#else
/**
 * dp_rx_page_pool_resize() - Resize page pool dynamically
 *
 * @soc: SoC handle
 * @pool_id: page pool id
 * @new_size: new size of the page pool
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_rx_page_pool_resize(struct dp_soc *soc, uint32_t pool_id,
		       size_t new_size)
{
	return QDF_STATUS_E_NOSUPPORT;
}

/**
 * dp_rx_page_pool_nbuf_alloc_and_map() - Allocate and map NBUF from page pool
 *
 * @soc: SoC handle
 * @nbuf_frag_info: NBUF frag info reference
 * @mac_id: Pool ID
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_rx_page_pool_nbuf_alloc_and_map(struct dp_soc *soc,
				   struct dp_rx_nbuf_frag_info *nbuf_frag_info,
				   uint8_t mac_id)
{
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_rx_page_pool_deinit() - Deinit page pool parameters for @pool_id
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: void
 */
static inline void dp_rx_page_pool_deinit(struct dp_soc *soc, uint32_t pool_id)
{
}

/**
 * dp_rx_page_pool_init() - Init page pool parameters for @pool_id
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_rx_page_pool_init(struct dp_soc *soc, uint32_t pool_id)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_page_pool_free() - Free RX Page Pools
 *
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 *
 * Return: void
 */
static inline void dp_rx_page_pool_free(struct dp_soc *soc, uint32_t pool_id)
{
}

/**
 * dp_rx_page_pool_alloc() - Allocate Page Pools for RX buffers
 *
 * @soc: SoC handle
 * @pool_id: Pool ID representing the RX desc pool
 * @pool_size: Size of the buffer pool, not to be confused with page pool size
 *
 * Return: QDF_STATUS_SUCCESS for successful page pool creation
 *	   QDF_STATUS_E_FAILURE for failed page pool creation
 */
static inline QDF_STATUS
dp_rx_page_pool_alloc(struct dp_soc *soc, uint32_t pool_id, uint32_t pool_size)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* DP_FEATURE_RX_BUFFER_RECYCLE */
#endif /* _DP_RX_BUFFER_POOL_H_ */
