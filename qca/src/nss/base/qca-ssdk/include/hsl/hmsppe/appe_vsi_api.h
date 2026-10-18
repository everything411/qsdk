/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_VSI_API_H_
#define _APPE_VSI_API_H_

#define VSI_REMAP_TBL_MAX_ENTRY	VSI_REMAP_TBL_NUM

sw_error_t
appe_vsi_remap_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_remap_tbl_u *value);

sw_error_t
appe_vsi_remap_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_remap_tbl_u *value);

sw_error_t
appe_vsi_remap_tbl_br_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_br_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_4_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_4_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_5_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_5_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_6_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_6_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_7_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_member_port_bitmap_7_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_vsi_remap_tbl_vsi_remap_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_vsi_remap_tbl_vsi_remap_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
