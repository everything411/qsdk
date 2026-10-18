/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DEBUG_HTT_STATS_H
#define DEBUG_HTT_STATS_H
#include "dp_htt.h"

#define ATH12K_HTT_STATS_BUF_SIZE		(1024 * 512)
#define ATH12K_HTT_STATS_COOKIE_LSB		GENMASK_ULL(31, 0)
#define ATH12K_HTT_STATS_COOKIE_MSB		GENMASK_ULL(63, 32)
#define ATH12K_HTT_STATS_MAGIC_VALUE		0xF0F0F0F0
#define ATH12K_HTT_STATS_MAX_RETRIES		3
#define ATH12K_HTT_STATS_SUBTYPE_MAX		16
#define ATH12K_HTT_MAX_STRING_LEN		256
#define ATH12K_HTT_STATS_MGMT_FRM_TYPE_MAX	16


#define ATH12K_HTT_STATS_RESET_BITMAP32_OFFSET(_idx)	((_idx) & 0x1f)
#define ATH12K_HTT_STATS_RESET_BITMAP64_OFFSET(_idx)	((_idx) & 0x3f)
#define ATH12K_HTT_STATS_RESET_BITMAP32_BIT(_idx)	(1 << \
		ATH12K_HTT_STATS_RESET_BITMAP32_OFFSET(_idx))
#define ATH12K_HTT_STATS_RESET_BITMAP64_BIT(_idx)	(1 << \
		ATH12K_HTT_STATS_RESET_BITMAP64_OFFSET(_idx))

void ath12k_debugfs_htt_stats_register(struct ath12k *ar);

#ifdef CPTCFG_ATH12K_DEBUGFS
void ath12k_debugfs_htt_ext_stats_handler(struct ath12k_base *ab,
					  struct sk_buff *skb);
int ath12k_debugfs_htt_stats_req(struct ath12k *ar);
#else /* CPTCFG_ATH12K_DEBUGFS */
static inline void ath12k_debugfs_htt_ext_stats_handler(struct ath12k_base *ab,
							struct sk_buff *skb)
{
}
static inline int ath12k_debugfs_htt_stats_req(struct ath12k *ar)
{
	return 0;
}
#endif

/**
 * DOC: target -> host extended statistics upload
 *
 * The following field definitions describe the format of the HTT
 * target to host stats upload confirmation message.
 * The message contains a cookie echoed from the HTT host->target stats
 * upload request, which identifies which request the confirmation is
 * for, and a single stats can span over multiple HTT stats indication
 * due to the HTT message size limitation so every HTT ext stats
 * indication will have tag-length-value stats information elements.
 * The tag-length header for each HTT stats IND message also includes a
 * status field, to indicate whether the request for the stat type in
 * question was fully met, partially met, unable to be met, or invalid
 * (if the stat type in question is disabled in the target).
 * A Done bit 1's indicate the end of the of stats info elements.
 *
 *
 * |31                         16|15    12|11|10 8|7   5|4       0|
 * |--------------------------------------------------------------|
 * |                   reserved                   |    msg type   |
 * |--------------------------------------------------------------|
 * |                         cookie LSBs                          |
 * |--------------------------------------------------------------|
 * |                         cookie MSBs                          |
 * |--------------------------------------------------------------|
 * |      stats entry length     | rsvd   | D|  S |   stat type   |
 * |--------------------------------------------------------------|
 * |                   type-specific stats info                   |
 * |                      (see debugfs_htt_stats.h)               |
 * |--------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: Identifies this is a extended statistics upload confirmation
 *             message.
 *    Value: 0x1c
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 *
 * Stats Information Element tag-length header fields:
 *  - STAT_TYPE
 *    Bits 7:0
 *    Purpose: identifies the type of statistics info held in the
 *        following information element
 *    Value: ath12k_dbg_htt_ext_stats_type
 *  - STATUS
 *    Bits 10:8
 *    Purpose: indicate whether the requested stats are present
 *    Value:
 *       0 -> The requested stats have been delivered in full
 *       1 -> The requested stats have been delivered in part
 *       2 -> The requested stats could not be delivered (error case)
 *       3 -> The requested stat type is either not recognized (invalid)
 *  - DONE
 *    Bits 11
 *    Purpose:
 *        Indicates the completion of the stats entry, this will be the last
 *        stats conf HTT segment for the requested stats type.
 *    Value:
 *        0 -> the stats retrieval is ongoing
 *        1 -> the stats retrieval is complete
 *  - LENGTH
 *    Bits 31:16
 *    Purpose: indicate the stats information size
 *    Value: This field specifies the number of bytes of stats information
 *       that follows the element tag-length header.
 *       It is expected but not required that this length is a multiple of
 *       4 bytes.
 */

#define ATH12K_HTT_T2H_EXT_STATS_INFO1_DONE		BIT(11)
#define ATH12K_HTT_T2H_EXT_STATS_INFO1_LENGTH		GENMASK(31, 16)

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_pdev_stats_flush_tlv_v {
	u32 flush_errs[0]; /* HTT_TX_PDEV_MAX_FLUSH_REASON_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_pdev_stats_urrn_tlv_v {
	u32 urrn_stats[0]; /* HTT_TX_PDEV_MAX_URRN_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_pdev_stats_sifs_tlv_v {
	u32 sifs_status[0]; /* HTT_TX_PDEV_MAX_SIFS_BURST_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_pdev_stats_phy_err_tlv_v {
	u32  phy_errs[0]; /* HTT_TX_PDEV_MAX_PHY_ERR_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_pdev_stats_sifs_hist_tlv_v {
	u32 sifs_hist_status[0]; /* HTT_TX_PDEV_SIFS_BURST_HIST_STATS */
};

#define HTT_MAX_COUNTER_NAME 8
struct htt_counter_tlv {
	__le32 counter_name[HTT_MAX_COUNTER_NAME];
	u32 count;
};

struct htt_peer_stats_cmn_tlv {
	u32 ppdu_cnt;
	u32 mpdu_cnt;
	u32 msdu_cnt;
	u32 pause_bitmap;
	u32 block_bitmap;
	u32 current_timestamp;
	u32 peer_tx_airtime;
	u32 peer_rx_airtime;
	s32 rssi;
	u32 peer_enqueued_count_low;
	u32 peer_enqueued_count_high;
	u32 peer_dequeued_count_low;
	u32 peer_dequeued_count_high;
	u32 peer_dropped_count_low;
	u32 peer_dropped_count_high;
	u32 ppdu_transmitted_bytes_low;
	u32 ppdu_transmitted_bytes_high;
	u32 peer_ttl_removed_count;
	u32 inactive_time;
};

#define ATH12K_HTT_PEER_DETAILS_VDEV_ID	GENMASK(7, 0)
#define ATH12K_HTT_PEER_DETAILS_PDEV_ID	GENMASK(15, 8)
#define ATH12K_HTT_PEER_DETAILS_AST_IDX	GENMASK(31, 16)

#define ATH12K_HTT_PEER_DETAILS_ML_PEER_OFFSET_DWORD 8
#define ATH12K_HTT_PEER_DETAILS_ML_PEER_ID_VALID   BIT(0)
#define ATH12K_HTT_PEER_DETAILS_ML_PEER_ID         GENMASK(12, 1)
#define ATH12K_HTT_PEER_DETAILS_LINK_IDX           GENMASK(20, 13)
#define ATH12K_HTT_PEER_DETAILS_USE_PPE            BIT(21)
#define ATH12K_HTT_PEER_DETAILS_SRC_INFO           GENMASK(11, 0)
#define ATH12K_HTT_PEER_PS_DETAILS_PEER_PS_ENTRY   GENMASK(7, 0)
#define ATH12K_HTT_PEER_PS_DETAILS_PEER_PS_EXIT    GENMASK(15, 8)
#define ATH12K_HTT_PEER_PS_DETAILS_PEER_PSPOLL     GENMASK(23, 16)
#define ATH12K_HTT_PEER_PS_DETAILS_PEER_UAPSD      GENMASK(31, 24)
#define ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST0    GENMASK(9, 0)
#define ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST1    GENMASK(19, 10)
#define ATH12K_HTT_PEER_HIST_DETAILS_PEER_HIST2    GENMASK(29, 20)

/* Max seq ctrl can be active in txq at a given instant */
#define ATH12K_HTT_PDEV_STATS_MAX_SEQ_CTRL_HIST	4
/* For BE max active seq_ctrl that can be in HWQ */
#define ATH12K_HTT_PDEV_STATS_MAX_ACTIVE_SEQ_IN_HWQ_HIST 2

#define ATH12K_HTT_STATS_HWMLO_MAX_LINKS 6
#define ATH12K_HTT_STATS_MLO_MAX_IPC_RINGS 7

struct htt_peer_details_tlv {
	__le32 peer_type;
	__le32 sw_peer_id;
	__le32 vdev_pdev_ast_idx;
	struct htt_mac_addr mac_addr;
	__le32 peer_flags;
	__le32 qpeer_flags;
	__le32 src_info;
	__le32 peer_details;
	__le32 peer_ps_details;
	__le32 peer_hist_details;
};

/* NOTE: Variable length TLV, use length spec to infer array size .
 *
 *  Tried_mpdu_cnt_hist is the histogram of MPDUs tries per HWQ.
 *  The tries here is the count of the  MPDUS within a PPDU that the
 *  HW had attempted to transmit on  air, for the HWSCH Schedule
 *  command submitted by FW.It is not the retry attempts.
 *  The histogram bins are  0-29, 30-59, 60-89 and so on. The are
 *   10 bins in this histogram. They are defined in FW using the
 *  following macros
 *  #define WAL_MAX_TRIED_MPDU_CNT_HISTOGRAM 9
 *  #define WAL_TRIED_MPDU_CNT_HISTOGRAM_INTERVAL 30
 */
struct htt_tx_pdev_stats_tried_mpdu_cnt_hist_tlv_v {
	u32 hist_bin_size;
	u32 tried_mpdu_cnt_hist[]; /* HTT_TX_PDEV_TRIED_MPDU_CNT_HIST */
};

struct htt_tx_pdev_stats_tx_ppdu_stats_tlv_v {
	u32 num_data_ppdus_legacy_su;
	u32 num_data_ppdus_ac_su;
	u32 num_data_ppdus_ax_su;
	u32 num_data_ppdus_ac_su_txbf;
	u32 num_data_ppdus_ax_su_txbf;
};

struct ath12k_htt_extd_stats_msg {
	__le32 info0;
	__le64 cookie;
	__le32 info1;
	u8 data[];
} __packed;

struct htt_ast_entry_tlv {
	u32 sw_peer_id;
	u32 ast_index;
	u8 pdev_id;
	u8 vdev_id;
	u8 next_hop;
	u8 mcast;
	u8 monitor_direct;
	u8 mesh_sta;
	u8 mec;
	u8 intra_bss;
	u32 reserved;
	struct htt_mac_addr mac_addr;
};

enum htt_stats_param_type {
	HTT_STATS_PREAM_OFDM,
	HTT_STATS_PREAM_CCK,
	HTT_STATS_PREAM_HT,
	HTT_STATS_PREAM_VHT,
	HTT_STATS_PREAM_HE,
	HTT_STATS_PREAM_EHT,
	HTT_STATS_PREAM_UHR,

	HTT_STATS_PREAM_COUNT,
};

enum htt_stats_direction {
	HTT_STATS_DIRECTION_TX,
	HTT_STATS_DIRECTION_RX,
};

enum htt_stats_ppdu_type {
	HTT_STATS_PPDU_TYPE_MODE_SU,
	HTT_STATS_PPDU_TYPE_DL_MU_MIMO,
	HTT_STATS_PPDU_TYPE_UL_MU_MIMO,
	HTT_STATS_PPDU_TYPE_DL_MU_OFDMA,
	HTT_STATS_PPDU_TYPE_UL_MU_OFDMA,
};

#define HTT_TX_PEER_STATS_NUM_MCS_COUNTERS        12
#define HTT_TX_PEER_STATS_NUM_GI_COUNTERS          4
#define HTT_TX_PEER_STATS_NUM_DCM_COUNTERS         5
#define HTT_TX_PEER_STATS_NUM_BW_COUNTERS          4
#define HTT_TX_PEER_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_TX_PEER_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT
#define HTT_RX_PEER_STATS_NUM_BW_EXT_COUNTERS      4
#define HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS   2
#define HTT_TX_PEER_STATS_NUM_REDUCED_CHAN_TYPES   2
#define HTT_TX_PEER_STATS_NUM_EXTRA2_MCS_COUNTERS  2

struct htt_tx_peer_rate_stats_tlv {
	u32 tx_ldpc;
	u32 rts_cnt;
	u32 ack_rssi;

	u32 tx_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_su_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_mu_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	/* element 0,1, ...7 -> NSS 1,2, ...8 */
	u32 tx_nss[HTT_TX_PEER_STATS_NUM_SPATIAL_STREAMS];
	/* element 0: 20 MHz, 1: 40 MHz, 2: 80 MHz, 3: 160 and 80+80 MHz */
	u32 tx_bw[HTT_TX_PEER_STATS_NUM_BW_COUNTERS];
	u32 tx_stbc[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_pream[HTT_TX_PEER_STATS_NUM_PREAMBLE_TYPES];

	/* Counters to track number of tx packets in each GI
	 * (400us, 800us, 1600us & 3200us) in each mcs (0-11)
	 */
	u32 tx_gi[HTT_TX_PEER_STATS_NUM_GI_COUNTERS][HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];

	/* Counters to track packets in dcm mcs (MCS 0, 1, 3, 4) */
	u32 tx_dcm[HTT_TX_PEER_STATS_NUM_DCM_COUNTERS];
	u32 tx_mcs_ext[HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 tx_su_mcs_ext[HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 tx_mu_mcs_ext[HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 tx_stbc_ext[HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 tx_gi_ext[HTT_TX_PEER_STATS_NUM_GI_COUNTERS]
		     [HTT_TX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 reduced_tx_bw[HTT_TX_PEER_STATS_NUM_REDUCED_CHAN_TYPES]
			 [HTT_TX_PEER_STATS_NUM_BW_COUNTERS];
	u32 tx_bw_320mhz;
	u32 tx_mcs_ext_2[HTT_TX_PEER_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 tx_ppdu_cnt;
	__le32 tx_mpdu_try_cnt;
	__le32 tx_mpdu_success_cnt;
};

#define HTT_RX_PEER_STATS_NUM_MCS_COUNTERS        12
#define HTT_RX_PEER_STATS_NUM_GI_COUNTERS          4
#define HTT_RX_PEER_STATS_NUM_DCM_COUNTERS         5
#define HTT_RX_PEER_STATS_NUM_BW_COUNTERS          4
#define HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_RX_PEER_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT
#define HTT_RX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS   2
#define HTT_RX_PEER_STATS_NUM_REDUCED_CHAN_TYPES   2
#define HTT_RX_PEER_STATS_NUM_EXTRA2_MCS_COUNTERS  2

struct htt_rx_peer_rate_stats_tlv {
	u32 nsts;

	/* Number of rx ldpc packets */
	u32 rx_ldpc;
	/* Number of rx rts packets */
	u32 rts_cnt;

	u32 rssi_mgmt; /* units = dB above noise floor */
	u32 rssi_data; /* units = dB above noise floor */
	u32 rssi_comb; /* units = dB above noise floor */
	u32 rx_mcs[HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
	/* element 0,1, ...7 -> NSS 1,2, ...8 */
	u32 rx_nss[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS];
	u32 rx_dcm[HTT_RX_PEER_STATS_NUM_DCM_COUNTERS];
	u32 rx_stbc[HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
	/* element 0: 20 MHz, 1: 40 MHz, 2: 80 MHz, 3: 160 and 80+80 MHz */
	u32 rx_bw[HTT_RX_PEER_STATS_NUM_BW_COUNTERS];
	u32 rx_pream[HTT_RX_PEER_STATS_NUM_PREAMBLE_TYPES];
	/* units = dB above noise floor */
	u8 rssi_chain[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS]
		     [HTT_RX_PEER_STATS_NUM_BW_COUNTERS];

	/* Counters to track number of rx packets in each GI in each mcs (0-11) */
	u32 rx_gi[HTT_RX_PEER_STATS_NUM_GI_COUNTERS]
		 [HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 rx_ulofdma_non_data_ppdu;
	u32 rx_ulofdma_data_ppdu;
	u32 rx_ulofdma_mpdu_ok;
	u32 rx_ulofdma_mpdu_fail;
	s8  rx_ul_fd_rssi[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS];

	u32 per_chain_rssi_pkt_type;
	s8  rx_per_chain_rssi_in_dbm[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS]
				    [HTT_RX_PEER_STATS_NUM_BW_COUNTERS];
	u32 rx_ulmumimo_non_data_ppdu;
	u32 rx_ulmumimo_data_ppdu;
	u32 rx_ulmumimo_mpdu_ok;
	u32 rx_ulmumimo_mpdu_fail;
	u8  rssi_chain_ext[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS]
			  [HTT_RX_PEER_STATS_NUM_BW_EXT_COUNTERS];

	/* Stats for MCS 12/13 */
	u32 rx_mcs_ext[HTT_RX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 rx_stbc_ext[HTT_RX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 rx_gi_ext[HTT_RX_PEER_STATS_NUM_GI_COUNTERS]
		     [HTT_RX_PEER_STATS_NUM_EXTRA_MCS_COUNTERS];
	u32 reduced_rx_bw[HTT_RX_PEER_STATS_NUM_REDUCED_CHAN_TYPES]
			 [HTT_RX_PEER_STATS_NUM_BW_COUNTERS];
	s8 rx_per_chain_rssi_in_dbm_ext[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS]
				       [HTT_RX_PEER_STATS_NUM_BW_EXT_COUNTERS];
	u32 rx_bw_320mhz;
	u32 rx_mcs_ext_2[HTT_RX_PEER_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 tot_rx_ppdu_bytes;
	__le32 rx_mpdu_try_cnt;
};

enum htt_peer_stats_req_mode {
	HTT_PEER_STATS_REQ_MODE_NO_QUERY,
	HTT_PEER_STATS_REQ_MODE_QUERY_TQM,
	HTT_PEER_STATS_REQ_MODE_FLUSH_TQM,
};

/* htt_dbg_ext_stats_type */
enum ath12k_dbg_htt_ext_stats_type {
	ATH12K_DBG_HTT_EXT_STATS_RESET				= 0,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX			= 1,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_HWQ			= 3,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_SCHED			= 4,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_ERROR			= 5,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TQM			= 6,
	ATH12K_DBG_HTT_EXT_STATS_TQM_CMDQ			= 7,
	ATH12K_DBG_HTT_EXT_STATS_TX_DE_INFO			= 8,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_RATE			= 9,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_RX_RATE			= 10,
	ATH12K_DBG_HTT_EXT_STATS_PEER_INFO			= 11,
	ATH12K_DBG_HTT_EXT_STATS_TX_SELFGEN_INFO		= 12,
	ATH12K_DBG_HTT_EXT_STATS_TX_MU_HWQ			= 13,
	ATH12K_DBG_HTT_EXT_STATS_RING_IF_INFO			= 14,
	ATH12K_DBG_HTT_EXT_STATS_SRNG_INFO			= 15,
	ATH12K_DBG_HTT_EXT_STATS_SFM_INFO			= 16,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_MU			= 17,
	ATH12K_DBG_HTT_EXT_STATS_ACTIVE_PEERS_LIST		= 18,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_CCA_STATS			= 19,
	ATH12K_DBG_HTT_EXT_STATS_TX_SOUNDING_INFO		= 22,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_OBSS_PD_STATS		= 23,
	ATH12K_DBG_HTT_EXT_STATS_LATENCY_PROF_STATS		= 25,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_UL_TRIG_STATS		= 26,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_UL_MUMIMO_TRIG_STATS	= 27,
	ATH12K_DBG_HTT_EXT_STATS_FSE_RX				= 28,
	ATH12K_DBG_HTT_EXT_PEER_CTRL_PATH_TXRX_STATS		= 29,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_RX_RATE_EXT		= 30,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_TX_RATE_TXBF		= 31,
	ATH12K_DBG_HTT_EXT_STATS_TXBF_OFDMA			= 32,
	ATH12K_DBG_HTT_EXT_STA_11AX_UL_STATS			= 33,
	ATH12K_DBG_HTT_EXT_VDEV_RTT_RESP_STATS			= 34,
	ATH12K_DBG_HTT_EXT_PKTLOG_AND_HTT_RING_STATS		= 35,
	ATH12K_DBG_HTT_EXT_STATS_DLPAGER_STATS			= 36,
	ATH12K_DBG_HTT_EXT_PHY_COUNTERS_AND_PHY_STATS		= 37,
	ATH12K_DBG_HTT_EXT_VDEVS_TXRX_STATS			= 38,
	ATH12K_DBG_HTT_EXT_VDEV_RTT_INITIATOR_STATS		= 39,
	ATH12K_DBG_HTT_EXT_PDEV_PER_STATS			= 40,
	ATH12K_DBG_HTT_EXT_AST_ENTRIES				= 41,
	ATH12K_DBG_HTT_EXT_RX_RING_STATS			= 42,
	ATH12K_DBG_HTT_STRM_GEN_MPDUS_STATS			= 43,
	ATH12K_DBG_HTT_STRM_GEN_MPDUS_DETAILS_STATS		= 44,
	ATH12K_DBG_HTT_EXT_STATS_SOC_ERROR			= 45,
	ATH12K_DBG_HTT_DBG_PDEV_PUNCTURE_STATS			= 46,
	ATH12K_DBG_HTT_DBG_EXT_STATS_ML_PEERS_INFO		= 47,
	ATH12K_DBG_HTT_DBG_ODD_MANDATORY_STATS			= 48,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_SCHED_ALGO		= 49,
	ATH12K_DBG_HTT_DBG_ODD_MANDATORY_MUMIMO_STATS		= 50,
	ATH12K_DBG_HTT_EXT_STATS_MANDATORY_MUOFDMA		= 51,
	ATH12K_DBG_HTT_DBG_EXT_PHY_PROF_CAL_STATS		= 52,
	ATH12K_DGB_HTT_DBG_EXT_STATS_PDEV_BW_MGR		= 53,
	ATH12K_DGB_HTT_EXT_STATS_PDEV_MBSSID_CTRL_FRAME		= 54,
	ATH12K_DBG_HTT_UMAC_RESET_SSR_STATS                     = 55,
	ATH12K_DBG_HTT_STATS_GTX_STATS				= 68,
	ATH12K_DBG_HTT_EXT_STATS_PDEV_UL_MUMIMO_ELIGIBLE        = 74,
	ATH12K_DBG_HTT_DBG_EXT_STATS_HDS_PROF 			= 76,
	ATH12K_DBG_HTT_DBG_EXT_STATS_OPTIONAL_CONFIGS		= 77,
	ATH12K_DBG_HTT_DBG_EXT_STATS_FTM			= 79,
	ATH12K_DBG_HTT_DBG_EXT_STATS_FTM_TPCCAL_EXT		= 80,
	ATH12K_DBG_HTT_DBG_EXT_STATS_ANI_HISTOGRAM		= 81,
	ATH12K_DBG_HTT_DBG_EXT_STATS_RESET_HISTORY		= 82,
	ATH12K_DBG_HTT_STATS_REGULATORY				= 83,
	ATH12K_DBG_HTT_DBG_EXT_STATS_DPD_STATS_EXT		= 85,
	ATH12K_DBG_HTT_DBG_EXT_STATS_PHY_DPD_TPC		= 87,

	/* keep this last */
	ATH12K_DBG_HTT_NUM_EXT_STATS,
};

enum ath12k_dbg_htt_tlv_tag {
	HTT_STATS_TX_PDEV_CMN_TAG			= 0,
	HTT_STATS_TX_PDEV_UNDERRUN_TAG			= 1,
	HTT_STATS_TX_PDEV_SIFS_TAG			= 2,
	HTT_STATS_TX_PDEV_FLUSH_TAG			= 3,
	HTT_STATS_TX_PDEV_PHY_ERR_TAG			= 4,
	HTT_STATS_STRING_TAG				= 5,
	HTT_STATS_TX_HWQ_CMN_TAG			= 6,
	HTT_STATS_TX_HWQ_DIFS_LATENCY_TAG		= 7,
	HTT_STATS_TX_HWQ_CMD_RESULT_TAG			= 8,
	HTT_STATS_TX_HWQ_CMD_STALL_TAG			= 9,
	HTT_STATS_TX_HWQ_FES_STATUS_TAG			= 10,
	HTT_STATS_TX_TQM_GEN_MPDU_TAG			= 11,
	HTT_STATS_TX_TQM_LIST_MPDU_TAG			= 12,
	HTT_STATS_TX_TQM_LIST_MPDU_CNT_TAG		= 13,
	HTT_STATS_TX_TQM_CMN_TAG			= 14,
	HTT_STATS_TX_TQM_PDEV_TAG			= 15,
	HTT_STATS_TX_TQM_CMDQ_STATUS_TAG		= 16,
	HTT_STATS_TX_DE_EAPOL_PACKETS_TAG		= 17,
	HTT_STATS_TX_DE_CLASSIFY_FAILED_TAG		= 18,
	HTT_STATS_TX_DE_CLASSIFY_STATS_TAG		= 19,
	HTT_STATS_TX_DE_CLASSIFY_STATUS_TAG		= 20,
	HTT_STATS_TX_DE_ENQUEUE_PACKETS_TAG		= 21,
	HTT_STATS_TX_DE_ENQUEUE_DISCARD_TAG		= 22,
	HTT_STATS_TX_DE_CMN_TAG				= 23,
	HTT_STATS_RING_IF_TAG				= 24,
	HTT_STATS_TX_PDEV_MU_MIMO_STATS_TAG		= 25,
	HTT_STATS_SFM_CMN_TAG				= 26,
	HTT_STATS_SRING_STATS_TAG			= 27,
	HTT_STATS_RX_PDEV_FW_STATS_TAG			= 28,
	HTT_STATS_RX_PDEV_FW_RING_MPDU_ERR_TAG		= 29,
	HTT_STATS_RX_PDEV_FW_MPDU_DROP_TAG		= 30,
	HTT_STATS_RX_SOC_FW_STATS_TAG			= 31,
	HTT_STATS_RX_SOC_FW_REFILL_RING_EMPTY_TAG	= 32,
	HTT_STATS_RX_SOC_FW_REFILL_RING_NUM_REFILL_TAG	= 33,
	HTT_STATS_TX_PDEV_RATE_STATS_TAG		= 34,
	HTT_STATS_RX_PDEV_RATE_STATS_TAG		= 35,
	HTT_STATS_TX_PDEV_SCHEDULER_TXQ_STATS_TAG	= 36,
	HTT_STATS_TX_SCHED_CMN_TAG			= 37,
	HTT_STATS_SCHED_TXQ_CMD_POSTED_TAG		= 39,
	HTT_STATS_RING_IF_CMN_TAG			= 40,
	HTT_STATS_SFM_CLIENT_USER_TAG			= 41,
	HTT_STATS_SFM_CLIENT_TAG			= 42,
	HTT_STATS_TX_TQM_ERROR_STATS_TAG                = 43,
	HTT_STATS_SCHED_TXQ_CMD_REAPED_TAG		= 44,
	HTT_STATS_SRING_CMN_TAG				= 45,
	HTT_STATS_TX_SELFGEN_AC_ERR_STATS_TAG		= 46,
	HTT_STATS_TX_SELFGEN_CMN_STATS_TAG		= 47,
	HTT_STATS_TX_SELFGEN_AC_STATS_TAG		= 48,
	HTT_STATS_TX_SELFGEN_AX_STATS_TAG		= 49,
	HTT_STATS_TX_SELFGEN_AX_ERR_STATS_TAG		= 50,
	HTT_STATS_TX_HWQ_MUMIMO_SCH_STATS_TAG		= 51,
	HTT_STATS_TX_HWQ_MUMIMO_MPDU_STATS_TAG		= 52,
	HTT_STATS_TX_HWQ_MUMIMO_CMN_STATS_TAG		= 53,
	HTT_STATS_HW_INTR_MISC_TAG			= 54,
	HTT_STATS_HW_WD_TIMEOUT_TAG			= 55,
	HTT_STATS_HW_PDEV_ERRS_TAG			= 56,
	HTT_STATS_COUNTER_NAME_TAG			= 57,
	HTT_STATS_TX_TID_DETAILS_TAG			= 58,
	HTT_STATS_RX_TID_DETAILS_TAG			= 59,
	HTT_STATS_PEER_STATS_CMN_TAG			= 60,
	HTT_STATS_PEER_DETAILS_TAG			= 61,
	HTT_STATS_PEER_TX_RATE_STATS_TAG		= 62,
	HTT_STATS_PEER_RX_RATE_STATS_TAG		= 63,
	HTT_STATS_PEER_MSDU_FLOWQ_TAG			= 64,
	HTT_STATS_TX_DE_COMPL_STATS_TAG			= 65,
	HTT_STATS_WHAL_TX_TAG				= 66,
	HTT_STATS_TX_PDEV_SIFS_HIST_TAG			= 67,
	HTT_STATS_RX_PDEV_FW_STATS_PHY_ERR_TAG		= 68,
	HTT_STATS_TX_TID_DETAILS_V1_TAG			= 69,
	HTT_STATS_PDEV_CCA_1SEC_HIST_TAG		= 70,
	HTT_STATS_PDEV_CCA_100MSEC_HIST_TAG		= 71,
	HTT_STATS_PDEV_CCA_STAT_CUMULATIVE_TAG		= 72,
	HTT_STATS_PDEV_CCA_COUNTERS_TAG			= 73,
	HTT_STATS_TX_PDEV_MPDU_STATS_TAG		= 74,
	HTT_STATS_PDEV_TWT_SESSIONS_TAG			= 75,
	HTT_STATS_PDEV_TWT_SESSION_TAG			= 76,
	HTT_STATS_RX_REFILL_RXDMA_ERR_TAG		= 77,
	HTT_STATS_RX_REFILL_REO_ERR_TAG			= 78,
	HTT_STATS_RX_REO_RESOURCE_STATS_TAG		= 79,
	HTT_STATS_TX_SOUNDING_STATS_TAG			= 80,
	HTT_STATS_TX_PDEV_TX_PPDU_STATS_TAG		= 81,
	HTT_STATS_TX_PDEV_TRIED_MPDU_CNT_HIST_TAG	= 82,
	HTT_STATS_TX_HWQ_TRIED_MPDU_CNT_HIST_TAG	= 83,
	HTT_STATS_TX_HWQ_TXOP_USED_CNT_HIST_TAG		= 84,
	HTT_STATS_TX_DE_FW2WBM_RING_FULL_HIST_TAG	= 85,
	HTT_STATS_SCHED_TXQ_SCHED_ORDER_SU_TAG		= 86,
	HTT_STATS_SCHED_TXQ_SCHED_INELIGIBILITY_TAG	= 87,
	HTT_STATS_PDEV_OBSS_PD_TAG			= 88,
	HTT_STATS_HW_WAR_TAG				= 89,
	HTT_STATS_RING_BACKPRESSURE_STATS_TAG		= 90,
	HTT_STATS_LATENCY_PROF_STATS_TAG		= 91,
	HTT_STATS_LATENCY_CTX_TAG			= 92,
	HTT_STATS_LATENCY_CNT_TAG			= 93,
	HTT_STATS_RX_PDEV_UL_TRIG_STATS_TAG		= 94,
	HTT_STATS_RX_PDEV_UL_OFDMA_USER_STATS_TAG	= 95,
	HTT_STATS_RX_PDEV_UL_MUMIMO_TRIG_STATS_TAG	= 97,
	HTT_STATS_RX_FSE_STATS_TAG			= 98,
	HTT_STATS_SCHED_TXQ_SUPERCYCLE_TRIGGER_TAG	= 100,
	HTT_STATS_PEER_CTRL_PATH_TXRX_STATS_TAG		= 101,
	HTT_STATS_PDEV_CTRL_PATH_TX_STATS_TAG		= 102,
	HTT_STATS_RX_PDEV_RATE_EXT_STATS_TAG		= 103,
	HTT_STATS_TX_PDEV_DL_MU_MIMO_STATS_TAG		= 104,
	HTT_STATS_TX_PDEV_UL_MU_MIMO_STATS_TAG		= 105,
	HTT_STATS_TX_PDEV_DL_MU_OFDMA_STATS_TAG		= 106,
	HTT_STATS_TX_PDEV_UL_MU_OFDMA_STATS_TAG		= 107,
	HTT_STATS_PDEV_TX_RATE_TXBF_STATS_TAG		= 108,
	HTT_STATS_UNSUPPORTED_ERROR_STATS_TAG		= 109,
	HTT_STATS_UNAVAILABLE_ERROR_STATS_TAG		= 110,
	HTT_STATS_TX_SELFGEN_AC_SCHED_STATUS_STATS_TAG	= 111,
	HTT_STATS_TX_SELFGEN_AX_SCHED_STATUS_STATS_TAG	= 112,
	HTT_STATS_STA_UL_OFDMA_STATS_TAG		= 117,
	HTT_STATS_VDEV_RTT_RESP_STATS_TAG		= 118,
	HTT_STATS_PKTLOG_AND_HTT_RING_STATS_TAG		= 119,
	HTT_STATS_DLPAGER_STATS_TAG			= 120,
	HTT_STATS_PHY_COUNTERS_TAG			= 121,
	HTT_STATS_PHY_STATS_TAG				= 122,
	HTT_STATS_PHY_RESET_COUNTERS_TAG		= 123,
	HTT_STATS_PHY_RESET_STATS_TAG			= 124,
	HTT_STATS_SOC_TXRX_STATS_COMMON_TAG		= 125,
	HTT_STATS_VDEV_TXRX_STATS_HW_STATS_TAG		= 126,
	HTT_STATS_VDEV_RTT_INIT_STATS_TAG		= 127,
	HTT_STATS_PER_RATE_STATS_TAG			= 128,
	HTT_STATS_MU_PPDU_DIST_TAG			= 129,
	HTT_STATS_TX_PDEV_MUMIMO_GRP_STATS_TAG		= 130,
	HTT_STATS_TX_PDEV_BE_RATE_STATS_TAG		= 131,
	HTT_STATS_AST_ENTRY_TAG				= 132,
	HTT_STATS_TX_PDEV_BE_DL_MU_OFDMA_STATS_TAG      = 133,
	HTT_STATS_TX_PDEV_BE_UL_MU_OFDMA_STATS_TAG      = 134,
	HTT_STATS_TX_PDEV_RATE_STATS_BE_BN_OFDMA_TAG	= 135,
	HTT_STATS_RX_PDEV_UL_MUMIMO_TRIG_BE_STATS_TAG	= 136,
	HTT_STATS_TX_SELFGEN_BE_ERR_STATS_TAG		= 137,
	HTT_STATS_TX_SELFGEN_BE_STATS_TAG		= 138,
	HTT_STATS_TX_SELFGEN_BE_SCHED_STATUS_STATS_TAG	= 139,
	HTT_STATS_RX_RING_STATS_TAG			= 142,
	HTT_STATS_RX_PDEV_BE_BN_UL_TRIG_TAG		= 143,
	HTT_STATS_TX_PDEV_SAWF_RATE_STATS_TAG		= 144,
	HTT_STATS_STRM_GEN_MPDUS_TAG			= 145,
	HTT_STATS_STRM_GEN_MPDUS_DETAILS_TAG		= 146,
	HTT_STATS_TXBF_OFDMA_AX_NDPA_STATS_TAG		= 147,
	HTT_STATS_TXBF_OFDMA_AX_NDP_STATS_TAG		= 148,
	HTT_STATS_TXBF_OFDMA_AX_BRP_STATS_TAG		= 149,
	HTT_STATS_TXBF_OFDMA_AX_STEER_STATS_TAG		= 150,
	HTT_STATS_TXBF_OFDMA_BE_NDPA_STATS_TAG		= 151,
	HTT_STATS_TXBF_OFDMA_BE_NDP_STATS_TAG		= 152,
	HTT_STATS_TXBF_OFDMA_BE_BRP_STATS_TAG		= 153,
	HTT_STATS_TXBF_OFDMA_BE_STEER_STATS_TAG		= 154,
	HTT_STATS_DMAC_RESET_STATS_TAG			= 155,
	HTT_STATS_RX_PDEV_BE_UL_OFDMA_USER_STATS_TAG	= 156,
	HTT_STATS_PHY_TPC_STATS_TAG			= 157,
	HTT_STATS_PDEV_PUNCTURE_STATS_TAG		= 158,
	HTT_STATS_ML_PEER_DETAILS_TAG			= 159,
	HTT_STATS_ML_PEER_EXT_DETAILS_TAG		= 160,
	HTT_STATS_ML_LINK_INFO_DETAILS_TAG		= 161,
	HTT_STATS_TX_PDEV_PPDU_DUR_TAG			= 162,
	HTT_STATS_RX_PDEV_PPDU_DUR_TAG			= 163,
	HTT_STATS_ODD_PDEV_MANDATORY_TAG		= 164,
	HTT_STATS_PDEV_SCHED_ALGO_OFDMA_STATS_TAG	= 165,
	HTT_DBG_ODD_MANDATORY_MUMIMO_TAG		= 166,
	HTT_DBG_ODD_MANDATORY_MUOFDMA_TAG		= 167,
	HTT_STATS_LATENCY_PROF_CAL_STATS_TAG		= 168,
	HTT_STATS_TX_PDEV_MUEDCA_PARAMS_STATS_TAG	= 169,
	HTT_STATS_PDEV_BW_MGR_STATS_TAG			= 170,
	HTT_STATS_TX_PDEV_AP_EDCA_PARAMS_STATS_TAG	= 171,
	HTT_STATS_TXBF_OFDMA_AX_STEER_MPDU_STATS_TAG	= 172,
	HTT_STATS_TXBF_OFDMA_BE_STEER_MPDU_STATS_TAG	= 173,
	HTT_STATS_PEER_AX_OFDMA_STATS_TAG		= 174,
	HTT_STATS_TX_PDEV_MU_EDCA_PARAMS_STATS_TAG	= 175,
	HTT_STATS_PDEV_MBSSID_CTRL_FRAME_STATS_TAG	= 176,
	HTT_STATS_TX_PDEV_MLO_ABORT_TAG			= 177,
	HTT_STATS_TX_PDEV_MLO_TXOP_ABORT_TAG		= 178,
	HTT_STATS_UMAC_SSR_TAG                          = 179,
	HTT_STATS_PDEV_TDMA_TAG				= 187,
	HTT_STATS_MLO_SCHED_STATS_TAG                   = 190,
	HTT_STATS_PDEV_MLO_IPC_STATS_TAG		= 191,
	HTT_STATS_WHAL_WSI_TAG				= 192,
	HTT_STATS_LATENCY_PROF_CAL_DATA_TAG		= 193,
	HTT_STATS_GTX_TAG				= 199,
	HTT_STATS_TX_PDEV_WIFI_RADAR_TAG		= 200,
	HTT_STATS_TXBF_OFDMA_BE_PARBW_TAG		= 201,
	HTT_STATS_PDEV_SPECTRAL_TAG			= 204,
	HTT_STATS_PDEV_RTT_DELAY_TAG			= 205,
	HTT_STATS_PDEV_AOA_TAG				= 206,
	HTT_STATS_PDEV_FTM_TPCCAL_TAG			= 207,
	HTT_STATS_PDEV_UL_MUMIMO_GRP_STATS_TAG		= 208,
	HTT_STATS_PDEV_UL_MUMIMO_DENYLIST_STATS_TAG	= 209,
	HTT_STATS_PDEV_UL_MUMIMO_SEQ_TERM_STATS_TAG	= 210,
	HTT_STATS_PDEV_UL_MUMIMO_HIST_INELIGIBILITY_TAG	= 211,
	HTT_STATS_PHY_PAPRD_PB_TAG			= 212,
	HTT_STATS_HDS_PROF_STATS_TAG			= 213,
	HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_ON_SCHED_POST_HIST_TAG	= 215,
	HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_IN_HWQ_HIST_TAG	= 216,
	HTT_STATS_TX_PDEV_PENDING_SEQ_CNT_IN_TXQ_HIST_TAG	= 217,
	HTT_STATS_SCHED_TXQ_EARLY_COMPL_TAG		= 218,
	HTT_STATS_RX_PDEV_BN_UL_OFDMA_USER_TAG		= 219,
	HTT_STATS_TX_SELFGEN_BN_ERR_TAG			= 220,
	HTT_STATS_TX_SELFGEN_BN_TAG			= 221,
	HTT_STATS_TX_SELFGEN_BN_SCHED_STATUS_TAG	= 222,
	HTT_STATS_TX_PDEV_BN_DL_MU_OFDMA_STATS_TAG	= 223,
	HTT_STATS_TX_PDEV_BN_UL_MU_OFDMA_STATS_TAG	= 224,
	HTT_STATS_TXBF_OFDMA_BN_NDPA_TAG                = 226,
	HTT_STATS_TXBF_OFDMA_BN_NDP_TAG                 = 227,
	HTT_STATS_TXBF_OFDMA_BN_BRP_TAG                 = 228,
	HTT_STATS_TXBF_OFDMA_BN_STEER_TAG               = 229,
	HTT_STATS_TXBF_OFDMA_BN_STEER_MPDU_TAG          = 230,
	HTT_STATS_TXBF_OFDMA_BN_PARBW_TAG               = 231,
	HTT_STATS_OPTIONAL_CONFIGS_STATS_TAG		= 232,
	HTT_STATS_FTM_TAG				= 234,
	HTT_STATS_PDEV_FTM_TPCCAL_EXT_TAG		= 235,
	HTT_STATS_TX_PDEV_BN_RATE_TAG			= 236,
	HTT_STATS_RX_PDEV_UL_MUMIMO_TRIG_BN_STATS_TAG   = 237,
	HTT_STATS_PDEV_ANI_STATS_HIST_TAG		= 240,
	HTT_STATS_ANI_SCALAR_TAG			= 241,
	HTT_STATS_ANI_PKT_CNT_TAG			= 242,
	HTT_STATS_ANI_CRC_PASS_TAG			= 243,
	HTT_STATS_ANI_PER_BLK_ERR_TAG			= 244,
	HTT_STATS_ANI_OTA_ERR_TAG			= 245,
	HTT_STATS_RESET_HISTORY_TAG			= 246,
	HTT_STATS_REGDB_CTRY_TAG			= 247,
	HTT_STATS_REGDB_REGDOMAIN_TAG			= 248,
	HTT_STATS_REG_6G_TAG				= 249,
	HTT_STATS_REG_6G_CH_PWR_INFO_TAG		= 250,
	HTT_STATS_REG_6G_OOBE_TAG			= 251,
	HTT_STATS_TX_SELFGEN_RESP_FRAME_STATS_TAG       = 252,
	HTT_STATS_DPD_HALPHY_TAG				= 253,
	HTT_STATS_DPD_HW_CAL_PARAMS_TAG			= 254,
	HTT_STATS_DPD_HW_CAL_RESULTS_TAG		= 255,
	HTT_STATS_TX_PDEV_TXOP_DUR_TAG                  = 256,
	HTT_STATS_TXQ_COMBINED_SEQ_STATE_TAG            = 257,
	HTT_STATS_CTL_TAG                               = 258,
	HTT_STATS_ENHANCED_CTL_TAG                      = 259,
	HTT_STATS_NPCA_STATS_TAG			= 260,
	HTT_STATS_SCHED_TXQ_TX_MODE_SIMPLIFIED_TAG	= 261,
	HTT_STATS_SCHED_TXQ_TX_MODE_WINNER_TAG		= 262,
	HTT_STATS_DFS_RADAR_HISTORY_TAG			= 263,
	HTT_STATS_DFS_INI_TAG			        = 264,
	HTT_STATS_DFS_IPC_RING_TAG			= 265,
	HTT_STATS_PHY_DPD_DEBUG_V1_TAG                  = 266,
	HTT_STATS_PHY_TPC_DEBUG_V1_TAG                  = 267,
	HTT_STATS_PHY_NF_SUBBAND_TAG			= 268,
	HTT_STATS_MAX_TAG,
};

#define HTT_STATS_MAX_STRING_SZ32            4
#define HTT_STATS_MACID_INVALID              0xff
#define HTT_TX_HWQ_MAX_DIFS_LATENCY_BINS     10
#define HTT_TX_HWQ_MAX_CMD_RESULT_STATS      13
#define HTT_TX_HWQ_MAX_CMD_STALL_STATS       5
#define HTT_TX_HWQ_MAX_FES_RESULT_STATS      10

#define ATH12K_HTT_TX_PDEV_MAX_PHY_ERR_STATS          142

#define ATH12K_HTT_STATS_MAC_ID				GENMASK(7, 0)

#define ATH12K_HTT_TX_PDEV_MAX_SIFS_BURST_STATS		9
#define ATH12K_HTT_TX_PDEV_MAX_FLUSH_REASON_STATS	150

/* MU MIMO distribution stats is a 2-dimensional array
 * with dimension one denoting stats for nr4[0] or nr8[1]
 */
#define ATH12K_HTT_STATS_NUM_NR_BINS_ACTUAL 1
#define ATH12K_HTT_STATS_NUM_NR_BINS			2
#define ATH12K_HTT_STATS_MAX_NUM_MU_PPDU_PER_BURST	10
#define ATH12K_HTT_TX_PDEV_MAX_SIFS_BURST_HIST_STATS	10
#define ATH12K_HTT_STATS_MAX_NUM_SCHED_STATUS		9
#define ATH12K_HTT_STATS_NUM_SCHED_STATUS_WORDS		\
	(ATH12K_HTT_STATS_NUM_NR_BINS * ATH12K_HTT_STATS_MAX_NUM_SCHED_STATUS)
#define ATH12K_HTT_STATS_MU_PPDU_PER_BURST_WORDS	\
	(ATH12K_HTT_STATS_NUM_NR_BINS * ATH12K_HTT_STATS_MAX_NUM_MU_PPDU_PER_BURST)

enum ath12k_htt_tx_pdev_underrun_enum {
	HTT_STATS_TX_PDEV_NO_DATA_UNDERRUN		= 0,
	HTT_STATS_TX_PDEV_DATA_UNDERRUN_BETWEEN_MPDU	= 1,
	HTT_STATS_TX_PDEV_DATA_UNDERRUN_WITHIN_MPDU	= 2,
	HTT_TX_PDEV_MAX_URRN_STATS			= 3,
};

enum ath12k_htt_stats_reset_cfg_param_alloc_pos {
	ATH12K_HTT_STATS_RESET_PARAM_CFG_32_BYTES = 1,
	ATH12K_HTT_STATS_RESET_PARAM_CFG_64_BYTES,
	ATH12K_HTT_STATS_RESET_PARAM_CFG_128_BYTES,
};

struct debug_htt_stats_req {
	bool done;
	bool override_cfg_param;
	u8 pdev_id;
	struct ath12k *ar; /* back-pointer for NF extraction in telemetry */
	enum ath12k_dbg_htt_ext_stats_type type;
	u32 cfg_param[4];
	u8 peer_addr[ETH_ALEN];
	struct completion htt_stats_rcvd;
	u32 buf_len;
	u8 buf[];
};

#define ATH12K_HTT_NUM_AC_WMM   0x4
struct ath12k_htt_tx_pdev_stats_cmn_tlv {
	__le32 mac_id__word;
	__le32 hw_queued;
	__le32 hw_reaped;
	__le32 underrun;
	__le32 hw_paused;
	__le32 hw_flush;
	__le32 hw_filt;
	__le32 tx_abort;
	__le32 mpdu_requed;
	__le32 tx_xretry;
	__le32 data_rc;
	__le32 mpdu_dropped_xretry;
	__le32 illgl_rate_phy_err;
	__le32 cont_xretry;
	__le32 tx_timeout;
	__le32 pdev_resets;
	__le32 phy_underrun;
	__le32 txop_ovf;
	__le32 seq_posted;
	__le32 seq_failed_queueing;
	__le32 seq_completed;
	__le32 seq_restarted;
	__le32 mu_seq_posted;
	__le32 seq_switch_hw_paused;
	__le32 next_seq_posted_dsr;
	__le32 seq_posted_isr;
	__le32 seq_ctrl_cached;
	__le32 mpdu_count_tqm;
	__le32 msdu_count_tqm;
	__le32 mpdu_removed_tqm;
	__le32 msdu_removed_tqm;
	__le32 mpdus_sw_flush;
	__le32 mpdus_hw_filter;
	__le32 mpdus_truncated;
	__le32 mpdus_ack_failed;
	__le32 mpdus_expired;
	__le32 mpdus_seq_hw_retry;
	__le32 ack_tlv_proc;
	__le32 coex_abort_mpdu_cnt_valid;
	__le32 coex_abort_mpdu_cnt;
	__le32 num_total_ppdus_tried_ota;
	__le32 num_data_ppdus_tried_ota;
	__le32 local_ctrl_mgmt_enqued;
	__le32 local_ctrl_mgmt_freed;
	__le32 local_data_enqued;
	__le32 local_data_freed;
	__le32 mpdu_tried;
	__le32 isr_wait_seq_posted;

	__le32 tx_active_dur_us_low;
	__le32 tx_active_dur_us_high;
	__le32 remove_mpdus_max_retries;
	__le32 comp_delivered;
	__le32 ppdu_ok;
	__le32 self_triggers;
	__le32 tx_time_dur_data;
	__le32 seq_qdepth_repost_stop;
	__le32 mu_seq_min_msdu_repost_stop;
	__le32 seq_min_msdu_repost_stop;
	__le32 seq_txop_repost_stop;
	__le32 next_seq_cancel;
	__le32 fes_offsets_err_cnt;
	__le32 num_mu_peer_blacklisted;
	__le32 mu_ofdma_seq_posted;
	__le32 ul_mumimo_seq_posted;
	__le32 ul_ofdma_seq_posted;

	__le32 thermal_suspend_cnt;
	__le32 dfs_suspend_cnt;
	__le32 tx_abort_suspend_cnt;
	__le32 tgt_spec_opaque_txq_suspend_info;
	__le32 last_suspend_reason;
	__le32 num_dyn_mimo_ps_dlmumimo_sequences;
	__le32 num_su_txbf_denylisted;
	__le32 pdev_up_time_us_low;
	__le32 pdev_up_time_us_high;
	__le32 ofdma_seq_flush;
	struct {
		__le32 low_32;
		__le32 high_32;
	} bytes_sent;
	__le32 num_ppdu_tried_ota_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 hw_reaped_lpi;
	__le32 hw_reaped_sp;
	__le32 hw_reaped_vlp;
	__le32 avg_channel_access_latency_per_ac[ATH12K_HTT_NUM_AC_WMM];
} __packed;


struct ath12k_htt_tx_pdev_ap_edca_params_stats_tlv {
	__le32 ul_mumimo_less_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_mumimo_medium_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_mumimo_highly_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_mumimo_default_relaxed[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_muofdma_less_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_muofdma_medium_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_muofdma_highly_aggressive[ATH12K_HTT_NUM_AC_WMM];
	__le32 ul_muofdma_default_relaxed[ATH12K_HTT_NUM_AC_WMM];
} __packed;

struct ath12k_htt_tx_pdev_pending_seq_cnt_on_sched_post_hist_tlv {
	__le32 mac_id__word;
	__le32 pending_seq_on_sched_post_hist[ATH12K_HTT_PDEV_STATS_MAX_SEQ_CTRL_HIST];
} __packed;

struct ath12k_htt_tx_pdev_pending_seq_cnt_in_hwq_hist_tlv {
	__le32 mac_id__word;
	__le32 active_seq_in_hwq_hist[ATH12K_HTT_PDEV_STATS_MAX_ACTIVE_SEQ_IN_HWQ_HIST];
} __packed;

struct ath12k_htt_tx_pdev_pending_seq_cnt_in_txq_hist_tlv {
	__le32 mac_id__word;
	__le32 active_seq_in_txq_hist[ATH12K_HTT_PDEV_STATS_MAX_SEQ_CTRL_HIST];
} __packed;

struct ath12k_htt_sched_txq_early_compl_tlv {
	__le32 mac_id__word;
	__le32 ist_txop_end_indicated_cnt;
	__le32 ist_txop_end_notify_at_cmd_status_end;
	__le32 ist_txop_end_notify_at_isr_end;
	__le32 sched_cmd_post_skip_on_seq_unavail;
	__le32 ist_txop_end_skip_on_seq_unavail;
	__le32 ist_txop_end_skip_on_mpdu_ownership;
	__le32 skip_early_schedule_due_to_per;
	__le32 sched_cmd_posted_at_hw_txop_end;
	__le32 sched_cmd_missed_at_hw_txop_end;
	__le32 sched_cmd_posted_at_sched_cmd_compl;
	__le32 sched_cmd_missed_at_sched_cmd_compl;
	__le32 num_qos_sched_runs;
} __packed;

struct ath12k_htt_tx_pdev_mu_edca_params_stats_tlv {
	__le32 relaxed_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 mumimo_aggressive_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 mumimo_relaxed_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 muofdma_aggressive_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 muofdma_relaxed_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 latency_mu_edca[ATH12K_HTT_NUM_AC_WMM];
	__le32 psd_boost_mu_edca[ATH12K_HTT_NUM_AC_WMM];
} __packed;

#define ATH12K_HTT_TX_PDEV_MLO_ABORT_COUNT	25

struct ath12k_htt_tx_pdev_stats_mlo_abort_tlv {
	DECLARE_FLEX_ARRAY(__le32, mlo_abort_cnt);
} __packed;

struct ath12k_htt_tx_pdev_stats_mlo_txop_abort_tlv {
	DECLARE_FLEX_ARRAY(__le32, mlo_txop_abort_cnt);
} __packed;

struct ath12k_htt_tx_pdev_stats_urrn_tlv {
	DECLARE_FLEX_ARRAY(__le32, urrn_stats);
} __packed;

struct ath12k_htt_tx_pdev_stats_flush_tlv {
	DECLARE_FLEX_ARRAY(__le32, flush_errs);
} __packed;

struct ath12k_htt_tx_pdev_stats_phy_err_tlv {
	DECLARE_FLEX_ARRAY(__le32, phy_errs);
} __packed;

struct ath12k_htt_tx_pdev_stats_sifs_tlv {
	DECLARE_FLEX_ARRAY(__le32, sifs_status);
} __packed;

struct ath12k_htt_pdev_ctrl_path_tx_stats_tlv {
	__le32 fw_tx_mgmt_subtype[ATH12K_HTT_STATS_SUBTYPE_MAX];
} __packed;

struct ath12k_htt_peer_ctrl_path_txrx_stats_tlv {
	/* peer mac address */
	u8 peer_mac_addr[ETH_ALEN];
	u8 rsvd[2];
	/* Num of tx mgmt frames with subtype on peer level */
	__le32 peer_tx_mgmt_subtype[ATH12K_HTT_STATS_MGMT_FRM_TYPE_MAX];
	/* Num of rx mgmt frames with subtype on peer level */
	__le32 peer_rx_mgmt_subtype[ATH12K_HTT_STATS_MGMT_FRM_TYPE_MAX];
} __packed;

struct ath12k_htt_tx_pdev_stats_sifs_hist_tlv {
	DECLARE_FLEX_ARRAY(__le32, sifs_hist_status);
} __packed;

enum ath12k_htt_stats_hw_mode {
	ATH12K_HTT_STATS_HWMODE_AC = 0,
	ATH12K_HTT_STATS_HWMODE_AX = 1,
	ATH12K_HTT_STATS_HWMODE_BE = 2,
};

#define HTT_NUM_MCS_PER_NSS 16

struct htt_stats_gtx_stats {
	__le32 gtx_enabled;
	__le32 mcs_tpc_min[HTT_NUM_MCS_PER_NSS];
	__le32 mcs_tpc_max[HTT_NUM_MCS_PER_NSS];
	__le32 mcs_tpc_diff[HTT_NUM_MCS_PER_NSS];
};


struct ath12k_htt_tx_pdev_mu_ppdu_dist_stats_tlv {
	__le32 hw_mode;
	__le32 num_seq_term_status[ATH12K_HTT_STATS_NUM_SCHED_STATUS_WORDS];
	__le32 num_ppdu_cmpl_per_burst[ATH12K_HTT_STATS_MU_PPDU_PER_BURST_WORDS];
	__le32 num_seq_posted[ATH12K_HTT_STATS_NUM_NR_BINS];
	__le32 num_ppdu_posted_per_burst[ATH12K_HTT_STATS_MU_PPDU_PER_BURST_WORDS];
	__le32 wifi_version;
} __packed;

/*
 * Introduce new TX counters to support 320MHz support and punctured modes
 */
enum ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_TYPE {
	ATH12K_HTT_TX_PDEV_STATS_PUNCTURED_NONE = 0,
	ATH12K_HTT_TX_PDEV_STATS_PUNCTURED_20 = 1,
	ATH12K_HTT_TX_PDEV_STATS_PUNCTURED_40 = 2,
	ATH12K_HTT_TX_PDEV_STATS_PUNCTURED_80 = 3,
	ATH12K_HTT_TX_PDEV_STATS_PUNCTURED_120 = 4,
	ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS = 5
};

#define ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES 2 /* 0 - Half, 1 - Quarter */
#define ATH12K_HTT_TX_PDEV_STATS_NUM_11BE_TRIGGER_TYPES	  6
#define ATH12K_HTT_TX_PDEV_STATS_NUM_HE_SIG_B_MCS_COUNTERS 6

enum ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE {
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_26,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_52,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_106,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_242,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_484,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_996,
	ATH12K_HTT_TX_RX_PDEV_STATS_AX_RU_SIZE_996x2,
	ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS,
};

#define ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS		12
#define ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS		4
#define ATH12K_HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS		5
#define ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS		4
#define ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS		8
#define ATH12K_HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES		7
#define ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_CCK_STATS		4
#define ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS		8
#define ATH12K_HTT_TX_PDEV_STATS_NUM_LTF			4
#define ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS		2
#define ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS	2
#define ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS	4
#define ATH12K_HTT_TX_PDEV_STATS_NUM_11AX_TRIGGER_TYPES		6
#define ATH12K_HTT_TX_PDEV_STATS_NUM_11BE_TRIGGER_TYPES		6
#define ATH12K_HTT_TX_PDEV_STATS_NUM_11BN_TRIGGER_TYPES		6
#define ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES		2
#define ATH12K_HTT_TX_PDEV_STATS_NUM_HE_SIG_B_MCS_COUNTERS	6
#define ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS		16
#define ATH12K_HTT_TX_PDEV_STATS_NUM_BE_BW_COUNTERS		5
#define ATH12K_HTT_TX_PDEV_STATS_NUM_PER_COUNTERS		101
#define ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS		5
#define ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS_EXT		14
/*Macros to use when appending iMCS values to MCS statistics*/
#define ATH12K_HTT_TX_RX_MCS_RATE_1                             1
#define ATH12K_HTT_TX_RX_MCS_RATE_3                             3
#define ATH12K_HTT_TX_RX_MCS_RATE_4                             4
#define ATH12K_HTT_TX_RX_MCS_RATE_7                             7


#define ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_COUNTERS		4
#define ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT		14
#define ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS	2
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT2_COUNTERS		5
#define ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS	5
/* 11bn MCS counters: all BE MCS indices and 4 UHR iMCS */
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BN_MCS_COUNTERS		20
/* 20,40,80,160,320 MHz */
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS		5
/* 1.1, 3.1, 4.1, 7.1 */
#define ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS	 4

#define ATH12K_HTT_TX_PDEV_STATS_TOTAL_MCS_COUNTERS		\
	(ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS +		\
	 ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS)
#define ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_DROP_COUNTERS		\
	(ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS +		\
	 ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +	\
	 ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS)

#define	HTT_TX_PDEV_RATE_STATS_MAC_ID	GENMASK(7, 0)
#define	HTT_TX_PDEV_RATE_STATS_WIFI_VERSION	GENMASK(11, 8)

struct ath12k_htt_tx_pdev_rate_stats_tlv {
	__le32 mac_id_word;
	__le32 tx_ldpc;
	__le32 rts_cnt;
	__le32 ack_rssi;
	__le32 tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 tx_su_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 tx_mu_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 tx_stbc[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 tx_pream[ATH12K_HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES];
	__le32 tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
		[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 tx_dcm[ATH12K_HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS];
	__le32 rts_success;
	__le32 tx_legacy_cck_rate[ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_CCK_STATS];
	__le32 tx_legacy_ofdm_rate[ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS];
	__le32 ac_mu_mimo_tx_ldpc;
	__le32 ax_mu_mimo_tx_ldpc;
	__le32 ofdma_tx_ldpc;
	__le32 tx_he_ltf[ATH12K_HTT_TX_PDEV_STATS_NUM_LTF];
	__le32 ac_mu_mimo_tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ax_mu_mimo_tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ofdma_tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ac_mu_mimo_tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 ax_mu_mimo_tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 ofdma_tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 ac_mu_mimo_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 ax_mu_mimo_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 ofdma_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 ac_mu_mimo_tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			    [ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ax_mimo_tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			    [ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ofdma_tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
		       [ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 trigger_type_11ax[ATH12K_HTT_TX_PDEV_STATS_NUM_11AX_TRIGGER_TYPES];
	__le32 tx_11ax_su_ext;
	__le32 tx_mcs_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 tx_stbc_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 tx_gi_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
		     [ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 ax_mu_mimo_tx_mcs_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 ofdma_tx_mcs_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 ax_tx_gi_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
				[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 ofd_tx_gi_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			   [ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 tx_mcs_ext_2[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 tx_bw_320mhz;
	__le32 tx_gi_ext_2[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
		       [ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 tx_su_punctured_mode[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 reduced_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	/** 11AC VHT DL MU MIMO TX BW stats at reduced channel config */
	__le32 reduced_ac_mu_mimo_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	/** 11AX HE DL MU MIMO TX BW stats at reduced channel config */
	__le32 reduced_ax_mu_mimo_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	/** 11AX HE DL MU OFDMA TX BW stats at reduced channel config */
	__le32 reduced_ax_mu_ofdma_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	/** 11AX HE DL MU OFDMA TX RU Size stats */
	__le32 ofdma_tx_ru_size[ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS];
	/** 11AX HE DL MU OFDMA HE-SIG-B MCS stats */
	__le32 ofdma_he_sig_b_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_HE_SIG_B_MCS_COUNTERS];
	/** 11AX HE SU data + embedded trigger PPDU success stats (stats for HETP ack success PPDU cnt) */
	__le32 ax_su_embedded_trigger_data_ppdu;
	/** 11AX HE SU data + embedded trigger PPDU failure stats (stats for HETP ack failure PPDU cnt) */
	__le32 ax_su_embedded_trigger_data_ppdu_err;
	/** sta side trigger stats */
	__le32 trigger_type_11be[ATH12K_HTT_TX_PDEV_STATS_NUM_11BE_TRIGGER_TYPES];
	__le32 extra_eht_ltf;
	__le32 extra_eht_ltf_ofdma;
	__le32 ofdma_ba_ru_size[ATH12K_HTT_TX_RX_PDEV_STATS_NUM_AX_RU_SIZE_CNTRS];
	__le32 tx_2xldpc;
	__le32 npca_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS];

	__le32 npca_tx_su_punctured_mode
		[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	/** STA side 11bn trigger stats */
	__le32 trigger_type_11bn[ATH12K_HTT_TX_PDEV_STATS_NUM_11BN_TRIGGER_TYPES];
};

struct ath12k_htt_tx_pdev_rate_stats_be_tlv {
	__le32 be_mu_mimo_tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS];
	__le32 be_mu_mimo_tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 be_mu_mimo_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BE_BW_COUNTERS];
	__le32 be_mu_mimo_tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			       [ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS];
	__le32 be_mu_mimo_tx_ldpc;
	__le32 wifi_version;
} __packed;

struct ath12k_htt_tx_pdev_rate_stats_sawf_tlv {
	__le32 rate_retry_mcs_drop_cnt;
	__le32 mcs_drop_rate[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_DROP_COUNTERS];
	__le32 per_histogram_cnt[ATH12K_HTT_TX_PDEV_STATS_NUM_PER_COUNTERS];
	__le32 low_latency_rate_cnt;
	__le32 su_burst_rate_drop_cnt;
	__le32 su_burst_rate_drop_fail_cnt;
} __packed;

#define HTT_TX_PDEV_BN_RATE_STATS_MAC_ID   GENMASK(7, 0)
#define HTT_TX_PDEV_BN_RATE_STATS_WIFI_VERSION     GENMASK(11, 8)

struct ath12k_htt_tx_pdev_bn_rate_stats_tlv {
	__le32 mac_id__word;

	__le32 tx_mcs_ext_3[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS];

	__le32 tx_gi_ext_3[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS];

	__le32 tx_stbc_ext_3[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS];
	__le32 tx_11bn_su_elr;
} __packed;



#define ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_CCK_STATS		4
#define ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_OFDM_STATS		8
#define ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS		12
#define ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS		4
#define ATH12K_HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS		5
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS		4
#define ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS		8
#define ATH12K_HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES		7
#define ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER			8
#define ATH12K_HTT_RX_PDEV_STATS_RXEVM_MAX_PILOTS_NSS		16
#define ATH12K_HTT_RX_PDEV_STATS_NUM_RU_SIZE_COUNTERS		6
#define ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER		8
#define ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS		2
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_2_COUNTERS			8

struct ath12k_htt_rx_pdev_rate_stats_tlv {
	__le32 mac_id_word;
	__le32 nsts;
	__le32 rx_ldpc;
	__le32 rts_cnt;
	__le32 rssi_mgmt;
	__le32 rssi_data;
	__le32 rssi_comb;
	__le32 rx_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rx_nss[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 rx_dcm[ATH12K_HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS];
	__le32 rx_stbc[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rx_bw[ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 rx_pream[ATH12K_HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES];
	u8 rssi_chain_in_db[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
		     [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 rx_gi[ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS]
		[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rssi_in_dbm;
	__le32 rx_11ax_su_ext;
	__le32 rx_11ac_mumimo;
	__le32 rx_11ax_mumimo;
	__le32 rx_11ax_ofdma;
	__le32 txbf;
	__le32 rx_legacy_cck_rate[ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_CCK_STATS];
	__le32 rx_legacy_ofdm_rate[ATH12K_HTT_RX_PDEV_STATS_NUM_LEGACY_OFDM_STATS];
	__le32 rx_active_dur_us_low;
	__le32 rx_active_dur_us_high;
	__le32 rx_11ax_ul_ofdma;
	__le32 ul_ofdma_rx_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ul_ofdma_rx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			  [ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 ul_ofdma_rx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 ul_ofdma_rx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 ul_ofdma_rx_stbc;
	__le32 ul_ofdma_rx_ldpc;
	__le32 rx_ulofdma_non_data_ppdu[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 rx_ulofdma_data_ppdu[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 rx_ulofdma_mpdu_ok[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 rx_ulofdma_mpdu_fail[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 nss_count;
	__le32 pilot_count;
	s32 rx_pil_evm_db[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			   [ATH12K_HTT_RX_PDEV_STATS_RXEVM_MAX_PILOTS_NSS];
	__le32 rx_pilot_evm_db_mean[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	s8 rx_ul_fd_rssi[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 per_chain_rssi_pkt_type;
	s8 rx_per_chain_rssi_in_dbm[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
				   [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 rx_su_ndpa;
	__le32 rx_11ax_su_txbf_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rx_mu_ndpa;
	__le32 rx_11ax_mu_txbf_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rx_br_poll;
	__le32 rx_11ax_dl_ofdma_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	__le32 rx_11ax_dl_ofdma_ru[ATH12K_HTT_RX_PDEV_STATS_NUM_RU_SIZE_COUNTERS];
	__le32 rx_ulmumimo_non_data_ppdu[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	__le32 rx_ulmumimo_data_ppdu[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	__le32 rx_ulmumimo_mpdu_ok[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	__le32 rx_ulmumimo_mpdu_fail[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	__le32 rx_ulofdma_non_data_nusers[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 rx_ulofdma_data_nusers[ATH12K_HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	__le32 rx_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS];
	__le32 rx_11bn_su_elr;
};

#define ATH12K_HTT_RX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES		2

#define HTT_RX_PDEV_RATE_EXT_STATS_WIFI_VERSION		GENMASK(3, 0)

struct ath12k_htt_rx_pdev_rate_ext_stats_tlv {
	u8 rssi_chain_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			 [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_COUNTERS];
	s8 rx_per_chain_rssi_ext_in_dbm[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
				       [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_COUNTERS];
	__le32 rssi_mcast_in_dbm;
	__le32 rssi_mgmt_in_dbm;
	__le32 rx_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_stbc_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_gi_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS]
		     [ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 ul_ofdma_rx_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 ul_ofdma_rx_gi_ext[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			      [ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_11ax_su_txbf_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_11ax_mu_txbf_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_11ax_dl_ofdma_mcs_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS_EXT];
	__le32 rx_mcs_ext_2[ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 rx_bw_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT2_COUNTERS];
	__le32 rx_gi_ext_2[ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS]
		[ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 rx_su_punctured_mode[ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 reduced_rx_bw[ATH12K_HTT_RX_PDEV_STATS_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	u8 rssi_chain_ext_2[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			   [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_2_COUNTERS];
	s8 rx_per_chain_rssi_ext_2_in_dbm[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
					 [ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_2_COUNTERS];
	/*Holds a HTT_RX_TX_PDEV_STATS_WIFI_VERSION value*/
	__le32 wifi_version_word;
	/* Number of rx 2xldpc packets */
	__le32 rx_2xldpc;
	__le32 npca_rx_bw_ext[ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS];

	__le32 npca_rx_su_punctured_mode
		[ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];

	__le32 rx_mcs_ext_3
		[ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS];
};

#define ATH12K_HTT_TX_PDEV_STATS_SCHED_PER_TXQ_MAC_ID	GENMASK(7, 0)
#define ATH12K_HTT_TX_PDEV_STATS_SCHED_PER_TXQ_ID	GENMASK(15, 8)

#define ATH12K_HTT_TX_PDEV_NUM_SCHED_ORDER_LOG	20

#define HTT_TX_PDEV_MAX_FLUSH_REASON_STATS     71
#define HTT_TX_PDEV_MAX_SIFS_BURST_STATS       9
#define HTT_TX_PDEV_MAX_SIFS_BURST_HIST_STATS  10
#define HTT_TX_PDEV_MAX_PHY_ERR_STATS          18
#define HTT_TX_PDEV_SCHED_TX_MODE_MAX          22
#define HTT_TX_PDEV_NUM_SCHED_ORDER_LOG        20

#define HTT_RX_STATS_REFILL_MAX_RING         4
#define HTT_RX_STATS_RXDMA_MAX_ERR           16
#define HTT_RX_STATS_FW_DROP_REASON_MAX      16

struct ath12k_htt_stats_tx_sched_cmn_tlv {
	__le32 mac_id__word;
	__le32 current_timestamp;
} __packed;

struct ath12k_htt_tx_pdev_stats_sched_per_txq_tlv {
	__le32 mac_id__word;
	__le32 sched_policy;
	__le32 last_sched_cmd_posted_timestamp;
	__le32 last_sched_cmd_compl_timestamp;
	__le32 sched_2_tac_lwm_count;
	__le32 sched_2_tac_ring_full;
	__le32 sched_cmd_post_failure;
	__le32 num_active_tids;
	__le32 num_ps_schedules;
	__le32 sched_cmds_pending;
	__le32 num_tid_register;
	__le32 num_tid_unregister;
	__le32 num_qstats_queried;
	__le32 qstats_update_pending;
	__le32 last_qstats_query_timestamp;
	__le32 num_tqm_cmdq_full;
	__le32 num_de_sched_algo_trigger;
	__le32 num_rt_sched_algo_trigger;
	__le32 num_tqm_sched_algo_trigger;
	__le32 notify_sched;
	__le32 dur_based_sendn_term;
	__le32 su_notify2_sched;
	__le32 su_optimal_queued_msdus_sched;
	__le32 su_delay_timeout_sched;
	__le32 su_min_txtime_sched_delay;
	__le32 su_no_delay;
	__le32 num_supercycles;
	__le32 num_subcycles_with_sort;
	__le32 num_subcycles_no_sort;
	__le32 num_dps_client_scheduled;
	__le32 num_allowed_first_sched_command_for_combining;
	__le32 num_allowed_second_command_for_combining;
	__le32 num_aborted_first_sched_command;
	__le32 num_aborted_second_sched_command;
	__le32 total_combined_sched_cmds_success;
	__le32 total_combined_sched_cmds_failed;
} __packed;

struct ath12k_htt_sched_txq_tx_mode_winner_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_tx_mode_winner);
} __packed;

struct ath12k_htt_sched_txq_tx_mode_simplified_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_tx_mode_simplified);
} __packed;

struct ath12k_htt_sched_txq_cmd_posted_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_cmd_posted);
} __packed;

struct ath12k_htt_sched_txq_cmd_reaped_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_cmd_reaped);
} __packed;

struct ath12k_htt_sched_txq_sched_order_su_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_order_su);
} __packed;

struct ath12k_htt_sched_txq_sched_ineligibility_tlv {
	DECLARE_FLEX_ARRAY(__le32, sched_ineligibility);
} __packed;

enum ath12k_htt_sched_txq_supercycle_triggers_tlv_enum {
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_NONE = 0,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_FORCED,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_LESS_NUM_TIDQ_ENTRIES,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_LESS_NUM_ACTIVE_TIDS,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_MAX_ITR_REACHED,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_DUR_THRESHOLD_REACHED,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_TWT_TRIGGER,
	ATH12K_HTT_SCHED_SUPERCYCLE_TRIGGER_MAX,
};

struct ath12k_htt_sched_txq_supercycle_triggers_tlv {
	DECLARE_FLEX_ARRAY(__le32, supercycle_triggers);
} __packed;

enum ath12k_htt_sched_txq_combined_seq_status_tlv_enum {
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_SUCCESS = 0,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_NO_ACTIVE_PREV_SEQ,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_B2B_ALLOW_FLAG,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_SOUNDING_SEQ_ABORT,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_SEQ_NOT_CONSTRUCTED,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_HW_PAUSED_SEQ_CONSRUCTED,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_HW_PAUSED_SEQ_NOT_CONSTRUCTED,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_HW_PAUSED_SEQ_POSTED,
	ATH12K_HTT_SCHED_COMBINED_SEQ_STATUS_MAX,
};

struct ath12k_htt_stats_sched_txq_combined_seq_state_tlv {
	DECLARE_FLEX_ARRAY(__le32, combined_seq_state);
} __packed;

#define HTT_STATS_MAX_HW_MODULE_NAME_LEN 8
struct htt_hw_stats_wd_timeout_tlv {
	/* Stored as little endian */
	u8 hw_module_name[HTT_STATS_MAX_HW_MODULE_NAME_LEN];
	u32 count;
};

/* ============ PEER STATS ============ */
struct htt_msdu_flow_stats_tlv {
	u32 last_update_timestamp;
	u32 last_add_timestamp;
	u32 last_remove_timestamp;
	u32 total_processed_msdu_count;
	u32 cur_msdu_count_in_flowq;
	u32 sw_peer_id;
	u32 tx_flow_no__tid_num__drop_rule;
	u32 last_cycle_enqueue_count;
	u32 last_cycle_dequeue_count;
	u32 last_cycle_drop_count;
	u32 current_drop_th;
};

#define MAX_HTT_TID_NAME 8

/* Tidq stats */
struct htt_tx_tid_stats_tlv {
	/* Stored as little endian */
	u8     tid_name[MAX_HTT_TID_NAME];
	u32 sw_peer_id__tid_num;
	u32 num_sched_pending__num_ppdu_in_hwq;
	u32 tid_flags;
	u32 hw_queued;
	u32 hw_reaped;
	u32 mpdus_hw_filter;

	u32 qdepth_bytes;
	u32 qdepth_num_msdu;
	u32 qdepth_num_mpdu;
	u32 last_scheduled_tsmp;
	u32 pause_module_id;
	u32 block_module_id;
	u32 tid_tx_airtime;
};

/* Tidq stats */
struct htt_tx_tid_stats_v1_tlv {
	/* Stored as little endian */
	u8 tid_name[MAX_HTT_TID_NAME];
	u32 sw_peer_id__tid_num;
	u32 num_sched_pending__num_ppdu_in_hwq;
	u32 tid_flags;
	u32 max_qdepth_bytes;
	u32 max_qdepth_n_msdus;
	u32 rsvd;

	u32 qdepth_bytes;
	u32 qdepth_num_msdu;
	u32 qdepth_num_mpdu;
	u32 last_scheduled_tsmp;
	u32 pause_module_id;
	u32 block_module_id;
	u32 tid_tx_airtime;
	u32 allow_n_flags;
	u32 sendn_frms_allowed;
};

struct htt_rx_tid_stats_tlv {
	u32 sw_peer_id__tid_num;
	u8 tid_name[MAX_HTT_TID_NAME];
	u32 dup_in_reorder;
	u32 dup_past_outside_window;
	u32 dup_past_within_window;
	u32 rxdesc_err_decrypt;
	u32 tid_rx_airtime;
};

struct ath12k_htt_hw_stats_pdev_errs_tlv {
	__le32 mac_id__word;
	__le32 tx_abort;
	__le32 tx_abort_fail_count;
	__le32 rx_abort;
	__le32 rx_abort_fail_count;
	__le32 warm_reset;
	__le32 cold_reset;
	__le32 tx_flush;
	__le32 tx_glb_reset;
	__le32 tx_txq_reset;
	__le32 rx_timeout_reset;
	__le32 mac_cold_reset_restore_cal;
	__le32 mac_cold_reset;
	__le32 mac_warm_reset;
	__le32 mac_only_reset;
	__le32 phy_warm_reset;
	__le32 phy_warm_reset_ucode_trig;
	__le32 mac_warm_reset_restore_cal;
	__le32 mac_sfm_reset;
	__le32 phy_warm_reset_m3_ssr;
	__le32 phy_warm_reset_reason_phy_m3;
	__le32 phy_warm_reset_reason_tx_hw_stuck;
	__le32 phy_warm_reset_reason_num_rx_frame_stuck;
	__le32 phy_warm_reset_reason_wal_rx_rec_rx_busy;
	__le32 phy_warm_reset_reason_wal_rx_rec_mac_hng;
	__le32 phy_warm_reset_reason_mac_conv_phy_reset;
	__le32 wal_rx_recovery_rst_mac_hang_cnt;
	__le32 wal_rx_recovery_rst_known_sig_cnt;
	__le32 wal_rx_recovery_rst_no_rx_cnt;
	__le32 wal_rx_recovery_rst_no_rx_consec_cnt;
	__le32 wal_rx_recovery_rst_rx_busy_cnt;
	__le32 wal_rx_recovery_rst_phy_mac_hang_cnt;
	__le32 rx_flush_cnt;
	__le32 phy_warm_reset_reason_tx_exp_cca_stuck;
	__le32 phy_warm_reset_reason_tx_consec_flsh_war;
	__le32 phy_warm_reset_reason_tx_hwsch_reset_war;
	__le32 phy_warm_reset_reason_hwsch_cca_wdog_war;
	__le32 fw_rx_rings_reset;
	__le32 rx_dest_drain_rx_descs_leak_prevented;
	__le32 rx_dest_drain_rx_descs_saved_cnt;
	__le32 rx_dest_drain_rxdma2reo_leak_detected;
	__le32 rx_dest_drain_rxdma2fw_leak_detected;
	__le32 rx_dest_drain_rxdma2wbm_leak_detected;
	__le32 rx_dest_drain_rxdma1_2sw_leak_detected;
	__le32 rx_dest_drain_rx_drain_ok_mac_idle;
	__le32 rx_dest_drain_ok_mac_not_idle;
	__le32 rx_dest_drain_prerequisite_invld;
	__le32 rx_dest_drain_skip_non_lmac_reset;
	__le32 rx_dest_drain_hw_fifo_notempty_post_wait;
} __packed;

/* =========== MUMIMO HWQ stats =========== */
/* MU MIMO stats per hwQ */
struct htt_tx_hwq_mu_mimo_sch_stats_tlv {
	u32 mu_mimo_sch_posted;
	u32 mu_mimo_sch_failed;
	u32 mu_mimo_ppdu_posted;
};

struct htt_tx_hwq_mu_mimo_mpdu_stats_tlv {
	u32 mu_mimo_mpdus_queued_usr;
	u32 mu_mimo_mpdus_tried_usr;
	u32 mu_mimo_mpdus_failed_usr;
	u32 mu_mimo_mpdus_requeued_usr;
	u32 mu_mimo_err_no_ba_usr;
	u32 mu_mimo_mpdu_underrun_usr;
	u32 mu_mimo_ampdu_underrun_usr;
};

struct htt_tx_hwq_mu_mimo_cmn_stats_tlv {
	u32 mac_id__hwq_id__word;
};

/* == TX HWQ STATS == */
struct htt_tx_hwq_stats_cmn_tlv {
	u32 mac_id__hwq_id__word;

	/* PPDU level stats */
	u32 xretry;
	u32 underrun_cnt;
	u32 flush_cnt;
	u32 filt_cnt;
	u32 null_mpdu_bmap;
	u32 user_ack_failure;
	u32 ack_tlv_proc;
	u32 sched_id_proc;
	u32 null_mpdu_tx_count;
	u32 mpdu_bmap_not_recvd;

	/* Selfgen stats per hwQ */
	u32 num_bar;
	u32 rts;
	u32 cts2self;
	u32 qos_null;

	/* MPDU level stats */
	u32 mpdu_tried_cnt;
	u32 mpdu_queued_cnt;
	u32 mpdu_ack_fail_cnt;
	u32 mpdu_filt_cnt;
	u32 false_mpdu_ack_count;

	u32 txq_timeout;
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_hwq_difs_latency_stats_tlv_v {
	u32 hist_intvl;
	/* histogram of ppdu post to hwsch - > cmd status received */
	u32 difs_latency_hist[]; /* HTT_TX_HWQ_MAX_DIFS_LATENCY_BINS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_hwq_cmd_result_stats_tlv_v {
	/* Histogram of sched cmd result */
	u32 cmd_result[0]; /* HTT_TX_HWQ_MAX_CMD_RESULT_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_hwq_cmd_stall_stats_tlv_v {
	/* Histogram of various pause conitions */
	u32 cmd_stall_status[0]; /* HTT_TX_HWQ_MAX_CMD_STALL_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_hwq_fes_result_stats_tlv_v {
	/* Histogram of number of user fes result */
	u32 fes_result[0]; /* HTT_TX_HWQ_MAX_FES_RESULT_STATS */
};

/* NOTE: Variable length TLV, use length spec to infer array size
 *
 *  The hwq_tried_mpdu_cnt_hist is a  histogram of MPDUs tries per HWQ.
 *  The tries here is the count of the  MPDUS within a PPDU that the HW
 *  had attempted to transmit on  air, for the HWSCH Schedule command
 *  submitted by FW in this HWQ .It is not the retry attempts. The
 *  histogram bins are  0-29, 30-59, 60-89 and so on. The are 10 bins
 *  in this histogram.
 *  they are defined in FW using the following macros
 *  #define WAL_MAX_TRIED_MPDU_CNT_HISTOGRAM 9
 *  #define WAL_TRIED_MPDU_CNT_HISTOGRAM_INTERVAL 30
 */
struct htt_tx_hwq_tried_mpdu_cnt_hist_tlv_v {
	u32 hist_bin_size;
	/* Histogram of number of mpdus on tried mpdu */
	u32 tried_mpdu_cnt_hist[]; /* HTT_TX_HWQ_TRIED_MPDU_CNT_HIST */
};

/* NOTE: Variable length TLV, use length spec to infer array size
 *
 * The txop_used_cnt_hist is the histogram of txop per burst. After
 * completing the burst, we identify the txop used in the burst and
 * incr the corresponding bin.
 * Each bin represents 1ms & we have 10 bins in this histogram.
 * they are deined in FW using the following macros
 * #define WAL_MAX_TXOP_USED_CNT_HISTOGRAM 10
 * #define WAL_TXOP_USED_HISTOGRAM_INTERVAL 1000 ( 1 ms )
 */
struct htt_tx_hwq_txop_used_cnt_hist_tlv_v {
	/* Histogram of txop used cnt */
	u32 txop_used_cnt_hist[0]; /* HTT_TX_HWQ_TXOP_USED_CNT_HIST */
};

/* == TQM CMDQ stats == */
struct htt_tx_tqm_cmdq_status_tlv {
	u32 mac_id__cmdq_id__word;
	u32 sync_cmd;
	u32 write_cmd;
	u32 gen_mpdu_cmd;
	u32 mpdu_queue_stats_cmd;
	u32 mpdu_head_info_cmd;
	u32 msdu_flow_stats_cmd;
	u32 remove_mpdu_cmd;
	u32 remove_msdu_cmd;
	u32 flush_cache_cmd;
	u32 update_mpduq_cmd;
	u32 update_msduq_cmd;
};

/* == TX-DE STATS == */
/* Structures for tx de stats */
struct htt_tx_de_eapol_packets_stats_tlv {
	u32 m1_packets;
	u32 m2_packets;
	u32 m3_packets;
	u32 m4_packets;
	u32 g1_packets;
	u32 g2_packets;
};

struct htt_tx_de_classify_failed_stats_tlv {
	u32 ap_bss_peer_not_found;
	u32 ap_bcast_mcast_no_peer;
	u32 sta_delete_in_progress;
	u32 ibss_no_bss_peer;
	u32 invalid_vdev_type;
	u32 invalid_ast_peer_entry;
	u32 peer_entry_invalid;
	u32 ethertype_not_ip;
	u32 eapol_lookup_failed;
	u32 qpeer_not_allow_data;
	u32 fse_tid_override;
	u32 ipv6_jumbogram_zero_length;
	u32 qos_to_non_qos_in_prog;
};

struct htt_tx_de_classify_stats_tlv {
	u32 arp_packets;
	u32 igmp_packets;
	u32 dhcp_packets;
	u32 host_inspected;
	u32 htt_included;
	u32 htt_valid_mcs;
	u32 htt_valid_nss;
	u32 htt_valid_preamble_type;
	u32 htt_valid_chainmask;
	u32 htt_valid_guard_interval;
	u32 htt_valid_retries;
	u32 htt_valid_bw_info;
	u32 htt_valid_power;
	u32 htt_valid_key_flags;
	u32 htt_valid_no_encryption;
	u32 fse_entry_count;
	u32 fse_priority_be;
	u32 fse_priority_high;
	u32 fse_priority_low;
	u32 fse_traffic_ptrn_be;
	u32 fse_traffic_ptrn_over_sub;
	u32 fse_traffic_ptrn_bursty;
	u32 fse_traffic_ptrn_interactive;
	u32 fse_traffic_ptrn_periodic;
	u32 fse_hwqueue_alloc;
	u32 fse_hwqueue_created;
	u32 fse_hwqueue_send_to_host;
	u32 mcast_entry;
	u32 bcast_entry;
	u32 htt_update_peer_cache;
	u32 htt_learning_frame;
	u32 fse_invalid_peer;
	/*
	 * mec_notify is HTT TX WBM multicast echo check notification
	 * from firmware to host.  FW sends SA addresses to host for all
	 * multicast/broadcast packets received on STA side.
	 */
	u32    mec_notify;
};

struct htt_tx_de_classify_status_stats_tlv {
	u32 eok;
	u32 classify_done;
	u32 lookup_failed;
	u32 send_host_dhcp;
	u32 send_host_mcast;
	u32 send_host_unknown_dest;
	u32 send_host;
	u32 status_invalid;
};

struct htt_tx_de_enqueue_packets_stats_tlv {
	u32 enqueued_pkts;
	u32 to_tqm;
	u32 to_tqm_bypass;
};

struct htt_tx_de_enqueue_discard_stats_tlv {
	u32 discarded_pkts;
	u32 local_frames;
	u32 is_ext_msdu;
};

struct htt_tx_de_compl_stats_tlv {
	u32 tcl_dummy_frame;
	u32 tqm_dummy_frame;
	u32 tqm_notify_frame;
	u32 fw2wbm_enq;
	u32 tqm_bypass_frame;
};

/*
 *  The htt_tx_de_fw2wbm_ring_full_hist_tlv is a histogram of time we waited
 *  for the fw2wbm ring buffer.  we are requesting a buffer in FW2WBM release
 *  ring,which may fail, due to non availability of buffer. Hence we sleep for
 *  200us & again request for it. This is a histogram of time we wait, with
 *  bin of 200ms & there are 10 bin (2 seconds max)
 *  They are defined by the following macros in FW
 *  #define ENTRIES_PER_BIN_COUNT 1000  // per bin 1000 * 200us = 200ms
 *  #define RING_FULL_BIN_ENTRIES (WAL_TX_DE_FW2WBM_ALLOC_TIMEOUT_COUNT /
 *                               ENTRIES_PER_BIN_COUNT)
 */
struct htt_tx_de_fw2wbm_ring_full_hist_tlv {
	u32 fw2wbm_ring_full_hist[0];
};

struct htt_tx_de_cmn_stats_tlv {
	u32   mac_id__word;

	/* Global Stats */
	u32   tcl2fw_entry_count;
	u32   not_to_fw;
	u32   invalid_pdev_vdev_peer;
	u32   tcl_res_invalid_addrx;
	u32   wbm2fw_entry_count;
	u32   invalid_pdev;
};

#define ATH12K_HTT_STATS_RX_FW_RING_SIZE_NUM_ENTRIES(dword) (((dword) >> 0)  & 0xffff)
#define ATH12K_HTT_STATS_RX_FW_RING_CURR_NUM_ENTRIES(dword) (((dword) >> 16) & 0xffff)
#define ATH12K_WAL_RX_REO2SW4_BK_HIST_COUNT 3

/* Rx debug info for status rings */
struct ath12k_htt_stats_rx_ring_stats_tlv {
	/**
	 * BIT [15 :  0] :- max possible number of entries in respective ring
	 *                  (size of the ring in terms of entries)
	 * BIT [16 : 31] :- current number of entries occupied in respective ring
	 */
	__le32 entry_status_sw2rxdma;
	__le32 entry_status_rxdma2reo;
	__le32 entry_status_reo2sw1;
	__le32 entry_status_reo2sw4;
	__le32 entry_status_refillringipa;
	__le32 entry_status_refillringhost;
	/** datarate - Moving Average of Number of Entries */
	__le32 datarate_refillringipa;
	__le32 datarate_refillringhost;
	/**
	 * refillringhost_backpress_hist and refillringipa_backpress_hist are
	 * deprecated, and will be filled with 0x0 by the target.
	 */
	__le32 refillringhost_backpress_hist[ATH12K_WAL_RX_REO2SW4_BK_HIST_COUNT];
	__le32 refillringipa_backpress_hist[ATH12K_WAL_RX_REO2SW4_BK_HIST_COUNT];
	/**
	 * Number of times reo2sw4(IPA_DEST_RING) ring is back-pressured
	 * in recent time periods
	 * element 0: in last 0 to 250ms
	 * element 1: 250ms to 500ms
	 * element 2: above 500ms
	 */
	__le32 reo2sw4ringipa_backpress_hist[ATH12K_WAL_RX_REO2SW4_BK_HIST_COUNT];
} __packed;

/* == RING-IF STATS == */
#define HTT_STATS_LOW_WM_BINS      5
#define HTT_STATS_HIGH_WM_BINS     5

struct htt_ring_if_stats_tlv {
	u32 base_addr; /* DWORD aligned base memory address of the ring */
	u32 elem_size;
	u32 num_elems__prefetch_tail_idx;
	u32 head_idx__tail_idx;
	u32 shadow_head_idx__shadow_tail_idx;
	u32 num_tail_incr;
	u32 lwm_thresh__hwm_thresh;
	u32 overrun_hit_count;
	u32 underrun_hit_count;
	u32 prod_blockwait_count;
	u32 cons_blockwait_count;
	u32 low_wm_hit_count[HTT_STATS_LOW_WM_BINS];
	u32 high_wm_hit_count[HTT_STATS_HIGH_WM_BINS];
};

struct htt_ring_if_cmn_tlv {
	u32 mac_id__word;
	u32 num_records;
};

/* == SFM STATS == */
/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_sfm_client_user_tlv_v {
	/* Number of DWORDS used per user and per client */
	u32 dwords_used_by_user_n[0];
};

struct htt_sfm_client_tlv {
	/* Client ID */
	u32 client_id;
	/* Minimum number of buffers */
	u32 buf_min;
	/* Maximum number of buffers */
	u32 buf_max;
	/* Number of Busy buffers */
	u32 buf_busy;
	/* Number of Allocated buffers */
	u32 buf_alloc;
	/* Number of Available/Usable buffers */
	u32 buf_avail;
	/* Number of users */
	u32 num_users;
};

struct htt_sfm_cmn_tlv {
	u32 mac_id__word;
	/* Indicates the total number of 128 byte buffers
	 * in the CMEM that are available for buffer sharing
	 */
	u32 buf_total;
	/* Indicates for certain client or all the clients
	 * there is no dowrd saved in SFM, refer to SFM_R1_MEM_EMPTY
	 */
	u32 mem_empty;
	/* DEALLOCATE_BUFFERS, refer to register SFM_R0_DEALLOCATE_BUFFERS */
	u32 deallocate_bufs;
	/* Number of Records */
	u32 num_records;
};

/* == RX PDEV/SOC STATS == */
struct htt_rx_soc_fw_stats_tlv {
	u32 fw_reo_ring_data_msdu;
	u32 fw_to_host_data_msdu_bcmc;
	u32 fw_to_host_data_msdu_uc;
	u32 ofld_remote_data_buf_recycle_cnt;
	u32 ofld_remote_free_buf_indication_cnt;

	u32 ofld_buf_to_host_data_msdu_uc;
	u32 reo_fw_ring_to_host_data_msdu_uc;

	u32 wbm_sw_ring_reap;
	u32 wbm_forward_to_host_cnt;
	u32 wbm_target_recycle_cnt;

	u32 target_refill_ring_recycle_cnt;
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_soc_fw_refill_ring_empty_tlv_v {
	u32 refill_ring_empty_cnt[0]; /* HTT_RX_STATS_REFILL_MAX_RING */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_soc_fw_refill_ring_num_refill_tlv_v {
	u32 refill_ring_num_refill[0]; /* HTT_RX_STATS_REFILL_MAX_RING */
};

/* RXDMA error code from WBM released packets */
enum htt_rx_rxdma_error_code_enum {
	HTT_RX_RXDMA_OVERFLOW_ERR				= 0,
	HTT_RX_RXDMA_MPDU_LENGTH_ERR				= 1,
	HTT_RX_RXDMA_FCS_ERR					= 2,
	HTT_RX_RXDMA_DECRYPT_ERR				= 3,
	HTT_RX_RXDMA_TKIP_MIC_ERR				= 4,
	HTT_RX_RXDMA_UNECRYPTED_ERR				= 5,
	HTT_RX_RXDMA_MSDU_LEN_ERR				= 6,
	HTT_RX_RXDMA_MSDU_LIMIT_ERR				= 7,
	HTT_RX_RXDMA_WIFI_PARSE_ERR				= 8,
	HTT_RX_RXDMA_AMSDU_PARSE_ERR				= 9,
	HTT_RX_RXDMA_SA_TIMEOUT_ERR				= 10,
	HTT_RX_RXDMA_DA_TIMEOUT_ERR				= 11,
	HTT_RX_RXDMA_FLOW_TIMEOUT_ERR				= 12,
	HTT_RX_RXDMA_FLUSH_REQUEST				= 13,
	HTT_RX_RXDMA_ERR_CODE_RVSD0				= 14,
	HTT_RX_RXDMA_ERR_CODE_RVSD1				= 15,

	/* This MAX_ERR_CODE should not be used in any host/target messages,
	 * so that even though it is defined within a host/target interface
	 * definition header file, it isn't actually part of the host/target
	 * interface, and thus can be modified.
	 */
	HTT_RX_RXDMA_MAX_ERR_CODE
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_soc_fw_refill_ring_num_rxdma_err_tlv_v {
	u32 rxdma_err[0]; /* HTT_RX_RXDMA_MAX_ERR_CODE */
};

/* REO error code from WBM released packets */
enum htt_rx_reo_error_code_enum {
	HTT_RX_REO_QUEUE_DESC_ADDR_ZERO				= 0,
	HTT_RX_REO_QUEUE_DESC_NOT_VALID				= 1,
	HTT_RX_AMPDU_IN_NON_BA					= 2,
	HTT_RX_NON_BA_DUPLICATE					= 3,
	HTT_RX_BA_DUPLICATE					= 4,
	HTT_RX_REGULAR_FRAME_2K_JUMP				= 5,
	HTT_RX_BAR_FRAME_2K_JUMP				= 6,
	HTT_RX_REGULAR_FRAME_OOR				= 7,
	HTT_RX_BAR_FRAME_OOR					= 8,
	HTT_RX_BAR_FRAME_NO_BA_SESSION				= 9,
	HTT_RX_BAR_FRAME_SN_EQUALS_SSN				= 10,
	HTT_RX_PN_CHECK_FAILED					= 11,
	HTT_RX_2K_ERROR_HANDLING_FLAG_SET			= 12,
	HTT_RX_PN_ERROR_HANDLING_FLAG_SET			= 13,
	HTT_RX_QUEUE_DESCRIPTOR_BLOCKED_SET			= 14,
	HTT_RX_REO_ERR_CODE_RVSD				= 15,

	/* This MAX_ERR_CODE should not be used in any host/target messages,
	 * so that even though it is defined within a host/target interface
	 * definition header file, it isn't actually part of the host/target
	 * interface, and thus can be modified.
	 */
	HTT_RX_REO_MAX_ERR_CODE
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_soc_fw_refill_ring_num_reo_err_tlv_v {
	u32 reo_err[0]; /* HTT_RX_REO_MAX_ERR_CODE */
};

/* == RX PDEV STATS == */
#define HTT_STATS_SUBTYPE_MAX     16

struct htt_rx_pdev_fw_stats_tlv {
	__le32 mac_id__word;
	__le32 ppdu_recvd;
	__le32 mpdu_cnt_fcs_ok;
	__le32 mpdu_cnt_fcs_err;
	__le32 tcp_msdu_cnt;
	__le32 tcp_ack_msdu_cnt;
	__le32 udp_msdu_cnt;
	__le32 other_msdu_cnt;
	__le32 fw_ring_mpdu_ind;
	__le32 fw_ring_mgmt_subtype[HTT_STATS_SUBTYPE_MAX];
	__le32 fw_ring_ctrl_subtype[HTT_STATS_SUBTYPE_MAX];
	__le32 fw_ring_mcast_data_msdu;
	__le32 fw_ring_bcast_data_msdu;
	__le32 fw_ring_ucast_data_msdu;
	__le32 fw_ring_null_data_msdu;
	__le32 fw_ring_mpdu_drop;
	__le32 ofld_local_data_ind_cnt;
	__le32 ofld_local_data_buf_recycle_cnt;
	__le32 drx_local_data_ind_cnt;
	__le32 drx_local_data_buf_recycle_cnt;
	__le32 local_nondata_ind_cnt;
	__le32 local_nondata_buf_recycle_cnt;

	__le32 fw_status_buf_ring_refill_cnt;
	__le32 fw_status_buf_ring_empty_cnt;
	__le32 fw_pkt_buf_ring_refill_cnt;
	__le32 fw_pkt_buf_ring_empty_cnt;
	__le32 fw_link_buf_ring_refill_cnt;
	__le32 fw_link_buf_ring_empty_cnt;

	__le32 host_pkt_buf_ring_refill_cnt;
	__le32 host_pkt_buf_ring_empty_cnt;
	__le32 mon_pkt_buf_ring_refill_cnt;
	__le32 mon_pkt_buf_ring_empty_cnt;
	__le32 mon_status_buf_ring_refill_cnt;
	__le32 mon_status_buf_ring_empty_cnt;
	__le32 mon_desc_buf_ring_refill_cnt;
	__le32 mon_desc_buf_ring_empty_cnt;
	__le32 mon_dest_ring_update_cnt;
	__le32 mon_dest_ring_full_cnt;

	__le32 rx_suspend_cnt;
	__le32 rx_suspend_fail_cnt;
	__le32 rx_resume_cnt;
	__le32 rx_resume_fail_cnt;
	__le32 rx_ring_switch_cnt;
	__le32 rx_ring_restore_cnt;
	__le32 rx_flush_cnt;
	__le32 rx_recovery_reset_cnt;
	__le32 rx_lwm_prom_filter_dis;
	__le32 rx_hwm_prom_filter_en;
	__le32 bytes_received_low_32;
	__le32 bytes_received_high_32;
};

#define HTT_STATS_PHY_ERR_MAX 43

struct htt_rx_pdev_fw_stats_phy_err_tlv {
	u32 mac_id__word;
	u32 total_phy_err_cnt;
	/* Counts of different types of phy errs
	 * The mapping of PHY error types to phy_err array elements is HW dependent.
	 * The only currently-supported mapping is shown below:
	 *
	 * 0 phyrx_err_phy_off Reception aborted due to receiving a PHY_OFF TLV
	 * 1 phyrx_err_synth_off
	 * 2 phyrx_err_ofdma_timing
	 * 3 phyrx_err_ofdma_signal_parity
	 * 4 phyrx_err_ofdma_rate_illegal
	 * 5 phyrx_err_ofdma_length_illegal
	 * 6 phyrx_err_ofdma_restart
	 * 7 phyrx_err_ofdma_service
	 * 8 phyrx_err_ppdu_ofdma_power_drop
	 * 9 phyrx_err_cck_blokker
	 * 10 phyrx_err_cck_timing
	 * 11 phyrx_err_cck_header_crc
	 * 12 phyrx_err_cck_rate_illegal
	 * 13 phyrx_err_cck_length_illegal
	 * 14 phyrx_err_cck_restart
	 * 15 phyrx_err_cck_service
	 * 16 phyrx_err_cck_power_drop
	 * 17 phyrx_err_ht_crc_err
	 * 18 phyrx_err_ht_length_illegal
	 * 19 phyrx_err_ht_rate_illegal
	 * 20 phyrx_err_ht_zlf
	 * 21 phyrx_err_false_radar_ext
	 * 22 phyrx_err_green_field
	 * 23 phyrx_err_bw_gt_dyn_bw
	 * 24 phyrx_err_leg_ht_mismatch
	 * 25 phyrx_err_vht_crc_error
	 * 26 phyrx_err_vht_siga_unsupported
	 * 27 phyrx_err_vht_lsig_len_invalid
	 * 28 phyrx_err_vht_ndp_or_zlf
	 * 29 phyrx_err_vht_nsym_lt_zero
	 * 30 phyrx_err_vht_rx_extra_symbol_mismatch
	 * 31 phyrx_err_vht_rx_skip_group_id0
	 * 32 phyrx_err_vht_rx_skip_group_id1to62
	 * 33 phyrx_err_vht_rx_skip_group_id63
	 * 34 phyrx_err_ofdm_ldpc_decoder_disabled
	 * 35 phyrx_err_defer_nap
	 * 36 phyrx_err_fdomain_timeout
	 * 37 phyrx_err_lsig_rel_check
	 * 38 phyrx_err_bt_collision
	 * 39 phyrx_err_unsupported_mu_feedback
	 * 40 phyrx_err_ppdu_tx_interrupt_rx
	 * 41 phyrx_err_unsupported_cbf
	 * 42 phyrx_err_other
	 */
	u32 phy_err[HTT_STATS_PHY_ERR_MAX];
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_pdev_fw_ring_mpdu_err_tlv_v {
	/* Num error MPDU for each RxDMA error type  */
	u32 fw_ring_mpdu_err[0]; /* HTT_RX_STATS_RXDMA_MAX_ERR */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_rx_pdev_fw_mpdu_drop_tlv_v {
	/* Num MPDU dropped  */
	u32 fw_mpdu_drop[0]; /* HTT_RX_STATS_FW_DROP_REASON_MAX */
};

#define HTT_PDEV_CCA_STATS_TX_FRAME_INFO_PRESENT               (0x1)
#define HTT_PDEV_CCA_STATS_RX_FRAME_INFO_PRESENT               (0x2)
#define HTT_PDEV_CCA_STATS_RX_CLEAR_INFO_PRESENT               (0x4)
#define HTT_PDEV_CCA_STATS_MY_RX_FRAME_INFO_PRESENT            (0x8)
#define HTT_PDEV_CCA_STATS_USEC_CNT_INFO_PRESENT              (0x10)
#define HTT_PDEV_CCA_STATS_MED_RX_IDLE_INFO_PRESENT           (0x20)
#define HTT_PDEV_CCA_STATS_MED_TX_IDLE_GLOBAL_INFO_PRESENT    (0x40)
#define HTT_PDEV_CCA_STATS_CCA_OBBS_USEC_INFO_PRESENT         (0x80)

struct htt_pdev_stats_twt_session_tlv {
	u32 vdev_id;
	struct htt_mac_addr peer_mac;
	u32 flow_id_flags;

	/* TWT_DIALOG_ID_UNAVAILABLE is used
	 * when TWT session is not initiated by host
	 */
	u32 dialog_id;
	u32 wake_dura_us;
	u32 wake_intvl_us;
	u32 sp_offset_us;
};

struct htt_pdev_stats_twt_sessions_tlv {
	u32 pdev_id;
	u32 num_sessions;
	struct htt_pdev_stats_twt_session_tlv twt_session[];
};

enum htt_rx_reo_resource_sample_id_enum {
	/* Global link descriptor queued in REO */
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_0           = 0,
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_1           = 1,
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_2           = 2,
	/*Number of queue descriptors of this aging group */
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC0                   = 3,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC1                   = 4,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC2                   = 5,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC3                   = 6,
	/* Total number of MSDUs buffered in AC */
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC0               = 7,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC1               = 8,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC2               = 9,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC3               = 10,

	HTT_RX_REO_RESOURCE_STATS_MAX                          = 16
};

struct htt_rx_reo_resource_stats_tlv_v {
	/* Variable based on the Number of records. HTT_RX_REO_RESOURCE_STATS_MAX */
	u32 sample_id;
	u32 total_max;
	u32 total_avg;
	u32 total_sample;
	u32 non_zeros_avg;
	u32 non_zeros_sample;
	u32 last_non_zeros_max;
	u32 last_non_zeros_min;
	u32 last_non_zeros_avg;
	u32 last_non_zeros_sample;
};

/* == TX SCHED STATS == */
/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_sched_txq_cmd_posted_tlv_v {
	u32 sched_cmd_posted[0]; /* HTT_TX_PDEV_SCHED_TX_MODE_MAX */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_sched_txq_cmd_reaped_tlv_v {
	u32 sched_cmd_reaped[0]; /* HTT_TX_PDEV_SCHED_TX_MODE_MAX */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_sched_txq_sched_order_su_tlv_v {
	u32 sched_order_su[0]; /* HTT_TX_PDEV_NUM_SCHED_ORDER_LOG */
};

enum htt_sched_txq_sched_ineligibility_tlv_enum {
	HTT_SCHED_TID_SKIP_SCHED_MASK_DISABLED = 0,
	HTT_SCHED_TID_SKIP_NOTIFY_MPDU,
	HTT_SCHED_TID_SKIP_MPDU_STATE_INVALID,
	HTT_SCHED_TID_SKIP_SCHED_DISABLED,
	HTT_SCHED_TID_SKIP_TQM_BYPASS_CMD_PENDING,
	HTT_SCHED_TID_SKIP_SECOND_SU_SCHEDULE,

	HTT_SCHED_TID_SKIP_CMD_SLOT_NOT_AVAIL,
	HTT_SCHED_TID_SKIP_NO_ENQ,
	HTT_SCHED_TID_SKIP_LOW_ENQ,
	HTT_SCHED_TID_SKIP_PAUSED,
	HTT_SCHED_TID_SKIP_UL,
	HTT_SCHED_TID_REMOVE_PAUSED,
	HTT_SCHED_TID_REMOVE_NO_ENQ,
	HTT_SCHED_TID_REMOVE_UL,
	HTT_SCHED_TID_QUERY,
	HTT_SCHED_TID_SU_ONLY,
	HTT_SCHED_TID_ELIGIBLE,
	HTT_SCHED_INELIGIBILITY_MAX,
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_sched_txq_sched_ineligibility_tlv_v {
	/* indexed by htt_sched_txq_sched_ineligibility_tlv_enum */
	u32 sched_ineligibility[0];
};

struct htt_tx_pdev_stats_sched_per_txq_tlv {
	u32 mac_id__txq_id__word;
	u32 sched_policy;
	u32 last_sched_cmd_posted_timestamp;
	u32 last_sched_cmd_compl_timestamp;
	u32 sched_2_tac_lwm_count;
	u32 sched_2_tac_ring_full;
	u32 sched_cmd_post_failure;
	u32 num_active_tids;
	u32 num_ps_schedules;
	u32 sched_cmds_pending;
	u32 num_tid_register;
	u32 num_tid_unregister;
	u32 num_qstats_queried;
	u32 qstats_update_pending;
	u32 last_qstats_query_timestamp;
	u32 num_tqm_cmdq_full;
	u32 num_de_sched_algo_trigger;
	u32 num_rt_sched_algo_trigger;
	u32 num_tqm_sched_algo_trigger;
	u32 notify_sched;
	u32 dur_based_sendn_term;
};

struct htt_stats_tx_sched_cmn_tlv {
	/* BIT [ 7 :  0]   :- mac_id
	 * BIT [31 :  8]   :- reserved
	 */
	u32 mac_id__word;
	/* Current timestamp */
	u32 current_timestamp;
};

/* == TQM STATS == */
#define HTT_TX_TQM_MAX_GEN_MPDU_END_REASON          16
#define HTT_TX_TQM_MAX_LIST_MPDU_END_REASON         16
#define HTT_TX_TQM_MAX_LIST_MPDU_CNT_HISTOGRAM_BINS 16

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_tqm_gen_mpdu_stats_tlv_v {
	u32 gen_mpdu_end_reason[0]; /* HTT_TX_TQM_MAX_GEN_MPDU_END_REASON */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_tqm_list_mpdu_stats_tlv_v {
	u32 list_mpdu_end_reason[0]; /* HTT_TX_TQM_MAX_LIST_MPDU_END_REASON */
};

/* NOTE: Variable length TLV, use length spec to infer array size */
struct htt_tx_tqm_list_mpdu_cnt_tlv_v {
	u32 list_mpdu_cnt_hist[0];
			/* HTT_TX_TQM_MAX_LIST_MPDU_CNT_HISTOGRAM_BINS */
};

struct htt_tx_tqm_pdev_stats_tlv_v {
	u32 msdu_count;
	u32 mpdu_count;
	u32 remove_msdu;
	u32 remove_mpdu;
	u32 remove_msdu_ttl;
	u32 send_bar;
	u32 bar_sync;
	u32 notify_mpdu;
	u32 sync_cmd;
	u32 write_cmd;
	u32 hwsch_trigger;
	u32 ack_tlv_proc;
	u32 gen_mpdu_cmd;
	u32 gen_list_cmd;
	u32 remove_mpdu_cmd;
	u32 remove_mpdu_tried_cmd;
	u32 mpdu_queue_stats_cmd;
	u32 mpdu_head_info_cmd;
	u32 msdu_flow_stats_cmd;
	u32 remove_msdu_cmd;
	u32 remove_msdu_ttl_cmd;
	u32 flush_cache_cmd;
	u32 update_mpduq_cmd;
	u32 enqueue;
	u32 enqueue_notify;
	u32 notify_mpdu_at_head;
	u32 notify_mpdu_state_valid;
	/*
	 * On receiving TQM_FLOW_NOT_EMPTY_STATUS from TQM, (on MSDUs being enqueued
	 * the flow is non empty), if the number of MSDUs is greater than the threshold,
	 * notify is incremented. UDP_THRESH counters are for UDP MSDUs, and NONUDP are
	 * for non-UDP MSDUs.
	 * MSDUQ_SWNOTIFY_UDP_THRESH1 threshold    - sched_udp_notify1 is incremented
	 * MSDUQ_SWNOTIFY_UDP_THRESH2 threshold    - sched_udp_notify2 is incremented
	 * MSDUQ_SWNOTIFY_NONUDP_THRESH1 threshold - sched_nonudp_notify1 is incremented
	 * MSDUQ_SWNOTIFY_NONUDP_THRESH2 threshold - sched_nonudp_notify2 is incremented
	 *
	 * Notify signifies that we trigger the scheduler.
	 */
	u32 sched_udp_notify1;
	u32 sched_udp_notify2;
	u32 sched_nonudp_notify1;
	u32 sched_nonudp_notify2;
};

struct htt_tx_tqm_cmn_stats_tlv {
	u32 mac_id__word;
	u32 max_cmdq_id;
	u32 list_mpdu_cnt_hist_intvl;

	/* Global stats */
	u32 add_msdu;
	u32 q_empty;
	u32 q_not_empty;
	u32 drop_notification;
	u32 desc_threshold;
};

struct htt_tx_tqm_error_stats_tlv {
	/* Error stats */
	u32 q_empty_failure;
	u32 q_not_empty_failure;
	u32 add_msdu_failure;
};

/* == SRNG STATS == */
struct htt_sring_stats_tlv {
	u32 mac_id__ring_id__arena__ep;
	u32 base_addr_lsb; /* DWORD aligned base memory address of the ring */
	u32 base_addr_msb;
	u32 ring_size;
	u32 elem_size;

	u32 num_avail_words__num_valid_words;
	u32 head_ptr__tail_ptr;
	u32 consumer_empty__producer_full;
	u32 prefetch_count__internal_tail_ptr;
};

struct htt_sring_cmn_tlv {
	u32 num_records;
};

#define ATH12K_HTT_STATS_MAX_HW_INTR_NAME_LEN 8
struct ath12k_htt_hw_stats_intr_misc_tlv {
	u8 hw_intr_name[ATH12K_HTT_STATS_MAX_HW_INTR_NAME_LEN];
	__le32 mask;
	__le32 count;
} __packed;

struct ath12k_htt_hw_stats_whal_tx_tlv {
	__le32 mac_id__word;
	__le32 last_unpause_ppdu_id;
	__le32 hwsch_unpause_wait_tqm_write;
	__le32 hwsch_dummy_tlv_skipped;
	__le32 hwsch_misaligned_offset_received;
	__le32 hwsch_reset_count;
	__le32 hwsch_dev_reset_war;
	__le32 hwsch_delayed_pause;
	__le32 hwsch_long_delayed_pause;
	__le32 sch_rx_ppdu_no_response;
	__le32 sch_selfgen_response;
	__le32 sch_rx_sifs_resp_trigger;
} __packed;

struct ath12k_htt_hw_stats_whal_wsib_tlv {
	__le32 wsib_event_watchdog_timeout;
	__le32 wsib_event_slave_tlv_length_error;
	__le32 wsib_event_slave_parity_error;
	__le32 wsib_event_slave_direct_message;
	__le32 wsib_event_slave_backpressure_error;
	__le32 wsib_event_master_tlv_length_error;
} __packed;

struct ath12k_htt_hw_war_stats_tlv {
	__le32 mac_id__word;
	DECLARE_FLEX_ARRAY(__le32, hw_wars);
} __packed;

struct ath12k_htt_tx_tqm_cmn_stats_tlv {
	__le32 mac_id__word;
	__le32 max_cmdq_id;
	__le32 list_mpdu_cnt_hist_intvl;
	__le32 add_msdu;
	__le32 q_empty;
	__le32 q_not_empty;
	__le32 drop_notification;
	__le32 desc_threshold;
	__le32 hwsch_tqm_invalid_status;
	__le32 missed_tqm_gen_mpdus;
	__le32 tqm_active_tids;
	__le32 tqm_inactive_tids;
	__le32 tqm_active_msduq_flows;
	__le32 msduq_timestamp_updates;
	__le32 msduq_updates_mpdu_head_info_cmd;
	__le32 msduq_updates_emp_to_nonemp_status;
	__le32 get_mpdu_head_info_cmds_by_query;
	__le32 get_mpdu_head_info_cmds_by_tac;
	__le32 gen_mpdu_cmds_by_query;
	__le32 high_prio_q_not_empty;
} __packed;

struct ath12k_htt_tx_tqm_error_stats_tlv {
	__le32 q_empty_failure;
	__le32 q_not_empty_failure;
	__le32 add_msdu_failure;
	__le32 tqm_cache_ctl_err;
	__le32 tqm_soft_reset;
	__le32 tqm_reset_num_in_use_link_descs;
	__le32 tqm_reset_num_lost_link_descs;
	__le32 tqm_reset_num_lost_host_tx_buf_cnt;
	__le32 tqm_reset_num_in_use_internal_tqm;
	__le32 tqm_reset_num_in_use_idle_link_rng;
	__le32 tqm_reset_time_to_tqm_hang_delta_ms;
	__le32 tqm_reset_recovery_time_ms;
	__le32 tqm_reset_num_peers_hdl;
	__le32 tqm_reset_cumm_dirty_hw_mpduq_cnt;
	__le32 tqm_reset_cumm_dirty_hw_msduq_proc;
	__le32 tqm_reset_flush_cache_cmd_su_cnt;
	__le32 tqm_reset_flush_cache_cmd_other_cnt;
	__le32 tqm_reset_flush_cache_cmd_trig_type;
	__le32 tqm_reset_flush_cache_cmd_trig_cfg;
	__le32 tqm_reset_flush_cmd_skp_status_null;
} __packed;

struct ath12k_htt_tx_tqm_gen_mpdu_stats_tlv {
	DECLARE_FLEX_ARRAY(__le32, gen_mpdu_end_reason);
} __packed;

#define ATH12K_HTT_TX_TQM_MAX_LIST_MPDU_END_REASON		16
#define ATH12K_HTT_TX_TQM_MAX_LIST_MPDU_CNT_HISTOGRAM_BINS	16

struct ath12k_htt_tx_tqm_list_mpdu_stats_tlv {
	DECLARE_FLEX_ARRAY(__le32, list_mpdu_end_reason);
} __packed;

struct ath12k_htt_tx_tqm_list_mpdu_cnt_tlv {
	DECLARE_FLEX_ARRAY(__le32, list_mpdu_cnt_hist);
} __packed;

struct ath12k_htt_tx_tqm_pdev_stats_tlv {
	__le32 msdu_count;
	__le32 mpdu_count;
	__le32 remove_msdu;
	__le32 remove_mpdu;
	__le32 remove_msdu_ttl;
	__le32 send_bar;
	__le32 bar_sync;
	__le32 notify_mpdu;
	__le32 sync_cmd;
	__le32 write_cmd;
	__le32 hwsch_trigger;
	__le32 ack_tlv_proc;
	__le32 gen_mpdu_cmd;
	__le32 gen_list_cmd;
	__le32 remove_mpdu_cmd;
	__le32 remove_mpdu_tried_cmd;
	__le32 mpdu_queue_stats_cmd;
	__le32 mpdu_head_info_cmd;
	__le32 msdu_flow_stats_cmd;
	__le32 remove_msdu_cmd;
	__le32 remove_msdu_ttl_cmd;
	__le32 flush_cache_cmd;
	__le32 update_mpduq_cmd;
	__le32 enqueue;
	__le32 enqueue_notify;
	__le32 notify_mpdu_at_head;
	__le32 notify_mpdu_state_valid;
	__le32 sched_udp_notify1;
	__le32 sched_udp_notify2;
	__le32 sched_nonudp_notify1;
	__le32 sched_nonudp_notify2;
	__le32 tqm_enqueue_msdu_count;
	__le32 tqm_dropped_msdu_count;
	__le32 tqm_dequeue_msdu_count;
} __packed;

#define ATH12K_HTT_TX_PDEV_SIFS_BURST_HIST_STATS	10

struct ath12k_htt_odd_mandatory_pdev_stats_tlv {
	__le32 hw_queued;
	__le32 hw_reaped;
	__le32 hw_paused;
	__le32 hw_filt;
	__le32 seq_posted;
	__le32 seq_completed;
	__le32 underrun;
	__le32 hw_flush;
	__le32 next_seq_posted_dsr;
	__le32 seq_posted_isr;
	__le32 mpdu_cnt_fcs_ok;
	__le32 mpdu_cnt_fcs_err;
	__le32 msdu_count_tqm;
	__le32 mpdu_count_tqm;
	__le32 mpdus_ack_failed;
	__le32 num_data_ppdus_tried_ota;
	__le32 ppdu_ok;
	__le32 num_total_ppdus_tried_ota;
	__le32 thermal_suspend_cnt;
	__le32 dfs_suspend_cnt;
	__le32 tx_abort_suspend_cnt;
	__le32 suspended_txq_mask;
	__le32 last_suspend_reason;
	__le32 seq_failed_queueing;
	__le32 seq_restarted;
	__le32 seq_txop_repost_stop;
	__le32 next_seq_cancel;
	__le32 seq_min_msdu_repost_stop;
	__le32 total_phy_err_cnt;
	__le32 ppdu_recvd;
	__le32 tcp_msdu_cnt;
	__le32 tcp_ack_msdu_cnt;
	__le32 udp_msdu_cnt;
	__le32 fw_tx_mgmt_subtype[ATH12K_HTT_STATS_SUBTYPE_MAX];
	__le32 fw_rx_mgmt_subtype[ATH12K_HTT_STATS_SUBTYPE_MAX];
	__le32 fw_ring_mpdu_err[HTT_RX_STATS_RXDMA_MAX_ERR];
	__le32 urrn_stats[HTT_TX_PDEV_MAX_URRN_STATS];
	__le32 sifs_status[ATH12K_HTT_TX_PDEV_MAX_SIFS_BURST_STATS];
	__le32 sifs_hist_status[ATH12K_HTT_TX_PDEV_SIFS_BURST_HIST_STATS];
	__le32 rx_suspend_cnt;
	__le32 rx_suspend_fail_cnt;
	__le32 rx_resume_cnt;
	__le32 rx_resume_fail_cnt;
	__le32 hwq_beacon_cmd_result[HTT_TX_HWQ_MAX_CMD_RESULT_STATS];
	__le32 hwq_voice_cmd_result[HTT_TX_HWQ_MAX_CMD_RESULT_STATS];
	__le32 hwq_video_cmd_result[HTT_TX_HWQ_MAX_CMD_RESULT_STATS];
	__le32 hwq_best_effort_cmd_result[HTT_TX_HWQ_MAX_CMD_RESULT_STATS];
	__le32 hwq_beacon_mpdu_tried_cnt;
	__le32 hwq_voice_mpdu_tried_cnt;
	__le32 hwq_video_mpdu_tried_cnt;
	__le32 hwq_best_effort_mpdu_tried_cnt;
	__le32 hwq_beacon_mpdu_queued_cnt;
	__le32 hwq_voice_mpdu_queued_cnt;
	__le32 hwq_video_mpdu_queued_cnt;
	__le32 hwq_best_effort_mpdu_queued_cnt;
	__le32 hwq_beacon_mpdu_ack_fail_cnt;
	__le32 hwq_voice_mpdu_ack_fail_cnt;
	__le32 hwq_video_mpdu_ack_fail_cnt;
	__le32 hwq_best_effort_mpdu_ack_fail_cnt;
	__le32 pdev_resets;
	__le32 phy_warm_reset;
	__le32 hwsch_reset_count;
	__le32 phy_warm_reset_ucode_trig;
	__le32 mac_cold_reset;
	__le32 mac_warm_reset;
	__le32 mac_warm_reset_restore_cal;
	__le32 phy_warm_reset_m3_ssr;
	__le32 fw_rx_rings_reset;
	__le32 tx_flush;
	__le32 hwsch_dev_reset_war;
	__le32 mac_cold_reset_restore_cal;
	__le32 mac_only_reset;
	__le32 mac_sfm_reset;
	__le32 tx_ldpc; /* Number of tx PPDUs with LDPC coding */
	__le32 rx_ldpc; /* Number of rx PPDUs with LDPC coding */
	__le32 gen_mpdu_end_reason[HTT_TX_TQM_MAX_GEN_MPDU_END_REASON];
	__le32 list_mpdu_end_reason[ATH12K_HTT_TX_TQM_MAX_LIST_MPDU_END_REASON];
	__le32 tx_mcs[ATH12K_HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS +
		      ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
		      ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 half_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 quarter_tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 tx_su_punctured_mode[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 rx_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS +
		      ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
		      ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 rx_nss[ATH12K_HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 rx_bw[ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	__le32 rx_stbc[ATH12K_HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS +
		       ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA_MCS_COUNTERS +
		       ATH12K_HTT_RX_PDEV_STATS_NUM_EXTRA2_MCS_COUNTERS];
	__le32 rts_cnt;
	__le32 rts_success;
} __packed;

#define ATH12K_HTT_STATS_TDMA_MAC_ID_M	GENMASK(7, 0)

struct ath12k_htt_pdev_tdma_stats_tlv {
	__le32 mac_id__word;
	__le32 num_tdma_active_schedules;
	__le32 num_tdma_reserved_schedules;
	__le32 num_tdma_restricted_schedules;
	__le32 num_tdma_unconfigured_schedules;
	__le32 num_tdma_slot_switches;
	__le32 num_tdma_edca_switches;
} __packed;

struct ath12k_htt_tx_de_cmn_stats_tlv {
	__le32 mac_id__word;
	__le32 tcl2fw_entry_count;
	__le32 not_to_fw;
	__le32 invalid_pdev_vdev_peer;
	__le32 tcl_res_invalid_addrx;
	__le32 wbm2fw_entry_count;
	__le32 invalid_pdev;
	__le32 tcl_res_addrx_timeout;
	__le32 invalid_vdev;
	__le32 invalid_tcl_exp_frame_desc;
	__le32 vdev_id_mismatch_cnt;
} __packed;

struct ath12k_htt_tx_de_eapol_packets_stats_tlv {
	__le32 m1_packets;
	__le32 m2_packets;
	__le32 m3_packets;
	__le32 m4_packets;
	__le32 g1_packets;
	__le32 g2_packets;
	__le32 rc4_packets;
	__le32 eap_packets;
	__le32 eapol_start_packets;
	__le32 eapol_logoff_packets;
	__le32 eapol_encap_asf_packets;
	__le32 m1_success;
	__le32 m1_compl_fail;
	__le32 m2_success;
	__le32 m2_compl_fail;
	__le32 m3_success;
	__le32 m3_compl_fail;
	__le32 m4_success;
	__le32 m4_compl_fail;
	__le32 g1_success;
	__le32 g1_compl_fail;
	__le32 g2_success;
	__le32 g2_compl_fail;
	__le32 m1_enq_success;
	__le32 m1_enq_fail;
	__le32 m2_enq_success;
	__le32 m2_enq_fail;
	__le32 m3_enq_success;
	__le32 m3_enq_fail;
	__le32 m4_enq_success;
	__le32 m4_enq_fail;
	__le32 g1_enq_success;
	__le32 g1_enq_fail;
	__le32 g2_enq_success;
	__le32 g2_enq_fail;
} __packed;

struct ath12k_htt_tx_de_classify_stats_tlv {
	__le32 arp_packets;
	__le32 igmp_packets;
	__le32 dhcp_packets;
	__le32 host_inspected;
	__le32 htt_included;
	__le32 htt_valid_mcs;
	__le32 htt_valid_nss;
	__le32 htt_valid_preamble_type;
	__le32 htt_valid_chainmask;
	__le32 htt_valid_guard_interval;
	__le32 htt_valid_retries;
	__le32 htt_valid_bw_info;
	__le32 htt_valid_power;
	__le32 htt_valid_key_flags;
	__le32 htt_valid_no_encryption;
	__le32 fse_entry_count;
	__le32 fse_priority_be;
	__le32 fse_priority_high;
	__le32 fse_priority_low;
	__le32 fse_traffic_ptrn_be;
	__le32 fse_traffic_ptrn_over_sub;
	__le32 fse_traffic_ptrn_bursty;
	__le32 fse_traffic_ptrn_interactive;
	__le32 fse_traffic_ptrn_periodic;
	__le32 fse_hwqueue_alloc;
	__le32 fse_hwqueue_created;
	__le32 fse_hwqueue_send_to_host;
	__le32 mcast_entry;
	__le32 bcast_entry;
	__le32 htt_update_peer_cache;
	__le32 htt_learning_frame;
	__le32 fse_invalid_peer;
	__le32 mec_notify;
	__le32 arp_response;
	__le32 arp_request;
} __packed;

struct ath12k_htt_tx_de_classify_failed_stats_tlv {
	__le32 ap_bss_peer_not_found;
	__le32 ap_bcast_mcast_no_peer;
	__le32 sta_delete_in_progress;
	__le32 ibss_no_bss_peer;
	__le32 invalid_vdev_type;
	__le32 invalid_ast_peer_entry;
	__le32 peer_entry_invalid;
	__le32 ethertype_not_ip;
	__le32 eapol_lookup_failed;
	__le32 qpeer_not_allow_data;
	__le32 fse_tid_override;
	__le32 ipv6_jumbogram_zero_length;
	__le32 qos_to_non_qos_in_prog;
	__le32 ap_bcast_mcast_eapol;
	__le32 unicast_on_ap_bss_peer;
	__le32 ap_vdev_invalid;
	__le32 incomplete_llc;
	__le32 eapol_duplicate_m3;
	__le32 eapol_duplicate_m4;
	__le32 eapol_invalid_mac;
} __packed;

struct ath12k_htt_tx_de_classify_status_stats_tlv {
	__le32 eok;
	__le32 classify_done;
	__le32 lookup_failed;
	__le32 send_host_dhcp;
	__le32 send_host_mcast;
	__le32 send_host_unknown_dest;
	__le32 send_host;
	__le32 status_invalid;
} __packed;

struct ath12k_htt_tx_de_enqueue_packets_stats_tlv {
	__le32 enqueued_pkts;
	__le32 to_tqm;
	__le32 to_tqm_bypass;
} __packed;

struct ath12k_htt_tx_de_enqueue_discard_stats_tlv {
	__le32 discarded_pkts;
	__le32 local_frames;
	__le32 is_ext_msdu;
	__le32 mlo_invalid_routing_discard;
	__le32 mlo_invalid_routing_dup_entry_discard;
	__le32 discard_peer_unauthorized_pkts;
} __packed;

struct ath12k_htt_tx_de_compl_stats_tlv {
	__le32 tcl_dummy_frame;
	__le32 tqm_dummy_frame;
	__le32 tqm_notify_frame;
	__le32 fw2wbm_enq;
	__le32 tqm_bypass_frame;
} __packed;

enum ath12k_htt_tx_mumimo_grp_invalid_reason_code_stats {
	ATH12K_HTT_TX_MUMIMO_GRP_VALID,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_NUM_MU_USERS_EXCEEDED_MU_MAX_USERS,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_SCHED_ALGO_NOT_MU_COMPATIBLE_GID,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_NON_PRIMARY_GRP,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_ZERO_CANDIDATES,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_MORE_CANDIDATES,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_GROUP_SIZE_EXCEED_NSS,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_GROUP_INELIGIBLE,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_GROUP_EFF_MU_TPUT_OMBPS,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_GRP,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_TOTAL_NSS_LESS_THAN_GROUP_SIZE,
	ATH12K_HTT_TX_MUMIMO_GRP_INSUFFICIENT_CANDIDATES_UL_MU_1SS_RATE,
	ATH12K_HTT_TX_MUMIMO_GRP_MU_GRP_NOT_NEEDED,
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_MAX_REASON_CODE,
};

struct htt_vdev_rtt_resp_stats_tlv {
	/* No of Fine Timing Measurement frames transmitted successfully */
	u32 tx_ftm_suc;
	/* No of Fine Timing Measurement frames transmitted successfully after retry */
	u32 tx_ftm_suc_retry;
	/* No of Fine Timing Measurement frames not transmitted successfully */
	u32 tx_ftm_fail;
	/* No of Fine Timing Measurement Request frames received, including initial,
	 * non-initial, and duplicates
	 */
	u32 rx_ftmr_cnt;
	/* No of duplicate Fine Timing Measurement Request frames received, including
	 * both initial and non-initial
	 */
	u32 rx_ftmr_dup_cnt;
	/* No of initial Fine Timing Measurement Request frames received */
	u32 rx_iftmr_cnt;
	/* No of duplicate initial Fine Timing Measurement Request frames received */
	u32 rx_iftmr_dup_cnt;
	/* No of responder sessions rejected when initiator was active */
	u32 initiator_active_responder_rejected_cnt;
	/* Responder terminate count */
	u32 responder_terminate_cnt;
	u32 vdev_id;
};

struct htt_vdev_rtt_init_stats_tlv {
	u32 vdev_id;
	/* No of Fine Timing Measurement request frames transmitted successfully */
	u32 tx_ftmr_cnt;
	/* No of Fine Timing Measurement request frames not transmitted successfully */
	u32 tx_ftmr_fail;
	/* No of Fine Timing Measurement request frames transmitted successfully
	 * after retry
	 */
	u32 tx_ftmr_suc_retry;
	/* No of Fine Timing Measurement frames received, including initial, non-initial
	 * and duplicates
	 */
	u32 rx_ftm_cnt;
	/* Initiator Terminate count */
	u32 initiator_terminate_cnt;
	u32 tx_meas_req_count;
};

#define ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT 25
#define ATH12K_HTT_STATS_MAX_CHAINS		8
enum {
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_TYPE_NONE = 0,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_TYPE_GAIN_BINARY_SEARCH = 1,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_TYPE_TX_GAIN_BINARY_SEARCH = 2,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_TYPE_RECAL_GAIN_VALIDATION = 3,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_TYPE_RECAL_GAIN_BINARY_SEARCH = 4,
	/* the value 5 is reserved for future use */

	ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_TYPES = 6
};

enum {
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_NONE = 0,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_DPD_ABORT = 1,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_CONVERGENCE = 2,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_TX_EXCEEDS_RETRY = 3,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_CAPTURE = 4,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_NEW_CHANNEL_CHANGE = 5,
	ATH12K_HTT_STATS_WIFI_RADAR_CAL_FAILURE_NEW_CAL_REQ = 6,
	/* the values 7-9 are reserved for future use */

	ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_FAILURE_REASONS = 10
};

struct htt_stats_tx_pdev_wifi_radar_tlv {
	__le32 capture_in_progress;
	__le32 calibration_in_progress;
	__le32 periodicity;
	__le32 latest_req_timestamp;
	__le32 latest_resp_timestamp;
	__le32 latest_calibration_timing;
	__le32 calibration_timing_per_chain[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 wifi_radar_req_count;
	__le32 num_wifi_radar_pkt_success;
	__le32 num_wifi_radar_pkt_queued;
	__le32 num_wifi_radar_cal_pkt_success;
	__le32 wifi_radar_cal_init_tx_gain;
	__le32 latest_wifi_radar_cal_type;
	__le32 wifi_radar_cal_type_counts[ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_TYPES];
	__le32 latest_wifi_radar_cal_fail_reason;
	__le32 wifi_radar_cal_fail_reason_counts[
			ATH12K_HTT_STATS_NUM_WIFI_RADAR_CAL_FAILURE_REASONS];
	__le32 wifi_radar_licensed;
	__le32 cmd_results_cts2self[ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT];
	__le32 cmd_results_wifi_radar[ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT];
	/* Tx gain index from gain table obtained/used for calibration */
	__le32 wifi_radar_tx_gains[ATH12K_HTT_STATS_MAX_CHAINS];
	/* Rx gain index from gain table obtained/used from calibration */
	__le32 wifi_radar_rx_gains
			[ATH12K_HTT_STATS_MAX_CHAINS][ATH12K_HTT_STATS_MAX_CHAINS];
} __packed;

struct htt_pktlog_and_htt_ring_stats_tlv {
	/* No of pktlog payloads that were dropped in htt_ppdu_stats path */
	u32 pktlog_lite_drop_cnt;
	/* No of pktlog payloads that were dropped in TQM path */
	u32 pktlog_tqm_drop_cnt;
	/* No of pktlog ppdu stats payloads that were dropped */
	u32 pktlog_ppdu_stats_drop_cnt;
	/* No of pktlog ppdu ctrl payloads that were dropped */
	u32 pktlog_ppdu_ctrl_drop_cnt;
	/* No of pktlog sw events payloads that were dropped */
	u32 pktlog_sw_events_drop_cnt;
};

#define HTT_DLPAGER_STATS_MAX_HIST            10
#define HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_M 0x000000FF
#define HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_S 0
#define HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_M  0x0000FF00
#define HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_S  8
#define HTT_DLPAGER_TOTAL_LOCKED_PAGES_M      0x0000FFFF
#define HTT_DLPAGER_TOTAL_LOCKED_PAGES_S      0
#define HTT_DLPAGER_TOTAL_FREE_PAGES_M        0xFFFF0000
#define HTT_DLPAGER_TOTAL_FREE_PAGES_S        16
#define HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_M    0x0000FFFF
#define HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_S    0
#define HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_M  0xFFFF0000
#define HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_S  16

#define HTT_DLPAGER_ASYNC_LOCK_PAGE_COUNT_GET(_var) \
	(((_var) & HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_M) >> \
	HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_S)

#define HTT_DLPAGER_ASYNC_LOCK_PAGE_COUNT_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT, _val); \
		((_var) &= ~(HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_M));\
		((_var) |= ((_val) << HTT_DLPAGER_ASYNC_LOCKED_PAGE_COUNT_S)); \
	} while (0)

#define HTT_DLPAGER_SYNC_LOCK_PAGE_COUNT_GET(_var) \
	(((_var) & HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_M) >> \
	HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_S)

#define HTT_DLPAGER_SYNC_LOCK_PAGE_COUNT_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT, _val); \
		((_var) &= ~(HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_M));\
		((_var) |= ((_val) << HTT_DLPAGER_SYNC_LOCKED_PAGE_COUNT_S)); \
	} while (0)

#define HTT_DLPAGER_TOTAL_LOCKED_PAGES_GET(_var) \
	(((_var) & HTT_DLPAGER_TOTAL_LOCKED_PAGES_M) >> \
	HTT_DLPAGER_TOTAL_LOCKED_PAGES_S)

#define HTT_DLPAGER_TOTAL_LOCKED_PAGES_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_TOTAL_LOCKED_PAGES, _val); \
		((_var) &= ~(HTT_DLPAGER_TOTAL_LOCKED_PAGES_M)); \
		((_var) |= ((_val) << HTT_DLPAGER_TOTAL_LOCKED_PAGES_S)); \
	} while (0)

#define HTT_DLPAGER_TOTAL_FREE_PAGES_GET(_var) \
	(((_var) & HTT_DLPAGER_TOTAL_FREE_PAGES_M) >> \
	HTT_DLPAGER_TOTAL_FREE_PAGES_S)

#define HTT_DLPAGER_TOTAL_FREE_PAGES_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_TOTAL_FREE_PAGES, _val); \
		((_var) &= ~(HTT_DLPAGER_TOTAL_FREE_PAGES_M)); \
		((_var) |= ((_val) << HTT_DLPAGER_TOTAL_FREE_PAGES_S)); \
	} while (0)

#define HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_GET(_var) \
	(((_var) & HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_M) >> \
	HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_S)

#define HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_LAST_LOCKED_PAGE_IDX, _val); \
		((_var) &= ~(HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_M)); \
		((_var) |= ((_val) << HTT_DLPAGER_LAST_LOCKED_PAGE_IDX_S)); \
	} while (0)

#define HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_GET(_var) \
	(((_var) & HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_M) >> \
	HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_S)

#define HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX, _val); \
		((_var) &= ~(HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_M)); \
		((_var) |= ((_val) << HTT_DLPAGER_LAST_UNLOCKED_PAGE_IDX_S)); \
	} while (0)

struct htt_stats_error_tlv_v {
	u32 htt_stats_type;
};

#define ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS		4
#define ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS		8
#define ATH12K_HTT_TX_NUM_OFDMA_USER_STATS		74
#define ATH12K_HTT_TX_NUM_OFDMA_USER_STATS_TO_SHOW	37
#define ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS	    16
#define ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS		8

struct htt_tx_pdev_dl_mu_ofdma_sch_stats_tlv {
	u32 ax_mu_ofdma_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
};

struct htt_tx_pdev_ul_mu_ofdma_sch_stats_tlv {
	u32 ax_ul_mu_ofdma_basic_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	u32 ax_ul_mu_ofdma_bsr_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	u32 ax_ul_mu_ofdma_bar_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	u32 ax_ul_mu_ofdma_brp_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
};

struct htt_t2h_vdev_txrx_stats_hw_stats_tlv {
	__le32 vdev_id;
	__le32 rx_msdu_byte_cnt_hi;
	__le32 rx_msdu_byte_cnt_lo;
	__le32 rx_msdu_cnt_hi;
	__le32 rx_msdu_cnt_lo;
	__le32 tx_msdu_byte_cnt_hi;
	__le32 tx_msdu_byte_cnt_lo;
	__le32 tx_msdu_cnt_hi;
	__le32 tx_msdu_cnt_lo;
	__le32 tx_msdu_excessive_retry_discard_cnt_hi;
	__le32 tx_msdu_excessive_retry_discard_cnt_lo;
	__le32 tx_msdu_cong_ctrl_drop_cnt_hi;
	__le32 tx_msdu_cong_ctrl_drop_cnt_lo;
	__le32 tx_msdu_ttl_expire_drop_cnt_hi;
	__le32 tx_msdu_ttl_expire_drop_cnt_lo;
	__le32 tx_msdu_excessive_retry_discard_byte_cnt_lo;
	__le32 tx_msdu_excessive_retry_discard_byte_cnt_hi;
	__le32 tx_msdu_cong_ctrl_drop_byte_cnt_lo;
	__le32 tx_msdu_cong_ctrl_drop_byte_cnt_hi;
	__le32 tx_msdu_ttl_expire_drop_byte_cnt_lo;
	__le32 tx_msdu_ttl_expire_drop_byte_cnt_hi;
	__le32 tqm_bypass_frame_cnt_lo;
	__le32 tqm_bypass_frame_cnt_hi;
	__le32 tqm_bypass_byte_cnt_lo;
	__le32 tqm_bypass_byte_cnt_hi;
} __packed;

struct htt_tx_pdev_dl_mu_mimo_sch_stats_tlv {
	/* Number of MU MIMO schedules posted to HW */
	u32 mu_mimo_sch_posted;
	/* Number of MU MIMO schedules failed to post */
	u32 mu_mimo_sch_failed;
	/* Number of MU MIMO PPDUs posted to HW */
	u32 mu_mimo_ppdu_posted;
	/*
	 * This is the common description for the below sch stats.
	 * Counts the number of transmissions of each number of MU users
	 * in each TX mode.
	 * The array index is the "number of users - 1".
	 * For example, ac_mu_mimo_sch_nusers[1] counts the number of 11AC MU2
	 * TX PPDUs, ac_mu_mimo_sch_nusers[2] counts the number of 11AC MU3
	 * TX PPDUs and so on.
	 * The same is applicable for the other TX mode stats.
	 */
	/* Represents the count for 11AC DL MU MIMO sequences */
	u32 ac_mu_mimo_sch_nusers[ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS];
	/* Represents the count for 11AX DL MU MIMO sequences */
	u32 ax_mu_mimo_sch_nusers[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	/* Number of 11AC DL MU MIMO schedules posted per group size */
	u32 ac_mu_mimo_sch_posted_per_grp_sz[ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS];
	/* Number of 11AX DL MU MIMO schedules posted per group size */
	u32 ax_mu_mimo_sch_posted_per_grp_sz[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
};

struct htt_tx_pdev_ul_mu_mimo_sch_stats_tlv {
	/* Represents the count for 11AX UL MU MIMO sequences with Basic Triggers */
	u32 ax_ul_mu_mimo_basic_sch_nusers[ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS];
	/* Represents the count for 11AX UL MU MIMO sequences with BRP Triggers */
	u32 ax_ul_mu_mimo_brp_sch_nusers[ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS];
};

/* UL RESP Queues 0 - HIPRI, 1 - LOPRI & 2 - BSR */
#define HTT_STA_UL_OFDMA_NUM_UL_QUEUES 3

/* Actual resp type sent by STA for trigger
 * 0 - HE TB PPDU, 1 - NULL Delimiter
 */
#define HTT_STA_UL_OFDMA_NUM_RESP_END_TYPE 2

/* Counter for MCS 0-13 */
#define HTT_STA_UL_OFDMA_NUM_MCS_COUNTERS 14

/* Counters BW 20,40,80,160,320 */
#define HTT_STA_UL_OFDMA_NUM_BW_COUNTERS 5

/* 0 - Half, 1 - Quarter */
#define HTT_STA_UL_OFDMA_NUM_REDUCED_CHAN_TYPES 2

#define HTT_NUM_AC_WMM	0x4

enum HTT_STA_UL_OFDMA_RX_TRIG_TYPE {
	HTT_ULTRIG_QBOOST_TRIGGER = 0,
	HTT_ULTRIG_PSPOLL_TRIGGER,
	HTT_ULTRIG_UAPSD_TRIGGER,
	HTT_ULTRIG_11AX_TRIGGER,
	HTT_ULTRIG_11AX_WILDCARD_TRIGGER,
	HTT_ULTRIG_11AX_UNASSOC_WILDCARD_TRIGGER,
	HTT_STA_UL_OFDMA_NUM_TRIG_TYPE,
};

enum HTT_STA_UL_OFDMA_11AX_TRIG_TYPE {
	HTT_11AX_TRIGGER_BASIC_E		= 0,
	HTT_11AX_TRIGGER_BRPOLL_E		= 1,
	HTT_11AX_TRIGGER_MU_BAR_E		= 2,
	HTT_11AX_TRIGGER_MU_RTS_E		= 3,
	HTT_11AX_TRIGGER_BUFFER_SIZE_E		= 4,
	HTT_11AX_TRIGGER_GCR_MU_BAR_E		= 5,
	HTT_11AX_TRIGGER_BQRP_E			= 6,
	HTT_11AX_TRIGGER_NDP_FB_REPORT_POLL_E	= 7,
	HTT_11AX_TRIGGER_RESERVED_8_E		= 8,
	HTT_11AX_TRIGGER_RESERVED_9_E		= 9,
	HTT_11AX_TRIGGER_RESERVED_10_E		= 10,
	HTT_11AX_TRIGGER_RESERVED_11_E		= 11,
	HTT_11AX_TRIGGER_RESERVED_12_E		= 12,
	HTT_11AX_TRIGGER_RESERVED_13_E		= 13,
	HTT_11AX_TRIGGER_RESERVED_14_E		= 14,
	HTT_11AX_TRIGGER_RESERVED_15_E		= 15,
	HTT_STA_UL_OFDMA_NUM_11AX_TRIG_TYPE,
};

struct htt_print_sta_ul_ofdma_stats_tlv {
	u32 pdev_id;
	/* Trigger Type reported by HWSCH on RX reception
	 * Each index populate enum HTT_STA_UL_OFDMA_RX_TRIG_TYPE
	 */
	u32 rx_trigger_type[HTT_STA_UL_OFDMA_NUM_TRIG_TYPE];
	/* 11AX Trigger Type on RX reception
	 * Each index populate enum HTT_STA_UL_OFDMA_11AX_TRIG_TYPE
	 */
	u32 ax_trigger_type[HTT_STA_UL_OFDMA_NUM_11AX_TRIG_TYPE];
	/* Num data PPDUs/Delims responded to trigs. per HWQ for UL RESP */
	u32 num_data_ppdu_responded_per_hwq[HTT_STA_UL_OFDMA_NUM_UL_QUEUES];
	u32 num_null_delimiters_responded_per_hwq[HTT_STA_UL_OFDMA_NUM_UL_QUEUES];
	/* Overall UL STA RESP Status 0 - HE TB PPDU, 1 - NULL Delimiter
	 * Super set of num_data_ppdu_responded_per_hwq,
	 * num_null_delimiters_responded_per_hwq
	 */
	u32 num_total_trig_responses[HTT_STA_UL_OFDMA_NUM_RESP_END_TYPE];
	/* Time interval between current time ms and last successful trigger RX
	 * 0xFFFFFFFF denotes no trig received / timestamp roll back
	 */
	u32 last_trig_rx_time_delta_ms;
	/* Rate Statistics for UL OFDMA
	 * UL TB PPDU TX MCS, NSS, GI, BW from STA HWQ
	 */
	u32 ul_ofdma_tx_mcs[HTT_STA_UL_OFDMA_NUM_MCS_COUNTERS];
	u32 ul_ofdma_tx_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	u32 ul_ofdma_tx_gi[ATH12K_HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			  [HTT_STA_UL_OFDMA_NUM_MCS_COUNTERS];
	u32 ul_ofdma_tx_ldpc;
	u32 ul_ofdma_tx_bw[HTT_STA_UL_OFDMA_NUM_BW_COUNTERS];

	/* Trig based PPDU TX/ RBO based PPDU TX Count */
	u32 trig_based_ppdu_tx;
	u32 rbo_based_ppdu_tx;
	/* Switch MU EDCA to SU EDCA Count */
	u32 mu_edca_to_su_edca_switch_count;
	/* Num MU EDCA applied Count */
	u32 num_mu_edca_param_apply_count;

	/* Current MU EDCA Parameters for WMM ACs
	 * Mode - 0 - SU EDCA, 1- MU EDCA
	 */
	u32 current_edca_hwq_mode[HTT_NUM_AC_WMM];
	/* Contention Window minimum. Range: 1 - 10 */
	u32 current_cw_min[HTT_NUM_AC_WMM];
	/* Contention Window maximum. Range: 1 - 10 */
	u32 current_cw_max[HTT_NUM_AC_WMM];
	/* AIFS value - 0 -255 */
	u32 current_aifs[HTT_NUM_AC_WMM];
	u32 reduced_ul_ofdma_tx_bw[HTT_STA_UL_OFDMA_NUM_REDUCED_CHAN_TYPES]
				  [HTT_STA_UL_OFDMA_NUM_BW_COUNTERS];
};

#define ATH12K_HTT_NUM_AC_WMM				0x4
#define ATH12K_HTT_MAX_NUM_SBT_INTR			4
#define ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS		8
#define ATH12K_HTT_TX_NUM_BN_MUMIMO_USER_STATS		8
#define ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS	7
#define ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ		8
#define ATH12K_HTT_STATS_MUMIMO_TPUT_NUM_BINS		10

#define ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE \
	ATH12K_HTT_TX_MUMIMO_GRP_INVALID_MAX_REASON_CODE
#define ATH12K_HTT_TX_NUM_MUMIMO_GRP_INVALID_WORDS \
	(ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ * ATH12K_HTT_STATS_MAX_INVALID_REASON_CODE)

enum ath12k_htt_tx_pdev_stats_bn_dru_size {
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_26,
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_52,
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_106,
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_242,
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_484,
	ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_CNT = 5,
};

enum ath12k_htt_bn_ul_ofdma_ru_alloc_mode {
	ATH12K_HTT_BN_UL_OFDMA_RRU_ONLY_ALLOC_MODE,
	ATH12K_HTT_BN_UL_OFDMA_DRU_ONLY_ALLOC_MODE,
	ATH12K_HTT_BN_UL_OFDMA_HYBRID_RU_ALLOC_MODE,
	ATH12K_HTT_BN_UL_OFDMA_RU_ALLOC_MODE_RESERVED_1,
	ATH12K_HTT_BN_UL_OFDMA_RU_ALLOC_MODE_RESERVED_2,
	ATH12K_HTT_BN_UL_OFDMA_NUM_RU_ALLOC_MODES = 5,
};

enum ath12k_htt_bn_ul_ofdma_dru_sbw_size {
	ATH12K_HTT_BN_UL_OFDMA_DRU_SBW_20MHZ,
	ATH12K_HTT_BN_UL_OFDMA_DRU_SBW_40MHZ,
	ATH12K_HTT_BN_UL_OFDMA_DRU_SBW_80MHZ,
	ATH12K_HTT_BN_UL_OFDMA_NUM_DRU_SBW_COUNT = 3,
};

enum ath12k_htt_ru_alloc_mode {
	ATH12K_HTT_RU_ALLOC_MODE_PF,
	ATH12K_HTT_RU_ALLOC_MODE_QOS,
	ATH12K_HTT_RU_ALLOC_MODE_STATIC,
	ATH12K_HTT_RU_ALLOC_MODE_EQUAL,
	ATH12K_HTT_RU_ALLOC_MODE_SIMPLIFIED,
	/* Reserving additional modes for future use */
	ATH12K_HTT_RU_ALLOC_MODE_RESERVED_1,
	ATH12K_HTT_RU_ALLOC_MODE_RESERVED_2,
	ATH12K_HTT_RU_ALLOC_NUM_MODES,
};

struct ath12k_htt_tx_selfgen_cmn_stats_tlv {
	__le32 mac_id__word;
	__le32 su_bar;
	__le32 rts;
	__le32 cts2self;
	__le32 qos_null;
	__le32 delayed_bar_1;
	__le32 delayed_bar_2;
	__le32 delayed_bar_3;
	__le32 delayed_bar_4;
	__le32 delayed_bar_5;
	__le32 delayed_bar_6;
	__le32 delayed_bar_7;
	__le32 bar_with_tqm_head_seq_num;
	__le32 bar_with_tid_seq_num;
	__le32 su_sw_rts_queued;
	__le32 su_sw_rts_tried;
	__le32 su_sw_rts_err;
	__le32 su_sw_rts_flushed;
	__le32 su_sw_rts_rcvd_cts_diff_bw;
	__le32 combined_ax_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_ax_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_ax_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_ax_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 smart_basic_trig_sch_histogram[ATH12K_HTT_MAX_NUM_SBT_INTR];
	__le32 ru_alloc_mode_cnt[ATH12K_HTT_RU_ALLOC_NUM_MODES];
	__le32 mu_bar_pipeline_seq_cnt;
	__le32 mu_bar_pipeline_resume_cnt;
	__le32 mu_bar_pipeline_resume_fail_cnt;
} __packed;

struct ath12k_htt_tx_selfgen_ac_stats_tlv {
	__le32 ac_su_ndpa;
	__le32 ac_su_ndp;
	__le32 ac_mu_mimo_ndpa;
	__le32 ac_mu_mimo_ndp;
	__le32 ac_mu_mimo_brpoll_1;
	__le32 ac_mu_mimo_brpoll_2;
	__le32 ac_mu_mimo_brpoll_3;
	__le32 ac_su_ndpa_queued;
	__le32 ac_su_ndp_queued;
	__le32 ac_mu_mimo_ndpa_queued;
	__le32 ac_mu_mimo_ndp_queued;
	__le32 ac_mu_mimo_brpoll_1_queued;
	__le32 ac_mu_mimo_brpoll_2_queued;
	__le32 ac_mu_mimo_brpoll_3_queued;
} __packed;

struct ath12k_htt_tx_selfgen_ax_stats_tlv {
	__le32 ax_su_ndpa;
	__le32 ax_su_ndp;
	__le32 ax_mu_mimo_ndpa;
	__le32 ax_mu_mimo_ndp;
	__le32 ax_mu_mimo_brpoll[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1];
	__le32 ax_basic_trigger;
	__le32 ax_bsr_trigger;
	__le32 ax_mu_bar_trigger;
	__le32 ax_mu_rts_trigger;
	__le32 ax_ulmumimo_trigger;
	__le32 ax_su_ndpa_queued;
	__le32 ax_su_ndp_queued;
	__le32 ax_mu_mimo_ndpa_queued;
	__le32 ax_mu_mimo_ndp_queued;
	__le32 ax_mu_mimo_brpoll_queued[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1];
	__le32 ax_ul_mumimo_trigger[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 combined_ax_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_ax_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_ax_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_ax_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_ax_su_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_ax_su_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_ax_mu_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_ax_mu_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 ax_basic_trigger_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 ax_basic_trigger_errors_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 ax_mu_bar_trigger_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 ax_mu_bar_trigger_errors_per_ac[ATH12K_HTT_NUM_AC_WMM];
} __packed;

struct ath12k_htt_tx_selfgen_be_stats_tlv {
	__le32 be_su_ndpa;
	__le32 be_su_ndp;
	__le32 be_mu_mimo_ndpa;
	__le32 be_mu_mimo_ndp;
	__le32 be_mu_mimo_brpoll[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1];
	__le32 be_basic_trigger;
	__le32 be_bsr_trigger;
	__le32 be_mu_bar_trigger;
	__le32 be_mu_rts_trigger;
	__le32 be_ulmumimo_trigger;
	__le32 be_su_ndpa_queued;
	__le32 be_su_ndp_queued;
	__le32 be_mu_mimo_ndpa_queued;
	__le32 be_mu_mimo_ndp_queued;
	__le32 be_mu_mimo_brpoll_queued[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1];
	__le32 be_ul_mumimo_trigger[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 combined_be_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_be_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_be_su_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_be_su_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_be_mu_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_be_mu_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 be_basic_trigger_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 be_basic_trigger_errors_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 be_mu_bar_trigger_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 be_mu_bar_trigger_errors_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_be_ulofdma_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_be_ulofdma_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_ulofdma_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_ulofdma_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_be_ulmumimo_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_be_ulmumimo_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_ulmumimo_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_be_ulmumimo_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 wifi_version;
} __packed;

struct ath12k_htt_tx_selfgen_bn_stats_tlv {
	__le32 bn_basic_trigger;
	__le32 bn_bsr_trigger;
	__le32 bn_mu_bar_trigger;
	__le32 bn_mu_rts_trigger;

	__le32 combined_bn_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_bn_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_bn_bsr_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_bn_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_bn_su_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_bn_su_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_bn_mu_ulofdma_basic_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 manual_bn_mu_ulofdma_basic_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 bn_basic_trig_ru_alloc_mode[ATH12K_HTT_BN_UL_OFDMA_NUM_RU_ALLOC_MODES];

	__le32 bn_ulmumimo_trigger;
	__le32 bn_ul_mumimo_trigger[ATH12K_HTT_TX_NUM_BN_MUMIMO_USER_STATS];
	__le32 combined_bn_ulmumimo_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 combined_bn_ulmumimo_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_bn_ulmumimo_trigger_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 standalone_bn_ulmumimo_trigger_err[ATH12K_HTT_NUM_AC_WMM];

	__le32 sta_dps_bn_bsr_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 sta_dps_bn_mu_rts_trigger[ATH12K_HTT_NUM_AC_WMM];
	__le32 sta_dps_bn_bsr_trigger_err[ATH12K_HTT_NUM_AC_WMM];
	__le32 sta_dps_bn_mu_rts_trigger_err[ATH12K_HTT_NUM_AC_WMM];

	__le32 bn_basic_trig_ru_alloc_mode_err[ATH12K_HTT_BN_UL_OFDMA_NUM_RU_ALLOC_MODES];

} __packed;

struct ath12k_htt_tx_selfgen_ac_err_stats_tlv {
	__le32 ac_su_ndp_err;
	__le32 ac_su_ndpa_err;
	__le32 ac_mu_mimo_ndpa_err;
	__le32 ac_mu_mimo_ndp_err;
	__le32 ac_mu_mimo_brp1_err;
	__le32 ac_mu_mimo_brp2_err;
	__le32 ac_mu_mimo_brp3_err;
	__le32 ac_su_ndp_flushed;
	__le32 ac_su_ndpa_flushed;
	__le32 ac_mu_mimo_ndpa_flushed;
	__le32 ac_mu_mimo_ndp_flushed;
	__le32 ac_mu_mimo_brp1_flushed;
	__le32 ac_mu_mimo_brp2_flushed;
	__le32 ac_mu_mimo_brp3_flushed;
} __packed;

struct ath12k_htt_tx_selfgen_ax_err_stats_tlv {
	__le32 ax_su_ndp_err;
	__le32 ax_su_ndpa_err;
	__le32 ax_mu_mimo_ndpa_err;
	__le32 ax_mu_mimo_ndp_err;
	__le32 ax_mu_mimo_brp_err[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1];
	__le32 ax_basic_trigger_err;
	__le32 ax_bsr_trigger_err;
	__le32 ax_mu_bar_trigger_err;
	__le32 ax_mu_rts_trigger_err;
	__le32 ax_ulmumimo_trigger_err;
	__le32 ax_mu_mimo_brp_err_num_cbf_received[
		ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 ax_su_ndpa_flushed;
	__le32 ax_su_ndp_flushed;
	__le32 ax_mu_mimo_ndpa_flushed;
	__le32 ax_mu_mimo_ndp_flushed;
	__le32 ax_mu_mimo_brpoll_flushed[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS - 1];
	__le32 ax_ul_mumimo_trigger_err[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 ax_basic_trigger_partial_resp;
	__le32 ax_bsr_trigger_partial_resp;
	__le32 ax_mu_bar_trigger_partial_resp;
} __packed;

struct ath12k_htt_tx_selfgen_be_err_stats_tlv {
	__le32 be_su_ndp_err;
	__le32 be_su_ndpa_err;
	__le32 be_mu_mimo_ndpa_err;
	__le32 be_mu_mimo_ndp_err;
	__le32 be_mu_mimo_brp_err[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1];
	__le32 be_basic_trigger_err;
	__le32 be_bsr_trigger_err;
	__le32 be_mu_bar_trigger_err;
	__le32 be_mu_rts_trigger_err;
	__le32 be_ul_mumimo_total_trigger_err;
	__le32 be_mu_mimo_brp_err_num_cbf_rxd[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 be_su_ndpa_flushed;
	__le32 be_su_ndp_flushed;
	__le32 be_mu_mimo_ndpa_flushed;
	__le32 be_mu_mimo_ndp_flushed;
	__le32 be_mu_mimo_brpoll_flushed[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS - 1];
	__le32 be_ul_mumimo_trigger_err[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 be_basic_trigger_partial_resp;
	__le32 be_bsr_trigger_partial_resp;
	__le32 be_mu_bar_trigger_partial_resp;
	__le32 be_mu_rts_trigger_blocked;
	__le32 be_bsr_trigger_blocked;
	__le32 wifi_version;
} __packed;

struct ath12k_htt_tx_selfgen_bn_err_stats_tlv {
	__le32 bn_basic_trigger_err;
	__le32 bn_bsr_trigger_err;
	__le32 bn_mu_bar_trigger_err;
	__le32 bn_mu_rts_trigger_err;

	__le32 bn_basic_trigger_partial_resp;
	__le32 bn_bsr_trigger_partial_resp;
	__le32 bn_mu_bar_trigger_partial_resp;
	__le32 bn_mu_rts_trigger_blocked;
	__le32 bn_bsr_trigger_blocked;
	__le32 bn_ul_mumimo_total_trigger_err;
	__le32 bn_ul_mumimo_trigger_err[ATH12K_HTT_TX_NUM_BN_MUMIMO_USER_STATS];
} __packed;

enum ath12k_htt_tx_selfgen_sch_tsflag_error_stats {
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_FLUSH_RCVD_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_FILT_SCHED_CMD_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_RESP_MISMATCH_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_RESP_CBF_MIMO_CTRL_MISMATCH_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_RESP_CBF_BW_MISMATCH_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_RETRY_COUNT_FAIL_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_RESP_TOO_LATE_RECEIVED_ERR,
	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_SIFS_STALL_NO_NEXT_CMD_ERR,

	ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS
};

struct ath12k_htt_tx_selfgen_ac_sched_status_stats_tlv {
	__le32 ac_su_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ac_su_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ac_su_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ac_mu_mimo_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ac_mu_mimo_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ac_mu_mimo_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ac_mu_mimo_brp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ac_mu_mimo_brp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
} __packed;

struct ath12k_htt_tx_selfgen_ax_sched_status_stats_tlv {
	__le32 ax_su_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_su_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_su_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ax_mu_mimo_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_mu_mimo_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_mu_mimo_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ax_mu_brp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_mu_brp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ax_mu_bar_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_mu_bar_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ax_basic_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_basic_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 ax_ulmumimo_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 ax_ulmumimo_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
} __packed;

struct ath12k_htt_tx_selfgen_be_sched_status_stats_tlv {
	__le32 be_su_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_su_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_su_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 be_mu_mimo_ndpa_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_mu_mimo_ndp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_mu_mimo_ndp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 be_mu_brp_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_mu_brp_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 be_mu_bar_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_mu_bar_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 be_basic_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_basic_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 be_ulmumimo_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 be_ulmumimo_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 wifi_version;
} __packed;

struct ath12k_htt_tx_selfgen_bn_sched_status_stats_tlv {
	__le32 bn_mu_bar_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 bn_mu_bar_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 bn_basic_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 bn_basic_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
	__le32 bn_ulmumimo_trig_sch_status[ATH12K_HTT_TX_PDEV_STATS_NUM_TX_ERR_STATUS];
	__le32 bn_ulmumimo_trig_sch_flag_err[ATH12K_HTT_TX_SELFGEN_SCH_TSFLAG_ERR_STATS];
} __packed;

struct ath12k_htt_tx_pdev_be_dl_mu_ofdma_sch_stats_tlv {
	__le32 be_mu_ofdma_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
} __packed;

struct ath12k_htt_tx_pdev_be_ul_mu_ofdma_sch_stats_tlv {
	__le32 be_ul_mu_ofdma_basic_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 be_ul_mu_ofdma_bsr_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 be_ul_mu_ofdma_bar_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 be_ul_mu_ofdma_brp_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
} __packed;

struct ath12k_htt_tx_pdev_bn_dl_mu_ofdma_sch_stats_tlv {
	__le32 bn_mu_ofdma_sch_nusers[ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS];
} __packed;

struct ath12k_htt_tx_pdev_bn_ul_mu_ofdma_sch_stats_tlv {
	__le32 bn_ul_mu_ofdma_basic_sch_nusers[ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS];
	__le32 bn_ul_mu_ofdma_bsr_sch_nusers[ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS];
	__le32 bn_ul_mu_ofdma_bar_sch_nusers[ATH12K_HTT_TX_NUM_BN_OFDMA_USER_STATS];
} __packed;

struct ath12k_htt_stats_string_tlv {
	DECLARE_FLEX_ARRAY(__le32, data);
} __packed;

#define ATH12K_HTT_SRING_STATS_MAC_ID                  GENMASK(7, 0)
#define ATH12K_HTT_SRING_STATS_RING_ID                 GENMASK(15, 8)
#define ATH12K_HTT_SRING_STATS_ARENA                   GENMASK(23, 16)
#define ATH12K_HTT_SRING_STATS_EP                      BIT(24)
#define ATH12K_HTT_SRING_STATS_NUM_AVAIL_WORDS         GENMASK(15, 0)
#define ATH12K_HTT_SRING_STATS_NUM_VALID_WORDS         GENMASK(31, 16)
#define ATH12K_HTT_SRING_STATS_HEAD_PTR                GENMASK(15, 0)
#define ATH12K_HTT_SRING_STATS_TAIL_PTR                GENMASK(31, 16)
#define ATH12K_HTT_SRING_STATS_CONSUMER_EMPTY          GENMASK(15, 0)
#define ATH12K_HTT_SRING_STATS_PRODUCER_FULL           GENMASK(31, 16)
#define ATH12K_HTT_SRING_STATS_PREFETCH_COUNT          GENMASK(15, 0)
#define ATH12K_HTT_SRING_STATS_INTERNAL_TAIL_PTR       GENMASK(31, 16)

struct ath12k_htt_sring_stats_tlv {
	__le32 mac_id__ring_id__arena__ep;
	__le32 base_addr_lsb;
	__le32 base_addr_msb;
	__le32 ring_size;
	__le32 elem_size;
	__le32 num_avail_words__num_valid_words;
	__le32 head_ptr__tail_ptr;
	__le32 consumer_empty__producer_full;
	__le32 prefetch_count__internal_tail_ptr;
} __packed;

struct ath12k_htt_sfm_cmn_tlv {
	__le32 mac_id__word;
	__le32 buf_total;
	__le32 mem_empty;
	__le32 deallocate_bufs;
	__le32 num_records;
} __packed;

struct ath12k_htt_sfm_client_tlv {
	__le32 client_id;
	__le32 buf_min;
	__le32 buf_max;
	__le32 buf_busy;
	__le32 buf_alloc;
	__le32 buf_avail;
	__le32 num_users;
} __packed;

struct ath12k_htt_sfm_client_user_tlv {
	DECLARE_FLEX_ARRAY(__le32, dwords_used_by_user_n);
} __packed;

struct ath12k_htt_tx_pdev_mu_mimo_sch_stats_tlv {
	__le32 mu_mimo_sch_posted;
	__le32 mu_mimo_sch_failed;
	__le32 mu_mimo_ppdu_posted;
	__le32 ac_mu_mimo_sch_nusers[ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS];
	__le32 ax_mu_mimo_sch_nusers[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 ax_ofdma_sch_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 ax_ul_ofdma_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 ax_ul_ofdma_bsr_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 ax_ul_ofdma_bar_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 ax_ul_ofdma_brp_nusers[ATH12K_HTT_TX_NUM_OFDMA_USER_STATS];
	__le32 ax_ul_mumimo_nusers[ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS];
	__le32 ax_ul_mumimo_brp_nusers[ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS];
	__le32 ac_mu_mimo_per_grp_sz[ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS];
	__le32 ax_mu_mimo_per_grp_sz[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 be_mu_mimo_sch_nusers[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 be_mu_mimo_per_grp_sz[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 ac_mu_mimo_grp_sz_ext[ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS];
	__le32 wifi_version;
} __packed;

struct ath12k_htt_tx_pdev_mumimo_grp_stats_tlv {
	__le32 dl_mumimo_grp_best_grp_size[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 dl_mumimo_grp_best_num_usrs[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 dl_mumimo_grp_eligible[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 dl_mumimo_grp_ineligible[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 dl_mumimo_grp_invalid[ATH12K_HTT_TX_NUM_MUMIMO_GRP_INVALID_WORDS];
	__le32 dl_mumimo_grp_tputs[ATH12K_HTT_STATS_MUMIMO_TPUT_NUM_BINS];
	__le32 ul_mumimo_grp_best_grp_size[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 ul_mumimo_grp_best_usrs[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS];
	__le32 ul_mumimo_grp_tputs[ATH12K_HTT_STATS_MUMIMO_TPUT_NUM_BINS];
} __packed;

enum ath12k_htt_stats_tx_sched_modes {
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_AC = 1,
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_AX,
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_AX,
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_BE,
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_MIMO_BE,
	ATH12K_HTT_STATS_TX_SCHED_MODE_MU_OFDMA_BN
};

struct ath12k_htt_tx_pdev_mpdu_stats_tlv {
	__le32 mpdus_queued_usr;
	__le32 mpdus_tried_usr;
	__le32 mpdus_failed_usr;
	__le32 mpdus_requeued_usr;
	__le32 err_no_ba_usr;
	__le32 mpdu_underrun_usr;
	__le32 ampdu_underrun_usr;
	__le32 user_index;
	__le32 tx_sched_mode;
	__le32 wifi_version;
} __packed;

struct ath12k_htt_pdev_stats_cca_counters_tlv {
	__le32 tx_frame_usec;
	__le32 rx_frame_usec;
	__le32 rx_clear_usec;
	__le32 my_rx_frame_usec;
	__le32 usec_cnt;
	__le32 med_rx_idle_usec;
	__le32 med_tx_idle_global_usec;
	__le32 cca_obss_usec;
	__le32 pre_rx_frame_usec;
} __packed;

struct ath12k_htt_pdev_cca_stats_hist_v1_tlv {
	__le32 chan_num;
	__le32 num_records;
	__le32 valid_cca_counters_bitmap;
	__le32 collection_interval;
} __packed;

#define ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS		8
#define ATH12K_HTT_TX_NUM_AC_MUMIMO_USER_STATS		4
#define ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS          8
#define ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS		8
#define ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS	4
#define ATH12K_HTT_TX_NUM_MCS_CNTRS			12
#define ATH12K_HTT_TX_NUM_EXTRA_MCS_CNTRS		2

#define ATH12K_HTT_MAX_MUMIMO_USERS 4

#define ATH12K_HTT_TX_NUM_OF_SOUNDING_STATS_WORDS \
	(ATH12K_HTT_TX_PDEV_STATS_NUM_BW_COUNTERS * \
	 ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS)

enum ath12k_htt_txbf_sound_steer_modes {
	ATH12K_HTT_IMPL_STEER_STATS		= 0,
	ATH12K_HTT_EXPL_SUSIFS_STEER_STATS	= 1,
	ATH12K_HTT_EXPL_SURBO_STEER_STATS	= 2,
	ATH12K_HTT_EXPL_MUSIFS_STEER_STATS	= 3,
	ATH12K_HTT_EXPL_MURBO_STEER_STATS	= 4,
	ATH12K_HTT_TXBF_MAX_NUM_OF_MODES	= 5
};

enum ath12k_htt_stats_sounding_tx_mode {
	ATH12K_HTT_TX_AC_SOUNDING_MODE		= 0,
	ATH12K_HTT_TX_AX_SOUNDING_MODE		= 1,
	ATH12K_HTT_TX_BE_SOUNDING_MODE		= 2,
	ATH12K_HTT_TX_CMN_SOUNDING_MODE		= 3,
	ATH12K_HTT_TX_CV_CORR_MODE		= 4,
};

struct ath12k_htt_tx_sounding_stats_tlv {
	__le32 tx_sounding_mode;
	__le32 cbf_20[ATH12K_HTT_TXBF_MAX_NUM_OF_MODES];
	__le32 cbf_40[ATH12K_HTT_TXBF_MAX_NUM_OF_MODES];
	__le32 cbf_80[ATH12K_HTT_TXBF_MAX_NUM_OF_MODES];
	__le32 cbf_160[ATH12K_HTT_TXBF_MAX_NUM_OF_MODES];
	__le32 sounding[ATH12K_HTT_TX_NUM_OF_SOUNDING_STATS_WORDS];
	__le32 cv_nc_mismatch_err;
	__le32 cv_fcs_err;
	__le32 cv_frag_idx_mismatch;
	__le32 cv_invalid_peer_id;
	__le32 cv_no_txbf_setup;
	__le32 cv_expiry_in_update;
	__le32 cv_pkt_bw_exceed;
	__le32 cv_dma_not_done_err;
	__le32 cv_update_failed;
	__le32 cv_total_query;
	__le32 cv_total_pattern_query;
	__le32 cv_total_bw_query;
	__le32 cv_invalid_bw_coding;
	__le32 cv_forced_sounding;
	__le32 cv_standalone_sounding;
	__le32 cv_nc_mismatch;
	__le32 cv_fb_type_mismatch;
	__le32 cv_ofdma_bw_mismatch;
	__le32 cv_bw_mismatch;
	__le32 cv_pattern_mismatch;
	__le32 cv_preamble_mismatch;
	__le32 cv_nr_mismatch;
	__le32 cv_in_use_cnt_exceeded;
	__le32 cv_found;
	__le32 cv_not_found;
	__le32 sounding_320[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 cbf_320[ATH12K_HTT_TXBF_MAX_NUM_OF_MODES];
	__le32 cv_ntbr_sounding;
	__le32 cv_found_upload_in_progress;
	__le32 cv_expired_during_query;
	__le32 cv_dma_timeout_error;
	__le32 cv_buf_ibf_uploads;
	__le32 cv_buf_ebf_uploads;
	__le32 cv_buf_received;
	__le32 cv_buf_fed_back;
	__le32 cv_total_query_ibf;
	__le32 cv_found_ibf;
	__le32 cv_not_found_ibf;
	__le32 cv_expired_during_query_ibf;
	__le32 adaptive_snd_total_query;
	__le32 adaptive_snd_total_mcs_drop[ATH12K_HTT_TX_PDEV_STATS_TOTAL_MCS_COUNTERS];
	__le32 adaptive_snd_kicked_in;
	__le32 adaptive_snd_back_to_def;
	__le32 cv_corr_trig_online_mode;
	__le32 cv_corr_trig_offline_mode;
	__le32 cv_corr_trig_hybrid_mode;
	__le32 cv_corr_trig_comp_level_0;
	__le32 cv_corr_trig_comp_level_1;
	__le32 cv_corr_trig_comp_level_2;
	__le32 cv_corr_trigger_num_users[ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS];
	__le32 cv_corr_trigger_num_streams[ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS];
	__le32 total_buf_received;
	__le32 total_buf_fed_back;
	__le32 total_processing_failed;
	__le32 total_users_zero;
	__le32 failed_tot_users_exceeded;
	__le32 failed_peer_not_found;
	__le32 user_nss_exceeded;
	__le32 invalid_lookup_index;
	__le32 total_num_users[ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS];
	__le32 total_num_streams[ATH12K_HTT_TX_CV_CORR_MAX_NUM_COLUMNS];
	__le32 lookahead_sounding_dl_cnt;
	__le32 lookahead_snd_dl_num_users[ATH12K_HTT_TX_NUM_BE_MUMIMO_USER_STATS];
	__le32 lookahead_sounding_ul_cnt;
	__le32 lookahead_snd_ul_num_users[ATH12K_HTT_TX_NUM_UL_MUMIMO_USER_STATS];
} __packed;

struct ath12k_htt_pdev_obss_pd_stats_tlv {
	__le32 num_obss_tx_ppdu_success;
	__le32 num_obss_tx_ppdu_failure;
	__le32 num_sr_tx_transmissions;
	__le32 num_spatial_reuse_opportunities;
	__le32 num_non_srg_opportunities;
	__le32 num_non_srg_ppdu_tried;
	__le32 num_non_srg_ppdu_success;
	__le32 num_srg_opportunities;
	__le32 num_srg_ppdu_tried;
	__le32 num_srg_ppdu_success;
	__le32 num_psr_opportunities;
	__le32 num_psr_ppdu_tried;
	__le32 num_psr_ppdu_success;
	__le32 num_non_srg_tried_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 num_non_srg_success_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 num_srg_tried_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 num_srg_success_per_ac[ATH12K_HTT_NUM_AC_WMM];
	__le32 num_obss_min_dur_check_flush_cnt;
	__le32 num_sr_ppdu_abort_flush_cnt;
} __packed;

struct ath12k_htt_ring_backpressure_stats_tlv {
	u32 pdev_id;
	u32 current_head_idx;
	u32 current_tail_idx;
	u32 num_htt_msgs_sent;
	/* Time in milliseconds for which the ring has been in
	 * its current backpressure condition
	 */
	u32 backpressure_time_ms;
	/* backpressure_hist - histogram showing how many times
	 * different degrees of backpressure duration occurred:
	 * Index 0 indicates the number of times ring was
	 * continuously in backpressure state for 100 - 200ms.
	 * Index 1 indicates the number of times ring was
	 * continuously in backpressure state for 200 - 300ms.
	 * Index 2 indicates the number of times ring was
	 * continuously in backpressure state for 300 - 400ms.
	 * Index 3 indicates the number of times ring was
	 * continuously in backpressure state for 400 - 500ms.
	 * Index 4 indicates the number of times ring was
	 * continuously in backpressure state beyond 500ms.
	 */
	u32 backpressure_hist[5];
};

#define ATH12K_HTT_STATS_MAX_PROF_STATS_NAME_LEN	32
#define ATH12K_HTT_LATENCY_PROFILE_NUM_MAX_HIST		3
#define ATH12K_HTT_INTERRUPTS_LATENCY_PROFILE_MAX_HIST	3

struct ath12k_htt_latency_prof_stats_tlv {
	__le32 print_header;
	s8 latency_prof_name[ATH12K_HTT_STATS_MAX_PROF_STATS_NAME_LEN];
	__le32 cnt;
	__le32 min;
	__le32 max;
	__le32 last;
	__le32 tot;
	__le32 avg;
	__le32 hist_intvl;
	__le32 hist[ATH12K_HTT_LATENCY_PROFILE_NUM_MAX_HIST];
	__le32 page_fault_max;
	__le32 page_fault_total;
	__le32 ignored_latency_count;
	__le32 interrupts_max;
	__le32 interrupts_hist[ATH12K_HTT_INTERRUPTS_LATENCY_PROFILE_MAX_HIST];
	__le32 min_pcycles_time;
	__le32 max_pcycles_time;
	__le32 total_pcycles_time;
	__le32 avg_pcycles_time;
}  __packed;

struct ath12k_htt_latency_prof_ctx_tlv {
	__le32 duration;
	__le32 tx_msdu_cnt;
	__le32 tx_mpdu_cnt;
	__le32 tx_ppdu_cnt;
	__le32 rx_msdu_cnt;
	__le32 rx_mpdu_cnt;
} __packed;

struct ath12k_htt_latency_prof_cnt_tlv {
	__le32 prof_enable_cnt;
} __packed;

#define ATH12K_HTT_RX_NUM_MCS_CNTRS		12
#define ATH12K_HTT_RX_NUM_GI_CNTRS		4
#define ATH12K_HTT_RX_NUM_SPATIAL_STREAMS	8
#define ATH12K_HTT_RX_NUM_BW_CNTRS		4
#define ATH12K_HTT_RX_NUM_RU_SIZE_CNTRS		6
#define ATH12K_HTT_RX_NUM_RU_SIZE_160MHZ_CNTRS	7
#define ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK	5
#define ATH12K_HTT_RX_NUM_REDUCED_CHAN_TYPES	2
#define ATH12K_HTT_RX_NUM_EXTRA_MCS_CNTRS	2
#define ATH12K_HTT_RX_PDEV_STATS_TOTAL_BW_COUNTERS \
	 (ATH12K_HTT_RX_PDEV_STATS_NUM_BW_EXT_COUNTERS \
	  + ATH12K_HTT_RX_PDEV_STATS_NUM_BW_COUNTERS)
#define ATH12K_HTT_NUM_TCP_IMPLICIT_TRIG_INTR	12
#define ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS 12
#define ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_INTERVAL_US 1000

struct ath12k_htt_rx_pdev_ul_ofdma_user_stats_tlv {
	__le32 user_index;
	__le32 rx_ulofdma_non_data_ppdu;
	__le32 rx_ulofdma_data_ppdu;
	__le32 rx_ulofdma_mpdu_ok;
	__le32 rx_ulofdma_mpdu_fail;
	__le32 rx_ulofdma_non_data_nusers;
	__le32 rx_ulofdma_data_nusers;
} __packed;

struct ath12k_htt_rx_pdev_ul_trigger_stats_tlv {
	__le32 mac_id__word;
	__le32 rx_11ax_ul_ofdma;
	__le32 ul_ofdma_rx_mcs[ATH12K_HTT_RX_NUM_MCS_CNTRS];
	__le32 ul_ofdma_rx_gi[ATH12K_HTT_RX_NUM_GI_CNTRS][ATH12K_HTT_RX_NUM_MCS_CNTRS];
	__le32 ul_ofdma_rx_nss[ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	__le32 ul_ofdma_rx_bw[ATH12K_HTT_RX_NUM_BW_CNTRS];
	__le32 ul_ofdma_rx_stbc;
	__le32 ul_ofdma_rx_ldpc;
	__le32 data_ru_size_ppdu[ATH12K_HTT_RX_NUM_RU_SIZE_160MHZ_CNTRS];
	__le32 non_data_ru_size_ppdu[ATH12K_HTT_RX_NUM_RU_SIZE_160MHZ_CNTRS];
	__le32 uplink_sta_aid[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 uplink_sta_target_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 uplink_sta_fd_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 uplink_sta_power_headroom[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 red_bw[ATH12K_HTT_RX_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_RX_NUM_BW_CNTRS];
	__le32 ul_ofdma_bsc_trig_rx_qos_null_only;
	__le32 tcp_aware_implicit_trig_hist_ms[ATH12K_HTT_NUM_TCP_IMPLICIT_TRIG_INTR];
	__le32 ulofdma_implicit_trig_tried;
	__le32 ulofdma_implicit_trig_qos_null;
	__le32 ul_ofdma_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
} __packed;

#define ATH12K_HTT_TX_UL_MUMIMO_USER_STATS	8

struct ath12k_htt_rx_ul_mumimo_trig_stats_tlv {
	__le32 mac_id__word;
	__le32 rx_11ax_ul_mumimo;
	__le32 ul_mumimo_rx_mcs[ATH12K_HTT_RX_NUM_MCS_CNTRS];
	__le32 ul_rx_gi[ATH12K_HTT_RX_NUM_GI_CNTRS][ATH12K_HTT_RX_NUM_MCS_CNTRS];
	__le32 ul_mumimo_rx_nss[ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	__le32 ul_mumimo_rx_bw[ATH12K_HTT_RX_NUM_BW_CNTRS];
	__le32 ul_mumimo_rx_stbc;
	__le32 ul_mumimo_rx_ldpc;
	__le32 ul_mumimo_rx_mcs_ext[ATH12K_HTT_RX_NUM_EXTRA_MCS_CNTRS];
	__le32 ul_gi_ext[ATH12K_HTT_RX_NUM_GI_CNTRS][ATH12K_HTT_RX_NUM_EXTRA_MCS_CNTRS];
	s8 ul_rssi[ATH12K_HTT_RX_NUM_SPATIAL_STREAMS]
		  [ATH12K_HTT_RX_PDEV_STATS_TOTAL_BW_COUNTERS];
	s8 tgt_rssi[ATH12K_HTT_TX_UL_MUMIMO_USER_STATS][ATH12K_HTT_RX_NUM_BW_CNTRS];
	s8 fd[ATH12K_HTT_TX_UL_MUMIMO_USER_STATS][ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	s8 db[ATH12K_HTT_TX_UL_MUMIMO_USER_STATS][ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	__le32 red_bw[ATH12K_HTT_RX_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_RX_NUM_BW_CNTRS];
	__le32 mumimo_bsc_trig_rx_qos_null_only;
	__le32 ul_mimo_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
} __packed;

#define ATH12K_HTT_RX_NUM_MAX_PEAK_OCCUPANCY_INDEX	10
#define ATH12K_HTT_RX_NUM_MAX_CURR_OCCUPANCY_INDEX	10
#define ATH12K_HTT_RX_NUM_SQUARE_INDEX			6
#define ATH12K_HTT_RX_NUM_MAX_PEAK_SEARCH_INDEX		4
#define ATH12K_HTT_RX_NUM_MAX_PENDING_SEARCH_INDEX	4

struct ath12k_htt_rx_fse_stats_tlv {
	__le32 fse_enable_cnt;
	__le32 fse_disable_cnt;
	__le32 fse_cache_invalidate_entry_cnt;
	__le32 fse_full_cache_invalidate_cnt;
	__le32 fse_num_cache_hits_cnt;
	__le32 fse_num_searches_cnt;
	__le32 fse_cache_occupancy_peak_cnt[ATH12K_HTT_RX_NUM_MAX_PEAK_OCCUPANCY_INDEX];
	__le32 fse_cache_occupancy_curr_cnt[ATH12K_HTT_RX_NUM_MAX_CURR_OCCUPANCY_INDEX];
	__le32 fse_search_stat_square_cnt[ATH12K_HTT_RX_NUM_SQUARE_INDEX];
	__le32 fse_search_stat_peak_cnt[ATH12K_HTT_RX_NUM_MAX_PEAK_SEARCH_INDEX];
	__le32 fse_search_stat_pending_cnt[ATH12K_HTT_RX_NUM_MAX_PENDING_SEARCH_INDEX];
} __packed;

#define ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS		14
#define ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS		8
#define ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS		8
#define ATH12K_HTT_TXBF_NUM_BW_CNTRS				5
#define ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES			2

struct ath12k_htt_pdev_txrate_txbf_stats_tlv {
	__le32 tx_su_txbf_mcs[ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS];
	__le32 tx_su_ibf_mcs[ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS];
	__le32 tx_su_ol_mcs[ATH12K_HTT_TX_BF_RATE_STATS_NUM_MCS_COUNTERS];
	__le32 tx_su_txbf_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 tx_su_ibf_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 tx_su_ol_nss[ATH12K_HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	__le32 tx_su_txbf_bw[ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 tx_su_ibf_bw[ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 tx_su_ol_bw[ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 tx_legacy_ofdm_rate[ATH12K_HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS];
	__le32 txbf[ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 ibf[ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 ol[ATH12K_HTT_TXBF_NUM_REDUCED_CHAN_TYPES][ATH12K_HTT_TXBF_NUM_BW_CNTRS];
	__le32 txbf_flag_set_mu_mode;
	__le32 txbf_flag_set_final_status;
	__le32 txbf_flag_not_set_verified_txbf_mode;
	__le32 txbf_flag_not_set_disable_p2p_access;
	__le32 txbf_flag_not_set_max_nss_in_he160;
	__le32 txbf_flag_not_set_disable_uldlofdma;
	__le32 txbf_flag_not_set_mcs_threshold_val;
	__le32 txbf_flag_not_set_final_status;
} __packed;

struct ath12k_htt_txbf_ofdma_ax_ndpa_stats_elem_t {
	__le32 ax_ofdma_ndpa_queued;
	__le32 ax_ofdma_ndpa_tried;
	__le32 ax_ofdma_ndpa_flush;
	__le32 ax_ofdma_ndpa_err;
} __packed;

struct ath12k_htt_txbf_ofdma_ax_ndpa_stats_tlv {
	__le32 num_elems_ax_ndpa_arr;
	__le32 arr_elem_size_ax_ndpa;
	DECLARE_FLEX_ARRAY(struct ath12k_htt_txbf_ofdma_ax_ndpa_stats_elem_t, ax_ndpa);
} __packed;

struct ath12k_htt_txbf_ofdma_ax_ndp_stats_elem_t {
	__le32 ax_ofdma_ndp_queued;
	__le32 ax_ofdma_ndp_tried;
	__le32 ax_ofdma_ndp_flush;
	__le32 ax_ofdma_ndp_err;
} __packed;

struct ath12k_htt_txbf_ofdma_ax_ndp_stats_tlv {
	__le32 num_elems_ax_ndp_arr;
	__le32 arr_elem_size_ax_ndp;
	DECLARE_FLEX_ARRAY(struct ath12k_htt_txbf_ofdma_ax_ndp_stats_elem_t, ax_ndp);
} __packed;

struct ath12k_htt_txbf_ofdma_ax_brp_stats_elem_t {
	__le32 ax_ofdma_brp_queued;
	__le32 ax_ofdma_brp_tried;
	__le32 ax_ofdma_brp_flushed;
	__le32 ax_ofdma_brp_err;
	__le32 ax_ofdma_num_cbf_rcvd;
} __packed;

struct ath12k_htt_txbf_ofdma_ax_brp_stats_tlv {
	__le32 num_elems_ax_brp_arr;
	__le32 arr_elem_size_ax_brp;
	DECLARE_FLEX_ARRAY(struct ath12k_htt_txbf_ofdma_ax_brp_stats_elem_t, ax_brp);
} __packed;

struct ath12k_htt_txbf_ofdma_ax_steer_stats_elem_t {
	__le32 num_ppdu_steer;
	__le32 num_ppdu_ol;
	__le32 num_usr_prefetch;
	__le32 num_usr_sound;
	__le32 num_usr_force_sound;
} __packed;

struct ath12k_htt_txbf_ofdma_ax_steer_stats_tlv {
	__le32 num_elems_ax_steer_arr;
	__le32 arr_elem_size_ax_steer;
	DECLARE_FLEX_ARRAY(struct ath12k_htt_txbf_ofdma_ax_steer_stats_elem_t, ax_steer);
} __packed;

struct ath12k_htt_txbf_ofdma_ax_steer_mpdu_stats_tlv {
	__le32 ax_ofdma_rbo_steer_mpdus_tried;
	__le32 ax_ofdma_rbo_steer_mpdus_failed;
	__le32 ax_ofdma_sifs_steer_mpdus_tried;
	__le32 ax_ofdma_sifs_steer_mpdus_failed;
} __packed;

enum ath12k_htt_stats_page_lock_state {
	ATH12K_HTT_STATS_PAGE_LOCKED	= 0,
	ATH12K_HTT_STATS_PAGE_UNLOCKED	= 1,
	ATH12K_NUM_PG_LOCK_STATE
};

#define ATH12K_PAGER_MAX	10

#define ATH12K_HTT_DLPAGER_ASYNC_LOCK_PG_CNT_INFO0	GENMASK(7, 0)
#define ATH12K_HTT_DLPAGER_SYNC_LOCK_PG_CNT_INFO0	GENMASK(15, 8)
#define ATH12K_HTT_DLPAGER_TOTAL_LOCK_PAGES_INFO1	GENMASK(15, 0)
#define ATH12K_HTT_DLPAGER_TOTAL_FREE_PAGES_INFO1	GENMASK(31, 16)
#define ATH12K_HTT_DLPAGER_TOTAL_LOCK_PAGES_INFO2	GENMASK(15, 0)
#define ATH12K_HTT_DLPAGER_TOTAL_FREE_PAGES_INFO2	GENMASK(31, 16)

struct ath12k_htt_pgs_info {
	__le32 page_num;
	__le32 num_pgs;
	__le32 ts_lsb;
	__le32 ts_msb;
} __packed;

struct ath12k_htt_dl_pager_stats_tlv {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	struct ath12k_htt_pgs_info pgs_info[ATH12K_NUM_PG_LOCK_STATE][ATH12K_PAGER_MAX];
} __packed;

#define ATH12K_HTT_MAX_RX_PKT_CNT		8
#define ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT	8
#define ATH12K_HTT_MAX_PER_BLK_ERR_CNT		20
#define ATH12K_HTT_MAX_RX_OTA_ERR_CNT		14
#define ATH12K_HTT_MAX_CH_PWR_INFO_SIZE		16
#define ATH12K_HTT_MAX_RX_PKT_CNT_EXT		4
#define ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT_EXT	4
#define ATH12K_HTT_MAX_RX_PKT_MU_CNT		14
#define ATH12K_HTT_MAX_TX_PKT_CNT		10
#define ATH12K_HTT_MAX_PHY_TX_ABORT_CNT		10
#define ATH12K_HTT_MAX_NEGATIVE_POWER_LEVEL 10 /* 0 to -10 dBm */
#define ATH12K_HTT_MAX_POWER_LEVEL 32 /* 0 to 32 dBm */
#define ATH12K_HTT_MAX_DPD_CAL_TABLE 12

#define HTT_STATS_ANI_MODE_M	GENMASK(7, 0)
#define HTT_STATS_CURR_EANI_MODE_M  GENMASK(7, 0)

struct ath12k_htt_phy_stats_tlv {
	a_sle32 nf_chain[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 false_radar_cnt;
	__le32 radar_cs_cnt;
	a_sle32 ani_level;
	__le32 fw_run_time;
	a_sle32 runtime_nf_chain[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 current_operating_width;
	__le32 current_device_width;
	__le32 last_radar_type;
	__le32 dfs_reg_domain;
	__le32 radar_mask_bit;
	__le32 radar_rssi;
	__le32 radar_dfs_flags;
	__le32 band_center_frequency_operating;
	__le32 band_center_frequency_device;
	union {
		u32 dword__ani_mode;
		struct {
			u32 ani_mode: 8,
			reserved:24;
		};
	};
	/*Maximum allowed ANI value */
	__le32 dl_max_ANI;
	/*Minimum RSSI of connected clients*/
	__le32 ani_dl_max_for_rssi;
	/*BIT [ 7 :  0]   :- curr_eani_mode
	 * BIT [31 :  8]   :- rsvd
	 *
	 * curr_eani_mode:
	 * EANI_MODE_MIN_DESENSE = 0,
	 * EANI_MODE_MAX_DESENSE = 1,
	 * EANI_MODE_DEFAULT_DESENSE = 2,
	 * EANI_MODE_MAX,
	 * EANI_MODE_TYPE_INVALID = 0xFF
	 */
	union {
		__le32 dword_curr_eani_mode;
		struct {
			__le32 curr_eani_mode: 8,
			    rsvd:24;
		};
	};
} __packed;

/* Publishes only the per-20MHz subband runtime NF and BDF NF values, separate from the
 * default struct ath12k_htt_phy_stats_tlv above.
 */

#define ATH12K_HTT_STATS_MAX_20MHZ_SUBBANDS  16
#define ATH12K_HTT_PHY_NF_SUBBAND_INVALID    1

enum ath12k_htt_phy_stats_subtype {
	ATH12K_HTT_PHY_STATS_SUBTYPE_DEFAULT,
	ATH12K_HTT_PHY_STATS_SUBTYPE_NF_SUBBAND,
};

struct ath12k_htt_phy_nf_subband_tlv {
	/* num_subbands: active 20 MHz subband count for current channel BW.
	 *   20 MHz -> 1, 40 MHz -> 2, 80 MHz -> 4, 160 MHz -> 8, 320 MHz -> 16.
	 * nf_runtime_subband[chain][0..num_subbands-1] holds valid dBm values.
	 * Inactive entries are set to ATH12K_HTT_PHY_NF_SUBBAND_INVALID, matching
	 * the existing sentinel used for unpopulated entries in nf_chain[]/
	 * runtime_nf_chain[] of struct ath12k_htt_phy_stats_tlv.
	 */
	__le32 num_subbands;
	a_sle32 nf_runtime_subband[ATH12K_HTT_STATS_MAX_CHAINS]
				   [ATH12K_HTT_STATS_MAX_20MHZ_SUBBANDS];
	/* per chain, per 20MHz subband BDF (calibration-loaded) NF in dBm */
	a_sle32 nf_bdf_subband[ATH12K_HTT_STATS_MAX_CHAINS]
			       [ATH12K_HTT_STATS_MAX_20MHZ_SUBBANDS];
} __packed;

struct ath12k_htt_phy_counters_tlv {
	__le32 rx_ofdma_timing_err_cnt;
	__le32 rx_cck_fail_cnt;
	__le32 mactx_abort_cnt;
	__le32 macrx_abort_cnt;
	__le32 phytx_abort_cnt;
	__le32 phyrx_abort_cnt;
	__le32 phyrx_defer_abort_cnt;
	__le32 rx_gain_adj_lstf_event_cnt;
	__le32 rx_gain_adj_non_legacy_cnt;
	__le32 rx_pkt_cnt[ATH12K_HTT_MAX_RX_PKT_CNT];
	__le32 rx_pkt_crc_pass_cnt[ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT];
	__le32 per_blk_err_cnt[ATH12K_HTT_MAX_PER_BLK_ERR_CNT];
	__le32 rx_ota_err_cnt[ATH12K_HTT_MAX_RX_OTA_ERR_CNT];
	__le32 rx_pkt_cnt_ext[ATH12K_HTT_MAX_RX_PKT_CNT_EXT];
	__le32 rx_pkt_crc_pass_cnt_ext[ATH12K_HTT_MAX_RX_PKT_CRC_PASS_CNT_EXT];
	__le32 rx_pkt_mu_cnt[ATH12K_HTT_MAX_RX_PKT_MU_CNT];
	__le32 tx_pkt_cnt[ATH12K_HTT_MAX_TX_PKT_CNT];
	__le32 phy_tx_abort_cnt[ATH12K_HTT_MAX_PHY_TX_ABORT_CNT];
} __packed;

#define HTT_STATS_PHY_RESET_CAL_DATA_COMPRESSED_M	GENMASK(0, 0)
#define HTT_STATS_PHY_RESET_CAL_DATA_SOURCE_M		GENMASK(2, 1)
#define HTT_STATS_PHY_RESET_XTALCAL_M			GENMASK(3, 3)
#define HTT_STATS_PHY_RESET_TPCCAL2GFPC_M		GENMASK(4, 4)
#define HTT_STATS_PHY_RESET_TPCCAL2GOPC_M		GENMASK(5, 5)
#define HTT_STATS_PHY_RESET_TPCCAL5GFPC_M		GENMASK(6, 6)
#define HTT_STATS_PHY_RESET_TPCCAL5GOPC_M		GENMASK(7, 7)
#define HTT_STATS_PHY_RESET_TPCCAL6GFPC_M		GENMASK(8, 8)
#define HTT_STATS_PHY_RESET_TPCCAL6GOPC_M		GENMASK(9, 9)
#define HTT_STATS_PHY_RESET_RXGAINCAL2G_M		GENMASK(10, 10)
#define HTT_STATS_PHY_RESET_RXGAINCAL5G_M		GENMASK(11, 11)
#define HTT_STATS_PHY_RESET_RXGAINCAL6G_M		GENMASK(12, 12)
#define HTT_STATS_PHY_RESET_AOACAL2G_M			GENMASK(13, 13)
#define HTT_STATS_PHY_RESET_AOACAL5G_M			GENMASK(14, 14)
#define HTT_STATS_PHY_RESET_AOACAL6G_M			GENMASK(15, 15)
#define HTT_STATS_PHY_RESET_XTAL_FROM_OTP_M		GENMASK(16, 16)

#define HTT_STATS_PHY_RESET_GLUT_LINEARITY_M		GENMASK(7, 0)
#define HTT_STATS_PHY_RESET_PLUT_LINEARITY_M		GENMASK(15, 8)
#define HTT_STATS_PHY_RESET_WLANDRIVERMODE_M		GENMASK(23, 16)

struct ath12k_htt_phy_reset_stats_tlv {
	__le32 pdev_id;
	__le32 chan_mhz;
	__le32 chan_band_center_freq1;
	__le32 chan_band_center_freq2;
	__le32 chan_phy_mode;
	__le32 chan_flags;
	__le32 chan_num;
	__le32 reset_cause;
	__le32 prev_reset_cause;
	__le32 phy_warm_reset_src;
	__le32 rx_gain_tbl_mode;
	__le32 xbar_val;
	__le32 force_calibration;
	__le32 phyrf_mode;
	__le32 phy_homechan;
	__le32 phy_tx_ch_mask;
	__le32 phy_rx_ch_mask;
	__le32 phybb_ini_mask;
	__le32 phyrf_ini_mask;
	__le32 phy_dfs_en_mask;
	__le32 phy_sscan_en_mask;
	__le32 phy_synth_sel_mask;
	__le32 phy_adfs_freq;
	__le32 cck_fir_settings;
	__le32 phy_dyn_pri_chan;
	__le32 cca_thresh;
	__le32 dyn_cca_status;
	__le32 rxdesense_thresh_hw;
	__le32 rxdesense_thresh_sw;
	__le32 phy_bw_code;
	__le32 phy_rate_mode;
	__le32 phy_band_code;
	__le32 phy_vreg_base;
	__le32 phy_vreg_base_ext;
	__le32 cur_table_index;
	__le32 whal_config_flag;
	__le32 nfcal_iteration_counts[3];

	union {
		u32 calmerge_stats;
		struct {
			u32 CalData_Compressed:1,
				CalDataSource:2,
				xtalcal:1,
				tpccal2GFPC:1,
				tpccal2GOPC:1,
				tpccal5GFPC:1,
				tpccal5GOPC:1,
				tpccal6GFPC:1,
				tpccal6GOPC:1,
				rxgaincal2G:1,
				rxgaincal5G:1,
				rxgaincal6G:1,
				aoacal2G:1,
				aoacal5G:1,
				aoacal6G:1,
				XTAL_from_OTP:1,
				rsvd1:15;
		};
	};
	union {
		u32 misc_stats;
		struct {
			u32 GLUT_linearity:8,
				PLUT_linearity:8,
				WlanDriverMode:8,
				rsvd2:8;
		};
	};
	__le32 BoardIDfromOTP;
} __packed;

struct ath12k_htt_phy_reset_counters_tlv {
	__le32 pdev_id;
	__le32 cf_active_low_fail_cnt;
	__le32 cf_active_low_pass_cnt;
	__le32 phy_off_through_vreg_cnt;
	__le32 force_calibration_cnt;
	__le32 rf_mode_switch_phy_off_cnt;
	__le32 temperature_recal_cnt;
} __packed;

#define HTT_MAX_CH_PWR_INFO_SIZE    16

#define HTT_PHY_TPC_STATS_CTL_REGION_GRP_M	GENMASK(7, 0)
#define HTT_PHY_TPC_STATS_SUB_BAND_INDEX_M    GENMASK(15, 8)
#define HTT_PHY_TPC_STATS_AG_CAP_EXT2_ENABLED_M    GENMASK(23, 16)
#define HTT_PHY_TPC_STATS_CTL_FLAG_M    GENMASK(31, 24)

struct ath12k_htt_phy_tpc_stats_tlv {
	__le32 pdev_id;
	__le32 tx_power_scale;
	__le32 tx_power_scale_db;
	__le32 min_negative_tx_power;
	__le32 reg_ctl_domain;
	__le32 max_reg_allowed_power[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 max_reg_allowed_power_6ghz[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 twice_max_rd_power;
	__le32 max_tx_power;
	__le32 home_max_tx_power;
	__le32 psd_power;
	__le32 eirp_power;
	__le32 power_type_6ghz;
	__le32 sub_band_cfreq[ATH12K_HTT_MAX_CH_PWR_INFO_SIZE];
	__le32 sub_band_txpower[ATH12K_HTT_MAX_CH_PWR_INFO_SIZE];
	__le32 array_gain_cap[ATH12K_HTT_STATS_MAX_CHAINS *
((ATH12K_HTT_STATS_MAX_CHAINS / 2) + 1)];
	union {
		struct {
		u32
			ctl_region_grp:8,
			sub_band_index:8,
			array_gain_cap_ext2_enabled:8,
			ctl_flag:8;
		};
		u32 ctl_args;
	};
	__le32 max_reg_only_allowed_power[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 tx_num_chains[ATH12K_HTT_STATS_MAX_CHAINS];
	__le32 tx_power[ATH12K_HTT_MAX_POWER_LEVEL];
	__le32 tx_power_neg[ATH12K_HTT_MAX_NEGATIVE_POWER_LEVEL];
	__le32 tpc_ie_power;
} __packed;

struct ath12k_htt_t2h_soc_txrx_stats_common_tlv {
	__le32 inv_peers_msdu_drop_count_hi;
	__le32 inv_peers_msdu_drop_count_lo;
} __packed;

#define ATH12K_HTT_AST_PDEV_ID_INFO		GENMASK(1, 0)
#define ATH12K_HTT_AST_VDEV_ID_INFO		GENMASK(9, 2)
#define ATH12K_HTT_AST_NEXT_HOP_INFO		BIT(10)
#define ATH12K_HTT_AST_MCAST_INFO		BIT(11)
#define ATH12K_HTT_AST_MONITOR_DIRECT_INFO	BIT(12)
#define ATH12K_HTT_AST_MESH_STA_INFO		BIT(13)
#define ATH12K_HTT_AST_MEC_INFO			BIT(14)
#define ATH12K_HTT_AST_INTRA_BSS_INFO		BIT(15)

#define ATH12K_HTT_AST_INFO1_TX_MON_OVERRIDE_STA	BIT(0)
#define ATH12K_HTT_AST_INFO1_RX_MON_OVERRIDE_STA	BIT(1)

struct ath12k_htt_ast_entry_tlv {
	__le32 sw_peer_id;
	__le32 ast_index;
	struct htt_mac_addr mac_addr;
	__le32 info;
	__le32 info1;
} __packed;

enum ath12k_htt_stats_direction {
	ATH12K_HTT_STATS_DIRECTION_TX,
	ATH12K_HTT_STATS_DIRECTION_RX
};

enum ath12k_htt_stats_ppdu_type {
	ATH12K_HTT_STATS_PPDU_TYPE_MODE_SU,
	ATH12K_HTT_STATS_PPDU_TYPE_DL_MU_MIMO,
	ATH12K_HTT_STATS_PPDU_TYPE_UL_MU_MIMO,
	ATH12K_HTT_STATS_PPDU_TYPE_DL_MU_OFDMA,
	ATH12K_HTT_STATS_PPDU_TYPE_UL_MU_OFDMA
};

enum ath12k_htt_stats_param_type {
	ATH12K_HTT_STATS_PREAM_OFDM,
	ATH12K_HTT_STATS_PREAM_CCK,
	ATH12K_HTT_STATS_PREAM_HT,
	ATH12K_HTT_STATS_PREAM_VHT,
	ATH12K_HTT_STATS_PREAM_HE,
	ATH12K_HTT_STATS_PREAM_EHT,
	ATH12K_HTT_STATS_PREAM_UHR,
	ATH12K_HTT_STATS_PREAM_COUNT,
};

#define ATH12K_HTT_PUNCT_STATS_MAX_SUBBAND_CNT	32

struct ath12k_htt_pdev_puncture_stats_tlv {
	__le32 mac_id__word;
	__le32 direction;
	__le32 preamble;
	__le32 ppdu_type;
	__le32 subband_cnt;
	__le32 last_used_pattern_mask;
	__le32 num_subbands_used_cnt[ATH12K_HTT_PUNCT_STATS_MAX_SUBBAND_CNT];
} __packed;

struct ath12k_htt_dmac_reset_stats_tlv {
	__le32 reset_count;
	__le32 reset_time_lo_ms;
	__le32 reset_time_hi_ms;
	__le32 disengage_time_lo_ms;
	__le32 disengage_time_hi_ms;
	__le32 engage_time_lo_ms;
	__le32 engage_time_hi_ms;
	__le32 disengage_count;
	__le32 engage_count;
	__le32 drain_dest_ring_mask;
} __packed;

#define ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS	16
#define ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_EXT_BINS	6
#define ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_INTERVAL_US	250

struct ath12k_htt_tx_pdev_ppdu_dur_stats_tlv {
	__le32 tx_ppdu_dur_hist[ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS];
	__le32 tx_success_time_us_low;
	__le32 tx_success_time_us_high;
	__le32 tx_fail_time_us_low;
	__le32 tx_fail_time_us_high;
	__le32 pdev_up_time_us_low;
	__le32 pdev_up_time_us_high;
	__le32 tx_ofdma_ppdu_dur_hist[ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS];
	__le32 tx_ppdu_dur_hist_ext[ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_EXT_BINS];
} __packed;

struct ath12k_htt_rx_pdev_ppdu_dur_stats_tlv {
	/** Tx PPDU duration histogram **/
	__le32 rx_ppdu_dur_hist[ATH12K_HTT_PDEV_STATS_PPDU_DUR_HIST_BINS];
} __packed;

struct ath12k_htt_tx_pdev_txop_dur_stats_tlv {
	__le32 tx_su_mdsb_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
	__le32 tx_ofdma_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
	__le32 tx_mimo_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
	__le32 combined_sched_cmd_txop_dur_hist[ATH12K_HTT_PDEV_STATS_TXOP_DUR_HIST_BINS];
} __packed;

#define ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS 16 /* 0-13, -2, -1 */
#define ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS 8
#define ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS  5  /* 20,40,80,160,320 MHz */

struct ath12k_htt_rx_pdev_ul_mumimo_trig_be_stats_tlv {
	__le32 mac_id__word;

	/* Number of times UL MUMIMO RX packets received */
	__le32 rx_11be_ul_mumimo;

	/* 11BE EHT UL MU-MIMO RX TB PPDU MCS stats */
	__le32 be_ul_mumimo_rx_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS];
	/* 11BE EHT UL MU-MIMO RX GI & LTF stats.
	 * Index 0 indicates 1xLTF + 1.6 msec GI
	 * Index 1 indicates 2xLTF + 1.6 msec GI
	 * Index 2 indicates 4xLTF + 3.2 msec GI
	 */
	__le32 be_ul_mumimo_rx_gi[ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS]
				 [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_MCS_COUNTERS];
	/* 11BE EHT UL MU-MIMO RX TB PPDU NSS stats
	 * (Increments the individual user NSS in the UL MU MIMO PPDU received)
	 */
	__le32 be_ul_mumimo_rx_nss[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/* 11BE EHT UL MU-MIMO RX TB PPDU BW stats */
	__le32 be_ul_mumimo_rx_bw[ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS];
	/* Number of times UL MUMIMO TB PPDUs received with STBC */
	__le32 be_ul_mumimo_rx_stbc;
	/* Number of times UL MUMIMO TB PPDUs received with LDPC */
	__le32 be_ul_mumimo_rx_ldpc;

	/* RSSI in dBm for Rx TB PPDUs */
	s8 be_rx_ul_mumimo_chain_rssi_in_dbm
				[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS]
				[ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS];
	/* Target RSSI programmed in UL MUMIMO triggers (units dBm) */
	s8 be_rx_ul_mumimo_target_rssi[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				      [ATH12K_HTT_RX_PDEV_STATS_NUM_BE_BW_COUNTERS];
	/* FD RSSI measured for Rx UL TB PPDUs (units dBm) */
	s8 be_rx_ul_mumimo_fd_rssi[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				  [ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/* Average pilot EVM measued for RX UL TB PPDU */
	s8 be_rx_ulmumimo_pilot_evm_db_mean[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/** Number of times UL MUMIMO TB PPDUs received in a punctured mode */
	__le32 rx_ul_mumimo_punctured_mode
		[ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	/**
	 * Number of EHT UL MU-MIMO per-user responses containing only a QoS null
	 * in response to basic trigger. Typically a data response is expected.
	 */
	__le32 be_ul_mumimo_basic_trigger_rx_qos_null_only;
} __packed;

#define ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS  5  /* 20,40,80,160,320 MHz */

struct ath12k_htt_rx_pdev_ul_mumimo_trig_bn_stats_tlv {
	__le32 mac_id__word;

	/* Number of times UL MUMIMO RX packets received */
	__le32 rx_11bn_ul_mumimo;

	/* 11BE UHR UL MU-MIMO RX TB PPDU MCS stats */
	__le32 bn_ul_mumimo_rx_mcs[ATH12K_HTT_RX_PDEV_STATS_NUM_BN_MCS_COUNTERS];
	/* 11BE UHR UL MU-MIMO RX GI & LTF stats.
	 * Index 0 indicates 1xLTF + 1.6 msec GI
	 * Index 1 indicates 2xLTF + 1.6 msec GI
	 * Index 2 indicates 4xLTF + 3.2 msec GI
	 */
	__le32 bn_ul_mumimo_rx_gi[ATH12K_HTT_RX_PDEV_STATS_NUM_GI_COUNTERS]
				 [ATH12K_HTT_RX_PDEV_STATS_NUM_BN_MCS_COUNTERS];
	/* 11BE UHR UL MU-MIMO RX TB PPDU NSS stats
	 * (Increments the individual user NSS in the UL MU MIMO PPDU received)
	 */
	__le32 bn_ul_mumimo_rx_nss[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/* 11BE EHT UL MU-MIMO RX TB PPDU BW stats */
	__le32 bn_ul_mumimo_rx_bw[ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	/* Number of times UL MUMIMO TB PPDUs received with STBC */
	__le32 bn_ul_mumimo_rx_stbc;
	/* Number of times UL MUMIMO TB PPDUs received with LDPC */
	__le32 bn_ul_mumimo_rx_ldpc;

	/* RSSI in dBm for Rx TB PPDUs */
	s8 bn_rx_ul_mumimo_chain_rssi_in_dbm
				[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS]
				[ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	/* Target RSSI programmed in UL MUMIMO triggers (units dBm) */
	s8 bn_rx_ul_mumimo_target_rssi[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				      [ATH12K_HTT_RX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	/* FD RSSI measured for Rx UL TB PPDUs (units dBm) */
	s8 bn_rx_ul_mumimo_fd_rssi[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				  [ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/* Average pilot EVM measued for RX UL TB PPDU */
	s8 bn_rx_ulmumimo_pilot_evm_db_mean[ATH12K_HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER]
				[ATH12K_HTT_RX_PDEV_STATS_ULMUMIMO_NUM_SPATIAL_STREAMS];
	/** Number of times UL MUMIMO TB PPDUs received in a punctured mode */
	__le32 bn_rx_ul_mumimo_punctured_mode
		[ATH12K_HTT_RX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	/**
	 * Number of UHR UL MU-MIMO per-user responses containing only a QoS null
	 * in response to basic trigger. Typically a data response is expected.
	 */
	__le32 bn_ul_mumimo_basic_trigger_rx_qos_null_only;
} __packed;

#define ATH12K_HTT_MAX_NUM_CHAN_ACC_LAT_INTR	9

enum ath12k_htt_stats_sched_ofdma_txbf_ineligibility {
	ATH12K_SCHED_OFDMA_TXBF = 0,
	ATH12K_SCHED_OFDMA_TXBF_IS_SANITY_FAILED,
	ATH12K_SCHED_OFDMA_TXBF_IS_EBF_ALLOWED_FAILIED,
	ATH12K_SCHED_OFDMA_TXBF_RU_ALLOC_BW_DROP_COUNT,
	ATH12K_SCHED_OFDMA_TXBF_INVALID_CV_QUERY_COUNT,
	ATH12K_SCHED_OFDMA_TXBF_AVG_TXTIME_LESS_THAN_TXBF_SND_THERHOLD,
	ATH12K_SCHED_OFDMA_TXBF_IS_CANDIDATE_KICKED_OUT,
	ATH12K_SCHED_OFDMA_TXBF_CV_IMAGE_BUF_INVALID,
	ATH12K_SCHED_OFDMA_TXBF_INELIGIBILITY_MAX,
};

enum ath12k_htt_stats_sched_algo_comb_first_cmd_abort {
	ATH12K_FIRST_CMD_ABORT_PRIMARY_NOT_TCP = 0,
	ATH12K_FIRST_CMD_ABORT_SOUNDING_SEQ,
	ATH12K_FIRST_CMD_ABORT_STANDALONE_BSRP,
	ATH12K_FIRST_CMD_ABORT_SPECIAL_SEQUENCE,
	ATH12K_FIRST_CMD_ABORT_NOT_BURST_SEQUENCE,
	ATH12K_FIRST_CMD_ABORT_TXTIME_NOT_SUFFICIENT,
	ATH12K_FIRST_CMD_ABORT_RESERVED1,
	ATH12K_FIRST_CMD_ABORT_RESERVED2,
	ATH12K_FIRST_CMD_ABORT_RESERVED3,
	ATH12K_COMB_ABORT_FIRST_CMD_MAX,
};

enum ath12k_htt_stats_sched_algo_comb_second_cmd_abort {
	ATH12K_SECOND_CMD_ABORT_PRIMARY_NOT_TCP = 0,
	ATH12K_SECOND_CMD_ABORT_SOUNDING_SEQ,
	ATH12K_SECOND_CMD_ABORT_STANDALONE_BSRP,
	ATH12K_SECOND_CMD_ABORT_SPECIAL_SEQUENCE,
	ATH12K_DUAL_RING_SENSITIVE_BIT_SET,
	ATH12K_SECOND_CMD_NOT_AVAILABLE,
	ATH12K_SECOND_CMD_ABORT_BW_MISMATCH,
	ATH12K_SECOND_CMD_ABORT_AC_MISMATCH,
	ATH12K_SECOND_CMD_ABORT_SAME_TXTYPE,
	ATH12K_TOTAL_TXOP_EXCEEDED,
	ATH12K_SECOND_CMD_ABORT_RESERVED1,
	ATH12K_SECOND_CMD_ABORT_RESERVED2,
	ATH12K_SECOND_CMD_ABORT_RESERVED3,
	ATH12K_COMB_ABORT_SECOND_CMD_MAX,
};

struct ath12k_htt_pdev_sched_algo_ofdma_stats_tlv {
	__le32 mac_id__word;
	__le32 rate_based_dlofdma_enabled_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 rate_based_dlofdma_disabled_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 rate_based_dlofdma_probing_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 rate_based_dlofdma_monitor_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 chan_acc_lat_based_dlofdma_enabled_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 chan_acc_lat_based_dlofdma_disabled_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 chan_acc_lat_based_dlofdma_monitor_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 downgrade_to_dl_su_ru_alloc_fail[ATH12K_HTT_NUM_AC_WMM];
	__le32 candidate_list_single_user_disable_ofdma[ATH12K_HTT_NUM_AC_WMM];
	__le32 dl_cand_list_dropped_high_ul_qos_weight[ATH12K_HTT_NUM_AC_WMM];
	__le32 ax_dlofdma_disabled_due_to_pipelining[ATH12K_HTT_NUM_AC_WMM];
	__le32 dlofdma_disabled_su_only_eligible[ATH12K_HTT_NUM_AC_WMM];
	__le32 dlofdma_disabled_consec_no_mpdus_tried[ATH12K_HTT_NUM_AC_WMM];
	__le32 dlofdma_disabled_consec_no_mpdus_success[ATH12K_HTT_NUM_AC_WMM];
	__le32 txbf_ofdma_ineligibility_stat[ATH12K_SCHED_OFDMA_TXBF_INELIGIBILITY_MAX];
	__le32 avg_chan_acc_lat_hist[ATH12K_HTT_MAX_NUM_CHAN_ACC_LAT_INTR];
	__le32 dl_ofdma_nbinwb_selected_over_mu_mimo[ATH12K_HTT_NUM_AC_WMM];
	__le32 dl_ofdma_nbinwb_selected_standalone[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_only_dl_scheduler_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_only_ul_scheduler_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_additional_dl_scheduler_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_additional_ul_scheduler_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_ul_scheduler_for_bsrp_cnt[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_dl_scheduler_due_to_skip_ul[ATH12K_HTT_NUM_AC_WMM];
	__le32 running_ul_scheduler_due_to_skip_dl[ATH12K_HTT_NUM_AC_WMM];
	__le32 abort_reason_comb_first_cmd[ATH12K_COMB_ABORT_FIRST_CMD_MAX];
	__le32 abort_reason_comb_second_cmd[ATH12K_COMB_ABORT_SECOND_CMD_MAX];
} __packed;

#define ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS		4
#define ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS	8
#define ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS		14

enum ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE {
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_26,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_52,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_52_26,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_106,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_106_26,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_242,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_484,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_484_242,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996_484,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996_484_242,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x2,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x2_484,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x3,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x3_484,
	ATH12K_HTT_TX_RX_PDEV_STATS_BE_RU_SIZE_996x4,
	ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS,
};

enum ATH12K_HTT_RC_MODE {
	ATH12K_HTT_RC_MODE_SU_OL,
	ATH12K_HTT_RC_MODE_SU_BF,
	ATH12K_HTT_RC_MODE_MU1_INTF,
	ATH12K_HTT_RC_MODE_MU2_INTF,
	ATH12K_HTT_RC_MODE_MU3_INTF,
	ATH12K_HTT_RC_MODE_MU4_INTF,
	ATH12K_HTT_RC_MODE_MU5_INTF,
	ATH12K_HTT_RC_MODE_MU6_INTF,
	ATH12K_HTT_RC_MODE_MU7_INTF,
	ATH12K_HTT_RC_MODE_2D_COUNT
};

enum ath12k_htt_stats_rc_mode {
	ATH12K_HTT_STATS_RC_MODE_DLSU     = 0,
	ATH12K_HTT_STATS_RC_MODE_DLMUMIMO = 1,
	ATH12K_HTT_STATS_RC_MODE_DLOFDMA  = 2,
	ATH12K_HTT_STATS_RC_MODE_ULMUMIMO = 3,
	ATH12K_HTT_STATS_RC_MODE_ULOFDMA  = 4,
};

enum ath12k_htt_stats_ru_type {
	ATH12K_HTT_STATS_RU_TYPE_INVALID,
	ATH12K_HTT_STATS_RU_TYPE_SINGLE_RU_ONLY,
	ATH12K_HTT_STATS_RU_TYPE_SINGLE_AND_MULTI_RU,
};

struct ath12k_htt_tx_rate_stats {
	__le32 ppdus_tried;
	__le32 ppdus_ack_failed;
	__le32 mpdus_tried;
	__le32 mpdus_failed;
} __packed;

struct ath12k_htt_stats_npca_stats_tlv {
	__le32 mu_rts_tx_success;
	__le32 mu_rts_tx_fail;
	__le32 mu_rts_rx_success;
	__le32 bsrp_ntb_tx_success;
	__le32 bsrp_ntb_tx_fail;
	__le32 bsrp_ntb_rx_success;
	__le32 bsrp_tx_success;
	__le32 bsrp_tx_fail;
	__le32 bsrp_rx_success;
	__le32 tx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	__le32 tx_punctured_mode[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 rx_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	__le32 rx_punctured_mode[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 schd_cmd_result_npca[ATH12K_HTT_STATS_MAX_SCH_CMD_RESULT];
	struct ath12k_htt_tx_rate_stats
		npca_per_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	struct ath12k_htt_tx_rate_stats npca_per_tx_su_punctured_mode
			[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
} __packed;

#define ATH12K_HTT_TX_PER_RATE_STATS_NUM_QUEUE_DEPTH_COUNTERS 2
#define ATH12K_HTT_TX_PER_RATE_STATS_NUM_MLO_RA_DD_MCS_DROP_COUNTERS 2
#define HTT_PER_RATE_STATS_WIFI_VERSION		GENMASK(3, 0)

struct ath12k_htt_tx_per_rate_stats_tlv {
	__le32 rc_mode;
	__le32 last_probed_mcs;
	__le32 last_probed_nss;
	__le32 last_probed_bw;
	struct ath12k_htt_tx_rate_stats per_bw[ATH12K_HTT_TX_PDEV_STATS_NUM_BW_CNTRS];
	struct ath12k_htt_tx_rate_stats per_nss[ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS];
	struct ath12k_htt_tx_rate_stats per_mcs[ATH12K_HTT_TXBF_RATE_STAT_NUM_MCS_CNTRS];
	struct ath12k_htt_tx_rate_stats per_bw320;
	__le32 probe_cnt[ATH12K_HTT_RC_MODE_2D_COUNT];
	__le32 ru_type;
	struct ath12k_htt_tx_rate_stats ru[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	struct ath12k_htt_tx_rate_stats per_tx_su_punctured_mode
			[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	__le32 mlo_ra_queue_depth_status
		[ATH12K_HTT_TX_PER_RATE_STATS_NUM_QUEUE_DEPTH_COUNTERS];
	__le32 mlo_rate_drop_down
		[ATH12K_HTT_TX_PER_RATE_STATS_NUM_MLO_RA_DD_MCS_DROP_COUNTERS];
	__le32 wifi_version_word;
	struct ath12k_htt_tx_rate_stats npca_per_bw
			[ATH12K_HTT_TX_PDEV_STATS_NUM_BN_BW_COUNTERS];
	struct ath12k_htt_tx_rate_stats npca_per_tx_su_punctured_mode
			[ATH12K_HTT_TX_PDEV_STATS_NUM_PUNCTURED_MODE_COUNTERS];
	struct ath12k_htt_tx_rate_stats per_mcs_ext_3
			[ATH12K_HTT_TX_PDEV_STATS_NUM_EXTRA3_MCS_COUNTERS];
	struct ath12k_htt_tx_rate_stats per_dru
			[ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_CNT];
} __packed;

#define ATH12K_HTT_TX_PDEV_NUM_BE_MCS_CNTRS		16
#define ATH12K_HTT_TX_PDEV_NUM_BE_BW_CNTRS		5
#define ATH12K_HTT_TX_PDEV_NUM_EHT_SIG_MCS_CNTRS	4
#define ATH12K_HTT_TX_PDEV_NUM_GI_CNTRS			4
#define ATH12K_HTT_TX_PDEV_NUM_BN_MCS_CNTRS     20
#define ATH12K_HTT_TX_PDEV_NUM_UHR_SIG_MCS_CNTRS    4
#define ATH12K_HTT_TX_PDEV_NUM_BN_BW_CNTRS      5
#define ATH12K_HTT_TX_PDEV_MAX_TXBF_OFDMA_USERS		8

struct ath12k_htt_tx_pdev_rate_stats_be_ofdma_tlv {
	__le32 mac_id__word;
	__le32 be_ofdma_tx_ldpc;
	__le32 be_ofdma_tx_mcs[ATH12K_HTT_TX_PDEV_NUM_BE_MCS_CNTRS];
	__le32 be_ofdma_tx_nss[ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS];
	__le32 be_ofdma_tx_bw[ATH12K_HTT_TX_PDEV_NUM_BE_BW_CNTRS];
	__le32 gi[ATH12K_HTT_TX_PDEV_NUM_GI_CNTRS][ATH12K_HTT_TX_PDEV_NUM_BE_MCS_CNTRS];
	__le32 be_ofdma_tx_ru_size[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 be_ofdma_eht_sig_mcs[ATH12K_HTT_TX_PDEV_NUM_EHT_SIG_MCS_CNTRS];
	__le32 be_ofdma_ba_ru_size[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];

	__le32 bn_ofdma_tx_ldpc;
	__le32 bn_ofdma_tx_mcs[ATH12K_HTT_TX_PDEV_NUM_BN_MCS_CNTRS];
	__le32 bn_ofdma_tx_nss[ATH12K_HTT_PDEV_STAT_NUM_SPATIAL_STREAMS];
	__le32 bn_ofdma_tx_bw[ATH12K_HTT_TX_PDEV_NUM_BN_BW_CNTRS];
	__le32 gi_bn[ATH12K_HTT_TX_PDEV_NUM_GI_CNTRS]
		    [ATH12K_HTT_TX_PDEV_NUM_BN_MCS_CNTRS];
	__le32 bn_ofdma_tx_ru_size[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 bn_ofdma_uhr_sig_mcs[ATH12K_HTT_TX_PDEV_NUM_UHR_SIG_MCS_CNTRS];
	__le32 bn_ofdma_ba_ru_size[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
} __packed;

struct ath12k_htt_pdev_mbssid_ctrl_frame_tlv {
	__le32 mac_id__word;
	__le32 basic_trigger_across_bss;
	__le32 basic_trigger_within_bss;
	__le32 bsr_trigger_across_bss;
	__le32 bsr_trigger_within_bss;
	__le32 mu_rts_across_bss;
	__le32 mu_rts_within_bss;
	__le32 ul_mumimo_trigger_across_bss;
	__le32 ul_mumimo_trigger_within_bss;
} __packed;

struct htt_txbf_ofdma_be_ndpa_stats_elem_t {
	u32 be_ofdma_ndpa_queued;
	u32 be_ofdma_ndpa_tried;
	u32 be_ofdma_ndpa_flushed;
	u32 be_ofdma_ndpa_err;
};

struct htt_txbf_ofdma_be_ndpa_stats_tlv {
	u32 num_elems_be_ndpa_arr;
	u32 arr_elem_size_be_ndpa;
	struct htt_txbf_ofdma_be_ndpa_stats_elem_t be_ndpa[1];
};

struct htt_txbf_ofdma_be_ndp_stats_elem_t {
	u32 be_ofdma_ndp_queued;
	u32 be_ofdma_ndp_tried;
	u32 be_ofdma_ndp_flushed;
	u32 be_ofdma_ndp_err;
};

struct htt_txbf_ofdma_be_ndp_stats_tlv {
	u32 num_elems_be_ndp_arr;
	u32 arr_elem_size_be_ndp;
	struct htt_txbf_ofdma_be_ndp_stats_elem_t be_ndp[1];
};

struct htt_txbf_ofdma_be_brp_stats_elem_t {
	u32 be_ofdma_brpoll_queued;
	u32 be_ofdma_brpoll_tried;
	u32 be_ofdma_brpoll_flushed;
	u32 be_ofdma_brp_err;
	u32 be_ofdma_brp_err_num_cbf_rcvd;
};

struct htt_txbf_ofdma_be_brp_stats_tlv {
	u32 num_elems_be_brp_arr;
	u32 arr_elem_size_be_brp;
	struct htt_txbf_ofdma_be_brp_stats_elem_t be_brp[1];
};

struct htt_txbf_ofdma_be_steer_stats_elem_t {
	u32 be_ofdma_num_ppdu_steer;
	u32 be_ofdma_num_ppdu_ol;
	u32 be_ofdma_num_usrs_prefetch;
	u32 be_ofdma_num_usrs_sound;
	u32 be_ofdma_num_usrs_force_sound;
};

struct htt_txbf_ofdma_be_steer_stats_tlv {
	u32 num_elems_be_steer_arr;
	u32 arr_elem_size_be_steer;
	struct htt_txbf_ofdma_be_steer_stats_elem_t be_steer[1];
};

struct ath12k_htt_txbf_ofdma_be_steer_mpdu_stats_tlv {
	__le32 be_ofdma_rbo_steer_mpdus_tried;
	__le32 be_ofdma_rbo_steer_mpdus_failed;
	__le32 be_ofdma_sifs_steer_mpdus_tried;
	__le32 be_ofdma_sifs_steer_mpdus_failed;
} __packed;

struct htt_txbf_ofdma_bn_ndpa_stats_elem_t {
	__le32 bn_ofdma_ndpa_queued;
	__le32 bn_ofdma_ndpa_tried;
	__le32 bn_ofdma_ndpa_flushed;
	__le32 bn_ofdma_ndpa_err;
} __packed;

struct htt_txbf_ofdma_bn_ndpa_stats_tlv {
	__le32 num_elems_bn_ndpa_arr;
	__le32 arr_elem_size_bn_ndpa;
	struct htt_txbf_ofdma_bn_ndpa_stats_elem_t
		bn_ndpa[ATH12K_HTT_TX_PDEV_MAX_TXBF_OFDMA_USERS];
} __packed;

struct htt_txbf_ofdma_bn_ndp_stats_elem_t {
	__le32 bn_ofdma_ndp_queued;
	__le32 bn_ofdma_ndp_tried;
	__le32 bn_ofdma_ndp_flushed;
	__le32 bn_ofdma_ndp_err;
} __packed;

struct htt_txbf_ofdma_bn_ndp_stats_tlv {
	__le32 num_elems_bn_ndp_arr;
	__le32 arr_elem_size_bn_ndp;
	struct htt_txbf_ofdma_bn_ndp_stats_elem_t
		bn_ndp[ATH12K_HTT_TX_PDEV_MAX_TXBF_OFDMA_USERS];
} __packed;

struct htt_txbf_ofdma_bn_brp_stats_elem_t {
	__le32 bn_ofdma_brpoll_queued;
	__le32 bn_ofdma_brpoll_tried;
	__le32 bn_ofdma_brpoll_flushed;
	__le32 bn_ofdma_brp_err;
	__le32 bn_ofdma_brp_err_num_cbf_rcvd;
} __packed;

struct htt_txbf_ofdma_bn_brp_stats_tlv {
	__le32 num_elems_bn_brp_arr;
	__le32 arr_elem_size_bn_brp;
	struct htt_txbf_ofdma_bn_brp_stats_elem_t
		bn_brp[ATH12K_HTT_TX_PDEV_MAX_TXBF_OFDMA_USERS];
} __packed;

struct htt_txbf_ofdma_bn_steer_stats_elem_t {
	__le32 bn_ofdma_num_ppdu_steer;
	__le32 bn_ofdma_num_ppdu_ol;
	__le32 bn_ofdma_num_usrs_prefetch;
	__le32 bn_ofdma_num_usrs_sound;
	__le32 bn_ofdma_num_usrs_force_sound;
} __packed;

struct htt_txbf_ofdma_bn_steer_stats_tlv {
	__le32 num_elems_bn_steer_arr;
	__le32 arr_elem_size_bn_steer;
	struct htt_txbf_ofdma_bn_steer_stats_elem_t
		bn_steer[ATH12K_HTT_TX_PDEV_MAX_TXBF_OFDMA_USERS];
} __packed;

struct ath12k_htt_txbf_ofdma_bn_steer_mpdu_stats_tlv {
	__le32 bn_ofdma_rbo_steer_mpdus_tried;
	__le32 bn_ofdma_rbo_steer_mpdus_failed;
	__le32 bn_ofdma_sifs_steer_mpdus_tried;
	__le32 bn_ofdma_sifs_steer_mpdus_failed;
} __packed;

struct htt_rx_pdev_be_ul_ofdma_user_stats_tlv {
	u32 user_index;
	u32 be_rx_ulofdma_non_data_ppdu;
	u32 be_rx_ulofdma_data_ppdu;
	u32 be_rx_ulofdma_mpdu_ok;
	u32 be_rx_ulofdma_mpdu_fail;
	u32 be_rx_ulofdma_non_data_nusers;
	u32 be_rx_ulofdma_data_nusers;
};

struct htt_rx_pdev_bn_ul_ofdma_user_stats_tlv {
	__le32 user_index;
	__le32 bn_rx_ulofdma_non_data_ppdu;
	__le32 bn_rx_ulofdma_data_ppdu;
	__le32 bn_rx_ulofdma_mpdu_ok;
	__le32 bn_rx_ulofdma_mpdu_fail;
	__le32 bn_rx_ulofdma_non_data_nusers;
	__le32 bn_rx_ulofdma_data_nusers;
};

#define ATH12K_HTT_RX_NUM_BE_MCS_COUNTERS	16
#define ATH12K_HTT_RX_NUM_GI_COUNTERS		4
#define ATH12K_HTT_RX_NUM_SPATIAL_STREAMS	8
#define ATH12K_HTT_RX_NUM_BE_BW_COUNTERS	5
#define ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK	5
#define ATH12K_HTT_RX_NUM_BN_MCS_COUNTERS	20
#define ATH12K_HTT_RX_NUM_BN_BW_COUNTERS	5

struct ath12k_htt_rx_pdev_be_ul_trig_stats_tlv {
	__le32 mac_id_word;
	__le32 rx_11be_ul_ofdma;
	__le32 be_ul_ofdma_rx_mcs[ATH12K_HTT_RX_NUM_BE_MCS_COUNTERS];
	__le32 be_ul_ofdma_rx_gi[ATH12K_HTT_RX_NUM_GI_COUNTERS]
				[ATH12K_HTT_RX_NUM_BE_MCS_COUNTERS];
	__le32 be_ul_ofdma_rx_nss[ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	__le32 be_ul_ofdma_rx_bw[ATH12K_HTT_RX_NUM_BE_BW_COUNTERS];
	__le32 be_ul_ofdma_rx_stbc;
	__le32 be_ul_ofdma_rx_ldpc;
	__le32 be_rx_data_ru_size_ppdu[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 be_rx_non_data_ru_size_ppdu[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 be_uplink_sta_aid[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	s32 be_uplink_sta_target_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	s32 be_uplink_sta_fd_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 be_uplink_sta_power_headroom[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 be_ul_ofdma_basic_trig_rx_qos_null_only;
	__le32 ul_mlo_send_qdepth_params_count;
	__le32 ul_mlo_proc_qdepth_params_count;
	__le32 ul_mlo_proc_accepted_qdepth_params_cnt;
	__le32 ul_mlo_proc_discarded_qdepth_params_cnt;
	__le32 rx_11bn_ul_ofdma;
	__le32 bn_ul_ofdma_rx_mcs[ATH12K_HTT_RX_NUM_BN_MCS_COUNTERS];
	__le32 bn_ul_ofdma_rx_gi[ATH12K_HTT_RX_NUM_GI_COUNTERS]
				[ATH12K_HTT_RX_NUM_BN_MCS_COUNTERS];
	__le32 bn_ul_ofdma_rx_nss[ATH12K_HTT_RX_NUM_SPATIAL_STREAMS];
	__le32 bn_ul_ofdma_rx_bw[ATH12K_HTT_RX_NUM_BN_BW_COUNTERS];
	__le32 bn_ul_ofdma_rx_stbc;
	__le32 bn_ul_ofdma_rx_ldpc;
	__le32 bn_rx_data_ru_size_ppdu[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 bn_rx_non_data_ru_size_ppdu[ATH12K_HTT_TX_RX_PDEV_NUM_BE_RU_SIZE_CNTRS];
	__le32 bn_uplink_sta_aid[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	s32 bn_uplink_sta_target_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	s32 bn_uplink_sta_fd_rssi[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 bn_uplink_sta_power_headroom[ATH12K_HTT_RX_UL_MAX_UPLINK_RSSI_TRACK];
	__le32 bn_ul_ofdma_basic_trig_rx_qos_null_only;
	__le32 bn_ul_ofdma_rx_dru_sbw[ATH12K_HTT_BN_UL_OFDMA_NUM_DRU_SBW_COUNT];
	__le32 bn_rx_data_dru_size_ppdu[ATH12K_HTT_TX_PDEV_STATS_BN_DRU_SIZE_CNT];
	__le32 be_ulofdma_implicit_trig_tried;
	__le32 be_ulofdma_implicit_trig_qos_null;
	__le32 bn_ulofdma_implicit_trig_tried;
	__le32 bn_ulofdma_implicit_trig_qos_null;
} __packed;

struct htt_umac_ssr_stats_tlv {
        u32 total_done;
        u32 trigger_requests_count;
        u32 total_trig_dropped;
        u32 umac_disengaged_count;
        u32 umac_soft_reset_count;
        u32 umac_engaged_count;
        u32 last_trigger_request_ms;
        u32 last_start_ms;
        u32 last_start_disengage_umac_ms;
        u32 last_enter_ssr_platform_thread_ms;
        u32 last_exit_ssr_platform_thread_ms;
        u32 last_start_engage_umac_ms;
        u32 last_done_successful_ms;
        u32 last_e2e_delta_ms;
        u32 max_e2e_delta_ms;
        u32 trigger_count_for_umac_hang;
        u32 trigger_count_for_mlo_quick_ssr;
        u32 trigger_count_for_unknown_signature;
        u32 post_reset_tqm_sync_cmd_completion_ms;
        u32 htt_sync_mlo_initiate_umac_recovery_ms;
        u32 htt_sync_do_pre_reset_ms;
        u32 htt_sync_do_post_reset_start_ms;
        u32 htt_sync_do_post_reset_complete_ms;
};

#define HTT_PUNCTURE_STATS_MAX_SUBBAND_COUNT 32

#define HTT_PDEV_PUNCTURE_STATS_MAC_ID_M 0x000000ff
#define HTT_PDEV_PUNCTURE_STATS_MAC_ID_S 0

#define HTT_PDEV_PUNCTURE_STATS_MAC_ID_GET(_var) \
	(((_var) & HTT_PDEV_PUNCTURE_STATS_MAC_ID_M) >> \
	 HTT_PDEV_PUNCTURE_STATS_MAC_ID_S)
#define HTT_PDEV_PUNCTURE_STATS_MAC_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_PDEV_PUNCTURE_STATS_MAC_ID, _val); \
		((_var) |= ((_val) << HTT_PDEV_PUNCTURE_STATS_MAC_ID_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_NUM_LINKS_M			0x00000003
#define HTT_ML_PEER_DETAILS_NUM_LINKS_S			0
#define HTT_ML_PEER_DETAILS_ML_PEER_ID_M		0x00003FFC
#define HTT_ML_PEER_DETAILS_ML_PEER_ID_S		2
#define HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_M		0x0001C000
#define HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_S		14
#define HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_M		0x00060000
#define HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_S		17
#define HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_M		0x00380000
#define HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_S		19
#define HTT_ML_PEER_DETAILS_NON_STR_M			0x00400000
#define HTT_ML_PEER_DETAILS_NON_STR_S			22
#define HTT_ML_PEER_DETAILS_EMLSR_M			0x00800000
#define HTT_ML_PEER_DETAILS_EMLSR_S			23
#define HTT_ML_PEER_DETAILS_IS_STA_KO_M			0x01000000
#define HTT_ML_PEER_DETAILS_IS_STA_KO_S			24
#define HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_M		0x06000000
#define HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_S		25
#define HTT_ML_PEER_DETAILS_ALLOCATED_M			0x08000000
#define HTT_ML_PEER_DETAILS_ALLOCATED_S			27

#define HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_M	0x000000ff
#define HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_S	0

#define HTT_ML_PEER_DETAILS_NUM_LINKS_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_NUM_LINKS_M) >> \
	 HTT_ML_PEER_DETAILS_NUM_LINKS_S)

#define HTT_ML_PEER_DETAILS_NUM_LINKS_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_NUM_LINKS, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_NUM_LINKS_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_NUM_LINKS_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_ML_PEER_ID_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_ML_PEER_ID_M) >> \
	 HTT_ML_PEER_DETAILS_ML_PEER_ID_S)

#define HTT_ML_PEER_DETAILS_ML_PEER_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_ML_PEER_ID, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_ML_PEER_ID_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_ML_PEER_ID_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_M) >> \
	 HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_S)

#define HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_PRIMARY_LINK_IDX_S)); \
	} while (0)
#define HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_M) >> \
	 HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_S)

#define HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_PRIMARY_CHIP_ID_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_M) >> \
	 HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_S)

#define HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_LINK_INIT_COUNT, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_LINK_INIT_COUNT_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_NON_STR_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_NON_STR_M) >> \
	 HTT_ML_PEER_DETAILS_NON_STR_S)

#define HTT_ML_PEER_DETAILS_NON_STR_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_NON_STR, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_NON_STR_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_NON_STR_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_EMLSR_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_EMLSR_M) >> \
	 HTT_ML_PEER_DETAILS_EMLSR_S)

#define HTT_ML_PEER_DETAILS_EMLSR_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_EMLSR, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_EMLSR_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_EMLSR_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_IS_STA_KO_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_IS_STA_KO_M) >> \
	 HTT_ML_PEER_DETAILS_IS_STA_KO_S)

#define HTT_ML_PEER_DETAILS_IS_STA_KO_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_IS_STA_KO, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_IS_STA_KO_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_IS_STA_KO_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_M) >> \
	 HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_S)
#define HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_NUM_LOCAL_LINKS_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_ALLOCATED_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_ALLOCATED_M) >> \
	 HTT_ML_PEER_DETAILS_ALLOCATED_S)

#define HTT_ML_PEER_DETAILS_ALLOCATED_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_ALLOCATED, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_ALLOCATED_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_ALLOCATED_S)); \
	} while (0)

#define HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_GET(_var) \
	(((_var) & HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_M) >> \
	 HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_S)

#define HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP, _val); \
		((_var) &= ~(HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_DETAILS_PARTICIPATING_CHIPS_BITMAP_S)); \
	} while (0)

struct htt_ml_peer_details_tlv {
	struct htt_mac_addr remote_mld_mac_addr;
	union {
		struct {
			u32 num_links:2,
			    ml_peer_id:12,
			    primary_link_idx:3,
			    primary_chip_id:2,
			    link_init_count:3,
			    non_str:1,
			    emlsr:1,
			    is_sta_ko:1,
			    num_local_links:2,
			    allocated:1,
			    reserved:4;
		};
		u32 msg_dword_1;
	};

	union {
		struct {
			u32 participating_chips_bitmap:8,
			    status_required:8,
			    reserved1:16;
		};
		u32 msg_dword_2;
	};

	u32 ml_peer_flags;
};

#define HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_M		0x0000003F
#define HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_S		0
#define HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_M	0x00000FC0
#define HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_S	6
#define HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_M			0x0FFFF000
#define HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_S			12

#define HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_GET(_var) \
	(((_var) & HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_M) >> \
	 HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_S)

#define HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD, _val); \
		((_var) &= ~(HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_EXT_DETAILS_PEER_ASSOC_IPC_RECVD_S)); \
	} while (0)

#define HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_GET(_var) \
	(((_var) & HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_M) >> \
	 HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_S)

#define HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD, _val); \
		((_var) &= ~(HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_EXT_DETAILS_SCHED_PEER_DELETE_RECVD_S)); \
	} while (0)

#define HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_GET(_var) \
	(((_var) & HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_M) >> \
	 HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_S)

#define HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX, _val); \
		((_var) &= ~(HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_M)); \
		((_var) |= ((_val) << HTT_ML_PEER_EXT_DETAILS_MLD_AST_INDEX_S)); \
	} while (0)

struct htt_ml_peer_ext_details_tlv {
	union {
		struct {
			u32 peer_assoc_ipc_recvd:6,
			    sched_peer_delete_recvd:6,
			    mld_ast_index:16,
			    reserved:4;
		};
		u32 msg_dword_1;
	};
};

#define HTT_ML_LINK_INFO_VALID_M		0x00000001
#define HTT_ML_LINK_INFO_VALID_S		0
#define HTT_ML_LINK_INFO_ACTIVE_M		0x00000002
#define HTT_ML_LINK_INFO_ACTIVE_S		1
#define HTT_ML_LINK_INFO_PRIMARY_M		0x00000004
#define HTT_ML_LINK_INFO_PRIMARY_S		2
#define HTT_ML_LINK_INFO_ASSOC_LINK_M		0x00000008
#define HTT_ML_LINK_INFO_ASSOC_LINK_S		3
#define HTT_ML_LINK_INFO_CHIP_ID_M		0x00000070
#define HTT_ML_LINK_INFO_CHIP_ID_S		4
#define HTT_ML_LINK_INFO_IEEE_LINK_ID_M		0x00007F80
#define HTT_ML_LINK_INFO_IEEE_LINK_ID_S		7
#define HTT_ML_LINK_INFO_HW_LINK_ID_M		0x00038000
#define HTT_ML_LINK_INFO_HW_LINK_ID_S		15
#define HTT_ML_LINK_INFO_LOGICAL_LINK_ID_M	0x000C0000
#define HTT_ML_LINK_INFO_LOGICAL_LINK_ID_S	18
#define HTT_ML_LINK_INFO_MASTER_LINK_M		0x00100000
#define HTT_ML_LINK_INFO_MASTER_LINK_S		20
#define HTT_ML_LINK_INFO_ANCHOR_LINK_M		0x00200000
#define HTT_ML_LINK_INFO_ANCHOR_LINK_S		21
#define HTT_ML_LINK_INFO_INITIALIZED_M		0x00400000
#define HTT_ML_LINK_INFO_INITIALIZED_S		22
#define HTT_STATS_ML_LINK_INFO_BRIDGE_PEER BIT(23)

#define HTT_ML_LINK_INFO_SW_PEER_ID_M		0x0000ffff
#define HTT_ML_LINK_INFO_SW_PEER_ID_S		0
#define HTT_ML_LINK_INFO_VDEV_ID_M		0x00ff0000
#define HTT_ML_LINK_INFO_VDEV_ID_S		16
#define HTT_STATS_ML_LINK_INFO_PS_STATE  BIT(24)

#define HTT_ML_LINK_INFO_VALID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_VALID_M) >> \
	 HTT_ML_LINK_INFO_VALID_S)

#define HTT_ML_LINK_INFO_VALID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_VALID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_VALID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_VALID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_ACTIVE_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_ACTIVE_M) >> \
	 HTT_ML_LINK_INFO_ACTIVE_S)

#define HTT_ML_LINK_INFO_ACTIVE_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_ACTIVE, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_ACTIVE_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_ACTIVE_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_PRIMARY_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_PRIMARY_M) >> \
	 HTT_ML_LINK_INFO_PRIMARY_S)

#define HTT_ML_LINK_INFO_PRIMARY_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_PRIMARY, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_PRIMARY_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_PRIMARY_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_ASSOC_LINK_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_ASSOC_LINK_M) >> \
	 HTT_ML_LINK_INFO_ASSOC_LINK_S)

#define HTT_ML_LINK_INFO_ASSOC_LINK_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_ASSOC_LINK, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_ASSOC_LINK_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_ASSOC_LINK_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_CHIP_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_CHIP_ID_M) >> \
	 HTT_ML_LINK_INFO_CHIP_ID_S)

#define HTT_ML_LINK_INFO_CHIP_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_CHIP_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_CHIP_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_CHIP_ID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_IEEE_LINK_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_IEEE_LINK_ID_M) >> \
	 HTT_ML_LINK_INFO_IEEE_LINK_ID_S)

#define HTT_ML_LINK_INFO_IEEE_LINK_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_IEEE_LINK_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_IEEE_LINK_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_IEEE_LINK_ID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_HW_LINK_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_HW_LINK_ID_M) >> \
	 HTT_ML_LINK_INFO_HW_LINK_ID_S)

#define HTT_ML_LINK_INFO_HW_LINK_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_HW_LINK_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_HW_LINK_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_HW_LINK_ID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_LOGICAL_LINK_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_LOGICAL_LINK_ID_M) >> \
	 HTT_ML_LINK_INFO_LOGICAL_LINK_ID_S)

#define HTT_ML_LINK_INFO_LOGICAL_LINK_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_LOGICAL_LINK_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_LOGICAL_LINK_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_LOGICAL_LINK_ID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_MASTER_LINK_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_MASTER_LINK_M) >> \
	 HTT_ML_LINK_INFO_MASTER_LINK_S)

#define HTT_ML_LINK_INFO_MASTER_LINK_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_MASTER_LINK, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_MASTER_LINK_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_MASTER_LINK_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_ANCHOR_LINK_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_ANCHOR_LINK_M) >> \
	 HTT_ML_LINK_INFO_ANCHOR_LINK_S)

#define HTT_ML_LINK_INFO_ANCHOR_LINK_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_ANCHOR_LINK, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_ANCHOR_LINK_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_ANCHOR_LINK_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_INITIALIZED_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_INITIALIZED_M) >> \
	 HTT_ML_LINK_INFO_INITIALIZED_S)

#define HTT_ML_LINK_INFO_INITIALIZED_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_INITIALIZED, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_INITIALIZED_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_INITIALIZED_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_SW_PEER_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_SW_PEER_ID_M) >> \
	 HTT_ML_LINK_INFO_SW_PEER_ID_S)

#define HTT_ML_LINK_INFO_SW_PEER_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_SW_PEER_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_SW_PEER_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_SW_PEER_ID_S)); \
	} while (0)

#define HTT_ML_LINK_INFO_VDEV_ID_GET(_var) \
	(((_var) & HTT_ML_LINK_INFO_VDEV_ID_M) >> \
	 HTT_ML_LINK_INFO_VDEV_ID_S)

#define HTT_ML_LINK_INFO_VDEV_ID_SET(_var, _val) \
	do { \
		HTT_CHECK_SET_VAL(HTT_ML_LINK_INFO_VDEV_ID, _val); \
		((_var) &= ~(HTT_ML_LINK_INFO_VDEV_ID_M)); \
		((_var) |= ((_val) << HTT_ML_LINK_INFO_VDEV_ID_S)); \
	} while (0)

struct htt_ml_link_info_tlv {
	union {
		struct {
			u32 valid:1,
			    active:1,
			    primary:1,
			    assoc_link:1,
			    chip_id:3,
			    ieee_link_id:8,
			    hw_link_id:3,
			    logical_link_id:2,
			    master_link:1,
			    anchor_link:1,
			    initialized:1,
			    bridge_peer     : 1,
			    reserved        : 8;
		};
		u32 msg_dword_1;
	};

	union {
		struct {
			u32 sw_peer_id:16,
			    vdev_id:8,
			    ps              : 1,
			    reserved1       : 7;
		};
		u32 msg_dword_2;
	};

	u32 primary_tid_mask;
};

#define HTT_STATS_HDS_PROF_STATS_CIRCULAR_BUF_LEN 10

struct htt_stats_hds_prof_stats_tlv {
	struct {
		struct {
			__le32 bandwidth_mhz:16;
			__le32 band_center_freq1:16; /* MHz units */
		};
		struct {
			__le32 phyMode:8;     /* phyMode - WLAN_PHY_MODE enum type */
			__le32 txChainmask:8;
			__le32 rxChainmask:8;
			__le32 swProfile:8;
		};
		__le32 channelSwitchTime;
		__le32 calModuleTime;
		__le32 iniModuleTime;
		__le32 tpcModuleTime;
		__le32 miscModuleTime;
		__le32 ctlModuleTime;
		__le32 reserved;
	} channelChange_stats[HTT_STATS_HDS_PROF_STATS_CIRCULAR_BUF_LEN];
	__le32 idx; /* shows how many channel changes have occurred */
};

struct htt_stats_optional_configs_stats_tlv {
	union {
		u32 flags;
		struct {
			/*
			 * bit 0: Dynamic ED CCA status flag
			 * bit 1: LPI status flag
			 * bit 2: Green Tx status flag
			 * bit 3: ANI status flag
			 * bit 4: Static ANI status flag
			 * bit 5: ANN Powerboost status flag
			 * bit 6: EANI status flag
			 * bit 7: Spur Mitigation status flag
			 * bit 8: Multigain RSSI status flag
			 */
			u32 is_dyn_cca_enabled:1;        /* bit 0 */
			u32 is_lpi_enabled:1;            /* bit 1 */
			u32 is_gtx_enabled:1;            /* bit 2 */
			u32 is_ani_enabled:1;            /* bit 3 */
			u32 is_static_ani_enabled:1;     /* bit 4 */
			u32 is_ann_pbt_enabled:1;        /* bit 5 */
			u32 is_e_ani_enabled:1;          /* bit 6 */
			u32 is_spur_mit_enabled:1;       /* bit 7 */
			u32 is_multigain_rssi_enabled:1; /* bit 8 */
			u32 is_olpc_clpc:1;              /* bit 9  */
			u32 is_abi_en_dis:1;             /* bit 10 */
			u32 is_str_sig_rec:1;            /* bit 11 */
			u32 is_verbose_enabled:1;        /* bit 12 */
			u32 is_dyn_ant_sel_supported:1;  /* bit 13 */
			u32 is_dfs_punc_enabled:1;       /* bit 14 */
			u32 dpd_pow_bo_status:1;         /* bit 15 */
			u32 is_eeprom_compressed:1;      /* bit 16 */
			u32 reserved:15;                 /* bits 31:17 */
		};
	};

	/* RxSOP config word */
	union {
		u32 rxsop_config;
		struct {
			u32 is_rxsop_enabled:1;   /* bit 0 */
			u32 rxsop_config_value:8; /* bits 8:1 */
			u32 rxsop_reserved:23;    /* bits 31:9 */
		};
	};

	u32 abi_max_rg_gain;          /* dB units */
	u32 abi_max_lg_gain;          /* dB units */
	u32 abi_max_vlg_gain;         /* dB units */

	/* abi_hold_mode:
	 * Controls how long gain table decisions are held:
	 *   0: Cleared each packet
	 *   1: Held for timer value
	 *   2: Gain table choices are under CSR control
	 */
	u32 abi_hold_mode;
	u32 abi_hold_count;

	s32 rssi_temp_offset;         /* dB units */
	s32 rssi_xlna_bypass_offset;  /* dB units */
	s32 rssi_cbw_offset;          /* dB units */
	s32 rssi_chan_freq_offset;    /* dB units */

	u32 otp_version;

	u32 cfr_clip_factor[4];

	/* Heavy-clip premcs & pkt_type packed word */
	union {
		u32 hc__premcs__pkt_type__word32;
		struct {
			u32 hc_premcs:16;   /* bits 15:0  */
			u32 hc_pkt_type:16; /* bits 31:16 */
		};
	};

	/* Heavy-clip NSS threshold + rx-gain forced fields packed word */
	union {
		u32 hc_nss_thr__is_rx_gain_forced__rx_gain_forced_val__word32;
		struct {
			u32 hc_nss_thr:8;         /* bits 7:0   */
			u32 is_rx_gain_forced:8;  /* bits 15:8  */
			u32 rx_gain_forced_val:8; /* bits 23:16 */
			u32 reserved2:8;          /* bits 31:24 */
		};
	};
} __packed;

#define HTT_STATS_OPT_CONF_DYN_CCA        BIT(0)	/*Dynamic ED CCA Mask*/
#define HTT_STATS_OPT_CONF_LPI            BIT(1)	/*LPI Mask*/
#define HTT_STATS_OPT_CONF_GTX            BIT(2)	/*Green Tx Mask*/
#define HTT_STATS_OPT_CONF_ANI            BIT(3)	/*ANI Mask*/
#define HTT_STATS_OPT_CONF_STATIC_ANI     BIT(4)	/*Static ANI Mask*/
#define HTT_STATS_OPT_CONF_ANN_PBT        BIT(5)	/*ANN Powerboost Mask*/
#define HTT_STATS_OPT_CONF_EANI           BIT(6)	/*EANI Mask*/
#define HTT_STATS_OPT_CONF_SPUR_MIT       BIT(7)	/*Spur Mitigation Mask*/
#define HTT_STATS_OPT_CONF_MULTIGAIN_RSSI BIT(8)	/*Multigain RSSI Mask*/
#define HTT_STATS_OPT_CONF_OLPC_CLPC           BIT(9)  /* OLPC / CLPC */
#define HTT_STATS_OPT_CONF_ABI_EN_DIS          BIT(10) /* ABI enable/disable */
#define HTT_STATS_OPT_CONF_STR_SIG_REC         BIT(11) /* Strong signal recovery */
#define HTT_STATS_OPT_CONF_VERBOSE             BIT(12) /* Verbose mode */
#define HTT_STATS_OPT_CONF_DYN_ANT_SEL         BIT(13) /* Dynamic antenna select */
#define HTT_STATS_OPT_CONF_DFS_PUNC            BIT(14) /* DFS puncturing */
#define HTT_STATS_OPT_CONF_DPD_POW_BO          BIT(15) /* DPD power backoff */
#define HTT_STATS_OPT_CONF_EEPROM_COMPRESSED  BIT(16) /* EEPROM compressed */
#define HTT_STATS_OPT_CONF_RXSOP_ENABLE_BIT        BIT(0)
#define HTT_STATS_OPT_CONF_RXSOP_CONFIG_SHIFT     1
#define HTT_STATS_OPT_CONF_RXSOP_CONFIG_MASK      GENMASK(8, 1)
#define HTT_STATS_OPT_CONF_HC_PREMCS_MASK          GENMASK(15, 0)
#define HTT_STATS_OPT_CONF_HC_PKT_TYPE_SHIFT       16
#define HTT_STATS_OPT_CONF_HC_PKT_TYPE_MASK        GENMASK(31, 16)
#define HTT_STATS_OPT_CONF_HC_NSS_THR_SHIFT        0
#define HTT_STATS_OPT_CONF_HC_NSS_THR_MASK         GENMASK(7, 0)
#define HTT_STATS_OPT_CONF_IS_RX_GAIN_FORCED_SHIFT 8
#define HTT_STATS_OPT_CONF_IS_RX_GAIN_FORCED_MASK  GENMASK(15, 8)
#define HTT_STATS_OPT_CONF_RX_GAIN_FORCED_VAL_SHIFT 16
#define HTT_STATS_OPT_CONF_RX_GAIN_FORCED_VAL_MASK  GENMASK(23, 16)

struct ath12k_htt_stats_txbf_ofdma_be_parbw_tlv {
	__le32 be_ofdma_parbw_user_snd;
	__le32 be_ofdma_parbw_cv;
	__le32 be_ofdma_total_cv;
} __packed;

struct ath12k_htt_stats_txbf_ofdma_bn_parbw_tlv {
	__le32 bn_ofdma_parbw_user_snd;
	__le32 bn_ofdma_parbw_cv;
	__le32 bn_ofdma_total_cv;
} __packed;

enum {
	ATH12K_HTT_STATS_CAL_PROF_COLD_BOOT = 0,
	ATH12K_HTT_STATS_CAL_PROF_FULL_CHAN_SWITCH = 1,
	ATH12K_HTT_STATS_CAL_PROF_SCAN_CHAN_SWITCH = 2,
	ATH12K_HTT_STATS_CAL_PROF_DPD_SPLIT_CAL = 3,
	ATH12K_HTT_STATS_MAX_PROF_CAL = 4,
};

enum htt_ctrl_path_stats_cal_type_ids {
	HTT_CTRL_PATH_STATS_CAL_TYPE_ADC                     = 0x0,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DAC                     = 0x1,
	HTT_CTRL_PATH_STATS_CAL_TYPE_PROCESS                 = 0x2,
	HTT_CTRL_PATH_STATS_CAL_TYPE_NOISE_FLOOR             = 0x3,
	HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO                   = 0x4,
	HTT_CTRL_PATH_STATS_CAL_TYPE_COMB_TXLO_TXIQ_RXIQ     = 0x5,
	HTT_CTRL_PATH_STATS_CAL_TYPE_TXLO                    = 0x6,
	HTT_CTRL_PATH_STATS_CAL_TYPE_TXIQ                    = 0x7,
	HTT_CTRL_PATH_STATS_CAL_TYPE_RXIQ                    = 0x8,
	HTT_CTRL_PATH_STATS_CAL_TYPE_IM2                     = 0x9,
	HTT_CTRL_PATH_STATS_CAL_TYPE_LNA                     = 0xa,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXDCO            = 0xb,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXIQ             = 0xc,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORYLESS          = 0xd,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORY              = 0xe,
	HTT_CTRL_PATH_STATS_CAL_TYPE_IBF                     = 0xf,
	HTT_CTRL_PATH_STATS_CAL_TYPE_PDET_AND_PAL            = 0x10,
	HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_IQ                = 0x11,
	HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_DTIM              = 0x12,
	HTT_CTRL_PATH_STATS_CAL_TYPE_TPC_CAL                 = 0x13,
	HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_TIMEREQ             = 0x14,
	HTT_CTRL_PATH_STATS_CAL_TYPE_BWFILTER                = 0x15,
	HTT_CTRL_PATH_STATS_CAL_TYPE_PEF                     = 0x16,
	HTT_CTRL_PATH_STATS_CAL_TYPE_PADROOP                 = 0x17,
	HTT_CTRL_PATH_STATS_CAL_TYPE_SELFCALTPC              = 0x18,
	HTT_CTRL_PATH_STATS_CAL_TYPE_RXSPUR                  = 0x19,
	HTT_CTRL_PATH_STATS_CAL_TYPE_INVALID                 = 0xFF
};

#define ATH12K_HTT_STATS_MAX_CAL_IDX_CNT 8

#define ATH12K_HTT_GET_BITS(_val, _index, _num_bits) \
	(((_val) >> (_index)) & ((1 << (_num_bits)) - 1))

#define ATH12K_HTT_CTRL_PATH_CALIBRATION_STATS_CAL_TYPE_GET(cal_info) \
	ATH12K_HTT_GET_BITS(cal_info, 0, 8)

#ifdef HTT_CTRL_PATH_STATS_CAL_TYPE_STRINGS
static inline u8 *htt_ctrl_path_cal_type_id_to_name(u32 cal_type_id)
{
	switch (cal_type_id) {
	case HTT_CTRL_PATH_STATS_CAL_TYPE_ADC:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_ADC";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DAC:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DAC";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_PROCESS:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_PROCESS";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_NOISE_FLOOR:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_NOISE_FLOOR";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_COMB_TXLO_TXIQ_RXIQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_COMB_TXLO_TXIQ_RXIQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_TXLO:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_TXLO";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_TXIQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_TXIQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_RXIQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_RXIQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_IM2:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_IM2";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_LNA:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_LNA";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXDCO:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXDCO";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXIQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_LP_RXIQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORYLESS:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORYLESS";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORY:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_MEMORY";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_IBF:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_IBF";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_PDET_AND_PAL:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_PDET_AND_PAL";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_IQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_IQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_DTIM:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_RXDCO_DTIM";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_TPC_CAL:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_TPC_CAL";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_TIMEREQ:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_DPD_TIMEREQ";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_BWFILTER:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_BWFILTER";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_PEF:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_PEF";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_PADROOP:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_PADROOP";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_SELFCALTPC:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_SELFCALTPC";
	case HTT_CTRL_PATH_STATS_CAL_TYPE_RXSPUR:
		return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_RXSPUR";
	default: return (u8 *)"HTT_CTRL_PATH_STATS_CAL_TYPE_UNKNOWN";
	}
}
#endif

struct ath12k_htt_stats_latency_prof_cal_data_tlv {
	__le32 enable;
	__le32 pdev_id;
	__le32 cal_cnt[ATH12K_HTT_STATS_MAX_PROF_CAL];
	u8 latency_prof_name[ATH12K_HTT_STATS_MAX_PROF_CAL]
[ATH12K_HTT_STATS_MAX_PROF_STATS_NAME_LEN];
	struct {
	__le32 cnt;
	__le32 min;
	__le32 max;
	__le32 last;
	__le32 tot;
	__le32 hist_intvl;
	__le32 hist[ATH12K_HTT_INTERRUPTS_LATENCY_PROFILE_MAX_HIST];
	__le32 pf_last;
	__le32 pf_tot;
	__le32 pf_max;
	__le32 enabled_cal_idx;
	} latency_data[ATH12K_HTT_STATS_MAX_PROF_CAL][ATH12K_HTT_STATS_MAX_CAL_IDX_CNT];
};


#define ATH12K_HTT_STATS_TPCCAL_LAST_IDX_M		GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_STATS_MEASPWR_M		GENMASK(15, 0)

#define ATH12K_HTT_STATS_TPCCAL_STATS_PDADC_M		GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_STATS_CHANNEL_M		GENMASK(15, 0)

#define ATH12K_HTT_STATS_TPCCAL_STATS_CHAIN_M		GENMASK(23, 16)

#define ATH12K_HTT_STATS_TPCCAL_STATS_GAININDEX_M	GENMASK(31, 24)

#define ATH12K_HTT_STATS_TPCCAL_POSTPROC_CHANNEL_M	GENMASK(15, 0)

#define ATH12K_HTT_STATS_TPCCAL_POSTPROC_CHAIN_M	GENMASK(23, 16)

#define ATH12K_HTT_STATS_TPCCAL_POSTPROC_BAND_M		GENMASK(31, 24)

#define ATH12K_HTT_STATS_TPCCAL_POSTPROC_NUMGAIN_M	GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_POSTPROC_CALDBSTATUS_M	GENMASK(15, 8)

#define ATH12K_HTT_MAX_TPCCAL_STATS 25
#define ATH12K_HTT_STATS_TPC_CAL_MAX_NUM_POINTS 64
#define ATH12K_HTT_STATS_NUM_CAL_GAINS_SELECTED_4_GLUT 16
#define ATH12K_HTT_STATS_NUM_CAL_GAINS_SELECTED_4_PLUT 10


struct ath12k_htt_stats_pdev_ftm_tpccal_tlv {
	union {
		u32 dword__tpccal_last_idx;
		struct {
			u32 tpccal_last_idx:8,
				rsvd1:24;
		};
	};

	struct {
		union {
			s32 dword__measPwr;
			struct {
				u32 measPwr:16, /* dBm units */
					rsvd2:16;
			};
		};

		union {
			u32 dword__channel_chain_gainIndex;
			struct {
				u32 channel:16, /* MHz units */
					chain:8,
					gainIndex:8;
			};
		};

		union {
			u32 dword__pdadc;
			struct {
				u32 pdadc:8,
					rsvd3:24;
			};
		};
	} tpccal_stats[ATH12K_HTT_MAX_TPCCAL_STATS];

	struct {
		u32 calStatus;
		s32  measPwr[ATH12K_HTT_STATS_TPC_CAL_MAX_NUM_POINTS]; /* dBm units */
		u32 pdadc[ATH12K_HTT_STATS_TPC_CAL_MAX_NUM_POINTS];
		u32 gainIndex[ATH12K_HTT_STATS_TPC_CAL_MAX_NUM_POINTS];
		union {
			u32 dword__channel_chain_band;
			struct {
				u32 channel:16, /* MHz units */
				chain:8,
				band:8; /* 0: 2GHz, 1: 5GHz, 2: 6GHz */
			};
		};

		union {
			u32 dword__numgain_caldbStatus;
			struct {
				u32 numgain:8,
					caldbStatus:8,
					rsvd4:16;
			};
		};
	} tpccal_stats_postproc;
	/*cal DB timeout in ms*/
	__le32 cal_db_timeout_ms;
	struct {
		s32 tgt_meas_pwr[ATH12K_HTT_STATS_NUM_CAL_GAINS_SELECTED_4_GLUT];
		__le32 tgt_pdadc[ATH12K_HTT_STATS_NUM_CAL_GAINS_SELECTED_4_PLUT];
	} tpc_targets;
} __packed;

#define ATH12K_HTT_STATS_TPCCAL_PDADC_LAST_IDX		GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_PDADC_NUMGAIN		GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_PDADC_BAND		GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_PDADC_CHANNEL		GENMASK(23, 8)

#define ATH12K_HTT_STATS_TPCCAL_PDADC_CHAIN		GENMASK(31, 24)

#define ATH12K_HTT_STATS_TPCCAL_RES_PDADC_GAINIDX	GENMASK(7, 0)

#define ATH12K_HTT_STATS_TPCCAL_RES_PDADC_VAL		GENMASK(15, 8)

#define ATH12K_HTT_STATS_TPCCAL_RES_PDADC_MEASPWR	GENMASK(15, 0)

#define ATH12K_HTT_STATS_TPC_CAL_PDADC_BUF_LEN 3

#define ATH12K_HTT_STATS_TPCCALRSP_MISCFLAGS_CALERROR_PLUT_NON_LINEAR     BIT(17)
#define ATH12K_HTT_STATS_TPCCALRSP_MISCFLAGS_CALERROR_PLUT_NOT_FILLED     BIT(19)

struct ath12k_htt_stats_pdev_ftm_tpccal_ext_tlv {
	/*
	 * cal_status can be intrepreted with the below values:
	 *   TPCCAL_CALDATA                                 (1 << 0)
	 *   TPCCAL_CALINFO                                 (1 << 1)
	 *   TPCCAL_CALERROR                                (1 << 2)
	 *   bits 6:4 - reserved
	 *   TPCCAL_DONE_MASK                               (1 << 7)
	 *   bits 15:8 - reserved
	 *   TPCCALRSP_MISCFLAGS_CALERROR_GLUTS_NOT_FILLED  (1 << 16)
	 *   TPCCALRSP_MISCFLAGS_CALERROR_PLUT_NON_LINEAR   (1 << 17)
	 *   TPCCALRSP_MISCFLAGS_CALERROR_ATTEMPTS_EXCEEDED (1 << 18)
	 *   TPCCALRSP_MISCFLAGS_CALERROR_PLUT_NOT_FILLED   (1 << 19)
	 *   bits 31:20 - reserved
	 */
	__le32 cal_status;
	/* dword__tpccal_pdadc_last_idx:
	 * Hold the last updated index for pdadc buffer which holds
	 * band,chain,chan_idx,gain_idx for pdadc non-linearity
	 * BIT [7 : 0]   :- tpcccal_pdadc_last_idx
	 * BIT [31 : 8]  :- rsvd
	 */
	union {
		__le32 dword__tpccal_pdadc_last_idx;
		struct {
			__le32 tpccal_pdadc_last_idx:8,
			    rsvd:24;
		};
	};
	struct {
		/* dword__tpccal_pdadc_numgain:
		 * Hold the number of gain_idx for band for which
		 * PDADC non-linearity is observed
		 * BIT [7 : 0]   :- tpccal_pdadc_numgain
		 * BIT [31 : 8]  :- rsvd1
		 */
		union {
			__le32 dword__tpccal_pdadc_numgain;
			struct {
				__le32 tpccal_pdadc_numgain:8,
				    rsvd1:24;
			};
		};
		/*
		 * dword__band_channel_chain:
		 * band, channel, chain for which pdadc is non-linear
		 * BIT [7 : 0]   :- band
		 * BIT [23 : 8]  :- channel
		 * BIT [31 : 24] :- chain
		 */
		union {
			__le32 dword__band_channel_chain;
			struct {
				__le32 band:8,
				    channel:16,
				    chain:8;
			};
		};
	} tpccal_stats_pdadc[ATH12K_HTT_STATS_TPC_CAL_PDADC_BUF_LEN];
	/*
	 * tpccal_tpc_cal_res
	 * holds meas_pwr, pdadc, gain_index from tpcCalResult for band, channel,
	 * chain, gain_idx for which pdadc is non-linear
	 * BIT [7 : 0]   :- gain_idx
	 * BIT [15 : 8]  :- pdadc
	 * BIT [31 : 16] :- rsvd
	 */
	struct {
		union {
			__le32 dword_calres;
			struct {
				__le32 gain_idx:8,
				    pdadc:8,
				    rsvd:16;
			};
		};
		/*
		 * dword__meas_pwr:
		 * BIT [15 : 0]  :- meas_pwr
		 * BIT [31 : 16] :- rsvd3
		 */
		union {
			s32 dword_meas_pwr;
			struct {
				s32 meas_pwr:16, /* dBm units */
				    rsvd3:16;
			};
		};
	} tpccal_tpc_cal_res[ATH12K_HTT_STATS_TPC_CAL_PDADC_BUF_LEN]
				   [ATH12K_HTT_STATS_TPC_CAL_MAX_NUM_POINTS];
} __packed;

/* ================= dword_txparams ================= */
#define ATH12K_HTT_STATS_CWTONE_GAIN_IDX               GENMASK(7, 0)
#define ATH12K_HTT_STATS_INFINITE_BURSTING_MODE        GENMASK(15, 8)
#define ATH12K_HTT_STATS_FTM_MODE                      GENMASK(23, 16)
#define ATH12K_HTT_STATS_SIFS_US                       GENMASK(31, 24)

/* ================= dword_txparams_ext_1 ================= */
#define ATH12K_HTT_STATS_AGG_STATUS                    GENMASK(0, 0)
#define ATH12K_HTT_STATS_DPD_FLAG                      GENMASK(1, 1)
#define ATH12K_HTT_STATS_PPDU_DUR_BUF_LAST_IDX         GENMASK(9, 2)

/* ================= dword_txparams_ext_2 ================= */
#define ATH12K_HTT_STATS_TX_DUTY                       GENMASK(7, 0)
#define ATH12K_HTT_STATS_PPDU_TYPE                     GENMASK(15, 8)
#define ATH12K_HTT_STATS_NUM_USERS_OFDMA_TONE_PLAN     GENMASK(23, 16)
#define ATH12K_HTT_STATS_NUM_USERS_OFDMA_TONE_PLAN_EHT GENMASK(31, 24)

/* ================= dword_TimingStats ================= */
#define ATH12K_HTT_STATS_TIMESTAMP_CNT                 GENMASK(7, 0)

/* ================= dword_TlvcmdInfo ================= */
#define ATH12K_HTT_STATS_CMD_ENTRY                     GENMASK(15, 0)
#define ATH12K_HTT_STATS_CAL_TYPE                      GENMASK(23, 16)

/* ================= dword_TlvCmdParsing ================= */
#define ATH12K_HTT_STATS_FLAG_CMD_PARSING              GENMASK(0, 0)
#define ATH12K_HTT_STATS_NUM_TLV_CMD_DROPPED           GENMASK(8, 1)

/* ================= dword_rxGaincalMaxNumchan ================= */
#define ATH12K_HTT_STATS_RXGAINCAL_MAX_NUMCHAN         GENMASK(7, 0)

/* ================= dword_rxgainCalRefISS ================= */
#define ATH12K_HTT_STATS_RXGAINCAL_REF_ISS             GENMASK(7, 0)

#define ATH12K_HTT_STATS_FTM_TIMESTAMP_TOTAL_CNT 50
#define ATH12K_HTT_STATS_FTM_PPDU_DUR_CIRCULAR_BUF_CNT 10
#define ATH12K_HTT_STATS_FTM_RXGAINCAL_MAX_NUM_CHAN_TLV2 16
#define ATH12K_HTT_STATS_FTM_FPC_TIMING_SHIFT 32

struct ath12k_htt_stats_ftm_tlv {
	/* TX PARAMS
	 *
	 * BIT [7 : 0]   :- gain idx used for CW tone
	 * BIT [15 : 8]  :- infinite_bursting_mode
	 * BIT [23 : 16] :- ftm_mode
	 * BIT [31 : 24] :- sifs_us
	 */
	struct {
		union {
			__le32 dword_txparams;
			struct {
				__le32 cwtone_gain_idx:8,
				    infinite_bursting_mode:8,
				    ftm_mode:8,
				    sifs_us:8;
			};
		};
		/* dword_txparams_ext_1
		 * BIT [0]       :- agg_status
		 * BIT [1]       :- dpd_flag
		 * BIT [9 : 2]   :- ppdu_dur_buf_last_idx (last idx of ppdu
		 *                  duration circular buffer - 'actual_ppdu_dur_us')
		 * BIT [31 : 10] :- rsvd
		 */
		union {
			__le32 dword_txparams_ext_1;
			struct {
				__le32 agg_status:1,
				    dpd_flag:1,
				    ppdu_dur_buf_last_idx:8,
				    rsvd:22;
			};
		};
		__le32 actual_ppdu_dur_us[ATH12K_HTT_STATS_FTM_PPDU_DUR_CIRCULAR_BUF_CNT];
		__le32 target_fes_duration_us;
		__le32 target_txoff_duration_us;
		__le32 phy_mode;
		__le32 pkt_len_post_truncation;
		/* dword_txparams_ext_2
		 * BIT [7 : 0]   :- target_tx_duty (TX ON ratio)
		 * BIT [15 : 8]  :- ppdu_type
		 * BIT [23 : 16] :- num_users_ofdma_tone_plan
		 * BIT [31 : 24] :- num_users_ofdma_tone_plan_eht
		 */
		union {
			__le32 dword_txparams_ext_2;
			struct {
				__le32 target_tx_duty:8,
				    ppdu_type:8,
				    num_users_ofdma_tone_plan:8,
				    num_users_ofdma_tone_plan_eht:8;
			};
		};
	} tx_params;
	/*CALIBRATION VERIFICATION FW ONLY TIMING STATS:*/
	struct {
		__le32 ts_fpc_high;
		__le32 ts_fpc_low;
		__le32 ts_cal_db_regen_time;
		__le32 ts_opc;
		__le32 ts_xtalcal;
		__le32 ts_rxgaincal;
		__le32 ts_nfcal;
		__le32 ts_tx_ver;
		__le32 ts_rx_ver;
		/*Overall channel change time for tx/rx verification*/
		__le32 tx_ver_overall_chan_change_timing_ts;
		__le32 rx_ver_overall_chan_change_timing_ts;
		/*dword_timing_stats
		 * Number of TLVs for which tlv_cmd_info to be dumped
		 * BIT [7:0]      :- tlv_timestamp_cnt_filled
		 * BIT [31 :8]   :- rsvd1
		 */
		union {
			__le32 dword_timing_stats;
			struct {
				__le32 tlv_timestamp_cnt_filled:8,
				    rsvd1:24;
			};
		};
		/*TLV cmd and timing info */
		struct {
			__le32 tlv_tim_del;
			/*dword_tlv_cmd_info
			 * TLV CMD ID and cal_type corresponding to TLV
			 * BIT [15:0]       :- tlv_cmd_entry - TLV cmd ID
			 * BIT [23 : 16]    :- tlv_cal_type - cal_type
			 * BIT [31 : 24]    :- rsvd2
			 */
			union {
				__le32 dword_tlv_cmd_info;
				struct {
					__le32 tlv_cmd_entry:16,
					    tlv_cal_type:8,
					    rsvd2:8;
				};
			};
		} tlv_cmd_tim_info[ATH12K_HTT_STATS_FTM_TIMESTAMP_TOTAL_CNT];
	} timing_stats;
	/*TLV params*/
	struct {
		__le32 last_tlv_num_params;
		__le32 last_tlv_num_params_parsed;
		__le32 last_tlv_cmd;
		__le32 last_tlv_rsp_cmd;
		/*dword_tlv_cmd_parsing
		 * BIT [0]         :- flag_tlv_cmd_parsing
		 * BIT [8 : 1]     :- num_tlv_cmd_dropped
		 * BIT [31 : 9]    :- rsvd3
		 */
		union {
			__le32 dword_tlv_cmd_parsing;
			struct {
				__le32 flag_tlv_cmd_parsing:1,
				    num_tlv_cmd_dropped:8,
				    rsvd3:23;
			};
		};
		__le32 last_tlv_cmd_dropped;
	} tlv_params;
	/*RX gain cal params*/
	/*dword_rx_gain_cal_max_num_chan
	 * Num channels for which good_pkt_cnt for RX gain cal to be dumped
	 * BIT [7 : 0]     :- rx_gain_cal_max_num_chan
	 * BIT [31 : 8]    :- rsvd4
	 */
	struct {
		union {
			__le32 dword_rx_gain_cal_max_num_chan;
			struct {
				__le32 rx_gain_cal_max_num_chan:8,
				    rsvd4:24;
			};
		};
		__le32 good_pkt_cnt[ATH12K_HTT_STATS_FTM_RXGAINCAL_MAX_NUM_CHAN_TLV2];
		/*dword_rx_gain_cal_ref_iss
		 * Reference Input Signal Strength for Rx gain cal in dBm
		 * BIT [7 : 0]     :- rx_gain_cal_ref_iss
		 * BIT [31 : 8]    :- rsvd
		 */
		union {
			s32 dword_rx_gain_cal_ref_iss;
			struct {
				s32 rx_gain_cal_ref_iss:8,
				    pad:24;
			};
		};
	} rx_gain_cal_params;
	/*Miscallenous params*/
	__le32 bd_read_rsp;
} __packed;

#define ATH12K_HTT_STATS_NUM_ANI_HIST_TYPE_1SEC 0
#define ATH12K_HTT_STATS_NUM_ANI_HIST_TYPE_GRANULAR 1
#define ATH12K_HTT_STATS_NUM_ANI_HIST_TYPE_1SEC_NOT_ENABLED 2
#define ATH12K_HTT_STATS_NUM_ANI_HIST_TYPE_GRANULAR_NOT_ENABLED 3
#define ATH12K_HTT_STATS_NUM_ANI_HIST_RX_PKT_CNT_MAX 9
#define ATH12K_HTT_STATS_NUM_ANI_HIST_RX_PKT_CRC_PASS_CNT_MAX 9
#define ATH12K_HTT_STATS_NUM_ANI_HIST_PER_BLK_ERR_CNT_MAX 16
#define ATH12K_HTT_STATS_NUM_ANI_HIST_RX_OTA_ERR_CNT_MAX 10
#define ATH12K_HTT_STATS_NUM_ANI_HIST_PRINT_CNT_MAX 9

struct htt_stats_pdev_ani_hist_tlv {
	__le32 chan_num;
	__le32 num_records;
	__le32 collection_interval;
	__le32 ani_hist_type;
};

struct htt_stats_ani_scalar_tlv {
	__le32 rx_ofdma_timing_err_cnt;
	__le32 rx_cck_fail_cnt;
	__le32 rx_cck1_fail_cnt;
	__le32 rx_cck2_fail_cnt;
	__le32 rx_cck7_fail_cnt;
	__le32 mactx_abort_cnt;
	__le32 macrx_abort_cnt;
	__le32 phytx_abort_cnt;
	__le32 phyrx_abort_cnt;
	__le32 phyrx_defer_abort_cnt;
	__le32 rx_sizing1_event_cnt;
	__le32 rx_sizing2_event_cnt;
};

struct htt_stats_ani_pkt_cnt_tlv {
	__le32 rx_pkt_cnt[ATH12K_HTT_STATS_NUM_ANI_HIST_RX_PKT_CNT_MAX];
};

struct htt_stats_ani_crc_pass_tlv {
	__le32 rx_pkt_crc_pass_cnt[ATH12K_HTT_STATS_NUM_ANI_HIST_RX_PKT_CRC_PASS_CNT_MAX];
};

struct htt_stats_ani_per_blk_err_tlv {
	__le32 per_blk_err_cnt[ATH12K_HTT_STATS_NUM_ANI_HIST_PER_BLK_ERR_CNT_MAX];
};

struct htt_stats_ani_ota_err_tlv {
	__le32 rx_ota_err_cnt[ATH12K_HTT_STATS_NUM_ANI_HIST_RX_OTA_ERR_CNT_MAX];
};

struct ath12k_htt_stats_phy_paprd_pb_tlv {
	__le32 pdev_id;
	__le32 total_dpd_cal_count;
	__le32 chan_change_dpd_cal_count;
	__le32 thermal_dpd_cal_count;
	__le32 recovery_dpd_cal_count;
	__le32 pb_cal_count;
	__le32 total_dpd_fail_count;
	__le32 chan_change_dpd_fail_count;
	__le32 thermal_dpd_fail_count;
	__le32 recovery_dpd_fail_count;
	__le32 pb_fail_count;

	union {
		u32 dpd_pb_validity_status;
		struct {
			u32 is_dpd_valid:1,
				is_pb_valid:1,
				rsvd:30;
		};
	};

	__le32 last_dpd_cal_time;

	__le32 last_pb_cal_time;

	__le32 power_boost_gain
[ATH12K_HTT_TX_PDEV_STATS_NUM_BE_BW_COUNTERS]
[ATH12K_HTT_TX_PDEV_STATS_NUM_BE_MCS_COUNTERS];
} __packed;

#define HTT_STATS_PDEV_RTT_DELAY_NUM_INSTANCES (2)
#define HTT_STATS_PDEV_RTT_DELAY_PKT_BW (6)
#define HTT_STATS_PDEV_RTT_TX_RX_INSTANCES (2)
struct ath12k_htt_stats_pdev_rtt_delay_tlv {
	struct {
		__le32 base_delay[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES]
				 [HTT_STATS_PDEV_RTT_DELAY_PKT_BW];
		__le32 final_delay[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES]
				  [HTT_STATS_PDEV_RTT_DELAY_PKT_BW];
		__le32 per_chan_bias[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES];
		__le32 off_chan_bias[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES];
		__le32 chan_bw_bias[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES];
		__le32 rtt_11mc_chain_idx[HTT_STATS_PDEV_RTT_TX_RX_INSTANCES];
		__le32 chan_freq; /* MHz units */
		__le32 bandwidth; /* MHz units */
		__le32 vreg_cache;
		__le32 rtt_11mc_vreg_set_cnt;
		__le32 cfr_vreg_set_cnt;
		__le32 cir_vreg_set_cnt;
		__le32 digital_block_status;
	} rtt_delay[HTT_STATS_PDEV_RTT_DELAY_NUM_INSTANCES];
} __packed;

#define HTT_STATS_PDEV_SPECTRAL_PCFG_MAX_DET (3)
#define HTT_STATS_PDEV_SPECTRAL_MAX_PCSS_RING_FOR_IPC (3)

struct ath12k_htt_stats_pdev_spectral_tlv {
	__le32 dbg_num_buf;
	__le32 dbg_num_events;

	__le32 host_head_idx;
	__le32 host_tail_idx;
	__le32 host_shadow_tail_idx;

	__le32 in_ring_head_idx;
	__le32 in_ring_tail_idx;
	__le32 in_ring_shadow_tail_idx;
	__le32 in_ring_shadow_head_idx;

	__le32 out_ring_head_idx;
	__le32 out_ring_tail_idx;
	__le32 out_ring_shadow_tail_idx;
	__le32 out_ring_shadow_head_idx;

	struct {
		__le32 head_idx;
		__le32 tail_idx;
		__le32 shadow_tail_idx;
		__le32 shadow_head_idx;
	} ipc_rings[HTT_STATS_PDEV_SPECTRAL_MAX_PCSS_RING_FOR_IPC];

	struct {
		__le32 scan_priority;
		__le32 scan_count;
		__le32 scan_period;
		__le32 scan_chn_mask;
		__le32 scan_ena;
		__le32 scan_update_mask;
		__le32 scan_ready_intrpt;
		__le32 scans_performed;
		__le32 intrpts_sent;
		__le32 scan_pending_count;
		__le32 num_pcss_elem_zero;
		__le32 num_in_elem_zero;
		__le32 num_out_elem_zero;
		__le32 num_elem_moved;
	} pcfg_stats_det[HTT_STATS_PDEV_SPECTRAL_PCFG_MAX_DET];

	struct {
		__le32 scan_no_ipc_buf_avail;
		__le32 agile_scan_no_ipc_buf_avail;
		__le32 scan_FFT_discard_count;
		__le32 scan_recapture_FFT_discard_count;
		__le32 scan_recapture_count;
	} pcfg_stats_vreg;
} __packed;

#define HTT_STATS_PDEV_AOA_MAX_HISTOGRAM (10)
#define HTT_STATS_PDEV_AOA_MAX_CHAINS (4)
struct ath12k_htt_stats_pdev_aoa_tlv {
	__le32 gain_idx[HTT_STATS_PDEV_AOA_MAX_HISTOGRAM];
	__le32 gain_table[HTT_STATS_PDEV_AOA_MAX_HISTOGRAM];
	__le32 phase_calculated[HTT_STATS_PDEV_AOA_MAX_HISTOGRAM]
			       [HTT_STATS_PDEV_AOA_MAX_CHAINS];
	__le32 phase_in_degree[HTT_STATS_PDEV_AOA_MAX_HISTOGRAM]
			      [HTT_STATS_PDEV_AOA_MAX_CHAINS];
} __packed;

enum ath12k_htt_stats_candidate_sched_compatible_code {
	ATH12K_HTT_STATS_CANDIDATE_MU_NOT_COMPATIBLE = 1,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_NR_INDEX,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_BASIC_CHECKS_INELIGIBLE,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_ZERO_NSS,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_MCS_THRESHOLD_LIMIT,
	ATH12K_STATS_CANDIDATE_SKIP_POWER_IMBALANCE,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_NULL_MU_RC,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_CV_CORR_SKIP_PEER,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_SEND_BAR_SET_FOR_AC_MUMIMO,
	ATH12K_HTT_STATS_CANDIDATE_SKIP_REASON_MAX
};

struct ath12k_htt_stats_pdev_ulmumimo_grp_stats_tlv {
	__le32 pdev_id;
	__le32 mu_grp_eligible[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 mu_grp_ineligible[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 mu_grp_invalid[ATH12K_HTT_TX_NUM_MUMIMO_GRP_INVALID_WORDS];
	__le32 mu_grp_candidate_skip[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS]
				    [ATH12K_HTT_STATS_CANDIDATE_SKIP_REASON_MAX];
	__le32 mu_grp_eligible_1ss[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 mu_grp_ineligible_1ss[ATH12K_HTT_STATS_NUM_MAX_MUMIMO_SZ];
	__le32 mu_grp_invalid_1ss[ATH12K_HTT_TX_NUM_MUMIMO_GRP_INVALID_WORDS];
	__le32 mu_grp_candidate_skip_1ss[ATH12K_HTT_TX_NUM_AX_MUMIMO_USER_STATS]
		[ATH12K_HTT_STATS_CANDIDATE_SKIP_REASON_MAX];
} __packed;

struct ath12k_htt_stats_pdev_ulmumimo_denylist_stats_tlv {
	__le32 num_peer_denylist_cnt;
	__le32 trig_bitmap_fail_cnt;
	__le32 trig_consecutive_fail_cnt;
} __packed;

#define ATH12K_HTT_STATS_SEQ_EFFICIENCY_HISTOGRAM 10

struct ath12k_htt_stats_pdev_ulmumimo_seq_term_stats_tlv {
	__le32 num_terminate_seq;
	__le32 num_terminate_low_qdepth;
	__le32 num_terminate_seq_inefficient;
	__le32 hist_seq_efficiency[ATH12K_HTT_STATS_SEQ_EFFICIENCY_HISTOGRAM];
} __packed;

#define ATH12K_HTT_STATS_MAX_ULMUMIMO_TRIGGERS 6
#define ATH12K_HTT_STATS_TXOP_HISTOGRAM_BINS 24
#define ATH12K_HTT_STATS_ULMUMIMO_DUR_INTERVAL_US 500
#define ATH12K_HTT_STATS_ULMUMIMO_MIN_PPDU_DUR_US 1000
#define ATH12K_HTT_STATS_MAX_PPDU_DURATION_BINS 10

struct ath12k_htt_stats_pdev_ulmumimo_hist_ineligibility_tlv {
	__le32 num_triggers[ATH12K_HTT_STATS_MAX_ULMUMIMO_TRIGGERS];
	__le32 txop_history[ATH12K_HTT_STATS_TXOP_HISTOGRAM_BINS];
	/* ppdu_duration_hist:
	 * PPDU Duration History (histogram)
	 * Num PPDUs from 1 to 6
	 * 0 to 6 ms with interval of 500us
	 */
	__le32 ppdu_duration_hist[ATH12K_HTT_STATS_MAX_ULMUMIMO_TRIGGERS]
				 [ATH12K_HTT_STATS_MAX_PPDU_DURATION_BINS];
	__le32 ineligible_count;
	/* history_ineligibility:
	 * History based ineligibility counter for ULMUMIMO.
	 * Checks for 8 eligible instances of ULMUMIMO in the past 32 instances.
	 */
	__le32 history_ineligibility;
} __packed;

/*======= Bandwidth Manager stats ====================*/

#define HTT_BW_MGR_STATS_MAC_ID			GENMASK(7, 0)

#define HTT_BW_MGR_STATS_PRI20_IDX		GENMASK(15, 8)

#define HTT_BW_MGR_STATS_PRI20_FREQ		GENMASK(31, 16)

#define HTT_BW_MGR_STATS_CENTER_FREQ1		GENMASK(15, 0)

#define HTT_BW_MGR_STATS_CENTER_FREQ2		GENMASK(31, 16)

#define HTT_BW_MGR_STATS_CHAN_PHY_MODE		GENMASK(7, 0)

#define HTT_BW_MGR_STATS_STATIC_PATTERN		GENMASK(23, 8)

#define HTT_BW_MGR_STATS_WIFI_VERSION		GENMASK(3, 0)

struct ath12k_htt_stats_pdev_bw_mgr_stats_tlv {
	__le32 mac_id__pri20_idx__freq;
	__le32 centre_freq1__freq2;
	__le32 phy_mode__static_pattern;
	__le32 npca__wifi_version__pri20_idx__freq;
	__le32 npca__centre_freq1__freq2;
	__le32 npca__phy_mode__static_pattern;
} __packed;

struct ath12k_htt_stats_mlo_sched_stats_tlv {
	__le32 pref_link_num_sec_link_sched;
	__le32 pref_link_num_pref_link_timeout;
	__le32 pref_link_num_pref_link_sch_delay_ipc;
	__le32 pref_link_num_pref_link_timeout_ipc;
} __packed;

struct ath12k_htt_pdev_mlo_ipc_stats_tlv {
	__le32 mlo_ipc_ring_full_cnt[ATH12K_HTT_STATS_HWMLO_MAX_LINKS]
				    [ATH12K_HTT_STATS_MLO_MAX_IPC_RINGS];
} __packed;

enum HTT_RX_TX_PDEV_STATS_WIFI_VERSION {
	HTT_WIFI_VER_UNKNOWN = 0,
	HTT_WIFI_VER_11N  = 4,
	HTT_WIFI_VER_11AC = 5,
	HTT_WIFI_VER_11AX = 6,
	HTT_WIFI_VER_11BE = 7,
	HTT_WIFI_VER_11BN = 8,
};

/******************** Regulatory Stats *********************/
struct ath12k_htt_stats_regdb_ctry_tlv {
	union {
		struct {
			__le32 major_version : 8,
			       minor_version : 8,
			       custom_version: 8,
			       rsvd          : 8;
		};
		__le32 regdb_version;
	};
	__le32 country_code;
	__le32 alpha_code;	/* [7:0] = alpha[0], [15:8] = alpha[1], ...*/
	__le32 reg_domain_pair_id;
	__le32 super_domain_id;
	__le32 phymode_bitmap;
	__le32 chan_priority_freq; /* in MHz */
	__le32 tpc_region;
	__le32 max_bw_2g;	/* in MHz */
	__le32 max_bw_5g;	/*in MHz */
	__le32 max_bw_6g;	/*in MHz */
};

#define ATH12K_HTT_STATS_REGDB_MAJ_VERSION	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REGDB_MIN_VERSION	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REGDB_CUSTOM_VERSION	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REGDB_ALPHA_CODE_0	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REGDB_ALPHA_CODE_1	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REGDB_ALPHA_CODE_2	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REGDB_ALPHA_CODE_3	GENMASK(31, 24)

struct ath12k_htt_stats_regdb_reg_rule {
	union {
		struct {
			__le32 start_freq : 16,	/* in MHz */
			       end_freq   : 16;	/* in MHz */
		};
		__le32 freq_info;
	};
	union {
		struct {
			__le32 max_bw     : 16,	/* in MHz */
			       reg_power  : 8,	/* in dBm */
			       ant_gain   : 8;	/* in dB */
		};
		__le32 bw_pwr_info;
	};
	union {
		struct {
			__le32 flags      : 16,
			       ctl_region : 8,
			       rsvd       : 8;
		};
		__le32 flag_info;
	};
	union {
		struct {
			__le32 is_psd     : 1,
			       rsvd1      : 15,
			       psd_power  : 16;	/* dBm / MHz  (signed)*/
		};
		__le32 psd_power_info;
	};
};

#define ATH12K_HTT_STATS_REG_RULE_START_FREQ	GENMASK(15, 0)
#define ATH12K_HTT_STATS_REG_RULE_END_FREQ	GENMASK(31, 16)
#define ATH12K_HTT_STATS_REG_RULE_MAX_BW	GENMASK(15, 0)
#define ATH12K_HTT_STATS_REG_RULE_REG_PWR	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REG_RULE_ANT_GAIN	GENMASK(31, 24)
#define ATH12K_HTT_STATS_REG_RULE_FLAGS		GENMASK(15, 0)
#define ATH12K_HTT_STATS_REG_RULE_CTL_RGN	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REG_RULE_IS_PSD	BIT(0)
#define ATH12K_HTT_STATS_REG_RULE_PSD_PWR	GENMASK(31, 16)

enum ath12k_htt_stats_regdb_regdomain_type {
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_2G             = 0,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_5G             = 1,
	/* 2 is unused */
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_AP_LPI      = 3,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_AP_SP       = 4,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_AP_VLP      = 5,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL1_LPI     = 6,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL1_SP      = 7,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL1_VLP     = 8,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL2_LPI     = 9,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL2_SP      = 10,
	ATH12K_HTT_STATS_REGULATORY_REG_DMN_6G_CL2_VLP     = 11,
};

struct ath12k_htt_stats_regdb_regdomain_tlv {
	__le32 rd_type;	/*enum ath12k_htt_stats_regdb_regdomain_type*/
	union {
		struct {
			__le32 ctl_region    : 8,
			    cca_region       : 8,
			    dfs_region       : 8,
			    domain_ctl_index : 8;
		};
		__le32 ctl_cca_dfs;
	};
	__le32 rd_code;
	__le32 reserved;
	union {
		struct {
			__le32 num_rules : 16,
			       rule_size : 16;
		};
		__le32 rule_num_and_size;
	};
	struct ath12k_htt_stats_regdb_reg_rule rules[];
};

#define ATH12K_HTT_STATS_REGDOMAIN_CTL_REGION	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REGDOMAIN_CCA_REGION	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REGDOMAIN_DFS_REGION	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REGDOMAIN_DOMAIN_CTL_INDEX   GENMASK(31, 24)
#define ATH12K_HTT_STATS_REGDOMAIN_NUM_RULES	GENMASK(15, 0)
#define ATH12K_HTT_STATS_REGDOMAIN_RULE_SIZE	GENMASK(31, 16)

struct ath12k_htt_stats_reg_6g_tlv {
	union {
		struct {
			__le32 afc_local_rsvd : 8,
				 deployment_type: 8,
				 power_mode_mask: 8,
				 reserved       : 8;
		};
		__le32 afc_ini_params;
	};
	__le32 tx_allowed_reason_code;
	union {
		struct {
			__le32 set_tpc_count      : 16,
				 set_tpc_pass_count : 16;
		};
		__le32 set_tpc_counters;
	};
	union {
		struct {
			__le32 current_power_mode              : 4,
				 last_best_power_mode            : 4,
				 is_current_mode_best_power_mode : 1,
				 rsvd1                           : 7,
				 best_power_mode_count           : 16;
		};
		__le32 power_mode_stats;
	};
};

#define ATH12K_HTT_STATS_REG_6G_AFC_LOCAL_RSVD	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REG_6G_DEPLOYMENT_TYPE	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REG_6G_POWER_MODE_MASK	GENMASK(23, 16)
#define ATH12K_HTT_STATS_REG_6G_SET_TPC_COUNT	GENMASK(15, 0)
#define ATH12K_HTT_STATS_REG_6G_SET_TPC_PASS_COUNT	GENMASK(31, 16)
#define ATH12K_HTT_STATS_REG_6G_CURRENT_POWER_MODE	GENMASK(3, 0)
#define ATH12K_HTT_STATS_REG_6G_LAST_BEST_POWER_MODE	GENMASK(7, 4)
#define ATH12K_HTT_STATS_REG_6G_BEST_POWER_MODE_COUNT	GENMASK(31, 16)
#define ATH12K_HTT_STATS_REG_6G_IS_CURRENT_POWER_MODE_BEST	BIT(8)

struct ath12k_htt_stats_reg_freq_power_pair {
	union {
		struct {
			__le32 freq  : 16, /* in MHz */
			       power : 16; /* in 0.25 dBm */
		};
		__le32 freq_power_pair;
	};
};

#define ATH12K_HTT_STATS_FREQ_POWER_PAIR_FREQ	GENMASK(15, 0)
#define ATH12K_HTT_STATS_FREQ_POWER_PAIR_POWER	GENMASK(31, 16)

#define ATH12K_HTT_STATS_REG_POWER_INFO_6G_MAX_SUBBANDS	16

struct ath12k_htt_stats_reg_6g_ch_power_info_tlv {
	__le32 index;
	union {
		__le32 power_info_word;
		struct {
			__le32 power_type_6ghz : 8,
			       eirp_power : 8,      /* in dBm */
			       is_psd_power : 1,
			       both_psd_eirp_support : 1,
			       rsvd :14;
		};
	};
	union {
		__le32 num_levels_word;
		struct {
			__le32 num_power_levels : 8,
			       num_psd_levels : 8,
			       num_eirp_levels : 8,
			       unused : 8;
		};
	};
	__le32 puncture_bitmap;
	struct ath12k_htt_stats_reg_freq_power_pair
		tx_power_freq_pair[ATH12K_HTT_STATS_REG_POWER_INFO_6G_MAX_SUBBANDS];
	struct ath12k_htt_stats_reg_freq_power_pair
		psd_power_freq_pair[ATH12K_HTT_STATS_REG_POWER_INFO_6G_MAX_SUBBANDS];
	struct ath12k_htt_stats_reg_freq_power_pair
		eirp_power_freq_pair[ATH12K_HTT_STATS_REG_POWER_INFO_6G_MAX_SUBBANDS];
};

#define ATH12K_HTT_STATS_REG_6G_CH_POWER_TYPE	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REG_6G_CH_EIRP_POWER	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REG_6G_CH_IS_PSD_POWER	BIT(16)
#define ATH12K_HTT_STATS_REG_6G_CH_IS_BOTH_PSD_EIRP	BIT(17)
#define ATH12K_HTT_STATS_REG_6G_CH_NUM_POWER_LEVELS	GENMASK(7, 0)
#define ATH12K_HTT_STATS_REG_6G_CH_NUM_PSD_LEVELS	GENMASK(15, 8)
#define ATH12K_HTT_STATS_REG_6G_CH_NUM_EIRP_LEVELS	GENMASK(23, 16)

#define ATH12K_HTT_STATS_REG_OOBE_MAX_BW 5

struct ath12k_htt_stats_reg_6g_oobe_tlv {
	__le32 oobe_psd[ATH12K_HTT_STATS_REG_OOBE_MAX_BW]; /* 0.25 dBm / MHz */
	union {
		struct {
			__le32 oobe_limit_offset : 16,  /* in MHz */
			       oobe_limit_mask   : 16;  /* in dBr */
		};
		__le32 offset_mask_pair;
	} oobe_limit[ATH12K_HTT_STATS_REG_OOBE_MAX_BW];
};

#define ATH12K_HTT_STATS_REG_6G_OOBE_LIMIT_OFFSET	GENMASK(15, 0)
#define ATH12K_HTT_STATS_REG_6G_OOBE_LIMIT_MASK	GENMASK(31, 16)

#define ATH12K_QUARTER_DBM_TO_DBM_INT(q)	((q) / 4)
#define ATH12K_QUARTER_DBM_TO_DBM_FRAC(q)	(((q) % 4) * 25 * ((q) < 0 ? -1 : 1))

struct ath12k_htt_stats_ctl_tlv {
	 __le32 array_gain_cap[HTT_STATS_MAX_CHAINS * ((HTT_STATS_MAX_CHAINS / 2) + 1)];
	union {
		struct {
			 __le32 reg_rule_index:16,
				power_rule_index:8,
				rsvd:8;
		};
				__le32 ctl_args;
	};
};
struct ath12k_htt_stats_enhanced_ctl_tlv {
	union {
		struct {
			 __le32 enhanced_ctl_enable:1,
				rsvd:7,
				domain_ctl_idx:8,
				array_gain_cap_ctl_region:8,
				exception_ctl_region:8;
		};
			__le32 enhanced_ctl_args;
	};
};

#define ATH12K_HTT_STATS_GET_ENHANCED_CTL_ENABLE GENMASK(0, 0)
#define ATH12K_HTT_STATS_GET_DOMAIN_CTL_INDEX GENMASK(15, 8)
#define ATH12K_HTT_STATS_GET_ARRAY_GAIN_CAP_CTL_REGION GENMASK(23, 16)
#define ATH12K_HTT_STATS_GET_EXCEPTION_CTL_REGION GENMASK(31, 24)
#define ATH12K_HTT_STATS_GET_REG_RULE_INDEX GENMASK(15, 0)
#define ATH12K_HTT_STATS_GET_POWER_RULE_INDEX GENMASK(23, 16)

#define HTT_STATS_DFS_RADAR_HISTORY_MAX_ENTRIES  5

struct ath12k_htt_stats_dfs_radar_history_tlv {
	struct {
		__le32 freq_det_info;
		__le32 rssi_pulse_info;
		__le32 domain_type_info;
		__le32 sidx_freq_info;
		__le32 dur_info;
		__le32 threshold_info;
		__le32 pri_info;
	} radar_entry_stats[HTT_STATS_DFS_RADAR_HISTORY_MAX_ENTRIES];
	__le32 idx;      /* next write position */
	__le32 count;    /* valid entries in buffer */
} __packed;

#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_CHAN_FREQ_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_DETECTOR_ID_GET GENMASK(23, 16)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_IS_CHIRP_GET GENMASK(24, 24)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_PULSEID_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RADAR_RSSI_DBM_GET GENMASK(23, 16)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_DOMAIN_GET GENMASK(7, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_TYPE_GET GENMASK(15, 8)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_RADAR_TYPE_GET GENMASK(23, 16)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_SIDX_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_FREQ_OFFSET_GET GENMASK(31, 16)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_MINDUR_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_MAXDUR_GET GENMASK(31, 16)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_THRESHOLD_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_MINPRI_GET GENMASK(15, 0)
#define HTT_STATS_DFS_RADAR_HISTORY_RADAR_ENTRY_STATS_RF_MAXPRI_GET GENMASK(31, 16)

#define ATH12K_HTT_STATS_DFS_INI_NUM_REGS	10

struct ath12k_htt_stats_dfs_ini_tlv {
	__le32 reg_val[ATH12K_HTT_STATS_DFS_INI_NUM_REGS];
} __packed;


/* Array index identifies the ring:
 *   [0] = DFS0 (IPC_DFS0_RINGID=2)
 *   [1] = DFS1 (IPC_DFS1_RINGID=3)
 *   [2] = DFS2 (IPC_DFS2_RINGID=4)
 */
#define ATH12K_HTT_STATS_DFS_IPC_RING_COUNT  3

struct ath12k_htt_stats_dfs_ipc_ring_tlv {
	struct {
		__le32 head_idx;
		__le32 shadow_head_idx;
		__le32 tail_idx;
		__le32 shadow_tail_idx;
	} ipc_ring_stats[ATH12K_HTT_STATS_DFS_IPC_RING_COUNT];
} __packed;

/*======= Selfgen Response stats ====================*/

enum ATH12K_HTT_STATS_RESP_FRAME_TYPES {
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_11L_MBPS = 0,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_5_5L_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_2L_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_1L_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_11S_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_5_5S_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_CCK_2S_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_6_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_9_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_12_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_18_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_24_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_36_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_48_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_OFDM_54_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_CBF_11AC,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_CBF_11AX,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_CBF_11BE,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_CBF_11AX_EXT,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_ACK_CTS_11AX_EXT_DCM,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11N_MCS0,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11N_MCS1,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11N_MCS3,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AC_MCS0,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AC_MCS1,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AC_MCS3,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11A_6_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11A_12_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11A_24_MBPS,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AX_MCS0,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AX_MCS1,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_RTT_11AX_MCS3,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_AX_BE_EXT_DEF,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_11BE_MCS14_MCS15,
	ATH12K_HTT_STATS_RESP_FRAME_TYPE_MAX
};

/* Mask and shift definitions for type_rate_chainmask fields */
#define ATH12K_HTT_STATS_WHAL_SELFGEN_PKT_TYPE_M           0x0000000F
#define ATH12K_HTT_STATS_WHAL_SELFGEN_PKT_TYPE_S           0
#define ATH12K_HTT_STATS_WHAL_SELFGEN_NSS_M                0x00000070
#define ATH12K_HTT_STATS_WHAL_SELFGEN_NSS_S                4
#define ATH12K_HTT_STATS_WHAL_SELFGEN_RATE_MCS_M           0x00000780
#define ATH12K_HTT_STATS_WHAL_SELFGEN_RATE_MCS_S           7
#define ATH12K_HTT_STATS_WHAL_SELFGEN_BANDWIDTH_M          0x00003800
#define ATH12K_HTT_STATS_WHAL_SELFGEN_BANDWIDTH_S          11
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHAIN_MASK_M         0x003FC000
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHAIN_MASK_S         14
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_CHAIN_MASK_M     0x3FC00000
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_CHAIN_MASK_S     22

/* Mask and shift definitions for tx_pwr_alt fields */
#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_M             0x000000FF
#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_S             0
#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_1_M           0x0000FF00
#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_1_S           8
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_M         0x00FF0000
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_S         16
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_1_M       0xFF000000
#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_1_S       24

/* Accessor Macros */
#define ATH12K_HTT_STATS_WHAL_SELFGEN_PKT_TYPE_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_PKT_TYPE_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_PKT_TYPE_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_NSS_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_NSS_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_NSS_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_RATE_MCS_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_RATE_MCS_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_RATE_MCS_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_BANDWIDTH_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_BANDWIDTH_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_BANDWIDTH_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHAIN_MASK_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_CHAIN_MASK_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_CHAIN_MASK_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_CHAIN_MASK_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_CHAIN_MASK_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_CHAIN_MASK_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_1_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_1_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_TX_PWR_1_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_1_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_1_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_ALT_TX_PWR_1_S)

/* Structure Definition */
struct ath12k_htt_stats_whal_selfgen_resp_frame_entry {
	union {
		__le32 type_rate_chainmask;
		struct {
			u32	pkt_type	:4,
				nss		:3,
				rate_mcs	:4,
				bandwidth	:3,
				chain_mask	:8,
				alt_chain_mask	:8,
				reserved	:2;
		};
	};

	union {
		__le32 tx_pwr_alt;
		struct {
			u32	tx_pwr		:8,
				tx_pwr_1	:8,
				alt_tx_pwr	:8,
				alt_tx_pwr_1	:8;
		};
	};
} __packed;

/* Mask and Shift Definitions */
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_MHZ_M        0x0000FFFF
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_MHZ_S        0
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_FLAGS_M      0xFFFF0000
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_FLAGS_S      16

/* Accessor Macros */
#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_MHZ_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_MHZ_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_MHZ_S)

#define ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_FLAGS_GET(_var) \
	(((_var) & ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_FLAGS_M) >> \
	ATH12K_HTT_STATS_WHAL_SELFGEN_CHANNEL_FLAGS_S)

/* Structure Definition */
struct ath12k_htt_stats_whal_selfgen_channel_context {
	union {
		__le32 channel_info;
		struct {
			u32	mhz		:16,
				flags		:16;
		};
	};
	__le32 phy_mode;
} __packed;

struct ath12k_htt_stats_tx_selfgen_resp_frame_stats_tlv {
	struct ath12k_htt_stats_whal_selfgen_channel_context channel_context;
	struct ath12k_htt_stats_whal_selfgen_resp_frame_entry
		frame_data[ATH12K_HTT_STATS_RESP_FRAME_TYPE_MAX];
} __packed;

#define HTT_STATS_RESET_HISTORY_MAX_ENTRIES 10

/**
 * @brief TLV structure for Upstream reset stats 82 (reset history)
 * This structure holds the last HTT_STATS_RESET_HISTORY_MAX_ENTRIES.
 * The entries are copied from the firmware's internal circular buffer.
 */
struct htt_stats_reset_history_tlv {
	/**
	 * @brief An array of structures, each holding the details of a single
	 *        reset event. The entries are ordered chronologically from
	 *        oldest to newest.
	 */
	struct {
		__le32 timestamp_ms;
		__le32 reset_flags;
		__le32 reset_cause;
		__le32 reset_reason;
		__le32 phy_mode;

		union {
			__le32 channel_freq;
			struct {
				__le32 mhz:16,
				       band_center_freq1:16;
			};
		};

		union {
			__le32 channel_info;
			struct {
				__le32 flags:16,
				       phy_id:8,
				       swprofile:8;
			};
		};

		union {
			__le32 home_channel_info;
			struct {
				__le32 is_home_chan:1,
				       reserved:31;
			};
		};
		__le32 reserved_dwords[2]; /* reserved for future use */
	} reset_history[HTT_STATS_RESET_HISTORY_MAX_ENTRIES];

	__le32 idx;                   /**< Current write index of the circular buffer */
	__le32 count;                 /**< Total number of resets captured (up to 10) */
} __packed;

#define HTT_STATS_RESET_HISTORY_MHZ_GET                         GENMASK(15, 0)
#define HTT_STATS_RESET_HISTORY_BAND_CENTER_FREQ1_GET           GENMASK(31, 16)
#define HTT_STATS_RESET_HISTORY_FLAGS_GET                       GENMASK(15, 0)
#define HTT_STATS_RESET_HISTORY_PHY_ID_GET                      GENMASK(23, 16)
#define HTT_STATS_RESET_HISTORY_SWPROFILE_GET                   GENMASK(31, 24)
#define HTT_STATS_RESET_HISTORY_IS_HOME_CHAN_GET                GENMASK(0, 0)

struct ath12k_htt_stats_dpd_halphy_tlv {
	__le32 pdev_id;
	__le32 dpd_cal_start_time;
	__le32 dpd_cal_end_time;
	__le32 total_tx_pass_cnt;
	__le32 total_tx_fail_cnt;
	__le32 dpd_trigger_reason;
	a_sle32 dpd_training_temp[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	a_sle32 cal_start_temp;
	a_sle32 cal_end_temp;
	__le32 cal_channel;
	__le32 cal_phy_mode;
	__le32 cal_chan_flags;
	__le32 mem_dpd_post_proc_trigger_cnt;
	__le32 mem_dpd_post_proc_complete_cnt;
	__le32 sch_cmd_result_per_chain[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	__le32 tx_status_per_chain[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	__le32 dpd_cal_state;
	__le32 dpd_cal_status;
	__le32 dpd_fail_reason;
} __packed;

struct ath12k_htt_stats_dpd_hw_cal_parm_tlv {
	__le32 dpd_rx_gain[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	__le32 tpc_gain_idx[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	__le32 tpc_glut[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
} __packed;

struct ath12k_htt_stats_dpd_hw_cal_res_tlv {
	__le32 sq_value[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	__le32 sq_idx[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
	a_sle32 nmse_chain[ATH12K_HTT_STATS_MAX_CHAINS]
		[ATH12K_HTT_MAX_DPD_CAL_TABLE];
} __packed;

/* subid values for ATH12K_DBG_HTT_DBG_EXT_STATS_PHY_DPD_TPC (cfg_param[0]) */
#define ATH12K_HTT_STATS_PHY_DPD_TPC_SUBID_DPD		0
#define ATH12K_HTT_STATS_PHY_DPD_TPC_SUBID_TPC		1

/* PHY DPD debug structures (PHY_DPD_DEBUG_STRUCT_V1) */
/* PHY DPD debug structures */
/* Macros for chain_idx__version__chainmask word */
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_IDX_M        0x000000ff
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_IDX_S        0
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_VERSION_M    0x0000ff00
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_VERSION_S    8
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_CHAINMASK_M  0xffff0000
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_CHAINMASK_S  16

#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_IDX_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_VERSION_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_VERSION_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_VERSION_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_CHAINMASK_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_CHAINMASK_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_CHAIN_CHAINMASK_S)

/* Macros for num_gain_idx__reserved word */
#define ATH12K_HTT_PHY_DPD_DEBUG_NUM_GAIN_IDX_M     0x000000ff
#define ATH12K_HTT_PHY_DPD_DEBUG_NUM_GAIN_IDX_S     0

#define ATH12K_HTT_PHY_DPD_DEBUG_NUM_GAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_NUM_GAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_NUM_GAIN_IDX_S)

/* Macros for dpd_training_power_db8__dpd_out_sq word */
#define ATH12K_HTT_PHY_DPD_DEBUG_TRAINING_PWR_DB8_M  0x0000ffff
#define ATH12K_HTT_PHY_DPD_DEBUG_TRAINING_PWR_DB8_S  0
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_M            0xffff0000
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_S            16

#define ATH12K_HTT_PHY_DPD_DEBUG_TRAINING_PWR_DB8_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_TRAINING_PWR_DB8_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_TRAINING_PWR_DB8_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_S)

/* Macros for dpd_state__...__dpd_out_train_dac_gain word */
#define ATH12K_HTT_PHY_DPD_DEBUG_STATE_M             0x000000ff
#define ATH12K_HTT_PHY_DPD_DEBUG_STATE_S             0
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GLUT_M           0x0000ff00
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GLUT_S           8
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_TX_GAIN_M        0x00ff0000
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_TX_GAIN_S        16
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_DAC_GAIN_M  0xff000000
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_DAC_GAIN_S  24

#define ATH12K_HTT_PHY_DPD_DEBUG_STATE_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_STATE_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_STATE_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GLUT_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_IN_GLUT_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_IN_GLUT_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_TX_GAIN_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_IN_TX_GAIN_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_IN_TX_GAIN_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_DAC_GAIN_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_DAC_GAIN_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_DAC_GAIN_S)

/* Macros for dpd_in_gc__...__dpd_in_kernel_sel word */
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GC_M             0x000000ff
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GC_S             0
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_IDX_M        0x0000ff00
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_IDX_S        8
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_RX_GAIN_IDX_M  0x00ff0000
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_RX_GAIN_IDX_S  16
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_KERNEL_SEL_M     0xff000000
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_KERNEL_SEL_S     24

#define ATH12K_HTT_PHY_DPD_DEBUG_IN_GC_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_IN_GC_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_IN_GC_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_IDX_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_OUT_SQ_IDX_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_RX_GAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_RX_GAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_OUT_TRAIN_RX_GAIN_IDX_S)
#define ATH12K_HTT_PHY_DPD_DEBUG_IN_KERNEL_SEL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_DPD_DEBUG_IN_KERNEL_SEL_M) >> \
	 ATH12K_HTT_PHY_DPD_DEBUG_IN_KERNEL_SEL_S)

/* Macros for TPC chain header word */
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_IDX_M        0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_IDX_S        0
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_VERSION_M    0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_VERSION_S    8
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_CHAINMASK_M  0xffff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_CHAINMASK_S  16

#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_IDX_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_VERSION_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_VERSION_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_VERSION_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_CHAINMASK_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_CHAINMASK_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CHAIN_CHAINMASK_S)

/* Macros for TPC word1: lat_glut_idx/lat_tx_gain_idx/lat_dac_gain/lat_target_power */
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_GLUT_IDX_M     0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_GLUT_IDX_S     0
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TX_GAIN_IDX_M  0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TX_GAIN_IDX_S  8
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_DAC_GAIN_M     0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_DAC_GAIN_S     16
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TARGET_POWER_M 0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TARGET_POWER_S 24

#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_GLUT_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_GLUT_IDX_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_GLUT_IDX_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TX_GAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_TX_GAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_TX_GAIN_IDX_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_DAC_GAIN_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_DAC_GAIN_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_DAC_GAIN_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_TARGET_POWER_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_TARGET_POWER_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_TARGET_POWER_S)

/* Macros for TPC word2: lat_acc_clpc_error/lat_clpc_err */
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_ACC_CLPC_ERROR_M  0x0000ffff
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_ACC_CLPC_ERROR_S  0
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_CLPC_ERR_M        0xffff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_CLPC_ERR_S        16

#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_ACC_CLPC_ERROR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_ACC_CLPC_ERROR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_ACC_CLPC_ERROR_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_CLPC_ERR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_CLPC_ERR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_CLPC_ERR_S)

/* Macros for TPC word3: lat_meas_pwr/lat_wsi_temp_valid/lat_wsi_full_pkt_pwr_valid */
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_MEAS_PWR_M               0x0000ffff
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_MEAS_PWR_S               0
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_VALID_M         0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_VALID_S         16
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_VALID_M 0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_VALID_S 24

#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_MEAS_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_MEAS_PWR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_MEAS_PWR_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_VALID_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_VALID_S)

/* Macros for TPC word4: lat_wsi_pream_pwr_valid/lat_wsi_temp/lat_wsi_full_pkt_pwr */
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_VALID_M  0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_VALID_S  0
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_M             0x00ffff00
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_S             8
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_M     0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_S     24

#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_VALID_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TEMP_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_FULL_PKT_PWR_S)

/* Macros for TPC word5: lat_wsi_pream_pwr/.../lat_wsi_tpc_attn */
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_M          0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_S          0
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TX_GAIN_IDX_M        0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TX_GAIN_IDX_S        8
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_PDET_GAIN_IDX_M  0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_PDET_GAIN_IDX_S  16
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_ATTN_M           0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_ATTN_S           24

#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_PREAM_PWR_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TX_GAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TX_GAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TX_GAIN_IDX_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_PDET_GAIN_IDX_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_PDET_GAIN_IDX_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_PDET_GAIN_IDX_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_ATTN_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_ATTN_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_LAT_WSI_TPC_ATTN_S)

/* Macros for TPC word6: glut_dac_gain_cal/.../dpd_tx_gain_idx_cal */
#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_DAC_GAIN_CAL_M      0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_DAC_GAIN_CAL_S      0
#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_MAX_DAC_GAIN_CAL_M  0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_MAX_DAC_GAIN_CAL_S  8
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_DAC_GAIN_CAL_M       0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_DAC_GAIN_CAL_S       16
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_TX_GAIN_IDX_CAL_M    0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_TX_GAIN_IDX_CAL_S    24

#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_DAC_GAIN_CAL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_GLUT_DAC_GAIN_CAL_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_GLUT_DAC_GAIN_CAL_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_GLUT_MAX_DAC_GAIN_CAL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_GLUT_MAX_DAC_GAIN_CAL_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_GLUT_MAX_DAC_GAIN_CAL_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_DAC_GAIN_CAL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_DPD_DAC_GAIN_CAL_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_DPD_DAC_GAIN_CAL_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_DPD_TX_GAIN_IDX_CAL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_DPD_TX_GAIN_IDX_CAL_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_DPD_TX_GAIN_IDX_CAL_S)

/* Macros for TPC word7: target_pwr_clpc_thr_corr/.../target_pwr_clpc_thr_update */
#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_CORR_M    0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_CORR_S    0
#define ATH12K_HTT_PHY_TPC_DEBUG_OLPC_MODE_M                 0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_OLPC_MODE_S                 8
#define ATH12K_HTT_PHY_TPC_DEBUG_WSI_TIMEOUT_M               0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_WSI_TIMEOUT_S               16
#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_UPDATE_M  0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_UPDATE_S  24

#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_CORR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_CORR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_CORR_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_OLPC_MODE_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_OLPC_MODE_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_OLPC_MODE_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_WSI_TIMEOUT_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_WSI_TIMEOUT_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_WSI_TIMEOUT_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_UPDATE_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_UPDATE_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_TGT_PWR_CLPC_THR_UPDATE_S)

/* Macros for TPC word8: ro_temp_valid/ro_full_pkt_pwr_valid/ro_pream_pwr_valid */
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_VALID_M          0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_VALID_S          0
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_VALID_M  0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_VALID_S  8
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_VALID_M     0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_VALID_S     16

#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_VALID_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_VALID_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_VALID_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_VALID_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_VALID_S)

/* Macros for TPC word9: ro_temp/ro_full_pkt_pwr/ro_pream_pwr */
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_M          0x0000ffff
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_S          0
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_M  0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_S  16
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_M     0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_S     24

#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_TEMP_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_PWR_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_PREAM_PWR_S)

/* Macros for TPC word10: ro_tpc_fe_sel/.../ro_pdacc_avg_out */
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TPC_FE_SEL_M      0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TPC_FE_SEL_S      0
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_AVG_OUT_M  0x0000ff00
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_AVG_OUT_S  8
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_LAT_DC_M          0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_LAT_DC_S          16
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PDACC_AVG_OUT_M   0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PDACC_AVG_OUT_S   24

#define ATH12K_HTT_PHY_TPC_DEBUG_RO_TPC_FE_SEL_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_TPC_FE_SEL_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_TPC_FE_SEL_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_AVG_OUT_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_AVG_OUT_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_FULL_PKT_AVG_OUT_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_LAT_DC_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_LAT_DC_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_LAT_DC_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_RO_PDACC_AVG_OUT_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_RO_PDACC_AVG_OUT_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_RO_PDACC_AVG_OUT_S)

/* Macros for TPC word11: temp_per_chain/cal_cmd/cal_time */
#define ATH12K_HTT_PHY_TPC_DEBUG_TEMP_PER_CHAIN_M  0x0000ffff
#define ATH12K_HTT_PHY_TPC_DEBUG_TEMP_PER_CHAIN_S  0
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_CMD_M         0x00ff0000
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_CMD_S         16
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_TIME_M        0xff000000
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_TIME_S        24

#define ATH12K_HTT_PHY_TPC_DEBUG_TEMP_PER_CHAIN_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_TEMP_PER_CHAIN_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_TEMP_PER_CHAIN_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_CMD_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CAL_CMD_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CAL_CMD_S)
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_TIME_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CAL_TIME_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CAL_TIME_S)

/* Macros for TPC word12: cal_result */
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_RESULT_M  0x000000ff
#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_RESULT_S  0

#define ATH12K_HTT_PHY_TPC_DEBUG_CAL_RESULT_GET(_var) \
	(((_var) & ATH12K_HTT_PHY_TPC_DEBUG_CAL_RESULT_M) >> \
	 ATH12K_HTT_PHY_TPC_DEBUG_CAL_RESULT_S)

/* htt_stats_phy_dpd_debug_params_v1 - 7 x __le32 words */
struct ath12k_htt_phy_dpd_debug_params_v1 {
	a_sle32  dpd_out_nmse_x10;
	__le32   pa_max_avg_tx;
	__le32   dpd_training_cnt;
	__le32   dpd_scaling;
	/* BIT[15:0] = dpd_training_power_db8, BIT[31:16] = dpd_out_sq */
	__le32   dpd_training_power_db8__dpd_out_sq;
	/* BIT[7:0]=state, BIT[15:8]=in_glut, BIT[23:16]=in_tx_gain,
	 * BIT[31:24]=out_train_dac_gain
	 */
	__le32   dpd_state__in_glut__in_tx_gain__out_train_dac_gain;
	/* BIT[7:0]=in_gc, BIT[15:8]=out_sq_idx,
	 * BIT[23:16]=out_train_rx_gain_idx, BIT[31:24]=in_kernel_sel
	 */
	__le32   dpd_in_gc__out_sq_idx__out_rx_gain_idx__in_kernel_sel;
} __packed;

/* htt_stats_phy_dpd_debug_chain_v1_tlv */
struct ath12k_htt_phy_dpd_debug_chain_tlv {
	/* BIT[7:0]=chain_idx, BIT[15:8]=version, BIT[31:16]=chainmask */
	__le32 chain_idx__version__chainmask;
	/* BIT[7:0]=num_gain_idx, BIT[31:8]=reserved */
	__le32 num_gain_idx__reserved;
	struct ath12k_htt_phy_dpd_debug_params_v1
		dpd_debug_params[ATH12K_HTT_MAX_DPD_CAL_TABLE];
} __packed;

/* PHY TPC debug structures */
/* htt_stats_phy_tpc_debug_chain_v1_tlv */
struct ath12k_htt_phy_tpc_debug_chain_tlv {
	/* BIT[7:0]=chain_idx, BIT[15:8]=version, BIT[31:16]=chainmask */
	__le32 chain_idx__version__chainmask;
	/* BIT[7:0]=lat_glut_idx, BIT[15:8]=lat_tx_gain_idx,
	 * BIT[23:16]=lat_dac_gain, BIT[31:24]=lat_target_power
	 */
	__le32 lat_glut_idx__tx_gain_idx__dac_gain__target_power;
	/* BIT[15:0]=lat_acc_clpc_error, BIT[31:16]=lat_clpc_err */
	__le32 lat_acc_clpc_error__lat_clpc_err;
	/* BIT[15:0]=lat_meas_pwr, BIT[23:16]=lat_wsi_temp_valid,
	 * BIT[31:24]=lat_wsi_full_pkt_pwr_valid
	 */
	__le32 lat_meas_pwr__lat_wsi_temp_valid__lat_wsi_full_pkt_pwr_valid;
	/* BIT[7:0]=lat_wsi_pream_pwr_valid, BIT[23:8]=lat_wsi_temp,
	 * BIT[31:24]=lat_wsi_full_pkt_pwr
	 */
	__le32 lat_wsi_pream_pwr_valid__lat_wsi_temp__lat_wsi_full_pkt_pwr;
	/* BIT[7:0]=lat_wsi_pream_pwr, BIT[15:8]=lat_wsi_tx_gain_idx,
	 * BIT[23:16]=lat_wsi_tpc_pdet_gain_idx, BIT[31:24]=lat_wsi_tpc_attn
	 */
	__le32 lat_wsi_pream_pwr__tx_gain_idx__tpc_pdet_gain_idx__tpc_attn;
	/* BIT[7:0]=glut_dac_gain_cal, BIT[15:8]=glut_max_dac_gain_cal,
	 * BIT[23:16]=dpd_dac_gain_cal, BIT[31:24]=dpd_tx_gain_idx_cal
	 */
	__le32 glut_dac_gain_cal__max_dac_cal__dpd_dac_gain_cal__tx_gain_idx;
	/* BIT[7:0]=target_pwr_clpc_thr_corr, BIT[15:8]=olpc_mode,
	 * BIT[23:16]=wsi_timeout, BIT[31:24]=target_pwr_clpc_thr_update
	 */
	__le32 tgt_pwr_clpc_thr_corr__olpc_mode__wsi_timeout__clpc_thr_update;
	/* BIT[7:0]=ro_temp_valid, BIT[15:8]=ro_full_pkt_pwr_valid,
	 * BIT[23:16]=ro_pream_pwr_valid, BIT[31:24]=reserved
	 */
	__le32 ro_temp_valid__ro_full_pkt_pwr_valid__ro_pream_pwr_valid;
	/* BIT[15:0]=ro_temp, BIT[23:16]=ro_full_pkt_pwr,
	 * BIT[31:24]=ro_pream_pwr
	 */
	__le32 ro_temp__ro_full_pkt_pwr__ro_pream_pwr;
	/* BIT[7:0]=ro_tpc_fe_sel, BIT[15:8]=ro_full_pkt_avg_out,
	 * BIT[23:16]=ro_lat_dc, BIT[31:24]=ro_pdacc_avg_out
	 */
	__le32 ro_tpc_fe_sel__full_pkt_avg_out__lat_dc__pdacc_avg_out;
	/* BIT[15:0]=temp_per_chain, BIT[23:16]=cal_cmd,
	 * BIT[31:24]=cal_time
	 */
	__le32 temp_per_chain__cal_cmd__cal_time;
	/* BIT[7:0]=cal_result, BIT[31:8]=reserved */
	__le32 cal_result__reserved;
} __packed;

#endif
