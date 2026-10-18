/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_PPPOE_REG_H_
#define _APPE_PPPOE_REG_H_

/*[table] PPPOE_SESSION*/
#define PPPOE_SESSION
#define PPPOE_SESSION_ADDRESS 0x6800
#define PPPOE_SESSION_NUM     64
#define PPPOE_SESSION_INC     0x4
#define PPPOE_SESSION_TYPE    REG_TYPE_RW
#define PPPOE_SESSION_DEFAULT 0x0
	/*[field] SESSION_ID*/
	#define PPPOE_SESSION_SESSION_ID
	#define PPPOE_SESSION_SESSION_ID_OFFSET  0
	#define PPPOE_SESSION_SESSION_ID_LEN     16
	#define PPPOE_SESSION_SESSION_ID_DEFAULT 0x0
	/*[field] PORT_BITMAP reuse TYPE[0]*/
	#define PPPOE_SESSION_PORT_BITMAP
	#define PPPOE_SESSION_PORT_BITMAP_OFFSET  16
	#define PPPOE_SESSION_PORT_BITMAP_LEN     9
	#define PPPOE_SESSION_PORT_BITMAP_DEFAULT 0x0
	/*[field] PORT_VP_ID reuse TYPE[1]*/
	#define PPPOE_SESSION_PORT_VP_ID
	#define PPPOE_SESSION_PORT_VP_ID_OFFSET  16
	#define PPPOE_SESSION_PORT_VP_ID_LEN     9
	#define PPPOE_SESSION_PORT_VP_ID_DEFAULT 0x0
	/*[field] VP_PROFILE reuse TYPE[2]*/
	#define PPPOE_SESSION_VP_PROFILE
	#define PPPOE_SESSION_VP_PROFILE_OFFSET  16
	#define PPPOE_SESSION_VP_PROFILE_LEN     9
	#define PPPOE_SESSION_VP_PROFILE_DEFAULT 0x0
	/*[field] TYPE*/
	#define PPPOE_SESSION_TYPE_F
	#define PPPOE_SESSION_TYPE_F_OFFSET  25
	#define PPPOE_SESSION_TYPE_F_LEN     2
	#define PPPOE_SESSION_TYPE_F_DEFAULT 0x0

struct pppoe_session_0 {
	a_uint32_t  session_id:16;
	a_uint32_t  port_bitmap:9;
	a_uint32_t  type:2;
	a_uint32_t  _reserved0:5;
};

struct pppoe_session_1 {
	a_uint32_t  session_id:16;
	a_uint32_t  port_vp_id:9;
	a_uint32_t  type:2;
	a_uint32_t  _reserved0:5;
};

struct pppoe_session_2 {
	a_uint32_t  session_id:16;
	a_uint32_t  vp_profile:9;
	a_uint32_t  type:2;
	a_uint32_t  _reserved0:5;
};

union pppoe_session_u {
	a_uint32_t val;
	struct pppoe_session_0 bf;
	struct pppoe_session_1 bf1;
	struct pppoe_session_2 bf2;
};

/*[table] PPPOE_SESSION_EXT*/
#define PPPOE_SESSION_EXT
#define PPPOE_SESSION_EXT_ADDRESS 0x6a00
#define PPPOE_SESSION_EXT_NUM     64
#define PPPOE_SESSION_EXT_INC     0x4
#define PPPOE_SESSION_EXT_TYPE    REG_TYPE_RW
#define PPPOE_SESSION_EXT_DEFAULT 0x0
	/*[field] MC_VALID*/
	#define PPPOE_SESSION_EXT_MC_VALID
	#define PPPOE_SESSION_EXT_MC_VALID_OFFSET  0
	#define PPPOE_SESSION_EXT_MC_VALID_LEN     1
	#define PPPOE_SESSION_EXT_MC_VALID_DEFAULT 0x0
	/*[field] UC_VALID*/
	#define PPPOE_SESSION_EXT_UC_VALID
	#define PPPOE_SESSION_EXT_UC_VALID_OFFSET  1
	#define PPPOE_SESSION_EXT_UC_VALID_LEN     1
	#define PPPOE_SESSION_EXT_UC_VALID_DEFAULT 0x0
	/*[field] SMAC_VALID*/
	#define PPPOE_SESSION_EXT_SMAC_VALID
	#define PPPOE_SESSION_EXT_SMAC_VALID_OFFSET  2
	#define PPPOE_SESSION_EXT_SMAC_VALID_LEN     1
	#define PPPOE_SESSION_EXT_SMAC_VALID_DEFAULT 0x0
	/*[field] SMAC*/
	#define PPPOE_SESSION_EXT_SMAC
	#define PPPOE_SESSION_EXT_SMAC_OFFSET  16
	#define PPPOE_SESSION_EXT_SMAC_LEN     16
	#define PPPOE_SESSION_EXT_SMAC_DEFAULT 0x0

struct pppoe_session_ext {
	a_uint32_t  mc_valid:1;
	a_uint32_t  uc_valid:1;
	a_uint32_t  smac_valid:1;
	a_uint32_t  _reserved0:13;
	a_uint32_t  smac:16;
};

union pppoe_session_ext_u {
	a_uint32_t val;
	struct pppoe_session_ext bf;
};

/*[table] PPPOE_SESSION_EXT1*/
#define PPPOE_SESSION_EXT1
#define PPPOE_SESSION_EXT1_ADDRESS 0x6c00
#define PPPOE_SESSION_EXT1_NUM     64
#define PPPOE_SESSION_EXT1_INC     0x4
#define PPPOE_SESSION_EXT1_TYPE    REG_TYPE_RW
#define PPPOE_SESSION_EXT1_DEFAULT 0x0
	/*[field] SMAC*/
	#define PPPOE_SESSION_EXT1_SMAC
	#define PPPOE_SESSION_EXT1_SMAC_OFFSET  0
	#define PPPOE_SESSION_EXT1_SMAC_LEN     32
	#define PPPOE_SESSION_EXT1_SMAC_DEFAULT 0x0

struct pppoe_session_ext1 {
	a_uint32_t  smac:32;
};

union pppoe_session_ext1_u {
	a_uint32_t val;
	struct pppoe_session_ext1 bf;
};

/*[table] PPPOE_SESSION_EXT2*/
#define PPPOE_SESSION_EXT2
#define PPPOE_SESSION_EXT2_ADDRESS 0x6e00
#define PPPOE_SESSION_EXT2_NUM     64
#define PPPOE_SESSION_EXT2_INC     0x4
#define PPPOE_SESSION_EXT2_TYPE    REG_TYPE_RW
#define PPPOE_SESSION_EXT2_DEFAULT 0x0
	/*[field] TL_L3_IF_INDEX*/
	#define PPPOE_SESSION_EXT2_TL_L3_IF_INDEX
	#define PPPOE_SESSION_EXT2_TL_L3_IF_INDEX_OFFSET  0
	#define PPPOE_SESSION_EXT2_TL_L3_IF_INDEX_LEN     7
	#define PPPOE_SESSION_EXT2_TL_L3_IF_INDEX_DEFAULT 0x0
	/*[field] TL_L3_IF_VALID*/
	#define PPPOE_SESSION_EXT2_TL_L3_IF_VALID
	#define PPPOE_SESSION_EXT2_TL_L3_IF_VALID_OFFSET  7
	#define PPPOE_SESSION_EXT2_TL_L3_IF_VALID_LEN     1
	#define PPPOE_SESSION_EXT2_TL_L3_IF_VALID_DEFAULT 0x0
	/*[field] L3_IF_INDEX*/
	#define PPPOE_SESSION_EXT2_L3_IF_INDEX
	#define PPPOE_SESSION_EXT2_L3_IF_INDEX_OFFSET  8
	#define PPPOE_SESSION_EXT2_L3_IF_INDEX_LEN     8
	#define PPPOE_SESSION_EXT2_L3_IF_INDEX_DEFAULT 0x0
	/*[field] L3_IF_VALID*/
	#define PPPOE_SESSION_EXT2_L3_IF_VALID
	#define PPPOE_SESSION_EXT2_L3_IF_VALID_OFFSET  16
	#define PPPOE_SESSION_EXT2_L3_IF_VALID_LEN     1
	#define PPPOE_SESSION_EXT2_L3_IF_VALID_DEFAULT 0x0

struct pppoe_session_ext2 {
	a_uint32_t  tl_l3_if_index:7;
	a_uint32_t  tl_l3_if_valid:1;
	a_uint32_t  l3_if_index:8;
	a_uint32_t  l3_if_valid:1;
	a_uint32_t  _reserved0:15;
};

union pppoe_session_ext2_u {
	a_uint32_t val;
	struct pppoe_session_ext2 bf;
};

#endif
