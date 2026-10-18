/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_QM_H_
#define _ADPT_JHPPE_QM_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_qm_ddrq_counter_get(a_uint32_t dev_id,
			       a_uint32_t queue_id, fal_queue_stats_t *info);

sw_error_t
adpt_jhppe_qm_tcont_get(a_uint32_t dev_id,
			a_uint32_t queue_id,
			fal_queue_tcont_cfg_t *cfg);

sw_error_t
adpt_jhppe_qm_tcont_set(a_uint32_t dev_id,
			a_uint32_t queue_id,
			fal_queue_tcont_cfg_t *cfg);

sw_error_t
adpt_jhppe_qm_tcont_stat_get(a_uint32_t dev_id,
			     a_uint32_t tcont_id,
			     fal_queue_tcont_stat_t *stat);

sw_error_t
adpt_jhppe_qm_tcont_stat_set(a_uint32_t dev_id,
			     a_uint32_t tcont_id,
			     fal_queue_tcont_stat_t *stat);

sw_error_t
adpt_jhppe_qm_cpucode_enqueue_get(a_uint32_t dev_id,
				  a_uint32_t cpu_code,
				  a_bool_t *enable);

sw_error_t
adpt_jhppe_qm_cpucode_enqueue_set(a_uint32_t dev_id,
				  a_uint32_t cpu_code,
				  a_bool_t enable);

sw_error_t
adpt_jhppe_qm_counter_monitor_en_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
adpt_jhppe_qm_counter_monitor_en_get(a_uint32_t dev_id, a_bool_t *enable);

sw_error_t
adpt_jhppe_qm_counter_monitor_stats_cleanup(a_uint32_t dev_id);

sw_error_t
adpt_jhppe_qm_counter_monitor_status_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, a_bool_t *status);

sw_error_t
adpt_jhppe_qm_counter_monitor_map_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map);

sw_error_t
adpt_jhppe_qm_counter_monitor_map_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t map_id, fal_qm_monitor_map_t *monitor_map);

sw_error_t
adpt_jhppe_qm_counter_monitor_ctrl_set(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl);

sw_error_t
adpt_jhppe_qm_counter_monitor_ctrl_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_ctrl_t *monitor_ctrl);

sw_error_t
adpt_jhppe_qm_counter_monitor_stats_get(a_uint32_t dev_id, fal_qm_queue_type_t type,
		a_uint32_t cnt_id, fal_qm_monitor_stats_t *monitor_stats);

sw_error_t
adpt_jhppe_qm_enqueue_servcode_config_set(a_uint32_t dev_id,
					  fal_enqueue_cfg_t *enqueue_cfg);

sw_error_t
adpt_jhppe_qm_enqueue_servcode_config_get(a_uint32_t dev_id,
					  fal_enqueue_cfg_t *enqueue_cfg);

sw_error_t
adpt_jhppe_ucast_queue_ddrq_en_set(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ucast_queue_ddrq_en_t *ddrqen);

sw_error_t
adpt_jhppe_ucast_queue_ddrq_en_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ucast_queue_ddrq_en_t *ddrqen);

sw_error_t
adpt_jhppe_qm_passthrough_source_profile_set(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, fal_passthrough_src_profile_t *profile);

sw_error_t
adpt_jhppe_qm_passthrough_source_profile_get(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, fal_passthrough_src_profile_t *profile);

sw_error_t
adpt_jhppe_qm_passthrough_direct_enqueue_set(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, a_bool_t enable);

sw_error_t
adpt_jhppe_qm_passthrough_direct_enqueue_get(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, a_bool_t *enable);

sw_error_t
adpt_jhppe_qm_passthrough_cpucode_set(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode);

sw_error_t
adpt_jhppe_qm_passthrough_cpucode_get(a_uint32_t dev_id, fal_passthrough_cpucode_t *cpucode);

sw_error_t
adpt_jhppe_qm_passthrough_cpucode_en_set(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, a_bool_t enable);

sw_error_t
adpt_jhppe_qm_passthrough_cpucode_en_get(a_uint32_t dev_id,
		fal_passthrough_mode_t mode, a_bool_t *enable);
sw_error_t
adpt_jhppe_qm_crosschip_bp_qmap_set(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp);
sw_error_t
adpt_jhppe_qm_crosschip_bp_qmap_get(a_uint32_t dev_id, a_uint32_t lp_qid,
			     fal_bp_queue_bmp_t *queue_bmp);
sw_error_t
adpt_jhppe_qm_crosschip_bp_status_get(a_uint32_t dev_id,
			       fal_bp_queue_bmp_t *queue_bmp);
sw_error_t
adpt_jhppe_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t ucast_enqueue_en);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
