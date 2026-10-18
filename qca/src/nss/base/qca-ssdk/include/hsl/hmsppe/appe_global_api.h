/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_GLOBAL_API_H_
#define _APPE_GLOBAL_API_H_

#define TX_BUFF_THRSH_MAX_ENTRY	TX_BUFF_THRSH_NUM

sw_error_t
appe_tx_buff_thrsh_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_buff_thrsh_u *value);

sw_error_t
appe_tx_buff_thrsh_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tx_buff_thrsh_u *value);

sw_error_t
appe_tx_buff_thrsh_xoff_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_tx_buff_thrsh_xoff_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_tx_buff_thrsh_xon_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_tx_buff_thrsh_xon_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
