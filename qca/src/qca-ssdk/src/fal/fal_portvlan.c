/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_port_vlan FAL_PORT_VLAN
 * @{
 */
#include "sw.h"
#include "fal_portvlan.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_port_vlan_trans_adv_add(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_trans_adv_add, dev_id, port_id, direction, rule, action)

sw_error_t fal_port_vlan_trans_adv_del(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_trans_adv_del, dev_id, port_id, direction, rule, action)

sw_error_t fal_port_vlan_trans_adv_getfirst(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_trans_adv_getfirst, dev_id, port_id, direction, rule, action)

sw_error_t fal_port_vlan_trans_adv_getnext(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_trans_adv_getnext, dev_id, port_id, direction, rule, action)

sw_error_t fal_portvlan_member_update(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t mem_port_map)
    DEFINE_FAL_FUNC_EXPORT(portvlan_member_update, dev_id, port_id, mem_port_map)

sw_error_t fal_portvlan_member_get(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t * mem_port_map)
    DEFINE_FAL_FUNC_EXPORT(portvlan_member_get, dev_id, port_id, mem_port_map)

sw_error_t fal_port_vlantag_egmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_vlantag_egress_mode_t *port_egvlanmode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlantag_egmode_set, dev_id, port_id, port_egvlanmode)

sw_error_t fal_port_vlantag_egmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlantag_egress_mode_t *port_egvlanmode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlantag_egmode_get, dev_id, port_id, port_egvlanmode)

sw_error_t fal_port_vlantag_vsi_egmode_enable(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT(port_vlantag_vsi_egmode_enable_set, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_vlantag_vsi_egmode_enable);

sw_error_t fal_port_vlantag_vsi_egmode_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_ADPT(port_vlantag_vsi_egmode_enable_get, dev_id, port_id, enable)
    EXPORT_SYMBOL(fal_port_vlantag_vsi_egmode_status_get);

sw_error_t fal_port_vlan_xlt_miss_cmd_set(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_xlt_miss_cmd_set, dev_id, port_id, cmd)

sw_error_t fal_port_vlan_xlt_miss_cmd_get(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t *cmd)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_xlt_miss_cmd_get, dev_id, port_id, cmd)

sw_error_t fal_port_qinq_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_qinq_mode_set, dev_id, port_id, mode)

sw_error_t fal_port_qinq_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_qinq_mode_get, dev_id, port_id, mode)

sw_error_t fal_ingress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid)
    DEFINE_FAL_FUNC_ADPT(tpid_set, dev_id, tpid)
    EXPORT_SYMBOL(fal_ingress_tpid_set);

sw_error_t fal_ingress_tpid_get(a_uint32_t dev_id, fal_tpid_t * tpid)
    DEFINE_FAL_FUNC_ADPT(tpid_get, dev_id, tpid)
    EXPORT_SYMBOL(fal_ingress_tpid_get);

sw_error_t fal_egress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid)
    DEFINE_FAL_FUNC_ADPT_EXPORT(egress_tpid_set, dev_id, tpid)

sw_error_t fal_egress_tpid_get(a_uint32_t dev_id, fal_tpid_t * tpid)
    DEFINE_FAL_FUNC_ADPT_EXPORT(egress_tpid_get, dev_id, tpid)

sw_error_t fal_port_vlan_propagation_set(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vlan_propagation_set, dev_id, port_id, mode)

sw_error_t fal_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t port_egvlanmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_egvlanmode_set, dev_id, port_id, port_egvlanmode)

sw_error_t fal_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t * pport_egvlanmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_egvlanmode_get, dev_id, port_id, pport_egvlanmode)

sw_error_t fal_global_qinq_mode_set(a_uint32_t dev_id, fal_global_qinq_mode_t *mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(global_qinq_mode_set, dev_id, mode)

sw_error_t fal_global_qinq_mode_get(a_uint32_t dev_id, fal_global_qinq_mode_t *mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(global_qinq_mode_get, dev_id, mode)

sw_error_t fal_port_vsi_egmode_set(a_uint32_t dev_id, a_uint32_t vsi, a_uint32_t port_id, fal_pt_1q_egmode_t egmode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vsi_egmode_set, dev_id, vsi, port_id, egmode)

sw_error_t fal_port_vsi_egmode_get(a_uint32_t dev_id, a_uint32_t vsi, a_uint32_t port_id, fal_pt_1q_egmode_t * egmode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vsi_egmode_get, dev_id, vsi, port_id, egmode)

sw_error_t fal_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t port_1qmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_1qmode_set, dev_id, port_id, port_1qmode)

sw_error_t fal_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t * pport_1qmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_1qmode_get, dev_id, port_id, pport_1qmode)

sw_error_t fal_port_default_svid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_svid_set, dev_id, port_id, vid)

sw_error_t fal_port_default_svid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_svid_get, dev_id, port_id, vid)

sw_error_t fal_port_default_cvid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_cvid_set, dev_id, port_id, vid)

sw_error_t fal_port_default_cvid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_cvid_get, dev_id, port_id, vid)

sw_error_t fal_port_tls_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_tls_set, dev_id, port_id, enable)

sw_error_t fal_qinq_mode_set(a_uint32_t dev_id, fal_qinq_mode_t mode)
    DEFINE_FAL_FUNC_EXPORT(qinq_mode_set, dev_id, mode)

sw_error_t fal_qinq_mode_get(a_uint32_t dev_id, fal_qinq_mode_t * mode)
    DEFINE_FAL_FUNC_EXPORT(qinq_mode_get, dev_id, mode)

sw_error_t fal_port_qinq_role_set(a_uint32_t dev_id, fal_port_t port_id, fal_qinq_port_role_t role)
    DEFINE_FAL_FUNC_EXPORT(port_qinq_role_set, dev_id, port_id, role)

sw_error_t fal_port_qinq_role_get(a_uint32_t dev_id, fal_port_t port_id, fal_qinq_port_role_t * role)
    DEFINE_FAL_FUNC_EXPORT(port_qinq_role_get, dev_id, port_id, role)

sw_error_t fal_port_default_vlantag_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_port_default_vid_enable_t *default_vid_en, fal_port_vlan_tag_t *default_tag)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_default_vlantag_set, dev_id, port_id, direction, default_vid_en, default_tag)

sw_error_t fal_port_default_vlantag_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_port_default_vid_enable_t *default_vid_en, fal_port_vlan_tag_t *default_tag)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_default_vlantag_get, dev_id, port_id, direction, default_vid_en, default_tag)

sw_error_t fal_nestvlan_tpid_set(a_uint32_t dev_id, a_uint32_t tpid)
    DEFINE_FAL_FUNC_HSL_EXPORT(nestvlan_tpid_set, dev_id, tpid)

sw_error_t fal_nestvlan_tpid_get(a_uint32_t dev_id, a_uint32_t * tpid)
    DEFINE_FAL_FUNC_HSL_EXPORT(nestvlan_tpid_get, dev_id, tpid)

sw_error_t fal_port_ingress_vlan_filter_set(a_uint32_t dev_id, fal_port_t port_id, fal_ingress_vlan_filter_t *filter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_ingress_vlan_filter_set, dev_id, port_id, filter)

sw_error_t fal_port_ingress_vlan_filter_get(a_uint32_t dev_id, fal_port_t port_id, fal_ingress_vlan_filter_t * filter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_ingress_vlan_filter_get, dev_id, port_id, filter)

sw_error_t fal_port_invlan_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_invlan_mode_t mode)
    DEFINE_FAL_FUNC_EXPORT(port_invlan_mode_set, dev_id, port_id, mode)

sw_error_t fal_port_invlan_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_invlan_mode_t * mode)
    DEFINE_FAL_FUNC_EXPORT(port_invlan_mode_get, dev_id, port_id, mode)

sw_error_t fal_port_force_default_vid_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_default_vid_set, dev_id, port_id, enable)

sw_error_t fal_port_force_default_vid_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_default_vid_get, dev_id, port_id, enable)

sw_error_t fal_port_force_portvlan_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_portvlan_set, dev_id, port_id, enable)

sw_error_t fal_port_force_portvlan_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_portvlan_get, dev_id, port_id, enable)

sw_error_t fal_port_tls_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_tls_get, dev_id, port_id, enable)

sw_error_t fal_port_pri_propagation_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_pri_propagation_set, dev_id, port_id, enable)

sw_error_t fal_port_pri_propagation_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_pri_propagation_get, dev_id, port_id, enable)

sw_error_t fal_port_vlan_propagation_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vlan_propagation_get, dev_id, port_id, mode)

sw_error_t fal_port_mac_vlan_xlt_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mac_vlan_xlt_set, dev_id, port_id, enable)

sw_error_t fal_port_mac_vlan_xlt_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mac_vlan_xlt_get, dev_id, port_id, enable)

sw_error_t fal_netisolate_set(a_uint32_t dev_id, a_uint32_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(netisolate_set, dev_id, enable)

sw_error_t fal_netisolate_get(a_uint32_t dev_id, a_uint32_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(netisolate_get, dev_id, enable)

sw_error_t fal_eg_trans_filter_bypass_en_set(a_uint32_t dev_id, a_uint32_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eg_trans_filter_bypass_en_set, dev_id, enable)

sw_error_t fal_eg_trans_filter_bypass_en_get(a_uint32_t dev_id, a_uint32_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eg_trans_filter_bypass_en_get, dev_id, enable)

sw_error_t fal_port_vrf_id_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vrf_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vrf_id_set, dev_id, port_id, vrf_id)

sw_error_t fal_port_vrf_id_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vrf_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vrf_id_get, dev_id, port_id, vrf_id)

sw_error_t fal_portvlan_member_add(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t mem_port_id)
    DEFINE_FAL_FUNC_EXPORT(portvlan_member_add, dev_id, port_id, mem_port_id)

sw_error_t fal_portvlan_member_del(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t mem_port_id)
    DEFINE_FAL_FUNC_EXPORT(portvlan_member_del, dev_id, port_id, mem_port_id)

#ifndef IN_PORTVLAN_MINI
sw_error_t fal_port_vlan_trans_add(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry)
    DEFINE_FAL_FUNC_EXPORT(port_vlan_trans_add, dev_id, port_id, entry)

sw_error_t fal_port_vlan_trans_del(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry)
    DEFINE_FAL_FUNC_EXPORT(port_vlan_trans_del, dev_id, port_id, entry)

sw_error_t fal_port_vlan_trans_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_trans_entry_t *entry)
    DEFINE_FAL_FUNC_EXPORT(port_vlan_trans_get, dev_id, port_id, entry)

sw_error_t fal_port_vlan_trans_iterate(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * iterator, fal_vlan_trans_entry_t * entry)
    DEFINE_FAL_FUNC_EXPORT(port_vlan_trans_iterate, dev_id, port_id, iterator, entry)

sw_error_t fal_port_tag_propagation_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlantag_propagation_t *prop)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_tag_propagation_set, dev_id, port_id, direction, prop)

sw_error_t fal_port_tag_propagation_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_vlan_direction_t direction, fal_vlantag_propagation_t *prop)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_tag_propagation_get, dev_id, port_id, direction, prop)

sw_error_t fal_port_vlan_counter_get(a_uint32_t dev_id, a_uint32_t cnt_index, fal_port_vlan_counter_t * counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_counter_get, dev_id, cnt_index, counter)

sw_error_t fal_port_vlan_counter_cleanup(a_uint32_t dev_id, a_uint32_t cnt_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_counter_cleanup, dev_id, cnt_index)

sw_error_t fal_port_vlan_vpgroup_set(a_uint32_t dev_id, a_uint32_t vport, fal_port_vlan_direction_t direction, a_uint32_t vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_vpgroup_set, dev_id, vport, direction, vpgroup_id)

sw_error_t fal_port_vlan_vpgroup_get(a_uint32_t dev_id, a_uint32_t vport, fal_port_vlan_direction_t direction, a_uint32_t *vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_vlan_vpgroup_get, dev_id, vport, direction, vpgroup_id)

sw_error_t fal_portvlan_isol_set(a_uint32_t dev_id, fal_port_t port_id, fal_portvlan_isol_ctrl_t *isol_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(portvlan_isol_set, dev_id, port_id, isol_ctrl)

sw_error_t fal_portvlan_isol_get(a_uint32_t dev_id, fal_port_t port_id, fal_portvlan_isol_ctrl_t *isol_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(portvlan_isol_get, dev_id, port_id, isol_ctrl)

sw_error_t fal_portvlan_isol_group_set(a_uint32_t dev_id, a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(portvlan_isol_group_set, dev_id, isol_group_id, isol_group_bmp)

sw_error_t fal_portvlan_isol_group_get(a_uint32_t dev_id, a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(portvlan_isol_group_get, dev_id, isol_group_id, isol_group_bmp)

sw_error_t fal_port_egress_vlan_filter_set(a_uint32_t dev_id, fal_port_t port_id, fal_egress_vlan_filter_t *filter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_egress_vlan_filter_set, dev_id, port_id, filter)

sw_error_t fal_port_egress_vlan_filter_get(a_uint32_t dev_id, fal_port_t port_id, fal_egress_vlan_filter_t *filter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_egress_vlan_filter_get, dev_id, port_id, filter)
#endif

