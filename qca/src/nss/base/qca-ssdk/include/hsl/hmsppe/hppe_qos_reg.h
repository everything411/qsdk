/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_QOS_REG_H_
#define _HPPE_QOS_REG_H_

/*[register] TDM_DEPTH_CFG*/
#define TDM_DEPTH_CFG
#define TDM_DEPTH_CFG_ADDRESS 0x0
#define TDM_DEPTH_CFG_NUM     1
#define TDM_DEPTH_CFG_INC     0x4
#define TDM_DEPTH_CFG_TYPE    REG_TYPE_RW
#define TDM_DEPTH_CFG_DEFAULT 0x3b
	/*[field] TDM_DEPTH*/
	#define TDM_DEPTH_CFG_TDM_DEPTH
	#define TDM_DEPTH_CFG_TDM_DEPTH_OFFSET  0
	#define TDM_DEPTH_CFG_TDM_DEPTH_LEN     8
	#define TDM_DEPTH_CFG_TDM_DEPTH_DEFAULT 0x3b

struct tdm_depth_cfg {
	a_uint32_t  tdm_depth:8;
	a_uint32_t  _reserved0:24;
};

union tdm_depth_cfg_u {
	a_uint32_t val;
	struct tdm_depth_cfg bf;
};

/*[table] L0_FLOW_MAP_TBL*/
#define L0_FLOW_MAP_TBL
#define L0_FLOW_MAP_TBL_ADDRESS 0x2000
#define L0_FLOW_MAP_TBL_NUM     300
#define L0_FLOW_MAP_TBL_INC     0x10
#define L0_FLOW_MAP_TBL_TYPE    REG_TYPE_RW
#define L0_FLOW_MAP_TBL_DEFAULT 0x0
	/*[field] SP_ID*/
	#define L0_FLOW_MAP_TBL_SP_ID
	#define L0_FLOW_MAP_TBL_SP_ID_OFFSET  0
	#define L0_FLOW_MAP_TBL_SP_ID_LEN     6
	#define L0_FLOW_MAP_TBL_SP_ID_DEFAULT 0x0
	/*[field] C_PRI*/
	#define L0_FLOW_MAP_TBL_C_PRI
	#define L0_FLOW_MAP_TBL_C_PRI_OFFSET  6
	#define L0_FLOW_MAP_TBL_C_PRI_LEN     3
	#define L0_FLOW_MAP_TBL_C_PRI_DEFAULT 0x0
	/*[field] E_PRI*/
	#define L0_FLOW_MAP_TBL_E_PRI
	#define L0_FLOW_MAP_TBL_E_PRI_OFFSET  9
	#define L0_FLOW_MAP_TBL_E_PRI_LEN     3
	#define L0_FLOW_MAP_TBL_E_PRI_DEFAULT 0x0
	/*[field] C_DRR_WT*/
	#define L0_FLOW_MAP_TBL_C_DRR_WT
	#define L0_FLOW_MAP_TBL_C_DRR_WT_OFFSET  12
	#define L0_FLOW_MAP_TBL_C_DRR_WT_LEN     10
	#define L0_FLOW_MAP_TBL_C_DRR_WT_DEFAULT 0x0
	/*[field] E_DRR_WT*/
	#define L0_FLOW_MAP_TBL_E_DRR_WT
	#define L0_FLOW_MAP_TBL_E_DRR_WT_OFFSET  22
	#define L0_FLOW_MAP_TBL_E_DRR_WT_LEN     10
	#define L0_FLOW_MAP_TBL_E_DRR_WT_DEFAULT 0x0
	/*[field] C_DRR_ID*/
	#define L0_FLOW_MAP_TBL_C_DRR_ID
	#define L0_FLOW_MAP_TBL_C_DRR_ID_OFFSET  32
	#define L0_FLOW_MAP_TBL_C_DRR_ID_LEN     8
	#define L0_FLOW_MAP_TBL_C_DRR_ID_DEFAULT 0x0
	/*[field] E_DRR_ID*/
	#define L0_FLOW_MAP_TBL_E_DRR_ID
	#define L0_FLOW_MAP_TBL_E_DRR_ID_OFFSET  40
	#define L0_FLOW_MAP_TBL_E_DRR_ID_LEN     8
	#define L0_FLOW_MAP_TBL_E_DRR_ID_DEFAULT 0x0
	/*[field] C_DRR_CREDIT_UNIT*/
	#define L0_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT
	#define L0_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_OFFSET  48
	#define L0_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_LEN     1
	#define L0_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_DEFAULT 0x0
	/*[field] E_DRR_CREDIT_UNIT*/
	#define L0_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT
	#define L0_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_OFFSET  49
	#define L0_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_LEN     1
	#define L0_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_DEFAULT 0x0

struct l0_flow_map_tbl {
	a_uint32_t  sp_id:6;
	a_uint32_t  c_pri:3;
	a_uint32_t  e_pri:3;
	a_uint32_t  c_drr_wt:10;
	a_uint32_t  e_drr_wt:10;
	a_uint32_t  c_drr_id:8;
	a_uint32_t  e_drr_id:8;
	a_uint32_t  c_drr_credit_unit:1;
	a_uint32_t  e_drr_credit_unit:1;
	a_uint32_t  _reserved0:14;
};

union l0_flow_map_tbl_u {
	a_uint32_t val[2];
	struct l0_flow_map_tbl bf;
};

/*[table] L0_FLOW_PORT_MAP_TBL*/
#define L0_FLOW_PORT_MAP_TBL
#define L0_FLOW_PORT_MAP_TBL_ADDRESS 0x8000
#define L0_FLOW_PORT_MAP_TBL_NUM     300
#define L0_FLOW_PORT_MAP_TBL_INC     0x10
#define L0_FLOW_PORT_MAP_TBL_TYPE    REG_TYPE_RW
#define L0_FLOW_PORT_MAP_TBL_DEFAULT 0x0
	/*[field] PORT_NUM*/
	#define L0_FLOW_PORT_MAP_TBL_PORT_NUM
	#define L0_FLOW_PORT_MAP_TBL_PORT_NUM_OFFSET  0
	#define L0_FLOW_PORT_MAP_TBL_PORT_NUM_LEN     4
	#define L0_FLOW_PORT_MAP_TBL_PORT_NUM_DEFAULT 0x0

struct l0_flow_port_map_tbl {
	a_uint32_t  port_num:4;
	a_uint32_t  _reserved0:28;
};

union l0_flow_port_map_tbl_u {
	a_uint32_t val;
	struct l0_flow_port_map_tbl bf;
};

/*[table] RFC_BLOCK_TBL*/
#define RFC_BLOCK_TBL
#define RFC_BLOCK_TBL_ADDRESS 0x2c000
#define RFC_BLOCK_TBL_NUM     300
#define RFC_BLOCK_TBL_INC     0x10
#define RFC_BLOCK_TBL_TYPE    REG_TYPE_RO
#define RFC_BLOCK_TBL_DEFAULT 0x0
	/*[field] RFC_BLOCK*/
	#define RFC_BLOCK_TBL_RFC_BLOCK
	#define RFC_BLOCK_TBL_RFC_BLOCK_OFFSET  0
	#define RFC_BLOCK_TBL_RFC_BLOCK_LEN     1
	#define RFC_BLOCK_TBL_RFC_BLOCK_DEFAULT 0x0

struct rfc_block_tbl {
	a_uint32_t  rfc_block:1;
	a_uint32_t  _reserved0:31;
};

union rfc_block_tbl_u {
	a_uint32_t val;
	struct rfc_block_tbl bf;
};

/*[table] RFC_STATUS_TBL*/
#define RFC_STATUS_TBL
#define RFC_STATUS_TBL_ADDRESS 0x2e000
#define RFC_STATUS_TBL_NUM     300
#define RFC_STATUS_TBL_INC     0x10
#define RFC_STATUS_TBL_TYPE    REG_TYPE_RO
#define RFC_STATUS_TBL_DEFAULT 0x0
	/*[field] RFC_STATUS*/
	#define RFC_STATUS_TBL_RFC_STATUS
	#define RFC_STATUS_TBL_RFC_STATUS_OFFSET  0
	#define RFC_STATUS_TBL_RFC_STATUS_LEN     1
	#define RFC_STATUS_TBL_RFC_STATUS_DEFAULT 0x0

struct rfc_status_tbl {
	a_uint32_t  rfc_status:1;
	a_uint32_t  _reserved0:31;
};

union rfc_status_tbl_u {
	a_uint32_t val;
	struct rfc_status_tbl bf;
};

/*[table] DEQ_DIS_TBL*/
#define DEQ_DIS_TBL
#define DEQ_DIS_TBL_ADDRESS 0x30000
#define DEQ_DIS_TBL_NUM     300
#define DEQ_DIS_TBL_INC     0x10
#define DEQ_DIS_TBL_TYPE    REG_TYPE_RW
#define DEQ_DIS_TBL_DEFAULT 0x0
	/*[field] DEQ_DIS*/
	#define DEQ_DIS_TBL_DEQ_DIS
	#define DEQ_DIS_TBL_DEQ_DIS_OFFSET  0
	#define DEQ_DIS_TBL_DEQ_DIS_LEN     1
	#define DEQ_DIS_TBL_DEQ_DIS_DEFAULT 0x0

struct deq_dis_tbl {
	a_uint32_t  deq_dis:1;
	a_uint32_t  _reserved0:31;
};

union deq_dis_tbl_u {
	a_uint32_t val;
	struct deq_dis_tbl bf;
};

/*[table] RING_Q_MAP_TBL*/
#define RING_Q_MAP_TBL
#define RING_Q_MAP_TBL_ADDRESS 0x34000
#define RING_Q_MAP_TBL_NUM     176
#define RING_Q_MAP_TBL_INC     0x40
#define RING_Q_MAP_TBL_TYPE    REG_TYPE_RW
#define RING_Q_MAP_TBL_DEFAULT 0x0
	/*[field] QUEUE_BITMAP*/
	#define RING_Q_MAP_TBL_QUEUE_BITMAP
	#define RING_Q_MAP_TBL_QUEUE_BITMAP_OFFSET  0
	#define RING_Q_MAP_TBL_QUEUE_BITMAP_LEN     300
	#define RING_Q_MAP_TBL_QUEUE_BITMAP_DEFAULT 0x0

struct ring_q_map_tbl {
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

union ring_q_map_tbl_u {
	a_uint32_t val[10];
	struct ring_q_map_tbl bf;
};

/*[table] L1_FLOW_MAP_TBL*/
#define L1_FLOW_MAP_TBL
#define L1_FLOW_MAP_TBL_ADDRESS 0x40000
#define L1_FLOW_MAP_TBL_NUM     64
#define L1_FLOW_MAP_TBL_INC     0x10
#define L1_FLOW_MAP_TBL_TYPE    REG_TYPE_RW
#define L1_FLOW_MAP_TBL_DEFAULT 0x0
	/*[field] SP_ID*/
	#define L1_FLOW_MAP_TBL_SP_ID
	#define L1_FLOW_MAP_TBL_SP_ID_OFFSET  0
	#define L1_FLOW_MAP_TBL_SP_ID_LEN     6
	#define L1_FLOW_MAP_TBL_SP_ID_DEFAULT 0x0
	/*[field] C_PRI*/
	#define L1_FLOW_MAP_TBL_C_PRI
	#define L1_FLOW_MAP_TBL_C_PRI_OFFSET  6
	#define L1_FLOW_MAP_TBL_C_PRI_LEN     3
	#define L1_FLOW_MAP_TBL_C_PRI_DEFAULT 0x0
	/*[field] E_PRI*/
	#define L1_FLOW_MAP_TBL_E_PRI
	#define L1_FLOW_MAP_TBL_E_PRI_OFFSET  9
	#define L1_FLOW_MAP_TBL_E_PRI_LEN     3
	#define L1_FLOW_MAP_TBL_E_PRI_DEFAULT 0x0
	/*[field] C_DRR_WT*/
	#define L1_FLOW_MAP_TBL_C_DRR_WT
	#define L1_FLOW_MAP_TBL_C_DRR_WT_OFFSET  12
	#define L1_FLOW_MAP_TBL_C_DRR_WT_LEN     10
	#define L1_FLOW_MAP_TBL_C_DRR_WT_DEFAULT 0x0
	/*[field] E_DRR_WT*/
	#define L1_FLOW_MAP_TBL_E_DRR_WT
	#define L1_FLOW_MAP_TBL_E_DRR_WT_OFFSET  22
	#define L1_FLOW_MAP_TBL_E_DRR_WT_LEN     10
	#define L1_FLOW_MAP_TBL_E_DRR_WT_DEFAULT 0x0
	/*[field] C_DRR_ID*/
	#define L1_FLOW_MAP_TBL_C_DRR_ID
	#define L1_FLOW_MAP_TBL_C_DRR_ID_OFFSET  32
	#define L1_FLOW_MAP_TBL_C_DRR_ID_LEN     6
	#define L1_FLOW_MAP_TBL_C_DRR_ID_DEFAULT 0x0
	/*[field] E_DRR_ID*/
	#define L1_FLOW_MAP_TBL_E_DRR_ID
	#define L1_FLOW_MAP_TBL_E_DRR_ID_OFFSET  38
	#define L1_FLOW_MAP_TBL_E_DRR_ID_LEN     6
	#define L1_FLOW_MAP_TBL_E_DRR_ID_DEFAULT 0x0
	/*[field] C_DRR_CREDIT_UNIT*/
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_OFFSET  44
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_LEN     1
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_DEFAULT 0x0
	/*[field] E_DRR_CREDIT_UNIT*/
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_OFFSET  45
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_LEN     1
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_DEFAULT 0x0

struct l1_flow_map_tbl {
	a_uint32_t  sp_id:6;
	a_uint32_t  c_pri:3;
	a_uint32_t  e_pri:3;
	a_uint32_t  c_drr_wt:10;
	a_uint32_t  e_drr_wt:10;
	a_uint32_t  c_drr_id:6;
	a_uint32_t  e_drr_id:6;
	a_uint32_t  c_drr_credit_unit:1;
	a_uint32_t  e_drr_credit_unit:1;
	a_uint32_t  _reserved0:18;
};

union l1_flow_map_tbl_u {
	a_uint32_t val[2];
	struct l1_flow_map_tbl bf;
};

/*[table] L1_FLOW_PORT_MAP_TBL*/
#define L1_FLOW_PORT_MAP_TBL
#define L1_FLOW_PORT_MAP_TBL_ADDRESS 0x46000
#define L1_FLOW_PORT_MAP_TBL_NUM     64
#define L1_FLOW_PORT_MAP_TBL_INC     0x10
#define L1_FLOW_PORT_MAP_TBL_TYPE    REG_TYPE_RW
#define L1_FLOW_PORT_MAP_TBL_DEFAULT 0x0
	/*[field] PORT_NUM*/
	#define L1_FLOW_PORT_MAP_TBL_PORT_NUM
	#define L1_FLOW_PORT_MAP_TBL_PORT_NUM_OFFSET  0
	#define L1_FLOW_PORT_MAP_TBL_PORT_NUM_LEN     4
	#define L1_FLOW_PORT_MAP_TBL_PORT_NUM_DEFAULT 0x0

struct l1_flow_port_map_tbl {
	a_uint32_t  port_num:4;
	a_uint32_t  _reserved0:28;
};

union l1_flow_port_map_tbl_u {
	a_uint32_t val;
	struct l1_flow_port_map_tbl bf;
};

/*[table] PSCH_TDM_CFG_TBL*/
#define PSCH_TDM_CFG_TBL
#define PSCH_TDM_CFG_TBL_ADDRESS 0x7a000
#define PSCH_TDM_CFG_TBL_NUM     128
#define PSCH_TDM_CFG_TBL_INC     0x10
#define PSCH_TDM_CFG_TBL_TYPE    REG_TYPE_RW
#define PSCH_TDM_CFG_TBL_DEFAULT 0x0
	/*[field] DES_PORT*/
	#define PSCH_TDM_CFG_TBL_DES_PORT
	#define PSCH_TDM_CFG_TBL_DES_PORT_OFFSET  0
	#define PSCH_TDM_CFG_TBL_DES_PORT_LEN     4
	#define PSCH_TDM_CFG_TBL_DES_PORT_DEFAULT 0x0
	/*[field] ENS_PORT*/
	#define PSCH_TDM_CFG_TBL_ENS_PORT
	#define PSCH_TDM_CFG_TBL_ENS_PORT_OFFSET  4
	#define PSCH_TDM_CFG_TBL_ENS_PORT_LEN     4
	#define PSCH_TDM_CFG_TBL_ENS_PORT_DEFAULT 0x0
	/*[field] ENS_PORT_BITMAP*/
	#define PSCH_TDM_CFG_TBL_ENS_PORT_BITMAP
	#define PSCH_TDM_CFG_TBL_ENS_PORT_BITMAP_OFFSET  8
	#define PSCH_TDM_CFG_TBL_ENS_PORT_BITMAP_LEN     9
	#define PSCH_TDM_CFG_TBL_ENS_PORT_BITMAP_DEFAULT 0x0
	/*[field] DES_SEC_PORT_EN*/
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_EN
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_EN_OFFSET  17
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_EN_LEN     1
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_EN_DEFAULT 0x0
	/*[field] DES_SEC_PORT*/
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_OFFSET  18
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_LEN     4
	#define PSCH_TDM_CFG_TBL_DES_SEC_PORT_DEFAULT 0x0

struct psch_tdm_cfg_tbl {
	a_uint32_t  des_port:4;
	a_uint32_t  ens_port:4;
	a_uint32_t  ens_port_bitmap:9;
	a_uint32_t  des_sec_port_en:1;
	a_uint32_t  des_sec_port:4;
	a_uint32_t  _reserved0:10;
};

union psch_tdm_cfg_tbl_u {
	a_uint32_t val;
	struct psch_tdm_cfg_tbl bf;
};

#endif
