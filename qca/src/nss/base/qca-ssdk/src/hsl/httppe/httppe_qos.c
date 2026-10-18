/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_l0_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l0_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l0_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_MAP_TBL_ADDRESS + \
				index * L0_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l0_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l0_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value)
{
	if (index >= L0_FLOW_PORT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L0_FLOW_PORT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
httppe_l0_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_flow_port_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L0_FLOW_PORT_MAP_TBL_INC,
				value->val);
}

sw_error_t
httppe_l1_flow_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l1_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l1_flow_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_MAP_TBL_ADDRESS + \
				index * L1_FLOW_MAP_TBL_INC,
				value->val,
				sizeof(union l1_flow_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_l1_flow_port_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value)
{
	if (index >= L1_FLOW_PORT_MAP_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L1_FLOW_PORT_MAP_TBL_INC,
				&value->val);
}

sw_error_t
httppe_l1_flow_port_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_flow_port_map_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_FLOW_PORT_MAP_TBL_ADDRESS + \
				index * L1_FLOW_PORT_MAP_TBL_INC,
				value->val);
}

sw_error_t
httppe_l0_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value)
{
	if (index >= L0_COMP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_COMP_CFG_TBL_ADDRESS + \
				index * L0_COMP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
httppe_l0_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_COMP_CFG_TBL_ADDRESS + \
				index * L0_COMP_CFG_TBL_INC,
				value->val);
}

sw_error_t
httppe_l1_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value)
{
	if (index >= L1_COMP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_COMP_CFG_TBL_ADDRESS + \
				index * L1_COMP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
httppe_l1_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_COMP_CFG_TBL_ADDRESS + \
				index * L1_COMP_CFG_TBL_INC,
				value->val);
}

sw_error_t
httppe_tdm_depth_cfg_get(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + TDM_DEPTH_CFG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_tdm_depth_cfg_set(
		a_uint32_t dev_id,
		union tdm_depth_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + TDM_DEPTH_CFG_ADDRESS,
				value->val);
}

sw_error_t
httppe_psch_tdm_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value)
{
	if (index >= PSCH_TDM_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_TDM_CFG_TBL_ADDRESS + \
				index * PSCH_TDM_CFG_TBL_INC,
				&value->val);
}

sw_error_t
httppe_psch_tdm_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_tdm_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_TDM_CFG_TBL_ADDRESS + \
				index * PSCH_TDM_CFG_TBL_INC,
				value->val);
}

sw_error_t
httppe_qos_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value)
{
	if (index >= QOS_MAPPING_TBL_NUM)
		return SW_OUT_OF_RANGE;
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + QOS_MAPPING_TBL_ADDRESS + \
				index * QOS_MAPPING_TBL_INC,
				value->val,
				2);
}

sw_error_t
httppe_qos_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + QOS_MAPPING_TBL_ADDRESS + \
				index * QOS_MAPPING_TBL_INC,
				value->val,
				2);
}

sw_error_t
httppe_qos_mapping_tbl_flow_policer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *policer_en,
		a_uint32_t *policer_index)
{
	union qos_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_qos_mapping_tbl_get(dev_id, index, &reg_val);
	*policer_en = reg_val.bf.flow_policy_valid;
	*policer_index = reg_val.bf.flow_policy_id;
	return ret;
}

sw_error_t
httppe_qos_mapping_tbl_flow_policer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t policer_en,
		a_uint32_t policer_index)
{
	union qos_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_qos_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.flow_policy_valid = policer_en;
	reg_val.bf.flow_policy_id = policer_index;
	ret = httppe_qos_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}

