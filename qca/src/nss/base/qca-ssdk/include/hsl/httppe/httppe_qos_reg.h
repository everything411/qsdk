/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_QOS_REG_H_
#define _HTTPPE_QOS_REG_H_

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
	#define L1_FLOW_MAP_TBL_SP_ID_LEN     4
	#define L1_FLOW_MAP_TBL_SP_ID_DEFAULT 0x0
	/*[field] C_PRI*/
	#define L1_FLOW_MAP_TBL_C_PRI
	#define L1_FLOW_MAP_TBL_C_PRI_OFFSET  4
	#define L1_FLOW_MAP_TBL_C_PRI_LEN     3
	#define L1_FLOW_MAP_TBL_C_PRI_DEFAULT 0x0
	/*[field] E_PRI*/
	#define L1_FLOW_MAP_TBL_E_PRI
	#define L1_FLOW_MAP_TBL_E_PRI_OFFSET  7
	#define L1_FLOW_MAP_TBL_E_PRI_LEN     3
	#define L1_FLOW_MAP_TBL_E_PRI_DEFAULT 0x0
	/*[field] C_DRR_WT*/
	#define L1_FLOW_MAP_TBL_C_DRR_WT
	#define L1_FLOW_MAP_TBL_C_DRR_WT_OFFSET  10
	#define L1_FLOW_MAP_TBL_C_DRR_WT_LEN     10
	#define L1_FLOW_MAP_TBL_C_DRR_WT_DEFAULT 0x0
	/*[field] E_DRR_WT*/
	#define L1_FLOW_MAP_TBL_E_DRR_WT
	#define L1_FLOW_MAP_TBL_E_DRR_WT_OFFSET  20
	#define L1_FLOW_MAP_TBL_E_DRR_WT_LEN     10
	#define L1_FLOW_MAP_TBL_E_DRR_WT_DEFAULT 0x0
	/*[field] C_DRR_ID*/
	#define L1_FLOW_MAP_TBL_C_DRR_ID
	#define L1_FLOW_MAP_TBL_C_DRR_ID_OFFSET  30
	#define L1_FLOW_MAP_TBL_C_DRR_ID_LEN     6
	#define L1_FLOW_MAP_TBL_C_DRR_ID_DEFAULT 0x0
	/*[field] E_DRR_ID*/
	#define L1_FLOW_MAP_TBL_E_DRR_ID
	#define L1_FLOW_MAP_TBL_E_DRR_ID_OFFSET  36
	#define L1_FLOW_MAP_TBL_E_DRR_ID_LEN     6
	#define L1_FLOW_MAP_TBL_E_DRR_ID_DEFAULT 0x0
	/*[field] C_DRR_CREDIT_UNIT*/
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_OFFSET  42
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_LEN     1
	#define L1_FLOW_MAP_TBL_C_DRR_CREDIT_UNIT_DEFAULT 0x0
	/*[field] E_DRR_CREDIT_UNIT*/
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_OFFSET  43
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_LEN     1
	#define L1_FLOW_MAP_TBL_E_DRR_CREDIT_UNIT_DEFAULT 0x0

struct l1_flow_map_tbl {
	a_uint32_t  sp_id:4;
	a_uint32_t  c_pri:3;
	a_uint32_t  e_pri:3;
	a_uint32_t  c_drr_wt:10;
	a_uint32_t  e_drr_wt:10;
	a_uint32_t  c_drr_id_0:2;
	a_uint32_t  c_drr_id_1:4;
	a_uint32_t  e_drr_id:6;
	a_uint32_t  c_drr_credit_unit:1;
	a_uint32_t  e_drr_credit_unit:1;
	a_uint32_t  _reserved0:20;
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

/*[table] L0_COMP_CFG_TBL*/
#define L0_COMP_CFG_TBL
#define L0_COMP_CFG_TBL_ADDRESS 0x28000
#define L0_COMP_CFG_TBL_NUM     300
#define L0_COMP_CFG_TBL_INC     0x10
#define L0_COMP_CFG_TBL_TYPE    REG_TYPE_RW
#define L0_COMP_CFG_TBL_DEFAULT 0x0
	/*[field] SHAPER_METER_LEN*/
	#define L0_COMP_CFG_TBL_SHAPER_METER_LEN
	#define L0_COMP_CFG_TBL_SHAPER_METER_LEN_OFFSET  0
	#define L0_COMP_CFG_TBL_SHAPER_METER_LEN_LEN     2
	#define L0_COMP_CFG_TBL_SHAPER_METER_LEN_DEFAULT 0x0
	/*[field] DRR_METER_LEN*/
	#define L0_COMP_CFG_TBL_DRR_METER_LEN
	#define L0_COMP_CFG_TBL_DRR_METER_LEN_OFFSET  2
	#define L0_COMP_CFG_TBL_DRR_METER_LEN_LEN     2
	#define L0_COMP_CFG_TBL_DRR_METER_LEN_DEFAULT 0x0

struct l0_comp_cfg_tbl {
	a_uint32_t  shaper_meter_len:2;
	a_uint32_t  drr_meter_len:2;
	a_uint32_t  _reserved0:28;
};

union l0_comp_cfg_tbl_u {
	a_uint32_t val;
	struct l0_comp_cfg_tbl bf;
};

/*[table] L1_COMP_CFG_TBL*/
#define L1_COMP_CFG_TBL
#define L1_COMP_CFG_TBL_ADDRESS 0x6a000
#define L1_COMP_CFG_TBL_NUM     64
#define L1_COMP_CFG_TBL_INC     0x10
#define L1_COMP_CFG_TBL_TYPE    REG_TYPE_RW
#define L1_COMP_CFG_TBL_DEFAULT 0x0
	/*[field] SHAPER_METER_LEN*/
	#define L1_COMP_CFG_TBL_SHAPER_METER_LEN
	#define L1_COMP_CFG_TBL_SHAPER_METER_LEN_OFFSET  0
	#define L1_COMP_CFG_TBL_SHAPER_METER_LEN_LEN     2
	#define L1_COMP_CFG_TBL_SHAPER_METER_LEN_DEFAULT 0x0
	/*[field] DRR_METER_LEN*/
	#define L1_COMP_CFG_TBL_DRR_METER_LEN
	#define L1_COMP_CFG_TBL_DRR_METER_LEN_OFFSET  2
	#define L1_COMP_CFG_TBL_DRR_METER_LEN_LEN     2
	#define L1_COMP_CFG_TBL_DRR_METER_LEN_DEFAULT 0x0

struct l1_comp_cfg_tbl {
	a_uint32_t  shaper_meter_len:2;
	a_uint32_t  drr_meter_len:2;
	a_uint32_t  _reserved0:28;
};

union l1_comp_cfg_tbl_u {
	a_uint32_t val;
	struct l1_comp_cfg_tbl bf;
};

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

/*[table] QOS_MAPPING_TBL*/
#define QOS_MAPPING_TBL
#define QOS_MAPPING_TBL_ADDRESS 0x40000
#define QOS_MAPPING_TBL_NUM     544
#define QOS_MAPPING_TBL_INC     0x8
#define QOS_MAPPING_TBL_TYPE    REG_TYPE_RW
#define QOS_MAPPING_TBL_DEFAULT 0x0
	/*[field] INT_DSCP_TC*/
	#define QOS_MAPPING_TBL_INT_DSCP_TC
	#define QOS_MAPPING_TBL_INT_DSCP_TC_OFFSET  0
	#define QOS_MAPPING_TBL_INT_DSCP_TC_LEN     8
	#define QOS_MAPPING_TBL_INT_DSCP_TC_DEFAULT 0x0
	/*[field] DSCP_TC_MASK*/
	#define QOS_MAPPING_TBL_DSCP_TC_MASK
	#define QOS_MAPPING_TBL_DSCP_TC_MASK_OFFSET  8
	#define QOS_MAPPING_TBL_DSCP_TC_MASK_LEN     8
	#define QOS_MAPPING_TBL_DSCP_TC_MASK_DEFAULT 0x0
	/*[field] INT_DSCP_EN*/
	#define QOS_MAPPING_TBL_INT_DSCP_EN
	#define QOS_MAPPING_TBL_INT_DSCP_EN_OFFSET  16
	#define QOS_MAPPING_TBL_INT_DSCP_EN_LEN     1
	#define QOS_MAPPING_TBL_INT_DSCP_EN_DEFAULT 0x0
	/*[field] INT_PCP_EN*/
	#define QOS_MAPPING_TBL_INT_PCP_EN
	#define QOS_MAPPING_TBL_INT_PCP_EN_OFFSET  17
	#define QOS_MAPPING_TBL_INT_PCP_EN_LEN     1
	#define QOS_MAPPING_TBL_INT_PCP_EN_DEFAULT 0x0
	/*[field] INT_PCP*/
	#define QOS_MAPPING_TBL_INT_PCP
	#define QOS_MAPPING_TBL_INT_PCP_OFFSET  18
	#define QOS_MAPPING_TBL_INT_PCP_LEN     3
	#define QOS_MAPPING_TBL_INT_PCP_DEFAULT 0x0
	/*[field] INT_DEI_EN*/
	#define QOS_MAPPING_TBL_INT_DEI_EN
	#define QOS_MAPPING_TBL_INT_DEI_EN_OFFSET  21
	#define QOS_MAPPING_TBL_INT_DEI_EN_LEN     1
	#define QOS_MAPPING_TBL_INT_DEI_EN_DEFAULT 0x0
	/*[field] INT_DEI*/
	#define QOS_MAPPING_TBL_INT_DEI
	#define QOS_MAPPING_TBL_INT_DEI_OFFSET  22
	#define QOS_MAPPING_TBL_INT_DEI_LEN     1
	#define QOS_MAPPING_TBL_INT_DEI_DEFAULT 0x0
	/*[field] INT_PRI_EN*/
	#define QOS_MAPPING_TBL_INT_PRI_EN
	#define QOS_MAPPING_TBL_INT_PRI_EN_OFFSET  23
	#define QOS_MAPPING_TBL_INT_PRI_EN_LEN     1
	#define QOS_MAPPING_TBL_INT_PRI_EN_DEFAULT 0x0
	/*[field] INT_PRI*/
	#define QOS_MAPPING_TBL_INT_PRI
	#define QOS_MAPPING_TBL_INT_PRI_OFFSET  24
	#define QOS_MAPPING_TBL_INT_PRI_LEN     4
	#define QOS_MAPPING_TBL_INT_PRI_DEFAULT 0x0
	/*[field] INT_DP_EN*/
	#define QOS_MAPPING_TBL_INT_DP_EN
	#define QOS_MAPPING_TBL_INT_DP_EN_OFFSET  28
	#define QOS_MAPPING_TBL_INT_DP_EN_LEN     1
	#define QOS_MAPPING_TBL_INT_DP_EN_DEFAULT 0x0
	/*[field] INT_DP*/
	#define QOS_MAPPING_TBL_INT_DP
	#define QOS_MAPPING_TBL_INT_DP_OFFSET  29
	#define QOS_MAPPING_TBL_INT_DP_LEN     2
	#define QOS_MAPPING_TBL_INT_DP_DEFAULT 0x0
	/*[field] QOS_RES_PREC*/
	#define QOS_MAPPING_TBL_QOS_RES_PREC
	#define QOS_MAPPING_TBL_QOS_RES_PREC_OFFSET  31
	#define QOS_MAPPING_TBL_QOS_RES_PREC_LEN     3
	#define QOS_MAPPING_TBL_QOS_RES_PREC_DEFAULT 0x0
	/*[field] FLOW_POLICY_VALID*/
	#define QOS_MAPPING_TBL_FLOW_POLICY_VALID
	#define QOS_MAPPING_TBL_FLOW_POLICY_VALID_OFFSET  34
	#define QOS_MAPPING_TBL_FLOW_POLICY_VALID_LEN     1
	#define QOS_MAPPING_TBL_FLOW_POLICY_VALID_DEFAULT 0x0
	/*[field] FLOW_POLICY_ID*/
	#define QOS_MAPPING_TBL_FLOW_POLICY_ID
	#define QOS_MAPPING_TBL_FLOW_POLICY_ID_OFFSET  35
	#define QOS_MAPPING_TBL_FLOW_POLICY_ID_LEN     9
	#define QOS_MAPPING_TBL_FLOW_POLICY_ID_DEFAULT 0x0

struct qos_mapping_tbl {
	a_uint32_t  int_dscp_tc:8;
	a_uint32_t  dscp_tc_mask:8;
	a_uint32_t  int_dscp_en:1;
	a_uint32_t  int_pcp_en:1;
	a_uint32_t  int_pcp:3;
	a_uint32_t  int_dei_en:1;
	a_uint32_t  int_dei:1;
	a_uint32_t  int_pri_en:1;
	a_uint32_t  int_pri:4;
	a_uint32_t  int_dp_en:1;
	a_uint32_t  int_dp:2;
	a_uint32_t  qos_res_prec_0:1;
	a_uint32_t  qos_res_prec_1:2;
	a_uint32_t  flow_policy_valid:1;
	a_uint32_t  flow_policy_id:9;
	a_uint32_t  _reserved0:20;
};

union qos_mapping_tbl_u {
	a_uint32_t val[2];
	struct qos_mapping_tbl bf;
};

#endif
