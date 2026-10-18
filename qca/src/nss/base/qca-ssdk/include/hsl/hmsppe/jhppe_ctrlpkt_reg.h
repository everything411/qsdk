/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_CTRLPKT_REG_H_
#define _JHPPE_CTRLPKT_REG_H_

/*[table] EG_CPU_CODE_TBL*/
#define EG_CPU_CODE_TBL
#define EG_CPU_CODE_TBL_ADDRESS 0x28400
#define EG_CPU_CODE_TBL_NUM     256
#define EG_CPU_CODE_TBL_INC     0x4
#define EG_CPU_CODE_TBL_TYPE    REG_TYPE_RW
#define EG_CPU_CODE_TBL_DEFAULT 0x0
	/*[field] CPU_CODE_DISABLE*/
	#define EG_CPU_CODE_TBL_CPU_CODE_DISABLE
	#define EG_CPU_CODE_TBL_CPU_CODE_DISABLE_OFFSET  0
	#define EG_CPU_CODE_TBL_CPU_CODE_DISABLE_LEN     1
	#define EG_CPU_CODE_TBL_CPU_CODE_DISABLE_DEFAULT 0x0
	/*[field] EDIT_ENABLE*/
	#define EG_CPU_CODE_TBL_EDIT_ENABLE
	#define EG_CPU_CODE_TBL_EDIT_ENABLE_OFFSET  1
	#define EG_CPU_CODE_TBL_EDIT_ENABLE_LEN     1
	#define EG_CPU_CODE_TBL_EDIT_ENABLE_DEFAULT 0x0

struct eg_cpu_code_tbl {
	a_uint32_t  cpu_code_disable:1;
	a_uint32_t  edit_enable:1;
	a_uint32_t  _reserved0:30;
};

union eg_cpu_code_tbl_u {
	a_uint32_t val;
	struct eg_cpu_code_tbl bf;
};

#endif
