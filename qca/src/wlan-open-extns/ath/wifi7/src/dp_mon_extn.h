/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WIFI7_DP_MON_EXTN_H
#define WIFI7_DP_MON_EXTN_H
void ath12k_wifi7_dp_mon_tx_record_tlv(struct ath12k_pdev_mon_dp *mon_pdev,
				       void *ppdu_info,
				       u16 tlv_tag);
void ath12k_wifi7_dp_mon_rx_record_tlv(struct ath12k_pdev_mon_dp *mon_pdev,
				       void *ppdu_info,
				       u16 tlv_tag);
#endif //WIFI7_DP_MON2_EXTN_H
