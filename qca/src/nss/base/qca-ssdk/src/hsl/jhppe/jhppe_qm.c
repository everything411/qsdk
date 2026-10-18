/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_mc_enq_ctrl_get(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + MC_ENQ_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_mc_enq_ctrl_set(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + MC_ENQ_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_mc_enq_ctrl_dot1p_pon_vp_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.dot1p_pon_vp;
	return ret;
}

sw_error_t
jhppe_mc_enq_ctrl_dot1p_pon_vp_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dot1p_pon_vp = value;
	ret = jhppe_mc_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_mc_enq_ctrl_uc_enq_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.uc_enq_en;
	return ret;
}

sw_error_t
jhppe_mc_enq_ctrl_uc_enq_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.uc_enq_en = value;
	ret = jhppe_mc_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_mc_enq_ctrl_uc_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.uc_port_id;
	return ret;
}

sw_error_t
jhppe_mc_enq_ctrl_uc_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union mc_enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mc_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.uc_port_id = value;
	ret = jhppe_mc_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_ext_get(
		a_uint32_t dev_id,
		union enq_ctrl_ext_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + ENQ_CTRL_EXT_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_enq_ctrl_ext_set(
		a_uint32_t dev_id,
		union enq_ctrl_ext_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + ENQ_CTRL_EXT_ADDRESS,
				value->val);
}

sw_error_t
jhppe_uniq_flowctrl_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_flowctrl_status_u *value)
{
	if (index >= UNIQ_FLOWCTRL_STATUS_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + UNIQ_FLOWCTRL_STATUS_ADDRESS + \
				index * UNIQ_FLOWCTRL_STATUS_INC,
				&value->val);
}

sw_error_t
jhppe_uniq_flowctrl_status_uniq_flowctrl_status_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union uniq_flowctrl_status_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_uniq_flowctrl_status_get(dev_id, index, &reg_val);
	*value = reg_val.bf.uniq_flowctrl_status_status;
	return ret;
}

sw_error_t
jhppe_uniq_q_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_q_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + UNIQ_Q_MAP_TBL_ADDRESS + \
				index * UNIQ_Q_MAP_TBL_INC,
				value->val,
				sizeof(union uniq_q_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_uniq_q_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_q_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + UNIQ_Q_MAP_TBL_ADDRESS + \
				index * UNIQ_Q_MAP_TBL_INC,
				value->val,
				sizeof(union uniq_q_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_enq_ctrl_get(
		a_uint32_t dev_id,
		union enq_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + ENQ_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_enq_ctrl_set(
		a_uint32_t dev_id,
		union enq_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + ENQ_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_enq_ctrl_pass_through0_direc_enq_dis_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through0_direc_enq_dis;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_direc_enq_dis_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through0_direc_enq_dis = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through0_src_profile;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through0_src_profile = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_eg_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through0_src_profile_eg_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_eg_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through0_src_profile_eg_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through0_src_profile_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through0_src_profile_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_direc_enq_dis_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through1_direc_enq_dis;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_direc_enq_dis_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through1_direc_enq_dis = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through1_src_profile;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through1_src_profile = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_eg_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through1_src_profile_eg_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_eg_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through1_src_profile_eg_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through1_src_profile_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through1_src_profile_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_direc_enq_dis_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through2_direc_enq_dis;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_direc_enq_dis_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through2_direc_enq_dis = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through2_src_profile;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through2_src_profile = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_eg_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through2_src_profile_eg_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_eg_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through2_src_profile_eg_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through2_src_profile_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through2_src_profile_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_direc_enq_dis_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through3_direc_enq_dis;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_direc_enq_dis_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through3_direc_enq_dis = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through3_src_profile;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through3_src_profile = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_eg_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through3_src_profile_eg_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_eg_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through3_src_profile_eg_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pass_through3_src_profile_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pass_through3_src_profile_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_qid_mismatch_enq_cpu_code_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.qid_mismatch_enq_cpu_code;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_qid_mismatch_enq_cpu_code_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.qid_mismatch_enq_cpu_code = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_queue_map_rule_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.queue_map_rule_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_queue_map_rule_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.queue_map_rule_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_enq_ctrl_uc_enq_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.uc_enq_en;
	return ret;
}

sw_error_t
jhppe_enq_ctrl_uc_enq_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union enq_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_enq_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.uc_enq_en = value;
	ret = jhppe_enq_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_cm_ctrl_0_get(
		a_uint32_t dev_id,
		union cm_ctrl_0_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_0_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_cm_ctrl_0_set(
		a_uint32_t dev_id,
		union cm_ctrl_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_0_ADDRESS,
				value->val);
}

sw_error_t
jhppe_cm_ctrl_1_get(
		a_uint32_t dev_id,
		union cm_ctrl_1_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_1_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_cm_ctrl_1_set(
		a_uint32_t dev_id,
		union cm_ctrl_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_1_ADDRESS,
				value->val);
}

sw_error_t
jhppe_cm_ctrl_2_get(
		a_uint32_t dev_id,
		union cm_ctrl_2_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_2_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_cm_ctrl_2_set(
		a_uint32_t dev_id,
		union cm_ctrl_2_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_2_ADDRESS,
				value->val);
}

sw_error_t
jhppe_cm_ctrl_3_get(
		a_uint32_t dev_id,
		union cm_ctrl_3_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_3_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_cm_ctrl_3_set(
		a_uint32_t dev_id,
		union cm_ctrl_3_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_3_ADDRESS,
				value->val);
}

sw_error_t
jhppe_cm_ctrl_4_get(
		a_uint32_t dev_id,
		union cm_ctrl_4_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_4_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_cm_ctrl_4_set(
		a_uint32_t dev_id,
		union cm_ctrl_4_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + CM_CTRL_4_ADDRESS,
				value->val);
}

sw_error_t
jhppe_ddrq_ctrl_get(
		a_uint32_t dev_id,
		union ddrq_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_ddrq_ctrl_set(
		a_uint32_t dev_id,
		union ddrq_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sram_qcnt_ctrl_get(
		a_uint32_t dev_id,
		union sram_qcnt_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_QCNT_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sram_qcnt_ctrl_set(
		a_uint32_t dev_id,
		union sram_qcnt_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_QCNT_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_status_u *value)
{
	if (index >= SRAM_UNIQ_CNT_STATUS_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_STATUS_ADDRESS + \
				index * SRAM_UNIQ_CNT_STATUS_INC,
				&value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_status_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_STATUS_ADDRESS + \
				index * SRAM_UNIQ_CNT_STATUS_INC,
				value->val);
}

sw_error_t
jhppe_sram_mulq_cnt_status_get(
		a_uint32_t dev_id,
		union sram_mulq_cnt_status_u *value)
{
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_STATUS_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sram_mulq_cnt_status_set(
		a_uint32_t dev_id,
		union sram_mulq_cnt_status_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_STATUS_ADDRESS,
				value->val);
}

sw_error_t
jhppe_mcast_priority_map8_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map8_u *value)
{
	if (index >= MCAST_PRIORITY_MAP8_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP8_ADDRESS + \
				index * MCAST_PRIORITY_MAP8_INC,
				&value->val);
}

sw_error_t
jhppe_mcast_priority_map8_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map8_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MCAST_PRIORITY_MAP8_ADDRESS + \
				index * MCAST_PRIORITY_MAP8_INC,
				value->val);
}

sw_error_t
jhppe_mcast_priority_map8_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mcast_priority_map8_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mcast_priority_map8_get(dev_id, index, &reg_val);
	*value = reg_val.bf.class;
	return ret;
}

sw_error_t
jhppe_mcast_priority_map8_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mcast_priority_map8_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mcast_priority_map8_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.class = value;
	ret = jhppe_mcast_priority_map8_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_ucast_queue_map_rule_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_rule_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_RULE_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_RULE_TBL_INC,
				value->val,
				sizeof(union ucast_queue_map_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_ucast_queue_map_rule_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_rule_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_RULE_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_RULE_TBL_INC,
				value->val,
				sizeof(union ucast_queue_map_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_ucast_queue_map_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_action_tbl_u *value)
{
	if (index >= UCAST_QUEUE_MAP_ACTION_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_ACTION_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_ACTION_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ucast_queue_map_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_action_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_MAP_ACTION_TBL_ADDRESS + \
				index * UCAST_QUEUE_MAP_ACTION_TBL_INC,
				value->val);
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_ctrl_tbl_u *value)
{
	if (index >= UCAST_QUEUE_CTRL_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_CTRL_TBL_ADDRESS + \
				index * UCAST_QUEUE_CTRL_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_ctrl_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + UCAST_QUEUE_CTRL_TBL_ADDRESS + \
				index * UCAST_QUEUE_CTRL_TBL_INC,
				value->val);
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_ddrq_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ucast_queue_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_ucast_queue_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ddrq_en;
	return ret;
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_ddrq_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ucast_queue_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_ucast_queue_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ddrq_en = value;
	ret = jhppe_ucast_queue_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_qid_mismatch_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ucast_queue_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_ucast_queue_ctrl_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.qid_mismatch_check_en;
	return ret;
}

sw_error_t
jhppe_ucast_queue_ctrl_tbl_qid_mismatch_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ucast_queue_ctrl_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_ucast_queue_ctrl_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.qid_mismatch_check_en = value;
	ret = jhppe_ucast_queue_ctrl_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_ddrq_ensch_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_ensch_tbl_u *value)
{
	if (index >= DDRQ_ENSCH_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_ENSCH_TBL_ADDRESS + \
				index * DDRQ_ENSCH_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ddrq_ensch_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_ensch_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_ENSCH_TBL_ADDRESS + \
				index * DDRQ_ENSCH_TBL_INC,
				value->val);
}

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_pkt_cnt_tbl_u *value)
{
	if (index >= DDRQ_PKT_CNT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_PKT_CNT_TBL_ADDRESS + \
				index * DDRQ_PKT_CNT_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_pkt_cnt_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_PKT_CNT_TBL_ADDRESS + \
				index * DDRQ_PKT_CNT_TBL_INC,
				value->val);
}

sw_error_t
jhppe_ddrq_byte_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_byte_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_BYTE_CNT_TBL_ADDRESS + \
				index * DDRQ_BYTE_CNT_TBL_INC,
				value->val,
				sizeof(union ddrq_byte_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_ddrq_byte_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_byte_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_BYTE_CNT_TBL_ADDRESS + \
				index * DDRQ_BYTE_CNT_TBL_INC,
				value->val,
				sizeof(union ddrq_byte_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sram_uniq_cnt_qid_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_qid_map_u *value)
{
	if (index >= SRAM_UNIQ_CNT_QID_MAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_QID_MAP_ADDRESS + \
				index * SRAM_UNIQ_CNT_QID_MAP_INC,
				&value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_qid_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_qid_map_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_QID_MAP_ADDRESS + \
				index * SRAM_UNIQ_CNT_QID_MAP_INC,
				value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_cfg_tbl_u *value)
{
	if (index >= SRAM_UNIQ_CNT_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_CFG_TBL_ADDRESS + \
				index * SRAM_UNIQ_CNT_CFG_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_CFG_TBL_ADDRESS + \
				index * SRAM_UNIQ_CNT_CFG_TBL_INC,
				value->val);
}

sw_error_t
jhppe_sram_uniq_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_TBL_ADDRESS + \
				index * SRAM_UNIQ_CNT_TBL_INC,
				value->val,
				sizeof(union sram_uniq_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sram_uniq_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_UNIQ_CNT_TBL_ADDRESS + \
				index * SRAM_UNIQ_CNT_TBL_INC,
				value->val,
				sizeof(union sram_uniq_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_cfg_tbl_u *value)
{
	if (index >= SRAM_MULQ_CNT_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_CFG_TBL_ADDRESS + \
				index * SRAM_MULQ_CNT_CFG_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_CFG_TBL_ADDRESS + \
				index * SRAM_MULQ_CNT_CFG_TBL_INC,
				value->val);
}

sw_error_t
jhppe_sram_mulq_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_TBL_ADDRESS + \
				index * SRAM_MULQ_CNT_TBL_INC,
				value->val,
				sizeof(union sram_mulq_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sram_mulq_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + SRAM_MULQ_CNT_TBL_ADDRESS + \
				index * SRAM_MULQ_CNT_TBL_INC,
				value->val,
				sizeof(union sram_mulq_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_map_tbl_u *value)
{
	if (index >= QUEUE_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + QUEUE_MAP_TBL_ADDRESS + \
				index * QUEUE_MAP_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + QUEUE_MAP_TBL_ADDRESS + \
				index * QUEUE_MAP_TBL_INC,
				value->val);
}

sw_error_t
jhppe_tcont_byte_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_byte_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + TCONT_BYTE_CNT_TBL_ADDRESS + \
				index * TCONT_BYTE_CNT_TBL_INC,
				value->val,
				sizeof(union tcont_byte_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_tcont_byte_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_byte_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + TCONT_BYTE_CNT_TBL_ADDRESS + \
				index * TCONT_BYTE_CNT_TBL_INC,
				value->val,
				sizeof(union tcont_byte_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_ddrq_debit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_debit_tbl_u *value)
{
	if (index >= DDRQ_DEBIT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_DEBIT_TBL_ADDRESS + \
				index * DDRQ_DEBIT_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ddrq_debit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_debit_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_DEBIT_TBL_ADDRESS + \
				index * DDRQ_DEBIT_TBL_INC,
				value->val);
}

sw_error_t
jhppe_tcont_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_credit_tbl_u *value)
{
	if (index >= TCONT_CREDIT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + TCONT_CREDIT_TBL_ADDRESS + \
				index * TCONT_CREDIT_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_tcont_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_credit_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + TCONT_CREDIT_TBL_ADDRESS + \
				index * TCONT_CREDIT_TBL_INC,
				value->val);
}

sw_error_t
jhppe_ddrq_comp_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_comp_tbl_u *value)
{
	if (index >= DDRQ_COMP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_COMP_TBL_ADDRESS + \
				index * DDRQ_COMP_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_ddrq_comp_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_comp_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + DDRQ_COMP_TBL_ADDRESS + \
				index * DDRQ_COMP_TBL_INC,
				value->val);
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p8_tbl_u *value)
{
	if (index >= OQ_LL_MUL_P8_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P8_TBL_ADDRESS + \
				index * OQ_LL_MUL_P8_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p8_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + OQ_LL_MUL_P8_TBL_ADDRESS + \
				index * OQ_LL_MUL_P8_TBL_INC,
				value->val);
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.egress_mirr;
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.egress_mirr = value;
	ret = jhppe_oq_ll_mul_p8_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ingress_mirr;
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ingress_mirr = value;
	ret = jhppe_oq_ll_mul_p8_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.next_pointer;
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.next_pointer = value;
	ret = jhppe_oq_ll_mul_p8_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.normal_fwd;
	return ret;
}

sw_error_t
jhppe_oq_ll_mul_p8_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union oq_ll_mul_p8_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_oq_ll_mul_p8_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.normal_fwd = value;
	ret = jhppe_oq_ll_mul_p8_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p8_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P8_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P8_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p8_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p8_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				QUEUE_MANAGER_BASE_ADDR + MUL_P8_DROP_CNT_TBL_ADDRESS + \
				index * MUL_P8_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union mul_p8_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union mul_p8_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mul_p8_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mul_p8_drop_byte_1 << 32 | \
		reg_val.bf.mul_p8_drop_byte_0;
	return ret;
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union mul_p8_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mul_p8_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p8_drop_byte_1 = value >> 32;
	reg_val.bf.mul_p8_drop_byte_0 = value & (((a_uint64_t)1<<32)-1);
	ret = jhppe_mul_p8_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union mul_p8_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mul_p8_drop_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mul_p8_drop_pkt;
	return ret;
}

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union mul_p8_drop_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_mul_p8_drop_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mul_p8_drop_pkt = value;
	ret = jhppe_mul_p8_drop_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

