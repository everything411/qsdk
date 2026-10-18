/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MPPE_SERVCODE_REG_H_
#define _MPPE_SERVCODE_REG_H_

/*[table] TL_VP_SERVICE_CODE_GEN*/
#define TL_VP_SERVICE_CODE_GEN
#define TL_VP_SERVICE_CODE_GEN_ADDRESS 0x800
#define TL_VP_SERVICE_CODE_GEN_NUM     256
#define TL_VP_SERVICE_CODE_GEN_INC     0x4
#define TL_VP_SERVICE_CODE_GEN_TYPE    REG_TYPE_RW
#define TL_VP_SERVICE_CODE_GEN_DEFAULT 0x0
	/*[field] SERVICE_CODE*/
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_OFFSET  0
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_LEN     8
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_DEFAULT 0x0
	/*[field] SERVICE_CODE_EN*/
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_EN
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_EN_OFFSET  8
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_EN_LEN     1
	#define TL_VP_SERVICE_CODE_GEN_SERVICE_CODE_EN_DEFAULT 0x0

struct tl_vp_service_code_gen {
	a_uint32_t  service_code:8;
	a_uint32_t  service_code_en:1;
	a_uint32_t  _reserved0:23;
};

union tl_vp_service_code_gen_u {
	a_uint32_t val;
	struct tl_vp_service_code_gen bf;
};

#endif
