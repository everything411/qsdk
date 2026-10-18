/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_ipr_udf_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_ctrl_u *value)
{
	if (index >= IPR_UDF_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_CTRL_ADDRESS + \
				index * IPR_UDF_CTRL_INC,
				&value->val);
}

sw_error_t
httppe_ipr_udf_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_CTRL_ADDRESS + \
				index * IPR_UDF_CTRL_INC,
				value->val);
}

sw_error_t
httppe_ipr_udf_profile_base_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_base_u *value)
{
	if (index >= IPR_UDF_PROFILE_BASE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_PROFILE_BASE_ADDRESS + \
				index * IPR_UDF_PROFILE_BASE_INC,
				&value->val);
}

sw_error_t
httppe_ipr_udf_profile_base_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_base_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_PROFILE_BASE_ADDRESS + \
				index * IPR_UDF_PROFILE_BASE_INC,
				value->val);
}

sw_error_t
httppe_ipr_udf_profile_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_offset_u *value)
{
	if (index >= IPR_UDF_PROFILE_OFFSET_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_PROFILE_OFFSET_ADDRESS + \
				index * IPR_UDF_PROFILE_OFFSET_INC,
				&value->val);
}

sw_error_t
httppe_ipr_udf_profile_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_offset_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + IPR_UDF_PROFILE_OFFSET_ADDRESS + \
				index * IPR_UDF_PROFILE_OFFSET_INC,
				value->val);
}

sw_error_t
httppe_ipo_rule_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_rule_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPO_CSR_BASE_ADDR + IPO_RULE_REG_ADDRESS + \
				index * IPO_RULE_REG_INC,
				value->val,
				3);
}

sw_error_t
httppe_ipo_rule_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_rule_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPO_CSR_BASE_ADDR + IPO_RULE_REG_ADDRESS + \
				index * IPO_RULE_REG_INC,
				value->val,
				3);
}

sw_error_t
httppe_ipo_mask_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_mask_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPO_CSR_BASE_ADDR + IPO_MASK_REG_ADDRESS + \
				index * IPO_MASK_REG_INC,
				value->val,
				2);
}

sw_error_t
httppe_ipo_mask_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_mask_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPO_CSR_BASE_ADDR + IPO_MASK_REG_ADDRESS + \
				index * IPO_MASK_REG_INC,
				value->val,
				2);
}

sw_error_t
httppe_ipo_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_action_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IPO_ACTION_ADDRESS + \
				index * IPO_ACTION_INC,
				value->val,
				sizeof(union ipo_action_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipo_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_action_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IPO_ACTION_ADDRESS + \
				index * IPO_ACTION_INC,
				value->val,
				sizeof(union ipo_action_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipo_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_cnt_tbl_u *value)
{
		return hppe_reg_tbl_get(
					dev_id,
					INGRESS_POLICER_BASE_ADDR + IPO_CNT_TBL_ADDRESS + \
					index * IPO_CNT_TBL_INC,
					value->val,
					sizeof(union ipo_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_ipo_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_cnt_tbl_u *value)
{
		return hppe_reg_tbl_set(
					dev_id,
					INGRESS_POLICER_BASE_ADDR + IPO_CNT_TBL_ADDRESS + \
					index * IPO_CNT_TBL_INC,
					value->val,
					sizeof(union ipo_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l2_dscp_pbit_map_tbl_get(
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
httppe_l2_dscp_pbit_map_tbl_set(
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
httppe_l2_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_0;
	return ret;
}

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_0 = value;
	ret = httppe_l2_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_1;
	return ret;
}

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_1 = value;
	ret = httppe_l2_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}
