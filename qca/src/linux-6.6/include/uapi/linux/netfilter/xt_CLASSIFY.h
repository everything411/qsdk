/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _XT_CLASSIFY_H
#define _XT_CLASSIFY_H

#include <linux/types.h>

enum {
	XT_SET_PRIORITY = 1 << 0,
	XT_SET_INT_PRI = 1 << 1
};

struct xt_classify_target_info {
	__u32 priority;
	__u8 int_pri;
	__u8 set_flags;
};

#endif /*_XT_CLASSIFY_H */
