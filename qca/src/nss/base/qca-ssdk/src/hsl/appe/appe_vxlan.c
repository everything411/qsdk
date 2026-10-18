/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
appe_udp_port_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union udp_port_cfg_u *value)
{
	if (index >= UDP_PORT_CFG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + UDP_PORT_CFG_ADDRESS + \
				index * UDP_PORT_CFG_INC,
				&value->val);
}

sw_error_t
appe_udp_port_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union udp_port_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + UDP_PORT_CFG_ADDRESS + \
				index * UDP_PORT_CFG_INC,
				value->val);
}

sw_error_t
appe_tpr_vxlan_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_CFG_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_vxlan_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_CFG_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_vxlan_gpe_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_GPE_CFG_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_vxlan_gpe_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_GPE_CFG_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_prot_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_GPE_PROT_CFG_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_prot_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_VXLAN_GPE_PROT_CFG_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vxlan_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_vxlan_cfg_get(dev_id, &reg_val);
	*value = reg_val.bf.udp_port_map;
	return ret;
}

sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vxlan_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_vxlan_cfg_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.udp_port_map = value;
	ret = appe_tpr_vxlan_cfg_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_vxlan_gpe_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_vxlan_gpe_cfg_get(dev_id, &reg_val);
	*value = reg_val.bf.udp_port_map;
	return ret;
}

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_vxlan_gpe_cfg_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_vxlan_gpe_cfg_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.udp_port_map = value;
	ret = appe_tpr_vxlan_gpe_cfg_set(dev_id, &reg_val);
	return ret;
}

