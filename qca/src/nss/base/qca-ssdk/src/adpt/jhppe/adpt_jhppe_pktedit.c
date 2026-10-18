/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"
#include "shared_func.h"

sw_error_t
adpt_jhppe_pktedit_en_set(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t enable)
{
	return jhppe_eg_cpu_code_tbl_edit_enable_set(dev_id, cpucode, enable);
}

sw_error_t
adpt_jhppe_pktedit_en_get(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t *enable)
{
	return jhppe_eg_cpu_code_tbl_edit_enable_get(dev_id, cpucode, enable);
}

sw_error_t
adpt_jhppe_rxdesc_cpucode_en_set(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t enable)
{
	return jhppe_eg_cpu_code_tbl_cpu_code_disable_set(dev_id, cpucode, !enable);
}

sw_error_t
adpt_jhppe_rxdesc_cpucode_en_get(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t val = 0;

	rv = jhppe_eg_cpu_code_tbl_cpu_code_disable_get(dev_id, cpucode, &val);
	SW_RTN_ON_ERROR(rv);

	*enable = !val;

	return rv;
}
/**
 * @}
 */

