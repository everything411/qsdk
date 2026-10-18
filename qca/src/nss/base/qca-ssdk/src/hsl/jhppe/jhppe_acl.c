/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value)
{
	if (index >= L2_DSCP_PBIT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * L2_DSCP_PBIT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * L2_DSCP_PBIT_MAP_TBL_INC,
				value->val);
}

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_0;
	return ret;
}

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_0 = value;
	ret = jhppe_l2_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_1;
	return ret;
}

sw_error_t
jhppe_l2_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_1 = value;
	ret = jhppe_l2_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_rule_inner_outer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_inner_outer_u *value)
{
	if (index >= PRE_IPO_RULE_INNER_OUTER_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + PRE_IPO_RULE_INNER_OUTER_ADDRESS + \
				index * PRE_IPO_RULE_INNER_OUTER_INC,
				&value->val);
}

sw_error_t
jhppe_pre_ipo_rule_inner_outer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_rule_inner_outer_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + PRE_IPO_RULE_INNER_OUTER_ADDRESS + \
				index * PRE_IPO_RULE_INNER_OUTER_INC,
				value->val);
}

sw_error_t
jhppe_pre_ipo_rule_inner_outer_inner_outer_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_rule_inner_outer_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_rule_inner_outer_get(dev_id, index, &reg_val);
	*value = reg_val.bf.inner_outer_sel;
	return ret;
}

sw_error_t
jhppe_pre_ipo_rule_inner_outer_inner_outer_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_rule_inner_outer_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_rule_inner_outer_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.inner_outer_sel = value;
	ret = jhppe_pre_ipo_rule_inner_outer_set(dev_id, index, &reg_val);
	return ret;
}

