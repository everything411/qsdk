/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_RX_H
#define ATH12K_HAL_RX_H

#include "hal_desc.h"
#include "hal.h"
#include "hal_mon_qcn9074.h"

#define HAL_INVALID_PEERID	0x3fff

#define HAL_RX_MPDU_INFO_PN_GET_BYTE1(__val) \
	le32_get_bits((__val), GENMASK(7, 0))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE2(__val) \
	le32_get_bits((__val), GENMASK(15, 8))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE3(__val) \
	le32_get_bits((__val), GENMASK(23, 16))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE4(__val) \
	le32_get_bits((__val), GENMASK(31, 24))

struct hal_rx_msdu_desc_info {
	u32 msdu_flags;
	u16 msdu_len; /* 14 bits for length */
};

#define HAL_RX_NUM_MSDU_DESC 6
struct hal_rx_msdu_list {
	struct hal_rx_msdu_desc_info msdu_info[HAL_RX_NUM_MSDU_DESC];
	u64 paddr[HAL_RX_NUM_MSDU_DESC];
	u32 sw_cookie[HAL_RX_NUM_MSDU_DESC];
	u8 rbm[HAL_RX_NUM_MSDU_DESC];
};

struct hal_rx_buf_info {
	uint64_t paddr;
	uint32_t sw_cookie;
	uint8_t rbm;
};

/* info1 subfields */
#define HAL_RX_FSE_SRC_PORT			GENMASK(15, 0)
#define HAL_RX_FSE_DEST_PORT			GENMASK(31, 16)

/* info2 subfields */
#define HAL_RX_FSE_L4_PROTOCOL			GENMASK(7, 0)
#define HAL_RX_FSE_VALID			GENMASK(8, 8)
#define HAL_RX_FSE_RESERVED			GENMASK(12, 9)
#define HAL_RX_FSE_SERVICE_CODE			GENMASK(21, 13)
#define HAL_RX_FSE_PRIORITY_VLD			GENMASK(22, 22)
#define HAL_RX_FSE_USE_PPE			GENMASK(23, 23)
#define HAL_RX_FSE_REO_INDICATION		GENMASK(28, 24)
#define HAL_RX_FSE_MSDU_DROP			GENMASK(29, 29)
#define HAL_RX_FSE_REO_DESTINATION_HANDLER	GENMASK(31, 30)

/* info 3 subfields */
#define HAL_RX_FSE_AGGREGATION_COUNT		GENMASK(15, 0)
#define HAL_RX_FSE_LRO_ELIGIBLE			GENMASK(31, 16)
#define HAL_RX_FSE_MSDU_COUNT			GENMASK(31, 16)

/* info4 subfields */
#define HAL_RX_FSE_CUMULATIVE_IP_LEN1		GENMASK(15, 0)
#define HAL_RX_FSE_CUMULATIVE_IP_LEN		GENMASK(31, 16)

/* This structure should not be modified as it is shared with HW */
struct hal_rx_fse {
	u32 src_ip_127_96;
	u32 src_ip_95_64;
	u32 src_ip_63_32;
	u32 src_ip_31_0;
	u32 dest_ip_127_96;
	u32 dest_ip_95_64;
	u32 dest_ip_63_32;
	u32 dest_ip_31_0;
	u32 info1;
	u32 info2;
	u32 metadata;
	u32 info3;
	u32 msdu_byte_count;
	u32 timestamp;
	u32 info4;
	u32 tcp_sequence_number;
};

#define HAL_FST_HASH_DATA_SIZE		37
#define HAL_FST_HASH_KEY_SIZE_WORDS	10
#define HAL_RX_FST_MAX_SEARCH		16
#define HAL_RX_FLOW_SEARCH_TABLE_SIZE	2048
#define HAL_RX_FST_TOEPLITZ_KEYLEN	40
#define NUM_OF_DWORDS_RX_FLOW_SEARCH_ENTRY	16
#define HAL_RX_FST_ENTRY_SIZE		(NUM_OF_DWORDS_RX_FLOW_SEARCH_ENTRY * 4)
#define HAL_RX_FSE_REO_DEST_FT 0

struct hal_rx_flow {
	struct hal_flow_tuple_info tuple_info;
	u32 fse_metadata;
	u16 service_code;
	u8 reo_destination_handler;
	u8 reo_indication;
	u8 use_ppe      :1,
	   drop         :1;
};

struct hal_rx_rxpcu_classification_overview {
	u32 rsvd0;
} __packed;

/* RXPCU_PPDU_END_INFO word 2 - phyrx_abort_request_info_valid at bit [28] */
#define RXPCU_PPDU_END_INFO_2_PHYRX_ABORT_VALID_OFFSET		0x00000008
#define RXPCU_PPDU_END_INFO_2_PHYRX_ABORT_VALID			GENMASK(28, 28)

/* RXPCU_PPDU_END_INFO word 10 - phyrx_abort_request_info_details at byte offset 0x28 */
#define RXPCU_PPDU_END_INFO_10_PHYRX_ABORT_DETAILS_OFFSET	0x00000028

/* phyrx_abort_request_info field macros */
#define PHYRX_ABORT_REQUEST_INFO_REASON				GENMASK(7, 0)

static inline void
ath12k_wifi6_hal_rx_get_phyrx_abort(const void *tlv_data,
				    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_wifi6_phyrx_abort_info *abort_info =
		ppdu_info->ppdu_info_extn.phyrx_abort_info;

	if (!abort_info)
		return;

	if (le32_get_bits(*(__le32 *)((u8 *)tlv_data +
				      RXPCU_PPDU_END_INFO_2_PHYRX_ABORT_VALID_OFFSET),
			  RXPCU_PPDU_END_INFO_2_PHYRX_ABORT_VALID)) {
		const u8 *phyrx_abort_tlv = (const u8 *)tlv_data +
			RXPCU_PPDU_END_INFO_10_PHYRX_ABORT_DETAILS_OFFSET;
		abort_info->phyrx_abort = 1;
		abort_info->phyrx_abort_reason =
			le32_get_bits(*(__le32 *)phyrx_abort_tlv,
				      PHYRX_ABORT_REQUEST_INFO_REASON);
	}
}

/* L_SIG_A word 0 - length and parity (802.11 standard bit positions) */
#define L_SIG_A_INFO_0_LENGTH			GENMASK(16, 5)
#define L_SIG_A_INFO_0_PARITY			GENMASK(17, 17)

/* VHT_SIG_A word 0 - TXOP_PS_NOT_ALLOWED at bit [22] */
#define VHT_SIG_A_INFO_0_TXOP_PS_NOT_ALLOWED	GENMASK(22, 22)
/* VHT_SIG_A word 1 - CRC at bits [17:10] */
#define VHT_SIG_A_INFO_1_CRC			GENMASK(17, 10)

/* HE_SIG_A_SU word 1 - CRC at bits [19:16] */
#define HE_SIG_A_SU_INFO_1_CRC			GENMASK(19, 16)

/* HE_SIG_A_MU_DL word 1 - CRC at bits [19:16] */
#define HE_SIG_A_MU_DL_INFO_1_CRC		GENMASK(19, 16)

int ath12k_wifi6_hal_reo_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_reo_cmd_type type,
				  struct ath12k_hal_reo_cmd *cmd);
void ath12k_wifi6_hal_reo_status_queue_stats(struct ath12k_base *ab,
					     struct hal_tlv_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi6_hal_reo_flush_queue_status(struct ath12k_base *ab,
					     struct hal_tlv_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi6_hal_reo_flush_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi6_hal_reo_unblk_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_hdr *tlv,
					     struct hal_reo_status *status);
void
ath12k_wifi6_hal_reo_flush_timeout_list_status(struct ath12k_base *ab,
					       struct hal_tlv_hdr *tlv,
					       struct hal_reo_status *status);
void
ath12k_wifi6_hal_reo_desc_thresh_reached_status(struct ath12k_base *ab,
						struct hal_tlv_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi6_hal_reo_update_rx_reo_queue_status(struct ath12k_base *ab,
						struct hal_tlv_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi6_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_rx_buf_return_buf_manager *rbm);
void
ath12k_wifi6_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action);
int ath12k_wifi6_hal_desc_reo_parse_err(struct ath12k_dp *dp,
					struct hal_reo_dest_ring *desc,
					dma_addr_t *paddr, u32 *desc_bank);
void ath12k_wifi6_hal_rx_reo_ent_paddr_get(struct ath12k_base *ab,
					   struct ath12k_buffer_addr *buff_addr,
					   dma_addr_t *paddr, u32 *cookie);
void ath12k_wifi6_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_wifi6_hal_reo_hw_setup(struct ath12k_base *ab);
void ath12k_wifi6_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type);
u32 ath12k_wifi6_hal_rx_get_trunc_hash(struct hal_rx_fst *fst, u32 hash);
u32 ath12k_wifi6_hal_flow_toeplitz_hash(struct ath12k_base *ab,
					struct hal_rx_fst *fst,
					struct hal_flow_tuple_info *tuple_info);
int ath12k_wifi6_hal_rx_find_flow_from_tuple(struct ath12k_base *ab,
					     struct hal_rx_fst *fst,
					     u32 flow_hash,
					     void *flow_tuple_info,
					     u32 *flow_idx);
ssize_t ath12k_wifi6_hal_rx_dump_fst_table(struct ath12k_base *ab,
					   struct hal_rx_fst *fst,
					   char *buf, int size);
struct hal_rx_fst *ath12k_wifi6_hal_rx_fst_attach(struct ath12k_base *ab);
void ath12k_wifi6_hal_rx_fst_detach(struct ath12k_base *ab, struct hal_rx_fst *fst);
int ath12k_wifi6_hal_rx_flow_insert_entry(struct ath12k_base *ab,
					  struct hal_rx_fst *fst,
					  u32 flow_hash,
					  void *flow_tuple_info,
					  u32 *flow_idx);
void *ath12k_wifi6_hal_rx_flow_setup_fse(struct ath12k_base *ab,
					 struct hal_rx_fst *fst,
					 u32 table_offset,
					 struct hal_rx_flow *flow);
void ath12k_wifi6_hal_rx_flow_delete_entry(struct ath12k_base *ab,
					   struct hal_rx_fse *hal_fse);
void ath12k_wifi6_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid);
void ath12k_wifi6_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
void ath12k_wifi6_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
void ath12k_hal_qcn9274_get_tsf2_scratch_reg(struct ath12k_base *ab,
					     u8 mac_id, u64 *value);
void ath12k_hal_qcn9274_get_tqm_scratch_reg(struct ath12k_base *ab, u64 *value);
void ath12k_wifi6_hal_rx_msdu_list_get(void *desc, void *list, u16 *num_msdus);
void ath12k_wifi6_hal_rx_reo_ent_buf_paddr_get(void *rx_desc,
					       struct hal_rx_buf_info *buf_info,
					       u32 *msdu_cnt);
void ath12k_wifi6_dp_mon_rx_priv_info_set(u8 *buf, u8 *priv_data, u8 len);
void ath12k_wifi6_dp_mon_rx_priv_info_get(u8 *buf, u8 *priv_data, u8 len);
void ath12k_wifi6_hal_rx_buf_addr_info_set(struct ath12k_buffer_addr *b_info,
					   dma_addr_t paddr, u32 cookie,
					   u8 manager);
void ath12k_wifi6_hal_rx_buf_addr_info_get(struct ath12k_buffer_addr *b_info,
					   dma_addr_t *paddr, u32 *cookie,
					   u8 *rbm);
#endif
