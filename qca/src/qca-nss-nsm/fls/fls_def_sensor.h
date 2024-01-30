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

#ifndef __FLS_DEF_SENSOR_H
#define __FLS_DEF_SENSOR_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include "fls_sensor_manager.h"

#define FLS_DEF_SENSOR_MAX_SAMPLE_COUNT 10
#define FLS_DEF_SENSOR_TOTAL_TIME (fls_def_sensor_sample_length * fls_def_sensor_sample_count)

extern uint32_t fls_def_sensor_sample_length;
extern uint32_t fls_def_sensor_delay;
extern int32_t fls_def_sensor_max_events;
extern uint32_t fls_def_sensor_sample_count;
extern uint32_t fls_def_sensor_bytes;
extern uint32_t fls_def_sensor_ipat;

struct fls_def_sensor_sample {
	uint64_t packets;
	uint64_t bytes;
	uint64_t bytes_min;
	uint64_t bytes_max;
	uint64_t delta_sum;
	uint64_t delta_min;
	uint64_t delta_max;
	ktime_t last_packet_time;
	ktime_t sample_start_time;
};

struct fls_def_sensor_data {
	struct fls_def_sensor_sample samples[FLS_DEF_SENSOR_MAX_SAMPLE_COUNT];
	ktime_t first_packet_time;
	ktime_t event_start_time;
	uint32_t sample_index;
	uint32_t events;
};

bool fls_def_sensor_init(struct fls_sensor_manager *fsm);
#endif