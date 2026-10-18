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
hppe_rt_interface_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rt_interface_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + RT_INTERFACE_CNT_TBL_ADDRESS + \
				index * RT_INTERFACE_CNT_TBL_INC,
				value->val,
				sizeof(union rt_interface_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_rt_interface_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rt_interface_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + RT_INTERFACE_CNT_TBL_ADDRESS + \
				index * RT_INTERFACE_CNT_TBL_INC,
				value->val,
				sizeof(union rt_interface_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_my_mac_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union my_mac_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + MY_MAC_TBL_ADDRESS + \
				index * MY_MAC_TBL_INC,
				value->val,
				sizeof(union my_mac_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_my_mac_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union my_mac_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + MY_MAC_TBL_ADDRESS + \
				index * MY_MAC_TBL_INC,
				value->val,
				sizeof(union my_mac_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l3_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vsi_u *value)
{
	if (index >= L3_VSI_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VSI_ADDRESS + \
				index * L3_VSI_INC,
				&value->val);
}

sw_error_t
hppe_l3_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vsi_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VSI_ADDRESS + \
				index * L3_VSI_INC,
				value->val);
}

sw_error_t
hppe_l3_vsi_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vsi_ext_u *value)
{
	if (index >= L3_VSI_EXT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VSI_EXT_ADDRESS + \
				index * L3_VSI_EXT_INC,
				&value->val);
}

sw_error_t
hppe_l3_vsi_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vsi_ext_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VSI_EXT_ADDRESS + \
				index * L3_VSI_EXT_INC,
				value->val);
}

sw_error_t
hppe_in_pub_ip_addr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_pub_ip_addr_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_PUB_IP_ADDR_TBL_ADDRESS + \
				index * IN_PUB_IP_ADDR_TBL_INC,
				value->val);
}

sw_error_t
hppe_in_pub_ip_addr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_pub_ip_addr_tbl_u *value)
{
	if (index >= IN_PUB_IP_ADDR_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_PUB_IP_ADDR_TBL_ADDRESS + \
				index * IN_PUB_IP_ADDR_TBL_INC,
				&value->val);
}

sw_error_t
hppe_network_route_ip_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_ip_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_IP_ADDRESS + \
				index * NETWORK_ROUTE_IP_INC,
				value->val,
				sizeof(union network_route_ip_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_network_route_ip_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_ip_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_IP_ADDRESS + \
				index * NETWORK_ROUTE_IP_INC,
				value->val,
				sizeof(union network_route_ip_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_network_route_ip_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_ip_ext_u *value)
{
	if (index >= NETWORK_ROUTE_IP_EXT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_IP_EXT_ADDRESS + \
				index * NETWORK_ROUTE_IP_EXT_INC,
				&value->val);
}

sw_error_t
hppe_network_route_ip_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_ip_ext_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_IP_EXT_ADDRESS + \
				index * NETWORK_ROUTE_IP_EXT_INC,
				value->val);
}

sw_error_t
hppe_network_route_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_action_u *value)
{
	if (index >= NETWORK_ROUTE_ACTION_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_ACTION_ADDRESS + \
				index * NETWORK_ROUTE_ACTION_INC,
				&value->val);
}

sw_error_t
hppe_network_route_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union network_route_action_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + NETWORK_ROUTE_ACTION_ADDRESS + \
				index * NETWORK_ROUTE_ACTION_INC,
				value->val);
}

sw_error_t
hppe_l3_route_ctrl_get(
		a_uint32_t dev_id,
		union l3_route_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_ROUTE_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
hppe_l3_route_ctrl_set(
		a_uint32_t dev_id,
		union l3_route_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_ROUTE_CTRL_ADDRESS,
				value->val);
}

sw_error_t
hppe_l3_route_ctrl_ext_get(
		a_uint32_t dev_id,
		union l3_route_ctrl_ext_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_ROUTE_CTRL_EXT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_l3_route_ctrl_ext_set(
		a_uint32_t dev_id,
		union l3_route_ctrl_ext_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_ROUTE_CTRL_EXT_ADDRESS,
				value->val);
}

sw_error_t
hppe_host_tbl_op_get(
		a_uint32_t dev_id,
		union host_tbl_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_host_tbl_op_set(
		a_uint32_t dev_id,
		union host_tbl_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_host_tbl_op_data_set(
		a_uint32_t dev_id, a_uint32_t index,
		a_uint32_t value)
{
	if (index >= HOST_TBL_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_OP_DATA_ADDRESS +
				HOST_TBL_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_host_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union host_tbl_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_host_tbl_rd_op_get(
		a_uint32_t dev_id,
		union host_tbl_rd_op_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_RD_OP_ADDRESS,
				&value->val);
}

sw_error_t
hppe_host_tbl_rd_op_set(
		a_uint32_t dev_id,
		union host_tbl_rd_op_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_RD_OP_ADDRESS,
				value->val);
}

sw_error_t
hppe_host_tbl_rd_op_data_set(
		a_uint32_t dev_id, a_uint32_t index,
		a_uint32_t value)
{

	if (index >= HOST_TBL_RD_OP_DATA_NUM)
		return SW_OUT_OF_RANGE;

	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_RD_OP_DATA_ADDRESS +
				HOST_TBL_RD_OP_DATA_INC * index,
				value);
}

sw_error_t
hppe_host_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union host_tbl_rd_op_rslt_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_RD_OP_RSLT_ADDRESS,
				&value->val);
}

sw_error_t
hppe_in_l3_if_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l3_if_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_L3_IF_TBL_ADDRESS + \
				index * IN_L3_IF_TBL_INC,
				value->val,
				sizeof(union in_l3_if_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_l3_if_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l3_if_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_L3_IF_TBL_ADDRESS + \
				index * IN_L3_IF_TBL_INC,
				value->val,
				sizeof(union in_l3_if_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv6_mcast_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv6_mcast_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV6_MCAST_TBL_ADDRESS + \
				index * HOST_IPV6_MCAST_TBL_INC,
				value->val,
				sizeof(union host_ipv6_mcast_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv6_mcast_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv6_mcast_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV6_MCAST_TBL_ADDRESS + \
				index * HOST_IPV6_MCAST_TBL_INC,
				value->val,
				sizeof(union host_ipv6_mcast_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv4_mcast_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv4_mcast_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV4_MCAST_TBL_ADDRESS + \
				index * HOST_IPV4_MCAST_TBL_INC,
				value->val,
				sizeof(union host_ipv4_mcast_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv4_mcast_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv4_mcast_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV4_MCAST_TBL_ADDRESS + \
				index * HOST_IPV4_MCAST_TBL_INC,
				value->val,
				sizeof(union host_ipv4_mcast_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_ADDRESS + \
				index * HOST_TBL_INC,
				value->val,
				sizeof(union host_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_ADDRESS + \
				index * HOST_TBL_INC,
				value->val,
				sizeof(union host_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv6_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv6_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV6_TBL_ADDRESS + \
				index * HOST_IPV6_TBL_INC,
				value->val,
				sizeof(union host_ipv6_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_ipv6_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union host_ipv6_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_IPV6_TBL_ADDRESS + \
				index * HOST_IPV6_TBL_INC,
				value->val,
				sizeof(union host_ipv6_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_nexthop_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_nexthop_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + IN_NEXTHOP_TBL_ADDRESS + \
				index * IN_NEXTHOP_TBL_INC,
				value->val,
				sizeof(union in_nexthop_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_in_nexthop_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_nexthop_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + IN_NEXTHOP_TBL_ADDRESS + \
				index * IN_NEXTHOP_TBL_INC,
				value->val,
				sizeof(union in_nexthop_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_l3_if_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_l3_if_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_L3_IF_TBL_ADDRESS + \
				index * EG_L3_IF_TBL_INC,
				value->val,
				sizeof(union eg_l3_if_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_l3_if_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_l3_if_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_L3_IF_TBL_ADDRESS + \
				index * EG_L3_IF_TBL_INC,
				value->val,
				sizeof(union eg_l3_if_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_host_tbl_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.op_rslt;
	return ret;
}

sw_error_t
hppe_host_tbl_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.valid_cnt;
	return ret;
}

sw_error_t
hppe_host_tbl_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.entry_index;
	return ret;
}

sw_error_t
hppe_host_tbl_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.cmd_id;
	return ret;
}

sw_error_t
hppe_host_tbl_rd_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.op_rslt;
	return ret;
}

sw_error_t
hppe_host_tbl_rd_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.valid_cnt;
	return ret;
}

sw_error_t
hppe_host_tbl_rd_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.entry_index;
	return ret;
}

sw_error_t
hppe_host_tbl_rd_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union host_tbl_rd_op_rslt_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_host_tbl_rd_op_rslt_get(dev_id, &reg_val);
	*value = reg_val.bf.cmd_id;
	return ret;
}

sw_error_t
hppe_host_op_common(
		a_uint32_t dev_id,
		a_uint32_t op_type,
		a_uint32_t op_mode,
		a_uint32_t *index)
{
	union host_tbl_op_u op;
	union host_tbl_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = op_type;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;

	rv = hppe_host_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_host_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_host_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0) {
		*index = result.bf.entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
	
}

sw_error_t
hppe_host_entry_op(
		a_uint32_t dev_id,
		a_uint8_t op_type, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size)
{
	a_uint32_t i = 0;
	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_host_tbl_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}
	return hppe_host_op_common(dev_id, op_type, op_mode, index);
}

sw_error_t
hppe_host_ipv4_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv4_mcast_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_mcast_add(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_ADD, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv4_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv4_mcast_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_mcast_del(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry)
{
	return hppe_host_entry_op(dev_id, OP_DEL, op_mode, index,
			entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_get_common(
		a_uint32_t dev_id,
		a_uint32_t op_mode,
		a_uint32_t *index,
		a_uint32_t *data,
		a_uint32_t num)
{
	union host_tbl_rd_op_u op;
	union host_tbl_rd_op_rslt_u result;
	a_uint32_t i = 0x100;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 2;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = op_mode;
	op.bf.entry_index = *index;

	rv = hppe_host_tbl_rd_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_host_tbl_rd_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_host_tbl_rd_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0) {
		hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + HOST_TBL_RD_RSLT_DATA_ADDRESS,
				data, num);
		*index = result.bf.entry_index;
		return SW_OK;
	}
	else
		return SW_FAIL;
	
}

sw_error_t
hppe_host_entry_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, a_uint32_t *entry, a_uint32_t entry_size)
{
	a_uint32_t i = 0;
	if (op_mode == HASH_MODE) {
		while (i < entry_size) {
			hppe_host_tbl_rd_op_data_set(dev_id, i, entry[i]);
			i++;
		}
	}
	return hppe_host_get_common(dev_id, op_mode, index, entry, entry_size);
}

sw_error_t
hppe_host_ipv4_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_tbl_u *entry)
{
	return hppe_host_entry_get(dev_id, op_mode, index, entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_tbl_u *entry)
{
	return hppe_host_entry_get(dev_id, op_mode, index, entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv4_mcast_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv4_mcast_tbl_u *entry)
{
	return hppe_host_entry_get(dev_id, op_mode, index, entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_ipv6_mcast_get(
		a_uint32_t dev_id, a_uint32_t op_mode,
		a_uint32_t *index, union host_ipv6_mcast_tbl_u *entry)
{
	return hppe_host_entry_get(dev_id, op_mode, index, entry->val, ARRAY_SIZE(entry->val));
}

sw_error_t
hppe_host_flush_common(a_uint32_t dev_id)
{
	union host_tbl_op_u op;
	union host_tbl_op_rslt_u result;
	a_uint32_t i = 0x100 * 50;
	sw_error_t rv;

	op.bf.byp_rslt_en = 0;
	op.bf.op_type = 3;
	op.bf.hash_block_bitmap = 3;
	op.bf.op_mode = 0;

	rv = hppe_host_tbl_op_set(dev_id, &op);
	if (SW_OK != rv)
		return rv;
	rv = hppe_host_tbl_op_rslt_get(dev_id, &result);
	if (SW_OK != rv)
		return rv;
	while (!result.bf.valid_cnt && --i) {
		hppe_host_tbl_op_rslt_get(dev_id, &result);
	}
	if (i == 0)
		return SW_BUSY;
	if (result.bf.op_rslt == 0)
		return SW_OK;
	else
		return SW_FAIL;
	
	
}

sw_error_t
appe_l3_vp_port_tbl_ipo_vp_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l3_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l3_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipo_vp_profile = value;
	ret = hppe_l3_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l3_vp_port_tbl_ipo_vp_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l3_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_l3_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipo_vp_profile;
	return ret;
}

