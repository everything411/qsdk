// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../hif.h"
#include "mgmt_rx.h"
#include "hal.h"
#include "dp.h"
#include "hal_qcn9625.h"
#include "../peer.h"
#include "wmi.h"

static void ath12k_wifi8_mgmt_rx_ring_free(struct ath12k_base *ab);

void ath12k_mgmt_srng_hw_disable(struct ath12k_base *ab, struct mgmt_srng *ring)
{
	struct hal_srng *srng = &ab->hal.srng_list[ring->ring_id];

	ath12k_wifi8_hal_srng_hw_disable(ab, srng);
}

void ath12k_wifi8_srng_hw_mgmt_rings_disable(struct ath12k_base *ab)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(ab->mgmt);

	ath12k_mgmt_srng_hw_disable(ab, &mgmt_wifi8->reo_dst_rx_ring);
	ath12k_mgmt_srng_hw_disable(ab, &mgmt_wifi8->reo_dst_rx_err_ring);
#ifdef CPTCFG_QCN_EXTN
	ath12k_mgmt_srng_hw_disable_extn(ab);
#endif /* CPTCFG_QCN_EXTN */
}

void ath12k_wifi8_mgmt_rx_replenish_buffs(struct ath12k_mgmt *mgmt,
					  struct mgmt_srng *rx_refill_ring,
					  struct list_head *desc_used_list,
					  bool reuse)
{
	struct ath12k_base *ab = mgmt->ab;
	struct ath12k_buffer_addr *desc;
	struct sk_buff *skb;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct hal_srng *srng;
	u8 manager = mgmt->hal->hal_params->rx_mgmt_buf_rbm;
	int allocated_entries = 0;

	if (reuse) {
		/* Count entries for reuse */
		list_for_each_entry_safe(rx_desc, tmp_rx_desc, desc_used_list, list) {
			allocated_entries++;
		}
	} else {
		list_for_each_entry_safe(rx_desc, tmp_rx_desc, desc_used_list, list) {
			skb = dev_alloc_skb(MGMT_RX_BUFFER_SIZE);
			if (!skb)
				break;

			paddr = ath12k_core_dma_map_single(mgmt->dev, skb->data,
							   skb->len + skb_tailroom(skb),
							   DMA_FROM_DEVICE);
			if (unlikely(!paddr)) {
				dev_kfree_skb_any(skb);
				break;
			}

			allocated_entries++;

			ATH12K_SKB_RXCB(skb)->paddr = paddr;

			rx_desc->skb = skb;
			rx_desc->paddr = paddr;
			rx_desc->vaddr = skb->data;
			rx_desc->is_frag = 0;
			rx_desc->in_use = true;
		}
	}

	srng = &ab->hal.srng_list[rx_refill_ring->ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (allocated_entries > 0) {
		rx_desc = list_first_entry_or_null(desc_used_list,
						   struct ath12k_rx_desc_info, list);
		if (unlikely(!rx_desc))
			goto out;

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!desc))
			goto out;

		list_del(&rx_desc->list);

		allocated_entries--;
		ath12k_hal_rx_buf_addr_info_set(desc, rx_desc->paddr, rx_desc->cookie,
						manager);
	}

out:
	ath12k_hal_srng_access_end(ab, srng);

	/* add the remaining descriptors to the free_list */
	if (!list_empty(desc_used_list)) {
		struct ath12k_rx_desc_info *rx_desc, *safe;
		struct sk_buff *skb;

		list_for_each_entry_safe(rx_desc, safe, desc_used_list, list) {
			rx_desc->in_use = false;
			rx_desc->is_frag = 0;
			skb = rx_desc->skb;
			rx_desc->skb = NULL;
			ath12k_core_dma_unmap_single(mgmt->dev, rx_desc->paddr,
						     skb->len + skb_tailroom(skb),
						     DMA_FROM_DEVICE);
			if (reuse)
				skb_queue_tail(&ab->dp_umac_reset.rx_skb_queue, skb);
			else
				dev_kfree_skb_any(skb);
		}

		spin_lock_bh(&mgmt->rx_desc_lock);
		list_splice_tail(desc_used_list, &mgmt->rx_desc_free_list);
		spin_unlock_bh(&mgmt->rx_desc_lock);
	}

	spin_unlock_bh(&srng->lock);
}

#ifndef CPTCFG_QCN_EXTN
static
#endif /* !CPTCFG_QCN_EXTN */
int ath12k_wifi8_mgmt_rx_reap_packets(struct ath12k_base *ab,
				      struct mgmt_srng *ring,
				      struct sk_buff_head *mmpdu_list)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct list_head rx_desc_used_list;
	struct ath12k_rx_desc_info *desc_info;
	struct hal_reo_dest_ring *desc;
	struct ath12k_skb_rxcb *rxcb;
	int num_buffs_reaped = 0;
	u8 hw_link_id;
	struct hal_srng *srng;
	struct sk_buff *mmpdu;
	bool done = false;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif

	INIT_LIST_HEAD(&rx_desc_used_list);

	srng = &ab->hal.srng_list[ring->ring_id];

	spin_lock_bh(&srng->lock);

try_again:
	ath12k_hal_srng_access_begin(ab, srng);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = ath12k_hal_srng_dst_num_free(ab, srng, false);
	if (unlikely(!valid_entries)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_dst_invalidate_entry(ab->dp, srng, valid_entries);
#endif

	while ((desc = ath12k_hal_srng_dst_get_next_cached_entry(ab, srng, NULL))) {
		struct hal_rx_mpdu_ext_desc_info *mpdu_ext_info = &desc->rx_mpdu_ext_info;
		struct hal_rx_mpdu_desc *mpdu_info = &desc->rx_mpdu_info;
		struct hal_rx_msdu_desc *msdu_info = &desc->rx_msdu_info;
		enum hal_reo_dest_ring_push_reason push_reason;
		u32 cookie;

		hw_link_id = le32_get_bits(mpdu_ext_info->info0,
					   HAL_RX_MPDU_EXT_DESC_INFO_INFO0_SRC_LINK_ID);
		cookie = le32_get_bits(desc->buf_addr_info.info1,
				       BUFFER_ADDR_INFO1_SW_COOKIE);

		desc_info = ath12k_mgmt_get_rx_desc_from_cookie(mgmt, cookie);
		if (!desc_info) {
			ath12k_err(ab, "Unable to retrieve mgmt rx_desc for cookie 0x%x",
				   cookie);
			WARN_ON_ONCE(1);
			continue;
		}

		if (desc_info->magic != ATH12K_MGMT_RX_DESC_MAGIC)
			ath12k_warn(ab, "MGMT RX desc is tainted");

		mmpdu = desc_info->skb;
		desc_info->skb = NULL;

		list_add_tail(&desc_info->list, &rx_desc_used_list);

		rxcb = ATH12K_SKB_RXCB(mmpdu);
		ath12k_core_dma_unmap_single(mgmt->ab->dev, rxcb->paddr,
					     mmpdu->len + skb_tailroom(mmpdu),
					     DMA_FROM_DEVICE);

		num_buffs_reaped++;

		push_reason =
			le32_get_bits(mpdu_ext_info->info0,
				      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_PUSH_REASON);
		if (push_reason !=
		    HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
			mgmt->srng_stats.invalid_push_pkts++;
			dev_kfree_skb_any(mmpdu);
			continue;
		}

		if (!le32_get_bits(mpdu_ext_info->info0,
				   HAL_RX_MPDU_EXT_DESC_INFO_INFO0_MGMT_PKT)) {
			/* BAR frames reach REO to move the BA window so consume the frame
			 * silently.
			 */
			if (!!(le32_to_cpu(mpdu_info->info0) &
			       HAL_RX_MPDU_DESC_INFO_INFO0_BAR_FRAME_FLAG))
				mgmt->srng_stats.bar_pkts[ATH12K_MGMT_SRNG_PKT_TYPE_RX]++;
			else
				mgmt->srng_stats.invalid_pkts++;

			dev_kfree_skb_any(mmpdu);
			continue;
		}

		rxcb->is_first_msdu = le32_get_bits(msdu_info->info0,
			HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG);
		rxcb->is_last_msdu = le32_get_bits(msdu_info->info0,
			HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG);
		rxcb->is_continuation = le32_get_bits(msdu_info->info0,
			HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION);

		rxcb->hw_link_id = hw_link_id;

		__skb_queue_tail(mmpdu_list, mmpdu);

		done = !rxcb->is_continuation;
	}

	/* Hw might have updated the head pointer after we cached it.
	 * In this case, even though there are entries in the ring we'll
	 * get rx_desc NULL. Give the read another try with updated cached
	 * head pointer so that we can reap complete MMPDU in the current
	 * rx processing.
	 */
	if (!done && ath12k_hal_srng_dst_num_free(ab, srng, true)) {
		ath12k_hal_srng_access_end(ab, srng);
		goto try_again;
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (!num_buffs_reaped)
		goto exit;

	ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, &mgmt_wifi8->wbm_refill_ring,
					     &rx_desc_used_list, false);

exit:
	return num_buffs_reaped;
}

static inline
void ath12k_wifi8_mgmt_rx_desc_copy_end_tlv(struct ath12k_base *ab,
					    struct hal_rx_desc *fdesc,
					    struct hal_rx_desc *ldesc)
{
	ab->hal.hal_ops->rx_desc_copy_end_tlv(fdesc, ldesc);
}

static int
ath12k_wifi8_mgmt_rx_mmpdu_coalesce(struct ath12k_mgmt *mgmt,
				    struct sk_buff_head *mmpdu_list,
				    struct sk_buff *first, struct sk_buff *last,
				    struct hal_rx_desc_data *rx_desc_data)
{
	struct sk_buff *skb;
	struct ath12k_base *ab = mgmt->ab;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(first);
	u8 l3_pad_bytes = rx_desc_data->l3_pad_bytes;
	u16 mmpdu_len = rx_desc_data->msdu_len;
	u32 hal_rx_desc_sz = mgmt->hal->hal_desc_sz;
	struct hal_rx_desc *ldesc;
	int first_buf_hdr_len, first_buf_len;
	int space_extra, rem_len, buf_len;
	bool is_continuation;

	/* As the MMPDU is spread across multiple rx buffers,
	 * the MMPDU len will be more than MGMT_RX_BUFFER_SIZE
	 * excluding the headers in the first buffer.
	 */
	first_buf_hdr_len = hal_rx_desc_sz + l3_pad_bytes;
	first_buf_len = MGMT_RX_BUFFER_SIZE - first_buf_hdr_len;

	if (WARN_ON_ONCE(mmpdu_len <= first_buf_len)) {
		skb_put(first, first_buf_hdr_len + mmpdu_len);
		skb_pull(first, first_buf_hdr_len);
		return 0;
	}

	rxcb->is_first_msdu = rx_desc_data->is_first_msdu;
	rxcb->is_last_msdu = rx_desc_data->is_last_msdu;

	ldesc = (struct hal_rx_desc *)last->data;

	/* Data in the first buf will be
	 * MGMT_RX_BUFFER_SIZE - HAL_RX_DESC_SIZE
	 */
	skb_put(first, MGMT_RX_BUFFER_SIZE);
	skb_pull(first, first_buf_hdr_len);

	/* MSDU_END TLVs are valid only in the last buffer */
	ath12k_wifi8_mgmt_rx_desc_copy_end_tlv(ab, rxcb->rx_desc, ldesc);

	space_extra = mmpdu_len - (first_buf_len + skb_tailroom(first));
	if (space_extra > 0 &&
	    (pskb_expand_head(first, 0, space_extra, GFP_ATOMIC) < 0)) {
		/* Free up all buffers of the MMPDU */
		while ((skb = __skb_dequeue(mmpdu_list)) != NULL) {
			rxcb = ATH12K_SKB_RXCB(skb);
			is_continuation = rxcb->is_continuation;
			dev_kfree_skb_any(skb);
			if (!is_continuation)
				break;
		}
		return -ENOMEM;
	}

	rem_len = mmpdu_len - first_buf_len;
	while ((skb = __skb_dequeue(mmpdu_list)) != NULL && rem_len > 0) {
		rxcb = ATH12K_SKB_RXCB(skb);
		is_continuation = rxcb->is_continuation;
		if (is_continuation)
			buf_len = MGMT_RX_BUFFER_SIZE - hal_rx_desc_sz;
		else
			buf_len = rem_len;

		if (buf_len > (MGMT_RX_BUFFER_SIZE - hal_rx_desc_sz)) {
			WARN_ON_ONCE(1);
			dev_kfree_skb_any(skb);
			return -EINVAL;
		}

		skb_put(skb, buf_len + hal_rx_desc_sz);
		skb_pull(skb, hal_rx_desc_sz);
		skb_copy_from_linear_data(skb, skb_put(first, buf_len),
					  buf_len);
		dev_kfree_skb_any(skb);

		rem_len -= buf_len;
		if (!is_continuation)
			break;
	}

	return 0;
}

static void
ath12k_wifi8_mgmt_rx_h_ppdu(struct ath12k *partner_ar, struct sk_buff *mmpdu,
			    struct ieee80211_rx_status *status,
			    struct hal_rx_desc_data *desc_data)
{
	struct ieee80211_hdr *hdr = (void *)mmpdu->data;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *channel;
	enum rx_msdu_start_pkt_type pkt_type;
	u32 center_freq, meta_data;
	u8 channel_num, bw, sgi, rate_mcs, nss;
	struct ath12k_link_sta *arsta;
	struct ieee80211_hw *hw;
	bool is_cck;
	s8 rssi;

	status->freq = 0;
	status->rate_idx = 0;
	status->nss = 0;
	status->encoding = RX_ENC_LEGACY;
	status->bw = RATE_INFO_BW_20;
	status->enc_flags = 0;
	status->band = NUM_NL80211_BANDS;

	meta_data = desc_data->freq;
	channel_num = meta_data;
	center_freq = meta_data >> 16;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		status->band = NL80211_BAND_6GHZ;
		status->freq = center_freq;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		status->band = NL80211_BAND_5GHZ;
	}

	hw = partner_ar->ah->hw;
	if (status->band == NUM_NL80211_BANDS || !hw->wiphy->bands[status->band]) {
		ath12k_err(partner_ar->ab,
			   "sband invalid for band:%u center_freq=%u channel=%u on pdev=%u",
			   status->band, center_freq, channel_num, partner_ar->pdev_idx);
		spin_lock_bh(&partner_ar->data_lock);
		channel = partner_ar->rx_channel;
		if (channel) {
			status->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		} else {
			ath12k_err(partner_ar->ab, "Failed to derive channel info on pdev=%u",
				   partner_ar->pdev_idx);
		}
		spin_unlock_bh(&partner_ar->data_lock);
		status->freq = ieee80211_channel_to_frequency(channel_num, status->band);
	}

	if (status->band != NL80211_BAND_6GHZ)
		status->freq = ieee80211_channel_to_frequency(channel_num, status->band);

	bw = status->bw;
	pkt_type = desc_data->pkt_type;
	sgi = desc_data->sgi;
	rate_mcs = desc_data->rate_mcs;
	nss = desc_data->nss;
	rssi = desc_data->snr + partner_ar->rssi_offsets.rssi_offset;

	status->signal = rssi;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		sband = &partner_ar->mac.sbands[status->band];
		status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
							     is_cck);
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		status->encoding = RX_ENC_HT;
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(partner_ar->ab,
				    "Received with invalid mcs in HT mode %d",
				    rate_mcs);
			break;
		}
		status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		status->encoding = RX_ENC_VHT;
		status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX + ATH12K_VHT_VENDOR_MCS_MAX) {
			ath12k_warn(partner_ar->ab,
				    "Received with invalid mcs in VHT mode %d",
				    rate_mcs);
			break;
		}
		status->nss = nss;
		if (sgi)
			status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(partner_ar->ab,
				    "Received with invalid mcs in HE mode %d",
				    rate_mcs);
			break;
		}
		status->encoding = RX_ENC_HE;
		status->nss = nss;
		status->he_gi = ath12k_mac_he_gi_to_nl80211_he_gi(sgi);
		status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(partner_ar->ab,
				    "Received with invalid mcs in EHT mode %d",
				    rate_mcs);
			break;
		}

		status->encoding = RX_ENC_EHT;
		status->nss = nss;
		status->eht.gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(sgi);
		status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	case RX_MSDU_START_PKT_TYPE_11BN:
		status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_UHR_MCS_MAX) {
			ath12k_warn(partner_ar->ab,
					"Received with invalid mcs in UHR mode %d",
					rate_mcs);
			break;
		}

		status->encoding = RX_ENC_UHR;
		status->nss = nss;
		status->eht.gi = ath12k_mac_uhr_gi_to_nl80211_uhr_gi(sgi);
		status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		break;
	default:
		break;
	}

	spin_lock_bh(&partner_ar->arsta_lock);

	/* Fetch arsta from A2 on AP and A1 on STA */
	arsta = ath12k_link_sta_find_by_addr(partner_ar, hdr->addr2);
	if (!arsta)
		arsta = ath12k_link_sta_find_by_addr(partner_ar, hdr->addr1);

	if (arsta) {
		arsta->max_rssi = max(arsta->max_rssi, rssi);
		arsta->min_rssi = min(arsta->min_rssi, rssi);
	}

	spin_unlock_bh(&partner_ar->arsta_lock);
}

static void
ath12k_wifi8_mgmt_rx_pull_crypto(struct ath12k_mgmt *mgmt, struct sk_buff *mmpdu,
				 struct ieee80211_rx_status *status,
				 struct hal_rx_desc_data *desc_data)
{
	struct ieee80211_hdr *hdr = (void *)mmpdu->data;
	size_t hdr_len, mic_len, icv_len, crypto_len;

	if (!(status->flag & RX_FLAG_DECRYPTED))
		return;

	mic_len = ath12k_core_crypto_mic_len(mgmt->ab, desc_data->enctype);
	icv_len = ath12k_core_crypto_icv_len(mgmt->ab, desc_data->enctype);

	/* Tail - MIC and ICV */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		skb_trim(mmpdu, mmpdu->len - mic_len);
		skb_trim(mmpdu, mmpdu->len - icv_len);
	} else {
		/* Tail - MIC */
		if (status->flag & RX_FLAG_MIC_STRIPPED)
			skb_trim(mmpdu, mmpdu->len - mic_len);

		/* Tail - ICV */
		if (status->flag & RX_FLAG_ICV_STRIPPED)
			skb_trim(mmpdu, mmpdu->len - icv_len);
	}

	/* Head - Crypto header */
	if (status->flag & RX_FLAG_IV_STRIPPED) {
		hdr_len = ieee80211_hdrlen(hdr->frame_control);
		crypto_len = ath12k_core_crypto_param_len(mgmt->ab, desc_data->enctype);

		memmove(mmpdu->data + crypto_len, mmpdu->data, hdr_len);
		skb_pull(mmpdu, crypto_len);
	}
}

static void
ath12k_wifi8_mgmt_rx_h_mpdu(struct ath12k_mgmt *mgmt, struct sk_buff *mmpdu,
			    struct ieee80211_rx_status *status,
			    struct hal_rx_desc_data *desc_data)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)mmpdu->data;
	enum hal_encrypt_type enctype = HAL_ENCRYPT_TYPE_OPEN;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(mmpdu);
	u32 err_bitmap = desc_data->err_bitmap;
	struct ath12k_link_sta *arsta;
	struct ath12k *partner_ar;
	bool is_decrypted = false;

	partner_ar = ath12k_core_ar_from_hw_link_id(mgmt->ab, rxcb->hw_link_id);
	spin_lock_bh(&partner_ar->arsta_lock);

	/* Fetch arsta from A2 on AP and A1 on STA */
	arsta = ath12k_link_sta_find_by_addr(partner_ar, hdr->addr2);
	if (!arsta)
		arsta = ath12k_link_sta_find_by_addr(partner_ar, hdr->addr1);

	/* Skip self-peer arsta: ahsta is NULL and enctype is not meaningful */
	if (arsta && arsta->ahsta && !arsta->is_self_peer)
		enctype = arsta->ahsta->enctype;

	spin_unlock_bh(&partner_ar->arsta_lock);

	desc_data->enctype = enctype;

	if (desc_data->enctype != HAL_ENCRYPT_TYPE_OPEN && !err_bitmap)
		is_decrypted = desc_data->is_decrypted;

	/* Management frames are treated like raw frames. Hence, remove the FCS trailer */
	skb_trim(mmpdu, mmpdu->len - FCS_LEN);

	/* Errors are routed to be via error ring */
	status->flag &= ~(RX_FLAG_FAILED_FCS_CRC |
			  RX_FLAG_MMIC_ERROR |
			  RX_FLAG_DECRYPTED |
			  RX_FLAG_IV_STRIPPED |
			  RX_FLAG_MMIC_STRIPPED);

	if (err_bitmap & HAL_RX_MPDU_ERR_FCS)
		status->flag |= RX_FLAG_FAILED_FCS_CRC; /* HW doesn't forward */

	if (err_bitmap & HAL_RX_MPDU_ERR_TKIP_MIC)
		status->flag |= RX_FLAG_MMIC_ERROR;

	if (is_decrypted) {
		status->flag |= RX_FLAG_DECRYPTED;

		/* MMIE/PN validation for broadcast packets will be done in mac80211 */
		if (rxcb->is_mcbc)
			status->flag |= RX_FLAG_ICV_STRIPPED;
		else
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_PN_VALIDATED;

		ath12k_wifi8_mgmt_rx_pull_crypto(mgmt, mmpdu, status, desc_data);
	}
}

void
ath12k_wifi8_cu_mem_update(struct ath12k_base *ab,
			   struct ath12k_link_vif *arvif, bool is_probe_req)
{
	struct ieee80211_vif *vif;
	struct ieee80211_bss_conf *link_conf;
	u32 eht_bpcc = 0, cu_flags = 0, reconfig = 0, expec_dur = 0;
	u32 uhr_ebpcc = 0, uhr_countdown = 0;
	bool eht_cu = false, uhr_cu = false;
	unsigned int dbg_level = is_probe_req ? ATH12K_DBG_L1 : ATH12K_DBG_L0;

	if (!arvif->is_up || !arvif->ahvif || arvif->is_scan_vif)
		return;

	if (!arvif->ahvif->vif || !arvif->ahvif->vif->valid_links)
		return;

	if (arvif->link_id >= IEEE80211_MLD_MAX_NUM_LINKS ||
	    !(arvif->ahvif->vif->valid_links & BIT(arvif->link_id)))
		return;

	vif = arvif->ahvif->vif;

	if (ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_EHT_BPCC) {
		ath12k_mac_read_cu_mem(arvif,
				       offsetof(struct ath12k_cu_mem,
						cu_flags) / sizeof(u32),
				       &cu_flags);
		ath12k_mac_read_cu_mem(arvif,
				       offsetof(struct ath12k_cu_mem,
						eht_bpcc) / sizeof(u32),
				       &eht_bpcc);
		eht_cu = u32_get_bits(cu_flags, BIT(0));
		ieee80211_critical_update(vif, arvif->link_id,
					  !!eht_cu, (u8)eht_bpcc);
	}

	if (ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_ML_RECONFIG) {
		ath12k_mac_read_cu_mem(arvif,
				       offsetof(struct ath12k_cu_mem,
						reconfig) / sizeof(u32),
				       &reconfig);
		if (arvif->is_link_removal_in_progress)
			ieee80211_link_removal_count_update(vif, arvif->link_id,
							    (u16)reconfig);
	}

	link_conf = rcu_dereference(vif->link_conf[arvif->link_id]);

	if (link_conf && link_conf->cu_info.cu_in_progress) {
		if (ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_UHR_EBPCC) {
			ath12k_mac_read_cu_mem(arvif,
					       offsetof(struct ath12k_cu_mem,
							cu_flags) / sizeof(u32),
					       &cu_flags);
			ath12k_mac_read_cu_mem(arvif,
					       offsetof(struct ath12k_cu_mem,
							uhr_ebpcc) / sizeof(u32),
					       &uhr_ebpcc);
			uhr_cu = u32_get_bits(cu_flags, BIT(1));
		}

		if (ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_UHR_COUNTDOWN) {
			ath12k_mac_read_cu_mem(arvif,
					       offsetof(struct ath12k_cu_mem,
							uhr_countdown) / sizeof(u32),
					       &uhr_countdown);
		}
		if (((ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_UHR_EBPCC) ||
		     (ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_UHR_COUNTDOWN)) &&
		    uhr_countdown)
			ieee80211_critical_update_ecu(vif, arvif->link_id,
						      !!uhr_cu, (u8)uhr_ebpcc,
						       uhr_countdown);
	}

	if (is_probe_req &&
	    ab->cu_mem_cfg_mask & WMI_TBTT_COUNT_DOWN_CFG_TTLM_EXP_DUR) {
		ath12k_mac_read_cu_mem(arvif,
				       offsetof(struct ath12k_cu_mem,
						ttlm_expected_duration) /
				       sizeof(u32),
				       &expec_dur);
		if (expec_dur) {
			ieee80211_ttlm_info_expec_dur_update(vif, arvif->link_id,
							     expec_dur);
		}
	}

	ath12k_dbg_level(ab, ATH12K_DBG_CU, dbg_level,
			 "cu_update vdev %d eht_cu %d eht_bpcc %u reconfig %u uhr_cu %d uhr_ebpcc %u uhr_countdown %u expec_dur %u\n",
			 arvif->vdev_id, eht_cu, eht_bpcc, reconfig,
			 uhr_cu, uhr_ebpcc,
			 uhr_countdown, expec_dur);
}


static u8 ath12k_wifi8_mgmt_rx_get_vdev_id(struct ath12k_base *ab,
					   enum ath12k_peer_metadata_version ver,
					   u32 peer_metadata)
{
	switch (ver) {
	default:
		ath12k_warn(ab, "Unknown peer metadata version: %d", ver);
		fallthrough;
	case ATH12K_PEER_METADATA_V0:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V0_VDEV_ID);
	case ATH12K_PEER_METADATA_V1:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1_VDEV_ID);
	case ATH12K_PEER_METADATA_V1A:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1A_VDEV_ID);
	case ATH12K_PEER_METADATA_V1B:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1B_VDEV_ID);
	}
}

static u8 ath12k_wifi8_mgmt_rx_get_hw_link_id(struct ath12k_base *ab,
					       enum ath12k_peer_metadata_version ver,
					       u32 peer_metadata)
{
	switch (ver) {
	default:
		ath12k_warn(ab, "Unknown peer metadata version: %d", ver);
		fallthrough;
	case ATH12K_PEER_METADATA_V0:
	case ATH12K_PEER_METADATA_V1:
		ath12k_warn(ab, "peer metadata version: %d does not support hw_link_id",
			    ver);
		return 0;
	case ATH12K_PEER_METADATA_V1A:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1A_LOGICAL_LINK_ID);
	case ATH12K_PEER_METADATA_V1B:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1B_HW_LINK_ID);
	}
}

static void
ath12k_wifi8_mgmt_rx_cu_mem_update(struct ath12k *ar, struct sk_buff *mmpdu,
				   u32 peer_metadata)
{
	enum ath12k_peer_metadata_version ver;
	struct ath12k_link_vif *arvif;
	struct ieee80211_hdr *hdr;
	struct ath12k *target_ar = ar;

	hdr = (struct ieee80211_hdr *)mmpdu->data;

	if (ieee80211_is_assoc_req(hdr->frame_control) ||
	    ieee80211_is_reassoc_req(hdr->frame_control)) {
		ver = ath12k_core_get_peer_metadata_ver(ar->ab);
		u8 vdev_id = ath12k_wifi8_mgmt_rx_get_vdev_id(ar->ab,
							       ver,
							       peer_metadata);
		u8 hw_link_id = ath12k_wifi8_mgmt_rx_get_hw_link_id(ar->ab,
								    ver,
								    peer_metadata);
		if (ar->hw_link_id != hw_link_id)
			target_ar = ath12k_core_ar_from_hw_link_id(ar->ab, hw_link_id);

		if (!target_ar)
			return;

		spin_lock_bh(&target_ar->data_lock);
		arvif = ath12k_mac_get_arvif_by_global_vdev_id(target_ar, vdev_id);
		if (!arvif || !arvif->cu_mem || !arvif->is_up)
			goto unlock;

		if (!arvif->ahvif->vif->valid_links)
			goto unlock;

		ath12k_wifi8_cu_mem_update(target_ar->ab, arvif, false);

unlock:
		spin_unlock_bh(&target_ar->data_lock);
	}
}

static int ath12k_wifi8_mgmt_rx_process_mmpdu(struct ath12k_mgmt *mgmt,
					      struct ath12k *partner_ar,
					      struct sk_buff *mmpdu,
					      struct sk_buff_head *mmpdu_list,
					      struct ieee80211_rx_status *rx_status)
{
	struct hal_rx_desc_data rx_desc_data = {0};
	u32 hal_rx_desc_sz = mgmt->hal->hal_desc_sz;
	struct hal_rx_desc *rx_desc, *lrx_desc;
	struct ath12k_skb_rxcb *rxcb;
	struct sk_buff *last_buf;
	u32 peer_metadata;
	u8 l3_pad_bytes;
	u16 mmpdu_len;
	int ret;

	last_buf = ath12k_mgmt_rx_get_mmpdu_last_buf(mmpdu_list, mmpdu);
	if (!last_buf) {
		ath12k_warn(mgmt,
			    "No valid Rx buffer to access MSDU_END TLV");
		return -EIO;
	}

	rx_desc = (struct hal_rx_desc *)mmpdu->data;
	lrx_desc = (struct hal_rx_desc *)last_buf->data;
	rxcb = ATH12K_SKB_RXCB(mmpdu);
	rxcb->rx_desc = rx_desc;

	ath12k_wifi8_mgmt_extract_rx_desc_data(mgmt, &rx_desc_data, rx_desc,
					       lrx_desc);
	if (!rx_desc_data.msdu_done) {
		ath12k_warn(mgmt, "msdu_done bit in MSDU_END is not set");
		return -EIO;
	}

	rxcb->is_mcbc = rx_desc_data.is_mcbc;

	mmpdu_len = rx_desc_data.msdu_len;
	l3_pad_bytes = rx_desc_data.l3_pad_bytes;

	if (!rxcb->is_continuation) {
		if ((mmpdu_len + hal_rx_desc_sz) > MGMT_RX_BUFFER_SIZE) {
			ath12k_warn(mgmt->ab, "Invalid MMPDU len=%u", mmpdu_len);
			ath12k_dbg_dump(mgmt->ab, ATH12K_DBG_MGMT, NULL, "",
					rx_desc, sizeof(*rx_desc));
			return -EINVAL;
		}
		skb_put(mmpdu, hal_rx_desc_sz + l3_pad_bytes + mmpdu_len);
		skb_pull(mmpdu, hal_rx_desc_sz + l3_pad_bytes);
	} else {
		ret = ath12k_wifi8_mgmt_rx_mmpdu_coalesce(mgmt, mmpdu_list,
							  mmpdu, last_buf,
							  &rx_desc_data);
		if (ret) {
			ath12k_warn(mgmt,
				    "Failed to coalesce MMPDU rx buffer");
			return ret;
		}
	}

	ath12k_wifi8_mgmt_rx_h_ppdu(partner_ar, mmpdu, rx_status, &rx_desc_data);
	ath12k_wifi8_mgmt_rx_h_mpdu(mgmt, mmpdu, rx_status, &rx_desc_data);

	rx_status->flag |= RX_FLAG_SKIP_MONITOR | RX_FLAG_DUP_VALIDATED;

	peer_metadata = ath12k_wifi8_mgmt_rx_h_peer_meta_data(mgmt, rx_desc);
	ath12k_wifi8_mgmt_rx_cu_mem_update(partner_ar, mmpdu, peer_metadata);

	return 0;
}

#ifndef CPTCFG_QCN_EXTN
static void
ath12k_wifi8_mgmt_rx_deliver_mmpdu(struct ath12k_mgmt *mgmt, struct ath12k *partner_ar,
				   struct sk_buff *mmpdu,
				   struct ieee80211_rx_status *status,
				   enum ath12k_mgmt_srng_pkt_type pkt_type)
#else /* !CPTCFG_QCN_EXTN */
/* Note: @pkt_type supports both &enum ath12k_mgmt_srng_pkt_type and
 * &enum ath12k_mgmt_srng_pkt_type_extn.
 */
void ath12k_wifi8_mgmt_rx_deliver_mmpdu(struct ath12k_mgmt *mgmt,
					struct ath12k *partner_ar,
					struct sk_buff *mmpdu,
					struct ieee80211_rx_status *status,
					u32 pkt_type)
#endif /* !CPTCFG_QCN_EXTN */
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(partner_ar);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_hdr *hdr;
	struct ath12k_device_mgmt_srng_stats *mgmt_srng_stats;
	struct ath12k_mgmt *partner_mgmt;
	u16 frm_stype, fc;

	if (ah->state != ATH12K_HW_STATE_ON && ah->state != ATH12K_HW_STATE_RESTARTED) {
		/* drop packets received before mac start */
		dev_kfree_skb_any(mmpdu);
		return;
	}

	hdr = (struct ieee80211_hdr *)mmpdu->data;
	fc = le16_to_cpu(hdr->frame_control);
	frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	partner_mgmt = partner_ar->ab->mgmt ? partner_ar->ab->mgmt : mgmt;

	mgmt_srng_stats = &partner_mgmt->srng_stats;

#ifdef CPTCFG_QCN_EXTN
	/* Stats extension */
	if (ath12k_wifi8_mgmt_rx_deliver_mmpdu_extn(mgmt, partner_ar, mmpdu, pkt_type))
#endif
		mgmt_srng_stats->rx_pkts[frm_stype]++;

	rx_status = IEEE80211_SKB_RXCB(mmpdu);
	*rx_status = *status;

	if (ath12k_mac_mgmt_need_smd_sta_session_ctx(mmpdu) &&
	    !ath12k_smd_collect_sta_session_ctx(partner_ar, mmpdu))
		return;

	ieee80211_rx_ni(hw, mmpdu);
}

static bool ath12k_wifi8_mgmt_rx_h_reo_err(struct ath12k_mgmt *mgmt,
					   struct sk_buff_head *mmpdu_list,
					   struct sk_buff *mmpdu)
{
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(mmpdu);
	u32 hal_rx_desc_sz = mgmt->hal->hal_desc_sz;
	struct hal_rx_desc_data rx_desc_data = {0};
	struct ath12k_mgmt *partner_mgmt = mgmt;
	struct hal_rx_desc *rx_desc, *lrx_desc;
	u16 buf_hdr_len, mmpdu_len;
	struct ath12k *partner_ar;
	struct ieee80211_hdr *hdr;
	struct sk_buff *last_buf;
	u16 frm_stype, fc;

	mgmt->srng_stats.reo_err[rxcb->err_code]++;

	switch (rxcb->err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP:
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR:
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN:
		break;
	default:
		goto drop;
	}

	last_buf = ath12k_mgmt_rx_get_mmpdu_last_buf(mmpdu_list, mmpdu);
	if (!last_buf)
		goto drop;

	rx_desc = (struct hal_rx_desc *)mmpdu->data;
	lrx_desc = (struct hal_rx_desc *)last_buf->data;

	ath12k_wifi8_mgmt_extract_rx_desc_data(mgmt, &rx_desc_data, rx_desc, lrx_desc);
	if (!rx_desc_data.msdu_done)
		goto drop;

	buf_hdr_len = hal_rx_desc_sz + rx_desc_data.l3_pad_bytes;
	mmpdu_len = rx_desc_data.msdu_len;

	if (sizeof(struct ieee80211_hdr) > mmpdu_len ||
	    buf_hdr_len + sizeof(struct ieee80211_hdr) > MGMT_RX_BUFFER_SIZE)
		goto drop;

	hdr = (struct ieee80211_hdr *)(mmpdu->data + buf_hdr_len);

	fc = le16_to_cpu(hdr->frame_control);
	frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	partner_ar = ath12k_core_ar_from_hw_link_id(mgmt->ab, rxcb->hw_link_id);
	if (partner_ar)
		partner_mgmt = partner_ar->ab->mgmt ? partner_ar->ab->mgmt : mgmt;

	/* There may be stations using different SN spaces for management frames though
	 * they are all individually addressed.
	 *
	 * Accept them to avoid interoperability issues.
	 */
	partner_mgmt->srng_stats.reo_err_rx[frm_stype]++;
	return false;

drop:
	return true;
}

static void ath12k_wifi8_mgmt_rx_clean_up_err_sg_mmpdu(struct sk_buff_head *mmpdu_list,
						       struct sk_buff *mmpdu)
{
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(mmpdu);
	bool mmpdu_continues = rxcb->is_continuation;
	struct sk_buff *skb;

	dev_kfree_skb_any(mmpdu);

	if (!mmpdu_continues)
		return;

	while ((skb = __skb_dequeue(mmpdu_list))) {
		rxcb = ATH12K_SKB_RXCB(skb);
		mmpdu_continues = rxcb->is_continuation;
		dev_kfree_skb_any(skb);

		/* last buf, stop */
		if (!mmpdu_continues)
			break;
	}
}

/* Return: true when frame to be dropped, false otherwise */
static bool ath12k_wifi8_mgmt_rx_process_err_mmpdu(struct ath12k_mgmt *mgmt,
						   struct sk_buff_head *mmpdu_list,
						   struct sk_buff *mmpdu)
{
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(mmpdu);

	switch (rxcb->err_rel_src) {
	case HAL_REO_REL_SRC_MODULE_RXDMA:
		mgmt->srng_stats.rxdma_err[rxcb->err_code]++;
		break;
	case HAL_REO_REL_SRC_MODULE_REO:
		if (!ath12k_wifi8_mgmt_rx_h_reo_err(mgmt, mmpdu_list, mmpdu))
			return false;
		break;
	default:
		/* invalid source, free the buffer */
		break;
	}

	/* Clean-up continuation buffers while dropping */
	ath12k_wifi8_mgmt_rx_clean_up_err_sg_mmpdu(mmpdu_list, mmpdu);

	return true;
}

#ifndef CPTCFG_QCN_EXTN
static void
ath12k_wifi8_mgmt_rx_process_packets(struct ath12k_mgmt *mgmt,
				     struct sk_buff_head *mmpdu_list,
				     enum ath12k_mgmt_srng_pkt_type pkt_type)
#else /* !CPTCFG_QCN_EXTN */
/* Note: @pkt_type supports both &enum ath12k_mgmt_srng_pkt_type and
 * &enum ath12k_mgmt_srng_pkt_type_extn.
 */
void ath12k_wifi8_mgmt_rx_process_packets(struct ath12k_mgmt *mgmt,
					  struct sk_buff_head *mmpdu_list,
					  u32 pkt_type)
#endif /* !CPTCFG_QCN_EXTN */
{
	struct ieee80211_rx_status rx_status = {0};
	struct sk_buff *mmpdu;
	struct ath12k_skb_rxcb *rxcb;
	struct ath12k *partner_ar;
	int ret;

	if (skb_queue_empty(mmpdu_list))
		return;

	rcu_read_lock();

	while ((mmpdu = __skb_dequeue(mmpdu_list))) {
		rxcb = ATH12K_SKB_RXCB(mmpdu);

		partner_ar = ath12k_core_ar_from_hw_link_id(mgmt->ab, rxcb->hw_link_id);
		if (!partner_ar ||
		    test_bit(ATH12K_FLAG_CAC_RUNNING, &partner_ar->dev_flags)) {
			dev_kfree_skb_any(mmpdu);
			continue;
		}

		if (pkt_type == ATH12K_MGMT_SRNG_PKT_TYPE_RX_ERR &&
		    ath12k_wifi8_mgmt_rx_process_err_mmpdu(mgmt, mmpdu_list, mmpdu))
			continue;

		ret = ath12k_wifi8_mgmt_rx_process_mmpdu(mgmt, partner_ar, mmpdu,
							 mmpdu_list, &rx_status);
		if (ret) {
			dev_kfree_skb_any(mmpdu);
			continue;
		}

		ath12k_wifi8_mgmt_rx_deliver_mmpdu(mgmt, partner_ar, mmpdu,
						   &rx_status, pkt_type);
	}

	rcu_read_unlock();
}

static void ath12k_wifi8_mgmt_rx_process(struct ath12k_base *ab,
					 struct ath12k_mgmt_irq_grp *irq_grp,
					 struct mgmt_srng *ring)
{
	struct sk_buff_head mmpdu_list;
	int ret;

	__skb_queue_head_init(&mmpdu_list);

	ret = ath12k_wifi8_mgmt_rx_reap_packets(ab, ring, &mmpdu_list);
	if (!ret)
		return;

	ath12k_wifi8_mgmt_rx_process_packets(ab->mgmt, &mmpdu_list,
					     ATH12K_MGMT_SRNG_PKT_TYPE_RX);
}

static int
ath12k_wifi8_mgmt_rx_parse_desc_err(struct ath12k_mgmt *mgmt,
				    struct hal_reo_dest_ring *desc,
				    struct hal_rx_reo_dest_rel_info *err_info)
{
	struct ath12k_base *ab = mgmt->ab;
	struct hal_rx_mpdu_desc *mpdu_info = &desc->rx_mpdu_info;
	struct hal_rx_mpdu_ext_desc_info *mpdu_ext_info = &desc->rx_mpdu_ext_info;
	u32 rxdma_push_reason, rxdma_err_code, reo_push_reason, reo_err_code;
	struct hal_rx_msdu_desc *rx_msdu_info = &desc->rx_msdu_info;
	enum hal_reo_dest_ring_buffer_type type;
	enum hal_reo_dest_rel_src_module rel_src;
	bool is_frag;

	is_frag = !!(le32_to_cpu(mpdu_info->info0) &
		     HAL_RX_MPDU_DESC_INFO_INFO0_FRAGMENT_FLAG);
	if (is_frag) {
		mgmt->srng_stats.frag_pkts++;
		return 0;
	}

	type = le32_get_bits(mpdu_ext_info->info0,
			     HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_DEST_BUFFER_TYPE);
	if (type != HAL_REO_DEST_RING_BUFFER_TYPE_MSDU)
		return -EINVAL;

	rxdma_push_reason =
		le32_get_bits(mpdu_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_PUSH_REASON);
	rxdma_err_code =
		le32_get_bits(mpdu_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RXDMA_ERROR_CODE);
	reo_push_reason =
		le32_get_bits(mpdu_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_PUSH_REASON);
	reo_err_code =
		le32_get_bits(mpdu_ext_info->info0,
			      HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_ERROR_CODE);

	rel_src = le32_get_bits(mpdu_ext_info->info0,
				HAL_RX_MPDU_EXT_DESC_INFO_INFO0_RELEASE_SOURCE_MODULE);
	if (rel_src != HAL_REO_REL_SRC_MODULE_REO) {
		ath12k_warn(ab,
			    "Invalid source module %u for error packets, rxdma (%u %u) reo (%u %u)",
			    rel_src, rxdma_push_reason, rxdma_err_code,
			    reo_push_reason, reo_err_code);
		return -EINVAL;
	}

	err_info->cookie = le32_get_bits(desc->buf_addr_info.info1,
					 BUFFER_ADDR_INFO1_SW_COOKIE);
	err_info->rx_desc = ath12k_mgmt_get_rx_desc_from_cookie(mgmt, err_info->cookie);

	err_info->first_msdu = le32_get_bits(rx_msdu_info->info0,
		HAL_RX_MSDU_DESC_INFO_INFO0_FIRST_MSDU_IN_MPDU_FLAG);
	err_info->last_msdu = le32_get_bits(rx_msdu_info->info0,
		HAL_RX_MSDU_DESC_INFO_INFO0_LAST_MSDU_IN_MPDU_FLAG);
	err_info->continuation = le32_get_bits(rx_msdu_info->info0,
		HAL_RX_MSDU_DESC_INFO_INFO0_MSDU_CONTINUATION);

	if (rxdma_push_reason !=
	    HAL_REO_ENTR_RING_RXDMA_PUSH_REASON_ROUTING_INSTRUCTION) {
		err_info->push_reason = rxdma_push_reason;
		err_info->err_code = rxdma_err_code;
		rel_src = HAL_REO_REL_SRC_MODULE_RXDMA;
	} else {
		err_info->push_reason = reo_push_reason;
		err_info->err_code = reo_err_code;
	}
	err_info->err_rel_src = rel_src;

	return 0;
}

static int
ath12k_wifi8_mgmt_rx_reap_err_packets(struct ath12k_base *ab,
				      struct mgmt_srng *ring,
				      struct sk_buff_head *mmpdu_list)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct hal_rx_reo_dest_rel_info err_info = { 0 };
	struct list_head rx_desc_used_list;
	struct ath12k_rx_desc_info *desc_info;
	struct hal_reo_dest_ring *desc;
	struct ath12k_skb_rxcb *rxcb;
	int num_buffs_reaped = 0;
	u8 hw_link_id;
	struct hal_srng *srng;
	struct sk_buff *mmpdu;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int ret;

	INIT_LIST_HEAD(&rx_desc_used_list);

	srng = &ab->hal.srng_list[ring->ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = ath12k_hal_srng_dst_num_free(ab, srng, false);
	if (unlikely(!valid_entries)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_dst_invalidate_entry(ab->dp, srng, valid_entries);
#endif

	while ((desc = ath12k_hal_srng_dst_get_next_cached_entry(ab, srng, NULL))) {
		struct hal_rx_mpdu_ext_desc_info *mpdu_ext_info = &desc->rx_mpdu_ext_info;
		struct hal_rx_mpdu_desc *mpdu_info = &desc->rx_mpdu_info;

		mgmt->srng_stats.err_ring_pkts++;

		ret = ath12k_wifi8_mgmt_rx_parse_desc_err(mgmt, desc, &err_info);
		if (ret < 0) {
			ath12k_warn(ab, "Failed to parse mgmt reo_err_desc: %d", ret);
			WARN_ON_ONCE(1);
			continue;
		}

		desc_info = err_info.rx_desc;
		if (!desc_info) {
			ath12k_err(ab,
				   "Unable to retrieve mgmt err rx_desc for cookie 0x%x",
				   err_info.cookie);
			WARN_ON_ONCE(1);
			continue;
		}

		if (desc_info->magic != ATH12K_MGMT_RX_DESC_MAGIC)
			ath12k_warn(ab, "MGMT RX err desc is tainted");

		mmpdu = desc_info->skb;
		desc_info->skb = NULL;
		list_add_tail(&desc_info->list, &rx_desc_used_list);

		rxcb = ATH12K_SKB_RXCB(mmpdu);
		ath12k_core_dma_unmap_single(mgmt->dev, rxcb->paddr,
					     mmpdu->len + skb_tailroom(mmpdu),
					     DMA_FROM_DEVICE);

		num_buffs_reaped++;

		if (!le32_get_bits(mpdu_ext_info->info0,
				   HAL_RX_MPDU_EXT_DESC_INFO_INFO0_MGMT_PKT)) {
			/* BAR frames reach REO to move the BA window so consume the frame
			 * silently.
			 */
			if (!!(le32_to_cpu(mpdu_info->info0) &
			       HAL_RX_MPDU_DESC_INFO_INFO0_BAR_FRAME_FLAG))
				mgmt->srng_stats.bar_pkts
					[ATH12K_MGMT_SRNG_PKT_TYPE_RX_ERR]++;
			else
				mgmt->srng_stats.invalid_pkts++;

			dev_kfree_skb_any(mmpdu);
			continue;
		}

		hw_link_id = le32_get_bits(mpdu_ext_info->info0,
					   HAL_RX_MPDU_EXT_DESC_INFO_INFO0_SRC_LINK_ID);

		rxcb->is_first_msdu = err_info.first_msdu;
		rxcb->is_last_msdu = err_info.last_msdu;
		rxcb->is_continuation = err_info.continuation;
		rxcb->err_rel_src = err_info.err_rel_src;
		rxcb->err_code = err_info.err_code;
		rxcb->rx_desc = (struct hal_rx_desc *)mmpdu->data;
		rxcb->hw_link_id = hw_link_id;

		__skb_queue_tail(mmpdu_list, mmpdu);
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (!num_buffs_reaped)
		goto exit;

	ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, &mgmt_wifi8->wbm_refill_ring,
					     &rx_desc_used_list, false);

exit:
	return num_buffs_reaped;
}

static void ath12k_wifi8_mgmt_rx_process_err(struct ath12k_base *ab,
					     struct ath12k_mgmt_irq_grp *irq_grp,
					     struct mgmt_srng *ring)
{
	struct sk_buff_head mmpdu_list;
	int ret;

	__skb_queue_head_init(&mmpdu_list);

	ret = ath12k_wifi8_mgmt_rx_reap_err_packets(ab, ring, &mmpdu_list);
	if (!ret)
		return;

	ath12k_wifi8_mgmt_rx_process_packets(ab->mgmt, &mmpdu_list,
					     ATH12K_MGMT_SRNG_PKT_TYPE_RX_ERR);
}

void ath12k_wifi8_mgmt_service_srng(struct ath12k_base *ab,
				    struct ath12k_mgmt_irq_grp *irq_grp)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(ab->mgmt);

	ath12k_wifi8_mgmt_rx_process(ab, irq_grp, &mgmt_wifi8->reo_dst_rx_ring);

	ath12k_wifi8_mgmt_rx_process_err(ab, irq_grp, &mgmt_wifi8->reo_dst_rx_err_ring);
}

#if LINUX_VERSION_IS_GEQ(6, 13, 0)
void ath12k_wifi8_mgmt_workqueue(struct work_struct *w)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_work(irq_grp, w, intr_wq);
	struct ath12k_base *ab = irq_grp->ab;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags) ||
	    ab->ag->wsi_remap_in_progress || ab->is_bypassed)
		goto exit;

	ath12k_wifi8_mgmt_service_srng(ab, irq_grp);
exit:
	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#else
void ath12k_wifi8_mgmt_tasklet(struct tasklet_struct *t)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_tasklet(irq_grp, t, intr_tq);
	struct ath12k_base *ab = irq_grp->ab;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags) ||
	    ab->ag->wsi_remap_in_progress || ab->is_bypassed)
		goto exit;

	ath12k_wifi8_mgmt_service_srng(ab, irq_grp);
exit:
	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#endif

int ath12k_wifi8_mgmt_rx_refill_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	int ret;
	LIST_HEAD(used_list);

	/* WBM Idle Buffer ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->wbm_idle_buf_ring,
				     HAL_WBM_IDLE_BUF_MGMT, 0, 0,
				     ATH12K_MGMT_IRQ_GRP_ID_INVALID,
				     MGMT_WBM_IDLE_BUF_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to setup mgmt wbm_idle_buf_ring: %d", ret);
		return ret;
	}

	/* WBM Refill ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->wbm_refill_ring,
				     HAL_WBM_BUF_MGMT, 0, 0,
				     ATH12K_MGMT_IRQ_GRP_ID_INVALID,
				     MGMT_REFILL_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to setup mgmt wbm_refill_ring: %d", ret);
		return ret;
	}

	return 0;
}

void ath12k_wifi8_mgmt_rx_refill_ring_init(struct ath12k_base *ab)
{
	LIST_HEAD(list);
	size_t req_entries;
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng *rx_refill_ring = &mgmt_wifi8->wbm_refill_ring;

	req_entries = ath12k_mgmt_get_req_entries_from_refill_ring(ab, rx_refill_ring,
								   &list);
	if (req_entries)
		ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, rx_refill_ring, &list, false);
}

void ath12k_wifi8_mgmt_refill_rings_deinit(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);

	ath12k_mgmt_srng_hw_disable(ab, &mgmt_wifi8->wbm_refill_ring);
	ath12k_mgmt_srng_hw_disable(ab, &mgmt_wifi8->wbm_idle_buf_ring);
}

int ath12k_wifi8_mgmt_rx_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	int ring_num = 0, err_ring_num = 0, ret;

	/* Mgmt Rx ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->reo_dst_rx_ring,
				     HAL_REO_DST_MGMT, ring_num++, 0, 0,
				     MGMT_REO_DST_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt reo_dst_ring: %d", ret);
		return ret;
	}

	/* Mgmt Rx Error/Exception ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->reo_dst_rx_err_ring,
				     HAL_REO_EXCEPTION_MGMT, err_ring_num++, 0,
				     0, MGMT_REO_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt reo_dst_err_ring: %d", ret);
		goto err_srng_cleanup;
	}

#ifdef CPTCFG_QCN_EXTN
	/* Mgmt Rx High-priority ring */
	ret = ath12k_wifi8_mgmt_rx_ring_setup_extn(ab);
	if (ret) {
		ath12k_err(ab, "Failed to set up additional mgmt rings: %d", ret);
		return ret;
	}
#endif

	if (ath12k_dp_umac_reset_in_progress(ab))
		return ret;

	/* Mgmt Rx Refill rings */
	ret = ath12k_wifi8_mgmt_rx_refill_ring_setup(ab);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt refill rings: %d", ret);
		goto err_srng_cleanup;
	}

	/* Initialize WBM ring with descriptors and buffers */
	ath12k_wifi8_mgmt_rx_refill_ring_init(ab);

	return 0;

err_srng_cleanup:
	ath12k_wifi8_mgmt_rx_ring_free(ab);
	return ret;
}

static void ath12k_wifi8_mgmt_rx_ring_free(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);

#ifdef CPTCFG_QCN_EXTN
	ath12k_wifi8_mgmt_rx_ring_free_extn(ab);
#endif
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->wbm_refill_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->wbm_idle_buf_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->reo_dst_rx_err_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->reo_dst_rx_ring);
}

struct ath12k_mgmt *ath12k_wifi8_get_cumac_mgmt(struct ath12k_mgmt *mgmt)
{
	struct ath12k_hw_group *ag = mgmt->ab->ag;
	struct ath12k_base *partner_ab;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed || !partner_ab->mgmt)
			continue;

		if (partner_ab->is_cumac_chip)
			return partner_ab->mgmt;
	}

	return NULL;
}

int ath12k_wifi8_mgmt_op_device_init(struct ath12k_mgmt *mgmt)
{
	struct ath12k_base *ab = mgmt->ab;
	int ret;

	if (!ab->is_cumac_chip) {
		ath12k_dbg(ab, ATH12K_DBG_MGMT,
			   "Skip mgmt op init for non C-UMAC device %d", ab->device_id);
		mgmt->init_done = true;
		return 0;
	}

	if (mgmt->init_done) {
		ath12k_dbg(ab, ATH12K_DBG_MGMT,
			   "mgmt op init already completed for device %d", ab->device_id);
		return 0;
	}

	ret = ath12k_mgmt_rx_desc_init(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to initialize mgmt rx desc: %d", ret);
		return ret;
	}

	ret = ath12k_wifi8_mgmt_rx_ring_setup(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to setup mgmt rx REO rings: %d", ret);
		goto fail_rx_desc_cleanup;
	}

	ath12k_mgmt_irq_grp_setup(mgmt);
	ret = ath12k_hif_mgmt_irq_setup(ab, mgmt);
	if (ret) {
		ath12k_warn(ab, "Failed to configure mgmt IRQs: %d", ret);
		ath12k_mgmt_irq_grp_cleanup(mgmt);
		goto fail_srng_free;
	}

	ath12k_hif_mgmt_irq_enable(ab);

	ath12k_info(ab, "C-UMAC init is success for mgmt on device %d", ab->device_id);
	mgmt->init_done = true;
	return 0;

fail_srng_free:
	ath12k_wifi8_mgmt_rx_ring_free(ab);

fail_rx_desc_cleanup:
	ath12k_mgmt_rx_desc_cleanup(ab);

	return ret;
}

void ath12k_wifi8_mgmt_op_device_deinit(struct ath12k_mgmt *mgmt)
{
	struct ath12k_base *ab = mgmt->ab;

	if (!mgmt->init_done)
		return;

	if (!ab->is_cumac_chip) {
		ath12k_dbg(ab, ATH12K_DBG_MGMT,
			   "Skip mgmt op deinit for non C-UMAC device %d", ab->device_id);
		mgmt->init_done = false;
		return;
	}

	ath12k_hif_mgmt_irq_disable(ab);
	ath12k_mgmt_rx_desc_cleanup(ab);
	ath12k_mgmt_irq_grp_cleanup(mgmt);
	ath12k_hif_mgmt_irq_cleanup(ab);
	ath12k_wifi8_mgmt_rx_ring_free(ab);
	mgmt->init_done = false;
}

int ath12k_wifi8_mgmt_wbm_ring_sel_config_qcn9625(struct ath12k_base *ab)
{
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	struct ath12k_mgmt_wifi8 *cumac_mgmt_wifi8;
	u32 hal_rx_desc_sz, wbm_ring_id;
	struct ath12k_mgmt *cumac_mgmt;
	int ret;

	hal_rx_desc_sz = ab->hal.hal_desc_sz;

	cumac_mgmt = ath12k_wifi8_get_cumac_mgmt(ab->mgmt);
	if (!cumac_mgmt) {
		ath12k_err(ab, "Failed to get C-UMAC mgmt for HTT setup");
		return -EINVAL;
	}

	cumac_mgmt_wifi8 = ath12k_get_mgmt_wifi8(cumac_mgmt);
	wbm_ring_id = cumac_mgmt_wifi8->wbm_idle_buf_ring.ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;

	ath12k_core_srng_get_htt_mgmt_filter(ab, &tlv_filter);

	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset = ab->hal.rx_mpdu_start_offset;
	tlv_filter.rx_msdu_end_offset = ab->hal.rx_msdu_end_offset;

	tlv_filter.rx_mpdu_start_wmask = ab->hal.rx_mpdu_start_wmask;
	tlv_filter.rx_msdu_end_wmask = ab->hal.rx_msdu_end_wmask;

	/* WBM Idle Buffer Pool 1 is used for mgmt */
	tlv_filter.rdi_based_source_cfg =
		ath12k_wifi8_hal_get_rdi_source_cfg(ab, SOURCE_RING_CTRL_MGMT);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "Configuring compact tlv masks: rx_mpdu_start_wmask 0x%x rx_msdu_end_wmask 0x%x",
		   tlv_filter.rx_mpdu_start_wmask, tlv_filter.rx_msdu_end_wmask);

	ret = ath12k_core_srng_htt_rx_filter_setup(ab, wbm_ring_id, 0,
						   HAL_WBM_IDLE_BUF_MGMT,
						   MGMT_RX_BUFFER_SIZE, &tlv_filter);

	return ret;
}

static int ath12k_wifi8_mgmt_op_htt_setup(struct ath12k_mgmt *mgmt)
{
	struct ath12k_base *ab = mgmt->ab;
	int ret;

	ret = ab->hw_params->hw_ops->mgmt_rxdma_ring_sel_config(ab);
	if (ret)
		ath12k_err(ab, "Failed to set up MGMT rxdma ring selection: %d", ret);

	return ret;
}

static int
ath12k_wifi8_mgmt_dump_ring_stats(struct ath12k_mgmt *mgmt, char *buf, int size)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct ath12k_base *ab = mgmt->ab;
	int len = 0;

	if (len < size)
		len += ath12k_hal_dump_ring_stats(ab, HAL_REO_DST_MGMT,
						  mgmt_wifi8->reo_dst_rx_ring.ring_id,
						  buf + len, size - len);

	if (len < size)
		len += ath12k_hal_dump_ring_stats(ab, HAL_REO_EXCEPTION_MGMT,
						  mgmt_wifi8->reo_dst_rx_err_ring.ring_id,
						  buf + len, size - len);

	if (len < size)
		len += ath12k_hal_dump_ring_stats(ab, HAL_WBM_BUF_MGMT,
						  mgmt_wifi8->wbm_refill_ring.ring_id,
						  buf + len, size - len);

	if (len < size)
		len += ath12k_hal_dump_ring_stats(ab, HAL_WBM_IDLE_BUF_MGMT,
						  mgmt_wifi8->wbm_idle_buf_ring.ring_id,
						  buf + len, size - len);

#ifdef CPTCFG_QCN_EXTN
	len = ath12k_wifi8_mgmt_dump_ring_stats_extn(mgmt, buf, size, len);
#endif

	return len;
}

static void ath12k_wifi8_mgmt_rx_replenish_buffs_wrapper(struct ath12k_mgmt *mgmt,
							 struct list_head *used_list,
							 bool reuse)
{
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);

	ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, &mgmt_wifi8->wbm_refill_ring,
					     used_list, reuse);
}

static struct ath12k_mgmt_arch_ops ath12k_wifi8_mgmt_arch_ops = {
	.mgmt_op_device_init = ath12k_wifi8_mgmt_op_device_init,
	.mgmt_op_device_deinit = ath12k_wifi8_mgmt_op_device_deinit,
	.mgmt_op_htt_setup = ath12k_wifi8_mgmt_op_htt_setup,
	.mgmt_op_dump_ring_stats = ath12k_wifi8_mgmt_dump_ring_stats,
	.mgmt_rx_replenish_buffs = ath12k_wifi8_mgmt_rx_replenish_buffs_wrapper,
	.mgmt_op_override_mld_tx = ath12k_wifi8_mgmt_op_override_mld_tx,
};

struct ath12k_mgmt *ath12k_wifi8_mgmt_init(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8;
	struct ath12k_mgmt_irq_grp *irq_grp;

	mgmt = kzalloc(sizeof(*mgmt) + sizeof(*mgmt_wifi8), GFP_KERNEL);
	if (!mgmt)
		return NULL;

	irq_grp = kzalloc(sizeof(*irq_grp), GFP_KERNEL);
	if (!irq_grp) {
		kfree(mgmt);
		return NULL;
	}

	*irq_grp = (struct ath12k_mgmt_irq_grp){
		.grp_id = 0,
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		.irq_grp_handler = ath12k_wifi8_mgmt_workqueue,
#else
		.irq_grp_handler = ath12k_wifi8_mgmt_tasklet,
#endif
		.num_irq = 1,
	};

	mgmt->ab = ab;
	mgmt->dev = ab->dev;
	mgmt->hal = &ab->hal;
	mgmt->arch_ops = &ath12k_wifi8_mgmt_arch_ops;
	mgmt->hw_params = ab->hw_params;
	mgmt->irq_grp = irq_grp;
	mgmt->num_irq_grp = 1;

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_wifi8_mgmt_init_extn(mgmt)) {
		kfree(mgmt->irq_grp);
		kfree(mgmt);
		return NULL;
	}
#endif

	return mgmt;
}

void ath12k_wifi8_mgmt_deinit(struct ath12k_mgmt *mgmt)
{
#ifdef CPTCFG_QCN_EXTN
	ath12k_wifi8_mgmt_deinit_extn(mgmt);
#endif
	kfree(mgmt->irq_grp);
	kfree(mgmt);
}
