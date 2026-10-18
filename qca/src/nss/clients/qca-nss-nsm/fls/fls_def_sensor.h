/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __FLS_DEF_SENSOR_H
#define __FLS_DEF_SENSOR_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include "fls_sensor_manager.h"

#define FLS_DEF_SENSOR_MAX_SAMPLE_COUNT 5
#define FLS_DEF_SENSOR_WINDOWS 3
#define FLS_DEF_SENSOR_WINDOW_LG (FLS_DEF_SENSOR_WINDOWS - 1)
#define FLS_DEF_SENSOR_TOTAL_TIME (fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOW_LG] * fls_def_sensor_sample_count)
#define FLS_DEF_SENSOR_WINDOW_FLAG_SM 0x01
#define FLS_DEF_SENSOR_WINDOW_FLAG_MD 0x02
#define FLS_DEF_SENSOR_WINDOW_FLAG_LG 0x04

#ifdef FLS_DEF_SENSOR_WINDOW_SMALL
#define FLS_DEF_SENSOR_WINDOW_MIN 0
#else
#define FLS_DEF_SENSOR_WINDOW_MIN FLS_DEF_SENSOR_WINDOW_LG - 1
#endif

extern uint32_t fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOWS];
extern uint32_t fls_def_sensor_delay;
extern int32_t fls_def_sensor_max_events;
extern uint32_t fls_def_sensor_sample_count;
extern uint32_t fls_def_sensor_bytes;
extern uint32_t fls_def_sensor_ipat;
extern uint32_t fls_def_sensor_stop_forever;
extern uint32_t fls_def_sensor_pkts_hwm;
extern uint32_t fls_def_sensor_bytes_hwm;
extern uint32_t fls_def_sensor_burst;
extern uint32_t fls_def_sensor_burst_threshold[FLS_DEF_SENSOR_WINDOWS];
extern uint32_t fls_def_sensor_burst_short_intvl[FLS_DEF_SENSOR_WINDOWS];
extern uint32_t fls_def_sensor_burst_long_intvl[FLS_DEF_SENSOR_WINDOWS];
extern uint32_t fls_def_sensor_xxl_sz_threshold;
extern uint32_t fls_def_sensor_xxl_short;
extern uint32_t fls_def_sensor_xxl_long;
extern uint32_t fls_def_sensor_xxl_window;
extern uint32_t fls_def_sensor_xl_window;
extern uint32_t fls_def_sensor_sample_freq;

struct fls_def_sensor_burst {
	bool active;
	uint32_t sz;
	ktime_t start;
	ktime_t last;
};

struct fls_def_sensor_window {
	bool open;
	uint32_t packets;
	uint32_t bytes;
	uint32_t bytes_min;
	uint32_t bytes_max;
	uint64_t delta_sum;
	uint64_t delta_min;
	uint64_t delta_max;
	uint32_t bursts;
	uint32_t burst_sz_sum;
	uint32_t burst_sz_min;
	uint32_t burst_sz_max;
	uint64_t burst_dur_sum;
	uint64_t burst_dur_min;
	uint64_t burst_dur_max;
	struct fls_def_sensor_burst burst_data;
};

struct fls_def_sensor_sample {
	ktime_t last_packet_time;
	ktime_t sample_start_time;
	struct fls_def_sensor_window window[FLS_DEF_SENSOR_WINDOWS];
};

struct fls_def_sensor_timer_data {
	struct hrtimer timer;
	struct fls_conn_cmn *cmn;
	uint8_t flags;
};

struct fls_def_sensor_timers {
	struct fls_def_sensor_timer_data *delay_timer;
	struct fls_def_sensor_timer_data *window_timer;
	struct fls_def_sensor_timer_data *xl_xxl_timer;
};

struct fls_def_sensor_data {
	struct fls_def_sensor_sample samples[FLS_DEF_SENSOR_MAX_SAMPLE_COUNT];
	ktime_t first_packet_time;
	ktime_t event_start_time;
	struct fls_def_sensor_sample xxl_sample;
	struct fls_def_sensor_sample xl_sample;

	/* sendevent:
	 * true - send event when it is generated.
	 * false - not send the event to IFLI when it is generated.
	 * sendevent will be set to false
	 * when receive stop_cmd and if either below is true:
	 *	stop_forever == false
	 *	stop_forever == true and max_events == -1.
	 * sendevent will be reset to true when next XXL window event generates.
	 */
	bool sendevent;

	uint32_t sample_index;
	uint32_t events;
};

struct fls_gro_frag_stats {
	uint16_t min_bytes;		/* Minimum fragment size in bytes */
	uint16_t max_bytes;		/* Maximum fragment size in bytes */
	uint16_t frags_count;		/* Fragment count */
	uint16_t last_frag_ip_id;	/* IP Header id of last frag */
	bool is_gro_skb;		/* skb is gro or not */
};

void fls_def_sensor_timer_init(struct fls_def_sensor_timers *timers);
void fls_def_sensor_timer_delete(struct fls_conn *conn);
void fls_def_sensor_conn_rearm(struct fls_conn *conn);
bool fls_def_sensor_init(struct fls_sensor_manager *fsm);
uint8_t fls_def_sensor_packet_cb(void *app_data, struct fls_conn *conn, struct sk_buff *skb);
#endif
