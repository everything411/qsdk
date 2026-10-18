// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../dp.h"
#include "../dp_rx.h"
#include "umac_reset.h"
#include "dp_tx.h"

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "../ppe.h"
#endif

/**
 * ath12k_wifi7_clear_link_desc_pool_task - Task to clear link desc pool
 * @ab: Pointer to ath12k_base structure
 *
 * This task clears the WBM link descriptor pool for a specific AB.
 * Multiple instances of this task run in parallel (one per AB in the group).
 */
static void ath12k_wifi7_clear_link_desc_pool_task(struct ath12k_base *ab)
{
	/* Clear link desc pool for this AB */
	ath12k_dp_clear_link_desc_pool(ath12k_ab_to_dp(ab));
}

/**
 * ath12k_wifi7_post_pre_reset_send_cb - Callback after pre_reset message sent
 * @ab: Pointer to ath12k_base structure
 *
 * This callback executes immediately after the pre_reset HTT message is
 * successfully sent to firmware. It enqueues tasks to clear the WBM link
 * descriptor pool for all ABs in the hardware group, then triggers SMP
 * calls to schedule tasklets on all online CPUs for parallel processing.
 */
static void ath12k_wifi7_post_pre_reset_send_cb(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	unsigned long flags;

	/* Enqueue clear_link_desc_pool task for the current ab */
	if (ab->is_bypassed || test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
		return;

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);
	/*
	 * Mark bit 0 to avoid premature response to fw. Do this only
	 * when post reset processing hasnt already started.
	 */
	if (bitmap_empty(&mlo_umac_reset->task_map, BITS_PER_LONG))
		set_bit(0, &mlo_umac_reset->task_map);

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	/* Enqueue unbound task - any CPU can process it */
	ath12k_umac_reset_enqueue_task(ag,
				       ath12k_wifi7_clear_link_desc_pool_task,
				       ab,
				       ATH12K_UMAC_RESET_DO_PRE_RESET,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);

	/* Trigger SMP calls to schedule tasklets on all online CPUs.
	 * This allows parallel processing of the clear_link_desc_pool tasks
	 * while FW processes the pre_reset message.
	 */
	ath12k_umac_reset_schedule_all_tasklets(ag);
}

static void ath12k_wifi7_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	set_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);
	ath12k_hif_mgmt_irq_disable(ab);

	ath12k_hif_irq_disable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ath12k_dp_ppeds_service_enable_disable(ab, true);
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop(ab);
		ab->dp->ppe.ppe_ops->ath12k_ppeds_stop(ab);
		ath12k_dp_ppeds_service_enable_disable(ab, false);
	}
#endif

	/* Set callback to clear link desc pool after pre_reset message is sent.
	 * This ensures the memset happens after successful message send and
	 * overlaps with FW processing time.
	 */
	ath12k_umac_reset_set_post_send_cb(ab, ath12k_wifi7_post_pre_reset_send_cb);
}

void ath12k_wifi7_umac_reset_handle_pre_reset_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int cpu;

	ath12k_wifi7_umac_reset_handle_pre_reset(ab);

	/* Schedule dummy tasks on all online CPUs to ensure
	 * no ath12k_wifi7_dp_service_srng instances are running.
	 * Each task is bound to a specific CPU, and only after all
	 * CPU-bound tasks complete will pre_reset_done be sent.
	 */

	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	for_each_online_cpu(cpu) {
		ath12k_umac_reset_enqueue_task(ag,
					       ath12k_dummy_pre_reset_callback,
					       ab,
					       ATH12K_UMAC_RESET_DO_PRE_RESET,
					       ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
					       cpu);  /* Bind to specific CPU */
	}
}

static void ath12k_wifi7_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_dp *dp;
	int i, n_link_desc, ret;
	struct hal_srng *srng = NULL;
	unsigned long end;
	bool is_initiator_chip = (mlo_umac_reset->initiator_chip == ab->device_id);

	ath12k_dp_srng_hw_ring_disable(ab);

	/* Busy wait for 2 ms to make sure the rings are
	 * in idle state before enabling it
	 */
	end = jiffies + msecs_to_jiffies(2);
	while (time_before(jiffies, end))
		;

	ret = ath12k_wbm_idle_ring_init(ab);
	if (ret)
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);

	dp = ath12k_ab_to_dp(ab);
	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];
	n_link_desc = dp->wbm_idle_ring.num_entries;

	ret = ath12k_dp_link_desc_init(ab, dp->link_desc_banks,
				       HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret)
		ath12k_warn(ab, "failed to setup link desc: %d\n", ret);

	ath12k_dp_srng_common_init(ab);
	ath12k_wifi7_dp_tx_ring_init(ab);

	ret = ath12k_dp_srng_init(ab,
				  &dp->rx_refill_buf_ring.refill_buf_ring,
				  HAL_RXDMA_BUF, 0, 0);

	if (ret)
		ath12k_warn(ab, "failed to setup rx_refill_buf_ring\n");

	if (is_initiator_chip)
		ath12k_dp_umac_tx_desc_cleanup(ab);

	ath12k_dp_umac_rx_desc_cleanup(ab);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) && is_initiator_chip)
		ath12k_dp_ppeds_tx_desc_cleanup(ab);
#endif

	ret = ath12k_dp_srng_init(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				  HAL_WBM2SW_REL_ERR_RING_NUM, 0);
	if (ret)
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);

	for (i = 0; i < ATH12K_DP_RX_REGULAR_RING_MAX; i++) {
		ret = ath12k_dp_srng_init(ab, &dp->reo_dst_ring[i],
					  HAL_REO_DST, i, 0);
		if (ret)
			ath12k_warn(ab, "failed to setup reo_dst_ring\n");
	}

	ath12k_dp_rx_reo_cmd_list_cleanup(ab);

	ath12k_dp_tid_cleanup(ab);

	/* Reprogram PCP-TID map and TID map precedence after UMAC reset */
	ath12k_hal_tx_set_pcp_tid_map(ab, ab->ag->dp_hw_grp->pcp_tid_map);
	ath12k_hal_tx_set_tid_map_precedence(ab,
					     ab->ag->dp_hw_grp->tid_map_precedence);
}

void ath12k_wifi7_umac_reset_handle_post_reset_start_wrapper(struct ath12k_base *ab)
{
	/* Enqueue post_reset_start task bound to the same CPU that did the clear */
	ath12k_umac_reset_enqueue_task(ab->ag,
				       ath12k_wifi7_umac_reset_handle_post_reset_start,
				       ab,
				       ATH12K_UMAC_RESET_DO_POST_RESET_START,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);
}

/**
 * ath12k_wifi7_post_reset_task - Free saved SKBs, replenish rx refill ring
 * @ab: Pointer to ath12k_base structure
 *
 * This task frees all saved TX and RX SKBs for a specific AB.
 * Multiple instances of this task run in parallel (one per AB in the group).
 * And also replenishes rx refill ring
 */
static void ath12k_wifi7_post_reset_task(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct sk_buff *skb;

	/* Free all saved TX SKBs */
	while ((skb = skb_dequeue(&umac_reset->tx_skb_queue)) != NULL)
		dev_kfree_skb_any(skb);

	/* Free all saved RX SKBs */
	while ((skb = skb_dequeue(&umac_reset->rx_skb_queue)) != NULL)
		dev_kfree_skb_any(skb);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Free all saved TX SKBs */
	while ((skb = skb_dequeue(&umac_reset->ppeds_tx_skb_queue)) != NULL)
		dev_kfree_skb_any(skb);
#endif

	/* If ring was not replenished completely due to insufficient
	 * rx descs in use during umac reset, we take care of replenishing
	 * the rest of the ring here
	 */
	ath12k_dp_rxdma_buf_setup(ab);
}

static void ath12k_wifi7_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	ath12k_hif_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
		ab->dp->ppe.ppe_ops->ath12k_ppeds_start(ab);
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start(ab);
	}
#endif
	ath12k_hif_mgmt_irq_enable(ab);
	clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

	/* Set callback to free saved SKBs after post_reset_complete message is sent.
	 * This ensures the SKB freeing happens after successful message send and
	 * is distributed across all CPUs for parallel processing.
	 */
	ath12k_umac_reset_set_post_send_cb(ab, ath12k_wifi7_post_reset_task);
}

void ath12k_wifi7_umac_reset_handle_post_reset_complete_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	ath12k_umac_reset_enqueue_task(ag,
				       ath12k_wifi7_umac_reset_handle_post_reset_complete,
				       ab,
				       ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);
}
