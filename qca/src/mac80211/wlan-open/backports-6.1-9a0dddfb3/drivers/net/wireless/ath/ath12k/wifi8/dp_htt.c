// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../peer.h"
#include "../htc.h"
#include "../dp_htt.h"
#include "dp_htt.h"
#include "dp_tx_flow_info.h"
#include "dp_peer.h"
#include "dp.h"

#define ATH12K_HTT_RETRY_TIME_MS	100
#define MAX_RETRY_COUNT			50

static void ath12k_dp_htt_add_to_retry_list(struct ath12k_dp_hw_group *dp_hw_grp,
					    u16 peer_id,
					    u8 hw_link_id)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_dp_htt_cmd_retry_info *retry_info = NULL;

	if (hw_link_id >= ATH12K_GROUP_MAX_RADIO) {
		ath12k_err(NULL, "unable to add htt retry entry link_id %d", hw_link_id);
		return;
	}
	if (peer_id >= ATH12K_MAX_PEER_ID) {
		ath12k_err(NULL, "unable to add htt retry entry peer_id %d", peer_id);
		return;
	}

	spin_lock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);
	retry_info = &dp_hw_grp_wifi8->retry_info[hw_link_id];
	set_bit(peer_id, retry_info->htt_retry_peer_id_map);
	retry_info->retry_count++;
	spin_unlock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);

	if (atomic_read(&dp_hw_grp_wifi8->retry_work_active))
		schedule_delayed_work(&dp_hw_grp_wifi8->dp_htt_retry_dwork,
				      msecs_to_jiffies(ATH12K_HTT_RETRY_TIME_MS));
}

static void ath12k_dp_htt_reset_retry_list(struct ath12k_dp_hw_group *dp_hw_grp,
					   u16 peer_id,
					   u8 hw_link_id)
{	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_dp_htt_cmd_retry_info *retry_info = NULL;

	spin_lock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);
	retry_info = &dp_hw_grp_wifi8->retry_info[hw_link_id];
	clear_bit(peer_id, retry_info->htt_retry_peer_id_map);
	retry_info->retry_count = 0;
	spin_unlock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);
}

void ath12k_dp_tx_htt_retry_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
				container_of(dwork,
					     struct ath12k_dp_hw_group_wifi8,
					     dp_htt_retry_dwork);
	struct ath12k_dp_hw_group *dp_hw_grp = ath12k_get_dp_hw_group(dp_hw_grp_wifi8);
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_dp_hw *dp_hw = NULL;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_htt_cmd_retry_info *retry_info = NULL;
	u8 hw_link_id = 0;
	unsigned long peer_id;
	unsigned long pending_peers[BITS_TO_LONGS(ATH12K_MAX_PEER_ID)];

	rcu_read_lock();
	for (hw_link_id = 0; hw_link_id < ATH12K_GROUP_MAX_RADIO; hw_link_id++) {
		dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
		if (unlikely(!dp_pdev))
			continue;

		dp_hw = dp_pdev->dp_hw;
		if (!dp_hw)
			continue;

		spin_lock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);
		retry_info = &dp_hw_grp_wifi8->retry_info[hw_link_id];

		if (retry_info->retry_count >= MAX_RETRY_COUNT) {
			spin_unlock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);
			ath12k_err(NULL, "HTT retry max count reached hw_link_id %d\n",
				   hw_link_id);
			WARN_ON_ONCE(1);
			break;
		}

		bitmap_copy(pending_peers, retry_info->htt_retry_peer_id_map,
			    ATH12K_MAX_PEER_ID);
		spin_unlock_bh(&dp_hw_grp_wifi8->htt_cmd_retry_lock);

		/* iterate over all the required peers */
		for (peer_id = find_first_bit(pending_peers, ATH12K_MAX_PEER_ID);
		     peer_id < ATH12K_MAX_PEER_ID;
		     peer_id = find_next_bit(pending_peers, ATH12K_MAX_PEER_ID,
					     peer_id + 1)) {
			dp_peer = rcu_dereference(dp_pdev->dp_hw->dp_peer_list[peer_id]);
			if (!dp_peer)
				continue;

			if (dp_peer->dp_peer_state >= ATH12K_DP_PEER_LOGICALLY_DELETED)
				continue;

			if (dp_peer->is_vdev_peer)
				(void)ath12k_dp_tx_mcast_msduq_mpduq_setup(dp_hw_grp,
									   dp_peer);
			else
				(void)ath12k_dp_tx_peer_msduq_mpduq_setup(dp_hw_grp,
									  dp_peer,
									  hw_link_id);
		}
	}
	rcu_read_unlock();
}

static int ath12k_dp_tx_htt_msduq_mpduq_setup(struct ath12k_base *ab,
					      struct list_head *mpduq_list_head,
					      struct list_head *msduq_list_head,
					      u8 num_mpduq,
					      u8 num_msduq,
					      u8 hw_link_id)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_mpduq_and_msduq_info_hdr *cmd;
	struct htt_mpduq_or_msduq_info *txq_cmd;
	struct ath12k_dp_mpdu_q_info *mpduq, *mpduq_temp;
	struct ath12k_dp_msdu_q_info *msduq, *msduq_temp;
	int hdr_size = sizeof(struct htt_mpduq_and_msduq_info_hdr);
	int q_info_size = sizeof(struct htt_mpduq_or_msduq_info);
	dma_addr_t pn_addr;
	struct sk_buff *skb = NULL;
	int total_len;
	int ret;

	if (!num_mpduq && !num_msduq)
		return -EINVAL;

	total_len = hdr_size +
		((num_mpduq + num_msduq) * q_info_size);
	if (total_len > ATH12K_MAX_DP_HTT_MSG_LEN)
		return -EINVAL;

	skb = ath12k_htc_alloc_skb(ab, total_len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, total_len);

	memset(skb->data, 0x0, total_len);
	/* encode the header */
	cmd = (struct htt_mpduq_and_msduq_info_hdr *)skb->data;
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO_HDR,
				      HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_MSG_TYPE) |
		     le32_encode_bits(total_len - hdr_size,
				      HTT_MPDUQ_AND_MSDUQ_INFO_HDR_INFO0_PAYLOAD_SIZE);

	/* encode mpduq info */
	txq_cmd = (struct htt_mpduq_or_msduq_info *)((u8 *)cmd + hdr_size);
	list_for_each_entry_safe(mpduq, mpduq_temp, mpduq_list_head, list) {
		pn_addr = ath12k_dp_get_page_paddr(dp->dp_hw_grp,
						   mpduq->flow_info.peer_id);

		txq_cmd->info0 =
			le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_MSG_TYPE) |
			le32_encode_bits(mpduq->flow_info.flow_type,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_Q_TYPE) |
			le32_encode_bits(hw_link_id,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_HW_LINK_ID);
		txq_cmd->mpduq.info1 =
			le32_encode_bits((mpduq->mpdu_q_paddr) >> 8,
					 HTT_MPDUQ_INFO_CMD_INFO1_MPDUQ_ADDR);
		txq_cmd->mpduq.info2 =
			le32_encode_bits(mpduq->queue_number,
					 HTT_MPDUQ_INFO_CMD_INFO2_MPDUQ_NUM) |
			le32_encode_bits(((u64)pn_addr) >> HAL_ADDR_MSB_REG_SHIFT,
					 HTT_MPDUQ_INFO_CMD_INFO2_PN_ADDR_39_32);
		txq_cmd->mpduq.info3 =
			le32_encode_bits(pn_addr, HTT_MPDUQ_INFO_CMD_INFO3_PN_ADDR_31_0);

		if (mpduq->mpduq_sam_id != HAL_SAM_INVALID_MPDUQ_ID) {
			txq_cmd->mpduq.info4 =
			le32_encode_bits(mpduq->mpduq_sam_id,
					 HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_ID) |
			le32_encode_bits(1, HTT_MPDUQ_INFO_CMD_INFO4_SAM_MPDUQ_ALLOCATED);
		}
		txq_cmd++;
	}

	/* encode msduq info */
	list_for_each_entry_safe(msduq, msduq_temp, msduq_list_head, list) {
		txq_cmd->info0 =
			le32_encode_bits(HTT_H2T_MSG_TYPE_MPDUQ_AND_MSDUQ_INFO,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_MSG_TYPE) |
			le32_encode_bits(msduq->flow_info.flow_type,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_Q_TYPE) |
			le32_encode_bits(hw_link_id,
					 HTT_MPDUQ_AND_MSDUQ_INFO_CMD_INFO0_HW_LINK_ID);
		txq_cmd->msduq.info1 =
			le32_encode_bits(msduq->queue_number,
					 HTT_MSDUQ_INFO_CMD_INFO1_MSDUQ_NUM) |
			le32_encode_bits(msduq->svc_id,
					 HTT_MSDUQ_INFO_CMD_INFO1_SVC_CLASS_ID);
		txq_cmd->msduq.info2 =
			le32_encode_bits((msduq->msdu_q_paddr) >> 8,
					 HTT_MSDUQ_INFO_CMD_INFO2_MSDUQ_ADDR);

		txq_cmd->msduq.info3 =
		le32_encode_bits(msduq->svc_id != INVALID_SVC_ID,
				 HTT_MSDUQ_INFO_CMD_INFO3_SVC_INST_REQ_TYPE_VALID);
		txq_cmd->msduq.info3 |=
			le32_encode_bits(HTT_SDWF_SVC_INST_CREATE_REQ,
					 HTT_MSDUQ_INFO_CMD_INFO3_SVC_INST_REQ_TYPE);

		if (msduq->msduq_sam_id != HAL_SAM_INVALID_MSDUQ_ID) {
			txq_cmd->msduq.info3 =
			le32_encode_bits(msduq->msduq_sam_id,
					 HTT_MSDUQ_INFO_CMD_INFO3_SAM_MSDUQ_ID) |
			le32_encode_bits(1, HTT_MSDUQ_INFO_CMD_INFO3_SAM_MSDUQ_ALLOCATED);
		}
		if (msduq->txop_intent) {
			txq_cmd->msduq.info3 |=
				le32_encode_bits(1,
						 HTT_MPDUQ_INFO_CMD_INFO3_TXOP_INTENT);
			txq_cmd->msduq.info3 |=
				le32_encode_bits(msduq->qm_id,
						 HTT_MPDUQ_INFO_CMD_INFO3_QMID);
		}
		txq_cmd++;
	}

	ret = ath12k_htc_send(&ab->htc, dp->msdu_eid, skb);
	if (ret) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_dp_tx_htt_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					    struct ath12k_dp_peer *dp_peer,
					    struct list_head *mpduq_pending_list_head,
					    struct list_head *msduq_pending_list_head,
					    u8 link_id,
					    bool is_mcast_queues)
{
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_dp_mpdu_q_info *mpduq, *mpduq_temp;
	struct ath12k_dp_msdu_q_info *msduq, *msduq_temp;
	struct list_head mpduq_list_head;
	struct list_head msduq_list_head;
	int q_info_size = sizeof(struct htt_mpduq_or_msduq_info);
	int total_len;
	u8 num_mpduq = 0;
	u8 num_msduq = 0;
	int i;
	int ret;
	u8 hw_link_id;
	struct ath12k_dp *dp;
	bool retry = false;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_list_head);
	INIT_LIST_HEAD(&msduq_list_head);

init_tx_queues:
	num_mpduq = 0;
	num_msduq = 0;
	total_len = 0;

	list_for_each_entry_safe(mpduq, mpduq_temp, mpduq_pending_list_head, list) {
		/* truncate the list once we reach the limit */
		if ((total_len + q_info_size) >
		    (ATH12K_MAX_DP_HTT_MSG_LEN - sizeof(struct ath12k_htc_hdr))) {
			break;
		}
		list_move_tail(&mpduq->list, &mpduq_list_head);
		num_mpduq++;
		total_len += q_info_size;
	}

	list_for_each_entry_safe(msduq, msduq_temp, msduq_pending_list_head, list) {
		/* truncate the list once we reach the limit */
		if ((total_len + q_info_size) >
		    (ATH12K_MAX_DP_HTT_MSG_LEN - sizeof(struct ath12k_htc_hdr))) {
			break;
		}
		list_move_tail(&msduq->list, &msduq_list_head);
		num_msduq++;
		total_len += q_info_size;
	}

	if (!num_mpduq && !num_msduq)
		return 0;

	rcu_read_lock();

	if (link_id >= ATH12K_GROUP_MAX_RADIO) {
		/* send the message to all the required socs */
		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			dp =
			ath12k_dp_hw_grp_to_dp(dp_hw_grp,
					       dp_hw_grp->hw_links[hw_link_id].device_id);
			if (!dp)
				continue;

			if (!is_mcast_queues) {
				/* only after assoc done we are allowed send the htt */
				if (!link_peer->assoc_success) {
					ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER,
							 ATH12K_DBG_L2,
							 "smd htt-setup: skip link %u for %pM peer_id=%u — assoc_success=false\n",
							 hw_link_id, dp_peer->addr,
							 dp_peer->peer_id);
					continue;
				}
			}
			set_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);
			ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
					 "smd htt-setup: sending HTT for %pM peer_id=%u hw_link=%u num_mpduq=%u num_msduq=%u txq_links=0x%lx\n",
					 dp_peer->addr, dp_peer->peer_id, hw_link_id,
					 num_mpduq, num_msduq,
					 tx_info->txq_hw_links_bitmap);
			ret = ath12k_dp_tx_htt_msduq_mpduq_setup(dp->ab,
								 &mpduq_list_head,
								 &msduq_list_head,
								 num_mpduq,
								 num_msduq,
								 hw_link_id);
			if (ret) {
				retry = true;
				ath12k_dp_htt_add_to_retry_list(dp_hw_grp,
								dp_peer->peer_id,
								hw_link_id);
				continue;
			}
			ath12k_dp_htt_reset_retry_list(dp_hw_grp,
						       dp_peer->peer_id,
						       hw_link_id);
		}

		if (retry) {
			rcu_read_unlock();
			goto err_release;
		}
	} else {
		/* send the message to specific soc */
		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (hw_link_id != link_id)
				continue;

			dp =
			ath12k_dp_hw_grp_to_dp(dp_hw_grp,
					       dp_hw_grp->hw_links[hw_link_id].device_id);
			if (!dp)
				continue;

			if (!is_mcast_queues) {
				/* only after assoc done we are allowed send the htt */
				if (!link_peer->assoc_success) {
					ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER,
							 ATH12K_DBG_L2,
							 "smd htt-setup: skip link %u for %pM peer_id=%u — assoc_success=false\n",
							 hw_link_id,
							 dp_peer->addr,
							 dp_peer->peer_id);
					continue;
				}
			}
			ath12k_dbg_level(dp->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
					 "smd htt-setup: sending HTT for %pM peer_id=%u hw_link=%u num_mpduq=%u num_msduq=%u txq_links=0x%lx\n",
					 dp_peer->addr, dp_peer->peer_id, hw_link_id,
					 num_mpduq, num_msduq,
					 tx_info->txq_hw_links_bitmap);
			ret = ath12k_dp_tx_htt_msduq_mpduq_setup(dp->ab,
								 &mpduq_list_head,
								 &msduq_list_head,
								 num_mpduq,
								 num_msduq,
								 hw_link_id);
			if (ret) {
				ath12k_dp_htt_add_to_retry_list(dp_hw_grp,
								dp_peer->peer_id,
								hw_link_id);
				rcu_read_unlock();
				goto err_release;
			}
			set_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);
			ath12k_dp_htt_reset_retry_list(dp_hw_grp,
						       dp_peer->peer_id,
						       hw_link_id);
			break;
		}
	}
	rcu_read_unlock();

	if (tx_info->txq_hw_links_bitmap == tx_info->assoc_hw_links_bitmap) {
		/* change the state to init done */
		list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list) {
			mpduq->mpduq_state = ATH12K_TX_Q_INIT_DONE;
			list_del(&mpduq->list);
		}

		list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list) {
			msduq->msduq_state = ATH12K_TX_Q_INIT_DONE;
			list_del(&msduq->list);
		}
	} else {
		list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list)
			list_del(&mpduq->list);

		list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list)
			list_del(&msduq->list);
	}

	/* init for pending queues */
	if (!list_empty(mpduq_pending_list_head) ||
	    !list_empty(msduq_pending_list_head)) {
		goto init_tx_queues;
	}

	return 0;
err_release:
	list_for_each_entry_safe(mpduq, mpduq_temp, &mpduq_list_head, list)
		list_del(&mpduq->list);

	list_for_each_entry_safe(msduq, msduq_temp, &msduq_list_head, list)
		list_del(&msduq->list);

	return 0;
}

int ath12k_dp_rx_htt_ast_info_setup(struct ath12k_base *ab,
				    struct ath12k_hal_ast_param *ast_param)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ast_info_t *cmd;
	int len = sizeof(*cmd);
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_ast_info_t *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_AST_INFO,
				      HTT_AST_INFO0_MSG_TYPE) |
		     le32_encode_bits(ast_param->num_ast_entries,
				      HTT_AST_INFO0_TABLE_SIZE) |
		     le32_encode_bits(ast_param->skid_len,
				      HTT_AST_INFO0_MAX_SEARCH);
	cmd->info1 = le32_encode_bits(ast_param->paddr,
				      HTT_AST_INFO1_BASE_ADDR_31_0);
	cmd->info2 = le32_encode_bits(ast_param->ase_hash_key1,
				      HTT_AST_INFO2_HASH_KEY_1);
	cmd->info3 = le32_encode_bits(ast_param->ase_hash_key2,
				      HTT_AST_INFO3_HASH_KEY_2);
	cmd->info4 = le32_encode_bits(ast_param->ase_hash_key3,
				      HTT_AST_INFO4_HASH_KEY_3);
	cmd->info5 = le32_encode_bits(((u64)ast_param->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				      HTT_AST_INFO5_BASE_ADDR_39_32);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_err(ab, "failed to send ast info htt: %d", ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

void ath12k_dp_htt_peer_cleanup_indication(struct ath12k_dp *dp,
					   struct sk_buff *skb)
{
	struct htt_t2h_global_peer_id_unmap *msg =
				(struct htt_t2h_global_peer_id_unmap *)skb->data;
	u16 peer_id;
	u8 hw_link_id;

	peer_id = le32_get_bits(msg->info, HTT_T2H_GLOBAL_PEER_ID_UNMAP_PEER_ID);
	hw_link_id = le32_get_bits(msg->info, HTT_T2H_GLOBAL_PEER_ID_UNMAP_HW_LINK_ID);

	ath12k_dp_peer_cleanup_indication(dp, peer_id, hw_link_id);
}

int ath12k_wifi8_dp_msdu_htt_connect(struct ath12k_dp *dp)
{
	struct ath12k_htc_svc_conn_resp conn_resp = {0};
	struct ath12k_htc_svc_conn_req conn_req = {0};
	int status;

	conn_req.ep_ops.ep_tx_complete = ath12k_dp_htt_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_dp_htt_htc_t2h_msg_handler;

	/* connect to control service */
	conn_req.service_id = ATH12K_HTC_SVC_ID_HTT_DATA4_MSG;

	status = ath12k_htc_connect_service(&dp->ab->htc, &conn_req,
					    &conn_resp);
	if (status)
		return status;

	dp->msdu_eid = conn_resp.eid;

	return 0;
}
