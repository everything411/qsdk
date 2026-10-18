/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_L2_VP_API_H_
#define _JHPPE_L2_VP_API_H_

#define EG_SRC_PORT_REMAP_MAX_ENTRY	EG_SRC_PORT_REMAP_NUM

sw_error_t
jhppe_eg_src_port_remap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_src_port_remap_u *value);

sw_error_t
jhppe_eg_src_port_remap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_src_port_remap_u *value);

sw_error_t
jhppe_eg_src_port_remap_src_port_num_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_src_port_remap_src_port_num_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_ISOL_ACTION_MAX_ENTRY	PORT_ISOL_ACTION_NUM

sw_error_t
jhppe_port_isol_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_isol_action_u *value);

sw_error_t
jhppe_port_isol_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_isol_action_u *value);

sw_error_t
jhppe_port_isol_action_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_port_isol_action_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L2_VP_PORT_POST_TBL_MAX_ENTRY	L2_VP_PORT_POST_TBL_NUM

sw_error_t
jhppe_l2_vp_port_post_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_post_tbl_u *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_post_tbl_u *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_ctag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_ctag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_stag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_stag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_eg_vlan_fmt_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_phy_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_phy_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_enq_service_code_queue_dis_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_extra_header_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_extra_header_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_isol_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_mirror_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_mirror_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_mtu_check_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_mtu_check_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_physical_port_mtu_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_context_active_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_context_active_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_eg_data_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_eg_data_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_state_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_state_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_vp_port_post_tbl_vp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VP_ISOL_ACTION_TBL_MAX_ENTRY	VP_ISOL_ACTION_TBL_NUM

sw_error_t
jhppe_vp_isol_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_isol_action_tbl_u *value);

sw_error_t
jhppe_vp_isol_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_isol_action_tbl_u *value);

sw_error_t
jhppe_vp_isol_action_tbl_vp_profile_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_vp_isol_action_tbl_vp_profile_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

#endif
