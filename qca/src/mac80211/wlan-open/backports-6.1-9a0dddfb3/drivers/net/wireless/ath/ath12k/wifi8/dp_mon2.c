// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor dual ring
 * model
 */
#include <linux/if_vlan.h>
#include "../dp_mon.h"
#include "../debug.h"
#include "hal_qcn9625.h"
#include "hal_mon.h"
#include "../peer.h"
#include "../dp_mon_filter.h"
#include "dp_mon2.h"
#include "../trace.h"
#include "../ath12k_notif.h"

const struct ath12k_dp_arch_mon_ops ath12k_wifi8_dp_arch_mon_dual_ring_ops = {
	.rx_srng_setup = ath12k_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_dp_mon_rx_srng_cleanup,
	.rx_ring_init = ath12k_dp_mon_rx_ring_init,
	.rx_ring_deinit = ath12k_dp_mon_rx_ring_deinit,
	.rx_htt_srng_setup = ath12k_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = ath12k_dp_mon_pdev_rx_srng_setup,
	.mon_pdev_rx_srng_cleanup = ath12k_dp_mon_pdev_rx_srng_cleanup,
	.mon_pdev_rx_htt_srng_setup = ath12k_dp_mon_pdev_rx_htt_srng_setup,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.mon_pdev_rx_mpdu_list_init = ath12k_dp_mon_pdev_rx_mpdu_list_init,
	.rx_filter_alloc = ath12k_dp_mon_rx_filter_alloc,
	.rx_filter_free = ath12k_dp_mon_rx_filter_free,
	.rx_stats_enable = ath12k_dp_mon_rx_stats_enable,
	.rx_stats_disable = ath12k_dp_mon_rx_stats_disable,
	.rx_filter_update = ath12k_dp_mon_rx_update_ring_filter,
	.rx_monitor_mode_set = ath12k_dp_mon_rx_monitor_mode_set,
	.rx_monitor_mode_reset = ath12k_dp_mon_rx_monitor_mode_reset,
	.rx_nrp_set = ath12k_dp_mon_rx_nrp_set,
	.rx_nrp_reset = ath12k_dp_mon_rx_nrp_reset,
	.setup_ppdu_desc = ath12k_wifi8_dp_mon_rx_dual_ring_setup_ppdu_desc,
	.cleanup_ppdu_desc = ath12k_wifi8_dp_mon_rx_dual_ring_cleanup_ppdu_desc,
	.mon_rx_wq_init = ath12k_wifi8_dp_mon_rx_wq_init,
	.mon_rx_wq_deinit = ath12k_wifi8_dp_mon_rx_wq_deinit,
	.rx_cfr_rcc_config = ath12k_dp_mon_rx_cfr_rcc_config_filter,
	.rx_get_cfr_rcc_filter_valid = ath12k_dp_mon_get_rx_cfr_rcc_filter_valid,
	.mon_rx_wmask = ath12k_dp_mon_rx_wmask_subscribe,
	.rx_enable_packet_filters = ath12k_dp_mon_rx_enable_packet_filters,
	.pktlog_config = ath12k_dp_mon_pktlog_config_filter,
	.htt_rx_filter_rxmon_cfg = ath12k_dp_htt_rx_filter_rxmon_cfg,
	.ext_mon_validate_request = ath12k_wifi8_dp_ext_mon_validate_request,
	.ext_mon_alloc = ath12k_dp_ext_mon_alloc,
	.ext_mon_free = ath12k_dp_ext_mon_free,
	.htt_rx_phy_err_filter_cfg = NULL,
	.rx_undecoded_metadata_config_filter = NULL,
	.rx_undecoded_metadata_reset_filter = NULL,
	.rx_undecoded_metadata_capture_set = NULL,
	.rx_undecoded_phy_err_mask_set = NULL,
	.ext_mon_setup_rx_filter = ath12k_wifi8_dp_ext_mon_setup_rx_filter,

	/* Below are TxMonitor Ops */
	/* At Device Init/Exit */
	.mon_tx_srng_alloc_setup = ath12k_dp_mon_tx_srng_alloc_setup,
	.mon_tx_srng_cleanup = ath12k_dp_mon_tx_srng_cleanup,
	.mon_tx_srng_init_setup = ath12k_dp_mon_tx_srng_init_setup,
	/* At VAP up/down */
	.mon_tx_htt_srng_setup = ath12k_dp_mon_tx_htt_srng_setup,
	.mon_tx_htt_srng_cleanup = ath12k_dp_mon_tx_htt_srng_cleanup,
	.mon_tx_filter_configure = ath12k_dp_mon_tx_config_filter,
	.mon_tx_filter_update = ath12k_dp_mon_tx_update_ring_filter,
	/* At Pdev Init/Exit */
	.mon_tx_dst_ring_alloc_setup = ath12k_dp_mon_tx_dst_ring_alloc_setup,
	.mon_tx_dst_ring_cleanup = ath12k_dp_mon_tx_dst_ring_cleanup,
	.mon_tx_wq_start = ath12k_dp_mon_tx_wq_start,
	.mon_tx_wq_stop = ath12k_dp_mon_tx_wq_stop,
	.htt_tx_mon_cfg_fill_extended_wmask =
		ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask,
};

static inline void
ath12k_wifi8_dp_mon_rx_memset_ppdu_info(struct ath12k_pdev_dp *pdev_dp,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp_stats *mon_stats = &pdev_dp->dp_mon_pdev->mon_stats;
	struct sk_buff *skb;
	int i;
	int num_skb = 0, frag_cnt = 0;

	/* Check all queues for unexpected length and free skbs */
	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		while ((skb = skb_dequeue(&ppdu_info->mpdu_q[i]))) {
			ath12k_dp_mon_cnt_skb_and_frags(skb, &num_skb, &frag_cnt);
			dev_kfree_skb_any(skb);
			mon_stats->num_skb_free += num_skb;
			mon_stats->pkt_tlv_free += frag_cnt;
			num_skb = 0;
			frag_cnt = 0;
		}
	}

	memset(ppdu_info, 0, sizeof(*ppdu_info));
	ppdu_info->peer_id = HAL_INVALID_PEERID;

	/* Re-initialize all queues */
	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		skb_queue_head_init(&ppdu_info->mpdu_q[i]);
}

static inline void
ath12k_wifi8_dp_mon_rx_parse_status_msdu_end(struct ath12k_mon_data *pmon)
{
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	u8 user_id = ppdu_info->user_id;

	pmon->err_bitmap = ppdu_info->errmap;
	pmon->mon_ppdu_info.mpdu_info[user_id].err_bitmap = ppdu_info->errmap;
	pmon->mon_ppdu_info.msdu_info[user_id].first_buffer = false;
	pmon->decap_format = ppdu_info->decap_format;
}

static int
ath12k_wifi8_dp_mon_rx_parse_status_buf(struct ath12k_pdev_dp *dp_pdev,
					struct ath12k_mon_data *pmon,
					const struct dp_mon_packet_info *packet_info)
{
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_pdev->dp_mon_pdev->mon_stats;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct ath12k_dp_mon_desc *mon_desc;
	struct list_head mon_desc_used_list;
	struct sk_buff *skb, *tmp_skb;
	struct hal_rx_mon_msdu_info *msdu_info;
	u32 pkt_len;
	u8 *mon_buf, user_id = ppdu_info->user_id;
	int ret = 0;

	INIT_LIST_HEAD(&mon_desc_used_list);
	mon_desc = (struct ath12k_dp_mon_desc *)(uintptr_t)(packet_info->cookie);
	if (unlikely(!mon_desc)) {
		ath12k_warn(dp, "pkt buf: NULL mon desc received in mac_id %d\n",
			    dp_pdev->mac_id);
		ret = -ENOMEM;
		return ret;
	}

	mon_buf = mon_desc->mon_buf;
	mon_desc->mon_buf = NULL;
	if (unlikely(mon_desc->magic != ATH12K_MON_MAGIC_VALUE)) {
		ath12k_warn(dp, "pkt buf: invalid magic value in mac_id %d\n",
			    dp_pdev->mac_id);
		ret = -EINVAL;
		mon_stats->invalid_pkt_magic_num++;
		goto buf_replenish;
	}

	if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
		ath12k_warn(dp,
			    "pkt buf: invalid in_use=[%d] flag, mac_id %d\n",
			    mon_desc->in_use,
			    dp_pdev->mac_id);
		ret = -EINVAL;
		mon_stats->invalid_in_use++;
		goto buf_replenish;
	}

	list_add_tail(&mon_desc->list, &mon_desc_used_list);
	mon_desc->in_use = DP_MON_DESC_PACKET_REAP;

	ath12k_core_dma_unmap_page(dp->dev, mon_desc->paddr, ATH12K_DP_MON_RX_BUF_SIZE,
				   DMA_FROM_DEVICE);

	/* The hardware reports the buffer length as (actual_length - 1),
	 * likely due to internal indexing or alignment constraints.
	 * To obtain the true buffer length for processing, increment
	 * the reported end_offset by 1 before using it.
	 */
	pkt_len = packet_info->dma_length + 1;

	if (unlikely(pkt_len > (ATH12K_DP_MON_RX_BUF_SIZE - ATH12K_MON_RX_PKT_OFFSET))) {
		ath12k_warn(dp, "pkt buf: invalid dma length %d in mac_id %d\n",
			    pkt_len, dp_pdev->mac_id);
		page_frag_free(mon_buf);
		mon_stats->pkt_tlv_free++;
		goto buf_replenish;
	}

	if (unlikely(!ppdu_info->mpdu_info[user_id].rx_hdr_rcvd)) {
		ath12k_warn(dp, "pkt buf: packet buffer without rx hdr in mac_id %d\n",
			    dp_pdev->mac_id);
		page_frag_free(mon_buf);
		mon_stats->rx_hdr_not_rcvd++;
		mon_stats->pkt_tlv_free++;
		goto buf_replenish;
	}

	if (unlikely(ppdu_info->mpdu_info[user_id].decap_type ==
		     DP_RX_DECAP_TYPE_INVALID)) {
		ath12k_warn(dp, "pkt buf: invalid decap type in mac_id %d\n",
			    dp_pdev->mac_id);
		page_frag_free(mon_buf);
		mon_stats->pkt_tlv_free++;
		goto buf_replenish;
	}

	skb = skb_peek_tail(&ppdu_info->mpdu_q[user_id]);
	if (unlikely(!skb)) {
		ath12k_warn(dp, "pkt buf: empty mpdu queue in mac_id %d\n",
			    dp_pdev->mac_id);
		page_frag_free(mon_buf);
		mon_stats->pkt_tlv_free++;
		goto buf_replenish;
	}

	tmp_skb = ath12k_dp_mon_get_skb_valid_frag(dp, skb);
	if (!tmp_skb) {
		tmp_skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR);

		if (!tmp_skb) {
			page_frag_free(mon_buf);
			mon_stats->skb_alloc_fail++;
			mon_stats->pkt_tlv_free++;
			goto buf_replenish;
		}

		mon_stats->num_skb_alloc++;
		ath12k_dp_mon_append_skb(skb, tmp_skb);
	}

	if (ppdu_info->mpdu_info[user_id].decap_type == DP_RX_DECAP_TYPE_RAW) {
		ath12k_dp_mon_add_rx_frag(tmp_skb, mon_buf, ATH12K_MON_RX_PKT_OFFSET,
					  pkt_len, false);

		/* Adjust parent skb length if a fragment gets added to the skb
		 * which got fetched from frag_list.
		 */
		if (tmp_skb != skb)
			ath12k_dp_mon_update_skb_len(skb, pkt_len);

		mon_stats->pkt_tlv_processed++;
	} else {
		ath12k_dp_mon_add_rx_frag(tmp_skb, mon_buf, ATH12K_MON_RX_PKT_OFFSET,
					  pkt_len, false);
		if (tmp_skb != skb)
			ath12k_dp_mon_update_skb_len(skb, pkt_len);

		msdu_info = (struct hal_rx_mon_msdu_info *)mon_buf;
		if (!ppdu_info->msdu_info[user_id].first_buffer) {
			msdu_info->first_buffer = true;
			ppdu_info->msdu_info[user_id].first_buffer = true;
		} else {
			msdu_info->first_buffer = false;
		}

		if (packet_info->msdu_continuation)
			msdu_info->last_buffer = false;
		else
			msdu_info->last_buffer = true;
	}

	/* Buffer address TLV is subscribed only when a full packet is
	 * requested; it is not subscribed for short packets.
	 */
	ppdu_info->mpdu_info[user_id].full_pkt = true;

	if (unlikely(packet_info->truncated)) {
		mon_stats->pkt_tlv_truncated++;
		ppdu_info->mpdu_info[user_id].truncated = true;
	}

buf_replenish:
	spin_lock_bh(&dp_mon->mon_desc_lock);
	list_splice_tail(&mon_desc_used_list, &dp_mon->mon_desc_free_list);
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	return ret;
}

static int
ath12k_wifi8_dp_mon_parse_status_rx_hdr(struct ath12k_pdev_dp *dp_pdev,
					struct ath12k_mon_data *pmon,
					struct hal_tlv_parsed_hdr *tlv_parsed_hdr,
					const void *mon_buf)
{
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct sk_buff *skb, *tmp_skb;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_pdev->dp_mon_pdev->mon_stats;
	struct ath12k_dp_rx_ext_mon *ext_mon_config =
				dp_pdev->dp_mon_pdev->rx_ext_mon_config;
	const void *tlv_data = tlv_parsed_hdr->data;
	int offset, frag_len = tlv_parsed_hdr->len - ATH12K_MON_RX_PKT_OFFSET;
	u8 user_id = ppdu_info->user_id;

	offset = (const u8 *)tlv_data - (const u8 *)mon_buf;
	offset += ATH12K_MON_RX_PKT_OFFSET;
	if (unlikely(frag_len <= 0) || frag_len > ATH12K_WIFI8_DP_MON_RX_HDR_LEN) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
			   "invalid rx header length: %d", frag_len);
		return 0;
	}

	if (!ppdu_info->mpdu_info[user_id].mpdu_start_received) {
		skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
		if (unlikely(!skb)) {
			ath12k_warn(dp_pdev->dp,
				    "skb allocation for rx hdr failed\n");
			return -ENOMEM;
		}

		mon_stats->num_skb_alloc++;
		skb_queue_tail(&ppdu_info->mpdu_q[user_id], skb);

		if (ppdu_info->mpdu_info[user_id].decap_type != DP_RX_DECAP_TYPE_RAW ||
		    (ext_mon_config && ext_mon_config->enable &&
				ext_mon_config->short_pkt_en))
			ath12k_dp_mon_add_rx_frag(skb, mon_buf, offset, frag_len, true);

		ppdu_info->mpdu_info[user_id].mpdu_start_received = true;

		/*
		 * The first 64 bytes of skb->data are used for storing MPDU metadata.
		 * After allocating a new skb, skb->data may contain junk values.
		 * Reset the metadata region to zero.
		 */
		memset(skb->data, 0, sizeof(struct ath12k_dp_mon_mpdu_meta));
	} else {
		if (ppdu_info->mpdu_info[user_id].decap_type == DP_RX_DECAP_TYPE_RAW)
			return 0;

		skb = skb_peek_tail(&ppdu_info->mpdu_q[user_id]);
		if (unlikely(!skb))
			return -ENODATA;

		tmp_skb = ath12k_dp_mon_get_skb_valid_frag(dp_pdev->dp, skb);
		if (!tmp_skb) {
			tmp_skb = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
			if (!tmp_skb)
				return -ENOMEM;

			mon_stats->num_skb_alloc++;
			ath12k_dp_mon_append_skb(skb, tmp_skb);
		}

		ath12k_dp_mon_add_rx_frag(tmp_skb, mon_buf, offset, frag_len, true);
		if (tmp_skb != skb)
			ath12k_dp_mon_update_skb_len(skb, frag_len);
	}

	ppdu_info->mpdu_info[user_id].rx_hdr_rcvd = true;

	return 0;
}

static int
ath12k_dp_mon_parse_mpdu_start(struct ath12k_pdev_dp *dp_pdev,
			       struct ath12k_mon_data *pmon)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct ath12k_dp_mon_mpdu_meta *mpdu_meta;
	struct sk_buff *skb;
	u8 user_id = ppdu_info->user_id;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_pdev->dp_mon_pdev->mon_stats;

	if (!ppdu_info->mpdu_info[user_id].rx_hdr_rcvd)
		return 0;

	skb = skb_peek_tail(&ppdu_info->mpdu_q[user_id]);
	if (unlikely(!skb)) {
		ath12k_warn(dp, "No skb found in the mpdu skb queue\n");
		mon_stats->null_mpdu_q++;
		return -ENODATA;
	}

	mpdu_meta = (struct ath12k_dp_mon_mpdu_meta *)skb->data;
	mpdu_meta->decap_type =  ppdu_info->mpdu_info[user_id].decap_type;
	return 0;
}

static void
ath12k_wifi8_dp_mon_rx_parse_mpdu_end(struct ath12k_pdev_dp *dp_pdev,
				      struct ath12k_mon_data *pmon)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct ath12k_dp_mon_mpdu_meta *mpdu_meta;
	struct sk_buff *skb;
	u8 user_id = ppdu_info->user_id;

	if (!ppdu_info->mpdu_info[user_id].rx_hdr_rcvd)
		goto reset_mpdu_info;

	skb = skb_peek_tail(&ppdu_info->mpdu_q[user_id]);
	if (unlikely(!skb)) {
		ath12k_warn(dp, "No skb found in the mpdu skb queue during mpdu_end\n");
		goto reset_mpdu_info;
	}

	mpdu_meta = (struct ath12k_dp_mon_mpdu_meta *)skb->data;
	mpdu_meta->truncated = ppdu_info->mpdu_info[user_id].truncated;
	mpdu_meta->fcs_err = ppdu_info->mpdu_info[user_id].fcs_err;
	mpdu_meta->full_pkt = ppdu_info->mpdu_info[user_id].full_pkt;

reset_mpdu_info:
	ppdu_info->mpdu_info[user_id].truncated = false;
	ppdu_info->mpdu_info[user_id].full_pkt = false;
	ppdu_info->mpdu_info[user_id].mpdu_start_received = false;
}

static int
ath12k_wifi8_dp_mon_rx_parse_dest_tlv(struct ath12k_pdev_dp *dp_pdev,
				      struct ath12k_mon_data *pmon,
				      enum hal_rx_mon_status hal_status,
				      struct hal_tlv_parsed_hdr *tlv_parsed_hdr,
				      const void *mon_buf)
{
	const void *tlv_data = tlv_parsed_hdr->data;

	switch (hal_status) {
	case HAL_RX_MON_STATUS_MPDU_START:
		return ath12k_dp_mon_parse_mpdu_start(dp_pdev, pmon);
	case HAL_RX_MON_STATUS_BUF_ADDR:
		return ath12k_wifi8_dp_mon_rx_parse_status_buf(dp_pdev, pmon, tlv_data);
	case HAL_RX_MON_STATUS_MPDU_END:
		ath12k_wifi8_dp_mon_rx_parse_mpdu_end(dp_pdev, pmon);
		break;
	case HAL_RX_MON_STATUS_MSDU_END:
		ath12k_wifi8_dp_mon_rx_parse_status_msdu_end(pmon);
		break;
	case HAL_RX_MON_STATUS_RX_HDR:
		return ath12k_wifi8_dp_mon_parse_status_rx_hdr(dp_pdev, pmon,
							       tlv_parsed_hdr,
							       mon_buf);
	default:
		break;
	}

	return 0;
}

static void
ath12k_wifi8_dp_mon_free_pkt_buf(struct ath12k_pdev_dp *pdev_dp,
				 u8 *mon_buf, u16 mon_buf_len)
{
	struct ath12k_dp *dp = pdev_dp->dp;
	struct dp_mon_packet_info *packet_info;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_dp_mon_desc *pkt_desc;
	struct list_head mon_desc_used_list;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &pdev_dp->dp_mon_pdev->mon_stats;
	u8 *ptr = mon_buf;
	u16 tlv_tag, tlv_len;
	int num_buf = 0;

	INIT_LIST_HEAD(&mon_desc_used_list);

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = le64_get_bits(tlv->tl, HAL_WIFI8_TLV_64_HDR_TAG);
		ptr += sizeof(*tlv);

		if (tlv_tag == HAL_RX_PPDU_END)
			tlv_len = sizeof(struct hal_rx_rxpcu_classification_overview);
		else
			tlv_len = le64_get_bits(tlv->tl, HAL_WIFI8_TLV_64_HDR_LEN);

		if (tlv_tag == HAL_MON_BUF_ADDR) {
			packet_info = (struct dp_mon_packet_info *)ptr;
			pkt_desc = (struct ath12k_dp_mon_desc *)
				    (uintptr_t)(packet_info->cookie);

			if (unlikely(!pkt_desc)) {
				ath12k_warn(dp, "mon_flush: NULL pkt_desc received in macid %d\n",
					    pdev_dp->mac_id);
				goto next_tlv;
			}

			if (unlikely(pkt_desc->magic != ATH12K_MON_MAGIC_VALUE)) {
				ath12k_warn(dp, "mon_flush: invalid magic value in macid %d\n",
					    pdev_dp->mac_id);
				goto next_tlv;
			}

			list_add_tail(&pkt_desc->list, &mon_desc_used_list);
			num_buf++;

			if (unlikely(pkt_desc->in_use != DP_MON_DESC_TO_HW)) {
				ath12k_warn(dp,
					    "mon_flush: invalid in_use=[%d] flag, macid %d\n",
					    pkt_desc->in_use,
					    pdev_dp->mac_id);
				goto next_tlv;
			}

			ath12k_core_dma_unmap_page(dp->dev, pkt_desc->paddr,
						   ATH12K_DP_MON_RX_BUF_SIZE,
						   DMA_FROM_DEVICE);
			mon_stats->pkt_tlv_free++;
			page_frag_free(pkt_desc->mon_buf);
			pkt_desc->mon_buf = NULL;
			pkt_desc->in_use = DP_MON_DESC_H_PROC_ERR;
		}

next_tlv:
		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);
	} while ((ptr - mon_buf) < mon_buf_len);

	if (likely(!list_empty(&mon_desc_used_list))) {
		spin_lock_bh(&dp_mon->mon_desc_lock);
		list_splice_tail(&mon_desc_used_list, &dp_mon->mon_desc_free_list);
		spin_unlock_bh(&dp_mon->mon_desc_lock);
	}
}

static enum hal_rx_mon_status
ath12k_wifi8_dp_mon_rx_parse_dest(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_mon_status_desc *status_desc)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_tlv_64_hdr *tlv;
	struct hal_tlv_parsed_hdr tlv_parsed_hdr = {0};
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	enum hal_rx_mon_status hal_status;
	u16 tlv_tag, tlv_len, buf_len = status_desc->buf_len, tlv_userid;
	s16 rem_buf_len;
	u8 *mon_buf = status_desc->mon_buf;
	u8 *ptr = mon_buf;
	int ret;

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = le64_get_bits(tlv->tl, HAL_WIFI8_TLV_64_HDR_TAG);
		tlv_len = le64_get_bits(tlv->tl, HAL_WIFI8_TLV_64_HDR_LEN);
		tlv_userid = le64_get_bits(tlv->tl, HAL_WIFI8_TLV_64_USR_ID);
		ptr += sizeof(*tlv);

		/* The actual length of PPDU_END is the combined length of many PHY
		 * TLVs that follow. Skip the TLV header and
		 * rx_rxpcu_classification_overview that follows the header to get to
		 * next TLV.
		 */

		if (tlv_tag == HAL_RX_PPDU_END)
			tlv_len = sizeof(struct hal_rx_rxpcu_classification_overview);

		tlv_parsed_hdr.tag = tlv_tag;
		tlv_parsed_hdr.len = tlv_len;
		tlv_parsed_hdr.userid = tlv_userid;
		tlv_parsed_hdr.data = ptr;

		hal_status =
			ath12k_wifi8_hal_mon_rx_parse_status_tlv(dp_pdev->dp->hal,
								 &pmon->mon_ppdu_info,
								 &tlv_parsed_hdr);

		ret = ath12k_wifi8_dp_mon_rx_parse_dest_tlv(dp_pdev, pmon,
							    hal_status,
							    &tlv_parsed_hdr,
							    mon_buf);
		/* During error case scenario, print the error type and continue
		 * to TLV parsing to free the remaining buffer address TLVs.
		 */
		if (ret)
			ath12k_warn(dp_pdev->dp,
				    "mon_rx_parse_dest failed with ret %d", ret);

		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);

		if ((ptr - mon_buf) >= buf_len)
			break;

	} while ((hal_status == HAL_RX_MON_STATUS_PPDU_NOT_DONE) ||
		 (hal_status == HAL_RX_MON_STATUS_BUF_ADDR) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_START) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_END) ||
		 (hal_status == HAL_RX_MON_STATUS_MSDU_END) ||
		 (hal_status == HAL_RX_MON_STATUS_RX_HDR));

	if (status_desc->end_of_ppdu)
		hal_status = HAL_RX_MON_STATUS_PPDU_DONE;

	if (unlikely(pmon->mon_ppdu_info.is_drop_tlv)) {
		rem_buf_len = ptr - mon_buf;
		if (rem_buf_len > 0 && rem_buf_len < buf_len) {
			buf_len -= rem_buf_len;
			ath12k_wifi8_dp_mon_free_pkt_buf(dp_pdev, ptr, buf_len);
		}
		hal_status = HAL_RX_MON_STATUS_DROP_TLV;
		mon_stats->drop_tlv++;
		page_frag_free(mon_buf);
		status_desc->mon_buf = NULL;
		mon_stats->status_buf_free++;
	}

	return hal_status;
}

int ath12k_wifi8_dp_mon_update_band_and_get_freq(struct ath12k_base *ab, int pdev_id,
						 u16 channel_num,
						 struct ieee80211_rx_status *rxs)
{
	struct ath12k *ar = ab->pdevs[pdev_id].ar;
	struct ieee80211_channel *channel;
	int freq = -1;

	if (unlikely(rxs->band == NUM_NL80211_BANDS ||
		     !ath12k_ar_to_hw(ar)->wiphy->bands[rxs->band])) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "sband is NULL for status band %d channel_num %d pdev_id %d\n",
			   rxs->band, channel_num, ar->pdev_idx);

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rxs->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		}
		spin_unlock_bh(&ar->data_lock);
	}

	if (rxs->band < NUM_NL80211_BANDS)
		freq = ieee80211_channel_to_frequency(channel_num, rxs->band);
	return freq;
}

static int
ath12k_wifi8_dp_ext_mon_rx_deliver_mpdu(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct sk_buff *mpdu,
					struct ieee80211_rx_status *rxs)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_rx_ext_mon *config;
	struct ieee80211_hdr *hdr;
	struct ath12k_ext_mon_pkt_config *pkt_config = NULL;
	u8 filter_category, type, sub_type;
	bool is_mcast = false;
	bool need_rtap = true;
	int ret = 0;

	spin_lock(&dp_mon_pdev->rx_ext_mon_lock);
	config = dp_mon_pdev->rx_ext_mon_config;

	if (unlikely(!(config && config->enable))) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_warn(dp_pdev->dp, "ext mon not enabled\n");
		return -EINVAL;
	}

	filter_category =
		ppdu_info->userstats[ppdu_info->user_id].filter_category;

	hdr = (struct ieee80211_hdr *)ath12k_dp_mon_skb_get_frag_addr(mpdu, 0);

	type = ((__le16_to_cpu(hdr->frame_control) & IEEE80211_FCTL_FTYPE) >>
		ATH12K_FC0_TYPE_SHIFT);

	if (unlikely(type >= ATH12K_EXT_MON_FRAME_MAX)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
			   "Incorrect type received in fc: %x\n", hdr->frame_control);
		return -EINVAL;
	}

	switch (filter_category) {
	case DP_MPDU_FILTER_CATEGORY_FP:
		if (config->fp_enabled) {
			pkt_config = &config->fp;
			/* When FP statistics are enabled and extended monitor filters
			 * are ORed in, extra frame type/subtypes may be received;
			 * explicitly filter out those unintended frames here.
			 */
			sub_type = ((__le16_to_cpu(hdr->frame_control) &
				     IEEE80211_FCTL_STYPE) >> ATH12K_FC0_SUBTYPE_SHIFT);
			is_mcast = is_multicast_ether_addr(hdr->addr1);
			ret = ath12k_dp_ext_mon_subtype_check(pkt_config, type,
							      sub_type, is_mcast);
			if (unlikely(ret)) {
				spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
				ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
					   "Filtered subtype %x\n", sub_type);
				return -EINVAL;
			}
		}
		break;
	case DP_MPDU_FILTER_CATEGORY_MD:
		if (config->md_enabled) {
			pkt_config = &config->md;
			if (config->peer_count && ppdu_info->nrp_info.fc_valid &&
			    ppdu_info->nrp_info.to_ds_flag &&
			    ppdu_info->nrp_info.mac_addr2_valid)
				ath12k_dp_ext_mon_update_snr(ppdu_info, config);
		}
		break;
	case DP_MPDU_FILTER_CATEGORY_MO:
		if (config->mo_enabled)
			pkt_config = &config->mo;
		break;
	case DP_MPDU_FILTER_CATEGORY_FP_MO:
		if (config->fpmo_enabled)
			pkt_config = &config->fpmo;
		break;
	default:
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
			   "Unknown filter category in wifi8 ext mon: %x\n",
			   filter_category);
		return -EINVAL;
	}

	if (unlikely(!pkt_config)) {
		spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
			   "Filter category %x not enabled in current ext_mon config\n",
			   filter_category);
		return -EINVAL;
	}

	need_rtap = config->metadata & ATH12K_EXT_MON_METADATA_RTAP_HDR;

	/* Unlock spinlock here; ext_mon_config must not be
	 * accessed beyond this point
	 */
	spin_unlock(&dp_mon_pdev->rx_ext_mon_lock);

	if (need_rtap) {
		skb_reserve(mpdu, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
		ath12k_dp_mon_update_radiotap(dp_pdev, ppdu_info, mpdu, rxs);

		ath12k_dp_mon_rx_deliver_skb(dp_pdev, NULL, mpdu,
					     rxs, ppdu_info);
	} else {
#ifdef CPTCFG_QCN_EXTN
		/*
		 * If an ext_mon listener is registered, deliver the MPDU via the
		 * ext_mon SRCU notifier chain. See struct ieee80211_ext_mon_rx_event_extn
		 * for SKB ownership, rx_status placement.
		 *
		 * If no ext_mon listener is registered:
		 *   - need_rtap set: deliver to mac80211 (mac80211 adds radiotap).
		 *   - need_rtap not set: drop the MPDU.
		 */
		if (ieee80211_ext_mon_rx_notifier_has_listeners_extn()) {
			struct ieee80211_rx_status *rx_status;
			struct ieee80211_ext_mon_rx_event_extn rx_event;
			enum ieee80211_ext_mon_event_type_extn ext_mon_event =
						IEEE80211_EXT_MON_PRE_RTAP;

			rxs->link_valid = 0;
			rxs->link_id = 0;
			rx_status = IEEE80211_SKB_RXCB(mpdu);
			*rx_status = *rxs;

			rx_event.mpdu = mpdu;
			rx_event.hw = ath12k_dp_pdev_to_hw(dp_pdev);
			ieee80211_ext_mon_rx_notifier_call_extn(ext_mon_event,
								&rx_event);

			/*
			 * Driver always frees the original SKB after the chain returns.
			 * Listeners that need to retain the frame must call skb_clone()
			 * inside their callback and take ownership of the clone.
			 */
			dev_kfree_skb_any(mpdu);
		} else {
			ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON,
				   "pkt dropped! listener absent & rtap not needed\n");
			return -EINVAL;
		}
#else
		/* Rtap not needed, packet to be dropped */
		return -EOPNOTSUPP;
#endif /* CPTCFG_QCN_EXTN */
	}

	return 0;
}

int
ath12k_wifi8_dp_mon_rx_deliver_mpdu(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info,
				    struct sk_buff *mpdu)
{
	struct ieee80211_rx_status rxs = {0};
	int freq_update = -1, ret = 0;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	ath12k_dp_mon_fill_rx_stats_info(ppdu_info, &rxs);

	freq_update = ath12k_wifi8_dp_mon_update_band_and_get_freq(dp_pdev->dp->ab,
								   dp_pdev->ar->pdev_idx,
								   ppdu_info->chan_num,
								   &rxs);
	if (freq_update != -1)
		rxs.freq = freq_update;

	rxs.flag |= RX_FLAG_ONLY_MONITOR;
	if (skb_shinfo(mpdu)->nr_frags)
		rxs.flag |= RX_FLAG_AMSDU_MORE;

	if (ppdu_info->mpdu_info[ppdu_info->user_id].fcs_err)
		rxs.flag |= RX_FLAG_FAILED_FCS_CRC;

	if (!(dp_mon_pdev->rx_ext_mon_config &&
	      dp_mon_pdev->rx_ext_mon_config->enable)) {
		skb_reserve(mpdu, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
		ath12k_dp_mon_update_radiotap(dp_pdev, ppdu_info, mpdu, &rxs);
		ath12k_dp_mon_rx_deliver_skb(dp_pdev, NULL, mpdu, &rxs, ppdu_info);
	} else {
		ret = ath12k_wifi8_dp_ext_mon_rx_deliver_mpdu(dp_pdev, ppdu_info,
							      mpdu, &rxs);
	}

	return ret;
}

static int
ath12k_wifi8_dp_mon_pad_amsdu(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu,
			      struct sk_buff *parent_mpdu,
			      struct ath12k_dp_mon_pad_params *params)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int amsdu_pad;
	int frag_idx = params->frag_idx;
	u8 *pad_start_ptr;

	/* Calculate AMSDU padding to align to 4-byte boundary */
	amsdu_pad = (params->frag_size + params->msdu_llc_len) % 4;
	amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

	if (amsdu_pad && amsdu_pad <= params->pad_byte_holder) {
		skb_coalesce_rx_frag(mpdu, params->frag_idx, amsdu_pad, 0);
		pad_start_ptr = (u8 *)ath12k_dp_mon_skb_get_frag_addr(mpdu,
								      params->frag_idx);

		if (!params->is_head_msdu) {
			parent_mpdu->len += amsdu_pad;
			parent_mpdu->data_len += amsdu_pad;
		}

		if (!pad_start_ptr)
			return -EINVAL;

		pad_start_ptr += ath12k_dp_mon_get_frag_size_by_idx(dp,
								    mpdu, frag_idx);
		/* pad_start_ptr points at end of frag, to add amsdu padding
		 * subtract the amsdu pad len in pad_start_ptr.
		 */
		pad_start_ptr -= amsdu_pad;
		memset(pad_start_ptr, 0, amsdu_pad);
	}

	return 0;
}

static u32
ath12k_wifi8_dp_mon_calculate_hdrlen(struct sk_buff *mpdu, u32 *msdu_llc_len,
				     bool *is_amsdu)
{
	struct ieee80211_hdr *hdr;
	__le16 fc;
	u16 hdr_len = 0, tot_hdr_len;
	u8 *hdr_desc;

	*is_amsdu = false;

	/* Get pointer to the start of the 802.11 header from frag[0]
	 */
	hdr_desc = (u8 *)ath12k_dp_mon_skb_get_frag_addr(mpdu, 0);
	if (unlikely(!hdr_desc))
		return hdr_len;

	hdr_len = sizeof(struct ieee80211_hdr_3addr);
	hdr = (struct ieee80211_hdr *)hdr_desc;
	fc = hdr->frame_control;
	if (ieee80211_has_a4(fc))
		hdr_len += 6;

	if (ieee80211_is_data_qos(fc)) {
		u8 *qos_hdr = ieee80211_get_qos_ctl(hdr);

		hdr_len += 2;
		if (*qos_hdr & IEEE80211_QOS_CTL_A_MSDU_PRESENT)
			*is_amsdu = true;
	}

	if (ieee80211_has_protected(fc)) {
		u8 *iv = (u8 *)hdr_desc + hdr_len;

		hdr_len += IEEE80211_WEP_IV_LEN;

		/* Check if the Key IV bit is set in the 4th byte of the IV.
		 * If set, this indicates the presence of an Integrity Check Value,
		 * so we need to account for its length in the header.
		 */
		if (iv[3] & ATH12K_DP_MON_KEYIV)
			hdr_len += IEEE80211_WEP_ICV_LEN;
	}

	if (ieee80211_has_order(fc))
		hdr_len += IEEE80211_HT_CTL_LEN;

	*msdu_llc_len = ATH12K_DP_MON_LLC_SIZE + ATH12K_DP_MON_SNAP_SIZE;
	if (*is_amsdu)
		*msdu_llc_len += ATH12K_DP_MON_DECAP_HDR_SIZE;

	tot_hdr_len = *msdu_llc_len + hdr_len;

	return tot_hdr_len;
}

static void
ath12k_wifi8_dp_mon_process_msdu_frag(struct ath12k_pdev_dp *dp_pdev,
				      struct sk_buff *mpdu, u8 msdu_llc_len,
				      u32 tot_msdu_len,
				      bool is_amsdu)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct sk_buff *msdu_cur;
	struct hal_rx_mon_msdu_info *msdu_meta;
	void *frag_addr;
	u32 frag_idx = 1, hdr_frag_size, frag_size, pad_byte_holder = 0, frag_offset;
	int ret, llc_amsdu_len, size, frag_llc_len;
	u8 frag_iter, num_frags, amsdu_pad = 0, l2_hdr_offset, tot_num_frags;
	bool prev_msdu_end_received = false, is_head_msdu = true;

	tot_num_frags = ath12k_dp_mon_get_num_frags_in_fraglist(mpdu);

	for (msdu_cur = mpdu; msdu_cur;) {
		/*frag_iter starts from 0 for 2nd skb onwards
		 */
		frag_iter = (msdu_cur == mpdu) ? 2 : 0;
		num_frags = skb_shinfo(msdu_cur)->nr_frags;

		for (; frag_iter < num_frags; frag_iter++) {
			frag_idx++;

			/* After finishing an MSDU (i.e., one AMSDU subframe), adjust the
			 * next fragment's offset to the start of the AMSDU subframe
			 * header. This retains both the AMSDU subframe header and the
			 * LLC/SNAP header for correct reassembly.
			 * Note: msdu_llc_len here represents (LLC/SNAP + AMSDU subframe
			 * header) length. Although calculated as (LLC/SNAP + decap
			 * header) length, this works as both have the same size in this
			 * context.
			 */
			if (prev_msdu_end_received) {
				hdr_frag_size =
				ath12k_dp_mon_get_frag_size_by_idx(dp, msdu_cur,
								   frag_iter);
				/* Adjust page frag offset to point to LLC/SNAP header*/
				if (hdr_frag_size > msdu_llc_len) {
					frag_llc_len = hdr_frag_size - msdu_llc_len;
					skb_coalesce_rx_frag(msdu_cur, frag_iter,
							     -frag_llc_len, 0);

					if (!is_head_msdu) {
						mpdu->len -= frag_llc_len;
						mpdu->data_len -= frag_llc_len;
					}
				}

				prev_msdu_end_received = false;
				continue;
			}

			frag_addr = ath12k_dp_mon_skb_get_frag_addr(msdu_cur, frag_iter);
			if (unlikely(!frag_addr))
				continue;

			msdu_meta = (struct hal_rx_mon_msdu_info *)
					((u8 *)frag_addr - ATH12K_MON_RX_PKT_OFFSET);
			frag_size =
				ath12k_dp_mon_get_frag_size_by_idx(dp, msdu_cur,
								   frag_iter);

			/*For middle buffers, no need to add headers
			 */
			if (!msdu_meta->first_buffer && !msdu_meta->last_buffer) {
				tot_msdu_len += frag_size;
				amsdu_pad = 0;
				continue;
			}

			/* calculate if current buffer has palceholder to accommodate
			 * amsdu pad byte
			 */
			pad_byte_holder = ATH12K_DP_MON_RX_BUF_SIZE -
						(frag_size + ATH12K_MON_RX_PKT_OFFSET);

			/* Below mention three conditions are handled here.
			 * 1. If msdu has single buffer
			 * 2. First buffer in case MSDU is spread in multiple buffer.
			 * 3. Last buffer in case MSDU is spread in multiple buffer.
			 *		First buffer	|	Last buffer
			 * Case 1:		1	|	1
			 * Case 2:		1	|	0
			 * Case 3:		0	|	1
			 * In 3rd case, only L2 header padding byte is 0 and in other
			 * cases it will be 2 bytes.
			 */

			l2_hdr_offset = msdu_meta->first_buffer ? 2 : 0;
			if (msdu_meta->first_buffer) {
				/* Adjust fragment offset to point to the start of the
				 * AMSDU subframe header. Only the first MSDU has the
				 * 802.11 header; subsequent subframes begin directly
				 * with the subframe header.
				 */
				hdr_frag_size =
				ath12k_dp_mon_get_frag_size_by_idx(dp, msdu_cur,
								   frag_iter - 1);
				llc_amsdu_len = msdu_llc_len + amsdu_pad;
				if (hdr_frag_size > llc_amsdu_len) {
					llc_amsdu_len = msdu_llc_len + amsdu_pad;
					size = hdr_frag_size - llc_amsdu_len;
					skb_coalesce_rx_frag(msdu_cur, frag_iter - 1,
							     -size, 0);
					if (!is_head_msdu) {
						mpdu->len -= size;
						mpdu->data_len -= size;
					}
				}

				/* Adjust page frag offset to point to the AMSDU subframe
				 * header
				 */
				frag_offset = ATH12K_DP_MON_DECAP_HDR_SIZE +
						l2_hdr_offset;
				if (frag_size > frag_offset) {
					ret = ath12k_dp_mon_adj_frag_offset(msdu_cur,
									    frag_iter,
									    frag_offset);
					if (ret) {
						ath12k_warn(dp,
							    "Failed to adjust offset %d\n",
							    frag_iter);
						continue;
					}

					frag_size -= frag_offset;
					if (!is_head_msdu) {
						mpdu->len -= frag_offset;
						mpdu->data_len -= frag_offset;
					}
				}

				/* calculate new page offset and create hole if amsdu_pad
				 * required
				 */
				tot_msdu_len = frag_size;

				/* No amsdu padding required for first frame of
				 * continuation buffer
				 */
				if (!msdu_meta->first_buffer) {
					amsdu_pad = 0;
					continue;
				}
			} else {
				tot_msdu_len += frag_size;
			}

			/* Below mentioned two cases are handled here
			 * 1. Single buffer MSDU
			 * 2. Last buffer of MSDU in case of multiple buffer MSDU
			 */

			/* This flag is used to identify the msdu boundary
			 */
			prev_msdu_end_received = true;

			/* check size of buffer if amsdu padding required if it is last
			 * subframe, then padding is not required
			 */
			if (is_amsdu && (frag_idx != (tot_num_frags - 1))) {
				/* tot_msdu_len is total data payload size
				 * amsdu padding gets calculated on complete AMSDU frame,
				 * so msdu_llc_len needs to be added to data payload.
				 */
				struct ath12k_dp_mon_pad_params params = {
					.frag_size = tot_msdu_len,
					.msdu_llc_len =  msdu_llc_len,
					.pad_byte_holder = pad_byte_holder,
					.frag_idx = frag_iter,
					.is_head_msdu = is_head_msdu,
				};

				ret = ath12k_wifi8_dp_mon_pad_amsdu(dp_pdev,
								    msdu_cur,
								    mpdu,
								    &params);
				if (unlikely(ret)) {
					ath12k_warn(dp,
						    "proc_msdu_frag: AMSDU padding failed %d\n",
						    ret);
					continue;
				}
			}

			tot_msdu_len = 0;
		}

		msdu_cur = is_head_msdu ?
			   skb_shinfo(msdu_cur)->frag_list : msdu_cur->next;
		is_head_msdu = false;
	}
}

static struct hal_rx_mon_msdu_info *
ath12k_dp_mon_get_msdu_meta(struct sk_buff *mpdu)
{
	void *frag_addr;

	frag_addr = ath12k_dp_mon_skb_get_frag_addr(mpdu, 1);
	if (!frag_addr)
		return NULL;

	return (struct hal_rx_mon_msdu_info *)((u8 *)frag_addr -
						ATH12K_MON_RX_PKT_OFFSET);
}

static int
ath12k_wifi8_dp_mon_restitch_frags(struct sk_buff *mpdu,
				   struct ath12k_pdev_dp *dp_pdev)
{
	struct sk_buff *head_msdu;
	struct hal_rx_mon_msdu_info *msdu_meta;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_pdev->dp_mon_pdev->mon_stats;
	u32 hdr_frag_size, frag_size, frag_page_offset, msdu_llc_len, pad_byte_holder;
	u32 tot_msdu_len = 0;
	u8 mpdu_buf_len;
	u8 num_frags = ath12k_dp_mon_get_num_frags_in_fraglist(mpdu);
	struct vlan_ethhdr *ethvlan;
	void *frag_addr;
	bool is_amsdu = false;
	int ret;

	head_msdu = mpdu;

	if (unlikely(num_frags < ATH12K_DP_MON_MIN_FRAGS_RESTITCH)) {
		mon_stats->restitch_insuff_frags_cnt++;
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
			   "mon_rx_restitch: not enough frags %d to proceed further",
			   num_frags);
		ret = -EINVAL;
		mon_stats->min_frags_unavailable++;
		goto free_mpdu;
	}

	mpdu_buf_len = ath12k_wifi8_dp_mon_calculate_hdrlen(mpdu, &msdu_llc_len,
							    &is_amsdu);
	if (unlikely(!mpdu_buf_len)) {
		ath12k_warn(dp, "mon_rx_restitch: calculated buffer length is 0\n");
		ret = -EINVAL;
		mon_stats->invalid_mpdu_hdr_len++;
		goto free_mpdu;
	}

	hdr_frag_size = ath12k_dp_mon_get_frag_size_by_idx(dp, mpdu, 0);
	if (unlikely(!hdr_frag_size)) {
		ath12k_warn(dp, "mon_rx_restitch: rx header size is 0\n");
		ret = -EINVAL;
		goto free_mpdu;
	}

	/* Adjust the fragment offset to align with the start of the 802.11 header.
	 * If the current fragment size is larger than the expected MPDU buffer length,
	 */
	if (hdr_frag_size > mpdu_buf_len)
		skb_coalesce_rx_frag(head_msdu, 0, -(hdr_frag_size - mpdu_buf_len), 0);

	msdu_meta = ath12k_dp_mon_get_msdu_meta(mpdu);
	if (unlikely(!msdu_meta)) {
		ath12k_warn(dp,
			    "mon_rx_restitch: Failed to get MSDU metadata from frag-1\n");
		ret = -EINVAL;
		goto free_mpdu;
	}

	frag_size = ath12k_dp_mon_get_frag_size_by_idx(dp, mpdu, 1);
	if (unlikely(!frag_size)) {
		ath12k_warn(dp, "mon_rx_restitch: pkt size is 0\n");
		ret = -EINVAL;
		goto free_mpdu;
	}
	pad_byte_holder = ATH12K_DP_MON_RX_BUF_SIZE -
				(frag_size + ATH12K_MON_RX_PKT_OFFSET);

	/* Adjust frag[1] offset to skip decap header and L3 padding.
	 * This ensures the fragment points to the start of the L3 payload.
	 */
	frag_page_offset = ATH12K_DP_MON_DECAP_HDR_SIZE + ATH12K_DP_MON_L3_HDR_PAD;

	frag_addr = ath12k_dp_mon_skb_get_frag_addr(mpdu, 1);
	ethvlan = (struct vlan_ethhdr *)(frag_addr + ATH12K_DP_MON_L3_HDR_PAD);
	if (ethvlan->h_vlan_proto == htons(ETH_P_8021Q))
		frag_page_offset += ATH12K_DP_MON_ETH_TYPE_VLAN_LEN;
	else if (ethvlan->h_vlan_proto == htons(ETH_P_8021AD))
		frag_page_offset += ATH12K_DP_MON_ETH_TYPE_DOUBLE_VLAN_LEN;

	if (unlikely(frag_size <= frag_page_offset)) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
			   "mon_rx_restitch: pkt size %u is less than frag offset %u\n",
			   frag_size, frag_page_offset);
		ret = -EINVAL;
		goto free_mpdu;
	}

	ret = ath12k_dp_mon_adj_frag_offset(mpdu, 1, frag_page_offset);
	if (unlikely(ret)) {
		ath12k_warn(dp,
			    "mon_rx_restitch: Failed to adjust frag[1] offset by %d\n",
			    frag_page_offset);
		ret = -EINVAL;
		goto free_mpdu;
	}

	frag_size = frag_size - frag_page_offset;
	if (msdu_meta->first_buffer && msdu_meta->last_buffer) {
		/* MSDU is a single buffer
		 */
		if (is_amsdu) {
			/* frag size is datapayload size, amsdu padding gets calculated on
			 * complete AMSDU frame, so msdu_llc_len needs to be added to data
			 * payload.
			 */
			struct ath12k_dp_mon_pad_params params = {
				.frag_size = frag_size,
				.msdu_llc_len = msdu_llc_len,
				.pad_byte_holder = pad_byte_holder,
				.frag_idx = 1,
				.is_head_msdu = true,
			};

			ret = ath12k_wifi8_dp_mon_pad_amsdu(dp_pdev, mpdu, mpdu, &params);
			if (unlikely(ret)) {
				ath12k_warn(dp,
					    "mon_rx_restitch: AMSDU padding failed %d\n",
					    ret);
				ret = -EINVAL;
				goto free_mpdu;
			}
		}
	} else {
		tot_msdu_len = frag_size;
	}

	ath12k_wifi8_dp_mon_process_msdu_frag(dp_pdev, mpdu, msdu_llc_len,
					      tot_msdu_len, is_amsdu);
free_mpdu:
	return ret;
}

void ath12k_wifi8_dp_mon_rx_process_mpdu_queue(struct ath12k_pdev_dp *dp_pdev,
					       struct hal_rx_mon_ppdu_info *ppdu_info,
					       int queue_idx)
{
	struct sk_buff *mpdu;
	struct ath12k_dp_mon_mpdu_meta *mpdu_meta;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_pdev->dp_mon_pdev->mon_stats;
	u32 buf_size = ATH12K_DP_MON_RX_BUF_SIZE, num_skb = 0, pkt_tlv = 0;
	int ret, fcs_len_left, last_frag_idx, last_frag_size;

	while ((mpdu = skb_dequeue(&ppdu_info->mpdu_q[queue_idx]))) {
		mpdu_meta = (struct ath12k_dp_mon_mpdu_meta *)mpdu->data;
		/* In some cases, an RX_HDR TLV may get received with no content
		 * and without an accompanying BUFFER ADDR TLV—meaning there is
		 * no payload attached. In such cases, the skb may have fragments
		 * present (nr_frags > 0) but its total length (skb->len) is zero.
		 * To avoid processing an skb with zero length, add a check to
		 * skip these cases.
		 */
		if (!mpdu->len || mpdu_meta->truncated) {
			ath12k_dp_mon_cnt_skb_and_frags(mpdu, &num_skb,
							&pkt_tlv);
			mon_stats->num_skb_raw += num_skb;
			mon_stats->num_frag_raw += pkt_tlv;
			dev_kfree_skb_any(mpdu);
			mon_stats->num_skb_free += num_skb;
			mon_stats->pkt_tlv_free += pkt_tlv;
			num_skb = 0;
			pkt_tlv = 0;
			goto next_mpdu;
		}

		if (mpdu_meta->decap_type == DP_RX_DECAP_TYPE_RAW) {
			if (mpdu_meta->full_pkt) {
				fcs_len_left = FCS_LEN;
				last_frag_idx = skb_shinfo(mpdu)->nr_frags - 1;
				if (skb_shinfo(mpdu)->nr_frags >= 2) {
					last_frag_size =
					ath12k_dp_mon_get_frag_size_by_idx(
								dp_pdev->dp,
								mpdu, last_frag_idx);
					if (last_frag_size > 0 && last_frag_size <=
					    FCS_LEN) {
						ath12k_dp_mon_skb_remove_frag(
								dp_pdev->dp,
								mpdu, last_frag_idx,
								buf_size);
						fcs_len_left -= last_frag_size;
					}
				}

				last_frag_idx = skb_shinfo(mpdu)->nr_frags - 1;
				if (fcs_len_left > 0) {
					last_frag_size =
					ath12k_dp_mon_get_frag_size_by_idx(
								dp_pdev->dp, mpdu,
								last_frag_idx);
					if (last_frag_size <= fcs_len_left) {
						dev_kfree_skb_any(mpdu);
						mon_stats->num_skb_free++;
						num_skb = 0;
						pkt_tlv = 0;
						goto next_mpdu;
					}
					skb_coalesce_rx_frag(mpdu, last_frag_idx,
							-fcs_len_left, 0);
				}
			}
			ath12k_dp_mon_cnt_skb_and_frags(mpdu, &num_skb, &pkt_tlv);
			mon_stats->num_skb_raw += num_skb;
			mon_stats->num_frag_raw += pkt_tlv;
		} else {
			ath12k_dp_mon_cnt_skb_and_frags(mpdu, &num_skb,
							&pkt_tlv);
			mon_stats->num_skb_eth += num_skb;
			mon_stats->num_frag_eth += pkt_tlv;
			if (mpdu_meta->full_pkt) {
				ret = ath12k_wifi8_dp_mon_restitch_frags(mpdu, dp_pdev);
				if (unlikely(ret)) {
					dev_kfree_skb_any(mpdu);
					mon_stats->num_skb_free += num_skb;
					mon_stats->pkt_tlv_free += pkt_tlv;
					num_skb = 0;
					pkt_tlv = 0;
					goto next_mpdu;
				}
			}
		}

		ret = ath12k_wifi8_dp_mon_rx_deliver_mpdu(dp_pdev, ppdu_info, mpdu);
		if (unlikely(ret)) {
			dev_kfree_skb_any(mpdu);
			mon_stats->num_skb_free++;
			num_skb = 0;
			pkt_tlv = 0;
		}

next_mpdu:
		mon_stats->num_skb_to_mac80211 += num_skb;
		mon_stats->pkt_tlv_to_mac80211 += pkt_tlv;
	}
}

enum hal_rx_mon_status
ath12k_wifi8_dp_mon_rx_parse_ppdu_status(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_mon_data *pmon,
					 struct ath12k_dp_mon_status_desc *status_desc)
{
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	enum hal_rx_mon_status hal_status;
	int i;

	hal_status = ath12k_wifi8_dp_mon_rx_parse_dest(dp_pdev, status_desc);
	if (hal_status != HAL_RX_MON_STATUS_PPDU_DONE)
		return hal_status;

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		ath12k_wifi8_dp_mon_rx_process_mpdu_queue(dp_pdev, ppdu_info, i);

	return hal_status;
}

static void ath12k_wifi8_dp_mon_h_flush_tlv(struct ath12k_pdev_dp *pdev_dp,
					    struct ath12k_dp_mon_status_desc *status_desc)
{
	struct ath12k_pdev_mon_dp_stats *mon_stats = &pdev_dp->dp_mon_pdev->mon_stats;
	u8 *mon_buf = status_desc->mon_buf;
	u16 mon_buf_len = status_desc->buf_len;
	u8 *ptr = mon_buf;

	ath12k_wifi8_dp_mon_free_pkt_buf(pdev_dp, ptr, mon_buf_len);

	mon_stats->status_buf_free++;
	page_frag_free(mon_buf);
	status_desc->mon_buf = NULL;
}

static inline void
ath12k_wifi8_dp_mon_rx_reset_ppdu_desc(struct ath12k_dp_mon_ppdu_desc *ppdu_desc)
{
	memset(ppdu_desc->status_desc, 0,
	       ppdu_desc->status_desc_cnt * sizeof(*ppdu_desc->status_desc));
	ppdu_desc->status_desc_cnt = 0;
}

static void ath12k_wifi8_dp_mon_rx_h_empty_desc(struct ath12k_pdev_dp *pdev_dp)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_dp_mon_ppdu_desc *last_ppdu_desc;
	struct ath12k_dp_mon_status_desc *status_desc;
	int desc_cnt;

	last_ppdu_desc = list_last_entry(&dp_mon_pdev->ppdu_desc_used_list,
					 struct ath12k_dp_mon_ppdu_desc,
					 list);

	list_del_init(&last_ppdu_desc->list);

	if (unlikely(!last_ppdu_desc->status_desc_cnt)) {
		ath12k_warn(pdev_dp->dp,
			    "empty desc: invalid status_desc_cnt\n");
		goto free_desc;
	}

	for (desc_cnt = 0; desc_cnt < last_ppdu_desc->status_desc_cnt; desc_cnt++) {
		status_desc = &last_ppdu_desc->status_desc[desc_cnt];
		if (unlikely(!status_desc->mon_buf))
			continue;

		ath12k_core_dma_unmap_page(pdev_dp->dp->dev, status_desc->paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE,
					   DMA_FROM_DEVICE);
		ath12k_wifi8_dp_mon_h_flush_tlv(pdev_dp, status_desc);
	}

free_desc:
	ath12k_wifi8_dp_mon_rx_reset_ppdu_desc(last_ppdu_desc);
	list_add_tail(&last_ppdu_desc->list, &dp_mon_pdev->ppdu_desc_free_list);
}

static void
ath12k_wifi8_dp_mon_desc_list_add_to_free(struct list_head *local_list,
					  struct ath12k_dp_mon *dp_mon)
{
	spin_lock_bh(&dp_mon->mon_desc_lock);
	list_splice_tail_init(local_list, &dp_mon->mon_desc_free_list);
	spin_unlock_bh(&dp_mon->mon_desc_lock);
}

static void
ath12k_wifi8_dp_mon_flush_used_list(struct ath12k_pdev_dp *dp_pdev,
				    struct list_head *mon_desc_used_list)
{
	struct ath12k_dp_mon_desc *tmp_desc, *entry_desc;
	struct ath12k_dp_mon_status_desc desc;
	struct ath12k_dp_mon *dp_mon = dp_pdev->dp_mon_pdev->dp_mon;

	list_for_each_entry_safe(entry_desc, tmp_desc,
				 mon_desc_used_list, list) {
		if (unlikely(!entry_desc->mon_buf))
			continue;

		ath12k_core_dma_unmap_page(dp_pdev->dp->dev, entry_desc->paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE,
					   DMA_FROM_DEVICE);
		desc.mon_buf = entry_desc->mon_buf;
		desc.buf_len = entry_desc->buf_len;
		desc.end_of_ppdu = entry_desc->end_of_ppdu;
		ath12k_wifi8_dp_mon_h_flush_tlv(dp_pdev, &desc);
	}

	ath12k_wifi8_dp_mon_desc_list_add_to_free(mon_desc_used_list, dp_mon);
}

static struct ath12k_dp_mon_ppdu_desc *
ath12k_wifi8_dp_mon_rx_get_ppdu_desc(struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	ppdu_desc = list_first_entry_or_null(&dp_mon_pdev->ppdu_desc_free_list,
					     struct ath12k_dp_mon_ppdu_desc, list);
	if (ppdu_desc)
		list_del(&ppdu_desc->list);

	dp_mon_pdev->mon_stats.ppdu_desc_free--;
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);

	return ppdu_desc;
}

static int ath12k_wifi8_dp_mon_rx_add_ppdu_desc(struct list_head *mon_desc_used_list,
						struct ath12k_pdev_mon_dp *dp_mon_pdev)
{
	struct ath12k_dp_mon *dp_mon = dp_mon_pdev->dp_mon;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_desc *desc;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	int desc_cnt;

	ppdu_desc = ath12k_wifi8_dp_mon_rx_get_ppdu_desc(dp_mon_pdev);
	if (!ppdu_desc) {
		mon_stats->ppdu_desc_free_list_empty_cnt++;
		ath12k_dbg(dp_mon->dp->ab, ATH12K_DBG_DP_MON,
			   "No entry in the ppdu desc free list\n");
		return -ENOENT;
	}

	list_for_each_entry(desc, mon_desc_used_list, list) {
		struct ath12k_base *ab = dp_mon->dp->ab;
		desc_cnt = ppdu_desc->status_desc_cnt;
		if (unlikely(desc_cnt >= ATH12K_DP_MON_STATUS_BUF)) {
			ath12k_warn(dp_mon->dp,
				    "status desc buffer full (count = %u, max = %u)",
				    ppdu_desc->status_desc_cnt, ATH12K_DP_MON_STATUS_BUF);
			ath12k_wifi8_dp_mon_rx_reset_ppdu_desc(ppdu_desc);
			spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
			list_add_tail(&ppdu_desc->list,
				      &dp_mon_pdev->ppdu_desc_free_list);
			mon_stats->ppdu_desc_free++;
			spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
			return -EOVERFLOW;
		}

		ppdu_desc->status_desc[desc_cnt].mon_buf = desc->mon_buf;
		ppdu_desc->status_desc[desc_cnt].paddr = desc->paddr;
		ppdu_desc->status_desc[desc_cnt].buf_len = desc->buf_len;
		ppdu_desc->status_desc[desc_cnt].end_of_ppdu = desc->end_of_ppdu;
		ppdu_desc->status_desc_cnt++;
	}

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	list_add_tail(&ppdu_desc->list, &dp_mon_pdev->ppdu_desc_used_list);
	mon_stats->ppdu_desc_used++;
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);

	queue_work(dp_mon_pdev->rxmon_wq, &dp_mon_pdev->rxmon_work);

	ath12k_wifi8_dp_mon_desc_list_add_to_free(mon_desc_used_list, dp_mon);

	return 0;
}

static void ath12k_dp_rx_mon_ppdu_notify(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *pdev_dp,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_ppdu_event event;
	struct sk_buff *skb;
	struct ath12k_ppdu_rx_info *ppdu_evt_data;
	unsigned int len;

	/* Early exit if no one is listening for RX events - avoid unnecessary work */
	if (!ath12k_ppdu_notifier_has_listeners(ATH12K_EVENT_PPDU_RX_COMPLETE))
		return;

	if (ppdu_info->peer_id == HAL_INVALID_PEERID) {
		if (unlikely(!ath12k_dp_mon_get_rx_cfr_rcc_filter_valid(pdev_dp)))
			return;
	}

	/* Send PPDU notification to registered listeners */
	ppdu_info->device_id = ath12k_get_ab_device_id(dp->ab);

	len = sizeof(*ppdu_evt_data);
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
				"Allocation failed for RX PPDU evt notification data");
		return;
	}

	ppdu_evt_data = skb_put_zero(skb, len);
	memcpy(&ppdu_evt_data->ppdu_info, ppdu_info, sizeof(*ppdu_info));

	/*
	 * Re-initialize the MPDU queues to safe state
	 * to avoid corrupted access by listener.
	 */
	for (int i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		skb_queue_head_init(&ppdu_evt_data->ppdu_info.mpdu_q[i]);

	memset(&event, 0, sizeof(event));
	event.skb = skb;

	ath12k_ppdu_notifier_call_chain(ATH12K_EVENT_PPDU_RX_COMPLETE, &event);

	/*
	 * NOTE:
	 * This skb is producer-owned.
	 * Freeing or cloning it from listener context will cause leaks.
	 * Only skb_copy() is permitted if a separate buffer is needed.
	 */
	if (refcount_read(&skb->users) > 1)
		ath12k_warn(dp, "SKB ref cnt held by Rx PPDU listener = %d\n",
			    refcount_read(&skb->users));
	kfree_skb(skb);
}

static void
ath12k_wifi8_dp_mon_rx_h_drop_tlv(struct ath12k_pdev_dp *pdev_dp,
				  struct hal_rx_mon_ppdu_info *ppdu_info,
				  struct ath12k_dp_mon_ppdu_desc *ppdu_desc,
				  int desc_cnt)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	struct ath12k_dp_mon_status_desc *status_desc;
	struct sk_buff *mpdu;
	u32 num_skb_free = 0;
	u32 pkt_tlv_free = 0;
	u32 status_desc_cnt, i;

	status_desc_cnt = ppdu_desc->status_desc_cnt;
	for (; desc_cnt < status_desc_cnt; desc_cnt++) {
		status_desc = &ppdu_desc->status_desc[desc_cnt];
		if (!status_desc->mon_buf)
			continue;

		ath12k_core_dma_unmap_page(pdev_dp->dp->dev, status_desc->paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE, DMA_FROM_DEVICE);
		ath12k_wifi8_dp_mon_h_flush_tlv(pdev_dp, status_desc);
	}

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++) {
		while ((mpdu = skb_dequeue(&ppdu_info->mpdu_q[i]))) {
			ath12k_dp_mon_cnt_skb_and_frags(mpdu, &num_skb_free,
							&pkt_tlv_free);
			dev_kfree_skb_any(mpdu);
			mon_stats->num_skb_free += num_skb_free;
			mon_stats->pkt_tlv_free += pkt_tlv_free;
			num_skb_free = 0;
			pkt_tlv_free = 0;
		}
	}
}

static void
ath12k_wifi8_dp_mon_rx_process_ppdu(struct work_struct *work)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev =
		container_of(work, struct ath12k_pdev_mon_dp, rxmon_work);
	struct ath12k_pdev_dp *pdev_dp = dp_mon_pdev->dp_pdev;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_status_desc *status_desc;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_neighbor_peer *nrp, *tmp;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	enum hal_rx_mon_status hal_status;
	int desc_cnt;
	u8 filter_category = 0, ppdu_desc_prcd = 0;
	bool is_addr_equal;

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	list_splice_init(&dp_mon_pdev->ppdu_desc_used_list,
			 &dp_mon_pdev->ppdu_desc_proc_list);
	mon_stats->ppdu_desc_proc += mon_stats->ppdu_desc_used;
	mon_stats->ppdu_desc_used = 0;
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);

	list_for_each_entry(ppdu_desc, &dp_mon_pdev->ppdu_desc_proc_list, list) {
		for (desc_cnt = 0; desc_cnt < ppdu_desc->status_desc_cnt; desc_cnt++) {
			status_desc = &ppdu_desc->status_desc[desc_cnt];
			if (!status_desc->mon_buf)
				continue;

			ath12k_core_dma_unmap_page(dp->dev, status_desc->paddr,
						   ATH12K_DP_MON_RX_BUF_SIZE,
						   DMA_FROM_DEVICE);

			mon_stats->status_buf_processed++;
			hal_status =
				ath12k_wifi8_dp_mon_rx_parse_ppdu_status(pdev_dp, pmon,
									 status_desc);
			if (unlikely(hal_status == HAL_RX_MON_STATUS_DROP_TLV)) {
				ath12k_wifi8_dp_mon_rx_h_drop_tlv(pdev_dp, ppdu_info,
								  ppdu_desc, desc_cnt);
				goto next_ppdu;
			}

			ath12k_dp_rx_pktlog_process(pdev_dp, status_desc);

			if (unlikely(hal_status != HAL_RX_MON_STATUS_PPDU_DONE)) {
				ppdu_info->ppdu_continuation = true;
				page_frag_free(status_desc->mon_buf);
				mon_stats->status_buf_free++;
				continue;
			}

			filter_category =
				ppdu_info->userstats[ppdu_info->user_id].filter_category;
			if (filter_category == DP_MPDU_FILTER_CATEGORY_MO)
				goto free_buf;

			rcu_read_lock_bh();
			spin_lock_bh(&dp->dp_lock);

			if (!list_empty(&dp->neighbor_peers)) {
				list_for_each_entry_safe(nrp, tmp,
							 &dp->neighbor_peers, list) {
					u8 avg = nrp->avg_rssi;
					u8 snr = ppdu_info->rssi_comb;

					is_addr_equal =
					ether_addr_equal(nrp->addr,
							 ppdu_info->nrp_info.mac_addr2);
					if (filter_category ==
					    DP_MPDU_FILTER_CATEGORY_MD && is_addr_equal) {
						avg = ath12k_dp_get_avg_snr(snr, avg);
						nrp->rssi = ppdu_info->rssi_comb;
						nrp->avg_rssi = avg;
						nrp->timestamp =
							ktime_to_ms(ktime_get_real());
						goto unlock;
					}
				}
			}

			if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
				ath12k_dp_mon_rx_update_peer_su_stats(pdev_dp,
								      ppdu_info);
				ath12k_dp_mon_ppdu_rx_time_update(pdev_dp, ppdu_info, 0);
				ath12k_dp_mon_ppdu_rssi_update(pdev_dp, ppdu_info);
			} else if ((ppdu_info->fc_valid) &&
				   (ppdu_info->peer_id != HAL_INVALID_PEERID)) {
				ath12k_dp_mon_rx_process_ulofdma_stats(ppdu_info);
				ath12k_dp_mon_rx_update_peer_mu_stats(pdev_dp, ppdu_info);
				ath12k_dp_mon_ppdu_rx_time_update(pdev_dp, ppdu_info, 0);
				ath12k_dp_mon_ppdu_rssi_update(pdev_dp, ppdu_info);
			}
unlock:
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock_bh();
			ath12k_dp_rx_mon_ppdu_notify(dp, pdev_dp, ppdu_info);
free_buf:
			page_frag_free(status_desc->mon_buf);
			mon_stats->status_buf_free++;
		}
next_ppdu:
		mon_stats->num_ppdu_processed++;
		ppdu_desc_prcd++;
		ath12k_wifi8_dp_mon_rx_reset_ppdu_desc(ppdu_desc);
		ath12k_wifi8_dp_mon_rx_memset_ppdu_info(pdev_dp, ppdu_info);
	}

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	list_splice_tail_init(&dp_mon_pdev->ppdu_desc_proc_list,
			      &dp_mon_pdev->ppdu_desc_free_list);
	mon_stats->ppdu_desc_free += ppdu_desc_prcd;
	mon_stats->ppdu_desc_proc -= ppdu_desc_prcd;
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
}

int ath12k_wifi8_dp_mon_rx_dual_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
					     struct napi_struct *napi, int *budget)
{
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct hal_mon_dest_desc *mon_dst_desc, *next_desc;
	struct dp_srng *mon_dst_ring;
	struct hal_srng *srng;
	struct ath12k_dp_mon_desc *mon_desc;
	struct list_head *mon_desc_used_list = &dp_mon_pdev->mon_desc_used_list;
	struct ath12k_pdev_mon_dp_stats *mon_stats = &dp_mon_pdev->mon_stats;
	u64 desc_va;
	int num_buffs_reaped = 0, srng_id, ret;
	u32 end_offset, info0, end_reason;
	u8 pdev_idx = ath12k_hw_mac_id_to_pdev_id(ab->hw_params, pdev_dp->mac_id);
	u8 *mon_buf;

	srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, pdev_idx);
	mon_dst_ring = &pdev_dp->dp_mon_pdev->rxdma_mon_dst_ring[srng_id];

	srng = &ab->hal.srng_list[mon_dst_ring->ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (likely(*budget)) {
		mon_dst_desc = ath12k_hal_srng_dst_peek(ab, srng);
		if (unlikely(!mon_dst_desc))
			break;

		/* In case of empty descriptor, the cookie in the ring descriptor
		 * is invalid. Therefore, this entry is skipped, and ring processing
		 * continues.
		 */
		info0 = le32_to_cpu(mon_dst_desc->info0);
		if (u32_get_bits(info0, HAL_MON_DEST_INFO0_EMPTY_DESC)) {
			if (!list_empty(mon_desc_used_list)) {
				ath12k_wifi8_dp_mon_flush_used_list(pdev_dp,
								    mon_desc_used_list);
			} else {
				spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
				if (!list_empty(&dp_mon_pdev->ppdu_desc_used_list))
					ath12k_wifi8_dp_mon_rx_h_empty_desc(pdev_dp);
				spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
			}

			mon_stats->ring_desc_empty++;
			goto move_next;
		}

		desc_va = le64_to_cpu(mon_dst_desc->stat_buf_va);
		mon_desc = (struct ath12k_dp_mon_desc *)(uintptr_t)(desc_va);
		if (unlikely(!mon_desc)) {
			ath12k_warn(dp, "mon_dest: NULL mon_desc received in mac_id %d\n",
				    pdev_dp->mac_id);
			goto move_next;
		}

		mon_stats->status_buf_reaped++;
		if (unlikely(mon_desc->magic != ATH12K_MON_MAGIC_VALUE)) {
			ath12k_warn(dp, "mon_dest: invalid magic value in mac_id %d\n",
				    pdev_dp->mac_id);
			mon_stats->invalid_status_magic_num++;
			goto move_next;
		}

		if (unlikely(mon_desc->in_use != DP_MON_DESC_TO_HW)) {
			ath12k_warn(dp,
				    "mon_dest: invalid in_use=[%d] flag, mac_id %d\n",
				    mon_desc->in_use,
				    pdev_dp->mac_id);
			mon_stats->invalid_in_use++;
			goto move_next;
		}

		list_add_tail(&mon_desc->list, mon_desc_used_list);
		mon_buf = mon_desc->mon_buf;
		mon_desc->in_use = DP_MON_DESC_STATUS_REAP;
		if (unlikely(!mon_buf)) {
			ath12k_warn(dp, "mon_dest: NULL mon_buf received in mac_id %d\n",
				    pdev_dp->mac_id);
			goto move_next;
		}

		end_offset = u32_get_bits(info0, HAL_MON_DEST_INFO0_END_OFFSET);
		if (unlikely(end_offset > ATH12K_DP_MON_RX_BUF_SIZE))
			ath12k_warn(dp,
				    "mon_dest: invalid offset %u received in mac_id %d\n",
				    end_offset, pdev_dp->mac_id);

		if (unlikely(end_offset > ATH12K_DP_MON_RX_BUF_SIZE)) {
			end_offset = ATH12K_DP_MON_RX_BUF_SIZE - 1;
			mon_stats->invalid_end_offset++;
		}

		/* The hardware reports the buffer length as (actual_length - 1),
		 * likely due to internal indexing or alignment constraints.
		 * To obtain the true buffer length for processing, increment
		 * the reported end_offset by 1 before using it.
		 */
		mon_desc->buf_len = end_offset + 1;
		end_reason = u32_get_bits(info0, HAL_MON_DEST_INFO0_END_REASON);

		/* HAL_MON_FLUSH_DETECTED implies that an rx flush received at the end of
		 * rx PPDU and HAL_MON_PPDU_TRUNCATED implies that the PPDU got
		 * truncated due to a system level error. In both the cases, buffer data
		 * can be discarded
		 */
		if (end_reason == HAL_MON_FLUSH_DETECTED ||
		    end_reason == HAL_MON_PPDU_TRUNCATED) {
			if (end_reason == HAL_MON_FLUSH_DETECTED)
				mon_stats->ring_desc_flush++;
			else
				mon_stats->ring_desc_trunc++;

			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "mon_dest: descriptor end reason %d mac_id %d",
				   end_reason, pdev_dp->mac_id);
			ath12k_wifi8_dp_mon_flush_used_list(pdev_dp, mon_desc_used_list);
			goto move_next;
		}

		/* Calculate the budget when the ring descriptor with the
		 * HAL_MON_END_OF_PPDU to ensure that one PPDU worth of data is always
		 * reaped. This helps to efficiently utilize the NAPI budget.
		 */
		if (end_reason == HAL_MON_END_OF_PPDU) {
			mon_desc->end_of_ppdu = true;
			mon_stats->num_ppdu_reaped++;
			next_desc = ath12k_hal_srng_dst_next_peek_nolock(srng);
			if (!next_desc ||
			    !u32_get_bits(le32_to_cpu(next_desc->info0),
					  HAL_MON_DEST_INFO0_EMPTY_DESC)) {
				*budget -= 1;
				ret = ath12k_wifi8_dp_mon_rx_add_ppdu_desc(
						mon_desc_used_list, dp_mon_pdev);
				if (unlikely(ret)) {
					ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
						   "mon_dest: Failed to add mon desc to ppdu ret %d",
						   ret);
					ath12k_wifi8_dp_mon_flush_used_list(
							pdev_dp, mon_desc_used_list);
				}
			}
		}

move_next:
		ath12k_hal_srng_dst_get_next_entry(ab, srng);
		num_buffs_reaped++;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	if (!num_buffs_reaped)
		return 0;

	return num_buffs_reaped;
}

int ath12k_wifi8_dp_mon_rx_dual_ring_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_base *ab = dp->ab;
	u32 mon_status_buf = ATH12K_DP_MON_STATUS_BUF;
	int i, ret;

	spin_lock_init(&dp_mon_pdev->ppdu_desc_lock);
	dp_mon_pdev->ppdu_desc_pool = kcalloc(dp_mon->mon_num_ppdu_desc,
					      sizeof(*dp_mon_pdev->ppdu_desc_pool),
					      GFP_KERNEL);
	if (unlikely(!dp_mon_pdev->ppdu_desc_pool)) {
		ath12k_warn(dp_pdev->dp, "Failed to allocate monitor PPDU desc pool\n");
		return -ENOMEM;
	}

	for (i = 0; i < dp_mon->mon_num_ppdu_desc; i++) {
		dp_mon_pdev->ppdu_desc_pool[i] =
				kzalloc(sizeof(*dp_mon_pdev->ppdu_desc_pool[i]),
					GFP_KERNEL);
		if (!dp_mon_pdev->ppdu_desc_pool[i]) {
			ath12k_warn(dp_pdev->dp,
				    "Failed to allocate monitor PPDU desc pool\n");
			ret = -ENOMEM;
			goto fail;
		}
		/* Allocate status_desc array for each RX PPDU descriptor.
		 * The RX path (ath12k_wifi8_dp_mon_rx_add_ppdu_desc) directly accesses
		 * ppdu_desc->status_desc[desc_cnt], so it must be allocated here.
		 */
		dp_mon_pdev->ppdu_desc_pool[i]->status_desc =
			kcalloc(mon_status_buf,
				sizeof(struct ath12k_dp_mon_status_desc),
				GFP_KERNEL);
		if (!dp_mon_pdev->ppdu_desc_pool[i]->status_desc) {
			ath12k_warn(dp_pdev->dp,
				    "Failed to allocate status_desc for RX PPDU desc %d\n",
				    i);
			ret = -ENOMEM;
			goto fail;
		}
	}

	INIT_LIST_HEAD(&dp_mon_pdev->ppdu_desc_free_list);
	INIT_LIST_HEAD(&dp_mon_pdev->ppdu_desc_used_list);
	INIT_LIST_HEAD(&dp_mon_pdev->ppdu_desc_proc_list);

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	for (i = 0; i < dp_mon->mon_num_ppdu_desc; i++) {
		INIT_LIST_HEAD(&dp_mon_pdev->ppdu_desc_pool[i]->list);
		list_add_tail(&dp_mon_pdev->ppdu_desc_pool[i]->list,
			      &dp_mon_pdev->ppdu_desc_free_list);
		dp_mon_pdev->mon_stats.ppdu_desc_free++;
	}
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);

	return 0;
fail:
	ath12k_wifi8_dp_mon_rx_dual_ring_cleanup_ppdu_desc(dp_pdev);
	return ret;
}

void ath12k_wifi8_dp_mon_rx_dual_ring_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	if (!dp_mon_pdev->ppdu_desc_pool) {
		spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
		return;
	}

	for (i = 0; i < dp_mon->mon_num_ppdu_desc; i++) {
		if (dp_mon_pdev->ppdu_desc_pool[i]) {
		/* Free status_desc arrays allocated for each RX PPDU descriptor */
			kfree(dp_mon_pdev->ppdu_desc_pool[i]->status_desc);
			dp_mon_pdev->ppdu_desc_pool[i]->status_desc = NULL;
			kfree(dp_mon_pdev->ppdu_desc_pool[i]);
		}
		dp_mon_pdev->ppdu_desc_pool[i] = NULL;
	}

	kfree(dp_mon_pdev->ppdu_desc_pool);
	dp_mon_pdev->ppdu_desc_pool = NULL;
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
}

int ath12k_wifi8_dp_mon_rx_wq_init(struct ath12k_pdev_dp *dp_pdev)
{
	return ath12k_dp_mon_rx_wq_init_common(dp_pdev,
					       ath12k_wifi8_dp_mon_rx_process_ppdu);
}

static void ath12k_wifi8_dp_mon_rx_drain_wq(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_mon_ppdu_desc *ppdu_desc;
	struct ath12k_dp_mon_status_desc *status_desc;
	struct list_head local_list;
	int desc_cnt;

	INIT_LIST_HEAD(&local_list);

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	list_splice_init(&dp_mon_pdev->ppdu_desc_used_list, &local_list);
	list_splice_init(&dp_mon_pdev->ppdu_desc_proc_list, &local_list);
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);

	list_for_each_entry(ppdu_desc, &local_list, list) {
		for (desc_cnt = 0; desc_cnt < ppdu_desc->status_desc_cnt; desc_cnt++) {
			status_desc = &ppdu_desc->status_desc[desc_cnt];
			if (!status_desc->mon_buf)
				continue;
			ath12k_core_dma_unmap_page(dp->dev, status_desc->paddr,
						   ATH12K_DP_MON_RX_BUF_SIZE,
						   DMA_FROM_DEVICE);
			ath12k_wifi8_dp_mon_h_flush_tlv(dp_pdev, status_desc);
		}
		ath12k_dp_mon_reset_ppdu_desc(ppdu_desc);
	}

	spin_lock_bh(&dp_mon_pdev->ppdu_desc_lock);
	list_splice_init(&local_list, &dp_mon_pdev->ppdu_desc_free_list);
	spin_unlock_bh(&dp_mon_pdev->ppdu_desc_lock);
}

void ath12k_wifi8_dp_mon_rx_wq_deinit(struct ath12k_pdev_dp *dp_pdev, bool destroy)
{
	struct ath12k_pdev_mon_dp *mon_pdev = dp_pdev->dp_mon_pdev;

	cancel_work_sync(&mon_pdev->rxmon_work);

	ath12k_wifi8_dp_mon_rx_drain_wq(dp_pdev);

	flush_workqueue(mon_pdev->rxmon_wq);

	if (destroy)
		destroy_workqueue(mon_pdev->rxmon_wq);
}

static int
ath12k_wifi8_ext_mon_validate_pkt_data_mpdu_tlv(struct ath12k_dp *dp,
						const struct
						ath12k_ext_mon_pkt_config *pkt)
{
	const struct {
		u32 filter_bit;
		u8  mask;
	} subtypes[] = {
		{ FILTER_DATA_MCAST, pkt->data_mpdu_tlv.mcast       },
		{ FILTER_DATA_UCAST, pkt->data_mpdu_tlv.ucast       },
		{ FILTER_DATA_NULL,  pkt->data_mpdu_tlv.null_frm    },
	};
	u32 data_filter = pkt->filter[ATH12K_EXT_MON_FRAME_DATA];
	int i;

	for (i = 0; i < ARRAY_SIZE(subtypes); i++) {
		bool in_filter = !!(data_filter & subtypes[i].filter_bit);

		/* in_filter: subtype for which tlv subscription is configured is also
		 * subscribed for Packet Delivery.
		 * If subtype is in_filter: only mask 0xD (mpdu_start+mpdu_end+rx_header)
		 * or 0xF (all four TLVs) allowed; as the other MPDU TLVs are needed
		 * for packet construction.
		 */
		if (in_filter &&
		    subtypes[i].mask != 0xD && subtypes[i].mask != 0xF) {
			ath12k_warn(dp, "only mask 0xD/0xF allowed for mpdu_tlv stype %d",
				    i);
			return -EINVAL;
		}
		/* If subtype is not in_filter: BIT(3) (rx_header) must be unset.
		 * As subtype is not subscribed for packet deliver, Rx Header is not
		 * needed.
		 */
		if (!in_filter && (subtypes[i].mask & BIT(3))) {
			ath12k_warn(dp, "rx_hdr not allowed for mpdu_tlv stype %d", i);
			return -EINVAL;
		}
	}

	return 0;
}

int ath12k_wifi8_dp_ext_mon_validate_request(struct ath12k_pdev_dp *dp_pdev,
					     const struct ath12k_ext_mon_config *req)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	const struct ath12k_ext_mon_peer_info *peer = NULL;
	int i, ret;

	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	if (!dp_mon_pdev) {
		ath12k_warn(dp_pdev->dp, "monitor pdev is null");
		return -EINVAL;
	}

	if (req->cmd_type == ATH12K_EXT_MON_CMD_TYPE_SET_PEER) {
		if (req->peer.action == ATH12K_EXT_MON_PEER_ACTION_ADD) {
			for (i = 0; i < req->peer.count; i++) {
				peer = &req->peer.peer_info[i];
				/*
				 * For wifi8, ra_addr is not supported. Reject any
				 * request with ra_addr set or a non-default bitmap.
				 */
				if (peer->ra_addr || peer->ta_info.bitmap !=
						ATH12K_EXT_MON_DEFAULT_PEER_BITMAP) {
					ath12k_warn(dp_pdev->dp,
						    "invalid ra_addr: %d or bitmap: %02x",
						    peer->ra_addr, peer->ta_info.bitmap);
					return -EINVAL;
				}
			}
		}
	}

	if (req->cmd_type == ATH12K_EXT_MON_CMD_TYPE_SET_FILTER) {
		if (req->filter.all_peer.data_mpdu_tlv.tlv_configured ||
		    req->filter.target_peer.data_mpdu_tlv.tlv_configured) {
			if (req->direction == QCA_VENDOR_EXT_MON_DIRECTION_TX) {
				ath12k_warn(dp_pdev->dp,
					    "data MPDU TLV filter not supported for TX");
				return -EINVAL;
			}

			if (req->filter.all_peer.data_mpdu_tlv.tlv_configured) {
				ret = ath12k_wifi8_ext_mon_validate_pkt_data_mpdu_tlv(
						dp_pdev->dp, &req->filter.all_peer);
				if (ret)
					return ret;
			}
			if (req->filter.target_peer.data_mpdu_tlv.tlv_configured) {
				ret = ath12k_wifi8_ext_mon_validate_pkt_data_mpdu_tlv(
						dp_pdev->dp, &req->filter.target_peer);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

void ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask(
		struct htt_tx_mon_ring_selection_cfg_cmd *cmd,
		const struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	/*word 13*/
	cmd->tlv_word_mask_in6 |=
		le32_encode_bits(htt_tlv_filter->wmask.mactx_user_desc_common,
				 HTT_TX_MON_WMASK_IN6_MACTX_USR_DESC_CMN_MASK);
	/*word 14*/
	cmd->tlv_word_mask_in7 |=
		le32_encode_bits(htt_tlv_filter->wmask.rx_resp_required_info,
				 HTT_TX_MON_WMASK_IN7_RX_RESP_REQD_INFO_MASK);
	cmd->tlv_filter_mask_in4 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_upstream_tlv_flags3,
				 HTT_TX_FILTER_MASK_IN4_UPSTREAM_TLV_FLAGS3) |
		le32_encode_bits(htt_tlv_filter->tx_mon_downstream_tlv_flags1,
				 HTT_TX_FILTER_MASK_IN4_DOWNSTREAM_TLV_FLAGS1);
}

void
ath12k_wifi8_dp_ext_mon_setup_rx_filter(struct htt_rx_ring_tlv_filter *tlv_filter,
					const struct ath12k_dp_rx_ext_mon *rx_ext_mon)
{
	const struct ath12k_ext_mon_data_mpdu_tlv_config *fp_mpdu_tlv =
							&rx_ext_mon->fp.data_mpdu_tlv;
	const struct ath12k_ext_mon_data_mpdu_tlv_config *fpmo_mpdu_tlv =
							&rx_ext_mon->fpmo.data_mpdu_tlv;
	u8 hdr_bits;

	tlv_filter->rx_mon_fp_mgmt_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_ctrl_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fp_data_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fpmo_mgmt_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fpmo_ctrl_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_fpmo_data_hdrlen = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_mgmt_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_ctrl_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_mo_data_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_md_mgmt_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_md_ctrl_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;
	tlv_filter->rx_mon_md_data_hdrlen   = HTT_RX_HDR_LEN_64_BYTES;

	if (rx_ext_mon->fp_enabled) {
		if (rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_MGMT] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fp_mgmt_hdrlen =
				rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_MGMT];
		if (rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_CTRL] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fp_ctrl_hdrlen =
				rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_CTRL];
		if (rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_DATA] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fp_data_hdrlen =
				rx_ext_mon->fp.len[ATH12K_EXT_MON_FRAME_DATA];
	}

	if (rx_ext_mon->fpmo_enabled) {
		if (rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_MGMT] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fpmo_mgmt_hdrlen =
				rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_MGMT];
		if (rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_CTRL] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fpmo_ctrl_hdrlen =
				rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_CTRL];
		if (rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_DATA] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_fpmo_data_hdrlen =
				rx_ext_mon->fpmo.len[ATH12K_EXT_MON_FRAME_DATA];
	}

	if (rx_ext_mon->mo_enabled) {
		if (rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_MGMT] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_mo_mgmt_hdrlen =
				rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_MGMT];
		if (rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_CTRL] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_mo_ctrl_hdrlen =
				rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_CTRL];
		if (rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_DATA] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_mo_data_hdrlen =
				rx_ext_mon->mo.len[ATH12K_EXT_MON_FRAME_DATA];
	}

	if (rx_ext_mon->md_enabled) {
		if (rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_MGMT] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_md_mgmt_hdrlen =
				rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_MGMT];
		if (rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_CTRL] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_md_ctrl_hdrlen =
				rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_CTRL];
		if (rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_DATA] !=
				ATH12K_EXT_MON_LEN_FULL_PKT)
			tlv_filter->rx_mon_md_data_hdrlen =
				rx_ext_mon->md.len[ATH12K_EXT_MON_FRAME_DATA];
	}

	if (rx_ext_mon->level == ATH12K_EXT_MON_FILTER_LEVEL_PPDU)
		tlv_filter->rx_mon_enable_hdr_per_ppdu = 1;

	/* Data MPDU TLV filtering is supported only for FP and FPMO modes;
	 * MO and MD modes have no corresponding hardware filter register.
	 *
	 * filter0 (30 bits): packs 5 data subtypes at 6 bits each:
	 *   bit 0 = rx_mpdu_start
	 *   bit 1 = rx_msdu_end
	 *   bit 2 = rx_mpdu_end
	 *   bit 3 = rx_header
	 *   bit 4 = rx_header_per_msdu
	 *   bit 5 = rx_header_per_ppdu
	 * Bit layout in the 30-bit word matches RXDMA
	 * FP_DATA_MPDU_TLV_FILTER_IN_CONTROL:
	 *   [5:0]   = mcast
	 *   [11:6]  = ucast
	 *   [17:12] = null
	 *   [23:18] = qos_null
	 *   [29:24] = qos_null_tb
	 *
	 * filter1: NDP subtype TLV bitmask (same 6-bit layout as above).
	 * Both FP and FP_MO NDP masks are packed into the same HTT info5
	 * word via HTT_RX_RING_SEL_CFG_CMD_INFO5_FP_DATA_MPDU_TLV_FILTER_1
	 * (GENMASK(5,0)) and
	 * HTT_RX_RING_SEL_CFG_CMD_INFO5_FPMO_DATA_MPDU_TLV_FILTER_1
	 * (GENMASK(11,6)).
	 *
	 * Bits 4 and 5 are only set for slots that are already active (non-zero
	 * lower 4 bits). A zero slot means the subtype is not subscribed, so
	 * per-msdu/per-ppdu header generation for it serves no purpose.
	 *
	 * Rx Monitor Stats do not subscribe for QoS_Null and QoS_Null_TB.
	 * As enabling this for any data subtype enables the FP FPMO Data MPDU TLV
	 * for all FP/FPMO data subtypes subscription, we need to subscribe for all
	 * TLVs for these subtypes as they will be needed if QoS_Null and
	 * QoS_Null_TB are enabled for packet delivery.
	 */
	hdr_bits = (tlv_filter->rx_filter &
		    HTT_RX_FILTER_TLV_FLAGS_PER_MSDU_HEADER) ? 0x10 : 0;
	hdr_bits |= (tlv_filter->rx_mon_enable_hdr_per_ppdu) ? 0x20 : 0;

	if (fp_mpdu_tlv->tlv_configured || fpmo_mpdu_tlv->tlv_configured) {
		tlv_filter->fp_data_mpdu_tlv_filter0 =
			HTT_FP_DATA_TLV_SUBTYPE(fp_mpdu_tlv->mcast, 0, hdr_bits) |
			HTT_FP_DATA_TLV_SUBTYPE(fp_mpdu_tlv->ucast, 6, hdr_bits) |
			HTT_FP_DATA_TLV_SUBTYPE(fp_mpdu_tlv->null_frm, 12, hdr_bits) |
			(0xFu | hdr_bits) << 18 |  /* qos_null */
			(0xFu | hdr_bits) << 24;   /* qos_null_tb */
		tlv_filter->fp_data_mpdu_tlv_filter1 = 0xFu | hdr_bits; /* ndp */

		tlv_filter->fpmo_data_mpdu_tlv_filter0 =
			HTT_FP_DATA_TLV_SUBTYPE(fpmo_mpdu_tlv->mcast, 0, hdr_bits) |
			HTT_FP_DATA_TLV_SUBTYPE(fpmo_mpdu_tlv->ucast, 6, hdr_bits) |
			HTT_FP_DATA_TLV_SUBTYPE(fpmo_mpdu_tlv->null_frm, 12, hdr_bits) |
			(0xFu | hdr_bits) << 18 |  /* qos_null */
			(0xFu | hdr_bits) << 24;   /* qos_null_tb */
		tlv_filter->fpmo_data_mpdu_tlv_filter1 = 0xFu | hdr_bits; /* ndp */

		tlv_filter->fp_fpmo_data_mpdu_filter_in_en = 1;
	}
}
