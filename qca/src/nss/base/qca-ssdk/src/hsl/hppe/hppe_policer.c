/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2021-2022, 2024, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_meter_cmpst_length_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union meter_cmpst_length_reg_u *value)
{
	if (index >= METER_CMPST_LENGTH_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + METER_CMPST_LENGTH_REG_ADDRESS + \
				index * METER_CMPST_LENGTH_REG_INC,
				&value->val);
}

sw_error_t
hppe_meter_cmpst_length_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union meter_cmpst_length_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + METER_CMPST_LENGTH_REG_ADDRESS + \
				index * METER_CMPST_LENGTH_REG_INC,
				value->val);
}

sw_error_t
hppe_pc_drop_bypass_reg_set(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_DROP_BYPASS_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_pc_drop_bypass_reg_get(
		a_uint32_t dev_id,
		union pc_drop_bypass_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PC_DROP_BYPASS_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_time_slot_reg_get(
		a_uint32_t dev_id,
		union time_slot_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + TIME_SLOT_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_time_slot_reg_set(
		a_uint32_t dev_id,
		union time_slot_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + TIME_SLOT_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_acl_meter_cfg_tbl_get(
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
hppe_in_acl_meter_cfg_tbl_set(
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
hppe_in_port_meter_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cfg_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CFG_TBL_ADDRESS + \
				index * IN_PORT_METER_CFG_TBL_INC,
				value->val,
				sizeof(union in_port_meter_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_port_meter_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cfg_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CFG_TBL_ADDRESS + \
				index * IN_PORT_METER_CFG_TBL_INC,
				value->val,
				sizeof(union in_port_meter_cfg_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_acl_meter_crdt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_crdt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CRDT_TBL_ADDRESS + \
				index * IN_ACL_METER_CRDT_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_crdt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_acl_meter_crdt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_crdt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CRDT_TBL_ADDRESS + \
				index * IN_ACL_METER_CRDT_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_crdt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_port_meter_crdt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_crdt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CRDT_TBL_ADDRESS + \
				index * IN_PORT_METER_CRDT_TBL_INC,
				value->val,
				sizeof(union in_port_meter_crdt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_port_meter_crdt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_crdt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CRDT_TBL_ADDRESS + \
				index * IN_PORT_METER_CRDT_TBL_INC,
				value->val,
				sizeof(union in_port_meter_crdt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_port_meter_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CNT_TBL_ADDRESS + \
				index * IN_PORT_METER_CNT_TBL_INC,
				value->val,
				sizeof(union in_port_meter_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_acl_meter_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CNT_TBL_ADDRESS + \
				index * IN_ACL_METER_CNT_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_pc_global_cnt_tbl_get(
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
hppe_pc_global_cnt_tbl_set(
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
hppe_in_port_meter_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_port_meter_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_PORT_METER_CNT_TBL_ADDRESS + \
				index * IN_PORT_METER_CNT_TBL_INC,
				value->val,
				sizeof(union in_port_meter_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_acl_meter_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_acl_meter_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_ACL_METER_CNT_TBL_ADDRESS + \
				index * IN_ACL_METER_CNT_TBL_INC,
				value->val,
				sizeof(union in_acl_meter_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_drop_cpu_cnt_tbl_get(
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
hppe_drop_cpu_cnt_tbl_set(
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

sw_error_t
hppe_port_tx_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PORT_TX_DROP_CNT_TBL_ADDRESS + \
				index * PORT_TX_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union port_tx_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_port_tx_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PORT_TX_DROP_CNT_TBL_ADDRESS + \
				index * PORT_TX_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union port_tx_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vp_tx_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VP_TX_DROP_CNT_TBL_ADDRESS + \
				index * VP_TX_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union vp_tx_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vp_tx_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_drop_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VP_TX_DROP_CNT_TBL_ADDRESS + \
				index * VP_TX_DROP_CNT_TBL_INC,
				value->val,
				sizeof(union vp_tx_drop_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vlan_dev_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VLAN_DEV_CNT_TBL_ADDRESS + \
				index * VLAN_DEV_CNT_TBL_INC,
				value->val,
				sizeof(union vlan_dev_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vlan_dev_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_dev_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VLAN_DEV_CNT_TBL_ADDRESS + \
				index * VLAN_DEV_CNT_TBL_INC,
				value->val,
				sizeof(union vlan_dev_cnt_tbl_u)/sizeof(a_uint32_t));
}

