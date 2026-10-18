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
uint32_t fls_def_sensor_sample_freq;
struct hrtimer global_timer;

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
		atomic_inc(&conn->fls_conn_exception_counters[FLS_CONN_EXCEPTION_CANNOT_CREATE_EVENT_UNIDIR_FLOW]);
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
			atomic_inc(&orig->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_XXL_FAIL]);
			atomic_inc(&reverse->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_XXL_FAIL]);
		} else if (sendevent) {
			atomic_inc(&orig->fls_conn_counters[FLS_CONN_RFS_ENQUEUE_XXL]);
			atomic_inc(&reverse->fls_conn_counters[FLS_CONN_RFS_ENQUEUE_XXL]);
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
			atomic_inc(&orig->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_XL_FAIL]);
			atomic_inc(&reverse->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_XL_FAIL]);
		} else if (sendevent) {
			atomic_inc(&orig->fls_conn_counters[FLS_CONN_RFS_ENQUEUE_XL]);
			atomic_inc(&reverse->fls_conn_counters[FLS_CONN_RFS_ENQUEUE_XL]);
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
		atomic_inc(&orig->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_DEF_FAIL]);
		atomic_inc(&reverse->fls_conn_exception_counters[FLS_CONN_EXCEPTION_RFS_ENQUEUE_DEF_FAIL]);
	} else {
		atomic_inc(&orig->fls_conn_counters[FLS_CONN_RFS_EVENT_TYPE_DEF]);
		atomic_inc(&reverse->fls_conn_counters[FLS_CONN_RFS_EVENT_TYPE_DEF]);
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

/*
 * fls_def_sensor_delay_timer_callback()
 *	Handler for when per connection delay finishes
 */
static enum hrtimer_restart fls_def_sensor_delay_timer_callback(struct hrtimer *timer)
{
	struct fls_def_sensor_timer_data *data = container_of(timer, struct fls_def_sensor_timer_data, timer);
	struct fls_conn_cmn *cmn = data->cmn;
	int i;
	ktime_t kt;

	/*
	 * Handling delay is done
	 */
	FLS_INFO("%p Delay finished, starting data collection", cmn->orig);
	cmn->orig->flags |= SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED;
	cmn->reply->flags |= SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED;
	fls_debug_print_conn_info(cmn->orig);

	/*
	 * Set flags and timers for default window data collection
	 */
	cmn->orig->stats.isd.sendevent = true;
	cmn->reply->stats.isd.sendevent = true;

	/*
	 * For XL and XXL samples, Only the last window is opened for data collection
	 */
	cmn->orig->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;
	cmn->reply->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;
	cmn->orig->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;
	cmn->reply->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].open = true;

	/*
	 * Trigger Sample timer
	 */
	FLS_TRACE("trigger XL and XXL post delay\n");
	kt = ms_to_ktime(fls_def_sensor_xl_window);

	/*
	 * Initialize window counter with 1, as we cannot divide by 0
	 */
	cmn->timers->xl_xxl_timer->flags = 1;
	hrtimer_start(&cmn->timers->xl_xxl_timer->timer, kt, HRTIMER_MODE_REL);

	for (i = 0; i < FLS_DEF_SENSOR_MAX_SAMPLE_COUNT; i++) {
		int j;

		for (j = 0; j < FLS_DEF_SENSOR_WINDOWS; j++) {
			cmn->orig->stats.isd.samples[i].window[j].open = true;
			cmn->reply->stats.isd.samples[i].window[j].open = true;
		}
	}

	/*
	 * Trigger timer for sample and window statistics division
	 * This timer must be triggered last, as it contains the check to stop
	 * recording data based on event count
	 */
	cmn->timers->window_timer->flags = 1 << (FLS_DEF_SENSOR_WINDOW_MIN);
	FLS_TRACE("trigger window %d post delay\n", FLS_DEF_SENSOR_WINDOW_MIN);
	kt = ms_to_ktime(fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOW_MIN]);
	hrtimer_start(&cmn->timers->window_timer->timer, kt, HRTIMER_MODE_REL);
	atomic_inc(&cmn->orig->fls_conn_counters[FLS_CONN_PER_CONN_DELAY_FINISHED]);
	atomic_inc(&cmn->reply->fls_conn_counters[FLS_CONN_PER_CONN_DELAY_FINISHED]);

	return HRTIMER_NORESTART;
}

/*
 * fls_def_sensor_window_timer_callback()
 *	Handler for when windows of windowed samples expire
 */
static enum hrtimer_restart fls_def_sensor_window_timer_callback(struct hrtimer *timer)
{
	struct fls_def_sensor_timer_data *data = container_of(timer, struct fls_def_sensor_timer_data, timer);
	struct fls_conn_cmn *cmn;
	uint32_t sample_index, window_time_diff;
	uint8_t window_next, window_index;
	ktime_t kt, now;

	/*
	 * Determine which window size triggered the CB
	 */
	if (data->flags & FLS_DEF_SENSOR_WINDOW_FLAG_SM) {
		FLS_TRACE("Small window timer expired %p\n", data->timer);
		window_index = 0;
	} else if (data->flags & FLS_DEF_SENSOR_WINDOW_FLAG_MD) {
		FLS_TRACE("Medium window timer expired %p\n", data->timer);
		window_index = 1;
	} else if (data->flags & FLS_DEF_SENSOR_WINDOW_FLAG_LG){
		FLS_TRACE("Large window timer expired %p\n", data->timer);
		window_index = FLS_DEF_SENSOR_WINDOW_LG;
	} else {
		FLS_ERROR("Invalid flags fed to window_timer_callback\n");
		atomic_inc(&data->cmn->orig->fls_conn_exception_counters[FLS_CONN_EXCEPTION_INVALID_FLAGS_WINDOW_TIMER_CALLBACK]);
		atomic_inc(&data->cmn->reply->fls_conn_exception_counters[FLS_CONN_EXCEPTION_INVALID_FLAGS_WINDOW_TIMER_CALLBACK]);
		return HRTIMER_NORESTART;
	}

	/*
	 * Fetch conn_cmn structure to reference connection instances
	 */
	cmn = data->cmn;

	sample_index = cmn->orig->stats.isd.sample_index;
	if (cmn->orig->stats.isd.samples[sample_index].window[window_index].open) {
		fls_def_sensor_window_close(&cmn->orig->stats.isd.samples[sample_index], &cmn->orig->stats.isd.samples[sample_index].window[window_index]);
	} else {
		FLS_WARN("%p Invalid Window Close Call, sample index: %u, window_index: %u \n", cmn->orig, sample_index, window_index);
	}

	if (cmn->reply->stats.isd.samples[sample_index].window[window_index].open) {
		fls_def_sensor_window_close(&cmn->reply->stats.isd.samples[sample_index], &cmn->reply->stats.isd.samples[sample_index].window[window_index]);
	} else {
		FLS_WARN("%p Invalid Reply Window Close Call, sample index: %u, window_index: %u \n", cmn->reply, sample_index, window_index);
	}

	/*
	 * If the largest sample has closed start a new one
	 */
	if (window_index == FLS_DEF_SENSOR_WINDOW_LG) {
		/*
		 * Check if HWM was exceeded
		 */
		if ((fls_def_sensor_pkts_hwm && (cmn->orig->stats.isd.samples[sample_index].window[window_index].packets >= fls_def_sensor_pkts_hwm ||
			cmn->reply->stats.isd.samples[sample_index].window[window_index].packets >= fls_def_sensor_pkts_hwm)) ||
			(fls_def_sensor_bytes_hwm && ((cmn->orig->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].bytes >= fls_def_sensor_bytes_hwm) ||
			cmn->reply->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].bytes >= fls_def_sensor_bytes_hwm))) {
			FLS_WARN("%p HWM exceeded. orig_pkts=%u reply_pkts= %u pkt_hwm=%u, orig_bytes=%u reply_bytes=%u bytes_hwm=%u", cmn->orig, cmn->orig->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].packets,
					cmn->reply->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].packets,fls_def_sensor_pkts_hwm, cmn->orig->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].bytes,
					cmn->reply->stats.isd.samples[sample_index].window[FLS_DEF_SENSOR_WINDOW_LG].bytes, fls_def_sensor_bytes_hwm);
			atomic_inc(&cmn->orig->fls_conn_exception_counters[FLS_CONN_SENSOR_HWM_EXCEEDED]);
			atomic_inc(&cmn->reply->fls_conn_exception_counters[FLS_CONN_SENSOR_HWM_EXCEEDED]);
			cmn->orig->flags = SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
			cmn->reply->flags = SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
			return HRTIMER_NORESTART;
		}

		/*
		 * Increase the sample index, as we have filled all windows of this sample
		 */
		sample_index += 1;
		FLS_TRACE("%p increased sample_index to %u", cmn->orig, sample_index);

		/*
		 * Handling if we have fully filled an event
		 */
		if (sample_index > fls_def_sensor_sample_count - 1) {
			/*
		 	 * Create a new event
		 	 */
			now = ktime_get_boottime();
			FLS_WARN("%p Create Default event, t = %lld, [%pI4:%hu -> %pI4:%hu] proto = %u]", cmn->orig, now,
				cmn->orig->src_ip, ntohs(cmn->orig->src_port), cmn->orig->dest_ip, ntohs(cmn->orig->dest_port),
				cmn->orig->protocol);
			fls_def_sensor_event_create(cmn->orig, now, FLS_RFS_EVENT_TYPE_DEF);
			cmn->orig->stats.isd.events++;
			cmn->reply->stats.isd.events++;

			/*
		 	 * If we have a nonnegative max event count and have exceeded it, disable this connection and return;
			 */
			if ((fls_def_sensor_max_events >= 0) && (cmn->orig->stats.isd.events >= fls_def_sensor_max_events)) {
				FLS_TRACE("Exceeded max event count, disabling connection %p\n", cmn->orig->flags);
				atomic_inc(&cmn->orig->fls_conn_exception_counters[FLS_CONN_SENSOR_MAX_EVENT_EXCEEDED]);
				atomic_inc(&cmn->reply->fls_conn_exception_counters[FLS_CONN_SENSOR_MAX_EVENT_EXCEEDED]);
				cmn->orig->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				cmn->reply->flags &= ~SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
				return HRTIMER_NORESTART;
			}
			sample_index = 0;
		}

		cmn->orig->stats.isd.sample_index = sample_index;
		cmn->reply->stats.isd.sample_index = sample_index;

		/*
		 * Reset the window index
		 */
		window_time_diff = fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOW_MIN];
		window_index = FLS_DEF_SENSOR_WINDOW_MIN;
	} else {
		/*
		 * Trigger the timer using the diff between the time elapsed and the length of the next window;
		 */
		window_time_diff = fls_def_sensor_window_sz[window_index + 1] - fls_def_sensor_window_sz[window_index];
		window_index++;
	}

	/*
	 * Increment flags to the next window
	 */
	window_next = 1 << (window_index);

	/*
 	 * Timer call for the next window
	 */
	cmn->timers->window_timer->flags = window_next;
	kt = ms_to_ktime(window_time_diff);
	hrtimer_forward_now(timer, kt);

	return HRTIMER_RESTART;
}

/*
 * fls_def_sensor_sampler_timer_callback()
 *	Handler for when samples of non-windowed samples expire
 */
static enum hrtimer_restart fls_def_sensor_sample_timer_callback(struct hrtimer *timer)
{
	struct fls_def_sensor_timer_data *data = container_of(timer, struct fls_def_sensor_timer_data, timer);
	struct fls_conn_cmn *cmn;
	ktime_t kt, now;
	uint32_t quotient, remainder;

	cmn = data->cmn;

	if (!(cmn->orig->flags & SFE_FLS_CONNECTION_FLAG_DEF_ENABLE) || !(cmn->reply->flags & SFE_FLS_CONNECTION_FLAG_DEF_ENABLE)) {
		FLS_INFO("%p: Defer XL or XXL event create due to stats collection disabled\n", cmn->orig);
		return HRTIMER_NORESTART;
	}

	/*
	 * The timer will trigger at a frequency of the
	 * greatest common factor between the xl and xxl
	 * windows. To determine if enough calls of this timer
	 * at a frequency of fls_def_sensor_sample_freq
	 * have occured to call either the xl or xxl timer,
	 * the window frequency is divided by the window count.
	 */
	quotient = fls_def_sensor_xl_window / data->flags;
	remainder = fls_def_sensor_xl_window % data->flags;
	if (quotient == fls_def_sensor_sample_freq && remainder == 0) {
		FLS_TRACE("xl sample timer expired %p\n", data->timer);

		/*
	 	 * No need to invoke window_close function
	 	 * since WINDOW_LG itself is used for X large samples.
		 */
		if (fls_def_sensor_burst) {
			fls_def_sensor_burst_close(&cmn->orig->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
			fls_def_sensor_burst_close(&cmn->reply->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
			FLS_TRACE("%px Sending XL window: original burst_cnt = %d, reply_cnt %d\n", cmn->orig, cmn->orig->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts,cmn->reply->stats.isd.xl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
		}

		now = ktime_get_boottime();
		FLS_WARN("%p Create XL event, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[proto = %u]", cmn->orig, now,
			cmn->orig->src_ip, ntohs(cmn->orig->src_port), cmn->orig->dest_ip, ntohs(cmn->orig->dest_port),
			cmn->orig->protocol);
		fls_def_sensor_event_create(cmn->orig, now, FLS_RFS_EVENT_TYPE_XL);
	}

	/*
	 * The timer will trigger at a frequency of the
	 * greatest common factor between the xl and xxl
	 * windows. To determine if enough calls of this timer
	 * at a frequency of fls_def_sensor_sample_freq
	 * have occured to call either the xl or xxl timer,
	 * the window frequency is divided by the window count.
	 */
	quotient = fls_def_sensor_xxl_window / data->flags;
	remainder = fls_def_sensor_xxl_window % data->flags;
	if (quotient == fls_def_sensor_sample_freq && remainder == 0) {
		FLS_TRACE("xxl sample timer expired %p\n", data->timer);

		/*
	 	 * No need to invoke window_close function
	 	 * since WINDOW_LG itself is used for XX large samples.
		 */
		if (fls_def_sensor_burst) {
			fls_def_sensor_burst_close(&cmn->orig->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
			fls_def_sensor_burst_close(&cmn->reply->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG]);
			FLS_TRACE("%px Sending XL window: original burst_cnt = %d, reply_cnt %d\n", cmn->orig, cmn->orig->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts,cmn->reply->stats.isd.xxl_sample.window[FLS_DEF_SENSOR_WINDOW_LG].bursts);
		}

		now = ktime_get_boottime();
		FLS_WARN("%p Create XXL event, t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[proto = %u]", cmn->orig, now,
			cmn->orig->src_ip, ntohs(cmn->orig->src_port), cmn->orig->dest_ip, ntohs(cmn->orig->dest_port),
			cmn->orig->protocol);
		fls_def_sensor_event_create(cmn->orig, now, FLS_RFS_EVENT_TYPE_XXL);
		data->flags = 0;
	}

	data->flags++;
	kt = ms_to_ktime(fls_def_sensor_sample_freq);
	hrtimer_forward_now(timer, kt);

	/*
	 * When HRTIMER_NORESTART is returned from the window timer
	 * the sample timer is automatically cancelled by fls_def_sensor_timer_delete
	 * thus there is no need for a norestart return value
	 */
	return HRTIMER_RESTART;
}

void fls_def_sensor_timer_delete(struct fls_conn *conn)
{
	if (!conn->cmn || !conn->cmn->timers) {
		return;
	}

	if (conn->cmn->timers->delay_timer && conn->cmn->timers->delay_timer->timer.function) {
		hrtimer_cancel(&conn->cmn->timers->delay_timer->timer);
		conn->cmn->timers->delay_timer->timer.function = NULL;
	}

	if (conn->cmn->timers->window_timer && conn->cmn->timers->window_timer->timer.function) {
		hrtimer_cancel(&conn->cmn->timers->window_timer->timer);
		conn->cmn->timers->window_timer->timer.function = NULL;
	}
	if (conn->cmn->timers->xl_xxl_timer && conn->cmn->timers->xl_xxl_timer->timer.function) {
		hrtimer_cancel(&conn->cmn->timers->xl_xxl_timer->timer);
		conn->cmn->timers->xl_xxl_timer->timer.function = NULL;
	}
	atomic_inc(&conn->fls_conn_counters[FLS_CONN_TIMER_DELETE]);
}

/*
 * fls_def_sensor_conn_rearm()
 *	Restarts default-sensor stats collection for a connection pair as if
 *	newly created. Used when a previously stopped/disabled connection is
 *	reinspected, since fls_def_sensor_timer_delete() leaves timer.function
 *	NULL and packet_cb() only (re)arms the delay timer on a zero
 *	first_packet_time.
 */
void fls_def_sensor_conn_rearm(struct fls_conn *conn)
{
	struct fls_conn *reply;
	struct fls_conn_cmn *cmn;

	/*
	 * Timer cancellation must happen outside fct.lock: hrtimer_cancel()
	 * can block waiting for a running callback, which is unsafe under a
	 * bh-spinlock.
	 */
	fls_def_sensor_timer_delete(conn);

	/*
	 * conn->reverse is only safe to read/deref under fct.lock, since a
	 * concurrent delete (e.g. via the RCU-callback teardown path) can
	 * free the reverse connection and clear this side's pointer.
	 */
	spin_lock_bh(&fct.lock);
	reply = conn->reverse;

	conn->flags = SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
	conn->stats.isd.first_packet_time = 0;
	conn->stats.isd.events = 0;
	conn->stats.isd.sample_index = 0;
	conn->stats.isd.sendevent = true;
	memset(conn->stats.isd.samples, 0, sizeof(conn->stats.isd.samples));
	memset(&conn->stats.isd.xxl_sample, 0, sizeof(conn->stats.isd.xxl_sample));
	memset(&conn->stats.isd.xl_sample, 0, sizeof(conn->stats.isd.xl_sample));

	if (reply) {
		reply->flags = SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
		reply->stats.isd.first_packet_time = 0;
		reply->stats.isd.events = 0;
		reply->stats.isd.sample_index = 0;
		reply->stats.isd.sendevent = true;
		memset(reply->stats.isd.samples, 0, sizeof(reply->stats.isd.samples));
		memset(&reply->stats.isd.xxl_sample, 0, sizeof(reply->stats.isd.xxl_sample));
		memset(&reply->stats.isd.xl_sample, 0, sizeof(reply->stats.isd.xl_sample));
	}

	/*
	 * cmn must be read and used to init timers while still holding
	 * fct.lock: a concurrent fls_conn_delete() also takes fct.lock
	 * before freeing cmn, so releasing the lock before this point would
	 * leave a window where cmn could be freed out from under us.
	 * fls_def_sensor_timer_init() only calls hrtimer_init()/atomic_inc(),
	 * neither of which blocks or re-takes fct.lock, so it is safe to
	 * call here.
	 */
	cmn = conn->cmn;
	if (cmn) {
		fls_def_sensor_timer_init(cmn->timers);
	}
	spin_unlock_bh(&fct.lock);
}

uint8_t fls_def_sensor_packet_cb(void *app_data, struct fls_conn *conn, struct sk_buff *skb)
{
	ktime_t now, kt;
	uint32_t sample_index;
	struct fls_def_sensor_sample *sample;
	struct fls_def_sensor_sample *xxl_sample;
	struct fls_def_sensor_sample *xl_sample;
	uint32_t delay = fls_def_sensor_delay;
	uint32_t sample_length = fls_def_sensor_window_sz[FLS_DEF_SENSOR_WINDOW_LG];
	struct fls_gro_frag_stats gro_stats = {0};
	int i, ret = 0;

	if (fls_def_sensor_max_events == 0 || sample_length == 0) {
		FLS_TRACE("%p Default sensor disabled.\n", conn);
		atomic_inc(&conn->fls_conn_exception_counters[FLS_CONN_EXCEPTION_DEFAULT_SENSOR_DISABLED]);
		return SFE_FLS_CONNECTION_FLAG_DEF_DISABLE;
	}

	if (unlikely(conn->flags == SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED)) {
		FLS_TRACE("%p HWM exceeded, Statistics disabled.\n", conn);

		/*
		 * Kill any active timers
		 */
		fls_def_sensor_timer_delete(conn);
		atomic_inc(&conn->fls_conn_exception_counters[FLS_CONN_SENSOR_HWM_EXCEEDED]);

		return SFE_FLS_CONNECTION_FLAG_HWM_EXCEEDED;
	}

	if (!(conn->flags & SFE_FLS_CONNECTION_FLAG_DEF_ENABLE)) {
		FLS_TRACE("%p Statistics disabled.\n", conn);

		/*
		 * Kill any active timers
		 */
		fls_def_sensor_timer_delete(conn);
		atomic_inc(&conn->fls_conn_exception_counters[FLS_CONN_EXCEPTION_DEFAULT_SENSOR_DISABLED]);

		return SFE_FLS_CONNECTION_FLAG_DEF_DISABLE;
	}

	if(conn->externalrule) {
		now = skb->tstamp;
		skb->len -= 14;
	} else {
		now = ktime_get_boottime();
	}

	/*
	 * If check will trigger delay timer when first packet is recieved
	 */
	if (!conn->stats.isd.first_packet_time) {
		FLS_WARN("%p First packet. t = %lld, [%pI4:%hu -> %pI4:%hu] IP Header[id = %u, proto = %u]", conn, now,
			conn->src_ip, ntohs(conn->src_port), conn->dest_ip, ntohs(conn->dest_port), ntohs(ip_hdr(skb)->id),
			ip_hdr(skb)->protocol);
		conn->stats.isd.first_packet_time = now;
		conn->reverse->stats.isd.first_packet_time = now;
		fls_debug_print_conn_info(conn);

		kt = ms_to_ktime(delay);
		hrtimer_start(&conn->cmn->timers->delay_timer->timer, kt, HRTIMER_MODE_REL);
	}

	/*
	 * Bail from the function if the delay period has not passed
	 */
	if (!(conn->flags & SFE_FLS_CONNECTION_FLAG_DELAY_FINISHED)) {
		return SFE_FLS_CONNECTION_FLAG_DEF_ENABLE;
	}

	sample_index = conn->stats.isd.sample_index;
	sample = &(conn->stats.isd.samples[sample_index]);
	xl_sample = &(conn->stats.isd.xl_sample);
	xxl_sample = &(conn->stats.isd.xxl_sample);

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
			FLS_WARN("%p: Failed to traverse GRO skb, continue with head skb\n", skb);
		}

		FLS_TRACE("Minimum fragment size: %u\n", gro_stats.min_bytes);
		FLS_TRACE("Maximum fragment size: %u\n", gro_stats.max_bytes);
	}

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

/*
 * fls_def_sensor_timer_init
 *	Initializes common timers used by fls def sensor callback
 */
void fls_def_sensor_timer_init(struct fls_def_sensor_timers *timers)
{
	struct fls_conn_cmn *cmn = timers->delay_timer->cmn;

	/*
	 * Initialize delay timer
	 */
	hrtimer_init(&timers->delay_timer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timers->delay_timer->timer.function = &fls_def_sensor_delay_timer_callback;

	/*
	 * Initialize standard window timer
	 */
	hrtimer_init(&timers->window_timer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timers->window_timer->timer.function = &fls_def_sensor_window_timer_callback;

	/*
	 * Initialize large window timer
	 */
	hrtimer_init(&timers->xl_xxl_timer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timers->xl_xxl_timer->timer.function = &fls_def_sensor_sample_timer_callback;

	atomic_inc(&cmn->orig->fls_conn_counters[FLS_CONN_TIMER_INIT]);
	atomic_inc(&cmn->reply->fls_conn_counters[FLS_CONN_TIMER_INIT]);
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
