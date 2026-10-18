/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_POLICER_API_H_
#define _APPE_POLICER_API_H_


sw_error_t
appe_in_meter_head_reg_get(
		a_uint32_t dev_id,
		union in_meter_head_reg_u *value);

sw_error_t
appe_in_meter_head_reg_set(
		a_uint32_t dev_id,
		union in_meter_head_reg_u *value);

sw_error_t
appe_in_meter_head_reg_meter_ll_head_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_in_meter_head_reg_meter_ll_head_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_in_meter_head_reg_meter_ll_tail_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_in_meter_head_reg_meter_ll_tail_set(
		a_uint32_t dev_id,
		unsigned int value);

#define DSCP_REMAP_TBL_MAX_ENTRY	DSCP_REMAP_TBL_NUM

sw_error_t
appe_dscp_remap_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_remap_tbl_u *value);

sw_error_t
appe_dscp_remap_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_remap_tbl_u *value);

sw_error_t
appe_dscp_remap_tbl_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_dscp_remap_tbl_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_dscp_remap_tbl_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_dscp_remap_tbl_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_dscp_remap_tbl_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_dscp_remap_tbl_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
