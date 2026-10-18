/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
hmsppe_pon_mode_get(
		a_uint32_t dev_id,
		union hmsppe_pon_mode_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + SWITCH_PON_MODE_ADDRESS,
				&value->val);
}

sw_error_t
hmsppe_pon_mode_set(
		a_uint32_t dev_id,
		union hmsppe_pon_mode_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + SWITCH_PON_MODE_ADDRESS,
				value->val);
}
