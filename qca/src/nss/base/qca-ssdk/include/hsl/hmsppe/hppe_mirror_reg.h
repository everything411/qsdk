/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_MIRROR_REG_H_
#define _HPPE_MIRROR_REG_H_

/*[register] MIRROR_ANALYZER*/
#define MIRROR_ANALYZER
#define MIRROR_ANALYZER_ADDRESS 0x40
#define MIRROR_ANALYZER_NUM     1
#define MIRROR_ANALYZER_INC     0x4
#define MIRROR_ANALYZER_TYPE    REG_TYPE_RW
#define MIRROR_ANALYZER_DEFAULT 0x0
	/*[field] IN_ANALYZER_PORT*/
	#define MIRROR_ANALYZER_IN_ANALYZER_PORT
	#define MIRROR_ANALYZER_IN_ANALYZER_PORT_OFFSET  0
	#define MIRROR_ANALYZER_IN_ANALYZER_PORT_LEN     6
	#define MIRROR_ANALYZER_IN_ANALYZER_PORT_DEFAULT 0x0
	/*[field] EG_ANALYZER_PORT*/
	#define MIRROR_ANALYZER_EG_ANALYZER_PORT
	#define MIRROR_ANALYZER_EG_ANALYZER_PORT_OFFSET  8
	#define MIRROR_ANALYZER_EG_ANALYZER_PORT_LEN     6
	#define MIRROR_ANALYZER_EG_ANALYZER_PORT_DEFAULT 0x0

struct mirror_analyzer {
	a_uint32_t  in_analyzer_port:6;
	a_uint32_t  _reserved0:2;
	a_uint32_t  eg_analyzer_port:6;
	a_uint32_t  _reserved1:18;
};

union mirror_analyzer_u {
	a_uint32_t val;
	struct mirror_analyzer bf;
};

/*[register] PORT_MIRROR*/
#define PORT_MIRROR
#define PORT_MIRROR_ADDRESS 0x800
#define PORT_MIRROR_NUM     9
#define PORT_MIRROR_INC     0x4
#define PORT_MIRROR_TYPE    REG_TYPE_RW
#define PORT_MIRROR_DEFAULT 0x0
	/*[field] IN_MIRR_EN*/
	#define PORT_MIRROR_IN_MIRR_EN
	#define PORT_MIRROR_IN_MIRR_EN_OFFSET  0
	#define PORT_MIRROR_IN_MIRR_EN_LEN     1
	#define PORT_MIRROR_IN_MIRR_EN_DEFAULT 0x0
	/*[field] EG_MIRR_EN*/
	#define PORT_MIRROR_EG_MIRR_EN
	#define PORT_MIRROR_EG_MIRR_EN_OFFSET  1
	#define PORT_MIRROR_EG_MIRR_EN_LEN     1
	#define PORT_MIRROR_EG_MIRR_EN_DEFAULT 0x0

struct port_mirror {
	a_uint32_t  in_mirr_en:1;
	a_uint32_t  eg_mirr_en:1;
	a_uint32_t  _reserved0:30;
};

union port_mirror_u {
	a_uint32_t val;
	struct port_mirror bf;
};

#endif
