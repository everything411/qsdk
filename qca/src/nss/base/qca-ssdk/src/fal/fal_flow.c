/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_flow FAL_FLOW
 * @{
 */
#include "sw.h"
#include "fal_flow.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_flow_host_add( a_uint32_t dev_id, a_uint32_t add_mode, fal_flow_host_entry_t *flow_host_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_host_add, dev_id, add_mode, flow_host_entry)

sw_error_t fal_flow_entry_get( a_uint32_t dev_id, a_uint32_t get_mode, fal_flow_entry_t *flow_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_get, dev_id, get_mode, flow_entry)

sw_error_t fal_flow_entry_del( a_uint32_t dev_id, a_uint32_t del_mode, fal_flow_entry_t *flow_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_del, dev_id, del_mode, flow_entry)

sw_error_t fal_flow_entry_next( a_uint32_t dev_id, a_uint32_t next_mode, fal_flow_entry_t *flow_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_next, dev_id, next_mode, flow_entry)

sw_error_t fal_flow_status_get(a_uint32_t dev_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_status_get, dev_id, enable)

sw_error_t fal_flow_mgmt_set( a_uint32_t dev_id, fal_flow_pkt_type_t type, fal_flow_direction_t dir, fal_flow_mgmt_t *mgmt)
    DEFINE_FAL_FUNC_ADPT(flow_ctrl_set, dev_id, type, dir, mgmt)
    EXPORT_SYMBOL(fal_flow_mgmt_set);

sw_error_t fal_flow_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_status_set, dev_id, enable)

sw_error_t fal_flow_host_get( a_uint32_t dev_id, a_uint32_t get_mode, fal_flow_host_entry_t *flow_host_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_host_get, dev_id, get_mode, flow_host_entry)

sw_error_t fal_flow_host_del( a_uint32_t dev_id, a_uint32_t del_mode, fal_flow_host_entry_t *flow_host_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_host_del, dev_id, del_mode, flow_host_entry)

sw_error_t fal_flow_mgmt_get( a_uint32_t dev_id, fal_flow_pkt_type_t type, fal_flow_direction_t dir, fal_flow_mgmt_t *mgmt)
    DEFINE_FAL_FUNC_ADPT(flow_ctrl_get, dev_id, type, dir, mgmt)
    EXPORT_SYMBOL(fal_flow_mgmt_get);

sw_error_t fal_flow_entry_add( a_uint32_t dev_id, a_uint32_t add_mode, /*index or hash*/ fal_flow_entry_t *flow_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_add, dev_id, add_mode, flow_entry)

sw_error_t fal_flow_global_cfg_get( a_uint32_t dev_id, fal_flow_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_global_cfg_get, dev_id, cfg)

sw_error_t fal_flow_global_cfg_set( a_uint32_t dev_id, fal_flow_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_global_cfg_set, dev_id, cfg)

sw_error_t fal_flow_counter_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_entry_counter_t *flow_counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_counter_get, dev_id, flow_index, flow_counter)

sw_error_t fal_flow_counter_cleanup(a_uint32_t dev_id, a_uint32_t flow_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_counter_cleanup, dev_id, flow_index)

sw_error_t fal_flow_entry_en_set(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_en_set, dev_id, flow_index, enable)

sw_error_t fal_flow_entry_en_get(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_entry_en_get, dev_id, flow_index, enable)

sw_error_t fal_flow_qos_set(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_qos_set, dev_id, flow_index, flow_qos)

sw_error_t fal_flow_qos_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_qos_t *flow_qos)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_qos_get, dev_id, flow_index, flow_qos)

sw_error_t fal_flow_npt66_prefix_add(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t prefix_len)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_prefix_add, dev_id, l3_if_index, ip6, prefix_len)

sw_error_t fal_flow_npt66_prefix_get(a_uint32_t dev_id, a_uint32_t l3_if_index, fal_ip6_addr_t *ip6, a_uint32_t *prefix_len)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_prefix_get, dev_id, l3_if_index, ip6, prefix_len)

sw_error_t fal_flow_npt66_prefix_del(a_uint32_t dev_id, a_uint32_t l3_if_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_prefix_del, dev_id, l3_if_index)

sw_error_t fal_flow_npt66_iid_cal(a_uint32_t dev_id, fal_flow_npt66_iid_calc_t *iid_cal, fal_flow_npt66_iid_t *iid_result)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_iid_cal, dev_id, iid_cal, iid_result)

sw_error_t fal_flow_npt66_iid_add(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_npt66_iid_t *iid_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_iid_add, dev_id, flow_index, iid_entry)

sw_error_t fal_flow_npt66_iid_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_npt66_iid_t *iid_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_iid_get, dev_id, flow_index, iid_entry)

sw_error_t fal_flow_npt66_iid_del(a_uint32_t dev_id, a_uint32_t flow_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_iid_del, dev_id, flow_index)

sw_error_t fal_flow_npt66_status_get(a_uint32_t dev_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_status_get, dev_id, enable)

sw_error_t fal_flow_npt66_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_npt66_status_set, dev_id, enable)

sw_error_t fal_flow_key_get(a_uint32_t dev_id, fal_flow_protocol_type_t key_type, fal_flow_key_t *flow_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_key_get, dev_id, key_type, flow_key)

sw_error_t fal_flow_key_set(a_uint32_t dev_id, fal_flow_protocol_type_t key_type, fal_flow_key_t *flow_key)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_key_set, dev_id, key_type, flow_key)

sw_error_t fal_flow_sampling_id_get(a_uint32_t dev_id, a_uint32_t flow_index, a_uint32_t *sampling_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_sampling_id_get, dev_id, flow_index, sampling_id)

sw_error_t fal_flow_sampling_id_set(a_uint32_t dev_id, a_uint32_t flow_index, a_uint32_t sampling_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_sampling_id_set, dev_id, flow_index, sampling_id)

sw_error_t fal_flow_gro_en_get(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_gro_en_get, dev_id, flow_index, enable)

sw_error_t fal_flow_gro_en_set(a_uint32_t dev_id, a_uint32_t flow_index, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_gro_en_set, dev_id, flow_index, enable)

sw_error_t fal_flow_eip_lookup_mode_set(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_eip_lookup_mode_set, dev_id, mode)

sw_error_t fal_flow_eip_lookup_mode_get(a_uint32_t dev_id, fal_flow_eip_lookup_mode_t *mode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_eip_lookup_mode_get, dev_id, mode)

sw_error_t fal_flow_fwd_type_set(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_fwd_type_t fwd_type)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_fwd_type_set, dev_id, flow_index, fwd_type)

sw_error_t fal_flow_fwd_type_get(a_uint32_t dev_id, a_uint32_t flow_index, fal_flow_fwd_type_t *fwd_type)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_fwd_type_get, dev_id, flow_index, fwd_type)

sw_error_t fal_flow_app_entry_add(a_uint32_t dev_id, fal_flow_app_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_app_entry_add, dev_id, entry)

sw_error_t fal_flow_app_entry_get(a_uint32_t dev_id, fal_flow_app_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_app_entry_get, dev_id, entry)

sw_error_t fal_flow_app_entry_del(a_uint32_t dev_id, fal_flow_app_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_app_entry_del, dev_id, entry)

#if !defined(IN_FLOW_MINI)
sw_error_t fal_flow_age_timer_set(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_age_timer_set, dev_id, age_timer)

sw_error_t fal_flow_age_timer_get(a_uint32_t dev_id, fal_flow_age_timer_t *age_timer)
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_age_timer_get, dev_id, age_timer)
#endif

