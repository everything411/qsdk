/*
 **************************************************************************
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef FLS_RFS_H
#define FLS_RFS_H

#include <linux/cdev.h>
#include <linux/relay.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include "fls_def_sensor.h"

#define FLS_RFS_NAME "fls"
#define FLS_RFS_SAMPLES_MAX 10
#define FLS_RFS_WINDOWS_MAX 3
#define FLS_RFS_EVENTS_LIMIT 5
#define FLS_RFS_TELEMETRY_DATA_TYPE 2
#define FLS_RFS_WRITE_DELAY msecs_to_jiffies(50)

enum FLS_PROTOCOL_TYPE
{
    UDP,
    TCP
};

struct fls_rfs {
	struct rchan *rfschan;
	struct rchan_buf *rbuf;
	struct dentry *de;
};


enum fls_rfs_event_types {
	FLS_RFS_EVENT_TYPE_DEF,
	FLS_RFS_EVENT_TYPE_XL, /* X large window event type */
	FLS_RFS_EVENT_TYPE_XXL, /* XL large window event type */

};

struct fls_def_event_window {
	uint32_t orig_packets;
	uint32_t orig_bytes;
	uint32_t orig_bytes_min;
	uint32_t orig_bytes_max;
	uint64_t orig_delta_sum;
	uint64_t orig_delta_min;
	uint64_t orig_delta_max;
	uint32_t orig_bursts;
	uint32_t orig_burst_sz_sum;
	uint32_t orig_burst_sz_min;
	uint32_t orig_burst_sz_max;
	uint64_t orig_burst_dur_sum;
	uint64_t orig_burst_dur_min;
	uint64_t orig_burst_dur_max;

	uint32_t ret_packets;
	uint32_t ret_bytes;
	uint32_t ret_bytes_min;
	uint32_t ret_bytes_max;
	uint64_t ret_delta_sum;
	uint64_t ret_delta_min;
	uint64_t ret_delta_max;
	uint32_t ret_bursts;
	uint32_t ret_burst_sz_sum;
	uint32_t ret_burst_sz_min;
	uint32_t ret_burst_sz_max;
	uint64_t ret_burst_dur_sum;
	uint64_t ret_burst_dur_min;
	uint64_t ret_burst_dur_max;
};

struct fls_def_event_sample
{
	struct fls_def_event_window window[FLS_RFS_WINDOWS_MAX];
};

struct fls_def_event {
	uint32_t window_length[FLS_RFS_WINDOWS_MAX];
	uint32_t sample_count;
	struct fls_def_event_sample samples[FLS_RFS_SAMPLES_MAX];
};

struct fls_event {
	uint8_t event_type;
	uint8_t dir;

	uint8_t ip_version;
	uint8_t protocol;

	uint16_t orig_src_port;
	uint16_t orig_dest_port;
	uint32_t orig_src_ip[4];
	uint32_t orig_dest_ip[4];

	uint16_t ret_src_port;
	uint16_t ret_dest_port;
	uint32_t ret_src_ip[4];
	uint32_t ret_dest_ip[4];

	ktime_t timestamp;

	union {
		struct fls_def_event def_event;
	};
};

struct fls_rfs_telemetry_agent_header
{
	u_int32_t   start_magic_num;
	u_int32_t   stats_version;
	u_int32_t   stats_type;
	u_int32_t   payload_len;
} __attribute__ ((__packed__));

bool fls_rfs_enqueue(struct fls_event *event);
void fls_rfs_shutdown(void);
int fls_rfs_init(void);
void fls_rfs_clean_events(void);

#endif
