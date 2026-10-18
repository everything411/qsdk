// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/bitmap.h>

#include "core.h"
#include "coredump.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "hif.h"
#include "hal.h"
#include "dp.h"
#include "umac_reset.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif

/* State machine transition validation table */
static const bool umac_reset_state_transition_valid
	[ATH12K_UMAC_RESET_STATE_MAX][ATH12K_UMAC_RESET_STATE_MAX] = {
	/* From IDLE */
	[ATH12K_UMAC_RESET_STATE_IDLE] = {
		[ATH12K_UMAC_RESET_STATE_INIT] = true,
	},
	/* From INIT */
	[ATH12K_UMAC_RESET_STATE_INIT] = {
		[ATH12K_UMAC_RESET_STATE_TRIGGER_SENT] = true,
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From TRIGGER_SENT */
	[ATH12K_UMAC_RESET_STATE_TRIGGER_SENT] = {
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_START] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From PRE_RESET_START */
	[ATH12K_UMAC_RESET_STATE_PRE_RESET_START] = {
		[ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From PRE_RESET_DONE */
	[ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_START] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_START */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_START] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_DONE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_DONE */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_DONE] = {
		[ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From POST_RESET_COMPLETE */
	[ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE] = {
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
		[ATH12K_UMAC_RESET_STATE_ERROR] = true,
	},
	/* From ERROR */
	[ATH12K_UMAC_RESET_STATE_ERROR] = {
		[ATH12K_UMAC_RESET_STATE_IDLE] = true,
	},
};

static const char *ath12k_umac_reset_state_to_str(enum ath12k_umac_reset_state state)
{
	switch (state) {
	case ATH12K_UMAC_RESET_STATE_IDLE:
		return "IDLE";
	case ATH12K_UMAC_RESET_STATE_INIT:
		return "INIT";
	case ATH12K_UMAC_RESET_STATE_TRIGGER_SENT:
		return "TRIGGER_SENT";
	case ATH12K_UMAC_RESET_STATE_PRE_RESET_START:
		return "PRE_RESET_START";
	case ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE:
		return "PRE_RESET_DONE";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_START:
		return "POST_RESET_START";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_DONE:
		return "POST_RESET_DONE";
	case ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE:
		return "POST_RESET_COMPLETE";
	case ATH12K_UMAC_RESET_STATE_ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}

static int ath12k_umcmn_irq_config(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_ENABLE)
		return 0;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_irq_config)
		return dp->arch_ops->umcmn_irq_config(ab);

	return 0;
}

static int ath12k_umcmn_timer_config(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_POLL)
		return 0;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_timer_config)
		return dp->arch_ops->umcmn_timer_config(ab);

	return 0;
}

static void ath12k_umcmn_irq_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_ENABLE)
		return;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_irq_free)
		dp->arch_ops->umcmn_irq_free(ab);
}

void ath12k_umcmn_irq_disable(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_ENABLE)
		return;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_irq_disable)
		dp->arch_ops->umcmn_irq_disable(ab);
}
EXPORT_SYMBOL(ath12k_umcmn_irq_disable);

void ath12k_umcmn_timer_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_POLL)
		return;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_timer_free)
		dp->arch_ops->umcmn_timer_free(ab);
}
EXPORT_SYMBOL(ath12k_umcmn_timer_free);

void ath12k_umcmn_irq_enable(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_ENABLE)
		return;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_irq_enable)
		dp->arch_ops->umcmn_irq_enable(ab);
}
EXPORT_SYMBOL(ath12k_umcmn_irq_enable);

void ath12k_umcmn_timer_enable(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (DP_UMCMN_INTR_HANDLING_DISABLE ||
	    ab->hw_params->support_umcmn_interrupts != UMCMN_INTERRUPT_POLL)
		return;

	if (dp && dp->arch_ops && dp->arch_ops->umcmn_timer_enable)
		dp->arch_ops->umcmn_timer_enable(ab);
}
EXPORT_SYMBOL(ath12k_umcmn_timer_enable);

static bool ath12k_umac_reset_validate_transition(struct ath12k_base *ab,
						  enum ath12k_umac_reset_state from,
						  enum ath12k_umac_reset_state to)
{
	if (from >= ATH12K_UMAC_RESET_STATE_MAX || to >= ATH12K_UMAC_RESET_STATE_MAX)
		return false;

	return umac_reset_state_transition_valid[from][to];
}

static int ath12k_umac_reset_state_transition(struct ath12k_base *ab,
					      enum ath12k_umac_reset_state new_state)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	enum ath12k_umac_reset_state old_state;
	unsigned long flags;

	spin_lock_irqsave(&umac_reset->state_lock, flags);

	old_state = umac_reset->current_state;

	/* Validate transition */
	if (!ath12k_umac_reset_validate_transition(ab, old_state, new_state)) {
		/* Track error and transition to ERROR state */
		umac_reset->state_error_count++;
		umac_reset->error_from_state = old_state;
		umac_reset->prev_state = old_state;
		umac_reset->current_state = ATH12K_UMAC_RESET_STATE_ERROR;
		umac_reset->state_transition_count[ATH12K_UMAC_RESET_STATE_ERROR]++;
		umac_reset->state_entry_time[ATH12K_UMAC_RESET_STATE_ERROR] =
							jiffies_to_msecs(jiffies);
		spin_unlock_irqrestore(&umac_reset->state_lock, flags);

		ath12k_warn(ab, "[UMAC_RESET] Invalid state transition: %s -> %s\n",
			    ath12k_umac_reset_state_to_str(old_state),
			    ath12k_umac_reset_state_to_str(new_state));
		return -EINVAL;
	}

	/* Perform transition */
	umac_reset->prev_state = old_state;
	umac_reset->current_state = new_state;
	umac_reset->state_transition_count[new_state]++;
	umac_reset->state_entry_time[new_state] = jiffies_to_msecs(jiffies);

	spin_unlock_irqrestore(&umac_reset->state_lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "[UMAC_RESET] State transition: %s -> %s (count: %u)\n",
		   ath12k_umac_reset_state_to_str(old_state),
		   ath12k_umac_reset_state_to_str(new_state),
		   umac_reset->state_transition_count[new_state]);

	return 0;
}

enum ath12k_umac_reset_state ath12k_umac_reset_get_state(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	enum ath12k_umac_reset_state state;
	unsigned long flags;

	spin_lock_irqsave(&umac_reset->state_lock, flags);
	state = umac_reset->current_state;
	spin_unlock_irqrestore(&umac_reset->state_lock, flags);

	return state;
}
EXPORT_SYMBOL(ath12k_umac_reset_get_state);

int ath12k_htt_umac_reset_msg_send(struct ath12k_base *ab,
				   struct ath12k_htt_umac_reset_setup_cmd_params *params)
{
	struct sk_buff *skb;
	struct htt_dp_umac_reset_setup_req_cmd *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct htt_dp_umac_reset_setup_req_cmd *)skb->data;
	cmd->msg_info = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_PREREQUISITE_SETUP,
					HTT_H2T_MSG_TYPE_SET);
	cmd->msg_info |= u32_encode_bits(0, HTT_H2T_MSG_METHOD);
	cmd->msg_info |= u32_encode_bits(0, HTT_T2H_MSG_METHOD);
	cmd->msi_data = params->msi_data;
	cmd->msg_shared_mem.size = sizeof(struct htt_h2t_paddr_size);
	cmd->msg_shared_mem.addr_lo = params->addr_lo;
	cmd->msg_shared_mem.addr_hi = params->addr_hi;


	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);

	if (ret) {
		ath12k_warn(ab, "DP UMAC INIT msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "DP UMAC INIT msg sent from host\n");
	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_htt_umac_reset_send_start_pre_reset_cmd(struct ath12k_base *ab, int is_initiator,
						   int is_target_recovery)
{
	struct sk_buff *skb;
	struct h2t_umac_hang_recovery_start_pre_reset *cmd;
	struct ath12k_dp *dp;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	dp = ath12k_ab_to_dp(ab);
	skb_put(skb, len);
	cmd = (struct h2t_umac_hang_recovery_start_pre_reset*)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = u32_encode_bits(HTT_H2T_MSG_TYPE_UMAC_RESET_START_PRE_RESET,
				   HTT_H2T_UMAC_RESET_MSG_TYPE);
	cmd->hdr |= u32_encode_bits(is_initiator, HTT_H2T_UMAC_RESET_IS_INITIATOR_SET);
	cmd->hdr |= u32_encode_bits(!is_target_recovery, HTT_H2T_UMAC_RESET_IS_TARGET_RECOVERY_SET);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
                ath12k_warn(ab, "failed to send htt umac reset pre reset start: %d\n",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_get_umac_reset_intr_offset(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ATH12K_EXT_IRQ_NUM_MAX; i++) {
		if (ab->hw_params->ring_mask->umac_dp_reset[i])
			return i;
	}
	return 0;
}

int ath12k_htt_umac_reset_setup_cmd(struct ath12k_base *ab)
{
	int msi_data_count;
	struct ath12k_htt_umac_reset_setup_cmd_params params = {};
	u32 msi_data_start, msi_irq_start;
	int ret, intr_ctxt;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;

	intr_ctxt = ath12k_get_umac_reset_intr_offset(ab);

	if (!ab->hw_params->umac_reset_ipc) {
		ret = ath12k_hif_get_user_msi_vector(ab, "DP",
						     &msi_data_count, &msi_data_start,
						     &msi_irq_start);
		if (ret) {
			ath12k_err(ab, "Failed to fill msi_data\n");
			return ret;
		}

		params.msi_data = (intr_ctxt % msi_data_count) + msi_data_start;
	} else {
		params.msi_data = ab->hw_params->umac_reset_ipc;
	}

	params.addr_lo = umac_reset->shmem_paddr_aligned & HAL_ADDR_LSB_REG_MASK;
	params.addr_hi = (u64)umac_reset->shmem_paddr_aligned >> HAL_ADDR_MSB_REG_SHIFT;

	return ath12k_htt_umac_reset_msg_send(ab, &params);
}

/**
 * ath12k_umac_reset_schedule_tasklet - SMP callback to schedule tasklet
 * @info: Pointer to mlo_umac_reset structure
 *
 * Called on target CPU via smp_call_function_single
 */
static void ath12k_umac_reset_schedule_tasklet(void *info)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = info;
	int cpu = smp_processor_id();

	tasklet_hi_schedule(&mlo_umac_reset->tasklet[cpu]);
}

int ath12k_dp_umac_reset_init(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	int alloc_size, ret, cpu;

	if (!ab->hw_params->support_umac_reset)
		return 0;

	umac_reset = &ab->dp_umac_reset;
	umac_reset->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;
	umac_reset->post_send_cb = NULL;

	/* Initialize SKB queues for deferred cleanup */
	skb_queue_head_init(&umac_reset->tx_skb_queue);
	skb_queue_head_init(&umac_reset->rx_skb_queue);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	skb_queue_head_init(&umac_reset->ppeds_tx_skb_queue);
#endif

	alloc_size = sizeof(struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t) +
			    ATH12K_DP_UMAC_RESET_SHMEM_ALIGN - 1;

	umac_reset->shmem_vaddr_unaligned = dma_alloc_coherent(ab->dev,
                                                     alloc_size,
                                                     &umac_reset->shmem_paddr_unaligned,
                                                     GFP_KERNEL);
	if (!umac_reset->shmem_vaddr_unaligned) {
		ath12k_warn(ab, "Failed to allocate memory with size:%u\n", alloc_size);
		return -ENOMEM;
	}

	umac_reset->shmem_vaddr_aligned =
		PTR_ALIGN(umac_reset->shmem_vaddr_unaligned, ATH12K_DP_UMAC_RESET_SHMEM_ALIGN);
	umac_reset->shmem_paddr_aligned =
		umac_reset->shmem_paddr_unaligned + ((unsigned long)umac_reset->shmem_vaddr_aligned -
				(unsigned long)umac_reset->shmem_vaddr_unaligned);
	umac_reset->shmem_size = alloc_size;
	umac_reset->shmem_vaddr_aligned->magic_num = ATH12K_DP_UMAC_RESET_SHMEM_MAGIC_NUM;
	umac_reset->intr_offset = ath12k_get_umac_reset_intr_offset(ab);
	memset(&umac_reset->ts, 0, sizeof(struct ath12k_umac_reset_ts));

	/* Initialize state machine */
	spin_lock_init(&umac_reset->state_lock);
	umac_reset->current_state = ATH12K_UMAC_RESET_STATE_IDLE;
	umac_reset->prev_state = ATH12K_UMAC_RESET_STATE_IDLE;
	memset(umac_reset->state_transition_count, 0,
	       sizeof(umac_reset->state_transition_count));
	memset(umac_reset->state_entry_time, 0, sizeof(umac_reset->state_entry_time));
	umac_reset->state_error_count = 0;
	umac_reset->error_from_state = ATH12K_UMAC_RESET_STATE_IDLE;

	/* Initialize per-CPU call_single_data structures for async SMP calls */
	if (ag) {
		mlo_umac_reset = &ag->mlo_umac_reset;
		for_each_possible_cpu(cpu) {
			mlo_umac_reset->csd[cpu].func =
						ath12k_umac_reset_schedule_tasklet;
			mlo_umac_reset->csd[cpu].info = mlo_umac_reset;
		}
	}

	ret = ath12k_hif_dp_umac_reset_irq_config(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to register interrupt for UMAC RECOVERY\n");
		goto shmem_free;
	}

	ret = ath12k_umcmn_irq_config(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to register interrupt for UMCMN\n");
		goto free_irq;
	}

	ret = ath12k_umcmn_timer_config(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to configure timer for UMCMN\n");
		goto free_irq;
	}

	ret = ath12k_htt_umac_reset_setup_cmd(ab);
	if (ret) {
		ath12k_warn(ab, "Unable to setup UMAC RECOVERY\n");
		goto free_irq;
	}

	ath12k_hif_dp_umac_reset_enable_irq(ab);
	ab->umac_reset_init_done = true;
	return 0;

free_irq:
	ath12k_hif_dp_umac_reset_free_irq(ab);
shmem_free:
	dma_free_coherent(ab->dev,
			  umac_reset->shmem_size,
			  umac_reset->shmem_vaddr_unaligned,
			  umac_reset->shmem_paddr_unaligned);
	umac_reset->shmem_vaddr_unaligned = NULL;
	ab->umac_reset_init_done = false;
	return ret;
}

void ath12k_umac_reset_pre_reset_validation(struct ath12k_base *ab, bool *is_initiator,
					    bool *is_target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	*is_initiator = *is_target_recovery = false;

	if (!ag)
		return;

	mlo_umac_reset = &ag->mlo_umac_reset;

	spin_lock_bh(&mlo_umac_reset->lock);
	if (mlo_umac_reset->initiator_chip == ab->device_id)
		*is_initiator = true;
	if (mlo_umac_reset->umac_reset_info & ATH12K_IS_UMAC_RESET_TYPE_RECOVERY)
		*is_target_recovery = true;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

void ath12k_umac_reset_completion(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;

	if (!mlo_umac_reset || test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	if (!ath12k_dp_umac_reset_in_progress(ab))
		return;

	spin_lock_bh(&mlo_umac_reset->lock);
	mlo_umac_reset->umac_reset_info = 0;
	mlo_umac_reset->initiator_chip = 0;
	spin_unlock_bh(&mlo_umac_reset->lock);
}

int ath12k_umac_reset_send_htt(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr_aligned;
	enum ath12k_umac_reset_state next_state;
	bool is_initiator, is_target_recovery;
	int ret;

	shmem_vaddr_aligned = umac_reset->shmem_vaddr_aligned;

	switch(tx_event) {
	case ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE:
		ath12k_umac_reset_pre_reset_validation(ab, &is_initiator,
						       &is_target_recovery);
		ret = ath12k_htt_umac_reset_send_start_pre_reset_cmd(ab,
								     is_initiator,
								     is_target_recovery);
		if (ret) {
			ath12k_warn(ab, "Unable to send umac trigger ret %d\n", ret);
			return ret;
		}

		/* Transition to TRIGGER_SENT state */
		next_state = ATH12K_UMAC_RESET_STATE_TRIGGER_SENT;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_PRE_RESET_DONE_SET);
		/* Transition to PRE_RESET_DONE state */
		next_state = ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
						ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_START_DONE_SET);
		/* Transition to POST_RESET_DONE state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_DONE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	case ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE:
		shmem_vaddr_aligned->h2t_msg = u32_encode_bits(1,
				ATH12K_HTT_UMAC_RESET_MSG_SHMEM_POST_RESET_COMPLETE_DONE);
		/* Transition back to IDLE state */
		next_state = ATH12K_UMAC_RESET_STATE_IDLE;
		ath12k_umac_reset_state_transition(ab, next_state);
		break;
	default:
		ath12k_warn(ab, "Invalid tx_event: %d\n", tx_event);
		return -EINVAL;
        }

	if (tx_event == ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE) {
		ath12k_umac_reset_completion(ab);
		ath12k_info(ab, "MLO UMAC Recovery completed\n");
	}

	return 0;
}

/**
 * ath12k_umac_reset_invoke_post_send_cb - Invoke and clear post-send callback
 * @ab: Pointer to ath12k_base structure
 *
 * Reads the callback, clears it to NULL, then invokes it if non-NULL.
 * This ensures the callback is only called once per stage.
 */
static void ath12k_umac_reset_invoke_post_send_cb(struct ath12k_base *ab)
{
	void (*cb)(struct ath12k_base *ab);

	/* Read callback pointer safely */
	cb = READ_ONCE(ab->dp_umac_reset.post_send_cb);

	/* Clear callback before invocation to prevent re-entry */
	WRITE_ONCE(ab->dp_umac_reset.post_send_cb, NULL);

	/* Invoke callback if it was set */
	if (cb) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Invoking post-send callback\n");
		cb(ab);
	}
}

int ath12k_umac_reset_notify_target(struct ath12k_base *ab, int tx_event)
{
	struct ath12k_base *partner_ab;
	struct ath12k_hw_group *ag = ab->ag;
	int i, ret;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    (test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags) &&
		     !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &partner_ab->dev_flags)) ||
		    test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &partner_ab->dev_flags))
			continue;

		if (partner_ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE &&
		    ag->wsi_remap_in_progress) {
			continue;
		}

		ath12k_umac_reset_invoke_post_send_cb(partner_ab);

		/* Send HTT message to FW */
		ret = ath12k_umac_reset_send_htt(partner_ab, tx_event);
		if (ret) {
			ath12k_warn(partner_ab,
				    "Failed to send HTT message for tx_event %d (devide : %d): %d\n",
				    tx_event, partner_ab->device_id, ret);
			continue;
		}
	}

	return 0;
}

static bool ath12k_dp_umac_reset_fallback_needed(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	bool fb_needed = false;

	if (!ab->hw_params->support_umac_reset)
		return fb_needed;

	spin_lock_bh(&mlo_umac_reset->lock);

	if ((mlo_umac_reset->umac_reset_info & ATH12K_IS_UMAC_RESET_IN_PROGRESS) &&
	    !(mlo_umac_reset->umac_reset_info & ATH12K_IS_UMAC_RESET_FB_IN_PROGRESS)) {
		mlo_umac_reset->umac_reset_info |= ATH12K_IS_UMAC_RESET_FB_IN_PROGRESS;
		fb_needed = true;
	}

	spin_unlock_bh(&mlo_umac_reset->lock);

	return fb_needed;
}

bool ath12k_dp_umac_reset_in_progress(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	bool umac_in_progress = false;

	if (!ab->hw_params->support_umac_reset)
		return umac_in_progress;

	spin_lock_bh(&mlo_umac_reset->lock);
	if (mlo_umac_reset->umac_reset_info &
			ATH12K_IS_UMAC_RESET_IN_PROGRESS)
		umac_in_progress = true;
	spin_unlock_bh(&mlo_umac_reset->lock);

	return umac_in_progress;
}
EXPORT_SYMBOL(ath12k_dp_umac_reset_in_progress);

int ath12k_umac_reset_initiate_recovery(struct ath12k_base *ab,
					bool target_recovery)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	enum ath12k_umac_reset_state current_state;
	int ret, i;

	/* Check current state before initiating recovery */
	current_state = ath12k_umac_reset_get_state(ab);
	if (current_state != ATH12K_UMAC_RESET_STATE_IDLE) {
		ath12k_warn(ab, "Cannot initiate recovery from state %s, expected INIT\n",
			    ath12k_umac_reset_state_to_str(current_state));
		return -EINVAL;
	}

	spin_lock_bh(&mlo_umac_reset->lock);

	if (mlo_umac_reset->umac_reset_info & ATH12K_IS_UMAC_RESET_IN_PROGRESS) {
		spin_unlock_bh(&mlo_umac_reset->lock);
		ath12k_warn(ab, "UMAC RECOVERY IS IN PROGRESS\n");
		WARN_ON(1);
		return -ECANCELED;
	}

	/* Set flag to indicate UMAC recovery is in progress */
	mlo_umac_reset->umac_reset_info = ATH12K_IS_UMAC_RESET_IN_PROGRESS;

	 /* Set flag to indicate Target recovery if that is the case */
	if (target_recovery)
		mlo_umac_reset->umac_reset_info |= ATH12K_IS_UMAC_RESET_TYPE_RECOVERY;

	/* Reset task_map and task_id at start of a new recovery sequence */
	mlo_umac_reset->task_map = 0;
	atomic_set(&mlo_umac_reset->task_id, 0);
	mlo_umac_reset->initiator_chip = ab->device_id;

	for (i = 0; i < ag->num_devices; i++) {
		struct ath12k_base *partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    (test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags) &&
		    !partner_ab->skip_cumac_hw_reset))
			continue;

		/* Transition to INIT state */
		ret = ath12k_umac_reset_state_transition(partner_ab,
							 ATH12K_UMAC_RESET_STATE_INIT);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to INIT state\n");
			spin_unlock_bh(&mlo_umac_reset->lock);
			return -EINVAL;
		}
	}

	spin_unlock_bh(&mlo_umac_reset->lock);

	return 0;
}

void ath12k_umac_reset_notify_target_sync_and_send(struct ath12k_base *ab,
						   enum dp_umac_reset_tx_cmd tx_cmd,
						   int task_bit)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	unsigned long flags;
	bool should_send = false;

	/* Atomically clear bit and check if map is empty */
	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);

	clear_bit(task_bit, &mlo_umac_reset->task_map);

	/* Check if this was the last bit */
	if (bitmap_empty(&mlo_umac_reset->task_map, BITS_PER_LONG)) {
		should_send = true;
	}

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	/* Handle based on tx_cmd - check NONE first for early exit */
	if (tx_cmd == ATH12K_UMAC_RESET_TX_CMD_NONE) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Task %d complete (no cmd to send)\n", task_bit);
	} else if (should_send) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Task %d complete, all tasks done, sending notify for cmd %d\n",
			   task_bit, tx_cmd);
		ath12k_umac_reset_notify_target(ab, tx_cmd);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Task %d complete, tasks still pending\n", task_bit);
	}
}
EXPORT_SYMBOL(ath12k_umac_reset_notify_target_sync_and_send);

/* Dummy callback for CPU synchronization during pre-reset */
void ath12k_dummy_pre_reset_callback(struct ath12k_base *ab)
{
	/* This callback intentionally does nothing.
	 * Its purpose is to ensure that when it runs on a CPU,
	 * no ath12k_wifi_dp_service_srng instances are running
	 * on that CPU anymore (due to the early return check).
	 */

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Dummy pre-reset callback executed on CPU %d\n",
		   smp_processor_id());
}
EXPORT_SYMBOL(ath12k_dummy_pre_reset_callback);

/**
 * ath12k_umac_reset_set_post_send_cb - Set post-send callback
 * @ab: Pointer to ath12k_base structure
 * @cb: Callback function pointer (or NULL to clear)
 *
 * Sets the callback to be executed after FW message send completes.
 * Uses WRITE_ONCE for safe concurrent access.
 */
void ath12k_umac_reset_set_post_send_cb(struct ath12k_base *ab,
					void (*cb)(struct ath12k_base *ab))
{
	WRITE_ONCE(ab->dp_umac_reset.post_send_cb, cb);
}
EXPORT_SYMBOL(ath12k_umac_reset_set_post_send_cb);

void ath12k_umac_reset_handle_pre_reset(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_pre_reset)
		dp->arch_ops->umac_reset_handle_pre_reset(ab);
}

void ath12k_umac_reset_handle_post_reset_start(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_post_reset_start)
		dp->arch_ops->umac_reset_handle_post_reset_start(ab);
}

void ath12k_umac_reset_handle_post_reset_complete(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_post_reset_complete)
		dp->arch_ops->umac_reset_handle_post_reset_complete(ab);
}

static void ath12k_umac_reset_handle_init_recovery(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int tx_event = ATH12K_UMAC_RESET_TX_CMD_TRIGGER_DONE;

	if (dp && dp->arch_ops && dp->arch_ops->umac_reset_handle_init_recovery)
		dp->arch_ops->umac_reset_handle_init_recovery(ab);

	if (mlo_umac_reset->initiator_chip == ab->device_id)
		ath12k_umac_reset_notify_target(ab, tx_event);
}

/* Task queue management functions */

/**
 * ath12k_umac_reset_enqueue_task - Enqueue a task for multi-core processing
 * @ag: Hardware group
 * @callback: Function to execute
 * @ab: Device context
 * @event: Event type for debugging
 * @tx_cmd: Command to send to target after callback completion
 * @bound_cpu_id: CPU ID to bind task to, or ATH12K_UMAC_RESET_CPU_UNBOUND
 *
 * Returns: 0 on success, negative error code on failure
 */
int ath12k_umac_reset_enqueue_task(struct ath12k_hw_group *ag,
				   umac_reset_handler_fn callback,
				   struct ath12k_base *ab,
				   enum dp_umac_reset_recover_action event,
				   enum dp_umac_reset_tx_cmd tx_cmd,
				   int bound_cpu_id)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_umac_reset_task *task;
	unsigned long flags;
	int next_id;

	if (!callback || !ab)
		return -EINVAL;

	task = kzalloc(sizeof(*task), GFP_ATOMIC);
	if (!task)
		return -ENOMEM;

	/* Get next task_id and check bounds */
	next_id = atomic_inc_return(&mlo_umac_reset->task_id);

	/* Limit task_id to BITS_PER_LONG - 1 (reserve 0, max is 63 for 64-bit) */
	if (next_id >= BITS_PER_LONG) {
		ath12k_warn(ab, "Task queue full: task_id %d exceeds limit %d\n",
			    next_id, BITS_PER_LONG - 1);
		kfree(task);
		return -ENOSPC;
	}

	INIT_LIST_HEAD(&task->list);
	task->callback = callback;
	task->ab = ab;
	task->event = event;
	task->tx_cmd = tx_cmd;
	task->task_id = next_id;
	task->bound_cpu_id = bound_cpu_id;

	/* Use direct bit mapping (no modulo) - task_id is the bit position */
	set_bit(task->task_id, &mlo_umac_reset->task_map);

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);
	list_add_tail(&task->list, &mlo_umac_reset->task_queue);
	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Enqueued task %u for event %d with tx_cmd %d (bit %u set, bound_cpu: %d)\n",
		   task->task_id, event, tx_cmd, task->task_id, bound_cpu_id);

	return 0;
}
EXPORT_SYMBOL(ath12k_umac_reset_enqueue_task);

/**
 * ath12k_umac_reset_dequeue_task - Dequeue a task for specific CPU
 * @ag: Hardware group
 * @cpu: CPU ID to find task for
 *
 * Scans the queue and returns the first task that is either:
 * - Bound to the specified CPU
 * - Unbound (can run on any CPU)
 *
 * Returns: Task structure or NULL if no suitable task found
 */
struct ath12k_umac_reset_task *ath12k_umac_reset_dequeue_task(struct ath12k_hw_group *ag,
							      int cpu)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_umac_reset_task *task = NULL;
	struct ath12k_umac_reset_task *tmp;
	unsigned long flags;

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);

	/* Scan the queue for a task suitable for this CPU */
	list_for_each_entry(tmp, &mlo_umac_reset->task_queue, list) {
		if (tmp->bound_cpu_id == ATH12K_UMAC_RESET_CPU_UNBOUND ||
		    tmp->bound_cpu_id == cpu) {
			/* Found a suitable task */
			task = tmp;
			list_del(&task->list);
			break;
		}
	}

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	return task;
}

/**
 * ath12k_umac_reset_tasklet_handler_percpu - Per-CPU tasklet handler
 * @t: Tasklet structure
 *
 * Processes tasks from the queue using smart dequeue that automatically
 * finds tasks suitable for this CPU (either bound to this CPU or unbound).
 * This eliminates the need for skip tracking and re-enqueuing logic.
 */
void ath12k_umac_reset_tasklet_handler_percpu(struct tasklet_struct *t)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset;
	struct ath12k_umac_reset_task *task;
	struct ath12k_hw_group *ag;
	int cpu = smp_processor_id();

	/* Get the mlo_umac_reset structure from tasklet */
	mlo_umac_reset = container_of(t, struct ath12k_mlo_dp_umac_reset, tasklet[cpu]);
	ag = container_of(mlo_umac_reset, struct ath12k_hw_group, mlo_umac_reset);

	/* Process tasks until no more suitable tasks for this CPU */
	while ((task = ath12k_umac_reset_dequeue_task(ag, cpu)) != NULL) {
		if (task->callback) {
			ath12k_dbg(task->ab, ATH12K_DBG_DP_UMAC_RESET,
				   "CPU %d processing task %u (bound_cpu: %d) for event %d\n",
				   cpu, task->task_id, task->bound_cpu_id, task->event);

			task->callback(task->ab);

			/* Atomically clear bit and notify target if all tasks complete */
			ath12k_umac_reset_notify_target_sync_and_send(task->ab,
								      task->tx_cmd,
								      task->task_id);
		}
		kfree(task);
	}

	/* No more tasks for this CPU */
	ath12k_dbg(ag->ab[0], ATH12K_DBG_DP_UMAC_RESET,
		   "CPU %d: no more suitable tasks in queue\n", cpu);
}

/**
 * ath12k_umac_reset_schedule_all_tasklets - Schedule tasklets on all online CPUs
 * @ag: Hardware group
 *
 * Triggers SMP calls to schedule tasklets on all online CPUs if there are
 * pending tasks in the queue. This allows parallel processing of enqueued tasks.
 */
void ath12k_umac_reset_schedule_all_tasklets(struct ath12k_hw_group *ag)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	int cpu;

	/* Only schedule if there are tasks pending */
	if (list_empty(&mlo_umac_reset->task_queue))
		return;

	/* Trigger SMP calls to schedule tasklets on all online CPUs */
	for_each_online_cpu(cpu)
		smp_call_function_single_async(cpu, &mlo_umac_reset->csd[cpu]);
}
EXPORT_SYMBOL(ath12k_umac_reset_schedule_all_tasklets);

/* Static table mapping rx_event to handler functions */
static const umac_reset_handler_fn umac_reset_handlers[] = {
	[ATH12K_UMAC_RESET_RX_EVENT_NONE] = NULL,
	[ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY] =
					ath12k_umac_reset_handle_init_recovery,
	[ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC] =
					ath12k_umac_reset_handle_init_recovery,
	[ATH12K_UMAC_RESET_DO_PRE_RESET] = ath12k_umac_reset_handle_pre_reset,
	[ATH12K_UMAC_RESET_DO_POST_RESET_START] =
					ath12k_umac_reset_handle_post_reset_start,
	[ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE] =
					ath12k_umac_reset_handle_post_reset_complete,
};

/* Static table mapping rx_event to corresponding tx_cmd */
static const enum dp_umac_reset_tx_cmd umac_reset_rx_to_tx_map[] = {
	[ATH12K_UMAC_RESET_RX_EVENT_NONE] = ATH12K_UMAC_RESET_TX_CMD_NONE,
	[ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY] = ATH12K_UMAC_RESET_TX_CMD_NONE,
	[ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC] =
					ATH12K_UMAC_RESET_TX_CMD_NONE,
	[ATH12K_UMAC_RESET_DO_PRE_RESET] = ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE,
	[ATH12K_UMAC_RESET_DO_POST_RESET_START] =
					ATH12K_UMAC_RESET_TX_CMD_POST_RESET_START_DONE,
	[ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE] =
					ATH12K_UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE,
};

static int
ath12k_dp_umac_reset_check_n_change_state(struct ath12k_base *ab,
					  enum dp_umac_reset_recover_action rx_event)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	bool target_recovery = false;
	enum ath12k_umac_reset_state next_state;
	int ret = 0;

	/* Validate rx_event is within valid range */
	if (rx_event <= ATH12K_UMAC_RESET_RX_EVENT_NONE ||
	    rx_event >= ARRAY_SIZE(umac_reset_handlers)) {
		ath12k_warn(ab, "Invalid UMAC RESET event: %d\n", rx_event);
		return -EINVAL;
	}

	switch(rx_event) {
	case ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC:
		target_recovery = true;
		fallthrough;
	case ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY:
		ret = ath12k_umac_reset_initiate_recovery(ab, target_recovery);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to initate Umac recovery\n");
			break;
		}
		atomic_set(&mlo_umac_reset->request_chip, ag->num_started);

		break;
	case ATH12K_UMAC_RESET_DO_PRE_RESET:
		/* Transition to PRE_RESET_START state */
		next_state = ATH12K_UMAC_RESET_STATE_PRE_RESET_START;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to PRE_RESET_START state\n");
			break;
		}

		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_START:
		/* Transition to POST_RESET_START state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_START;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to POST_RESET_START state\n");
			break;
		}
		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	case ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE:
		/* Transition to POST_RESET_COMPLETE state */
		next_state = ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE;
		ret = ath12k_umac_reset_state_transition(ab, next_state);
		if (ret) {
			ath12k_warn(ab, "Failed to transition to POST_RESET_COMPLETE state\n");
			break;
		}
		atomic_inc(&mlo_umac_reset->request_chip);
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Unknown UMAC RESET event received\n");
		break;
	}

	return ret;
}

irqreturn_t ath12k_umac_reset_interrupt_handler(int irq, void *arg)
{
	struct ath12k_base *ab = arg;
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr;
	u32 t2h_msg;
	u64 timestamp;
	bool has_valid_event = false;
	int htt_msg;

	shmem_vaddr = umac_reset->shmem_vaddr_aligned;

	/* Validate shared memory */
	if (!shmem_vaddr || shmem_vaddr->magic_num != umac_reset->magic_num) {
		/* Spurious interrupt - no valid shared memory */
		return IRQ_HANDLED;
	}

	/* Read event message */
	t2h_msg = shmem_vaddr->t2h_msg;

	/* Check if any valid events are present */
	if (!t2h_msg) {
		/* No events - spurious interrupt */
		return IRQ_HANDLED;
	}

	/* Capture timestamp immediately for all events */
	timestamp = jiffies_to_msecs(jiffies);

	/* Check for valid events and record per-event timestamps */
	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY)) {
		umac_reset->ts.event_irq_init_umac_recovery = timestamp;
		has_valid_event = true;
	}

	htt_msg = HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC;
	if (u32_get_bits(t2h_msg, htt_msg)) {
		umac_reset->ts.event_irq_init_target_recovery = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET)) {
		umac_reset->ts.event_irq_pre_reset = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START)) {
		umac_reset->ts.event_irq_post_reset_start = timestamp;
		has_valid_event = true;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE)) {
		umac_reset->ts.event_irq_post_reset_complete = timestamp;
		has_valid_event = true;
	}

	/* Only schedule tasklet if we have valid events */
	if (has_valid_event)
		tasklet_schedule(&umac_reset->intr_tq);

	return IRQ_HANDLED;
}

void ath12k_dp_umac_reset_handle(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_dp_htt_umac_reset_recovery_msg_shmem_t *shmem_vaddr;
	struct ath12k_base *partner_ab;
	enum ath12k_umac_reset_state current_state;
	enum dp_umac_reset_tx_cmd tx_cmd;
	int rx_event, num_event = 0;
	unsigned long flags;
	u32 t2h_msg;
	int i;

	shmem_vaddr = umac_reset->shmem_vaddr_aligned;
	if (!shmem_vaddr) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address NULL\n");
		return;
	}

	if (shmem_vaddr->magic_num != umac_reset->magic_num) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Shared memory address is invalid shmem:0x%x u:0x%x\n",
			   shmem_vaddr->magic_num, umac_reset->magic_num);
		return;
	}

	/* Log current state for debugging */
	current_state = ath12k_umac_reset_get_state(ab);
	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Processing UMAC reset event in state: %s\n",
		   ath12k_umac_reset_state_to_str(current_state));

	t2h_msg = shmem_vaddr->t2h_msg;
	shmem_vaddr->t2h_msg = 0;

	rx_event = ATH12K_UMAC_RESET_RX_EVENT_NONE;

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_UMAC_RECOVERY)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_UMAC_RECOVERY;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_INIT_TARGET_RECOVERY_SYNC_USING_UMAC)) {
		rx_event |= ATH12K_UMAC_RESET_INIT_TARGET_RECOVERY_SYNC_USING_UMAC;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_PRE_RESET)) {
		rx_event |= ATH12K_UMAC_RESET_DO_PRE_RESET;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_START)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_START;
		num_event++;
	}

	if (u32_get_bits(t2h_msg, HTT_ATH12K_UMAC_RESET_T2H_DO_POST_RESET_COMPLETE)) {
		rx_event |= ATH12K_UMAC_RESET_DO_POST_RESET_COMPLETE;
		num_event++;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET, "Deduced rx event:%d num:%d\n", rx_event, num_event);

	if (num_event > 1) {
		ath12k_warn(ab, "Multiple events notified in single msg while in state %s\n",
			    ath12k_umac_reset_state_to_str(current_state));
		WARN_ON_ONCE(1);
		return;
	}

	/* First, validate the event */
	if (ath12k_dp_umac_reset_check_n_change_state(ab, rx_event)) {
		ath12k_warn(ab, "UMAC reset event validation failed for event %d\n",
			    rx_event);
		return;
	}

	/* Check if all chips have sent requests */
	if (atomic_read(&mlo_umac_reset->request_chip) < ag->num_started) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Not all chips ready: request_chip=%d num_started=%d, exiting early\n",
			   atomic_read(&mlo_umac_reset->request_chip), ag->num_started);
		return;
	}

	/* Reset request counter after processing all chips */
	atomic_set(&mlo_umac_reset->request_chip, 0);

	/* All chips have sent requests - process event for entire group serially */
	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "All chips ready: request_chip=%d num_started=%d, processing event for entire group\n",
		   atomic_read(&mlo_umac_reset->request_chip), ag->num_started);

	/* set the reserved bit 0 to hold the premature execution of enqueued tasks.
	 * Without this there is a possibility of task_map becoming 0 before other tasks
	 * are even enqueued and we end up sending response to firmware
	 */
	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);

	set_bit(0, &mlo_umac_reset->task_map);

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	/* Process event for all partner devices in the group */
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (partner_ab->is_bypassed ||
		    (test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags) &&
		     !partner_ab->skip_cumac_hw_reset))
			continue;

		if (umac_reset_handlers[rx_event])
			umac_reset_handlers[rx_event](partner_ab);
	}

	/* After handlers, schedule tasklets if tasks were enqueued */
	ath12k_umac_reset_schedule_all_tasklets(ag);

	/* At this poing we are assured that all tasks are enququed
	 * and there is no premature response to firmware
	 */

	/* Look up the corresponding TX command for this RX event */
	tx_cmd = umac_reset_rx_to_tx_map[rx_event];

	/* Atomically clear reserved bit 0 and notify target if all tasks complete */
	ath12k_umac_reset_notify_target_sync_and_send(ab, tx_cmd, 0);
}

void ath12k_umac_reset_tasklet_handler(struct tasklet_struct *umac_cntxt)
{
	struct ath12k_dp_umac_reset *umac_reset = from_tasklet(umac_reset, umac_cntxt, intr_tq);
	struct ath12k_base *ab = container_of(umac_reset, struct ath12k_base, dp_umac_reset);

	ath12k_hif_dp_umac_intr_line_reset(ab);
	ath12k_dp_umac_reset_handle(ab);
}

/**
 * ath12k_umac_reset_free_skb_queues - Free accumulated SKBs
 * @ab: Pointer to ath12k_base structure
 *
 * Frees all SKBs that were accumulated in tx_skb_queue and rx_skb_queue
 * during UMAC reset flow. These queues hold packets that couldn't be
 * processed due to the reset.
 */
static void ath12k_umac_reset_free_skb_queues(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	struct sk_buff *skb;
	u32 tx_count = 0, rx_count = 0;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 ppeds_tx_count = 0;
#endif

	/* Free all saved TX SKBs */
	while ((skb = skb_dequeue(&umac_reset->tx_skb_queue)) != NULL) {
		dev_kfree_skb_any(skb);
		tx_count++;
	}

	/* Free all saved RX SKBs */
	while ((skb = skb_dequeue(&umac_reset->rx_skb_queue)) != NULL) {
		dev_kfree_skb_any(skb);
		rx_count++;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Free all saved PPEDS TX SKBs */
	while ((skb = skb_dequeue(&umac_reset->ppeds_tx_skb_queue)) != NULL) {
		dev_kfree_skb_any(skb);
		ppeds_tx_count++;
	}

	if (ppeds_tx_count)
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Freed %u TX SKBs during fallback cleanup\n",
			   ppeds_tx_count);
#endif
	if (tx_count || rx_count)
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Freed %u TX SKBs and %u RX SKBs during fallback cleanup\n",
			   tx_count, rx_count);
}

/**
 * ath12k_umac_reset_clear_task_queue - Clear pending task queue
 * @ag: Pointer to hardware group
 *
 * Clears all pending tasks from the UMAC reset task queue and resets
 * the task_map bitmap. This ensures no stale tasks remain after a
 * failed reset attempt.
 */
static void ath12k_umac_reset_clear_task_queue(struct ath12k_hw_group *ag)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	struct ath12k_umac_reset_task *task, *tmp;
	unsigned long flags;
	u32 cleared_count = 0;

	spin_lock_irqsave(&mlo_umac_reset->task_queue_lock, flags);

	/* Clear all pending tasks */
	list_for_each_entry_safe(task, tmp, &mlo_umac_reset->task_queue, list) {
		list_del(&task->list);
		kfree(task);
		cleared_count++;
	}

	/* Reset task_map bitmap */
	mlo_umac_reset->task_map = 0;

	/* Reset task_id counter */
	atomic_set(&mlo_umac_reset->task_id, 0);

	spin_unlock_irqrestore(&mlo_umac_reset->task_queue_lock, flags);

	if (cleared_count)
		ath12k_dbg(ag->ab[0], ATH12K_DBG_DP_UMAC_RESET,
			   "Cleared %u pending tasks from queue\n", cleared_count);
}

/**
 * ath12k_umac_reset_restore_irqs - Re-enable IRQs if they were disabled
 * @ab: Pointer to ath12k_base structure
 * @state: Current UMAC reset state
 *
 * Re-enables IRQs that were disabled during UMAC reset flow based on
 * the state where the reset got stuck.
 */
static void ath12k_umac_reset_restore_irqs(struct ath12k_base *ab,
					   enum ath12k_umac_reset_state state)
{
	/* IRQs are disabled during PRE_RESET phase */
	if (state >= ATH12K_UMAC_RESET_STATE_PRE_RESET_START &&
	    state < ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE) {
		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Re-enabling IRQs during fallback cleanup\n");

		ath12k_hif_irq_enable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags)) {
			ab->dp->ppe.ppe_ops->ath12k_ppeds_start(ab);
			ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start(ab);
		}
#endif
		ath12k_hif_mgmt_irq_enable(ab);
	}
}

/**
 * ath12k_umac_reset_cleanup_from_state - Perform state-specific cleanup
 * @ab: Pointer to ath12k_base structure
 * @state: Current UMAC reset state
 *
 * Performs cleanup operations specific to the state where UMAC reset
 * got stuck. Different states require different cleanup actions.
 */
static void ath12k_umac_reset_cleanup_from_state(struct ath12k_base *ab,
						 enum ath12k_umac_reset_state state)
{
	struct ath12k_hw_group *ag = ab->ag;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	struct dp_ppe_ds_idxs idx;
#endif

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Performing fallback cleanup from state: %s\n",
		   ath12k_umac_reset_state_to_str(state));

	switch (state) {
	case ATH12K_UMAC_RESET_STATE_IDLE:
	case ATH12K_UMAC_RESET_STATE_INIT:
	case ATH12K_UMAC_RESET_STATE_TRIGGER_SENT:
	case ATH12K_UMAC_RESET_STATE_PRE_RESET_START:
		/* Nothing to clean up */
		break;

	case ATH12K_UMAC_RESET_STATE_PRE_RESET_DONE:
	case ATH12K_UMAC_RESET_STATE_POST_RESET_START:
		ath12k_umac_reset_clear_task_queue(ag);
		/* A dummy registration is needed to avoid breaking
		 * the state machine at the DS module
		 */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
		    (ab->dp->ppe.ppe_ops &&
		     (ab->dp->ppe.ppe_ops->ath12k_ppeds_register_soc)))
			ab->dp->ppe.ppe_ops->ath12k_ppeds_register_soc(ab->dp, &idx);
#endif
		ath12k_umac_reset_restore_irqs(ab, state);
		break;

	case ATH12K_UMAC_RESET_STATE_POST_RESET_DONE:
		/* Hardware rings modified, IRQs still disabled */
		ath12k_umac_reset_free_skb_queues(ab);
		ath12k_umac_reset_clear_task_queue(ag);
		ath12k_umac_reset_restore_irqs(ab, state);
		break;

	case ATH12K_UMAC_RESET_STATE_POST_RESET_COMPLETE:
		/* Almost complete - just free SKBs */
		ath12k_umac_reset_free_skb_queues(ab);
		ath12k_umac_reset_clear_task_queue(ag);
		break;

	case ATH12K_UMAC_RESET_STATE_ERROR:
		/* Already in error state - just clean up resources */
		ath12k_umac_reset_free_skb_queues(ab);
		ath12k_umac_reset_clear_task_queue(ag);
		ath12k_warn(ab, "Invalid UMAC reset state: %d\n", state);
		break;

	default:
		ath12k_warn(ab, "Unknown UMAC reset state: %d\n", state);
		break;
	}
}

/**
 * ath12k_umac_reset_transition_to_idle - Transition state machine to IDLE
 * @ab: Pointer to ath12k_base structure
 * @current_state: Current UMAC reset state
 *
 * Transitions the UMAC reset state machine from current state to ERROR
 * state, and then to IDLE state. This ensures proper state machine
 * cleanup even when firmware doesn't respond.
 */
static
void ath12k_umac_reset_transition_to_idle(struct ath12k_base *ab,
					  enum ath12k_umac_reset_state current_state)
{
	struct ath12k_dp_umac_reset *umac_reset = &ab->dp_umac_reset;
	unsigned long flags;

	if (current_state == ATH12K_UMAC_RESET_STATE_IDLE)
		return;

	spin_lock_irqsave(&umac_reset->state_lock, flags);

	/* Transition to ERROR state if not already there */
	if (current_state != ATH12K_UMAC_RESET_STATE_ERROR) {
		umac_reset->prev_state = current_state;
		umac_reset->current_state = ATH12K_UMAC_RESET_STATE_ERROR;
		umac_reset->state_error_count++;
		umac_reset->error_from_state = current_state;
		umac_reset->state_transition_count[ATH12K_UMAC_RESET_STATE_ERROR]++;
		umac_reset->state_entry_time[ATH12K_UMAC_RESET_STATE_ERROR] =
			jiffies_to_msecs(jiffies);

		ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
			   "Fallback: Transitioned to ERROR state from %s\n",
			   ath12k_umac_reset_state_to_str(current_state));
	}

	/* Transition from ERROR to IDLE */
	umac_reset->prev_state = ATH12K_UMAC_RESET_STATE_ERROR;
	umac_reset->current_state = ATH12K_UMAC_RESET_STATE_IDLE;
	umac_reset->state_transition_count[ATH12K_UMAC_RESET_STATE_IDLE]++;
	umac_reset->state_entry_time[ATH12K_UMAC_RESET_STATE_IDLE] =
		jiffies_to_msecs(jiffies);

	spin_unlock_irqrestore(&umac_reset->state_lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Fallback: Transitioned to IDLE state\n");
}

/**
 * ath12k_umac_reset_clear_mlo_flags - Clear MLO reset flags and counters
 * @ab: Pointer to ath12k_base structure
 *
 * Clears MLO-specific UMAC reset flags and resets counters to ensure
 * clean state after fallback cleanup.
 */
static void ath12k_umac_reset_clear_mlo_flags(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ag->mlo_umac_reset;
	unsigned long flags;

	spin_lock_irqsave(&mlo_umac_reset->lock, flags);

	/* Clear UMAC reset in progress flag */
	mlo_umac_reset->umac_reset_info = 0;

	/* Clear initiator chip ID */
	mlo_umac_reset->initiator_chip = 0;

	/* Reset request chip counter */
	atomic_set(&mlo_umac_reset->request_chip, 0);

	spin_unlock_irqrestore(&mlo_umac_reset->lock, flags);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Cleared MLO UMAC reset flags\n");
}

/**
 * ath12k_umac_reset_fallback_cleanup - Main fallback cleanup function
 * @ab: Pointer to ath12k_base structure
 *
 * This is the main entry point for UMAC reset fallback cleanup. It is
 * called from ath12k_core_cleanup() when the system is being torn down
 * and UMAC reset may not have completed successfully.
 *
 * The function:
 * 1. Checks if UMAC reset was in progress
 * 2. Performs state-aware cleanup based on current state
 * 3. Frees accumulated resources (SKBs, tasks)
 * 4. Restores IRQs if needed
 * 5. Transitions state machine to IDLE
 * 6. Clears MLO reset flags
 * 7. Calls ath12k_umac_reset_completion() to finish cleanup
 */
static void __ath12k_umac_reset_fallback_cleanup(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	enum ath12k_umac_reset_state current_state;
	bool was_in_progress = false;

	if (!ab->hw_params->support_umac_reset)
		return;

	if (!ag) {
		ath12k_warn(ab, "Hardware group not available for fallback cleanup\n");
		return;
	}

	/* Get current state */
	current_state = ath12k_umac_reset_get_state(ab);

	/* Check if UMAC reset was in progress */
	if (current_state != ATH12K_UMAC_RESET_STATE_IDLE) {
		was_in_progress = true;
		ath12k_warn(ab, "UMAC reset incomplete at state %s, performing fallback cleanup\n",
			    ath12k_umac_reset_state_to_str(current_state));
	}

	/* Perform state-specific cleanup */
	if (was_in_progress) {
		ath12k_umac_reset_cleanup_from_state(ab, current_state);

		/* Transition state machine to IDLE */
		ath12k_umac_reset_transition_to_idle(ab, current_state);

		/* Clear the UMAC recovery in progress flag */
		clear_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

		ath12k_info(ab, "UMAC reset fallback cleanup completed\n");
	}
}

void ath12k_umac_reset_fallback_cleanup(struct ath12k_base *ab)
{
	struct ath12k_base *partner_ab;
	struct ath12k_hw_group *ag = ab->ag;
	int i;

	if (!ath12k_dp_umac_reset_fallback_needed(ab))
		return;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		__ath12k_umac_reset_fallback_cleanup(partner_ab);
	}

	/* Clear MLO reset flags */
	ath12k_umac_reset_clear_mlo_flags(ab);
}
EXPORT_SYMBOL(ath12k_umac_reset_fallback_cleanup);

void ath12k_dp_umac_reset_deinit(struct ath12k_base *ab)
{
	struct ath12k_dp_umac_reset *umac_reset;

	if (!ab->hw_params->support_umac_reset ||
	    test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	umac_reset = &ab->dp_umac_reset;

	if (!umac_reset->shmem_vaddr_unaligned)
		return;

	ath12k_hif_dp_umac_reset_free_irq(ab);
	ath12k_umcmn_irq_free(ab);
	ath12k_umcmn_timer_free(ab);

	dma_free_coherent(ab->dev,
			  umac_reset->shmem_size,
			  umac_reset->shmem_vaddr_unaligned,
			  umac_reset->shmem_paddr_unaligned);

	umac_reset->shmem_vaddr_unaligned = NULL;
	ab->umac_reset_init_done = false;
}
