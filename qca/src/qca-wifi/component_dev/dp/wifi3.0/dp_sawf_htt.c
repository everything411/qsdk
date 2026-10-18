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

#include <dp_types.h>
#include <dp_peer.h>
#include <dp_internal.h>
#include <dp_htt.h>
#include <dp_sawf.h>
#include <dp_sawf_htt.h>
#include "cdp_txrx_cmn_reg.h"
#include <wlan_telemetry_agent.h>

#define HTT_MSG_BUF_SIZE(msg_bytes) \
	((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

static void
dp_htt_h2t_send_complete_free_netbuf(void *soc, A_STATUS status,
				     qdf_nbuf_t netbuf)
{
	qdf_nbuf_free(netbuf);
}

QDF_STATUS
dp_htt_sawf_get_host_queue(struct dp_soc *soc, uint32_t *msg_word,
			   uint8_t *host_queue)
{
	uint8_t cl_info, fl_override, hlos_tid;
	uint16_t ast_idx;
	hlos_tid = HTT_T2H_SAWF_MSDUQ_INFO_HTT_HLOS_TID_GET(*msg_word);
	ast_idx = HTT_T2H_SAWF_MSDUQ_INFO_HTT_AST_LIST_IDX_GET(*msg_word);

	if (hlos_tid > DP_SAWF_TID_MAX - 1) {
		wlan_cfg_set_sawf_config(soc->wlan_cfg_ctx, 0);
		qdf_err("Invalid HLOS TID:%u, Disable sawf.", hlos_tid);
		return QDF_STATUS_E_FAILURE;
	}

	switch (soc->arch_id) {
	case CDP_ARCH_TYPE_LI:
		qdf_info("|AST idx: %d|HLOS TID:%d", ast_idx, hlos_tid);
		if ((ast_idx != 2) && (ast_idx != 3)) {
			wlan_cfg_set_sawf_config(soc->wlan_cfg_ctx, 0);
			qdf_err("Invalid HLOS TID:%u, Disable sawf.", hlos_tid);
			return QDF_STATUS_E_FAILURE;
		}
		*host_queue = DP_SAWF_DEFAULT_Q_MAX +
			      (ast_idx - 2) * DP_SAWF_TID_MAX + hlos_tid;

		break;
	case CDP_ARCH_TYPE_BE:
		cl_info =
		HTT_T2H_SAWF_MSDUQ_INFO_HTT_WHO_CLASS_INFO_SEL_GET(*msg_word);
		fl_override =
		HTT_T2H_SAWF_MSDUQ_INFO_HTT_FLOW_OVERRIDE_GET(*msg_word);
		*host_queue = (cl_info * DP_SAWF_DEFAULT_Q_MAX) +
			      (fl_override * DP_SAWF_TID_MAX) + hlos_tid;
		qdf_info("|Cl Info:%u|FL Override:%u|HLOS TID:%u", cl_info,
			 fl_override, hlos_tid);
		break;
	default:
		dp_err("unkonwn arch_id 0x%x", soc->arch_id);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_htt_h2t_sawf_def_queues_map_req(struct htt_soc *soc,
				   uint8_t svc_class_id, uint16_t peer_id)
{
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	uint8_t *htt_logger_bufp;
	struct dp_htt_htc_pkt *pkt;
	QDF_STATUS status;

	htt_msg = qdf_nbuf_alloc(
			soc->osdev,
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_MAP_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);

	if (!htt_msg) {
		dp_htt_err("Fail to allocate htt msg buffer");
		return QDF_STATUS_E_NOMEM;
	}

	if (!qdf_nbuf_put_tail(htt_msg, HTT_SAWF_DEF_QUEUES_MAP_REQ_BYTES)) {
		dp_htt_err("Fail to expand head for SAWF_DEF_QUEUES_MAP_REQ");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_FAILURE;
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;

	HTT_H2T_MSG_TYPE_SET(*msg_word,
			     HTT_H2T_SAWF_DEF_QUEUES_MAP_REQ);
	HTT_RX_SAWF_DEF_QUEUES_MAP_REQ_SVC_CLASS_ID_SET(*msg_word,
							svc_class_id - 1);
	HTT_RX_SAWF_DEF_QUEUES_MAP_REQ_PEER_ID_SET(*msg_word, peer_id);

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		dp_htt_err("Fail to allocate dp_htt_htc_pkt buffer");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL;

	/* macro to set packet parameters for TX */
	SET_HTC_PACKET_INFO_TX(
			&pkt->htc_pkt,
			dp_htt_h2t_send_complete_free_netbuf,
			qdf_nbuf_data(htt_msg),
			qdf_nbuf_len(htt_msg),
			soc->htc_endpoint,
			HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, htt_msg);

	status = DP_HTT_SEND_HTC_PKT(soc, pkt,
				     HTT_H2T_SAWF_DEF_QUEUES_MAP_REQ,
				     htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

QDF_STATUS
dp_htt_h2t_sawf_def_queues_unmap_req(struct htt_soc *soc,
				     uint8_t svc_id, uint16_t peer_id)
{
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	uint8_t *htt_logger_bufp;
	struct dp_htt_htc_pkt *pkt;
	QDF_STATUS status;

	htt_msg = qdf_nbuf_alloc(
			soc->osdev,
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_UNMAP_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);

	if (!htt_msg) {
		dp_htt_err("Fail to allocate htt msg buffer");
		return QDF_STATUS_E_NOMEM;
	}

	if (!qdf_nbuf_put_tail(htt_msg, HTT_SAWF_DEF_QUEUES_UNMAP_REQ_BYTES)) {
		dp_htt_err("Fail to expand head for SAWF_DEF_QUEUES_UNMAP_REQ");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_FAILURE;
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;

	HTT_H2T_MSG_TYPE_SET(*msg_word,
			     HTT_H2T_SAWF_DEF_QUEUES_UNMAP_REQ);
	HTT_RX_SAWF_DEF_QUEUES_UNMAP_REQ_SVC_CLASS_ID_SET(*msg_word,
							  svc_id - 1);
	HTT_RX_SAWF_DEF_QUEUES_UNMAP_REQ_PEER_ID_SET(*msg_word, peer_id);

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		dp_htt_err("Fail to allocate dp_htt_htc_pkt buffer");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL;

	/* macro to set packet parameters for TX */
	SET_HTC_PACKET_INFO_TX(
			&pkt->htc_pkt,
			dp_htt_h2t_send_complete_free_netbuf,
			qdf_nbuf_data(htt_msg),
			qdf_nbuf_len(htt_msg),
			soc->htc_endpoint,
			HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, htt_msg);

	status = DP_HTT_SEND_HTC_PKT(
			soc, pkt,
			HTT_H2T_SAWF_DEF_QUEUES_UNMAP_REQ, htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

QDF_STATUS
dp_htt_h2t_sawf_def_queues_map_report_req(struct htt_soc *soc,
					  uint16_t peer_id, uint8_t tid_mask)
{
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	uint8_t *htt_logger_bufp;
	struct dp_htt_htc_pkt *pkt;
	QDF_STATUS status;

	htt_msg = qdf_nbuf_alloc(
			soc->osdev,
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_MAP_REPORT_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);

	if (!htt_msg) {
		dp_htt_err("Fail to allocate htt msg buffer");
		return QDF_STATUS_E_NOMEM;
	}

	if (!qdf_nbuf_put_tail(htt_msg,
			       HTT_SAWF_DEF_QUEUES_MAP_REPORT_REQ_BYTES)) {
		dp_htt_err("Fail to expand head for SAWF_DEF_QUEUES_MAP_REQ");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_FAILURE;
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;

	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_SAWF_DEF_QUEUES_MAP_REPORT_REQ);
	HTT_RX_SAWF_DEF_QUEUES_MAP_REPORT_REQ_TID_MASK_SET(*msg_word, tid_mask);
	HTT_RX_SAWF_DEF_QUEUES_MAP_REPORT_REQ_PEER_ID_SET(*msg_word, peer_id);
	msg_word++;
	*msg_word = 0;
	HTT_RX_SAWF_DEF_QUEUES_MAP_REPORT_REQ_EXISTING_TIDS_ONLY_SET(*msg_word,
								     0);

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		dp_htt_err("Fail to allocate dp_htt_htc_pkt buffer");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL; /* not used during send-done callback */

	/* macro to set packet parameters for TX */
	SET_HTC_PACKET_INFO_TX(
			&pkt->htc_pkt,
			dp_htt_h2t_send_complete_free_netbuf,
			qdf_nbuf_data(htt_msg),
			qdf_nbuf_len(htt_msg),
			soc->htc_endpoint,
			HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, htt_msg);

	status = DP_HTT_SEND_HTC_PKT(
			soc, pkt,
			HTT_H2T_SAWF_DEF_QUEUES_MAP_REPORT_REQ,
			htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

QDF_STATUS
dp_htt_sawf_def_queues_map_report_conf(struct htt_soc *soc,
				       uint32_t *msg_word,
				       qdf_nbuf_t htt_t2h_msg)
{
	int tid_report_bytes, rem_bytes;
	uint16_t peer_id;
	uint8_t tid, svc_class_id;
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;

	peer_id = HTT_T2H_SAWF_DEF_QUEUES_MAP_REPORT_CONF_PEER_ID_GET(*msg_word);

	peer = dp_peer_get_ref_by_id(soc->dp_soc, peer_id,
				     DP_MOD_ID_HTT);
	if (!peer) {
		qdf_info("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_peer_unref_delete(peer, DP_MOD_ID_HTT);
		qdf_err("Invalid SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Target can send multiple tid reports in the response
	 * based on the report request, tid_report_bytes is the length of
	 * all tid reports in bytes.
	 */
	tid_report_bytes = qdf_nbuf_len(htt_t2h_msg) -
		HTT_SAWF_DEF_QUEUES_MAP_REPORT_CONF_HDR_BYTES;

	for (rem_bytes = tid_report_bytes; rem_bytes > 0;
		rem_bytes -= HTT_SAWF_DEF_QUEUES_MAP_REPORT_CONF_ELEM_BYTES) {
		++msg_word;
		tid = HTT_T2H_SAWF_DEF_QUEUES_MAP_REPORT_CONF_TID_GET(*msg_word);
		svc_class_id = HTT_T2H_SAWF_DEF_QUEUES_MAP_REPORT_CONF_SVC_CLASS_ID_GET(*msg_word);
		svc_class_id += 1;
		qdf_info("peer id %u tid 0x%x svc_class_id %u",
			 peer_id, tid, svc_class_id);
		if (tid < DP_SAWF_MAX_TIDS) {
			/* update tid report */
			sawf_ctx->tid_reports[tid].svc_class_id = svc_class_id;
		}
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_HTT);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_htt_sawf_dynamic_ast_update(struct htt_soc *soc, uint32_t *msg_word,
			       qdf_nbuf_t htt_t2h_msg)
{
	uint16_t peer_id;
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	struct dp_peer *primary_link_peer = NULL;
	uint16_t *dynamic_ast_idx;

	peer_id = HTT_PEER_AST_OVERRIDE_SW_PEER_ID_GET(*msg_word);

	peer = dp_peer_get_ref_by_id(soc->dp_soc, peer_id,
				     DP_MOD_ID_SAWF);
	if (!peer) {
		dp_sawf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (IS_MLO_DP_MLD_PEER(peer)) {
		primary_link_peer =
		dp_get_primary_link_peer_by_id(soc->dp_soc,
					       peer->peer_id, DP_MOD_ID_SAWF);

		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			dp_sawf_err("Invalid peer");
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
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_err("Invalid SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	dynamic_ast_idx = sawf_ctx->dynamic_ast_idx;

	/* The dynamic AST index*/
	msg_word += 2;
	*dynamic_ast_idx = HTT_PEER_AST_OVERRIDE_AST_INDEX1_GET(*msg_word);
	dynamic_ast_idx++;

	msg_word++;
	*dynamic_ast_idx = HTT_PEER_AST_OVERRIDE_AST_INDEX2_GET(*msg_word);

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_htt_sawf_msduq_map(struct htt_soc *soc, uint32_t *msg_word,
		      qdf_nbuf_t htt_t2h_msg)
{
	uint16_t peer_id;
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	uint8_t tid_queue;
	uint8_t host_queue, remapped_tid;
	struct dp_sawf_msduq *msduq;
	struct dp_sawf_msduq_tid_map *msduq_map;
	uint8_t host_tid_queue;
	uint8_t msduq_index = 0;
	struct dp_peer *primary_link_peer = NULL;

	peer_id = HTT_T2H_SAWF_MSDUQ_INFO_HTT_PEER_ID_GET(*msg_word);
	tid_queue = HTT_T2H_SAWF_MSDUQ_INFO_HTT_QTYPE_GET(*msg_word);

	peer = dp_peer_get_ref_by_id(soc->dp_soc, peer_id,
				     DP_MOD_ID_SAWF);
	if (!peer) {
		qdf_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (IS_MLO_DP_MLD_PEER(peer)) {
		primary_link_peer = dp_get_primary_link_peer_by_id(soc->dp_soc,
					peer->peer_id, DP_MOD_ID_SAWF);
		if (!primary_link_peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
			qdf_err("Invalid peer");
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
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		qdf_err("Invalid SAWF ctx");
		return QDF_STATUS_E_FAILURE;
	}

	msg_word++;
	remapped_tid = HTT_T2H_SAWF_MSDUQ_INFO_HTT_REMAP_TID_GET(*msg_word);

	if (dp_htt_sawf_get_host_queue(soc->dp_soc, msg_word, &host_queue) ==
		QDF_STATUS_E_FAILURE) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		qdf_err("Invalid host queue");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_info("|TID Q:%d|Remapped TID:%d|Host Q:%d|",
		 tid_queue, remapped_tid, host_queue);

	host_tid_queue = tid_queue - DP_SAWF_DEFAULT_Q_PTID_MAX;

	if (remapped_tid < DP_SAWF_TID_MAX &&
	    host_tid_queue < DP_SAWF_DEFINED_Q_PTID_MAX) {
		msduq_map = &sawf_ctx->msduq_map[remapped_tid][host_tid_queue];
		msduq_map->host_queue_id = host_queue;
	}

	msduq_index = host_queue - DP_SAWF_DEFAULT_Q_MAX;

	if (msduq_index < DP_SAWF_Q_MAX) {
		msduq = &sawf_ctx->msduq[msduq_index];
		msduq->remapped_tid = remapped_tid;
		msduq->htt_msduq = host_tid_queue;
		telemetry_sawf_updt_tid_msduq(peer->sawf->telemetry_ctx,
					      msduq_index,
					      remapped_tid, host_tid_queue);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_SAWF_STATS
QDF_STATUS
dp_sawf_htt_h2t_mpdu_stats_req(struct htt_soc *soc,
			       uint8_t stats_type, uint8_t enable,
			       uint32_t config_param0,
			       uint32_t config_param1,
			       uint32_t config_param2,
			       uint32_t config_param3)
{
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	uint8_t *htt_logger_bufp;
	struct dp_htt_htc_pkt *pkt;
	QDF_STATUS status;

	dp_sawf_info("stats_type %u enable %u", stats_type, enable);

	htt_msg = qdf_nbuf_alloc(
			soc->osdev,
			HTT_MSG_BUF_SIZE(HTT_H2T_STREAMING_STATS_REQ_MSG_SZ),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);

	if (!htt_msg) {
		dp_sawf_err("Fail to allocate htt msg buffer");
		return QDF_STATUS_E_NOMEM;
	}

	if (!qdf_nbuf_put_tail(htt_msg,
			       HTT_H2T_STREAMING_STATS_REQ_MSG_SZ)) {
		dp_sawf_err("Fail to expand head");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_FAILURE;
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;

	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ);
	HTT_H2T_STREAMING_STATS_REQ_STATS_TYPE_SET(*msg_word, stats_type);
	HTT_H2T_STREAMING_STATS_REQ_ENABLE_SET(*msg_word, enable);

	*++msg_word = config_param0;
	*++msg_word = config_param1;
	*++msg_word = config_param2;
	*++msg_word = config_param3;

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		dp_sawf_err("Fail to allocate dp_htt_htc_pkt buffer");
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL; /* not used during send-done callback */

	/* macro to set packet parameters for TX */
	SET_HTC_PACKET_INFO_TX(
			&pkt->htc_pkt,
			dp_htt_h2t_send_complete_free_netbuf,
			qdf_nbuf_data(htt_msg),
			qdf_nbuf_len(htt_msg),
			soc->htc_endpoint,
			HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, htt_msg);

	status = DP_HTT_SEND_HTC_PKT(
			soc, pkt,
			HTT_H2T_MSG_TYPE_STREAMING_STATS_REQ,
			htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

/*
 * dp_sawf_htt_gen_mpdus_details_tlv() - Parse MPDU TLV and update stats
 * @soc: SOC handle
 * @tlv_buf: Pointer to TLV buffer
 *
 * @Return: void
 */
static void dp_sawf_htt_gen_mpdus_tlv(struct dp_soc *soc, uint8_t *tlv_buf)
{
	htt_stats_strm_gen_mpdus_tlv_t *tlv;
	uint8_t remapped_tid, host_tid_queue;

	tlv = (htt_stats_strm_gen_mpdus_tlv_t *)tlv_buf;

	dp_sawf_debug("queue_id: peer_id %u tid %u htt_qtype %u|"
			"svc_intvl: success %u fail %u|"
			"burst_size: success %u fail %u",
			tlv->queue_id.peer_id,
			tlv->queue_id.tid,
			tlv->queue_id.htt_qtype,
			tlv->svc_interval.success,
			tlv->svc_interval.fail,
			tlv->burst_size.success,
			tlv->burst_size.fail);

	remapped_tid = tlv->queue_id.tid;
	host_tid_queue = tlv->queue_id.htt_qtype - DP_SAWF_DEFAULT_Q_PTID_MAX;

	if (remapped_tid > DP_SAWF_TID_MAX - 1 ||
	    host_tid_queue > DP_SAWF_DEFINED_Q_PTID_MAX - 1) {
		dp_sawf_err("Invalid tid (%u) or msduq idx (%u)",
			    remapped_tid, host_tid_queue);
		return;
	}

	dp_sawf_update_mpdu_basic_stats(soc,
					tlv->queue_id.peer_id,
					remapped_tid, host_tid_queue,
					tlv->svc_interval.success,
					tlv->svc_interval.fail,
					tlv->burst_size.success,
					tlv->burst_size.fail);
}

/*
 * dp_sawf_htt_gen_mpdus_details_tlv() - Parse MPDU Details TLV
 * @soc: SOC handle
 * @tlv_buf: Pointer to TLV buffer
 *
 * @Return: void
 */
static void dp_sawf_htt_gen_mpdus_details_tlv(struct dp_soc *soc,
					      uint8_t *tlv_buf)
{
	htt_stats_strm_gen_mpdus_details_tlv_t *tlv =
		(htt_stats_strm_gen_mpdus_details_tlv_t *)tlv_buf;

	dp_sawf_debug("queue_id: peer_id %u tid %u htt_qtype %u|"
			"svc_intvl: ts_prior %ums ts_now %ums "
			"intvl_spec %ums margin %ums|"
			"burst_size: consumed_bytes_orig %u "
			"consumed_bytes_final %u remaining_bytes %u "
			"burst_size_spec %u margin_bytes %u",
			tlv->queue_id.peer_id,
			tlv->queue_id.tid,
			tlv->queue_id.htt_qtype,
			tlv->svc_interval.timestamp_prior_ms,
			tlv->svc_interval.timestamp_now_ms,
			tlv->svc_interval.interval_spec_ms,
			tlv->svc_interval.margin_ms,
			tlv->burst_size.consumed_bytes_orig,
			tlv->burst_size.consumed_bytes_final,
			tlv->burst_size.remaining_bytes,
			tlv->burst_size.burst_size_spec,
			tlv->burst_size.margin_bytes);
}

QDF_STATUS
dp_sawf_htt_mpdu_stats_handler(struct htt_soc *soc,
			       qdf_nbuf_t htt_t2h_msg)
{
	uint32_t length;
	uint32_t *msg_word;
	uint8_t *tlv_buf;
	uint8_t tlv_type;
	uint8_t tlv_length;

	msg_word = (uint32_t *)qdf_nbuf_data(htt_t2h_msg);
	length = qdf_nbuf_len(htt_t2h_msg);

	msg_word++;

	if (length > HTT_T2H_STREAMING_STATS_IND_HDR_SIZE)
		length -= HTT_T2H_STREAMING_STATS_IND_HDR_SIZE;
	else
		return QDF_STATUS_E_FAILURE;

	while (length > 0) {
		tlv_buf = (uint8_t *)msg_word;
		tlv_type = HTT_STATS_TLV_TAG_GET(*msg_word);
		tlv_length = HTT_STATS_TLV_LENGTH_GET(*msg_word);

		if (!tlv_length)
			break;

		if (tlv_type == HTT_STATS_STRM_GEN_MPDUS_TAG)
			dp_sawf_htt_gen_mpdus_tlv(soc->dp_soc, tlv_buf);
		else if (tlv_type == HTT_STATS_STRM_GEN_MPDUS_DETAILS_TAG)
			dp_sawf_htt_gen_mpdus_details_tlv(soc->dp_soc, tlv_buf);

		msg_word = (uint32_t *)(tlv_buf + tlv_length);
		length -= tlv_length;
	}

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS
dp_sawf_htt_h2t_mpdu_stats_req(struct htt_soc *soc,
			       uint8_t stats_type, uint8_t enable,
			       uint32_t config_param0,
			       uint32_t config_param1,
			       uint32_t config_param2,
			       uint32_t config_param3)
{
	return QDF_STATUS_E_FAILURE;
}

static void dp_sawf_htt_gen_mpdus_tlv(struct dp_soc *soc, uint8_t *tlv_buf)
{
}

static void dp_sawf_htt_gen_mpdus_details_tlv(struct dp_soc *soc,
					      uint8_t *tlv_buf)
{
}

QDF_STATUS
dp_sawf_htt_mpdu_stats_handler(struct htt_soc *soc,
			       qdf_nbuf_t htt_t2h_msg)
{
	return QDF_STATUS_E_FAILURE;
}
#endif
