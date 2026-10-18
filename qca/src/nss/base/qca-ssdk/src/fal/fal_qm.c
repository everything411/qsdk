/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_qm FAL_QM
 * @{
 */
#include "sw.h"
#include "fal_qm.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_ac_dynamic_threshold_get( a_uint32_t dev_id, a_uint32_t queue_id, fal_ac_dynamic_threshold_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_dynamic_threshold_get, dev_id, queue_id, cfg)

sw_error_t fal_ucast_queue_base_profile_get( a_uint32_t dev_id, fal_ucast_queue_dest_t *queue_dest, a_uint32_t *queue_base, a_uint8_t *profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_queue_base_profile_get, dev_id, queue_dest, queue_base, profile)

sw_error_t fal_ucast_hash_map_set( a_uint32_t dev_id, a_uint8_t profile, a_uint8_t rss_hash, a_int8_t queue_hash)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_hash_map_set, dev_id, profile, rss_hash, queue_hash)

sw_error_t fal_ac_dynamic_threshold_set( a_uint32_t dev_id, a_uint32_t queue_id, fal_ac_dynamic_threshold_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_dynamic_threshold_set, dev_id, queue_id, cfg)

sw_error_t fal_ac_prealloc_buffer_set( a_uint32_t dev_id, fal_ac_obj_t *obj, a_uint16_t num)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_prealloc_buffer_set, dev_id, obj, num)

sw_error_t fal_ac_queue_group_get( a_uint32_t dev_id, a_uint32_t queue_id, a_uint8_t *group_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_queue_group_get, dev_id, queue_id, group_id)

sw_error_t fal_ac_ctrl_get( a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_ctrl_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_ctrl_get, dev_id, obj, cfg)

sw_error_t fal_ac_prealloc_buffer_get( a_uint32_t dev_id, fal_ac_obj_t *obj, a_uint16_t *num)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_prealloc_buffer_get, dev_id, obj, num)

sw_error_t fal_ucast_hash_map_get( a_uint32_t dev_id, a_uint8_t profile, a_uint8_t rss_hash, a_int8_t *queue_hash)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_hash_map_get, dev_id, profile, rss_hash, queue_hash)

sw_error_t fal_ac_static_threshold_set( a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_static_threshold_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_static_threshold_set, dev_id, obj, cfg)

sw_error_t fal_ac_queue_group_set( a_uint32_t dev_id, a_uint32_t queue_id, a_uint8_t group_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_queue_group_set, dev_id, queue_id, group_id)

sw_error_t fal_ac_group_buffer_get( a_uint32_t dev_id, a_uint8_t group_id, fal_ac_group_buffer_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_group_buffer_get, dev_id, group_id, cfg)

sw_error_t fal_ac_ctrl_set( a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_ctrl_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_ctrl_set, dev_id, obj, cfg)

sw_error_t fal_ucast_priority_class_get( a_uint32_t dev_id, a_uint8_t profile, a_uint8_t priority, a_uint8_t *class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_priority_class_get, dev_id, profile, priority, class)

sw_error_t fal_queue_flush( a_uint32_t dev_id, fal_port_t port, a_uint16_t queue_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_flush, dev_id, port, queue_id)

sw_error_t fal_queue_counter_ctrl_get(a_uint32_t dev_id, a_bool_t *cnt_en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_counter_ctrl_get, dev_id, cnt_en)

sw_error_t fal_queue_counter_cleanup(a_uint32_t dev_id, a_uint32_t queue_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_counter_cleanup, dev_id, queue_id)

sw_error_t fal_ucast_priority_class_set( a_uint32_t dev_id, a_uint8_t profile, a_uint8_t priority, a_uint8_t class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_priority_class_set, dev_id, profile, priority, class)

sw_error_t fal_ac_static_threshold_get( a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_static_threshold_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_static_threshold_get, dev_id, obj, cfg)

sw_error_t fal_ucast_queue_base_profile_set( a_uint32_t dev_id, fal_ucast_queue_dest_t *queue_dest, a_uint32_t queue_base, a_uint8_t profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_queue_base_profile_set, dev_id, queue_dest, queue_base, profile)

sw_error_t fal_ac_group_buffer_set( a_uint32_t dev_id, a_uint8_t group_id, fal_ac_group_buffer_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ac_group_buffer_set, dev_id, group_id, cfg)

sw_error_t fal_queue_counter_ctrl_set(a_uint32_t dev_id, a_bool_t cnt_en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_counter_ctrl_set, dev_id, cnt_en)

sw_error_t fal_queue_counter_get( a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_stats_t *info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_counter_get, dev_id, queue_id, info)

sw_error_t fal_qm_enqueue_ctrl_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_enqueue_ctrl_set, dev_id, queue_id, enable)

sw_error_t fal_qm_enqueue_ctrl_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_enqueue_ctrl_get, dev_id, queue_id, enable)

sw_error_t fal_qm_port_source_profile_set(a_uint32_t dev_id, fal_port_t port, a_uint32_t src_profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_port_source_profile_set, dev_id, port, src_profile)

sw_error_t fal_qm_port_source_profile_get(a_uint32_t dev_id, fal_port_t port, a_uint32_t *src_profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_port_source_profile_get, dev_id, port, src_profile)

sw_error_t fal_qm_enqueue_config_set(a_uint32_t dev_id, fal_enqueue_cfg_t *enqueue_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_enqueue_config_set, dev_id, enqueue_cfg)

sw_error_t fal_qm_enqueue_config_get(a_uint32_t dev_id, fal_enqueue_cfg_t *enqueue_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_enqueue_config_get, dev_id, enqueue_cfg)

sw_error_t fal_qm_threshold_reset(a_uint32_t dev_id, a_uint32_t queue_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_threshold_reset, dev_id, queue_id)

sw_error_t
fal_qm_counter_monitor_en_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_en_set, dev_id, enable)

sw_error_t
fal_qm_counter_monitor_en_get(a_uint32_t dev_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_en_get, dev_id, enable)

sw_error_t
fal_qm_counter_monitor_stats_cleanup(a_uint32_t dev_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_stats_cleanup, dev_id)

sw_error_t
fal_qm_counter_monitor_status_get(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t cnt_id, a_bool_t *status)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_status_get, dev_id, type, cnt_id, status)

sw_error_t
fal_qm_counter_monitor_map_set(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_map_set, dev_id, type, map_id, monitor_map)

sw_error_t
fal_qm_counter_monitor_map_get(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_map_get, dev_id, type, map_id, monitor_map)

sw_error_t
fal_qm_counter_monitor_ctrl_set(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_ctrl_set, dev_id, type, cnt_id, monitor_ctrl)

sw_error_t
fal_qm_counter_monitor_ctrl_get(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_ctrl_get, dev_id, type, cnt_id, monitor_ctrl)

sw_error_t
fal_qm_counter_monitor_stats_get(a_uint32_t dev_id, fal_qm_queue_type_t type, a_uint32_t cnt_id, fal_qm_monitor_stats_t *monitor_stats)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_counter_monitor_stats_get, dev_id, type, cnt_id, monitor_stats)

sw_error_t fal_qm_tcont_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_tcont_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_tcont_get, dev_id, queue_id, cfg)

sw_error_t fal_qm_tcont_set(a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_tcont_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_tcont_set, dev_id, queue_id, cfg)

sw_error_t fal_qm_cpucode_enqueue_get(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_cpucode_enqueue_get, dev_id, cpu_code, enable)

sw_error_t fal_qm_cpucode_enqueue_set(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_cpucode_enqueue_set, dev_id, cpu_code, enable)

sw_error_t fal_qm_tcont_stat_get(a_uint32_t dev_id, a_uint32_t tcont_id, fal_queue_tcont_stat_t *stat)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_tcont_stat_get, dev_id, tcont_id, stat)

sw_error_t fal_ucast_queue_ddrq_en_set(a_uint32_t dev_id, a_uint32_t queue_id, fal_ucast_queue_ddrq_en_t *ddrqen)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_queue_ddrq_en_set, dev_id, queue_id, ddrqen)

sw_error_t fal_ucast_queue_ddrq_en_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_ucast_queue_ddrq_en_t *ddrqen)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_queue_ddrq_en_get, dev_id, queue_id, ddrqen)

sw_error_t fal_qm_tcont_stat_set(a_uint32_t dev_id, a_uint32_t tcont_id, fal_queue_tcont_stat_t *stat)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_tcont_stat_set, dev_id, tcont_id, stat)

sw_error_t fal_qm_passthrough_source_profile_set(a_uint32_t dev_id, fal_passthrough_mode_t mode, fal_passthrough_src_profile_t *profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_source_profile_set, dev_id, mode, profile)

sw_error_t fal_qm_passthrough_source_profile_get(a_uint32_t dev_id, fal_passthrough_mode_t mode, fal_passthrough_src_profile_t *profile)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_source_profile_get, dev_id, mode, profile)

sw_error_t fal_qm_passthrough_direct_enqueue_set(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_direct_enqueue_set, dev_id, mode, enable)

sw_error_t fal_qm_passthrough_direct_enqueue_get(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_direct_enqueue_get, dev_id, mode, enable)

sw_error_t fal_qm_passthrough_cpucode_set(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_cpucode_set, dev_id, cpucode)

sw_error_t fal_qm_passthrough_cpucode_get(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_cpucode_get, dev_id, cpucode)

sw_error_t fal_qm_passthrough_cpucode_en_set(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_cpucode_en_set, dev_id, mode, enable)

sw_error_t fal_qm_passthrough_cpucode_en_get(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_passthrough_cpucode_en_get, dev_id, mode, enable)

sw_error_t fal_qm_dequeue_drop_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_dequeue_drop_set, dev_id, queue_id, enable)

sw_error_t fal_qm_dequeue_drop_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_dequeue_drop_get, dev_id, queue_id, enable)

sw_error_t fal_qm_crosschip_bp_qmap_set(a_uint32_t dev_id, a_uint32_t lp_qid, fal_bp_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_crosschip_bp_qmap_set, dev_id, lp_qid, queue_bmp)

sw_error_t fal_qm_crosschip_bp_qmap_get(a_uint32_t dev_id, a_uint32_t lp_qid, fal_bp_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_crosschip_bp_qmap_get, dev_id, lp_qid, queue_bmp)

sw_error_t fal_qm_crosschip_bp_status_get(a_uint32_t dev_id, fal_bp_queue_bmp_t *queue_bmp)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_crosschip_bp_status_get, dev_id, queue_bmp)

sw_error_t fal_qm_ac_drop_state_set(a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_drop_state_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_ac_drop_state_set, dev_id, obj, cfg)

sw_error_t fal_qm_ac_drop_state_get(a_uint32_t dev_id, fal_ac_obj_t *obj, fal_ac_drop_state_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_ac_drop_state_get, dev_id, obj, cfg)
#if !defined(IN_QM_MINI)
sw_error_t fal_ucast_default_hash_get( a_uint32_t dev_id, a_uint8_t *hash_value)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_default_hash_get, dev_id, hash_value)

sw_error_t fal_ucast_default_hash_set( a_uint32_t dev_id, a_uint8_t hash_value)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ucast_default_hash_set, dev_id, hash_value)

sw_error_t fal_mcast_cpu_code_class_set( a_uint32_t dev_id, a_uint8_t cpu_code, a_uint8_t queue_class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mcast_cpu_code_class_set, dev_id, cpu_code, queue_class)

sw_error_t fal_mcast_cpu_code_class_get( a_uint32_t dev_id, a_uint8_t cpu_code, a_uint8_t *queue_class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mcast_cpu_code_class_get, dev_id, cpu_code, queue_class)

#endif
sw_error_t fal_port_mcast_priority_class_set( a_uint32_t dev_id, fal_port_t port, a_uint8_t priority, a_uint8_t queue_class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mcast_priority_class_set, dev_id, port, priority, queue_class)

sw_error_t fal_port_mcast_priority_class_get( a_uint32_t dev_id, fal_port_t port, a_uint8_t priority, a_uint8_t *queue_class)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_mcast_priority_class_get, dev_id, port, priority, queue_class)

sw_error_t fal_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t ucast_enqueue_en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_mcast_enqueue_ctrl_set, dev_id, port_id, ucast_enqueue_en)

sw_error_t fal_qm_mcast_enqueue_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *ucast_enqueue_en)
    DEFINE_FAL_FUNC_ADPT_EXPORT(qm_mcast_enqueue_ctrl_get, dev_id, port_id, ucast_enqueue_en)
