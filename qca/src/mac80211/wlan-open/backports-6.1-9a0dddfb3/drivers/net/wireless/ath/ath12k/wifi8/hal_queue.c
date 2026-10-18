// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_queue.h"
#include "dp_pool.h"
#include "../debug.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_tx_tid.h"

#define ATH12K_MAX_AMSDU_AGGR_LIMIT 256
#define ATH12K_MAX_NUM_OF_EXT_DESCRIPTORS 24
#define ATH12K_MPDU_SEQ_NUM_MASK 0xFFF
#define ATH12K_PN_ADDR_HIGH_BYTE_MASK 0x000000FF

int ath12k_wifi8_hal_tx_msdu_queue_cleanup(struct ath12k_dp_hw_group *dp_hw_grp,
					   u32 msduq_idx,
					   struct hal_tx_msdu_flow_info *ti)
{
	struct hal_tx_msdu_flow *msduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
#ifndef CONFIG_IO_COHERENCY
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
#endif

#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_cpu(dev, ti->paddr, MSDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
#endif
	u32 info;

	msduq = pool_node_from_id(dp_hw_grp_wifi8->msduq_ctxt, msduq_idx);
	if (!msduq) {
		ath12k_err(ab, "MSDUQ flow is NULL\n");
		return -ENOMEM;
	}

	/* Clear MSDU count and byte count */

	info = le32_to_cpu(msduq->info0);
	info &= ~HAL_TX_MSDU_FLOW_ASSOCIATED_LINK_DESC_COUNT;
	info |= u32_encode_bits(0, HAL_TX_MSDU_FLOW_ASSOCIATED_LINK_DESC_COUNT);
	msduq->info0 = cpu_to_le32(info);
	info = le32_to_cpu(msduq->info1);
	info &= ~HAL_TX_MSDU_FLOW_MSDU_COUNT;
	info |= u32_encode_bits(0, HAL_TX_MSDU_FLOW_MSDU_COUNT);
	msduq->info1 = cpu_to_le32(info);
	info = le32_to_cpu(msduq->info2);
	info &= ~HAL_TX_MSDU_FLOW_FLOW_BYTE_COUNT;
	info |= u32_encode_bits(0, HAL_TX_MSDU_FLOW_FLOW_BYTE_COUNT);
	msduq->info2 = cpu_to_le32(info);

	/* Clear buffer address info */
	memset(&msduq->buf_addr_info_head, 0, sizeof(struct ath12k_buffer_addr));
	memset(&msduq->buf_addr_info_tail, 0, sizeof(struct ath12k_buffer_addr));

	/* Clear statistics and state */
	msduq->info6 = 0;
	msduq->info7 = 0;
	msduq->info8 = 0;

	/* Clear drop notification flags but keep thresholds */
	info = le32_to_cpu(msduq->info9);
	info &= ~(HAL_TX_MSDU_FLOW_GEN_SLOW_DROP_NOTIFICATION |
			HAL_TX_MSDU_FLOW_GEN_MED_DROP_NOTIFICATION |
			HAL_TX_MSDU_FLOW_GEN_HARD_DROP_NOTIFICATION |
			HAL_TX_MSDU_FLOW_ADD_FRAME_COUNT_SINCE_DROP |
			HAL_TX_MSDU_FLOW_SLOW_DROP_THRESHOLD);
	info |= u32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_SLOW_DROP_NOTIFICATION) |
		u32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_MED_DROP_NOTIFICATION) |
		u32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_HARD_DROP_NOTIFICATION) |
		u32_encode_bits(0, HAL_TX_MSDU_FLOW_ADD_FRAME_COUNT_SINCE_DROP) |
		u32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
				HAL_TX_MSDU_FLOW_SLOW_DROP_THRESHOLD);
	msduq->info9 = cpu_to_le32(info);

	info = le32_to_cpu(msduq->info10);
	info &= ~(HAL_TX_MSDU_FLOW_MED_DROP_THRESHOLD |
		  HAL_TX_MSDU_FLOW_HARD_DROP_THRESHOLD);
	info |= u32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
				HAL_TX_MSDU_FLOW_MED_DROP_THRESHOLD) |
		 u32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
				 HAL_TX_MSDU_FLOW_HARD_DROP_THRESHOLD);
	msduq->info10 = cpu_to_le32(info);

	msduq->info11 = 0;
	msduq->info12 = 0;
	info = le32_to_cpu(msduq->info13);
	info &= ~HAL_TX_MSDU_FLOW_PROCESSED_BYTE_COUNT_48_32;
	info |= u32_encode_bits(0, HAL_TX_MSDU_FLOW_PROCESSED_BYTE_COUNT_48_32);
	msduq->info13 = cpu_to_le32(info);

#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_device(dev, ti->paddr, MSDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
#endif

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MSDUQ cleanup:",
			msduq, sizeof(*msduq));

	return 0;
}

/**
 * ath12k_wifi8_cleanup_peer_msdu_queues() - Cleanup all MSDU queues for a peer
 * @dp_hw_grp: DP hardware group pointer
 * @peer: Peer whose MSDU queues need to be cleaned up
 *
 * This function iterates through all TIDs and management queues for a peer
 * and cleans up the MSDU queue descriptors by resetting counters and buffers.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_wifi8_cleanup_peer_msdu_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						 struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_tx_tid_info *tid;
	struct ath12k_dp_msdu_q_info *msduq;
	struct hal_tx_msdu_flow_info ti = {0};
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	int i, j;
	int ret = 0;

	if (!peer) {
		ath12k_err(ab, "Peer is NULL\n");
		return -EINVAL;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(peer);
	if (!tx_flow_info) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX, "No TX flow info for peer %pM\n",
			   peer->addr);
		return 0;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	/* Cleanup MSDU queues for all data TIDs */
	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		tid = &tx_flow_info->tid_info[i];
		if (!tid || tid->tid_num == ATH12K_INVALID_TID)
			continue;

		/* Cleanup all MSDU queues for this TID */
		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			msduq = tid->msduq[j];
			if (!msduq || msduq->msduq_state != ATH12K_TX_Q_INIT_DONE)
				continue;

			/* Prepare cleanup parameters */
			ti.paddr = msduq->msdu_q_paddr;
			ti.peer_id = peer->peer_id;
			ti.tid = (tid->tid_num < NON_QOS_TID) ? tid->tid_num :
								TQM_NON_DATA_TID;
			ti.mlo = peer->is_mlo ? 1 : 0;
			ti.bitmap = msduq->bitmap;

			/* Call HAL cleanup function */
			ret = ath12k_wifi8_hal_tx_msdu_queue_cleanup(dp_hw_grp,
								     msduq->msduq_idx,
								     &ti);
			if (ret) {
				ath12k_warn(ab, "Failed to cleanup MSDU queue for peer %pM TID %d bank %d: %d\n",
					    peer->addr, i, j, ret);
			}
		}
	}

	/* Cleanup multicast MSDU queue */
	if (tx_flow_info->mcast_msduq &&
	    tx_flow_info->mcast_msduq->msduq_state == ATH12K_TX_Q_INIT_DONE) {
		msduq = tx_flow_info->mcast_msduq;
		ti.paddr = msduq->msdu_q_paddr;

		ret = ath12k_wifi8_hal_tx_msdu_queue_cleanup(dp_hw_grp,
							     msduq->msduq_idx,
							     &ti);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup mcast MSDU queue for peer %pM: %d\n",
				    peer->addr, ret);
		}
	}

	/* Cleanup management MSDU queues */
	for (i = 0; i < MGMT_MSDUQ_TYPE_MAX; i++) {
		if (tx_flow_info->mgmt_msduq[i] &&
		    tx_flow_info->mgmt_msduq[i]->msduq_state == ATH12K_TX_Q_CREATED) {
			tid = &tx_flow_info->tid_info[i];
			msduq = tx_flow_info->mgmt_msduq[i];
			ti.paddr = msduq->msdu_q_paddr;
			ti.tid = tid->tid_num;

			ret = ath12k_wifi8_hal_tx_msdu_queue_cleanup(dp_hw_grp,
								     msduq->msduq_idx,
								     &ti);
			if (ret) {
				ath12k_warn(ab, "Failed to cleanup mgmt MSDU queue %d for peer %pM: %d\n",
					    i, peer->addr, ret);
			}
		}
	}

	/* Cleanup HOL (Head of Line) MSDU queue */
	if (tx_flow_info->hol_msduq &&
	    tx_flow_info->hol_msduq->msduq_state == ATH12K_TX_Q_CREATED) {
		msduq = tx_flow_info->hol_msduq;
		ti.paddr = msduq->msdu_q_paddr;

		ret = ath12k_wifi8_hal_tx_msdu_queue_cleanup(dp_hw_grp,
							     msduq->msduq_idx,
							     &ti);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup HOL MSDU queue for peer %pM: %d\n",
				    peer->addr, ret);
		}
	}

	spin_unlock_bh(&tx_flow_info->tx_q_lock);

	ath12k_dbg(ab, ATH12K_DBG_DP_TX, "Cleaned up MSDU queues for peer %pM\n",
		   peer->addr);
	return 0;
}
int ath12k_wifi8_hal_tx_msdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 msduq_idx,
					 struct hal_tx_msdu_flow_info *ti)
{
	struct hal_tx_msdu_flow *msduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	msduq = pool_node_from_id(dp_hw_grp_wifi8->msduq_ctxt, msduq_idx);
	if (!msduq) {
		ath12k_err(ab, "MSDUQ flow is NULL\n");
		return -ENOMEM;
	}

	ath12k_core_dma_sync_single_for_cpu(dev, ti->paddr,
					    MSDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
	if (ath12k_wifi8_hal_msduq_is_valid(msduq)) {
		ath12k_err(ab, "MSDUQ flow %pK already valid\n", msduq);
		ath12k_wifi8_hal_msduq_set_invalid(dp_hw_grp, msduq, ti->paddr);
		return -EEXIST;
	}

	memset(msduq, 0, sizeof(*msduq));
	msduq->header.info0 = le32_encode_bits(HAL_WIFITQM_OWNED,
					       HAL_TX_DESCRIPTOR_HEADER_OWNER) |
			      le32_encode_bits(HAL_WIFITRANSMIT_FLOW_DESCRIPTOR,
					       HAL_TX_DESCRIPTOR_HEADER_BUFFER_TYPE) |
			      le32_encode_bits(ti->queue_number,
					       HAL_TX_DESCRIPTOR_HEADER_QUEUE_NUMBER);

	msduq->info0 = le32_encode_bits(ti->msduq_sam_id, HAL_TX_MSDU_FLOW_QUEUE_SAM_ID) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_FLOW_VALID) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_EMPTY_TO_N_EMPTY) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_N_EMPTY_TO_EMPTY) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION) |
		       le32_encode_bits(1, HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION2) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_ASSOCIATED_LINK_DESC_COUNT) |
		       le32_encode_bits(ti->tid, HAL_TX_MSDU_FLOW_TID) |
		       le32_encode_bits(ti->mlo, HAL_TX_MSDU_FLOW_MLO_VALID);
	msduq->info1 = le32_encode_bits((ATH12K_MAX_AMSDU_AGGR_LIMIT / 3),
					HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES);
	msduq->info3 = le32_encode_bits(ti->peer_id, HAL_TX_MSDU_FLOW_SW_PEER_ID) |
		       le32_encode_bits(((2 * ATH12K_MAX_AMSDU_AGGR_LIMIT) / 3),
					HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES_2);
	msduq->info9 = le32_encode_bits(HAL_WIFIDROP_ENABLED,
					HAL_TX_MSDU_FLOW_DROP_RULE) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP0_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP0) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP1_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP1) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_SLOW_DROP_NOTIFICATION) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_MED_DROP_NOTIFICATION) |
		       le32_encode_bits(0, HAL_TX_MSDU_FLOW_GEN_HARD_DROP_NOTIFICATION) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP2_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP2) |
		       le32_encode_bits(((ti->bitmap & ATH12K_CHIP3_BITMAP_MASK) ? 1 : 0),
					HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP3) |
		       le32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
					HAL_TX_MSDU_FLOW_SLOW_DROP_THRESHOLD);
	msduq->info10 = le32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
					 HAL_TX_MSDU_FLOW_MED_DROP_THRESHOLD) |
			le32_encode_bits(ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD,
					 HAL_TX_MSDU_FLOW_HARD_DROP_THRESHOLD);
	msduq->info17 = le32_encode_bits(ti->stats_id,
					 HAL_TX_MSDU_FLOW_TQM_STATS_ID);

	if (ti->svc < HAL_TQM_SERVICE_CATEGORY_MAX)
		msduq->info16 =
			le32_encode_bits(1, HAL_TX_MSDU_FLOW_SERVICE_CATEGORY_VALID) |
			le32_encode_bits(ti->svc, HAL_TX_MSDU_FLOW_SERVICE_CATEGORY);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       MSDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MSDUQ setup:",
			msduq, sizeof(*msduq));
	return 0;
}

int ath12k_wifi8_hal_tx_mpdu_queue_cleanup(struct ath12k_dp_hw_group *dp_hw_grp,
					   u32 mpduq_idx,
					   struct hal_tx_mpdu_queue_head_info *ti)
{
	struct hal_tx_mpdu_queue_head *mpduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
#ifndef CONFIG_IO_COHERENCY
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
#endif
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u32 info, last_seq;

#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_cpu(dev, ti->paddr, MPDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
#endif

	mpduq = pool_node_from_id(dp_hw_grp_wifi8->mpduq_ctxt, mpduq_idx);
	if (!mpduq) {
		ath12k_err(ab, "MPDUQ flow is NULL\n");
		return -ENOMEM;
	}

	/* Clear MPDU type and count */
	info = le32_to_cpu(mpduq->info0);
	info &= ~(HAL_TX_MPDU_QUEUE_HEAD_MPDU_TYPE |
		  HAL_TX_MPDU_QUEUE_HEAD_MPDU_COUNT);
	info |= u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_MPDU_TYPE) |
		 u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_MPDU_COUNT);
	mpduq->info0 = cpu_to_le32(info);

	/* Clear queue byte count */
	info = le32_to_cpu(mpduq->info1);
	info &= ~HAL_TX_MPDU_QUEUE_HEAD_QUEUE_BYTE_COUNT;
	info |= u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_BYTE_COUNT);
	mpduq->info1 = cpu_to_le32(info);

	/* Clear sequence numbers and descriptor counts */
	info = le32_to_cpu(mpduq->info2);
	last_seq = le32_to_cpu(le32_get_bits(info,
					     HAL_TX_MPDU_QUEUE_HEAD_MPDU_LAST_SEQ_NUM));
	info &= ~(HAL_TX_MPDU_QUEUE_HEAD_MPDU_START_SEQ_NUM |
		  HAL_TX_MPDU_QUEUE_HEAD_ASSOC_LINK_DESC_CNT |
		  HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_LINK_DESC_IDX);
	info |= u32_encode_bits(last_seq, HAL_TX_MPDU_QUEUE_HEAD_MPDU_START_SEQ_NUM) |
		u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_ASSOC_LINK_DESC_CNT) |
		u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_LINK_DESC_IDX);
	mpduq->info2 = cpu_to_le32(info);

	/* Clear extension descriptor count */
	info = le32_to_cpu(mpduq->info3);
	info &= ~HAL_TX_MPDU_QUEUE_HEAD_NUM_EXT_DESC_IN_USE;
	info |= u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_NUM_EXT_DESC_IN_USE);
	mpduq->info3 = cpu_to_le32(info);

	info = le32_to_cpu(mpduq->info5);
	info &= ~HAL_TX_MPDU_QUEUE_HEAD_NUM_OF_EXT_DESC;

	/* Reset extension descriptor count to default */
	info |= u32_encode_bits(ATH12K_MAX_NUM_OF_EXT_DESCRIPTORS,
				HAL_TX_MPDU_QUEUE_HEAD_NUM_OF_EXT_DESC);
	mpduq->info5 = cpu_to_le32(info);

	mpduq->info6 = 0;

	/* Clear last MPDU index */
	info = le32_to_cpu(mpduq->info9);
	info &= ~HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_INDEX;
	info |= u32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_INDEX);
	mpduq->info9 = cpu_to_le32(info);

	/* Clear all buffer address info */
	memset(&mpduq->buf_addr_info_last_mpdu, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_first_ext, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_last_ext, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_link0, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_link1, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_link2, 0, sizeof(struct ath12k_buffer_addr));
	memset(&mpduq->buf_addr_info_link3, 0, sizeof(struct ath12k_buffer_addr));

#ifndef CONFIG_IO_COHERENCY
	dma_sync_single_for_device(dev, ti->paddr, MPDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
#endif

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MPDUQ cleanup:",
			mpduq, sizeof(*mpduq));

	return 0;
}

/**
 * ath12k_wifi8_cleanup_peer_mpdu_queues() - Cleanup all MPDU queues for a peer
 * @dp_hw_grp: DP hardware group pointer
 * @peer: Peer whose MPDU queues need to be cleaned up
 * @dp_vif: DP VIF pointer for encapsulation type
 *
 * This function iterates through all TIDs and management queues for a peer
 * and cleans up the MPDU queue head descriptors by resetting counters and buffers.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_wifi8_cleanup_peer_mpdu_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						 struct ath12k_dp_peer *peer,
						 struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_tx_tid_info *tid;
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct hal_tx_mpdu_queue_head_info ti = {0};
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_sta *ahsta;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id;
	int i, j;
	int ret = 0;

	if (!peer || !dp_vif) {
		ath12k_err(ab, "Peer or VIF is NULL\n");
		return -EINVAL;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(peer);
	if (!tx_flow_info) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX, "No TX flow info for peer %pM\n",
			   peer->addr);
		return 0;
	}

	/* Prepare common TI parameters */
	ti.peer_id = peer->peer_id;
	ti.encap_type = dp_vif->tx_encap_type;
	ti.mlo = peer->is_mlo;
	ti.assoc_link_id = ATH12K_INVALID_LINK_ID;
	ti.link_id1 = ATH12K_INVALID_LINK_ID;
	ti.link_id2 = ATH12K_INVALID_LINK_ID - 1;

	/* Get MLO link IDs if peer has STA */
	if (peer->sta) {
		rcu_read_lock();
		ahsta = ath12k_sta_to_ahsta(peer->sta);
		ti.assoc_link_id = ahsta->assoc_link_id;

		for (j = 0; j < ATH12K_DP_PEER_MAX_MLO_LINKS; j++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, j);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (hw_link_id == ti.assoc_link_id)
				continue;

			if (ti.link_id1 == ATH12K_INVALID_LINK_ID) {
				ti.link_id1 = hw_link_id;
				continue;
			}
			if (ti.link_id2 == (ATH12K_INVALID_LINK_ID - 1)) {
				ti.link_id2 = hw_link_id;
				break;
			}
		}
		rcu_read_unlock();
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	/* Cleanup MPDU queues for all data TIDs */
	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		tid = &tx_flow_info->tid_info[i];
		if (!tid || tid->tid_num == ATH12K_INVALID_TID)
			continue;

		mpduq = tid->mpduq;
		if (!mpduq || mpduq->mpduq_state != ATH12K_TX_Q_INIT_DONE)
			continue;

		/* Prepare cleanup parameters */
		ti.paddr = mpduq->mpdu_q_paddr;
		ti.tid = (tid->tid_num < NON_QOS_TID) ? tid->tid_num : TQM_NON_DATA_TID;
		ti.pn_dma_addr = mpduq->pn_addr;

		/* Call HAL cleanup function */
		ret = ath12k_wifi8_hal_tx_mpdu_queue_cleanup(dp_hw_grp,
							     mpduq->mpduq_id,
							     &ti);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup MPDU queue for peer %pM TID %d: %d\n",
				    peer->addr, i, ret);
		}
	}

	/* Cleanup multicast MPDU queue */
	if (tx_flow_info->mcast_mpduq &&
	    tx_flow_info->mcast_mpduq->mpduq_state == ATH12K_TX_Q_INIT_DONE) {
		mpduq = tx_flow_info->mcast_mpduq;
		ti.paddr = mpduq->mpdu_q_paddr;
		ti.tid = TQM_NON_DATA_TID;

		ret = ath12k_wifi8_hal_tx_mpdu_queue_cleanup(dp_hw_grp,
							     mpduq->mpduq_id,
							     &ti);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup mcast MPDU queue for peer %pM: %d\n",
				    peer->addr, ret);
		}
	}

	/* Cleanup management MPDU queue */
	if (tx_flow_info->mgmt_mpduq &&
	    tx_flow_info->mgmt_mpduq->mpduq_state == ATH12K_TX_Q_CREATED) {
		mpduq = tx_flow_info->mgmt_mpduq;
		ti.paddr = mpduq->mpdu_q_paddr;
		ti.tid = TQM_NON_DATA_TID;

		ret = ath12k_wifi8_hal_tx_mpdu_queue_cleanup(dp_hw_grp,
							     mpduq->mpduq_id,
							     &ti);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup mgmt MPDU queue for peer %pM: %d\n",
				    peer->addr, ret);
		}
	}

	spin_unlock_bh(&tx_flow_info->tx_q_lock);

	ath12k_dbg(ab, ATH12K_DBG_DP_TX, "Cleaned up MPDU queues for peer %pM\n",
		   peer->addr);
	return 0;
}

/**
 * ath12k_wifi8_cleanup_all_peers_tx_queues() - Cleanup TX queues for all peers
 * @dp_hw: DP hardware pointer containing peer list
 * @dp_hw_grp: DP hardware group pointer
 *
 * This function iterates through all peers in the dp_hw->peers list and
 * cleans up both MSDU and MPDU queue descriptors for each peer. For MPDU
 * cleanup, it uses the VIF associated with each peer.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_wifi8_cleanup_all_peers_tx_queues(struct ath12k_dp_hw *dp_hw,
					     struct ath12k_dp_hw_group *dp_hw_grp,
					     struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *peer;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_vif *dp_vif;
	struct ieee80211_vif *vif;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	int peer_count = 0;
	int ret = 0;

	if (!dp_hw || !dp_hw_grp) {
		if (ab)
			ath12k_err(ab, "DP HW or DP HW group is NULL\n");
		return -EINVAL;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Starting TX queue cleanup for all peers\n");

	spin_lock_bh(&dp_hw->peer_list_lock);

	/* Iterate through all peers in the list */
	list_for_each_entry(peer, &dp_hw->peers, list) {
		if (!peer)
			continue;

		peer_count++;

		/* Cleanup MSDU queues for this peer */
		ret = ath12k_wifi8_cleanup_peer_msdu_queues(dp_hw_grp, peer);
		if (ret) {
			ath12k_warn(ab, "Failed to cleanup MSDU queues for peer %pM: %d\n",
				    peer->addr, ret);
			/* Continue with other peers even if one fails */
		}

		/* Cleanup MPDU queues for this peer using its associated VIF */
		dp_vif = NULL;
		rcu_read_lock();
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer,
								   dp_pdev->hw_link_id);
		if (link_peer && ath12k_dp_link_peer_get_vif(link_peer)) {
			vif = ath12k_dp_link_peer_get_vif(link_peer);
			ahvif = ath12k_vif_to_ahvif(vif);
			if (ahvif)
				dp_vif = &ahvif->dp_vif;
		}
		rcu_read_unlock();

		if (dp_vif) {
			ret = ath12k_wifi8_cleanup_peer_mpdu_queues(dp_hw_grp,
								    peer,
								    dp_vif);
			if (ret) {
				ath12k_warn(ab, "Failed to cleanup MPDU queues for peer %pM: %d\n",
					    peer->addr, ret);
				/* Continue with other peers even if one fails */
			}
		} else {
			ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
				   "No VIF found for peer %pM, skipping MPDU cleanup\n",
				   peer->addr);
		}
	}

	spin_unlock_bh(&dp_hw->peer_list_lock);

	ath12k_dbg(ab, ATH12K_DBG_DP_UMAC_RESET,
		   "Completed TX queue cleanup for %d peers\n", peer_count);

	return 0;
}

int ath12k_wifi8_hal_tx_mpdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 mpduq_idx,
					 struct hal_tx_mpdu_queue_head_info *ti)
{
	struct hal_tx_mpdu_queue_head *mpduq;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u32 paddr_lo;
	u8 paddr_hi;

	mpduq = pool_node_from_id(dp_hw_grp_wifi8->mpduq_ctxt, mpduq_idx);
	if (!mpduq) {
		ath12k_err(ab, "MPDUQ flow is NULL\n");
		return -ENOMEM;
	}

	ath12k_core_dma_sync_single_for_cpu(dev, ti->paddr,
					    MPDU_STRUCT_SZ, DMA_BIDIRECTIONAL);
	if (ath12k_wifi8_hal_mpduq_is_valid(mpduq)) {
		ath12k_err(ab, "MPDUQ flow %pK already valid\n", mpduq);
		ath12k_wifi8_hal_mpduq_set_invalid(dp_hw_grp, mpduq, ti->paddr);
		return -EEXIST;
	}

	memset(mpduq, 0, sizeof(*mpduq));
	mpduq->header.info0 = le32_encode_bits(HAL_WIFITQM_OWNED,
					       HAL_TX_DESCRIPTOR_HEADER_OWNER) |
			      le32_encode_bits(HAL_WIFITRANSMIT_MPDU_HEAD_DESCRIPTOR,
					       HAL_TX_DESCRIPTOR_HEADER_BUFFER_TYPE) |
			      le32_encode_bits(ti->queue_number,
					       HAL_TX_DESCRIPTOR_HEADER_QUEUE_NUMBER);
	mpduq->info0 = le32_encode_bits(ti->mlo, HAL_TX_MPDU_QUEUE_HEAD_MLO_VALID) |
		       le32_encode_bits(HAL_WIFIMPDU_TYPE_BASIC,
					HAL_TX_MPDU_QUEUE_HEAD_MPDU_TYPE);
	mpduq->info2 = le32_encode_bits(1, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID) |
		       le32_encode_bits(0,
					HAL_TX_MPDU_QUEUE_HEAD_ASSOC_LINK_DESC_CNT) |
		       le32_encode_bits(ATH12K_MPDU_SEQ_NUM_MASK,
					HAL_TX_MPDU_QUEUE_HEAD_MPDU_LAST_SEQ_NUM);
	mpduq->info3 = le32_encode_bits(ti->tid, HAL_TX_MPDU_QUEUE_HEAD_TID) |
		       le32_encode_bits(ti->peer_id, HAL_TX_MPDU_QUEUE_HEAD_SW_PEER_ID);
	paddr_lo = lower_32_bits(ti->pn_dma_addr);
	mpduq->info4 = le32_encode_bits(paddr_lo,
					HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_31_0);
	paddr_hi = (u8)(upper_32_bits(ti->pn_dma_addr) & ATH12K_PN_ADDR_HIGH_BYTE_MASK);
	mpduq->info5 = le32_encode_bits(paddr_hi, HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_39_32);
	if (ti->wapi)
		mpduq->info5 |= le32_encode_bits(2, HAL_TX_MPDU_QUEUE_HEAD_PN_INC_VALUE);
	else
		mpduq->info5 |= le32_encode_bits(1, HAL_TX_MPDU_QUEUE_HEAD_PN_INC_VALUE);

	mpduq->info5 |= le32_encode_bits(ti->header_len,
					 HAL_TX_MPDU_QUEUE_HEAD_MPDU_HDR_LEN);
	mpduq->info5 |= le32_encode_bits(ATH12K_MAX_NUM_OF_EXT_DESCRIPTORS,
					 HAL_TX_MPDU_QUEUE_HEAD_NUM_OF_EXT_DESC);
	mpduq->info9 = le32_encode_bits(ti->assoc_link_id,
					HAL_TX_MPDU_QUEUE_HEAD_LINK0_ID) |
		       le32_encode_bits(ti->link_id1, HAL_TX_MPDU_QUEUE_HEAD_LINK1_ID) |
		       le32_encode_bits(ti->link_id2, HAL_TX_MPDU_QUEUE_HEAD_LINK2_ID);
	mpduq->info17 = le32_encode_bits(ti->stats_id,
					 HAL_TX_MPDU_QUEUE_HEAD_TQM_PER_MLO_STATS_ID);

	mpduq->info21 = le32_encode_bits(ti->mpduq_sam_id, HAL_TX_MPDU_QUEUE_HEAD_SAM_ID);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       MPDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);

	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal MPDUQ setup:",
			mpduq, sizeof(*mpduq));
	return 0;
}

void ath12k_wifi8_hal_txpt_classify_info_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					       struct hal_txpt_classify_info *tx_tid_ptr,
					       struct hal_txpt_classify_data *ti)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	if (!tx_tid_ptr) {
		ath12k_err(ab, "txpt_classify_info is NULL\n");
		return;
	}
	switch (ti->subtype) {
	case HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP:
		tx_tid_ptr->info0 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
		break;
	case HAL_CLASSIFY_BANK_SUBTYPE_UDP:
		tx_tid_ptr->info1 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
		break;
	case HAL_CLASSIFY_BANK_SUBTYPE_MIXED:
		tx_tid_ptr->info0 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
		tx_tid_ptr->info1 =
			le32_encode_bits(ti->msdu_paddr,
					 HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
		break;
	default:
		ath12k_err(ab, "Invalid classify bank subtype %d\n", ti->subtype);
		return;
	}

	tx_tid_ptr->info2 = le32_encode_bits(ti->flow_handler,
					     HAL_TXPT_CLASSIFY_TQM_FLOW_HANDLER) |
			    le32_encode_bits(ti->flow_loop_handler,
					     HAL_TXPT_CLASSIFY_TQM_FLOW_LOOP_HANDLER) |
			    le32_encode_bits(ti->msdu_drop,
					     HAL_TXPT_CLASSIFY_MSDU_DROP) |
			    le32_encode_bits(ti->metadata,
					     HAL_TXPT_CLASSIFY_METADATA) |
			    le32_encode_bits(ti->assoc_link_id,
					     HAL_TXPT_CLASSIFY_TCL_FW_LINK_ID);

	ath12k_core_dma_sync_single_for_device(dev, ti->paddr,
					       ATH12K_SIZE_OF_TID_INFO,
					       DMA_BIDIRECTIONAL);

	ath12k_wifi8_hal_txpt_classify_info_flush(ab);
	ath12k_dbg_dump(ab, ATH12K_DBG_HAL, NULL, "Hal CLASSIFY INFO setup:",
			tx_tid_ptr, sizeof(*tx_tid_ptr));
}

bool ath12k_wifi8_hal_msduq_is_valid(struct hal_tx_msdu_flow *msduq)
{
	return !!le32_get_bits(msduq->info0, HAL_TX_MSDU_FLOW_FLOW_VALID);
}

bool ath12k_wifi8_hal_mpduq_is_valid(struct hal_tx_mpdu_queue_head *mpduq)
{
	return !!le32_get_bits(mpduq->info2, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID);
}

u32 ath12k_wifi8_hal_get_txpt_flow_ptr(struct hal_txpt_classify_info *tx_tid_ptr,
				       enum hal_classify_bank_subtype subtype)
{
	if (subtype == HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP)
		return le32_get_bits(tx_tid_ptr->info0,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8);
	else if (subtype == HAL_CLASSIFY_BANK_SUBTYPE_UDP)
		return le32_get_bits(tx_tid_ptr->info1,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
	else if ((subtype == HAL_CLASSIFY_BANK_SUBTYPE_MIXED) &&
		 (le32_get_bits(tx_tid_ptr->info1,
				HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8) ==
		  le32_get_bits(tx_tid_ptr->info0,
				HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8)))
		return le32_get_bits(tx_tid_ptr->info1,
				     HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8);
	return 0;
}

void ath12k_wifi8_hal_msduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_msdu_flow *msduq,
					dma_addr_t paddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	msduq->info0 = le32_encode_bits(0, HAL_TX_MSDU_FLOW_FLOW_VALID);

	ath12k_core_dma_sync_single_for_device(dev, paddr,
					       MSDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);
}

void ath12k_wifi8_hal_mpduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_mpdu_queue_head *mpduq,
					dma_addr_t paddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	mpduq->info2 = le32_encode_bits(0, HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID);
	ath12k_core_dma_sync_single_for_device(dev, paddr,
					       MPDU_STRUCT_SZ,
					       DMA_BIDIRECTIONAL);
}
