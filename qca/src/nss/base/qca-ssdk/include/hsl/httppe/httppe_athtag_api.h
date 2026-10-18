/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_ATHTAG_API_H_
#define _HTTPPE_ATHTAG_API_H_

#define EG_HDR_XMIT_PRI_MAPPING_MAX_ENTRY	EG_HDR_XMIT_PRI_MAPPING_NUM

sw_error_t
httppe_eg_hdr_xmit_pri_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value);

sw_error_t
httppe_eg_hdr_xmit_pri_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value);

sw_error_t
httppe_eg_gen_ctrl_get(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value);

sw_error_t
httppe_eg_gen_ctrl_set(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value);

#define SERVICE_CODE_MAPPING_MAX_ENTRY  SERVICE_CODE_MAPPING_NUM

sw_error_t
httppe_service_code_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_code_mapping_u *value);

sw_error_t
httppe_service_code_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_code_mapping_u *value);

#define DST_PORT_MAPPING_TBL_MAX_ENTRY  DST_PORT_MAPPING_TBL_NUM

sw_error_t
httppe_dst_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dst_port_mapping_tbl_u *value);

sw_error_t
httppe_dst_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dst_port_mapping_tbl_u *value);
#endif
