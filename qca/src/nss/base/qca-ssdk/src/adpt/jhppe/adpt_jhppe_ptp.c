/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "fal_port_ctrl.h"
#include "adpt.h"

sw_error_t adpt_jhppe_port_pps_ctrl_set(a_uint32_t dev_id, fal_port_pps_ctrl_t *pps_ctrl)
{
	union xgmac_pps_ctrl_u xgmac_pps_ctrl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pps_ctrl);

	aos_mem_zero(&xgmac_pps_ctrl, sizeof(xgmac_pps_ctrl));

	rv = jhppe_xgmac_pps_ctrl_get(dev_id, &xgmac_pps_ctrl);
	SW_RTN_ON_ERROR(rv);

	xgmac_pps_ctrl.bf.pps_in_sel = pps_ctrl->pps_in_sel;
	xgmac_pps_ctrl.bf.pps_out_sel = pps_ctrl->pps_out_sel;

	return jhppe_xgmac_pps_ctrl_set(dev_id, &xgmac_pps_ctrl);
}

sw_error_t adpt_jhppe_port_pps_ctrl_get(a_uint32_t dev_id, fal_port_pps_ctrl_t *pps_ctrl)
{
	union xgmac_pps_ctrl_u xgmac_pps_ctrl;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pps_ctrl);

	aos_mem_zero(&xgmac_pps_ctrl, sizeof(xgmac_pps_ctrl));

	rv = jhppe_xgmac_pps_ctrl_get(dev_id, &xgmac_pps_ctrl);
	SW_RTN_ON_ERROR(rv);

	pps_ctrl->pps_in_sel = xgmac_pps_ctrl.bf.pps_in_sel;
	pps_ctrl->pps_out_sel = xgmac_pps_ctrl.bf.pps_out_sel;

	return rv;
}
/**
 * @}
 */
