// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_tx.h"
#include "../peer.h"
#include "dp_tx.h"
#include "dp.h"
#include "hal_rx.h"
#include "hal_desc.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../sdwf.h"
#include "../dp_stats.h"
#include "../dp_peer.h"
#include "../telemetry.h"
#include "../telemetry_agent_if.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"
#include <linux/vmalloc.h>
#include "../ppe_public.h"
#include "ppeds.h"

#define ATH12K_HW_MAX_ACTIVE_QUEUES	3

struct ath12k_tx_sw_metadata {
	struct sk_buff *skb;
	u64 paddr      : 40,
	    len        : 16,
	    hw_link_id : 5,
	    mmesh      : 1,
	    rsvd1      : 2;
	u8  flags      : 4,
	    rsvd2      : 4;
	u32 hw_enqueue_tstamp;
} __packed __aligned(32);

static_assert(sizeof(struct ath12k_tx_sw_metadata) == 32,
	      "ath12k_tx_sw_metadata size != 32");

struct ath12k_wifi8_tx_status_entry {
	struct hal_tqm2sw_completion_ring tx_status;
	union {
		struct ath12k_tx_sw_metadata sw_metadata;
		void *tx_desc;
	};
} __packed;

static_assert(sizeof(struct ath12k_wifi8_tx_status_entry) == 64,
	      "ath12k_wifi8_tx_status_entry size != 64");

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_pkt_stats[reason]++;
	tstats->tid_stats[tid].tx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}

void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_drop_stats[reason]++;
	tstats->tid_stats[tid].tx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}

int ath12k_wifi8_dp_tqm_cmd_send(struct ath12k_base *ab,
				 enum hal_tlv_tag_be type,
				 struct ath12k_hal_tqm_cmd *cmd,
				 struct ath12k_dp_tx_queue *data,
				 void (*callback_fn)(struct ath12k_dp *dp,
				 void *ctx, struct hal_tqm_status *tqm_status))
{
	struct ath12k_dp *dp = ath12k_get_central_dp(ab->dp);
	struct ath12k_dp_tqm_cmd *dp_cmd = NULL;
	struct ath12k_dp_wifi8 *dp_wifi8;
	struct hal_srng *cmd_ring;
	int cmd_num;

	if (!dp)
		return -ENODEV;

	/* Switch to the central ab */
	ab = dp->ab;
	dp_wifi8 = ath12k_get_dp_wifi8(dp);

	if ((test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) &&
	     !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags)) ||
	    test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	if (callback_fn) {
		dp_cmd = kzalloc(sizeof(*dp_cmd), GFP_ATOMIC);
		if (!dp_cmd)
			return -ENOMEM;

		dp_cmd->cmd_type = type;
		dp_cmd->handler = callback_fn;
		memcpy(&dp_cmd->data, data, sizeof(*data));
		dp_cmd->cmd_num = 0;
		spin_lock_bh(&dp->tqm_cmd_lock);
		list_add_tail(&dp_cmd->list, &dp->tqm_cmd_list);
		spin_unlock_bh(&dp->tqm_cmd_lock);
	}

	cmd_ring = &ab->hal.srng_list[dp_wifi8->tqm_cmd_ring.ring_id];
	cmd_num = ath12k_wifi8_hal_tqm_cmd_send(ab, cmd_ring, type, cmd);

	//error, hence return error code
	if (cmd_num < 0) {
		ath12k_warn(ab, "Failed to send TQM command: %d", cmd_num);
		if (dp_cmd) {
			spin_lock_bh(&dp->tqm_cmd_lock);
			list_del(&dp_cmd->list);
			spin_unlock_bh(&dp->tqm_cmd_lock);
			kfree(dp_cmd);
		}
		return cmd_num;
	}

	if (!dp_cmd)
		return 0;

	/* cmd_num starts from 1; 0 means the HW ring was full or the
	 * command was not written.
	 */
	if (cmd_num == 0) {
		ath12k_warn(ab, "TQM command returned zero cmd_num");
		spin_lock_bh(&dp->tqm_cmd_lock);
		list_del(&dp_cmd->list);
		spin_unlock_bh(&dp->tqm_cmd_lock);
		kfree(dp_cmd);
		return -EINVAL;
	}

	/* Publish the real cmd_num.  The status-ring processor uses
	 * READ_ONCE() on cmd_num so it will not match this entry until
	 * the store is visible, preventing a false-positive match on the
	 * sentinel value 0.
	 */
	WRITE_ONCE(dp_cmd->cmd_num, cmd_num);

	return 0;
}

//to be called from dp->hw_params->ring_mask->tqm_status
int ath12k_wifi8_dp_tx_process_tqm_status(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	struct ath12k_dp_tqm_cmd *cmd, *tmp;
	struct hal_tqm_status tqm_status;
	bool found = false;
	int quota = budget;
	u16 tag;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	srng = &ab->hal.srng_list[dp_wifi8->tqm_status_ring.ring_id];

	memset(&tqm_status, 0, sizeof(tqm_status));

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- && (hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		switch (tag) {
		case HAL_TQM_REMOVE_MSDU_STATUS_BO:
			ath12k_wifi8_hal_tqm_remove_msdu_status(ab, hdr,
								&tqm_status);
			break;
		case HAL_TQM_REMOVE_MPDU_STATUS_BO:
			ath12k_wifi8_hal_tqm_remove_mpdu_status(ab, hdr,
								&tqm_status);
			break;
		case HAL_TQM_SYNC_CMD_STATUS_BO:
			ath12k_wifi8_hal_tqm_sync_cmd_status(ab, hdr,
							     &tqm_status);
			break;
		case HAL_TQM_GET_MPDUQ_STATS_STATUS_BO:
			ath12k_wifi8_hal_tqm_get_mpduq_stats_cmd_status(ab,
									hdr,
									&tqm_status);
			break;
		case HAL_TQM_UPDATE_MSDUQ_STATUS_BO:
			ath12k_wifi8_hal_tqm_update_msduq_cmd_status(ab, hdr,
								     &tqm_status);
			break;
		case HAL_TQM_UPDATE_MPDUQ_STATUS_BO:
			ath12k_wifi8_hal_tqm_update_mpduq_cmd_status(ab,
								     hdr,
								     &tqm_status);
			break;
		default:
			ath12k_warn(ab, "unknown tqm status type %d", tag);
			continue;
		}

		spin_lock_bh(&dp->tqm_cmd_lock);
		list_for_each_entry_safe(cmd, tmp, &dp->tqm_cmd_list, list) {
			if (READ_ONCE(cmd->cmd_num) &&
			    tqm_status.status_hdr.status_num == READ_ONCE(cmd->cmd_num)) {
				found = true;
				list_del(&cmd->list);
				break;
			}
		}
		spin_unlock_bh(&dp->tqm_cmd_lock);

		if (found) {
			cmd->handler(dp, (void *)&cmd->data, &tqm_status);
			kfree(cmd);
		}
		found = false;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

void ath12k_wifi8_dp_peer_tqm_sync_release(struct kref *ref)
{
	struct ath12k_dp_peer *dp_peer = container_of(ref, struct ath12k_dp_peer,
						      refcount);

	ath12k_dbg_level(NULL, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-cleanup-sync: EXIT peer_id=%u cleanup complete\n",
			 dp_peer->peer_id);

	ath12k_wifi8_dp_peer_release(ref);
}

void ath12k_dp_peer_cleanup_tqm_sync(struct ath12k_dp *dp, void *ctx,
				     struct hal_tqm_status *tqm_status)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_tx_queue *data = ctx;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_peer *dp_peer = NULL, *t;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	u8 pdev_id;
	u8 hw_link_id = data->hw_link_id;
	u16 peer_id = data->peer_id;

	/* NULL tqm_status means the TQM status ring was not drained before
	 * cleanup (e.g. wifi down races a pending TQM_SYNC_CMD).  Treat it
	 * as a forced cleanup so peer_ext_ctx is not leaked.
	 */
	if (tqm_status &&
	    tqm_status->status_hdr.cmd_execution_status != HAL_TQM_SUCCESSFUL_EXECUTION) {
		ath12k_err(ab, "Error: TQM STATUS FAILED with reason %d",
			   tqm_status->status_hdr.cmd_execution_status);
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-cleanup-sync: TQM status successful, proceeding with cleanup\n");

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
					      dp_hw_grp->hw_links[hw_link_id].pdev_idx);
	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "tqm-cleanup-sync: dp_pdev NULL, EXIT\n");
		return;
	}

	dp_hw = dp_pdev->dp_hw;
	if (!dp_hw) {
		rcu_read_unlock();
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "tqm-cleanup-sync: dp_hw NULL, EXIT\n");
		return;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	list_for_each_entry(t, &dp_hw->tqm_cleanup_list, tqm_cleanup_node) {
		if (t->peer_id == peer_id) {
			dp_peer = t;
			break;
		}
	}

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-cleanup-sync: found dp_peer %pM peer_state=%d peer_ext_ctx=%p\n",
			 dp_peer->addr, dp_peer->dp_peer_state, dp_peer->peer_ext_ctx);

	/* SMD case: peer_ext_ctx was already detached and parked for target AP.
	 * Skip freeing resources as they will be reused by target AP peer.
	 */
	if (!dp_peer->peer_ext_ctx) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "tqm-cleanup-sync: peer_ext_ctx NULL (SMD, resources transferred)\n");
		goto update_peer_state;
	}

	/*
	 * SMD BSS Transition: protect peer_ext_ctx from being freed prematurely
	 * in two related scenarios:
	 *
	 * (1) PREP not yet started (or in progress before prep_transfer_ext_ctx):
	 *     A partner link is removed during PREP setup (assoc_success path).
	 *     The per-link TXQ teardown sends TQM REMOVE+SYNC even though the
	 *     peer still has other active hw-links (peer_links_map non-zero).
	 *     Free only at the last link — when peer_links_map == 0.
	 *
	 * (2) PREP already parked ext_ctx (prep_transfer_ext_ctx ran):
	 *     smd_parked_ext_ctx == dp_peer->peer_ext_ctx means ext_ctx is now
	 *     owned by the target peer via smd_parked_ext_ctx.  Do not free.
	 *
	 * Both conditions apply only to STA BSS peers (the only peers that
	 * carry peer_ext_ctx through SMD BSS Transition).
	 */
	if (dp_peer->is_sta_bss_peer) {
		bool parked;

		spin_lock_bh(&dp->dp_hw_grp->smd_transition_lock);
		parked = (dp->dp_hw_grp->smd_parked_ext_ctx &&
			  dp->dp_hw_grp->smd_parked_ext_ctx ==
			  dp_peer->peer_ext_ctx);
		spin_unlock_bh(&dp->dp_hw_grp->smd_transition_lock);

		if (parked || dp_peer->peer_links_map) {
			ath12k_dbg(ab, ATH12K_DBG_SMD | ATH12K_DBG_PEER,
				   "tqm-cleanup-sync: SMD PREP in progress for peer %pM id=%u (parked=%d links_map=0x%x) — defer ext_ctx free\n",
				   dp_peer->addr, peer_id, parked,
				   dp_peer->peer_links_map);
			goto update_peer_state;
		}
	}

	if (dp_peer->dp_peer_state < ATH12K_DP_PEER_LOGICALLY_DELETED) {
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L2,
				 "tqm-cleanup-sync: deleting AST entry ast_index=%u\n",
				 dp_peer->peer_ext_ctx->ast_index);
		ath12k_dp_ast_entry_delete(dp->dp_hw_grp,
					   dp_peer->peer_ext_ctx->ast_index);
	}

update_peer_state:

	dp_peer->dp_peer_state = ATH12K_DP_PEER_LOGICALLY_DELETED;

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "tqm-cleanup-sync: setting peer_state to LOGICALLY_DELETED\n");

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	rcu_read_unlock();

	kref_put(&dp_peer->refcount, ath12k_wifi8_dp_peer_tqm_sync_release);
}

static inline u32 ath12k_qos_get_metadata(u16 qos_id)
{
	u32 tcl_metadata = 0;

	tcl_metadata = u32_encode_bits(HTT_TCL_META_DATA_TYPE_SVC_ID_BASED,
				       HTT_TCL_META_DATA_TYPE_V3) |
			u32_encode_bits(1, HTT_TCL_META_DATA_SAWF_TID_OVERRIDE_V3) |
			u32_encode_bits(qos_id, HTT_TCL_META_DATA_SAWF_SVC_ID_V3);
	return tcl_metadata;
}

/* TODO: SAWF Team to review */
static inline
void ath12k_wifi_qos_desc(struct hal_tcl_data_cmd *desc,
			  u32 msduq, u16 qos_id)
{
	u32 tid, flow_override, who_classify_info_sel;
	u32 meta_data_flags;

	tid = u32_get_bits(msduq, MSDUQ_TID);
	flow_override = u32_get_bits(msduq, MSDUQ_FLOW_OVERRIDE);
	who_classify_info_sel = u32_get_bits(msduq, MSDUQ_WHO_CL_INFO);

	desc->info1 |= u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		       u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE);

	desc->info2 |= u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		       u32_encode_bits(who_classify_info_sel,
				       HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL);

	meta_data_flags = ath12k_qos_get_metadata(qos_id);
	desc->info3 = u32_encode_bits(flow_override,
				      HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT);
	desc->tcl_cmd_number = cpu_to_le32(meta_data_flags);
}

static inline
void ath12k_wifi_qos_hlos_tid(struct hal_tcl_data_cmd *desc,
			      u8 tid)
{
	desc->info1 |= u32_encode_bits(tid < ATH12K_MAX_TID_VALUE ? tid : 0,
				       HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		       u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE);
}

static inline u8 ath12k_get_qos_tag(u32 mark)
{
	u8 qos_tag = u32_get_bits(mark, QOS_TAG_MASK);

	if (qos_tag == QOS_SCS_TAG || qos_tag == QOS_MSCS_TAG)
		return qos_tag;

	return 0;
}

static inline void
ath12k_wifi8_dp_qos_update(struct ath12k_dp *dp, struct ath12k_pdev_dp *dp_pdev,
			   u32 mark, struct hal_tcl_data_cmd *desc, u8 qos_tag,
			   struct ath12k_dp_peer *dp_peer, u8 link_id)
{
	u8 scs_id;
	u8 msduq;
	u16 peer_id;
	u16 qos_id = QOS_ID_MAX;
	int ret;

	if (mark & SDWF_VALID_MASK) {
		rcu_read_lock();
		peer_id = u32_get_bits(mark, SDWF_PEER_ID);
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return;
		}
		msduq = u32_get_bits(mark, SDWF_MSDUQ_ID);
		if (msduq >= MSDUQ_MAX_DEF)
			qos_id = dp_peer_msduq_qos_id(dp->ab, dp_peer->qos,
						      msduq);
		rcu_read_unlock();
	} else if (qos_tag == QOS_SCS_TAG) {
		scs_id = u32_get_bits(mark, QOS_QOS_ID_MASK);

		if (!dp_peer)
			return;

		ret = ath12k_dp_peer_scs_data(dp,
					      scs_id, dp_pdev->dp_hw,
					      &msduq, &qos_id,
					      dp_peer);
		if (ret != 0) {
			ath12k_err(dp->ab, "SCS Peer Data is NULL");
			return;
		}
	} else {
		msduq = u32_get_bits(mark, QOS_QOS_ID_MASK);
	}
	/* Update Desc for HLOS TID Override */
	if (msduq < MSDUQ_MAX_DEF) {
		ath12k_wifi_qos_hlos_tid(desc, msduq);
	} else if (msduq < QOS_MSDUQ_MAX) {
		/* Update Desc for User Defined QoS MSDUQ */
		ath12k_wifi_qos_desc(desc, msduq, qos_id);
	}
}

static void
ath12k_dp_sdwftx_ingress_stats_update(struct ath12k *ar,
				      u32 *skb_mark, u32 qos_nw_delay,
				      unsigned int skb_len)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_peer *dp_peer;
	u8 msduq;
	u16 peer_id, qos_id;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (!ar)
		return;

	if (!(*skb_mark & SDWF_VALID_MASK))
		return;

	msduq = u32_get_bits(*skb_mark, SDWF_MSDUQ_ID);

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    (ath12k_debugfs_is_qos_stats_enabled(ar) &
	    ATH12K_QOS_STATS_BASIC)) {
		peer_id = u32_get_bits(*skb_mark, SDWF_PEER_ID);

		if (!ar->dp.dp)
			return;

		dp = ar->dp.dp;

		rcu_read_lock();
		spin_lock_bh(&dp->dp_lock);

		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		if (!dp_peer || !dp_peer->qos) {
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		qos_id = dp_peer_msduq_qos_id(ar->ab, dp_peer->qos, msduq);
		if (qos_id == QOS_ID_INVALID) {
			ath12k_err(ar->ab, "msduq_id: %u not yet reserved\n",
				   msduq);
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		ath12k_qos_tx_enqueue_peer_stats(dp_peer,
						 dp_pdev->hw_link_id,
						 msduq, skb_len);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
	}
}

#define HW_TX_DELAY_MAX				0x1000000

#define TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US	10
#define HW_TX_DELAY_MASK			0x1FFFFFFF
#define TX_COMPL_BUFFER_TSTAMP_US(TSTAMP) \
	(((TSTAMP) << TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US) & \
	 HW_TX_DELAY_MASK)

#define ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER 1000
#define ATH12K_MOV_AVG_PKT_WIN	10

#define ATH12K_HIST_AVG_DIV	2

#define HTT_META_DATA_ALIGNMENT 0x8

#ifndef PLATFORM_SDX
static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end) {
#ifndef CONFIG_IO_COHERENCY
	dmac_clean_range_no_dsb(start, end);
#endif
}
#else
static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end) {
}
#endif

/**
 * ath12k_wifi8_mcbc_get_gsn() - Atomically increment and return the multicast GSN
 * @dp_vif: DP virtual interface whose mcbc_gsn counter is incremented
 *
 * Atomically increments the per-vif multicast/broadcast Global Sequence Number
 * counter and returns the lower 12 bits. The 12-bit wrap-around ensures the
 * value fits in the HTT TCL metadata GSN field.
 *
 * Returns: 12-bit GSN value (0..4095)
 */
static u16 ath12k_wifi8_mcbc_get_gsn(struct ath12k_dp_vif *dp_vif)
{
	return atomic_inc_return(&dp_vif->mcbc_gsn) & 0xfff;
}

/**
 * ath12k_wifi8_tx_recovery_drop() - Check if packet has to be dropped during recovery
 * @ab: ath12k_base
 *
 * Returns: true if packet needs to be dropped, false otherwise
 */
static bool ath12k_wifi8_tx_recovery_drop(struct ath12k_base *ab)
{
	return unlikely(test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS,
			&ab->dev_flags) ||
			(test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) &&
			!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags)));
}

/**
 * ath12k_wifi8_dp_get_ring_id() - Select TX ring based on SKB
 * @dp: DP structure containing hardware parameters and ring selector callback
 * @ring_id: Output ring ID, set to the selected ring index
 * @skb: Socket buffer used as input to the hardware-specific ring selector
 *
 * Calls the hardware-specific get_ring_selector() callback (e.g. CPU ID for
 * QCN9625) and wraps the result modulo the
 * maximum number of TX rings configured for this hardware.
 */
static inline
void ath12k_wifi8_dp_get_ring_id(struct ath12k_dp *dp, u8 *ring_id,
				 struct sk_buff *skb)
{
	u8 ring_selector;

	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);
	*ring_id = ring_selector % dp->hw_params->max_tx_ring;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	if (unlikely(*ring_id == ATH12K_IPA_TCL_RING))
		*ring_id = ATH12K_IPA_TCL_SW_RING;
#endif
}

static void
ath12k_wifi8_dp_tx_update_gsn_metadata(struct ath12k_dp_tx_msdu_info *msdu_info,
				       struct ath12k_dp_link_vif *dp_link_vif,
				       int mcbc_gsn);

/**
 * ath12k_wifi8_ucast_setup_msdu_info() - Setup MSDU info for unicast transmission
 * @arvif: ath12k link virtual interface
 * @arsta: Link station pointer
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to fill with link vif parameters
 * @skb: Socket buffer (unused, reserved for future use)
 * @dp_pdev: DP pdev structure for the transmitting radio
 *
 * Populates the MSDU info structure with unicast-specific fields from the
 * DP virtual interface: bank ID, TCL metadata, vdev ID,
 * BSS AST index/hash, and descriptor type (HAL_TCL_DESC_TYPE_BUFFER).
 */
static void ath12k_wifi8_ucast_setup_msdu_info(struct ath12k_link_vif *arvif,
					       struct ath12k_link_sta *arsta,
					       struct ath12k_dp *dp,
					       struct ath12k_dp_tx_msdu_info *msdu_info,
					       struct sk_buff *skb,
					       struct ath12k_pdev_dp *dp_pdev,
					       struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_sta *ahsta = NULL;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		msdu_info->bss_ast_idx = dp_vlan_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vlan_vif->ast_hash;
		msdu_info->lookup_override = true;
	} else {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	} else if (arsta) {
		ahsta = arsta->ahsta;
		if (ahsta->use_4addr_set) {
			rcu_read_lock();
			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
								      ahsta->dp_peer_id);
			if (!dp_peer || !dp_peer->peer_ext_ctx) {
				rcu_read_unlock();
				return;
			}

			msdu_info->bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
			msdu_info->bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
			msdu_info->lookup_override = true;
			msdu_info->meta_data_flags =
				u32_encode_bits(0, HTT_TCL_META_DATA_TYPE_V3) |
				u32_encode_bits(dp_peer->peer_id,
						HTT_TCL_META_DATA_PEER_ID_V3);
			rcu_read_unlock();
		}
	}

	msdu_info->bank_id = dp_vif->bank_id;
	msdu_info->vdev_id = dp_vif->dp_vif_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->pkt_offset = 0;
}

/**
 * ath12k_wifi8_mcbc_setup_msdu_info() - Setup MSDU info for multicast/broadcast
 * @arvif: ath12k link virtual interface
 * @dp_link_vif: DP link virtual interface with bank, lmac, vdev, and AST info
 * @arsta: Link station pointer (optional; used for 4-addr/WDS frames)
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to populate
 * @gsn: Global Sequence Number for MLO multicast reinjection
 * @gsn_valid: Whether the GSN should be embedded in TCL metadata
 * @skb: Socket buffer; header parsed for 4-addr detection in non-Ethernet mode
 * @is_eth: True if the frame is in Ethernet encapsulation format
 * @dp_pdev: DP pdev structure for the transmitting radio
 *
 * Populates the MSDU info for multicast/broadcast transmission. For 4-addr
 * (WDS) Ethernet frames, uses the station's AST info and sets lookup_override.
 * For 4-addr native WiFi frames, redirects to firmware. Otherwise uses the
 * BSS AST info from the link vif. If GSN is valid and lookup_override is not
 * set, embeds the GSN into the TCL metadata for MLO multicast reinjection.
 *
 * Returns: 0 always
 */
static int ath12k_wifi8_mcbc_setup_msdu_info(struct ath12k_link_vif *arvif,
					     struct ath12k_dp_link_vif *dp_link_vif,
					     struct ath12k_link_sta *arsta,
					     struct ath12k_dp *dp,
					     struct ath12k_dp_tx_msdu_info *msdu_info,
					     u16 gsn, bool gsn_valid,
					     struct sk_buff *skb,
					     bool is_eth, struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_vif *vlan_ahvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_vif *dp_vlan_vif = &vlan_ahvif->dp_vif;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_sta *ahsta = NULL;

	msdu_info->meta_data_flags = dp_link_vif->tcl_metadata;

	if (dp_vlan_vif && dp_vlan_vif->is_wds_4addr) {
		msdu_info->bss_ast_idx = dp_vlan_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vlan_vif->ast_hash;
		msdu_info->lookup_override = true;
	} else {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	}

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
		msdu_info->bss_ast_idx = dp_vif->ast_idx;
		msdu_info->bss_ast_hash = dp_vif->ast_hash;
	} else if (arsta) {
		ahsta = arsta->ahsta;
		if (ahsta->use_4addr_set) {
			rcu_read_lock();
			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
								      ahsta->dp_peer_id);
			if (!dp_peer) {
				rcu_read_unlock();
				return -EINVAL;
			}

			msdu_info->bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
			msdu_info->bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
			msdu_info->lookup_override = true;
			msdu_info->meta_data_flags =
				u32_encode_bits(0, HTT_TCL_META_DATA_TYPE_V3) |
				u32_encode_bits(dp_peer->peer_id,
						HTT_TCL_META_DATA_PEER_ID_V3);

			rcu_read_unlock();
		}
	} else {
		msdu_info->bss_ast_hash = dp_link_vif->ast_hash;
		msdu_info->bss_ast_idx = dp_link_vif->ast_idx;
		msdu_info->lookup_override = true;
	}

	if (gsn_valid && !arsta) {
		/* Reset and Initialize meta_data_flags with Global Sequence
		 * Number (GSN) info.
		 */
		msdu_info->meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					HTT_TCL_META_DATA_TYPE_V3) |
			u32_encode_bits(gsn, HTT_TCL_META_DATA_GLOBAL_SEQ_NUM_V3);
		msdu_info->tx_notify_frame = 6;

		if (arvif->nawds_support)
			msdu_info->meta_data_flags |=
				u32_encode_bits(1, HTT_TCL_META_DATA_GSN_INSPECTED_V3);
	}

	msdu_info->bank_id = dp_vif->bank_id;
	msdu_info->vdev_id = dp_vif->dp_vif_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->pkt_offset = 0;

	if (gsn_valid && !arsta)
		msdu_info->vdev_id += HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
	else if (arvif->nawds_support && !msdu_info->lookup_override)
		msdu_info->meta_data_flags |=
			u32_encode_bits(1, HTT_TCL_META_DATA_HOST_INSPECTED_MISSION_V3);

	return 0;
}

static bool ath12k_wifi8_is_mpsk_enabled(struct ath12k_vif *ahvif)
{
	if (ahvif->vif &&
	    ahvif->vif->type == NL80211_IFTYPE_AP &&
	    ahvif->u.ap.dynamic_vlan)
		return true;

	return false;
}

/**
 * ath12k_wifi8_mcbc_setup_encryption() - Setup encryption context for multicast
 * @dp_vif: DP virtual interface
 * @dp_pdev: DP pdev for the transmitting radio (provides the ath12k pointer)
 * @link_id: MLO link identifier
 * @skb: Socket buffer whose skb_cb will be populated with cipher/link info
 * @is_sta: True if the transmitting vdev is in STA mode (skips group slot lookup)
 * @msdu_info: MSDU info used to carry group slot metadata
 *
 * Resolves the ath12k link vif for the given link_id, sets the skb_cb fields
 * (ar, link_id, vif), and looks up the BSS peer to find the current multicast
 * key. If a key is found, sets the cipher and ATH12K_SKB_CIPHER_SET flag.
 * For non-STA vdevs, also resolves the VLAN group key slot.
 *
 * Returns: 0 on success, -ENOENT if ar, arvif, or BSS peer is not found
 */
static int ath12k_wifi8_mcbc_setup_encryption(struct ath12k_dp_vif *dp_vif,
					      struct ath12k_pdev_dp *dp_pdev,
					      u8 link_id,
					      struct sk_buff *skb,
					      bool is_sta, bool is_eth,
					      struct ath12k_dp_tx_msdu_info *msdu_info,
					      struct ath12k_vif *vlan_ahvif,
					      struct ieee80211_tx_info *info,
					      struct ieee80211_sta *sta)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_hdr *hdr = NULL;
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_key_conf *key;
	struct ath12k *ar;
	bool mpsk_enabled;

	if (!is_eth && !sta)
		ath12k_mlo_mcast_update_tx_link_address(ahvif->vif, link_id,
							skb, info->flags);
	/* Get AR from link */
	ar = dp_pdev->ar;
	if (!ar)
		return -ENOENT;

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (unlikely(!arvif))
		return -ENOENT;

	skb_cb->u.ar = ar;
	skb_cb->link_id = link_id;
	skb_cb->vif = ahvif->vif;

	/* Skip for open mode */
	if (unlikely(arvif->key_cipher == WMI_CIPHER_NONE))
		return 0;

	spin_lock_bh(&dp_pdev->dp_hw->peer_hash_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr(dp_pdev->dp_hw, sta->addr,
						      ahvif->vif);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_pdev->dp_hw, arvif->bssid,
						      ahvif->vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_pdev->dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	/* Get multicast key */
	spin_lock_bh(&dp_peer->keys_lock);

	key = dp_peer->keys[dp_peer->mcast_keyidx];
	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
		if (!is_eth) {
			hdr = (struct ieee80211_hdr *)skb->data;
			if (!ieee80211_has_protected(hdr->frame_control))
				hdr->frame_control |=
						cpu_to_le16(IEEE80211_FCTL_PROTECTED);
		}
	}

	spin_unlock_bh(&dp_peer->keys_lock);
	spin_unlock_bh(&dp_pdev->dp_hw->peer_hash_lock);

	mpsk_enabled = !is_sta && ath12k_wifi8_is_mpsk_enabled(ahvif);
	if (mpsk_enabled) {
		if (vlan_ahvif && vlan_ahvif->vif->type == NL80211_IFTYPE_AP_VLAN)
			msdu_info->group_slot =
				ath12k_dp_tx_get_mcast_group_slot(vlan_ahvif,
								  link_id);
		else if (ahvif->vif && ahvif->vif->type == NL80211_IFTYPE_AP)
			msdu_info->group_slot = 0;
	}

	return 0;
}

/**
 * ath12k_wifi8_dp_mac_encrypt_handler() - Dynamic VLAN (DVLAN) feature handler
 * @msdu_info: MSDU information structure to update with DVLAN settings
 *
 * Handles dynamic-VLAN tagged frames by setting the encapsulation type to RAW,
 * disabling encryption (OPEN), and directing the frame to firmware for
 * processing. Sets the HTT metadata flag and marks the frame for kernel
 * memory extension (ext_kmem) so an extended descriptor is allocated.
 *
 * Returns: DP_TX_FEATURE_SUCCESS always
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_mac_encrypt_handler(struct ath12k_dp_tx_msdu_info *msdu_info)
{
	msdu_info->ext_kmem = true;
	msdu_info->to_fw = true;
	msdu_info->ext_desc.add_htt_metadata = true;
	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;

	return  DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_encap_mismatch_handler() - Encapsulation mismatch handler
 * @dp_vif: DP virtual interface with the configured TX encapsulation type
 * @dp: DP structure for accessing hardware parameters
 * @msdu_info: MSDU information structure to update with corrected encap settings
 * @skb: Socket buffer being transmitted
 *
 * Handles frames where the encapsulation type does not match the expected
 * type for the vdev. For Ethernet-mode vdevs: EAPOL frames are redirected
 * to firmware with HTT metadata; null-function frames are sent to FW with
 * RAW encapsulation.
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on invalid encap type
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_encap_mismatch_handler(struct ath12k_dp_vif *dp_vif,
				       struct ath12k_dp *dp,
				       struct ath12k_dp_tx_msdu_info *msdu_info,
				       struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	msdu_info->ext_desc.encap_type = ath12k_dp_tx_get_encap_type(dp->ab, skb);
	if (msdu_info->ext_desc.encap_type >= HAL_TCL_ENCAP_TYPE_802_3)
		return DP_TX_ERROR;

	msdu_info->mpsk_diff_encap = true;
	msdu_info->is_null = ieee80211_is_any_nullfunc(hdr->frame_control);
	if (unlikely(dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_ETHERNET)) {
		msdu_info->ext_kmem = true;
		msdu_info->to_fw = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

		if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			msdu_info->ext_desc.add_htt_metadata = true;
		} else if (msdu_info->is_null) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		}
	}
	return ret;
}

/**
 * ath12k_wifi8_dp_nwifi_handler() - Native WiFi feature handler
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_nwifi_handler(struct sk_buff *skb)
{
	ath12k_dp_tx_encap_nwifi(skb);
	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_raw_mode_handler() - RAW mode feature handler
 * @dp_vif: DP virtual interface
 * @dp: DP structure
 * @msdu_info: MSDU information
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi8_dp_raw_mode_handler(struct ath12k_dp_vif *dp_vif,
				 struct ath12k_dp *dp,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 u32 *len,
				 struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	if (!test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &dp->ab->ag->flags))
		return DP_TX_ERROR;

	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	if (skb->protocol == cpu_to_be16(ETH_P_ARP)) {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
	}

	/* Handle encryption in RAW mode */
	if (skb_cb->flags & ATH12K_SKB_CIPHER_SET) {
		msdu_info->ext_desc.encrypt_type =
			ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

		/* Add MIC for encrypted frames */
		if (ieee80211_has_protected(hdr->frame_control)) {
			skb_put(skb, IEEE80211_CCMP_MIC_LEN);
			*len = skb->len;
		}
	} else {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi8_dp_tx_process_features() - Process feature bitmap
 * @dp_vif: DP virtual interface
 * @dp_pdev: DP pdev structure for the transmitting radio
 * @skb: Socket buffer
 * @msdu_info: MSDU info
 * @skb_ctrl: Per-SKB feature bitmap
 *
 * Returns: DP_TX_FEATURE_SUCCESS to continue, DP_TX_DROP to drop
 */
static enum ath12k_dp_feature_result
ath12k_wifi8_dp_tx_process_features(struct ath12k_dp_vif *dp_vif,
				    struct ath12k_pdev_dp *dp_pdev,
				    struct sk_buff *skb,
				    u32 *len,
				    struct ath12k_dp_tx_msdu_info *msdu_info,
				    struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	/* Fast path: no features enabled */
	if (likely(ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_NONE)))
		return DP_TX_FEATURE_SUCCESS;

	/* Process SW encrpt feature */
	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features, DP_FEATURE_SW_ENCRPT)) {
		ret = ath12k_wifi8_dp_mac_encrypt_handler(msdu_info);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features,
				   DP_FEATURE_ENCAP_MISMATCH_HANDLE)) {
		ret = ath12k_wifi8_dp_encap_mismatch_handler(dp_vif, dp_pdev->dp,
							     msdu_info, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process Native WiFi feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_NATIVE_WIFI)) {
		ret = ath12k_wifi8_dp_nwifi_handler(skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process RAW mode feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_RAW_MODE)) {
		ret = ath12k_wifi8_dp_raw_mode_handler(dp_vif, dp_pdev->dp,
						       msdu_info, len, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* HLOS TID override */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_HLOS)) {
		if (unlikely(skb->priority)) {
			msdu_info->tid = skb->priority;
			msdu_info->tid_override = true;
		}
	}

	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_ME)) {
		if (!ath12k_dp_me_tx(dp_vif, skb, msdu_info))
			return DP_TX_RETURN;
	}

	/* Process Scatter-Gather Feature */
	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features, DP_FEATURE_SG)) {
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_SG;
		msdu_info->to_fw = 0;
	}

	return ret;
}

/**
 * ath12k_wifi8_dp_dma_align_handler() - DMA alignment feature handler
 * @dp_vif: DP virtual interface
 * @skb: Socket buffer
 *
 * Returns: None
 */
static inline
void ath12k_wifi8_dp_dma_align_handler(struct ath12k_dp *dp,
				       struct sk_buff *skb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	int ret;

	/* Check if alignment is needed */
	if (!iova_mask || !((unsigned long)skb->data & iova_mask))
		return;

	/* Align buffer */
	ret = ath12k_dp_tx_align_payload(dp, &skb);
	if (ret) {
		/* Continue with unaligned buffer */
		return;
	}
}

/**
 * ath12k_wifi8_dp_tx_hal_tcl_desc_update() - Populate HAL TCL data descriptor
 * @hal_tcl_desc: Pointer to the HAL TCL data command descriptor to fill
 * @msdu_info: MSDU information containing all descriptor field values
 * @skb: Socket buffer (unused, reserved for future use)
 *
 * Fills all fields of the HAL TCL data command descriptor from the
 * ath12k_dp_tx_msdu_info structure, including buffer address info (physical
 * address, RBM ID, SW cookie), descriptor type, bank ID, TCL metadata flags,
 * data length, TID, LMAC ID, vdev ID, AST index, and cache set number.
 */
static inline
void ath12k_wifi8_dp_tx_hal_tcl_desc_update(struct hal_tcl_data_cmd *hal_tcl_desc,
					    struct ath12k_dp_tx_msdu_info *msdu_info,
					    struct sk_buff *skb)
{
	hal_tcl_desc->buf_addr_info.info0 =
		le32_encode_bits(msdu_info->paddr, BUFFER_ADDR_INFO0_ADDR);
	hal_tcl_desc->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)msdu_info->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	hal_tcl_desc->buf_addr_info.info1 |=
		le32_encode_bits((msdu_info->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(msdu_info->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	hal_tcl_desc->info0 =
		le32_encode_bits(msdu_info->type,
				 HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE) |
		le32_encode_bits(msdu_info->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID) |
		le32_encode_bits(msdu_info->vdev_id, HAL_TCL_DATA_CMD_INFO0_VDEV_ID) |
		le32_encode_bits(msdu_info->data_len, HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH);

	hal_tcl_desc->search_index = cpu_to_le32(msdu_info->bss_ast_idx);

	hal_tcl_desc->info1 =
		le32_encode_bits(msdu_info->bss_ast_hash,
				 HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM) |
		le32_encode_bits(msdu_info->lookup_override,
				 HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE) |
		le32_encode_bits(msdu_info->tid, HAL_TCL_DATA_CMD_INFO1_HLOS_TID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO1_HEADER_LENGTH_READ_SEL);

	hal_tcl_desc->info2 = cpu_to_le32(msdu_info->flags0) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_MSDU_COLOR) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_FORMAT) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO2_RX_TIMESTAMP_VALID);

	hal_tcl_desc->info3 = cpu_to_le32(msdu_info->flags1) |
		le32_encode_bits(msdu_info->tx_notify_frame,
				 HAL_TCL_DATA_CMD_INFO3_TX_NOTIFY_FRAME) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT) |
		le32_encode_bits(msdu_info->pkt_offset,
				 HAL_TCL_DATA_CMD_INFO3_METADATA_LENGTH) |
		le32_encode_bits(HAL_TX_WILD_CARD_LINK_ID,
				 HAL_TCL_DATA_CMD_INFO3_LINK_ID);

	hal_tcl_desc->tcl_cmd_number = cpu_to_le32(msdu_info->meta_data_flags);

	hal_tcl_desc->info4 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TX_ENQUEUE_TIMESTAMP_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID_VALID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO4_TELEMETRY_STREAM_ID);

	hal_tcl_desc->insert_vlan_tci_override_val = 0;

	hal_tcl_desc->info5 =
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_INSERT_VLAN_TCI_OVERRIDE_EN) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_RING_ID) |
		le32_encode_bits(0, HAL_TCL_DATA_CMD_INFO5_LOOPING_COUNT);
}

/**
 * ath12k_wifi8_dp_tx_hw_enqueue() - Enqueue a frame to the TCL hardware ring
 * @dp_link_vif: DP link virtual interface
 * @dp_pdev: DP pdev structure for the transmitting radio
 * @msdu_info: MSDU information with all descriptor fields populated
 * @ring_id: TX ring index to enqueue to
 * @arsta: Link station pointer (optional, for QoS tag updates)
 * @skb: Socket buffer to enqueue
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 * @tx_desc: Tx Software Descriptor
 * @dp_vif: DP mld virtual interface
 * @feat_bypass: Feature Bypass flag for fast path statistics
 * @is_mcast: Flag for Multicast packet to increment stats
 *
 * Acquires the TCL ring, writes the TCL data descriptor, applies QoS
 * descriptor updates for SDWF/SCS-tagged frames, and releases the ring.
 * CONFIG_IO_COHERENCY variant writes directly to the ring entry; the
 * non-coherent variant uses a local copy and memcpy for cache safety.
 *
 * Returns: 0 on success, -ENOENT if no TCL descriptor is available
 */
#ifdef CONFIG_IO_COHERENCY
static int ath12k_wifi8_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_tx_msdu_info *msdu_info,
					 u8 ring_id,
					 struct ath12k_link_sta *arsta,
					 struct sk_buff *skb,
					 u32 qos_nw_delay,
					 struct ath12k_tx_desc_info *tx_desc,
					 struct ath12k_dp_vif *dp_vif,
					 bool feat_bypass, bool is_mcast,
					 struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	u32 len = msdu_info->data_len;
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		return -ENOENT;
	}

	ath12k_wifi8_dp_tx_hal_tcl_desc_update(hal_tcl_desc, msdu_info, skb);
	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_wifi8_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
					   0, dp_peer, dp_link_vif->link_id);
		ath12k_dp_sdwftx_ingress_stats_update(dp_pdev->ar,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}

	if (unlikely(dp_peer)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_wifi8_dp_qos_update(dp, dp_pdev, skb->mark,
						   hal_tcl_desc, qos_tag,
						   dp_peer, dp_link_vif->link_id);
	}

	ath12k_dmb();

	/* Update success statistics */
	if (likely(feat_bypass)) {
		DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, len, ring_id);
		dp_pdev->dp->device_stats.tx_fast_unicast[ring_id]++;
	} else {
		ath12k_dp_tx_stats_post_enqueue(dp, dp_pdev, dp_vif, skb,
						msdu_info, ring_id,
						len, is_mcast, tx_desc);
	}

	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#else
static int ath12k_wifi8_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_tx_msdu_info *msdu_info,
					 u8 ring_id,
					 struct ath12k_link_sta *arsta,
					 struct sk_buff *skb,
					 u32 qos_nw_delay,
					 struct ath12k_tx_desc_info *tx_desc,
					 struct ath12k_dp_vif *dp_vif,
					 bool feat_bypass, bool is_mcast,
					 struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tcl_data_cmd tcl_desc = {0};
	u32 len = msdu_info->data_len;
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		return -ENOENT;
	}

	ath12k_wifi8_dp_tx_hal_tcl_desc_update(&tcl_desc, msdu_info, skb);

	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(&tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_wifi8_dp_qos_update(dp, dp_pdev, skb->mark, &tcl_desc,
					   0, dp_peer, dp_link_vif->link_id);
		ath12k_dp_sdwftx_ingress_stats_update(dp_pdev->ar,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
		skb->tstamp = net_timedelta(skb->tstamp);
	}

	if (unlikely(dp_peer)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_wifi8_dp_qos_update(dp, dp_pdev, skb->mark,
						   &tcl_desc, qos_tag,
						   dp_peer, dp_link_vif->link_id);
	}

	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
	ath12k_dmb();

	/* Update success statistics */
	if (likely(feat_bypass)) {
		DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, len, ring_id);
		dp_pdev->dp->device_stats.tx_fast_unicast[ring_id]++;
	} else {
		ath12k_dp_tx_stats_post_enqueue(dp, dp_pdev, dp_vif, skb,
						msdu_info, ring_id,
						len, is_mcast, tx_desc);
	}

	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#endif

static void
ath12k_wifi8_dp_tx_set_group_htt_metadata(struct hal_tx_msdu_metadata *htt_desc,
					  struct ath12k_dp_tx_msdu_info *msdu_info)
{
	htt_desc->info0 |=
		le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL) |
		le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_KEY_FLAGS);
	htt_desc->info2 |=
		le32_encode_bits(msdu_info->group_slot,
				 HAL_TX_MSDU_METADATA_INFO2_KEY_FLAGS);
}

static void
ath12k_wifi8_dp_tx_update_gsn_metadata(struct ath12k_dp_tx_msdu_info *msdu_info,
				       struct ath12k_dp_link_vif *dp_link_vif,
				       int mcbc_gsn)
{
	msdu_info->meta_data_flags =
		u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
				HTT_TCL_META_DATA_TYPE_V3) |
		u32_encode_bits(mcbc_gsn,
				HTT_TCL_META_DATA_GLOBAL_SEQ_NUM_V3);

	if (dp_link_vif->nawds_support)
		msdu_info->meta_data_flags |=
			u32_encode_bits(1,
					HTT_TCL_META_DATA_GSN_INSPECTED_V3);
	msdu_info->meta_data_flags |=
		u32_encode_bits(1, HTT_TCL_META_DATA_GLOBAL_HTT_EXT_PRESENT_V3);
}

/**
 * ath12k_wifi8_dp_ext_desc_populate() - Allocate and populate extended TX descriptor
 * @dp: DP structure for slab cache access and DMA mapping
 * @dp_link_vif: DP link virtual interface (for NAWDS and GSN metadata)
 * @msdu_info: MSDU information with ext_feature type and buffer addresses
 * @tx_desc: SW TX descriptor to update with ext_desc pointer and physical address
 * @gsn_valid: Whether the Global Sequence Number is valid for MLO multicast
 * @gsn: Global Sequence Number value to embed in metadata
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on allocation failure
 */
static int
ath12k_wifi8_dp_ext_desc_populate(struct ath12k_dp *dp,
				  struct ath12k_dp_vif *dp_vif,
				  struct ath12k_dp_link_vif *dp_link_vif,
				  struct sk_buff *skb,
				  struct ath12k_dp_tx_msdu_info *msdu_info,
				  struct ath12k_tx_desc_info *tx_desc,
				  bool gsn_valid, int gsn, u8 ring_id)
{
	struct ath12k_dp_ext_desc *ext_desc = NULL;
	struct ath12k_dp_ext_desc_msdu_info *ext_msdu_info =
		&msdu_info->ext_desc;
	struct hal_tx_msdu_metadata *htt_desc_ext = NULL;
	u16 ext_data_len = 0;
	u8 htt_desc_size;

	/* Allocate extended descriptor */
	ext_desc = kmem_cache_alloc(dp->ext_cache, GFP_DMA | __GFP_ZERO);
	if (!ext_desc) {
		ath12k_warn(dp->ab, "Ext Descriptor not allocated\n");
		return -ENOMEM;
	}
	memset(ext_desc, 0, ATH12K_DP_EXT_DESC_SZ);

	switch (msdu_info->ext_desc.ext_feature) {
	case DP_EXT_ME5:
		dma_addr_t mac_paddr, buf_paddr;
		u8 *mac_addr;
		u16 buf_len;

		/* First segment */
		mac_addr = ath12k_dp_ext_desc_get_spare(ext_desc, ETH_ALEN);
		mac_paddr = virt_to_phys(mac_addr);
		ether_addr_copy(mac_addr, msdu_info->ext_desc.peer_mac_addr);
		ath12k_dp_ext_desc_set_buf0(ext_desc, mac_paddr, ETH_ALEN);

		/* Second segment */
		buf_paddr = msdu_info->paddr + ETH_ALEN;
		buf_len =  msdu_info->data_len - ETH_ALEN;
		ath12k_dp_ext_desc_set_buf1(ext_desc, buf_paddr, buf_len);
		ext_data_len = ATH12K_DP_EXT_DESC_SZ;

		break;
	case DP_EXT_ENCAP_OVERRIDE:
		ath12k_dp_ext_desc_set_buf0(ext_desc, msdu_info->paddr,
					    msdu_info->data_len);
		ath12k_dp_ext_desc_override_set(&ext_desc->desc,
						ext_msdu_info);
		ext_data_len = ATH12K_TX_MSDU_EXT_SZ;

		break;
	case DP_EXT_TSO:
	case DP_EXT_SG:
		if (ath12k_dp_sg_ext_desc_populate(dp, dp_vif, ext_desc,
							 skb, ring_id))
			goto fail_free_ext_desc;
		tx_desc->is_from_sg = 1;
		ext_data_len = msdu_info->data_len;
		break;
	default:
		break;
	}

	if (msdu_info->ext_desc.add_htt_metadata) {
		msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT_V3;
		htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
		htt_desc_ext = (struct hal_tx_msdu_metadata *)
				ath12k_dp_ext_desc_get_rsvd0(ext_desc);

		if (!htt_desc_ext)
			goto fail_free_ext_desc;

		htt_desc_ext->info0 |=
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE) |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);

		ext_data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;
	}

	if (msdu_info->group_slot >= 0 && msdu_info->mpsk_diff_encap) {
		htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
		htt_desc_ext = (struct hal_tx_msdu_metadata *)
				ath12k_dp_ext_desc_get_rsvd0(ext_desc);
		if (!htt_desc_ext)
			goto fail_free_ext_desc;

		ath12k_wifi8_dp_tx_set_group_htt_metadata(htt_desc_ext, msdu_info);

		if (gsn_valid) {
			ath12k_wifi8_dp_tx_update_gsn_metadata(msdu_info,
							       dp_link_vif, gsn);
			msdu_info->vdev_id |=
				HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
		} else {
			msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT_V3;
		}

		ext_data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;
		msdu_info->to_fw = true;
	}

	msdu_info->type = HAL_TCL_DESC_TYPE_EXT_DESC;
	msdu_info->data_len = ext_data_len;
	msdu_info->paddr = ath12k_dp_ext_desc_map(dp, ext_desc);

	tx_desc->ext_kmem = msdu_info->ext_kmem;
	tx_desc->paddr_ext_desc = msdu_info->paddr;
	tx_desc->ext_desc = ext_desc;
	tx_desc->ext_desc_len = ATH12K_DP_EXT_DESC_SZ;

	return 0;

fail_free_ext_desc:
	if (ext_desc)
		kmem_cache_free(dp->ext_cache, ext_desc);

	return -ENOMEM;
}

/**
 * ath12k_wifi8_dp_tx_desc_populate() - Populate SW TX descriptor fields
 * @dp: DP structure (passed to ath12k_wifi8_dp_ext_desc_populate if needed)
 * @skb: Socket buffer
 * @dp_link_vif: DP link virtual interface (passed to ext_desc_populate)
 * @msdu_info: MSDU information with DMA address, length, and ext_kmem flag
 * @tx_desc: SW TX descriptor to populate
 * @gsn_valid: Whether the Global Sequence Number is valid
 * @gsn: Global Sequence Number value
 *
 * Stores the DMA physical address, data length, and FW-redirect flag into
 * the SW TX descriptor. If extended kernel memory is required (ext_kmem is
 * set in msdu_info), allocates and populates an extended descriptor via
 * ath12k_wifi8_dp_ext_desc_populate().
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on ext_desc failure
 */
static enum ath12k_dp_feature_result
ath12k_wifi8_dp_tx_desc_populate(struct ath12k_dp *dp, struct sk_buff *skb,
				 struct ath12k_dp_vif *dp_vif,
				 struct ath12k_dp_link_vif *dp_link_vif,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 struct ath12k_tx_desc_info *tx_desc,
				 bool gsn_valid, int gsn, u8 ring_id)
{
	int ret = 0;

	tx_desc->len = msdu_info->data_len;
	tx_desc->skb = skb;

	if (msdu_info->ext_kmem)
		ret = ath12k_wifi8_dp_ext_desc_populate(dp, dp_vif, dp_link_vif,
							skb, msdu_info, tx_desc,
							gsn_valid, gsn, ring_id);

	if (msdu_info->to_fw) {
		msdu_info->flags0 |= u32_encode_bits(1,
				     HAL_TCL_DATA_CMD_INFO2_TO_FW_SW);
		tx_desc->to_fw = msdu_info->to_fw;
	}

	return ret;
}

static int
ath12k_wifi8_dp_prepare_group_htt_metadata(struct sk_buff *skb,
					   struct ath12k_dp_tx_msdu_info *msdu_info,
					   struct ath12k_dp_link_vif *dp_link_vif,
					   bool gsn_valid, int gsn)
{
	struct hal_tx_msdu_metadata *htt_desc;
	u8 align_pad, htt_desc_size, htt_hdr_size;

	align_pad = (unsigned long)skb->data & (HTT_META_DATA_ALIGNMENT - 1);
	htt_desc_size = ALIGN(sizeof(*htt_desc), HTT_META_DATA_ALIGNMENT);
	htt_hdr_size = align_pad + htt_desc_size;

	htt_desc = ath12k_dp_metadata_align_skb_head(skb, htt_hdr_size);
	if (!htt_desc)
		return -ENOMEM;

	ath12k_wifi8_dp_tx_set_group_htt_metadata(htt_desc, msdu_info);
	msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT_V3;

	if (gsn_valid)
		ath12k_wifi8_dp_tx_update_gsn_metadata(msdu_info, dp_link_vif, gsn);

	msdu_info->pkt_offset = htt_hdr_size;
	msdu_info->data_len = skb->len - htt_hdr_size;
	msdu_info->to_fw = true;

	return 0;
}

enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_vif *ahvif,
			      struct ath12k_dp_link_vif *dp_link_vif,
			      u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			      bool gsn_valid, u16 gsn,
			      struct sk_buff *skb, struct ath12k_link_sta *arsta,
			      struct ath12k_dp_skb_ctrl *skb_ctrl, bool htt_mesh)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(dp_pdev->dp);
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	enum ath12k_dp_tx_enq_error drop_reason;
	u32 qos_nw_delay = msdu_info->qos_nw_delay;
	u8 tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
	bool dma_map;
	int ret;

	if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_spl_desc_free_list,
					   ring_id);
	else
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_desc_free_list,
					   ring_id);

	if (!tx_desc) {
		drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
		goto fail;
	}

	if (msdu_info->group_slot >= 0 && !msdu_info->mpsk_diff_encap) {
		ret = ath12k_wifi8_dp_prepare_group_htt_metadata(skb, msdu_info,
								 dp_link_vif,
								 gsn_valid, gsn);
		if (ret) {
			drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
			goto fail;
		}
	}

	ath12k_wifi8_dp_dma_align_handler(central_dp, skb);

	/* For multicast packets, map the full buffer since MCAST always uses the slow
	 * path. skb->len includes any HTT metadata as well.
	 */
	dma_map = ATH12K_TX_BUFFER_MAP(central_dp, skb, skb->len, tx_desc,
				       msdu_info, skb_ctrl);

	if (unlikely(!dma_map)) {
		drop_reason = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail;
	}

	msdu_info->desc_id = tx_desc->desc_id;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;

	if (msdu_info->me_convert) {
		gsn_valid = false;
		msdu_info->tx_notify_frame = 0;
		msdu_info->vdev_id = ahvif->dp_vif.dp_vif_id;
	}

	ret = ath12k_wifi8_dp_tx_desc_populate(central_dp, skb, dp_vif, dp_link_vif,
					       msdu_info, tx_desc,
					       gsn_valid, gsn, ring_id);

	if (ret < 0) {
		drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail;
	}

	/* mcast maps skb->len (includes HTT header); fix up tx_desc->len so
	 * the completion-path unmap covers the same region.
	 */
	tx_desc->len = skb->len;

	/* Enqueue to hardware */
	ret = ath12k_wifi8_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, msdu_info,
					    ring_id, arsta, skb, qos_nw_delay,
					    tx_desc, &ahvif->dp_vif, false, true, NULL);
	if (ret < 0) {
		drop_reason = DP_TX_ENQ_DROP_HW_ENQ_FAIL;
		goto fail;
	}

	return DP_TX_ENQ_SUCCESS;

fail:
	if (tx_desc && tx_desc->ext_desc) {
		if (tx_desc->is_from_sg)
			ath12k_dp_tx_sg_unmap_buf(central_dp, tx_desc->ext_desc, skb);
		ath12k_dp_ext_desc_unmap(central_dp, tx_desc->paddr_ext_desc);
		kmem_cache_free(central_dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(central_dp, tx_desc, ring_id);

	if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_dp_vow_stats_enabled(dp_pdev))
			ath12k_dp_tx_drop_pdev_tid_stats(dp_pdev, drop_reason,
							 tid, ring_id);
	}
	return drop_reason;
}

/**
 * ath12k_wifi8_dp_tx_set_ast() - Fill ast hash & index
 * @dp: DP structure
 * @dp_peer: Peer structure
 * @msdu_info: MSDU information
 */
void ath12k_wifi8_dp_tx_set_ast(struct ath12k_dp_peer *dp_peer,
				struct ath12k_dp_tx_msdu_info *msdu_info,
				u8 hw_link_id)
{
	msdu_info->bss_ast_hash = dp_peer->peer_ext_ctx->ast_hash;
	msdu_info->bss_ast_idx = dp_peer->peer_ext_ctx->ast_index;
}

/**
 * ath12k_wifi8_ucast_handler() - Unicast packet transmission handler
 * @dp_vif: DP virtual interface
 * @link_id: MLO link identifier for the target link
 * @arsta: Link station pointer (optional; used for QoS tag updates)
 * @skb: Socket buffer to transmit
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Main unicast TX handler. Resolves the DP pdev, checks the TX pending limit,
 * selects a TX ring, populates MSDU info, processes the TX feature bitmap,
 * assigns a TX descriptor, DMA-maps the buffer, populates the SW TX descriptor,
 * and enqueues the frame to the TCL hardware ring.
 *
 */
void ath12k_wifi8_ucast_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
				struct ath12k_link_sta *arsta, struct sk_buff *skb,
				struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
				struct ath12k_vif *vlan_ahvif,
				struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = rcu_dereference(ahvif->link[link_id]);
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_dp *central_dp = NULL;
	struct ath12k_base *ab;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	enum ath12k_dp_tx_enq_error drop_reason = DP_TX_ENQ_DROP_MISC;
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	enum ath12k_dp_feature_result ret;
	bool feat_bypass = true;
	bool dma_map;
	u8 ring_id = 0;
	u32 len = skb->len;
	u8 tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;

	ab = dp_link_vif->ab;
	if (!ab)
		goto fail;

	/* Get DP pdev */
	dp_pdev = ath12k_dp_to_dp_pdev(ab->dp, dp_link_vif->pdev_idx);
	if (!dp_pdev) {
		drop_reason = DP_TX_ENQ_DROP_INV_PDEV;
		goto fail;
	}

	prefetch(dp_pdev);
	central_dp = ath12k_get_central_dp(dp_pdev->dp);

	/* Check recovery state */
	if (ath12k_wifi8_tx_recovery_drop(central_dp->ab)) {
		ieee80211_free_txskb(dp_pdev->ar->ah->hw, skb);
		drop_reason = DP_TX_ENQ_DROP_FW_RECOVERY;
		return;
	}

	/* Get ring ID  */
	if (likely(skb_ctrl->flags & DP_SKB_FAST_TX))
		ring_id = smp_processor_id();
	else
		ath12k_wifi8_dp_get_ring_id(central_dp, &ring_id, skb);

	/* Update receive statistics */
	ath12k_dp_tx_stats_update_pre_enqueue(dp_pdev, dp_vif, skb, ring_id, len);

	/* Setup MSDU info */
	ath12k_wifi8_ucast_setup_msdu_info(arvif, arsta, central_dp, &msdu_info, skb,
					   dp_pdev, vlan_ahvif);
	msdu_info.meta_data_flags = dp_link_vif->tcl_metadata;

	/* Fast path: no features enabled */
	if (unlikely((DP_FEATURE_IS_ANY(dp_vif) || skb_ctrl->features))) {
		/* Process features based on bitmap */
		feat_bypass = false;
		ret = ath12k_wifi8_dp_tx_process_features(dp_vif, dp_pdev, skb, &len,
							&msdu_info, skb_ctrl);

		if (ret != DP_TX_FEATURE_SUCCESS && ret != DP_TX_RETURN) {
			drop_reason = DP_TX_ENQ_DROP_FEAT_ERR;
			goto fail;
		}
	}

	prefetch(central_dp->device_stats.tx_fast_unicast);

	/* Assign TX descriptor */
#ifdef CPTCFG_QCN_EXTN
	/* EAPOL frames always use the special descriptor pool (with lock).
	 * For regular unicast frames on the DP_SKB_FAST_TX path use the
	 * per-ring hot list (lock-free); fall back to the regular pool for
	 * all other frames.
	 */
	if (likely(skb_ctrl->flags & DP_SKB_FAST_TX))
		tx_desc =
		ath12k_dp_tx_assign_buffer_hot(central_dp->dp_hw_grp, ring_id);
	else if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
#else
	if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE)))
#endif
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_spl_desc_free_list,
					   ring_id);
	else
		tx_desc =
		ath12k_dp_tx_assign_buffer(central_dp->dp_hw_grp,
					   central_dp->dp_hw_grp->tx_desc_free_list,
					   ring_id);

	if (unlikely(!tx_desc)) {
		central_dp->device_stats.tx_err.txbuf_na[ring_id]++;
		drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
		goto fail;
	}

	dma_map = ATH12K_TX_BUFFER_MAP(central_dp, skb, len, tx_desc, &msdu_info,
				       skb_ctrl);
	if (unlikely(!dma_map)) {
		ath12k_warn(central_dp->ab, "failed to DMA map data Tx buffer\n");
		atomic_inc(&central_dp->device_stats.tx_err.misc_fail);
		drop_reason = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail;
	}

	msdu_info.desc_id = tx_desc->desc_id;
	msdu_info.data_len = len;
	msdu_info.group_slot = -1;
	msdu_info.mpsk_diff_encap = 0;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;

	ret = ath12k_wifi8_dp_tx_desc_populate(central_dp, skb, dp_vif, dp_link_vif,
					       &msdu_info, tx_desc, false, 0, ring_id);
	if (ret != DP_TX_FEATURE_SUCCESS) {
		drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail;
	}

	/* Enqueue to hardware */
	ret = ath12k_wifi8_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, &msdu_info, ring_id,
					    arsta, skb, qos_nw_delay,
					    tx_desc, dp_vif, feat_bypass, false, dp_peer);

	if (ret) {
		drop_reason = DP_TX_ENQ_DROP_HW_ENQ_FAIL;
		goto fail;
	}

	atomic_inc(&dp_pdev->num_tx_pending);

	return;

fail:
	if (tx_desc && tx_desc->ext_desc) {
		if (tx_desc->is_from_sg)
			ath12k_dp_tx_sg_unmap_buf(central_dp, tx_desc->ext_desc, skb);
		ath12k_dp_ext_desc_unmap(central_dp, tx_desc->paddr_ext_desc);
		kmem_cache_free(central_dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(central_dp, tx_desc, ring_id);

	if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_tid_stats_enabled(dp_pdev))
			ath12k_dp_tx_drop_tid_stats(dp_vif, drop_reason, tid, len);

		if (ath12k_dp_vow_stats_enabled(dp_pdev))
			ath12k_dp_tx_drop_pdev_tid_stats(dp_pdev, drop_reason,
							 tid, ring_id);
	}

	ath12k_mac_ieee80211_free_txskb(ahvif->ah->hw, skb, dp_pdev,
					arsta ? ath12k_ahsta_to_sta(arsta->ahsta) : NULL,
					dp_vif, drop_reason, ring_id, false);
}

/**
 * ath12k_wifi8_mcbc_handler() - Multicast/Broadcast packet transmission handler
 * @dp_vif: DP virtual interface
 * @link_id: MLO link identifier for the target link
 * @arsta: ath12k_link station pointer (optional; used for 4-addr/WDS frames)
 * @skb: Socket buffer to transmit (will be cloned/copied per active link)
 * @is_eth: True if the frame is in Ethernet encapsulation format
 * @gsn_valid: True if a Global Sequence Number should be assigned (MLO mcast)
 * @is_sta: True if the transmitting vdev is in STA mode
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Iterates over all active MLO links of the vif. For each link, clones or
 * copies the SKB, sets up encryption context, populates MSDU info, processes
 * the TX feature bitmap, assigns a TX descriptor, DMA-maps the buffer, and
 * enqueues the frame to the TCL hardware ring. Frames are dropped per-link
 * on any error without affecting other links.
 */
void ath12k_wifi8_mcbc_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
			       struct ath12k_link_sta *arsta, struct sk_buff *skb,
			       bool is_eth, bool gsn_valid, bool is_sta,
			       struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
			       bool htt_mesh, struct ath12k_vif *vlan_ahvif,
			       struct ieee80211_tx_info *info,
			       struct ieee80211_sta *sta)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	enum ath12k_dp_feature_result feature_ret;
	enum ath12k_dp_tx_enq_error err;
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *skb_new;
	struct ath12k_dp *central_dp = NULL;
	unsigned long links_map = 0;
	u32 len;
	u16 gsn;
	u8 ring_id = 0;
	int ret;

	/* Get active links.*/
	if (gsn_valid) {
		links_map = ahvif->links_map;
		gsn = ath12k_wifi8_mcbc_get_gsn(dp_vif);
	} else {
		set_bit(link_id, &links_map);
	}

	/* Update entry statistics */
	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);

	/*
	 * Iterate over links_map using explicit bitmap traversal.
	 *
	 * For gsn_valid case, only the first set link_id is processed.
	 * For non-gsn_valid case, all valid (set) link_ids in the VIF
	 * are iterated sequentially.
	 *
	 * find_first_bit() is used to get the initial link_id and
	 * find_next_bit() is used to traverse remaining set bits.
	 * The loop terminates when no more set bits are found
	 * (i.e., link_id >= IEEE80211_MLD_MAX_NUM_LINKS).
	 */

	do {
		struct ath12k_link_vif *arvif =
			rcu_dereference(ahvif->link[link_id]);
		struct ath12k *ar = NULL;
		struct ath12k_base *ab = NULL;

		if (!arvif || !arvif->is_up) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_INV_ARVIF],
				     1, ring_id);
			goto next;
		}

		ar = arvif->ar;
		if (!ar)
			goto next;

		/* For MLO multicast, skip links with no associated stations */
		if (ath12k_wifi8_is_mpsk_enabled(ahvif) &&
		    !(vlan_ahvif && vlan_ahvif->vif->type == NL80211_IFTYPE_AP_VLAN) &&
		    gsn_valid) {
			bool no_sta;

			spin_lock_bh(&ar->data_lock);
			no_sta = (arvif->num_stations == 0);
			spin_unlock_bh(&ar->data_lock);

			if (no_sta) {
				DP_STATS_INC(dp_vif,
					     tx_i.drop[DP_TX_ENQ_DROP_MCAST_NO_LINK],
					     1, ring_id);
				goto next;
			}
		}

		dp_link_vif = &dp_vif->dp_link_vif[link_id];

		if (!dp_link_vif) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_INV_ARVIF],
				     1, ring_id);
			goto next;
		}

		ab = dp_link_vif->ab;
		if (!ab)
			goto next;

		dp_pdev = ath12k_dp_to_dp_pdev(ab->dp,
					       dp_link_vif->pdev_idx);

		if (!dp_pdev) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_INV_PDEV],
				     1, ring_id);
			goto next;
		}

		central_dp = ath12k_get_central_dp(dp_pdev->dp);
		ath12k_wifi8_dp_get_ring_id(central_dp, &ring_id, skb);

		if (ath12k_wifi8_tx_recovery_drop(central_dp->ab)) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_FW_RECOVERY],
				     1, ring_id);
			return;
		}

		if (is_eth) {
			skb_new = skb_clone(skb, GFP_ATOMIC);
			if (!skb_new)
				goto next;
		} else {
			skb_new = skb_copy(skb, GFP_ATOMIC);
			if (!skb_new)
				goto next;
		}

		len = skb_new->len;

		msdu_info.group_slot = -1;
		msdu_info.mpsk_diff_encap = 0;
		ret = ath12k_wifi8_mcbc_setup_encryption(dp_vif, dp_pdev,
							 link_id, skb_new,
							 is_sta, is_eth,
							 &msdu_info,
							 vlan_ahvif, info, sta);
		if (ret) {
			dev_kfree_skb_any(skb_new);
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_MCBC_ENCRY_FAIL],
				     1, ring_id);
			goto next;
		}

		msdu_info.qos_nw_delay = qos_nw_delay;
		ret = ath12k_wifi8_mcbc_setup_msdu_info(arvif, dp_link_vif, arsta,
							central_dp, &msdu_info, gsn,
							gsn_valid, skb_new, is_eth,
							dp_pdev, vlan_ahvif);
		if (ret < 0) {
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		feature_ret = ath12k_wifi8_dp_tx_process_features(dp_vif, dp_pdev,
								  skb_new, &len,
								  &msdu_info,
								  skb_ctrl);

		if (feature_ret != DP_TX_FEATURE_SUCCESS) {
			if (feature_ret == DP_TX_RETURN)
				break;

			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_FEAT_ERR],
				     1, ring_id);
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		msdu_info.data_len = len;

		err = ath12k_wifi8_dp_tx_mcast_send(dp_pdev, ahvif, dp_link_vif,
						    ring_id, &msdu_info, gsn_valid,
						    gsn, skb_new, arsta, skb_ctrl,
						    htt_mesh);

		if (unlikely(err != DP_TX_ENQ_SUCCESS)) {
			DP_STATS_INC(dp_vif, tx_i.drop[err], 1, ring_id);
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		atomic_inc(&dp_pdev->num_tx_pending);

next:
		links_map &= ~BIT(link_id);
		link_id = find_first_bit(&links_map,
					 IEEE80211_MLD_MAX_NUM_LINKS);

	} while (link_id < IEEE80211_MLD_MAX_NUM_LINKS);

}

static inline void
ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(struct hal_tx_status *ts,
					       struct ath12k_dp *dp)
{
	ts->hw_link_id = (DP_GET_HW_LINK_ID_FRM_PPDU_ID(ts->ppdu_id,
							dp->link_id_offset,
							dp->link_id_bits));
}

static void ath12k_wifi8_dp_tx_free_txbuf(struct ath12k_dp *dp,
					  struct sk_buff *msdu,
					  struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct ath12k_pdev_dp *dp_pdev;

	ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr,
				  sw_metadata->len, DMA_TO_DEVICE);

	rcu_read_lock();

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

static u32 ath12k_wifi8_dp_tx_compute_hw_delay(struct hal_tx_status *ts)
{
	u32 hw_delay;

	hw_delay = ts->delay_stats.hw.wifi_sched_latency;

	if (hw_delay > HW_TX_DELAY_MAX)
		return 0;

	return hw_delay;
}

static void
ath12k_wifi8_dp_tx_htt_tx_complete_buf(struct ath12k_dp *dp,
				       struct sk_buff *msdu,
				       struct dp_tx_ring *tx_ring,
				       struct hal_tx_status *ts,
				       struct ath12k_tx_sw_metadata *sw_metadata,
				       u16 peer_id, struct ath12k_dp_peer *dp_peer)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev;
	struct ethhdr *eth;
	struct ieee80211_hdr *hdr;
	size_t hdrlen;
	enum ath12k_dp_eapol_key_type subtype;
	u32 hw_enq_tstamp = 0;
	u32 hw_delay = 0;
	u8 ring_id;

	ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr, sw_metadata->len,
				  DMA_TO_DEVICE);

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	skb_cb = ATH12K_SKB_CB(msdu);
	info = IEEE80211_SKB_CB(msdu);

	vif = ath12k_dp_peer_get_vif(dp_peer);
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		if (dp_pdev->wmm_stats.tx_type) {
			ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
			if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
				ahvif->wmm_stats.total_wmm_tx_drop[ahvif->wmm_stats.tx_type]++;
		}
	}

	if (msdu->protocol == cpu_to_be16(ETH_P_PAE)) {
		if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
			eth = (struct ethhdr *)msdu->data;
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 eth->h_dest);
		} else {
			hdr = (struct ieee80211_hdr *)msdu->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(msdu);
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + hdrlen +
							      LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX)
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 hdr->addr1);
		}
	}

	memset(&info->status, 0, sizeof(info->status));

	if (ts->acked) {
		if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		} else {
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
		}
	}

	if (!dp_peer || !ath12k_dp_peer_get_sta(dp_peer))
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n", peer_id);
	else {
		if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
		    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id
							(dp_peer, dp_pdev->hw_link_id);
			if (link_peer)
				WRITE_ONCE(link_peer->peer_stats.last_ack, jiffies);
		}
		status.sta = ath12k_dp_peer_get_sta(dp_peer);
	}

	if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
		if (unlikely(ath12k_debugfs_is_qos_stats_enabled(dp_pdev->ar))) {
			hw_delay = ts->delay_stats.hw.wifi_sched_latency;

			ath12k_qos_stats_update(dp_peer, ts->hw_link_id,
						dp_pdev->ar, msdu, ts,
						dp_pdev, msdu->tstamp,
						hw_delay);
		}

		if (unlikely(ath12k_dp_latency_stats_enabled(dp_pdev))) {
			hw_enq_tstamp = sw_metadata->hw_enqueue_tstamp;
			hw_delay = ath12k_wifi8_dp_tx_compute_hw_delay(ts);
			ring_id = tx_ring->tcl_data_ring_id;

			ath12k_dp_tx_update_peer_latency_stats(dp_pdev,
							       dp_peer,
							       ts, ring_id,
							       sw_metadata->skb,
							       hw_enq_tstamp,
							       hw_delay);
		}
	}

	status.info = info;
	status.skb = msdu;
	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
}

static void ath12k_wifi8_dp_tx_comp_update_peer_stats(struct ath12k_dp_peer *peer,
						      struct hal_tx_status *ts,
						      u16 tx_desc_flags,
						      u8 link_id, u32 msdu_len)
{
	if (peer->is_vdev_peer) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU) {
			if (tx_desc_flags & DP_TX_DESC_FLAG_BCAST)
				DP_PEER_DBG_STATS_PKT_LEN(peer, tx_dbg, bcast,
							  link_id, 1, msdu_len);
			if (tx_desc_flags & DP_TX_DESC_FLAG_MCAST)
				DP_PEER_DBG_STATS_PKT_LEN(peer, tx_dbg, mcast,
							  link_id, 1, msdu_len);
		}
	}

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM) {
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, release_src_not_tqm,
				      link_id, 1);
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, wbm_rel_reason[ts->status],
				      link_id, 1);
		return;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, multiple_retry_count,
					   link_id, ts->transmit_cnt > 2, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, ofdma, link_id,
					   ts->ofdma, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, amsdu_cnt, link_id,
					   ts->msdu_part_of_amsdu, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, non_amsdu_cnt, link_id,
					   !ts->msdu_part_of_amsdu, 1);
	}

	if (ts->status < HAL_WBM_TQM_REL_REASON_MAX) {
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, tqm_rel_reason[ts->status],
				      link_id, 1);
	}
}

static void
ath12k_wifi8_dp_tx_htt_update_peer_stats(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_peer *peer,
					 struct hal_tx_status *ts,
					 u32 msdu_len, u32 htt_status,
					 int link_id, int ring_id,
					 u8 tx_desc_flags)
{
	u8 vow_tid = 0;

	if (peer) {
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_wifi8_dp_tx_comp_update_peer_stats(peer,
									  ts,
									  tx_desc_flags,
									  link_id,
									  msdu_len);

			if (unlikely(ath12k_dp_vow_stats_enabled(dp_pdev))) {
				vow_tid = ath12k_vow_tid_validate(ts->tid);

				DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring_id, vow_tid,
							  htt_status_cnt, htt_status);
			}
		}

	} else {
		DP_DEVICE_STATS_INC(dp,
				    tx_err.tx_comp_err
				    [DP_TX_COMP_ERR_INVALID_PEER][ring_id],
				    1);
	}
}

static void
ath12k_wifi8_dp_tx_process_htt_tx_complete(struct ath12k_dp *dp,
					   void *desc, struct sk_buff *msdu,
					   struct dp_tx_ring *tx_ring,
					   struct ath12k_tx_sw_metadata *sw_metadata,
					   struct hal_tx_status *ts,
					   int ring_id, u32 htt_status)
{
	struct htt_tx_completion *status_desc;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_peer *peer = NULL;
	u8 link_id = 0;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	status_desc = desc;

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
		rcu_read_unlock();
		return;
	}

	if ((sw_metadata->flags & DP_TX_DESC_FLAG_FAST) &&
	    !ath12k_dp_stats_enabled(dp_pdev)) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	ts->ppdu_id = le32_get_bits(status_desc->info2,
				    HTT_TX_WBM_COMP_INFO2_PPDU_ID);
	ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(ts, dp);

	if (le32_get_bits(status_desc->info3, HTT_TX_WBM_COMP_INFO3_VALID)) {
		ts->peer_id = le32_get_bits(status_desc->info3,
					    HTT_TX_WBM_COMP_INFO3_SW_PEER_ID);
	} else {
		ts->peer_id = HAL_INVALID_PEERID;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer)
		link_id = ath12k_dp_validate_hw_link_id(ts->hw_link_id);

	/* For FAST path packets (bypassing mac80211), collect peer stats and
	 * free the SKB with dev_kfree_skb_any() before reaching the switch
	 * statement, which calls mac80211 functions not suitable for FAST path.
	 */
	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK) {
			ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
			ts->acked = true;
			ts->ack_rssi = le32_get_bits(status_desc->info2,
						     HTT_TX_WBM_COMP_INFO2_ACK_RSSI);
		} else if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP) {
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
		} else if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL) {
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
		}

		ath12k_wifi8_dp_tx_htt_update_peer_stats(dp, dp_pdev, peer, ts,
							 msdu_len, htt_status,
							 link_id, ring_id,
							 tx_desc_flags);

		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	switch (htt_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
		ts->acked = (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts->ack_rssi = le32_get_bits(status_desc->info2,
					     HTT_TX_WBM_COMP_INFO2_ACK_RSSI);

		ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
		ath12k_wifi8_dp_tx_htt_tx_complete_buf(dp, msdu, tx_ring, ts,
						       sw_metadata, ts->peer_id, peer);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
		ath12k_warn(dp->ab,
			    "smd-tx-dbg: FW TX drop htt_status=%u peer_id=%u ppdu_id=%u hw_link=%u\n",
			    htt_status, ts->peer_id, ts->ppdu_id,
			    ts->hw_link_id);
		switch (htt_status) {
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
			break;
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
			fallthrough;
		default:
			break;
		}
		fallthrough;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH:
		if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH)
			ath12k_warn(dp->ab,
				    "smd-tx-dbg: FW TX drop VDEVID_MISMATCH peer_id=%u ppdu_id=%u hw_link=%u\n",
				    ts->peer_id, ts->ppdu_id, ts->hw_link_id);
		ath12k_wifi8_dp_tx_free_txbuf(dp, msdu, sw_metadata);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY:
		/* This event is to be handled only when the driver decides to
		 * use WDS offload functionality.
		 */
		break;
	default:
		ath12k_warn(dp->ab, "Unknown htt tx status %d\n", htt_status);
		break;
	}

	ath12k_wifi8_dp_tx_htt_update_peer_stats(dp, dp_pdev, peer, ts,
						 msdu_len, htt_status,
						 link_id, ring_id,
						 tx_desc_flags);

	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_cache_peer_stats(struct ath12k *ar,
				    struct sk_buff *msdu,
				    struct hal_tx_status *ts)
{
	struct ath12k_per_peer_tx_stats *peer_stats = &ar->cached_stats;

	if (ts->try_cnt > 1) {
		peer_stats->retry_pkts += ts->try_cnt - 1;
		peer_stats->retry_bytes += (ts->try_cnt - 1) * msdu->len;

		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
			peer_stats->failed_pkts += 1;
			peer_stats->failed_bytes += msdu->len;
		}
	}
}

static void
ath12k_wifi8_dp_tx_update_txcompl(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_peer *dp_peer;
	struct rate_info txrate = {0};
	u16 rate, ru_tones;
	u8 rate_idx = 0;
	int ret;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (!dp_peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "MLD peer NA with peer_id: %u\n", ts->peer_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ts->hw_link_id);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	sta = ath12k_dp_link_peer_get_sta(peer);
	ahsta = ath12k_sta_to_ahsta(sta);

	if (peer->last_txrate.nss)
		txrate.nss = peer->last_txrate.nss;
	spin_unlock_bh(&dp->dp_lock);

	switch (ts->pkt_type) {
	case HAL_TX_RATE_STATS_PKT_TYPE_11A:
	case HAL_TX_RATE_STATS_PKT_TYPE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(ts->mcs,
							    ts->pkt_type,
							    &rate_idx,
							    &rate);
		if (ret < 0) {
			ath12k_warn(ab, "Invalid tx legacy rate %d\n", ret);
			return;
		}

		txrate.legacy = rate;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11N:
		if (ts->mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ab, "Invalid HT mcs index %d\n", ts->mcs);
			return;
		}

		if (txrate.nss < 1 ||
		    (dp_pdev->ar->pdev->cap.max_tx_nss &&
		     (txrate.nss > dp_pdev->ar->pdev->cap.max_tx_nss)) ||
		      (txrate.nss > hweight32(dp_pdev->ar->pdev->cap.tx_chain_mask)))
			ath12k_warn(ab, "Invalid nss value: %d", txrate.nss);
		else
			txrate.mcs = ts->mcs + 8 * (txrate.nss - 1);

		txrate.flags = RATE_INFO_FLAGS_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AC:
		if (ts->mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid VHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_VHT_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AX:
		if (ts->mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ab, "Invalid HE mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		txrate.he_gi = ath12k_he_gi_to_nl80211_he_gi(ts->sgi);
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11BE:
		if (ts->mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid EHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		txrate.eht_gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(ts->sgi);
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11BN:
		if (ts->mcs > ATH12K_UHR_MCS_MAX) {
			ath12k_warn(ab, "Invalid UHR mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_UHR_MCS;

		/*
		 * We fill EHT params for UHR mode as well since
		 * the APIs such as _cfg80211_calculate_bitrate_eht_uhr() etc.
		 * remain common and use EHT params to calculate Tx Bit rate etc.
		 */
		txrate.eht_gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(ts->sgi);
		break;
	default:
		ath12k_warn(ab, "Invalid tx pkt type: %d\n", ts->pkt_type);
		return;
	}

	txrate.bw = ath12k_mac_bw_to_mac80211_bw(ts->bw);

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		txrate.bw = RATE_INFO_BW_HE_RU;
		ru_tones = ath12k_mac_he_convert_tones_to_ru_tones(ts->tones);
		txrate.he_ru_alloc =
			ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	}

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE) {
		txrate.bw = RATE_INFO_BW_EHT_RU;
		txrate.eht_ru_alloc =
			ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(ts->tones);
	}

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BN) {
		txrate.bw = RATE_INFO_BW_EHT_RU;
		txrate.eht_ru_alloc =
			ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(ts->tones);
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ts->hw_link_id);
	if (peer)
		peer->txrate = txrate;
	else
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_wifi8_dp_tx_complete_msdu(struct ath12k_pdev_dp *dp_pdev,
					     struct sk_buff *msdu,
					     struct hal_tx_status *ts,
					     struct ath12k_tx_sw_metadata *sw_metadata,
					     u8 mac_id, int ring)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_rate_status status_rate = { 0 };
	struct rate_info rate;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k *ar;
	struct ath12k_dp_peer *peer = NULL;
	u8 hw_link_id = 0;
	u8 reason = 0;
	u8 tid = 0;
	enum ath12k_dp_tx_comp_error drop_reason = DP_TX_COMP_ERR_MISC;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;
	u8 vow_tid = 0;
	u32 hw_enq_tstamp = 0;
	u32 hw_delay = 0;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_TQM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	if (sw_metadata->skb)
		ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr,
					  sw_metadata->len, DMA_TO_DEVICE);

	dp_pdev->wmm_stats.tx_type =
		ath12k_tid_to_ac(ts->tid > ATH12K_DSCP_PRIORITY ? 0 : ts->tid);
	if (dp_pdev->wmm_stats.tx_type) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
			dp_pdev->wmm_stats.total_wmm_tx_drop[dp_pdev->wmm_stats.tx_type]++;
	}

	skb_cb = ATH12K_SKB_CB(msdu);

	rcu_read_lock();

	if (!rcu_dereference(ab->pdevs_active[dp_pdev->mac_id])) {
		drop_reason = DP_TX_COMP_ERR_INVALID_PDEV;
		goto exit;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	vif = ath12k_dp_peer_get_vif(peer);
	if (!vif) {
		drop_reason = DP_TX_COMP_ERR_INVALID_VIF;
		goto exit;
	}

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	/* skip tx rate update from ieee80211_status*/
	info->status.rates[0].idx = -1;

	ar = dp_pdev->ar;

	if (peer) {
		hw_link_id = ath12k_dp_validate_hw_link_id(ts->hw_link_id);

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_wifi8_dp_tx_comp_update_peer_stats(peer,
									  ts,
									  tx_desc_flags,
									  hw_link_id,
									  msdu_len);
			if (unlikely(ath12k_debugfs_is_qos_stats_enabled(ar))) {
				hw_delay = ts->delay_stats.hw.wifi_sched_latency;

				ath12k_qos_stats_update(peer, hw_link_id,
							ar, msdu, ts,
							dp_pdev, msdu->tstamp,
							hw_delay);
			}

			if (unlikely(ath12k_dp_latency_stats_enabled(dp_pdev))) {
				hw_enq_tstamp = sw_metadata->hw_enqueue_tstamp;
				hw_delay =
					ath12k_wifi8_dp_tx_compute_hw_delay(ts);

				ath12k_dp_tx_update_peer_latency_stats(dp_pdev,
								       peer, ts,
								       ring,
								       sw_metadata->skb,
								       hw_enq_tstamp,
								       hw_delay);
			}

			if (ath12k_tid_stats_enabled(dp_pdev)) {
				ahvif = ath12k_vif_to_ahvif(vif);
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_COMPLETED_PKTS);
			}

			/* Update peer level protocol stats at TX completion */
			if (unlikely(ath12k_proto_stats_enabled(dp_pdev)))
				ath12k_dp_tx_peer_update_proto_stats(peer,
								     hw_link_id,
								     msdu,
								     TX_COMP,
								     ring);

			if (unlikely(ath12k_dp_vow_stats_enabled(dp_pdev))) {
				vow_tid = ath12k_vow_tid_validate(ts->tid);

				DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring, vow_tid,
							  tqm_status_cnt,
							  ts->status);
			}
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring], 1);
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !(info->flags & IEEE80211_TX_CTL_NO_ACK))	{
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ts->ack_rssi;

		if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
			      ab->wmi_ab.svc_map))
			info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

		info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
	    (info->flags & IEEE80211_TX_CTL_NO_ACK))
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts->status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_TQM_REMOVE_MPDU;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_TQM_THRESHOLD;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_TQM_REMOVE_AGED;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_TQM_REMOVE_TX;
			goto exit;
		default:
			/* TODO: Remove this print and add as a stats */
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts->status);
		}
	}

	/* NOTE: Tx rate status reporting. Tx completion status does not have
	 * necessary information (for example nss) to build the tx rate.
	 * Might end up reporting it out-of-band from HTT stats.
	 */

	if (ath12k_extd_tx_stats_enabled(&ar->dp)) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath12k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath12k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath12k_wifi8_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

	ath12k_wifi8_dp_tx_update_txcompl(dp_pdev, ts);

	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
							     ts->peer_id);
	if (!link_peer || !ath12k_dp_link_peer_get_sta(link_peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   ts->peer_id);
		drop_reason = DP_TX_COMP_ERR_INVALID_LINK_PEER;
		goto exit;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
		WRITE_ONCE(link_peer->peer_stats.last_ack, jiffies);
	}

	status.sta = ath12k_dp_link_peer_get_sta(link_peer);
	status.info = info;

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	status.skb = msdu;
	rate = link_peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;

	ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	rcu_read_unlock();
	return;

exit:
	DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[drop_reason][ring], 1);

	if (ahvif && ath12k_dp_stats_enabled(dp_pdev) &&
			ath12k_tid_stats_enabled(dp_pdev))
		ath12k_tid_tx_drop_stats(ahvif, ts->tid, msdu_len, reason);

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	rcu_read_unlock();
}

static void
ath12k_wifi8_dp_tx_status_parse(struct ath12k_base *ab,
				struct hal_tqm2sw_completion_ring *desc,
				struct hal_tx_status *ts)
{
	u32 info0 = le32_to_cpu(desc->rate_stats.info0);

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
	    ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)
		return;

	ts->ppdu_id = le32_get_bits(desc->info1,
				    HAL_TQM2SW_COMPLETION_RING_INFO1_TQM_STATUS_NUMBER);

	ts->ack_rssi = FIELD_GET(HAL_TQM2SW_COMPLETION_RING_INFO2_ACK_FRAME_RSSI,
				 desc->info2);

	ts->peer_id = le16_to_cpu(desc->sw_peer_id);

	if (info0 & HAL_TX_RATE_TID_INFO_INFO0_TX_RATE_STATS_INFO_VALID) {
		ts->pkt_type = u32_get_bits(info0,
					    HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_PKT_TYPE);
		ts->mcs = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_MCS);
		ts->sgi = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_SGI);
		ts->bw = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TRANSMIT_BW);
		ts->tones = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TONES_IN_RU);
		ts->ofdma = u32_get_bits(info0,
					 HAL_TX_RATE_TID_INFO_INFO0_OFDMA_TRANSMISSION);
		ts->tid = u32_get_bits(info0, HAL_TX_RATE_TID_INFO_INFO0_TID);
	}

	ts->transmit_cnt = le32_get_bits(desc->info1,
					 HAL_TQM2SW_COMPLETION_RING_INFO1_TRANSMIT_COUNT);
	ts->first_msdu = le32_get_bits(desc->info2,
				       HAL_TQM2SW_COMPLETION_RING_INFO2_FIRST_MSDU);
	ts->last_msdu = le32_get_bits(desc->info2,
				      HAL_TQM2SW_COMPLETION_RING_INFO2_LAST_MSDU);
	ts->msdu_part_of_amsdu =
			(ts->first_msdu && ts->last_msdu) ? false : true;

	ts->delay_stats.hw.nw_latency_valid = le32_get_bits(desc->info2,
			HAL_TQM2SW_COMPLETION_RING_INFO2_NW_LATENCY_VLD);
	ts->delay_stats.hw.nw_latency = le32_get_bits(desc->info2,
			HAL_TQM2SW_COMPLETION_RING_INFO2_NW_LATENCY);

	ts->delay_stats.hw.stream_id_valid = le32_get_bits(desc->info3,
			HAL_TQM2SW_COMPLETION_RING_INFO3_TELE_STREAM_ID_VLD);
	ts->delay_stats.hw.stream_id = le32_get_bits(desc->info4,
			HAL_TQM2SW_COMPLETION_RING_INFO4_TELE_STREAM_ID);

	ts->delay_stats.hw.wifi_sched_latency = le32_get_bits(desc->info4,
			HAL_TQM2SW_COMPLETION_RING_INFO4_WIFI_SCHED_LATENCY);

	ath12k_wifi8_dp_tx_get_hw_link_id_from_ppdu_id(ts, ab->dp);
}

int ath12k_wifi8_dp_tx_mec_handler(struct ath12k_dp *dp,
				   struct ath12k_tx_desc_info *tx_desc,
				   int status)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	u8 *sa_addr = NULL;
	struct ethhdr *eth = NULL;
	struct sk_buff *skb = tx_desc->skb;
	u8 pdev_id = tx_desc->hw_link_id;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	int ret = 0;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};
	struct ieee80211_hdr *hdr;
	bool is_mcast = false, is_eth = false;

	/* get the src address from the packet */
	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		sa_addr = eth->h_source;
	} else {
		sa_addr = ieee80211_get_SA((struct ieee80211_hdr *)(skb->data));
	}

	if (status == HAL_WBM_TQM_REL_REASON_TCL_MEC_SEARCH_FAIL_FOR_VDEV) {
		ath12k_core_dma_unmap_single(dp->dev, tx_desc->paddr,
					     tx_desc->len, DMA_TO_DEVICE);
		if (tx_desc->paddr_ext_desc) {
			ath12k_core_dma_unmap_single(dp->dev,
						     tx_desc->paddr_ext_desc,
						     tx_desc->ext_desc_len,
						     DMA_TO_DEVICE);
			dev_kfree_skb_any(tx_desc->skb_ext_desc);
		}
		ath12k_dp_tx_release_txbuf(dp, tx_desc, tx_desc->pool_id);

		rcu_read_lock();
		dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, pdev_id);
		if (!dp_pdev) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: dp_pdev is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}
		if (atomic_dec_and_test(&dp_pdev->num_tx_pending))
			wake_up(&dp_pdev->tx_empty_waitq);

		memcpy(ast_param.mac_addr, sa_addr, ETH_ALEN);
		ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_MEC;
		ret = ath12k_dp_ast_entry_create(dp->dp_hw_grp, &ast_param);
		if (ret && ret != -EALREADY) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "unable to create the mec entry %pM", sa_addr);
			rcu_read_unlock();
			return ret;
		}

		/* reinject the packet back */
		vif = skb_cb->vif;
		if (!vif) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: vif is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}
		ahvif = ath12k_vif_to_ahvif(vif);
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		if (!arvif) {
			dev_kfree_skb_any(skb);
			ath12k_err(dp->ab, "MEC: arvif is null %d", pdev_id);
			rcu_read_unlock();
			return -EINVAL;
		}

		hdr = (struct ieee80211_hdr *)skb->data;

		if (info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
			eth = (struct ethhdr *)skb->data;
			is_eth = true;
			is_mcast = is_multicast_ether_addr(eth->h_dest);
		} else {
			is_mcast = is_multicast_ether_addr(hdr->addr1);
		}

		if (is_mcast)
			ath12k_wifi8_mcbc_handler(&arvif->ahvif->dp_vif, arvif->link_id,
						  NULL, skb, is_eth, false, false,
						  &skb_ctrl, 0, false, NULL, info, NULL);
		else
			ath12k_wifi8_ucast_handler(&arvif->ahvif->dp_vif, arvif->link_id,
						   NULL, skb, &skb_ctrl, 0, NULL, NULL);

		rcu_read_unlock();
	} else if (status == HAL_WBM_TQM_REL_REASON_TCL_MEC_KEEP_ALIVE_FOR_VDEV) {
		ret = ath12k_mec_entry_keep_alive_update(dp->dp_hw_grp, sa_addr);
	}

	return ret;
}

static inline bool
ath12k_wifi8_dp_tx_mec_filter(struct ath12k_dp *dp,
			      struct ath12k_tx_desc_info *tx_desc,
			      int status)
{
	int ret;

	if (unlikely(status == HAL_WBM_TQM_REL_REASON_TCL_MEC_SEARCH_FAIL_FOR_VDEV ||
		     status == HAL_WBM_TQM_REL_REASON_TCL_MEC_KEEP_ALIVE_FOR_VDEV)) {
		ret = ath12k_wifi8_dp_tx_mec_handler(dp, tx_desc, status);
		if (ret)
			ath12k_err(dp->ab, "MEC: pkt drop in MEC handling ret = %d", ret);

		return true;
	}

	return false;
}

int ath12k_wifi8_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	struct ath12k_dp_tx_comp_status tx_comp_status;
	int i;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
	struct ath12k_wifi8_tx_status_entry *tx_status_entry_next;
	struct sk_buff *skb;
#endif
	int orig_budget = budget;
	struct ath12k_skb_cb *skb_cb;
	struct sk_buff *msdu = NULL;
	struct ath12k_vif *ahvif = NULL;
	bool fast_flag;
	int pdev_tx_comp_cnt[ATH12K_GROUP_MAX_RADIO] = {0};
	u8 hw_link_id = 0;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_wifi8_tx_status_entry *tx_status_entry;
	struct ath12k_tx_sw_metadata *sw_metadata;
	struct ath12k_tx_sw_metadata *sw_metadata_pf;
	u8 n_entry = 0, idx = 0;
	struct list_head desc_free_list;
	struct hal_tqm2sw_completion_ring *tx_status;
	struct sk_buff_head free_list_head;
	int tx_status_idx = smp_processor_id();
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] = {0};
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON] = {0};
	u32 fw_tx_status[MAX_FW_TX_STATUS] = {0};
	u32 htt_status = 0, tx_completed = 0;
	u32 tx_desc_free_cnt = 0, *used_cnt;
	u8 tid = 0;

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_dp_srng_dst_invalidate_entries(dp, status_ring, valid_entries);
#endif

	INIT_LIST_HEAD(&desc_free_list);
	skb_queue_head_init(&free_list_head);

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)
				dp_hw_grp->tx_status_buf[tx_status_idx];
	while (budget-- &&
	       (tx_status = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL))) {
		ath12k_wifi8_hal_tx_completion_process(tx_status, &tx_comp_status);
		tx_desc = (struct ath12k_tx_desc_info *)((unsigned long)tx_comp_status.tx_desc);
		if (unlikely(!tx_desc)) {
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_DESC][ring_id], 1);
			ath12k_warn(ab, "unable to retrieve tx_desc!");
			continue;
		}

		if (ath12k_wifi8_dp_tx_mec_filter(dp, tx_desc,
						  tx_comp_status.u.htt_status))
			continue;

		tx_status_entry->tx_desc = tx_desc;

#ifdef CONFIG_IO_COHERENCY
		prefetch(tx_desc);
#endif
		n_entry++;
		memcpy(&tx_status_entry->tx_status, tx_status, sizeof(*tx_status));
		tx_status_entry++;
	}

	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	if (!n_entry)
		return orig_budget - budget;

	spin_lock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)
				dp_hw_grp->tx_status_buf[tx_status_idx];
	for (i = 0; i < n_entry; i++) {
		sw_metadata = &tx_status_entry->sw_metadata;
		tx_desc = tx_status_entry->tx_desc;
		tx_status_entry++;

#ifndef CONFIG_IO_COHERENCY
		if ((i + 10) < n_entry) {
			tx_status_entry_next = tx_status_entry + 8;

			prefetch(tx_status_entry_next->tx_desc);
			prefetch((tx_status_entry_next + 1));
		}
#endif

		if (unlikely(!tx_desc->in_use)) {
			sw_metadata->skb = NULL;
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_DESC_INUSE][ring_id], 1);
			continue;
		}

		if (likely(!tx_desc->spl_desc))
			list_add_tail(&tx_desc->list, &desc_free_list);
		else
			list_add_tail(&tx_desc->list,
				      &dp->dp_hw_grp->tx_spl_desc_free_list[ring_id]);

		tx_desc_free_cnt++;

		sw_metadata->skb = tx_desc->skb;
		sw_metadata->paddr = tx_desc->paddr;
		sw_metadata->len = tx_desc->len;
		sw_metadata->flags = tx_desc->flags;
		sw_metadata->hw_enqueue_tstamp = tx_desc->hw_enqueue_tstamp;

		if (unlikely(!(sw_metadata->flags & DP_TX_DESC_FLAG_FAST))) {
			if (tx_desc->ext_kmem) {
				/* Unmap SG buffers */
				if (tx_desc->is_from_sg) {
					ath12k_dp_tx_sg_unmap_buf(dp, tx_desc->ext_desc,
								  tx_desc->skb);
					tx_desc->is_from_sg = 0;
				}
				ath12k_core_dma_unmap_single(dp->dev,
							     tx_desc->paddr_ext_desc,
							     tx_desc->ext_desc_len,
							     DMA_TO_DEVICE);
				kmem_cache_free(dp->ext_cache, tx_desc->ext_desc);
				tx_desc->paddr_ext_desc = 0;
				tx_desc->ext_desc_len = 0;
				tx_desc->ext_desc = NULL;
				tx_desc->ext_kmem = 0;
			}
		}

		sw_metadata->hw_link_id = tx_desc->hw_link_id;
		pdev_tx_comp_cnt[sw_metadata->hw_link_id]++;

		tx_desc->skb = NULL;
		tx_desc->in_use = false;
		tx_desc->flags = 0;
	}

	list_splice(&desc_free_list, &dp->dp_hw_grp->tx_desc_free_list[ring_id]);

	used_cnt = this_cpu_ptr(dp_hw_grp->tx_desc_used_cnt);
	(*used_cnt) -= tx_desc_free_cnt;

	spin_unlock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	for (hw_link_id = 0; hw_link_id < ATH12K_GROUP_MAX_RADIO; hw_link_id++) {
		if (likely(pdev_tx_comp_cnt[hw_link_id])) {
			rcu_read_lock();
			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
			rcu_read_unlock();

			if (unlikely(!dp_pdev))
				continue;

			if (atomic_sub_and_test(pdev_tx_comp_cnt[hw_link_id],
						&dp_pdev->num_tx_pending))
				wake_up(&dp_pdev->tx_empty_waitq);
		}
	}

	tx_status_entry = (struct ath12k_wifi8_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	while (n_entry--) {
		fast_flag = false;
		tx_status = &tx_status_entry->tx_status;
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		if (!sw_metadata->skb)
			continue;

		tx_completed++;
		ts.buf_rel_source =
		    le32_get_bits(tx_status->info0,
				  HAL_TQM2SW_COMPLETION_RING_INFO0_RELEASE_SOURCE_MODULE);

		tx_wbm_rel_source[ts.buf_rel_source]++;

		msdu = sw_metadata->skb;
		if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			skb_cb = ATH12K_SKB_CB(msdu);
			ahvif = ath12k_vif_to_ahvif(skb_cb->vif);
			tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, msdu->len,
					    ATH_TX_WBM_REL_SRC);
		}

		if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
			ts.status = le32_get_bits(tx_status->info0,
				     HAL_TQM2SW_COMPLETION_RING_INFO0_TQM_RELEASE_REASON);
			tqm_rel_reason[ts.status]++;
		} else if (ts.buf_rel_source == HAL_TQM_REL_SRC_MODULE_FW) {
			htt_status = le32_get_bits(tx_status->info0,
						   HTT_TX_WBM_COMP_INFO0_STATUS);
			fw_tx_status[htt_status]++;
			if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_FW_STATUS);
			}

			ath12k_wifi8_dp_tx_process_htt_tx_complete(dp,
								   (void *)tx_status,
								   sw_metadata->skb,
								   tx_ring, sw_metadata,
								   &ts, ring_id,
								   htt_status);
			sw_metadata->skb = NULL;
			continue;
		}

		if (likely(n_entry >= 1)) {
			sw_metadata_pf = &tx_status_entry->sw_metadata;
			prefetchw(sw_metadata_pf->skb);
		}

		if ((sw_metadata->flags & DP_TX_DESC_FLAG_FAST) &&
		    dp_pdev && !ath12k_dp_stats_enabled(dp_pdev)) {
			if (likely(sw_metadata->flags & DP_TX_DESC_FLAG_RECYCLE)) {
				ATH12K_TX_BUFFER_UNMAP(dp->dev, sw_metadata->paddr,
						       sw_metadata->len,
						       DMA_TO_DEVICE);
				__skb_queue_tail(&free_list_head, sw_metadata->skb);
#ifdef CONFIG_IO_COHERENCY
				prefetch((uint8_t *)sw_metadata->skb + 64);
				prefetch((uint8_t *)sw_metadata->skb + 128);
				prefetch((uint8_t *)sw_metadata->skb + 192);
#endif
				sw_metadata->skb = NULL;
				fast_flag = true;
			}
		}

		if (n_entry == 1)
			prefetch(&dp->device_stats);

		if (unlikely(!fast_flag && sw_metadata->skb)) {
			rcu_read_lock();

			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp,
							      sw_metadata->hw_link_id);
			if (!dp_pdev) {
				DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
				rcu_read_unlock();
				continue;
			}

			ath12k_wifi8_dp_tx_status_parse(ab, tx_status, &ts);
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_COMPLETED_PKTS);
			}
			ath12k_wifi8_dp_tx_complete_msdu(dp_pdev, sw_metadata->skb, &ts,
							 sw_metadata,
							 sw_metadata->hw_link_id,
							 ring_id);
			sw_metadata->skb = NULL;

			rcu_read_unlock();
		}
	}

#ifndef CONFIG_IO_COHERENCY
	skb_queue_walk(&free_list_head, skb) {
		prefetch((uint8_t *)skb + 64);
		prefetch((uint8_t *)skb + 128);
		prefetch((uint8_t *)skb + 192);
	}
#endif
	dp->device_stats.tx_comp_stats[ring_id].tx_completed += tx_completed;

	for (idx = 0; idx < HAL_WBM_REL_SRC_MODULE_MAX; idx++)
		dp->device_stats.tx_comp_stats[ring_id].tx_wbm_rel_source[idx] +=
			tx_wbm_rel_source[idx];

	for (idx = 0; idx < MAX_TQM_RELEASE_REASON; idx++)
		dp->device_stats.tx_comp_stats[ring_id].tqm_rel_reason[idx] +=
			tqm_rel_reason[idx];

	for (idx = 0; idx < MAX_FW_TX_STATUS; idx++)
		dp->device_stats.tx_comp_stats[ring_id].fw_tx_status[idx] +=
			fw_tx_status[idx];

	dev_kfree_skb_list_fast(&free_list_head);

	return orig_budget - budget;
}

u32 ath12k_wifi8_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable)
{
	u32 bank_config = 0;
	enum hal_encrypt_type encrypt_type = 0;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	u32 key_cipher = ahvif->deflink.key_cipher;
	bool vdev_id_check_en;

	if (force_vdev_id_check_disable)
		vdev_id_check_en = false;
	else
		vdev_id_check_en = dp_vif->vdev_id_check_en;

	/* Only valid for raw frames with HW crypto enabled.
	 * With SW crypto, mac80211 sets key per packet
	 */
	if (dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW &&
	    test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    key_cipher != INVALID_CIPHER)
		bank_config |=
			u32_encode_bits(ath12k_dp_tx_get_encrypt_type(key_cipher),
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);
	else
		encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

	bank_config |= u32_encode_bits(dp_vif->tx_encap_type,
				       HAL_TX_BANK_CONFIG_ENCAP_TYPE) |
			u32_encode_bits(encrypt_type,
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);

	bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_SRC_BUFFER_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_LINK_META_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_EPD);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		bank_config |= u32_encode_bits(1, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);
	else
		bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);

	bank_config |= u32_encode_bits(dp_vif->hal_addr_search_flags &
				       HAL_TX_ADDRX_EN,	HAL_TX_BANK_CONFIG_ADDRX_EN) |
			u32_encode_bits(!!(dp_vif->hal_addr_search_flags &
					HAL_TX_ADDRY_EN),
					HAL_TX_BANK_CONFIG_ADDRY_EN);

	bank_config |= u32_encode_bits(ieee80211_vif_is_mesh(ahvif->vif) ? 3 : 0,
					HAL_TX_BANK_CONFIG_MESH_EN) |
			u32_encode_bits(vdev_id_check_en,
					HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN);

	/*TODO need to revist with qos implementation */
	bank_config |= u32_encode_bits(ahvif->map_id,
				       HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID);

	return bank_config;
}

int ath12k_wifi8_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb, struct ath12k_link_sta *arsta,
				       struct ath12k_dp_peer *dp_peer)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath12k_dp_vif *dp_vif = &arvif->ahvif->dp_vif;
	u32 info_flags = info->flags;
	bool is_mcast = false, is_eth = false;
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};

	/* Native WiFi format */
	hdr = (struct ieee80211_hdr *)skb->data;

	/* Check if HW encapsulation */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		is_eth = true;
		is_mcast = is_multicast_ether_addr(eth->h_dest);
	} else {
		is_mcast = is_multicast_ether_addr(hdr->addr1);
	}

	if (is_mcast)
		ath12k_wifi8_mcbc_handler(dp_vif, arvif->link_id, arsta, skb,
					  is_eth, false, false, &skb_ctrl,
					  0, false, NULL, info, NULL);
	else
		ath12k_wifi8_ucast_handler(dp_vif, arvif->link_id,
					   arsta, skb, &skb_ctrl, 0, NULL, dp_peer);
	return 0;
}

/*
 * ath12k_wifi8_dp_tx_exception_to_tx_comp() - Redirect TX exception buffer
 * to the normal TX completion ring instead of freeing it directly.
 *
 * Buffers arriving in the TX exception ring are not freed here. A SW2TQM
 * command is sent so C-TQM routes the completion to the normal TX completion
 * ring.
 */
int ath12k_wifi8_dp_tx_exception_to_tx_comp(struct ath12k_dp *dp,
						dma_addr_t paddr,
						u8 rbm,
						u32 cookie)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal_tqm_cmd cmd = {};
	int ret;

	/*
	 * Follow the ath12k_wifi8_dp_tqm_cmd_send pattern:
	 * populate fw_buf_release_params and call the DP-level send function
	 * which routes through ath12k_wifi8_hal_tqm_cmd_send() →
	 * case HAL_TQM_FW_COMPLETION_BO → ath12k_wifi8_hal_tqm_fw_buf_release_cmd().
	 */
	cmd.fw_buf_release_params.paddr  = paddr;
	cmd.fw_buf_release_params.rbm    = rbm;
	cmd.fw_buf_release_params.cookie = cookie;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_FW_COMPLETION_BO,
					   &cmd, NULL, NULL);
	if (ret < 0)
		ath12k_warn(ab,
			    "fw_buf: cmd send failed cookie=0x%x rbm=%u ret=%d\n",
			    cookie, rbm, ret);

	return ret;
}

/*
 * ath12k_wifi8_dp_fw_tqm2sw_handler() - Process fw_tqm2sw_ring completions
 *
 * This ring receives completions for TX exception ring entries where the
 * SW cookie has a non-zero RBM (bits[19:18] != 0), i.e. buffers that belong
 * to the FW's buffer pool rather than the host's pool.  During target
 * recovery, R-FW-owned buffers arrive in the TX exception ring; instead of
 * freeing them directly the host sends a SW2TQM sync command with
 * HAL_TQM_HOST_STATUS_RING=1, and C-TQM routes the resulting completion here.
 *
 * Called when ring_mask->fw_tqm2sw fires (interrupt group 14).
 */
int ath12k_wifi8_dp_tqm2sw_fw_handler(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct hal_tqm2sw_completion_ring *desc;
	struct hal_srng *srng;
	struct ath12k_hal_tqm_cmd cmd;
	int count = 0;
	int quota = budget;
	int ret;
	u8 link_id, chip_id, rbm;
	u32 cookie;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return 0;

	srng = &ab->hal.srng_list[dp_wifi8->tqm2sw_fw_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- > 0) {
		desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!desc)
			break;

		memset(&cmd, 0, sizeof(cmd));
		cmd.fw_buf_release_params.paddr =
			((u64)le32_get_bits(desc->buf_addr_info.info1,
					    BUFFER_ADDR_INFO1_ADDR) << 32) |
			le32_get_bits(desc->buf_addr_info.info0,
				      BUFFER_ADDR_INFO0_ADDR);
		cookie = le32_get_bits(desc->buf_addr_info.info1,
				       BUFFER_ADDR_INFO1_SW_COOKIE);

		/*
		 * Derive the correct FW RBM from the cookie's link_id field
		 * (ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID field, bits [15:13]).
		 * Extract link_id, look up chip_id (device_id) via hw_links[],
		 * and map to the correct FW buffer manager for the owning chip.
		 */
		link_id = (u8)ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_GET(cookie);
		if (link_id >= ATH12K_GROUP_MAX_RADIO) {
			ath12k_warn(ab,
				    "tqm2sw_fw: invalid link_id %u (max %u) cookie=0x%x\n",
				    link_id, ATH12K_GROUP_MAX_RADIO, cookie);
			continue;
		}

		chip_id = dp->dp_hw_grp->hw_links[link_id].device_id;
		switch (chip_id) {
		case 0:
			rbm = HAL_RX_BUF_RBM_FW_CHIP0_BM;
			break;
		case 1:
			rbm = HAL_RX_BUF_RBM_FW_CHIP1_BM;
			break;
		case 2:
			rbm = HAL_RX_BUF_RBM_FW_CHIP2_BM;
			break;
		case 3:
			rbm = HAL_RX_BUF_RBM_FW_CHIP3_BM;
			break;
		case 4:
			rbm = HAL_RX_BUF_RBM_FW_CHIP4_BM;
			break;
		default:
			ath12k_warn(ab,
				    "tqm2sw_fw: unknown chip_id %u (link_id %u) cookie=0x%x\n",
				    chip_id, link_id, cookie);
			continue;
		}

		cmd.fw_buf_release_params.rbm    = rbm;
		cmd.fw_buf_release_params.cookie = cookie;

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_FW_COMPLETION_BO,
						   &cmd, NULL, NULL);
		if (ret < 0)
			ath12k_warn(ab,
				    "tqm2sw_fw: cmd send failed ret=%d\n", ret);

		count++;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

void ath12k_wifi8_dp_tx_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int i;

	ath12k_wifi8_hal_sam_cmd_staging_free(ab);
	ath12k_wifi8_hal_tqm_cmd_staging_free(ab);

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_comp_ring);
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_data_ring);
	}
	/* Cleanup dedicated FW TQM2SW ring (V1 WAR only) */
	if (ab->hw_params->tqm2sw_fw_war)
		ath12k_dp_srng_cleanup(ab, &dp_wifi8->tqm2sw_fw_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tx_exception);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tcl_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tcl_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tqm_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tqm_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->sam_status_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->sam_cmd_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_cmd_ring);
}

int ath12k_wifi8_dp_tx_ring_alloc(struct ath12k_base *ab)
{
	int i, tx_comp_ring_num;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	int ret;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;

		ret = ath12k_dp_srng_alloc(ab, &dp->tx_ring[i].tcl_data_ring,
					   HAL_TCL_DATA, i, 0,
					   ath12k_dp_tcl_data_ring_size[i]);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_data ring (%d) :%d\n",
				    i, ret);
			goto err;
		}

		ret = ath12k_dp_srng_alloc(ab, &dp->tx_ring[i].tcl_comp_ring,
					   HAL_TX_COMPLETION, tx_comp_ring_num, 0,
					   ath12k_dp_tx_comp_ring_size[i]);
		if (ret) {
			ath12k_warn(ab, "failed to set up tx_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			goto err;
		}
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tx_exception, HAL_TX_EXCEPTION, 0, 0,
				   DP_TX_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up wbm2sw_release ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tcl_status_ring, HAL_TCL_STATUS, 0, 0,
				   DP_TCL_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tcl_status ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tcl_cmd_ring, HAL_TCL_CMD, 0, 0,
				   DP_TCL_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tcl_cmd ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tqm_cmd_ring, HAL_TQM_CMD, 0, 0,
				   DP_TQM_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tqm command ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_wifi8_hal_tqm_cmd_staging_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to allocate tqm staging buffer :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tqm_status_ring, HAL_TQM_STATUS, 0, 0,
				   DP_TQM_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up tqm_status ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->rx_ase_cmd_ring, HAL_ASE_CMD_RING, 0, 0,
				   DP_RX_ASE_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up ase_cmd ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->sam_cmd_ring, HAL_SAM_CMD,
				   0, 0, DP_SAM_CMD_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup sam cmd ring: %d\n", ret);
		goto err;
	}

	ret = ath12k_wifi8_hal_sam_cmd_staging_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to allocate sam staging buffer :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->sam_status_ring, HAL_SAM_STATUS,
				   0, 0, DP_SAM_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup sam status ring: %d\n", ret);
		goto err;
	}

	return 0;

err:
	ath12k_wifi8_dp_tx_ring_cleanup(ab);
	return ret;
}

int ath12k_wifi8_dp_tx_ring_init(struct ath12k_base *ab)
{
	int i, tx_comp_ring_num;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	int ret;
	u8 rbm_id;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;
		rbm_id = map[i].rbm_id;

		ret = ath12k_dp_srng_init(ab, &dp->tx_ring[i].tcl_data_ring,
					  HAL_TCL_DATA, i, 0);
		if (ret) {
			ath12k_warn(ab, "failed to init tcl_data ring (%d) :%d\n",
				    i, ret);
			return ret;
		}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_hal_tx_config_rbm_mapping(ab, i, rbm_id, HAL_TCL_DATA);
#endif

		ret = ath12k_dp_srng_init(ab, &dp->tx_ring[i].tcl_comp_ring,
					  HAL_TX_COMPLETION, tx_comp_ring_num, 0);
		if (ret) {
			ath12k_warn(ab, "failed to init tx_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			return ret;
		}
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tx_exception, HAL_TX_EXCEPTION, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm2sw_release ring :%d\n", ret);
		return ret;
	}

	/*
	 * Allocate dedicated TQM2SW ring for FW-owned buffer completions.
	 * When a TX exception buffer has cookie RBM != 0 (FW-owned), the host
	 * sends a SW2TQM sync command with HAL_TQM_HOST_STATUS_RING=1. C-TQM
	 * routes the completion to this ring instead of the normal tqm_status_ring.
	 * This is a V1-only hardware WAR; skip on V2 and later silicon.
	 */
	if (ab->hw_params->tqm2sw_fw_war) {
		ret = ath12k_dp_srng_setup(ab, &dp_wifi8->tqm2sw_fw_ring,
					   HAL_TQM2SW_FW_COMPLETION,
					   0, 0,
					   DP_FW_TQM2SW_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up fw_tqm2sw ring: %d\n", ret);
			return ret;
		}
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tcl_status_ring, HAL_TCL_STATUS, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init tcl_status ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tcl_cmd_ring, HAL_TCL_CMD, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init tcl_cmd ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tqm_cmd_ring, HAL_TQM_CMD, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init tqm command ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tqm_status_ring, HAL_TQM_STATUS, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init tqm_status ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->rx_ase_cmd_ring, HAL_ASE_CMD_RING, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init ase_cmd ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->sam_cmd_ring, HAL_SAM_CMD, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init sam cmd ring: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->sam_status_ring, HAL_SAM_STATUS, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init sam status ring: %d\n", ret);
		return ret;
	}

	/* Send clear command to reset SAM related structures.*/
	ath12k_wifi8_hal_tx_sam_program_clear(ab);

	ath12k_wifi8_hal_tx_configure_cmn_reg(ab);
	return 0;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_ppeds_tx_update_stats(struct ath12k *ar, int skb_len,
				  struct hal_tqm2sw_completion_ring *tx_status)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct hal_tx_status ts = { 0 };
	bool tx_drop = false;
	bool tx_status_default = false;
	u8 reason;

	dp = ath12k_ab_to_dp(ab);
	ath12k_wifi8_dp_tx_status_parse(ab, tx_status, &ts);
	dp->ppe.ppeds_stats.tqm_rel_reason[ts.status]++;

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts.status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_DS_TQM_REMOVE_MPDU;
			break;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_DS_TQM_DROP_THRESHOLD;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_DS_TQM_REMOVE_TX;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_DS_TQM_REMOVE_AGED;
			break;
		default:
			reason = ATH_TX_DS_TQM_REMOVE_DEF;
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts.status);
			tx_status_default = true;
		}
		tx_drop = true;
	}

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, ts.peer_id);
	if (unlikely(!peer || !ath12k_dp_link_peer_get_sta(peer) ||
		     !ath12k_dp_link_peer_get_vif(peer))) {
		rcu_read_unlock();
		return;
	}

	if (ts.status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
		WRITE_ONCE(peer->peer_stats.last_ack, jiffies);

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		ahvif = ath12k_vif_to_ahvif(ath12k_dp_link_peer_get_vif(peer));
		if (tx_drop) {
			ath12k_tid_tx_drop_stats(ahvif, ts.tid, skb_len,
						 reason);
		} else {
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_PPEDS_PKTS);
			ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
					    ATH_TX_COMPLETED_PKTS);
		}
	}

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !tx_status_default) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();
}

int ath12k_wifi8_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k *ar;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_ppeds_tx_comp_ring *tx_ring = &dp->ppe.ppeds_comp_ring;
	int hal_ring_id = tx_ring->ppeds_txcmpl_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_ppeds_tx_desc_info *tx_desc = NULL;
	int valid_entries, count = 0;
	int list_no_skb_count = 0;
	struct htt_tx_completion *status_desc;
	int htt_status;
	struct list_head local_list;
	struct list_head local_list_no_skb;
	size_t stat_size;
	struct hal_tqm2sw_completion_ring *desc;
	struct hal_tqm2sw_completion_ring *tx_status;
	struct ath12k_dp_tx_comp_status tx_comp_status;

	if (WARN_ON_ONCE(budget > DP_PPEDS_SERVICE_BUDGET))
		return count;

	if (likely(ab->stats_disable))
		/* only need buf_addr_info and info0 */
		stat_size = 3 * sizeof(u32);
	else
		stat_size = sizeof(struct hal_tqm2sw_completion_ring);
	INIT_LIST_HEAD(&local_list);
	INIT_LIST_HEAD(&local_list_no_skb);

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return count;
	}

	if (valid_entries >= budget)
		valid_entries = budget;

	ath12k_hal_srng_ppeds_dst_inv_entry(ab, status_ring, valid_entries);

	while (likely(valid_entries--)) {
		desc = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring, NULL);
		if (!desc)
			continue;

		ath12k_wifi8_hal_tx_completion_process(desc, &tx_comp_status);
		tx_status = (struct hal_tqm2sw_completion_ring *)desc;
		if (likely(!ab->stats_disable))
			memcpy(((void *)tx_ring->tx_status) +
			       (count * status_ring->entry_size),
			       desc, stat_size);

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS W0:%u W1:%u W2:%u rate:%u peer_id:%d W3:%u W4:%u",
				tx_status->info0, tx_status->info1,
				tx_status->info2, tx_status->rate_stats.info0,
				tx_status->sw_peer_id, tx_status->info3,
				tx_status->info4);

		/* HW done cookie conversion */
		tx_desc = (struct ath12k_ppeds_tx_desc_info *)
				((unsigned long)tx_comp_status.tx_desc);
		if (unlikely(!tx_desc)) {
			ath12k_warn(ab, "unable to retrieve ppe ds tx_desc!");
			continue;
		}

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS completion txdesc:%p\n", tx_desc);

		tx_ring->macid[count] = tx_desc->mac_id;

		if (unlikely(tx_comp_status.buf_rel_source ==
			HAL_WBM_REL_SRC_MODULE_FW)) {
			status_desc = (void *)tx_status;
			htt_status = le32_get_bits(status_desc->info0,
					HAL_TX_COMP_TQM_RELEASE_REASON_MASK);

			if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ)
				ath12k_ppeds_reinject_handler(ab,
						tx_desc,
						(struct htt_tx_completion *)status_desc);

			if (htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_OK &&
				htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ) {
				ab->dp->ppe.ppeds_stats.fw2wbm_pkt_drops++;
				ath12k_dbg(ab, ATH12K_DBG_PPE,
					"Frame rcvd from unexpected src %d status %d!\n",
					tx_comp_status.buf_rel_source, htt_status);
			}
			tx_ring->macid[count] = 0xF;
		}
		/* add descriptor to local list to process in bulk */
		tx_desc->in_use = false;
		if (likely(tx_desc->skb)) {
			list_add_tail(&tx_desc->list, &local_list);
			if (tx_ring->macid[count] != 0xF) {
				ar = ab->pdevs[tx_ring->macid[count]].ar;
				dp_pdev = &ar->dp;
				if (ath12k_dp_stats_enabled(dp_pdev))
					ath12k_ppeds_tx_update_stats(ar,
							tx_desc->skb->len,
							desc);
			}
			count++;
		} else {
			list_add_tail(&tx_desc->list, &local_list_no_skb);
			list_no_skb_count++;
		}
	}
	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	ath12k_dp_ppeds_tx_release_desc_list_bulk(dp->ab, dp->dp_hw_grp,
						  &local_list, count,
						  &local_list_no_skb,
						  list_no_skb_count);
	return (count + list_no_skb_count);
}
#endif /* CPTCFG_ATH12K_PPE_DS_SUPPORT */

#define INDEX_LOOKUP_OVERRIDE_ENABLED 1
#define HLOS_TID_OVERWRITE_ENABLED 1
#define FLOW_OVERRIDE_ENABLED 1
static int ath12k_wifi8_dp_tx_reinject(struct ath12k_dp *dp,
				       struct ath12k_dp_vif *dp_vif,
				       struct hal_tcl_exit_base *tx_exception_desc,
				       struct ath12k_tx_desc_info *tx_desc)
{
	struct hal_tcl_data_cmd tcl_desc = {0};
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct dp_tx_ring *tx_ring;
	u8 ring_selector = 0, ring_id = 0;
	struct ath12k_hal *hal = dp->hal;
	struct hal_srng *tcl_ring;
	u8 hal_ring_id;
	u32 data_len;

	ring_selector = dp->hw_params->hw_ops->get_ring_selector(tx_desc->skb);
	ring_id = ring_selector % dp->hw_params->max_tx_ring;

	memcpy(&tcl_desc.buf_addr_info, &tx_exception_desc->buf_addr_info,
	       sizeof(struct ath12k_buffer_addr));
	tcl_desc.info0 =
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BUF_OR_EXT_DESC_TYPE,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO0_BUF_OR_EXT_DESC_TYPE,
					     le32_to_cpu(tx_exception_desc->info0))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_BANK_ID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO8_BANK_ID,
					     le32_to_cpu(tx_exception_desc->info8))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_VDEV_ID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO7_VDEV_ID,
					     le32_to_cpu(tx_exception_desc->info7))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO0_DATA_LENGTH,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO6_DATA_LENGTH,
					     le32_to_cpu(tx_exception_desc->info6)));
	if (le16_get_bits(tx_exception_desc->info3,
			  HAL_TCL_EXIT_BASE_INFO3_INDEX_LOOKUP_OVERRIDE)) {
		tcl_desc.search_index = tx_exception_desc->addrx_ast_hash_idx;
		tcl_desc.info1 =
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_CACHE_SET_NUM,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO7_CACHE_SET_NUM,
					     le32_to_cpu(tx_exception_desc->info7))) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_INDEX_LOOKUP_OVERRIDE,
				   INDEX_LOOKUP_OVERRIDE_ENABLED);
	}
	tcl_desc.info2 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TO_FW_SW,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO7_TO_FW_SW,
					      le32_to_cpu(tx_exception_desc->info7))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_IPV4_CHECKSUM_EN,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO6_IPV4_CHECKSUM_EN,
					      le32_to_cpu(tx_exception_desc->info6))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_UDP_OVER_IPV4_CHECKSUM_EN,
				    FIELD_GET(
					HAL_TCL_EXIT_BASE_INFO6_UDP_OVER_IPV4_CHECKSUM_EN,
					le32_to_cpu(tx_exception_desc->info6))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_L4_CHECKSUM_EN,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO6_L4_CHECKSUM_EN,
					      le32_to_cpu(tx_exception_desc->info6)));
	tcl_desc.info3 = FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_FLOW_SELECT,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO3_FLOW_SELECT,
					      le32_to_cpu(tx_exception_desc->info3))) |
			 FIELD_PREP(HAL_TCL_DATA_CMD_INFO3_LINK_ID,
				    FIELD_GET(HAL_TCL_EXIT_BASE_INFO3_CMD_LINK_ID,
					      le32_to_cpu(tx_exception_desc->info3)));
	tcl_desc.tcl_cmd_number = tx_exception_desc->tcl_status_number;

	if (le32_get_bits(tx_exception_desc->info6,
			  HAL_TCL_EXIT_BASE_INFO6_HLOS_TID_OVERWRITE))
		tcl_desc.info1 |=
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_HLOS_TID,
				   FIELD_GET(HAL_TCL_EXIT_BASE_INFO0_TID,
					     le32_to_cpu(tx_exception_desc->info0)) &
				   (ATH12K_MAX_TID_VALUE - 1)) |
			FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_HLOS_TID_OVERWRITE,
				   HLOS_TID_OVERWRITE_ENABLED);
	if (le16_get_bits(tx_exception_desc->info3,
			  HAL_TCL_EXIT_BASE_INFO3_FLOW_OVERRIDE_ENABLE))
		tcl_desc.info2 |=
		FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_FLOW_OVERRIDE_ENABLE,
			   FLOW_OVERRIDE_ENABLED) |
		FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_WHO_CLASSIFY_INFO_SEL,
			   FIELD_GET(HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFOSEL,
				     le32_to_cpu(tx_exception_desc->info7)));

	tx_ring = &dp->tx_ring[ring_id];
	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &hal->srng_list[hal_ring_id];

	ath12k_hal_srng_access_begin_no_lock(tcl_ring);
	hal_tcl_desc = ath12k_hal_srng_src_get_next_entry(dp->ab, tcl_ring);
	if (unlikely(!hal_tcl_desc)) {
		ath12k_hal_srng_access_end_no_lock(dp->ab, tcl_ring);
		dp->device_stats.tx_err.desc_na[ring_id]++;
		return DP_TX_ENQ_DROP_TCL_DESC_NA;
	}

	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));

	data_len = FIELD_GET(HAL_TCL_EXIT_BASE_INFO6_DATA_LENGTH,
			     le32_to_cpu(tx_exception_desc->info6));
	ath12k_hal_srng_access_end_no_lock(dp->ab, tcl_ring);
	return DP_TX_ENQ_SUCCESS;
}

static int ath12k_wifi8_dp_tx_null_flowq_handler(
			struct ath12k_dp *dp,
			struct hal_tcl_exit_base *tx_exception_desc,
			u32 desc_id,
			struct ath12k_tx_desc_info *tx_desc)
{
	enum hal_tcl_encap_type encap_type;
	struct ath12k_dp_tx_queue_metadata tx_q_params = {0};
	struct ath12k_dp_peer *peer;
	struct ath12k_pdev_dp *dp_pdev;
	u8 tidno, flow_type;
	u16 peer_id;
	u8 hw_link_id, link_id;
	bool non_qos, is_tcp, is_udp, mcast;
	u8 bank_id;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_vif *dp_vif;
	int ret;

	tidno = le32_get_bits(tx_exception_desc->info0,
			      HAL_TCL_EXIT_BASE_INFO0_TID);

	if (le16_get_bits(tx_exception_desc->info4,
			  HAL_TCL_EXIT_BASE_INFO4_ADDRX_IDX_INVALID)) {
		ath12k_err(dp->ab, "Addr X index is invalid for desc_id %d",
			   desc_id);
		return -EINVAL;
	}
	peer_id = le16_to_cpu(tx_exception_desc->meta_data_ase);
	hw_link_id = le32_get_bits(tx_exception_desc->info8,
				   HAL_TCL_EXIT_BASE_INFO8_FW_LINK_ID);

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
	if (!dp_pdev) {
		ath12k_err(dp->ab, "link id is invalid for desc_id %d",
			   desc_id);
		ret = -EINVAL;
		goto end;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_err(dp->ab, "peer_id is invalid for desc_id %d",
			   desc_id);
		ret = -EINVAL;
		goto end;
	}
	if (peer->qos_stats_lvl == ATH12K_QOS_SINGLE_LINK_STATS)
		link_id =
		ath12k_dp_peer_convert_hw_to_logical_link_id(peer, dp_pdev->hw_link_id);
	else
		link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(peer,
								       hw_link_id);

	if (link_id < ATH12K_NUM_MAX_LINKS) {
		if (ath12k_dp_peer_get_vif(peer)) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
		} else {
			ath12k_err(dp->ab, "vif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
		if (ahvif) {
			dp_vif = &ahvif->dp_vif;
		} else {
			ath12k_err(dp->ab, "ahvif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
		if (dp_vif) {
			encap_type = dp_vif->tx_encap_type;
		} else {
			ath12k_err(dp->ab, "dp_vif is invalid for link_id %d",
				   link_id);
			ret = -EINVAL;
			goto end;
		}
	} else {
		ath12k_err(dp->ab, "link_id %d exceeding MAX_LINKS",
			   link_id);
		ret = -EINVAL;
		goto end;
	}

	mcast = le16_get_bits(tx_exception_desc->info3,
			      HAL_TCL_EXIT_BASE_INFO3_DA_IS_BCAST_MCAST);
	is_udp = le32_get_bits(tx_exception_desc->info2,
			       HAL_TCL_EXIT_BASE_INFO2_UDP_PROTO);
	is_tcp = le32_get_bits(tx_exception_desc->info2,
			       HAL_TCL_EXIT_BASE_INFO2_TCP_PROTO);
	non_qos = le32_get_bits(tx_exception_desc->info1,
				HAL_TCL_EXIT_BASE_INFO1_NON_QOS);

	if (peer->is_vdev_peer || non_qos) {
		ath12k_err(dp->ab,
			   "Frame is either mcast %d or non_qos %d for desc_id %d",
			   mcast, non_qos, desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}

	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_BANK_ID_EXCEEDED)) {
		ath12k_err(dp->ab, "Bank ID exceeded for desc_id %d",
			   desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	if (le32_get_bits(tx_exception_desc->info5,
			  HAL_TCL_EXIT_BASE_INFO5_BANK_NOT_CONFIGURED)) {
		ath12k_err(dp->ab, "Bank registers not configured for desc_id %d",
			   desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFO_SEL_EXCEEDED)) {
		ath12k_err(dp->ab, "Number of who_classify_info exceeded for desc_id %d",
			   desc_id);
		rcu_read_unlock();
		return -EINVAL;
	}
	bank_id = le32_get_bits(tx_exception_desc->info7,
				HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFOSEL);

	flow_type = bank_id * ATH12K_NUM_MSDU_Q_PER_TID + is_udp;
	tx_q_params.encap_type = encap_type;
	tx_q_params.tidno = tidno;
	tx_q_params.flow_type = flow_type;
	ret = ath12k_peer_alloc_dynamic_queue(dp->dp_hw_grp, peer,
					      &tx_q_params);
end:
	rcu_read_unlock();

	if (ret)
		return ret;

	if (tx_desc)
		return ath12k_wifi8_dp_tx_reinject(dp, dp_vif,
						   tx_exception_desc,
						   tx_desc);
	else
		return 0;
}

static bool
ath12k_wifi8_dp_validate_tx_exception_error(struct ath12k_dp_wifi8 *dp_wifi8,
					    struct hal_tcl_exit_base *tx_exception_desc)
{
	struct ath12k_wifi8_tx_exc_stats *stats = &dp_wifi8->stats.tx_exc_stats;

	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_VDEV_ID_CHECK_EN) &&
	    le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_VDEV_ID_CHECK_FAILURE)) {
		stats->vdev_id_check_fail++;
		return true;
	}
	if (le16_get_bits(tx_exception_desc->info4,
			  HAL_TCL_EXIT_BASE_INFO4_ADDRX_IDX_INVALID)) {
		stats->addrx_invalid++;
		return true;
	}
	if (le16_get_bits(tx_exception_desc->info4,
			  HAL_TCL_EXIT_BASE_INFO4_ADDRX_IDX_TIMEOUT)) {
		stats->addrx_timeout++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info0,
			  HAL_TCL_EXIT_BASE_INFO0_MSDU_DROP)) {
		stats->msdu_drop++;
		return true;
	}

	if (le32_get_bits(tx_exception_desc->info1,
			  HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_FRAME)) {
		stats->illegal_pkts++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info1,
			  HAL_TCL_EXIT_BASE_INFO1_ILLEGAL_ETH_HEADER)) {
		stats->illegal_pkt_hdr++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info5,
			  HAL_TCL_EXIT_BASE_INFO5_PEER_POINTER_NULL_EXCEPTION)) {
		stats->peer_ptr_null++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info5,
			  HAL_TCL_EXIT_BASE_INFO5_BANK_NOT_CONFIGURED)) {
		stats->bank_not_configured++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info6,
			  HAL_TCL_EXIT_BASE_INFO6_MSDU_LENGTH_ERROR)) {
		stats->msdu_len_err++;
		return true;
	}
	/* In case tcl cmd to SW */
	if ((le32_get_bits(tx_exception_desc->info7,
			   HAL_TCL_EXIT_BASE_INFO7_TO_FW_SW)) == 2) {
		stats->to_sw_pkts++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_PARSER_OP_TLV_SEQUENCE_ERR)) {
		stats->parse_err++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_WHO_CLASSIFY_INFO_SEL_EXCEEDED)) {
		stats->classify_info_sel_exceed++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_BANK_ID_EXCEEDED)) {
		stats->bank_id_exceed++;
		return true;
	}
	if (le32_get_bits(tx_exception_desc->info7,
			  HAL_TCL_EXIT_BASE_INFO7_BUFFER_LENGTH_ERROR)) {
		stats->buf_len_err++;
		return true;
	}

	return false;
}

int ath12k_wifi8_dp_tx_exception_handler(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_wifi8_tx_exc_stats *stats = &dp_wifi8->stats.tx_exc_stats;
	struct ath12k_base *ab = dp->ab;
	struct hal_tcl_exit_base *tx_exception_desc = NULL;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	u32 desc_id;
	struct hal_srng *srng;
	int quota = budget;
	dma_addr_t paddr;
	int ret;
	bool tx_exception_error = false;
	u8 rbm;

	srng = &ab->hal.srng_list[dp_wifi8->tx_exception.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	while (budget-- &&
	       (tx_exception_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		stats->tx_exceptions++;
		paddr = ((u64)(le32_get_bits(tx_exception_desc->buf_addr_info.info1,
					     BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(tx_exception_desc->buf_addr_info.info0,
				      BUFFER_ADDR_INFO0_ADDR);
		if (!paddr) {
			ath12k_warn(dp->ab, "invalid  paddr in TX exception path\n");
			print_hex_dump(KERN_ERR, "tx exception desc: ",
				       DUMP_PREFIX_ADDRESS, 16, 1, tx_exception_desc,
				       sizeof(*tx_exception_desc), false);
			continue;
		}

		tx_exception_error =
			ath12k_wifi8_dp_validate_tx_exception_error(dp_wifi8,
								    tx_exception_desc);
		desc_id = le32_get_bits(tx_exception_desc->buf_addr_info.info1,
					BUFFER_ADDR_INFO1_SW_COOKIE);
		tx_desc = ath12k_dp_get_tx_desc(dp, desc_id);

		if (tx_exception_error)
			goto send_tqm2sw_fw_comp;

		if (le32_get_bits(tx_exception_desc->info7,
				  HAL_TCL_EXIT_BASE_INFO7_FLOW_POINTER_NULL)) {
			stats->null_flowq_pkts++;
			ret = ath12k_wifi8_dp_tx_null_flowq_handler(dp,
								    tx_exception_desc,
								    desc_id,
								    tx_desc);
			if (ret == 0) {
				stats->reinject_pkts++;
				continue;
			}
		}

send_tqm2sw_fw_comp:
		rbm = le32_get_bits(tx_exception_desc->buf_addr_info.info1,
				    BUFFER_ADDR_INFO1_RET_BUF_MGR);
		ath12k_wifi8_dp_tx_exception_to_tx_comp(dp, paddr,
							rbm, desc_id);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

int ath12k_wifi8_dp_tx_process_sam_status(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	int quota = budget;
	u16 tag;

	srng = &ab->hal.srng_list[dp_wifi8->sam_status_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget-- && (hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		ath12k_wifi8_hal_tx_sam_status(ab, hdr);
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return quota - budget;
}

/**
 * ath12k_wifi8_dp_tx_calculate_drop2 - Calculate per-flow MSDU drop counts
 *                                      using the weighted-cost algorithm
 *
 * dp:          Data-path context.
 * svc_datas:   Array of HAL_TQM_SERVICE_CATEGORY_MAX sorted-flow descriptors.
 *              Each element lists up to HAL_TQM_MAX_SORTED_FLOW flows for one
 *              service category, ordered by msdu_count descending.
 *              svc_datas[s].weight carries the per-service drop weight.
 * target_drop: Total number of MSDUs to remove.
 * drop:        Output descriptor.  On return, drop->flows[i] identifies a
 *              flow and the number of MSDUs to remove from it.
 *
 * Implements the weighted-cost drop algorithm:
 *  1. Build a flat list of flows across all services;
 *     cost = msdu_count * weight.  Sort by cost descending (insertion sort,
 *     at most HAL_TQM_MAX_SORTED_FLOW_ALL_SVC entries).
 *  2. Find the smallest prefix of the sorted list from which target_drop MSDUs
 *     can be removed.  A floor (limit = next_entry.cost) prevents any single
 *     flow from being drained below the level of its neighbours.
 *  3. Compute excess_num = num_can_drop - target_drop and redistribute it back
 *     to services proportionally by weight (giveback), so the total drop
 *     matches target_drop as closely as possible.
 *  4. Store the net per-flow drop counts in drop.
 *
 * Returns 0 on success, -EINVAL on bad arguments.
 */
void
ath12k_wifi8_dp_tx_calculate_drop2(struct ath12k_wifi8_dp_congestion_control *congstn,
				   struct ath12k_wifi8_svc_sorted_flows *svc_datas,
				   u32 target_drop,
				   struct ath12k_wifi8_svc_remove_flows *drop)
{
	struct ath12k_wifi8_dp_tx_flow_cost *entries = congstn->entries;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		container_of(congstn, struct ath12k_dp_hw_group_wifi8, congstn);
	struct ath12k_dp *dp = dp_hw_grp_wifi8->cumac_dp;
	u32 excess_nums[HAL_TQM_SERVICE_CATEGORY_MAX] = {};
	u32 num_drop_queues_per_svc[HAL_TQM_SERVICE_CATEGORY_MAX] = {};
	enum hal_tqm_service_category svc;
	u32 num_can_drop, excess_num;
	u32 contrib_x1000, limit;
	int n_entries, prefix_end;
	int i, j;

	memset(drop, 0, sizeof(*drop));

	if (!target_drop)
		return;

	/* Step 1 – build flat list: cost = msdu_count * weight */
	n_entries = 0;
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		struct ath12k_wifi8_svc_sorted_flows *sd = &svc_datas[svc];
		int nf = sd->num_flows;

		if (!sd->weight)
			continue;

		for (i = 0; i < nf && n_entries < HAL_TQM_MAX_SORTED_FLOW_ALL_SVC; i++) {
			entries[n_entries].svc    = svc;
			entries[n_entries].idx    = i;
			entries[n_entries].num    = sd->flows[i].msdu_count;
			entries[n_entries].weight = sd->weight;
			entries[n_entries].cost   = sd->flows[i].msdu_count *
						    sd->weight;
			n_entries++;
		}
	}

	/* Sort by cost descending – insertion sort (n_entries <= 64) */
	for (i = 1; i < n_entries; i++) {
		struct ath12k_wifi8_dp_tx_flow_cost tmp = entries[i];

		j = i - 1;
		while (j >= 0 && entries[j].cost < tmp.cost) {
			entries[j + 1] = entries[j];
			j--;
		}
		entries[j + 1] = tmp;
	}

	/* Step 2 – find smallest prefix covering target_drop
	 *
	 * For each candidate prefix of length prefix_end, the floor is set to
	 * the cost of the first excluded entry (limit = entries[prefix_end].cost).
	 * Each entry in the prefix can contribute at most
	 *   delta_num = (entry.cost - limit) / entry.weight  MSDUs.
	 * We stop as soon as the cumulative delta_num >= target_drop.
	 */
	prefix_end   = 1;
	limit        = 0;
	num_can_drop = 0;

	while (prefix_end < n_entries) {
		limit = entries[prefix_end].cost;

		num_can_drop = 0;
		for (i = 0; i < prefix_end; i++) {
			u32 delta_cost = entries[i].cost - limit;
			u32 delta_num  = 0;

			if (delta_cost > 0 && entries[i].weight)
				delta_num = delta_cost / entries[i].weight;

			num_can_drop += delta_num;
		}

		if (num_can_drop >= target_drop)
			break;

		prefix_end++;
	}

	/* When the prefix-based loop cannot accumulate enough drops to meet
	 * target_drop — this covers:
	 *   - n_entries == 1: the while loop never executes (1 < 1 is false)
	 *   - n_entries > 1 but all prefix candidates are exhausted without
	 *     reaching target_drop (total available msdus may still be >=
	 *     target_drop, but the floor limit prevents it)
	 *
	 * Fall back to limit=0 so every entry contributes its full msdu_count
	 * (delta_num = cost/weight = msdu_count).  The excess giveback in
	 * Step 3 will then reduce the total back towards target_drop.
	 */
	if (num_can_drop < target_drop) {
		limit        = 0;
		prefix_end   = n_entries;
		num_can_drop = 0;
		for (i = 0; i < n_entries; i++) {
			if (entries[i].weight)
				num_can_drop += entries[i].num;
		}
	}

	/* Step 3 – compute excess and distribute giveback proportionally
	 *
	 * excess_num = num_can_drop - target_drop (over-drop).
	 * Redistribute excess back to services proportionally:
	 *   contrib = sum(num_drop_queues[s] / weight[s])
	 *   ref_num = excess_num / contrib
	 *   excess_nums[s] = ref_num / weight[s]
	 *
	 * Use a x1000 scaling factor to avoid fractional arithmetic.
	 */
	if (num_can_drop > target_drop)
		excess_num = num_can_drop - target_drop;
	else
		excess_num = 0;

	if (!excess_num)
		goto skip_excess;

	for (i = 0; i < prefix_end; i++)
		num_drop_queues_per_svc[entries[i].svc]++;

	contrib_x1000 = 0;
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		if (num_drop_queues_per_svc[svc] > 0 && svc_datas[svc].weight)
			contrib_x1000 += num_drop_queues_per_svc[svc] * 1000 * 100 /
					 svc_datas[svc].weight;
	}

	if (contrib_x1000) {
		for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
			if (!num_drop_queues_per_svc[svc] || !svc_datas[svc].weight)
				continue;

			excess_nums[svc] = (excess_num * 1000 * 100) /
					   (contrib_x1000 * svc_datas[svc].weight);
		}
	}

skip_excess:
	/* Step 4 – store net drop counts in @drop */
	for (i = 0; i < prefix_end; i++) {
		u8 svc_idx = entries[i].svc;
		u8 flow_idx = entries[i].idx;
		u32 delta_cost = entries[i].cost - limit;
		u32 delta_num = 0;

		if (delta_cost > 0 && entries[i].weight)
			delta_num = delta_cost / entries[i].weight;

		if (delta_num > excess_nums[svc_idx])
			delta_num -= excess_nums[svc_idx];
		else
			delta_num = 0;

		if (!delta_num)
			continue;

		if (drop->num_flows >= HAL_TQM_MAX_SORTED_FLOW_ALL_SVC)
			break;

		drop->flows[drop->num_flows].u.flow_number =
			svc_datas[svc_idx].flows[flow_idx].u.flow_number;
		drop->flows[drop->num_flows].svc = svc_idx;
		drop->flows[drop->num_flows].drop = delta_num;
		drop->flows[drop->num_flows].idx = entries[i].idx;
		drop->num_flows++;
	}

	/* Post-process: integer rounding in the giveback calculation may
	 * cause the sum of per-flow drops to exceed target_drop by a few
	 * MSDUs.  Walk the drop list from the last entry backwards and
	 * shave off the surplus so the total never exceeds target_drop.
	 */
	if (drop->num_flows > 0) {
		u32 total_drops = 0;
		int k;

		for (k = 0; k < drop->num_flows; k++)
			total_drops += drop->flows[k].drop;

		if (total_drops > target_drop) {
			u32 over = total_drops - target_drop;

			for (k = (int)drop->num_flows - 1; k >= 0 && over > 0; k--) {
				u32 reduce = min(drop->flows[k].drop, over);

				drop->flows[k].drop -= reduce;
				over -= reduce;
			}
		}
	}

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
		   "congestion recovery2: target=%u can_drop=%u excess=%u flows=%u\n",
		   target_drop, num_can_drop, excess_num, drop->num_flows);
}

void
ath12k_wifi8_dp_tx_calculate_drop(struct ath12k_wifi8_dp_congestion_control *congstn,
				  struct ath12k_wifi8_svc_sorted_flows *svc_datas,
				  u32 target_drop,
				  struct ath12k_wifi8_svc_remove_flows *drop)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		container_of(congstn, struct ath12k_dp_hw_group_wifi8, congstn);
	struct ath12k_dp *dp = dp_hw_grp_wifi8->cumac_dp;
	enum hal_tqm_service_category svc;
	u32 score[HAL_TQM_SERVICE_CATEGORY_MAX] = {0};
	u32 final_drop[HAL_TQM_SERVICE_CATEGORY_MAX] = {0};
	bool is_locked[HAL_TQM_SERVICE_CATEGORY_MAX] = {false};
	struct ath12k_wifi8_svc_sorted_flows *svc_data;
	struct ath12k_wifi8_flow_entry *entry;
	u32 svc_drop, svc_total, flow_drop;
	u32 total_active_msdu = 0, remaining_drop, flow_grace_count;
	bool violation_found;
	u8 i, idx;

	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		svc_data = &svc_datas[svc];

		total_active_msdu += svc_data->total_msdu_count;
		score[svc] = (svc_data->total_msdu_count * svc_data->weight) / 100;
	}

	if (!total_active_msdu)
		return;

	remaining_drop = target_drop;

	i = 0;
	violation_found = true;

	while (i < HAL_TQM_SERVICE_CATEGORY_MAX && remaining_drop && violation_found) {
		bool current_pass_violators[HAL_TQM_SERVICE_CATEGORY_MAX] = {false};
		u32 share, composite_score = 0;

		i++;
		violation_found = false;

		for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
			if (!is_locked[svc])
				composite_score += score[svc];
		}

		if (!composite_score)
			break;

		for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
			if (is_locked[svc] || !score[svc])
				continue;

			svc_data = &svc_datas[svc];
			share = remaining_drop * score[svc] / composite_score;

			if (share > svc_data->total_msdu_count) {
				share = svc_data->total_msdu_count;
				current_pass_violators[svc] = true;
				violation_found = true;
			}
			final_drop[svc] = share;
		}

		if (violation_found) {
			for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
				if (current_pass_violators[svc]) {
					remaining_drop -= final_drop[svc];
					is_locked[svc] = true;
				}
			}
		} else {
			remaining_drop = 0;
		}
	}

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
		   "congestion recovery: total_msdu=%u drop_needed=%u remaining=%u\n",
		   total_active_msdu, target_drop, remaining_drop);

	svc = HAL_TQM_SERVICE_CATEGORY_MAX;
	drop->num_flows = 0;
	while (svc) {
		svc--;
		svc_data = &svc_datas[svc];

		svc_drop  = final_drop[svc];
		svc_total = svc_data->total_msdu_count;
		if (!svc_drop)
			continue;

		flow_grace_count = congstn->flow_drop_grace_percent * svc_total / 100;

		idx = svc_data->num_flows;
		while (idx) {
			idx--;

			entry = &svc_data->flows[idx];

			/* assumed the flows are in sort order */
			if (entry->msdu_count >= flow_grace_count)
				break;

			/* skip the flows below the grace count */
			svc_total -= entry->msdu_count;
		}

		if (!svc_total)
			continue;

		for (idx = 0; idx < svc_data->num_flows; idx++) {
			if (drop->num_flows >= HAL_TQM_MAX_SORTED_FLOW_ALL_SVC)
				break;

			entry = &svc_data->flows[idx];
			if (entry->msdu_count < flow_grace_count)
				continue;

			flow_drop = (svc_drop * entry->msdu_count) / svc_total;

			i = drop->num_flows;

			drop->flows[i].u.flow_number = entry->u.flow_number;
			drop->flows[i].svc = svc;
			drop->flows[i].drop = flow_drop;
			drop->flows[i].idx = idx;

			drop->num_flows++;
		}
	}
}

static int
ath12k_wifi8_dp_tx_proceed_drop(struct ath12k_wifi8_dp_congestion_control *congstn,
				struct ath12k_wifi8_svc_sorted_flows *svc_datas,
				unsigned long drop_jiffies,
				struct ath12k_wifi8_svc_remove_flows *drop)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		container_of(congstn, struct ath12k_dp_hw_group_wifi8, congstn);
	struct ath12k_dp *dp = dp_hw_grp_wifi8->cumac_dp;
	struct ath12k_wifi8_flow_remove_entry *entry;
	struct ath12k_wifi8_flow_entry *flow;
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *msduq;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_hal_tqm_cmd cmd;
	struct ath12k_base *ab = dp->ab;
	u32 peer_id, tid_num, flow_type, limit;
	struct ath12k_dp_peer *dp_peer;
	enum hal_tlv_tag_be tag;
	u8 pdev_id, idx;
	int ret = 0;

	for (idx = 0; idx < drop->num_flows; idx++) {
		entry = &drop->flows[idx];

		peer_id   = entry->u.flow_info.peer_id;
		tid_num   = ath12k_dp_tx_get_tid(entry->u.flow_info.tid_num);
		flow_type = entry->u.flow_info.flow_type;

		rcu_read_lock();

		dp_peer = NULL;
		for (pdev_id = 0; pdev_id < ab->num_radios; pdev_id++) {
			dp_pdev = rcu_dereference(dp->dp_pdevs[pdev_id]);
			if (!dp_pdev)
				continue;

			dp_peer = ath12k_dp_peer_find_by_peerid_index(dp,
								      dp_pdev,
								      peer_id);
			if (dp_peer)
				break;
		}

		if (!dp_peer) {
			rcu_read_unlock();
			ath12k_info(ab,
				    "peer find failed for flow peer_id %d flow_type %d tid %d\n",
				    peer_id, flow_type, tid_num);
			continue;
		}

		tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
		if (!tx_flow_info) {
			ath12k_info(ab,
				    "invalid tx flow info peer %pM peer_id %d flow_type %d tid %d in proceed drop",
				    dp_peer->addr, peer_id, flow_type, tid_num);
			rcu_read_unlock();
			continue;
		}

		spin_lock_bh(&tx_flow_info->tx_q_lock);

		if (flow_type == HTT_TID_MSDUQ_MCAST) {
			msduq = tx_flow_info->mcast_msduq;
		} else {
			if (tid_num >= ATH12K_MAX_NUM_DATA_TIDS ||
			    flow_type >= ATH12K_MAX_DP_MSDUQ_PER_TID) {
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				rcu_read_unlock();
				continue;
			}

			msduq = tx_flow_info->tid_info[tid_num].msduq[flow_type];
		}

		if (!msduq || msduq->msduq_state != ATH12K_TX_Q_INIT_DONE) {
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			rcu_read_unlock();
			continue;
		}

		/* Track continuous drops: if this flow was dropped in the
		 * previous handler invocation (last_drop_jiffies matches
		 * congstn->last_jiffies), increment the consecutive drop
		 * counter; otherwise reset it to 1.
		 */
		if ((msduq->last_drop_jiffies &&
		     msduq->last_drop_jiffies == congstn->last_drop_jiffies) ||
		    msduq->in_threshold_list)
			msduq->consecutive_drop_count++;
		else
			msduq->consecutive_drop_count = 1;

		msduq->last_drop_jiffies = drop_jiffies;

		memset(&cmd, 0, sizeof(cmd));
		cmd.std.peer_id = (u16)peer_id;

		/* If the flow has been continuously dropped for
		 * continuous_drop_threshold consecutive invocations and is
		 * not already in the threshold list, add it to the
		 * per-service-category threshold list.
		 */
		if (msduq->consecutive_drop_count >= congstn->continuous_drop_threshold) {
			flow = &svc_datas[entry->svc].flows[entry->idx];
			limit = flow->msdu_count - entry->drop;

			entry->limited = 1;
			entry->limit = limit;

			if (!msduq->in_threshold_list &&
			    entry->svc < HAL_TQM_SERVICE_CATEGORY_MAX) {
				spin_lock(&congstn->threshold_list_lock);

				msduq->in_threshold_list = true;
				list_add_tail(&msduq->threshold_node,
					      &congstn->threshold_list[entry->svc]);

				ath12k_dbg(ab, ATH12K_DBG_DP_TX,
					   "flow 0x%x add in threshold limit %u\n",
					   msduq->queue_number, limit);

				spin_unlock(&congstn->threshold_list_lock);
			}

			cmd.update_tx_msdu_params.svc = entry->svc;
			cmd.update_tx_msdu_params.tx_flow_number = msduq->queue_number;
			cmd.update_tx_msdu_params.msdu_q_paddr = msduq->msdu_q_paddr;
			cmd.update_tx_msdu_params.tid = msduq->flow_info.tid_num;
			cmd.update_tx_msdu_params.hard_drop_threshold = limit;
			cmd.update_tx_msdu_params.update_hard_drop_threshold = true;

			tag = HAL_TQM_UPDATE_MSDUQ_BO;
		} else {
			cmd.remove_msdu_params.type = HAL_WIFIREMOVE_HEAD_MSDUS;
			cmd.remove_msdu_params.block_tx_notify_frame_removal = 0;
			cmd.remove_msdu_params.count = (u16)min_t(u32,
								  entry->drop,
								  ATH12K_MAX_MSDU_COUNT);
			cmd.remove_msdu_params.qtype = msduq->flow_info.flow_type;
			cmd.remove_msdu_params.msdu_q_paddr = msduq->msdu_q_paddr;

			tag = HAL_TQM_REMOVE_MSDU_BO;
		}

		ret = ath12k_wifi8_dp_tqm_cmd_send(ab, tag, &cmd, NULL, NULL);
		if (ret)
			ath12k_warn(ab, "tqm %s cmd failed ret %d peer %pM\n",
				    ((tag == HAL_TQM_REMOVE_MSDU_BO) ?
				     "remove" : "update msdu"),
				    ret, dp_peer->addr);

		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		rcu_read_unlock();
	}

	return ret;
}

static void
ath12k_wifi8_dp_tx_restore_flow_limit(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				      unsigned long cur_jiffies)
{
	struct ath12k_wifi8_dp_congestion_control *congstn = &dp_hw_grp_wifi8->congstn;
	struct ath12k_dp *dp = dp_hw_grp_wifi8->cumac_dp;
	struct ath12k_dp_msdu_q_info *msduq, *tmp;
	struct ath12k_base *ab = dp->ab;
	enum hal_tqm_service_category svc;
	LIST_HEAD(restore_list);
	u16 hard_drop_threshold = ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD;
	int ret;

	/* Collect all flows that have not been seen (dropped) for the past
	 * 1 second from all per-service-category threshold lists.
	 */
	spin_lock_bh(&congstn->threshold_list_lock);
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		list_for_each_entry_safe(msduq, tmp,
					 &congstn->threshold_list[svc],
					 threshold_node) {
			if (time_after(cur_jiffies,
				       msduq->last_drop_jiffies + HZ)) {
				list_del(&msduq->threshold_node);
				msduq->in_threshold_list = false;
				msduq->consecutive_drop_count = 0;
				list_add_tail(&msduq->threshold_node,
					      &restore_list);
			}
		}
	}
	spin_unlock_bh(&congstn->threshold_list_lock);

	if (list_empty(&restore_list))
		return;

	/* Send TQM update to reset hard drop threshold to default for each
	 * flow not seen for the past 1 second.
	 */
	list_for_each_entry_safe(msduq, tmp, &restore_list, threshold_node) {
		list_del(&msduq->threshold_node);

		ath12k_dbg(ab, ATH12K_DBG_DP_TX, "flow 0x%x removed threshold\n",
			   msduq->queue_number);

		ret = ath12k_wifi8_dp_tx_update_msdu_flow(dp, msduq->queue_number,
							  msduq->flow_info.tid_num,
							  msduq->svc,
							  hard_drop_threshold);
		if (ret)
			ath12k_warn(ab,
				    "failed to reset hard drop threshold for flow %u: %d\n",
				    msduq->queue_number, ret);
	}
}

static void ath12k_wifi8_dp_tx_get_desc_used_cnt(struct ath12k_dp_hw_group *dp_hw_grp,
						 u32 *count,
						 u32 *ppeds_count)
{
	u32 used_cnt = 0, ppeds_used_cnt = 0;
	u32 *tx_desc_used_cnt;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 *ppeds_tx_desc_used_cnt;
#endif
	int cpu;

	for_each_possible_cpu(cpu) {
		tx_desc_used_cnt = per_cpu_ptr(dp_hw_grp->tx_desc_used_cnt, cpu);
		used_cnt += *tx_desc_used_cnt;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ppeds_tx_desc_used_cnt = per_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt,
						     cpu);
		ppeds_used_cnt += *ppeds_tx_desc_used_cnt;
#endif
	}

	*count = used_cnt;
	*ppeds_count = ppeds_used_cnt;

	return;
}

void ath12k_wifi8_dp_tx_congestion_recovery_handler(struct timer_list *t)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					from_timer(dp_hw_grp_wifi8, t, congstn.timer);
	struct ath12k_wifi8_dp_congestion_control *congstn = &dp_hw_grp_wifi8->congstn;
	struct ath12k_wifi8_svc_sorted_flows *svc_data = congstn->svc_data;
	u8 idx, i, svc_mask = (1 << HAL_TQM_SERVICE_CATEGORY_MAX) - 1;
	struct ath12k_wifi8_svc_remove_flows *drop = congstn->drop;
	u32 target_drop = 0, ppeds_target_drop = 0, total_drop;
	u32 flow_number, high_msdu_count, total_active_msdu;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	struct ppe_ds_wlan_rxfill_ring_info info = { };
#endif
	struct ath12k_dp *dp = dp_hw_grp_wifi8->cumac_dp;
	struct ath12k_wifi8_svc_sorted_flows *svc_flows;
	struct ath12k_wifi8_flow_entry *entry;
	enum hal_tqm_service_category svc;
	struct ath12k_base *ab = dp->ab;
	u32 used_cnt, ppeds_used_cnt;
	unsigned long cur_jiffies;
	int ret;

	if (!congstn->init)
		return;

	cur_jiffies = jiffies;

	/* TQM sorted-flow registers are unreliable while this chip is mid
	 * SSR (FW held in reset / PCIe or AHB path down). Reading them here
	 * returns garbage that decodes to bogus peer_ids, causing an
	 * unthrottled log flood on every 100ms tick that can starve the
	 * console/RCU long enough to trip a stall panic. Skip this tick
	 * entirely and just keep the timer alive so it resumes normal
	 * operation as soon as recovery completes.
	 */
	if (test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
		goto out;

	ath12k_wifi8_dp_tx_get_desc_used_cnt(dp_hw_grp_wifi8->dp_hw_grp,
					     &used_cnt,
					     &ppeds_used_cnt);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_ppeds_get_rxfill_ring_info_v2(dp_hw_grp_wifi8->cumac_dp->ppe.ds_node_id,
					     &info);
	ppeds_used_cnt -= (info.wifi8.prim_active_cnt + info.wifi8.secd_active_cnt);
#endif
	if (used_cnt <= congstn->used_threshold &&
	    ppeds_used_cnt <= congstn->ppeds_used_threshold)
		goto skip_history;

	if (!svc_data)
		goto skip_history;

	if (!drop)
		goto skip_history;

	memset(drop, 0, sizeof(*drop));
	memset(svc_data, 0, HAL_TQM_SERVICE_CATEGORY_MAX * sizeof(*svc_data));

	ath12k_wifi8_hal_tqm_sorting_latch(&ab->hal);

	total_active_msdu = 0;
	while (svc_mask) {
		svc = fls(svc_mask) - 1;
		svc_mask ^= 1 << svc;

		svc_flows = &svc_data[svc];
		svc_flows->weight = congstn->weights[svc];

		for (idx = 0; idx < HAL_TQM_MAX_SORTED_FLOW; idx++) {
			ret = ath12k_wifi8_hal_tqm_get_svc_sorted_list(&ab->hal,
								       svc,
								       idx,
								       &flow_number,
								       &high_msdu_count);
			if (ret || !high_msdu_count)
				continue;

			entry = &svc_flows->flows[svc_flows->num_flows];

			entry->u.flow_number = flow_number;
			entry->svc = svc;
			entry->msdu_count = high_msdu_count;
			total_active_msdu += high_msdu_count;

			svc_flows->num_flows++;
			svc_flows->total_msdu_count += high_msdu_count;
		}
	}

	if (!total_active_msdu)
		goto skip_drop;

	if (used_cnt > congstn->used_threshold)
		target_drop = used_cnt - congstn->used_threshold;

	if (ppeds_used_cnt > congstn->ppeds_used_threshold)
		ppeds_target_drop = ppeds_used_cnt - congstn->ppeds_used_threshold;

	total_drop = target_drop + ppeds_target_drop;
	if (total_drop < total_active_msdu) {
		congstn->calculate_drop(congstn, svc_data,
					total_drop,
					drop);
	} else {
		drop->num_flows = 0;
		for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
			svc_flows = &svc_data[svc];
			if (!svc_flows->weight)
				continue;

			for (idx = 0; idx < svc_flows->num_flows; idx++) {
				if (drop->num_flows >= ARRAY_SIZE(drop->flows))
					break;

				entry = &svc_flows->flows[idx];
				i = drop->num_flows;

				drop->flows[i].u.flow_number = entry->u.flow_number;
				drop->flows[i].svc = entry->svc;
				drop->flows[i].drop = entry->msdu_count;
				drop->flows[i].idx = idx;

				drop->num_flows++;
			}
		}
	}

	if (!drop->num_flows)
		goto skip_drop;

	ret = ath12k_wifi8_dp_tx_proceed_drop(congstn, svc_data, cur_jiffies, drop);
	if (ret)
		ath12k_warn(ab, "drop msdu failed %d used_cnt %u ppeds_used_cnt %u\n",
			    ret, used_cnt, ppeds_used_cnt);

skip_drop:
	/* Store this congestion recovery event in the circular history buffer */
	if (congstn->history && congstn->history_enable) {
		struct ath12k_wifi8_congstn_history_entry *hist;
		enum hal_tqm_service_category hsvc;

		spin_lock(&congstn->history_lock);
		hist = &congstn->history[congstn->history_head];

		hist->timestamp = cur_jiffies;
		hist->total_active_msdu = total_active_msdu;
		hist->used_threshold = congstn->used_threshold;
		hist->ppeds_used_threshold = congstn->ppeds_used_threshold;
		hist->target_drop = total_drop;
		hist->used_cnt = used_cnt;
		hist->ppeds_used_cnt = ppeds_used_cnt;
		hist->num_drop_flows = drop->num_flows;

		for (hsvc = 0; hsvc < HAL_TQM_SERVICE_CATEGORY_MAX; hsvc++) {
			hist->svc_num_flows[hsvc] = svc_data[hsvc].num_flows;
			hist->svc_total_msdu[hsvc] = svc_data[hsvc].total_msdu_count;
		}

		memcpy(hist->drop_flows, drop->flows,
		       drop->num_flows * sizeof(*drop->flows));

		congstn->history_head = (congstn->history_head + 1) %
					congstn->history_size;
		congstn->history_count++;
		spin_unlock(&congstn->history_lock);
	}

	congstn->last_drop_jiffies = cur_jiffies;

skip_history:
	if (used_cnt + ppeds_used_cnt > congstn->max_used)
		congstn->max_used = used_cnt + ppeds_used_cnt;

	congstn->tick_counter++;
	if (congstn->tick_counter >= congstn->scaling_factor) {
		congstn->tick_counter = 0;
		ath12k_wifi8_dp_tx_restore_flow_limit(dp_hw_grp_wifi8, cur_jiffies);
	}

out:
	congstn->last_jiffies = cur_jiffies;
	if (congstn->start)
		mod_timer(&dp_hw_grp_wifi8->congstn.timer,
			  jiffies + msecs_to_jiffies(dp_hw_grp_wifi8->congstn.interval));
}

int ath12k_wifi8_dp_tx_congestion_control_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	struct ath12k_wifi8_dp_congestion_control *congstn =
			&dp_hw_grp_wifi8->congstn;
	enum hal_tqm_service_category svc;

	if (!ath12k_congestion_ctrl)
		return 0;

	if (congstn->init)
		return 0;

	if (!ath12k_drop_algo)
		congstn->calculate_drop = ath12k_wifi8_dp_tx_calculate_drop;
	else
		congstn->calculate_drop = ath12k_wifi8_dp_tx_calculate_drop2;

	congstn->weights[HAL_TQM_SERVICE_CATEGORY_SC0] = 0;
	congstn->weights[HAL_TQM_SERVICE_CATEGORY_SC1] = 10;
	congstn->weights[HAL_TQM_SERVICE_CATEGORY_SC2] = 40;
	congstn->weights[HAL_TQM_SERVICE_CATEGORY_SC3] = 50;

	congstn->used_threshold =
			ATH12K_DP_TX_GET_USED_THRSHLD(DP_TX_DESC_COUNT_POOL0,
						      ATH12K_HW_MAX_ACTIVE_QUEUES);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	congstn->ppeds_used_threshold =
		ATH12K_DP_TX_GET_USED_THRSHLD(ATH12K_NUM_POOL_PPEDS_TX_DESC_DEFAULT,
					      1);
#endif
	congstn->flow_drop_grace_percent = ATH12K_DP_TX_SORT_FLOW_DROP_GRACE;

	/* Enable sorting for flows in service category.*/
	ath12k_wifi8_hal_enable_service_category_sorting(&dp->ab->hal);

	timer_setup(&congstn->timer,
		    ath12k_wifi8_dp_tx_congestion_recovery_handler, 0);

	congstn->interval = ATH12K_DP_TX_CONGESTION_CTRL_INTERVAL_MS;
	congstn->scaling_factor = ATH12K_DP_TX_CONGESTION_CTRL_2SEC_MS /
				  congstn->interval;

	/* Initialize per-service-category threshold lists */
	congstn->continuous_drop_threshold = ATH12K_DP_TX_FLOW_CONTINUOUS_DROP_THRESHOLD;
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++)
		INIT_LIST_HEAD(&congstn->threshold_list[svc]);

	spin_lock_init(&congstn->threshold_list_lock);

	congstn->svc_data = kcalloc(HAL_TQM_SERVICE_CATEGORY_MAX,
				    sizeof(*congstn->svc_data), GFP_KERNEL);
	if (!congstn->svc_data)
		goto err;

	congstn->drop = kzalloc(sizeof(*congstn->drop), GFP_KERNEL);
	if (!congstn->drop)
		goto err_svc_data;

	congstn->entries = kcalloc(HAL_TQM_MAX_SORTED_FLOW_ALL_SVC,
				   sizeof(*congstn->entries),
				   GFP_KERNEL);
	if (!congstn->entries)
		goto err_drop;

	/* Allocate and initialize history circular buffer */
	congstn->history_size = (ATH12K_DP_TX_CONGSTN_HISTORY_DURATION_SEC * 1000)
				/ congstn->interval;
	congstn->history = vcalloc(congstn->history_size, sizeof(*congstn->history));
	if (!congstn->history)
		goto err_entries;

	congstn->history_head = 0;
	congstn->history_count = 0;
	spin_lock_init(&congstn->history_lock);

	/* History logging is disabled by default; enable via debugfs */
	congstn->history_enable = false;
	congstn->init = true;

	congstn->start = true;
	mod_timer(&congstn->timer, jiffies + msecs_to_jiffies(congstn->interval));

	return 0;

err_entries:
	kfree(congstn->entries);
	congstn->entries = NULL;

err_drop:
	kfree(congstn->drop);
	congstn->drop = NULL;

err_svc_data:
	kfree(congstn->svc_data);
	congstn->svc_data = NULL;

err:
	return -ENOMEM;
}

void ath12k_wifi8_dp_tx_congestion_control_deinit(struct ath12k_dp *dp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	struct ath12k_wifi8_dp_congestion_control *congstn =
			&dp_hw_grp_wifi8->congstn;
	struct ath12k_dp_msdu_q_info *msduq, *tmp;
	enum hal_tqm_service_category svc;

	if (!ath12k_congestion_ctrl)
		return;

	if (!congstn->init)
		return;

	congstn->start = false;
	del_timer_sync(&congstn->timer);

	/* Drain all per-service-category threshold lists */
	spin_lock_bh(&congstn->threshold_list_lock);
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		list_for_each_entry_safe(msduq, tmp,
					 &congstn->threshold_list[svc],
					 threshold_node) {
			list_del(&msduq->threshold_node);
			msduq->in_threshold_list = false;
		}
	}
	spin_unlock_bh(&congstn->threshold_list_lock);

	/* Free dynamically allocated buffers */
	kfree(congstn->svc_data);
	congstn->svc_data = NULL;

	kfree(congstn->drop);
	congstn->drop = NULL;

	kfree(congstn->entries);
	congstn->entries = NULL;

	vfree(congstn->history);
	congstn->history = NULL;

	congstn->init = false;
}

ssize_t ath12k_wifi8_dp_tx_dump_svc_sorted_list(struct ath12k_dp *dp, u8 ac_mask,
						char *buf, int size)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	enum hal_tqm_service_category svc;
	u32 flow_number, high_msdu_count;
	int len = 0, ret;
	u8 idx;

	dp = ath12k_get_central_dp(dp);
	if (!dp) {
		len += scnprintf(buf + len, size - len, "DP unavailable\n");
		goto out;
	}

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8->congstn.init) {
		len += scnprintf(buf + len, size - len, "Not inited\n");
		goto out;
	}

	len += scnprintf(buf + len, size - len,
			 "SVC\tidx\tFlow number\tmsdu_count\n");

	ath12k_wifi8_hal_tqm_sorting_latch(&dp->ab->hal);

	while (ac_mask) {
		svc = fls(ac_mask) - 1;
		ac_mask ^= 1 << svc;
		for (idx = 0; idx < HAL_TQM_MAX_SORTED_FLOW; idx++) {
			ret = ath12k_wifi8_hal_tqm_get_svc_sorted_list(&dp->ab->hal,
								       svc,
								       idx,
								       &flow_number,
								       &high_msdu_count);
			if (ret) {
				flow_number = -1;
				high_msdu_count = -1;
			}

			len += scnprintf(buf + len, size - len,
					 "SC%d\t%d\t0x%x\t\t%d\n",
					 svc, idx, flow_number, high_msdu_count);
		}
	}

out:
	return len;
}

ssize_t ath12k_wifi8_dp_tx_dump_congestion_ctrl_stats(struct ath12k_dp *dp,
						      char *buf, int size)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_wifi8_dp_congestion_control *congstn;
	struct ath12k_dp_msdu_q_info *msduq;
	enum hal_tqm_service_category svc;
	struct ath12k_hal *hal;
	u32 threshold_count, used_cnt, ppeds_used_cnt;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	struct ppe_ds_wlan_rxfill_ring_info info = { };
#endif
	int len = 0;

	dp = ath12k_get_central_dp(dp);
	if (!dp)
		return 0;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return 0;

	congstn = &dp_hw_grp_wifi8->congstn;
	if (!congstn->init) {
		len += scnprintf(buf + len, size - len, "Not inited\n");
		goto out;
	}

	hal = &dp->ab->hal;

	ath12k_wifi8_dp_tx_get_desc_used_cnt(dp->dp_hw_grp,
					     &used_cnt,
					     &ppeds_used_cnt);

	len += scnprintf(buf + len, size - len,
			 "Congestion Control Statistics:\n");
	len += scnprintf(buf + len, size - len,
			 "algorithm:                 %u\n",
			 ath12k_drop_algo);
	len += scnprintf(buf + len, size - len,
			 "init:                      %s\n",
			 congstn->init ? "true" : "false");
	len += scnprintf(buf + len, size - len,
			 "start:                     %s\n",
			 congstn->start ? "true" : "false");
	len += scnprintf(buf + len, size - len,
			 "interval:                  %u ms\n",
			 congstn->interval);
	len += scnprintf(buf + len, size - len,
			 "tx_desc_used_cnt:          %u\n",
			 used_cnt);
	len += scnprintf(buf + len, size - len,
			 "ppeds_tx_desc_used_cnt:    %u\n",
			 ppeds_used_cnt);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_ppeds_get_rxfill_ring_info_v2(dp->ppe.ds_node_id,
					     &info);
	len += scnprintf(buf + len, size - len,
			 "ppeds_refill_cnt:          %u\n",
			 info.wifi8.prim_active_cnt + info.wifi8.secd_active_cnt);
#endif
	len += scnprintf(buf + len, size - len,
			 "used_threshold:            %u\n",
			 congstn->used_threshold);
	len += scnprintf(buf + len, size - len,
			 "ppeds_used_threshold:      %u\n",
			 congstn->ppeds_used_threshold);
	len += scnprintf(buf + len, size - len,
			 "max_used:                  %u\n",
			 congstn->max_used);

	svc = HAL_TQM_SERVICE_CATEGORY_MAX;
	len += scnprintf(buf + len, size - len,
			 "\nTotal active_msdu:       %u\n",
			 ath12k_wifi8_hal_tqm_get_active_msdu(hal, svc));

	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++)
		len += scnprintf(buf + len, size - len,
				 "SC%d active_msdu: %u\n",
				 svc,
				 ath12k_wifi8_hal_tqm_get_active_msdu(hal, svc));

	len += scnprintf(buf + len, size - len,
			 "\nflow_drop_grace_percent:   %u%%\n",
			 congstn->flow_drop_grace_percent);
	len += scnprintf(buf + len, size - len,
			 "continuous_drop_threshold: %u\n",
			 congstn->continuous_drop_threshold);
	len += scnprintf(buf + len, size - len,
			 "scaling_factor:            %u\n",
			 congstn->scaling_factor);
	len += scnprintf(buf + len, size - len,
			 "tick_counter:              %u\n",
			 congstn->tick_counter);
	len += scnprintf(buf + len, size - len,
			 "last_drop_jiffies:         %lu\n",
			 congstn->last_drop_jiffies);
	len += scnprintf(buf + len, size - len,
			 "last_jiffies:              %lu\n",
			 congstn->last_jiffies);
	len += scnprintf(buf + len, size - len,
			 "history_enable:            %s (param type %u: 0=disable, 1=enable)\n",
			 congstn->history_enable ? "true" : "false",
			 ATH12K_CONGSTN_CTRL_HISTORY_ENABLE);

	len += scnprintf(buf + len, size - len,
			 "\nPer-Service-Category Weights:\n");
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++)
		len += scnprintf(buf + len, size - len,
				 "SC%d weight: %u\n",
				 svc, congstn->weights[svc]);

	len += scnprintf(buf + len, size - len,
			 "\nThreshold List (continuously dropped flows):\n");
	len += scnprintf(buf + len, size - len,
			 "SVC\tpeer_id\ttid\tflow_type\tconsec_drops\tlast_drop_jiffies\n");

	spin_lock_bh(&congstn->threshold_list_lock);
	for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
		threshold_count = 0;
		list_for_each_entry(msduq, &congstn->threshold_list[svc],
				    threshold_node) {
			len += scnprintf(buf + len, size - len,
					 "SC%d\t%u\t%u\t%u\t\t%u\t\t%lu\n",
					 svc,
					 msduq->flow_info.peer_id,
					 msduq->flow_info.tid_num,
					 msduq->flow_info.flow_type,
					 msduq->consecutive_drop_count,
					 msduq->last_drop_jiffies);
			threshold_count++;
			if (len >= size - 128)
				break;
		}
		if (!threshold_count)
			len += scnprintf(buf + len, size - len,
					 "SC%d: (empty)\n", svc);
	}
	spin_unlock_bh(&congstn->threshold_list_lock);

out:
	return len;
}

/**
 * ath12k_wifi8_dp_tx_dump_congestion_recovery_hist() - Dump the circular
 *   history buffer of congestion recovery events to a user-supplied buffer.
 *
 * @dp:   DP context (will be resolved to the central DP internally).
 * @buf:  Output character buffer.
 * @size: Size of @buf in bytes.
 *
 * Each entry in the history corresponds to one invocation of
 * ath12k_wifi8_dp_tx_congestion_recovery_handler() that resulted in a drop
 * action.  The entries are printed in reverse chronological order (latest first).
 *
 * Returns the number of bytes written to @buf.
 */
ssize_t ath12k_wifi8_dp_tx_dump_congestion_recovery_hist(struct ath12k_dp *dp,
							 char *buf, int size)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_wifi8_dp_congestion_control *congstn;
	struct ath12k_wifi8_congstn_history_entry *hist;
	const struct ath12k_wifi8_flow_remove_entry *f;
	enum hal_tqm_service_category svc;
	u32 total, i, seq;
	u8 fi;
	int len = 0;

	dp = ath12k_get_central_dp(dp);
	if (!dp)
		return 0;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return 0;

	congstn = &dp_hw_grp_wifi8->congstn;
	if (!congstn->init) {
		len += scnprintf(buf + len, size - len, "Not inited\n");
		goto out;
	}

	spin_lock_bh(&congstn->history_lock);

	if (!congstn->history) {
		len += scnprintf(buf + len, size - len,
				 "Congestion Recovery History: (buffer not allocated)\n");
		goto unlock;
	}

	total = min_t(u32, congstn->history_count,
		      congstn->history_size);

	len += scnprintf(buf + len, size - len,
			 "Congestion Recovery History (%u entries, %u total events, %u Max used):\n",
			 total, congstn->history_count, congstn->max_used);

	if (!total) {
		len += scnprintf(buf + len, size - len, "  (no events recorded)\n");
		goto unlock;
	}

	len += scnprintf(buf + len, size - len,
			 "  seq  timestamp    used_cnt  ppeds_used_cnt  threshold  ppeds_threshold  total_msdu  target_drop  num_flows\n");

	/* history_head points to the next write slot.
	 * The last written (latest) entry is at (history_head - 1).
	 * Iterate backwards from there to print latest first.
	 */
	for (i = 0; i < total; i++) {
		u32 idx = (congstn->history_head - 1 - i + congstn->history_size) %
			  congstn->history_size;

		hist = &congstn->history[idx];
		seq = congstn->history_count - i;

		len += scnprintf(buf + len, size - len,
				 "  %-4u %-12lu %-9u %-15u %-10u %-16u %-11u %-12u %u\n",
				 seq,
				 hist->timestamp,
				 hist->used_cnt,
				 hist->ppeds_used_cnt,
				 hist->used_threshold,
				 hist->ppeds_used_threshold,
				 hist->total_active_msdu,
				 hist->target_drop,
				 hist->num_drop_flows);

		/* Per-service-category breakdown */
		for (svc = 0; svc < HAL_TQM_SERVICE_CATEGORY_MAX; svc++) {
			if (!hist->svc_num_flows[svc] && !hist->svc_total_msdu[svc])
				continue;
			len += scnprintf(buf + len, size - len,
					 "       SC%d: flows=%u msdu=%u\n",
					 svc,
					 hist->svc_num_flows[svc],
					 hist->svc_total_msdu[svc]);
		}

		/* Per-flow drop details */
		if (hist->num_drop_flows > 0) {
			len += scnprintf(buf + len, size - len,
					 "       drops: flow_num   SC  peer  tid  flow_type  count  limit\n");
			for (fi = 0; fi < hist->num_drop_flows; fi++) {
				f = &hist->drop_flows[fi];
				len += scnprintf(buf + len, size - len,
						 "              0x%06x  SC%d  %-5u %-4u %-10u %-7u %-6u\n",
						 f->u.flow_number,
						 f->svc,
						 f->u.flow_info.peer_id,
						 f->u.flow_info.tid_num,
						 f->u.flow_info.flow_type,
						 f->drop,
						 f->limit);
				if (len >= size - 256)
					break;
			}
		}

		if (len >= size - 256)
			break;
	}

unlock:
	spin_unlock_bh(&congstn->history_lock);
out:
	return len;
}

int ath12k_wifi8_dp_tx_set_congestion_ctrl_param(struct ath12k_dp *dp,
						 u32 type, u32 value)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_wifi8_dp_congestion_control *congstn;

	dp = ath12k_get_central_dp(dp);
	if (!dp)
		return -EINVAL;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return -EINVAL;

	congstn = &dp_hw_grp_wifi8->congstn;
	if (!congstn->init) {
		ath12k_warn(dp->ab,
			    "congestion ctrl not inited\n");
		return -EOPNOTSUPP;
	}

	switch (type) {
	case ATH12K_CONGSTN_CTRL_USED_THRESHOLD:
		congstn->used_threshold = value;
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "congestion ctrl: used_threshold set to %u\n", value);
		break;
	case ATH12K_CONGSTN_CTRL_FLOW_DROP_GRACE_PCT:
		if (value > 100)
			return -EINVAL;
		congstn->flow_drop_grace_percent = (u8)value;
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "congestion ctrl: flow_drop_grace_percent set to %u\n", value);
		break;
	case ATH12K_CONGSTN_CTRL_HISTORY_ENABLE:
		if (value != 0 && value != 1)
			return -EINVAL;
		spin_lock_bh(&congstn->history_lock);
		congstn->history_enable = (value != 0);
		spin_unlock_bh(&congstn->history_lock);
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "congestion ctrl: history_enable set to %u\n", value);
		break;
	case ATH12K_CONGSTN_CTRL_RESET_USED:
		congstn->max_used = 0;
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "congestion ctrl: Reset max used\n");
		break;
	case ATH12K_CONGSTN_CTRL_PPEDS_USED_THRESHOLD:
		congstn->ppeds_used_threshold = value;
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "congestion ctrl: ppeds_used_threshold set to %u\n", value);
		break;
	default:
		ath12k_warn(dp->ab,
			    "congestion ctrl: unknown param type %u\n", type);
		return -EINVAL;
	}

	return 0;
}

int ath12k_wifi8_dp_tx_update_msdu_flow(struct ath12k_dp *dp,
					u32 flow_number,
					u8 tid,
					u8 service_category,
					u16 hard_drop_threshold)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_hal_tqm_cmd cmd;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	union {
		u32 flow_number;
		struct ath12k_flow_metadata flow_info;
	} flow;
	u8 pdev_id, flow_type;
	int ret = 0;

	flow.flow_number = flow_number;
	flow_type = flow.flow_info.flow_type;

	rcu_read_lock();
	for (pdev_id = 0; pdev_id < dp->ab->num_radios; pdev_id++) {
		dp_pdev = rcu_dereference(dp->dp_pdevs[pdev_id]);
		if (!dp_pdev)
			continue;

		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp,
							      dp_pdev,
							      flow.flow_info.peer_id);
		if (dp_peer)
			break;
	}

	if (!dp_peer) {
		ret = -ENOENT;
		goto out;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ret = -ENOENT;
		goto out;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	if (flow_type == HTT_TID_MSDUQ_MCAST) {
		sw_msduq_ptr = tx_flow_info->mcast_msduq;
	} else {
		if (tid >= ATH12K_MAX_NUM_DATA_TIDS ||
		    flow_type >= ATH12K_MAX_DP_MSDUQ_PER_TID)
			goto unlock;

		sw_msduq_ptr = tx_flow_info->tid_info[tid].msduq[flow_type];
	}

	if (!sw_msduq_ptr || sw_msduq_ptr->msduq_state != ATH12K_TX_Q_INIT_DONE)
		goto unlock;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = (u16)dp_peer->peer_id;

	cmd.update_tx_msdu_params.svc = (enum hal_tqm_service_category)service_category;
	cmd.update_tx_msdu_params.tx_flow_number = sw_msduq_ptr->queue_number;
	cmd.update_tx_msdu_params.msdu_q_paddr = sw_msduq_ptr->msdu_q_paddr;
	cmd.update_tx_msdu_params.tid = tid;
	cmd.update_tx_msdu_params.hard_drop_threshold = hard_drop_threshold;

	if (hard_drop_threshold)
		cmd.update_tx_msdu_params.update_hard_drop_threshold = true;

	ret = ath12k_wifi8_dp_tqm_cmd_send(dp->ab, HAL_TQM_UPDATE_MSDUQ_BO, &cmd,
					   NULL, NULL);
	if (ret) {
		ath12k_err(dp->ab, "TQM UPDATE MSDUQ send failed %pM id=%d tid=%d svc=%d q=%d\n",
			   dp_peer->addr, dp_peer->peer_id, tid,
			   service_category,
			   flow_type);
		goto unlock;
	}

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
		   "TQM update MSDUQ peer %pM flow_num 0x%x svc %d tid %d\n",
		   dp_peer->addr, flow_number, service_category, tid);

unlock:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
out:
	rcu_read_unlock();
	return ret;
}
