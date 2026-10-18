/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <dp_types.h>
#include <dp_peer.h>
#include <dp_internal.h>
#include <dp_htt.h>
#include <dp_sawf_htt.h>
#include <dp_sawf.h>
#include <dp_hist.h>
#include <hal_tx.h>
#include "hal_hw_headers.h"
#include "hal_api.h"
#include "hal_rx.h"
#include "qdf_trace.h"
#include "dp_tx.h"
#include "dp_peer.h"
#include "dp_internal.h"
#include "osif_private.h"
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_telemetry_agent.h>
#include <wlan_sawf.h>

/**
 ** SAWF_metadata related information.
 **/
#define SAWF_VALID_TAG 0xAA
#define SAWF_TAG_SHIFT 0x18
#define SAWF_SERVICE_CLASS_SHIFT 0x10
#define SAWF_SERVICE_CLASS_MASK 0xff
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
#define SAWF_NW_DELAY_MASK 0x3ff
#else
#define SAWF_NW_DELAY_MASK 0x3ffff
#endif
#define SAWF_NW_DELAY_SHIFT 0x6
#define SAWF_MSDUQ_MASK 0x3f

/**
 ** SAWF_metadata extraction.
 **/
#define SAWF_TAG_GET(x) ((x) >> SAWF_TAG_SHIFT)
#ifdef ENABLE_CFG80211_BACKPORTS_MLO
#define SAWF_SERVICE_CLASS_GET(x) 0
#else
#define SAWF_SERVICE_CLASS_GET(x) (((x) >> SAWF_SERVICE_CLASS_SHIFT) \
	& SAWF_SERVICE_CLASS_MASK)
#endif
#define SAWF_MSDUQ_GET(x) ((x) & SAWF_MSDUQ_MASK)
#define SAWF_TAG_IS_VALID(x) \
	((SAWF_TAG_GET(x) == SAWF_VALID_TAG) ? true : false)

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
#define SAWF_NW_DELAY_SET(__metadata, __nw_delay, __q_id) \
	DP_SAWF_METADATA_SET(__metadata, 0, ((__nw_delay << SAWF_NW_DELAY_SHIFT) | __q_id))
#else
#define SAWF_NW_DELAY_SET(__metadata, __nw_delay, __q_id) \
	((SAWF_VALID_TAG << SAWF_TAG_SHIFT) | \
	((__nw_delay) << SAWF_NW_DELAY_SHIFT) | __q_id)
#endif
#define SAWF_NW_DELAY_GET(x) (((x) >> SAWF_NW_DELAY_SHIFT) \
	& SAWF_NW_DELAY_MASK)

#define DP_TX_TCL_METADATA_TYPE_SET(_var, _val) \
	HTT_TX_TCL_METADATA_TYPE_V2_SET(_var, _val)
#define DP_TCL_METADATA_TYPE_SVC_ID_BASED \
	HTT_TCL_METADATA_V2_TYPE_SVC_ID_BASED

#define SAWF_TELEMETRY_MOV_AVG_PACKETS 1000
#define SAWF_TELEMETRY_MOV_AVG_WINDOWS 10

#define SAWF_TELEMETRY_SLA_PACKETS 100000
#define SAWF_TELEMETRY_SLA_TIME    10

#define SAWF_BASIC_STATS_ENABLED(x) \
	(((x) & (0x1 << SAWF_STATS_BASIC)) ? true : false)
#define SAWF_ADVNCD_STATS_ENABLED(x) \
	(((x) & (0x1 << SAWF_STATS_ADVNCD)) ? true : false)
#define SAWF_LATENCY_STATS_ENABLED(x) \
	(((x) & (0x1 << SAWF_STATS_LATENCY)) ? true : false)

#define SAWF_FLOW_START 1
#define SAWF_FLOW_STOP  2
#define SAWF_FLOW_DEPRIORITIZE 3
#define DP_RETRY_COUNT 7

#define DP_SAWF_MIN_SVC_ID 1
#define DP_SAWF_MAX_SVC_ID 128

void dp_soc_sawf_init(struct dp_soc *soc)
{
	qdf_spinlock_create(&soc->sawf_flow_sync_lock);
}

void dp_soc_sawf_deinit(struct dp_soc *soc)
{
	qdf_spinlock_destroy(&soc->sawf_flow_sync_lock);
}

uint16_t dp_sawf_msduq_peer_id_set(uint16_t peer_id, uint8_t msduq)
{
	uint16_t peer_msduq = 0;

	peer_msduq |= (peer_id & SAWF_PEER_ID_MASK) << SAWF_PEER_ID_SHIFT;
	peer_msduq |= (msduq & SAWF_MSDUQ_MASK);
	return peer_msduq;
}

#ifdef ENABLE_CFG80211_BACKPORTS_MLO
bool dp_sawf_tag_valid_get(qdf_nbuf_t nbuf)
{
	uint32_t mark;

	mark = qdf_nbuf_get_mark(nbuf);

	if (SAWF_TAG_IS_VALID(mark) && (mark & SAWF_META_VALID_MASK))
		return true;

	return false;
}
#else
bool dp_sawf_tag_valid_get(qdf_nbuf_t nbuf)
{
	if (SAWF_TAG_IS_VALID(qdf_nbuf_get_mark(nbuf)))
		return true;

	return false;
}
#endif

uint32_t dp_sawf_queue_id_get(qdf_nbuf_t nbuf)
{
	uint32_t mark = qdf_nbuf_get_mark(nbuf);
	uint8_t msduq = 0;

	msduq = SAWF_MSDUQ_GET(mark);

	if (!SAWF_TAG_IS_VALID(mark) || msduq == SAWF_MSDUQ_MASK)
		return DP_SAWF_DEFAULT_Q_INVALID;

	return msduq;
}

/*
 * dp_sawf_tid_get() - Get TID from MSDU Queue
 * @queue_id: MSDU queue ID
 *
 * @Return: TID
 */
uint8_t dp_sawf_tid_get(uint16_t queue_id)
{
    uint8_t tid;

    tid = ((queue_id) - DP_SAWF_DEFAULT_Q_MAX) & (DP_SAWF_TID_MAX - 1);
    return tid;
}

static uint8_t
dp_sawf_get_svc_id(struct dp_peer *peer, uint8_t msduq_id)
{
	struct dp_peer_sawf *sawf_ctx;
	uint8_t msduq_idx;
	uint8_t svc_id = DP_SAWF_INVALID_SVC_ID;

	if (msduq_id < DP_SAWF_DEFAULT_Q_MAX) {
		dp_sawf_err("Invalid msduq_id in mark %u", msduq_id);
		return DP_SAWF_INVALID_SVC_ID;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx)
		return DP_SAWF_INVALID_SVC_ID;

	msduq_idx = msduq_id - DP_SAWF_DEFAULT_Q_MAX;

	svc_id = sawf_ctx->msduq[msduq_idx].svc_id;

	return svc_id;
}

uint16_t dp_sawf_tcl_cmd(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
			 bool is_fast_tx)
{
	qdf_nbuf_t nbuf = tx_desc->nbuf;
	uint32_t mark = qdf_nbuf_get_mark(nbuf);
	uint8_t service_id = SAWF_SERVICE_CLASS_GET(mark);
	uint16_t peer_id = SAWF_PEER_ID_GET(mark);
	uint8_t msduq_id = SAWF_MSDUQ_GET(mark);
	uint16_t htt_tcl_metadata = 0;
	struct dp_peer *peer = NULL;

	if (!service_id) {
		peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
		if (!peer) {
			dp_sawf_debug("Invalid peer_id %u", peer_id);
			return DP_SAWF_INVALID_TCL_CMD;
		}

		service_id = dp_sawf_get_svc_id(peer, msduq_id);
	}

	if (service_id < DP_SAWF_MIN_SVC_ID ||
	    service_id > DP_SAWF_MAX_SVC_ID) {
		dp_sawf_err("Invalid svc id %u", service_id);
		if (peer)
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return DP_SAWF_INVALID_TCL_CMD;
	}

	if (!is_fast_tx) {
		if (!peer) {
			peer = dp_peer_get_ref_by_id(soc, peer_id,
						     DP_MOD_ID_SAWF);
			if (!peer) {
				dp_sawf_debug("Invalid peer_id %u", peer_id);
				return DP_SAWF_INVALID_TCL_CMD;
			}
		}

		dp_sawf_tx_enqueue_peer_stats(soc, peer, tx_desc);
	}

	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	DP_TX_TCL_METADATA_TYPE_SET(htt_tcl_metadata,
				    DP_TCL_METADATA_TYPE_SVC_ID_BASED);
	HTT_TX_FLOW_METADATA_TID_OVERRIDE_SET(htt_tcl_metadata, 1);
	HTT_TX_TCL_METADATA_SVC_CLASS_ID_SET(htt_tcl_metadata, service_id - 1);

	return htt_tcl_metadata;
}

QDF_STATUS
dp_peer_sawf_ctx_alloc(struct dp_soc *soc,
		       struct dp_peer *peer)
{
	struct dp_peer_sawf *sawf_ctx;
	uint8_t index = 0;

	/*
	 * In MLO case, primary link peer holds SAWF ctx.
	 * Secondary link peers does not hold SAWF ctx.
	 */
	if (!dp_peer_is_primary_link_peer(peer)) {
		peer->sawf = NULL;
		return QDF_STATUS_SUCCESS;
	}

	sawf_ctx = qdf_mem_malloc(sizeof(struct dp_peer_sawf));
	if (!sawf_ctx) {
		dp_sawf_err("Failed to allocate peer SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	for (index = 0; index < DP_SAWF_MAX_DYNAMIC_AST; index++)
		sawf_ctx->dynamic_ast_idx[index] = DP_SAWF_INVALID_AST_IDX;

	for (index = 0; index < DP_SAWF_Q_MAX; index++)
		qdf_atomic_init(&sawf_ctx->msduq[index].ref_count);

	qdf_spinlock_create(&sawf_ctx->sawf_peer_lock);

	peer->sawf = sawf_ctx;

	return QDF_STATUS_SUCCESS;
}

static inline void dp_peer_sawf_dec_svc_msduq_count(struct dp_soc *soc,
						    struct dp_peer *peer)
{
	uint8_t q_id;
	uint32_t svc_id;
	struct cdp_soc_t *cdp_soc = &soc->cdp_soc;

	for (q_id = 0; q_id < DP_SAWF_Q_MAX; q_id++) {
		svc_id = dp_sawf(peer, q_id, svc_id);
		if (svc_id) {
			wlan_service_id_dec_msduq_count(svc_id);
			dp_sawf_debug("Service class ID:%d, ref_count:%d, "
				      "msduq_count:%d", svc_id,
				      wlan_service_id_get_ref_count(svc_id),
				      wlan_service_id_get_msduq_count(svc_id));

			if (!wlan_service_id_is_used(svc_id)) {
				if (soc->cdp_soc.ol_ops->disable_sawf_svc)
					cdp_soc->ol_ops->disable_sawf_svc(
									svc_id);
				dp_sawf_debug("Service class ID:%d is disabled",
					      svc_id);
				telemetry_sawf_set_svclass_cfg(false, svc_id, 0,
							       0, 0, 0, 0, 0, 0);
				wlan_disable_service_class(svc_id);
			}
		}
	}

}

QDF_STATUS
dp_peer_sawf_ctx_free(struct dp_soc *soc,
		      struct dp_peer *peer)
{
	if (!peer->sawf) {
		/*
		 * In MLO case, primary link peer holds SAWF ctx.
		 */
		if (!dp_peer_is_primary_link_peer(peer)) {
			return QDF_STATUS_SUCCESS;
		}
		dp_sawf_err("Failed to free peer SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	if (peer->sawf->telemetry_ctx)
		telemetry_sawf_peer_ctx_free(peer->sawf->telemetry_ctx);

	dp_peer_sawf_dec_svc_msduq_count(soc, peer);
	qdf_spinlock_destroy(&peer->sawf->sawf_peer_lock);
	qdf_mem_free(peer->sawf);

	return QDF_STATUS_SUCCESS;
}

struct dp_peer_sawf *dp_peer_sawf_ctx_get(struct dp_peer *peer)
{
	struct dp_peer_sawf *sawf_ctx;

	sawf_ctx = peer->sawf;
	if (!sawf_ctx) {
		dp_sawf_err("Failed to allocate peer SAWF ctx");
		return NULL;
	}

	return sawf_ctx;
}

QDF_STATUS
dp_sawf_get_peer_msduq_info(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			    uint8_t svc_id, uint8_t debug_level)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t q_idx;
	struct sawf_tx_stats tx_stats;
	struct sawf_delay_stats delay_stats;
	bool is_print_per_svc = false;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (peer->bss_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF ctx");
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	dp_sawf_debug("svc_id:%d, Debug level:%d", svc_id, debug_level);

	if (svc_id >= DP_SAWF_SVC_CLASS_MIN && svc_id <= DP_SAWF_SVC_CLASS_MAX)
		is_print_per_svc = true;

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);

	if (debug_level >= SAWF_MSDUQ_DBG_LVL_INFO) {
		dp_sawf_info("Peer: "QDF_MAC_ADDR_FMT"",
			     QDF_MAC_ADDR_REF(mac_addr));

		dp_sawf_nofl_err("------------------------------------");
		dp_sawf_nofl_err("| Queue | TID | TID Queue | SVC ID |");
		dp_sawf_nofl_err("------------------------------------");

		for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
			msduq = &sawf_ctx->msduq[q_idx];
			if (is_print_per_svc && msduq->svc_id != svc_id)
				continue;
			if (msduq->q_state != SAWF_MSDUQ_IN_USE)
				continue;
			dp_sawf_nofl_err("|  %u   |  %u  |    %u      |   %u    |",
					 q_idx + DP_SAWF_DEFAULT_Q_MAX,
					 msduq->remapped_tid, msduq->htt_msduq,
					 msduq->svc_id);
		}
		dp_sawf_nofl_err("------------------------------------");
	}

	if (debug_level >= SAWF_MSDUQ_DBG_LVL_DEBUG) {
		dp_sawf_nofl_err("\n");
		dp_sawf_nofl_err("------------------------------------------------------------------");
		dp_sawf_nofl_err("| Queue_id | tgt_opaque_id |  map_done  |  ref_count  |   state   |");
		dp_sawf_nofl_err("-------------------------------------------------------------------");

		for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
			msduq = &sawf_ctx->msduq[q_idx];
			if (is_print_per_svc && msduq->svc_id != svc_id)
				continue;
			if (msduq->q_state == SAWF_MSDUQ_UNUSED)
				continue;
			dp_sawf_nofl_err("|    %d    |       0x%x      |      %d      |     %d    |     %s    ",
					  q_idx + DP_SAWF_DEFAULT_Q_MAX,
					  msduq->tgt_opaque_id,
					  msduq->map_done,
					  qdf_atomic_read(&msduq->ref_count),
					  dp_sawf_msduq_state_to_string(
					  msduq->q_state));
		}
		dp_sawf_nofl_err("-------------------------------------------------------------------");
	}

	if (debug_level >= SAWF_MDSUQ_DBG_LVL_TRACE) {
#ifdef SAWF_MSDUQ_DEBUG
		dp_sawf_nofl_err("\n");
		dp_sawf_nofl_err("SAWF HTT DE-ACTIVATE COUNTERS:");
		dp_sawf_nofl_err("-------------------------------------");
		dp_sawf_nofl_err("| Queue_id | recv_failure | timeout |");
		dp_sawf_nofl_err("-------------------------------------");

		for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
			msduq = &sawf_ctx->msduq[q_idx];
			if (is_print_per_svc && msduq->svc_id != svc_id)
				continue;
			if ((msduq->q_state == SAWF_MSDUQ_UNUSED) ||
			    (msduq->q_state == SAWF_MSDUQ_DEACTIVATED))
				continue;
			dp_sawf_nofl_err("|     %d      |      %d      |      %d     |",
					 q_idx + DP_SAWF_DEFAULT_Q_MAX,
					 msduq->deactivate_stats.recv_failure,
					 msduq->deactivate_stats.timeout);
		}

		dp_sawf_nofl_err("\n");
		dp_sawf_nofl_err("SAWF HTT RE-ACTIVATE COUNTERS:");
		dp_sawf_nofl_err("-------------------------------------");
		dp_sawf_nofl_err("| Queue_id | recv_failure | timeout |");
		dp_sawf_nofl_err("-------------------------------------");

		for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
			msduq = &sawf_ctx->msduq[q_idx];
			if (is_print_per_svc && msduq->svc_id != svc_id)
				continue;
			if ((msduq->q_state == SAWF_MSDUQ_UNUSED) ||
			    (msduq->q_state == SAWF_MSDUQ_DEACTIVATED))
				continue;
			dp_sawf_nofl_err("|    %d    |     %d      |      %d     |",
					 q_idx + DP_SAWF_DEFAULT_Q_MAX,
					 msduq->reactivate_stats.recv_failure,
					 msduq->reactivate_stats.timeout);
		}
		dp_sawf_nofl_err("-------------------------------------");
#endif

		for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
			msduq = &sawf_ctx->msduq[q_idx];
			if (is_print_per_svc && msduq->svc_id != svc_id)
				continue;
			if (msduq->q_state != SAWF_MSDUQ_IN_USE)
				continue;
			dp_sawf_get_peer_tx_stats(soc_hdl, msduq->svc_id,
						  mac_addr, &tx_stats);
			dp_sawf_get_peer_delay_stats(soc_hdl, msduq->svc_id,
						     mac_addr, &delay_stats);
		}
	}

	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_def_queues_get_map_report(struct cdp_soc_t *soc_hdl,
				  uint8_t *mac_addr)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint8_t tid, tid_active, svc_id;
	struct dp_peer_sawf *sawf_ctx;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF ctx");
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	dp_sawf_info("Peer " QDF_MAC_ADDR_FMT,
		     QDF_MAC_ADDR_REF(mac_addr));
	dp_sawf_nofl_err("TID    Active    Service Class ID");
	for (tid = 0; tid < DP_SAWF_TID_MAX; ++tid) {
		svc_id = sawf_ctx->tid_reports[tid].svc_class_id;
		tid_active = svc_id &&
			     (svc_id != HTT_SAWF_SVC_CLASS_INVALID_ID);
		dp_sawf_nofl_err("%u        %u            %u",
				 tid, tid_active, svc_id);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_def_queues_map_req(struct cdp_soc_t *soc_hdl,
			   uint8_t *mac_addr, uint8_t svc_class_id)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint16_t peer_id;
	QDF_STATUS status;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	peer_id = peer->peer_id;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	dp_sawf_info("peer " QDF_MAC_ADDR_FMT "svc id %u peer id %u",
		 QDF_MAC_ADDR_REF(mac_addr), svc_class_id, peer_id);

	status = dp_htt_h2t_sawf_def_queues_map_req(dp_soc->htt_handle,
						    svc_class_id, peer_id);

	if (status != QDF_STATUS_SUCCESS)
		return status;

	/*
	 * Request map repot conf from FW for all TIDs
	 */
	return dp_htt_h2t_sawf_def_queues_map_report_req(dp_soc->htt_handle,
							 peer_id, 0xff);
}

QDF_STATUS
dp_sawf_def_queues_unmap_req(struct cdp_soc_t *soc_hdl,
			     uint8_t *mac_addr, uint8_t svc_id)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer;
	uint16_t peer_id;
	QDF_STATUS status;
	uint8_t wildcard_mac[QDF_MAC_ADDR_SIZE] = {0xff, 0xff, 0xff,
						   0xff, 0xff, 0xff};

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	if (!qdf_mem_cmp(mac_addr, wildcard_mac, QDF_MAC_ADDR_SIZE)) {
		/* wildcard unmap */
		peer_id = HTT_H2T_SAWF_DEF_QUEUES_UNMAP_PEER_ID_WILDCARD;
	} else {
		peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
					      DP_VDEV_ALL, DP_MOD_ID_CDP);
		if (!peer) {
			dp_sawf_err("Invalid peer");
			return QDF_STATUS_E_FAILURE;
		}
		peer_id = peer->peer_id;
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	}

	dp_sawf_info("peer " QDF_MAC_ADDR_FMT "svc id %u peer id %u",
		     QDF_MAC_ADDR_REF(mac_addr), svc_id, peer_id);

	status =  dp_htt_h2t_sawf_def_queues_unmap_req(dp_soc->htt_handle,
						       svc_id, peer_id);

	if (status != QDF_STATUS_SUCCESS)
		return status;

	/*
	 * Request map repot conf from FW for all TIDs
	 */
	return dp_htt_h2t_sawf_def_queues_map_report_req(dp_soc->htt_handle,
							 peer_id, 0xff);
}

QDF_STATUS
dp_sawf_get_msduq_map_info(struct dp_soc *soc, uint16_t peer_id,
			   uint8_t host_q_idx,
			   uint8_t *remaped_tid, uint8_t *target_q_idx)
{
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t tid, q_idx;
	uint8_t mdsuq_index = 0;

	mdsuq_index = host_q_idx - DP_SAWF_DEFAULT_Q_MAX;

	if (mdsuq_index >= DP_SAWF_Q_MAX) {
		dp_sawf_err("Invalid Host Queue Index");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer for peer_id %d", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("ctx doesn't exist");
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAILURE;
	}

	msduq = &sawf_ctx->msduq[mdsuq_index];
	/*
	 * Find tid and msdu queue idx from host msdu queue number
	 * host idx to be taken from the tx descriptor
	 */
	tid = msduq->remapped_tid;
	q_idx = msduq->htt_msduq;

	if (remaped_tid)
		*remaped_tid = tid;

	if (target_q_idx)
		*target_q_idx = q_idx;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

uint16_t dp_sawf_get_peerid(struct dp_soc *soc, uint8_t *dest_mac,
			    uint8_t vdev_id)
{
	struct dp_ast_entry *ast_entry = NULL;
	uint16_t peer_id;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_by_vdevid(soc, dest_mac, vdev_id);

	if (!ast_entry) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		qdf_warn("NULL ast entry for dest mac addr: " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(dest_mac));
		return HTT_INVALID_PEER;
	}

	peer_id = ast_entry->peer_id;
	qdf_spin_unlock_bh(&soc->ast_lock);
	return peer_id;
}

bool dp_sawf_get_search_index(struct dp_soc *soc, uint8_t vdev_id,
			      uint16_t peer_id, uint16_t queue_id,
			      uint32_t *flow_index)
{
	struct dp_peer *peer = NULL;
	uint8_t index;
	uint16_t dyn_ast_idx = 0;
	bool status = false;

	index = (queue_id - DP_SAWF_DEFAULT_Q_MAX) / DP_SAWF_TID_MAX;
	if (index > DP_PEER_AST_FLOWQ_LOW_PRIO) {
		dp_sawf_warn("Invalid index:%d queue_id:%u", index, queue_id);
		return status;
	}

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_warn("NULL peer peer_id %u", peer_id);
		return status;
	}

	dyn_ast_idx = peer->sawf->dynamic_ast_idx[index];

	if (dyn_ast_idx == DP_SAWF_INVALID_AST_IDX) {
		*flow_index =  peer->peer_ast_flowq_idx[0].ast_idx;
	} else {
		*flow_index =  dyn_ast_idx;
		status = true;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return status;
}

static struct dp_peer *dp_find_peer_by_destmac(struct dp_soc *soc,
		uint8_t *dest_mac,
		uint8_t vdev_id,
		enum dp_mod_id mod_id)
{
	bool ast_ind_disable = wlan_cfg_get_ast_indication_disable(
							soc->wlan_cfg_ctx);

	if ((!soc->ast_offload_support) || (!ast_ind_disable)) {
		struct dp_ast_entry *ast_entry = NULL;
		uint16_t peer_id;

		qdf_spin_lock_bh(&soc->ast_lock);
		if (vdev_id == DP_VDEV_ALL)
			ast_entry = dp_peer_ast_hash_find_soc(soc, dest_mac);
		else
			ast_entry = dp_peer_ast_hash_find_by_vdevid
					(soc, dest_mac, vdev_id);

		if (!ast_entry) {
			qdf_spin_unlock_bh(&soc->ast_lock);
			dp_err("NULL ast entry");
			return NULL;
		}

		peer_id = ast_entry->peer_id;
		qdf_spin_unlock_bh(&soc->ast_lock);

		if (peer_id == HTT_INVALID_PEER)
			return NULL;

		return dp_peer_get_ref_by_id(soc, peer_id, mod_id);
	} else {
		struct cdp_peer_info peer_info = {0};

		DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, dest_mac, false,
					 CDP_WILD_PEER_TYPE);

		return dp_peer_hash_find_wrapper(soc, &peer_info, mod_id);
	}
}

static void
dp_sawf_peer_msduq_q_state_update(struct dp_sawf_msduq *msduq, uint8_t q_id,
				  uint8_t new_q_state)
{
	dp_sawf_info("msduq:%d, q_state:%s -> %s", q_id,
		     dp_sawf_msduq_state_to_string(msduq->q_state),
		     dp_sawf_msduq_state_to_string(new_q_state));
	msduq->q_state = new_q_state;
}

static void
dp_sawf_peer_msduq_reconfigure(struct dp_soc *dpsoc, struct dp_peer *peer,
			       struct dp_sawf_msduq *msduq, uint8_t q_id,
			       HTT_MSDUQ_DEACTIVATE_E q_action)
{
	uint8_t current_q_state, new_q_state;
	uint16_t peer_id;

	peer_id = peer->peer_id;
	current_q_state = msduq->q_state;

	/* If tgt_opaque_id is not initialized HTT send needs to be stopped */
	if (!msduq->map_done) {
		dp_sawf_err("peer:%d | Uninitialized tgt_opaque_id", peer_id);
		return;
	}

	if (q_action == HTT_MSDUQ_DEACTIVATE) {
		if (current_q_state == SAWF_MSDUQ_IN_USE) {
			dp_sawf_info("peer:%d | msduq:%d, Deactivate Queue - Send HTT to "
				     "FW", peer_id, q_id);
			new_q_state = SAWF_MSDUQ_DEACTIVATE_PENDING;
			dp_sawf_peer_msduq_q_state_update(msduq, q_id,
							  new_q_state);
			dp_htt_sawf_msduq_recfg_req(dpsoc->htt_handle, peer,
						    msduq, q_id, q_action);
		}
	} else if (q_action == HTT_MSDUQ_REACTIVATE) {
		if (current_q_state == SAWF_MSDUQ_DEACTIVATED) {
			dp_sawf_info("peer:%d | msduq:%d, Reactivate Queue - Send HTT to "
				     "FW", peer_id, q_id);
			new_q_state = SAWF_MSDUQ_REACTIVATE_PENDING;
			dp_sawf_peer_msduq_q_state_update(msduq, q_id,
							  new_q_state);
			dp_htt_sawf_msduq_recfg_req(dpsoc->htt_handle, peer,
						    msduq, q_id, q_action);
		}
	}
}

QDF_STATUS
dp_sawf_peer_flow_count(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			uint8_t svc_id, uint8_t direction,
			uint8_t start_or_stop, uint8_t *peer_mac,
			uint16_t peer_id, uint16_t flow_count)
{
	struct dp_soc *dpsoc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer, *mld_peer, *primary_link_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t current_q_state;
	bool match_found = false;
	uint8_t q_idx;

	qdf_spin_lock_bh(&dpsoc->sawf_flow_sync_lock);

	dp_sawf_info("peer:%d | mac_addr: "QDF_MAC_ADDR_FMT" svc_id %u direction %u "
		     "start_or_stop %u, flow_count %u", peer_id,
		     QDF_MAC_ADDR_REF(mac_addr), svc_id, direction,
		     start_or_stop, flow_count);

	if (peer_id != HTT_INVALID_PEER)
		peer = dp_peer_get_ref_by_id(dpsoc, peer_id, DP_MOD_ID_SAWF);
	else
		peer = dp_find_peer_by_destmac(dpsoc, mac_addr, DP_VDEV_ALL,
					       DP_MOD_ID_SAWF);

	if (!peer) {
		dp_sawf_debug("Peer is NULL");
		goto fail;
	}

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;
	else
		mld_peer = NULL;

	if (mld_peer) {
		primary_link_peer = dp_get_primary_link_peer_by_id
			(dpsoc, mld_peer->peer_id, DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_sawf_err("Invalid primary peer");
			goto fail;
		}

		/*
		 * Release the MLD-peer reference.
		 * Hold only primary link ref now.
		 */
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		peer = primary_link_peer;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("sawf_ctx doesn't exist");
		goto fail;
	}

	for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
		msduq = &sawf_ctx->msduq[q_idx];
		current_q_state = msduq->q_state;
		if ((msduq->svc_id == svc_id) &&
		    ((current_q_state == SAWF_MSDUQ_IN_USE) ||
		     (current_q_state == SAWF_MSDUQ_REACTIVATE_PENDING))) {
			match_found = true;
			break;
		}
	}

	if (match_found) {
		dp_sawf_debug("peer:%d | Match Found, q_idx:%d  MSDUQ ref_count:%u",
			      peer_id, q_idx, qdf_atomic_read(&msduq->ref_count));
		if (start_or_stop == SAWF_FLOW_START) {
			if (wlan_cfg_get_sawf_msduq_reclaim_config(
							dpsoc->wlan_cfg_ctx)) {
				if (msduq->is_deactivation_needed) {
					msduq->is_deactivation_needed = 0;
					dp_sawf_debug("peer:%d | "
						      "Reset Deactivation Flag",
						      peer_id);
				}
			}
			/*
			 * Increment MSDUQ refcount and send add notification
			 * when refcount is 1
			 */
			qdf_atomic_add(flow_count, &msduq->ref_count);
			if (qdf_atomic_read(&msduq->ref_count) == 1) {
				dp_sawf_debug("peer:%d | MSDUQ Ref_count: 0->1",
					      peer_id);
				dp_sawf_peer_msduq_event_notify(dpsoc, peer, q_idx,
								svc_id,
								SAWF_PEER_MSDUQ_ADD_EVENT);
			}
		} else if (start_or_stop == SAWF_FLOW_STOP ||
			   start_or_stop == SAWF_FLOW_DEPRIORITIZE) {
			if ((qdf_atomic_read(&msduq->ref_count) >= flow_count)) {
				/*
				 * Decrement MSDUQ refcount and send delete
				 * notification when refcount becomes 0
				 */
				qdf_atomic_sub(flow_count, &msduq->ref_count);
				if (!qdf_atomic_read(&msduq->ref_count)) {
					    dp_sawf_peer_msduq_event_notify(dpsoc, peer,
									    q_idx, svc_id,
									    SAWF_PEER_MSDUQ_DELETE_EVENT);
					if (wlan_cfg_get_sawf_msduq_reclaim_config(dpsoc->wlan_cfg_ctx)) {
						/*
						 * Send MSDUQ Deactivate cmd to FW only
						 * when the request is triggered with
						 * SAWF_FLOW_DEPRIORITIZE cmd.
						 */
						if ((current_q_state == SAWF_MSDUQ_IN_USE) &&
						    (start_or_stop == SAWF_FLOW_DEPRIORITIZE)) {
							dp_sawf_info("peer:%d | Deactivate request via"
								     " Flow Deprioritize",
								     peer_id);
							dp_sawf_peer_msduq_reconfigure(
								dpsoc, peer, msduq, q_idx,
								HTT_MSDUQ_DEACTIVATE);
						}
						dp_sawf_debug("peer:%d | msduq:%d, q_state:%s",
							      peer_id, q_idx,
							      dp_sawf_msduq_state_to_string(
										msduq->q_state));
					}
				}
			}
		}
		dp_sawf_debug("peer:%d | MSDUQ Ref_count:%d",
			      peer_id, qdf_atomic_read(&msduq->ref_count));
	}

	qdf_spin_unlock_bh(&dpsoc->sawf_flow_sync_lock);

	if (peer_mac)
		qdf_mem_copy(peer_mac, peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_SUCCESS;
fail:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	qdf_spin_unlock_bh(&dpsoc->sawf_flow_sync_lock);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_peer_config_ul(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
		       uint8_t tid, uint32_t service_interval,
		       uint32_t burst_size, uint32_t min_tput,
		       uint32_t max_latency, uint8_t add_or_sub,
		       uint16_t peer_id)
{
	struct dp_soc *dpsoc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;
	struct dp_peer *peer;
	QDF_STATUS status;

	if (peer_id != HTT_INVALID_PEER)
		peer = dp_peer_get_ref_by_id(dpsoc, peer_id, DP_MOD_ID_SAWF);
	else
		peer = dp_find_peer_by_destmac(dpsoc, mac_addr, DP_VDEV_ALL,
					       DP_MOD_ID_SAWF);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	vdev = peer->vdev;

	status = soc_hdl->ol_ops->peer_update_sawf_ul_params(dpsoc->ctrl_psoc,
			vdev->vdev_id, mac_addr,
			tid, TID_TO_WME_AC(tid),
			service_interval, burst_size, min_tput, max_latency,
			add_or_sub);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO_3_LINK_TX
static bool dp_is_mlo_3_link_peer(struct dp_peer *peer)
{
	struct dp_peer *mld_peer = NULL;
	uint8_t i = 0;
	uint8_t num_links = 0;

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;
	else
		return false;

	if (!mld_peer) {
		dp_sawf_err("mld peer is null for MLO capable peer");
		return false;
	}

	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		if (mld_peer->link_peers[i].is_valid &&
		    !mld_peer->link_peers[i].is_bridge_peer)
			num_links++;
	}

	if (num_links == 3)
		return true;

	return false;
}

static bool dp_is_mlo_3_link_war_needed(struct dp_peer *peer)
{
	struct dp_soc *primary_link_soc = NULL;
	uint32_t target_type;
	bool war_needed = true;
	struct dp_peer *mld_peer = NULL;

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;

	if (!mld_peer) {
		dp_sawf_debug("mld peer is null for MLO capable peer");
		return false;
	}

	if (!dp_is_mlo_3_link_peer(peer))
		return false;

	primary_link_soc = mld_peer->vdev->pdev->soc;
	target_type = hal_get_target_type(primary_link_soc->hal_soc);
	if (target_type != TARGET_TYPE_QCN9224)
		war_needed = false;

	return war_needed;
}

uint32_t dscp_tid_map[WMI_HOST_DSCP_MAP_MAX] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 0, 1, 0, 1, 0, 1,
	0, 2, 3, 2, 3, 2, 3, 2,
	4, 3, 4, 3, 4, 3, 4, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 6, 5, 6, 5,
	7, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
};

static inline uint16_t
dp_sawf_get_3_link_best_suitable_tid(struct dp_peer *peer,
				     uint16_t tid1,
				     uint16_t tid2)
{
	uint64_t tid1_weight = 0;
	uint64_t tid2_weight = 0;

	/* if no weights are feeded fall back to equal distribution */
	if (!peer->tid_weight[tid1] || !peer->tid_weight[tid2]) {
		tid1_weight = peer->flow_cnt[tid1];
		tid2_weight = peer->flow_cnt[tid2];
		goto tid_selection;
	}

	/* for the first two flows fall back to equal distribution */
	if (!peer->flow_cnt[tid1] || !peer->flow_cnt[tid2]) {
		tid1_weight = peer->flow_cnt[tid1];
		tid2_weight = peer->flow_cnt[tid2];
		goto tid_selection;
	}

	/* weight based distribution */
	tid1_weight = peer->flow_cnt[tid1] * peer->tid_weight[tid2];
	tid2_weight = peer->flow_cnt[tid2] * peer->tid_weight[tid1];

tid_selection:
	if (tid1_weight > tid2_weight)
		return tid2;
	else
		return tid1;
}

static uint16_t dp_sawf_get_3_link_queue_id(struct dp_peer *peer, uint16_t tid)
{
	uint16_t queue_id = DP_SAWF_PEER_Q_INVALID;
	uint8_t ac = 0;

	ac = TID_TO_WME_AC(tid);
	qdf_spin_lock_bh(&peer->flow_info_lock);

	switch (ac) {
	case WME_AC_BE:
		queue_id = dp_sawf_get_3_link_best_suitable_tid(peer, 0, 3);
		break;
	case WME_AC_BK:
		queue_id = dp_sawf_get_3_link_best_suitable_tid(peer, 1, 2);
		break;
	case WME_AC_VI:
		queue_id = dp_sawf_get_3_link_best_suitable_tid(peer, 4, 5);
		break;
	case WME_AC_VO:
		queue_id = dp_sawf_get_3_link_best_suitable_tid(peer, 6, 7);
		break;
	default:
		break;
	}
	if (queue_id != DP_SAWF_PEER_Q_INVALID) {
		if (queue_id < CDP_DATA_TID_MAX)
			peer->flow_cnt[queue_id]++;
		else
			queue_id = DP_SAWF_PEER_Q_INVALID;
	}

	qdf_spin_unlock_bh(&peer->flow_info_lock);

	return queue_id;
}

uint16_t dp_sawf_get_peer_msduq(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				uint8_t *peer_mac,
				uint32_t dscp_pcp, bool pcp)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer = NULL;
	struct dp_peer *mld_peer = NULL;
	uint16_t queue_id = DP_SAWF_PEER_Q_INVALID;

	peer = dp_find_peer_by_destmac(soc, peer_mac, vdev_id,
				       DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_debug("Peer is NULL");
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (!dp_is_mlo_3_link_war_needed(peer)) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;

	if (!mld_peer) {
		dp_sawf_debug("mld peer is null for MLO capable peer");
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (pcp)
		queue_id = dscp_pcp;
	else
		queue_id = dscp_tid_map[dscp_pcp];

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return dp_sawf_get_3_link_queue_id(mld_peer, queue_id);
}

QDF_STATUS
dp_sawf_3_link_peer_flow_count(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			       uint16_t peer_id, uint32_t mark_metadata)
{
	struct dp_soc *dpsoc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer, *mld_peer;
	uint8_t i = 0;
	uint8_t num_links = 0;
	uint16_t queue_id;

	dp_sawf_info("mac_addr: ", QDF_MAC_ADDR_FMT,
		     QDF_MAC_ADDR_REF(mac_addr));

	if (peer_id != HTT_INVALID_PEER)
		peer = dp_peer_get_ref_by_id(dpsoc, peer_id, DP_MOD_ID_SAWF);
	else
		peer = dp_find_peer_by_destmac(dpsoc, mac_addr, DP_VDEV_ALL,
					       DP_MOD_ID_SAWF);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	if (IS_MLO_DP_LINK_PEER(peer)) {
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	} else if (IS_MLO_DP_MLD_PEER(peer)) {
		mld_peer = peer;
	} else {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	if (!mld_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		if (mld_peer->link_peers[i].is_valid &&
		    !mld_peer->link_peers[i].is_bridge_peer)
			num_links++;
	}

	if (num_links != 3) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	queue_id = DP_SAWF_QUEUE_ID_GET(mark_metadata);
	if (queue_id < CDP_DATA_TID_MAX) {
		qdf_spin_lock_bh(&peer->flow_info_lock);
		if (mld_peer->flow_cnt[queue_id])
			mld_peer->flow_cnt[queue_id]--;

		qdf_spin_unlock_bh(&peer->flow_info_lock);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_3_link_peer_set_tid_weight(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
				   uint16_t peer_id, uint8_t tid_weight[])
{
	struct dp_soc *dpsoc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer, *mld_peer;
	uint8_t i = 0;

	dp_sawf_info("mac_addr: ", QDF_MAC_ADDR_FMT,
		     QDF_MAC_ADDR_REF(mac_addr));

	if (peer_id != HTT_INVALID_PEER)
		peer = dp_peer_get_ref_by_id(dpsoc, peer_id, DP_MOD_ID_SAWF);
	else
		peer = dp_find_peer_by_destmac(dpsoc, mac_addr, DP_VDEV_ALL,
					       DP_MOD_ID_SAWF);

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	if (IS_MLO_DP_LINK_PEER(peer)) {
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	} else if (IS_MLO_DP_MLD_PEER(peer)) {
		mld_peer = peer;
	} else {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	if (!dp_is_mlo_3_link_peer(peer)) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < CDP_DATA_TID_MAX; i++) {
		if (mld_peer->tid_weight[i] != tid_weight[i])
			mld_peer->tid_weight[i] = tid_weight[i];
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_11BE_MLO_3_LINK_SUPPORT */

QDF_STATUS
dp_sawf_notify_deactivate_msduq(struct dp_soc *soc, struct dp_peer *peer,
				uint8_t q_id, uint8_t svc_id)
{
	uint8_t pdev_id;
	uint16_t peer_id, msduq_peer_id;
	uint32_t host_q_id;
	struct dp_sawf_flow_deprioritize_params result_params = {0};
	bool is_mlo = false;

	if (IS_DP_LEGACY_PEER(peer)) {
		qdf_mem_copy(&result_params.peer_mac, &peer->mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
#ifdef WLAN_FEATURE_11BE_MLO
		struct dp_peer *mld_peer;

		is_mlo = true;
		if (IS_MLO_DP_LINK_PEER(peer))
			mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
		else if (IS_MLO_DP_MLD_PEER(peer))
			mld_peer = peer;
		else
			mld_peer = NULL;

		if (!mld_peer) {
			dp_sawf_err("invalid mld_peer");
			return QDF_STATUS_E_FAILURE;
		}
		qdf_mem_copy(&result_params.peer_mac, &mld_peer->mac_addr,
			     QDF_MAC_ADDR_SIZE);
#endif
	}

	pdev_id = peer->vdev->pdev->pdev_id;
	peer_id = peer->peer_id;
	host_q_id = q_id + DP_SAWF_DEFAULT_Q_MAX;
	msduq_peer_id = dp_sawf_msduq_peer_id_set(peer_id, host_q_id);
	DP_SAWF_METADATA_SET(result_params.mark_metadata, svc_id, msduq_peer_id);
	dp_sawf_info("peer:%d |is_mlo:%d | peer_mac:%pM |msduq:%d |svc_id:%d |mark_metadata 0x%x",
		     peer_id, is_mlo, result_params.peer_mac, q_id, svc_id,
		     result_params.mark_metadata);

	if (soc->cdp_soc.ol_ops->notify_deactivate_msduq)
		return soc->cdp_soc.ol_ops->notify_deactivate_msduq((void *)soc->ctrl_psoc,
								    pdev_id,
								    is_mlo,
								    &result_params);

	return QDF_STATUS_E_FAILURE;
}

void
dp_sawf_notify_nw_manager(struct dp_soc *soc, struct dp_peer *peer,
			  uint8_t *no_resp_q)
{
	uint8_t q_id = 0;
	struct dp_peer_sawf *sawf_ctx;

	sawf_ctx = peer->sawf;
	if (!sawf_ctx) {
		dp_sawf_err("sawf_ctx not present");
		return;
	}

	for (q_id = 0; q_id < DP_SAWF_Q_MAX; q_id++) {
		if (no_resp_q[q_id]) {
			dp_sawf_debug("no_resp_q [%d] is set", q_id);
			/* Inform NW Manager about Reactivate failure */
			if (dp_sawf_notify_deactivate_msduq(
			    soc, peer, q_id, sawf_ctx->msduq[q_id].svc_id) !=
							QDF_STATUS_SUCCESS)
				dp_sawf_err("Notify Reactivate failure to NW "
					    "Manager failed");
		}
	}
}

/*
 * dp_sawf_peer_msduq_htt_resp_timeout - Checks the MSDUQ state to trigger HTT
 * response timeout handling code.
 *
 * @soc: SOC handle
 * @peer: Peer handle
 * @msduq: MSDUQ structure
 * @q_id: MSDUQ ID
 * @no_resp_q: MSDUQ with no resp from tgt
 * @is_notify_needed: Flag to know if notify to Network manager is needed
 * for any one of the queues due to reactivate failure.
 *
 * When the timer expires, if the msduq state equals REACTIVATE_PENDING or
 * DEACTIVATE_PENDING,
 *	a. In the first expiry, no_resp_ind is set.
 *	b. In the next expirty, if the q_state is same and with
 *	no_resp_ind set, the state of the MSDUQ will
 *	fallback to corresponding prior states.
 *		- REACTIVATE_PENDING -> DEACTIVATED.
 *		Also, reactivation failed queues are marked in order to
 *		notify NW manager.
 *		- DEACTIVATE_PENDING -> IN_USE
 *
 * Return: Void
 */
static void
dp_sawf_peer_msduq_htt_resp_timeout(struct dp_soc *soc, struct dp_peer *peer,
				    struct dp_sawf_msduq *msduq, uint8_t q_id,
				    uint8_t *no_resp_q, bool *is_notify_needed)
{
	uint8_t current_q_state, new_q_state;
	uint16_t peer_id;

	peer_id = peer->peer_id;
	current_q_state = msduq->q_state;

	if ((current_q_state == SAWF_MSDUQ_DEACTIVATE_PENDING) ||
	    (current_q_state == SAWF_MSDUQ_REACTIVATE_PENDING)) {
		dp_sawf_debug("msduq:%d, q_state:%s, peer:%d", q_id,
			      dp_sawf_msduq_state_to_string(current_q_state),
			      peer_id);
		if (!msduq->no_resp_ind) {
			msduq->no_resp_ind = 1;
			dp_sawf_debug("peer:%d | Set no_resp_ind Flag",
				      peer_id);
		} else {
			msduq->no_resp_ind = 0;
			dp_sawf_debug("peer: %d | Reset no_resp_ind Flag",
				      peer_id);
			if (current_q_state == SAWF_MSDUQ_REACTIVATE_PENDING) {
				dp_sawf_err("peer:%d | HTT Reactivate timeout",
					    peer_id);
				DP_SAWF_MSDUQ_STATS_INC(reactivate_stats, timeout);
				/*
				 * Record MSDUQ with no response for Reactivate,
				 * to notify NW Manager.
				 */
				no_resp_q[q_id] = 1;
				new_q_state = SAWF_MSDUQ_DEACTIVATED;
				if (!(*is_notify_needed))
					*is_notify_needed = true;
			} else {
				dp_sawf_err("peer:%d | HTT Deactivate timeout",
					    peer_id);
				DP_SAWF_MSDUQ_STATS_INC(deactivate_stats, timeout);
				new_q_state = SAWF_MSDUQ_IN_USE;
			}

			dp_sawf_peer_msduq_q_state_update(msduq, q_id,
							  new_q_state);
		}
	}
}

/*
 * dp_sawf_peer_msduq_timer_update - Checks the queue to see if it is
 * in SAWF_MSDUQ_IN_USE state to deactivate it, if ref_count equals zero.
 *
 * @dp_soc: SOC handle
 * @peer: Peer handle
 * @msduq: MSDUQ structure
 * @q_id: MSDUQ ID
 *
 * If q_state is SAWF_MSDUQ_IN_USE and ref_count is zero, is_deactivation_needed
 * flag is set in the first timer expiry.
 * When the timer is expired in the next 20 seconds, if q_state is same and
 * still ref_count is zero, state is changed to SAWF_MSDUQ_DEACTIVATE_PENDING
 * and HTT command is sent to FW for deactivation.
 *
 * Return: Void
 */
static void
dp_sawf_peer_msduq_timer_update(struct dp_soc *dpsoc, struct dp_peer *peer,
				struct dp_sawf_msduq *msduq, uint8_t q_id)
{
	uint8_t current_q_state;
	uint16_t peer_id;

	peer_id = peer->peer_id;
	current_q_state = msduq->q_state;

	if (current_q_state == SAWF_MSDUQ_IN_USE) {
		dp_sawf_trace("peer:%d | msduq:%d, q_state:%s, MSDUQ Ref_count:%d",
			      peer_id, q_id,
			      dp_sawf_msduq_state_to_string(current_q_state),
			      qdf_atomic_read(&msduq->ref_count));
		if (!qdf_atomic_read(&msduq->ref_count)) {
			dp_sawf_debug("peer:%d, msduq:%d", peer_id, q_id);
			if (!msduq->is_deactivation_needed) {
				msduq->is_deactivation_needed = 1;
				dp_sawf_debug("peer:%d | Set Deactivation Flag",
					      peer_id);
			} else {
				msduq->is_deactivation_needed = 0;
				dp_sawf_debug("peer:%d | Reset Deactivation Flag",
					      peer_id);
				dp_sawf_peer_msduq_reconfigure(
						dpsoc, peer, msduq, q_id,
						HTT_MSDUQ_DEACTIVATE);
			}
		}
	}
}

/*
 * dp_sawf_peer_msduq_timeout - Takes sawf_peer_lock and loops through MSDUQ to
 * call MSDUQ state handling functions.
 *
 * @dp_soc: SOC handle
 * @peer: Peer handle
 * @arg: NULL
 *
 * Loops through the MSDUQ for primary peer and calls MSDUQ state update handler
 * and HTT Response timeout handler functions to handle MSDUQ queue states.
 *
 * Intimates NW manager if there is a reactivate failure in any of the queues.
 *
 * Return: Void
 */
static void
dp_sawf_peer_msduq_timeout(struct dp_soc *dpsoc, struct dp_peer *peer,
			   void *arg)
{
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t no_resp_q[DP_SAWF_Q_MAX] = {0};
	bool is_notify_needed = false;
	uint8_t q_id;

	/*
	 * In MLO cases timer handler needed to be processed only for the
	 * primary link.
	 *
	 */
	if (peer->bss_peer || !dp_peer_is_primary_link_peer(peer)) {
		return;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_trace("sawf_ctx doesn't exist, peer_id:%d",
			      peer->peer_id);
		return;
	}

	dp_sawf_trace("MSDUQ timer expiry, peer:%d", peer->peer_id);

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);
	for (q_id = 0; q_id < DP_SAWF_Q_MAX; q_id++) {
		msduq = &sawf_ctx->msduq[q_id];
		dp_sawf_peer_msduq_timer_update(dpsoc, peer, msduq, q_id);
		dp_sawf_peer_msduq_htt_resp_timeout(dpsoc, peer, msduq, q_id,
						    no_resp_q,
						    &is_notify_needed);
	}
	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);

	if (is_notify_needed)
		dp_sawf_notify_nw_manager(dpsoc, peer, no_resp_q);
}

/*
 * dp_sawf_msduq_timer_handler()- Timer handler to loop all peers and
 * to loop over each MSDUQ to check and update state and HTT response timeout
 * operation every DP_SAWF_MSDUQ_TIMER_MS.
 * @arg: SoC Handle
 *
 * Return: Void
 */

void
dp_sawf_msduq_timer_handler(void *arg)
{
	struct dp_soc *dpsoc = (struct dp_soc *)arg;
	int sawf_reclaim_timer =
		wlan_cfg_get_sawf_msduq_reclaim_timer_val(dpsoc->wlan_cfg_ctx);

	dp_soc_iterate_peer(dpsoc, dp_sawf_peer_msduq_timeout, NULL,
			    DP_MOD_ID_SAWF);

	qdf_timer_mod(&dpsoc->sawf_msduq_timer, sawf_reclaim_timer);
}

void dp_soc_sawf_msduq_timer_init(struct dp_soc *soc)
{
	/*
	 * Timer is not initialized/started when sawf_msduq_reclaim ini is not
	 * enabled.
	 */
	if (!wlan_cfg_get_sawf_msduq_reclaim_config(soc->wlan_cfg_ctx))
		return;

	qdf_timer_init(soc->osdev, &soc->sawf_msduq_timer,
		       dp_sawf_msduq_timer_handler, (void *)soc,
		       QDF_TIMER_TYPE_WAKE_APPS);
}

void dp_soc_sawf_msduq_timer_deinit(struct dp_soc *soc)
{
	if (!wlan_cfg_get_sawf_msduq_reclaim_config(soc->wlan_cfg_ctx))
		return;

	if (soc->sawf_msduq_timer_enabled) {
		soc->sawf_msduq_timer_enabled = false;
		dp_sawf_info("SAWF MDSUQ timer is stopped");
		qdf_timer_free(&soc->sawf_msduq_timer);
	}
}

/*
 * dp_sawf_get_available_msduq() - To find the available MSDUQ based on MSDUQ
 * states. Queue logic varies based on whether MSDUQ reclaim feature is enabled
 * or disabled.
 * @soc: dp_soc handle
 * @sawf_ctx: sawf context of the peer
 * @peer: dp_peer context of the peer
 * @q1_id: Queue ID #1 of a particular TID
 * @q2_id: Queue ID #2 of a particular TID
 *
 * If MSDUQ reclaim feature is enabled:
 * 1. If q1 is in Unused state, return q1.
 * 2. If q1 is in Deactivated state, check q2 state.
 *	a. If q2 is in Unused state, return q2.
 *	b. Else if q2 in Deactivate pending or Reactivate pending/In Use
 *	   state, return q1.
 * 3. If q1 is in Deactivate pending or Reactivate pending/In Use state,
 *    Check q2 state.
 *	a. If q2 is in Unused or Deactivated state, return q2.
 *	b. Else if q2 in Deactivate pending or Reactivate pending/In Use
 *	   state, return DP_SAWF_Q_INVALID to loop over available lower
 *	   or higher TID queues.
 *
 * Else if MSDUQ reclaim feature is disabled:
 * 1. If q1 is in Unused state, return q1.
 * 2. Else if q2 is in Unused state, return q2.
 * 3. Else return DP_SAWF_Q_INVALID to loop over available lower or
 *    higher TID queues.
 *
 * Return: q_id of the available Queue. Else, DP_SAWF_Q_INVALID is returned.
 *
 */
static uint8_t
dp_sawf_get_available_msduq(struct dp_soc *dpsoc, struct dp_peer_sawf *sawf_ctx,
			    struct dp_peer *peer, uint8_t q1_id, uint8_t q2_id)
{
	uint8_t q_id;
	uint8_t q1_state, q2_state;

	q1_state = dp_sawf(peer, q1_id, q_state);
	q2_state = dp_sawf(peer, q2_id, q_state);
	dp_sawf_debug("msduq1:%d - q_state:%s, msduq2:%d - q_state:%s",
		      q1_id, dp_sawf_msduq_state_to_string(q1_state),
		      q2_id, dp_sawf_msduq_state_to_string(q2_state));

	if (wlan_cfg_get_sawf_msduq_reclaim_config(dpsoc->wlan_cfg_ctx)) {
		if (q1_state == SAWF_MSDUQ_UNUSED) {
			q_id = q1_id;
		} else if (q1_state == SAWF_MSDUQ_DEACTIVATED) {
			if (q2_state == SAWF_MSDUQ_UNUSED)
				q_id = q2_id;
			else
				q_id = q1_id;
		} else {
			if (q2_state == SAWF_MSDUQ_UNUSED ||
			    q2_state ==	SAWF_MSDUQ_DEACTIVATED)
				q_id = q2_id;
			else
				q_id = DP_SAWF_Q_INVALID;
		}
	} else {
		if (q1_state == SAWF_MSDUQ_UNUSED)
			q_id = q1_id;
		else if (q2_state == SAWF_MSDUQ_UNUSED)
			q_id = q2_id;
		else
			q_id = DP_SAWF_Q_INVALID;
	}

	dp_sawf_debug("q_id returned: %d", q_id);
	return q_id;
}

/*
 * dp_sawf_peer_msduq_update() - MSDUQ attributes such as SVC ID and
 * q_state is updated in the msduq structure.
 * Logic varies based on whether MSDUQ reclaim feature is enabled or
 * disabled.
 *
 * @peer: dp_peer context of the peer
 * @soc: dp_soc context
 * @q_id: Queue Id for MSDUQ
 * @service_id: Service class ID
 * @msduq: MSDUQ structure
 *
 * If MSDUQ reclaim feature is enabled:
 * 1. If the Queue is in SAWF_MSDUQ_UNUSED state,
 * it is set to SAWF_MSDUQ_IN_USE.
 * 2. Else, if the Queue is in DEACTIVATED state, updates msduq state to
 * REACTIVATE_PENDING and sends HTT message to FW for the queue to be
 * Reactivated.
 *
 * Else if MSDUQ reclaim feature is disabled, Queue state is changed to
 * SAWF_MSDUQ_IN_USE.
 *
 * Return: Void
 */
static void
dp_sawf_peer_msduq_update(struct dp_peer *peer, struct dp_soc *dpsoc,
			  uint8_t q_id, uint32_t service_id,
			  struct dp_sawf_msduq *msduq)
{
	uint8_t current_q_state, new_q_state;

	current_q_state = msduq->q_state;
	msduq->svc_id = service_id;

	if (wlan_cfg_get_sawf_msduq_reclaim_config(dpsoc->wlan_cfg_ctx)) {
		/* Update msduq is called when the Queue is in either
		 * SAWF_MSDUQ_UNUSED or in SAWF_MSDUQ_DEACTIVATED state.
		 */

		if (current_q_state == SAWF_MSDUQ_UNUSED) {
			new_q_state = SAWF_MSDUQ_IN_USE;
			dp_sawf_peer_msduq_q_state_update(msduq, q_id,
							  new_q_state);
		} else if (current_q_state == SAWF_MSDUQ_DEACTIVATED) {
			dp_sawf_peer_msduq_reconfigure(dpsoc, peer, msduq, q_id,
						       HTT_MSDUQ_REACTIVATE);
		}
	} else {
		new_q_state = SAWF_MSDUQ_IN_USE;
		dp_sawf_peer_msduq_q_state_update(msduq, q_id, new_q_state);
	}
}

uint16_t dp_sawf_get_msduq(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   uint8_t *dest_mac, uint32_t service_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_peer *peer, *mld_peer, *primary_link_peer;
	uint16_t peer_id;
	uint8_t q_id;
	void *tmetry_ctx;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t static_tid, tid;
	bool is_tid_skid_enabled;
	bool is_lower_tid_checked = false;

	if (!wlan_cfg_get_sawf_config(soc->wlan_cfg_ctx)) {
		dp_sawf_debug("SAWF is disabled");
		return DP_SAWF_PEER_Q_INVALID;
	}

	static_tid = wlan_service_id_tid(service_id);
	tid = static_tid;

	peer = dp_find_peer_by_destmac(soc, dest_mac, vdev_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_debug("Peer is NULL");
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;
	else
		mld_peer = NULL;

	if (mld_peer) {
		primary_link_peer = dp_get_primary_link_peer_by_id
			(soc, mld_peer->peer_id, DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			dp_sawf_err("Invalid primary peer");
			return DP_SAWF_PEER_Q_INVALID;
		}
		/*
		 * Release the MLD-peer reference.
		 * Hold only primary link ref now.
		 */
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		peer = primary_link_peer;
	}

	peer_id = peer->peer_id;

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_warn("Peer SAWF ctx invalid");
		return DP_SAWF_PEER_Q_INVALID;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_warn("Invalid txrx peer");
		return DP_SAWF_PEER_Q_INVALID;
	}

	if (wlan_cfg_get_sawf_msduq_reclaim_config(soc->wlan_cfg_ctx)) {
		int sawf_reclaim_timer =
			wlan_cfg_get_sawf_msduq_reclaim_timer_val(
							soc->wlan_cfg_ctx);

		if (!soc->sawf_msduq_timer_enabled) {
			soc->sawf_msduq_timer_enabled = true;
			dp_sawf_info("SAWF MDSUQ timer is started");
			qdf_timer_start(&soc->sawf_msduq_timer,
					sawf_reclaim_timer);
		}
	}

	is_tid_skid_enabled =
		wlan_cfg_get_sawf_msduq_tid_skid_config(soc->wlan_cfg_ctx);

	dp_sawf_trace("RX callback from NW Connection manager, peer_id:%d, "
		      "svc_id:%u", peer_id, service_id);

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);

	/* Check if the Service class is already mapped to a Queue and it is in
	 * SAWF_MSDUQ_IN_USE or SAWF_MSDUQ_REACTIVATE_PENDING state.
	 */
	for (q_id = 0; q_id < DP_SAWF_Q_MAX; q_id++) {
		msduq = &sawf_ctx->msduq[q_id];
		if ((msduq->svc_id == service_id) &&
		    ((msduq->q_state == SAWF_MSDUQ_IN_USE) ||
		     (msduq->q_state == SAWF_MSDUQ_REACTIVATE_PENDING))) {
			q_id = q_id + DP_SAWF_DEFAULT_Q_MAX;
			dp_sawf_debug("msduq:%d is returned to NW manager -> "
				      "peer_id:%d, svc_id:%u, "
				      "q_state:%s",
				      (q_id - DP_SAWF_DEFAULT_Q_MAX), peer_id,
				      service_id,
				      dp_sawf_msduq_state_to_string(
							msduq->q_state));
			qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			return dp_sawf_msduq_peer_id_set(peer_id, q_id);
		}
	}

	/*
	 * Check available MSDUQ for associated TID of Service class,
	 * skid to lower TID values if MSDUQ is unavailable.
	 * If MSDU queues of lower TID values are unavailable, skid to higher
	 * TID values and check.
	 */
	while ((tid >= DP_SAWF_TID_MIN) && (tid < DP_SAWF_TID_MAX)) {
		q_id = dp_sawf_get_available_msduq(soc, sawf_ctx, peer, tid,
						   tid + DP_SAWF_TID_MAX);

		if (q_id == DP_SAWF_Q_INVALID && !is_tid_skid_enabled) {
			dp_sawf_trace("TID Skid logic is disabled. TID:%d, "
				      "peer_id:%d", tid, peer_id);
			goto fail;
		}

		if (q_id != DP_SAWF_Q_INVALID) {
			msduq = &sawf_ctx->msduq[q_id];
			dp_sawf_peer_msduq_update(peer, soc, q_id, service_id,
						  msduq);
			peer->sawf->sla_mask |=
			     wlan_service_id_get_enabled_param_mask(service_id);
			if (txrx_peer->sawf_stats &&
			    !peer->sawf->telemetry_ctx) {
				tmetry_ctx = telemetry_sawf_peer_ctx_alloc(
						soc, txrx_peer->sawf_stats,
						peer->mac_addr.raw,
						service_id, q_id);
				if (tmetry_ctx)
					peer->sawf->telemetry_ctx = tmetry_ctx;
			}
			q_id = q_id + DP_SAWF_DEFAULT_Q_MAX;
			dp_sawf_debug("msduq:%d is returned to NW manager -> "
				      "peer_id:%d, svc_id:%u "
				      "q_state:%s",
				      (q_id - DP_SAWF_DEFAULT_Q_MAX), peer_id,
				      service_id,
				      dp_sawf_msduq_state_to_string(
							msduq->q_state));
			qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			return dp_sawf_msduq_peer_id_set(peer_id, q_id);
		}

		if (!is_lower_tid_checked) {
			if (tid == 0) {
				dp_sawf_debug("peer:%d | Lower TID Queues below TID %d "
					      "are unavailable, Moving to "
					      "Higher TID Queues.", peer_id,
					      static_tid);
				is_lower_tid_checked = true;
				tid = static_tid + 1;
			} else {
				tid--;
			}
		} else {
			tid++;
		}
	}

fail:
	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
	dp_sawf_info("MSDU Queues are not available for the peer -> peer_id:%d "
		     "is_tid_skid_enabled:%d", peer_id,
		     is_tid_skid_enabled);
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	/* request for more msdu queues. Return error*/
	return DP_SAWF_PEER_Q_INVALID;
}

qdf_export_symbol(dp_sawf_get_msduq);

QDF_STATUS
dp_sawf_get_peer_msduq_svc_params(struct cdp_soc_t *soc, uint8_t *mac,
				  void *data)
{
	uint16_t peer_id, msduq_peer_id;
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq = NULL;
	struct sawf_msduq_svc_params *dst;
	uint8_t index = 0, type = 0, tid = 0, queue_id = 0;
	uint32_t service_interval = 0, burst_size = 0,
		min_throughput = 0, max_latency = 0, priority = 0;

	dst = (struct sawf_msduq_svc_params *)data;
	if (!dst) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac, 0,
				      DP_VDEV_ALL, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_SAWF);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	peer_id = primary_link_peer->peer_id;

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("sawf_ctx doesn't exist");
		goto fail;
	}

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);

	for (index = 0; index < DP_SAWF_Q_MAX; index++) {
		msduq = &sawf_ctx->msduq[index];

		/* Check if msduq is used and flow count is non-zero */
		if (((msduq->q_state == SAWF_MSDUQ_IN_USE) ||
		     (msduq->q_state == SAWF_MSDUQ_REACTIVATE_PENDING)) &&
		    qdf_atomic_read(&msduq->ref_count)) {
			dst->is_used = true;
			dst->svc_id = msduq->svc_id;

			wlan_sawf_get_downlink_params(msduq->svc_id, &tid,
						      &service_interval,
						      &burst_size, &min_throughput,
						      &max_latency, &priority,
						      &type);

			dst->svc_type = type;
			dst->svc_tid = tid;
			dst->svc_ac = TID_TO_WME_AC(tid);
			dst->priority = priority;
			dst->service_interval = service_interval;
			dst->burst_size = burst_size;
			dst->min_throughput = min_throughput;
			dst->delay_bound = max_latency;

			queue_id = index;
			queue_id = queue_id + DP_SAWF_DEFAULT_Q_MAX;
			msduq_peer_id = dp_sawf_msduq_peer_id_set(peer_id, queue_id);

			DP_SAWF_METADATA_SET(dst->mark_metadata, dst->svc_id,
					     msduq_peer_id);
		}
		dst++;
	}

	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_SAWF);
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_SAWF);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_swaf_peer_sla_configuration(struct cdp_soc_t *soc_hdl, uint8_t *mac_addr,
			       uint16_t *arg_sla_mask)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_sawf_msduq *msduq;
	uint8_t q_idx;
	uint16_t sla_mask = 0;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac_addr, 0,
				      DP_VDEV_ALL, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_debug("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_debug("txrx peer is NULL");
		goto fail;
	}

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_SAWF);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);

	for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; q_idx++) {
		msduq = &sawf_ctx->msduq[q_idx];
		if (((msduq->q_state == SAWF_MSDUQ_IN_USE) ||
		     (msduq->q_state == SAWF_MSDUQ_REACTIVATE_PENDING)) &&
		    qdf_atomic_read(&msduq->ref_count)) {
			sla_mask |= wlan_service_id_get_enabled_param_mask
				(msduq->svc_id);
		}
	}

	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);

	if (arg_sla_mask)
		*arg_sla_mask = sla_mask;

	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_SAWF);
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_SAWF);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	if (arg_sla_mask)
		*arg_sla_mask = 0;
	return QDF_STATUS_E_INVAL;
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef CONFIG_SAWF_STATS
static QDF_STATUS
dp_sawf_inc_reinject_pkt(struct dp_peer *peer, uint8_t msduq_idx)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf_stats *stats_ctx;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_debug("NULL txrx_peer");
		return QDF_STATUS_E_FAILURE;
	}

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_debug("Invalid SAWF stats ctx");
		return QDF_STATUS_E_FAILURE;
	}

	DP_STATS_INC(stats_ctx, tx_stats[msduq_idx].reinject_pkt, 1);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_sawf_inc_reinject_pkt(struct dp_peer *peer, uint8_t msduq_idx)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
dp_sawf_reinject_handler(struct dp_soc *soc, qdf_nbuf_t nbuf,
			 uint32_t *htt_desc)
{
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf;
	uint16_t peer_id;
	uint8_t svc_id;
	uint8_t msduq;
	uint32_t metadata = 0;
	struct dp_tx_msdu_info_s msdu_info;
	uint32_t reinject_status;
	uint8_t htt_q_idx;
	uint8_t host_tid_queue;
	uint8_t msduq_idx;
	uint16_t peer_msduq;
	uint8_t tid;
	uint16_t data_length;
	struct dp_vdev *vdev;
	QDF_STATUS status;

	reinject_status = htt_desc[2];

	peer_id = HTT_TX_WBM_REINJECT_SW_PEER_ID_GET(reinject_status);
	data_length = HTT_TX_WBM_REINJECT_DATA_LEN_GET(reinject_status);

	reinject_status = htt_desc[3];

	tid = HTT_TX_WBM_REINJECT_TID_GET(reinject_status);

	htt_q_idx = HTT_TX_WBM_REINJECT_MSDUQ_ID_GET(reinject_status);

	dp_sawf_debug("peer_id %u data_length %u tid %u htt_q_idx %u",
		      peer_id, data_length, tid, htt_q_idx);

	host_tid_queue = htt_q_idx - DP_SAWF_DEFAULT_Q_PTID_MAX;

	msduq_idx = tid + host_tid_queue * DP_SAWF_TID_MAX;
	if (msduq_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid msduq idx, tid %u htt_q_idx %u",
			    tid, htt_q_idx);
		return QDF_STATUS_E_FAILURE;
	}

	msduq = msduq_idx + DP_SAWF_DEFAULT_Q_MAX;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	sawf = dp_peer_sawf_ctx_get(peer);
	if (!sawf) {
		dp_sawf_err("Invalid SAWF ctx for peer id %u", peer_id);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = peer->vdev;

	svc_id = sawf->msduq[msduq_idx].svc_id;

	status = dp_sawf_inc_reinject_pkt(peer, msduq_idx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_sawf_debug("Unable to inc reinject stats for peer id %u",
			    peer_id);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return status;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	peer_msduq = dp_sawf_msduq_peer_id_set(peer_id, msduq);
	DP_SAWF_METADATA_SET(metadata, svc_id, peer_msduq);

	qdf_nbuf_set_mark(nbuf, metadata);

	qdf_nbuf_set_len(nbuf, data_length);

	qdf_nbuf_set_fast_xmit(nbuf, 0);
	nbuf->fast_recycled = 0;
	nbuf->recycled_for_ds = 0;

	qdf_mem_zero(&msdu_info, sizeof(msdu_info));

	dp_tx_get_queue(vdev, nbuf,
			&msdu_info.tx_queue);

	msdu_info.tid = HTT_TX_EXT_TID_INVALID;

	nbuf = dp_tx_send_msdu_single(vdev, nbuf,
				      &msdu_info,
				      peer_id,
				      NULL);
	if (nbuf)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#else /* QCA_HOST_MODE_WIFI_DISABLED */
QDF_STATUS
dp_sawf_reinject_handler(struct dp_soc *soc, qdf_nbuf_t nbuf,
			 uint32_t *htt_desc)
{
	return QDF_STATUS_E_FAILURE;
}
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef SAWF_ADMISSION_CONTROL
void dp_sawf_peer_msduq_event_notify(struct dp_soc *soc, struct dp_peer *peer,
				     uint8_t queue_id, uint8_t svc_id,
				     enum cdp_sawf_peer_msduq_event event_type)
{
	uint16_t peer_id, msduq_peer_id;
	uint32_t mark_metadata;
	uint8_t tid = 0, type = 0;
	uint32_t service_interval = 0, burst_size = 0, min_throughput = 0,
		delay_bound = 0, priority = 0;
	struct cdp_sawf_peer_msduq_event_intf params = {0};

	params.vdev_id = peer->vdev->vdev_id;
	params.event_type = event_type;
	params.queue_id = queue_id;

	peer_id = peer->peer_id;
	qdf_mem_copy(params.peer_mac, peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);

	/* No need of service class params in case of delete */
	if (event_type == SAWF_PEER_MSDUQ_DELETE_EVENT)
		goto dispatch_event;

	if (wlan_sawf_get_downlink_params(svc_id, &tid, &service_interval,
					  &burst_size, &min_throughput,
					  &delay_bound, &priority, &type) != QDF_STATUS_SUCCESS)
		return;

	queue_id = queue_id + DP_SAWF_DEFAULT_Q_MAX;
	msduq_peer_id = dp_sawf_msduq_peer_id_set(peer_id, queue_id);
	DP_SAWF_METADATA_SET(mark_metadata, svc_id, msduq_peer_id);

	params.svc_id = svc_id;
	params.type = type;
	params.priority = priority;
	params.tid = tid;
	params.ac = TID_TO_WME_AC(tid);
	params.mark_metadata = mark_metadata;
	params.service_interval = service_interval;
	params.burst_size = burst_size;
	params.min_throughput = min_throughput;
	params.delay_bound = delay_bound;

dispatch_event:
	dp_wdi_event_handler(WDI_EVENT_PEER_MSDUQ_EVENT, soc, (void *)&params,
			     HTT_INVALID_PEER, WDI_NO_VAL,
			     peer->vdev->pdev->pdev_id);
}
#else
void dp_sawf_peer_msduq_event_notify(struct dp_soc *soc, struct dp_peer *peer,
				     uint8_t queue_id, uint8_t svc_id,
				     enum cdp_sawf_peer_msduq_event event_type)
{
}
#endif

#ifdef CONFIG_SAWF_STATS
struct sawf_telemetry_params sawf_telemetry_cfg;

QDF_STATUS
dp_peer_sawf_stats_ctx_alloc(struct dp_soc *soc,
			     struct dp_txrx_peer *txrx_peer)
{
	struct dp_peer_sawf_stats *ctx;
	struct sawf_stats *stats;
	uint8_t q_idx;
	uint8_t stats_cfg;

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!stats_cfg)
		return QDF_STATUS_SUCCESS;

	ctx = qdf_mem_malloc(sizeof(struct dp_peer_sawf_stats));
	if (!ctx) {
		dp_sawf_err("Failed to allocate peer SAWF stats");
		return QDF_STATUS_E_FAILURE;
	}

	txrx_peer->sawf_stats = ctx;
	stats = &ctx->stats;

	/* Initialize delay stats hist */
	for (q_idx = 0; q_idx < DP_SAWF_Q_MAX; ++q_idx) {
		dp_hist_init(&stats->delay[q_idx].delay_hist,
			     CDP_HIST_TYPE_HW_TX_COMP_DELAY);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_peer_sawf_stats_ctx_free(struct dp_soc *soc,
			    struct dp_txrx_peer *txrx_peer)
{
	uint8_t stats_cfg;

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!stats_cfg)
		return QDF_STATUS_SUCCESS;

	if (!txrx_peer->sawf_stats) {
		dp_sawf_err("Failed to free peer SAWF stats");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_free(txrx_peer->sawf_stats);
	txrx_peer->sawf_stats = NULL;

	return QDF_STATUS_SUCCESS;
}

struct dp_peer_sawf_stats *
dp_peer_sawf_stats_ctx_get(struct dp_txrx_peer *txrx_peer)
{
	struct dp_peer_sawf_stats *sawf_stats;

	sawf_stats = txrx_peer->sawf_stats;
	if (!sawf_stats) {
		dp_sawf_err("Failed to get SAWF stats ctx");
		return NULL;
	}

	return sawf_stats;
}

static QDF_STATUS
dp_sawf_compute_tx_delay_us(struct dp_tx_desc_s *tx_desc,
			    uint32_t *delay)
{
	int64_t wifi_entry_ts, timestamp_hw_enqueue;

	timestamp_hw_enqueue = qdf_ktime_to_us(tx_desc->timestamp);
	wifi_entry_ts = qdf_nbuf_get_timestamp_us(tx_desc->nbuf);

	if (timestamp_hw_enqueue == 0)
		return QDF_STATUS_E_FAILURE;

	*delay = (uint32_t)(timestamp_hw_enqueue - wifi_entry_ts);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_sawf_compute_tx_hw_delay_us(struct dp_soc *soc,
			       struct dp_vdev *vdev,
			       struct hal_tx_completion_status *ts,
			       uint32_t *delay_us)
{
	QDF_STATUS status;
	uint32_t delay;

	/* Tx_rate_stats_info_valid is 0 and tsf is invalid then */
	if (!ts->valid) {
		dp_sawf_info("Invalid Tx rate stats info");
		return QDF_STATUS_E_FAILURE;
	}

	if (!vdev) {
		dp_sawf_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	if (soc->arch_ops.dp_tx_compute_hw_delay)
		status = soc->arch_ops.dp_tx_compute_hw_delay(soc, vdev, ts,
							      &delay);
	else
		status = QDF_STATUS_E_NOSUPPORT;

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (delay_us)
		*delay_us = delay;

	return QDF_STATUS_SUCCESS;
}

static inline uint32_t dp_sawf_get_mov_avg_num_pkt(void)
{
	return sawf_telemetry_cfg.mov_avg.packet;
}

static inline uint32_t dp_sawf_get_sla_num_pkt(void)
{
	return sawf_telemetry_cfg.sla.num_packets;
}

static QDF_STATUS
dp_sawf_update_tx_delay(struct dp_soc *soc,
			struct dp_vdev *vdev,
			struct hal_tx_completion_status *ts,
			struct dp_tx_desc_s *tx_desc,
			struct dp_peer_sawf *sawf_ctx,
			struct sawf_stats *stats,
			uint8_t tid,
			uint8_t host_q_idx)
{
	struct sawf_delay_stats *tx_delay;
	struct wlan_sawf_svc_class_params *svclass_params;
	uint8_t svc_id;
	uint32_t num_pkt_win, hw_delay, sw_delay, nw_delay;
	uint64_t nwdelay_win_avg, swdelay_win_avg, hwdelay_win_avg;

	if (QDF_IS_STATUS_ERROR(dp_sawf_compute_tx_hw_delay_us(soc, vdev, ts,
							       &hw_delay))) {
		return QDF_STATUS_E_FAILURE;
	}

	/* Update hist */
	tx_delay = &stats->delay[host_q_idx];
	dp_hist_update_stats(&tx_delay->delay_hist, hw_delay);

	tx_delay->num_pkt++;

	tx_delay->hwdelay_win_total += hw_delay;
	nw_delay = SAWF_NW_DELAY_GET(qdf_nbuf_get_mark(tx_desc->nbuf));

	tx_delay->nwdelay_win_total += nw_delay;

	dp_sawf_compute_tx_delay_us(tx_desc, &sw_delay);
	tx_delay->swdelay_win_total += sw_delay;

	num_pkt_win = dp_sawf_get_mov_avg_num_pkt();
	if (!(tx_delay->num_pkt % num_pkt_win)) {
		nwdelay_win_avg = qdf_do_div(tx_delay->nwdelay_win_total,
					     num_pkt_win);
		swdelay_win_avg = qdf_do_div(tx_delay->swdelay_win_total,
					     num_pkt_win);
		hwdelay_win_avg = qdf_do_div(tx_delay->hwdelay_win_total,
					     num_pkt_win);
		/* Update the avg per window */
		telemetry_sawf_update_delay_mvng(sawf_ctx->telemetry_ctx,
						 tid, host_q_idx,
						 nwdelay_win_avg,
						 swdelay_win_avg,
						 hwdelay_win_avg);

		tx_delay->nwdelay_win_total = 0;
		tx_delay->swdelay_win_total = 0;
		tx_delay->hwdelay_win_total = 0;
	}

	svc_id = sawf_ctx->msduq[host_q_idx].svc_id;
	if (!wlan_delay_bound_configured(svc_id))
		goto cont;

	svclass_params = wlan_get_svc_class_params(svc_id);
	if (svclass_params) {
		(hw_delay > svclass_params->delay_bound *
		 DP_SAWF_DELAY_BOUND_MS_MULTIPLER) ?
			tx_delay->failure++ : tx_delay->success++;

		if (!(tx_delay->num_pkt % dp_sawf_get_sla_num_pkt())) {
			/* Update the success/failure count */
			telemetry_sawf_update_delay(sawf_ctx->telemetry_ctx,
						    tid, host_q_idx,
						    tx_delay->success,
						    tx_delay->failure);
		}
	}

cont:
	return QDF_STATUS_SUCCESS;
}

/*
 * dp_sawf_update_peer_mcs_stats() - Update mcs cout on msduq level
 *
 * @stats_ctx: dp sawf peer stats context
 * @ts: hal tx completion status
 * @tx_stats: sawf tx stats
 * @host_q_idx: host msduq index
 *
 * check mcs and pkt_type range and Update mcs cout on msduq level
 *
 * Return: Void
 */
void dp_sawf_update_peer_mcs_stats(struct dp_peer_sawf_stats *stats_ctx,
				   struct hal_tx_completion_status *ts,
				   struct sawf_tx_stats *tx_stats,
				   uint8_t host_q_idx)
{
	uint8_t mcs, pkt_type, dst_mcs_idx;

	mcs = ts->mcs;
	pkt_type = ts->pkt_type;
	pkt_type = (pkt_type >= HAL_DOT11_MAX ? DOT11_MAX :
		    hal_2_dp_pkt_type_map[pkt_type]);

	dst_mcs_idx = dp_get_mcs_array_index_by_pkt_type_mcs(pkt_type, mcs);

	if (dst_mcs_idx != MCS_INVALID_ARRAY_INDEX) {
		DP_STATS_INC(stats_ctx,
			     tx_stats[host_q_idx].pkt_type[pkt_type].mcs_count[dst_mcs_idx],
			     1);
	}
}

QDF_STATUS
dp_sawf_tx_compl_update_peer_stats(struct dp_soc *soc,
				   struct dp_vdev *vdev,
				   struct dp_txrx_peer *txrx_peer,
				   struct dp_tx_desc_s *tx_desc,
				   struct hal_tx_completion_status *ts,
				   uint8_t host_tid)
{
	struct dp_peer *peer;
	struct dp_peer *primary_link_peer = NULL;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;
	uint8_t host_msduq_idx, host_q_idx, stats_cfg;
	uint16_t peer_id;
	qdf_size_t length;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!stats_cfg)
		return QDF_STATUS_SUCCESS;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	if (!ts || !ts->valid)
		return QDF_STATUS_E_INVAL;

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);

	if (!stats_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		return QDF_STATUS_E_FAILURE;
	}

	peer_id = tx_desc->peer_id;
	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_debug("No peer for id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (IS_MLO_DP_MLD_PEER(peer)) {
		primary_link_peer = dp_get_primary_link_peer_by_id(
						soc, peer->peer_id,
						DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			dp_sawf_debug("NULL peer");
			return QDF_STATUS_E_FAILURE;
		}
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		peer = primary_link_peer;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("No SAWF ctx for peer_id %u", peer_id);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return QDF_STATUS_E_FAILURE;
	}

	if (!SAWF_BASIC_STATS_ENABLED(stats_cfg))
		goto latency_stats_update;

	length = qdf_nbuf_len(tx_desc->nbuf);

	switch (ts->status) {
	case HAL_TX_TQM_RR_FRAME_ACKED:
		DP_STATS_INC_PKT(stats_ctx, tx_stats[host_q_idx].tx_success, 1,
			  length);
		dp_sawf_update_peer_mcs_stats(stats_ctx, ts, tx_stats, host_q_idx);
		break;
	case HAL_TX_TQM_RR_REM_CMD_REM:
		DP_STATS_INC_PKT(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem, 1,
			  length);
		break;
	case HAL_TX_TQM_RR_REM_CMD_NOTX:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_notx, 1);
		break;
	case HAL_TX_TQM_RR_REM_CMD_TX:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_tx, 1);
		break;
	case HAL_TX_TQM_RR_REM_CMD_AGED:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.age_out, 1);
		break;
	case HAL_TX_TQM_RR_FW_REASON1:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason1, 1);
		break;
	case HAL_TX_TQM_RR_FW_REASON2:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason2, 1);
		break;
	case HAL_TX_TQM_RR_FW_REASON3:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_reason3, 1);
		break;
	case HAL_TX_TQM_RR_REM_CMD_DISABLE_QUEUE:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_queue_disable, 1);
		break;
	case HAL_TX_TQM_RR_REM_CMD_TILL_NONMATCHING:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.fw_rem_no_match, 1);
		break;
	case HAL_TX_TQM_RR_DROP_THRESHOLD:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.drop_threshold, 1);
		break;
	case HAL_TX_TQM_RR_LINK_DESC_UNAVAILABLE:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.drop_link_desc_na, 1);
		break;
	case HAL_TX_TQM_RR_DROP_OR_INVALID_MSDU:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.invalid_drop, 1);
		break;
	case HAL_TX_TQM_RR_MULTICAST_DROP:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.mcast_vdev_drop, 1);
		break;
	default:
		DP_STATS_INC(stats_ctx, tx_stats[host_q_idx].dropped.invalid_rr, 1);
		break;
	}

	tx_stats = &stats_ctx->stats.tx_stats[host_q_idx];

	tx_stats->tx_failed = tx_stats->dropped.fw_rem.num +
				tx_stats->dropped.fw_rem_notx +
				tx_stats->dropped.fw_rem_tx +
				tx_stats->dropped.age_out +
				tx_stats->dropped.fw_reason1 +
				tx_stats->dropped.fw_reason2 +
				tx_stats->dropped.fw_reason3 +
				tx_stats->dropped.fw_rem_queue_disable +
				tx_stats->dropped.fw_rem_no_match +
				tx_stats->dropped.drop_threshold +
				tx_stats->dropped.drop_link_desc_na +
				tx_stats->dropped.invalid_drop +
				tx_stats->dropped.mcast_vdev_drop +
				tx_stats->dropped.invalid_rr;

	DP_STATS_DEC(stats_ctx, tx_stats[host_q_idx].queue_depth, 1);

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].total_retries_count,
			  (ts->transmit_cnt - 1),
			  (ts->status == HAL_TX_TQM_RR_FRAME_ACKED
			  && ts->transmit_cnt > 1));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].retry_count, 1,
			  (ts->status == HAL_TX_TQM_RR_FRAME_ACKED
			  && ts->transmit_cnt > 1));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].multiple_retry_count, 1,
			  (ts->status == HAL_TX_TQM_RR_FRAME_ACKED
			  && ts->transmit_cnt > 2));

	DP_STATS_INCC(stats_ctx, tx_stats[host_q_idx].failed_retry_count, 1,
			  (ts->status != HAL_TX_TQM_RR_FRAME_ACKED
			  && ts->transmit_cnt > DP_RETRY_COUNT));

	if (!(tx_stats->tx_success.num + tx_stats->tx_failed) %
	      dp_sawf_get_sla_num_pkt()) {
		telemetry_sawf_update_msdu_drop(sawf_ctx->telemetry_ctx,
						host_tid, host_q_idx,
						tx_stats->tx_success.num,
						tx_stats->tx_failed,
						tx_stats->dropped.age_out);
	}

latency_stats_update:
	if (SAWF_LATENCY_STATS_ENABLED(stats_cfg)) {
		status = dp_sawf_update_tx_delay(soc, vdev, ts, tx_desc,
						 sawf_ctx, &stats_ctx->stats,
						 host_tid, host_q_idx);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return status;
}

void dp_peer_tid_delay_avg(struct cdp_delay_tx_stats *tx_delay,
			   uint32_t nw_delay,
			   uint32_t sw_delay,
			   uint32_t hw_delay)
{
	uint64_t sw_avg_sum = 0;
	uint64_t hw_avg_sum = 0;
	uint64_t nw_avg_sum = 0;
	uint32_t cur_win, idx;
	uint32_t max_pkt_per_window_size;
	uint32_t max_window_size;

	max_pkt_per_window_size = tx_delay->max_pkt_per_window_size;
	max_window_size = tx_delay->max_window_size;

	cur_win = tx_delay->curr_win_idx;
	tx_delay->sw_delay_win_avg[cur_win] += (uint64_t)sw_delay;
	tx_delay->hw_delay_win_avg[cur_win] += (uint64_t)hw_delay;
	tx_delay->nw_delay_win_avg[cur_win] += (uint64_t)nw_delay;
	tx_delay->cur_win_num_pkts++;

	if (!(tx_delay->cur_win_num_pkts % max_pkt_per_window_size)) {
		/* Update the average of the completed window */
		tx_delay->sw_delay_win_avg[cur_win] = qdf_do_div(
					tx_delay->sw_delay_win_avg[cur_win],
					max_pkt_per_window_size);
		tx_delay->hw_delay_win_avg[cur_win] = qdf_do_div(
				tx_delay->hw_delay_win_avg[cur_win],
				max_pkt_per_window_size);
		tx_delay->nw_delay_win_avg[cur_win] = qdf_do_div(
				tx_delay->nw_delay_win_avg[cur_win],
				max_pkt_per_window_size);
		tx_delay->curr_win_idx++;
		tx_delay->cur_win_num_pkts = 0;

		/* Compute the moving average from all windows */
		if (tx_delay->curr_win_idx == max_window_size) {
			for (idx = 0; idx < max_window_size; idx++) {
				sw_avg_sum += tx_delay->sw_delay_win_avg[idx];
				hw_avg_sum += tx_delay->hw_delay_win_avg[idx];
				nw_avg_sum += tx_delay->nw_delay_win_avg[idx];
				tx_delay->sw_delay_win_avg[idx] = 0;
				tx_delay->hw_delay_win_avg[idx] = 0;
				tx_delay->nw_delay_win_avg[idx] = 0;
			}
			tx_delay->swdelay_avg = qdf_do_div(sw_avg_sum,
							   max_window_size);
			tx_delay->hwdelay_avg = qdf_do_div(hw_avg_sum,
							   max_window_size);
			tx_delay->nwdelay_avg = qdf_do_div(nw_avg_sum,
							   max_window_size);
			tx_delay->curr_win_idx = 0;
		}
	}
}

QDF_STATUS
dp_sawf_tx_enqueue_fail_peer_stats(struct dp_soc *soc,
				   struct dp_tx_desc_s *tx_desc)
{
	struct dp_peer_sawf_stats *sawf_ctx;
	struct dp_peer *peer;
	struct dp_txrx_peer *txrx_peer;
	uint8_t host_msduq_idx, host_q_idx, stats_cfg;
	uint16_t peer_id;

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!SAWF_BASIC_STATS_ENABLED(stats_cfg))
		return QDF_STATUS_E_FAILURE;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	peer_id = tx_desc->peer_id;

	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_TX);
	if (!peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		goto fail;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_DEC(sawf_ctx, tx_stats[host_q_idx].queue_depth, 1);

	dp_peer_unref_delete(peer, DP_MOD_ID_TX);

	return QDF_STATUS_SUCCESS;
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_TX);
	return QDF_STATUS_E_FAILURE;
}

static void dp_sawf_set_nw_delay(qdf_nbuf_t nbuf)
{
	uint32_t mark;
	uint32_t nw_delay = 0;
	uint32_t msduq;

	if (qdf_nbuf_get_tx_ftype(nbuf) == CB_FTYPE_SAWF) {
		nw_delay = (uint32_t)((uintptr_t)qdf_nbuf_get_tx_fctx(nbuf));
		if (nw_delay > SAWF_NW_DELAY_MASK)
			nw_delay = 0;
	}

	mark = qdf_nbuf_get_mark(nbuf);
	msduq = SAWF_MSDUQ_GET(mark);
	qdf_nbuf_set_mark(nbuf, SAWF_NW_DELAY_SET(mark, nw_delay, msduq));
}

QDF_STATUS
dp_sawf_tx_enqueue_peer_stats(struct dp_soc *soc,
			      struct dp_peer *peer,
			      struct dp_tx_desc_s *tx_desc)
{
	struct dp_peer_sawf_stats *sawf_ctx;
	struct dp_txrx_peer *txrx_peer;
	uint8_t host_msduq_idx, host_q_idx, stats_cfg;
	uint16_t peer_id;
	bool find_peer = false;

	if (!dp_sawf_tag_valid_get(tx_desc->nbuf))
		return QDF_STATUS_E_INVAL;

	peer_id = SAWF_PEER_ID_GET(qdf_nbuf_get_mark(tx_desc->nbuf));
	tx_desc->peer_id = peer_id;

	/*
	 * Set n/w-delay into mark field and process the same in the
	 * completion-path.If n/w-delay is invalid for any reason,
	 * move forward and process the other stats.
	 */
	dp_sawf_set_nw_delay(tx_desc->nbuf);

	/* Set enqueue tstamp in tx_desc */
	tx_desc->timestamp = qdf_ktime_real_get();

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!SAWF_BASIC_STATS_ENABLED(stats_cfg))
		return QDF_STATUS_E_FAILURE;

	host_msduq_idx = dp_sawf_queue_id_get(tx_desc->nbuf);
	if (host_msduq_idx == DP_SAWF_DEFAULT_Q_INVALID ||
	    host_msduq_idx < DP_SAWF_DEFAULT_Q_MAX)
		return QDF_STATUS_E_FAILURE;

	if (!peer) {
		peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_TX);
		if (!peer) {
			dp_sawf_debug("Invalid peer_id %u", peer_id);
			return QDF_STATUS_E_FAILURE;
		}
		find_peer = true;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("Invalid peer_id %u", peer_id);
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_ctx) {
		dp_sawf_err("Invalid SAWF stats ctx");
		goto fail;
	}

	host_q_idx = host_msduq_idx - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_INC(sawf_ctx, tx_stats[host_q_idx].queue_depth, 1);
	DP_STATS_INC_PKT(sawf_ctx, tx_stats[host_q_idx].tx_ingress, 1,
			 tx_desc->length);

	if (find_peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_TX);

	return QDF_STATUS_SUCCESS;
fail:
	if (find_peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_TX);
	return QDF_STATUS_E_FAILURE;
}

static void dp_sawf_dump_delay_stats(struct sawf_delay_stats *stats)
{
	uint8_t idx;

	/* CDP hist min, max and weighted average */
	dp_sawf_print_stats("Min  = %d Max  = %d Avg  = %d",
			    stats->delay_hist.min,
			    stats->delay_hist.max,
			    stats->delay_hist.avg);

	/* CDP hist bucket frequency */
	for (idx = 0; idx < CDP_HIST_BUCKET_MAX; idx++) {
		dp_sawf_print_stats("%s:  Packets = %llu",
				    dp_hist_tx_hw_delay_str(idx),
				    stats->delay_hist.hist.freq[idx]);
	}

	dp_sawf_print_stats("Delay bound success = %llu", stats->success);
	dp_sawf_print_stats("Delay bound failure = %llu", stats->failure);
}

static void dp_sawf_dump_tx_stats(struct sawf_tx_stats *tx_stats)
{
	dp_sawf_print_stats("tx_success: num = %u bytes = %llu",
			    tx_stats->tx_success.num,
			    tx_stats->tx_success.bytes);
	dp_sawf_print_stats("tx_ingress: num = %u bytes = %llu",
			    tx_stats->tx_ingress.num,
			    tx_stats->tx_ingress.bytes);
	dp_sawf_print_stats("dropped: fw_rem num = %u bytes = %llu",
			    tx_stats->dropped.fw_rem.num,
			    tx_stats->dropped.fw_rem.bytes);
	dp_sawf_print_stats("dropped: fw_rem_notx = %u",
		       tx_stats->dropped.fw_rem_notx);
	dp_sawf_print_stats("dropped: fw_rem_tx = %u",
		       tx_stats->dropped.age_out);
	dp_sawf_print_stats("dropped: fw_reason1 = %u",
		       tx_stats->dropped.fw_reason1);
	dp_sawf_print_stats("dropped: fw_reason2 = %u",
		       tx_stats->dropped.fw_reason2);
	dp_sawf_print_stats("dropped: fw_reason3 = %u",
		       tx_stats->dropped.fw_reason3);
	dp_sawf_print_stats("dropped: fw_rem_queue_disable = %u",
			tx_stats->dropped.fw_rem_queue_disable);
	dp_sawf_print_stats("dropped: fw_rem_no_match = %u",
			tx_stats->dropped.fw_rem_no_match);
	dp_sawf_print_stats("dropped: drop_threshold = %u",
			tx_stats->dropped.drop_threshold);
	dp_sawf_print_stats("dropped: drop_link_desc_na = %u",
			tx_stats->dropped.drop_link_desc_na);
	dp_sawf_print_stats("dropped: invalid_drop = %u",
			tx_stats->dropped.invalid_drop);
	dp_sawf_print_stats("dropped: mcast_vdev_drop = %u",
			tx_stats->dropped.mcast_vdev_drop);
	dp_sawf_print_stats("dropped: invalid_rr = %u",
			tx_stats->dropped.invalid_rr);
	dp_sawf_print_stats("tx_failed = %u", tx_stats->tx_failed);
	dp_sawf_print_stats("total_retries_count = %u", tx_stats->total_retries_count);
	dp_sawf_print_stats("retry_count = %u", tx_stats->retry_count);
	dp_sawf_print_stats("multiple_retry_count = %u", tx_stats->multiple_retry_count);
	dp_sawf_print_stats("failed_retry_count = %u", tx_stats->failed_retry_count);
	dp_sawf_print_stats("queue_depth = %u", tx_stats->queue_depth);
	dp_sawf_print_stats("throughput = %u", tx_stats->throughput);
	dp_sawf_print_stats("min throughput = %u", tx_stats->min_throughput);
	dp_sawf_print_stats("max throughput = %u", tx_stats->max_throughput);
	dp_sawf_print_stats("avg throughput = %u", tx_stats->avg_throughput);
	dp_sawf_print_stats("per = %u", tx_stats->per);
	dp_sawf_print_stats("retries percentage = %u", tx_stats->retries_pct);
	dp_sawf_print_stats("ingress rate = %u", tx_stats->ingress_rate);
	dp_sawf_print_stats("reinject packet = %u", tx_stats->reinject_pkt);
}

static void
dp_sawf_copy_delay_stats(struct sawf_delay_stats *dst,
			 struct sawf_delay_stats *src)
{
	dp_copy_hist_stats(&src->delay_hist, &dst->delay_hist);
	dst->nwdelay_avg = src->nwdelay_avg;
	dst->swdelay_avg = src->swdelay_avg;
	dst->hwdelay_avg = src->hwdelay_avg;
	dst->success = src->success;
	dst->failure = src->failure;
}

static void
dp_sawf_copy_tx_stats(struct sawf_tx_stats *dst, struct sawf_tx_stats *src)
{
	uint8_t pream_type, i;
	dst->tx_success.num = src->tx_success.num;
	dst->tx_success.bytes = src->tx_success.bytes;

	dst->tx_ingress.num = src->tx_ingress.num;
	dst->tx_ingress.bytes = src->tx_ingress.bytes;

	dst->dropped.fw_rem.num = src->dropped.fw_rem.num;
	dst->dropped.fw_rem.bytes = src->dropped.fw_rem.bytes;
	dst->dropped.fw_rem_notx = src->dropped.fw_rem_notx;
	dst->dropped.fw_rem_tx = src->dropped.fw_rem_tx;
	dst->dropped.age_out = src->dropped.age_out;
	dst->dropped.fw_reason1 = src->dropped.fw_reason1;
	dst->dropped.fw_reason2 = src->dropped.fw_reason2;
	dst->dropped.fw_reason3 = src->dropped.fw_reason3;

	dst->svc_intval_stats.success_cnt = src->svc_intval_stats.success_cnt;
	dst->svc_intval_stats.failure_cnt = src->svc_intval_stats.failure_cnt;
	dst->burst_size_stats.success_cnt = src->burst_size_stats.success_cnt;
	dst->burst_size_stats.failure_cnt = src->burst_size_stats.failure_cnt;

	dst->tx_failed = src->tx_failed;
	dst->queue_depth = src->queue_depth;
	dst->throughput = src->throughput;
	dst->ingress_rate = src->ingress_rate;
	dst->total_retries_count = src->total_retries_count;
	dst->retry_count = src->retry_count;
	dst->min_throughput = src->min_throughput;
	dst->max_throughput = src->max_throughput;
	dst->avg_throughput = src->avg_throughput;
	dst->per = src->per;
	dst->retries_pct = src->retries_pct;
	dst->multiple_retry_count = src->multiple_retry_count;
	dst->failed_retry_count = src->failed_retry_count;
	for (pream_type = 0; pream_type <  DOT11_MAX; pream_type++) {
		for (i = 0; i < MAX_MCS; i++) {
			dst->pkt_type[pream_type].mcs_count[i] =
			src->pkt_type[pream_type].mcs_count[i];
		}
	}
}

static QDF_STATUS
dp_sawf_find_msdu_from_svc_id(struct dp_peer_sawf *ctx, uint8_t svc_id,
			      uint8_t *tid, uint8_t *q_idx)
{
	uint8_t index = 0;

	for (index = 0; index < DP_SAWF_Q_MAX; index++) {
		if (ctx->msduq[index].svc_id == svc_id) {
			*tid = ctx->msduq[index].remapped_tid;
			*q_idx = ctx->msduq[index].htt_msduq;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

static uint16_t
dp_sawf_get_host_msduq_id(struct dp_peer_sawf *sawf_ctx,
			  uint8_t tid, uint8_t queue)
{
	uint8_t host_q_id;

	if (tid < DP_SAWF_TID_MAX && queue < DP_SAWF_DEFINED_Q_PTID_MAX) {
		host_q_id = sawf_ctx->msduq_map[tid][queue].host_queue_id;
		if (host_q_id)
			return host_q_id;
	}

	return DP_SAWF_PEER_Q_INVALID;
}

QDF_STATUS
dp_sawf_get_peer_delay_stats(struct cdp_soc_t *soc,
			     uint32_t svc_id, uint8_t *mac, void *data)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_delay_stats *stats, *dst, *src;
	uint8_t tid, q_idx;
	uint16_t host_q_id, host_q_idx;
	QDF_STATUS status;
	uint32_t nwdelay_avg, swdelay_avg, hwdelay_avg;
	uint8_t stats_cfg;

	stats = (struct sawf_delay_stats *)data;
	if (!stats) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	stats_cfg = wlan_cfg_get_sawf_stats_config(dp_soc->wlan_cfg_ctx);
	if (!stats_cfg) {
		dp_sawf_debug("sawf stats is disabled");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_err("stats ctx doesn't exist");
		goto fail;
	}

	dst = stats;

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_CDP);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	if (svc_id == DP_SAWF_STATS_SVC_CLASS_ID_ALL) {
		for (tid = 0; tid < DP_SAWF_MAX_TIDS; tid++) {
			for (q_idx = 0; q_idx < DP_SAWF_MAX_QUEUES; q_idx++) {
				host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
								      tid,
								      q_idx);
				if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
				    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
					dst++;
					continue;
				}
				host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
				if (host_q_idx > DP_SAWF_Q_MAX - 1) {
					dst++;
					continue;
				}

				src = &stats_ctx->stats.delay[host_q_idx];
				telemetry_sawf_get_mov_avg(
						sawf_ctx->telemetry_ctx,
						tid, host_q_idx, &nwdelay_avg,
						&swdelay_avg, &hwdelay_avg);
				src->nwdelay_avg = nwdelay_avg;
				src->swdelay_avg = swdelay_avg;
				src->hwdelay_avg = hwdelay_avg;

				dp_sawf_print_stats("-- TID: %u MSDUQ: %u --",
						    tid, q_idx);
				dp_sawf_dump_delay_stats(src);
				dp_sawf_copy_delay_stats(dst, src);

				dst++;
			}
		}
	} else {
		/*
		 * Find msduqs of the peer from service class ID
		 */
		status = dp_sawf_find_msdu_from_svc_id(sawf_ctx, svc_id,
						       &tid, &q_idx);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_sawf_err("No MSDU Queue for svc id %u",
				    svc_id);
			goto fail;
		}

		host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
						      tid,
						      q_idx);
		if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
		    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
			dp_sawf_err("No host queue for tid %u queue %u",
				    tid, q_idx);
			goto fail;
		}
		host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
		if (host_q_idx > DP_SAWF_Q_MAX - 1) {
			dp_sawf_err("Invalid host queue idx %u", host_q_idx);
			goto fail;
		}

		src = &stats_ctx->stats.delay[host_q_idx];
		telemetry_sawf_get_mov_avg(sawf_ctx->telemetry_ctx,
					   tid, host_q_idx, &nwdelay_avg,
					   &swdelay_avg, &hwdelay_avg);
		src->nwdelay_avg = nwdelay_avg;
		src->swdelay_avg = swdelay_avg;
		src->hwdelay_avg = hwdelay_avg;

		dp_sawf_print_stats("----TID: %u MSDUQ: %u ----", tid, q_idx);
		dp_sawf_dump_delay_stats(src);
		dp_sawf_copy_delay_stats(dst, src);

		dst->tid = tid;
		dst->msduq = q_idx;
	}

	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_peer_tx_stats(struct cdp_soc_t *soc,
			  uint32_t svc_id, uint8_t *mac, void *data)
{
	struct dp_soc *dp_soc;
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *stats, *dst, *src;
	uint8_t tid, q_idx;
	uint16_t host_q_id, host_q_idx;
	uint32_t throughput, ingress_rate;
	uint32_t min_tput, max_tput, avg_tput, per, retries_pct;
	QDF_STATUS status;
	uint8_t stats_cfg;

	stats = (struct sawf_tx_stats *)data;
	if (!stats) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	stats_cfg = wlan_cfg_get_sawf_stats_config(dp_soc->wlan_cfg_ctx);
	if (!stats_cfg) {
		dp_sawf_debug("sawf stats is disabled");
		return QDF_STATUS_E_FAILURE;
	}

	peer = dp_peer_find_hash_find(dp_soc, mac, 0,
				      DP_VDEV_ALL, DP_MOD_ID_CDP);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_err("stats ctx doesn't exist");
		goto fail;
	}

	dst = stats;

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_CDP);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto fail;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	if (svc_id == DP_SAWF_STATS_SVC_CLASS_ID_ALL) {
		for (tid = 0; tid < DP_SAWF_MAX_TIDS; tid++) {
			for (q_idx = 0; q_idx < DP_SAWF_MAX_QUEUES; q_idx++) {
				host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
								      tid,
								      q_idx);
				if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
				    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
					dst++;
					continue;
				}
				host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
				if (host_q_idx > DP_SAWF_Q_MAX - 1) {
					dst++;
					continue;
				}

				src = &stats_ctx->stats.tx_stats[host_q_idx];
				telemetry_sawf_get_rate(sawf_ctx->telemetry_ctx,
							tid, host_q_idx,
							&throughput,
							&ingress_rate);
				telemetry_sawf_get_tx_rate(
						sawf_ctx->telemetry_ctx,
						tid, host_q_idx,
						&min_tput, &max_tput,
						&avg_tput, &per, &retries_pct);
				src->throughput = throughput;
				src->ingress_rate = ingress_rate;
				src->min_throughput = min_tput;
				src->max_throughput = max_tput;
				src->avg_throughput = avg_tput;
				src->per = per;
				src->retries_pct = retries_pct;

				dp_sawf_print_stats("-- TID: %u MSDUQ: %u --",
						    tid, q_idx);
				dp_sawf_dump_tx_stats(src);
				dp_sawf_copy_tx_stats(dst, src);
				dst->tid = tid;
				dst->msduq = q_idx;

				dst++;
			}
		}
	} else {
		/*
		 * Find msduqs of the peer from service class ID
		 */
		status = dp_sawf_find_msdu_from_svc_id(sawf_ctx, svc_id,
						       &tid, &q_idx);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_sawf_err("No MSDU Queue for svc id %u",
				    svc_id);
			goto fail;
		}

		host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
						      tid,
						      q_idx);
		if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
		    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
			dp_sawf_err("No host queue for tid %u queue %u",
				    tid, q_idx);
			goto fail;
		}
		host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
		if (host_q_idx > DP_SAWF_Q_MAX - 1) {
			dp_sawf_err("Invalid host queue idx %u", host_q_idx);
			goto fail;
		}

		src = &stats_ctx->stats.tx_stats[host_q_idx];
		telemetry_sawf_get_rate(sawf_ctx->telemetry_ctx,
					tid, host_q_idx,
					&throughput, &ingress_rate);
		telemetry_sawf_get_tx_rate(
					sawf_ctx->telemetry_ctx,
					tid, host_q_idx,
					&min_tput, &max_tput,
					&avg_tput, &per, &retries_pct);
		src->throughput = throughput;
		src->ingress_rate = ingress_rate;
		src->min_throughput = min_tput;
		src->max_throughput = max_tput;
		src->avg_throughput = avg_tput;
		src->per = per;
		src->retries_pct = retries_pct;

		dp_sawf_print_stats("----TID: %u MSDUQ: %u ----",
				    tid, q_idx);
		dp_sawf_dump_tx_stats(src);
		dp_sawf_copy_tx_stats(dst, src);
		dst->tid = tid;
		dst->msduq = q_idx;
	}

	dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
fail:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_CDP);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return QDF_STATUS_E_FAILURE;
}

#ifdef QCA_PEER_EXT_STATS
QDF_STATUS
dp_txrx_pull_tx_peer_stats(uint8_t *peer_mac, uint32_t *min_tput,
			   uint32_t *max_tput, uint32_t *avg_tput,
			   uint32_t *per, uint32_t *retries_pct)
{
	return telemetry_pull_tx_peer_stats(peer_mac, min_tput, max_tput,
					    avg_tput, per, retries_pct);
}
#endif

#ifdef SAWF_ADMISSION_CONTROL
QDF_STATUS
dp_sawf_get_peer_admctrl_stats(struct cdp_soc_t *soc, uint8_t *mac, void *data,
			       enum cdp_peer_type peer_type)
{
	uint8_t tid, q_idx;
	uint8_t stats_cfg;
	struct dp_soc *dp_soc;
	struct sawf_tx_stats *sawf_stats;
	uint64_t tx_success_pkt = 0;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	uint16_t host_q_id, host_q_idx;
	cdp_peer_stats_param_t buf = {0};
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_admctrl_peer_stats *admctrl_stats;
	struct sawf_admctrl_msduq_stats *msduq_stats;
	struct cdp_peer_info peer_info = {0};
	uint8_t stats_arr_size, inx, link_id, ac;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dp_peer_per_pkt_stats *per_pkt_stats;
	struct cdp_peer_telemetry_stats telemetry_stats = {0};
	struct dp_peer *peer = NULL, *primary_link_peer = NULL;

	admctrl_stats = (struct sawf_admctrl_peer_stats *)data;
	if (!admctrl_stats) {
		dp_sawf_err("Invalid data to fill");
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	stats_cfg = wlan_cfg_get_sawf_stats_config(dp_soc->wlan_cfg_ctx);
	if (!stats_cfg) {
		dp_sawf_debug("sawf stats is disabled");
		return QDF_STATUS_E_FAILURE;
	}

	DP_PEER_INFO_PARAMS_INIT(&peer_info, DP_VDEV_ALL, mac, false,
				 peer_type);

	peer = dp_peer_hash_find_wrapper(dp_soc, &peer_info, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto done;
	}

	if (!IS_MLO_DP_LINK_PEER(peer)) {
		stats_arr_size = txrx_peer->stats_arr_size;
		for (inx = 0; inx < stats_arr_size; inx++) {
			per_pkt_stats = &txrx_peer->stats[inx].per_pkt_stats;
			tx_success_pkt += per_pkt_stats->tx.tx_success.num;
		}
	} else {
		link_id = dp_get_peer_hw_link_id(dp_soc, peer->vdev->pdev);
		per_pkt_stats =
			&txrx_peer->stats[link_id].per_pkt_stats;
		tx_success_pkt = per_pkt_stats->tx.tx_success.num;
	}

	/* Fill Tx Success Pkt Count */
	admctrl_stats->tx_success_pkt = tx_success_pkt;

	/* Only Tx_Success Pkt count is required for MLD MAC, return */
	if (peer_type == CDP_MLD_PEER_TYPE) {
		status = QDF_STATUS_SUCCESS;
		goto done;
	}

	/* Fetch and update Tx airtime consumption stats */
	dp_get_peer_telemetry_stats(soc, mac, &telemetry_stats);
	for (ac = 0; ac < WME_AC_MAX; ac++)
		admctrl_stats->tx_airtime_consumption[ac] = telemetry_stats.tx_airtime_consumption[ac];

	/* Fetch and update Tx average rate */
	dp_txrx_get_peer_extd_stats_param(peer, cdp_peer_tx_avg_rate, &buf);
	admctrl_stats->avg_tx_rate = buf.tx_rate_avg;

	/* Fetch and update SAWF MSDUQ Stats */
	stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!stats_ctx) {
		dp_sawf_err("stats ctx doesn't exist");
		goto done;
	}

	primary_link_peer = dp_get_primary_link_peer_by_id(dp_soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_SAWF);
	if (!primary_link_peer) {
		dp_sawf_err("No primary link peer found");
		goto done;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(primary_link_peer);
	if (!sawf_ctx) {
		dp_sawf_err("sawf_ctx doesn't exist");
		goto done;
	}

	msduq_stats = admctrl_stats->msduq_stats;

	for (tid = 0; tid < DP_SAWF_MAX_TIDS; tid++) {
		for (q_idx = 0; q_idx < DP_SAWF_MAX_QUEUES; q_idx++) {
			host_q_id = dp_sawf_get_host_msduq_id(sawf_ctx,
							      tid,
							      q_idx);
			if (host_q_id == DP_SAWF_PEER_Q_INVALID ||
			    host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
				msduq_stats++;
				continue;
			}
			host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
			if (host_q_idx > DP_SAWF_Q_MAX - 1) {
				msduq_stats++;
				continue;
			}

			sawf_stats = &stats_ctx->stats.tx_stats[host_q_idx];
			msduq_stats->tx_success_pkt = sawf_stats->tx_success.num;
			msduq_stats++;
		}
	}

	status = QDF_STATUS_SUCCESS;
done:
	if (primary_link_peer)
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_SAWF);
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return status;
}
#endif

QDF_STATUS
dp_sawf_get_tx_stats(void *arg, uint64_t *in_bytes, uint64_t *in_cnt,
		     uint64_t *tx_bytes, uint64_t *tx_cnt,
		     uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*in_bytes = tx_stats->tx_ingress.bytes;
	*in_cnt = tx_stats->tx_ingress.num;
	*tx_bytes = tx_stats->tx_success.bytes;
	*tx_cnt = tx_stats->tx_success.num;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_CONFIG_TELEMETRY_AGENT
QDF_STATUS
dp_sawf_get_msduq_tx_stats(void *arg,
			   void *msduq_tx_stats,
			   uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;
	struct telemetry_msduq_tx_stats *retries_mcs_stats = (struct telemetry_msduq_tx_stats *)msduq_tx_stats;
	int pkt_type_index, mcs_index;

	stats_ctx = (struct dp_peer_sawf_stats *)arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	if (!stats_ctx) {
		dp_sawf_err("stats_ctx in NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	retries_mcs_stats->retry_count = tx_stats->retry_count;
	retries_mcs_stats->tx_failed = tx_stats->tx_failed;
	retries_mcs_stats->total_retries_count = tx_stats->total_retries_count;
	for (pkt_type_index = 0; pkt_type_index < DOT11_MAX; pkt_type_index++) {
		for (mcs_index = 0; mcs_index < MAX_MCS; mcs_index++) {
			retries_mcs_stats->packet_type[pkt_type_index].mcs_count[mcs_index] =
				tx_stats->pkt_type[pkt_type_index].mcs_count[mcs_index];
		}
	}

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS
dp_sawf_get_msduq_tx_stats(void *arg,
			   void *msduq_tx_stats,
			   uint8_t msduq)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS
dp_sawf_get_mpdu_sched_stats(void *arg, uint64_t *svc_int_pass,
			     uint64_t *svc_int_fail, uint64_t *burst_pass,
			     uint64_t *burst_fail, uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*svc_int_pass = tx_stats->svc_intval_stats.success_cnt;
	*svc_int_fail = tx_stats->svc_intval_stats.failure_cnt;
	*burst_pass = tx_stats->burst_size_stats.success_cnt;
	*burst_fail = tx_stats->burst_size_stats.failure_cnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_sawf_get_drop_stats(void *arg, uint64_t *pass, uint64_t *drop,
		       uint64_t *drop_ttl, uint8_t tid, uint8_t msduq)
{
	struct dp_peer_sawf_stats *stats_ctx;
	struct sawf_tx_stats *tx_stats;

	stats_ctx = arg;
	tx_stats = &stats_ctx->stats.tx_stats[msduq];

	*pass = tx_stats->tx_success.num;
	*drop = tx_stats->tx_failed;
	*drop_ttl = tx_stats->dropped.age_out;

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_sawf_mpdu_stats_req_send() - Send MPDU stats request to target
 * @soc_hdl: SOC handle
 * @stats_type: MPDU stats type
 * @enable: 1: Enable 0: Disable
 * @config_param0: Opaque configuration
 * @config_param1: Opaque configuration
 * @config_param2: Opaque configuration
 * @config_param3: Opaque configuration
 *
 * @Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
dp_sawf_mpdu_stats_req_send(struct cdp_soc_t *soc_hdl,
			    uint8_t stats_type, uint8_t enable,
			    uint32_t config_param0, uint32_t config_param1,
			    uint32_t config_param2, uint32_t config_param3)
{
	struct dp_soc *dp_soc;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (!dp_soc) {
		dp_sawf_err("Invalid soc");
		return QDF_STATUS_E_INVAL;
	}

	return dp_sawf_htt_h2t_mpdu_stats_req(dp_soc->htt_handle,
						   stats_type,
						   enable,
						   config_param0,
						   config_param1,
						   config_param2,
						   config_param3);
}

QDF_STATUS
dp_sawf_mpdu_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	struct dp_soc *dp_soc;
	uint8_t stats_cfg;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);
	stats_cfg = wlan_cfg_get_sawf_stats_config(dp_soc->wlan_cfg_ctx);
	if (!SAWF_ADVNCD_STATS_ENABLED(stats_cfg))
		return QDF_STATUS_E_FAILURE;

	return dp_sawf_mpdu_stats_req_send(soc_hdl,
					   HTT_STRM_GEN_MPDUS_STATS,
					   enable,
					   0, 0, 0, 0);
}

QDF_STATUS
dp_sawf_mpdu_details_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	struct dp_soc *dp_soc;
	uint8_t stats_cfg;

	dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);
	stats_cfg = wlan_cfg_get_sawf_stats_config(dp_soc->wlan_cfg_ctx);
	if (!SAWF_ADVNCD_STATS_ENABLED(stats_cfg))
		return QDF_STATUS_E_FAILURE;

	return dp_sawf_mpdu_stats_req_send(soc_hdl,
					   HTT_STRM_GEN_MPDUS_DETAILS_STATS,
					   enable,
					   0, 0, 0, 0);
}

QDF_STATUS dp_sawf_update_mpdu_basic_stats(struct dp_soc *soc,
					   uint16_t peer_id,
					   uint8_t tid, uint8_t q_idx,
					   uint64_t svc_intval_success_cnt,
					   uint64_t svc_intval_failure_cnt,
					   uint64_t burst_size_success_cnt,
					   uint64_t burst_size_failue_cnt)
{
	struct dp_peer *peer, *mld_peer, *primary_link_peer;
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer_sawf_stats *sawf_stats_ctx;
	uint8_t host_q_id, host_q_idx, stats_cfg;

	if (!soc) {
		dp_sawf_err("Invalid soc context");
		return QDF_STATUS_E_FAILURE;
	}

	stats_cfg = wlan_cfg_get_sawf_stats_config(soc->wlan_cfg_ctx);
	if (!SAWF_ADVNCD_STATS_ENABLED(stats_cfg))
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (IS_MLO_DP_LINK_PEER(peer))
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
	else if (IS_MLO_DP_MLD_PEER(peer))
		mld_peer = peer;
	else
		mld_peer = NULL;

	if (mld_peer) {
		primary_link_peer = dp_get_primary_link_peer_by_id
			(soc, mld_peer->peer_id, DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			dp_sawf_err("Invalid primary peer");
			return QDF_STATUS_E_FAILURE;
		}

		/*
		 * Release the MLD-peer reference.
		 * Hold only primary link ref now.
		 */
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		peer = primary_link_peer;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_sawf_err("stats_ctx doesn't exist");
		goto fail;
	}

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_err("txrx peer is NULL");
		goto fail;
	}

	sawf_stats_ctx = dp_peer_sawf_stats_ctx_get(txrx_peer);
	if (!sawf_stats_ctx) {
		dp_sawf_err("No sawf stats ctx for peer_id %d", peer_id);
		goto fail;
	}

	host_q_id = sawf_ctx->msduq_map[tid][q_idx].host_queue_id;
	if (!host_q_id || host_q_id < DP_SAWF_DEFAULT_Q_MAX) {
		dp_sawf_err("Invalid host queue id %u", host_q_id);
		goto fail;
	}

	host_q_idx = host_q_id - DP_SAWF_DEFAULT_Q_MAX;
	if (host_q_idx > DP_SAWF_Q_MAX - 1) {
		dp_sawf_err("Invalid host queue idx %u", host_q_idx);
		goto fail;
	}

	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].svc_intval_stats.success_cnt,
		     svc_intval_success_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].svc_intval_stats.failure_cnt,
		     svc_intval_failure_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].burst_size_stats.success_cnt,
		     burst_size_success_cnt);
	DP_STATS_INC(sawf_stats_ctx,
		     tx_stats[host_q_idx].burst_size_stats.failure_cnt,
		     burst_size_failue_cnt);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_SUCCESS;
fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_sawf_set_mov_avg_params(uint32_t num_pkt,
				      uint32_t num_win)
{
	sawf_telemetry_cfg.mov_avg.packet = num_pkt;
	sawf_telemetry_cfg.mov_avg.window = num_win;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_sawf_set_mov_avg_params);

QDF_STATUS dp_sawf_set_sla_params(uint32_t num_pkt,
				  uint32_t time_secs)
{
	sawf_telemetry_cfg.sla.num_packets = num_pkt;
	sawf_telemetry_cfg.sla.time_secs = time_secs;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_sawf_set_sla_params);

QDF_STATUS dp_sawf_init_telemetry_params(void)
{
	sawf_telemetry_cfg.mov_avg.packet = SAWF_TELEMETRY_MOV_AVG_PACKETS;
	sawf_telemetry_cfg.mov_avg.window = SAWF_TELEMETRY_MOV_AVG_WINDOWS;

	sawf_telemetry_cfg.sla.num_packets = SAWF_TELEMETRY_SLA_PACKETS;
	sawf_telemetry_cfg.sla.time_secs = SAWF_TELEMETRY_SLA_TIME;

	return QDF_STATUS_SUCCESS;
}

static void dp_sawf_peer_stats_clear(struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer;
	struct dp_peer_sawf_stats *sawf_stats;
	uint8_t tid, msduq, idx;

	txrx_peer = dp_get_txrx_peer(peer);
	if (!txrx_peer) {
		dp_sawf_warn("Invalid txrx peer");
		return;
	}

	sawf_stats = txrx_peer->sawf_stats;
	if (!sawf_stats) {
		dp_sawf_warn("Invalid sawf stats");
		return;
	}

	qdf_spin_lock_bh(&sawf_stats->stats.lock);
	for (idx = 0; idx < DP_SAWF_Q_MAX; idx++) {
		tid = sawf_stats->stats.delay[idx].tid;
		msduq = sawf_stats->stats.delay[idx].msduq;

		qdf_mem_zero(&sawf_stats->stats.tx_stats[idx],
			     sizeof(struct sawf_tx_stats));
		qdf_mem_zero(&sawf_stats->stats.delay[idx],
			     sizeof(struct sawf_delay_stats));
		sawf_stats->stats.delay[idx].tid = tid;
		sawf_stats->stats.delay[idx].msduq = msduq;
		sawf_stats->stats.tx_stats[idx].tid = tid;
		sawf_stats->stats.tx_stats[idx].msduq = msduq;
	}
	qdf_spin_unlock_bh(&sawf_stats->stats.lock);
}

void
dp_sawf_peer_stats_reset(struct dp_soc *soc,
			 struct dp_peer *peer)
{
	struct dp_peer_sawf *sawf_ctx = NULL;

	sawf_ctx = dp_peer_sawf_ctx_get(peer);

	dp_sawf_peer_stats_clear(peer);

	if (sawf_ctx && sawf_ctx->telemetry_ctx)
		telemetry_sawf_peer_stats_reset(sawf_ctx->telemetry_ctx);
}

#else
QDF_STATUS
dp_peer_sawf_stats_ctx_alloc(struct dp_soc *soc,
			     struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_peer_sawf_stats_ctx_free(struct dp_soc *soc,
			    struct dp_txrx_peer *txrx_peer)
{
	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
dp_sawf_update_tx_delay(struct dp_soc *soc,
			struct dp_vdev *vdev,
			struct hal_tx_completion_status *ts,
			struct dp_tx_desc_s *tx_desc,
			struct dp_peer_sawf *sawf_ctx,
			struct sawf_stats *stats,
			uint8_t tid,
			uint8_t host_q_idx)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_tx_compl_update_peer_stats(struct dp_soc *soc,
				   struct dp_vdev *vdev,
				   struct dp_txrx_peer *txrx_peer,
				   struct dp_tx_desc_s *tx_desc,
				   struct hal_tx_completion_status *ts,
				   uint8_t host_tid)
{
	return QDF_STATUS_E_FAILURE;
}

void dp_peer_tid_delay_avg(struct cdp_delay_tx_stats *tx_delay,
			   uint32_t nw_delay,
			   uint32_t sw_delay,
			   uint32_t hw_delay)
{
}

QDF_STATUS
dp_sawf_tx_enqueue_fail_peer_stats(struct dp_soc *soc,
				   struct dp_tx_desc_s *tx_desc)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_tx_enqueue_peer_stats(struct dp_soc *soc,
			      struct dp_tx_desc_s *tx_desc)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_peer_delay_stats(struct cdp_soc_t *soc,
			     uint32_t svc_id, uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_peer_tx_stats(struct cdp_soc_t *soc,
			  uint32_t svc_id, uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

#ifdef QCA_PEER_EXT_STATS
QDF_STATUS
dp_txrx_pull_tx_peer_stats(uint8_t *peer_mac, uint32_t *min_tput,
			   uint32_t *max_tput, uint32_t *avg_tput,
			   uint32_t *per, uint32_t *retries_pct)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS
dp_sawf_get_tx_stats(void *arg, uint64_t *in_bytes, uint64_t *in_cnt,
		     uint64_t *tx_bytes, uint64_t *tx_cnt,
		     uint8_t tid, uint8_t msduq)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_msduq_tx_stats(void *arg,
			   struct telemetry_msduq_tx_stats *msduq_tx_stats,
			   uint8_t msduq);
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_mpdu_sched_stats(void *arg, uint64_t *svc_int_pass,
			     uint64_t *svc_int_fail, uint64_t *burst_pass,
			     uint64_t *burst_fail, uint8_t tid, uint8_t msduq)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_get_drop_stats(void *arg, uint64_t *pass, uint64_t *drop,
		       uint64_t *drop_ttl, uint8_t tid, uint8_t msduq)
{
	return QDF_STATUS_E_FAILURE;
}


QDF_STATUS
dp_sawf_mpdu_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_sawf_mpdu_details_stats_req(struct cdp_soc_t *soc_hdl, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}
QDF_STATUS dp_sawf_update_mpdu_basic_stats(struct dp_soc *soc,
					   uint16_t peer_id,
					   uint8_t tid, uint8_t q_idx,
					   uint64_t svc_intval_success_cnt,
					   uint64_t svc_intval_failure_cnt,
					   uint64_t burst_size_success_cnt,
					   uint64_t burst_size_failue_cnt)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_sawf_set_mov_avg_params(uint32_t num_pkt,
				      uint32_t num_win)
{
	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(dp_sawf_set_mov_avg_params);

QDF_STATUS dp_sawf_set_sla_params(uint32_t num_pkt,
				  uint32_t time_secs)
{
	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(dp_sawf_set_sla_params);

QDF_STATUS dp_sawf_init_telemetry_params(void)
{
	return QDF_STATUS_E_FAILURE;
}

void
dp_sawf_peer_stats_reset(struct dp_soc *soc,
			 struct dp_peer *peer)
{
}
#endif
