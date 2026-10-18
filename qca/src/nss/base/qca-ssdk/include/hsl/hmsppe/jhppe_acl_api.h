/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_ACL_API_H_
#define _JHPPE_ACL_API_H_

#define L2_DSCP_PBIT_MAP_TBL_MAX_ENTRY	L2_DSCP_PBIT_MAP_TBL_NUM

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PRE_IPO_RULE_INNER_OUTER_MAX_ENTRY	PRE_IPO_RULE_INNER_OUTER_NUM

sw_error_t
jhppe_pre_ipo_rule_inner_outer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_inner_outer_u *value);

sw_error_t
jhppe_pre_ipo_rule_inner_outer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_inner_outer_u *value);

sw_error_t
jhppe_pre_ipo_rule_inner_outer_inner_outer_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_pre_ipo_rule_inner_outer_inner_outer_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
