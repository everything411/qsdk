/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_ATHTAG_REG_H_
#define _JHPPE_ATHTAG_REG_H_

/*[register] SRC_PORT_MAPPING_TBL*/
#define SRC_PORT_MAPPING_TBL
#define SRC_PORT_MAPPING_TBL_ADDRESS 0x800
#define SRC_PORT_MAPPING_TBL_NUM     256
#define SRC_PORT_MAPPING_TBL_INC     0x4
#define SRC_PORT_MAPPING_TBL_TYPE    REG_TYPE_RW
#define SRC_PORT_MAPPING_TBL_DEFAULT 0x0
	/*[field] PORT_VP*/
	#define SRC_PORT_MAPPING_TBL_PORT_VP
	#define SRC_PORT_MAPPING_TBL_PORT_VP_OFFSET  0
	#define SRC_PORT_MAPPING_TBL_PORT_VP_LEN     8
	#define SRC_PORT_MAPPING_TBL_PORT_VP_DEFAULT 0x0
	/*[field] PORT_VALID*/
	#define SRC_PORT_MAPPING_TBL_PORT_VALID
	#define SRC_PORT_MAPPING_TBL_PORT_VALID_OFFSET  8
	#define SRC_PORT_MAPPING_TBL_PORT_VALID_LEN     1
	#define SRC_PORT_MAPPING_TBL_PORT_VALID_DEFAULT 0x0
	/*[field] DST_INFO*/
	#define SRC_PORT_MAPPING_TBL_DST_INFO
	#define SRC_PORT_MAPPING_TBL_DST_INFO_OFFSET  9
	#define SRC_PORT_MAPPING_TBL_DST_INFO_LEN     14
	#define SRC_PORT_MAPPING_TBL_DST_INFO_DEFAULT 0x0
	/*[field] DST_INFO_VALID*/
	#define SRC_PORT_MAPPING_TBL_DST_INFO_VALID
	#define SRC_PORT_MAPPING_TBL_DST_INFO_VALID_OFFSET  23
	#define SRC_PORT_MAPPING_TBL_DST_INFO_VALID_LEN     1
	#define SRC_PORT_MAPPING_TBL_DST_INFO_VALID_DEFAULT 0x0

struct src_port_mapping_tbl {
	a_uint32_t  port_vp:8;
	a_uint32_t  port_valid:1;
	a_uint32_t  dst_info:14;
	a_uint32_t  dst_info_valid:1;
	a_uint32_t  _reserved0:8;
};

union src_port_mapping_tbl_u {
	a_uint32_t val;
	struct src_port_mapping_tbl bf;
};

#endif
