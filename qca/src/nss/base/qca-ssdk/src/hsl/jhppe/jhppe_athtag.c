/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_src_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union src_port_mapping_tbl_u *value)
{
	if (index >= SRC_PORT_MAPPING_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRX_BASE_ADDR + SRC_PORT_MAPPING_TBL_ADDRESS + \
				index * SRC_PORT_MAPPING_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_src_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union src_port_mapping_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRX_BASE_ADDR + SRC_PORT_MAPPING_TBL_ADDRESS + \
				index * SRC_PORT_MAPPING_TBL_INC,
				value->val);
}
