/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_PORTVLAN_API_H_
#define _HPPE_PORTVLAN_API_H_

#define PORT_PARSING_REG_MAX_ENTRY	PORT_PARSING_REG_NUM

sw_error_t
hppe_port_parsing_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_parsing_reg_u *value);

sw_error_t
hppe_port_parsing_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_parsing_reg_u *value);

sw_error_t
hppe_port_parsing_reg_port_role_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_parsing_reg_port_role_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_parsing_reg_src_port_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_parsing_reg_src_port_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_parsing_reg_vlan_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_parsing_reg_vlan_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_vlan_tpid_reg_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_u *value);

sw_error_t
hppe_vlan_tpid_reg_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_u *value);

sw_error_t
hppe_vlan_tpid_reg_ctag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_vlan_tpid_reg_ctag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_vlan_tpid_reg_stag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_vlan_tpid_reg_stag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_bridge_config_get(
		a_uint32_t dev_id,
		union bridge_config_u *value);

sw_error_t
hppe_bridge_config_set(
		a_uint32_t dev_id,
		union bridge_config_u *value);

sw_error_t
hppe_bridge_config_bridge_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_bridge_config_bridge_type_set(
		a_uint32_t dev_id,
		unsigned int value);

#define PORT_DEF_VID_MAX_ENTRY	PORT_DEF_VID_NUM

sw_error_t
hppe_port_def_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_vid_u *value);

sw_error_t
hppe_port_def_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_vid_u *value);

sw_error_t
hppe_port_def_vid_port_def_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_vid_port_def_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_vid_port_def_cvid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_vid_port_def_cvid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_vid_port_def_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_vid_port_def_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_vid_port_def_svid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_vid_port_def_svid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_DEF_PCP_MAX_ENTRY	PORT_DEF_PCP_NUM

sw_error_t
hppe_port_def_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_pcp_u *value);

sw_error_t
hppe_port_def_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_pcp_u *value);

sw_error_t
hppe_port_def_pcp_port_def_cdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_pcp_port_def_cdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_pcp_port_def_cpcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_pcp_port_def_cpcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_pcp_port_def_sdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_pcp_port_def_sdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_def_pcp_port_def_spcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_def_pcp_port_def_spcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_VLAN_CONFIG_MAX_ENTRY	PORT_VLAN_CONFIG_NUM

sw_error_t
hppe_port_vlan_config_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vlan_config_u *value);

sw_error_t
hppe_port_vlan_config_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vlan_config_u *value);

sw_error_t
hppe_port_vlan_config_port_in_dei_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_in_dei_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_in_pcp_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_in_pcp_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_in_vlan_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_in_vlan_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_pri_tag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_pri_tag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_tag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_tag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_untag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_untag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define XLT_RULE_TBL_MAX_ENTRY	XLT_RULE_TBL_NUM

sw_error_t
hppe_xlt_rule_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_rule_tbl_u *value);

sw_error_t
hppe_xlt_rule_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_rule_tbl_u *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ckey_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ckey_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ctpid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ctpid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_ctpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_ctpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_dhcp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_dhcp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_frm_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_frm_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_frm_type_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_frm_type_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_gem_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_gem_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_mc_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_mc_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_port_vp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_port_vp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_prot_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_prot_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_prot_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_prot_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_skey_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_skey_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_stpid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_stpid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_stpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_stpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_vni_resv_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_rule_tbl_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_rule_tbl_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define XLT_ACTION_TBL_MAX_ENTRY	XLT_ACTION_TBL_NUM

sw_error_t
hppe_xlt_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_action_tbl_u *value);

sw_error_t
hppe_xlt_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_action_tbl_u *value);

sw_error_t
hppe_xlt_action_tbl_counter_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_counter_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_counter_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_counter_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_counter_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_counter_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_ctpid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_ctpid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_ctpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_ctpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_dei_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_dei_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_dest_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_dest_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_dest_info_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_dest_info_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_dscp_pbit_mapping_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_dscp_pbit_mapping_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_pcp_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_pcp_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_service_code_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_service_code_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_src_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_src_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_src_info_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_src_info_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_src_info_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_src_info_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_stpid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_stpid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_stpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_stpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_tags_to_remove_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_tags_to_remove_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_vid_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_vid_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_vsi_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_vsi_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cdei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cdei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cpcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cpcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cpcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cpcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_cvid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_cvid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_sdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_sdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_sdei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_sdei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_spcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_spcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_spcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_spcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_xlt_action_tbl_xlt_svid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_xlt_action_tbl_xlt_svid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_EG_DEF_VID_MAX_ENTRY	PORT_EG_DEF_VID_NUM

sw_error_t
hppe_port_eg_def_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_def_vid_u *value);

sw_error_t
hppe_port_eg_def_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_def_vid_u *value);

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_def_vid_port_def_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_def_vid_port_def_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_def_vid_port_def_svid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_def_vid_port_def_svid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_EG_VLAN_MAX_ENTRY	PORT_EG_VLAN_NUM

sw_error_t
hppe_port_eg_vlan_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_vlan_u *value);

sw_error_t
hppe_port_eg_vlan_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_vlan_u *value);

sw_error_t
hppe_port_eg_vlan_port_eg_dei_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_port_eg_dei_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_port_eg_pcp_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_port_eg_pcp_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_ctag_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_ctag_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_stag_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_stag_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_port_vlan_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_port_vlan_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_tx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_tx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_eg_vlan_vsi_tag_mode_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_eg_vlan_vsi_tag_mode_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_eg_vlan_tpid_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_u *value);

sw_error_t
hppe_eg_vlan_tpid_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_u *value);

sw_error_t
hppe_eg_vlan_tpid_ctpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_vlan_tpid_ctpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_vlan_tpid_stpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_vlan_tpid_stpid_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_eg_bridge_config_get(
		a_uint32_t dev_id,
		union eg_bridge_config_u *value);

sw_error_t
hppe_eg_bridge_config_set(
		a_uint32_t dev_id,
		union eg_bridge_config_u *value);

sw_error_t
hppe_eg_bridge_config_bridge_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_bridge_type_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_field_update_enable_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_field_update_enable_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_pkt_l2_edit_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_pkt_l2_edit_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_hppe_eip_rsv_w4_3130_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_hppe_eip_rsv_w4_3130_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_bridge_config_queue_cnt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_bridge_config_queue_cnt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

#define EG_VSI_TAG_MAX_ENTRY	EG_VSI_TAG_NUM

sw_error_t
hppe_eg_vsi_tag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_tag_u *value);

sw_error_t
hppe_eg_vsi_tag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_tag_u *value);

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_VLAN_XLT_ACTION_MAX_ENTRY	EG_VLAN_XLT_ACTION_NUM

sw_error_t
hppe_eg_vlan_xlt_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_action_u *value);

sw_error_t
hppe_eg_vlan_xlt_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_action_u *value);

sw_error_t
hppe_eg_vlan_xlt_action_counter_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_counter_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_counter_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_counter_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_ctpid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_ctpid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_ctpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_ctpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_dei_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_dei_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_dscp_pbit_mapping_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_dscp_pbit_mapping_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_pcp_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_pcp_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_stpid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_stpid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_stpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_stpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_tags_to_remove_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_tags_to_remove_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_vid_swap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_vid_swap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_vni_resv_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_vni_resv_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_vni_resv_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_vni_resv_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cdei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cdei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cpcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cpcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cpcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cpcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cvid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_cvid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_sdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_sdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_sdei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_sdei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_spcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_spcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_spcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_spcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_svid_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_action_xlt_svid_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_VLAN_XLT_RULE_MAX_ENTRY	EG_VLAN_XLT_RULE_NUM

sw_error_t
hppe_eg_vlan_xlt_rule_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_rule_u *value);

sw_error_t
hppe_eg_vlan_xlt_rule_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_rule_u *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ckey_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ctpid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ctpid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_ctpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_ctpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_dhcp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_dhcp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_mc_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_mc_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_prot_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_prot_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_prot_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_prot_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_dei_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_dei_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_skey_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_stpid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_stpid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_stpid_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_stpid_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_vlan_xlt_rule_vsi_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VLAN_DEV_TX_COUNTER_TBL_MAX_ENTRY	VLAN_DEV_TX_COUNTER_TBL_NUM

sw_error_t
hppe_vlan_dev_tx_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_tx_counter_tbl_u *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_tx_counter_tbl_u *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_drop_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_drop_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_drop_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_drop_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_vlan_dev_tx_counter_tbl_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_edma_vlan_tpid_reg_get(
		a_uint32_t dev_id,
		union edma_vlan_tpid_reg_u *value);

sw_error_t
hppe_edma_vlan_tpid_reg_set(
		a_uint32_t dev_id,
		union edma_vlan_tpid_reg_u *value);

sw_error_t
hppe_edma_vlan_tpid_reg_cvlan_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_edma_vlan_tpid_reg_cvlan_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_edma_vlan_tpid_reg_svlan_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_edma_vlan_tpid_reg_svlan_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
