/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_IPMC_REG_H_
#define _HTTPPE_IPMC_REG_H_

/*[register] IPMC_TBL_OP*/
#define IPMC_TBL_OP
#define IPMC_TBL_OP_ADDRESS 0x150
#define IPMC_TBL_OP_NUM     1
#define IPMC_TBL_OP_INC     0x4
#define IPMC_TBL_OP_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define IPMC_TBL_OP_CMD_ID
	#define IPMC_TBL_OP_CMD_ID_OFFSET  0
	#define IPMC_TBL_OP_CMD_ID_LEN     4
	#define IPMC_TBL_OP_CMD_ID_DEFAULT 0x0
	/*[field] BYP_RSLT_EN*/
	#define IPMC_TBL_OP_BYP_RSLT_EN
	#define IPMC_TBL_OP_BYP_RSLT_EN_OFFSET  4
	#define IPMC_TBL_OP_BYP_RSLT_EN_LEN     1
	#define IPMC_TBL_OP_BYP_RSLT_EN_DEFAULT 0x0
	/*[field] OP_TYPE*/
	#define IPMC_TBL_OP_OP_TYPE
	#define IPMC_TBL_OP_OP_TYPE_OFFSET  5
	#define IPMC_TBL_OP_OP_TYPE_LEN     3
	#define IPMC_TBL_OP_OP_TYPE_DEFAULT 0x0
	/*[field] HASH_BLOCK_BITMAP*/
	#define IPMC_TBL_OP_HASH_BLOCK_BITMAP
	#define IPMC_TBL_OP_HASH_BLOCK_BITMAP_OFFSET  8
	#define IPMC_TBL_OP_HASH_BLOCK_BITMAP_LEN     2
	#define IPMC_TBL_OP_HASH_BLOCK_BITMAP_DEFAULT 0x0
	/*[field] OP_MODE*/
	#define IPMC_TBL_OP_OP_MODE
	#define IPMC_TBL_OP_OP_MODE_OFFSET  10
	#define IPMC_TBL_OP_OP_MODE_LEN     1
	#define IPMC_TBL_OP_OP_MODE_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define IPMC_TBL_OP_ENTRY_INDEX
	#define IPMC_TBL_OP_ENTRY_INDEX_OFFSET  11
	#define IPMC_TBL_OP_ENTRY_INDEX_LEN     11
	#define IPMC_TBL_OP_ENTRY_INDEX_DEFAULT 0x0

struct ipmc_tbl_op {
	a_uint32_t  cmd_id:4;
	a_uint32_t  byp_rslt_en:1;
	a_uint32_t  op_type:3;
	a_uint32_t  hash_block_bitmap:2;
	a_uint32_t  op_mode:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:10;
};

union ipmc_tbl_op_u {
	a_uint32_t val;
	struct ipmc_tbl_op bf;
};

/*[register] IPMC_TBL_OP_DATA0*/
#define IPMC_TBL_OP_DATA0
#define IPMC_TBL_OP_DATA0_ADDRESS 0x154
#define IPMC_TBL_OP_DATA0_NUM     1
#define IPMC_TBL_OP_DATA0_INC     0x4
#define IPMC_TBL_OP_DATA0_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA0_DATA
	#define IPMC_TBL_OP_DATA0_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA0_DATA_LEN     32
	#define IPMC_TBL_OP_DATA0_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data0 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data0_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data0 bf;
};

/*[register] IPMC_TBL_OP_DATA1*/
#define IPMC_TBL_OP_DATA1
#define IPMC_TBL_OP_DATA1_ADDRESS 0x158
#define IPMC_TBL_OP_DATA1_NUM     1
#define IPMC_TBL_OP_DATA1_INC     0x4
#define IPMC_TBL_OP_DATA1_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA1_DATA
	#define IPMC_TBL_OP_DATA1_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA1_DATA_LEN     32
	#define IPMC_TBL_OP_DATA1_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data1 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data1_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data1 bf;
};

/*[register] IPMC_TBL_OP_DATA2*/
#define IPMC_TBL_OP_DATA2
#define IPMC_TBL_OP_DATA2_ADDRESS 0x15c
#define IPMC_TBL_OP_DATA2_NUM     1
#define IPMC_TBL_OP_DATA2_INC     0x4
#define IPMC_TBL_OP_DATA2_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA2_DATA
	#define IPMC_TBL_OP_DATA2_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA2_DATA_LEN     32
	#define IPMC_TBL_OP_DATA2_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data2 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data2_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data2 bf;
};

/*[register] IPMC_TBL_OP_DATA3*/
#define IPMC_TBL_OP_DATA3
#define IPMC_TBL_OP_DATA3_ADDRESS 0x160
#define IPMC_TBL_OP_DATA3_NUM     1
#define IPMC_TBL_OP_DATA3_INC     0x4
#define IPMC_TBL_OP_DATA3_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA3_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA3_DATA
	#define IPMC_TBL_OP_DATA3_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA3_DATA_LEN     32
	#define IPMC_TBL_OP_DATA3_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data3 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data3_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data3 bf;
};

/*[register] IPMC_TBL_OP_DATA4*/
#define IPMC_TBL_OP_DATA4
#define IPMC_TBL_OP_DATA4_ADDRESS 0x164
#define IPMC_TBL_OP_DATA4_NUM     1
#define IPMC_TBL_OP_DATA4_INC     0x4
#define IPMC_TBL_OP_DATA4_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA4_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA4_DATA
	#define IPMC_TBL_OP_DATA4_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA4_DATA_LEN     32
	#define IPMC_TBL_OP_DATA4_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data4 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data4_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data4 bf;
};

/*[register] IPMC_TBL_OP_DATA5*/
#define IPMC_TBL_OP_DATA5
#define IPMC_TBL_OP_DATA5_ADDRESS 0x168
#define IPMC_TBL_OP_DATA5_NUM     1
#define IPMC_TBL_OP_DATA5_INC     0x4
#define IPMC_TBL_OP_DATA5_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA5_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA5_DATA
	#define IPMC_TBL_OP_DATA5_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA5_DATA_LEN     32
	#define IPMC_TBL_OP_DATA5_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data5 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data5_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data5 bf;
};

/*[register] IPMC_TBL_OP_DATA6*/
#define IPMC_TBL_OP_DATA6
#define IPMC_TBL_OP_DATA6_ADDRESS 0x16c
#define IPMC_TBL_OP_DATA6_NUM     1
#define IPMC_TBL_OP_DATA6_INC     0x4
#define IPMC_TBL_OP_DATA6_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA6_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA6_DATA
	#define IPMC_TBL_OP_DATA6_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA6_DATA_LEN     32
	#define IPMC_TBL_OP_DATA6_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data6 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data6_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data6 bf;
};

/*[register] IPMC_TBL_OP_DATA7*/
#define IPMC_TBL_OP_DATA7
#define IPMC_TBL_OP_DATA7_ADDRESS 0x170
#define IPMC_TBL_OP_DATA7_NUM     1
#define IPMC_TBL_OP_DATA7_INC     0x4
#define IPMC_TBL_OP_DATA7_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA7_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA7_DATA
	#define IPMC_TBL_OP_DATA7_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA7_DATA_LEN     32
	#define IPMC_TBL_OP_DATA7_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data7 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data7_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data7 bf;
};

/*[register] IPMC_TBL_OP_DATA8*/
#define IPMC_TBL_OP_DATA8
#define IPMC_TBL_OP_DATA8_ADDRESS 0x174
#define IPMC_TBL_OP_DATA8_NUM     1
#define IPMC_TBL_OP_DATA8_INC     0x4
#define IPMC_TBL_OP_DATA8_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA8_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA8_DATA
	#define IPMC_TBL_OP_DATA8_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA8_DATA_LEN     32
	#define IPMC_TBL_OP_DATA8_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data8 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data8_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data8 bf;
};

/*[register] IPMC_TBL_OP_DATA9*/
#define IPMC_TBL_OP_DATA9
#define IPMC_TBL_OP_DATA9_ADDRESS 0x178
#define IPMC_TBL_OP_DATA9_NUM     1
#define IPMC_TBL_OP_DATA9_INC     0x4
#define IPMC_TBL_OP_DATA9_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA9_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA9_DATA
	#define IPMC_TBL_OP_DATA9_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA9_DATA_LEN     32
	#define IPMC_TBL_OP_DATA9_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data9 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data9_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data9 bf;
};

/*[register] IPMC_TBL_OP_DATA10*/
#define IPMC_TBL_OP_DATA10
#define IPMC_TBL_OP_DATA10_ADDRESS 0x17c
#define IPMC_TBL_OP_DATA10_NUM     1
#define IPMC_TBL_OP_DATA10_INC     0x4
#define IPMC_TBL_OP_DATA10_TYPE    REG_TYPE_RW
#define IPMC_TBL_OP_DATA10_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_OP_DATA10_DATA
	#define IPMC_TBL_OP_DATA10_DATA_OFFSET  0
	#define IPMC_TBL_OP_DATA10_DATA_LEN     32
	#define IPMC_TBL_OP_DATA10_DATA_DEFAULT 0x0

struct ipmc_tbl_op_data10 {
	a_uint32_t  data:32;
};

union ipmc_tbl_op_data10_u {
	a_uint32_t val;
	struct ipmc_tbl_op_data10 bf;
};

/*[register] IPMC_TBL_OP_RSLT*/
#define IPMC_TBL_OP_RSLT
#define IPMC_TBL_OP_RSLT_ADDRESS 0x180
#define IPMC_TBL_OP_RSLT_NUM     1
#define IPMC_TBL_OP_RSLT_INC     0x4
#define IPMC_TBL_OP_RSLT_TYPE    REG_TYPE_RO
#define IPMC_TBL_OP_RSLT_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define IPMC_TBL_OP_RSLT_CMD_ID
	#define IPMC_TBL_OP_RSLT_CMD_ID_OFFSET  0
	#define IPMC_TBL_OP_RSLT_CMD_ID_LEN     4
	#define IPMC_TBL_OP_RSLT_CMD_ID_DEFAULT 0x0
	/*[field] OP_RSLT*/
	#define IPMC_TBL_OP_RSLT_OP_RSLT
	#define IPMC_TBL_OP_RSLT_OP_RSLT_OFFSET  4
	#define IPMC_TBL_OP_RSLT_OP_RSLT_LEN     1
	#define IPMC_TBL_OP_RSLT_OP_RSLT_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define IPMC_TBL_OP_RSLT_ENTRY_INDEX
	#define IPMC_TBL_OP_RSLT_ENTRY_INDEX_OFFSET  5
	#define IPMC_TBL_OP_RSLT_ENTRY_INDEX_LEN     11
	#define IPMC_TBL_OP_RSLT_ENTRY_INDEX_DEFAULT 0x0
	/*[field] VALID_CNT*/
	#define IPMC_TBL_OP_RSLT_VALID_CNT
	#define IPMC_TBL_OP_RSLT_VALID_CNT_OFFSET  16
	#define IPMC_TBL_OP_RSLT_VALID_CNT_LEN     4
	#define IPMC_TBL_OP_RSLT_VALID_CNT_DEFAULT 0x0

struct ipmc_tbl_op_rslt {
	a_uint32_t  cmd_id:4;
	a_uint32_t  op_rslt:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  valid_cnt:4;
	a_uint32_t  _reserved0:12;
};

union ipmc_tbl_op_rslt_u {
	a_uint32_t val;
	struct ipmc_tbl_op_rslt bf;
};

/*[register] IPMC_TBL_RD_OP*/
#define IPMC_TBL_RD_OP
#define IPMC_TBL_RD_OP_ADDRESS 0x184
#define IPMC_TBL_RD_OP_NUM     1
#define IPMC_TBL_RD_OP_INC     0x4
#define IPMC_TBL_RD_OP_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define IPMC_TBL_RD_OP_CMD_ID
	#define IPMC_TBL_RD_OP_CMD_ID_OFFSET  0
	#define IPMC_TBL_RD_OP_CMD_ID_LEN     4
	#define IPMC_TBL_RD_OP_CMD_ID_DEFAULT 0x0
	/*[field] BYP_RSLT_EN*/
	#define IPMC_TBL_RD_OP_BYP_RSLT_EN
	#define IPMC_TBL_RD_OP_BYP_RSLT_EN_OFFSET  4
	#define IPMC_TBL_RD_OP_BYP_RSLT_EN_LEN     1
	#define IPMC_TBL_RD_OP_BYP_RSLT_EN_DEFAULT 0x0
	/*[field] OP_TYPE*/
	#define IPMC_TBL_RD_OP_OP_TYPE
	#define IPMC_TBL_RD_OP_OP_TYPE_OFFSET  5
	#define IPMC_TBL_RD_OP_OP_TYPE_LEN     3
	#define IPMC_TBL_RD_OP_OP_TYPE_DEFAULT 0x0
	/*[field] HASH_BLOCK_BITMAP*/
	#define IPMC_TBL_RD_OP_HASH_BLOCK_BITMAP
	#define IPMC_TBL_RD_OP_HASH_BLOCK_BITMAP_OFFSET  8
	#define IPMC_TBL_RD_OP_HASH_BLOCK_BITMAP_LEN     2
	#define IPMC_TBL_RD_OP_HASH_BLOCK_BITMAP_DEFAULT 0x0
	/*[field] OP_MODE*/
	#define IPMC_TBL_RD_OP_OP_MODE
	#define IPMC_TBL_RD_OP_OP_MODE_OFFSET  10
	#define IPMC_TBL_RD_OP_OP_MODE_LEN     1
	#define IPMC_TBL_RD_OP_OP_MODE_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define IPMC_TBL_RD_OP_ENTRY_INDEX
	#define IPMC_TBL_RD_OP_ENTRY_INDEX_OFFSET  11
	#define IPMC_TBL_RD_OP_ENTRY_INDEX_LEN     11
	#define IPMC_TBL_RD_OP_ENTRY_INDEX_DEFAULT 0x0

struct ipmc_tbl_rd_op {
	a_uint32_t  cmd_id:4;
	a_uint32_t  byp_rslt_en:1;
	a_uint32_t  op_type:3;
	a_uint32_t  hash_block_bitmap:2;
	a_uint32_t  op_mode:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  _reserved0:10;
};

union ipmc_tbl_rd_op_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op bf;
};

/*[register] IPMC_TBL_RD_OP_DATA0*/
#define IPMC_TBL_RD_OP_DATA0
#define IPMC_TBL_RD_OP_DATA0_ADDRESS 0x188
#define IPMC_TBL_RD_OP_DATA0_NUM     1
#define IPMC_TBL_RD_OP_DATA0_INC     0x4
#define IPMC_TBL_RD_OP_DATA0_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA0_DATA
	#define IPMC_TBL_RD_OP_DATA0_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA0_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA0_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data0 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data0_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data0 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA1*/
#define IPMC_TBL_RD_OP_DATA1
#define IPMC_TBL_RD_OP_DATA1_ADDRESS 0x18c
#define IPMC_TBL_RD_OP_DATA1_NUM     1
#define IPMC_TBL_RD_OP_DATA1_INC     0x4
#define IPMC_TBL_RD_OP_DATA1_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA1_DATA
	#define IPMC_TBL_RD_OP_DATA1_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA1_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA1_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data1 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data1_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data1 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA2*/
#define IPMC_TBL_RD_OP_DATA2
#define IPMC_TBL_RD_OP_DATA2_ADDRESS 0x190
#define IPMC_TBL_RD_OP_DATA2_NUM     1
#define IPMC_TBL_RD_OP_DATA2_INC     0x4
#define IPMC_TBL_RD_OP_DATA2_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA2_DATA
	#define IPMC_TBL_RD_OP_DATA2_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA2_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA2_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data2 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data2_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data2 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA3*/
#define IPMC_TBL_RD_OP_DATA3
#define IPMC_TBL_RD_OP_DATA3_ADDRESS 0x194
#define IPMC_TBL_RD_OP_DATA3_NUM     1
#define IPMC_TBL_RD_OP_DATA3_INC     0x4
#define IPMC_TBL_RD_OP_DATA3_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA3_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA3_DATA
	#define IPMC_TBL_RD_OP_DATA3_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA3_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA3_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data3 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data3_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data3 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA4*/
#define IPMC_TBL_RD_OP_DATA4
#define IPMC_TBL_RD_OP_DATA4_ADDRESS 0x198
#define IPMC_TBL_RD_OP_DATA4_NUM     1
#define IPMC_TBL_RD_OP_DATA4_INC     0x4
#define IPMC_TBL_RD_OP_DATA4_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA4_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA4_DATA
	#define IPMC_TBL_RD_OP_DATA4_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA4_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA4_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data4 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data4_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data4 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA5*/
#define IPMC_TBL_RD_OP_DATA5
#define IPMC_TBL_RD_OP_DATA5_ADDRESS 0x19c
#define IPMC_TBL_RD_OP_DATA5_NUM     1
#define IPMC_TBL_RD_OP_DATA5_INC     0x4
#define IPMC_TBL_RD_OP_DATA5_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA5_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA5_DATA
	#define IPMC_TBL_RD_OP_DATA5_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA5_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA5_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data5 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data5_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data5 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA6*/
#define IPMC_TBL_RD_OP_DATA6
#define IPMC_TBL_RD_OP_DATA6_ADDRESS 0x1a0
#define IPMC_TBL_RD_OP_DATA6_NUM     1
#define IPMC_TBL_RD_OP_DATA6_INC     0x4
#define IPMC_TBL_RD_OP_DATA6_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA6_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA6_DATA
	#define IPMC_TBL_RD_OP_DATA6_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA6_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA6_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data6 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data6_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data6 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA7*/
#define IPMC_TBL_RD_OP_DATA7
#define IPMC_TBL_RD_OP_DATA7_ADDRESS 0x1a4
#define IPMC_TBL_RD_OP_DATA7_NUM     1
#define IPMC_TBL_RD_OP_DATA7_INC     0x4
#define IPMC_TBL_RD_OP_DATA7_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA7_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA7_DATA
	#define IPMC_TBL_RD_OP_DATA7_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA7_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA7_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data7 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data7_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data7 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA8*/
#define IPMC_TBL_RD_OP_DATA8
#define IPMC_TBL_RD_OP_DATA8_ADDRESS 0x1a8
#define IPMC_TBL_RD_OP_DATA8_NUM     1
#define IPMC_TBL_RD_OP_DATA8_INC     0x4
#define IPMC_TBL_RD_OP_DATA8_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA8_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA8_DATA
	#define IPMC_TBL_RD_OP_DATA8_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA8_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA8_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data8 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data8_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data8 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA9*/
#define IPMC_TBL_RD_OP_DATA9
#define IPMC_TBL_RD_OP_DATA9_ADDRESS 0x1ac
#define IPMC_TBL_RD_OP_DATA9_NUM     1
#define IPMC_TBL_RD_OP_DATA9_INC     0x4
#define IPMC_TBL_RD_OP_DATA9_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA9_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA9_DATA
	#define IPMC_TBL_RD_OP_DATA9_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA9_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA9_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data9 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data9_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data9 bf;
};

/*[register] IPMC_TBL_RD_OP_DATA10*/
#define IPMC_TBL_RD_OP_DATA10
#define IPMC_TBL_RD_OP_DATA10_ADDRESS 0x1b0
#define IPMC_TBL_RD_OP_DATA10_NUM     1
#define IPMC_TBL_RD_OP_DATA10_INC     0x4
#define IPMC_TBL_RD_OP_DATA10_TYPE    REG_TYPE_RW
#define IPMC_TBL_RD_OP_DATA10_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_OP_DATA10_DATA
	#define IPMC_TBL_RD_OP_DATA10_DATA_OFFSET  0
	#define IPMC_TBL_RD_OP_DATA10_DATA_LEN     32
	#define IPMC_TBL_RD_OP_DATA10_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_op_data10 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_op_data10_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_data10 bf;
};

/*[register] IPMC_TBL_RD_OP_RSLT*/
#define IPMC_TBL_RD_OP_RSLT
#define IPMC_TBL_RD_OP_RSLT_ADDRESS 0x1b4
#define IPMC_TBL_RD_OP_RSLT_NUM     1
#define IPMC_TBL_RD_OP_RSLT_INC     0x4
#define IPMC_TBL_RD_OP_RSLT_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_OP_RSLT_DEFAULT 0x0
	/*[field] CMD_ID*/
	#define IPMC_TBL_RD_OP_RSLT_CMD_ID
	#define IPMC_TBL_RD_OP_RSLT_CMD_ID_OFFSET  0
	#define IPMC_TBL_RD_OP_RSLT_CMD_ID_LEN     4
	#define IPMC_TBL_RD_OP_RSLT_CMD_ID_DEFAULT 0x0
	/*[field] OP_RSLT*/
	#define IPMC_TBL_RD_OP_RSLT_OP_RSLT
	#define IPMC_TBL_RD_OP_RSLT_OP_RSLT_OFFSET  4
	#define IPMC_TBL_RD_OP_RSLT_OP_RSLT_LEN     1
	#define IPMC_TBL_RD_OP_RSLT_OP_RSLT_DEFAULT 0x0
	/*[field] ENTRY_INDEX*/
	#define IPMC_TBL_RD_OP_RSLT_ENTRY_INDEX
	#define IPMC_TBL_RD_OP_RSLT_ENTRY_INDEX_OFFSET  5
	#define IPMC_TBL_RD_OP_RSLT_ENTRY_INDEX_LEN     11
	#define IPMC_TBL_RD_OP_RSLT_ENTRY_INDEX_DEFAULT 0x0
	/*[field] VALID_CNT*/
	#define IPMC_TBL_RD_OP_RSLT_VALID_CNT
	#define IPMC_TBL_RD_OP_RSLT_VALID_CNT_OFFSET  16
	#define IPMC_TBL_RD_OP_RSLT_VALID_CNT_LEN     4
	#define IPMC_TBL_RD_OP_RSLT_VALID_CNT_DEFAULT 0x0

struct ipmc_tbl_rd_op_rslt {
	a_uint32_t  cmd_id:4;
	a_uint32_t  op_rslt:1;
	a_uint32_t  entry_index:11;
	a_uint32_t  valid_cnt:4;
	a_uint32_t  _reserved0:12;
};

union ipmc_tbl_rd_op_rslt_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_op_rslt bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA0*/
#define IPMC_TBL_RD_RSLT_DATA0
#define IPMC_TBL_RD_RSLT_DATA0_ADDRESS 0x1b8
#define IPMC_TBL_RD_RSLT_DATA0_NUM     1
#define IPMC_TBL_RD_RSLT_DATA0_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA0_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA0_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA0_DATA
	#define IPMC_TBL_RD_RSLT_DATA0_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA0_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA0_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data0 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data0_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data0 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA1*/
#define IPMC_TBL_RD_RSLT_DATA1
#define IPMC_TBL_RD_RSLT_DATA1_ADDRESS 0x1bc
#define IPMC_TBL_RD_RSLT_DATA1_NUM     1
#define IPMC_TBL_RD_RSLT_DATA1_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA1_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA1_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA1_DATA
	#define IPMC_TBL_RD_RSLT_DATA1_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA1_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA1_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data1 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data1_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data1 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA2*/
#define IPMC_TBL_RD_RSLT_DATA2
#define IPMC_TBL_RD_RSLT_DATA2_ADDRESS 0x1c0
#define IPMC_TBL_RD_RSLT_DATA2_NUM     1
#define IPMC_TBL_RD_RSLT_DATA2_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA2_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA2_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA2_DATA
	#define IPMC_TBL_RD_RSLT_DATA2_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA2_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA2_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data2 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data2_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data2 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA3*/
#define IPMC_TBL_RD_RSLT_DATA3
#define IPMC_TBL_RD_RSLT_DATA3_ADDRESS 0x1c4
#define IPMC_TBL_RD_RSLT_DATA3_NUM     1
#define IPMC_TBL_RD_RSLT_DATA3_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA3_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA3_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA3_DATA
	#define IPMC_TBL_RD_RSLT_DATA3_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA3_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA3_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data3 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data3_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data3 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA4*/
#define IPMC_TBL_RD_RSLT_DATA4
#define IPMC_TBL_RD_RSLT_DATA4_ADDRESS 0x1c8
#define IPMC_TBL_RD_RSLT_DATA4_NUM     1
#define IPMC_TBL_RD_RSLT_DATA4_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA4_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA4_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA4_DATA
	#define IPMC_TBL_RD_RSLT_DATA4_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA4_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA4_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data4 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data4_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data4 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA5*/
#define IPMC_TBL_RD_RSLT_DATA5
#define IPMC_TBL_RD_RSLT_DATA5_ADDRESS 0x1cc
#define IPMC_TBL_RD_RSLT_DATA5_NUM     1
#define IPMC_TBL_RD_RSLT_DATA5_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA5_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA5_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA5_DATA
	#define IPMC_TBL_RD_RSLT_DATA5_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA5_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA5_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data5 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data5_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data5 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA6*/
#define IPMC_TBL_RD_RSLT_DATA6
#define IPMC_TBL_RD_RSLT_DATA6_ADDRESS 0x1d0
#define IPMC_TBL_RD_RSLT_DATA6_NUM     1
#define IPMC_TBL_RD_RSLT_DATA6_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA6_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA6_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA6_DATA
	#define IPMC_TBL_RD_RSLT_DATA6_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA6_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA6_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data6 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data6_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data6 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA7*/
#define IPMC_TBL_RD_RSLT_DATA7
#define IPMC_TBL_RD_RSLT_DATA7_ADDRESS 0x1d4
#define IPMC_TBL_RD_RSLT_DATA7_NUM     1
#define IPMC_TBL_RD_RSLT_DATA7_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA7_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA7_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA7_DATA
	#define IPMC_TBL_RD_RSLT_DATA7_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA7_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA7_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data7 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data7_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data7 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA8*/
#define IPMC_TBL_RD_RSLT_DATA8
#define IPMC_TBL_RD_RSLT_DATA8_ADDRESS 0x1d8
#define IPMC_TBL_RD_RSLT_DATA8_NUM     1
#define IPMC_TBL_RD_RSLT_DATA8_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA8_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA8_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA8_DATA
	#define IPMC_TBL_RD_RSLT_DATA8_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA8_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA8_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data8 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data8_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data8 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA9*/
#define IPMC_TBL_RD_RSLT_DATA9
#define IPMC_TBL_RD_RSLT_DATA9_ADDRESS 0x1dc
#define IPMC_TBL_RD_RSLT_DATA9_NUM     1
#define IPMC_TBL_RD_RSLT_DATA9_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA9_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA9_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA9_DATA
	#define IPMC_TBL_RD_RSLT_DATA9_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA9_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA9_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data9 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data9_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data9 bf;
};

/*[register] IPMC_TBL_RD_RSLT_DATA10*/
#define IPMC_TBL_RD_RSLT_DATA10
#define IPMC_TBL_RD_RSLT_DATA10_ADDRESS 0x1e0
#define IPMC_TBL_RD_RSLT_DATA10_NUM     1
#define IPMC_TBL_RD_RSLT_DATA10_INC     0x4
#define IPMC_TBL_RD_RSLT_DATA10_TYPE    REG_TYPE_RO
#define IPMC_TBL_RD_RSLT_DATA10_DEFAULT 0x0
	/*[field] DATA*/
	#define IPMC_TBL_RD_RSLT_DATA10_DATA
	#define IPMC_TBL_RD_RSLT_DATA10_DATA_OFFSET  0
	#define IPMC_TBL_RD_RSLT_DATA10_DATA_LEN     32
	#define IPMC_TBL_RD_RSLT_DATA10_DATA_DEFAULT 0x0

struct ipmc_tbl_rd_rslt_data10 {
	a_uint32_t  data:32;
};

union ipmc_tbl_rd_rslt_data10_u {
	a_uint32_t val;
	struct ipmc_tbl_rd_rslt_data10 bf;
};

/*[register] IPMC_CONVERT_UC_CTRL*/
#define IPMC_CONVERT_UC_CTRL
#define IPMC_CONVERT_UC_CTRL_ADDRESS 0xa84
#define IPMC_CONVERT_UC_CTRL_NUM     1
#define IPMC_CONVERT_UC_CTRL_INC     0x4
#define IPMC_CONVERT_UC_CTRL_TYPE    REG_TYPE_RW
#define IPMC_CONVERT_UC_CTRL_DEFAULT 0x1
	/*[field] ENABLE_PORT_NUM*/
	#define IPMC_CONVERT_UC_CTRL_ENABLE_PORT_NUM
	#define IPMC_CONVERT_UC_CTRL_ENABLE_PORT_NUM_OFFSET  0
	#define IPMC_CONVERT_UC_CTRL_ENABLE_PORT_NUM_LEN     4
	#define IPMC_CONVERT_UC_CTRL_ENABLE_PORT_NUM_DEFAULT 0x1
	/*[field] CONVERT_PORT_VP*/
	#define IPMC_CONVERT_UC_CTRL_CONVERT_PORT_VP
	#define IPMC_CONVERT_UC_CTRL_CONVERT_PORT_VP_OFFSET  4
	#define IPMC_CONVERT_UC_CTRL_CONVERT_PORT_VP_LEN     8
	#define IPMC_CONVERT_UC_CTRL_CONVERT_PORT_VP_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define IPMC_CONVERT_UC_CTRL_SERVICE_CODE
	#define IPMC_CONVERT_UC_CTRL_SERVICE_CODE_OFFSET  12
	#define IPMC_CONVERT_UC_CTRL_SERVICE_CODE_LEN     8
	#define IPMC_CONVERT_UC_CTRL_SERVICE_CODE_DEFAULT 0x0
	/*[field] POST_BYPASS_BITMAP*/
	#define IPMC_CONVERT_UC_CTRL_POST_BYPASS_BITMAP
	#define IPMC_CONVERT_UC_CTRL_POST_BYPASS_BITMAP_OFFSET  20
	#define IPMC_CONVERT_UC_CTRL_POST_BYPASS_BITMAP_LEN     8
	#define IPMC_CONVERT_UC_CTRL_POST_BYPASS_BITMAP_DEFAULT 0x0

struct ipmc_convert_uc_ctrl {
	a_uint32_t  enable_port_num:4;
	a_uint32_t  convert_port_vp:8;
	a_uint32_t  service_code:8;
	a_uint32_t  post_bypass_bitmap:8;
	a_uint32_t  _reserved0:4;
};

union ipmc_convert_uc_ctrl_u {
	a_uint32_t val;
	struct ipmc_convert_uc_ctrl bf;
};

/*[table] IPMC_GIPV4_TBL*/
#define IPMC_GIPV4_TBL
#define IPMC_GIPV4_TBL_ADDRESS 0x30000
#define IPMC_GIPV4_TBL_NUM     2048
#define IPMC_GIPV4_TBL_INC     0x10
#define IPMC_GIPV4_TBL_TYPE    REG_TYPE_RW
#define IPMC_GIPV4_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define IPMC_GIPV4_TBL_VALID
	#define IPMC_GIPV4_TBL_VALID_OFFSET  0
	#define IPMC_GIPV4_TBL_VALID_LEN     1
	#define IPMC_GIPV4_TBL_VALID_DEFAULT 0x0
	/*[field] KEY_TYPE*/
	#define IPMC_GIPV4_TBL_KEY_TYPE
	#define IPMC_GIPV4_TBL_KEY_TYPE_OFFSET  1
	#define IPMC_GIPV4_TBL_KEY_TYPE_LEN     2
	#define IPMC_GIPV4_TBL_KEY_TYPE_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define IPMC_GIPV4_TBL_FWD_CMD
	#define IPMC_GIPV4_TBL_FWD_CMD_OFFSET  3
	#define IPMC_GIPV4_TBL_FWD_CMD_LEN     2
	#define IPMC_GIPV4_TBL_FWD_CMD_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IPMC_GIPV4_TBL_SYN_TOGGLE
	#define IPMC_GIPV4_TBL_SYN_TOGGLE_OFFSET  5
	#define IPMC_GIPV4_TBL_SYN_TOGGLE_LEN     1
	#define IPMC_GIPV4_TBL_SYN_TOGGLE_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define IPMC_GIPV4_TBL_DST_INFO
	#define IPMC_GIPV4_TBL_DST_INFO_OFFSET  6
	#define IPMC_GIPV4_TBL_DST_INFO_LEN     14
	#define IPMC_GIPV4_TBL_DST_INFO_DEFAULT 0x0
	/*[field] VID*/
	#define IPMC_GIPV4_TBL_VID
	#define IPMC_GIPV4_TBL_VID_OFFSET  20
	#define IPMC_GIPV4_TBL_VID_LEN     12
	#define IPMC_GIPV4_TBL_VID_DEFAULT 0x0
	/*[field] VID_FMT*/
	#define IPMC_GIPV4_TBL_VID_FMT
	#define IPMC_GIPV4_TBL_VID_FMT_OFFSET  32
	#define IPMC_GIPV4_TBL_VID_FMT_LEN     2
	#define IPMC_GIPV4_TBL_VID_FMT_DEFAULT 0x0
	/*[field] CHECK_FMT*/
	#define IPMC_GIPV4_TBL_CHECK_FMT
	#define IPMC_GIPV4_TBL_CHECK_FMT_OFFSET  34
	#define IPMC_GIPV4_TBL_CHECK_FMT_LEN     2
	#define IPMC_GIPV4_TBL_CHECK_FMT_DEFAULT 0x0
	/*[field] CHECK_SRC*/
	#define IPMC_GIPV4_TBL_CHECK_SRC
	#define IPMC_GIPV4_TBL_CHECK_SRC_OFFSET  36
	#define IPMC_GIPV4_TBL_CHECK_SRC_LEN     2
	#define IPMC_GIPV4_TBL_CHECK_SRC_DEFAULT 0x0
	/*[field] CHECK_SRC_PORT*/
	#define IPMC_GIPV4_TBL_CHECK_SRC_PORT
	#define IPMC_GIPV4_TBL_CHECK_SRC_PORT_OFFSET  38
	#define IPMC_GIPV4_TBL_CHECK_SRC_PORT_LEN     8
	#define IPMC_GIPV4_TBL_CHECK_SRC_PORT_DEFAULT 0x0
	/*[field] CONVERT_UC*/
	#define IPMC_GIPV4_TBL_CONVERT_UC
	#define IPMC_GIPV4_TBL_CONVERT_UC_OFFSET  46
	#define IPMC_GIPV4_TBL_CONVERT_UC_LEN     1
	#define IPMC_GIPV4_TBL_CONVERT_UC_DEFAULT 0x0
	/*[field] IP_ADDR*/
	#define IPMC_GIPV4_TBL_IP_ADDR
	#define IPMC_GIPV4_TBL_IP_ADDR_OFFSET  48
	#define IPMC_GIPV4_TBL_IP_ADDR_LEN     32
	#define IPMC_GIPV4_TBL_IP_ADDR_DEFAULT 0x0

struct ipmc_gipv4_tbl {
	a_uint32_t  valid:1;
	a_uint32_t  key_type:2;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  dst_info:14;
	a_uint32_t  vid:12;
	a_uint32_t  vid_fmt:2;
	a_uint32_t  check_fmt:2;
	a_uint32_t  check_src:2;
	a_uint32_t  check_src_port:8;
	a_uint32_t  convert_uc:1;
	a_uint32_t  _reserved0:1;
	a_uint32_t  ip_addr_0:16;
	a_uint32_t  ip_addr_1:16;
	a_uint32_t  _reserved1:16;
};

union ipmc_gipv4_tbl_u {
	a_uint32_t val[3];
	struct ipmc_gipv4_tbl bf;
};

/*[table] IPMC_GIPV6_TBL*/
#define IPMC_GIPV6_TBL
#define IPMC_GIPV6_TBL_ADDRESS 0x30000
#define IPMC_GIPV6_TBL_NUM     1024
#define IPMC_GIPV6_TBL_INC     0x20
#define IPMC_GIPV6_TBL_TYPE    REG_TYPE_RW
#define IPMC_GIPV6_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define IPMC_GIPV6_TBL_VALID
	#define IPMC_GIPV6_TBL_VALID_OFFSET  0
	#define IPMC_GIPV6_TBL_VALID_LEN     1
	#define IPMC_GIPV6_TBL_VALID_DEFAULT 0x0
	/*[field] KEY_TYPE*/
	#define IPMC_GIPV6_TBL_KEY_TYPE
	#define IPMC_GIPV6_TBL_KEY_TYPE_OFFSET  1
	#define IPMC_GIPV6_TBL_KEY_TYPE_LEN     2
	#define IPMC_GIPV6_TBL_KEY_TYPE_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define IPMC_GIPV6_TBL_FWD_CMD
	#define IPMC_GIPV6_TBL_FWD_CMD_OFFSET  3
	#define IPMC_GIPV6_TBL_FWD_CMD_LEN     2
	#define IPMC_GIPV6_TBL_FWD_CMD_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IPMC_GIPV6_TBL_SYN_TOGGLE
	#define IPMC_GIPV6_TBL_SYN_TOGGLE_OFFSET  5
	#define IPMC_GIPV6_TBL_SYN_TOGGLE_LEN     1
	#define IPMC_GIPV6_TBL_SYN_TOGGLE_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define IPMC_GIPV6_TBL_DST_INFO
	#define IPMC_GIPV6_TBL_DST_INFO_OFFSET  6
	#define IPMC_GIPV6_TBL_DST_INFO_LEN     14
	#define IPMC_GIPV6_TBL_DST_INFO_DEFAULT 0x0
	/*[field] VID*/
	#define IPMC_GIPV6_TBL_VID
	#define IPMC_GIPV6_TBL_VID_OFFSET  20
	#define IPMC_GIPV6_TBL_VID_LEN     12
	#define IPMC_GIPV6_TBL_VID_DEFAULT 0x0
	/*[field] VID_FMT*/
	#define IPMC_GIPV6_TBL_VID_FMT
	#define IPMC_GIPV6_TBL_VID_FMT_OFFSET  32
	#define IPMC_GIPV6_TBL_VID_FMT_LEN     2
	#define IPMC_GIPV6_TBL_VID_FMT_DEFAULT 0x0
	/*[field] CHECK_FMT*/
	#define IPMC_GIPV6_TBL_CHECK_FMT
	#define IPMC_GIPV6_TBL_CHECK_FMT_OFFSET  34
	#define IPMC_GIPV6_TBL_CHECK_FMT_LEN     2
	#define IPMC_GIPV6_TBL_CHECK_FMT_DEFAULT 0x0
	/*[field] CHECK_SRC*/
	#define IPMC_GIPV6_TBL_CHECK_SRC
	#define IPMC_GIPV6_TBL_CHECK_SRC_OFFSET  36
	#define IPMC_GIPV6_TBL_CHECK_SRC_LEN     2
	#define IPMC_GIPV6_TBL_CHECK_SRC_DEFAULT 0x0
	/*[field] CHECK_SRC_PORT*/
	#define IPMC_GIPV6_TBL_CHECK_SRC_PORT
	#define IPMC_GIPV6_TBL_CHECK_SRC_PORT_OFFSET  38
	#define IPMC_GIPV6_TBL_CHECK_SRC_PORT_LEN     8
	#define IPMC_GIPV6_TBL_CHECK_SRC_PORT_DEFAULT 0x0
	/*[field] CONVERT_UC*/
	#define IPMC_GIPV6_TBL_CONVERT_UC
	#define IPMC_GIPV6_TBL_CONVERT_UC_OFFSET  46
	#define IPMC_GIPV6_TBL_CONVERT_UC_LEN     1
	#define IPMC_GIPV6_TBL_CONVERT_UC_DEFAULT 0x0
	/*[field] IPV6_ADDR*/
	#define IPMC_GIPV6_TBL_IPV6_ADDR
	#define IPMC_GIPV6_TBL_IPV6_ADDR_OFFSET  48
	#define IPMC_GIPV6_TBL_IPV6_ADDR_LEN     128
	#define IPMC_GIPV6_TBL_IPV6_ADDR_DEFAULT 0x0

struct ipmc_gipv6_tbl {
	a_uint32_t  valid:1;
	a_uint32_t  key_type:2;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  dst_info:14;
	a_uint32_t  vid:12;
	a_uint32_t  vid_fmt:2;
	a_uint32_t  check_fmt:2;
	a_uint32_t  check_src:2;
	a_uint32_t  check_src_port:8;
	a_uint32_t  convert_uc:1;
	a_uint32_t  _reserved0:1;
	a_uint32_t  ipv6_addr_0:16;
	a_uint32_t  ipv6_addr_1:32;
	a_uint32_t  ipv6_addr_2:32;
	a_uint32_t  ipv6_addr_3:32;
	a_uint32_t  ipv6_addr_4:16;
	a_uint32_t  _reserved1:16;
};

union ipmc_gipv6_tbl_u {
	a_uint32_t val[6];
	struct ipmc_gipv6_tbl bf;
};

/*[table] IPMC_SIPV4_GIPV4_TBL*/
#define IPMC_SIPV4_GIPV4_TBL
#define IPMC_SIPV4_GIPV4_TBL_ADDRESS 0x30000
#define IPMC_SIPV4_GIPV4_TBL_NUM     1024
#define IPMC_SIPV4_GIPV4_TBL_INC     0x20
#define IPMC_SIPV4_GIPV4_TBL_TYPE    REG_TYPE_RW
#define IPMC_SIPV4_GIPV4_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define IPMC_SIPV4_GIPV4_TBL_VALID
	#define IPMC_SIPV4_GIPV4_TBL_VALID_OFFSET  0
	#define IPMC_SIPV4_GIPV4_TBL_VALID_LEN     1
	#define IPMC_SIPV4_GIPV4_TBL_VALID_DEFAULT 0x0
	/*[field] KEY_TYPE*/
	#define IPMC_SIPV4_GIPV4_TBL_KEY_TYPE
	#define IPMC_SIPV4_GIPV4_TBL_KEY_TYPE_OFFSET  1
	#define IPMC_SIPV4_GIPV4_TBL_KEY_TYPE_LEN     2
	#define IPMC_SIPV4_GIPV4_TBL_KEY_TYPE_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define IPMC_SIPV4_GIPV4_TBL_FWD_CMD
	#define IPMC_SIPV4_GIPV4_TBL_FWD_CMD_OFFSET  3
	#define IPMC_SIPV4_GIPV4_TBL_FWD_CMD_LEN     2
	#define IPMC_SIPV4_GIPV4_TBL_FWD_CMD_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IPMC_SIPV4_GIPV4_TBL_SYN_TOGGLE
	#define IPMC_SIPV4_GIPV4_TBL_SYN_TOGGLE_OFFSET  5
	#define IPMC_SIPV4_GIPV4_TBL_SYN_TOGGLE_LEN     1
	#define IPMC_SIPV4_GIPV4_TBL_SYN_TOGGLE_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define IPMC_SIPV4_GIPV4_TBL_DST_INFO
	#define IPMC_SIPV4_GIPV4_TBL_DST_INFO_OFFSET  6
	#define IPMC_SIPV4_GIPV4_TBL_DST_INFO_LEN     14
	#define IPMC_SIPV4_GIPV4_TBL_DST_INFO_DEFAULT 0x0
	/*[field] VID*/
	#define IPMC_SIPV4_GIPV4_TBL_VID
	#define IPMC_SIPV4_GIPV4_TBL_VID_OFFSET  20
	#define IPMC_SIPV4_GIPV4_TBL_VID_LEN     12
	#define IPMC_SIPV4_GIPV4_TBL_VID_DEFAULT 0x0
	/*[field] VID_FMT*/
	#define IPMC_SIPV4_GIPV4_TBL_VID_FMT
	#define IPMC_SIPV4_GIPV4_TBL_VID_FMT_OFFSET  32
	#define IPMC_SIPV4_GIPV4_TBL_VID_FMT_LEN     2
	#define IPMC_SIPV4_GIPV4_TBL_VID_FMT_DEFAULT 0x0
	/*[field] CHECK_FMT*/
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_FMT
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_FMT_OFFSET  34
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_FMT_LEN     2
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_FMT_DEFAULT 0x0
	/*[field] CHECK_SRC*/
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_OFFSET  36
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_LEN     2
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_DEFAULT 0x0
	/*[field] CHECK_SRC_PORT*/
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_PORT
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_PORT_OFFSET  38
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_PORT_LEN     8
	#define IPMC_SIPV4_GIPV4_TBL_CHECK_SRC_PORT_DEFAULT 0x0
	/*[field] CONVERT_UC*/
	#define IPMC_SIPV4_GIPV4_TBL_CONVERT_UC
	#define IPMC_SIPV4_GIPV4_TBL_CONVERT_UC_OFFSET  46
	#define IPMC_SIPV4_GIPV4_TBL_CONVERT_UC_LEN     1
	#define IPMC_SIPV4_GIPV4_TBL_CONVERT_UC_DEFAULT 0x0
	/*[field] GIP_ADDR*/
	#define IPMC_SIPV4_GIPV4_TBL_GIP_ADDR
	#define IPMC_SIPV4_GIPV4_TBL_GIP_ADDR_OFFSET  48
	#define IPMC_SIPV4_GIPV4_TBL_GIP_ADDR_LEN     32
	#define IPMC_SIPV4_GIPV4_TBL_GIP_ADDR_DEFAULT 0x0
	/*[field] SIP_ADDR*/
	#define IPMC_SIPV4_GIPV4_TBL_SIP_ADDR
	#define IPMC_SIPV4_GIPV4_TBL_SIP_ADDR_OFFSET  80
	#define IPMC_SIPV4_GIPV4_TBL_SIP_ADDR_LEN     32
	#define IPMC_SIPV4_GIPV4_TBL_SIP_ADDR_DEFAULT 0x0

struct ipmc_sipv4_gipv4_tbl {
	a_uint32_t  valid:1;
	a_uint32_t  key_type:2;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  dst_info:14;
	a_uint32_t  vid:12;
	a_uint32_t  vid_fmt:2;
	a_uint32_t  check_fmt:2;
	a_uint32_t  check_src:2;
	a_uint32_t  check_src_port:8;
	a_uint32_t  convert_uc:1;
	a_uint32_t  _reserved0:1;
	a_uint32_t  gip_addr_0:16;
	a_uint32_t  gip_addr_1:16;
	a_uint32_t  sip_addr_0:16;
	a_uint32_t  sip_addr_1:16;
	a_uint32_t  _reserved1:16;
};

union ipmc_sipv4_gipv4_tbl_u {
	a_uint32_t val[4];
	struct ipmc_sipv4_gipv4_tbl bf;
};

/*[table] IPMC_SIPV6_GIPV6_TBL*/
#define IPMC_SIPV6_GIPV6_TBL
#define IPMC_SIPV6_GIPV6_TBL_ADDRESS 0x30000
#define IPMC_SIPV6_GIPV6_TBL_NUM     512
#define IPMC_SIPV6_GIPV6_TBL_INC     0x40
#define IPMC_SIPV6_GIPV6_TBL_TYPE    REG_TYPE_RW
#define IPMC_SIPV6_GIPV6_TBL_DEFAULT 0x0
	/*[field] VALID*/
	#define IPMC_SIPV6_GIPV6_TBL_VALID
	#define IPMC_SIPV6_GIPV6_TBL_VALID_OFFSET  0
	#define IPMC_SIPV6_GIPV6_TBL_VALID_LEN     1
	#define IPMC_SIPV6_GIPV6_TBL_VALID_DEFAULT 0x0
	/*[field] KEY_TYPE*/
	#define IPMC_SIPV6_GIPV6_TBL_KEY_TYPE
	#define IPMC_SIPV6_GIPV6_TBL_KEY_TYPE_OFFSET  1
	#define IPMC_SIPV6_GIPV6_TBL_KEY_TYPE_LEN     2
	#define IPMC_SIPV6_GIPV6_TBL_KEY_TYPE_DEFAULT 0x0
	/*[field] FWD_CMD*/
	#define IPMC_SIPV6_GIPV6_TBL_FWD_CMD
	#define IPMC_SIPV6_GIPV6_TBL_FWD_CMD_OFFSET  3
	#define IPMC_SIPV6_GIPV6_TBL_FWD_CMD_LEN     2
	#define IPMC_SIPV6_GIPV6_TBL_FWD_CMD_DEFAULT 0x0
	/*[field] SYN_TOGGLE*/
	#define IPMC_SIPV6_GIPV6_TBL_SYN_TOGGLE
	#define IPMC_SIPV6_GIPV6_TBL_SYN_TOGGLE_OFFSET  5
	#define IPMC_SIPV6_GIPV6_TBL_SYN_TOGGLE_LEN     1
	#define IPMC_SIPV6_GIPV6_TBL_SYN_TOGGLE_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define IPMC_SIPV6_GIPV6_TBL_DST_INFO
	#define IPMC_SIPV6_GIPV6_TBL_DST_INFO_OFFSET  6
	#define IPMC_SIPV6_GIPV6_TBL_DST_INFO_LEN     14
	#define IPMC_SIPV6_GIPV6_TBL_DST_INFO_DEFAULT 0x0
	/*[field] VID*/
	#define IPMC_SIPV6_GIPV6_TBL_VID
	#define IPMC_SIPV6_GIPV6_TBL_VID_OFFSET  20
	#define IPMC_SIPV6_GIPV6_TBL_VID_LEN     12
	#define IPMC_SIPV6_GIPV6_TBL_VID_DEFAULT 0x0
	/*[field] VID_FMT*/
	#define IPMC_SIPV6_GIPV6_TBL_VID_FMT
	#define IPMC_SIPV6_GIPV6_TBL_VID_FMT_OFFSET  32
	#define IPMC_SIPV6_GIPV6_TBL_VID_FMT_LEN     2
	#define IPMC_SIPV6_GIPV6_TBL_VID_FMT_DEFAULT 0x0
	/*[field] CHECK_FMT*/
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_FMT
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_FMT_OFFSET  34
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_FMT_LEN     2
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_FMT_DEFAULT 0x0
	/*[field] CHECK_SRC*/
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_OFFSET  36
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_LEN     2
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_DEFAULT 0x0
	/*[field] CHECK_SRC_PORT*/
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_PORT
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_PORT_OFFSET  38
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_PORT_LEN     8
	#define IPMC_SIPV6_GIPV6_TBL_CHECK_SRC_PORT_DEFAULT 0x0
	/*[field] CONVERT_UC*/
	#define IPMC_SIPV6_GIPV6_TBL_CONVERT_UC
	#define IPMC_SIPV6_GIPV6_TBL_CONVERT_UC_OFFSET  46
	#define IPMC_SIPV6_GIPV6_TBL_CONVERT_UC_LEN     1
	#define IPMC_SIPV6_GIPV6_TBL_CONVERT_UC_DEFAULT 0x0
	/*[field] GIPV6_ADDR*/
	#define IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR
	#define IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_OFFSET  48
	#define IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_LEN     128
	#define IPMC_SIPV6_GIPV6_TBL_GIPV6_ADDR_DEFAULT 0x0
	/*[field] SIPV6_ADDR*/
	#define IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR
	#define IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_OFFSET  176
	#define IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_LEN     128
	#define IPMC_SIPV6_GIPV6_TBL_SIPV6_ADDR_DEFAULT 0x0

struct ipmc_sipv6_gipv6_tbl {
	a_uint32_t  valid:1;
	a_uint32_t  key_type:2;
	a_uint32_t  fwd_cmd:2;
	a_uint32_t  syn_toggle:1;
	a_uint32_t  dst_info:14;
	a_uint32_t  vid:12;
	a_uint32_t  vid_fmt:2;
	a_uint32_t  check_fmt:2;
	a_uint32_t  check_src:2;
	a_uint32_t  check_src_port:8;
	a_uint32_t  convert_uc:1;
	a_uint32_t  _reserved0:1;
	a_uint32_t  gipv6_addr_0:16;
	a_uint32_t  gipv6_addr_1:32;
	a_uint32_t  gipv6_addr_2:32;
	a_uint32_t  gipv6_addr_3:32;
	a_uint32_t  gipv6_addr_4:16;
	a_uint32_t  sipv6_addr_0:16;
	a_uint32_t  sipv6_addr_1:32;
	a_uint32_t  sipv6_addr_2:32;
	a_uint32_t  sipv6_addr_3:32;
	a_uint32_t  sipv6_addr_4:16;
	a_uint32_t  _reserved1:16;
};

union ipmc_sipv6_gipv6_tbl_u {
	a_uint32_t val[10];
	struct ipmc_sipv6_gipv6_tbl bf;
};

#endif
