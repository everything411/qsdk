/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_port_parsing_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_parsing_reg_u *value)
{
	if (index >= PORT_PARSING_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + PORT_PARSING_REG_ADDRESS + \
				index * PORT_PARSING_REG_INC,
				&value->val);
}

sw_error_t
hppe_port_parsing_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_parsing_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + PORT_PARSING_REG_ADDRESS + \
				index * PORT_PARSING_REG_INC,
				value->val);
}

sw_error_t
hppe_edma_vlan_tpid_reg_get(
		a_uint32_t dev_id,
		union edma_vlan_tpid_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				EDMA_BASE_ADDR + EDMA_VLAN_TPID_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_edma_vlan_tpid_reg_set(
		a_uint32_t dev_id,
		union edma_vlan_tpid_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				EDMA_BASE_ADDR + EDMA_VLAN_TPID_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_vlan_tpid_reg_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_vlan_tpid_reg_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_bridge_config_get(
		a_uint32_t dev_id,
		union bridge_config_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + BRIDGE_CONFIG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_bridge_config_set(
		a_uint32_t dev_id,
		union bridge_config_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + BRIDGE_CONFIG_ADDRESS,
				value->val);
}

sw_error_t
hppe_port_vlan_config_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vlan_config_u *value)
{
	if (index >= PORT_VLAN_CONFIG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_VLAN_CONFIG_ADDRESS + \
				index * PORT_VLAN_CONFIG_INC,
				&value->val);
}

sw_error_t
hppe_port_vlan_config_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vlan_config_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_VLAN_CONFIG_ADDRESS + \
				index * PORT_VLAN_CONFIG_INC,
				value->val);
}

sw_error_t
hppe_xlt_rule_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_rule_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + XLT_RULE_TBL_ADDRESS + \
				index * XLT_RULE_TBL_INC,
				value->val,
				sizeof(union xlt_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_xlt_rule_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_rule_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + XLT_RULE_TBL_ADDRESS + \
				index * XLT_RULE_TBL_INC,
				value->val,
				sizeof(union xlt_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_xlt_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_action_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + XLT_ACTION_TBL_ADDRESS + \
				index * XLT_ACTION_TBL_INC,
				value->val,
				sizeof(union xlt_action_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_xlt_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union xlt_action_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + XLT_ACTION_TBL_ADDRESS + \
				index * XLT_ACTION_TBL_INC,
				value->val,
				sizeof(union xlt_action_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_bridge_config_bridge_type_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.bridge_type;
	return ret;
}

sw_error_t
hppe_bridge_config_bridge_type_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bridge_type = value;
	ret = hppe_bridge_config_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_vlan_xlt_miss_fwd_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_vlan_xlt_miss_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_vlan_xlt_miss_fwd_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_vlan_xlt_rule_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_rule_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_XLT_RULE_ADDRESS + \
				index * EG_VLAN_XLT_RULE_INC,
				value->val,
				sizeof(union eg_vlan_xlt_rule_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vlan_xlt_rule_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_rule_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_XLT_RULE_ADDRESS + \
				index * EG_VLAN_XLT_RULE_INC,
				value->val,
				sizeof(union eg_vlan_xlt_rule_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vsi_tag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_tag_u *value)
{
	if (index >= EG_VSI_TAG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_TAG_ADDRESS + \
				index * EG_VSI_TAG_INC,
				&value->val);
}

sw_error_t
hppe_eg_vsi_tag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_tag_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_TAG_ADDRESS + \
				index * EG_VSI_TAG_INC,
				value->val);
}

sw_error_t
hppe_port_eg_vlan_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_vlan_u *value)
{
	if (index >= PORT_EG_VLAN_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_EG_VLAN_ADDRESS + \
				index * PORT_EG_VLAN_INC,
				&value->val);
}

sw_error_t
hppe_port_eg_vlan_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_vlan_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_EG_VLAN_ADDRESS + \
				index * PORT_EG_VLAN_INC,
				value->val);
}

sw_error_t
hppe_eg_vlan_tpid_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_ADDRESS,
				&value->val);
}

sw_error_t
hppe_eg_vlan_tpid_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_ADDRESS,
				value->val);
}

sw_error_t
hppe_eg_bridge_config_get(
		a_uint32_t dev_id,
		union eg_bridge_config_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_BRIDGE_CONFIG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_eg_bridge_config_set(
		a_uint32_t dev_id,
		union eg_bridge_config_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_BRIDGE_CONFIG_ADDRESS,
				value->val);
}

sw_error_t
hppe_eg_vlan_xlt_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_action_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_XLT_ACTION_ADDRESS + \
				index * EG_VLAN_XLT_ACTION_INC,
				value->val,
				sizeof(union eg_vlan_xlt_action_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vlan_xlt_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vlan_xlt_action_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_XLT_ACTION_ADDRESS + \
				index * EG_VLAN_XLT_ACTION_INC,
				value->val,
				sizeof(union eg_vlan_xlt_action_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_vsi_tag_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vsi_tag_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tagged_mode_port_bitmap;
	return ret;
}

sw_error_t
hppe_eg_vsi_tag_tagged_mode_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_vsi_tag_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vsi_tag_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tagged_mode_port_bitmap = value;
	ret = hppe_eg_vsi_tag_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_ctag_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_eg_vlan_ctag_mode;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_ctag_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_eg_vlan_ctag_mode = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_vsi_tag_mode_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vsi_tag_mode_en;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_vsi_tag_mode_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vsi_tag_mode_en = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_stag_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_eg_vlan_stag_mode;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_vlan_stag_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_eg_vlan_stag_mode = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_vlan_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_vlan_type;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_vlan_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_vlan_type = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_vlan_tpid_ctpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vlan_tpid_get(dev_id, &reg_val);
	*value = reg_val.bf.ctpid;
	return ret;
}

sw_error_t
hppe_eg_vlan_tpid_ctpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vlan_tpid_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctpid = value;
	ret = hppe_eg_vlan_tpid_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_vlan_tpid_stpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vlan_tpid_get(dev_id, &reg_val);
	*value = reg_val.bf.stpid;
	return ret;
}

sw_error_t
hppe_eg_vlan_tpid_stpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_vlan_tpid_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stpid = value;
	ret = hppe_eg_vlan_tpid_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_bridge_config_bridge_type_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.bridge_type;
	return ret;
}

sw_error_t
hppe_eg_bridge_config_bridge_type_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bridge_type = value;
	ret = hppe_eg_bridge_config_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_bridge_config_pkt_l2_edit_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.pkt_l2_edit_en;
	return ret;
}

sw_error_t
hppe_eg_bridge_config_pkt_l2_edit_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pkt_l2_edit_en = value;
	ret = hppe_eg_bridge_config_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_xlt_rule_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union xlt_rule_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_xlt_rule_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = hppe_xlt_rule_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_xlt_rule_tbl_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union xlt_rule_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_xlt_rule_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_bitmap = value;
	ret = hppe_xlt_rule_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_vid_u *value)
{
	if (index >= PORT_DEF_VID_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_DEF_VID_ADDRESS + \
				index * PORT_DEF_VID_INC,
				&value->val);
}

sw_error_t
hppe_port_def_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_pcp_u *value)
{
	if (index >= PORT_DEF_PCP_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_DEF_PCP_ADDRESS + \
				index * PORT_DEF_PCP_INC,
				&value->val);
}

sw_error_t
hppe_port_def_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_pcp_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_DEF_PCP_ADDRESS + \
				index * PORT_DEF_PCP_INC,
				value->val);
}

sw_error_t
hppe_port_def_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_def_vid_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + PORT_DEF_VID_ADDRESS + \
				index * PORT_DEF_VID_INC,
				value->val);
}

sw_error_t
hppe_port_def_vid_port_def_cvid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cvid_en;
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_cvid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cvid_en = value;
	ret = hppe_port_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_svid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_svid_en;
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_svid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_svid_en = value;
	ret = hppe_port_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cvid;
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cvid = value;
	ret = hppe_port_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_svid;
	return ret;
}

sw_error_t
hppe_port_def_vid_port_def_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_svid = value;
	ret = hppe_port_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_sdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_sdei;
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_sdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_sdei = value;
	ret = hppe_port_def_pcp_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_spcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_spcp;
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_spcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_spcp = value;
	ret = hppe_port_def_pcp_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_cdei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cdei;
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_cdei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cdei = value;
	ret = hppe_port_def_pcp_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_cpcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cpcp;
	return ret;
}

sw_error_t
hppe_port_def_pcp_port_def_cpcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_def_pcp_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_def_pcp_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cpcp = value;
	ret = hppe_port_def_pcp_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_def_vid_u *value)
{
	if (index >= PORT_EG_DEF_VID_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_EG_DEF_VID_ADDRESS + \
				index * PORT_EG_DEF_VID_INC,
				&value->val);
}

sw_error_t
hppe_port_eg_def_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_eg_def_vid_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_EG_DEF_VID_ADDRESS + \
				index * PORT_EG_DEF_VID_INC,
				value->val);
}

sw_error_t
hppe_port_eg_def_vid_port_def_svid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_svid_en;
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_svid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_svid_en = value;
	ret = hppe_port_eg_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_svid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_svid;
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_svid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_svid = value;
	ret = hppe_port_eg_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cvid_en;
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cvid_en = value;
	ret = hppe_port_eg_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_def_cvid;
	return ret;
}

sw_error_t
hppe_port_eg_def_vid_port_def_cvid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_def_vid_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_def_vid_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_def_cvid = value;
	ret = hppe_port_eg_def_vid_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_untag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_untag_fltr_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_untag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_untag_fltr_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_vlan_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_in_vlan_fltr_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_vlan_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_in_vlan_fltr_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_pri_tag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_pri_tag_fltr_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_pri_tag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_pri_tag_fltr_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_tag_fltr_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_tag_fltr_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_tag_fltr_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_tag_fltr_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_parsing_reg_port_role_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_role;
	return ret;
}

sw_error_t
hppe_port_parsing_reg_port_role_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_role = value;
	ret = hppe_port_parsing_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_vlan_tpid_reg_stag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vlan_tpid_reg_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid;
	return ret;
}

sw_error_t
hppe_vlan_tpid_reg_stag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vlan_tpid_reg_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid = value;
	ret = hppe_vlan_tpid_reg_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_vlan_tpid_reg_ctag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vlan_tpid_reg_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid;
	return ret;
}

sw_error_t
hppe_vlan_tpid_reg_ctag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_vlan_tpid_reg_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid = value;
	ret = hppe_vlan_tpid_reg_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_dei_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_in_dei_prop_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_dei_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_in_dei_prop_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_pcp_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_in_pcp_prop_cmd;
	return ret;
}

sw_error_t
hppe_port_vlan_config_port_in_pcp_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_vlan_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_vlan_config_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_in_pcp_prop_cmd = value;
	ret = hppe_port_vlan_config_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_xlt_rule_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union xlt_rule_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_xlt_rule_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
hppe_xlt_rule_tbl_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union xlt_rule_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_xlt_rule_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_bitmap;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_tx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_counting_en;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_tx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_counting_en = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_pcp_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_eg_pcp_prop_cmd;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_pcp_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_eg_pcp_prop_cmd = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_dei_prop_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_eg_dei_prop_cmd;
	return ret;
}

sw_error_t
hppe_port_eg_vlan_port_eg_dei_prop_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_eg_vlan_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_eg_vlan_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_eg_dei_prop_cmd = value;
	ret = hppe_port_eg_vlan_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_bridge_config_queue_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.queue_cnt_en;
	return ret;
}

sw_error_t
hppe_eg_bridge_config_queue_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.queue_cnt_en = value;
	ret = hppe_eg_bridge_config_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_vlan_dev_tx_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_tx_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VLAN_DEV_TX_COUNTER_TBL_ADDRESS + \
				index * VLAN_DEV_TX_COUNTER_TBL_INC,
				value->val,
				sizeof(union vlan_dev_tx_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vlan_dev_tx_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_tx_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VLAN_DEV_TX_COUNTER_TBL_ADDRESS + \
				index * VLAN_DEV_TX_COUNTER_TBL_INC,
				value->val,
				sizeof(union vlan_dev_tx_counter_tbl_u)/sizeof(a_uint32_t));
}

#if defined(JHPPE)
sw_error_t
hppe_port_parsing_reg_src_port_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.src_port_sel;
	return ret;
}

sw_error_t
hppe_port_parsing_reg_src_port_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.src_port_sel = value;
	ret = hppe_port_parsing_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_port_parsing_reg_vlan_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vlan_mode;
	return ret;
}

sw_error_t
hppe_port_parsing_reg_vlan_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union port_parsing_reg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_port_parsing_reg_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vlan_mode = value;
	ret = hppe_port_parsing_reg_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code0_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.passthrough_cpu_code0;
	return ret;
}

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code0_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.passthrough_cpu_code0 = value;
	ret = hppe_eg_bridge_config_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code1_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	*value = reg_val.bf.passthrough_cpu_code1;
	return ret;
}

sw_error_t
hppe_eg_bridge_config_passthrough_cpu_code1_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_bridge_config_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_bridge_config_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.passthrough_cpu_code1 = value;
	ret = hppe_eg_bridge_config_set(dev_id, &reg_val);
	return ret;
}

#endif
