/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ipa_stats.h"

static void ath12k_dp_ipa_stats_update_ucast(struct ath12k_dp_ipa_stats_vif_stats *stats)
{
	stats->ucast.bytes = stats->total.bytes - stats->bcast.bytes - stats->mcast.bytes;
	stats->ucast.packets = stats->total.packets - stats->bcast.packets -
			       stats->mcast.packets;
}

void ath12k_dp_ipa_tx_rx_mcbc_stats(struct ath12k_dp_ipa_stats_vif_stats *stats,
				    bool is_mcast,
				    bool is_bcast,
				    u64 skb_len)
{
	if (is_mcast) {
		stats->mcast.bytes += skb_len;
		stats->mcast.packets++;
	}

	if (is_bcast) {
		stats->bcast.bytes += skb_len;
		stats->bcast.packets++;
	}

	ath12k_dp_ipa_stats_update_ucast(stats);
}

void ath12k_dp_ipa_tx_mcbc_stats(struct sk_buff *skb,
				 struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_ipa_stats_vif_hw_stats *link_vif_stats;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_hdr *hdr = NULL;

	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(skb_cb->vif);
	bool is_mcast, is_bcast;
	u64 skb_len = skb->len;
	struct ethhdr *eth = NULL;
	u8 *dst_addr;

	if (!dp_pdev) {
		ath12k_err(NULL, "Got dp_pdev NULL");
		return;
	}

	if (!dp_pdev->ar->fw_stats.en_vdev_stats_ol) {
		return;
	}

	if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		dst_addr = eth->h_dest;
	} else {
		hdr = (void *)skb->data;
		dst_addr = hdr->addr1;
	}

	is_mcast = is_multicast_ether_addr(dst_addr);
	is_bcast = false;

	if (is_mcast) {
		rcu_read_lock();
		arvif = rcu_dereference(ahvif->link[skb_cb->link_id]);
		link_vif_stats = arvif->extn.stats;
		is_bcast = is_broadcast_ether_addr(dst_addr);
		if (is_bcast)
			is_mcast = false;

		ath12k_dp_ipa_tx_rx_mcbc_stats(&link_vif_stats->tx.stats,
					       is_mcast, is_bcast,
					       skb_len);
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL(ath12k_dp_ipa_tx_mcbc_stats);

void ath12k_dp_ipa_rx_error_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_skb_rxcb *rxcb, u8 link_id)
{
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_vif *ahvif;
	struct ath12k *ar;
	struct ath12k_wbm_rx_stats *err;

	if (!dp_pdev)
		return;

	ar = dp_pdev->ar;
	if (!ar && !ar->fw_stats.en_vdev_stats_ol)
		return;

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							     rxcb->peer_id);
	if (!link_peer)
		return;
	ahvif = ath12k_vif_to_ahvif(link_peer->vif);

	if (!ahvif)
		return;
	struct ath12k_link_vif *arvif = rcu_dereference(ahvif->link[link_id]);

	if (!arvif)
		return;
	err = &arvif->extn.stats->rx.err;
	switch (rxcb->err_rel_src) {
	case HAL_WBM_REL_SRC_MODULE_REO:
		err->reo_error[rxcb->err_code]++;
		break;
	case HAL_WBM_REL_SRC_MODULE_RXDMA:
		err->rxdma_error[rxcb->err_code]++;
		break;
	default:
		break;
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL(ath12k_dp_ipa_rx_error_stats);

static inline void
ath12k_dp_ipa_stats_ieee80211_rx_stats(struct net_device *dev,
					u64 packets, u64 bytes)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(netdev_tstats(dev));

	u64_stats_update_begin(&tstats->syncp);

	u64_stats_set(&tstats->rx_packets, packets);
	u64_stats_set(&tstats->rx_bytes, bytes);

	u64_stats_update_end(&tstats->syncp);
}

static inline void
ath12k_dp_ipa_stats_ieee80211_tx_stats(struct net_device *dev,
					u64 packets, u64 bytes)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(netdev_tstats(dev));

	u64_stats_update_begin(&tstats->syncp);

	u64_stats_set(&tstats->tx_packets, packets);
	u64_stats_set(&tstats->tx_bytes, bytes);

	u64_stats_update_end(&tstats->syncp);
}

void ath12k_dp_ipa_htt_vdev_txrx_stats_handler(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct htt_t2h_vdev_txrx_stats_ind *vdev_tlv;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_ipa_stats_vif_hw_stats *link_vif_stats;
	struct ath12k_dp_ipa_stats_vif_stats *tx_stats, *rx_stats;
	struct ieee80211_vif *vif = NULL;
	struct wireless_dev *wdev = NULL;
	struct net_device *net_dev = NULL;
	u32 *tx_err;
	u32 *data = (u32 *)skb->data;
	u32 vdev_id;
	u16 payload_bytes, tlv_tag, tlv_len;

	payload_bytes = u32_get_bits(*data, HTT_T2H_VDEV_STATS_PERIODIC_PAYLOAD_BYTES);
	if (payload_bytes > skb->len) {
		ath12k_err(ab, "Payload bytes incorrect in vdev offload stats");
		return;
	}

	/* Adjust msg_word to point to the first TLV in buffer */
	data += HTT_T2H_VDEV_TXRX_STATS_PERIODIC_IND_HDR_WORDS;

	rcu_read_lock();
	while (payload_bytes > 0) {
		tlv_tag = u32_get_bits(*data, HAL_STATS_TLV_HDR_TAG);
		tlv_len = u32_get_bits(*data, HAL_STATS_TLV_HDR_LEN);

		/* Adjust data to point to the data value after TLV header */
		data += HTT_T2H_VDEV_TXRX_STATS_TLV_HDR_WORDS;

		if (tlv_tag == HTT_STATS_VDEV_TXRX_STATS_HW_STATS_TAG) {
			vdev_tlv = (struct htt_t2h_vdev_txrx_stats_ind *)data;
			vdev_id = __le32_to_cpu(vdev_tlv->vdev_id);
			arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
			if (!arvif) {
				ath12k_err(ab,
					   "ol vdev stats error in getting arvif from vdev id:%d\n",
					   vdev_id);
				goto next;
			}

			link_vif_stats = arvif->extn.stats;
			if (!link_vif_stats) {
				ath12k_err(ab,
					   "ol vdev stats error in getting stats from arvif\n");
				goto next;
			}

			rx_stats = &link_vif_stats->rx.stats;
			tx_stats = &link_vif_stats->tx.stats;
			tx_err = link_vif_stats->tx.err;

			rx_stats->total.bytes =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->rx_msdu_byte_cnt_lo,
						       vdev_tlv->rx_msdu_byte_cnt_hi);
			rx_stats->total.packets =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->rx_msdu_cnt_lo,
						       vdev_tlv->rx_msdu_cnt_hi);
			tx_stats->total.bytes =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->tx_msdu_byte_cnt_lo,
						       vdev_tlv->tx_msdu_byte_cnt_hi);
			tx_stats->total.packets =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->tx_msdu_cnt_lo,
						       vdev_tlv->tx_msdu_cnt_hi);

			/* Retry Drop packet count */
			tx_err[ATH12K_DP_IPA_STATS_RETRY] =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->tx_retry_cnt_lo,
						       vdev_tlv->tx_retry_cnt_hi);
			/* Expired Packet count */
			tx_err[ATH12K_DP_IPA_STATS_EXPIRED] =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->tx_drop_cnt_lo,
						       vdev_tlv->tx_drop_cnt_hi);

			/* Congestion Control drop(Age out) Packet count */
			tx_err[ATH12K_DP_IPA_STATS_AGE_OUT] =
				HTT_VDEV_GET_STATS_U64(vdev_tlv->msdu_ttl_cnt_lo,
						       vdev_tlv->msdu_ttl_cnt_hi);

			ath12k_dp_ipa_stats_update_ucast(&link_vif_stats->tx.stats);
			ath12k_dp_ipa_stats_update_ucast(&link_vif_stats->rx.stats);

			vif = ath12k_ahvif_to_vif(arvif->ahvif);
			if (!vif) {
				ath12k_err(ab,
					   "ol vif stats error in getting vif from arvif\n");
				goto next;
			}

			wdev = ieee80211_vif_to_wdev(vif);
			net_dev = wdev ? wdev->netdev : NULL;
			if (!net_dev) {
				ath12k_err(ab,
					   "Net device is Null");
				goto next;
			}

			ath12k_dp_ipa_stats_ieee80211_rx_stats(net_dev,
							       rx_stats->total.packets,
							       rx_stats->total.bytes);
			ath12k_dp_ipa_stats_ieee80211_tx_stats(net_dev,
							       tx_stats->total.packets,
							       tx_stats->total.bytes);
		}

next:
		data += tlv_len >> BYTE_TO_DWORD_SHIFT;
		/* Add header size to tlv length */
		tlv_len += HTT_T2H_VDEV_TXRX_STATS_TLV_HDR_SIZE;
		payload_bytes -= tlv_len;
	}
	rcu_read_unlock();
}

static void ath12k_dp_ipa_aggr_stats(struct ath12k_dp_ipa_stats_vif_stats *src,
				     struct ath12k_dp_ipa_stats_vif_stats *dst)
{
	dst->total.packets += src->total.packets;
	dst->total.bytes += src->total.bytes;
	dst->mcast.bytes += src->mcast.bytes;
	dst->mcast.packets += src->mcast.packets;
	dst->ucast.bytes += src->ucast.bytes;
	dst->ucast.packets += src->ucast.packets;
	dst->bcast.bytes += src->bcast.bytes;
	dst->bcast.packets += src->bcast.packets;
}

static inline void ath12k_dp_ipa_aggr_tx_err_stats(u32 *src, u32 *dst)
{
	int tx_errors;

	for (tx_errors = 0; tx_errors < ATH12K_DP_IPA_STATS_TX_ERR_MAX; tx_errors++)
		dst[tx_errors] += src[tx_errors];
}

static inline void
ath12k_dp_ipa_aggr_rx_err_stats(struct ath12k_wbm_rx_stats *src,
				struct ath12k_wbm_rx_stats *dst)
{
	int reo_ring, rxdma_ring;

	for (rxdma_ring = 0; rxdma_ring < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	     rxdma_ring++)
		dst->rxdma_error[rxdma_ring] += src->rxdma_error[rxdma_ring];

	for (reo_ring = 0; reo_ring < HAL_REO_DEST_RING_ERROR_CODE_MAX; reo_ring++)
		dst->reo_error[reo_ring] += src->reo_error[reo_ring];
}

static inline void
ath12k_dp_ipa_copy_rx(struct ath12k_dp_ipa_stats_rx_vif_stats *src,
		      struct ath12k_dp_ipa_stats_rx_vif_stats *dst)
{
	ath12k_dp_ipa_aggr_stats(&src->stats, &dst->stats);
	ath12k_dp_ipa_aggr_rx_err_stats(&src->err, &dst->err);
}

static inline void
ath12k_dp_ipa_copy_tx(struct ath12k_dp_ipa_stats_tx_vif_stats *src,
		      struct ath12k_dp_ipa_stats_tx_vif_stats *dst)
{
	ath12k_dp_ipa_aggr_stats(&src->stats, &dst->stats);
	ath12k_dp_ipa_aggr_tx_err_stats(src->err, dst->err);
}

static inline void
ath12k_dp_ipa_get_link_stats(struct ath12k_dp_ipa_stats_vif_hw_stats *stats_src,
			     struct ath12k_dp_ipa_stats_vif_hw_stats *stats_dst)
{
	ath12k_dp_ipa_copy_rx(&stats_src->rx, &stats_dst->rx);
	ath12k_dp_ipa_copy_tx(&stats_src->tx, &stats_dst->tx);
}

static void
ath12k_dp_ipa_get_vif_stats(struct ath12k_vif *ahvif,
			    struct ath12k_dp_ipa_stats_vif_hw_stats
				*telemetry_vif,
			    u8 link_id)
{
	unsigned long links_map = ahvif->links_map;
	struct ath12k_link_vif *arvif;
	struct ath12k_dp_ipa_stats_vif_hw_stats *lv_stats;

	if(link_id!=0xFF && links_map & BIT(link_id)) {
		rcu_read_lock();
		arvif = rcu_dereference(ahvif->link[link_id]);
		if (arvif)
			ath12k_dp_ipa_get_link_stats(lv_stats, telemetry_vif);
		rcu_read_unlock();
	} else {
		/*Get stats for requested vif*/
		for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
			rcu_read_lock();
			arvif = rcu_dereference(ahvif->link[link_id]);
			if (!arvif || !arvif->is_created) {
				rcu_read_unlock();
				continue;
			}
			lv_stats = arvif->extn.stats;
			ath12k_dp_ipa_get_link_stats(lv_stats, telemetry_vif);
			rcu_read_unlock();
		}
	}
}

static inline u32 ath12k_dp_ipa_get_rx_err(struct ath12k_wbm_rx_stats *err)
{
	u32 err_pkt = 0;
	int reo_ring, rxdma_ring;

	for (rxdma_ring = 0; rxdma_ring < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	     rxdma_ring++)
		err_pkt += err->rxdma_error[rxdma_ring];

	for (reo_ring = 0; reo_ring < HAL_REO_DEST_RING_ERROR_CODE_MAX; reo_ring++)
		err_pkt += err->reo_error[reo_ring];

	return err_pkt;
}

static int ath12k_dp_ipa_fill_vap_rx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_ipa_stats_rx_vif_stats
						 *telemetry_rx_vif,
						 int ring_num, bool is_extended)
{
	struct nlattr *attr;
	u32 stat_u32 = 0;
	u64 stat_u64 = 0;

	/* Basic Stats*/
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif stats - rx from reo");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif stats - rx to stack");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_rx_vif->stats.total.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_rx_vif->stats.total.bytes : 0;
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx  attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif rx stats - rx to stack fast");
		return -EINVAL;
	}

	stat_u32 = 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx  attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = 0;
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	stat_u32 = !ring_num ? ath12k_dp_ipa_get_rx_err(&telemetry_rx_vif->err) : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx  attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED,
			   ring_num + 1);
		return -EINVAL;
	}

	/* Advance Stats */
	if (!is_extended)
		return 0;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif rx stats - rx mcast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_rx_vif->stats.mcast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_rx_vif->stats.mcast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx stats  attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif rx stats - rx ucast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_rx_vif->stats.ucast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_rx_vif->stats.ucast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_BCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif rx stats - rx bcast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_rx_vif->stats.bcast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_rx_vif->stats.bcast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
			0)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
			0)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
			0)) {
		ath12k_err(NULL, "nla put failure: vif rx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
			   ring_num + 1);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_dp_ipa_fill_rxdma_err_attrs(struct ath12k_base *ab,
					      struct sk_buff *vendor_event)
{
	struct nlattr *attr;
	int rxdma;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device rxdma error");
		return -EINVAL;
	}

	for (rxdma = 0; rxdma < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX; rxdma++)
		if (nla_put_u32(vendor_event, rxdma + 1, 0)) {
			ath12k_err(ab,
				   "nla put failure: Device rxdma err attr %d",
				   rxdma + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_dp_ipa_fill_reo_err_attrs(struct ath12k_base *ab,
					    struct sk_buff *vendor_event)
{
	struct nlattr *attr;
	int reo_attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device reo error");
		return -EINVAL;
	}

	for (reo_attr = 0; reo_attr < HAL_REO_DEST_RING_ERROR_CODE_MAX;
	     reo_attr++)
		if (nla_put_u32(vendor_event, reo_attr + 1,
				0)) {
			ath12k_err(ab,
				   "nla put failure: Device reo err attr %d",
				   reo_attr + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_dp_ipa_fill_peer_rx_wbm_err_attrs(struct sk_buff *vendor_event)
{
	if (ath12k_dp_ipa_fill_rxdma_err_attrs(NULL, vendor_event)) {
		ath12k_err(NULL, "Error filling vif rxdma error");
		return -EINVAL;
	}

	if (ath12k_dp_ipa_fill_reo_err_attrs(NULL, vendor_event)) {
		ath12k_err(NULL, "Error filling vif reo error");
		return -EINVAL;
	}

	return 0;
}

static int
ath12k_dp_ipa_fill_vap_rx_stats(struct sk_buff *vendor_event,
				struct ath12k_dp_ipa_stats_vif_hw_stats
				*telemetry_vif,
				bool is_extended)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	/*Rx Per Pkt Stats*/
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Vdev stats Rx");
		return -EINVAL;
	}

	for (ring_num = 0;
	     ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX;
	     ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: vif rx stats Rx ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_dp_ipa_fill_vap_rx_stats_attrs(vendor_event,
							  &telemetry_vif->rx,
							  ring_num,
							  is_extended)) {
			ath12k_err(NULL,
				   "nla put failure: vif rx stats for ring %d",
				   ring_num + 1);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	/*Rx WBM Err Stats*/
	if (ath12k_dp_ipa_fill_peer_rx_wbm_err_attrs(vendor_event)) {
		ath12k_err(NULL, "Error filling vif rx wbm err stats");
		return -EINVAL;
	}

	return 0;
}

static inline u32 ath12k_dp_ipa_get_tx_err(u32 *err)
{
	u32 err_pkt = 0;
	int tx_errors = 0;

	for (tx_errors = 0; tx_errors < ATH12K_DP_IPA_STATS_TX_ERR_MAX; tx_errors++)
		err_pkt += err[tx_errors];

	return err_pkt;
}

static int ath12k_dp_ipa_fill_vif_tx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_ipa_stats_tx_vif_stats
						 *telemetry_tx_vif,
						 int ring_num,
						 bool is_extended)
{
	struct nlattr *attr;
	int reason;
	u32 stat_u32 = 0;
	u64 stat_u64 = 0;

	/* Basic Stats*/
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif tx stats - tx comp");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      0,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif tx stats- tx success");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_tx_vif->stats.total.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_tx_vif->stats.total.bytes : 0;
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	stat_u32 = !ring_num ? ath12k_dp_ipa_get_tx_err(telemetry_tx_vif->err) : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED);
		return -EINVAL;
	}

	/* Advance Stats */
	if (!is_extended)
		return 0;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif stats - wbm rel rsn");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX;
	     reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: vif tx stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
				   reason + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif tx stats- tqm rel rsn");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX;
	     reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: vif tx stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON,
				   reason + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT, ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA, ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
			0)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
			   ring_num + 1);
		return -EINVAL;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif tx stats - mcast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_tx_vif->stats.mcast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif tx stats  attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	stat_u64 = !ring_num ? telemetry_tx_vif->stats.mcast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif stats - ucast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_tx_vif->stats.ucast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif stats  stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	stat_u64 = !ring_num ? telemetry_tx_vif->stats.ucast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif stats - bcast");
		return -EINVAL;
	}

	stat_u32 = !ring_num ? telemetry_tx_vif->stats.bcast.packets : 0;
	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			stat_u32)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	stat_u64 = !ring_num ? telemetry_tx_vif->stats.bcast.bytes : 0;
	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      stat_u64,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: vif tx stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_vif_tx_stats(struct sk_buff *vendor_event,
			 struct ath12k_dp_ipa_stats_tx_vif_stats *telemetry_tx_vif,
			 bool is_extended)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	/*Tx vif Stats*/
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vif stats");
		return -EINVAL;
	}

	for (ring_num = 0;
	     ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX;
	     ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: vif stats - ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_dp_ipa_fill_vif_tx_stats_attrs(vendor_event, telemetry_tx_vif,
							  ring_num, is_extended)) {
			ath12k_err(NULL, "Error filling vif tx stats for ring %d",
				   ring_num + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_dp_ipa_fill_tx_ingress_stats_attrs(struct sk_buff *vendor_event,
					  struct ath12k_dp_ipa_stats_vif_hw_stats
					  *telemetry_vif,
					  bool is_extended)
{
	struct nlattr *attr;
	int attr_index;

	/* Basic stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats from stack");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			0)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      0,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			0)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      0,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw fast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			0)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      0,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats drop type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < DP_TX_ENQ_ERR_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (!is_extended)
		return 0;

	/* Extended stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encap type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < HAL_TCL_ENCAP_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encrypt type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < HAL_ENCRYPT_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats desc type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < DP_TCL_DESC_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				0)) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
				   attr_index + 1);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats mcast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS,
			0)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,
			      0,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_dp_ipa_fill_tx_ingress_stats(struct sk_buff *vendor_event,
					       struct ath12k_dp_ipa_stats_vif_hw_stats
					       *telemetry_vif,
					       bool is_extended)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vif ingress stats");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: vif ingress stats - ring %d",
				   ring_num + 1);
			return -EINVAL;
		}

		if (ath12k_dp_ipa_fill_tx_ingress_stats_attrs(vendor_event,
						      telemetry_vif,
						      is_extended)) {
			ath12k_err(NULL,
				   "Error filling vap ingress stats for ring %d",
				   ring_num + 1);
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_dp_ipa_fill_vap_tx_stats(struct sk_buff *vendor_event,
					   struct ath12k_dp_ipa_stats_vif_hw_stats
					   *telemetry_vif,
					   bool is_extended)
{
	int ret;

	ret = ath12k_fill_vif_tx_stats(vendor_event,
				       &telemetry_vif->tx,
				       is_extended);
	if (ret) {
		ath12k_err(NULL, "Error filling vap tx stats");
		return ret;
	}

	/* Ingress tx stats */
	ret = ath12k_dp_ipa_fill_tx_ingress_stats(vendor_event,
						  telemetry_vif,
						  is_extended);

	return ret;
}

static int ath12k_dp_ipa_get_dp_ingress_attr_len(void)
{
	struct ath12k_dp_pkt_info pkt;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_TCL_ENCAP_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_ENCRYPT_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TCL_DESC_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TX_ENQ_ERR_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt));
		attr_size +=  nla_total_size_nested(payload_size);

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}
	/* Parent Ingress Stats Attr size */
	total_size += nla_total_size_nested(total_size);

	return total_size;
}

static int ath12k_dp_ipa_get_peer_rx_stats_size(void)
{
	struct ath12k_dp_pkt_info pkt;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_REO_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		/* Advance */
		attr_size += nla_total_size(sizeof(pkt.packets));

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets));

		/* Ring Attr Size */
		total_size += nla_total_size_nested(attr_size);
	}

	return total_size;
}

static int ath12k_dp_ipa_get_feat_rx_peer_attr_size(void)
{
	int payload_size;
	int total_size;
	int attr_size;

	/* Rx Stats */
	payload_size = ath12k_dp_ipa_get_peer_rx_stats_size();
	/* Rx Per Pkt Stats Attr */
	attr_size = nla_total_size_nested(payload_size);

	/* Rx WBM Err*/
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_DEST_RING_ERROR_CODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* Parent RX Stats Attr Size */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_dp_ipa_get_feat_tx_peer_attr_size(void)
{
	struct ath12k_dp_pkt_info pkt;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(pkt.bytes));

		/* Advance */
		payload_size = nla_total_size(sizeof(u32)) *
				(HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX);
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(u32)) *
				(HAL_WBM_TQM_REL_REASON_MAX);
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.packets));

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(pkt.packets)) +
				nla_total_size(sizeof(pkt.bytes));
		attr_size += nla_total_size_nested(payload_size);

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}

	/* Parent Tx Attr Size */
	total_size = nla_total_size_nested(total_size);

	/* Parent Tx Attr Size */
	total_size = nla_total_size_nested(total_size);
	return total_size;
}

static int ath12k_dp_ipa_get_dp_peer_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_rx)
		total_size += ath12k_dp_ipa_get_feat_rx_peer_attr_size();

	if (cmd->feat.feat_tx)
		total_size += ath12k_dp_ipa_get_feat_tx_peer_attr_size();

	return total_size;
}

int ath12k_dp_ipa_get_dp_vif_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_tx)
		total_size += ath12k_dp_ipa_get_dp_ingress_attr_len();

	/*Aggregated Sta Stats Size */
	total_size += ath12k_dp_ipa_get_dp_peer_attr_len(cmd);

	return total_size;
}

int ath12k_dp_ipa_get_common_nl_event_attr_size(void)
{
	int common_size;

	common_size = nla_total_size(sizeof(u32)) +
		      nla_total_size(sizeof(u8)) + /* Link Id */
		      nla_total_size(sizeof(u64)); /* Request Id */

	return common_size;
}

static bool ath12k_dp_ipa_get_extended(struct ath12k_vif *ahvif,
				       u8 link_id)
{
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	bool is_extended = false;
	unsigned long links_map = ahvif->links_map;

	if (links_map & BIT(link_id)) {
		rcu_read_lock();
		if (ahvif)
			arvif = rcu_dereference(ahvif->link[link_id]);
		if (arvif) {
			ar = arvif->ar;
			is_extended = ar->fw_stats.en_vdev_stats_ol;
		}
		rcu_read_unlock();
	} else {
		/*legacy vif stats handling*/
		if (hweight16(links_map) == 0) {
			if (ahvif)
				arvif = &ahvif->deflink;
			if (arvif) {
				ar = arvif->ar;
				is_extended = ar->fw_stats.en_vdev_stats_ol;
			}
		} else {
			for_each_set_bit(link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
				rcu_read_lock();
				if (ahvif)
					arvif = rcu_dereference(ahvif->link[link_id]);
				if (!arvif || !arvif->is_created) {
					rcu_read_unlock();
					continue;
				}
				if (arvif)
					ar = arvif->ar;
				if (ar)
					is_extended |= ar->fw_stats.en_vdev_stats_ol;
				rcu_read_unlock();
			}
		}
	}

	return is_extended;
}

int ath12k_dp_ipa_prepare_vif_vendor_event(struct sk_buff *vendor_event,
					   struct ath12k_vif *ahvif,
					   struct ath12k_telemetry_command *cmd)
{
	struct ath12k_dp_ipa_stats_vif_hw_stats *telemetry_vif;
	struct nlattr *attr;
	int ret = -EINVAL;
	bool is_extended;

	telemetry_vif = vmalloc(sizeof(*telemetry_vif));
	if (!telemetry_vif)
		return -ENOMEM;

	memset(telemetry_vif, 0, sizeof(*telemetry_vif));

	ath12k_dp_ipa_get_vif_stats(ahvif, telemetry_vif, cmd->link_id);
	is_extended = ath12k_dp_ipa_get_extended(ahvif, cmd->link_id);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_dp_ipa_fill_vap_rx_stats(vendor_event,
						    telemetry_vif, is_extended)) {
				ath12k_err(NULL, "Error filling vap rx stats");
				goto out;
			}

			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: vap rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_dp_ipa_fill_vap_tx_stats(vendor_event,
						    telemetry_vif, is_extended)) {
				ath12k_err(NULL, "Error filling vap tx stats");
				goto out;
			}

			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: vap tx feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(telemetry_vif);
	return ret;
}
