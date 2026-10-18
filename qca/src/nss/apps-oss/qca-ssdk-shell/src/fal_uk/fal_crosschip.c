/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw_ioctl.h"
#include "fal_crosschip.h"
#include "fal_uk_if.h"


sw_error_t
fal_crosschip_bp_mode_set(a_uint32_t dev_id, fal_crosschip_bp_mode_t mode)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_CROSSCHIP_BP_MODE_SET, dev_id, mode);
	return rv;
}

sw_error_t
fal_crosschip_bp_mode_get(a_uint32_t dev_id, fal_crosschip_bp_mode_t *mode)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_CROSSCHIP_BP_MODE_GET, dev_id, mode);
	return rv;
}

sw_error_t
fal_crosschip_bp_en_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_CROSSCHIP_BP_EN_SET, dev_id, queue_id, enable);
	return rv;
}

sw_error_t
fal_crosschip_bp_en_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_CROSSCHIP_BP_EN_GET, dev_id, queue_id, enable);
	return rv;
}
