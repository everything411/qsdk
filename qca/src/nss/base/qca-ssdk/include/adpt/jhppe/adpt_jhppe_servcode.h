/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_SERVCODE_
#define _ADPT_JHPPE_SERVCODE_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_spec_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
		fal_servcode_config_t *entry);

sw_error_t
adpt_jhppe_spec_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
		fal_servcode_config_t *entry);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
