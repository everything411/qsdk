/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_ACL_REG_H_
#define _JHPPE_ACL_REG_H_

/*[table] L2_DSCP_PBIT_MAP_TBL*/
#define L2_DSCP_PBIT_MAP_TBL
#define L2_DSCP_PBIT_MAP_TBL_ADDRESS 0x38000
#define L2_DSCP_PBIT_MAP_TBL_NUM     256
#define L2_DSCP_PBIT_MAP_TBL_INC     0x4
#define L2_DSCP_PBIT_MAP_TBL_TYPE    REG_TYPE_RW
#define L2_DSCP_PBIT_MAP_TBL_DEFAULT 0x0
	/*[field] PCP_0*/
	#define L2_DSCP_PBIT_MAP_TBL_PCP_0
	#define L2_DSCP_PBIT_MAP_TBL_PCP_0_OFFSET  0
	#define L2_DSCP_PBIT_MAP_TBL_PCP_0_LEN     3
	#define L2_DSCP_PBIT_MAP_TBL_PCP_0_DEFAULT 0x0
	/*[field] PCP_1*/
	#define L2_DSCP_PBIT_MAP_TBL_PCP_1
	#define L2_DSCP_PBIT_MAP_TBL_PCP_1_OFFSET  3
	#define L2_DSCP_PBIT_MAP_TBL_PCP_1_LEN     3
	#define L2_DSCP_PBIT_MAP_TBL_PCP_1_DEFAULT 0x0

struct l2_dscp_pbit_map_tbl {
	a_uint32_t  pcp_0:3;
	a_uint32_t  pcp_1:3;
	a_uint32_t  _reserved0:26;
};

union l2_dscp_pbit_map_tbl_u {
	a_uint32_t val;
	struct l2_dscp_pbit_map_tbl bf;
};

/*[register] PRE_IPO_RULE_INNER_OUTER*/
#define PRE_IPO_RULE_INNER_OUTER
#define PRE_IPO_RULE_INNER_OUTER_ADDRESS 0x4500
#define PRE_IPO_RULE_INNER_OUTER_NUM     16
#define PRE_IPO_RULE_INNER_OUTER_INC     0x4
#define PRE_IPO_RULE_INNER_OUTER_TYPE    REG_TYPE_RW
#define PRE_IPO_RULE_INNER_OUTER_DEFAULT 0x0
	/*[field] INNER_OUTER_SEL*/
	#define PRE_IPO_RULE_INNER_OUTER_INNER_OUTER_SEL
	#define PRE_IPO_RULE_INNER_OUTER_INNER_OUTER_SEL_OFFSET  0
	#define PRE_IPO_RULE_INNER_OUTER_INNER_OUTER_SEL_LEN     1
	#define PRE_IPO_RULE_INNER_OUTER_INNER_OUTER_SEL_DEFAULT 0x0

struct pre_ipo_rule_inner_outer {
	a_uint32_t  inner_outer_sel:1;
	a_uint32_t  _reserved0:31;
};

union pre_ipo_rule_inner_outer_u {
	a_uint32_t val;
	struct pre_ipo_rule_inner_outer bf;
};

#endif
