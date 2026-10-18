/*
 * Copyright (c) 2016-2017, 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_in_flow_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_FLOW_CNT_TBL_ADDRESS + \
				index * IN_FLOW_CNT_TBL_INC,
				value->val,
				sizeof(union in_flow_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + IN_FLOW_CNT_TBL_ADDRESS + \
				index * IN_FLOW_CNT_TBL_INC,
				value->val,
				sizeof(union in_flow_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_flow_ctrl0_get(
		a_uint32_t dev_id,
		union flow_ctrl0_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_CTRL0_ADDRESS,
				&value->val);
}

sw_error_t
hppe_flow_ctrl0_set(
		a_uint32_t dev_id,
		union flow_ctrl0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_CTRL0_ADDRESS,
				value->val);
}

sw_error_t
hppe_flow_ctrl1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union flow_ctrl1_u *value)
{
	if (index >= FLOW_CTRL1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_CTRL1_ADDRESS + \
				index * FLOW_CTRL1_INC,
				&value->val);
}

sw_error_t
hppe_flow_ctrl1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union flow_ctrl1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_CTRL1_ADDRESS + \
				index * FLOW_CTRL1_INC,
				value->val);
}

sw_error_t
hppe_in_flow_tbl_op_get(
		a_uint32_t dev_id,
		union in_flow_tbl_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_flow_tbl_op_set(
		a_uint32_t dev_id,
		union in_flow_tbl_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_flow_host_tbl_op_get(
		a_uint32_t dev_id,
		union in_flow_host_tbl_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_HOST_TBL_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_flow_host_tbl_op_set(
		a_uint32_t dev_id,
		union in_flow_host_tbl_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_HOST_TBL_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_flow_tbl_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	if (index > IN_FLOW_TBL_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_OP_DATA_ADDRESS +
				IN_FLOW_TBL_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_flow_host_tbl_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	if (index >= FLOW_HOST_TBL_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_HOST_TBL_OP_DATA_ADDRESS +
				FLOW_HOST_TBL_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_in_flow_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union in_flow_tbl_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_flow_host_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union flow_host_tbl_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_HOST_TBL_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_flow_tbl_rd_op_get(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_flow_tbl_rd_op_set(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_flow_host_tbl_rd_op_get(
		a_uint32_t dev_id,
		union in_flow_host_tbl_rd_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_HOST_TBL_RD_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_flow_host_tbl_rd_op_set(
		a_uint32_t dev_id,
		union in_flow_host_tbl_rd_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_HOST_TBL_RD_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_flow_tbl_rd_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	if (index >= IN_FLOW_TBL_RD_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_OP_DATA_ADDRESS +
				IN_FLOW_TBL_RD_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_flow_host_tbl_rd_op_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	if (index >= FLOW_HOST_TBL_RD_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_HOST_TBL_RD_OP_DATA_ADDRESS +
				FLOW_HOST_TBL_RD_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union flow_host_tbl_rd_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_HOST_TBL_RD_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_flow_host_tbl_rd_rslt_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	if (index >= FLOW_HOST_TBL_RD_RSLT_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + FLOW_HOST_TBL_RD_RSLT_DATA_ADDRESS +
				FLOW_HOST_TBL_RD_RSLT_DATA_INC * index,
				value);
}

sw_error_t
hppe_in_flow_3tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_3tuple_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_3TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_3TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_3tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_3tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_3tuple_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_3TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_3TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_3tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_ipv6_3tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_3tuple_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_IPV6_3TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_IPV6_3TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_ipv6_3tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_ipv6_3tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_3tuple_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_IPV6_3TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_IPV6_3TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_ipv6_3tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_ipv6_5tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_5tuple_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_IPV6_5TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_IPV6_5TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_ipv6_5tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_ipv6_5tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_5tuple_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_IPV6_5TUPLE_TBL_ADDRESS + \
				index * IN_FLOW_IPV6_5TUPLE_TBL_INC,
				value->val,
				sizeof(union in_flow_ipv6_5tuple_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_ADDRESS + \
				index * IN_FLOW_TBL_INC,
				value->val,
				sizeof(union in_flow_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_flow_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_ADDRESS + \
				index * IN_FLOW_TBL_INC,
				value->val,
				sizeof(union in_flow_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_flow_tree_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_tree_map_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_TREE_MAP_TBL_ADDRESS + \
				index * EG_FLOW_TREE_MAP_TBL_INC,
				value->val,
				sizeof(union eg_flow_tree_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_flow_tree_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_tree_map_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_TREE_MAP_TBL_ADDRESS + \
				index * EG_FLOW_TREE_MAP_TBL_INC,
				value->val,
				sizeof(union eg_flow_tree_map_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_ipv6_prefix_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_ipv6_prefix_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_IPV6_PREFIX_TBL_ADDRESS + \
				index * EG_IPV6_PREFIX_TBL_INC,
				value->val,
				sizeof(union eg_ipv6_prefix_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_ipv6_prefix_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_ipv6_prefix_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_IPV6_PREFIX_TBL_ADDRESS + \
				index * EG_IPV6_PREFIX_TBL_INC,
				value->val,
				sizeof(union eg_ipv6_prefix_tbl_u)/sizeof(a_uint32_t));
}

#ifdef JHPPE
sw_error_t
hppe_eg_flow_ipv6_iid_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value)
{
	if (index >= EG_FLOW_IPV6_IID_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_IPV6_IID_TBL_ADDRESS + \
				(index/2) * EG_FLOW_IPV6_IID_TBL_INC,
				&value->val);
}

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_IPV6_IID_TBL_ADDRESS + \
				(index/2) * EG_FLOW_IPV6_IID_TBL_INC,
				value->val);
}

#else
sw_error_t
hppe_eg_flow_ipv6_iid_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_IPV6_IID_TBL_ADDRESS + \
				(index/2) * EG_FLOW_IPV6_IID_TBL_INC,
				value->val,
				ARRAY_SIZE(value->val));
}

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_FLOW_IPV6_IID_TBL_ADDRESS + \
				(index/2) * EG_FLOW_IPV6_IID_TBL_INC,
				value->val,
				ARRAY_SIZE(value->val));
}

#endif
sw_error_t
hppe_eg_global_ctrl_get(
		a_uint32_t dev_id,
		union eg_global_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_GLOBAL_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hppe_eg_global_ctrl_set(
		a_uint32_t dev_id,
		union eg_global_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_GLOBAL_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hppe_in_flow_tbl_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.op_rslt;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.valid_cnt;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_op_rslt_flow_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.flow_entry_index;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.cmd_id;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.op_rslt;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.valid_cnt;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_flow_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.flow_entry_index;
	return ret;
}

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union in_flow_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.cmd_id;
	return ret;
}

sw_error_t
hppe_flow_host_tbl_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union flow_host_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_flow_host_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.host_entry_index;
	return ret;
}

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union flow_host_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_flow_host_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.host_entry_index;
	return ret;
}

sw_error_t
hppe_flow_get_common(
		a_uint32_t dev_id,
		a_uint32_t op_mode,
		a_uint32_t *index,
		a_uint32_t *data,
		a_uint32_t num)
{
	union in_flow_tbl_rd_op_u op;
	union in_flow_tbl_rd_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 2;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;
	op.bf.op_host_en = 0;

	rv = hppe_in_flow_tbl_rd_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0) {
		hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_RSLT_DATA_ADDRESS,
				data, num);
		*index = result.bf.flow_entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
	
}

sw_error_t
hppe_flow_flush_common(a_uint32_t dev_id)
{
	union in_flow_tbl_op_u op;
	union in_flow_tbl_op_rslt_u result;
	a_uint32_t i = 0x100 * 50;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 3;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = 0;
	op.bf.op_host_en = 0;

	rv = hppe_in_flow_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0)
		return SW_OK;
	else
		return SW_FAIL;
	
	
}

sw_error_t
hppe_flow_op_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index)
{
	union in_flow_tbl_op_u op;
	union in_flow_tbl_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = op_type;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;
	op.bf.op_host_en = 0;

	rv = hppe_in_flow_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0) {
		*index = result.bf.flow_entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
	
}

sw_error_t
hppe_flow_host_get_common(
		a_uint32_t dev_id,
		a_uint32_t op_mode,
		a_uint32_t *index,
		a_uint32_t *data,
		a_uint32_t num)
{
	union in_flow_tbl_rd_op_u op;
	union in_flow_tbl_rd_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 2;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;
	op.bf.op_host_en = 1;

	rv = hppe_in_flow_tbl_rd_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_rd_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;

	if (result.bf.op_rslt == 0) {
		hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_FLOW_TBL_RD_RSLT_DATA_ADDRESS,
				data, num);
		*index = result.bf.flow_entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
}

sw_error_t
hppe_flow_host_flush_common(a_uint32_t dev_id)
{
	union in_flow_tbl_op_u op;
	union in_flow_tbl_op_rslt_u result;
	a_uint32_t i = 0x100 * 50;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 3;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = 0;
	op.bf.op_host_en = 1;

	rv = hppe_in_flow_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0)
		return SW_OK;
	else
		return SW_FAIL;
}

sw_error_t
hppe_flow_host_op_both_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index)
{
	union in_flow_tbl_op_u op;
	union in_flow_tbl_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = op_type;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;
	op.bf.op_host_en = 1;

	rv = hppe_in_flow_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_in_flow_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0) {
		*index = result.bf.flow_entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
}

sw_error_t
hppe_flow_entry_op(
		a_uint32_t dev_id,
		a_uint32_t op_type, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size, a_bool_t flow_host)
{
	a_uint32_t i = 0;

	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_in_flow_tbl_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}

	if (flow_host)
		return hppe_flow_host_op_both_common(dev_id, op_type, op_mode, index);

	return hppe_flow_op_common(dev_id, op_type, op_mode, index);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size, a_bool_t flow_host)
{
	a_uint32_t i = 0;
	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_in_flow_tbl_rd_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}

	if (flow_host)
		return hppe_flow_host_get_common(dev_id, op_mode, index, entry, entry_size);

	return hppe_flow_get_common(dev_id, op_mode, index, entry, entry_size);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv4_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_entry_host_op_ipv6_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_TRUE);
}

sw_error_t
hppe_flow_host_data_op_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index)
{
	union in_flow_host_tbl_op_u op;
	sw_error_t rv;

	op.bf.hash_block_bitmap = 3;
	op.bf.host_entry_index = *index;

	rv = hppe_in_flow_host_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;

	return SW_OK;
}

sw_error_t
hppe_flow_host_data_rd_op_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index)
{
	union in_flow_host_tbl_rd_op_u op;
	sw_error_t rv;

	op.bf.hash_block_bitmap = 3;
	op.bf.host_entry_index = *index;

	rv = hppe_in_flow_host_tbl_rd_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;

	return SW_OK;
}

sw_error_t
hppe_flow_host_entry_op(
		a_uint32_t dev_id,
		a_uint32_t op_type, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size)
{
	a_uint32_t i = 0;

	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_flow_host_tbl_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}

	return hppe_flow_host_data_op_common(dev_id, op_type, op_mode, index);
}

sw_error_t
hppe_flow_host_ipv4_data_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_flow_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_host_ipv6_data_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_flow_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_host_entry_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size)
{
	a_uint32_t i = 0;

	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_flow_host_tbl_rd_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}

	return hppe_flow_host_data_rd_op_common(dev_id, OP_ADD, op_mode, index);
}

sw_error_t
hppe_flow_host_ipv4_data_rd_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_flow_host_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_host_ipv6_data_rd_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_flow_host_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_host_ipv4_data_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	a_uint32_t i = 0;

	while (i < ARRAY_SIZE(entry->val)) {
		hppe_flow_host_tbl_rd_rslt_data_get(dev_id, i, &(entry->val[i]));
		i++;
	}
	return SW_OK;
}

sw_error_t
hppe_flow_host_ipv6_data_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	a_uint32_t i = 0;

	while (i < ARRAY_SIZE(entry->val)) {
		hppe_flow_host_tbl_rd_rslt_data_get(dev_id, i, &(entry->val[i]));
		i++;
	}
	return SW_OK;
}

sw_error_t
hppe_flow_host_ipv4_data_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_flow_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_host_ipv6_data_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_flow_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_flow_ipv4_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv4_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_5tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_3tuple_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv4_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv4_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_5tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_3tuple_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv4_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv4_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_5tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_5tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

sw_error_t
hppe_flow_ipv6_3tuple_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union in_flow_ipv6_3tuple_tbl_u *entry)
{
	return hppe_flow_entry_get(dev_id, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val), A_FALSE);
}

#ifdef JHPPE
sw_error_t
hppe_eg_global_ctrl_pm_port_bitmap_for_gem_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.pm_port_bitmap_for_gem;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_pm_port_bitmap_for_gem_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pm_port_bitmap_for_gem = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_port_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.port_cnt_byp_xlt_drop_en;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_port_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_cnt_byp_xlt_drop_en = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_prefix_xlt_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.prefix_xlt_en;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_prefix_xlt_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.prefix_xlt_en = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_private_tag_tpid_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.private_tag_tpid;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_private_tag_tpid_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.private_tag_tpid = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_vp_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.vp_cnt_byp_xlt_drop_en;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_vp_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vp_cnt_byp_xlt_drop_en = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_vsi_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.vsi_cnt_byp_xlt_drop_en;
	return ret;
}

sw_error_t
hppe_eg_global_ctrl_vsi_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union eg_global_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_eg_global_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vsi_cnt_byp_xlt_drop_en = value;
	ret = hppe_eg_global_ctrl_set(dev_id, &reg_val);
	return ret;
}

#endif
