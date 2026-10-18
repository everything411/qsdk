/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_PTP_
#define _ADPT_JHPPE_PTP_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t adpt_jhppe_port_pps_ctrl_set(a_uint32_t dev_id,
					fal_port_pps_ctrl_t *pps_ctrl);
sw_error_t adpt_jhppe_port_pps_ctrl_get(a_uint32_t dev_id,
					fal_port_pps_ctrl_t *pps_ctrl);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
