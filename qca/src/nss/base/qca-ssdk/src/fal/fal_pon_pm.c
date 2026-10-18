/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_pon_pm FAL_PON_PM
 * @{
 */
#include "sw.h"
#include "fal_pon_pm.h"
#include "adpt.h"

sw_error_t fal_pon_pm_counter_entry_set(a_uint32_t dev_id, a_uint32_t entry_index,
		fal_direction_t direction, fal_pon_pm_counter_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_pm_counter_entry_set, dev_id, entry_index, direction, entry)

sw_error_t fal_pon_pm_counter_entry_get(a_uint32_t dev_id, a_uint32_t entry_index,
		fal_direction_t direction, fal_pon_pm_counter_entry_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_pm_counter_entry_get, dev_id, entry_index, direction, entry)

sw_error_t fal_pon_pm_counter_get(a_uint32_t dev_id, a_uint32_t counter_id,
		fal_direction_t direction, fal_pon_pm_counter_t *pm_counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_pm_counter_get, dev_id, counter_id, direction, pm_counter)

sw_error_t fal_pon_pm_counter_flush(a_uint32_t dev_id, a_uint32_t counter_id,
		fal_direction_t direction)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pon_pm_counter_flush, dev_id, counter_id, direction)
