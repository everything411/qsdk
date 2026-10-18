/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_vsi_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_TBL_ADDRESS + \
				index * VSI_TBL_INC,
				value->val,
				sizeof(union vsi_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vsi_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_TBL_ADDRESS + \
				index * VSI_TBL_INC,
				value->val,
				sizeof(union vsi_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vlan_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VLAN_CNT_TBL_ADDRESS + \
				index * VLAN_CNT_TBL_INC,
				value->val,
				sizeof(union vlan_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_vlan_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vlan_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + VLAN_CNT_TBL_ADDRESS + \
				index * VLAN_CNT_TBL_INC,
				value->val,
				sizeof(union vlan_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vsi_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_COUNTER_TBL_ADDRESS + \
				index * EG_VSI_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_vsi_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_eg_vsi_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_COUNTER_TBL_ADDRESS + \
				index * EG_VSI_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_vsi_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_pre_l2_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_l2_cnt_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PRE_L2_CNT_TBL_ADDRESS + \
				index * PRE_L2_CNT_TBL_INC,
				value->val,
				sizeof(union pre_l2_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_pre_l2_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_l2_cnt_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PRE_L2_CNT_TBL_ADDRESS + \
				index * PRE_L2_CNT_TBL_INC,
				value->val,
				sizeof(union pre_l2_cnt_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_drop_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.rx_drop_byte_cnt_1 << 24 | \
		reg_val.bf.rx_drop_byte_cnt_0;
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_drop_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_drop_byte_cnt_1 = value >> 24;
	reg_val.bf.rx_drop_byte_cnt_0 = value & (((a_uint64_t)1<<24)-1);
	ret = hppe_pre_l2_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.rx_byte_cnt_1 << 32 | \
		reg_val.bf.rx_byte_cnt_0;
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_byte_cnt_1 = value >> 32;
	reg_val.bf.rx_byte_cnt_0 = value & (((a_uint64_t)1<<32)-1);
	ret = hppe_pre_l2_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.rx_pkt_cnt;
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_pkt_cnt = value;
	ret = hppe_pre_l2_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_drop_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.rx_drop_pkt_cnt_1 << 24 | \
		reg_val.bf.rx_drop_pkt_cnt_0;
	return ret;
}

sw_error_t
hppe_pre_l2_cnt_tbl_rx_drop_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_l2_cnt_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_pre_l2_cnt_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_drop_pkt_cnt_1 = value >> 24;
	reg_val.bf.rx_drop_pkt_cnt_0 = value & (((a_uint64_t)1<<24)-1);
	ret = hppe_pre_l2_cnt_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_l3_vp_port_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VP_PORT_TBL_ADDRESS + \
				index * L3_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l3_vp_port_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_l3_vp_port_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_VP_PORT_TBL_ADDRESS + \
				index * L3_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l3_vp_port_tbl_u)/sizeof(a_uint32_t));
}

