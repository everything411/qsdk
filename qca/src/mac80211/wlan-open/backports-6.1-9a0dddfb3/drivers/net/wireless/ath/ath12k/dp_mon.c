// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "debug.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "debugfs.h"
#include "dp_mon_filter.h"
#include "dp_tx_mon.h"
#include "telemetry_agent_if.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "wmi.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#include <linux/skbuff_ref.h>
#endif

static inline u32
ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(enum ath12k_eht_ru_size ru_size)
{
	switch (ru_size) {
	case ATH12K_EHT_RU_26:
		return RU_26;
	case ATH12K_EHT_RU_52:
		return RU_52;
	case ATH12K_EHT_RU_52_26:
		return RU_52_26;
	case ATH12K_EHT_RU_106:
		return RU_106;
	case ATH12K_EHT_RU_106_26:
		return RU_106_26;
	case ATH12K_EHT_RU_242:
		return RU_242;
	case ATH12K_EHT_RU_484:
		return RU_484;
	case ATH12K_EHT_RU_484_242:
		return RU_484_242;
	case ATH12K_EHT_RU_996:
		return RU_996;
	case ATH12K_EHT_RU_996_484:
		return RU_996_484;
	case ATH12K_EHT_RU_996_484_242:
		return RU_996_484_242;
	case ATH12K_EHT_RU_996x2:
		return RU_2X996;
	case ATH12K_EHT_RU_996x2_484:
		return RU_2X996_484;
	case ATH12K_EHT_RU_996x3:
		return RU_3X996;
	case ATH12K_EHT_RU_996x3_484:
		return RU_3X996_484;
	case ATH12K_EHT_RU_996x4:
		return RU_4X996;
	default:
		return RU_INVALID;
	}
}

void
ath12k_dp_mon_fill_rx_stats_info(struct hal_rx_mon_ppdu_info *ppdu_info,
				 struct ieee80211_rx_status *rx_status)
{
	u32 center_freq = ppdu_info->freq;

	rx_status->freq = center_freq;
	rx_status->bw = ath12k_mac_bw_to_mac80211_bw(ppdu_info->bw);
	rx_status->nss = ppdu_info->nss;
	rx_status->rate_idx = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	} else {
		rx_status->band = NUM_NL80211_BANDS;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_stats_info);

static void
ath12k_dp_mon_rx_update_radiotap_he(struct hal_rx_mon_ppdu_info *rx_status,
				    u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_data1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data3, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data4, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data5, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data6, &rtap_buf[rtap_len]);
}

static void
ath12k_dp_mon_rx_update_radiotap_he_mu(struct hal_rx_mon_ppdu_info *rx_status,
				       u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_flags1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_flags2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	rtap_buf[rtap_len] = rx_status->he_RU[0];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[1];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[2];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[3];
}

static void
ath12k_dp_mon_update_radiotap_uhr(struct hal_rx_mon_ppdu_info *ppduinfo,
				  struct sk_buff *mon_skb,
				  struct ieee80211_rx_status *rxs)
{
	struct ieee80211_radiotap_tlv *tlv;
	struct ieee80211_radiotap_uhr *uhr;
	struct ieee80211_radiotap_uhr_usig *uhr_usig;
	struct ieee80211_radiotap_uhr_elr *uhr_elr;
	u16 uhr_len = 0, len = 0, uhr_usig_len, i, uhr_elr_len = 0;
	u8 user;

	if (ppduinfo->is_uhr && !ppduinfo->is_uhr_elr) {
		uhr_len = struct_size(uhr, user, ppduinfo->uhr_info.num_user_info);
		len += sizeof(*tlv) + uhr_len;
	}

	if (ppduinfo->is_uhr && ppduinfo->is_uhr_elr) {
		uhr_elr_len = sizeof(*uhr_elr);
		len += sizeof(*tlv) + uhr_elr_len;
	}

	if (ppduinfo->uhr_usig) {
		uhr_usig_len = sizeof(*uhr_usig);
		len += sizeof(*tlv) + uhr_usig_len;
	}

	rxs->flag |= RX_FLAG_RADIOTAP_TLV_AT_END;
	rxs->encoding = RX_ENC_UHR;

	tlv = skb_push(mon_skb, len);

	if (ppduinfo->uhr_usig) {
		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
		tlv->len = cpu_to_le16(uhr_usig_len);
		uhr_usig = (struct ieee80211_radiotap_uhr_usig *)tlv->data;
		*uhr_usig = ppduinfo->u_sig_info.uhr_usig;
		tlv = (struct ieee80211_radiotap_tlv *)&(tlv->data[uhr_usig_len]);
	}

	if (ppduinfo->is_uhr && !ppduinfo->is_uhr_elr) {
		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_UHR);
		tlv->len = cpu_to_le16(uhr_len);
		uhr = (struct ieee80211_radiotap_uhr *)tlv->data;
		uhr->known = ppduinfo->uhr_info.uhr.known;
		for (i = 0;
		     i < ARRAY_SIZE(uhr->data) &&
		     i < ARRAY_SIZE(ppduinfo->uhr_info.uhr.data);
		     i++)
			uhr->data[i] = ppduinfo->uhr_info.uhr.data[i];

		for (user = 0; user < ppduinfo->uhr_info.num_user_info; user++) {
			put_unaligned_le32(ppduinfo->uhr_info.user_known[user],
					   &uhr->user[user].known);
			put_unaligned_le32(ppduinfo->uhr_info.user_info[user],
					   &uhr->user[user].info);
			}

		tlv = (struct ieee80211_radiotap_tlv *)&tlv->data[uhr_len];
	}

	if (ppduinfo->is_uhr && ppduinfo->is_uhr_elr) {
		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_UHR_ELR);
		tlv->len = cpu_to_le16(uhr_elr_len);
		uhr_elr = (struct ieee80211_radiotap_uhr_elr *)tlv->data;
		uhr_elr->known = ppduinfo->elr_info.known;
		uhr_elr->sig1 = ppduinfo->elr_info.sig1;
		uhr_elr->sig2 = ppduinfo->elr_info.sig2;
		uhr_elr->mark = ppduinfo->elr_info.mark;
	}
}

static void
ath12k_dp_mon_update_radiotap_eht(struct hal_rx_mon_ppdu_info *ppduinfo,
				  struct sk_buff *mon_skb,
				  struct ieee80211_rx_status *rxs)
{
	struct ieee80211_radiotap_tlv *tlv;
	struct ieee80211_radiotap_eht *eht;
	struct ieee80211_radiotap_eht_usig *usig;
	u16 len = 0, i, eht_len = 0, usig_len;
	u8 user;

	if (ppduinfo->is_eht) {
		eht_len = struct_size(eht,
				      user_info,
				      ppduinfo->eht_info.num_user_info);
		len += sizeof(*tlv) + eht_len;
	}

	if (ppduinfo->eht_usig) {
		usig_len = sizeof(*usig);
		len += sizeof(*tlv) + usig_len;
	}

	rxs->flag |= RX_FLAG_RADIOTAP_TLV_AT_END;
	rxs->encoding = RX_ENC_EHT;

	tlv = skb_push(mon_skb, len);

	if (ppduinfo->eht_usig) {
		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
		tlv->len = cpu_to_le16(usig_len);

		usig = (struct ieee80211_radiotap_eht_usig *)tlv->data;
		*usig = ppduinfo->u_sig_info.usig;
		tlv = (struct ieee80211_radiotap_tlv *)&(tlv->data[usig_len]);
	}

	if (ppduinfo->is_eht) {
		tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT);
		tlv->len = cpu_to_le16(eht_len);

		eht = (struct ieee80211_radiotap_eht *)tlv->data;
		eht->known = ppduinfo->eht_info.eht.known;

		for (i = 0;
		     i < ARRAY_SIZE(eht->data) &&
		     i < ARRAY_SIZE(ppduinfo->eht_info.eht.data);
		     i++)
			eht->data[i] = ppduinfo->eht_info.eht.data[i];

		for (user = 0; user < ppduinfo->eht_info.num_user_info; user++)
			put_unaligned_le32(ppduinfo->eht_info.user_info[user],
					   &eht->user_info[user]);

		tlv = (struct ieee80211_radiotap_tlv *)&tlv->data[eht_len];
	}
}

void
ath12k_dp_mon_rx_add_pf_tag_to_headroom(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_msdu_info *msdu_info;
	struct sk_buff *skb = NULL;
	u8 user_id = ppdu_info->user_id;
	u16 cce_metadata, fse_metadata;
	u16 flow_tag = 0;
	u8 *skb_head;
	bool invalid_cce = false, invalid_fse = false;
	u16 marker = ATH12K_RX_MON_MSDU_MARKER;
	u32 proto_type;
	u16 cce_tag = 0;

	skb = skb_peek_tail(&ppdu_info->mpdu_q[user_id]);
	if (unlikely(!skb))
		return;

	if (unlikely(skb_headroom(skb) < ATH12K_RX_MON_MAX_METADATA_HDR))
		return;

	msdu_info = &ppdu_info->msdu_info[user_id];

	if (unlikely(msdu_info->msdu_index > ATH12K_RX_MON_MAX_MSDU))
		return;

	fse_metadata = msdu_info->fse_metadata & ATH12K_RX_MON_FSE_TAG_MASK;
	cce_metadata = msdu_info->cce_metadata;

	if (cce_metadata < ATH12K_RX_PROTOCOL_TAG_START_OFFSET) {
		invalid_cce = true;
	} else {
		proto_type = cce_metadata - ATH12K_RX_PROTOCOL_TAG_START_OFFSET;
		if (unlikely(proto_type >= ATH12K_PKT_TYPE_MAX))
			invalid_cce = true;
		else
			cce_tag = dp_pdev->protocol_tag_map[proto_type].tag;
	}

	/* A valid fse entry is expected to have a non-zero tag.
	 */
	if (!fse_metadata)
		invalid_fse = true;

	flow_tag = fse_metadata;

	if (invalid_cce && invalid_fse)
		return;

	skb_head = skb->head;

	put_unaligned_le16(marker, skb_head);
	skb_head += ATH12K_RX_MON_MSDU_MARKER_SIZE;

	put_unaligned_le16(msdu_info->msdu_index, skb_head);
	skb_head += ATH12K_RX_MON_MSDU_CNT_SIZE;
	skb_head += ((msdu_info->msdu_index - 1) * ATH12K_RX_MON_PF_TAG_SIZE);

	put_unaligned_le16(cce_tag, skb_head);
	skb_head += sizeof(u16);

	put_unaligned_le16(flow_tag, skb_head);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_add_pf_tag_to_headroom);

u16 ath12k_dp_mon_rx_vendor_tlv_len(struct sk_buff *mpdu)
{
	u16 msdu_count;

	if (get_unaligned_le16(mpdu->head) == ATH12K_RX_MON_MSDU_MARKER) {
		msdu_count =
			get_unaligned_le16(&mpdu->head[ATH12K_RX_MON_MSDU_MARKER_SIZE]);
		if (!msdu_count || msdu_count > ATH12K_RX_MON_MAX_MSDU)
			return 0;

		return ATH12K_RX_MON_MSDU_CNT_SIZE +
		       (msdu_count * ATH12K_RX_MON_PF_TAG_SIZE);
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_vendor_tlv_len);

void
ath12k_dp_rx_mon_add_vendor_tlv(struct sk_buff *skb,
				struct ieee80211_rx_status *rxs,
				u16 vendor_data_len)
{
	struct ieee80211_radiotap_vendor_tlv *rtap;

	rtap = skb_push(skb, sizeof(*rtap) + vendor_data_len);
	rtap->type = cpu_to_le16(IEEE80211_RADIOTAP_VENDOR_NAMESPACE);
	rtap->len = cpu_to_le16(sizeof(*rtap) -
				sizeof(struct ieee80211_radiotap_tlv) +
				vendor_data_len);

	/* Qualcomm OUI */
	rtap->content.oui[0] = 0x00;
	rtap->content.oui[1] = 0x03;
	rtap->content.oui[2] = 0x7f;
	/* monitor metadata sub-namespace */
	rtap->content.oui_subtype = 1;
	rtap->content.vendor_type = cpu_to_le16(0);
	rtap->content.reserved = cpu_to_le16(0);

	memcpy(rtap->content.data, &skb->head[ATH12K_RX_MON_MSDU_MARKER_SIZE],
	       vendor_data_len);

	rxs->flag |= RX_FLAG_RADIOTAP_TLV_AT_END;
}
EXPORT_SYMBOL(ath12k_dp_rx_mon_add_vendor_tlv);

void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_mon_ppdu_info *ppduinfo,
				   struct sk_buff *mon_skb,
				   struct ieee80211_rx_status *rxs)
{
	struct ieee80211_supported_band *sband;
	u8 *ptr = NULL;

	rxs->flag |= RX_FLAG_MACTIME_START;
	rxs->signal = (s8)(ppduinfo->rssi_comb + dp_pdev->ar->rssi_offsets.rssi_offset);
	rxs->flag &= ~RX_FLAG_NO_SIGNAL_VAL;
	rxs->noise = dp_pdev->ar->rssi_offsets.avg_nf_dbm;
	rxs->nss = ppduinfo->nss + 1;

	if (ppduinfo->userstats[ppduinfo->user_id].ampdu_present) {
		rxs->flag |= RX_FLAG_AMPDU_DETAILS;
		rxs->ampdu_reference = ppduinfo->userstats[ppduinfo->user_id].ampdu_id;
	}

	skb_reset_mac_header(mon_skb);
	if (ppduinfo->vendor_tlv_len)
		ath12k_dp_rx_mon_add_vendor_tlv(mon_skb, rxs, ppduinfo->vendor_tlv_len);

	if (ppduinfo->is_uhr || ppduinfo->uhr_usig) {
		ath12k_dp_mon_update_radiotap_uhr(ppduinfo, mon_skb, rxs);
	} else if (ppduinfo->is_eht || ppduinfo->eht_usig) {
		ath12k_dp_mon_update_radiotap_eht(ppduinfo, mon_skb, rxs);
	} else if (ppduinfo->he_mu_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE_MU;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he_mu));
		ath12k_dp_mon_rx_update_radiotap_he_mu(ppduinfo, ptr);
	} else if (ppduinfo->he_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he));
		ath12k_dp_mon_rx_update_radiotap_he(ppduinfo, ptr);
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->vht_flags) {
		rxs->encoding = RX_ENC_VHT;
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->ht_flags) {
		rxs->encoding = RX_ENC_HT;
		rxs->rate_idx = ppduinfo->rate;
	} else {
		rxs->encoding = RX_ENC_LEGACY;
		if (rxs->band < NUM_NL80211_BANDS) {
			sband = &dp_pdev->ar->mac.sbands[rxs->band];
			rxs->rate_idx =
				ath12k_mac_hw_rate_to_idx(sband, ppduinfo->rate,
							  ppduinfo->cck_flag);
		}
	}

	rxs->mactime = ppduinfo->tsft;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_radiotap);

void ath12k_dp_mon_rx_deliver_skb(struct ath12k_pdev_dp *dp_pdev,
				  struct napi_struct *napi,
				  struct sk_buff *msdu,
				  struct ieee80211_rx_status *status,
				  struct hal_rx_mon_ppdu_info *ppduinfo)
{
	static const struct ieee80211_radiotap_he known = {
		.data1 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
				     IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN),
		.data2 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN),
	};
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_radiotap_he *he = NULL;

	status->link_valid = 0;
	status->link_id = 0;

	if ((status->encoding == RX_ENC_HE) && !(status->flag & RX_FLAG_RADIOTAP_HE) &&
	    !(status->flag & RX_FLAG_SKIP_MONITOR)) {
		he = skb_push(msdu, sizeof(known));
		memcpy(he, &known, sizeof(known));
		status->flag |= RX_FLAG_RADIOTAP_HE;
	}

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!napi)
		ieee80211_rx_ni(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	else
		ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), NULL, msdu, napi);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_deliver_skb);

int ath12k_dp_mon_rx_set_pktlen(struct sk_buff *skb, u32 len)
{
	if (skb->len > len) {
		skb_trim(skb, len);
	} else {
		if (skb_tailroom(skb) < len - skb->len) {
			if ((pskb_expand_head(skb, 0,
					      len - skb->len - skb_tailroom(skb),
					      GFP_ATOMIC))) {
				return -ENOMEM;
			}
		}
		skb_put(skb, (len - skb->len));
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_set_pktlen);

static void
ath12k_dp_mon_handle_mon_desc(struct ath12k_dp *dp, struct ath12k_dp_mon_desc *mon_desc,
			      struct dp_mon_desc_list_params *list_params)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u8 *mon_buf = mon_desc->mon_buf;

	if (mon_buf) {
		ath12k_core_dma_unmap_page(dp->dev, mon_desc->paddr,
					   list_params->buff_size,
					   DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->num_frag_free++;
		mon_desc->mon_buf = NULL;
	}

	spin_lock_bh(list_params->desc_lock);
	list_del(&mon_desc->list);
	list_add_tail(&mon_desc->list, list_params->free_list);
	spin_unlock_bh(list_params->desc_lock);
}

int ath12k_dp_mon_rx_buf_replenish(struct ath12k_dp *dp,
				   struct dp_rxdma_mon_ring *buf_ring,
				   struct list_head *used_list,
				   int req_entries)
{
	struct dp_mon_desc_list_params list_params = {
		.desc_lock = &dp->dp_mon->mon_desc_lock,
		.free_list = &dp->dp_mon->mon_desc_free_list,
		.list_local = used_list,
		.pf_cache = &dp->dp_mon->rx_mon_pf_cache,
		.buff_size = ATH12K_DP_MON_RX_BUF_SIZE,
		.is_tx_monitor = false,
	};

	return ath12k_dp_mon_buf_replenish(dp, buf_ring, req_entries, &list_params);
}

int ath12k_dp_mon_buf_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_mon_ring *buf_ring,
				int req_entries,
				struct dp_mon_desc_list_params *list_params)
{
	struct ath12k_base *ab = dp->ab;
	void *mon_buf_desc;
	struct hal_srng *srng;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_mon_desc *mon_desc, *tmp_mon_desc;
	struct page *page;
	u64 offset, cookie;
	u32 addr_lo, addr_hi;
	dma_addr_t paddr;
	int ret = 0;
	u8 *mon_buf;
	bool tx_monitor = (list_params->is_tx_monitor);

	list_for_each_entry_safe(mon_desc, tmp_mon_desc, list_params->list_local, list) {
		if (unlikely(mon_desc->in_use != DP_MON_DESC_REPLENISH)) {
			ath12k_warn(dp,
				    "Invalid in_use %d, possibly desc from freelist\n",
				    mon_desc->in_use);
			ath12k_dp_mon_handle_mon_desc(dp, mon_desc, list_params);
			continue;
		}

		mon_buf = page_frag_alloc(list_params->pf_cache,
					  list_params->buff_size, GFP_ATOMIC);
		if (unlikely(!mon_buf)) {
			if (tx_monitor)
				dp_mon->dp_tx_mon->tx_mon_stats.alloc_fail++;
			ret = -ENOMEM;
			goto out;
		}

		page = virt_to_head_page(mon_buf);
		offset = ((void *)mon_buf) - page_address(page);
		paddr = ath12k_core_dma_map_page(ab->dev, page, offset,
						 list_params->buff_size, DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(ab->dev, paddr))) {
			if (tx_monitor)
				dp_mon->dp_tx_mon->tx_mon_stats.dma_fail++;
			page_frag_free(mon_buf);
			mon_desc->mon_buf = NULL;
			dp_mon->num_frag_free++;
			ret = -EIO;
			goto out;
		}

		mon_desc->mon_buf = mon_buf;
		mon_desc->paddr = paddr;
		mon_desc->magic = ATH12K_MON_MAGIC_VALUE;
		mon_desc->in_use = DP_MON_DESC_TO_HW;
		mon_desc->buf_len = 0;
		mon_desc->end_of_ppdu = 0;
		dp_mon->num_frag_replenish++;
	}

	srng = &ab->hal.srng_list[buf_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (req_entries > 0) {
		mon_desc = list_first_entry_or_null(list_params->list_local,
						    struct ath12k_dp_mon_desc, list);
		if (unlikely(!mon_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		mon_buf_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!mon_buf_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		list_del(&mon_desc->list);
		addr_lo = cpu_to_le32(lower_32_bits(mon_desc->paddr));
		addr_hi = cpu_to_le32(upper_32_bits(mon_desc->paddr));
		cookie = cpu_to_le64((uintptr_t)mon_desc);
		ath12k_hal_mon_set_mon_buf_desc(&ab->hal, mon_buf_desc, addr_lo,
						addr_hi, cookie);
		req_entries--;
		if (tx_monitor)
			dp_mon->dp_tx_mon->tx_mon_stats.buf_replenished++;
	}

ring_unlock:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

out:
	if (unlikely(!list_empty(list_params->list_local))) {
		/* Reset the use flag */
		list_for_each_entry_safe(mon_desc, tmp_mon_desc,
					 list_params->list_local, list) {
			mon_buf = mon_desc->mon_buf;
			if (mon_buf) {
				ath12k_core_dma_unmap_page(ab->dev, mon_desc->paddr,
							   list_params->buff_size,
							   DMA_FROM_DEVICE);
				page_frag_free(mon_buf);
				mon_desc->mon_buf = NULL;
				dp_mon->num_frag_free++;
			}

			ath12k_dp_mon_desc_reset(mon_desc);
			mon_desc->in_use = DP_MON_DESC_H_REPLENISH_ERR;
		}

		spin_lock_bh(list_params->desc_lock);
		list_splice_tail(list_params->list_local, list_params->free_list);
		spin_unlock_bh(list_params->desc_lock);
	}

	return ret;
}

size_t ath12k_dp_mon_list_cut_nodes(struct list_head *list, struct list_head *head,
				    size_t count)
{
	struct list_head *cur;
	struct ath12k_dp_mon_desc *mon_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		mon_desc = list_entry(cur, struct ath12k_dp_mon_desc, list);
		ath12k_dp_mon_desc_reset(mon_desc);
		mon_desc->in_use = DP_MON_DESC_REPLENISH;

		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);

out:
	return nodes;
}

size_t
ath12k_dp_mon_get_free_desc_list(struct ath12k_dp *dp,
				 struct dp_rxdma_mon_ring *rx_ring,
				 struct dp_mon_desc_list_params *list_params,
				 size_t max_entries)
{
	struct hal_srng *srng;
	struct ath12k_base *ab = dp->ab;
	size_t num_free, req_entries, num_req_buf;

	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	spin_lock_bh(list_params->desc_lock);
	ath12k_hal_srng_access_begin(ab, srng);
	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!num_free) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(list_params->desc_lock);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_access_end(ab, srng);

	num_req_buf = num_free;

	if (max_entries && rx_ring->bufs_max >= max_entries)
		num_req_buf = max_entries;

	if (num_free < num_req_buf)
		num_req_buf = num_free;

	req_entries = ath12k_dp_mon_list_cut_nodes(list_params->list_local,
						   list_params->free_list,
						   num_req_buf);
	spin_unlock_bh(list_params->desc_lock);
	spin_unlock_bh(&srng->lock);

	return req_entries;
}

size_t ath12k_dp_mon_get_rx_free_desc_list(struct ath12k_dp *dp,
					   struct dp_rxdma_mon_ring *rx_ring,
					   struct list_head *list,
					   size_t ring_lvl)
{
	struct dp_mon_desc_list_params list_params = {
		.desc_lock = &dp->dp_mon->mon_desc_lock,
		.free_list = &dp->dp_mon->mon_desc_free_list,
		.list_local = list,
		.is_tx_monitor = false,
	};

	return ath12k_dp_mon_get_free_desc_list(dp, rx_ring, &list_params, ring_lvl);
}

static void
ath12k_dp_mon_rx_update_peer_rate_table_stats(struct ath12k_rx_peer_stats *rx_stats,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      struct hal_rx_user_status *user_stats,
					      u32 num_msdu)
{
	struct ath12k_rx_peer_rate_stats *stats;
	u32 mcs_idx = (user_stats) ? user_stats->mcs : ppdu_info->mcs;
	u32 nss_idx = (user_stats) ? user_stats->nss - 1 : ppdu_info->nss - 1;
	u32 bw_idx = ppdu_info->bw;
	u32 gi_idx = ppdu_info->gi;
	u32 len;

	if (!rx_stats)
		return;

	if (mcs_idx > HAL_RX_MAX_MCS_BN || nss_idx >= HAL_RX_MAX_NSS ||
	    bw_idx >= HAL_RX_BW_MAX || gi_idx >= HAL_RX_GI_MAX) {
		return;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BN)
		gi_idx = ath12k_he_gi_to_nl80211_he_gi(ppdu_info->gi);

	rx_stats->pkt_stats.rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += num_msdu;
	stats = &rx_stats->byte_stats;

	if (user_stats)
		len = user_stats->mpdu_ok_byte_count;
	else
		len = ppdu_info->mpdu_len;

	stats->rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += len;
}

/**
 * ath12k_dp_mon_rx_calculate_avg_rate() - Calculate the average rate using a
 * low-pass filter.
 * @avg_rate: The current average rate.
 * @new_rate_sample: The new rate sample to include in the average.
 *
 * This function applies a low-pass filter to average the data rate. It gives
 * more weight to the previous average, creating a smoothing effect.
 *
 * Return: The new calculated average rate.
 */
static inline u64 ath12k_dp_mon_rx_calculate_avg_rate(u64 avg_rate, int new_rate_sample)
{
	new_rate_sample = DP_ATH_RATE_IN(new_rate_sample);
	if (avg_rate == DUMMY_MARKER)
		return new_rate_sample;

	return ((avg_rate << 3) + new_rate_sample - avg_rate) >> 3;
}

/**
 * ath12k_dp_mon_rx_get_output_rate() - Convert an internal rate representation to an
 * output rate.
 * @internal_rate: The internal rate value to be converted.
 *
 * This function converts an internal rate representation to a final
 * output rate, including rounding logic.
 *
 * Return: The converted output rate or DUMMY_MARKER if the input is a dummy.
 */
static inline u64 ath12k_dp_mon_rx_get_output_rate(u64 internal_rate)
{
	const int multiplier = DP_ATH_RATE_EP_MULTIPLIER;

	if (internal_rate == DUMMY_MARKER)
		return DUMMY_MARKER;

	if ((internal_rate % multiplier) >= (multiplier / 2))
		return (internal_rate + (multiplier - 1)) / multiplier;
	else
		return internal_rate / multiplier;
}

static inline u16 ath12k_dp_get_avg_rate_stats_filter_val(void)
{
	/* Note: Returning default as ini config is absent */
	return DP_AVG_RATE_FILTER_DEFAULT;
}

static inline bool ath12k_dp_mon_eval_avg_rate_filter(u32 ratekbps, u32 avg_rx_rate)
{
	u16 filter_val = 0;

	filter_val = ath12k_dp_get_avg_rate_stats_filter_val();

	if (!filter_val || avg_rx_rate < filter_val || ratekbps > filter_val)
		return true;

	return false;
}

static void ath12k_dp_rx_update_rate_stats(struct ath12k_rx_ppdu_stats *rx_stats,
					   struct rate_info *rate)
{
	u32 ratekbps, ppdu_rx_rate;

	if (!rx_stats || !rate)
		return;

	/* Converting cfg80211_calculate_bitrate(100Kbps) to Kbps */
	ratekbps = cfg80211_calculate_bitrate(rate) * 100;
	rx_stats->last_rx_rate = ratekbps;

	if (likely(ath12k_dp_mon_eval_avg_rate_filter(ratekbps, rx_stats->avg_rx_rate))) {
		rx_stats->avg_rx_rate =
			ath12k_dp_mon_rx_calculate_avg_rate(rx_stats->avg_rx_rate,
							    ratekbps);
	}

	ppdu_rx_rate = ath12k_dp_mon_rx_get_output_rate(rx_stats->avg_rx_rate);
	rx_stats->rnd_avg_rx_rate = ppdu_rx_rate;

	if (rx_stats->preamble_info < HAL_RX_PREAMBLE_11N)
		rx_stats->rx_ratecode = ath12k_mac_get_rate_hw_value(ratekbps);
	else
		rx_stats->rx_ratecode =
			ATH12K_HW_RATE_CODE(rate->mcs, rate->nss,
					    rx_stats->preamble_info, 0);
}

static u8 ath12k_dp_rx_rate_convert_bw(u8 bw)
{
	u8 ret = 0;

	switch (bw) {
	case CMN_BW_20MHZ:
		ret = RATE_INFO_BW_20;
		break;
	case CMN_BW_40MHZ:
		ret = RATE_INFO_BW_40;
		break;
	case CMN_BW_80MHZ:
		ret = RATE_INFO_BW_80;
		break;
	case CMN_BW_160MHZ:
		ret = RATE_INFO_BW_160;
		break;
	case CMN_BW_240MHZ:
		ret = RATE_INFO_BW_160;
		break;
	case CMN_BW_320MHZ:
		ret = RATE_INFO_BW_320;
		break;
	default:
		ret = RATE_INFO_BW_20;
		break;
	}

	return ret;
}

static void ath12k_dp_rx_fill_rate_info(struct rate_info *rate,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct hal_rx_user_status *user_stats,
					bool is_su)
{
	u8 mcs, nss, preamble_type;
	u8 rix = 0, ret;
	u16 bitrate = 0;

	if (!rate || !ppdu_info)
		return;

	mcs = (user_stats) ? user_stats->mcs : ppdu_info->mcs;
	nss = (user_stats) ? user_stats->nss : ppdu_info->nss;
	preamble_type = ppdu_info->preamble_type;

	memset(rate, 0, sizeof(*rate));
	rate->nss = nss;

	if ((preamble_type > HAL_RX_PREAMBLE_11B) && (nss < 1 || nss > HAL_RX_MAX_NSS))
		return;

	rate->bw = ath12k_mac_bw_to_mac80211_bw(ppdu_info->bw);

	switch (preamble_type) {
	case HAL_RX_PREAMBLE_11A:
	case HAL_RX_PREAMBLE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(mcs, preamble_type,
							    &rix, &bitrate);
		if (ret < 0)
			return;
		rate->legacy = bitrate;
		break;

	case HAL_RX_PREAMBLE_11N:
		if (mcs > HAL_RX_MAX_MCS_HT)
			return;
		rate->mcs = mcs + 8 * (nss - 1);
		rate->flags = RATE_INFO_FLAGS_MCS;
		if (ppdu_info->sgi)
			rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AC:
		if (mcs > HAL_RX_MAX_MCS_VHT)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_VHT_MCS;
		if (ppdu_info->sgi)
			rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AX:
		if (mcs > HAL_RX_MAX_MCS_HE)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_HE_MCS;
		rate->he_gi = ath12k_he_gi_to_nl80211_he_gi(ppdu_info->sgi);
		if (is_su) {
			rate->bw = ath12k_dp_rx_rate_convert_bw(ppdu_info->bw);
		} else {
			rate->bw = RATE_INFO_BW_HE_RU;
			rate->he_ru_alloc = ppdu_info->ru_alloc;
		}
		break;

	case HAL_RX_PREAMBLE_11BE:
		if (mcs > HAL_RX_MAX_MCS_BE)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_EHT_MCS;
		rate->eht_gi = ath12k_eht_gi_to_nl80211_eht_gi(ppdu_info->sgi);
		if (is_su) {
			rate->bw = ath12k_dp_rx_rate_convert_bw(ppdu_info->bw);
		} else {
			rate->bw = RATE_INFO_BW_EHT_RU;
			rate->eht_ru_alloc = ppdu_info->ru_alloc;
		}
		break;

	case HAL_RX_PREAMBLE_11BN:
		if (mcs > HAL_RX_MAX_MCS_BN)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_UHR_MCS;

		if (ppdu_info->is_uhr_elr)
			rate->flags |= RATE_INFO_FLAGS_UHR_ELR_MCS;

		/*
		 * We fill EHT params for UHR mode as well since
		 * the APIs such as _cfg80211_calculate_bitrate_eht_uhr() etc.
		 * remain common and use EHT params to calculate Rx Bit rate etc.
		 */
		rate->eht_gi = ath12k_uhr_gi_to_nl80211_uhr_gi(ppdu_info->sgi);
		if (is_su) {
			rate->bw = ath12k_dp_rx_rate_convert_bw(ppdu_info->bw);
		} else {
			rate->bw = RATE_INFO_BW_EHT_RU; /* Remains same as that of EHT */
			rate->eht_ru_alloc = ppdu_info->ru_alloc;
		}
		break;

	default:
		return;
	}
}

static void ath12k_dp_rx_rate_stats_update(struct hal_rx_mon_ppdu_info *ppdu_info,
					   struct ath12k_dp_link_peer *peer, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	bool is_su = true;

	if (!peer || !ppdu_info)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU) {
		user_stats = &ppdu_info->userstats[uid];
		is_su = false;
		if (!user_stats)
			return;
	}

	spin_lock_bh(&peer->ppdu_stats_lock);
	ath12k_dp_rx_fill_rate_info(&peer->rxrate, ppdu_info, user_stats, is_su);
	spin_unlock_bh(&peer->ppdu_stats_lock);
}

void ath12k_dp_mon_rx_update_advance_stats(struct ath12k_rx_ppdu_stats *rx_stats,
					   struct hal_rx_mon_ppdu_info *ppdu_info,
					   u32 num_msdu, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	struct ath12k_rx_peer_user_stats *mu = NULL;
	struct pkt_type *su_ppdu_cnt = NULL;
	u8 preamble_type, mcs, nss, ac, punc_mode, max_mcs, res_mcs, mu_type;
	u8 proto_idx;
	u32 byte_count, tid;

	if (!rx_stats || !ppdu_info || uid >= HAL_MAX_UL_MU_USERS)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU)
		user_stats = &ppdu_info->userstats[uid];

	preamble_type = user_stats ? user_stats->preamble_type : ppdu_info->preamble_type;
	mcs = user_stats ? user_stats->mcs : ppdu_info->mcs;
	nss = user_stats ? user_stats->nss : ppdu_info->nss;
	byte_count = user_stats ? user_stats->mpdu_ok_byte_count : ppdu_info->mpdu_len;
	tid = user_stats ? user_stats->tid : ppdu_info->tid;
	ac = ath12k_tid_to_ac(tid);
	punc_mode = ppdu_info->punc_bw;

	if (preamble_type >= HAL_RX_PREAMBLE_MAX)
		return;

	if (tid <= IEEE80211_NUM_TIDS && ac < WME_NUM_AC) {
		rx_stats->wme_ac_type_pkts[ac] += num_msdu;
		rx_stats->wme_ac_type_bytes[ac] += byte_count;
	}

	if (punc_mode < MAX_PUNCTURED_MODE)
		rx_stats->punc_bw[punc_mode] += num_msdu;

	rx_stats->num_bar += ppdu_info->ctrl_frm_info[uid].bar;
	rx_stats->num_ndpa += ppdu_info->ctrl_frm_info[uid].ndpa;

	max_mcs = max_mcs_by_preamble[preamble_type];
	res_mcs = (mcs < max_mcs) ? mcs : (MAX_MCS - 1);

	rx_stats->proto_type[preamble_type].mcs_count[res_mcs] += num_msdu;

	proto_idx = ath12k_preamble_to_rx_mon_proto_idx(preamble_type);

	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
		if (proto_idx < ATH12K_RX_PPDU_PROTO_MAX) {
			su_ppdu_cnt = &rx_stats->su_ppdu_count[proto_idx];
			su_ppdu_cnt->mcs_count[res_mcs] += 1;
		}
		if (likely(nss) && (nss - 1) < HAL_RX_MAX_NSS)
			rx_stats->ppdu_nss[nss - 1] += 1;
	} else {
		if (!user_stats)
			return;

		mu_type = (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO) ?
			TXRX_TYPE_MU_MIMO : TXRX_TYPE_MU_OFDMA;

		if (proto_idx < ATH12K_RX_PPDU_PROTO_MAX) {
			mu = &rx_stats->rx_mu[proto_idx][mu_type];
			mu->mpdu_cnt_fcs_ok += user_stats->mpdu_cnt_fcs_ok;
			mu->ppdu.mcs_count[res_mcs] += 1;
			mu->mpdu_cnt_fcs_err += user_stats->mpdu_cnt_fcs_err;
			if (likely(nss) && (nss - 1) < HAL_RX_MAX_NSS)
				mu->ppdu_nss[nss - 1] += 1;
		}
	}
	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->ppdu_reception[ppdu_info->reception_type] += 1;
}

static void
ath12k_dp_mon_fill_rx_user_stats_peer_mac(struct ath12k_dp_link_peer *peer,
					  struct hal_rx_user_status *user_stats)
{
	if (!user_stats)
		return;

	if (!peer)
		memset(user_stats->peer_mac, 0, ETH_ALEN);
	else
		memcpy(user_stats->peer_mac, peer->addr, ETH_ALEN);
}

void ath12k_dp_mon_rx_update_basic_stats(struct ath12k_dp_link_peer *peer,
					 struct ath12k_rx_ppdu_stats *rx_stats,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 u32 num_msdu, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	u32 ru_width_factor;
	u64 rx_duration_scaled;
	u16 rx_time_us, num_msdu_retry_count;
	u8 preamble_type, mcs, nss, tid;
	u8 ac = 0;

	if (!rx_stats || !ppdu_info)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU)
		user_stats = &ppdu_info->userstats[uid];

	tid = user_stats ? user_stats->tid : ppdu_info->tid;
	ac = ath12k_tid_to_ac(tid);
	preamble_type = user_stats ? user_stats->preamble_type : ppdu_info->preamble_type;
	mcs = user_stats ? user_stats->mcs : ppdu_info->mcs;
	nss = user_stats ? user_stats->nss : ppdu_info->nss;
	num_msdu_retry_count = user_stats ? user_stats->retried_msdu_count :
			       ppdu_info->retried_msdu_count;

	rx_stats->num_ppdus += 1;
	rx_stats->num_mpdu_retry_count += ppdu_info->mpdu_retry_cnt;
	rx_stats->num_msdu_retry_count += num_msdu_retry_count;
	rx_stats->bw_info = ppdu_info->bw;
	rx_stats->gi_info = ppdu_info->gi;
	rx_stats->mcs_info = mcs;
	rx_stats->nss_info = nss;
	rx_stats->preamble_info = ppdu_info->preamble_type;
	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
		rx_time_us = ppdu_info->rx_duration;
		rx_stats->num_mpdus += ppdu_info->num_mpdu_fcs_ok +
				       ppdu_info->num_mpdu_fcs_err;
	} else {
		/* MU */
		ru_width_factor = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
		if (!ru_width_factor)
			ru_width_factor = 1;

		rx_duration_scaled = ppdu_info->rx_duration * user_stats->nss *
				     user_stats->ul_ofdma_ru_width;
		rx_time_us = (u16)div_u64(rx_duration_scaled, ru_width_factor);
		rx_stats->num_mpdus += user_stats->mpdu_cnt_fcs_ok +
				       user_stats->mpdu_cnt_fcs_err;
	}

	rx_stats->rx_duration_ac[ac] += rx_time_us;
	rx_stats->rx_duration += rx_time_us;

	/* Update per-GI, per-NSS, per-BW, and per-MCS MSDU counts */
	if (ppdu_info->gi < HAL_RX_GI_MAX)
		rx_stats->gi_count[ppdu_info->gi] += num_msdu;

	if (likely(nss) && (nss - 1) < HAL_RX_MAX_NSS)
		rx_stats->nss_count[nss - 1] += num_msdu;

	if (ppdu_info->bw < HAL_RX_BW_MAX)
		rx_stats->bw_count[ppdu_info->bw] += num_msdu;
}

/**
 * ath12k_dp_mon_check_rssi_deauth() - Ultra-lightweight RSSI monitoring
 * @peer: Pointer to peer structure
 * @rssi_dBm: Current RSSI value in dBm (Computed value from signal_stats)
 *
 * Monitors peer RSSI and schedules deauth workqueue if RSSI stays below
 * threshold for grace period. Optimized for minimal CPU overhead:
 *
 */
static void ath12k_dp_mon_check_rssi_deauth(struct ath12k_dp_link_peer *peer,
					    s8 signal_dbm)
{
	struct ath12k_rssi_deauth_config *cfg;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ath12k_sta *ahsta;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return;

	ahsta = ath12k_sta_to_ahsta(ath12k_dp_link_peer_get_sta(peer));
	if (!ahsta)
		return;

	ahvif = ahsta->ahvif;
	if (!ahvif)
		return;

	if (peer->link_id < IEEE80211_MLD_MAX_NUM_LINKS)
		arsta = rcu_dereference(ahsta->link[peer->link_id]);
	if (!arsta)
		arsta = &ahsta->deflink;

	arvif = arsta->arvif;
	if (!arvif)
		return;

	cfg = &arvif->rssi_deauth_cfg;
	if (!cfg) {
		ath12k_generic_dbg(ATH12K_DBG_DATA, ATH12K_DBG_L0,
				   "failed to find the configured rssi threshold for peer_id %d\n",
				   peer->peer_id);
		return;
	}
	peer->rssi_mon.cfg = cfg;

	if (likely(!cfg->enabled))
		return;

	ath12k_generic_dbg(ATH12K_DBG_PEER, ATH12K_DBG_L1,
			   "peer: (%pM vif type: %d low rssi count: %d), cfg (en: %d thres %d grace: %d) last rssi: %d\n",
			   peer->addr, ath12k_dp_link_peer_get_vif_type(peer),
			   peer->rssi_mon.low_rssi_count, cfg->enabled,
			   cfg->rssi_threshold, cfg->grace_samples, signal_dbm);

	peer->rssi_mon.last_rssi = signal_dbm;

	/* RSSI above threshold - reset counters and exit */
	if (signal_dbm >= cfg->rssi_threshold) {
		peer->rssi_mon.low_rssi_count = 0;
		peer->rssi_mon.first_low_jiffies = 0;
		return;
	}

	/* RSSI below threshold */
	/* First time below threshold - record timestamp */
	if (peer->rssi_mon.low_rssi_count == 0)
		peer->rssi_mon.first_low_jiffies = jiffies;

	peer->rssi_mon.low_rssi_count++;

	/* Check if we've hit the grace period */
	if (peer->rssi_mon.low_rssi_count == cfg->grace_samples) {
		ath12k_generic_dbg(ATH12K_DBG_PEER, ATH12K_DBG_L1,
				   "Enqueue peer for deauth: (%pM vif type: %d low rssi count: %d), cfg (en: %d thres %d grace: %d) last rssi: %d\n",
				   peer->addr, ath12k_dp_link_peer_get_vif_type(peer),
				   peer->rssi_mon.low_rssi_count,
				   cfg->enabled, cfg->rssi_threshold,
				   cfg->grace_samples, signal_dbm);

		peer->event.peer_id = peer->peer_id;
		peer->event.common.link_id = peer->link_id;
		peer->event.common.hw_link_id = peer->hw_link_id;
		ath12k_peer_event_set_and_queue(peer, &ahvif->event_queue,
						ATH12K_PEER_EVENT_RSSI_LOW);
	}
}

/**
 * ath12k_dp_mon_rx_update_peer_ppdu_stats() - Update ath12k_rx_ppdu_stats fields
 * @peer: Pointer to link peer structure
 * @rx_ppdu_stats: Pointer to peer's rx_ppdu_stats, may be NULL
 * @ppdu_info: HAL PPDU info parsed from the monitor status ring
 * @pdev_dp: Pointer to pdev DP structure, used for the advance-stats sub-knob
 * @num_msdu: Number of MSDUs seen in this PPDU
 *
 * Caller must only invoke this when rx_ppdu_stats telemetry is enabled -
 * only a NULL pointer check is done here.
 */
static void
ath12k_dp_mon_rx_update_peer_ppdu_stats(struct ath12k_dp_link_peer *peer,
					struct ath12k_rx_ppdu_stats *rx_ppdu_stats,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct ath12k_pdev_dp *pdev_dp,
					u32 num_msdu)
{
	if (!rx_ppdu_stats)
		return;

	rx_ppdu_stats->num_mpdu_fcs_ok += ppdu_info->num_mpdu_fcs_ok;
	rx_ppdu_stats->num_mpdu_fcs_err += ppdu_info->num_mpdu_fcs_err;

	if (ppdu_info->num_mpdu_fcs_ok > 1)
		rx_ppdu_stats->ampdu_msdu_count += num_msdu;
	else
		rx_ppdu_stats->non_ampdu_msdu_count += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_ppdu_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	ath12k_dp_mon_rx_update_basic_stats(peer, rx_ppdu_stats,
					    ppdu_info, num_msdu, 0);

	rx_ppdu_stats->signal_stats = peer->signal_stats;

	/* Update Advance stats */
	if (ath12k_dp_stats_enabled(pdev_dp) &&
	    ath12k_dp_advance_stats_enabled(pdev_dp)) {
		ath12k_dp_mon_rx_update_advance_stats(rx_ppdu_stats,
						      ppdu_info, num_msdu, 0);
		ath12k_dp_rx_update_rate_stats(rx_ppdu_stats, &peer->rxrate);
	}
}

/**
 * ath12k_dp_mon_rx_update_peer_ext_stats() - Update ath12k_rx_peer_stats fields
 * @rx_stats: Pointer to peer's extended rx_stats, may be NULL
 * @ppdu_info: HAL PPDU info parsed from the monitor status ring
 * @num_msdu: Number of MSDUs seen in this PPDU
 *
 * Caller must only invoke this when extended rx stats telemetry is enabled -
 * only a NULL pointer check is done here.
 */
static void
ath12k_dp_mon_rx_update_peer_ext_stats(struct ath12k_rx_peer_stats *rx_stats,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       u32 num_msdu)
{
	if (!rx_stats)
		return;

	rx_stats->num_msdu += num_msdu;
	rx_stats->num_mpdu_fcs_ok += ppdu_info->num_mpdu_fcs_ok;
	rx_stats->num_mpdu_fcs_err += ppdu_info->num_mpdu_fcs_err;

	if (ppdu_info->num_mpdu_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	rx_stats->rx_duration += ppdu_info->rx_duration;

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) {
		ppdu_info->nss = 1;
		ppdu_info->mcs = HAL_RX_MAX_MCS;
		ppdu_info->tid = IEEE80211_NUM_TIDS;
	}

	rx_stats->tcp_msdu_count += ppdu_info->tcp_msdu_count +
				  ppdu_info->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += ppdu_info->udp_msdu_count;
	rx_stats->other_msdu_count += ppdu_info->other_msdu_count;
	rx_stats->dcm_count += ppdu_info->dcm;

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (ppdu_info->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[ppdu_info->tid] += num_msdu;

	if (ppdu_info->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[ppdu_info->preamble_type] += num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (ppdu_info->nss > 0 && ppdu_info->nss <= HAL_RX_MAX_NSS) {
		rx_stats->pkt_stats.nss_count[ppdu_info->nss - 1] += num_msdu;
		rx_stats->byte_stats.nss_count[ppdu_info->nss - 1] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11N &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HT) {
		rx_stats->pkt_stats.ht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.ht_mcs_count[ppdu_info->mcs] +=
			ppdu_info->mpdu_len;
		ppdu_info->mcs = ppdu_info->mcs % 8;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AC &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_VHT) {
		rx_stats->pkt_stats.vht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.vht_mcs_count[ppdu_info->mcs] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HE) {
		rx_stats->pkt_stats.he_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.he_mcs_count[ppdu_info->mcs] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_BE) {
		rx_stats->pkt_stats.be_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.be_mcs_count[ppdu_info->mcs] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BN &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_BN) {
		rx_stats->pkt_stats.bn_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.bn_mcs_count[ppdu_info->mcs] +=
			ppdu_info->mpdu_len;
	}

	if ((ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	     ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) &&
	     ppdu_info->rate < HAL_RX_LEGACY_RATE_INVALID) {
		rx_stats->pkt_stats.legacy_count[ppdu_info->rate] += num_msdu;
		rx_stats->byte_stats.legacy_count[ppdu_info->rate] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		rx_stats->pkt_stats.gi_count[ppdu_info->gi] += num_msdu;
		rx_stats->byte_stats.gi_count[ppdu_info->gi] +=
			ppdu_info->mpdu_len;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		rx_stats->pkt_stats.bw_count[ppdu_info->bw] += num_msdu;
		rx_stats->byte_stats.bw_count[ppdu_info->bw] +=
			ppdu_info->mpdu_len;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats, ppdu_info,
						      NULL, num_msdu);
}

void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_rx_ppdu_stats *rx_ppdu_stats;
	struct ath12k_rx_peer_stats *rx_stats;
	bool extd_rx = ath12k_extd_rx_stats_enabled(pdev_dp);
	bool rx_ppdu_stats_en = ath12k_dp_rx_ppdu_stats_enabled(pdev_dp);
	u32 num_msdu;

	peer = ath12k_dp_link_peer_find_by_peerid_index(pdev_dp->dp, pdev_dp,
							ppdu_info->peer_id);
	ath12k_dp_mon_fill_rx_user_stats_peer_mac(peer, &ppdu_info->userstats[0]);
	if (!peer) {
		if (ppdu_info->peer_id != HAL_INVALID_PEERID)
			ath12k_dbg(pdev_dp->ar->ab, ATH12K_DBG_DATA,
				   "failed to find the peer with monitor peer_id %d\n",
				   ppdu_info->peer_id);
		/* BAR frames often have peer_id=0 so peer lookup fails.
		 * Still update the pdev-level BAR counter so CTRL stats
		 * are always accounted for.
		 */
		if (ppdu_info->user_id < ARRAY_SIZE(ppdu_info->ctrl_frm_info))
			pdev_dp->stats.telemetry_stats.rx_bar_cnt +=
				ppdu_info->ctrl_frm_info[ppdu_info->user_id].bar;
		return;
	}

	num_msdu = ppdu_info->tcp_msdu_count + ppdu_info->tcp_ack_msdu_count +
		   ppdu_info->udp_msdu_count + ppdu_info->other_msdu_count;

	peer->rx_duration += ppdu_info->rx_duration;
	peer->rx_packets += num_msdu;
	peer->rx_bytes += ppdu_info->mpdu_len;
	peer->peer_stats.rx_retries += ppdu_info->mpdu_retry;
	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);
	ath12k_dp_rx_rate_stats_update(ppdu_info, peer, 0);

	if (ppdu_info->user_id < ARRAY_SIZE(ppdu_info->ctrl_frm_info)) {
		pdev_dp->stats.telemetry_stats.rx_bar_cnt +=
			ppdu_info->ctrl_frm_info[ppdu_info->user_id].bar;
		peer->peer_stats.num_bar +=
			ppdu_info->ctrl_frm_info[ppdu_info->user_id].bar;
	}

	rx_ppdu_stats = peer->peer_stats.rx_ppdu_stats;
	rx_stats = peer->peer_stats.rx_stats;

	if (rx_ppdu_stats_en)
		ath12k_dp_mon_rx_update_peer_ppdu_stats(peer, rx_ppdu_stats, ppdu_info,
							pdev_dp, num_msdu);
	if (extd_rx)
		ath12k_dp_mon_rx_update_peer_ext_stats(rx_stats, ppdu_info, num_msdu);

	if (rx_ppdu_stats)
		peer->rx_duration = rx_ppdu_stats->rx_duration;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_su_stats);

void ath12k_dp_mon_rx_process_ulofdma_stats(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *rx_user_status;
	u32 num_users, i, mu_ul_user_v0_word0, mu_ul_user_v0_word1, ru_size;
	u32 ru_width;

	if (!(ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO))
		return;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++) {
		rx_user_status = &ppdu_info->userstats[i];
		mu_ul_user_v0_word0 =
			rx_user_status->ul_ofdma_user_v0_word0;
		mu_ul_user_v0_word1 =
			rx_user_status->ul_ofdma_user_v0_word1;

		if (u32_get_bits(mu_ul_user_v0_word0,
				 HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID) &&
		    !u32_get_bits(mu_ul_user_v0_word0,
				  HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER)) {
			rx_user_status->mcs =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS);
			rx_user_status->nss =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS) + 1;

			ppdu_info->usr_nss_sum += rx_user_status->nss;
			rx_user_status->ofdma_info_valid = 1;
			rx_user_status->ul_ofdma_ru_start_index =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START);

			ru_size = u32_get_bits(mu_ul_user_v0_word1,
					       HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE);
			rx_user_status->ul_ofdma_ru_width = ru_size;
			ru_width = ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(ru_size);
			rx_user_status->ul_ofdma_ru_width = ru_width;
			rx_user_status->ul_ofdma_ru_size = ru_size;
		}
		rx_user_status->ldpc = u32_get_bits(mu_ul_user_v0_word1,
						    HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC);
	}
	ppdu_info->ldpc = 1;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_ulofdma_stats);

/**
 * ath12k_dp_mon_rx_update_user_ppdu_stats() - Update ath12k_rx_ppdu_stats fields
 * @peer: Pointer to link peer structure
 * @rx_ppdu_stats: Pointer to peer's rx_ppdu_stats, may be NULL
 * @ppdu_info: HAL PPDU info parsed from the monitor status ring
 * @user_stats: Per-user HAL status for this uid
 * @pdev_dp: Pointer to pdev DP structure, used for advance stats and telemetry
 * @num_msdu: Number of MSDUs seen from this user in this PPDU
 * @uid: User index within the PPDU
 * @extd_rx: Whether extended rx stats telemetry is enabled, only used to
 *           decide priority of the pdev telemetry snapshot below
 *
 * Caller must only invoke this when rx_ppdu_stats telemetry is enabled -
 * only a NULL pointer check is done here.
 */
static void
ath12k_dp_mon_rx_update_user_ppdu_stats(struct ath12k_dp_link_peer *peer,
					struct ath12k_rx_ppdu_stats *rx_ppdu_stats,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct hal_rx_user_status *user_stats,
					struct ath12k_pdev_dp *pdev_dp,
					u32 num_msdu, u32 uid)
{
	if (!rx_ppdu_stats)
		return;

	rx_ppdu_stats->num_mpdu_fcs_ok += user_stats->mpdu_cnt_fcs_ok;
	rx_ppdu_stats->num_mpdu_fcs_err += user_stats->mpdu_cnt_fcs_err;

	if (user_stats->mpdu_cnt_fcs_ok > 1)
		rx_ppdu_stats->ampdu_msdu_count += num_msdu;
	else
		rx_ppdu_stats->non_ampdu_msdu_count += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_ppdu_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	ath12k_dp_mon_rx_update_basic_stats(peer, rx_ppdu_stats,
					    ppdu_info, num_msdu, uid);

	rx_ppdu_stats->signal_stats = peer->signal_stats;

	/* Update Advance stats */
	if (ath12k_dp_stats_enabled(pdev_dp) &&
	    ath12k_dp_advance_stats_enabled(pdev_dp)) {
		ath12k_dp_mon_rx_update_advance_stats(rx_ppdu_stats,
						      ppdu_info, num_msdu, uid);
		ath12k_dp_rx_update_rate_stats(rx_ppdu_stats, &peer->rxrate);
	}
}

/**
 * ath12k_dp_mon_rx_update_user_ext_stats() - Update ath12k_rx_peer_stats fields
 * @rx_stats: Pointer to peer's extended rx_stats, may be NULL
 * @ppdu_info: HAL PPDU info parsed from the monitor status ring
 * @user_stats: Per-user HAL status for this uid
 * @pdev_dp: Pointer to pdev DP structure, used for the telemetry snapshot
 * @num_msdu: Number of MSDUs seen from this user in this PPDU
 *
 * Caller must only invoke this when extended rx stats telemetry is enabled -
 * only a NULL pointer check is done here.
 */
static void
ath12k_dp_mon_rx_update_user_ext_stats(struct ath12k_rx_peer_stats *rx_stats,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       struct hal_rx_user_status *user_stats,
				       struct ath12k_pdev_dp *pdev_dp,
				       u32 num_msdu)
{
	struct ath12k_rx_peer_rate_stats *pkt, *byt;
	u8 ru_sz;

	if (!rx_stats)
		return;

	rx_stats->num_msdu += num_msdu;
	rx_stats->num_mpdu_fcs_ok += user_stats->mpdu_cnt_fcs_ok;
	rx_stats->num_mpdu_fcs_err += user_stats->mpdu_cnt_fcs_err;

	if (user_stats->mpdu_cnt_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	rx_stats->rx_duration += ppdu_info->rx_duration;

	pkt = &rx_stats->pkt_stats;
	byt = &rx_stats->byte_stats;
	ru_sz = user_stats->ul_ofdma_ru_size;

	rx_stats->tcp_msdu_count += user_stats->tcp_msdu_count +
				    user_stats->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += user_stats->udp_msdu_count;
	rx_stats->other_msdu_count += user_stats->other_msdu_count;
	rx_stats->dcm_count += ppdu_info->dcm;

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (user_stats->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[user_stats->tid] += num_msdu;

	if (user_stats->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[user_stats->preamble_type] +=
			num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (ppdu_info->reception_type ==
	    HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	    ppdu_info->reception_type ==
	    HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO) {
		if (ru_sz < HAL_RX_RU_ALLOC_TYPE_MAX)
			rx_stats->ru_alloc_cnt[ru_sz] += num_msdu;
	}

	if (user_stats->nss > 0 && user_stats->nss <= HAL_RX_MAX_NSS) {
		pkt->nss_count[user_stats->nss - 1] += num_msdu;
		byt->nss_count[user_stats->nss - 1] +=
			user_stats->mpdu_ok_byte_count;
	}

	if (user_stats->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    user_stats->mcs <= HAL_RX_MAX_MCS_HE) {
		pkt->he_mcs_count[user_stats->mcs] += num_msdu;
		byt->he_mcs_count[user_stats->mcs] +=
			user_stats->mpdu_ok_byte_count;
	}

	if (user_stats->preamble_type == HAL_RX_PREAMBLE_11BE &&
	    user_stats->mcs <= HAL_RX_MAX_MCS_BE) {
		pkt->be_mcs_count[user_stats->mcs] += num_msdu;
		byt->be_mcs_count[user_stats->mcs] +=
			user_stats->mpdu_ok_byte_count;
	}

	if (user_stats->preamble_type == HAL_RX_PREAMBLE_11BN &&
	    user_stats->mcs <= HAL_RX_MAX_MCS_BN) {
		pkt->bn_mcs_count[user_stats->mcs] += num_msdu;
		byt->bn_mcs_count[user_stats->mcs] +=
			user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		pkt->gi_count[ppdu_info->gi] += num_msdu;
		byt->gi_count[ppdu_info->gi] +=
			user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		pkt->bw_count[ppdu_info->bw] += num_msdu;
		byt->bw_count[ppdu_info->bw] +=
			user_stats->mpdu_ok_byte_count;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats,
						      ppdu_info,
						      user_stats,
						      num_msdu);
}

static void
ath12k_dp_mon_rx_update_user_stats(struct ath12k_pdev_dp *pdev_dp,
				   struct hal_rx_mon_ppdu_info *ppdu_info,
				   u32 uid)
{
	struct ath12k_rx_peer_stats *rx_stats;
	struct ath12k_rx_ppdu_stats *rx_ppdu_stats;
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_link_peer *peer;
	bool extd_rx = ath12k_extd_rx_stats_enabled(pdev_dp);
	bool rx_ppdu_stats_en = ath12k_dp_rx_ppdu_stats_enabled(pdev_dp);
	u32 num_msdu;
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp_stats *pdev_stats = &pdev_dp->stats;

	if (ppdu_info->peer_id == HAL_INVALID_PEERID)
		return;

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, pdev_dp,
							user_stats->sw_peer_id);
	ath12k_dp_mon_fill_rx_user_stats_peer_mac(peer, user_stats);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON, "peer with peer id %d can't be found\n",
			   ppdu_info->peer_id);
		return;
	}

	num_msdu = user_stats->tcp_msdu_count + user_stats->tcp_ack_msdu_count +
		   user_stats->udp_msdu_count + user_stats->other_msdu_count;
	peer->rx_packets += num_msdu;
	peer->rx_bytes += ppdu_info->mpdu_len;

	peer->peer_stats.rx_retries = user_stats->mpdu_retry;
	peer->rx_duration += ppdu_info->rx_duration;
	ath12k_dp_rx_rate_stats_update(ppdu_info, peer, uid);

	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	if (extd_rx || rx_ppdu_stats_en) {
		ppdu_info->usr_nss_sum += user_stats->nss;
		ppdu_info->usr_ru_tones_sum += user_stats->ul_ofdma_ru_width;
	}

	rx_ppdu_stats = peer->peer_stats.rx_ppdu_stats;
	rx_stats = peer->peer_stats.rx_stats;

	if (rx_ppdu_stats_en)
		ath12k_dp_mon_rx_update_user_ppdu_stats(peer, rx_ppdu_stats, ppdu_info,
							user_stats, pdev_dp, num_msdu,
							uid);
	if (extd_rx)
		ath12k_dp_mon_rx_update_user_ext_stats(rx_stats, ppdu_info, user_stats,
						       pdev_dp, num_msdu);

	pdev_stats->telemetry_stats.rx_data_msdu_cnt = num_msdu;
	pdev_stats->telemetry_stats.total_rx_data_bytes =
		user_stats->mpdu_ok_byte_count;

	/* If rx_ppdu_stats is enabled, the rx_duration will account both SU and MU
	 * duration. Otherwise, rx_duation  will account only SU duration.
	 */
	if (rx_ppdu_stats)
		peer->rx_duration = rx_ppdu_stats->rx_duration;
}

void
ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
				      struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, i;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++)
		ath12k_dp_mon_rx_update_user_stats(pdev_dp, ppdu_info, i);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_mu_stats);

static void
ath12k_dp_mon_ppdu_per_user_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
                                          struct hal_rx_mon_ppdu_info *ppdu_info,
                                          u32 uid)
{
       struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
       struct ath12k_dp_link_peer_stats *stats = NULL;
       struct ath12k_dp_link_peer *peer;
       u32 nss_ru_width_sum = 0;
       u64 temp_result = 0;
       u16 rx_time_us = 0;
       u8 ac = 0;

       if (!dp_pdev)
               return;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU per user rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
		ath12k_dbg_level(dp_pdev->ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L3,
				 "peer stats not found on ppdu peer id %d\n",
				 user_stats->sw_peer_id);
		return;
	}

       nss_ru_width_sum = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
       if (!nss_ru_width_sum)
               nss_ru_width_sum = 1;

       if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU) {
               temp_result = ppdu_info->rx_duration * user_stats->nss *
                             user_stats->ul_ofdma_ru_width;
               rx_time_us = (u16)div_u64(temp_result, nss_ru_width_sum);
       } else
               rx_time_us = ppdu_info->rx_duration;

       ac = ath12k_tid_to_ac(ppdu_info->tid);
       stats = &peer->peer_stats;
       stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption += rx_time_us;
       ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_DP_HTT, "peer: %pM tid: %d ac: %d sum nss: %d tones: %d per user nss: %d tone: %d time: %d cons: %d\n",
                  peer->addr,
                  ppdu_info->tid, ac,
                  ppdu_info->usr_nss_sum, ppdu_info->usr_ru_tones_sum,
                  user_stats->nss, user_stats->ul_ofdma_ru_width,
                  rx_time_us,
                  stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption);
}

#define RSSI_OFFSET 100
static void
ath12k_dp_calc_rx_peer_rssi(struct ath12k_pdev_dp *dp_pdev,
			    struct ath12k_dp_link_peer *link_peer)
{
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_dp_link_peer_rx_signal_stats *stats;
	s8 rssi, rssi_dp;

	if (!ar || !link_peer)
		return;

	stats = &link_peer->signal_stats;
	rssi = ath12k_dp_get_rssi_value(stats->snr, stats, &ar->rssi_offsets,
					false);
	stats->rssi = rssi;
	rssi_dp = ath12k_dp_get_rssi_value(stats->snr_dp, stats,
					   &ar->rssi_offsets, false);
	stats->rssi_dp = rssi_dp;
	ewma_avg_rssi_add(&stats->avg_rssi, (stats->rssi + RSSI_OFFSET) << 8);
	stats->rssi_avg =
		(ewma_avg_rssi_read(&stats->avg_rssi) >> 8) - RSSI_OFFSET;
	ewma_avg_rssi_dp_add(&stats->avg_rssi_dp, (stats->rssi_dp + RSSI_OFFSET) << 8);
	stats->rssi_dp_avg =
		(ewma_avg_rssi_dp_read(&stats->avg_rssi_dp) >> 8) - RSSI_OFFSET;
}

static void
ath12k_dp_mon_link_peer_signal_stats(struct ath12k_pdev_dp *dp_pdev,
				     struct hal_rx_mon_ppdu_info *ppdu_info,
				     u32 uid)
{
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_link_peer_rx_signal_stats *stats = NULL;
	u32 last_rx_rate;
	u16 peer_id;
	u8 pdev_id;
	u8 soc_id;
	u8 i;

	if (!dp_pdev)
		return;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);
	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
	if (!peer) {
		ath12k_dbg_level(dp_pdev->ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L3,
				 "peer stats not found on ppdu peer id %d\n",
				 user_stats->sw_peer_id);
		rcu_read_unlock();
		return;
	}

	stats = &peer->signal_stats;
	stats->snr = ppdu_info->rssi_comb;
	stats->rssi_region_offset = ppdu_info->rssi_region_offset;
	ewma_avg_snr_add(&stats->avg_snr, stats->snr);
	stats->snr_avg = ewma_avg_snr_read(&stats->avg_snr);
	stats->channel_bw = ppdu_info->bw;
	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		stats->rssi_chain_pri20[i] = ppdu_info->rssi_chain_pri20[i];

	if (likely(ppdu_info->fc_valid)) {
		switch (ppdu_info->frame_control & 0x00F0) {
		case IEEE80211_STYPE_DATA:
		case IEEE80211_STYPE_DATA_CFACK:
		case IEEE80211_STYPE_DATA_CFPOLL:
		case IEEE80211_STYPE_DATA_CFACKPOLL:
		case IEEE80211_STYPE_QOS_DATA:
		case IEEE80211_STYPE_QOS_DATA_CFACK:
		case IEEE80211_STYPE_QOS_DATA_CFPOLL:
		case IEEE80211_STYPE_QOS_DATA_CFACKPOLL:
			if ((ppdu_info->preamble_type != HAL_RX_PREAMBLE_11A &&
			     ppdu_info->preamble_type != HAL_RX_PREAMBLE_11B)) {
				stats->snr_dp = ppdu_info->rssi_comb;
				ewma_avg_snr_dp_add(&stats->avg_snr_dp, stats->snr_dp);
				stats->snr_dp_avg =
					ewma_avg_snr_dp_read(&stats->avg_snr_dp);
			}
			break;
		default:
			break;
		}
	}
	ath12k_dp_calc_rx_peer_rssi(dp_pdev, peer);

	if (IS_VALID_RSSI(stats->rssi)) {
		peer->max_rssi = max(peer->max_rssi, stats->rssi);
		peer->min_rssi = min(peer->min_rssi, stats->rssi);
	}

	if (ath12k_dp_rx_ppdu_stats_enabled(dp_pdev) &&
	    peer->peer_stats.rx_ppdu_stats &&
	    IS_VALID_RATE(peer->peer_stats.rx_ppdu_stats->last_rx_rate) &&
	    IS_VALID_RSSI(stats->rssi)) {
		last_rx_rate = peer->peer_stats.rx_ppdu_stats->last_rx_rate;
		soc_id = ath12k_get_ab_device_id(dp_pdev->ar->ab);
		pdev_id = ath12k_get_pdev_id(dp_pdev->ar->pdev);
		peer_id = ath12k_dp_link_peer_get_peer_id(dp_pdev->ar->ab, peer);

		ath12k_telemetry_update_rssi_rate_breach(soc_id,
							 pdev_id,
							 peer_id,
							 peer->addr,
							 PATH_TYPE_RX,
							 stats->rssi,
							 last_rx_rate);
	}
	/* RSSI deauth check */
	ath12k_dp_mon_check_rssi_deauth(peer, stats->rssi);

	rcu_read_unlock();
}

static void
ath12k_dp_mon_per_user_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					u32 uid)
{
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_mon_peer_stats *stats = NULL;
	struct ath12k_dp_link_peer *peer;
	u8 rssi_comb;

	if (!dp_pdev)
		return;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
		ath12k_dbg_level(dp_pdev->ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L3,
				 "peer stats not found on ppdu peer id %d\n",
				 user_stats->sw_peer_id);
		return;
	}

	rssi_comb = ppdu_info->rssi_comb;
	stats = &peer->peer_stats.dp_mon_stats;
	stats->snr = rssi_comb;
	if (unlikely(stats->avg_snr == SNR_INVALID))
		stats->avg_snr = WEIGHTED_AVG_IN(stats->snr);
	else
		WEIGHTED_AVG_UPDATE(stats->avg_snr, stats->snr);
}

void ath12k_dp_mon_ppdu_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       bool is_stat)
{
	u32 num_users, uid;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (uid = 0; uid < num_users; uid++) {
		if (ppdu_info->userstats[uid].sw_peer_id == HAL_INVALID_PEERID)
			continue;
		ath12k_dp_mon_ppdu_per_user_rx_time_update(dp_pdev, ppdu_info, uid);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rx_time_update);

void ath12k_dp_mon_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, uid;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (uid = 0; uid < num_users; uid++) {
		if (ppdu_info->userstats[uid].sw_peer_id == HAL_INVALID_PEERID)
			continue;
		ath12k_dp_mon_per_user_ppdu_rssi_update(dp_pdev, ppdu_info, uid);
		ath12k_dp_mon_link_peer_signal_stats(dp_pdev, ppdu_info, uid);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rssi_update);

static
int ath12k_dp_mon_rx_srng_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int ret;

	ret = ath12k_dp_srng_alloc(ab,
				   &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
				   HAL_RXDMA_MONITOR_BUF, 0, 0,
				   dp_mon->mon_buf_ring_size);
	if (ret) {
		ath12k_warn(dp, "srng alloc failed for HAL_RXDMA_MONITOR_BUF %d\n",
			    ret);
		return ret;
	}

	return 0;
}

static
int ath12k_dp_mon_rx_desc_pool_alloc(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	spin_lock_init(&dp_mon->mon_desc_lock);
	spin_lock_bh(&dp_mon->mon_desc_lock);
	dp_mon->mon_desc_pool = kcalloc(dp_mon->mon_buf_ring_size,
					sizeof(*dp_mon->mon_desc_pool),
					GFP_ATOMIC);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		return -ENOMEM;
	}
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	return 0;
}

int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_dp_mon_rx_srng_alloc(dp);
	if (ret)
		return ret;

	ret = ath12k_dp_mon_rx_desc_pool_alloc(dp);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_setup);

static
void ath12k_dp_mon_rx_srng_free(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;

	srng = &dp_mon->rxdma_mon_buf_ring.refill_buf_ring;

	ath12k_dp_srng_cleanup(ab, srng);
}

static
void ath12k_dp_mon_rx_desc_pool_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	if (!dp_mon->mon_desc_pool)
		return;

	spin_lock_bh(&dp_mon->mon_desc_lock);
	kfree(dp_mon->mon_desc_pool);
	dp_mon->mon_desc_pool = NULL;
	spin_unlock_bh(&dp_mon->mon_desc_lock);
}

void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	ath12k_dp_mon_rx_desc_pool_free(dp);
	ath12k_dp_mon_rx_srng_free(dp);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_cleanup);

static
int ath12k_dp_mon_rx_desc_pool_init(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;

	INIT_LIST_HEAD(&dp_mon->mon_desc_free_list);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		return -EINVAL;
	}

	memset(dp_mon->mon_desc_pool, 0,
	       dp_mon->mon_buf_ring_size * sizeof(*dp_mon->mon_desc_pool));

	for (i = 0; i < dp_mon->mon_buf_ring_size; i++) {
		dp_mon->mon_desc_pool[i].magic = ATH12K_MON_MAGIC_VALUE;
		INIT_LIST_HEAD(&dp_mon->mon_desc_pool[i].list);
		list_add_tail(&dp_mon->mon_desc_pool[i].list,
			      &dp_mon->mon_desc_free_list);
	}

	spin_unlock_bh(&dp_mon->mon_desc_lock);

	return 0;
}

static
int ath12k_dp_mon_rx_buf_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	LIST_HEAD(list);
	struct dp_rxdma_mon_ring *rx_ring;
	size_t req_entries;
	int num_entries, ret = -EINVAL;
	size_t ring_lvl = DP_RXDMA_MONITOR_DEFAULT_RING_FILL_LVL;

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	num_entries =  rx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_BUF);

	rx_ring->bufs_max = num_entries;

	req_entries =
		ath12k_dp_mon_get_rx_free_desc_list(dp, rx_ring, &list, ring_lvl - 1);
	if (req_entries) {
		ret = ath12k_dp_mon_rx_buf_replenish(dp, rx_ring, &list, req_entries);
		if (ret)
			return ret;

		if (rx_ring->bufs_max > ring_lvl)
			rx_ring->bufs_fill_lvl = ring_lvl;
		else
			rx_ring->bufs_fill_lvl = rx_ring->bufs_max;
	} else {
		ath12k_warn(dp, "No required entries available for mon buf ring\n");
		ret = -ENOMEM;
	}

	return ret;
}

int ath12k_dp_mon_rx_ring_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int ret;

	ret = ath12k_dp_srng_init(ab,
				  &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
				  HAL_RXDMA_MONITOR_BUF, 0, 0);
	if (ret) {
		ath12k_warn(dp, "srng init failed for HAL_RXDMA_MONITOR_BUF %d\n",
			    ret);
		return ret;
	}

	ret = ath12k_dp_mon_rx_desc_pool_init(dp);
	if (ret) {
		ath12k_err(ab, "monitor rx desc pool init failed %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_mon_rx_buf_alloc(dp);
	if (ret) {
		ath12k_err(ab, "monitor rx buffer alloc failed %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_ring_init);

static
void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;
	u8 *mon_buf;

	spin_lock_bh(&dp_mon->mon_desc_lock);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		return;
	}

	for (i = 0; i < dp_mon->mon_buf_ring_size; i++) {
		if (dp_mon->mon_desc_pool[i].in_use != DP_MON_DESC_TO_HW)
			continue;

		mon_buf = dp_mon->mon_desc_pool[i].mon_buf;
		if (!mon_buf)
			goto reset_mon_desc;

		ath12k_core_dma_unmap_page(dp->dev, dp_mon->mon_desc_pool[i].paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE, DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->mon_desc_pool[i].mon_buf = NULL;
		dp_mon->num_frag_free++;

reset_mon_desc:
		ath12k_dp_mon_desc_reset(&dp_mon->mon_desc_pool[i]);
	}
	spin_unlock_bh(&dp_mon->mon_desc_lock);
}

void ath12k_dp_mon_rx_ring_deinit(struct ath12k_dp *dp)
{
	ath12k_dp_mon_rx_buf_free(dp);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_ring_deinit);

int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int ret;

	ring_id = dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					  0, HAL_RXDMA_MONITOR_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rxdma_mon_buf_ring %d\n",
			    ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_htt_srng_setup);

int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_dp_srng_alloc(dp->ab,
					   &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i],
					   HAL_RXDMA_MONITOR_DST,
					   0, mac_id + i,
					   dp_mon->mon_dst_ring_size);
		if (ret) {
			ath12k_warn(dp->ab,
				    "srng alloc is failed for  HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
		ret = ath12k_dp_srng_init(dp->ab,
					  &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i],
					  HAL_RXDMA_MONITOR_DST,
					  0, mac_id + i);
		if (ret) {
			ath12k_warn(dp->ab,
				    "srng init is failed for HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_setup);

int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 ring_id;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(dp->ab, ring_id,
						  mac_id + i,
						  HAL_RXDMA_MONITOR_DST);
		if (ret) {
			ath12k_warn(dp->ab,
				    "failed to configure rxdma_mon_dst_ring %d %d\n",
				    i, ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_htt_srng_setup);

void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon)
{
	INIT_LIST_HEAD(&pmon->dp_rx_mon_mpdu_list);
	pmon->mon_mpdu = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_mpdu_list_init);

void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int i;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++)
		ath12k_dp_srng_cleanup(dp->ab,
				       &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i]);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_cleanup);

void ath12k_dp_mon_cfg_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	dp_mon->mon_status_ring_size = DP_RXDMA_MON_STATUS_RING_SIZE(ab);
	dp_mon->mon_desc_ring_size = DP_RXDMA_MONITOR_DESC_RING_SIZE(ab);
	dp_mon->mon_buf_ring_size = DP_RXDMA_MONITOR_BUF_RING_SIZE(ab);
	dp_mon->mon_dst_ring_size = DP_RXDMA_MONITOR_DST_RING_SIZE(ab);
	dp_mon->mon_num_ppdu_desc = DP_MON_NUM_PPDU_DESC(ab);
}
EXPORT_SYMBOL(ath12k_dp_mon_cfg_init);

int ath12k_dp_mon_init(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon;
	struct ath12k_dp_tx_mon *dp_tx_mon;

	dp_mon = kzalloc(sizeof(*dp_mon), GFP_KERNEL);
	if (!dp_mon)
		return -ENOMEM;

	dp_mon->dp = dp;
	dp->dp_mon = dp_mon;

	if (ath12k_dp_tx_mon_feature_eval(dp)) {
		dp_tx_mon = kzalloc(sizeof(*dp_tx_mon), GFP_KERNEL);
		if (!dp_tx_mon) {
			dp->dp_mon = NULL;
			kfree(dp_mon);
			return -ENOMEM;
		}

		spin_lock_init(&dp_tx_mon->tx_mon_desc_lock);
		INIT_LIST_HEAD(&dp_tx_mon->tx_mon_desc_free_list);
		dp_tx_mon->tx_mon_buf_ring_ready = false;
		dp_mon->dp_tx_mon = dp_tx_mon;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_init);

void ath12k_dp_mon_deinit(struct ath12k_dp *dp)
{
	if (dp->dp_mon) {
		kfree(dp->dp_mon->dp_tx_mon);
		dp->dp_mon->dp_tx_mon = NULL;
		kfree(dp->dp_mon);
	}
	dp->dp_mon = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_deinit);

int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_tx_mon *dp_pdev_tx_mon;

	dp_mon_pdev = kzalloc(sizeof(*dp_mon_pdev), GFP_KERNEL);
	if (!dp_mon_pdev)
		return -ENOMEM;

	if (ath12k_dp_tx_mon_feature_eval(dp_pdev->dp)) {
		struct ath12k_dp_mon *dp_mon = dp_pdev->dp->dp_mon;

		if (dp_mon->dp_tx_mon) {
			dp_pdev_tx_mon = kzalloc(sizeof(*dp_pdev_tx_mon), GFP_KERNEL);
			if (!dp_pdev_tx_mon) {
				kfree(dp_mon_pdev);
				return -ENOMEM;
			}

			dp_mon_pdev->dp_pdev_tx_mon = dp_pdev_tx_mon;
			dp_pdev_tx_mon->mon_pdev = dp_mon_pdev;
			dp_pdev_tx_mon->dp_tx_mon = dp_mon->dp_tx_mon;
		}
	}

	dp_mon_pdev->dp_pdev = dp_pdev;
	dp_mon_pdev->dp_mon = dp_pdev->dp->dp_mon;
	dp_pdev->dp_mon_pdev = dp_mon_pdev;

	dp_mon_pdev->smart_mon_filter =
		ath12k_mac_get_cached_smart_mon_filter(dp_pdev);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_alloc);

void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_mac_cache_smart_mon_filter(dp_pdev,
					  dp_pdev->dp_mon_pdev->smart_mon_filter);
	kfree(dp_pdev->dp_mon_pdev->dp_pdev_tx_mon);
	dp_pdev->dp_mon_pdev->dp_pdev_tx_mon = NULL;
	kfree(dp_pdev->dp_mon_pdev);
	dp_pdev->dp_mon_pdev = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_free);

void ath12k_dp_mon_pdev_rx_detach(struct ath12k_pdev_dp *dp_pdev)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	if (mon_ops && mon_ops->cleanup_mon_link_desc)
		mon_ops->cleanup_mon_link_desc(dp_pdev);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_detach);

void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = &dp_mon_pdev->mon_data;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	int i;

	skb_queue_head_init(&pmon->rx_status_q);

	pmon->mon_ppdu_status = DP_PPDU_STATUS_START;

	memset(&pmon->rx_mon_stats, 0,
	       sizeof(pmon->rx_mon_stats));

	pmon->mon_last_linkdesc_paddr = 0;
	pmon->mon_last_buf_cookie = DP_RX_DESC_COOKIE_MAX + 1;
	spin_lock_init(&pmon->mon_lock);

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	if (mon_ops && mon_ops->setup_mon_link_desc)
		mon_ops->setup_mon_link_desc(dp_pdev);

	if (mon_ops && mon_ops->mon_pdev_rx_mpdu_list_init)
		mon_ops->mon_pdev_rx_mpdu_list_init(pmon);

	INIT_LIST_HEAD(&dp_mon_pdev->mon_desc_used_list);

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		skb_queue_head_init(&ppdu_info->mpdu_q[i]);

	ppdu_info->peer_id = HAL_INVALID_PEERID;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_attach);

void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_extd_rx_stats_enabled(dp_pdev) ||
	    ath12k_dp_rx_ppdu_stats_enabled(dp_pdev))
		mode = ATH12k_DP_MON_EXTD_STATS;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ar->ab->dev_flags))
		 mode = ATH12k_DP_MON_EXTD_STATS;
#endif
	ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_enable);

void ath12k_dp_mon_rx_stats_disable(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (mode == ATH12k_DP_MON_EXTD_STATS) {
		/* while disabling extd rx stats, Basic stats filters
		 * to be configured.
		 */
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
		mode = ATH12k_DP_MON_BASIC_STATS;
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     true);
	} else {
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_disable);

void ath12k_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_set);

void ath12k_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_reset);

void ath12k_dp_mon_rx_nrp_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_set);

void ath12k_dp_mon_rx_nrp_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_reset);

void ath12k_dp_mon_rx_smart_mon_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_smart_mon_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_smart_mon_set);

void ath12k_dp_mon_rx_smart_mon_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_smart_mon_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_smart_mon_reset);

static void ath12k_dp_mon_clear_pdev_airtime_stats(struct ath12k *ar)
{
       struct ath12k_pdev_dp *pdev_dp = &ar->dp;
       u8 ac;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               pdev_dp->stats.telemetry_stats.tx_link_airtime[ac] = 0;
               pdev_dp->stats.telemetry_stats.rx_link_airtime[ac] = 0;
       }
}

u64 ath12k_get_timestamp_in_us(void)
{
       struct timespec64 ts;

       ktime_get_ts64(&ts);

       return ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

void ath12k_dp_mon_peer_update_telemetry_stats(struct ath12k_pdev_dp *dp_pdev,
					       struct ath12k_dp_link_peer *peer,
					       void *context)
{
       struct ath12k_mon_peer_airtime_stats *airtime_stats;
       struct peer_airtime_consumption *peer_consump;
       u64 current_time = ath12k_get_timestamp_in_us();
       u32 remainder, time_diff;
       u32 usage;
       u16 consump_per_sec;
	struct ath12k_pdev_dp_stats *pdev_stats = &dp_pdev->stats;
       u8 ac;
	struct ath12k_atf_peer_airtime *atf_airtime;

       airtime_stats = &peer->peer_stats.dp_mon_stats.mon_stats;
       time_diff = (u32)(current_time - airtime_stats->last_update_time);
	atf_airtime = &peer->atf_peer_airtime;

	if (!time_diff)
		return;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               /* *_link_airtime refers to the amount of time a peer spends
                * transmitting or receiving data over the medium
                * To calculate total pdev's airtime cosumption, then store
                * each peer airtime consumption in pdev telemetry link airtime
                */

               /* Tx Airtime Consumption */
               peer_consump = &airtime_stats->tx_airtime_consumption[ac];
		atf_airtime->tx_airtime_consumption[ac].consumption +=
			airtime_stats->tx_airtime_consumption[ac].consumption;
		atf_airtime->tx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.tx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.tx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

               /* Rx Airtime Consumption */
               peer_consump = &airtime_stats->rx_airtime_consumption[ac];
		atf_airtime->rx_airtime_consumption[ac].consumption +=
			airtime_stats->rx_airtime_consumption[ac].consumption;
		atf_airtime->rx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.rx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.rx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

		ath12k_dbg(dp_pdev->dp->ab,
			   ATH12K_DBG_DP_HTT,
			   "peer: %pM time diff: %d link air tx: %d rx: %d cons tx: %d rx: %d\n",
			    peer->addr, time_diff,
			    pdev_stats->telemetry_stats.tx_link_airtime[ac],
			    pdev_stats->telemetry_stats.rx_link_airtime[ac],
			    airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec,
			    airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec);
       }

       airtime_stats->last_update_time = current_time;
}

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             const int pdev_id)
{
       struct ath12k_pdev *pdev;
	struct ath12k_pdev_dp *dp_pdev;

       rcu_read_lock();
       pdev = rcu_dereference(ab->pdevs_active[pdev_id]);
       if (!pdev) {
               rcu_read_unlock();
               return -EINVAL;
       }

       if (pdev->ar)
               ath12k_dp_mon_clear_pdev_airtime_stats(pdev->ar);

	dp_pdev = &pdev->ar->dp;

	ath12k_dp_link_peer_iterate_by_dp_pdev(dp_pdev,
					       ath12k_dp_mon_peer_update_telemetry_stats,
					       NULL);
	rcu_read_unlock();

       return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_update_telemetry_stats);

void ath12k_dp_mon_peer_telemetry_stats(const struct ath12k_dp_link_peer *peer,
					struct ath12k_peer_telemetry_stats *stats)
{
       const struct ath12k_dp_mon_peer_stats *dp_stats;
       u8 ac;

       dp_stats = &peer->peer_stats.dp_mon_stats;
       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               stats->tx_airtime_consumption[ac] =
                       dp_stats->mon_stats.tx_airtime_consumption[ac].avg_consumption_per_sec;
               stats->rx_airtime_consumption[ac] =
                       dp_stats->mon_stats.rx_airtime_consumption[ac].avg_consumption_per_sec;
       }
	stats->snr = dp_stats->avg_snr;
}

static inline struct sk_buff *ath12k_mon_get_last_skb_from_fraglist(struct sk_buff *skb)
{
	struct sk_buff *last_skb;

	for (last_skb = skb_shinfo(skb)->frag_list;
	     last_skb && last_skb->next;
	     last_skb = last_skb->next)
		;

	return last_skb;
}

struct sk_buff *
ath12k_dp_mon_get_skb_valid_frag(struct ath12k_dp *dp, struct sk_buff *skb)
{
	struct sk_buff *last_skb;
	u32 num_frags;

	if (unlikely(!skb)) {
		ath12k_warn(dp, "invalid skb, cannot retrieve valid skb\n");
		return NULL;
	}

	num_frags = skb_shinfo(skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return skb;

	if (unlikely(!skb_has_frag_list(skb)))
		return NULL;

	last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
	if (unlikely(!last_skb)) {
		ath12k_warn(dp, "frag_list present but no valid last skb found\n");
		return NULL;
	}

	num_frags = skb_shinfo(last_skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return last_skb;

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
		   "no skb with available frag slots found in skb or frag_list\n");
	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_skb_valid_frag);

void ath12k_dp_mon_update_skb_len(struct sk_buff *skb_head, u32 frag_len)
{
	skb_head->data_len += frag_len;
	skb_head->len += frag_len;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_skb_len);

void ath12k_dp_mon_append_skb(struct sk_buff *skb, struct sk_buff *tmp_skb)
{
	struct sk_buff *last_skb;

	if (unlikely(!skb_has_frag_list(skb))) {
		skb_shinfo(skb)->frag_list = tmp_skb;
	} else {
		last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
		if (last_skb)
			last_skb->next = tmp_skb;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_append_skb);

void ath12k_dp_mon_skb_remove_frag(struct ath12k_dp *dp, struct sk_buff *skb,
				   u16 idx, u16 truesize)
{
	struct page *page;
	u16 frag_len;

	page = skb_frag_page(&skb_shinfo(skb)->frags[idx]);
	if (unlikely(!page))
		return;

	frag_len = ath12k_dp_mon_get_frag_size_by_idx(dp, skb, idx);
	put_page(page);
	skb->len -= frag_len;
	skb->data_len -= frag_len;
	skb->truesize -= truesize;
	skb_shinfo(skb)->nr_frags--;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_remove_frag);

void ath12k_dp_mon_add_rx_frag(struct sk_buff *skb, const void *mon_buf,
			       int offset, int frag_len, bool take_frag_ref)
{
	struct page *page = virt_to_head_page(mon_buf);
	int frag_offset = mon_buf - page_address(page);
	int nr_frags = skb_shinfo(skb)->nr_frags;

	skb_add_rx_frag(skb, nr_frags, page,
			(frag_offset + offset), frag_len,
			ATH12K_DP_MON_RX_BUF_SIZE);

	if (unlikely(take_frag_ref))
		skb_frag_ref(skb, nr_frags);
}
EXPORT_SYMBOL(ath12k_dp_mon_add_rx_frag);

static int
ath12k_dp_mon_rx_set_low_threshold(struct ath12k_dp *dp,
				   struct dp_rxdma_mon_ring *rx_ring,
				   u32 low_threshold)
{
	struct ath12k_base *ab = dp->ab;
	u32 ring_id;
	struct hal_srng *srng;
	int ret;

	ring_id = rx_ring->refill_buf_ring.ring_id;
	srng = &dp->hal->srng_list[ring_id];
	ath12k_hal_set_low_threshold(srng, (low_threshold >> 1));
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					  0, HAL_RXDMA_MONITOR_BUF);
	if (ret) {
		ath12k_info(ab, "failed to send HTT SRNG setup for monitor buf ring %d\n",
			    ret);
		return ret;
	}

	return 0;
}

int ath12k_dp_mon_rx_monitor_mode_buf_setup(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	size_t req_entries;
	LIST_HEAD(list);
	int ret = 0;

	if (unlikely(!dp_mon))
		return -EINVAL;

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	/* Ensure monitor RX ring is fully replenished now that the monitor VAP
	 * is being started. At boot we only partially filled the ring up to
	 * DP_RXDMA_MONITOR_DEFAULT_RING_FILL_LVL entries if the ring is large;
	 * here we replenish the remaining free entries so that the ring is full.
	 */
	if (rx_ring->bufs_fill_lvl < rx_ring->bufs_max) {
		req_entries =
			ath12k_dp_mon_get_rx_free_desc_list(dp, rx_ring, &list,
							    (rx_ring->bufs_max -
							    rx_ring->bufs_fill_lvl - 1));
		if (req_entries) {
			ret = ath12k_dp_mon_rx_buf_replenish(dp, rx_ring, &list,
							     req_entries);
			if (ret)
				return ret;

			rx_ring->bufs_fill_lvl += (rx_ring->bufs_max -
						   rx_ring->bufs_fill_lvl);
		} else {
			ath12k_warn(dp, "No required entries available for mon buf ring\n");
			return -ENOMEM;
		}

		ret = ath12k_dp_mon_rx_set_low_threshold(dp, rx_ring, rx_ring->bufs_max);
		if (ret)
			return ret;
	}

	return ret;
}

void ath12k_dp_mon_rx_process_low_thres(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	struct hal_srng *srng;
	int num_free, req_entries;
	LIST_HEAD(list);

	rx_ring = &dp_mon->rxdma_mon_buf_ring;
	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);
	spin_lock_bh(&dp_mon->mon_desc_lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);

	if (num_free < (rx_ring->bufs_max - rx_ring->bufs_fill_lvl)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		spin_unlock_bh(&srng->lock);
		return;
	}
	num_free = num_free - (rx_ring->bufs_max - rx_ring->bufs_fill_lvl);

	/* if ring is less than half filled need to replenish */
	if (num_free < (rx_ring->bufs_fill_lvl / 2)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		spin_unlock_bh(&srng->lock);
		return;
	}

	ath12k_hal_srng_access_end(ab, srng);

	req_entries = ath12k_dp_mon_list_cut_nodes(&list,
						   &dp->dp_mon->mon_desc_free_list,
						   num_free);
	spin_unlock_bh(&dp_mon->mon_desc_lock);
	spin_unlock_bh(&srng->lock);

	if (req_entries)
		ath12k_dp_mon_rx_buf_replenish(dp, rx_ring, &list, req_entries);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_low_thres);

void
ath12k_dp_mon_cnt_skb_and_frags(struct sk_buff *skb, u32 *skb_count, u32 *frag_count)
{
	struct sk_buff *iter;
	u32 total_skb = 0, total_frags = 0;

	if (unlikely(!skb))
		return;

	total_skb++;
	total_frags += skb_shinfo(skb)->nr_frags;

	if (skb_has_frag_list(skb)) {
		for (iter = skb_shinfo(skb)->frag_list; iter; iter = iter->next) {
			total_skb++;
			total_frags += skb_shinfo(iter)->nr_frags;
		}
	}

	*skb_count += total_skb;
	*frag_count += total_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_cnt_skb_and_frags);

u32 ath12k_dp_mon_get_frag_size_by_idx(struct ath12k_dp *dp,
				       struct sk_buff *skb, u8 idx)
{
	u32 size = 0;

	if (likely(idx < MAX_SKB_FRAGS))
		size = skb_frag_size(&skb_shinfo(skb)->frags[idx]);

	return size;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_frag_size_by_idx);

int ath12k_dp_mon_get_puncture_type(u16 puncture_pattern, u8 bw)
{
	u16 mask;
	u8 punctured_bits;

	if (!puncture_pattern)
		return NO_PUNCTURE;

	switch (bw) {
	case HAL_RX_BW_80MHZ:
		mask = PUNCTURE_80MHZ_MASK;
		break;
	case HAL_RX_BW_160MHZ:
		mask = PUNCTURE_160MHZ_MASK;
		break;
	case HAL_RX_BW_320MHZ:
		mask = PUNCTURE_320MHZ_MASK;
		break;
	default:
		return NO_PUNCTURE;
	}

	/* 0s in puncture pattern received in TLV indicates punctured 20Mhz,
	 * after complement, 1s will indicate punctured 20Mhz
	 */
	puncture_pattern = ~puncture_pattern;
	puncture_pattern &= mask;

	if (puncture_pattern) {
		punctured_bits = 0;
		while (puncture_pattern != 0) {
			punctured_bits++;
			puncture_pattern &= (puncture_pattern - 1);
		}

		if (bw == HAL_RX_BW_80MHZ) {
			if (punctured_bits == PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == HAL_RX_BW_160MHZ) {
			if (punctured_bits == PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else if (punctured_bits == PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == HAL_RX_BW_320MHZ) {
			if (punctured_bits == PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else if (punctured_bits == PUNC_MINUS80MHZ)
				return PUNCTURED_80MHZ;
			else if (punctured_bits == PUNC_MINUS120MHZ)
				return PUNCTURED_120MHZ;
			else
				return NO_PUNCTURE;
		}
	}
	return NO_PUNCTURE;
}

void *ath12k_dp_mon_skb_get_frag_addr(struct sk_buff *skb, u8 idx)
{
	void *frag = NULL;

	if (likely(idx < MAX_SKB_FRAGS))
		frag = skb_frag_address(&skb_shinfo(skb)->frags[idx]);

	return frag;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_get_frag_addr);

int ath12k_dp_mon_adj_frag_offset(struct sk_buff *skb, u8 idx, int offset)
{
	u32 frag_offset;
	skb_frag_t *frag;

	if (unlikely(idx >= skb_shinfo(skb)->nr_frags))
		return -EINVAL;

	frag = &skb_shinfo(skb)->frags[idx];
	frag_offset = skb_frag_off(frag);
	frag_offset += offset;
	skb_frag_off_set(frag, frag_offset);
	skb_coalesce_rx_frag(skb, idx, -(offset), 0);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_adj_frag_offset);

u32 ath12k_dp_mon_get_num_frags_in_fraglist(struct sk_buff *skb)
{
	struct sk_buff *list = NULL;
	u32 num_frags = skb_shinfo(skb)->nr_frags;

	skb_walk_frags(skb, list)
		num_frags += skb_shinfo(list)->nr_frags;

	return num_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_num_frags_in_fraglist);

/**
 * ath12k_dp_mon_reset_ppdu_desc() - Reset PPDU descriptor
 * @ppdu_desc: PPDU descriptor to reset
 *
 * Resets the PPDU descriptor by clearing the status descriptor array and
 * resetting the count. This prepares the descriptor for reuse in TX and RX
 * monitor processing.
 */
void
ath12k_dp_mon_reset_ppdu_desc(struct ath12k_dp_mon_ppdu_desc *ppdu_desc)
{
	memset(ppdu_desc->status_desc, 0,
	       ppdu_desc->status_desc_cnt * sizeof(*ppdu_desc->status_desc));
	ppdu_desc->status_desc_cnt = 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_reset_ppdu_desc);

void
ath12k_dp_mon_fill_rx_rate(struct ath12k_pdev_dp *dp_pdev,
			   struct hal_rx_mon_ppdu_info *ppdu_info,
			   struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_supported_band *sband;
	struct ath12k *ar = dp_pdev->ar;
	enum rx_msdu_start_pkt_type pkt_type;
	u8 rate_mcs, nss, sgi, bw;
	bool is_cck;

	pkt_type = ppdu_info->preamble_type;
	rate_mcs = ppdu_info->mcs;
	nss = ppdu_info->nss;
	sgi = ppdu_info->gi;
	bw = ppdu_info->bw;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		if (rx_status->band < NUM_NL80211_BANDS) {
			sband = &ar->mac.sbands[rx_status->band];
			rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
									is_cck);
		}
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in HT mode %d\n",
				     rate_mcs);
			break;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in VHT mode %d\n",
				     rate_mcs);
			break;
		}
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in HE mode %d\n",
				    rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in EHT mode %d\n",
				    rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_EHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->eht.gi = ath12k_eht_gi_to_nl80211_eht_gi(sgi);
		break;
	case RX_MSDU_START_PKT_TYPE_11BN:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_UHR_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in UHR mode %d\n",
				    rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_UHR;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;

		/*
		 * We fill EHT params for UHR mode as well since
		 * the APIs such as _cfg80211_calculate_bitrate_eht_uhr() etc.
		 * remain common and use EHT params to calculate Rx Bit rate etc.
		 */
		rx_status->eht.gi = ath12k_uhr_gi_to_nl80211_uhr_gi(sgi);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "monitor receives invalid preamble type %d",
			    pkt_type);
		break;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_rate);

int ath12k_dp_mon_get_link_peer_rssi(void *ptr, const u8 *peer_mac,
				     s8 *min_rssi, s8 *max_rssi)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = (struct ath12k_dp_peer *)ptr;

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_mac_addr(dp_peer, peer_mac);
	if (!link_peer) {
		rcu_read_unlock();
		ath12k_dbg(NULL, ATH12K_DBG_DP_MON,
			   "dp_mon: link_peer not found for %pM\n", peer_mac);
		return -ENOENT;
	}

	*min_rssi = link_peer->min_rssi;
	*max_rssi = link_peer->max_rssi;

	rcu_read_unlock();

	ath12k_dbg(NULL, ATH12K_DBG_DP_MON,
		   "dp_mon: Retrieved RSSI for %pM: min=%d, max=%d\n",
		   peer_mac, *min_rssi, *max_rssi);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_link_peer_rssi);

static void
ath12k_dp_ext_mon_update_rx_config(struct ath12k_pdev_mon_dp *dp_mon_pdev,
				   const struct ath12k_ext_mon_filter_config *new_config)
{
	struct ath12k_dp_rx_ext_mon *curr_config;

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	curr_config = dp_mon_pdev->rx_ext_mon_config;

	if (!new_config->disable) {
		curr_config->level = new_config->level;

		curr_config->fp = new_config->all_peer;
		curr_config->mo = new_config->all_neighbor;
		curr_config->fpmo = new_config->target_peer;
		curr_config->md = new_config->target_neighbor;

		curr_config->fp_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->all_peer);
		curr_config->mo_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->all_neighbor);
		curr_config->fpmo_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->target_peer);
		curr_config->md_enabled =
			ath12k_dp_ext_mon_is_mode_enabled(&new_config->target_neighbor);

		curr_config->metadata = new_config->meta_data;
		curr_config->short_pkt_en =
				!!ath12k_ext_mon_get_max_shortpkt_len(curr_config);
		curr_config->enable = true;
	} else {
		curr_config->enable = false;
		curr_config->level = 0;
		curr_config->metadata = 0;
		curr_config->short_pkt_en = false;
		curr_config->fp_enabled = false;
		curr_config->mo_enabled = false;
		curr_config->fpmo_enabled = false;
		curr_config->md_enabled = false;
		memset(&curr_config->fp, 0, sizeof(curr_config->fp));
		memset(&curr_config->mo, 0, sizeof(curr_config->mo));
		memset(&curr_config->fpmo, 0, sizeof(curr_config->fpmo));
		memset(&curr_config->md, 0, sizeof(curr_config->md));
		curr_config->fp.data_mpdu_tlv.mcast = 0xF;
		curr_config->fp.data_mpdu_tlv.ucast = 0xF;
		curr_config->fp.data_mpdu_tlv.null_frm = 0xF;
		curr_config->fpmo.data_mpdu_tlv.mcast = 0xF;
		curr_config->fpmo.data_mpdu_tlv.ucast = 0xF;
		curr_config->fpmo.data_mpdu_tlv.null_frm = 0xF;
	}
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
}

static
void ath12k_dp_ext_mon_enable_mac_ext_mon(struct ath12k_pdev_dp *dp_pdev,
					  bool enable)
{
	struct ieee80211_vif *mon_vif = NULL;
	struct ath12k_link_vif *arvif = NULL;

	list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
		    arvif->is_started) {
			mon_vif = arvif->ahvif->vif;
			break;
		}
	}
	if (mon_vif)
		ieee80211_enable_ext_monitor(mon_vif, enable);
}

static void
ath12k_dp_ext_mon_config_offchan_capture(struct ath12k_pdev_dp *dp_pdev, bool enable)
{
	struct ath12k_link_vif *arvif;

	list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
		if (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR &&
		    arvif->is_started) {
			ieee80211_enable_offchan_packet_capture(arvif->ahvif->vif,
								enable);
			return;
		}
	}
	ath12k_warn(dp_pdev->dp,
		    "no active monitor vdev found for offchan capture\n");
}

static
int ath12k_dp_ext_mon_set_rx_filter(struct ath12k_pdev_dp *dp_pdev,
				    const struct ath12k_ext_mon_filter_config *new_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;
	bool already_enabled;
	bool offchan_capture_enabled;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (unlikely(!rx_ext_mon)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "rx_ext_mon_config is null\n");
		return -EINVAL;
	}

	if (new_config->disable && !rx_ext_mon->enable) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "already disabled\n");
		return -EINVAL;
	}

	already_enabled = rx_ext_mon->enable;
	offchan_capture_enabled =
		!!(rx_ext_mon->metadata & ATH12K_EXT_MON_METADATA_OFFCHAN_PKT);
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	if (new_config->disable) {
		ath12k_dp_ext_mon_rx_config_filter(dp_pdev, false);
		ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, true);
		ret = ath12k_dp_mon_rx_update_ring_filter(dp_pdev);
		if (ret) {
			ath12k_warn(dp_pdev->dp,
				    "failed to update filter for disable: %d\n", ret);
			ath12k_dp_ext_mon_rx_config_filter(dp_pdev, true);
			ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, false);
			return ret;
		}

		/*
		 * If off-channel packet capture was previously enabled,
		 * disable it now that ext mon is being disabled.
		 */
		if (offchan_capture_enabled)
			ath12k_dp_ext_mon_config_offchan_capture(dp_pdev, false);

		ath12k_dp_ext_mon_update_rx_config(dp_mon_pdev, new_config);
		ath12k_dp_ext_mon_enable_mac_ext_mon(dp_pdev, false);
	} else {
		if (already_enabled)
			ath12k_dp_ext_mon_rx_config_filter(dp_pdev, false);
		else
			ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, false);

		ath12k_dp_ext_mon_update_rx_config(dp_mon_pdev, new_config);
		ath12k_dp_ext_mon_rx_config_filter(dp_pdev, true);
		ret = ath12k_dp_mon_rx_update_ring_filter(dp_pdev);
		if (ret) {
			ath12k_warn(dp_pdev->dp,
				    "failed to update ring filter: %d\n", ret);
			ath12k_dp_ext_mon_rx_config_filter(dp_pdev, false);
			if (!already_enabled)
				ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, true);
			return ret;
		}
		ath12k_dp_ext_mon_enable_mac_ext_mon(dp_pdev, true);

		/*
		 * If the off-channel packet capture bit is set in metadata,
		 * enable IEEE80211_SDATA_OFFCHAN_PACKETS on the monitor VIF
		 * so that off-channel frames are delivered to this interface.
		 */
		if (new_config->meta_data & ATH12K_EXT_MON_METADATA_OFFCHAN_PKT)
			ath12k_dp_ext_mon_config_offchan_capture(dp_pdev, true);
	}

	return ret;
}

static int
ath12k_dp_ext_mon_add_rx_peers(struct ath12k_pdev_dp *dp_pdev,
				const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;
	struct ath12k_set_neighbor_rx_params param = {0};
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	const struct ath12k_ext_mon_peer_info *peer_info;
	struct ath12k_link_vif *arvif;
	LIST_HEAD(peers_to_wmi);
	int staged_count = 0;
	bool found;
	int ret = 0;
	int i;

	param.action = WMI_FILTER_NRP_ACTION_ADD;
	param.vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	if (param.vdev_id == -1) {
		ath12k_warn(dp_pdev->dp, "no active monitor vdev found\n");
		return -ENODEV;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (unlikely(!rx_ext_mon)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "rx_ext_mon_config is null\n");
		return -EINVAL;
	}

	if (rx_ext_mon->peer_count + peer_config->count >
	    ATH12K_EXT_MON_NUM_PEERS(dp_pdev->dp->ab)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp,
			    "adding %u peers would exceed max %d (current: %u)\n",
			    peer_config->count,
			    ATH12K_EXT_MON_NUM_PEERS(dp_pdev->dp->ab),
			    rx_ext_mon->peer_count);
		return -EINVAL;
	}

	for (i = 0; i < peer_config->count; i++) {
		peer_info = &peer_config->peer_info[i];
		found = false;

		list_for_each_entry(arvif, &dp_pdev->ar->arvifs, list) {
			if (ether_addr_equal(arvif->bssid, peer_info->mac_addr)) {
				found = true;
				ath12k_err(dp_pdev->dp->ab,
					   "skipped adding bssid %pM as neighbor peer\n",
					   peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		list_for_each_entry(peer, &rx_ext_mon->peer_list, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					peer_info->mac_addr) &&
					peer->peer_info.ra_addr == peer_info->ra_addr) {
				found = true;
				ath12k_warn(dp_pdev->dp, "peer %pM already added\n",
					    peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		list_for_each_entry(peer, &peers_to_wmi, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					     peer_info->mac_addr)) {
				found = true;
				ath12k_warn(dp_pdev->dp,
					    "peer %pM duplicate in request\n",
					    peer_info->mac_addr);
				break;
			}
		}
		if (found)
			continue;

		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer)
			continue;

		memcpy(&peer->peer_info, peer_info, sizeof(*peer_info));
		if (peer_info->ra_addr) {
			list_add_tail(&peer->list, &rx_ext_mon->peer_list);
			rx_ext_mon->peer_count++;
			rx_ext_mon->ra_peer_count++;
		} else {
			list_add_tail(&peer->list, &peers_to_wmi);
			staged_count++;
		}
	}
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	list_for_each_entry_safe(peer, tmp, &peers_to_wmi, list) {
		ether_addr_copy(param.nrp_addr, peer->peer_info.mac_addr);
		if (ath12k_wmi_vdev_set_neighbor_rx_cmd(dp_pdev->ar, &param)) {
			ath12k_err(dp_pdev->dp->ab,
				   "wmi add fail vdev %d peer addr %pM\n",
				   param.vdev_id, param.nrp_addr);
			list_del(&peer->list);
			kfree(peer);
			staged_count--;
		}
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (likely(rx_ext_mon)) {
		rx_ext_mon->peer_count += staged_count;
		list_splice_tail(&peers_to_wmi, &rx_ext_mon->peer_list);
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
	} else {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		param.action = WMI_FILTER_NRP_ACTION_REMOVE;
		list_for_each_entry_safe(peer, tmp, &peers_to_wmi, list) {
			ether_addr_copy(param.nrp_addr, peer->peer_info.mac_addr);
			if (ath12k_wmi_vdev_set_neighbor_rx_cmd(dp_pdev->ar, &param))
				ath12k_warn(dp_pdev->dp->ab,
					    "wmi undo-add failed for peer %pM vdev %d\n",
					    peer->peer_info.mac_addr, param.vdev_id);
			list_del(&peer->list);
			kfree(peer);
		}
		ret = -EINVAL;
	}

	return ret;
}

static int
ath12k_dp_ext_mon_remove_rx_peers(struct ath12k_pdev_dp *dp_pdev,
				   const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;
	struct ath12k_set_neighbor_rx_params param = {0};
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	const struct ath12k_ext_mon_peer_info *peer_info;
	LIST_HEAD(peers_to_wmi);
	bool found;
	int i;

	param.action = WMI_FILTER_NRP_ACTION_REMOVE;
	param.vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	if (param.vdev_id == -1) {
		ath12k_warn(dp_pdev->dp, "no active monitor vdev found\n");
		return -ENODEV;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (unlikely(!rx_ext_mon)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "rx_ext_mon_config is null\n");
		return -EINVAL;
	}

	for (i = 0; i < peer_config->count; i++) {
		peer_info = &peer_config->peer_info[i];
		found = false;

		if (is_zero_ether_addr(peer_info->mac_addr)) {
			ath12k_warn(dp_pdev->dp,
				    "removing all peers not supported in RX direction\n");
			continue;
		}

		list_for_each_entry_safe(peer, tmp, &rx_ext_mon->peer_list, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					peer_info->mac_addr) &&
					peer->peer_info.ra_addr == peer_info->ra_addr) {
				list_del(&peer->list);
				rx_ext_mon->peer_count--;
				if (peer->peer_info.ra_addr) {
					rx_ext_mon->ra_peer_count--;
					kfree(peer);
				} else {
					list_add_tail(&peer->list, &peers_to_wmi);
				}
				found = true;
				break;
			}
		}

		if (!found)
			ath12k_warn(dp_pdev->dp, "peer %pM (%s) not found for remove\n",
				    peer_info->mac_addr,
				    peer_info->ra_addr ? "RA" : "TA");
	}
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	list_for_each_entry_safe(peer, tmp, &peers_to_wmi, list) {
		ether_addr_copy(param.nrp_addr, peer->peer_info.mac_addr);
		if (ath12k_wmi_vdev_set_neighbor_rx_cmd(dp_pdev->ar, &param)) {
			ath12k_err(dp_pdev->dp->ab,
				   "wmi remove fail vdev %d nrp %pM\n",
				   param.vdev_id, param.nrp_addr);
		}
		list_del(&peer->list);
		kfree(peer);
	}

	return 0;
}

static int
ath12k_dp_ext_mon_set_rx_peer(struct ath12k_pdev_dp *dp_pdev,
			       const struct ath12k_ext_mon_peer_config *peer_config)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	switch (peer_config->action) {
	case QCA_VENDOR_EXT_MON_PEER_ACTION_ADD:
		return ath12k_dp_ext_mon_add_rx_peers(dp_pdev, peer_config);
	case QCA_VENDOR_EXT_MON_PEER_ACTION_REMOVE:
		return ath12k_dp_ext_mon_remove_rx_peers(dp_pdev, peer_config);
	default:
		ath12k_warn(dp_pdev->dp, "invalid peer action %u\n",
			    peer_config->action);
		return -EINVAL;
	}
}

static
int ath12k_dp_ext_mon_set_peer(struct ath12k_pdev_dp *dp_pdev,
			       const struct ath12k_ext_mon_config *req)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	switch (req->direction) {
	case QCA_VENDOR_EXT_MON_DIRECTION_RX:
		ret = ath12k_dp_ext_mon_set_rx_peer(dp_pdev, &req->peer);
		break;
	case QCA_VENDOR_EXT_MON_DIRECTION_TX:
		ret = ath12k_dp_ext_mon_handle_tx_peer(dp_pdev, &req->peer);
		break;
	default:
		ath12k_warn(dp_pdev->dp, "invalid direction\n");
		ret = -EINVAL;
	}

	return ret;
}

static
int ath12k_dp_ext_mon_set_filter(struct ath12k_pdev_dp *dp_pdev,
				 const struct ath12k_ext_mon_config *req)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	switch (req->direction) {
	case QCA_VENDOR_EXT_MON_DIRECTION_RX:
		ret = ath12k_dp_ext_mon_set_rx_filter(dp_pdev, &req->filter);
		break;
	case QCA_VENDOR_EXT_MON_DIRECTION_TX:
		ret = ath12k_dp_ext_mon_set_tx_filter(dp_pdev, &req->filter);
		break;
	default:
		ath12k_warn(dp_pdev->dp, "invalid direction\n");
		ret = -EINVAL;
	}
	return ret;
}

static int
ath12k_dp_ext_mon_get_rx_filter(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (unlikely(!rx_ext_mon)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "rx_ext_mon_config is null\n");
		return -EINVAL;
	}

	resp->filter.disable = !rx_ext_mon->enable;
	if (!rx_ext_mon->enable) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		return 0;
	}

	resp->filter.level = rx_ext_mon->level;
	resp->filter.all_peer = rx_ext_mon->fp;
	resp->filter.all_neighbor = rx_ext_mon->mo;
	resp->filter.target_peer = rx_ext_mon->fpmo;
	resp->filter.target_neighbor = rx_ext_mon->md;
	resp->filter.meta_data = rx_ext_mon->metadata;
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	return 0;
}

static int
ath12k_dp_ext_mon_get_filter(struct ath12k_pdev_dp *dp_pdev,
			     const struct ath12k_ext_mon_config *req,
			     struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	switch (req->direction) {
	case QCA_VENDOR_EXT_MON_DIRECTION_RX:
		ret = ath12k_dp_ext_mon_get_rx_filter(dp_pdev, resp);
		break;
	case QCA_VENDOR_EXT_MON_DIRECTION_TX:
		ret = ath12k_dp_ext_mon_get_tx_filter(dp_pdev, resp);
		break;
	default:
		ath12k_warn(dp_pdev->dp, "invalid direction\n");
		ret = -EINVAL;
	}

	return ret;
}

static int
ath12k_dp_ext_mon_get_rx_peer(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_ext_mon;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_ext_mon = dp_mon_pdev->rx_ext_mon_config;
	if (unlikely(!rx_ext_mon)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "rx_ext_mon_config is null\n");
		return -EINVAL;
	}

	ath12k_dp_get_ext_mon_peers(dp_pdev,
				    &rx_ext_mon->peer_list, resp,
				    &dp_mon_pdev->rx_ext_mon_lock);
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	return 0;
}

static int
ath12k_dp_ext_mon_get_peer(struct ath12k_pdev_dp *dp_pdev,
			   const struct ath12k_ext_mon_config *req,
			   struct ath12k_ext_mon_config *resp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	int ret = 0;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	switch (req->direction) {
	case QCA_VENDOR_EXT_MON_DIRECTION_RX:
		ret = ath12k_dp_ext_mon_get_rx_peer(dp_pdev, resp);
		break;
	case QCA_VENDOR_EXT_MON_DIRECTION_TX:
		ret = ath12k_dp_ext_mon_get_tx_peer(dp_pdev, resp);
		break;
	default:
		ath12k_warn(dp_pdev->dp, "invalid direction\n");
		ret = -EINVAL;
	}

	return ret;
}

void ath12k_dp_ext_mon_process_request(struct ath12k_pdev_dp *dp_pdev,
				       const struct ath12k_ext_mon_config *req,
				       struct ath12k_ext_mon_config *resp)
{
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret = 0;
	u8 smart_mon_profile;

	smart_mon_profile = dp_pdev->dp_mon_pdev->smart_mon_filter &
				DP_SMART_MON_PROFILE_MASK;

	if (dp_pdev->dp_mon_pdev->smart_mon_filter & DP_SMART_MON_VALID ||
	    dp_pdev->dp_mon_pdev->smart_mon_state != ATH12K_DP_SMART_MON_DISABLED) {
		ath12k_warn(dp_pdev->dp, "Failed, smart mon enabled\n");
		resp->status_code = ATH12K_EXT_MON_FILTER_SETUP_FAIL;
		return;
	}

	if ((smart_mon_profile == DP_SMART_MON_PROFILE_512M ||
	     smart_mon_profile == DP_SMART_MON_PROFILE_256M) &&
	    req->cmd_type == QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER &&
	    !req->filter.disable &&
	    (ath12k_dp_ext_mon_is_mode_enabled(&req->filter.all_peer) ||
	     ath12k_dp_ext_mon_is_mode_enabled(&req->filter.all_neighbor) ||
	     ath12k_dp_ext_mon_is_mode_enabled(&req->filter.target_peer))) {
		ath12k_warn(dp_pdev->dp,
			    "Only target neighbor filter allowed on low mem profile\n");
		resp->status_code = ATH12K_EXT_MON_FILTER_SETUP_FAIL;
		return;
	}

	if (dp_pdev->dp_mon_pdev->nrp_enabled) {
		ath12k_warn(dp_pdev->dp, "nrp enabled\n");
		resp->status_code = ATH12K_EXT_MON_FILTER_SETUP_FAIL;
		return;
	}

	if (req->cmd_type == ATH12K_EXT_MON_CMD_TYPE_SET_FILTER ||
	    req->cmd_type == ATH12K_EXT_MON_CMD_TYPE_SET_PEER) {
		mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
		if (mon_ops && mon_ops->ext_mon_validate_request) {
			ret = mon_ops->ext_mon_validate_request(dp_pdev, req);
			if (ret) {
				ath12k_warn(dp_pdev->dp, "extmon validation failed: %d\n",
					    ret);
				resp->status_code = ATH12K_EXT_MON_VALIDATION_FAIL;
				return;
			}
		}
	}

	switch (req->cmd_type) {
	case QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER:
		ret = ath12k_dp_ext_mon_set_filter(dp_pdev, req);
		if (ret) {
			ath12k_warn(dp_pdev->dp, "set_filter failed: %d\n", ret);
			resp->status_code = ATH12K_EXT_MON_FILTER_SETUP_FAIL;
		}
		break;
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER:
		ret = ath12k_dp_ext_mon_get_filter(dp_pdev, req, resp);
		if (ret) {
			ath12k_warn(dp_pdev->dp, "get_filter failed: %d\n", ret);
			resp->status_code = ATH12K_EXT_MON_FILTER_SETUP_FAIL;
		}
		break;
	case QCA_VENDOR_EXT_MON_CMD_TYPE_SET_PEER:
		ret = ath12k_dp_ext_mon_set_peer(dp_pdev, req);
		if (ret) {
			ath12k_warn(dp_pdev->dp, "set_peer failed: %d\n", ret);
			resp->status_code = ATH12K_EXT_MON_PEER_SETUP_FAIL;
		}
		break;
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER:
		ret = ath12k_dp_ext_mon_get_peer(dp_pdev, req, resp);
		if (ret) {
			ath12k_warn(dp_pdev->dp, "get_peer failed: %d\n", ret);
			resp->status_code = ATH12K_EXT_MON_PEER_SETUP_FAIL;
		}
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_process_request);

int ath12k_dp_ext_mon_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_rx_ext_mon *rx_config = NULL;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return -EINVAL;
	}

	rx_config = kzalloc(sizeof(*rx_config), GFP_KERNEL);
	if (!rx_config)
		return -ENOMEM;

	rx_config->fp.data_mpdu_tlv.mcast = 0xF;
	rx_config->fp.data_mpdu_tlv.ucast = 0xF;
	rx_config->fp.data_mpdu_tlv.null_frm = 0xF;
	rx_config->fpmo.data_mpdu_tlv.mcast = 0xF;
	rx_config->fpmo.data_mpdu_tlv.ucast = 0xF;
	rx_config->fpmo.data_mpdu_tlv.null_frm = 0xF;

	INIT_LIST_HEAD(&rx_config->peer_list);
	spin_lock_init(&dp_mon_pdev->rx_ext_mon_lock);

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	dp_mon_pdev->rx_ext_mon_config = rx_config;
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_alloc);

static void
ath12k_dp_ext_mon_drain_peer_list(struct ath12k_pdev_dp *dp_pdev,
				  struct list_head *peer_list,
				  int vdev_id)
{
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	struct ath12k_set_neighbor_rx_params param = {0};

	if (vdev_id == -1) {
		ath12k_warn(dp_pdev->dp->ab,
			    "no active monitor vdev; skipping WMI REMOVE for peers\n");
		list_for_each_entry_safe(peer, tmp, peer_list, list) {
			list_del(&peer->list);
			kfree(peer);
		}
		return;
	}

	param.vdev_id = vdev_id;
	param.action = WMI_FILTER_NRP_ACTION_REMOVE;

	list_for_each_entry_safe(peer, tmp, peer_list, list) {
		if (!peer->peer_info.ra_addr) {
			ether_addr_copy(param.nrp_addr, peer->peer_info.mac_addr);
			if (ath12k_wmi_vdev_set_neighbor_rx_cmd(dp_pdev->ar, &param))
				ath12k_warn(dp_pdev->dp->ab,
					    "wmi remove failed for peer %pM vdev %d\n",
					    peer->peer_info.mac_addr, vdev_id);
		}
		list_del(&peer->list);
		kfree(peer);
	}
}

void ath12k_dp_ext_mon_free(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_config = NULL;
	LIST_HEAD(peers_to_drain);
	int vdev_id;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return;
	}

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_config = dp_mon_pdev->rx_ext_mon_config;
	if (!rx_config) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		return;
	}

	dp_mon_pdev->rx_ext_mon_config = NULL;
	list_splice_init(&rx_config->peer_list, &peers_to_drain);
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	ath12k_dp_ext_mon_drain_peer_list(dp_pdev, &peers_to_drain, vdev_id);

	kfree(rx_config);
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_free);

void ath12k_dp_ext_mon_reset(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *rx_config;
	LIST_HEAD(peers_to_drain);
	int vdev_id;

	if (unlikely(!dp_mon_pdev)) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null\n");
		return;
	}

	ath12k_dp_ext_mon_rx_config_filter(dp_pdev, false);
	ath12k_dp_ext_mon_enable_mac_ext_mon(dp_pdev, false);
	ath12k_dp_ext_mon_config_offchan_capture(dp_pdev, false);

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	rx_config = dp_mon_pdev->rx_ext_mon_config;
	if (!rx_config) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		return;
	}

	list_splice_init(&rx_config->peer_list, &peers_to_drain);
	memset(rx_config, 0, sizeof(*rx_config));
	INIT_LIST_HEAD(&rx_config->peer_list);
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	vdev_id = ath12k_dp_ext_mon_find_mon_vdev_id(dp_pdev);
	ath12k_dp_ext_mon_drain_peer_list(dp_pdev, &peers_to_drain, vdev_id);
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_reset);

void
ath12k_dp_rx_pktlog_process(struct ath12k_pdev_dp *dp_pdev,
			    struct ath12k_dp_mon_status_desc *status_desc)
{
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	u16 log_type = 0;

	if (!ar->debug.is_pkt_logging ||
	    !dp_mon_pdev ||
	    !status_desc->mon_buf ||
	    !status_desc->buf_len) {
		return;
	}

	if (dp_mon_pdev->rx_pktlog_mode == ATH12K_PKTLOG_MODE_LITE)
		log_type = ATH12K_PKTLOG_TYPE_LITE_RX;
	else if ((dp_mon_pdev->rx_pktlog_mode == ATH12K_PKTLOG_MODE_FULL) &&
		 (ar->debug.pktlog_filter & ATH12K_PKTLOG_RX))
		log_type = ATH12K_PKTLOG_TYPE_RX_STATBUF;

	if (!log_type) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DATA,
			   "pktlog: skipping processing with no log type\n");
		return;
	}

	trace_ath12k_htt_rxdesc(ar, status_desc->mon_buf,
				log_type, status_desc->buf_len);
	ath12k_dp_txrx_stats_buf_pktlog_process(ar, status_desc->mon_buf,
						log_type, status_desc->buf_len);
}
EXPORT_SYMBOL(ath12k_dp_rx_pktlog_process);

/**
 * ath12k_dp_rx_populate_cbf_hdr() - Wrap CBF frame with HTT headers
 * @dp_pdev: ath12k dp pdev handle
 * @skb: SKB containing CBF frame
 * @ppdu_id: PPDU ID for correlation
 *
 * Wraps a CBF frame with HTT headers for pktlog:
 * 1. HTT PPDU stats indication header
 * 2. RX management/control payload TLV
 * 3. Original CBF frame payload
 *
 * The wrapped frame is then written to the pktlog circular buffer.
 *
 */
static inline void
ath12k_dp_rx_populate_cbf_hdr(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *skb,
			      struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv cbf_tlv;
	struct htt_t2h_ppdu_stats_ind_hdr htt_hdr;
	u32 frame_len, len_align;

	frame_len = skb->len;
	len_align = ALIGN(frame_len +
			  sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv),
			  PKTLOG_ALIGN);

	htt_hdr.info = FIELD_PREP(HTT_T2H_PPDU_STATS_INFO_MSG_TYPE,
				  HTT_T2H_MSG_TYPE_PPDU_STATS_IND) |
		FIELD_PREP(HTT_T2H_PPDU_STATS_INFO_MAC_ID, dp_pdev->mac_id) |
		FIELD_PREP(HTT_T2H_PPDU_STATS_INFO_PDEV_ID,
			   dp_pdev->ar->pdev->pdev_id) |
		FIELD_PREP(HTT_T2H_PPDU_STATS_INFO_PAYLOAD_SIZE, len_align);

	htt_hdr.ppdu_id = cpu_to_le32(ppdu_info->ppdu_id);
	htt_hdr.timestamp_us = cpu_to_le32(ppdu_info->tsft);
	htt_hdr.rsvd = 0;

	len_align = ALIGN(frame_len - sizeof(cbf_tlv.header) +
			  sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv),
			  PKTLOG_ALIGN);

	cbf_tlv.header = FIELD_PREP(HTT_TLV_TAG,
				    HTT_PPDU_STATS_RX_MGMTCTRL_PAYLOAD_TLV) |
		FIELD_PREP(HTT_TLV_LEN, len_align);
	cbf_tlv.frame_length = cpu_to_le16(frame_len);
	cbf_tlv.rsvd1 = 0;
	cbf_tlv.rsvd2 = 0;
	cbf_tlv.rsvd3 = 0;

	ath12k_cbf_pktlog_process(dp_pdev->ar, skb->data,
				  skb->len, &htt_hdr, &cbf_tlv);
}

void
ath12k_dp_mon_rx_process_dest_pktlog(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff *mpdu,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	void *data;
	struct ieee80211_hdr *wh;
	u8 num_frags, type, subtype;

	if (!dp_pdev || !mpdu || !ppdu_info)
		return;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	if (!dp_mon_pdev || !dp_mon_pdev->rx_pktlog_cbf)
		return;

	num_frags = ath12k_dp_mon_get_num_frags_in_fraglist(mpdu);
	if (num_frags)
		data = ath12k_dp_mon_skb_get_frag_addr(mpdu, 0);
	else
		data = mpdu->data;

	if (unlikely(!data))
		return;

	wh = (struct ieee80211_hdr *)data;
	type = wh->frame_control & IEEE80211_FCTL_FTYPE;
	subtype = wh->frame_control & IEEE80211_FCTL_STYPE;

	if (type != IEEE80211_FTYPE_MGMT ||
	    subtype != IEEE80211_STYPE_ACTION_NO_ACK)
		return;

	ath12k_dp_rx_populate_cbf_hdr(dp_pdev, mpdu, ppdu_info);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_dest_pktlog);

int ath12k_dp_mon_rx_wq_init_common(struct ath12k_pdev_dp *dp_pdev,
				    void (*work_handler)(struct work_struct *))
{
	struct ath12k_pdev_mon_dp *mon_pdev = dp_pdev->dp_mon_pdev;

	mon_pdev->rxmon_wq = alloc_workqueue("rxmon_%s-%s%d", WQ_UNBOUND | WQ_SYSFS, 0,
					     ath12k_bus_str(dp_pdev->dp->ab->hif.bus),
					     dev_name(dp_pdev->dp->ab->dev),
					     dp_pdev->mac_id);
	if (unlikely(!mon_pdev->rxmon_wq)) {
		ath12k_warn(dp_pdev->dp,
			    "failed to allocate rxmon workqueue for mac_id %d\n",
			    dp_pdev->mac_id);
		return -ENOMEM;
	}

	INIT_WORK(&mon_pdev->rxmon_work, work_handler);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_wq_init_common);

void ath12k_dp_ext_mon_update_snr(struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath12k_dp_rx_ext_mon *config)
{
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	u8 avg_snr;
	u8 peer_avg_snr;

	if (!list_empty(&config->peer_list)) {
		list_for_each_entry_safe(peer, tmp,
					 &config->peer_list, list) {
			if (ether_addr_equal(peer->peer_info.mac_addr,
					     ppdu_info->nrp_info.mac_addr2)) {
				peer_avg_snr = peer->peer_info.ta_info.snr_info.avg_snr;
				avg_snr =
					ath12k_dp_get_avg_snr(ppdu_info->rssi_comb,
								    peer_avg_snr);
				peer->peer_info.ta_info.snr_info.avg_snr = avg_snr;
				peer->peer_info.ta_info.snr_info.snr =
							ppdu_info->rssi_comb;
				peer->peer_info.ta_info.snr_info.timestamp =
						ktime_to_ms(ktime_get_real());
				break;
			}
		}
	}
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_update_snr);

int
ath12k_dp_ext_mon_subtype_check(struct ath12k_ext_mon_pkt_config *config,
				u8 type, u8 sub_type, bool is_mcast)
{
	u32 filter = config->filter[type];

	switch (type) {
	case ATH12K_EXT_MON_FRAME_MGMT:
	case ATH12K_EXT_MON_FRAME_CTRL:
		if (filter && ((filter >> sub_type) & 0x1))
			return 0;
		break;

	case ATH12K_EXT_MON_FRAME_DATA:
		if ((is_mcast && (filter & FILTER_DATA_MCAST)) ||
		    (!is_mcast && (filter & FILTER_DATA_UCAST)))
			return 0;
		break;
	}

	return -EINVAL; /* Failure - filter out the frame */
}
EXPORT_SYMBOL(ath12k_dp_ext_mon_subtype_check);
