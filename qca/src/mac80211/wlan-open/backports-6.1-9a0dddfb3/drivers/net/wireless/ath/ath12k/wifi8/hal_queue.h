/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_QUEUE_H
#define ATH12K_HAL_QUEUE_H

#include <linux/types.h>
#include "hal_desc.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"

enum hal_tx_descriptor_header_owner {
	HAL_WIFIWBM_OWNED,
	HAL_WIFISW_OR_FW_OWNED,
	HAL_WIFITQM_OWNED,
	HAL_WIFIRXDMA_OWNED,
	HAL_WIFIREO_OWNED,
	HAL_WIFISWITCH_OWNED,
};

enum hal_descriptor_header_buffer_type {
	HAL_WIFITRANSMIT_MSDU_LINK_DESCRIPTOR,
	HAL_WIFITRANSMIT_MPDU_LINK_DESCRIPTOR,
	HAL_WIFITRANSMIT_MPDU_HEAD_DESCRIPTOR,
	HAL_WIFITRANSMIT_MPDU_EXT_DESCRIPTOR,
	HAL_WIFITRANSMIT_FLOW_DESCRIPTOR,
	HAL_WIFITRANSMIT_BUFFER,
};

enum hal_tx_msdu_drop_rule {
	HAL_WIFIDROP_DISABLED,
	HAL_WIFIDROP_ENABLED,
};

enum hal_tx_mpdu_type {
	HAL_WIFIMPDU_TYPE_BASIC,
	HAL_WIFIMPDU_TYPE_AMSDU,
};

enum hal_tqm_flow_handler {
	HAL_WIFITXPT_TO_FW,
	HAL_WIFITXPT_TO_TQM,
	HAL_WIFITXPT_TO_SW,
	HAL_WIFITXPT_USE_CCE,
};

enum hal_tqm_flow_loop_handler {
	HAL_WIFITXPT_LOOP_TO_FW,
	HAL_WIFITXPT_LOOP_TO_TQM,
	HAL_WIFITXPT_LOOP_TO_SW,
};

enum hal_classify_bank_subtype {
	HAL_CLASSIFY_BANK_SUBTYPE_NON_UDP,
	HAL_CLASSIFY_BANK_SUBTYPE_UDP,
	HAL_CLASSIFY_BANK_SUBTYPE_MIXED,
};

#define HAL_TX_DESCRIPTOR_HEADER_OWNER           GENMASK(3, 0)
#define HAL_TX_DESCRIPTOR_HEADER_BUFFER_TYPE     GENMASK(7, 4)
#define HAL_TX_DESCRIPTOR_HEADER_QUEUE_NUMBER    GENMASK(31, 8)

struct hal_uniform_descriptor_header {
	__le32 info0;
} __packed;

#define HAL_TX_MSDU_FLOW_QUEUE_SAM_ID                      GENMASK(12, 0)
#define HAL_TX_MSDU_FLOW_FLOW_VALID                        BIT(20)
#define HAL_TX_MSDU_FLOW_EMPTY_TO_N_EMPTY                  BIT(21)
#define HAL_TX_MSDU_FLOW_N_EMPTY_TO_EMPTY                  BIT(22)
#define HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION            BIT(23)
#define HAL_TX_MSDU_FLOW_THRESHOLD_NOTIFICATION2           BIT(24)
#define HAL_TX_MSDU_FLOW_ASSOCIATED_LINK_DESC_COUNT        GENMASK(26, 25)
#define HAL_TX_MSDU_FLOW_TID                               GENMASK(30, 27)
#define HAL_TX_MSDU_FLOW_MLO_VALID                         BIT(31)

#define HAL_TX_MSDU_FLOW_MSDU_COUNT			   GENMASK(15, 0)
#define HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES             GENMASK(31, 16)

#define HAL_TX_MSDU_FLOW_FLOW_BYTE_COUNT		   GENMASK(31, 0)

#define HAL_TX_MSDU_FLOW_SW_PEER_ID                        GENMASK(15, 0)
#define HAL_TX_MSDU_FLOW_SW_NOTIFICATION_THRES_2           GENMASK(31, 16)

#define HAL_TX_MSDU_FLOW_DROP_RULE                         BIT(0)
#define HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP0              BIT(2)
#define HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP1              BIT(3)
#define HAL_TX_MSDU_FLOW_GEN_SLOW_DROP_NOTIFICATION        BIT(4)
#define HAL_TX_MSDU_FLOW_GEN_MED_DROP_NOTIFICATION         BIT(6)
#define HAL_TX_MSDU_FLOW_GEN_HARD_DROP_NOTIFICATION        BIT(8)
#define HAL_TX_MSDU_FLOW_ADD_FRAME_COUNT_SINCE_DROP	   GENMASK(13, 10)
#define HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP2              BIT(14)
#define HAL_TX_MSDU_FLOW_TQM_STATUS_FOR_CHIP3              BIT(15)
#define HAL_TX_MSDU_FLOW_SLOW_DROP_THRESHOLD               GENMASK(31, 16)

#define HAL_TX_MSDU_FLOW_MED_DROP_THRESHOLD                GENMASK(15, 0)
#define HAL_TX_MSDU_FLOW_HARD_DROP_THRESHOLD               GENMASK(31, 16)

//info13
#define HAL_TX_MSDU_FLOW_PROCESSED_BYTE_COUNT_48_32	GENMASK(15, 0)
#define HAL_TX_MSDU_FLOW_TQM_STATS_ID		GENMASK(10, 0)

//info16
#define HAL_TX_MSDU_FLOW_RED_MSDU_DROP_THRESHOLD		GENMASK(15, 0)
#define HAL_TX_MSDU_FLOW_GEN_GREEN_MSDU_DROP_NOTIFICATION	BIT(16)
#define HAL_TX_MSDU_FLOW_GEN_YELLOW_MSDU_DROP_NOTIFICATION	BIT(17)
#define HAL_TX_MSDU_FLOW_GEN_RED_MSDU_DROP_NOTIFICATION		BIT(18)
#define HAL_TX_MSDU_FLOW_GREEN_MSDU_DROP_NOTIFICATION_DONE	BIT(19)
#define HAL_TX_MSDU_FLOW_YELLOW_MSDU_DROP_NOTIFICATION_DONE	BIT(20)
#define HAL_TX_MSDU_FLOW_RED_MSDU_DROP_NOTIFICATION_DONE	BIT(21)
#define HAL_TX_MSDU_FLOW_SERVICE_CATEGORY			GENMASK(23, 22)
#define HAL_TX_MSDU_FLOW_SERVICE_CATEGORY_VALID			BIT(24)
#define HAL_TX_MSDU_FLOW_PER_PEER_FLOW_NUMBER			GENMASK(31, 25)

struct hal_tx_msdu_flow {
	struct hal_uniform_descriptor_header header;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	struct ath12k_buffer_addr buf_addr_info_head;
	__le32 info4;
	struct ath12k_buffer_addr buf_addr_info_tail;
	__le32 info5;
	__le32 info6;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
	__le32 info12;
	__le32 info13;
	__le32 info14;
	__le32 info15;
	__le32 info16;
	__le32 info17;
	struct ath12k_buffer_addr buf_addr_info_link1;
	struct ath12k_buffer_addr buf_addr_info_link2;
	struct ath12k_buffer_addr buf_addr_info_link3;
	struct ath12k_buffer_addr buf_addr_info_first_ext;
	struct ath12k_buffer_addr buf_addr_info_last_ext;
	__le32 info18;
	__le32 info19;
	__le32 info20;
	__le32 info21;
	__le32 info22;
	__le32 info23;
	__le32 info24;
	__le32 info25;
	__le32 info26;
	__le32 info27;
	__le32 info28;
	__le32 info29;
	__le32 info30;
	__le32 info31;
	__le32 info32;
} __packed;

struct ath12k_tx_rate_stats_info {
	__le32 info0;
	__le32 info1;
	__le32 info2;
} __packed;

#define HAL_TX_MPDU_QUEUE_HEAD_MLO_VALID                        BIT(0)
#define	HAL_TX_MPDU_QUEUE_HEAD_MPDU_TYPE			BIT(15)
#define	HAL_TX_MPDU_QUEUE_HEAD_MPDU_COUNT			GENMASK(31, 16)

#define HAL_TX_MPDU_QUEUE_HEAD_QUEUE_BYTE_COUNT			GENMASK(31, 0)

#define HAL_TX_MPDU_QUEUE_HEAD_MPDU_START_SEQ_NUM		GENMASK(11, 0)
#define HAL_TX_MPDU_QUEUE_HEAD_QUEUE_VALID                      BIT(12)
#define HAL_TX_MPDU_QUEUE_HEAD_ASSOC_LINK_DESC_CNT              GENMASK(14, 13)
#define HAL_TX_MPDU_QUEUE_HEAD_MPDU_LAST_SEQ_NUM                GENMASK(26, 15)
#define HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_LINK_DESC_IDX		GENMASK(31, 27)

#define HAL_TX_MPDU_QUEUE_HEAD_TID                              GENMASK(6, 3)
#define HAL_TX_MPDU_QUEUE_HEAD_NUM_EXT_DESC_IN_USE		GENMASK(12, 7)
#define HAL_TX_MPDU_QUEUE_HEAD_SW_PEER_ID                       GENMASK(31, 16)

#define HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_31_0                     GENMASK(31, 0)

#define HAL_TX_MPDU_QUEUE_HEAD_PN_ADDR_39_32                    GENMASK(7, 0)
#define HAL_TX_MPDU_QUEUE_HEAD_PN_INC_VALUE                     GENMASK(15, 8)
#define HAL_TX_MPDU_QUEUE_HEAD_MPDU_HDR_LEN                     GENMASK(24, 16)
#define HAL_TX_MPDU_QUEUE_HEAD_NUM_OF_EXT_DESC                  GENMASK(30, 25)

#define HAL_TX_MPDU_QUEUE_HEAD_LAST_MPDU_INDEX			GENMASK(2, 0)

#define HAL_TX_MPDU_QUEUE_HEAD_LINK0_ID                         GENMASK(19, 17)
#define HAL_TX_MPDU_QUEUE_HEAD_LINK1_ID                         GENMASK(22, 20)
#define HAL_TX_MPDU_QUEUE_HEAD_LINK2_ID                         GENMASK(25, 23)

#define HAL_TX_MPDU_QUEUE_HEAD_SAM_ID                           GENMASK(10, 0)

#define HAL_TX_MPDU_QUEUE_HEAD_TQM_PER_MLO_STATS_ID		GENMASK(14, 4)

struct hal_tx_mpdu_queue_head {
	struct hal_uniform_descriptor_header header;
	__le32 info0;
	__le32 info1;
	__le32 info2;
	struct ath12k_buffer_addr buf_addr_info_last_mpdu;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	struct ath12k_buffer_addr buf_addr_info_first_ext;
	struct ath12k_buffer_addr buf_addr_info_last_ext;
	__le32 info7;
	__le32 info8;
	__le32 info9;
	__le32 info10;
	__le32 info11;
	__le32 info12;
	__le32 info13;
	__le32 info14;
	__le32 info15;
	__le32 info16;
	__le32 info17;
	__le32 info18;
	struct ath12k_tx_rate_stats_info tx_rate_stats_info_link0;
	struct ath12k_tx_rate_stats_info tx_rate_stats_info_link1;
	struct ath12k_tx_rate_stats_info tx_rate_stats_info_link2;
	struct ath12k_buffer_addr buf_addr_info_link0;
	struct ath12k_buffer_addr buf_addr_info_link1;
	struct ath12k_buffer_addr buf_addr_info_link2;
	struct ath12k_buffer_addr buf_addr_info_link3;
	struct ath12k_buffer_addr last_listed_mpdu_queue_ext_pointer;
	__le32 info19;
	__le32 info20;
	__le32 info21;
} __packed;

#define HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_NON_UDP_39_8     GENMASK(31, 0)
#define HAL_TXPT_CLASSIFY_TQM_FLOW_PTR_UDP_39_8         GENMASK(31, 0)
#define HAL_TXPT_CLASSIFY_TQM_FLOW_HANDLER              GENMASK(1, 0)
#define HAL_TXPT_CLASSIFY_TQM_FLOW_LOOP_HANDLER         GENMASK(3, 2)
#define HAL_TXPT_CLASSIFY_MSDU_DROP                     BIT(4)
#define HAL_TXPT_CLASSIFY_METADATA                      GENMASK(15, 5)
#define HAL_TXPT_CLASSIFY_TCL_FW_LINK_ID		GENMASK(18, 16)

struct hal_txpt_classify_info {
	__le32 info0;
	__le32 info1;
	__le32 info2;
} __packed;

struct hal_tx_msdu_flow_info {
	dma_addr_t paddr;
	u32 queue_number;
	enum hal_tqm_service_category svc;
	u16 peer_id;
	u16 msduq_sam_id;
	u8 bitmap;
	u8 tid:4,
	   mlo:1,
	   is_mgmtq:1;
	u16 stats_id;
};

struct hal_tx_mpdu_queue_head_info {
	dma_addr_t paddr;
	dma_addr_t pn_dma_addr;
	u32 queue_number;
	u32 header_len;
	u16 peer_id;
	u16 mpduq_sam_id;
	u8 tid:4,
	   encap_type:2,
	   wapi:1,
	   mlo:1;
	bool is_mgmtq;
	u8 assoc_link_id;
	u8 link_id1;
	u8 link_id2;
	u16 stats_id;
};

struct hal_txpt_classify_data {
	dma_addr_t paddr;
	u32 msdu_paddr;
	u32 subtype:2,
	    flow_handler:2,
	    flow_loop_handler:2,
	    msdu_drop:1,
	    metadata:11,
	    assoc_link_id:8,
	    reserved:6;
};

int ath12k_wifi8_hal_tx_msdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 msduq_idx,
					 struct hal_tx_msdu_flow_info *ti);
int ath12k_wifi8_hal_tx_mpdu_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 u32 mpduq_idx,
					 struct hal_tx_mpdu_queue_head_info *ti);
void ath12k_wifi8_hal_txpt_classify_info_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					       struct hal_txpt_classify_info *tx_tid_ptr,
					       struct hal_txpt_classify_data *ti);
bool ath12k_wifi8_hal_msduq_is_valid(struct hal_tx_msdu_flow *msduq);
bool ath12k_wifi8_hal_mpduq_is_valid(struct hal_tx_mpdu_queue_head *mpduq);
u32 ath12k_wifi8_hal_get_txpt_flow_ptr(struct hal_txpt_classify_info *tx_tid_ptr,
				       enum hal_classify_bank_subtype subtype);
void ath12k_wifi8_hal_msduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_msdu_flow *msduq,
					dma_addr_t paddr);
void ath12k_wifi8_hal_mpduq_set_invalid(struct ath12k_dp_hw_group *dp_hw_grp,
					struct hal_tx_mpdu_queue_head *mpduq,
					dma_addr_t paddr);
int ath12k_wifi8_hal_tx_msdu_queue_cleanup(struct ath12k_dp_hw_group *dp_hw_grp,
					   u32 msduq_idx,
					   struct hal_tx_msdu_flow_info *ti);
int ath12k_wifi8_hal_tx_mpdu_queue_cleanup(struct ath12k_dp_hw_group *dp_hw_grp,
					   u32 mpduq_idx,
					   struct hal_tx_mpdu_queue_head_info *ti);
int ath12k_wifi8_cleanup_all_peers_tx_queues(struct ath12k_dp_hw *dp_hw,
					     struct ath12k_dp_hw_group *dp_hw_grp,
					     struct ath12k_pdev_dp *dp_pdev);
#endif
