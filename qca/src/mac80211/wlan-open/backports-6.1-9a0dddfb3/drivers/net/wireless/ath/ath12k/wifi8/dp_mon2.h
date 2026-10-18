/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DPMON2_H
#define ATH12K_DPMON2_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

#define ATH12K_WIFI8_DP_MON_RX_HDR_LEN		256

/*
 * Pack one 6-bit data MPDU TLV subtype slot at bit position @shift.
 * If @field is zero the subtype is not subscribed — leave slot empty.
 * Otherwise set TLV mask bits and per-msdu/per-ppdu header bits.
 */
#define HTT_FP_DATA_TLV_SUBTYPE(field, shift, hdr_bits) \
	((u32)((field) ? (u8)(field) | (hdr_bits) : 0) << (shift))

struct workqueue_struct;

int ath12k_wifi8_dp_mon_rx_dual_ring_setup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi8_dp_mon_rx_dual_ring_cleanup_ppdu_desc(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi8_dp_mon_rx_wq_init(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi8_dp_mon_rx_wq_deinit(struct ath12k_pdev_dp *dp_pdev, bool destroy);
int ath12k_wifi8_dp_ext_mon_validate_request(struct ath12k_pdev_dp *dp_pdev,
					     const struct ath12k_ext_mon_config *req);
void ath12k_wifi8_htt_tx_mon_cfg_fill_extended_wmask(
		struct htt_tx_mon_ring_selection_cfg_cmd *cmd,
		const struct htt_tx_ring_tlv_filter *htt_tlv_filter);
void
ath12k_wifi8_dp_ext_mon_setup_rx_filter(struct htt_rx_ring_tlv_filter *tlv_filter,
					const struct ath12k_dp_rx_ext_mon *rx_ext_mon);
#endif
