/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HTTPPE_SERVCODE_H_
#define _ADPT_HTTPPE_SERVCODE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t adpt_httppe_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
					   fal_servcode_config_t *entry);
sw_error_t adpt_httppe_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
					   fal_servcode_config_t *entry);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
/**
 * @}
 */
