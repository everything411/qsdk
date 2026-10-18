/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_PPPOE_API_H_
#define _APPE_PPPOE_API_H_

#define PPPOE_SESSION_MAX_ENTRY	PPPOE_SESSION_NUM

sw_error_t
appe_pppoe_session_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_u *value);

sw_error_t
appe_pppoe_session_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_u *value);

sw_error_t
appe_pppoe_session_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_port_vp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_port_vp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_session_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_session_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PPPOE_SESSION_EXT_MAX_ENTRY	PPPOE_SESSION_EXT_NUM

sw_error_t
appe_pppoe_session_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext_u *value);

sw_error_t
appe_pppoe_session_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext_u *value);

sw_error_t
appe_pppoe_session_ext_mc_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext_mc_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext_smac_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext_smac_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext_smac_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext_smac_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext_uc_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext_uc_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PPPOE_SESSION_EXT1_MAX_ENTRY	PPPOE_SESSION_EXT1_NUM

sw_error_t
appe_pppoe_session_ext1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext1_u *value);

sw_error_t
appe_pppoe_session_ext1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext1_u *value);

sw_error_t
appe_pppoe_session_ext1_smac_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext1_smac_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PPPOE_SESSION_EXT2_MAX_ENTRY	PPPOE_SESSION_EXT2_NUM

sw_error_t
appe_pppoe_session_ext2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext2_u *value);

sw_error_t
appe_pppoe_session_ext2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pppoe_session_ext2_u *value);

sw_error_t
appe_pppoe_session_ext2_l3_if_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext2_l3_if_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext2_l3_if_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext2_l3_if_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext2_tl_l3_if_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext2_tl_l3_if_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_pppoe_session_ext2_tl_l3_if_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_pppoe_session_ext2_tl_l3_if_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
