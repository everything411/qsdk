/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_COUNTER_REG_H_
#define _JHPPE_COUNTER_REG_H_

/*[register] EG_DROP_COUNTER*/
#define EG_DROP_COUNTER
#define EG_DROP_COUNTER_ADDRESS 0x124
#define EG_DROP_COUNTER_NUM     1
#define EG_DROP_COUNTER_INC     0x4
#define EG_DROP_COUNTER_TYPE    REG_TYPE_RW
#define EG_DROP_COUNTER_DEFAULT 0x0
	/*[field] PKT_NUM*/
	#define EG_DROP_COUNTER_PKT_NUM
	#define EG_DROP_COUNTER_PKT_NUM_OFFSET  0
	#define EG_DROP_COUNTER_PKT_NUM_LEN     32
	#define EG_DROP_COUNTER_PKT_NUM_DEFAULT 0x0

struct eg_drop_counter {
	a_uint32_t  pkt_num:32;
};

union eg_drop_counter_u {
	a_uint32_t val;
	struct eg_drop_counter bf;
};

#endif
