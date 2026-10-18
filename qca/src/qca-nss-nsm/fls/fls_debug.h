/*
 **************************************************************************
 * Copyright (c) 2023, 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#ifndef __FLS_DEBUG_H
#define __FLS_DEBUG_H

#ifndef FLS_LITE_ENABLE
#include "fls_conn.h"
#include "fls_rfs.h"
#endif

#ifndef FLS_LITE_ENABLE
enum FLS_PFS_CMD_TYPE {
	FLS_PFS_FLUSH,
	FLS_PFS_EVENT,
	FLS_PFS_RESULT,
	FLS_PFS_CLEAN_EVENTS
};

struct fls_cmdinfo {
	uint8_t cmd;
	uint8_t version;
	uint32_t src_ip[4];
	uint32_t dst_ip[4];
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t protocol;
	union fls_data {
		struct packetinfo{
			uint32_t packet_size;
			uint32_t timestamp_sec;
			long timestamp_nsec;
		} fls_packetinfo;
		uint8_t classid;
	} data;
};
#endif

enum fls_debug_level {
	FLS_DEBUG_LEVEL_NONE,
	FLS_DEBUG_LEVEL_ERROR,
	FLS_DEBUG_LEVEL_WARN,
	FLS_DEBUG_LEVEL_INFO,
	FLS_DEBUG_LEVEL_TRACE,
	FLS_DEBUG_LEVEL_MAX
};

#define FLS_ERROR(...) fls_debug_print(FLS_DEBUG_LEVEL_ERROR, __VA_ARGS__)
#define FLS_WARN(...) fls_debug_print(FLS_DEBUG_LEVEL_WARN, __VA_ARGS__)
#define FLS_INFO(...) fls_debug_print(FLS_DEBUG_LEVEL_INFO, __VA_ARGS__)
#define FLS_TRACE(...) fls_debug_print(FLS_DEBUG_LEVEL_TRACE, __VA_ARGS__)

void fls_debug_print(uint32_t level, char *fmt, ...);
#ifndef FLS_LITE_ENABLE
void fls_debug_print_event_info(struct fls_event *event);
void fls_debug_print_conn_info(struct fls_conn *conn);
#endif
void fls_debug_deinit(void);
void fls_debug_init(void);

#endif
