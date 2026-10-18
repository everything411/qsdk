/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt.h"
#include "hppe_init.h"
#include "adpt_jhppe_loopback.h"

sw_error_t
adpt_jhppe_lpbk_mib_cpukeep_set(a_uint32_t dev_id, a_bool_t enable)
{
	union lpbk_mib_ctrl_u reg_value;
	sw_error_t rv = SW_OK;
	fal_port_t ppe_loopback_port = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	rv = adpt_jhppe_switch_loopback_port_get(dev_id, &ppe_loopback_port);
	SW_RTN_ON_ERROR(rv);
	ppe_loopback_port = TO_LPBK_ID(ppe_loopback_port);
	rv = jhppe_lpbk_mib_ctrl_get(dev_id, ppe_loopback_port, &reg_value);
	SW_RTN_ON_ERROR(rv);
	if(!enable)
	{
		reg_value.bf.mib_rd_clr = A_TRUE;
	}
	else
	{
		reg_value.bf.mib_rd_clr = A_FALSE;
	}
	rv = jhppe_lpbk_mib_ctrl_set(dev_id, ppe_loopback_port, &reg_value);

	return rv;
}

sw_error_t
adpt_jhppe_lpbk_mib_status_set(a_uint32_t dev_id, a_bool_t enable)
{
	union lpbk_mib_ctrl_u reg_value;
	sw_error_t rv = SW_OK;
	fal_port_t ppe_loopback_port = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	rv = adpt_jhppe_switch_loopback_port_get(dev_id, &ppe_loopback_port);
	SW_RTN_ON_ERROR(rv);
	ppe_loopback_port = TO_LPBK_ID(ppe_loopback_port);
	rv = jhppe_lpbk_mib_ctrl_get(dev_id, ppe_loopback_port, &reg_value);
	SW_RTN_ON_ERROR(rv);
	reg_value.bf.mib_en = enable;
	rv = jhppe_lpbk_mib_ctrl_set(dev_id, ppe_loopback_port, &reg_value);

	return rv;
}

sw_error_t
adpt_jhppe_lpbk_mib_flush_counters(a_uint32_t dev_id, fal_port_t port_id)
{
	union lpbk_mib_ctrl_u reg_value;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_mib_ctrl_get(dev_id, port_id, &reg_value);
	SW_RTN_ON_ERROR(rv);
	reg_value.bf.mib_reset = A_TRUE;
	rv = jhppe_lpbk_mib_ctrl_set(dev_id, port_id, &reg_value);
	SW_RTN_ON_ERROR(rv);
	reg_value.bf.mib_reset = A_FALSE;
	rv = jhppe_lpbk_mib_ctrl_set(dev_id, port_id, &reg_value);

	return rv;
}

sw_error_t
adpt_jhppe_lpbk_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
	fal_mib_info_t * mib_info )
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mib_info);
	memset(mib_info, 0, sizeof(*mib_info));

	port_id = TO_LPBK_ID(port_id);
	jhppe_lpbk_mib_uni_get(dev_id, (a_uint32_t)port_id,
		(union lpbkuni_u *)&mib_info->RxUniCast);
	jhppe_lpbk_mib_multi_get(dev_id, (a_uint32_t)port_id,
		(union lpbkmulti_u *)&mib_info->RxMulti);
	jhppe_lpbk_mib_broad_get(dev_id, (a_uint32_t)port_id,
		(union lpbkbroad_u *)&mib_info->RxBroad);
	jhppe_lpbk_mib_pkt64_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt64_u *)&mib_info->Rx64Byte);
	jhppe_lpbk_mib_pkt65to127_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt65to127_u *)&mib_info->Rx128Byte);
	jhppe_lpbk_mib_pkt128to255_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt128to255_u *)&mib_info->Rx256Byte);
	jhppe_lpbk_mib_pkt256to511_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt256to511_u *)&mib_info->Rx512Byte);
	jhppe_lpbk_mib_pkt512to1023_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt512to1023_u *)&mib_info->Rx1024Byte);
	jhppe_lpbk_mib_pkt1024to1518_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt1024to1518_u *)&mib_info->Rx1518Byte);
	jhppe_lpbk_mib_pkt1519tox_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt1519tox_u *)&mib_info->RxMaxByte);
	jhppe_lpbk_mib_toolong_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkttoolong_u *)&mib_info->RxTooLong);
	jhppe_lpbk_mib_byte_l_get(dev_id, (a_uint32_t)port_id,
		(union lpbkbyte_l_u *)&mib_info->RxGoodByte_lo);
	jhppe_lpbk_mib_byte_h_get(dev_id, (a_uint32_t)port_id,
		(union lpbkbyte_h_u *)&mib_info->RxGoodByte_hi);
	jhppe_lpbk_mib_drop_get(dev_id, (a_uint32_t)port_id,
		(union lpbkdropcounter_u *)&mib_info->Filtered);
	jhppe_lpbk_mib_tooshort_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkttooshort_u *)&mib_info->RxRunt);
	jhppe_lpbk_mib_pkt14to63_get(dev_id, (a_uint32_t)port_id,
		(union lpbkpkt14to63_u *)&mib_info->Rx14To63);
	jhppe_lpbk_mib_toolongbyte_l_get(dev_id, (a_uint32_t)port_id,
		(union lpbktoolongbyte_l_u *)&mib_info->RxTooLongByte_lo);
	jhppe_lpbk_mib_toolongbyte_h_get(dev_id, (a_uint32_t)port_id,
		(union lpbktoolongbyte_h_u *)&mib_info->RxTooLongByte_hi);
	jhppe_lpbk_mib_tooshortbyte_l_get(dev_id, (a_uint32_t)port_id,
		(union lpbktooshortbyte_l_u *)&mib_info->RxRuntByte_lo);
	jhppe_lpbk_mib_tooshortbyte_h_get(dev_id, (a_uint32_t)port_id,
		(union lpbktooshortbyte_h_u *)&mib_info->RxRuntByte_hi);

	return SW_OK;
}
/**
 * @}
 */
