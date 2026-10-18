/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_DP_TX_MON_H
#define ATH12K_DP_TX_MON_H

#define ATH12K_DP_MON_TX_MAX_RADIO_TAP_HDR 512

#define INITIATOR_WINDOW 0
#define RESPONSE_WINDOW  1

#define ATH12K_DP_MON_TX_BA_PER_STA_TID_INF_SZ     2
#define ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ       2
#define ATH12K_DP_MON_TX_BA_CTRL_SZ                2
#define ATH12K_DP_MON_TX_BA_BITMAP_BASE_SZ         4
#define ATH12K_DP_MON_TX_BA_BITMAP_SZ_MAX          7

#define ATH12K_BA_DURATION_US  32

#define ATH12K_DP_MON_TX_SPC_PKT_ID_MAX	7

#define ATH12K_DP_MON_TX_BA_BITMAP_BYTES(sz) \
	(ATH12K_DP_MON_TX_BA_BITMAP_BASE_SZ << (sz))

#define ATH12K_MU_BA_CTRL_MULTI_TID  0x0016

#define ATH12K_DP_MON_TX_MU_BA_INFO_SZ(bitmap_sz)  \
	((ATH12K_DP_MON_TX_BA_START_SQ_CTRL_SZ) +\
	 (ATH12K_DP_MON_TX_BA_PER_STA_TID_INF_SZ) +\
	 (4 << (bitmap_sz)))

#define ATH12K_FREQ_2GHZ_MIN               2412
#define ATH12K_FREQ_2GHZ_MAX               2484
#define ATH12K_FREQ_5GHZ_MIN               5150
#define ATH12K_FREQ_5GHZ_MAX               5895
#define ATH12K_FREQ_6GHZ_MIN               5925
#define ATH12K_FREQ_6GHZ_MAX               7125

/* Frame timing constants */
#define ATH12K_ACK_TX_TIME_US       44
#define ATH12K_CTS_TX_TIME_US       44
#define ATH12K_SIFS_2GHZ_US         10
#define ATH12K_SIFS_5GHZ_US         16
#define ATH12K_DEFAULT_NOISE_FLOOR  -95

/* Rate Table */
#define ATH12K_11B_RATE_0MCS (11 * 2)
#define ATH12K_11B_RATE_1MCS (5.5 * 2)
#define ATH12K_11B_RATE_2MCS (2 * 2)
#define ATH12K_11B_RATE_3MCS (1 * 2)
#define ATH12K_11B_RATE_4MCS ATH12K_11B_RATE_0MCS
#define ATH12K_11B_RATE_5MCS ATH12K_11B_RATE_1MCS
#define ATH12K_11B_RATE_6MCS ATH12K_11B_RATE_2MCS

#define ATH12K_11A_RATE_0MCS (48 * 2)
#define ATH12K_11A_RATE_1MCS (24 * 2)
#define ATH12K_11A_RATE_2MCS (12 * 2)
#define ATH12K_11A_RATE_3MCS (6 * 2)
#define ATH12K_11A_RATE_4MCS (54 * 2)
#define ATH12K_11A_RATE_5MCS (36 * 2)
#define ATH12K_11A_RATE_6MCS (18 * 2)
#define ATH12K_11A_RATE_7MCS (9 * 2)

#define ATH12K_LEGACY_MCS0 0
#define ATH12K_LEGACY_MCS1 1
#define ATH12K_LEGACY_MCS2 2
#define ATH12K_LEGACY_MCS3 3
#define ATH12K_LEGACY_MCS4 4
#define ATH12K_LEGACY_MCS5 5
#define ATH12K_LEGACY_MCS6 6
#define ATH12K_LEGACY_MCS7 7

/* Legacy rate values in 500kbps units - for radiotap */
#define ATH12K_RATE_1MBPS_500KBPS	2
#define ATH12K_RATE_2MBPS_500KBPS	4
#define ATH12K_RATE_5_5MBPS_500KBPS	11
#define ATH12K_RATE_6MBPS_500KBPS	12
#define ATH12K_RATE_9MBPS_500KBPS	18
#define ATH12K_RATE_11MBPS_500KBPS	22
#define ATH12K_RATE_12MBPS_500KBPS	24
#define ATH12K_RATE_18MBPS_500KBPS	36
#define ATH12K_RATE_24MBPS_500KBPS	48
#define ATH12K_RATE_36MBPS_500KBPS	72
#define ATH12K_RATE_48MBPS_500KBPS	96
#define ATH12K_RATE_54MBPS_500KBPS	108

/* L-SIG field masks for extracting rate and length */
#define ATH12K_LSIG_RATE_MASK              GENMASK(3, 0)
#define ATH12K_LSIG_LENGTH_MASK            GENMASK(16, 5)
#define ATH12K_RADIOTAP_LSIG_LENGTH_SHIFT  4

/* Legacy rate values in 100kbps units - for radiotap */
#define ATH12K_LEGACY_RATE_1MBPS_100KBPS   10
#define ATH12K_LEGACY_RATE_2MBPS_100KBPS   20
#define ATH12K_LEGACY_RATE_5_5MBPS_100KBPS 55
#define ATH12K_LEGACY_RATE_6MBPS_100KBPS   60
#define ATH12K_LEGACY_RATE_9MBPS_100KBPS   90
#define ATH12K_LEGACY_RATE_11MBPS_100KBPS  110
#define ATH12K_LEGACY_RATE_12MBPS_100KBPS  120
#define ATH12K_LEGACY_RATE_18MBPS_100KBPS  180
#define ATH12K_LEGACY_RATE_24MBPS_100KBPS  240
#define ATH12K_LEGACY_RATE_36MBPS_100KBPS  360
#define ATH12K_LEGACY_RATE_48MBPS_100KBPS  480
#define ATH12K_LEGACY_RATE_54MBPS_100KBPS  540
#define ATH12K_LEGACY_RATE_DEFAULT_100KBPS 60

/* Rate status configuration */
#define ATH12K_RATE_STATUS_N_RATES         1
#define ATH12K_RATE_STATUS_TRY_COUNT       1
#define ATH12K_RATE_STATUS_DEFAULT_BW      0
#define ATH12K_RATE_STATUS_DEFAULT_NSS     1
#define ATH12K_RATE_STATUS_DEFAULT_IDX	  -1

#define _ATH12K_STAT_INC(dp_pdev, field) \
	do { \
		if (likely((dp_pdev) && (dp_pdev)->dp_mon_pdev && \
			(dp_pdev)->dp_mon_pdev->dp_pdev_tx_mon)) \
			(dp_pdev)->dp_mon_pdev->dp_pdev_tx_mon-> \
				pdev_tx_mon_stats.field++; \
	} while (0)

#define ATH12K_TX_MON_STAT_INC(dp_pdev, field)  _ATH12K_STAT_INC(dp_pdev, field)

#define ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, field) _ATH12K_STAT_INC(dp_pdev, field)

#define ATH12K_BA_USER_ID_INVAL            255

enum dp_mon_tx_filter_mode;
struct dp_mon_tx_filter;
struct ath12k_ext_mon_config;
struct ath12k_ext_mon_filter_config;
struct ath12k_ext_mon_peer_config;
struct ath12k_ext_mon_pkt_config;

/**
 * struct ath12k_pdev_tx_mon_stats - per-pdev TX monitor ring stats counters
 * @num_bufs_reaped: total ring entries consumed from the TX monitor dst ring
 * @truncated_buf: PPDUs dropped because hardware reported HAL_MON_PPDU_TRUNCATED
 * @flushed_buf: PPDUs dropped because hardware reported HAL_MON_FLUSH_DETECTED
 * @null_buf: ring entries skipped due to NULL DMA buffer pointer
 * @mon_desc_free: descriptors returned to the free pool after end-of-PPDU delivery
 * @pkt_buf_null: packet TLV processing aborted due to NULL buffer
 * @status_buf_null: status TLV processing aborted due to NULL buffer
 * @prep_wq_failed: failures adding a completed PPDU descriptor to the work queue list
 * @empty_descriptor: Empty descriptors provided by hardware
 * @ppdu_processed: PPDUs fully processed through the work queue handler
 * @status_desc_processed: individual status descriptors parsed inside a PPDU
 * @pkt_buf_processed: packet data buffers successfully DMA-unmapped and attached
 *			to an MPDU skb fragment during PPDU reconstruction
 * @ppdu_desc_overflow: PPDUs dropped because status descriptor count exceeded the
 *						pool limit
 * @zero_status_desc: PPDUs skipped because the descriptor list was empty at
 *						work queue time
 * @ppdu_prep_failed: failures building the consolidated ppdu_desc from the
 *						descriptor list
 * @tlv_process_failed: failures in processing info from status TLVs
 * @data_gen_failed: failures attaching a payload buffer as an skb fragment
 * @buf_extract_failed: failures extracting buffer address info from a buffer-address TLV
 * @magic_value_error: descriptors rejected due to magic-value mismatch
 * @pkt_buf_free: packet TLV buffers released back to the page-fragment allocator
 * @status_buf_free: status buffers released back to the page-fragment allocator
 * @mu_user_frame: MU frames generated for individual users within a MU-MIMO PPDU
 * @data_ppdu_delivered: Data PPDUs successfully delivered
 * @prot_ppdu_delivered: Protection PPDUs successfully delivered
 * @total_frames_delivered: Frames successfully delivered
 * @self_gen_failed: failures generating self-generated response frames (ACK/CTS/BA)
 * @skb_alloc_failed: failures allocating skb for a new MPDU during PPDU reconstruction
 * @ring_extract_failed: failures reading the next entry from the TX mon destination ring
 * @get_num_users_failed: failures parsing the number of users from the PPDU start TLV
 * @frames_drop_in_sw: frames dropped by host in ext_mon filtering
 * @custom_call_back_delivered: frames delivered via notifier chain
 * @wmi_peer_send_failed: wmi peer command send failed
 */
struct ath12k_pdev_tx_mon_stats {
	/* Tasklet related stats */
	u32 num_bufs_reaped;
	u32 truncated_buf;
	u32 flushed_buf;
	u32 null_buf;
	u32 mon_desc_free;
	u32 pkt_buf_null;
	u32 status_buf_null;
	u32 prep_wq_failed;
	u32 empty_descriptor;

	/* ppdu descriptor stats */
	u32 ppdu_processed;
	u32 status_desc_processed;
	u32 pkt_buf_processed;
	u32 ppdu_desc_overflow;
	u32 zero_status_desc;
	u32 ppdu_prep_failed;
	u32 tlv_process_failed;
	u32 data_gen_failed;
	u32 buf_extract_failed;
	u32 magic_value_error;
	u32 pkt_buf_free;
	u32 status_buf_free;
	u32 mu_user_frame;

	/* Delivery statistics */
	u32 prot_ppdu_delivered;
	u32 data_ppdu_delivered;
	u32 total_frames_delivered;

	/* frame generation failures */
	u32 self_gen_failed;
	u32 skb_alloc_failed;

	/* HAL related statistics */
	u32 ring_extract_failed;
	u32 get_num_users_failed;
	/* bitmap: 0=unknown 1=ARP 2=EAPOL 3=DHCP 4=DNS 5=ICMP 6=MGMT */
	u32 spl_pkt_cap_stats[ATH12K_DP_MON_TX_SPC_PKT_ID_MAX];

	/* Extended Monitor mode */
	u32 frames_drop_in_sw;
	u32 wmi_peer_send_failed;
	u32 custom_call_back_delivered;
};

/**
 * struct ath12k_dp_tx_mon_stats - device-level TX monitor buffer lifecycle counters
 * @buf_replenished: buffers successfully written into the TX monitor refill ring
 * @alloc_fail: page_frag_alloc() failures during buffer replenishment
 * @dma_fail: DMA mapping failures during buffer replenishment
 * @with_hw: current snapshot of buffers owned by hardware
 * @in_reap: current snapshot of buffers being reaped by the host
 * @free: current snapshot of buffers in the free pool
 * @replenish_err: current snapshot of buffers stranded due to replenish failure
 * @proc_err: current snapshot of buffers stranded due to processing failure
 */
struct ath12k_dp_tx_mon_stats {
	/* monitor buffer replenishment statistics */
	u32 buf_replenished;
	u32 alloc_fail;
	u32 dma_fail;
	/* buffer ownership snapshot */
	u32 with_hw;
	u32 in_reap;
	u32 free;
	u32 replenish_err;
	u32 proc_err;
};

struct ieee80211_frame_min {
	__le16 frame_control;
	__le16 duration;
	u8 ra[ETH_ALEN];
} __packed __aligned(2);

struct ieee80211_ctl_frm {
	__le16 frame_control;
	__le16 duration;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
} __packed;

struct ieee80211_mu_block_ack_hdr {
	__le16 frame_control;
	__le16 duration;
	u8 ra[ETH_ALEN];
	u8 ta[ETH_ALEN];
	__le16 ba_control;
} __packed;

enum txmon_generated_response {
	TXMON_GEN_RESP_SELFGEN_ACK = 0,
	TXMON_GEN_RESP_SELFGEN_CTS,
	TXMON_GEN_RESP_SELFGEN_BA,
	TXMON_GEN_RESP_SELFGEN_MBA,
	TXMON_GEN_RESP_SELFGEN_CBF,
	TXMON_GEN_RESP_SELFGEN_TRIG,
	TXMON_GEN_RESP_SELFGEN_NDP_LMR
};

enum ath12k_dp_mon_tx_dma_length {
	ATH12K_DP_MON_TX_DMA_LENGTH_64B = BIT(0),
	ATH12K_DP_MON_TX_DMA_LENGTH_128B = BIT(1),
	ATH12K_DP_MON_TX_DMA_LENGTH_256B = BIT(2),
	ATH12K_DP_MON_TX_DMA_LENGTH_MAX = GENMASK(2, 0),
};

struct ath12k_dp_ext_mon_tx_peer_params {
	int vdev_id;
	struct list_head *peers_to_wmi;
	struct list_head *peer_list;
	u8 staged_count;
	u8 *ext_mon_peer_count;
	const struct ath12k_ext_mon_peer_config *peer_config;
	bool toggle_hw_state;
};

int ath12k_dp_mon_tx_process_ring(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				  struct napi_struct *napi, int *budget);

static inline int
ath12k_dp_tx_mon_process_ring(struct ath12k_dp *dp, int mac_id,
			      struct napi_struct *napi, int budget)
{
	struct ath12k_pdev_dp *dp_pdev;
	u8 pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);
	int num_buffs_reaped = 0;

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev))
		return 0;

	num_buffs_reaped =
		ath12k_dp_mon_tx_process_ring(dp_pdev, mac_id, napi, &budget);

	return num_buffs_reaped;
}

/* TX Monitor PPDU Processing */
void ath12k_dp_tx_mon_process_ppdu(struct work_struct *work);

int ath12k_dp_mon_tx_srng_alloc_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_srng_init_setup(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_dst_ring_alloc_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
void ath12k_dp_mon_tx_dst_ring_cleanup(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_tx_htt_srng_setup(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_htt_srng_cleanup(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev, bool enable);
int ath12k_dp_mon_tx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_tx_wq_start(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
void ath12k_dp_mon_tx_wq_stop(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_tx_desc_pool_alloc(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_desc_pool_free(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_buff_alloc(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_htt_dst_ring_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
int ath12k_dp_mon_tx_monitor_start_stop(struct ath12k *ar, bool state);
void ath12k_dp_mon_tx_filter_free(struct ath12k_pdev_dp *dp_pdev);
bool ath12k_dp_tx_mon_feature_eval(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_update_spl_pkt_cap_stats(struct ath12k_pdev_dp *dp_pdev,
					    struct hal_tx_mon_status_info *status_info);
void ath12k_dp_mon_spl_pkt_cap_config(struct ath12k_base *ab);
int ath12k_dp_mon_tx_srng_alloc(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_srng_init(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_htt_src_ring_cleanup(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_pdev_free(struct ath12k_pdev_dp *dp_pdev);
int ath12k_dp_mon_tx_htt_src_ring_setup(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_config_full_monitor(struct ath12k *ar, bool set);
int ath12k_dp_mon_tx_update_filter(struct ath12k *ar);
void ath12k_dp_mon_tx_srng_free(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_srng_deinit(struct ath12k_dp *dp);
int ath12k_dp_mon_tx_pdev_alloc(struct ath12k_pdev_dp *dp_pdev,
				u32 mac_id);
int ath12k_dp_mon_tx_set_monitor_flags(struct ath12k *ar, u32 new_flags, u32 *cur_flags);
void ath12k_dp_mon_tx_process_low_thres(struct ath12k_dp *dp);
void ath12k_dp_mon_tx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_tx_filter_mode mode,
				      struct dp_mon_tx_filter *filter);
void
ath12k_dp_mon_tx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_tx_ring_tlv_filter *src_tlv_filter);
void ath12k_dp_mon_tx_update_buf_ownership_stats(struct ath12k_dp *dp);
int ath12k_dp_ext_mon_tx_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_ext_mon_tx_free(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_tx_ssr_restart_pdev(struct ath12k_pdev_dp *dp_pdev);
int
ath12k_dp_ext_mon_get_tx_filter(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_ext_mon_config *resp);
int ath12k_dp_ext_mon_get_tx_peer(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_ext_mon_config *resp);
int ath12k_dp_ext_mon_set_tx_filter(struct ath12k_pdev_dp *dp_pdev,
				    const struct ath12k_ext_mon_filter_config
				    *new_config);
enum ath12k_dp_mon_tx_dma_length
ath12k_dp_mon_tx_get_ext_mon_filter_len(u8 filter_len);
int ath12k_dp_ext_mon_handle_tx_peer(struct ath12k_pdev_dp *dp_pdev,
				     const struct ath12k_ext_mon_peer_config
				     *peer_config);
void
ath12k_dp_tx_mon_set_tx_mon_teardown(struct ath12k *ar);
int
ath12k_dp_ext_mon_filter_peer(struct ieee80211_hdr *wh, struct list_head *peer_list);
int
ath12k_dp_ext_mon_filter_type(struct ieee80211_hdr *wh,
			      struct ath12k_ext_mon_pkt_config *pkt_config);
int
ath12k_dp_ext_mon_filter_subtype(struct ieee80211_hdr *wh,
				 struct ath12k_ext_mon_pkt_config *pkt_config);

#endif /* ATH12K_DP_TX_MON_H */
