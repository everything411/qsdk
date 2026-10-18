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
#include "../hw.h"
#include "../dp_rx.h"
#include "../debugfs_htt_stats.h"
#include "../dp_tx.h"
#include "../dp_mon.h"
#include "hal_rx.h"
#include "dp_rx.h"
#include "hal_qcn9625.h"
#include "../debugfs.h"
#include "../dp.h"
#include "dp.h"

#define ATH12K_DP_RX_FRAGMENT_TIMEOUT_MS (2 * HZ)

static int ath12k_wifi8_dp_rx_h_cmp_frags(struct ath12k_base *ab,
					  struct sk_buff *a, struct sk_buff *b)
{
	int frag1, frag2;

	frag1 = ath12k_wifi8_dp_rx_h_frag_no(ab, a);
	frag2 = ath12k_wifi8_dp_rx_h_frag_no(ab, b);

	return frag1 - frag2;
}

static void ath12k_wifi8_dp_rx_h_sort_frags(struct ath12k_base *ab,
					    struct sk_buff_head *frag_list,
					    struct sk_buff *cur_frag)
{
	struct sk_buff *skb;
	int cmp;

	skb_queue_walk(frag_list, skb) {
		cmp = ath12k_wifi8_dp_rx_h_cmp_frags(ab, skb, cur_frag);
		if (cmp < 0)
			continue;
		__skb_queue_before(frag_list, skb, cur_frag);
		return;
	}
	__skb_queue_tail(frag_list, cur_frag);
}

static u64 ath12k_wifi8_dp_rx_h_get_pn(struct ath12k_dp *dp, struct sk_buff *skb)
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
ath12k_wifi8_dp_rx_h_defrag_validate_incr_pn(struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_dp_rx_tid *rx_tid,
					     enum hal_encrypt_type encrypt_type)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
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

	last_pn = ath12k_wifi8_dp_rx_h_get_pn(dp, first_frag);
	skb_queue_walk(&rx_tid->rx_frags, skb) {
		if (skb == first_frag)
			continue;

		cur_pn = ath12k_wifi8_dp_rx_h_get_pn(dp, skb);
		if (cur_pn != last_pn + 1)
			return false;
		last_pn = cur_pn;
	}
	return true;
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

	ath12k_wifi8_dp_rx_desc_get_dot11_hdr(dp, rx_desc, &hdr);
	hdr_len = ieee80211_hdrlen(hdr.frame_control);

	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		crypto_len = ath12k_dp_rx_crypto_param_len(dp, enctype);
		crypto_hdr = skb_push(msdu, crypto_len);
		ath12k_wifi8_dp_rx_desc_get_crypto_header(ab, rx_desc,
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

static void ath12k_wifi8_dp_rx_h_undecap_eth(struct ath12k_pdev_dp *dp_pdev,
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

static int ath12k_wifi8_dp_rx_h_undecap(struct ath12k_pdev_dp *dp_pdev,
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
	u8 is_mcbc;
	struct ieee80211_vif *vif;

	ath12k_wifi8_dp_extract_rx_desc_data(dp, &rx_desc_data, desc, desc);
	switch (tlv_info->decap) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		pkt_reason = ATH_RX_NATIVE_WIFI_PKTS;
		ath12k_wifi8_dp_rx_h_undecap_nwifi(dp_pdev, msdu, enctype, status, desc,
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

		is_mcbc = rx_msdu_info->ra_is_mcbc;
		/* mac80211 allows fast path only for authorized STA */
		if (ehdr->h_proto == cpu_to_be16(ETH_P_PAE) ||
		    enctype == HAL_ENCRYPT_TYPE_TKIP_MIC) {
			ath12k_wifi8_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
			break;
		}

		/* Drop the 3addr da_mcbc packets if allow_3addr_mc is not set
		 * for 4addr sta as it will double the packet for connected clients.
		 */
#ifdef CPTCFG_QCN_EXTN
		if (peer && peer->vif) {
			ahvif = ath12k_vif_to_ahvif(peer->vif);
			dp_vif = &ahvif->dp_vif;
			allow_3addr_mc = dp_vif->dp_extn.allow_3addr_mc;
		}
#endif

		if (is_4addr_sta && rx_msdu_info->da_is_mcbc &&
		    !rx_msdu_info->to_ds && !allow_3addr_mc) {
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				dp_peer = ath12k_dp_peer_find_by_peerid_index(dp,
									      dp_pdev,
									      peer_id);
				if (dp_peer) {
					vif = ath12k_dp_peer_get_vif(dp_peer);
					ahvif = ath12k_vif_to_ahvif(vif);
					ath12k_tid_drop_rx_stats(ahvif, rx_desc_data.tid,
								 msdu->len,
								 ATH_RX_3AADR_DUP);
				}
				rcu_read_unlock();
			}
			return -1;
		}

		if (rx_msdu_info->fr_ds && rx_msdu_info->to_ds && peer &&
		    !peer->use_4addr) {
			ath12k_wifi8_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype, status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
			break;
		}

		/* PN for mcast packets will be validated in mac80211;
		 * remove eth header and add 802.11 header.
		 */
		if (is_mcbc && decrypted)
			ath12k_wifi8_dp_rx_h_undecap_eth(dp_pdev, msdu, enctype,
							 status,
							 tlv_info->mesh_ctrl_present,
							 desc, is_mcbc, tid);
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

static int ath12k_wifi8_dp_rx_h_verify_tkip_mic(struct ath12k_pdev_dp *dp_pdev,
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

	ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev, rxs, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return -EINVAL;

	ret = ath12k_wifi8_dp_rx_h_undecap(dp_pdev, msdu, rx_desc,
					   HAL_ENCRYPT_TYPE_TKIP_MIC, rxs, true, false,
					   &rx_msdu_info, &tlv_info, NULL,
					   ATH12K_SKB_RXCB(msdu)->peer_id,
					   rx_desc_data->tid);
	if (unlikely(ret))
		return -EINVAL;

	ieee80211_rx(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	return -EINVAL;
}

static int ath12k_wifi8_dp_rx_h_defrag(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_peer *peer,
				       struct ath12k_dp_rx_tid *rx_tid,
				       struct sk_buff **defrag_skb,
				       enum hal_encrypt_type enctype,
				       bool decrypted,
				       struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
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

	if (ath12k_wifi8_dp_rx_h_verify_tkip_mic(dp_pdev, peer, enctype, first_frag,
						 rx_desc_data))
		first_frag = NULL;

	*defrag_skb = first_frag;
	return 0;
}

static int
ath12k_wifi8_dp_rx_h_defrag_reo_reinject(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_rx_tid *rx_tid,
					 struct sk_buff *defrag_skb,
					 struct hal_rx_spd_data *spd_desc_l)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_hal *hal = dp->hal;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)defrag_skb->data;
	struct hal_reo_entrance_ring *reo_ent_ring;
	struct hal_rx_spd_data *spd_desc;
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
	enum hal_wifi8_rx_buf_return_buf_manager idle_link_rbm = dp->idle_link_rbm;
	const void *end;
	u8 dst_ind;

	hal_rx_desc_sz = hal->hal_desc_sz;
	link_desc_banks = dp->link_desc_banks;
	spd_desc = rx_tid->desc;

	ath12k_wifi8_hal_rx_reo_ent_paddr_get(&spd_desc->buf_addr,
					      &link_paddr, &cookie);

	desc_bank = u32_get_bits(cookie, DP_LINK_DESC_BANK_MASK);

	msdu_link = (struct hal_rx_msdu_link *)(link_desc_banks[desc_bank].vaddr +
			(link_paddr - link_desc_banks[desc_bank].paddr));
	msdu0 = &msdu_link->msdu_0;
	msdu_ext_info = le32_to_cpu(msdu0->rx_msdu_ext_info.info0);
	dst_ind =
		u32_get_bits(msdu_ext_info,
			     HAL_RX_MSDU_EXT_DESC_INFO_INFO0_REO_DESTINATION_INDICATION);

	memset(msdu0, 0, sizeof(*msdu0));

	msdu_info = u32_encode_bits(1,
				    HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG) |
		    u32_encode_bits(1,
				    HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG) |
		    u32_encode_bits(0, HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION) |
		u32_encode_bits(defrag_skb->len - hal_rx_desc_sz,
				HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_LENGTH_OR_FLOW_IDX_LSB) |
		    u32_encode_bits(1, HAL_RX_MSDU_DESC_INFO_INFO0_SA_IS_VALID) |
		    u32_encode_bits(1, HAL_RX_MSDU_DESC_INFO_INFO0_DA_IS_VALID);
	msdu0->rx_msdu_info.info0 = cpu_to_le32(msdu_info);
	msdu0->rx_msdu_ext_info.info0 = cpu_to_le32(msdu_ext_info);

	len_diff = defrag_skb->len - hal_rx_desc_sz;
	/* change msdu len in hal rx desc */
	ath12k_wifi8_dp_rxdesc_set_msdu_len(ab, rx_desc, len_diff);

	end = defrag_skb->data + DP_RX_BUFFER_SIZE;
	ath12k_core_dmac_clean_range(defrag_skb->data, end);

	ATH12K_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr);
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

	mpdu_info = u32_encode_bits(1, HAL_RX_MPDU_DESC_INFO_INFO0_MSDU_COUNT) |
		    u32_encode_bits(0, HAL_RX_MPDU_DESC_INFO_INFO0_FRAGMENT_FLAG) |
		    u32_encode_bits(1, HAL_RX_MPDU_DESC_INFO_INFO0_RAW_MPDU) |
	       u32_encode_bits(1,
			       HAL_RX_MPDU_DESC_INFO_INFO0_PN_FIELDS_CONTAIN_VALID_INFO) |
		    u32_encode_bits(rx_tid->tid, HAL_RX_MPDU_DESC_INFO_INFO0_TID);

	reo_ent_ring->rx_reo_mpdu_info.info0 = cpu_to_le32(mpdu_info);
	reo_ent_ring->rx_reo_mpdu_info.peer_meta_data =
			spd_desc->rx_mpdu_info.peer_meta_data;

	if (ab->hw_params->reoq_lut_support) {
		reo_ent_ring->rx_reo_queue_desc_addr_31_0 =
			spd_desc->rx_mpdu_info.peer_meta_data;
		queue_addr_hi = 0;
	} else {
		reo_ent_ring->rx_reo_queue_desc_addr_31_0 =
			cpu_to_le32(lower_32_bits(rx_tid->paddr));
		queue_addr_hi = upper_32_bits(rx_tid->paddr);
	}

	reo_ent_ring->info0 =
	      le32_encode_bits(queue_addr_hi,
			       HAL_REO_ENTRANCE_RING_INFO0_RX_REO_QUEUE_DESC_ADDR_39_32) |
	      le32_encode_bits(dst_ind,
			       HAL_REO_ENTRANCE_RING_INFO0_REO_DESTINATION_INDICATION);

	reo_ent_ring->info1 =
		le32_encode_bits(rx_tid->cur_sn,
				 HAL_REO_ENTRANCE_RING_INFO1_MPDU_SEQUENCE_NUMBER);
	dest_ring_info0 = spd_desc_l->rx_mpdu_info.src_link_id;
	reo_ent_ring->rx_reo_mpdu_info.info1 =
		cpu_to_le32(u32_get_bits(dest_ring_info0,
					 HAL_RX_REO_MPDU_DESC_INFO_INFO1_SRC_LINK_ID));

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
	ATH12K_DMA_UNMAP_SINGLE(ab->dev, buf_paddr, DP_RX_BUFFER_SIZE,
				DMA_FROM_DEVICE);
	return ret;
}

int ath12k_wifi8_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action)
{
	struct hal_wbm_release_ring *desc;
	struct ath12k_base *ab;
	struct hal_srng *srng;
	int ret = 0;

	dp = ath12k_get_central_dp(dp);
	ab = dp->ab;

	srng = &ab->hal.srng_list[dp->wbm_desc_rel_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!desc) {
		ret = -ENOBUFS;
		goto exit;
	}

	ath12k_wifi8_hal_rx_msdu_link_desc_set(ab, desc, buf_addr_info, action);

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return ret;
}

static int ath12k_wifi8_dp_rx_frag_h_mpdu(struct ath12k_pdev_dp *dp_pdev,
					  struct sk_buff *msdu,
					  struct hal_rx_spd_data *spd_desc_l,
					  struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
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

	frag_no = ath12k_wifi8_dp_rx_h_frag_no(ab, msdu);
	more_frags = ath12k_wifi8_dp_rx_h_more_frags(ab, msdu);
	seqno = rx_desc_data->seq_no;

	peer_id = ath12k_wifi8_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
						 spd_desc_l->rx_mpdu_info.peer_meta_data);
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
		spin_unlock_bh(&rx_tid->tid_lock);
		ret = -EINVAL;
		goto out_unlock;
	}

	if ((!rx_tid->rx_frag_bitmap || frag_no > __fls(rx_tid->rx_frag_bitmap)))
		__skb_queue_tail(&rx_tid->rx_frags, msdu);
	else
		ath12k_wifi8_dp_rx_h_sort_frags(ab, &rx_tid->rx_frags, msdu);

	rx_tid->rx_frag_bitmap |= BIT(frag_no);
	if (!more_frags)
		rx_tid->last_frag_no = frag_no;

	if (frag_no == 0) {
		rx_tid->desc = kmemdup(spd_desc_l,
				       sizeof(struct hal_rx_spd_data),
				       GFP_ATOMIC);
		if (!rx_tid->desc) {
			ret = -ENOMEM;
			goto out_unlock;
		}
	} else {
		ath12k_wifi8_dp_rx_link_desc_return(dp, &spd_desc_l->buf_addr,
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

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer)
		goto err_frags_cleanup;

	if (!ath12k_wifi8_dp_rx_h_defrag_validate_incr_pn(dp_pdev, rx_tid, enctype))
		goto err_frags_cleanup;

	if (ath12k_wifi8_dp_rx_h_defrag(dp_pdev, peer, rx_tid, &defrag_skb,
					enctype, rx_desc_data->is_decrypted,
					rx_desc_data))
		goto err_frags_cleanup;

	if (!defrag_skb)
		goto err_frags_cleanup;

	if (ath12k_wifi8_dp_rx_h_defrag_reo_reinject(dp, dp_pdev, rx_tid, defrag_skb, spd_desc_l))
		goto err_frags_cleanup;

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
ath12k_wifi8_dp_process_rx_err_buf(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_spd_data *spd_desc_l,
				   struct list_head *used_list,
				   bool drop, u32 cookie)
{
	struct ath12k_dp *dp = ath12k_get_central_dp(dp_pdev->dp);
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

	/* Always use cookie to get rx desc reo desc contains link desc cookie */
	desc_info = ath12k_dp_get_rx_desc(dp, cookie);
	if (!desc_info) {
		ath12k_warn(ab, "Invalid cookie in DP rx error descriptor retrieval: 0x%x\n",
			    cookie);
		return -EINVAL;
	}

	if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC)
		ath12k_warn(ab, " RX Exception, Check HW CC implementation");

	msdu = desc_info->skb;
	desc_info->skb = NULL;
	rxcb = ATH12K_SKB_RXCB(msdu);
	rxcb->peer_id =
		ath12k_wifi8_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
					       spd_desc_l->rx_mpdu_info.peer_meta_data);

	list_add_tail(&desc_info->list, used_list);

	ath12k_core_dmac_inv_range(desc_info->vaddr,
				   desc_info->vaddr + DP_RX_BUFFER_SIZE);

	if (drop) {
		rcu_read_lock();
		peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							   rxcb->peer_id);
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
	ath12k_wifi8_dp_extract_rx_desc_data(dp, &rx_desc_data, rx_desc, rx_desc);

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							   rxcb->peer_id);
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

	if (ath12k_wifi8_dp_rx_frag_h_mpdu(dp_pdev, msdu, spd_desc_l, &rx_desc_data)) {
		dev_kfree_skb_any(msdu);
		ath12k_wifi8_dp_rx_link_desc_return(dp, &spd_desc_l->buf_addr,
						    HAL_WBM_REL_BM_ACT_PUT_IN_IDLE);
	}
exit:
	rcu_read_unlock();
	return 0;
}

static int ath12k_wifi8_dp_h_link_desc(struct ath12k_dp *dp,
				       struct hal_rx_spd_data *spd_desc_l)
{
	struct list_head rx_desc_used_list;
	struct hal_srng *refill_srng;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int cpu_id = smp_processor_id();
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	u32 msdu_cookies[HAL_NUM_RX_MSDUS_PER_LINK_DESC];
	int num_buffs_reaped = 0;
	struct dp_link_desc_bank *link_desc_banks;
	enum hal_wifi8_rx_buf_return_buf_manager rbm;
	struct hal_rx_msdu_link *link_desc_va;
	int i;
	u8 hw_link_id;
	u32 desc_bank, num_msdus;
	struct ath12k_pdev_dp *dp_pdev;
	dma_addr_t paddr;
	bool is_frag, drop = false;
	enum hal_wbm_rel_bm_act act;
	u32 cookie;

	INIT_LIST_HEAD(&rx_desc_used_list);

	//TODO add stats for link descriptors
	dp->device_stats.err_ring_pkts++;

	hw_link_id = spd_desc_l->rx_mpdu_info.src_link_id;

	ath12k_wifi8_hal_rx_reo_ent_paddr_get(&spd_desc_l->buf_addr,
					      &paddr, &cookie);

	desc_bank = u32_get_bits(cookie, DP_LINK_DESC_BANK_MASK);

	link_desc_banks = dp->link_desc_banks;
	link_desc_va = link_desc_banks[desc_bank].vaddr +
		       (paddr - link_desc_banks[desc_bank].paddr);
	ath12k_wifi8_hal_rx_msdu_link_info_get(link_desc_va, &num_msdus,
					       msdu_cookies, &rbm);

	if (rbm != dp->idle_link_rbm &&
	    rbm != dp->hal->hal_params->rx_buf_rbm) {
		act = HAL_WBM_REL_BM_ACT_REL_MSDU;
		dp->device_stats.invalid_rbm++;
		ath12k_warn(ab, "invalid return buffer manager %d\n", rbm);
		ath12k_wifi8_dp_rx_link_desc_return(dp,
						    &spd_desc_l->buf_addr,
						    act);
		goto exit;
	}

	is_frag = !!spd_desc_l->rx_mpdu_info.fragment_flag;

	/* Process only rx fragments with one msdu per link desc below, and drop
	 * msdu's indicated due to error reasons.
	 * Dynamic fragmentation not supported in Multi-link client, so drop the
	 * partner device buffers.
	 */
	if (!is_frag || num_msdus > 1) {
		drop = true;
		act = HAL_WBM_REL_BM_ACT_PUT_IN_IDLE;

		/* Return the link desc back to wbm idle list */
		ath12k_wifi8_dp_rx_link_desc_return(dp,
						    &spd_desc_l->buf_addr,
						    act);
	}

	rcu_read_lock();

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
	if (unlikely(!dp_pdev)) {
		rcu_read_unlock();
		goto exit;
	}

	if (drop)
		dp_pdev->wmm_stats.total_wmm_rx_drop[dp_pdev->wmm_stats.rx_type]++;

	for (i = 0; i < num_msdus; i++) {
		if (!ath12k_wifi8_dp_process_rx_err_buf(dp_pdev, spd_desc_l,
							&rx_desc_used_list,
							drop,
							msdu_cookies[i])) {
			num_buffs_reaped++;
		}
	}

	rcu_read_unlock();

	refill_srng = &ab->hal.srng_list[dp_wifi8->wbm_refill_ring[cpu_id %
					DP_WBM_REFILL_RING_MAX].ring_id];

	ath12k_dp_rx_bufs_replenish(dp, refill_srng, &rx_desc_used_list, false);

exit:
	return num_buffs_reaped;
}

void ath12k_wifi8_convert_n_deliver_nw_frame(struct ath12k_pdev_dp *dp_pdev,
					     struct hal_rx_spd_data *rx_spd,
					     struct ath12k_dp_peer *peer,
					     struct ieee80211_rx_status *status,
					     struct napi_struct *napi,
					     struct rx_tlv_info_1 *prev_tlv_info)
{
	struct ieee80211_rx_status *rx_status;
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ieee80211_sta *pubsta = NULL;
	u8 *rx_tlv_hdr;
	bool ret;

	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;

	rx_tlv_hdr = rx_spd->vaddr;

	status->flag |= RX_FLAG_DECRYPTED |
			RX_FLAG_MMIC_STRIPPED |
			RX_FLAG_IV_STRIPPED |
			RX_FLAG_SKIP_MONITOR |
			RX_FLAG_DUP_VALIDATED;

	pubsta = peer->sta;
	if (pubsta && pubsta->valid_links) {
		u8 ln = rx_mpdu_info->src_link_id;

		status->link_valid = 1;
		status->link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(peer, ln);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi8_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
					HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return;
	}

	ath12k_wifi8_dp_rx_h_undecap_eth(dp_pdev, msdu, peer->sec_type,
					 status,
					 tlv_info->mesh_ctrl_present,
					 (struct hal_rx_desc *)rx_tlv_hdr,
					 rx_msdu_info->da_is_mcbc,
					 rx_mpdu_info->tid);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	ath12k_dp_rx_update_eapol_stats(dp_pdev->dp, msdu);

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
}

static void ath12k_wifi8_wbm_process_frame(struct ath12k_pdev_dp *dp_pdev,
					   struct hal_rx_spd_data *spd_desc_l,
					   struct ath12k_dp_peer *peer,
					   struct ieee80211_rx_status *rx_status,
					   struct napi_struct *napi,
					   struct rx_tlv_info_1 *prev_tlv_info)
{
	struct link_peer_rx_tid_stats stats;

	switch (spd_desc_l->tlv_info.decap) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_wifi8_deliver_nwifi_frame(dp_pdev, spd_desc_l,
						 peer, rx_status,
						 napi, &stats,
						 prev_tlv_info);
		break;
	case DP_RX_DECAP_TYPE_RAW:
		ath12k_wifi8_deliver_raw_frame(dp_pdev, spd_desc_l,
					       peer, rx_status,
					       napi, &stats,
					       prev_tlv_info);
		break;
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		ath12k_wifi8_deliver_ethernet_frame(dp_pdev, spd_desc_l,
						    peer, rx_status,
						    napi, &stats,
						    prev_tlv_info);
		break;
	}
}

static bool ath12k_wifi8_dp_tkip_mic_err(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_peer *peer,
					 struct ieee80211_rx_status *rx_status,
					 struct hal_rx_spd_data *spd_desc_l,
					 struct napi_struct *napi,
					 struct rx_tlv_info_1 *prev_tlv_info)
{
	struct link_peer_rx_tid_stats stats;
	int ret;

	ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev, rx_status, &spd_desc_l->tlv_info,
					HAL_WBM_REL_SRC_MODULE_RXDMA);
	if (unlikely(ret))
		return true;

	rx_status->flag |= RX_FLAG_MMIC_ERROR;

	ath12k_wifi8_deliver_raw_frame(dp_pdev, spd_desc_l,
				       peer, rx_status,
				       napi, &stats,
				       prev_tlv_info);

	return false;
}

static bool ath12k_wifi8_handle_reo_route(struct ath12k_pdev_dp *dp_pdev,
					  struct ath12k_dp_peer *peer,
					  struct ieee80211_rx_status *rx_status,
					  struct hal_rx_spd_data *spd_desc_l,
					  struct napi_struct *napi,
					  struct rx_tlv_info_1 *prev_tlv_info)
{
	struct ath12k *ar = dp_pdev->ar;

	if (!peer)
		return true;

	switch (peer->rx_decap_type) {
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		/*
		 * Convert eapol packets into 80211 packets as mac80211
		 * needs unautorized packets in 80211 format.
		 * Allow all other ERP/other routed packets through
		 * regular path.
		 */
		if (spd_desc_l->cce_metadata == ATH12K_ROUTE_EAP_METADATA) {
			ath12k_wifi8_convert_n_deliver_nw_frame(dp_pdev, spd_desc_l,
								peer, rx_status,
								napi,
								prev_tlv_info);
			break;
		}
		fallthrough;
	case DP_RX_DECAP_TYPE_RAW:
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_dp_rx_update_eapol_stats(dp_pdev->dp, spd_desc_l->msdu);
		ath12k_wifi8_wbm_process_frame(dp_pdev, spd_desc_l,
					       peer, rx_status,
					       napi,
					       prev_tlv_info);
		break;
	default:
		return true;
	}

	if (ar->erp_trigger_set)
		queue_work(ar->ab->workqueue, &ar->erp_handle_trigger_work);

	return false;
}

bool ath12k_wifi8_handle_null_queue(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_peer *peer,
				    struct ieee80211_rx_status *rx_status,
				    struct hal_rx_spd_data *spd_desc_l,
				    struct napi_struct *napi,
				    struct rx_tlv_info_1 *prev_tlv_info)

{
	struct rx_msdu_desc_info *rx_msdu_info = &spd_desc_l->rx_msdu_info;
	bool is_mcbc = rx_msdu_info->da_is_mcbc;
	bool is_4addr_sta;
	bool to_ds = rx_msdu_info->to_ds;
	bool fr_ds = rx_msdu_info->fr_ds;

	ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_RX,
		   "null_q_desc mcbc: peer_id=%u da_is_mcbc=%u is_decrypted=%u decap=%u",
		   peer ? peer->peer_id : ATH12K_PEER_ID_INVALID, is_mcbc,
		   spd_desc_l->tlv_info.is_decrypted,
		   spd_desc_l->tlv_info.decap);

	switch (spd_desc_l->tlv_info.decap) {
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		if (!peer)
			return true;
		is_4addr_sta = peer->vdev_type_4addr & BIT(NL80211_IFTYPE_STATION);
		if (is_4addr_sta && is_mcbc && !to_ds)
			return true;

		if (peer)
			is_mcbc = is_mcbc && !peer->is_reset_mcbc;

		if ((fr_ds && to_ds && peer && !peer->use_4addr) || is_mcbc) {
			ath12k_wifi8_convert_n_deliver_nw_frame(dp_pdev,
								spd_desc_l,
								peer, rx_status,
								napi,
								prev_tlv_info);
			break;
		}
		fallthrough;
	case DP_RX_DECAP_TYPE_RAW:
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_wifi8_wbm_process_frame(dp_pdev, spd_desc_l,
					       peer, rx_status,
					       napi,
					       prev_tlv_info);
		break;
	default:
		return true;
	}

	return false;
}

static bool ath12k_wifi8_dp_unauth_wds_err(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_peer *peer,
					   struct ieee80211_rx_status *rx_status,
					   struct hal_rx_spd_data *spd_desc_l,
					   struct napi_struct *napi,
					   struct rx_tlv_info_1 *prev_tlv_info)
{
	struct sk_buff *msdu = spd_desc_l->msdu;
	struct ath12k_dp *dp = dp_pdev->dp;

	if (!peer) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
			   "failed to find the peer to process unauth wds err handling");
		return true;
	}

	switch (peer->rx_decap_type) {
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		struct ethhdr *ehdr = (struct ethhdr *)msdu->data;

		if (ehdr->h_proto != cpu_to_be16(ETH_P_PAE))
			return true;

		ath12k_wifi8_convert_n_deliver_nw_frame(dp_pdev, spd_desc_l,
							peer, rx_status,
							napi,
							prev_tlv_info);
		return false;
	case DP_RX_DECAP_TYPE_RAW:
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)msdu->data;
		int hdr_len = ieee80211_hdrlen(hdr->frame_control);
		struct ath12k_dp_rx_rfc1042_hdr *llc =
			(struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);
		bool is_null = ieee80211_is_qos_nullfunc(hdr->frame_control);

		if (!(llc->snap_type == cpu_to_be16(ETH_P_PAE) || is_null))
			return true;

		ath12k_dp_rx_update_eapol_stats(dp, msdu);
		break;
	default:
		return true;
	}

	ath12k_wifi8_wbm_process_frame(dp_pdev, spd_desc_l,
				       peer, rx_status,
				       napi,
				       prev_tlv_info);

	return false;
}

static void
ath12k_wifi8_dp_process_reo_rx_err_packets(struct ath12k_dp *dp,
					   struct napi_struct *napi,
					   struct hal_rx_spd_data *rx_spd,
					   int num_msdus)
{
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 prev_tlv = {0};
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct sk_buff *msdu;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	struct ath12k_base *partner_ab;
	struct ath12k_dp *partner_dp;
	struct ath12k_vif *ahvif = NULL;
	u8 hw_link_id, pdev_id;
	int msdu_idx = 0;
	u32 drop_reason, error_code;
	bool drop;
	bool vow_stats_needed = false;
	bool fse_tagged;
	struct ath12k_hal *hal = dp->hal;
	u32 hal_rx_desc_sz = hal->hal_desc_sz;
	u16 msdu_len;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_pdev_dp *dp_pdev;
	u16 peer_id = 0;
	u8 tid = 0;
	u32 peer_metadata;
	struct hal_rx_desc *rx_desc;
	int reason, device_id, pdev_idx;

	rcu_read_lock();

	for (msdu_idx = 0; msdu_idx < num_msdus; msdu_idx++) {
		struct hal_rx_spd_data *spd_desc_l = &rx_spd[msdu_idx];
		struct ieee80211_rx_status rx_status = {0};
		enum hal_wbm_rel_src_module src;
		bool stats_needed = false;

		rx_msdu_info = &spd_desc_l->rx_msdu_info;
		rx_mpdu_info = &spd_desc_l->rx_mpdu_info;

		if (rx_mpdu_info->reo_dest_buffer_type ==
				HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC) {
			ath12k_wifi8_dp_h_link_desc(dp, spd_desc_l);
			continue;
		}

		src = rx_mpdu_info->release_source_module;

		/*
		 * In wifi8, the default src is always REO.
		 * Hence, to handle RXDMA errors, if the
		 * rxdma push reason is error, then change the
		 * source to RXDMA.
		 */
		if (rx_mpdu_info->rxdma_push_reason !=
			HAL_RXDMA_PUSH_REASON_ROUTING_INSTRUCTION) {
			src = HAL_WBM_REL_SRC_MODULE_RXDMA;
		}

		fse_tagged = false;
		drop = ath12k_dp_err_drop_needed(src,
						 rx_mpdu_info->reo_push_reason,
						 rx_mpdu_info->reo_error_code,
						 rx_mpdu_info->rxdma_push_reason,
						 rx_mpdu_info->rxdma_error_code);
		if (!drop)
			ath12k_wifi8_dp_adjust_skb(spd_desc_l, NULL,
						   &msdu_idx, hal_rx_desc_sz);

		rx_desc = (struct hal_rx_desc *)spd_desc_l->vaddr;
		ath12k_wifi8_dp_extract_rx_spd_data(hal, spd_desc_l, rx_desc);

		if (drop) {
			dev_kfree_skb_any(spd_desc_l->msdu);
			spd_desc_l->msdu = NULL;
		}

		hw_link_id = rx_mpdu_info->src_link_id;
		device_id = hw_links[hw_link_id].device_id;
		pdev_idx = hw_links[hw_link_id].pdev_idx;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      pdev_idx);

		dp->device_stats.rx_wbm_rel_source[src][device_id]++;

		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (unlikely(!dp_pdev)) {
			if (spd_desc_l->msdu) {
				dev_kfree_skb_any(spd_desc_l->msdu);
				spd_desc_l->msdu = NULL;
			}
			continue;
		}

		partner_ab = partner_dp->ab;
		if (!rcu_dereference(partner_ab->pdevs_active[pdev_id])) {
			if (spd_desc_l->msdu) {
				dev_kfree_skb_any(spd_desc_l->msdu);
				spd_desc_l->msdu = NULL;
			}
			continue;
		}

		peer_metadata = rx_mpdu_info->peer_meta_data;
		peer_id = ath12k_wifi8_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
							 peer_metadata);
		tid = rx_mpdu_info->tid;
		msdu_len = rx_msdu_info->msdu_length;

		peer = ath12k_dp_peer_find_by_peerid_index(partner_dp,
							   dp_pdev, peer_id);
		if (peer)
			ahvif = ath12k_vif_to_ahvif(peer->vif);

		hw_link_id = ath12k_dp_validate_hw_link_id(hw_link_id);
		spd_desc_l->rx_mpdu_info.src_link_id = hw_link_id;

		msdu = spd_desc_l->msdu;

		if (likely(ahvif)) {
			ahvif->wmm_stats.rx_type = dp_pdev->wmm_stats.rx_type;
			ahvif->wmm_stats.total_wmm_rx_pkts[ahvif->wmm_stats.rx_type]++;
		}

		if (ath12k_dp_stats_enabled(dp_pdev)) {
			if (peer && ahvif && ath12k_tid_stats_enabled(dp_pdev))
				stats_needed = true;

			if (ath12k_dp_vow_stats_enabled(dp_pdev))
				vow_stats_needed = true;
		}

		if (stats_needed) {
			int pkt_rsn = ath12k_dp_get_rx_frame_type(peer->rx_decap_type);

			ath12k_tid_rx_stats(ahvif, tid, msdu_len, pkt_rsn);
			ath12k_tid_rx_stats(ahvif, tid, msdu_len, ATH_RX_TOTAL_PKTS);
			ath12k_tid_rx_stats(ahvif, tid, msdu_len, ATH_RX_WBM_REL_TOTAL);
		}

		if (!drop) {
			struct dp_rx_tag_params tag = {
				.cce_match        = spd_desc_l->cce_match,
				.cce_metadata     = spd_desc_l->cce_metadata,
				.flow_idx_invalid = rx_mpdu_info->flow_idx_invalid,
				.flow_idx_timeout = rx_mpdu_info->flow_idx_timeout,
				.flow_metadata    =
					rx_mpdu_info->flow_info.flow_metadata,
			};

			if (tag.cce_match && dp_pdev->protocol_tag_active_count)
				dp_rx_update_protocol_tag(dp_pdev, msdu, &tag);
			dp_rx_update_flow_tag(dp_pdev, msdu, &tag);
		}

		if (src == HAL_WBM_REL_SRC_MODULE_REO) {
			if (rx_mpdu_info->reo_push_reason ==
				HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
				dp->device_stats.wbm_err.hal_reo_route++;
				fse_tagged = !rx_mpdu_info->flow_idx_invalid &&
					     !rx_mpdu_info->flow_idx_timeout;
				drop = ath12k_wifi8_handle_reo_route(dp_pdev, peer,
								     &rx_status,
								     spd_desc_l,
								     napi,
								     &prev_tlv);
				if (drop)
					drop_reason = ATH_RX_INVALID_RBM;
			} else if (rx_mpdu_info->reo_push_reason ==
					HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED) {
				/* flow_idx is always invalid on REO error frames;
				 * fse_tagged stays false (initialized at loop top)
				 */
				error_code = rx_mpdu_info->reo_error_code;
				if (error_code ==
					HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO) {
					drop = ath12k_wifi8_handle_null_queue(dp_pdev,
									      peer,
									      &rx_status,
									      spd_desc_l,
									      napi,
									      &prev_tlv);

					if (drop)
						drop_reason = ATH_RX_NULL_Q_DESC;
				} else {
					reason = WBM_ERR_DROP_REO_GENERIC;

					DP_DEVICE_STATS_INC(dp, wbm_err.drop[reason], 1);
					drop_reason =
						ath12k_fill_reo_drop_reason(error_code);
				}

				DP_DEVICE_STATS_INC(dp, wbm_err.reo_error[error_code], 1);
				DP_PEER_LINK_STATS_CNT(peer,
						       wbm_err.reo_error[error_code], 1,
						       hw_link_id);

				if (vow_stats_needed)
					ath12k_dp_tid_wbm_err_stats(dp_pdev, tid, true,
								    error_code);
			} else {
				reason = WBM_ERR_DROP_INVALID_PUSH_REASON;
				ath12k_dp_rx_wbm_err_dev_free_skb(dp, msdu, reason);
				continue;
			}

			if (drop && msdu) {
				if (fse_tagged)
					dp_pdev->fse_cce_stats.fse_drop_pkts++;
				if (spd_desc_l->cce_match) {
					dp_pdev->fse_cce_stats.wbm_reo_cce_drop++;
					dp_pdev->fse_cce_stats.cce_drop_pkts++;
				}
				dev_kfree_skb_any(msdu);
			}

			if (stats_needed) {
				if (drop)
					ath12k_tid_drop_rx_stats(ahvif, tid,
								 msdu_len, drop_reason);
				else
					ath12k_tid_rx_stats(ahvif, tid, msdu_len,
							    ATH_RX_REO_ERR_PKTS);
			}
		} else if (src == HAL_WBM_REL_SRC_MODULE_RXDMA) {
			drop = false;

			if (rx_mpdu_info->rxdma_push_reason !=
				HAL_RXDMA_PUSH_REASON_ERR_DETECTED) {
				fse_tagged = !rx_mpdu_info->flow_idx_invalid &&
					     !rx_mpdu_info->flow_idx_timeout;
				if (fse_tagged) {
					dp_pdev->fse_cce_stats.wbm_rxdma_fse_drop++;
					dp_pdev->fse_cce_stats.fse_drop_pkts++;
				}
				if (spd_desc_l->cce_match)
					dp_pdev->fse_cce_stats.cce_drop_pkts++;
				reason = WBM_ERR_DROP_INVALID_PUSH_REASON;
				ath12k_dp_rx_wbm_err_dev_free_skb(dp, msdu,
								  reason);
				continue;
			}

			error_code = rx_mpdu_info->rxdma_error_code;

			switch (error_code) {
			case HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR:
				drop = ath12k_wifi8_dp_unauth_wds_err(dp_pdev,
								      peer,
								      &rx_status,
								      spd_desc_l,
								      napi,
								      &prev_tlv);
				if (drop)
					drop_reason = ATH_RX_UNAUTH_WDS_ERR;

				DP_DEVICE_STATS_INC(dp, wbm_err.rxdma_error[error_code],
						    1);
				DP_PEER_LINK_STATS_CNT(peer,
						       wbm_err.rxdma_error[error_code],
						       1, hw_link_id);
				break;
			case HAL_REO_ENTR_RING_RXDMA_ECODE_MULTICAST_ECHO_ERR:
				if (ahvif)
					ath12k_dp_rx_h_mec_drop(dp_pdev, ahvif,
								hw_link_id,
								peer_id);
				drop = true;
				drop_reason = ATH_RX_ECHO_ERR;
				break;
			case HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR:
				dp_pdev->stats.telemetry_stats.rx_decrypt_err++;
				drop = true;
				drop_reason = ATH_RX_TKIP_MIC_ERR;
				break;
			case HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR:
				dp_pdev->stats.telemetry_stats.rx_mic_err++;
				drop = ath12k_wifi8_dp_tkip_mic_err(dp_pdev,
								    peer,
								    &rx_status,
								    spd_desc_l,
								    napi,
								    &prev_tlv);

				drop_reason = ATH_RX_TKIP_MIC_ERR;
				break;
			case HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR:
				dp_pdev->stats.telemetry_stats.rx_over_run++;
				break;
			default:
				drop = true;
				drop_reason = ATH_RX_RXDMA_ERR;
				break;
			}

			if (vow_stats_needed)
				ath12k_dp_tid_wbm_err_stats(dp_pdev, tid, false,
							    error_code);

			/* flow_idx is always invalid on RXDMA error frames;
			 * only count CCE drops here
			 */
			if (drop && msdu) {
				if (spd_desc_l->cce_match)
					dp_pdev->fse_cce_stats.cce_drop_pkts++;
				dev_kfree_skb_any(msdu);
			}

			if (stats_needed) {
				if (drop)
					ath12k_tid_drop_rx_stats(ahvif, tid,
								 msdu_len, drop_reason);
				else
					ath12k_tid_rx_stats(ahvif, tid, msdu_len,
							    ATH_RX_RXDMA_PKTS);
			}
		}

		spd_desc_l->msdu = NULL;

		if (likely(msdu_idx + 1 < num_msdus)) {
			u8 *vaddr;
			struct hal_rx_spd_data *spd_desc_next =
				&rx_spd[msdu_idx + 1];
			struct sk_buff *next_msdu = spd_desc_next->msdu;

			/* In case of fragment packets or link descriptor released vaddr
			 * and next_msdu would not be valid avoid prefetch
			 */
			if ((spd_desc_next->rx_mpdu_info.reo_dest_buffer_type !=
			     HAL_REO_DEST_RING_BUFFER_TYPE_MSDU) || !next_msdu)
				continue;

			vaddr = spd_desc_next->vaddr;

			prefetch(vaddr);
			prefetch(&vaddr[64]);
			prefetch(&vaddr[128]);
			prefetch(next_msdu);
			prefetch(&next_msdu->_skb_refdst);
			prefetch(&next_msdu->__pkt_type_offset);
			prefetch(&next_msdu->data);
			prefetch(skb_shinfo(next_msdu));
		}
	}

	rcu_read_unlock();
}

int ath12k_wifi8_dp_rx_process_err(struct ath12k_dp *dp,
				   struct napi_struct *napi, int budget)

{
	int cpu_id = DP_REO_ERR_RINGS_MAX + smp_processor_id();
	int ring_id = dp->reo_except_ring.ring_id;
	struct hal_srng *srng = &dp->hal->srng_list[ring_id];
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct hal_rx_spd_data *rx_status_desc =
		(struct hal_rx_spd_data *)dp_hw_grp->rx_status_buf[cpu_id];
	int total_msdu_reaped =
		ath12k_wifi8_dp_rx_process_reo_rings(dp, srng, rx_status_desc,
						     0, budget, cpu_id);

	if (total_msdu_reaped)
		ath12k_wifi8_dp_process_reo_rx_err_packets(dp, napi,
							   rx_status_desc,
							   total_msdu_reaped);

	return total_msdu_reaped;
}
