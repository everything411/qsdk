/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
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
hppe_fdb_tbl_op_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_op_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_age_timer_get(
		a_uint32_t dev_id,
		union age_timer_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + AGE_TIMER_ADDRESS,
				&value->val);
}

sw_error_t
hppe_age_timer_set(
		a_uint32_t dev_id,
		union age_timer_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + AGE_TIMER_ADDRESS,
				value->val);
}

sw_error_t
hppe_l2_global_conf_get(
		a_uint32_t dev_id,
		union l2_global_conf_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_GLOBAL_CONF_ADDRESS,
				&value->val);
}

sw_error_t
hppe_l2_global_conf_set(
		a_uint32_t dev_id,
		union l2_global_conf_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_GLOBAL_CONF_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_RSLT_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_RSLT_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_RSLT_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_op_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_op_data0_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA0_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_op_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_op_data1_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA1_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_op_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_op_data2_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data2_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_OP_DATA2_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA0_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data0_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA0_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data1_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA1_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data1_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA1_ADDRESS,
				value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data2_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA2_ADDRESS,
				&value->val);
}

sw_error_t
hppe_fdb_tbl_rd_op_data2_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data2_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_RD_OP_DATA2_ADDRESS,
				value->val);
}

sw_error_t
hppe_port_bridge_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_bridge_ctrl_u *value)
{
	if (index >= PORT_BRIDGE_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_BRIDGE_CTRL_ADDRESS + \
				index * PORT_BRIDGE_CTRL_INC,
				&value->val);
}

sw_error_t
hppe_port_bridge_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_bridge_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_BRIDGE_CTRL_ADDRESS + \
				index * PORT_BRIDGE_CTRL_INC,
				value->val);
}

sw_error_t
hppe_port_lrn_limit_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_ctrl_u *value)
{
	if (index >= PORT_LRN_LIMIT_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_LRN_LIMIT_CTRL_ADDRESS + \
				index * PORT_LRN_LIMIT_CTRL_INC,
				&value->val);
}

sw_error_t
hppe_port_lrn_limit_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_LRN_LIMIT_CTRL_ADDRESS + \
				index * PORT_LRN_LIMIT_CTRL_INC,
				value->val);
}

sw_error_t
hppe_port_lrn_limit_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_counter_u *value)
{
	if (index >= PORT_LRN_LIMIT_COUNTER_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + PORT_LRN_LIMIT_COUNTER_ADDRESS + \
				index * PORT_LRN_LIMIT_COUNTER_INC,
				&value->val);
}

sw_error_t
hppe_rfdb_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfdb_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + RFDB_TBL_ADDRESS + \
				index * RFDB_TBL_INC,
				value->val,
				sizeof(union rfdb_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_rfdb_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfdb_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + RFDB_TBL_ADDRESS + \
				index * RFDB_TBL_INC,
				value->val,
				sizeof(union rfdb_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_fdb_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union fdb_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_ADDRESS + \
				index * FDB_TBL_INC,
				value->val,
				sizeof(union fdb_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_fdb_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union fdb_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + FDB_TBL_ADDRESS + \
				index * FDB_TBL_INC,
				value->val,
				sizeof(union fdb_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l2_global_conf_fdb_hash_full_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.fdb_hash_full_fwd_cmd;
	return ret;
}

sw_error_t
hppe_l2_global_conf_fdb_hash_full_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.fdb_hash_full_fwd_cmd = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_failover_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.failover_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_failover_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.failover_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_lrn_ctrl_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.lrn_ctrl_mode;
	return ret;
}

sw_error_t
hppe_l2_global_conf_lrn_ctrl_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_ctrl_mode = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_age_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.age_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_age_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.age_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_1_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.fdb_hash_mode_1;
	return ret;
}

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_1_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.fdb_hash_mode_1 = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.lrn_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_0_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.fdb_hash_mode_0;
	return ret;
}

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_0_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.fdb_hash_mode_0 = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_age_ctrl_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.age_ctrl_mode;
	return ret;
}

sw_error_t
hppe_l2_global_conf_age_ctrl_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.age_ctrl_mode = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_service_code_loop_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.service_code_loop;
	return ret;
}

sw_error_t
hppe_l2_global_conf_service_code_loop_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.service_code_loop = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_rfdb_tbl_mac_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union rfdb_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_rfdb_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.mac_addr_1 << 32 | \
		reg_val.bf.mac_addr_0;
	return ret;
}

sw_error_t
hppe_rfdb_tbl_mac_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union rfdb_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_rfdb_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mac_addr_1 = value >> 32;
	reg_val.bf.mac_addr_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_rfdb_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_rfdb_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union rfdb_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_rfdb_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
hppe_rfdb_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union rfdb_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_rfdb_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = hppe_rfdb_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#ifdef JHPPE
sw_error_t
hppe_l2_global_conf_bc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.bc_pvlan_isol_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_bc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bc_pvlan_isol_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_dot1p_mapper_pcp_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.dot1p_mapper_pcp_mode;
	return ret;
}

sw_error_t
hppe_l2_global_conf_dot1p_mapper_pcp_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dot1p_mapper_pcp_mode = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_dot1p_mapper_vlan_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.dot1p_mapper_vlan_mode;
	return ret;
}

sw_error_t
hppe_l2_global_conf_dot1p_mapper_vlan_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dot1p_mapper_vlan_mode = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_flow_cpy_escape_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.flow_cpy_escape;
	return ret;
}

sw_error_t
hppe_l2_global_conf_flow_cpy_escape_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.flow_cpy_escape = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.ipmc_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_0_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.ipmc_hash_mode_0;
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_0_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc_hash_mode_0 = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_1_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.ipmc_hash_mode_1;
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_1_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc_hash_mode_1 = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_mismatch_act_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.ipmc_mismatch_act;
	return ret;
}

sw_error_t
hppe_l2_global_conf_ipmc_mismatch_act_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc_mismatch_act = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_dmac_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.mc_dmac_check_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_dmac_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mc_dmac_check_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.mc_pvlan_isol_en;
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mc_pvlan_isol_en = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_vlan_match_mode_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	*value = reg_val.bf.mc_vlan_match_mode;
	return ret;
}

sw_error_t
hppe_l2_global_conf_mc_vlan_match_mode_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union l2_global_conf_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l2_global_conf_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mc_vlan_match_mode = value;
	ret = hppe_l2_global_conf_set(dev_id, &reg_val);
	return ret;
}

#endif
