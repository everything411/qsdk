/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include "../hal.h"
#include "hal_tx.h"
#include "hal_rx.h"

#define HAL_REG_WRITER_VALUE_BASE_ADDR 0xF1E000
#define HAL_REG_WRITER_VALUE_OFFSET 0x3000

/**
 * Enumeration for Reg Write block index selection
 * Each Enum position corresponds to respective
 * UMAC_UMCMN_R0_REG_ADDR_LSB_IX_n
 * UMAC_UMCMN_R0_REG_ADDR_MSB_IX_n
 * UMAC_UMCMN_R2_REG_VALUE_IX_n
 *
 * Any value written to the value register will be copied by
 * Reg Writer Block to Address written in Reg Address Register.
 *
 * NOTE: Only value Registers defined in the enum should be used by the Host.
 * Rest are unconfigured, and if used may result in invalid memory access
 **/
enum hal_reg_write_selection {
	HAL_REG_WRITER_RXMON_SW2MON_BUF_RING = 0,
	HAL_REG_WRITER_TXMON_SW2MON_BUF_RING = 0x4,
	HAL_REG_WRITER_RXMON_M0_MON2SW_DEST_RING = 0x8,
	HAL_REG_WRITER_RXMON_M1_MON2SW_DEST_RING = 0xc,
	HAL_REG_WRITER_TXMON_M0_MON2SW_DEST_RING = 0x10,
	HAL_REG_WRITER_TXMON_M1_MON2SW_DEST_RING = 0x14,
	HAL_REG_WRITER_RXOLE2SW_ASE_DEST_RING = 0x18,
};

extern const struct hal_ops hal_qcn9625_ops;

/*
 * Generic QCN9625 monitor ops (shared entries only), defined in
 * hal_mon_qcn9625.c. Each revision's mon-ops-init helper copies this and
 * overrides the version-specific entries.
 */
extern const struct hal_mon_ops hal_qcn9625_mon_ops_base;

/* Per-revision monitor ops init: copy the base, override version-specific
 * entries, and point hal->hal_mon_ops at the result.
 */
void ath12k_wifi8_hal_qcn9625_hw20_mon_ops_init(struct ath12k_hal *hal);
void ath12k_wifi8_hal_qcn9625_hw10_mon_ops_init(struct ath12k_hal *hal);

extern struct hal_ops hal_qcn9625_v1_ops;
void ath12k_wifi8_hal_init_v1_ops(void);
u32 ath12k_wifi8_hal_get_rx_desc_size_qcn9625_v1(void);
u16 ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625_v1(void);
u32 ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625_v1(void);
u32 ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625_v1(void);
u32 ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625_v1(void);

/* HAL init/deinit and core config — shared between HW1.0 and HW2.0 ops tables */
int ath12k_wifi8_hal_init_qcn9625(struct ath12k_hal *hal, u8 hw_version);
void ath12k_wifi8_hal_deinit_qcn9625(struct ath12k_hal *hal);
int ath12k_wifi8_hal_srng_create_config_qcn9625(struct ath12k_hal *hal);
void ath12k_wifi8_hal_set_reg_writer_hptp_addr(struct ath12k_base *ab,
					       struct hal_srng *srng,
					       int mac_id,
					       int idx,
					       enum hal_ring_type ring_type);

u32 ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625(struct hal_rx_desc *desc);
void
ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625(struct hal_rx_desc *desc,
						   u8 *crypto_hdr,
						   enum hal_encrypt_type encyp);
void
ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625(struct hal_rx_desc *desc,
					       struct ieee80211_hdr *hdr);
void ath12k_wifi8_hal_extract_rx_desc_data_qcn9625(struct hal_rx_desc_data *rx_desc_data,
						   struct hal_rx_desc *rx_desc,
						   struct hal_rx_desc *ldesc);
void ath12k_wifi8_hal_extract_rx_spd_data_qcn9625(struct hal_rx_spd_data *rx_info,
						  struct hal_rx_desc *rx_desc);
static inline
bool ath12k_wifi8_hal_rx_h_first_msdu_qcn9625(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			       RX_MSDU_END_INFO5_FIRST_MSDU);
}

static inline
bool ath12k_wifi8_hal_rx_h_last_msdu_qcn9625(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			       RX_MSDU_END_INFO5_LAST_MSDU);
}

static inline
u8 ath12k_wifi8_hal_rx_h_l3pad_qcn9625(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_L3_HEADER_PADDING);
}

static inline
bool ath12k_wifi8_hal_encrypt_valid_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_FRAME_ENCRYPTION_INFO_VALID);
}

static inline
u8 ath12k_wifi8_hal_rx_h_decap_type_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_DECAP_FORMAT);
}

static inline
u8 ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info12,
			     RX_MSDU_END_INFO12_MESH_CONTROL_PRESENT);
}

static inline
bool ath12k_wifi8_hal_rx_h_seq_ctrl_valid_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_CONTROL_VALID);
}

static inline
bool ath12k_wifi8_hal_rx_h_fc_valid_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info2,
			       RX_MPDU_INFO_INFO2_MPDU_FRAME_CONTROL_VALID);
}

static inline
bool ath12k_wifi8_hal_rx_h_is_ip_valid_qcn9625(struct hal_rx_desc *desc)
{
	return !!(le32_get_bits(desc->u.qcn9625_compact.msdu_end.info12,
				RX_MSDU_END_INFO12_IPV4_PROTO) ||
		  le32_get_bits(desc->u.qcn9625_compact.msdu_end.info12,
				RX_MSDU_END_INFO12_IPV6_PROTO));
}

static inline
u16 ath12k_wifi8_hal_rx_h_seq_no_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info2,
			     RX_MPDU_INFO_INFO2_MPDU_SEQUENCE_NUMBER);
}

static inline
u16 ath12k_wifi8_hal_rx_h_msdu_len_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info11,
			     RX_MSDU_END_INFO11_MSDU_LENGTH);
}

static inline
u8 ath12k_wifi8_hal_rx_h_sgi_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_SGI);
}

static inline
u8 ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_RATE_MCS);
}

static inline
u8 ath12k_wifi8_hal_rx_h_rx_bw_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_RECEIVE_BANDWIDTH);
}

static inline
u32 ath12k_wifi8_hal_rx_h_freq_qcn9625(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact.msdu_end.phy_meta_data);
}

static inline
u8 ath12k_wifi8_hal_rx_h_snr_qcn9625(struct hal_rx_desc *desc)
{
	/* the field contains SNR value */
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_USER_RSSI);
}

static inline
u8 ath12k_wifi8_hal_rx_h_pkt_type_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_PKT_TYPE);
}

static inline
u8 ath12k_wifi8_hal_rx_h_nss_qcn9625(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9625_compact.msdu_end.info13,
			     RX_MSDU_END_INFO13_MIMO_SS_BITMAP);
}

static inline
u8 ath12k_wifi8_hal_rx_h_tid_qcn9625(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_TID);
}

static inline
u8 ath12k_wifi8_hal_rx_h_from_ds_qcn9625(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_FR_DS);
}

static inline
u8 ath12k_wifi8_hal_rx_h_to_ds_qcn9625(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact.msdu_end.info5,
			     RX_MSDU_END_INFO5_TO_DS);
}

static inline
void ath12k_wifi8_hal_rx_desc_end_tlv_copy_qcn9625(struct hal_rx_desc *fdesc,
						   struct hal_rx_desc *ldesc)
{
	fdesc->u.qcn9625_compact.msdu_end = ldesc->u.qcn9625_compact.msdu_end;
}

static inline
u32 ath12k_wifi8_hal_rx_h_peer_meta_data_qcn9625(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact.mpdu_start.peer_meta_data);
}

static inline void
ath12k_wifi8_hal_rxdesc_set_msdu_len_qcn9625(struct hal_rx_desc *desc, u16 len)
{
	u32 info = __le32_to_cpu(desc->u.qcn9625_compact.msdu_end.info11);

	info = u32_replace_bits(info, len, RX_MSDU_END_INFO11_MSDU_LENGTH);
	desc->u.qcn9625_compact.msdu_end.info11 = __cpu_to_le32(info);
}

static inline
u8 *ath12k_wifi8_hal_rx_desc_get_msdu_payload_qcn9625(struct hal_rx_desc *desc)
{
	return &desc->u.qcn9625_compact.msdu_payload[0];
}

static inline
u32 ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625(void)
{
	return offsetof(struct hal_rx_desc_qcn9625_compact, mpdu_start);
}

static inline
u32 ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625(void)
{
	return offsetof(struct hal_rx_desc_qcn9625_compact, msdu_end);
}

static inline
bool ath12k_wifi8_hal_rxdesc_mac_addr2_valid_qcn9625(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact.mpdu_start.info2) &
			     RX_MPDU_INFO_INFO2_MAC_ADDR_AD2_VALID;
}

static inline u8 *
ath12k_wifi8_hal_rxdesc_get_mpdu_start_addr2_qcn9625(struct hal_rx_desc *desc)
{
	return ath12k_wifi8_hal_rxdesc_mac_addr2_valid_qcn9625(desc) ?
			desc->u.qcn9625_compact.mpdu_start.addr2 : NULL;
}

static inline
bool ath12k_wifi8_hal_rx_h_is_da_mcbc_qcn9625(struct hal_rx_desc *desc)
{
	return (ath12k_wifi8_hal_rx_h_first_msdu_qcn9625(desc) &&
		(__le16_to_cpu(desc->u.qcn9625_compact.msdu_end.info5) &
	       RX_MSDU_END_INFO5_DA_IS_MCBC));
}

static inline u16
ath12k_wifi8_hal_rxdesc_get_mpdu_frame_ctrl_qcn9625(struct hal_rx_desc *desc)
{
	return __le16_to_cpu(desc->u.qcn9625_compact.mpdu_start.frame_ctrl);
}

static inline
bool ath12k_wifi8_hal_rx_h_msdu_done_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.msdu_end.info15,
			       RX_MSDU_END_INFO15_MSDU_DONE);
}

static inline
bool ath12k_wifi8_hal_rx_h_l4_cksum_fail_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.msdu_end.info14,
			       RX_MSDU_END_INFO14_TCP_UDP_CHKSUM_FAIL);
}

static inline
bool ath12k_wifi8_hal_rx_h_ip_cksum_fail_qcn9625(struct hal_rx_desc *desc)
{
	return !!le32_get_bits(desc->u.qcn9625_compact.msdu_end.info14,
			       RX_MSDU_END_INFO14_IP_CHKSUM_FAIL);
}

static inline
bool ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625(struct hal_rx_desc *desc)
{
	return (le32_get_bits(desc->u.qcn9625_compact.msdu_end.info15,
			      RX_MSDU_END_INFO15_DECRYPT_STATUS_CODE) ==
			RX_DESC_DECRYPT_STATUS_CODE_OK);
}

static inline u32 ath12k_wifi8_hal_get_rx_desc_size_qcn9625(void)
{
	return sizeof(struct hal_rx_desc_qcn9625_compact);
}

static inline
u8 ath12k_wifi8_hal_rx_get_msdu_src_link_qcn9625(struct hal_rx_desc *desc)
{
	return le64_get_bits(desc->u.qcn9625_compact.msdu_end.msdu_end_tag,
			     RX_MSDU_END_64_TLV_SRC_LINK_ID);
}

static inline u16 ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625(void)
{
	return QCN9625_MPDU_START_WMASK;
}

static inline u32 ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625(void)
{
	return QCN9625_MSDU_END_WMASK;
}

static inline
void ath12k_wifi8_hal_rx_desc_get_fse_info_qcn9625(struct hal_rx_desc *desc,
						   struct rx_mpdu_desc_info
						   *rx_mpdu_info)
{
	__le32 flow_idx_info = desc->u.qcn9625_compact.msdu_end.info8;

	rx_mpdu_info->flow_idx_timeout =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT);
	rx_mpdu_info->flow_idx_invalid =
		le32_get_bits(flow_idx_info,
			      RX_MSDU_END_INFO8_FLOW_IDX_INVALID);

	rx_mpdu_info->flow_info.flow_metadata =
		le16_get_bits(desc->u.qcn9625_compact.msdu_end.fse_metadata,
			      ATH12K_DP_RX_FSE_FLOW_METADATA_MASK);
}

static inline
u32 ath12k_wifi8_hal_rx_get_fse_metadata_qcn9625(struct hal_rx_desc *desc)
{
	return __le32_to_cpu(desc->u.qcn9625_compact.msdu_end.fse_metadata);
}

static inline
u16 ath12k_wifi8_hal_rx_get_cce_metadata_qcn9625(struct hal_rx_desc *desc)
{
	/* rsvd_0 lower 16 bits = cce_metadata, upper 16 bits = tcp_udp_chksum */
	return __le16_to_cpu(desc->u.qcn9625_compact.msdu_end.cce_metadata);
}

static inline
bool ath12k_wifi8_hal_rx_get_cce_match_bit_qcn9625(struct hal_rx_desc *desc)
{
	return le16_get_bits(desc->u.qcn9625_compact.msdu_end.info8,
			     RX_MSDU_END_INFO8_CCE_MATCH);
}

static inline
void ath12k_wifi8_hal_rx_get_flow_params_qcn9625(struct hal_rx_desc *desc,
						 u32 *flow_idx,
						 bool *flow_idx_invalid,
						 bool *flow_idx_timeout)
{
	__le32 info7 = desc->u.qcn9625_compact.msdu_end.info7;
	__le16 info8 = desc->u.qcn9625_compact.msdu_end.info8;

	*flow_idx = le32_get_bits(info7, RX_MSDU_END_INFO7_FLOW_IDX);
	*flow_idx_invalid = le16_get_bits(info8, RX_MSDU_END_INFO8_FLOW_IDX_INVALID);
	*flow_idx_timeout = le16_get_bits(info8, RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT);
}
