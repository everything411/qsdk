/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../dp_mon.h"
#include "../hal_desc.h"
static enum
ath12k_dp_mon_tlv_logger_category ath12k_wifi7_dp_mon_rx_get_category(u16 tlv_tag)
{
	switch (tlv_tag) {
		case HAL_RX_PPDU_START:
		case HAL_RX_PPDU_START_USER_INFO:
			return PPDU_START;
		case HAL_RX_MPDU_START:
		case HAL_RX_MPDU_END:
		case HAL_RX_HEADER:
		case HAL_RX_MSDU_END:
		case HAL_MON_BUF_ADDR:
			return MPDU;
		case HAL_RX_USER_PPDU_END:
		case HAL_RX_PPDU_END:
		case HAL_PHYRX_RSSI_LEGACY:
		case HAL_PHYRX_L_SIG_B:
		case HAL_PHYRX_COMMON_USER_INFO:
		case HAL_PHYRX_DATA_DONE:
		case HAL_PHYRX_PKT_END_PART1:
		case HAL_PHYTX_PKT_END:
		case HAL_RXPCU_PPDU_END_INFO:
		case HAL_RX_PPDU_END_USER_STATS:
		case HAL_RX_PPDU_END_STATUS_DONE:
			return PPDU_END;
	}
	return NOT_SUPPORTED;
}

void ath12k_wifi7_dp_mon_rx_record_tlv(struct ath12k_pdev_mon_dp *mon_pdev,
				       void *ppdu_info,
				       u16 tlv_tag)
{
	struct hal_rx_mon_ppdu_info *rx_info = (struct hal_rx_mon_ppdu_info *)ppdu_info;
	struct ath12k_dp_mon_tlv_logger *logger;
	struct ath12k_dp_mon_tlv_info *buff;
	u16 *ppdu_start_idx;
	u16 *mpdu_idx;
	u16 *ppdu_end_idx;
	u8 category;

	logger = mon_pdev->pdev_mon_dp_extn.rx_tlv_log;

	if (!logger || !logger->tlv_logging_enable || !logger->buff)
		return;

	buff = (struct ath12k_dp_mon_tlv_info *)logger->buff;
	ppdu_start_idx = &logger->ppdu_start_idx;
	mpdu_idx = &logger->mpdu_idx;
	ppdu_end_idx = &logger->ppdu_end_idx;
	category = ath12k_wifi7_dp_mon_rx_get_category(tlv_tag);

	if (category == NOT_SUPPORTED)
		return;

	if (category == PPDU_START) {
		buff[*ppdu_start_idx].tlv_tag = tlv_tag;

		switch (tlv_tag) {
			case HAL_RX_PPDU_START:
				buff[*ppdu_start_idx].data = rx_info->ppdu_id;
				break;
			case HAL_RX_PPDU_START_USER_INFO:
				buff[*ppdu_start_idx].data =
					(FIELD_PREP(GENMASK(5, 0), rx_info->user_id) |
					FIELD_PREP(GENMASK(9, 6), rx_info->mcs) |
					FIELD_PREP(GENMASK(12, 10), rx_info->nss) |
					FIELD_PREP(GENMASK(15, 13), rx_info->reception_type) |
					FIELD_PREP(GENMASK(17, 16), rx_info->sgi));
				break;
			default:
				break;
		}

		(*ppdu_start_idx)++;

		return;
	}
	if (category == MPDU) {
		buff[*mpdu_idx].tlv_tag = tlv_tag;

		switch (tlv_tag) {
		case HAL_RX_MPDU_START:
			buff[*mpdu_idx].data =
				(FIELD_PREP(GENMASK(5, 0), rx_info->user_id) |
				 FIELD_PREP(BIT(6), logger->wrap_flag));
			break;
		case HAL_RX_MPDU_END:
			buff[*mpdu_idx].data =
				(FIELD_PREP(GENMASK(5, 0), rx_info->user_id) |
				 FIELD_PREP(BIT(6), rx_info->rs_fcs_err) |
				 FIELD_PREP(BIT(7), logger->wrap_flag));
			break;
		case HAL_RX_HEADER:
			buff[*mpdu_idx].data =
				FIELD_PREP(BIT(0), logger->wrap_flag);
			break;
		case HAL_RX_MSDU_END:
			buff[*mpdu_idx].data =
				(FIELD_PREP(GENMASK(5, 0), rx_info->user_id) |
				 FIELD_PREP(BIT(6), logger->wrap_flag));
			break;
		case HAL_MON_BUF_ADDR:
			buff[*mpdu_idx].data = FIELD_PREP(BIT(0), logger->wrap_flag);
			break;
		default:
			break;
		}

		if (*mpdu_idx < logger->max_mpdu_idx) {
			(*mpdu_idx)++;
		} else {
			*mpdu_idx -= DP_TLV_LOGGER_MAX_MPDU_TLV_NUM - 1;
			logger->wrap_flag ^= 1;
		}
	}

	if (category == PPDU_END) {
		buff[*ppdu_end_idx].tlv_tag = tlv_tag;
		switch (tlv_tag) {
			case HAL_RX_USER_PPDU_END:
			case HAL_RX_PPDU_END:
			case HAL_PHYRX_RSSI_LEGACY:
			case HAL_PHYRX_L_SIG_B:
			case HAL_PHYRX_COMMON_USER_INFO:
			case HAL_PHYRX_DATA_DONE:
			case HAL_PHYRX_PKT_END_PART1:
			case HAL_PHYTX_PKT_END:
			case HAL_RXPCU_PPDU_END_INFO:
			case HAL_RX_PPDU_END_USER_STATS:
			case HAL_RX_PPDU_END_STATUS_DONE:
			default:
				break;
		}
		if (*ppdu_end_idx < logger->max_ppdu_end_idx)
			(*ppdu_end_idx)++;
		else
			*ppdu_end_idx -= DP_TLV_LOGGER_MAX_PPDU_END_TLV_NUM - 1;
	}
}
EXPORT_SYMBOL(ath12k_wifi7_dp_mon_rx_record_tlv);

static enum ath12k_dp_mon_tlv_logger_category
ath12k_wifi7_dp_mon_tx_get_category(u16 tlv_tag)
{
	switch (tlv_tag) {
		case HAL_TX_FES_SETUP:
		case HAL_TXPCU_BUFFER_STATUS:
		case HAL_PCU_PPDU_SETUP_INIT:
		case HAL_SCH_CRITICAL_TLV_REFERENCE:
		case HAL_TX_PEER_ENTRY:
		case HAL_TX_RAW_OR_NATIVE_FRAME_SETUP:
		case HAL_TX_QUEUE_EXTENSION:
		case HAL_TX_FES_SETUP_COMPLETE:
		case HAL_FW2SW_MON:
		case HAL_SCHEDULER_END:
		case HAL_TQM_MPDU_GLOBAL_START:
			return PPDU_START;
		case HAL_TX_MPDU_START:
		case HAL_TX_MSDU_START:
		case HAL_TX_DATA:
		case HAL_TX_MSDU_END:
		case HAL_TX_MPDU_END:
			return MPDU;
		case HAL_TX_LAST_MPDU_FETCHED:
		case HAL_TX_LAST_MPDU_END:
		case HAL_PDG_TX_REQ:
		case HAL_TX_FES_STATUS_START_PPDU:
		case HAL_PHYTX_PPDU_HEADER_INFO_REQUEST:
		case HAL_MACTX_L_SIG_A:
		case HAL_TXPCU_PREAMBLE_DONE:
		case HAL_MACTX_USER_DESC_COMMON:
		case HAL_MACTX_SERVICE:
		case HAL_TXDMA_STOP_REQUEST:
		case HAL_TXPCU_USER_BUFFER_STATUS:
		case HAL_TX_FES_STATUS_USER_PPDU:
		case HAL_TX_MPDU_COUNT_TRANSFERND:
		case HAL_RX_START_PARAM:
		case HAL_TX_FES_STATUS_ACK_OR_BA:
		case HAL_TX_FES_STATUS_USER_RESPONSE:
		case HAL_TX_FES_STATUS_END:
		case HAL_TX_FES_STATUS_PROT:
		case HAL_MACTX_PHY_DESC:
		case HAL_MACTX_HE_SIG_A_SU:
			return PPDU_END;
	}
	return NOT_SUPPORTED;
}

void ath12k_wifi7_dp_mon_tx_record_tlv(struct ath12k_pdev_mon_dp *mon_pdev,
				       void *ppdu_info,
				       u16 tlv_tag)
{
	struct ath12k_dp_mon_tlv_logger *logger;
	struct ath12k_dp_mon_tlv_info *buff;
	u16 *ppdu_start_idx;
	u16 *mpdu_idx;
	u16 *ppdu_end_idx;
	u8 category;

	logger = mon_pdev->pdev_mon_dp_extn.tx_tlv_log;

	if (!logger || !logger->tlv_logging_enable || !logger->buff)
		return;

	buff = (struct ath12k_dp_mon_tlv_info *)logger->buff;
	ppdu_start_idx = &logger->ppdu_start_idx;
	mpdu_idx = &logger->mpdu_idx;
	ppdu_end_idx = &logger->ppdu_end_idx;
	category = ath12k_wifi7_dp_mon_tx_get_category(tlv_tag);

	if (category == NOT_SUPPORTED)
		return;

	if (category == PPDU_START) {
		buff[*ppdu_start_idx].tlv_tag = tlv_tag;
		switch (tlv_tag) {
		case HAL_TX_FES_SETUP:
		case HAL_TXPCU_BUFFER_STATUS:
		case HAL_PCU_PPDU_SETUP_INIT:
		case HAL_SCH_CRITICAL_TLV_REFERENCE:
		case HAL_TX_PEER_ENTRY:
		case HAL_TX_RAW_OR_NATIVE_FRAME_SETUP:
		case HAL_TX_QUEUE_EXTENSION:
		case HAL_TX_FES_SETUP_COMPLETE:
		case HAL_FW2SW_MON:
		case HAL_SCHEDULER_END:
		case HAL_TQM_MPDU_GLOBAL_START:
		default:
			break;
		}

		(*ppdu_start_idx)++;

		return;
	}

	if (category == MPDU) {
		buff[*mpdu_idx].tlv_tag = tlv_tag;
		switch (tlv_tag) {
		case HAL_TX_MPDU_START:
		case HAL_TX_MSDU_START:
		case HAL_TX_DATA:
		case HAL_TX_MSDU_END:
		case HAL_TX_MPDU_END:
		default:
			break;
		}
		if (*mpdu_idx < logger->max_mpdu_idx) {
			(*mpdu_idx)++;
		} else {
			*mpdu_idx -= DP_TLV_LOGGER_MAX_MPDU_TLV_NUM - 1;
			logger->wrap_flag ^= 1;
		}
		return;
	}

	if (category == PPDU_END) {
		buff[*ppdu_end_idx].tlv_tag = tlv_tag;
		switch (tlv_tag) {
		case HAL_TX_LAST_MPDU_FETCHED:
		case HAL_TX_LAST_MPDU_END:
		case HAL_PDG_TX_REQ:
		case HAL_TX_FES_STATUS_START_PPDU:
		case HAL_PHYTX_PPDU_HEADER_INFO_REQUEST:
		case HAL_MACTX_L_SIG_A:
		case HAL_TXPCU_PREAMBLE_DONE:
		case HAL_MACTX_USER_DESC_COMMON:
		case HAL_MACTX_SERVICE:
		case HAL_TXDMA_STOP_REQUEST:
		case HAL_TXPCU_USER_BUFFER_STATUS:
		case HAL_TX_FES_STATUS_USER_PPDU:
		case HAL_TX_MPDU_COUNT_TRANSFERND:
		case HAL_RX_START_PARAM:
		case HAL_TX_FES_STATUS_ACK_OR_BA:
		case HAL_TX_FES_STATUS_USER_RESPONSE:
		case HAL_TX_FES_STATUS_END:
		case HAL_TX_FES_STATUS_PROT:
		case HAL_MACTX_PHY_DESC:
		case HAL_MACTX_HE_SIG_A_SU:
		default:
			break;
		}

		if (*ppdu_end_idx < logger->max_ppdu_end_idx)
			(*ppdu_end_idx)++;
		else
			*ppdu_end_idx -= DP_TLV_LOGGER_MAX_PPDU_END_TLV_NUM - 1;
	}
}
EXPORT_SYMBOL(ath12k_wifi7_dp_mon_tx_record_tlv);
