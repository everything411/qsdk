/*
 * Copyright (c) 2012, 2015, 2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_ip_legacy FAL_IP_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_ip_legacy.h"
#include "hsl_api.h"
#include "adpt.h"

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

/**
 * @}
 */
