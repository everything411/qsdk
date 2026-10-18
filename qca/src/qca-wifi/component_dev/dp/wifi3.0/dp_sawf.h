/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DP_SAWF_H_
#define _DP_SAWF_H_

#include <qdf_lock.h>
#include <dp_types.h>
#include <dp_peer.h>
#include "dp_internal.h"
#include "dp_types.h"
#include "cdp_txrx_cmn_struct.h"
#include "cdp_txrx_hist_struct.h"
#include "cdp_txrx_extd_struct.h"
#include "cdp_txrx_sawf.h"

#define dp_sawf_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_trace(params...) \
	__QDF_TRACE_FL(QDF_TRACE_LEVEL_TRACE, QDF_MODULE_ID_DP_SAWF, ## params)

#define dp_sawf_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_DP_SAWF, params)

#define dp_sawf_alert_rl(params...) \
	QDF_TRACE_FATAL_RL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_err_rl(params...) \
	QDF_TRACE_ERROR_RL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_warn_rl(params...) \
	QDF_TRACE_WARN_RL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_info_rl(params...) \
	QDF_TRACE_INFO_RL(QDF_MODULE_ID_DP_SAWF, params)
#define dp_sawf_debug_rl(params...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_DP_SAWF, params)

#define dp_sawf_debug_hex(ptr, size) \
	qdf_trace_hex_dump(QDF_MODULE_ID_DP_SAWF, \
			QDF_TRACE_LEVEL_DEBUG, ptr, size)

#define dp_sawf_print_stats(params ...)\
	dp_sawf_debug(params)

#define DP_SAWF_SVC_CLASS_MIN 1
#define DP_SAWF_SVC_CLASS_MAX 128

#define MSDU_QUEUE_LATENCY_WIN_MIN_SAMPLES 20
#define WLAN_TX_DELAY_UNITS_US 10
#define WLAN_TX_DELAY_MASK 0x1FFFFFFF
#define DP_SAWF_DEFINED_Q_PTID_MAX 2
#define DP_SAWF_DEFAULT_Q_PTID_MAX 2
#define DP_SAWF_TID_MIN 0
#define DP_SAWF_TID_MAX 8
#define DP_SAWF_DEFAULT_QUEUE_MIN 0
#define DP_SAWF_DEFAULT_QUEUE_MAX (DP_SAWF_DEFAULT_QUEUE_MIN + DP_SAWF_TID_MAX)
#define DP_SAWF_Q_MAX (DP_SAWF_DEFINED_Q_PTID_MAX * DP_SAWF_TID_MAX)
#define DP_SAWF_DEFAULT_Q_MAX (DP_SAWF_DEFAULT_Q_PTID_MAX * DP_SAWF_TID_MAX)
#define dp_sawf(peer, msduq_num, field) ((peer)->sawf->msduq[msduq_num].field)
#define DP_SAWF_DEFAULT_Q_INVALID 0xff
#define DP_SAWF_PEER_Q_INVALID 0xffff
#define DP_SAWF_Q_INVALID 0xff
#define DP_SAWF_TAG_SHIFT 24
#define DP_SAWF_SERVICE_CLASS_SHIFT 16
#define DP_SAWF_QUEUE_ID_SHIFT 0
#define DP_SAWF_VALID_TAG 0xAA
#define DP_SAWF_SERVICE_CLASS_MASK 0xff
#define DP_SAWF_META_DATA_INVALID 0xffffffff
#define DP_SAWF_TAG_SET(tag) (tag << DP_SAWF_TAG_SHIFT)
#define DP_SAWF_SERVICE_CLASS_SET(srvc_id) \
		((srvc_id & DP_SAWF_SERVICE_CLASS_MASK) << \
					DP_SAWF_SERVICE_CLASS_SHIFT)
#define DP_SAWF_QUEUE_ID_SET(queue_id) (queue_id << DP_SAWF_QUEUE_ID_SHIFT)

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
#define SAWF_META_VALID_MASK 0x00400000
#define DP_SAWF_METADATA_SET(metadata, srvc_id, queue_id) \
	(metadata = DP_SAWF_TAG_SET(DP_SAWF_VALID_TAG) | \
	 DP_SAWF_QUEUE_ID_SET(queue_id) | \
	 SAWF_META_VALID_MASK)
#else
#define DP_SAWF_METADATA_SET(metadata, srvc_id, queue_id) \
	(metadata = DP_SAWF_TAG_SET(DP_SAWF_VALID_TAG) | \
	 DP_SAWF_SERVICE_CLASS_SET(srvc_id) | \
	 DP_SAWF_QUEUE_ID_SET(queue_id))
#endif

#define SAWF_PEER_ID_SHIFT 0x6
#define SAWF_PEER_ID_MASK 0x3ff

#define SAWF_PEER_ID_GET(x) (((x) >> SAWF_PEER_ID_SHIFT) \
	& SAWF_PEER_ID_MASK)

#define DP_SAWF_INVALID_SVC_ID 0
#define DP_SAWF_INVALID_TCL_CMD 0xffff

#ifdef CONFIG_SAWF
#ifdef SAWF_MSDUQ_DEBUG
#define DP_SAWF_MSDUQ_STATS_INC(htt_ind_dbg, result) \
	do { \
		msduq->htt_ind_dbg.result++; \
	} while (0)

#else
#define DP_SAWF_MSDUQ_STATS_INC(htt_ind_dbg, result)
#endif
#endif

#define DP_SAWF_QUEUE_ID_GET(metadata) metadata & 0xFFFF
#define DP_SAWF_INVALID_AST_IDX 0xffff
#define DP_SAWF_MAX_DYNAMIC_AST 2

#define DP_SAWF_DELAY_BOUND_MS_MULTIPLER 1000

/**
 * msduq_state - status of MSDUQ
 * @SAWF_MSDUQ_UNUSED : unused MSDUQ
 * @SAWF_MSDUQ_DEACTIVATE_PENDING : De-Activate pending resp from tgt
 * @SAWF_MSDUQ_DEACTIVATED : De-Activate an MSDUQ
 * @SAWF_MSDUQ_REACTIVATE_PENDING : Re-Activate pending resp from tgt
 * @SAWF_MSDUQ_IN_USE : MSDUQ is in use
 */
enum msduq_state {
	SAWF_MSDUQ_UNUSED,
	SAWF_MSDUQ_DEACTIVATE_PENDING,
	SAWF_MSDUQ_DEACTIVATED,
	SAWF_MSDUQ_REACTIVATE_PENDING,
	SAWF_MSDUQ_IN_USE,
};

static inline char *dp_sawf_msduq_state_to_string(enum msduq_state q_state)
{
	switch (q_state) {
	case SAWF_MSDUQ_UNUSED:
		return "UNUSED";
	case SAWF_MSDUQ_DEACTIVATE_PENDING:
		return "DEACTIVATE_PENDING";
	case SAWF_MSDUQ_DEACTIVATED:
		return "DEACTIVATED";
	case SAWF_MSDUQ_REACTIVATE_PENDING:
		return "REACTIVATE_PENDING";
	case SAWF_MSDUQ_IN_USE:
		return "IN_USE";
	default:
		return "UNKNOWN_STATE";
	}
}

/**
 * dp_msduq_report_level - Debug level from userspace to print MSDUQ info.
 * SAWF_MSDUQ_DBG_LVL_INFO: Level to print Queue info
 * SAWF_MSDUQ_DBG_LVL_DEBUG: Level to print advanced Queue info
 * SAWF_MDSUQ_DBG_LVL_TRACE: Level to print advanced Queue info with
 * reactivate/deactive counters and stats
 */
enum dp_msduq_report_level {
	SAWF_MSDUQ_DBG_LVL_INFO = 1,
	SAWF_MSDUQ_DBG_LVL_DEBUG,
	SAWF_MDSUQ_DBG_LVL_TRACE,
};

/**
 * sawf_stats_level - sawf stats level
 * @SAWF_STATS_BASIC : sawf basic stats
 * @SAWF_STATS_ADVNCD : sawf advanced stats
 * @SAWF_STATS_LATENCY : sawf latency stats
 */
enum sawf_stats_level {
	SAWF_STATS_BASIC = 0,
	SAWF_STATS_ADVNCD = 1,
	SAWF_STATS_LATENCY = 2
};

/**
 * sawf_stats - sawf stats
 * @delay: delay stats per host msdu queue
 * @tx_stats: Tx stats per host msdu queue
 * @lock: Protection for sawf-stats
 */
struct sawf_stats {
	struct sawf_delay_stats delay[DP_SAWF_Q_MAX];
	struct sawf_tx_stats tx_stats[DP_SAWF_Q_MAX];
	struct qdf_spinlock lock;
};

struct dp_peer_sawf_stats {
	struct sawf_stats stats;
};

struct sawf_def_queue_report {
	uint8_t svc_class_id;
};

/* Structure to pass relevant information for deprio */
struct dp_sawf_flow_deprioritize_params {
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint32_t mark_metadata;
	bool netdev_info_valid;
	uint32_t netdev_ifindex;
	uint8_t netdev_mac[QDF_MAC_ADDR_SIZE];
};

/**
 * sawf_mov_avg_params - SAWF telemetry moving average params
 * @packet: num of packets per window
 * @window: num of windows
 *
 */
struct sawf_mov_avg_params {
	uint32_t packet;
	uint32_t window;
};

/**
 * sawf_mov_avg_params - SAWF telemetry SLA params
 * @num_packets: num of packets for SLA detection
 * @time_secs: num of sec for SLA detection
 *
 */
struct sawf_sla_params {
	uint32_t num_packets;
	uint32_t time_secs;
};

/**
 * sawf_telemetry_params - SAWF telemetry  params
 * @mov_avg: moving average params
 * @sla: SLA params
 *
 */
struct sawf_telemetry_params {
	struct sawf_mov_avg_params mov_avg;
	struct sawf_sla_params sla;
};

/**
 * dp_sawf_def_queues_unmap_req - unmap peer to service class ID mapping
 * @soc: soc handle
 * @mac_addr: mac address
 * @svc_id: service class ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_def_queues_unmap_req(struct cdp_soc_t *soc_hdl,
			     uint8_t *mac_addr,
			     uint8_t svc_id);

/**
 * dp_sawf_def_queues_get_map_report - get peer to sevice class ID mappings
 * @soc: soc handle
 * @mac_addr: mac address
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_def_queues_get_map_report(struct cdp_soc_t *soc_hdl,
				  uint8_t *mac_addr);

/**
 * dp_sawf_def_queues_map_req - map peer to service class ID
 * @soc: soc handle
 * @mac_addr: mac address
 * @svc_clss_id: service class ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_def_queues_map_req(struct cdp_soc_t *soc_hdl,
			   uint8_t *mac_addr, uint8_t svc_class_id);

/**
 * dp_peer_sawf_ctx_alloc - allocate SAWF ctx
 * @soc: soc handle
 * @peer: dp peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_sawf_ctx_alloc(struct dp_soc *soc,
		       struct dp_peer *peer);

/**
 * dp_peer_sawf_ctx_free - free SAWF ctx
 * @soc: soc handle
 * @peer: dp peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_sawf_ctx_free(struct dp_soc *soc,
		      struct dp_peer *peer);

/**
 * dp_peer_sawf_ctx_get - get SAWF ctx
 * @peer: dp peer
 *
 * Return: SAWF ctx on success; NULL otherwise
 */
struct dp_peer_sawf *dp_peer_sawf_ctx_get(struct dp_peer *peer);

/**
 * dp_peer_sawf_stats_ctx_alloc - allocate SAWF stats ctx
 * @soc: soc handle
 * @txrx_peer: DP txrx peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_sawf_stats_ctx_alloc(struct dp_soc *soc,
			     struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_sawf_stats_ctx_free - free SAWF stats ctx
 * @soc: soc handle
 * @txrx_peer: DP txrx peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_sawf_stats_ctx_free(struct dp_soc *soc,
			    struct dp_txrx_peer *txrx_peer);

/**
 * dp_peer_sawf_stats_ctx_free - free SAWF stats ctx
 * @txrx_peer: DP txrx peer
 *
 * Return: SAWF stas ctx on success, NULL otherwise
 */
struct dp_peer_sawf_stats *
dp_peer_sawf_stats_ctx_get(struct dp_txrx_peer *txrx_peer);

/**
 * dp_sawf_tx_compl_update_peer_stats - update SAWF stats in Tx completion
 * @soc: soc handle
 * @vdev: DP vdev context
 * @txrx_peer: DP txrx peer
 * @tx_desc: Tx descriptor
 * @ts: Tx completion status
 * @tid: TID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_tx_compl_update_peer_stats(struct dp_soc *soc,
				   struct dp_vdev *vdev,
				   struct dp_txrx_peer *txrx_peer,
				   struct dp_tx_desc_s *tx_desc,
				   struct hal_tx_completion_status *ts,
				   uint8_t tid);

/**
 * dp_sawf_tx_enqueue_fail_peer_stats - update SAWF stats in Tx enqueue failure
 * @soc: soc handle
 * @tx_desc: Tx descriptor
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_tx_enqueue_fail_peer_stats(struct dp_soc *soc,
				   struct dp_tx_desc_s *tx_desc);

/**
 * dp_peer_tid_delay_avg - Compute per peer TID delay average
 * @tx_delay: Delay structure
 * @nw_delay: Networking Delay
 * @sw_delay: Wifi Software Delay
 * @hw_delay: Wifi HW delay
 *
 * Return: None
 */
void dp_peer_tid_delay_avg(struct cdp_delay_tx_stats *tx_delay,
			   uint32_t nw_delay,
			   uint32_t sw_delay,
			   uint32_t hw_delay);

/**
 * dp_sawf_tx_enqueue_peer_stats - update SAWF stats in Tx enqueue
 * @soc: soc handle
 * @tx_desc: Tx descriptor
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_tx_enqueue_peer_stats(struct dp_soc *soc,
			      struct dp_peer *peer,
			      struct dp_tx_desc_s *tx_desc);

#define DP_SAWF_STATS_SVC_CLASS_ID_ALL	0

/**
 * dp_sawf_dump_peer_stats - print peer stats
 * @txrx_peer: DP txrx peer
 *
 * Return: Success
 */
QDF_STATUS
dp_sawf_dump_peer_stats(struct dp_txrx_peer *txrx_peer);

/**
 * dp_sawf_get_peer_delay_stats - get delay stats
 * @soc: soc handle
 * @svc_id: service class ID
 * @mac: mac address
 * @data: data to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_peer_delay_stats(struct cdp_soc_t *soc,
			     uint32_t svc_id, uint8_t *mac, void *data);

/**
 * dp_sawf_get_tx_stats- get MSDU Tx stats
 * @arg: argument
 * @in_bytes: Ingress Bytes
 * @in_cnt: Ingress Count
 * @tx_bytes: Transmit Bytes
 * @tx_cnt: Transmit count
 * @tid: TID
 * @msduq: MSDUQ ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_tx_stats(void *arg, uint64_t *in_bytes, uint64_t *in_cnt,
		     uint64_t *tx_bytes, uint64_t *tx_cnt,
		     uint8_t tid, uint8_t msduq);

QDF_STATUS
dp_sawf_get_msduq_tx_stats(void *arg,
			   void *msduq_tx_stats,
			   uint8_t msduq);

/**
 * dp_sawf_get_mpdu_sched_stats - get MPDU scheduling stats
 * @arg: argument
 * @svc_int_pass: MPDU with service interval passed
 * @svc_int_fail: MPDU with service interval failed
 * @burst_pass: MPDU with burst size failed
 * @burst_fail: MPDU with burst size passed
 * @tid: TID
 * @msduq: MSDUQ ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_mpdu_sched_stats(void *arg, uint64_t *svc_int_pass,
			     uint64_t *svc_int_fail, uint64_t *burst_pass,
			     uint64_t *burst_fail, uint8_t tid, uint8_t msduq);

/**
 * dp_sawf_get_drop_stats- get MSDU drop stats
 * @arg: argument
 * @pass: MSDU trasnmitted successfully
 * @drop: MSDU dropped for any reason
 * @drop_ttl: MSDU dropped for TTL expiry
 * @tid: TID
 * @msduq: MSDUQ ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_drop_stats(void *arg, uint64_t *pass, uint64_t *drop,
		       uint64_t *drop_ttl, uint8_t tid, uint8_t msduq);

/**
 * dp_sawf_get_peer_tx_stats - get Tx stats
 * @soc: soc handle
 * @svc_id: service class ID
 * @mac: mac address
 * @data: data to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_peer_tx_stats(struct cdp_soc_t *soc,
			  uint32_t svc_id, uint8_t *mac, void *data);

/**
 * dp_sawf_peer_stats_reset - Reset sawf peer stats
 * @soc: dp_soc
 * @peer: dp_peer
 *
 * Return: none
 */
void dp_sawf_peer_stats_reset(struct dp_soc *soc,
			      struct dp_peer *peer);

#ifdef QCA_PEER_EXT_STATS
/**
 * dp_txrx_pull_tx_peer_stats - pull tx peer stats
 * @peer_mac: peer mac addr
 * @min_tput: pointer to min threshold
 * @max_tput: pointer to max threshold
 * @avg_tput: pointer to average threshold
 * @per: packet pointer to error rate
 * @retries_pct: pointer to retries percentage
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_txrx_pull_tx_peer_stats(uint8_t *peer_mac, uint32_t *min_tput,
			   uint32_t *max_tput, uint32_t *avg_tput,
			   uint32_t *per, uint32_t *retries_pct);
#endif

/**
 * dp_sawf_get_peer_msduq_svc_params - get peer msduq svc info
 * @soc: soc handle
 * @mac: mac address
 * @data: data to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_peer_msduq_svc_params(struct cdp_soc_t *soc, uint8_t *mac,
				  void *data);

#ifdef SAWF_ADMISSION_CONTROL
/**
 * dp_sawf_get_peer_admctrl_stats - Get Peer SAWF AdmCtrl Stats
 * @soc: soc handle
 * @mac: mac address
 * @data: data to be filled
 * @peer_type: Peer type
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_peer_admctrl_stats(struct cdp_soc_t *soc, uint8_t *mac,
			       void *data, enum cdp_peer_type peer_type);
#endif

#ifdef SAWF_MSDUQ_DEBUG
struct msduq_htt_stats {
	uint16_t recv_failure;
	uint16_t timeout;
};
#endif

struct dp_sawf_msduq {
	qdf_atomic_t ref_count;
	uint32_t tgt_opaque_id;
	uint32_t svc_id;
	uint8_t htt_msduq;
	uint8_t remapped_tid;
	uint8_t q_state;
	uint8_t is_deactivation_needed:1,
		no_resp_ind:1,
		map_done:1,
		reserved:5;
#ifdef SAWF_MSDUQ_DEBUG
	struct msduq_htt_stats deactivate_stats;
	struct msduq_htt_stats reactivate_stats;
#endif
};

struct dp_sawf_msduq_tid_map {
	uint8_t host_queue_id;
};

struct dp_peer_sawf {
	/* qdf_bitmap queue_usage; */
	struct dp_sawf_msduq msduq[DP_SAWF_Q_MAX];
	qdf_spinlock_t sawf_peer_lock;
	struct dp_sawf_msduq_tid_map
	       msduq_map[DP_SAWF_TID_MAX][DP_SAWF_DEFINED_Q_PTID_MAX];
	struct sawf_def_queue_report tid_reports[DP_SAWF_TID_MAX];
	uint16_t sla_mask;
	bool is_sla;
	uint16_t dynamic_ast_idx[DP_SAWF_MAX_DYNAMIC_AST];
	void *telemetry_ctx;
};

#ifdef WLAN_FEATURE_11BE_MLO_3_LINK_TX
uint16_t dp_sawf_get_peer_msduq(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				uint8_t *peer_mac,
				uint32_t dscp_pcp, bool pcp);
QDF_STATUS
dp_sawf_3_link_peer_flow_count(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			       uint16_t peer_id, uint32_t mark_metadata);
QDF_STATUS
dp_sawf_3_link_peer_set_tid_weight(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
				   uint16_t peer_id, uint8_t tid_weight[]);
#endif
void dp_sawf_msduq_timer_handler(void *arg);
uint16_t dp_sawf_get_msduq(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   uint8_t *dest_mac, uint32_t service_id);
bool dp_sawf_get_search_index(struct dp_soc *soc, uint8_t vdev_id,
			      uint16_t peer_id, uint16_t queue_id,
			      uint32_t *flow_index);

uint32_t dp_sawf_queue_id_get(qdf_nbuf_t nbuf);
uint16_t dp_sawf_tcl_cmd(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
			 bool is_fast_tx);
bool dp_sawf_tag_valid_get(qdf_nbuf_t nbuf);
uint8_t dp_sawf_tid_get(uint16_t queue_id);

/*
 * dp_sawf_mpdu_stats_req() - Send MPDU basic stats request to target
 * @soc_hdl: SOC handle
 * @enable: 1: Enable 0: Disable
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_sawf_mpdu_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable);

/*
 * dp_sawf_mpdu_details_stats_req() - Send MPDU details stats request to target
 * @soc_hdl: SOC handle
 * @enable: 1: Enable 0: Disable
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
dp_sawf_mpdu_details_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable);

/**
 * dp_sawf_update_mpdu_basic_stats - Update Tx MPDU basic stats
 * @soc: soc handle
 * @peer_id: peer ID
 * @tid: tid used for Tx
 * @q_idx: msdu queue-index used for Tx
 * @svc_intval_success_cnt: no of msdu's successfully transmitted
 * @svc_intval_failure_cnt: no of msdu's failed to be transmitted
 * @burst_size_success_cnt: burst size success count
 * @burst_size_failue_cnt: burst size failure count
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_sawf_update_mpdu_basic_stats(struct dp_soc *soc,
					   uint16_t peer_id,
					   uint8_t tid, uint8_t q_idx,
					   uint64_t svc_intval_success_cnt,
					   uint64_t svc_intval_failure_cnt,
					   uint64_t burst_size_success_cnt,
					   uint64_t burst_size_failue_cnt);

/**
 * dp_sawf_set_mov_avg_params- Set moving average pararms
 * @num_pkt: No of packets per window to calucalte moving average
 * @num_win: No of windows to calucalte moving average
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_sawf_set_mov_avg_params(uint32_t num_pkt,
				      uint32_t num_win);

/**
 * dp_sawf_set_sla_params- Set SLA pararms
 * @num_pkt: No of packets to detect SLA breach
 * @time_secs: Time ins secs to detect breach
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_sawf_set_sla_params(uint32_t num_pkt,
				  uint32_t time_secs);

/**
 * dp_sawf_init_telemtry_param - Initialize telemetry params
 *
 * Return: QDF_STATUS
 */

QDF_STATUS dp_sawf_init_telemetry_params(void);

/**
 * dp_sawf_peer_config_ul - Config uplink parameters
 * @soc_hdl: SOC handle
 * @mac_addr: peer MAC address
 * @tid: TID
 * @service_interval: Service Interval
 * @burst_size: Burst size
 * @min_tput: min throughput
 * @max_latency: max latency
 * @add_or_sub: Add or Sub
 * @peer_id: peer id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_peer_config_ul(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
		       uint8_t tid, uint32_t service_interval,
		       uint32_t burst_size, uint32_t min_tput,
		       uint32_t max_latency, uint8_t add_or_sub,
		       uint16_t peer_id);

/**
 * dp_sawf_peer_flow_count - Increment or decrement flow count per MSDU queue
 * @soc_hdl: SOC handle
 * @mac_addr: MAC address from frame
 * @svc_id: Service Class ID
 * @direction: Indication of forward and reverse service class match
 * @start_or_stop: Indication of start of stop
 * @peer_mac: Pointer to hold peer MAC address
 * @peer_id: peer id
 * @flow_count: flow count
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_peer_flow_count(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			uint8_t svc_id, uint8_t direction,
			uint8_t start_or_stop, uint8_t *peer_mac,
			uint16_t peer_id, uint16_t flow_count);

/*
 * dp_swaf_peer_sla_configuration() - Get sla configuration for a peer
 * @soc_hdl: SOC handle
 * @mac_addr: peer mac address
 * @sla_mask: SLA Mask
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_swaf_peer_sla_configuration(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			       uint16_t *sla_mask);

/**
 * dp_sawf_get_peer_msduq_info - get peer MSDU Queue information
 * @soc: soc handle
 * @mac_addr: mac address
 * @svc_id: Service class ID
 * @debug_level: Debug level
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_get_peer_msduq_info(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			    uint8_t svc_id, uint8_t debug_level);

/**
 * dp_sawf_reinject_handler - Re-inject handler
 * @soc: SOC handle
 * @nbuf: nbuf
 * @htt_desc: HTT descriptor
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_sawf_reinject_handler(struct dp_soc *soc, qdf_nbuf_t nbuf,
			 uint32_t *htt_desc);

/**
 * dp_sawf_peer_msduq_event_notify - Notifier for peer msduq event
 * @soc: SOC handle
 * @peer: peer handle
 * @queue_id: msduq id
 * @svc_id: service class id
 * @event_type: event type (add/delete/update)
 *
 * Return: void
 */
void dp_sawf_peer_msduq_event_notify(struct dp_soc *soc, struct dp_peer *peer,
				     uint8_t queue_id, uint8_t svc_id,
				     enum cdp_sawf_peer_msduq_event event_type);

/**
 * dp_sawf_notify_deactivate_msduq - Collects required params and notify
 * reactivate failure to NW Manager
 *
 * @soc: SOC handle
 * @peer: peer handle
 * @q_id: MSDUQ ID
 * @svc_id: Service class ID
 *
 * Required params were collected and passed to notify_deactivate_msduq to
 * notify reactivate failure to NW Manager
 *
 * Return: QDF_STATUS_SUCCESS on success
 *
 */
QDF_STATUS
dp_sawf_notify_deactivate_msduq(struct dp_soc *soc, struct dp_peer *peer,
				uint8_t q_id, uint8_t svc_id);

/**
 * dp_sawf_notify_nw_manager() - Check for MSDUQ with no resp to notify
 * NW Manager.
 * @soc: SOC handle
 * @peer: dp peer handle
 * @no_resp_q: Array to indicate MSDUQ with no response
 *
 * Checks for the MSDUQ with no response from tgt and notify NW Manager.
 *
 * Return: Void
 */
void dp_sawf_notify_nw_manager(struct dp_soc *soc, struct dp_peer *peer,
			       uint8_t *no_resp_q);

/**
 * dp_soc_sawf_msduq_timer_init() - Initialize the SAWF msduq timer.
 * @soc: data path SOC handle
 *
 * Return: void
 */
void dp_soc_sawf_msduq_timer_init(struct dp_soc *soc);

/**
 * dp_soc_sawf_msduq_timer_deinit() - De-Initialize the SAWF msduq timer.
 * @soc: data path SoC handle
 *
 * Return: void
 */
void dp_soc_sawf_msduq_timer_deinit(struct dp_soc *soc);

void dp_soc_sawf_init(struct dp_soc *soc);

void dp_soc_sawf_deinit(struct dp_soc *soc);
#endif /* DP_SAWF_H*/
