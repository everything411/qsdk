/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw_ioctl.h"
#include "fal_pon_pm.h"
#include "fal_uk_if.h"

sw_error_t fal_pon_pm_counter_entry_set(a_uint32_t dev_id, a_uint32_t entry_index,
			fal_direction_t direction, fal_pon_pm_counter_entry_t *entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_PM_COUNTER_ENTRY_SET, dev_id, entry_index, direction, entry);
	return rv;
}

sw_error_t fal_pon_pm_counter_entry_get(a_uint32_t dev_id, a_uint32_t entry_index,
			fal_direction_t direction, fal_pon_pm_counter_entry_t *entry)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_PM_COUNTER_ENTRY_GET, dev_id, entry_index, direction, entry);
	return rv;
}

sw_error_t fal_pon_pm_counter_get(a_uint32_t dev_id, a_uint32_t counter_id,
			  fal_direction_t direction, fal_pon_pm_counter_t *pm_counter)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_PM_COUNTER_GET, dev_id, counter_id, direction, pm_counter);
	return rv;
}
sw_error_t fal_pon_pm_counter_flush(a_uint32_t dev_id, a_uint32_t counter_id,
			fal_direction_t direction)
{
	sw_error_t rv;

	rv = sw_uk_exec(SW_API_PON_PM_COUNTER_FLUSH, dev_id, counter_id, direction);
	return rv;
}
