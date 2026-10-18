/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_SERVCODE_REG_H_
#define _APPE_SERVCODE_REG_H_

/*[table] TL_SERVICE_TBL*/
#define TL_SERVICE_TBL
#define TL_SERVICE_TBL_ADDRESS 0x6000
#define TL_SERVICE_TBL_NUM     256
#define TL_SERVICE_TBL_INC     0x4
#define TL_SERVICE_TBL_TYPE    REG_TYPE_RW
#define TL_SERVICE_TBL_DEFAULT 0x0
	/*[field] BYPASS_BITMAP*/
	#define TL_SERVICE_TBL_BYPASS_BITMAP
	#define TL_SERVICE_TBL_BYPASS_BITMAP_OFFSET  0
	#define TL_SERVICE_TBL_BYPASS_BITMAP_LEN     32
	#define TL_SERVICE_TBL_BYPASS_BITMAP_DEFAULT 0x0

struct tl_service_tbl {
	a_uint32_t  bypass_bitmap:32;
};

union tl_service_tbl_u {
	a_uint32_t val;
	struct tl_service_tbl bf;
};

#endif
