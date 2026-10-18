/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "adpt.h"
#include "fal_crosschip.h"
#include "hsl_htt_reg.h"

#define CROSSCHIP_UCAST_QUEUE_ID_MAX           256
#define HTTPPE_VCH_ID                          16

sw_error_t
adpt_httppe_crosschip_mdio_master_config(a_uint32_t dev_id, a_bool_t timer_en, a_uint32_t div)
{
#define HTTPPE_MDIO_TIMER_CNT_DEFAULT 0xc8  /* 200 cycles - provides appropriate polling interval */

	union mdio_master_ctrl0_u ctrl_0;
	union mdio_master_ctrl1_u ctrl_1;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	memset(&ctrl_0, 0, sizeof(ctrl_0));
	memset(&ctrl_1, 0, sizeof(ctrl_1));

	rv = httppe_mdio_master_ctrl0_get(dev_id, &ctrl_0);
	SW_RTN_ON_ERROR(rv);

	rv = httppe_mdio_master_ctrl1_get(dev_id, &ctrl_1);
	SW_RTN_ON_ERROR(rv);

	/* Enable the MDIO transmission. */
	ctrl_0.bf.trigger_en = 1;

	/* polling or trigger. */
	ctrl_0.bf.timer_en = timer_en;

	/* In the timer mode, the timer is 250MHZ/div * timer_cnt. */
	ctrl_0.bf.div_factor = div;
	ctrl_1.bf.timer_cnt = HTTPPE_MDIO_TIMER_CNT_DEFAULT;

	/* Preamble bytes. */
	ctrl_0.bf.preamble = 2;

	rv = httppe_mdio_master_ctrl0_set(dev_id, &ctrl_0);
	SW_RTN_ON_ERROR(rv);

	return httppe_mdio_master_ctrl1_set(dev_id, &ctrl_1);
}

sw_error_t
adpt_httppe_crosschip_bp_mode_set(a_uint32_t dev_id, fal_crosschip_bp_mode_t mode)
{
	sw_error_t rv = SW_OK;
	union crosschip_bp_ctrl_u crosschip_bp_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&crosschip_bp_ctrl, 0, sizeof(crosschip_bp_ctrl));

	if (mode > FAL_CROSSCHIP_BP_QUEUE)
		return SW_BAD_PARAM;

	rv = httppe_crosschip_bp_ctrl_get(dev_id, &crosschip_bp_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (mode == FAL_CROSSCHIP_BP_DISABLE)
		crosschip_bp_ctrl.bf.crosschip_bp_en = 0;
	else if (mode == FAL_CROSSCHIP_BP_EDMA) {
		crosschip_bp_ctrl.bf.crosschip_bp_en = 1;
		crosschip_bp_ctrl.bf.crosschip_bp_mode = 1;
	} else if (mode == FAL_CROSSCHIP_BP_QUEUE) {
		crosschip_bp_ctrl.bf.crosschip_bp_en = 1;
		crosschip_bp_ctrl.bf.crosschip_bp_mode = 0;
	}

	return httppe_crosschip_bp_ctrl_set(dev_id, &crosschip_bp_ctrl);
}

sw_error_t
adpt_httppe_crosschip_bp_mode_get(a_uint32_t dev_id, fal_crosschip_bp_mode_t *mode)
{
	sw_error_t rv = SW_OK;
	union crosschip_bp_ctrl_u crosschip_bp_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mode);
	memset(&crosschip_bp_ctrl, 0, sizeof(crosschip_bp_ctrl));

	rv = httppe_crosschip_bp_ctrl_get(dev_id, &crosschip_bp_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (crosschip_bp_ctrl.bf.crosschip_bp_en == 0)
		*mode = FAL_CROSSCHIP_BP_DISABLE;
	else if ((crosschip_bp_ctrl.bf.crosschip_bp_en == 1) && (crosschip_bp_ctrl.bf.crosschip_bp_mode == 1))
		*mode = FAL_CROSSCHIP_BP_EDMA;
	else if ((crosschip_bp_ctrl.bf.crosschip_bp_en == 1) && (crosschip_bp_ctrl.bf.crosschip_bp_mode == 0))
		*mode = FAL_CROSSCHIP_BP_QUEUE;

	return SW_OK;
}

sw_error_t
adpt_httppe_crosschip_bp_en_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union crosschip_queue_ctrl_u crosschip_queue_ctrl;
	a_uint32_t reg_idx = 0, bit_pos = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&crosschip_queue_ctrl, 0, sizeof(crosschip_queue_ctrl));

	if (queue_id >= CROSSCHIP_UCAST_QUEUE_ID_MAX) {
		return SW_BAD_PARAM;
	}

	reg_idx = queue_id / 32;	/* register index: 0 to 7 */
	bit_pos = queue_id % 32;	/* bit position within register: 0 to 31 */

	rv = httppe_crosschip_queue_ctrl_get(dev_id, reg_idx, &crosschip_queue_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (bit_pos < 16) {
		if (enable) {
			crosschip_queue_ctrl.bf.crosschip_queue_en_0 |= BIT(bit_pos);
		} else {
			crosschip_queue_ctrl.bf.crosschip_queue_en_0 &= ~BIT(bit_pos);
		}
	} else {
		if (enable) {
			crosschip_queue_ctrl.bf.crosschip_queue_en_1 |= BIT(bit_pos - 16);
		} else {
			crosschip_queue_ctrl.bf.crosschip_queue_en_1 &= ~BIT(bit_pos - 16);
		}
	}

	return httppe_crosschip_queue_ctrl_set(dev_id, reg_idx, &crosschip_queue_ctrl);
}

sw_error_t
adpt_httppe_crosschip_bp_en_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union crosschip_queue_ctrl_u crosschip_queue_ctrl;
	a_uint32_t reg_idx, bit_pos;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);
	memset(&crosschip_queue_ctrl, 0, sizeof(crosschip_queue_ctrl));

	if (queue_id >= CROSSCHIP_UCAST_QUEUE_ID_MAX) {
		return SW_BAD_PARAM;
	}

	reg_idx = queue_id / 32;    /* register index: 0 to 7 */
	bit_pos = queue_id % 32;    /* bit position within register: 0 to 31 */

	rv = httppe_crosschip_queue_ctrl_get(dev_id, reg_idx, &crosschip_queue_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (bit_pos < 16) {
		*enable = (crosschip_queue_ctrl.bf.crosschip_queue_en_0 & (BIT(bit_pos))) ? 1 : 0;
	} else {
		*enable = (crosschip_queue_ctrl.bf.crosschip_queue_en_1 & (BIT(bit_pos - 16))) ? 1 : 0;
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_vch_bp_config_set(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union vp_bp_thres_u vp_bp_thres;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&vp_bp_thres, 0, sizeof(vp_bp_thres));

	if (vch_id >= HTTPPE_VCH_ID) {
		return SW_BAD_PARAM;
	}

	rv = httppe_vp_bp_thres_get(dev_id, vch_id, &vp_bp_thres);
	SW_RTN_ON_ERROR(rv);

	vp_bp_thres.bf.vp_bp_en = enable;
	return httppe_vp_bp_thres_set(dev_id, vch_id, &vp_bp_thres);
}

sw_error_t
adpt_httppe_vch_bp_config_get(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union vp_bp_thres_u vp_bp_thres;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);
	memset(&vp_bp_thres, 0, sizeof(vp_bp_thres));

	if (vch_id >= HTTPPE_VCH_ID) {
		return SW_BAD_PARAM;
	}

	rv = httppe_vp_bp_thres_get(dev_id, vch_id, &vp_bp_thres);
	SW_RTN_ON_ERROR(rv);

	*enable = vp_bp_thres.bf.vp_bp_en;

	return SW_OK;
}

sw_error_t
adpt_httppe_vch_bp_thres_set(a_uint32_t dev_id, a_uint32_t vch_id,
		a_uint16_t on_thres, a_uint16_t off_thres)
{
	sw_error_t rv = SW_OK;
	union vp_bp_thres_u vp_bp_thres;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&vp_bp_thres, 0, sizeof(vp_bp_thres));

	if (vch_id >= HTTPPE_VCH_ID) {
		return SW_BAD_PARAM;
	}

	rv = httppe_vp_bp_thres_get(dev_id, vch_id, &vp_bp_thres);
	SW_RTN_ON_ERROR(rv);

	vp_bp_thres.bf.vp_bp_xoff = off_thres;
	vp_bp_thres.bf.vp_bp_xon = on_thres;

	return httppe_vp_bp_thres_set(dev_id, vch_id, &vp_bp_thres);
}

sw_error_t
adpt_httppe_vch_bp_thres_get(a_uint32_t dev_id, a_uint32_t vch_id,
		a_uint16_t *on_thres, a_uint16_t *off_thres)
{
	sw_error_t rv = SW_OK;
	union vp_bp_thres_u vp_bp_thres;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(on_thres);
	ADPT_NULL_POINT_CHECK(off_thres);
	memset(&vp_bp_thres, 0, sizeof(vp_bp_thres));

	if (vch_id >= HTTPPE_VCH_ID) {
		return SW_BAD_PARAM;
	}

	rv = httppe_vp_bp_thres_get(dev_id, vch_id, &vp_bp_thres);
	SW_RTN_ON_ERROR(rv);

	*off_thres = vp_bp_thres.bf.vp_bp_xoff;
	*on_thres = vp_bp_thres.bf.vp_bp_xon;

	return SW_OK;
}

sw_error_t
adpt_httppe_vch_bp_stats_get(a_uint32_t dev_id, a_uint32_t vch_id, a_uint32_t *stats)
{
	sw_error_t rv = SW_OK;
	union vp_bp_cnt_u vp_bp_cnt;
	a_uint32_t reg_idx = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(stats);

	memset(&vp_bp_cnt, 0, sizeof(vp_bp_cnt));

	if (vch_id >= HTTPPE_VCH_ID) {
		return SW_BAD_PARAM;
	}

	if (vch_id <= 7) {
		reg_idx = vch_id;
		rv = httppe_vp_bp_cnt_get(dev_id, reg_idx, &vp_bp_cnt);
		SW_RTN_ON_ERROR(rv);
		*stats = vp_bp_cnt.bf.vp_bp_cnt_0;
	} else {
		reg_idx = vch_id - 8;
		rv = httppe_vp_bp_cnt_get(dev_id, reg_idx, &vp_bp_cnt);
		SW_RTN_ON_ERROR(rv);
		*stats = vp_bp_cnt.bf.vp_bp_cnt_1;
	}

	return SW_OK;
}

sw_error_t adpt_httppe_crosschip_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);
	ADPT_NULL_POINT_CHECK(p_adpt_api);

	p_adpt_api->adpt_crosschip_bp_mode_set = adpt_httppe_crosschip_bp_mode_set;
	p_adpt_api->adpt_crosschip_bp_mode_get = adpt_httppe_crosschip_bp_mode_get;
	p_adpt_api->adpt_crosschip_bp_en_set = adpt_httppe_crosschip_bp_en_set;
	p_adpt_api->adpt_crosschip_bp_en_get = adpt_httppe_crosschip_bp_en_get;
	p_adpt_api->adpt_vch_bp_config_set = adpt_httppe_vch_bp_config_set;
	p_adpt_api->adpt_vch_bp_config_get = adpt_httppe_vch_bp_config_get;
	p_adpt_api->adpt_vch_bp_thres_set = adpt_httppe_vch_bp_thres_set;
	p_adpt_api->adpt_vch_bp_thres_get = adpt_httppe_vch_bp_thres_get;
	p_adpt_api->adpt_vch_bp_stats_get = adpt_httppe_vch_bp_stats_get;
	p_adpt_api->adpt_crosschip_mdio_master_config = adpt_httppe_crosschip_mdio_master_config;

	return SW_OK;
}

/**
 * @}
 */
