/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef _ATHDBG_UIO_H_
#define _ATHDBG_UIO_H_

#include <linux/stdarg.h>
#include "../core.h"

#define ATH12K_DEV "host"

int athdbg_uio_register(void);
bool athdbg_uio_check_dump_upload(void);
int athdbg_uio_unregister(void);
void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab,
					 u32 crit_enum);
void athdbg_uio_log_info(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_warn(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_err(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_debug(struct ath12k_base *ab, u64 mask, const char *fmt,
			  va_list args);

struct ath12k_crit_record {
	u64 ts_nsec;
	u32 crit_enum;
	u16 radio_id;
} __packed;

struct athdbg_uio_trace {
	u32 dropped;
};

#endif /* _ATHDBG_UIO_H_ */
