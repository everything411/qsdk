/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_PORTVLAN_API_H_
#define _JHPPE_PORTVLAN_API_H_


sw_error_t
jhppe_tpr_vlan_tpid_ext0_get(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext0_u *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext0_set(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext0_u *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext0_ctag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext0_ctag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_tpr_vlan_tpid_ext0_stag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext0_stag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_tpr_vlan_tpid_ext1_get(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext1_u *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext1_set(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext1_u *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_tpr_vlan_tpid_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_vlan_tpid_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_vlan_tpid_reg_ext0_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext0_u *value);

sw_error_t
jhppe_vlan_tpid_reg_ext0_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext0_u *value);

sw_error_t
jhppe_vlan_tpid_reg_ext0_ctag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_vlan_tpid_reg_ext0_ctag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_vlan_tpid_reg_ext0_stag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_vlan_tpid_reg_ext0_stag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_vlan_tpid_reg_ext1_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext1_u *value);

sw_error_t
jhppe_vlan_tpid_reg_ext1_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext1_u *value);

sw_error_t
jhppe_vlan_tpid_reg_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_vlan_tpid_reg_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_vlan_tpid_reg_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_vlan_tpid_reg_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);

#define IN_VLAN_DSCP_PBIT_MAP_TBL_MAX_ENTRY	IN_VLAN_DSCP_PBIT_MAP_TBL_NUM

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_vlan_dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_vlan_dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
jhppe_eg_vlan_tpid_ext0_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext0_u *value);

sw_error_t
jhppe_eg_vlan_tpid_ext0_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext0_u *value);

sw_error_t
jhppe_eg_vlan_tpid_ext0_ctpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_eg_vlan_tpid_ext0_ctpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_eg_vlan_tpid_ext0_stpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_eg_vlan_tpid_ext0_stpid_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_eg_vlan_tpid_ext1_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext1_u *value);

sw_error_t
jhppe_eg_vlan_tpid_ext1_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext1_u *value);

sw_error_t
jhppe_eg_vlan_tpid_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_eg_vlan_tpid_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_eg_vlan_tpid_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_eg_vlan_tpid_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		unsigned int value);

#define DSCP_PBIT_MAP_TBL_MAX_ENTRY	DSCP_PBIT_MAP_TBL_NUM

sw_error_t
jhppe_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_pbit_map_tbl_u *value);

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VLAN_TO_PORT_MAPPING_CTRL_MAX_ENTRY	VLAN_TO_PORT_MAPPING_CTRL_NUM

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_ctrl_u *value);

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_ctrl_u *value);

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_private_tag_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_private_tag_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VLAN_TO_PORT_MAPPING_TBL_MAX_ENTRY	VLAN_TO_PORT_MAPPING_TBL_NUM

sw_error_t
jhppe_vlan_to_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_tbl_u *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_tbl_u *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_mask_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_mask_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_vlan_to_port_mapping_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
