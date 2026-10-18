/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "fal_ip.h"
#include "adpt.h"

sw_error_t
adpt_jhppe_ip_port_vlan_as_flow_key_get(a_uint32_t dev_id, fal_port_t port_id,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
{
	union l3_vp_port_tbl_u l3_vp_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vlan_key);

	aos_mem_zero(&l3_vp_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	vlan_key->valid = l3_vp_tbl.bf.vlan_as_flow_key;
	vlan_key->mode = l3_vp_tbl.bf.vlan_as_flow_key_mode;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_port_vlan_as_flow_key_set(a_uint32_t dev_id, fal_port_t port_id,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
{
	union l3_vp_port_tbl_u l3_vp_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vlan_key);

	aos_mem_zero(&l3_vp_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	l3_vp_tbl.bf.vlan_as_flow_key = vlan_key->valid;
	l3_vp_tbl.bf.vlan_as_flow_key_mode = vlan_key->mode;

	return hppe_l3_vp_port_tbl_set(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_tbl);
}

sw_error_t
adpt_jhppe_ip_intf_vlan_as_flow_key_get(a_uint32_t dev_id, a_uint32_t intf,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
{
	union in_l3_if_tbl_u in_l3_if_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vlan_key);

	aos_mem_zero(&in_l3_if_tbl, sizeof(union in_l3_if_tbl_u));

	rv = hppe_in_l3_if_tbl_get(dev_id, intf, &in_l3_if_tbl);
	SW_RTN_ON_ERROR(rv);

	vlan_key->valid = in_l3_if_tbl.bf.vlan_as_flow_key_l3if;
	vlan_key->mode = in_l3_if_tbl.bf.vlan_as_flow_key_mode;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_intf_vlan_as_flow_key_set(a_uint32_t dev_id, a_uint32_t intf,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
{
	union in_l3_if_tbl_u in_l3_if_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vlan_key);

	aos_mem_zero(&in_l3_if_tbl, sizeof(union in_l3_if_tbl_u));

	rv = hppe_in_l3_if_tbl_get(dev_id, intf, &in_l3_if_tbl);
	SW_RTN_ON_ERROR(rv);

	in_l3_if_tbl.bf.vlan_as_flow_key_l3if = vlan_key->valid;
	in_l3_if_tbl.bf.vlan_as_flow_key_mode = vlan_key->mode;

	vlan_key->valid = in_l3_if_tbl.bf.vlan_as_flow_key_l3if;
	vlan_key->mode = in_l3_if_tbl.bf.vlan_as_flow_key_mode;

	return hppe_in_l3_if_tbl_set(dev_id, intf, &in_l3_if_tbl);
}

sw_error_t
adpt_jhppe_ip_vsi_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi,
		fal_sg_cfg_t *sg_cfg)
{
	union l3_vsi_u l3_vsi;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sg_cfg);

	aos_mem_zero(&l3_vsi, sizeof(union l3_vsi_u));

	rv = hppe_l3_vsi_get(dev_id, vsi, &l3_vsi);
	SW_RTN_ON_ERROR(rv);

	sg_cfg->ipv4_sg_pass_action = l3_vsi.bf.ipv4_sg_pass_cmd;
	sg_cfg->ipv6_sg_pass_action = l3_vsi.bf.ipv6_sg_pass_cmd;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_vsi_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi,
		fal_sg_cfg_t *sg_cfg)
{
	union l3_vsi_u l3_vsi;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sg_cfg);

	aos_mem_zero(&l3_vsi, sizeof(union l3_vsi_u));

	rv = hppe_l3_vsi_get(dev_id, vsi, &l3_vsi);
	SW_RTN_ON_ERROR(rv);

	l3_vsi.bf.ipv4_sg_pass_cmd = sg_cfg->ipv4_sg_pass_action;
	l3_vsi.bf.ipv6_sg_pass_cmd = sg_cfg->ipv6_sg_pass_action;

	return hppe_l3_vsi_set(dev_id, vsi, &l3_vsi);
}

sw_error_t
adpt_jhppe_ip_port_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
    			fal_sg_cfg_t *sg_cfg)
{
	union l3_vp_port_tbl_u l3_vp_port_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sg_cfg);

	aos_mem_zero(&l3_vp_port_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	sg_cfg->ipv4_sg_pass_action = l3_vp_port_tbl.bf.ipv4_sg_pass_cmd;
	sg_cfg->ipv6_sg_pass_action = l3_vp_port_tbl.bf.ipv6_sg_pass_cmd;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_port_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
    			fal_sg_cfg_t *sg_cfg)
{
	union l3_vp_port_tbl_u l3_vp_port_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sg_cfg);

	aos_mem_zero(&l3_vp_port_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l3_vp_port_tbl.bf.ipv4_sg_pass_cmd = sg_cfg->ipv4_sg_pass_action;
	l3_vp_port_tbl.bf.ipv6_sg_pass_cmd = sg_cfg->ipv6_sg_pass_action;

	return hppe_l3_vp_port_tbl_set(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
}

sw_error_t
adpt_jhppe_ip_vsi_arp_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi,
		fal_arp_sg_cfg_t *arp_sg_cfg)
{
	union l3_vsi_u l3_vsi;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(arp_sg_cfg);

	aos_mem_zero(&l3_vsi, sizeof(union l3_vsi_u));

	rv = hppe_l3_vsi_get(dev_id, vsi, &l3_vsi);
	SW_RTN_ON_ERROR(rv);

	arp_sg_cfg->ipv4_arp_sg_pass_action = l3_vsi.bf.ip_arp_sg_pass_cmd;
	arp_sg_cfg->ip_nd_sg_pass_action = l3_vsi.bf.ip_nd_sg_pass_cmd;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_vsi_arp_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi,
		fal_arp_sg_cfg_t *arp_sg_cfg)
{
	union l3_vsi_u l3_vsi;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(arp_sg_cfg);

	aos_mem_zero(&l3_vsi, sizeof(union l3_vsi_u));

	rv = hppe_l3_vsi_get(dev_id, vsi, &l3_vsi);
	SW_RTN_ON_ERROR(rv);

	l3_vsi.bf.ip_arp_sg_pass_cmd = arp_sg_cfg->ipv4_arp_sg_pass_action;
	l3_vsi.bf.ip_nd_sg_pass_cmd = arp_sg_cfg->ip_nd_sg_pass_action;

	return hppe_l3_vsi_set(dev_id, vsi, &l3_vsi);
}

sw_error_t
adpt_jhppe_ip_port_arp_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
    			fal_arp_sg_cfg_t *arp_sg_cfg)
{
	union l3_vp_port_tbl_u l3_vp_port_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(arp_sg_cfg);

	aos_mem_zero(&l3_vp_port_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	arp_sg_cfg->ipv4_arp_sg_pass_action = l3_vp_port_tbl.bf.ip_arp_sg_pass_cmd;
	arp_sg_cfg->ip_nd_sg_pass_action = l3_vp_port_tbl.bf.ip_nd_sg_pass_cmd;

	return SW_OK;
}

sw_error_t
adpt_jhppe_ip_port_arp_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
    			fal_arp_sg_cfg_t *arp_sg_cfg)
{
	union l3_vp_port_tbl_u l3_vp_port_tbl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(arp_sg_cfg);

	aos_mem_zero(&l3_vp_port_tbl, sizeof(union l3_vp_port_tbl_u));

	rv = hppe_l3_vp_port_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
	SW_RTN_ON_ERROR(rv);

	l3_vp_port_tbl.bf.ip_arp_sg_pass_cmd = arp_sg_cfg->ipv4_arp_sg_pass_action;
	l3_vp_port_tbl.bf.ip_nd_sg_pass_cmd = arp_sg_cfg->ip_nd_sg_pass_action;

	return hppe_l3_vp_port_tbl_set(dev_id, FAL_PORT_ID_VALUE(port_id), &l3_vp_port_tbl);
}

/**
 * @}
 */
