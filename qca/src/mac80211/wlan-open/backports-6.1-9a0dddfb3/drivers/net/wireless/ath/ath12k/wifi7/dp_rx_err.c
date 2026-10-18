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
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../debugfs.h"
#include "../dp.h"
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */

static
void ath12k_wifi7_convert_n_deliver_nw_frame(struct ath12k_pdev_dp *dp_pdev,
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
		status->link_valid = 1;
		status->link_id =
			ath12k_dp_peer_convert_hw_to_logical_link_id(
							peer,
							rx_spd->reo.src_link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi7_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
				  HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return;
	}

	ath12k_wifi7_dp_rx_h_undecap_eth(dp_pdev, msdu, peer->sec_type,
					 status,
					 tlv_info->mesh_ctrl_present,
					 (struct hal_rx_desc *)rx_tlv_hdr,
					 rx_msdu_info->da_is_mcbc,
					 rx_mpdu_info->tid);

	ath12k_dp_rx_update_eapol_stats(dp_pdev->dp, msdu);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
}

static void ath12k_wifi7_wbm_process_frame(struct ath12k_pdev_dp *dp_pdev,
					   struct hal_rx_spd_data *spd_desc_l,
					   struct ath12k_dp_peer *peer,
					   struct ieee80211_rx_status *rx_status,
					   struct napi_struct *napi,
					   struct rx_tlv_info_1 *prev_tlv_info)
{
	struct link_peer_rx_tid_stats stats;

	/* Mark the error ring_id (Last ring is considered error ring) */
	spd_desc_l->reo.ring_id = DP_REO_DST_RING_MAX - 1;

	switch (spd_desc_l->tlv_info.decap) {
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_wifi7_deliver_nwifi_frame(dp_pdev, spd_desc_l,
						 peer, rx_status,
						 napi, &stats,
						 prev_tlv_info);
		break;
	case DP_RX_DECAP_TYPE_RAW:
		ath12k_wifi7_deliver_raw_frame(dp_pdev, spd_desc_l,
					       peer, rx_status,
					       napi, &stats,
					       prev_tlv_info);
		break;
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		ath12k_wifi7_deliver_ethernet_frame(dp_pdev, spd_desc_l,
						    peer, rx_status,
						    napi, &stats,
						    prev_tlv_info);
		break;
	}
}

static bool ath12k_wifi7_dp_tkip_mic_err(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_peer *peer,
					 struct ieee80211_rx_status *rx_status,
					 struct hal_rx_spd_data *spd_desc_l,
					 struct napi_struct *napi,
					 struct rx_tlv_info_1 *prev_tlv_info)
{
	struct link_peer_rx_tid_stats stats;
	int ret;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, rx_status, &spd_desc_l->tlv_info,
					HAL_WBM_REL_SRC_MODULE_RXDMA);
	if (unlikely(ret))
		return true;

	rx_status->flag |= RX_FLAG_MMIC_ERROR;

	ath12k_wifi7_deliver_raw_frame(dp_pdev, spd_desc_l,
				       peer, rx_status,
				       napi, &stats,
				       prev_tlv_info);

	return false;
}

static bool ath12k_wifi7_handle_reo_route(struct ath12k_pdev_dp *dp_pdev,
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
			ath12k_wifi7_convert_n_deliver_nw_frame(dp_pdev, spd_desc_l,
								peer, rx_status,
								napi,
								prev_tlv_info);
			break;
		}
		fallthrough;
	case DP_RX_DECAP_TYPE_RAW:
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_dp_rx_update_eapol_stats(dp_pdev->dp, spd_desc_l->msdu);
		ath12k_wifi7_wbm_process_frame(dp_pdev, spd_desc_l,
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

static bool ath12k_wifi7_handle_null_queue(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_peer *peer,
					   struct ieee80211_rx_status *rx_status,
					   struct hal_rx_spd_data *spd_desc_l,
					   struct napi_struct *napi,
					   struct rx_tlv_info_1 *prev_tlv_info)

{
	struct rx_msdu_desc_info *rx_msdu_info = &spd_desc_l->rx_msdu_info;
	bool is_mcbc = rx_msdu_info->da_is_mcbc;
	bool ra_is_mcbc = is_mcbc;
	bool is_4addr_sta;
	bool to_ds = rx_msdu_info->to_ds;
	bool fr_ds = rx_msdu_info->fr_ds;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_vif *ahvif;
#endif /* CPTCFG_QCN_EXTN */
	bool allow_3addr_mc = false;

	switch (spd_desc_l->tlv_info.decap) {
	case DP_RX_DECAP_TYPE_ETHERNET2_DIX:
		if (!peer)
			return true;
#ifdef CPTCFG_QCN_EXTN
		if (peer->vif) {
			ahvif = ath12k_vif_to_ahvif(peer->vif);
			dp_vif = &ahvif->dp_vif;
			allow_3addr_mc = dp_vif->dp_extn.allow_3addr_mc;
		}
#endif
		is_4addr_sta = peer->vdev_type_4addr & BIT(NL80211_IFTYPE_STATION);
		if (is_4addr_sta && is_mcbc && !to_ds && !allow_3addr_mc)
			return true;

		if (peer)
			ra_is_mcbc = is_mcbc && !peer->is_reset_mcbc;

		if ((fr_ds && to_ds && peer && !peer->use_4addr) || ra_is_mcbc) {
			ath12k_wifi7_convert_n_deliver_nw_frame(dp_pdev,
								spd_desc_l,
								peer, rx_status,
								napi,
								prev_tlv_info);
			break;
		}
		fallthrough;
	case DP_RX_DECAP_TYPE_RAW:
	case DP_RX_DECAP_TYPE_NATIVE_WIFI:
		ath12k_wifi7_wbm_process_frame(dp_pdev, spd_desc_l,
					       peer, rx_status,
					       napi,
					       prev_tlv_info);
		break;
	default:
		return true;
	}

	return false;
}

static bool ath12k_wifi7_dp_unauth_wds_err(struct ath12k_pdev_dp *dp_pdev,
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

		ath12k_wifi7_convert_n_deliver_nw_frame(dp_pdev, spd_desc_l,
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

	ath12k_wifi7_wbm_process_frame(dp_pdev, spd_desc_l,
				       peer, rx_status,
				       napi,
				       prev_tlv_info);

	return false;
}

static void
ath12k_wifi7_dp_process_wbm_rx_packets(struct ath12k_dp *dp,
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
		enum hal_wbm_rel_src_module src =
			spd_desc_l->wbm.release_source_module;
		bool stats_needed = false;

		rx_msdu_info = &spd_desc_l->rx_msdu_info;
		rx_mpdu_info = &spd_desc_l->rx_mpdu_info;

		fse_tagged = false;

		drop = ath12k_dp_err_drop_needed(src,
						 spd_desc_l->wbm.reo_push_reason,
						 spd_desc_l->wbm.reo_error_code,
						 spd_desc_l->wbm.rxdma_push_reason,
						 spd_desc_l->wbm.rxdma_error_code);

		if (!drop)
			ath12k_wifi7_dp_adjust_skb(spd_desc_l, NULL,
						   &msdu_idx, hal_rx_desc_sz);

		rx_desc = (struct hal_rx_desc *)spd_desc_l->vaddr;
		ath12k_wifi7_dp_extract_rx_spd_data(hal, spd_desc_l, rx_desc);
		hw_link_id = ath12k_wifi7_dp_rx_get_msdu_src_link(dp, rx_desc);

		if (drop) {
			dev_kfree_skb_any(spd_desc_l->msdu);
			spd_desc_l->msdu = NULL;
		}

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
		peer_id = ath12k_wifi7_dp_rx_get_peer_id(dp->ab, dp->peer_metadata_ver,
							 peer_metadata);
		tid = rx_mpdu_info->tid;
		msdu_len = rx_msdu_info->msdu_length;

		peer = ath12k_dp_peer_find_by_peerid_index(partner_dp,
							   dp_pdev, peer_id);
		if (peer)
			ahvif = ath12k_vif_to_ahvif(peer->vif);

		hw_link_id = ath12k_dp_validate_hw_link_id(hw_link_id);
		spd_desc_l->reo.src_link_id = hw_link_id;

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

		if (spd_desc_l->wbm.release_source_module ==
				HAL_WBM_REL_SRC_MODULE_REO) {
			if (spd_desc_l->wbm.reo_push_reason ==
				HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION) {
				dp->device_stats.wbm_err.hal_reo_route++;
				fse_tagged = !spd_desc_l->rx_mpdu_info.flow_idx_invalid &&
					     !spd_desc_l->rx_mpdu_info.flow_idx_timeout;
				drop = ath12k_wifi7_handle_reo_route(dp_pdev, peer,
								     &rx_status,
								     spd_desc_l,
								     napi,
								     &prev_tlv);
				if (drop)
					drop_reason = ATH_RX_INVALID_RBM;
			} else if (spd_desc_l->wbm.reo_push_reason ==
					HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED) {
				/* flow_idx is always invalid on REO error frames;
				 * fse_tagged stays false (initialized at loop top)
				 */
				error_code = spd_desc_l->wbm.reo_error_code;
				if (error_code ==
					HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO) {
					drop = ath12k_wifi7_handle_null_queue(dp_pdev,
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
		} else if (spd_desc_l->wbm.release_source_module ==
				HAL_WBM_REL_SRC_MODULE_RXDMA) {
			drop = false;

			if (spd_desc_l->wbm.rxdma_push_reason !=
				HAL_RXDMA_PUSH_REASON_ERR_DETECTED) {
				fse_tagged = !spd_desc_l->rx_mpdu_info.flow_idx_invalid &&
					     !spd_desc_l->rx_mpdu_info.flow_idx_timeout;
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

			error_code = spd_desc_l->wbm.rxdma_error_code;

			switch (error_code) {
			case HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR:
				drop = ath12k_wifi7_dp_unauth_wds_err(dp_pdev,
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
				drop_reason = ATH_RX_RXDMA_ERR;
				break;
			case HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR:
				dp_pdev->stats.telemetry_stats.rx_mic_err++;
				drop = ath12k_wifi7_dp_tkip_mic_err(dp_pdev,
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

static bool check_sg_termination(struct hal_srng *srng,
				 int valid_entries)
{
	struct hal_wbm_release_ring_cc_rx *desc;
	struct rx_msdu_desc *msdu_info;

	if (valid_entries >= 9)
		return true;

	if (!valid_entries)
		return false;

	desc = (struct hal_wbm_release_ring_cc_rx *)
		ath12k_hal_srng_fetch_entry(srng,
					    valid_entries - 1);
	msdu_info = &desc->rx_msdu_info;
	return !(le32_to_cpu(msdu_info->info0) &
			RX_MSDU_DESC_INFO0_MSDU_CONTINUATION);
}

int ath12k_wifi7_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct hal_srng *srng = NULL;
	struct hal_rx_spd_data *rx_status_desc = NULL;
	struct hal_rx_spd_data *tmp_spd = NULL;
	struct hal_wbm_release_ring *hw_rx_desc = NULL;
	struct hal_wbm_release_ring *next_hw_rx_desc = NULL;
	struct hal_wbm_release_ring *pf_next_hw_rx_desc = NULL;
	int rx_status_idx = smp_processor_id() + DP_REO_DST_RING_MAX;
	u8 device_id;
	u16 total_rx_reaped = 0;
	u16 num_rx_reaped = 0;
	u16 valid_entries = 0;
	u32 curr_tp = 0;
	bool first_sg_frame = true;
	struct ath12k_rx_desc_info *sw_rx_desc = NULL;
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	int num_rx_reaped_per_device[ATH12K_MAX_SOCS] = {};
	struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
	struct hal_srng *refill_srng;
	struct ath12k_dp *partner_dp;

	srng = &dp->hal->srng_list[dp->rx_rel_ring.ring_id];

	__ath12k_hal_srng_access_begin(srng);

	valid_entries = __ath12k_hal_srng_dst_num_available_to_reap(srng,
								    false);
	if (unlikely(!valid_entries)) {
		__ath12k_hal_srng_access_end(ab, srng);
		return 0;
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

		rx_spd->flags = 0;
		hw_rx_desc =
			__ath12k_hal_get_dst_srng_desc(srng, &curr_tp,
						       (void **)&next_hw_rx_desc);

		/* this check is redundant */
		if (unlikely(!hw_rx_desc))
			pr_err("HW bug: NULL entry in ring, invalid entry");

		sw_rx_desc = ath12k_wifi7_get_sw_desc_from_wbm_hw_desc(hw_rx_desc);

		ath12k_wifi7_rx_sw_desc_sanity_check(sw_rx_desc);

		ath12k_wifi7_cpy_hw_wbm_rx_desc_to_spad_desc(hw_rx_desc,
							     rx_spd);

		rx_spd->rx_mpdu_info.fragment_flag = sw_rx_desc->is_frag;
		sw_rx_desc->is_frag = 0;

		if (pf_next_hw_rx_desc)
			ath12k_wifi7_pretech_next_sw_desc_wbm(next_hw_rx_desc);

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
				if (!check_sg_termination(srng,
							  valid_entries)) {
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

		IPA_SET_RX_BUF_SMMU_UNMAP(dp->ab, rx_spd->msdu, true);

		sw_rx_desc->in_use = 0;
		num_rx_reaped_per_device[device_id]++;
		num_rx_reaped++;
		list_add_tail(&sw_rx_desc->list, &rx_desc_used_list[device_id]);

	}
	ath12k_dsb();

	__ath12k_hal_srng_access_end(ab, srng);

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

	ath12k_wifi7_dp_process_wbm_rx_packets(dp, napi, rx_status_desc,
					       total_rx_reaped);

	return total_rx_reaped;
}
