// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal.h"
#include "../core.h"
#include "../dp.h"
#include "umac_reset.h"
#include "hal_queue.h"
#include "dp.h"
#include "mgmt_rx.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "dp_telemetry.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "../ppe.h"
#include "ppeds.h"
#endif


void ath12k_wifi8_umac_reset_ppeds_rx_rings_disable(struct ath12k_base *cumac_ab);

/**
 * ath12k_wifi8_clear_link_desc_pool_task - Task to clear link desc pool
 * @ab: Pointer to ath12k_base structure
 *
 * This task clears the WBM link descriptor pool for a specific AB.
 * Multiple instances of this task run in parallel (one per AB in the group).
 */
static void ath12k_wifi8_clear_link_desc_pool_task(struct ath12k_base *ab)
{
	/* Clear link desc pool for this AB */
	ath12k_dp_clear_link_desc_pool(ath12k_ab_to_dp(ab));
}

static void ath12k_q_post_reset_task(struct ath12k_base *ab,
				     umac_reset_handler_fn callback)
{
	struct ath12k_hw_group *ag = ab->ag;

	ath12k_umac_reset_enqueue_task(ag, callback, ab,
				       ATH12K_UMAC_RESET_DO_PRE_RESET,
				       ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
				       ATH12K_UMAC_RESET_CPU_UNBOUND);
}

static void ath12k_umac_reset_cleanup_tx_queues(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	int i, j, ret;

	/* Cleanup TX queues for all peers after UMAC reset */
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;

			if (!ar)
				continue;

			ret = ath12k_wifi8_cleanup_all_peers_tx_queues(&ar->ah->dp_hw,
								       ag->dp_hw_grp,
								       &ar->dp);
			if (ret)
				ath12k_warn(ab,
					    "Failed to cleanup TX queues for all peers: %d\n",
					    ret);
			else
				ath12k_dbg(ab, ATH12K_DBG_BOOT,
					   "Successfully cleaned up TX queues for all peers\n");
		}
	}
}

void ath12k_wifi8_umac_reset_handle_init_recovery(struct ath12k_base *ab)
{
	struct ath12k_base *cumac_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->ag->dp_hw_grp);

	/* Pause TX during UMAC reset */
	set_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

	/* Set umac in recovery flag for soc under Q6 only reset */
	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &cumac_ab->dev_flags))
		set_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &cumac_ab->dev_flags);
}

static void ath12k_wifi8_umac_reset_refill_rings_deinit(struct ath12k_base *ab)
{
	int i;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	for (i = 0 ; i < DP_WBM_REFILL_RING_MAX; i++)
		ath12k_dp_srng_hw_disable(ab, &dp_wifi8->wbm_refill_ring[i]);

	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->wbm_idle_buf_ring);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
/**
 * ath12k_wifi8_ppe2wbm_ring_disable
 * @ab: Pointer to ath12k_base structure
 *
 */
static void ath12k_wifi8_ppe2wbm_ring_disable(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	uint32_t i;

	for (i = 0 ; i < dp_wifi8->num_ppe2wbm_refill_rings; i++) {
		ath12k_dp_srng_hw_disable(ab, &dp_wifi8->ppe2wbm_refill_ring[i]);
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
				"PPE2WBM[%d]REFILL:%p cumac=%d\n",
				i, &dp_wifi8->ppe2wbm_refill_ring[i], ab->is_cumac_chip);
	}

	if (dp_wifi8->dp_ppe2wbm_use_dedicated_pool) {
		ath12k_dp_srng_hw_disable(ab, &dp_wifi8->ppe2wbm_idle_buf_ring);
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
				"PPE2WBM IDLE_BUF:%p cumac=%d\n",
				&dp_wifi8->ppe2wbm_idle_buf_ring, ab->is_cumac_chip);
	}
}
#endif

/**
 * ath12k_wifi8_post_pre_reset_send_cb - Callback after pre_reset message sent
 * @ab: Pointer to ath12k_base structure
 *
 * This callback executes immediately after the pre_reset HTT message is
 * successfully sent to firmware. It enqueues tasks, then triggers SMP
 * calls to schedule tasklets on all online CPUs for parallel processing.
 */
static void ath12k_wifi8_post_pre_reset_send_cb(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	unsigned long flags, end;

	/* Enqueue clear_link_desc_pool task for the current ab */
	if (ab->is_bypassed || (test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
	    !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags)))
		return;

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);
	/*
	 * Mark bit 0 to avoid premature response to fw. Do this only
	 * when post reset processing hasnt already started.
	 */
	if (bitmap_empty(&mlo_umac_reset->task_map, BITS_PER_LONG))
		set_bit(0, &mlo_umac_reset->task_map);

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);


#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_ppe2wbm_ring_disable(ab);
#endif
	ath12k_wifi8_umac_reset_refill_rings_deinit(ab);
	ath12k_wifi8_mgmt_refill_rings_deinit(ab);

	end = jiffies + msecs_to_jiffies(2);
	while (time_before(jiffies, end))
		;

	ath12k_wifi8_dp_rx_wbm_srng_init(ab);
	ath12k_wifi8_mgmt_rx_refill_ring_setup(ab);

	/* Enqueue unbound tasks - any CPU can process it */
	ath12k_q_post_reset_task(ab, ath12k_wifi8_clear_link_desc_pool_task);
	ath12k_q_post_reset_task(ab, ath12k_dp_umac_tx_desc_cleanup);
	ath12k_q_post_reset_task(ab, ath12k_dp_rx_reo_cmd_list_cleanup);
	ath12k_q_post_reset_task(ab, ath12k_wifi8_dp_tx_tqm_cmd_list_cleanup);
	ath12k_q_post_reset_task(ab, ath12k_umac_reset_cleanup_tx_queues);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		ath12k_q_post_reset_task(ab, ath12k_dp_ppeds_tx_desc_cleanup);
#endif

	/* Trigger SMP calls to schedule tasklets on all online CPUs.
	 * This allows parallel processing of the clear_link_desc_pool tasks
	 * while FW processes the pre_reset message.
	 */
	ath12k_umac_reset_schedule_all_tasklets(ag);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
/**
 * ath12k_wifi8_umac_reset_ppeds_stop
 * @ab: Pointer to ath12k_base structure
 *
 */
static void ath12k_wifi8_umac_reset_ppeds_stop(struct ath12k_base *cumac_ab)
{
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &cumac_ab->dev_flags)) {
		ath12k_dp_ppeds_service_enable_disable(cumac_ab, true);
		cumac_ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop(cumac_ab);
		cumac_ab->dp->ppe.ppe_ops->ath12k_ppeds_stop(cumac_ab);
		ath12k_dp_ppeds_service_enable_disable(cumac_ab, false);
		ath12k_dbg(cumac_ab, ATH12K_DBG_DP_UMAC_RESET,
				"PPEDS UMAC RESET INST STOP DONE cumac=%p\n", cumac_ab);
	}
}
#endif

void ath12k_wifi8_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_base *cumac_ab;
	int cpu;
	int ret;

	mlo_umac_reset = &ag->mlo_umac_reset;

	ath12k_hif_irq_disable(ab);
	ath12k_hif_mgmt_irq_disable(ab);

	/* Handle only once */
	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &cumac_ab->dev_flags) &&
	    !cumac_ab->skip_cumac_hw_reset) {
		ret = ath12k_cumac_hw_pre_reset(cumac_ab);
		if (ret) {
			ath12k_err(ab, "CUMAC HW pre reset failed");
			return;
		}
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_umac_reset_ppeds_rx_rings_disable(cumac_ab);
	ath12k_wifi8_umac_reset_ppeds_stop(cumac_ab);
#endif
	ath12k_umac_reset_set_post_send_cb(cumac_ab,
					   ath12k_wifi8_post_pre_reset_send_cb);

	/* Schedule dummy tasks on all online CPUs to ensure
	 * no ath12k_wifi8_dp_service_srng instances are running.
	 * Each task is bound to a specific CPU, and only after all
	 * CPU-bound tasks complete will pre_reset_done be sent.
	 */

	for_each_online_cpu(cpu) {
		ath12k_umac_reset_enqueue_task(ag,
					       ath12k_dummy_pre_reset_callback,
					       ab,
					       ATH12K_UMAC_RESET_DO_PRE_RESET,
					       ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
					       cpu);
	}
}

void ath12k_wifi8_dp_rx_init(struct ath12k_base *ab)
{
	ath12k_wifi8_dp_rx_ring_init(ab);
	ath12k_dp_umac_rx_desc_cleanup(ab);
	ath12k_wifi8_dp_rx_wbm_buf_ring_init(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_dp_rx_ppe2wbm_idle_buff_init(ab);
#endif
}

void ath12k_wifi8_dp_rx_mgmt_init(struct ath12k_base *ab)
{
	ath12k_wifi8_mgmt_rx_ring_setup(ab);
	ath12k_mgmt_rx_desc_cleanup(ab);
	ath12k_wifi8_mgmt_rx_refill_ring_init(ab);
}

void ath12k_wifi8_dp_wbm_idle_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct hal_srng *srng;
	int n_link_desc, ret;

	ret = ath12k_wbm_idle_ring_init(ab);
	if (ret)
		ath12k_warn(ab, "failed to init wbm_idle_ring: %d\n", ret);

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];
	n_link_desc = dp->wbm_idle_ring.num_entries;

	if (ath12k_dp_link_desc_init(ab, dp->link_desc_banks,
				     HAL_WBM_IDLE_LINK, srng, n_link_desc))
		ath12k_warn(ab, "failed to init link desc: %d\n", ret);
}

static void ath12k_dp_srng_common_init_wrapper(struct ath12k_base *ab)
{
	ath12k_dp_srng_common_init(ab);
}

static void ath12k_wifi8_dp_tx_ring_init_wrapper(struct ath12k_base *ab)
{
	ath12k_wifi8_dp_tx_ring_init(ab);
}

static void ath12k_wifi8_dp_telemetry_umac_setup_wrapper(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wifi8_dp_telemetry_umac_init(ab);
	if (ret)
		ath12k_warn(ab, "failed to init telemetry config: %d\n", ret);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_umac_reset_ppeds_rx_rings_disable(struct ath12k_base *cumac_ab)
{
	uint32_t ring_idx;

	/*
	 * Disabling REO2PPE/TQM2PPE SRNG.
	 */
	for (ring_idx = 0; ring_idx < ath12k_ppeds_reo2ppe_rings_max; ring_idx++)
		ath12k_dp_srng_hw_disable(cumac_ab,
				&cumac_ab->dp->ppe.reo2ppe_ring[ring_idx]);

	if (cumac_ab->dp->ppe.hw_buff_mgmt)
		ath12k_dp_srng_hw_disable(cumac_ab,
			&cumac_ab->dp->ppe.tqm2ppe_txcmp_ring);
}

static void ath12k_wifi8_umac_reset_ppeds_tx_rings_disable(struct ath12k_base *cumac_ab)
{
	uint32_t ring_idx;

	/*
	 * Disabling PPE2TCL.
	 */
	for (ring_idx = 0; ring_idx < ath12k_ppeds_ppe2tcl_rings_max; ring_idx++)
		ath12k_dp_srng_hw_disable(cumac_ab,
				&cumac_ab->dp->ppe.ppe2tcl_ring[ring_idx]);
}

static void ath12k_wifi8_umac_reset_ppeds_srng_init(struct ath12k_base *cumac_ab)
{
	int ret;

	ath12k_dbg(cumac_ab, ATH12K_DBG_DP_UMAC_RESET, "cab=%p device_id=%d\n",
		   cumac_ab, cumac_ab ? cumac_ab->device_id : -1);
	ret = ath12k_wifi8_dp_srng_ppeds_init(cumac_ab);
	if (ret)
		ath12k_warn(cumac_ab, "failed to init ppe-ds srngs :%d\n", ret);

	ath12k_dbg(cumac_ab, ATH12K_DBG_DP_UMAC_RESET,
		   "PPEDS UMAC_RESET SRNG INIT DONE cumac=%p\n", cumac_ab);
}
#endif

static void ath12k_wifi8_pcp_tid_map_reprogram_wrapper(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	/* Reprogram PCP-TID map and TID map precedence after UMAC reset */
	ath12k_dp_pcp_tid_map(ag->dp_hw_grp);
	ath12k_dp_tid_map_precedence(ag->dp_hw_grp);
}

void ath12k_wifi8_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *cumac_ab;
	struct ath12k_base *partner_ab;
	unsigned long end;
	int i, ret;

	if (!ag)
		return;

	mlo_umac_reset = &ag->mlo_umac_reset;

	/* Handle only once */
	if (mlo_umac_reset->initiator_chip != ab->device_id)
		return;

	cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &cumac_ab->dev_flags) &&
	    !cumac_ab->skip_cumac_hw_reset) {
		ret = ath12k_cumac_hw_reset(cumac_ab);
		if (ret) {
			ath12k_err(ab, "CUMAC HW reset failed");
			return;
		}
	}

	ath12k_dp_srng_hw_ring_disable(cumac_ab);
	ath12k_wifi8_srng_hw_ring_disable(cumac_ab);
	ath12k_wifi8_srng_hw_mgmt_rings_disable(cumac_ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_umac_reset_ppeds_tx_rings_disable(cumac_ab);
#endif

	end = jiffies + msecs_to_jiffies(2);

	while (time_before(jiffies, end))
		;

	ath12k_wifi8_dp_rx_init(cumac_ab);

	ath12k_q_post_reset_task(cumac_ab, ath12k_dp_srng_common_init_wrapper);
	ath12k_q_post_reset_task(cumac_ab, ath12k_wifi8_dp_tx_ring_init_wrapper);
	ath12k_q_post_reset_task(cumac_ab, ath12k_wifi8_dp_wbm_idle_init);
	ath12k_q_post_reset_task(cumac_ab, ath12k_wifi8_dp_rx_mgmt_init);
	ath12k_q_post_reset_task(cumac_ab, ath12k_wifi8_clean_pending_ast_entries);
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab)
			continue;
		ath12k_q_post_reset_task(partner_ab, ath12k_dp_tid_cleanup);
	}
	/* Telemetry UMAC setup must run after ring setup tasks are queued. */
	ath12k_q_post_reset_task(cumac_ab,
				 ath12k_wifi8_dp_telemetry_umac_setup_wrapper);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_umac_reset_ppeds_srng_init(cumac_ab);
#endif
	/* Reprogram PCP-TID map and TID map precedence as the last queued task */
	ath12k_q_post_reset_task(cumac_ab,
				 ath12k_wifi8_pcp_tid_map_reprogram_wrapper);
}

/**
 * ath12k_wifi8_post_reset_task - Free saved SKBs, replenish rx refill ring
 * @ab: Pointer to ath12k_base structure
 *
 * This task frees all saved TX and RX SKBs for a specific AB.
 * Multiple instances of this task run in parallel (one per AB in the group).
 * And also replenishes rx refill ring
 */
static void ath12k_wifi8_post_reset_task(struct ath12k_base *ab)
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
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static void ath12k_wifi8_umac_reset_ppeds_start(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);

	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &cumac_ab->dev_flags) &&
		ab->is_cumac_chip) {
		cumac_ab->dp->ppe.ppe_ops->ath12k_ppeds_start(cumac_ab);
		cumac_ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start(cumac_ab);
		ath12k_dbg(cumac_ab, ATH12K_DBG_DP_UMAC_RESET,
				"PPEDS UMAC_RESET INST START DONE cumac=%p\n",
				cumac_ab);
	}
}
#endif

void ath12k_wifi8_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *cumac_ab = ath12k_dp_get_ab_from_dp_hw_group(ag->dp_hw_grp);
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int ret;

	/* Handle only once */
	if (mlo_umac_reset->initiator_chip == ab->device_id &&
	    test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &cumac_ab->dev_flags) &&
	    !cumac_ab->skip_cumac_hw_reset) {
		ret = ath12k_cumac_hw_post_reset(cumac_ab);
		if (ret) {
			ath12k_err(ab, "CUMAC HW post reset failed");
			return;
		}

		/* Reprogram PCP-TID map and TID map precedence after CUMAC HW reset
		 * (Mode 2 / Q6 BCR reset).
		 */
		ath12k_dp_pcp_tid_map(ab->ag->dp_hw_grp);
		ath12k_dp_tid_map_precedence(ab->ag->dp_hw_grp);
		ath12k_hif_irq_enable(cumac_ab);
		ath12k_hif_mgmt_irq_enable(cumac_ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_wifi8_umac_reset_ppeds_start(cumac_ab);
#endif
		clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &cumac_ab->dev_flags);
	}

	ath12k_hif_irq_enable(ab);
	ath12k_hif_mgmt_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_wifi8_umac_reset_ppeds_start(ab);
#endif

	/* Resume TX during UMAC reset */
	clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

	ath12k_wifi8_post_reset_task(ab);
}

static int ath12k_check_txrx_idle(struct ath12k_base *ab, u32 address)
{
	int ret;

	ret = ath12k_hif_poll32(ab, address, HAL_TXRX_IDLE_CHECK_VALUE,
				HAL_TXRX_IDLE_CHECK_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret)
		ath12k_err(ab, "TXRX idle check timeout at address 0x%x value 0x%x",
			   address, ath12k_hif_read32(ab, address));

	return ret;
}

static int ath12k_check_read_write_pointer(struct ath12k_base *ab, u32 mxi_base)
{
	u32 wr_cmd_fifo_wr_ptr, wr_cmd_fifo_rd_ptr;
	u32 rd_cmd_fifo_wr_ptr, rd_cmd_fifo_rd_ptr;
	int wait = HAL_MAC_IDLE_CHECK_TIMEOUT_USEC / HAL_MAC_IDLE_CHECK_DELAY_USEC;
	u32 fifo_dbg_sts = mxi_base + HAL_MXI_CMD_FIFO_DBG_STS;

	for ( ; wait != 0; wait--) {
		wr_cmd_fifo_wr_ptr = ath12k_hif_read32_masked(ab, fifo_dbg_sts,
							      WR_CMD_FIFO_WR_MASK);
		wr_cmd_fifo_rd_ptr = ath12k_hif_read32_masked(ab, fifo_dbg_sts,
							      WR_CMD_FIFO_RD_MASK);
		rd_cmd_fifo_wr_ptr = ath12k_hif_read32_masked(ab, fifo_dbg_sts,
							      RD_CMD_FIFO_WR_MASK);
		rd_cmd_fifo_rd_ptr = ath12k_hif_read32_masked(ab, fifo_dbg_sts,
							      RD_CMD_FIFO_RD_MASK);

		if (wr_cmd_fifo_wr_ptr == wr_cmd_fifo_rd_ptr &&
		    rd_cmd_fifo_wr_ptr == rd_cmd_fifo_rd_ptr)
			return 0;

		udelay(HAL_MAC_IDLE_CHECK_DELAY_USEC);
	}

	ath12k_err(ab, "MXI FIFO read/write pointer sync timeout (wr_wr:%u wr_rd:%u rd_wr:%u rd_rd:%u)",
		   wr_cmd_fifo_wr_ptr, wr_cmd_fifo_rd_ptr,
		   rd_cmd_fifo_wr_ptr, rd_cmd_fifo_rd_ptr);
	return -EINVAL;
}

static int ath12k_check_mxi_idle(struct ath12k_base *ab, u32 mxi_base)
{
	int ret;

	ret = ath12k_hif_poll32(ab, mxi_base + HAL_WMAC_GXI_SM_STATES_IX_0,
				HAL_MXI_SM_STATES_IDLE_VALUE,
				HAL_MXI_SM_STATES_IDLE_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "sm states idle check failed at address 0x%x value 0x%x",
			   mxi_base + HAL_WMAC_GXI_SM_STATES_IX_0,
			   ath12k_hif_read32(ab, mxi_base + HAL_WMAC_GXI_SM_STATES_IX_0));
		return ret;
	}

	ret = ath12k_hif_poll32(ab, mxi_base + HAL_GXI_WDOG_WARN_STATUS,
				HAL_GXI_WDOG_WARN_STATUS_VALUE,
				HAL_GXI_WDOG_WARN_STATUS_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "wdog warn status check failed at address 0x%x value 0x%x",
			   mxi_base + HAL_GXI_WDOG_WARN_STATUS,
			   ath12k_hif_read32(ab, mxi_base + HAL_GXI_WDOG_WARN_STATUS));
		return ret;
	}

	ret = ath12k_check_read_write_pointer(ab, mxi_base);
	if (ret) {
		ath12k_err(ab, "fifo read write pointer check failed");
		return ret;
	}

	return 0;
}

static int ath12k_check_pmac_idle(struct ath12k_base *ab)
{
	int i;
	int ret;

	int pmac_pmcmn_base[] = HAL_SEQ_WCSS_PMAC_PMCMN_REG;
	int pmac_mxi_base[] = HAL_SEQ_WCSS_PMAC_MXI_REG;

	for (i = 0; i < ab->num_radios; i++) {
		ret = ath12k_check_txrx_idle(ab, pmac_pmcmn_base[i] +
					     HAL_PMAC_PMCMN_MCMN_MAC_IDLE);
		if (ret) {
			ath12k_err(ab, "pmac txrx idle check failed");
			return ret;
		}

		ret = ath12k_check_mxi_idle(ab, pmac_mxi_base[i]);
		if (ret) {
			ath12k_err(ab, "pmxi idle check failed");
			return ret;
		}
	}

	return 0;
}

static int ath12k_check_dmac_cmn_idle(struct ath12k_base *ab, u32 dmcmn_base)
{
	int ret;

	ret = ath12k_hif_poll32(ab, dmcmn_base + HAL_DMAC_DMCMN_DMAC_IDLE_COMMON,
				HAL_DMAC_CMN_IDLE_VALUE,
				HAL_DMAC_CMN_IDLE_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret)
		ath12k_err(ab, "DMAC common idle check timeout at address 0x%x value 0x%x",
			   dmcmn_base + HAL_DMAC_DMCMN_DMAC_IDLE_COMMON,
			   ath12k_hif_read32(ab, dmcmn_base +
					     HAL_DMAC_DMCMN_DMAC_IDLE_COMMON));

	return ret;
}

static int ath12k_check_dmac_idle(struct ath12k_base *ab)
{
	int ret;

	int dmcmn_dmac_idle[] = HAL_DMAC_DMCMN_DMAC_IDLE;

	ret = ath12k_check_txrx_idle(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG +
				     dmcmn_dmac_idle[0]);
	if (ret) {
		ath12k_err(ab, "dmac0 txrx idle check failed");
		return ret;
	}

	ret = ath12k_check_txrx_idle(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG +
				     dmcmn_dmac_idle[1]);
	if (ret) {
		ath12k_err(ab, "dmac1 txrx idle check failed");
		return ret;
	}

	ret = ath12k_check_dmac_cmn_idle(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG);
	if (ret) {
		ath12k_err(ab, "dmac cmn check failed");
		return ret;
	}

	ret = ath12k_check_mxi_idle(ab, HAL_SEQ_WCSS_DMAC_MXI_REG);
	if (ret) {
		ath12k_err(ab, "dmxi idle check failed");
		return ret;
	}

	return 0;
}

static int ath12k_check_umac_reset_prerequisites(struct ath12k_base *ab, u32 arg)
{
	int ret;

	ret = ath12k_check_pmac_idle(ab);
	if (ret) {
		ath12k_err(ab, "pmac idle check failed");
		ATH12K_CUMAC_RESET_ERR_INC(ab, pmac_idle_fail);
	}

	ret = ath12k_check_dmac_idle(ab);
	if (ret) {
		ath12k_err(ab, "dmac idle check failed");
		ATH12K_CUMAC_RESET_ERR_INC(ab, dmac_idle_fail);
	}

	ret = ath12k_check_mxi_idle(ab, HAL_SEQ_WCSS_UMAC_MXI_REG);
	if (ret) {
		ath12k_err(ab, "umxi idle check failed");
		ATH12K_CUMAC_RESET_ERR_INC(ab, umac_idle_fail);
	}

	return 0;
}

static int ath12k_enable_rxdma_prefetch(struct ath12k_base *ab, u32 enable)
{
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_DMAC_RXDMA_REG + HAL_DMAC_RXDMA_GLOBAL_RER_CMN,
			 HAL_RXDMA_PREFETCH_MASK_ENABLE, enable);
	return 0;
}

static int ath12k_halt_mlo_doorbells(struct ath12k_base *ab, u32 is_halt)
{
	int i;

	int pmac_hwmlo_base[] = HAL_SEQ_WCSS_PMAC_HWMLO_REG;

	for (i = 0; i < ab->num_radios; i++) {
		ath12k_hif_rmw32(ab, pmac_hwmlo_base[i] + HAL_PMAC_HWMLO_WAR_OPTIONS,
				 HAL_HALT_DOORBELL_ACTIVITY_MASK, is_halt);
	}
	return 0;
}

static void ath12k_wsib_reset(struct ath12k_base *ab)
{
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_WFSS_CC_REG + HAL_WFSS_WCSS_MLO_WSI_CBCR,
			 HAL_WSI_RESET_MASK, 1);
	udelay(HAL_WSI_RESET_DELAY_USEC);
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_WFSS_CC_REG + HAL_WFSS_WCSS_MLO_WSI_CBCR,
			 HAL_WSI_RESET_MASK, 0);
	ath12k_hif_write32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG + HAL_MAC_WSIB_MASTER_SYNC, 1);
}

static int ath12k_pause_global_wsi(struct ath12k_base *ab, u32 pause)
{
	int ret;
	int value;

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG + HAL_MAC_WSIB_CFG,
			 HAL_WSIB_PAUSE_MASK, pause);

	if (pause) {
		ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG +
					HAL_MAC_WSIB_IDLE_STATUS,
					HAL_WSIB_IDLE_VALUE, HAL_WSIB_IDLE_MASK,
					HAL_WSIB_IDLE_CHECK_DELAY_USEC,
					HAL_WSIB_IDLE_CHECK_TIMEOUT_USEC);

		if (ret) {
			ath12k_info(ab, "WSI is not idle, address 0x%x value 0x%x proceed to reset",
				    HAL_SEQ_WCSS_MAC_WSIB_REG + HAL_MAC_WSIB_IDLE_STATUS,
				    ath12k_hif_read32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG +
						      HAL_MAC_WSIB_IDLE_STATUS));
			ATH12K_CUMAC_RESET_ERR_INC(ab, wsi_idle_fail);
			/* Read the value of CFG and write back after WSI reset */
			value = ath12k_hif_read32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG +
						  HAL_MAC_WSIB_CFG);
			ath12k_wsib_reset(ab);
			ath12k_hif_write32(ab, HAL_SEQ_WCSS_MAC_WSIB_REG +
					   HAL_MAC_WSIB_CFG, value);
		}
	}
	return 0;
}

static int ath12k_pmac_tx_flush(struct ath12k_base *ab, int pmac_hwsch_base,
				int flush_reason)
{
	int ret;

	ath12k_hif_write32(ab, pmac_hwsch_base + HAL_PMAC_HWSCH_FLUSH_TLV_MSG_CFG,
			   (flush_reason << HAL_TX_FLUSH_UCODE_MSG_SHIFT) |
			   HAL_FLUSH_TLV_FLUSH_CODE);

	ath12k_hif_write32(ab, pmac_hwsch_base +
			   HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_0,
			   HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_0_VALUE);

	ath12k_hif_write32(ab, pmac_hwsch_base +
			   HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_1,
			   HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_1_VALUE);

	ath12k_hif_write32(ab, pmac_hwsch_base + HAL_PMAC_HWSCH_FLUSH_STATUS, 0);

	ath12k_hif_write32(ab, pmac_hwsch_base + HAL_PMAC_HWSCH_SEND_FLUSH,
			   HAL_PMAC_HWSCH_FLUSH_CMD);

	ret = ath12k_hif_poll32(ab, pmac_hwsch_base + HAL_PMAC_HWSCH_FLUSH_STATUS,
				HAL_PMAC_HWSCH_FLUSH_STATUS_VALUE,
				HAL_PMAC_HWSCH_FLUSH_STATUS_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "PMAC: HWSCH flush timeout at address 0x%x value 0x%x\n",
			   pmac_hwsch_base + HAL_PMAC_HWSCH_FLUSH_STATUS,
			   ath12k_hif_read32(ab, pmac_hwsch_base +
					     HAL_PMAC_HWSCH_FLUSH_STATUS));
		ATH12K_CUMAC_RESET_ERR_INC(ab, hwsch_flush_fail);
		return ret;
	}

	ath12k_hif_rmw32(ab, pmac_hwsch_base + HAL_PMAC_HWSCH_CMD_MGR_GLB_CTRL_IX_0,
			 HAL_PMAC_HWSCH_HALT_ALL_SCH_CMD_RINGS, 1);

	return 0;
}

static int ath12k_pmac_tx_abort(struct ath12k_base *ab)
{
	int i, ret;
	int pmac_hwsch_base[] = HAL_SEQ_WCSS_PMAC_HWSCH_REG;
	int pmac_rxpcu_base[] = HAL_SEQ_WCSS_PMAC_RXPCU_REG;

	for (i = 0; i < ab->num_radios; i++) {
		ath12k_hif_rmw32(ab, pmac_rxpcu_base[i] + HAL_PMAC_RXPCU_SIFS_RESP_CTRL,
				 HAL_PMAC_RXPCU_UL_TRIGGER_EN_MASK, 1);

		ret = ath12k_pmac_tx_flush(ab, pmac_hwsch_base[i],
					   HAL_TX_FLUSH_REASON_HALT_TX);
		if (ret) {
			ath12k_err(ab, "pmac tx flush failed\n");
			return ret;
		}
	}

	return 0;
}

static int ath12k_pmac_rx_abort(struct ath12k_base *ab, int pmac_rxpcu_base,
				int pmac_pmcmn_base)
{
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_DMAC_RXOLE_REG +
			 HAL_DMAC_RXOLE_CLKGATE_DISABLE,
			 HAL_DMAC_RXOLE_CLKGATE_DISABLE_SFM_WRITE_MASK, 1);

	ath12k_hif_rmw32(ab, pmac_pmcmn_base + HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW,
				 HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW_HALT_RX_MASK, 1);

	ath12k_hif_rmw32(ab, pmac_rxpcu_base +
			 HAL_PMAC_RXPCU_RX_ERR_INJECTION_CFG,
			 HAL_PMAC_RXPCU_RX_ERR_INJECTION_CFG_AMPI_ERR_SEL_MASK, 0);

	ath12k_hif_rmw32(ab, pmac_rxpcu_base +
			 HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL,
			 HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL_SEND_MASK, 0);

	ath12k_hif_rmw32(ab, pmac_rxpcu_base +
			 HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL,
			 HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL_SEND_MASK, 1);

	return 0;
}

static int ath12k_pmac_rx_flush(struct ath12k_base *ab, int pmac_rxpcu_base, int isr)
{
	int ret;

	ath12k_hif_rmw32(ab, pmac_rxpcu_base + HAL_PMAC_RXPCU_RX_FLUSH_CTRL,
			 HAL_PMAC_RXPCU_RX_FLUSH_CTRL_RX_FLUSH_REQ, 1);

	ath12k_hif_rmw32(ab, pmac_rxpcu_base + HAL_PMAC_RXPCU_RX_FLUSH_CTRL,
			 HAL_PMAC_RXPCU_RX_FLUSH_CTRL_MIN_DURATION,
			 HAL_PMAC_RX_FLUSH_MIN_DURATION);

	ret = ath12k_hif_poll32(ab, pmac_rxpcu_base + HAL_PMAC_RXPCU_RX_FLUSH_CTRL,
				HAL_PMAC_RXPCU_RX_FLUSH_RX_FLUSH_ACK_VALUE,
				HAL_PMAC_RXPCU_RX_FLUSH_RX_FLUSH_ACK_MASK,
				HAL_RX_FLUSH_DELAY_USEC, HAL_RX_FLUSH_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "pmac rx flush failed at address 0x%x value 0x%x\n",
			   pmac_rxpcu_base + HAL_PMAC_RXPCU_RX_FLUSH_CTRL,
			   ath12k_hif_read32(ab, pmac_rxpcu_base +
					     HAL_PMAC_RXPCU_RX_FLUSH_CTRL));
		ATH12K_CUMAC_RESET_ERR_INC(ab, pmac_rx_flush_fail);
		return ret;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG + isr,
				HAL_DMAC_DMCMN_ISR_S24_VALUE, HAL_DMAC_DMCMN_ISR_S24_MASK,
				HAL_RX_FLUSH_DELAY_USEC, HAL_RX_FLUSH_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "rxdma flush failed at address 0x%x value 0x%x\n",
			   HAL_SEQ_WCSS_DMAC_DMCMN_REG + isr,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG + isr));
		ATH12K_CUMAC_RESET_ERR_INC(ab, rxdma_flush_fail);
		return ret;
	}

	ath12k_hif_rmw32(ab, pmac_rxpcu_base + HAL_PMAC_RXPCU_RX_FLUSH_CTRL,
			 HAL_PMAC_RXPCU_RX_FLUSH_CTRL_RX_FLUSH_REQ, 0);

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG + isr,
			 HAL_DMAC_DMCMN_ISR_S24_MASK, 1);

	return 0;
}

static int ath12k_pmac_rx_suspend(struct ath12k_base *ab)
{
	int i, ret;
	int pmac_hwsch_base[] = HAL_SEQ_WCSS_PMAC_HWSCH_REG;
	int pmac_rxpcu_base[] = HAL_SEQ_WCSS_PMAC_RXPCU_REG;
	int pmac_pmcmn_base[] = HAL_SEQ_WCSS_PMAC_PMCMN_REG;
	int isr[] = HAL_DMAC_DMCMN_ISR_S24;

	for (i = 0; i < ab->num_radios; i++) {
		ath12k_hif_write32(ab, pmac_pmcmn_base[i] +
				 HAL_PMAC_HWSCH_R0_MTU_FOR_HMAC_CONTROLS_IX_0, 0);

		ath12k_hif_rmw32(ab, pmac_pmcmn_base[i] + HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW,
				 HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW_HALT_RX_MASK, 1);

		ret = ath12k_pmac_tx_flush(ab, pmac_hwsch_base[i],
					   HAL_TX_FLUSH_REASON_HALT_RX);
		if (ret) {
			ath12k_err(ab, "pmac tx flush failed\n");
			return ret;
		}

		ret = ath12k_pmac_rx_abort(ab, pmac_rxpcu_base[i], pmac_pmcmn_base[i]);
		if (ret) {
			ath12k_err(ab, "pmac rx abort failed\n");
			return ret;
		}

		ret = ath12k_pmac_rx_flush(ab, pmac_rxpcu_base[i], isr[i]);
		if (ret) {
			ath12k_err(ab, "pmac rx flush failed\n");
			return ret;
		}
	}

	return 0;
}

static int ath12k_pmac_decouple(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_pmac_tx_abort(ab);
	if (ret) {
		ath12k_err(ab, "pmac tx abort failed\n");
		return ret;
	}

	ret = ath12k_pmac_rx_suspend(ab);
	if (ret) {
		ath12k_err(ab, "pmac rx suspend failed\n");
		return ret;
	}

	return 0;
}

static int ath12k_dmac_decouple(struct ath12k_base *ab)
{
	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	int ring_id;

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_DMAC_RXDMA_REG +
			 HAL_DMAC_M0_RXDMA_ENTRANCE_FITLER,
			 HAL_RXDMA_ENT_FITLER_GLOBAL_ENABLE, 0);

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_DMAC_RXDMA_REG +
			 HAL_DMAC_M1_RXDMA_ENTRANCE_FITLER,
			 HAL_RXDMA_ENT_FITLER_GLOBAL_ENABLE, 0);

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_UMCMN_REG +
			 HAL_UMAC_UMCMN_GLOBAL_CFG,
			 HAL_UMAC_UMCMN_GLOBAL_CFG_HOLD, 1);

	ring_id = HAL_WBM2RXDMA_LINK_MLO1_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	ring_id = HAL_WBM2RXDMA_SW0_BUFF_MLO1_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	ring_id = HAL_WBM2RXDMA_SW1_BUFF_MLO1_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	ring_id = HAL_WBM2RXDMA_PPE_BUFF_MLO1_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	ring_id = HAL_RXDMA2REO_MLO0_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	ring_id = HAL_RXDMA2REO_MLO_IPC_RING;
	ath12k_hif_rmw32(ab, hal_wifi8->reset_rings[ring_id].srng_misc_reg,
			 HAL_TCL1_RING_MISC_SRNG_ENABLE, 0);

	return 0;
}

static int ath12k_dmac_pmac_decouple(struct ath12k_base *ab, u32 arg)
{
	int ret;

	ret = ath12k_pmac_decouple(ab);
	if (ret) {
		ath12k_err(ab, "pmac decouple failed\n");
		return ret;
	}

	ret = ath12k_dmac_decouple(ab);
	if (ret) {
		ath12k_err(ab, "dmac decouple failed\n");
		return ret;
	}

	return 0;
}

static int ath12k_tcl_idle_check(struct ath12k_base *ab)
{
	bool tcl2tqm_bkp, tcl2fw_bkp, tcl2fw_status_bkp;
	int tcl_not_idle;
	u32 halt_stat;

	tcl2tqm_bkp = (ath12k_hif_read32_masked(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
						TCL2TQM_RING_STATUS,
						TCL_NUM_AVAIL_WORDS_MASK) == 0);
	tcl2fw_bkp = (ath12k_hif_read32_masked(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					       TCL2FW_EXCEPTION_RING_STATUS,
					       TCL_NUM_AVAIL_WORDS_MASK) == 0);
	tcl2fw_status_bkp = (ath12k_hif_read32_masked(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
						      TCL2FW_STATUS_RING_STATUS,
						      TCL_NUM_AVAIL_WORDS_MASK) == 0);

	/* TCL is not necessarily expected to go to Idle when it is backpressured by
	 * TQM/FW. TCL Idle check irrelevant in case of TCL backpressure.
	 */
	if (tcl2tqm_bkp || tcl2fw_bkp || tcl2fw_status_bkp)
		return 0;

	tcl_not_idle = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					 HAL_TCL1_RING_CMN_CTRL_REG,
					 TCL_IDLE_VALUE, TCL_IDLE_MASK,
					 HAL_MAC_IDLE_CHECK_DELAY_USEC,
					 HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);

	halt_stat = ath12k_hif_read32_masked(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
					     HAL_TCL_RING_HALT_STAT_REG,
					     HAL_TCL_RING_HALT_STAT_MASK);

	/* TCL is idle */
	if (!tcl_not_idle) {
		if (halt_stat == HAL_TCL_RING_HALT_STAT_MASK)
			return 0;
		ath12k_err(ab, "TCL is idle but HALT_STAT mismatch (expected:0x%lx got:0x%x)",
			   HAL_TCL_RING_HALT_STAT_MASK, halt_stat);
		ATH12K_CUMAC_RESET_ERR_INC(ab, tcl_idle_fail);
		return -EINVAL;
	}

	/* TCL is not idle */
	if (halt_stat == HAL_TCL_RING_HALT_STAT_MASK) {
		ath12k_err(ab, "TCL HALT_STAT set but TCL not idle (halt_stat:0x%x)",
			   halt_stat);
		ATH12K_CUMAC_RESET_ERR_INC(ab, tcl_idle_fail);
		return -EINVAL;
	}

	ath12k_err(ab, "TCL not idle and HALT_STAT not set (halt_stat:0x%x)",
		   halt_stat);
	ATH12K_CUMAC_RESET_ERR_INC(ab, tcl_idle_fail);
	/* TODO: Check ARB WAIT state for the rings */
	return 0;
}

static int ath12k_halt_tcl(struct ath12k_base *ab, u32 is_halt)
{
	if (is_halt) {
		int ret;

		ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				 HAL_TCL1_RING_CMN_CTRL_REG,
				 HAL_TCL_RING_HALT_MASK,
				 HAL_TCL_RING_HALT_VALUE);

		ret = ath12k_tcl_idle_check(ab);
		if (ret) {
			ath12k_err(ab, "TCL idle check failed");
			return -EINVAL;
		}
	} else {
		ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_TCL_REG +
				 HAL_TCL1_RING_CMN_CTRL_REG,
				 HAL_TCL_RING_HALT_MASK,
				 HAL_TCL_RING_UNHALT_VALUE);
	}
	return 0;
}

static int ath12k_sam_idle_check(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_UMCMN_REG +
				HAL_UMCMN_R0_IDLE_SIGNAL,
				HAL_SAM_IDLE_VALUE, HAL_SAM_IDLE_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "SAM idle check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_UMCMN_REG + HAL_UMCMN_R0_IDLE_SIGNAL,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_UMCMN_REG +
					     HAL_UMCMN_R0_IDLE_SIGNAL));
		ATH12K_CUMAC_RESET_ERR_INC(ab, sam_idle_fail);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_enable_sam(struct ath12k_base *ab, u32 enable)
{
	int ret;

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_SAM_REG + HAL_SAM_R0_CONFIG,
			 HAL_SAM_ENABLE, enable);

	if (!enable) {
		ret = ath12k_sam_idle_check(ab);
		if (ret) {
			ath12k_err(ab, "SAM idle check failed");
			return ret;
		}
	}

	return 0;
}

static int ath12k_is_tqm_prefetch_idle(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				TCL2TQM_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM TCL2TQM ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   TCL2TQM_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     TCL2TQM_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				SW_CMD_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM SW_CMD ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   SW_CMD_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     SW_CMD_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				SW_CMD1_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM SW_CMD1 ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   SW_CMD1_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     SW_CMD1_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				TQM2TQM_IN1_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM TQM2TQM_IN1 ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   TQM2TQM_IN1_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     TQM2TQM_IN1_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				TQM2TQM_IN2_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM TQM2TQM_IN2 ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   TQM2TQM_IN2_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     TQM2TQM_IN2_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				TQM2TQM_IN3_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM TQM2TQM_IN3 ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   TQM2TQM_IN3_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     TQM2TQM_IN3_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				TQM2TQM_IN4_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM TQM2TQM_IN4 ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   TQM2TQM_IN4_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     TQM2TQM_IN4_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
				FW2TQM_RING_CONSUMER_PREFETCH_STATUS,
				HAL_TQM_PREFETCH_COUNT_VALUE, HAL_TQM_PREFETCH_COUNT_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM FW2TQM ring consumer prefetch status check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG +
			   FW2TQM_RING_CONSUMER_PREFETCH_STATUS,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     FW2TQM_RING_CONSUMER_PREFETCH_STATUS));
		return -EINVAL;
	}

	return 0;
}

static int ath12k_is_tqm_sm_idle(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +	HAL_TQM_SM_STATES_IX0,
				HAL_TQM_SM_STATES_VALUE, HAL_TQM_SM_STATES_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM state machine IX0 idle check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_SM_STATES_IX0,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     HAL_TQM_SM_STATES_IX0));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +	HAL_TQM_SM_STATES_IX1,
				HAL_TQM_SM_STATES_VALUE, HAL_TQM_SM_STATES_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM state machine IX1 idle check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_SM_STATES_IX1,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     HAL_TQM_SM_STATES_IX1));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +	HAL_TQM_SM_STATES_IX2,
				HAL_TQM_SM_STATES_VALUE, HAL_TQM_SM_STATES_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM state machine IX2 idle check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_SM_STATES_IX2,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     HAL_TQM_SM_STATES_IX2));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +	HAL_TQM_SM_STATES_IX3,
				HAL_TQM_SM_STATES_VALUE, HAL_TQM_SM_STATES_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "TQM state machine IX3 idle check failed at address 0x%x value 0x%x",
			   HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_SM_STATES_IX3,
			   ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     HAL_TQM_SM_STATES_IX3));
		return -EINVAL;
	}

	return 0;
}

static int ath12k_tqm_idle_check(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_is_tqm_prefetch_idle(ab);
	if (ret) {
		ath12k_err(ab, "TQM prefetch is busy");
		ATH12K_CUMAC_RESET_ERR_INC(ab, tqm_prefetch_idle_fail);
		return ret;
	}

	ret = ath12k_is_tqm_sm_idle(ab);
	if (ret) {
		ath12k_err(ab, "TQM state machine is not idle");
		ATH12K_CUMAC_RESET_ERR_INC(ab, tqm_sm_idle_fail);
		return ret;
	}

	return 0;
}

static int ath12k_pause_tqm(struct ath12k_base *ab, u32 pause)
{
	int ret;

	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_R0_CONTROL,
			 HAL_TQM_BLOCK_PREFETCH_MASK, pause);

	if (pause) {
		ret = ath12k_tqm_idle_check(ab);
		if (ret) {
			ath12k_err(ab, "TQM idle check failed");
			return ret;
		}
	}

	return 0;
}

static int ath12k_enable_tqm(struct ath12k_base *ab, u32 enable)
{
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_R0_CONTROL,
			 HAL_TQM_BLOCK_ENABLE_MASK, enable);

	return 0;
}

static int ath12k_enable_wbm(struct ath12k_base *ab, u32 enable)
{
	if (enable) {
		ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_GENERAL_ENABLE,
				 HAL_WBM_ENABLE_MASK, HAL_WBM_ENABLE_VALUE);
	} else {
		ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_GENERAL_ENABLE,
				 HAL_WBM_ENABLE_MASK, HAL_WBM_DISABLE_VALUE);
	}

	return 0;
}

static int ath12k_enable_reo(struct ath12k_base *ab, u32 enable)
{
	ath12k_hif_rmw32(ab, HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_GEN_ENABLE,
			 HAL_REO_ENABLE_MASK, enable);

	return 0;
}

static int ath12k_mlo_ring_idle_check(struct ath12k_base *ab, int i,
				      const struct ath12k_hal_reset_rings *reset_rings)
{
	int ret;

	ret = ath12k_hif_poll32(ab, reset_rings->srng_misc_reg,
				HAL_SRNG_SM_STATE_VALUE, HAL_SRNG_SM_STATE1_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "Reset ring %d SM STATE1 check failed at address 0x%x value 0x%x",
			   i, reset_rings->srng_misc_reg,
			   ath12k_hif_read32(ab, reset_rings->srng_misc_reg));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, reset_rings->srng_misc_reg,
				HAL_SRNG_SM_STATE_VALUE, HAL_SRNG_SM_STATE2_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "Reset ring %d SM STATE2 check failed at address 0x%x value 0x%x",
			   i, reset_rings->srng_misc_reg,
			   ath12k_hif_read32(ab, reset_rings->srng_misc_reg));
		return -EINVAL;
	}

	ret = ath12k_hif_poll32(ab, reset_rings->consumer_producer_mlo,
				HAL_SRNG_SM_STATE_VALUE, HAL_SRNG_SM_STATE3_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "Reset ring %d SM STATE3 check failed at address 0x%x value 0x%x",
			   i, reset_rings->consumer_producer_mlo,
			   ath12k_hif_read32(ab, reset_rings->consumer_producer_mlo));
		return -EINVAL;
	}

	return 0;
}

static int ath12k_ring_idle_check(struct ath12k_base *ab, int i,
				  const struct ath12k_hal_reset_rings *reset_rings)
{
	int ret;

	ret = ath12k_hif_poll32(ab, reset_rings->srng_misc_reg,
				HAL_SRNG_IDLE_VALUE, HAL_SRNG_IDLE_MASK,
				HAL_MAC_IDLE_CHECK_DELAY_USEC,
				HAL_MAC_IDLE_CHECK_TIMEOUT_USEC);
	if (ret) {
		ath12k_err(ab, "Reset ring %d idle check failed at address 0x%x value 0x%x",
			   i, reset_rings->srng_misc_reg,
			   ath12k_hif_read32(ab, reset_rings->srng_misc_reg));
		return -EINVAL;
	}

	return 0;
}

static void ath12k_tqm_tp_timer_handler(struct timer_list *t)
{
	struct ath12k_base *ab = from_timer(ab, t, tqm_tp_timer);
	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	const struct ath12k_hal_reset_rings *reset_ring;
	u32 hp;

	reset_ring = &hal_wifi8->reset_rings[HAL_TQM_LOWPRI_STATUS_RING];

	/* Mirror the ring HP into the TP so the TQM always sees the ring as
	 * drained while the FW is not reaping it.
	 */
	hp = ath12k_hif_read32(ab, reset_ring->hp);
	ath12k_hif_write32(ab, reset_ring->hp + 4, hp);

	mod_timer(&ab->tqm_tp_timer,
		  jiffies + msecs_to_jiffies(ATH12K_TQM_TP_TIMER_INTERVAL_MS));
}

void ath12k_wifi8_update_tqm_status_ring_tp(struct ath12k_base *ab, bool enable)
{
	if (!ab->hw_params->tqm_status_war)
		return;

	if (!enable) {
		del_timer_sync(&ab->tqm_tp_timer);
		return;
	}

	timer_setup(&ab->tqm_tp_timer, ath12k_tqm_tp_timer_handler, 0);
	mod_timer(&ab->tqm_tp_timer,
		  jiffies + msecs_to_jiffies(ATH12K_TQM_TP_TIMER_INTERVAL_MS));
}


static const u32 umcmn_isr_s_regs[] = {
	[0]  = HAL_UMAC_UMCMN_R0_ISR_S0,
	[1]  = 0,
	[2]  = HAL_UMAC_UMCMN_R0_ISR_S2,
	[3]  = HAL_UMAC_UMCMN_R0_ISR_S3,
	[4]  = HAL_UMAC_UMCMN_R0_ISR_S4,
	[5]  = HAL_UMAC_UMCMN_R0_ISR_S5,
	[6]  = HAL_UMAC_UMCMN_R0_ISR_S6,
	[7]  = HAL_UMAC_UMCMN_R0_ISR_S7,
	[8]  = HAL_UMAC_UMCMN_R0_ISR_S8,
	[9]  = HAL_UMAC_UMCMN_R0_ISR_S9,
	[10] = HAL_UMAC_UMCMN_R0_ISR_S10,
	[11] = HAL_UMAC_UMCMN_R0_ISR_S11,
	[12] = HAL_UMAC_UMCMN_R0_ISR_S12,
	[13] = HAL_UMAC_UMCMN_R0_ISR_S13,
	[14] = HAL_UMAC_UMCMN_R0_ISR_S14,
	[15] = HAL_UMAC_UMCMN_R0_ISR_S15,
	[16] = HAL_UMAC_UMCMN_R0_ISR_S16,
	[17] = HAL_UMAC_UMCMN_R0_ISR_S17,
	[18] = HAL_UMAC_UMCMN_R0_ISR_S18,
	[19] = HAL_UMAC_UMCMN_R0_ISR_S19,
	[20] = HAL_UMAC_UMCMN_R0_ISR_S20,
	[21] = HAL_UMAC_UMCMN_R0_ISR_S21,
	[22] = HAL_UMAC_UMCMN_R0_ISR_S22,
	[23] = HAL_UMAC_UMCMN_R0_ISR_S23,
	[24] = HAL_UMAC_UMCMN_R0_ISR_S24,
	[25] = HAL_UMAC_UMCMN_R0_ISR_S25,
	[26] = HAL_UMAC_UMCMN_R0_ISR_S26,
	[27] = HAL_UMAC_UMCMN_R0_ISR_S27,
	[28] = HAL_UMAC_UMCMN_R0_ISR_S28,
	[29] = HAL_UMAC_UMCMN_R0_ISR_S29,
	[30] = HAL_UMAC_UMCMN_R0_ISR_S30,
};

static const u32 umcmn_isr_s_debug_mask[] = {
	[0]  = 0x00000000,
	[1]  = 0x00000000,
	[2]  = 0x0000000F,
	[3]  = 0x64008000,
	[4]  = 0x00000000,
	[5]  = 0x2041401E,
	[6]  = 0x00000000,
	[7]  = 0x07FF0000,
	[8]  = 0x00094000,
	[9]  = 0x00000000,
	[10] = 0x4007FFFF,
	[11] = 0x01140415,
	[12] = 0x1140D04F,
	[13] = 0x0002ABEA,
	[14] = 0x7A497FFF,
	[15] = 0x00033E02,
	[16] = 0x000001EE,
	[17] = 0x00000000,
	[18] = 0x6002AAAA,
	[19] = 0x00003492,
	[20] = 0xF803FE00,
	[21] = 0x40000FFF,
	[22] = 0x0AAAB6AD,
	[23] = 0x01555555,
	[24] = 0x2AA54A95,
	[25] = 0x24924955,
	[26] = 0x09249249,
	[27] = 0x09249249,
	[28] = 0x0AD51249,
	[29] = 0x00000040,
	[30] = 0x00000AAA,
};

static const u32 umcmn_isr_s_fatal_mask[] = {
	[0]  = 0,
	[1]  = 0,
	[2]  = 0,
	[3]  = 0x64008000,
	[4]  = 0,
	[5]  = 0x2041401E,
	[6]  = 0,
	[7]  = 0,
	[8]  = 0,
	[9]  = 0,
	[10] = 0,
	[11] = 0x01140415,
	[12] = 0x0000000F,
	[13] = 0x0002ABEA,
	[14] = 0x00007FFD,
	[15] = 0x00003000,
	[16] = 0x0000000E,
	[17] = 0,
	[18] = 0,
	[19] = 0,
	[20] = 0,
	[21] = 0,
	[22] = 0,
	[23] = 0,
	[24] = 0,
	[25] = 0,
	[26] = 0,
	[27] = 0,
	[28] = 0,
	[29] = 0,
	[30] = 0,
};

static const struct ath12k_tqm_sm_state tqm_sm_pairs[] = {
	{ HAL_TQM_SM_STATES_IX0, HAL_TQM_BANK_SM_STATES_IX0 },
	{ HAL_TQM_SM_STATES_IX1, HAL_TQM_BANK_SM_STATES_IX1 },
	{ HAL_TQM_SM_STATES_IX2, HAL_TQM_BANK_SM_STATES_IX2 },
	{ HAL_TQM_SM_STATES_IX3, HAL_TQM_BANK_SM_STATES_IX3 },
	{ HAL_TQM_SM_STATES_IX4, HAL_TQM_BANK_SM_STATES_IX4 },
	{ HAL_TQM_SM_STATES_IX5, HAL_TQM_BANK_SM_STATES_IX5 },
	{ HAL_TQM_SM_STATES_IX6, HAL_TQM_BANK_SM_STATES_IX6 },
	{ HAL_TQM_SM_STATES_IX7, HAL_TQM_BANK_SM_STATES_IX7 },
	{ HAL_TQM_SM_STATES_IX8, HAL_TQM_BANK_SM_STATES_IX8 },
	{ HAL_TQM_SM_STATES_IX9, HAL_TQM_BANK_SM_STATES_IX9 },
	{ HAL_TQM_SM_STATES_IX10, HAL_TQM_BANK_SM_STATES_IX10 },
	{ HAL_TQM_SM_STATES_IX11, HAL_TQM_BANK_SM_STATES_IX11 },
	{ HAL_TQM_SM_STATES_IX12, HAL_TQM_BANK_SM_STATES_IX12 },
	{ HAL_TQM_SM_STATES_IX13, HAL_TQM_BANK_SM_STATES_IX13 },
};

static void ath12k_clear_isr_registers(struct ath12k_base *ab)
{
	int i;
	int isr_wlan_rx_ok[] = HAL_DMAC_DMCMN_ISR_WLAN_RX_OK;

	/* Clear all ISR_S registers */
	for (i = 0; i < ARRAY_SIZE(umcmn_isr_s_regs); i++) {
		if (!umcmn_isr_s_regs[i])
			continue;
		ath12k_hif_write32(ab, umcmn_isr_s_regs[i], 0);
	}

	/* Clear ISR_P register */
	ath12k_hif_write32(ab, HAL_UMAC_UMCMN_R0_ISR_P, 0);

	/* Clear RX_OK interrupt */
	for (i = 0; i < ab->num_radios; i++) {
		ath12k_hif_write32(ab, HAL_SEQ_WCSS_DMAC_DMCMN_REG + isr_wlan_rx_ok[i],
				   0);
	}
}

static int ath12k_clear_pending_interrupts(struct ath12k_base *ab, u32 arg)
{
	ath12k_clear_isr_registers(ab);

	ath12k_umcmn_irq_enable(ab);
	ath12k_umcmn_timer_enable(ab);

	return 0;
}

static int ath12k_umac_ring_enable(struct ath12k_base *ab, u32 enable)
{
	int i, ret;
	u32 val;

	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	const struct ath12k_hal_reset_rings *reset_rings;

	for (i = 0; i < HAL_RESET_RING_TYPE_MAX; i++) {
		reset_rings = &hal_wifi8->reset_rings[i];

		/* If LSB is not programmed, skip enable and disable of the ring */
		val = ath12k_hif_read32(ab, reset_rings->srng_misc_reg - 16);
		if (!val)
			continue;

		if (reset_rings->consumer_prefetch_timer)
			ath12k_hif_write32(ab, reset_rings->consumer_prefetch_timer,
					   enable ? HAL_RING_PREFETCH_TIMER_ENABLE :
					   HAL_RING_PREFETCH_TIMER_DISABLE);

		if (reset_rings->consumer_producer_mlo) {
			ath12k_hif_rmw32(ab, reset_rings->consumer_producer_mlo,
					 HAL_INTERVAL_OF_FETCH_POINTER_MASK,
					 enable ? 1 : 0);

			if (enable) {
				ret = ath12k_mlo_ring_idle_check(ab, i, reset_rings);
				if (ret) {
					ath12k_err(ab, "MLO %d ring idle check failed",
						   i);
					ATH12K_CUMAC_RESET_ERR_INC(ab,
								   mlo_ring_idle_fail);
					return -EINVAL;
				}
			}
		} else if (enable) {
			ret = ath12k_ring_idle_check(ab, i, reset_rings);
			if (ret) {
				ath12k_err(ab, "%d ring idle check failed", i);
				ATH12K_CUMAC_RESET_ERR_INC(ab, ring_idle_fail);
				return -EINVAL;
			}
		}
		ath12k_hif_rmw32(ab, reset_rings->srng_misc_reg,
				 HAL_TCL1_RING_MISC_SRNG_ENABLE, enable);
	}
	return 0;
}

static int ath12k_umac_ring_reset(struct ath12k_base *ab)
{
	int i, ret;
	u32 val;

	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	const struct ath12k_hal_reset_rings *reset_rings;

	for (i = 0; i < HAL_RESET_RING_TYPE_MAX; i++) {
		reset_rings = &hal_wifi8->reset_rings[i];

		/* If LSB is not programmed, skip reset of the ring */
		val = ath12k_hif_read32(ab, reset_rings->srng_misc_reg - 16);
		if (!val)
			continue;

		if (reset_rings->consumer_producer_mlo) {
			ret = ath12k_mlo_ring_idle_check(ab, i, reset_rings);
			if (ret) {
				ath12k_err(ab, "MLO %d ring idle check failed", i);
				ATH12K_CUMAC_RESET_ERR_INC(ab, mlo_ring_idle_fail);
				return -EINVAL;
			}
			ath12k_hif_write32(ab, reset_rings->mlo_doorbell_press, 0);
		} else {
			ret = ath12k_ring_idle_check(ab, i, reset_rings);
			if (ret) {
				ath12k_err(ab, "%d ring idle check failed", i);
				ATH12K_CUMAC_RESET_ERR_INC(ab, ring_idle_fail);
				return -EINVAL;
			}
		}
		ath12k_hif_write32(ab, reset_rings->hp, 0);
		ath12k_hif_write32(ab, reset_rings->hp + 4, 0);
	}
	return 0;
}

static int ath12k_umac_pre_ring_reset(struct ath12k_base *ab, u32 arg)
{
	int ret;

	ret = ath12k_umac_ring_enable(ab, 0);
	if (ret) {
		ath12k_err(ab, "umac ring disable failed");
		return ret;
	}

	ret = ath12k_umac_ring_reset(ab);
	if (ret) {
		ath12k_err(ab, "umac ring reset failed");
		return ret;
	}

	return 0;
}

static int ath12k_umac_apply_soft_reset(struct ath12k_base *ab, u32 arg)
{
	u32 soft_reset_val;
	int ret;

	soft_reset_val = ath12k_hif_read32(ab, HAL_UMAC_UMRCM_SOFTRESET);
	soft_reset_val |= HAL_UMAC_UMRCM_SOFTRESET_VALUE;
	ath12k_hif_write32(ab, HAL_UMAC_UMRCM_SOFTRESET, soft_reset_val);

	ret = ath12k_pause_tqm(ab, 0);
	if (ret) {
		ath12k_err(ab, "RESET: Unpause TQM during soft reset failed\n");
		return ret;
	}

	ret = ath12k_enable_tqm(ab, 0);
	if (ret) {
		ath12k_err(ab, "RESET: Enable TQM during soft reset failed\n");
		return ret;
	}

	udelay(HAL_UMAC_UMRCM_SOFTRESET_DELAY);

	soft_reset_val &= ~(HAL_UMAC_UMRCM_SOFTRESET_VALUE);
	ath12k_hif_write32(ab, HAL_UMAC_UMRCM_SOFTRESET, soft_reset_val);

	return 0;
}

static int ath12k_umac_post_ring_reset(struct ath12k_base *ab, u32 arg)
{
	int ret;

	ret = ath12k_umac_ring_reset(ab);
	if (ret) {
		ath12k_err(ab, "umac ring reset failed");
		return ret;
	}

	ret = ath12k_umac_ring_enable(ab, 1);
	if (ret) {
		ath12k_err(ab, "umac ring enable failed");
		return ret;
	}

	return 0;
}

/* Dummy handlers for START/END steps (no-op, just for timestamp recording) */
static int ath12k_dummy_step(struct ath12k_base *ab, u32 arg)
{
	return 0;
}

static int ath12k_cumac_reset_wrapper(struct ath12k_base *ab,
					   const struct cumac_hw_reset_step *step,
					   const char *phase_name,
					   u64 *ts)
{
	int ret;

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "%s: Executing step '%s'\n",
		   phase_name, step->name);

	ret = step->fn(ab, step->arg);

	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "%s: Step '%s' failed with error %d\n",
			   phase_name, step->name, ret);
	} else {
		*ts = jiffies;
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "%s: Step '%s' completed successfully\n",
			   phase_name, step->name);
	}

	return ret;
}

static void ath12k_cumac_reset_print_summary(struct ath12k_base *ab,
					     const struct cumac_hw_reset_step *steps,
					     const u64 *ts,
					     int count,
					     const char *phase_name)
{
	int i;
	u64 start, end, duration;

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "%s: Step timestamp summary:\n", phase_name);

	for (i = 0; i < count; i++) {
		start = (i > 0) ? ts[i - 1] : ts[0];
		end = ts[i];
		duration = (i > 0) ? jiffies_to_msecs(end - start) : 0;
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "%s: [%2d] %-35s start=%llu end=%llu time_taken=%llu ms\n",
			   phase_name, i, steps[i].name, start, end, duration);
	}

	duration = jiffies_to_msecs(ts[count - 1] - ts[0]);
	ath12k_info(ab, "%s: Total time: %llu ms\n", phase_name, duration);
}

static const struct cumac_hw_reset_step pre_reset_steps[] = {
	[CUMAC_HW_PRE_RESET_START] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Pre-reset start"
	},
	[CUMAC_HW_PRE_RESET_RXDMA_PREFETCH_DISABLE] = {
		ath12k_enable_rxdma_prefetch,
		0,
		"CUMAC HW Disable RXDMA prefetch"
	},
	[CUMAC_HW_PRE_RESET_HALT_MLO_DOORBELLS] = {
		ath12k_halt_mlo_doorbells,
		1,
		"CUMAC HW Halt MLO doorbells"
	},
	[CUMAC_HW_PRE_RESET_PAUSE_GLOBAL_WSI] = {
		ath12k_pause_global_wsi,
		1,
		"CUMAC HW Pause global WSI"
	},
	[CUMAC_HW_PRE_RESET_DMAC_PMAC_DECOUPLE] = {
		ath12k_dmac_pmac_decouple,
		0,
		"CUMAC HW Decouple DMAC/PMAC"
	},
	[CUMAC_HW_PRE_RESET_HALT_TCL] = {
		ath12k_halt_tcl,
		1,
		"CUMAC HW Halt TCL"
	},
	[CUMAC_HW_PRE_RESET_DISABLE_SAM] = {
		ath12k_enable_sam,
		0,
		"CUMAC HW Disable SAM"
	},
	[CUMAC_HW_PRE_RESET_PAUSE_TQM] = {
		ath12k_pause_tqm,
		1,
		"CUMAC HW Pause TQM"
	},
	[CUMAC_HW_PRE_RESET_DISABLE_WBM] = {
		ath12k_enable_wbm,
		0,
		"CUMAC HW Disable WBM"
	},
	[CUMAC_HW_PRE_RESET_DISABLE_REO] = {
		ath12k_enable_reo,
		0,
		"CUMAC HW Disable REO"
	},
	[CUMAC_HW_PRE_RESET_END] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Pre-reset end"
	},
};

static const struct cumac_hw_reset_step reset_steps[] = {
	[CUMAC_HW_RESET_START] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Reset start"
	},
	[CUMAC_HW_RESET_PREREQUISITES] = {
		ath12k_check_umac_reset_prerequisites,
		0,
		"CUMAC HW Check reset prerequisites"
	},
	[CUMAC_HW_RESET_PRE_RING_RESET] = {
		ath12k_umac_pre_ring_reset,
		0,
		"CUMAC HW Pre-ring reset"
	},
	[CUMAC_HW_RESET_APPLY_SOFT_RESET] = {
		ath12k_umac_apply_soft_reset,
		0,
		"CUMAC HW Apply soft reset"
	},
	[CUMAC_HW_RESET_POST_RING_RESET] = {
		ath12k_umac_post_ring_reset,
		0,
		"CUMAC HW Post-ring reset"
	},
	[CUMAC_HW_RESET_END] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Reset end"
	},
};

static const struct cumac_hw_reset_step post_reset_steps[] = {
	[CUMAC_HW_POST_RESET_START] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Post-reset start"
	},
	[CUMAC_HW_POST_RESET_ENABLE_TQM] = {
		ath12k_enable_tqm,
		1,
		"CUMAC HW Enable TQM"
	},
	[CUMAC_HW_POST_RESET_CLEAR_INTERRUPTS] = {
		ath12k_clear_pending_interrupts,
		0,
		"CUMAC HW Clear pending interrupts"
	},
	[CUMAC_HW_POST_RESET_ENABLE_SAM] = {
		ath12k_enable_sam,
		1,
		"CUMAC HW Enable SAM"
	},
	[CUMAC_HW_POST_RESET_ENABLE_WBM] = {
		ath12k_enable_wbm,
		1,
		"CUMAC HW Enable WBM"
	},
	[CUMAC_HW_POST_RESET_ENABLE_REO] = {
		ath12k_enable_reo,
		1,
		"CUMAC HW Enable REO"
	},
	[CUMAC_HW_POST_RESET_UNPAUSE_GLOBAL_WSI] = {
		ath12k_pause_global_wsi,
		0,
		"CUMAC HW Unpause global WSI"
	},
	[CUMAC_HW_POST_RESET_UNHALT_MLO_DOORBELLS] = {
		ath12k_halt_mlo_doorbells,
		0,
		"CUMAC HW Unhalt MLO doorbells"
	},
	[CUMAC_HW_POST_RESET_ENABLE_RXDMA_PREFETCH] = {
		ath12k_enable_rxdma_prefetch,
		1,
		"CUMAC HW Enable RXDMA prefetch"
	},
	[CUMAC_HW_POST_RESET_UNHALT_TCL] = {
		ath12k_halt_tcl,
		0,
		"CUMAC HW Unhalt TCL"
	},
	[CUMAC_HW_POST_RESET_END] = {
		ath12k_dummy_step,
		0,
		"CUMAC HW Post-reset end"
	},
};

int ath12k_cumac_hw_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	struct ath12k_cumac_hw_reset_timestamps *ssr_ts = &hal_wifi8->ssr_ts;
	int ret;
	int step;

	for (step = 0; step < CUMAC_HW_PRE_RESET_MAX; step++) {
		ret = ath12k_cumac_reset_wrapper(ab, &pre_reset_steps[step],
						 "CUMAC HW PRE-RESET",
						 &ssr_ts->cumac_hw_pre_reset_ts[step]);
		if (ret) {
			ath12k_err(ab, "CUMAC HW PRE-RESET: step '%s' failed: %d\n",
				   pre_reset_steps[step].name, ret);
			return ret;
		}
	}

	ath12k_cumac_reset_print_summary(ab, pre_reset_steps,
					 ssr_ts->cumac_hw_pre_reset_ts,
					 CUMAC_HW_PRE_RESET_MAX, "CUMAC HW PRE-RESET");

	return 0;
}

int ath12k_cumac_hw_reset(struct ath12k_base *ab)
{
	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	struct ath12k_cumac_hw_reset_timestamps *ssr_ts = &hal_wifi8->ssr_ts;
	int ret;
	int step;

	for (step = 0; step < CUMAC_HW_RESET_MAX; step++) {
		ret = ath12k_cumac_reset_wrapper(ab, &reset_steps[step],
						 "CUMAC HW RESET",
						 &ssr_ts->cumac_hw_reset_ts[step]);
		if (ret) {
			ath12k_err(ab, "CUMAC HW RESET: step '%s' failed: %d\n",
				   reset_steps[step].name, ret);
			return ret;
		}
	}

	ath12k_cumac_reset_print_summary(ab, reset_steps,
					 ssr_ts->cumac_hw_reset_ts,
					 CUMAC_HW_RESET_MAX, "CUMAC HW RESET");

	return 0;
}

int ath12k_cumac_hw_post_reset(struct ath12k_base *ab)
{
	struct ath12k_hal_wifi8 *hal_wifi8 = ath12k_get_hal_wifi8(&ab->hal);
	struct ath12k_cumac_hw_reset_timestamps *ssr_ts = &hal_wifi8->ssr_ts;
	int ret;
	int step;

	for (step = 0; step < CUMAC_HW_POST_RESET_MAX; step++) {
		ret = ath12k_cumac_reset_wrapper(ab, &post_reset_steps[step],
						 "CUMAC HW POST-RESET",
						 &ssr_ts->cumac_hw_post_reset_ts[step]);
		if (ret) {
			ath12k_err(ab, "CUMAC HW POST-RESET: step '%s' failed: %d\n",
				   post_reset_steps[step].name, ret);
			return ret;
		}
	}

	ath12k_cumac_reset_print_summary(ab, post_reset_steps,
					 ssr_ts->cumac_hw_post_reset_ts,
					 CUMAC_HW_POST_RESET_MAX, "CUMAC HW POST-RESET");

	return 0;
}

void ath12k_wifi8_umcmn_irq_disable(struct ath12k_base *ab)
{
	ath12k_hif_umcmn_irq_disable(ab);
}

void ath12k_wifi8_umcmn_irq_enable(struct ath12k_base *ab)
{
	ath12k_hif_umcmn_irq_enable(ab);
}

static bool ath12k_tqm_watchdog_nonfatal(struct ath12k_base *ab)
{
	u32 sm_state, bank_state;
	int i;

	for (i = 0; i < ARRAY_SIZE(tqm_sm_pairs); i++) {
		sm_state = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					     tqm_sm_pairs[i].sm);
		udelay(HAL_MAC_IDLE_CHECK_DELAY_USEC);
		bank_state = ath12k_hif_read32(ab, HAL_SEQ_WCSS_UMAC_TQM_REG +
					       tqm_sm_pairs[i].bank);

		/* TQM states are still processing. wdog is non-fatal */
		if (sm_state != bank_state)
			return true;
	}

	return false;
}

irqreturn_t ath12k_wifi8_umcmn_interrupt_handler(int irq, void *arg)
{
	struct ath12k_base *ab = arg;
	unsigned long isr_p_long;
	u32 isr_p, isr_s, debug, fatal;
	int bit;

	/* Step 1: Read ISR_P to determine which block triggered the interrupt */
	isr_p = ath12k_hif_read32(ab, HAL_UMAC_UMCMN_R0_ISR_P);
	if (!isr_p)
		return IRQ_HANDLED;

	/* Step 2: For each set bit in ISR_P bits [0-30], read the corresponding
	 * ISR_S register.
	 */
	isr_p_long = isr_p;
	for_each_set_bit(bit, &isr_p_long, ARRAY_SIZE(umcmn_isr_s_regs)) {
		/* Bit 1 has no ISR_S1 register */
		if (!umcmn_isr_s_regs[bit])
			continue;

		isr_s = ath12k_hif_read32(ab, umcmn_isr_s_regs[bit]);
		debug = isr_s & umcmn_isr_s_debug_mask[bit];
		fatal = isr_s & umcmn_isr_s_fatal_mask[bit];

		if (debug)
			ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
				   "umcmn debug interrupt received: ISR_P bit %d, ISR_S%d: 0x%08x, debug_mask: 0x%08x\n",
				   bit, bit, isr_s, umcmn_isr_s_debug_mask[bit]);

		if (ab->hw_params->tqm_status_war &&
		    bit == HAL_UMCMN_ISR_S14_INDEX &&
		    (fatal & HAL_UMCMN_ISR_S14_TQM_WATCHDOG) &&
		    ath12k_tqm_watchdog_nonfatal(ab))
			fatal &= ~HAL_UMCMN_ISR_S14_TQM_WATCHDOG;

		/* Step 3: Check for fatal errors */
		if (fatal) {
			ath12k_err(ab,
				   "umcmn fatal interrupt received: ISR_P bit %d, ISR_S%d: 0x%08x, fatal_mask: 0x%08x\n",
				   bit, bit, isr_s, umcmn_isr_s_fatal_mask[bit]);

			clear_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags);
			ath12k_umcmn_irq_disable(ab);
			ath12k_wifi8_update_tqm_status_ring_tp(ab, false);

#ifdef CPTCFG_QCN_EXTN
			if (ath12k_cfg_get(ab, ATH12K_CFG_CRASH_ON_UMCMN_FATAL)) {
				ath12k_err(ab,
					   "Trigger Q6 BCR Reset and crash for full dump collection\n");
				ab->umcmn_fatal_received = true;
			} else {
#endif /* CPTCFG_QCN_EXTN */
				ath12k_err(ab,
					   "Trigger SOC Global Reset and fallback to mode0 recovery by triggering partner crash\n");
				ath12k_recovery_skip_partner_dump_collection(ab);
				ath12k_core_trigger_partner_device_crash(ab);
#ifdef CPTCFG_QCN_EXTN
			}
#endif /* CPTCFG_QCN_EXTN */
			break;
		}
	}

	return IRQ_HANDLED;
}

void ath12k_umcmn_timer_handler(struct timer_list *t)
{
	struct ath12k_base *ab = from_timer(ab, t, umcmn_timer);

	ath12k_wifi8_umcmn_interrupt_handler(ab->umcmn_irq_num, ab);

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags))
		mod_timer(&ab->umcmn_timer,
			  jiffies + msecs_to_jiffies(ATH12K_UMCMN_TIMER_INTERVAL_MS));
}



int ath12k_wifi8_umcmn_timer_config(struct ath12k_base *ab)
{
	timer_setup(&ab->umcmn_timer, ath12k_umcmn_timer_handler, 0);
	return 0;
}

void ath12k_wifi8_umcmn_timer_free(struct ath12k_base *ab)
{
	del_timer_sync(&ab->umcmn_timer);
}

void ath12k_wifi8_umcmn_timer_enable(struct ath12k_base *ab)
{
	mod_timer(&ab->umcmn_timer,
		  jiffies + msecs_to_jiffies(ATH12K_UMCMN_TIMER_INTERVAL_MS));
}

int ath12k_wifi8_umcmn_irq_config(struct ath12k_base *ab)
{
	return ath12k_hif_umcmn_irq_config(ab, ath12k_wifi8_umcmn_interrupt_handler);
}

void ath12k_wifi8_umcmn_irq_free(struct ath12k_base *ab)
{
	ath12k_hif_umcmn_irq_free(ab);
}
