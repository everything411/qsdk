/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_ACL_API_H_
#define _HTTPPE_ACL_API_H_

#define IPR_UDF_CTRL_MAX_ENTRY	IPR_UDF_CTRL_NUM

sw_error_t
httppe_ipr_udf_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_ctrl_u *value);

sw_error_t
httppe_ipr_udf_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_ctrl_u *value);

#define IPR_UDF_PROFILE_BASE_MAX_ENTRY	IPR_UDF_PROFILE_BASE_NUM

sw_error_t
httppe_ipr_udf_profile_base_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_base_u *value);

sw_error_t
httppe_ipr_udf_profile_base_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_base_u *value);

#define IPR_UDF_PROFILE_OFFSET_MAX_ENTRY	IPR_UDF_PROFILE_OFFSET_NUM

sw_error_t
httppe_ipr_udf_profile_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_offset_u *value);

sw_error_t
httppe_ipr_udf_profile_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_udf_profile_offset_u *value);

#define IPO_CNT_TBL_MAX_ENTRY	IPO_CNT_TBL_NUM

sw_error_t
httppe_ipo_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_cnt_tbl_u *value);

sw_error_t
httppe_ipo_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_cnt_tbl_u *value);

sw_error_t
httppe_ipo_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_action_u *value);

sw_error_t
httppe_ipo_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipo_action_u *value);

#define L2_DSCP_PBIT_MAP_TBL_MAX_ENTRY	L2_DSCP_PBIT_MAP_TBL_NUM

sw_error_t
httppe_l2_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value);

sw_error_t
httppe_l2_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_dscp_pbit_map_tbl_u *value);

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_l2_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
#endif
