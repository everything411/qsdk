/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_pm_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pm_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PM_COUNTER_TBL_ADDRESS + \
				index * PM_COUNTER_TBL_INC,
				value->val,
				sizeof(union pm_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_pm_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pm_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_POLICER_BASE_ADDR + PM_COUNTER_TBL_ADDRESS + \
				index * PM_COUNTER_TBL_INC,
				value->val,
				sizeof(union pm_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_pm_counter_tbl_bcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bcast_packet_1 << 24 | \
		reg_val.bf.bcast_packet_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_bcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bcast_packet_1 = value >> 24;
	reg_val.bf.bcast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_crc_error_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.crc_error_1 << 24 | \
		reg_val.bf.crc_error_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_crc_error_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.crc_error_1 = value >> 24;
	reg_val.bf.crc_error_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_1024_1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_1024_1518_1 << 24 | \
		reg_val.bf.frame_1024_1518_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_1024_1518_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_1024_1518_1 = value >> 24;
	reg_val.bf.frame_1024_1518_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_128_255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_128_255_1 << 24 | \
		reg_val.bf.frame_128_255_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_128_255_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_128_255_1 = value >> 24;
	reg_val.bf.frame_128_255_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_256_511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_256_511_1 << 24 | \
		reg_val.bf.frame_256_511_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_256_511_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_256_511_1 = value >> 24;
	reg_val.bf.frame_256_511_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_512_1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_512_1023_1 << 24 | \
		reg_val.bf.frame_512_1023_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_512_1023_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_512_1023_1 = value >> 24;
	reg_val.bf.frame_512_1023_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_64_1 << 24 | \
		reg_val.bf.frame_64_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_64_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_64_1 = value >> 24;
	reg_val.bf.frame_64_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_65_127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_65_127_1 << 24 | \
		reg_val.bf.frame_65_127_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_frame_65_127_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_65_127_1 = value >> 24;
	reg_val.bf.frame_65_127_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_mcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mcast_packet_1 << 24 | \
		reg_val.bf.mcast_packet_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_mcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mcast_packet_1 = value >> 24;
	reg_val.bf.mcast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_octets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.octets_1 << 32 | \
		reg_val.bf.octets_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_octets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.octets_1 = value >> 32;
	reg_val.bf.octets_0 = value & (((a_uint64_t)1<<32)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_oversize_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.oversize_1 << 24 | \
		reg_val.bf.oversize_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_oversize_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.oversize_1 = value >> 24;
	reg_val.bf.oversize_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_ucast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ucast_packet_1 << 24 | \
		reg_val.bf.ucast_packet_0;
	return ret;
}

sw_error_t
jhppe_pm_counter_tbl_ucast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ucast_packet_1 = value >> 24;
	reg_val.bf.ucast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_pm_counter_gen_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + PRE_IPO_PM_COUNTER_GEN_ADDRESS + \
				index * PRE_IPO_PM_COUNTER_GEN_INC,
				value->val,
				sizeof(union pre_ipo_pm_counter_gen_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union pre_ipo_pm_counter_gen_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				PRE_IPO_CSR_BASE_ADDR + PRE_IPO_PM_COUNTER_GEN_ADDRESS + \
				index * PRE_IPO_PM_COUNTER_GEN_INC,
				value->val,
				sizeof(union pre_ipo_pm_counter_gen_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_counter_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.counter_id;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_counter_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.counter_id = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_ipmc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipmc;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_ipmc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_incl;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_incl = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_type;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_type = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_tag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tag_fmt;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_tag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tag_fmt = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid_incl;
	return ret;
}

sw_error_t
jhppe_pre_ipo_pm_counter_gen_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union pre_ipo_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pre_ipo_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid_incl = value;
	ret = jhppe_pre_ipo_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_gen_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_PM_COUNTER_GEN_ADDRESS + \
				index * EG_PM_COUNTER_GEN_INC,
				value->val,
				sizeof(union eg_pm_counter_gen_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_pm_counter_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_gen_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_PM_COUNTER_GEN_ADDRESS + \
				index * EG_PM_COUNTER_GEN_INC,
				value->val,
				sizeof(union eg_pm_counter_gen_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_pm_counter_gen_ipmc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ipmc_1 << 1 | \
		reg_val.bf.ipmc_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_ipmc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipmc_1 = value >> 1;
	reg_val.bf.ipmc_0 = value & (((a_uint64_t)1<<1)-1);
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_pcp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_pcp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_pcp_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.pcp_incl;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_pcp_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.pcp_incl = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_type;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_type = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_tag_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tag_fmt;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_tag_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tag_fmt = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_vid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_vid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_vid_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vid_incl;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_gen_vid_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_gen_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_gen_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vid_incl = value;
	ret = jhppe_eg_pm_counter_gen_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_PM_COUNTER_TBL_ADDRESS + \
				index * EG_PM_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_pm_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_pm_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_pm_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_PM_COUNTER_TBL_ADDRESS + \
				index * EG_PM_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_pm_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_pm_counter_tbl_bcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bcast_packet_1 << 24 | \
		reg_val.bf.bcast_packet_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_bcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bcast_packet_1 = value >> 24;
	reg_val.bf.bcast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_1024_1518_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_1024_1518_1 << 24 | \
		reg_val.bf.frame_1024_1518_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_1024_1518_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_1024_1518_1 = value >> 24;
	reg_val.bf.frame_1024_1518_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_128_255_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_128_255_1 << 24 | \
		reg_val.bf.frame_128_255_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_128_255_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_128_255_1 = value >> 24;
	reg_val.bf.frame_128_255_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_256_511_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_256_511_1 << 24 | \
		reg_val.bf.frame_256_511_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_256_511_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_256_511_1 = value >> 24;
	reg_val.bf.frame_256_511_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_512_1023_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_512_1023_1 << 24 | \
		reg_val.bf.frame_512_1023_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_512_1023_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_512_1023_1 = value >> 24;
	reg_val.bf.frame_512_1023_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_64_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_64_1 << 24 | \
		reg_val.bf.frame_64_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_64_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_64_1 = value >> 24;
	reg_val.bf.frame_64_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_65_127_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.frame_65_127_1 << 24 | \
		reg_val.bf.frame_65_127_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_frame_65_127_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.frame_65_127_1 = value >> 24;
	reg_val.bf.frame_65_127_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_mcast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mcast_packet_1 << 24 | \
		reg_val.bf.mcast_packet_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_mcast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mcast_packet_1 = value >> 24;
	reg_val.bf.mcast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_octets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.octets_1 << 32 | \
		reg_val.bf.octets_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_octets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.octets_1 = value >> 32;
	reg_val.bf.octets_0 = value & (((a_uint64_t)1<<32)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_oversize_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.oversize_1 << 24 | \
		reg_val.bf.oversize_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_oversize_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.oversize_1 = value >> 24;
	reg_val.bf.oversize_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_ucast_packet_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ucast_packet_1 << 24 | \
		reg_val.bf.ucast_packet_0;
	return ret;
}

sw_error_t
jhppe_eg_pm_counter_tbl_ucast_packet_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_pm_counter_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_pm_counter_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ucast_packet_1 = value >> 24;
	reg_val.bf.ucast_packet_0 = value & (((a_uint64_t)1<<24)-1);
	ret = jhppe_eg_pm_counter_tbl_set(dev_id, index, &reg_val);
	return ret;
}

