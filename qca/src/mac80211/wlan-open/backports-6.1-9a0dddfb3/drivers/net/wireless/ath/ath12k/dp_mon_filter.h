/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DP_MON_FILTER_H_
#define _DP_MON_FILTER_H_

#include "debug.h"
#include "pktlog.h"

#define DP_SMART_MON_DATA_FILTER        BIT(1)
#define DP_SMART_MON_MGMT_FILTER        BIT(2)
#define DP_SMART_MON_CTRL_FILTER        BIT(3)

/* Each bit set represent 64, 128, 256.
 * If all 3 bits are set config length is > 256.
 */
#define DP_TX_MON_MAX_DMA_LENGTH        GENMASK(2, 0)

/**
 * struct dp_mon_rx_filter - Monitor RX TLV filter
 * @tlv_filter: Rx ring TLV filter
 * @valid: enable/disable TLV filter
 */
struct dp_mon_rx_filter {
	struct htt_rx_ring_tlv_filter rx_tlv_filter;
	bool valid;
};

/**
 * struct dp_mon_tx_filter - Monitor Tx TLV filter
 * @filter: Tx ring TLV filter
 * @valid: enable/disable TLV filter
 */
struct dp_mon_tx_filter {
	struct htt_tx_ring_tlv_filter filter;
	bool valid;
};

enum dp_mon_tx_filter_mode {
	DP_MON_TX_FULL_MONITOR,
	DP_MON_TX_FILTER_PKTLOG_HYBRID,
	DP_MON_TX_FILTER_EXT_MON_MODE,
	DP_MON_TX_FILTER_SPL_PKT_CAP,
	DP_MON_TX_FILTER_MAX
};

enum dp_mon_tx_filter_srng_type {
	DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST,
	DP_MON_TX_FILTER_SRNG_TYPE_MAX
};

#define DP_MON_RCC_TYPE_M_DIRECTED_FTM	BIT(0)
#define DP_MON_RCC_TYPE_M_ALL_FTM_ACK	BIT(1)
#define DP_MON_RCC_TYPE_M_NDPA_NDP_DIRECTED	BIT(2)
#define DP_MON_RCC_TYPE_M_NDPA_NDP_ALL	BIT(3)
#define DP_MON_RCC_TYPE_M_TA_RA_FILTER	BIT(4)
#define DP_MON_RCC_TYPE_M_ALL_PACKET	BIT(5)

enum dp_mon_filter_mode {
	DP_MON_FILTER_STATS_MODE,
	DP_MON_FILTER_MONITOR_MODE,
	DP_MON_FILTER_NRP_MODE,
	DP_MON_FILTER_PKTLOG_FULL_MODE,
	DP_MON_FILTER_PKTLOG_LITE_MODE,
	DP_MON_FILTER_PKTLOG_CBF_MODE,
	DP_MON_FILTER_EXT_MON_MODE,
	DP_MON_FILTER_UNDECODED_METADATA_CAPTURE_MODE,
	DP_MON_FILTER_CFR_RCC_MODE,
	DP_MON_FILTER_MAX_MODE
};

enum dp_mon_filter_srng_type {
	DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS,
	DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF,
	DP_MON_FILTER_SRNG_TYPE_RXMON_DEST,
	DP_MON_FILTER_SRNG_TYPE_MAX
};

enum dp_mpdu_filter_category {
	DP_MPDU_FILTER_CATEGORY_FP,
	DP_MPDU_FILTER_CATEGORY_MD,
	DP_MPDU_FILTER_CATEGORY_MO,
	DP_MPDU_FILTER_CATEGORY_FP_MO
};

int ath12k_dp_mon_rx_filter_alloc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_filter_free(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_stats_config_filter(struct ath12k_pdev_dp *dp_pdev,
					  enum dp_mon_stats_mode stats_mode,
					  bool enable);
int ath12k_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev,
					     bool enable);
void
ath12k_dp_mon_rx_setup_mon_mode_filter(struct ath12k_dp *dp,
				       struct htt_rx_ring_tlv_filter *tlv_filter,
				       u32 rx_mon_tlv_filter_flags);
int ath12k_dp_mon_rx_config_filters(struct ath12k_dp *dp,
				    struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_filter_srng_type srng_type,
				    struct htt_rx_ring_tlv_filter *rx_tlv_filter);
void ath12k_dp_mon_rx_prepare_filter(struct ath12k_dp *dp,
				     struct ath12k_pdev_dp *dp_pdev,
				     enum dp_mon_filter_srng_type srng_type,
				     struct dp_mon_rx_filter *rx_mon_filter);
void ath12k_dp_mon_rx_display_filters(struct ath12k_dp *dp,
				      enum dp_mon_filter_mode mode,
				      struct dp_mon_rx_filter *rx_filter);
void ath12k_dp_mon_rx_nrp_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable);
void ath12k_dp_mon_rx_smart_mon_config_filter(struct ath12k_pdev_dp *dp_pdev,
					      bool enable);
void ath12k_dp_mon_rx_cfr_rcc_config_filter(struct ath12k_pdev_dp *dp_pdev,
					    bool enable, u8 rcc_type);
bool ath12k_dp_mon_get_rx_cfr_rcc_filter_valid(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_rx_wmask_subscribe(void *ptr,
				      struct htt_rx_ring_tlv_filter *tlv_filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_mon_rx_enable_packet_filters(void *ptr,
					    struct htt_rx_ring_tlv_filter *tlv_filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_mgmt_fpmo_flag0_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_ctrl_fpmo_flag0_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_tx_htt_rx_data_fpmo_flag1_filter_set(u32 *ptr, u16 filter);
void ath12k_dp_htt_rx_filter_rxmon_cfg(void *ptr,
				       struct htt_rx_ring_tlv_filter *tlv_filter);
void ath12k_dp_ext_mon_rx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					bool enable);
#endif
