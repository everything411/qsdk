/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_FLOW_H_
#define _ADPT_JHPPE_FLOW_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t adpt_jhppe_flow_key_get(a_uint32_t dev_id,
				   fal_flow_protocol_type_t key_type,
				   fal_flow_key_t *flow_key);

sw_error_t adpt_jhppe_flow_key_set(a_uint32_t dev_id,
				   fal_flow_protocol_type_t key_type,
				   fal_flow_key_t *flow_key);

sw_error_t adpt_jhppe_flow_sampling_id_get(a_uint32_t dev_id,
					   a_uint32_t flow_index,
					   a_uint32_t *sampling_id);

sw_error_t adpt_jhppe_flow_sampling_id_set(a_uint32_t dev_id,
					   a_uint32_t flow_index,
					   a_uint32_t sampling_id);

sw_error_t adpt_jhppe_flow_gro_en_get(a_uint32_t dev_id,
				      a_uint32_t flow_index,
				      a_bool_t *enable);

sw_error_t adpt_jhppe_flow_gro_en_set(a_uint32_t dev_id,
				      a_uint32_t flow_index,
				      a_bool_t enable);

sw_error_t adpt_jhppe_flow_app_entry_add(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry);

sw_error_t adpt_jhppe_flow_app_entry_get(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry);

sw_error_t adpt_jhppe_flow_app_entry_del(a_uint32_t dev_id,
					 fal_flow_app_entry_t *entry);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
