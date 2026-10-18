/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_GENEVE_REG_H_
#define _APPE_GENEVE_REG_H_

/*[register] TPR_GENEVE_CFG*/
#define TPR_GENEVE_CFG
#define TPR_GENEVE_CFG_ADDRESS 0x4e8
#define TPR_GENEVE_CFG_NUM     1
#define TPR_GENEVE_CFG_INC     0x4
#define TPR_GENEVE_CFG_TYPE    REG_TYPE_RW
#define TPR_GENEVE_CFG_DEFAULT 0x0
	/*[field] UDP_PORT_MAP*/
	#define TPR_GENEVE_CFG_UDP_PORT_MAP
	#define TPR_GENEVE_CFG_UDP_PORT_MAP_OFFSET  0
	#define TPR_GENEVE_CFG_UDP_PORT_MAP_LEN     6
	#define TPR_GENEVE_CFG_UDP_PORT_MAP_DEFAULT 0x0

struct tpr_geneve_cfg {
	a_uint32_t  udp_port_map:6;
	a_uint32_t  _reserved0:26;
};

union tpr_geneve_cfg_u {
	a_uint32_t val;
	struct tpr_geneve_cfg bf;
};

#endif
