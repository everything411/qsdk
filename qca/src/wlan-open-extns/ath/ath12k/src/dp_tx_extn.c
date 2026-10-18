// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_extn.h"

#ifndef CONFIG_IO_COHERENCY
/**
 * ath12k_core_dma_clean_range_no_dsb() - Clean CPU cache for a buffer without DSB
 * @start: Start address (inclusive)
 * @end: End address (exclusive)
 *
 * Cleans the cache lines covering the range [@start, @end) without issuing a
 * data synchronization barrier (DSB). This is used on the TX fast-path for
 * recycler SKBs where the caller ensures ordering requirements, to avoid the
 * overhead of a full barrier.
 *
 * On some platforms (e.g. PLATFORM_SDX) this is a no-op.
 */
static inline void ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end)
{
#ifndef PLATFORM_SDX
	dmac_clean_range_no_dsb(start, end);
#endif
}

#ifdef PLATFORM_SDX
/**
 * ath12k_dp_tx_buffer_map() - Map TX skb data buffer for DMA
 * @dp: Datapath instance (provides DMA device)
 * @skb: Socket buffer whose linear data will be mapped
 * @len: Number of bytes to map from @skb->data
 * @skb_ctrl: Per-skb control (DP_SKB_RECYCLED indicates recycler/fast-path skb)
 *
 * When @skb_ctrl has DP_SKB_RECYCLED set, the buffer is assumed to come from the
 * recycler/fast-path. In that case the cache is cleaned without a DSB and the
 * physical address is obtained via virt_to_phys() (bypassing dma_map_single()).
 *
 * Otherwise the linear data buffer is mapped for DMA_TO_DEVICE using
 * dma_map_single() on non IO-coherent platforms, or translated via
 * virt_to_phys() on IO-coherent platforms (CONFIG_IO_COHERENCY).
 *
 * Return: DMA address for the mapped buffer. Caller must use
 * dma_mapping_error() to validate the returned address.
 */
static inline
dma_addr_t ath12k_dp_tx_buffer_map(struct ath12k_dp *dp, struct sk_buff *skb, u32 len,
				   struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	dma_addr_t paddr;

	/* DMA cache clean */
	paddr = dma_map_single(dp->dev, skb->data, len, DMA_TO_DEVICE);
	return paddr;
}
#else
/**
 * ath12k_dp_tx_buffer_map() - Map TX skb data buffer for DMA
 * @dp: Datapath instance (provides DMA device)
 * @skb: Socket buffer whose linear data will be mapped
 * @len: Number of bytes to map from @skb->data
 * @skb_ctrl: Per-skb control (DP_SKB_RECYCLED indicates recycler/fast-path skb)
 *
 * When @skb_ctrl has DP_SKB_RECYCLED set, the buffer is assumed to come from the
 * recycler/fast-path. In that case the cache is cleaned without a DSB and the
 * physical address is obtained via virt_to_phys() (bypassing dma_map_single()).
 *
 * Otherwise the linear data buffer is mapped for DMA_TO_DEVICE using
 * dma_map_single() on non IO-coherent platforms, or translated via
 * virt_to_phys() on IO-coherent platforms (CONFIG_IO_COHERENCY).
 *
 * Return: DMA address for the mapped buffer. Caller must use
 * dma_mapping_error() to validate the returned address.
 */
static inline
dma_addr_t ath12k_dp_tx_buffer_map(struct ath12k_dp *dp, struct sk_buff *skb, u32 len,
				   struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	dma_addr_t paddr;

	if (likely(skb_ctrl && (skb_ctrl->flags & DP_SKB_RECYCLED))) {
		ath12k_core_dma_clean_range_no_dsb(skb->data,
						   skb->data + DP_TX_SFE_BUFFER_SIZE);
		paddr = virt_to_phys(skb->data);
		return paddr;
	}

	/* DMA cache clean */
	paddr = dma_map_single(dp->dev, skb->data, len, DMA_TO_DEVICE);
	return paddr;
}
#endif

/**
 * ath12k_dp_tx_buffer_unmap() - Unmap a previously DMA-mapped TX buffer
 * @dev: DMA device used for mapping
 * @dma_handle: DMA address returned from the map operation
 * @size: Size of the mapped region in bytes
 * @direction: DMA direction used for the mapping
 *
 * Unmaps a DMA mapping created for TX. On IO-coherent platforms this becomes a
 * no-op (see CONFIG_IO_COHERENCY block below).
 */
void ath12k_dp_tx_buffer_unmap(struct device *dev, dma_addr_t dma_handle, size_t size,
			       enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_handle, size, direction);
}
#else
/**
 * ath12k_core_dma_clean_range_no_dsb() - Clean CPU cache for a buffer without DSB
 * @start: Start address (inclusive)
 * @end: End address (exclusive)
 *
 * Stub for IO-coherent platforms: cache maintenance is not required.
 */
static inline
void ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end)
{
}

/**
 * ath12k_dp_tx_buffer_map() - Map TX skb data buffer for DMA
 * @dp: Datapath instance (provides DMA device)
 * @skb: Socket buffer whose linear data will be mapped
 * @len: Number of bytes to map from @skb->data
 * @skb_ctrl: Per-skb control (DP_SKB_RECYCLED indicates recycler/fast-path skb)
 *
 * When @skb_ctrl has DP_SKB_RECYCLED set, the buffer is assumed to come from the
 * recycler/fast-path. In that case the cache is cleaned without a DSB and the
 * physical address is obtained via virt_to_phys() (bypassing dma_map_single()).
 *
 * Otherwise the linear data buffer is translated via virt_to_phys() on
 * IO-coherent platforms (CONFIG_IO_COHERENCY).
 *
 * Return: DMA address for the mapped buffer. Caller must use
 * dma_mapping_error() to validate the returned address.
 */
static inline
dma_addr_t ath12k_dp_tx_buffer_map(struct ath12k_dp *dp, struct sk_buff *skb, u32 len,
				   struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	dma_addr_t paddr;

	if (likely(skb_ctrl && (skb_ctrl->flags & DP_SKB_RECYCLED))) {
		ath12k_core_dma_clean_range_no_dsb(skb->data, skb->data + DP_TX_SFE_BUFFER_SIZE);
		paddr = virt_to_phys(skb->data);
		return paddr;
	}

	/* DMA cache clean */
	paddr = virt_to_phys(skb->data);
	return paddr;
}

/**
 * ath12k_dp_tx_buffer_unmap() - Unmap a previously DMA-mapped TX buffer
 * @dev: DMA device used for mapping
 * @dma_handle: DMA address returned from the map operation
 * @size: Size of the mapped region in bytes
 * @direction: DMA direction used for the mapping
 *
 * Stub for IO-coherent platforms: unmapping is not required.
 */
void ath12k_dp_tx_buffer_unmap(struct device *dev, dma_addr_t dma_handle, size_t size,
			       enum dma_data_direction direction)
{
}
#endif /* CONFIG_IO_COHERENCY */

/**
 * ath12k_dp_tx_dma_map() - DMA map TX skb and update descriptor info
 * @dp: Datapath instance (provides DMA device and stats)
 * @skb: Socket buffer whose data buffer will be mapped
 * @len: Data length to map from @skb->data
 * @tx_desc: TX descriptor info to be updated with DMA address/flags
 * @msdu_info: MSDU info to be updated with DMA address
 * @skb_ctrl: Per-skb control flags (MAC ctrl / recycled fast path)
 *
 * Maps @skb for TX DMA using ath12k_dp_tx_buffer_map(), stores the resulting
 * DMA address in @msdu_info and @tx_desc and sets descriptor flags for fast
 * path and recycler frames.
 *
 * Return: true on success, false on failure
 */
bool ath12k_dp_tx_dma_map(struct ath12k_dp *dp, struct sk_buff *skb, u32 len,
			  struct ath12k_tx_desc_info *tx_desc,
			  struct ath12k_dp_tx_msdu_info *msdu_info,
			  struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	dma_addr_t paddr;

	paddr = ath12k_dp_tx_buffer_map(dp, skb, len, skb_ctrl);
	if (dma_mapping_error(dp->dev, paddr)) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		return false;
	}

	msdu_info->paddr = tx_desc->paddr = paddr;
	if (likely(skb_ctrl && (skb_ctrl->flags & DP_SKB_FAST_TX))) {
		tx_desc->flags |= DP_TX_DESC_FLAG_FAST;
		if (skb_ctrl->flags & DP_SKB_RECYCLED)
			tx_desc->flags |= DP_TX_DESC_FLAG_RECYCLE;
	} else {
		tx_desc->flags = 0;
	}

	return true;
}
EXPORT_SYMBOL(ath12k_dp_tx_dma_map);
EXPORT_SYMBOL(ath12k_dp_tx_buffer_unmap);

void ath12k_dp_tx_hw_group_extn_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	int i = 0;
	struct ath12k_dp_hw_group_extn *hw_grp_extn = &dp_hw_grp->extn;

	mutex_lock(&dp_hw_grp->tx_init_lock);

	if (hw_grp_extn->tx_desc_extn_initialized)
		goto unlock;

	for (i = 0; i < ATH12K_HW_MAX_QUEUES; i++)
		INIT_LIST_HEAD(&hw_grp_extn->tx_hot_desc_list[i]);

	hw_grp_extn->tx_desc_extn_initialized = true;
unlock:
	mutex_unlock(&dp_hw_grp->tx_init_lock);
}

EXPORT_SYMBOL(ath12k_dp_tx_hw_group_extn_init);

/**
 * ath12k_dp_tx_assign_buffer_hot() - Assign a TX descriptor from the per-ring
 *                                    hot list (lock-free fast-TX path).
 * @dp_hw_grp: HW group owning the descriptor pools
 * @ring_id:   TX ring / pool index (== smp_processor_id() on fast-TX path)
 *
 * Tries to pop a descriptor from the per-CPU hot list without any lock.
 * If the hot list is empty, it refills it with up to
 * ATH12K_DP_TX_HOT_DESC_BATCH_SIZE entries from the regular tx_desc_free_list
 * (under tx_desc_lock) and retries once.
 *
 * Return: pointer to a free ath12k_tx_desc_info, or NULL when the pool is
 *         exhausted.
 */
struct ath12k_tx_desc_info *
ath12k_dp_tx_assign_buffer_hot(struct ath12k_dp_hw_group *dp_hw_grp, u8 ring_id)
{
	struct ath12k_tx_desc_info *desc;
	struct list_head *hot_list = &dp_hw_grp->extn.tx_hot_desc_list[ring_id];
	LIST_HEAD(batch);
	int i;
	u32 *tx_desc_used_cnt;

	/* Fast path: hot list has entries – no lock needed. */
	desc = list_first_entry_or_null(hot_list,
					struct ath12k_tx_desc_info, list);
	if (likely(desc)) {
		list_del(&desc->list);
		return desc;
	}

	/* Hot list empty – refill from the regular pool under lock. */
	spin_lock_bh(&dp_hw_grp->tx_desc_lock[ring_id]);
	for (i = 0; i < ATH12K_DP_TX_HOT_DESC_BATCH_SIZE; i++) {
		desc = list_first_entry_or_null(&dp_hw_grp->tx_desc_free_list[ring_id],
						struct ath12k_tx_desc_info, list);
		if (!desc)
			break;

		desc->in_use = true;
		tx_desc_used_cnt = this_cpu_ptr(dp_hw_grp->tx_desc_used_cnt);
		(*tx_desc_used_cnt)++;

		list_move_tail(&desc->list, &batch);
	}
	spin_unlock_bh(&dp_hw_grp->tx_desc_lock[ring_id]);

	if (list_empty(&batch))
		return NULL;

	/* Move the batch into the hot list. */
	list_splice_tail(&batch, hot_list);

	/* Consume the first entry from the freshly populated hot list. */
	desc = list_first_entry(hot_list, struct ath12k_tx_desc_info, list);
	list_del(&desc->list);
	return desc;
}

EXPORT_SYMBOL(ath12k_dp_tx_assign_buffer_hot);
