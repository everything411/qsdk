// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include "../core.h"
#include "hal.h"
#include "../debug.h"
#include "../peer.h"
#include "dp_peer.h"
#include "dp.h"
#include "../hw.h"
#include "../dp_rx.h"
#include "../debugfs_htt_stats.h"
#include "../dp_tx.h"
#include "../dp_mon.h"
#include "hal_rx.h"
#include "dp_rx.h"
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../debugfs.h"
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include <ppe_vp_public.h>
#include <ppe_vp_tx.h>
#endif
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/mesh_util.h"
#endif /* CPTCFG_QCN_EXTN */
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_QCN_EXTN */
#endif
#include "../fse.h"

#define ATH12K_DP_RX_FRAGMENT_TIMEOUT_MS (2 * HZ)

extern bool ath12k_debug_critical;

static int ath12k_wifi7_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid);
void ath12k_wifi7_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid);

int ath12k_wifi7_dp_reo_cmd_send(struct ath12k_base *ab,
				 void *data, size_t len,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    struct hal_reo_status *status))
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_rx_reo_cmd *dp_cmd;
	struct hal_srng *cmd_ring;
	int cmd_num;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) ||
	    test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags))
		return -ESHUTDOWN;

	cmd_ring = &ab->hal.srng_list[dp->reo_cmd_ring.ring_id];
	cmd_num = ath12k_wifi7_hal_reo_cmd_send(ab, cmd_ring, type, cmd);

	/* cmd_num should start from 1, during failure return the error code */
	if (cmd_num < 0)
		return cmd_num;

	/* reo cmd ring descriptors has cmd_num starting from 1 */
	if (cmd_num == 0)
		return -EINVAL;

	if (!cb)
		return 0;

	/* Can this be optimized so that we keep the pending command list only
	 * for tid delete command to free up the resource on the command status
	 * indication?
	 */
	dp_cmd = kzalloc(sizeof(*dp_cmd), GFP_ATOMIC);

	if (!dp_cmd)
		return -ENOMEM;

	if (WARN_ON(len > sizeof(dp_cmd->u)))
		return -EINVAL;
	memcpy(&dp_cmd->u, data, len);
	dp_cmd->cmd_num = cmd_num;
	dp_cmd->handler = cb;

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&dp_cmd->list, &dp->reo_cmd_list);
	spin_unlock_bh(&dp->reo_cmd_lock);

	return 0;
}

int ath12k_wifi7_dp_reo_cache_flush(struct ath12k_base *ab,
                                    struct ath12k_dp_rx_tid *rx_tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS |
		HAL_REO_CMD_FLG_FLUSH_FWD_ALL_MPDUS;

	/* For all QoS TIDs (except NON_QOS), the driver allocates a maximum
	 * window size of 1024. In such cases, the driver can issue a single
	 * 1KB descriptor flush command instead of sending multiple 128-byte
	 * flush commands for each QoS TID, improving efficiency.
	 */

	if (rx_tid->tid != HAL_NON_QOS_TID)
		cmd.flag |= HAL_REO_CMD_FLG_FLUSH_QUEUE_1K_DESC;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd, ath12k_dp_reo_cmd_free);

	return ret;
}

void ath12k_wifi7_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id, u16 tid,
					 dma_addr_t paddr)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool ml_peer = false;

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (!dp->reoq_lut.vaddr || !dp->ml_reoq_lut.vaddr) {
		ath12k_warn(ab, "reo qref table is not setup\n");
		return;
	}

	if (peer_id & ATH12K_PEER_ML_ID_VALID) {
		peer_id &= ~ATH12K_PEER_ML_ID_VALID;
		ml_peer = true;
	}

	if (peer_id > DP_MAX_PEER_ID) {
		ath12k_warn(ab, "peer id %d is more than Max peer id\n", peer_id);
		return;
	}

	if (ml_peer)
		qref = (struct ath12k_reo_queue_ref *)dp->ml_reoq_lut.vaddr +
				(peer_id * ab->hal.hal_params->num_tids + tid);
	else
		qref = (struct ath12k_reo_queue_ref *)dp->reoq_lut.vaddr +
				(peer_id * ab->hal.hal_params->num_tids + tid);

	qref->info0 = u32_encode_bits(lower_32_bits(paddr),
				      BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(upper_32_bits(paddr),
				      BUFFER_ADDR_INFO1_ADDR) |
		      u32_encode_bits(tid, DP_REO_QREF_NUM);
	ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);
}

void ath12k_wifi7_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
				     struct hal_reo_status *status)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_rx_tid *rx_tid = ctx, *update_rx_tid;
	struct ath12k_dp_rx_reo_cache_flush_elem *elem, *tmp;
	struct dp_reo_update_rx_queue_elem *qelem, *qtmp;

	if (!status || status->uniform_hdr.cmd_status == HAL_REO_CMD_DRAIN) {
		goto free_desc;
	} else if (status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS) {
		/* Shouldn't happen! Cleanup in case of other failure? */
		ath12k_warn(ab, "failed to delete rx tid %d hw descriptor %d\n",
			    rx_tid->tid, status->uniform_hdr.cmd_status);
		return;
	}

	/* Check if there is any pending rx_queue, if yes then update it */
	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_for_each_entry_safe(qelem, qtmp, &dp->reo_cmd_update_rx_queue_list,
				 list) {
		if (qelem->reo_cmd_update_rx_queue_resend_flag &&
		    qelem->data.active) {
			update_rx_tid = &qelem->data;

			if (ath12k_wifi7_peer_rx_tid_delete_handler(ab, update_rx_tid,
							      qelem->tid)) {
				update_rx_tid->active = true;
				break;
			}
			update_rx_tid->active = false;
			update_rx_tid->vaddr = NULL;
			update_rx_tid->paddr = 0;
			update_rx_tid->size = 0;
			update_rx_tid->pending_desc_size = 0;

			list_del(&qelem->list);
			kfree(qelem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		goto free_desc;

	elem->ts = jiffies;
	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&elem->list, &dp->reo_cmd_cache_flush_list);
	dp->reo_cmd_cache_flush_count++;

	/* Flush and invalidate aged REO desc from HW cache */
	list_for_each_entry_safe(elem, tmp, &dp->reo_cmd_cache_flush_list,
				 list) {
		if (dp->reo_cmd_cache_flush_count > ATH12K_DP_RX_REO_DESC_FREE_THRES ||
		    time_after(jiffies, elem->ts +
			       msecs_to_jiffies(ATH12K_DP_RX_REO_DESC_FREE_TIMEOUT_MS))) {
			/* Unlock the reo_cmd_lock before using ath12k_dp_reo_cmd_send()
			 * within ath12k_wifi7_dp_reo_cache_flush. The reo_cmd_cache_flush_list
			 * is used in only two contexts, one is in this function called
			 * from napi and the other in ath12k_dp_free during core destroy.
			 * Before dp_free, the irqs would be disabled and would wait to
			 * synchronize. Hence there wouldn’t be any race against add or
			 * delete to this list. Hence unlock-lock is safe here.
			 */
			spin_unlock_bh(&dp->reo_cmd_lock);
			if (ath12k_wifi7_dp_reo_cache_flush(dp->ab, &elem->data)) {
				/* In failure case, just update the timestamp
				 * for flush cache elem and continue
				 */
				spin_lock_bh(&dp->reo_cmd_lock);
				elem->ts = jiffies;
				break;
			}
			spin_lock_bh(&dp->reo_cmd_lock);
			list_del(&elem->list);
			dp->reo_cmd_cache_flush_count--;
			kfree(elem);
		}
	}
	spin_unlock_bh(&dp->reo_cmd_lock);

	return;
free_desc:
	rx_tid->active = false;
	ath12k_core_dma_unmap_single(ab->dev, rx_tid->paddr, rx_tid->size,
				     DMA_BIDIRECTIONAL);
	kfree(rx_tid->vaddr);
	rx_tid->vaddr = NULL;
}

static int ath12k_wifi7_peer_rx_tid_delete_handler(struct ath12k_base *ab,
						   struct ath12k_dp_rx_tid *rx_tid,
						   u8 tid)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.upd0 |= HAL_REO_CMD_UPD0_VLD;
	cmd.upd0 |= HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = (tid == HAL_NON_QOS_TID) ?
			      rx_tid->ba_win_sz : DP_BA_WIN_SZ_MAX;
	cmd.upd1 |= HAL_REO_CMD_UPD1_VLD;

	return ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					    HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					    ath12k_wifi7_dp_rx_tid_del_func);
}

void ath12k_wifi7_peer_rx_tid_qref_reset(struct ath12k_base *ab, u16 peer_id, u16 tid)
{
	struct ath12k_reo_queue_ref *qref;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool ml_peer = false;

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (peer_id & ATH12K_PEER_ML_ID_VALID) {
		peer_id &= ~ATH12K_PEER_ML_ID_VALID;
		ml_peer = true;
	}

	if (peer_id > DP_MAX_PEER_ID) {
		ath12k_warn(ab, "peer id %d is more than Max peer id\n", peer_id);
		return;
	}

	if (ml_peer)
		qref = (struct ath12k_reo_queue_ref *)dp->ml_reoq_lut.vaddr +
				(peer_id * ab->hal.hal_params->num_tids + tid);
	else
		qref = (struct ath12k_reo_queue_ref *)dp->reoq_lut.vaddr +
				(peer_id * ab->hal.hal_params->num_tids + tid);

	qref->info0 = u32_encode_bits(0, BUFFER_ADDR_INFO0_ADDR);
	qref->info1 = u32_encode_bits(0, BUFFER_ADDR_INFO1_ADDR);
}

void ath12k_wifi7_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid)
{
	struct ath12k_dp_rx_tid *rx_tid = &peer->dp_peer->rx_tid[tid];
	struct ath12k_dp_rx_tid *temp_rx_tid = NULL;
	struct dp_reo_update_rx_queue_elem *elem, *tmp;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp   = ath12k_ab_to_dp(ab);

	if (!rx_tid->active)
		return;

	elem = kzalloc(sizeof(*elem), GFP_ATOMIC);
	if (!elem)
		return;

	elem->reo_cmd_update_rx_queue_resend_flag = false;
	elem->peer_id = peer->peer_id;
	elem->tid = tid;
	elem->is_ml_peer = peer->mlo ? true : false;
	elem->ml_peer_id = peer->ml_id;

	memcpy(&elem->data, rx_tid, sizeof(*rx_tid));

	spin_lock_bh(&dp->reo_cmd_update_rx_queue_lock);
	list_add_tail(&elem->list, &dp->reo_cmd_update_rx_queue_list);

	list_for_each_entry_safe(elem, tmp, &dp->reo_cmd_update_rx_queue_list,
			list) {
		temp_rx_tid = &elem->data;

		if (ath12k_wifi7_peer_rx_tid_delete_handler(ab, temp_rx_tid, elem->tid)) {
			temp_rx_tid->active = true;
			elem->reo_cmd_update_rx_queue_resend_flag = true;
			break;
		}
		temp_rx_tid->active = false;
		temp_rx_tid->vaddr = NULL;
		temp_rx_tid->paddr = 0;
		temp_rx_tid->size = 0;
		temp_rx_tid->pending_desc_size = 0;

		list_del(&elem->list);
		kfree(elem);
	}
	spin_unlock_bh(&dp->reo_cmd_update_rx_queue_lock);

	rx_tid->active = false;
	ath12k_wifi7_peer_rx_tid_qref_reset(ab,	peer->dp_peer->peer_id, tid);
	ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);
	rx_tid->vaddr = NULL;
	rx_tid->paddr = 0;
	rx_tid->size = 0;
	rx_tid->pending_desc_size = 0;
}

bool ath12k_wifi7_dp_mlo_peer_tid_teardown_ready(struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *link_peer)
{
	return link_peer->primary_link;
}

void  ath12k_wifi7_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					     struct ath12k_dp_rx_tid *rx_tid,
					     u32 cipher, enum set_key_cmd key_cmd)
{
	cmd->flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd->upd0 = HAL_REO_CMD_UPD0_PN |
			HAL_REO_CMD_UPD0_PN_SIZE |
			HAL_REO_CMD_UPD0_PN_VALID |
			HAL_REO_CMD_UPD0_PN_CHECK |
			HAL_REO_CMD_UPD0_SVLD;

	switch (cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		if (key_cmd == SET_KEY) {
			cmd->upd1 |= HAL_REO_CMD_UPD1_PN_CHECK;
			cmd->pn_size = 48;
		}
		break;
	default:
		break;
	}

	cmd->addr_lo = lower_32_bits(rx_tid->paddr);
	cmd->addr_hi = upper_32_bits(rx_tid->paddr);
}

int ath12k_wifi7_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action)
{
	struct hal_wbm_release_ring *desc;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	int ret = 0;

	srng = &ab->hal.srng_list[dp->wbm_desc_rel_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!desc) {
		ret = -ENOBUFS;
		goto exit;
	}

	ath12k_wifi7_hal_rx_msdu_link_desc_set(ab, desc, buf_addr_info, action);

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi7_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	int ret;

	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.upd0 = HAL_REO_CMD_UPD0_BA_WINDOW_SIZE;
	cmd.ba_window_size = ba_win_sz;

	if (update_ssn) {
		cmd.upd0 |= HAL_REO_CMD_UPD0_SSN;
		cmd.upd2 = u32_encode_bits(ssn, HAL_REO_CMD_UPD2_SSN);
	}

	ret = ath12k_wifi7_dp_reo_cmd_send(ar->ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_UPDATE_RX_QUEUE, &cmd,
					   NULL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to update rx tid queue, tid %d (%d)\n",
			    rx_tid->tid, ret);
		return ret;
	}

	rx_tid->ba_win_sz = ba_win_sz;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "rx tid queue update successful for tid: %u, peer_id: %d, peer: %pM, link_id: %u ba_win_sz: %u\n",
			 rx_tid->tid, peer->peer_id, peer->addr,
			 peer->link_id, rx_tid->ba_win_sz);

	return 0;
}

static inline
void ath12k_wifi7_dp_rx_update_ppe_msdu_mark(struct ath12k_base *ab,
					     struct ath12k_dp_peer *peer,
					     struct sk_buff *msdu,
					     struct rx_mpdu_desc_info *rx_mpdu_info,
					     struct hal_rx_desc *rx_desc)
{
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
	u8 egress_macid;
	if (peer->ppe_vp_num <= 0)
		return;

	ab->hw_params->hal_ops->rx_desc_get_fse_info(rx_desc, rx_mpdu_info);
	if (!rx_mpdu_info->flow_idx_timeout &&
	    !rx_mpdu_info->flow_idx_invalid &&
	    rx_mpdu_info->flow_info.flow_metadata &&
	    (rx_mpdu_info->flow_info.flow_metadata &
	    ATH12K_RX_FSE_FLOW_MATCH_USE_PPE)) {
		egress_macid =
			FIELD_GET(ATH12K_DP_RX_FSE_FL_EGRESS_MACID_MASK,
				  rx_mpdu_info->flow_info.flow_metadata);
		msdu->mark =
			u32_encode_bits(ATH12K_FSE_MAGIC_NUM,
					ATH12K_FSE_MAGIC_NUM_MASK) |
			u32_encode_bits(rx_mpdu_info->flow_info.flow_metadata,
					ATH12K_PPE_VP_NUM) |
			u32_encode_bits(egress_macid,
					ATH12K_EGRESS_MACID_MASK);
	}
#endif
}

static void ath12k_get_dot11_hdr_from_rx_desc(struct ath12k_pdev_dp *dp_pdev,
					      struct sk_buff *msdu,
					      struct ieee80211_rx_status *status,
					      enum hal_encrypt_type enctype,
					      bool mesh_ctrl_present,
					      struct hal_rx_desc *rx_desc,
					      bool is_mcbc, u16 qos_ctl)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	size_t hdr_len, crypto_len;
	struct ieee80211_hdr hdr;
	u8 *crypto_hdr;

	ath12k_wifi7_dp_rx_desc_get_dot11_hdr(ab, rx_desc, &hdr);
	hdr_len = ieee80211_hdrlen(hdr.frame_control);

	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		crypto_len = ath12k_dp_rx_crypto_param_len(dp, enctype);
		crypto_hdr = skb_push(msdu, crypto_len);
		ath12k_wifi7_dp_rx_desc_get_crypto_header(ab, rx_desc,
							  crypto_hdr, enctype);
	}

	skb_push(msdu, hdr_len);
	memcpy(msdu->data, &hdr, min(hdr_len, sizeof(hdr)));

	if (is_mcbc)
		status->flag &= ~RX_FLAG_PN_VALIDATED;

	/* Add QOS header */
	if (ieee80211_is_data_qos(hdr.frame_control)) {
		struct ieee80211_hdr *qhdr = (struct ieee80211_hdr *)msdu->data;

		if (mesh_ctrl_present)
			qos_ctl |= IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT;

		/* TODO: Add other QoS ctl fields when required */
		memcpy(ieee80211_get_qos_ctl(qhdr),
		       &qos_ctl, IEEE80211_QOS_CTL_LEN);
	}
}

void ath12k_wifi7_dp_rx_h_undecap_eth(struct ath12k_pdev_dp *dp_pdev,
				      struct sk_buff *msdu,
				      enum hal_encrypt_type enctype,
				      struct ieee80211_rx_status *status,
				      bool mesh_ctrl_present,
				      struct hal_rx_desc *desc,
				      bool is_mcbc, u16 tid)
{
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
	struct ath12k_dp_rx_rfc1042_hdr rfc = {0xaa, 0xaa, 0x03, {0x00, 0x00, 0x00}};

	eth = (struct ethhdr *)msdu->data;
	ether_addr_copy(da, eth->h_dest);
	ether_addr_copy(sa, eth->h_source);
	rfc.snap_type = eth->h_proto;
	skb_pull(msdu, sizeof(*eth));
	memcpy(skb_push(msdu, sizeof(rfc)), &rfc,
			sizeof(rfc));
	ath12k_get_dot11_hdr_from_rx_desc(dp_pdev, msdu, status, enctype,
					  mesh_ctrl_present, desc, is_mcbc, tid);

	/* original 802.11 header has a different DA and in
	 * case of 4addr it may also have different SA
	 */
	hdr = (struct ieee80211_hdr *)msdu->data;
	ether_addr_copy(ieee80211_get_DA(hdr), da);
	ether_addr_copy(ieee80211_get_SA(hdr), sa);
	status->flag &= ~RX_FLAG_8023;
}

#define ETH_P_1905 0x893a
static
int ath12k_wifi7_dp_rx_h_undecap(struct ath12k_pdev_dp *dp_pdev,
				 struct sk_buff *msdu,
				 struct hal_rx_desc *desc,
				 enum hal_encrypt_type enctype,
				 struct ieee80211_rx_status *status,
				 bool decrypted, bool is_4addr_sta,
				 struct rx_msdu_desc_info *rx_msdu_info,
				 struct rx_tlv_info_1 *tlv_info,
				 struct ath12k_dp_peer *peer, u16 peer_id, u16 tid)
{
	struct ethhdr *ehdr;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_desc_data rx_desc_data = {0};
	struct ath12k_dp_peer *dp_peer;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_dp_vif *dp_vif;
#endif /* CPTCFG_QCN_EXTN */
	u8 allow_3addr_mc = false;
	struct ath12k_vif *ahvif;
	u32 pkt_reason;
	u8 is_mcbc, ra_is_mcbc;
	struct ieee80211_vif *vif;

	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, desc, desc);

	switch (tlv_info->decap) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		pkt_reason = ATH_RX_NATIVE_WIFI_PKTS;
		ath12k_wifi7_dp_rx_h_undecap_nwifi(dp_pdev, msdu, enctype, status, desc,
						   tlv_info->mesh_ctrl_present, tid);
		break;
	case DP_RX_DECAP_TYPE_RAW:
		pkt_reason = ATH_RX_RAW_PKTS;
		ath12k_dp_rx_h_undecap_raw(dp_pdev, msdu, desc, enctype, status,
					   decrypted, peer_id);
		break;
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		pkt_reason = ATH_RX_ETH_PKTS;
		ehdr = (struct ethhdr *)msdu->data;

		/* PN for multicast packets are not validate in HW,
		 * so skip 802.3 rx path
		 * Also, fast_rx expects the STA to be authorized, hence
		 * eapol packets are sent in slow path.
		 */
		status->flag |= RX_FLAG_8023;

		is_mcbc = rx_msdu_info->da_is_mcbc;
		ra_is_mcbc = is_mcbc;
		if (peer)
			ra_is_mcbc = is_mcbc && !peer->is_reset_mcbc;

		/* mac80211 allows fast path only for authorized STA */
		if (ehdr->h_proto == cpu_to_be16(ETH_P_PAE) ||
		    enctype == HAL_ENCRYPT_TYPE_TKIP_MIC) {
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, ra_is_mcbc, tid);
			break;
		}

		/* Drop the 3addr da_mcbc packets if allow_3addr_mc is not set
		 * for 4addr sta as it will double the packet for connected clients.
		 * Allow only 3addr IEEE1905 packets which are required for Easymesh.
		 */
#ifdef CPTCFG_QCN_EXTN
		if (peer && peer->vif) {
			ahvif = ath12k_vif_to_ahvif(peer->vif);
			dp_vif = &ahvif->dp_vif;
			allow_3addr_mc = dp_vif->dp_extn.allow_3addr_mc;
		}
#endif

		if (is_4addr_sta && rx_msdu_info->da_is_mcbc &&
		    !rx_msdu_info->to_ds && !allow_3addr_mc &&
		    ehdr->h_proto != cpu_to_be16(ETH_P_1905)) {
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
									      peer_id);
				if (dp_peer) {
					vif = ath12k_dp_peer_get_vif(dp_peer);
					ahvif = ath12k_vif_to_ahvif(vif);
					ath12k_tid_drop_rx_stats(ahvif, rx_desc_data.tid, msdu->len,
								 ATH_RX_3AADR_DUP);
				}
				rcu_read_unlock();
			}
			return -1;
		}

		if (rx_msdu_info->fr_ds && rx_msdu_info->to_ds && peer && !peer->use_4addr) {
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, ra_is_mcbc, tid);
			break;
		}


		/* PN for mcast packets will be validated in mac80211;
		 * remove eth header and add 802.11 header.
		 */
		if (ra_is_mcbc && decrypted)
			ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype,
							 status,
							 tlv_info->mesh_ctrl_present,
							 desc, ra_is_mcbc, tid);
		break;
	case DP_RX_DECAP_TYPE_8023:
		pkt_reason = ATH_RX_8023_PKTS;
		/* Note that decap_format = 2 indicates that the decapped
		 * packet is either Ethernet 2 (DIX)  or 802.3 (uses SNAP/LLC).
		 * So, decap_format = 2 or 3 is all the same.
		 */
		status->flag |= RX_FLAG_8023;
		break;
	}
	dp_pdev->wmm_stats.total_wmm_rx_pkts[dp_pdev->wmm_stats.rx_type]++;

	rcu_read_lock();
	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (dp_peer) {
		ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(dp_peer));
		ahvif->wmm_stats.total_wmm_rx_pkts[ahvif->wmm_stats.rx_type]++;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev)) {
			ath12k_tid_rx_stats(ahvif, rx_desc_data.tid, msdu->len,
					    pkt_reason);
		}

	}
	rcu_read_unlock();
	return 0;
}

static bool ath12k_wifi7_dp_rx_h_rate(struct ath12k_pdev_dp *dp_pdev,
				      struct ieee80211_rx_status *rx_status,
				      struct rx_tlv_info_1 *tlv_info)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ieee80211_supported_band *sband;
	enum rx_msdu_start_pkt_type pkt_type = tlv_info->pkt_type;
	u8 bw = tlv_info->bw, sgi = tlv_info->sgi;
	u8 rate_mcs = tlv_info->rate_mcs, nss = tlv_info->nss;
	bool is_cck;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		if (rx_status->band == NUM_NL80211_BANDS) {
			ath12k_warn(dp->ab, "Received with invalid band");
			return true;
		}
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		sband = &dp_pdev->ar->mac.sbands[rx_status->band];
		rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
								is_cck);
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HT mode %d\n",
				    rate_mcs);
			return true;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in VHT mode %d\n",
				    rate_mcs);
			return true;
		}
		rx_status->nss = nss;
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in HE mode %d\n",
				    rate_mcs);
			return true;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;

		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(dp->ab,
				    "Received with invalid mcs in EHT mode %d\n",
				    rate_mcs);
			return true;
		}

		rx_status->encoding = RX_ENC_EHT;
		rx_status->nss = nss;
		rx_status->eht.gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(sgi);
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	default:
		break;
	}

	return false;
}

bool ath12k_wifi7_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src)
{
	u8 channel_num;
	u32 center_freq, meta_data;
	struct ieee80211_channel *channel;
	struct ath12k *ar = dp_pdev->ar;

	rx_status->freq = 0;
	rx_status->rate_idx = 0;
	rx_status->nss = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->bw = RATE_INFO_BW_20;
	rx_status->enc_flags = 0;

	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	meta_data = tlv_info->freq;
	channel_num = meta_data;
	center_freq = meta_data >> 16;

	rx_status->band = NUM_NL80211_BANDS;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
		rx_status->freq = center_freq;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	}

	if (unlikely(rx_status->band == NUM_NL80211_BANDS ||
		!dp_pdev->hw->wiphy->bands[rx_status->band])) {
		if (err_rel_src == HAL_WBM_REL_SRC_MODULE_REO ||
		    err_rel_src == HAL_WBM_REL_SRC_MODULE_RXDMA) {
			if(ath12k_debug_critical)
				WARN_ON_ONCE(1);
			return true;
		}
		else {
			ath12k_err(ar->ab,
				   "sband is NULL for status band %d channel_num %d center_freq %d pdev_id %d\n",
				   rx_status->band, channel_num, center_freq,
				   ar->pdev_idx);
		}

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rx_status->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		} else {
			ath12k_err(ar->ab, "unable to determine channel, band for rx packet");
		}
		spin_unlock_bh(&ar->data_lock);

		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);
		goto h_rate;
	}

	if (rx_status->band != NL80211_BAND_6GHZ)
		rx_status->freq = ieee80211_channel_to_frequency(channel_num,
								 rx_status->band);

h_rate:
	return ath12k_wifi7_dp_rx_h_rate(dp_pdev, rx_status, tlv_info);
}

u16 ath12k_wifi7_dp_rx_get_peer_id(struct ath12k_base *ab,
				   enum ath12k_peer_metadata_version ver,
				   __le32 peer_metadata)
{
	switch (ver) {
	default:
		ath12k_warn(ab, "Unknown peer metadata version: %d", ver);
		fallthrough;
	case ATH12K_PEER_METADATA_V0:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V0_PEER_ID);
	case ATH12K_PEER_METADATA_V1:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1_PEER_ID);
	case ATH12K_PEER_METADATA_V1A:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1A_PEER_ID);
	case ATH12K_PEER_METADATA_V1B:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1B_PEER_ID);
	}
}

void ath12k_wifi7_dp_adjust_skb(struct hal_rx_spd_data *spd_desc_l,
				struct link_peer_rx_tid_stats *stats,
				int *msdu_idx, u32 hal_rx_desc_sz)
{
	struct sk_buff *msdu = spd_desc_l->msdu;
	struct rx_msdu_desc_info *rx_msdu_info = &spd_desc_l->rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info = &spd_desc_l->rx_mpdu_info;
	int l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;
	int msdu_len = rx_msdu_info->msdu_length;
	int idx;

	if (unlikely(rx_mpdu_info->fragment_flag))
		/* an ieee80211 rx fragmented MPDU is handled in
		 * exception path, only TLV need to pulled from skb
		 */
		skb_pull(msdu, hal_rx_desc_sz);
	else if (unlikely(spd_desc_l->first_sg_frame)) {
		/* create a frag_list for MSDUs which are spread across
		 * multiple buffers/skbs. pulling of TLV header and
		 * setting of length is done in below API.
		 */
		idx = ath12k_wifi7_rx_create_fraglist(spd_desc_l,
						      hal_rx_desc_sz);
		*msdu_idx += idx;
		if (stats) {
			stats->sg_cnt++;
			stats->sg_bytes++;
		}
	} else {
		/* this is the most likely case, a regular MSDU */
		skb_put(msdu, hal_rx_desc_sz + l3_pad_bytes + msdu_len);
		skb_pull(msdu, hal_rx_desc_sz + l3_pad_bytes);
	}
}

static void
ath12k_wifi7_dp_process_reo_rx_packets(struct ath12k_dp *dp,
				       struct napi_struct *napi,
				       struct hal_rx_spd_data *rx_spd,
				       int ring_id, int num_msdus)
{
	struct ieee80211_rx_status rx_status = {0};
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 prev_tlv_info = {0};
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	u8 *rx_tlv_hdr;
	struct sk_buff *msdu;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	struct ath12k_base *partner_ab;
	struct ath12k_dp *partner_dp;
	u8 hw_link_id, pdev_id;
	u8 prev_hw_link_id = 0xff;
	int msdu_idx = 0;
	struct ath12k_hal *hal = dp->hal;
	u32 hal_rx_desc_sz = hal->hal_desc_sz;
	u16 msdu_len;
	u8 l3_pad_bytes;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_pdev_dp *dp_pdev;
	u16 peer_id = 0;
	u16 old_peer_id = 0xffff;
	struct link_peer_rx_tid_stats tid_stats[8] = {0};
	struct link_peer_rx_tid_stats *stats;
	u8 tid = 0;
	u8 active_tid_mask = 0;
	struct ath12k_tid_rx_stats *tid_rx_stats_ring = NULL;
	bool is_delay_enabled = false;

	rcu_read_lock();

	for (msdu_idx = 0; msdu_idx < num_msdus; msdu_idx++) {
		u8 *vaddr;
		struct hal_rx_spd_data *spd_desc_l = &rx_spd[msdu_idx];

		rx_msdu_info = &spd_desc_l->rx_msdu_info;
		rx_mpdu_info = &spd_desc_l->rx_mpdu_info;
		msdu = spd_desc_l->msdu;
		vaddr = spd_desc_l->vaddr;

		prefetch(&vaddr[128]);

		if (likely(msdu_idx + 1 < num_msdus)) {
			struct hal_rx_spd_data *spd_desc_next = &rx_spd[msdu_idx + 1];
			struct sk_buff *next_msdu = spd_desc_next->msdu;

			prefetch(next_msdu);
			prefetchw(&next_msdu->len);
			prefetchw(&next_msdu->protocol);
			prefetchw(&next_msdu->data);
		}

		hw_link_id = ath12k_dp_validate_hw_link_id(spd_desc_l->reo.src_link_id);
		tid = rx_mpdu_info->tid;

		rx_mpdu_info->flow_info.peer_id =
			ath12k_wifi7_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
						       rx_mpdu_info->peer_meta_data);

		peer_id = rx_mpdu_info->flow_info.peer_id;

		if (peer)
			prefetch(&peer->rx_decap_type);

		/*
		 * access dp_pdev object only if there is a miss-match
		 * between old hw_link_id and current hw_link_id
		 * this ensures minimum cache misses
		 */
		if (unlikely(prev_hw_link_id != hw_link_id)) {
			int device_id, pdev_idx;
			struct ath12k_pdev *pdev_active;

			if (peer)
				ath12k_dp_rx_update_stats(dp_pdev, peer,
							  tid_stats,
							  ring_id,
							  prev_hw_link_id,
							  active_tid_mask);
			prev_hw_link_id = hw_link_id;
			memset(tid_stats, 0, sizeof(tid_stats));
			active_tid_mask = 0;

			device_id = hw_links[hw_link_id].device_id;
			pdev_idx = hw_links[hw_link_id].pdev_idx;

			partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
			pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
							      pdev_idx);

			dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
			if (unlikely(!dp_pdev)) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_PDEV_NA,
						      NULL, tid);
				spd_desc_l->msdu = NULL;
				prev_hw_link_id = 0xff;
				continue;
			}
			is_delay_enabled = ath12k_dp_latency_stats_enabled(dp_pdev);

			tid_rx_stats_ring = &dp_pdev->tid_stats.tid_rx[ring_id][0];

			partner_ab = partner_dp->ab;
			pdev_active = partner_ab->pdevs_active[pdev_id];

			if (unlikely(!rcu_dereference(pdev_active))) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_PDEV_NA,
						      NULL, tid);
				spd_desc_l->msdu = NULL;
				prev_hw_link_id = 0xff;
				continue;
			}
		}

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev) &&
			     (ath12k_dp_vow_stats_enabled(dp_pdev) || is_delay_enabled)))
			__net_timestamp(msdu);

		prefetch(&partner_dp->hal);
		/*
		 * access dp_peer object only if there is a miss-match
		 * between old peer_id and current peer_id
		 * this ensures minimum cache misses
		 */
		if (unlikely(old_peer_id != peer_id)) {
			old_peer_id = peer_id;
			if (peer)
				ath12k_dp_rx_update_stats(dp_pdev, peer,
							  tid_stats,
							  ring_id, hw_link_id,
							  active_tid_mask);

			peer = ath12k_dp_peer_find_by_peerid_index(partner_dp,
								   dp_pdev,
								   peer_id);
			if (unlikely(!peer)) {
				ath12k_dp_rx_skb_free(msdu, dp, ring_id,
						      DP_RX_ERR_DROP_INV_PEER,
						      dp_pdev, tid);
				spd_desc_l->msdu = NULL;
				old_peer_id = 0xffff;
				continue;
			}
			memset(tid_stats, 0, sizeof(tid_stats));
			active_tid_mask = 0;
		}

		/* stats should be collected only after the below assignment */
		active_tid_mask |= 1 << tid;
		stats = &tid_stats[tid];

		stats->received_frm_reo_cnt++;
		stats->received_frm_reo_bytes += rx_msdu_info->msdu_length;
		/*
		 * pull the TLV header + padding bytes and set the length of
		 * the skb accordingly. this is needed irrespective of
		 * decap type.
		 */
		msdu_len = rx_msdu_info->msdu_length;
		l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;

		ath12k_wifi7_dp_adjust_skb(spd_desc_l, stats,
					   &msdu_idx, hal_rx_desc_sz);

		rx_tlv_hdr = spd_desc_l->vaddr;

		/* beyond this point RX TLV info could be over-written by
		 * user-specific meta data, hence copy all the nessacary info
		 * ex: flow_valid bit, flow_meta_info, BW, NSS etc...
		 * to scratchpad descriptor.
		 */
		ath12k_wifi7_dp_extract_rx_spd_data(dp_pdev->dp->hal,
						    spd_desc_l,
						    (struct hal_rx_desc *)rx_tlv_hdr);

		{
			struct dp_rx_tag_params tag = {
				.cce_match        = spd_desc_l->cce_match,
				.cce_metadata     = spd_desc_l->cce_metadata,
				.flow_idx_invalid =
				spd_desc_l->rx_mpdu_info.flow_idx_invalid,
				.flow_idx_timeout =
				spd_desc_l->rx_mpdu_info.flow_idx_timeout,
				.flow_metadata    =
				spd_desc_l->rx_mpdu_info.flow_info.flow_metadata,
			};
			if (tag.cce_match && dp_pdev->protocol_tag_active_count)
				dp_rx_update_protocol_tag(dp_pdev, msdu, &tag);
			dp_rx_update_flow_tag(dp_pdev, msdu, &tag);
		}

		if (likely(msdu_idx + 1 < num_msdus)) {
			struct hal_rx_spd_data *spd_desc_next = &rx_spd[msdu_idx + 1];

			vaddr = spd_desc_next->vaddr;
			prefetch(vaddr);
			prefetch(&vaddr[64]);
			prefetch(&vaddr[128]);
		}

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_proto_stats_enabled(dp_pdev) &&
			    peer->rx_decap_type == DP_RX_DECAP_TYPE_ETHERNET2_DIX)
				ath12k_dp_rx_update_protocol_stats(peer,
					hw_link_id, msdu,
					RX_SENT_TO_STACK, ring_id);

			if (ath12k_dp_vow_stats_enabled(dp_pdev)) {
				u8 da_is_mcbc = rx_msdu_info->da_is_mcbc;

				ath12k_dp_rx_update_vow_delay_stats(dp_pdev, stats,
								    msdu,
								    da_is_mcbc,
								    tid,
								    tid_rx_stats_ring);
			}

			if (is_delay_enabled)
				ath12k_dp_rx_update_delay_stats(peer, msdu,
								tid, ring_id);
		}

		if (likely(peer->rx_decap_type ==
			   DP_RX_DECAP_TYPE_ETHERNET2_DIX)) {
			ath12k_wifi7_deliver_ethernet_frame(dp_pdev, spd_desc_l,
							    peer, &rx_status,
							    napi, stats,
							    &prev_tlv_info);
		} else if (peer->rx_decap_type ==
			   DP_RX_DECAP_TYPE_NATIVE_WIFI) {
			ath12k_wifi7_deliver_nwifi_frame(dp_pdev, spd_desc_l,
							 peer, &rx_status,
							 napi, stats,
							 &prev_tlv_info);
		} else if (peer->rx_decap_type == DP_RX_DECAP_TYPE_RAW) {
			ath12k_wifi7_deliver_raw_frame(dp_pdev, spd_desc_l,
						       peer, &rx_status,
						       napi, stats,
						       &prev_tlv_info);
		}

		if (rx_msdu_info->first_msdu & rx_msdu_info->last_msdu)
			stats->non_amsdu++;
		else
			stats->amsdu++;

		if (rx_msdu_info->last_msdu & rx_mpdu_info->mpdu_retry_bit)
			stats->mpdu_retry++;
	}

	if (peer)
		ath12k_dp_rx_update_stats(dp_pdev, peer, tid_stats,
					  ring_id, hw_link_id,
					  active_tid_mask);

	rcu_read_unlock();
}

static bool check_sg_termination(struct hal_srng *srng,
				 int valid_entries)
{
	struct hal_reo_dest_ring *desc;
	struct rx_msdu_desc *msdu_info;

	if (valid_entries >= 9)
		return true;

	if (!valid_entries)
		return false;

	desc = (struct hal_reo_dest_ring *)
		ath12k_hal_srng_fetch_entry(srng,
				valid_entries - 1);
	msdu_info = &desc->rx_msdu_info;
	return !(le32_to_cpu(msdu_info->info0) &
			RX_MSDU_DESC_INFO0_MSDU_CONTINUATION);
}

int ath12k_wifi7_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct hal_srng *srng = NULL;
	struct hal_rx_spd_data *rx_status_desc = NULL;
	struct hal_rx_spd_data *tmp_spd = NULL;
	struct hal_reo_dest_ring *hw_rx_desc = NULL;
	struct hal_reo_dest_ring *next_hw_rx_desc = NULL;
	struct hal_reo_dest_ring *pf_next_hw_rx_desc = NULL;
	u8 device_id;
	u16 total_rx_reaped = 0;
	u16 num_rx_reaped = 0;
	u16 valid_entries = 0;
	u32 curr_tp = 0;
	bool first_sg_frame = true;
	struct ath12k_rx_desc_info *sw_rx_desc = NULL;
	u8 push_rsn = 0;
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	int num_rx_reaped_per_device[ATH12K_MAX_SOCS] = {};
	struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
	struct hal_srng *refill_srng;
	struct ath12k_dp *partner_dp;
	int rx_status_idx = smp_processor_id();

	srng = &dp->hal->srng_list[dp->reo_dst_ring[ring_id].ring_id];

	__ath12k_hal_srng_access_begin(srng);

	valid_entries = __ath12k_hal_srng_dst_num_available_to_reap(srng,
								    false);
	if (unlikely(!valid_entries)) {
		__ath12k_hal_srng_access_end(ab, srng);
		return num_rx_reaped;
	}

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	if (valid_entries > budget)
		valid_entries = budget;

	rx_status_desc =
		(struct hal_rx_spd_data *)dp_hw_grp->rx_status_buf[rx_status_idx];

	ath12k_dp_srng_dst_invalidate_entries(dp, srng, valid_entries);

	/* This loop will reap the HW desc from the reo ring and copy
	 * the contents of the HW desc to scratch-pad (spad) desc.
	 * get the corresponding sw_desc desc and save the skb in
	 * spad desc.
	 */
	while (valid_entries) {
		struct hal_rx_spd_data *rx_spd = &rx_status_desc[num_rx_reaped];

		/* reset all the flags before using scratch_pad desc */
		rx_spd->flags = 0;
		hw_rx_desc = __ath12k_hal_get_dst_srng_desc(srng, &curr_tp,
							    (void **)&next_hw_rx_desc);

		/* this check is redundant */
		if (unlikely(!hw_rx_desc)) {
			pr_err("HW bug: NULL entry in ring, invalid entry");
			WARN_ON(1);
		}

		push_rsn = le32_get_bits(hw_rx_desc->info0,
					 HAL_REO_DEST_RING_INFO0_PUSH_REASON);

		if (unlikely(push_rsn ==
			     HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED)) {
			pr_err("HW bug: err pkts routed on regular rx ring");
			WARN_ON(1);
		}

		sw_rx_desc = ath12k_wifi7_get_sw_desc_from_hw_desc(hw_rx_desc);

		ath12k_wifi7_rx_sw_desc_sanity_check(sw_rx_desc);

		ath12k_wifi7_cpy_hw_rx_desc_to_spad_desc(hw_rx_desc, rx_spd);

		rx_spd->rx_mpdu_info.fragment_flag = sw_rx_desc->is_frag;
		sw_rx_desc->is_frag = 0;

		if (pf_next_hw_rx_desc)
			ath12k_wifi7_pretech_next_sw_desc(pf_next_hw_rx_desc);

		prefetch(next_hw_rx_desc);
		pf_next_hw_rx_desc = next_hw_rx_desc;

		device_id = sw_rx_desc->device_id;
		valid_entries--;
		/* Scatter-gather (SG) frame reap logic
		 * This is a case where an MSDU is spread across
		 * multiple buffer. The continuation bit in
		 * HW descriptor indicates the current MSDU
		 * is spread across multiple buffers
		 */
		if (unlikely(rx_spd->rx_msdu_info.msdu_continuation)) {
			/* if this is the first SG frame and if the
			 * number of valid entries remaining to be reaped
			 * is less than 8 in this NAPI context, update the
			 * curr_tp as ring's tp and break. this mpdu/msdu
			 * will be reaped  in next NAPI poll context.
			 * Note: reason for why at least 8 valid entries are
			 *       needed to reap a SG MPDU and MSDU.
			 *       MAX MPDU size = 11454 buffer size = 1536
			 *       hence an MPDU at best will need 8 buffers
			 *       MAX MSDU size = 2304, buffer size = 1536
			 *       hence an MSDU at best will need 2 buffers.
			 */
			if (first_sg_frame) {
				if (!check_sg_termination(srng, valid_entries)) {
					__ath12k_hal_srng_update_tp(srng,
								    curr_tp);
					break;
				}
				tmp_spd = rx_spd;
				first_sg_frame = false;
				rx_spd->first_sg_frame = true;
			}
		} else { /* non SG frame handling */
			/* if the previous reaped hw desc has continuation bit
			 * set, then the current reaped HW desc is the last SG
			 * frame of the MSDU, set the state accordingly
			 */
			if (unlikely(!first_sg_frame)) {
				rx_spd->last_sg_frame = true;
				first_sg_frame = true;
				tmp_spd->rx_msdu_info.msdu_length =
					rx_spd->rx_msdu_info.msdu_length;
			}
		}

		ath12k_dp_rx_buffer_unmap(dp, sw_rx_desc);
		rx_spd->msdu = sw_rx_desc->skb;
		rx_spd->vaddr = sw_rx_desc->vaddr;
		rx_spd->reo.ring_id = ring_id;

		sw_rx_desc->in_use = 0;
		num_rx_reaped_per_device[device_id]++;
		num_rx_reaped++;
		list_add_tail(&sw_rx_desc->list, &rx_desc_used_list[device_id]);
	}
	ath12k_dsb();

	__ath12k_hal_srng_access_end(ab, srng);

	dp->device_stats.reo_rx[ring_id][dp->device_id] += num_rx_reaped;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_rx_reaped_per_device[device_id])
			continue;

		total_rx_reaped += num_rx_reaped_per_device[device_id];
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;
		refill_srng =
			&partner_dp->ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
		ath12k_dp_rx_bufs_replenish(partner_dp, refill_srng,
					    &rx_desc_used_list[device_id],
					    false);
	}

	ath12k_wifi7_dp_process_reo_rx_packets(dp, napi, rx_status_desc,
					       ring_id, total_rx_reaped);

	return total_rx_reaped;


}

static int ath12k_wifi7_dp_rx_h_verify_tkip_mic(struct ath12k_pdev_dp *dp_pdev,
						struct ath12k_dp_peer *peer,
						enum hal_encrypt_type enctype,
						struct sk_buff *msdu,
						struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_tlv_info_1 tlv_info;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ieee80211_rx_status *rxs = IEEE80211_SKB_RXCB(msdu);
	struct ieee80211_key_conf *key_conf;
	struct ieee80211_hdr *hdr;
	u8 mic[IEEE80211_CCMP_MIC_LEN];
	int head_len, tail_len, ret;
	size_t data_len;
	u32 hdr_len, hal_rx_desc_sz = ab->hal.hal_desc_sz;
	u8 *key, *data;
	u8 key_idx;

	if (enctype != HAL_ENCRYPT_TYPE_TKIP_MIC)
		return 0;

	hdr = (struct ieee80211_hdr *)(msdu->data + hal_rx_desc_sz);
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	head_len = hdr_len + hal_rx_desc_sz + IEEE80211_TKIP_IV_LEN;
	tail_len = IEEE80211_CCMP_MIC_LEN + IEEE80211_TKIP_ICV_LEN + FCS_LEN;

	spin_lock_bh(&peer->keys_lock);

	if (!is_multicast_ether_addr(hdr->addr1))
		key_idx = peer->ucast_keyidx;
	else
		key_idx = peer->mcast_keyidx;

	key_conf = peer->keys[key_idx];

	data = msdu->data + head_len;
	data_len = msdu->len - head_len - tail_len;
	key = &key_conf->key[NL80211_TKIP_DATA_OFFSET_RX_MIC_KEY];

	ret = ath12k_dp_rx_h_michael_mic(peer->tfm_mmic, key, hdr, data,
					 data_len, mic);

	spin_unlock_bh(&peer->keys_lock);

	if (ret || memcmp(mic, data + data_len, IEEE80211_CCMP_MIC_LEN))
		goto mic_fail;

	return 0;

mic_fail:
	(ATH12K_SKB_RXCB(msdu))->is_first_msdu = true;
	(ATH12K_SKB_RXCB(msdu))->is_last_msdu = true;

	rxs->flag |= RX_FLAG_MMIC_ERROR | RX_FLAG_MMIC_STRIPPED |
		    RX_FLAG_IV_STRIPPED | RX_FLAG_DECRYPTED;
	skb_pull(msdu, hal_rx_desc_sz);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu)))
		return -EINVAL;

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;


	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, rxs, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return -EINVAL;

	ret = ath12k_wifi7_dp_rx_h_undecap(dp_pdev, msdu, rx_desc,
					   HAL_ENCRYPT_TYPE_TKIP_MIC, rxs, true, false,
					   &rx_msdu_info, &tlv_info, NULL,
					   ATH12K_SKB_RXCB(msdu)->peer_id,
					   rx_desc_data->tid);
	if (unlikely(ret))
		return -EINVAL;

	ieee80211_rx(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	return -EINVAL;
}

static int ath12k_wifi7_dp_rx_h_defrag(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_peer *peer,
				       struct ath12k_dp_rx_tid *rx_tid,
				       struct sk_buff **defrag_skb,
				       enum hal_encrypt_type enctype,
				       bool decrypted, struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb, *first_frag, *last_frag;
	struct ieee80211_hdr *hdr;
	bool is_decrypted = false;
	int msdu_len = 0;
	int extra_space;
	u32 flags, hal_rx_desc_sz = ab->hal.hal_desc_sz;

	first_frag = skb_peek(&rx_tid->rx_frags);
	last_frag = skb_peek_tail(&rx_tid->rx_frags);
	if (!first_frag || !last_frag)
		return -EINVAL;

	skb_queue_walk(&rx_tid->rx_frags, skb) {
		flags = 0;
		hdr = (struct ieee80211_hdr *)(skb->data + hal_rx_desc_sz);

		if (enctype != HAL_ENCRYPT_TYPE_OPEN)
			is_decrypted = decrypted;

		if (is_decrypted) {
			if (skb != first_frag)
				flags |= RX_FLAG_IV_STRIPPED;
			if (skb != last_frag)
				flags |= RX_FLAG_ICV_STRIPPED |
					 RX_FLAG_MIC_STRIPPED;
		}

		/* RX fragments are always raw packets */
		if (skb != last_frag)
			skb_trim(skb, skb->len - FCS_LEN);
		ath12k_dp_rx_h_undecap_frag(dp_pdev, skb, enctype, flags);

		if (skb != first_frag)
			skb_pull(skb, hal_rx_desc_sz +
				      ieee80211_hdrlen(hdr->frame_control));
		msdu_len += skb->len;
	}

	extra_space = msdu_len - (DP_RX_BUFFER_SIZE + skb_tailroom(first_frag));
	if (extra_space > 0 &&
	    (pskb_expand_head(first_frag, 0, extra_space, GFP_ATOMIC) < 0))
		return -ENOMEM;

	__skb_unlink(first_frag, &rx_tid->rx_frags);
	while ((skb = __skb_dequeue(&rx_tid->rx_frags))) {
		skb_put_data(first_frag, skb->data, skb->len);
		dev_kfree_skb_any(skb);
	}

	hdr = (struct ieee80211_hdr *)(first_frag->data + hal_rx_desc_sz);
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_FCTL_MOREFRAGS);

	if (ath12k_wifi7_dp_rx_h_verify_tkip_mic(dp_pdev, peer, enctype, first_frag,
						 rx_desc_data))
		first_frag = NULL;

	*defrag_skb = first_frag;
	return 0;
}

static int
ath12k_wifi7_dp_rx_h_defrag_reo_reinject(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_rx_tid *rx_tid,
					 struct sk_buff *defrag_skb)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = dp->hal;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)defrag_skb->data;
	struct hal_reo_entrance_ring *reo_ent_ring;
	struct hal_reo_dest_ring *reo_dest_ring;
	struct dp_link_desc_bank *link_desc_banks;
	struct hal_rx_msdu_link *msdu_link;
	struct hal_rx_msdu_details *msdu0;
	struct hal_srng *srng;
	dma_addr_t link_paddr, buf_paddr;
	u32 desc_bank, msdu_info, msdu_ext_info, mpdu_info;
	u32 cookie, hal_rx_desc_sz, dest_ring_info0, queue_addr_hi;
	int ret, len_diff;
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_buffer_addr *info;
	enum hal_rx_buf_return_buf_manager idle_link_rbm = dp->idle_link_rbm;
	const void *end;
	u8 dst_ind;

	hal_rx_desc_sz = hal->hal_desc_sz;
	link_desc_banks = dp->link_desc_banks;
	reo_dest_ring = rx_tid->desc;

	ath12k_wifi7_hal_rx_reo_ent_paddr_get(ab, &reo_dest_ring->buf_addr_info,
					      &link_paddr, &cookie);
	desc_bank = u32_get_bits(cookie, DP_LINK_DESC_BANK_MASK);

	msdu_link = (struct hal_rx_msdu_link *)(link_desc_banks[desc_bank].vaddr +
			(link_paddr - link_desc_banks[desc_bank].paddr));
	msdu0 = &msdu_link->msdu_link[0];
	msdu_ext_info = le32_to_cpu(msdu0->rx_msdu_ext_info.info0);
	dst_ind = u32_get_bits(msdu_ext_info, RX_MSDU_EXT_DESC_INFO0_REO_DEST_IND);

	memset(msdu0, 0, sizeof(*msdu0));

	msdu_info = u32_encode_bits(1, RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU) |
		    u32_encode_bits(0, RX_MSDU_DESC_INFO0_MSDU_CONTINUATION) |
		    u32_encode_bits(defrag_skb->len - hal_rx_desc_sz,
				    RX_MSDU_DESC_INFO0_MSDU_LENGTH) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_VALID_SA) |
		    u32_encode_bits(1, RX_MSDU_DESC_INFO0_VALID_DA);
	msdu0->rx_msdu_info.info0 = cpu_to_le32(msdu_info);
	msdu0->rx_msdu_ext_info.info0 = cpu_to_le32(msdu_ext_info);

	len_diff = defrag_skb->len - hal_rx_desc_sz;
	/* change msdu len in hal rx desc */
	ath12k_wifi7_dp_rxdesc_set_msdu_len(ab, rx_desc, len_diff);

	end = defrag_skb->data + DP_RX_BUFFER_SIZE;
	ath12k_core_dmac_clean_range(defrag_skb->data, end);

	ATH12K_IPA_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr);
	VIRT_TO_PHYS(defrag_skb, buf_paddr);
	IPA_SET_RX_BUF_SMMU_MAP(ab, defrag_skb);

	if (!buf_paddr)
		return -ENOMEM;

	spin_lock_bh(&dp->rx_desc_lock);
	desc_info = list_first_entry_or_null(&dp->rx_desc_free_list,
					     struct ath12k_rx_desc_info,
					     list);
	if (!desc_info) {
		spin_unlock_bh(&dp->rx_desc_lock);
		ath12k_warn(ab, "failed to find rx desc for reinject\n");
		ret = -ENOMEM;
		goto err_unmap_dma;
	}

	desc_info->skb = defrag_skb;
	desc_info->in_use = true;
	desc_info->paddr = buf_paddr;
	desc_info->vaddr = defrag_skb->data;
	desc_info->is_frag = 1;

	list_del(&desc_info->list);
	spin_unlock_bh(&dp->rx_desc_lock);

	info = (struct ath12k_buffer_addr *)&msdu0->buf_addr_info;
	ath12k_hal_rx_buf_addr_info_set(info, buf_paddr,
				       desc_info->cookie,
				       HAL_RX_BUF_RBM_SW5_BM);

	/* Fill mpdu details into reo entrance ring */
	srng = &hal->srng_list[dp->reo_reinject_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	reo_ent_ring = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!reo_ent_ring) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		ret = -ENOSPC;
		goto err_free_desc;
	}
	memset(reo_ent_ring, 0, sizeof(*reo_ent_ring));

	info = (struct ath12k_buffer_addr *)&reo_ent_ring->buf_addr_info;
	ath12k_hal_rx_buf_addr_info_set(info, link_paddr, cookie, idle_link_rbm);

	mpdu_info = u32_encode_bits(1, RX_MPDU_DESC_INFO0_MSDU_COUNT) |
		    u32_encode_bits(0, RX_MPDU_DESC_INFO0_FRAG_FLAG) |
		    u32_encode_bits(1, RX_MPDU_DESC_INFO0_RAW_MPDU) |
		    u32_encode_bits(1, RX_MPDU_DESC_INFO0_VALID_PN) |
		    u32_encode_bits(rx_tid->tid, RX_MPDU_DESC_INFO0_TID);

	reo_ent_ring->rx_mpdu_info.info0 = cpu_to_le32(mpdu_info);
	reo_ent_ring->rx_mpdu_info.peer_meta_data =
		reo_dest_ring->rx_mpdu_info.peer_meta_data;

	if (ab->hw_params->reoq_lut_support) {
		reo_ent_ring->queue_addr_lo = reo_dest_ring->rx_mpdu_info.peer_meta_data;
		queue_addr_hi = 0;
	} else {
		reo_ent_ring->queue_addr_lo = cpu_to_le32(lower_32_bits(rx_tid->paddr));
		queue_addr_hi = upper_32_bits(rx_tid->paddr);
	}

	reo_ent_ring->info0 = le32_encode_bits(queue_addr_hi,
					       HAL_REO_ENTR_RING_INFO0_QUEUE_ADDR_HI) |
			      le32_encode_bits(dst_ind,
					       HAL_REO_ENTR_RING_INFO0_DEST_IND);

	reo_ent_ring->info1 = le32_encode_bits(rx_tid->cur_sn,
					       HAL_REO_ENTR_RING_INFO1_MPDU_SEQ_NUM);
	dest_ring_info0 = le32_get_bits(reo_dest_ring->info0,
					HAL_REO_DEST_RING_INFO0_SRC_LINK_ID);
	reo_ent_ring->info2 =
		cpu_to_le32(u32_get_bits(dest_ring_info0,
					 HAL_REO_ENTR_RING_INFO2_SRC_LINK_ID));

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return 0;

err_free_desc:
	spin_lock_bh(&dp->rx_desc_lock);
	desc_info->in_use = false;
	desc_info->skb = NULL;
	list_add_tail(&desc_info->list, &dp->rx_desc_free_list);
	spin_unlock_bh(&dp->rx_desc_lock);
err_unmap_dma:
	IPA_SET_RX_BUF_SMMU_UNMAP(ab, defrag_skb, false);
	ATH12K_DMA_UNMAP_SINGLE(ab->dev, buf_paddr, DP_RX_BUFFER_SIZE,
				DMA_FROM_DEVICE);
	return ret;
}

static int ath12k_wifi7_dp_rx_h_cmp_frags(struct ath12k_base *ab,
					  struct sk_buff *a, struct sk_buff *b)
{
	int frag1, frag2;

	frag1 = ath12k_wifi7_dp_rx_h_frag_no(ab, a);
	frag2 = ath12k_wifi7_dp_rx_h_frag_no(ab, b);

	return frag1 - frag2;
}

static void ath12k_wifi7_dp_rx_h_sort_frags(struct ath12k_base *ab,
					    struct sk_buff_head *frag_list,
					    struct sk_buff *cur_frag)
{
	struct sk_buff *skb;
	int cmp;

	skb_queue_walk(frag_list, skb) {
		cmp = ath12k_wifi7_dp_rx_h_cmp_frags(ab, skb, cur_frag);
		if (cmp < 0)
			continue;
		__skb_queue_before(frag_list, skb, cur_frag);
		return;
	}
	__skb_queue_tail(frag_list, cur_frag);
}

static u64 ath12k_wifi7_dp_rx_h_get_pn(struct ath12k_dp *dp, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;
	u64 pn = 0;
	u8 *ehdr;
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;

	hdr = (struct ieee80211_hdr *)(skb->data + hal_rx_desc_sz);
	ehdr = skb->data + hal_rx_desc_sz + ieee80211_hdrlen(hdr->frame_control);

	pn = ehdr[0];
	pn |= (u64)ehdr[1] << 8;
	pn |= (u64)ehdr[4] << 16;
	pn |= (u64)ehdr[5] << 24;
	pn |= (u64)ehdr[6] << 32;
	pn |= (u64)ehdr[7] << 40;

	return pn;
}

static bool
ath12k_wifi7_dp_rx_h_defrag_validate_incr_pn(struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_dp_rx_tid *rx_tid,
					     enum hal_encrypt_type encrypt_type)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct sk_buff *first_frag, *skb;
	u64 last_pn;
	u64 cur_pn;

	first_frag = skb_peek(&rx_tid->rx_frags);
	if (!first_frag)
		return false;

	if (encrypt_type != HAL_ENCRYPT_TYPE_CCMP_128 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_CCMP_256 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_GCMP_128 &&
	    encrypt_type != HAL_ENCRYPT_TYPE_AES_GCMP_256)
		return true;

	last_pn = ath12k_wifi7_dp_rx_h_get_pn(dp, first_frag);
	skb_queue_walk(&rx_tid->rx_frags, skb) {
		if (skb == first_frag)
			continue;

		cur_pn = ath12k_wifi7_dp_rx_h_get_pn(dp, skb);
		if (cur_pn != last_pn + 1)
			return false;
		last_pn = cur_pn;
	}
	return true;
}

static int ath12k_wifi7_dp_rx_frag_h_mpdu(struct ath12k_pdev_dp *dp_pdev,
					  struct sk_buff *msdu,
					  struct hal_reo_dest_ring *ring_desc,
					  struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_peer *peer;
	struct ath12k_dp_rx_tid *rx_tid;
	struct sk_buff *defrag_skb = NULL;
	u32 peer_id;
	u16 seqno, frag_no;
	u8 tid = rx_desc_data->tid;
	int ret = 0;
	bool more_frags;
	enum hal_encrypt_type enctype;

	peer_id = le32_get_bits(ring_desc->rx_mpdu_info.peer_meta_data,
				RX_MPDU_DESC_META_DATA_V1_PEER_ID);
	frag_no = ath12k_wifi7_dp_rx_h_frag_no(ab, msdu);
	more_frags = ath12k_wifi7_dp_rx_h_more_frags(ab, msdu);
	seqno = rx_desc_data->seq_no;

	if (!rx_desc_data->seq_ctl_valid || !rx_desc_data->fc_valid ||
	    tid >= ab->hal.hal_params->num_tids)
		return -EINVAL;

	/* received unfragmented packet in reo
	 * exception ring, this shouldn't happen
	 * as these packets typically come from
	 * reo2sw srngs.
	 */
	if (WARN_ON_ONCE(!frag_no && !more_frags))
		return -EINVAL;

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_warn(ab, "failed to find the peer to de-fragment received fragment peer_id %d\n",
			    peer_id);
		ret = -ENOENT;
		goto out_unlock;
	}

	if (rx_desc_data->is_mcbc)
		enctype = peer->sec_type_grp;
	else
		enctype = peer->sec_type;

	if (!peer->primary_link_frag_setup) {
		ath12k_warn(ab, "The peer %pM [%d] has uninitialized datapath\n",
			    peer->addr, peer_id);
		ret = -ENOENT;
		goto out_unlock;
	}

	rx_tid = &peer->rx_tid[tid];

	spin_lock_bh(&rx_tid->tid_lock);

	if ((!skb_queue_empty(&rx_tid->rx_frags) && seqno != rx_tid->cur_sn) ||
	    skb_queue_empty(&rx_tid->rx_frags)) {
		/* Flush stored fragments and start a new sequence */
		ath12k_dp_rx_frags_cleanup(rx_tid, true);
		rx_tid->cur_sn = seqno;
	}

	if (rx_tid->rx_frag_bitmap & BIT(frag_no)) {
		/* Fragment already present */
		ret = -EINVAL;
		spin_unlock_bh(&rx_tid->tid_lock);
		goto out_unlock;
	}

	if ((!rx_tid->rx_frag_bitmap || frag_no > __fls(rx_tid->rx_frag_bitmap)))
		__skb_queue_tail(&rx_tid->rx_frags, msdu);
	else
		ath12k_wifi7_dp_rx_h_sort_frags(ab, &rx_tid->rx_frags, msdu);

	rx_tid->rx_frag_bitmap |= BIT(frag_no);
	if (!more_frags)
		rx_tid->last_frag_no = frag_no;

	if (frag_no == 0) {
		rx_tid->desc = kmemdup(ring_desc,
				       sizeof(struct hal_reo_dest_ring),
				       GFP_ATOMIC);
		if (!rx_tid->desc) {
			ret = -ENOMEM;
			spin_unlock_bh(&rx_tid->tid_lock);
			goto out_unlock;
		}
	} else {
		ath12k_wifi7_dp_rx_link_desc_return(dp, &ring_desc->buf_addr_info,
						    HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);
	}

	if (!rx_tid->last_frag_no ||
	    rx_tid->rx_frag_bitmap != GENMASK(rx_tid->last_frag_no, 0)) {
		mod_timer(&rx_tid->frag_timer, jiffies +
					       ATH12K_DP_RX_FRAGMENT_TIMEOUT_MS);
		spin_unlock_bh(&rx_tid->tid_lock);
		goto out_unlock;
	}

	spin_unlock_bh(&rx_tid->tid_lock);

	del_timer_sync(&rx_tid->frag_timer);

	spin_lock_bh(&rx_tid->tid_lock);

	if (!ath12k_wifi7_dp_rx_h_defrag_validate_incr_pn(dp_pdev, rx_tid, enctype))
		goto err_frags_cleanup;

	if (ath12k_wifi7_dp_rx_h_defrag(dp_pdev, peer, rx_tid, &defrag_skb,
					enctype, rx_desc_data->is_decrypted,
					rx_desc_data))
		goto err_frags_cleanup;

	if (!defrag_skb)
		goto err_frags_cleanup;

	if (ath12k_wifi7_dp_rx_h_defrag_reo_reinject(dp, dp_pdev, rx_tid, defrag_skb))
	{
		IPA_SET_RX_BUF_SMMU_UNMAP(ab, defrag_skb, true);
		goto err_frags_cleanup;
	}

	ath12k_dp_rx_frags_cleanup(rx_tid, false);

	spin_unlock_bh(&rx_tid->tid_lock);
	goto out_unlock;

err_frags_cleanup:
	dev_kfree_skb_any(defrag_skb);
	ath12k_dp_rx_frags_cleanup(rx_tid, true);
	spin_unlock_bh(&rx_tid->tid_lock);
out_unlock:
	spin_unlock_bh(&dp->dp_lock);
	return ret;
}

static int
ath12k_wifi7_dp_process_rx_err_buf(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_reo_dest_ring *desc,
				   struct list_head *used_list,
				   bool drop, u32 cookie)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_desc_data rx_desc_data = {0};
	struct hal_rx_desc *rx_desc;
	struct ath12k_skb_rxcb *rxcb;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_peer *peer;
	struct sk_buff *msdu;
	u16 msdu_len;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;
	struct ath12k_rx_desc_info *desc_info;
	u64 desc_va;

	desc_va = ((u64)le32_to_cpu(desc->buf_va_hi) << 32 |
		   le32_to_cpu(desc->buf_va_lo));
	desc_info = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	/* retry manual desc retrieval */
	if (!desc_info) {
		desc_info = ath12k_dp_get_rx_desc(dp, cookie);
		if (!desc_info) {
			ath12k_warn(ab, "Invalid cookie in DP rx error descriptor retrieval: 0x%x\n",
				    cookie);
			return -EINVAL;
		}
	}

	if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC) {
		ath12k_warn(ab, " RX Exception, Check HW CC implementation");
		BUG_ON(1);
	}

	msdu = desc_info->skb;
	desc_info->skb = NULL;
	rxcb = ATH12K_SKB_RXCB(msdu);
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	rxcb->mhdr = NULL;
#endif
	rxcb->peer_id = le32_get_bits(desc->rx_mpdu_info.peer_meta_data,
				      RX_MPDU_DESC_META_DATA_V1_PEER_ID);

	list_add_tail(&desc_info->list, used_list);

	IPA_SET_RX_BUF_SMMU_UNMAP(ab, msdu, true);
	ATH12K_CORE_DMAC_INV_RANGE(desc_info);

	if (drop) {
		rcu_read_lock();
		peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, rxcb->peer_id);
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
			ahvif->wmm_stats.rx_type = dp_pdev->wmm_stats.rx_type;
			ahvif->wmm_stats.total_wmm_rx_drop[ahvif->wmm_stats.rx_type]++;
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev))
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, 0,
							 ATH_RX_RBM_ERR);
		}
		rcu_read_unlock();
		dev_kfree_skb_any(msdu);
		return 0;
	}

	rcu_read_lock();
	if (!rcu_dereference(ar->ab->pdevs_active[ar->pdev_idx])) {
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	rx_desc = (struct hal_rx_desc *)msdu->data;
	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, rx_desc, rx_desc);

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, rxcb->peer_id);
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
			ath12k_tid_rx_stats(ahvif, rxcb->tid, rx_desc_data.msdu_len,
					    ATH_RX_FRAG_PKTS);
		}
	}

	msdu_len = rx_desc_data.msdu_len;
	if ((msdu_len + hal_rx_desc_sz) > DP_RX_BUFFER_SIZE) {
		ath12k_warn(ab, "invalid msdu leng %u", msdu_len);
		ath12k_dbg_dump(ab, ATH12K_DBG_DATA, NULL, "", rx_desc,
				sizeof(*rx_desc));
		dev_kfree_skb_any(msdu);
		goto exit;
	}

	skb_put(msdu, hal_rx_desc_sz + msdu_len);

	if (ath12k_wifi7_dp_rx_frag_h_mpdu(dp_pdev, msdu, desc, &rx_desc_data)) {
		dev_kfree_skb_any(msdu);
		ath12k_wifi7_dp_rx_link_desc_return(dp, &desc->buf_addr_info,
						    HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);
	}
exit:
	rcu_read_unlock();
	return 0;
}

static int ath12k_wifi7_handle_msdu_buftype(struct ath12k_dp *dp,
					    struct hal_reo_dest_ring *reo_desc,
					    struct list_head *rx_desc_used_list)
{
	struct ath12k_rx_desc_info *desc_info;
	struct sk_buff *msdu;
	const void *end;
	u64 desc_va;

	desc_va = ((u64)le32_to_cpu(reo_desc->buf_va_hi) << 32 |
		   le32_to_cpu(reo_desc->buf_va_lo));
	desc_info = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	if (!desc_info) {
		ath12k_warn(dp, " rx exception, hw cookie conversion failed");
		u32 cookie = le32_get_bits(reo_desc->buf_addr_info.info1,
					   BUFFER_ADDR_INFO1_SW_COOKIE);
		desc_info = ath12k_dp_get_rx_desc(dp, cookie);
		if (!desc_info) {
			ath12k_warn(dp->ab, "Unable to retrieve rx_desc for va 0x%lx",
				    (unsigned long)desc_va);
			return -EINVAL;
		}
	}

	if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC) {
		ath12k_warn(dp, " rx exception, magic check failed");
		BUG_ON(1);
	}

	msdu = desc_info->skb;
	desc_info->skb = NULL;

	list_add_tail(&desc_info->list, rx_desc_used_list);

	end = desc_info->vaddr + DP_RX_BUFFER_SIZE;
	ath12k_core_dmac_inv_range(desc_info->vaddr, end);
	dev_kfree_skb_any(msdu);

	return 0;
}

int ath12k_wifi7_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp *partner_dp;
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	u32 msdu_cookies[HAL_NUM_RX_MSDUS_PER_LINK_DESC];
	int num_buffs_reaped[ATH12K_MAX_SOCS] = {};
	struct dp_link_desc_bank *link_desc_banks;
	enum hal_rx_buf_return_buf_manager rbm;
	struct hal_rx_msdu_link *link_desc_va;
	int tot_n_bufs_reaped, quota, ret, i;
	struct hal_reo_dest_ring *reo_desc;
	struct dp_rxdma_ring *rx_ring;
	struct dp_srng *reo_except;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	u8 hw_link_id, device_id;
	u32 desc_bank, num_msdus;
	struct hal_srng *srng;
	struct hal_srng *refill_srng;
	struct ath12k_pdev_dp *dp_pdev;
	dma_addr_t paddr;
	bool is_frag, drop = false;
	int pdev_id;
	struct list_head *used_list;
	enum hal_wbm_rel_bm_act act;

	tot_n_bufs_reaped = 0;
	quota = budget;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	reo_except = &dp->reo_except_ring;

	srng = &ab->hal.srng_list[reo_except->ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (budget &&
	       (reo_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		drop = false;
		dp->device_stats.err_ring_pkts++;

		hw_link_id = le32_get_bits(reo_desc->info0,
					   HAL_REO_DEST_RING_INFO0_SRC_LINK_ID);
		device_id = hw_links[hw_link_id].device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);

		ret = ath12k_wifi7_hal_desc_reo_parse_err(partner_dp, reo_desc, &paddr,
							  &desc_bank);
		if (ret) {
			ath12k_warn(ab, "failed to parse error reo desc %d\n",
				    ret);
			if (ret == -EOPNOTSUPP) {
				used_list = &rx_desc_used_list[device_id];
				if (!ath12k_wifi7_handle_msdu_buftype(partner_dp,
								      reo_desc,
								      used_list))
					tot_n_bufs_reaped++;
			}
			continue;
		}

		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);

		link_desc_banks = partner_dp->link_desc_banks;
		link_desc_va = link_desc_banks[desc_bank].vaddr +
			       (paddr - link_desc_banks[desc_bank].paddr);
		ath12k_wifi7_hal_rx_msdu_link_info_get(link_desc_va, &num_msdus,
						       msdu_cookies, &rbm);
		if (rbm != partner_dp->idle_link_rbm &&
		    rbm != HAL_RX_BUF_RBM_SW5_BM &&
		    rbm != partner_dp->hal->hal_params->rx_buf_rbm) {
			act = HAL_WBM_REL_BM_ACT_REL_MSDU;
			partner_dp->device_stats.invalid_rbm++;
			ath12k_warn(ab, "invalid return buffer manager %d\n", rbm);
			ath12k_wifi7_dp_rx_link_desc_return(partner_dp,
							    &reo_desc->buf_addr_info,
							    act);
			continue;
		}

		is_frag = !!(le32_to_cpu(reo_desc->rx_mpdu_info.info0) &
			     RX_MPDU_DESC_INFO0_FRAG_FLAG);

		/* Process only rx fragments with one msdu per link desc below, and drop
		 * msdu's indicated due to error reasons.
		 * Dynamic fragmentation not supported in Multi-link client, so drop the
		 * partner device buffers.
		 */
		if (!is_frag || num_msdus > 1 ||
		    partner_dp->device_id != dp->device_id) {
			drop = true;
			act = HAL_WBM_REL_BM_ACT_PUT_IN_IDLE;

			/* Return the link desc back to wbm idle list */
			ath12k_wifi7_dp_rx_link_desc_return(partner_dp,
							    &reo_desc->buf_addr_info,
							    act);
		}

		rcu_read_lock();

		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (!dp_pdev) {
			rcu_read_unlock();
			continue;
		}

		if (drop)
			dp_pdev->wmm_stats.total_wmm_rx_drop[dp_pdev->wmm_stats.rx_type]++;

		for (i = 0; i < num_msdus; i++) {
			used_list = &rx_desc_used_list[device_id];

			if (!ath12k_wifi7_dp_process_rx_err_buf(dp_pdev, reo_desc,
								used_list,
								drop,
								msdu_cookies[i])) {
				num_buffs_reaped[device_id]++;
				tot_n_bufs_reaped++;
			}
		}

		rcu_read_unlock();

		if (tot_n_bufs_reaped >= quota) {
			tot_n_bufs_reaped = quota;
			goto exit;
		}

		budget = quota - tot_n_bufs_reaped;
	}

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_buffs_reaped[device_id])
			continue;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;
		refill_srng =
			&partner_dp->ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
		ath12k_dp_rx_bufs_replenish(partner_dp, refill_srng,
					    &rx_desc_used_list[device_id], false);
	}

	return tot_n_bufs_reaped;
}

int ath12k_wifi7_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned,
				    u16 stats_id)
{
	u8 tid = rx_tid->tid;
	u32 ba_win_sz = rx_tid->ba_win_sz;
	void *vaddr;
	u32 hw_desc_sz;
	dma_addr_t paddr;
	int ret;

	/* TODO: Optimize the memory allocation for qos tid based on
	 * the actual BA window size in REO tid update path.
	 */
	if (tid == HAL_NON_QOS_TID)
		hw_desc_sz = ath12k_wifi7_hal_reo_qdesc_size(ba_win_sz, tid);
	else
		hw_desc_sz = ath12k_wifi7_hal_reo_qdesc_size(DP_BA_WIN_SZ_MAX, tid);

	vaddr = kzalloc(hw_desc_sz + HAL_LINK_DESC_ALIGN - 1, GFP_ATOMIC);
	if (!vaddr)
		return -ENOMEM;

	*addr_aligned = PTR_ALIGN(vaddr, HAL_LINK_DESC_ALIGN);
	ath12k_wifi7_hal_reo_qdesc_setup(*addr_aligned, tid, ba_win_sz, ssn, pn_type,
					 stats_id);
#ifndef CONFIG_IO_COHERENCY
	paddr = dma_map_single(ab->dev, *addr_aligned, hw_desc_sz,
			       DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(ab->dev, paddr);
	if (ret) {
		kfree(vaddr);
		return ret;
	}
#else
	paddr = virt_to_phys(*addr_aligned);
	if (!paddr) {
		kfree(vaddr);
		return ret;
	}
#endif
	rx_tid->vaddr = vaddr;
	rx_tid->paddr = paddr;
	rx_tid->size = hw_desc_sz;

	return 0;
}

int ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	ATH12K_DP_RXDMA_RING_CONFIG(ring_id, dp);

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
				    FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset =
		ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_qcn9274();
	tlv_filter.rx_msdu_end_offset =
		ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_qcn9274();

	tlv_filter.rx_mpdu_start_wmask =
			ath12k_wifi7_hal_rx_mpdu_start_wmask_get_qcn9274();
	tlv_filter.rx_msdu_end_wmask =
			ath12k_wifi7_hal_rx_msdu_end_wmask_get_qcn9274();

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "Configuring compact tlv masks rx_mpdu_start_wmask 0x%x rx_msdu_end_wmask 0x%x\n",
		   tlv_filter.rx_mpdu_start_wmask, tlv_filter.rx_msdu_end_wmask);

	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
					       HAL_RXDMA_BUF,
					       DP_RX_BUFFER_SIZE,
					       ATH12K_PKTLOG_DISABLED,
					       &tlv_filter);

	return ret;
}

int ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret = 0;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;
	int i;

	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
				    FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_header_offset = offsetof(struct hal_rx_desc_wcn7850, pkt_hdr_tlv);

	tlv_filter.rx_mpdu_start_offset =
		ath12k_wifi7_hal_rx_desc_get_mpdu_start_offset_wcn7850();
	tlv_filter.rx_msdu_end_offset =
		ath12k_wifi7_hal_rx_desc_get_msdu_end_offset_wcn7850();

	/* TODO: Selectively subscribe to required qwords within msdu_end
	 * and mpdu_start and setup the mask in below msg
	 * and modify the rx_desc struct
	 */

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp->rx_mac_buf_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, i,
						       HAL_RXDMA_BUF,
						       DP_RX_BUFFER_SIZE,
						       ATH12K_PKTLOG_DISABLED,
						       &tlv_filter);
	}

	return ret;
}

void ath12k_wifi7_dp_rx_process_reo_status(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_tlv_64_hdr *hdr;
	struct hal_srng *srng;
	struct ath12k_dp_rx_reo_cmd *cmd, *tmp;
	bool found = false;
	u16 tag;
	struct hal_reo_status reo_status;

	srng = &ab->hal.srng_list[dp->reo_status_ring.ring_id];

	memset(&reo_status, 0, sizeof(reo_status));

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while ((hdr = ath12k_hal_srng_dst_get_next_entry(ab, srng))) {
		tag = le64_get_bits(hdr->tl, HAL_SRNG_TLV_HDR_TAG);

		switch (tag) {
		case HAL_REO_GET_QUEUE_STATS_STATUS:
			ath12k_wifi7_hal_reo_status_queue_stats(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_QUEUE_STATUS:
			ath12k_wifi7_hal_reo_flush_queue_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_CACHE_STATUS:
			ath12k_wifi7_hal_reo_flush_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_UNBLOCK_CACHE_STATUS:
			ath12k_wifi7_hal_reo_unblk_cache_status(ab, hdr,
								&reo_status);
			break;
		case HAL_REO_FLUSH_TIMEOUT_LIST_STATUS:
			ath12k_wifi7_hal_reo_flush_timeout_list_status(ab, hdr,
								       &reo_status);
			break;
		case HAL_REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS:
			ath12k_wifi7_hal_reo_desc_thresh_reached_status(ab, hdr,
									&reo_status);
			break;
		case HAL_REO_UPDATE_RX_REO_QUEUE_STATUS:
			ath12k_wifi7_hal_reo_update_rx_reo_queue_status(ab, hdr,
									&reo_status);
			break;
		default:
			ath12k_warn(ab, "Unknown reo status type %d\n", tag);
			continue;
		}

		spin_lock_bh(&dp->reo_cmd_lock);
		list_for_each_entry_safe(cmd, tmp, &dp->reo_cmd_list, list) {
			if (reo_status.uniform_hdr.cmd_num == cmd->cmd_num) {
				found = true;
				list_del(&cmd->list);
				break;
			}
		}
		spin_unlock_bh(&dp->reo_cmd_lock);

		if (found) {
			cmd->handler(dp, &cmd->u.data, &reo_status);
			kfree(cmd);
		}

		found = false;
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);
}

int ath12k_wifi7_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	fst->num_entries = 0;

	fst->base = kcalloc(HAL_RX_FLOW_SEARCH_TABLE_SIZE,
			    sizeof(struct dp_rx_fse), GFP_KERNEL);
	if (!fst->base)
		return -ENOMEM;

	fst->hal_rx_fst = ath12k_wifi7_hal_rx_fst_attach(ab);
	if (!fst->hal_rx_fst) {
		ath12k_err(ab, "Rx Hal fst allocation failed\n");
		kfree(fst->base);
		return -ENOMEM;
	}

	return 0;
}

void ath12k_wifi7_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst)
{
	struct ath12k_base *ab = dp->ab;

	ath12k_wifi7_hal_rx_fst_detach(ab, fst->hal_rx_fst);
	kfree(fst->base);
}

void ath12k_wifi7_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;
	struct ath12k_base *ab = dp->ab;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest IP address %x:%x:%x:%x",
		   tuple_info->dest_ip_127_96,
		   tuple_info->dest_ip_95_64,
		   tuple_info->dest_ip_63_32,
		   tuple_info->dest_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Source IP address %x:%x:%x:%x",
		   tuple_info->src_ip_127_96,
		   tuple_info->src_ip_95_64,
		   tuple_info->src_ip_63_32,
		   tuple_info->src_ip_31_0);
	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Dest port %u, Src Port %u, Protocol %u",
		   tuple_info->dest_port,
		   tuple_info->src_port,
		   tuple_info->l4_protocol);
}

static
u32 ath12k_dp_rx_flow_compute_flow_hash(struct ath12k_base *ab,
					struct dp_rx_fst *fst,
					struct rx_flow_info *rx_flow_info,
					struct hal_rx_flow *flow)
{
	memcpy(&flow->tuple_info, &rx_flow_info->flow_tuple_info,
	       sizeof(struct hal_flow_tuple_info));

	return ath12k_wifi7_hal_flow_toeplitz_hash(ab, fst->hal_rx_fst,
						   &flow->tuple_info);
}

static inline struct dp_rx_fse *
ath12k_dp_rx_flow_get_fse(struct dp_rx_fst *fst, u32 flow_hash)
{
	struct dp_rx_fse *fse;
	u32 idx = ath12k_wifi7_hal_rx_get_trunc_hash(fst->hal_rx_fst, flow_hash);

	fse = (struct dp_rx_fse *)fst->base;
	return &fse[idx];
}

struct dp_rx_fse *
ath12k_dp_rx_flow_find_entry_by_tuple(struct ath12k_base *ab,
				      struct dp_rx_fst *fst,
				      struct rx_flow_info *flow_info,
				      struct hal_rx_flow *flow)
{
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi7_hal_rx_find_flow_from_tuple(ab, fst->hal_rx_fst,
							  flow_hash,
							  &flow_info->flow_tuple_info,
							  &flow_idx);
	if (status != 0) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Could not find tuple with hash %u", flow_hash);
		ath12k_wifi7_dp_rx_flow_dump_entry(ab->dp, flow_info);
		return NULL;
	}

	return ath12k_dp_rx_flow_get_fse(fst, flow_idx);
}

ssize_t ath12k_wifi7_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = ab->ag->dp_hw_grp->fst;
	int len = 0;

	if (!fst) {
		ath12k_warn(ab, "FST table is NULL\n");
		return -ENODEV;
	}

	len += scnprintf(buf + len, size - len,
			 "Number of entries in FST table: %d\n", fst->num_entries);
	len += ath12k_wifi7_hal_rx_dump_fst_table(ab, fst->hal_rx_fst, buf + len, size - len);

	return len;
}

bool
ath12k_wifi7_dp_rx_check_if_flow_to_be_updated(struct dp_rx_fse *fse,
					       struct rx_flow_info *flow_info)
{
	struct hal_rx_fse *hal_fse = fse->hal_fse;
	u32 use_ppe;

	use_ppe = u32_get_bits(hal_fse->info2, HAL_RX_FSE_SERVICE_CODE);

	/* If everything matches, it is a duplicate flow request,
	 * no need to modify anything,
	 * else modify the flow entry.
	 */
	if (use_ppe == flow_info->use_ppe &&
	    (hal_fse->metadata & flow_info->fse_metadata))
		return false;

	return true;
}

struct dp_rx_fse *
ath12k_wifi7_dp_rx_flow_alloc_entry(struct ath12k_base *ab,
				    struct dp_rx_fst *fst,
				    struct rx_flow_info *flow_info,
				    struct hal_rx_flow *flow)
{
	struct dp_rx_fse *fse;
	u32 flow_hash;
	u32 flow_idx;
	int status;

	flow_hash = ath12k_dp_rx_flow_compute_flow_hash(ab, fst, flow_info, flow);

	status = ath12k_wifi7_hal_rx_flow_insert_entry(ab, fst->hal_rx_fst, flow_hash,
						       &flow_info->flow_tuple_info,
						       &flow_idx);
	if (status != 0) {
		if (status == -EEXIST) {
			/* Even if the flow tuple info exists, there is a possibility
			 * that the flow entry has to be updated - so check
			 * for rx_flow_info if it matches exactly with the programmed
			 * fse entry
			 */
			fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);

			if (ath12k_wifi7_dp_rx_check_if_flow_to_be_updated(fse,
									   flow_info)) {
				ath12k_dbg(ab, ATH12K_DBG_DP_FST,
					   "Flow entry to be updated - hash %u",
					   flow_hash);
				return fse;
			}
		}
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "Add entry failed with status %d for tuple with hash %u",
			   status, flow_hash);
		return NULL;
	}

	fse = ath12k_dp_rx_flow_get_fse(fst, flow_idx);
	fse->flow_hash = flow_hash;
	fse->flow_id = flow_idx;

	return fse;
}

static int ath12k_dp_fst_get_reo_indication(struct ath12k_dp *dp)
{
	u8 reo_indication;

	reo_indication = dp->fst_config.fst_core_map[dp->fst_config.core_idx] + 1;
	dp->fst_config.core_idx = (dp->fst_config.core_idx + 1) %
					dp->fst_config.fst_num_cores;

	return reo_indication;
}

int ath12k_wifi7_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;
	bool is_new;

	/* Allocate entry in DP FST */
	fse = ath12k_wifi7_dp_rx_flow_alloc_entry(ab, fst, flow_info, &flow);
	if (!fse) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX FSE alloc failed");
		return -ENOMEM;
	}

	/* is_new is true only for a genuinely fresh slot; alloc_entry no longer
	 * sets is_valid, so we can detect re-programs (EEXIST path) here.
	 */
	is_new = !fse->is_valid;

	flow.drop = flow_info->drop;

	/* Reo indication is required only when drop bit is not set */
	if (!flow.drop) {
		if (flow_info->ring_id && flow_info->ring_id <= DP_REO_DST_RING_MAX)
			flow.reo_indication = flow_info->ring_id;
		else
			flow.reo_indication = ath12k_dp_fst_get_reo_indication(dp);
	}

	fse->reo_indication = flow.reo_indication;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	flow.fse_metadata |= flow_info->fse_metadata;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (flow_info->use_ppe) {
		flow.use_ppe = flow_info->use_ppe;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		flow.service_code = PPE_DRV_SC_SPF_BYPASS;
#endif
	}
#endif

	fse->hal_fse = ath12k_wifi7_hal_rx_flow_setup_fse(ab, fst->hal_rx_fst,
							  fse->flow_id, &flow);
	if (!fse->hal_fse) {
		ath12k_err(ab, "Unable to alloc FSE entry");
		fse->is_valid = false;
		return -EEXIST;
	}

	fse->is_valid = true;
	if (is_new) {
		fst->num_entries++;
		if (fse->reo_indication)
			fst->flows_per_reo[fse->reo_indication - 1]++;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d, reo_dest_ind = %d, reo_dest_hand = %u",
		   fst->num_entries, flow.reo_indication,
		   flow.reo_destination_handler);

	return 0;
}

int ath12k_wifi7_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info)
{
	struct ath12k_base *ab = dp->ab;
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fse *fse;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;

	fse = ath12k_dp_rx_flow_find_entry_by_tuple(ab, fst, flow_info, &flow);
	if (!fse || !fse->is_valid) {
		ath12k_dbg(ab, ATH12K_DBG_DP_FST, "RX flow delete entry failed");
		return -EINVAL;
	}

	/* Delete the FSE in HW FST */
	ath12k_wifi7_hal_rx_flow_delete_entry(ab, fse->hal_fse);

	/* mark the FSE entry as invalid */
	fse->is_valid = false;

	/* Decrement number of valid entries in table */
	fst->num_entries--;
	if (fse->reo_indication)
		fst->flows_per_reo[fse->reo_indication - 1]--;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

int ath12k_wifi7_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct dp_rx_fst *fst = dp->dp_hw_grp->fst;
	struct dp_rx_fse *fse;
	int i;

	fse = (struct dp_rx_fse *)fst->base;
	if (!fse)
		return -ENODEV;

	for (i = 0; i < fst->hal_rx_fst->max_entries; i++, fse++) {
		if (!fse->is_valid)
			continue;

		ath12k_wifi7_hal_rx_flow_delete_entry(ab, fse->hal_fse);

		fse->is_valid = false;

		fst->num_entries--;
		if (fse->reo_indication)
			fst->flows_per_reo[fse->reo_indication - 1]--;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST,
		   "FST num_entries = %d", fst->num_entries);

	return 0;
}

int ath12k_wifi7_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id, u8 pdev_id)
{
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_base *ab = dp->ab;
	int ret, tid;

	for (tid = 0; tid < ab->hal.hal_params->num_tids; tid++) {
		rx_tid = &peer->dp_peer->rx_tid[tid];

		spin_lock_bh(&rx_tid->tid_lock);
		ath12k_wifi7_peer_rx_tid_qref_reset(ab, peer->dp_peer->peer_id, tid);
		ath12k_wifi7_hal_reo_shared_qaddr_cache_clear(ab);

		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS;
		ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
						   HAL_REO_CMD_FLUSH_QUEUE,
						   &cmd, NULL);

		if (ret) {
			ath12k_warn(ab, "failed to flush rx tid queue, tid %d (%d)\n",
				    rx_tid->tid, ret);
			spin_unlock_bh(&rx_tid->tid_lock);
			return ret;
		}
		spin_unlock_bh(&rx_tid->tid_lock);
	}

	cmd.flag = 0;
	rx_tid = &peer->dp_peer->rx_tid[0];

	spin_lock_bh(&rx_tid->tid_lock);

	rx_tid->chip_id = chip_id;
	rx_tid->peer_id = peer_id;
	rx_tid->pdev_id = pdev_id;

	cmd.addr_lo = lower_32_bits(rx_tid->paddr);
	cmd.addr_hi = upper_32_bits(rx_tid->paddr);
	cmd.flag |= HAL_REO_CMD_FLG_NEED_STATUS;
	cmd.flag |= HAL_REO_CMD_FLG_FLUSH_ALL;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					   HAL_REO_CMD_FLUSH_CACHE,
					   &cmd,
					   ath12k_dp_primary_peer_migrate_setup);
	if (ret) {
		ath12k_warn(ab, "failed to flush cache for peer_id %x\n", peer->peer_id);
		spin_unlock_bh(&rx_tid->tid_lock);
		return ret;
	}

	cmd.flag = 0;
	cmd.flag = HAL_REO_CMD_FLG_UNBLK_CACHE;

	ret = ath12k_wifi7_dp_reo_cmd_send(ab, rx_tid, sizeof(*rx_tid),
					  HAL_REO_CMD_UNBLOCK_CACHE,
					  &cmd, NULL);
	if (ret)
		ath12k_warn(ab, "failed to unblock cache for peer_id %x\n", peer->peer_id);

	spin_unlock_bh(&rx_tid->tid_lock);
	return ret;
}

int ath12k_wifi7_dp_rx_htt_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 ring_id;
	int i, ret;

	/* TODO: Need to verify the HTT setup for QCN9224 */
	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_RXDMA_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rx_refill_buf_ring %d\n",
			    ret);
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
							  i, HAL_RXDMA_BUF);
			if (ret) {
				ath12k_warn(ab, "failed to configure rx_mac_buf_ring%d %d\n",
					    i, ret);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ring_id = dp->rxdma_err_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
						  i, HAL_RXDMA_DST);
		if (ret) {
			ath12k_warn(ab, "failed to configure rxdma_err_dest_ring%d %d\n",
				    i, ret);
			return ret;
		}
	}

	ret = ath12k_dp_mon_rx_htt_setup(dp);
	if (ret) {
		ath12k_warn(ab, "Failed to setup rxdma monitor rings\n");
		return ret;
	}

	return 0;
}

int ath12k_wifi7_dp_rx_flow_fse_cache_operation(struct ath12k_base *ab,
						enum dp_flow_fst_operation op_code,
						struct hal_flow_tuple_info *tuple_info)
{
	int i;
	int ret = 0;

	for (i = 0; i < ab->ag->num_devices; i++) {
		struct ath12k_base *partner_ab = ab->ag->ab[i];

		if (!partner_ab || partner_ab->is_bypassed)
			continue;

		if (partner_ab->ag->wsi_remap_in_progress &&
		    partner_ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE)
			continue;

		/* Skip sending HTT command during recovery, crash flush, or
		 * when CE pipes are not yet initialized. Accessing an
		 * uninitialized CE source ring causes a NULL dereference in
		 * __ath12k_hal_srng_access_begin via the HTT CE send path.
		 */
		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &partner_ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags) ||
		    !partner_ab->ce_pipe_init_done)
			continue;

		ret = ath12k_dp_htt_rx_flow_fse_operation(partner_ab,
							  op_code,
							  tuple_info);
		if (ret) {
			ath12k_err(partner_ab, "Unable to invalidate cache entry ret %d",
				   ret);
			return ret;
		}
	}

	return ret;
}

void ath12k_wifi7_dp_pdev_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k *ar;
	int i;

	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], NULL);
	}
	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ath12k_fw_stats_free(&ar->fw_stats);

		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_tx_pdev_free(&ar->dp);
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);
			ar->dp.dp_mon_pdev_configured = false;
		}
	}
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ab->dp->ppe.ppe_ops->ath12k_ppeds_stop(ab);
#endif
}

int ath12k_wifi7_dp_pdev_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k *ar;
	int ret, i, j;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_dp_ppe_rxole_rxdma_cfg(ab);
	if (ret) {
		ath12k_err(ab, "Failed to send htt RxOLE and RxDMA messages to target :%d\n",
			   ret);
		goto out;
	}
#endif

	ret = ath12k_wifi7_dp_rx_htt_setup(ab);
	if (ret)
		goto out;

	/* TODO: Per-pdev rx ring unlike tx ring which is mapped to different AC's */
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		memset(&ar->stats, 0, sizeof(struct ath12k_pdev_ctrl_path_stats));
		dp_pdev = &ar->dp;

		dp_pdev->hw = ar->ah->hw;
		dp_pdev->dp = dp;
		/* Below linking is a temporary linking to handle few cases like cac
		 * timeout, active pdev etc in dp rx. Some flags/fileds can be added
		 * in dp_pdev to remove ar dependencies in the performance critical
		 * path.
		 *
		 * TODO: remove this once those dependencies are resolved.
		 */
		dp_pdev->ar = ar;
		dp_pdev->dp_hw = &ar->ah->dp_hw;
		dp_pdev->hw_link_id = ar->hw_link_id;

		/* Restore dp_stats_mask from INI to persist value across reboots */
		ar->dp.dp_stats_mask = DP_STATS_MASK;

		if (!dp_pdev->dp_mon_pdev_configured) {
			ret = ath12k_dp_mon_pdev_init(dp_pdev);
			if (ret) {
				ath12k_warn(ab, "failed to initialize mon pdev %d\n", i);
				goto err_cleanup_pdevs;
			}

			ret = ath12k_dp_mon_pdev_rx_alloc(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to alloc rx filter for pdev %d\n",
					    i);
				goto err_mon_pdev_deinit;
			}

			ret = ath12k_dp_mon_pdev_rx_htt_setup(dp_pdev, i);
			if (ret) {
				ath12k_warn(ab,
					    "failed to setup rx htt for pdev %d\n", i);
				goto err_mon_pdev_rx_free;
			}

			ret = ath12k_dp_mon_tx_pdev_alloc(dp_pdev, i);
			if (ret) {
				ath12k_err(ab, "TX Monitor: alloc fail pdev %d(%d)\n",
					   i, ret);
				goto err_mon_pdev_tx_free;
			}

			dp_pdev->dp_mon_pdev_configured = true;
		}
	}
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops &&
		ab->dp->ppe.ppe_ops->ath12k_ppeds_start) {
		ret = ab->dp->ppe.ppe_ops->ath12k_ppeds_start(ab);
		if (ret) {
			ath12k_err(ab, "failed to start DP PPEDS\n");
			goto err_cleanup_pdevs;
		}
	}
#endif

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], &ar->dp);
	}
	spin_unlock_bh(&dp->dp_lock);

	dp->num_radios = ab->num_radios;

	return ret;

err_mon_pdev_tx_free:
	ath12k_dp_mon_tx_pdev_free(dp_pdev);
err_mon_pdev_rx_free:
	/* Clean up the current pdev's RX allocation */
	ath12k_dp_mon_pdev_rx_free(dp_pdev);
err_mon_pdev_deinit:
	/* Clean up the current pdev's monitor initialization */
	ath12k_dp_mon_pdev_deinit(dp_pdev);
err_cleanup_pdevs:
	/* Clean up all previously configured pdevs */
	for (j = 0; j < i; j++) {
		ar = ab->pdevs[j].ar;
		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_tx_pdev_free(&ar->dp);
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);
			ar->dp.dp_mon_pdev_configured = false;
		}
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ab->dp->ppe.ppe_ops->ath12k_ppeds_stop(ab);
#endif
out:
	return ret;
}

void ath12k_wifi7_dp_rx_ring_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	ath12k_dp_srng_cleanup(ab, &dp->rx_refill_buf_ring.refill_buf_ring);

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		if (ab->hw_params->rx_mac_buf_ring)
			ath12k_dp_srng_cleanup(ab, &dp->rx_mac_buf_ring[i]);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++)
		ath12k_dp_srng_cleanup(ab, &dp->rxdma_err_dst_ring[i]);

	ath12k_dp_srng_cleanup(ab, &dp->rx_rel_ring);
	ath12k_dp_rx_reo_cleanup(ab);
}

int ath12k_wifi7_dp_rx_ring_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret;

	ret = ath12k_dp_rx_reo_alloc(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize reo destination rings: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_REL_ERR_RING_NUM, 0,
				   DP_RX_RELEASE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab,
				   &dp->rx_refill_buf_ring.refill_buf_ring,
				   HAL_RXDMA_BUF, 0, 0,
				   DP_RXDMA_BUF_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to setup rx_refill_buf_ring\n");
		goto err;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ret = ath12k_dp_srng_alloc(ab,
						   &dp->rx_mac_buf_ring[i],
						   HAL_RXDMA_BUF, 1,
						   i, DP_RX_MAC_BUF_RING_SIZE);
			if (ret) {
				ath12k_warn(ab, "failed to setup rx_mac_buf_ring %d\n",
					    i);
				goto err;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ret = ath12k_dp_srng_alloc(ab, &dp->rxdma_err_dst_ring[i],
					   HAL_RXDMA_DST, 0, i,
					   DP_RXDMA_ERR_DST_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to setup rxdma_err_dst_ring %d\n", i);
			goto err;
		}
	}

	return 0;

err:
	ath12k_wifi7_dp_rx_ring_free(ab);
	return ret;
}

int ath12k_wifi7_dp_rx_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret;

	ret = ath12k_dp_rx_reo_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize reo destination rings: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp->rx_rel_ring, HAL_WBM2SW_RELEASE,
				  HAL_WBM2SW_REL_ERR_RING_NUM, 0);
	if (ret) {
		ath12k_warn(ab, "failed to set up rx_rel ring :%d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab,
				  &dp->rx_refill_buf_ring.refill_buf_ring,
				  HAL_RXDMA_BUF, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to setup rx_refill_buf_ring\n");
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ret = ath12k_dp_srng_init(ab,
						  &dp->rx_mac_buf_ring[i],
						  HAL_RXDMA_BUF, 1,
						  i);
			if (ret) {
				ath12k_warn(ab, "failed to setup rx_mac_buf_ring %d\n",
					    i);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ret = ath12k_dp_srng_init(ab, &dp->rxdma_err_dst_ring[i],
					  HAL_RXDMA_DST, 0, i);
		if (ret) {
			ath12k_warn(ab, "failed to setup rxdma_err_dst_ring %d\n", i);
			return ret;
		}
	}

	RETURN_IPA_CODE(0);
	ret = ath12k_dp_rxdma_buf_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma ring\n");
		return ret;
	}

	return 0;
}
