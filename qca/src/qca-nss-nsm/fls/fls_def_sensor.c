/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/gro.h>

#include "fls_conn.h"
#include "fls_def_sensor.h"
#include "fls_debug.h"

#define FLS_DEF_SENSOR_DELAY_DEF 0
#define FLS_DEF_SENSOR_SAMPLE_LEN_DEF 0
#define FLS_DEF_SENSOR_MAX_EVENTS_DEF -1
#define FLS_DEF_SENSOR_DYNAMIC_SAMPLES_DEF true


uint32_t fls_def_sensor_delay;
uint32_t fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOWS];
int32_t fls_def_sensor_max_events;
uint32_t fls_def_sensor_sample_count;
uint32_t fls_def_sensor_bytes;
uint32_t fls_def_sensor_ipat;
uint32_t fls_def_sensor_stop_forever;
uint32_t fls_def_sensor_burst;
uint32_t fls_def_sensor_burst_threshold[FLS_DEF_SENSOR_WINDOWS];
uint32_t fls_def_sensor_burst_short_intvl[FLS_DEF_SENSOR_WINDOWS];
uint32_t fls_def_sensor_burst_long_intvl[FLS_DEF_SENSOR_WINDOWS];
bool fls_def_sensor_dynamic_samples;
static struct fls_event event;
uint32_t fls_def_sensor_pkts_hwm;
uint32_t fls_def_sensor_bytes_hwm;
uint32_t fls_def_sensor_xxl_sz_threshold;
uint32_t fls_def_sensor_xxl_short;
uint32_t fls_def_sensor_xxl_long;
uint32_t fls_def_sensor_xxl_window;
uint32_t fls_def_sensor_xl_window;

static void fls_def_sensor_window_to_event_window(struct fls_def_sensor_window *orig_sw, struct fls_def_sensor_window *repl_sw, struct fls_def_event_window *ew)
{
		ew->orig_packets = orig_sw->packets;
		ew->orig_bytes = orig_sw->bytes;
		ew->orig_bytes_min = orig_sw->bytes_min;
		ew->orig_bytes_max = orig_sw->bytes_max;
		ew->orig_delta_sum = orig_sw->delta_sum;
		ew->orig_delta_min = orig_sw->delta_min;
		ew->orig_delta_max = orig_sw->delta_max;
		ew->orig_bursts = orig_sw->bursts;
		ew->orig_burst_sz_sum = orig_sw->burst_sz_sum;
		ew->orig_burst_sz_min = orig_sw->burst_sz_min;
		ew->orig_burst_sz_max = orig_sw->burst_sz_max;
		ew->orig_burst_dur_sum = orig_sw->burst_dur_sum;
		ew->orig_burst_dur_min = orig_sw->burst_dur_min;
		ew->orig_burst_dur_max = orig_sw->burst_dur_max;

		orig_sw->packets = 0;
		orig_sw->bytes = 0;
		orig_sw->bytes_min = 0;
		orig_sw->bytes_max = 0;
		orig_sw->delta_sum = 0;
		orig_sw->delta_min = 0;
		orig_sw->delta_max = 0;
		orig_sw->bursts = 0;
		orig_sw->burst_sz_sum = 0;
		orig_sw->burst_sz_min = 0;
		orig_sw->burst_sz_max = 0;
		orig_sw->burst_dur_sum = 0;
		orig_sw->burst_dur_min = 0;
		orig_sw->burst_dur_max = 0;

		ew->ret_packets = repl_sw->packets;
		ew->ret_bytes = repl_sw->bytes;
		ew->ret_bytes_min = repl_sw->bytes_min;
		ew->ret_bytes_max = repl_sw->bytes_max;
		ew->ret_delta_sum = repl_sw->delta_sum;
		ew->ret_delta_min = repl_sw->delta_min;
		ew->ret_delta_max = repl_sw->delta_max;
		ew->ret_bursts = repl_sw->bursts;
		ew->ret_burst_sz_sum = repl_sw->burst_sz_sum;
		ew->ret_burst_sz_min = repl_sw->burst_sz_min;
		ew->ret_burst_sz_max = repl_sw->burst_sz_max;
		ew->ret_burst_dur_sum = repl_sw->burst_dur_sum;
		ew->ret_burst_dur_min = repl_sw->burst_dur_min;
		ew->ret_burst_dur_max = repl_sw->burst_dur_max;

		repl_sw->packets = 0;
		repl_sw->bytes = 0;
		repl_sw->bytes_min = 0;
		repl_sw->bytes_max = 0;
		repl_sw->delta_sum = 0;
		repl_sw->delta_min = 0;
		repl_sw->delta_max = 0;
		repl_sw->bursts = 0;
		repl_sw->burst_sz_sum = 0;
		repl_sw->burst_sz_min = 0;
		repl_sw->burst_sz_max = 0;
		repl_sw->burst_dur_sum = 0;
		repl_sw->burst_dur_min = 0;
		repl_sw->burst_dur_max = 0;
}

/*
 * fls_def_sensor_event_create
 * 	conn: connection which event will be created upon.
 *	time: timestamp.
 * 	isXXL: Event is a X large Large window event.
 * 	isXL: Event is a X large window event.
 */
static void fls_def_sensor_event_create(struct fls_conn *conn, ktime_t time, enum fls_rfs_event_types type)
{
	bool sendevent = conn->stats.isd.sendevent;
	uint32_t i;
	struct fls_conn *orig;
	struct fls_conn *reverse;

	if (!conn->reverse) {
		FLS_WARN("%p cannot create event for unidirectional flow.", conn);
		return;
	}

	if (conn->dir == FLS_CONN_DIRECTION_ORIG) {
		orig = conn;
		reverse = conn->reverse;
	} else {
		orig = conn->reverse;
		reverse = conn;
	}

	event.event_type = type;

	event.dir = 0xEB;
	event.ip_version = conn->ip_version;
	event.protocol = conn->protocol;

	event.orig_src_port = orig->src_port;
	event.orig_dest_port = orig->dest_port;
	event.orig_src_ip[0] = orig->src_ip[0];
	event.orig_src_ip[1] = orig->src_ip[1];
	event.orig_src_ip[2] = orig->src_ip[2];
	event.orig_src_ip[3] = orig->src_ip[3];
	event.orig_dest_ip[0] = orig->dest_ip[0];
	event.orig_dest_ip[1] = orig->dest_ip[1];
	event.orig_dest_ip[2] = orig->dest_ip[2];
	event.orig_dest_ip[3] = orig->dest_ip[3];

	event.ret_src_port = reverse->src_port;
	event.ret_dest_port = reverse->dest_port;
	event.ret_src_ip[0] = reverse->src_ip[0];
	event.ret_src_ip[1] = reverse->src_ip[1];
	event.ret_src_ip[2] = reverse->src_ip[2];
	event.ret_src_ip[3] = reverse->src_ip[3];
	event.ret_dest_ip[0] = reverse->dest_ip[0];
	event.ret_dest_ip[1] = reverse->dest_ip[1];
	event.ret_dest_ip[2] = reverse->dest_ip[2];
	event.ret_dest_ip[3] = reverse->dest_ip[3];
	event.timestamp = time;

	if (type == FLS_RFS_EVENT_TYPE_XXL) {
		FLS_TRACE("%px: %sEnqueue XXL event\n", conn, sendevent? "":"Skip ");

		/*
		 * sample[0].window[0] contains large window data.
		 * window will be closed until next sample starts.
		 */
		fls_def_sensor_window_to_event_window(&orig->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG], &reverse->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG],&event.def_event.samples[0].window[0]);

		orig->stats.isd.xxl_sample.last_packet_time = 0;
		reverse->stats.isd.xxl_sample.last_packet_time = 0;

		//Reset the start time for the next XXL event.
		orig->stats.isd.xxl_sample.sample_start_time = time;
		reverse->stats.isd.xxl_sample.sample_start_time = time;

		event.def_event.window_length[0] = fls_def_sensor_xxl_window;
		event.def_event.sample_count = 1;

		if (sendevent && !fls_rfs_enqueue(&event)) {
			FLS_WARN("XXL Event dropped!\n");
		}
		// enable sendevent for XXL only.
		orig->stats.isd.sendevent = true;
		reverse->stats.isd.sendevent = true;
		return;
	}

	if (type == FLS_RFS_EVENT_TYPE_XL) {
		FLS_TRACE("%px: %sEnqueue XL event\n", conn, sendevent? "":"Skip ");

		/*
		 * sample[0].window[0] contains large window data.
		 * window will be closed until next sample starts.
		 */
		fls_def_sensor_window_to_event_window(&orig->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG], &reverse->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG],&event.def_event.samples[0].window[0]);

		orig->stats.isd.xl_sample.last_packet_time = 0;
		reverse->stats.isd.xl_sample.last_packet_time = 0;

		orig->stats.isd.xl_sample.sample_start_time = time;
		reverse->stats.isd.xl_sample.sample_start_time = time;

		event.def_event.window_length[0] = fls_def_sensor_xl_window;
		event.def_event.sample_count = 1;

		if (sendevent && !fls_rfs_enqueue(&event)) {
			FLS_WARN("XL Event dropped!\n");
		}

		orig->stats.isd.sendevent = true;
		reverse->stats.isd.sendevent = true;
		return;
	}

	event.def_event.sample_count = fls_def_sensor_sample_count;
	for (i = 0; i < FLS_DEF_SENSOR_WINDOWS; i++) {
		event.def_event.window_length[i] = fls_def_sensor_window_sz[i];
	}

	for (i = 0; i < fls_def_sensor_sample_count; i++) {
		uint32_t j;

		for (j = 0; j < FLS_DEF_SENSOR_WINDOWS; j++) {
			fls_def_sensor_window_to_event_window(&orig->stats.isd.samples[i].window[j], &reverse->stats.isd.samples[i].window[j], &event.def_event.samples[i].window[j]);
			orig->stats.isd.samples[i].window[j].open = true;
			reverse->stats.isd.samples[i].window[j].open = true;
		}
		orig->stats.isd.samples[i].last_packet_time = 0;
		reverse->stats.isd.samples[i].last_packet_time = 0;
	}

	if (!fls_rfs_enqueue(&event)) {
		FLS_WARN("Event dropped!\n");
	}
}

static void fls_def_sensor_bytes_record(struct fls_def_sensor_sample *sample, uint32_t bytes, struct fls_gro_frag_stats gro_stats)
{
	uint32_t min_bytes = gro_stats.is_gro_skb ? gro_stats.min_bytes : bytes;
	uint32_t max_bytes = gro_stats.is_gro_skb ? gro_stats.max_bytes : bytes;

	if (sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets == 0) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes = bytes;
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_min = min_bytes;
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_max = max_bytes;
		return;
	}

	sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes += bytes;
	if (min_bytes < sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_min) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_min = min_bytes;
	} else if (max_bytes > sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_max) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_max = max_bytes;
	}
}

static void fls_def_sensor_burst_open(struct fls_def_sensor_burst *burst_data, ktime_t now, uint32_t bytes, uint32_t thresh) {
	burst_data->start = now;
	burst_data->last = now;
	burst_data->sz = bytes;
	if (bytes > thresh) {
		burst_data->active = true;
	} else {
		burst_data->active = false;
	}
}

static void fls_def_sensor_burst_close(struct fls_def_sensor_window *window)
{
	ktime_t dur;

	if (!window->burst_data.active) {
		memset(&(window->burst_data), 0, sizeof(struct fls_def_sensor_burst));
		return;
	}

	dur = ktime_sub(window->burst_data.last, window->burst_data.start);

	if (window->bursts == 0) {
		window->burst_dur_sum = dur;
		window->burst_dur_min = dur;
		window->burst_dur_max = dur;

		window->burst_sz_sum = window->burst_data.sz;
		window->burst_sz_min = window->burst_data.sz;
		window->burst_sz_max = window->burst_data.sz;

		memset(&(window->burst_data), 0, sizeof(struct fls_def_sensor_burst));
		window->bursts++;
		return;
	}

	window->bursts++;

	window->burst_dur_sum += dur;
	if (dur < window->burst_dur_min) {
		window->burst_dur_min = dur;
	} else if (dur > window->burst_dur_max) {
		window->burst_dur_max = dur;
	}

	window->burst_sz_sum += window->burst_data.sz;
	if (window->burst_data.sz < window->burst_sz_min) {
		window->burst_sz_min = window->burst_data.sz;
	} else if (window->burst_data.sz > window->burst_sz_max) {
		window->burst_sz_max = window->burst_data.sz;
	}

	memset(&(window->burst_data), 0, sizeof(struct fls_def_sensor_burst));
}

static void fls_def_sensor_burst_record(struct fls_def_sensor_window *window, ktime_t now, uint32_t bytes, uint32_t thresh, uint32_t short_intvl, uint32_t long_intvl)
{
	uint64_t delta;

	if (!window->burst_data.start) {
		fls_def_sensor_burst_open(&window->burst_data, now, bytes, thresh);
		return;
	}

	if (window->burst_data.active) {
		delta = ktime_sub(now, window->burst_data.last);
		if (delta >= ms_to_ktime(long_intvl)) {
			fls_def_sensor_burst_close(window);
			fls_def_sensor_burst_open(&window->burst_data, now, bytes, thresh);
			return;
		}

		window->burst_data.sz += bytes;
		window->burst_data.last = now;
		return;
	}

	delta = ktime_sub(now, window->burst_data.start);
	if (delta >= ms_to_ktime(short_intvl)) {
		fls_def_sensor_burst_open(&window->burst_data, now, bytes, thresh);
		return;
	}

	window->burst_data.sz += bytes;
	window->burst_data.last = now;
	if (window->burst_data.sz > thresh) {
		window->burst_data.active = true;
	}
}

static void fls_def_sensor_ipat_record(struct fls_def_sensor_sample *sample, ktime_t now)
{
	ktime_t delta;

	if (sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets == 0) {
		sample->last_packet_time = now;
		return;
	}

	delta = ktime_sub(now, sample->last_packet_time);
	sample->last_packet_time = now;

	if (sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_sum == 0) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_sum = delta;
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_min = delta;
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_max = delta;
		return;
	}

	sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_sum += delta;
	if (delta < sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_min) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_min = delta;
	} else if (delta > sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_max) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_max = delta;
	}
}

static void fls_def_sensor_window_close(struct fls_def_sensor_sample *sample, struct fls_def_sensor_window *window)
{
	window->open = false;
	window->packets = sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets;
	window->bytes = sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes;
	window->bytes_min = sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_min;
	window->bytes_max = sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes_max;
	window->delta_sum = sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_sum;
	window->delta_min = sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_min;
	window->delta_max = sample->window[FLS_DEF_SENSOR_WINDOW_LG].delta_max;
	fls_def_sensor_burst_close(window);
}

/*
 * Traverses the skb to analyze its GRO fragments.
 * Calculates the minimum and maximum fragment sizes in the skb.
 */
int fls_def_traverse_gro_skb(struct sk_buff *skb, struct fls_gro_frag_stats *gro_stats) {
	struct sk_buff *frag;
	uint16_t min_frag_size = 0;
	uint16_t max_frag_size = 0;
	uint16_t frag_size = 0;
	uint16_t frag_hdrlen = 0;
	uint16_t last_frag_ip_id = 0;

	/*
	 * There are two cases that needs to be handled for GRO skb:
	 * Case 1: Handle fragments in skb_shinfo(skb)->frag_list (chained fragments).
	 * Case 2: Handle fragments in skb_shinfo(skb)->nr_frags (when page mode is enabled).
	 * TODO: Case 2 is not handled in the current implementation.
	 */

	/*
	 * Case 1: Handle fragments in skb_shinfo(skb)->frag_list.
	 */

	/*
	 * Start with the head skb length as a fragment size (includes headers).
	 */
	min_frag_size = max_frag_size = skb_headlen(skb);

	/*
	 * Check if the skb contains a fragment list (skb_shinfo(skb)->frag_list).
	 * It is typically used to determine whether the skb includes
	 * additional data fragments beyond the linear data area.
	 */
	if (likely(skb_has_frag_list(skb))) {
		/*
		 * skb frag parsing
		 */
		skb_walk_frags(skb, frag) {
			/*
			 * The following logic calculates the header length in the fragment skb.
			 * It is determined as follows:
			 * frag->head: points to the start of the headroom.
			 * frag->network_header: points to the network offset.
			 * frag->data: points to the data start (which is the payload).
			 * The header length includes both the IP and transport headers.
			 */
			frag_hdrlen = frag->data - (frag->head + frag->network_header);

			/*
			 * Calculate the total fragment size by adding the fragment length and header length.
			 */
			frag_size = frag->len + frag_hdrlen;

			/*
			 * Update min fragment size
			 */
			if (frag_size < min_frag_size) {
				min_frag_size = frag_size;
			}

			/*
			 * Update max fragment size
			 */
			if (frag_size > max_frag_size) {
				max_frag_size = frag_size;
			}

			/*
			 * Update the IP Header ID of the last fragment
			 */
			last_frag_ip_id = ntohs(ip_hdr(frag)->id);

			FLS_TRACE("frag_len = %u, frag_hdrlen = %u, frag_size(total) = %u, min_frag_size = %u, max_frag_size = %u\n", frag->len, frag_hdrlen, frag_size, min_frag_size, max_frag_size);
		}
		/*
		 * Update the final min, max frag sizes and last frag IP Header ID.
		 */
		gro_stats->min_bytes = min_frag_size;
		gro_stats->max_bytes = max_frag_size;
		gro_stats->last_frag_ip_id = last_frag_ip_id;

		return 0;
	} else if (skb_shinfo(skb)->nr_frags > 0) {
		/*
		 * TODO:
		 * Case 2: Unhandled paged buffers case detected.
		 */
		FLS_WARN("Unhandled paged buffers (nr_frags=%d)\n", skb_shinfo(skb)->nr_frags);
	} else {
		/*
		 * No fragments (neither frag_list nor nr_frags)
		 * A case when skb is marked as GRO but contains only linear data.
		 */
		FLS_WARN("No fragments: (neither frag_list nor nr_frags)\n");
	}

	gro_stats->min_bytes = skb_headlen(skb);
	gro_stats->max_bytes = skb_headlen(skb);
	gro_stats->last_frag_ip_id = ntohs(ip_hdr(skb)->id);

	return -1;
}

uint8_t fls_def_sensor_packet_cb(void *app_data, struct fls_conn *conn, struct sk_buff *skb)
{
	ktime_t now;
	uint32_t sample_index;
	struct fls_def_sensor_sample *sample;
	struct fls_def_sensor_sample *xxl_sample;
	struct fls_def_sensor_sample *xl_sample;
	uint32_t delay = fls_def_sensor_delay;
	uint32_t sample_length = fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOW_LG];
	struct fls_gro_frag_stats gro_stats = {0};
	int64_t sample_diff, xxl_diff, xl_diff;
	int i, ret = 0;

	if (fls_def_sensor_max_events == 0 || sample_length == 0) {
		FLS_TRACE("%p Default sensor disabled.\n", conn);
		return SFE_FLS_CONNECTION_FLAG_DEF_DISABLE;
	}

	if (!(conn->flags & SFE_FLS_CONNECTION_FLAG_DEF_ENABLE)) {
		FLS_TRACE("%p Statistics disabled.\n", conn);
		return SFE_FLS_CONNECTION_FLAG_DEF_DISABLE;
	}

	if (unlikely(conn->flags == SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED)) {
		FLS_TRACE("%p HWM exceeded, Statistics disabled.\n", conn);
		return SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
	}

	if(conn->externalrule) {
		now = skb->tstamp;
		skb->len -= 14;
	} else {
		now = ktime_get_boottime();
	}

	if (!conn->stats.isd.first_packet_time) {
		FLS_WARN("%p First packet. t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]", conn, now,
			conn->src_ip, ntohs(conn->src_port), conn->dest_ip, ntohs(conn->dest_port), ntohs(ip_hdr(skb)->id),
			ip_hdr(skb)->protocol);

		fls_debug_print_conn_info(conn);

		conn->stats.isd.first_packet_time = now;
		conn->stats.isd.event_start_time = now;
		conn->stats.isd.samples[0].sample_start_time = now;

		/* Initializing X large and XL large window. */
		conn->stats.isd.xxl_sample.sample_start_time = now;
		conn->stats.isd.xl_sample.sample_start_time = now;
		conn->stats.isd.sendevent = true;

		for (i = 0; i < FLS_DEF_SENSOR_MAX_SAMPLE_COUNT; i++) {
			int j;

			for (j = 0; j < FLS_DEF_SENSOR_WINDOWS; j++) {
				conn->stats.isd.samples[i].window[j].open = true;
			}
		}

		/*
		 * For XXL sample, Only the last window is opened for data collection
		 */
		FLS_WARN("%p start XXL window, t = %lld \n", conn, now);
		conn->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;

		/*
		 * For XL sample, Only the last window is opened for data collection
		 * since only one window is needed
		 */
		FLS_WARN("%p start XL window, t = %lld \n", conn, now);
		conn->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;

		if (conn->reverse) {
			struct fls_conn *reverse = conn->reverse;
			reverse->stats.isd.first_packet_time = now;
			reverse->stats.isd.event_start_time = now;
			reverse->stats.isd.samples[0].sample_start_time = now;
			reverse->stats.isd.xxl_sample.sample_start_time = now;
			reverse->stats.isd.xl_sample.sample_start_time = now;
			reverse->stats.isd.sendevent = true;

			for (i = 0; i < FLS_DEF_SENSOR_MAX_SAMPLE_COUNT; i++) {
				int j;

				for (j = 0; j < FLS_DEF_SENSOR_WINDOWS; j++) {
					reverse->stats.isd.samples[i].window[j].open = true;
				}
			}

			reverse->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;
			reverse->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;
		}
	}

	if (!(conn->flags & SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED)) {
		uint64_t diff = ktime_to_ms(ktime_sub(now, conn->stats.isd.first_packet_time));
		if (diff < delay) {
			return SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
		}

		conn->flags |= SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED;
		FLS_WARN("%p Delay finished, starting data collection, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]",
			conn, now, &conn->src_ip, ntohs(conn->src_port), &conn->dest_ip, ntohs(conn->dest_port), ntohs(ip_hdr(skb)->id),
			ip_hdr(skb)->protocol);
		conn->stats.isd.first_packet_time = now;
		conn->stats.isd.event_start_time = now;
		conn->stats.isd.samples[0].sample_start_time = now;
		conn->stats.isd.xxl_sample.sample_start_time = now;
		conn->stats.isd.xl_sample.sample_start_time = now;

		if (conn->reverse) {
			conn->reverse->flags |= SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED;
			conn->reverse->stats.isd.first_packet_time = now;
			conn->reverse->stats.isd.event_start_time = now;
			conn->reverse->stats.isd.samples[0].sample_start_time = now;
			conn->reverse->stats.isd.xxl_sample.sample_start_time = now;
			conn->reverse->stats.isd.xl_sample.sample_start_time = now;
		}

		fls_debug_print_conn_info(conn);
	}

	/*
	 * For GRO skbs, retrieve the fragment count and add it to the packet count.
	 * Also, determine the minimum and maximum fragment sizes to update the min and max byte values.
	 */
	if (skb_is_gso(skb)) {
		gro_stats.frags_count = NAPI_GRO_CB(skb)->count;
		gro_stats.is_gro_skb = true;

		FLS_TRACE("%p: skb_is_gso, gso_size(mss) = %u, GRO fragments count: %u, nr_frags = %d, gso_segs = %u\n", skb, skb_shinfo(skb)->gso_size, gro_stats.frags_count, skb_shinfo(skb)->nr_frags, skb_shinfo(skb)->gso_segs);
		FLS_TRACE("%p: skb_is_gso, skb->len = %u, skb->data_len = %u, skb->headlen = %u\n", skb, skb->len, skb->data_len, skb_headlen(skb));

		ret = fls_def_traverse_gro_skb(skb, &gro_stats);
		if (ret) {
			FLS_ERROR("%p: Failed to traverse GRO skb, continue with head skb\n", skb);
		}

		FLS_TRACE("Minimum fragment size: %u\n", gro_stats.min_bytes);
		FLS_TRACE("Maximum fragment size: %u\n", gro_stats.max_bytes);
	}

	sample_index = conn->stats.isd.sample_index;
	sample_diff = ktime_to_ms(ktime_sub(now, conn->stats.isd.samples[sample_index].sample_start_time));
	for (i = 0; i < FLS_DEF_SENSOR_WINDOW_LG; i++) {
		if (sample_diff >= fls_def_sensor_window_sz[i] && conn->stats.isd.samples[sample_index].window[i].open) {
			fls_def_sensor_window_close(&conn->stats.isd.samples[sample_index], &conn->stats.isd.samples[sample_index].window[i]);
			if (conn->reverse) {
				fls_def_sensor_window_close(&conn->reverse->stats.isd.samples[sample_index], &conn->reverse->stats.isd.samples[sample_index].window[i]);
			}
		}
	}

	/*
	 * If the time is past the end of the current sample, we need to start a new sample.
	 */
	if (sample_diff >= sample_length) {
		/*
		 * Check if sample exceeds watermark
		 */
		sample = &(conn->stats.isd.samples[sample_index]);
		if ((fls_def_sensor_pkts_hwm && sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets >= fls_def_sensor_pkts_hwm) || (fls_def_sensor_bytes_hwm && sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes >= fls_def_sensor_bytes_hwm)) {
			FLS_INFO("%p HWM exceeded. pkts=%u pkt_hwm=%u, bytes=%u bytes_hwm=%u", conn, sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets, fls_def_sensor_pkts_hwm, sample->window[FLS_DEF_SENSOR_WINDOW_LG].bytes, fls_def_sensor_bytes_hwm);
			conn->flags = SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
			if (conn->reverse) {
				conn->reverse->flags = SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
			}
			return SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
		}

		if (fls_def_sensor_burst) {
			fls_def_sensor_burst_close(&conn->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG]);
			if (conn->reverse) {
				fls_def_sensor_burst_close(&conn->reverse->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG]);
			}
		}

		sample_index += 1;
		FLS_TRACE("%p increased sample_index to %u", conn, sample_index);

		if (sample_index < fls_def_sensor_sample_count) {
			conn->stats.isd.samples[sample_index].sample_start_time = now;
			if (conn->reverse) {
				conn->reverse->stats.isd.samples[sample_index].sample_start_time = now;
			}
		}
	}

	if (conn->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open) {
		xl_diff = ktime_to_ms(ktime_sub(now, conn->stats.isd.xl_sample.sample_start_time));
		if(xl_diff >= fls_def_sensor_xl_window) {
			/*
			 * No need to invoke window_close function
			 * since WINDOW_LG itself is used for X large samples.
			 */
			if (fls_def_sensor_burst) {
				fls_def_sensor_burst_close(&conn->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
				if (conn->reverse) {
					fls_def_sensor_burst_close(&conn->reverse->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
					FLS_TRACE("%px Sending XL window: original burst_cnt = %d, reply_cnt %d\n",
							conn, conn->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts,
							conn->reverse->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
				} else {
					FLS_TRACE("%px Sending XL window: original burst_cnt = %d\n",
							conn, conn->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
				}
			}
			FLS_WARN("%p Create XL event, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]", conn, now,
				conn->src_ip, ntohs(conn->src_port), conn->dest_ip, ntohs(conn->dest_port),
				skb_is_gso(skb) ? gro_stats.last_frag_ip_id : ntohs(ip_hdr(skb)->id), ip_hdr(skb)->protocol);
			fls_def_sensor_event_create(conn, now, FLS_RFS_EVENT_TYPE_XL);
		}
	}

	if (conn->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open) {
		xxl_diff = ktime_to_ms(ktime_sub(now, conn->stats.isd.xxl_sample.sample_start_time));
		if(xxl_diff >= fls_def_sensor_xxl_window) {

			/*
			 * No need to invoke window_close function
			 * since WINDOW_LG itself is used for X large samples.
			 */
			if (fls_def_sensor_burst) {
				fls_def_sensor_burst_close(&conn->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
				if (conn->reverse) {
					fls_def_sensor_burst_close(&conn->reverse->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
					FLS_TRACE("%px Sending XXL window: original burst_cnt = %d, reply_cnt %d\n", conn, conn->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts,conn->reverse->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
				} else {
					FLS_TRACE("%px Sending XXL window: original burst_cnt = %d\n", conn, conn->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
				}
			}

			FLS_WARN("%p Create XXL event, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]", conn, now,
				conn->src_ip, ntohs(conn->src_port), conn->dest_ip, ntohs(conn->dest_port),
				skb_is_gso(skb) ? gro_stats.last_frag_ip_id : ntohs(ip_hdr(skb)->id), ip_hdr(skb)->protocol);
			fls_def_sensor_event_create(conn, now, FLS_RFS_EVENT_TYPE_XXL);
		}
	}

	/*
	 * If we've already generated enough samples, it's time to create a new event
	 */
	if (sample_index >= fls_def_sensor_sample_count) {
		int32_t abs_diff = ktime_to_ms(ktime_sub(now, conn->stats.isd.first_packet_time));
		uint32_t event_count;
		ktime_t event_start_new;

		/*
		 * Calculate the index of the event to be written.
		 */
		if (fls_def_sensor_dynamic_samples) {
			event_count = conn->stats.isd.events + 1;
			event_start_new = now;
		} else {
			event_count = abs_diff / FLS_DEF_SENSOR_TOTAL_TIME;
			event_start_new = ktime_add_ms(conn->stats.isd.event_start_time, (event_count - conn->stats.isd.events) * FLS_DEF_SENSOR_TOTAL_TIME);
		}

		/*
		 * If the calculated event index is greater than the index of the last generated event, generate a new event.
		 */
		if ((event_count > conn->stats.isd.events)) {
			struct fls_conn *reply = conn->reverse;

			FLS_WARN("%p Create Default event, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]", conn, now,
				conn->src_ip, ntohs(conn->src_port), conn->dest_ip, ntohs(conn->dest_port),
				skb_is_gso(skb) ? gro_stats.last_frag_ip_id : ntohs(ip_hdr(skb)->id), ip_hdr(skb)->protocol);
			fls_def_sensor_event_create(conn, now, FLS_RFS_EVENT_TYPE_DEF);
			conn->stats.isd.events = event_count;
			if (reply) {
				reply->stats.isd.events = event_count;
			}

			/*
			 * If we have a nonnegative max event count and have passed it, disable this connection and return.
			 */
			if ((fls_def_sensor_max_events >= 0) && (event_count >= fls_def_sensor_max_events)) {
				conn->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				if (reply) {
					reply->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				}

				return SFE_FLS_CONNECTION_FLAG_DEF_DISABLE;
			}

			conn->stats.isd.event_start_time = event_start_new;
			if (conn->stats.isd.event_start_time > now) {
				FLS_WARN("Advanced time too far, now=%lli, event_start=%lli", now, conn->stats.isd.event_start_time);
			}

			if (reply) {
				reply->stats.isd.event_start_time = conn->stats.isd.event_start_time;
			}

			sample_index = 0;
			conn->stats.isd.samples[sample_index].sample_start_time = now;

			if (reply) {
				reply->stats.isd.samples[sample_index].sample_start_time = now;
			}
		}
	}

	conn->stats.isd.sample_index = sample_index;

	xxl_sample = &(conn->stats.isd.xxl_sample);

	xl_sample = &(conn->stats.isd.xl_sample);

	if (conn->reverse) {
		conn->reverse->stats.isd.sample_index = sample_index;
	}

	sample = &(conn->stats.isd.samples[sample_index]);

	/* Record window data, along with XXL/XL window if it is open. */
	if (fls_def_sensor_bytes) {
		fls_def_sensor_bytes_record(sample, skb->len, gro_stats);
		if (xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			fls_def_sensor_bytes_record(xxl_sample, skb->len, gro_stats);
		if (xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			fls_def_sensor_bytes_record(xl_sample, skb->len, gro_stats);
	}

	if (fls_def_sensor_ipat) {
		fls_def_sensor_ipat_record(sample, now);
		if(xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			fls_def_sensor_ipat_record(xxl_sample, now);
		if(xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			fls_def_sensor_ipat_record(xl_sample, now);
	}

	if (fls_def_sensor_burst) {
		for (i = 0; i < FLS_DEF_SENSOR_WINDOWS; i++){
			if (sample->window[i].open) {
				fls_def_sensor_burst_record(&sample->window[i], now, skb->len, fls_def_sensor_burst_threshold[i], fls_def_sensor_burst_short_intvl[i], fls_def_sensor_burst_long_intvl[i]);
			}
		}

		if(xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open) {
			fls_def_sensor_burst_record(&xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG], now, skb->len, fls_def_sensor_xxl_sz_threshold, fls_def_sensor_xxl_short, fls_def_sensor_xxl_long);
		}

		if(xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open) {
			fls_def_sensor_burst_record(&xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG], now, skb->len, fls_def_sensor_xxl_sz_threshold, fls_def_sensor_xxl_short, fls_def_sensor_xxl_long);
		}
	}

	if ((gro_stats.is_gro_skb) && (skb_shinfo(skb)->nr_frags == 0)) {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets += gro_stats.frags_count;
		if (xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets += gro_stats.frags_count;

		if (xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets += gro_stats.frags_count;
	} else {
		sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets++;
		if (xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			xxl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets++;

		if (xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].open)
			xl_sample->window[FLS_DEF_SENSOR_WINDOW_LG].packets++;
	}

	return SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
}

bool fls_def_sensor_init(struct fls_sensor_manager *fsm)
{
	fls_def_sensor_delay = FLS_DEF_SENSOR_DELAY_DEF;
	fls_def_sensor_max_events = FLS_DEF_SENSOR_MAX_EVENTS_DEF;
	fls_def_sensor_dynamic_samples = FLS_DEF_SENSOR_DYNAMIC_SAMPLES_DEF;
	fls_def_sensor_sample_count = FLS_DEF_SENSOR_MAX_SAMPLE_COUNT;
	fls_def_sensor_bytes = 1;
	fls_def_sensor_ipat = 1;

	fls_def_sensor_stop_forever = 1;

	fls_def_sensor_bytes_hwm = 0;
	fls_def_sensor_pkts_hwm = 0;
	fls_def_sensor_burst = 1;
	fls_def_sensor_xxl_sz_threshold = 0;
	fls_def_sensor_xxl_short = 0;
	fls_def_sensor_xxl_long = 0;
	fls_def_sensor_xxl_window = 0;
	fls_def_sensor_xl_window = 0;

	return fls_sensor_manager_register(fsm, fls_def_sensor_packet_cb, NULL);
}
