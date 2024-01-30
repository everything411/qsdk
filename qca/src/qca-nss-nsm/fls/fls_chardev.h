/*
 **************************************************************************
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef FLS_CHARDEV_H
#define FLS_CHARDEV_H

#include <linux/cdev.h>
#include <linux/types.h>

#define FLS_CHARDEV_NAME "fls"
#define FLS_CHARDEV_SAMPLES_MAX 10

enum fls_chardev_event_types {
	FLS_CHARDEV_EVENT_TYPE_DEF
};

struct fls_def_event_sample
{
	uint64_t orig_packets;
	uint64_t orig_bytes;
	uint64_t orig_bytes_min;
	uint64_t orig_bytes_max;
	uint64_t orig_delta_sum;
	uint64_t orig_delta_min;
	uint64_t orig_delta_max;

	uint64_t ret_packets;
	uint64_t ret_bytes;
	uint64_t ret_bytes_min;
	uint64_t ret_bytes_max;
	uint64_t ret_delta_sum;
	uint64_t ret_delta_min;
	uint64_t ret_delta_max;
};

struct fls_def_event {
	uint32_t sample_length_ms;

	uint32_t sample_count;
	struct fls_def_event_sample samples[FLS_CHARDEV_SAMPLES_MAX];
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

bool fls_chardev_enqueue(struct fls_event *event);
void fls_chardev_shutdown(void);
int fls_chardev_init(void);

#endif