/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_ATHTAG_API_H_
#define _JHPPE_ATHTAG_API_H_

#define SRC_PORT_MAPPING_TBL_MAX_ENTRY	SRC_PORT_MAPPING_TBL_NUM

sw_error_t
jhppe_src_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union src_port_mapping_tbl_u *value);

sw_error_t
jhppe_src_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union src_port_mapping_tbl_u *value);

sw_error_t
jhppe_src_port_mapping_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_src_port_mapping_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_src_port_mapping_tbl_dst_info_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_src_port_mapping_tbl_dst_info_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_src_port_mapping_tbl_port_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_src_port_mapping_tbl_port_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_src_port_mapping_tbl_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_src_port_mapping_tbl_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
