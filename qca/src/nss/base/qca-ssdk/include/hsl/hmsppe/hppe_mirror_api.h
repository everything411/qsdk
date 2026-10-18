/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_MIRROR_API_H_
#define _HPPE_MIRROR_API_H_


sw_error_t
hppe_mirror_analyzer_get(
		a_uint32_t dev_id,
		union mirror_analyzer_u *value);

sw_error_t
hppe_mirror_analyzer_set(
		a_uint32_t dev_id,
		union mirror_analyzer_u *value);

sw_error_t
hppe_mirror_analyzer_eg_analyzer_port_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_mirror_analyzer_eg_analyzer_port_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_mirror_analyzer_in_analyzer_port_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_mirror_analyzer_in_analyzer_port_set(
		a_uint32_t dev_id,
		unsigned int value);

#define PORT_MIRROR_MAX_ENTRY	PORT_MIRROR_NUM

sw_error_t
hppe_port_mirror_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_mirror_u *value);

sw_error_t
hppe_port_mirror_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_mirror_u *value);

sw_error_t
hppe_port_mirror_eg_mirr_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_mirror_eg_mirr_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_mirror_in_mirr_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_mirror_in_mirr_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
