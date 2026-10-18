/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _JHPPE_PTP_H_
#define _JHPPE_PTP_H_

sw_error_t
jhppe_xgmac_pps_ctrl_get(
		a_uint32_t dev_id,
		union xgmac_pps_ctrl_u *value);

sw_error_t
jhppe_xgmac_pps_ctrl_set(
		a_uint32_t dev_id,
		union xgmac_pps_ctrl_u *value);

#endif
