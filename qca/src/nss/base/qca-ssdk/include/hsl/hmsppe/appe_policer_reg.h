/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_POLICER_REG_H_
#define _APPE_POLICER_REG_H_

/*[register] IN_METER_HEAD_REG*/
#define IN_METER_HEAD_REG
#define IN_METER_HEAD_REG_ADDRESS 0x88
#define IN_METER_HEAD_REG_NUM     1
#define IN_METER_HEAD_REG_INC     0x4
#define IN_METER_HEAD_REG_TYPE    REG_TYPE_RW
#define IN_METER_HEAD_REG_DEFAULT 0x0
	/*[field] METER_LL_HEAD*/
	#define IN_METER_HEAD_REG_METER_LL_HEAD
	#define IN_METER_HEAD_REG_METER_LL_HEAD_OFFSET  7
	#define IN_METER_HEAD_REG_METER_LL_HEAD_LEN     9
	#define IN_METER_HEAD_REG_METER_LL_HEAD_DEFAULT 0x0
	/*[field] METER_LL_TAIL*/
	#define IN_METER_HEAD_REG_METER_LL_TAIL
	#define IN_METER_HEAD_REG_METER_LL_TAIL_OFFSET  22
	#define IN_METER_HEAD_REG_METER_LL_TAIL_LEN     9
	#define IN_METER_HEAD_REG_METER_LL_TAIL_DEFAULT 0x0

struct in_meter_head_reg {
	a_uint32_t  _reserved0:7;
	a_uint32_t  meter_ll_head:9;
	a_uint32_t  _reserved1:6;
	a_uint32_t  meter_ll_tail:9;
	a_uint32_t  _reserved2:1;
};

union in_meter_head_reg_u {
	a_uint32_t val;
	struct in_meter_head_reg bf;
};

/*[table] DSCP_REMAP_TBL*/
#define DSCP_REMAP_TBL
#define DSCP_REMAP_TBL_ADDRESS 0xa000
#define DSCP_REMAP_TBL_NUM     128
#define DSCP_REMAP_TBL_INC     0x10
#define DSCP_REMAP_TBL_TYPE    REG_TYPE_RW
#define DSCP_REMAP_TBL_DEFAULT 0x0
	/*[field] DSCP*/
	#define DSCP_REMAP_TBL_DSCP
	#define DSCP_REMAP_TBL_DSCP_OFFSET  0
	#define DSCP_REMAP_TBL_DSCP_LEN     6
	#define DSCP_REMAP_TBL_DSCP_DEFAULT 0x0
	/*[field] PCP*/
	#define DSCP_REMAP_TBL_PCP
	#define DSCP_REMAP_TBL_PCP_OFFSET  6
	#define DSCP_REMAP_TBL_PCP_LEN     3
	#define DSCP_REMAP_TBL_PCP_DEFAULT 0x0
	/*[field] DEI*/
	#define DSCP_REMAP_TBL_DEI
	#define DSCP_REMAP_TBL_DEI_OFFSET  9
	#define DSCP_REMAP_TBL_DEI_LEN     1
	#define DSCP_REMAP_TBL_DEI_DEFAULT 0x0

struct dscp_remap_tbl {
	a_uint32_t  dscp:6;
	a_uint32_t  pcp:3;
	a_uint32_t  dei:1;
	a_uint32_t  _reserved0:22;
};

union dscp_remap_tbl_u {
	a_uint32_t val;
	struct dscp_remap_tbl bf;
};

#endif
