/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_tpr_vlan_tpid_ext0_get(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext0_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VLAN_TPID_EXT0_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_tpr_vlan_tpid_ext0_set(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VLAN_TPID_EXT0_ADDRESS,
				value->val);
}

sw_error_t
jhppe_tpr_vlan_tpid_ext0_ctag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid;
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext0_ctag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid = value;
	ret = jhppe_tpr_vlan_tpid_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext0_stag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid;
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext0_stag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid = value;
	ret = jhppe_tpr_vlan_tpid_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_get(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext1_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VLAN_TPID_EXT1_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_set(
		a_uint32_t dev_id,
		union tpr_vlan_tpid_ext1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VLAN_TPID_EXT1_ADDRESS,
				value->val);
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid_map;
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid_map = value;
	ret = jhppe_tpr_vlan_tpid_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid_map;
	return ret;
}

sw_error_t
jhppe_tpr_vlan_tpid_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_vlan_tpid_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid_map = value;
	ret = jhppe_tpr_vlan_tpid_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_EXT0_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_EXT0_ADDRESS,
				value->val);
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_ctag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid;
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_ctag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid = value;
	ret = jhppe_vlan_tpid_reg_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_stag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid;
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext0_stag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid = value;
	ret = jhppe_vlan_tpid_reg_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_get(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_EXT1_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_set(
		a_uint32_t dev_id,
		union vlan_tpid_reg_ext1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + VLAN_TPID_REG_EXT1_ADDRESS,
				value->val);
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid_map;
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid_map = value;
	ret = jhppe_vlan_tpid_reg_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union vlan_tpid_reg_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid_map;
	return ret;
}

sw_error_t
jhppe_vlan_tpid_reg_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union vlan_tpid_reg_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_tpid_reg_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid_map = value;
	ret = jhppe_vlan_tpid_reg_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_vlan_dscp_pbit_map_tbl_u *value)
{
	if (index >= IN_VLAN_DSCP_PBIT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + IN_VLAN_DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * IN_VLAN_DSCP_PBIT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_vlan_dscp_pbit_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + IN_VLAN_DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * IN_VLAN_DSCP_PBIT_MAP_TBL_INC,
				value->val);
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union in_vlan_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_vlan_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_0;
	return ret;
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union in_vlan_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_vlan_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_0 = value;
	ret = jhppe_in_vlan_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union in_vlan_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_vlan_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_1;
	return ret;
}

sw_error_t
jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union in_vlan_dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_vlan_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_1 = value;
	ret = jhppe_in_vlan_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext0_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_EXT0_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext0_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_EXT0_ADDRESS,
				value->val);
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_ctpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.ctpid;
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_ctpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctpid = value;
	ret = jhppe_eg_vlan_tpid_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_stpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext0_get(dev_id, &reg_val);
	*value = reg_val.bf.stpid;
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext0_stpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_ext0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext0_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stpid = value;
	ret = jhppe_eg_vlan_tpid_ext0_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_get(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext1_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_EXT1_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_set(
		a_uint32_t dev_id,
		union eg_vlan_tpid_ext1_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VLAN_TPID_EXT1_ADDRESS,
				value->val);
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_ctag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.ctag_tpid_map;
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_ctag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ctag_tpid_map = value;
	ret = jhppe_eg_vlan_tpid_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_stag_tpid_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext1_get(dev_id, &reg_val);
	*value = reg_val.bf.stag_tpid_map;
	return ret;
}

sw_error_t
jhppe_eg_vlan_tpid_ext1_stag_tpid_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_vlan_tpid_ext1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_vlan_tpid_ext1_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.stag_tpid_map = value;
	ret = jhppe_eg_vlan_tpid_ext1_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_dscp_pbit_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_pbit_map_tbl_u *value)
{
	if (index >= DSCP_PBIT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * DSCP_PBIT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_dscp_pbit_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dscp_pbit_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + DSCP_PBIT_MAP_TBL_ADDRESS + \
				index * DSCP_PBIT_MAP_TBL_INC,
				value->val);
}

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_0;
	return ret;
}

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_0 = value;
	ret = jhppe_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_1;
	return ret;
}

sw_error_t
jhppe_dscp_pbit_map_tbl_pcp_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union dscp_pbit_map_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_dscp_pbit_map_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_1 = value;
	ret = jhppe_dscp_pbit_map_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_ctrl_u *value)
{
	if (index >= VLAN_TO_PORT_MAPPING_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRX_BASE_ADDR + VLAN_TO_PORT_MAPPING_CTRL_ADDRESS + \
				index * VLAN_TO_PORT_MAPPING_CTRL_INC,
				&value->val);
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRX_BASE_ADDR + VLAN_TO_PORT_MAPPING_CTRL_ADDRESS + \
				index * VLAN_TO_PORT_MAPPING_CTRL_INC,
				value->val);
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_private_tag_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.private_tag_en;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_private_tag_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.private_tag_en = value;
	ret = jhppe_vlan_to_port_mapping_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tpid;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_ctrl_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tpid = value;
	ret = jhppe_vlan_to_port_mapping_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				PRX_BASE_ADDR + VLAN_TO_PORT_MAPPING_TBL_ADDRESS + \
				index * VLAN_TO_PORT_MAPPING_TBL_INC,
				value->val,
				sizeof(union vlan_to_port_mapping_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_to_port_mapping_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				PRX_BASE_ADDR + VLAN_TO_PORT_MAPPING_TBL_ADDRESS + \
				index * VLAN_TO_PORT_MAPPING_TBL_INC,
				value->val,
				sizeof(union vlan_to_port_mapping_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_port_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_vp;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_port_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_vp = value;
	ret = jhppe_vlan_to_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_mask_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tci_mask;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_mask_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tci_mask = value;
	ret = jhppe_vlan_to_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tci_value;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_tci_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tci_value = value;
	ret = jhppe_vlan_to_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
jhppe_vlan_to_port_mapping_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vlan_to_port_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = jhppe_vlan_to_port_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

