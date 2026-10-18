/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_SHAPER_API_H_
#define _APPE_SHAPER_API_H_


sw_error_t
appe_shp_cfg_l0_get(
		a_uint32_t dev_id,
		union shp_cfg_l0_u *value);

sw_error_t
appe_shp_cfg_l0_set(
		a_uint32_t dev_id,
		union shp_cfg_l0_u *value);

sw_error_t
appe_shp_cfg_l0_l0_shp_ll_head_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_shp_cfg_l0_l0_shp_ll_head_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_shp_cfg_l0_l0_shp_ll_tail_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_shp_cfg_l0_l0_shp_ll_tail_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
appe_shp_cfg_l1_get(
		a_uint32_t dev_id,
		union shp_cfg_l1_u *value);

sw_error_t
appe_shp_cfg_l1_set(
		a_uint32_t dev_id,
		union shp_cfg_l1_u *value);

sw_error_t
appe_shp_cfg_l1_l1_shp_ll_head_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_shp_cfg_l1_l1_shp_ll_head_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_shp_cfg_l1_l1_shp_ll_tail_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_shp_cfg_l1_l1_shp_ll_tail_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
