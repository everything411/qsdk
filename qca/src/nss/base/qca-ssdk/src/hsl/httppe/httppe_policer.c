/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_pc_drop_bypass_reg_get(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_DROP_BYPASS_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_pc_drop_bypass_reg_set(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_DROP_BYPASS_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_time_slot_reg_get(
		a_uint32_t dev_id,
		union time_slot_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + TIME_SLOT_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_time_slot_reg_set(
		a_uint32_t dev_id,
		union time_slot_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + TIME_SLOT_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_in_acl_meter_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CFG_TBL_ADDRESS + \
				index * IN_ACL_METER_CFG_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_in_acl_meter_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CFG_TBL_ADDRESS + \
				index * IN_ACL_METER_CFG_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_pc_global_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pc_global_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_GLOBAL_CNT_TBL_ADDRESS + \
				index * PC_GLOBAL_CNT_TBL_INC,
				value->val,
				sizeof(union pc_global_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_pc_global_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pc_global_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_GLOBAL_CNT_TBL_ADDRESS + \
				index * PC_GLOBAL_CNT_TBL_INC,
				value->val,
				sizeof(union pc_global_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_drop_cpu_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cpu_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + DROP_CPU_CNT_TBL_ADDRESS + \
				index * DROP_CPU_CNT_TBL_INC,
				value->val,
				sizeof(union drop_cpu_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_drop_cpu_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cpu_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + DROP_CPU_CNT_TBL_ADDRESS + \
				index * DROP_CPU_CNT_TBL_INC,
				value->val,
				sizeof(union drop_cpu_cnt_tbl_u)/sizeof(a_uint32_t));
}

