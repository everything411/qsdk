// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_tx.h"
#include "../peer.h"
#include "dp_tx.h"
#include "hal_rx.h"
#include "../debugfs_sta.h"
#include "../debugfs.h"
#include "../sdwf.h"
#include "../dp_stats.h"
#include "../dp_peer.h"
#include "../telemetry.h"
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
#include "../qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */
#include "../telemetry_agent_if.h"
#include "dp_peer.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif /* CPTCFG_QCN_EXTN */

#ifndef CPTCFG_EXT_IPA_OFFLOAD
#define ATH12K_DMA_UNMAP_WITH_FREE_SKB(...) ((void)0)
#define IPA_TX_BUFFER_FREE(...) ((void)0)
#define IPA_TX_BUFFER_ALLOC(...) ((void)0)
#define ATH12K_SKB_COPY_PADDR(...) ((void)0)
#define ATH12K_SET_IPA_TCL_RING(...) ((void)0)

#define ATH12K_HAL_SRNG_ACCESS_BEGIN(ab, tcl_ring) do { \
	(void)(ab); \
	ath12k_hal_srng_access_begin_no_lock(tcl_ring); \
	} while (0)

#define ATH12K_HAL_SRNG_ACCESS_END(ab, tcl_ring) \
	ath12k_hal_srng_access_end_no_lock(ab, tcl_ring)
#endif

/**
 * struct ath12k_tx_sw_metadata - Software metadata stored alongside each TX descriptor
 * @skb: Pointer to the socket buffer being transmitted
 * @skb_ext_desc: Pointer to the extended descriptor slab object (if ext_kmem is set)
 * @paddr: 40-bit DMA physical address of the SKB data buffer
 * @len: 16-bit length of the SKB data in bytes
 * @mac_id: 5-bit MAC/radio identifier used to resolve the pdev at completion
 * @rsvd1: Reserved bits (padding to 64-bit boundary)
 * @paddr_ext_desc: 40-bit DMA physical address of the extended descriptor
 * @ext_desc_len: 16-bit length of the extended descriptor in bytes
 * @flags: 4-bit TX descriptor flags (e.g. DP_TX_DESC_FLAG_FAST, DP_TX_DESC_FLAG_RECYCLE)
 * @rsvd2: Reserved bits (padding to 64-bit boundary)
 *
 * Packed to exactly 32 bytes and stored in the TX status buffer alongside the
 * WBM completion ring entry. Carries all information needed to free the SKB
 * and unmap DMA buffers at TX completion time without accessing the original
 * SW TX descriptor.
 */
struct ath12k_tx_sw_metadata {
	struct sk_buff *skb;
	u64 paddr      : 40,
	    len        : 16,
	    hw_link_id : 5,
	    mmesh      : 1,
	    rsvd1      : 2;
	u8  flags      : 4,
	    rsvd2      : 4;
	u32 hw_enqueue_tstamp;
} __packed __aligned(32);

static_assert(sizeof(struct ath12k_tx_sw_metadata) == 32,
	      "size of struct ath12k_tx_sw_metadata is not 64 bytes!");

/**
 * struct ath12k_wifi7_tx_status_entry - Cached TX completion status entry
 * @tx_status: Copy of the WBM completion ring TX descriptor
 * @sw_metadata: Software metadata extracted from the SW TX descriptor
 * @tx_desc: Raw pointer to the SW TX descriptor (union with sw_metadata)
 *
 * Used to batch TX completion processing. WBM ring entries are first copied
 * into an array of these structures (under the ring lock), then processed
 * outside the lock. The union allows the tx_desc pointer to be stored before
 * sw_metadata is populated from the descriptor. Must be exactly 64 bytes.
 */
struct ath12k_wifi7_tx_status_entry {
	struct hal_wbm_completion_ring_tx tx_status;
	union {
		struct ath12k_tx_sw_metadata sw_metadata;
		void *tx_desc;
	};
} __packed;

static_assert(sizeof(struct ath12k_wifi7_tx_status_entry) == 64,
	      "size of struct ath12k_wifi7_tx_status_entry is not 64 bytes!");

static inline u32 ath12k_qos_get_metadata(u16 qos_id)
{
	u32 tcl_metadata = 0;

	tcl_metadata = u32_encode_bits(HTT_TCL_META_DATA_TYPE_SVC_ID_BASED,
				       HTT_TCL_META_DATA_TYPE_MISSION) |
			u32_encode_bits(1, HTT_TCL_META_DATA_SAWF_TID_OVERRIDE) |
			u32_encode_bits(qos_id, HTT_TCL_META_DATA_SAWF_SVC_ID);
	return tcl_metadata;
}

static inline u32 ath12k_qos_get_tcl_cmd(u32 msduq)
{
	u32 tid, flow_override, who_classify_info_sel, update = 0;

	tid = u32_get_bits(msduq, MSDUQ_TID);
	flow_override = u32_get_bits(msduq, MSDUQ_FLOW_OVERRIDE);
	who_classify_info_sel = u32_get_bits(msduq, MSDUQ_WHO_CL_INFO);

	update = u32_encode_bits(tid, HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE_EN) |
		 u32_encode_bits(who_classify_info_sel,
				 HAL_TCL_DATA_CMD_INFO3_CLASSIFY_INFO_SEL) |
		 u32_encode_bits(flow_override,
				 HAL_TCL_DATA_CMD_INFO3_FLOW_OVERRIDE);
	return update;
}

static inline
void ath12k_wifi_qos_desc(struct hal_tcl_data_cmd *desc,
			  u32 msduq, u16 qos_id)
{
	u32 meta_data_flags;

	desc->info3 |= ath12k_qos_get_tcl_cmd(msduq);
	meta_data_flags = ath12k_qos_get_metadata(qos_id);
	desc->info1 = u32_encode_bits(meta_data_flags,
				      HAL_TCL_DATA_CMD_INFO1_CMD_NUM);
}

static inline
void ath12k_wifi_qos_hlos_tid(struct hal_tcl_data_cmd *desc,
			      u8 tid)
{
	desc->info3 |= u32_encode_bits(tid < ATH12K_MAX_TID_VALUE ? tid : 0,
				       HAL_TCL_DATA_CMD_INFO3_TID) |
		 u32_encode_bits(1, HAL_TCL_DATA_CMD_INFO3_TID_OVERWRITE);
}

static inline u8 ath12k_get_qos_tag(u32 mark)
{
	u8 qos_tag = u32_get_bits(mark, QOS_TAG_MASK);

	if (qos_tag == QOS_SCS_TAG || qos_tag == QOS_MSCS_TAG)
		return qos_tag;

	return 0;
}

static void
ath12k_wifi7_dp_qos_update(struct ath12k_dp *dp, struct ath12k_pdev_dp *dp_pdev,
			   u32 mark, struct hal_tcl_data_cmd *desc, u8 qos_tag,
			   struct ath12k_dp_peer *dp_peer, u8 link_id)
{
	u8 scs_id;
	u8 msduq;
	u16 peer_id;
	u16 qos_id = QOS_ID_MAX;
	int ret;

	if (mark & SDWF_VALID_MASK) {
		/*
		 * dp_peer is NULL in this case
		 * hence find dp_peer using peer_id
		 */
		rcu_read_lock();
		peer_id = u32_get_bits(mark, SDWF_PEER_ID);
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return;
		}
		msduq = u32_get_bits(mark, SDWF_MSDUQ_ID);
		if (msduq >= MSDUQ_MAX_DEF)
			qos_id = dp_peer_msduq_qos_id(dp->ab, dp_peer->qos,
						      msduq);
		rcu_read_unlock();
	} else if (qos_tag == QOS_SCS_TAG) {
		scs_id = u32_get_bits(mark, QOS_QOS_ID_MASK);

		if (!dp_peer)
			return;

		ret = ath12k_dp_peer_scs_data(dp,
					      scs_id, dp_pdev->dp_hw,
					      &msduq, &qos_id,
					      dp_peer);
		if (ret != 0) {
			ath12k_err(dp->ab, "SCS Peer Data is NULL");
			return;
		}
	} else {
		msduq = u32_get_bits(mark, QOS_QOS_ID_MASK);
	}
	/* Update Desc for HLOS TID Override */
	if (msduq < MSDUQ_MAX_DEF) {
		ath12k_wifi_qos_hlos_tid(desc, msduq);
	} else if (msduq < QOS_MSDUQ_MAX) {
		/* Update Desc for User Defined QoS MSDUQ */
		ath12k_wifi_qos_desc(desc, msduq, qos_id);
	}
}

static void
ath12k_dp_sdwftx_ingress_stats_update(struct ath12k *ar,
				      u32 *skb_mark, u32 qos_nw_delay,
				      unsigned int skb_len)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_peer *dp_peer;
	u8 msduq;
	u16 peer_id, qos_id;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;

	if (!ar)
		return;

	if (!(*skb_mark & SDWF_VALID_MASK))
		return;

	msduq = u32_get_bits(*skb_mark, SDWF_MSDUQ_ID);

	if (ath12k_dp_stats_enabled(&ar->dp) &&
	    (ath12k_debugfs_is_qos_stats_enabled(ar) &
	    ATH12K_QOS_STATS_BASIC)) {
		peer_id = u32_get_bits(*skb_mark, SDWF_PEER_ID);

		if (!ar->dp.dp)
			return;

		dp = ar->dp.dp;

		rcu_read_lock();
		spin_lock_bh(&dp->dp_lock);

		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      peer_id);
		if (!dp_peer || !dp_peer->qos) {
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		qos_id = dp_peer_msduq_qos_id(ar->ab, dp_peer->qos, msduq);
		if (qos_id == QOS_ID_INVALID) {
			ath12k_err(ar->ab, "msduq_id: %u not yet reserved\n",
				   msduq);
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
			return;
		}

		ath12k_qos_tx_enqueue_peer_stats(dp_peer,
						 dp_pdev->hw_link_id,
						 msduq, skb_len);
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
	}

	/* Store the NWDELAY to skb->mark which can be fetched
	 * during tx completion.
	 */
	if (qos_nw_delay > QOS_NW_DELAY_MAX)
		qos_nw_delay = QOS_NW_DELAY_MAX;

	*skb_mark = SDWF_VALID_MASK |
		    (qos_nw_delay << QOS_NW_DELAY_SHIFT) | msduq;
}

#define HW_TX_DELAY_MAX				0x1000000

#define TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US	10
#define HW_TX_DELAY_MASK			0x1FFFFFFF
#define TX_COMPL_BUFFER_TSTAMP_US(TSTAMP) \
	(((TSTAMP) << TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US) & \
	 HW_TX_DELAY_MASK)

#define ATH12K_DP_SAWF_DELAY_BOUND_MS_MULTIPLER 1000
#define ATH12K_MOV_AVG_PKT_WIN	10

#define ATH12K_HIST_AVG_DIV	2

void ath12k_wifi7_compute_hw_delay(struct ath12k *ar, struct hal_tx_status *ts,
				   u32 *hw_delay)
{
	/* low 32 alone will be filled for TSF2 from FW and the value can be
	 * negative for both TSF2 and TQM delta
	 */
	int tmp_delta_tsf2 = ar->delta_tsf2, tmp_delta_tqm = ar->delta_tqm;
	u32 msdu_tqm_enqueue_tstamp_us, final_msdu_tqm_enqueue_tstamp_us;
	u32 msdu_compl_tsf_tstamp_us, final_msdu_compl_tsf_tstamp_us;
	struct ath12k_hw_group *ag = ar->ab->ag;
	/* MLO TSTAMP OFFSET can be negative
	 */
	int mlo_offset = ag->mlo_tstamp_offset;
	int delta_tsf2, delta_tqm;

	msdu_tqm_enqueue_tstamp_us =
		TX_COMPL_BUFFER_TSTAMP_US(ts->delay_stats.ts.buffer_timestamp);
	msdu_compl_tsf_tstamp_us = ts->delay_stats.ts.tsf;
	delta_tsf2 = mlo_offset - tmp_delta_tsf2;
	delta_tqm = mlo_offset - tmp_delta_tqm;

	final_msdu_tqm_enqueue_tstamp_us =
		(msdu_tqm_enqueue_tstamp_us + delta_tqm) & HW_TX_DELAY_MASK;
	final_msdu_compl_tsf_tstamp_us =
		(msdu_compl_tsf_tstamp_us + delta_tsf2) & HW_TX_DELAY_MASK;

	*hw_delay = (final_msdu_compl_tsf_tstamp_us -
			final_msdu_tqm_enqueue_tstamp_us) & HW_TX_DELAY_MASK;
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
static int
ath12k_wifi7_dp_prepare_mesh_htt_metadata(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  struct ath12k_dp_ext_desc *ext_desc,
					  struct ath12k_dp_tx_msdu_info *msdu_info)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct hal_tx_msdu_metadata *htt_desc_ext;
	struct meta_hdr_s *mhdr;
	u8 htt_desc_size;

	if (!(skb_cb->flags & ATH12K_SKB_MESH_TX_INFO)) {
		msdu_info->to_fw = 0;
		goto skip;
	}

	htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
	htt_desc_ext = (struct hal_tx_msdu_metadata *)
		    ath12k_dp_ext_desc_get_rsvd0(ext_desc);

	if (!htt_desc_ext)
		return -ENOMEM;

	msdu_info->to_fw = 1;
	msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;

	htt_desc_ext->info0 = le32_encode_bits
			      (1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);
	htt_desc_ext->info1 = le32_encode_bits
			      (1, HAL_TX_MSDU_METADATA_INFO1_UPDATE_PEER_CACHE);

	mhdr = (struct meta_hdr_s *)(skb->data - msdu_info->mhdr_len);

	if (!(mhdr->flags & METAHDR_FLAG_NOENCRYPT))
		htt_desc_ext->info5 =
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO5_LEARNING_FRAME);

	if (!(mhdr->flags & METAHDR_FLAG_AUTO_RATE)) {
		htt_desc_ext->info0 |=
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_MCS_MASK) |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_PWR)        |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_NSS_MASK)   |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_PREAM_TYPE) |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_BW_INFO)    |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_KEY_FLAGS)  |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_DYN_BW)     |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_RETRIES)    |
			le32_encode_bits(mhdr->rate_info[0].max_tries,
					 HAL_TX_MSDU_METADATA_INFO0_RETRY_LIMIT);

		htt_desc_ext->info1 |=
			le32_encode_bits(mhdr->power, HAL_TX_MSDU_METADATA_INFO1_POWER) |
			le32_encode_bits(mhdr->rate_info[0].mcs,
					 HAL_TX_MSDU_METADATA_INFO1_MCS_MASK) |
			le32_encode_bits(mhdr->rate_info[0].nss,
					 HAL_TX_MSDU_METADATA_INFO1_NSS_MASK) |
			le32_encode_bits(mhdr->rate_info[0].preamble_type,
					 HAL_TX_MSDU_METADATA_INFO1_PREAM_TYPE) |
			le32_encode_bits(1,
					 HAL_TX_MSDU_METADATA_INFO1_UPDATE_PEER_CACHE);

		htt_desc_ext->info2 |=
			le32_encode_bits((mhdr->keyix & 0x3),
					  HAL_TX_MSDU_METADATA_INFO2_KEY_FLAGS);
	}

	if (mhdr->flags & METAHDR_FLAG_NOENCRYPT) {
		htt_desc_ext->info0 |=
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE);
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MMESH, ATH12K_DBG_L1,
			 "skb %p pwr %d mcs 0x%x nss %d preamble %d",
			skb, mhdr->power, mhdr->rate_info[0].mcs, mhdr->rate_info[0].nss,
			mhdr->rate_info[0].preamble_type);

	ath12k_dbg_level(ab, ATH12K_DBG_MMESH, ATH12K_DBG_L1,
			 "retries %d auto %d encrypt %d\n",
			 mhdr->rate_info[0].max_tries,
			 !!(mhdr->flags & METAHDR_FLAG_AUTO_RATE),
			 !!(mhdr->flags & METAHDR_FLAG_NOENCRYPT));

	ath12k_dbg_level(ab, ATH12K_DBG_MMESH, ATH12K_DBG_L1,
			 "Meta hdr %0X %0X %0X %0X %0X %0X  to_fw %u",
			 htt_desc_ext->info0, htt_desc_ext->info1, htt_desc_ext->info2,
			 htt_desc_ext->info3, htt_desc_ext->info4, htt_desc_ext->info5,
			 msdu_info->to_fw);
skip:
	return 0;
}
#endif

static inline void
ath12k_core_dma_clean_range_no_dsb(const void *start, const void *end)
{
#ifndef CONFIG_IO_COHERENCY
#ifndef PLATFORM_SDX
	dmac_clean_range_no_dsb(start, end);
#endif
#endif
}

/**
 * ath12k_wifi7_dp_raw_mode_handler() - RAW mode feature handler
 * @dp_vif: DP virtual interface
 * @msdu_info: MSDU information
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi7_dp_raw_mode_handler(struct ath12k_dp_vif *dp_vif,
				 struct ath12k_dp *dp,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 u32 *len,
				 struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	if (!test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &dp->ab->ag->flags))
		return DP_TX_ERROR;

	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	if (skb->protocol == cpu_to_be16(ETH_P_ARP)) {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
	}

	/* Handle encryption in RAW mode */
	if (skb_cb->flags & ATH12K_SKB_CIPHER_SET) {
		msdu_info->ext_desc.encrypt_type =
			ath12k_dp_tx_get_encrypt_type(skb_cb->cipher);

		/* Add MIC for encrypted frames */
		if (ieee80211_has_protected(hdr->frame_control)) {
			skb_put(skb, IEEE80211_CCMP_MIC_LEN);
			*len = skb->len;
		}
	} else {
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi7_dp_nwifi_handler() - Native WiFi feature handler
 * @skb: Socket buffer
 *
 * Returns: Feature result code
 */
enum ath12k_dp_feature_result
ath12k_wifi7_dp_nwifi_handler(struct sk_buff *skb)
{
	ath12k_dp_tx_encap_nwifi(skb);
	return DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi7_dp_mac_encrpt_handler() - Dynamic VLAN (DVLAN) feature handler
 * @msdu_info: MSDU information structure to update with DVLAN settings
 *
 * Handles dynamic-VLAN tagged frames by setting the encapsulation type to RAW,
 * disabling encryption (OPEN), and directing the frame to firmware for
 * processing. Sets the HTT metadata flag and marks the frame for kernel
 * memory extension (ext_kmem) so an extended descriptor is allocated.
 *
 * Returns: DP_TX_FEATURE_SUCCESS always
 */
enum ath12k_dp_feature_result
ath12k_wifi7_dp_mac_encrpt_handler(struct ath12k_dp_tx_msdu_info *msdu_info)
{
	msdu_info->ext_desc.add_htt_metadata = true;
	msdu_info->ext_kmem = true;
	msdu_info->to_fw = true;
	msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
	msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;

	return  DP_TX_FEATURE_SUCCESS;
}

/**
 * ath12k_wifi7_dp_encap_mismatch_handler() - Encapsulation mismatch handler
 * @dp_vif: DP virtual interface with the configured TX encapsulation type
 * @dp: DP structure for accessing hardware parameters
 * @msdu_info: MSDU information structure to update with corrected encap settings
 * @skb: Socket buffer being transmitted
 *
 * Handles frames where the encapsulation type does not match the expected
 * type for the vdev. For Ethernet-mode vdevs: EAPOL frames are redirected
 * to firmware with HTT metadata; null-function frames are sent to FW with
 * RAW encapsulation.
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on invalid encap type
 */
enum ath12k_dp_feature_result
ath12k_wifi7_dp_encap_mismatch_handler(struct ath12k_dp_vif *dp_vif,
				       struct ath12k_dp *dp,
				       struct ath12k_dp_tx_msdu_info *msdu_info,
				       struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	msdu_info->ext_desc.encap_type = ath12k_dp_tx_get_encap_type(dp->ab, skb);
	if (msdu_info->ext_desc.encap_type >= HAL_TCL_ENCAP_TYPE_802_3)
		return DP_TX_ERROR;

	msdu_info->mpsk_diff_encap = true;
	msdu_info->is_null = ieee80211_is_any_nullfunc(hdr->frame_control);
	if (unlikely(dp_vif->tx_encap_type == ATH12K_HW_TXRX_ETHERNET)) {
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_ENCAP_OVERRIDE;
		msdu_info->to_fw = true;
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

		if (skb->protocol == cpu_to_be16(ETH_P_PAE)) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
			msdu_info->ext_desc.add_htt_metadata = true;
		} else if (msdu_info->is_null) {
			msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_RAW;
		}
	}
	return ret;
}

/**
 * ath12k_wifi7_dp_dma_align_handler() - DMA alignment feature handler
 * @dp_vif: DP virtual interface
 * @skb: Socket buffer
 *
 * Returns: None
 */
static inline
void ath12k_wifi7_dp_dma_align_handler(struct ath12k_dp *dp,
				       struct sk_buff *skb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	int ret;

	/* Check if alignment is needed */
	if (!iova_mask || !((unsigned long)skb->data & iova_mask))
		return;

	/* Align buffer */
	ret = ath12k_dp_tx_align_payload(dp, &skb);
	if (ret) {
		/* Continue with unaligned buffer */
		return;
	}
}

/**
 * ath12k_wifi7_dp_tx_hal_tcl_desc_update() - Populate HAL TCL data descriptor
 * @hal_tcl_desc: Pointer to the HAL TCL data command descriptor to fill
 * @msdu_info: MSDU information containing all descriptor field values
 * @skb: Socket buffer (unused, reserved for future use)
 *
 * Fills all fields of the HAL TCL data command descriptor from the
 * ath12k_dp_tx_msdu_info structure, including buffer address info (physical
 * address, RBM ID, SW cookie), descriptor type, bank ID, TCL metadata flags,
 * data length, TID, LMAC ID, vdev ID, AST index, and cache set number.
 */
static inline
void ath12k_wifi7_dp_tx_hal_tcl_desc_update(struct hal_tcl_data_cmd *hal_tcl_desc,
					    struct ath12k_dp_tx_msdu_info *msdu_info,
					    struct sk_buff *skb)
{
	hal_tcl_desc->buf_addr_info.info0 =
		le32_encode_bits(msdu_info->paddr, BUFFER_ADDR_INFO0_ADDR);
	hal_tcl_desc->buf_addr_info.info1 =
		le32_encode_bits(((uint64_t)msdu_info->paddr >> HAL_ADDR_MSB_REG_SHIFT),
				 BUFFER_ADDR_INFO1_ADDR);
	hal_tcl_desc->buf_addr_info.info1 |=
		le32_encode_bits((msdu_info->rbm_id), BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(msdu_info->desc_id, BUFFER_ADDR_INFO1_SW_COOKIE);

	hal_tcl_desc->info0 =
		le32_encode_bits(msdu_info->type, HAL_TCL_DATA_CMD_INFO0_DESC_TYPE) |
		le32_encode_bits(msdu_info->bank_id, HAL_TCL_DATA_CMD_INFO0_BANK_ID);

	hal_tcl_desc->info1 =
		le32_encode_bits(msdu_info->meta_data_flags,
				 HAL_TCL_DATA_CMD_INFO1_CMD_NUM);

	hal_tcl_desc->info2 = cpu_to_le32(msdu_info->flags0) |
		le32_encode_bits(msdu_info->data_len, HAL_TCL_DATA_CMD_INFO2_DATA_LEN) |
		le32_encode_bits(msdu_info->pkt_offset,
				 HAL_TCL_DATA_CMD_INFO2_PKT_OFFSET);

	hal_tcl_desc->info3 = cpu_to_le32(msdu_info->flags1) |
		le32_encode_bits(msdu_info->lmac_id, HAL_TCL_DATA_CMD_INFO3_PMAC_ID) |
		le32_encode_bits(msdu_info->vdev_id, HAL_TCL_DATA_CMD_INFO3_VDEV_ID);

	hal_tcl_desc->info4 =
		le32_encode_bits(msdu_info->lookup_override,
				 HAL_TCL_DATA_CMD_INFO4_IDX_LOOKUP_OVERRIDE) |
		le32_encode_bits(msdu_info->bss_ast_idx,
				 HAL_TCL_DATA_CMD_INFO4_SEARCH_INDEX) |
		le32_encode_bits(msdu_info->bss_ast_hash,
				 HAL_TCL_DATA_CMD_INFO4_CACHE_SET_NUM);
	hal_tcl_desc->info5 = 0;
}

/**
 * ath12k_wifi7_dp_tx_update_delay_stats() - Update SW/HW delay histograms at completion
 * @dp_pdev: DP pdev handle
 * @skb: Socket buffer (skb->tstamp holds the MAC TX entry time set by
 *        __net_timestamp() in mac_op_tx)
 * @ts: TX completion status (contains hardware timestamps)
 * @ring_id: TX ring index
 * @enqueue_tstamp: HW TCL enqueue time in microseconds, captured just before
 *                  the TCL ring is released in ath12k_wifi7_dp_tx_hw_enqueue()
 *                  and stored in tx_desc->hw_enqueue_tstamp.
 *
 * Called at TX completion time when VoW delay stats are enabled.
 * Updates:
 *   - swq_delay: SW enqueue delay (mac_op_tx entry -> HW TCL enqueue).
 *                Computed as enqueue_tstamp - entry_tstamp, where
 *                entry_tstamp is read from skb->tstamp via skb_get_ktime().
 *   - hwtx_delay: HW transmit delay (TQM enqueue -> TX completion), derived
 *                 from hardware timestamps in the WBM completion ring entry
 *                 via ath12k_sdwf_compute_hw_delay().
 *   - intfrm_delay: timestamp updated at enqueue time but delay is calulated
 *                   in this function
 *
 * Return: void
 */
static inline void
ath12k_wifi7_dp_tx_update_delay_stats(struct ath12k_pdev_dp *dp_pdev,
				      struct sk_buff *skb,
				      struct hal_tx_status *ts,
				      u8 ring_id,
				      u32 enqueue_tstamp)
{
	struct ath12k_tid_tx_stats *tid_tx;
	u32 sw_delay, hw_delay;
	u32 entry_tstamp;
	u32 intfrm_delay;
	u64 ingress_ts;
	u8 vow_tid;

	vow_tid = ath12k_vow_tid_validate(ts->tid);

	if (ring_id >= DP_TCL_NUM_RING_MAX)
		return;

	tid_tx = &dp_pdev->tid_stats.tid_tx[ring_id][vow_tid];

	entry_tstamp = (u32)ktime_to_us(skb_get_ktime(skb));
	if (enqueue_tstamp && entry_tstamp) {
		sw_delay = enqueue_tstamp - entry_tstamp;
		ath12k_dp_update_hist_stats(&tid_tx->swq_delay,
					    sw_delay / USEC_PER_MSEC);
	}

	ath12k_wifi7_compute_hw_delay(dp_pdev->ar, ts, &hw_delay);
	if (hw_delay <= HW_TX_DELAY_MAX)
		ath12k_dp_update_hist_stats(&tid_tx->hwtx_delay,
					    hw_delay / USEC_PER_MSEC);

	if (skb->tstamp) {
		ingress_ts = ktime_to_ms(skb->tstamp);

		if (dp_pdev->prev_tx_enq_tstamp &&
		    ingress_ts > dp_pdev->prev_tx_enq_tstamp) {
			intfrm_delay = (u32)(ingress_ts - dp_pdev->prev_tx_enq_tstamp);
			ath12k_dp_update_hist_stats(&tid_tx->intfrm_delay,
						    intfrm_delay);
		}
		dp_pdev->prev_tx_enq_tstamp = ingress_ts;
	}
}

/**
 * ath12k_wifi7_dp_tx_hw_enqueue() - Enqueue a frame to the TCL hardware ring
 * @dp_link_vif: DP link virtual interface
 * @dp_pdev: DP pdev structure for the transmitting radio
 * @msdu_info: MSDU information with all descriptor fields populated
 * @ring_id: TX ring index to enqueue to
 * @arsta: Link station pointer (optional, for QoS tag updates)
 * @skb: Socket buffer to enqueue
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 * @tx_desc: Tx Software Descriptor
 * @dp_vif: DP mld virtual interface
 * @feat_bypass: Feature Bypass flag for fast path statistics
 * @is_mcast: Flag for Multicast packet to increment stats
 *
 * Acquires the TCL ring, writes the TCL data descriptor, applies QoS
 * descriptor updates for SDWF/SCS-tagged frames, and releases the ring.
 * CONFIG_IO_COHERENCY variant writes directly to the ring entry; the
 * non-coherent variant uses a local copy and memcpy for cache safety.
 *
 * Returns: 0 on success, -ENOENT if no TCL descriptor is available
 */
#ifdef CONFIG_IO_COHERENCY
static inline
int ath12k_wifi7_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
				  struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_tx_msdu_info *msdu_info,
				  u8 ring_id,
				  struct ath12k_link_sta *arsta,
				  struct sk_buff *skb,
				  u32 qos_nw_delay,
				  struct ath12k_tx_desc_info *tx_desc,
				  struct ath12k_dp_vif *dp_vif,
				  bool feat_bypass, bool is_mcast,
				  struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	u32 len = msdu_info->data_len;
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		dp->device_stats.tx_err.desc_na[ring_id]++;
		return -ENOENT;
	}

	ath12k_wifi7_dp_tx_hal_tcl_desc_update(hal_tcl_desc, msdu_info, skb);
	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(hal_tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_wifi7_dp_qos_update(dp, dp_pdev, skb->mark, hal_tcl_desc,
					   0, dp_peer, dp_link_vif->link_id);
		ath12k_dp_sdwftx_ingress_stats_update(dp_pdev->ar,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
	}

	if (unlikely(dp_peer)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_wifi7_dp_qos_update(dp, dp_pdev, skb->mark,
						   hal_tcl_desc, qos_tag,
						   dp_peer, dp_link_vif->link_id);
	}

	ath12k_dmb();

	/* Update success statistics */
	if (likely(feat_bypass)) {
		DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, len, ring_id);
		dp_pdev->dp->device_stats.tx_fast_unicast[ring_id]++;
	} else {
		ath12k_dp_tx_stats_post_enqueue(dp, dp_pdev, dp_vif, skb,
						msdu_info, ring_id,
						len, is_mcast, tx_desc);
	}

	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#else
static inline
int ath12k_wifi7_dp_tx_hw_enqueue(struct ath12k_dp_link_vif *dp_link_vif,
				  struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_tx_msdu_info *msdu_info,
				  u8 ring_id,
				  struct ath12k_link_sta *arsta,
				  struct sk_buff *skb,
				  u32 qos_nw_delay,
				  struct ath12k_tx_desc_info *tx_desc,
				  struct ath12k_dp_vif *dp_vif,
				  bool feat_bypass, bool is_mcast,
				  struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_hal *hal = dp->hal;
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u8 hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	struct hal_srng *tcl_ring = &hal->srng_list[hal_ring_id];
	struct hal_tcl_data_cmd *hal_tcl_desc;
	struct hal_tcl_data_cmd tcl_desc = {0};
	u32 len = msdu_info->data_len;
	u8 qos_tag;

	msdu_info->rbm_id = hal->tcl_to_cmp_rbm_map[ring_id].rbm_id;
	hal_tcl_desc =
		(void *)ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(tcl_ring);

	if (!hal_tcl_desc) {
		ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
		return -ENOENT;
	}

	ath12k_wifi7_dp_tx_hal_tcl_desc_update(&tcl_desc, msdu_info, skb);

	if (unlikely(msdu_info->tid_override))
		ath12k_wifi_qos_hlos_tid(&tcl_desc, msdu_info->tid);

	if (unlikely(skb->mark & SDWF_VALID_MASK)) {
		ath12k_wifi7_dp_qos_update(dp, dp_pdev, skb->mark, &tcl_desc,
					   0, dp_peer, dp_link_vif->link_id);
		ath12k_dp_sdwftx_ingress_stats_update(dp_pdev->ar,
						      &skb->mark,
						      qos_nw_delay,
						      skb_headlen(skb));
	}

	if (unlikely(dp_peer)) {
		qos_tag = ath12k_get_qos_tag(skb->mark);
		if (qos_tag)
			ath12k_wifi7_dp_qos_update(dp, dp_pdev, skb->mark,
						   &tcl_desc, qos_tag,
						   dp_peer, dp_link_vif->link_id);
	}

	memcpy(hal_tcl_desc, &tcl_desc, sizeof(tcl_desc));
	ath12k_dmb();

	/* Update success statistics */
	if (likely(feat_bypass)) {
		DP_STATS_INC_PKT(dp_vif, tx_i.enque_to_hw_fast, 1, len, ring_id);
		dp_pdev->dp->device_stats.tx_fast_unicast[ring_id]++;
	} else {
		ath12k_dp_tx_stats_post_enqueue(dp, dp_pdev, dp_vif, skb,
						msdu_info, ring_id,
						len, is_mcast, tx_desc);
	}

	ath12k_hal_srng_access_umac_src_ring_end_nolock_fast(tcl_ring);
	return 0;
}
#endif

void ucast_enable_ast_override(struct ath12k_link_sta *arsta,
			       u16 *bss_ast_idx, u16 *bss_ast_hash,
			       bool *lookup_override)
{
	if (arsta) {
		*bss_ast_idx = arsta->ast_idx;
		*bss_ast_hash = arsta->ast_hash;
		*lookup_override = true;
	}
}

/**
 * ath12k_wifi7_ucast_setup_msdu_info() - Setup MSDU info for unicast transmission
 * @dp_link_vif: DP link virtual interface containing bank, lmac, vdev, and AST info
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to fill with link vif parameters
 * @skb: Socket buffer (unused, reserved for future use)
 *
 * Populates the MSDU info structure with unicast-specific fields from the
 * DP link virtual interface: bank ID, TCL metadata, LMAC ID, vdev ID,
 * BSS AST index/hash, and descriptor type (HAL_TCL_DESC_TYPE_BUFFER).
 * The lookup_override flag is cleared for normal unicast address lookup.
 */
static void ath12k_wifi7_ucast_setup_msdu_info(struct ath12k_dp_link_vif *dp_link_vif,
					       struct ath12k_dp_tx_msdu_info *msdu_info,
					       struct sk_buff *skb, bool htt_mesh,
					       struct ath12k_link_sta *arsta,
					       struct ath12k_base *ab)
{
	u16 bss_ast_idx;
	u16 bss_ast_hash;
	bool lookup_override;

	if (!msdu_info)
		return;

	/* default: use main link vif */
	bss_ast_idx = dp_link_vif->ast_idx;
	bss_ast_hash = dp_link_vif->ast_hash;
	lookup_override = false;

	if (unlikely(test_bit(ATH12K_FLAG_UCAST_ENABLE_AST_OVERRIDE, &ab->dev_flags)))
		ucast_enable_ast_override(arsta, &bss_ast_idx, &bss_ast_hash,
					  &lookup_override);

	msdu_info->bss_ast_idx = bss_ast_idx;
	msdu_info->bss_ast_hash = bss_ast_hash;
	msdu_info->lookup_override = lookup_override;

	msdu_info->bank_id = dp_link_vif->bank_id;
	msdu_info->meta_data_flags = dp_link_vif->tcl_metadata;
	msdu_info->lmac_id = dp_link_vif->lmac_id;
	msdu_info->vdev_id = dp_link_vif->vdev_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->ext_kmem = htt_mesh;
	msdu_info->htt_mesh = htt_mesh;
}

/**
 * ath12k_wifi7_dp_get_ring_id() - Select TX ring based on SKB
 * @dp: DP structure containing hardware parameters and ring selector callback
 * @ring_id: Output ring ID, set to the selected ring index
 * @skb: Socket buffer used as input to the hardware-specific ring selector
 *
 * Calls the hardware-specific get_ring_selector() callback (e.g. CPU ID for
 * QCN9274, queue mapping for WCN7850) and wraps the result modulo the
 * maximum number of TX rings configured for this hardware.
 */
static inline
void ath12k_wifi7_dp_get_ring_id(struct ath12k_dp *dp, u8 *ring_id,
				 struct sk_buff *skb)
{
	u8 ring_selector;

	ring_selector = dp->hw_params->hw_ops->get_ring_selector(skb);
	*ring_id = ring_selector % dp->hw_params->max_tx_ring;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	if (unlikely(*ring_id == ATH12K_IPA_TCL_RING))
		*ring_id = ATH12K_IPA_TCL_SW_RING;
#endif
}

/**
 * ath12k_wifi7_dp_tx_process_features() - Process feature bitmap
 * @dp_vif: DP virtual interface
 * @dp: DP structure
 * @skb: Socket buffer
 * @msdu_info: MSDU info
 *
 * Returns: DP_TX_FEATURE_SUCCESS to continue, DP_TX_DROP to drop
 */
static enum ath12k_dp_feature_result
ath12k_wifi7_dp_tx_process_features(struct ath12k_dp_vif *dp_vif,
				    struct ath12k_pdev_dp *dp_pdev,
				    struct sk_buff *skb,
				    u32 *len,
				    struct ath12k_dp_tx_msdu_info *msdu_info,
				    struct ath12k_dp_skb_ctrl *skb_ctrl)
{
	enum ath12k_dp_feature_result ret = DP_TX_FEATURE_SUCCESS;

	ath12k_wifi7_dp_dma_align_handler(dp_pdev->dp, skb);
	/* Process SW encrpt feature */
	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features, DP_FEATURE_SW_ENCRPT)) {
		ret = ath12k_wifi7_dp_mac_encrpt_handler(msdu_info);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features,
				   DP_FEATURE_ENCAP_MISMATCH_HANDLE)) {
		ret = ath12k_wifi7_dp_encap_mismatch_handler(dp_vif, dp_pdev->dp,
							     msdu_info, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process Native WiFi feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_NATIVE_WIFI)) {
		ret = ath12k_wifi7_dp_nwifi_handler(skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* Process RAW mode feature */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_RAW_MODE)) {
		ret = ath12k_wifi7_dp_raw_mode_handler(dp_vif, dp_pdev->dp,
						       msdu_info, len, skb);
		if (ret == DP_TX_ERROR)
			return ret;
	}

	/* HLOS TID override */
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_HLOS)) {
		if (unlikely(skb->priority)) {
			msdu_info->tid = skb->priority;
			msdu_info->tid_override = true;
		}
	}
	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_ME)) {
		if (!ath12k_dp_me_tx(dp_vif, skb, msdu_info))
			return DP_TX_RETURN;
	}

	if (ath12k_dp_feature_enabled(dp_vif, DP_FEATURE_MESH)) {
		msdu_info->ext_desc.ext_feature |= DP_EXT_MESH;
		msdu_info->ext_desc.encap_type = HAL_TCL_ENCAP_TYPE_ETHERNET;
		msdu_info->ext_desc.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
	}

	/* Process Scatter-Gather Feature */
	if (DP_SKB_FEATURE_ENABLED(skb_ctrl->features, DP_FEATURE_SG)) {
		msdu_info->ext_kmem = true;
		msdu_info->ext_desc.ext_feature |= DP_EXT_SG;
		msdu_info->to_fw = 0;
	}

	return ret;
}

/**
 * ath12k_wifi7_dp_tx_update_gsn_metadata() - Embed GSN into TCL metadata
 * @msdu_info: MSDU information structure whose meta_data_flags will be updated
 * @dp_link_vif: DP link virtual interface; checked for NAWDS support
 * @mcbc_gsn: Global Sequence Number to embed in the TCL metadata
 *
 * Encodes the Global Sequence Number (GSN) into the TCL metadata flags for
 * multicast/broadcast frames in MLO mode. Sets the HTT extended present bit
 * and the GSN type. If NAWDS (Non-Associated Wireless Distribution System)
 * is enabled on the link vif, also sets the GSN-inspected bit.
 */

static
void ath12k_wifi7_dp_tx_update_gsn_metadata(struct ath12k_dp_tx_msdu_info *msdu_info,
					    struct ath12k_dp_link_vif *dp_link_vif,
					    int mcbc_gsn)
{
	msdu_info->meta_data_flags |=
		u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
				HTT_TCL_META_DATA_TYPE) |
		u32_encode_bits(mcbc_gsn,
				HTT_TCL_META_DATA_GLOBAL_SEQ_NUM);

	if (dp_link_vif->nawds_support)
		msdu_info->meta_data_flags |=
			u32_encode_bits(1, HTT_TCL_META_DATA_GSN_INSPECTED);

	msdu_info->meta_data_flags |= HTT_TCL_META_DATA_GLOBAL_HTT_EXT_PRESENT;
}

#define HTT_META_DATA_ALIGNMENT 0x8

static void
ath12k_wifi7_dp_tx_set_group_htt_metadata(struct hal_tx_msdu_metadata *htt_desc,
					  struct ath12k_dp_tx_msdu_info *msdu_info)
{
	htt_desc->info0 |=
		le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL) |
		le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_VALID_KEY_FLAGS);
	htt_desc->info2 |=
		le32_encode_bits(msdu_info->group_slot,
				 HAL_TX_MSDU_METADATA_INFO2_KEY_FLAGS);
}

static int
ath12k_wifi7_dp_prepare_group_htt_metadata(struct sk_buff *skb,
					   struct ath12k_dp_tx_msdu_info *msdu_info,
					   struct ath12k_dp_link_vif *dp_link_vif,
					   bool gsn_valid, int gsn)
{
	struct hal_tx_msdu_metadata *htt_desc;
	u8 align_pad, htt_desc_size, htt_hdr_size;

	align_pad = (unsigned long)skb->data & (HTT_META_DATA_ALIGNMENT - 1);
	htt_desc_size = ALIGN(sizeof(*htt_desc), HTT_META_DATA_ALIGNMENT);
	htt_hdr_size = align_pad + htt_desc_size;

	htt_desc = ath12k_dp_metadata_align_skb_head(skb, htt_hdr_size);
	if (!htt_desc)
		return -ENOMEM;

	msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
	ath12k_wifi7_dp_tx_set_group_htt_metadata(htt_desc, msdu_info);

	if (gsn_valid)
		ath12k_wifi7_dp_tx_update_gsn_metadata(msdu_info, dp_link_vif, gsn);

	msdu_info->pkt_offset = htt_hdr_size;
	msdu_info->data_len = skb->len - htt_hdr_size;
	msdu_info->to_fw = true;

	return 0;
}

/**
 * ath12k_wifi7_dp_ext_desc_populate() - Allocate and populate extended TX descriptor
 * @dp: DP structure for slab cache access and DMA mapping
 * @dp_link_vif: DP link virtual interface (for NAWDS and GSN metadata)
 * @msdu_info: MSDU information with ext_feature type and buffer addresses
 * @tx_desc: SW TX descriptor to update with ext_desc pointer and physical address
 * @gsn_valid: Whether the Global Sequence Number is valid for MLO multicast
 * @gsn: Global Sequence Number value to embed in metadata
 *
 * Allocates an extended descriptor from the ext_cache slab, then populates
 * it based on the ext_feature type:
 *  - DP_EXT_ME5: copies destination MAC to spare area, sets buf0/buf1
 *  - DP_EXT_ENCAP_OVERRIDE: sets buf0 and applies encap/encrypt override
 *  - DP_EXT_TSO/DP_EXT_SG: reserved for future use
 * Optionally adds HTT MSDU metadata for DVLAN frames or VLAN group key frames.
 * DMA-maps the descriptor and updates the SW TX descriptor fields.
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on allocation failure
 */
static int
ath12k_wifi7_dp_ext_desc_populate(struct ath12k_dp *dp,
				  struct ath12k_dp_vif *dp_vif,
				  struct ath12k_dp_link_vif *dp_link_vif,
				  struct sk_buff *skb,
				  struct ath12k_dp_tx_msdu_info *msdu_info,
				  struct ath12k_tx_desc_info *tx_desc,
				  bool gsn_valid, int gsn,
				  u8 ring_id)
{
	struct ath12k_dp_ext_desc *ext_desc = NULL;
	struct ath12k_dp_ext_desc_msdu_info *ext_msdu_info =
		&msdu_info->ext_desc;
	struct hal_tx_msdu_metadata *htt_desc_ext = NULL;
	u8 *mac_addr;
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	int ret;
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */
	u8 htt_desc_size;

	/* Allocate extended descriptor */
	ext_desc = kmem_cache_alloc(dp->ext_cache, GFP_DMA | __GFP_ZERO);
	if (!ext_desc) {
		ath12k_warn(dp->ab, "Ext Descriptor not allocated\n");
		return -ENOMEM;
	}
	memset(ext_desc, 0, ATH12K_DP_EXT_DESC_SZ);

	switch (msdu_info->ext_desc.ext_feature) {
	case DP_EXT_ME5:
		mac_addr = ath12k_dp_ext_desc_get_spare(ext_desc, ETH_ALEN);
		ether_addr_copy(mac_addr, msdu_info->ext_desc.peer_mac_addr);
		ath12k_dp_ext_desc_set_buf0(ext_desc,
					    virt_to_phys(mac_addr), ETH_ALEN);
		msdu_info->paddr += ETH_ALEN;
		ath12k_dp_ext_desc_set_buf1(ext_desc, msdu_info->paddr,
					    (msdu_info->data_len - ETH_ALEN));
		msdu_info->data_len = ATH12K_DP_EXT_DESC_SZ;
		break;
	case DP_EXT_ENCAP_OVERRIDE:
	case DP_EXT_MESH:
		ath12k_dp_ext_desc_set_buf0(ext_desc, msdu_info->paddr,
					    msdu_info->data_len);
		ath12k_dp_ext_desc_override_set(&ext_desc->desc,
						ext_msdu_info);
		msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ;
		if (msdu_info->ext_desc.ext_feature == DP_EXT_MESH)
			msdu_info->data_len = ATH12K_DP_EXT_DESC_SZ;
		break;
	case DP_EXT_TSO:
	case DP_EXT_SG:
		if (ath12k_dp_sg_ext_desc_populate(dp, dp_vif, ext_desc,
							 skb, ring_id))
			goto fail_free_ext_desc;
		tx_desc->is_from_sg = 1;
		break;
	default:
		break;
	}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	if (msdu_info->htt_mesh) {
		ret = ath12k_wifi7_dp_prepare_mesh_htt_metadata(dp->ab, tx_desc->skb,
								ext_desc, msdu_info);
		msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
		if (ret < 0) {
			ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
				   "Failed to add HTT meta data, dropping packet\n");
			goto fail_free_ext_desc;
		}
		msdu_info->to_fw = true;
	}
#endif

	if (msdu_info->ext_desc.add_htt_metadata) {
		msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;
		htt_desc_size = sizeof(struct hal_tx_msdu_metadata);

		htt_desc_ext = (struct hal_tx_msdu_metadata *)
				ath12k_dp_ext_desc_get_rsvd0(ext_desc);
		if (!htt_desc_ext)
			goto fail_free_ext_desc;
		htt_desc_ext->info0 |=
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_FLAG) |
			le32_encode_bits(0, HAL_TX_MSDU_METADATA_INFO0_ENCRYPT_TYPE) |
			le32_encode_bits(1, HAL_TX_MSDU_METADATA_INFO0_HOST_TX_DESC_POOL);

		msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;
		msdu_info->to_fw = true;
	}

	if (msdu_info->group_slot > 0 && msdu_info->mpsk_diff_encap) {
		htt_desc_size = sizeof(struct hal_tx_msdu_metadata);
		htt_desc_ext = (struct hal_tx_msdu_metadata *)
				ath12k_dp_ext_desc_get_rsvd0(ext_desc);
		if (!htt_desc_ext)
			goto fail_free_ext_desc;

		ath12k_wifi7_dp_tx_set_group_htt_metadata(htt_desc_ext, msdu_info);
		msdu_info->meta_data_flags |= HTT_TCL_META_DATA_VALID_HTT;

		if (gsn_valid)
			ath12k_wifi7_dp_tx_update_gsn_metadata(msdu_info,
							       dp_link_vif, gsn);

		msdu_info->data_len = ATH12K_TX_MSDU_EXT_SZ + htt_desc_size;
		msdu_info->to_fw = true;
	}

	msdu_info->type = HAL_TCL_DESC_TYPE_EXT_DESC;
	msdu_info->paddr = ath12k_dp_ext_desc_map(dp, ext_desc);

	tx_desc->ext_kmem = msdu_info->ext_kmem;
	tx_desc->paddr_ext_desc = msdu_info->paddr;
	tx_desc->ext_desc = ext_desc;
	tx_desc->ext_desc_len = ATH12K_DP_EXT_DESC_SZ;

	return 0;

fail_free_ext_desc:
	if (ext_desc)
		kmem_cache_free(dp->ext_cache, ext_desc);

	return -ENOMEM;
}

/**
 * ath12k_wifi7_dp_tx_desc_populate() - Populate SW TX descriptor fields
 * @dp: DP structure (passed to ath12k_wifi7_dp_ext_desc_populate if needed)
 * @dp_link_vif: DP link virtual interface (passed to ext_desc_populate)
 * @msdu_info: MSDU information with DMA address, length, and ext_kmem flag
 * @tx_desc: SW TX descriptor to populate
 * @gsn_valid: Whether the Global Sequence Number is valid
 * @gsn: Global Sequence Number value
 *
 * Stores the DMA physical address, data length, and FW-redirect flag into
 * the SW TX descriptor. If extended kernel memory is required (ext_kmem is
 * set in msdu_info), allocates and populates an extended descriptor via
 * ath12k_wifi7_dp_ext_desc_populate().
 *
 * Returns: DP_TX_FEATURE_SUCCESS on success, DP_TX_ERROR on ext_desc failure
 */
static enum ath12k_dp_feature_result
ath12k_wifi7_dp_tx_desc_populate(struct ath12k_pdev_dp *dp_pdev,
				 struct sk_buff *skb,
				 struct ath12k_dp_vif *dp_vif,
				 struct ath12k_dp_link_vif *dp_link_vif,
				 struct ath12k_dp_tx_msdu_info *msdu_info,
				 struct ath12k_tx_desc_info *tx_desc,
				 bool gsn_valid, int gsn,
				 u8 ring_id)
{
	int ret = 0;

	tx_desc->len = msdu_info->data_len;
	tx_desc->skb = skb;
	tx_desc->hw_link_id = dp_pdev->hw_link_id;

	if (msdu_info->ext_kmem)
		ret = ath12k_wifi7_dp_ext_desc_populate(dp_pdev->dp, dp_vif,
							dp_link_vif,
							skb, msdu_info, tx_desc,
							gsn_valid, gsn, ring_id);

	if (msdu_info->to_fw) {
		msdu_info->flags0 |= u32_encode_bits(1,
				     HAL_TCL_DATA_CMD_INFO2_TO_FW);
		tx_desc->to_fw = msdu_info->to_fw;
	}

	return ret;
}

/**
 * ath12k_wifi7_mcbc_get_gsn() - Atomically increment and return the multicast GSN
 * @dp_vif: DP virtual interface whose mcbc_gsn counter is incremented
 *
 * Atomically increments the per-vif multicast/broadcast Global Sequence Number
 * counter and returns the lower 12 bits. The 12-bit wrap-around ensures the
 * value fits in the HTT TCL metadata GSN field.
 *
 * Returns: 12-bit GSN value (0..4095)
 */
static u16 ath12k_wifi7_mcbc_get_gsn(struct ath12k_dp_vif *dp_vif)
{
	return atomic_inc_return(&dp_vif->mcbc_gsn) & 0xfff;
}

/**
 * ath12k_wifi7_mcbc_setup_msdu_info() - Setup MSDU info for multicast/broadcast
 * @dp_link_vif: DP link virtual interface with bank, lmac, vdev, and AST info
 * @arsta: Link station pointer (optional; used for 4-addr/WDS frames)
 * @dp: DP structure (unused, reserved for future use)
 * @msdu_info: MSDU info structure to populate
 * @gsn: Global Sequence Number for MLO multicast reinjection
 * @gsn_valid: Whether the GSN should be embedded in TCL metadata
 * @skb: Socket buffer; header parsed for 4-addr detection in non-Ethernet mode
 * @is_eth: True if the frame is in Ethernet encapsulation format
 *
 * Populates the MSDU info for multicast/broadcast transmission. For 4-addr
 * (WDS) Ethernet frames, uses the station's AST info and sets lookup_override.
 * For 4-addr native WiFi frames, redirects to firmware. Otherwise uses the
 * BSS AST info from the link vif. If GSN is valid and lookup_override is not
 * set, embeds the GSN into the TCL metadata for MLO multicast reinjection.
 *
 * Returns: 0 always
 */
static int ath12k_wifi7_mcbc_setup_msdu_info(struct ath12k_dp_link_vif *dp_link_vif,
					     struct ath12k_link_sta *arsta,
					     struct ath12k_dp *dp,
					     struct ath12k_dp_tx_msdu_info *msdu_info,
					     u16 gsn, bool gsn_valid,
					     struct sk_buff *skb,
					     bool is_eth, bool htt_mesh)
{
	struct ieee80211_hdr *hdr = NULL;

	if (!is_eth)
		hdr = (void *)skb->data;

	msdu_info->bank_id = dp_link_vif->bank_id;
	msdu_info->lmac_id = dp_link_vif->lmac_id;
	msdu_info->vdev_id = dp_link_vif->vdev_id;
	msdu_info->type = HAL_TCL_DESC_TYPE_BUFFER;
	msdu_info->htt_mesh = htt_mesh;
	msdu_info->ext_kmem = htt_mesh;

	/* Setup AST info */
	if (arsta && is_eth) {
		msdu_info->meta_data_flags = arsta->tcl_metadata;
		msdu_info->bss_ast_hash = arsta->ast_hash;
		msdu_info->bss_ast_idx = arsta->ast_idx;
		msdu_info->lookup_override = true;
	} else if (arsta && hdr && ieee80211_has_a4(hdr->frame_control)) {
		msdu_info->meta_data_flags = arsta->tcl_metadata;
		msdu_info->flags0 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TO_FW, 1);
	} else {
		msdu_info->meta_data_flags = dp_link_vif->tcl_metadata;
	}

	if (!msdu_info->lookup_override) {
		msdu_info->bss_ast_hash = dp_link_vif->ast_hash;
		msdu_info->bss_ast_idx = dp_link_vif->ast_idx;
	}

	if (!msdu_info->lookup_override && gsn_valid) {
		/* Setup GSN metadata */
		msdu_info->meta_data_flags =
			u32_encode_bits(HTT_TCL_META_DATA_TYPE_GLOBAL_SEQ_NUM,
					HTT_TCL_META_DATA_TYPE) |
			u32_encode_bits(gsn, HTT_TCL_META_DATA_GLOBAL_SEQ_NUM);
		if (dp_link_vif->nawds_support)
			msdu_info->meta_data_flags |= u32_encode_bits(1,
					HTT_TCL_META_DATA_GSN_INSPECTED);
	}

	return 0;
}

/**
 * ath12k_wifi7_mcbc_setup_encryption() - Setup encryption context for multicast
 * @dp_vif: DP virtual interface
 * @dp_pdev: DP pdev for the transmitting radio (provides the ath12k pointer)
 * @link_id: MLO link identifier
 * @skb: Socket buffer whose skb_cb will be populated with cipher/link info
 * @is_sta: True if the transmitting vdev is in STA mode (skips group slot lookup)
 * @msdu_info: MSDU info used to carry group slot metadata
 * @vlan_vif: VLAN virtual interface for group key slot lookup (may be NULL)
 *
 * Resolves the ath12k link vif for the given link_id, sets the skb_cb fields
 * (ar, link_id, vif), and looks up the BSS peer to find the current multicast
 * key. If a key is found, sets the cipher and ATH12K_SKB_CIPHER_SET flag.
 * For non-STA vdevs, also resolves the VLAN group key slot.
 *
 * Returns: 0 on success, -ENOENT if ar, arvif, or BSS peer is not found
 */
static int ath12k_wifi7_mcbc_setup_encryption(struct ath12k_dp_vif *dp_vif,
					      struct ath12k_pdev_dp *dp_pdev,
					      u8 link_id,
					      struct sk_buff *skb,
					      bool is_sta, bool is_eth,
					      struct ath12k_dp_tx_msdu_info *msdu_info,
					      struct ieee80211_tx_info *info,
					      struct ieee80211_vif *vlan_vif,
					      struct ieee80211_sta *sta)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_key_conf *key;
	struct ath12k *ar;

	if (!is_eth && !sta)
		ath12k_mlo_mcast_update_tx_link_address(ahvif->vif, link_id,
							skb, info->flags);
	/* Get AR from link */
	ar = dp_pdev->ar;
	if (!ar)
		return -ENOENT;

	arvif = rcu_dereference(ahvif->link[link_id]);
	if (unlikely(!arvif))
		return -ENOENT;

	skb_cb->u.ar = ar;
	skb_cb->link_id = link_id;
	skb_cb->vif = ahvif->vif;

	/* Skip for open mode */
	if (unlikely(arvif->key_cipher == WMI_CIPHER_NONE))
		return 0;

	spin_lock_bh(&dp_pdev->dp_hw->peer_hash_lock);
	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr(dp_pdev->dp_hw, sta->addr,
						      ahvif->vif);
	else
		dp_peer = ath12k_dp_peer_find_by_addr(dp_pdev->dp_hw, arvif->bssid,
						      ahvif->vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_pdev->dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	/* Get multicast key */
	spin_lock_bh(&dp_peer->keys_lock);

	key = dp_peer->keys[dp_peer->mcast_keyidx];
	if (key) {
		skb_cb->cipher = key->cipher;
		skb_cb->flags |= ATH12K_SKB_CIPHER_SET;
	}

	spin_unlock_bh(&dp_peer->keys_lock);
	spin_unlock_bh(&dp_pdev->dp_hw->peer_hash_lock);

	if (!is_sta && vlan_vif && vlan_vif->type == NL80211_IFTYPE_AP_VLAN)
		msdu_info->group_slot =
			ath12k_dp_tx_get_mcast_group_slot(ath12k_vif_to_ahvif(vlan_vif),
							  link_id);

	return 0;
}

/**
 * ath12k_wifi7_tx_validate_recovery() - Check recovery state
 * @ab: ath12k_base
 *
 * Returns: true if in recovery, false otherwise
 */
static bool ath12k_wifi7_tx_validate_recovery(struct ath12k_base *ab)
{
	return unlikely(test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS,
			&ab->dev_flags) ||
			test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags));
}


/**
 * ath12k_wifi7_mcbc_handler() - Multicast/Broadcast packet transmission handler
 * @dp_vif: DP virtual interface
 * @arsta: ath12k_link station pointer (optional; used for 4-addr/WDS frames)
 * @skb: Socket buffer to transmit (will be cloned/copied per active link)
 * @is_eth: True if the frame is in Ethernet encapsulation format
 * @gsn_valid: True if a Global Sequence Number should be assigned (MLO mcast)
 * @is_sta: True if the transmitting vdev is in STA mode
 * @vlan_vif: VLAN virtual interface for group key slot lookup (may be NULL)
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Iterates over all active MLO links of the vif. For each link, clones or
 * copies the SKB, sets up encryption context, populates MSDU info, processes
 * the TX feature bitmap, assigns a TX descriptor, DMA-maps the buffer, and
 * enqueues the frame to the TCL hardware ring. Frames are dropped per-link
 * on any error without affecting other links.
 */
void ath12k_wifi7_mcbc_handler(struct ath12k_dp_vif *dp_vif,
			       u8 link_id,
			       struct ath12k_link_sta *arsta,
			       struct sk_buff *skb,
			       bool is_eth,
			       bool gsn_valid,
			       bool is_sta,
			       struct ieee80211_vif *vlan_vif,
			       struct ath12k_dp_skb_ctrl *skb_ctrl,
			       struct ieee80211_tx_info *info,
			       u32 qos_nw_delay, bool htt_mesh,
			       struct ieee80211_sta *sta)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_link_vif *dp_link_vif;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	struct sk_buff *skb_new;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	unsigned long links_map = 0;
	unsigned long filtered_links_map;
	u16 gsn = 0;
	u8 ring_id = 0;
	enum ath12k_dp_feature_result feature_ret;
	enum ath12k_dp_tx_enq_error err;
	u32 len;
	int ret;

	/* Get active links */
	if (gsn_valid) {
		links_map = dp_vif->links_map;
		/* Apply per-packet mcast link bitmap filter if enabled */
		if (ath12k_mcast_link_bmap_enable &&
		    (skb_cb->flags & ATH12K_SKB_MCAST_LINK_BMAP_VALID)) {
			filtered_links_map = links_map &
				(skb_cb->mcast_link_bmap &
				 ATH12K_MCAST_LINK_BMAP_MASK);
			if (filtered_links_map)
				links_map = filtered_links_map;
		}
		/* Get GSN */
		gsn = ath12k_wifi7_mcbc_get_gsn(dp_vif);
	} else {
		set_bit(link_id, &links_map);
	}

	/* Update entry statistics */
	DP_STATS_INC_PKT(dp_vif, tx_i.recv_from_stack, 1, skb->len, ring_id);


	/*
	 * Iterate over links_map using explicit bitmap traversal.
	 *
	 * For gsn_valid case, only the first set link_id is processed.
	 * For non-gsn_valid case, all valid (set) link_ids in the VIF
	 * are iterated sequentially.
	 *
	 * find_first_bit() is used to get the initial link_id and
	 * find_next_bit() is used to traverse remaining set bits.
	 * The loop terminates when no more set bits are found
	 * (i.e., link_id >= IEEE80211_MLD_MAX_NUM_LINKS).
	 */

	/*
	 * Note: Bridge vdev are excluded in loop iteration.
	 * For valid STA scenarios, the corresponding bridge link
	 * will be selected and used instead.
	 */

	do {
		struct ath12k_link_vif *arvif =
			rcu_dereference(ahvif->link[link_id]);
		struct ath12k_base *ab = NULL;

		if (!arvif || !arvif->is_up) {
			DP_STATS_INC(dp_vif,
					tx_i.drop[DP_TX_ENQ_DROP_INV_ARVIF],
					1, ring_id);
			goto next;
		}

		dp_link_vif = &dp_vif->dp_link_vif[link_id];

		if (!dp_link_vif) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_INV_ARVIF],
				     1, ring_id);
			goto next;
		}

		ab = dp_link_vif->ab;
		if (!ab)
			goto next;

		dp_pdev = ath12k_dp_to_dp_pdev(ab->dp,
					       dp_link_vif->pdev_idx);
		if (!dp_pdev) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_INV_PDEV],
				     1, ring_id);
			goto next;
		}

		if (unlikely(ath12k_dp_stats_enabled(dp_pdev)))
			if (unlikely(ath12k_dp_latency_stats_enabled(dp_pdev)))
				__net_timestamp(skb);

		ath12k_wifi7_dp_get_ring_id(dp_pdev->dp, &ring_id, skb);
		dp = dp_pdev->dp;

		if (ath12k_wifi7_tx_validate_recovery(dp->ab)) {
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_FW_RECOVERY],
				     1, ring_id);
			return;
		}

		if (is_eth) {
			skb_new = skb_clone(skb, GFP_ATOMIC);
			if (!skb_new)
				goto next;
		} else {
			skb_new = skb_copy(skb, GFP_ATOMIC);
			if (!skb_new)
				goto next;
		}

		len = skb_new->len;

		msdu_info.group_slot = -1;
		msdu_info.mpsk_diff_encap = 0;
		ret = ath12k_wifi7_mcbc_setup_encryption(dp_vif, dp_pdev,
							 link_id, skb_new,
							 is_sta, is_eth,
							 &msdu_info,
							 info, vlan_vif, sta);
		if (ret) {
			dev_kfree_skb_any(skb_new);
			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_MCBC_ENCRY_FAIL],
				     1, ring_id);
			goto next;
		}

		msdu_info.qos_nw_delay = qos_nw_delay;
		ret = ath12k_wifi7_mcbc_setup_msdu_info(dp_link_vif, arsta, dp,
							&msdu_info, gsn, gsn_valid,
							skb_new, is_eth, htt_mesh);
		if (ret < 0) {
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		feature_ret = ath12k_wifi7_dp_tx_process_features(dp_vif, dp_pdev,
								  skb_new, &len,
								  &msdu_info,
								  skb_ctrl);
		if (feature_ret != DP_TX_FEATURE_SUCCESS) {
			if (feature_ret == DP_TX_RETURN)
				break;

			DP_STATS_INC(dp_vif,
				     tx_i.drop[DP_TX_ENQ_DROP_FEAT_ERR],
				     1, ring_id);
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		msdu_info.data_len = len;

		if (gsn_valid)
			msdu_info.vdev_id +=
				HTT_TX_MLO_MCAST_HOST_REINJECT_BASE_VDEV_ID;
		else if (arvif->nawds_support && !msdu_info.lookup_override)
			msdu_info.meta_data_flags |=
				u32_encode_bits(1,
						HTT_TCL_META_DATA_HOST_INSPECTED_MISSION);

		err = ath12k_wifi7_dp_tx_mcast_send(dp_pdev, ahvif, dp_link_vif,
						    ring_id, &msdu_info,
						    gsn_valid, gsn, skb_new,
						    arsta, skb_ctrl, htt_mesh);

		if (unlikely(err != DP_TX_ENQ_SUCCESS)) {
			DP_STATS_INC(dp_vif, tx_i.drop[err], 1, ring_id);
			dev_kfree_skb_any(skb_new);
			goto next;
		}

		atomic_inc(&dp_pdev->num_tx_pending);

next:
		links_map &= ~BIT(link_id);
		link_id = find_first_bit(&links_map,
					 IEEE80211_MLD_MAX_NUM_LINKS);

	} while (link_id < IEEE80211_MLD_MAX_NUM_LINKS);
}

/**
 * ath12k_wifi7_ucast_handler() - Unicast packet transmission handler
 * @dp_vif: DP virtual interface
 * @link_id: MLO link identifier for the target link
 * @arsta: Link station pointer (optional; used for QoS tag updates)
 * @skb: Socket buffer to transmit
 * @dp_skb_features: Per-SKB feature bitmap (e.g. DP_ETH_OFFLOAD, DP_FEATURE_DVLAN)
 * @qos_nw_delay: QoS network delay value for SDWF ingress statistics
 *
 * Main unicast TX handler. Resolves the DP pdev, checks the TX pending limit,
 * selects a TX ring, populates MSDU info, processes the TX feature bitmap,
 * assigns a TX descriptor, DMA-maps the buffer, populates the SW TX descriptor,
 * and enqueues the frame to the TCL hardware ring.
 *
 */
void ath12k_wifi7_ucast_handler(struct ath12k_dp_vif *dp_vif,
				u8 link_id,
				struct ath12k_link_sta *arsta,
				struct sk_buff *skb,
				struct ath12k_dp_skb_ctrl *skb_ctrl,
				u32 qos_nw_delay, bool htt_mesh,
				struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_vif *ahvif = container_of(dp_vif, struct ath12k_vif, dp_vif);
	struct ath12k_dp_link_vif *dp_link_vif = &dp_vif->dp_link_vif[link_id];
	struct ath12k_dp *dp = NULL;
	struct ath12k_base *ab;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct ath12k_dp_tx_msdu_info msdu_info = {0};
	enum ath12k_dp_feature_result ret;
	bool feat_bypass = true;
	bool dma_map = false;
	u8 ring_id = 0;
	u32 len = skb->len;
	u8 tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
	enum ath12k_dp_tx_enq_error drop_reason = DP_TX_ENQ_DROP_MISC;

	ab = dp_link_vif->ab;
	if (!ab)
		goto fail;

	/* Get DP pdev */
	dp_pdev = ath12k_dp_to_dp_pdev(ab->dp, dp_link_vif->pdev_idx);
	if (!dp_pdev) {
		drop_reason = DP_TX_ENQ_DROP_INV_PDEV;
		goto fail;
	}

	prefetch(dp_pdev);

	/* Check recovery state */
	if (ath12k_wifi7_tx_validate_recovery(ab)) {
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), skb);
		drop_reason = DP_TX_ENQ_DROP_FW_RECOVERY;
		return;
	}

	/* Get ring ID  */
	if (likely(skb_ctrl->flags & DP_SKB_FAST_TX))
		ring_id = smp_processor_id();
	else
		ath12k_wifi7_dp_get_ring_id(dp_pdev->dp, &ring_id, skb);

	/* Update receive statistics */
	ath12k_dp_tx_stats_update_pre_enqueue(dp_pdev, dp_vif,
					      skb, ring_id, len);

	/* Setup MSDU info */
	ath12k_wifi7_ucast_setup_msdu_info(dp_link_vif, &msdu_info, skb,
					   htt_mesh, arsta, ab);

	/* Fast path: no features enabled */
	if (unlikely((DP_FEATURE_IS_ANY(dp_vif) || skb_ctrl->features))) {
		/* Process features based on bitmap */
		feat_bypass = false;
		ret = ath12k_wifi7_dp_tx_process_features(dp_vif, dp_pdev, skb, &len,
							  &msdu_info, skb_ctrl);

		if (ret != DP_TX_FEATURE_SUCCESS && ret != DP_TX_RETURN) {
			drop_reason = DP_TX_ENQ_DROP_FEAT_ERR;
			goto fail;
		}
	}

	/* Assign TX descriptor */
	dp = dp_pdev->dp;
	prefetch(dp->device_stats.tx_fast_unicast);

	if (unlikely(!(skb_ctrl->flags & DP_SKB_FAST_TX))) {
		if (unlikely(skb->protocol == cpu_to_be16(ETH_P_PAE))) {
			struct list_head *spl_desc_free_list;

			spl_desc_free_list = dp->dp_hw_grp->tx_spl_desc_free_list;
			tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
							     spl_desc_free_list, ring_id);
			if (unlikely(!tx_desc)) {
				dp->device_stats.tx_err.txbuf_na[ring_id]++;
				drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
				goto fail;
			}
			goto skip_assign_buffer;
		}
	}

	tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
					     dp->dp_hw_grp->tx_desc_free_list, ring_id);

	if (unlikely(!tx_desc)) {
		dp->device_stats.tx_err.txbuf_na[ring_id]++;
		drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
		goto fail;
	}

skip_assign_buffer:
	dma_map = ath12k_dp_tx_dma_map(dp, skb, len, tx_desc, &msdu_info,
				       skb_ctrl);
	if (unlikely(!dma_map)) {
		ath12k_warn(dp->ab, "failed to DMA map data Tx buffer\n");
		atomic_inc(&dp->device_stats.tx_err.misc_fail);
		drop_reason = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail;
	}

	msdu_info.desc_id = tx_desc->desc_id;
	msdu_info.data_len = len;
	msdu_info.group_slot = -1;
	msdu_info.mpsk_diff_encap = 0;

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	 tx_desc->mmesh = (ahvif &&
			  (QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(ahvif->vap_submode)));
	 /* mmesh uses msdu ext desc to program rates to
	  * firmware
	  */
	msdu_info.mhdr_len = dp_vif->dp_extn.mhdr_len;
#endif

	ret = ath12k_wifi7_dp_tx_desc_populate(dp_pdev, skb, dp_vif, dp_link_vif,
					       &msdu_info, tx_desc, false, 0,
					       ring_id);
	if (ret != DP_TX_FEATURE_SUCCESS) {
		drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail;
	}

	/* Enqueue to hardware */
	ret = ath12k_wifi7_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, &msdu_info, ring_id,
					    arsta, skb, qos_nw_delay,
					    tx_desc, dp_vif, feat_bypass, false, dp_peer);
	if (ret) {
		drop_reason = DP_TX_ENQ_DROP_HW_ENQ_FAIL;
		goto fail;
	}
	atomic_inc(&dp_pdev->num_tx_pending);
	return;

fail:
	if (dma_map && tx_desc)
		ath12k_dp_tx_buffer_unmap(dp->dev, tx_desc->paddr, len,
					  DMA_TO_DEVICE);

	if (tx_desc && tx_desc->ext_desc) {
		if (tx_desc->is_from_sg)
			ath12k_dp_tx_sg_unmap_buf(dp, tx_desc->ext_desc, skb);
		ath12k_dp_ext_desc_unmap(dp, tx_desc->paddr_ext_desc);
		kmem_cache_free(dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);

	if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev)) {
		if (ath12k_tid_stats_enabled(dp_pdev))
			ath12k_dp_tx_drop_tid_stats(dp_vif, drop_reason, tid, len);

		if (ath12k_dp_vow_stats_enabled(dp_pdev))
			ath12k_dp_tx_drop_pdev_tid_stats(dp_pdev, drop_reason,
							 tid, ring_id);
	}

	ath12k_mac_ieee80211_free_txskb(ahvif->ah->hw, skb, dp_pdev,
					arsta ? ath12k_ahsta_to_sta(arsta->ahsta) : NULL,
					dp_vif, drop_reason, ring_id, false);
}

enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_vif *ahvif,
			      struct ath12k_dp_link_vif *dp_link_vif,
			      u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			      bool gsn_valid, u16 gsn,
			      struct sk_buff *skb, struct ath12k_link_sta *arsta,
			      struct ath12k_dp_skb_ctrl *skb_ctrl, bool htt_mesh)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_tx_desc_info *tx_desc = NULL;
	bool dma_map = false;
	enum ath12k_dp_tx_enq_error drop_reason;
	u32 qos_nw_delay = msdu_info->qos_nw_delay;
	int ret;
	bool is_mcast = true;
	u8 tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;

	tx_desc = ath12k_dp_tx_assign_buffer(dp->dp_hw_grp,
					     dp->dp_hw_grp->tx_desc_free_list,
					     ring_id);
	if (!tx_desc) {
		drop_reason = DP_TX_ENQ_DROP_SW_DESC_NA;
		goto fail;
	}

	if (msdu_info->group_slot > 0 && !msdu_info->mpsk_diff_encap) {
		ret = ath12k_wifi7_dp_prepare_group_htt_metadata(skb, msdu_info,
								 dp_link_vif,
								 gsn_valid, gsn);
		if (ret) {
			drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
			goto fail;
		}
	}

	ath12k_wifi7_dp_dma_align_handler(dp, skb);

	/* For multicast packets, map the full buffer since MCAST always uses the slow
	 * path. skb->len includes any HTT metadata as well.
	 */
	dma_map = ath12k_dp_tx_dma_map(dp, skb, skb->len, tx_desc,
				       msdu_info, skb_ctrl);

	if (unlikely(!dma_map)) {
		drop_reason = DP_TX_ENQ_DROP_DMA_ERR;
		goto fail;
	}

	msdu_info->desc_id = tx_desc->desc_id;

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	 tx_desc->mmesh = (ahvif &&
			  (QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(ahvif->vap_submode)));
	 /* mmesh uses msdu ext desc to program rates to
	  * firmware
	  */
	msdu_info->mhdr_len = ahvif->dp_vif.dp_extn.mhdr_len;
#endif

	ret = ath12k_wifi7_dp_tx_desc_populate(dp_pdev, skb, dp_vif, dp_link_vif,
					       msdu_info, tx_desc,
					       gsn_valid, gsn, ring_id);

	if (ret < 0) {
		drop_reason = DP_TX_ENQ_DROP_TCL_DESC_NA;
		goto fail;
	}

	/* mcast maps skb->len (includes HTT header); fix up tx_desc->len so
	 * the completion-path unmap covers the same region.
	 */
	tx_desc->len = skb->len;

	/* Enqueue to hardware */
	ret = ath12k_wifi7_dp_tx_hw_enqueue(dp_link_vif, dp_pdev, msdu_info,
					    ring_id, arsta, skb, qos_nw_delay,
					    tx_desc, &ahvif->dp_vif, false,
					    is_mcast, NULL);
	if (ret) {
		drop_reason = DP_TX_ENQ_DROP_HW_ENQ_FAIL;
		goto fail;
	}
	return DP_TX_ENQ_SUCCESS;

fail:
	if (dma_map && tx_desc)
		ath12k_dp_tx_buffer_unmap(dp->dev, tx_desc->paddr, skb->len,
					  DMA_TO_DEVICE);

	if (tx_desc && tx_desc->ext_desc) {
		if (tx_desc->is_from_sg)
			ath12k_dp_tx_sg_unmap_buf(dp, tx_desc->ext_desc, skb);
		ath12k_dp_ext_desc_unmap(dp, tx_desc->paddr_ext_desc);
		kmem_cache_free(dp->ext_cache, tx_desc->ext_desc);
	}

	if (tx_desc)
		ath12k_dp_tx_release_txbuf(dp, tx_desc, ring_id);

	if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_dp_vow_stats_enabled(dp_pdev))
		ath12k_dp_tx_drop_pdev_tid_stats(dp_pdev, drop_reason, tid, ring_id);

	return drop_reason;
}


void ath12k_wifi7_dp_tx_set_ast(struct ath12k_dp_peer *dp_peer,
				struct ath12k_dp_tx_msdu_info *msdu_info,
				u8 hw_link_id)
{
	struct ath12k_dp_link_peer *dp_link_peer;

	dp_link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, hw_link_id);

	if (dp_link_peer) {
		msdu_info->bss_ast_hash = dp_link_peer->ast_hash;
		msdu_info->bss_ast_idx = dp_link_peer->hw_peer_id;
	}
}

static inline void
ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(struct hal_tx_status *ts,
					       struct ath12k_dp *dp)
{
	ts->hw_link_id = (DP_GET_HW_LINK_ID_FRM_PPDU_ID(ts->ppdu_id,
							dp->link_id_offset,
							dp->link_id_bits));
}

static void ath12k_wifi7_dp_tx_free_txbuf(struct ath12k_dp *dp,
					  struct sk_buff *msdu,
					  struct dp_tx_ring *tx_ring,
					  struct ath12k_tx_sw_metadata *sw_metadata)
{
	struct ath12k_pdev_dp *dp_pdev;

	ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr,
				  sw_metadata->len, DMA_TO_DEVICE);
	rcu_read_lock();

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

static u32 ath12k_wifi7_dp_tx_compute_hw_delay(struct ath12k_pdev_dp *dp_pdev,
					       struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_pdev_dp *tx_dp_pdev;
	struct ath12k *tx_ar, *primary_ar;
	u32 fw_delay_us = 0;
	u32 tqm_enqueue_us, final_tqm_enqueue_us;
	u32 compl_tsf_us, final_compl_tsf_us;
	int delta_tqm, delta_tsf2;

	if (unlikely(!ar || !ar->ab))
		return 0;

	/*
	 * FW/HW delay: time from TQM enqueue to over-the-air TX completion.
	 * Use HW buffer_timestamp (from TQM) instead of SW hw_tstamp.
	 */
	tqm_enqueue_us =
		TX_COMPL_BUFFER_TSTAMP_US(ts->delay_stats.ts.buffer_timestamp);
	compl_tsf_us = ts->delay_stats.ts.tsf;

	if (unlikely(tqm_enqueue_us == 0 || compl_tsf_us == 0))
		return 0;

	if (unlikely(ag->mlo_tstamp_offset == 0))
		return 0;

	primary_ar = ar->ab->pdevs[0].ar;
	if (primary_ar)
		delta_tqm = (int)ag->mlo_tstamp_offset - (int)primary_ar->delta_tqm;
	else
		delta_tqm = (int)ag->mlo_tstamp_offset - (int)ar->delta_tqm;

	tx_dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, ts->hw_link_id);
	tx_ar = (tx_dp_pdev && tx_dp_pdev->ar) ? tx_dp_pdev->ar : ar;

	if (unlikely(!tx_ar))
		return 0;

	delta_tsf2 = (int)ag->mlo_tstamp_offset - (int)tx_ar->delta_tsf2;

	final_tqm_enqueue_us = (tqm_enqueue_us + delta_tqm) & HW_TX_DELAY_MASK;
	final_compl_tsf_us = (compl_tsf_us + delta_tsf2) & HW_TX_DELAY_MASK;

	fw_delay_us = (final_compl_tsf_us - final_tqm_enqueue_us) & HW_TX_DELAY_MASK;

	/* Discard values that exceed the plausible maximum (clock wrap-around
	 * or uninitialized MLO offsets produce very large unsigned results).
	 */
	if (fw_delay_us > HW_TX_DELAY_MAX)
		fw_delay_us = 0;

	return fw_delay_us;
}

static void
ath12k_wifi7_dp_tx_htt_tx_complete_buf(struct ath12k_dp *dp,
				       struct sk_buff *msdu,
				       struct dp_tx_ring *tx_ring,
				       struct hal_tx_status *ts,
				       struct ath12k_tx_sw_metadata *sw_metadata,
				       u16 peer_id, int link_id,
				       struct ath12k_dp_peer *dp_peer, u8 ring)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev;
	struct ethhdr *eth;
	struct ieee80211_hdr *hdr;
	size_t hdrlen;
	enum ath12k_dp_eapol_key_type subtype;
	u32 hw_enq_tstamp = 0;
	u32 hw_delay = 0;

	ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr, sw_metadata->len,
				  DMA_TO_DEVICE);

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	skb_cb = ATH12K_SKB_CB(msdu);
	info = IEEE80211_SKB_CB(msdu);

	vif = ath12k_dp_peer_get_vif(dp_peer);
	if (vif) {
		ahvif = ath12k_vif_to_ahvif(vif);
		if (dp_pdev->wmm_stats.tx_type) {
			ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
			if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
				ahvif->wmm_stats.total_wmm_tx_drop[ahvif->wmm_stats.tx_type]++;
		}
	}

	if (msdu->protocol == cpu_to_be16(ETH_P_PAE)) {
		if (skb_cb->flags & ATH12K_SKB_HW_80211_ENCAP) {
			eth = (struct ethhdr *)msdu->data;
			subtype = ath12k_dp_get_eapol_subtype(msdu->data + ETH_HLEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX) {
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 eth->h_dest);
			}
		} else {
			hdr = (struct ieee80211_hdr *)msdu->data;
			hdrlen = ieee80211_get_hdrlen_from_skb(msdu);
			subtype = ath12k_dp_get_eapol_subtype(msdu->data
					+ hdrlen + LLC_SNAP_HDR_LEN);
			if (subtype != DP_EAPOL_KEY_TYPE_MAX) {
				ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
						 "Tx completion success for %s%d EAPOL frame to STA %pM\n",
						 subtype <= 4 ? "M" : "G",
						 subtype <= 4 ? subtype : (subtype - 4),
						 hdr->addr1);
			}
		}
	}

	if (!dp_peer || !ath12k_dp_peer_get_sta(dp_peer))
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n", peer_id);
	else {
		if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
		    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id
							(dp_peer, dp_pdev->hw_link_id);
			if (link_peer)
				WRITE_ONCE(link_peer->peer_stats.last_ack, jiffies);
		}

		status.sta = ath12k_dp_peer_get_sta(dp_peer);
	}

	if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
		if (unlikely(ath12k_debugfs_is_qos_stats_enabled(dp_pdev->ar))) {
			ath12k_wifi7_compute_hw_delay(dp_pdev->ar, ts, &hw_delay);
			ath12k_qos_stats_update(dp_peer, ts->hw_link_id,
						dp_pdev->ar, msdu, ts,
						dp_pdev, msdu->tstamp,
						hw_delay);
			}
		if (unlikely(ath12k_dp_latency_stats_enabled(dp_pdev))) {
			hw_enq_tstamp = sw_metadata->hw_enqueue_tstamp;
			hw_delay = ath12k_wifi7_dp_tx_compute_hw_delay(dp_pdev, ts);

			ath12k_dp_tx_update_peer_latency_stats(dp_pdev,
							       dp_peer,
							       ts, ring,
							       sw_metadata->skb,
							       hw_enq_tstamp,
							       hw_delay);
		}
	}

	status.skb = msdu;
	if (vif && (vif->offload_flags & IEEE80211_OFFLOAD_TXRX_STATS)) {
		ieee80211_tx_status_offload(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	} else {
		memset(&info->status, 0, sizeof(info->status));

		if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
		    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		}

		if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
		    (info->flags & IEEE80211_TX_CTL_NO_ACK))
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

		status.info = info;
		if (status.sta && status.sta->valid_links && (link_id >= 0)) {
			status.link_valid = 1;
			status.link_id =
				ath12k_dp_peer_convert_hw_to_logical_link_id(
								dp_peer,
								link_id);
		}
		ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	}
	rcu_read_unlock();
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
static void ath12k_dp_tx_update_mmesh_stats(struct ath12k_dp *dp,
					    struct ath12k_pdev_dp *dp_pdev,
					    struct sk_buff *skb,
					    struct ath12k_dp_peer *peer,
					    int reason)
{
	struct meta_hdr_s *mhdr;

	if (!peer)
		return;

	if (reason == HAL_WBM_REL_SRC_MODULE_FW) {
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, tofw, 0, 1);
	} else {
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, direct, 0, 1);
		return;
	}

	mhdr = (struct meta_hdr_s *)(skb->data - MMESH_TX_META_HDR);
	if (mhdr->flags & METAHDR_FLAG_NOQOS)
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, no_qos, 0, 1);
	if (mhdr->flags & METAHDR_FLAG_NOENCRYPT)
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, no_enc, 0, 1);
	if (mhdr->flags & METAHDR_FLAG_INFO_UPDATED)
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, txinfo, 0, 1);
	if (mhdr->flags & METAHDR_FLAG_AUTO_RATE)
		DP_PEER_MISC_STATS_INC(peer, mmesh_stat, auto_rate, 0, 1);
}
#endif

static void ath12k_wifi7_dp_tx_update_peer_basic_stats(struct ath12k_dp_peer *peer,
						       u32 msdu_len, u8 tx_status,
						       u8 link_id, int ring_id)
{
	DP_PEER_STATS_PKT_LEN(peer, tx, ring_id, comp_pkt, link_id, 1, msdu_len);

	if (tx_status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_STATS_PKT_LEN(peer, tx, ring_id, tx_success, link_id,
				      1, msdu_len);
	} else {
		DP_PEER_STATS_INC(peer, tx, ring_id, tx_failed, link_id, 1);
	}
}

static void ath12k_wifi7_dp_tx_comp_update_peer_stats(struct ath12k_dp_peer *peer,
						      struct hal_tx_status *ts,
						      u16 tx_desc_flags,
						      u8 link_id, u32 msdu_len)
{
	if (peer->is_vdev_peer) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU) {
			if (tx_desc_flags & DP_TX_DESC_FLAG_BCAST)
				DP_PEER_DBG_STATS_PKT_LEN(peer, tx_dbg, bcast,
							  link_id, 1, msdu_len);
			if (tx_desc_flags & DP_TX_DESC_FLAG_MCAST)
				DP_PEER_DBG_STATS_PKT_LEN(peer, tx_dbg, mcast,
							  link_id, 1, msdu_len);
		}
	} else {
		DP_PEER_DBG_STATS_PKT_LEN(peer, tx_dbg, ucast, link_id, 1,
					  msdu_len);
	}

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM) {
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, release_src_not_tqm,
				      link_id, 1);
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, wbm_rel_reason[ts->status],
				      link_id, 1);
		return;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, retry_count, link_id,
					   ts->transmit_cnt > 1, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, total_msdu_retries,
					   link_id, ts->transmit_cnt > 1,
					   ts->transmit_cnt - 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, multiple_retry_count,
					   link_id, ts->transmit_cnt > 2, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, ofdma, link_id,
					   ts->ofdma, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, amsdu_cnt, link_id,
					   ts->msdu_part_of_amsdu, 1);

		DP_PEER_DBG_STATS_COND_INC(peer, tx_dbg, non_amsdu_cnt, link_id,
					   !ts->msdu_part_of_amsdu, 1);
	}

	if (ts->status < HAL_WBM_TQM_REL_REASON_MAX) {
		DP_PEER_DBG_STATS_INC(peer, tx_dbg, tqm_rel_reason[ts->status],
				      link_id, 1);
	}
}

static void
ath12k_wifi7_dp_tx_htt_update_peer_stats(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_peer *peer,
					 struct hal_tx_status *ts,
					 u32 msdu_len, u32 htt_status,
					 int link_id, int ring_id,
					 u8 tx_desc_flags,
					 struct sk_buff *skb)
{
	u8 vow_tid;

	if (peer) {
		ath12k_wifi7_dp_tx_update_peer_basic_stats(peer, msdu_len,
							   htt_status, link_id,
							   ring_id);
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_wifi7_dp_tx_comp_update_peer_stats(peer,
									  ts,
									  tx_desc_flags,
									  link_id,
									  msdu_len);
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
			if (peer->is_mmesh_peer)
				ath12k_dp_tx_update_mmesh_stats(dp, dp_pdev,
								skb, peer,
								ts->buf_rel_source);
#endif
			if (unlikely(ath12k_dp_vow_stats_enabled(dp_pdev))) {
				vow_tid = ath12k_vow_tid_validate(ts->tid);
				if (htt_status < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX)
					DP_PDEV_TID_TX_REASON_INC(dp_pdev,
								  ring_id,
								  vow_tid,
								  htt_status_cnt,
								  htt_status);
			}
		}
	} else {
		DP_DEVICE_STATS_INC(dp,
				    tx_err.tx_comp_err
				    [DP_TX_COMP_ERR_INVALID_PEER][ring_id],
				    1);
	}
}

static void
ath12k_wifi7_dp_tx_process_htt_tx_complete(struct ath12k_dp *dp,
					   void *desc, struct sk_buff *msdu,
					   struct dp_tx_ring *tx_ring,
					   struct ath12k_tx_sw_metadata *sw_metadata,
					   struct hal_tx_status *ts,
					   int ring_id, u32 htt_status)
{
	struct htt_tx_completion *status_desc;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_peer *peer = NULL;
	int link_id = -1;
	u32 msdu_len = msdu->len;
	u8 tx_desc_flags = sw_metadata->flags;

	status_desc = desc;

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, sw_metadata->hw_link_id);
	if (!dp_pdev) {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
		rcu_read_unlock();
		return;
	}

	if ((sw_metadata->flags & DP_TX_DESC_FLAG_FAST) &&
	    !ath12k_dp_stats_enabled(dp_pdev)) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	ts->ppdu_id = le32_get_bits(status_desc->info2,
				    HTT_TX_WBM_COMP_INFO2_PPDU_ID);
	ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(ts, dp);

	if (le32_get_bits(status_desc->info3, HTT_TX_WBM_COMP_INFO3_VALID)) {
		ts->peer_id = le32_get_bits(status_desc->info3,
					    HTT_TX_WBM_COMP_INFO3_SW_PEER_ID);
	} else {
		ts->peer_id = HAL_INVALID_PEERID;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (peer)
		link_id = ath12k_dp_validate_hw_link_id(ts->hw_link_id);

	/* For FAST path packets (bypassing mac80211), collect peer stats and
	 * free the SKB with dev_kfree_skb_any() before reaching the switch
	 * statement, which calls mac80211 functions not suitable for FAST path.
	 */
	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK) {
			ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
			ts->acked = true;
			ts->ack_rssi = le32_get_bits(status_desc->info2,
						     HTT_TX_WBM_COMP_INFO2_ACK_RSSI);
		} else if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP) {
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
		} else if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL) {
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
		}

		ath12k_wifi7_dp_tx_htt_update_peer_stats(dp, dp_pdev, peer, ts,
							 msdu_len, htt_status,
							 link_id, ring_id,
							 tx_desc_flags,
							 sw_metadata->skb);

		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	switch (htt_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
		ts->acked = (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts->ack_rssi = le32_get_bits(status_desc->info2,
					    HTT_TX_WBM_COMP_INFO2_ACK_RSSI);

		ts->status = HAL_WBM_TQM_REL_REASON_FRAME_ACKED;
		ath12k_wifi7_dp_tx_htt_tx_complete_buf(dp, msdu, tx_ring, ts,
						       sw_metadata, ts->peer_id,
						       link_id, peer, ring_id);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
		switch (htt_status) {
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU;
			break;
		case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
			ts->status = HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX;
			fallthrough;
		default:
			break;
		}
		fallthrough;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH:
		ath12k_wifi7_dp_tx_free_txbuf(dp, msdu, tx_ring, sw_metadata);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY:
		/* This event is to be handled only when the driver decides to
		 * use WDS offload functionality.
		 */
		ATH12K_DMA_UNMAP_WITH_FREE_SKB(dp->ab, msdu);
		break;
	default:
		ath12k_warn(dp->ab, "Unknown htt tx status %d\n", htt_status);
		ATH12K_DMA_UNMAP_WITH_FREE_SKB(dp->ab, msdu);
		break;
	}

	ath12k_wifi7_dp_tx_htt_update_peer_stats(dp, dp_pdev, peer, ts,
						 msdu_len, htt_status,
						 link_id, ring_id,
						 tx_desc_flags,
						 sw_metadata->skb);
	rcu_read_unlock();
}

static void
ath12k_wifi7_dp_tx_cache_peer_stats(struct ath12k *ar,
				    struct sk_buff *msdu,
				    struct hal_tx_status *ts)
{
	struct ath12k_per_peer_tx_stats *peer_stats = &ar->cached_stats;

	if (ts->try_cnt > 1) {
		peer_stats->retry_pkts += ts->try_cnt - 1;
		peer_stats->retry_bytes += (ts->try_cnt - 1) * msdu->len;

		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
			peer_stats->failed_pkts += 1;
			peer_stats->failed_bytes += msdu->len;
		}
	}
}

static void
ath12k_wifi7_dp_tx_update_txcompl(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_tx_status *ts)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_link_peer *peer;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_peer *dp_peer;
	struct rate_info txrate = {0};
	u16 rate, ru_tones;
	u8 rate_idx = 0;
	int ret;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	if (!dp_peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "MLD peer NA with peer_id: %u\n", ts->peer_id);
		return;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ts->hw_link_id);
	if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	sta = ath12k_dp_link_peer_get_sta(peer);
	ahsta = ath12k_sta_to_ahsta(sta);

	if (peer->last_txrate.nss)
		txrate.nss = peer->last_txrate.nss;
	spin_unlock_bh(&dp->dp_lock);

	switch (ts->pkt_type) {
	case HAL_TX_RATE_STATS_PKT_TYPE_11A:
	case HAL_TX_RATE_STATS_PKT_TYPE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(ts->mcs,
							    ts->pkt_type,
							    &rate_idx,
							    &rate);
		if (ret < 0) {
			ath12k_warn(ab, "Invalid tx legacy rate %d\n", ret);
			return;
		}

		txrate.legacy = rate;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11N:
		if (ts->mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ab, "Invalid HT mcs index %d\n", ts->mcs);
			return;
		}

		if (txrate.nss < 1 ||
		    (dp_pdev->ar->pdev->cap.max_tx_nss &&
		     (txrate.nss > dp_pdev->ar->pdev->cap.max_tx_nss)) ||
		      (txrate.nss > hweight32(dp_pdev->ar->pdev->cap.tx_chain_mask)))
			ath12k_warn(ab, "Invalid nss value: %d", txrate.nss);
		else
			txrate.mcs = ts->mcs + 8 * (txrate.nss - 1);

		txrate.flags = RATE_INFO_FLAGS_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AC:
		if (ts->mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid VHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_VHT_MCS;

		if (ts->sgi)
			txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11AX:
		if (ts->mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ab, "Invalid HE mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		txrate.he_gi = ath12k_he_gi_to_nl80211_he_gi(ts->sgi);
		break;
	case HAL_TX_RATE_STATS_PKT_TYPE_11BE:
		if (ts->mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ab, "Invalid EHT mcs index %d\n", ts->mcs);
			return;
		}

		txrate.mcs = ts->mcs;
		txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		txrate.eht_gi = ath12k_mac_eht_gi_to_nl80211_eht_gi(ts->sgi);
		break;
	default:
		ath12k_warn(ab, "Invalid tx pkt type: %d\n", ts->pkt_type);
		return;
	}

	txrate.bw = ath12k_mac_bw_to_mac80211_bw(ts->bw);

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		txrate.bw = RATE_INFO_BW_HE_RU;
		ru_tones = ath12k_mac_he_convert_tones_to_ru_tones(ts->tones);
		txrate.he_ru_alloc =
			ath12k_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	}

	if (ts->ofdma && ts->pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11BE) {
		txrate.bw = RATE_INFO_BW_EHT_RU;
		txrate.eht_ru_alloc =
			ath12k_mac_eht_ru_tones_to_nl80211_eht_ru_alloc(ts->tones);
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ts->hw_link_id);
	if (peer)
		peer->txrate = txrate;
	else
		ath12k_dbg(ab, ATH12K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
	spin_unlock_bh(&dp->dp_lock);
}

static void ath12k_wifi7_dp_tx_complete_msdu(struct ath12k_pdev_dp *dp_pdev,
					     struct sk_buff *msdu,
					     struct hal_tx_status *ts,
					     struct ath12k_tx_sw_metadata *sw_metadata,
					     int ring)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_rate_status status_rate = { 0 };
	struct rate_info rate;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k *ar;
	struct ath12k_dp_peer *peer = NULL;
	u8 hw_link_id = 0;
	u8 reason = 0;
	u8 tid = 0;
	u8 vow_tid = 0;
	enum ath12k_dp_tx_comp_error drop_reason = DP_TX_COMP_ERR_MISC;
	u32 msdu_len = msdu->len, enq_tstamp = 0;
	u8 tx_desc_flags = sw_metadata->flags;
	u32 hw_enq_tstamp = 0;
	u32 hw_delay_latency = 0;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	if (sw_metadata->skb)
		ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr,
					  sw_metadata->len, DMA_TO_DEVICE);

	dp_pdev->wmm_stats.tx_type = ath12k_tid_to_ac(ts->tid > ATH12K_DSCP_PRIORITY ? 0:ts->tid);
	if (dp_pdev->wmm_stats.tx_type) {
		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
			dp_pdev->wmm_stats.total_wmm_tx_drop
				[dp_pdev->wmm_stats.tx_type]++;
	}

	skb_cb = ATH12K_SKB_CB(msdu);

	rcu_read_lock();

	if (!rcu_dereference(ab->pdevs_active[dp_pdev->mac_id])) {
		drop_reason = DP_TX_COMP_ERR_INVALID_PDEV;
		goto exit;
	}

	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, ts->peer_id);
	vif = ath12k_dp_peer_get_vif(peer);
	if (!vif) {
		drop_reason = DP_TX_COMP_ERR_INVALID_VIF;
		goto exit;
	}

	info = IEEE80211_SKB_CB(msdu);
	ar = dp_pdev->ar;

	if (peer) {
		hw_link_id = ath12k_dp_validate_hw_link_id(ts->hw_link_id);
		ath12k_wifi7_dp_tx_update_peer_basic_stats(peer, msdu_len,
							   ts->status,
							   hw_link_id, ring);
		if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
			if (ath12k_dp_debug_stats_enabled(dp_pdev))
				ath12k_wifi7_dp_tx_comp_update_peer_stats(peer,
									  ts,
									  tx_desc_flags,
									  hw_link_id,
									  msdu_len);
			if (unlikely(ath12k_debugfs_is_qos_stats_enabled(ar))) {
				u32 hw_delay = 0;

				ath12k_wifi7_compute_hw_delay(ar, ts, &hw_delay);
				ath12k_qos_stats_update(peer, hw_link_id,
							ar, msdu, ts,
							dp_pdev, msdu->tstamp,
							hw_delay);
			}

			/* Update peer level protocol stats at TX completion */
			if (unlikely(ath12k_proto_stats_enabled(dp_pdev))) {
				ath12k_dp_tx_peer_update_proto_stats(peer,
								     hw_link_id,
								     msdu,
								     TX_COMP,
								     ring);
			}
			if (unlikely(ath12k_dp_latency_stats_enabled(dp_pdev))) {
				hw_enq_tstamp = sw_metadata->hw_enqueue_tstamp;
				hw_delay_latency =
					ath12k_wifi7_dp_tx_compute_hw_delay(dp_pdev, ts);

				ath12k_dp_tx_update_peer_latency_stats(dp_pdev,
								       peer,
								       ts, ring,
								       sw_metadata->skb,
								       hw_enq_tstamp,
								       hw_delay_latency);
			}
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
			if (peer->is_mmesh_peer)
				ath12k_dp_tx_update_mmesh_stats(dp, dp_pdev,
								msdu,
								peer,
								ts->buf_rel_source);
#endif
			if (unlikely(ath12k_dp_vow_stats_enabled(dp_pdev))) {
				enq_tstamp = sw_metadata->hw_enqueue_tstamp;

				vow_tid = ath12k_vow_tid_validate(ts->tid);
				if (ts->status < HAL_WBM_TQM_REL_REASON_MAX)
					DP_PDEV_TID_TX_REASON_INC(dp_pdev,
								      ring,
								      vow_tid,
								      tqm_status_cnt,
								      ts->status);

				ath12k_wifi7_dp_tx_update_delay_stats(dp_pdev, msdu,
								      ts, ring,
								      enq_tstamp);
			}

			if (ath12k_tid_stats_enabled(dp_pdev)) {
				ahvif = ath12k_vif_to_ahvif(vif);
				tid = msdu->priority & IEEE80211_QOS_CTL_TID_MASK;
				ath12k_tid_tx_stats(ahvif, tid, msdu->len,
						    ATH_TX_COMPLETED_PKTS);
			}
		}
	} else {
		DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PEER][ring], 1);
	}

	if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts->status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_TQM_REMOVE_MPDU;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_TQM_THRESHOLD;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_TQM_REMOVE_AGED;
			goto exit;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_TQM_REMOVE_TX;
			goto exit;
		default:
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n", ts->status);
 		}
	}

	/* NOTE: Tx rate status reporting. Tx completion status does not have
	 * necessary information (for example nss) to build the tx rate.
	 * Might end up reporting it out-of-band from HTT stats.
	 */

	if (ath12k_extd_tx_stats_enabled(&ar->dp)) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
						sizeof(struct ath12k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);
				memset(&ar->cached_stats, 0,
						sizeof(struct ath12k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath12k_wifi7_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

	ath12k_wifi7_dp_tx_update_txcompl(dp_pdev, ts);

	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
							     ts->peer_id);
	if (!link_peer || !ath12k_dp_link_peer_get_sta(link_peer)) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   ts->peer_id);
		drop_reason = DP_TX_COMP_ERR_INVALID_LINK_PEER;
		goto exit;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !(info->flags & IEEE80211_TX_CTL_NO_ACK))
		WRITE_ONCE(link_peer->peer_stats.last_ack, jiffies);

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST) {
		dev_kfree_skb_any(msdu);
		rcu_read_unlock();
		return;
	}

	status.skb = msdu;
	if (vif && (vif->offload_flags & IEEE80211_OFFLOAD_TXRX_STATS)) {
		ieee80211_tx_status_offload(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	} else {
		memset(&info->status, 0, sizeof(info->status));

		/* skip tx rate update from ieee80211_status*/
		info->status.rates[0].idx = -1;
		if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
		    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ts->ack_rssi;

			if (!test_bit(WMI_TLV_SERVICE_HW_DB2DBM_CONVERSION_SUPPORT,
				      ab->wmi_ab.svc_map))
				info->status.ack_signal += ATH12K_DEFAULT_NOISE_FLOOR;

			info->status.flags = IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		}

		if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
		    (info->flags & IEEE80211_TX_CTL_NO_ACK))
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

		status.sta = ath12k_dp_link_peer_get_sta(link_peer);
		status.info = info;
		rate = link_peer->last_txrate;

		status_rate.rate_idx = rate;
		status_rate.try_count = 1;

		status.rates = &status_rate;
		status.n_rates = 1;

		if (status.sta && status.sta->valid_links && (hw_link_id >= 0)) {
			status.link_valid = 1;
			status.link_id =
				ath12k_dp_peer_convert_hw_to_logical_link_id(
								peer,
								hw_link_id);
		}
		ieee80211_tx_status_ext(ath12k_dp_pdev_to_hw(dp_pdev), &status);
	}
	rcu_read_unlock();
	return;

exit:
	DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[drop_reason][ring], 1);

	if (peer)
		DP_PEER_STATS_PKT_LEN(peer, tx, ring, tx_dropped, hw_link_id,
				      1, msdu_len);

	if (ahvif && ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev))
		ath12k_tid_tx_drop_stats(ahvif, tid, msdu_len, reason);

	if (sw_metadata->flags & DP_TX_DESC_FLAG_FAST)
		dev_kfree_skb_any(msdu);
	else
		ieee80211_free_txskb(ath12k_dp_pdev_to_hw(dp_pdev), msdu);

	rcu_read_unlock();
}

void
ath12k_wifi7_dp_tx_status_parse(struct ath12k_base *ab,
				struct hal_wbm_completion_ring_tx *desc,
				struct hal_tx_status *ts)
{
	u32 info0 = le32_to_cpu(desc->rate_stats.info0);

	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
	    ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)
		return;

	ts->ppdu_id = le32_get_bits(desc->info1,
				    HAL_WBM_COMPL_TX_INFO1_TQM_STATUS_NUMBER);

	ts->ack_rssi = FIELD_GET(HAL_WBM_COMPL_TX_INFO2_ACK_FRAME_RSSI,
				 desc->info2);

	ts->peer_id = le32_get_bits(desc->info3, HAL_WBM_COMPL_TX_INFO3_PEER_ID);

	if (info0 & HAL_TX_RATE_STATS_INFO0_VALID) {
		ts->pkt_type = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_PKT_TYPE);
		ts->mcs = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_MCS);
		ts->sgi = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_SGI);
		ts->bw = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_BW);
		ts->tones = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_TONES_IN_RU);
		ts->ofdma = u32_get_bits(info0, HAL_TX_RATE_STATS_INFO0_OFDMA_TX);
	}

	ts->tid = FIELD_GET(HAL_WBM_RELEASE_TX_INFO3_TID, desc->info3);
	ts->transmit_cnt = le32_get_bits(desc->info1,
					 HAL_WBM_COMPL_TX_INFO1_TRANSMIT_COUNT);

	ts->delay_stats.ts.buffer_timestamp =
		FIELD_GET(HAL_WBM_RELEASE_TX_INFO2_BUFFER_TIMESTAMP, desc->info2);
	ts->delay_stats.ts.tsf = desc->rate_stats.tsf;

	ts->first_msdu = le32_get_bits(desc->info2,
				       HAL_WBM_COMPL_TX_INFO2_FIRST_MSDU);
	ts->last_msdu = le32_get_bits(desc->info2,
				      HAL_WBM_COMPL_TX_INFO2_LAST_MSDU);
	ts->msdu_part_of_amsdu =
			(ts->first_msdu && ts->last_msdu) ? false : true;

	ath12k_wifi7_dp_tx_get_hw_link_id_from_ppdu_id(ts, ab->dp);
}

static void ath12k_dp_tx_update_tid_stats(struct ath12k_dp *dp,
		struct ath12k_pdev_dp *dp_pdev,
		struct sk_buff *skb,
		u32 peer_id,
		int reason)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_vif *ahvif;
	u8 tid;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (dp_peer && ath12k_dp_peer_get_vif(dp_peer)) {
		ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(dp_peer));
		if (ahvif) {
			tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
			ath12k_tid_tx_stats(ahvif, tid, skb->len, reason);
		}
	}
}

int ath12k_wifi7_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_dp *dp_pdev = NULL;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_tx_desc_info *tx_desc = NULL;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	struct ath12k_dp_tx_comp_status sw_status;
	int i;
#ifndef CONFIG_IO_COHERENCY
	int valid_entries;
#endif
	int orig_budget = budget;
	bool fast_flag;
	int pdev_tx_comp_cnt[ATH12K_GROUP_MAX_RADIO] = {0};
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_wifi7_tx_status_entry *tx_status_entry;
	struct ath12k_tx_sw_metadata *sw_metadata;
	u8 n_entry = 0, idx = 0;
	struct list_head desc_free_list;
	struct hal_wbm_completion_ring_tx *tx_status;
	struct sk_buff_head free_list_head;
	int tx_status_idx = smp_processor_id();
	u32 tx_wbm_rel_source[HAL_WBM_REL_SRC_MODULE_MAX] = {0};
	u32 tqm_rel_reason[MAX_TQM_RELEASE_REASON] = {0};
	u32 fw_tx_status[MAX_FW_TX_STATUS] = {0};
	u32 htt_status = 0, tx_completed = 0;
	u32 tx_desc_free_cnt = 0, *used_cnt;
	u8 hw_link_id = 0;

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

#ifndef CONFIG_IO_COHERENCY
	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return 0;
	}

	if (valid_entries > budget)
		valid_entries = budget;

	ath12k_hal_srng_dst_invalidate_entry(dp, status_ring, valid_entries);
#endif
	INIT_LIST_HEAD(&desc_free_list);
	skb_queue_head_init(&free_list_head);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)
				dp_hw_grp->tx_status_buf[tx_status_idx];
	while (budget-- &&
	       (tx_status = __ath12k_hal_srng_dst_get_next_cached_entry(status_ring,
									NULL))) {
		if (!ath12k_wifi7_hal_tx_completion_process(tx_status,
							    &sw_status))
			continue;

		tx_desc =
			(struct ath12k_tx_desc_info *)((unsigned long)sw_status.tx_desc);

		if (unlikely(!tx_desc)) {
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err
					    [DP_TX_COMP_ERR_INVALID_DESC][ring_id], 1);
			ath12k_warn(ab, "unable to retrieve tx_desc!");
			continue;
		}

		tx_status_entry->tx_desc = tx_desc;

		n_entry++;
		memcpy(&tx_status_entry->tx_status, tx_status, sizeof(*tx_status));
		tx_status_entry++;
	}

	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	if (!n_entry)
		return orig_budget - budget;

	spin_lock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	for (i = 0; i < n_entry; i++) {
		struct ath12k_wifi7_tx_status_entry *tx_status_entry_next;
		sw_metadata = &tx_status_entry->sw_metadata;
		tx_desc = tx_status_entry->tx_desc;
		tx_status_entry++;

		if ((i + 10) < n_entry) {
			tx_status_entry_next = tx_status_entry + 8;
			prefetch(tx_status_entry_next->tx_desc);
			prefetch((tx_status_entry_next + 1));
		}

		if (unlikely(!tx_desc->in_use)) {
			sw_metadata->skb = NULL;
			DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_DESC_INUSE][ring_id], 1);
			continue;
		}

		if (likely(!tx_desc->spl_desc))
			list_add_tail(&tx_desc->list, &desc_free_list);
		else
			list_add_tail(&tx_desc->list,
				      &dp->dp_hw_grp->tx_spl_desc_free_list[ring_id]);

		tx_desc_free_cnt++;

		sw_metadata->skb = tx_desc->skb;
		sw_metadata->paddr = tx_desc->paddr;
		sw_metadata->len = tx_desc->len;
		sw_metadata->flags = tx_desc->flags;

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
		sw_metadata->mmesh = tx_desc->mmesh;
#endif

		if (unlikely(!(sw_metadata->flags & DP_TX_DESC_FLAG_FAST))) {
			if (tx_desc->ext_kmem) {
				/* Unmap SG buffers */
				if (tx_desc->is_from_sg) {
					ath12k_dp_tx_sg_unmap_buf(dp, tx_desc->ext_desc,
								  tx_desc->skb);
					tx_desc->is_from_sg = 0;
				}
				ath12k_core_dma_unmap_single(dp->dev,
							     tx_desc->paddr_ext_desc,
							     tx_desc->ext_desc_len,
							     DMA_TO_DEVICE);
				kmem_cache_free(dp->ext_cache, tx_desc->ext_desc);
				tx_desc->paddr_ext_desc = 0;
				tx_desc->ext_desc_len = 0;
				tx_desc->ext_desc = NULL;
				tx_desc->ext_kmem = 0;
			}
		}

		sw_metadata->hw_link_id = tx_desc->hw_link_id;
		sw_metadata->hw_enqueue_tstamp = tx_desc->hw_enqueue_tstamp;
		pdev_tx_comp_cnt[sw_metadata->hw_link_id]++;

		tx_desc->skb = NULL;
		tx_desc->in_use = false;
		tx_desc->mmesh = 0;
		tx_desc->flags = 0;
		tx_desc->to_fw = 0;
	}

	list_splice(&desc_free_list, &dp->dp_hw_grp->tx_desc_free_list[ring_id]);

	used_cnt = this_cpu_ptr(dp_hw_grp->tx_desc_used_cnt);
	(*used_cnt) -= tx_desc_free_cnt;

	spin_unlock_bh(&dp->dp_hw_grp->tx_desc_lock[ring_id]);

	for (hw_link_id = 0; hw_link_id < ATH12K_GROUP_MAX_RADIO; hw_link_id++) {
		if (likely(pdev_tx_comp_cnt[hw_link_id])) {
			rcu_read_lock();
			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
			rcu_read_unlock();

			if (unlikely(!dp_pdev))
				continue;

			if (atomic_sub_and_test(pdev_tx_comp_cnt[hw_link_id],
						&dp_pdev->num_tx_pending))
				wake_up(&dp_pdev->tx_empty_waitq);
		}
	}

	tx_status_entry = (struct ath12k_wifi7_tx_status_entry *)dp_hw_grp->tx_status_buf[tx_status_idx];
	while (n_entry--) {
		fast_flag = false;
		tx_status = &tx_status_entry->tx_status;
		sw_metadata = &tx_status_entry->sw_metadata;

		tx_status_entry++;

		if (!sw_metadata->skb)
			continue;

		tx_completed++;
		ts.buf_rel_source =
			le32_get_bits(tx_status->info0, HAL_WBM_COMPL_TX_INFO0_REL_SRC_MODULE);

		tx_wbm_rel_source[ts.buf_rel_source]++;


                if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
                        ts.status = le32_get_bits(tx_status->info0,
                                                  HAL_WBM_COMPL_TX_INFO0_TQM_RELEASE_REASON);
                        tqm_rel_reason[ts.status]++;
                } else if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW) {
                        htt_status = le32_get_bits(tx_status->info0,
                                                   HTT_TX_WBM_COMP_INFO0_STATUS);
                        fw_tx_status[htt_status]++;

			if (dp_pdev && ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				ath12k_dp_tx_update_tid_stats(dp, dp_pdev,
							      sw_metadata->skb,
							      ts.peer_id,
							      ATH_TX_FW_STATUS);
				rcu_read_unlock();
			}
			ath12k_wifi7_dp_tx_process_htt_tx_complete(dp, (void *)tx_status,
								   sw_metadata->skb,
								   tx_ring, sw_metadata,
								   &ts, ring_id, htt_status);
			sw_metadata->skb = NULL;
			continue;
		}

		if ((sw_metadata->flags & DP_TX_DESC_FLAG_FAST) &&
		    dp_pdev && !ath12k_dp_stats_enabled(dp_pdev)) {
			if (likely(sw_metadata->flags & DP_TX_DESC_FLAG_RECYCLE)) {
				ath12k_dp_tx_buffer_unmap(dp->dev, sw_metadata->paddr,
							  sw_metadata->len,
							  DMA_TO_DEVICE);
				__skb_queue_head(&free_list_head, sw_metadata->skb);
				sw_metadata->skb = NULL;
				fast_flag = true;
			}
		}

		if (n_entry == 1)
			prefetch(&dp->device_stats);

		if (unlikely(!fast_flag && sw_metadata->skb)) {
			rcu_read_lock();

			dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp,
							      sw_metadata->hw_link_id);
			if (!dp_pdev) {
				DP_DEVICE_STATS_INC(dp, tx_err.tx_comp_err[DP_TX_COMP_ERR_INVALID_PDEV][ring_id], 1);
				rcu_read_unlock();
				continue;
			}

			ath12k_wifi7_dp_tx_status_parse(ab, tx_status, &ts);
			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				ath12k_dp_tx_update_tid_stats(dp, dp_pdev,
							      sw_metadata->skb,
							      ts.peer_id,
							      ATH_TX_COMPLETED_PKTS);
				if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM)
					ath12k_dp_tx_update_tid_stats(dp, dp_pdev,
								      sw_metadata->skb,
								      ts.peer_id,
								      ATH_TX_WBM_REL_SRC);
			}
			ath12k_wifi7_dp_tx_complete_msdu(dp_pdev, sw_metadata->skb, &ts,
							 sw_metadata, ring_id);
			sw_metadata->skb = NULL;

			rcu_read_unlock();
		}
	}

        dp->device_stats.tx_comp_stats[ring_id].tx_completed += tx_completed;

        for (idx = 0; idx < HAL_WBM_REL_SRC_MODULE_MAX; idx++)
                dp->device_stats.tx_comp_stats[ring_id].tx_wbm_rel_source[idx] += tx_wbm_rel_source[idx];

        for (idx = 0; idx < MAX_TQM_RELEASE_REASON; idx++)
                dp->device_stats.tx_comp_stats[ring_id].tqm_rel_reason[idx] += tqm_rel_reason[idx];

        for (idx = 0; idx < MAX_FW_TX_STATUS; idx++)
                dp->device_stats.tx_comp_stats[ring_id].fw_tx_status[idx] += fw_tx_status[idx];

	dev_kfree_skb_list_fast(&free_list_head);

	return orig_budget - budget;
}

u32 ath12k_wifi7_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable)
{
	u32 bank_config = 0;
	enum hal_encrypt_type encrypt_type = 0;
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	u32 key_cipher = ahvif->deflink.key_cipher;
	bool vdev_id_check_en;

	if (force_vdev_id_check_disable)
		vdev_id_check_en = false;
	else
		vdev_id_check_en = dp_vif->vdev_id_check_en;

	/* Only valid for raw frames with HW crypto enabled.
	 * With SW crypto, mac80211 sets key per packet
	 */
	if (dp_vif->tx_encap_type == HAL_TCL_ENCAP_TYPE_RAW &&
	    test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ab->ag->flags) &&
	    key_cipher != INVALID_CIPHER)
		bank_config |=
			u32_encode_bits(ath12k_dp_tx_get_encrypt_type(key_cipher),
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);
	else
		encrypt_type = HAL_ENCRYPT_TYPE_OPEN;

	bank_config |= u32_encode_bits(dp_vif->tx_encap_type,
					HAL_TX_BANK_CONFIG_ENCAP_TYPE) |
					u32_encode_bits(encrypt_type,
					HAL_TX_BANK_CONFIG_ENCRYPT_TYPE);

	bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_SRC_BUFFER_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_LINK_META_SWAP) |
			u32_encode_bits(0, HAL_TX_BANK_CONFIG_EPD);

	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		bank_config |= u32_encode_bits(1, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);
	else
		bank_config |= u32_encode_bits(0, HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN);

	bank_config |= u32_encode_bits(dp_vif->hal_addr_search_flags & HAL_TX_ADDRX_EN,
					HAL_TX_BANK_CONFIG_ADDRX_EN) |
			u32_encode_bits(!!(dp_vif->hal_addr_search_flags &
					HAL_TX_ADDRY_EN),
					HAL_TX_BANK_CONFIG_ADDRY_EN);

	bank_config |= u32_encode_bits(ieee80211_vif_is_mesh(ahvif->vif) ? 3 : 0,
					HAL_TX_BANK_CONFIG_MESH_EN) |
			u32_encode_bits(vdev_id_check_en,
					HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN);

	bank_config |= u32_encode_bits(ahvif->map_id, HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID);

	return bank_config;
}

int ath12k_wifi7_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb, struct ath12k_link_sta *arsta,
				       struct ath12k_dp_peer *dp_peer)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath12k_dp_vif *dp_vif = &arvif->ahvif->dp_vif;
	u32 info_flags = info->flags;
	bool is_mcast = false, is_eth = false;
	struct ieee80211_hdr *hdr;
	struct ethhdr *eth;
	struct ath12k_dp_skb_ctrl skb_ctrl = {0};

	/* Native WiFi format */
	hdr = (struct ieee80211_hdr *)skb->data;

	/* Check if HW encapsulation */
	if (info_flags & IEEE80211_TX_CTL_HW_80211_ENCAP) {
		eth = (struct ethhdr *)skb->data;
		is_eth = true;
		is_mcast = is_multicast_ether_addr(eth->h_dest);
	} else {
		is_mcast = is_multicast_ether_addr(hdr->addr1);
	}

	if (is_mcast)
		ath12k_wifi7_mcbc_handler(dp_vif, arvif->link_id, arsta, skb,
					  is_eth, false, false, NULL, &skb_ctrl,
					  info, 0, false, NULL);
	else
		ath12k_wifi7_ucast_handler(dp_vif, arvif->link_id,
					   arsta, skb, &skb_ctrl, 0, false,
					   dp_peer);
	return 0;
}

void ath12k_wifi7_dp_tx_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	int i;

	IPA_TX_BUFFER_FREE(ab);
	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_comp_ring);
		ath12k_dp_srng_cleanup(ab, &dp->tx_ring[i].tcl_data_ring);
	}
}

int ath12k_wifi7_dp_tx_ring_alloc(struct ath12k_base *ab)
{
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	struct ath12k_dp *dp = ab->dp;
	int i, ret, tx_comp_ring_num;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;

		ret = ath12k_dp_srng_alloc(ab, &dp->tx_ring[i].tcl_data_ring,
					   HAL_TCL_DATA, i, 0,
					   ath12k_dp_tcl_data_ring_size[i]);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_data ring (%d) :%d\n",
				    i, ret);
			goto err;
		}

		ret = ath12k_dp_srng_alloc(ab, &dp->tx_ring[i].tcl_comp_ring,
					   HAL_WBM2SW_RELEASE, tx_comp_ring_num, 0,
					   ath12k_dp_tx_comp_ring_size[i]);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			goto err;
		}
	}

	IPA_TX_BUFFER_ALLOC(ab);

	return 0;

err:
	ath12k_wifi7_dp_tx_ring_cleanup(ab);
	return ret;
}

int ath12k_wifi7_dp_tx_ring_init(struct ath12k_base *ab)
{
	const struct ath12k_hal_tcl_to_cmp_rbm_map *map;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret, tx_comp_ring_num;
	u8 rbm_id;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++) {
		map = ab->hal.tcl_to_cmp_rbm_map;
		tx_comp_ring_num = map[i].cmp_ring_num;
		rbm_id = map[i].rbm_id;

		ret = ath12k_dp_srng_init(ab, &dp->tx_ring[i].tcl_data_ring,
					  HAL_TCL_DATA, i, 0);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_data ring (%d) :%d\n",
				    i, ret);
			return ret;
		}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_hal_tx_config_rbm_mapping(ab, i, rbm_id, HAL_TCL_DATA);
#endif

		ret = ath12k_dp_srng_init(ab, &dp->tx_ring[i].tcl_comp_ring,
					  HAL_WBM2SW_RELEASE, tx_comp_ring_num, 0);
		if (ret) {
			ath12k_warn(ab, "failed to set up tcl_comp ring (%d) :%d\n",
				    tx_comp_ring_num, ret);
			return ret;
		}
	}

	return 0;
}

int ath12k_wifi7_dp_tx_ring_setup(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wifi7_dp_tx_ring_alloc(ab);
	if (ret)
		return ret;

	ret = ath12k_wifi7_dp_tx_ring_init(ab);
	if (ret) {
		ath12k_wifi7_dp_tx_ring_cleanup(ab);
		return ret;
	}

	return 0;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static inline void
ath12k_dp_tx_ppeds_update_peer_basic_stats(struct ath12k_dp_peer *dp_peer,
					   u32 status, int link_id)
{
	/* Ring ID and MSDU len are not available for PPEDS completions */
	DP_PEER_STATS_INC(dp_peer, tx, ATH12K_DP_PPEDS_RING_ID, comp_pkt.packets,
			  link_id, 1);
	if (status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
		DP_PEER_STATS_INC(dp_peer, tx, ATH12K_DP_PPEDS_RING_ID,
				  tx_success.packets, link_id, 1);
	else
		DP_PEER_STATS_INC(dp_peer, tx, ATH12K_DP_PPEDS_RING_ID,
				  tx_failed, link_id, 1);
}

static inline void
ath12k_dp_tx_ppeds_update_peer_debug_stats(struct ath12k_dp_peer *dp_peer,
					   struct hal_tx_status *ts,
					   u8 link_id)
{
	if (ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM) {
		DP_PEER_DBG_STATS_INC(dp_peer, tx_dbg, release_src_not_tqm,
				      link_id, 1);
		DP_PEER_DBG_STATS_INC(dp_peer, tx_dbg, wbm_rel_reason[ts->status],
				      link_id, 1);
		return;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, retry_count, link_id,
					   ts->transmit_cnt > 1, 1);
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, total_msdu_retries,
					   link_id, ts->transmit_cnt > 1,
					   ts->transmit_cnt - 1);
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, multiple_retry_count,
					   link_id, ts->transmit_cnt > 2, 1);
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, ofdma, link_id,
					   ts->ofdma, 1);
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, amsdu_cnt, link_id,
					   ts->msdu_part_of_amsdu, 1);
		DP_PEER_DBG_STATS_COND_INC(dp_peer, tx_dbg, non_amsdu_cnt, link_id,
					   !ts->msdu_part_of_amsdu, 1);
	}

	if (ts->status < HAL_WBM_TQM_REL_REASON_MAX) {
		DP_PEER_DBG_STATS_INC(dp_peer, tx_dbg, tqm_rel_reason[ts->status],
				      link_id, 1);
	}
}

void ath12k_ppeds_tx_update_stats(struct ath12k *ar, int skb_len,
				  struct hal_wbm_completion_ring_tx *tx_status)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp;
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_vif *ahvif;
	struct hal_tx_status ts = { 0 };
	bool tx_drop = false;
	bool tx_status_default = false;
	u8 reason, link_id = 0;
	int ring_id = 0;
	int vow_tid = 0;
	u8 hw_link_id = 0;
	u32 hw_delay = 0;
	u32 fwhw_transmit_delay = 0;

	dp = ath12k_ab_to_dp(ab);
	ath12k_wifi7_dp_tx_status_parse(ab, tx_status, &ts);

	if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
		ts.status = le32_get_bits(tx_status->info0,
					  HAL_WBM_COMPL_TX_INFO0_TQM_RELEASE_REASON);
		dp->ppe.ppeds_stats.tqm_rel_reason[ts.status]++;
	}

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
		switch (ts.status) {
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU:
			reason = ATH_TX_DS_TQM_REMOVE_MPDU;
			break;
		case HAL_WBM_TQM_REL_REASON_DROP_THRESHOLD:
			reason = ATH_TX_DS_TQM_DROP_THRESHOLD;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX:
			reason = ATH_TX_DS_TQM_REMOVE_TX;
			break;
		case HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES:
			reason = ATH_TX_DS_TQM_REMOVE_AGED;
			break;
		default:
			reason = ATH_TX_DS_TQM_REMOVE_DEF;
			//TODO: Remove this print and add as a stats
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "tx frame is not acked status %d\n",
				   ts.status);
			tx_status_default = true;
		}
		tx_drop = true;
	}

	rcu_read_lock();

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, ts.peer_id);
	if (unlikely(!peer || !peer->dp_peer || !ath12k_dp_link_peer_get_sta(peer) ||
		     !ath12k_dp_link_peer_get_vif(peer))) {
		rcu_read_unlock();
		return;
	}

	hw_link_id = ath12k_dp_validate_hw_link_id(ts.hw_link_id);
	link_id = ath12k_dp_peer_convert_hw_to_logical_link_id(peer->dp_peer,
							       hw_link_id);

	/* Update peer TX statistics for PPE DS offload path */
	ath12k_dp_tx_ppeds_update_peer_basic_stats(peer->dp_peer, ts.status, hw_link_id);

	if (ts.status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED)
		WRITE_ONCE(peer->peer_stats.last_ack, jiffies);

	if (unlikely(ath12k_dp_stats_enabled(dp_pdev))) {
		ath12k_dp_tx_ppeds_update_peer_debug_stats(peer->dp_peer, &ts,
							   hw_link_id);
		if (unlikely(ath12k_tid_stats_enabled(dp_pdev))) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_link_peer_get_vif(peer));
			if (tx_drop) {
				ath12k_tid_tx_drop_stats(ahvif, ts.tid, skb_len,
							 reason);
			} else {
				ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
						    ATH_TX_PPEDS_PKTS);
				ath12k_tid_tx_stats(ahvif, ts.tid, skb_len,
						    ATH_TX_COMPLETED_PKTS);
			}
		}

		if (unlikely(ath12k_dp_vow_stats_enabled(dp_pdev))) {
			struct ath12k_tid_tx_stats *tid_tx;

			/* Track TQM status */
			vow_tid = ath12k_vow_tid_validate(ts.tid);
			tid_tx = &dp_pdev->tid_stats.tid_tx[ring_id][vow_tid];
			if (ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_TQM) {
				if (ts.status < HAL_WBM_TQM_REL_REASON_MAX)
					DP_PDEV_TID_TX_REASON_INC(dp_pdev,
								      ring_id,
								      vow_tid,
								      tqm_status_cnt,
								      ts.status);
			}
			ath12k_wifi7_compute_hw_delay(ar, &ts, &hw_delay);
			if (hw_delay <= HW_TX_DELAY_MAX)
				ath12k_dp_update_hist_stats(&tid_tx->hwtx_delay,
							    hw_delay / USEC_PER_MSEC);
		}
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_dp_latency_stats_enabled(dp_pdev)) {
		fwhw_transmit_delay = ath12k_wifi7_dp_tx_compute_hw_delay(dp_pdev, &ts);
		ath12k_dp_tx_compute_hw_delay_stats(dp_pdev, peer->dp_peer,
						    DP_TCL_PPEDS_RING_IDX, &ts,
						    fwhw_transmit_delay);
	}

	if (ts.status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !tx_status_default) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();
}

int ath12k_wifi7_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k *ar;
	struct ath12k_pdev_dp *dp_pdev;
	struct dp_ppeds_tx_comp_ring *tx_ring = &dp->ppe.ppeds_comp_ring;
	int hal_ring_id = tx_ring->ppeds_txcmpl_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_ppeds_tx_desc_info *tx_desc = NULL;
	struct ath12k_dp_tx_comp_status tx_status;
	int valid_entries, count = 0;
	int list_no_skb_count = 0;
	struct htt_tx_completion *status_desc;
	struct hal_wbm_completion_ring_tx *desc;
	struct list_head local_list;
	struct list_head local_list_no_skb;
	size_t stat_size;
	int htt_status;

	if (WARN_ON_ONCE(unlikely(budget > DP_PPEDS_SERVICE_BUDGET))) {
		ath12k_err(ab, "Invalid Budget");
		return -EINVAL;
	}

	if (likely(ab->stats_disable))
		/* only need buf_addr_info and info0 */
		stat_size = 3 * sizeof(u32);
	else
		stat_size = status_ring->entry_size;
	INIT_LIST_HEAD(&local_list);
	INIT_LIST_HEAD(&local_list_no_skb);

	ath12k_hal_srng_access_dst_ring_begin_nolock(ab, status_ring);

	valid_entries = __ath12k_hal_srng_dst_num_free(status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);
		return count;
	}

	if (valid_entries >= budget)
		valid_entries = budget;

	ath12k_hal_srng_ppeds_dst_inv_entry(ab, status_ring, valid_entries);

	while (likely(valid_entries--)) {
		desc = ath12k_hal_srng_dst_get_next_cached_entry(ab, status_ring, NULL);
		if (!desc || !ath12k_wifi7_hal_tx_completion_process(desc, &tx_status))
			continue;

		if (likely(!ab->stats_disable))
			memcpy(((void *)tx_ring->tx_status) +
			       (count * status_ring->entry_size),
			       desc, stat_size);

		ath12k_dp_ppeds_tx_comp_get_desc(ab, &tx_status, &tx_desc);

		if (unlikely(!tx_desc)) {
			ath12k_warn(ab, "unable to retrieve ppe ds tx_desc!");
			continue;
		}
		tx_ring->macid[count] = tx_desc->mac_id;

		if (unlikely(tx_status.buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW)) {
			status_desc = (void *)desc;

			htt_status = tx_status.u.htt_status;

			if (htt_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ)
				ath12k_ppeds_reinject_handler(ab, tx_desc, status_desc);

			if (htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_OK &&
			    htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ) {
				ab->dp->ppe.ppeds_stats.fw2wbm_pkt_drops++;
				ath12k_dbg(ab, ATH12K_DBG_PPE,
					   "ath12k: Frame received from unexpected source %d status %d!\n",
					   tx_status.buf_rel_source,
					   htt_status);
			}
			tx_ring->macid[count] = 0xF;
		}
		/* add descriptor to local list to process in bulk */
		tx_desc->in_use = false;
		if (likely(tx_desc->skb)) {
			list_add_tail(&tx_desc->list, &local_list);
			if (tx_ring->macid[count] != 0xF) {
				ar = ab->pdevs[tx_ring->macid[count]].ar;
				dp_pdev = &ar->dp;
				if (ath12k_dp_stats_enabled(dp_pdev))
					ath12k_ppeds_tx_update_stats(ar,
								     tx_desc->skb->len,
								     desc);
			}
			count++;
		} else {
			list_add_tail(&tx_desc->list, &local_list_no_skb);
			list_no_skb_count++;
		}
	}
	ath12k_hal_srng_access_dst_ring_end_nolock(status_ring);

	ath12k_dp_ppeds_tx_release_desc_list_bulk(dp->ab, dp->dp_hw_grp,
						  &local_list, count,
						  &local_list_no_skb,
						  list_no_skb_count);
	return (count + list_no_skb_count);
}
#endif
