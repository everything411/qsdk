/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_app_udp_port_cfg_get(
		a_uint32_t dev_id,
		union app_udp_port_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + APP_UDP_PORT_CFG_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_app_udp_port_cfg_set(
		a_uint32_t dev_id,
		union app_udp_port_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + APP_UDP_PORT_CFG_ADDRESS,
				value->val);
}

sw_error_t
jhppe_nat_t_cfg_get(
		a_uint32_t dev_id,
		union nat_t_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + NAT_T_CFG_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_nat_t_cfg_set(
		a_uint32_t dev_id,
		union nat_t_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + NAT_T_CFG_ADDRESS,
				value->val);
}

sw_error_t
jhppe_nat_t_cfg_app_udp_port_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union nat_t_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_nat_t_cfg_get(dev_id, &reg_val);
	*value = reg_val.bf.app_udp_port_map;
	return ret;
}

sw_error_t
jhppe_nat_t_cfg_app_udp_port_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union nat_t_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_nat_t_cfg_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.app_udp_port_map = value;
	ret = jhppe_nat_t_cfg_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_l3_flow_key_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_gen_u *value)
{
	if (index >= L3_FLOW_KEY_GEN_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_KEY_GEN_ADDRESS + \
				index * L3_FLOW_KEY_GEN_INC,
				&value->val);
}

sw_error_t
jhppe_l3_flow_key_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_gen_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_KEY_GEN_ADDRESS + \
				index * L3_FLOW_KEY_GEN_INC,
				value->val);
}

sw_error_t
jhppe_l3_flow_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_sel_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_KEY_SEL_ADDRESS + \
				index * L3_FLOW_KEY_SEL_INC,
				value->val,
				sizeof(union l3_flow_key_sel_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l3_flow_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_sel_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_KEY_SEL_ADDRESS + \
				index * L3_FLOW_KEY_SEL_INC,
				value->val,
				sizeof(union l3_flow_key_sel_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l3_my_mac_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_my_mac_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_MY_MAC_TBL_ADDRESS + \
				index * L3_MY_MAC_TBL_INC,
				value->val,
				sizeof(union l3_my_mac_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l3_my_mac_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_my_mac_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_MY_MAC_TBL_ADDRESS + \
				index * L3_MY_MAC_TBL_INC,
				value->val,
				sizeof(union l3_my_mac_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l3_my_mac_tbl_l3_if_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.l3_if_index;
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_l3_if_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.l3_if_index = value;
	ret = jhppe_l3_my_mac_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_mac_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mac_1 << 32 | \
		reg_val.bf.mac_0;
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_mac_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_1 = value >> 32;
	reg_val.bf.mac_0 = value & (((a_uint64_t)1<<32)-1);
	ret = jhppe_l3_my_mac_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = jhppe_l3_my_mac_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vsi;
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vsi = value;
	ret = jhppe_l3_my_mac_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_vsi_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vsi_valid;
	return ret;
}

sw_error_t
jhppe_l3_my_mac_tbl_vsi_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l3_my_mac_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l3_my_mac_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vsi_valid = value;
	ret = jhppe_l3_my_mac_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_in_flow_6tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_6tuple_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_6TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_6TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_6tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_in_flow_6tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_6tuple_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_6TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_6TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_6tuple_tbl_u)/sizeof(a_uint32_t));
}

