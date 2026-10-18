/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/


/**
 * @defgroup fal_sampling FAL_SAMPL
 * @{
 */
#include "sw.h"
#include "sw_ioctl.h"
#include "fal_sampling.h"
#include "fal_uk_if.h"

sw_error_t
fal_sampling_time_set(a_uint32_t dev_id, a_uint32_t index, a_uint32_t value)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_TIME_SET, dev_id, index, value);
	return rv;
}

sw_error_t
fal_sampling_time_get(a_uint32_t dev_id, a_uint32_t index, a_uint32_t *value)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_TIME_GET, dev_id, index, value);
	return rv;
}

sw_error_t
fal_sampling_buff_status_get(a_uint32_t dev_id, a_uint32_t index,
		a_bool_t *done_status)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_BUFF_STATUS_GET, dev_id, index,
			done_status);
	return rv;
}

sw_error_t
fal_sampling_buff_status_clear(a_uint32_t dev_id, a_uint32_t buff_index)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_BUFF_STATUS_CLEAR, dev_id, buff_index);
	return rv;
}

sw_error_t
fal_sampling_done_miss_counter_get(a_uint32_t dev_id, a_uint32_t *done_miss)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_DONE_MISS_CNT_GET, dev_id, done_miss);
	return rv;
}

sw_error_t
fal_sampling_done_miss_counter_clear(a_uint32_t dev_id)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_DONE_MISS_CNT_CLEAR, dev_id);
	return rv;
}

sw_error_t
fal_sampling_burst_cfg_set(a_uint32_t dev_id,
		fal_sampling_burst_cfg_t *burst_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_BURST_CFG_SET, dev_id, burst_cfg);
	return rv;
}

sw_error_t
fal_sampling_burst_cfg_get(a_uint32_t dev_id,
		fal_sampling_burst_cfg_t *burst_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_BURST_CFG_GET, dev_id, burst_cfg);
	return rv;
}

sw_error_t
fal_sampling_ctrl_set(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_CTRL_SET, dev_id, ctrl_cfg);
	return rv;
}

sw_error_t
fal_sampling_ctrl_get(a_uint32_t dev_id, fal_sampling_ctrl_t *ctrl_cfg)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_CTRL_GET, dev_id, ctrl_cfg);
	return rv;
}

sw_error_t
fal_sampling_window_en_set(a_uint32_t dev_id,
		a_uint32_t window_index, a_bool_t enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_WINDOW_EN_SET,
			dev_id, window_index, enable);
	return rv;
}

sw_error_t
fal_sampling_window_en_get(a_uint32_t dev_id,
		a_uint32_t window_index, a_bool_t *enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_WINDOW_EN_GET,
			dev_id, window_index, enable);
	return rv;
}

sw_error_t
fal_sampling_counter_get(a_uint32_t dev_id, a_uint32_t buff_index,
		a_uint32_t counter_id, fal_sampling_counter_entry_t *counter)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_SAMPL_COUNTER_GET,
			dev_id, buff_index, counter_id, counter);
	return rv;
}

