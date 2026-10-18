/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_eg_src_port_remap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_src_port_remap_u *value)
{
	if (index >= EG_SRC_PORT_REMAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SRC_PORT_REMAP_ADDRESS + \
				index * EG_SRC_PORT_REMAP_INC,
				&value->val);
}

sw_error_t
jhppe_eg_src_port_remap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_src_port_remap_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SRC_PORT_REMAP_ADDRESS + \
				index * EG_SRC_PORT_REMAP_INC,
				value->val);
}

sw_error_t
jhppe_port_isol_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_isol_action_u *value)
{
	if (index >= PORT_ISOL_ACTION_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_ISOL_ACTION_ADDRESS + \
				index * PORT_ISOL_ACTION_INC,
				&value->val);
}

sw_error_t
jhppe_port_isol_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_isol_action_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_ISOL_ACTION_ADDRESS + \
				index * PORT_ISOL_ACTION_INC,
				value->val);
}

sw_error_t
jhppe_port_isol_action_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_isol_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_port_isol_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.action;
	return ret;
}

sw_error_t
jhppe_port_isol_action_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_isol_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_port_isol_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.action = value;
	ret = jhppe_port_isol_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_post_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_POST_TBL_ADDRESS + \
				index * L2_VP_PORT_POST_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_post_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l2_vp_port_post_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_post_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_POST_TBL_ADDRESS + \
				index * L2_VP_PORT_POST_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_post_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_ctag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_ctag_fmt;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_ctag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_ctag_fmt = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_stag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_stag_fmt;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_stag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_stag_fmt = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_vlan_fltr_cmd;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_vlan_fltr_cmd = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_vlan_fmt_valid;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_vlan_fmt_valid = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_phy_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_phy_port;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_phy_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_phy_port = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_1 << 2 | \
		reg_val.bf.enq_service_code_0;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_1 = value >> 2;
	reg_val.bf.enq_service_code_0 = value & (((a_uint64_t)1<<2)-1);
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_en;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_en = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_queue_dis;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_queue_dis = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_extra_header_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.extra_header_len;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_extra_header_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.extra_header_len = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile_en;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile_en = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_mirror_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mirror_en;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_mirror_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mirror_en = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_mtu_check_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu_check_type;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_mtu_check_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu_check_type = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.physical_port;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.physical_port = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.physical_port_mtu_check_en;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.physical_port_mtu_check_en = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_context_active_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_context_active;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_context_active_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_context_active = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_eg_data_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_eg_data_valid;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_eg_data_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_eg_data_valid = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_profile;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_profile = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_state_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_state_check_en;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_state_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_state_check_en = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_type;
	return ret;
}

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_type = value;
	ret = jhppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vp_isol_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_isol_action_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + VP_ISOL_ACTION_TBL_ADDRESS + \
				index * VP_ISOL_ACTION_TBL_INC,
				value->val,
				sizeof(union vp_isol_action_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_vp_isol_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_isol_action_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + VP_ISOL_ACTION_TBL_ADDRESS + \
				index * VP_ISOL_ACTION_TBL_INC,
				value->val,
				sizeof(union vp_isol_action_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_vp_isol_action_tbl_vp_profile_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union vp_isol_action_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vp_isol_action_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.vp_profile_action_1 << 32 | \
		reg_val.bf.vp_profile_action_0;
	return ret;
}

sw_error_t
jhppe_vp_isol_action_tbl_vp_profile_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union vp_isol_action_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vp_isol_action_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_profile_action_1 = value >> 32;
	reg_val.bf.vp_profile_action_0 = value & (((a_uint64_t)1<<32)-1);
	ret = jhppe_vp_isol_action_tbl_set(dev_id, index, &reg_val);
	return ret;
}

