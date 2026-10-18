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

#include "dp_rx_buffer_pool.h"
#include "dp_ipa.h"

#ifdef DP_FEATURE_RX_BUFFER_RECYCLE
#include "qdf_page_pool.h"
#include "qdf_list.h"
#endif

#ifndef DP_RX_BUFFER_POOL_SIZE
#define DP_RX_BUFFER_POOL_SIZE 128
#endif

#ifndef DP_RX_BUFF_POOL_ALLOC_THRES
#define DP_RX_BUFF_POOL_ALLOC_THRES 1
#endif

#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
bool dp_rx_buffer_pool_refill(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	struct rx_buff_pool *bufpool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t next_nbuf, first_nbuf, refill_nbuf;
	bool consumed = false;

	if (!bufpool->is_initialized || !pdev)
		return consumed;

	/* process only buffers of RXDMA ring */
	if (soc->wlan_cfg_ctx->rxdma1_enable)
		return consumed;

	first_nbuf = nbuf;

	while (nbuf) {
		next_nbuf = qdf_nbuf_next(nbuf);

		if (qdf_likely(qdf_nbuf_queue_head_qlen(&bufpool->emerg_nbuf_q) >=
		    DP_RX_BUFFER_POOL_SIZE))
			break;

		refill_nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
					     RX_BUFFER_RESERVATION,
					     rx_desc_pool->buf_alignment,
					     FALSE);

		/* Failed to allocate new nbuf, reset and place it back
		 * in to the pool.
		 */
		if (!refill_nbuf) {
			DP_STATS_INC(pdev,
				     rx_buffer_pool.num_bufs_consumed, 1);
			consumed = true;
			break;
		}

		/* Successful allocation!! */
		DP_STATS_INC(pdev,
			     rx_buffer_pool.num_bufs_alloc_success, 1);
		qdf_nbuf_queue_head_enqueue_tail(&bufpool->emerg_nbuf_q,
						 refill_nbuf);
		nbuf = next_nbuf;
	}

	nbuf = first_nbuf;
	if (consumed) {
		/* Free the MSDU/scattered MSDU */
		while (nbuf) {
			next_nbuf = qdf_nbuf_next(nbuf);
			dp_rx_buffer_pool_nbuf_free(soc, nbuf, mac_id);
			nbuf = next_nbuf;
		}
	}

	return consumed;
}

void dp_rx_buffer_pool_nbuf_free(struct dp_soc *soc, qdf_nbuf_t nbuf, u8 mac_id)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_desc_pool *rx_desc_pool;
	struct rx_buff_pool *buff_pool;

	if (!wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
		mac_id = dp_pdev->lmac_id;

	rx_desc_pool = &soc->rx_desc_buf[mac_id];
	buff_pool = &soc->rx_buff_pool[mac_id];

	if (qdf_likely(qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q) >=
		       DP_RX_BUFFER_POOL_SIZE) ||
	    !buff_pool->is_initialized)
		return qdf_nbuf_free(nbuf);

	qdf_nbuf_reset(nbuf, RX_BUFFER_RESERVATION,
		       rx_desc_pool->buf_alignment);
	qdf_nbuf_queue_head_enqueue_tail(&buff_pool->emerg_nbuf_q, nbuf);
}

void dp_rx_refill_buff_pool_enqueue(struct dp_soc *soc)
{
	struct rx_desc_pool *rx_desc_pool;
	struct rx_refill_buff_pool *buff_pool;
	qdf_device_t dev;
	qdf_nbuf_t nbuf;
	QDF_STATUS ret;
	int count, i;
	uint16_t num_refill;
	uint16_t total_num_refill;
	uint16_t total_count = 0;
	uint16_t head, tail;

	if (!soc)
		return;

	dev = soc->osdev;
	buff_pool = &soc->rx_refill_buff_pool;
	rx_desc_pool = &soc->rx_desc_buf[0];
	if (!buff_pool->is_initialized)
		return;

	head = buff_pool->head;
	tail = buff_pool->tail;
	if (tail > head)
		total_num_refill = (tail - head - 1);
	else
		total_num_refill = (buff_pool->max_bufq_len - head +
				    tail - 1);

	while (total_num_refill) {
		if (total_num_refill > DP_RX_REFILL_BUFF_POOL_BURST)
			num_refill = DP_RX_REFILL_BUFF_POOL_BURST;
		else
			num_refill = total_num_refill;

		count = 0;
		for (i = 0; i < num_refill; i++) {
			nbuf = qdf_nbuf_alloc(dev, rx_desc_pool->buf_size,
					      RX_BUFFER_RESERVATION,
					      rx_desc_pool->buf_alignment,
					      FALSE);
			if (qdf_unlikely(!nbuf))
				continue;

			ret = qdf_nbuf_map_nbytes_single(dev, nbuf,
							 QDF_DMA_FROM_DEVICE,
							 rx_desc_pool->buf_size);
			if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
				qdf_nbuf_free(nbuf);
				continue;
			}

			buff_pool->buf_elem[head++] = nbuf;
			head &= (buff_pool->max_bufq_len - 1);
			count++;
		}

		if (count) {
			buff_pool->head = head;
			total_num_refill -= count;
			total_count += count;
		}
	}

	DP_STATS_INC(buff_pool->dp_pdev,
		     rx_refill_buff_pool.num_bufs_refilled,
		     total_count);
}

static inline qdf_nbuf_t dp_rx_refill_buff_pool_dequeue_nbuf(struct dp_soc *soc)
{
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	qdf_nbuf_t nbuf = NULL;
	uint16_t head, tail;

	head = buff_pool->head;
	tail = buff_pool->tail;

	if (head == tail)
		return NULL;

	nbuf = buff_pool->buf_elem[tail++];
	tail &= (buff_pool->max_bufq_len - 1);
	buff_pool->tail = tail;

	return nbuf;
}

qdf_nbuf_t
dp_rx_buffer_pool_nbuf_alloc(struct dp_soc *soc, uint32_t mac_id,
			     struct rx_desc_pool *rx_desc_pool,
			     uint32_t num_available_buffers)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct rx_buff_pool *buff_pool;
	struct dp_srng *dp_rxdma_srng;
	qdf_nbuf_t nbuf;

	nbuf = dp_rx_refill_buff_pool_dequeue_nbuf(soc);
	if (qdf_likely(nbuf)) {
		DP_STATS_INC(dp_pdev,
			     rx_refill_buff_pool.num_bufs_allocated, 1);
		return nbuf;
	}

	if (!wlan_cfg_per_pdev_lmac_ring(soc->wlan_cfg_ctx))
		mac_id = dp_pdev->lmac_id;

	buff_pool = &soc->rx_buff_pool[mac_id];
	dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_id];

	nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
			      RX_BUFFER_RESERVATION,
			      rx_desc_pool->buf_alignment,
			      FALSE);

	if (!buff_pool->is_initialized)
		return nbuf;

	if (qdf_likely(nbuf)) {
		buff_pool->nbuf_fail_cnt = 0;
		return nbuf;
	}

	buff_pool->nbuf_fail_cnt++;

	/* Allocate buffer from the buffer pool */
	if (buff_pool->nbuf_fail_cnt >= DP_RX_BUFFER_POOL_ALLOC_THRES ||
	    (num_available_buffers < dp_rxdma_srng->num_entries / 10)) {
		nbuf = qdf_nbuf_queue_head_dequeue(&buff_pool->emerg_nbuf_q);
		if (nbuf)
			DP_STATS_INC(dp_pdev,
				     rx_buffer_pool.num_pool_bufs_replenish, 1);
	}

	return nbuf;
}

QDF_STATUS
dp_rx_buffer_pool_nbuf_map(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (!QDF_NBUF_CB_PADDR((nbuf_frag_info_t->virt_addr).nbuf))
		ret = qdf_nbuf_map_nbytes_single(soc->osdev,
						 (nbuf_frag_info_t->virt_addr).nbuf,
						 QDF_DMA_FROM_DEVICE,
						 rx_desc_pool->buf_size);

	return ret;
}

static void dp_rx_refill_buff_pool_init(struct dp_soc *soc, u8 mac_id)
{
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	qdf_nbuf_t nbuf;
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	QDF_STATUS ret;
	uint16_t head = 0;
	int i;

	if (!wlan_cfg_is_rx_refill_buffer_pool_enabled(soc->wlan_cfg_ctx)) {
		dp_err("RX refill buffer pool support is disabled");
		buff_pool->is_initialized = false;
		return;
	}

	buff_pool->max_bufq_len =
		wlan_cfg_get_rx_refill_buf_pool_size(soc->wlan_cfg_ctx);

	buff_pool->buf_elem = qdf_mem_malloc(buff_pool->max_bufq_len *
					     sizeof(qdf_nbuf_t));
	if (!buff_pool->buf_elem) {
		dp_err("Failed to allocate memory for RX refill buf element");
		buff_pool->is_initialized = false;
		return;
	}

	buff_pool->dp_pdev = dp_get_pdev_for_lmac_id(soc, 0);
	buff_pool->tail = 0;

	for (i = 0; i < (buff_pool->max_bufq_len - 1); i++) {
		nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
				      RX_BUFFER_RESERVATION,
				      rx_desc_pool->buf_alignment, FALSE);
		if (!nbuf)
			continue;

		ret = qdf_nbuf_map_nbytes_single(soc->osdev, nbuf,
						 QDF_DMA_FROM_DEVICE,
						 rx_desc_pool->buf_size);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
			qdf_nbuf_free(nbuf);
			continue;
		}

		buff_pool->buf_elem[head] = nbuf;
		head++;
	}

	buff_pool->head =  head;

	dp_info("RX refill buffer pool required allocation: %u actual allocation: %u",
		buff_pool->max_bufq_len,
		buff_pool->head);

	buff_pool->is_initialized = true;
}

void dp_rx_buffer_pool_init(struct dp_soc *soc, u8 mac_id)
{
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	struct rx_buff_pool *buff_pool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t nbuf;
	int i;

	dp_rx_refill_buff_pool_init(soc, mac_id);

	if (!wlan_cfg_is_rx_buffer_pool_enabled(soc->wlan_cfg_ctx)) {
		dp_info("RX buffer pool support is disabled");
		buff_pool->is_initialized = false;
		return;
	}

	if (buff_pool->is_initialized)
		return;

	qdf_nbuf_queue_head_init(&buff_pool->emerg_nbuf_q);

	for (i = 0; i < DP_RX_BUFFER_POOL_SIZE; i++) {
		nbuf = qdf_nbuf_alloc(soc->osdev, rx_desc_pool->buf_size,
				      RX_BUFFER_RESERVATION,
				      rx_desc_pool->buf_alignment, FALSE);
		if (!nbuf)
			continue;
		qdf_nbuf_queue_head_enqueue_tail(&buff_pool->emerg_nbuf_q,
						 nbuf);
	}

	dp_info("RX buffer pool required allocation: %u actual allocation: %u",
		DP_RX_BUFFER_POOL_SIZE,
		qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q));

	buff_pool->is_initialized = true;
}

static void dp_rx_refill_buff_pool_deinit(struct dp_soc *soc, u8 mac_id)
{
	struct rx_refill_buff_pool *buff_pool = &soc->rx_refill_buff_pool;
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	qdf_nbuf_t nbuf;
	uint32_t count = 0;

	if (!buff_pool->is_initialized)
		return;

	while ((nbuf = dp_rx_refill_buff_pool_dequeue_nbuf(soc))) {
		qdf_nbuf_unmap_nbytes_single(soc->osdev, nbuf,
					     QDF_DMA_BIDIRECTIONAL,
					     rx_desc_pool->buf_size);
		qdf_nbuf_free(nbuf);
		count++;
	}

	dp_info("Rx refill buffers freed during deinit %u head: %u, tail: %u",
		count, buff_pool->head, buff_pool->tail);

	qdf_mem_free(buff_pool->buf_elem);
	buff_pool->is_initialized = false;
}

void dp_rx_buffer_pool_deinit(struct dp_soc *soc, u8 mac_id)
{
	struct rx_buff_pool *buff_pool = &soc->rx_buff_pool[mac_id];
	qdf_nbuf_t nbuf;

	dp_rx_refill_buff_pool_deinit(soc, mac_id);

	if (!buff_pool->is_initialized)
		return;

	dp_info("buffers in the RX buffer pool during deinit: %u",
		qdf_nbuf_queue_head_qlen(&buff_pool->emerg_nbuf_q));

	while ((nbuf = qdf_nbuf_queue_head_dequeue(&buff_pool->emerg_nbuf_q)))
		qdf_nbuf_free(nbuf);

	buff_pool->is_initialized = false;
}
#endif /* WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL */

#ifdef DP_FEATURE_RX_BUFFER_RECYCLE

#define DP_RX_PP_PAGE_SIZE_HIGHER_ORDER		(2 * DP_RX_PP_PAGE_SIZE_MIDDLE_ORDER)
#define DP_RX_PP_PAGE_SIZE_MIDDLE_ORDER		(4 * DP_RX_PP_PAGE_SIZE_LOWER_ORDER)
#define DP_RX_PP_PAGE_SIZE_LOWER_ORDER		PAGE_SIZE

#define DP_RX_PP_POOL_SIZE_THRES	 4096
#define DP_RX_PP_AUX_POOL_SIZE           2048
#define DP_RX_PP_INACTIVE_TIMER_MS	10000

static struct dp_rx_pp_params *
dp_rx_get_base_pp(struct dp_rx_page_pool *rx_pp)
{
	struct dp_rx_pp_params *pp_params;

	/* Page Pool at 0th index is base page pool */
	pp_params = &rx_pp->main_pool[0];
	if (pp_params->pp && !qdf_page_pool_empty(pp_params->pp))
		return pp_params;

	pp_params = &rx_pp->aux_pool;
	if (pp_params->pp && !qdf_page_pool_empty(pp_params->pp))
		return pp_params;

	return NULL;
}

static QDF_STATUS
dp_rx_page_pool_check_pages_availability(qdf_page_pool_t pp,
					 uint32_t pool_size,
					 size_t page_size)
{
	qdf_page_t *pages_list;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t offset;
	int i;

	/*
	 * Get and put pages from page pool to make sure,
	 * no map failures will be encountered later during
	 * actual allocation. This will also make sure memory
	 * allocation failures will not be encountered.
	 */
	if (!pp) {
		dp_err("Invalid PP params passed");
		return QDF_STATUS_E_INVAL;
	}

	pages_list = qdf_mem_malloc(pool_size *
				    sizeof(qdf_page_t));
	if (!pages_list)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < pool_size; i++) {
		pages_list[i] = qdf_page_pool_alloc_frag(pp, &offset,
							 page_size);
		if (!pages_list[i]) {
			dp_err("page alloc failed for idx:%u", i);
			ret = QDF_STATUS_E_FAILURE;
			goto out_put_page;
		}
	}

out_put_page:
	for (i = 0; i < pool_size; i++) {
		if (!pages_list[i])
			continue;

		qdf_page_pool_put_page(pp,
				       pages_list[i], false);
	}

	qdf_mem_free(pages_list);

	return ret;
}

QDF_STATUS
dp_rx_page_pool_nbuf_alloc_and_map(struct dp_soc *soc,
				   struct dp_rx_nbuf_frag_info *nbuf_frag_info,
				   uint32_t mac_id)
{
	struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[mac_id];
	struct dp_rx_pp_params *pp_params;
	qdf_page_t page;
	qdf_nbuf_t nbuf;
	uint32_t offset;
	QDF_STATUS ret;
	int i;

	if (!wlan_cfg_get_dp_rx_buffer_recycle(soc->wlan_cfg_ctx))
		return QDF_STATUS_E_FAILURE;

	if (qdf_atomic_read(&rx_pp->update_in_progress)) {
		pp_params = dp_rx_get_base_pp(rx_pp);
		if (!pp_params)
			return QDF_STATUS_E_FAILURE;

		qdf_spin_lock_bh(&rx_pp->pp_lock);
		goto nbuf_alloc;
	}

	qdf_spin_lock_bh(&rx_pp->pp_lock);

	pp_params = &rx_pp->main_pool[rx_pp->active_pp_idx];
	if (pp_params->pp && !qdf_page_pool_empty(pp_params->pp))
		goto nbuf_alloc;

	for (i = 0; i < DP_PAGE_POOL_MAX; i++) {
		pp_params = &rx_pp->main_pool[i];

		if (!pp_params->pp ||
		    qdf_page_pool_empty(pp_params->pp))
			continue;

		rx_pp->active_pp_idx = i;
		goto nbuf_alloc;
	}

	pp_params = &rx_pp->aux_pool;
	if (pp_params->pp && qdf_page_pool_empty(pp_params->pp)) {
		ret = QDF_STATUS_E_FAILURE;
		goto out_fail;
	}

nbuf_alloc:
	nbuf = qdf_nbuf_page_pool_alloc(soc->osdev, rx_desc_pool->buf_size,
					RX_BUFFER_RESERVATION,
					rx_desc_pool->buf_alignment,
					pp_params->pp, &offset);
	if (!nbuf) {
		ret = QDF_STATUS_E_FAILURE;
		goto out_fail;
	}

	page = qdf_virt_to_head_page(nbuf->data);
	nbuf_frag_info->paddr = QDF_NBUF_CB_PADDR(nbuf) =
		qdf_page_pool_get_dma_addr(page) + offset +
		qdf_nbuf_headroom(nbuf);

	(nbuf_frag_info->virt_addr).nbuf = nbuf;

	ret = qdf_nbuf_track_map_single(soc->osdev, nbuf, QDF_DMA_FROM_DEVICE);
	if (!QDF_IS_STATUS_SUCCESS(ret)) {
		qdf_nbuf_free(nbuf);
		goto out_fail;
	}

	dp_ipa_handle_rx_buf_smmu_mapping(soc, nbuf,
					  rx_desc_pool->buf_size,
					  true, __func__, __LINE__,
					  DP_RX_IPA_SMMU_MAP_REPLENISH);

	dp_audio_smmu_map(soc, nbuf, rx_desc_pool->buf_size);

	qdf_spin_unlock_bh(&rx_pp->pp_lock);

	return QDF_STATUS_SUCCESS;

out_fail:
	qdf_spin_unlock_bh(&rx_pp->pp_lock);
	return ret;
}

static void dp_rx_page_pool_inactive_timer(void *arg)
{
	struct dp_rx_page_pool *rx_pp = (struct dp_rx_page_pool *)arg;
	struct dp_rx_pp_params *curr, *next;

	qdf_spin_lock_bh(&rx_pp->pp_lock);
	qdf_list_for_each_del(&rx_pp->inactive_list, curr, next, node) {
		if (!curr->pp)
			continue;

		if (!qdf_page_pool_full_bh(curr->pp))
			continue;

		qdf_page_pool_destroy(curr->pp);
		qdf_list_remove_node(&rx_pp->inactive_list, &curr->node);
		qdf_mem_free(curr);
	}
	qdf_spin_unlock_bh(&rx_pp->pp_lock);

	if (!qdf_list_empty(&rx_pp->inactive_list))
		qdf_timer_mod(&rx_pp->pool_inactivity_timer,
			      DP_RX_PP_INACTIVE_TIMER_MS);
}

void dp_rx_page_pool_deinit(struct dp_soc *soc, uint32_t pool_id)
{
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[pool_id];
	struct dp_rx_pp_params *pp_params;
	int i;

	rx_pp->active_pp_idx = 0;

	qdf_spin_lock(&rx_pp->pp_lock);
	for (i = 0; i < DP_PAGE_POOL_MAX; i++) {
		pp_params = &rx_pp->main_pool[i];

		if (!pp_params->pp)
			continue;

		pp_params->pool_size = 0;
		pp_params->pp_size = 0;
	}

	rx_pp->aux_pool.pool_size = 0;
	rx_pp->aux_pool.pp_size = 0;
	qdf_spin_unlock(&rx_pp->pp_lock);

	qdf_timer_free(&rx_pp->pool_inactivity_timer);
}

QDF_STATUS dp_rx_page_pool_init(struct dp_soc *soc, uint32_t pool_id)
{
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[pool_id];

	if (!wlan_cfg_get_dp_rx_buffer_recycle(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	qdf_atomic_init(&rx_pp->update_in_progress);
	rx_pp->active_pp_idx = 0;

	qdf_timer_init(soc->osdev, &rx_pp->pool_inactivity_timer,
		       dp_rx_page_pool_inactive_timer, (void *)rx_pp,
		       QDF_TIMER_TYPE_WAKE_APPS);
	qdf_list_create(&rx_pp->inactive_list, 0);

	return QDF_STATUS_SUCCESS;
}

void dp_rx_page_pool_free(struct dp_soc *soc, uint32_t pool_id)
{
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[pool_id];
	struct dp_rx_pp_params *pp_params;
	struct dp_rx_pp_params *curr, *next;
	int i;

	if (!wlan_cfg_get_dp_rx_buffer_recycle(soc->wlan_cfg_ctx))
		return;

	dp_rx_page_pool_deinit(soc, pool_id);
	qdf_spin_lock_bh(&rx_pp->pp_lock);
	for (i = 0; i < DP_PAGE_POOL_MAX; i++) {
		pp_params = &rx_pp->main_pool[i];

		if (!pp_params->pp)
			continue;

		qdf_page_pool_destroy(pp_params->pp);
		pp_params->pp = NULL;
	}

	if (rx_pp->aux_pool.pp) {
		qdf_page_pool_destroy(rx_pp->aux_pool.pp);
		rx_pp->aux_pool.pp = NULL;
	}

	qdf_list_for_each_del(&rx_pp->inactive_list, curr, next, node) {
		if (!curr->pp)
			continue;

		qdf_page_pool_destroy(curr->pp);
		qdf_list_remove_node(&rx_pp->inactive_list, &curr->node);
		qdf_mem_free(curr);
	}
	qdf_spin_unlock_bh(&rx_pp->pp_lock);

	qdf_spinlock_destroy(&rx_pp->pp_lock);
}

static qdf_page_pool_t
__dp_rx_page_pool_create(struct dp_soc *soc, uint32_t pool_size,
			 size_t buf_size, size_t *page_size,
			 size_t *pp_size)
{
	qdf_page_pool_t pp;
	size_t bufs_per_page;
	QDF_STATUS status;

	*page_size = DP_RX_PP_PAGE_SIZE_HIGHER_ORDER;
alloc_page_pool:
	bufs_per_page = *page_size / buf_size;
	*pp_size = pool_size / bufs_per_page;
	if (pool_size % bufs_per_page)
		*pp_size = (*pp_size + 1);

	pp = qdf_page_pool_create(soc->osdev, *pp_size,
				  *page_size);
	if (!pp) {
		dp_err("Failed to create page pool");
		return NULL;
	}

	status = dp_rx_page_pool_check_pages_availability(pp, *pp_size,
							  *page_size);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_info("page pool resources not available for page_size:%zu",
			*page_size);
		qdf_page_pool_destroy(pp);
		pp = NULL;

		switch (*page_size) {
		case DP_RX_PP_PAGE_SIZE_HIGHER_ORDER:
			*page_size = DP_RX_PP_PAGE_SIZE_MIDDLE_ORDER;
			goto alloc_page_pool;
		case DP_RX_PP_PAGE_SIZE_MIDDLE_ORDER:
			*page_size = DP_RX_PP_PAGE_SIZE_LOWER_ORDER;
			goto alloc_page_pool;
		case DP_RX_PP_PAGE_SIZE_LOWER_ORDER:
			break;
		}
	}

	return pp;
}

QDF_STATUS dp_rx_page_pool_alloc(struct dp_soc *soc, uint32_t pool_id,
				 uint32_t pool_size)
{
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[pool_id];
	struct dp_rx_pp_params *pp_params;
	uint64_t req_rx_buffers = 0;
	uint64_t in_use_rx_buffers = 0;
	size_t page_size;
	qdf_page_pool_t pp;
	size_t buf_size;
	size_t rem_size;
	size_t pp_size;
	int pp_count;
	int i;

	if (!wlan_cfg_get_dp_rx_buffer_recycle(soc->wlan_cfg_ctx)) {
		dp_err("RX buffer recycle disabled from INI");
		return QDF_STATUS_E_FAILURE;
	}

	dp_rx_get_num_buff_descs_info((struct cdp_soc_t *)soc,
				      &req_rx_buffers, &in_use_rx_buffers,
				      pool_id);

	if (!rx_pp->base_pool_size) {
		if (req_rx_buffers) {
			rx_pp->base_pool_size = req_rx_buffers;
			pool_size = req_rx_buffers;
		} else {
			rx_pp->base_pool_size = DP_RX_PP_POOL_SIZE_THRES;
		}
	}

	if (pool_size > rx_pp->base_pool_size) {
		pp_count = pool_size / rx_pp->base_pool_size;
		rem_size = pool_size % rx_pp->base_pool_size;

		if (rem_size)
			pp_count++;
	} else {
		pp_count = 1;
		rem_size = pool_size;
	}

	if (pp_count > DP_PAGE_POOL_MAX) {
		dp_err("Failed to allocate page pools, invalid pool count %d",
		       pp_count);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spinlock_create(&rx_pp->pp_lock);

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	if (RX_DATA_BUFFER_OPT_ALIGNMENT)
		buf_size += RX_DATA_BUFFER_OPT_ALIGNMENT - 1;
	buf_size += QDF_SHINFO_SIZE;
	buf_size = QDF_NBUF_ALIGN(buf_size);

	for (i = 0; i < pp_count; i++) {
		pp_params = &rx_pp->main_pool[i];
		pool_size = DP_RX_PP_POOL_SIZE_THRES;

		if (i == pp_count - 1 && rem_size)
			pool_size = rem_size;

		pp = __dp_rx_page_pool_create(soc, pool_size,
					      buf_size, &page_size,
					      &pp_size);
		if (!pp)
			goto out_pp_fail;

		pp_params->pp = pp;
		pp_params->pool_size = pool_size;
		pp_params->pp_size = pp_size;

		dp_info("Page pool idx %d pool_size %d pp_size %zu", i,
			pool_size, pp_size);
	}

	rx_pp->aux_pool.pool_size = DP_RX_PP_AUX_POOL_SIZE;
	rx_pp->aux_pool.pp = __dp_rx_page_pool_create(soc,
						      rx_pp->aux_pool.pool_size,
						      buf_size, &page_size,
						      &pp_size);
	if (!rx_pp->aux_pool.pp)
		goto out_pp_fail;

	rx_pp->aux_pool.pp_size = pp_size;
	rx_pp->curr_pool_size = pool_size;

	if (QDF_IS_STATUS_ERROR(dp_rx_page_pool_init(soc, pool_id)))
		goto out_pp_fail;

	return QDF_STATUS_SUCCESS;

out_pp_fail:
	dp_rx_page_pool_free(soc, pool_id);
	return QDF_STATUS_E_FAILURE;
}

static bool
dp_rx_page_pool_reattach(struct dp_rx_page_pool *rx_pp,
			 struct dp_rx_pp_params *pp_params, size_t pool_size)
{
	struct dp_rx_pp_params *curr, *next;
	bool attach = false;

	qdf_spin_lock_bh(&rx_pp->pp_lock);
	qdf_list_for_each_del(&rx_pp->inactive_list, curr, next, node) {
		if (!curr->pp)
			continue;

		if (curr->pool_size != pool_size)
			continue;

		qdf_list_remove_node(&rx_pp->inactive_list, &curr->node);
		qdf_mem_copy(pp_params, curr, sizeof(*curr));
		qdf_mem_free(curr);
		attach = true;
		break;
	}
	qdf_spin_unlock_bh(&rx_pp->pp_lock);

	return attach;
}

static QDF_STATUS
dp_rx_page_pool_upsize(struct dp_soc *soc, struct dp_rx_page_pool *rx_pp,
		       size_t new_size)
{
	struct dp_rx_pp_params *pp_params;
	qdf_page_pool_t pp;
	size_t rem_size;
	size_t pp_count;
	size_t buf_size;
	size_t pp_size;
	uint16_t upscale_cnt = 1;
	uint32_t pool_size;
	size_t page_size;
	int i;

	pp_count = new_size / rx_pp->base_pool_size;
	rem_size = new_size % rx_pp->base_pool_size;
	if (rem_size)
		pp_count++;

	if (pp_count > DP_PAGE_POOL_MAX) {
		dp_err("Failed to allocate page pools, invalid pool count %zu",
		       pp_count);
		return QDF_STATUS_E_FAILURE;
	}

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	if (RX_DATA_BUFFER_OPT_ALIGNMENT)
		buf_size += RX_DATA_BUFFER_OPT_ALIGNMENT - 1;
	buf_size += QDF_SHINFO_SIZE;
	buf_size = QDF_NBUF_ALIGN(buf_size);

	/* Base page pool at 0th index is always present,
	 * so allocate page pools from 1st index.
	 */
	for (i = 1; i < pp_count; i++) {
		pp_params = &rx_pp->main_pool[i];
		if (pp_params->pp)
			continue;

		pool_size = rx_pp->base_pool_size;

		if (i == pp_count - 1 && rem_size)
			pool_size = rem_size;

		/* Try to rettach pools which are inactive first
		 * before allocating new pools.
		 */
		if (dp_rx_page_pool_reattach(rx_pp, pp_params, pool_size)) {
			upscale_cnt++;
			continue;
		}

		pp = __dp_rx_page_pool_create(soc, pool_size,
					      buf_size, &page_size,
					      &pp_size);
		if (!pp)
			goto out_pp_fail;

		pp_params->pp = pp;
		pp_params->pool_size = pool_size;
		pp_params->pp_size = pp_size;
		upscale_cnt++;

		dp_info("Page pool idx %d pool_size %d pp_size %zu", i,
			pool_size, pp_size);
	}

	if (upscale_cnt != pp_count) {
		dp_err("Failed to upscale RX buffers using page pool");
		goto out_pp_fail;
	}

	return QDF_STATUS_SUCCESS;

out_pp_fail:
	while (i > 1) {
		pp_params = &rx_pp->main_pool[--i];
		if (!pp_params->pp)
			continue;
		qdf_page_pool_destroy(pp_params->pp);
		pp_params->pp = NULL;
	}
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_rx_page_pool_resize(struct dp_soc *soc, uint32_t pool_id,
				  size_t new_size)
{
	struct dp_rx_page_pool *rx_pp = &soc->rx_pp[pool_id];
	struct dp_rx_pp_params *pp_params;
	struct dp_rx_pp_params *inactive_pp;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	int i;

	if (!wlan_cfg_get_dp_rx_buffer_recycle(soc->wlan_cfg_ctx))
		return QDF_STATUS_E_FAILURE;

	if (rx_pp->curr_pool_size == new_size) {
		dp_info("No change in pool size, continue with existing pools");
		return QDF_STATUS_SUCCESS;
	}

	if (!rx_pp->curr_pool_size || !new_size)
		return QDF_STATUS_E_FAILURE;

	qdf_atomic_set(&rx_pp->update_in_progress, 1);

	if (new_size > rx_pp->curr_pool_size) {
		status = dp_rx_page_pool_upsize(soc, rx_pp, new_size);
		goto resize_done;
	}

	qdf_spin_lock_bh(&rx_pp->pp_lock);
	/* Base page pool at 0th index is always present,
	 * so destroy page pools from 1st index.
	 */
	for (i = 1; i < DP_PAGE_POOL_MAX; i++) {
		pp_params = &rx_pp->main_pool[i];

		if (!pp_params->pp)
			continue;

		/* Immediately destroy the page pool if there
		 * are no inflight pages.
		 */
		if (qdf_page_pool_full_bh(pp_params->pp)) {
			qdf_page_pool_destroy(pp_params->pp);
			qdf_mem_set(pp_params, sizeof(*pp_params), 0);
			continue;
		}

		inactive_pp = qdf_mem_malloc(sizeof(*inactive_pp));
		if (!inactive_pp) {
			dp_info("Failed to alloc inactive pp node for %pK",
				pp_params);
			qdf_page_pool_destroy(pp_params->pp);
			qdf_mem_set(pp_params, sizeof(*pp_params), 0);
			continue;
		}

		qdf_mem_copy(inactive_pp, pp_params, sizeof(*pp_params));
		qdf_mem_set(pp_params, sizeof(*pp_params), 0);
		qdf_list_insert_back(&rx_pp->inactive_list, &inactive_pp->node);
	}

	if (!qdf_list_empty(&rx_pp->inactive_list))
		qdf_timer_mod(&rx_pp->pool_inactivity_timer,
			      DP_RX_PP_INACTIVE_TIMER_MS);

	rx_pp->active_pp_idx = 0;
	qdf_spin_unlock_bh(&rx_pp->pp_lock);

resize_done:
	if (QDF_IS_STATUS_SUCCESS(status))
		rx_pp->curr_pool_size = new_size;

	qdf_atomic_set(&rx_pp->update_in_progress, 0);

	return status;
}

#endif /* DP_FEATURE_RX_BUFFER_RECYCLE */
