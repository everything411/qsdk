/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/


/**
 * @defgroup fal_sampling FAL_SAMPL
 * @{
 */
#include "sw.h"
#include "fal_sampling.h"
#include "adpt.h"

sw_error_t fal_sampling_time_set(a_uint32_t dev_id, a_uint32_t buff_index, a_uint32_t time_value)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_time_set, dev_id, buff_index, time_value)

sw_error_t fal_sampling_time_get(a_uint32_t dev_id, a_uint32_t buff_index, a_uint32_t *time_value)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_time_get, dev_id, buff_index, time_value)

sw_error_t fal_sampling_buff_status_get(a_uint32_t dev_id, a_uint32_t buff_index, a_bool_t *done_status)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_buff_status_get, dev_id, buff_index, done_status)

sw_error_t fal_sampling_buff_status_clear(a_uint32_t dev_id, a_uint32_t buff_index)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_buff_status_clear, dev_id, buff_index)

sw_error_t fal_sampling_done_miss_counter_get(a_uint32_t dev_id, a_uint32_t *done_miss)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_done_miss_counter_get, dev_id, done_miss)

sw_error_t fal_sampling_done_miss_counter_clear(a_uint32_t dev_id)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_done_miss_counter_clear, dev_id)

sw_error_t fal_sampling_burst_cfg_set(a_uint32_t dev_id, fal_sampling_burst_cfg_t *burst_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_burst_cfg_set, dev_id, burst_cfg)

sw_error_t fal_sampling_burst_cfg_get(a_uint32_t dev_id, fal_sampling_burst_cfg_t *burst_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_burst_cfg_get, dev_id, burst_cfg)

sw_error_t fal_sampling_ctrl_set(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_ctrl_set, dev_id, ctrl_cfg)

sw_error_t fal_sampling_ctrl_get(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_ctrl_get, dev_id, ctrl_cfg)

sw_error_t fal_sampling_window_en_set(a_uint32_t dev_id, a_uint32_t window_index, a_bool_t enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_window_en_set, dev_id, window_index, enable)

sw_error_t fal_sampling_window_en_get(a_uint32_t dev_id, a_uint32_t window_index, a_bool_t *enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_window_en_get, dev_id, window_index, enable)

sw_error_t fal_sampling_counter_get(a_uint32_t dev_id, a_uint32_t buff_index, a_uint32_t counter_id, fal_sampling_counter_entry_t *counter)
	DEFINE_FAL_FUNC_ADPT_EXPORT(sampling_counter_get, dev_id, buff_index, counter_id, counter)


