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
#include <dp_sawf_htt.h>
#include "cdp_txrx_cmn_reg.h"
#include <wlan_telemetry_agent.h>
#include <wlan_sawf.h>

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

	htt_msg = qdf_nbuf_alloc_no_recycler(
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_MAP_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4);

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

static QDF_STATUS
dp_htt_sawf_msduq_recfg_req_send(struct htt_soc *soc,
				 struct dp_sawf_msduq *msduq, uint16_t peer_id,
				 uint8_t q_id, HTT_MSDUQ_DEACTIVATE_E q_ind)
{
	qdf_nbuf_t htt_msg;
	uint32_t *msg_word;
	uint8_t *htt_logger_bufp;
	uint8_t req_cookie;
	uint16_t htt_size;
	struct dp_htt_htc_pkt *pkt;
	QDF_STATUS status;

	htt_size = sizeof(struct htt_h2t_sdwf_msduq_recfg_req);
	htt_msg = qdf_nbuf_alloc_no_recycler(
			HTT_MSG_BUF_SIZE(htt_size),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4);

	if (!htt_msg) {
		dp_htt_err("peer:%d | Fail to allocate htt msg buffer",
			   peer_id);
		return QDF_STATUS_E_NOMEM;
	}

	if (!qdf_nbuf_put_tail(htt_msg, htt_size)) {
		dp_htt_err("peer:%d | Fail to expand head for HTT_H2T_MSG_TYPE_SDWF_MSDUQ_RECFG_REQ",
			   peer_id);
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_FAILURE;
	}

	msg_word = (uint32_t *)qdf_nbuf_data(htt_msg);

	qdf_nbuf_push_head(htt_msg, HTC_HDR_ALIGNMENT_PADDING);

	/* word 0 */
	htt_logger_bufp = (uint8_t *)msg_word;
	*msg_word = 0;

	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_SDWF_MSDUQ_RECFG_REQ);
	HTT_H2T_SDWF_MSDUQ_RECFG_REQ_TGT_OPAQUE_MSDUQ_ID_SET(*msg_word,
							     msduq->tgt_opaque_id);

	/* word 1 */
	msg_word++;
	*msg_word = 0;

	HTT_H2T_SDWF_MSDUQ_RECFG_REQ_SVC_CLASS_ID_SET(*msg_word,
						      msduq->svc_id - 1);
	HTT_H2T_SDWF_MSDUQ_RECFG_REQ_DEACTIVATE_SET(*msg_word, q_ind);

	req_cookie = DP_SAWF_MSDUQ_COOKIE_CREATE(q_id, q_ind);
	dp_sawf_info("peer:%d | tgt_opaque_id: 0x%x | svc_id: %d | q_ind: %d | req_cookie: 0x%x",
		     peer_id, msduq->tgt_opaque_id, msduq->svc_id, q_ind,
		     req_cookie);
	HTT_H2T_SDWF_MSDUQ_RECFG_REQUEST_COOKIE_SET(*msg_word, req_cookie);

	pkt = htt_htc_pkt_alloc(soc);
	if (!pkt) {
		dp_htt_err("peer:%d | Fail to allocate dp_htt_htc_pkt buffer",
			   peer_id);
		qdf_nbuf_free(htt_msg);
		return QDF_STATUS_E_NOMEM;
	}

	pkt->soc_ctxt = NULL;

	/* Clear no_resp_ind bit */
	msduq->no_resp_ind = 0;
	dp_sawf_debug("peer: %d | Reset no_resp_ind Flag", peer_id);

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
			HTT_H2T_MSG_TYPE_SDWF_MSDUQ_RECFG_REQ, htt_logger_bufp);

	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(htt_msg);
		htt_htc_pkt_free(soc, pkt);
	}

	return status;
}

QDF_STATUS
dp_htt_sawf_msduq_recfg_req(struct htt_soc *soc, struct dp_peer *peer,
			    struct dp_sawf_msduq *msduq, uint8_t q_id,
			    HTT_MSDUQ_DEACTIVATE_E q_ind)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (IS_DP_LEGACY_PEER(peer)) {
		status = dp_htt_sawf_msduq_recfg_req_send(soc, msduq,
							  peer->peer_id, q_id,
							  q_ind);
	} else {
#ifdef WLAN_FEATURE_11BE
		struct dp_soc *dpsoc;
		uint8_t cnt = 0;
		bool htt_sent_for_soc[GLOBAL_SOC_SIZE] = {false};
		struct dp_peer *link_peer, *mld_peer;
		struct dp_mld_link_peers link_peers_info = {NULL};

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

		dp_get_link_peers_ref_from_mld_peer(soc->dp_soc, mld_peer,
						    &link_peers_info,
						    DP_MOD_ID_SAWF);
		/*
		 * Loop through all the link peers and send HTT command to all
		 * the link peer soc
		 */
		for (cnt = 0; cnt < link_peers_info.num_links; cnt++) {
			link_peer = link_peers_info.link_peers[cnt];
			if (!link_peer)
				continue;

			dpsoc = link_peer->vdev->pdev->soc;

			/*
			 * In case of split-phy chips when both radio vaps are
			 * under same mld this loop will try to send HTT twice
			 * on same SOC.
			 * Locally storing the sent record to avoid such
			 * back-to-back HTT send commands.
			 */
			if (htt_sent_for_soc[dp_get_chip_id(dpsoc)]) {
				dp_sawf_debug("htt recfg_req for peer: %d soc:%d "
					      "svc_id:%d tgt_opaque_id: 0x%x "
					      "sent already. "
					      "Hence dropping here !",
					      peer->peer_id,
					      dp_get_chip_id(dpsoc),
					      msduq->svc_id,
					      msduq->tgt_opaque_id);
				continue;
			}

			htt_sent_for_soc[dp_get_chip_id(dpsoc)] = true;

			dp_sawf_debug("htt recfg_req send for MLO soc:%d peer:%d",
				      dp_get_chip_id(dpsoc), peer->peer_id);
			status = dp_htt_sawf_msduq_recfg_req_send(dpsoc->htt_handle,
								  msduq,
								  peer->peer_id,
								  q_id, q_ind);
			if (status != QDF_STATUS_SUCCESS) {
				dp_sawf_err("recfg_req send failed for soc:%d peer:%d",
					    dp_get_chip_id(dpsoc),
					    peer->peer_id);
				break;
			}
		}
		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_SAWF);
#endif
	}

	return status;
}

QDF_STATUS
dp_htt_sawf_msduq_recfg_ind(struct htt_soc *soc, uint32_t *msg_word)
{
	struct dp_peer *peer;
	struct dp_peer_sawf *sawf_ctx;
	uint16_t peer_id;
	uint32_t req_cookie;
	uint8_t htt_qtype, err_code;
	uint8_t q_id, q_ind, new_q_state, curr_q_state;
	bool is_success;
	struct dp_sawf_msduq *msduq;
	uint32_t svc_id;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	/* word 0 */
	htt_qtype = HTT_T2H_MSG_TYPE_SDWF_MSDUQ_CFG_IND_HTT_QTYPE_GET(*msg_word);
	peer_id = HTT_T2H_MSG_TYPE_SDWF_MSDUQ_CFG_IND_PEER_ID_GET(*msg_word);

	/*
	 * In MLO case, recfg indication will always come in primary link
	 * with primary link peer id.
	 */
	peer = dp_peer_get_ref_by_id(soc->dp_soc, peer_id, DP_MOD_ID_SAWF);

	dp_sawf_debug("HTT resp received for peer:%d soc:%d",
		      peer_id, dp_get_chip_id(soc->dp_soc));

	if (!peer) {
		dp_sawf_err("Invalid peer:%d", peer_id);
		return status;
	}

	if (!dp_peer_is_primary_link_peer(peer)) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_err("Non primary link peer, soc_id:%d, peer:%d",
			    dp_get_chip_id(soc->dp_soc), peer->peer_id);
		return QDF_STATUS_E_FAILURE;
	}

	sawf_ctx = dp_peer_sawf_ctx_get(peer);
	if (!sawf_ctx) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_err("Invalid SAWF ctx");
		return status;
	}

	/* word 1 */
	msg_word++;
	err_code = HTT_T2H_MSG_TYPE_SDWF_MSDUQ_CFG_IND_ERROR_CODE_GET(*msg_word);

	/* word 2 */
	msg_word++;
	req_cookie = HTT_T2H_MSG_TYPE_SDWF_MSDUQ_CFG_IND_REQUEST_COOKIE_GET(*msg_word);
	q_id = DP_SAWF_GET_MSDUQ_ID_FROM_COOKIE(req_cookie);
	q_ind = DP_SAWF_GET_MSDUQ_INDICATION_FROM_COOKIE(req_cookie);

	if (q_id >= DP_SAWF_Q_MAX) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_err("Invalid q_id:%d", q_id);
		return status;
	}

	msduq = &sawf_ctx->msduq[q_id];
	if (!msduq) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		dp_sawf_err("Invalid SAWF ctx");
		return status;
	}

	svc_id = msduq->svc_id;
	dp_sawf_info("peer:%d| svc_class_id: %d| err_code: 0x%x| tgt_opaque_id: 0x%x| req_cookie: 0x%x",
		     peer_id, svc_id, err_code, msduq->tgt_opaque_id,
		     req_cookie);

	qdf_spin_lock_bh(&sawf_ctx->sawf_peer_lock);

	/* Clear no_resp_ind bit */
	msduq->no_resp_ind = 0;
	dp_sawf_debug("peer:%d | Reset no_resp_ind Flag", peer_id);

	curr_q_state = msduq->q_state;

	/*
	 * Q Indication(q_ind) needs to correlate with q_state of MSDUQ.
	 * If not, the resp needs to be dropped.
	 */
	if ((q_ind == HTT_MSDUQ_DEACTIVATE && curr_q_state != SAWF_MSDUQ_DEACTIVATE_PENDING) ||
	    (q_ind == HTT_MSDUQ_REACTIVATE && curr_q_state != SAWF_MSDUQ_REACTIVATE_PENDING)) {
		dp_sawf_err("peer:%d | Resp dropped for q_ind:%d msduq->q_state:%s",
			    peer_id, q_ind,
			    dp_sawf_msduq_state_to_string(curr_q_state));
		qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return status;
	}

	is_success = (err_code == HTT_SDWF_MSDUQ_CFG_IND_ERROR_NONE) ? 1 : 0;

	switch (curr_q_state) {
	case SAWF_MSDUQ_DEACTIVATE_PENDING:
		new_q_state = is_success ? SAWF_MSDUQ_DEACTIVATED : SAWF_MSDUQ_IN_USE;
		if (is_success) {
			wlan_service_id_dec_msduq_count(svc_id);
			dp_sawf_debug("Service class ID:%d, ref_count:%d, "
				      "msduq_count:%d", svc_id,
				      wlan_service_id_get_ref_count(svc_id),
				      wlan_service_id_get_msduq_count(svc_id));

			if (!wlan_service_id_is_used(svc_id)) {
				if (soc->dp_soc->cdp_soc.ol_ops->disable_sawf_svc)
				    soc->dp_soc->cdp_soc.ol_ops->disable_sawf_svc(svc_id);

				dp_sawf_debug("Service class ID:%d is disabled",
					      svc_id);
				telemetry_sawf_set_svclass_cfg(false, svc_id, 0,
							       0, 0, 0, 0, 0, 0);
				wlan_disable_service_class(svc_id);
			}
		} else {
			DP_SAWF_MSDUQ_STATS_INC(deactivate_stats, recv_failure);
		}
		break;
	case SAWF_MSDUQ_REACTIVATE_PENDING:
		new_q_state = is_success ? SAWF_MSDUQ_IN_USE : SAWF_MSDUQ_DEACTIVATED;
		if (is_success) {
			wlan_service_id_inc_msduq_count(svc_id);
			dp_sawf_debug("Service class ID:%d, ref_count:%d, "
				      "msduq_count:%d", svc_id,
				      wlan_service_id_get_ref_count(svc_id),
				      wlan_service_id_get_msduq_count(svc_id));
		} else {
			DP_SAWF_MSDUQ_STATS_INC(reactivate_stats, recv_failure);
		}
		break;
	default:
		dp_sawf_err("peer:%d | Invalid q_state:%d", peer_id,
			    curr_q_state);
		qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		return status;
	}

	msduq->q_state = new_q_state;

	qdf_spin_unlock_bh(&sawf_ctx->sawf_peer_lock);
	dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);

	dp_sawf_info("peer:%d | msduq:%d | %s -> %s", peer_id, q_id,
		     dp_sawf_msduq_state_to_string(curr_q_state),
		     dp_sawf_msduq_state_to_string(new_q_state));

	if (!is_success) {
		status = QDF_STATUS_E_FAILURE;
		dp_sawf_err("Resp Failed for peer:%d | q_id:%d | svc_id:%d | q_state:%s with error code: 0x%x ",
			    peer_id, q_id, svc_id,
			    dp_sawf_msduq_state_to_string(curr_q_state),
			    err_code);
		if (new_q_state == SAWF_MSDUQ_DEACTIVATED) {
			/* Notify NM Manager about Reactivate failure */
			if (dp_sawf_notify_deactivate_msduq(soc->dp_soc, peer,
							    q_id, svc_id) != QDF_STATUS_SUCCESS)
				dp_sawf_err("peer:%d | NOTIFY REACTIVATE FAILURE to NW Manager failed",
					    peer_id);
		}
	} else {
		status = QDF_STATUS_SUCCESS;
		dp_sawf_debug("Resp Success for peer:%d | q_id:%d | svc_id:%d",
			      peer_id, q_id, svc_id);

		if (new_q_state == SAWF_MSDUQ_IN_USE)
			telemetry_sawf_update_msduq_info(peer->sawf->telemetry_ctx,
							 q_id, msduq->remapped_tid,
							 msduq->htt_msduq, svc_id);
		else
			telemetry_sawf_clear_msduq_info(peer->sawf->telemetry_ctx,
							q_id);
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

	htt_msg = qdf_nbuf_alloc_no_recycler(
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_UNMAP_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4);

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

	htt_msg = qdf_nbuf_alloc_no_recycler(
			HTT_MSG_BUF_SIZE(HTT_SAWF_DEF_QUEUES_MAP_REPORT_REQ_BYTES),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4);

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
	uint32_t tgt_opaque_id;
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

	/* dword2 */
	msg_word++;
	remapped_tid = HTT_T2H_SAWF_MSDUQ_INFO_HTT_REMAP_TID_GET(*msg_word);

	if (dp_htt_sawf_get_host_queue(soc->dp_soc, msg_word, &host_queue) ==
		QDF_STATUS_E_FAILURE) {
		dp_peer_unref_delete(peer, DP_MOD_ID_SAWF);
		qdf_err("Invalid host queue");
		return QDF_STATUS_E_FAILURE;
	}

	/* dword3 */
	msg_word++;
	tgt_opaque_id = HTT_T2H_SAWF_MSDUQ_INFO_HTT_TGT_OPAQUE_ID_GET(*msg_word);

	qdf_info("peer:%d|TID Q:%d|Remapped TID:%d|Host Q:%d|tgt_opaque_id: 0x%x |",
		 peer_id, tid_queue, remapped_tid, host_queue, tgt_opaque_id);

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
		msduq->tgt_opaque_id = tgt_opaque_id;
		msduq->map_done = 1;
		telemetry_sawf_update_msduq_info(peer->sawf->telemetry_ctx,
						 msduq_index,
						 remapped_tid, host_tid_queue,
						 msduq->svc_id);
	}

	wlan_service_id_inc_msduq_count(msduq->svc_id);
	dp_sawf_debug("Service class ID:%d, ref_count:%d, msduq_count:%d",
		      msduq->svc_id,
		      wlan_service_id_get_ref_count(msduq->svc_id),
		      wlan_service_id_get_msduq_count(msduq->svc_id));

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

	htt_msg = qdf_nbuf_alloc_no_recycler(
			HTT_MSG_BUF_SIZE(HTT_H2T_STREAMING_STATS_REQ_MSG_SZ),
			HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4);

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
