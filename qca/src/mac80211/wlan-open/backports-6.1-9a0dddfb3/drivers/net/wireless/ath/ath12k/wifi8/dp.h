/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI8_H
#define ATH12K_DP_WIFI8_H

#include "../core.h"
#include "../dp_cmn.h"
#include "hw.h"
#include "dp_ast.h"
#include "hal.h"
#include "dp_tx.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/dp.h"
#endif

#define DP_TX_EXCEPTION_RING_SIZE      512
#define DP_WBM_REFILL_RING_MAX         4

/* Dedicated TQM2SW ring for FW-owned buffer completions during recovery */
#define DP_FW_TQM2SW_RING_SIZE		1024
#define DP_FSE_CMD_RING_SIZE		8192
#define DP_PPE2WBM_REFILL_RING_MAX     3
#define DP_PPE2WBM_SFE_POOL_REFILL_RING_NUM	5
#define DP_PPE2WBM_REFILL_RING_SIZE    8192
#define DP_TELEMETRY_TX_RING_SIZE	256
#define DP_TELEMETRY_RX_RING_SIZE	256
#define DP_REO_FLUSH_RING_SIZE		256

#define ATH12K_DEFAULT_UCAST_STATS_ID	512
#define ATH12K_MAX_EXT_UCAST_STATS_ID	1024
#define ATH12K_MAX_UCAST_STATS_ID	1536
#define ATH12K_MAX_STATS_ID		1624
#define ATH12K_MIN_DGCAST_STATS_ID	1536
#define ATH12K_MAX_DGCAST_STATS_ID	1616

#ifndef CPTCFG_QCN_EXTN
#define DP_WBM_IDLE_BUF_PER_SOC		8192
#define DP_PPE2WBM_IDLE_BUF_RING_SIZE	12288
#define DP_WBM_REFILL_RING_SIZE		512
#define DP_WBM_SFE_HIGH_WATERMARK	0x1000
#define DP_WBM_MGMT_HIGH_WATERMARK	0x200
#define DP_WBM_PPE_HIGH_WATERMARK	0x1000
#define DP_PPE2WBM_DEDICATED_POOL	0
#else
#define DP_WBM_IDLE_BUF_PER_SOC		ATH12K_DP_INI_GET(WBM_IDLE_BUF_PER_SOC)
#define DP_PPE2WBM_IDLE_BUF_RING_SIZE	ATH12K_DP_INI_GET(PPE2WBM_IDLE_BUF_RING_SIZE)
#define DP_WBM_REFILL_RING_SIZE		ATH12K_DP_INI_GET(WBM_REFILL_RING_SIZE)
#define DP_WBM_SFE_HIGH_WATERMARK	ATH12K_DP_INI_GET(WBM_SFE_HIGH_WATERMARK)
#define DP_WBM_MGMT_HIGH_WATERMARK	ATH12K_DP_INI_GET(WBM_MGMT_HIGH_WATERMARK)
#define DP_WBM_PPE_HIGH_WATERMARK	ATH12K_DP_INI_GET(WBM_PPE_HIGH_WATERMARK)
#define DP_PPE2WBM_DEDICATED_POOL	ATH12K_DP_INI_GET(PPE2WBM_DEDICATED_BUF_POOL)
#endif

struct stats_to_peer_id_map {
	u16 dp_peer_id;
	u8 tid;
	u8 hw_link_id;
};

#define ATH12K_INVALID_SVC_ID	0xFF

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp_htt_cmd_retry_info {
	DECLARE_BITMAP(htt_retry_peer_id_map, ATH12K_MAX_PEER_ID);
	u8 retry_count;
};

struct ath12k_wifi8_tx_exc_stats {
	u32 tx_exceptions;
	u32 null_flowq_pkts;
	u32 reinject_pkts;
	u32 invalid_desc;
	u32 vdev_id_check_fail;
	u32 addrx_invalid;
	u32 addrx_timeout;
	u32 msdu_drop;
	u32 illegal_pkts;
	u32 illegal_pkt_hdr;
	u32 peer_ptr_null;
	u32 bank_not_configured;
	u32 msdu_len_err;
	u32 to_sw_pkts;
	u32 parse_err;
	u32 classify_info_sel_exceed;
	u32 bank_id_exceed;
	u32 buf_len_err;
};

struct ath12k_wifi8_rx_stats {
	u32 rx_flush_pkts;
	u32 rx_mgmt_flush_pkts;
	u32 rx_flush_null_descs;
};

struct ath12k_wifi8_dp_stats {
	struct ath12k_wifi8_tx_exc_stats tx_exc_stats;
	struct ath12k_wifi8_rx_stats rx_stats;
};

#define ATH12K_DP_TX_CONGSTN_HISTORY_DURATION_SEC	60

/**
 * struct ath12k_wifi8_congstn_history_entry - single congestion recovery event record
 * @timestamp:         jiffies at the time of the handler invocation
 * @used_cnt:          tx_desc_used_cnt at the time of the invocation
 * @ppeds_used_cnt:    ppeds_tx_desc_used_cnt at the time of the invocation
 * @used_threshold:    congestion threshold configured
 * @ppeds_used_threshold:    congestion threshold for PPEDS configured
 * @total_active_msdu: total active MSDUs across all service categories
 * @target_drop:       number of MSDUs targeted for removal
 * @num_drop_flows:    number of flows selected for drop
 * @svc_num_flows:     per-service-category flow count
 * @svc_total_msdu:    per-service-category total MSDU count
 * @drop_flows:        per-flow drop details (flow number, svc, drop count)
 */
struct ath12k_wifi8_congstn_history_entry {
	unsigned long timestamp;
	u32 total_active_msdu;
	u32 used_threshold;
	u32 ppeds_used_threshold;
	u32 target_drop;
	u32 used_cnt;
	u32 ppeds_used_cnt;
	u8  num_drop_flows;
	u8  svc_num_flows[HAL_TQM_SERVICE_CATEGORY_MAX];
	u32 svc_total_msdu[HAL_TQM_SERVICE_CATEGORY_MAX];
	struct ath12k_wifi8_flow_remove_entry drop_flows[HAL_TQM_MAX_SORTED_FLOW_ALL_SVC];
};

struct ath12k_wifi8_dp_congestion_control {
	struct timer_list timer;
	void (*calculate_drop)(struct ath12k_wifi8_dp_congestion_control *congstn,
			       struct ath12k_wifi8_svc_sorted_flows *svc_datas,
			       u32 target_drop,
			       struct ath12k_wifi8_svc_remove_flows *drop);
	bool init;
	bool start;
	u32 interval;
	u32 used_threshold;
	u32 ppeds_used_threshold;
	u32 max_used;
	u8 flow_drop_grace_percent;
	u32 weights[HAL_TQM_SERVICE_CATEGORY_MAX];
	unsigned long last_jiffies;
	unsigned long last_drop_jiffies;
	u32 continuous_drop_threshold;
	/* per-service-category threshold lists */
	struct list_head threshold_list[HAL_TQM_SERVICE_CATEGORY_MAX];
	/* single lock protecting all threshold_list entries */
	spinlock_t threshold_list_lock;
	u32 scaling_factor;
	/* tick counter: incremented each timer invocation to track elapsed time */
	u32 tick_counter;
	/* per-invocation sorted-flow scratch buffer (allocated once at init) */
	struct ath12k_wifi8_svc_sorted_flows *svc_data;
	/* per-invocation drop scratch buffer (allocated once at init) */
	struct ath12k_wifi8_svc_remove_flows *drop;
	struct ath12k_wifi8_dp_tx_flow_cost *entries;
	/* circular history buffer for congestion recovery log (dynamically allocated) */
	struct ath12k_wifi8_congstn_history_entry *history;
	u16 history_size;
	/* next write index (wraps at ATH12K_DP_TX_CONGSTN_HISTORY_SIZE) */
	u32 history_head;
	/* total entries written */
	u32 history_count;
	/* lock to protect history */
	spinlock_t history_lock;
	/* enable/disable history logging (default: false) */
	bool history_enable;
};

struct ath12k_dp_wifi8 {
	struct ath12k_dp *dp;
	bool alloc_done;
	bool init_done;
	atomic_t sam_cmd_num;
	struct dp_srng tx_exception;
	struct dp_srng tcl_cmd_ring;
	struct dp_srng tcl_status_ring;
	struct dp_srng reo_dst_high_prio_ring;
	struct dp_srng wbm_refill_ring[DP_WBM_REFILL_RING_MAX];
	struct dp_srng wbm_idle_buf_ring;
	struct dp_srng reo_high_prio_cmd_ring;
	struct dp_srng tqm_cmd_ring;
	struct dp_srng tqm_status_ring;
	struct dp_srng fse_cmd_ring;
	struct dp_srng tqm2sw_fw_ring;
	struct dp_srng sam_cmd_ring;
	struct dp_srng sam_status_ring;
	struct dp_srng rx_ase_cmd_ring;
	struct dp_srng rx_ase_status_ring;
	struct dp_srng reo_flush_ring;
	struct ath12k_wifi8_dp_stats stats;

	/* SAM command ring staging in words */
	u32 *sam_cmd_staging;
	struct dp_srng ppe2wbm_refill_ring[DP_PPE2WBM_REFILL_RING_MAX];
	struct dp_srng ppe2wbm_idle_buf_ring;
	u8 num_ppe2wbm_refill_rings;
	bool dp_ppe2wbm_use_dedicated_pool;
	struct dp_srng rx_peer_telemetry_ring;
	struct dp_srng tx_peer_telemetry_ring;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_dp_wifi8_extn extn;
#endif /* CPTCFG_QCN_EXTN */
};

struct ath12k_dp_hw_group_wifi8 {
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp *cumac_dp;
	struct ath12k_dp_global_ast_table dp_ast_base;
	struct ath12k_pn_page_info *pn_page_info;
	struct pool_ctxt_t *msduq_ctxt;
	struct pool_ctxt_t *sw_msduq_ctxt;
	struct pool_ctxt_t *mpduq_ctxt;
	struct pool_ctxt_t *sw_mpduq_ctxt;
	/* lock for tx flow pool */
	spinlock_t tx_pool_lock;
	u8 num_pn_pages;
	struct completion peer_init_done;

	struct ath12k_dp_htt_cmd_retry_info retry_info[ATH12K_GROUP_MAX_RADIO];
	/* lock for htt cmd retry info */
	spinlock_t htt_cmd_retry_lock;
	struct delayed_work dp_htt_retry_dwork;
	atomic_t retry_work_active;
	struct timer_list hw_grp_timer;
	/* lock for hw grp timer */
	spinlock_t hw_grp_timer_lock;
	struct list_head timer_list_head;
	u32 current_timer_val;
	u32 timer_entry_count;
	struct list_head mec_entry_list_head;
	u16 mec_timer_key;
	DECLARE_BITMAP(free_stats_id, ATH12K_MAX_STATS_ID);
	DECLARE_BITMAP(free_link_band_id, ATH12K_MAX_STATS_ID);
	u16 last_ucast_stats_id;
	u16 last_gcast_stats_id;
	struct stats_to_peer_id_map stats_id_map[ATH12K_MAX_STATS_ID];

	DECLARE_BITMAP(msduq_sam_id_alloc_map, MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED);
	DECLARE_BITMAP(mpduq_sam_id_alloc_map, MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED);
	u16 last_msduq_sam_id;
	u16 last_mpduq_sam_id;
	/* lock for sam id alloc map*/
	spinlock_t sam_id_lock;

	/* structure for congestion control.*/
	struct ath12k_wifi8_dp_congestion_control congstn;
};

struct dp_hw_grp_timer_entry {
	void *arg;
	u32 timeout_ms;
	u8 scaling_factor;
	u8 counter;
	u16 key_value;
	struct list_head list;
	void (*cmd_callback)(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			     void *arg);
};

struct ath12k_dp_hw_grp_timer_entry_param {
	u32 timeout_ms;
	void *arg;
	void (*callback)(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
			 void *arg);
	u16 key_value;
};

void ath12k_dp_hw_group_timer_fn(struct timer_list *timer);
int ath12k_dp_hw_group_add_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
				       struct ath12k_dp_hw_grp_timer_entry_param *param);
bool ath12k_dp_hw_group_del_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
					u16 key_value);

static inline struct ath12k_dp_wifi8 *ath12k_get_dp_wifi8(struct ath12k_dp *dp)
{
	return (struct ath12k_dp_wifi8 *)dp->arch_data;
}

static inline struct ath12k_dp *ath12k_get_dp(struct ath12k_dp_wifi8 *dp_wifi8)
{
	return dp_wifi8->dp;
}

static inline struct ath12k_dp_hw_group_wifi8 *
		ath12k_get_dp_hw_group_wifi8(struct ath12k_dp_hw_group *dp_hw_grp)
{
	return (struct ath12k_dp_hw_group_wifi8 *)dp_hw_grp->arch_data;
}

static inline struct ath12k_dp_hw_group *
		ath12k_get_dp_hw_group(struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8)
{
	return dp_hw_group_wifi8->dp_hw_grp;
}

static inline struct ath12k_base *
		ath12k_dp_get_ab_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->ab;
}

static inline struct device *
		ath12k_dp_get_dev_from_dp_hw_group(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	if (!dp_hw_grp_wifi8->cumac_dp)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp->dev;
}

static inline struct ath12k_dp *ath12k_get_central_dp(struct ath12k_dp *dp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	if (!dp_hw_grp_wifi8)
		return NULL;

	return dp_hw_grp_wifi8->cumac_dp;
}

int ath12k_wifi8_fetch_smd_ctx(struct ath12k_base *ab, struct ath12k_dp_hw *dp_hw,
			       struct ieee80211_vif *vif, struct ath12k_dp_smd_ctx *ctx,
			       void (*rx_cb)(struct ath12k_dp *dp, void *ctx,
					     struct hal_reo_status *reo_status),
			       void (*tx_cb)(struct ath12k_dp *dp, void *ctx,
					     u8 *addr, u8 tid));

struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp);
void ath12k_wifi8_srng_hw_ring_disable(struct ath12k_base *ab);
#endif
