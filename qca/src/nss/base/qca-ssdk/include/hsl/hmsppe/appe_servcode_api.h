/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_SERVCODE_API_H_
#define _APPE_SERVCODE_API_H_

#define TL_SERVICE_TBL_MAX_ENTRY	TL_SERVICE_TBL_NUM

sw_error_t
appe_tl_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_service_tbl_u *value);

sw_error_t
appe_tl_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_service_tbl_u *value);

sw_error_t
appe_tl_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_tl_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
