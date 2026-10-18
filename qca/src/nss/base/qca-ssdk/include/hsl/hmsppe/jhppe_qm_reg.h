/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_QM_REG_H_
#define _JHPPE_QM_REG_H_

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

/*[register] ENQ_CTRL_EXT*/
#define ENQ_CTRL_EXT
#define ENQ_CTRL_EXT_ADDRESS 0x8c
#define ENQ_CTRL_EXT_NUM     1
#define ENQ_CTRL_EXT_INC     0x4
#define ENQ_CTRL_EXT_TYPE    REG_TYPE_RW
#define ENQ_CTRL_EXT_DEFAULT 0xf9fafc00
	/*[field] CPU_CODE_EN*/
	#define ENQ_CTRL_EXT_CPU_CODE_EN
	#define ENQ_CTRL_EXT_CPU_CODE_EN_OFFSET  0
	#define ENQ_CTRL_EXT_CPU_CODE_EN_LEN     4
	#define ENQ_CTRL_EXT_CPU_CODE_EN_DEFAULT 0x0
	/*[field] RSV*/
	#define ENQ_CTRL_EXT_RSV
	#define ENQ_CTRL_EXT_RSV_OFFSET  4
	#define ENQ_CTRL_EXT_RSV_LEN     4
	#define ENQ_CTRL_EXT_RSV_DEFAULT 0x0
	/*[field] DROP_CPU_CODE*/
	#define ENQ_CTRL_EXT_DROP_CPU_CODE
	#define ENQ_CTRL_EXT_DROP_CPU_CODE_OFFSET  8
	#define ENQ_CTRL_EXT_DROP_CPU_CODE_LEN     8
	#define ENQ_CTRL_EXT_DROP_CPU_CODE_DEFAULT 0xfc
	/*[field] CPU_CODE_0*/
	#define ENQ_CTRL_EXT_CPU_CODE_0
	#define ENQ_CTRL_EXT_CPU_CODE_0_OFFSET  16
	#define ENQ_CTRL_EXT_CPU_CODE_0_LEN     8
	#define ENQ_CTRL_EXT_CPU_CODE_0_DEFAULT 0xfa
	/*[field] CPU_CODE_1*/
	#define ENQ_CTRL_EXT_CPU_CODE_1
	#define ENQ_CTRL_EXT_CPU_CODE_1_OFFSET  24
	#define ENQ_CTRL_EXT_CPU_CODE_1_LEN     8
	#define ENQ_CTRL_EXT_CPU_CODE_1_DEFAULT 0xf9

struct enq_ctrl_ext {
	a_uint32_t  cpu_code_en:4;
	a_uint32_t  rsv:4;
	a_uint32_t  drop_cpu_code:8;
	a_uint32_t  cpu_code_0:8;
	a_uint32_t  cpu_code_1:8;
};

union enq_ctrl_ext_u {
	a_uint32_t val;
	struct enq_ctrl_ext bf;
};

/*[register] UNIQ_FLOWCTRL_STATUS*/
#define UNIQ_FLOWCTRL_STATUS
#define UNIQ_FLOWCTRL_STATUS_ADDRESS 0x40
#define UNIQ_FLOWCTRL_STATUS_NUM     8
#define UNIQ_FLOWCTRL_STATUS_INC     0x4
#define UNIQ_FLOWCTRL_STATUS_TYPE    REG_TYPE_RO
#define UNIQ_FLOWCTRL_STATUS_DEFAULT 0x0
	/*[field] UNIQ_FLOWCTRL_STATUS_STATUS*/
	#define UNIQ_FLOWCTRL_STATUS_UNIQ_FLOWCTRL_STATUS_STATUS
	#define UNIQ_FLOWCTRL_STATUS_UNIQ_FLOWCTRL_STATUS_STATUS_OFFSET  0
	#define UNIQ_FLOWCTRL_STATUS_UNIQ_FLOWCTRL_STATUS_STATUS_LEN     32
	#define UNIQ_FLOWCTRL_STATUS_UNIQ_FLOWCTRL_STATUS_STATUS_DEFAULT 0x0

struct uniq_flowctrl_status {
	a_uint32_t  uniq_flowctrl_status_status:32;
};

union uniq_flowctrl_status_u {
	a_uint32_t val;
	struct uniq_flowctrl_status bf;
};

/*[table] UNIQ_Q_MAP_TBL*/
#define UNIQ_Q_MAP_TBL
#define UNIQ_Q_MAP_TBL_ADDRESS 0x38000
#define UNIQ_Q_MAP_TBL_NUM     300
#define UNIQ_Q_MAP_TBL_INC     0x40
#define UNIQ_Q_MAP_TBL_TYPE    REG_TYPE_RW
#define UNIQ_Q_MAP_TBL_DEFAULT 0x0
	/*[field] QUEUE_BITMAP*/
	#define UNIQ_Q_MAP_TBL_QUEUE_BITMAP
	#define UNIQ_Q_MAP_TBL_QUEUE_BITMAP_OFFSET  0
	#define UNIQ_Q_MAP_TBL_QUEUE_BITMAP_LEN     300
	#define UNIQ_Q_MAP_TBL_QUEUE_BITMAP_DEFAULT 0x0

struct uniq_q_map_tbl {
	a_uint32_t  queue_bitmap_0:32;
	a_uint32_t  queue_bitmap_1:32;
	a_uint32_t  queue_bitmap_2:32;
	a_uint32_t  queue_bitmap_3:32;
	a_uint32_t  queue_bitmap_4:32;
	a_uint32_t  queue_bitmap_5:32;
	a_uint32_t  queue_bitmap_6:32;
	a_uint32_t  queue_bitmap_7:32;
	a_uint32_t  queue_bitmap_8:32;
	a_uint32_t  queue_bitmap_9:12;
	a_uint32_t  _reserved0:20;
};

union uniq_q_map_tbl_u {
	a_uint32_t val[10];
	struct uniq_q_map_tbl bf;
};

/*[register] ENQ_CTRL*/
#define ENQ_CTRL
#define ENQ_CTRL_ADDRESS 0x90
#define ENQ_CTRL_NUM     1
#define ENQ_CTRL_INC     0x4
#define ENQ_CTRL_TYPE    REG_TYPE_RW
#define ENQ_CTRL_DEFAULT 0x3f37fb01
	/*[field] UC_ENQ_EN*/
	#define ENQ_CTRL_UC_ENQ_EN
	#define ENQ_CTRL_UC_ENQ_EN_OFFSET  0
	#define ENQ_CTRL_UC_ENQ_EN_LEN     1
	#define ENQ_CTRL_UC_ENQ_EN_DEFAULT 0x1
	/*[field] QUEUE_MAP_RULE_EN*/
	#define ENQ_CTRL_QUEUE_MAP_RULE_EN
	#define ENQ_CTRL_QUEUE_MAP_RULE_EN_OFFSET  1
	#define ENQ_CTRL_QUEUE_MAP_RULE_EN_LEN     1
	#define ENQ_CTRL_QUEUE_MAP_RULE_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH0_SRC_PROFILE_EG_EN*/
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EG_EN
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EG_EN_OFFSET  4
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EG_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EG_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH1_SRC_PROFILE_EG_EN*/
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EG_EN
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EG_EN_OFFSET  5
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EG_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EG_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH2_SRC_PROFILE_EG_EN*/
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EG_EN
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EG_EN_OFFSET  6
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EG_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EG_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH3_SRC_PROFILE_EG_EN*/
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EG_EN
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EG_EN_OFFSET  7
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EG_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EG_EN_DEFAULT 0x0
	/*[field] QID_MISMATCH_ENQ_CPU_CODE*/
	#define ENQ_CTRL_QID_MISMATCH_ENQ_CPU_CODE
	#define ENQ_CTRL_QID_MISMATCH_ENQ_CPU_CODE_OFFSET  8
	#define ENQ_CTRL_QID_MISMATCH_ENQ_CPU_CODE_LEN     8
	#define ENQ_CTRL_QID_MISMATCH_ENQ_CPU_CODE_DEFAULT 0xfb
	/*[field] PASS_THROUGH0_DIREC_ENQ_DIS*/
	#define ENQ_CTRL_PASS_THROUGH0_DIREC_ENQ_DIS
	#define ENQ_CTRL_PASS_THROUGH0_DIREC_ENQ_DIS_OFFSET  16
	#define ENQ_CTRL_PASS_THROUGH0_DIREC_ENQ_DIS_LEN     1
	#define ENQ_CTRL_PASS_THROUGH0_DIREC_ENQ_DIS_DEFAULT 0x1
	/*[field] PASS_THROUGH1_DIREC_ENQ_DIS*/
	#define ENQ_CTRL_PASS_THROUGH1_DIREC_ENQ_DIS
	#define ENQ_CTRL_PASS_THROUGH1_DIREC_ENQ_DIS_OFFSET  17
	#define ENQ_CTRL_PASS_THROUGH1_DIREC_ENQ_DIS_LEN     1
	#define ENQ_CTRL_PASS_THROUGH1_DIREC_ENQ_DIS_DEFAULT 0x1
	/*[field] PASS_THROUGH2_DIREC_ENQ_DIS*/
	#define ENQ_CTRL_PASS_THROUGH2_DIREC_ENQ_DIS
	#define ENQ_CTRL_PASS_THROUGH2_DIREC_ENQ_DIS_OFFSET  18
	#define ENQ_CTRL_PASS_THROUGH2_DIREC_ENQ_DIS_LEN     1
	#define ENQ_CTRL_PASS_THROUGH2_DIREC_ENQ_DIS_DEFAULT 0x1
	/*[field] PASS_THROUGH3_DIREC_ENQ_DIS*/
	#define ENQ_CTRL_PASS_THROUGH3_DIREC_ENQ_DIS
	#define ENQ_CTRL_PASS_THROUGH3_DIREC_ENQ_DIS_OFFSET  19
	#define ENQ_CTRL_PASS_THROUGH3_DIREC_ENQ_DIS_LEN     1
	#define ENQ_CTRL_PASS_THROUGH3_DIREC_ENQ_DIS_DEFAULT 0x0
	/*[field] PASS_THROUGH0_SRC_PROFILE_EN*/
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EN
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EN_OFFSET  20
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_EN_DEFAULT 0x1
	/*[field] PASS_THROUGH1_SRC_PROFILE_EN*/
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EN
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EN_OFFSET  21
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_EN_DEFAULT 0x1
	/*[field] PASS_THROUGH2_SRC_PROFILE_EN*/
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EN
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EN_OFFSET  22
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH3_SRC_PROFILE_EN*/
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EN
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EN_OFFSET  23
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EN_LEN     1
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_EN_DEFAULT 0x0
	/*[field] PASS_THROUGH0_SRC_PROFILE*/
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_OFFSET  24
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_LEN     2
	#define ENQ_CTRL_PASS_THROUGH0_SRC_PROFILE_DEFAULT 0x3
	/*[field] PASS_THROUGH1_SRC_PROFILE*/
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_OFFSET  26
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_LEN     2
	#define ENQ_CTRL_PASS_THROUGH1_SRC_PROFILE_DEFAULT 0x3
	/*[field] PASS_THROUGH2_SRC_PROFILE*/
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_OFFSET  28
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_LEN     2
	#define ENQ_CTRL_PASS_THROUGH2_SRC_PROFILE_DEFAULT 0x3
	/*[field] PASS_THROUGH3_SRC_PROFILE*/
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_OFFSET  30
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_LEN     2
	#define ENQ_CTRL_PASS_THROUGH3_SRC_PROFILE_DEFAULT 0x0

struct enq_ctrl {
	a_uint32_t  uc_enq_en:1;
	a_uint32_t  queue_map_rule_en:1;
	a_uint32_t  _reserved0:2;
	a_uint32_t  pass_through0_src_profile_eg_en:1;
	a_uint32_t  pass_through1_src_profile_eg_en:1;
	a_uint32_t  pass_through2_src_profile_eg_en:1;
	a_uint32_t  pass_through3_src_profile_eg_en:1;
	a_uint32_t  qid_mismatch_enq_cpu_code:8;
	a_uint32_t  pass_through0_direc_enq_dis:1;
	a_uint32_t  pass_through1_direc_enq_dis:1;
	a_uint32_t  pass_through2_direc_enq_dis:1;
	a_uint32_t  pass_through3_direc_enq_dis:1;
	a_uint32_t  pass_through0_src_profile_en:1;
	a_uint32_t  pass_through1_src_profile_en:1;
	a_uint32_t  pass_through2_src_profile_en:1;
	a_uint32_t  pass_through3_src_profile_en:1;
	a_uint32_t  pass_through0_src_profile:2;
	a_uint32_t  pass_through1_src_profile:2;
	a_uint32_t  pass_through2_src_profile:2;
	a_uint32_t  pass_through3_src_profile:2;
};

union enq_ctrl_u {
	a_uint32_t val;
	struct enq_ctrl bf;
};

/*[register] CM_CTRL_0*/
#define CM_CTRL_0
#define CM_CTRL_0_ADDRESS 0x94
#define CM_CTRL_0_NUM     1
#define CM_CTRL_0_INC     0x4
#define CM_CTRL_0_TYPE    REG_TYPE_RW
#define CM_CTRL_0_DEFAULT 0x7fc00ff
	/*[field] PARK_FIFO_XOFF*/
	#define CM_CTRL_0_PARK_FIFO_XOFF
	#define CM_CTRL_0_PARK_FIFO_XOFF_OFFSET  0
	#define CM_CTRL_0_PARK_FIFO_XOFF_LEN     9
	#define CM_CTRL_0_PARK_FIFO_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_FREE_HEAD*/
	#define CM_CTRL_0_PARK_FIFO_FREE_HEAD
	#define CM_CTRL_0_PARK_FIFO_FREE_HEAD_OFFSET  9
	#define CM_CTRL_0_PARK_FIFO_FREE_HEAD_LEN     9
	#define CM_CTRL_0_PARK_FIFO_FREE_HEAD_DEFAULT 0x0
	/*[field] PARK_FIFO_FREE_TAIL*/
	#define CM_CTRL_0_PARK_FIFO_FREE_TAIL
	#define CM_CTRL_0_PARK_FIFO_FREE_TAIL_OFFSET  18
	#define CM_CTRL_0_PARK_FIFO_FREE_TAIL_LEN     9
	#define CM_CTRL_0_PARK_FIFO_FREE_TAIL_DEFAULT 0x1ff

struct cm_ctrl_0 {
	a_uint32_t  park_fifo_xoff:9;
	a_uint32_t  park_fifo_free_head:9;
	a_uint32_t  park_fifo_free_tail:9;
	a_uint32_t  _reserved0:5;
};

union cm_ctrl_0_u {
	a_uint32_t val;
	struct cm_ctrl_0 bf;
};

/*[register] CM_CTRL_1*/
#define CM_CTRL_1
#define CM_CTRL_1_ADDRESS 0x98
#define CM_CTRL_1_NUM     1
#define CM_CTRL_1_INC     0x4
#define CM_CTRL_1_TYPE    REG_TYPE_RW
#define CM_CTRL_1_DEFAULT 0x3fdfeff
	/*[field] PARK_FIFO_P0_XOFF*/
	#define CM_CTRL_1_PARK_FIFO_P0_XOFF
	#define CM_CTRL_1_PARK_FIFO_P0_XOFF_OFFSET  0
	#define CM_CTRL_1_PARK_FIFO_P0_XOFF_LEN     9
	#define CM_CTRL_1_PARK_FIFO_P0_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P1_XOFF*/
	#define CM_CTRL_1_PARK_FIFO_P1_XOFF
	#define CM_CTRL_1_PARK_FIFO_P1_XOFF_OFFSET  9
	#define CM_CTRL_1_PARK_FIFO_P1_XOFF_LEN     9
	#define CM_CTRL_1_PARK_FIFO_P1_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P2_XOFF*/
	#define CM_CTRL_1_PARK_FIFO_P2_XOFF
	#define CM_CTRL_1_PARK_FIFO_P2_XOFF_OFFSET  18
	#define CM_CTRL_1_PARK_FIFO_P2_XOFF_LEN     9
	#define CM_CTRL_1_PARK_FIFO_P2_XOFF_DEFAULT 0xff

struct cm_ctrl_1 {
	a_uint32_t  park_fifo_p0_xoff:9;
	a_uint32_t  park_fifo_p1_xoff:9;
	a_uint32_t  park_fifo_p2_xoff:9;
	a_uint32_t  _reserved0:5;
};

union cm_ctrl_1_u {
	a_uint32_t val;
	struct cm_ctrl_1 bf;
};

/*[register] CM_CTRL_2*/
#define CM_CTRL_2
#define CM_CTRL_2_ADDRESS 0x9c
#define CM_CTRL_2_NUM     1
#define CM_CTRL_2_INC     0x4
#define CM_CTRL_2_TYPE    REG_TYPE_RW
#define CM_CTRL_2_DEFAULT 0x3fdfeff
	/*[field] PARK_FIFO_P3_XOFF*/
	#define CM_CTRL_2_PARK_FIFO_P3_XOFF
	#define CM_CTRL_2_PARK_FIFO_P3_XOFF_OFFSET  0
	#define CM_CTRL_2_PARK_FIFO_P3_XOFF_LEN     9
	#define CM_CTRL_2_PARK_FIFO_P3_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P4_XOFF*/
	#define CM_CTRL_2_PARK_FIFO_P4_XOFF
	#define CM_CTRL_2_PARK_FIFO_P4_XOFF_OFFSET  9
	#define CM_CTRL_2_PARK_FIFO_P4_XOFF_LEN     9
	#define CM_CTRL_2_PARK_FIFO_P4_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P5_XOFF*/
	#define CM_CTRL_2_PARK_FIFO_P5_XOFF
	#define CM_CTRL_2_PARK_FIFO_P5_XOFF_OFFSET  18
	#define CM_CTRL_2_PARK_FIFO_P5_XOFF_LEN     9
	#define CM_CTRL_2_PARK_FIFO_P5_XOFF_DEFAULT 0xff

struct cm_ctrl_2 {
	a_uint32_t  park_fifo_p3_xoff:9;
	a_uint32_t  park_fifo_p4_xoff:9;
	a_uint32_t  park_fifo_p5_xoff:9;
	a_uint32_t  _reserved0:5;
};

union cm_ctrl_2_u {
	a_uint32_t val;
	struct cm_ctrl_2 bf;
};

/*[register] CM_CTRL_3*/
#define CM_CTRL_3
#define CM_CTRL_3_ADDRESS 0xa0
#define CM_CTRL_3_NUM     1
#define CM_CTRL_3_INC     0x4
#define CM_CTRL_3_TYPE    REG_TYPE_RW
#define CM_CTRL_3_DEFAULT 0x3fdfeff
	/*[field] PARK_FIFO_P6_XOFF*/
	#define CM_CTRL_3_PARK_FIFO_P6_XOFF
	#define CM_CTRL_3_PARK_FIFO_P6_XOFF_OFFSET  0
	#define CM_CTRL_3_PARK_FIFO_P6_XOFF_LEN     9
	#define CM_CTRL_3_PARK_FIFO_P6_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P7_XOFF*/
	#define CM_CTRL_3_PARK_FIFO_P7_XOFF
	#define CM_CTRL_3_PARK_FIFO_P7_XOFF_OFFSET  9
	#define CM_CTRL_3_PARK_FIFO_P7_XOFF_LEN     9
	#define CM_CTRL_3_PARK_FIFO_P7_XOFF_DEFAULT 0xff
	/*[field] PARK_FIFO_P8_XOFF*/
	#define CM_CTRL_3_PARK_FIFO_P8_XOFF
	#define CM_CTRL_3_PARK_FIFO_P8_XOFF_OFFSET  18
	#define CM_CTRL_3_PARK_FIFO_P8_XOFF_LEN     9
	#define CM_CTRL_3_PARK_FIFO_P8_XOFF_DEFAULT 0xff

struct cm_ctrl_3 {
	a_uint32_t  park_fifo_p6_xoff:9;
	a_uint32_t  park_fifo_p7_xoff:9;
	a_uint32_t  park_fifo_p8_xoff:9;
	a_uint32_t  _reserved0:5;
};

union cm_ctrl_3_u {
	a_uint32_t val;
	struct cm_ctrl_3 bf;
};

/*[register] CM_CTRL_4*/
#define CM_CTRL_4
#define CM_CTRL_4_ADDRESS 0xa4
#define CM_CTRL_4_NUM     1
#define CM_CTRL_4_INC     0x4
#define CM_CTRL_4_TYPE    REG_TYPE_RW
#define CM_CTRL_4_DEFAULT 0x4
	/*[field] PREFETCH_TC_BWM_XOFF*/
	#define CM_CTRL_4_PREFETCH_TC_BWM_XOFF
	#define CM_CTRL_4_PREFETCH_TC_BWM_XOFF_OFFSET  0
	#define CM_CTRL_4_PREFETCH_TC_BWM_XOFF_LEN     5
	#define CM_CTRL_4_PREFETCH_TC_BWM_XOFF_DEFAULT 0x4

struct cm_ctrl_4 {
	a_uint32_t  prefetch_tc_bwm_xoff:5;
	a_uint32_t  _reserved0:27;
};

union cm_ctrl_4_u {
	a_uint32_t val;
	struct cm_ctrl_4 bf;
};

/*[register] DDRQ_CTRL*/
#define DDRQ_CTRL
#define DDRQ_CTRL_ADDRESS 0xa8
#define DDRQ_CTRL_NUM     1
#define DDRQ_CTRL_INC     0x4
#define DDRQ_CTRL_TYPE    REG_TYPE_RW
#define DDRQ_CTRL_DEFAULT 0x1
	/*[field] DDRQ_ENSCH_EMPTY*/
	#define DDRQ_CTRL_DDRQ_ENSCH_EMPTY
	#define DDRQ_CTRL_DDRQ_ENSCH_EMPTY_OFFSET  0
	#define DDRQ_CTRL_DDRQ_ENSCH_EMPTY_LEN     1
	#define DDRQ_CTRL_DDRQ_ENSCH_EMPTY_DEFAULT 0x1
	/*[field] DDRQ_ENSCH_TAIL*/
	#define DDRQ_CTRL_DDRQ_ENSCH_TAIL
	#define DDRQ_CTRL_DDRQ_ENSCH_TAIL_OFFSET  1
	#define DDRQ_CTRL_DDRQ_ENSCH_TAIL_LEN     8
	#define DDRQ_CTRL_DDRQ_ENSCH_TAIL_DEFAULT 0x0
	/*[field] DDRQ_ENSCH_HEAD*/
	#define DDRQ_CTRL_DDRQ_ENSCH_HEAD
	#define DDRQ_CTRL_DDRQ_ENSCH_HEAD_OFFSET  9
	#define DDRQ_CTRL_DDRQ_ENSCH_HEAD_LEN     8
	#define DDRQ_CTRL_DDRQ_ENSCH_HEAD_DEFAULT 0x0

struct ddrq_ctrl {
	a_uint32_t  ddrq_ensch_empty:1;
	a_uint32_t  ddrq_ensch_tail:8;
	a_uint32_t  ddrq_ensch_head:8;
	a_uint32_t  _reserved0:15;
};

union ddrq_ctrl_u {
	a_uint32_t val;
	struct ddrq_ctrl bf;
};

/*[register] SRAM_QCNT_CTRL*/
#define SRAM_QCNT_CTRL
#define SRAM_QCNT_CTRL_ADDRESS 0xb0
#define SRAM_QCNT_CTRL_NUM     1
#define SRAM_QCNT_CTRL_INC     0x4
#define SRAM_QCNT_CTRL_TYPE    REG_TYPE_RW
#define SRAM_QCNT_CTRL_DEFAULT 0x1
	/*[field] SRAMQ_GO*/
	#define SRAM_QCNT_CTRL_SRAMQ_GO
	#define SRAM_QCNT_CTRL_SRAMQ_GO_OFFSET  0
	#define SRAM_QCNT_CTRL_SRAMQ_GO_LEN     1
	#define SRAM_QCNT_CTRL_SRAMQ_GO_DEFAULT 0x1
	/*[field] SRAMQ_CLEAR*/
	#define SRAM_QCNT_CTRL_SRAMQ_CLEAR
	#define SRAM_QCNT_CTRL_SRAMQ_CLEAR_OFFSET  1
	#define SRAM_QCNT_CTRL_SRAMQ_CLEAR_LEN     1
	#define SRAM_QCNT_CTRL_SRAMQ_CLEAR_DEFAULT 0x0

struct sram_qcnt_ctrl {
	a_uint32_t  sramq_go:1;
	a_uint32_t  sramq_clear:1;
	a_uint32_t  _reserved0:30;
};

union sram_qcnt_ctrl_u {
	a_uint32_t val;
	struct sram_qcnt_ctrl bf;
};

/*[register] SRAM_UNIQ_CNT_STATUS*/
#define SRAM_UNIQ_CNT_STATUS
#define SRAM_UNIQ_CNT_STATUS_ADDRESS 0xb4
#define SRAM_UNIQ_CNT_STATUS_NUM     2
#define SRAM_UNIQ_CNT_STATUS_INC     0x4
#define SRAM_UNIQ_CNT_STATUS_TYPE    REG_TYPE_RW
#define SRAM_UNIQ_CNT_STATUS_DEFAULT 0x0
	/*[field] STATUS*/
	#define SRAM_UNIQ_CNT_STATUS_STATUS
	#define SRAM_UNIQ_CNT_STATUS_STATUS_OFFSET  0
	#define SRAM_UNIQ_CNT_STATUS_STATUS_LEN     32
	#define SRAM_UNIQ_CNT_STATUS_STATUS_DEFAULT 0x0

struct sram_uniq_cnt_status {
	a_uint32_t  status:32;
};

union sram_uniq_cnt_status_u {
	a_uint32_t val;
	struct sram_uniq_cnt_status bf;
};

/*[register] SRAM_MULQ_CNT_STATUS*/
#define SRAM_MULQ_CNT_STATUS
#define SRAM_MULQ_CNT_STATUS_ADDRESS 0xc0
#define SRAM_MULQ_CNT_STATUS_NUM     1
#define SRAM_MULQ_CNT_STATUS_INC     0x4
#define SRAM_MULQ_CNT_STATUS_TYPE    REG_TYPE_RW
#define SRAM_MULQ_CNT_STATUS_DEFAULT 0x0
	/*[field] STATUS*/
	#define SRAM_MULQ_CNT_STATUS_STATUS
	#define SRAM_MULQ_CNT_STATUS_STATUS_OFFSET  0
	#define SRAM_MULQ_CNT_STATUS_STATUS_LEN     9
	#define SRAM_MULQ_CNT_STATUS_STATUS_DEFAULT 0x0

struct sram_mulq_cnt_status {
	a_uint32_t  status:9;
	a_uint32_t  _reserved0:23;
};

union sram_mulq_cnt_status_u {
	a_uint32_t val;
	struct sram_mulq_cnt_status bf;
};

/*[register] MCAST_PRIORITY_MAP8*/
#define MCAST_PRIORITY_MAP8
#define MCAST_PRIORITY_MAP8_ADDRESS 0x300
#define MCAST_PRIORITY_MAP8_NUM     16
#define MCAST_PRIORITY_MAP8_INC     0x4
#define MCAST_PRIORITY_MAP8_TYPE    REG_TYPE_RW
#define MCAST_PRIORITY_MAP8_DEFAULT 0x0
	/*[field] CLASS*/
	#define MCAST_PRIORITY_MAP8_CLASS
	#define MCAST_PRIORITY_MAP8_CLASS_OFFSET  0
	#define MCAST_PRIORITY_MAP8_CLASS_LEN     1
	#define MCAST_PRIORITY_MAP8_CLASS_DEFAULT 0x0

struct mcast_priority_map8 {
	a_uint32_t  class:1;
	a_uint32_t  _reserved0:31;
};

union mcast_priority_map8_u {
	a_uint32_t val;
	struct mcast_priority_map8 bf;
};

/*[table] UCAST_QUEUE_MAP_RULE_TBL*/
#define UCAST_QUEUE_MAP_RULE_TBL
#define UCAST_QUEUE_MAP_RULE_TBL_ADDRESS 0x2000
#define UCAST_QUEUE_MAP_RULE_TBL_NUM     64
#define UCAST_QUEUE_MAP_RULE_TBL_INC     0x10
#define UCAST_QUEUE_MAP_RULE_TBL_TYPE    REG_TYPE_RW
#define UCAST_QUEUE_MAP_RULE_TBL_DEFAULT 0x0
	/*[field] SRC_PROFILE_MAP*/
	#define UCAST_QUEUE_MAP_RULE_TBL_SRC_PROFILE_MAP
	#define UCAST_QUEUE_MAP_RULE_TBL_SRC_PROFILE_MAP_OFFSET  0
	#define UCAST_QUEUE_MAP_RULE_TBL_SRC_PROFILE_MAP_LEN     4
	#define UCAST_QUEUE_MAP_RULE_TBL_SRC_PROFILE_MAP_DEFAULT 0x0
	/*[field] PASS_THROUGH_MAP*/
	#define UCAST_QUEUE_MAP_RULE_TBL_PASS_THROUGH_MAP
	#define UCAST_QUEUE_MAP_RULE_TBL_PASS_THROUGH_MAP_OFFSET  4
	#define UCAST_QUEUE_MAP_RULE_TBL_PASS_THROUGH_MAP_LEN     4
	#define UCAST_QUEUE_MAP_RULE_TBL_PASS_THROUGH_MAP_DEFAULT 0x0
	/*[field] FIELD_0_MODE*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_MODE
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_MODE_OFFSET  8
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_MODE_LEN     1
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_MODE_DEFAULT 0x0
	/*[field] FIELD_0_TYPE_MAP reuse FIELD_0_MODE[0]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_MAP
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_MAP_OFFSET  9
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_MAP_LEN     4
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_MAP_DEFAULT 0x0
	/*[field] FIELD_0_TYPE reuse FIELD_0_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_OFFSET  9
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_LEN     2
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_TYPE_DEFAULT 0x0
	/*[field] FIELD_0_INFO reuse FIELD_0_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_OFFSET  11
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_LEN     8
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_DEFAULT 0x0
	/*[field] FIELD_0_INFO_HI reuse FIELD_0_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_HI
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_HI_OFFSET  19
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_HI_LEN     8
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_0_INFO_HI_DEFAULT 0x0
	/*[field] FIELD_1_MODE*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_MODE
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_MODE_OFFSET  27
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_MODE_LEN     1
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_MODE_DEFAULT 0x0
	/*[field] FIELD_1_TYPE_MAP reuse FIELD_1_MODE[0]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_MAP
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_MAP_OFFSET  28
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_MAP_LEN     4
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_MAP_DEFAULT 0x0
	/*[field] FIELD_1_TYPE reuse FIELD_1_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_OFFSET  28
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_LEN     2
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_TYPE_DEFAULT 0x0
	/*[field] FIELD_1_INFO reuse FIELD_1_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_OFFSET  30
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_LEN     8
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_DEFAULT 0x0
	/*[field] FIELD_1_INFO_HI reuse FIELD_1_MODE[1]*/
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_HI
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_HI_OFFSET  38
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_HI_LEN     8
	#define UCAST_QUEUE_MAP_RULE_TBL_FIELD_1_INFO_HI_DEFAULT 0x0
	/*[field] INT_PRI_INCL*/
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_INCL
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_INCL_OFFSET  46
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_INCL_LEN     1
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_INCL_DEFAULT 0x0
	/*[field] INT_PRI*/
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_OFFSET  47
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_LEN     4
	#define UCAST_QUEUE_MAP_RULE_TBL_INT_PRI_DEFAULT 0x0
	/*[field] SLICE*/
	#define UCAST_QUEUE_MAP_RULE_TBL_SLICE
	#define UCAST_QUEUE_MAP_RULE_TBL_SLICE_OFFSET  51
	#define UCAST_QUEUE_MAP_RULE_TBL_SLICE_LEN     1
	#define UCAST_QUEUE_MAP_RULE_TBL_SLICE_DEFAULT 0x0
	/*[field] VALID*/
	#define UCAST_QUEUE_MAP_RULE_TBL_VALID
	#define UCAST_QUEUE_MAP_RULE_TBL_VALID_OFFSET  52
	#define UCAST_QUEUE_MAP_RULE_TBL_VALID_LEN     1
	#define UCAST_QUEUE_MAP_RULE_TBL_VALID_DEFAULT 0x0

struct ucast_queue_map_rule_tbl_0 {
	a_uint32_t  src_profile_map:4;
	a_uint32_t  pass_through_map:4;
	a_uint32_t  field_0_mode:1;
	a_uint32_t  field_0_type_map:4;
	a_uint32_t  _reserved0:14;
	a_uint32_t  field_1_mode:1;
	a_uint32_t  field_1_type_map:4;
	a_uint32_t  _reserved1:14;
	a_uint32_t  int_pri_incl:1;
	a_uint32_t  int_pri:4;
	a_uint32_t  slice:1;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved2:11;
};

struct ucast_queue_map_rule_tbl_1 {
	a_uint32_t  src_profile_map:4;
	a_uint32_t  pass_through_map:4;
	a_uint32_t  field_0_mode:1;
	a_uint32_t  field_0_type_map:4;
	a_uint32_t  _reserved0:14;
	a_uint32_t  field_1_mode:1;
	a_uint32_t  field_1_type:2;
	a_uint32_t  field_1_info_0:2;
	a_uint32_t  field_1_info_1:6;
	a_uint32_t  field_1_info_hi:8;
	a_uint32_t  int_pri_incl:1;
	a_uint32_t  int_pri:4;
	a_uint32_t  slice:1;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved1:11;
};

struct ucast_queue_map_rule_tbl_2 {
	a_uint32_t  src_profile_map:4;
	a_uint32_t  pass_through_map:4;
	a_uint32_t  field_0_mode:1;
	a_uint32_t  field_0_type:2;
	a_uint32_t  field_0_info:8;
	a_uint32_t  field_0_info_hi:8;
	a_uint32_t  field_1_mode:1;
	a_uint32_t  field_1_type_map:4;
	a_uint32_t  _reserved0:14;
	a_uint32_t  int_pri_incl:1;
	a_uint32_t  int_pri:4;
	a_uint32_t  slice:1;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved1:11;
};

struct ucast_queue_map_rule_tbl_3 {
	a_uint32_t  src_profile_map:4;
	a_uint32_t  pass_through_map:4;
	a_uint32_t  field_0_mode:1;
	a_uint32_t  field_0_type:2;
	a_uint32_t  field_0_info:8;
	a_uint32_t  field_0_info_hi:8;
	a_uint32_t  field_1_mode:1;
	a_uint32_t  field_1_type:2;
	a_uint32_t  field_1_info_0:2;
	a_uint32_t  field_1_info_1:6;
	a_uint32_t  field_1_info_hi:8;
	a_uint32_t  int_pri_incl:1;
	a_uint32_t  int_pri:4;
	a_uint32_t  slice:1;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved0:11;
};

union ucast_queue_map_rule_tbl_u {
	a_uint32_t val[2];
	struct ucast_queue_map_rule_tbl_0 bf;
	struct ucast_queue_map_rule_tbl_1 bf1;
	struct ucast_queue_map_rule_tbl_2 bf2;
	struct ucast_queue_map_rule_tbl_3 bf3;
};

/*[table] UCAST_QUEUE_MAP_ACTION_TBL*/
#define UCAST_QUEUE_MAP_ACTION_TBL
#define UCAST_QUEUE_MAP_ACTION_TBL_ADDRESS 0x4000
#define UCAST_QUEUE_MAP_ACTION_TBL_NUM     64
#define UCAST_QUEUE_MAP_ACTION_TBL_INC     0x10
#define UCAST_QUEUE_MAP_ACTION_TBL_TYPE    REG_TYPE_RW
#define UCAST_QUEUE_MAP_ACTION_TBL_DEFAULT 0x0
	/*[field] QUEUEID_MODE*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUEID_MODE
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUEID_MODE_OFFSET  0
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUEID_MODE_LEN     2
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUEID_MODE_DEFAULT 0x0
	/*[field] QUEUE_MAP_BASE reuse QUEUEID_MODE[0]*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_MAP_BASE
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_MAP_BASE_OFFSET  2
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_MAP_BASE_LEN     4
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_MAP_BASE_DEFAULT 0x0
	/*[field] QUEUE_ID reuse QUEUEID_MODE[1]*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_ID
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_ID_OFFSET  2
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_ID_LEN     8
	#define UCAST_QUEUE_MAP_ACTION_TBL_QUEUE_ID_DEFAULT 0x0
	/*[field] FORCE_INDEX_TYPE_EN reuse QUEUEID_MODE[0]*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_EN
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_EN_OFFSET  6
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_EN_LEN     1
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_EN_DEFAULT 0x0
	/*[field] FORCE_INDEX_TYPE reuse QUEUEID_MODE[0]*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_OFFSET  7
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_LEN     3
	#define UCAST_QUEUE_MAP_ACTION_TBL_FORCE_INDEX_TYPE_DEFAULT 0x0
	/*[field] PROFILE_ID reuse QUEUEID_MODE[1]*/
	#define UCAST_QUEUE_MAP_ACTION_TBL_PROFILE_ID
	#define UCAST_QUEUE_MAP_ACTION_TBL_PROFILE_ID_OFFSET  10
	#define UCAST_QUEUE_MAP_ACTION_TBL_PROFILE_ID_LEN     4
	#define UCAST_QUEUE_MAP_ACTION_TBL_PROFILE_ID_DEFAULT 0x0

struct ucast_queue_map_action_tbl_0 {
	a_uint32_t  queueid_mode:2;
	a_uint32_t  queue_map_base:4;
	a_uint32_t  force_index_type_en:1;
	a_uint32_t  force_index_type:3;
	a_uint32_t  _reserved0:22;
};

struct ucast_queue_map_action_tbl_1 {
	a_uint32_t  queueid_mode:2;
	a_uint32_t  queue_id:8;
	a_uint32_t  profile_id:4;
	a_uint32_t  _reserved0:18;
};

struct ucast_queue_map_action_tbl_2 {
	a_uint32_t  queueid_mode:2;
	a_uint32_t  _reserved0:30;
};

struct ucast_queue_map_action_tbl_3 {
	a_uint32_t  queueid_mode:2;
	a_uint32_t  _reserved0:30;
};

union ucast_queue_map_action_tbl_u {
	a_uint32_t val;
	struct ucast_queue_map_action_tbl_0 bf;
	struct ucast_queue_map_action_tbl_1 bf1;
	struct ucast_queue_map_action_tbl_2 bf2;
	struct ucast_queue_map_action_tbl_3 bf3;
};

/*[table] UCAST_QUEUE_CTRL_TBL*/
#define UCAST_QUEUE_CTRL_TBL
#define UCAST_QUEUE_CTRL_TBL_ADDRESS 0x6000
#define UCAST_QUEUE_CTRL_TBL_NUM     256
#define UCAST_QUEUE_CTRL_TBL_INC     0x10
#define UCAST_QUEUE_CTRL_TBL_TYPE    REG_TYPE_RW
#define UCAST_QUEUE_CTRL_TBL_DEFAULT 0x0
	/*[field] DDRQ_EN*/
	#define UCAST_QUEUE_CTRL_TBL_DDRQ_EN
	#define UCAST_QUEUE_CTRL_TBL_DDRQ_EN_OFFSET  0
	#define UCAST_QUEUE_CTRL_TBL_DDRQ_EN_LEN     1
	#define UCAST_QUEUE_CTRL_TBL_DDRQ_EN_DEFAULT 0x0
	/*[field] QID_MISMATCH_CHECK_EN*/
	#define UCAST_QUEUE_CTRL_TBL_QID_MISMATCH_CHECK_EN
	#define UCAST_QUEUE_CTRL_TBL_QID_MISMATCH_CHECK_EN_OFFSET  1
	#define UCAST_QUEUE_CTRL_TBL_QID_MISMATCH_CHECK_EN_LEN     1
	#define UCAST_QUEUE_CTRL_TBL_QID_MISMATCH_CHECK_EN_DEFAULT 0x0

struct ucast_queue_ctrl_tbl {
	a_uint32_t  ddrq_en:1;
	a_uint32_t  qid_mismatch_check_en:1;
	a_uint32_t  _reserved0:30;
};

union ucast_queue_ctrl_tbl_u {
	a_uint32_t val;
	struct ucast_queue_ctrl_tbl bf;
};

/*[table] DDRQ_ENSCH_TBL*/
#define DDRQ_ENSCH_TBL
#define DDRQ_ENSCH_TBL_ADDRESS 0x33000
#define DDRQ_ENSCH_TBL_NUM     160
#define DDRQ_ENSCH_TBL_INC     0x10
#define DDRQ_ENSCH_TBL_TYPE    REG_TYPE_RW
#define DDRQ_ENSCH_TBL_DEFAULT 0x0
	/*[field] DDRQ_ID*/
	#define DDRQ_ENSCH_TBL_DDRQ_ID
	#define DDRQ_ENSCH_TBL_DDRQ_ID_OFFSET  0
	#define DDRQ_ENSCH_TBL_DDRQ_ID_LEN     8
	#define DDRQ_ENSCH_TBL_DDRQ_ID_DEFAULT 0x0

struct ddrq_ensch_tbl {
	a_uint32_t  ddrq_id:8;
	a_uint32_t  _reserved0:24;
};

union ddrq_ensch_tbl_u {
	a_uint32_t val;
	struct ddrq_ensch_tbl bf;
};

/*[table] DDRQ_PKT_CNT_TBL*/
#define DDRQ_PKT_CNT_TBL
#define DDRQ_PKT_CNT_TBL_ADDRESS 0x34000
#define DDRQ_PKT_CNT_TBL_NUM     160
#define DDRQ_PKT_CNT_TBL_INC     0x10
#define DDRQ_PKT_CNT_TBL_TYPE    REG_TYPE_RW
#define DDRQ_PKT_CNT_TBL_DEFAULT 0x0
	/*[field] DDRQ_PKT_CNT*/
	#define DDRQ_PKT_CNT_TBL_DDRQ_PKT_CNT
	#define DDRQ_PKT_CNT_TBL_DDRQ_PKT_CNT_OFFSET  0
	#define DDRQ_PKT_CNT_TBL_DDRQ_PKT_CNT_LEN     21
	#define DDRQ_PKT_CNT_TBL_DDRQ_PKT_CNT_DEFAULT 0x0

struct ddrq_pkt_cnt_tbl {
	a_uint32_t  ddrq_pkt_cnt:21;
	a_uint32_t  _reserved0:11;
};

union ddrq_pkt_cnt_tbl_u {
	a_uint32_t val;
	struct ddrq_pkt_cnt_tbl bf;
};

/*[table] DDRQ_BYTE_CNT_TBL*/
#define DDRQ_BYTE_CNT_TBL
#define DDRQ_BYTE_CNT_TBL_ADDRESS 0x36000
#define DDRQ_BYTE_CNT_TBL_NUM     160
#define DDRQ_BYTE_CNT_TBL_INC     0x10
#define DDRQ_BYTE_CNT_TBL_TYPE    REG_TYPE_RW
#define DDRQ_BYTE_CNT_TBL_DEFAULT 0x0
	/*[field] DDRQ_BYTE_CNT*/
	#define DDRQ_BYTE_CNT_TBL_DDRQ_BYTE_CNT
	#define DDRQ_BYTE_CNT_TBL_DDRQ_BYTE_CNT_OFFSET  0
	#define DDRQ_BYTE_CNT_TBL_DDRQ_BYTE_CNT_LEN     38
	#define DDRQ_BYTE_CNT_TBL_DDRQ_BYTE_CNT_DEFAULT 0x0

struct ddrq_byte_cnt_tbl {
	a_uint32_t  ddrq_byte_cnt_0:32;
	a_uint32_t  ddrq_byte_cnt_1:6;
	a_uint32_t  _reserved0:26;
};

union ddrq_byte_cnt_tbl_u {
	a_uint32_t val[2];
	struct ddrq_byte_cnt_tbl bf;
};

/*[table] SRAM_UNIQ_CNT_QID_MAP*/
#define SRAM_UNIQ_CNT_QID_MAP
#define SRAM_UNIQ_CNT_QID_MAP_ADDRESS 0x38000
#define SRAM_UNIQ_CNT_QID_MAP_NUM     256
#define SRAM_UNIQ_CNT_QID_MAP_INC     0x10
#define SRAM_UNIQ_CNT_QID_MAP_TYPE    REG_TYPE_RW
#define SRAM_UNIQ_CNT_QID_MAP_DEFAULT 0x0
	/*[field] UNIQ_CNT_ID*/
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_OFFSET  0
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_LEN     6
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_DEFAULT 0x0
	/*[field] UNIQ_CNT_ID_EN*/
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_EN
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_EN_OFFSET  6
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_EN_LEN     1
	#define SRAM_UNIQ_CNT_QID_MAP_UNIQ_CNT_ID_EN_DEFAULT 0x0

struct sram_uniq_cnt_qid_map {
	a_uint32_t  uniq_cnt_id:6;
	a_uint32_t  uniq_cnt_id_en:1;
	a_uint32_t  _reserved0:25;
};

union sram_uniq_cnt_qid_map_u {
	a_uint32_t val;
	struct sram_uniq_cnt_qid_map bf;
};

/*[table] SRAM_UNIQ_CNT_CFG_TBL*/
#define SRAM_UNIQ_CNT_CFG_TBL
#define SRAM_UNIQ_CNT_CFG_TBL_ADDRESS 0x3a000
#define SRAM_UNIQ_CNT_CFG_TBL_NUM     64
#define SRAM_UNIQ_CNT_CFG_TBL_INC     0x10
#define SRAM_UNIQ_CNT_CFG_TBL_TYPE    REG_TYPE_RW
#define SRAM_UNIQ_CNT_CFG_TBL_DEFAULT 0x0
	/*[field] UNIQ_HI_TH_TYPE*/
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_TYPE
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_TYPE_OFFSET  0
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_TYPE_LEN     1
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_TYPE_DEFAULT 0x0
	/*[field] UNIQ_HI_TH*/
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_OFFSET  1
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_LEN     20
	#define SRAM_UNIQ_CNT_CFG_TBL_UNIQ_HI_TH_DEFAULT 0x0

struct sram_uniq_cnt_cfg_tbl {
	a_uint32_t  uniq_hi_th_type:1;
	a_uint32_t  uniq_hi_th:20;
	a_uint32_t  _reserved0:11;
};

union sram_uniq_cnt_cfg_tbl_u {
	a_uint32_t val;
	struct sram_uniq_cnt_cfg_tbl bf;
};

/*[table] SRAM_UNIQ_CNT_TBL*/
#define SRAM_UNIQ_CNT_TBL
#define SRAM_UNIQ_CNT_TBL_ADDRESS 0x3c000
#define SRAM_UNIQ_CNT_TBL_NUM     64
#define SRAM_UNIQ_CNT_TBL_INC     0x10
#define SRAM_UNIQ_CNT_TBL_TYPE    REG_TYPE_RW
#define SRAM_UNIQ_CNT_TBL_DEFAULT 0x0
	/*[field] UNIQ_PEAK_BYTE_CNT*/
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_BYTE_CNT
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_BYTE_CNT_OFFSET  0
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_BYTE_CNT_LEN     20
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_BYTE_CNT_DEFAULT 0x0
	/*[field] UNIQ_PEAK_PKT_CNT*/
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_PKT_CNT
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_PKT_CNT_OFFSET  20
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_PKT_CNT_LEN     12
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PEAK_PKT_CNT_DEFAULT 0x0
	/*[field] UNIQ_BYTE_CNT*/
	#define SRAM_UNIQ_CNT_TBL_UNIQ_BYTE_CNT
	#define SRAM_UNIQ_CNT_TBL_UNIQ_BYTE_CNT_OFFSET  32
	#define SRAM_UNIQ_CNT_TBL_UNIQ_BYTE_CNT_LEN     20
	#define SRAM_UNIQ_CNT_TBL_UNIQ_BYTE_CNT_DEFAULT 0x0
	/*[field] UNIQ_PKT_CNT*/
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PKT_CNT
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PKT_CNT_OFFSET  52
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PKT_CNT_LEN     12
	#define SRAM_UNIQ_CNT_TBL_UNIQ_PKT_CNT_DEFAULT 0x0

struct sram_uniq_cnt_tbl {
	a_uint32_t  uniq_peak_byte_cnt:20;
	a_uint32_t  uniq_peak_pkt_cnt:12;
	a_uint32_t  uniq_byte_cnt:20;
	a_uint32_t  uniq_pkt_cnt:12;
};

union sram_uniq_cnt_tbl_u {
	a_uint32_t val[2];
	struct sram_uniq_cnt_tbl bf;
};

/*[table] SRAM_MULQ_CNT_CFG_TBL*/
#define SRAM_MULQ_CNT_CFG_TBL
#define SRAM_MULQ_CNT_CFG_TBL_ADDRESS 0x3e000
#define SRAM_MULQ_CNT_CFG_TBL_NUM     9
#define SRAM_MULQ_CNT_CFG_TBL_INC     0x10
#define SRAM_MULQ_CNT_CFG_TBL_TYPE    REG_TYPE_RW
#define SRAM_MULQ_CNT_CFG_TBL_DEFAULT 0x0
	/*[field] MULQ_QID*/
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_OFFSET  0
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_LEN     4
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_DEFAULT 0x0
	/*[field] MULQ_QID_EN*/
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_EN
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_EN_OFFSET  4
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_EN_LEN     1
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_QID_EN_DEFAULT 0x0
	/*[field] MULQ_HI_TH_TYPE*/
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_TYPE
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_TYPE_OFFSET  5
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_TYPE_LEN     1
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_TYPE_DEFAULT 0x0
	/*[field] MULQ_HI_TH*/
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_OFFSET  6
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_LEN     22
	#define SRAM_MULQ_CNT_CFG_TBL_MULQ_HI_TH_DEFAULT 0x0

struct sram_mulq_cnt_cfg_tbl {
	a_uint32_t  mulq_qid:4;
	a_uint32_t  mulq_qid_en:1;
	a_uint32_t  mulq_hi_th_type:1;
	a_uint32_t  mulq_hi_th:22;
	a_uint32_t  _reserved0:4;
};

union sram_mulq_cnt_cfg_tbl_u {
	a_uint32_t val;
	struct sram_mulq_cnt_cfg_tbl bf;
};

/*[table] SRAM_MULQ_CNT_TBL*/
#define SRAM_MULQ_CNT_TBL
#define SRAM_MULQ_CNT_TBL_ADDRESS 0x3f000
#define SRAM_MULQ_CNT_TBL_NUM     9
#define SRAM_MULQ_CNT_TBL_INC     0x10
#define SRAM_MULQ_CNT_TBL_TYPE    REG_TYPE_RW
#define SRAM_MULQ_CNT_TBL_DEFAULT 0x0
	/*[field] MULQ_PEAK_BYTE_CNT*/
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_BYTE_CNT
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_BYTE_CNT_OFFSET  0
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_BYTE_CNT_LEN     22
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_BYTE_CNT_DEFAULT 0x0
	/*[field] MULQ_PEAK_PKT_CNT*/
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_PKT_CNT
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_PKT_CNT_OFFSET  22
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_PKT_CNT_LEN     14
	#define SRAM_MULQ_CNT_TBL_MULQ_PEAK_PKT_CNT_DEFAULT 0x0
	/*[field] MULQ_BYTE_CNT*/
	#define SRAM_MULQ_CNT_TBL_MULQ_BYTE_CNT
	#define SRAM_MULQ_CNT_TBL_MULQ_BYTE_CNT_OFFSET  36
	#define SRAM_MULQ_CNT_TBL_MULQ_BYTE_CNT_LEN     22
	#define SRAM_MULQ_CNT_TBL_MULQ_BYTE_CNT_DEFAULT 0x0
	/*[field] MULQ_PKT_CNT*/
	#define SRAM_MULQ_CNT_TBL_MULQ_PKT_CNT
	#define SRAM_MULQ_CNT_TBL_MULQ_PKT_CNT_OFFSET  58
	#define SRAM_MULQ_CNT_TBL_MULQ_PKT_CNT_LEN     14
	#define SRAM_MULQ_CNT_TBL_MULQ_PKT_CNT_DEFAULT 0x0

struct sram_mulq_cnt_tbl {
	a_uint32_t  mulq_peak_byte_cnt:22;
	a_uint32_t  mulq_peak_pkt_cnt_0:10;
	a_uint32_t  mulq_peak_pkt_cnt_1:4;
	a_uint32_t  mulq_byte_cnt:22;
	a_uint32_t  mulq_pkt_cnt_0:6;
	a_uint32_t  mulq_pkt_cnt_1:8;
	a_uint32_t  _reserved0:24;
};

union sram_mulq_cnt_tbl_u {
	a_uint32_t val[3];
	struct sram_mulq_cnt_tbl bf;
};

/*[table] QUEUE_MAP_TBL*/
#define QUEUE_MAP_TBL
#define QUEUE_MAP_TBL_ADDRESS 0x40000
#define QUEUE_MAP_TBL_NUM     300
#define QUEUE_MAP_TBL_INC     0x10
#define QUEUE_MAP_TBL_TYPE    REG_TYPE_RW
#define QUEUE_MAP_TBL_DEFAULT 0x0
	/*[field] TCONT_ID*/
	#define QUEUE_MAP_TBL_TCONT_ID
	#define QUEUE_MAP_TBL_TCONT_ID_OFFSET  0
	#define QUEUE_MAP_TBL_TCONT_ID_LEN     5
	#define QUEUE_MAP_TBL_TCONT_ID_DEFAULT 0x0
	/*[field] TCONT_ID_VLD*/
	#define QUEUE_MAP_TBL_TCONT_ID_VLD
	#define QUEUE_MAP_TBL_TCONT_ID_VLD_OFFSET  5
	#define QUEUE_MAP_TBL_TCONT_ID_VLD_LEN     1
	#define QUEUE_MAP_TBL_TCONT_ID_VLD_DEFAULT 0x0

struct queue_map_tbl {
	a_uint32_t  tcont_id:5;
	a_uint32_t  tcont_id_vld:1;
	a_uint32_t  _reserved0:26;
};

union queue_map_tbl_u {
	a_uint32_t val;
	struct queue_map_tbl bf;
};

/*[table] TCONT_BYTE_CNT_TBL*/
#define TCONT_BYTE_CNT_TBL
#define TCONT_BYTE_CNT_TBL_ADDRESS 0x42000
#define TCONT_BYTE_CNT_TBL_NUM     32
#define TCONT_BYTE_CNT_TBL_INC     0x10
#define TCONT_BYTE_CNT_TBL_TYPE    REG_TYPE_RW
#define TCONT_BYTE_CNT_TBL_DEFAULT 0x0
	/*[field] TCONT_BYTE_CNT*/
	#define TCONT_BYTE_CNT_TBL_TCONT_BYTE_CNT
	#define TCONT_BYTE_CNT_TBL_TCONT_BYTE_CNT_OFFSET  0
	#define TCONT_BYTE_CNT_TBL_TCONT_BYTE_CNT_LEN     38
	#define TCONT_BYTE_CNT_TBL_TCONT_BYTE_CNT_DEFAULT 0x0

struct tcont_byte_cnt_tbl {
	a_uint32_t  tcont_byte_cnt_0:32;
	a_uint32_t  tcont_byte_cnt_1:6;
	a_uint32_t  _reserved0:26;
};

union tcont_byte_cnt_tbl_u {
	a_uint32_t val[2];
	struct tcont_byte_cnt_tbl bf;
};

/*[table] DDRQ_DEBIT_TBL*/
#define DDRQ_DEBIT_TBL
#define DDRQ_DEBIT_TBL_ADDRESS 0x44000
#define DDRQ_DEBIT_TBL_NUM     160
#define DDRQ_DEBIT_TBL_INC     0x10
#define DDRQ_DEBIT_TBL_TYPE    REG_TYPE_RW
#define DDRQ_DEBIT_TBL_DEFAULT 0x0
	/*[field] DDRQ_DEBIT*/
	#define DDRQ_DEBIT_TBL_DDRQ_DEBIT
	#define DDRQ_DEBIT_TBL_DDRQ_DEBIT_OFFSET  0
	#define DDRQ_DEBIT_TBL_DDRQ_DEBIT_LEN     15
	#define DDRQ_DEBIT_TBL_DDRQ_DEBIT_DEFAULT 0x0

struct ddrq_debit_tbl {
	a_uint32_t  ddrq_debit:15;
	a_uint32_t  _reserved0:17;
};

union ddrq_debit_tbl_u {
	a_uint32_t val;
	struct ddrq_debit_tbl bf;
};

/*[table] TCONT_CREDIT_TBL*/
#define TCONT_CREDIT_TBL
#define TCONT_CREDIT_TBL_ADDRESS 0x46000
#define TCONT_CREDIT_TBL_NUM     32
#define TCONT_CREDIT_TBL_INC     0x10
#define TCONT_CREDIT_TBL_TYPE    REG_TYPE_RW
#define TCONT_CREDIT_TBL_DEFAULT 0x0
	/*[field] TCONT_CREDIT*/
	#define TCONT_CREDIT_TBL_TCONT_CREDIT
	#define TCONT_CREDIT_TBL_TCONT_CREDIT_OFFSET  0
	#define TCONT_CREDIT_TBL_TCONT_CREDIT_LEN     16
	#define TCONT_CREDIT_TBL_TCONT_CREDIT_DEFAULT 0x0

struct tcont_credit_tbl {
	a_uint32_t  tcont_credit:16;
	a_uint32_t  _reserved0:16;
};

union tcont_credit_tbl_u {
	a_uint32_t val;
	struct tcont_credit_tbl bf;
};

/*[table] DDRQ_COMP_TBL*/
#define DDRQ_COMP_TBL
#define DDRQ_COMP_TBL_ADDRESS 0x47000
#define DDRQ_COMP_TBL_NUM     160
#define DDRQ_COMP_TBL_INC     0x10
#define DDRQ_COMP_TBL_TYPE    REG_TYPE_RW
#define DDRQ_COMP_TBL_DEFAULT 0x0
	/*[field] COMP_BYTE_CNT*/
	#define DDRQ_COMP_TBL_COMP_BYTE_CNT
	#define DDRQ_COMP_TBL_COMP_BYTE_CNT_OFFSET  0
	#define DDRQ_COMP_TBL_COMP_BYTE_CNT_LEN     17
	#define DDRQ_COMP_TBL_COMP_BYTE_CNT_DEFAULT 0x0

struct ddrq_comp_tbl {
	a_uint32_t  comp_byte_cnt:17;
	a_uint32_t  _reserved0:15;
};

union ddrq_comp_tbl_u {
	a_uint32_t val;
	struct ddrq_comp_tbl bf;
};

/*[table] OQ_LL_MUL_P8_TBL*/
#define OQ_LL_MUL_P8_TBL
#define OQ_LL_MUL_P8_TBL_ADDRESS 0x1b0000
#define OQ_LL_MUL_P8_TBL_NUM     3072
#define OQ_LL_MUL_P8_TBL_INC     0x10
#define OQ_LL_MUL_P8_TBL_TYPE    REG_TYPE_RW
#define OQ_LL_MUL_P8_TBL_DEFAULT 0x0
	/*[field] NORMAL_FWD*/
	#define OQ_LL_MUL_P8_TBL_NORMAL_FWD
	#define OQ_LL_MUL_P8_TBL_NORMAL_FWD_OFFSET  0
	#define OQ_LL_MUL_P8_TBL_NORMAL_FWD_LEN     1
	#define OQ_LL_MUL_P8_TBL_NORMAL_FWD_DEFAULT 0x0
	/*[field] EGRESS_MIRR*/
	#define OQ_LL_MUL_P8_TBL_EGRESS_MIRR
	#define OQ_LL_MUL_P8_TBL_EGRESS_MIRR_OFFSET  1
	#define OQ_LL_MUL_P8_TBL_EGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P8_TBL_EGRESS_MIRR_DEFAULT 0x0
	/*[field] INGRESS_MIRR*/
	#define OQ_LL_MUL_P8_TBL_INGRESS_MIRR
	#define OQ_LL_MUL_P8_TBL_INGRESS_MIRR_OFFSET  2
	#define OQ_LL_MUL_P8_TBL_INGRESS_MIRR_LEN     1
	#define OQ_LL_MUL_P8_TBL_INGRESS_MIRR_DEFAULT 0x0
	/*[field] NEXT_POINTER*/
	#define OQ_LL_MUL_P8_TBL_NEXT_POINTER
	#define OQ_LL_MUL_P8_TBL_NEXT_POINTER_OFFSET  3
	#define OQ_LL_MUL_P8_TBL_NEXT_POINTER_LEN     12
	#define OQ_LL_MUL_P8_TBL_NEXT_POINTER_DEFAULT 0x0

struct oq_ll_mul_p8_tbl {
	a_uint32_t  normal_fwd:1;
	a_uint32_t  egress_mirr:1;
	a_uint32_t  ingress_mirr:1;
	a_uint32_t  next_pointer:12;
	a_uint32_t  _reserved0:17;
};

union oq_ll_mul_p8_tbl_u {
	a_uint32_t val;
	struct oq_ll_mul_p8_tbl bf;
};

/*[table] MUL_P8_DROP_CNT_TBL*/
#define MUL_P8_DROP_CNT_TBL
#define MUL_P8_DROP_CNT_TBL_ADDRESS 0x1f8000
#define MUL_P8_DROP_CNT_TBL_NUM     6
#define MUL_P8_DROP_CNT_TBL_INC     0x10
#define MUL_P8_DROP_CNT_TBL_TYPE    REG_TYPE_RW
#define MUL_P8_DROP_CNT_TBL_DEFAULT 0x0
	/*[field] MUL_P8_DROP_PKT*/
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_PKT
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_PKT_OFFSET  0
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_PKT_LEN     32
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_PKT_DEFAULT 0x0
	/*[field] MUL_P8_DROP_BYTE*/
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_BYTE
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_BYTE_OFFSET  32
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_BYTE_LEN     40
	#define MUL_P8_DROP_CNT_TBL_MUL_P8_DROP_BYTE_DEFAULT 0x0

struct mul_p8_drop_cnt_tbl {
	a_uint32_t  mul_p8_drop_pkt:32;
	a_uint32_t  mul_p8_drop_byte_0:32;
	a_uint32_t  mul_p8_drop_byte_1:8;
	a_uint32_t  _reserved0:24;
};

union mul_p8_drop_cnt_tbl_u {
	a_uint32_t val[3];
	struct mul_p8_drop_cnt_tbl bf;
};

#endif
