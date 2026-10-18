/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_ACL_REG_H_
#define _HPPE_ACL_REG_H_

/*[table] IPO_CNT_TBL*/
#define IPO_CNT_TBL
#define IPO_CNT_TBL_ADDRESS 0x74000
#define IPO_CNT_TBL_NUM     128
#define IPO_CNT_TBL_INC     0x10
#define IPO_CNT_TBL_TYPE    REG_TYPE_RW
#define IPO_CNT_TBL_DEFAULT 0x0
	/*[field] HIT_PKT_CNT*/
	#define IPO_CNT_TBL_HIT_PKT_CNT
	#define IPO_CNT_TBL_HIT_PKT_CNT_OFFSET  0
	#define IPO_CNT_TBL_HIT_PKT_CNT_LEN     32
	#define IPO_CNT_TBL_HIT_PKT_CNT_DEFAULT 0x0
	/*[field] HIT_BYTE_CNT*/
	#define IPO_CNT_TBL_HIT_BYTE_CNT
	#define IPO_CNT_TBL_HIT_BYTE_CNT_OFFSET  32
	#define IPO_CNT_TBL_HIT_BYTE_CNT_LEN     40
	#define IPO_CNT_TBL_HIT_BYTE_CNT_DEFAULT 0x0

struct ipo_cnt_tbl {
	a_uint32_t  hit_pkt_cnt:32;
	a_uint32_t  hit_byte_cnt_0:32;
	a_uint32_t  hit_byte_cnt_1:8;
	a_uint32_t  _reserved0:24;
};

union ipo_cnt_tbl_u {
	a_uint32_t val[3];
	struct ipo_cnt_tbl bf;
};

/*[table] IPO_ACTION*/
#define IPO_ACTION
#define IPO_ACTION_ADDRESS 0x8000
#define IPO_ACTION_NUM     128
#define IPO_ACTION_INC     0x20
#define IPO_ACTION_TYPE    REG_TYPE_RW
#define IPO_ACTION_DEFAULT 0x0
	/*[field] DEST_INFO_CHANGE_EN*/
	#define IPO_ACTION_DEST_INFO_CHANGE_EN
	#define IPO_ACTION_DEST_INFO_CHANGE_EN_OFFSET  0
	#define IPO_ACTION_DEST_INFO_CHANGE_EN_LEN     1
	#define IPO_ACTION_DEST_INFO_CHANGE_EN_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define IPO_ACTION_FWD_CMD
	#define IPO_ACTION_FWD_CMD_OFFSET  1
	#define IPO_ACTION_FWD_CMD_LEN     2
	#define IPO_ACTION_FWD_CMD_DEFAULT 0x0
	/*[field] DEST_INFO*/
	#define IPO_ACTION_DEST_INFO
	#define IPO_ACTION_DEST_INFO_OFFSET  3
	#define IPO_ACTION_DEST_INFO_LEN     14
	#define IPO_ACTION_DEST_INFO_DEFAULT 0x0
	/*[field] MIRROR_EN*/
	#define IPO_ACTION_MIRROR_EN
	#define IPO_ACTION_MIRROR_EN_OFFSET  17
	#define IPO_ACTION_MIRROR_EN_LEN     1
	#define IPO_ACTION_MIRROR_EN_DEFAULT 0x0
	/*[field] BYPASS_BITMAP*/
	#define IPO_ACTION_BYPASS_BITMAP
	#define IPO_ACTION_BYPASS_BITMAP_OFFSET  18
	#define IPO_ACTION_BYPASS_BITMAP_LEN     32
	#define IPO_ACTION_BYPASS_BITMAP_DEFAULT 0x0
	/*[field] SVID_CHANGE_EN*/
	#define IPO_ACTION_SVID_CHANGE_EN
	#define IPO_ACTION_SVID_CHANGE_EN_OFFSET  50
	#define IPO_ACTION_SVID_CHANGE_EN_LEN     1
	#define IPO_ACTION_SVID_CHANGE_EN_DEFAULT 0x0
	/*[field] STAG_FMT*/
	#define IPO_ACTION_STAG_FMT
	#define IPO_ACTION_STAG_FMT_OFFSET  51
	#define IPO_ACTION_STAG_FMT_LEN     2
	#define IPO_ACTION_STAG_FMT_DEFAULT 0x0
	/*[field] SVID*/
	#define IPO_ACTION_SVID
	#define IPO_ACTION_SVID_OFFSET  53
	#define IPO_ACTION_SVID_LEN     12
	#define IPO_ACTION_SVID_DEFAULT 0x0
	/*[field] CVID_CHANGE_EN*/
	#define IPO_ACTION_CVID_CHANGE_EN
	#define IPO_ACTION_CVID_CHANGE_EN_OFFSET  65
	#define IPO_ACTION_CVID_CHANGE_EN_LEN     1
	#define IPO_ACTION_CVID_CHANGE_EN_DEFAULT 0x0
	/*[field] CTAG_FMT*/
	#define IPO_ACTION_CTAG_FMT
	#define IPO_ACTION_CTAG_FMT_OFFSET  66
	#define IPO_ACTION_CTAG_FMT_LEN     2
	#define IPO_ACTION_CTAG_FMT_DEFAULT 0x0
	/*[field] CVID*/
	#define IPO_ACTION_CVID
	#define IPO_ACTION_CVID_OFFSET  68
	#define IPO_ACTION_CVID_LEN     12
	#define IPO_ACTION_CVID_DEFAULT 0x0
	/*[field] DSCP_TC_CHANGE_EN*/
	#define IPO_ACTION_DSCP_TC_CHANGE_EN
	#define IPO_ACTION_DSCP_TC_CHANGE_EN_OFFSET  80
	#define IPO_ACTION_DSCP_TC_CHANGE_EN_LEN     1
	#define IPO_ACTION_DSCP_TC_CHANGE_EN_DEFAULT 0x0
	/*[field] DSCP_TC*/
	#define IPO_ACTION_DSCP_TC
	#define IPO_ACTION_DSCP_TC_OFFSET  81
	#define IPO_ACTION_DSCP_TC_LEN     8
	#define IPO_ACTION_DSCP_TC_DEFAULT 0x0
	/*[field] STAG_PCP_CHANGE_EN*/
	#define IPO_ACTION_STAG_PCP_CHANGE_EN
	#define IPO_ACTION_STAG_PCP_CHANGE_EN_OFFSET  89
	#define IPO_ACTION_STAG_PCP_CHANGE_EN_LEN     4
	#define IPO_ACTION_STAG_PCP_CHANGE_EN_DEFAULT 0x0
	/*[field] STAG_PCP*/
	#define IPO_ACTION_STAG_PCP
	#define IPO_ACTION_STAG_PCP_OFFSET  93
	#define IPO_ACTION_STAG_PCP_LEN     3
	#define IPO_ACTION_STAG_PCP_DEFAULT 0x0
	/*[field] STAG_DEI_CHANGE_EN*/
	#define IPO_ACTION_STAG_DEI_CHANGE_EN
	#define IPO_ACTION_STAG_DEI_CHANGE_EN_OFFSET  96
	#define IPO_ACTION_STAG_DEI_CHANGE_EN_LEN     2
	#define IPO_ACTION_STAG_DEI_CHANGE_EN_DEFAULT 0x0
	/*[field] STAG_DEI*/
	#define IPO_ACTION_STAG_DEI
	#define IPO_ACTION_STAG_DEI_OFFSET  98
	#define IPO_ACTION_STAG_DEI_LEN     1
	#define IPO_ACTION_STAG_DEI_DEFAULT 0x0
	/*[field] CTAG_PCP_CHANGE_EN*/
	#define IPO_ACTION_CTAG_PCP_CHANGE_EN
	#define IPO_ACTION_CTAG_PCP_CHANGE_EN_OFFSET  99
	#define IPO_ACTION_CTAG_PCP_CHANGE_EN_LEN     4
	#define IPO_ACTION_CTAG_PCP_CHANGE_EN_DEFAULT 0x0
	/*[field] CTAG_PCP*/
	#define IPO_ACTION_CTAG_PCP
	#define IPO_ACTION_CTAG_PCP_OFFSET  103
	#define IPO_ACTION_CTAG_PCP_LEN     3
	#define IPO_ACTION_CTAG_PCP_DEFAULT 0x0
	/*[field] CTAG_DEI_CHANGE_EN*/
	#define IPO_ACTION_CTAG_DEI_CHANGE_EN
	#define IPO_ACTION_CTAG_DEI_CHANGE_EN_OFFSET  106
	#define IPO_ACTION_CTAG_DEI_CHANGE_EN_LEN     2
	#define IPO_ACTION_CTAG_DEI_CHANGE_EN_DEFAULT 0x0
	/*[field] CTAG_DEI*/
	#define IPO_ACTION_CTAG_DEI
	#define IPO_ACTION_CTAG_DEI_OFFSET  108
	#define IPO_ACTION_CTAG_DEI_LEN     1
	#define IPO_ACTION_CTAG_DEI_DEFAULT 0x0
	/*[field] TAGS_TO_REMOVE*/
	#define IPO_ACTION_TAGS_TO_REMOVE
	#define IPO_ACTION_TAGS_TO_REMOVE_OFFSET  109
	#define IPO_ACTION_TAGS_TO_REMOVE_LEN     2
	#define IPO_ACTION_TAGS_TO_REMOVE_DEFAULT 0x0
	/*[field] STPID_CMD*/
	#define IPO_ACTION_STPID_CMD
	#define IPO_ACTION_STPID_CMD_OFFSET  111
	#define IPO_ACTION_STPID_CMD_LEN     2
	#define IPO_ACTION_STPID_CMD_DEFAULT 0x0
	/*[field] STPID_INDEX*/
	#define IPO_ACTION_STPID_INDEX
	#define IPO_ACTION_STPID_INDEX_OFFSET  113
	#define IPO_ACTION_STPID_INDEX_LEN     2
	#define IPO_ACTION_STPID_INDEX_DEFAULT 0x0
	/*[field] CTPID_CMD*/
	#define IPO_ACTION_CTPID_CMD
	#define IPO_ACTION_CTPID_CMD_OFFSET  115
	#define IPO_ACTION_CTPID_CMD_LEN     2
	#define IPO_ACTION_CTPID_CMD_DEFAULT 0x0
	/*[field] CTPID_INDEX*/
	#define IPO_ACTION_CTPID_INDEX
	#define IPO_ACTION_CTPID_INDEX_OFFSET  117
	#define IPO_ACTION_CTPID_INDEX_LEN     2
	#define IPO_ACTION_CTPID_INDEX_DEFAULT 0x0
	/*[field] DSCP_PBIT_MAPPING_INDEX*/
	#define IPO_ACTION_DSCP_PBIT_MAPPING_INDEX
	#define IPO_ACTION_DSCP_PBIT_MAPPING_INDEX_OFFSET  119
	#define IPO_ACTION_DSCP_PBIT_MAPPING_INDEX_LEN     1
	#define IPO_ACTION_DSCP_PBIT_MAPPING_INDEX_DEFAULT 0x0
	/*[field] COUNTER_EN*/
	#define IPO_ACTION_COUNTER_EN
	#define IPO_ACTION_COUNTER_EN_OFFSET  120
	#define IPO_ACTION_COUNTER_EN_LEN     1
	#define IPO_ACTION_COUNTER_EN_DEFAULT 0x0
	/*[field] COUNTER_ID*/
	#define IPO_ACTION_COUNTER_ID
	#define IPO_ACTION_COUNTER_ID_OFFSET  121
	#define IPO_ACTION_COUNTER_ID_LEN     7
	#define IPO_ACTION_COUNTER_ID_DEFAULT 0x0
	/*[field] COUNTER_MODE*/
	#define IPO_ACTION_COUNTER_MODE
	#define IPO_ACTION_COUNTER_MODE_OFFSET  128
	#define IPO_ACTION_COUNTER_MODE_LEN     1
	#define IPO_ACTION_COUNTER_MODE_DEFAULT 0x0
	/*[field] ENQUEUE_PRI_CHANGE_EN*/
	#define IPO_ACTION_ENQUEUE_PRI_CHANGE_EN
	#define IPO_ACTION_ENQUEUE_PRI_CHANGE_EN_OFFSET  129
	#define IPO_ACTION_ENQUEUE_PRI_CHANGE_EN_LEN     1
	#define IPO_ACTION_ENQUEUE_PRI_CHANGE_EN_DEFAULT 0x0
	/*[field] ENQUEUE_PRI*/
	#define IPO_ACTION_ENQUEUE_PRI
	#define IPO_ACTION_ENQUEUE_PRI_OFFSET  130
	#define IPO_ACTION_ENQUEUE_PRI_LEN     4
	#define IPO_ACTION_ENQUEUE_PRI_DEFAULT 0x0
	/*[field] INT_DP_CHANGE_EN*/
	#define IPO_ACTION_INT_DP_CHANGE_EN
	#define IPO_ACTION_INT_DP_CHANGE_EN_OFFSET  134
	#define IPO_ACTION_INT_DP_CHANGE_EN_LEN     1
	#define IPO_ACTION_INT_DP_CHANGE_EN_DEFAULT 0x0
	/*[field] INT_DP*/
	#define IPO_ACTION_INT_DP
	#define IPO_ACTION_INT_DP_OFFSET  135
	#define IPO_ACTION_INT_DP_LEN     2
	#define IPO_ACTION_INT_DP_DEFAULT 0x0
	/*[field] POLICER_EN*/
	#define IPO_ACTION_POLICER_EN
	#define IPO_ACTION_POLICER_EN_OFFSET  137
	#define IPO_ACTION_POLICER_EN_LEN     1
	#define IPO_ACTION_POLICER_EN_DEFAULT 0x0
	/*[field] POLICER_INDEX*/
	#define IPO_ACTION_POLICER_INDEX
	#define IPO_ACTION_POLICER_INDEX_OFFSET  138
	#define IPO_ACTION_POLICER_INDEX_LEN     9
	#define IPO_ACTION_POLICER_INDEX_DEFAULT 0x0
	/*[field] QID_EN*/
	#define IPO_ACTION_QID_EN
	#define IPO_ACTION_QID_EN_OFFSET  147
	#define IPO_ACTION_QID_EN_LEN     1
	#define IPO_ACTION_QID_EN_DEFAULT 0x0
	/*[field] QID*/
	#define IPO_ACTION_QID
	#define IPO_ACTION_QID_OFFSET  148
	#define IPO_ACTION_QID_LEN     8
	#define IPO_ACTION_QID_DEFAULT 0x0
	/*[field] SERVICE_CODE_EN*/
	#define IPO_ACTION_SERVICE_CODE_EN
	#define IPO_ACTION_SERVICE_CODE_EN_OFFSET  156
	#define IPO_ACTION_SERVICE_CODE_EN_LEN     1
	#define IPO_ACTION_SERVICE_CODE_EN_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define IPO_ACTION_SERVICE_CODE
	#define IPO_ACTION_SERVICE_CODE_OFFSET  157
	#define IPO_ACTION_SERVICE_CODE_LEN     8
	#define IPO_ACTION_SERVICE_CODE_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IPO_ACTION_SYN_TOGGLE
	#define IPO_ACTION_SYN_TOGGLE_OFFSET  165
	#define IPO_ACTION_SYN_TOGGLE_LEN     1
	#define IPO_ACTION_SYN_TOGGLE_DEFAULT 0x0
	/*[field] CPU_CODE_EN*/
	#define IPO_ACTION_CPU_CODE_EN
	#define IPO_ACTION_CPU_CODE_EN_OFFSET  166
	#define IPO_ACTION_CPU_CODE_EN_LEN     1
	#define IPO_ACTION_CPU_CODE_EN_DEFAULT 0x0
	/*[field] CPU_CODE*/
	#define IPO_ACTION_CPU_CODE
	#define IPO_ACTION_CPU_CODE_OFFSET  167
	#define IPO_ACTION_CPU_CODE_LEN     8
	#define IPO_ACTION_CPU_CODE_DEFAULT 0x0
	/*[field] METADATA_EN*/
	#define IPO_ACTION_METADATA_EN
	#define IPO_ACTION_METADATA_EN_OFFSET  175
	#define IPO_ACTION_METADATA_EN_LEN     1
	#define IPO_ACTION_METADATA_EN_DEFAULT 0x0
	/*[field] DSCP_TC_MASK*/
	#define IPO_ACTION_DSCP_TC_MASK
	#define IPO_ACTION_DSCP_TC_MASK_OFFSET  176
	#define IPO_ACTION_DSCP_TC_MASK_LEN     8
	#define IPO_ACTION_DSCP_TC_MASK_DEFAULT 0x0
	/*[field] QOS_RES_PREC*/
	#define IPO_ACTION_QOS_RES_PREC
	#define IPO_ACTION_QOS_RES_PREC_OFFSET  184
	#define IPO_ACTION_QOS_RES_PREC_LEN     3
	#define IPO_ACTION_QOS_RES_PREC_DEFAULT 0x0
	/*[field] METADATA_PRI*/
	#define IPO_ACTION_METADATA_PRI
	#define IPO_ACTION_METADATA_PRI_OFFSET  187
	#define IPO_ACTION_METADATA_PRI_LEN     4
	#define IPO_ACTION_METADATA_PRI_DEFAULT 0x0

struct ipo_action {
	a_uint32_t  dest_info_change_en:1;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  dest_info:14;
	a_uint32_t  mirror_en:1;
	a_uint32_t  bypass_bitmap_0:14;
	a_uint32_t  bypass_bitmap_1:18;
	a_uint32_t  svid_change_en:1;
	a_uint32_t  stag_fmt:2;
	a_uint32_t  svid_0:11;
	a_uint32_t  svid_1:1;
	a_uint32_t  cvid_change_en:1;
	a_uint32_t  ctag_fmt:2;
	a_uint32_t  cvid:12;
	a_uint32_t  dscp_tc_change_en:1;
	a_uint32_t  dscp_tc:8;
	a_uint32_t  stag_pcp_change_en:4;
	a_uint32_t  stag_pcp:3;
	a_uint32_t  stag_dei_change_en:2;
	a_uint32_t  stag_dei:1;
	a_uint32_t  ctag_pcp_change_en:4;
	a_uint32_t  ctag_pcp:3;
	a_uint32_t  ctag_dei_change_en:2;
	a_uint32_t  ctag_dei:1;
	a_uint32_t  tags_to_remove:2;
	a_uint32_t  stpid_cmd:2;
	a_uint32_t  stpid_index:2;
	a_uint32_t  ctpid_cmd:2;
	a_uint32_t  ctpid_index:2;
	a_uint32_t  dscp_pbit_mapping_index:1;
	a_uint32_t  counter_en:1;
	a_uint32_t  counter_id:7;
	a_uint32_t  counter_mode:1;
	a_uint32_t  enqueue_pri_change_en:1;
	a_uint32_t  enqueue_pri:4;
	a_uint32_t  int_dp_change_en:1;
	a_uint32_t  int_dp:2;
	a_uint32_t  policer_en:1;
	a_uint32_t  policer_index:9;
	a_uint32_t  qid_en:1;
	a_uint32_t  qid:8;
	a_uint32_t  service_code_en:1;
	a_uint32_t  service_code_0:3;
	a_uint32_t  service_code_1:5;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  cpu_code_en:1;
	a_uint32_t  cpu_code:8;
	a_uint32_t  metadata_en:1;
	a_uint32_t  dscp_tc_mask:8;
	a_uint32_t  qos_res_prec:3;
	a_uint32_t  metadata_pri:4;
	a_uint32_t  _reserved0:1;
};

union ipo_action_u {
	a_uint32_t val[6];
	struct ipo_action bf;
};

/*[register] RULE_EXT_1_REG*/
#define RULE_EXT_1_REG
#define RULE_EXT_1_REG_ADDRESS 0x4000
#define RULE_EXT_1_REG_NUM     16
#define RULE_EXT_1_REG_INC     0x4
#define RULE_EXT_1_REG_TYPE    REG_TYPE_RW
#define RULE_EXT_1_REG_DEFAULT 0x0
	/*[field] EXT2_0*/
	#define RULE_EXT_1_REG_EXT2_0
	#define RULE_EXT_1_REG_EXT2_0_OFFSET  0
	#define RULE_EXT_1_REG_EXT2_0_LEN     1
	#define RULE_EXT_1_REG_EXT2_0_DEFAULT 0x0
	/*[field] EXT2_1*/
	#define RULE_EXT_1_REG_EXT2_1
	#define RULE_EXT_1_REG_EXT2_1_OFFSET  1
	#define RULE_EXT_1_REG_EXT2_1_LEN     1
	#define RULE_EXT_1_REG_EXT2_1_DEFAULT 0x0
	/*[field] EXT2_2*/
	#define RULE_EXT_1_REG_EXT2_2
	#define RULE_EXT_1_REG_EXT2_2_OFFSET  2
	#define RULE_EXT_1_REG_EXT2_2_LEN     1
	#define RULE_EXT_1_REG_EXT2_2_DEFAULT 0x0
	/*[field] EXT2_3*/
	#define RULE_EXT_1_REG_EXT2_3
	#define RULE_EXT_1_REG_EXT2_3_OFFSET  3
	#define RULE_EXT_1_REG_EXT2_3_LEN     1
	#define RULE_EXT_1_REG_EXT2_3_DEFAULT 0x0

struct rule_ext_1_reg {
	a_uint32_t  ext2_0:1;
	a_uint32_t  ext2_1:1;
	a_uint32_t  ext2_2:1;
	a_uint32_t  ext2_3:1;
	a_uint32_t  _reserved0:28;
};

union rule_ext_1_reg_u {
	a_uint32_t val;
	struct rule_ext_1_reg bf;
};

/*[register] RULE_EXT_2_REG*/
#define RULE_EXT_2_REG
#define RULE_EXT_2_REG_ADDRESS 0x4100
#define RULE_EXT_2_REG_NUM     16
#define RULE_EXT_2_REG_INC     0x4
#define RULE_EXT_2_REG_TYPE    REG_TYPE_RW
#define RULE_EXT_2_REG_DEFAULT 0x0
	/*[field] EXT4_0*/
	#define RULE_EXT_2_REG_EXT4_0
	#define RULE_EXT_2_REG_EXT4_0_OFFSET  0
	#define RULE_EXT_2_REG_EXT4_0_LEN     1
	#define RULE_EXT_2_REG_EXT4_0_DEFAULT 0x0
	/*[field] EXT4_1*/
	#define RULE_EXT_2_REG_EXT4_1
	#define RULE_EXT_2_REG_EXT4_1_OFFSET  1
	#define RULE_EXT_2_REG_EXT4_1_LEN     1
	#define RULE_EXT_2_REG_EXT4_1_DEFAULT 0x0

struct rule_ext_2_reg {
	a_uint32_t  ext4_0:1;
	a_uint32_t  ext4_1:1;
	a_uint32_t  _reserved0:30;
};

union rule_ext_2_reg_u {
	a_uint32_t val;
	struct rule_ext_2_reg bf;
};

/*[register] RULE_EXT_4_REG*/
#define RULE_EXT_4_REG
#define RULE_EXT_4_REG_ADDRESS 0x4200
#define RULE_EXT_4_REG_NUM     16
#define RULE_EXT_4_REG_INC     0x4
#define RULE_EXT_4_REG_TYPE    REG_TYPE_RW
#define RULE_EXT_4_REG_DEFAULT 0x0
	/*[field] EXT8*/
	#define RULE_EXT_4_REG_EXT8
	#define RULE_EXT_4_REG_EXT8_OFFSET  0
	#define RULE_EXT_4_REG_EXT8_LEN     1
	#define RULE_EXT_4_REG_EXT8_DEFAULT 0x0

struct rule_ext_4_reg {
	a_uint32_t  ext8:1;
	a_uint32_t  _reserved0:31;
};

union rule_ext_4_reg_u {
	a_uint32_t val;
	struct rule_ext_4_reg bf;
};

#endif
