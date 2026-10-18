/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_tpr_tuple_ip_addr_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_0_u *value)
{
	if (index >= TPR_TUPLE_IP_ADDR_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_0_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_0_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_0_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_0_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_0_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_0_get(dev_id, index, &reg_val);
	*value = reg_val.bf.value;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_0_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_0_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_0_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.value = value;
	ret = jhppe_tpr_tuple_ip_addr_0_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_1_u *value)
{
	if (index >= TPR_TUPLE_IP_ADDR_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_1_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_1_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_1_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_1_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_1_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_1_get(dev_id, index, &reg_val);
	*value = reg_val.bf.value;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_1_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_1_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_1_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.value = value;
	ret = jhppe_tpr_tuple_ip_addr_1_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_2_u *value)
{
	if (index >= TPR_TUPLE_IP_ADDR_2_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_2_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_2_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_2_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_2_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_2_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_2_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_2_get(dev_id, index, &reg_val);
	*value = reg_val.bf.value;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_2_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_2_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_2_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.value = value;
	ret = jhppe_tpr_tuple_ip_addr_2_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_3_u *value)
{
	if (index >= TPR_TUPLE_IP_ADDR_3_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_3_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_3_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_3_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_3_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_3_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_3_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_3_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_3_get(dev_id, index, &reg_val);
	*value = reg_val.bf.value;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_3_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_3_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_3_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.value = value;
	ret = jhppe_tpr_tuple_ip_addr_3_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_ctrl_u *value)
{
	if (index >= TPR_TUPLE_IP_ADDR_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_CTRL_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_CTRL_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_IP_ADDR_CTRL_ADDRESS + \
				index * TPR_TUPLE_IP_ADDR_CTRL_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.type;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.type = value;
	ret = jhppe_tpr_tuple_ip_addr_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_ver_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_ip_addr_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ver;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_ver_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_ip_addr_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_ip_addr_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ver = value;
	ret = jhppe_tpr_tuple_ip_addr_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_l4_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_l4_port_u *value)
{
	if (index >= TPR_TUPLE_L4_PORT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_L4_PORT_ADDRESS + \
				index * TPR_TUPLE_L4_PORT_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_l4_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_l4_port_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_L4_PORT_ADDRESS + \
				index * TPR_TUPLE_L4_PORT_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_l4_port_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_l4_port_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_l4_port_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_type;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_l4_port_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_l4_port_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_l4_port_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_type = value;
	ret = jhppe_tpr_tuple_l4_port_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_l4_port_port_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_l4_port_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_l4_port_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_value;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_l4_port_port_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_l4_port_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_l4_port_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_value = value;
	ret = jhppe_tpr_tuple_l4_port_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_addr_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_addr_match_u *value)
{
	if (index >= TPR_TUPLE_ADDR_MATCH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_ADDR_MATCH_ADDRESS + \
				index * TPR_TUPLE_ADDR_MATCH_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_addr_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_addr_match_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_ADDR_MATCH_ADDRESS + \
				index * TPR_TUPLE_ADDR_MATCH_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_addr_match_addr_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_addr_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_addr_match_get(dev_id, index, &reg_val);
	*value = reg_val.bf.addr_map;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_addr_match_addr_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_addr_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_addr_match_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.addr_map = value;
	ret = jhppe_tpr_tuple_addr_match_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_port_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_port_match_u *value)
{
	if (index >= TPR_TUPLE_PORT_MATCH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_PORT_MATCH_ADDRESS + \
				index * TPR_TUPLE_PORT_MATCH_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_port_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_port_match_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_PORT_MATCH_ADDRESS + \
				index * TPR_TUPLE_PORT_MATCH_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_port_match_port_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_port_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_port_match_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_map;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_port_match_port_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_port_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_port_match_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_map = value;
	ret = jhppe_tpr_tuple_port_match_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_match_ctrl_u *value)
{
	if (index >= TPR_TUPLE_MATCH_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_MATCH_CTRL_ADDRESS + \
				index * TPR_TUPLE_MATCH_CTRL_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_match_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_TUPLE_MATCH_CTRL_ADDRESS + \
				index * TPR_TUPLE_MATCH_CTRL_INC,
				value->val);
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tunnel_type;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tunnel_type = value;
	ret = jhppe_tpr_tuple_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tunnel_type_en;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tunnel_type_en = value;
	ret = jhppe_tpr_tuple_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tuple_id_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tuple_id_en;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_tuple_id_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tuple_id_en = value;
	ret = jhppe_tpr_tuple_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
jhppe_tpr_tuple_match_ctrl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_tuple_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_tuple_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = jhppe_tpr_tuple_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tunnel_type_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tunnel_type_mapping_u *value)
{
	if (index >= TUNNEL_TYPE_MAPPING_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + TUNNEL_TYPE_MAPPING_ADDRESS + \
				index * TUNNEL_TYPE_MAPPING_INC,
				&value->val);
}

sw_error_t
jhppe_tunnel_type_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tunnel_type_mapping_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + TUNNEL_TYPE_MAPPING_ADDRESS + \
				index * TUNNEL_TYPE_MAPPING_INC,
				value->val);
}

sw_error_t
jhppe_tunnel_type_mapping_app_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tunnel_type_mapping_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tunnel_type_mapping_get(dev_id, index, &reg_val);
	*value = reg_val.bf.app_type;
	return ret;
}

sw_error_t
jhppe_tunnel_type_mapping_app_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tunnel_type_mapping_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tunnel_type_mapping_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.app_type = value;
	ret = jhppe_tunnel_type_mapping_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tunnel_type_mapping_outer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tunnel_type_mapping_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tunnel_type_mapping_get(dev_id, index, &reg_val);
	*value = reg_val.bf.outer_en;
	return ret;
}

sw_error_t
jhppe_tunnel_type_mapping_outer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tunnel_type_mapping_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tunnel_type_mapping_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.outer_en = value;
	ret = jhppe_tunnel_type_mapping_set(dev_id, index, &reg_val);
	return ret;
}

