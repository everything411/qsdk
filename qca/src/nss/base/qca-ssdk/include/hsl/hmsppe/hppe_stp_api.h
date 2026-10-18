/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_STP_API_H_
#define _HPPE_STP_API_H_

#define CST_STATE_MAX_ENTRY	CST_STATE_NUM

sw_error_t
hppe_cst_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union cst_state_u *value);

sw_error_t
hppe_cst_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union cst_state_u *value);

sw_error_t
hppe_cst_state_port_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_cst_state_port_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
