/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <qdf_status.h>
#include <qdf_types.h>
#include <osdep_adf.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_global_obj.h>
#include "wlan_telemetry_agent.h"
#include <cfg80211_ven_cmd.h>
#include <wlan_mlme_if.h>
#include <wlan_sawf.h>

struct telemetry_agent_ops *g_agent_ops;
qdf_export_symbol(g_agent_ops);

void wlan_telemetry_agent_application_init_notify(
		enum agent_notification_event event)
{
	if (g_agent_ops)
		g_agent_ops->agent_notify_app_event(event);
}

void wlan_telemetry_emesh_application_init_deinit_notify(
		enum agent_notification_event event)
{
	if (g_agent_ops)
		g_agent_ops->agent_notify_emesh_event(event);
}

qdf_export_symbol(wlan_telemetry_emesh_application_init_deinit_notify);

static QDF_STATUS
telemetry_agent_psoc_create_handler(struct wlan_objmgr_psoc *psoc,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct agent_psoc_obj psoc_obj = {0};

	psoc_obj.psoc_id = wlan_psoc_get_id(psoc);
	psoc_obj.psoc_back_pointer = psoc;

	if (g_agent_ops)
		g_agent_ops->agent_psoc_create_handler(psoc, &psoc_obj);
	return status;
}

static QDF_STATUS
telemetry_agent_pdev_create_handler(struct wlan_objmgr_pdev *pdev,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc;
	struct agent_pdev_obj pdev_obj = {0};

	psoc = wlan_pdev_get_psoc(pdev);

	pdev_obj.pdev_back_pointer = pdev;
	pdev_obj.psoc_back_pointer = psoc;
	pdev_obj.psoc_id = wlan_psoc_get_id(psoc);
	pdev_obj.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if (g_agent_ops)
		g_agent_ops->agent_pdev_create_handler(pdev, &pdev_obj);

	return status;
}

static QDF_STATUS
telemetry_agent_peer_create_handler(struct wlan_objmgr_peer *peer,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct agent_peer_obj peer_obj = {0};

	struct wlan_objmgr_psoc *psoc =  wlan_peer_get_psoc(peer);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	if (wlan_peer_get_peer_type(peer) == WLAN_PEER_AP ||
	    wlan_peer_get_peer_type(peer) == WLAN_PEER_STA_TEMP ||
	    wlan_peer_get_peer_type(peer) == WLAN_PEER_MLO_TEMP) {
		return status;
	}

	peer_obj.peer_back_pointer = peer;
	peer_obj.pdev_back_pointer = pdev;
	peer_obj.psoc_back_pointer = psoc;

	peer_obj.psoc_id = wlan_psoc_get_id(psoc);
	peer_obj.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	WLAN_ADDR_COPY(&peer_obj.peer_mac_addr, peer->macaddr);

	if (g_agent_ops)
		g_agent_ops->agent_peer_create_handler(peer, &peer_obj);

	return status;
}


static QDF_STATUS
telemetry_agent_psoc_delete_handler(struct wlan_objmgr_psoc *psoc,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct agent_psoc_obj psoc_obj = {0};

	psoc_obj.psoc_id = wlan_psoc_get_id(psoc);
	psoc_obj.psoc_back_pointer = psoc;

	if (g_agent_ops)
		g_agent_ops->agent_psoc_destroy_handler(psoc, &psoc_obj);

	return status;
}

static QDF_STATUS
telemetry_agent_pdev_delete_handler(struct wlan_objmgr_pdev *pdev,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc;
	struct agent_pdev_obj pdev_obj = {0};

	psoc = wlan_pdev_get_psoc(pdev);

	pdev_obj.pdev_back_pointer = pdev;
	pdev_obj.psoc_back_pointer = psoc;
	pdev_obj.psoc_id = wlan_psoc_get_id(psoc);
	pdev_obj.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if (g_agent_ops)
		g_agent_ops->agent_pdev_destroy_handler(pdev, &pdev_obj);

	return status;
}

static QDF_STATUS
telemetry_agent_peer_delete_handler(struct wlan_objmgr_peer *peer,
				    void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct agent_peer_obj peer_obj = {0};

	struct wlan_objmgr_psoc *psoc =  wlan_peer_get_psoc(peer);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	if (wlan_peer_get_peer_type(peer) == WLAN_PEER_AP ||
	    wlan_peer_get_peer_type(peer) == WLAN_PEER_STA_TEMP ||
	    wlan_peer_get_peer_type(peer) == WLAN_PEER_MLO_TEMP) {
		return status;
	}

	peer_obj.peer_back_pointer = peer;
	peer_obj.pdev_back_pointer = pdev;
	peer_obj.psoc_back_pointer = psoc;

	peer_obj.psoc_id = wlan_psoc_get_id(psoc);
	peer_obj.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	WLAN_ADDR_COPY(&peer_obj.peer_mac_addr, peer->macaddr);

	if (g_agent_ops)
		g_agent_ops->agent_peer_destroy_handler(peer, &peer_obj);

	return status;
}

static int telemetry_agent_convert_command_to_agent_command(int command)
{
	int cmd = AGENT_INVALID_PARAM;

	switch (command) {
	case IEEE80211_PARAM_TELEMETRY_AGENT_DEBUG_LEVEL:
		cmd = AGENT_SET_DEBUG_LEVEL;
		break;
	default:
		cmd = AGENT_INVALID_PARAM;
		break;
	}

	return cmd;
}

int telemetry_agent_set_param(int command, int value)
{
	int cmd = 0;

	cmd = telemetry_agent_convert_command_to_agent_command(command);
	if (cmd == AGENT_INVALID_PARAM) {
		qdf_err("invalid command %d", command);
		return -1;
	}

	if (g_agent_ops)
		return g_agent_ops->agent_set_param(cmd, value);

	return 0;
}

qdf_export_symbol(telemetry_agent_set_param);

void *telemetry_sawf_peer_ctx_alloc(void *soc, void *sawf_ctx,
				    uint8_t *mac_addr,
				    uint8_t svc_id, uint8_t hostq_id)
{
	if (g_agent_ops)
		return g_agent_ops->sawf_alloc_peer(soc, sawf_ctx,
						    mac_addr,
						    svc_id,
						    hostq_id);

	return NULL;
}

qdf_export_symbol(telemetry_sawf_peer_ctx_alloc);

void telemetry_sawf_peer_ctx_free(void *telemetry_ctx)
{
	if (g_agent_ops)
		g_agent_ops->sawf_free_peer(telemetry_ctx);
}

qdf_export_symbol(telemetry_sawf_peer_ctx_free);

QDF_STATUS telemetry_sawf_updt_tid_msduq(void *telemetry_ctx,
					 uint8_t hostq_id,
					 uint8_t tid, uint8_t msduq_idx)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_updt_queue_info(telemetry_ctx,
						      hostq_id, tid,
						      msduq_idx))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(telemetry_sawf_updt_tid_msduq);

QDF_STATUS telemetry_sawf_set_mov_avg_params(uint32_t num_pkt,
					     uint32_t num_win)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_updt_delay_mvng(num_pkt, num_win))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_set_mov_avg_params);

QDF_STATUS telemetry_sawf_set_sla_params(uint32_t num_pkt,
					 uint32_t time_sec)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_updt_sla_params(num_pkt, time_sec))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_set_sla_params);

QDF_STATUS telemetry_sawf_set_sla_cfg(uint8_t svc_id,
				      uint8_t min_tput_rate,
				      uint8_t max_tput_rate,
				      uint8_t burst_size,
				      uint8_t svc_interval,
				      uint8_t delay_bound,
				      uint8_t msdu_ttl,
				      uint8_t msdu_rate_loss)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_set_sla_cfg(svc_id,
						  min_tput_rate,
						  max_tput_rate,
						  burst_size,
						  svc_interval,
						  delay_bound,
						  msdu_ttl,
						  msdu_rate_loss))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_set_sla_cfg);

QDF_STATUS telemetry_sawf_set_svclass_cfg(bool enable, uint8_t svc_id,
					  uint32_t min_tput_rate,
					  uint32_t max_tput_rate,
					  uint32_t burst_size,
					  uint32_t svc_interval,
					  uint32_t delay_bound,
					  uint32_t msdu_ttl,
					  uint32_t msdu_rate_loss)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_set_svclass_cfg(enable, svc_id,
						      min_tput_rate,
						      max_tput_rate,
						      burst_size,
						      svc_interval,
						      delay_bound,
						      msdu_ttl,
						      msdu_rate_loss))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_set_svclass_cfg);

QDF_STATUS telemetry_sawf_set_sla_detect_cfg(uint8_t detect_type,
					     uint8_t min_tput_rate,
					     uint8_t max_tput_rate,
					     uint8_t burst_size,
					     uint8_t svc_interval,
					     uint8_t delay_bound,
					     uint8_t msdu_ttl,
					     uint8_t msdu_rate_loss)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_set_sla_dtct_cfg(detect_type,
						       min_tput_rate,
						       max_tput_rate,
						       burst_size,
						       svc_interval,
						       delay_bound,
						       msdu_ttl,
						       msdu_rate_loss))
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_set_sla_detect_cfg);

QDF_STATUS telemetry_sawf_update_delay(void *telemetry_ctx, uint8_t tid,
				       uint8_t queue, uint64_t pass,
				       uint64_t fail)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_push_delay(telemetry_ctx, tid,
						 queue, pass, fail))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(telemetry_sawf_update_delay);

QDF_STATUS telemetry_sawf_update_delay_mvng(void *telemetry_ctx,
					    uint8_t tid, uint8_t queue,
					    uint64_t nwdelay_winavg,
					    uint64_t swdelay_winavg,
					    uint64_t hwdelay_winavg)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_push_delay_mvng(telemetry_ctx,
						      tid, queue,
						      nwdelay_winavg,
						      swdelay_winavg,
						      hwdelay_winavg))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(telemetry_sawf_update_delay_mvng);

QDF_STATUS telemetry_sawf_update_msdu_drop(void *telemetry_ctx,
					   uint8_t tid, uint8_t queue,
					   uint64_t success,
					   uint64_t failure_drop,
					   uint64_t failure_ttl)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_push_msdu_drop(telemetry_ctx, tid,
						     queue, success,
						     failure_drop,
						     failure_ttl))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_update_msdu_drop);

QDF_STATUS telemetry_sawf_get_rate(void *telemetry_ctx, uint8_t tid,
				   uint8_t queue, uint32_t *egress_rate,
				   uint32_t *ingress_rate)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_pull_rate(telemetry_ctx, tid, queue,
						egress_rate, ingress_rate))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_get_rate);

QDF_STATUS telemetry_sawf_get_mov_avg(void *telemetry_ctx, uint8_t tid,
				      uint8_t queue, uint32_t *nwdelay_avg,
				      uint32_t *swdelay_avg,
				      uint32_t *hwdelay_avg)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_pull_mov_avg(telemetry_ctx, tid,
						   queue, nwdelay_avg,
						   swdelay_avg, hwdelay_avg))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;

}

qdf_export_symbol(telemetry_sawf_get_mov_avg);

QDF_STATUS telemetry_sawf_reset_peer_stats(uint8_t *peer_mac)
{
	if (g_agent_ops) {
		if (g_agent_ops->sawf_reset_peer_stats(peer_mac))
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(telemetry_sawf_reset_peer_stats);

int register_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops)
{
	g_agent_ops = agent_ops;
	g_agent_ops->agent_get_psoc_info = wifi_driver_get_psoc_info;
	g_agent_ops->agent_get_pdev_info = wifi_driver_get_pdev_info;
	g_agent_ops->agent_get_peer_info = wifi_driver_get_peer_info;
	g_agent_ops->agent_get_psoc_stats = wifi_driver_get_psoc_stats;
	g_agent_ops->agent_get_pdev_stats = wifi_driver_get_pdev_stats;
	g_agent_ops->agent_get_peer_stats = wifi_driver_get_peer_stats;
	g_agent_ops->agent_get_emesh_pdev_stats = wifi_driver_get_emesh_pdev_stats;
	g_agent_ops->agent_get_emesh_peer_stats = wifi_driver_get_emesh_peer_stats;
	/* SAWF ops */
	g_agent_ops->sawf_get_tput_stats = wlan_sawf_get_tput_stats;
	g_agent_ops->sawf_get_mpdu_stats = wlan_sawf_get_mpdu_stats;
	g_agent_ops->sawf_get_drop_stats = wlan_sawf_get_drop_stats;
	g_agent_ops->sawf_notify_breach = wlan_sawf_notify_breach;

	qdf_info("Registered Telemetry Agent ops: %p", g_agent_ops);
	return QDF_STATUS_SUCCESS;
}

int
unregister_telemetry_agent_ops(struct telemetry_agent_ops *agent_ops)
{
	g_agent_ops = NULL;
	qdf_info("UnRegistered Telemetry Agent ops: %p", g_agent_ops);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_telemetry_agent_init(void)
{
	int status = QDF_STATUS_SUCCESS;

	if (wlan_objmgr_register_psoc_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto psoc_create_failed;
	}

	if (wlan_objmgr_register_pdev_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto pdev_create_failed;
	}

	if (wlan_objmgr_register_peer_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_peer_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto peer_create_failed;
	}

	if (wlan_objmgr_register_psoc_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto psoc_destroy_failed;
	}

	if (wlan_objmgr_register_pdev_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto pdev_destroy_failed;
	}

	if (wlan_objmgr_register_peer_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_peer_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto peer_destroy_failed;
	}

	return status;

peer_destroy_failed:
	if (wlan_objmgr_unregister_pdev_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
pdev_destroy_failed:
	if (wlan_objmgr_unregister_psoc_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
psoc_destroy_failed:
	if (wlan_objmgr_unregister_peer_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_peer_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

peer_create_failed:
	if (wlan_objmgr_unregister_pdev_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

pdev_create_failed:
	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

psoc_create_failed:

	return status;
}

QDF_STATUS wlan_telemetry_agent_deinit(void)
{
	int status = QDF_STATUS_SUCCESS;

	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_pdev_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_create_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_peer_create_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_psoc_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_psoc_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_pdev_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_pdev_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_destroy_handler(WLAN_COMP_TELEMETRY_AGENT,
				telemetry_agent_peer_delete_handler,
				NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}
