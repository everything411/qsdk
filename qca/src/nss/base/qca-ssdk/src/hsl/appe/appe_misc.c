/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
hppe_port_fc_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_STATUS_ADDRESS + \
				index * PORT_FC_STATUS_INC,
				&value->val);
}

sw_error_t
hppe_port_fc_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_status_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_STATUS_ADDRESS + \
				index * PORT_FC_STATUS_INC,
				value->val);
}
