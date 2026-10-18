/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_HTTPPE_QM_
#define _ADPT_HTTPPE_QM_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_httppe_queue_flush(a_uint32_t dev_id, fal_port_t port, a_uint16_t queue_id);

sw_error_t
adpt_httppe_ac_dynamic_threshold_get(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg);

sw_error_t
adpt_httppe_ac_dynamic_threshold_set(a_uint32_t dev_id, a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg);

sw_error_t
adpt_httppe_ac_static_threshold_get(a_uint32_t dev_id, fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg);

sw_error_t
adpt_httppe_ac_static_threshold_set(a_uint32_t dev_id, fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg);

sw_error_t
adpt_httppe_ac_ctrl_get(a_uint32_t dev_id, fal_ac_obj_t *obj,fal_ac_ctrl_t *cfg);

sw_error_t
adpt_httppe_ac_ctrl_set(a_uint32_t dev_id, fal_ac_obj_t *obj,fal_ac_ctrl_t *cfg);

sw_error_t
adpt_httppe_ac_queue_group_get(a_uint32_t dev_id, a_uint32_t queue_id, a_uint8_t *group_id);

sw_error_t
adpt_httppe_ac_queue_group_set(a_uint32_t dev_id, a_uint32_t queue_id, a_uint8_t group_id);

sw_error_t
adpt_httppe_ac_group_buffer_get(a_uint32_t dev_id, a_uint8_t group_id, fal_ac_group_buffer_t *cfg);

sw_error_t
adpt_httppe_ac_group_buffer_set(a_uint32_t dev_id, a_uint8_t group_id, fal_ac_group_buffer_t *cfg);

sw_error_t
adpt_httppe_ac_prealloc_buffer_set(a_uint32_t dev_id, fal_ac_obj_t *obj, a_uint16_t num);

sw_error_t
adpt_httppe_ac_prealloc_buffer_get(a_uint32_t dev_id, fal_ac_obj_t *obj, a_uint16_t *num);

sw_error_t
adpt_httppe_queue_counter_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_stats_t *info);

sw_error_t
adpt_httppe_qm_threshold_reset(a_uint32_t dev_id, a_uint32_t queue_id);

sw_error_t
adpt_httppe_queue_counter_ctrl_get(a_uint32_t dev_id, a_bool_t *cnt_en);

sw_error_t
adpt_httppe_queue_counter_ctrl_set(a_uint32_t dev_id, a_bool_t cnt_en);

sw_error_t
adpt_httppe_queue_counter_cleanup(a_uint32_t dev_id, a_uint32_t queue_id);

sw_error_t
adpt_httppe_qm_port_source_profile_set(a_uint32_t dev_id, fal_port_t port,
		a_uint32_t src_profile);

sw_error_t
adpt_httppe_qm_port_source_profile_get(a_uint32_t dev_id, fal_port_t port,
		a_uint32_t *src_profile);

sw_error_t
adpt_httppe_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t ucast_enqueue_en);

sw_error_t
adpt_httppe_qm_mcast_enqueue_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t *ucast_enqueue_en);

 #ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
