/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_qm.h"
#include "fal_uk_if.h"

sw_error_t
fal_ac_ctrl_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_CTRL_SET, dev_id, obj, cfg);
    return rv;
}

sw_error_t
fal_ac_ctrl_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_CTRL_GET, dev_id, obj, cfg);
    return rv;
}

sw_error_t
fal_ac_prealloc_buffer_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t num)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_PRE_BUFFER_SET, dev_id, obj, num);
    return rv;
}

sw_error_t
fal_ac_prealloc_buffer_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t *num)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_PRE_BUFFER_GET, dev_id, obj, num);
    return rv;
}

sw_error_t
fal_ac_queue_group_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t group_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_GROUP_SET, dev_id, queue_id, group_id);
    return rv;
}

sw_error_t
fal_ac_queue_group_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t *group_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_GROUP_GET, dev_id, queue_id, group_id);
    return rv;
}

sw_error_t
fal_ac_static_threshold_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STATIC_THRESH_SET, dev_id, obj, cfg);
    return rv;
}

sw_error_t
fal_ac_static_threshold_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STATIC_THRESH_GET, dev_id, obj, cfg);
    return rv;
}

sw_error_t
fal_ac_dynamic_threshold_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_DYNAMIC_THRESH_SET, dev_id, queue_id, cfg);
    return rv;
}

sw_error_t
fal_ac_dynamic_threshold_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_DYNAMIC_THRESH_GET, dev_id, queue_id, cfg);
    return rv;
}

sw_error_t
fal_ac_group_buffer_set(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_GOURP_BUFFER_SET, dev_id, group_id, cfg);
    return rv;
}

sw_error_t
fal_ac_group_buffer_get(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_GOURP_BUFFER_GET, dev_id, group_id, cfg);
    return rv;
}

sw_error_t
fal_ucast_queue_base_profile_set(
		a_uint32_t dev_id,
		fal_ucast_queue_dest_t *queue_dest,
		a_uint32_t queue_base, a_uint8_t profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_QUEUE_BASE_PROFILE_SET, dev_id, queue_dest, queue_base, profile);
    return rv;
}

sw_error_t
fal_ucast_queue_base_profile_get(
		a_uint32_t dev_id,
		fal_ucast_queue_dest_t *queue_dest,
		a_uint32_t *queue_base, a_uint8_t *profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_QUEUE_BASE_PROFILE_GET, dev_id, queue_dest, queue_base, profile);
    return rv;
}

sw_error_t
fal_ucast_priority_class_set(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t priority,
		a_uint8_t class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_PRIORITY_CLASS_SET, dev_id, profile, priority, class);
    return rv;
}

sw_error_t
fal_ucast_priority_class_get(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t priority,
		a_uint8_t *class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_PRIORITY_CLASS_GET, dev_id, profile, priority, class);
    return rv;
}

sw_error_t
fal_ucast_hash_map_set(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t rss_hash,
		a_int8_t queue_hash)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_HASH_MAP_SET, dev_id, profile, rss_hash, queue_hash);
    return rv;
}

sw_error_t
fal_ucast_hash_map_get(
		a_uint32_t dev_id,
		a_uint8_t profile,
		a_uint8_t rss_hash,
		a_int8_t *queue_hash)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_HASH_MAP_GET, dev_id, profile, rss_hash, queue_hash);
    return rv;
}

sw_error_t
fal_mcast_cpu_code_class_set(
		a_uint32_t dev_id,
		a_uint8_t cpu_code,
		a_uint8_t queue_class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MCAST_CPUCODE_CLASS_SET, dev_id, cpu_code, queue_class);
    return rv;
}

sw_error_t
fal_mcast_cpu_code_class_get(
		a_uint32_t dev_id,
		a_uint8_t cpu_code,
		a_uint8_t *queue_class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MCAST_CPUCODE_CLASS_GET, dev_id, cpu_code, queue_class);
    return rv;
}

sw_error_t
fal_port_mcast_priority_class_set(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint8_t priority,
		a_uint8_t queue_class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MCAST_PRIORITY_CLASS_SET, dev_id, port, priority, queue_class);
    return rv;
}

sw_error_t
fal_port_mcast_priority_class_get(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint8_t priority,
		a_uint8_t *queue_class)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_MCAST_PRIORITY_CLASS_GET, dev_id, port, priority, queue_class);
    return rv;
}

sw_error_t
fal_queue_flush(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint16_t queue_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_FLUSH, dev_id, port, queue_id);
    return rv;
}

sw_error_t
fal_ucast_default_hash_set(
		a_uint32_t dev_id,
		a_uint8_t hash_value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_DFLT_HASH_MAP_SET, dev_id, hash_value);
    return rv;
}

sw_error_t
fal_ucast_default_hash_get(
		a_uint32_t dev_id,
		a_uint8_t *hash_value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_DFLT_HASH_MAP_GET, dev_id, hash_value);
    return rv;
}

sw_error_t
fal_queue_counter_ctrl_set(a_uint32_t dev_id, a_bool_t cnt_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_CNT_CTRL_SET, dev_id, cnt_en);
    return rv;
}

sw_error_t
fal_queue_counter_ctrl_get(a_uint32_t dev_id, a_bool_t *cnt_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_CNT_CTRL_GET, dev_id, cnt_en);
    return rv;
}

sw_error_t
fal_queue_counter_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_queue_stats_t *info)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_CNT_GET, dev_id, queue_id, info);
    return rv;
}

sw_error_t
fal_queue_counter_cleanup(a_uint32_t dev_id, a_uint32_t queue_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QUEUE_CNT_CLEANUP, dev_id, queue_id);
    return rv;
}

sw_error_t
fal_qm_enqueue_ctrl_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_ENQUEUE_CTRL_SET, dev_id, queue_id, enable);
    return rv;
}

sw_error_t
fal_qm_enqueue_ctrl_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_ENQUEUE_CTRL_GET, dev_id, queue_id, enable);
    return rv;
}

sw_error_t
fal_qm_port_source_profile_set(
	a_uint32_t dev_id, fal_port_t port,
	a_uint32_t src_profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_SOURCE_PROFILE_SET, dev_id, port, src_profile);
    return rv;
}

sw_error_t
fal_qm_port_source_profile_get(
	a_uint32_t dev_id, fal_port_t port,
	a_uint32_t *src_profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_SOURCE_PROFILE_GET, dev_id, port, src_profile);
    return rv;
}

sw_error_t
fal_qm_enqueue_config_set(a_uint32_t dev_id,
		fal_enqueue_cfg_t *enqueue_cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_ENQUEUE_CFG_SET, dev_id, enqueue_cfg);
    return rv;
}

sw_error_t
fal_qm_enqueue_config_get(a_uint32_t dev_id,
		fal_enqueue_cfg_t *enqueue_cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_ENQUEUE_CFG_GET, dev_id, enqueue_cfg);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_en_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_EN_SET, dev_id, enable);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_en_get(a_uint32_t dev_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_EN_GET, dev_id, enable);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_stats_cleanup(a_uint32_t dev_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_STATS_CLEANUP, dev_id);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_status_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
	a_uint32_t cnt_id, a_bool_t *status)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_STATUS_GET, dev_id, type, cnt_id, status);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_map_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_MAP_SET, dev_id, type, map_id, monitor_map);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_map_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_MAP_GET, dev_id, type, map_id, monitor_map);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_ctrl_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_CTRL_SET, dev_id, type, cnt_id, monitor_ctrl);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_ctrl_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_CTRL_GET, dev_id, type, cnt_id, monitor_ctrl);
    return rv;
}

sw_error_t
fal_qm_counter_monitor_stats_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_stats_t *monitor_stats)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_COUNTER_MONITOR_STATS_GET, dev_id, type, cnt_id, monitor_stats);
    return rv;
}

sw_error_t fal_qm_tcont_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_queue_tcont_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_TCONT_GET, dev_id, queue_id, cfg);
    return rv;
}

sw_error_t fal_qm_tcont_set(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_queue_tcont_cfg_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_TCONT_SET, dev_id, queue_id, cfg);
    return rv;
}

sw_error_t
fal_qm_cpucode_enqueue_get(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_CPUCODE_ENQUEUE_GET, dev_id, cpu_code, enable);
    return rv;
}

sw_error_t
fal_qm_cpucode_enqueue_set(a_uint32_t dev_id, a_uint32_t cpu_code, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_CPUCODE_ENQUEUE_SET, dev_id, cpu_code, enable);
    return rv;
}

sw_error_t fal_qm_tcont_stat_get(a_uint32_t dev_id, a_uint32_t tcont_id,
				 fal_queue_tcont_stat_t *stat)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_TCONT_STAT_GET, dev_id, tcont_id, stat);
    return rv;
}

sw_error_t
fal_ucast_queue_ddrq_en_set(a_uint32_t dev_id, a_uint32_t queue_id,
			    fal_ucast_queue_ddrq_en_t *ddrqen)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_QUEUE_DDRQ_EN_SET, dev_id, queue_id, ddrqen);
    return rv;
}

sw_error_t
fal_ucast_queue_ddrq_en_get(a_uint32_t dev_id, a_uint32_t queue_id,
			    fal_ucast_queue_ddrq_en_t *ddrqen)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_UCAST_QUEUE_DDRQ_EN_GET, dev_id, queue_id, ddrqen);
    return rv;
}

sw_error_t fal_qm_tcont_stat_set(a_uint32_t dev_id, a_uint32_t tcont_id,
				 fal_queue_tcont_stat_t *stat)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_TCONT_STAT_SET, dev_id, tcont_id, stat);

    return rv;
}

sw_error_t
fal_qm_passthrough_source_profile_set(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		fal_passthrough_src_profile_t *profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_SOURCE_PROFILE_SET, dev_id, mode, profile);
    return rv;
}

sw_error_t
fal_qm_passthrough_source_profile_get(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		fal_passthrough_src_profile_t *profile)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_SOURCE_PROFILE_GET, dev_id, mode, profile);
    return rv;
}

sw_error_t
fal_qm_passthrough_direct_enqueue_set(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_DIRECT_ENQUEUE_SET, dev_id, mode, enable);
    return rv;
}

sw_error_t
fal_qm_passthrough_direct_enqueue_get(a_uint32_t dev_id, fal_passthrough_mode_t mode,
		a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_DIRECT_ENQUEUE_GET, dev_id, mode, enable);
    return rv;
}

sw_error_t
fal_qm_passthrough_cpucode_set(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_CPUCODE_SET, dev_id, cpucode);
    return rv;
}

sw_error_t
fal_qm_passthrough_cpucode_get(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_CPUCODE_GET, dev_id, cpucode);
    return rv;
}

sw_error_t
fal_qm_passthrough_cpucode_en_set(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_CPUCODE_EN_SET, dev_id, mode, enable);
    return rv;
}

sw_error_t
fal_qm_passthrough_cpucode_en_get(a_uint32_t dev_id, fal_passthrough_mode_t mode, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_PASSTHROUGH_CPUCODE_EN_GET, dev_id, mode, enable);
    return rv;
}

sw_error_t
fal_qm_dequeue_drop_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_DEQUEUE_DROP_SET, dev_id, queue_id, enable);
    return rv;
}

sw_error_t
fal_qm_dequeue_drop_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_DEQUEUE_DROP_GET, dev_id, queue_id, enable);
    return rv;
}

sw_error_t
fal_qm_crosschip_bp_qmap_set(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_CROSSCHIP_BP_QMAP_SET, dev_id, lp_qid, queue_bmp);
    return rv;
}

sw_error_t
fal_qm_crosschip_bp_qmap_get(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_CROSSCHIP_BP_QMAP_GET, dev_id, lp_qid, queue_bmp);
    return rv;
}

sw_error_t
fal_qm_crosschip_bp_status_get(a_uint32_t dev_id,
			       fal_bp_queue_bmp_t *queue_bmp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_CROSSCHIP_BP_STATUS_GET, dev_id, queue_bmp);
    return rv;
}

sw_error_t fal_qm_ac_drop_state_set(a_uint32_t dev_id, fal_ac_obj_t *obj,
				    fal_ac_drop_state_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_DROP_STATE_SET, dev_id, obj, cfg);
    return rv;
}

sw_error_t fal_qm_ac_drop_state_get(a_uint32_t dev_id, fal_ac_obj_t *obj,
				    fal_ac_drop_state_t *cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_AC_DROP_STATE_GET, dev_id, obj, cfg);
    return rv;
}

sw_error_t
fal_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
				a_bool_t ucast_enqueue_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_MCAST_ENQUEUE_CTRL_SET, dev_id, port_id, ucast_enqueue_en);
    return rv;
}

sw_error_t
fal_qm_mcast_enqueue_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
				a_bool_t *ucast_enqueue_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_QM_MCAST_ENQUEUE_CTRL_GET, dev_id, port_id, ucast_enqueue_en);
    return rv;
}
