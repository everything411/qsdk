/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/

#ifndef _ATHDBG_UIO_H_
#define _ATHDBG_UIO_H_

#include <linux/stdarg.h>
#include "../core.h"

/* Log levels */
#define ATH12K_LOG_LEVEL_DEBUG 0
#define ATH12K_LOG_LEVEL_INFO  1
#define ATH12K_LOG_LEVEL_WARN  2
#define ATH12K_LOG_LEVEL_ERR   3

struct ath12k_debug_log_entry {
	u64 timestamp;     /* ns */
	u64 debug_mask;    /* which mask triggered */
	u8  log_level;     /* 0=debug,1=info,2=warn,3=err */
	char message[256];
};

struct athdbg_crit_ring_ctx {
	void *buf;
	size_t size_bytes;
	struct ath12k_crit_ring *ring;
	struct ath12k_crit_record *records;
};

struct athdbg_uio_ctx {
	struct ath12k_base *ab;
	struct platform_device *pdev;
	struct device *uio_dev;
	struct uio_info *uio_info;

	/* Debug buffer */
	struct ath12k_debug_log_entry *dbg_buf;
	size_t dbg_size_bytes;
	u32 dbg_idx;

	/* Critical ring */
	struct athdbg_crit_ring_ctx crit;
};

/* Critical ring metadata */
struct ath12k_crit_ring {
	u32 version;
	u32 count;
	u32 write_idx;
	u32 read_idx;
	u32 dropped;
	u32 flags;
} __packed;

/* Critical record structure */
struct ath12k_crit_record {
	u16 radio_id;
	u32 crit_enum;
	u64 ts_nsec;
} __packed;

/* Ring Buffer sizes */
#define ATH12K_UIO_DBG_SIZE_BYTES  (256 * 1024)
#define ATH12K_UIO_CRIT_SIZE_BYTES (128 * 1024)

int athdbg_uio_register(struct ath12k_base *ab);
void athdbg_uio_unregister(struct ath12k_base *ab);
void athdbg_uio_log_info(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_warn(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_err(struct ath12k_base *ab, const char *fmt, va_list args);
void athdbg_uio_log_debug(struct ath12k_base *ab, u64 mask, const char *fmt,
			  va_list args);
void athdbg_uio_critical_failure_trigger(struct ath12k_base *ab, uint32_t crit_enum);

#endif /* _ATHDBG_UIO_H_ */
