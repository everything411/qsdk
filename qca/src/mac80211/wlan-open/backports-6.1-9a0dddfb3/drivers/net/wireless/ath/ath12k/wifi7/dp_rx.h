/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_DP_RX_WIFI7_H
#define ATH12K_DP_RX_WIFI7_H

#include "../core.h"
#include "../dp_rx.h"
#include "hal_desc.h"
#include "hal.h"
#include "../fse.h"
#include "../debug.h"

#define MAX_TP_TIDS	8

#ifndef PLATFORM_SDX
#define VIRT_TO_PHYS(defrag_skb, buf_paddr) \
({ \
	(buf_paddr) = (dma_addr_t)virt_to_phys((defrag_skb)->data); \
})

#define ATH12K_CORE_DMAC_INV_RANGE(desc_info) \
	ath12k_core_dmac_inv_range((desc_info)->vaddr, \
				   (desc_info)->vaddr + DP_RX_BUFFER_SIZE)

#define RETURN_IPA_CODE(...) ((void)0)

#define ATH12K_DP_RXDMA_RING_CONFIG(ring_id, dp) \
({ \
	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id; \
})

#define IPA_SET_RX_BUF_SMMU_MAP(...) ((void)0)
#define IPA_SET_RX_BUF_SMMU_UNMAP(...) ((void)0)
#define ATH12K_IPA_DMA_MAP_SINGLE(...) ((void)0)
#define ATH12K_IPA_DMA_UNMAP_SINGLE(...) ((void)0)

#else /* PLATFORM_SDX */
#define VIRT_TO_PHYS(...) ((void)0)
#define ATH12K_CORE_DMAC_INV_RANGE(...) ((void)0)
#define RETURN_IPA_CODE(...) ((void)0)
#define ATH12K_DP_RXDMA_RING_CONFIG(ring_id, dp) \
({ \
	(ring_id) = (dp)->rx_refill_buf_ring.refill_buf_ring.ring_id; \
})
#define IPA_SET_RX_BUF_SMMU_MAP(...) ((void)0)
#define IPA_SET_RX_BUF_SMMU_UNMAP(...) ((void)0)
#define ATH12K_IPA_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr) do { \
	__typeof__(ab) _ab = (ab); \
	__typeof__(defrag_skb) _skb = (defrag_skb); \
	(buf_paddr) = dma_map_single((_ab)->dev, (_skb)->data, \
				     DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE); \
	if (dma_mapping_error((_ab)->dev, (buf_paddr))) \
		(buf_paddr) = 0; \
} while (0)
#define ATH12K_IPA_DMA_UNMAP_SINGLE(partner_dp, desc_info) \
	ath12k_core_dma_unmap_single((partner_dp)->dev, (desc_info)->paddr, \
				     DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE)
#endif


struct dp_rx_fse {
	struct hal_rx_fse *hal_fse;
	u32 flow_hash;
	u32 flow_id;
	u8 reo_indication;
	bool is_valid;
};

bool ath12k_dp_rx_check_nwifi_hdr_len_valid(struct ath12k_dp *dp,
					    u8 decap_type,
					    struct sk_buff *msdu);
int ath12k_wifi7_dp_reo_cmd_send(struct ath12k_base *ab,
				 void *data, size_t len,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    struct hal_reo_status *status));
int ath12k_wifi7_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget);
int ath12k_wifi7_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget);
int ath12k_wifi7_dp_rx_process(struct ath12k_dp *dp, int mac_id,
			       struct napi_struct *napi,
			       int budget);
void ath12k_wifi7_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid);
bool ath12k_wifi7_dp_mlo_peer_tid_teardown_ready(struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *link_peer);
bool ath12k_wifi7_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src);
int ath12k_wifi7_dp_reo_cache_flush(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid);
int ath12k_wifi7_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn);
void ath12k_wifi7_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id,
					 u16 tid, dma_addr_t paddr);
int ath12k_wifi7_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action);
void ath12k_wifi7_dp_rx_process_reo_status(struct ath12k_dp *dp);

void ath12k_wifi7_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					    struct ath12k_dp_rx_tid *rx_tid,
					    u32 cipher, enum set_key_cmd key_cmd);
int ath12k_wifi7_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned,
				    u16 stats_id);
int ath12k_wifi7_dp_rxdma_ring_sel_config_qcn9274(struct ath12k_base *ab);
int ath12k_wifi7_dp_rxdma_ring_sel_config_wcn7850(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst);
void ath12k_wifi7_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst);

static inline
void ath12k_wifi7_dp_extract_rx_spd_data(struct ath12k_hal *hal,
					 struct hal_rx_spd_data *rx_info,
					 struct hal_rx_desc *rx_desc)
{
	hal->hal_ops->extract_rx_spd_data(rx_info, rx_desc);
}
static inline
void ath12k_wifi7_dp_extract_rx_desc_data(struct ath12k_dp *dp,
					  struct hal_rx_desc_data *rx_desc_data,
					  struct hal_rx_desc *rx_desc,
					  struct hal_rx_desc *ldesc)
{
	dp->hw_params->hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

static inline
u32 ath12k_wifi7_dp_rx_h_mpdu_err(struct ath12k_dp *dp, struct hal_rx_desc *rx_desc)
{
	return dp->hal->hal_ops->hal_rx_h_mpdu_err(rx_desc);
}

void ath12k_wifi7_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info);
int ath12k_wifi7_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp);
ssize_t ath12k_wifi7_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size);
int ath12k_wifi7_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id, u8 pdev_id);
void ath12k_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
			       struct hal_reo_status *status);
void ath12k_wifi7_dp_rx_ring_free(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_ring_init(struct ath12k_base *ab);
int ath12k_wifi7_dp_rx_flow_fse_cache_operation(struct ath12k_base *ab,
						enum dp_flow_fst_operation op_code,
						struct hal_flow_tuple_info *tuple_info);
void ath12k_wifi7_dp_rx_h_undecap_eth(struct ath12k_pdev_dp *dp_pdev,
				      struct sk_buff *msdu,
				      enum hal_encrypt_type enctype,
				      struct ieee80211_rx_status *status,
				      bool mesh_ctrl_present,
				      struct hal_rx_desc *desc,
				      bool is_mcbc, u16 tid);
void ath12k_wifi7_dp_adjust_skb(struct hal_rx_spd_data *spd_desc_l,
				struct link_peer_rx_tid_stats *stats,
				int *msdu_idx, u32 hal_rx_desc_sz);
static inline void ath12k_wifi7_convert_eth_2_80211_frame(struct hal_rx_spd_data *rx_spd);
static inline u8 ath12k_wifi7_dp_rx_get_msdu_src_link(struct ath12k_dp *dp,
						      struct hal_rx_desc *desc)
{
	return dp->hw_params->hal_ops->rx_desc_get_msdu_src_link_id(desc);
}

static inline void ath12k_wifi7_dp_rx_desc_end_tlv_copy(struct ath12k_base *ab,
							struct hal_rx_desc *fdesc,
							struct hal_rx_desc *ldesc)
{
	ab->hw_params->hal_ops->rx_desc_copy_end_tlv(fdesc, ldesc);
}

static inline void ath12k_wifi7_dp_rxdesc_set_msdu_len(struct ath12k_base *ab,
						       struct hal_rx_desc *desc,
						       u16 len)
{
	ab->hw_params->hal_ops->rx_desc_set_msdu_len(desc, len);
}

static inline void ath12k_wifi7_dp_rx_desc_get_dot11_hdr(struct ath12k_base *ab,
							 struct hal_rx_desc *desc,
							 struct ieee80211_hdr *hdr)
{
	ab->hw_params->hal_ops->rx_desc_get_dot11_hdr(desc, hdr);
}

static inline u16 ath12k_wifi7_dp_rxdesc_get_mpdu_frame_ctrl(struct ath12k_base *ab,
							     struct hal_rx_desc *desc)
{
	return ab->hw_params->hal_ops->rx_desc_get_mpdu_frame_ctl(desc);
}

static inline bool ath12k_wifi7_dp_rx_h_more_frags(struct ath12k_base *ab,
						   struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return ieee80211_has_morefrags(hdr->frame_control);
}

static inline u16 ath12k_wifi7_dp_rx_h_frag_no(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_FRAG;
}

static inline
void ath12k_wifi7_dp_rx_h_csum_offload(struct sk_buff *msdu,
				       struct rx_msdu_desc_info *rx_msdu_info)
{
	msdu->ip_summed = (rx_msdu_info->tcp_udp_chksum_fail ||
			   rx_msdu_info->ip_chksum_fail) ?
		CHECKSUM_NONE : CHECKSUM_UNNECESSARY;
}

static inline
void ath12k_wifi7_dp_rx_desc_get_crypto_header(struct ath12k_base *ab,
					       struct hal_rx_desc *desc,
					       u8 *crypto_hdr,
					       enum hal_encrypt_type enctype)
{
	ab->hw_params->hal_ops->rx_desc_get_crypto_header(desc, crypto_hdr,
			enctype);
}

static void ath12k_wifi7_dp_rx_h_undecap_nwifi(struct ath12k_pdev_dp *dp_pdev,
					       struct sk_buff *msdu,
					       enum hal_encrypt_type enctype,
					       struct ieee80211_rx_status *status,
					       struct hal_rx_desc *desc,
					       bool mesh_ctrl_present, u16 qos_ctl)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u8 decap_hdr[DP_MAX_NWIFI_HDR_LEN];
	struct ieee80211_hdr *hdr;
	size_t hdr_len;
	u8 *crypto_hdr;
	int len;

	/* pull decapped header */
	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	skb_pull(msdu, hdr_len);

	/*  Rebuild qos header */
	hdr->frame_control |= __cpu_to_le16(IEEE80211_STYPE_QOS_DATA);

	/* Reset the order bit as the HT_Control header is stripped */
	hdr->frame_control &= ~(__cpu_to_le16(IEEE80211_FCTL_ORDER));

	if (mesh_ctrl_present)
		qos_ctl |= IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT;

	/* TODO: Add other QoS ctl fields when required */

	/* copy decap header before overwriting for reuse below */
	memcpy(decap_hdr, hdr, hdr_len);

	/* Rebuild crypto header for mac80211 use */
	if (!(status->flag & RX_FLAG_IV_STRIPPED)) {
		len = ath12k_dp_rx_crypto_param_len(dp, enctype);
		crypto_hdr = skb_push(msdu, len);

		ath12k_wifi7_dp_rx_desc_get_crypto_header(ab, desc,
							  crypto_hdr, enctype);
	}

	memcpy(skb_push(msdu,
			IEEE80211_QOS_CTL_LEN), &qos_ctl,
			IEEE80211_QOS_CTL_LEN);
	memcpy(skb_push(msdu, hdr_len), decap_hdr, hdr_len);
}

static inline u8
ath12k_wifi7_rx_create_fraglist(struct hal_rx_spd_data *spd_desc,
				u32 rx_tlv_sz)
{
	struct hal_rx_spd_data *spd_desc_orig = spd_desc;
	struct sk_buff *parent = NULL;
	struct sk_buff *frag_list = NULL;
	struct sk_buff *tmp = NULL;
	struct rx_msdu_desc_info *rx_msdu_info;
	u16 msdu_len;
	u8 l3_pad_bytes;
	u8 idx = 0;
	u16 frag_list_len = 0;
	u16 buf_size = DP_RX_BUFFER_SIZE;

	rx_msdu_info = &spd_desc->rx_msdu_info;

	msdu_len = rx_msdu_info->msdu_length;
	l3_pad_bytes = rx_msdu_info->l3_header_padding_msb ? 2 : 0;

	parent = spd_desc->msdu;

	skb_put(parent, buf_size);
	skb_pull(parent, rx_tlv_sz + l3_pad_bytes);

	msdu_len -= parent->len;

	/* set checksum pass or fail only in parent skb */
	ath12k_wifi7_dp_rx_h_csum_offload(parent, rx_msdu_info);

	spd_desc->first_sg_frame = 0;
	do {
		spd_desc++;

		if (!frag_list) {
			frag_list = spd_desc->msdu;
			tmp = frag_list;
		} else {
			tmp->next = spd_desc->msdu;
			tmp = tmp->next;
		}

		if (msdu_len + rx_tlv_sz > buf_size) {
			skb_put(tmp, buf_size);
			msdu_len -= (buf_size - rx_tlv_sz);
		} else {
			skb_put(tmp, msdu_len + rx_tlv_sz);
			msdu_len = 0;
		}
		skb_pull(tmp, rx_tlv_sz);
		frag_list_len += tmp->len;
		idx++;
	} while (!spd_desc->last_sg_frame);
	spd_desc->last_sg_frame = 0;

	skb_shinfo(parent)->frag_list = frag_list;
	parent->data_len = 0;
	parent->data_len += frag_list_len;
	parent->len += frag_list_len;

	/* save the vaddr in the first scratch_pad desc
	 * since the last spad->vaddr (TLV_HDR) of the SG frame
	 * holds proper radio params and these parameters
	 * are needed to fill ieee80211_rx_status based on
	 * DECAP type.
	 */
	spd_desc_orig->vaddr = spd_desc->vaddr;
	return idx;
}

static inline
bool ath12k_wifi7_compare_tlv_info(struct rx_tlv_info_1 *prev_tlv_info,
				   struct rx_tlv_info_1 *tlv_info)
{
	if (prev_tlv_info->freq != tlv_info->freq ||
	    prev_tlv_info->rate_mcs != tlv_info->rate_mcs ||
	    prev_tlv_info->nss != tlv_info->nss ||
	    prev_tlv_info->sgi != tlv_info->sgi ||
	    prev_tlv_info->pkt_type != tlv_info->pkt_type ||
	    prev_tlv_info->bw != tlv_info->bw ||
	    prev_tlv_info->decap != tlv_info->decap)
		return false;
	else
		return true;
}

static inline
void ath12k_wifi7_copy_tlv_info(struct rx_tlv_info_1 *prev_tlv_info,
				struct rx_tlv_info_1 *tlv_info)
{
	prev_tlv_info->freq = tlv_info->freq;
	prev_tlv_info->rate_mcs = tlv_info->rate_mcs;
	prev_tlv_info->nss = tlv_info->nss;
	prev_tlv_info->sgi = tlv_info->sgi;
	prev_tlv_info->pkt_type = tlv_info->pkt_type;
	prev_tlv_info->bw = tlv_info->bw;
	prev_tlv_info->decap = tlv_info->decap;
}

static inline
int ath12k_wifi7_deliver_raw_frame(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_spd_data *rx_spd,
				   struct ath12k_dp_peer *peer,
				   struct ieee80211_rx_status *status,
				   struct napi_struct *napi,
				   struct link_peer_rx_tid_stats *stats,
				   struct rx_tlv_info_1 *prev_tlv_info)
{
	enum hal_encrypt_type enctype = HAL_ENCRYPT_TYPE_OPEN;
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal;
	struct hal_rx_desc *rx_tlv_hdr;
	bool is_mcbc = false;
	bool ret, decrypted;
	bool ra_mcbc = rx_spd->rx_msdu_info.da_is_mcbc;
	u16 peer_id = ATH12K_PEER_ID_INVALID;

	if (peer)
		ra_mcbc = ra_mcbc && !peer->is_reset_mcbc;

	is_mcbc = is_ieee80211_frame_da_mcast(msdu);

	hal = dp_pdev->dp->hal;

	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;

	rx_tlv_hdr = (struct hal_rx_desc *)rx_spd->vaddr;

	status->flag &= ~(RX_FLAG_FAILED_FCS_CRC |
			  RX_FLAG_MMIC_ERROR |
			  RX_FLAG_DECRYPTED |
			  RX_FLAG_IV_STRIPPED |
			  RX_FLAG_MMIC_STRIPPED);

	if (!peer) {
		status->link_valid = 0;
	} else {
		pubsta = peer->sta;
		enctype = ra_mcbc ? peer->sec_type_grp : peer->sec_type;
		peer_id = peer->peer_id;
	}

	if (pubsta && pubsta->valid_links) {
		status->link_valid = 1;
		status->link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(
							peer,
							rx_spd->reo.src_link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi7_dp_extract_rx_spd_data(hal, rx_spd, rx_tlv_hdr);

	decrypted = ath12k_hal_rx_h_is_decrypted(hal, rx_tlv_hdr);

	if (decrypted && enctype != HAL_ENCRYPT_TYPE_OPEN) {
		status->flag |= RX_FLAG_DECRYPTED | RX_FLAG_MMIC_STRIPPED;

		if (ra_mcbc)
			status->flag |= RX_FLAG_MIC_STRIPPED | RX_FLAG_ICV_STRIPPED;
		else
			status->flag |= RX_FLAG_IV_STRIPPED | RX_FLAG_PN_VALIDATED;
	} else {
		decrypted = false;
	}

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
					HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return 1;
	}

	ath12k_dp_rx_h_undecap_raw(dp_pdev, msdu,
				   (struct hal_rx_desc *)rx_tlv_hdr,
				   enctype,
				   status, decrypted, peer_id);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!is_mcbc) {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	}

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
	return 1;
}

static inline
int ath12k_wifi7_deliver_nwifi_frame(struct ath12k_pdev_dp *dp_pdev,
				     struct hal_rx_spd_data *rx_spd,
				     struct ath12k_dp_peer *peer,
				     struct ieee80211_rx_status *status,
				     struct napi_struct *napi,
				     struct link_peer_rx_tid_stats *stats,
				     struct rx_tlv_info_1 *prev_tlv_info)
{
	enum hal_encrypt_type enctype = HAL_ENCRYPT_TYPE_OPEN;
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal;
	u8 *rx_tlv_hdr;
	struct ieee80211_hdr *hdr;
	u32 hdr_len;
	bool ret;
	bool ra_mcbc = rx_spd->rx_msdu_info.da_is_mcbc;
	u16 peer_id = ATH12K_PEER_ID_INVALID;

	if (peer)
		ra_mcbc = ra_mcbc && !peer->is_reset_mcbc;

	/* ideally driver should not be doing this check.
	 * instead HW should flag this with error_code
	 * "rxdma_msdu_len_err" and release it via WBM
	 * release ring, based on this error code driver
	 * should drop the frame.
	 */
	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	if ((likely(hdr_len > DP_MAX_NWIFI_HDR_LEN)))
		WARN_ON(1);

	hal = dp_pdev->dp->hal;

	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;

	rx_tlv_hdr = rx_spd->vaddr;

	status->flag |= RX_FLAG_DECRYPTED |
			RX_FLAG_MMIC_STRIPPED |
			RX_FLAG_IV_STRIPPED |
			RX_FLAG_PN_VALIDATED |
			RX_FLAG_SKIP_MONITOR |
			RX_FLAG_DUP_VALIDATED;

	if (!peer) {
		status->link_valid = 0;
	} else {
		pubsta = peer->sta;
		enctype = ra_mcbc ? peer->sec_type_grp : peer->sec_type;
		peer_id = peer->peer_id;
	}

	if (pubsta && pubsta->valid_links) {
		status->link_valid = 1;
		status->link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(
							peer,
							rx_spd->reo.src_link_id);
	}

	msdu->priority = rx_mpdu_info->tid;

	ath12k_wifi7_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	ath12k_wifi7_dp_extract_rx_spd_data(hal,
					    rx_spd,
					    (struct hal_rx_desc *)rx_tlv_hdr);

	/* copy from scratch_pad to ieee80211_rx_status */
	tlv_info = &rx_spd->tlv_info;
	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, tlv_info,
				  HAL_WBM_REL_SRC_MODULE_REO);
	if (ret) {
		dev_kfree_skb_any(msdu);
		return 1;
	}

	ath12k_wifi7_dp_rx_h_undecap_nwifi(dp_pdev, msdu, enctype,
					   status,
					   (struct hal_rx_desc *)rx_tlv_hdr,
					   rx_spd->tlv_info.mesh_ctrl_present,
					   rx_mpdu_info->tid);

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!rx_msdu_info->da_is_mcbc) {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	}

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
	return 1;
}

#ifdef CPTCFG_QCN_EXTN
int ath12k_wifi7_deliver_ethernet_frame(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_spd_data *rx_spd,
					struct ath12k_dp_peer *peer,
					struct ieee80211_rx_status *status,
					struct napi_struct *napi,
					struct link_peer_rx_tid_stats *stats,
					struct rx_tlv_info_1 *prev_tlv_info);
#else
static inline
int ath12k_wifi7_deliver_ethernet_frame(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_spd_data *rx_spd,
					struct ath12k_dp_peer *peer,
					struct ieee80211_rx_status *status,
					struct napi_struct *napi,
					struct link_peer_rx_tid_stats *stats,
					struct rx_tlv_info_1 *prev_tlv_info)
{
	struct rx_msdu_desc_info *rx_msdu_info;
	struct rx_mpdu_desc_info *rx_mpdu_info;
	struct rx_tlv_info_1 *tlv_info;
	struct ieee80211_rx_status *rx_status;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ath12k_dp *dp = dp_pdev->dp;
	u8 *rx_tlv_hdr;
	u8 tid;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_hal *hal = dp_pdev->dp->hal;
	bool ret;

	rx_tlv_hdr = rx_spd->vaddr;
	rx_msdu_info = &rx_spd->rx_msdu_info;
	rx_mpdu_info = &rx_spd->rx_mpdu_info;
	tid = rx_mpdu_info->tid;

	/* set checksum pass or fail only in parent skb */
	ath12k_wifi7_dp_rx_h_csum_offload(msdu, rx_msdu_info);

	msdu->dev = peer->dev;

	/* ieee80211_rx_status object is reset when ever there is a
	 * peer change or radio change, if so extract radio params from
	 * the current MSDUs TLV headers and set the status info
	 * accordingly.
	 * Note: Radio params (ie: band, nss, sgi etc...) remain same
	 */
	ath12k_wifi7_dp_extract_rx_spd_data(hal, rx_spd,
					    (struct hal_rx_desc *)rx_tlv_hdr);

	tlv_info = &rx_spd->tlv_info;

	if (unlikely(!ath12k_wifi7_compare_tlv_info(prev_tlv_info,
						    tlv_info))) {
		ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev,
						status,
						tlv_info,
						HAL_WBM_REL_SRC_MODULE_REO);

		if (ret) {
			dev_kfree_skb_any(msdu);
			return 1;
		}

		status->flag |= RX_FLAG_8023 |
			RX_FLAG_DECRYPTED |
			RX_FLAG_MMIC_STRIPPED |
			RX_FLAG_IV_STRIPPED |
			RX_FLAG_PN_VALIDATED |
			RX_FLAG_SKIP_MONITOR |
			RX_FLAG_DUP_VALIDATED;
		ath12k_wifi7_copy_tlv_info(prev_tlv_info, tlv_info);
	}
	pubsta = peer->sta;
	if (pubsta && pubsta->valid_links) {
		status->link_valid = 1;
		status->link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(
							peer,
							rx_spd->reo.src_link_id);
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
		ath12k_wifi7_convert_eth_2_80211_frame(rx_spd);
		status->flag &= ~RX_FLAG_8023;
	}

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (rx_msdu_info->da_is_mcbc) {
		stats->sent_to_stack_mcast++;
		stats->sent_to_stack_mcast_bytes += msdu->len;
	} else {
		stats->sent_to_stack_ucast++;
		stats->sent_to_stack_ucast_bytes += msdu->len;
	}

	dp->device_stats.non_fast_unicast_rx[rx_spd->reo.ring_id][dp->device_id]++;
	prefetch(skb_shinfo(msdu));

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);

	return 1;
}
#endif

static inline
struct ath12k_rx_desc_info *
ath12k_wifi7_get_sw_desc_from_hw_wbm_desc(struct hal_wbm_completion_ring_rx *hw_rx_desc)
{
	u64 desc_va = 0;

	desc_va = ((u64)le32_to_cpu(hw_rx_desc->addr_hi) << 32 |
		   le32_to_cpu(hw_rx_desc->addr_lo));
	return (struct ath12k_rx_desc_info *)((unsigned long)desc_va);
}

static inline
struct ath12k_rx_desc_info *
ath12k_wifi7_get_sw_desc_from_wbm_hw_desc(struct hal_wbm_release_ring *desc)
{
	struct hal_wbm_release_ring_cc_rx *wbm_cc_desc =
				(struct hal_wbm_release_ring_cc_rx *)desc;
	u64 desc_va = 0;

	desc_va = ((u64)le32_to_cpu(wbm_cc_desc->buf_va_hi) << 32 |
			le32_to_cpu(wbm_cc_desc->buf_va_lo));

	return (struct ath12k_rx_desc_info *)((unsigned long)desc_va);
}

static inline
struct ath12k_rx_desc_info *
ath12k_wifi7_get_sw_desc_from_hw_desc(struct hal_reo_dest_ring *hw_rx_desc)
{
	u64 desc_va = 0;

	desc_va = ((u64)le32_to_cpu(hw_rx_desc->buf_va_hi) << 32 |
		   le32_to_cpu(hw_rx_desc->buf_va_lo));

	return (struct ath12k_rx_desc_info *)((unsigned long)desc_va);
}

static inline
void ath12k_wifi7_rx_sw_desc_sanity_check(struct ath12k_rx_desc_info *sw_desc)
{
	if (!sw_desc) {
		pr_err("looks like HW cookie conversion table is corrupted");
		WARN_ON(1);
	}

	if (unlikely(sw_desc->magic != ATH12K_DP_RX_DESC_MAGIC)) {
		pr_err("Check HW CC implementation, vaddr = 0x%p",
			(void *)sw_desc->vaddr);
			WARN_ON(1);
	}

	if (unlikely(!sw_desc->in_use)) {
		pr_err("The SW descriptor is in free pool (!in_use), yet HW released it to host, vaddr = 0x%p",
			(void *)sw_desc->vaddr);
		WARN_ON(1);
	}
}

static inline void
ath12k_wifi7_cpy_hw_wbm_rx_desc_to_spad_desc(struct hal_wbm_release_ring *hw_desc,
					     struct hal_rx_spd_data *rx_spd)
{
	rx_spd->info1 = le64_to_cpu(hw_desc->info1);
	rx_spd->info2 = le32_to_cpu(hw_desc->info2);
	rx_spd->info0 = le32_to_cpu(hw_desc->info0);
}

static inline
void ath12k_wifi7_cpy_hw_rx_desc_to_spad_desc(struct hal_reo_dest_ring *hw_desc,
					      struct hal_rx_spd_data *rx_spd)
{
	rx_spd->info1 = le64_to_cpu(hw_desc->info1);
	rx_spd->info2 = le32_to_cpu(hw_desc->info2);
	rx_spd->info0 = le32_to_cpu(hw_desc->info0);
}

static inline
void
ath12k_wifi7_pretech_next_sw_desc_wbm(struct hal_wbm_release_ring *desc)
{
	u64 desc_va = 0;
	struct ath12k_rx_desc_info *sw_desc = NULL;
	struct hal_wbm_release_ring_cc_rx *wbm_cc_desc =
				(struct hal_wbm_release_ring_cc_rx *)desc;

	desc_va = ((u64)le32_to_cpu(wbm_cc_desc->buf_va_hi) << 32 |
		   le32_to_cpu(wbm_cc_desc->buf_va_lo));
	sw_desc = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	if (sw_desc)
		prefetch(sw_desc);
}

static inline
void ath12k_wifi7_pretech_next_sw_desc(struct hal_reo_dest_ring *hw_rx_desc)
{
	u64 desc_va = 0;
	struct ath12k_rx_desc_info *sw_desc = NULL;

	desc_va = ((u64)le32_to_cpu(hw_rx_desc->buf_va_hi) << 32 |
		   le32_to_cpu(hw_rx_desc->buf_va_lo));
	sw_desc = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);

	if (sw_desc)
		prefetch(sw_desc);
}

static inline
void ath12k_wifi7_convert_eth_2_80211_frame(struct hal_rx_spd_data *rx_spd)
{
	struct ieee80211_hdr hdr;
	struct sk_buff *msdu = rx_spd->msdu;
	struct ethhdr *eth;
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
	u8 hdr_len;
	u16 tid = rx_spd->rx_mpdu_info.tid;
	struct ath12k_dp_rx_rfc1042_hdr rfc = {0xaa, 0xaa, 0x03, {0x00, 0x00, 0x00}};

	eth = (struct ethhdr *)msdu->data;
	ether_addr_copy(da, eth->h_dest);
	ether_addr_copy(sa, eth->h_source);
	rfc.snap_type = eth->h_proto;

	hdr.frame_control = rx_spd->frame_ctl;
	hdr.duration_id = rx_spd->duration_id;
	hdr.seq_ctrl = rx_spd->seq_ctl;

	ether_addr_copy(hdr.addr1, rx_spd->ad1);
	ether_addr_copy(hdr.addr2, rx_spd->ad2);

	ether_addr_copy(hdr.addr3, da);
	ether_addr_copy(hdr.addr4, sa);

	hdr_len = ieee80211_hdrlen(hdr.frame_control);

	skb_pull(msdu, sizeof(*eth));
	memcpy(skb_push(msdu, sizeof(rfc)), &rfc, sizeof(rfc));

	skb_push(msdu, hdr_len);
	memcpy(msdu->data, &hdr, min(hdr_len, sizeof(hdr)));

	if (ieee80211_is_data_qos(hdr.frame_control)) {
		struct ieee80211_hdr *qhdr = (struct ieee80211_hdr *)msdu->data;

		memcpy(ieee80211_get_qos_ctl(qhdr), &tid, IEEE80211_QOS_CTL_LEN);
	}
}
#endif
