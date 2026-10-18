/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_CTRLPKT_API_H_
#define _JHPPE_CTRLPKT_API_H_

#define EG_CPU_CODE_TBL_MAX_ENTRY	EG_CPU_CODE_TBL_NUM

sw_error_t
jhppe_eg_cpu_code_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_cpu_code_tbl_u *value);

sw_error_t
jhppe_eg_cpu_code_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_cpu_code_tbl_u *value);

sw_error_t
jhppe_eg_cpu_code_tbl_cpu_code_disable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_cpu_code_tbl_cpu_code_disable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_cpu_code_tbl_edit_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_cpu_code_tbl_edit_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
