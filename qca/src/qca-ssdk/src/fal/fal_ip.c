/*
 * Copyright (c) 2012, 2015, 2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

sw_error_t fal_ip_host_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_host_counter_bind, dev_id, entry_id, cnt_id, enable)

sw_error_t fal_ip_host_pppoe_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t pppoe_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_host_pppoe_bind, dev_id, entry_id, pppoe_id, enable)

sw_error_t fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flags)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_pt_arp_learn_set, dev_id, port_id, flags)

sw_error_t fal_ip_pt_arp_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * flags)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_pt_arp_learn_get, dev_id, port_id, flags)

sw_error_t fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_arp_learn_set, dev_id, mode)

sw_error_t fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_arp_learn_get, dev_id, mode)

sw_error_t fal_ip_source_guard_set(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_source_guard_set, dev_id, port_id, mode)

sw_error_t fal_ip_source_guard_get(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_source_guard_get, dev_id, port_id, mode)

sw_error_t fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_unk_source_cmd_set, dev_id, cmd)

sw_error_t fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_unk_source_cmd_get, dev_id, cmd)

sw_error_t fal_ip_arp_guard_set(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_arp_guard_set, dev_id, port_id, mode)

sw_error_t fal_ip_arp_guard_get(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_arp_guard_get, dev_id, port_id, mode)

sw_error_t fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_unk_source_cmd_set, dev_id, cmd)

sw_error_t fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_unk_source_cmd_get, dev_id, cmd)

sw_error_t fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_route_status_set, dev_id, enable)

sw_error_t fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_route_status_get, dev_id, enable)

sw_error_t fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_intf_entry_add, dev_id, entry)

sw_error_t fal_ip_intf_entry_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_intf_mac_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_intf_entry_del, dev_id, del_mode, entry)

sw_error_t fal_ip_intf_entry_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_intf_mac_entry_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_intf_entry_next, dev_id, next_mode, entry)

sw_error_t fal_ip_age_time_set(a_uint32_t dev_id, a_uint32_t * time)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_age_time_set, dev_id, time)

sw_error_t fal_ip_age_time_get(a_uint32_t dev_id, a_uint32_t * time)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_age_time_get, dev_id, time)

sw_error_t fal_ip_wcmp_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_wcmp_hash_mode_set, dev_id, hash_mode)

sw_error_t fal_ip_wcmp_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_wcmp_hash_mode_get, dev_id, hash_mode)

sw_error_t fal_ip_vrf_base_addr_set(a_uint32_t dev_id, a_uint32_t vrf_id, fal_ip4_addr_t addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_vrf_base_addr_set, dev_id, vrf_id, addr)

sw_error_t fal_ip_vrf_base_addr_get(a_uint32_t dev_id, a_uint32_t vrf_id, fal_ip4_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_vrf_base_addr_get, dev_id, vrf_id, addr)

sw_error_t fal_ip_vrf_base_mask_set(a_uint32_t dev_id, a_uint32_t vrf_id, fal_ip4_addr_t addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_vrf_base_mask_set, dev_id, vrf_id, addr)

sw_error_t fal_ip_vrf_base_mask_get(a_uint32_t dev_id, a_uint32_t vrf_id, fal_ip4_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_vrf_base_mask_get, dev_id, vrf_id, addr)

sw_error_t fal_ip_default_route_set(a_uint32_t dev_id, a_uint32_t droute_id, fal_default_route_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_default_route_set, dev_id, droute_id, entry)

sw_error_t fal_ip_default_route_get(a_uint32_t dev_id, a_uint32_t droute_id, fal_default_route_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_default_route_get, dev_id, droute_id, entry)

sw_error_t fal_ip_host_route_set(a_uint32_t dev_id, a_uint32_t hroute_id, fal_host_route_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_host_route_set, dev_id, hroute_id, entry)

sw_error_t fal_ip_host_route_get(a_uint32_t dev_id, a_uint32_t hroute_id, fal_host_route_t * entry)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_host_route_get, dev_id, hroute_id, entry)

sw_error_t fal_ip_wcmp_entry_set(a_uint32_t dev_id, a_uint32_t wcmp_id, fal_ip_wcmp_t * wcmp)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_wcmp_entry_set, dev_id, wcmp_id, wcmp)

sw_error_t fal_ip_wcmp_entry_get(a_uint32_t dev_id, a_uint32_t wcmp_id, fal_ip_wcmp_t * wcmp)
    DEFINE_FAL_FUNC_HSL_EXPORT(ip_wcmp_entry_get, dev_id, wcmp_id, wcmp)

sw_error_t fal_ip_rfs_ip4_rule_set(a_uint32_t dev_id, fal_ip4_rfs_t * rfs)
    DEFINE_FAL_FUNC_HSL(ip_rfs_ip4_set, dev_id, rfs)
    EXPORT_SYMBOL(fal_ip_rfs_ip4_rule_set);

sw_error_t fal_ip_rfs_ip6_rule_set(a_uint32_t dev_id, fal_ip6_rfs_t * rfs)
    DEFINE_FAL_FUNC_HSL(ip_rfs_ip6_set, dev_id, rfs)
    EXPORT_SYMBOL(fal_ip_rfs_ip6_rule_set);

sw_error_t fal_ip_rfs_ip4_rule_del(a_uint32_t dev_id, fal_ip4_rfs_t * rfs)
    DEFINE_FAL_FUNC_HSL(ip_rfs_ip4_del, dev_id, rfs)
    EXPORT_SYMBOL(fal_ip_rfs_ip4_rule_del);

sw_error_t fal_ip_rfs_ip6_rule_del(a_uint32_t dev_id, fal_ip6_rfs_t * rfs)
    DEFINE_FAL_FUNC_HSL(ip_rfs_ip6_del, dev_id, rfs)
    EXPORT_SYMBOL(fal_ip_rfs_ip6_rule_del);

sw_error_t fal_default_flow_cmd_set(a_uint32_t dev_id, a_uint32_t vrf_id, fal_flow_type_t type, fal_default_flow_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL(ip_default_flow_cmd_set, dev_id, vrf_id, type, cmd)
    EXPORT_SYMBOL(fal_default_flow_cmd_set);

sw_error_t fal_default_flow_cmd_get(a_uint32_t dev_id, a_uint32_t vrf_id, fal_flow_type_t type, fal_default_flow_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL(ip_default_flow_cmd_get, dev_id, vrf_id, type, cmd)
    EXPORT_SYMBOL(fal_default_flow_cmd_get);

sw_error_t fal_default_rt_flow_cmd_set(a_uint32_t dev_id, a_uint32_t vrf_id, fal_flow_type_t type, fal_default_flow_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL(ip_default_rt_flow_cmd_set, dev_id, vrf_id, type, cmd)
    EXPORT_SYMBOL(fal_default_rt_flow_cmd_set);

sw_error_t fal_default_rt_flow_cmd_get(a_uint32_t dev_id, a_uint32_t vrf_id, fal_flow_type_t type, fal_default_flow_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL(ip_default_rt_flow_cmd_get, dev_id, vrf_id, type, cmd)
    EXPORT_SYMBOL(fal_default_rt_flow_cmd_get);

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

#if !defined(IN_IP_MINI)
sw_error_t fal_ip_network_route_add(a_uint32_t dev_id, a_uint32_t index, fal_network_route_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_add, dev_id, index, entry)

sw_error_t fal_ip_network_route_get(a_uint32_t dev_id, a_uint32_t index, a_uint8_t type, fal_network_route_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_get, dev_id, index, type, entry)

sw_error_t fal_ip_network_route_del(a_uint32_t dev_id, a_uint32_t index, a_uint8_t type)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ip_network_route_del, dev_id, index, type)
#endif

