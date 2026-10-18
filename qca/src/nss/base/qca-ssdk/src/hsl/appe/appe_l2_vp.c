/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "hsl_reg.h"

sw_error_t
appe_vp_lrn_limit_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_lrn_limit_counter_u *value)
{
	if (index >= VP_LRN_LIMIT_COUNTER_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + VP_LRN_LIMIT_COUNTER_ADDRESS + \
				index * VP_LRN_LIMIT_COUNTER_INC,
				&value->val);
}

sw_error_t
appe_l2_vp_port_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_TBL_ADDRESS + \
				index * L2_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_l2_vp_port_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_VP_PORT_TBL_ADDRESS + \
				index * L2_VP_PORT_TBL_INC,
				value->val,
				sizeof(union l2_vp_port_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_l2_vp_port_tbl_station_move_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.station_move_lrn_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_station_move_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.station_move_lrn_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_policer_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_index;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_policer_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_index = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_station_move_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.station_move_fwd_cmd;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_station_move_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.station_move_fwd_cmd = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_isol_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_isol_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_new_addr_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.new_addr_fwd_cmd;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_new_addr_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.new_addr_fwd_cmd = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_promisc_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.promisc_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_promisc_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.promisc_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_new_addr_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.new_addr_lrn_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_new_addr_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.new_addr_lrn_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_exceed_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_exceed_fwd;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_exceed_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_exceed_fwd = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#ifdef JHPPE
sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_isolation_bitmap_1 << 1 | \
		reg_val.bf.port_isolation_bitmap_0;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_isolation_bitmap_1 = value >> 1;
	reg_val.bf.port_isolation_bitmap_0 = value & (((a_uint64_t)1<<1)-1);
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#else
sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.port_isolation_bitmap;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.port_isolation_bitmap = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}
#endif
sw_error_t
appe_l2_vp_port_tbl_policer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.policer_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_policer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.policer_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.dst_info;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.dst_info = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_cnt;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_cnt = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.lrn_lmt_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_lrn_lmt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.lrn_lmt_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_exception_fmt_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.exception_fmt_ctrl;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_exception_fmt_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.exception_fmt_ctrl = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_app_ctrl_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.app_ctrl_profile_1 << 6 | \
		reg_val.bf.app_ctrl_profile_0;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_app_ctrl_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.app_ctrl_profile_1 = value >> 6;
	reg_val.bf.app_ctrl_profile_0 = value & (((a_uint64_t)1<<6)-1);
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_invalid_vsi_forwarding_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.invalid_vsi_forwarding_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_invalid_vsi_forwarding_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.invalid_vsi_forwarding_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#ifndef JHPPE
sw_error_t
appe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.physical_port;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.physical_port = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}
#else
sw_error_t
appe_l2_vp_port_tbl_isol_profile_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.isol_profile_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_isol_profile_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.isol_profile_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_mirror_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.mirror_en;
	return ret;
}

sw_error_t
appe_l2_vp_port_tbl_mirror_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_vp_port_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_vp_port_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.mirror_en = value;
	ret = appe_l2_vp_port_tbl_set(dev_id, index, &reg_val);
	return ret;
}

#endif
