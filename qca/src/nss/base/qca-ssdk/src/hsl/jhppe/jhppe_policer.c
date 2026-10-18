/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_sawf_flow_statis_ctrl0_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl0_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL0_ADDRESS,
				value->val, sizeof(union sawf_flow_statis_ctrl0_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sawf_flow_statis_ctrl0_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl0_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL0_ADDRESS,
				value->val, sizeof(union sawf_flow_statis_ctrl0_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sawf_flow_statis_ctrl0_sample_time_sawf0_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl0_get(dev_id, &reg_val);
	*value = reg_val.bf.sample_time_sawf0;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl0_sample_time_sawf0_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.sample_time_sawf0 = value;
	ret = jhppe_sawf_flow_statis_ctrl0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl0_sample_time_sawf1_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl0_get(dev_id, &reg_val);
	*value = reg_val.bf.sample_time_sawf1;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl0_sample_time_sawf1_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.sample_time_sawf1 = value;
	ret = jhppe_sawf_flow_statis_ctrl0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl1_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl1_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL1_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl1_current_time_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl1_get(dev_id, &reg_val);
	*value = reg_val.bf.current_time;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl2_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl2_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL2_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl2_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl2_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL2_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl2_sawf0_done_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl2_get(dev_id, &reg_val);
	*value = reg_val.bf.sawf0_done;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl2_sawf0_done_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl2_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.sawf0_done = value;
	ret = jhppe_sawf_flow_statis_ctrl2_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl3_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl3_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL3_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl3_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl3_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL3_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl3_burst_threshold_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl3_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl3_get(dev_id, &reg_val);
	*value = reg_val.bf.burst_threshold;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl3_burst_threshold_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl3_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl3_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.burst_threshold = value;
	ret = jhppe_sawf_flow_statis_ctrl3_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl4_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl4_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL4_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl4_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl4_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL4_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl4_burst_short_intvl_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl4_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl4_get(dev_id, &reg_val);
	*value = reg_val.bf.burst_short_intvl;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl4_burst_short_intvl_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl4_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl4_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.burst_short_intvl = value;
	ret = jhppe_sawf_flow_statis_ctrl4_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl5_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl5_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL5_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl5_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl5_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL5_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl5_burst_long_intvl_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl5_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl5_get(dev_id, &reg_val);
	*value = reg_val.bf.burst_long_intvl;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl5_burst_long_intvl_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl5_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl5_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.burst_long_intvl = value;
	ret = jhppe_sawf_flow_statis_ctrl5_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl6_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL6_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl6_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL6_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_cycle_trigger_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl6_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl6_get(dev_id, &reg_val);
	*value = reg_val.bf.cycle_trigger_en;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_cycle_trigger_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl6_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl6_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.cycle_trigger_en = value;
	ret = jhppe_sawf_flow_statis_ctrl6_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_cycle_trigger_num_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl6_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl6_get(dev_id, &reg_val);
	*value = reg_val.bf.cycle_trigger_num;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl6_cycle_trigger_num_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl6_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl6_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.cycle_trigger_num = value;
	ret = jhppe_sawf_flow_statis_ctrl6_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl7_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL7_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl7_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL7_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_clear_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	*value = reg_val.bf.clear_mode;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_clear_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.clear_mode = value;
	ret = jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_cnt_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	*value = reg_val.bf.cnt_mode;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_cnt_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.cnt_mode = value;
	ret = jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_comp_len_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	*value = reg_val.bf.comp_len;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_comp_len_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.comp_len = value;
	ret = jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_iat_shift_bit_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	*value = reg_val.bf.iat_shift_bit;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_iat_shift_bit_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.iat_shift_bit = value;
	ret = jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_overflow_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	*value = reg_val.bf.overflow_ctrl;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl7_overflow_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl7_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.overflow_ctrl = value;
	ret = jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_state_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_state_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_STATE_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_state_bytes_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_state_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_state_get(dev_id, &reg_val);
	*value = reg_val.bf.bytes_id;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_state_sawf0_active0_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_state_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_state_get(dev_id, &reg_val);
	*value = reg_val.bf.sawf0_active0;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_state_sawf0_active1_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_state_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_state_get(dev_id, &reg_val);
	*value = reg_val.bf.sawf0_active1;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl8_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl8_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL8_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl8_short_interval_time_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl8_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl8_get(dev_id, &reg_val);
	*value = reg_val.bf.short_interval_time;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl9_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl9_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL9_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl9_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl9_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL9_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl9_sawf_done_miss_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl9_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl9_get(dev_id, &reg_val);
	*value = reg_val.bf.sawf_done_miss;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl9_sawf_done_miss_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl9_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl9_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.sawf_done_miss = value;
	ret = jhppe_sawf_flow_statis_ctrl9_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl10_get(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl10_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL10_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl10_set(
		a_uint32_t dev_id,
		union sawf_flow_statis_ctrl10_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_STATIS_CTRL10_ADDRESS,
				value->val);
}

sw_error_t
jhppe_sawf_flow_statis_ctrl10_sawf1_done_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union sawf_flow_statis_ctrl10_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl10_get(dev_id, &reg_val);
	*value = reg_val.bf.sawf1_done;
	return ret;
}

sw_error_t
jhppe_sawf_flow_statis_ctrl10_sawf1_done_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union sawf_flow_statis_ctrl10_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_statis_ctrl10_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.sawf1_done = value;
	ret = jhppe_sawf_flow_statis_ctrl10_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_block_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sawf_flow_block_map_u *value)
{
	if (index >= SAWF_FLOW_BLOCK_MAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_BLOCK_MAP_ADDRESS + \
				index * SAWF_FLOW_BLOCK_MAP_INC,
				&value->val);
}

sw_error_t
jhppe_sawf_flow_block_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sawf_flow_block_map_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_BLOCK_MAP_ADDRESS + \
				index * SAWF_FLOW_BLOCK_MAP_INC,
				value->val);
}

sw_error_t
jhppe_sawf_flow_block_map_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union sawf_flow_block_map_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_block_map_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
jhppe_sawf_flow_block_map_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union sawf_flow_block_map_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_sawf_flow_block_map_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = jhppe_sawf_flow_block_map_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_sawf_flow_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sawf_flow_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_CNT_TBL_ADDRESS + \
				index * SAWF_FLOW_CNT_TBL_INC,
				value->val,
				sizeof(union sawf_flow_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_sawf_flow_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sawf_flow_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + SAWF_FLOW_CNT_TBL_ADDRESS + \
				index * SAWF_FLOW_CNT_TBL_INC,
				value->val,
				sizeof(union sawf_flow_cnt_tbl_u)/sizeof(a_uint32_t));
}

