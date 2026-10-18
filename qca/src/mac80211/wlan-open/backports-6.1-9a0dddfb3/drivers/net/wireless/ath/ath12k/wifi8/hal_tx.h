/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_TX_H
#define ATH12K_HAL_TX_H

#include "hal_desc.h"

struct hal_rx_mon_ppdu_info;

/* Maximum payload across current TQM types (update if a larger one is added). */
#define HAL_TQM_VALUE_MAX_BYTES (sizeof(struct hal_tqm_sync_cmd))

#define HAL_TLV64_HDR_BYTES   (sizeof(struct hal_tlv_64_hdr))
#define HAL_TLV64_HDR_WORDS   (HAL_TLV64_HDR_BYTES >> 2)
#define HAL_TQM_CMD_MAX_BYTES (HAL_TLV64_HDR_BYTES + HAL_TQM_VALUE_MAX_BYTES)
#define HAL_TQM_CMD_MAX_WORDS (HAL_TQM_CMD_MAX_BYTES >> 2)

/* Maximum payload across current SAM command types (update if a larger one is added). */
#define HAL_SAM_VALUE_MAX_BYTES (sizeof(struct hal_sam_peer_clear_programming))

#define HAL_SAM_CMD_MAX_BYTES (HAL_TLV64_HDR_BYTES + HAL_SAM_VALUE_MAX_BYTES)
#define HAL_SAM_CMD_MAX_WORDS (HAL_SAM_CMD_MAX_BYTES >> 2)

#define HAL_TX_NUM_MAX_LINKS	5

/* TODO: check all these data can be managed with struct ath12k_tx_desc_info for perf */
struct hal_tx_info {
	u16 meta_data_flags; /* %HAL_TCL_DATA_CMD_INFO0_META_ */
	u8 ring_id;
	u8 rbm_id;
	u32 desc_id;
	enum hal_tcl_desc_type type;
	enum hal_tcl_encap_type encap_type;
	dma_addr_t paddr;
	u32 data_len;
	u32 pkt_offset;
	enum hal_encrypt_type encrypt_type;
	u32 flags0; /* %HAL_TCL_DATA_CMD_INFO1_ */
	u32 flags1; /* %HAL_TCL_DATA_CMD_INFO2_ */
	u16 addr_search_flags; /* %HAL_TCL_DATA_CMD_INFO0_ADDR(X/Y)_ */
	u16 bss_ast_hash;
	u16 bss_ast_idx;
	u8 tid;
	u8 search_type; /* %HAL_TX_ADDR_SEARCH_ */
	u8 link_id;
	u8 vdev_id;
	u8 dscp_tid_tbl_idx;
	bool enable_mesh;
	int bank_id;
	bool lookup_override;
	u8 tx_notify_frame;
};

extern u8 ath12k_default_dscp_tid_map[DSCP_TID_MAP_TBL_ENTRY_SIZE];
extern const u8 ath12k_default_pcp_tid_map[PCP_TID_MAP_TBL_SIZE];


/* TODO: Check if the actual desc macros can be used instead */
#define HAL_TX_STATUS_FLAGS_FIRST_MSDU		BIT(0)


#define HAL_TX_BANK_CONFIG_EPD			BIT(0)
#define HAL_TX_BANK_CONFIG_ENCAP_TYPE		GENMASK(2, 1)
#define HAL_TX_BANK_CONFIG_ENCRYPT_TYPE		GENMASK(6, 3)
#define HAL_TX_BANK_CONFIG_SRC_BUFFER_SWAP	BIT(7)
#define HAL_TX_BANK_CONFIG_LINK_META_SWAP	BIT(8)
#define HAL_TX_BANK_CONFIG_INDEX_LOOKUP_EN	BIT(9)
#define HAL_TX_BANK_CONFIG_ADDRX_EN		BIT(10)
#define HAL_TX_BANK_CONFIG_ADDRY_EN		BIT(11)
#define HAL_TX_BANK_CONFIG_MESH_EN		GENMASK(13, 12)
#define HAL_TX_BANK_CONFIG_VDEV_ID_CHECK_EN	BIT(14)
#define HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID	GENMASK(25, 18)

#define HAL_TX_WILD_CARD_LINK_ID       7

struct ath12k_hal_tx_cmd_ring_param {
	dma_addr_t ctrl_buf_addr;
	u32 meta_data_0;
	u8 cmd_num:4,
	   reserved:4;
};

int
ath12k_wifi8_hal_invalidate_tx_cache_cmd_send(struct ath12k_base *ab,
					      struct hal_srng *srng,
					      struct ath12k_hal_tx_cmd_ring_param *param);
void ath12k_wifi8_hal_tx_set_dscp_tid_map(struct ath12k_base *ab, u8 *map, int id);
void ath12k_wifi8_hal_tx_update_dscp_tid_map(struct ath12k_base *ab,
					     int id, u8 dscp, u8 tid);
void ath12k_wifi8_hal_tx_cmd_desc_setup(struct ath12k_base *ab,
					struct hal_tcl_data_cmd *tcl_cmd,
					struct hal_tx_info *ti);
int ath12k_wifi8_hal_reo_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_reo_cmd_type type,
				  struct ath12k_hal_reo_cmd *cmd);
int ath12k_wifi8_hal_reo_cmd_send_n_locked(struct ath12k_base *ab,
					   struct hal_srng *srng,
					   struct ath12k_reo_cmd_entry *entries,
					   int n);
int ath12k_wifi8_hal_reo_cmd_send_n(struct ath12k_base *ab,
				    struct hal_srng *srng,
				    struct ath12k_reo_cmd_entry *entries,
				    int n);
void ath12k_wifi8_hal_tx_configure_bank_register(struct ath12k_base *ab,
						 u32 bank_config,
						 u8 bank_id);
u32 ath12k_hal_tx_read_bank_register_internal(struct ath12k_base *ab, u8 bank_id);
int ath12k_wifi8_hal_tqm_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				  enum hal_tlv_tag_be type,
				  struct ath12k_hal_tqm_cmd *cmd);
int ath12k_wifi8_hal_tqm_remove_msdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd);
int ath12k_wifi8_hal_tqm_remove_mpdu_cmd(struct ath12k_base *ab,
					 struct hal_tlv_64_hdr *tlv,
					 struct ath12k_hal_tqm_cmd *cmd);
int ath12k_wifi8_hal_tqm_sync_cmd(struct ath12k_base *ab,
				  struct hal_tlv_64_hdr *tlv,
				  struct ath12k_hal_tqm_cmd *cmd);
int ath12k_wifi8_hal_tqm_update_msduq(struct ath12k_base *ab,
				      struct hal_tlv_64_hdr *tlv,
				      struct ath12k_hal_tqm_cmd *cmd);
void ath12k_wifi8_hal_tqm_remove_msdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status);
void ath12k_wifi8_hal_tqm_remove_mpdu_status(struct ath12k_base *ab,
					     struct hal_tlv_64_hdr *tlv,
					     struct hal_tqm_status *status);
void ath12k_wifi8_hal_tqm_sync_cmd_status(struct ath12k_base *ab,
					  struct hal_tlv_64_hdr *tlv,
					  struct hal_tqm_status *status);
int ath12k_wifi8_hal_tqm_cmd_staging_alloc(struct ath12k_base *ab);
void ath12k_wifi8_hal_tqm_cmd_staging_free(struct ath12k_base *ab);
void ath12k_wifi8_hal_tqm_get_mpduq_stats_cmd_status(struct ath12k_base *ab,
						     struct hal_tlv_64_hdr *tlv,
						     struct hal_tqm_status *status);
void ath12k_wifi8_hal_tqm_update_mpduq_cmd_status(struct ath12k_base *ab,
						  struct hal_tlv_64_hdr *tlv,
						  struct hal_tqm_status *status);
void ath12k_wifi8_hal_tx_sam_program_clear(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_sam_status(struct ath12k_base *ab, struct hal_tlv_64_hdr *tlv);
int ath12k_wifi8_hal_sam_cmd_staging_alloc(struct ath12k_base *ab);
void ath12k_wifi8_hal_sam_cmd_staging_free(struct ath12k_base *ab);
void ath12k_wifi8_hal_enable_service_category_sorting(struct ath12k_hal *hal);
void ath12k_wifi8_hal_tqm_update_msduq_cmd_status(struct ath12k_base *ab,
						  struct hal_tlv_64_hdr *tlv,
						  struct hal_tqm_status *status);
void ath12k_wifi8_hal_tqm_sorting_latch(struct ath12k_hal *hal);
u32 ath12k_wifi8_hal_tqm_get_active_msdu(struct ath12k_hal *hal,
					 enum hal_tqm_service_category svc);
int ath12k_wifi8_hal_tqm_get_svc_sorted_list(struct ath12k_hal *hal,
					     enum hal_tqm_service_category svc,
					     u8 idx,
					     u32 *flow_number,
					     u32 *msdu_count);
int ath12k_wifi8_hal_tx_sam_cmd_send(struct ath12k_base *ab, struct hal_srng *srng,
				     int src_link_id, enum hal_tlv_tag_be type, int id,
				     bool clear_all, bool force);
void ath12k_wifi8_hal_tx_set_pcp_tid_map(struct ath12k_base *ab, const u8 *map);
void ath12k_wifi8_hal_tx_set_tid_map_precedence(struct ath12k_base *ab,
						const u8 precedence);

/**
 * ath12k_wifi8_hal_tqm_fw_buf_release_cmd() - Build TQM FW-completion command
 * for FW-owned buffer release, routing completion to HAL_TQM_HOST_STATUS_RING_1
 * (fw_tqm2sw_ring). Uses TLV tag HAL_TQM_FW_COMPLETION_BO (637).
 *
 * Called via ath12k_wifi8_hal_tqm_cmd_send() switch case HAL_TQM_FW_COMPLETION_BO,
 * which in turn is called by ath12k_wifi8_dp_tqm_cmd_send() with
 * cmd->fw_buf_release_params populated.
 *
 * @ab:  ath12k_base
 * @tlv: TLV descriptor buffer (staging buffer)
 * @cmd: TQM command with fw_buf_release_params.{paddr, rbm, cookie} set
 */
void ath12k_wifi8_hal_tqm_fw_buf_release_cmd(struct ath12k_base *ab,
					    struct hal_tlv_64_hdr *tlv,
					    struct ath12k_hal_tqm_cmd *cmd);
#endif
