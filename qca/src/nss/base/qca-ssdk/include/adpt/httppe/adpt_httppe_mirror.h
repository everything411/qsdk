/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_HTTPPE_MIRROR_
#define _ADPT_HTTPPE_MIRROR_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_httppe_mirr_vp_in_get(a_uint32_t dev_id, fal_port_t port_id,
						 a_bool_t * enable);
sw_error_t
adpt_httppe_mirr_vp_eg_get(a_uint32_t dev_id, fal_port_t port_id,
						 a_bool_t * enable);
sw_error_t
adpt_httppe_mirr_vp_in_set(a_uint32_t dev_id, fal_port_t port_id,
						 a_bool_t enable);
sw_error_t
adpt_httppe_mirr_vp_eg_set(a_uint32_t dev_id, fal_port_t port_id,
						 a_bool_t enable);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
