/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "fal_qos.h"
#include "hsl_reg.h"
#include "adpt.h"

sw_error_t
adpt_jhppe_qos_port_pcp_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_pcp_cfg_t *pcp_cfg)
{
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pcp_cfg);

	hppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);

	mru_mtu_ctrl.bf.pcp_qos_mode = pcp_cfg->pcp_mode;
	mru_mtu_ctrl.bf.default_pcp_dei = pcp_cfg->default_pcp_dei;

	return hppe_mru_mtu_ctrl_tbl_set(dev_id, port_id, &mru_mtu_ctrl);
}

sw_error_t
adpt_jhppe_qos_port_pcp_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
			fal_qos_pcp_cfg_t *pcp_cfg)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl;

	memset(&mru_mtu_ctrl, 0, sizeof(mru_mtu_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pcp_cfg);

	rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl);
	if( rv != SW_OK )
		return rv;

	pcp_cfg->pcp_mode = mru_mtu_ctrl.bf.pcp_qos_mode;
	pcp_cfg->default_pcp_dei = mru_mtu_ctrl.bf.default_pcp_dei;

	return SW_OK;
}
/**
 * @}
 */

