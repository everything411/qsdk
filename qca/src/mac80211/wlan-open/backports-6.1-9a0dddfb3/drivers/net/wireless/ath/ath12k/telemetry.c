/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "core.h"
#include "debug.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "telemetry.h"
#include <linux/module.h>
#include "telemetry_agent_if.h"
#include "../telemetry_agent_wifi_driver_if.h"
#include "debugfs_htt_stats.h"
#include <linux/vmalloc.h>

#define MIN_THERSHOLD_PERCENTAGE 0
#define MAX_THERSHOLD_PERCENTAGE 100

static struct ath12k_telemetry_ctx *telemetry_ctx;

void ath12k_telemetry_init(struct ath12k_base *ab)
{
	if (telemetry_ctx)
		return;

	telemetry_ctx = kzalloc(sizeof(*telemetry_ctx), GFP_KERNEL);
	if (!telemetry_ctx) {
		ath12k_err(NULL, "telemetry context failed to initialize\n");
		return;
	}

	spin_lock_init(&telemetry_ctx->breach_ind_lock);
	telemetry_ctx->workqueue = create_singlethread_workqueue("breach_ind_wq");
	INIT_WORK(&telemetry_ctx->indicate_breach, ath12k_send_breach_indication);
	INIT_LIST_HEAD(&telemetry_ctx->list);

	/* RSSI/Rate breach initialization */
	spin_lock_init(&telemetry_ctx->rssi_rate_breach_lock);
	INIT_WORK(&telemetry_ctx->indicate_rssi_rate_breach,
		  ath12k_send_rssi_rate_breach_indication);
	INIT_LIST_HEAD(&telemetry_ctx->rssi_rate_breach_list);

	ath12k_info(NULL, "telemetry context initialized\n");
}

void ath12k_telemetry_deinit(struct ath12k_base *ab)
{
	if (!telemetry_ctx)
		return;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags)) {
		cancel_work_sync(&telemetry_ctx->indicate_breach);
		cancel_work_sync(&telemetry_ctx->indicate_rssi_rate_breach);
		return;
	}

	cancel_work_sync(&telemetry_ctx->indicate_breach);
	cancel_work_sync(&telemetry_ctx->indicate_rssi_rate_breach);
	destroy_workqueue(telemetry_ctx->workqueue);

	kfree(telemetry_ctx);
	telemetry_ctx = NULL;
	ath12k_info(NULL, "telemetry context freed\n");
}

int ath12k_telemetry_sdwf_sla_samples_config(struct ath12k_sla_samples_cfg param)
{
	struct ath12k_sla_samples_cfg *telemetry_param = NULL;
	int ret;

	if (param.moving_avg_pkt == 0 || param.moving_avg_win == 0 ||
	    param.sla_num_pkt == 0 || param.sla_time_sec == 0) {
		ath12k_err(NULL, "invalid telemetry sla samples configuration\n");
		return -EINVAL;
	}

	if (telemetry_ctx)
		telemetry_param = &telemetry_ctx->sla_samples_params;

	ret = ath12k_telemetry_set_mov_avg_params(param.moving_avg_pkt,
						  param.moving_avg_win);

	if (!ret) {
		if (telemetry_param) {
			telemetry_param->moving_avg_pkt = param.moving_avg_pkt;
			telemetry_param->moving_avg_win = param.moving_avg_win;
		}
		ath12k_info(NULL, "telemetry sla samples configuration "
			    "movavgpkt:%d movavgwin:%d done\n",
			    param.moving_avg_pkt, param.moving_avg_win);
	}

	if (ret && ret != -ENOENT) {
		ath12k_err(NULL, "telemetry failed to set mov avg params ret:%d\n",
			   ret);
		return ret;
	}

	ret = ath12k_telemetry_set_sla_params(param.sla_num_pkt,
					      param.sla_time_sec);

	if (!ret) {
		if (telemetry_param) {
			telemetry_param->sla_num_pkt = param.sla_num_pkt;
			telemetry_param->sla_time_sec = param.sla_time_sec;
		}
		ath12k_info(NULL, "telemetry sla samples configuration "
			    "slanumpkt:%d slatimesec:%d done\n",
			    param.sla_num_pkt, param.sla_time_sec);
	}

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla params ret:%d\n",
		   ret);
	return ret;
}

int ath12k_telemetry_sdwf_sla_thershold_config(struct ath12k_sla_thershold_cfg param)
{
	int ret;

	if (param.min_throughput_rate < MIN_THERSHOLD_PERCENTAGE ||
	    param.min_throughput_rate > MAX_THERSHOLD_PERCENTAGE ||
	    param.max_throughput_rate < MIN_THERSHOLD_PERCENTAGE ||
	    param.max_throughput_rate > MAX_THERSHOLD_PERCENTAGE ||
	    param.burst_size < MIN_THERSHOLD_PERCENTAGE ||
	    param.burst_size > MAX_THERSHOLD_PERCENTAGE ||
	    param.service_interval < MIN_THERSHOLD_PERCENTAGE ||
	    param.service_interval > MAX_THERSHOLD_PERCENTAGE ||
	    param.delay_bound < MIN_THERSHOLD_PERCENTAGE ||
	    param.delay_bound > MAX_THERSHOLD_PERCENTAGE ||
	    param.msdu_ttl < MIN_THERSHOLD_PERCENTAGE ||
	    param.msdu_ttl > MAX_THERSHOLD_PERCENTAGE ||
	    param.msdu_rate_loss < MIN_THERSHOLD_PERCENTAGE ||
	    param.msdu_rate_loss > MAX_THERSHOLD_PERCENTAGE) {
		ath12k_err(NULL, "invalid telemetry sla thershold configuration\n");
		return -EINVAL;
	}

	ret = ath12k_telemetry_set_sla_cfg(param);

	if (!ret)
		ath12k_info(NULL, "telemetry sla thershold configuration done, "
			    "svcid: %d MinThrRate:%d MaxThrRate:%d BurstSize:%d "
			    "serviceInt:%d \n DelayBound: %d MsduTtl:%d "
			    "msdurateloss:%d per:%u, mcs_min_threshold:%u,"
			    "mcs_max_thres%u, retries_thres:%u\n", param.svc_id,
			    param.min_throughput_rate,
			    param.max_throughput_rate,
			    param.burst_size,
			    param.service_interval,
			    param.delay_bound,
			    param.msdu_ttl,
			    param.msdu_rate_loss,
			    param.per,
			    param.mcs_min_thres,
			    param.mcs_max_thres,
			    param.retries_thres);

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla threshold configs ret:%d\n",
		   ret);
	return ret;
}

int ath12k_telemetry_sdwf_sla_detection_config(struct ath12k_sla_detect_cfg param)
{
	int ret;

	if ((param.sla_detect == SLA_DETECT_NUM_PACKET) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.burst_size || param.service_interval)) {
		ath12k_info(NULL, "unsupported sla detect config for number of packets.\n");
		return -1;
	}

	if (((param.sla_detect == SLA_DETECT_PER_SECOND)) &&
	    (param.burst_size || param.service_interval || param.delay_bound ||
	     param.msdu_ttl || param.msdu_rate_loss)) {
		ath12k_info(NULL, "unsupported sla detect config for per second.\n");
		return -1;
	}

	if ((param.sla_detect == SLA_DETECT_MOV_AVG) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.burst_size || param.service_interval || param.msdu_ttl ||
	     param.msdu_rate_loss)) {
		ath12k_info(NULL, "unsupported sla detect config for moving average.\n");
		return -1;
	}

	if ((param.sla_detect == SLA_DETECT_NUM_SECOND) &&
	    (param.min_throughput_rate || param.max_throughput_rate ||
	     param.delay_bound)) {
		ath12k_info(NULL, "unsupported sla detect config for number of seconds.\n");
		return -1;
	}

	ret = ath12k_telemetry_set_sla_detect_cfg(param);

	if (!ret)
		ath12k_info(NULL, "telemetry sla detection configuration done, detect option: %d "
			    "MinThrRate:%d MaxThrRate:%d BurstSize:%d ServiceInt:%d \n DelayBound: %d "
			    "MsduTtl:%d MsduRateLoss:%d PktErrorRate:%u, MCSminThreshold:%u"
			    "MCSmaxThreshold:%u RetriesThreshold:%u\n", param.sla_detect,
			    param.min_throughput_rate,
			    param.max_throughput_rate, param.burst_size,
			    param.service_interval, param.delay_bound,
			    param.msdu_ttl, param.msdu_rate_loss,
			    param.per, param.mcs_min_thres,
			    param.mcs_max_thres, param.retries_thres);

	if (!ret || ret == -ENOENT)
		return 0;

	ath12k_err(NULL, "telemetry failed to set sla detection configs ret:%d\n",
		   ret);
	return ret;
}

void ath12k_send_breach_indication(struct work_struct *work)
{
	struct ath12k_telemetry_ctx *telemetry_ctx = container_of(work, struct ath12k_telemetry_ctx, indicate_breach);
	struct ath12k_tele_breach_params *breach_params, *tmp;

	if (!telemetry_ctx) {
		ath12k_err(NULL, "Telemetry ctx is unavailable\n");
		return;
	}

	spin_lock_bh(&telemetry_ctx->breach_ind_lock);
	list_for_each_entry_safe(breach_params, tmp, &telemetry_ctx->list, list) {
		list_del(&breach_params->list);
		spin_unlock_bh(&telemetry_ctx->breach_ind_lock);
		ath12k_telemetry_notify_breach(breach_params->mac_addr,
					       breach_params->svc_id,
					       breach_params->param,
					       breach_params->set_clear,
					       breach_params->tid);
		kfree(breach_params);
		spin_lock_bh(&telemetry_ctx->breach_ind_lock);
	}
	spin_unlock_bh(&telemetry_ctx->breach_ind_lock);
}

void ath12k_telemetry_breach_indication(u8 *mac_addr, u8 svc_id, u8 param, bool set_clear, u8 tid)
{
	struct ath12k_tele_breach_params *breach_params;

	if (!mac_addr)
		return;

	if (!telemetry_ctx) {
		ath12k_err(NULL, "Breach detection received when telemetry ctx is unavailable\n");
		return;
	}

	breach_params = kzalloc(sizeof(*breach_params), GFP_NOWAIT);
	if (!breach_params) {
		ath12k_err(NULL, "Failed to allocate memory to indicate breach detection\n");
		return;
	}

	ether_addr_copy(breach_params->mac_addr, mac_addr);
	breach_params->svc_id = svc_id;
	breach_params->param = param;
	breach_params->set_clear = set_clear;
	breach_params->tid = tid;

	spin_lock_bh(&telemetry_ctx->breach_ind_lock);
	list_add_tail(&breach_params->list, &telemetry_ctx->list);
	spin_unlock_bh(&telemetry_ctx->breach_ind_lock);

	queue_work(telemetry_ctx->workqueue, &telemetry_ctx->indicate_breach);
}

/**
 * ath12k_send_rssi_rate_breach_indication - Workqueue handler for RSSI/Rate breaches
 * @work: Work struct
 *
 * Processes all queued RSSI/Rate breach indications from the list.
 * Called in work context.
 */
void ath12k_send_rssi_rate_breach_indication(struct work_struct *work)
{
	struct ath12k_telemetry_ctx *telemetry_ctx =
		container_of(work, struct ath12k_telemetry_ctx,
			     indicate_rssi_rate_breach);
	struct ath12k_rssi_rate_breach_params *breach_params;

	if (!telemetry_ctx) {
		ath12k_err(NULL, "Telemetry ctx is unavailable\n");
		return;
	}

	while (1) {
		spin_lock_bh(&telemetry_ctx->rssi_rate_breach_lock);

		if (list_empty(&telemetry_ctx->rssi_rate_breach_list)) {
			spin_unlock_bh(&telemetry_ctx->rssi_rate_breach_lock);
			break;
		}

		breach_params = list_first_entry(&telemetry_ctx->rssi_rate_breach_list,
						 typeof(*breach_params), list);
		list_del(&breach_params->list);
		spin_unlock_bh(&telemetry_ctx->rssi_rate_breach_lock);

		ath12k_rssi_rate_notify_breach_event(breach_params->mac_addr,
						     breach_params->breach_type,
						     breach_params->threshold_value,
						     breach_params->detected_value,
						     breach_params->set_clear);
		kfree(breach_params);
	}
}

/**
 * ath12k_rssi_rate_breach_indication - Queue RSSI/Rate breach for processing
 * @mac_addr: Peer MAC address
 * @breach_type: Type of breach (BREACH_TYPE_RSSI_MIN, etc.)
 * @threshold_value: Configured threshold value
 * @detected_value: Actual detected value that caused breach
 * @set_clear: true = breach detected, false = breach cleared
 *
 * This function queues the rssi/rate breach indication for asynchronous processing
 * via workqueue. It allocates breach parameters and adds to list.
 */
void ath12k_rssi_rate_breach_indication(u8 *mac_addr, u8 breach_type,
					u32 threshold_value, u32 detected_value,
					bool set_clear)
{
	struct ath12k_rssi_rate_breach_params *breach_params;

	if (!mac_addr)
		return;

	if (!telemetry_ctx) {
		ath12k_err(NULL, "RSSI/Rate breach received when telemetry ctx unavailable\n");
		return;
	}

	breach_params = kzalloc(sizeof(*breach_params), GFP_ATOMIC);
	if (!breach_params)
		return;

	ether_addr_copy(breach_params->mac_addr, mac_addr);
	breach_params->breach_type = breach_type;
	breach_params->threshold_value = threshold_value;
	breach_params->detected_value = detected_value;
	breach_params->set_clear = set_clear;

	spin_lock_bh(&telemetry_ctx->rssi_rate_breach_lock);
	list_add_tail(&breach_params->list, &telemetry_ctx->rssi_rate_breach_list);
	spin_unlock_bh(&telemetry_ctx->rssi_rate_breach_lock);

	queue_work(telemetry_ctx->workqueue, &telemetry_ctx->indicate_rssi_rate_breach);
}

bool ath12k_telemetry_get_sla_mov_avg_num_pkt(u32 *mov_avg)
{
	if (!telemetry_ctx) {
		*mov_avg = 1;
		return false;
	}

	*mov_avg = telemetry_ctx->sla_samples_params.moving_avg_pkt;
	return true;
}
EXPORT_SYMBOL(ath12k_telemetry_get_sla_mov_avg_num_pkt);

bool ath12k_telemetry_get_sla_num_pkts(u32 *pkt_num)
{
	if (!telemetry_ctx) {
		*pkt_num = 1;
		return false;
	}

	*pkt_num = telemetry_ctx->sla_samples_params.sla_num_pkt;
	return true;
}
EXPORT_SYMBOL(ath12k_telemetry_get_sla_num_pkts);

/**
 * ath12k_telemetry_get_phy_nf() - Fetch static NF from HTT PHY stats.
 *
 * Requests HTT stats type ATH12K_DBG_HTT_EXT_PHY_COUNTERS_AND_PHY_STATS (37)
 * and extracts:
 *  - bdf_nf_chains[]: all static NF chain values (1 = invalid sentinel)
 *
 * Dynamic (runtime) NF is obtained separately via the WMI BSS survey path
 * (same mechanism as "iw dev survey dump"), not from HTT PHY stats.
 *
 * ath12k_debugfs_htt_stats_req() is synchronous: it blocks on
 * stats_req->htt_stats_rcvd until the firmware response is fully received
 * and all TLVs (including HTT_STATS_PHY_STATS_TAG) have been processed by
 * ath12k_debugfs_htt_ext_stats_handler(). The NF values are therefore
 * already stored in ar->bdf_nf_chains[] before the call returns, making it
 * safe to free stats_req and clear ar->debug.htt_stats.stats_req immediately
 * afterwards.
 *
 * Must be called with wiphy_lock held.
 *
 * Returns 0 on success, negative error code on failure.
 */
#ifdef CPTCFG_ATH12K_DEBUGFS
int ath12k_telemetry_get_phy_nf(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct debug_htt_stats_req *stats_req;
	int ret;

	if (!ab->ag || ab->is_bypassed ||
	    test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ab->ag->flags) ||
	    test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return -ESHUTDOWN;

	stats_req = vzalloc(sizeof(*stats_req) + ATH12K_HTT_STATS_BUF_SIZE);

	if (!stats_req)
		return -ENOMEM;

	stats_req->type = ATH12K_DBG_HTT_EXT_PHY_COUNTERS_AND_PHY_STATS;
	stats_req->ar = ar;

	spin_lock_bh(&ar->data_lock);
	if (ar->debug.htt_stats.stats_req) {
		spin_unlock_bh(&ar->data_lock);
		vfree(stats_req);
		return -EBUSY;
	}
	ar->debug.htt_stats.stats_req = stats_req;
	spin_unlock_bh(&ar->data_lock);

	ret = ath12k_debugfs_htt_stats_req(ar);

	/* Clear the pointer under data_lock before freeing stats_req.
	 * ath12k_debugfs_htt_ext_stats_handler() accesses stats_req fields
	 * under data_lock; clearing the pointer here while holding the same
	 * lock ensures any handler already inside its critical section
	 * completes before we free, and any subsequent late firmware response
	 * (e.g. arriving after a timeout) will see NULL and bail out safely.
	 */
	spin_lock_bh(&ar->data_lock);
	ar->debug.htt_stats.stats_req = NULL;
	spin_unlock_bh(&ar->data_lock);
	vfree(stats_req);

	return ret;
}
EXPORT_SYMBOL(ath12k_telemetry_get_phy_nf);
#endif /* CPTCFG_ATH12K_DEBUGFS */
