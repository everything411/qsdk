/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_QM_REG_H_
#define _APPE_QM_REG_H_

/*[table] PORT_VSI_ENQUEUE_MAP*/
#define PORT_VSI_ENQUEUE_MAP
#define PORT_VSI_ENQUEUE_MAP_ADDRESS 0x29000
#define PORT_VSI_ENQUEUE_MAP_NUM     608
#define PORT_VSI_ENQUEUE_MAP_INC     0x4
#define PORT_VSI_ENQUEUE_MAP_TYPE    REG_TYPE_RW
#define PORT_VSI_ENQUEUE_MAP_DEFAULT 0x0
	/*[field] ENQUEUE_VP*/
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_OFFSET  0
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_LEN     8
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VP_DEFAULT 0x0
	/*[field] ENQUEUE_VALID*/
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_OFFSET  8
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_LEN     1
	#define PORT_VSI_ENQUEUE_MAP_ENQUEUE_VALID_DEFAULT 0x0

struct port_vsi_enqueue_map {
	a_uint32_t  enqueue_vp:8;
	a_uint32_t  enqueue_valid:1;
	a_uint32_t  _reserved0:23;
};

union port_vsi_enqueue_map_u {
	a_uint32_t val;
	struct port_vsi_enqueue_map bf;
};

#endif
