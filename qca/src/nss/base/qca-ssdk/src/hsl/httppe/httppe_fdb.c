/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_l2_global_conf_get(
		a_uint32_t dev_id,
		union l2_global_conf_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_GLOBAL_CONF_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l2_global_conf_set(
		a_uint32_t dev_id,
		union l2_global_conf_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_GLOBAL_CONF_ADDRESS,
				value->val);
}

