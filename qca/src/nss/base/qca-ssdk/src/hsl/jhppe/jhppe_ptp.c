/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
jhppe_xgmac_pps_ctrl_get(
		a_uint32_t dev_id,
		union xgmac_pps_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + XGMAC_PPS_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_xgmac_pps_ctrl_set(
		a_uint32_t dev_id,
		union xgmac_pps_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + XGMAC_PPS_CTRL_ADDRESS,
				value->val);
}
