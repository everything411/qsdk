/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "hsl_reg.h"

sw_error_t
appe_qos_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + QOS_MAPPING_TBL_ADDRESS + \
				index * QOS_MAPPING_TBL_INC,
				value->val,
				2);
}

sw_error_t
appe_qos_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union qos_mapping_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + QOS_MAPPING_TBL_ADDRESS + \
				index * QOS_MAPPING_TBL_INC,
				value->val,
				2);
}

#if defined(MPPE)
sw_error_t
mppe_qos_mapping_tbl_flow_policer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *policer_en,
		a_uint32_t *policer_index)
{
	union qos_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_qos_mapping_tbl_get(dev_id, index, &reg_val);
	*policer_en = reg_val.bf.flow_policy_valid;
	*policer_index = reg_val.bf.flow_policy_id;
	return ret;
}

sw_error_t
mppe_qos_mapping_tbl_flow_policer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t policer_en,
		a_uint32_t policer_index)
{
	union qos_mapping_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_qos_mapping_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.flow_policy_valid = policer_en;
	reg_val.bf.flow_policy_id = policer_index;
	ret = appe_qos_mapping_tbl_set(dev_id, index, &reg_val);
	return ret;
}
#endif

