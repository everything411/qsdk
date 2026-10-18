/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_RX_H
#define ATH12K_HAL_RX_H

#include "hal_desc.h"
#include "hal.h"

struct ath12k_dp;

struct hal_reo_status;

struct hal_rx_reo_dest_rel_info {
	u32 cookie;
	enum hal_reo_dest_rel_src_module err_rel_src;
	enum hal_reo_dest_ring_push_reason push_reason;
	enum hal_reo_dest_ring_buffer_type buffer_type;
	u32 err_code;
	bool first_msdu;
	bool last_msdu;
	bool continuation;
	void *rx_desc;
	bool hw_cc_done;
	__le32 peer_metadata;
};

#define HAL_RX_MPDU_INFO_PN_GET_BYTE1(__val) \
	le32_get_bits((__val), GENMASK(7, 0))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE2(__val) \
	le32_get_bits((__val), GENMASK(15, 8))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE3(__val) \
	le32_get_bits((__val), GENMASK(23, 16))

#define HAL_RX_MPDU_INFO_PN_GET_BYTE4(__val) \
	le32_get_bits((__val), GENMASK(31, 24))

struct hal_rx_rxpcu_classification_overview {
	u32 rsvd0;
} __packed;

struct rx_msdu_info {
	u32 msdu_flags;
	u16 msdu_len; /* 14 bits for length */
};

#define HAL_RX_NUM_MSDU_DESC 6
struct hal_rx_msdu_list {
	struct rx_msdu_info msdu_info[HAL_RX_NUM_MSDU_DESC];
	u64 paddr[HAL_RX_NUM_MSDU_DESC];
	u32 sw_cookie[HAL_RX_NUM_MSDU_DESC];
	u8 rbm[HAL_RX_NUM_MSDU_DESC];
};

#define REO_QUEUE_EXT_DESC_MAX 5
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_0 0xDDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_1 0xADBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_2 0xBDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_3 0xCDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_4 0xEDBEEF
#define REO_QUEUE_DESC_MAGIC_DEBUG_PATTERN_5 0xFDBEEF

/* info1 subfields */
#define HAL_RX_FSE_SRC_PORT			GENMASK(15, 0)
#define HAL_RX_FSE_DEST_PORT			GENMASK(31, 16)

/* info2 subfields */
#define HAL_RX_FSE_L4_PROTOCOL			GENMASK(7, 0)
#define HAL_RX_FSE_VALID			GENMASK(8, 8)
#define HAL_RX_FSE_RESERVED			GENMASK(13, 9)
#define HAL_RX_FSE_SERVICE_CODE			GENMASK(22, 14)
#define HAL_RX_FSE_PRIORITY_VLD			GENMASK(23, 23)
#define HAL_RX_FSE_REO_INDICATION		GENMASK(28, 24)
#define HAL_RX_FSE_MSDU_DROP			GENMASK(29, 29)
#define HAL_RX_FSE_REO_DESTINATION_HANDLER	GENMASK(31, 30)

/* info 3 subfields */

/* info4 subfields */
/*
 * Mapping provided fields to info4:
 *  - dest_info                 : [11:0]
 *  - dest_info_valid           : [12]
 *  - int_priority              : [16:13]
 *  - int_priority_valid        : [17]
 *  - ppe_classify_read_hint    : [19:18]
 *  - c_tdma_lut_ptr            : [25:20]
 *  - ll_pkt                    : [26]
 *  - rx_sdwf_policer_id        : [31:27]
 */
#define HAL_RX_FSE_DEST_INFO			GENMASK(11, 0)
#define HAL_RX_FSE_DEST_INFO_VALID		GENMASK(12, 12)
#define HAL_RX_FSE_INT_PRIORITY		GENMASK(16, 13)
#define HAL_RX_FSE_INT_PRIORITY_VALID		GENMASK(17, 17)
#define HAL_RX_FSE_PPE_CLASSIFY_READ_HINT	GENMASK(19, 18)
#define HAL_RX_FSE_C_TDMA_LUT_PTR		GENMASK(25, 20)
#define HAL_RX_FSE_LL_PKT			GENMASK(26, 26)
#define HAL_RX_FSE_RX_SDWF_POLICER_ID		GENMASK(31, 27)

/* info5 subfields */
/*
 * Mapping provided fields to info5:
 *  - telemetry_stream_id_valid  : [0]
 *  - telemetry_stream_id        : [8:1]
 *  - sw_peer_id_check_enable    : [9]
 *  - sw_peer_id                 : [25:10]
 *  - rx_sdwf_priority           : [26]
 *  - reserved_15                : [31:27]
 */
#define HAL_RX_FSE_TELEMETRY_STREAM_ID_VALID	GENMASK(0, 0)
#define HAL_RX_FSE_TELEMETRY_STREAM_ID	GENMASK(8, 1)
#define HAL_RX_FSE_SW_PEER_ID_CHECK_ENABLE	GENMASK(9, 9)
#define HAL_RX_FSE_SW_PEER_ID			GENMASK(25, 10)
#define HAL_RX_FSE_RX_SDWF_PRIORITY		GENMASK(26, 26)
#define HAL_RX_FSE_RESERVED_15			GENMASK(31, 27)

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
	u32 info5;
};

#define HAL_FST_HASH_DATA_SIZE		37
#define HAL_FST_HASH_KEY_SIZE_WORDS	10
#define HAL_RX_FST_MAX_SEARCH		16
#define HAL_RX_FLOW_SEARCH_TABLE_SIZE	8192
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
	u8 drop:1;

	u32 timestamp;

	/* info4 fields */
	u32 dest_info                                               : 12, // [11:0]
	    dest_info_valid                                         :  1, // [12:12]
	    int_priority                                            :  4, // [16:13]
	    int_priority_valid                                      :  1, // [17:17]
	    ppe_classify_read_hint                                  :  2, // [19:18]
	    c_tdma_lut_ptr                                          :  6, // [25:20]
	    ll_pkt                                                  :  1, // [26:26]
	    rx_sdwf_policer_id                                      :  5; // [31:27]

	/* info5 fields */
	u32 telemetry_stream_id_valid                               :  1, // [0:0]
	    telemetry_stream_id                                     :  8, // [8:1]
	    sw_peer_id_check_enable                                 :  1, // [9:9]
	    sw_peer_id                                              : 16, // [25:10]
	    rx_sdwf_priority                                        :  6; // [31:26]
};

struct ath12k_hal_rx_cmd_ring_param {
	u32 mac_addr_31_0;
	u32 mac_addr_47_32:16,
	    is_mcast:1,
	    is_mec:1,
	    ad1_match:1,
	    link_id:3,
	    reserved_0:10;
	u32 meta_data_0;
	u8 cmd_num:4,
	   reserved:4;
	u8 hw_link_bitmap;
};

struct hal_mon_tx_u_sig_uhr_common_info {
	u8 phy_version;
	u8 tx_bw;
	u8 ul_dl;
	u8 bss_color;
	u8 txop;
};

struct hal_mon_tx_u_sig_uhr_common_enc_info {
	u32 phy_version;
	u32 tx_bw;
	u32 ul_dl;
	u32 bss_color;
	u32 txop;
};

struct hal_mon_tx_u_sig_uhr_mu_info {
	struct hal_mon_tx_u_sig_uhr_common_info common;
	u8 ppdu_type;
	u8 validate_or_cobf_cosr;
	u8 punc_ch_info;
	u8 mcs;
	u8 num_uhr_sig_symbols;
	u8 crc;
	u8 tail;
	u8 rx_integ_check;
	u8 shared_ap_bss_color;
};

struct hal_mon_tx_u_sig_uhr_mu_enc_info {
	u32 ppdu_type;
	u32 validate_or_cobf_cosr;
	u32 punc_ch_info;
	u32 mcs;
	u32 num_uhr_sig_symbols;
	u32 crc;
	u32 tail;
	u32 bad_crc;
	u32 shared_ap_bss_color;
};

struct hal_mon_tx_u_sig_uhr_tb_info {
	struct hal_mon_tx_u_sig_uhr_common_info common;
	u8 ppdu_type;
	u8 spatial_reuse;
	u8 crc;
	u8 tail;
	u8 rx_integ_check;
};

struct hal_mon_tx_u_sig_uhr_tb_enc_info {
	u32 ppdu_type;
	u32 spatial_reuse_1;
	u32 spatial_reuse_2;
	u32 crc;
	u32 tail;
	u32 bad_crc;
};

struct hal_mon_tx_u_sig_uhr_elr_info {
	struct hal_mon_tx_u_sig_uhr_common_info common;
	u8 ppdu_type;
	u8 validate;
	u8 crc;
	u8 tail;
	u16 sta_id;
};

struct hal_mon_tx_u_sig_uhr_elr_enc_info {
	u32 ppdu_type;
	u32 sta_id;
	u32 validate;
	u32 crc;
	u32 tail;
};

int
ath12k_wifi8_hal_invalidate_rx_cache_cmd_send(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      struct ath12k_hal_rx_cmd_ring_param *param);
void ath12k_wifi8_hal_reo_status_queue_stats(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi8_hal_reo_status_queue_1k_stats(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status);
int
ath12k_wifi8_hal_reo_highest_sn_from_bitmap(u16 ssn,
					    const u32 *bitmap_words,
					    int num_words,
					    u16 *high_off);
void ath12k_wifi8_hal_reo_flush_queue_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi8_hal_reo_flush_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void ath12k_wifi8_hal_reo_unblk_cache_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_reo_status *status);
void
ath12k_wifi8_hal_reo_flush_timeout_list_status(struct ath12k_base *ab,
					       struct hal_tlv_64_hdr *tlv,
					       struct hal_reo_status *status);
void
ath12k_wifi8_hal_reo_desc_thresh_reached_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi8_hal_reo_update_rx_reo_queue_status(struct ath12k_base *ab,
						struct hal_tlv_64_hdr *tlv,
						struct hal_reo_status *status);
void
ath12k_wifi8_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action);
int ath12k_wifi8_hal_reo_rel_parse_err(struct ath12k_dp *dp, void *desc,
				       struct hal_rx_reo_dest_rel_info *rel_info);
void ath12k_wifi8_hal_rx_reo_ent_paddr_get(struct ath12k_buffer_addr *buff_addr,
					   dma_addr_t *paddr, u32 *cookie);
void ath12k_wifi8_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_wifi8_hal_reo_init_cmd_ring_offset(struct ath12k_base *ab,
					       struct hal_srng *srng,
					       u16 cmd_num);
int ath12k_wifi8_hal_fse_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  struct hal_fse_cmd *fse_cmd);
void ath12k_wifi8_hal_reo_hw_setup(struct ath12k_base *ab);
#ifdef CPTCFG_EXT_IPA_OFFLOAD
void ath12k_wifi8_hal_reo_ring_ipa_ctrl_hash_ix0_setup(struct ath12k_base *ab);
#endif
void ath12k_wifi8_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type,
				      u16 stats_id);
u32 ath12k_wifi8_hal_rx_get_trunc_hash(struct hal_rx_fst *fst, u32 hash);
u32 ath12k_wifi8_hal_flow_toeplitz_hash(struct ath12k_base *ab,
					struct hal_rx_fst *fst,
					struct hal_flow_tuple_info *tuple_info);
int ath12k_wifi8_hal_rx_find_flow_from_tuple(struct ath12k_base *ab,
					     struct hal_rx_fst *fst,
					     u32 flow_hash,
					     void *flow_tuple_info,
					     u32 *flow_idx);
ssize_t ath12k_wifi8_hal_rx_dump_fst_table(struct ath12k_base *ab,
					   struct hal_rx_fst *fst,
					   char *buf, int size);
struct hal_rx_fst *ath12k_wifi8_hal_rx_fst_attach(struct ath12k_base *ab);
void ath12k_wifi8_hal_rx_fst_detach(struct ath12k_base *ab, struct hal_rx_fst *fst);
int ath12k_wifi8_hal_rx_flow_insert_entry(struct ath12k_base *ab,
					  struct hal_rx_fst *fst,
					  u32 flow_hash,
					  void *flow_tuple_info,
					  u32 *flow_idx);
void *ath12k_wifi8_hal_rx_flow_setup_fse(struct ath12k_base *ab,
					 struct hal_rx_fst *fst,
					 u32 table_offset,
					 struct hal_rx_flow *flow);
void ath12k_wifi8_hal_rx_flow_delete_entry(struct ath12k_base *ab,
					   struct hal_rx_fse *hal_fse);
void ath12k_wifi8_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid);
void ath12k_wifi8_hal_reo_shared_qaddr_cache_clear(struct ath12k_base *ab);
void ath12k_hal_qcn9625_get_tsf2_scratch_reg(struct ath12k_base *ab,
					     u8 mac_id, u64 *value);
void ath12k_hal_qcn9625_get_tqm_scratch_reg(struct ath12k_base *ab, u64 *value);
void ath12k_wifi8_hal_rx_msdu_list_get(void *desc,
				       void *list,
				       u16 *num_msdus);
void ath12k_wifi8_hal_rx_reo_ent_buf_paddr_get(void *rx_desc, dma_addr_t *paddr,
					       u32 *sw_cookie,
					       struct ath12k_buffer_addr **pp_buf_addr,
					       u8 *rbm, u32 *msdu_cnt);
#endif
