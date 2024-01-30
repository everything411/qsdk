/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 */

#include "../inc/telemetry_agent_sawf.h"

struct telemetry_sawf_ctx *g_telemetry_sawf;

int telemetry_sawf_init_ctx(void)
{
	if (g_telemetry_sawf) {
		pr_sawf_err("SAWF ctx alloc failed");
		return -1;
	}

	g_telemetry_sawf = kzalloc(sizeof(struct telemetry_sawf_ctx),
				   GFP_ATOMIC);
	if (!g_telemetry_sawf) {
		pr_sawf_err("SAWF ctx alloc failed");
		return -1;
	}
	pr_sawf_dbg("SAWF ctx is initialized");
	INIT_LIST_HEAD(&(g_telemetry_sawf->peer_list));
	spin_lock_init(&g_telemetry_sawf->peer_list_lock);
	return 0;
}

void telemetry_sawf_free_ctx(void)
{
	telemetry_sawf_free_sla_timer();

	kfree(g_telemetry_sawf);
	g_telemetry_sawf = NULL;

	pr_sawf_dbg("Agent SAWF context is freed");
}

struct telemetry_sawf_ctx *telemetry_sawf_get_ctx(void)
{
	if (g_telemetry_sawf)
		return g_telemetry_sawf;
	else
		return NULL;
}

int telemetry_sawf_set_sla_detect_cfg(uint8_t type,
				      uint8_t min_thruput_rate,
				      uint8_t max_thruput_rate,
				      uint8_t burst_size,
				      uint8_t service_interval,
				      uint8_t delay_bound,
				      uint8_t msdu_ttl,
				      uint8_t msdu_rate_loss)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_sla_param *sla_detect;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	if (type >= SAWF_SLA_DETECT_MAX) {
		pr_sawf_err("Wrong SLA Detect Type");
		return -1;
	}

	sla_detect = &sawf_ctx->sla_detect.sla_detect[type];

	sla_detect->min_thruput_rate = min_thruput_rate;
	sla_detect->max_thruput_rate = max_thruput_rate;
	sla_detect->burst_size = burst_size;
	sla_detect->service_interval = service_interval;
	sla_detect->delay_bound = delay_bound;
	sla_detect->msdu_ttl = msdu_ttl;
	sla_detect->msdu_rate_loss = msdu_rate_loss;

	pr_sawf_dbg("type %d", type);
	pr_sawf_dbg("detect->min_thruput_rate:%d",
		    sla_detect->min_thruput_rate);
	pr_sawf_dbg("detect->max_thruput_rate:%d",
		    sla_detect->max_thruput_rate);
	pr_sawf_dbg("detect->burst_size:%d", sla_detect->burst_size);
	pr_sawf_dbg("detect->service_interval:%d",
		    sla_detect->service_interval);
	pr_sawf_dbg("detect->delay_bound:%d", sla_detect->delay_bound);
	pr_sawf_dbg("detect->msdu_ttl:%d", sla_detect->msdu_ttl);
	pr_sawf_dbg("detect->msdu_rate_loss:%d", sla_detect->msdu_rate_loss);

	return 0;
}

int telemetry_sawf_set_svclass_cfg(bool enable,
				   uint8_t svc_id,
				   uint32_t min_thruput_rate,
				   uint32_t max_thruput_rate,
				   uint32_t burst_size,
				   uint32_t service_interval,
				   uint32_t delay_bound,
				   uint32_t msdu_ttl,
				   uint32_t msdu_rate_loss)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_svc_class_param *svc_class;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	if (svc_id < SAWF_MIN_SVC_CLASS || svc_id > SAWF_MAX_SVC_CLASS) {
		pr_sawf_err("Invalid svc_id %u", svc_id);
		return -1;
	}

	svc_class =  &sawf_ctx->svc_class.svc_class[svc_id - 1];

	if (enable) {
		svc_class->min_thruput_rate = min_thruput_rate;
		svc_class->max_thruput_rate = max_thruput_rate;
		svc_class->burst_size = burst_size;
		svc_class->service_interval = service_interval;
		svc_class->delay_bound = delay_bound;
		svc_class->msdu_ttl = msdu_ttl;
		svc_class->msdu_rate_loss = msdu_rate_loss;
		set_bit(svc_id, sawf_ctx->svc_class.service_ids);
	} else {
		memset(svc_class, 0,
		       sizeof(struct telemetry_sawf_svc_class_param));
		clear_bit(svc_id, sawf_ctx->svc_class.service_ids);
	}
	return 0;
}

int telemetry_sawf_set_sla_cfg(uint8_t svc_id,
			       uint8_t min_thruput_rate,
			       uint8_t max_thruput_rate,
			       uint8_t burst_size,
			       uint8_t service_interval,
			       uint8_t delay_bound,
			       uint8_t msdu_ttl,
			       uint8_t msdu_rate_loss)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_sla_param *sla_cfg;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	if (svc_id < SAWF_MIN_SVC_CLASS || svc_id > SAWF_MAX_SVC_CLASS) {
		pr_sawf_err("Invalid svc_id %u", svc_id);
		return -1;
	}
	sla_cfg =  &sawf_ctx->sla.sla_cfg[svc_id - 1];

	sla_cfg->min_thruput_rate = min_thruput_rate;
	sla_cfg->max_thruput_rate = max_thruput_rate;
	sla_cfg->burst_size = burst_size;
	sla_cfg->service_interval = service_interval;
	sla_cfg->delay_bound = delay_bound;
	sla_cfg->msdu_ttl = msdu_ttl;
	sla_cfg->msdu_rate_loss = msdu_rate_loss;

	set_bit(svc_id, sawf_ctx->sla.sla_service_ids);

	pr_sawf_dbg("Service ID: %d", svc_id);
	pr_sawf_dbg("cfg->min_thruput_rate %d", sla_cfg->min_thruput_rate);
	pr_sawf_dbg("cfg->max_thruput_rate %d", sla_cfg->max_thruput_rate);
	pr_sawf_dbg("cfg->burst_size %d", sla_cfg->burst_size);
	pr_sawf_dbg("cfg->service_interval %d", sla_cfg->service_interval);
	pr_sawf_dbg("cfg->delay_bound %d", sla_cfg->delay_bound);
	pr_sawf_dbg("cfg->msdu_ttl %d", sla_cfg->msdu_ttl);
	pr_sawf_dbg("cfg->msdu_rate_loss %d", sla_cfg->msdu_rate_loss);
	return 0;
}

int telemetry_sawf_set_mov_avg_params(uint32_t packet,
				      uint32_t window)
{
	struct telemetry_sawf_ctx *sawf_ctx;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	sawf_ctx->mov_avg.packet = packet;
	sawf_ctx->mov_avg.window = window;

	pr_sawf_dbg("sawf_ctx->mov_avg.packet %d", sawf_ctx->mov_avg.packet);
	pr_sawf_dbg("sawf_ctx->mov_avg.window %d", sawf_ctx->mov_avg.window);
	return 0;
}

int telemetry_sawf_set_sla_params(uint32_t num_packet,
				  uint32_t time_secs)
{
	struct telemetry_sawf_ctx *sawf_ctx;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	sawf_ctx->sla_param.num_packets = num_packet;
	sawf_ctx->sla_param.time_secs = time_secs;

	pr_sawf_dbg("sawf_ctx->sla_param.num_packets%d",
		    sawf_ctx->sla_param.num_packets);
	pr_sawf_dbg("sawf_ctx->sla_param.time_secs%d",
		    sawf_ctx->sla_param.time_secs);

	return 0;
}

struct telemetry_sawf_mov_avg_params *
telemetry_sawf_get_mov_avg_param(void)
{
	struct telemetry_sawf_ctx *sawf_ctx;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return NULL;
	}

	return &(sawf_ctx->mov_avg);
}

struct telemetry_sawf_sla_params *
telemetry_sawf_get_sla_param(void)
{
	struct telemetry_sawf_ctx *sawf_ctx;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err(" SAWF context is NULL");
		return NULL;
	}

	return &(sawf_ctx->sla_param);
}

uint64_t *telemtry_sawf_delay_window_alloc(void)
{
	struct telemetry_sawf_mov_avg_params *mov_avg_param;
	uint64_t *mov_window_ptr;
	uint32_t window_size;

	mov_avg_param = telemetry_sawf_get_mov_avg_param();
	if (!mov_avg_param) {
		pr_sawf_err("Moving average param is null");
		return NULL;
	}

	/*
	 * The moving_window stats is maintained for all windows
	 * per queue for the 3 different delay-types.
	 */
	window_size = mov_avg_param->window * sizeof(uint64_t) *
		      SAWF_MAX_QUEUES * SAWF_MAX_LATENCY_TYPE;

	mov_window_ptr = kzalloc(window_size, GFP_ATOMIC);
	if (mov_window_ptr)
		return mov_window_ptr;
	else
		return NULL;
}

int telemtry_sawf_delay_window_free(uint64_t *mov_window)
{
	kfree(mov_window);
	return 0;
}

void *telemetry_sawf_alloc_peer(void *soc, void *sawf_stats_ctx,
				uint8_t *mac_addr,
				uint8_t svc_id, uint8_t host_q_id)
{
	struct telemetry_sawf_peer_ctx *peer_ctx;
	uint64_t *mov_window;
	struct peer_sawf_queue *peer_msduq;

	peer_ctx = kzalloc(sizeof(struct telemetry_sawf_peer_ctx), GFP_ATOMIC);
	if (!peer_ctx) {
		pr_sawf_err("peer context allocation failed");
		return NULL;
	}

	mov_window = telemtry_sawf_delay_window_alloc();
	if (!mov_window) {
		pr_sawf_err("windows allocation failed");
		kfree(peer_ctx);
		return NULL;
	}

	pr_sawf_dbg("Alloc peer ctx %pK for host QID:%d and service ID:%d",
		    peer_ctx, host_q_id, svc_id);

	peer_ctx->mov_window = mov_window;
	peer_ctx->sawf_ctx = sawf_stats_ctx;
	peer_ctx->soc = soc;
	memcpy(peer_ctx->mac_addr, mac_addr, MAC_ADDR_SIZE);

	peer_msduq = &(peer_ctx->peer_msduq);
	peer_msduq->msduq[host_q_id].svc_id = svc_id;

	set_bit(host_q_id, peer_msduq->active_queue);

	spin_lock_bh(&g_telemetry_sawf->peer_list_lock);
	if (list_empty(&g_telemetry_sawf->peer_list))
		telemetry_sawf_init_sla_timer();

	list_add_tail(&peer_ctx->node, &g_telemetry_sawf->peer_list);
	spin_unlock_bh(&g_telemetry_sawf->peer_list_lock);

	return peer_ctx;
}

int telemetry_sawf_update_queue_info(void *telemetry_ctx, uint8_t host_q_id,
				     uint8_t tid, uint8_t msduq)
{
	struct telemetry_sawf_peer_ctx *peer_ctx;
	struct peer_sawf_queue *peer_msduq;

	if (!telemetry_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	peer_ctx = telemetry_ctx;
	peer_msduq = &(peer_ctx->peer_msduq);

	peer_msduq->msduq[host_q_id].htt_msduq = msduq;
	peer_msduq->msduq[host_q_id].tid = tid;
	peer_msduq->msduq_map[tid][msduq] = host_q_id;

	set_bit(host_q_id, peer_msduq->active_queue);
	pr_sawf_info("Host Q Idx: %d | TID: %d | Queue ID: %d|",
		     peer_msduq->msduq[host_q_id].htt_msduq,
		     peer_msduq->msduq[host_q_id].tid = tid,
		     peer_msduq->msduq_map[tid][msduq]);

	return 0;
}

void telemetry_sawf_free_peer(void *telemetry_ctx)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;

	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return;
	}
	spin_lock_bh(&g_telemetry_sawf->peer_list_lock);
	list_del(&(peer_ctx->node));

	if (list_empty(&g_telemetry_sawf->peer_list))
		telemetry_sawf_free_sla_timer();

	spin_unlock_bh(&g_telemetry_sawf->peer_list_lock);
	pr_sawf_dbg("Free SAWF ctx %pK", peer_ctx);
	telemtry_sawf_delay_window_free(peer_ctx->mov_window);
	peer_ctx->mov_window = NULL;

	kfree(peer_ctx);
}

static int
telemetry_get_tput_stats(struct telemetry_sawf_peer_ctx *peer,
			 uint64_t *in_bytes, uint64_t *in_cnt,
			 uint64_t *tx_bytes, uint64_t *tx_cnt,
			 uint8_t svc_id, uint8_t queue)
{
	wlan_sawf_get_tput_stats(peer->soc, peer->sawf_ctx, in_bytes, in_cnt,
				 tx_bytes, tx_cnt, 0, queue);
	return 0;
}

static int
telemetry_get_drop_stats_num_sec(struct telemetry_sawf_peer_ctx *peer,
				    uint64_t *pass, uint64_t *drop,
				    uint64_t *drop_ttl,
				    uint8_t svc_id,
				    uint8_t queue)
{
	wlan_sawf_get_drop_stats(peer->soc, peer->sawf_ctx,
				 pass, drop, drop_ttl, 0, queue);
	return 0;
}

static int
telemetry_get_mpdu_stats_num_sec(struct telemetry_sawf_peer_ctx *peer,
				 uint64_t *svc_int_pass, uint64_t *svc_int_fail,
				 uint64_t *burst_pass, uint64_t *burst_fail,
				 uint8_t svc_id, uint8_t queue)
{
	wlan_sawf_get_mpdu_stats(peer->soc, peer->sawf_ctx,
				 svc_int_pass, svc_int_fail,
				 burst_pass, burst_fail, 0, queue);
	return 0;
}

static int telemetry_notify_breach(struct telemetry_sawf_peer_ctx *peer,
				   uint8_t svc_id,
				   uint8_t queue,
				   bool set_clear,
				   enum telemetry_sawf_param param)
{
	struct telemetry_sawf_stats *stats;
	struct sawf_msduq *msduq;
	uint8_t tid;

	stats = &peer->stats[queue];

	if (stats->breach[param] == set_clear)
		return 0;

	msduq = &peer->peer_msduq.msduq[queue];
	tid = msduq->tid;
	wlan_sawf_notify_breach(peer->mac_addr, svc_id, param, set_clear, tid);

	stats->breach[param] = set_clear;

	return 0;
}

static int
telemetry_detect_breach_peer_delay_num_pkt(struct telemetry_sawf_peer_ctx *peer,
					   uint8_t svc_id, uint8_t queue,
					   uint64_t pass, uint64_t fail)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_svc_class_param *svc_param;
	struct telemetry_sawf_sla_param *sla_param;
	struct telemetry_sawf_sla_param *sla_detect;
	uint32_t threshold, pass_p;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}
	svc_param = &sawf_ctx->svc_class.svc_class[svc_id - 1];
	sla_param = &sawf_ctx->sla.sla_cfg[svc_id - 1];
	sla_detect = &sawf_ctx->sla_detect.sla_detect[SAWF_SLA_DETECT_NUM_PACKET];

	threshold = sla_param->delay_bound;
	if (threshold && sla_detect->delay_bound) {
		pass_p = div_u64((pass * 100), (pass + fail));
		if (pass_p < threshold) {
			pr_sawf_err("SLA Delay Breach(num_pkt) Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("Delay threshold: %d", threshold);
			pr_sawf_err("Delay Success: %lld", pass);
			pr_sawf_err("Delay Failure: %lld", fail);
			pr_sawf_err("Pass Percentage: %d", pass_p);
			telemetry_notify_breach(peer, svc_id, queue,
						true, SAWF_PARAM_DELAY_BOUND);
		} else {
			telemetry_notify_breach(peer, svc_id, queue,
						false, SAWF_PARAM_DELAY_BOUND);
		}

	}

	return 0;
}

static int
telemetry_detect_breach_peer_drop_num_pkt(struct telemetry_sawf_peer_ctx *peer,
				     uint8_t svc_id, uint8_t queue,
				     uint64_t total, uint64_t drop,
				     uint64_t drop_ttl)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_svc_class_param *svc_param;
	struct telemetry_sawf_sla_param *sla_param;
	struct telemetry_sawf_sla_param *sla_detect;
	uint32_t threshold, pass_p, num_drops;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}
	svc_param = &sawf_ctx->svc_class.svc_class[svc_id - 1];
	sla_param = &sawf_ctx->sla.sla_cfg[svc_id - 1];
	sla_detect = &sawf_ctx->sla_detect.
		     sla_detect[SAWF_SLA_DETECT_NUM_PACKET];

	threshold = sla_param->msdu_ttl;
	if (threshold && sla_detect->msdu_ttl) {
		pass_p = div_u64(((total - drop_ttl) * 100), total);
		if (pass_p < threshold) {
			pr_sawf_err("SLA MSDU TTL Loss Breach(num_pkt) Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("MSDU TTL Drop threshold: %d", threshold);
			pr_sawf_err("MSDU TTL Drop Success: %lld", total);
			pr_sawf_err("MSDU TTL Drop Failure: %lld", drop_ttl);
			pr_sawf_err("Pass Percentage: %d", pass_p);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
						SAWF_PARAM_MSDU_TTL);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
						SAWF_PARAM_MSDU_TTL);
		}
	}

	threshold = sla_param->msdu_rate_loss;
	if (threshold && sla_detect->msdu_rate_loss) {
		threshold = sla_param->msdu_rate_loss;
		/* Number of packets drop allowed */
		num_drops = svc_param->msdu_rate_loss * MSDU_LOSS_UNIT * total;
		if (num_drops && div_u64((drop * 100), num_drops) < threshold) {
			pr_sawf_err("SLA MSDU Loss Breach(num_pkt) Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("MSDU Drop Threshold: %d", threshold);
			pr_sawf_err("MSDU Drop Max Count: %d", num_drops);
			pr_sawf_err("MSDU Drop Count: %lld", drop);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
						SAWF_PARAM_MSDU_LOSS);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
						SAWF_PARAM_MSDU_LOSS);
		}
	}
	return 0;
}

static int
telemetry_detect_breach_peer_mov_avg(struct telemetry_sawf_peer_ctx *peer,
				     uint8_t svc_id, uint8_t queue,
				     uint64_t mov_avg_delay)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_svc_class_param *svc_param;
	struct telemetry_sawf_sla_param *sla_param;
	struct telemetry_sawf_sla_param *sla_detect;
	uint32_t delay_bound;
	uint32_t threshold;
	uint32_t max_delay;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}
	svc_param = &sawf_ctx->svc_class.svc_class[svc_id - 1];
	sla_param = &sawf_ctx->sla.sla_cfg[svc_id - 1];
	sla_detect = &sawf_ctx->sla_detect.sla_detect[SAWF_SLA_DETECT_MOV_AVG];

	threshold = sla_param->delay_bound;
	if (threshold && sla_detect->delay_bound) {
		delay_bound = svc_param->delay_bound * DELAY_BOUND_UNIT;
		max_delay = delay_bound + div_u64((delay_bound *(100 - threshold)),100) ;
		if (mov_avg_delay > max_delay) {
			pr_sawf_err("SLA Delay Breach Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("Configured Delay Bound: %d", delay_bound);
			pr_sawf_err("Delay Moving Average: %lld", mov_avg_delay);
			pr_sawf_err("Configured Delay Bound thresd: %d", threshold);
			pr_sawf_err("Max Delay Bound threshold:%d", max_delay);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
						SAWF_PARAM_DELAY_BOUND);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
						SAWF_PARAM_DELAY_BOUND);
		}
	}

	return 0;
}

static int
telemetry_detect_breach_peer_per_sec(struct telemetry_sawf_peer_ctx *peer,
				     uint8_t svc_id,
				     uint8_t queue)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_throughput *tput;
	struct telemetry_sawf_svc_class_param *svc_param;
	struct telemetry_sawf_sla_param *sla_param;
	struct telemetry_sawf_sla_param *sla_detect;
	uint64_t in_bytes = 0, in_cnt = 0;
	uint64_t tx_bytes = 0, tx_cnt = 0;
	uint64_t in_rate = 0, eg_rate = 0;
	uint32_t throughput;
	uint32_t threshold;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}
	svc_param = &sawf_ctx->svc_class.svc_class[svc_id - 1];
	sla_param = &sawf_ctx->sla.sla_cfg[svc_id - 1];
	sla_detect = &sawf_ctx->sla_detect.
		     sla_detect[SAWF_SLA_DETECT_PER_SECOND];

	telemetry_get_tput_stats(peer, &in_bytes, &in_cnt,
				 &tx_bytes, &tx_cnt, svc_id, queue);

	tput = &peer->stats[queue].tx.throughput;
	in_rate = in_bytes - tput->last_in_bytes;
	eg_rate = tx_bytes - tput->last_bytes;

	tput->last_in_bytes = in_bytes;
	tput->last_bytes = tx_bytes;
	tput->in_rate = in_rate;
	tput->eg_rate = eg_rate;

	if (!test_bit(svc_id, sawf_ctx->sla.sla_service_ids))
		return 0;

	threshold = sla_param->min_thruput_rate;
	if (threshold && sla_detect->min_thruput_rate) {
		throughput = svc_param->min_thruput_rate * THROUGHPUT_UNIT;
		if (tput->in_rate > throughput &&
		    tput->eg_rate < div_u64((throughput * threshold), 100)) {
			pr_sawf_err("SLA Min Throughput Breach Detected peer service ID:%d",
				    svc_id);
			pr_sawf_err("Configured Min Throughput: %d", throughput);
			pr_sawf_err("Ingres Throughput: %d", tput->in_rate);
			pr_sawf_err("Egress Throughput: %d", tput->eg_rate);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
					        SAWF_PARAM_MIN_THROUGHPUT);
		}
	}
	threshold = sla_param->max_thruput_rate;
	if (threshold && sla_detect->max_thruput_rate) {
		throughput = sla_param->max_thruput_rate * THROUGHPUT_UNIT;
		if (in_rate > svc_param->max_thruput_rate &&
		    eg_rate < div_u64((throughput * threshold), 100)) {
			pr_sawf_err("SLA Max Throughput Breach Detected peer service ID:%d",
				    svc_id);
			pr_sawf_err("Configured Max Throughput: %d", throughput);
			pr_sawf_err("Ingres Throughput: %d", tput->in_rate);
			pr_sawf_err("Egress Throughput: %d", tput->eg_rate);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
					        SAWF_PARAM_MAX_THROUGHPUT);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
					        SAWF_PARAM_MAX_THROUGHPUT);
		}
	}

	return 0;
}

static int
telemetry_detect_breach_peer_num_sec(struct telemetry_sawf_peer_ctx *peer,
				     uint8_t svc_id, uint8_t queue)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_drops *msdu_drop;
	struct telemetry_service_interval *svc_int;
	struct telemetry_burst_size *burst;
	struct telemetry_sawf_svc_class_param *svc_param;
	struct telemetry_sawf_sla_param *sla_param;
	struct telemetry_sawf_sla_param *sla_detect;
	uint32_t threshold;

	uint64_t svc_int_pass = 0, svc_int_fail = 0;
	uint64_t burst_pass = 0, burst_fail = 0;
	uint64_t pass = 0, drop = 0, drop_ttl = 0;

	uint64_t curr_svc_int_pass = 0, curr_svc_int_fail = 0;
	uint64_t curr_burst_pass = 0, curr_burst_fail = 0;
	uint64_t curr_total = 0, curr_drop = 0, curr_drop_ttl = 0;
	uint64_t total;
	uint32_t pass_p, num_drops;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}
	svc_param = &sawf_ctx->svc_class.svc_class[svc_id - 1];
	sla_param = &sawf_ctx->sla.sla_cfg[svc_id - 1];
	sla_detect = &sawf_ctx->sla_detect.
		     sla_detect[SAWF_SLA_DETECT_NUM_SECOND];

	telemetry_get_mpdu_stats_num_sec(peer,
					 &svc_int_pass, &svc_int_fail,
					 &burst_pass, &burst_fail,
					 svc_id, queue);

	burst = &peer->stats[queue].tx.burst_size;
	curr_burst_pass = burst_pass - burst->last_success;
	curr_burst_fail = burst_fail - burst->last_failure;

	svc_int = &peer->stats[queue].tx.service_interval;
	curr_svc_int_pass = svc_int_pass - svc_int->last_success;
	curr_svc_int_fail = svc_int_fail - svc_int->last_failure;

	burst->last_success = burst_pass;
	burst->last_failure = burst_fail;
	svc_int->last_success = svc_int_pass;
	svc_int->last_failure = svc_int_fail;

	telemetry_get_drop_stats_num_sec(peer,
					 &pass, &drop, &drop_ttl,
					 svc_id,
					 queue);

	msdu_drop = &peer->stats[queue].tx.msdu_drop;
	curr_total = pass - msdu_drop->last_xmit_cnt_seconds;
	curr_drop = drop - msdu_drop->last_drop_cnt_seconds;
	curr_drop_ttl = drop_ttl - msdu_drop->last_ttl_drop_cnt_seconds;

	msdu_drop->last_xmit_cnt_seconds = pass;
	msdu_drop->last_drop_cnt_seconds = drop;
	msdu_drop->last_ttl_drop_cnt_seconds = drop_ttl;

	if (!test_bit(svc_id, sawf_ctx->sla.sla_service_ids))
		return 0;

	threshold = sla_param->burst_size;
	if (threshold && sla_detect->burst_size) {
		total = curr_burst_pass + curr_burst_fail;
		pass_p = div_u64(((total - curr_burst_fail) * 100), total);
		if (curr_svc_int_fail && pass_p < threshold) {
			pr_sawf_err("SLA Burst Size(per_x_sec) Breach Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("Burst Size threshold: %d", threshold);
			pr_sawf_err("Burst Size success: %lld", curr_burst_pass);
			pr_sawf_err("Burst Size Failure: %lld", curr_burst_fail);
			pr_sawf_err("Pass Percentage: %d", pass_p);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
					        SAWF_PARAM_BURST_SIZE);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
					        SAWF_PARAM_BURST_SIZE);
		}
	}

	threshold = sla_param->service_interval;
	if (threshold && sla_detect->service_interval) {
		total = curr_svc_int_pass + curr_svc_int_fail;
		pass_p = div_u64(((total - curr_svc_int_fail) * 100), total);
		if (curr_svc_int_fail && pass_p < threshold) {
			pr_sawf_err("SLA Service Interval(per_s_sec) Breach Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("Service Interval threshold: %d", threshold);
			pr_sawf_err("Service Interval success: %lld", curr_svc_int_pass);
			pr_sawf_err("Service Interval Failure: %lld", curr_svc_int_fail);
			pr_sawf_err("Pass Percentage: %d", pass_p);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
					        SAWF_PARAM_SERVICE_INTERVAL);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
					        SAWF_PARAM_SERVICE_INTERVAL);
		}
	}

	threshold = sla_param->msdu_ttl;
	if (threshold && sla_detect->msdu_ttl) {
		pass_p = div_u64(((curr_total - curr_drop_ttl) * 100), curr_total);
		if (curr_drop_ttl && pass_p < threshold) {
			pr_sawf_err("SLA MSDU TTL(per_x_sec) Breach Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("MSDU TTL Drop threshold: %d", threshold);
			pr_sawf_err("MSDU TTL Drop Success: %lld", curr_total);
			pr_sawf_err("MSDU TTL Drop Failure: %lld", curr_drop_ttl);
			pr_sawf_err("Pass Percentage: %d", pass_p);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
					        SAWF_PARAM_MSDU_TTL);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
					        SAWF_PARAM_MSDU_TTL);
		}
	}

	threshold = sla_param->msdu_rate_loss;
	if (threshold && sla_detect->msdu_rate_loss) {
		num_drops = svc_param->msdu_rate_loss * MSDU_LOSS_UNIT * curr_total;
		if (num_drops && div_u64((curr_drop * 100), num_drops) < threshold) {
			pr_sawf_err("SLA MSDU Loss(per_x_sec) Breach Detected peer service ID: %d",
				    svc_id);
			pr_sawf_err("MSDU Drop Threshold: %d", threshold);
			pr_sawf_err("MSDU Drop Max Count: %d", num_drops);
			pr_sawf_err("MSDU Drop Count: %lld", curr_drop);
			telemetry_notify_breach(peer, svc_id,
						queue, true,
						SAWF_PARAM_MSDU_LOSS);
		} else {
			telemetry_notify_breach(peer, svc_id,
						queue, false,
						SAWF_PARAM_MSDU_LOSS);
		}

	}
	return 0;
}

static int
telemetry_detect_breach_peer(struct telemetry_sawf_peer_ctx *peer,
			     enum telemetry_sawf_sla_detect_type type)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct peer_sawf_queue *peer_queue;
	uint8_t bit, svc_id;

	if (!peer) {
		pr_sawf_err("Peer ctx is null");
		return -1;
	}

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}

	peer_queue = &peer->peer_msduq;
	for_each_set_bit(bit, peer_queue->active_queue, SAWF_MAX_QUEUES) {
		svc_id = peer_queue->msduq[bit].svc_id;

		switch (type) {
		case SAWF_SLA_DETECT_PER_SECOND:
			telemetry_detect_breach_peer_per_sec(peer, svc_id,
							     bit);
			break;
		case SAWF_SLA_DETECT_NUM_SECOND:
			telemetry_detect_breach_peer_num_sec(peer, svc_id,
							     bit);
			break;
		default:
			break;
		}
	}

	return 0;
}

static int telemetry_detect_breach(enum telemetry_sawf_sla_detect_type type)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct list_head *node;
	struct telemetry_sawf_peer_ctx *peer;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}

	spin_lock_bh(&g_telemetry_sawf->peer_list_lock);
	list_for_each(node, &sawf_ctx->peer_list) {
		peer = list_entry(node, struct telemetry_sawf_peer_ctx, node);
		if (!peer) {
			pr_sawf_err("Peer ctx is null");
			continue;
		}

		switch (type) {
		case SAWF_SLA_DETECT_PER_SECOND:
			telemetry_detect_breach_peer(peer, SAWF_SLA_DETECT_PER_SECOND);
			break;
		case SAWF_SLA_DETECT_NUM_SECOND:
			telemetry_detect_breach_peer(peer, SAWF_SLA_DETECT_NUM_SECOND);
			break;
		default:
			break;
		}
	}
	spin_unlock_bh(&g_telemetry_sawf->peer_list_lock);
	return 0;
}

int telemetry_sawf_update_peer_delay(void *telemetry_ctx, uint8_t tid,
				     uint8_t queue, uint64_t pass,
				     uint64_t fail)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;
	uint64_t curr_success, curr_failure;
	struct telemetry_sawf_stats *stats;
	struct sawf_msduq *msduq;

	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	stats = &peer_ctx->stats[queue];

	curr_success = pass - stats->delay.last_success;
	curr_failure = fail - stats->delay.last_failure;

	stats->delay.last_success = pass;
	stats->delay.last_failure = fail;

	msduq = &peer_ctx->peer_msduq.msduq[queue];
	telemetry_detect_breach_peer_delay_num_pkt(peer_ctx, msduq->svc_id,
						   queue, curr_success,
						   curr_failure);

	return 0;
}

static inline
void telemetry_get_mov_window_offset(void *telemetry_ctx, uint8_t queue,
				     uint32_t curr_win, uint64_t **nw_delay,
				     uint64_t **sw_delay, uint64_t **hw_delay)
{
	struct telemetry_sawf_peer_ctx *peer_ctx;
	uint32_t max_win;
	uint64_t *mov_win_start;
	uint64_t *queue_offset;

	peer_ctx = telemetry_ctx;
	max_win = g_telemetry_sawf->mov_avg.window;
	mov_win_start = peer_ctx->mov_window;

	/*
	 * Stats for all windows for each delay-type is stored sequentially
	 * per queue.First reach the start of stats for each delay-type and
	 * then find the slot for the corresponding window.
	 */
	queue_offset = mov_win_start + (queue * max_win);
	*nw_delay = queue_offset + curr_win;

	queue_offset = mov_win_start + (SAWF_MAX_QUEUES * max_win);
	*sw_delay = queue_offset + curr_win;

	queue_offset = mov_win_start + ((SAWF_MAX_LATENCY_TYPE - 1) *
					SAWF_MAX_QUEUES * max_win);
	*hw_delay = queue_offset + curr_win;
}

int telemetry_sawf_update_peer_delay_mov_avg(void *telemetry_ctx,
					     uint8_t tid, uint8_t queue,
					     uint64_t nwdelay_avg,
					     uint64_t swdelay_avg,
					     uint64_t hwdelay_avg)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;
	struct telemetry_sawf_stats *stats;
	uint32_t max_win;
	uint64_t *nwdelay_qwin, *swdelay_qwin, *hwdelay_qwin;
	struct telemetry_sawf_delay *delay;
	uint32_t curr_win;
	struct sawf_msduq *msduq;

	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	stats = &peer_ctx->stats[queue];
	delay = &stats->delay;
	max_win = g_telemetry_sawf->mov_avg.window;

	if (delay->cur_window < max_win) {
		curr_win = delay->cur_window;
		telemetry_get_mov_window_offset(telemetry_ctx, queue, curr_win,
						&nwdelay_qwin, &swdelay_qwin,
						&hwdelay_qwin);
		*nwdelay_qwin = nwdelay_avg;
		*swdelay_qwin = swdelay_avg;
		*hwdelay_qwin = hwdelay_avg;
		delay->nwdelay_win_total = delay->nwdelay_win_total +
					   nwdelay_avg;
		delay->swdelay_win_total = delay->swdelay_win_total +
					   swdelay_avg;
		delay->hwdelay_win_total = delay->hwdelay_win_total +
					   hwdelay_avg;
		delay->cur_window++;
	} else if (delay->cur_window == max_win) {
		delay->nwdelay_avg = div_u64(delay->nwdelay_win_total, max_win);
		delay->swdelay_avg = div_u64(delay->swdelay_win_total, max_win);
		delay->hwdelay_avg = div_u64(delay->hwdelay_win_total, max_win);
		//Detect moving average SLA
		delay->cur_window = 0;
		delay->nwdelay_win_total = 0;
		delay->swdelay_win_total = 0;
		delay->hwdelay_win_total = 0;
		msduq = &peer_ctx->peer_msduq.msduq[queue];
		telemetry_detect_breach_peer_mov_avg(peer_ctx, msduq->svc_id,
						     queue, delay->hwdelay_avg);
	}

	return 0;
}

int telemetry_sawf_update_msdu_drop(void *telemetry_ctx,
				    uint8_t tid,
				    uint8_t queue, uint64_t total,
				    uint64_t fail_drop, uint64_t fail_ttl)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;
	struct telemetry_sawf_stats *stats;
	struct telemetry_sawf_drops *msdu_drop;
	uint64_t curr_total, curr_fail, curr_fail_ttl;
	struct sawf_msduq *msduq;

	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	stats = &peer_ctx->stats[queue];
	msdu_drop = &stats->tx.msdu_drop;

	curr_total = total - msdu_drop->last_xmit_cnt_packets;
	curr_fail = fail_drop - msdu_drop->last_drop_cnt_packets;
	curr_fail_ttl = fail_ttl - msdu_drop->last_ttl_drop_cnt_packets;

	msdu_drop->last_xmit_cnt_packets = total;
	msdu_drop->last_drop_cnt_packets = fail_drop;
	msdu_drop->last_ttl_drop_cnt_packets = fail_ttl;

	msduq = &peer_ctx->peer_msduq.msduq[queue];
	telemetry_detect_breach_peer_drop_num_pkt(peer_ctx, msduq->svc_id,
						  queue,
						  curr_total, curr_fail,
						  curr_fail_ttl);
	return 0;
}

int telemetry_sawf_get_rate(void *telemetry_ctx, uint8_t tid, uint8_t queue,
			    uint32_t *egress_rate, uint32_t *ingress_rate)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;
	struct telemetry_sawf_stats *stats;
	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	stats = &peer_ctx->stats[queue];

	*egress_rate = stats->tx.throughput.eg_rate;
	*ingress_rate = stats->tx.throughput.in_rate;

	return 0;
}

int telemetry_sawf_pull_mov_avg(void *telemetry_ctx, uint8_t tid, uint8_t queue,
				uint32_t *nwdelay_avg, uint32_t *swdelay_avg,
				uint32_t *hwdelay_avg)
{
	struct telemetry_sawf_peer_ctx *peer_ctx = telemetry_ctx;
	struct telemetry_sawf_stats *stats;
	if (!peer_ctx) {
		pr_sawf_err("SAWF peer ctx is null");
		return -1;
	}
	stats = &peer_ctx->stats[queue];

	*nwdelay_avg = stats->delay.nwdelay_avg;
	*swdelay_avg = stats->delay.swdelay_avg;
	*hwdelay_avg = stats->delay.hwdelay_avg;

	pr_sawf_dbg("nwdelay Mvng Avg: %d", *nwdelay_avg);
	pr_sawf_dbg("swdelay Mvng Avg: %u", *swdelay_avg);
	pr_sawf_dbg("hwdelay Mvng Avg: %u", *hwdelay_avg);
	return 0;
}

int telemetry_sawf_reset_peer_stats(uint8_t *mac_addr)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct list_head *node;
	struct telemetry_sawf_peer_ctx *peer;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF context is NULL");
		return -1;
	}

	spin_lock_bh(&sawf_ctx->peer_list_lock);
	list_for_each(node, &sawf_ctx->peer_list) {
		peer = list_entry(node,
				  struct telemetry_sawf_peer_ctx, node);
		if (!peer) {
			pr_sawf_err("Peer ctx is null");
			continue;
		}
		if (0 == memcmp(mac_addr, peer->mac_addr, MAC_ADDR_SIZE))
			memset(peer->stats, 0, sizeof(peer->stats));

	}
	spin_unlock_bh(&sawf_ctx->peer_list_lock);
	return 0;
}

int telemetry_sawf_init_sla_timer(void)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_sla_params *sla_param;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}

	sla_param = telemetry_sawf_get_sla_param();
	if (!sla_param) {
		pr_sawf_err("SAWF param is null");
		return -1;
	}

	timer_setup(&sawf_ctx->per_sec_timer,
		    telemetry_detect_sla_per_sec, 0);
	timer_setup(&sawf_ctx->num_sec_timer,
		    telemetry_detect_sla_num_sec, 0);

	if (!find_first_bit(sawf_ctx->sla.sla_service_ids,
			    SAWF_MAX_SVC_CLASS)) {
		pr_sawf_dbg("SLA not configured for any service class");
		return 0;
	}

	add_timer(&sawf_ctx->per_sec_timer);
	add_timer(&sawf_ctx->num_sec_timer);

	mod_timer(&sawf_ctx->per_sec_timer,
		  jiffies + msecs_to_jiffies(1000));
	mod_timer(&sawf_ctx->num_sec_timer,
		  jiffies + msecs_to_jiffies(sla_param->time_secs * 1000));

	return 0;
}

int telemetry_sawf_free_sla_timer(void)
{
	struct telemetry_sawf_ctx *sawf_ctx;

	sawf_ctx = telemetry_sawf_get_ctx();
	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return -1;
	}

	del_timer(&sawf_ctx->per_sec_timer);
	del_timer(&sawf_ctx->num_sec_timer);

	return 0;
}

void telemetry_detect_sla_per_sec(struct timer_list *timer)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_sla_param *sla_detect;
	sawf_ctx = from_timer(sawf_ctx, timer, per_sec_timer);

	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return;
	}

	sla_detect = &sawf_ctx->sla_detect.sla_detect[SAWF_SLA_DETECT_PER_SECOND];

	if (sla_detect->min_thruput_rate || sla_detect->max_thruput_rate) {
		telemetry_detect_breach(SAWF_SLA_DETECT_PER_SECOND);
	}

	mod_timer(&sawf_ctx->per_sec_timer, MSEC_TO_JIFFIES(1000));
}

void telemetry_detect_sla_num_sec(struct timer_list *timer)
{
	struct telemetry_sawf_ctx *sawf_ctx;
	struct telemetry_sawf_sla_param *sla_detect;
	sawf_ctx = from_timer(sawf_ctx, timer, num_sec_timer);

	if (!sawf_ctx) {
		pr_sawf_err("SAWF ctx is null");
		return;
	}

	sla_detect = &sawf_ctx->sla_detect.sla_detect[SAWF_SLA_DETECT_NUM_SECOND];

	//Check for SLA breaches
	if (sla_detect->burst_size || sla_detect->service_interval
	    || sla_detect->msdu_ttl || sla_detect->msdu_rate_loss) {
		telemetry_detect_breach(SAWF_SLA_DETECT_NUM_SECOND);
	}

	mod_timer(&sawf_ctx->num_sec_timer,
		  MSEC_TO_JIFFIES(sawf_ctx->sla_param.time_secs * 1000));
}
