/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_VXLAN_API_H_
#define _APPE_VXLAN_API_H_

#define UDP_PORT_CFG_MAX_ENTRY	UDP_PORT_CFG_NUM

sw_error_t
appe_udp_port_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union udp_port_cfg_u *value);

sw_error_t
appe_udp_port_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union udp_port_cfg_u *value);

sw_error_t
appe_udp_port_cfg_ip_ver_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_udp_port_cfg_ip_ver_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_udp_port_cfg_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_udp_port_cfg_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_udp_port_cfg_port_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_udp_port_cfg_port_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_udp_port_cfg_udp_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_udp_port_cfg_udp_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
appe_tpr_vxlan_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_cfg_u *value);

sw_error_t
appe_tpr_vxlan_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_cfg_u *value);

sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
appe_tpr_vxlan_gpe_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_cfg_u *value);

sw_error_t
appe_tpr_vxlan_gpe_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_cfg_u *value);

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_gpe_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_get(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_prot_cfg_u *value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_set(
		a_uint32_t dev_id,
		union tpr_vxlan_gpe_prot_cfg_u *value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ethernet_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ethernet_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ipv4_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ipv4_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ipv6_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_vxlan_gpe_prot_cfg_ipv6_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
