/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
hmsppe_pass_through_ctrl_get(
		a_uint32_t dev_id,
		union pass_through_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PASS_THROUGH_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hmsppe_pass_through_ctrl_set(
		a_uint32_t dev_id,
		union pass_through_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PASS_THROUGH_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hmsppe_pass_through_ctrl_copy_cpu_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union pass_through_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_pass_through_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.copy_cpu_en;
	return ret;
}

sw_error_t
hmsppe_pass_through_ctrl_copy_cpu_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union pass_through_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_pass_through_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.copy_cpu_en = value;
	ret = hmsppe_pass_through_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_get(
		a_uint32_t dev_id,
		union dot1p_mapper_default_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_DEFAULT_ADDRESS,
				&value->val);
}

sw_error_t
hmsppe_dot1p_mapper_default_set(
		a_uint32_t dev_id,
		union dot1p_mapper_default_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_DEFAULT_ADDRESS,
				value->val);
}

sw_error_t
hmsppe_dot1p_mapper_default_dei_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.dei;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_dei_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dei = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.dscp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dscp = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_dscp_mask_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.dscp_mask;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_dscp_mask_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dscp_mask = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.miss_action;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.miss_action = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.pcp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_vid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	*value = reg_val.bf.vid;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_default_vid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union dot1p_mapper_default_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_default_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid = value;
	ret = hmsppe_dot1p_mapper_default_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_rule_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_RULE_ADDRESS + \
				index * DOT1P_MAPPER_RULE_INC,
				value->val,
				sizeof(union dot1p_mapper_rule_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_dot1p_mapper_rule_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_rule_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_RULE_ADDRESS + \
				index * DOT1P_MAPPER_RULE_INC,
				value->val,
				sizeof(union dot1p_mapper_rule_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_dot1p_mapper_rule_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dei;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dei = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dei_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dei_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dest_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dest_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dest_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dest_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dest_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dest_info;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dest_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dest_info = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf1.dscp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf1.dscp = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf1.dscp_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf1.dscp_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_gem_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.gem_port;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_gem_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.gem_port = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_type;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_type = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_priority_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.priority_type;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_priority_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.priority_type = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_src_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.src_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_src_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.src_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_src_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.src_info;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_src_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.src_info = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid_1 << 10 | \
		reg_val.bf.vid_0;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid_1 = value >> 10;
	reg_val.bf.vid_0 = value & (((a_uint64_t)1<<10)-1);
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid_incl;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_rule_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_rule_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_rule_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid_incl = value;
	ret = hmsppe_dot1p_mapper_rule_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_action_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_ACTION_ADDRESS + \
				index * DOT1P_MAPPER_ACTION_INC,
				value->val,
				sizeof(union dot1p_mapper_action_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_dot1p_mapper_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_action_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + DOT1P_MAPPER_ACTION_ADDRESS + \
				index * DOT1P_MAPPER_ACTION_INC,
				value->val,
				sizeof(union dot1p_mapper_action_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_dot1p_mapper_action_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.action;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.action = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_phy_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_phy_port;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_phy_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_phy_port = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_override_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_port_override;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_override_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_port_override = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_port_vp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_port_vp = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enqueue_vp_1 << 2 | \
		reg_val.bf.enqueue_vp_0;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enqueue_vp_1 = value >> 2;
	reg_val.bf.enqueue_vp_0 = value & (((a_uint64_t)1<<2)-1);
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enqueue_vp_valid;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enqueue_vp_valid = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_gem_port_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.gem_port_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_gem_port_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.gem_port_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_dp;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_dp = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_dp_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_dp_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_pri;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_pri = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_pri_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_pri_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_ds_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_ds_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_ds_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_ds_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_id;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_policer_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_id = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.post_bypass_bitmap;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.post_bypass_bitmap = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.service_code;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.service_code = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.service_code_en;
	return ret;
}

sw_error_t
hmsppe_dot1p_mapper_action_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dot1p_mapper_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_dot1p_mapper_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.service_code_en = value;
	ret = hmsppe_dot1p_mapper_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_ctrl_u *value)
{
	if (index >= GEM_PORT_MAPPING_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRX_BASE_ADDR + GEM_PORT_MAPPING_CTRL_ADDRESS + \
				index * GEM_PORT_MAPPING_CTRL_INC,
				&value->val);
}

sw_error_t
hmsppe_gem_port_mapping_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRX_BASE_ADDR + GEM_PORT_MAPPING_CTRL_ADDRESS + \
				index * GEM_PORT_MAPPING_CTRL_INC,
				value->val);
}

sw_error_t
hmsppe_gem_port_mapping_ctrl_mapping_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mapping_en;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_ctrl_mapping_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mapping_en = value;
	ret = hmsppe_gem_port_mapping_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				PRX_BASE_ADDR + GEM_PORT_MAPPING_TBL_ADDRESS + \
				index * GEM_PORT_MAPPING_TBL_INC,
				value->val,
				sizeof(union gem_port_mapping_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_gem_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				PRX_BASE_ADDR + GEM_PORT_MAPPING_TBL_ADDRESS + \
				index * GEM_PORT_MAPPING_TBL_INC,
				value->val,
				sizeof(union gem_port_mapping_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dst_info;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dst_info = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dst_info_valid;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dst_info_valid = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_dp;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_dp = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_pri;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_pri = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_dp_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.int_pri_dp_valid;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_dp_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.int_pri_dp_valid = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_port_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_valid;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_port_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_valid = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_vp;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_vp = value;
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.service_code_1 << 1 | \
		reg_val.bf.service_code_0;
	return ret;
}

sw_error_t
hmsppe_gem_port_mapping_tbl_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union gem_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hmsppe_gem_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.service_code_1 = value >> 1;
	reg_val.bf.service_code_0 = value & (((a_uint64_t)1<<1)-1);
	ret = hmsppe_gem_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

