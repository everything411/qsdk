/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/


/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "fal_sampling.h"
#include "adpt.h"


sw_error_t
adpt_jhppe_sampling_time_set(a_uint32_t dev_id,
			a_uint32_t buff_index, a_uint32_t time_value)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (buff_index == FAL_SAMPL_BUFF0) {
		return jhppe_sawf_flow_statis_ctrl0_sample_time_sawf0_set
			(dev_id, time_value);
	} else if (buff_index == FAL_SAMPL_BUFF1) {
		return jhppe_sawf_flow_statis_ctrl0_sample_time_sawf1_set
			(dev_id, time_value);
	} else
		return SW_BAD_PARAM;
}

sw_error_t
adpt_jhppe_sampling_time_get(a_uint32_t dev_id,
			a_uint32_t buff_index, a_uint32_t *time_value)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(time_value);

	if (buff_index == FAL_SAMPL_BUFF0) {
		return jhppe_sawf_flow_statis_ctrl0_sample_time_sawf0_get
			(dev_id, time_value);
	} else if (buff_index == FAL_SAMPL_BUFF1) {
		return jhppe_sawf_flow_statis_ctrl0_sample_time_sawf1_get
			(dev_id, time_value);
	} else
		return SW_BAD_PARAM;
}

sw_error_t
adpt_jhppe_sampling_buff_status_get(a_uint32_t dev_id,
				a_uint32_t buff_index, a_bool_t *done_status)
{
	a_uint32_t reg_value = 0;
	sw_error_t ret = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(done_status);

	if (buff_index == FAL_SAMPL_BUFF0) {
		ret = jhppe_sawf_flow_statis_ctrl2_sawf0_done_get(dev_id,
								&reg_value);
	} else if (buff_index == FAL_SAMPL_BUFF1) {
		ret = jhppe_sawf_flow_statis_ctrl10_sawf1_done_get(dev_id,
								&reg_value);
	} else
		ret = SW_BAD_PARAM;

	*done_status = (reg_value == 1)? A_TRUE : A_FALSE;
	return ret;
}

sw_error_t
adpt_jhppe_sampling_buff_status_clear(a_uint32_t dev_id, a_uint32_t buff_index)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (buff_index == FAL_SAMPL_BUFF0) {
		return jhppe_sawf_flow_statis_ctrl2_sawf0_done_set(dev_id, 0);
	} else if (buff_index == FAL_SAMPL_BUFF1) {
		return jhppe_sawf_flow_statis_ctrl10_sawf1_done_set(dev_id, 0);
	} else
		return SW_BAD_PARAM;
}

sw_error_t
adpt_jhppe_sampling_done_miss_counter_get(a_uint32_t dev_id,
					a_uint32_t *done_miss)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(done_miss);

	return jhppe_sawf_flow_statis_ctrl9_sawf_done_miss_get(dev_id,
								done_miss);
}

sw_error_t
adpt_jhppe_sampling_done_miss_counter_clear(a_uint32_t dev_id)
{
	ADPT_DEV_ID_CHECK(dev_id);

	return jhppe_sawf_flow_statis_ctrl9_sawf_done_miss_set(dev_id, 0);
}

sw_error_t
adpt_jhppe_sampling_burst_cfg_set(a_uint32_t dev_id,
				fal_sampling_burst_cfg_t *burst_cfg)
{
	union sawf_flow_statis_ctrl3_u reg_val1;
	union sawf_flow_statis_ctrl4_u reg_val2;
	union sawf_flow_statis_ctrl5_u reg_val3;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(burst_cfg);

	reg_val1.bf.burst_threshold = burst_cfg->burst_threshold;
	reg_val2.bf.burst_short_intvl = burst_cfg->burst_short_intvl;
	reg_val3.bf.burst_long_intvl = burst_cfg->burst_long_intvl;
	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl3_set(dev_id, &reg_val1));
	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl4_set(dev_id, &reg_val2));
	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl5_set(dev_id, &reg_val3));
	return SW_OK;
}

sw_error_t
adpt_jhppe_sampling_burst_cfg_get(a_uint32_t dev_id,
				fal_sampling_burst_cfg_t *burst_cfg)
{
	union sawf_flow_statis_ctrl3_u reg_val1;
	union sawf_flow_statis_ctrl4_u reg_val2;
	union sawf_flow_statis_ctrl5_u reg_val3;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(burst_cfg);

	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl3_get(dev_id, &reg_val1));
	burst_cfg->burst_threshold = reg_val1.bf.burst_threshold;
	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl4_get(dev_id, &reg_val2));
	burst_cfg->burst_short_intvl = reg_val2.bf.burst_short_intvl;
	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl5_get(dev_id, &reg_val3));
	burst_cfg->burst_long_intvl = reg_val3.bf.burst_long_intvl;
	return SW_OK;
}

sw_error_t
adpt_jhppe_sampling_ctrl_set(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
{
	union sawf_flow_statis_ctrl7_u reg_val;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl_cfg);

	reg_val.bf.clear_mode = ctrl_cfg->counter_clear_mode;
	reg_val.bf.cnt_mode = ctrl_cfg->counter_mode;
	reg_val.bf.overflow_ctrl = ctrl_cfg->counter_overflow_flag;
	reg_val.bf.comp_len = ctrl_cfg->comp_len & 0x1f;
	reg_val.bf.iat_shift_bit = ctrl_cfg->iat_shift_bit & 0x1f;

	return jhppe_sawf_flow_statis_ctrl7_set(dev_id, &reg_val);
}

sw_error_t
adpt_jhppe_sampling_ctrl_get(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
{
	union sawf_flow_statis_ctrl7_u reg_val;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl_cfg);

	SW_RTN_ON_ERROR(jhppe_sawf_flow_statis_ctrl7_get(dev_id, &reg_val));

	ctrl_cfg->counter_clear_mode = reg_val.bf.clear_mode;
	ctrl_cfg->counter_mode =
			(fal_sampling_counter_mode_t)reg_val.bf.cnt_mode;
	ctrl_cfg->counter_overflow_flag = reg_val.bf.overflow_ctrl;
	ctrl_cfg->comp_len = reg_val.bf.comp_len;
	ctrl_cfg->iat_shift_bit = reg_val.bf.iat_shift_bit;

	return SW_OK;
}

sw_error_t
adpt_jhppe_sampling_window_en_set(a_uint32_t dev_id,
				a_uint32_t window_index, a_bool_t enable)
{
	ADPT_DEV_ID_CHECK(dev_id);
	if (window_index >= SAWF_FLOW_BLOCK_MAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	return jhppe_sawf_flow_block_map_valid_set(dev_id,
						window_index, enable);
}

sw_error_t
adpt_jhppe_sampling_window_en_get(a_uint32_t dev_id,
				a_uint32_t window_index, a_bool_t *enable)
{
	a_uint32_t reg_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (window_index >= SAWF_FLOW_BLOCK_MAP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	SW_RTN_ON_ERROR(
		jhppe_sawf_flow_block_map_valid_get(dev_id,
						window_index, &reg_value));
	*enable = (reg_value == 1)? A_TRUE : A_FALSE;
	return SW_OK;
}

sw_error_t
adpt_jhppe_sampling_counter_get(a_uint32_t dev_id, a_uint32_t buff_index,
		a_uint32_t counter_id, fal_sampling_counter_entry_t *counter)
{
	union sawf_flow_cnt_tbl_u reg_val;
	a_uint32_t index = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(counter);

	if ((buff_index != FAL_SAMPL_BUFF0) &&
		(buff_index != FAL_SAMPL_BUFF1))
		return SW_BAD_PARAM;

	if (counter_id >= (SAWF_FLOW_CNT_TBL_MAX_ENTRY/2))
		return SW_OUT_OF_RANGE;

	index = (buff_index << 8) | (counter_id & 0xff);

	SW_RTN_ON_ERROR(jhppe_sawf_flow_cnt_tbl_get(dev_id, index, &reg_val));
	counter->matched_pkts = reg_val.bf.packets;
	counter->matched_bytes = ((a_uint64_t)reg_val.bf.bytes_1 << \
		SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BYTES_OFFSET))| \
						reg_val.bf.bytes_0;
	counter->bytes_min = ((a_uint16_t)reg_val.bf.bytes_min_1 << \
		SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BYTES_MIN_OFFSET)) | \
					reg_val.bf.bytes_min_0;
	counter->bytes_max = (a_uint16_t)reg_val.bf.bytes_max;
	counter->packet_iat_min = (a_uint16_t)reg_val.bf.packet_iat_min;
	counter->packet_iat_max = (a_uint16_t)reg_val.bf.packet_iat_max;
	counter->packet_iat_sum =
		((a_uint16_t)reg_val.bf.packet_iat_sum_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_PACKET_IAT_SUM_OFFSET)) | \
				reg_val.bf.packet_iat_sum_0;
	counter->burst_oc_bytes = reg_val.bf.burst_oc_bytes;
	counter->burst_is_bytes = ((a_uint32_t)reg_val.bf.burst_is_bytes_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_IS_BYTES_OFFSET)) | \
					reg_val.bf.burst_is_bytes_0;
	counter->burst_bytes[0] = ((a_uint32_t)reg_val.bf.burst_bytes0_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_BYTES0_OFFSET)) | \
					reg_val.bf.burst_bytes0_0;
	counter->burst_bytes[1] = reg_val.bf.burst_bytes1;
	counter->burst_bytes[2] = reg_val.bf.burst_bytes2;
	counter->burst_bytes[3] = ((a_uint32_t)reg_val.bf.burst_bytes3_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_BYTES3_OFFSET)) | \
					reg_val.bf.burst_bytes3_0;
	counter->burst_bytes[4] = ((a_uint32_t)reg_val.bf.burst_bytes4_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_BYTES4_OFFSET)) | \
					reg_val.bf.burst_bytes4_0;
	counter->burst_bytes[5] = reg_val.bf.burst_bytes5;
	counter->burst_bytes[6] = reg_val.bf.burst_bytes6;
	counter->burst_bytes[7] = ((a_uint32_t)reg_val.bf.burst_bytes7_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_BYTES7_OFFSET)) | \
					reg_val.bf.burst_bytes7_0;
	counter->burst_bytes[8] = ((a_uint32_t)reg_val.bf.burst_bytes8_1 << \
	SW_FIELD_OFFSET_IN_WORD(SAWF_FLOW_CNT_TBL_BURST_BYTES8_OFFSET)) | \
					reg_val.bf.burst_bytes8_0;
	counter->burst_bytes[9] = reg_val.bf.burst_bytes9;
	counter->burst_timestamp = reg_val.bf.burst_timestamp;
	counter->flow_timestamp = reg_val.bf.flow_timestamp;
	counter->burst_flags = reg_val.bf.burst_flags;

	return SW_OK;
}

sw_error_t adpt_jhppe_sampling_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_sampling_time_set = adpt_jhppe_sampling_time_set;
	p_adpt_api->adpt_sampling_time_get = adpt_jhppe_sampling_time_get;
	p_adpt_api->adpt_sampling_buff_status_get =
				adpt_jhppe_sampling_buff_status_get;
	p_adpt_api->adpt_sampling_buff_status_clear =
				adpt_jhppe_sampling_buff_status_clear;
	p_adpt_api->adpt_sampling_done_miss_counter_get =
				adpt_jhppe_sampling_done_miss_counter_get;
	p_adpt_api->adpt_sampling_done_miss_counter_clear =
				adpt_jhppe_sampling_done_miss_counter_clear;
	p_adpt_api->adpt_sampling_burst_cfg_set =
				adpt_jhppe_sampling_burst_cfg_set;
	p_adpt_api->adpt_sampling_burst_cfg_get =
				adpt_jhppe_sampling_burst_cfg_get;
	p_adpt_api->adpt_sampling_ctrl_set = adpt_jhppe_sampling_ctrl_set;
	p_adpt_api->adpt_sampling_ctrl_get = adpt_jhppe_sampling_ctrl_get;
	p_adpt_api->adpt_sampling_window_en_set =
				adpt_jhppe_sampling_window_en_set;
	p_adpt_api->adpt_sampling_window_en_get =
				adpt_jhppe_sampling_window_en_get;
	p_adpt_api->adpt_sampling_counter_get = adpt_jhppe_sampling_counter_get;

	return SW_OK;
}

/**
 * @}
 */
