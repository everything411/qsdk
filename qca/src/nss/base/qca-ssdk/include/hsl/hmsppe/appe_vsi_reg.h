/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_VSI_REG_H_
#define _APPE_VSI_REG_H_

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

#endif
