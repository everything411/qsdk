/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef JHPPE_PTP_REG_H
#define JHPPE_PTP_REG_H

/*[register] XGMAC_PPS_CTRL*/
#define XGMAC_PPS_CTRL
#define XGMAC_PPS_CTRL_ADDRESS 0x50
#define XGMAC_PPS_CTRL_NUM     1
#define XGMAC_PPS_CTRL_INC     0x4
#define XGMAC_PPS_CTRL_TYPE    REG_TYPE_RW
#define XGMAC_PPS_CTRL_DEFAULT 0x0
	/*[field] PPS_IN_SEL*/
	#define XGMAC_PPS_CTRL_PPS_IN_SEL
	#define XGMAC_PPS_CTRL_PPS_IN_SEL_OFFSET  0
	#define XGMAC_PPS_CTRL_PPS_IN_SEL_LEN     1
	#define XGMAC_PPS_CTRL_PPS_IN_SEL_DEFAULT 0x0
	/*[field] PPS_OUT_SEL*/
	#define XGMAC_PPS_CTRL_PPS_OUT_SEL
	#define XGMAC_PPS_CTRL_PPS_OUT_SEL_OFFSET  1
	#define XGMAC_PPS_CTRL_PPS_OUT_SEL_LEN     3
	#define XGMAC_PPS_CTRL_PPS_OUT_SEL_DEFAULT 0x0

struct xgmac_pps_ctrl {
	a_uint32_t pps_in_sel:1;
	a_uint32_t pps_out_sel:3;
	a_uint32_t _reserved0:28;
};

union xgmac_pps_ctrl_u {
	a_uint32_t val;
	struct xgmac_pps_ctrl bf;
};

#endif
