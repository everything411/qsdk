// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * QCN9625 / QCN9589 HW1.0 (E3R65) — V1-specific implementations.
 *
 * This file contains two sets of HW1.0-specific code:
 *
 * 1. Monitor ops (RX monitor MPDU-start path):
 *    HW1.0 silicon divergences vs v2.0 (E3R86):
 *    a. struct hal_rx_mpdu_start — info0 sits one word earlier on v1.0
 *       (see hal_rx_mpdu_start_hw10 below).
 *    b. struct hal_rx_mon_mpdu_start_compact — matching member reorder
 *       (see hal_rx_mon_mpdu_start_compact_hw10 below).
 *    c. The RX_MON_MPDU_START WMASK encrypt_type selector is BIT(0) on v1.0
 *       vs BIT(1) on v2.0 (see RX_MON_MPDU_START_WMASK_HW10 below).
 *    These feed exactly two hal_mon_ops entries: rx_mpdu_start_info_get and
 *    get_mon_mpdu_start_wmask, which the HW1.0 mon-ops-init helper below
 *    overrides on top of the generic hal_qcn9625_mon_ops_base table (defined
 *    in hal_mon_qcn9625.c).
 *
 * 2. Compact RX descriptor access operations (V1 layout):
 *    All compact RX descriptor field accesses for the V1 layout
 *    (struct hal_rx_desc_qcn9625_compact_v1). These are wired into
 *    hal_qcn9625_v1_ops. The default (V2) implementations live in
 *    hal_qcn9625.c and are wired into hal_qcn9625_ops.
 *    The V1 structures are defined in hal_rx_desc.h alongside the V2 structs.
 */

#include <linux/types.h>
#include <linux/bitfield.h>
#include <linux/if_ether.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include "hal_desc.h"
#include "hal_rx_desc.h"
#include "hal_rx.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "../dp_cmn.h"
#include "hal_mon.h"
#include "hal_qcn9625.h"
#include "hal.h"

/* -----------------------------------------------------------------------
 * V1-specific field definitions
 *
 * The following macros are frozen at the V1 hardware field positions and
 * bit-masks.  They are defined here so that changes to the shared V2 headers in
 * hal_rx_desc.h / hal_mon.h cannot silently alter V1 descriptor parsing.
 * ------------------------------------------------------------------------
 */

/* --- RX_MSDU_END compact-descriptor field masks (V1 layout) --- */
#define RX_MSDU_END_INFO5_FIRST_MSDU_V1			BIT(12)
#define RX_MSDU_END_INFO5_LAST_MSDU_V1			BIT(13)
#define RX_MSDU_END_INFO5_L3_HEADER_PADDING_V1		GENMASK(11, 10)
#define RX_MSDU_END_INFO5_TID_V1			GENMASK(6, 3)
#define RX_MSDU_END_INFO5_FR_DS_V1			BIT(14)
#define RX_MSDU_END_INFO5_TO_DS_V1			BIT(2)
#define RX_MSDU_END_INFO5_DA_IS_MCBC_V1			BIT(9)
#define RX_MSDU_END_INFO7_FLOW_IDX_V1			GENMASK(25, 6)
#define RX_MSDU_END_INFO8_CCE_MATCH_V1			BIT(14)
#define RX_MSDU_END_INFO8_FLOW_IDX_INVALID_V1		BIT(13)
#define RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT_V1		BIT(12)
#define RX_MSDU_END_INFO11_MSDU_LENGTH_V1		GENMASK(13, 0)
#define RX_MSDU_END_INFO12_DECAP_FORMAT_V1		GENMASK(9, 8)
#define RX_MSDU_END_INFO12_MESH_CONTROL_PRESENT_V1	BIT(22)
#define RX_MSDU_END_INFO12_IPV4_PROTO_V1		BIT(10)
#define RX_MSDU_END_INFO12_IPV6_PROTO_V1		BIT(11)
#define RX_MSDU_END_INFO13_SGI_V1			GENMASK(13, 12)
#define RX_MSDU_END_INFO13_RATE_MCS_V1			GENMASK(18, 14)
#define RX_MSDU_END_INFO13_RECEIVE_BANDWIDTH_V1		GENMASK(21, 19)
#define RX_MSDU_END_INFO13_USER_RSSI_V1			GENMASK(7, 0)
#define RX_MSDU_END_INFO13_PKT_TYPE_V1			GENMASK(11, 8)
#define RX_MSDU_END_INFO13_MIMO_SS_BITMAP_V1		GENMASK(30, 25)
#define RX_MSDU_END_INFO14_FCS_ERR_V1			BIT(31)
#define RX_MSDU_END_INFO14_DECRYPT_ERR_V1		BIT(29)
#define RX_MSDU_END_INFO14_TKIP_MIC_ERR_V1		BIT(28)
#define RX_MSDU_END_INFO14_A_MSDU_ERROR_V1		BIT(12)
#define RX_MSDU_END_INFO14_OVERFLOW_ERR_V1		BIT(16)
#define RX_MSDU_END_INFO14_MSDU_LENGTH_ERR_V1		BIT(17)
#define RX_MSDU_END_INFO14_MPDU_LENGTH_ERR_V1		BIT(27)
#define RX_MSDU_END_INFO14_TCP_UDP_CHKSUM_FAIL_V1	BIT(18)
#define RX_MSDU_END_INFO14_IP_CHKSUM_FAIL_V1		BIT(19)
#define RX_MSDU_END_INFO15_DECRYPT_STATUS_CODE_V1	GENMASK(2, 0)
#define RX_MSDU_END_INFO15_MSDU_DONE_V1			BIT(31)
#define RX_MSDU_END_64_TLV_SRC_LINK_ID_V1		GENMASK(24, 22)

/* --- RX_MPDU_INFO compact-descriptor field masks (V1 layout) --- */
#define RX_MPDU_INFO_INFO2_FRAME_ENCRYPTION_INFO_VALID_V1	BIT(9)
#define RX_MPDU_INFO_INFO2_MAC_ADDR_AD2_VALID_V1		BIT(3)
#define RX_MPDU_INFO_INFO2_MAC_ADDR_AD4_VALID_V1		BIT(5)
#define RX_MPDU_INFO_INFO2_MPDU_FRAME_CONTROL_VALID_V1		BIT(0)
#define RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_CONTROL_VALID_V1	BIT(6)
#define RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_NUMBER_V1		GENMASK(31, 20)
#define RX_MPDU_INFO_INFO4_KEY_ID_OCTET_V1			GENMASK(7, 0)

/* --- PN byte extraction helpers (V1 layout) --- */
#define HAL_RX_MPDU_INFO_PN_GET_BYTE1_V1(__val) \
	le32_get_bits((__val), GENMASK(7, 0))
#define HAL_RX_MPDU_INFO_PN_GET_BYTE2_V1(__val) \
	le32_get_bits((__val), GENMASK(15, 8))
#define HAL_RX_MPDU_INFO_PN_GET_BYTE3_V1(__val) \
	le32_get_bits((__val), GENMASK(23, 16))
#define HAL_RX_MPDU_INFO_PN_GET_BYTE4_V1(__val) \
	le32_get_bits((__val), GENMASK(31, 24))

/* v1.0 descriptor layouts. Field-extraction masks (HAL_RX_MPDU_START_*) are
 * identical to v2.0 — only the word ordering differs.
 */
struct hal_rx_mpdu_start_hw10 {
	__le32 rsvd0;
	__le32 info0;
	__le32 rsvd1[6];
	__le32 info1;
	__le32 rsvd2;
	__le16 rsvd3;
	__le16 sw_peer_id;
	__le16 info2;
	__le16 phy_ppdu_id;
	__le32 info3;
	__le32 info4;
	__le16 mpdu_frame_control_field;
	__le16 rsvd4;
	__le32 rsvd5;
	__le16 rsvd6;
	__le16 mac_addr_ad2_15_0;
	__le32 mac_addr_ad2_47_16;
	__le32 rsvd7[20];
} __packed;

struct hal_rx_mon_mpdu_start_compact_hw10 {
	__le32 rsvd0;
	__le32 info0;
	__le32 info1;
	__le32 rsvd1;
	__le16 rsvd2;
	__le16 sw_peer_id;
	__le16 info2;
	__le16 phy_ppdu_id;
	__le32 info3;
	__le32 info4;
	__le16 mpdu_frame_control_field;
	__le16 rsvd3;
	__le32 rsvd4;
	__le16 rsvd5;
	__le16 mac_addr_ad2_15_0;
	__le32 mac_addr_ad2_47_16;
} __packed;

/*
 * v1.0 WMASK: the encrypt_type selector is BIT(0), vs BIT(1) on v2.0. All
 * other selectors match v2.0 and are reused from hal_mon.h.
 */
#define MPDU_START_SELECT_INFO0_ENCYRPT_TYP_HW10		BIT(0)

#define RX_MON_MPDU_START_WMASK_HW10					\
		(MPDU_START_SELECT_INFO0_ENCYRPT_TYP_HW10 |		\
		 MPDU_START_SELECT_INFO1_FC_ADDR2_DS_RETRY |		\
		 MPDU_START_SELECT_INFO2 |				\
		 MPDU_START_SELECT_INFO3_INFO4 |			\
		 MPDU_START_SELECT_FC |					\
		 MPDU_START_SELECT_AD2)

/*
 * Local copies of two static helpers from hal_mon.c. They are not exported in
 * any header, and their shared home would be hal_mon.h — but hal_mon.h does not
 * include dp_rx.h (DP_RX_DECAP_TYPE_RAW) or hal_mon_cmn.h (hal_rx_mon_ppdu_info)
 * that set_decap_type_raw_mode needs, so copying here avoids adding those
 * includes to hal_mon.h just for this file.
 */
static __always_inline void
ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(u16 addr_l16, u32 addr_h32, u8 *addr)
{
	memcpy(addr, &addr_l16, 2);
	memcpy(addr + 2, &addr_h32, ETH_ALEN - 2);
}

static void
ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode_hw10(
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u8 user_id = ppdu_info->user_id;

	if (ppdu_info->nrp_info.fc_valid && ppdu_info->mpdu_info[user_id].decap_type) {
		if (ieee80211_is_mgmt(ppdu_info->nrp_info.frame_control) ||
		    ieee80211_is_ctl(ppdu_info->nrp_info.frame_control) ||
		    ppdu_info->grp_id == HAL_MPDU_START_SW_FRAME_GRP_NULL_DATA) {
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		}
	}
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_hw10(const void *tlv_data,
						 u32 userid,
						 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mpdu_start_hw10 *mpdu_start =
		(struct hal_rx_mpdu_start_hw10 *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_TO_DS);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_FC_VALID);

	if (user_id < HAL_MAX_UL_MU_USERS) {
		ppdu_info->mpdu_info[user_id].raw_mpdu =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU);
		if (ppdu_info->mpdu_info[user_id].raw_mpdu)
			ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
		else
			ppdu_info->mpdu_info[user_id].decap_type =
				u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE);
	}

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);
	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(addr_16, addr_32,
							   ppdu_info->nrp_info.mac_addr2);

	if (user_id < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userstats[user_id].sw_peer_id = ppdu_info->peer_id;
		ppdu_info->userstats[user_id].ampdu_id =
			le16_to_cpu(mpdu_start->phy_ppdu_id);
		ppdu_info->userstats[user_id].filter_category =
			u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_FILTER_CAT);
		ppdu_info->userstats[user_id].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY);
		ppdu_info->userstats[user_id].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[user_id].frame_control =
				ppdu_info->nrp_info.frame_control;
	}

	ath12k_wifi8_hal_mon_rx_set_decap_type_raw_mode_hw10(ppdu_info);
}

static __always_inline void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact_hw10(
					const void *tlv_data, u32 userid,
					struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_mon_mpdu_start_compact_hw10 *mpdu_start =
		(struct hal_rx_mon_mpdu_start_compact_hw10 *)tlv_data;
	u16 peer_id, addr_16;
	u32 info[5], addr_32;
	u8 user_id = ppdu_info->user_id;

	info[0] = __le32_to_cpu(mpdu_start->info0);
	info[1] = __le32_to_cpu(mpdu_start->info1);
	info[2] = le16_to_cpu(mpdu_start->info2);
	info[3] = __le32_to_cpu(mpdu_start->info3);
	info[4] = __le32_to_cpu(mpdu_start->info4);

	ppdu_info->grp_id = u32_get_bits(info[2],
					 HAL_RX_MPDU_START_INFO2_SW_GRP_ID_CMPCT);

	peer_id = le16_to_cpu(mpdu_start->sw_peer_id);
	if (peer_id)
		ppdu_info->peer_id = peer_id;

	ppdu_info->mpdu_retry += u32_get_bits(info[1],
					      HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
	ppdu_info->nrp_info.mac_addr2_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_ADDR2_VALID_CMPCT);
	ppdu_info->nrp_info.to_ds_flag =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_TO_DS_CMPCT);
	ppdu_info->nrp_info.fc_valid =
		u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_FC_VALID_CMPCT);
	ppdu_info->mpdu_info[user_id].raw_mpdu =
		u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_RAW_MPDU_CMPCT);
	if (ppdu_info->mpdu_info[user_id].raw_mpdu)
		ppdu_info->mpdu_info[user_id].decap_type = DP_RX_DECAP_TYPE_RAW;
	else
		ppdu_info->mpdu_info[user_id].decap_type =
			u32_get_bits(info[3], HAL_RX_MPDU_START_INFO3_DECAP_TYPE_CMPCT);

	ppdu_info->mpdu_len += u32_get_bits(info[4],
					    HAL_RX_MPDU_START_INFO4_MPDU_LEN_CMPCT);
	ppdu_info->nrp_info.mcast_bcast =
		u32_get_bits(info[4], HAL_RX_MPDU_START_INFO4_MCAST_BCAST_CMPCT);
	ppdu_info->nrp_info.frame_control =
		le16_to_cpu(mpdu_start->mpdu_frame_control_field);

	addr_16 = le16_to_cpu(mpdu_start->mac_addr_ad2_15_0);
	addr_32 = le32_to_cpu(mpdu_start->mac_addr_ad2_47_16);
	if (ppdu_info->nrp_info.fc_valid &&
	    ppdu_info->nrp_info.to_ds_flag &&
	    ppdu_info->nrp_info.mac_addr2_valid)
		ath12k_wifi8_hal_mon_get_nrp_mac_addr_hw10(addr_16, addr_32,
							   ppdu_info->nrp_info.mac_addr2);

	if (user_id < HAL_MAX_UL_MU_USERS) {
		ppdu_info->userstats[user_id].sw_peer_id = ppdu_info->peer_id;
		ppdu_info->userstats[user_id].ampdu_id =
			le16_to_cpu(mpdu_start->phy_ppdu_id);
		ppdu_info->userstats[user_id].filter_category =
			u32_get_bits(info[2], HAL_RX_MPDU_START_INFO2_FILTER_CAT_CMPCT);
		ppdu_info->userstats[user_id].mpdu_retry +=
			u32_get_bits(info[1], HAL_RX_MPDU_START_INFO1_MPDU_RETRY_CMPCT);
		ppdu_info->userstats[user_id].frame_control_info_valid =
				ppdu_info->nrp_info.fc_valid;
		ppdu_info->userstats[user_id].frame_control =
				ppdu_info->nrp_info.frame_control;
	}
}

static u32
ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get_hw10(void)
{
	return RX_MON_MPDU_START_WMASK_HW10;
}

static void
ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse_hw10(const void *tlv_data,
						   u32 userid,
						   struct hal_rx_mon_ppdu_info *ppdu_info,
						   u32 tlv_len)
{
	if (likely(tlv_len < sizeof(struct hal_rx_mpdu_start_hw10)))
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_compact_hw10(tlv_data,
									 userid,
									 ppdu_info);
	else
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_get_hw10(tlv_data, userid,
								 ppdu_info);
}

/*
 * HW1.0 monitor ops. Start from the generic QCN9625 base table and override
 * only the two version-specific entries. hal_qcn9625_mon_ops_base is defined in
 * hal_mon_qcn9625.c and holds every shared entry, so those cannot drift or be
 * silently dropped from the HW1.0 table.
 */
static struct hal_mon_ops hal_qcn9625_hw10_mon_ops;

void ath12k_wifi8_hal_qcn9625_hw10_mon_ops_init(struct ath12k_hal *hal)
{
	hal_qcn9625_hw10_mon_ops = hal_qcn9625_mon_ops_base;
	hal_qcn9625_hw10_mon_ops.get_mon_mpdu_start_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get_hw10;
	hal_qcn9625_hw10_mon_ops.rx_mpdu_start_info_get =
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse_hw10;

	hal->hal_mon_ops = &hal_qcn9625_hw10_mon_ops;
}

/* -----------------------------------------------------------------------
 * QCN9625 HW1.0 compact RX descriptor access operations
 *
 * All functions below operate on the V1 compact descriptor layout
 * (struct hal_rx_desc_qcn9625_compact_v1) and are wired into
 * hal_qcn9625_v1_ops via ath12k_wifi8_hal_init_v1_ops() above.
 * -----------------------------------------------------------------------
 */

/* MSDU-end field accessors */

bool ath12k_wifi8_hal_rx_h_first_msdu_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			       RX_MSDU_END_INFO5_FIRST_MSDU_V1);
}

bool ath12k_wifi8_hal_rx_h_last_msdu_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			       RX_MSDU_END_INFO5_LAST_MSDU_V1);
}

u8 ath12k_wifi8_hal_rx_h_l3pad_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			     RX_MSDU_END_INFO5_L3_HEADER_PADDING_V1);
}

u8 ath12k_wifi8_hal_rx_h_decap_type_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info12,
			     RX_MSDU_END_INFO12_DECAP_FORMAT_V1);
}

u8 ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info12,
			     RX_MSDU_END_INFO12_MESH_CONTROL_PRESENT_V1);
}

bool ath12k_wifi8_hal_rx_h_is_ip_valid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!(le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info12,
				RX_MSDU_END_INFO12_IPV4_PROTO_V1) ||
		  le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info12,
				RX_MSDU_END_INFO12_IPV6_PROTO_V1));
}

u16 ath12k_wifi8_hal_rx_h_msdu_len_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info11,
			     RX_MSDU_END_INFO11_MSDU_LENGTH_V1);
}

u8 ath12k_wifi8_hal_rx_h_sgi_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_SGI_V1);
}

u8 ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_RATE_MCS_V1);
}

u8 ath12k_wifi8_hal_rx_h_rx_bw_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_RECEIVE_BANDWIDTH_V1);
}

u32 ath12k_wifi8_hal_rx_h_freq_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.phy_meta_data);
}

u8 ath12k_wifi8_hal_rx_h_snr_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_USER_RSSI_V1);
}

u8 ath12k_wifi8_hal_rx_h_pkt_type_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_PKT_TYPE_V1);
}

u8 ath12k_wifi8_hal_rx_h_nss_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info13,
			     RX_MSDU_END_INFO13_MIMO_SS_BITMAP_V1);
}

u8 ath12k_wifi8_hal_rx_h_tid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			     RX_MSDU_END_INFO5_TID_V1);
}

u8 ath12k_wifi8_hal_rx_h_from_ds_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			     RX_MSDU_END_INFO5_FR_DS_V1);
}

u8 ath12k_wifi8_hal_rx_h_to_ds_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info5,
			     RX_MSDU_END_INFO5_TO_DS_V1);
}

bool ath12k_wifi8_hal_rx_h_msdu_done_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info15,
			       RX_MSDU_END_INFO15_MSDU_DONE_V1);
}

bool ath12k_wifi8_hal_rx_h_l4_cksum_fail_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info14,
			       RX_MSDU_END_INFO14_TCP_UDP_CHKSUM_FAIL_V1);
}

bool ath12k_wifi8_hal_rx_h_ip_cksum_fail_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info14,
			       RX_MSDU_END_INFO14_IP_CHKSUM_FAIL_V1);
}

bool ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625_v1(struct hal_rx_desc *desc)
{
	return (le32_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info15,
			      RX_MSDU_END_INFO15_DECRYPT_STATUS_CODE_V1) ==
		RX_DESC_DECRYPT_STATUS_CODE_OK);
}

u8 ath12k_wifi8_hal_rx_get_msdu_src_link_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le64_get_bits(desc->u.qcn9625_compact_v1.msdu_end.msdu_end_tag,
			     RX_MSDU_END_64_TLV_SRC_LINK_ID_V1);
}

bool ath12k_wifi8_hal_rx_h_is_da_mcbc_qcn9625_v1(struct hal_rx_desc *desc)
{
	return (ath12k_wifi8_hal_rx_h_first_msdu_qcn9625_v1(desc) &&
		(__le16_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.info5) &
		 RX_MSDU_END_INFO5_DA_IS_MCBC_V1));
}

void ath12k_wifi8_hal_rx_desc_end_tlv_copy_qcn9625_v1(struct hal_rx_desc *fdesc,
						      struct hal_rx_desc *ldesc)
{
	fdesc->u.qcn9625_compact_v1.msdu_end = ldesc->u.qcn9625_compact_v1.msdu_end;
}

void ath12k_wifi8_hal_rxdesc_set_msdu_len_qcn9625_v1(struct hal_rx_desc *desc, u16 len)
{
	u32 info = __le32_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.info11);

	info = u32_replace_bits(info, len, RX_MSDU_END_INFO11_MSDU_LENGTH_V1);
	desc->u.qcn9625_compact_v1.msdu_end.info11 = __cpu_to_le32(info);
}

u8 *ath12k_wifi8_hal_rx_desc_get_msdu_payload_qcn9625_v1(struct hal_rx_desc *desc)
{
	return &desc->u.qcn9625_compact_v1.msdu_payload[0];
}

void ath12k_wifi8_hal_rx_desc_get_fse_info_qcn9625_v1(struct hal_rx_desc *desc,
						      struct rx_mpdu_desc_info
						      *rx_mpdu_info)
{
	__le32 flow_idx_info = desc->u.qcn9625_compact_v1.msdu_end.info8;

	rx_mpdu_info->flow_idx_timeout =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT_V1);
	rx_mpdu_info->flow_idx_invalid =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO8_FLOW_IDX_INVALID_V1);

	rx_mpdu_info->flow_info.flow_metadata =
		le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.fse_metadata,
			      ATH12K_DP_RX_FSE_FLOW_METADATA_MASK);
}

u32 ath12k_wifi8_hal_rx_get_fse_metadata_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.fse_metadata);
}

u16 ath12k_wifi8_hal_rx_get_cce_metadata_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.cce_metadata);
}

bool ath12k_wifi8_hal_rx_get_cce_match_bit_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact_v1.msdu_end.info8,
			     RX_MSDU_END_INFO8_CCE_MATCH_V1);
}

void ath12k_wifi8_hal_rx_get_flow_params_qcn9625_v1(struct hal_rx_desc *desc,
						    u32 *flow_idx,
						    bool *flow_idx_invalid,
						    bool *flow_idx_timeout)
{
	__le32 info7 = desc->u.qcn9625_compact_v1.msdu_end.info7;
	__le16 info8 = desc->u.qcn9625_compact_v1.msdu_end.info8;

	*flow_idx = le32_get_bits(info7, RX_MSDU_END_INFO7_FLOW_IDX_V1);
	*flow_idx_invalid = le16_get_bits(info8, RX_MSDU_END_INFO8_FLOW_IDX_INVALID_V1);
	*flow_idx_timeout = le16_get_bits(info8, RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT_V1);
}

/* MPDU-start field accessors */

bool ath12k_wifi8_hal_encrypt_valid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_FRAME_ENCRYPTION_INFO_VALID_V1);
}

bool ath12k_wifi8_hal_rx_h_seq_ctrl_valid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_CONTROL_VALID_V1);
}

bool ath12k_wifi8_hal_rx_h_fc_valid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact_v1.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_MPDU_FRAME_CONTROL_VALID_V1);
}

u16 ath12k_wifi8_hal_rx_h_seq_no_qcn9625_v1(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact_v1.mpdu_start.info2,
			     RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_NUMBER_V1);
}

u32 ath12k_wifi8_hal_rx_h_peer_meta_data_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact_v1.mpdu_start.peer_meta_data);
}

bool ath12k_wifi8_hal_rxdesc_mac_addr2_valid_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact_v1.mpdu_start.info2) &
			     RX_MPDU_INFO_INFO2_MAC_ADDR_AD2_VALID_V1;
}

u8 *ath12k_wifi8_hal_rxdesc_get_mpdu_start_addr2_qcn9625_v1(struct hal_rx_desc *desc)
{
	return ath12k_wifi8_hal_rxdesc_mac_addr2_valid_qcn9625_v1(desc) ?
		desc->u.qcn9625_compact_v1.mpdu_start.addr2 : NULL;
}

u16 ath12k_wifi8_hal_rxdesc_get_mpdu_frame_ctrl_qcn9625_v1(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.qcn9625_compact_v1.mpdu_start.frame_ctrl);
}

/* Descriptor layout / size queries */

u32 ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625_v1(void)
{
	return offsetof(struct hal_rx_desc_qcn9625_compact_v1, mpdu_start);
}

u32 ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625_v1(void)
{
	return offsetof(struct hal_rx_desc_qcn9625_compact_v1, msdu_end);
}

u32 ath12k_wifi8_hal_get_rx_desc_size_qcn9625_v1(void)
{
	return sizeof(struct hal_rx_desc_qcn9625_compact_v1);
}

u16 ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625_v1(void)
{
	return QCN9625_MPDU_START_WMASK_V1;
}

u32 ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625_v1(void)
{
	return QCN9625_MSDU_END_WMASK_V1;
}

/* Multi-field / composite accessors */

u32 ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625_v1(struct hal_rx_desc *desc)
{
	u32 info = __le32_to_cpu(desc->u.qcn9625_compact_v1.msdu_end.info14);
	u32 errmap = 0;

	if (info & RX_MSDU_END_INFO14_FCS_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_FCS;

	if (info & RX_MSDU_END_INFO14_DECRYPT_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_DECRYPT;

	if (info & RX_MSDU_END_INFO14_TKIP_MIC_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_TKIP_MIC;

	if (info & RX_MSDU_END_INFO14_A_MSDU_ERROR_V1)
		errmap |= HAL_RX_MPDU_ERR_AMSDU_ERR;

	if (info & RX_MSDU_END_INFO14_OVERFLOW_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_OVERFLOW;

	if (info & RX_MSDU_END_INFO14_MSDU_LENGTH_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_MSDU_LEN;

	if (info & RX_MSDU_END_INFO14_MPDU_LENGTH_ERR_V1)
		errmap |= HAL_RX_MPDU_ERR_MPDU_LEN;

	return errmap;
}

void
ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625_v1(struct hal_rx_desc *desc,
						      u8 *crypto_hdr,
						      enum hal_encrypt_type encype)
{
	unsigned int key_id;
	struct rx_mpdu_start_qcn9625_compact_v1 *mpdu_start =
						&desc->u.qcn9625_compact_v1.mpdu_start;

	switch (encype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2_V1(mpdu_start->pn[0]);
		crypto_hdr[1] = 0;
		crypto_hdr[2] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1_V1(mpdu_start->pn[0]);
		break;
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1_V1(mpdu_start->pn[0]);
		crypto_hdr[1] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2_V1(mpdu_start->pn[0]);
		crypto_hdr[2] = 0;
		break;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		return;
	}
	key_id = le32_get_bits(desc->u.qcn9625_compact_v1.mpdu_start.info4,
			       RX_MPDU_INFO_INFO4_KEY_ID_OCTET_V1);
	crypto_hdr[3] = 0x20 | (key_id << 6);
	crypto_hdr[4] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE3_V1(mpdu_start->pn[0]);
	crypto_hdr[5] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE4_V1(mpdu_start->pn[0]);
	crypto_hdr[6] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1_V1(mpdu_start->pn[1]);
	crypto_hdr[7] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2_V1(mpdu_start->pn[1]);
}

void
ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625_v1(struct hal_rx_desc *desc,
						  struct ieee80211_hdr *hdr)
{
	hdr->frame_control = desc->u.qcn9625_compact_v1.mpdu_start.frame_ctrl;
	hdr->duration_id = desc->u.qcn9625_compact_v1.mpdu_start.duration;
	ether_addr_copy(hdr->addr1, desc->u.qcn9625_compact_v1.mpdu_start.addr1);
	ether_addr_copy(hdr->addr2, desc->u.qcn9625_compact_v1.mpdu_start.addr2);
	ether_addr_copy(hdr->addr3, desc->u.qcn9625_compact_v1.mpdu_start.addr3);
	if (__le32_to_cpu(desc->u.qcn9625_compact_v1.mpdu_start.info2) &
	    RX_MPDU_INFO_INFO2_MAC_ADDR_AD4_VALID_V1)
		ether_addr_copy(hdr->addr4, desc->u.qcn9625_compact_v1.mpdu_start.addr4);
	hdr->seq_ctrl = desc->u.qcn9625_compact_v1.mpdu_start.seq_ctrl;
}

void
ath12k_wifi8_hal_extract_rx_desc_data_qcn9625_v1(struct hal_rx_desc_data *rx_desc_data,
						 struct hal_rx_desc *rx_desc,
						 struct hal_rx_desc *ldesc)
{
	rx_desc_data->msdu_done =
		ath12k_wifi8_hal_rx_h_msdu_done_qcn9625_v1(ldesc);
	rx_desc_data->msdu_len =
		ath12k_wifi8_hal_rx_h_msdu_len_qcn9625_v1(ldesc);
	rx_desc_data->l3_pad_bytes =
		ath12k_wifi8_hal_rx_h_l3pad_qcn9625_v1(ldesc);
	rx_desc_data->is_first_msdu =
		ath12k_wifi8_hal_rx_h_first_msdu_qcn9625_v1(ldesc);
	rx_desc_data->is_last_msdu =
		ath12k_wifi8_hal_rx_h_last_msdu_qcn9625_v1(ldesc);
	rx_desc_data->freq =
		ath12k_wifi8_hal_rx_h_freq_qcn9625_v1(rx_desc);
	rx_desc_data->snr =
		ath12k_wifi8_hal_rx_h_snr_qcn9625_v1(rx_desc);
	rx_desc_data->pkt_type =
		ath12k_wifi8_hal_rx_h_pkt_type_qcn9625_v1(rx_desc);
	rx_desc_data->bw =
		ath12k_wifi8_hal_rx_h_rx_bw_qcn9625_v1(rx_desc);
	rx_desc_data->rate_mcs =
		ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625_v1(rx_desc);
	rx_desc_data->nss =
		hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625_v1(rx_desc));
	rx_desc_data->sgi =
		ath12k_wifi8_hal_rx_h_sgi_qcn9625_v1(rx_desc);
	rx_desc_data->is_mcbc =
		ath12k_wifi8_hal_rx_h_is_da_mcbc_qcn9625_v1(rx_desc);
	rx_desc_data->seq_no =
		ath12k_wifi8_hal_rx_h_seq_no_qcn9625_v1(rx_desc);
	rx_desc_data->err_bitmap =
		ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625_v1(rx_desc);
	rx_desc_data->is_decrypted =
		ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625_v1(rx_desc);
	rx_desc_data->decap =
		ath12k_wifi8_hal_rx_h_decap_type_qcn9625_v1(rx_desc);
	rx_desc_data->ip_csum_fail =
		ath12k_wifi8_hal_rx_h_ip_cksum_fail_qcn9625_v1(rx_desc);
	rx_desc_data->l4_csum_fail =
		ath12k_wifi8_hal_rx_h_l4_cksum_fail_qcn9625_v1(rx_desc);
	rx_desc_data->tid =
		ath12k_wifi8_hal_rx_h_tid_qcn9625_v1(rx_desc);
	rx_desc_data->mesh_ctrl_present =
		ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625_v1(rx_desc);
	rx_desc_data->seq_ctl_valid =
		ath12k_wifi8_hal_rx_h_seq_ctrl_valid_qcn9625_v1(rx_desc);
	rx_desc_data->fc_valid =
		ath12k_wifi8_hal_rx_h_fc_valid_qcn9625_v1(rx_desc);
	rx_desc_data->is_ip_valid =
		ath12k_wifi8_hal_rx_h_is_ip_valid_qcn9625_v1(rx_desc);
	rx_desc_data->is_from_ds =
		ath12k_wifi8_hal_rx_h_from_ds_qcn9625_v1(rx_desc);
	rx_desc_data->is_to_ds =
		ath12k_wifi8_hal_rx_h_to_ds_qcn9625_v1(rx_desc);
}

void ath12k_wifi8_hal_extract_rx_spd_data_qcn9625_v1(struct hal_rx_spd_data *rx_info,
						     struct hal_rx_desc *rx_desc)
{
	__le32 flow_idx_info = rx_desc->u.qcn9625_compact_v1.msdu_end.info8;

	rx_info->tlv_info.decap =
		ath12k_wifi8_hal_rx_h_decap_type_qcn9625_v1(rx_desc);
	rx_info->tlv_info.mesh_ctrl_present =
		ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625_v1(rx_desc);
	rx_info->tlv_info.freq =
		ath12k_wifi8_hal_rx_h_freq_qcn9625_v1(rx_desc);
	rx_info->tlv_info.pkt_type =
		ath12k_wifi8_hal_rx_h_pkt_type_qcn9625_v1(rx_desc);
	rx_info->tlv_info.bw =
		ath12k_wifi8_hal_rx_h_rx_bw_qcn9625_v1(rx_desc);
	rx_info->tlv_info.rate_mcs =
		ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625_v1(rx_desc);
	rx_info->tlv_info.nss =
		hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625_v1(rx_desc));
	rx_info->tlv_info.sgi =
		ath12k_wifi8_hal_rx_h_sgi_qcn9625_v1(rx_desc);
	rx_info->cce_metadata =
		__le16_to_cpu(rx_desc->u.qcn9625_compact_v1.msdu_end.cce_metadata);
	rx_info->cce_match =
		le16_get_bits(rx_desc->u.qcn9625_compact_v1.msdu_end.info8,
			      RX_MSDU_END_INFO8_CCE_MATCH_V1);

	rx_info->rx_mpdu_info.flow_idx_timeout =
		le32_get_bits(flow_idx_info, RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT_V1);
	rx_info->rx_mpdu_info.flow_idx_invalid =
		le32_get_bits(flow_idx_info, RX_MSDU_END_INFO8_FLOW_IDX_INVALID_V1);
	rx_info->rx_mpdu_info.flow_info.flow_metadata =
		le16_get_bits(rx_desc->u.qcn9625_compact_v1.msdu_end.fse_metadata,
			      ATH12K_DP_RX_FSE_FLOW_METADATA_MASK);
}

/* -----------------------------------------------------------------------
 * QCN9625 HW1.0 V1 ops table
 *
 * hal_qcn9625_v1_ops is initialized at runtime as a copy of hal_qcn9625_ops
 * (V2), with only the compact RX descriptor functions overridden with V1
 * variants.  All non-compact ops (CE, srng, REO, TCL, etc.) are inherited
 * from V2.
 * -----------------------------------------------------------------------
 */

struct hal_ops hal_qcn9625_v1_ops;

void ath12k_wifi8_hal_init_v1_ops(void)
{
	/* Start from V2 ops — all non-compact entries are identical */
	hal_qcn9625_v1_ops = hal_qcn9625_ops;

	/* Override only the compact RX descriptor ops with V1 variants */
	hal_qcn9625_v1_ops.rx_desc_set_msdu_len =
		ath12k_wifi8_hal_rxdesc_set_msdu_len_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_get_dot11_hdr =
		ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_get_mpdu_frame_ctl =
		ath12k_wifi8_hal_rxdesc_get_mpdu_frame_ctrl_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_get_crypto_header =
		ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_copy_end_tlv =
		ath12k_wifi8_hal_rx_desc_end_tlv_copy_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_get_msdu_src_link_id =
		ath12k_wifi8_hal_rx_get_msdu_src_link_qcn9625_v1;
	hal_qcn9625_v1_ops.extract_rx_desc_data =
		ath12k_wifi8_hal_extract_rx_desc_data_qcn9625_v1;
	hal_qcn9625_v1_ops.extract_rx_spd_data =
		ath12k_wifi8_hal_extract_rx_spd_data_qcn9625_v1;
	hal_qcn9625_v1_ops.rxdesc_get_mpdu_start_addr2 =
		ath12k_wifi8_hal_rxdesc_get_mpdu_start_addr2_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_h_is_decrypted =
		ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_h_peer_meta_data =
		ath12k_wifi8_hal_rx_h_peer_meta_data_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_desc_get_fse_info =
		ath12k_wifi8_hal_rx_desc_get_fse_info_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_get_fse_metadata =
		ath12k_wifi8_hal_rx_get_fse_metadata_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_get_cce_metadata =
		ath12k_wifi8_hal_rx_get_cce_metadata_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_get_cce_match_bit =
		ath12k_wifi8_hal_rx_get_cce_match_bit_qcn9625_v1;
	hal_qcn9625_v1_ops.rx_get_flow_params =
		ath12k_wifi8_hal_rx_get_flow_params_qcn9625_v1;
}
