/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_RX_WIFI8_H
#define ATH12K_DP_RX_WIFI8_H

#include "../core.h"
#include "../dp_rx.h"
#include "hal_desc.h"
#include "hal.h"

enum dp_rx_ppeds_wbm_refill_ring {
	PPE2WBM_HW_REFILL_RING = 0,
	PPE2WBM_SW_REFILL_RING = 1,
};

#define ATH12K_FSE_CACHE_INVAL_FULL_DELAY_MS	500

struct dp_rx_fse {
	struct hal_rx_fse *hal_fse;
	u32 flow_hash;
	u32 flow_id;
	u8 reo_indication;
	bool is_valid;
};

#ifndef CPTCFG_EXT_IPA_OFFLOAD
#define IPA_SET_RX_BUF_SMMU_MAP(...) ((void)0)
#define IPA_SET_RX_BUF_SMMU_UNMAP(...) ((void)0)
#define ATH12K_IPA_DMA_MAP_SINGLE(...) ((void)0)
#endif

int ath12k_wifi8_dp_rx_wbm_buf_ring_init(struct ath12k_base *ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_dp_rx_ppe2wbm_idle_buff_init(struct ath12k_base *ab);
#endif
int ath12k_wifi8_dp_reo_cmd_send(struct ath12k_base *ab,
				 void *data, size_t len,
				 enum hal_reo_cmd_type type,
				 struct ath12k_hal_reo_cmd *cmd,
				 void (*cb)(struct ath12k_dp *dp, void *ctx,
					    struct hal_reo_status *status));
int ath12k_wifi8_dp_reo_cmd_send_highprio(struct ath12k_base *ab,
					  void *data, size_t len,
					  enum hal_reo_cmd_type type,
					  struct ath12k_hal_reo_cmd *cmd,
					  void (*cb)(struct ath12k_dp *dp, void *ctx,
						     struct hal_reo_status *status));
struct ath12k_reo_dp_cmd_desc {
	void *data;
	size_t len;
	void (*cb)(struct ath12k_dp *dp, void *ctx,
		   struct hal_reo_status *status);
};

int ath12k_wifi8_dp_reo_cmd_send_highprio_n(struct ath12k_base *ab,
					    struct ath12k_reo_cmd_entry *entries,
					    struct ath12k_reo_dp_cmd_desc *dp_descs,
					    int n);
int ath12k_wifi8_dp_fse_cmd_send(struct ath12k_base *ab,
				 struct hal_fse_cmd *fse_cmd);
int ath12k_wifi8_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget);
int ath12k_wifi8_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi,
			       int budget);
void ath12k_wifi8_dp_rx_peer_tid_delete(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer, u8 tid);
bool ath12k_wifi8_dp_mlo_peer_tid_teardown_ready(struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *link_peer);
bool ath12k_wifi8_dp_rx_h_ppdu(struct ath12k_pdev_dp *dp_pdev,
			       struct ieee80211_rx_status *rx_status,
			       struct rx_tlv_info_1 *tlv_info,
			       u8 err_rel_src);
int ath12k_wifi8_dp_reo_cache_flush(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid);
int ath12k_wifi8_peer_rx_tid_reo_update(struct ath12k *ar,
					struct ath12k_dp_link_peer *peer,
					struct ath12k_dp_rx_tid *rx_tid,
					u32 ba_win_sz, u16 ssn,
					bool update_ssn);
void ath12k_wifi8_peer_rx_tid_qref_setup(struct ath12k_base *ab, u16 peer_id,
					 u16 tid, dma_addr_t paddr);
int ath12k_wifi8_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action);
int ath12k_wifi8_dp_rx_process_reo_status(struct ath12k_dp *dp, int budget);

int ath12k_wifi8_dp_rx_peer_tid_setup(struct ath12k *ar, const u8 *peer_mac, int vdev_id,
				      u8 tid, u32 ba_win_sz, u16 ssn,
				      enum hal_pn_type pn_type);
void ath12k_wifi8_dp_setup_pn_check_reo_cmd(struct ath12k_hal_reo_cmd *cmd,
					    struct ath12k_dp_rx_tid *rx_tid,
					    u32 cipher, enum set_key_cmd key_cmd);
int ath12k_wifi8_dp_alloc_reo_qdesc(struct ath12k_base *ab,
				    struct ath12k_dp_rx_tid *rx_tid, u16 ssn,
				    enum hal_pn_type pn_type,
				    struct hal_rx_reo_queue **addr_aligned,
				    u16 stats_id);
int ath12k_wifi8_dp_rxdma_ring_sel_config_qcn9625(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_fst_attach(struct ath12k_dp *dp, struct dp_rx_fst *fst);
void ath12k_wifi8_dp_rx_fst_detach(struct ath12k_dp *dp, struct dp_rx_fst *fst);
void ath12k_wifi8_dp_rx_flow_dump_entry(struct ath12k_dp *dp,
					struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_add_entry(struct ath12k_dp *dp,
				      struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_delete_entry(struct ath12k_dp *dp,
					 struct rx_flow_info *flow_info);
int ath12k_wifi8_dp_rx_flow_delete_all_entries(struct ath12k_dp *dp);
ssize_t ath12k_wifi8_dp_dump_fst_table(struct ath12k_dp *dp, char *buf, int size);
int ath12k_wifi8_dp_peer_migrate_reo_cmd(struct ath12k_dp *dp,
					 struct ath12k_dp_link_peer *peer,
					 u16 peer_id, u8 chip_id);
void ath12k_dp_rx_tid_del_func(struct ath12k_dp *dp, void *ctx,
			       struct hal_reo_status *status);
void ath12k_wifi8_dp_rx_ring_cleanup(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_wbm_srng_init(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_ring_init(struct ath12k_base *ab);
int ath12k_wifi8_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi8_dp_pdev_free(struct ath12k_base *ab);
int ath12k_wifi8_dp_rx_flow_fse_cache_operation(struct ath12k_base *ab,
						enum dp_flow_fst_operation op_code,
						struct hal_flow_tuple_info *tuple_info);
int ath12k_wifi8_dp_rx_process_reo_flush_err(struct ath12k_dp *dp,
					     struct napi_struct *napi,
					     int budget);
int
ath12k_wifi8_peer_rx_tid_reo_update_for_smd(struct ath12k_base *ab,
					    struct ath12k_dp_hw *dp_hw,
					    struct ieee80211_vif *vif,
					    const u8 *peer_addr,
					    struct ath12k_rx_smd_ctx_per_tid *rx_tid_ctx);
int ath12k_wifi8_peer_rx_tid_svld_reset(struct ath12k_base *ab,
					struct ath12k_dp_hw *dp_hw,
					struct ieee80211_vif *vif,
					const u8 *peer_addr);
void ath12k_wifi8_peer_rx_tid_reo_clear_vld_cmd_init(struct ath12k_dp_rx_tid *rx_tid,
						     struct ath12k_hal_reo_cmd *cmd);
int ath12k_wifi8_peer_rx_tid_reo_clear_vld(struct ath12k_base *ab,
					   struct ath12k_dp_hw *dp_hw,
					   struct ieee80211_vif *vif,
					   const u8 *peer_addr,
					   u8 tid);
/* Module parameter: controls whether REO VLD is cleared after fetching SMD ctx.
 * Declared in wifi8/core.c; extern here so dp.c and any future SMD callers
 * can gate their behaviour without adding new function arguments.
 */
extern bool ath12k_wifi8_clear_vld_after_smd_ctx_fetch;
extern bool ath12k_wifi8_smd_skip_bitmap_update;
int ath12k_wifi8_dp_rx_ase_htt_srng_setup(struct ath12k_base *ab);

/* SMD BSS Transition: Rx Q Info park / restore */
int ath12k_wifi8_dp_smd_prep_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *addr);
int ath12k_wifi8_dp_smd_exec_rx_tid(struct ath12k_dp *dp,
				    struct ath12k_dp_hw *dp_hw,
				    struct ieee80211_vif *vif,
				    const u8 *addr);
void ath12k_wifi8_dp_smd_clear_old_peer_rx_lut(struct ath12k_dp *dp,
					       struct ath12k_dp_peer *dp_peer);
int ath12k_wifi8_dp_rx_process_reo_rings(struct ath12k_dp *dp,
					 struct hal_srng *srng,
					 struct hal_rx_spd_data *rx_status_desc,
					 int ring_id, int budget,
					 int cpu_id);
void ath12k_wifi8_dp_rx_h_undecap_nwifi(struct ath12k_pdev_dp *dp_pdev,
					struct sk_buff *msdu,
					enum hal_encrypt_type enctype,
					struct ieee80211_rx_status *status,
					struct hal_rx_desc *desc,
					bool mesh_ctrl_present, u16 qos_ctl);
void ath12k_wifi8_deliver_nwifi_frame(struct ath12k_pdev_dp *dp_pdev,
				      struct hal_rx_spd_data *rx_spd,
				      struct ath12k_dp_peer *peer,
				      struct ieee80211_rx_status *status,
				      struct napi_struct *napi,
				      struct link_peer_rx_tid_stats *stats,
				      struct rx_tlv_info_1 *prev_tlv_info);
void ath12k_wifi8_deliver_raw_frame(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_spd_data *rx_spd,
				    struct ath12k_dp_peer *peer,
				    struct ieee80211_rx_status *status,
				    struct napi_struct *napi,
				    struct link_peer_rx_tid_stats *stats,
				    struct rx_tlv_info_1 *prev_tlv_info);
void ath12k_wifi8_deliver_ethernet_frame(struct ath12k_pdev_dp *dp_pdev,
					 struct hal_rx_spd_data *rx_spd,
					 struct ath12k_dp_peer *peer,
					 struct ieee80211_rx_status *status,
					 struct napi_struct *napi,
					 struct link_peer_rx_tid_stats *stats,
					 struct rx_tlv_info_1 *prev_tlv_info);
void ath12k_wifi8_dp_adjust_skb(struct hal_rx_spd_data *spd_desc_l,
				struct link_peer_rx_tid_stats *stats,
				int *msdu_idx, u32 hal_rx_desc_sz);
void ath12k_wifi8_convert_n_deliver_nw_frame(struct ath12k_pdev_dp *dp_pdev,
					     struct hal_rx_spd_data *rx_spd,
					     struct ath12k_dp_peer *peer,
					     struct ieee80211_rx_status *status,
					     struct napi_struct *napi,
					     struct rx_tlv_info_1 *prev_tlv_info);
bool ath12k_wifi8_handle_null_queue(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_peer *peer,
				    struct ieee80211_rx_status *rx_status,
				    struct hal_rx_spd_data *spd_desc_l,
				    struct napi_struct *napi,
				    struct rx_tlv_info_1 *prev_tlv_info);
static inline
void ath12k_wifi8_dp_extract_rx_spd_data(struct ath12k_hal *hal,
					 struct hal_rx_spd_data *rx_info,
					 struct hal_rx_desc *rx_desc)
{
	hal->hal_ops->extract_rx_spd_data(rx_info, rx_desc);
}

static inline
void ath12k_wifi8_dp_extract_rx_desc_data(struct ath12k_dp *dp,
					  struct hal_rx_desc_data *rx_desc_data,
					  struct hal_rx_desc *rx_desc,
					  struct hal_rx_desc *ldesc)
{
	dp->hal->hal_ops->extract_rx_desc_data(rx_desc_data, rx_desc, ldesc);
}

static inline u8 ath12k_wifi8_dp_rx_get_msdu_src_link(struct ath12k_dp *dp,
						      struct hal_rx_desc *desc)
{
	return dp->hal->hal_ops->rx_desc_get_msdu_src_link_id(desc);
}

static inline void ath12k_wifi8_dp_rx_desc_end_tlv_copy(struct ath12k_base *ab,
							struct hal_rx_desc *fdesc,
							struct hal_rx_desc *ldesc)
{
	ab->hal.hal_ops->rx_desc_copy_end_tlv(fdesc, ldesc);
}

static inline void ath12k_wifi8_dp_rxdesc_set_msdu_len(struct ath12k_base *ab,
						       struct hal_rx_desc *desc,
						       u16 len)
{
	ab->hal.hal_ops->rx_desc_set_msdu_len(desc, len);
}

static inline void ath12k_wifi8_dp_rx_desc_get_dot11_hdr(struct ath12k_dp *dp,
							 struct hal_rx_desc *desc,
							 struct ieee80211_hdr *hdr)
{
	dp->hal->hal_ops->rx_desc_get_dot11_hdr(desc, hdr);
}

static inline
void ath12k_wifi8_dp_rx_desc_get_crypto_header(struct ath12k_base *ab,
					       struct hal_rx_desc *desc,
					       u8 *crypto_hdr,
					       enum hal_encrypt_type enctype)
{
	ab->hal.hal_ops->rx_desc_get_crypto_header(desc, crypto_hdr,
			enctype);
}

static inline u16 ath12k_wifi8_dp_rxdesc_get_mpdu_frame_ctrl(struct ath12k_base *ab,
							     struct hal_rx_desc *desc)
{
	return ab->hal.hal_ops->rx_desc_get_mpdu_frame_ctl(desc);
}

static inline bool ath12k_wifi8_dp_rx_h_more_frags(struct ath12k_base *ab,
						   struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return ieee80211_has_morefrags(hdr->frame_control);
}

static inline u16 ath12k_wifi8_dp_rx_h_frag_no(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;

	hdr = (struct ieee80211_hdr *)(skb->data + ab->hal.hal_desc_sz);
	return le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_FRAG;
}

static inline u16
ath12k_wifi8_dp_rx_get_peer_id(struct ath12k_base *ab,
			       enum ath12k_peer_metadata_version ver,
			       __le32 peer_metadata)
{
	switch (ver) {
	default:
		ath12k_warn(ab, "Unknown peer metadata version: %d", ver);
		fallthrough;
	case ATH12K_PEER_METADATA_V0:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V0_PEER_ID_WIFI8);
	case ATH12K_PEER_METADATA_V1:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1_PEER_ID_WIFI8);
	case ATH12K_PEER_METADATA_V1A:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1A_PEER_ID_WIFI8);
	case ATH12K_PEER_METADATA_V1B:
		return le32_get_bits(peer_metadata,
				     RX_MPDU_DESC_META_DATA_V1B_PEER_ID_WIFI8);
	}
}

static inline struct ath12k_rx_desc_info *
ath12k_wifi8_get_sw_desc_from_hw_desc(struct ath12k_dp *dp,
				      struct hal_reo_dest_ring *desc)
{
	u64 desc_va;
	struct ath12k_rx_desc_info *sw_rx_desc = NULL;
	u32 sw_cookie;

	/* When cookie conversion is enabled the hardware stores the
	 * SW descriptor VA directly in buf_addr_info; extract it.
	 * When cookie conversion is disabled (e.g. IPA_OFFLOAD) the
	 * field holds the raw DMA address + sw_cookie, so fall back
	 * to the manual SPT lookup.
	 */
	if (likely(le32_get_bits(desc->info0,
			  HAL_REO_DESTINATION_RING_INFO0_COOKIE_CONVERSION_STATUS))) {
		desc_va = ((u64)le32_to_cpu(desc->buf_addr_info.info1) << 32) |
				le32_to_cpu(desc->buf_addr_info.info0);
		sw_rx_desc = (struct ath12k_rx_desc_info *)((unsigned long)desc_va);
	} else {
		if (likely(le32_get_bits(desc->rx_mpdu_ext_info.info0,
				  HAL_RX_MPDU_EXT_DESC_INFO_INFO0_REO_DEST_BUFFER_TYPE) ==
				HAL_REO_DEST_RING_BUFFER_TYPE_MSDU)) {
			sw_cookie = le32_get_bits(desc->buf_addr_info.info1,
						  BUFFER_ADDR_INFO1_SW_COOKIE);

			sw_rx_desc = ath12k_dp_get_rx_desc(dp, sw_cookie);
		}
	}

	return sw_rx_desc;
}

static inline void
ath12k_wifi8_pretech_next_sw_desc(struct ath12k_dp *dp, struct hal_reo_dest_ring *desc)
{
	struct ath12k_rx_desc_info *sw_desc =
		ath12k_wifi8_get_sw_desc_from_hw_desc(dp, desc);

	if (sw_desc)
		prefetch(sw_desc);
}

static inline void
ath12k_wifi8_cpy_hw_rx_desc_to_spad_desc(struct hal_reo_dest_ring *desc,
					 struct hal_rx_spd_data *rx_spd)
{
	rx_spd->info0 = le32_to_cpu(desc->info0);
	rx_spd->info2 = le32_to_cpu(desc->info2);

	/* RX_MPDU_INFO */
	rx_spd->rx_mpdu_info.info0 = le32_to_cpu(desc->rx_mpdu_info.info0);
	rx_spd->rx_mpdu_info.peer_meta_data =
		le32_to_cpu(desc->rx_mpdu_info.peer_meta_data);
	rx_spd->rx_mpdu_info.info1 =
		le32_to_cpu(desc->rx_mpdu_ext_info.info0);
}

static inline void
ath12k_wifi8_copy_tlv_info(struct rx_tlv_info_1 *prev_tlv_info,
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

static inline bool
ath12k_wifi8_compare_tlv_info(struct rx_tlv_info_1 *prev_tlv_info,
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

static inline void
ath12k_wifi8_dp_rx_h_csum_offload(struct sk_buff *msdu,
				  struct rx_msdu_desc_info *rx_msdu_info)
{
	msdu->ip_summed = (rx_msdu_info->tcp_udp_chksum_fail ||
			rx_msdu_info->ip_chksum_fail) ?
		CHECKSUM_NONE : CHECKSUM_UNNECESSARY;
}

static inline
void ath12k_wifi8_convert_eth_2_80211_frame(struct ath12k_dp *dp,
					    struct hal_rx_spd_data *rx_spd)
{
	struct ieee80211_hdr hdr;
	struct sk_buff *msdu = rx_spd->msdu;
	struct hal_rx_desc *tlv = (struct hal_rx_desc *)rx_spd->vaddr;
	struct ethhdr *eth;
	u8 hdr_len;
	u16 tid = rx_spd->rx_mpdu_info.tid;
	struct ath12k_dp_rx_rfc1042_hdr rfc = {0xaa, 0xaa, 0x03, {0x00, 0x00, 0x00}};

	eth = (struct ethhdr *)msdu->data;
	rfc.snap_type = eth->h_proto;

	ath12k_wifi8_dp_rx_desc_get_dot11_hdr(dp, tlv, &hdr);

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
