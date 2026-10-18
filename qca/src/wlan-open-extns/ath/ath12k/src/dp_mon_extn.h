/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DP_MON_EXTN_H
#define DP_MON_EXTN_H
#include "../core.h"
#include "../wmi.h"
#include "../mac.h"
#include "../debug.h"
#include "../vendor.h"

#define DP_TLV_LOGGER_MAX_TLV_LOGGING_SIZE		1024	/* Total buffer size */
#define DP_TLV_LOGGER_MAX_NUM_PPDU_RECORD		4	/* Number of PPDU slots */
#define DP_TLV_LOGGER_MAX_TLVS_PER_PPDU			255	/* TLVs per PPDU slot */
#define DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM		38	/* PPDU start TLVs */
#define DP_TLV_LOGGER_MAX_MPDU_TLV_NUM			160	/* MPDU-level TLVs */
#define DP_TLV_LOGGER_MAX_PPDU_END_TLV_NUM		57	/* PPDU end TLVs */


struct ath12k_dp_mon_tlv_info {
	u32 tlv_tag:10;
	u32 data:22;
};

struct ath12k_dp_mon_tlv_logger {
	void *buff;
	u16 curr_ppdu_pos;
	u16 ppdu_start_idx;
	u16 mpdu_idx;
	u16 ppdu_end_idx;
	u16 max_ppdu_start_idx;
	u16 max_mpdu_idx;
	u16 max_ppdu_end_idx;
	u8 wrap_flag;
	u8 tlv_logging_enable;
};

enum ath12k_dp_mon_tlv_logger_category {
	PPDU_START,
	MPDU,
	PPDU_END,
	NOT_SUPPORTED,
};

enum tlv_logger_mode {
	TLV_LOGGER_RX_MODE,
	TLV_LOGGER_TX_MODE,
};

int ath12k_dp_mon_pdev_tlv_logger_init(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_pdev_tlv_logger_deinit(struct ath12k_pdev_dp *dp_pdev);
void ath12k_dp_mon_record_index_update(struct ath12k_pdev_mon_dp *mon_pdev, int mode);
void ath12k_dp_mon_tlv_logger_clear_buf(struct ath12k_pdev_mon_dp *mon_pdev, int mode);

#endif //DP_MON_EXTN_H
