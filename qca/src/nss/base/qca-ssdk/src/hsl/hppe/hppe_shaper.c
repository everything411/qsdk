/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
*  Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_shp_slot_cfg_l0_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_l0_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_L0_ADDRESS,
				&value->val);
}

sw_error_t
hppe_shp_slot_cfg_l0_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_l0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_L0_ADDRESS,
				value->val);
}

sw_error_t
hppe_shp_slot_cfg_l1_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_l1_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_L1_ADDRESS,
				&value->val);
}

sw_error_t
hppe_shp_slot_cfg_l1_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_l1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_L1_ADDRESS,
				value->val);
}

sw_error_t
hppe_shp_slot_cfg_port_get(
		a_uint32_t dev_id,
		union shp_slot_cfg_port_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_PORT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_shp_slot_cfg_port_set(
		a_uint32_t dev_id,
		union shp_slot_cfg_port_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + SHP_SLOT_CFG_PORT_ADDRESS,
				value->val);
}

sw_error_t
hppe_l0_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_credit_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_SHP_CREDIT_TBL_ADDRESS + \
				index * L0_SHP_CREDIT_TBL_INC,
				value->val,
				sizeof(union l0_shp_credit_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l0_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_credit_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_SHP_CREDIT_TBL_ADDRESS + \
				index * L0_SHP_CREDIT_TBL_INC,
				value->val,
				sizeof(union l0_shp_credit_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l0_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_SHP_CFG_TBL_ADDRESS + \
				index * L0_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union l0_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l0_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_SHP_CFG_TBL_ADDRESS + \
				index * L0_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union l0_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l0_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value)
{
	if (index >= L0_COMP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_COMP_CFG_TBL_ADDRESS + \
				index * L0_COMP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l0_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l0_comp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L0_COMP_CFG_TBL_ADDRESS + \
				index * L0_COMP_CFG_TBL_INC,
				value->val);
}

sw_error_t
hppe_l1_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_credit_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_SHP_CREDIT_TBL_ADDRESS + \
				index * L1_SHP_CREDIT_TBL_INC,
				value->val,
				sizeof(union l1_shp_credit_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l1_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_credit_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_SHP_CREDIT_TBL_ADDRESS + \
				index * L1_SHP_CREDIT_TBL_INC,
				value->val,
				sizeof(union l1_shp_credit_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l1_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_SHP_CFG_TBL_ADDRESS + \
				index * L1_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union l1_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l1_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_SHP_CFG_TBL_ADDRESS + \
				index * L1_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union l1_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l1_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value)
{
	if (index >= L1_COMP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_COMP_CFG_TBL_ADDRESS + \
				index * L1_COMP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_l1_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l1_comp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + L1_COMP_CFG_TBL_ADDRESS + \
				index * L1_COMP_CFG_TBL_INC,
				value->val);
}

sw_error_t
hppe_psch_shp_sign_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_sign_tbl_u *value)
{
	if (index >= PSCH_SHP_SIGN_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_SIGN_TBL_ADDRESS + \
				index * PSCH_SHP_SIGN_TBL_INC,
				&value->val);
}

sw_error_t
hppe_psch_shp_sign_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_sign_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_SIGN_TBL_ADDRESS + \
				index * PSCH_SHP_SIGN_TBL_INC,
				value->val);
}

sw_error_t
hppe_psch_shp_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_credit_tbl_u *value)
{
	if (index >= PSCH_SHP_CREDIT_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_CREDIT_TBL_ADDRESS + \
				index * PSCH_SHP_CREDIT_TBL_INC,
				&value->val);
}

sw_error_t
hppe_psch_shp_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_credit_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_CREDIT_TBL_ADDRESS + \
				index * PSCH_SHP_CREDIT_TBL_INC,
				value->val);
}

sw_error_t
hppe_psch_shp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_CFG_TBL_ADDRESS + \
				index * PSCH_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union psch_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_psch_shp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_shp_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_SHP_CFG_TBL_ADDRESS + \
				index * PSCH_SHP_CFG_TBL_INC,
				value->val,
				sizeof(union psch_shp_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_psch_comp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_comp_cfg_tbl_u *value)
{
	if (index >= PSCH_COMP_CFG_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_COMP_CFG_TBL_ADDRESS + \
				index * PSCH_COMP_CFG_TBL_INC,
				&value->val);
}

sw_error_t
hppe_psch_comp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union psch_comp_cfg_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + PSCH_COMP_CFG_TBL_ADDRESS + \
				index * PSCH_COMP_CFG_TBL_INC,
				value->val);
}

sw_error_t
hppe_ipg_pre_len_cfg_get(
		a_uint32_t dev_id,
		union ipg_pre_len_cfg_u *value)
{
	return hppe_reg_get(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + IPG_PRE_LEN_CFG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_ipg_pre_len_cfg_set(
		a_uint32_t dev_id,
		union ipg_pre_len_cfg_u *value)
{
	return hppe_reg_set(
				dev_id,
				TRAFFIC_MANAGER_BASE_ADDR + IPG_PRE_LEN_CFG_ADDRESS,
				value->val);
}

