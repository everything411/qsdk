/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_POLICER_API_H_
#define _HPPE_POLICER_API_H_

#define METER_CMPST_LENGTH_REG_MAX_ENTRY	METER_CMPST_LENGTH_REG_NUM

sw_error_t
hppe_meter_cmpst_length_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union meter_cmpst_length_reg_u *value);

sw_error_t
hppe_meter_cmpst_length_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union meter_cmpst_length_reg_u *value);

sw_error_t
hppe_meter_cmpst_length_reg_cmpst_length_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_meter_cmpst_length_reg_cmpst_length_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_pc_drop_bypass_reg_get(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value);

sw_error_t
hppe_pc_drop_bypass_reg_set(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value);

sw_error_t
hppe_pc_drop_bypass_reg_drop_bypass_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_pc_drop_bypass_reg_drop_bypass_en_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_time_slot_reg_get(
		a_uint32_t dev_id,
		union time_slot_reg_u *value);

sw_error_t
hppe_time_slot_reg_set(
		a_uint32_t dev_id,
		union time_slot_reg_u *value);

sw_error_t
hppe_time_slot_reg_time_slot_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_time_slot_reg_time_slot_set(
		a_uint32_t dev_id,
		unsigned int value);

#define IN_ACL_METER_CFG_TBL_MAX_ENTRY	IN_ACL_METER_CFG_TBL_NUM

sw_error_t
hppe_in_acl_meter_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cfg_tbl_u *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cfg_tbl_u *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cbs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cbs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_cir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_color_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_color_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_coupling_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_coupling_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_ebs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_ebs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_eir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_eir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_eir_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_eir_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dscp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_dscp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_pcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_pcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_pri_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_chg_pri_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_remap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_exceed_remap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_grp_cf_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_grp_cf_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_grp_end_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_grp_end_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_length_mod_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_length_mod_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_meter_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_nxt_ptr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_nxt_ptr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_token_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_token_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dscp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_dscp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_pcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_pcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_pri_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_chg_pri_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dscp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_dscp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_remap_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cfg_tbl_violate_remap_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_ACL_METER_CRDT_TBL_MAX_ENTRY	IN_ACL_METER_CRDT_TBL_NUM

sw_error_t
hppe_in_acl_meter_crdt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_crdt_tbl_u *value);

sw_error_t
hppe_in_acl_meter_crdt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_crdt_tbl_u *value);

sw_error_t
hppe_in_acl_meter_crdt_tbl_c_crdt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_crdt_tbl_c_crdt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_acl_meter_crdt_tbl_e_crdt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_crdt_tbl_e_crdt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_PORT_METER_CFG_TBL_MAX_ENTRY	IN_PORT_METER_CFG_TBL_NUM

sw_error_t
hppe_in_port_meter_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cfg_tbl_u *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cfg_tbl_u *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_cbs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_cbs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_cir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_cir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_color_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_color_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_coupling_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_coupling_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_ebs_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_ebs_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_eir_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_eir_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_dei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_dei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_dp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_dp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_pcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_pcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_pri_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_chg_pri_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_exceed_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_meter_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_token_unit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_token_unit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_dei_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_dei_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_dp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_dp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_pcp_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_pcp_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_pri_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_chg_pri_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_dp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_dp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cfg_tbl_violate_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_PORT_METER_CRDT_TBL_MAX_ENTRY	IN_PORT_METER_CRDT_TBL_NUM

sw_error_t
hppe_in_port_meter_crdt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_crdt_tbl_u *value);

sw_error_t
hppe_in_port_meter_crdt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_crdt_tbl_u *value);

sw_error_t
hppe_in_port_meter_crdt_tbl_c_crdt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_crdt_tbl_c_crdt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_in_port_meter_crdt_tbl_e_crdt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_crdt_tbl_e_crdt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_PORT_METER_CNT_TBL_MAX_ENTRY	IN_PORT_METER_CNT_TBL_NUM

sw_error_t
hppe_in_port_meter_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cnt_tbl_u *value);

sw_error_t
hppe_in_port_meter_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cnt_tbl_u *value);

sw_error_t
hppe_in_port_meter_cnt_tbl_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_in_port_meter_cnt_tbl_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_in_port_meter_cnt_tbl_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_port_meter_cnt_tbl_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_ACL_METER_CNT_TBL_MAX_ENTRY	IN_ACL_METER_CNT_TBL_NUM

sw_error_t
hppe_in_acl_meter_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cnt_tbl_u *value);

sw_error_t
hppe_in_acl_meter_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cnt_tbl_u *value);

sw_error_t
hppe_in_acl_meter_cnt_tbl_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_in_acl_meter_cnt_tbl_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_in_acl_meter_cnt_tbl_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_acl_meter_cnt_tbl_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PC_GLOBAL_CNT_TBL_MAX_ENTRY	PC_GLOBAL_CNT_TBL_NUM

sw_error_t
hppe_pc_global_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pc_global_cnt_tbl_u *value);

sw_error_t
hppe_pc_global_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pc_global_cnt_tbl_u *value);

sw_error_t
hppe_pc_global_cnt_tbl_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_pc_global_cnt_tbl_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_pc_global_cnt_tbl_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_pc_global_cnt_tbl_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DROP_CPU_CNT_TBL_MAX_ENTRY	DROP_CPU_CNT_TBL_NUM

sw_error_t
hppe_drop_cpu_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cpu_cnt_tbl_u *value);

sw_error_t
hppe_drop_cpu_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cpu_cnt_tbl_u *value);

sw_error_t
hppe_drop_cpu_cnt_tbl_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_drop_cpu_cnt_tbl_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_drop_cpu_cnt_tbl_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_drop_cpu_cnt_tbl_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_TX_DROP_CNT_TBL_MAX_ENTRY	PORT_TX_DROP_CNT_TBL_NUM

sw_error_t
hppe_port_tx_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_drop_cnt_tbl_u *value);

sw_error_t
hppe_port_tx_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_drop_cnt_tbl_u *value);

sw_error_t
hppe_port_tx_drop_cnt_tbl_tx_drop_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_port_tx_drop_cnt_tbl_tx_drop_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_port_tx_drop_cnt_tbl_tx_drop_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_tx_drop_cnt_tbl_tx_drop_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VP_TX_DROP_CNT_TBL_MAX_ENTRY	VP_TX_DROP_CNT_TBL_NUM

sw_error_t
hppe_vp_tx_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_drop_cnt_tbl_u *value);

sw_error_t
hppe_vp_tx_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_drop_cnt_tbl_u *value);

sw_error_t
hppe_vp_tx_drop_cnt_tbl_tx_drop_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_vp_tx_drop_cnt_tbl_tx_drop_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_vp_tx_drop_cnt_tbl_tx_drop_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_vp_tx_drop_cnt_tbl_tx_drop_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VLAN_DEV_CNT_TBL_MAX_ENTRY	VLAN_DEV_CNT_TBL_NUM

sw_error_t
hppe_vlan_dev_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_cnt_tbl_u *value);

sw_error_t
hppe_vlan_dev_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_cnt_tbl_u *value);

sw_error_t
hppe_vlan_dev_cnt_tbl_rx_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_vlan_dev_cnt_tbl_rx_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_vlan_dev_cnt_tbl_rx_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_vlan_dev_cnt_tbl_rx_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
