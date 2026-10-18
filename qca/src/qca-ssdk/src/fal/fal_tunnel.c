/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_tunnel FAL_TUNNEL
 * @{
 */
#include "sw.h"
#include "hsl_api.h"
#include "adpt.h"
#include "fal_tunnel.h"

sw_error_t fal_tunnel_decap_entry_add(a_uint32_t dev_id, fal_tunnel_op_mode_t add_mode, fal_tunnel_decap_entry_t *decap_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_entry_add, dev_id, add_mode, decap_entry)

sw_error_t fal_tunnel_decap_entry_del(a_uint32_t dev_id, fal_tunnel_op_mode_t del_mode, fal_tunnel_decap_entry_t *decap_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_entry_del, dev_id, del_mode, decap_entry)

sw_error_t fal_tunnel_decap_entry_get(a_uint32_t dev_id, fal_tunnel_op_mode_t get_mode, fal_tunnel_decap_entry_t *decap_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_entry_get, dev_id, get_mode, decap_entry)

sw_error_t fal_tunnel_decap_entry_getnext(a_uint32_t dev_id, fal_tunnel_op_mode_t next_mode, fal_tunnel_decap_entry_t *decap_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_entry_getnext, dev_id, next_mode, decap_entry)

sw_error_t fal_tunnel_decap_entry_flush(a_uint32_t dev_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_entry_flush, dev_id)

sw_error_t fal_tunnel_global_cfg_get(a_uint32_t dev_id, fal_tunnel_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_global_cfg_get, dev_id, cfg)

sw_error_t fal_tunnel_global_cfg_set(a_uint32_t dev_id, fal_tunnel_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_global_cfg_set, dev_id, cfg)

sw_error_t fal_tunnel_port_intf_set(a_uint32_t dev_id, fal_port_t port_id, fal_tunnel_port_intf_t *port_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_port_intf_set, dev_id, port_id, port_cfg)

sw_error_t fal_tunnel_port_intf_get(a_uint32_t dev_id, fal_port_t port_id, fal_tunnel_port_intf_t *port_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_port_intf_get, dev_id, port_id, port_cfg)

sw_error_t fal_tunnel_intf_set(a_uint32_t dev_id, a_uint32_t l3_if, fal_tunnel_intf_t *intf_t)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_intf_set, dev_id, l3_if, intf_t)

sw_error_t fal_tunnel_intf_get(a_uint32_t dev_id, a_uint32_t l3_if, fal_tunnel_intf_t *intf_t)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_intf_get, dev_id, l3_if, intf_t)

sw_error_t fal_tunnel_encap_port_tunnelid_set(a_uint32_t dev_id, fal_port_t port_id, fal_tunnel_id_t *tunnel_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_port_tunnelid_set, dev_id, port_id, tunnel_id)

sw_error_t fal_tunnel_encap_port_tunnelid_get(a_uint32_t dev_id, fal_port_t port_id, fal_tunnel_id_t *tunnel_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_port_tunnelid_get, dev_id, port_id, tunnel_id)

sw_error_t fal_tunnel_encap_intf_tunnelid_set(a_uint32_t dev_id, a_uint32_t intf_id, fal_tunnel_id_t *tunnel_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_intf_tunnelid_set, dev_id, intf_id, tunnel_id)

sw_error_t fal_tunnel_encap_intf_tunnelid_get(a_uint32_t dev_id, a_uint32_t intf_id, fal_tunnel_id_t *tunnel_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_intf_tunnelid_get, dev_id, intf_id, tunnel_id)

sw_error_t fal_tunnel_encap_entry_add(a_uint32_t dev_id, a_uint32_t tunnel_id, fal_tunnel_encap_cfg_t *tunnel_encap_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_entry_add, dev_id, tunnel_id, tunnel_encap_cfg)

sw_error_t fal_tunnel_encap_entry_del(a_uint32_t dev_id, a_uint32_t tunnel_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_entry_del, dev_id, tunnel_id)

sw_error_t fal_tunnel_encap_entry_get(a_uint32_t dev_id, a_uint32_t tunnel_id, fal_tunnel_encap_cfg_t *tunnel_encap_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_entry_get, dev_id, tunnel_id, tunnel_encap_cfg)

sw_error_t fal_tunnel_encap_entry_getnext(a_uint32_t dev_id, a_uint32_t tunnel_id, fal_tunnel_encap_cfg_t *tunnel_encap_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_entry_getnext, dev_id, tunnel_id, tunnel_encap_cfg)

sw_error_t fal_tunnel_encap_rule_entry_set(a_uint32_t dev_id, a_uint32_t rule_id, fal_tunnel_encap_rule_t *rule_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_rule_entry_set, dev_id, rule_id, rule_entry)

sw_error_t fal_tunnel_encap_rule_entry_get(a_uint32_t dev_id, a_uint32_t rule_id, fal_tunnel_encap_rule_t *rule_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_rule_entry_get, dev_id, rule_id, rule_entry)

sw_error_t fal_tunnel_encap_rule_entry_del(a_uint32_t dev_id, a_uint32_t rule_id, fal_tunnel_encap_rule_t *rule_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_rule_entry_del, dev_id, rule_id, rule_entry)

sw_error_t fal_tunnel_udf_profile_entry_add(a_uint32_t dev_id, a_uint32_t profile_id, fal_tunnel_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_entry_add, dev_id, profile_id, entry)

sw_error_t fal_tunnel_udf_profile_entry_del(a_uint32_t dev_id, a_uint32_t profile_id, fal_tunnel_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_entry_del, dev_id, profile_id, entry)

sw_error_t fal_tunnel_udf_profile_entry_getfirst(a_uint32_t dev_id, a_uint32_t profile_id, fal_tunnel_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_entry_getfirst, dev_id, profile_id, entry)

sw_error_t fal_tunnel_udf_profile_entry_getnext(a_uint32_t dev_id, a_uint32_t profile_id, fal_tunnel_udf_profile_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_entry_getnext, dev_id, profile_id, entry)

sw_error_t fal_tunnel_udf_profile_cfg_set(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t udf_idx, fal_tunnel_udf_type_t udf_type, a_uint32_t offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_cfg_set, dev_id, profile_id, udf_idx, udf_type, offset)

sw_error_t fal_tunnel_udf_profile_cfg_get(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t udf_idx, fal_tunnel_udf_type_t * udf_type, a_uint32_t * offset)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_udf_profile_cfg_get, dev_id, profile_id, udf_idx, udf_type, offset)

sw_error_t fal_tunnel_encap_header_ctrl_set(a_uint32_t dev_id, fal_tunnel_encap_header_ctrl_t *header_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_header_ctrl_set, dev_id, header_ctrl)

sw_error_t fal_tunnel_encap_header_ctrl_get(a_uint32_t dev_id, fal_tunnel_encap_header_ctrl_t *header_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_header_ctrl_get, dev_id, header_ctrl)

sw_error_t fal_tunnel_exp_decap_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_exp_decap_set, dev_id, port_id, enable)

sw_error_t fal_tunnel_exp_decap_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_exp_decap_get, dev_id, port_id, enable)

sw_error_t fal_tunnel_decap_key_set(a_uint32_t dev_id, fal_tunnel_type_t tunnel_type, fal_tunnel_decap_key_t *decap_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_key_set, dev_id, tunnel_type, decap_key)

sw_error_t fal_tunnel_decap_key_get(a_uint32_t dev_id, fal_tunnel_type_t tunnel_type, fal_tunnel_decap_key_t *decap_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_key_get, dev_id, tunnel_type, decap_key)

sw_error_t fal_tunnel_decap_en_set(a_uint32_t dev_id, a_uint32_t tunnel_index, a_bool_t en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_en_set, dev_id, tunnel_index, en)

sw_error_t fal_tunnel_decap_en_get(a_uint32_t dev_id, a_uint32_t tunnel_index, a_bool_t *en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_en_get, dev_id, tunnel_index, en)

sw_error_t fal_tunnel_decap_action_update(a_uint32_t dev_id, a_uint32_t tunnel_index, fal_tunnel_action_t *update_action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_action_update, dev_id, tunnel_index, update_action)

sw_error_t fal_tunnel_decap_counter_get(a_uint32_t dev_id, a_uint32_t tunnel_index, fal_entry_counter_t *decap_counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_counter_get, dev_id, tunnel_index, decap_counter)

#ifndef IN_TUNNEL_MINI
sw_error_t fal_tunnel_vlan_intf_add(a_uint32_t dev_id, fal_tunnel_vlan_intf_t *vlan_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_vlan_intf_add, dev_id, vlan_cfg)

sw_error_t fal_tunnel_vlan_intf_getfirst(a_uint32_t dev_id, fal_tunnel_vlan_intf_t *vlan_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_vlan_intf_getfirst, dev_id, vlan_cfg)

sw_error_t fal_tunnel_vlan_intf_getnext(a_uint32_t dev_id, fal_tunnel_vlan_intf_t *vlan_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_vlan_intf_getnext, dev_id, vlan_cfg)

sw_error_t fal_tunnel_vlan_intf_del(a_uint32_t dev_id, fal_tunnel_vlan_intf_t *vlan_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_vlan_intf_del, dev_id, vlan_cfg)

sw_error_t fal_tunnel_decap_ecn_mode_set(a_uint32_t dev_id, fal_tunnel_decap_ecn_rule_t *ecn_rule, fal_tunnel_decap_ecn_action_t *ecn_action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_ecn_mode_set, dev_id, ecn_rule, ecn_action)

sw_error_t fal_tunnel_decap_ecn_mode_get(a_uint32_t dev_id, fal_tunnel_decap_ecn_rule_t *ecn_rule, fal_tunnel_decap_ecn_action_t *ecn_action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_decap_ecn_mode_get, dev_id, ecn_rule, ecn_action)

sw_error_t fal_tunnel_encap_ecn_mode_set(a_uint32_t dev_id, fal_tunnel_encap_ecn_t *ecn_rule, fal_tunnel_ecn_val_t *ecn_value)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_ecn_mode_set, dev_id, ecn_rule, ecn_value)

sw_error_t fal_tunnel_encap_ecn_mode_get(a_uint32_t dev_id, fal_tunnel_encap_ecn_t *ecn_rule, fal_tunnel_ecn_val_t *ecn_value)
    DEFINE_FAL_FUNC_ADPT_EXPORT(tunnel_encap_ecn_mode_get, dev_id, ecn_rule, ecn_value)
#endif

