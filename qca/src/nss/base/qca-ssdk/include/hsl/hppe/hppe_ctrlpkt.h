/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _HPPE_CTRLPKT_H_
#define _HPPE_CTRLPKT_H_

#define ETHERTYPE_CTRL_MAX_ENTRY	4
#define RFDB_TBL_MAX_ENTRY	32
#define APP_CTRL_MAX_ENTRY	32


sw_error_t
hppe_ethertype_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value);

sw_error_t
hppe_ethertype_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value);

sw_error_t
hppe_app_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value);

sw_error_t
hppe_app_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value);

sw_error_t
hppe_ethertype_ctrl_ethertype_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ethertype_ctrl_ethertype_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ethertype_ctrl_ethertype_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif

