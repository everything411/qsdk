// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "hal.h"
#include "dp.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs.h"
#include "hw.h"
#include "peer.h"
#include "mac.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif
#include "hal.h"
#include "dp_peer.h"
#include "dp_stats.h"
#include "dp_htt.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/mesh_util.h"
#endif /* CPTCFG_QCN_EXTN */

void ath12k_tid_tx_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_pkt_stats[reason]++;
	tstats->tid_stats[tid].tx_pkt_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_tx_stats);

void ath12k_tid_tx_drop_stats(struct ath12k_vif *ahvif, u8 tid, u32 len, u32 reason)
{
	struct pcpu_netdev_tid_stats *tstats = this_cpu_ptr(ahvif->tstats);

	u64_stats_update_begin(&tstats->syncp);
	tstats->tid_stats[tid].tx_drop_stats[reason]++;
	tstats->tid_stats[tid].tx_drop_bytes[reason] += len;
	u64_stats_update_end(&tstats->syncp);
}
EXPORT_SYMBOL(ath12k_tid_tx_drop_stats);

int ath12k_dp_tx_get_mcast_group_slot(struct ath12k_vif *vlan_ahvif,
				      u8 link_id)
{
	int slot;

	if (!vlan_ahvif || !vlan_ahvif->vlan_iface ||
	    vlan_ahvif->vlan_iface->is_wds_4addr)
		return -1;

	if (link_id >= ATH12K_NUM_MAX_LINKS)
		return -1;

	slot = vlan_ahvif->vlan_iface->grp_key_slot[link_id];
	if (slot <= 0 || slot >= ATH12K_GROUP_KEYS_NUM_MAX)
		return -1;

	return slot;
}
EXPORT_SYMBOL(ath12k_dp_tx_get_mcast_group_slot);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void
ath12k_dp_ppeds_tx_release_desc_list_bulk(struct ath12k_base *ab,
					  struct ath12k_dp_hw_group *dp_hw_grp,
					  struct list_head *local_list,
					  int local_list_len,
					  struct list_head *local_list_no_skb,
					  int list_no_skb_count)
{
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *first_desc = NULL, *last_desc = NULL, *tmp;
	int hotlist_remaining_len;
	struct sk_buff_head free_list_head;
	struct sk_buff *skb;
	int count = 0;
	struct list_head local_list_for_reuse;
	u32 *used_cnt;

	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	if (unlikely(list_no_skb_count)) {
		list_for_each_entry_safe(desc, tmp, local_list_no_skb, list) {
			desc->paddr = (dma_addr_t)NULL;
			desc->in_use = false;
		}

		list_splice_tail(local_list_no_skb, &dp_hw_grp->ppeds_tx_desc_free_list);
		used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
		(*used_cnt) -= list_no_skb_count;
	}

	hotlist_remaining_len = ATH12K_PPEDS_HOTLIST_LEN_MAX_DEFAULT -
						dp_hw_grp->ppeds_tx_desc_reuse_list_len;

	if (likely(hotlist_remaining_len >= local_list_len)) {
		list_splice_tail(local_list, &dp_hw_grp->ppeds_tx_desc_reuse_list);

		used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
		(*used_cnt) -= local_list_len;

		dp_hw_grp->ppeds_tx_desc_reuse_list_len += local_list_len;
		spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);
		return;
	}

	if (hotlist_remaining_len == 0)
		goto skip_reuse_list;

	/* Identify the first and last descriptors in local_list with length of
	 * available room in hotlist
	 */
	list_for_each_entry_safe(desc, tmp, local_list, list) {
		if (count == 0)
			first_desc = desc;

		if (count == (hotlist_remaining_len - 1)) {
			last_desc = desc;
			break;
		}

		count++;
	}

	INIT_LIST_HEAD(&local_list_for_reuse);

	if (first_desc && last_desc) {
		/* cut the local_list into local_list_for_reuse and local_list */
		list_cut_position(&local_list_for_reuse, local_list, &last_desc->list);

		/* merge local_list_for_reuse into global ppeds_tx_desc_reuse_list */
		list_splice_tail(&local_list_for_reuse,
				 &dp_hw_grp->ppeds_tx_desc_reuse_list);
		dp_hw_grp->ppeds_tx_desc_reuse_list_len += count + 1;

		used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
		(*used_cnt) -= (count + 1);

	}

skip_reuse_list:
	skb_queue_head_init(&free_list_head);
	count = 0;

	list_for_each_entry_safe(desc, tmp, local_list, list) {
		count++;

		skb = desc->skb;
		desc->skb = NULL;
		desc->paddr = (dma_addr_t)NULL;
		desc->in_use = false;
		if (!skb) {
			pr_err("no skb in ds completion path");
			continue;
		}

		if (likely(skb->is_from_recycler))
			__skb_queue_head(&free_list_head, skb);
		else
			dev_kfree_skb(skb);
	}

	/* Add the remaining descriptors to the free list */
	list_splice_tail(local_list, &dp_hw_grp->ppeds_tx_desc_free_list);

	used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
	(*used_cnt) -= count;

	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	dev_kfree_skb_list_fast(&free_list_head);
}
EXPORT_SYMBOL(ath12k_dp_ppeds_tx_release_desc_list_bulk);
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_dp_ppeds_tx_comp_get_desc(struct ath12k_base *ab,
				      struct ath12k_dp_tx_comp_status *tx_comp_status,
				      struct ath12k_ppeds_tx_desc_info **tx_desc)
{
	if (tx_comp_status->tx_desc) {
		*tx_desc = (struct ath12k_ppeds_tx_desc_info *)
				((unsigned long)tx_comp_status->tx_desc);
	} else {
		*tx_desc = ath12k_dp_get_ppeds_tx_desc(ab, tx_comp_status->desc_id);
	}
}
EXPORT_SYMBOL(ath12k_dp_ppeds_tx_comp_get_desc);

void ath12k_hal_srng_ppeds_dst_inv_entry(struct ath12k_base *ab,
					 struct hal_srng *srng, int entries)
{
	u32 *desc, *last_desc;
	u32 tp, hp;
	u32 remaining_entries;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED) || !entries)
		return;

	tp = srng->u.dst_ring.tp;
	hp = srng->u.dst_ring.cached_hp;

	desc = srng->ring_base_vaddr + tp;
	if (hp > tp) {
		last_desc = ((void *)desc + entries * srng->entry_size * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)desc, (void *)last_desc);
	} else {
		remaining_entries = srng->ring_size - tp;
		last_desc = ((void *)desc + remaining_entries * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)desc, (void *)last_desc);

		last_desc = ((void *)srng->ring_base_vaddr + hp * sizeof(u32));
		ath12k_core_dmac_inv_range_no_dsb((void *)srng->ring_base_vaddr,
						  (void *)last_desc);
	}

	dsb(st);
}
EXPORT_SYMBOL(ath12k_hal_srng_ppeds_dst_inv_entry);

u16 dp_sawf_msduq_peer_id_set(u16 peer_id, u8 msduq)
{
	u16 peer_msduq = 0;

	peer_msduq |= (peer_id & SDWF_PEER_ID_MASK) << SDWF_PEER_ID_SHIFT;
	peer_msduq |= (msduq & SDWF_MSDUQ_MASK);
	return peer_msduq;
}

int ath12k_sdwf_reinject_handler(struct ath12k_base *ab, struct sk_buff *skb,
				 struct htt_tx_completion *status_desc, u8 mac_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k_skb_cb *skb_cb;
	struct ath12k_dp *dp;
	u8 host_tid_queue;
	u16 data_length;
	u16 peer_msduq;
	u8 htt_q_idx;
	u8 msduq_idx;
	u16 peer_id;
	u8 pdev_id;
	u8 msduq;
	int ret;
	u8 tid;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);

	peer_id = le32_get_bits(status_desc->info2, HTT_TX_WBM_REINJECT_SW_PEER_ID_M);
	data_length = le32_get_bits(status_desc->info2, HTT_TX_WBM_REINJECT_DATA_LEN_M);
	tid = le32_get_bits(status_desc->info3, HTT_TX_WBM_REINJECT_TID_M);
	htt_q_idx = le32_get_bits(status_desc->info3, HTT_TX_WBM_REINJECT_MSDUQ_ID_M);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
		   "peer_id %u data_length %u tid %u htt_q_idx %u",
		   peer_id, data_length, tid, htt_q_idx);

	host_tid_queue = htt_q_idx - DP_SDWF_DEFAULT_Q_PTID_MAX;
	msduq_idx = tid + host_tid_queue * DP_SDWF_TID_MAX;

	if (msduq_idx > DP_SDWF_Q_MAX - 1) {
		ath12k_err(ab, "Invalid msduq idx: %u, tid %u htt_q_idx %u",
			   msduq_idx, tid, htt_q_idx);
		return -EINVAL;
	}

	dp = ath12k_ab_to_dp(ab);
	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);

	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_err(ab, "Invalid peer id %u", peer_id);
		rcu_read_unlock();
		return -EINVAL;
	}

	if (!peer->dp_peer) {
		ath12k_err(ab, "dp_peer is NULL for peer_id %u", peer_id);
		rcu_read_unlock();
		return -EINVAL;
	}

	qos = peer->dp_peer->qos;
	if (!qos) {
		ath12k_err(ab, "QOS ctx for peer id %u", peer_id);
		rcu_read_unlock();
		return -EINVAL;
	}

	msduq = msduq_idx + DP_SDWF_DEFAULT_Q_MAX;
	peer_msduq = dp_sawf_msduq_peer_id_set(peer_id, msduq);

	skb->mark = ath_encode_sdwf_metadata(peer_msduq);
	skb->len = data_length;

	skb_cb = ATH12K_SKB_CB(skb);
	skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
	tx_info->flags |= IEEE80211_TX_CTL_HW_80211_ENCAP;

	arsta = ath12k_peer_get_link_sta(ab, peer);
	if (!arsta) {
		rcu_read_unlock();
		return -EINVAL;
	}
	arvif = arsta->arvif;

	/* This arch ops is temporary, must be removed once ppeds handler is moved to wifi7 */
	ret = dp->arch_ops->sdwf_reinject_handler(dp_pdev, arvif, skb,
						  arsta, peer->dp_peer);

	rcu_read_unlock();

	return ret;
}

void ath12k_ppeds_reinject_handler(struct ath12k_base *ab,
				   struct ath12k_ppeds_tx_desc_info *tx_desc,
				   struct htt_tx_completion *status_desc)
{
	u8 reinject_reason;
	int status;

	reinject_reason = le32_get_bits(status_desc->info1,
					HTT_TX_WBM_COMPLETION_V3_REINJECT_REASON_M);

	if (reinject_reason == HTT_TX_FW2WBM_REINJECT_REASON_SDWF_SVC_CLASS_ID_ABSENT) {
		struct sk_buff *skb = tx_desc->skb;
		/* sdwf reinject handler consume the skb,
		 * so set tx_desc->skb = NULL here.
		 */
		status = ath12k_sdwf_reinject_handler(ab, skb, status_desc,
						      tx_desc->mac_id);
		if (!status)
			tx_desc->skb = NULL;

		return;
	}
}
EXPORT_SYMBOL(ath12k_ppeds_reinject_handler);
#endif


void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u8 *qos_ctl;

	if (!ieee80211_is_data_qos(hdr->frame_control))
		return;

	qos_ctl = ieee80211_get_qos_ctl(hdr);
	memmove(skb->data + IEEE80211_QOS_CTL_LEN,
		skb->data, (void *)qos_ctl - (void *)skb->data);
	skb_pull(skb, IEEE80211_QOS_CTL_LEN);

	hdr = (void *)skb->data;
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
}
EXPORT_SYMBOL(ath12k_dp_tx_encap_nwifi);

void ath12k_dp_tx_release_txbuf_nolock(struct ath12k_dp *dp,
				       struct ath12k_tx_desc_info *tx_desc,
				       u8 pool_id)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	u32 *tx_desc_used_cnt;

	if (WARN_ON(!tx_desc->in_use))
		return;

	tx_desc->skb = NULL;
	tx_desc->skb_ext_desc = NULL;
	tx_desc->in_use = false;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	tx_desc->mmesh = 0;
#endif
	tx_desc->flags = 0;
	tx_desc->to_fw = 0;
	tx_desc->ext_kmem = 0;

	if (likely(!tx_desc->spl_desc))
		list_add_tail(&tx_desc->list, &dp_hw_grp->tx_desc_free_list[pool_id]);
	else
		list_add_tail(&tx_desc->list, &dp_hw_grp->tx_spl_desc_free_list[pool_id]);

	tx_desc_used_cnt = this_cpu_ptr(dp_hw_grp->tx_desc_used_cnt);
	(*tx_desc_used_cnt) ? (*tx_desc_used_cnt)-- : 0;
}
EXPORT_SYMBOL(ath12k_dp_tx_release_txbuf_nolock);

void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;

	spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	ath12k_dp_tx_release_txbuf_nolock(dp, tx_desc, pool_id);
	spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
}
EXPORT_SYMBOL(ath12k_dp_tx_release_txbuf);

struct
ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp_hw_group *dp_hw_grp,
						struct list_head *desc_free_list,
						u8 pool_id)
{
	struct ath12k_tx_desc_info *desc, *next_desc;
	u32 *tx_desc_used_cnt;

	spin_lock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
	desc = list_first_entry_or_null(&desc_free_list[pool_id],
					struct ath12k_tx_desc_info, list);
	if (!desc) {
		spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);
		return NULL;
	}

	list_del(&desc->list);
	desc->in_use = true;

	next_desc = list_first_entry_or_null(&desc_free_list[pool_id],
					     struct ath12k_tx_desc_info, list);
	if (next_desc)
		prefetch(next_desc);

	tx_desc_used_cnt = this_cpu_ptr(dp_hw_grp->tx_desc_used_cnt);
	(*tx_desc_used_cnt)++;

	spin_unlock_bh(&dp_hw_grp->tx_desc_lock[pool_id]);

	return desc;
}
EXPORT_SYMBOL(ath12k_dp_tx_assign_buffer);

enum hal_encrypt_type ath12k_dp_tx_get_encrypt_type(u32 cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		return HAL_ENCRYPT_TYPE_WEP_40;
	case WLAN_CIPHER_SUITE_WEP104:
		return HAL_ENCRYPT_TYPE_WEP_104;
	case WLAN_CIPHER_SUITE_TKIP:
		return HAL_ENCRYPT_TYPE_TKIP_MIC;
	case WLAN_CIPHER_SUITE_CCMP:
		return HAL_ENCRYPT_TYPE_CCMP_128;
	case WLAN_CIPHER_SUITE_CCMP_256:
		return HAL_ENCRYPT_TYPE_CCMP_256;
	case WLAN_CIPHER_SUITE_GCMP:
		return HAL_ENCRYPT_TYPE_GCMP_128;
	case WLAN_CIPHER_SUITE_GCMP_256:
		return HAL_ENCRYPT_TYPE_AES_GCMP_256;
	default:
		return HAL_ENCRYPT_TYPE_OPEN;
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_get_encrypt_type);

int ath12k_dp_tx_get_mpdu_encap_hdr_len(enum hal_encrypt_type enctype)
{
	switch (enctype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return 0;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
		/* 4-byte IV + 4-byte ICV */
		return IEEE80211_WEP_IV_LEN + IEEE80211_WEP_ICV_LEN;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
		/* 8-byte IV + 4-byte ICV (no MIC in header) */
		return IEEE80211_TKIP_IV_LEN + IEEE80211_TKIP_ICV_LEN;
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		/* 8-byte IV + 8-byte MIC + 4-byte ICV */
		return IEEE80211_TKIP_IV_LEN + IEEE80211_CCMP_MIC_LEN +
		       IEEE80211_TKIP_ICV_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_128:
		/* 8-byte IV + 8-byte MIC */
		return IEEE80211_CCMP_HDR_LEN + IEEE80211_CCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_CCMP_256:
		/* 8-byte IV + 16-byte MIC */
		return IEEE80211_CCMP_256_HDR_LEN + IEEE80211_CCMP_256_MIC_LEN;
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		/* 8-byte IV + 16-byte MIC */
		return IEEE80211_GCMP_HDR_LEN + IEEE80211_GCMP_MIC_LEN;
	case HAL_ENCRYPT_TYPE_WAPI:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	default:
		ath12k_err(NULL, "invalid encrypt type %d", enctype);
		return 0;
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_get_mpdu_encap_hdr_len);

void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len)
{
	struct sk_buff *tail;
	void *metadata;

	if (unlikely(skb_cow_data(skb, tail_len, &tail) < 0))
		return NULL;

	metadata = pskb_put(skb, tail, tail_len);
	memset(metadata, 0, tail_len);
	return metadata;
}
EXPORT_SYMBOL(ath12k_dp_metadata_align_skb);

void *ath12k_dp_metadata_align_skb_head(struct sk_buff *skb, u8 head_len)
{
	void *metadata;

	if (unlikely(skb_cow_head(skb, head_len)))
		return NULL;

	skb_push(skb, head_len);
	metadata = skb->data;
	memset(metadata, 0, head_len);

	return metadata;
}
EXPORT_SYMBOL(ath12k_dp_metadata_align_skb_head);

static void ath12k_dp_tx_move_payload(struct sk_buff *skb,
				      unsigned long delta,
				      bool head)
{
	unsigned long len = skb->len;

	if (head) {
		skb_push(skb, delta);
		memmove(skb->data, skb->data + delta, len);
		skb_trim(skb, len);
	} else {
		skb_put(skb, delta);
		memmove(skb->data + delta, skb->data, len);
		skb_pull(skb, delta);
	}
}

int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	unsigned long offset, delta1, delta2;
	struct sk_buff *skb2, *skb = *pskb;
	unsigned int headroom = skb_headroom(skb);
	int tailroom = skb_tailroom(skb);
	int ret = 0;

	offset = (unsigned long)skb->data & iova_mask;
	delta1 = offset;
	delta2 = iova_mask - offset + 1;

	if (headroom >= delta1) {
		ath12k_dp_tx_move_payload(skb, delta1, true);
	} else if (tailroom >= delta2) {
		ath12k_dp_tx_move_payload(skb, delta2, false);
	} else {
		skb2 = skb_realloc_headroom(skb, iova_mask);
		if (!skb2) {
			ret = -ENOMEM;
			goto out;
		}

		dev_kfree_skb_any(skb);

		offset = (unsigned long)skb2->data & iova_mask;
		if (offset)
			ath12k_dp_tx_move_payload(skb2, offset, true);
		*pskb = skb2;
	}

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_align_payload);

int
ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask)
{
	struct ath12k_base *ab = ar->ab;
	struct htt_h2t_msg_type_vdev_txrx_stats_req *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	int len = sizeof(*cmd), ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_h2t_msg_type_vdev_txrx_stats_req *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_MSG_TYPE,
			      HTT_H2T_MSG_TYPE_VDEV_TXRX_STATS_CFG);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_PDEV_ID,
			       ar->pdev->pdev_id);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_ENABLE, true);

	/* Periodic interval is calculated as 1 units = 8 ms.
	* Ex: 125 -> 1000 ms
	*/
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_INTERVAL,
			       (ATH12K_STATS_TIMER_DUR_1SEC >> 3));
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_RESET_STATS, true);
	cmd->vdev_id_lo_bitmask = (reset_bitmask & HTT_H2T_VDEV_TXRX_LO_BITMASK);
	cmd->vdev_id_hi_bitmask = ((reset_bitmask &
				    HTT_H2T_VDEV_TXRX_HI_BITMASK) >> 32);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type vdev stats offload request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_dp_tx_htt_pri_link_migr_msg(struct ath12k_base *ab, u16 vdev_id,
				       u16 peer_id, u16 ml_peer_id, u8 pdev_id,
				       u8 chip_id, u16 src_info, bool status)
{
	struct ath12k_htt_pri_link_migr_h2t_msg *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool src_info_valid = false;
	int len = sizeof(*cmd);
	struct sk_buff *skb;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct ath12k_htt_pri_link_migr_h2t_msg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_RESP,
				      ATH12K_HTT_PRI_LINK_MIGR_MSG_TYPE) |
		     le32_encode_bits(chip_id, ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID) |
		     le32_encode_bits(pdev_id, ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID) |
		     le32_encode_bits(vdev_id, ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID);

	ml_peer_id &= ~ATH12K_PEER_ML_ID_VALID;

	cmd->info1 = le32_encode_bits(peer_id, ATH12K_HTT_PRI_LINK_MIGR_PEER_ID) |
		     le32_encode_bits(ml_peer_id, ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID);

	if (src_info != 0)
		src_info_valid = true;

	cmd->info2 = le32_encode_bits(status, ATH12K_HTT_PRI_LINK_MIGR_STATUS) |
		     le32_encode_bits(src_info, ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO) |
		     le32_encode_bits(src_info_valid,
		     		      ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO_VALID);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt MLO send pri link migr resp for peer_id 0x%x ml_peer_id 0x%x vdev_id 0x%x pdev_id 0x%x chip_id 0x%x src_info 0x%x status %u\n",
		   peer_id, ml_peer_id, vdev_id, pdev_id, chip_id, src_info, status);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		dev_kfree_skb_any(skb);

	return ret;
}

/**
 * ath12k_wifi7_tx_classify_packet() - Classify packet type
 * @info: TX info
 * @skb: Socket buffer
 * @is_mcast: Output multicast flag
 * @is_mgmt: Output management flag
 *
 */
bool ath12k_dp_tx_classify_packet(struct ieee80211_hw *hw,
				  struct ath12k_dp_vif *dp_vif,
				  struct ieee80211_tx_info *info,
				  struct sk_buff *skb,
				  bool *is_mcast, bool *is_eth,
				  bool *data,
				  struct ieee80211_key_conf *key,
				  struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	enum ath12k_dp_tx_enq_error enq_drop;
	u32 info_flags = info->flags;
	u8 ring_id = 0;

	*data = false;
	*is_mcast = false;

	skb_ctrl->flags |= DP_SKB_MAC_CTRL;

	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	/* Check if non-linear skb for SG */
	if (skb_is_nonlinear(skb)) {
		if ((skb_shinfo(skb)->nr_frags) > DP_TX_MAX_NUM_FRAGS - 1) {
			if (skb_linearize(skb)) {
				enq_drop = DP_TX_ENQ_DROP_SKB_NO_LINEAR;
				goto drop;
			}
		} else {
			skb_ctrl->features |= DP_FEATURE_SG;
		}
	}

	/* Check if HW encapsulation */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		skb_ctrl->features |= DP_ETH_OFFLOAD;
		*is_eth = true;
		*data = true;
		skb_cb->flags |= ATH12K_SKB_HW_80211_ENCAP;
		*is_mcast = is_multicast_ether_addr(eth->h_dest);
		return true;
	}

	/* Native WiFi format */
	hdr = (struct ieee80211_hdr *)skb->data;
	if (dp_vif->tx_encap_type == ATH12K_HW_TXRX_ETHERNET)
		skb_ctrl->features |= DP_FEATURE_ENCAP_MISMATCH_HANDLE;

	if (ieee80211_is_mgmt(hdr->frame_control)) {
		return true;
	} else if (ieee80211_is_data(hdr->frame_control)) {
		const u8 *da = ieee80211_get_DA(hdr);
		*data = true;
		*is_mcast = is_multicast_ether_addr(da);
		return true;
	}
	enq_drop = DP_TX_ENQ_DROP_NON_DATA_FRAME;

drop:
	DP_STATS_INC(dp_vif, tx_i.drop[enq_drop], 1, ring_id);
	ieee80211_free_txskb(hw, skb);
	return false;
}
EXPORT_SYMBOL(ath12k_dp_tx_classify_packet);

int ath12k_dp_sg_ext_desc_populate(struct ath12k_dp *dp,
					 struct ath12k_dp_vif *dp_vif,
					 struct ath12k_dp_ext_desc *ext_desc,
					 struct sk_buff *skb, u8 ring_id)
{
	dma_addr_t paddr[DP_TX_MAX_NUM_FRAGS];
	const skb_frag_t *frag;
	size_t len;
	u32 hlen, nr_frags, cur_frag, i;

	nr_frags = skb_shinfo(skb)->nr_frags;
	hlen = skb_headlen(skb);

	paddr[0] = ath12k_core_dma_map_single(dp->dev, skb->data, hlen, DMA_TO_DEVICE);
	if (!paddr[0]) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:DMA mapping failed for skb head\n",
			   dp);
		goto fail_skb_head;
	}

	ath12k_dp_ext_desc_set_buf0(ext_desc, paddr[0], hlen);

	for (cur_frag = 0; cur_frag < nr_frags; cur_frag++) {
		frag = &skb_shinfo(skb)->frags[cur_frag];
		len = skb_frag_size(frag);

		paddr[cur_frag + 1] = ath12k_core_dma_map_frag(dp->dev, frag, len, 0,
							       DMA_TO_DEVICE);
		if (!paddr[cur_frag + 1]) {
			ath12k_dbg(NULL, ATH12K_DBG_DP_TX, "%p:DMA mapping failed for frag\n",
				   dp);
			goto fail_skb_frag;
		}
		ath12k_dp_ext_desc_set_buf(ext_desc, paddr[cur_frag + 1], len,
					   cur_frag + 1);
	}

	return 0;

fail_skb_frag:
	for (i = 0; i < cur_frag; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(frag);
		ath12k_core_dma_unmap_page(dp->dev, paddr[i + 1], len,
					   DMA_TO_DEVICE);
	}
	ath12k_core_dma_unmap_single(dp->dev, paddr[0], hlen, DMA_TO_DEVICE);
fail_skb_head:
	DP_STATS_INC(dp_vif, tx_i.sg_dma_map_err, 1, ring_id);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_sg_ext_desc_populate);

#ifndef CPTCFG_QCN_EXTN
bool ath12k_dp_tx_dma_map(struct ath12k_dp *dp,
			  struct sk_buff *skb, u32 len,
			  struct ath12k_tx_desc_info *tx_desc,
			  struct ath12k_dp_tx_msdu_info *msdu_info,
			  struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	dma_addr_t paddr;

	paddr = dma_map_single(dp->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(dp->dev, paddr))) {
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		return false;
	}

	msdu_info->paddr = paddr;
	tx_desc->paddr = paddr;
	tx_desc->flags = 0;

	return true;
}
EXPORT_SYMBOL(ath12k_dp_tx_dma_map);
#endif

void ath12k_dp_tx_sg_unmap_buf(struct ath12k_dp *dp,
			       struct ath12k_dp_ext_desc *ext_desc,
			       struct sk_buff *skb)
{
	dma_addr_t paddr;
	u32 nr_frags, i;
	u16 len;

	ath12k_dp_ext_desc_get_buf0(ext_desc, &paddr, &len);
	ath12k_core_dma_unmap_single(dp->dev, paddr, len, DMA_TO_DEVICE);

	nr_frags = skb_shinfo(skb)->nr_frags;
	for (i = 1; i <= nr_frags; i++) {
		ath12k_dp_ext_desc_get_buf(ext_desc, &paddr, &len, i);
		ath12k_core_dma_unmap_page(dp->dev, paddr, len, DMA_TO_DEVICE);
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_sg_unmap_buf);

enum hal_tcl_encap_type
ath12k_dp_tx_get_encap_type(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		return HAL_TCL_ENCAP_TYPE_RAW;

	if (tx_info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
		return HAL_TCL_ENCAP_TYPE_ETHERNET;

	return HAL_TCL_ENCAP_TYPE_NATIVE_WIFI;
}
EXPORT_SYMBOL(ath12k_dp_tx_get_encap_type);

void ath12k_dp_tx_drop_tid_stats(struct ath12k_dp_vif *dp_vif,
				 enum ath12k_dp_tx_enq_error drop_reason,
				 u8 tid, u32 len)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	enum ath12k_tx_drop_reasons drop;

	switch (drop_reason) {
	case DP_TX_ENQ_DROP_SW_DESC_NA:
		drop = ATH_TX_BUF_ERR;
		break;
	case DP_TX_ENQ_DROP_TCL_DESC_NA:
		drop = ATH_TX_DESC_NA_ERR;
		break;
	default:
		drop = ATH_TX_MISC_FAIL;
	}

	ath12k_tid_tx_drop_stats(ahvif, tid, len, drop);
}
EXPORT_SYMBOL(ath12k_dp_tx_drop_tid_stats);

void ath12k_dp_tx_drop_pdev_tid_stats(struct ath12k_pdev_dp *dp_pdev,
				      enum ath12k_dp_tx_enq_error drop_reason,
				      u8 tid, u8 ring_id)
{
	tid = ath12k_vow_tid_validate(tid);

	switch (drop_reason) {
	case DP_TX_ENQ_DROP_SW_DESC_NA:
		DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring_id, tid,
					  swdrop_cnt, DP_TID_TX_DESC_ERR);
		break;
	case DP_TX_ENQ_DROP_DMA_ERR:
		DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring_id, tid,
					  swdrop_cnt, DP_TID_TX_DMA_MAP_ERR);
		break;
	case DP_TX_ENQ_DROP_HW_ENQ_FAIL:
		DP_PDEV_TID_TX_REASON_INC(dp_pdev, ring_id, tid,
					  swdrop_cnt, DP_TID_TX_HW_ENQUEUE);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_drop_pdev_tid_stats);

void ath12k_dp_tx_stats_update_pre_enqueue(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_vif *dp_vif,
					   struct sk_buff *skb,
					   u8 ring_id, u32 len)
{
	/* --- STAGE 1: FAST PATH(Basic Stats) --- */
	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, len, ring_id);

	/* --- STAGE 2: GATEKEEPER (Global Knob) --- */
	if (likely(!ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_STATS)))
		return;

	/* --- STAGE 3: SLOW PATH (Debug Stats) --- */
	if (ath12k_proto_stats_enabled(dp_pdev)) {
		ath12k_dp_update_proto_stats_vif(dp_vif, 0, skb,
						 TX_RECV_FROM_STACK, ring_id);
	}

	if (ath12k_dp_latency_stats_enabled(dp_pdev) ||
	    ath12k_dp_vow_stats_enabled(dp_pdev) ||
	    unlikely(skb->mark & SDWF_VALID_MASK))
		__net_timestamp(skb);
}
EXPORT_SYMBOL(ath12k_dp_tx_stats_update_pre_enqueue);

void ath12k_dp_tx_stats_post_enqueue(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_vif *dp_vif,
				     struct sk_buff *skb,
				     struct ath12k_dp_tx_msdu_info *msdu_info,
				     u8 ring_id, u32 len, bool is_mcast,
				     struct ath12k_tx_desc_info *tx_desc)
{
	struct ath12k_vif *ahvif = NULL;
	enum hal_tcl_desc_type type;
	enum hal_tcl_encap_type encap_type;
	enum hal_encrypt_type encrypt_type;
	struct ethhdr *eth;
	struct ieee80211_hdr *hdr = NULL;
	struct ath12k_skb_cb *skb_cb = NULL;
	struct ath12k_base *ab = dp->ab;
	size_t hdrlen;
	u8 tid = 0, subtype = 0;

	DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw, 1, len, ring_id);

	if (is_mcast && !msdu_info->me_convert)
		dp->device_stats.tx_mcast[ring_id]++;
	else
		dp->device_stats.tx_unicast[ring_id]++;

	/* --- STAGE 2: GATEKEEPER (Global Knob) */
	if (!ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_STATS))
		return;

	/* --- STAGE 3: SLOW PATH (Debug Stats) --- */

	/* Update EAPOL stats */
	encap_type = ath12k_dp_tx_get_encap_type(ab, skb);

	if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
		dp->device_stats.tx_eapol[ring_id]++;
		if (encap_type == HAL_TCL_ENCAP_TYPE_NATIVE_WIFI) {
			hdr = (struct ieee80211_hdr *)skb->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(skb);
			subtype = ath12k_dp_get_eapol_subtype
				(skb->data + hdrlen + LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				u8 idx = subtype - 1;

				dp->device_stats.tx_eapol_type[idx][ring_id]++;
				ath12k_dbg(ab, ATH12K_DBG_EAPOL,
					   "Transmit %s%d EAPOL frame to STA %pM\n",
					   subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   hdr->addr1);
			}
		} else {
			eth = (struct ethhdr *)skb->data;
			subtype = ath12k_dp_get_eapol_subtype(skb->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
				u8 idx = subtype - 1;

				dp->device_stats.tx_eapol_type[idx][ring_id]++;
				ath12k_dbg(ab, ATH12K_DBG_EAPOL,
					   "Transmit %s%d EAPOL frame to STA %pM\n",
					   subtype <= 4 ? "M" : "G",
					   subtype <= 4 ? subtype : (subtype - 4),
					   eth->h_dest);
			}
		}
	}

	/* SG packets & bytes ingress */
	if (msdu_info->ext_desc.ext_feature & DP_EXT_SG)
		DP_STATS_INC_PKT(dp_vif, tx_i.sg_pkt, 1, len, ring_id);

	/* Update Tx Debug stats */
	if (ath12k_dp_debug_stats_enabled(dp_pdev)) {
		skb_cb = ATH12K_SKB_CB(skb);
		encrypt_type = ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

		if (encap_type < HAL_TCL_ENCAP_TYPE_MAX)
			DP_STATS_INC(dp_vif, tx_i.encap_type[encap_type], 1, ring_id);

		if (encrypt_type < HAL_ENCRYPT_TYPE_MAX)
			DP_STATS_INC(dp_vif, tx_i.encrypt_type[encrypt_type], 1, ring_id);

		if (msdu_info && msdu_info->type < DP_TCL_DESC_TYPE_MAX) {
			type = msdu_info->type;
			DP_STATS_INC(dp_vif, tx_i.desc_type[type], 1, ring_id);
		}

		if (is_mcast) {
			DP_STATS_INC_PKT(dp_vif, tx_i.mcast, 1, skb->len,
					 ring_id);
			eth = (struct ethhdr *)skb->data;
			if (eth && is_broadcast_ether_addr(eth->h_dest))
				tx_desc->flags |= DP_TX_DESC_FLAG_BCAST;
			else
				tx_desc->flags |= DP_TX_DESC_FLAG_MCAST;
		}
	}

	/* Update Protocol stats */
	if (ath12k_proto_stats_enabled(dp_pdev)) {
		ath12k_dp_update_proto_stats_vif(dp_vif, 0, skb,
						 TX_ENQUEUE_HW,
						 ring_id);
	}

	/* Update tid stats */
	if (ath12k_tid_stats_enabled(dp_pdev)) {
		ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
		ath12k_tid_tx_stats(ahvif, tid, skb->len,
				    ATH_TX_UNICAST_PKTS);
	}

	if (ath12k_dp_latency_stats_enabled(dp_pdev) ||
	    ath12k_dp_vow_stats_enabled(dp_pdev))
		tx_desc->hw_enqueue_tstamp = (u32)ktime_to_us(ktime_get_real());
}
EXPORT_SYMBOL(ath12k_dp_tx_stats_post_enqueue);
