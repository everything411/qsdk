/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_IPA_STATS_H
#define ATH12K_DP_IPA_STATS_H

#include "dp_ipa.h"
#include "../../vendor.h"

#define HAL_STATS_TLV_HDR_TAG	GENMASK(11, 0)
#define HAL_STATS_TLV_HDR_LEN	GENMASK(23, 12)
#define HTT_T2H_VDEV_TXRX_STATS_PERIODIC_IND_HDR_WORDS 4
#define HTT_T2H_VDEV_TXRX_STATS_TLV_HDR_WORDS 1
#define HTT_T2H_VDEV_TXRX_STATS_TLV_HDR_SIZE 4
#define BYTE_TO_DWORD_SHIFT 2

enum ath12k_tx_err {
	ATH12K_DP_IPA_STATS_RETRY,
	ATH12K_DP_IPA_STATS_EXPIRED,
	ATH12K_DP_IPA_STATS_AGE_OUT,
	ATH12K_DP_IPA_STATS_TX_ERR_MAX,
};

struct ath12k_dp_ipa_stats_vif_stats {
	struct ath12k_dp_pkt_info ucast;
	struct ath12k_dp_pkt_info mcast;
	struct ath12k_dp_pkt_info bcast;
	struct ath12k_dp_pkt_info total;
};

struct ath12k_dp_ipa_stats_tx_vif_stats {
	struct ath12k_dp_ipa_stats_vif_stats stats;
	u32 err[ATH12K_DP_IPA_STATS_TX_ERR_MAX];
};

struct ath12k_dp_ipa_stats_rx_vif_stats {
	struct ath12k_dp_ipa_stats_vif_stats stats;
	struct ath12k_wbm_rx_stats err;
};

struct ath12k_dp_ipa_stats_vif_hw_stats {
	struct ath12k_dp_ipa_stats_tx_vif_stats tx;
	struct ath12k_dp_ipa_stats_rx_vif_stats rx;
};

void ath12k_dp_ipa_htt_vdev_txrx_stats_handler(struct ath12k_base *ab,
					       struct sk_buff *skb);

void ath12k_dp_ipa_rx_error_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_skb_rxcb *rxcb,
				  u8 link_id);

void ath12k_dp_ipa_tx_mcbc_stats(struct sk_buff *skb,
				 struct ath12k_pdev_dp *dp_pdev);

void ath12k_dp_ipa_tx_rx_mcbc_stats(struct ath12k_dp_ipa_stats_vif_stats *stats,
				    bool is_mcast,
				    bool is_bcast,
				    u64 skb_len);

int ath12k_dp_ipa_prepare_vif_vendor_event(struct sk_buff *vendor_event,
					   struct ath12k_vif *ahvif,
					   struct ath12k_telemetry_command *cmd);

int ath12k_dp_ipa_get_dp_vif_attr_len(struct ath12k_telemetry_command *cmd);

int ath12k_dp_ipa_get_common_nl_event_attr_size(void);

#endif /* ATH12K_DP_IPA_STATS_H */

