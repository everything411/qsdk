/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_eg_hdr_xmit_pri_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value)
{
	if (index >= EG_HDR_XMIT_PRI_MAPPING_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_HDR_XMIT_PRI_MAPPING_ADDRESS + \
				index * EG_HDR_XMIT_PRI_MAPPING_INC,
				&value->val);
}

sw_error_t
httppe_eg_hdr_xmit_pri_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_hdr_xmit_pri_mapping_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_HDR_XMIT_PRI_MAPPING_ADDRESS + \
				index * EG_HDR_XMIT_PRI_MAPPING_INC,
				value->val);
}

sw_error_t
httppe_eg_gen_ctrl_set(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_GEN_CTRL_ADDRESS,
				value->val);
}

sw_error_t
httppe_eg_gen_ctrl_get(
		a_uint32_t dev_id,
		union eg_gen_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_GEN_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
httppe_service_code_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_code_mapping_u *value)
{
	if (index >= SERVICE_CODE_MAPPING_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRX_BASE_ADDR + SERVICE_CODE_MAPPING_ADDRESS + \
				index * SERVICE_CODE_MAPPING_INC,
				&value->val);
}

sw_error_t
httppe_service_code_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union service_code_mapping_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRX_BASE_ADDR + SERVICE_CODE_MAPPING_ADDRESS + \
				index * SERVICE_CODE_MAPPING_INC,
				value->val);
}

sw_error_t
httppe_dst_port_mapping_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dst_port_mapping_tbl_u *value)
{
	if (index >= DST_PORT_MAPPING_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				PRX_BASE_ADDR + DST_PORT_MAPPING_TBL_ADDRESS + \
				index * DST_PORT_MAPPING_TBL_INC,
				&value->val);
}

sw_error_t
httppe_dst_port_mapping_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union dst_port_mapping_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				PRX_BASE_ADDR + DST_PORT_MAPPING_TBL_ADDRESS + \
				index * DST_PORT_MAPPING_TBL_INC,
				value->val);
}

