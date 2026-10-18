/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_HTTPPE_BM_
#define _ADPT_HTTPPE_BM_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_httppe_port_bufgroup_map_get(a_uint32_t dev_id, fal_port_t port,
		a_uint8_t *group);

sw_error_t
adpt_httppe_port_bufgroup_map_set(a_uint32_t dev_id, fal_port_t port,
		a_uint8_t group);

sw_error_t
adpt_httppe_bm_port_reserved_buffer_get(a_uint32_t dev_id, fal_port_t port,
		a_uint16_t *prealloc_buff, a_uint16_t *react_buff);

sw_error_t
adpt_httppe_bm_port_reserved_buffer_set(a_uint32_t dev_id, fal_port_t port,
		a_uint16_t prealloc_buff, a_uint16_t react_buff);

sw_error_t
adpt_httppe_bm_bufgroup_buffer_get(a_uint32_t dev_id, a_uint8_t group,
		a_uint16_t *buff_num);

sw_error_t
adpt_httppe_bm_bufgroup_buffer_set(a_uint32_t dev_id, a_uint8_t group,
		a_uint16_t buff_num);

sw_error_t
adpt_httppe_bm_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port,
		fal_bm_static_cfg_t *cfg);

sw_error_t
adpt_httppe_bm_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port,
		fal_bm_static_cfg_t *cfg);

sw_error_t
adpt_httppe_bm_port_dynamic_thresh_get(a_uint32_t dev_id, fal_port_t port,
		fal_bm_dynamic_cfg_t *cfg);

sw_error_t
adpt_httppe_bm_port_dynamic_thresh_set(a_uint32_t dev_id, fal_port_t port,
		fal_bm_dynamic_cfg_t *cfg);

sw_error_t
adpt_httppe_bm_port_counter_get(a_uint32_t dev_id, fal_port_t port,
		fal_bm_port_counter_t *counter);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
