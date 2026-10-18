/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_XGPORTCTRL_REG_H_
#define _HTTPPE_XGPORTCTRL_REG_H_

#define MAC_TX_CONFIGURATION_ADDRESS 0x0
#define MAC_TX_CONFIGURATION_INC     0x4000

struct mac_tx_configuration {
	a_uint32_t  te:1;
	a_uint32_t  ddic:1;
	a_uint32_t  _reserved0:1;
	a_uint32_t  ism:1;
	a_uint32_t  isr:4;
	a_uint32_t  ipg:3;
	a_uint32_t  ifp:1;
	a_uint32_t  _reserved1:4;
	a_uint32_t  jd:1;
	a_uint32_t  _reserved2:3;
	a_uint32_t  sarc:3;
	a_uint32_t  _reserved3:1;
	a_uint32_t  vne:1;
	a_uint32_t  vnm:1;
	a_uint32_t  _reserved4:2;
	a_uint32_t  g9991en:1;
	a_uint32_t  ss:2;
	a_uint32_t  uss:1;
};

union mac_tx_configuration_u {
	a_uint32_t val;
	struct mac_tx_configuration bf;
};

#endif
