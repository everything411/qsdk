/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_MPPE_SERVCODE_H_
#define _ADPT_MPPE_SERVCODE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_mppe_port_servcode_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t servcode_index);

sw_error_t
adpt_mppe_port_servcode_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *servcode_index);

sw_error_t
adpt_mppe_servcode_athtag_set(a_uint32_t dev_id,
		a_uint32_t servcode_index, fal_servcode_athtag_t *entry);

sw_error_t
adpt_mppe_servcode_athtag_get(a_uint32_t dev_id,
		a_uint32_t servcode_index, fal_servcode_athtag_t *entry);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
