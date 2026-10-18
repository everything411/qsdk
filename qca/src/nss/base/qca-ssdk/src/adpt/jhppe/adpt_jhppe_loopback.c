/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_phy.h"
#include "hsl_port_prop.h"
#include "hppe_init.h"
#include "adpt.h"
#include "adpt_hppe.h"
#include "adpt_jhppe_loopback.h"

sw_error_t
adpt_jhppe_switch_loopback_port_get(a_uint32_t dev_id, fal_port_t *port_id)
{
	adpt_ppe_type_t chip_type = adpt_ppe_type_get(dev_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_id);

	switch (chip_type) {
		case HMSPPE_TYPE:
		case JHPPE_TYPE:
			*port_id = PHYSICAL_LPBK_PORT_ID;
			break;
		default:
			return SW_NOT_SUPPORTED;
	}
	return SW_OK;
}

sw_error_t
adpt_jhppe_switch_port_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_loopback_config_t *loopback_cfg)
{
	sw_error_t rv = SW_OK;
	union lpbk_enable_u loopback_cfg_tbl;
	union lpbk_pps_ctrl_u loopback_rate_ctrl_tbl;
	union port_bridge_ctrl_u port_bridge_ctrl;
	a_uint32_t physical_port = 0;
	a_uint32_t freq = 0, threshold = 0;

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;

	memset(&loopback_cfg_tbl, 0, sizeof(loopback_cfg_tbl));
	memset(&loopback_rate_ctrl_tbl, 0, sizeof(loopback_rate_ctrl_tbl));
	memset(&port_bridge_ctrl, 0, sizeof(port_bridge_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(loopback_cfg);

	physical_port = port_id;
	rv = hppe_port_bridge_ctrl_get(dev_id, physical_port, &port_bridge_ctrl);
	SW_RTN_ON_ERROR (rv);
	port_bridge_ctrl.bf.txmac_en = loopback_cfg->enable;

	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_pps_ctrl_get(dev_id, port_id, &loopback_rate_ctrl_tbl);
	SW_RTN_ON_ERROR (rv);

	if (adpt_ppe_type_get(dev_id) == HMSPPE_TYPE)
		freq = ADPT_HMSPPE_FREQUENCY;
	else if (adpt_ppe_type_get(dev_id) == JHPPE_TYPE)
		freq = ADPT_JHPPE_FREQUENCY;

	if (freq) {
		if (loopback_cfg->loopback_rate == 0)
			return SW_BAD_PARAM;
		threshold = (freq + loopback_cfg->loopback_rate / 2)
			/ loopback_cfg->loopback_rate;
		loopback_rate_ctrl_tbl.bf.lpbk_pps_threshold = threshold;
	}

	rv = jhppe_lpbk_enable_get(dev_id, port_id, &loopback_cfg_tbl);
	SW_RTN_ON_ERROR (rv);
	loopback_cfg_tbl.bf.lpbk_en = loopback_cfg->enable;
	loopback_cfg_tbl.bf.crc_strip_en = loopback_cfg->crc_stripped;

	if (loopback_cfg->enable == A_TRUE) {
		rv = jhppe_lpbk_pps_ctrl_set(dev_id, port_id, &loopback_rate_ctrl_tbl);
		SW_RTN_ON_ERROR (rv);
		rv = jhppe_lpbk_enable_set(dev_id, port_id, &loopback_cfg_tbl);
		SW_RTN_ON_ERROR (rv);
		msleep(100);
		rv = hppe_port_bridge_ctrl_set(dev_id, physical_port,
			&port_bridge_ctrl);
		SW_RTN_ON_ERROR (rv);
	} else {
		rv = hppe_port_bridge_ctrl_set(dev_id, physical_port,
			&port_bridge_ctrl);
		SW_RTN_ON_ERROR (rv);
		msleep(100);
		rv = jhppe_lpbk_pps_ctrl_set(dev_id, port_id, &loopback_rate_ctrl_tbl);
		SW_RTN_ON_ERROR (rv);
		rv = jhppe_lpbk_enable_set(dev_id, port_id, &loopback_cfg_tbl);
		SW_RTN_ON_ERROR (rv);
	}

	return rv;
}

sw_error_t
adpt_jhppe_switch_port_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_loopback_config_t *loopback_cfg)
{
	sw_error_t rv = SW_OK;
	union lpbk_enable_u loopback_cfg_tbl;
	union lpbk_pps_ctrl_u loopback_rate_ctrl_tbl;

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;

	memset(&loopback_cfg_tbl, 0, sizeof(loopback_cfg_tbl));
	memset(&loopback_rate_ctrl_tbl, 0, sizeof(loopback_rate_ctrl_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(loopback_cfg);

	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_enable_get(dev_id, port_id, &loopback_cfg_tbl);
	SW_RTN_ON_ERROR (rv);
	rv = jhppe_lpbk_pps_ctrl_get(dev_id, port_id, &loopback_rate_ctrl_tbl);
	SW_RTN_ON_ERROR (rv);

	loopback_cfg->enable = loopback_cfg_tbl.bf.lpbk_en;
	loopback_cfg->crc_stripped = loopback_cfg_tbl.bf.crc_strip_en;
	if (adpt_ppe_type_get(dev_id) == HMSPPE_TYPE) {
		 loopback_cfg->loopback_rate =
			ADPT_HMSPPE_FREQUENCY / loopback_rate_ctrl_tbl.bf.lpbk_pps_threshold;
	} else if (adpt_ppe_type_get(dev_id) == JHPPE_TYPE) {
		loopback_cfg->loopback_rate =
			ADPT_JHPPE_FREQUENCY / loopback_rate_ctrl_tbl.bf.lpbk_pps_threshold;
	}

	return rv;
}

sw_error_t
adpt_jhppe_switch_port_loopback_flowctrl_set(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union lpbk_enable_u loopback_cfg_tbl;

	memset(&loopback_cfg_tbl, 0, sizeof(loopback_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;

	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_enable_get(dev_id, port_id, &loopback_cfg_tbl);
	SW_RTN_ON_ERROR (rv);
	loopback_cfg_tbl.bf.flowctrl_en = enable;
	rv = jhppe_lpbk_enable_set(dev_id, port_id, &loopback_cfg_tbl);

	return rv;
}

sw_error_t
adpt_jhppe_switch_port_loopback_flowctrl_get(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union lpbk_enable_u loopback_cfg_tbl;

	memset(&loopback_cfg_tbl, 0, sizeof(loopback_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;

	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_enable_get(dev_id, port_id, &loopback_cfg_tbl);
	SW_RTN_ON_ERROR (rv);
	*enable = loopback_cfg_tbl.bf.flowctrl_en;

	return rv;
}

sw_error_t
adpt_jhppe_lpbk_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t *max_frame)
{
	sw_error_t rv = SW_OK;
	union lpbk_mac_junmo_size_u lpbk_mac_junmo_size;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(max_frame);

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;

	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_mac_junmo_size_get(dev_id, port_id, &lpbk_mac_junmo_size);
	SW_RTN_ON_ERROR(rv);
	*max_frame = lpbk_mac_junmo_size.bf.lpbk_mac_jumbo_size;

	return rv;
}

sw_error_t
adpt_jhppe_lpbk_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t max_frame)
{
	sw_error_t rv = SW_OK;
	union lpbk_mac_junmo_size_u lpbk_mac_junmo_size;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&lpbk_mac_junmo_size, 0, sizeof(lpbk_mac_junmo_size));

	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_FALSE)
		return SW_NOT_SUPPORTED;
	port_id = TO_LPBK_ID(port_id);
	rv = jhppe_lpbk_mac_junmo_size_get(dev_id, port_id, &lpbk_mac_junmo_size);
	SW_RTN_ON_ERROR(rv);
	lpbk_mac_junmo_size.bf.lpbk_mac_jumbo_size = max_frame;
	rv = jhppe_lpbk_mac_junmo_size_set(dev_id, port_id, &lpbk_mac_junmo_size);

	return rv;
}
/**
 * @}
 */
