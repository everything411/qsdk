/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_rx.h"
#include "hal_desc.h"

#ifndef HAL_MON_H
#define HAL_MON_H

#define HAL_PPDU_START_GET_CHAN			0x0000FFFF
#define HAL_PPDU_START_GET_FREQ			0xFFFF0000
#define HAL_PPDU_START_FREQ_MASK		16

#define HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA	0x3

/**
 * @HAL_RX_MON_PPDU_START: PPDU start TLV is decoded in HAL
 * @HAL_RX_MON_PPDU_END: PPDU end TLV is decoded in HAL
 * @HAL_RX_MON_PPDU_RESET: Not PPDU start and end TLV
 */
enum hal_ppdu_status {
	HAL_RX_MON_PPDU_START = 0,
	HAL_RX_MON_PPDU_END,
	HAL_RX_MON_PPDU_RESET,
};

#define HAL_RX_OFFSET(block, field) block##_##field##_OFFSET
#define HAL_RX_LSB(block, field) block##_##field##_LSB
#define HAL_RX_MASK(block, field) block##_##field##_MASK

#define HAL_RX_GET(_ptr, block, field) \
	(((*((volatile uint32_t *)_ptr + (HAL_RX_OFFSET(block, field)>>2))) & \
	HAL_RX_MASK(block, field)) >> \
	HAL_RX_LSB(block, field))

#define HAL_RX_FRAMECTRL_TYPE_MASK 0x0C
#define HAL_RX_GET_FRAME_CTRL_TYPE(fc) \
	(((fc) & HAL_RX_FRAMECTRL_TYPE_MASK) >> 2)
#define HAL_RX_FRAME_CTRL_TYPE_MGMT 0x0
#define HAL_RX_FRAME_CTRL_TYPE_CTRL 0x1
#define HAL_RX_FRAME_CTRL_TYPE_DATA 0x2

enum hal_rx_mpdu_info_sw_frame_group_id_type {
	HAL_MPDU_SW_FRAME_GROUP_NDP_FRAME = 0,
	HAL_MPDU_SW_FRAME_GROUP_MULTICAST_DATA,
	HAL_MPDU_SW_FRAME_GROUP_UNICAST_DATA,
	HAL_MPDU_SW_FRAME_GROUP_NULL_DATA,
	HAL_MPDU_SW_FRAME_GROUP_MGMT,
	HAL_MPDU_SW_FRAME_GROUP_MGMT_PROBE_REQ = 8,
	HAL_MPDU_SW_FRAME_GROUP_MGMT_BEACON = 12,
	HAL_MPDU_SW_FRAME_GROUP_CTRL = 20,
	HAL_MPDU_SW_FRAME_GROUP_CTRL_NDPA = 25,
	HAL_MPDU_SW_FRAME_GROUP_CTRL_BAR = 28,
	HAL_MPDU_SW_FRAME_GROUP_CTRL_RTS = 31,
	HAL_MPDU_SW_FRAME_GROUP_UNSUPPORTED = 36,
	HAL_MPDU_SW_FRAME_GROUP_MAX = 37,
};

#define HAL_LEGACY_MCS0  0
#define HAL_LEGACY_MCS1  1
#define HAL_LEGACY_MCS2  2
#define HAL_LEGACY_MCS3  3
#define HAL_LEGACY_MCS4  4
#define HAL_LEGACY_MCS5  5
#define HAL_LEGACY_MCS6  6
#define HAL_LEGACY_MCS7  7

#define HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(chain, word_1, word_2, \
					ppdu_info, rssi_info_tlv) \
	{						\
	ppdu_info->rssi_chain[chain][0] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_1,\
				   RSSI_PRI20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][1] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_1,\
				   RSSI_EXT20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][2] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_1,\
				   RSSI_EXT40_LOW20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][3] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_1,\
				   RSSI_EXT40_HIGH20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][4] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_2,\
				   RSSI_EXT80_LOW20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][5] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_2,\
				   RSSI_EXT80_LOW_HIGH20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][6] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_2,\
				   RSSI_EXT80_HIGH_LOW20_CHAIN##chain); \
	ppdu_info->rssi_chain[chain][7] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO_##word_2,\
				   RSSI_EXT80_HIGH20_CHAIN##chain); \
	}						\

#define HAL_RX_PPDU_UPDATE_RSSI(ppdu_info, rssi_info_tlv) \
	{HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(0, 0, 1, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(1, 2, 3, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(2, 4, 5, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(3, 6, 7, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(4, 8, 9, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(5, 10, 11, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(6, 12, 13, ppdu_info, rssi_info_tlv) \
	 HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(7, 14, 15, ppdu_info, rssi_info_tlv)} \

struct hal_tlv_parsed_hdr {
	u16 tag;
	u16 len;
	u16 userid;
	u8 *data;
};

struct hal_mon_rx_msdu_info {
	u8 is_decap_raw;
};

enum hal_rx_mon_status
ath12k_wifi6_hal_mon_rx_parse_status_tlv(struct ath12k_hal *hal,
                                        struct hal_rx_mon_ppdu_info *ppdu_info,
                                        struct hal_tlv_parsed_hdr *tlv_parsed_hdr);
void
ath12k_wifi6_dp_mon_rx_next_link_desc_get(struct hal_rx_msdu_link *msdu_link,
					  struct hal_rx_buf_info *buf_info);
#endif
