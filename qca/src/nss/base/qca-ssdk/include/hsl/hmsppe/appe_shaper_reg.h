/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_SHAPER_REG_H_
#define _APPE_SHAPER_REG_H_

/*[register] SHP_CFG_L0*/
#define SHP_CFG_L0
#define SHP_CFG_L0_ADDRESS 0x30
#define SHP_CFG_L0_NUM     1
#define SHP_CFG_L0_INC     0x4
#define SHP_CFG_L0_TYPE    REG_TYPE_RW
#define SHP_CFG_L0_DEFAULT 0x0
	/*[field] L0_SHP_LL_TAIL*/
	#define SHP_CFG_L0_L0_SHP_LL_TAIL
	#define SHP_CFG_L0_L0_SHP_LL_TAIL_OFFSET  0
	#define SHP_CFG_L0_L0_SHP_LL_TAIL_LEN     9
	#define SHP_CFG_L0_L0_SHP_LL_TAIL_DEFAULT 0x0
	/*[field] L0_SHP_LL_HEAD*/
	#define SHP_CFG_L0_L0_SHP_LL_HEAD
	#define SHP_CFG_L0_L0_SHP_LL_HEAD_OFFSET  16
	#define SHP_CFG_L0_L0_SHP_LL_HEAD_LEN     9
	#define SHP_CFG_L0_L0_SHP_LL_HEAD_DEFAULT 0x0

struct shp_cfg_l0 {
	a_uint32_t  l0_shp_ll_tail:9;
	a_uint32_t  _reserved0:7;
	a_uint32_t  l0_shp_ll_head:9;
	a_uint32_t  _reserved1:7;
};

union shp_cfg_l0_u {
	a_uint32_t val;
	struct shp_cfg_l0 bf;
};

/*[register] SHP_CFG_L1*/
#define SHP_CFG_L1
#define SHP_CFG_L1_ADDRESS 0x34
#define SHP_CFG_L1_NUM     1
#define SHP_CFG_L1_INC     0x4
#define SHP_CFG_L1_TYPE    REG_TYPE_RW
#define SHP_CFG_L1_DEFAULT 0x0
	/*[field] L1_SHP_LL_TAIL*/
	#define SHP_CFG_L1_L1_SHP_LL_TAIL
	#define SHP_CFG_L1_L1_SHP_LL_TAIL_OFFSET  0
	#define SHP_CFG_L1_L1_SHP_LL_TAIL_LEN     6
	#define SHP_CFG_L1_L1_SHP_LL_TAIL_DEFAULT 0x0
	/*[field] L1_SHP_LL_HEAD*/
	#define SHP_CFG_L1_L1_SHP_LL_HEAD
	#define SHP_CFG_L1_L1_SHP_LL_HEAD_OFFSET  16
	#define SHP_CFG_L1_L1_SHP_LL_HEAD_LEN     6
	#define SHP_CFG_L1_L1_SHP_LL_HEAD_DEFAULT 0x0

struct shp_cfg_l1 {
	a_uint32_t  l1_shp_ll_tail:6;
	a_uint32_t  _reserved0:10;
	a_uint32_t  l1_shp_ll_head:6;
	a_uint32_t  _reserved1:10;
};

union shp_cfg_l1_u {
	a_uint32_t val;
	struct shp_cfg_l1 bf;
};

#endif
