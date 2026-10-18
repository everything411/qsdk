/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon_extn.h"
#include "../dp_mon.h"

static void ath12k_dp_mon_free_tlv_logger(struct ath12k_dp_mon_tlv_logger **tlv_logger);

static int
ath12k_dp_mon_alloc_tlv_logger(struct ath12k_dp_mon_tlv_logger **tlv_logger)
{
	struct ath12k_dp_mon_tlv_logger *tlv_log;
	int buff_size = DP_TLV_LOGGER_MAX_TLV_LOGGING_SIZE *
			sizeof(struct ath12k_dp_mon_tlv_info);

	tlv_log = kzalloc(sizeof(*tlv_log), GFP_KERNEL);
	if (!tlv_log)
		return -ENOMEM;

	tlv_log->buff = kzalloc(buff_size, GFP_KERNEL);
	if (!tlv_log->buff) {
		kfree(tlv_log);
		return -ENOMEM;
	}

	tlv_log->curr_ppdu_pos = 0;
	tlv_log->wrap_flag = 0;
	tlv_log->ppdu_start_idx = 0;
	tlv_log->mpdu_idx = DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM;
	tlv_log->ppdu_end_idx = DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM +
				DP_TLV_LOGGER_MAX_MPDU_TLV_NUM;
	tlv_log->max_ppdu_start_idx = DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM - 1;
	tlv_log->max_mpdu_idx = DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM +
				DP_TLV_LOGGER_MAX_MPDU_TLV_NUM - 1;
	tlv_log->max_ppdu_end_idx = DP_TLV_LOGGER_MAX_TLVS_PER_PPDU - 1;
	tlv_log->tlv_logging_enable = true;

	*tlv_logger = tlv_log;
	return 0;
}

int ath12k_dp_mon_pdev_tlv_logger_init(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp_extn *mon_pdev;
	struct ath12k_base *ab;
	int ret;
	bool rx, tx;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return -EINVAL;

	ab = dp_pdev->dp->ab;

	if (ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_DISABLED)
		return 0;

	rx = (ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_RX_ENABLED ||
	      ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_RX_TX_ENABLED) ?
	      true : false;
	tx = (ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_TX_ENABLED ||
	      ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_RX_TX_ENABLED) ?
	      true : false;

	mon_pdev = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;

	if (rx) {
		ret = ath12k_dp_mon_alloc_tlv_logger(&mon_pdev->rx_tlv_log);
		if (ret != 0) {
			ath12k_err(ab,
				   "Failed to initialize RX TLV logger: %d\n", ret);
			return ret;
		}
	}

	if (tx) {
		ret = ath12k_dp_mon_alloc_tlv_logger(&mon_pdev->tx_tlv_log);
		if (ret != 0) {
			ath12k_err(ab,
				   "Failed to initialize TX TLV logger: %d\n", ret);
			if (rx)
				ath12k_dp_mon_free_tlv_logger(&mon_pdev->rx_tlv_log);
			return ret;
		}
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_MON,
		   "TLV loggers initialized successfully\n");

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_tlv_logger_init);

static void
ath12k_dp_mon_free_tlv_logger(struct ath12k_dp_mon_tlv_logger **tlv_logger)
{
	struct ath12k_dp_mon_tlv_logger *tlv_log = *tlv_logger;

	if (!tlv_log)
		return;

	if (tlv_log->buff) {
		tlv_log->tlv_logging_enable = false;
		kfree(tlv_log->buff);
		tlv_log->buff = NULL;
	}
	kfree(tlv_log);
	*tlv_logger = NULL;
}

void ath12k_dp_mon_pdev_tlv_logger_deinit(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp_extn *mon_pdev_extn;
	struct ath12k_base *ab;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return;

	ab = dp_pdev->dp->ab;

	if (ab->hw_params->tlv_logger_support == ATH12K_TLV_LOGGER_DISABLED)
		return;

	mon_pdev_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;

	ath12k_dp_mon_free_tlv_logger(&mon_pdev_extn->rx_tlv_log);

	ath12k_dp_mon_free_tlv_logger(&mon_pdev_extn->tx_tlv_log);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_tlv_logger_deinit);


void ath12k_dp_mon_record_index_update(struct ath12k_pdev_mon_dp *mon_pdev, int mode)
{
	struct ath12k_dp_mon_tlv_logger *logger = NULL;

	if (mode == TLV_LOGGER_RX_MODE)
		logger = mon_pdev->pdev_mon_dp_extn.rx_tlv_log;
	else
		logger = mon_pdev->pdev_mon_dp_extn.tx_tlv_log;

	if (!logger)
		return;

	(logger->curr_ppdu_pos + 1 == DP_TLV_LOGGER_MAX_NUM_PPDU_RECORD) ?
		logger->curr_ppdu_pos = 0 : logger->curr_ppdu_pos++;

	logger->wrap_flag = 0;
	logger->ppdu_start_idx = logger->curr_ppdu_pos *
		DP_TLV_LOGGER_MAX_TLVS_PER_PPDU;
	logger->mpdu_idx = logger->ppdu_start_idx +
		DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM;
	logger->ppdu_end_idx = logger->mpdu_idx + DP_TLV_LOGGER_MAX_MPDU_TLV_NUM;
	logger->max_ppdu_start_idx = logger->ppdu_start_idx +
		DP_TLV_LOGGER_MAX_PPDU_START_TLV_NUM - 1;
	logger->max_mpdu_idx = logger->mpdu_idx +
		DP_TLV_LOGGER_MAX_MPDU_TLV_NUM - 1;
	logger->max_ppdu_end_idx = logger->ppdu_end_idx +
		DP_TLV_LOGGER_MAX_PPDU_END_TLV_NUM - 1;
}
EXPORT_SYMBOL(ath12k_dp_mon_record_index_update);

void ath12k_dp_mon_tlv_logger_clear_buf(struct ath12k_pdev_mon_dp *mon_pdev, int mode)
{
	struct ath12k_dp_mon_tlv_logger *logger = NULL;
	struct ath12k_dp_mon_tlv_info *log_info = NULL;

	if (mode == TLV_LOGGER_RX_MODE)
		logger = mon_pdev->pdev_mon_dp_extn.rx_tlv_log;
	else
		logger = mon_pdev->pdev_mon_dp_extn.tx_tlv_log;

	if (!logger)
		return;

	log_info = (struct ath12k_dp_mon_tlv_info *)logger->buff;

	if (!log_info)
		return;

	memset(&log_info[logger->ppdu_start_idx], 0,
	       DP_TLV_LOGGER_MAX_TLVS_PER_PPDU * sizeof(struct ath12k_dp_mon_tlv_info));
}
EXPORT_SYMBOL(ath12k_dp_mon_tlv_logger_clear_buf);
