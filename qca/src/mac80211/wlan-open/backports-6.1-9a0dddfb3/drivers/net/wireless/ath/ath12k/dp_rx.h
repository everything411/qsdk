/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_DP_RX_H
#define ATH12K_DP_RX_H

#include "core.h"
#include <crypto/hash.h>
#include "hal.h"

struct ath12k_sta;

#define DP_MAX_NWIFI_HDR_LEN	30
#define MAX_TP_TIDS	8

/* 17 tids for DP, 2 for mgmt, and 1 shared between DP and mgmt */
#define ATH12K_MAX_TIDS 20

#define ip_hdrlen(iph) ((iph)->ihl * 4)

#ifndef PLATFORM_SDX
#define VIRT_TO_PHYS(defrag_skb, buf_paddr) \
({ \
	(buf_paddr) = (dma_addr_t)virt_to_phys((defrag_skb)->data); \
})

#define IPA_SET_RX_BUF_SMMU_MAP(...) ((void)0)
#define IPA_SET_RX_BUF_SMMU_UNMAP(...) ((void)0)
#define ATH12K_IPA_DMA_MAP_SINGLE(...) ((void)0)
#define ATH12K_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr) \
({ \
	(void)(ab); \
	(buf_paddr) = (dma_addr_t)virt_to_phys((defrag_skb)->data); \
})
#define ATH12K_DMA_UNMAP_SINGLE(...) ((void)0)
#else
#define VIRT_TO_PHYS(...) ((void)0)
#define ATH12K_DMA_MAP_SINGLE(ab, defrag_skb, buf_paddr) do { \
	__typeof__(ab) _ab = (ab); \
	__typeof__(defrag_skb) _skb = (defrag_skb); \
	(buf_paddr) = dma_map_single((_ab)->dev, (_skb)->data, \
				     DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE); \
	if (!dma_mapping_error((_ab)->dev, (buf_paddr))) \
		ATH12K_SKB_CB(_skb)->paddr = (buf_paddr); \
	else \
		(buf_paddr) = 0; \
} while (0)
#define ATH12K_DMA_UNMAP_SINGLE(dev, paddr, size, dir) \
	ath12k_core_dma_unmap_single(dev, paddr, size, dir)
#define ATH12K_IPA_RX_BUF_DMA_BITS	32
/* LINUX_6.18 has custom slab of 3136 causing RX buffer non-page aligned */
#define ATH12K_IPA_DP_RX_BUF_SIZE	3150
#endif

struct link_peer_rx_tid_stats {
	u32 received_frm_reo_cnt                : 8,
	    received_frm_reo_bytes              : 24;
	u32 sent_to_stack_ucast                 : 8,
	    sent_to_stack_ucast_bytes           : 24;
	u32 sent_to_stack_ucast_fast            : 8,
	    sent_to_stack_ucast_fast_bytes      : 24;
	u32 sent_to_stack_mcast                 : 8,
	    sent_to_stack_mcast_bytes           : 24;
	u32 sent_to_stack_mcast_fast            : 8,
	    sent_to_stack_mcast_fast_bytes      : 24;
	u32 sg_cnt                              : 8,
	    sg_bytes                            : 24;
	u8 non_amsdu;
	u8 amsdu;
	u8 mpdu_retry;
	u8 mcast_cnt;
	u8 bcast_cnt;
} __aligned(64);

enum filter_mgmt {
	FILTER_MGMT_ASSOC_REQ                   = BIT(0),
	FILTER_MGMT_ASSOC_RESP                  = BIT(1),
	FILTER_MGMT_REASSOC_REQ                 = BIT(2),
	FILTER_MGMT_REASSOC_RESP                = BIT(3),
	FILTER_MGMT_PROBE_REQ                   = BIT(4),
	FILTER_MGMT_PROBE_RESP                  = BIT(5),
	FILTER_MGMT_TIM_ADVT                    = BIT(6),
	FILTER_MGMT_RESERVED_7                  = BIT(7),
	FILTER_MGMT_BEACON                      = BIT(8),
	FILTER_MGMT_ATIM                        = BIT(9),
	FILTER_MGMT_DISASSOC                    = BIT(10),
	FILTER_MGMT_AUTH                        = BIT(11),
	FILTER_MGMT_DEAUTH                      = BIT(12),
	FILTER_MGMT_ACTION                      = BIT(13),
	FILTER_MGMT_ACT_NO_ACK                  = BIT(14),
	FILTER_MGMT_RESERVED_15                 = BIT(15),
	FILTER_MGMT_ALL                         = ~(0)
};

enum ctrl_filter {
	FILTER_CTRL_RESERVED_1                  = BIT(0),
	FILTER_CTRL_RESERVED_2                  = BIT(1),
	FILTER_CTRL_TRIGGER                     = BIT(2),
	FILTER_CTRL_RESERVED_4                  = BIT(3),
	FILTER_CTRL_BF_REP_POLL                 = BIT(4),
	FILTER_CTRL_VHT_NDP                     = BIT(5),
	FILTER_CTRL_FRAME_EXT                   = BIT(6),
	FILTER_CTRL_CTRLWRAP                    = BIT(7),
	FILTER_CTRL_BA_REQ                      = BIT(8),
	FILTER_CTRL_BA                          = BIT(9),
	FILTER_CTRL_PSPOLL                      = BIT(10),
	FILTER_CTRL_RTS                         = BIT(11),
	FILTER_CTRL_CTS                         = BIT(12),
	FILTER_CTRL_ACK                         = BIT(13),
	FILTER_CTRL_CFEND                       = BIT(14),
	FILTER_CTRL_CFEND_CFACK                 = BIT(15),
	FILTER_CTRL_ALL                         = ~(0)
};

enum data_filter {
	FILTER_DATA_DATA                        = BIT(0),
	FILTER_DATA_NULL                        = BIT(3),
	FILTER_DATA_MCAST                       = BIT(14),
	FILTER_DATA_UCAST                       = BIT(15),
	FILTER_DATA_ALL                         = ~(0)
};

enum dp_rx_desc_pool {
	DP_RX_DEFAULT_POOL = 0,
	DP_RX_PPE_POOL = 1,
};

#define ATH12K_RX_PROTOCOL_TAG_START_OFFSET  128
#define ATH12K_ROUTE_WBM_RELEASE(ab) \
	((ab)->hw_params->route_wbm_release)
#define ATH12K_ROUTE_EAP_METADATA       (ATH12K_RX_PROTOCOL_TAG_START_OFFSET + ATH12K_PKT_TYPE_EAP)

/* cce_metadata value that signals the MSDU must be dropped */
#define CCE_DROP          0xDEAD

/* bit in fse_metadata that signals the flow must be dropped */
#define FSE_FLOW_DROP_BIT BIT(31)

struct ath12k_dp_rx_tid {
	u8 tid;
	u32 *vaddr;
	dma_addr_t paddr;
	u32 size;
	u32 pending_desc_size;
	u32 ba_win_sz;
	bool active;

	/* Info related to rx fragments */
	u32 cur_sn;
	u16 last_frag_no;
	u16 rx_frag_bitmap;

	struct sk_buff_head rx_frags;
	void *desc;

	/* Timer info related to fragments */
	struct timer_list frag_timer;
	struct ath12k_dp *dp;

	/* Info related to UMAC migration */
	u16     peer_id;
	u8      chip_id;
	u8      pdev_id;

	/* Per-TID lock protecting concurrent access to this TID's state */
	spinlock_t tid_lock;
	void    *smd_ctx;
};

/**
 * struct ath12k_dp_smd_parked_rx_info - Parked Rx Q state during SMD BSS transition
 *
 * Analogous to smd_parked_ext_ctx for the Tx side. Holds the rx_tid[] state
 * from the old peer (Current AP) so it can be transferred to the new peer
 * (Target AP) during EXEC phase, preserving BA session state and REO queue
 * descriptors.
 *
 * The vaddr/paddr in each rx_tid[] entry point to the SAME DMA memory as the
 * old peer — ownership is transferred, not copied. The frag_timer fields are
 * NOT valid after parking (must be re-initialized on restore).
 *
 * Lifetime: allocated in ath12k_wifi8_dp_smd_prep_rx_tid(), freed in
 * ath12k_wifi8_dp_smd_exec_rx_tid().
 */
struct ath12k_dp_smd_parked_rx_info {
	/* Snapshot of rx_tid[] from old peer.
	 * vaddr/paddr: DMA memory ownership transferred (not copied).
	 * frag_timer: NOT valid — re-initialized on restore.
	 * rx_frags: flushed before parking (in-flight fragments dropped).
	 */
	struct ath12k_dp_rx_tid rx_tid[ATH12K_MAX_TIDS];

	/* MMIC crypto context — ownership transferred (not duplicated) */
	struct crypto_shash *tfm_mmic;

	/* Number of valid TIDs (from ab->hal.hal_params->num_tids) */
	u8 num_tids;

	/* Is this parked state valid and ready for EXEC restore? */
	bool valid;

	/* Completion for synchronous REO flush during PREP phase.
	 * Signaled by ath12k_wifi8_dp_smd_rx_flush_done() callback when
	 * the FLUSH_CACHE command completes.
	 */
	struct completion flush_done;
};

void ath12k_dp_rx_tid_free_desc(struct ath12k_base *ab,
				struct ath12k_dp_rx_tid *rx_tid);

struct ath12k_dp_rx_reo_cache_flush_elem {
	struct list_head list;
	struct ath12k_dp_rx_tid data;
	unsigned long ts;
};

struct dp_reo_update_rx_queue_elem {
	struct list_head list;
	struct ath12k_dp_rx_tid data;
	int peer_id;
	u8 tid;
	bool reo_cmd_update_rx_queue_resend_flag;
	bool is_ml_peer;
	u16 ml_peer_id;
};

struct ath12k_dp_rx_reo_cmd {
	struct list_head list;
	union {
		struct ath12k_dp_rx_tid data;
		struct ath12k_dp_smd_ctx smd_data;
	} u;
	int cmd_num;
	void (*handler)(struct ath12k_dp *dp, void *ctx,
			struct hal_reo_status *status);
};

#define ATH12K_DP_RX_REO_DESC_FREE_THRES  64
#define ATH12K_DP_RX_REO_DESC_FREE_TIMEOUT_MS 1000

enum ath12k_dp_rx_decap_type {
	DP_RX_DECAP_TYPE_RAW,
	DP_RX_DECAP_TYPE_NATIVE_WIFI,
	DP_RX_DECAP_TYPE_ETHERNET2_DIX,
	DP_RX_DECAP_TYPE_8023,
	DP_RX_DECAP_TYPE_INVALID,
};

struct ath12k_dp_rx_rfc1042_hdr {
	u8 llc_dsap;
	u8 llc_ssap;
	u8 llc_ctrl;
	u8 snap_oui[3];
	__be16 snap_type;
} __packed;

struct dp_rx_fst {
	struct ath12k_base *ab;
	u8 *base;
	struct hal_rx_fst *hal_rx_fst;
	u16 num_entries;
	u16 ipv4_fse_rule_cnt;
	u16 ipv6_fse_rule_cnt;
	u16 flows_per_reo[4];
	u32 flow_add_fail;
	u32 flow_del_fail;
	/* spinlock to prevent concurrent table access */
	spinlock_t fst_lock;
	struct delayed_work cache_inval_full_work;
};

static inline u32 ath12k_he_gi_to_nl80211_he_gi(u8 sgi)
{
	u32 ret = 0;

	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		ret = NL80211_RATE_INFO_HE_GI_0_8;
		break;
	case RX_MSDU_START_SGI_1_6_US:
		ret = NL80211_RATE_INFO_HE_GI_1_6;
		break;
	case RX_MSDU_START_SGI_3_2_US:
		ret = NL80211_RATE_INFO_HE_GI_3_2;
		break;
	default:
		ret = NL80211_RATE_INFO_HE_GI_0_8;
		break;
	}

	return ret;
}

static inline u32 ath12k_eht_gi_to_nl80211_eht_gi(u8 sgi)
{
	u32 ret = 0;

	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		ret = NL80211_RATE_INFO_EHT_GI_0_8;
		break;
	case RX_MSDU_START_SGI_1_6_US:
		ret = NL80211_RATE_INFO_EHT_GI_1_6;
		break;
	case RX_MSDU_START_SGI_3_2_US:
		ret = NL80211_RATE_INFO_EHT_GI_3_2;
		break;
	}

	return ret;
}

static inline u32 ath12k_uhr_gi_to_nl80211_uhr_gi(u8 sgi)
{
	u32 ret = 0;

	switch (sgi) {
	case RX_MSDU_START_SGI_0_8_US:
		ret = NL80211_RATE_INFO_UHR_GI_0_8;
		break;
	case RX_MSDU_START_SGI_1_6_US:
		ret = NL80211_RATE_INFO_UHR_GI_1_6;
		break;
	case RX_MSDU_START_SGI_3_2_US:
		ret = NL80211_RATE_INFO_UHR_GI_3_2;
		break;
	}

	return ret;
}

static inline bool
is_ieee80211_frame_da_mcast(struct sk_buff *msdu)
{
	struct ieee80211_hdr *hdr;
	u16 fc;
	u8 *da;

	hdr = (struct ieee80211_hdr *)msdu->data;

	fc = le16_to_cpu(hdr->frame_control);

	if (ieee80211_has_tods(fc) && ieee80211_has_fromds(fc)) {
		/* WDS: DA = Address 3 */
		da = hdr->addr3;
	} else if (ieee80211_has_tods(fc)) {
		/* STA -> DS: DA = Address 3 */
		da = hdr->addr3;
	} else {
		/* DS -> STA or IBSS: DA = Address 1 */
		da = hdr->addr1;
	}

	return(is_multicast_ether_addr(da) ? 1 : 0);
}

static inline bool
ath12k_dp_err_drop_needed(enum hal_wbm_rel_src_module src,
			  int reo_push_reason, int reo_error_code,
			  int rxdma_push_reason, int rxdma_error_code)
{
	if (src == HAL_WBM_REL_SRC_MODULE_REO) {
		int err_rsn = HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED;
		int add_zero = HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO;
		int rout_inst = HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION;

		if (reo_push_reason == rout_inst ||
		    (reo_push_reason == err_rsn && reo_error_code == add_zero))
			return false;

		return true;
	}

	if (src == HAL_WBM_REL_SRC_MODULE_RXDMA &&
	    rxdma_push_reason == HAL_RXDMA_PUSH_REASON_ERR_DETECTED) {
		switch (rxdma_error_code) {
		case HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR:
		case HAL_REO_ENTR_RING_RXDMA_ECODE_MULTICAST_ECHO_ERR:
		case HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR:
		case HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR:
			return false;
		default:
			return true;
		}
	}

	return true;
}

static inline void
ath12k_dp_rx_wbm_err_dev_free_skb(struct ath12k_dp *dp,
				  struct sk_buff *msdu,
				  enum ath12k_wbm_err_drop_reason drop_reason)
{
	DP_DEVICE_STATS_INC(dp, wbm_err.drop[drop_reason], 1);
	if (msdu)
		dev_kfree_skb_any(msdu);
}

void
ath12k_dp_rx_update_eapol_stats(struct ath12k_dp *dp, struct sk_buff *msdu);
void
ath12k_dp_rx_update_vow_delay_stats(struct ath12k_pdev_dp *dp_pdev,
				    struct link_peer_rx_tid_stats *stats,
				    struct sk_buff *msdu,
				    bool da_is_mcbc, u8 tid,
				    struct ath12k_tid_rx_stats *tstats);
int ath12k_dp_get_rx_frame_type(u8 rx_decap_type);
bool ath12k_dp_rx_check_nwifi_hdr_len_valid(struct ath12k_dp *dp,
					    u8 decap_type,
					    struct sk_buff *msdu);
u32 ath12k_fill_reo_drop_reason(enum hal_reo_dest_ring_error_code err_code);
bool ath12k_dp_rx_h_mec_drop(struct ath12k_pdev_dp *dp_pdev,
			     struct ath12k_vif *ahvif,
			     int link_id, int peer_id);
int ath12k_dp_rx_ampdu_start(struct ath12k *ar,
			     struct ieee80211_ampdu_params *params,
			     u8 link_id);
int ath12k_dp_rx_ampdu_stop(struct ath12k *ar,
			    struct ieee80211_ampdu_params *params,
			    u8 link_id);
int ath12k_dp_rx_peer_pn_replay_config(struct ath12k_link_vif *arvif,
				       const u8 *peer_addr,
				       enum set_key_cmd key_cmd,
				       struct ieee80211_key_conf *key,
				       struct ieee80211_sta *sta,
				       enum ath12k_rxtid_pn_check cfg);
int ath12k_dp_rx_peer_epp_pn_replay_config(struct ath12k_dp_peer *dp_peer,
					   struct ath12k_link_vif *arvif,
					   const u8 *peer_addr,
					   enum set_key_cmd key_cmd,
					   enum ath12k_rxtid_pn_check cfg);
void ath12k_dp_rx_peer_tid_cleanup(struct ath12k *ar,
				   struct ath12k_dp_link_peer *peer);
int ath12k_dp_rx_reo_alloc(struct ath12k_base *ab);
int ath12k_dp_rx_reo_init(struct ath12k_base *ab);
int ath12k_dp_rx_reo_setup(struct ath12k_base *ab);
void ath12k_dp_rx_reo_cleanup(struct ath12k_base *ab);
int ath12k_dp_rx_alloc(struct ath12k_base *ab);
void ath12k_dp_rx_free(struct ath12k_base *ab);
void ath12k_dp_rx_reo_cmd_list_cleanup(struct ath12k_base *ab);
void ath12k_dp_rx_bufs_replenish(struct ath12k_dp *dp,
				 struct hal_srng *srng,
				 struct list_head *used_list,
				 bool reuse);
int ath12k_dp_rx_peer_frag_setup(struct ath12k *ar,
				 struct ath12k_dp_link_peer *peer,
				 struct crypto_shash *tfm);

struct ath12k_dp_link_peer *
ath12k_dp_rx_h_find_peer(struct ath12k_pdev_dp *dp_pdev,
			 struct hal_rx_desc *rx_desc, u16 peer_id);
u8 ath12k_dp_rx_h_decap_type(struct ath12k_base *ab,
			     struct hal_rx_desc *desc);
u32 ath12k_dp_rx_h_mpdu_err(struct ath12k_base *ab,
			    struct hal_rx_desc *desc);
void ath12k_dp_rx_process_reo_status(struct ath12k_base *ab);
u16 ath12k_dp_rx_h_seq_no(struct ath12k_base *ab, struct hal_rx_desc *desc);
void ath12k_dp_rx_deliver_msdu(struct ath12k_pdev_dp *dp_pdev,
			       struct napi_struct *napi,
			       struct sk_buff *msdu,
			       struct ieee80211_rx_status *status,
			       u8 hw_link_id, bool is_mcbc, u16 peer_id, u16 tid);
void ath12k_dp_reo_cmd_free(struct ath12k_dp *dp, void *ctx,
			    struct hal_reo_status *status);
void ath12k_dp_rx_frags_cleanup(struct ath12k_dp_rx_tid *rx_tid,
				bool rel_link_desc);
int ath12k_dp_rx_crypto_mic_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype);
int ath12k_dp_rx_crypto_param_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype);
int ath12k_dp_rx_crypto_icv_len(struct ath12k_dp *dp, enum hal_encrypt_type enctype);
void ath12k_dp_rx_h_undecap_frag(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				 enum hal_encrypt_type enctype, u32 flags);
int ath12k_dp_rx_h_michael_mic(struct crypto_shash *tfm, u8 *key,
			       struct ieee80211_hdr *hdr, u8 *data,
			       size_t data_len, u8 *mic);
void ath12k_dp_rx_h_undecap_raw(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *msdu,
				struct hal_rx_desc *rx_desc,
				enum hal_encrypt_type enctype,
				struct ieee80211_rx_status *status, bool decrypted,
				u16 peer_id);
int ath12k_dp_rx_flow_add_entry(struct ath12k_base *ab,
				struct rx_flow_info *flow_info);
int ath12k_dp_rx_flow_delete_entry(struct ath12k_base *ab,
				   struct rx_flow_info *flow_info);
int ath12k_dp_rx_flow_delete_all_entries(struct ath12k_base *ab);
struct dp_rx_fst *ath12k_dp_rx_fst_attach(struct ath12k_base *ab);
void ath12k_dp_rx_fst_detach(struct ath12k_base *ab, struct dp_rx_fst *fst);
void ath12k_dp_fst_core_map_init(struct ath12k_base *ab);
void ath12k_dp_rx_fst_init(struct ath12k_base *ab);
ssize_t ath12k_dp_dump_fst_table(struct ath12k_base *ab, char *buf, int size);
size_t ath12k_dp_list_cut_nodes(struct list_head *list,
				struct list_head *head, size_t count, uint8_t pool_type);
int ath12k_dp_rx_peer_tid_setup(struct ath12k *ar, struct ath12k_dp_peer *dp_peer,
				const u8 *peer_mac, int vdev_id,
				u8 tid, u32 ba_win_sz, u16 ssn,
				enum hal_pn_type pn_type);
void ath12k_dp_tid_cleanup(struct ath12k_base *ab);
void ath12k_dp_reset_rx_reo_tid_q(void *vaddr, u32 ba_window_size, u8 tid);
int
ath12k_dp_rx_htt_rxdma_rxole_ppe_cfg_set(struct ath12k_base *ab,
					 struct ath12k_dp_htt_rxdma_ppe_cfg_param *param);
void
ath12k_dp_primary_peer_migrate_setup(struct ath12k_dp *dp, void *ctx,
				     struct hal_reo_status *status);
int
ath12k_dp_peer_migrate(struct ath12k_sta *ahsta, u16 peer_id,
		       u8 chip_id, u8 pdev_id);
int ath12k_dp_rx_pkt_type_filter(struct ath12k *ar,
				 enum ath12k_routing_pkt_type pkt_type,
				 u32 meta_data);
int ath12k_dp_rxdma_buf_setup(struct ath12k_base *ab);
void ath12k_dp_rx_skb_free(struct sk_buff *skb, struct ath12k_dp *dp, int ring,
			   enum ath12k_dp_rx_error drop_reason,
			   struct ath12k_pdev_dp *dp_pdev, u8 tid);
void ath12k_dp_rx_classify_mscs(struct ath12k_base *ab,
				struct ath12k_dp_peer *peer,
				struct sk_buff *msdu, u8 tid);
void ath12k_dp_rx_peer_tid_ba_config(struct ath12k_dp *dp, u8 tid, u32 *ba_win_size,
				     u16 *ssn);
bool ath12k_dp_rx_peer_tid_skip_pn_replay(struct ath12k_dp *dp, u8 tid);
int ath12k_dp_rxdma_ring_sel_config(struct ath12k_base *ab);
u16 ath12k_wifi7_dp_rx_get_peer_id(struct ath12k_base *ab,
				   enum ath12k_peer_metadata_version ver,
				   __le32 peer_metadata);
void ath12k_dp_rx_frag_timer(struct timer_list *timer);
void ath12k_dp_rx_update_stats(struct ath12k_pdev_dp *pdev,
			       struct ath12k_dp_peer *peer,
			       struct link_peer_rx_tid_stats *stats,
			       int ring_id, u8 hw_link_id,
			       u8 active_tid_mask);
void
ath12k_dp_rx_update_delay_stats(struct ath12k_dp_peer *peer, struct sk_buff *msdu,
				u8 tid, u8 ring);
void ath12k_dp_tid_wbm_err_stats(struct ath12k_pdev_dp *dp_pdev,
				 u8 tid,
				 bool is_reo,
				 u32 error_code);
/**
 * struct dp_rx_tag_params - chip-independent CCE/FSE classification results
 *
 * Populated by wifi7/wifi8 callers from their chip-specific scratchpad or HAL
 * ops before calling dp_rx_update_protocol_tag() / dp_rx_update_flow_tag().
 */
struct dp_rx_tag_params {
	bool    cce_match;
	u16     cce_metadata;
	bool    flow_idx_invalid;
	bool    flow_idx_timeout;
	u16     flow_metadata;
};

void dp_rx_update_protocol_tag(struct ath12k_pdev_dp *dp_pdev,
			       struct sk_buff *msdu,
			       const struct dp_rx_tag_params *tag);

void dp_rx_update_flow_tag(struct ath12k_pdev_dp *dp_pdev,
			   struct sk_buff *msdu,
			   const struct dp_rx_tag_params *tag);
#endif /* ATH12K_DP_RX_H */
