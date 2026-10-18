/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_QM_API_H_
#define _APPE_QM_API_H_

#define PORT_VSI_ENQUEUE_MAP_MAX_ENTRY	PORT_VSI_ENQUEUE_MAP_NUM

sw_error_t
appe_port_vsi_enqueue_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value);

sw_error_t
appe_port_vsi_enqueue_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value);

sw_error_t
appe_port_vsi_enqueue_map_enqueue_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_port_vsi_enqueue_map_enqueue_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_port_vsi_enqueue_map_enqueue_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_port_vsi_enqueue_map_enqueue_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
