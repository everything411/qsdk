/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_port_isol_action_get(
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
httppe_port_isol_action_set(
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
httppe_port_isol_action_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_isol_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_port_isol_action_get(dev_id, index, &reg_val);
	*value = reg_val.bf.action;
	return ret;
}

sw_error_t
httppe_port_isol_action_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_isol_action_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_port_isol_action_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.action = value;
	ret = httppe_port_isol_action_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_TBL_ADDRESS + \
				index * L2_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l2_vp_port_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_TBL_ADDRESS + \
				index * L2_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l2_vp_port_tbl_app_ctrl_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.app_ctrl_profile_1 << 6 | \
		reg_val.bf.app_ctrl_profile_0;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_app_ctrl_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.app_ctrl_profile_1 = value >> 6;
	reg_val.bf.app_ctrl_profile_0 = value & (((a_uint64_t)1<<6)-1);
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dst_info;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dst_info = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_exception_fmt_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.exception_fmt_ctrl;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_exception_fmt_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.exception_fmt_ctrl = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_invalid_vsi_forwarding_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.invalid_vsi_forwarding_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_invalid_vsi_forwarding_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.invalid_vsi_forwarding_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_isol_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_isol_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_isol_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_isol_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_cnt;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_cnt = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_exceed_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_exceed_fwd;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_lrn_lmt_exceed_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_exceed_fwd = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_mirror_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mirror_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_mirror_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mirror_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_new_addr_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.new_addr_fwd_cmd;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_new_addr_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.new_addr_fwd_cmd = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_new_addr_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.new_addr_lrn_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_new_addr_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.new_addr_lrn_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_policer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_policer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_policer_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_index;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_policer_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_index = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_port_isolation_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_isolation_bitmap_1 << 1 | \
		reg_val.bf.port_isolation_bitmap_0;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_port_isolation_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_isolation_bitmap_1 = value >> 1;
	reg_val.bf.port_isolation_bitmap_0 = value & (((a_uint64_t)1<<1)-1);
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_promisc_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.promisc_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_promisc_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.promisc_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_station_move_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.station_move_fwd_cmd;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_station_move_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.station_move_fwd_cmd = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_station_move_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.station_move_lrn_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_station_move_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.station_move_lrn_en = value;
	ret = httppe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_get(
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
httppe_l2_vp_port_post_tbl_set(
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
httppe_l2_vp_port_post_tbl_eg_ctag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_ctag_fmt;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_ctag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_ctag_fmt = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_stag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_stag_fmt;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_stag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_stag_fmt = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_vlan_fltr_cmd;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_vlan_fltr_cmd = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.eg_vlan_fmt_valid;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.eg_vlan_fmt_valid = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_phy_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_phy_port;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_phy_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_phy_port = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_1 << 2 | \
		reg_val.bf.enq_service_code_0;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_1 = value >> 2;
	reg_val.bf.enq_service_code_0 = value & (((a_uint64_t)1<<2)-1);
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_en = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enq_service_code_queue_dis;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enq_service_code_queue_dis = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_extra_header_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.extra_header_len;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_extra_header_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.extra_header_len = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_isol_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_isol_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_isol_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_isol_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile_en = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_mirror_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mirror_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_mirror_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mirror_en = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_mtu_check_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mtu_check_type;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_mtu_check_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mtu_check_type = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.physical_port;
	return ret;
}

sw_error_t
httppe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.physical_port = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.physical_port_mtu_check_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.physical_port_mtu_check_en = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_context_active_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_context_active;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_context_active_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_context_active = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_eg_data_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_eg_data_valid;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_eg_data_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_eg_data_valid = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_profile;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_profile = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_state_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_state_check_en;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_state_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_state_check_en = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vp_type;
	return ret;
}

sw_error_t
httppe_l2_vp_port_post_tbl_vp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_post_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_l2_vp_port_post_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_type = value;
	ret = httppe_l2_vp_port_post_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vp_isol_action_tbl_get(
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
httppe_vp_isol_action_tbl_set(
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
httppe_vp_isol_action_tbl_vp_profile_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union vp_isol_action_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vp_isol_action_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.vp_profile_action_1 << 32 | \
		reg_val.bf.vp_profile_action_0;
	return ret;
}

sw_error_t
httppe_vp_isol_action_tbl_vp_profile_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union vp_isol_action_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vp_isol_action_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_profile_action_1 = value >> 32;
	reg_val.bf.vp_profile_action_0 = value & (((a_uint64_t)1<<32)-1);
	ret = httppe_vp_isol_action_tbl_set(dev_id, index, &reg_val);
	return ret;
}
