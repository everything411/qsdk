/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_eg_drop_counter_get(
		a_uint32_t dev_id,
		union eg_drop_counter_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_DROP_COUNTER_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_eg_drop_counter_set(
		a_uint32_t dev_id,
		union eg_drop_counter_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_DROP_COUNTER_ADDRESS,
				value->val);
}

sw_error_t
jhppe_eg_drop_counter_pkt_num_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_drop_counter_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_drop_counter_get(dev_id, &reg_val);
	*value = reg_val.bf.pkt_num;
	return ret;
}

sw_error_t
jhppe_eg_drop_counter_pkt_num_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_drop_counter_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_drop_counter_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pkt_num = value;
	ret = jhppe_eg_drop_counter_set(dev_id, &reg_val);
	return ret;
}

