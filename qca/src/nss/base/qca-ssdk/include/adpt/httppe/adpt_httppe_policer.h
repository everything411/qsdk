/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#ifndef __ADPT_HTTPPE_POLICER_H__
#define __ADPT_HTTPPE_POLICER_H__

/* HTTPPE-specific IR read/write declaration */
sw_error_t adpt_httppe_acl_meter_cfg_read_ir(a_uint32_t dev_id, a_uint32_t index,
			__adpt_ppe_acl_meter_cfg_ir_t *ir);

sw_error_t adpt_httppe_acl_meter_cfg_write_ir(a_uint32_t dev_id, a_uint32_t index,
			const __adpt_ppe_acl_meter_cfg_ir_t *ir);

sw_error_t adpt_httppe_policer_time_slot_get(a_uint32_t dev_id,
						a_uint32_t *time_slot);

sw_error_t adpt_httppe_policer_time_slot_set(a_uint32_t dev_id,
						a_uint32_t time_slot);

sw_error_t adpt_httppe_policer_bypass_en_get(a_uint32_t dev_id,
			fal_policer_frame_type_t frame_type, a_bool_t *enable);

sw_error_t adpt_httppe_policer_bypass_en_set(a_uint32_t dev_id,
			fal_policer_frame_type_t frame_type, a_bool_t enable);
#ifndef IN_POLICER_MINI
sw_error_t adpt_httppe_policer_global_counter_get(a_uint32_t dev_id,
		fal_policer_global_counter_t *counter);
#endif

#endif /* __ADPT_HTTPPE_POLICER_H__ */
/**
 * @}
 */
