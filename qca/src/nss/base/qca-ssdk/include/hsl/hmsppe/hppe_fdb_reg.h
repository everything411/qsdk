/*
 * Copyright (c) 2016-2017, 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_FDB_REG_H_
#define _HPPE_FDB_REG_H_

/*[register] FDB_TBL_OP*/
#define FDB_TBL_OP
#define FDB_TBL_OP_ADDRESS 0x8
#define FDB_TBL_OP_NUM     1
#define FDB_TBL_OP_INC     0x4
#define FDB_TBL_OP_TYPE    REG_TYPE_RW
#define FDB_TBL_OP_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define FDB_TBL_OP_CMD_ID
	#define FDB_TBL_OP_CMD_ID_OFFSET  0
	#define FDB_TBL_OP_CMD_ID_LEN     4
	#define FDB_TBL_OP_CMD_ID_DEFAULT 0x0
	/*[field] BYP_RSLT_EN*/
	#define FDB_TBL_OP_BYP_RSLT_EN
	#define FDB_TBL_OP_BYP_RSLT_EN_OFFSET  4
	#define FDB_TBL_OP_BYP_RSLT_EN_LEN     1
	#define FDB_TBL_OP_BYP_RSLT_EN_DEFAULT 0x0
	/*[field] OP_TYPE*/
	#define FDB_TBL_OP_OP_TYPE
	#define FDB_TBL_OP_OP_TYPE_OFFSET  5
	#define FDB_TBL_OP_OP_TYPE_LEN     3
	#define FDB_TBL_OP_OP_TYPE_DEFAULT 0x0
	/*[field] HASH_BLOCK_BITMAP*/
	#define FDB_TBL_OP_HASH_BLOCK_BITMAP
	#define FDB_TBL_OP_HASH_BLOCK_BITMAP_OFFSET  8
	#define FDB_TBL_OP_HASH_BLOCK_BITMAP_LEN     2
	#define FDB_TBL_OP_HASH_BLOCK_BITMAP_DEFAULT 0x0
	/*[field] OP_MODE*/
	#define FDB_TBL_OP_OP_MODE
	#define FDB_TBL_OP_OP_MODE_OFFSET  10
	#define FDB_TBL_OP_OP_MODE_LEN     1
	#define FDB_TBL_OP_OP_MODE_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define FDB_TBL_OP_ENTRY_INDEX
	#define FDB_TBL_OP_ENTRY_INDEX_OFFSET  11
	#define FDB_TBL_OP_ENTRY_INDEX_LEN     11
	#define FDB_TBL_OP_ENTRY_INDEX_DEFAULT 0x0

struct fdb_tbl_op {
	a_uint32_t  cmd_id:4;
	a_uint32_t  byp_rslt_en:1;
	a_uint32_t  op_type:3;
	a_uint32_t  hash_block_bitmap:2;
	a_uint32_t  op_mode:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:10;
};

union fdb_tbl_op_u {
	a_uint32_t val;
	struct fdb_tbl_op bf;
};

/*[register] FDB_TBL_RD_OP*/
#define FDB_TBL_RD_OP
#define FDB_TBL_RD_OP_ADDRESS 0x10
#define FDB_TBL_RD_OP_NUM     1
#define FDB_TBL_RD_OP_INC     0x4
#define FDB_TBL_RD_OP_TYPE    REG_TYPE_RW
#define FDB_TBL_RD_OP_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define FDB_TBL_RD_OP_CMD_ID
	#define FDB_TBL_RD_OP_CMD_ID_OFFSET  0
	#define FDB_TBL_RD_OP_CMD_ID_LEN     4
	#define FDB_TBL_RD_OP_CMD_ID_DEFAULT 0x0
	/*[field] BYP_RSLT_EN*/
	#define FDB_TBL_RD_OP_BYP_RSLT_EN
	#define FDB_TBL_RD_OP_BYP_RSLT_EN_OFFSET  4
	#define FDB_TBL_RD_OP_BYP_RSLT_EN_LEN     1
	#define FDB_TBL_RD_OP_BYP_RSLT_EN_DEFAULT 0x0
	/*[field] OP_TYPE*/
	#define FDB_TBL_RD_OP_OP_TYPE
	#define FDB_TBL_RD_OP_OP_TYPE_OFFSET  5
	#define FDB_TBL_RD_OP_OP_TYPE_LEN     3
	#define FDB_TBL_RD_OP_OP_TYPE_DEFAULT 0x0
	/*[field] HASH_BLOCK_BITMAP*/
	#define FDB_TBL_RD_OP_HASH_BLOCK_BITMAP
	#define FDB_TBL_RD_OP_HASH_BLOCK_BITMAP_OFFSET  8
	#define FDB_TBL_RD_OP_HASH_BLOCK_BITMAP_LEN     2
	#define FDB_TBL_RD_OP_HASH_BLOCK_BITMAP_DEFAULT 0x0
	/*[field] OP_MODE*/
	#define FDB_TBL_RD_OP_OP_MODE
	#define FDB_TBL_RD_OP_OP_MODE_OFFSET  10
	#define FDB_TBL_RD_OP_OP_MODE_LEN     1
	#define FDB_TBL_RD_OP_OP_MODE_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define FDB_TBL_RD_OP_ENTRY_INDEX
	#define FDB_TBL_RD_OP_ENTRY_INDEX_OFFSET  11
	#define FDB_TBL_RD_OP_ENTRY_INDEX_LEN     11
	#define FDB_TBL_RD_OP_ENTRY_INDEX_DEFAULT 0x0

struct fdb_tbl_rd_op {
	a_uint32_t  cmd_id:4;
	a_uint32_t  byp_rslt_en:1;
	a_uint32_t  op_type:3;
	a_uint32_t  hash_block_bitmap:2;
	a_uint32_t  op_mode:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:10;
};

union fdb_tbl_rd_op_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op bf;
};

/*[register] FDB_TBL_OP_RSLT*/
#define FDB_TBL_OP_RSLT
#define FDB_TBL_OP_RSLT_ADDRESS 0x20
#define FDB_TBL_OP_RSLT_NUM     1
#define FDB_TBL_OP_RSLT_INC     0x4
#define FDB_TBL_OP_RSLT_TYPE    REG_TYPE_RO
#define FDB_TBL_OP_RSLT_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define FDB_TBL_OP_RSLT_CMD_ID
	#define FDB_TBL_OP_RSLT_CMD_ID_OFFSET  0
	#define FDB_TBL_OP_RSLT_CMD_ID_LEN     4
	#define FDB_TBL_OP_RSLT_CMD_ID_DEFAULT 0x0
	/*[field] OP_RSLT*/
	#define FDB_TBL_OP_RSLT_OP_RSLT
	#define FDB_TBL_OP_RSLT_OP_RSLT_OFFSET  4
	#define FDB_TBL_OP_RSLT_OP_RSLT_LEN     1
	#define FDB_TBL_OP_RSLT_OP_RSLT_DEFAULT 0x0
	/*[field] VALID_CNT*/
	#define FDB_TBL_OP_RSLT_VALID_CNT
	#define FDB_TBL_OP_RSLT_VALID_CNT_OFFSET  5
	#define FDB_TBL_OP_RSLT_VALID_CNT_LEN     4
	#define FDB_TBL_OP_RSLT_VALID_CNT_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define FDB_TBL_OP_RSLT_ENTRY_INDEX
	#define FDB_TBL_OP_RSLT_ENTRY_INDEX_OFFSET  9
	#define FDB_TBL_OP_RSLT_ENTRY_INDEX_LEN     11
	#define FDB_TBL_OP_RSLT_ENTRY_INDEX_DEFAULT 0x0

struct fdb_tbl_op_rslt {
	a_uint32_t  cmd_id:4;
	a_uint32_t  op_rslt:1;
	a_uint32_t  valid_cnt:4;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:12;
};

union fdb_tbl_op_rslt_u {
	a_uint32_t val;
	struct fdb_tbl_op_rslt bf;
};

/*[register] FDB_TBL_RD_OP_RSLT*/
#define FDB_TBL_RD_OP_RSLT
#define FDB_TBL_RD_OP_RSLT_ADDRESS 0x30
#define FDB_TBL_RD_OP_RSLT_NUM     1
#define FDB_TBL_RD_OP_RSLT_INC     0x4
#define FDB_TBL_RD_OP_RSLT_TYPE    REG_TYPE_RO
#define FDB_TBL_RD_OP_RSLT_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define FDB_TBL_RD_OP_RSLT_CMD_ID
	#define FDB_TBL_RD_OP_RSLT_CMD_ID_OFFSET  0
	#define FDB_TBL_RD_OP_RSLT_CMD_ID_LEN     4
	#define FDB_TBL_RD_OP_RSLT_CMD_ID_DEFAULT 0x0
	/*[field] OP_RSLT*/
	#define FDB_TBL_RD_OP_RSLT_OP_RSLT
	#define FDB_TBL_RD_OP_RSLT_OP_RSLT_OFFSET  4
	#define FDB_TBL_RD_OP_RSLT_OP_RSLT_LEN     1
	#define FDB_TBL_RD_OP_RSLT_OP_RSLT_DEFAULT 0x0
	/*[field] VALID_CNT*/
	#define FDB_TBL_RD_OP_RSLT_VALID_CNT
	#define FDB_TBL_RD_OP_RSLT_VALID_CNT_OFFSET  5
	#define FDB_TBL_RD_OP_RSLT_VALID_CNT_LEN     4
	#define FDB_TBL_RD_OP_RSLT_VALID_CNT_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define FDB_TBL_RD_OP_RSLT_ENTRY_INDEX
	#define FDB_TBL_RD_OP_RSLT_ENTRY_INDEX_OFFSET  9
	#define FDB_TBL_RD_OP_RSLT_ENTRY_INDEX_LEN     11
	#define FDB_TBL_RD_OP_RSLT_ENTRY_INDEX_DEFAULT 0x0

struct fdb_tbl_rd_op_rslt {
	a_uint32_t  cmd_id:4;
	a_uint32_t  op_rslt:1;
	a_uint32_t  valid_cnt:4;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:12;
};

union fdb_tbl_rd_op_rslt_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_rslt bf;
};

/*[register] AGE_TIMER*/
#define AGE_TIMER
#define AGE_TIMER_ADDRESS 0x34
#define AGE_TIMER_NUM     1
#define AGE_TIMER_INC     0x4
#define AGE_TIMER_TYPE    REG_TYPE_RW
#define AGE_TIMER_DEFAULT 0x96
	/*[field] AGE_VAL*/
	#define AGE_TIMER_AGE_VAL
	#define AGE_TIMER_AGE_VAL_OFFSET  0
	#define AGE_TIMER_AGE_VAL_LEN     20
	#define AGE_TIMER_AGE_VAL_DEFAULT 0x96

struct age_timer {
	a_uint32_t  age_val:20;
	a_uint32_t  _reserved0:12;
};

union age_timer_u {
	a_uint32_t val;
	struct age_timer bf;
};

/*[register] L2_GLOBAL_CONF*/
#define L2_GLOBAL_CONF
#define L2_GLOBAL_CONF_ADDRESS 0x38
#define L2_GLOBAL_CONF_NUM     1
#define L2_GLOBAL_CONF_INC     0x4
#define L2_GLOBAL_CONF_TYPE    REG_TYPE_RW
#define L2_GLOBAL_CONF_DEFAULT 0x400c0
	/*[field] FDB_HASH_MODE_0*/
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_0
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_0_OFFSET  0
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_0_LEN     2
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_0_DEFAULT 0x0
	/*[field] FDB_HASH_MODE_1*/
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_1
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_1_OFFSET  2
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_1_LEN     2
	#define L2_GLOBAL_CONF_FDB_HASH_MODE_1_DEFAULT 0x0
	/*[field] FDB_HASH_FULL_FWD_CMD*/
	#define L2_GLOBAL_CONF_FDB_HASH_FULL_FWD_CMD
	#define L2_GLOBAL_CONF_FDB_HASH_FULL_FWD_CMD_OFFSET  4
	#define L2_GLOBAL_CONF_FDB_HASH_FULL_FWD_CMD_LEN     2
	#define L2_GLOBAL_CONF_FDB_HASH_FULL_FWD_CMD_DEFAULT 0x0
	/*[field] LRN_EN*/
	#define L2_GLOBAL_CONF_LRN_EN
	#define L2_GLOBAL_CONF_LRN_EN_OFFSET  6
	#define L2_GLOBAL_CONF_LRN_EN_LEN     1
	#define L2_GLOBAL_CONF_LRN_EN_DEFAULT 0x1
	/*[field] AGE_EN*/
	#define L2_GLOBAL_CONF_AGE_EN
	#define L2_GLOBAL_CONF_AGE_EN_OFFSET  7
	#define L2_GLOBAL_CONF_AGE_EN_LEN     1
	#define L2_GLOBAL_CONF_AGE_EN_DEFAULT 0x1
	/*[field] LRN_CTRL_MODE*/
	#define L2_GLOBAL_CONF_LRN_CTRL_MODE
	#define L2_GLOBAL_CONF_LRN_CTRL_MODE_OFFSET  8
	#define L2_GLOBAL_CONF_LRN_CTRL_MODE_LEN     1
	#define L2_GLOBAL_CONF_LRN_CTRL_MODE_DEFAULT 0x0
	/*[field] AGE_CTRL_MODE*/
	#define L2_GLOBAL_CONF_AGE_CTRL_MODE
	#define L2_GLOBAL_CONF_AGE_CTRL_MODE_OFFSET  9
	#define L2_GLOBAL_CONF_AGE_CTRL_MODE_LEN     1
	#define L2_GLOBAL_CONF_AGE_CTRL_MODE_DEFAULT 0x0
	/*[field] FAILOVER_EN*/
	#define L2_GLOBAL_CONF_FAILOVER_EN
	#define L2_GLOBAL_CONF_FAILOVER_EN_OFFSET  10
	#define L2_GLOBAL_CONF_FAILOVER_EN_LEN     1
	#define L2_GLOBAL_CONF_FAILOVER_EN_DEFAULT 0x0
	/*[field] SERVICE_CODE_LOOP*/
	#define L2_GLOBAL_CONF_SERVICE_CODE_LOOP
	#define L2_GLOBAL_CONF_SERVICE_CODE_LOOP_OFFSET  11
	#define L2_GLOBAL_CONF_SERVICE_CODE_LOOP_LEN     1
	#define L2_GLOBAL_CONF_SERVICE_CODE_LOOP_DEFAULT 0x0
	/*[field] FLOW_CPY_ESCAPE*/
	#define L2_GLOBAL_CONF_FLOW_CPY_ESCAPE
	#define L2_GLOBAL_CONF_FLOW_CPY_ESCAPE_OFFSET  12
	#define L2_GLOBAL_CONF_FLOW_CPY_ESCAPE_LEN     1
	#define L2_GLOBAL_CONF_FLOW_CPY_ESCAPE_DEFAULT 0x0
	/*[field] MC_PVLAN_ISOL_EN*/
	#define L2_GLOBAL_CONF_MC_PVLAN_ISOL_EN
	#define L2_GLOBAL_CONF_MC_PVLAN_ISOL_EN_OFFSET  13
	#define L2_GLOBAL_CONF_MC_PVLAN_ISOL_EN_LEN     1
	#define L2_GLOBAL_CONF_MC_PVLAN_ISOL_EN_DEFAULT 0x0
	/*[field] BC_PVLAN_ISOL_EN*/
	#define L2_GLOBAL_CONF_BC_PVLAN_ISOL_EN
	#define L2_GLOBAL_CONF_BC_PVLAN_ISOL_EN_OFFSET  14
	#define L2_GLOBAL_CONF_BC_PVLAN_ISOL_EN_LEN     1
	#define L2_GLOBAL_CONF_BC_PVLAN_ISOL_EN_DEFAULT 0x0
	/*[field] IPMC_EN*/
	#define L2_GLOBAL_CONF_IPMC_EN
	#define L2_GLOBAL_CONF_IPMC_EN_OFFSET  15
	#define L2_GLOBAL_CONF_IPMC_EN_LEN     1
	#define L2_GLOBAL_CONF_IPMC_EN_DEFAULT 0x0
	/*[field] IPMC_HASH_MODE_0*/
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_0
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_0_OFFSET  16
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_0_LEN     2
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_0_DEFAULT 0x0
	/*[field] IPMC_HASH_MODE_1*/
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_1
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_1_OFFSET  18
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_1_LEN     2
	#define L2_GLOBAL_CONF_IPMC_HASH_MODE_1_DEFAULT 0x1
	/*[field] MC_VLAN_MATCH_MODE*/
	#define L2_GLOBAL_CONF_MC_VLAN_MATCH_MODE
	#define L2_GLOBAL_CONF_MC_VLAN_MATCH_MODE_OFFSET  20
	#define L2_GLOBAL_CONF_MC_VLAN_MATCH_MODE_LEN     1
	#define L2_GLOBAL_CONF_MC_VLAN_MATCH_MODE_DEFAULT 0x0
	/*[field] MC_DMAC_CHECK_EN*/
	#define L2_GLOBAL_CONF_MC_DMAC_CHECK_EN
	#define L2_GLOBAL_CONF_MC_DMAC_CHECK_EN_OFFSET  21
	#define L2_GLOBAL_CONF_MC_DMAC_CHECK_EN_LEN     1
	#define L2_GLOBAL_CONF_MC_DMAC_CHECK_EN_DEFAULT 0x0
	/*[field] IPMC_MISMATCH_ACT*/
	#define L2_GLOBAL_CONF_IPMC_MISMATCH_ACT
	#define L2_GLOBAL_CONF_IPMC_MISMATCH_ACT_OFFSET  22
	#define L2_GLOBAL_CONF_IPMC_MISMATCH_ACT_LEN     2
	#define L2_GLOBAL_CONF_IPMC_MISMATCH_ACT_DEFAULT 0x0
	/*[field] DOT1P_MAPPER_VLAN_MODE*/
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_VLAN_MODE
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_VLAN_MODE_OFFSET  24
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_VLAN_MODE_LEN     2
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_VLAN_MODE_DEFAULT 0x0
	/*[field] DOT1P_MAPPER_PCP_MODE*/
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_PCP_MODE
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_PCP_MODE_OFFSET  26
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_PCP_MODE_LEN     2
	#define L2_GLOBAL_CONF_DOT1P_MAPPER_PCP_MODE_DEFAULT 0x0

struct l2_global_conf {
	a_uint32_t  fdb_hash_mode_0:2;
	a_uint32_t  fdb_hash_mode_1:2;
	a_uint32_t  fdb_hash_full_fwd_cmd:2;
	a_uint32_t  lrn_en:1;
	a_uint32_t  age_en:1;
	a_uint32_t  lrn_ctrl_mode:1;
	a_uint32_t  age_ctrl_mode:1;
	a_uint32_t  failover_en:1;
	a_uint32_t  service_code_loop:1;
	a_uint32_t  flow_cpy_escape:1;
	a_uint32_t  mc_pvlan_isol_en:1;
	a_uint32_t  bc_pvlan_isol_en:1;
	a_uint32_t  ipmc_en:1;
	a_uint32_t  ipmc_hash_mode_0:2;
	a_uint32_t  ipmc_hash_mode_1:2;
	a_uint32_t  mc_vlan_match_mode:1;
	a_uint32_t  mc_dmac_check_en:1;
	a_uint32_t  ipmc_mismatch_act:2;
	a_uint32_t  dot1p_mapper_vlan_mode:2;
	a_uint32_t  dot1p_mapper_pcp_mode:2;
	a_uint32_t  _reserved0:4;
};

union l2_global_conf_u {
	a_uint32_t val;
	struct l2_global_conf bf;
};

/*[register] FDB_TBL_RD_OP_RSLT_DATA0*/
#define FDB_TBL_RD_OP_RSLT_DATA0
#define FDB_TBL_RD_OP_RSLT_DATA0_ADDRESS 0x200
#define FDB_TBL_RD_OP_RSLT_DATA0_NUM     1
#define FDB_TBL_RD_OP_RSLT_DATA0_INC     0x4
#define FDB_TBL_RD_OP_RSLT_DATA0_TYPE    REG_TYPE_RO
#define FDB_TBL_RD_OP_RSLT_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_RSLT_DATA0_DATA
	#define FDB_TBL_RD_OP_RSLT_DATA0_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_RSLT_DATA0_DATA_LEN     32
	#define FDB_TBL_RD_OP_RSLT_DATA0_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_rslt_data0 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_rslt_data0_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_rslt_data0 bf;
};

/*[register] FDB_TBL_RD_OP_RSLT_DATA1*/
#define FDB_TBL_RD_OP_RSLT_DATA1
#define FDB_TBL_RD_OP_RSLT_DATA1_ADDRESS 0x204
#define FDB_TBL_RD_OP_RSLT_DATA1_NUM     1
#define FDB_TBL_RD_OP_RSLT_DATA1_INC     0x4
#define FDB_TBL_RD_OP_RSLT_DATA1_TYPE    REG_TYPE_RO
#define FDB_TBL_RD_OP_RSLT_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_RSLT_DATA1_DATA
	#define FDB_TBL_RD_OP_RSLT_DATA1_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_RSLT_DATA1_DATA_LEN     32
	#define FDB_TBL_RD_OP_RSLT_DATA1_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_rslt_data1 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_rslt_data1_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_rslt_data1 bf;
};

/*[register] FDB_TBL_RD_OP_RSLT_DATA2*/
#define FDB_TBL_RD_OP_RSLT_DATA2
#define FDB_TBL_RD_OP_RSLT_DATA2_ADDRESS 0x208
#define FDB_TBL_RD_OP_RSLT_DATA2_NUM     1
#define FDB_TBL_RD_OP_RSLT_DATA2_INC     0x4
#define FDB_TBL_RD_OP_RSLT_DATA2_TYPE    REG_TYPE_RO
#define FDB_TBL_RD_OP_RSLT_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_RSLT_DATA2_DATA
	#define FDB_TBL_RD_OP_RSLT_DATA2_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_RSLT_DATA2_DATA_LEN     32
	#define FDB_TBL_RD_OP_RSLT_DATA2_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_rslt_data2 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_rslt_data2_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_rslt_data2 bf;
};

/*[register] FDB_TBL_OP_DATA0*/
#define FDB_TBL_OP_DATA0
#define FDB_TBL_OP_DATA0_ADDRESS 0x230
#define FDB_TBL_OP_DATA0_NUM     1
#define FDB_TBL_OP_DATA0_INC     0x4
#define FDB_TBL_OP_DATA0_TYPE    REG_TYPE_RW
#define FDB_TBL_OP_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_OP_DATA0_DATA
	#define FDB_TBL_OP_DATA0_DATA_OFFSET  0
	#define FDB_TBL_OP_DATA0_DATA_LEN     32
	#define FDB_TBL_OP_DATA0_DATA_DEFAULT 0x0

struct fdb_tbl_op_data0 {
	a_uint32_t  data:32;
};

union fdb_tbl_op_data0_u {
	a_uint32_t val;
	struct fdb_tbl_op_data0 bf;
};

/*[register] FDB_TBL_OP_DATA1*/
#define FDB_TBL_OP_DATA1
#define FDB_TBL_OP_DATA1_ADDRESS 0x234
#define FDB_TBL_OP_DATA1_NUM     1
#define FDB_TBL_OP_DATA1_INC     0x4
#define FDB_TBL_OP_DATA1_TYPE    REG_TYPE_RW
#define FDB_TBL_OP_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_OP_DATA1_DATA
	#define FDB_TBL_OP_DATA1_DATA_OFFSET  0
	#define FDB_TBL_OP_DATA1_DATA_LEN     32
	#define FDB_TBL_OP_DATA1_DATA_DEFAULT 0x0

struct fdb_tbl_op_data1 {
	a_uint32_t  data:32;
};

union fdb_tbl_op_data1_u {
	a_uint32_t val;
	struct fdb_tbl_op_data1 bf;
};

/*[register] FDB_TBL_OP_DATA2*/
#define FDB_TBL_OP_DATA2
#define FDB_TBL_OP_DATA2_ADDRESS 0x238
#define FDB_TBL_OP_DATA2_NUM     1
#define FDB_TBL_OP_DATA2_INC     0x4
#define FDB_TBL_OP_DATA2_TYPE    REG_TYPE_RW
#define FDB_TBL_OP_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_OP_DATA2_DATA
	#define FDB_TBL_OP_DATA2_DATA_OFFSET  0
	#define FDB_TBL_OP_DATA2_DATA_LEN     32
	#define FDB_TBL_OP_DATA2_DATA_DEFAULT 0x0

struct fdb_tbl_op_data2 {
	a_uint32_t  data:32;
};

union fdb_tbl_op_data2_u {
	a_uint32_t val;
	struct fdb_tbl_op_data2 bf;
};

/*[register] FDB_TBL_RD_OP_DATA0*/
#define FDB_TBL_RD_OP_DATA0
#define FDB_TBL_RD_OP_DATA0_ADDRESS 0x260
#define FDB_TBL_RD_OP_DATA0_NUM     1
#define FDB_TBL_RD_OP_DATA0_INC     0x4
#define FDB_TBL_RD_OP_DATA0_TYPE    REG_TYPE_RW
#define FDB_TBL_RD_OP_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_DATA0_DATA
	#define FDB_TBL_RD_OP_DATA0_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_DATA0_DATA_LEN     32
	#define FDB_TBL_RD_OP_DATA0_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_data0 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_data0_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_data0 bf;
};

/*[register] FDB_TBL_RD_OP_DATA1*/
#define FDB_TBL_RD_OP_DATA1
#define FDB_TBL_RD_OP_DATA1_ADDRESS 0x264
#define FDB_TBL_RD_OP_DATA1_NUM     1
#define FDB_TBL_RD_OP_DATA1_INC     0x4
#define FDB_TBL_RD_OP_DATA1_TYPE    REG_TYPE_RW
#define FDB_TBL_RD_OP_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_DATA1_DATA
	#define FDB_TBL_RD_OP_DATA1_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_DATA1_DATA_LEN     32
	#define FDB_TBL_RD_OP_DATA1_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_data1 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_data1_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_data1 bf;
};

/*[register] FDB_TBL_RD_OP_DATA2*/
#define FDB_TBL_RD_OP_DATA2
#define FDB_TBL_RD_OP_DATA2_ADDRESS 0x268
#define FDB_TBL_RD_OP_DATA2_NUM     1
#define FDB_TBL_RD_OP_DATA2_INC     0x4
#define FDB_TBL_RD_OP_DATA2_TYPE    REG_TYPE_RW
#define FDB_TBL_RD_OP_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define FDB_TBL_RD_OP_DATA2_DATA
	#define FDB_TBL_RD_OP_DATA2_DATA_OFFSET  0
	#define FDB_TBL_RD_OP_DATA2_DATA_LEN     32
	#define FDB_TBL_RD_OP_DATA2_DATA_DEFAULT 0x0

struct fdb_tbl_rd_op_data2 {
	a_uint32_t  data:32;
};

union fdb_tbl_rd_op_data2_u {
	a_uint32_t val;
	struct fdb_tbl_rd_op_data2 bf;
};

/*[register] PORT_BRIDGE_CTRL*/
#define PORT_BRIDGE_CTRL
#define PORT_BRIDGE_CTRL_ADDRESS 0x300
#define PORT_BRIDGE_CTRL_NUM     9
#define PORT_BRIDGE_CTRL_INC     0x4
#define PORT_BRIDGE_CTRL_TYPE    REG_TYPE_RW
#define PORT_BRIDGE_CTRL_DEFAULT 0x5ff09
	/*[field] NEW_ADDR_LRN_EN*/
	#define PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN
	#define PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN_OFFSET  0
	#define PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN_LEN     1
	#define PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN_DEFAULT 0x1
	/*[field] NEW_ADDR_FWD_CMD*/
	#define PORT_BRIDGE_CTRL_NEW_ADDR_FWD_CMD
	#define PORT_BRIDGE_CTRL_NEW_ADDR_FWD_CMD_OFFSET  1
	#define PORT_BRIDGE_CTRL_NEW_ADDR_FWD_CMD_LEN     2
	#define PORT_BRIDGE_CTRL_NEW_ADDR_FWD_CMD_DEFAULT 0x0
	/*[field] STATION_MOVE_LRN_EN*/
	#define PORT_BRIDGE_CTRL_STATION_MOVE_LRN_EN
	#define PORT_BRIDGE_CTRL_STATION_MOVE_LRN_EN_OFFSET  3
	#define PORT_BRIDGE_CTRL_STATION_MOVE_LRN_EN_LEN     1
	#define PORT_BRIDGE_CTRL_STATION_MOVE_LRN_EN_DEFAULT 0x1
	/*[field] STATION_MOVE_FWD_CMD*/
	#define PORT_BRIDGE_CTRL_STATION_MOVE_FWD_CMD
	#define PORT_BRIDGE_CTRL_STATION_MOVE_FWD_CMD_OFFSET  4
	#define PORT_BRIDGE_CTRL_STATION_MOVE_FWD_CMD_LEN     2
	#define PORT_BRIDGE_CTRL_STATION_MOVE_FWD_CMD_DEFAULT 0x0
	/*[field] PORT_ISOLATION_BITMAP*/
	#define PORT_BRIDGE_CTRL_PORT_ISOLATION_BITMAP
	#define PORT_BRIDGE_CTRL_PORT_ISOLATION_BITMAP_OFFSET  8
	#define PORT_BRIDGE_CTRL_PORT_ISOLATION_BITMAP_LEN     9
	#define PORT_BRIDGE_CTRL_PORT_ISOLATION_BITMAP_DEFAULT 0x1ff
	/*[field] TXMAC_EN*/
	#define PORT_BRIDGE_CTRL_TXMAC_EN
	#define PORT_BRIDGE_CTRL_TXMAC_EN_OFFSET  17
	#define PORT_BRIDGE_CTRL_TXMAC_EN_LEN     1
	#define PORT_BRIDGE_CTRL_TXMAC_EN_DEFAULT 0x0
	/*[field] PROMISC_EN*/
	#define PORT_BRIDGE_CTRL_PROMISC_EN
	#define PORT_BRIDGE_CTRL_PROMISC_EN_OFFSET  18
	#define PORT_BRIDGE_CTRL_PROMISC_EN_LEN     1
	#define PORT_BRIDGE_CTRL_PROMISC_EN_DEFAULT 0x1

struct port_bridge_ctrl {
	a_uint32_t  new_addr_lrn_en:1;
	a_uint32_t  new_addr_fwd_cmd:2;
	a_uint32_t  station_move_lrn_en:1;
	a_uint32_t  station_move_fwd_cmd:2;
	a_uint32_t  _reserved0:2;
	a_uint32_t  port_isolation_bitmap:9;
	a_uint32_t  txmac_en:1;
	a_uint32_t  promisc_en:1;
	a_uint32_t  _reserved1:13;
};

union port_bridge_ctrl_u {
	a_uint32_t val;
	struct port_bridge_ctrl bf;
};

/*[register] PORT_LRN_LIMIT_CTRL*/
#define PORT_LRN_LIMIT_CTRL
#define PORT_LRN_LIMIT_CTRL_ADDRESS 0x400
#define PORT_LRN_LIMIT_CTRL_NUM     9
#define PORT_LRN_LIMIT_CTRL_INC     0x4
#define PORT_LRN_LIMIT_CTRL_TYPE    REG_TYPE_RW
#define PORT_LRN_LIMIT_CTRL_DEFAULT 0x1800
	/*[field] LRN_LMT_CNT*/
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_CNT
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_CNT_OFFSET  0
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_CNT_LEN     12
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_CNT_DEFAULT 0x800
	/*[field] LRN_LMT_EN*/
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EN
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EN_OFFSET  12
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EN_LEN     1
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EN_DEFAULT 0x1
	/*[field] LRN_LMT_EXCEED_FWD*/
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EXCEED_FWD
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EXCEED_FWD_OFFSET  13
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EXCEED_FWD_LEN     2
	#define PORT_LRN_LIMIT_CTRL_LRN_LMT_EXCEED_FWD_DEFAULT 0x0

struct port_lrn_limit_ctrl {
	a_uint32_t  lrn_lmt_cnt:12;
	a_uint32_t  lrn_lmt_en:1;
	a_uint32_t  lrn_lmt_exceed_fwd:2;
	a_uint32_t  _reserved0:17;
};

union port_lrn_limit_ctrl_u {
	a_uint32_t val;
	struct port_lrn_limit_ctrl bf;
};

/*[register] PORT_LRN_LIMIT_COUNTER*/
#define PORT_LRN_LIMIT_COUNTER
#define PORT_LRN_LIMIT_COUNTER_ADDRESS 0x500
#define PORT_LRN_LIMIT_COUNTER_NUM     9
#define PORT_LRN_LIMIT_COUNTER_INC     0x4
#define PORT_LRN_LIMIT_COUNTER_TYPE    REG_TYPE_RO
#define PORT_LRN_LIMIT_COUNTER_DEFAULT 0x0
	/*[field] LRN_CNT*/
	#define PORT_LRN_LIMIT_COUNTER_LRN_CNT
	#define PORT_LRN_LIMIT_COUNTER_LRN_CNT_OFFSET  0
	#define PORT_LRN_LIMIT_COUNTER_LRN_CNT_LEN     12
	#define PORT_LRN_LIMIT_COUNTER_LRN_CNT_DEFAULT 0x0

struct port_lrn_limit_counter {
	a_uint32_t  lrn_cnt:12;
	a_uint32_t  _reserved0:20;
};

union port_lrn_limit_counter_u {
	a_uint32_t val;
	struct port_lrn_limit_counter bf;
};

/*[table] RFDB_TBL*/
#define RFDB_TBL
#define RFDB_TBL_ADDRESS 0x3000
#define RFDB_TBL_NUM     32
#define RFDB_TBL_INC     0x8
#define RFDB_TBL_TYPE    REG_TYPE_RW
#define RFDB_TBL_DEFAULT 0x0
	/*[field] MAC_ADDR*/
	#define RFDB_TBL_MAC_ADDR
	#define RFDB_TBL_MAC_ADDR_OFFSET  0
	#define RFDB_TBL_MAC_ADDR_LEN     48
	#define RFDB_TBL_MAC_ADDR_DEFAULT 0x0
	/*[field] VALID*/
	#define RFDB_TBL_VALID
	#define RFDB_TBL_VALID_OFFSET  48
	#define RFDB_TBL_VALID_LEN     1
	#define RFDB_TBL_VALID_DEFAULT 0x0

struct rfdb_tbl {
	a_uint32_t  mac_addr_0:32;
	a_uint32_t  mac_addr_1:16;
	a_uint32_t  valid:1;
	a_uint32_t  _reserved0:15;
};

union rfdb_tbl_u {
	a_uint32_t val[2];
	struct rfdb_tbl bf;
};

/*[table] FDB_TBL*/
#define FDB_TBL
#define FDB_TBL_ADDRESS 0x10000
#define FDB_TBL_NUM     2048
#define FDB_TBL_INC     0x10
#define FDB_TBL_TYPE    REG_TYPE_RW
#define FDB_TBL_DEFAULT 0x0
	/*[field] MAC_ADDR*/
	#define FDB_TBL_MAC_ADDR
	#define FDB_TBL_MAC_ADDR_OFFSET  0
	#define FDB_TBL_MAC_ADDR_LEN     48
	#define FDB_TBL_MAC_ADDR_DEFAULT 0x0
	/*[field] ENTRY_VALID*/
	#define FDB_TBL_ENTRY_VALID
	#define FDB_TBL_ENTRY_VALID_OFFSET  48
	#define FDB_TBL_ENTRY_VALID_LEN     1
	#define FDB_TBL_ENTRY_VALID_DEFAULT 0x0
	/*[field] LOOKUP_VALID*/
	#define FDB_TBL_LOOKUP_VALID
	#define FDB_TBL_LOOKUP_VALID_OFFSET  49
	#define FDB_TBL_LOOKUP_VALID_LEN     1
	#define FDB_TBL_LOOKUP_VALID_DEFAULT 0x0
	/*[field] VSI*/
	#define FDB_TBL_VSI
	#define FDB_TBL_VSI_OFFSET  50
	#define FDB_TBL_VSI_LEN     6
	#define FDB_TBL_VSI_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define FDB_TBL_DST_INFO
	#define FDB_TBL_DST_INFO_OFFSET  56
	#define FDB_TBL_DST_INFO_LEN     14
	#define FDB_TBL_DST_INFO_DEFAULT 0x0
	/*[field] SA_CMD*/
	#define FDB_TBL_SA_CMD
	#define FDB_TBL_SA_CMD_OFFSET  70
	#define FDB_TBL_SA_CMD_LEN     2
	#define FDB_TBL_SA_CMD_DEFAULT 0x0
	/*[field] DA_CMD*/
	#define FDB_TBL_DA_CMD
	#define FDB_TBL_DA_CMD_OFFSET  72
	#define FDB_TBL_DA_CMD_LEN     2
	#define FDB_TBL_DA_CMD_DEFAULT 0x0
	/*[field] HIT_AGE*/
	#define FDB_TBL_HIT_AGE
	#define FDB_TBL_HIT_AGE_OFFSET  74
	#define FDB_TBL_HIT_AGE_LEN     2
	#define FDB_TBL_HIT_AGE_DEFAULT 0x0

struct fdb_tbl {
	a_uint32_t  mac_addr_0:32;
	a_uint32_t  mac_addr_1:16;
	a_uint32_t  entry_valid:1;
	a_uint32_t  lookup_valid:1;
	a_uint32_t  vsi:6;
	a_uint32_t  dst_info_0:8;
	a_uint32_t  dst_info_1:6;
	a_uint32_t  sa_cmd:2;
	a_uint32_t  da_cmd:2;
	a_uint32_t  hit_age:2;
	a_uint32_t  _reserved0:20;
};

union fdb_tbl_u {
	a_uint32_t val[3];
	struct fdb_tbl bf;
};

#endif
