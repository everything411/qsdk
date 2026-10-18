/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_tbl_u *value)
{
	if (index >= SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + SERVICE_TBL_ADDRESS + \
				index * SERVICE_TBL_INC,
				value->val,
				sizeof(union service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + SERVICE_TBL_ADDRESS + \
				index * SERVICE_TBL_INC,
				value->val,
				sizeof(union service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_l2_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l2_service_tbl_u *value)
{
	if (index >= IN_L2_SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + IN_L2_SERVICE_TBL_ADDRESS + \
				index * IN_L2_SERVICE_TBL_INC,
				value->val,
				sizeof(union in_l2_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_l2_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l2_service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + IN_L2_SERVICE_TBL_ADDRESS + \
				index * IN_L2_SERVICE_TBL_INC,
				value->val,
				sizeof(union in_l2_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_service_tbl_u *value)
{
	if (index >= EG_SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;

	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SERVICE_TBL_ADDRESS + \
				index * EG_SERVICE_TBL_INC,
				value->val,
				sizeof(union eg_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SERVICE_TBL_ADDRESS + \
				index * EG_SERVICE_TBL_INC,
				value->val,
				sizeof(union eg_service_tbl_u)/sizeof(a_uint32_t));
}

