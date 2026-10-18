/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_STP_REG_H_
#define _HPPE_STP_REG_H_

/*[register] CST_STATE*/
#define CST_STATE
#define CST_STATE_ADDRESS 0x100
#define CST_STATE_NUM     9
#define CST_STATE_INC     0x4
#define CST_STATE_TYPE    REG_TYPE_RW
#define CST_STATE_DEFAULT 0x3
	/*[field] PORT_STATE*/
	#define CST_STATE_PORT_STATE
	#define CST_STATE_PORT_STATE_OFFSET  0
	#define CST_STATE_PORT_STATE_LEN     2
	#define CST_STATE_PORT_STATE_DEFAULT 0x3

struct cst_state {
	a_uint32_t  port_state:2;
	a_uint32_t  _reserved0:30;
};

union cst_state_u {
	a_uint32_t val;
	struct cst_state bf;
};

#endif
