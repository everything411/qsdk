// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "dp_tx_mon.h"
#include "dp_mon_filter.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#include <linux/skbuff_ref.h>
#endif

static void ath12k_dp_mon_tx_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);

/**
 * ath12k_dp_mon_tx_setup_ppdu_desc() - Setup TX monitor PPDU descriptor pool
 * @dp_pdev: Pointer to DP PDEV context for device-specific operations
 *
 * This function initializes the TX monitor PPDU descriptor pool and associated
 * management structures. It allocates memory for descriptor pool, initializes
 * list management structures, and populates the free descriptor list for
 * efficient descriptor allocation during TX monitor operations.
 *
 * The function performs the following initialization sequence:
 * 1. Allocates memory pool for PPDU descriptors using kcalloc()
 * 2. Initializes spinlock for thread-safe descriptor list operations
 * 3. Initializes free, used, and processing descriptor lists
 * 4. Populates free list with all allocated descriptors
 * 5. Updates statistics counters for descriptor tracking
 *
 * Context: Called during TX monitor initialization in process context
 * Locking: Initializes and uses tx_mon_ppdu_desc_lock for list operations
 *
 * Return: 0 on success, -ENOMEM on memory allocation failure
 */
static int
ath12k_dp_mon_tx_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_base *ab = dp_pdev->dp->ab;
	size_t alloc_size = sizeof(struct ath12k_dp_mon_ppdu_desc);
	size_t status_desc_size;
	int i, ret;
	u32 mon_status_buf = DP_TX_MON_NUM_STATUS_BUF(ab);

	if (unlikely(!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon))
		return -EINVAL;

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	if (dp_pdev_tx_mon->tx_mon_ppdu_desc_pool) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor PPDU desc pool already allocated, reusing\n");
		dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized = true;
		return 0;
	}

	spin_lock_init(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	dp_pdev_tx_mon->tx_mon_ppdu_desc_pool =
			kcalloc(DP_TX_MON_NUM_PPDU_DESC(ab),
				sizeof(*dp_pdev_tx_mon->tx_mon_ppdu_desc_pool),
				GFP_KERNEL);

	if (unlikely(!dp_pdev_tx_mon->tx_mon_ppdu_desc_pool)) {
		ath12k_warn(dp_pdev->dp->ab,
			    "Failed to allocate monitor PPDU desc pool\n");
		return -ENOMEM;
	}

	status_desc_size = sizeof(struct ath12k_dp_mon_status_desc) * mon_status_buf;

	for (i = 0; i < DP_TX_MON_NUM_PPDU_DESC(ab); i++) {
		dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i] =
			kzalloc(sizeof(*dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]),
				GFP_KERNEL);
		if (!dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]) {
			ath12k_warn(dp_pdev->dp,
				    "Failed to allocate monitor PPDU desc pool\n");
			ret = -ENOMEM;
			goto fail;
		}
		/* Allocate status_desc array for each PPDU descriptor */
		dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]->status_desc =
			kcalloc(mon_status_buf, sizeof(struct ath12k_dp_mon_status_desc),
				GFP_KERNEL);
		if (!dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]->status_desc) {
			ath12k_warn(ab,
				    "Failed to allocate status_desc for PPDU desc %d\n",
				    i);
			ret = -ENOMEM;
			goto fail;
		}
	}
	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX MON SETUP: Allocated PPDU desc pool at %p, size=%zu\n",
		   dp_pdev_tx_mon->tx_mon_ppdu_desc_pool,
		   alloc_size * DP_TX_MON_NUM_PPDU_DESC(ab));


	ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
		   "TX MON SETUP: Allocated status_desc arrays, size=%zu per desc\n",
		   status_desc_size);

	INIT_LIST_HEAD(&dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list);
	INIT_LIST_HEAD(&dp_pdev_tx_mon->tx_mon_ppdu_desc_used_list);
	INIT_LIST_HEAD(&dp_pdev_tx_mon->tx_mon_ppdu_desc_proc_list);

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	for (i = 0; i < DP_TX_MON_NUM_PPDU_DESC(ab); i++) {
		INIT_LIST_HEAD(&dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]->list);
		list_add_tail(&dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]->list,
			      &dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list);
	}
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized = true;

	return 0;
fail:
	ath12k_dp_mon_tx_cleanup_ppdu_desc(dp_pdev);
	return ret;
}

/**
 * ath12k_dp_mon_tx_wq_init() - Initialize TX monitor work queue
 * @dp_pdev: DP pdev handle
 *
 * This function initializes the TX monitor work queue and related structures.
 * with proper error handling and initialization order.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_dp_mon_tx_wq_init(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_mon_data *mon_data;
	size_t radiotap_vendor_size = sizeof(struct ieee80211_radiotap_vendor_ns) +
		sizeof(struct ath12k_rtap_vendor_ns);

	if (!dp_pdev || !dp_pdev->dp_mon_pdev ||
	    !dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)
		return -EINVAL;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	mon_data = &dp_mon_pdev->mon_data;
	if (!mon_data->rtap_vendor_tlv) {
		mon_data->rtap_vendor_tlv = kzalloc(radiotap_vendor_size,
						    GFP_KERNEL);
		if (!mon_data->rtap_vendor_tlv)
			return -ENOMEM;

		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor vendor TLV allocated\n");
	} else {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor vendor TLV already allocated, reusing\n");
	}

	INIT_LIST_HEAD(&dp_pdev_tx_mon->tx_mon_desc_work_list);

	dp_pdev_tx_mon->txmon_wq =
		alloc_workqueue("txmon_%s-%s%d",
				WQ_UNBOUND | WQ_SYSFS, 1,
				ath12k_bus_str(dp_pdev->dp->ab->hif.bus),
				dev_name(dp_pdev->dp->ab->dev), dp_pdev->mac_id);

	if (!dp_pdev_tx_mon->txmon_wq) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX monitor work queue allocation failed\n");
		goto free_vendor_tlv;
	}

	INIT_WORK(&dp_pdev_tx_mon->txmon_work,
		  ath12k_dp_tx_mon_process_ppdu);
	dp_pdev_tx_mon->tx_mon_wq_initialized = true;

	return 0;

free_vendor_tlv:
	kfree(mon_data->rtap_vendor_tlv);
	mon_data->rtap_vendor_tlv = NULL;
	return -ENOMEM;
}

/**
 * ath12k_dp_mon_tx_cleanup_ppdu_desc() - Cleanup TX monitor PPDU descriptors
 * @dp_pdev: Pointer to DP PDEV context for cleanup operations
 *
 * This function performs complete cleanup of TX monitor PPDU descriptor pool
 * and associated resources. It safely deallocates the descriptor pool memory
 * and resets the pool pointer to prevent use-after-free conditions.
 *
 * The function ensures safe cleanup by:
 * 1. Acquiring the descriptor pool spinlock to prevent concurrent access
 * 2. Freeing the dynamically allocated status_desc arrays for each descriptor
 * 3. Freeing the dynamically allocated tx_mon_ppdu_desc_pool[]
 * 4. Freeing the allocated descriptor pool memory using kfree()
 * 5. Setting the pool pointer to NULL to prevent dangling pointer access
 * 6. Releasing the spinlock after cleanup completion
 *
 * Context: Called during TX monitor shutdown in process context
 * Locking: Uses tx_mon_ppdu_desc_lock for safe memory deallocation
 */
static void ath12k_dp_mon_tx_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon_ppdu_desc *tx_ppdu_desc;
	int i;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon)
		return;

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	if (!dp_pdev_tx_mon->tx_mon_ppdu_desc_pool) {
		dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized = false;
		spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
		return;
	}

	for (i = 0; i < DP_TX_MON_NUM_PPDU_DESC(dp->ab); i++) {
		if (dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]) {
			tx_ppdu_desc = dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i];
			/* Free status_desc arrays for each PPDU descriptor */
			kfree(tx_ppdu_desc->status_desc);
			dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]->status_desc = NULL;
			kfree(dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i]);
		}
		dp_pdev_tx_mon->tx_mon_ppdu_desc_pool[i] = NULL;
	}

	kfree(dp_pdev_tx_mon->tx_mon_ppdu_desc_pool);
	dp_pdev_tx_mon->tx_mon_ppdu_desc_pool = NULL;
	dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized = false;
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
}

/**
 * ath12k_dp_mon_tx_wq_deinit() - Deinitialize TX monitor work queue
 * @dp_pdev: DP pdev handle
 *
 * This function deinitializes the TX monitor work queue and frees resources.
 */
static void ath12k_dp_mon_tx_wq_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_mon_data *mon_data;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev ||
	    !dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	mon_data = &dp_mon_pdev->mon_data;

	if (dp_pdev_tx_mon->txmon_wq) {
		cancel_work_sync(&dp_pdev_tx_mon->txmon_work);
		destroy_workqueue(dp_pdev_tx_mon->txmon_wq);
		dp_pdev_tx_mon->txmon_wq = NULL;
	}

	kfree(mon_data->rtap_vendor_tlv);
	mon_data->rtap_vendor_tlv = NULL;
}

/**
 * ath12k_dp_mon_tx_wq_start() - Start TX monitor work queue and PPDU descriptors
 * @dp_pdev: Pointer to DP PDEV context for device access and configuration
 * @mac_id: MAC ID for the physical device (used for logging and identification)
 *
 * This function initializes the TX monitor work queue infrastructure and sets up
 * PPDU descriptor management for TX monitor functionality. It should be called
 * after basic TX monitor ring allocation is complete but before any TX monitor
 * operations begin.
 *
 * The function performs initialization in the following order:
 * 1. Setup PPDU descriptors for TX monitor frame processing
 * 2. Initialize work queue for asynchronous TX monitor processing
 * 3. Provide proper cleanup on any failure
 *
 * This function is typically called during interface bring-up or when TX monitor
 * functionality needs to be activated. It complements the basic resource allocation
 * done in ath12k_dp_mon_tx_pdev_alloc().
 *
 * Context: Can be called from process context during interface initialization.
 * Locking: Uses internal locking through architecture-specific operations.
 *
 * Return: 0 on success, negative error code on failure
 *         -EINVAL if invalid parameters or missing operations
 *         Architecture-specific error codes from setup operations
 */
int ath12k_dp_mon_tx_wq_start(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	int ret = 0;

	if (unlikely(!dp_pdev)) {
		ath12k_err(NULL, "Tx Mon: Invalid DP Pdev\n");
		return -EINVAL;
	}

	if (!dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)
		return -EINVAL;

	ret = ath12k_dp_mon_tx_setup_ppdu_desc(dp_pdev);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "failed to setup TX mon ppdu desc: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_mon_tx_wq_init(dp_pdev);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "failed to init TX mon workqueue for pdev_id %d: %d\n",
			    mac_id, ret);
		goto cleanup_ppdu_desc;
	}
	return 0;

cleanup_ppdu_desc:
	ath12k_dp_mon_tx_cleanup_ppdu_desc(dp_pdev);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_wq_start);

/**
 * ath12k_dp_mon_tx_wq_stop() - Destroy TX monitor work queue and free descriptors
 * @dp_pdev: Pointer to DP PDEV context for cleanup operations
 *
 * Called at pdev free time (via ath12k_dp_mon_tx_pdev_free()). Drains any
 * in-flight txmon_work after successful WQ init, destroys the workqueue, and
 * frees the PPDU descriptor pool.
 */
void ath12k_dp_mon_tx_wq_stop(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;

	if (unlikely(!dp_pdev))
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon)
		return;

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	if (dp_pdev_tx_mon->tx_mon_wq_initialized) {
		ath12k_dp_mon_tx_wq_deinit(dp_pdev);
		dp_pdev_tx_mon->tx_mon_wq_initialized = false;
	}

	if (dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized)
		ath12k_dp_mon_tx_cleanup_ppdu_desc(dp_pdev);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_wq_stop);

/**
 * ath12k_dp_mon_tx_desc_free() - Free monitor descriptors
 * @local_list: List of descriptors to free
 * @dp_mon: DP monitor handle
 */
static void
ath12k_dp_mon_tx_desc_free(struct ath12k_pdev_dp *dp_pdev,
				struct list_head *local_list,
				struct ath12k_dp_mon *dp_mon)
{
	struct ath12k_dp_tx_mon *dp_tx_mon;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return;

	dp_tx_mon = dp_mon->dp_tx_mon;
	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	list_splice_tail_init(local_list, &dp_tx_mon->tx_mon_desc_free_list);
	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
	ATH12K_TX_MON_STAT_INC(dp_pdev, mon_desc_free);
}

/**
 * ath12k_dp_mon_tx_free_pkt_buf() - Free packet buffers from TLV data
 * @dp_pdev: Pointer to DP PDEV context containing device and statistics info
 * @status_desc: Status descriptor containing monitor buffer and TLV metadata
 *
 * This function parses TLV data from a monitor status buffer and frees any
 * packet buffers referenced by HAL_TX_MON_BUF_ADDR TLVs. It performs
 * comprehensive validation and cleanup of monitor descriptors and their
 * associated DMA buffers.
 *
 * The function handles:
 * - TLV parsing using HAL 64-bit TLV header format with proper alignment
 * - Cookie validation using magic number verification for memory safety
 * - DMA buffer unmapping to ensure proper cache coherency
 * - Page fragment deallocation to prevent memory leaks
 * - Descriptor state validation and error reporting
 * - Statistics tracking for monitoring system health
 *
 * TLV Processing Flow:
 * 1. Parse each TLV header to extract tag and length
 * 2. For HAL_TX_MON_BUF_ADDR TLVs, extract packet info and descriptor cookie
 * 3. Validate descriptor magic number and usage state
 * 4. Unmap DMA buffer and free page fragment
 * 5. Add descriptor to cleanup list for return to free pool
 * 6. Continue to next TLV with proper alignment
 *
 * Context: Called from work queue context during TLV flushing operations.
 * Locking: Uses internal locking for descriptor list management.
 */
static void
ath12k_dp_mon_tx_free_pkt_buf(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_dp_mon_status_desc *status_desc)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = &ab->hal;
	struct dp_mon_packet_info *packet_info;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_dp_mon_desc *pkt_desc;
	struct list_head mon_desc_used_list;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	u8 *mon_buf = status_desc->mon_buf;
	u32 mon_buf_len = status_desc->buf_len;
	u8 pkt_buf_cnt = status_desc->pkt_buf_cnt;
	u8 *ptr = mon_buf;
	u8 found = 0;
	u16 tlv_tag, tlv_len;

	dp_pdev_tx_mon = dp_pdev->dp_mon_pdev ?
			 dp_pdev->dp_mon_pdev->dp_pdev_tx_mon : NULL;
	if (unlikely(!dp_pdev_tx_mon || !dp_mon))
		return;

	INIT_LIST_HEAD(&mon_desc_used_list);

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = ath12k_hal_get_tlv_hdr_tag(hal, tlv->tl);
		ptr += sizeof(*tlv);

		tlv_len = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_LEN);

		if (ath12k_hal_is_mon_buf_addr_tlv(hal, tlv_tag)) {
			packet_info = (struct dp_mon_packet_info *)ptr;
			pkt_desc = (struct ath12k_dp_mon_desc *)
				(uintptr_t)(packet_info->cookie);

			if (unlikely(!pkt_desc)) {
				ath12k_warn(ab,
					    "mon_flush: NULL pkt_desc received in macid %d\n",
					    dp_pdev->mac_id);
				ATH12K_TX_MON_STAT_INC(dp_pdev, pkt_buf_null);
				goto next_tlv;
			}

			if (unlikely(pkt_desc->magic !=
				     ATH12K_MON_MAGIC_VALUE)) {
				ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, magic_value_error);
				ath12k_warn(ab,
					    "mon_flush: invalid magic value in macid %d\n",
					    dp_pdev->mac_id);
				goto next_tlv;
			}

			list_add_tail(&pkt_desc->list, &mon_desc_used_list);

			if (unlikely(pkt_desc->in_use != DP_MON_DESC_TO_HW)) {
				ath12k_warn(ab,
					    "mon_flush: invalid in_use=[%d] flag, macid %d\n",
					    pkt_desc->in_use, dp_pdev->mac_id);
				goto next_tlv;
			}

			ath12k_core_dma_unmap_page(dp->dev, pkt_desc->paddr,
						   ATH12K_DP_MON_TX_BUF_SIZE,
						   DMA_FROM_DEVICE);
			ATH12K_TX_MON_STAT_INC(dp_pdev, pkt_buf_free);

			page_frag_free(pkt_desc->mon_buf);
			pkt_desc->mon_buf = NULL;
			pkt_desc->in_use = DP_MON_DESC_H_PROC_ERR;

			if (pkt_buf_cnt && ++found == pkt_buf_cnt)
				break;
		}

next_tlv:
		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);
	} while ((ptr - mon_buf) < mon_buf_len);

	if (likely(!list_empty(&mon_desc_used_list)))
		ath12k_dp_mon_tx_desc_free(dp_pdev, &mon_desc_used_list, dp_mon);
}

/**
 * ath12k_dp_tx_mon_flush_tlv() - Flush TLV data and free associated resources
 * @dp_pdev: Pointer to DP PDEV context for device access and statistics
 * @status_desc: Pointer to status descriptor containing TLV buffer information
 *
 * This function performs comprehensive cleanup of a monitor status descriptor
 * by flushing its TLV data and freeing all associated resources. It serves as
 * the primary cleanup function for monitor status buffers that contain TLV
 * data with embedded packet buffer references.
 *
 * The function orchestrates the complete cleanup process:
 * - Delegates TLV parsing and packet buffer cleanup to helper function
 * - Updates system statistics for monitoring buffer usage
 * - Frees the monitor status buffer itself using page fragment allocator
 * - Ensures all resources are properly released to prevent memory leaks
 *
 * This function is called in several scenarios:
 * - Normal PPDU processing completion when TLV data is no longer needed
 * - Error recovery when PPDU processing fails and cleanup is required
 * - Descriptor list flushing during system shutdown or error conditions
 * - Ring processing overflow when descriptors must be discarded
 *
 * Resource Management:
 * - Calls ath12k_dp_mon_tx_free_pkt_buf() for embedded packet buffer cleanup
 * - Updates mon_stats->status_buf_free counter for buffer tracking
 * - Uses page_frag_free() for efficient memory deallocation
 * - Ensures proper cleanup ordering to prevent use-after-free conditions
 *
 * Context: Called from work queue context or error handling paths.
 * Locking: Internal locking handled by called functions.
 * Memory: Frees both embedded packet buffers and the status buffer itself.
 */
static void
ath12k_dp_tx_mon_flush_tlv(struct ath12k_pdev_dp *dp_pdev,
			   struct ath12k_dp_mon_status_desc *status_desc,
			   bool pkt_buf_cnt_in_desc)
{
	u8 *mon_buf = status_desc->mon_buf;

	if (!pkt_buf_cnt_in_desc || status_desc->pkt_buf_cnt)
		ath12k_dp_mon_tx_free_pkt_buf(dp_pdev, status_desc);

	if (unlikely(!dp_pdev->dp_mon_pdev || !dp_pdev->dp_mon_pdev->dp_pdev_tx_mon))
		return;
	ATH12K_TX_MON_STAT_INC(dp_pdev, status_buf_free);
	page_frag_free(mon_buf);
}

/**
 * ath12k_dp_tx_mon_flush_desc_list() - Flush monitor descriptor list
 * @dp_pdev: DP PDEV context
 * @mon_desc_list: List of monitor descriptors to flush
 *
 * This function flushes monitor descriptors when errors occur during
 * ring processing. It processes any TLV data in the descriptors,
 * returns the descriptors to the free list, and returns the number of
 * descriptors that had an active monitor buffer to flush.
 *
 * Called from NAPI context when PPDU preparation fails or during
 * error recovery scenarios.
 *
 * Return: Number of descriptors with a non-NULL monitor buffer flushed.
 */
static u32
ath12k_dp_tx_mon_flush_desc_list(struct ath12k_pdev_dp *dp_pdev,
				 struct list_head *mon_desc_list)
{
	struct ath12k_dp_mon_desc *tmp_desc, *entry_desc;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;
	struct ath12k_hal *hal = &dp_pdev->dp->ab->hal;
	bool pkt_buf_cnt_in_desc = ath12k_hal_mon_tx_pkt_buf_cnt_in_desc(hal);
	struct ath12k_dp_mon_status_desc desc;
	u32 mon_desc_flushed = 0;

	list_for_each_entry_safe(entry_desc, tmp_desc,
				 mon_desc_list, list) {
		if (unlikely(!entry_desc->mon_buf))
			continue;

		mon_desc_flushed++;

		ath12k_core_dma_unmap_page(dp_pdev->dp->dev, entry_desc->paddr,
					   ATH12K_DP_MON_TX_BUF_SIZE,
					   DMA_FROM_DEVICE);

		desc.mon_buf = entry_desc->mon_buf;
		desc.buf_len = entry_desc->buf_len;
		desc.end_of_ppdu = entry_desc->end_of_ppdu;
		desc.pkt_buf_cnt = entry_desc->pkt_buf_cnt;

		ath12k_dp_tx_mon_flush_tlv(dp_pdev, &desc, pkt_buf_cnt_in_desc);

		entry_desc->mon_buf = NULL;
		entry_desc->buf_len = 0;
		entry_desc->end_of_ppdu = false;
	}

	/* Free descriptor list */
	ath12k_dp_mon_tx_desc_free(dp_pdev, mon_desc_list, dp_mon);

	return mon_desc_flushed;
}

/**
 * ath12k_dp_mon_tx_prep_ppdu_info() - Prepare PPDU information for TX monitor processing
 * @dp_mon_pdev: Pointer to monitor PDEV context containing processing state
 * @ppdu_desc: Pointer to PPDU descriptor containing status descriptors with TLV data
 *
 * This function prepares PPDU information structures for TX monitor frame processing
 * by parsing the initial TLV data to extract user count and initializing the
 * processing contexts for both protection and data frames.
 *
 * The function performs the following key operations:
 * 1. Validates PPDU descriptor and status descriptor availability
 * 2. Parses the first TLV header to extract tag, length, and user ID information
 * 3. Determines the number of users in the PPDU from HAL TLV parsing
 * 4. Initializes protection and data PPDU info structures with user counts
 * 5. Initializes MPDU queues for all potential users (up to HAL_MAX_UL_MU_USERS)
 *
 * TLV Processing Details:
 * The function examines the first status descriptor's monitor buffer, which contains
 * TLV data from the hardware. It extracts the TLV header using HAL operations to
 * determine the TLV tag, user ID, and length. This information is then used to
 * parse the number of users in the PPDU through HAL-specific parsing functions.
 */
static int
ath12k_dp_mon_tx_prep_ppdu_info(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				struct ath12k_dp_mon_ppdu_desc *ppdu_desc)
{
	struct ath12k_mon_data *mon_data = &dp_mon_pdev->mon_data;
	struct ath12k_pdev_dp *dp_pdev = dp_mon_pdev->dp_pdev;
	u8 num_users = 0;
	struct ath12k_dp_mon_status_desc *status_desc;
	enum hal_tx_mon_status hal_status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;
	struct sk_buff_head *mpdu_q;
	struct hal_tlv_64_hdr *tlv_hdr;
	void *tlv_data;
	u16 tlv_tag, tlv_len;
	u32 tlv_userid = 0;
	int i;

	if (!dp_pdev || !dp_pdev->dp || !dp_pdev->dp->ab)
		return -EINVAL;

	if (unlikely(!ppdu_desc->status_desc_cnt)) {
		ath12k_warn(dp_pdev->dp->ab, "status_desc_cnt %d ",
			    ppdu_desc->status_desc_cnt);
		return -EINVAL;
	}

	status_desc = &ppdu_desc->status_desc[0];
	if (unlikely(!status_desc || !status_desc->mon_buf)) {
		ath12k_warn(dp_pdev->dp->ab, "status desc %p , mon buf%p ",
			    status_desc, status_desc ? status_desc->mon_buf : 0);
		return -EINVAL;
	}

	if (status_desc->buf_len < sizeof(struct hal_tlv_64_hdr)) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: Buffer too small for TLV header: %u < %zu\n",
			    status_desc->buf_len, sizeof(struct hal_tlv_64_hdr));
		return -EINVAL;
	}

	tlv_hdr = (struct hal_tlv_64_hdr *)status_desc->mon_buf;

	tlv_tag = ath12k_hal_get_tlv_hdr_tag(&dp_pdev->dp->ab->hal, tlv_hdr->tl);
	tlv_len = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_HDR_LEN);
	tlv_userid = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_USR_ID);

	if (unlikely(sizeof(*tlv_hdr) + tlv_len > status_desc->buf_len)) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: TLV length exceeds buffer: %u + %u > %u\n",
			   (u32)sizeof(*tlv_hdr), tlv_len,
			   status_desc->buf_len);
		return -EINVAL;
	}

	tlv_data = (u8 *)status_desc->mon_buf + sizeof(struct hal_tlv_64_hdr);

	hal_status = ath12k_hal_mon_tx_status_get_num_user(dp_pdev->dp->hal,
							   tlv_tag,
							   tlv_data,
							   &num_users,
							   tlv_len);
	if (hal_status == HAL_TX_MON_STATUS_PPDU_NOT_DONE || !num_users) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Failed to get num_users");
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "hal_status=%d, num_users=%u, tlv_tag=0x%x\n",
			   hal_status, num_users, tlv_tag);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, get_num_users_failed);
		return -EINVAL;
	}

	mon_data->prot_ppdu_info.tx_info.num_users = 1;
	mon_data->data_ppdu_info.tx_info.num_users = num_users;

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		mpdu_q = mon_data->prot_ppdu_info.tx_info.rx_status.mpdu_q;
		skb_queue_head_init(&mpdu_q[i]);
		mpdu_q = mon_data->data_ppdu_info.tx_info.rx_status.mpdu_q;
		skb_queue_head_init(&mpdu_q[i]);
	}

	return 0;
}

/**
 * ath12k_dp_mon_tx_deep_free_ppdu_info() - Deep cleanup of PPDU info structures
 * @dp_pdev: Pointer to DP PDEV context for device-specific operations
 * @mon_data: Pointer to monitor data containing PPDU info structures to clean
 *
 * This function performs comprehensive cleanup of all MPDU socket buffers
 * queued in both protection and data PPDU information structures. It ensures
 * complete memory deallocation to prevent memory leaks during TX monitor
 * processing cleanup or error recovery scenarios.
 *
 * The function performs deep cleanup by:
 * 1. Iterating through all possible user queues (up to HAL_MAX_UL_MU_USERS)
 * 2. Dequeuing and freeing all MPDU socket buffers from data PPDU queues
 * 3. Dequeuing and freeing all MPDU socket buffers from protection PPDU queues
 * 4. Using dev_kfree_skb_any() for safe deallocation in any context
 */
static void
ath12k_dp_mon_tx_deep_free_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_mon_data *mon_data)
{
	int i;
	struct hal_tx_mon_ppdu_info *data_ppdu_info =
		&mon_data->data_ppdu_info.tx_info;
	struct hal_tx_mon_ppdu_info *prot_ppdu_info =
		&mon_data->prot_ppdu_info.tx_info;

	struct sk_buff_head *mpdu_q;
	struct sk_buff *mpdu;

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		mpdu_q = &data_ppdu_info->rx_status.mpdu_q[i];
		while ((mpdu = skb_dequeue(mpdu_q)))
			dev_kfree_skb_any(mpdu);
		mpdu_q = &prot_ppdu_info->rx_status.mpdu_q[i];
		while ((mpdu = skb_dequeue(mpdu_q)))
			dev_kfree_skb_any(mpdu);
	}

	memset(&mon_data->prot_status_info, 0, sizeof(mon_data->prot_status_info));
	memset(&mon_data->data_status_info, 0, sizeof(mon_data->data_status_info));

	memset(&mon_data->prot_ppdu_info, 0, sizeof(mon_data->prot_ppdu_info));
	memset(&mon_data->data_ppdu_info, 0, sizeof(mon_data->data_ppdu_info));

	mon_data->prot_ppdu_info.tx_info.ba_user_id = ATH12K_BA_USER_ID_INVAL;
	mon_data->data_ppdu_info.tx_info.ba_user_id = ATH12K_BA_USER_ID_INVAL;
}

/**
 * ath12k_dp_tx_mon_update_ampdu_info() - Update AMPDU aggregation information
 * @ppdu_info: Pointer to PPDU information structure to update
 * @user_idx: User index for multi-user scenarios (0-based)
 *
 * This function analyzes MPDU count and user status information to determine
 * if the current transmission represents an AMPDU (Aggregated MPDU) and sets
 * appropriate flags for radiotap header generation and frame analysis.
 *
 * AMPDU Detection Logic:
 * The function uses different detection methods based on transmission type:
 * - Single User: AMPDU detected when MPDU count > 1
 * - Multi User: AMPDU detected when per-user MPDU count > 1 OR user has AMPDU flag set
 *
 * For single user transmissions, the function examines the total MPDU count
 * in the PPDU. For multi-user transmissions, it analyzes per-user statistics
 * from the rx_user_status array to determine aggregation on a per-user basis.
 *
 * When AMPDU is detected, the function sets the RX_FLAG_AMPDU_DETAILS flag
 * in the rx_status structure, which is used by the radiotap generation code
 * to include appropriate AMPDU information in monitor mode frames.
 */
static void
ath12k_dp_tx_mon_update_ampdu_info(struct dp_mon_tx_ppdu_info *ppdu_info,
				   u8 user_idx)
{
	struct hal_tx_mon_ppdu_info *tx_info = &ppdu_info->tx_info;
	struct hal_rx_mon_ppdu_info *rx_status = &tx_info->rx_status;
	bool is_ampdu = false;
	u32 mpdu_count = 0;
	struct hal_rx_user_status *rx_user_status = tx_info->rx_status.userstats;

	if (tx_info->num_users == 1) {
		mpdu_count = ppdu_info->num_mpdu_fcs_ok;
		is_ampdu = (mpdu_count > 1);
	} else if (user_idx < tx_info->num_users) {
		mpdu_count = rx_user_status[user_idx].mpdu_cnt_fcs_ok;
		is_ampdu = (mpdu_count > 1) || rx_user_status[user_idx].is_ampdu;
	} else {
		return;
	}

	if (is_ampdu)
		rx_status->ampdu_flag |= RX_FLAG_AMPDU_DETAILS;
}

/**
 * ath12k_dp_tx_mon_gen_rts() - Generate RTS frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for address selection
 * @window_flag: Initiator window flag for address ordering
 *
 * Creates RTS frame with proper 802.11 header fields.
 * Radiotap space will be added separately during delivery processing.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_rts(struct ath12k_pdev_dp *dp_pdev,
			 struct dp_mon_tx_ppdu_info *ppdu_info,
			 struct hal_tx_mon_status_info *status_info,
			 u8 window_flag)
{
	struct sk_buff *skb;
	struct ieee80211_rts *rts;
	struct hal_tx_mon_ppdu_info *tx_info;
	u16 duration_le;
	u16 frame_control;
	size_t rts_frame_size = sizeof(struct ieee80211_rts);

	if (!ppdu_info || !status_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    rts_frame_size);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				"Failed to generate rts frame.");
		return NULL;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	rts = (struct ieee80211_rts *)skb_put_zero(skb, rts_frame_size);

	frame_control = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS;
	rts->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	rts->duration = duration_le;

	if (!status_info->protection_addr)
		status_info = &dp_pdev->dp_mon_pdev->mon_data.data_status_info;

	if (window_flag == INITIATOR_WINDOW) {
		memcpy(rts->ra, status_info->addr1, ETH_ALEN);
		memcpy(rts->ta, status_info->addr2, ETH_ALEN);
	} else {
		memcpy(rts->ra, status_info->addr2, ETH_ALEN);
		memcpy(rts->ta, status_info->addr1, ETH_ALEN);
	}

	tx_info->is_used = 1;
	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_cts2self() - Generate CTS frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for address selection
 *
 * Creates CTS-to-self frame for medium protection. Uses standard
 * CTS frame format with self-addressing.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_cts2self(struct ath12k_pdev_dp *dp_pdev,
					struct dp_mon_tx_ppdu_info *ppdu_info,
					struct hal_tx_mon_status_info *status_info)
{
	struct sk_buff *skb;
	struct ieee80211_cts *cts;
	struct hal_tx_mon_ppdu_info *tx_info;
	u16 duration_le;
	u16 frame_control;
	size_t cts_frame_size = sizeof(struct ieee80211_cts);

	if (!ppdu_info || !status_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    cts_frame_size);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate cts2self frame.");
		return NULL;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	cts = (struct ieee80211_cts *)skb_put_zero(skb,
						   cts_frame_size);

	frame_control = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS;
	cts->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	cts->duration = duration_le;
	memcpy(cts->ra, status_info->addr2, ETH_ALEN);

	tx_info->is_used = 1;

	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_cts() - Generate CTS frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for address selection
 *
 * Wrapper to create CTS-to-self frame for medium protection.
 *
 * Return: 0 on success
 */
static int
ath12k_dp_tx_mon_gen_cts(struct ath12k_pdev_dp *dp_pdev,
				struct dp_mon_tx_ppdu_info *ppdu_info,
				struct hal_tx_mon_status_info *status_info)
{
	struct sk_buff *skb = NULL;
	struct sk_buff_head *mpdu_q;

	if (!ppdu_info || !status_info)
		return -EINVAL;

	mpdu_q = &ppdu_info->tx_info.rx_status.mpdu_q[0];
	skb = ath12k_dp_tx_mon_gen_cts2self(dp_pdev,
		ppdu_info, status_info);
	if (!skb)
		return -ENOMEM;

	skb_queue_tail(mpdu_q, skb);

	return 0;
}

/**
 * ath12k_dp_tx_mon_gen_qos_null_3addr() - Generate 3-address QoS NULL frame
 * @dp_pdev: Pointer to DP PDEV context for device-specific operations
 * @ppdu_info: PPDU info structure
 *
 * Creates 3-address QoS NULL frame for medium protection. Uses standard
 * 3-address data frame format without window-based logic.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_qos_null_3addr(struct ath12k_pdev_dp *dp_pdev,
				    struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr *qos_null;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	u16 duration_le;
	u16 frame_control;
	size_t qos_null_frame_size = sizeof(struct ieee80211_qos_hdr);

	if (!ppdu_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    qos_null_frame_size);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate qos null 3 address frame.");
		return NULL;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	qos_null = (struct ieee80211_qos_hdr *)skb_put_zero(skb,
							    qos_null_frame_size);

	frame_control = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC;
	qos_null->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	qos_null->duration_id = duration_le;

	status_info = &dp_pdev->dp_mon_pdev->mon_data.data_status_info;
	memcpy(qos_null->addr1, status_info->addr1, ETH_ALEN);
	memcpy(qos_null->addr2, status_info->addr2, ETH_ALEN);
	memcpy(qos_null->addr3, status_info->addr3, ETH_ALEN);

	qos_null->qos_ctrl = cpu_to_le16(0);

	tx_info->is_used = 1;

	return skb;
}

/**
 * ath12k_dp_tx_mon_gen_qos_null_4addr() - Generate 4-address QoS NULL frame
 * @dp_pdev: Pointer to DP PDEV context for device-specific operations
 * @ppdu_info: PPDU info structure
 *
 * Creates 4-address QoS NULL frame for medium protection. Uses standard
 * 4-address data frame format without window-based logic.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_qos_null_4addr(struct ath12k_pdev_dp *dp_pdev,
				    struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr_4addr *qos_null_4addr;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	u16 duration_le;
	u16 frame_control;
	size_t frame_size = sizeof(struct ieee80211_qos_hdr_4addr);

	if (!ppdu_info)
		return NULL;

	tx_info = &ppdu_info->tx_info;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR +
			    frame_size);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate qos null 4 address frame.");
		return NULL;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	qos_null_4addr = (struct ieee80211_qos_hdr_4addr *)skb_put_zero(skb,
									frame_size);

	frame_control = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC |
		IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS;
	qos_null_4addr->frame_control = cpu_to_le16(frame_control);

	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	duration_le = cpu_to_le16(tx_info->rx_status.rx_duration);
	qos_null_4addr->duration_id = duration_le;

	status_info = &dp_pdev->dp_mon_pdev->mon_data.data_status_info;

	memcpy(qos_null_4addr->addr1, status_info->addr1, ETH_ALEN);
	memcpy(qos_null_4addr->addr2, status_info->addr2, ETH_ALEN);
	memcpy(qos_null_4addr->addr3, status_info->addr3, ETH_ALEN);
	memcpy(qos_null_4addr->addr4, status_info->addr2, ETH_ALEN);

	qos_null_4addr->qos_ctrl = cpu_to_le16(0);
	tx_info->is_used = 1;

	return skb;
}

/**
 * ath12k_dp_tx_mon_generate_prot_frm() - Generate protection frame
 * @dp_pdev: DP pdev handle
 * @tx_prot_ppdu_info: Protection PPDU information
 *
 * Generates protection frames (RTS/CTS) based on protection type
 * specified in PPDU information. The generated frame is added to
 * the protection PPDU's MPDU queue for delivery to monitor stack.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_generate_prot_frm(struct ath12k_pdev_dp *dp_pdev,
				   struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info)
{
	struct sk_buff *skb = NULL;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct sk_buff_head *mpdu_q;
	struct hal_tx_mon_status_info *status_info;
	u32 protection_type;
	u8 window_flag = INITIATOR_WINDOW;

	tx_info = &tx_prot_ppdu_info->tx_info;
	mpdu_q = &tx_info->rx_status.mpdu_q[0];

	status_info = &dp_pdev->dp_mon_pdev->mon_data.prot_status_info;
	protection_type = status_info->medium_prot_type;

	switch (protection_type) {
	case DP_MON_TX_MEDIUM_NO_PROTECTION:
		tx_info->is_used = 0;
		return 0;
	case DP_MON_TX_MEDIUM_RTS_LEGACY:
	case DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW:
	case DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW:
		skb = ath12k_dp_tx_mon_gen_rts(dp_pdev,
					       tx_prot_ppdu_info,
					       status_info,
					       window_flag);
		break;

	case DP_MON_TX_MEDIUM_CTS2SELF:
		skb = ath12k_dp_tx_mon_gen_cts2self(dp_pdev,
			tx_prot_ppdu_info, status_info);
		break;

	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR:
		skb = ath12k_dp_tx_mon_gen_qos_null_3addr(dp_pdev,
							  tx_prot_ppdu_info);
		break;

	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR:
		skb = ath12k_dp_tx_mon_gen_qos_null_4addr(dp_pdev,
							  tx_prot_ppdu_info);
		break;

	default:
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: No protection frame needed, type=%u\n",
			   protection_type);
		return 0;
	}

	if (!skb) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: Failed to generate protection frame type %u\n",
			   protection_type);
		return -ENOMEM;
	}

	skb_queue_tail(mpdu_q, skb);
	tx_prot_ppdu_info->contains_host_frames = true;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX monitor: Generated protection frame type %u\n",
		   protection_type);

	return 0;
}

/**
 * ath12k_dp_tx_mon_free_last_mpdu_q() - Free incomplete MPDU from queue
 * @dp_mon_pdev: Monitor pdev context
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * Frees the last incomplete MPDU from the specified user's queue.
 */
void ath12k_dp_tx_mon_free_last_mpdu_q(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				       struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				       u32 usr_idx)
{
	struct hal_rx_mon_mpdu_info *mpdu_info;
	struct sk_buff_head *mpdu_q;
	struct sk_buff *skb;

	if (!dp_mon_pdev || !tx_ppdu_info)
		return;

	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];
	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];

	/*
	 * Only clean up if MPDU end was not received
	 * This indicates an incomplete MPDU that needs cleanup
	 */
	if (!mpdu_info->mpdu_end_received) {
		skb = skb_dequeue_tail(mpdu_q);
		if (skb)
			dev_kfree_skb_any(skb);

		/* Mark MPDU as ended to prevent further processing */
		mpdu_info->mpdu_end_received = true;
		mpdu_info->mpdu_start_received = false;
	}
}

/**
 * ath12k_dp_tx_mon_process_mpdu_start() - Process MPDU start TLV
 * @dp_pdev: DP pdev handle
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_process_mpdu_start(struct ath12k_pdev_dp *dp_pdev,
				    struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				    u32 usr_idx)
{
	struct sk_buff_head *mpdu_q;
	struct sk_buff *skb;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;

	if (usr_idx >= HAL_MAX_UL_MU_USERS) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: Invalid user index %u >= %u\n",
			    usr_idx, HAL_MAX_UL_MU_USERS);
		return -EINVAL;
	}

	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];
	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];

	if (!mpdu_info->mpdu_end_received) {
		skb = skb_dequeue_tail(mpdu_q);
		if (likely(skb))
			dev_kfree_skb_any(skb);
	}

	mpdu_info->mpdu_start_received = true;
	mpdu_info->mpdu_end_received = false;

	skb = dev_alloc_skb(ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);
	if (unlikely(!skb)) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: SKB allocation failed for user %u\n",
			    usr_idx);
		mpdu_info->mpdu_start_received = false;
		return -ENOMEM;
	}

	IEEE80211_SKB_CB(skb)->status.rates[0].idx = ATH12K_RATE_STATUS_DEFAULT_IDX;

	skb_reserve(skb, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);
	tx_ppdu_info->contains_host_frames = false;
	skb_queue_tail(mpdu_q, skb);

	if (skb_queue_len(mpdu_q) > 1)
		tx_ppdu_info->tx_info.rx_status.userstats[usr_idx].ampdu_present = true;

	ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Mon: MPDU header SKB allocated for user %u (reserved=%u)\n",
		   usr_idx, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);

	return 0;
}

/**
 * ath12k_dp_tx_mon_generate_data_frm() - Generate data frame with fragments
 * @dp_pdev: DP pdev handle
 * @ppdu_info: Data PPDU information
 * @user_idx: User index
 * @take_ref: Whether to take reference on buffer page
 *           - true: Increment page reference count (buffer may be reused);
 *                   uses buffer/offset/length from TX monitor data status info
 *           - false: Transfer page ownership (buffer consumed)
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_dp_tx_mon_generate_data_frm(struct ath12k_pdev_dp *dp_pdev,
				       struct dp_mon_tx_ppdu_info *ppdu_info,
				       u8 user_idx, bool take_ref)
{
	struct sk_buff *skb, *tmp_skb;
	struct hal_rx_mon_ppdu_info *rx_status;
	struct sk_buff_head *mpdu_q;
	struct hal_tx_mon_ppdu_info *tx_info;
	void *buffer_addr;
	u32 buffer_length;
	struct page *page;
	int frag_offset;

	if (!ppdu_info || user_idx >= HAL_MAX_UL_MU_USERS)
		return -EINVAL;

	tx_info = &ppdu_info->tx_info;
	rx_status = &tx_info->rx_status;
	mpdu_q = &rx_status->mpdu_q[user_idx];

	skb = skb_peek_tail(mpdu_q);
	if (!skb)
		return -ENOENT;

	if (ppdu_info->has_buffer_data && ppdu_info->buffer_addr) {
		buffer_addr = ppdu_info->buffer_addr;
		buffer_length = ppdu_info->buffer_length;
		page = ppdu_info->page;
		frag_offset = ppdu_info->frag_offset;

		tmp_skb = ath12k_dp_mon_get_skb_valid_frag(dp_pdev->dp, skb);
		if (!tmp_skb) {
			tmp_skb = dev_alloc_skb(ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);
			if (!tmp_skb) {
				ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
				ath12k_warn(dp_pdev->dp->ab,
					    "TX Mon: SKB allocation failed for frag renewal, user %u\n",
					    user_idx);
				return -ENOMEM;
			}
			ath12k_dp_mon_append_skb(skb, tmp_skb);
		}

		skb_add_rx_frag(tmp_skb, skb_shinfo(tmp_skb)->nr_frags,
				page,
				frag_offset,
				buffer_length,
				ATH12K_DP_MON_TX_BUF_SIZE);

		if (tmp_skb != skb)
			ath12k_dp_mon_update_skb_len(skb, buffer_length);

		if (take_ref)
			skb_frag_ref(tmp_skb, skb_shinfo(tmp_skb)->nr_frags - 1);
	}
	return 0;
}

/**
 * ath12k_dp_tx_mon_extract_buffer_info() - Extract buffer information only
 * @dp_mon: DP monitor handle
 * @tx_ppdu_info: PPDU info structure
 * @usr_idx: User index
 *
 * This function ONLY extracts buffer info and stores it in ppdu_info.
 * It does NOT add fragments - that's done by ath12k_dp_tx_mon_generate_data_frm().
 * Incorporates robust validation and error handling from original implementation.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_extract_buffer_info(struct ath12k_pdev_dp *dp_pdev,
				     struct dp_mon_tx_ppdu_info *tx_ppdu_info,
				     u32 usr_idx)
{
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;
	struct hal_tx_mon_ppdu_info *hal_info;
	struct hal_tx_mon_packet_info *packet_info = NULL;
	struct list_head mon_desc_used_list;
	struct ath12k_dp_mon_desc *mon_desc;
	struct sk_buff_head *mpdu_q;
	struct sk_buff *header_skb;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	int ret = 0;

	INIT_LIST_HEAD(&mon_desc_used_list);

	if (usr_idx >= HAL_MAX_UL_MU_USERS) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid user index %u >= %u\n",
			    usr_idx, HAL_MAX_UL_MU_USERS);
		return -EINVAL;
	}

	mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];

	if (!mpdu_info->mpdu_start_received) {
		ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Buffer without MPDU start for user %u\n",
			   usr_idx);
		return -EINVAL;
	}

	mpdu_q = &tx_ppdu_info->tx_info.rx_status.mpdu_q[usr_idx];

	hal_info = &tx_ppdu_info->tx_info;
	packet_info = &hal_info->packet_info;

	if (!packet_info || !packet_info->sw_cookie) {
		tx_ppdu_info->has_buffer_data = false;
		ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: No packet buffer info for user %u\n",
			   usr_idx);
		return 0;
	}

	mon_desc = (struct ath12k_dp_mon_desc *)(uintptr_t)(packet_info->sw_cookie);

	if (unlikely(mon_desc->magic != ATH12K_MON_MAGIC_VALUE)) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, magic_value_error);
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid magic value 0x%x for user %u\n",
			    mon_desc->magic, usr_idx);
		ret = -EINVAL;
		goto return_mon_desc;
	}

	if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid descriptor state %d for user %u\n",
			    mon_desc->in_use, usr_idx);
		ret = -EINVAL;
		goto return_mon_desc;
	}

	mon_desc->in_use = DP_MON_DESC_REPLENISH;
	list_add_tail(&mon_desc->list, &mon_desc_used_list);
	tx_ppdu_info->buffer_addr = mon_desc->mon_buf;
	mon_desc->mon_buf = NULL;

	if (!tx_ppdu_info->buffer_addr) {
		ret = -EINVAL;
		goto return_mon_desc;
	}

	if (packet_info->dma_length > ATH12K_DP_MON_TX_BUF_SIZE) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: Invalid DMA length %u for user %u\n",
			    packet_info->dma_length, usr_idx);
		ret = -EINVAL;
		page_frag_free(tx_ppdu_info->buffer_addr);
		goto return_mon_desc;
	}

	header_skb = skb_peek_tail(mpdu_q);
	if (!header_skb) {
		ath12k_warn(dp_mon->dp->ab,
			    "TX Mon: No header SKB in queue for user %u\n",
			    usr_idx);
		ret = -EINVAL;
		page_frag_free(tx_ppdu_info->buffer_addr);
		goto return_mon_desc;
	}

	ath12k_core_dma_unmap_page(dp_mon->dp->dev, mon_desc->paddr,
				   ATH12K_DP_MON_TX_BUF_SIZE,
				   DMA_FROM_DEVICE);

	tx_ppdu_info->buffer_length = packet_info->dma_length;
	tx_ppdu_info->msdu_continuation = packet_info->msdu_continuation;
	tx_ppdu_info->truncated = packet_info->truncated;
	tx_ppdu_info->has_buffer_data = true;
	tx_ppdu_info->page = virt_to_head_page(tx_ppdu_info->buffer_addr);
	tx_ppdu_info->frag_offset = (tx_ppdu_info->buffer_addr -
				     page_address(tx_ppdu_info->page));
	ATH12K_TX_MON_STAT_INC(dp_pdev, pkt_buf_processed);

return_mon_desc:

	ath12k_dp_mon_tx_desc_free(dp_pdev, &mon_desc_used_list, dp_mon);
	return ret;
}

/**
 * ath12k_dp_tx_mon_generate_ack_frm() - Generate ACK frame
 * @tx_data_ppdu_info: Data PPDU information
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_generate_ack_frm(struct ath12k_pdev_dp *dp_pdev,
					struct dp_mon_tx_ppdu_info *tx_data_ppdu_info,
					struct hal_tx_mon_status_info *status_info)
{
	struct sk_buff *skb;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct sk_buff_head *usr_mpdu_q;
	struct ieee80211_frame_min *ack = NULL;
	u16 frame_control;
	u32 frame_len = sizeof(struct ieee80211_frame_min);

	if (!tx_data_ppdu_info || !status_info)
		return -EINVAL;

	tx_info = &tx_data_ppdu_info->tx_info;
	usr_mpdu_q = &tx_info->rx_status.mpdu_q[0];

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR + frame_len);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate ack frame.");
		return -ENOMEM;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
	ack = (struct ieee80211_frame_min *)skb_put_zero(skb, frame_len);

	frame_control = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK;
	ack->frame_control = cpu_to_le16(frame_control);

	/* Update PPDU info */
	tx_info->rx_status.frame_control = frame_control;
	tx_info->rx_status.frame_control_info_valid = 1;

	ack->duration = cpu_to_le16(0);

	memcpy(ack->ra, status_info->addr1, ETH_ALEN);

	skb_queue_tail(usr_mpdu_q, skb);
	tx_info->is_used = 1;
	return 0;
}

/**
 * ath12k_dp_tx_mon_check_ba_tlv_missing() - Check if BA TLV is missing
 * @ppdu_info: PPDU info structure
 *
 * Simple check if Block ACK TLV information is missing by validating ba_user_id.
 *
 * Return: true if BA TLV is missing, false otherwise
 */
static inline bool
ath12k_dp_tx_mon_check_ba_tlv_missing(struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_ppdu_info *tx_info;

	if (unlikely(!ppdu_info))
		return true;

	tx_info = &ppdu_info->tx_info;
	if (!tx_info)
		return true;

	if (unlikely(tx_info->ba_user_id == ATH12K_BA_USER_ID_INVAL))
		return true;

	return false;
}

/**
 * ath12k_dp_tx_mon_gen_block_ack() - Generate Block ACK frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for BA parameters
 *
 * Creates Block ACK frame with proper BA control field, sequence control,
 * and bitmap based on TLV information.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_gen_block_ack(struct ath12k_pdev_dp *dp_pdev,
					struct dp_mon_tx_ppdu_info *ppdu_info,
					struct hal_tx_mon_status_info *status_info,
					u8 window_flag)
{
	struct sk_buff *skb;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct sk_buff_head *usr_mpdu_q;
	struct ieee80211_ctl_frm *ba_hdr;
	u32 ba_hdr_len = sizeof(struct ieee80211_ctl_frm);
	u32 user_id, ba_bitmap_sz, bitmap_bytes;
	u32 total_frame_sz;
	u16 frm_ctl;
	u8 *frm;

	if (!status_info)
		return -EINVAL;

	tx_info = &ppdu_info->tx_info;
	usr_mpdu_q = &tx_info->rx_status.mpdu_q[0];
	user_id = tx_info->ba_user_id;
	ba_bitmap_sz = tx_info->rx_status.userstats[user_id].ba_bitmap_sz;

	if (ba_bitmap_sz > ATH12K_DP_MON_TX_BA_BITMAP_SZ_MAX) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
					"BA frame gen failed: bitmap_sz %u exceeds max %u for user %u\n",
					ba_bitmap_sz, ATH12K_DP_MON_TX_BA_BITMAP_SZ_MAX,
					user_id);
		return -EINVAL;
	}

	bitmap_bytes = ATH12K_DP_MON_TX_BA_BITMAP_BYTES(ba_bitmap_sz);

	total_frame_sz = ba_hdr_len +
		ATH12K_DP_MON_TX_BA_CTRL_SZ +
		ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ +
		bitmap_bytes;

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR + total_frame_sz);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate block ack frame.");
		return -ENOMEM;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	ba_hdr = (struct ieee80211_ctl_frm *)skb_put_zero(skb, ba_hdr_len);

	frm_ctl = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK;
	ba_hdr->frame_control = cpu_to_le16(frm_ctl);

	tx_info->rx_status.frame_control = frm_ctl;
	tx_info->rx_status.frame_control_info_valid = 1;

	ba_hdr->duration = cpu_to_le16(ATH12K_BA_DURATION_US);

	if (window_flag) {
		memcpy(ba_hdr->addr1, status_info->addr1, ETH_ALEN);
		memcpy(ba_hdr->addr2, status_info->addr2, ETH_ALEN);
	} else {
		memcpy(ba_hdr->addr1, status_info->addr2, ETH_ALEN);
		memcpy(ba_hdr->addr2, status_info->addr1, ETH_ALEN);
	}

	frm = skb_put(skb, ATH12K_DP_MON_TX_BA_CTRL_SZ +
		      ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ + bitmap_bytes);

	*((u16 *)frm) = cpu_to_le16(tx_info->rx_status.userstats[user_id].ba_control);
	frm += 2;

	*((u16 *)frm) = cpu_to_le16(tx_info->rx_status.userstats[user_id].start_seq);
	frm += 2;

	memcpy(frm, tx_info->rx_status.userstats[user_id].ba_bitmap, bitmap_bytes);

	skb_queue_tail(usr_mpdu_q, skb);
	tx_info->is_used = 1;

	return 0;
}

/**
 * ath12k_dp_tx_mon_add_mu_ba_per_user_info() - Add per-user info to MU Block ACK
 * @tx_info: HAL PPDU info structure
 * @frame_ptr: Pointer to current position in frame
 * @user_id: User index
 *
 * Adds per-user TID info, starting sequence, and bitmap to MU Block ACK frame.
 *
 * Return: Updated frame pointer or NULL on error
 */
static u8 *
ath12k_dp_tx_mon_add_mu_ba_per_user_info(struct hal_tx_mon_ppdu_info *tx_info,
					 u8 *frame_ptr, u8 user_id)
{
	struct hal_rx_user_status *user_status;
	u16 per_aid_tid_info;
	u16 start_seq_ctrl;
	u8 bitmap_sz;
	u32 bitmap_bytes;

	user_status = &tx_info->rx_status.userstats[user_id];
	bitmap_sz = user_status->ba_bitmap_sz;
	bitmap_bytes = 4 << bitmap_sz;

	per_aid_tid_info = ((user_status->tid << 12) |
			    (user_status->aid & 0x7FF));
	*((u16 *)frame_ptr) = cpu_to_le16(per_aid_tid_info);
	frame_ptr += 2;

	start_seq_ctrl = user_status->start_seq;
	*((u16 *)frame_ptr) = cpu_to_le16(start_seq_ctrl);
	frame_ptr += 2;

	memcpy(frame_ptr, user_status->ba_bitmap, bitmap_bytes);
	frame_ptr += bitmap_bytes;

	return frame_ptr;
}

/**
 * ath12k_dp_tx_mon_gen_mu_block_ack() - Generate MU Block ACK frame
 * @ppdu_info: PPDU info structure
 * @status_info: TX status info for MU BA parameters
 *
 * Creates Multi-User Block ACK frame with per-user BA information
 * including AID, TID, and sequence numbers for MU scenarios.
 *
 * Return: Generated sk_buff or NULL on failure
 */
static struct sk_buff *
ath12k_dp_tx_mon_gen_mu_block_ack(struct ath12k_pdev_dp *dp_pdev,
					struct hal_tx_mon_ppdu_info *tx_info,
					struct hal_tx_mon_status_info *status_info,
					u8 window_flag, u8 num_users)
{
	struct sk_buff *skb;
	struct ieee80211_mu_block_ack_hdr *mu_ba_hdr;
	u32 ba_bitmap_sz, total_frame_sz = 0;
	u16 frm_ctl;
	u8 *per_user_info;
	u8 i;

	total_frame_sz = sizeof(struct ieee80211_mu_block_ack_hdr);

	for (i = 0; i < num_users; i++) {
		ba_bitmap_sz = tx_info->rx_status.userstats[i].ba_bitmap_sz;
		total_frame_sz += ATH12K_DP_MON_TX_MU_BA_INFO_SZ(ba_bitmap_sz);
	}

	skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR + total_frame_sz);
	if (!skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate mu block ack frame.");
		return NULL;
	}

	skb_reserve(skb, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

	mu_ba_hdr = (struct ieee80211_mu_block_ack_hdr *)skb_put_zero(skb,
					sizeof(struct ieee80211_mu_block_ack_hdr));

	frm_ctl = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK;
	mu_ba_hdr->frame_control = cpu_to_le16(frm_ctl);
	mu_ba_hdr->duration = cpu_to_le16(0);

	tx_info->rx_status.frame_control = frm_ctl;
	tx_info->rx_status.frame_control_info_valid = 1;

	if (window_flag == RESPONSE_WINDOW) {
		memcpy(mu_ba_hdr->ta, status_info->addr2, ETH_ALEN);
		if (num_users > 1)
			memset(mu_ba_hdr->ra, 0xFF, ETH_ALEN);
		else
			memcpy(mu_ba_hdr->ra, status_info->addr1, ETH_ALEN);
	} else {
		memcpy(mu_ba_hdr->ta, status_info->addr1, ETH_ALEN);
		memcpy(mu_ba_hdr->ra, status_info->addr2, ETH_ALEN);
	}

	mu_ba_hdr->ba_control = cpu_to_le16(ATH12K_MU_BA_CTRL_MULTI_TID);

	per_user_info =
		skb_put(skb,
			total_frame_sz - sizeof(struct ieee80211_mu_block_ack_hdr));

	for (i = 0; i < num_users; i++) {
		per_user_info = ath12k_dp_tx_mon_add_mu_ba_per_user_info(tx_info,
									 per_user_info,
									 i);
		if (!per_user_info) {
			dev_kfree_skb(skb);
			return NULL;
		}
	}

	tx_info->is_used = 1;
	return skb;
}

/**
 * ath12k_dp_tx_mon_generate_mu_block_ack_frm() - Generate and enqueue MU Block ACK
 * @dp_pdev: DP pdev handle
 * @tx_ppdu_info: PPDU info structure
 * @window_flag: Window flag for address ordering
 * @mac_id: MAC ID
 *
 * Generates MU Block ACK frame and enqueues to user 0 MPDU queue.
 * Only processes when called for the last user.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_generate_mu_block_ack_frm(struct ath12k_pdev_dp *dp_pdev,
					   struct dp_mon_tx_ppdu_info *tx_ppdu_info,
					   struct hal_tx_mon_status_info *status_info,
					   u8 window_flag)
{
	struct sk_buff *mu_ba_skb;
	struct hal_tx_mon_ppdu_info *tx_info;
	struct sk_buff_head *usr_mpdu_q;
	u8 num_users;
	u8 ba_user_id;

	if (!status_info)
		return -EINVAL;

	tx_info = &tx_ppdu_info->tx_info;
	num_users = tx_info->num_users;
	if (num_users == 0 || num_users > HAL_MAX_UL_MU_USERS) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Invalid users for MU Block ACK\n");
		return -EINVAL;
	}

	ba_user_id = tx_info->ba_user_id;
	/* Only the last user should proceed with MU Block ACK generation */
	if (ba_user_id != num_users - 1) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Skipping MU Block ACK for user %u (not last user)\n",
			   ba_user_id);
		return 0;
	}

	mu_ba_skb = ath12k_dp_tx_mon_gen_mu_block_ack(dp_pdev, tx_info,
		status_info, window_flag, num_users);
	if (!mu_ba_skb) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: Failed to generate MU Block ACK frame\n");
		return -ENOMEM;
	}

	usr_mpdu_q = &tx_info->rx_status.mpdu_q[0];
	skb_queue_tail(usr_mpdu_q, mu_ba_skb);

	tx_info->rx_status.he_mu_flags = 0;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Mon: Generated and enqueued MU Block ACK for %u users\n",
		   num_users);

	return 0;
}

/**
 * ath12k_dp_tx_mon_generated_response_frm() - Generate response frames
 * @dp_pdev: DP pdev handle
 * @tx_data_ppdu_info: Data PPDU information
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_generated_response_frm(struct ath12k_pdev_dp *dp_pdev,
					struct dp_mon_tx_ppdu_info *tx_data_ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct hal_tx_mon_status_info *tx_status_info;
	u8 gen_response = 0;
	int ret = 0;

	tx_status_info = &dp_mon_pdev->mon_data.data_status_info;
	gen_response = tx_status_info->generated_response;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX monitor: Processing response frame, type=%u\n",
		   gen_response);

	switch (gen_response) {
	case TXMON_GEN_RESP_SELFGEN_ACK:
		ret = ath12k_dp_tx_mon_generate_ack_frm(dp_pdev,
			tx_data_ppdu_info, tx_status_info);
		break;
	case TXMON_GEN_RESP_SELFGEN_CTS:
		ret = ath12k_dp_tx_mon_gen_cts(dp_pdev,
			tx_data_ppdu_info, tx_status_info);
		break;
	case TXMON_GEN_RESP_SELFGEN_BA:
		if (ath12k_dp_tx_mon_check_ba_tlv_missing(tx_data_ppdu_info))
			break;

		ret = ath12k_dp_tx_mon_gen_block_ack(dp_pdev,
								tx_data_ppdu_info,
						     tx_status_info,
						     RESPONSE_WINDOW);
		break;
	case TXMON_GEN_RESP_SELFGEN_MBA:
		if (ath12k_dp_tx_mon_check_ba_tlv_missing(tx_data_ppdu_info))
			break;

		ret = ath12k_dp_tx_mon_generate_mu_block_ack_frm(dp_pdev,
								 tx_data_ppdu_info,
								 tx_status_info,
								 RESPONSE_WINDOW);
		break;
	case TXMON_GEN_RESP_SELFGEN_CBF:
		break;
	case TXMON_GEN_RESP_SELFGEN_TRIG:
		break;
	case TXMON_GEN_RESP_SELFGEN_NDP_LMR:
		break;
	default:
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: No response frame needed, type=%u\n",
			   gen_response);
		break;
	}

	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "Failed to generate response frame\n");
		return ret;
	}

	tx_data_ppdu_info->contains_host_frames = true;
	return 0;
}

static int
dp_tx_mon_setup_buf_from_status(struct ath12k_pdev_dp *dp_pdev,
					struct dp_mon_tx_ppdu_info *ppdu_info)
{
	struct hal_tx_mon_status_info *status_info =
		&dp_pdev->dp_mon_pdev->mon_data.data_status_info;

	if (!status_info->buffer || !status_info->length)
		return -ENOENT;

	if (!ppdu_info->buffer_addr) {
		ppdu_info->buffer_addr = status_info->buffer;
		ppdu_info->buffer_length = status_info->length;
		ppdu_info->has_buffer_data = true;
		ppdu_info->page = virt_to_head_page(ppdu_info->buffer_addr);
		ppdu_info->frag_offset = status_info->offset;
	}

	return 0;
}

/**
 * ath12k_dp_tx_mon_update_ppdu_info_status() - Update PPDU info based on TLV status
 * @dp_pdev: DP pdev handle
 * @tx_ppdu_info: Could be either of below
 * tx_data_ppdu_info: Data PPDU information structure
 * tx_prot_ppdu_info: Protection PPDU information structure
 * @tx_tlv_hdr: TLV header pointer
 * @status_frag: Status fragment buffer
 * @tlv_status: TLV status from HAL parsing
 *
 * This function processes different TLV status types and performs
 * appropriate actions including frame generation, buffer management,
 * and PPDU information updates. It serves as the central dispatcher
 * for TLV-based processing in TX monitor.
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_dp_tx_mon_update_ppdu_info_status(struct ath12k_pdev_dp *dp_pdev,
					 struct dp_mon_tx_ppdu_info *tx_ppdu_info,
					 u32 tlv_status)
{
	struct hal_tx_mon_ppdu_info *tx_info;
	struct hal_tx_mon_status_info *status_info;
	struct hal_rx_mon_mpdu_info *mpdu_info;
	u32 usr_idx;
	int ret = 0;

	if (unlikely(!tx_ppdu_info))
		return -EINVAL;

	tx_info = &tx_ppdu_info->tx_info;
	usr_idx = tx_info->cur_usr_idx;

	switch (tlv_status) {
	case HAL_TX_MON_FES_SETUP:
		break;

	case HAL_TX_MON_RESPONSE_REQUIRED_INFO:
		break;

	case HAL_TX_MON_FES_STATUS_START_PROT:
		break;

	case HAL_TX_MON_FES_STATUS_START_PPDU:
		break;

	case HAL_TX_MON_FES_STATUS_PROT:
		tx_ppdu_info->tx_info.rx_status.ppdu_ts =
			tx_ppdu_info->tx_info.rx_status.ppdu_ts << 1;

		ret = ath12k_dp_tx_mon_generate_prot_frm(dp_pdev,
							 tx_ppdu_info);
		break;

	case HAL_TX_MON_MPDU_START:
		ret = ath12k_dp_tx_mon_process_mpdu_start(dp_pdev,
							  tx_ppdu_info,
							  usr_idx);
		if (ret) {
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: Failed to process MPDU start: %d\n",
				    ret);
		}
		break;

	case HAL_TX_MON_MPDU_END:
		/* MPDU end - mark MPDU as complete */
		if (usr_idx < HAL_MAX_UL_MU_USERS) {
			mpdu_info = &tx_ppdu_info->tx_info.rx_status.mpdu_info[usr_idx];
			mpdu_info->mpdu_end_received = true;
			mpdu_info->mpdu_start_received = false;
		}
		break;

	case HAL_TX_MON_MSDU_START:
		/* MSDU start processing */
		break;

	case HAL_TX_MON_DATA:
		/* Data frame generation - keep buffer reference for reuse */
		tx_info->is_used = 1;
		ret = dp_tx_mon_setup_buf_from_status(dp_pdev, tx_ppdu_info);
		if (ret) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, data_gen_failed);
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: Failed to setup data buffer for user %u: %d\n",
				    usr_idx, ret);
			tx_info->is_used = 0;
			break;
		}

		ret = ath12k_dp_tx_mon_generate_data_frm(dp_pdev, tx_ppdu_info,
							 usr_idx, true);
		if (ret) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, data_gen_failed);
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: Failed to gen data frm for user %u: %d\n",
				    usr_idx, ret);
			tx_info->is_used = 0;
		}
		break;

	case HAL_TX_MON_BUFFER_ADDR:
		/*
		 * Buffer address processing - extract buffer and transfer ownership
		 */
		tx_info->is_used = 1;
		ret = ath12k_dp_tx_mon_extract_buffer_info(dp_pdev,
							   tx_ppdu_info, usr_idx);
		if (ret) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, buf_extract_failed);
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: Failed to extract buffer info: %d\n",
				    ret);
			tx_info->is_used = 0;
		} else {
			ret = ath12k_dp_tx_mon_generate_data_frm(dp_pdev, tx_ppdu_info,
								 usr_idx, false);
			if (ret) {
				ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, data_gen_failed);
				ath12k_warn(dp_pdev->dp->ab,
					    "TX Mon: Failed to add buffer fragment: %d\n",
					    ret);
				tx_info->is_used = 0;
			}
		}
		break;

	case HAL_TX_MON_FES_STATUS_END:
		/*
		 * FES status end - clean up incomplete MPDUs for all users
		 */
		u32 num_users = tx_ppdu_info->tx_info.num_users;
		u8 i;

		for (i = 0; i < num_users && i < HAL_MAX_UL_MU_USERS; i++) {
			ath12k_dp_tx_mon_free_last_mpdu_q(dp_pdev->dp_mon_pdev,
							  tx_ppdu_info,
							  i);
		}
		break;

	case HAL_TX_MON_RESPONSE_END_STATUS_INFO:
		ret = ath12k_dp_tx_mon_generated_response_frm(dp_pdev,
							      tx_ppdu_info);
		break;

	case HAL_TX_MON_FW2SW:
		status_info = &dp_pdev->dp_mon_pdev->mon_data.data_status_info;
		tx_ppdu_info->tx_info.rx_status.freq = status_info->freq;

		break;

	default:
		break;
	}

	return ret;
}

/**
 * ath12k_dp_mon_tx_process_tlv() - Process TLV data with early filtering
 * @dp_pdev: DP PDEV context
 * @status_desc: Status descriptor containing TLV data
 * @mon_data: Monitor data structure
 *
 * This function parses TLV data using HAL functions and applies early software
 * filtering during TLV processing for optimal performance.
 */
static void
ath12k_dp_mon_tx_process_tlv(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_dp_mon_status_desc *status_desc,
			     struct ath12k_mon_data *mon_data)
{
	struct dp_mon_tx_ppdu_info *ppdu_info;
	struct hal_tlv_64_hdr *tlv_hdr;
	u16 tlv_tag, tlv_len, tlv_userid;
	u16 buf_len = status_desc->buf_len;
	u8 *tx_tlv_start = status_desc->mon_buf;
	u8 *mon_buf_iter = status_desc->mon_buf;
	enum hal_tx_mon_status tlv_status;
	bool is_prot_ppdu;
	int ret;

	do {
		tlv_hdr = (struct hal_tlv_64_hdr *)mon_buf_iter;

		tlv_tag = ath12k_hal_get_tlv_hdr_tag(&dp_pdev->dp->ab->hal, tlv_hdr->tl);
		tlv_len = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_HDR_LEN);
		tlv_userid = le64_get_bits(tlv_hdr->tl, HAL_TLV_64_USR_ID);

		ppdu_info = ath12k_hal_mon_tx_ppdu_info(&dp_pdev->dp->ab->hal,
							mon_data, tlv_tag);
		is_prot_ppdu = (ppdu_info == &mon_data->prot_ppdu_info);

		tlv_status =
			ath12k_hal_mon_tx_parse_status(&dp_pdev->dp->ab->hal,
						       &ppdu_info->tx_info,
						       &mon_data->data_status_info,
						       &mon_data->prot_status_info,
						       is_prot_ppdu,
						       tlv_tag,
						       mon_buf_iter + sizeof(*tlv_hdr),
						       tlv_userid, tlv_len,
						       tx_tlv_start);

		/* Process TLV status and update PPDU information */
		ret = ath12k_dp_tx_mon_update_ppdu_info_status(dp_pdev,
							       ppdu_info,
							       tlv_status);
		if (ret) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, tlv_process_failed);
			ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "TLV status processing failed: %d", ret);
		}

		mon_buf_iter += sizeof(*tlv_hdr) + tlv_len;
		mon_buf_iter = PTR_ALIGN(mon_buf_iter, HAL_TLV_64_ALIGN);

	} while ((mon_buf_iter - tx_tlv_start) < buf_len);
}

/**
 * ath12k_dp_mon_tx_populate_ppdu_info() - Populate PPDU with channel and metadata
 * @dp_pdev: Pointer to DP PDEV context for accessing radio information
 * @status_desc: Pointer to status descriptor (currently unused but reserved)
 * @mon_data: Pointer to monitor data containing PPDU information structures
 *
 * This function populates PPDU information structures with essential metadata
 * including channel information, band classification, and AMPDU detection for
 * both protection and data frame PPDUs. It serves as the central point for
 * enriching hardware-provided PPDU data with software-derived information.
 *
 * Channel Information Population:
 * The function extracts current channel information from the active radio
 * context (ar->rx_channel) and populates both data and protection PPDU
 * structures with:
 * - Channel frequency in MHz for radiotap channel field
 * - Channel number for protocol analysis
 * - Band classification (2.4GHz, 5GHz, 6GHz) for proper frame handling
 */
static void
ath12k_dp_mon_tx_populate_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_mon_status_desc *status_desc,
				    struct ath12k_mon_data *mon_data)
{
	struct dp_mon_tx_ppdu_info *data_ppdu_info = &mon_data->data_ppdu_info;
	struct dp_mon_tx_ppdu_info *prot_ppdu_info = &mon_data->prot_ppdu_info;
	struct hal_tx_mon_ppdu_info *data_hal_info = &data_ppdu_info->tx_info;
	struct hal_tx_mon_ppdu_info *prot_hal_info = &prot_ppdu_info->tx_info;
	struct hal_rx_mon_ppdu_info *data_rx_status = &data_hal_info->rx_status;
	struct hal_rx_mon_ppdu_info *prot_rx_status = &prot_hal_info->rx_status;
	struct ath12k *ar = dp_pdev->ar;
	u32 usr_idx, num_users;

	num_users = data_hal_info->num_users;
	if (ar && ar->rx_channel) {
		u32 chan_freq = ar->rx_channel->center_freq;
		u32 chan_num = ar->rx_channel->hw_value;
		u32 band;

		if (chan_freq >= ATH12K_FREQ_2GHZ_MIN &&
		    chan_freq <= ATH12K_FREQ_2GHZ_MAX)
			band = NL80211_BAND_2GHZ;
		else if (chan_freq >= ATH12K_FREQ_5GHZ_MIN &&
			 chan_freq <= ATH12K_FREQ_5GHZ_MAX)
			band = NL80211_BAND_5GHZ;
		else if (chan_freq >= ATH12K_FREQ_6GHZ_MIN &&
			 chan_freq <= ATH12K_FREQ_6GHZ_MAX)
			band = NL80211_BAND_6GHZ;
		else
			band = NL80211_BAND_2GHZ;

		if (unlikely(!data_rx_status->freq)) {
			data_ppdu_info->chan_freq = chan_freq;
			data_ppdu_info->chan_num = chan_num;
			data_rx_status->freq = chan_freq;
			data_rx_status->chan_num = chan_num;
			data_rx_status->band = band;
		}

		if (unlikely(!prot_rx_status->freq)) {
			prot_ppdu_info->chan_freq = chan_freq;
			prot_ppdu_info->chan_num = chan_num;
			prot_rx_status->freq = chan_freq;
			prot_rx_status->chan_num = chan_num;
			prot_rx_status->band = band;
		}
	}

	if (num_users == 1) {
		ath12k_dp_tx_mon_update_ampdu_info(data_ppdu_info, 0);
	} else {
		for (usr_idx = 0;
		     usr_idx < num_users && usr_idx < HAL_MAX_UL_MU_USERS;
		     usr_idx++) {
			ath12k_dp_tx_mon_update_ampdu_info(data_ppdu_info, usr_idx);
		}
	}
}

/**
 * ath12k_dp_tx_mon_validate_lsig_support() - Check if frame should have L-SIG
 * @rx_status: RX status information
 *
 * L-SIG is required for all OFDM-based modulations (802.11a/g/n/ac/ax/be)
 * but not for legacy CCK frames (802.11b).
 *
 * Return: true if frame should have L-SIG field in radiotap
 */
static bool
ath12k_dp_tx_mon_validate_lsig_support(struct hal_rx_mon_ppdu_info *rx_status)
{
	switch (rx_status->preamble_type) {
	case HAL_RX_PREAMBLE_11A:
	case HAL_RX_PREAMBLE_11N:
	case HAL_RX_PREAMBLE_11AC:
	case HAL_RX_PREAMBLE_11AX:
	case HAL_RX_PREAMBLE_11BE:
		return true;

	case HAL_RX_PREAMBLE_11B:
		return false;

	default:
		if (rx_status->freq > ATH12K_FREQ_2GHZ_MAX) {
			return true;
		} else {
			return rx_status->ofdm_flag ||
				(rx_status->rate >= ATH12K_RATE_6MBPS_500KBPS &&
				 rx_status->rate <= ATH12K_RATE_54MBPS_500KBPS);
		}
	}
}

/**
 * ath12k_dp_mon_tx_update_lsig_info() - Update LSIG radiotap field
 * @mon_info: Monitor info structure to populate
 * @rx_status: RX status containing HAL L-SIG data
 *
 * Extract L-SIG fields from HAL 32-bit format
 * Bits 0-3: Rate
 * Bits 5-16: Length
 *
 * Converts 32-bit HAL L-SIG fields to standard IEEE 802.11 radiotap LSIG format
 */
static void
ath12k_dp_mon_tx_update_lsig_info(struct ieee80211_tx_mon_info *mon_info,
				  struct hal_rx_mon_ppdu_info *rx_status)
{
	u32 lsig_a = rx_status->l_sig_a_info;
	u16 data1 = 0, data2 = 0;

	if (!ath12k_dp_tx_mon_validate_lsig_support(rx_status))
		return;

	if (lsig_a != 0) {
		u8 rate = u32_get_bits(lsig_a, ATH12K_LSIG_RATE_MASK);
		u16 length = u32_get_bits(lsig_a, ATH12K_LSIG_LENGTH_MASK);

		data1 = IEEE80211_RADIOTAP_LSIG_DATA1_RATE_KNOWN |
			IEEE80211_RADIOTAP_LSIG_DATA1_LENGTH_KNOWN;

		data2 = (rate & IEEE80211_RADIOTAP_LSIG_DATA2_RATE) |
			((length << ATH12K_RADIOTAP_LSIG_LENGTH_SHIFT) &
			 IEEE80211_RADIOTAP_LSIG_DATA2_LENGTH);

		mon_info->lsig.data1 = cpu_to_le16(data1);
		mon_info->lsig.data2 = cpu_to_le16(data2);

		tx_mon_hw_set(mon_info, LSIG_INFO);
	}
}

/**
 * ath12k_dp_tx_mon_get_sifs_time() - Get SIFS time based on frequency
 * @freq: Channel frequency
 *
 * Return: SIFS time in microseconds
 */
static u8 ath12k_dp_tx_mon_get_sifs_time(u16 freq)
{
	return (freq >= ATH12K_FREQ_2GHZ_MIN &&
		freq <= ATH12K_FREQ_2GHZ_MAX) ?
		ATH12K_SIFS_2GHZ_US : ATH12K_SIFS_5GHZ_US;
}

/**
 * ath12k_dp_tx_mon_get_channel_flags() - Get correct channel flags for radiotap
 *
 * Return: Channel flags for radiotap
 */
static u16
ath12k_dp_tx_mon_get_channel_flags(struct hal_rx_mon_ppdu_info *rx_status)
{
	u16 flags = 0;

	/* Set band flags */
	if (rx_status->freq >= ATH12K_FREQ_5GHZ_MIN)
		flags |= IEEE80211_CHAN_5GHZ;
	else
		flags |= IEEE80211_CHAN_2GHZ;

	if (rx_status->cck_flag)
		flags |= IEEE80211_CHAN_CCK;

	if (rx_status->ofdm_flag)
		flags |= IEEE80211_CHAN_OFDM;

	return flags;
}

static bool
ath12k_dp_tx_mon_has_rtap_eht_uhr_tlvs(const struct hal_rx_mon_ppdu_info *rx_status)
{
	return rx_status->eht_flags || rx_status->usig_flags ||
	       rx_status->is_uhr || rx_status->uhr_usig;
}

static void
ath12k_dp_tx_mon_get_rtap_eht_uhr_tlv_len(const struct hal_rx_mon_ppdu_info *rx_status,
					  u16 *total_len, u16 *eht_len, u16 *usig_len,
					  u16 *uhr_len, u16 *uhr_usig_len,
					  u16 *uhr_elr_len)
{
	if (rx_status->eht_flags) {
		*eht_len = struct_size((struct ieee80211_radiotap_eht *)NULL, user_info,
				       rx_status->eht_info.num_user_info);
		*total_len += sizeof(struct ieee80211_radiotap_tlv) + *eht_len;
	}

	if (rx_status->usig_flags) {
		*usig_len = sizeof(struct ieee80211_radiotap_eht_usig);
		*total_len += sizeof(struct ieee80211_radiotap_tlv) + *usig_len;
	}

	if (rx_status->uhr_usig) {
		*uhr_usig_len = sizeof(struct ieee80211_radiotap_uhr_usig);
		*total_len += sizeof(struct ieee80211_radiotap_tlv) + *uhr_usig_len;
	}

	if (rx_status->is_uhr && !rx_status->is_uhr_elr) {
		*uhr_len = struct_size((struct ieee80211_radiotap_uhr *)NULL, user,
				       rx_status->uhr_info.num_user_info);
		*total_len += sizeof(struct ieee80211_radiotap_tlv) + *uhr_len;
	}

	if (rx_status->is_uhr && rx_status->is_uhr_elr) {
		*uhr_elr_len = sizeof(struct ieee80211_radiotap_uhr_elr);
		*total_len += sizeof(struct ieee80211_radiotap_tlv) + *uhr_elr_len;
	}
}

static struct ieee80211_radiotap_tlv *
ath12k_dp_tx_mon_add_rtap_usig_tlv(struct ieee80211_radiotap_tlv *tlv,
				   const struct hal_rx_mon_ppdu_info *rx_status,
				   bool is_uhr)
{
	size_t offset = sizeof(struct ieee80211_radiotap_eht_usig);

	if (is_uhr) {
		struct ieee80211_radiotap_uhr_usig *uhr_usig;

		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
		tlv->len = cpu_to_le16(sizeof(*uhr_usig));
		uhr_usig = (struct ieee80211_radiotap_uhr_usig *)tlv->data;
		*uhr_usig = rx_status->u_sig_info.uhr_usig;

		return (struct ieee80211_radiotap_tlv *)&tlv->data[sizeof(*uhr_usig)];
	}

	tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
	tlv->len = cpu_to_le16(sizeof(struct ieee80211_radiotap_eht_usig));
	*(struct ieee80211_radiotap_eht_usig *)tlv->data = rx_status->u_sig_info.usig;

	return (struct ieee80211_radiotap_tlv *)&tlv->data[offset];
}

static struct ieee80211_radiotap_tlv *
ath12k_dp_tx_mon_add_rtap_eht_tlv(struct ieee80211_radiotap_tlv *tlv,
				  const struct hal_rx_mon_ppdu_info *rx_status,
				  u16 eht_len)
{
	struct ieee80211_radiotap_eht *eht;
	u16 i;
	u8 user;

	tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT);
	tlv->len = cpu_to_le16(eht_len);

	eht = (struct ieee80211_radiotap_eht *)tlv->data;
	eht->known = cpu_to_le32(rx_status->eht_info.eht.known);

	for (i = 0; i < ARRAY_SIZE(eht->data) &&
	     i < ARRAY_SIZE(rx_status->eht_info.eht.data); i++)
		eht->data[i] = cpu_to_le32(rx_status->eht_info.eht.data[i]);

	for (user = 0; user < ARRAY_SIZE(rx_status->eht_info.user_info) &&
	     user < rx_status->eht_info.num_user_info; user++)
		put_unaligned_le32(cpu_to_le32(rx_status->eht_info.user_info[user]),
				   &eht->user_info[user]);

	return (struct ieee80211_radiotap_tlv *)&tlv->data[eht_len];
}

static struct ieee80211_radiotap_tlv *
ath12k_dp_tx_mon_add_rtap_uhr_tlv(struct ieee80211_radiotap_tlv *tlv,
				  const struct hal_rx_mon_ppdu_info *rx_status,
				  u16 uhr_len)
{
	struct ieee80211_radiotap_uhr *uhr;
	u16 i;
	u8 user;

	tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_UHR);
	tlv->len = cpu_to_le16(uhr_len);

	uhr = (struct ieee80211_radiotap_uhr *)tlv->data;
	uhr->known = rx_status->uhr_info.uhr.known;

	for (i = 0; i < ARRAY_SIZE(uhr->data) &&
	     i < ARRAY_SIZE(rx_status->uhr_info.uhr.data); i++)
		uhr->data[i] = rx_status->uhr_info.uhr.data[i];

	for (user = 0; user < rx_status->uhr_info.num_user_info; user++) {
		put_unaligned_le32(rx_status->uhr_info.user_known[user],
				   &uhr->user[user].known);
		put_unaligned_le32(rx_status->uhr_info.user_info[user],
				   &uhr->user[user].info);
	}

	return (struct ieee80211_radiotap_tlv *)&tlv->data[uhr_len];
}

static void
ath12k_dp_tx_mon_add_rtap_uhr_elr_tlv(struct ieee80211_radiotap_tlv *tlv,
				      const struct hal_rx_mon_ppdu_info *rx_status,
				      u16 uhr_elr_len)
{
	struct ieee80211_radiotap_uhr_elr *uhr_elr;

	tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_UHR_ELR);
	tlv->len = cpu_to_le16(uhr_elr_len);

	uhr_elr = (struct ieee80211_radiotap_uhr_elr *)tlv->data;
	uhr_elr->known = rx_status->elr_info.known;
	uhr_elr->sig1 = rx_status->elr_info.sig1;
	uhr_elr->sig2 = rx_status->elr_info.sig2;
	uhr_elr->mark = rx_status->elr_info.mark;
}

/**
 * ath12k_dp_tx_mon_update_rtap_tlv_at_end() - prepend TX monitor radiotap TLVs
 * @mon_skb: monitor skb that carries radiotap header and 802.11 frame
 * @mon_info: TX monitor metadata passed to mac80211
 * @ppdu_info: HAL TX monitor PPDU status carrying EHT/U-SIG/UHR/ELR content
 *
 * Builds a "TLV-at-end" block and prepends it into @mon_skb. The block may
 * include EHT, EHT U-SIG, UHR U-SIG, UHR, and UHR ELR radiotap TLVs.
 */
static void ath12k_dp_tx_mon_update_rtap_tlv_at_end(struct sk_buff *mon_skb,
						    struct ieee80211_tx_mon_info *mon,
						    struct hal_tx_mon_ppdu_info *ppdu)
{
	struct hal_rx_mon_ppdu_info *rx_status = &ppdu->rx_status;
	struct ieee80211_radiotap_tlv *tlv;
	u16 total_len = 0, eht_len = 0, usig_len = 0;
	u16 uhr_len = 0, uhr_usig_len = 0, uhr_elr_len = 0;

	if (!ath12k_dp_tx_mon_has_rtap_eht_uhr_tlvs(rx_status))
		return;

	ath12k_dp_tx_mon_get_rtap_eht_uhr_tlv_len(rx_status, &total_len,
						  &eht_len, &usig_len, &uhr_len,
						  &uhr_usig_len, &uhr_elr_len);

	skb_reset_mac_header(mon_skb);
	tlv = skb_push(mon_skb, total_len);
	tx_mon_hw_set(mon, TLV_AT_END);

	if (rx_status->eht_flags)
		tlv = ath12k_dp_tx_mon_add_rtap_eht_tlv(tlv, rx_status, eht_len);

	if (rx_status->usig_flags)
		tlv = ath12k_dp_tx_mon_add_rtap_usig_tlv(tlv, rx_status, false);

	if (rx_status->uhr_usig)
		tlv = ath12k_dp_tx_mon_add_rtap_usig_tlv(tlv, rx_status, true);

	if (rx_status->is_uhr && !rx_status->is_uhr_elr)
		tlv = ath12k_dp_tx_mon_add_rtap_uhr_tlv(tlv, rx_status, uhr_len);

	if (rx_status->is_uhr && rx_status->is_uhr_elr)
		ath12k_dp_tx_mon_add_rtap_uhr_elr_tlv(tlv, rx_status, uhr_elr_len);
}

/**
 * ath12k_dp_mon_tx_update_rtap_vendor_tlv() - Populate ATH12K vendor radiotap TLV
 * @dp_pdev: Per-pdev DP context; provides the pre-allocated vendor TLV buffer
 * @mon_info: TX monitor info to update; VENDOR_TLV flag set on return
 * @rx_status: HAL PPDU status supplying device_id, tsft, l_sig_a/b_info
 *
 * Writes OUI, sub-namespace, skip_length, device_id, ppdu_start_timestamp,
 * lsig, and lsig_b into the reused vendor TLV buffer and attaches it to
 * @mon_info for radiotap delivery. All fields are refreshed on every call
 * to prevent stale values from a previous PPDU.
 */
static void
ath12k_dp_mon_tx_update_rtap_vendor_tlv(struct ath12k_pdev_dp *dp_pdev,
					struct ieee80211_tx_mon_info *mon_info,
					struct hal_rx_mon_ppdu_info *rx_status)
{
	struct ath12k_mon_data *mon_data = &dp_pdev->dp_mon_pdev->mon_data;
	struct ieee80211_radiotap_vendor_ns *v_tlv = mon_data->rtap_vendor_tlv;
	struct ath12k_rtap_vendor_ns *vendor_data;
	u8 ath_oui[] = {0x00, 0x03, 0x7f};

	if (!v_tlv) {
		ath12k_err(dp_pdev->dp->ab,
			   "TX Mon: Invalid Vendor TLV allocation in work queue\n");
		return;
	}

	mon_info->v_tlv = v_tlv;
	memcpy(v_tlv->oui, ath_oui, sizeof(v_tlv->oui));
	v_tlv->sub_namespace = 0;
	v_tlv->skip_length = cpu_to_le16(sizeof(struct ath12k_rtap_vendor_ns));
	vendor_data = (struct ath12k_rtap_vendor_ns *)&v_tlv->data;
	vendor_data->device_id = rx_status->device_id;
	vendor_data->ppdu_start_timestamp = rx_status->tsft;
	vendor_data->lsig = rx_status->l_sig_a_info;
	vendor_data->lsig_b = rx_status->l_sig_b_info;
	tx_mon_hw_set(mon_info, VENDOR_TLV);
}

/**
 * ath12k_dp_mon_tx_update_mon_info() - Comprehensive monitor info population
 * @dp_pdev: ath12k pdev dp context
 * @mon_info: mac80211 tx monitor info to fill
 * @ppdu_info: PPDU info structure containing HAL data
 * @status_info: HAL TX monitor status info
 *
 */
static void
ath12k_dp_mon_tx_update_mon_info(struct ath12k_pdev_dp *dp_pdev,
				 struct ieee80211_tx_mon_info *mon_info,
				 struct hal_tx_mon_ppdu_info *ppdu_info,
				 struct hal_tx_mon_status_info *status_info,
				 bool contains_host_frames,
				 bool is_response_frame,
				 u8 user_idx)
{
	struct hal_rx_mon_ppdu_info *rx_status;
	u8 sifs, tx_time;
	bool is_qos_data;

	if (!mon_info)
		return;

	memset(mon_info, 0, sizeof(*mon_info));

	rx_status = &ppdu_info->rx_status;
	if (!rx_status)
		return;

	if (is_response_frame) {
		sifs = ath12k_dp_tx_mon_get_sifs_time(rx_status->freq);

		if (ppdu_info->ack_recvd)
			tx_time = ATH12K_ACK_TX_TIME_US;
		else if (ppdu_info->cts_recvd)
			tx_time = ATH12K_CTS_TX_TIME_US;
		else
			tx_time = ATH12K_ACK_TX_TIME_US;

		mon_info->tsft = rx_status->tsft + sifs + tx_time;
	} else {
		if (rx_status->tsft)
			mon_info->tsft = rx_status->tsft;
	}

	tx_mon_hw_set(mon_info, END);
	tx_mon_hw_set(mon_info, FLAGS_INFO);

	mon_info->rtap_flags = 0;

	if (rx_status->sgi)
		mon_info->rtap_flags |= IEEE80211_RADIOTAP_F_SHORTGI;

	if (rx_status->cck_flag)
		mon_info->rtap_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

	if (rx_status->num_mpdu_fcs_err > 0)
		mon_info->rtap_flags |= IEEE80211_RADIOTAP_F_BADFCS;

	if (rx_status->freq) {
		mon_info->chan_freq = rx_status->freq;

		mon_info->chan_flags =
			ath12k_dp_tx_mon_get_channel_flags(rx_status);

		tx_mon_hw_set(mon_info, CHAN_INFO);
	}

	is_qos_data = ieee80211_is_data_qos(cpu_to_le16(rx_status->frame_control));

	if (is_qos_data && rx_status->userstats[user_idx].ampdu_present) {
		mon_info->ampdu_ref_num = ppdu_info->ppdu_id;
		mon_info->ampdu_flags = 0;
		mon_info->ampdu_reserved_flags = 0;
		tx_mon_hw_set(mon_info, AMPDU_STATUS_INFO);
	}

	ath12k_dp_mon_tx_update_lsig_info(mon_info, rx_status);

	if (rx_status->he_flags) {
		mon_info->he.data1 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data1);
		mon_info->he.data2 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data2);
		mon_info->he.data3 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data3);
		mon_info->he.data4 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data4);
		mon_info->he.data5 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data5);
		mon_info->he.data6 =
			cpu_to_le16(rx_status->userstats[user_idx].he_data6);
		tx_mon_hw_set(mon_info, HE_INFO);
	}

	if (rx_status->he_mu_flags) {
		mon_info->he_mu.flags1 =
			cpu_to_le16(rx_status->he_flags1 |
				    rx_status->userstats[user_idx].he_flags1);
		mon_info->he_mu.flags2 =
			cpu_to_le16(rx_status->he_flags2 |
				    rx_status->userstats[user_idx].he_flags2);

		memcpy(mon_info->he_mu.ru_ch1, &rx_status->userstats[user_idx].he_RU[0],
		       sizeof(mon_info->he_mu.ru_ch1));
		memcpy(mon_info->he_mu.ru_ch2, &rx_status->userstats[user_idx].he_RU[4],
		       sizeof(mon_info->he_mu.ru_ch2));

		tx_mon_hw_set(mon_info, HE_MU_INFO);
	}

	/* Always add vendor ns TLV */
	ath12k_dp_mon_tx_update_rtap_vendor_tlv(dp_pdev, mon_info, rx_status);
}

/**
 * ath12k_dp_ext_mon_filter_rx_ctrl() - ext_mon ctrl frame filter for TX monitor
 * @dp_pdev: DP pdev handle
 * @tx_ppdu_info: Pointer to struct hal_tx_mon_ppdu_info
 * @subtype_filter: control frame subtype bitmask (e.g. FILTER_CTRL_ACK,
 *                  FILTER_CTRL_CTS)
 * @wh: pointer to 802.11 frame header of the original frame
 *
 * Checks whether the given control frame subtype passes the ext_mon Rx filter,
 * and only then generate the required frame.
 *
 * Return: true if the frame passes the filter, else false
 */
static bool
ath12k_dp_ext_mon_filter_rx_ctrl(struct ath12k_pdev_dp *dp_pdev,
				 struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				 u32 subtype_filter,
				 struct ieee80211_hdr *wh)
{
	u16 peer_id;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_ext_mon_peer *peer;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon = NULL;

	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (!rx_ext_mon || !rx_ext_mon->enable)
		return false;

	if (rx_ext_mon->mo_enabled &&
	    (rx_ext_mon->mo.filter[ATH12K_EXT_MON_FRAME_CTRL] & subtype_filter))
		return false;

	if (tx_ppdu_info->ack_recvd) {
		rcu_read_lock();
		peer_id = tx_ppdu_info->rx_status.userstats[0].sw_peer_id;
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		/* Excluding ACK for non-connected clients */
		if (!dp_peer || dp_peer->is_vdev_peer) {
			rcu_read_unlock();
			return false;
		}
		rcu_read_unlock();
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	if (rx_ext_mon->peer_count) {
		list_for_each_entry(peer, &rx_ext_mon->peer_list, list) {
			if (!peer->peer_info.ra_addr &&
			    ether_addr_equal(peer->peer_info.mac_addr, wh->addr1)) {
				spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
				if (rx_ext_mon->fpmo_enabled &&
				    (rx_ext_mon->fpmo.filter[ATH12K_EXT_MON_FRAME_CTRL] &
				     subtype_filter))
					return true;

				return false;
			}
		}
	}
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	if (rx_ext_mon->fp_enabled &&
	    (rx_ext_mon->fp.filter[ATH12K_EXT_MON_FRAME_CTRL] & subtype_filter))
		return true;

	return false;
}

/**
 * ath12k_dp_tx_mon_generate_ack_rx_frm() - Generate ACK response frame
 * @dp_pdev: DP pdev handle
 * @mpdu_q: MPDU queue containing original frames
 * @tx_ppdu_info: TX PPDU info structure
 * @rx_ppdu_info: RX PPDU info structure
 *
 * Generate ACK response frame for TX monitor. Creates just the frame SKB
 * with proper headroom reserved for radiotap. The radiotap header will be
 * added later by ath12k_dp_mon_tx_update_mon_info() during delivery.
 *
 * Return: SKB containing ACK frame, or NULL on failure
 */
struct sk_buff *
ath12k_dp_tx_mon_generate_ack_rx_frm(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff_head *mpdu_q,
				     struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				     struct hal_tx_mon_ppdu_info *rx_ppdu_info)
{
	struct sk_buff *frame_skb = NULL;
	struct ieee80211_hdr *orig_hdr = NULL;
	struct sk_buff *first_mpdu = NULL;
	struct ieee80211_frame_min *ack_hdr = NULL;
	u16 frm_ctl;

	first_mpdu = skb_peek(mpdu_q);
	if (!first_mpdu || first_mpdu->len < sizeof(struct ieee80211_hdr) ||
	    !skb_shinfo(first_mpdu)->nr_frags) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: No valid header found for ACK generation\n");
		return NULL;
	}

	orig_hdr = (struct ieee80211_hdr *)
			skb_frag_address(&skb_shinfo(first_mpdu)->frags[0]);
	if (!ath12k_dp_ext_mon_filter_rx_ctrl(dp_pdev, tx_ppdu_info,
					      FILTER_CTRL_ACK, orig_hdr))
		return NULL;

	frame_skb = dev_alloc_skb(ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR +
				  sizeof(struct ieee80211_frame_min));
	if (!frame_skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate ack rx frame.");
		return NULL;
	}

	skb_reserve(frame_skb, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);

	ack_hdr = (struct ieee80211_frame_min *)skb_put_zero(frame_skb,
						sizeof(struct ieee80211_frame_min));

	frm_ctl = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK;
	ack_hdr->frame_control = cpu_to_le16(frm_ctl);
	memcpy(ack_hdr->ra, orig_hdr->addr2, ETH_ALEN);
	ack_hdr->duration = cpu_to_le16(0x0000);

	rx_ppdu_info->rx_status.frame_control = frm_ctl;
	rx_ppdu_info->rx_status.frame_control_info_valid = 1;
	rx_ppdu_info->rx_status.tsft = tx_ppdu_info->rx_status.tsft;
	rx_ppdu_info->rx_status.freq = tx_ppdu_info->rx_status.freq;
	rx_ppdu_info->rx_status.rssi_comb = tx_ppdu_info->ack_rssi;
	rx_ppdu_info->rx_status.sgi = 1;
	rx_ppdu_info->rx_status.bw = 0;
	rx_ppdu_info->rx_status.mcs = tx_ppdu_info->rx_status.mcs;
	if (tx_ppdu_info->rx_status.preamble_type == HAL_RX_PREAMBLE_11B) {
		rx_ppdu_info->rx_status.preamble_type = HAL_RX_PREAMBLE_11B;
		rx_ppdu_info->rx_status.ofdm_flag = 0;
		rx_ppdu_info->rx_status.cck_flag = 1;
	} else {
		rx_ppdu_info->rx_status.preamble_type = HAL_RX_PREAMBLE_11A;
		rx_ppdu_info->rx_status.ofdm_flag = 1;
		rx_ppdu_info->rx_status.cck_flag = 0;
	}
	rx_ppdu_info->ppdu_id = 0xDEAD;
	rx_ppdu_info->ack_recvd = 1;

	return frame_skb;
}

/**
 * ath12k_dp_tx_mon_generate_cts_rx_frm() - Generate CTS response frame
 * @dp_pdev: DP pdev handle
 * @mpdu_q: MPDU queue containing original frames
 * @tx_ppdu_info: TX PPDU info structure
 * @rx_ppdu_info: RX PPDU info structure
 *
 * Generate CTS response frame for TX monitor. Creates just the frame SKB
 * with proper headroom reserved for radiotap. The radiotap header will be
 * added later by ath12k_dp_mon_tx_update_mon_info() during delivery.
 *
 * Return: SKB containing CTS frame, or NULL on failure
 */
struct sk_buff *
ath12k_dp_tx_mon_generate_cts_rx_frm(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff_head *mpdu_q,
				     struct hal_tx_mon_ppdu_info *tx_ppdu_info,
				     struct hal_tx_mon_ppdu_info *rx_ppdu_info)
{
	struct sk_buff *frame_skb = NULL;
	struct sk_buff *first_mpdu = NULL;
	struct ieee80211_cts *cts_hdr = NULL;
	struct ieee80211_hdr *orig_hdr = NULL;
	u8 sifs;
	u16 duration = 0;
	u16 frm_ctl;

	first_mpdu = skb_peek(mpdu_q);
	if (!first_mpdu || first_mpdu->len < sizeof(struct ieee80211_rts)) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: No valid header found for CTS generation\n");
		return NULL;
	}

	orig_hdr = (struct ieee80211_hdr *)first_mpdu->data;
	if (!ath12k_dp_ext_mon_filter_rx_ctrl(dp_pdev, tx_ppdu_info,
					      FILTER_CTRL_CTS, orig_hdr))
		return NULL;

	duration = le16_to_cpu(orig_hdr->duration_id);
	sifs = ath12k_dp_tx_mon_get_sifs_time(tx_ppdu_info->rx_status.freq);

	if (duration > (ATH12K_CTS_TX_TIME_US + sifs))
		duration -= (ATH12K_CTS_TX_TIME_US + sifs);
	else
		duration = 0;

	frame_skb = dev_alloc_skb(ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR +
				  sizeof(struct ieee80211_cts));
	if (!frame_skb) {
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, skb_alloc_failed);
		ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, self_gen_failed);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to generate cts rx frame.");
		return NULL;
	}

	skb_reserve(frame_skb, ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR);

	cts_hdr = (struct ieee80211_cts *)skb_put_zero(frame_skb,
						       sizeof(struct ieee80211_cts));

	frm_ctl = IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS;
	cts_hdr->frame_control = cpu_to_le16(frm_ctl);
	memcpy(cts_hdr->ra, orig_hdr->addr2, ETH_ALEN);
	cts_hdr->duration = cpu_to_le16(duration);

	rx_ppdu_info->rx_status.frame_control = frm_ctl;
	rx_ppdu_info->rx_status.frame_control_info_valid = 1;
	rx_ppdu_info->rx_status.tsft = tx_ppdu_info->rx_status.tsft;
	rx_ppdu_info->rx_status.freq = tx_ppdu_info->rx_status.freq;
	rx_ppdu_info->rx_status.sgi = tx_ppdu_info->rx_status.sgi;
	rx_ppdu_info->rx_status.bw = tx_ppdu_info->rx_status.bw;
	rx_ppdu_info->rx_status.mcs = tx_ppdu_info->rx_status.mcs;
	if (tx_ppdu_info->rx_status.preamble_type == HAL_RX_PREAMBLE_11B) {
		rx_ppdu_info->rx_status.preamble_type = HAL_RX_PREAMBLE_11B;
		rx_ppdu_info->rx_status.ofdm_flag = 0;
		rx_ppdu_info->rx_status.cck_flag = 1;
	} else {
		rx_ppdu_info->rx_status.preamble_type = HAL_RX_PREAMBLE_11A;
		rx_ppdu_info->rx_status.ofdm_flag = 1;
		rx_ppdu_info->rx_status.cck_flag = 0;
	}
	rx_ppdu_info->ppdu_id = 0xDEAD;
	rx_ppdu_info->cts_recvd = 1;

	return frame_skb;
}

/**
 * ath12k_dp_tx_mon_trim_last_frag() - Trim bytes from the tail of the last frag
 * @dp_pdev: DP PDEV context for logging
 * @skb: Head SKB, whose fraglist may contain chained SKBs
 * @trim_len: Number of bytes to remove
 *
 * Shrinks the last rx frag of @skb by @trim_len bytes.  When frags have been
 * distributed across a fraglist chain (as ath12k_dp_tx_mon_generate_data_frm()
 * does when the head's frags[] array is full), the last frag may live in a
 * chained SKB rather than in the head itself.  Using the global fraglist count
 * directly as an index into the head's fixed-size frags[] array overflows
 * MAX_SKB_FRAGS; this helper walks the chain to find the right owner first.
 */
static void ath12k_dp_tx_mon_trim_last_frag(struct ath12k_pdev_dp *dp_pdev,
					    struct sk_buff *skb, u32 trim_len)
{
	struct sk_buff *last_frag_skb = skb;
	skb_frag_t *last_frag;
	struct sk_buff *iter;
	int last_frag_idx;

	skb_walk_frags(skb, iter) {
		if (skb_shinfo(iter)->nr_frags) {
			last_frag_skb = iter;
		} else {
			ath12k_warn(dp_pdev->dp,
				    "trim_tail walk: iter=%p has nr_frags=0 len=%u, skipping\n",
				    iter, iter->len);
			return;
		}
	}

	last_frag_idx = skb_shinfo(last_frag_skb)->nr_frags - 1;
	last_frag = &skb_shinfo(last_frag_skb)->frags[last_frag_idx];
	if (skb_frag_size(last_frag) <= trim_len)
		return;

	skb_coalesce_rx_frag(last_frag_skb,
			     last_frag_idx,
			     -trim_len, 0);

	/* skb_coalesce_rx_frag() updated last_frag_skb->{len,data_len}.
	 * Propagate the change to the head skb when they differ.
	 */
	if (last_frag_skb != skb) {
		skb->len -= trim_len;
		skb->data_len -= trim_len;
	}
}

/**
 * ath12k_dp_tx_mon_frame_trim_mic() - Trim MIC from encrypted frames
 * @skb: SKB containing the frame
 * @ppdu_info: PPDU info structure
 * @user_idx: User index for encryption info
 *
 * This function processes all encrypted frames, including both
 * hardware-generated and host-generated frames, trimming the
 * appropriate MIC bytes based on encryption type.
 *
 * For encrypted frames (WEP bit set), the MIC length depends on encryption type:
 * - CCMP-128: 8 bytes
 * - CCMP-256: 16 bytes
 * - GCMP-128/256: 16 bytes
 * - TKIP: 8 bytes
 * - Other types: No trimming
 */
static void
ath12k_dp_tx_mon_frame_trim_mic(struct ath12k_pdev_dp *dp_pdev,
				struct sk_buff *skb,
				struct hal_tx_mon_ppdu_info *ppdu_info,
				u8 user_idx)
{
	struct ieee80211_hdr *hdr;
	struct hal_rx_user_status *user_status;
	u32 trim_len = 0;
	int frag_count;

	if (skb->len < sizeof(struct ieee80211_hdr))
		return;

	frag_count = ath12k_dp_mon_get_num_frags_in_fraglist(skb);
	if (frag_count) {
		if (!skb_shinfo(skb)->nr_frags) {
			ath12k_dbg(NULL, ATH12K_DBG_DP_MON_TX,
				   "ext mon: mpdu fraglist reports %d frag(s) but skb nr_frags is 0\n",
				   frag_count);
			return;
		}
		hdr = (struct ieee80211_hdr *)ath12k_dp_mon_skb_get_frag_addr(skb, 0);
	} else
		hdr = (struct ieee80211_hdr *)skb->data;

	if (!ieee80211_has_protected(hdr->frame_control))
		return;

	user_status = &ppdu_info->rx_status.userstats[user_idx];

	switch (user_status->enc_type) {
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		trim_len = 8;
		break;
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		trim_len = 16;
		break;
	default:
		return;
	}

	if (trim_len && skb->len < trim_len)
		return;

	if (frag_count > 0)
		ath12k_dp_tx_mon_trim_last_frag(dp_pdev, skb, trim_len);
	else
		skb_trim(skb, skb->len - trim_len);
}


/**
 * ath12k_dp_tx_mon_get_legacy_rate() - Convert rate kbps to legacy format
 * @rate_500kbps: Rate in 500 kbps units
 *
 * Converts rate from 500 kbps units to legacy rate format (100kbps units).
 * This is used for populating status->rates[0].rate_idx.legacy.
 */
static u16 ath12k_dp_tx_mon_get_legacy_rate(u8 rate_500kbps)
{
	switch (rate_500kbps) {
	case ATH12K_RATE_1MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_1MBPS_100KBPS;
	case ATH12K_RATE_2MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_2MBPS_100KBPS;
	case ATH12K_RATE_5_5MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_5_5MBPS_100KBPS;
	case ATH12K_RATE_6MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_6MBPS_100KBPS;
	case ATH12K_RATE_9MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_9MBPS_100KBPS;
	case ATH12K_RATE_11MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_11MBPS_100KBPS;
	case ATH12K_RATE_12MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_12MBPS_100KBPS;
	case ATH12K_RATE_18MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_18MBPS_100KBPS;
	case ATH12K_RATE_24MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_24MBPS_100KBPS;
	case ATH12K_RATE_36MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_36MBPS_100KBPS;
	case ATH12K_RATE_48MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_48MBPS_100KBPS;
	case ATH12K_RATE_54MBPS_500KBPS:
		return ATH12K_LEGACY_RATE_54MBPS_100KBPS;
	default:
		return ATH12K_LEGACY_RATE_DEFAULT_100KBPS;
	}
}

/**
 * ath12k_dp_tx_mon_set_rate_a() - Convert MCS to 11a rate
 * @rx_status: RX status structure
 * Converts legacy MCS index to 11a rate in 500 kbps units for OFDM rates.
 * This is used for 5GHz and 2.4GHz OFDM transmissions.
 */
static void ath12k_dp_tx_mon_set_rate_a(struct hal_rx_mon_ppdu_info *rx_status)
{
	switch (rx_status->mcs) {
	case ATH12K_LEGACY_MCS0:
		rx_status->rate = ATH12K_11A_RATE_0MCS;
		break;
	case ATH12K_LEGACY_MCS1:
		rx_status->rate = ATH12K_11A_RATE_1MCS;
		break;
	case ATH12K_LEGACY_MCS2:
		rx_status->rate = ATH12K_11A_RATE_2MCS;
		break;
	case ATH12K_LEGACY_MCS3:
		rx_status->rate = ATH12K_11A_RATE_3MCS;
		break;
	case ATH12K_LEGACY_MCS4:
		rx_status->rate = ATH12K_11A_RATE_4MCS;
		break;
	case ATH12K_LEGACY_MCS5:
		rx_status->rate = ATH12K_11A_RATE_5MCS;
		break;
	case ATH12K_LEGACY_MCS6:
		rx_status->rate = ATH12K_11A_RATE_6MCS;
		break;
	case ATH12K_LEGACY_MCS7:
		rx_status->rate = ATH12K_11A_RATE_7MCS;
		break;
	default:
		break;
	}
}

/**
 * ath12k_dp_tx_mon_set_rate_b() - Convert MCS to 11b rate
 * @rx_status: RX status structure
 * Converts legacy MCS index to 11b rate in 500 kbps units for CCK rates.
 * This is used for 2.4GHz CCK transmissions.
 */
static void ath12k_dp_tx_mon_set_rate_b(struct hal_rx_mon_ppdu_info *rx_status)
{
	switch (rx_status->mcs) {
	case ATH12K_LEGACY_MCS0:
		rx_status->rate = ATH12K_11B_RATE_0MCS;
		break;
	case ATH12K_LEGACY_MCS1:
		rx_status->rate = ATH12K_11B_RATE_1MCS;
		break;
	case ATH12K_LEGACY_MCS2:
		rx_status->rate = ATH12K_11B_RATE_2MCS;
		break;
	case ATH12K_LEGACY_MCS3:
		rx_status->rate = ATH12K_11B_RATE_3MCS;
		break;
	case ATH12K_LEGACY_MCS4:
		rx_status->rate = ATH12K_11B_RATE_4MCS;
		break;
	case ATH12K_LEGACY_MCS5:
		rx_status->rate = ATH12K_11B_RATE_5MCS;
		break;
	case ATH12K_LEGACY_MCS6:
		rx_status->rate = ATH12K_11B_RATE_6MCS;
		break;
	default:
		break;
	}
}

/**
 * ath12k_dp_tx_mon_gi_to_nl80211() - Convert HAL GI to nl80211
 * @hal_gi: HAL guard interval value from rx_status->sgi
 *
 * Directly converts HAL guard interval values to nl80211 format.
 *
 * Return: nl80211 guard interval constant
 */
static u8 ath12k_dp_tx_mon_gi_to_nl80211(u8 hal_gi)
{
	switch (hal_gi) {
	case HE_GI_0_8:
		return NL80211_RATE_INFO_HE_GI_0_8;
	case HE_GI_1_6:
		return NL80211_RATE_INFO_HE_GI_1_6;
	case HE_GI_3_2:
		return NL80211_RATE_INFO_HE_GI_3_2;
	default:
		return NL80211_RATE_INFO_HE_GI_0_8;
	}
}

/**
 * ath12k_dp_mon_tx_fill_rate_status() - Populate tx_status->rates from HAL info
 * @dp_pdev: ath12k pdev dp context
 * @ppdu_info: PPDU info structure containing HAL data
 * @status: TX status structure to populate
 *
 * This function converts HAL rate information into ieee80211_rate_status
 * format so mac80211 can generate accurate radiotap rate fields.
 */
static void
ath12k_dp_mon_tx_fill_rate_status(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_mon_ppdu_info *ppdu_info,
				  struct ieee80211_tx_status *status)
{
	struct hal_rx_mon_ppdu_info *rx_status;
	struct ieee80211_rate_status *st_rate;
	struct rate_info *ri;

	if (!status)
		return;

	rx_status = &ppdu_info->rx_status;
	if (!rx_status)
		return;

	status->n_rates = ATH12K_RATE_STATUS_N_RATES;
	st_rate = &status->rates[status->n_rates - 1];
	memset(st_rate, 0, sizeof(*st_rate));
	st_rate->try_count = ATH12K_RATE_STATUS_TRY_COUNT;
	ri = &st_rate->rate_idx;

	ri->flags = 0;
	ri->mcs = rx_status->mcs;
	ri->bw = ath12k_mac_bw_to_mac80211_bw(rx_status->bw);
	ri->nss = rx_status->nss ? rx_status->nss : 1;

	switch (rx_status->preamble_type) {
	case HAL_RX_PREAMBLE_11A:
	case HAL_RX_PREAMBLE_11B:
		if (!rx_status->rate) {
			if (rx_status->preamble_type == HAL_RX_PREAMBLE_11B)
				ath12k_dp_tx_mon_set_rate_b(rx_status);
			else
				ath12k_dp_tx_mon_set_rate_a(rx_status);
		}
		ri->legacy = ath12k_dp_tx_mon_get_legacy_rate(rx_status->rate);
		ri->flags = 0;
		break;

	case HAL_RX_PREAMBLE_11N:
		ri->flags = RATE_INFO_FLAGS_MCS;
		ri->mcs = rx_status->mcs + 8 * (ri->nss - 1);
		if (rx_status->sgi)
			ri->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AC:
		ri->flags = RATE_INFO_FLAGS_VHT_MCS;
		ri->mcs = rx_status->mcs;
		if (rx_status->sgi)
			ri->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AX:
		ri->flags = RATE_INFO_FLAGS_HE_MCS;
		ri->mcs = rx_status->mcs;
		ri->he_gi = ath12k_dp_tx_mon_gi_to_nl80211(rx_status->sgi);
		break;

	case HAL_RX_PREAMBLE_11BA:
	case HAL_RX_PREAMBLE_11BE:
		ri->flags = RATE_INFO_FLAGS_EHT_MCS;
		ri->mcs = rx_status->mcs;
		ri->eht_gi = ath12k_dp_tx_mon_gi_to_nl80211(rx_status->sgi);
		break;

	case HAL_RX_PREAMBLE_11BN:
		ri->flags = RATE_INFO_FLAGS_UHR_MCS;
		ri->mcs = rx_status->mcs;
		ri->eht_gi = ath12k_dp_tx_mon_gi_to_nl80211(rx_status->sgi);
		break;

	default:
		ri->legacy = (rx_status->freq >= ATH12K_FREQ_2GHZ_MIN &&
			      rx_status->freq <= ATH12K_FREQ_2GHZ_MAX) ?
			      ATH12K_LEGACY_RATE_1MBPS_100KBPS :
			      ATH12K_LEGACY_RATE_6MBPS_100KBPS;
		ri->flags = 0;
		break;
	}
}

int ath12k_dp_mon_tx_ext_mon_deliver(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff *mpdu)
{
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ieee80211_ext_mon_tx_event_extn event;
	bool need_rtap;
	int ret;

	if (!ieee80211_ext_mon_tx_notifier_has_listeners_extn())
		return NOTIFY_DONE;

	if (unlikely(!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon))
		return NOTIFY_DONE;

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	tx_ext_mon = &tx_mon->tx_ext_mon;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_config = tx_ext_mon->tx_ext_mon_config;
	if (unlikely(!tx_config) || !tx_config->enable) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		return NOTIFY_DONE;
	}
	need_rtap = tx_config->metadata & ATH12K_EXT_MON_METADATA_RTAP_HDR;
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);

	/* RTAP_HDR requested: let the frame flow to the POST_RTAP path via
	 * ieee80211_tx_monitor() -> ieee80211_ext_mon_tx_notify() instead.
	 */
	if (need_rtap)
		return NOTIFY_DONE;

	event.hw = ath12k_dp_pdev_to_hw(dp_pdev);
	event.mpdu = mpdu;
	event.sdata = NULL;
	ret = ieee80211_ext_mon_tx_notifier_call_extn_nc(IEEE80211_EXT_MON_PRE_RTAP,
							 &event);
	return ret;
#else
	return NOTIFY_DONE;
#endif /* CPTCFG_QCN_EXTN */
}
/**
 * ath12k_dp_mon_tx_deliver_frame() - Helper to deliver single frame to monitor stack
 * @dp_pdev: ath12k pdev dp context
 * @hw: ieee80211_hw handle
 * @skb: Frame to deliver
 * @ppdu_info: PPDU info structure
 * @status_info: HAL status info structure
 * @contains_host_frames: Whether PPDU contains host-generated frames
 * @is_response_frame: Whether this is a response frame (ACK/CTS)
 * @user_idx: User index
 *
 * Common helper function to deliver a single frame to the monitor stack.
 * Handles radiotap header population and frame delivery.
 *
 * Return: 0 on success, negative error code on failure
 */
static void
ath12k_dp_mon_tx_deliver_frame(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_hw *hw,
			       struct sk_buff *skb,
			       struct hal_tx_mon_ppdu_info *ppdu_info,
			       struct hal_tx_mon_status_info *status_info,
			       bool contains_host_frames,
			       bool is_response_frame,
			       u8 user_idx)
{
	struct ieee80211_rate_status rate_status = {};
	struct ieee80211_tx_status status = {
		.skb = skb,
		.info = IEEE80211_SKB_CB(skb),
		.rates = &rate_status,
	};
	int ext_mon_ret;

	if (!is_response_frame)
		ath12k_dp_tx_mon_frame_trim_mic(dp_pdev, skb,
						ppdu_info, user_idx);

	ath12k_dp_mon_tx_update_mon_info(dp_pdev, &status.mon_info,
					 ppdu_info, status_info,
					 contains_host_frames,
					 is_response_frame, user_idx);

	ath12k_dp_mon_tx_update_spl_pkt_cap_stats(dp_pdev, status_info);

	/* If the extended-monitor consumer accepted the frame
	 * (NOTIFY_OK/NOTIFY_STOP), free it instead of passing it up
	 * the monitor netdev, to avoid delivering it twice. see --metadata
	 */
	ext_mon_ret = ath12k_dp_mon_tx_ext_mon_deliver(dp_pdev, skb);
	if (ext_mon_ret == NOTIFY_OK || ext_mon_ret == NOTIFY_STOP) {
		dev_kfree_skb(skb);
		ATH12K_TX_MON_STAT_INC(dp_pdev, custom_call_back_delivered);
		return;
	}

	if (!ieee80211_is_data_qos(cpu_to_le16(ppdu_info->rx_status.frame_control))) {
		status.n_rates = ATH12K_RATE_STATUS_N_RATES;
		status.rates = &rate_status;
		ath12k_dp_mon_tx_fill_rate_status(dp_pdev, ppdu_info, &status);
	} else {
		ath12k_dp_tx_mon_update_rtap_tlv_at_end(skb, &status.mon_info, ppdu_info);
	}

	ieee80211_tx_monitor_offload_ni(hw, &status);
}

static
int ath12k_dp_ext_mon_tx_filter_frame(struct ath12k_pdev_dp *dp_pdev,
					 struct sk_buff *mpdu, u32 mpdu_count)
{
	struct ath12k_pdev_tx_mon *tx_mon;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	int ret;

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (unlikely(!tx_ext_mon)) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
				"ext_mon in tx direction is null\n");
		return 0;
	}

	if (!tx_ext_mon->enable)
		return 0;

	// TODO: Optimise this to trim this before processing
	if (tx_ext_mon->level == ATH12K_EXT_MON_FILTER_LEVEL_PPDU &&
		mpdu_count > 1)
		return -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	if (mon_ops && mon_ops->ext_mon_filter) {
		ret = mon_ops->ext_mon_filter(dp_pdev, mpdu);
		if (ret)
			return ret;
	}

	return 0;
}

static int
ath12k_dp_ext_mon_tx_frame_is_filtered(struct ath12k_pdev_dp *dp_pdev,
					 struct sk_buff *mpdu, u32 mpdu_count)
{
	struct ath12k_pdev_tx_mon *tx_mon;
	int ret;

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	ret = ath12k_dp_ext_mon_tx_filter_frame(dp_pdev, mpdu,
						  mpdu_count);
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	return ret;
}

/**
 * ath12k_dp_mon_tx_deliver_single_ppdu() - Process and deliver MPDUs for single user
 * @dp_pdev: ath12k pdev dp context
 * @ppdu_info: PPDU info structure
 * @status_info: HAL status info structure
 * @user_idx: User index to process
 * @ppdu_context: PPDU context containing host frame flag
 *
 * - For user 0: generate response frame if ack_recvd/cts_recvd
 * - Process all TX frames in mpdu_q
 * - After TX frames, deliver response frame separately
 *
 * This ensures TX frames and response frames are handled separately,
 * not mixed in the same queue.
 */
static void
ath12k_dp_mon_tx_deliver_single_ppdu(struct ath12k_pdev_dp *dp_pdev,
				     struct hal_tx_mon_ppdu_info *ppdu_info,
				     struct hal_tx_mon_status_info *status_info,
				     struct sk_buff_head *mpdu_q,
				     u8 user_idx,
				     struct dp_mon_tx_ppdu_info *ppdu_context)
{
	struct ieee80211_hw *hw;
	struct sk_buff *mpdu;
	struct sk_buff *resp_skb = NULL;
	struct hal_tx_mon_ppdu_info *rx_ppdu_info = NULL;
	int delivered = 0;
	bool contains_host_frames;
	u32 mpdu_count = 0;

	if (!ppdu_info || !mpdu_q || !ppdu_context)
		return;

	if (!dp_pdev || !dp_pdev->dp) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Invalid dp_pdev or dp_pdev->dp\n");
		return;
	}

	if (!dp_pdev->dp->ab || !dp_pdev->ar)
		return;

	hw = ath12k_ar_to_hw(dp_pdev->ar);
	if (!hw)
		return;

	if (user_idx == 0) {
		if (!ppdu_context->contains_host_frames &&
		    ppdu_info->ack_recvd && !status_info->explicit_ack_type) {
			rx_ppdu_info = kzalloc(sizeof(*rx_ppdu_info), GFP_KERNEL);
			if (!rx_ppdu_info)
				return;

			resp_skb = ath12k_dp_tx_mon_generate_ack_rx_frm(dp_pdev,
									mpdu_q,
									ppdu_info,
									rx_ppdu_info);
			if (resp_skb)
				ppdu_context->contains_host_frames = true;
		} else if (ppdu_info->cts_recvd) {
			rx_ppdu_info = kzalloc(sizeof(*rx_ppdu_info), GFP_KERNEL);
			if (!rx_ppdu_info)
				return;
			resp_skb = ath12k_dp_tx_mon_generate_cts_rx_frm(dp_pdev,
									mpdu_q,
									ppdu_info,
									rx_ppdu_info);
			if (resp_skb)
				ppdu_context->contains_host_frames = true;
		}

		if (rx_ppdu_info && !resp_skb)
			kfree(rx_ppdu_info);
	}

	contains_host_frames = ppdu_context->contains_host_frames;

	while ((mpdu = skb_dequeue(mpdu_q))) {
		if (ath12k_dp_ext_mon_tx_frame_is_filtered(dp_pdev, mpdu,
							   ++mpdu_count)) {
			ATH12K_TX_MON_STAT_INC(dp_pdev, frames_drop_in_sw);
			dev_kfree_skb_any(mpdu);
			continue;
		}

		ath12k_dp_mon_tx_deliver_frame(dp_pdev, hw, mpdu,
					       ppdu_info, status_info,
					       contains_host_frames,
					       false, user_idx);
		delivered++;
		ATH12K_TX_MON_STAT_INC(dp_pdev, total_frames_delivered);
	}

	if ((user_idx == 0) && resp_skb) {
		ath12k_dp_mon_tx_deliver_frame(dp_pdev, hw, resp_skb,
					       rx_ppdu_info, status_info,
					       contains_host_frames,
					       true, user_idx);
		delivered++;
		ATH12K_TX_MON_STAT_INC(dp_pdev, total_frames_delivered);

		kfree(rx_ppdu_info);
	}

	if (delivered > 0) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: Delivered %d frames for user %u\n",
			   delivered, user_idx);
	}
}

/**
 * ath12k_dp_tx_mon_deliver_ppdu() - Main TX monitor delivery function
 * @dp_pdev: ath12k pdev dp context
 * @mon_data: Monitor data containing both data and protection PPDU info
 *
 * Always process both data and protection PPDUs for all users
 */
int ath12k_dp_tx_mon_deliver_ppdu(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *mon_data)
{
	int i;
	struct hal_tx_mon_ppdu_info *data_ppdu_info =
		&mon_data->data_ppdu_info.tx_info;
	struct hal_tx_mon_ppdu_info *prot_ppdu_info =
		&mon_data->prot_ppdu_info.tx_info;
	struct sk_buff_head *mpdu_q;
	u32 num_users;

	if (!mon_data)
		return -EINVAL;

	num_users = data_ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS) {
		ATH12K_TX_MON_STAT_INC(dp_pdev, mu_user_frame);
		num_users = HAL_MAX_UL_MU_USERS;
	}

	for (i = 0; i < num_users; i++) {
		mpdu_q = &data_ppdu_info->rx_status.mpdu_q[i];
		ath12k_dp_mon_tx_deliver_single_ppdu(dp_pdev,
						     data_ppdu_info,
						     &mon_data->data_status_info,
						     mpdu_q, i,
						     &mon_data->data_ppdu_info);

		mpdu_q = &prot_ppdu_info->rx_status.mpdu_q[i];
		ath12k_dp_mon_tx_deliver_single_ppdu(dp_pdev,
						     prot_ppdu_info,
						     &mon_data->prot_status_info,
						     mpdu_q, i,
						     &mon_data->prot_ppdu_info);
	}

	if (prot_ppdu_info->is_used)
		ATH12K_TX_MON_STAT_INC(dp_pdev, prot_ppdu_delivered);

	if (data_ppdu_info->is_used)
		ATH12K_TX_MON_STAT_INC(dp_pdev, data_ppdu_delivered);

	return 0;
}

/**
 * ath12k_dp_tx_mon_process_ppdu() - Work queue handler for TX monitor
 * @work: Work structure containing the monitor pdev context
 *
 * This function processes PPDU descriptors in work queue context, performing
 * heavy TLV parsing, software filtering, frame generation, and stack delivery.
 * It follows the efficient batch processing pattern while adding
 * comprehensive error handling and bridge functions for complete functionality.
 *
 * Processing Flow:
 * 1. Move descriptors from used to processing list (batch processing)
 * 2. Process each PPDU with validation and error handling
 * 3. Perform TLV parsing and software filtering
 * 4. Generate frames and deliver to stack
 * 5. Return descriptors to free pool for reuse
 */
void ath12k_dp_tx_mon_process_ppdu(struct work_struct *work)
{
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon =
		container_of(work, struct ath12k_pdev_tx_mon, txmon_work);
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_status_desc *status_desc;
	struct ath12k_mon_data *mon_data;
	int desc_idx, ppdu_prep_failed = 0;
	int ppdu_processed_cnt = 0, total_status_desc_cnt = 0;
	struct ath12k_base *ab;

	if (unlikely(!dp_pdev_tx_mon)) {
		ath12k_err(NULL, "TX Mon: NULL dp_pdev_tx_mon in work queue\n");
		return;
	}

	dp_mon_pdev = dp_pdev_tx_mon->mon_pdev;
	if (unlikely(!dp_mon_pdev)) {
		ath12k_err(NULL, "TX Mon: NULL dp_mon_pdev in work queue\n");
		return;
	}

	dp_pdev = dp_mon_pdev->dp_pdev;
	if (unlikely(!dp_pdev)) {
		ath12k_err(NULL, "TX Mon: NULL dp_pdev in work queue\n");
		return;
	}

	mon_data = &dp_mon_pdev->mon_data;

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	if (!dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized) {
		spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
		return;
	}
	list_splice_tail_init(&dp_pdev_tx_mon->tx_mon_ppdu_desc_used_list,
			      &dp_pdev_tx_mon->tx_mon_ppdu_desc_proc_list);
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	ab = dp_pdev->dp->ab;

	list_for_each_entry(ppdu_desc,
			    &dp_pdev_tx_mon->tx_mon_ppdu_desc_proc_list,
			    list) {
		if (unlikely(ppdu_desc->status_desc_cnt == 0)) {
			ATH12K_TX_MON_STAT_INC(dp_pdev, zero_status_desc);
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: Invalid PPDU desc or zero status count\n");
			continue;
		}

		if (unlikely(ppdu_desc->status_desc_cnt >
		    DP_TX_MON_NUM_STATUS_BUF(dp_pdev->dp->ab))) {
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: PPDU desc overflow count=%u max=%u\n",
				    ppdu_desc->status_desc_cnt,
				    DP_TX_MON_NUM_STATUS_BUF(dp_pdev->dp->ab));
			ppdu_desc->status_desc_cnt =
					DP_TX_MON_NUM_STATUS_BUF(dp_pdev->dp->ab);
			ATH12K_TX_MON_STAT_INC(dp_pdev, ppdu_desc_overflow);
		}

		ppdu_prep_failed = ath12k_dp_mon_tx_prep_ppdu_info(dp_mon_pdev,
							ppdu_desc);

		for (desc_idx = 0; desc_idx < ppdu_desc->status_desc_cnt; desc_idx++) {
			status_desc = &ppdu_desc->status_desc[desc_idx];

			if (unlikely(!status_desc->mon_buf)) {
				ATH12K_TX_MON_STAT_INC(dp_pdev, status_buf_null);
				ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: Null buffer address in ppdu desc idx=%d\n",
					   desc_idx);
				continue;
			}

			ath12k_core_dma_unmap_page(dp_pdev->dp->dev,
						   status_desc->paddr,
						   ATH12K_DP_MON_TX_BUF_SIZE,
						   DMA_FROM_DEVICE);

			if (ppdu_prep_failed) {
				page_frag_free(status_desc->mon_buf);
				status_desc->mon_buf = NULL;
				status_desc->paddr = 0;
				status_desc->buf_len = 0;
				status_desc->end_of_ppdu = false;
				continue;
			}

			mon_data->data_ppdu_info.contains_host_frames = false;
			mon_data->prot_ppdu_info.contains_host_frames = false;

			ath12k_dp_mon_tx_process_tlv(dp_pdev,
						     status_desc, mon_data);
			ath12k_dp_mon_tx_populate_ppdu_info(dp_pdev,
							    status_desc,
							    mon_data);

			if (status_desc->end_of_ppdu)
				ath12k_dp_tx_mon_deliver_ppdu(dp_pdev, mon_data);

			page_frag_free(status_desc->mon_buf);
			status_desc->mon_buf = NULL;
			status_desc->paddr = 0;
			status_desc->buf_len = 0;
			status_desc->end_of_ppdu = false;
			total_status_desc_cnt++;
			ATH12K_TX_MON_STAT_INC(dp_pdev, status_desc_processed);
		}
		if (ppdu_prep_failed) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, ppdu_prep_failed);
			goto ppdu_prep_failed;
		}

		ppdu_processed_cnt++;
		ATH12K_TX_MON_STAT_INC(dp_pdev, ppdu_processed);

ppdu_prep_failed:
		/* Reset PPDU descriptor for reuse */
		ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
		/* Deep cleanup of PPDU info */
		ath12k_dp_mon_tx_deep_free_ppdu_info(dp_pdev, mon_data);
	}

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	list_splice_tail_init(&dp_pdev_tx_mon->tx_mon_ppdu_desc_proc_list,
			      &dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list);
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "TX Mon: Work queue processed %d PPDUs, %d status descriptors\n",
		   ppdu_processed_cnt, total_status_desc_cnt);
}

/**
 * ath12k_dp_tx_mon_get_ppdu_desc() - Get PPDU descriptor from free list
 * @dp_mon_pdev: Monitor PDEV context
 *
 * Allocates a PPDU descriptor from the free list for use in TX monitor
 * processing. The descriptor is initialized and ready for use.
 *
 * Return: Pointer to PPDU descriptor on success, NULL if free list empty
 */
static struct ath12k_dp_mon_ppdu_desc *
ath12k_dp_tx_mon_get_ppdu_desc(struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;

	if (unlikely(!dp_pdev_tx_mon))
		return NULL;

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	ppdu_desc = list_first_entry_or_null
			(&dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list,
			 struct ath12k_dp_mon_ppdu_desc, list);

	if (likely(ppdu_desc))
		list_del(&ppdu_desc->list);

	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	return ppdu_desc;
}

/**
 * ath12k_dp_tx_mon_prep_wq() - Prepare descriptors for work queue processing
 * @mon_desc_used_list: List of monitor descriptors containing TLV data
 * @dp_mon_pdev: Monitor PDEV context
 *
 * This function prepares monitor descriptors for work queue processing by:
 * 1. Allocating a PPDU descriptor from free list
 * 2. Associating monitor descriptors with the PPDU descriptor
 * 3. Queuing the PPDU descriptor for work queue processing
 *
 * This is called from NAPI context when end of PPDU is detected during
 * ring processing. The actual heavy processing is deferred to work queue.
 *
 * Return: 0 on success, -ENOENT if no PPDU descriptor available
 */
static int ath12k_dp_tx_mon_prep_wq(struct list_head *mon_desc_used_list,
				    struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_desc *desc;
	struct ath12k_base *ab = dp_mon_pdev->dp_pdev->dp->ab;
	int desc_cnt;
	struct ath12k_pdev_dp *dp_pdev = dp_mon_pdev->dp_pdev;

	if (unlikely(!dp_pdev_tx_mon))
		return -EINVAL;

	ppdu_desc = ath12k_dp_tx_mon_get_ppdu_desc(dp_mon_pdev);
	if (unlikely(!ppdu_desc))
		return -ENOENT;

	/* Copy monitor descriptors to PPDU descriptor */
	list_for_each_entry(desc, mon_desc_used_list, list) {
		desc_cnt = ppdu_desc->status_desc_cnt;
		if (unlikely(desc_cnt >= DP_TX_MON_NUM_STATUS_BUF(ab))) {
			/* On overflow, reset and add to used list, return error */
			ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
			spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
			list_add_tail(&ppdu_desc->list,
				      &dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list);
			spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
			ATH12K_TX_MON_STAT_INC(dp_pdev, ppdu_desc_overflow);
			return -EOVERFLOW;
		}

		/* Copy individual fields from descriptor to status_desc */
		ppdu_desc->status_desc[desc_cnt].mon_buf = desc->mon_buf;
		ppdu_desc->status_desc[desc_cnt].paddr = desc->paddr;
		ppdu_desc->status_desc[desc_cnt].buf_len = desc->buf_len;
		ppdu_desc->status_desc[desc_cnt].end_of_ppdu = desc->end_of_ppdu;
		ppdu_desc->status_desc[desc_cnt].pkt_buf_cnt = desc->pkt_buf_cnt;
		ppdu_desc->status_desc_cnt++;
	}

	/* Add PPDU descriptor to used list for work queue processing */
	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	list_add_tail(&ppdu_desc->list, &dp_pdev_tx_mon->tx_mon_ppdu_desc_used_list);
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	return 0;
}

/**
 * ath12k_dp_tx_mon_process_pktlog() - Process TX monitor data for hybrid pktlog
 * @dp_pdev: DP pdev handle
 * @status_frag: Fragment containing TX monitor TLV data
 * @end_offset: End offset of valid data in the fragment
 *
 * This function processes TX monitor ring data when hybrid pktlog mode is enabled.
 * In hybrid mode, upstream TLVs (hardware-generated FES status, response status, etc.)
 * are captured from the TX monitor destination ring, while UMAC TLVs (firmware-generated
 * metadata) are received via the HTT path.
 *
 * The function:
 * 1. Checks if hybrid mode is enabled
 * 2. Validates input parameters
 * 3. Writes TLV data directly to pktlog buffer
 *
 * Return: None
 */
static void
ath12k_dp_tx_mon_process_pktlog(struct ath12k_pdev_dp *dp_pdev,
				u8 *status_frag, u32 end_offset)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k *ar = dp_pdev->ar;

	if (unlikely(!ar->debug.is_pkt_logging || !status_frag))
		return;

	if (unlikely(!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon ||
	    !dp_mon_pdev->dp_pdev_tx_mon->tx_pktlog_hybrid))
		return;

	ath12k_dp_txrx_stats_buf_pktlog_process(ar, status_frag,
						ATH12K_PKTLOG_TYPE_TX_STAT,
						end_offset);
}

/**
 * ath12k_dp_mon_tx_process_ring() - Process TX monitor destination ring
 * @dp: DP context
 * @mac_id: MAC ID for the radio
 * @napi: NAPI context for budget management
 * @budget: Processing budget (number of descriptors to process)
 *
 * This is the main entry point for TX monitor ring processing, called from
 * NAPI context. It implements the "work queue heavy, lighter tasklet" architecture:
 *
 * NAPI Context (Minimal Work):
 * - Ring descriptor processing
 * - TLV fragment collection
 * - Work queue scheduling at end of PPDU
 * - Budget management
 *
 * Work Queue Context (Heavy Work - scheduled by this function):
 * - Heavy TLV parsing
 * - Frame generation
 * - Stack delivery
 *
 * The function processes ring entries within the given budget, collecting
 * TLV fragments until end of PPDU is detected, then schedules work queue
 * for heavy processing.
 *
 * Return: Number of ring descriptors processed
 */
int ath12k_dp_mon_tx_process_ring(struct ath12k_pdev_dp *dp_pdev,
				  int mac_id,
				  struct napi_struct *napi,
				  int *budget)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_hal *hal = &ab->hal;
	struct ath12k_mon_ring_desc_info desc_info = {};
	struct hal_srng *tx_mon_dst_ring;
	struct list_head *mon_desc_head;
	void *ring_entry;
	u32 ring_id, end_offset, end_reason;
	int num_buffs_reaped = 0;
	u8 *status_frag = NULL;
	int ret;

	if (unlikely(!dp_mon_pdev || !budget || !dp_mon)) {
		ath12k_warn(ab, "TX Mon: Invalid parameters\n");
		return 0;
	}

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	if (unlikely(!dp_pdev_tx_mon)) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: pdev TX mon context not initialized\n");
		return 0;
	}

	mon_desc_head = &dp_pdev_tx_mon->tx_mon_desc_work_list;
	ring_id = dp_pdev_tx_mon->tx_mon_dst_ring.ring_id;
	tx_mon_dst_ring = &ab->hal.srng_list[ring_id];

	if (unlikely(!tx_mon_dst_ring || !tx_mon_dst_ring->ring_base_vaddr)) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: Invalid srng for ring_id=%u\n", ring_id);
		return 0;
	}

	spin_lock_bh(&tx_mon_dst_ring->lock);
	ath12k_hal_srng_access_begin(ab, tx_mon_dst_ring);

	/* Process ring entries within budget - minimal work in NAPI context */
	while (*budget &&
	       (ring_entry = ath12k_hal_srng_dst_peek(ab, tx_mon_dst_ring))) {
		struct ath12k_dp_mon_desc *mon_desc;

		if (!hal->hal_mon_ops ||
		    !hal->hal_mon_ops->extract_tx_mon_ring_desc) {
			ath12k_err(ab,
				   "TX monitor ring desc extraction not supported\n");
			goto move_next;
		}

		ret = ath12k_hal_mon_extract_tx_mon_ring_desc(hal,
							      ring_entry,
							      &desc_info);
		if (ret) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, ring_extract_failed);
			ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
				   "Failed to extract TX mon ring desc: %d\n",
				   ret);
			goto move_next;
		}

		if (desc_info.empty_desc) {
			ATH12K_TX_MON_STAT_INC(dp_pdev, empty_descriptor);
			if (desc_info.end_reason == HAL_MON_PPDU_TRUNCATED) {
				ATH12K_TX_MON_STAT_INC(dp_pdev, truncated_buf);
				ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: truncated ppdu end, mac_id=%d\n",
					   mac_id);
				ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
								 mon_desc_head);
				*budget -= 1;
			} else {
				ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: empty desc mac_id=%d\n",
					   mac_id);
			}
			goto move_next;
		}
		end_reason = desc_info.end_reason;
		end_offset = desc_info.end_offset;

		mon_desc = desc_info.mon_desc;
		if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
			ath12k_err(ab, "TX Mon: invalid in_use=[%d] flag, mac_id %d\n",
				   mon_desc->in_use, mac_id);
			goto move_next;
		}

		mon_desc->in_use = DP_MON_DESC_STATUS_REAP;

		/* The hardware reports buffer length as (actual_length - 1),
		 * likely due to internal indexing or alignment constraints.
		 * To obtain the true buffer length for processing, increment
		 * the reported end_offset by 1 before using it.
		 */
		end_offset += 1;
		if (unlikely(end_offset > ATH12K_DP_MON_TX_BUF_SIZE)) {
			ath12k_warn(ab, "TX Mon: invalid offset %u received in mac_id %d\n",
				    end_offset, mac_id);
			end_offset = ATH12K_DP_MON_TX_BUF_SIZE - 1;
		}
		mon_desc->buf_len = end_offset;
		mon_desc->pkt_buf_cnt = desc_info.pkt_buf_cnt;
		list_add_tail(&mon_desc->list, mon_desc_head);

		status_frag = (u8 *)mon_desc->mon_buf;
		if (unlikely(!status_frag)) {
			ATH12K_TX_MON_STAT_INC(dp_pdev, null_buf);
			ath12k_err(ab, "TX Mon: NULL buffer received in mac_id %d\n",
				   mac_id);
			goto move_next;
		}

		if (end_reason == HAL_MON_FLUSH_DETECTED ||
		    end_reason == HAL_MON_PPDU_TRUNCATED) {
			ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
							 mon_desc_head);
			if (end_reason == HAL_MON_PPDU_TRUNCATED)
				ATH12K_TX_MON_STAT_INC(dp_pdev, truncated_buf);
			else
				ATH12K_TX_MON_STAT_INC(dp_pdev, flushed_buf);
			ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
				   "TX Mon: Flush Detected - Buffers Dropped\n");
			*budget -= 1;
			goto move_next;
		}

		ath12k_dp_tx_mon_process_pktlog(dp_pdev,
						status_frag, end_offset);

		if (end_reason == HAL_MON_END_OF_PPDU) {
			*budget -= 1;
			mon_desc->end_of_ppdu = true;
			ret = ath12k_dp_tx_mon_prep_wq(mon_desc_head,
						       dp_mon_pdev);
			if (ret) {
				ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, prep_wq_failed);
				ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
					   "TX Mon: Add mon desc to ppdu fail - ret %d",
					   ret);
				ath12k_dp_tx_mon_flush_desc_list(dp_pdev,
								 mon_desc_head);
				goto move_next;
			}

			queue_work(dp_pdev_tx_mon->txmon_wq, &dp_pdev_tx_mon->txmon_work);
			ath12k_dp_mon_tx_desc_free(dp_pdev, mon_desc_head, dp_mon);
		}

move_next:
		ring_entry = ath12k_hal_srng_dst_get_next_entry(ab,
								tx_mon_dst_ring);
		num_buffs_reaped++;
		ATH12K_TX_MON_STAT_INC(dp_pdev, num_bufs_reaped);
	}

	ath12k_hal_srng_access_end(ab, tx_mon_dst_ring);
	spin_unlock_bh(&tx_mon_dst_ring->lock);

	return num_buffs_reaped;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_process_ring);

int ath12k_dp_mon_tx_srng_init(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;

	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_err(dp->ab, "TX Monitor: No monitor ops available");
		return -EINVAL;
	}

	if (mon_ops->mon_tx_srng_init_setup) {
		ret = mon_ops->mon_tx_srng_init_setup(dp);
		if (ret)
			ath12k_err(dp->ab, "TX Monitor: SRNG setup failed, ret=%d", ret);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_init);

int ath12k_dp_mon_tx_srng_alloc(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;

	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_err(dp->ab, "TX Monitor: No monitor ops available");
		return -EINVAL;
	}

	if (mon_ops->mon_tx_srng_alloc_setup) {
		ret = mon_ops->mon_tx_srng_alloc_setup(dp);
		if (ret)
			ath12k_err(dp->ab, "TX Monitor: SRNG setup failed, ret=%d", ret);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_alloc);

void ath12k_dp_mon_tx_htt_src_ring_cleanup(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops  = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_tx_htt_srng_cleanup)
		mon_ops->mon_tx_htt_srng_cleanup(dp);
}

int ath12k_dp_ext_mon_tx_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp->ab, "monitor pdev is null\n");
		return -EINVAL;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (unlikely(!tx_mon)) {
		ath12k_warn(dp_pdev->dp->ab, "tx monitor is null\n");
		return -EINVAL;
	}

	spin_lock_init(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	tx_config = kzalloc(sizeof(*tx_config), GFP_KERNEL);
	if (!tx_config)
		return -ENOMEM;

	INIT_LIST_HEAD(&tx_config->peer_list);

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_mon->tx_ext_mon.tx_ext_mon_config = tx_config;
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_tx_alloc);

static void
ath12k_dp_ext_mon_delete_tx_peer_list(struct list_head *peer_list)
{
	struct ath12k_dp_ext_mon_peer *peer, *tmp;

	list_for_each_entry_safe(peer, tmp, peer_list, list) {
		list_del(&peer->list);
		kfree(peer);
	}
}

static void
ath12k_dp_ext_mon_prepare_remove_peer(struct ath12k_pdev_dp *dp_pdev,
				      struct ath12k_dp_ext_mon_tx_peer_params
				      *ext_mon_peer_params)
{
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	const struct ath12k_ext_mon_peer_info *peer_info;
	bool found;
	int i;
	struct list_head *peer_list = ext_mon_peer_params->peer_list;
	const struct ath12k_ext_mon_peer_config *peer_config =
		ext_mon_peer_params->peer_config;

	for (i = 0; i < peer_config->count; i++) {
		peer_info = &peer_config->peer_info[i];
		found = false;

		/** An all-zero MAC is the wildcard sentinel for "remove all
		 * TX peers"; splice the whole list to peers_to_wmi instead
		 * of matching against individual entries.
		 */
		if (is_zero_ether_addr(peer_info->mac_addr)) {
			*ext_mon_peer_params->ext_mon_peer_count = 0;
			list_splice_tail_init(peer_list,
					      ext_mon_peer_params->peers_to_wmi);
			return;
		}
		if (!peer_info->ra_addr) {
			ath12k_warn(dp_pdev->dp,
				    "skipping peer %pM: TA not allowed in TX direction\n",
				    peer_config->peer_info[i].mac_addr);
			continue;
		}

		list_for_each_entry_safe(peer, tmp, peer_list, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					     peer_info->mac_addr)) {
				list_del(&peer->list);
				(*ext_mon_peer_params->ext_mon_peer_count)--;
				list_add_tail(&peer->list,
					      ext_mon_peer_params->peers_to_wmi);
				found = true;
				break;
			}
		}

		if (!found)
			ath12k_warn(dp_pdev->dp, "peer %pM (%s) not found for remove\n",
				    peer_info->mac_addr,
				    peer_info->ra_addr ? "RA" : "TA");
	}
}

static int
ath12k_dp_ext_mon_remove_tx_peers(struct ath12k_pdev_dp *dp_pdev,
				  bool retain_peer_list,
				  const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	LIST_HEAD(peers_to_wmi);
	int ret = 0, vdev_id;
	u8 peer_count;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp_ext_mon_tx_peer_params tx_mon_peer_params = {
		.peers_to_wmi = &peers_to_wmi,
		.peer_config = peer_config,
	};

	vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	if (vdev_id == -1)
		ath12k_warn(dp_pdev->dp, "no active monitor vdev found\n");

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (unlikely(!tx_ext_mon)) {
		ath12k_warn(dp_pdev->dp, "ext_mon for tx direction is null\n");
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		return -EINVAL;
	}

	if (tx_ext_mon->peer_count == 0) {
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		return 0;
	}

	tx_mon_peer_params.peer_list = &tx_ext_mon->peer_list;
	tx_mon_peer_params.ext_mon_peer_count = &tx_ext_mon->peer_count;
	peer_count = tx_ext_mon->peer_count;
	ath12k_dp_ext_mon_prepare_remove_peer(dp_pdev, &tx_mon_peer_params);

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);

	tx_mon_peer_params.toggle_hw_state =
		(tx_ext_mon->peer_count == 0);
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	if (vdev_id == -1)
		goto delete_peer_list;
	tx_mon_peer_params.vdev_id = vdev_id;

	if (mon_ops && mon_ops->ext_mon_remove_wmi_tx_peers)
		ret = mon_ops->ext_mon_remove_wmi_tx_peers(dp_pdev,
							   &tx_mon_peer_params);
	if (retain_peer_list) {
		/**
		 * splice the wmi peers back into tx_config->peer_list so
		 * host-side bookkeeping survives instead of being dropped
		 */
		spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		list_splice_tail_init(&peers_to_wmi, &tx_ext_mon->peer_list);
		tx_ext_mon->peer_count = peer_count;
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		return ret;
	}
delete_peer_list:
	ath12k_dp_ext_mon_delete_tx_peer_list(&peers_to_wmi);

	return ret;
}

static int
ath12k_dp_ext_mon_tx_remove_all_peers(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	int ret = 0;
	struct ath12k_ext_mon_peer_config *peer_config;

	peer_config = kzalloc(sizeof(*peer_config), GFP_KERNEL);
	if (!peer_config)
		return -ENOMEM;

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	peer_config->count = 1;
	eth_zero_addr(peer_config->peer_info[0].mac_addr);

	ret = ath12k_dp_ext_mon_remove_tx_peers(dp_pdev, !tx_mon->tx_mon_teardown,
						peer_config);

	kfree(peer_config);

	return ret;
}

void ath12k_dp_ext_mon_tx_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ath12k_pdev_tx_mon *tx_mon;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp->ab, "monitor pdev is null\n");
		return;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (unlikely(!tx_mon)) {
		ath12k_warn(dp_pdev->dp->ab, "tx monitor is null\n");
		return;
	}
	tx_mon->tx_mon_teardown = true;
	ath12k_dp_ext_mon_tx_remove_all_peers(dp_pdev);

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_config = tx_mon->tx_ext_mon.tx_ext_mon_config;
	tx_mon->tx_ext_mon.tx_ext_mon_config = NULL;
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	kfree(tx_config);
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_tx_free);

void ath12k_dp_mon_tx_pdev_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (unlikely(!dp_pdev)) {
		ath12k_err(NULL, "Tx Mon: Invalid DP Pdev\n");
		return;
	}

	dp = dp_pdev->dp;
	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops)
		return;

	if (mon_ops->mon_tx_wq_stop)
		mon_ops->mon_tx_wq_stop(dp_pdev);
	if (mon_ops->mon_tx_dst_ring_cleanup)
		mon_ops->mon_tx_dst_ring_cleanup(dp_pdev);
	if (mon_ops->ext_mon_tx_free)
		mon_ops->ext_mon_tx_free(dp_pdev);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_pdev_free);

int ath12k_dp_mon_tx_htt_src_ring_setup(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = -EINVAL;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_tx_htt_srng_setup) {
		ret = mon_ops->mon_tx_htt_srng_setup(dp);
		if (ret)
			ath12k_err(dp->ab, "TX Monitor: srng htt setup failed(%d)", ret);
	}

	return ret;
}

static void
ath12k_dp_ext_mon_tx_set_rtap_mode(struct ath12k_pdev_dp *dp_pdev, u8 meta_data)
{
#ifdef CPTCFG_QCN_EXTN
	struct ieee80211_vif *mon_vif = NULL;
	struct ath12k_link_vif *arvif;
	bool enable = !!(meta_data & ATH12K_EXT_MON_METADATA_RTAP_HDR);
	enum ieee80211_ext_mon_event_type_extn mode =
		enable ? IEEE80211_EXT_MON_POST_RTAP : IEEE80211_EXT_MON_PRE_RTAP;

	list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
		    arvif->is_started) {
			mon_vif = arvif->ahvif->vif;
			break;
		}
	}
	if (!mon_vif)
		return;

	ieee80211_set_ext_tx_monitor(mon_vif, enable);
	ieee80211_set_ext_tx_mon_evt_typ(mon_vif, mode);
#endif /* CPTCFG_QCN_EXTN */
}

static int ath12k_dp_tx_mon_reset_ext_mon_config(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_tx_ext_mon *tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	int ret = 0;

	tx_ext_mon = &dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_config = tx_ext_mon->tx_ext_mon_config;
	if (tx_config) {
		tx_config->enable = false;
		tx_config->level = 0;
		tx_config->monitor_flags = 0;
		tx_config->metadata = 0;
		tx_config->fp_enabled = false;
		tx_config->fpmo_enabled = false;
		memset(&tx_config->fp, 0, sizeof(tx_config->fp));
		memset(&tx_config->fpmo, 0, sizeof(tx_config->fpmo));
	}
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
	ath12k_dp_ext_mon_tx_set_rtap_mode(dp_pdev, 0);
	ret = ath12k_dp_ext_mon_tx_remove_all_peers(dp_pdev);
	return ret;
}

static int ath12k_dp_mon_tx_filter_cfg(const struct ath12k_dp_arch_mon_ops *mon_ops,
				       struct ath12k_pdev_dp *dp_pdev,
				       u8 new_mode, bool enable)
{
	u8 old_mode = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_mode;
	int ret = 0;

	if (!mon_ops || !mon_ops->mon_tx_filter_configure)
		return -EINVAL;

	ret = mon_ops->mon_tx_filter_configure(dp_pdev, false);

	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: failed to clear filter mode %u: %d\n",
			    old_mode, ret);
		return ret;
	}

	dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_mode = new_mode;
	ret = mon_ops->mon_tx_filter_configure(dp_pdev, enable);

	if (ret)
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: failed to %s filter mode %u: %d\n",
			    enable ? "enable" : "clear", new_mode, ret);
	return ret;
}

void
ath12k_dp_mon_tx_update_spl_pkt_cap_stats(struct ath12k_pdev_dp *dp_pdev,
					     struct hal_tx_mon_status_info *status_info)
{
	static const char * const pkt_names[] = {
		"unknown", "ARP", "EAPOL", "DHCP", "DNS", "ICMP", "MGMT"
	};
	struct ath12k_pdev_tx_mon *tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	u8 pkt_id = status_info->spc_pkt_id;

	tx_mon->pdev_tx_mon_stats.spl_pkt_cap_stats[pkt_id] +=
					status_info->dp_tx_pkt_cap_cookie;
	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
		   "tx_pkt_cap %s +%u (total %u)\n",
		   pkt_names[pkt_id],
		   status_info->dp_tx_pkt_cap_cookie,
		   tx_mon->pdev_tx_mon_stats.spl_pkt_cap_stats[pkt_id]);
}

/**
 * ath12k_dp_mon_spl_pkt_cap_config() - Send WMI special packet classify command
 * @ab: ath12k base structure
 *
 * Sends WMI_SOC_TX_PACKET_CUSTOM_CLASSIFY_CMDID to firmware for each radio
 * that has TX monitor enabled, so FW tags high-value protocol frames with a
 * cookie in FW2SW MON TLVs before any VAP is created.
 *
 * Called from ath12k_core_pdev_init() and re-sent automatically after SSR.
 */
void ath12k_dp_mon_spl_pkt_cap_config(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		struct ath12k *ar = ab->pdevs[i].ar;
		const struct ath12k_dp_arch_mon_ops *mon_ops;
		u32 bitmap = 0;
		int ret;

		if (!ar || !ath12k_dp_tx_mon_feature_eval(ar->dp.dp))
			continue;

		mon_ops = ath12k_dp_mon_ops_get(ar->dp.dp);
		if (mon_ops && mon_ops->mon_tx_get_spc_bitmap)
			bitmap = mon_ops->mon_tx_get_spc_bitmap(ab);

		ret = ath12k_wmi_tx_mon_pkt_cap_set_config(ar, bitmap);
		if (ret)
			ath12k_warn(ab,
				    "radio%d: TX mon pkt cap WMI failed: %d\n",
				    i, ret);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_spl_pkt_cap_config);

int ath12k_dp_mon_tx_config_full_monitor(struct ath12k *ar, bool set)
{
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	int ret = -EINVAL;

	if (unlikely(!ar || !ar->ab)) {
		ath12k_err(NULL, "Invalid Radio / Radio base\n");
		return -EINVAL;
	}

	ab = ar->ab;
	dp = ath12k_ab_to_dp(ab);
	mon_ops = ath12k_dp_mon_ops_get(dp);
	dp_pdev = &ar->dp;
	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	if (set) {
		ret = ath12k_dp_mon_tx_htt_src_ring_setup(dp);
		if (ret) {
			ath12k_err(dp->ab, "TX Monitor: HTT setup failed, ret=%d", ret);
			return ret;
		}
	}

	ret = ath12k_dp_mon_tx_filter_cfg(mon_ops, dp_pdev,
					  DP_MON_TX_FULL_MONITOR, set);
	ath12k_dp_tx_mon_reset_ext_mon_config(dp_pdev);

	return ret;
}

int ath12k_dp_mon_tx_update_filter(struct ath12k *ar)
{
	struct ath12k_base *ab;
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_dp *dp_pdev;
	int ret = -EINVAL;

	if (unlikely(!ar || !ar->ab)) {
		ath12k_err(NULL, "Invalid Radio / Radio base\n");
		return -EINVAL;
	}

	ab = ar->ab;
	dp_pdev = &ar->dp;
	dp = ath12k_ab_to_dp(ab);
	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && dp_pdev && mon_ops->mon_tx_filter_update)
		ret = mon_ops->mon_tx_filter_update(dp_pdev);

	if (ret)
		ath12k_warn(ab, "Tx Mon : Filter update failed\n");

	return ret;
}

void ath12k_dp_mon_tx_srng_deinit(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	ath12k_dp_mon_tx_htt_src_ring_cleanup(dp);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_deinit);

void ath12k_dp_mon_tx_srng_free(struct ath12k_dp *dp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (mon_ops && mon_ops->mon_tx_srng_cleanup)
		mon_ops->mon_tx_srng_cleanup(dp);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_free);

int ath12k_dp_mon_tx_pdev_alloc(struct ath12k_pdev_dp *dp_pdev,
				u32 mac_id)
{
	struct ath12k_dp *dp;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ext_ret;
	int ret = 0;

	if (unlikely(!dp_pdev)) {
		ath12k_err(NULL, "Tx Mon: Invalid DP Pdev\n");
		return -EINVAL;
	}

	dp = dp_pdev->dp;
	if (!ath12k_dp_tx_mon_feature_eval(dp))
		return 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);

	if (!mon_ops) {
		ath12k_warn(dp, "Tx Mon: mon ops is NULL\n");
		return -EINVAL;
	}

	if (mon_ops->mon_tx_dst_ring_alloc_setup) {
		ret = mon_ops->mon_tx_dst_ring_alloc_setup(dp_pdev, mac_id);
		if (ret) {
			ath12k_warn(dp, "Tx Mon: failed to alloc dst ring\n");
			return ret;
		}
	}

	if (mon_ops->mon_tx_wq_start) {
		ret = mon_ops->mon_tx_wq_start(dp_pdev, mac_id);
		if (ret)
			ath12k_warn(dp, "failed to start TX mon WQ for mac_id %d: %d\n",
				    mac_id, ret);
	}

	if (mon_ops->ext_mon_tx_alloc) {
		ext_ret = mon_ops->ext_mon_tx_alloc(dp_pdev);
		if (ext_ret) {
			/* Non-fatal: TX ext mon is not critical to pdev bringup */
			ath12k_warn(dp->ab,
				    "failed to allocate TX ext mon for %s mac_id %d: %d\n",
				    dev_name(dp_pdev->dp->ab->dev), mac_id, ext_ret);
		}
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_pdev_alloc);

int ath12k_dp_mon_tx_desc_pool_alloc(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;
	int ret = 0;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return -EINVAL;

	dp_tx_mon = dp_mon->dp_tx_mon;

	spin_lock_init(&dp_tx_mon->tx_mon_desc_lock);
	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	dp_tx_mon->tx_mon_buf_ring_ready = false;
	dp_tx_mon->tx_mon_desc_pool = kcalloc(DP_TX_MON_BUF_RING_SIZE(dp->ab),
					      sizeof(*dp_tx_mon->tx_mon_desc_pool),
					      GFP_ATOMIC);
	if (!dp_tx_mon->tx_mon_desc_pool) {
		ath12k_warn(dp, "failed to allocate memory for tx mon desc pool\n");
		ret = -ENOMEM;
	}
	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
	return ret;
}

void ath12k_dp_mon_tx_desc_pool_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return;

	dp_tx_mon = dp_mon->dp_tx_mon;

	/* This check is required because, in certain error scenarios where SRNG
	 * allocation fails, this API may still be invoked. At that point, the
	 * spin lock initialization may not have completed, so this validation
	 * is necessary to avoid accessing an uninitialized spin lock.
	 */

	if (!dp_tx_mon->tx_mon_desc_pool)
		return;

	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	kfree(dp_tx_mon->tx_mon_desc_pool);
	dp_tx_mon->tx_mon_desc_pool = NULL;
	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
}

size_t ath12k_dp_mon_get_tx_free_desc_list(struct ath12k_dp *dp,
					   struct dp_rxdma_mon_ring *rx_ring,
					   struct list_head *list,
					   size_t ring_lvl)
{
	struct ath12k_dp_tx_mon *dp_tx_mon = dp->dp_mon ? dp->dp_mon->dp_tx_mon : NULL;
	struct dp_mon_desc_list_params list_params;

	if (!dp_tx_mon)
		return 0;

	list_params.desc_lock = &dp_tx_mon->tx_mon_desc_lock;
	list_params.free_list = &dp_tx_mon->tx_mon_desc_free_list;
	list_params.list_local = list;
	list_params.pf_cache = NULL;
	list_params.buff_size = 0;
	list_params.is_tx_monitor = true;

	return ath12k_dp_mon_get_free_desc_list(dp, rx_ring, &list_params, ring_lvl);
}

int ath12k_dp_mon_tx_buff_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon = dp_mon ? dp_mon->dp_tx_mon : NULL;
	struct dp_rxdma_mon_ring *tx_ring;
	int num_entries, ret = -EINVAL, free_list_count;
	size_t ring_lvl = DP_TX_MON_BUF_RING_FILL_LVL(ab);
	LIST_HEAD(list);

	if (!dp_tx_mon)
		return -EINVAL;

	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	if (dp_tx_mon->tx_mon_buf_ring_ready) {
		spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX monitor: Buffers available\n");
		return 0;
	}

	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);

	tx_ring = &dp_tx_mon->tx_mon_buf_ring;

	num_entries =  tx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_TX_MONITOR_BUF);
	tx_ring->bufs_max = num_entries;

	free_list_count =
		ath12k_dp_mon_get_tx_free_desc_list(dp, tx_ring, &list, ring_lvl);

	if (unlikely(!free_list_count)) {
		ath12k_warn(dp, "Tx Mon: No entries available for mon buf ring\n");
		return -ENOMEM;
	}

	ret = ath12k_dp_mon_tx_buf_replenish(dp, tx_ring, &list, free_list_count);
	if (ret) {
		ath12k_warn(dp, "Tx Mon: Replenish of mon buf ring failed\n");
		return ret;
	}

	if (tx_ring->bufs_max > ring_lvl)
		tx_ring->bufs_fill_lvl = ring_lvl;
	else
		tx_ring->bufs_fill_lvl = tx_ring->bufs_max;

	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	dp_tx_mon->tx_mon_buf_ring_ready = true;
	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_buff_alloc);

int ath12k_dp_mon_tx_htt_dst_ring_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 ring_id;
	int ret;

	if (!dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)
		return -EINVAL;

	ring_id = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_mon_dst_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(dp->ab, ring_id, mac_id,
					  HAL_TX_MONITOR_DST);
	if (ret)
		ath12k_warn(dp->ab,
			    "TX Monitor: failed to configure dst ring %d\n", ret);

	return ret;
}

/**
 * ath12k_dp_mon_tx_monitor_start_stop() - Start or stop TX monitor
 * @ar: per-radio ath12k context
 * @state: true to start, false to stop
 *
 * Configures TX monitor HW and filter state for the given radio.
 * Idempotent: starting an already-started or stopping an already-stopped
 * monitor returns 0 immediately.
 *
 * The WQ is pre-initialised at pdev alloc and destroyed at pdev free;
 * this function does not create or destroy it. The SSR path may clear
 * tx_monitor_started without clearing is_tx_monitor_enabled_on_ssr so
 * recovery can distinguish runtime state from restart intent.
 *
 * Start sequence (state=true):
 *   1. Enable the TX monitor destination ring via
 *      ath12k_dp_mon_tx_config_monitor_mode().
 *   2. Push the TLV filter configuration to FW via
 *      ath12k_dp_mon_tx_update_filter().
 *   3. Set tx_monitor_started=true and is_tx_monitor_enabled_on_ssr=true.
 *
 * Stop sequence (state=false):
 *   1. Disable the TX monitor destination ring.
 *   2. Push cleared filter configuration to FW.
 *   3. Set tx_monitor_started=false and is_tx_monitor_enabled_on_ssr=false.
 *
 * Context: Process context. Caller must hold wiphy_lock when invoked
 *          from mac80211 ops path.
 *
 * Return: 0 on success, negative error code on failure.
 */
int ath12k_dp_mon_tx_monitor_start_stop(struct ath12k *ar, bool state)
{
	int ret = -EOPNOTSUPP;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	bool ext_mon_enabled = false;

	dp_mon_pdev = ar->dp.dp_mon_pdev;
	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_warn(ar->ab, "Tx Monitor: Invalid Pdev (%d)\n",
			    ret);
		return ret;
	}

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	spin_lock(&dp_pdev_tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_ext_mon = dp_pdev_tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (tx_ext_mon)
		ext_mon_enabled = tx_ext_mon->enable;
	spin_unlock(&dp_pdev_tx_mon->tx_ext_mon.tx_ext_mon_lock);

	if (state && dp_pdev_tx_mon->tx_monitor_started && !ext_mon_enabled) {
		ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON,
			   "Tx mon already active on requested interface\n");
		return 0;
	}

	if (!state && !dp_pdev_tx_mon->tx_monitor_started) {
		ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON,
			   "Not running Tx mon on requested interface\n");
		ar->is_tx_monitor_enabled_on_ssr = false;
		return 0;
	}

	if (state)
		dp_pdev_tx_mon->tx_mon_teardown = false;

	ret = ath12k_dp_mon_tx_config_full_monitor(ar, state);
	if (ret) {
		ath12k_warn(ar->ab, "Tx Monitor: Configuration Failure %d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_mon_tx_update_filter(ar);
	if (ret) {
		ath12k_warn(ar->ab, "Tx Monitor: fail tx monitor filter update ret %d\n",
			    ret);
		if (ath12k_dp_mon_tx_config_full_monitor(ar, false))
			ath12k_err(ar->ab,
				   "Tx Mon: Config failure potential state mismatch\n");
		dp_pdev_tx_mon->tx_monitor_started = false;
		ar->is_tx_monitor_enabled_on_ssr = false;
		return ret;
	}
	dp_pdev_tx_mon->tx_monitor_started = state;
	ar->is_tx_monitor_enabled_on_ssr = state;

	return ret;
}

int ath12k_dp_mon_tx_set_monitor_flags(struct ath12k *ar, u32 new_flags, u32 *cur_flags)
{
	bool req_state;
	int ret = -EINVAL;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;

	dp_mon_pdev = ar->dp.dp_mon_pdev;
	if (!ath12k_dp_tx_mon_feature_eval(ar->dp.dp)) {
		ath12k_warn(ar->ab, "Tx Monitor: Config disabled/mismatch\n");
		return 0;
	}

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_warn(ar->ab, "Tx Monitor: Invalid Pdev (%d)\n", ret);
		return ret;
	}

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;

	req_state = !(new_flags & MONITOR_FLAG_SKIP_TX);
	ret = ath12k_dp_mon_tx_monitor_start_stop(ar, req_state);
	if (ret) {
		ath12k_warn(ar->ab,
			    "Tx Monitor: Attempted %d state change failed(%d)\n",
			    req_state, ret);
		return ret;
	}
	*cur_flags &= ~MONITOR_FLAG_SKIP_TX; // Clear Flag
	*cur_flags |= new_flags & MONITOR_FLAG_SKIP_TX;
	ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_TX, "Tx Monitor req. state:%d ret (%d)\n",
		   dp_pdev_tx_mon->tx_monitor_started, ret);
	return ret;
}

bool ath12k_dp_tx_mon_feature_eval(struct ath12k_dp *dp)
{
	struct ath12k_base *ab;

	if (!dp)
		return false;

	ab = dp->ab;
	if (!ab || !ab->cfg_ctx)
		return false;

	if (!ab->hw_params)
		return false;

	if (!DP_TX_MONITOR || !ab->hw_params->supports_tx_monitor) {
		ab->hw_params->supports_tx_monitor = false;
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
			   "TX Monitor Feature flag disabled\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(ath12k_dp_tx_mon_feature_eval);

void ath12k_dp_mon_tx_buff_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon = dp_mon ? dp_mon->dp_tx_mon : NULL;
	dma_addr_t dma_paddr;
	int i;
	u8 *tx_mon_buf;

	if (!dp_tx_mon)
		return;

	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	dp_tx_mon->tx_mon_buf_ring_ready = false;

	if (!dp_tx_mon->tx_mon_desc_pool) {
		spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
		return;
	}

	for (i = 0; i < DP_TX_MON_BUF_RING_SIZE(dp->ab); i++) {
		if (dp_tx_mon->tx_mon_desc_pool[i].in_use != DP_MON_DESC_TO_HW)
			continue;

		tx_mon_buf = dp_tx_mon->tx_mon_desc_pool[i].mon_buf;

		if (tx_mon_buf) {
			dma_paddr = dp_tx_mon->tx_mon_desc_pool[i].paddr;
			ath12k_core_dma_unmap_page(dp->dev, dma_paddr,
						   ATH12K_DP_MON_TX_BUF_SIZE,
						   DMA_FROM_DEVICE);
			page_frag_free(tx_mon_buf);
			dp_tx_mon->tx_num_frag_free++;
		}
		ath12k_dp_mon_desc_reset(&dp_tx_mon->tx_mon_desc_pool[i]);
	}

	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
}

void ath12k_dp_mon_tx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_tx_filter_mode mode,
				      struct dp_mon_tx_filter *filter)
{
	struct ath12k_base *ab =  dp->ab;
	struct htt_tx_ring_tlv_filter *src_tlv_filter =
				&filter->filter;
	if (!ab) {
		ath12k_err(NULL, "ath12k base invalid - skip tx filter display\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "TX MON RING TLV FILTER CONFIG");
	ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "[Mode: %d]: Valid: %d",
		   mode, filter->valid);
	if (filter->valid) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "downstream TLV Flags: 0x%X",
			   src_tlv_filter->tx_mon_downstream_tlv_flags);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-0: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags0);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-1: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags1);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-2: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags2);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "upstream TLV Flags-3: 0x%X",
			   src_tlv_filter->tx_mon_upstream_tlv_flags3);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "downstream TLV Flags-1: 0x%X",
			   src_tlv_filter->tx_mon_downstream_tlv_flags1);

		/* Print wmask configuration */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask pcu_ppdu_setup_init: 0x%X",
			   src_tlv_filter->wmask.pcu_ppdu_setup_init);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_peer_entry: 0x%X",
			   src_tlv_filter->wmask.tx_peer_entry);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_queue_ext: 0x%X",
			   src_tlv_filter->wmask.tx_queue_ext);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_status_end: 0x%X",
			   src_tlv_filter->wmask.tx_fes_status_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask response_end_status: 0x%X",
			   src_tlv_filter->wmask.response_end_status);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_status_prot: 0x%X",
			   src_tlv_filter->wmask.tx_fes_status_prot);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_fes_setup: 0x%X",
			   src_tlv_filter->wmask.tx_fes_setup);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_msdu_start: 0x%X",
			   src_tlv_filter->wmask.tx_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask tx_mpdu_start: 0x%X",
			   src_tlv_filter->wmask.tx_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask rxpcu_user_setup: 0x%X",
			   src_tlv_filter->wmask.rxpcu_user_setup);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "wmask compaction_enable: %d",
			   src_tlv_filter->wmask.compaction_enable);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
			   "wmask mactx_user_desc_common: 0x%X",
			   src_tlv_filter->wmask.mactx_user_desc_common);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX,
			   "wmask rx_resp_required_info: 0x%X",
			   src_tlv_filter->wmask.rx_resp_required_info);

		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt filter enable: %d",
			   src_tlv_filter->tx_mon_mgmt_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data filter enable: %d",
			   src_tlv_filter->tx_mon_data_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl filter enable: %d",
			   src_tlv_filter->tx_mon_ctrl_filter);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length data: %d",
			   src_tlv_filter->tx_mon_data_pkt_dma_len);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length ctrl: %d",
			   src_tlv_filter->tx_mon_ctrl_pkt_dma_len);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "packet dma length mgmt: %d",
			   src_tlv_filter->tx_mon_mgmt_pkt_dma_len);

		/* Print MPDU/MSDU start/end flags */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_end: %d",
			   src_tlv_filter->mgmt_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_msdu_end: %d",
			   src_tlv_filter->mgmt_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_msdu_start: %d",
			   src_tlv_filter->mgmt_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_start: %d",
			   src_tlv_filter->mgmt_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_end: %d",
			   src_tlv_filter->ctrl_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_msdu_end: %d",
			   src_tlv_filter->ctrl_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_msdu_start: %d",
			   src_tlv_filter->ctrl_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_start: %d",
			   src_tlv_filter->ctrl_mpdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_end: %d",
			   src_tlv_filter->data_mpdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_msdu_end: %d",
			   src_tlv_filter->data_msdu_end);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_msdu_start: %d",
			   src_tlv_filter->data_msdu_start);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_start: %d",
			   src_tlv_filter->data_mpdu_start);

		/* Print additional boolean flags */
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "txmon_disable: %d",
			   src_tlv_filter->txmon_disable);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_mpdu_msdu_log_en: %d",
			   src_tlv_filter->mgmt_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_mpdu_msdu_log_en: %d",
			   src_tlv_filter->ctrl_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_mpdu_msdu_log_en: %d",
			   src_tlv_filter->data_mpdu_msdu_log_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mgmt_log_typ: %d",
			   src_tlv_filter->mgmt_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "ctrl_log_typ: %d",
			   src_tlv_filter->ctrl_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "data_log_typ: %d",
			   src_tlv_filter->data_log_typ);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "mac_addr_filter_en: %d",
			   src_tlv_filter->mac_addr_filter_en);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON_TX, "pkt_buf_cnt_en: %d",
			   src_tlv_filter->pkt_buf_cnt_en);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_display_filters);

void
ath12k_dp_mon_tx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_tx_ring_tlv_filter *src_tlv_filter)
{
	struct ath12k_hal *hal = dp->hal;

	src_tlv_filter->tx_mon_downstream_tlv_flags =
					HTT_TX_MON_FILTER_DW_STRM_TLV_DEFAULT_MODE;
	src_tlv_filter->tx_mon_upstream_tlv_flags0 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG0;
	src_tlv_filter->tx_mon_upstream_tlv_flags1 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG1;
	src_tlv_filter->tx_mon_upstream_tlv_flags2 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG2;
	src_tlv_filter->tx_mon_upstream_tlv_flags3 =
					HTT_TX_FILTER_TLV_FLAGS3_UHR_PREAMBLE_ALL;
	src_tlv_filter->tx_mon_downstream_tlv_flags1 =
					HTT_TX_FILTER_TLV_DS_FLAGS1_ALL;

	src_tlv_filter->tx_mon_mgmt_filter = 0x1;
	src_tlv_filter->tx_mon_data_filter = 0x1;
	src_tlv_filter->tx_mon_ctrl_filter = 0x1;

	src_tlv_filter->mgmt_mpdu_end = 1;
	src_tlv_filter->mgmt_msdu_end = 1;
	src_tlv_filter->mgmt_msdu_start = 1;
	src_tlv_filter->mgmt_mpdu_start = 1;
	src_tlv_filter->ctrl_mpdu_end = 1;
	src_tlv_filter->ctrl_msdu_end = 1;
	src_tlv_filter->ctrl_msdu_start = 1;
	src_tlv_filter->ctrl_mpdu_start = 1;
	src_tlv_filter->data_mpdu_end = 1;
	src_tlv_filter->data_msdu_end = 1;
	src_tlv_filter->data_msdu_start = 1;
	src_tlv_filter->data_mpdu_start = 1;

	src_tlv_filter->mgmt_mpdu_msdu_log_en = 1;
	src_tlv_filter->ctrl_mpdu_msdu_log_en = 1;
	src_tlv_filter->data_mpdu_msdu_log_en = 1;

	src_tlv_filter->mgmt_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	src_tlv_filter->ctrl_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
	src_tlv_filter->data_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;

	src_tlv_filter->tx_mon_mgmt_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	src_tlv_filter->tx_mon_data_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	src_tlv_filter->tx_mon_ctrl_pkt_dma_len = DP_TX_MON_MAX_DMA_LENGTH;
	src_tlv_filter->pkt_buf_cnt_en = ath12k_hal_mon_tx_pkt_buf_cnt_in_desc(hal);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_setup_mon_mode_filter);

void ath12k_dp_mon_tx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_tx_filter_srng_type srng_type,
				     struct dp_mon_tx_filter *tx_mon_filter)
{
	enum dp_mon_tx_filter_mode mode = 0;
	struct htt_tx_ring_tlv_filter *dst_tlv_filter = &tx_mon_filter->filter;
	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct dp_mon_tx_filter *src_tx_mon_filter;
	struct hal_tx_mon_wmask_config *src_wmask;
	struct hal_tx_mon_wmask_config *dest_wmask = &tx_mon_filter->filter.wmask;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon ||
	    !dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON_TX,
			   "TX Mon: prepare filter skipped - not initialized\n");
		return;
	}

	/*
	 * loop through all the modes
	 */
	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		src_tx_mon_filter =
			&dp_mon_pdev->dp_pdev_tx_mon->tx_mon_filter[mode][srng_type];
		src_tlv_filter = &src_tx_mon_filter->filter;
		src_wmask = &src_tlv_filter->wmask;

		if (!src_tx_mon_filter->valid)
			continue;

		tx_mon_filter->valid = true;
		dst_tlv_filter->tx_mon_downstream_tlv_flags |=
			src_tlv_filter->tx_mon_downstream_tlv_flags;
		dst_tlv_filter->tx_mon_upstream_tlv_flags0 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags0;
		dst_tlv_filter->tx_mon_upstream_tlv_flags1 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags1;
		dst_tlv_filter->tx_mon_upstream_tlv_flags2 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags2;
		dst_tlv_filter->tx_mon_upstream_tlv_flags3 |=
			src_tlv_filter->tx_mon_upstream_tlv_flags3;
		dst_tlv_filter->tx_mon_downstream_tlv_flags1 |=
			src_tlv_filter->tx_mon_downstream_tlv_flags1;
		dst_tlv_filter->tx_mon_mgmt_filter |=
					src_tlv_filter->tx_mon_mgmt_filter;
		dst_tlv_filter->tx_mon_data_filter |=
					src_tlv_filter->tx_mon_data_filter;
		dst_tlv_filter->tx_mon_ctrl_filter |=
					src_tlv_filter->tx_mon_ctrl_filter;

		dst_tlv_filter->mgmt_mpdu_end |= src_tlv_filter->mgmt_mpdu_end;
		dst_tlv_filter->mgmt_msdu_end |= src_tlv_filter->mgmt_msdu_end;
		dst_tlv_filter->mgmt_msdu_start |= src_tlv_filter->mgmt_msdu_start;
		dst_tlv_filter->mgmt_mpdu_start |= src_tlv_filter->mgmt_mpdu_start;
		dst_tlv_filter->ctrl_mpdu_end |= src_tlv_filter->ctrl_mpdu_end;
		dst_tlv_filter->ctrl_msdu_end |= src_tlv_filter->ctrl_msdu_end;
		dst_tlv_filter->ctrl_msdu_start |= src_tlv_filter->ctrl_msdu_start;
		dst_tlv_filter->ctrl_mpdu_start |= src_tlv_filter->ctrl_mpdu_start;
		dst_tlv_filter->data_mpdu_end |= src_tlv_filter->data_mpdu_end;
		dst_tlv_filter->data_msdu_end |= src_tlv_filter->data_msdu_end;
		dst_tlv_filter->data_msdu_start |= src_tlv_filter->data_msdu_start;
		dst_tlv_filter->data_mpdu_start |= src_tlv_filter->data_mpdu_start;
		dst_tlv_filter->mgmt_mpdu_msdu_log_en |=
					src_tlv_filter->mgmt_mpdu_msdu_log_en;
		dst_tlv_filter->ctrl_mpdu_msdu_log_en |=
					src_tlv_filter->ctrl_mpdu_msdu_log_en;
		dst_tlv_filter->data_mpdu_msdu_log_en |=
					src_tlv_filter->data_mpdu_msdu_log_en;

		dst_tlv_filter->mgmt_log_typ |= src_tlv_filter->mgmt_log_typ;
		dst_tlv_filter->ctrl_log_typ |= src_tlv_filter->ctrl_log_typ;
		dst_tlv_filter->data_log_typ |= src_tlv_filter->data_log_typ;

		dst_tlv_filter->txmon_disable |= src_tlv_filter->txmon_disable;
		dst_tlv_filter->pkt_buf_cnt_en |= src_tlv_filter->pkt_buf_cnt_en;
		dst_tlv_filter->mac_addr_filter_en |=
					src_tlv_filter->mac_addr_filter_en;
		dst_tlv_filter->tx_mon_mgmt_pkt_dma_len |=
					src_tlv_filter->tx_mon_mgmt_pkt_dma_len;
		dst_tlv_filter->tx_mon_data_pkt_dma_len |=
					src_tlv_filter->tx_mon_data_pkt_dma_len;
		dst_tlv_filter->tx_mon_ctrl_pkt_dma_len |=
					src_tlv_filter->tx_mon_ctrl_pkt_dma_len;

		dest_wmask->pcu_ppdu_setup_init |= src_wmask->pcu_ppdu_setup_init;
		dest_wmask->tx_peer_entry |= src_wmask->tx_peer_entry;
		dest_wmask->tx_queue_ext |= src_wmask->tx_queue_ext;
		dest_wmask->tx_fes_status_end |= src_wmask->tx_fes_status_end;
		dest_wmask->response_end_status |= src_wmask->response_end_status;
		dest_wmask->tx_fes_status_prot |= src_wmask->tx_fes_status_prot;
		dest_wmask->tx_fes_setup |= src_wmask->tx_fes_setup;
		dest_wmask->tx_msdu_start |= src_wmask->tx_msdu_start;
		dest_wmask->tx_mpdu_start |= src_wmask->tx_mpdu_start;
		dest_wmask->rxpcu_user_setup |= src_wmask->rxpcu_user_setup;
		dest_wmask->compaction_enable |= src_wmask->compaction_enable;
		dest_wmask->mactx_user_desc_common |=
					src_wmask->mactx_user_desc_common;
		dest_wmask->rx_resp_required_info |=
					src_wmask->rx_resp_required_info;

		ath12k_generic_dbg(ATH12K_DBG_DP_MON_TX, ATH12K_DBG_L1,
				   "Updated Tx filters for mode: %d", mode);
		ath12k_dp_mon_tx_display_filters(dp, mode, tx_mon_filter);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_prepare_filter);

int
ath12k_dp_mon_tx_htt_update_filters(struct ath12k_dp *dp,
				    struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_tx_filter_srng_type srng_type,
				    struct htt_tx_ring_tlv_filter *tx_tlv_filter)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = -EINVAL;
	enum hal_ring_type ring_type = HAL_TX_MONITOR_DST;
	struct ath12k_base *ab = dp->ab;

	int ring_buf_size, mac_id, ring_id;

	if (!dp_mon_pdev || !ab) {
		ath12k_err(NULL, "Tx Mon: mon pdev/base invalid - skip filter config\n");
		return ret;
	}

	if (!dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_err(dp->ab, "Tx Mon: pdev TX mon not initialized\n");
		return ret;
	}

	ring_id = dp_mon_pdev->dp_pdev_tx_mon->tx_mon_dst_ring.ring_id;
	mac_id = dp_pdev->mac_id;
	ring_buf_size = DP_RXDMA_REFILL_RING_SIZE;

	ret = ath12k_dp_htt_mon_tx_filter_setup(dp->ab, ring_id, mac_id,
						ring_type, ring_buf_size,
						tx_tlv_filter);
	if (ret) {
		ath12k_err(dp->ab,
			   "Tx Mon filter setup fail ring = %d srng_type = %d (%d)\n",
			   ring_id, srng_type, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_htt_update_filters);

int ath12k_dp_mon_tx_srng_alloc_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;
	int ret;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return -EINVAL;

	dp_tx_mon = dp_mon->dp_tx_mon;

	ret = ath12k_dp_srng_alloc(ab,
				   &dp_tx_mon->tx_mon_buf_ring.refill_buf_ring,
				   HAL_TX_MONITOR_BUF, 0, 0,
				   DP_TX_MON_BUF_RING_SIZE(ab));
	if (ret) {
		ath12k_warn(dp, "Tx Mon: failed to alloc buffer srng (%d)\n", ret);
		return ret;
	}

	ret = ath12k_dp_mon_tx_desc_pool_alloc(dp);
	if (ret)
		goto err_srng_cleanup;

	return 0;

err_srng_cleanup:
	ath12k_dp_srng_cleanup(ab, &dp_tx_mon->tx_mon_buf_ring.refill_buf_ring);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_alloc_setup);

void ath12k_dp_mon_tx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;
	struct dp_srng *srng;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return;
	dp_tx_mon = dp_mon->dp_tx_mon;
	srng = &dp_tx_mon->tx_mon_buf_ring.refill_buf_ring;
	ath12k_dp_mon_tx_desc_pool_free(dp);
	ath12k_dp_srng_cleanup(ab, srng);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_cleanup);

static
int ath12k_dp_mon_tx_desc_pool_init(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon = dp_mon ? dp_mon->dp_tx_mon : NULL;
	int i;

	if (!dp_tx_mon)
		return -EINVAL;

	INIT_LIST_HEAD(&dp_tx_mon->tx_mon_desc_free_list);
	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);
	if (!dp_tx_mon->tx_mon_desc_pool) {
		spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
		ath12k_warn(dp, "tx mon desc pool not available\n");
		return -ENOMEM;
	}

	 memset(dp_tx_mon->tx_mon_desc_pool, 0,
		DP_TX_MON_BUF_RING_SIZE(dp->ab) * sizeof(*dp_tx_mon->tx_mon_desc_pool));

	for (i = 0; i < DP_TX_MON_BUF_RING_SIZE(dp->ab); i++) {
		dp_tx_mon->tx_mon_desc_pool[i].magic = ATH12K_MON_MAGIC_VALUE;
		INIT_LIST_HEAD(&dp_tx_mon->tx_mon_desc_pool[i].list);
		list_add_tail(&dp_tx_mon->tx_mon_desc_pool[i].list,
			      &dp_tx_mon->tx_mon_desc_free_list);
	}

	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);

	return 0;
}

int ath12k_dp_mon_tx_srng_init_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon = dp_mon ? dp_mon->dp_tx_mon : NULL;
	int ret;

	if (!dp_tx_mon)
		return -EINVAL;

	ret = ath12k_dp_srng_init(ab,
				  &dp_tx_mon->tx_mon_buf_ring.refill_buf_ring,
				  HAL_TX_MONITOR_BUF, 0, 0);
	if (ret) {
		ath12k_warn(dp, "Tx Mon: failed to init buffer srng (%d)\n", ret);
		return ret;
	}

	ret = ath12k_dp_mon_tx_desc_pool_init(dp);
	if (ret) {
		ath12k_warn(dp, "Tx Mon: failed to init desc pool (%d)\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_srng_init_setup);

void ath12k_dp_mon_tx_filter_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_tx_filter **tx_mon_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	enum dp_mon_tx_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!dp_mon_pdev || !dp || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_err(NULL, "Monitor pdev/dp is NULL - skip free\n");
		return;
	}

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	tx_mon_filter = dp_pdev_tx_mon->tx_mon_filter;

	if (!tx_mon_filter) {
		ath12k_warn(dp, "Monitor tx filter is NULL - skip free\n");
		return;
	}

	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		if (!tx_mon_filter[mode])
			continue;
		kfree(tx_mon_filter[mode]);
		tx_mon_filter[mode] = NULL;
	}

	kfree(tx_mon_filter);
	dp_pdev_tx_mon->tx_mon_filter = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_filter_free);

int ath12k_dp_mon_tx_filter_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct dp_mon_tx_filter **tx_mon_filter = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	enum dp_mon_tx_filter_mode mode;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	size_t rq_size = sizeof(struct dp_mon_tx_filter *) * DP_MON_TX_FILTER_MAX;

	if (!dp_mon_pdev || !dp || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_err(NULL, "Monitor pdev/dp is NULL\n");
		return -EINVAL;
	}

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	tx_mon_filter = (struct dp_mon_tx_filter **)
				kzalloc(rq_size, GFP_KERNEL);
	if (!tx_mon_filter)
		return -ENOMEM;

	dp_pdev_tx_mon->tx_mon_filter = tx_mon_filter;
	rq_size = sizeof(struct dp_mon_tx_filter) * DP_MON_TX_FILTER_SRNG_TYPE_MAX;
	for (mode = 0; mode < DP_MON_TX_FILTER_MAX; mode++) {
		tx_mon_filter[mode] = (struct dp_mon_tx_filter *)
					kzalloc(rq_size, GFP_KERNEL);
		if (!tx_mon_filter[mode])
			goto free_tx_filter;
	}

	return 0;

free_tx_filter:
	ath12k_dp_mon_tx_filter_free(dp_pdev);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_filter_alloc);

int ath12k_dp_mon_tx_dst_ring_alloc_setup(struct ath12k_pdev_dp *dp_pdev,
					  u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int ret;

	if (!dp_pdev->dp_mon_pdev || !dp_pdev->dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_warn(dp->ab, "Tx Mon: pdev TX mon not initialized\n");
		return -EINVAL;
	}

	ret = ath12k_dp_srng_alloc(dp->ab,
				   &dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_mon_dst_ring,
				   HAL_TX_MONITOR_DST, 0, mac_id,
				   DP_TX_MON_DST_RING_SIZE(dp->ab));
	if (ret) {
		ath12k_warn(dp->ab, "Tx Mon: failed dest. ring allocation\n");
		return ret;
	}

	ret = ath12k_dp_srng_init(dp->ab,
				  &dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_mon_dst_ring,
				  HAL_TX_MONITOR_DST, 0, mac_id);
	if (ret) {
		ath12k_warn(dp->ab, "Tx Mon: failed dest. srng ring init\n");
		return ret;
	}

	ret = ath12k_dp_mon_tx_htt_dst_ring_setup(dp_pdev, mac_id);

	if (ret) {
		ath12k_warn(dp->ab, "Tx Mon: failed dest. ring config\n");
		return ret;
	}

	ret = ath12k_dp_mon_tx_filter_alloc(dp_pdev);
	if (ret)
		ath12k_warn(dp->ab, "Tx Mon: failed filter alloc\n");

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_dst_ring_alloc_setup);

void ath12k_dp_mon_tx_dst_ring_cleanup(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;

	ath12k_dp_mon_tx_filter_free(dp_pdev);
	if (!dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)
		return;
	ath12k_dp_srng_cleanup(dp->ab,
			       &dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->tx_mon_dst_ring);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_dst_ring_cleanup);

void ath12k_dp_mon_tx_htt_srng_cleanup(struct ath12k_dp *dp)
{
	ath12k_dp_mon_tx_buff_free(dp);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_htt_srng_cleanup);

int ath12k_dp_mon_tx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;
	u32 ring_id;
	int ret;

	if (!dp_mon || !dp_mon->dp_tx_mon)
		return -EINVAL;

	dp_tx_mon = dp_mon->dp_tx_mon;

	/* This is deferred buffer initialization, This is freed at mon_tx_srng_cleanup */
	ret = ath12k_dp_mon_tx_buff_alloc(dp);
	if (ret) {
		ath12k_warn(dp->ab, "TX Monitor: Buffer setup failed(%d)\n", ret);
		return ret;
	}

	ring_id = dp_tx_mon->tx_mon_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					  0, HAL_TX_MONITOR_BUF);
	if (ret)
		ath12k_warn(ab, "TX Monitor:failed to configure buffer ring %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_htt_srng_setup);

void ath12k_dp_mon_tx_update_buf_ownership_stats(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *mon_dp = dp->dp_mon;
	struct ath12k_dp_tx_mon_stats *stats;
	struct ath12k_dp_mon_desc *desc;
	u32 with_hw = 0, in_reap = 0, free = 0, proc_err = 0, replenish_err = 0, i;

	if (!mon_dp || !mon_dp->dp_tx_mon)
		return;

	stats = &mon_dp->dp_tx_mon->tx_mon_stats;
	/* lockdep_assert_held(&dp_mon->tx_mon_desc_lock) */
	for (i = 0; i < DP_TX_MON_BUF_RING_SIZE(dp->ab); i++) {
		desc = &mon_dp->dp_tx_mon->tx_mon_desc_pool[i];
		switch (desc->in_use) {
		case DP_MON_DESC_TO_HW:
			with_hw++;
			break;
		case DP_MON_DESC_PACKET_REAP:
		case DP_MON_DESC_STATUS_REAP:
			in_reap++;
			break;
		case DP_MON_DESC_REPLENISH:
			free++;
			break;
		case DP_MON_DESC_H_PROC_ERR:
			proc_err++;
			break;
		case DP_MON_DESC_H_REPLENISH_ERR:
			replenish_err++;
			break;
		}
	}
	stats->with_hw = with_hw;
	stats->in_reap = in_reap;
	stats->free = free;
	stats->proc_err = proc_err;
	stats->replenish_err = replenish_err;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_update_buf_ownership_stats);

int ath12k_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
				   bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter filter = {0};

	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	enum dp_mon_tx_filter_mode mode = DP_MON_TX_FULL_MONITOR;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!dp || !dp->hal) {
		ath12k_err(NULL, "dp / dp hal  invalid - skipping tx mon mode config\n");
		return -EINVAL;
	}

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon)
		return -EINVAL;

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (enable) {
		filter.valid = true;
		src_tlv_filter = &filter.filter;
		ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
		ath12k_hal_mon_tx_get_wmask_config(dp->hal, &src_tlv_filter->wmask);
		dp_pdev_tx_mon->tx_mon_filter[mode][srng_type] = filter;
	} else {
		dp_pdev_tx_mon->tx_mon_filter[mode][srng_type] = filter;
	}
	ath12k_dp_mon_tx_display_filters(dp, mode, &filter);
	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_config_filter);

int ath12k_dp_mon_tx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter tx_mon_filter = {0};
	struct htt_tx_ring_tlv_filter *tx_tlv_filter;
	int ret = -EINVAL;
	enum dp_mon_tx_filter_srng_type srng_type = DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;

	if (!dp) {
		ath12k_err(NULL, "dp invalid - skipping tx mon filter update\n");
		return ret;
	}

	tx_tlv_filter = &tx_mon_filter.filter;
	ath12k_dp_mon_tx_prepare_filter(dp, dp_pdev, srng_type, &tx_mon_filter);
	if (tx_mon_filter.valid)
		tx_tlv_filter->txmon_disable = false;
	else
		tx_tlv_filter->txmon_disable = true;

	ret = ath12k_dp_mon_tx_htt_update_filters(dp, dp_pdev, srng_type,
						  tx_tlv_filter);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_update_ring_filter);

int ath12k_dp_mon_tx_buf_replenish(struct ath12k_dp *dp,
				   struct dp_rxdma_mon_ring *buf_ring,
				   struct list_head *used_list,
				   int req_entries)
{
	struct ath12k_dp_tx_mon *dp_tx_mon = dp->dp_mon ? dp->dp_mon->dp_tx_mon : NULL;
	struct dp_mon_desc_list_params list_params;

	if (!dp_tx_mon)
		return -EINVAL;

	list_params.desc_lock = &dp_tx_mon->tx_mon_desc_lock;
	list_params.free_list = &dp_tx_mon->tx_mon_desc_free_list;
	list_params.list_local = used_list;
	list_params.pf_cache = &dp_tx_mon->tx_mon_pf_cache;
	list_params.buff_size = ATH12K_DP_MON_TX_BUF_SIZE;
	list_params.is_tx_monitor = true;

	return ath12k_dp_mon_buf_replenish(dp, buf_ring, req_entries, &list_params);
}

void ath12k_dp_mon_tx_process_low_thres(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon = dp_mon ? dp_mon->dp_tx_mon : NULL;
	struct dp_rxdma_mon_ring *tx_buff_ring;
	struct hal_srng *srng;
	int num_free, free_list_count;
	int reserved_bufs;
	LIST_HEAD(list);

	if (!dp_tx_mon)
		return;

	spin_lock_bh(&dp_tx_mon->tx_mon_desc_lock);

	/* Rings are not initialized - deffer the refill
	 * This IRQ group is shared by Rx Mon dest. ring interrupts - Likely to happen
	 */
	if (!dp_tx_mon->tx_mon_buf_ring_ready) {
		spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
		return;
	}

	tx_buff_ring = &dp_tx_mon->tx_mon_buf_ring;
	srng = &dp->hal->srng_list[tx_buff_ring->refill_buf_ring.ring_id];

	/* Buffers intentionally left unfilled so the ring is maintained at
	 * bufs_fill_lvl, not full.
	 */
	reserved_bufs = tx_buff_ring->bufs_max - tx_buff_ring->bufs_fill_lvl;

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);

	/* Discount the buffers intentionally left unfilled. num_free is signed:
	 * if the physical free count is below the reserved gap, num_free goes
	 * negative here and is caught by the half-fill check below (it never
	 * reaches the unsigned list_cut_nodes count). Keep num_free signed.
	 */
	num_free = num_free - reserved_bufs;

	/* if ring is less than half filled need to replenish */
	if (num_free < (tx_buff_ring->bufs_fill_lvl / 2)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);
		return;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	free_list_count =
		ath12k_dp_mon_list_cut_nodes(&list,
					     &dp_tx_mon->tx_mon_desc_free_list,
					     num_free);
	spin_unlock_bh(&dp_tx_mon->tx_mon_desc_lock);

	if (free_list_count)
		ath12k_dp_mon_tx_buf_replenish(dp, tx_buff_ring, &list, free_list_count);
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_process_low_thres);

static int
ath12k_dp_ext_mon_tx_prepare_add_peers(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_ext_mon_tx_peer_params
					 *ext_mon_peer_params)
{
	struct ath12k_link_vif *arvif;
	const struct ath12k_ext_mon_peer_info *peer_info;
	struct ath12k_dp_ext_mon_peer *peer;
	bool found;
	int i;
	const struct ath12k_ext_mon_peer_config *peer_config =
		ext_mon_peer_params->peer_config;
	u8 peers_to_add_cnt = peer_config->count;

	for (i = 0; i < peer_config->count; i++) {
		if (!peer_config->peer_info[i].ra_addr) {
			ath12k_warn(dp_pdev->dp,
				    "skipping peer %pM: TA not allowed in TX direction\n",
				    peer_config->peer_info[i].mac_addr);
			peers_to_add_cnt--;
		}
	}

	if (peers_to_add_cnt == 0)
		return -EINVAL;

	if ((*ext_mon_peer_params->ext_mon_peer_count) + peers_to_add_cnt >
		ATH12K_EXT_MON_MAX_PEERS) {
		ath12k_warn(dp_pdev->dp,
			    "adding %u peers would exceed max %d (current: %u)\n",
			    peers_to_add_cnt, ATH12K_EXT_MON_MAX_PEERS,
			    (*ext_mon_peer_params->ext_mon_peer_count));
		return -EINVAL;
	}

	for (i = 0; i < peer_config->count; i++) {
		peer_info = &peer_config->peer_info[i];
		found = false;

		if (!peer_info->ra_addr)
			continue;

		list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
			if (ether_addr_equal(arvif->bssid, peer_info->mac_addr)) {
				found = true;
				ath12k_warn(dp_pdev->dp,
					    "skipped adding bssid %pM as peer\n",
					    peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		/* TODO: Optimize with the help of hash table */
		list_for_each_entry(peer, ext_mon_peer_params->peer_list, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					     peer_info->mac_addr)) {
				found = true;
				ath12k_warn(dp_pdev->dp, "peer %pM already added\n",
					    peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		list_for_each_entry(peer, ext_mon_peer_params->peers_to_wmi, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					     peer_info->mac_addr)) {
				found = true;
				ath12k_warn(dp_pdev->dp,
					    "peer %pM duplicate in request\n",
					    peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer)
			continue;

		memcpy(&peer->peer_info, peer_info, sizeof(*peer_info));
		list_add_tail(&peer->list, ext_mon_peer_params->peers_to_wmi);
		ext_mon_peer_params->staged_count++;
	}

	if (ext_mon_peer_params->staged_count == 0)
		return -EINVAL;
	return 0;
}

static int
ath12k_dp_ext_mon_add_tx_peers(struct ath12k_pdev_dp *dp_pdev,
			       bool skip_validation,
			       const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_tx_mon *tx_mon;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	LIST_HEAD(peers_to_wmi);
	int ret = 0, vdev_id;
	struct ath12k_dp_ext_mon_tx_peer_params tx_mon_peer_params = {
		.peers_to_wmi = &peers_to_wmi,
		.staged_count = 0,
		.peer_config = peer_config,
		.toggle_hw_state = false,
	};

	vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	if (vdev_id == -1) {
		ath12k_warn(dp_pdev->dp, "no active monitor vdev found\n");
		return -ENODEV;
	}

	tx_mon_peer_params.vdev_id = vdev_id;
	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	if (!mon_ops) {
		ath12k_warn(dp_pdev->dp,
			   "peer configuration via ext_mon not supported\n");
		return -EOPNOTSUPP;
	}

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (unlikely(!tx_ext_mon)) {
		ath12k_warn(dp_pdev->dp, "ext_mon in tx direction is null\n");
		ret = -EINVAL;
		goto unlock;
	}

	if (skip_validation) {
		tx_mon_peer_params.staged_count = tx_ext_mon->peer_count;
		/**
		 * Staging all peers to add present in the peer list.
		 * Making peer_count = 0 to stage all the peers for addition.
		 */
		tx_ext_mon->peer_count = 0;
		list_splice_tail_init(&tx_ext_mon->peer_list,
				      tx_mon_peer_params.peers_to_wmi);
	} else {
		tx_mon_peer_params.peer_list = &tx_ext_mon->peer_list;
		tx_mon_peer_params.ext_mon_peer_count = &tx_ext_mon->peer_count;

		ret = ath12k_dp_ext_mon_tx_prepare_add_peers(dp_pdev,
							     &tx_mon_peer_params);
		if (ret)
			goto unlock;
	}
	if (tx_ext_mon->peer_count == 0)
		tx_mon_peer_params.toggle_hw_state = true;
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	/**
	 * ext_mon_add_wmi_tx_peers sends a WMI command and
	 * waits on the firmware round-trip, which can sleep, that is
	 * unsafe under a spinlock. tx_ext_mon_config is re-checked,
	 * re-locked below before peers_to_wmi is spliced back into
	 * tx_ext_mon->peer_list.
	 */
	if (mon_ops->ext_mon_add_wmi_tx_peers)
		ret = mon_ops->ext_mon_add_wmi_tx_peers(dp_pdev,
							&tx_mon_peer_params);

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (unlikely(!tx_ext_mon)) {
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		/** tx_ext_mon_config was torn down while the WMI add above was
		 * in flight, so these peers were never committed to
		 * tx_ext_mon->peer_list. If hw peer filtering was active, FW
		 * still has them staged from the add call; remove them so FW
		 * doesn't retain filter entries the host no longer tracks.
		 */
		tx_mon_peer_params.toggle_hw_state = true;
		if (mon_ops->ext_mon_remove_wmi_tx_peers)
			ret = mon_ops->ext_mon_remove_wmi_tx_peers(dp_pdev,
								   &tx_mon_peer_params);
		ath12k_dp_ext_mon_delete_tx_peer_list(&peers_to_wmi);
		return -EINVAL;
	}

	tx_ext_mon->peer_count += tx_mon_peer_params.staged_count;
	list_splice_tail(&peers_to_wmi, &tx_ext_mon->peer_list);
	/** Peers can be skipped from legitimate reasons,
	 * the successfully staged subset above is still committed.
	 * Report -EINVAL anyway so the caller knows the request
	 * wasn't fully honored, even though it's not a hard failure.
	 */
	if  (tx_mon_peer_params.staged_count != peer_config->count)
		ret = -EINVAL;

unlock:
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	return ret;
}

/**
 * ath12k_dp_mon_tx_drain_status_desc() - DMA-unmap and free one TX monitor
 * status descriptor during SSR cleanup.
 * @dp_pdev: DP pdev handle, used to reach the DMA device
 * @desc: status descriptor to drain; no-op if mon_buf is NULL
 *
 * Releases a single status descriptor entry during SSR drain. The TLV-aware
 * cleanup is needed because TX monitor status buffers can reference additional
 * packet buffers via HAL_TX_MON_BUF_ADDR TLVs.
 */
static void
ath12k_dp_mon_tx_drain_status_desc(struct ath12k_pdev_dp *dp_pdev,
				   struct ath12k_dp_mon_status_desc *desc)
{
	struct ath12k_hal *hal = &dp_pdev->dp->ab->hal;
	bool pkt_buf_cnt_in_desc;

	if (!desc->mon_buf)
		return;

	pkt_buf_cnt_in_desc = ath12k_hal_mon_tx_pkt_buf_cnt_in_desc(hal);

	ath12k_core_dma_unmap_page(dp_pdev->dp->dev, desc->paddr,
				   ATH12K_DP_MON_TX_BUF_SIZE, DMA_FROM_DEVICE);
	ath12k_dp_tx_mon_flush_tlv(dp_pdev, desc, pkt_buf_cnt_in_desc);

	desc->mon_buf = NULL;
	desc->paddr = 0;
	desc->buf_len = 0;
	desc->end_of_ppdu = false;
	desc->pkt_buf_cnt = 0;
}

/**
 * ath12k_dp_mon_tx_ssr_restart_pdev() - Restart TX monitor after SSR
 * @dp_pdev: DP pdev handle for the recovering radio
 *
 * Called from ath12k_mac_monitor_start() when is_tx_monitor_enabled_on_ssr
 * is set, indicating that TX monitor was active before the crash.
 *
 * The WQ is created by wq_start() at pdev alloc time and destroyed by
 * wq_stop() at pdev free. It remains alive across SSR because wq_stop() is
 * not called on the SSR path. This function cancels any in-flight txmon_work
 * and drains stale descriptors before recovery re-arms the HW/FW monitor state.
 *
 * Restart sequence:
 *   1. Verify dp_pdev_tx_mon is allocated; abort if not.
 *   2. Verify the descriptor pool is ready; warn and abort if not.
 *   3. Drain partial descriptors from tx_mon_desc_work_list.
 *   4. Splice used_list and proc_list into a private local_drain_list under
 *      tx_mon_ppdu_desc_lock. For each ppdu_desc, DMA-unmap and free every
 *      status_desc via ath12k_dp_mon_tx_drain_status_desc(), then reset via
 *      ath12k_dp_mon_reset_ppdu_desc(). Bulk-return to free_list under lock.
 *   5. Call ath12k_dp_mon_tx_monitor_start_stop(ar, true) to re-enable the HW
 *      ring and push TLV filter config to FW.
 *
 * On monitor_start_stop() failure: is_tx_monitor_enabled_on_ssr is cleared
 * here explicitly. monitor_start_stop(ar, true) does not reach its state-update
 * block on early failure returns, so the flag would not be cleared otherwise.
 * Clearing it prevents a re-entry loop on the next SSR cycle.
 */
void ath12k_dp_mon_tx_ssr_restart_pdev(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;
	struct ath12k_tx_mon_ssr_stats *stats;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct hal_srng *tx_mon_dst_ring;
	struct list_head *work_list;
	LIST_HEAD(local_drain_list);
	struct ath12k *ar;
	bool desc_initialized;
	u32 ppdu_desc_drained = 0;
	u32 status_desc_drained = 0;
	u32 mon_desc_flushed;
	u32 ring_id;
	int i, ret;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return;

	ar = dp_pdev->ar;
	dp_mon_pdev = dp_pdev->dp_mon_pdev;

	dp_pdev_tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (WARN_ON(!dp_pdev_tx_mon)) {
		ar->is_tx_monitor_enabled_on_ssr = false;
		return;
	}
	work_list = &dp_pdev_tx_mon->tx_mon_desc_work_list;
	stats = &ar->tx_mon_ssr_stats;
	stats->restart_attempts++;

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	desc_initialized = dp_pdev_tx_mon->tx_mon_ppdu_desc_initialized;
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	if (!desc_initialized) {
		ath12k_warn(ar->ab,
			    "TX monitor desc pool not ready during SSR recovery\n");
		stats->restart_no_pool++;
		ar->is_tx_monitor_enabled_on_ssr = false;
		return;
	}

	ring_id = dp_pdev_tx_mon->tx_mon_dst_ring.ring_id;
	tx_mon_dst_ring = &ar->ab->hal.srng_list[ring_id];
	spin_lock_bh(&tx_mon_dst_ring->lock);
	mon_desc_flushed = ath12k_dp_tx_mon_flush_desc_list(dp_pdev, work_list);
	spin_unlock_bh(&tx_mon_dst_ring->lock);
	stats->mon_desc_flushed += mon_desc_flushed;

	if (dp_pdev_tx_mon->tx_mon_wq_initialized) {
		stats->work_cancel++;
		cancel_work_sync(&dp_pdev_tx_mon->txmon_work);
	}

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	list_splice_init(&dp_pdev_tx_mon->tx_mon_ppdu_desc_used_list,
			 &local_drain_list);
	list_splice_tail_init(&dp_pdev_tx_mon->tx_mon_ppdu_desc_proc_list,
			      &local_drain_list);
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	list_for_each_entry(ppdu_desc, &local_drain_list, list) {
		ppdu_desc_drained++;
		for (i = 0; i < ppdu_desc->status_desc_cnt; i++) {
			if (ppdu_desc->status_desc[i].mon_buf)
				status_desc_drained++;
			ath12k_dp_mon_tx_drain_status_desc(dp_pdev,
							   &ppdu_desc->status_desc[i]);
		}
		ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
	}
	stats->ppdu_desc_drained += ppdu_desc_drained;
	stats->status_desc_drained += status_desc_drained;

	spin_lock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);
	list_splice_tail_init(&local_drain_list,
			      &dp_pdev_tx_mon->tx_mon_ppdu_desc_free_list);
	spin_unlock_bh(&dp_pdev_tx_mon->tx_mon_ppdu_desc_lock);

	/* SSR invalidates the HW/FW TX monitor programming; force re-arm below. */
	dp_pdev_tx_mon->tx_monitor_started = false;
	ret = ath12k_dp_mon_tx_monitor_start_stop(ar, true);
	if (ret) {
		ath12k_warn(ar->ab,
			    "TX monitor restart failed during SSR: %d\n", ret);
		stats->restart_fail++;
		ar->is_tx_monitor_enabled_on_ssr = false;
	} else {
		stats->restart_success++;
	}
}

int
ath12k_dp_ext_mon_handle_tx_peer(struct ath12k_pdev_dp *dp_pdev,
				 const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	if (!dp_mon_pdev->dp_pdev_tx_mon ||
	    !dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_started) {
		ath12k_warn(dp_pdev->dp, "Tx monitor not enabled\n");
		return -EINVAL;
	}

	switch (peer_config->action) {
	case QCA_VENDOR_EXT_MON_PEER_ACTION_ADD:
		return ath12k_dp_ext_mon_add_tx_peers(dp_pdev, false, peer_config);
	case QCA_VENDOR_EXT_MON_PEER_ACTION_REMOVE:
		return ath12k_dp_ext_mon_remove_tx_peers(dp_pdev, false, peer_config);
	default:
		ath12k_warn(dp_pdev->dp, "invalid peer action %u\n",
				peer_config->action);
	}

	return -EINVAL;
}

int
ath12k_dp_ext_mon_get_tx_filter(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon_config;
	int ret = 0;
	bool tx_mon_started;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp->ab, "monitor pdev is null\n");
		return -EINVAL;
	}

	if (unlikely(!dp_mon_pdev->dp_pdev_tx_mon)) {
		ath12k_warn(dp_pdev->dp, "tx monitor pdev is null\n");
		return -EINVAL;
	}

	tx_mon_started = dp_mon_pdev->dp_pdev_tx_mon->tx_monitor_started;
	if (!tx_mon_started)
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "Enable TX monitor for this feature.\n");

	tx_ext_mon = &dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_ext_mon_config = tx_ext_mon->tx_ext_mon_config;
	if (unlikely(!tx_ext_mon_config)) {
		ath12k_warn(dp_pdev->dp, "ext_mon in tx direction is null\n");
		ret = -EINVAL;
		goto unlock;
	}

	resp->filter.disable = !tx_ext_mon_config->enable;
	if (!tx_ext_mon_config->enable)
		goto unlock;

	resp->filter.level = tx_ext_mon_config->level;
	resp->filter.all_peer = tx_ext_mon_config->fp;
	resp->filter.target_peer = tx_ext_mon_config->fpmo;
	resp->filter.meta_data = tx_ext_mon_config->metadata;
	resp->filter.monitor_flags = tx_ext_mon_config->monitor_flags;

unlock:
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
	return ret;
}

int
ath12k_dp_ext_mon_get_tx_peer(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	struct ath12k_pdev_tx_mon *tx_mon;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp->ab, "monitor pdev is null\n");
		return -EINVAL;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (unlikely(!tx_mon)) {
		ath12k_warn(dp_pdev->dp, "TX monitor is null\n");
		return -EINVAL;
	}

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (!tx_ext_mon) {
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "ext_mon in tx direction is null\n");
		return -EINVAL;
	}

	ath12k_dp_get_ext_mon_peers(dp_pdev,
				    &tx_ext_mon->peer_list, resp,
				    &tx_mon->tx_ext_mon.tx_ext_mon_lock);
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	return 0;
}

static int
ath12k_dp_ext_mon_update_tx_config(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				   const struct ath12k_ext_mon_filter_config *new_config)
{
	struct ath12k_dp_tx_ext_mon *curr;
	struct ath12k_dp_tx_ext_mon_config *curr_config;

	if (unlikely(!dp_mon_pdev->dp_pdev_tx_mon))
		return -EINVAL;

	curr = &dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;

	spin_lock(&curr->tx_ext_mon_lock);
	curr_config = curr->tx_ext_mon_config;

	if (unlikely(!curr_config)) {
		spin_unlock(&curr->tx_ext_mon_lock);
		ath12k_dbg(dp_mon_pdev->dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "ext_mon is not initialized\n");
		return -EINVAL;
	}

	if (new_config->disable && !curr_config->enable) {
		spin_unlock(&curr->tx_ext_mon_lock);
		ath12k_dbg(dp_mon_pdev->dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "ext_mon is already disabled\n");
		return 0;
	}

	if (!new_config->disable) {
		curr_config->level = new_config->level;

		curr_config->fp = new_config->all_peer;
		curr_config->fpmo = new_config->target_peer;

		curr_config->fp_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->all_peer);
		curr_config->fpmo_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->target_peer);
		curr_config->monitor_flags = new_config->monitor_flags;
		curr_config->metadata = new_config->meta_data;
		curr_config->enable = true;
	} else {
		curr_config->enable = false;
		curr_config->level = 0;
		curr_config->metadata = 0;
		curr_config->monitor_flags = 0;
		curr_config->fp_enabled = false;
		curr_config->fpmo_enabled = false;
		memset(&curr_config->fp, 0, sizeof(curr_config->fp));
		memset(&curr_config->fpmo, 0, sizeof(curr_config->fpmo));
	}
	spin_unlock(&curr->tx_ext_mon_lock);
	return 0;
}

static int ath12k_dp_mon_tx_update_send_filter(struct ath12k_pdev_dp *dp_pdev,
					       struct ath12k_pdev_tx_mon *tx_mon,
					       enum dp_mon_tx_filter_mode mode)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops =
		ath12k_dp_mon_ops_get(dp_pdev->dp);
	int ret;

	if (unlikely(!mon_ops || !mon_ops->mon_tx_filter_configure))
		return -EINVAL;

	ret = ath12k_dp_mon_tx_filter_cfg(mon_ops, dp_pdev, mode, true);
	if (ret)
		return ret;

	ret = ath12k_dp_mon_tx_update_ring_filter(dp_pdev);
	if (ret)
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: failed to update ring filter for mode %u: %d\n",
			    mode, ret);
	return ret;
}

static void dp_tx_mon_disable_full_tx_mon(struct ath12k_pdev_dp *dp_pdev,
					  struct ath12k *ar)
{
	ath12k_warn(dp_pdev->dp->ab, "TX Mon: disabling TX monitor\n");
	if (ath12k_dp_mon_tx_monitor_start_stop(ar, false))
		ath12k_err(dp_pdev->dp->ab,
			   "TX Mon: disable failed after recovery failure\n");
}

/*
 * ath12k_dp_ext_mon_tx_recover() - recovery when ext-mon filter apply fails
 *
 * Tries, in order:
 *   1. Restore previous ext-mon config + filter  (only if ext-mon was on)
 *   2. Fall back to full monitor via start_stop(true)
 *   3. Kill the monitor entirely if all else fails
 */
static void ath12k_dp_ext_mon_tx_recover(struct ath12k_pdev_dp *dp_pdev,
					 const struct ath12k_dp_tx_ext_mon_config *old,
					 enum dp_mon_tx_filter_mode old_mode)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	struct ath12k *ar = dp_pdev->ar;

	if (old->enable) {
		struct ath12k_ext_mon_filter_config restore = {
			.level         = old->level,
			.monitor_flags = old->monitor_flags,
			.all_peer      = old->fp,
			.target_peer   = old->fpmo,
			.meta_data     = old->metadata,
		};
		ath12k_dp_ext_mon_tx_set_rtap_mode(dp_pdev, restore.meta_data);
		if (!ath12k_dp_ext_mon_update_tx_config(dp_mon_pdev, &restore) &&
		    !ath12k_dp_mon_tx_update_send_filter(dp_pdev, tx_mon, old_mode))
			return;
		ath12k_warn(dp_pdev->dp->ab, "TX Mon: old config restore failed\n");
	}

	if (!ath12k_dp_mon_tx_monitor_start_stop(ar, true))
		return;

	dp_tx_mon_disable_full_tx_mon(dp_pdev, ar);
}

static int
ath12k_dp_ext_mon_reconfigure_tx_peers(struct ath12k_pdev_dp *dp_pdev,
				       const struct ath12k_dp_tx_ext_mon_config
				       *old, const struct ath12k_ext_mon_filter_config
				       *new_config)
{
	int ret = 0;
	struct ath12k_ext_mon_peer_config *peer_config;
	u8 old_filter_mode = (old->fp_enabled << 1) | old->fpmo_enabled;
	bool fp_enabled =
		ath12k_dp_ext_mon_is_mode_enabled(&new_config->all_peer);
	bool fpmo_enabled =
		ath12k_dp_ext_mon_is_mode_enabled(&new_config->target_peer);
	u8 new_filter_mode = (fp_enabled << 1) | fpmo_enabled;

	if (old_filter_mode == new_filter_mode)
		return 0;

	peer_config = kzalloc(sizeof(*peer_config), GFP_KERNEL);
	if (!peer_config)
		return -ENOMEM;

	peer_config->count = old->peer_count;
	ret = ath12k_dp_ext_mon_tx_remove_all_peers(dp_pdev);
	if (ret) {
		kfree(peer_config);
		return ret;
	}

	ret = ath12k_dp_ext_mon_add_tx_peers(dp_pdev, true, peer_config);

	kfree(peer_config);

	return ret;
}

int ath12k_dp_ext_mon_set_tx_filter(struct ath12k_pdev_dp *dp_pdev,
				    const struct ath12k_ext_mon_filter_config *new_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_dp_tx_ext_mon_config old;
	enum dp_mon_tx_filter_mode old_mode;
	enum dp_mon_tx_filter_mode new_mode = DP_MON_TX_FULL_MONITOR;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp->ab, "monitor pdev is null\n");
		return -EINVAL;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	if (unlikely(!tx_mon)) {
		ath12k_warn(dp_pdev->dp->ab, "tx mon pdev is null\n");
		return -EINVAL;
	}

	/* S1: TX mon not running */
	if (!tx_mon->tx_monitor_started) {
		ath12k_warn(dp_pdev->dp->ab, "TX Mon: enable TX monitor first\n");
		return -EINVAL;
	}

	tx_ext_mon = &tx_mon->tx_ext_mon;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_config = tx_ext_mon->tx_ext_mon_config;
	if (unlikely(!tx_config)) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp->ab, "ext_mon TX config is null\n");
		return -EINVAL;
	}

	/* S2: already disabled, disable request — not honoured */
	if (new_config->disable && !tx_config->enable) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp->ab, "TX Mon: ext-mon already disabled\n");
		return -EINVAL;
	}

	old.enable        = tx_config->enable;
	old.level         = tx_config->level;
	old.monitor_flags = tx_config->monitor_flags;
	old.metadata      = tx_config->metadata;
	old.fp            = tx_config->fp;
	old.fpmo          = tx_config->fpmo;
	old_mode          = tx_mon->tx_monitor_mode;
	old.fp_enabled    = tx_config->fp_enabled;
	old.fpmo_enabled  = tx_config->fpmo_enabled;
	old.peer_count    = tx_config->peer_count;
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);

	/* S3: disable — restore full monitor */
	if (new_config->disable) {
		ret = ath12k_dp_mon_tx_monitor_start_stop(ar, true);
		if (ret) {
			ath12k_warn(dp_pdev->dp->ab,
				    "TX Mon: full monitor restore failed: %d\n", ret);
			dp_tx_mon_disable_full_tx_mon(dp_pdev, ar);
		}
		return ret;
	}

	ret = ath12k_dp_ext_mon_update_tx_config(dp_mon_pdev, new_config);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: config update failed: %d\n", ret);
		return ret;
	}

	/* S4/S5: enable or reconfigure */
	switch (new_config->monitor_flags) {
	case ATH12K_EXT_MON_DEFAULT:
		new_mode = DP_MON_TX_FILTER_EXT_MON_MODE;
		break;
	case ATH12K_EXT_MON_PKT_CAP:
		new_mode = DP_MON_TX_FILTER_SPL_PKT_CAP;
		break;
	default:
		ath12k_warn(dp_pdev->dp->ab, "TX Mon: invalid monitor_flags %u\n",
			    new_config->monitor_flags);
		return -EINVAL;
	}

	ret = ath12k_dp_mon_tx_update_send_filter(dp_pdev, tx_mon, new_mode);
	if (ret) {
		ath12k_warn(dp_pdev->dp->ab,
			    "TX Mon: filter apply failed for mode %u: %d\n",
			    new_mode, ret);
		ath12k_dp_ext_mon_tx_recover(dp_pdev, &old, old_mode);
		return ret;
	}

	ath12k_dp_ext_mon_tx_set_rtap_mode(dp_pdev, new_config->meta_data);
	ret = ath12k_dp_ext_mon_reconfigure_tx_peers(dp_pdev, &old, new_config);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_set_tx_filter);

enum ath12k_dp_mon_tx_dma_length
ath12k_dp_mon_tx_get_ext_mon_filter_len(u8 filter_len)
{
	switch (filter_len) {
	case ATH12K_EXT_MON_LEN_64B:
		return ATH12K_DP_MON_TX_DMA_LENGTH_64B;
	case ATH12K_EXT_MON_LEN_128B:
		return ATH12K_DP_MON_TX_DMA_LENGTH_128B;
	case ATH12K_EXT_MON_LEN_256B:
		return ATH12K_DP_MON_TX_DMA_LENGTH_256B;
	case ATH12K_EXT_MON_LEN_FULL_PKT:
		return ATH12K_DP_MON_TX_DMA_LENGTH_MAX;
	default:
		return ATH12K_DP_MON_TX_DMA_LENGTH_MAX;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_tx_get_ext_mon_filter_len);

void
ath12k_dp_tx_mon_set_tx_mon_teardown(struct ath12k *ar)
{
	if (ar->dp.dp_mon_pdev && ar->dp.dp_mon_pdev->dp_pdev_tx_mon)
		ar->dp.dp_mon_pdev->dp_pdev_tx_mon->tx_mon_teardown = true;
}

int
ath12k_dp_ext_mon_filter_peer(struct ieee80211_hdr *wh, struct list_head *peer_list)
{
	struct ath12k_dp_ext_mon_peer *peer;

	list_for_each_entry(peer, peer_list, list) {
		if (ether_addr_equal(peer->peer_info.mac_addr, wh->addr1))
			return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_filter_peer);

int
ath12k_dp_ext_mon_filter_type(struct ieee80211_hdr *wh,
			      struct ath12k_ext_mon_pkt_config *pkt_config)
{
	u16 type;

	type = ((__le16_to_cpu(wh->frame_control) & IEEE80211_FCTL_FTYPE) >>
		ATH12K_FC0_TYPE_SHIFT);

	if (type < ATH12K_EXT_MON_FRAME_MAX && pkt_config->filter[type])
		return 0;

	return -EINVAL;
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_filter_type);

int
ath12k_dp_ext_mon_filter_subtype(struct ieee80211_hdr *wh,
				 struct ath12k_ext_mon_pkt_config *pkt_config)
{
	bool is_mcast = false;
	u16 type, sub_type;

	type = ((__le16_to_cpu(wh->frame_control) & IEEE80211_FCTL_FTYPE) >>
		ATH12K_FC0_TYPE_SHIFT);
	sub_type = ((__le16_to_cpu(wh->frame_control) & IEEE80211_FCTL_STYPE) >>
		    ATH12K_FC0_SUBTYPE_SHIFT);
	is_mcast = is_multicast_ether_addr(wh->addr1);

	if (type >= ATH12K_EXT_MON_FRAME_MAX)
		return -EINVAL;

	return ath12k_dp_ext_mon_subtype_check(pkt_config, type, sub_type,
					       is_mcast);
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_filter_subtype);
