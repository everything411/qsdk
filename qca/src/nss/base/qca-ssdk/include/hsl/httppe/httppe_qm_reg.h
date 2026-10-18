/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_QM_REG_H_
#define _HTTPPE_QM_REG_H_

/*[register] MC_ENQ_CTRL*/
#define MC_ENQ_CTRL
#define MC_ENQ_CTRL_ADDRESS 0xa80
#define MC_ENQ_CTRL_NUM     1
#define MC_ENQ_CTRL_INC     0x4
#define MC_ENQ_CTRL_TYPE    REG_TYPE_RW
#define MC_ENQ_CTRL_DEFAULT 0x10
	/*[field] UC_PORT_ID*/
	#define MC_ENQ_CTRL_UC_PORT_ID
	#define MC_ENQ_CTRL_UC_PORT_ID_OFFSET  0
	#define MC_ENQ_CTRL_UC_PORT_ID_LEN     4
	#define MC_ENQ_CTRL_UC_PORT_ID_DEFAULT 0x0
	/*[field] UC_ENQ_EN*/
	#define MC_ENQ_CTRL_UC_ENQ_EN
	#define MC_ENQ_CTRL_UC_ENQ_EN_OFFSET  4
	#define MC_ENQ_CTRL_UC_ENQ_EN_LEN     1
	#define MC_ENQ_CTRL_UC_ENQ_EN_DEFAULT 0x1
	/*[field] DOT1P_PON_VP*/
	#define MC_ENQ_CTRL_DOT1P_PON_VP
	#define MC_ENQ_CTRL_DOT1P_PON_VP_OFFSET  5
	#define MC_ENQ_CTRL_DOT1P_PON_VP_LEN     8
	#define MC_ENQ_CTRL_DOT1P_PON_VP_DEFAULT 0x0

struct mc_enq_ctrl {
	a_uint32_t  uc_port_id:4;
	a_uint32_t  uc_enq_en:1;
	a_uint32_t  dot1p_pon_vp:8;
	a_uint32_t  _reserved0:19;
};

union mc_enq_ctrl_u {
	a_uint32_t val;
	struct mc_enq_ctrl bf;
};

/*[table] PORT_VSI_ENQUEUE_MAP*/
#define PORT_VSI_ENQUEUE_MAP
#define PORT_VSI_ENQUEUE_MAP_ADDRESS 0x29000
#define PORT_VSI_ENQUEUE_MAP_NUM     608
#define PORT_VSI_ENQUEUE_MAP_INC     0x4
#define PORT_VSI_ENQUEUE_MAP_TYPE    REG_TYPE_RW
#define PORT_VSI_ENQUEUE_MAP_DEFAULT 0x0
	/*[field] ENQUEUE_VP*/
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_OFFSET  0
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_LEN     8
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_DEFAULT 0x0
	/*[field] ENQUEUE_VALID*/
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_OFFSET  8
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_LEN     1
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_DEFAULT 0x0

struct port_vsi_enqueue_map {
	a_uint32_t  enqueue_vp:8;
	a_uint32_t  enqueue_valid:1;
	a_uint32_t  _reserved0:23;
};

union port_vsi_enqueue_map_u {
	a_uint32_t val;
	struct port_vsi_enqueue_map bf;
};

/*[register] FLUSH_CFG*/
#define FLUSH_CFG
#define FLUSH_CFG_ADDRESS 0x0
#define FLUSH_CFG_NUM     1
#define FLUSH_CFG_INC     0x4
#define FLUSH_CFG_TYPE    REG_TYPE_RW
#define FLUSH_CFG_DEFAULT 0x80000
	/*[field] FLUSH_QID*/
	#define FLUSH_CFG_FLUSH_QID
	#define FLUSH_CFG_FLUSH_QID_OFFSET  0
	#define FLUSH_CFG_FLUSH_QID_LEN     9
	#define FLUSH_CFG_FLUSH_QID_DEFAULT 0x0
	/*[field] FLUSH_STATUS*/
	#define FLUSH_CFG_FLUSH_STATUS
	#define FLUSH_CFG_FLUSH_STATUS_OFFSET  10
	#define FLUSH_CFG_FLUSH_STATUS_LEN     1
	#define FLUSH_CFG_FLUSH_STATUS_DEFAULT 0x0
	/*[field] FLUSH_WT_TIME*/
	#define FLUSH_CFG_FLUSH_WT_TIME
	#define FLUSH_CFG_FLUSH_WT_TIME_OFFSET  11
	#define FLUSH_CFG_FLUSH_WT_TIME_LEN     10
	#define FLUSH_CFG_FLUSH_WT_TIME_DEFAULT 0x100
	/*[field] FLUSH_DST_PORT*/
	#define FLUSH_CFG_FLUSH_DST_PORT
	#define FLUSH_CFG_FLUSH_DST_PORT_OFFSET  21
	#define FLUSH_CFG_FLUSH_DST_PORT_LEN     4
	#define FLUSH_CFG_FLUSH_DST_PORT_DEFAULT 0x0
	/*[field] FLUSH_ALL_QUEUES*/
	#define FLUSH_CFG_FLUSH_ALL_QUEUES
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_OFFSET  25
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_LEN     1
	#define FLUSH_CFG_FLUSH_ALL_QUEUES_DEFAULT 0x0
	/*[field] FLUSH_BUSY*/
	#define FLUSH_CFG_FLUSH_BUSY
	#define FLUSH_CFG_FLUSH_BUSY_OFFSET  31
	#define FLUSH_CFG_FLUSH_BUSY_LEN     1
	#define FLUSH_CFG_FLUSH_BUSY_DEFAULT 0x0

struct flush_cfg {
	a_uint32_t  flush_qid:9;
	a_uint32_t  _reserved0:1;
	a_uint32_t  flush_status:1;
	a_uint32_t  flush_wt_time:10;
	a_uint32_t  flush_dst_port:4;
	a_uint32_t  flush_all_queues:1;
	a_uint32_t  _reserved1:5;
	a_uint32_t  flush_busy:1;
};

union flush_cfg_u {
	a_uint32_t val;
	struct flush_cfg bf;
};

/*[table] AC_UNI_QUEUE_CFG_TBL*/
#define AC_UNI_QUEUE_CFG_TBL
#define AC_UNI_QUEUE_CFG_TBL_ADDRESS 0x48000
#define AC_UNI_QUEUE_CFG_TBL_NUM     256
#define AC_UNI_QUEUE_CFG_TBL_INC     0x20
#define AC_UNI_QUEUE_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_WRED_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_OFFSET  1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_WRED_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  2
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  3
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_CFG_GRP_ID*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_OFFSET  4
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_LEN     2
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRP_ID_DEFAULT 0x0
	/*[field] AC_CFG_PRE_ALLOC_LIMIT*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_OFFSET  6
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_DYNAMIC*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_OFFSET  18
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_LEN     1
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_DYNAMIC_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_WEIGHT*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_OFFSET  19
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_LEN     3
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_WEIGHT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_CEILING*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET  22
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_GRN_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_OFFSET  34
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_GRN_MIN_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL_MAX*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_OFFSET  46
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MAX_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET  58
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED_MAX*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_OFFSET  70
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MAX_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED_MIN*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_OFFSET  82
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_MIN_DEFAULT 0x0
	/*[field] AC_CFG_RED_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET  94
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_YEL_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_OFFSET  106
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_GRN_RESUME_OFFSET*/
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET  118
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_LEN     12
	#define AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_DEFAULT 0x0

struct ac_uni_queue_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_wred_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_cfg_grp_id:2;
	a_uint32_t  ac_cfg_pre_alloc_limit:12;
	a_uint32_t  ac_cfg_shared_dynamic:1;
	a_uint32_t  ac_cfg_shared_weight:3;
	a_uint32_t  ac_cfg_shared_ceiling_0:10;
	a_uint32_t  ac_cfg_shared_ceiling_1:2;
	a_uint32_t  ac_cfg_gap_grn_grn_min:12;
	a_uint32_t  ac_cfg_gap_grn_yel_max:12;
	a_uint32_t  ac_cfg_gap_grn_yel_min_0:6;
	a_uint32_t  ac_cfg_gap_grn_yel_min_1:6;
	a_uint32_t  ac_cfg_gap_grn_red_max:12;
	a_uint32_t  ac_cfg_gap_grn_red_min:12;
	a_uint32_t  ac_cfg_red_resume_offset_0:2;
	a_uint32_t  ac_cfg_red_resume_offset_1:10;
	a_uint32_t  ac_cfg_yel_resume_offset:12;
	a_uint32_t  ac_cfg_grn_resume_offset_0:10;
	a_uint32_t  ac_cfg_grn_resume_offset_1:2;
	a_uint32_t  _reserved0:30;
};

union ac_uni_queue_cfg_tbl_u {
	a_uint32_t val[5];
	struct ac_uni_queue_cfg_tbl bf;
};

/*[table] AC_MUL_QUEUE_CFG_TBL*/
#define AC_MUL_QUEUE_CFG_TBL
#define AC_MUL_QUEUE_CFG_TBL_ADDRESS 0x50000
#define AC_MUL_QUEUE_CFG_TBL_NUM     44
#define AC_MUL_QUEUE_CFG_TBL_INC     0x10
#define AC_MUL_QUEUE_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  2
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_CFG_GRP_ID*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_OFFSET  3
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_LEN     2
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRP_ID_DEFAULT 0x0
	/*[field] AC_CFG_PRE_ALLOC_LIMIT*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_OFFSET  5
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_PRE_ALLOC_LIMIT_DEFAULT 0x0
	/*[field] AC_CFG_SHARED_CEILING*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET  17
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_YEL*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET  29
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_DEFAULT 0x0
	/*[field] AC_CFG_GAP_GRN_RED*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_OFFSET  41
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_RED_DEFAULT 0x0
	/*[field] AC_CFG_RED_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET  53
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_YEL_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_OFFSET  65
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_CFG_GRN_RESUME_OFFSET*/
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET  77
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_LEN     12
	#define AC_MUL_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_DEFAULT 0x0

struct ac_mul_queue_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_cfg_grp_id:2;
	a_uint32_t  ac_cfg_pre_alloc_limit:12;
	a_uint32_t  ac_cfg_shared_ceiling:12;
	a_uint32_t  ac_cfg_gap_grn_yel_0:3;
	a_uint32_t  ac_cfg_gap_grn_yel_1:9;
	a_uint32_t  ac_cfg_gap_grn_red:12;
	a_uint32_t  ac_cfg_red_resume_offset_0:11;
	a_uint32_t  ac_cfg_red_resume_offset_1:1;
	a_uint32_t  ac_cfg_yel_resume_offset:12;
	a_uint32_t  ac_cfg_grn_resume_offset:12;
	a_uint32_t  _reserved0:7;
};

union ac_mul_queue_cfg_tbl_u {
	a_uint32_t val[3];
	struct ac_mul_queue_cfg_tbl bf;
};

/*[table] AC_GRP_CFG_TBL*/
#define AC_GRP_CFG_TBL
#define AC_GRP_CFG_TBL_ADDRESS 0x51000
#define AC_GRP_CFG_TBL_NUM     4
#define AC_GRP_CFG_TBL_INC     0x10
#define AC_GRP_CFG_TBL_TYPE    REG_TYPE_RW
#define AC_GRP_CFG_TBL_DEFAULT 0x0
	/*[field] AC_CFG_AC_EN*/
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_OFFSET  0
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_FORCE_AC_EN*/
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_OFFSET  1
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_FORCE_AC_EN_DEFAULT 0x0
	/*[field] AC_CFG_COLOR_AWARE*/
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_OFFSET  2
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_LEN     1
	#define AC_GRP_CFG_TBL_AC_CFG_COLOR_AWARE_DEFAULT 0x0
	/*[field] AC_GRP_GAP_GRN_RED*/
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_OFFSET  3
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_RED_DEFAULT 0x0
	/*[field] AC_GRP_GAP_GRN_YEL*/
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_OFFSET  15
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GAP_GRN_YEL_DEFAULT 0x0
	/*[field] AC_GRP_DP_THRD*/
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET  27
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_DP_THRD_DEFAULT 0x0
	/*[field] AC_GRP_LIMIT*/
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_OFFSET  39
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_LIMIT_DEFAULT 0x0
	/*[field] AC_GRP_RED_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_OFFSET  51
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_RED_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_YEL_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET  63
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_GRN_RESUME_OFFSET*/
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_OFFSET  75
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_GRN_RESUME_OFFSET_DEFAULT 0x0
	/*[field] AC_GRP_PALLOC_LIMIT*/
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET  87
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_LEN     12
	#define AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_DEFAULT 0x0

struct ac_grp_cfg_tbl {
	a_uint32_t  ac_cfg_ac_en:1;
	a_uint32_t  ac_cfg_force_ac_en:1;
	a_uint32_t  ac_cfg_color_aware:1;
	a_uint32_t  ac_grp_gap_grn_red:12;
	a_uint32_t  ac_grp_gap_grn_yel:12;
	a_uint32_t  ac_grp_dp_thrd_0:5;
	a_uint32_t  ac_grp_dp_thrd_1:7;
	a_uint32_t  ac_grp_limit:12;
	a_uint32_t  ac_grp_red_resume_offset:12;
	a_uint32_t  ac_grp_yel_resume_offset_0:1;
	a_uint32_t  ac_grp_yel_resume_offset_1:11;
	a_uint32_t  ac_grp_grn_resume_offset:12;
	a_uint32_t  ac_grp_palloc_limit_0:9;
	a_uint32_t  ac_grp_palloc_limit_1:3;
	a_uint32_t  _reserved0:29;
};

union ac_grp_cfg_tbl_u {
	a_uint32_t val[4];
	struct ac_grp_cfg_tbl bf;
};

/*[table] AC_UNI_QUEUE_CNT_TBL*/
#define AC_UNI_QUEUE_CNT_TBL
#define AC_UNI_QUEUE_CNT_TBL_ADDRESS 0x53000
#define AC_UNI_QUEUE_CNT_TBL_NUM     256
#define AC_UNI_QUEUE_CNT_TBL_INC     0x10
#define AC_UNI_QUEUE_CNT_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_CNT_TBL_DEFAULT 0x0
	/*[field] AC_UNI_QUEUE_CNT*/
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_OFFSET  0
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_LEN     13
	#define AC_UNI_QUEUE_CNT_TBL_AC_UNI_QUEUE_CNT_DEFAULT 0x0

struct ac_uni_queue_cnt_tbl {
	a_uint32_t  ac_uni_queue_cnt:13;
	a_uint32_t  _reserved0:19;
};

union ac_uni_queue_cnt_tbl_u {
	a_uint32_t val;
	struct ac_uni_queue_cnt_tbl bf;
};

/*[table] AC_MUL_QUEUE_CNT_TBL*/
#define AC_MUL_QUEUE_CNT_TBL
#define AC_MUL_QUEUE_CNT_TBL_ADDRESS 0x54000
#define AC_MUL_QUEUE_CNT_TBL_NUM     44
#define AC_MUL_QUEUE_CNT_TBL_INC     0x10
#define AC_MUL_QUEUE_CNT_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_CNT_TBL_DEFAULT 0x0
	/*[field] AC_MUL_QUEUE_CNT*/
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_OFFSET  0
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_LEN     14
	#define AC_MUL_QUEUE_CNT_TBL_AC_MUL_QUEUE_CNT_DEFAULT 0x0

struct ac_mul_queue_cnt_tbl {
	a_uint32_t  ac_mul_queue_cnt:14;
	a_uint32_t  _reserved0:18;
};

union ac_mul_queue_cnt_tbl_u {
	a_uint32_t val;
	struct ac_mul_queue_cnt_tbl bf;
};

/*[table] AC_GRP_CNT_TBL*/
#define AC_GRP_CNT_TBL
#define AC_GRP_CNT_TBL_ADDRESS 0x55000
#define AC_GRP_CNT_TBL_NUM     4
#define AC_GRP_CNT_TBL_INC     0x10
#define AC_GRP_CNT_TBL_TYPE    REG_TYPE_RW
#define AC_GRP_CNT_TBL_DEFAULT 0x0
	/*[field] AC_GRP_CNT*/
	#define AC_GRP_CNT_TBL_AC_GRP_CNT
	#define AC_GRP_CNT_TBL_AC_GRP_CNT_OFFSET  0
	#define AC_GRP_CNT_TBL_AC_GRP_CNT_LEN     17
	#define AC_GRP_CNT_TBL_AC_GRP_CNT_DEFAULT 0x0
	/*[field] AC_GRP_ALLOC_USED*/
	#define AC_GRP_CNT_TBL_AC_GRP_ALLOC_USED
	#define AC_GRP_CNT_TBL_AC_GRP_ALLOC_USED_OFFSET  17
	#define AC_GRP_CNT_TBL_AC_GRP_ALLOC_USED_LEN     17
	#define AC_GRP_CNT_TBL_AC_GRP_ALLOC_USED_DEFAULT 0x0

struct ac_grp_cnt_tbl {
	a_uint32_t  ac_grp_cnt:17;
	a_uint32_t  ac_grp_alloc_used_0:15;
	a_uint32_t  ac_grp_alloc_used_1:2;
	a_uint32_t  _reserved0:30;
};

union ac_grp_cnt_tbl_u {
	a_uint32_t val[2];
	struct ac_grp_cnt_tbl bf;
};

/*[table] AC_UNI_QUEUE_DROP_STATE_TBL*/
#define AC_UNI_QUEUE_DROP_STATE_TBL
#define AC_UNI_QUEUE_DROP_STATE_TBL_ADDRESS 0x56000
#define AC_UNI_QUEUE_DROP_STATE_TBL_NUM     256
#define AC_UNI_QUEUE_DROP_STATE_TBL_INC     0x10
#define AC_UNI_QUEUE_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_UNI_QUEUE_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  40
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  44
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_LEN     4
	#define AC_UNI_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_uni_queue_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:4;
	a_uint32_t  yel_drop_state:4;
	a_uint32_t  grn_drop_state:4;
	a_uint32_t  _reserved0:16;
};

union ac_uni_queue_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_uni_queue_drop_state_tbl bf;
};

/*[table] AC_MUL_QUEUE_DROP_STATE_TBL*/
#define AC_MUL_QUEUE_DROP_STATE_TBL
#define AC_MUL_QUEUE_DROP_STATE_TBL_ADDRESS 0x58000
#define AC_MUL_QUEUE_DROP_STATE_TBL_NUM     44
#define AC_MUL_QUEUE_DROP_STATE_TBL_INC     0x10
#define AC_MUL_QUEUE_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_MUL_QUEUE_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  37
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  38
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_LEN     1
	#define AC_MUL_QUEUE_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_mul_queue_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:1;
	a_uint32_t  yel_drop_state:1;
	a_uint32_t  grn_drop_state:1;
	a_uint32_t  _reserved0:25;
};

union ac_mul_queue_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_mul_queue_drop_state_tbl bf;
};

/*[table] AC_GRP_DROP_STATE_TBL*/
#define AC_GRP_DROP_STATE_TBL
#define AC_GRP_DROP_STATE_TBL_ADDRESS 0x5a000
#define AC_GRP_DROP_STATE_TBL_NUM     4
#define AC_GRP_DROP_STATE_TBL_INC     0x10
#define AC_GRP_DROP_STATE_TBL_TYPE    REG_TYPE_RW
#define AC_GRP_DROP_STATE_TBL_DEFAULT 0x0
	/*[field] RED_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_OFFSET  0
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_RED_RESUME_THRD_DEFAULT 0x0
	/*[field] YEL_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_OFFSET  12
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_YEL_RESUME_THRD_DEFAULT 0x0
	/*[field] GRN_RESUME_THRD*/
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_OFFSET  24
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_LEN     12
	#define AC_GRP_DROP_STATE_TBL_GRN_RESUME_THRD_DEFAULT 0x0
	/*[field] RED_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_OFFSET  36
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_RED_DROP_STATE_DEFAULT 0x0
	/*[field] YEL_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_OFFSET  37
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_YEL_DROP_STATE_DEFAULT 0x0
	/*[field] GRN_DROP_STATE*/
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_OFFSET  38
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_LEN     1
	#define AC_GRP_DROP_STATE_TBL_GRN_DROP_STATE_DEFAULT 0x0

struct ac_grp_drop_state_tbl {
	a_uint32_t  red_resume_thrd:12;
	a_uint32_t  yel_resume_thrd:12;
	a_uint32_t  grn_resume_thrd_0:8;
	a_uint32_t  grn_resume_thrd_1:4;
	a_uint32_t  red_drop_state:1;
	a_uint32_t  yel_drop_state:1;
	a_uint32_t  grn_drop_state:1;
	a_uint32_t  _reserved0:25;
};

union ac_grp_drop_state_tbl_u {
	a_uint32_t val[2];
	struct ac_grp_drop_state_tbl bf;
};

/*[table] OQ_DEQ_OPR_TBL*/
#define OQ_DEQ_OPR_TBL
#define OQ_DEQ_OPR_TBL_ADDRESS 0x64000
#define OQ_DEQ_OPR_TBL_NUM     300
#define OQ_DEQ_OPR_TBL_INC     0x10
#define OQ_DEQ_OPR_TBL_TYPE    REG_TYPE_RW
#define OQ_DEQ_OPR_TBL_DEFAULT 0x0
	/*[field] DEQ_DROP*/
	#define OQ_DEQ_OPR_TBL_DEQ_DROP
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_OFFSET  0
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_LEN     1
	#define OQ_DEQ_OPR_TBL_DEQ_DROP_DEFAULT 0x0
	/*[field] DEQ_HIGH_PRI*/
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_OFFSET  1
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_LEN     1
	#define OQ_DEQ_OPR_TBL_DEQ_HIGH_PRI_DEFAULT 0x0

struct oq_deq_opr_tbl {
	a_uint32_t  deq_drop:1;
	a_uint32_t  deq_high_pri:1;
	a_uint32_t  _reserved0:30;
};

union oq_deq_opr_tbl_u {
	a_uint32_t val;
	struct oq_deq_opr_tbl bf;
};

/*[table] OQ_HEAD_UNI_TBL*/
#define OQ_HEAD_UNI_TBL
#define OQ_HEAD_UNI_TBL_ADDRESS 0x6c000
#define OQ_HEAD_UNI_TBL_NUM     256
#define OQ_HEAD_UNI_TBL_INC     0x10
#define OQ_HEAD_UNI_TBL_TYPE    REG_TYPE_RW
#define OQ_HEAD_UNI_TBL_DEFAULT 0x0
	/*[field] EMPTY*/
	#define OQ_HEAD_UNI_TBL_EMPTY
	#define OQ_HEAD_UNI_TBL_EMPTY_OFFSET  0
	#define OQ_HEAD_UNI_TBL_EMPTY_LEN     1
	#define OQ_HEAD_UNI_TBL_EMPTY_DEFAULT 0x0
	/*[field] TAIL*/
	#define OQ_HEAD_UNI_TBL_TAIL
	#define OQ_HEAD_UNI_TBL_TAIL_OFFSET  1
	#define OQ_HEAD_UNI_TBL_TAIL_LEN     12
	#define OQ_HEAD_UNI_TBL_TAIL_DEFAULT 0x0
	/*[field] HEAD*/
	#define OQ_HEAD_UNI_TBL_HEAD
	#define OQ_HEAD_UNI_TBL_HEAD_OFFSET  13
	#define OQ_HEAD_UNI_TBL_HEAD_LEN     12
	#define OQ_HEAD_UNI_TBL_HEAD_DEFAULT 0x0
	/*[field] HEAD_PKT_LEN*/
	#define OQ_HEAD_UNI_TBL_HEAD_PKT_LEN
	#define OQ_HEAD_UNI_TBL_HEAD_PKT_LEN_OFFSET  25
	#define OQ_HEAD_UNI_TBL_HEAD_PKT_LEN_LEN     14
	#define OQ_HEAD_UNI_TBL_HEAD_PKT_LEN_DEFAULT 0x0

struct oq_head_uni_tbl {
	a_uint32_t  empty:1;
	a_uint32_t  tail:12;
	a_uint32_t  head:12;
	a_uint32_t  head_pkt_len_0:7;
	a_uint32_t  head_pkt_len_1:7;
	a_uint32_t  _reserved0:25;
};

union oq_head_uni_tbl_u {
	a_uint32_t val[2];
	struct oq_head_uni_tbl bf;
};

/*[table] OQ_HEAD_MUL_TBL*/
#define OQ_HEAD_MUL_TBL
#define OQ_HEAD_MUL_TBL_ADDRESS 0x74000
#define OQ_HEAD_MUL_TBL_NUM     44
#define OQ_HEAD_MUL_TBL_INC     0x10
#define OQ_HEAD_MUL_TBL_TYPE    REG_TYPE_RW
#define OQ_HEAD_MUL_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_HEAD_MUL_TBL_NORMAL_FWD
	#define OQ_HEAD_MUL_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_HEAD_MUL_TBL_NORMAL_FWD_LEN     1
	#define OQ_HEAD_MUL_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_HEAD_MUL_TBL_EGRESS_MIRR
	#define OQ_HEAD_MUL_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_HEAD_MUL_TBL_EGRESS_MIRR_LEN     1
	#define OQ_HEAD_MUL_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_HEAD_MUL_TBL_INGRESS_MIRR
	#define OQ_HEAD_MUL_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_HEAD_MUL_TBL_INGRESS_MIRR_LEN     1
	#define OQ_HEAD_MUL_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] EMPTY*/
	#define OQ_HEAD_MUL_TBL_EMPTY
	#define OQ_HEAD_MUL_TBL_EMPTY_OFFSET  3
	#define OQ_HEAD_MUL_TBL_EMPTY_LEN     1
	#define OQ_HEAD_MUL_TBL_EMPTY_DEFAULT 0x0
	/*[field] TAIL*/
	#define OQ_HEAD_MUL_TBL_TAIL
	#define OQ_HEAD_MUL_TBL_TAIL_OFFSET  4
	#define OQ_HEAD_MUL_TBL_TAIL_LEN     12
	#define OQ_HEAD_MUL_TBL_TAIL_DEFAULT 0x0
	/*[field] HEAD*/
	#define OQ_HEAD_MUL_TBL_HEAD
	#define OQ_HEAD_MUL_TBL_HEAD_OFFSET  16
	#define OQ_HEAD_MUL_TBL_HEAD_LEN     12
	#define OQ_HEAD_MUL_TBL_HEAD_DEFAULT 0x0
	/*[field] HEAD_PKT_LEN*/
	#define OQ_HEAD_MUL_TBL_HEAD_PKT_LEN
	#define OQ_HEAD_MUL_TBL_HEAD_PKT_LEN_OFFSET  28
	#define OQ_HEAD_MUL_TBL_HEAD_PKT_LEN_LEN     14
	#define OQ_HEAD_MUL_TBL_HEAD_PKT_LEN_DEFAULT 0x0

struct oq_head_mul_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  empty:1;
	a_uint32_t  tail:12;
	a_uint32_t  head:12;
	a_uint32_t  head_pkt_len_0:4;
	a_uint32_t  head_pkt_len_1:10;
	a_uint32_t  _reserved0:22;
};

union oq_head_mul_tbl_u {
	a_uint32_t val[2];
	struct oq_head_mul_tbl bf;
};

/*[table] OQ_LL_UNI_TBL*/
#define OQ_LL_UNI_TBL
#define OQ_LL_UNI_TBL_ADDRESS 0x90000
#define OQ_LL_UNI_TBL_NUM     3072
#define OQ_LL_UNI_TBL_INC     0x10
#define OQ_LL_UNI_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_UNI_TBL_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_UNI_TBL_NEXT_POINTER
	#define OQ_LL_UNI_TBL_NEXT_POINTER_OFFSET  0
	#define OQ_LL_UNI_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_UNI_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_uni_tbl {
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:20;
};

union oq_ll_uni_tbl_u {
	a_uint32_t val;
	struct oq_ll_uni_tbl bf;
};

/*[table] OQ_LL_MUL_P0_TBL*/
#define OQ_LL_MUL_P0_TBL
#define OQ_LL_MUL_P0_TBL_ADDRESS 0xb0000
#define OQ_LL_MUL_P0_TBL_NUM     3072
#define OQ_LL_MUL_P0_TBL_INC     0x10
#define OQ_LL_MUL_P0_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P0_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P0_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P0_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P0_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P0_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P0_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P0_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P0_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P0_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P0_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P0_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P0_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P0_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P0_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P0_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P0_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P0_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p0_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p0_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p0_tbl bf;
};

/*[table] OQ_LL_MUL_P1_TBL*/
#define OQ_LL_MUL_P1_TBL
#define OQ_LL_MUL_P1_TBL_ADDRESS 0xd0000
#define OQ_LL_MUL_P1_TBL_NUM     3072
#define OQ_LL_MUL_P1_TBL_INC     0x10
#define OQ_LL_MUL_P1_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P1_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P1_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P1_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P1_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P1_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P1_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P1_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P1_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P1_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P1_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P1_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P1_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P1_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P1_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P1_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P1_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P1_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p1_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p1_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p1_tbl bf;
};

/*[table] OQ_LL_MUL_P2_TBL*/
#define OQ_LL_MUL_P2_TBL
#define OQ_LL_MUL_P2_TBL_ADDRESS 0x110000
#define OQ_LL_MUL_P2_TBL_NUM     3072
#define OQ_LL_MUL_P2_TBL_INC     0x10
#define OQ_LL_MUL_P2_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P2_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P2_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P2_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P2_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P2_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P2_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P2_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P2_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P2_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P2_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P2_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P2_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P2_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P2_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P2_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P2_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P2_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p2_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p2_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p2_tbl bf;
};

/*[table] OQ_LL_MUL_P3_TBL*/
#define OQ_LL_MUL_P3_TBL
#define OQ_LL_MUL_P3_TBL_ADDRESS 0x130000
#define OQ_LL_MUL_P3_TBL_NUM     3072
#define OQ_LL_MUL_P3_TBL_INC     0x10
#define OQ_LL_MUL_P3_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P3_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P3_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P3_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P3_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P3_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P3_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P3_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P3_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P3_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P3_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P3_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P3_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P3_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P3_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P3_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P3_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P3_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p3_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p3_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p3_tbl bf;
};

/*[table] OQ_LL_MUL_P4_TBL*/
#define OQ_LL_MUL_P4_TBL
#define OQ_LL_MUL_P4_TBL_ADDRESS 0x150000
#define OQ_LL_MUL_P4_TBL_NUM     3072
#define OQ_LL_MUL_P4_TBL_INC     0x10
#define OQ_LL_MUL_P4_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P4_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P4_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P4_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P4_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P4_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P4_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P4_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P4_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P4_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P4_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P4_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P4_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P4_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P4_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P4_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P4_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P4_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p4_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p4_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p4_tbl bf;
};

/*[table] OQ_LL_MUL_P5_TBL*/
#define OQ_LL_MUL_P5_TBL
#define OQ_LL_MUL_P5_TBL_ADDRESS 0x170000
#define OQ_LL_MUL_P5_TBL_NUM     3072
#define OQ_LL_MUL_P5_TBL_INC     0x10
#define OQ_LL_MUL_P5_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P5_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P5_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P5_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P5_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P5_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P5_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P5_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P5_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P5_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P5_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P5_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P5_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P5_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P5_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P5_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P5_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P5_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p5_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p5_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p5_tbl bf;
};

/*[table] QUEUE_TX_COUNTER_TBL*/
#define QUEUE_TX_COUNTER_TBL
#define QUEUE_TX_COUNTER_TBL_ADDRESS 0x1a000
#define QUEUE_TX_COUNTER_TBL_NUM     300
#define QUEUE_TX_COUNTER_TBL_INC     0x10
#define QUEUE_TX_COUNTER_TBL_TYPE    REG_TYPE_RW
#define QUEUE_TX_COUNTER_TBL_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_OFFSET  0
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_LEN     32
	#define QUEUE_TX_COUNTER_TBL_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_OFFSET  32
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_LEN     40
	#define QUEUE_TX_COUNTER_TBL_TX_BYTES_DEFAULT 0x0

struct queue_tx_counter_tbl {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union queue_tx_counter_tbl_u {
	a_uint32_t val[3];
	struct queue_tx_counter_tbl bf;
};

/*[table] MUL_P5_DROP_CNT_TBL*/
#define MUL_P5_DROP_CNT_TBL
#define MUL_P5_DROP_CNT_TBL_ADDRESS 0x1f5000
#define MUL_P5_DROP_CNT_TBL_NUM     12
#define MUL_P5_DROP_CNT_TBL_INC     0x10
#define MUL_P5_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P5_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P5_DROP_PKT*/
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_OFFSET  0
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_LEN     32
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P5_DROP_BYTE*/
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_OFFSET  32
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_LEN     40
	#define MUL_P5_DROP_CNT_TBL_MUL_P5_DROP_BYTE_DEFAULT 0x0

struct mul_p5_drop_cnt_tbl {
	a_uint32_t  mul_p5_drop_pkt:32;
	a_uint32_t  mul_p5_drop_byte_0:32;
	a_uint32_t  mul_p5_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p5_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p5_drop_cnt_tbl bf;
};

/*[table] UNI_DROP_CNT_TBL*/
#define UNI_DROP_CNT_TBL
#define UNI_DROP_CNT_TBL_ADDRESS 0x1e0000
#define UNI_DROP_CNT_TBL_NUM     1536
#define UNI_DROP_CNT_TBL_INC     0x10
#define UNI_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define UNI_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] UNI_DROP_PKT*/
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_OFFSET  0
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_LEN     32
	#define UNI_DROP_CNT_TBL_UNI_DROP_PKT_DEFAULT 0x0
	/*[field] UNI_DROP_BYTE*/
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_OFFSET  32
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_LEN     40
	#define UNI_DROP_CNT_TBL_UNI_DROP_BYTE_DEFAULT 0x0

struct uni_drop_cnt_tbl {
	a_uint32_t  uni_drop_pkt:32;
	a_uint32_t  uni_drop_byte_0:32;
	a_uint32_t  uni_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union uni_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct uni_drop_cnt_tbl bf;
};

#endif
