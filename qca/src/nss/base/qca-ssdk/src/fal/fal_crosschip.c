/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "fal_crosschip.h"
#include "adpt.h"

sw_error_t fal_crosschip_bp_mode_set(a_uint32_t dev_id, fal_crosschip_bp_mode_t mode)
	DEFINE_FAL_FUNC_ADPT_EXPORT(crosschip_bp_mode_set, dev_id, mode)

sw_error_t fal_crosschip_bp_mode_get(a_uint32_t dev_id, fal_crosschip_bp_mode_t *mode)
	DEFINE_FAL_FUNC_ADPT_EXPORT(crosschip_bp_mode_get, dev_id, mode)

sw_error_t fal_crosschip_bp_en_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(crosschip_bp_en_set, dev_id, queue_id, enable)

sw_error_t fal_crosschip_bp_en_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
	DEFINE_FAL_FUNC_ADPT_EXPORT(crosschip_bp_en_get, dev_id, queue_id, enable)

sw_error_t fal_crosschip_mdio_master_config(a_uint32_t dev_id, a_bool_t timer_en, a_uint32_t div)
	DEFINE_FAL_FUNC_ADPT_EXPORT(crosschip_mdio_master_config, dev_id, timer_en, div)
/**
 * @}
 */
