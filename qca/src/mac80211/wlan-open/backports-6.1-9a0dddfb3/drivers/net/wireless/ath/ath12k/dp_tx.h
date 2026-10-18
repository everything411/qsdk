/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_H
#define ATH12K_DP_TX_H

#include "core.h"
#include "dp_ext_desc.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/dp_tx_extn.h"
#endif

#define ATH12K_INVALID_HW_LINKID 0xFF

#define DP_SDWF_DEFINED_Q_PTID_MAX 2
#define DP_SDWF_DEFAULT_Q_PTID_MAX 2
#define DP_SDWF_TID_MAX 8

#define DP_TX_MAX_NUM_FRAGS 6

#define DP_SDWF_Q_MAX (DP_SDWF_DEFINED_Q_PTID_MAX * DP_SDWF_TID_MAX)
#define DP_SDWF_DEFAULT_Q_MAX (DP_SDWF_DEFAULT_Q_PTID_MAX * DP_SDWF_TID_MAX)

#define DP_GET_HW_LINK_ID_FRM_PPDU_ID(PPDU_ID, LINK_ID_OFFSET, LINK_ID_BITS) \
	(((PPDU_ID) >> (LINK_ID_OFFSET)) & ((1 << (LINK_ID_BITS)) - 1))

struct ath12k_tx_desc_info;
struct ath12k_dp_skb_ctrl;
struct ath12k_dp_ext_desc;
struct ath12k_dp_tx_msdu_info;

struct ath12k_dp_htt_wbm_tx_status {
	bool acked;
	s8 ack_rssi;
};

void ath12k_dp_tx_put_bank_profile(struct ath12k_dp *dp, u8 bank_id);
u32 ath12k_dp_tx_get_bank_config_from_id(struct ath12k_dp *dp, u8 bank_id);

void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb);
void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len);
void *ath12k_dp_metadata_align_skb_head(struct sk_buff *skb, u8 head_len);
int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb);
void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id);
void ath12k_dp_tx_release_txbuf_nolock(struct ath12k_dp *dp,
				       struct ath12k_tx_desc_info *tx_desc,
				       u8 pool_id);
struct
ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp_hw_group *dp_hw_grp,
						struct list_head *free_list,
						u8 pool_id);
int ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask);
int ath12k_sdwf_reinject_handler(struct ath12k_base *ab, struct sk_buff *skb,
				 struct htt_tx_completion *status_desc, u8 mac_id);
int ath12k_dp_mmesh_tx(struct ieee80211_hw *hw, struct ath12k_base *ab,
		       struct ath12k_link_vif *arvif, struct ieee80211_vif *vlan_vif,
		       struct sk_buff *skb, struct ieee80211_sta *sta,
		       struct ath12k_dp_skb_ctrl *skb_ctrl, bool is_eth,
		       u8 link_id, bool is_mcast, bool *htt_mesh,
		       struct ieee80211_tx_info *info, u32 qos_nw_delay);
void ath12k_dp_tx_sg_unmap_buf(struct ath12k_dp *dp,
			       struct ath12k_dp_ext_desc *ext_desc,
			       struct sk_buff *skb);
enum hal_tcl_encap_type
ath12k_dp_tx_get_encap_type(struct ath12k_base *ab, struct sk_buff *skb);
void ath12k_dp_tx_drop_tid_stats(struct ath12k_dp_vif *dp_vif,
				 enum ath12k_dp_tx_enq_error drop_reason,
				 u8 tid, u32 len);
void ath12k_dp_tx_drop_pdev_tid_stats(struct ath12k_pdev_dp *dp_pdev,
				      enum ath12k_dp_tx_enq_error drop_reason,
				      u8 tid, u8 ring_id);
void ath12k_dp_tx_stats_update_pre_enqueue(struct ath12k_pdev_dp *dp_pdev,
					   struct ath12k_dp_vif *dp_vif,
					   struct sk_buff *skb,
					   u8 ring_id, u32 len);
void ath12k_dp_tx_stats_post_enqueue(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     struct ath12k_dp_vif *dp_vif,
				     struct sk_buff *skb,
				     struct ath12k_dp_tx_msdu_info *msdu_info,
				     u8 ring_id, u32 len, bool is_mcast,
				     struct ath12k_tx_desc_info *tx_desc);
int ath12k_dp_tx_get_mcast_group_slot(struct ath12k_vif *vlan_ahvif,
				      u8 link_id);
int ath12k_dp_sg_ext_desc_populate(struct ath12k_dp *dp,
				   struct ath12k_dp_vif *dp_vif,
				   struct ath12k_dp_ext_desc *ext_desc,
				   struct sk_buff *skb, u8 ring_id);

/**
 * ath12k_wifi7_tx_classify_packet() - Classify packet type
 * @info: TX info
 * @skb: Socket buffer
 * @is_mcast: Output multicast flag
 * @is_mgmt: Output management flag
 *
 */
bool ath12k_dp_tx_classify_packet(struct ieee80211_hw *hw,
				  struct ath12k_dp_vif *dp_vif,
				  struct ieee80211_tx_info *info,
				  struct sk_buff *skb,
				  bool *is_mcast, bool *is_eth,
				  bool *data,
				  struct ieee80211_key_conf *key,
				  struct ath12k_dp_skb_ctrl *skb_ctrl);

#ifndef CPTCFG_QCN_EXTN
bool ath12k_dp_tx_dma_map(struct ath12k_dp *dp,
			  struct sk_buff *skb, u32 len,
			  struct ath12k_tx_desc_info *tx_desc,
			  struct ath12k_dp_tx_msdu_info *msdu_info,
			  struct ath12k_dp_skb_ctrl *skb_ctrl);

/**
 * ath12k_dp_tx_buffer_unmap() - Unmap TX DMA mapping
 * @dev: DMA device used for mapping
 * @dma_handle: DMA address returned by the map operation
 * @size: Size of the mapped region
 * @direction: DMA data direction used during mapping
 *
 * Wrapper around dma_unmap_single() for TX buffers on non-coherent platforms.
 */
static inline
void ath12k_dp_tx_buffer_unmap(struct device *dev,
			       dma_addr_t dma_handle,
			       size_t size,
			       enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_handle, size, direction);
}

static inline void ath12k_dmb(void)
{
}
#endif

#ifdef PLATFORM_SDX
#define ATH12K_TX_BUFFER_UNMAP(dev, paddr, len, dir) \
	ath12k_dp_tx_buffer_unmap(dev, paddr, len, dir)
#else
#define ATH12K_TX_BUFFER_UNMAP(...) ((void)0)
#endif

#define ATH12K_TX_BUFFER_MAP(dp, skb, len, tx_desc, msdu_info, skb_ctrl) \
	ath12k_dp_tx_dma_map(dp, skb, len, tx_desc, msdu_info, skb_ctrl)

/*
 * DP TX Feature Bitmap (stored in ath12k_dp_vif.dp_features, 32-bit).
 *
 * Each bit enables a specific TX datapath feature for a virtual interface.
 * Features are checked per-packet in ath12k_wifi7_dp_tx_process_features().
 *
 * DP_FEATURE_NONE                 - No features enabled; fast-path Ethernet offload only.
 * DP_FEATURE_RAW_MODE             - RAW 802.11 frame encapsulation (no HW encap).
 * DP_FEATURE_NATIVE_WIFI          - Native WiFi encapsulation (802.11 header present).
 * DP_FEATURE_ENCAP_MISMATCH_HANDLE- Handle frames whose encap type
 *                                   differs from vdev config
 *                                   (e.g. EAPOL or null-func on Ethernet-mode vdev).
 * DP_FEATURE_HLOS                 - HLOS TID override: use skb->priority as TID.
 * DP_FEATURE_MAC_ENCRPT           - MAC added encryption frames.
 * DP_FEATURE_MESH                 - Mesh mode TX processing.
 * DP_ETH_OFFLOAD                  - Ethernet fast-path offload (SFE/recycler path).
 * DP_FEATURE_STATS                - Per-vif TX statistics collection.
 * DP_FEATURE_ME                   - Multicast Enhancement (ME) replication.
 * DP_FEATURE_SG                   - Scatter-Gather for non-linear skbs
 */
#define DP_FEATURE_NONE                  0x00000000
#define DP_FEATURE_RAW_MODE              BIT(0)
#define DP_FEATURE_NATIVE_WIFI           BIT(1)
#define DP_FEATURE_ENCAP_MISMATCH_HANDLE BIT(2)
#define DP_FEATURE_HLOS                  BIT(3)
#define DP_FEATURE_SW_ENCRPT		 BIT(4)
#define DP_FEATURE_MESH                  BIT(5)
#define DP_ETH_OFFLOAD                   BIT(6)
#define DP_FEATURE_STATS                 BIT(7)
#define DP_FEATURE_ME                    BIT(8)
#define DP_FEATURE_SG                    BIT(9)

/*
 * DP_SKB_* flags stored in skb control block (cb)
 *
 * DP_SKB_MAC_CTRL:
 *   Indicates that the packet is received from the MAC layer.
 *   Such packets follow a MAC-controlled transmit or handling path
 *   instead of the normal data transmit flow.
 *
 * DP_SKB_RECYCLED:
 *   Indicates that the packet is received from the recycler.
 *   The skb is reused from a recycled buffer pool and should not be
 *   treated as a freshly allocated skb.
 *
 * DP_SKB_FAST_TX:
 *   Indicates that the packet is received on the Fast TX path.
 *   These packets bypass normal processing and are transmitted using
 *   the fast transmit mechanism for reduced latency.
 */
#define DP_SKB_MAC_CTRL   BIT(0)
#define DP_SKB_RECYCLED   BIT(1)
#define DP_SKB_FAST_TX    BIT(2)

/**
 * DP_FEATURE_IS_NONE - Test whether a vif has no TX features enabled
 * @dp_vif: Pointer to the DP virtual interface
 *
 * Returns true when dp_features == DP_FEATURE_NONE, indicating the vif
 * uses the Ethernet fast-path only with no additional feature processing.
 */
#define DP_FEATURE_IS_NONE(dp_vif) \
	((dp_vif)->dp_features == DP_FEATURE_NONE)

/**
 * DP_FEATURE_IS_ANY - Test whether a vif has any TX feature enabled
 * @dp_vif: Pointer to the DP virtual interface
 *
 * Returns true when dp_features != DP_FEATURE_NONE, indicating the vif
 * requires additional feature processing beyond the Ethernet fast-path.
 */

#define DP_FEATURE_IS_ANY(dp_vif) \
	((dp_vif)->dp_features != 0)

/**
 * ath12k_dp_feature_enabled - Test whether a specific feature is enabled on a vif
 * @dp_vif: Pointer to the DP virtual interface
 * @mask:   Feature bit(s) to test (e.g. DP_FEATURE_RAW_MODE)
 *
 * When @mask is DP_FEATURE_NONE, returns true only if no features are set.
 * Otherwise returns true if any bit in @mask is set in dp_vif->dp_features.
 *
 * Using a static inline avoids checkpatch MACRO_ARG_REUSE warnings and prevents
 * side-effects if callers pass expressions as arguments.
 */
/*
 * NOTE: dp_tx.h cannot dereference struct ath12k_dp_vif because it may be
 * forward-declared only in translation units that include this header.
 * Keep this as a macro to avoid build failures due to incomplete types.
 */
#define ath12k_dp_feature_enabled(__dp_vif, __mask)			\
	(((__mask) == DP_FEATURE_NONE) ?					\
	 ((__dp_vif)->dp_features == DP_FEATURE_NONE) :			\
	 !!((__dp_vif)->dp_features & (__mask)))

/**
 * DP_SKB_FEATURE_EXACT - Test whether the per-SKB feature byte exactly matches
 * @dp_skb_features: Per-SKB feature byte (u8)
 * @feature:         Expected exact feature value to compare against
 *
 * Returns true only when dp_skb_features equals @feature exactly (no other
 * bits set). Used in the fast path to detect the common case where only
 * DP_ETH_OFFLOAD is set and no other per-SKB features are active.
 */
#define DP_SKB_FEATURE_EXACT(dp_skb_features, feature) \
	(!!(((dp_skb_features) == (feature))))

/**
 * DP_SKB_FEATURE_ENABLED - Test whether a specific per-SKB feature bit is set
 * @dp_skb_features: Per-SKB feature byte (u8)
 * @feature:         Feature bit to test (e.g. DP_FEATURE_DVLAN, DP_ETH_OFFLOAD)
 *
 * Returns non-zero if @feature bit is set in dp_skb_features. Used to check
 * per-packet features that are determined at the mac80211 TX entry point
 * (e.g. DVLAN tag detected, Ethernet offload eligible).
 */
#define DP_SKB_FEATURE_ENABLED(dp_skb_features, feature) \
	((dp_skb_features) & (feature))

/**
 * enum ath12k_dp_feature_result - Return codes from TX feature handler functions
 * @DP_TX_FEATURE_SUCCESS: Feature processed successfully; continue TX pipeline.
 * @DP_TX_FEATURE_SKIP:    Feature not applicable to this frame; skip silently.
 * @DP_TX_RETURN:          Feature consumed the frame (e.g. ME replication);
 *                         caller should not enqueue the original SKB to HW.
 * @DP_TX_ERROR:           Feature processing failed; drop the frame.
 *
 * Returned by each feature handler (raw mode, nwifi, dvlan, encap mismatch,
 * ME) called from ath12k_wifi7_dp_tx_process_features(). The caller uses
 * this to decide whether to continue, skip, return early, or drop.
 */
enum ath12k_dp_feature_result {
	DP_TX_FEATURE_SUCCESS = 0,
	DP_TX_FEATURE_SKIP,
	DP_TX_RETURN,
	DP_TX_ERROR,
};

/**
 * enum ath12k_dp_ext_desc_feature - Extended TX descriptor feature type selector
 * @DP_EXT_ME5:            Multicast Enhancement with 5-tuple replication.
 *                         Copies destination MAC into the spare area of the
 *                         extended descriptor and sets buf0/buf1 pointers.
 * @DP_EXT_ENCAP_OVERRIDE: Override the encapsulation and/or encryption type
 *                         for this frame (e.g. RAW encap, OPEN encrypt for
 *                         EAPOL, null-func, or DVLAN frames).
 * @DP_EXT_RAW:            RAW 802.11 frame with extended descriptor (reserved).
 * @DP_EXT_TSO:            TCP Segmentation Offload (reserved for future use).
 * @DP_EXT_SG:             Scatter-Gather TX (reserved for future use).
 *
 * Stored in ath12k_dp_ext_desc_msdu_info.ext_feature and used by
 * ath12k_wifi7_dp_ext_desc_populate() to select the descriptor fill path.
 */
enum ath12k_dp_ext_desc_feature {
	DP_EXT_ME5 = 0,
	DP_EXT_ENCAP_OVERRIDE,
	DP_EXT_RAW,
	DP_EXT_TSO,
	DP_EXT_SG,
	DP_EXT_MESH,
};

/**
 * struct ath12k_dp_ext_desc_msdu_info - Extended descriptor MSDU metadata
 * @encap_type: TCL encapsulation type (enum hal_tcl_encap_type, stored as u8)
 * @encrypt_type: Encryption type (enum hal_encrypt_type, stored as u8)
 * @ext_feature: Extended descriptor feature selector
 *               (enum ath12k_dp_ext_desc_feature, stored as u8)
 * @add_htt_metadata: Whether to add HTT MSDU metadata to the extended descriptor
 * @peer_mac_addr: Destination MAC address (used for ME5 multicast expansion)
 *
 */
struct ath12k_dp_ext_desc_msdu_info {
	u8 encap_type;		/* hal_tcl_encap_type */
	u8 encrypt_type;	/* hal_encrypt_type */
	u8 ext_feature;		/* ath12k_dp_ext_desc_feature */
	u8 add_htt_metadata	: 1,
	   reserved		: 7;
	u8 peer_mac_addr[ETH_ALEN];
};

/**
 * struct ath12k_dp_tx_msdu_info - Per-MSDU information for TCL descriptor setup
 * @paddr: DMA physical address of the MSDU data buffer (placed first for
 *         natural 8-byte alignment, eliminating padding)
 * @desc_id: SW TX descriptor cookie (SW_COOKIE field in buffer address info)
 * @data_len: Length of the MSDU data in bytes
 * @flags0: TCL data command INFO2 flags (e.g. TO_FW, PKT_OFFSET)
 * @flags1: TCL data command INFO3 flags (e.g. TID override, flow override)
 * @bss_ast_idx: BSS AST (Address Search Table) index for address lookup
 * @bss_ast_hash: BSS AST hash / cache set number
 * @meta_data_flags: HTT TCL metadata flags (peer ID, SAWF service ID, GSN)
 * @type: TCL descriptor type (enum hal_tcl_desc_type, stored as u8)
 * @bank_id: TCL bank ID for the transmitting vdev
 * @tid: Traffic Identifier for QoS prioritization
 * @lmac_id: LMAC/PMAC identifier for the transmitting radio
 * @vdev_id: Virtual device identifier
 * @rbm_id: Return Buffer Manager ID (set from tcl_to_cmp_rbm_map at enqueue)
 * @pkt_offset: Packet offset within the buffer (typically 0)
 * @lookup_override: If true, use bss_ast_idx for address lookup instead of DA
 * @group_slot: VLAN group-key slot for MPSK multicast; -1 when not applicable
 * @is_null: True if the frame is a null-function frame
 * @to_fw: True if the frame should be redirected to firmware
 * @ext_kmem: True if an extended descriptor (ext_cache slab) is required
 * @ext_desc: Extended descriptor MSDU metadata (encap/encrypt override, ME MAC)
 *
 */
struct ath12k_dp_tx_msdu_info {
	dma_addr_t paddr;

	u32 desc_id;
	u32 data_len;
	u32 flags0;
	u32 flags1;
	u32 mhdr_len;
	u32 qos_nw_delay;

	u16 bss_ast_idx;
	u16 bss_ast_hash;
	u16 meta_data_flags;

	u8 type;		/* hal_tcl_desc_type */
	u8 bank_id;
	u8 tid;
	u8 lmac_id;
	u8 vdev_id;
	u8 rbm_id;
	u8 pkt_offset;
	u8 tx_notify_frame;
	s8 group_slot;

	u8 lookup_override	: 1,
	   mpsk_diff_encap	: 1,
	   is_null		: 1,
	   to_fw		: 1,
	   ext_kmem		: 1,
	   tid_override		: 1,
	   htt_mesh		: 1,
	   me_convert		: 1,
	   reserved		: 1;

	/* Extended descriptor info */
	struct ath12k_dp_ext_desc_msdu_info ext_desc;
};

/**
 * struct ath12k_dp_skb_ctrl - Per-SKB datapath TX control
 * @features: Per-packet feature bitmap used by the TX pipeline.
 *            This is populated at the mac80211 TX entry point (or reinject
 *            path) to indicate packet-specific handling such as DVLAN,
 *            encap mismatch handling, or Ethernet offload eligibility.
 * @flags: TX descriptor flags to be programmed into the SW TX descriptor
 *         (struct ath12k_tx_desc_info::flags). These flags influence the
 *         TX completion/free path (e.g. recycler/fast free decisions).
 *
 * This control block is passed down the TX datapath to avoid re-parsing the
 * SKB and to keep the hot path decisions in a compact form.
 */
struct ath12k_dp_skb_ctrl {
	u32 features;
	u8 flags;
} __packed;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
struct ath12k_ppeds_tx_desc_info *
ath12k_dp_ppeds_tx_assign_desc_nolock(struct ath12k_dp *dp);
#endif
#endif
