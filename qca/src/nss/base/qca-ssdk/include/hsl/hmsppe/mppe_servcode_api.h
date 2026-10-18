/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MPPE_SERVCODE_API_H_
#define _MPPE_SERVCODE_API_H_

#define TL_VP_SERVICE_CODE_GEN_MAX_ENTRY	TL_VP_SERVICE_CODE_GEN_NUM

sw_error_t
mppe_tl_vp_service_code_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_vp_service_code_gen_u *value);

sw_error_t
mppe_tl_vp_service_code_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_vp_service_code_gen_u *value);

sw_error_t
mppe_tl_vp_service_code_gen_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_tl_vp_service_code_gen_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
mppe_tl_vp_service_code_gen_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
mppe_tl_vp_service_code_gen_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
