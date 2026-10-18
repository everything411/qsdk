/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_htt_reg.h"

sw_error_t
httppe_port_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value)
{
	if (index >= PORT_GROUP_ID_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_GROUP_ID_ADDRESS + \
				index * PORT_GROUP_ID_INC,
				&value->val);
}

sw_error_t
httppe_port_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_group_id_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_GROUP_ID_ADDRESS + \
				index * PORT_GROUP_ID_INC,
				value->val);
}

sw_error_t
httppe_port_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_cnt_u *value)
{
	if (index >= PORT_CNT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_CNT_ADDRESS + \
				index * PORT_CNT_INC,
				&value->val);
}

sw_error_t
httppe_port_reacted_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_reacted_cnt_u *value)
{
	if (index >= PORT_REACTED_CNT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_REACTED_CNT_ADDRESS + \
				index * PORT_REACTED_CNT_INC,
				&value->val);
}

sw_error_t
httppe_shared_group_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value)
{
	if (index >= SHARED_GROUP_CFG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + SHARED_GROUP_CFG_ADDRESS + \
				index * SHARED_GROUP_CFG_INC,
				&value->val);
}

sw_error_t
httppe_shared_group_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union shared_group_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + SHARED_GROUP_CFG_ADDRESS + \
				index * SHARED_GROUP_CFG_INC,
				value->val);
}

sw_error_t
httppe_port_fc_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_CFG_ADDRESS + \
				index * PORT_FC_CFG_INC,
				value->val,
				sizeof(union port_fc_cfg_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_port_fc_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_cfg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_CFG_ADDRESS + \
				index * PORT_FC_CFG_INC,
				value->val,
				sizeof(union port_fc_cfg_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_drop_stat_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PRX_CSR_BASE_ADDR + DROP_STAT_ADDRESS + \
				index * DROP_STAT_INC,
				value->val,
				3);
}

sw_error_t
httppe_port_fc_status_get(
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
httppe_port_fc_status_set(
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
