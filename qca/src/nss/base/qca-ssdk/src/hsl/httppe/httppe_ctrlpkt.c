/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_app_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + APP_CTRL_ADDRESS + \
				index * APP_CTRL_INC,
				value->val,
				sizeof(union app_ctrl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_app_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + APP_CTRL_ADDRESS + \
				index * APP_CTRL_INC,
				value->val,
				sizeof(union app_ctrl_u)/sizeof(a_uint32_t));
}

