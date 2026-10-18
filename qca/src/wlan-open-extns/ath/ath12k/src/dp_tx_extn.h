/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DP_TX_EXTN_H
#define DP_TX_EXTN_H

#include "../core.h"
#include "../dp.h"
#include "../wifi7/dp_tx.h"

/* Number of TX descriptors moved from the regular pool (under lock) into the
 * per-ring hot list in a single refill operation.
 */
#define ATH12K_DP_TX_HOT_DESC_BATCH_SIZE	128

struct ath12k_dp_tx_msdu_info;
struct ath12k_dp_skb_ctrl;
struct ath12k_tx_desc_info;

bool ath12k_dp_tx_dma_map(struct ath12k_dp *dp, struct sk_buff *skb, u32 len,
			  struct ath12k_tx_desc_info *tx_desc,
			  struct ath12k_dp_tx_msdu_info *msdu_info,
			  struct ath12k_dp_skb_ctrl *skb_ctrl);

void ath12k_dp_tx_buffer_unmap(struct device *dev, dma_addr_t dma_handle, size_t size,
			       enum dma_data_direction direction);

static inline void ath12k_dmb(void)
{
	dmb(oshst);
}

void ath12k_dp_tx_hw_group_extn_init(struct ath12k_dp_hw_group *dp_hw_grp);
/**
 * ath12k_dp_tx_assign_buffer_hot() - Assign a TX descriptor from the hot list
 * @dp_hw_grp: HW group containing the per-ring hot and free descriptor lists
 * @ring_id:   TX ring index (pool_id)
 *
 * Fast-path descriptor allocation for the DP_SKB_FAST_TX path.  Tries to
 * dequeue from the per-ring tx_hot_desc_list without taking a spinlock.
 * When the hot list is empty it refills it with up to
 * ATH12K_DP_TX_HOT_DESC_BATCH_SIZE entries from tx_desc_free_list under
 * tx_desc_lock, then retries.
 *
 * Return: pointer to a free ath12k_tx_desc_info, or NULL if the pool is
 *         exhausted.
 */
struct ath12k_tx_desc_info *
ath12k_dp_tx_assign_buffer_hot(struct ath12k_dp_hw_group *dp_hw_grp, u8 ring_id);
#endif // DP_TX_EXTN_H
