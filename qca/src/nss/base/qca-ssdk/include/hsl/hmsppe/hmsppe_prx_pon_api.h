/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HMSPPE_PRX_PON_API_H_
#define _HMSPPE_PRX_PON_API_H_


sw_error_t
hmsppe_pass_through_ctrl_get(
		a_uint32_t dev_id,
		union pass_through_ctrl_u *value);

sw_error_t
hmsppe_pass_through_ctrl_set(
		a_uint32_t dev_id,
		union pass_through_ctrl_u *value);

sw_error_t
hmsppe_pass_through_ctrl_copy_cpu_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_pass_through_ctrl_copy_cpu_en_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hmsppe_dot1p_mapper_default_get(
		a_uint32_t dev_id,
		union dot1p_mapper_default_u *value);

sw_error_t
hmsppe_dot1p_mapper_default_set(
		a_uint32_t dev_id,
		union dot1p_mapper_default_u *value);

sw_error_t
hmsppe_dot1p_mapper_default_dei_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_dei_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hmsppe_dot1p_mapper_default_dscp_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_dscp_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hmsppe_dot1p_mapper_default_dscp_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_dscp_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hmsppe_dot1p_mapper_default_miss_action_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_miss_action_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hmsppe_dot1p_mapper_default_pcp_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_pcp_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hmsppe_dot1p_mapper_default_vid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hmsppe_dot1p_mapper_default_vid_set(
		a_uint32_t dev_id,
		unsigned int value);

#define DOT1P_MAPPER_RULE_MAX_ENTRY	DOT1P_MAPPER_RULE_NUM

sw_error_t
hmsppe_dot1p_mapper_rule_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_rule_u *value);

sw_error_t
hmsppe_dot1p_mapper_rule_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_rule_u *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_dest_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dest_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_dest_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dest_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_dscp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_gem_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_gem_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_priority_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_priority_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_src_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_src_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_src_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_src_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_rule_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_rule_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DOT1P_MAPPER_ACTION_MAX_ENTRY	DOT1P_MAPPER_ACTION_NUM

sw_error_t
hmsppe_dot1p_mapper_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_action_u *value);

sw_error_t
hmsppe_dot1p_mapper_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dot1p_mapper_action_u *value);

sw_error_t
hmsppe_dot1p_mapper_action_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_phy_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_phy_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_override_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_override_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_eg_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_enqueue_vp_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_gem_port_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_gem_port_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_int_dp_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_int_pri_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_ds_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_ds_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_policer_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_dot1p_mapper_action_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_dot1p_mapper_action_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define GEM_PORT_MAPPING_CTRL_MAX_ENTRY	GEM_PORT_MAPPING_CTRL_NUM

sw_error_t
hmsppe_gem_port_mapping_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_ctrl_u *value);

sw_error_t
hmsppe_gem_port_mapping_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_ctrl_u *value);

sw_error_t
hmsppe_gem_port_mapping_ctrl_mapping_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_ctrl_mapping_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define GEM_PORT_MAPPING_TBL_MAX_ENTRY	GEM_PORT_MAPPING_TBL_NUM

sw_error_t
hmsppe_gem_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_tbl_u *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union gem_port_mapping_tbl_u *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_dst_info_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_dp_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_int_pri_dp_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_port_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_port_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hmsppe_gem_port_mapping_tbl_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hmsppe_gem_port_mapping_tbl_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
