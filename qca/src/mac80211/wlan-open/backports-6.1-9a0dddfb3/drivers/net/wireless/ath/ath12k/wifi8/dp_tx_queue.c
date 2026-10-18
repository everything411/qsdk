// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_queue.h"
#include "dp.h"
#include "../dp_tx.h"

u8 ath12k_tx_get_bank_id(struct ath12k_dp_peer *peer,
			 enum htt_tx_tid_msduq_mpdu_type msduq_idx,
			 enum ath12k_classify_bank_subid *bank_sub_id)
{
	u8 bank_id = 0;

	if (msduq_idx < HTT_TID_MISC_MSDUQ_TYPE_START) {
		bank_id = HTT_MSDUQ_INDEX_TO_CLASSIFY_BANK_ID(msduq_idx);
		/* HTT_TID_MSDUQ_NONUDP:0 and HTT_TID_MSDUQ_UDP:1 */
		/* Based on sub_id we try to assign which flowq pointer is assigned */
		if (bank_sub_id)
			*bank_sub_id = HTT_MSDUQ_INDEX_TO_CLASSIFY_BANK_SUBID(msduq_idx);
	}

	if (bank_id >= ATH12K_NUM_TX_CLASSIFY_BANKS)
		ath12k_err(NULL, "Error: Bank_id exceeded %d msduq_id %d\n",
			   bank_id, msduq_idx);
	return bank_id;
}

struct hal_txpt_classify_info *ath12k_get_txpt_info_ptr(struct ath12k_dp_peer *peer,
							u8 bank_id, u8 tidpos)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	u32 offset;

	if (bank_id >= ATH12K_NUM_TX_CLASSIFY_BANKS || tidpos >= ATH12K_NUM_TID_PER_BANK)
		return NULL;

	offset = (bank_id * ATH12K_NUM_TID_PER_BANK + tidpos) * ATH12K_SIZE_OF_TID_INFO;
	return (struct hal_txpt_classify_info *)
		((char *)tx_flow_info->hw_who_classify_info_vaddr + offset);
}

dma_addr_t ath12k_get_txpt_paddr(struct ath12k_dp_peer *peer, u8 bank_id, u8 tidpos)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	u32 offset;

	if (bank_id >= ATH12K_NUM_TX_CLASSIFY_BANKS || tidpos >= ATH12K_NUM_TID_PER_BANK)
		return 0;

	offset = (bank_id * ATH12K_NUM_TID_PER_BANK + tidpos) * ATH12K_SIZE_OF_TID_INFO;
	return (dma_addr_t)((char *)tx_flow_info->hw_who_classify_info_paddr + offset);
}

void ath12k_dp_txpt_update_to_all(struct ath12k_dp_peer *peer,
				  struct hal_txpt_classify_info *txpt_info_ptr)
{
	struct hal_txpt_classify_info *tx_tidx = NULL;

	if (txpt_info_ptr) {
		/* get bank_id 0, tid0 txpt ptr */
		tx_tidx = ath12k_get_txpt_info_ptr(peer, 0, 1);

		/* Copy all default tids information to all 0-7 tids */
		for (int tid_num = 0; tid_num < 8; tid_num++) {
			/* Assign the Non Qos msdu flowq to all default tids */
			memcpy(tx_tidx, txpt_info_ptr,
			       sizeof(struct hal_txpt_classify_info));
			tx_tidx++;
		}
	}
}

void ath12k_set_txpt_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	int tid;
	struct hal_txpt_classify_data ti = {0};
	dma_addr_t txpt_paddr;

	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, tid + 1);
		txpt_paddr = ath12k_get_txpt_paddr(peer, 0, tid + 1);

		if (tx_tid_ptr) {
			ti.flow_handler = HAL_WIFITXPT_TO_TQM;
			ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
			ti.paddr = txpt_paddr;
			ti.metadata = peer ? peer->peer_id : HAL_INVALID_PEERID;
			ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, tx_tid_ptr,
								  &ti);
		}
	}
}

int ath12k_tx_classify_info_alloc(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_dp_peer *peer, u8 tid_num,
				  enum ath12k_txpt_classify_flag flag, u8 flow_mask)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	u64 msdu_flow_dma_ptr = 0;
	struct ath12k_dp_msdu_q_info *msdu_flow_ptr = NULL;
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct hal_txpt_classify_data ti = {0};
	u8 bank_id;
	u8 tidno = (tid_num < NON_QOS_TID) ? tid_num : 0;
	dma_addr_t txpt_paddr;
	int requested = hweight8(flow_mask);
	int success_count = 0;
	enum ath12k_classify_bank_subid bank_sub_id;
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	if (!peer)
		return -EINVAL;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(peer);
	if (!tx_flow_info)
		return -ENOENT;

	if (flow_mask == 0)
		flow_mask |= (1 << HTT_TID_MSDUQ_UDP);

	for (enum htt_tx_tid_msduq_mpdu_type htt_msduq_idx = 0;
	     flow_mask; flow_mask >>= 1, htt_msduq_idx++) {
		bank_id = 0;
		bank_sub_id = CLASSIFY_BANK_SUBID_NON_UDP;

		if ((flow_mask & 0x1) == 0)
			continue;

		bank_id = ath12k_tx_get_bank_id(peer, htt_msduq_idx, &bank_sub_id);

		if (tid_num < ATH12K_MAX_NUM_DATA_TIDS || tid_num == NON_QOS_TID) {
			tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, bank_id, (tidno + 1));
			txpt_paddr = ath12k_get_txpt_paddr(peer, bank_id, (tidno + 1));
		}

		if (!tx_tid_ptr || txpt_paddr == 0)
			continue;

		ath12k_core_dma_sync_single_for_cpu(dev,
						    txpt_paddr,
						    ATH12K_SIZE_OF_TID_INFO,
						    DMA_BIDIRECTIONAL);
		if (bank_sub_id == CLASSIFY_BANK_SUBID_UDP) {
			/* Allocate and Initialize MSDU flow (UDP) */
			if (ath12k_wifi8_hal_get_txpt_flow_ptr(tx_tid_ptr,
			    HAL_CLASSIFY_BANK_SUBTYPE_UDP)) {
				success_count++;
				continue;
			}
			msdu_flow_ptr =
			tx_flow_info->tid_info[tidno].msduq[htt_msduq_idx] =
				ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp,
								peer, tid_num,
								htt_msduq_idx,
								MGMT_MSDUQ_TYPE_MAX);
			if (!msdu_flow_ptr)
				return -ENOENT;
			tx_flow_info->tid_info[tidno].num_of_active_msdu_queues++;
			msdu_flow_dma_ptr = (u64)msdu_flow_ptr->msdu_q_paddr;
			ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_UDP;
		} else {
			if (ath12k_wifi8_hal_get_txpt_flow_ptr(tx_tid_ptr,
			    HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP)) {
				success_count++;
				continue;
			}
			msdu_flow_ptr =
			tx_flow_info->tid_info[tidno].msduq[htt_msduq_idx] =
				ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp, peer,
								tid_num, htt_msduq_idx,
								MGMT_MSDUQ_TYPE_MAX);
			if (!msdu_flow_ptr)
				return -ENOENT;
			tx_flow_info->tid_info[tidno].num_of_active_msdu_queues++;
			msdu_flow_dma_ptr = (u64)msdu_flow_ptr->msdu_q_paddr;
			ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP;
		}

		ti.msdu_paddr = (u32)((msdu_flow_dma_ptr >> 0x8) & 0xFFFFFFFF);
		ti.paddr = txpt_paddr;
		ti.flow_handler = HAL_WIFITXPT_TO_TQM;
		ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
		ti.msdu_drop = 0;
		ti.metadata = peer ? peer->peer_id : HAL_INVALID_PEERID;
		if (peer->is_vdev_peer)
			ti.assoc_link_id = peer->hw_link_id;
		else
			ti.assoc_link_id = peer->assoc_hw_link_id;

		ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, tx_tid_ptr, &ti);
		success_count++;
	}
	return success_count == requested ? 0 : -EFAULT;
}

int ath12k_peer_alloc_default_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				     struct ath12k_dp_peer *peer,
				     struct ath12k_dp_vif *dp_vif, bool is_qos)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	struct ath12k_dp_tx_tid_info *tid;
	struct hal_txpt_classify_info *tx_classify_info = NULL;
	int msdu_alloc_ret;
	u8 tid_num;
	int ret = 0;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	if (is_qos) {
		ath12k_set_txpt_flow_ptr(dp_hw_grp, peer);
		for (tid_num = 0; tid_num < ATH12K_MAX_NUM_DATA_TIDS; tid_num++) {
			/* only allocate default tid queues */
			if (!(ATH12K_DEFAULT_TID_MAP & BIT(tid_num)))
				continue;
			tx_flow_info->tid_info[tid_num].mpduq =
				ath12k_peer_alloc_tid(dp_hw_grp, peer,
						      dp_vif->tx_encap_type,
						      tid_num, &tid,
						      HTT_TID_MSDUQ_UDP);
			msdu_alloc_ret =
				ath12k_tx_classify_info_alloc(dp_hw_grp, peer,
							      tid_num,
							      TXPT_DEFAULT_FLOWQ_ALLOC,
							      (1 << HTT_TID_MSDUQ_NONUDP)
							      | (1 << HTT_TID_MSDUQ_UDP));
			if (!tx_flow_info->tid_info[tid_num].mpduq || msdu_alloc_ret) {
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				return -ENOMEM;
			}
		}
	} else {
		tx_flow_info->tid_info[DEFAULT_TID].mpduq =
			ath12k_peer_alloc_tid(dp_hw_grp, peer, dp_vif->tx_encap_type,
					      NON_QOS_TID, &tid,
					      HTT_TID_MSDUQ_UDP);
		msdu_alloc_ret = ath12k_tx_classify_info_alloc(dp_hw_grp,
							  peer,
							  NON_QOS_TID,
							  TXPT_DEFAULT_FLOWQ_ALLOC,
							  (1 << HTT_TID_MSDUQ_NONUDP) |
							  (1 << HTT_TID_MSDUQ_UDP));
		if (!tx_flow_info->tid_info[DEFAULT_TID].mpduq || msdu_alloc_ret) {
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return -ENOMEM;
		}
		//Default non-qos tid: bankid 0 tid 0 with tidpos 1
		tx_classify_info = ath12k_get_txpt_info_ptr(peer, 0, 1);
		ath12k_dp_txpt_update_to_all(peer, tx_classify_info);
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);

	ret = ath12k_dp_tx_peer_msduq_mpduq_setup(dp_hw_grp, peer,
						  ATH12K_GROUP_MAX_RADIO);
	return ret;
}

int ath12k_peer_alloc_mgmt_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct ath12k_dp_peer *peer,
				  struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
		ath12k_dp_get_tx_flow_info_from_peer(peer);
	struct ath12k_dp_tx_tid_info *tid = NULL;
	enum ath12k_mgmt_msduq_type mgmt_msduq_type;
	enum htt_tx_tid_msduq_mpdu_type link_mgmt_msduq_type;
	u8 link;
	int ret = 0;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	tx_flow_info->mgmt_mpduq =
		ath12k_peer_alloc_tid(dp_hw_grp, peer, dp_vif->tx_encap_type,
				      MLO_MGMT_TID, &tid, MGMT_TID_MSDUQ_TYPE);
	if (!tx_flow_info->mgmt_mpduq) {
		ret = -ENOMEM;
		goto error;
	}
	if (peer->is_mlo) {
		tx_flow_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN] =
			ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp, peer,
							MLO_MGMT_TID,
							HTT_TID_MSDUQ_MGMT_LINK_AGNOSTIC,
							MGMT_MSDUQ_LINK_CMN);
		if (!tx_flow_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]) {
			ret = -ENOMEM;
			goto error;
		}
		for (link = 0; link < ATH12K_WMI_MLO_MAX_LINKS; link++) {
			mgmt_msduq_type = ATH12K_LINK_TO_MGMT_TYPE(link);
			link_mgmt_msduq_type = ATH12K_LINK_TO_MSDUQ_TYPE(link);
			tx_flow_info->mgmt_msduq[mgmt_msduq_type] =
				ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp, peer,
								MLO_MGMT_TID,
								link_mgmt_msduq_type,
								mgmt_msduq_type);
			if (!tx_flow_info->mgmt_msduq[mgmt_msduq_type]) {
				ret = -ENOMEM;
				goto error;
			}
		}
	} else {
		tx_flow_info->mgmt_msduq[MGMT_MSDUQ_NON_ML] =
			ath12k_init_alloc_tx_msdu_flowq(
					dp_hw_grp, peer,
					MLO_MGMT_TID,
					HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_0,
					MGMT_MSDUQ_NON_ML);
		if (!tx_flow_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]) {
			ret = -ENOMEM;
			goto error;
		}
	}
error:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

struct hal_txpt_classify_info
*ath12k_tx_alloc_mcast_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
				struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct hal_txpt_classify_data ti = {0};
	u64 msdu_flow_dma_ptr = 0;
	dma_addr_t txpt_paddr;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);

	sw_msduq_ptr = tx_flow_info->mcast_msduq;

	tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 1); //bank_id is 0 and tid pos is 1
	txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 1);
	if (!tx_tid_ptr || txpt_paddr == 0)
		return NULL;

	msdu_flow_dma_ptr = (u64)sw_msduq_ptr->msdu_q_paddr;
	ti.msdu_paddr = (u32)((msdu_flow_dma_ptr >> 0x8) & 0xFFFFFFFF);
	ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_MIXED;
	ti.flow_handler = HAL_WIFITXPT_TO_TQM;
	ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
	ti.metadata = peer ? peer->peer_id : HAL_INVALID_PEERID;
	/* mcast flowq will be allocated only for bss peer */
	ti.assoc_link_id = peer->hw_link_id;
	ti.paddr = txpt_paddr;
	ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, tx_tid_ptr, &ti);
	return tx_tid_ptr;
}

int ath12k_peer_alloc_mcast_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				   struct ath12k_dp_peer *peer,
				   struct ath12k_dp_vif *dp_vif)
{
	struct ath12k_dp_tx_tid_info *tid = NULL;
	struct hal_txpt_classify_info *txpt_info;
	int ret = 0;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	u8 tidno = NON_QOS_TID;

	if (peer->is_11s_mesh_peer)
		tidno = DEFAULT_TID;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	tx_flow_info->mcast_mpduq = ath12k_peer_alloc_tid(dp_hw_grp, peer,
							  dp_vif->tx_encap_type, tidno,
							  &tid, HTT_TID_MSDUQ_MCAST);
	tx_flow_info->mcast_msduq = ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp, peer,
								    tidno,
								    HTT_TID_MSDUQ_MCAST,
								    MGMT_MSDUQ_TYPE_MAX);

	if (!tx_flow_info->mcast_msduq || !tx_flow_info->mcast_mpduq) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return -ENOMEM;
	}
	txpt_info = ath12k_tx_alloc_mcast_flow_ptr(dp_hw_grp, peer);
	if (!txpt_info) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return -ENOMEM;
	}
	ath12k_dp_txpt_update_to_all(peer, txpt_info);
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	ret = ath12k_dp_tx_mcast_msduq_mpduq_setup(dp_hw_grp, peer);
	return ret;
}

int ath12k_tx_alloc_hol_flow_ptr(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct hal_txpt_classify_data ti = {0};
	u64 msdu_flow_dma_ptr = 0;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	dma_addr_t txpt_paddr;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);

	sw_msduq_ptr = tx_flow_info->hol_msduq;

	/* bank_id and tid pos is 0 */
	tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 0);
	txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 0);
	if (!tx_tid_ptr || txpt_paddr == 0)
		return -ENOMEM;

	msdu_flow_dma_ptr = (u64)sw_msduq_ptr->msdu_q_paddr;
	ti.msdu_paddr = (u32)((msdu_flow_dma_ptr >> 0x8) & 0xFFFFFFFF);
	ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP;
	ti.flow_handler = HAL_WIFITXPT_TO_TQM;
	ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
	ti.metadata = peer ? peer->peer_id : HAL_INVALID_PEERID;
	ti.assoc_link_id = peer->assoc_hw_link_id;

	ti.paddr = txpt_paddr;
	ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, tx_tid_ptr, &ti);
	return 0;
}

int ath12k_peer_alloc_hol_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer,
				 struct ath12k_dp_vif *dp_vif,
				 bool is_qos)
{
	struct ath12k_dp_tx_tid_info *tid_info = NULL;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	int ret = 0;
	u8 tid_num, tid_idx;

	if (is_qos) {
		tid_num = ATH12K_HOL_TID;
		tid_idx = ATH12K_HOL_TID;
	} else {
		tid_num = NON_QOS_TID;
		tid_idx = DEFAULT_TID;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	tx_flow_info->holq_tid = tid_idx;
	if (!tx_flow_info->tid_info[tid_idx].mpduq) {
		tx_flow_info->tid_info[tid_idx].mpduq =
			ath12k_peer_alloc_tid(dp_hw_grp, peer,
					      dp_vif->tx_encap_type,
					      tid_num,
					      &tid_info, HTT_TID_MSDUQ_NONUDP);
	}
	tx_flow_info->hol_msduq = ath12k_init_alloc_tx_msdu_flowq(dp_hw_grp,
								  peer,
								  tid_num,
								  HTT_TID_MSDUQ_HOL,
								  MGMT_MSDUQ_TYPE_MAX);

	if (!tx_flow_info->hol_msduq || !tx_flow_info->tid_info[tid_idx].mpduq) {
		ret = -ENOMEM;
		goto error;
	}
	tx_flow_info->tid_info[tid_idx].num_of_active_msdu_queues++;
	ret = ath12k_tx_alloc_hol_flow_ptr(dp_hw_grp, peer);
error:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

/* can be improvised */
static inline void
ath12k_dp_tx_modify_q_params(struct ath12k_dp_msdu_q_info *msduq,
			     struct ath12k_dp_tx_queue_metadata *tx_queue_params)
{
	int bit;
	u32 bitmap = tx_queue_params->q_params.bitmap;

	for (bit = 0; bit < 32; bit++) {
		if (!(bitmap & (1u << bit)))
			continue;
		switch (bit) {
		case 0:
			msduq->svc_id = tx_queue_params->q_params.svc_id;
			break;
		default:
			return;
		}
	}
}

int ath12k_peer_alloc_dynamic_queue(struct ath12k_dp_hw_group *dp_hw_grp,
				    struct ath12k_dp_peer *peer,
				    struct ath12k_dp_tx_queue_metadata *tx_queue_params)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
				ath12k_dp_get_tx_flow_info_from_peer(peer);
	struct ath12k_dp_tx_tid_info *tid;
	struct ath12k_dp_tx_tid_info *orig_tid;
	struct ath12k_dp_msdu_q_info *msduq = NULL;
	struct ath12k_dp_mpdu_q_info *mpduq = NULL;
	u8 flow_type = tx_queue_params->flow_type;
	int ret = 0;
	int status;

	if (!tx_flow_info)
		return -EINVAL;

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	tid = &tx_flow_info->tid_info[tx_queue_params->tidno];
	msduq = tid->msduq[flow_type];
	mpduq = tid->mpduq;

	if (msduq && mpduq) {
		if (tx_queue_params->q_params.bitmap == 0) {
			/* If no field modified, simply return */
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return 0;
		}
		/* If bitmap is set, modify param and change state to modified */
		msduq->msduq_state = ATH12K_TX_Q_MODIFIED;
		mpduq->mpduq_state = ATH12K_TX_Q_MODIFIED;
		ath12k_dp_tx_modify_q_params(msduq, tx_queue_params);
		goto unlock;
	}
	if (tid->num_of_active_msdu_queues == 0 && !mpduq) {
		orig_tid = tid;
		mpduq = ath12k_peer_alloc_tid(dp_hw_grp, peer,
						   tx_queue_params->encap_type,
						   tx_queue_params->tidno, &tid,
						   flow_type);
		/* ath12k_peer_alloc_tid sets *ptid=NULL on error; restore
		 * orig_tid so we can safely write mpduq back to the tid slot.
		 */
		if (!tid)
			tid = orig_tid;
		if (!mpduq) {
			ath12k_err(NULL, "mpduq is NULL");
			ret = -ENOMEM;
			goto unlock;
		}
		tid->mpduq = mpduq;
	}
	if (!msduq) {
		status =
			ath12k_tx_classify_info_alloc(dp_hw_grp, peer,
						      tx_queue_params->tidno,
						      TXPT_DEFAULT_FLOWQ_ALLOC,
						      (1 << flow_type));
		msduq = tid->msduq[flow_type];

		if (status || !msduq) {
			ath12k_err(NULL, "status %d or msduq is null", status);
			if (tid->num_of_active_msdu_queues == 0 && mpduq) {
				ath12k_peer_free_tid(dp_hw_grp, mpduq, tid);
				tid->mpduq = NULL;
			}
			ret = -ENOMEM;
			goto unlock;
		}

		if (flow_type >= HTT_TID_MSDUQ_CUSTOM_0 &&
		    flow_type <= HTT_TID_MSDUQ_CUSTOM_1) {
			msduq->svc_id = tx_queue_params->q_params.svc_id;
			msduq->qm_id = tx_queue_params->q_params.qm_id;
			msduq->txop_intent = tx_queue_params->q_params.txop_intent;
		} else {
			msduq->svc_id = ATH12K_INVALID_SVC_ID;
		}
	}
unlock:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	if (msduq && mpduq)
		ret = ath12k_dp_tx_peer_msduq_mpduq_setup(dp_hw_grp, peer,
							  ATH12K_GROUP_MAX_RADIO);
	return ret;
}

void ath12k_tx_classify_info_free(struct ath12k_dp_hw_group *dp_hw_grp,
				  struct hal_txpt_classify_info **tx_tid_ptr,
				  dma_addr_t txpt_paddr)
{
	struct hal_txpt_classify_data ti = {0};
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	if (!tx_tid_ptr || !(*tx_tid_ptr))
		return;

	ath12k_core_dma_sync_single_for_cpu(dev,
					    txpt_paddr,
					    ATH12K_SIZE_OF_TID_INFO,
					    DMA_BIDIRECTIONAL);
	if (ath12k_wifi8_hal_get_txpt_flow_ptr(*tx_tid_ptr,
					       HAL_CLASSIFY_BANK_SUBTYPE_UDP)) {
		ti.msdu_paddr = 0;
		ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_UDP;
		ti.paddr = txpt_paddr;
		ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, *tx_tid_ptr, &ti);
	}
	if (ath12k_wifi8_hal_get_txpt_flow_ptr(*tx_tid_ptr,
					       HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP)) {
		ti.msdu_paddr = 0;
		ti.subtype = HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP;
		ti.paddr = txpt_paddr;
		ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp, *tx_tid_ptr, &ti);
	}
}

static inline void ath12k_peer_free_hol_queues(struct ath12k_dp_hw_group *dp_hw_grp,
					       struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_dp_tx_tid_info *ptid = NULL;
	dma_addr_t txpt_paddr;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	u8 holq_tid;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 0);
	txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 0);
	ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
	holq_tid = tx_flow_info->holq_tid;

	sw_msduq_ptr = tx_flow_info->hol_msduq;
	if (sw_msduq_ptr) {
		ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
		tx_flow_info->tid_info[holq_tid].num_of_active_msdu_queues--;
		tx_flow_info->hol_msduq = NULL;
	}
	if (tx_flow_info->tid_info[holq_tid].num_of_active_msdu_queues)
		goto end;

	ptid = &tx_flow_info->tid_info[holq_tid];
	sw_mpduq_ptr = tx_flow_info->tid_info[holq_tid].mpduq;
	ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, ptid);
	tx_flow_info->tid_info[holq_tid].mpduq = NULL;
end:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
}

static inline void ath12k_peer_free_mgmt_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	enum ath12k_mgmt_msduq_type mgmt_msduq_type;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	u8 link;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	if (peer->is_mlo) {
		sw_msduq_ptr = tx_flow_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN];
		ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
		tx_flow_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN] = NULL;
		for (link = 0; link < ATH12K_WMI_MLO_MAX_LINKS; link++) {
			mgmt_msduq_type = ATH12K_LINK_TO_MGMT_TYPE(link);
			sw_msduq_ptr = tx_flow_info->mgmt_msduq[mgmt_msduq_type];
			ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
			tx_flow_info->mgmt_msduq[mgmt_msduq_type] = NULL;
		}
	} else {
		sw_msduq_ptr = tx_flow_info->mgmt_msduq[MGMT_MSDUQ_NON_ML];
		ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
		tx_flow_info->mgmt_msduq[MGMT_MSDUQ_NON_ML] = NULL;
	}
	sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
	ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, NULL);
	tx_flow_info->mgmt_mpduq = NULL;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
}

static inline void ath12k_peer_free_mcast_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						 struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	dma_addr_t txpt_paddr;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 1);
	txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 1);
	ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
	ath12k_dp_txpt_update_to_all(peer, tx_tid_ptr);

	sw_msduq_ptr = tx_flow_info->mcast_msduq;
	ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
	tx_flow_info->mcast_msduq = NULL;

	sw_mpduq_ptr = tx_flow_info->mcast_mpduq;
	ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, NULL);
	tx_flow_info->mcast_mpduq = NULL;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
}

static inline void ath12k_peer_free_data_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						struct ath12k_dp_peer *peer)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_dp_tx_tid_info *tid = NULL;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	dma_addr_t txpt_paddr;
	u8 tid_num, q;
	u8 holq_tid;

	spin_lock_bh(&tx_flow_info->tx_q_lock);

	tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 0);
	txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 0);
	ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
	sw_msduq_ptr = tx_flow_info->hol_msduq;
	if (sw_msduq_ptr) {
		ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
		holq_tid = tx_flow_info->holq_tid;
		tx_flow_info->tid_info[holq_tid].num_of_active_msdu_queues--;
		tx_flow_info->hol_msduq = NULL;
	}
	for (tid_num = 0; tid_num < ATH12K_MAX_NUM_DATA_TIDS; tid_num++) {
		tid = &tx_flow_info->tid_info[tid_num];
		for (q = 0; q < ATH12K_MAX_DP_MSDUQ_PER_TID; q++) {
			tx_tid_ptr = ath12k_get_txpt_info_ptr(
					peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));
			txpt_paddr = ath12k_get_txpt_paddr(
					peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));
			ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
			sw_msduq_ptr = tid->msduq[q];
			if (sw_msduq_ptr) {
				ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
				tid->num_of_active_msdu_queues--;
				tid->msduq[q] = NULL;
			}
		}
		if (tid->num_of_active_msdu_queues)
			continue;
		sw_mpduq_ptr = tx_flow_info->tid_info[tid_num].mpduq;
		ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, tid);
		tx_flow_info->tid_info[tid_num].mpduq = NULL;
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
}

static inline void ath12k_peer_free_default_queues(struct ath12k_dp_hw_group *dp_hw_grp,
						   struct ath12k_dp_peer *peer,
						   bool is_qos)
{
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_dp_tx_flow_info *tx_flow_info =
				ath12k_dp_get_tx_flow_info_from_peer(peer);
	dma_addr_t txpt_paddr;
	u8 tid_num;
	struct ath12k_dp_tx_tid_info *tid_info = NULL;

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	if (is_qos) {
		for (tid_num = 0; tid_num < ATH12K_MAX_NUM_DATA_TIDS; tid_num++) {
			tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, (tid_num + 1));
			txpt_paddr = ath12k_get_txpt_paddr(peer, 0, (tid_num + 1));
			ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
			tid_info = &tx_flow_info->tid_info[tid_num];

			sw_msduq_ptr =
				tid_info->msduq[HTT_TID_MSDUQ_UDP];
			if (sw_msduq_ptr) {
				ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
				tid_info->num_of_active_msdu_queues--;
				tid_info->msduq[HTT_TID_MSDUQ_UDP] = NULL;
			}
			sw_msduq_ptr = tid_info->msduq[HTT_TID_MSDUQ_NONUDP];
			if (sw_msduq_ptr) {
				ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
				tid_info->num_of_active_msdu_queues--;
				tid_info->msduq[HTT_TID_MSDUQ_NONUDP] = NULL;
			}
			if (tid_info->num_of_active_msdu_queues)
				continue;
			sw_mpduq_ptr = tid_info->mpduq;
			ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, tid_info);
			tid_info->mpduq = NULL;
		}
	} else {
		tx_tid_ptr = ath12k_get_txpt_info_ptr(peer, 0, 1);
		txpt_paddr = ath12k_get_txpt_paddr(peer, 0, 1);
		ath12k_tx_classify_info_free(dp_hw_grp, &tx_tid_ptr, txpt_paddr);
		ath12k_dp_txpt_update_to_all(peer, tx_tid_ptr);
		tid_info = &tx_flow_info->tid_info[DEFAULT_TID];

		sw_msduq_ptr = tid_info->msduq[HTT_TID_MSDUQ_UDP];
		if (sw_msduq_ptr) {
			ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
			tid_info->num_of_active_msdu_queues--;
			tid_info->msduq[HTT_TID_MSDUQ_UDP] = NULL;
		}
		sw_msduq_ptr = tid_info->msduq[HTT_TID_MSDUQ_NONUDP];
		if (sw_msduq_ptr) {
			ath12k_free_tx_msdu_flowq(dp_hw_grp, sw_msduq_ptr);
			tid_info->num_of_active_msdu_queues--;
			tid_info->msduq[HTT_TID_MSDUQ_NONUDP] = NULL;
		}
		if (tid_info->num_of_active_msdu_queues)
			goto end;

		sw_mpduq_ptr = tid_info->mpduq;
		ath12k_peer_free_tid(dp_hw_grp, sw_mpduq_ptr, tid_info);
		tid_info->mpduq = NULL;
	}
end:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
}

void ath12k_peer_free_static_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				    struct ath12k_dp_peer *peer)
{
	bool is_qos = true;

	if (peer->is_vdev_peer) {
		ath12k_peer_free_mcast_queues(dp_hw_grp, peer);
	} else {
		if (ath12k_dp_peer_get_sta(peer)->wme)
			is_qos = true;
		else
			is_qos = false;

		ath12k_peer_free_default_queues(dp_hw_grp, peer, is_qos);
		ath12k_peer_free_hol_queues(dp_hw_grp, peer);
		ath12k_peer_free_mgmt_queues(dp_hw_grp, peer);
	}
}

void ath12k_dp_peer_free_queues(struct ath12k_dp_hw_group *dp_hw_grp,
				struct ath12k_dp_peer *peer)
{
	if (peer->is_vdev_peer) {
		ath12k_peer_free_mcast_queues(dp_hw_grp, peer);
	} else {
		ath12k_peer_free_data_queues(dp_hw_grp, peer);
		ath12k_peer_free_mgmt_queues(dp_hw_grp, peer);
	}
}
