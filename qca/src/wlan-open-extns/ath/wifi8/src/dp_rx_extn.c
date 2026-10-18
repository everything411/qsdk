/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../core.h"
#include "../../dp_rx.h"
#include "../hal_desc.h"
#include "../hal.h"
#include "../dp_rx.h"
#include "../../fse.h"

void ath12k_wifi8_deliver_ethernet_frame(struct ath12k_pdev_dp *dp_pdev,
					 struct hal_rx_spd_data *rx_spd,
					 struct ath12k_dp_peer *peer,
					 struct ieee80211_rx_status *status,
					 struct napi_struct *napi,
					 struct link_peer_rx_tid_stats *stats,
					 struct rx_tlv_info_1 *prev_tlv_info)
{
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
	struct netdev_hw_offload_ops __rcu *offload_ops;
#endif
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ath12k_dp *dp = dp_pdev->dp;
	u8 ring_id = rx_spd->ring_id;
	u8 device_id = dp->device_id;
	struct hal_rx_desc *rx_tlv_hdr;
	u8 tid;

	rx_tlv_hdr = (struct hal_rx_desc *)rx_spd->vaddr;
	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;
	tid = rx_mpdu_info->tid;

	/* set checksum pass or fail only in parent skb */
	ath12k_wifi8_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	msdu->dev = peer->dev;

	if (unlikely(peer->mscs_session_exists)) {
		/* Update skb priority with the tid received
		 * in rx_mpdu_info.
		 * This has to be done per packet since
		 * a packet from the same flow can have
		 * different tid values
		 */
		msdu->priority = tid;
		/** Check if the flow has been timed out
		 * or if the flow is invalid
		 * If the flow is invalid, check for five-tuple info and
		 * then program the FST entry with MSCS tag
		 * If tid is 0, which is the default case,
		 * do not program the FSE entry and MSCS rule
		 */
		if (tid && !rx_mpdu_info->flow_idx_timeout &&
				    !(rx_mpdu_info->flow_info.flow_metadata &
				    ATH12K_RX_FSE_FLOW_MSCS_RULE_PROGRAMMED))
			ath12k_dp_rx_classify_mscs(dp->ab, peer, msdu, tid);
	}

	/* deliver 802.3 frame to netif_receive_skb only if the following
	 * conditions are met.
	 * stats are disabled (KPI path) and
	 * it is not an intrabss frame and peer is authorized and
	 * it is a 4-address mcast/bcast frame
	 * (note: fr_ds is not set & da_is_mcbc is set, STA -> AP)
	 * 3-address MCBC frame should be handled by MAC80211
	 * (ieee80211_rx_napi) for intrabss functionality.
	 * (AP -> STA) MCBC frames are handled in NULL Q handler.
	 */
	if (!rx_mpdu_info->flow_idx_timeout &&
	    !rx_mpdu_info->flow_idx_invalid && !peer->is_mmesh_peer &&
	    likely(peer->is_authorized)) {
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
		if (unlikely(peer->ppe_vp_num)) {
			u16 flow_metadata =
				rx_mpdu_info->flow_info.flow_metadata;

			msdu->mark =
				u32_encode_bits(ATH12K_FSE_MAGIC_NUM,
						ATH12K_FSE_MAGIC_NUM_MASK) |
				u32_encode_bits(flow_metadata,
						ATH12K_PPE_VP_NUM);

			if (!peer->dev) {
				dev_kfree_skb_any(msdu);
				return;
			}

			offload_ops = rcu_dereference(peer->dev->offload_ops);
			if ((flow_metadata & ATH12K_RX_FSE_FLOW_MATCH_USE_PPE)) {
				dp->device_stats.fast_rx[ring_id][device_id]++;
				if (offload_ops && offload_ops->recv &&
				    offload_ops->recv(peer->dev, msdu)) {
					return;
				}
			}
		}
#endif
		stats->sent_to_stack_ucast_fast++;
		stats->sent_to_stack_ucast_fast_bytes += msdu->len;

		msdu->protocol = eth_type_trans(msdu, peer->dev);
		dp->device_stats.fast_rx[ring_id][device_id]++;
		prefetch(skb_shinfo(msdu));
		netif_receive_skb(msdu);
	} else {
		struct ieee80211_sta *pubsta = NULL;
		struct ath12k_hal *hal = dp_pdev->dp->hal;
		bool ret;

		/* ieee80211_rx_status object is reset when ever there is a
		 * peer change or radio change, if so extract radio params from
		 * the current MSDUs TLV headers and set the status info
		 * accordingly.
		 * Note: Radio params (ie: band, nss, sgi etc...) remain same
		 */
		ath12k_wifi8_dp_extract_rx_spd_data(hal,
						    rx_spd, rx_tlv_hdr);

		tlv_info = &rx_spd->tlv_info;

		status->flag |= RX_FLAG_8023;
		if (unlikely(!ath12k_wifi8_compare_tlv_info(prev_tlv_info,
							    tlv_info))) {
			ret = ath12k_wifi8_dp_rx_h_ppdu(dp_pdev,
							status,
							tlv_info,
							HAL_WBM_REL_SRC_MODULE_REO);
			if (ret) {
				dev_kfree_skb_any(msdu);
				return;
			}

			status->flag |= RX_FLAG_8023 |
				RX_FLAG_DECRYPTED |
				RX_FLAG_MMIC_STRIPPED |
				RX_FLAG_IV_STRIPPED |
				RX_FLAG_PN_VALIDATED |
				RX_FLAG_SKIP_MONITOR |
				RX_FLAG_DUP_VALIDATED;
			ath12k_wifi8_copy_tlv_info(prev_tlv_info, tlv_info);
		}
		pubsta = peer->sta;
		if (pubsta && pubsta->valid_links) {
			status->link_valid = 1;
			status->link_id = peer->hw_links[rx_mpdu_info->src_link_id];
		}

		msdu->priority = rx_mpdu_info->tid;

		/* Convert 802.3 frame to 802.11 so mac80211 can handle it correctly.
		 * For 4addr frames, this is needed to support APVLAN interface creation.
		 * For TKIP peers, fast-rx is not set in mac80211, so RX_FLAG_8023 must
		 * be cleared and the frame converted to route through normal RX handlers.
		 */
		if ((!peer->use_4addr &&
		    (rx_spd->rx_msdu_info.fr_ds && rx_spd->rx_msdu_info.to_ds)) ||
		     peer->sec_type == HAL_ENCRYPT_TYPE_TKIP_MIC) {
			ath12k_wifi8_convert_eth_2_80211_frame(dp, rx_spd);
			status->flag &= ~RX_FLAG_8023;
		}

		rx_status = IEEE80211_SKB_RXCB(msdu);
		*rx_status = *status;

		if (rx_msdu_info->da_is_mcbc) {
			stats->sent_to_stack_mcast++;
			stats->sent_to_stack_mcast_bytes += msdu->len;
			dp->device_stats.non_fast_mcast_rx[ring_id][device_id]++;
		} else {
			stats->sent_to_stack_ucast++;
			stats->sent_to_stack_ucast_bytes += msdu->len;
			dp->device_stats.non_fast_unicast_rx[ring_id][device_id]++;
		}

		ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu,
				  napi);
	}
	return;
}
