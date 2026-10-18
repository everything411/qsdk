/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_VSI_REG_H_
#define _HTTPPE_VSI_REG_H_

/*[table] VSI_TBL*/
#define VSI_TBL
#define VSI_TBL_ADDRESS 0x3800
#define VSI_TBL_NUM     64
#define VSI_TBL_INC     0x10
#define VSI_TBL_TYPE    REG_TYPE_RW
#define VSI_TBL_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP*/
	#define VSI_TBL_MEMBER_PORT_BITMAP
	#define VSI_TBL_MEMBER_PORT_BITMAP_OFFSET  0
	#define VSI_TBL_MEMBER_PORT_BITMAP_LEN     9
	#define VSI_TBL_MEMBER_PORT_BITMAP_DEFAULT 0x0
	/*[field] UUC_BITMAP*/
	#define VSI_TBL_UUC_BITMAP
	#define VSI_TBL_UUC_BITMAP_OFFSET  9
	#define VSI_TBL_UUC_BITMAP_LEN     9
	#define VSI_TBL_UUC_BITMAP_DEFAULT 0x0
	/*[field] UMC_BITMAP*/
	#define VSI_TBL_UMC_BITMAP
	#define VSI_TBL_UMC_BITMAP_OFFSET  18
	#define VSI_TBL_UMC_BITMAP_LEN     9
	#define VSI_TBL_UMC_BITMAP_DEFAULT 0x0
	/*[field] BC_BITMAP*/
	#define VSI_TBL_BC_BITMAP
	#define VSI_TBL_BC_BITMAP_OFFSET  27
	#define VSI_TBL_BC_BITMAP_LEN     9
	#define VSI_TBL_BC_BITMAP_DEFAULT 0x0
	/*[field] NEW_ADDR_LRN_EN*/
	#define VSI_TBL_NEW_ADDR_LRN_EN
	#define VSI_TBL_NEW_ADDR_LRN_EN_OFFSET  36
	#define VSI_TBL_NEW_ADDR_LRN_EN_LEN     1
	#define VSI_TBL_NEW_ADDR_LRN_EN_DEFAULT 0x0
	/*[field] NEW_ADDR_FWD_CMD*/
	#define VSI_TBL_NEW_ADDR_FWD_CMD
	#define VSI_TBL_NEW_ADDR_FWD_CMD_OFFSET  37
	#define VSI_TBL_NEW_ADDR_FWD_CMD_LEN     2
	#define VSI_TBL_NEW_ADDR_FWD_CMD_DEFAULT 0x0
	/*[field] STATION_MOVE_LRN_EN*/
	#define VSI_TBL_STATION_MOVE_LRN_EN
	#define VSI_TBL_STATION_MOVE_LRN_EN_OFFSET  39
	#define VSI_TBL_STATION_MOVE_LRN_EN_LEN     1
	#define VSI_TBL_STATION_MOVE_LRN_EN_DEFAULT 0x0
	/*[field] STATION_MOVE_FWD_CMD*/
	#define VSI_TBL_STATION_MOVE_FWD_CMD
	#define VSI_TBL_STATION_MOVE_FWD_CMD_OFFSET  40
	#define VSI_TBL_STATION_MOVE_FWD_CMD_LEN     2
	#define VSI_TBL_STATION_MOVE_FWD_CMD_DEFAULT 0x0
	/*[field] IPMC_EN*/
	#define VSI_TBL_IPMC_EN
	#define VSI_TBL_IPMC_EN_OFFSET  42
	#define VSI_TBL_IPMC_EN_LEN     1
	#define VSI_TBL_IPMC_EN_DEFAULT 0x0

struct vsi_tbl {
	a_uint32_t  member_port_bitmap:9;
	a_uint32_t  uuc_bitmap:9;
	a_uint32_t  umc_bitmap:9;
	a_uint32_t  bc_bitmap_0:5;
	a_uint32_t  bc_bitmap_1:4;
	a_uint32_t  new_addr_lrn_en:1;
	a_uint32_t  new_addr_fwd_cmd:2;
	a_uint32_t  station_move_lrn_en:1;
	a_uint32_t  station_move_fwd_cmd:2;
	a_uint32_t  ipmc_en:1;
	a_uint32_t  _reserved0:21;
};

union vsi_tbl_u {
	a_uint32_t val[2];
	struct vsi_tbl bf;
};

/*[table] VSI_REMAP_TBL*/
#define VSI_REMAP_TBL
#define VSI_REMAP_TBL_ADDRESS 0x20000
#define VSI_REMAP_TBL_NUM     64
#define VSI_REMAP_TBL_INC     0x40
#define VSI_REMAP_TBL_TYPE    REG_TYPE_RW
#define VSI_REMAP_TBL_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_0*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_0
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_0_OFFSET  0
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_0_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_0_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_1*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_1
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_1_OFFSET  32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_1_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_1_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_2*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_2
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_2_OFFSET  64
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_2_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_2_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_3*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_3
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_3_OFFSET  96
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_3_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_3_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_4*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_4
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_4_OFFSET  128
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_4_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_4_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_5*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_5
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_5_OFFSET  160
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_5_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_5_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_6*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_6
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_6_OFFSET  192
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_6_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_6_DEFAULT 0x0
	/*[field] MEMBER_PORT_BITMAP_7*/
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_7
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_7_OFFSET  224
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_7_LEN     32
	#define VSI_REMAP_TBL_MEMBER_PORT_BITMAP_7_DEFAULT 0x0
	/*[field] VSI_REMAP_EN*/
	#define VSI_REMAP_TBL_VSI_REMAP_EN
	#define VSI_REMAP_TBL_VSI_REMAP_EN_OFFSET  256
	#define VSI_REMAP_TBL_VSI_REMAP_EN_LEN     1
	#define VSI_REMAP_TBL_VSI_REMAP_EN_DEFAULT 0x0
	/*[field] BR_VSI*/
	#define VSI_REMAP_TBL_BR_VSI
	#define VSI_REMAP_TBL_BR_VSI_OFFSET  257
	#define VSI_REMAP_TBL_BR_VSI_LEN     6
	#define VSI_REMAP_TBL_BR_VSI_DEFAULT 0x0

struct vsi_remap_tbl {
	a_uint32_t  member_port_bitmap_0:32;
	a_uint32_t  member_port_bitmap_1:32;
	a_uint32_t  member_port_bitmap_2:32;
	a_uint32_t  member_port_bitmap_3:32;
	a_uint32_t  member_port_bitmap_4:32;
	a_uint32_t  member_port_bitmap_5:32;
	a_uint32_t  member_port_bitmap_6:32;
	a_uint32_t  member_port_bitmap_7:32;
	a_uint32_t  vsi_remap_en:1;
	a_uint32_t  br_vsi:6;
	a_uint32_t  _reserved0:25;
};

union vsi_remap_tbl_u {
	a_uint32_t val[9];
	struct vsi_remap_tbl bf;
};

/*[table] EG_VSI_COUNTER_TBL*/
#define EG_VSI_COUNTER_TBL
#define EG_VSI_COUNTER_TBL_ADDRESS 0x10000
#define EG_VSI_COUNTER_TBL_NUM     64
#define EG_VSI_COUNTER_TBL_INC     0x10
#define EG_VSI_COUNTER_TBL_TYPE    REG_TYPE_RW
#define EG_VSI_COUNTER_TBL_DEFAULT 0x0
	/*[field] TX_PACKETS*/
	#define EG_VSI_COUNTER_TBL_TX_PACKETS
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_OFFSET  0
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_LEN     32
	#define EG_VSI_COUNTER_TBL_TX_PACKETS_DEFAULT 0x0
	/*[field] TX_BYTES*/
	#define EG_VSI_COUNTER_TBL_TX_BYTES
	#define EG_VSI_COUNTER_TBL_TX_BYTES_OFFSET  32
	#define EG_VSI_COUNTER_TBL_TX_BYTES_LEN     40
	#define EG_VSI_COUNTER_TBL_TX_BYTES_DEFAULT 0x0

struct eg_vsi_counter_tbl {
	a_uint32_t  tx_packets:32;
	a_uint32_t  tx_bytes_0:32;
	a_uint32_t  tx_bytes_1:8;
	a_uint32_t  _reserved0:24;
};

union eg_vsi_counter_tbl_u {
	a_uint32_t val[3];
	struct eg_vsi_counter_tbl bf;
};

#endif
