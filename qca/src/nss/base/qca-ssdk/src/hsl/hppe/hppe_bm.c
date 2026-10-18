/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
hppe_port_fc_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_mode_u *value)
{
	if (index >= PORT_FC_MODE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_MODE_ADDRESS + \
				index * PORT_FC_MODE_INC,
				&value->val);
}

sw_error_t
hppe_port_fc_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_fc_mode_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + PORT_FC_MODE_ADDRESS + \
				index * PORT_FC_MODE_INC,
				value->val);
}

sw_error_t
hppe_port_group_id_get(
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
hppe_port_group_id_set(
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
hppe_port_cnt_get(
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
hppe_port_reacted_cnt_get(
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
hppe_shared_group_cfg_get(
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
hppe_shared_group_cfg_set(
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
hppe_port_fc_cfg_get(
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
hppe_port_fc_cfg_set(
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

