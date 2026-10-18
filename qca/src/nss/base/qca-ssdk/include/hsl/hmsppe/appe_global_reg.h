/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_GLOBAL_REG_H_
#define _APPE_GLOBAL_REG_H_

/*[register] TX_BUFF_THRSH*/
#define TX_BUFF_THRSH
#define TX_BUFF_THRSH_ADDRESS 0x6100
#define TX_BUFF_THRSH_NUM     9
#define TX_BUFF_THRSH_INC     0x4
#define TX_BUFF_THRSH_TYPE    REG_TYPE_RW
#define TX_BUFF_THRSH_DEFAULT 0x203
	/*[field] XOFF*/
	#define TX_BUFF_THRSH_XOFF
	#define TX_BUFF_THRSH_XOFF_OFFSET  0
	#define TX_BUFF_THRSH_XOFF_LEN     8
	#define TX_BUFF_THRSH_XOFF_DEFAULT 0x3
	/*[field] XON*/
	#define TX_BUFF_THRSH_XON
	#define TX_BUFF_THRSH_XON_OFFSET  8
	#define TX_BUFF_THRSH_XON_LEN     8
	#define TX_BUFF_THRSH_XON_DEFAULT 0x2

struct tx_buff_thrsh {
	a_uint32_t  xoff:8;
	a_uint32_t  xon:8;
	a_uint32_t  _reserved0:16;
};

union tx_buff_thrsh_u {
	a_uint32_t val;
	struct tx_buff_thrsh bf;
};

#endif
