/*
 * Copyright (c) 2012, 2015, 2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_ip FAL_IP
 * @{
 */
#include "sw.h"
#include "fal_ip.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
    DEFINE_FAL_FUNC_EXPORT(ip_host_add, dev_id, host_entry)

sw_error_t fal_ip_host_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_host_entry_t * host_entry)
    DEFINE_FAL_FUNC_EXPORT(ip_host_del, dev_id, del_mode, host_entry)

sw_error_t fal_ip_host_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_host_entry_t * host_entry)
    DEFINE_FAL_FUNC_EXPORT(ip_host_get, dev_id, get_mode, host_entry)

sw_error_t fal_ip_host_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_host_entry_t * host_entry)
    DEFINE_FAL_FUNC_EXPORT(ip_host_next, dev_id, next_mode, host_entry)

sw_error_t fal_ip_vsi_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi, fal_sg_cfg_t *sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_sg_cfg_get, dev_id, vsi, sg_cfg)

sw_error_t fal_ip_port_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_sg_cfg_t *sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_sg_cfg_set, dev_id, port_id, sg_cfg)

sw_error_t fal_ip_port_intf_get(a_uint32_t dev_id, fal_port_t port_id, fal_intf_id_t *id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_intf_get, dev_id, port_id, id)

sw_error_t fal_ip_vsi_arp_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi, fal_arp_sg_cfg_t *arp_sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_arp_sg_cfg_set, dev_id, vsi, arp_sg_cfg)

sw_error_t fal_ip_pub_addr_get(a_uint32_t dev_id, a_uint32_t index, fal_ip_pub_addr_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_pub_addr_get, dev_id, index, entry)

sw_error_t fal_ip_port_intf_set(a_uint32_t dev_id, fal_port_t port_id, fal_intf_id_t *id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_intf_set, dev_id, port_id, id)

sw_error_t fal_ip_vsi_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi, fal_sg_cfg_t *sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_sg_cfg_set, dev_id, vsi, sg_cfg)

sw_error_t fal_ip_port_macaddr_set(a_uint32_t dev_id, fal_port_t port_id, fal_macaddr_entry_t *macaddr)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_macaddr_set, dev_id, port_id, macaddr)

sw_error_t fal_ip_vsi_intf_get(a_uint32_t dev_id, a_uint32_t vsi, fal_intf_id_t *id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_intf_get, dev_id, vsi, id)

sw_error_t fal_ip_port_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_sg_cfg_t *sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_sg_cfg_get, dev_id, port_id, sg_cfg)

sw_error_t fal_ip_intf_get( a_uint32_t dev_id, a_uint32_t index, fal_intf_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_get, dev_id, index, entry)

sw_error_t fal_ip_pub_addr_set(a_uint32_t dev_id, a_uint32_t index, fal_ip_pub_addr_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_pub_addr_set, dev_id, index, entry)

sw_error_t fal_ip_route_mismatch_action_get(a_uint32_t dev_id, fal_fwd_cmd_t *action)
    DEFINE_FAL_FUNC_ADPT(ip_route_mismatch_get, dev_id, action)
    EXPORT_SYMBOL(fal_ip_route_mismatch_action_get);

sw_error_t fal_ip_vsi_arp_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi, fal_arp_sg_cfg_t *arp_sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_arp_sg_cfg_get, dev_id, vsi, arp_sg_cfg)

sw_error_t fal_ip_port_arp_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_arp_sg_cfg_t *arp_sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_arp_sg_cfg_set, dev_id, port_id, arp_sg_cfg)

sw_error_t fal_ip_vsi_mc_mode_set(a_uint32_t dev_id, a_uint32_t vsi, fal_mc_mode_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_mc_mode_set, dev_id, vsi, cfg)

sw_error_t fal_ip_vsi_intf_set(a_uint32_t dev_id, a_uint32_t vsi, fal_intf_id_t *id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_intf_set, dev_id, vsi, id)

sw_error_t fal_ip_nexthop_get(a_uint32_t dev_id, a_uint32_t index, fal_ip_nexthop_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_nexthop_get, dev_id, index, entry)

sw_error_t fal_ip_route_mismatch_action_set(a_uint32_t dev_id, fal_fwd_cmd_t action)
    DEFINE_FAL_FUNC_ADPT(ip_route_mismatch_set, dev_id, action)
    EXPORT_SYMBOL(fal_ip_route_mismatch_action_set);

sw_error_t fal_ip_intf_set( a_uint32_t dev_id, a_uint32_t index, fal_intf_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_set, dev_id, index, entry)

sw_error_t fal_ip_vsi_mc_mode_get(a_uint32_t dev_id, a_uint32_t vsi, fal_mc_mode_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_vsi_mc_mode_get, dev_id, vsi, cfg)

sw_error_t fal_ip_port_macaddr_get(a_uint32_t dev_id, fal_port_t port_id, fal_macaddr_entry_t *macaddr)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_macaddr_get, dev_id, port_id, macaddr)

sw_error_t fal_ip_port_arp_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_arp_sg_cfg_t *arp_sg_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_arp_sg_cfg_get, dev_id, port_id, arp_sg_cfg)

sw_error_t fal_ip_nexthop_set(a_uint32_t dev_id, a_uint32_t index, fal_ip_nexthop_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_nexthop_set, dev_id, index, entry)

sw_error_t fal_ip_global_ctrl_set(a_uint32_t dev_id, fal_ip_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_global_ctrl_set, dev_id, cfg)

sw_error_t fal_ip_global_ctrl_get(a_uint32_t dev_id, fal_ip_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_global_ctrl_get, dev_id, cfg)

sw_error_t fal_ip_intf_mtu_mru_set(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t mtu, a_uint32_t mru)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_mtu_mru_set, dev_id, l3_if, mtu, mru)

sw_error_t fal_ip_intf_mtu_mru_get(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t *mtu, a_uint32_t *mru)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_mtu_mru_get, dev_id, l3_if, mtu, mru)

sw_error_t fal_ip6_intf_mtu_mru_set(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t mtu, a_uint32_t mru)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip6_intf_mtu_mru_set, dev_id, l3_if, mtu, mru)

sw_error_t fal_ip6_intf_mtu_mru_get(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t *mtu, a_uint32_t *mru)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip6_intf_mtu_mru_get, dev_id, l3_if, mtu, mru)

sw_error_t fal_ip_intf_macaddr_add(a_uint32_t dev_id, a_uint32_t l3_if, fal_intf_macaddr_t *mac)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_macaddr_add, dev_id, l3_if, mac)

sw_error_t fal_ip_intf_macaddr_del(a_uint32_t dev_id, a_uint32_t l3_if, fal_intf_macaddr_t *mac)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_macaddr_del, dev_id, l3_if, mac)

sw_error_t fal_ip_intf_macaddr_get_first(a_uint32_t dev_id, a_uint32_t l3_if, fal_intf_macaddr_t *mac)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_macaddr_get_first, dev_id, l3_if, mac)

sw_error_t fal_ip_intf_macaddr_get_next(a_uint32_t dev_id, a_uint32_t l3_if, fal_intf_macaddr_t *mac)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_macaddr_get_next, dev_id, l3_if, mac)

sw_error_t fal_ip_intf_dmac_check_set(a_uint32_t dev_id, a_uint32_t l3_if, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_dmac_check_set, dev_id, l3_if, enable)

sw_error_t fal_ip_intf_dmac_check_get(a_uint32_t dev_id, a_uint32_t l3_if, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_dmac_check_get, dev_id, l3_if, enable)

sw_error_t fal_ip_port_vlan_as_flow_key_set(a_uint32_t dev_id, fal_port_t port_id,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_vlan_as_flow_key_set, dev_id, port_id, vlan_key)

sw_error_t fal_ip_port_vlan_as_flow_key_get(a_uint32_t dev_id, fal_port_t port_id,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_port_vlan_as_flow_key_get, dev_id, port_id, vlan_key)

sw_error_t fal_ip_intf_vlan_as_flow_key_set(a_uint32_t dev_id, a_uint32_t intf,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_vlan_as_flow_key_set, dev_id, intf, vlan_key)

sw_error_t fal_ip_intf_vlan_as_flow_key_get(a_uint32_t dev_id, a_uint32_t intf,
				 fal_ip_vlan_as_flow_key_t *vlan_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_intf_vlan_as_flow_key_get, dev_id, intf, vlan_key)

#if !defined(IN_IP_MINI)
sw_error_t fal_ip_network_route_add(a_uint32_t dev_id, a_uint32_t index, fal_network_route_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_add, dev_id, index, entry)

sw_error_t fal_ip_network_route_get(a_uint32_t dev_id, a_uint32_t index, a_uint8_t type, fal_network_route_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_get, dev_id, index, type, entry)

sw_error_t fal_ip_network_route_del(a_uint32_t dev_id, a_uint32_t index, a_uint8_t type)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_del, dev_id, index, type)
#endif

