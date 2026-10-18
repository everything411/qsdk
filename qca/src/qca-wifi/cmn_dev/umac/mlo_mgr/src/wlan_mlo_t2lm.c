/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains T2LM APIs
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_mlo_mgr_cmn.h>
#include <qdf_util.h>
#include <wlan_cm_api.h>
#include "wlan_utility.h"
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE)
#include <wlan_t2lm_api.h>
#include <wlan_mlo_link_force.h>
#endif
#include <wlan_mlo_mgr_sta.h>
#include <wlan_sm_engine.h>
#include <wlan_mlo_mgr_ap.h>

#ifdef WLAN_FEATURE_11BE_MLO_TTLM

#define TTLM_REQUEST_TIMEOUT 5000

void ttlm_set_state(struct wlan_mlo_peer_context *ml_peer,
		    enum wlan_ttlm_sm_state state)
{
	if (state < WLAN_TTLM_S_MAX)
		ml_peer->ttlm_sm.ttlm_state = state;
	else
		t2lm_err("ML peer %d state (%d) is invalid",
			 ml_peer->mlo_peer_id, state);
}

void ttlm_set_substate(struct wlan_mlo_peer_context *ml_peer,
		       enum wlan_ttlm_sm_state substate)
{
	if (substate > WLAN_TTLM_S_MAX && substate < WLAN_TTLM_SS_MAX)
		ml_peer->ttlm_sm.ttlm_substate = substate;
	else
		t2lm_err("ML peer %d sub state (%d) is invalid",
			 ml_peer->mlo_peer_id, substate);
}

void ttlm_sm_state_update(struct wlan_mlo_peer_context *ml_peer,
			  enum wlan_ttlm_sm_state state,
			  enum wlan_ttlm_sm_state substate)
{
	if (!ml_peer)
		return;

	ttlm_set_state(ml_peer, state);
	ttlm_set_substate(ml_peer, substate);
}

void ttlm_lock_create(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_mutex_create(&ml_peer->ttlm_sm.ttlm_sm_lock);
}

void ttlm_timer_init(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_mc_timer_init(&ml_peer->ttlm_request_timer, QDF_TIMER_TYPE_SW,
			  ttlm_req_timeout_cb, ml_peer);
}

void ttlm_timer_deinit(struct wlan_mlo_peer_context *ml_peer)
{
	if (QDF_TIMER_STATE_RUNNING ==
		qdf_mc_timer_get_current_state(&ml_peer->ttlm_request_timer))
		qdf_mc_timer_stop(&ml_peer->ttlm_request_timer);

	qdf_mc_timer_destroy(&ml_peer->ttlm_request_timer);
}

void ttlm_req_timeout_cb(void *user_data)
{
	struct wlan_mlo_peer_context *ml_peer = user_data;

	t2lm_err("Failed to get the TTLM req response");
	ttlm_sm_deliver_event(ml_peer, WLAN_TTLM_SM_EV_TTLM_REQ_TIMEOUT,
			      0, NULL);
}

void ttlm_lock_destroy(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_mutex_destroy(&ml_peer->ttlm_sm.ttlm_sm_lock);
}

void ttlm_lock_acquire(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_mutex_acquire(&ml_peer->ttlm_sm.ttlm_sm_lock);
}

void ttlm_lock_release(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_mutex_release(&ml_peer->ttlm_sm.ttlm_sm_lock);
}

void ttlm_sm_transition_to(struct wlan_mlo_peer_context *ml_peer,
			   enum wlan_ttlm_sm_state state)
{
	wlan_sm_transition_to(ml_peer->ttlm_sm.sm_hdl, state);
}

QDF_STATUS ttlm_sm_deliver_event_sync(struct wlan_mlo_peer_context *ml_peer,
				      enum wlan_ttlm_sm_evt event,
				      uint16_t data_len, void *data)
{
	return wlan_sm_dispatch(ml_peer->ttlm_sm.sm_hdl, event, data_len, data);
}

static QDF_STATUS ttlm_tx_action_req_cb(struct scheduler_msg *msg)
{
	struct ttlm_req_params *ttlm_req = msg->bodyptr;
	QDF_STATUS status;
	uint8_t dir = WLAN_T2LM_BIDI_DIRECTION;

	status = ttlm_sm_deliver_event(ttlm_req->ml_peer,
				       WLAN_TTLM_SM_EV_TX_ACTION_REQ_ACTIVE,
				       sizeof(struct wlan_t2lm_info),
				       &ttlm_req->t2lm_info[dir]);

	qdf_mem_free(msg->bodyptr);
	return status;
}

static QDF_STATUS
wlan_ttlm_handle_tx_action_req(struct wlan_mlo_peer_context *ml_peer,
			       struct wlan_t2lm_info *ttlm_info)
{
	struct scheduler_msg ttlm_msg = {0};
	QDF_STATUS qdf_status;
	struct ttlm_req_params *ttlm_req_info;
	uint8_t i;
	uint8_t dir = WLAN_T2LM_BIDI_DIRECTION;

	ttlm_req_info = qdf_mem_malloc(sizeof(*ttlm_req_info));
	if (!ttlm_req_info)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(ttlm_req_info, sizeof(*ttlm_req_info));
	ttlm_req_info->ml_peer = ml_peer;
	for (i = 0; i < WLAN_T2LM_MAX_DIRECTION; i++)
		ttlm_req_info->t2lm_info[i].direction = WLAN_T2LM_INVALID_DIRECTION;

	qdf_mem_copy(&ttlm_req_info->t2lm_info[dir], ttlm_info,
		     sizeof(struct wlan_t2lm_info));
	ttlm_msg.bodyptr = ttlm_req_info;
	ttlm_msg.callback = ttlm_tx_action_req_cb;

	qdf_status = scheduler_post_message(
					QDF_MODULE_ID_OS_IF,
					QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_OS_IF,
					&ttlm_msg);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		mlo_err("Post TTLM STA Request msg fail");
		qdf_mem_free(ttlm_req_info);
	}

	return qdf_status;
}

static QDF_STATUS
ttlm_populate_tx_action_req(struct wlan_mlo_peer_context *ml_peer,
			    struct wlan_t2lm_info *ttlm_info,
			    uint8_t *dialog_token)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_peer_t2lm_policy *t2lm_policy;
	struct wlan_objmgr_peer *peer;
	struct wlan_t2lm_onging_negotiation_info t2lm_neg = {0};
	uint8_t dir = WLAN_T2LM_BIDI_DIRECTION;
	uint8_t i = 0;
	struct wlan_t2lm_info *ttlm_req_info;
	uint8_t tid_num;
	QDF_STATUS status;
	struct ml_link_force_state curr_force_state = {0};
	struct wlan_objmgr_psoc *psoc;

	if (!ml_peer) {
		t2lm_err("ML peer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);

	if (!vdev) {
		t2lm_err("VDEV is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("peer is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_vdev;
	}

	if (!vdev->mlo_dev_ctx) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_peer;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_peer;
	}

	ml_nlink_get_curr_force_state(psoc, vdev, &curr_force_state);
	t2lm_debug("Current force state force_inactive_bitmap: %d force_active_bitmap: %d curr_dynamic_inactive_bitmap: %d curr_active_bitmap: %d curr_inactive_bitmap: %d",
		   curr_force_state.force_inactive_bitmap,
		   curr_force_state.force_active_bitmap,
		   curr_force_state.curr_dynamic_inactive_bitmap,
		   curr_force_state.curr_active_bitmap,
		   curr_force_state.curr_inactive_bitmap);

	if (!ttlm_info->default_link_mapping &&
	    ttlm_info->ieee_link_map_tid[0] &&
	    ((ttlm_info->ieee_link_map_tid[0] &
	      curr_force_state.force_inactive_bitmap) ==
	    ttlm_info->ieee_link_map_tid[0])) {
		t2lm_err("TTLM req: 0x%x failed due to force_inactive link: 0x%x",
			 ttlm_info->ieee_link_map_tid[0],
			 curr_force_state.force_inactive_bitmap);
		status = QDF_STATUS_E_INVAL;
		goto release_peer;
	}

	t2lm_policy = &peer->mlo_peer_ctx->t2lm_policy;
	t2lm_neg = t2lm_policy->ongoing_tid_to_link_mapping;

	t2lm_neg.category = WLAN_T2LM_CATEGORY_REQUEST;
	t2lm_neg.dialog_token = t2lm_gen_dialog_token(t2lm_policy);
	*dialog_token = t2lm_neg.dialog_token;

	qdf_mem_zero(&t2lm_neg.t2lm_info,
		     sizeof(struct wlan_t2lm_info) * WLAN_T2LM_MAX_DIRECTION);

	for (i = 0; i < WLAN_T2LM_MAX_DIRECTION; i++)
		t2lm_neg.t2lm_info[i].direction = WLAN_T2LM_INVALID_DIRECTION;

	t2lm_neg.t2lm_info[dir].direction = WLAN_T2LM_BIDI_DIRECTION;
	if (ttlm_info->default_link_mapping) {
		t2lm_neg.t2lm_info[dir].default_link_mapping =
						ttlm_info->default_link_mapping;
		t2lm_debug("Default mapping: %d dialog_token: %d",
			   t2lm_neg.t2lm_info[dir].default_link_mapping,
			   t2lm_neg.dialog_token);
		goto deliver_event;
	}

	t2lm_neg.t2lm_info[dir].default_link_mapping = 0;
	t2lm_neg.t2lm_info[dir].mapping_switch_time_present = 0;
	t2lm_neg.t2lm_info[dir].expected_duration_present = 0;
	t2lm_neg.t2lm_info[dir].link_mapping_size = 1;

	t2lm_debug("dir %d dialog_token: %d", t2lm_neg.t2lm_info[dir].direction,
		   t2lm_neg.dialog_token);

	ttlm_req_info = &t2lm_neg.t2lm_info[dir];
	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		ttlm_req_info->ieee_link_map_tid[tid_num] =
				ttlm_info->ieee_link_map_tid[tid_num];
		t2lm_debug("TID[%d]: %d", tid_num,
			   ttlm_req_info->ieee_link_map_tid[tid_num]);
	}

deliver_event:
	status = t2lm_deliver_event(vdev, peer,
				    WLAN_T2LM_EV_ACTION_FRAME_TX_REQ,
				    &t2lm_neg,
				    0,
				    &t2lm_neg.dialog_token);
release_peer:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
release_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return status;
}

/**
 * ttlm_handle_ap_action_req() - Handle AP action req
 * @ml_peer: ML peer context
 * @t2lm_req: Ongoing T2LM req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
ttlm_handle_ap_action_req(struct wlan_mlo_peer_context *ml_peer,
			  struct wlan_t2lm_onging_negotiation_info *t2lm_req)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	QDF_STATUS status;

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);
	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_vdev;
	}

	status = ttlm_valid_n_copy_for_rx_req(vdev, peer, t2lm_req);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
release_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return status;
}

static QDF_STATUS
ttlm_send_ttlm_action_rsp(struct wlan_mlo_peer_context *ml_peer,
			  struct ttlm_rsp_info *t2lm_rsp_info)
{
	lim_send_ttlm_action_rsp_frame(t2lm_rsp_info->token,
				       t2lm_rsp_info->t2lm_resp_type,
				       (uint8_t *)&t2lm_rsp_info->dest_addr);
	return QDF_STATUS_SUCCESS;
}

/**
 * ttlm_state_init_entry() - Entry API for init state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to init state
 *
 * Return: void
 */
static void ttlm_state_init_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_state(ml_peer, WLAN_TTLM_S_INIT);
}

/**
 * ttlm_state_init_exit() - Exit API for init state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from init state
 *
 * Return: void
 */
static void ttlm_state_init_exit(void *ctx)
{
}

/**
 * ttlm_state_init_event() - Init State event handler for TTLM
 * @ctx: MLO peer context
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in INIT state
 *
 * Return: bool
 */
static bool ttlm_state_init_event(void *ctx, uint16_t event, uint16_t data_len,
				  void *data)
{
	bool event_handled = true;
	struct wlan_mlo_peer_context *ml_peer = ctx;
	QDF_STATUS status;

	switch (event) {
	case WLAN_TTLM_SM_EV_TX_ACTION_REQ:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer,
					   WLAN_TTLM_SM_EV_TX_ACTION_REQ_START,
					   data_len, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_RX_ACTION_REQ:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_BEACON:
		break;
	case WLAN_TTLM_SM_EV_BTM_LINK_DISABLE:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_state_inprogress_entry() - Entry API for INPROGRESS state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to inprogress state
 *
 * Return: void
 */
static void ttlm_state_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_state(ml_peer, WLAN_TTLM_S_INPROGRESS);
}

/**
 * ttlm_state_inprogress_exit() - Exit API for INPROGRESS state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from inprogress state
 *
 * Return: void
 */
static void ttlm_state_inprogress_exit(void *ctx)
{
}

/**
 * ttlm_state_inprogress_event() - INPROGRESS state event handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in INPROGRESS state
 *
 * Return: bool
 */
static bool ttlm_state_inprogress_event(void *ctx, uint16_t event,
					uint16_t data_len, void *data)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;
	bool event_handled = true;
	QDF_STATUS status;

	switch (event) {
	case WLAN_TTLM_SM_EV_TX_ACTION_REQ_START:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_SS_STA_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_RX_ACTION_REQ:
		ttlm_sm_transition_to(ml_peer,
				      WLAN_TTLM_SS_AP_ACTION_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_BTM_LINK_DISABLE:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_SS_AP_BTM_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_state_negotiated_entry() - Entry API for NEGOTIATED state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to negotiated state
 *
 * Return: void
 */
static void ttlm_state_negotiated_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_state(ml_peer, WLAN_TTLM_S_NEGOTIATED);
}

/**
 * ttlm_state_negotiated_exit() - Exit API for NEGOTIATED state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from negotiated state
 *
 * Return: void
 */
static void ttlm_state_negotiated_exit(void *ctx)
{
}

/**
 * ttlm_state_negotiated_event() - NEGOTIATED state event handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in NEGOTIATED state
 *
 * Return: true if event handle is SUCCESSFUL else false
 */
static bool ttlm_state_negotiated_event(void *ctx, uint16_t event,
					uint16_t data_len, void *data)
{
	bool event_handled = true;
	struct wlan_mlo_peer_context *ml_peer = ctx;
	QDF_STATUS status;

	switch (event) {
	case WLAN_TTLM_SM_EV_TX_ACTION_REQ:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer,
				WLAN_TTLM_SM_EV_TX_ACTION_REQ_START,
				data_len, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_RX_ACTION_REQ:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_BTM_LINK_DISABLE:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_INPROGRESS);
		status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len,
						    data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_subst_sta_inprogress_entry() - Entry API for STA INPROGRESS sub-state
 * for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to STA INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_sta_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_substate(ml_peer, WLAN_TTLM_SS_STA_INPROGRESS);
}

/**
 * ttlm_subst_sta_inprogress_exit() - Exit API for STA INPROGRESS sub-state for
 * TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from STA INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_sta_inprogress_exit(void *ctx)
{
}

static void ttlm_handle_status_ind(struct wlan_mlo_peer_context *ml_peer,
				   uint8_t dialog_token,
				   QDF_STATUS status)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *mlo_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	void *context = NULL;
	struct ttlm_comp_priv priv = {0};
	get_ttlm_send_ind_cb resp_cb = NULL;

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);
	if (!vdev) {
		t2lm_err("VDEV is null");
		return;
	}

	mlo_ctx = vdev->mlo_dev_ctx;
	if (!mlo_ctx) {
		mlo_err("null mlo_dev_ctx");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return;
	}

	sta_ctx = mlo_ctx->sta_ctx;
	if (!sta_ctx) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return;
	}

	mlo_dev_lock_acquire(mlo_ctx);

	resp_cb = sta_ctx->ttlm_send_info.ttlm_send_cmd_resp_cb;
	context = sta_ctx->ttlm_send_info.context;

	mlo_dev_lock_release(mlo_ctx);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	priv.ml_peer = ml_peer;
	priv.dialog_token = dialog_token;
	priv.status = status;

	if (resp_cb && context)
		resp_cb(&priv, context);
}

static QDF_STATUS
ttlm_handle_rx_action_rsp_in_sta_in_progress_state(
			struct wlan_mlo_peer_context *ml_peer,
			struct ttlm_rsp_info *t2lm_rsp_info)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct wlan_t2lm_info *t2lm_nego = NULL;
	QDF_STATUS status;

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);
	if (!vdev) {
		t2lm_err("VDEV is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("Peer is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (QDF_TIMER_STATE_RUNNING ==
		qdf_mc_timer_get_current_state(&ml_peer->ttlm_request_timer)) {
		status = qdf_mc_timer_stop(&ml_peer->ttlm_request_timer);
		if (QDF_IS_STATUS_ERROR(status)) {
			t2lm_err("Failed to stop the TTLM request timer");
			wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (t2lm_rsp_info->t2lm_resp_type == WLAN_T2LM_RESP_TYPE_SUCCESS) {
		wlan_t2lm_clear_peer_negotiation(peer);
		/* Apply T2LM config to peer T2LM ctx */
		t2lm_nego = &ml_peer->t2lm_policy.t2lm_negotiated_info.t2lm_info[WLAN_T2LM_BIDI_DIRECTION];
		qdf_mem_copy(t2lm_nego, t2lm_rsp_info->t2lm_info,
			     sizeof(struct wlan_t2lm_info));
		status =
			wlan_send_tid_to_link_mapping(vdev,
						      t2lm_rsp_info->t2lm_info);
		if (QDF_IS_STATUS_ERROR(status)) {
			t2lm_err("sending t2lm wmi failed");
			wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
			return QDF_STATUS_E_NULL_VALUE;
		}

		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);

		wlan_mlo_dev_t2lm_notify_link_update(vdev,
						     t2lm_rsp_info->t2lm_info);
	} else {
		t2lm_debug("T2LM rsp status: %d, clear ongoing tid mapping",
			   t2lm_rsp_info->t2lm_resp_type);
		wlan_t2lm_clear_ongoing_negotiation(peer);
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * ttlm_handle_timer_timeout() - Handle TTLM req timer timeout
 * @ml_peer: MLO peer context
 *
 * Return: void
 */
static void ttlm_handle_timer_timeout(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);
	if (!vdev) {
		t2lm_err("VDEV is null");
		return;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("Peer is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return;
	}

	wlan_t2lm_clear_ongoing_negotiation(peer);
	ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
}

/**
 * ttlm_subst_sta_inprogress_event() - STA INPROGRESS sub-state event handler
 * for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in STA INPROGRESS sub-state
 *
 * Return: bool
 */
static bool ttlm_subst_sta_inprogress_event(void *ctx, uint16_t event,
					    uint16_t data_len, void *data)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;
	bool event_handled = true;
	struct wlan_t2lm_info *ttlm_info = data;
	QDF_STATUS status, qdf_status;
	uint8_t dialog_token = 0;

	switch (event) {
	case WLAN_TTLM_SM_EV_TX_ACTION_REQ_START:
		status = wlan_ttlm_handle_tx_action_req(ml_peer, ttlm_info);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;

		break;
	case WLAN_TTLM_SM_EV_TX_ACTION_REQ_ACTIVE:
		status = ttlm_populate_tx_action_req(ml_peer, ttlm_info,
						     &dialog_token);
		if (QDF_IS_STATUS_ERROR(status)) {
			ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
			event_handled = false;
		}

		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_status = qdf_mc_timer_start(
					&ml_peer->ttlm_request_timer,
					TTLM_REQUEST_TIMEOUT);
			if (QDF_IS_STATUS_ERROR(qdf_status))
				t2lm_err("Failed to start the timer");
		}

		ttlm_handle_status_ind(ml_peer, dialog_token, status);
		break;
	case WLAN_TTLM_SM_EV_TIMEOUT:
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
		break;
	case WLAN_TTLM_SM_EV_RX_ACTION_RSP:
		status = ttlm_handle_rx_action_rsp_in_sta_in_progress_state(ml_peer, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
		break;
	case WLAN_TTLM_SM_EV_TTLM_REQ_TIMEOUT:
		ttlm_handle_timer_timeout(ml_peer);
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_subst_ap_action_inprogress_entry() - Entry API for AP ACTION INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to AP ACTION INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_action_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_substate(ml_peer, WLAN_TTLM_SS_AP_ACTION_INPROGRESS);
}

/**
 * ttlm_subst_ap_action_inprogress_exit() - Exit API for AP ACTION INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from AP ACTION INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_action_inprogress_exit(void *ctx)
{
}

/**
 * ttlm_subst_ap_action_inprogress_event() - AP ACTION INPROGRESS sub-state
 * event handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in AP ACTION INPROGRESS sub-state
 *
 * Return: true if event handle is successful else false
 */
static bool ttlm_subst_ap_action_inprogress_event(void *ctx, uint16_t event,
						  uint16_t data_len, void *data)
{
	bool event_handled = true;
	struct wlan_mlo_peer_context *ml_peer = ctx;
	QDF_STATUS status;

	switch (event) {
	case WLAN_TTLM_SM_EV_RX_ACTION_REQ:
		status = ttlm_handle_ap_action_req(ml_peer, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		break;
	case WLAN_TTLM_SM_EV_TX_ACTION_RSP:
		status = ttlm_send_ttlm_action_rsp(ml_peer, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_subst_ap_beacon_inprogress_entry() - Entry API for AP BEACON INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to AP BEACON INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_beacon_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_substate(ml_peer, WLAN_TTLM_SS_AP_BEACON_INPROGRESS);
}

/**
 * ttlm_subst_ap_beacon_inprogress_exit() - Exit API for AP BEACON INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from AP BEACON INPROGRESS  sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_beacon_inprogress_exit(void *ctx)
{
}

/**
 * ttlm_subst_ap_beacon_inprogress_event() - AP BEACON INPROGRESS sub-state
 *event handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in AP BEACON INPROGRESS sub-state
 *
 * Return: true if event handle is successful else false
 */
static bool ttlm_subst_ap_beacon_inprogress_event(void *ctx, uint16_t event,
						  uint16_t data_len, void *data)
{
	bool event_handled = true;

	switch (event) {
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_subst_ap_btm_inprogress_entry() - Entry API for AP BTM INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to AP BTM INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_btm_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_substate(ml_peer, WLAN_TTLM_SS_AP_BTM_INPROGRESS);
}

/**
 * ttlm_subst_ap_btm_inprogress_exit() - Exit API for AP BTM INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from AP BTM INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_ap_btm_inprogress_exit(void *ctx)
{
}

static QDF_STATUS
ttlm_handle_btm_link_disable_t2lm_frame(struct wlan_mlo_peer_context *ml_peer,
					struct wlan_t2lm_onging_negotiation_info *t2lm_neg)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = mlo_get_first_vdev_by_ml_peer(ml_peer);
	if (!vdev) {
		t2lm_err("vdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("peer is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_vdev;
	}

	status = t2lm_deliver_event(vdev, peer,
				    WLAN_T2LM_EV_ACTION_FRAME_TX_REQ,
				    t2lm_neg, 0, &t2lm_neg->dialog_token);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);

release_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return status;
}

/**
 * ttlm_subst_ap_btm_inprogress_event() - AP BTM INPROGRESS sub-state event
 * handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in AP BTM INPROGRESS sub-state
 *
 * Return: bool
 */
static bool ttlm_subst_ap_btm_inprogress_event(void *ctx, uint16_t event,
					       uint16_t data_len, void *data)
{
	bool event_handled = true;
	QDF_STATUS status;
	struct wlan_mlo_peer_context *ml_peer = ctx;

	switch (event) {
	case WLAN_TTLM_SM_EV_BTM_LINK_DISABLE:
		status = ttlm_handle_btm_link_disable_t2lm_frame(ml_peer, data);
		if (QDF_IS_STATUS_ERROR(status)) {
			ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
			event_handled = false;
			break;
		}

		status = qdf_mc_timer_start(&ml_peer->ttlm_request_timer,
					    TTLM_REQUEST_TIMEOUT);
		if (QDF_IS_STATUS_ERROR(status))
			t2lm_err("Failed to start the timer");
		break;
	case WLAN_TTLM_SM_EV_RX_ACTION_RSP:
		status = ttlm_handle_rx_action_rsp_in_sta_in_progress_state(ml_peer, data);
		if (QDF_IS_STATUS_ERROR(status))
			event_handled = false;
		ttlm_sm_transition_to(ml_peer, WLAN_TTLM_S_NEGOTIATED);
		break;
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}

/**
 * ttlm_subst_teardown_inprogress_entry() - Entry API for TEARDOWN INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on moving to TEARDOWN INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_teardown_inprogress_entry(void *ctx)
{
	struct wlan_mlo_peer_context *ml_peer = ctx;

	ttlm_set_substate(ml_peer, WLAN_TTLM_SS_TEARDOWN_INPROGRESS);
}

/**
 * ttlm_subst_teardown_inprogress_exit() - Exit API for TEARDOWN INPROGRESS
 * sub-state for TTLM
 * @ctx: MLO peer ctx
 *
 * API to perform operations on exiting from TEARDOWN INPROGRESS sub-state
 *
 * Return: void
 */
static void ttlm_subst_teardown_inprogress_exit(void *ctx)
{
}

/**
 * ttlm_subst_teardown_inprogress_event() - TEARDOWN INPROGRESS sub-state event
 * handler for TTLM
 * @ctx: MLO peer ctx
 * @event: event
 * @data_len: length of @data
 * @data: event data
 *
 * API to handle events in TEARDOWN INPROGRESS sub-state
 *
 * Return: bool
 */
static bool ttlm_subst_teardown_inprogress_event(void *ctx, uint16_t event,
						 uint16_t data_len, void *data)
{
	bool event_handled = true;

	switch (event) {
	default:
		event_handled = false;
		break;
	}

	return event_handled;
}
#else
static inline void ttlm_state_init_entry(void *ctx)
{
}

static inline void ttlm_state_init_exit(void *ctx)
{
}

static inline bool ttlm_state_init_event(void *ctx, uint16_t event,
					 uint16_t data_len, void *data)
{
return true;
}

static inline void ttlm_state_inprogress_entry(void *ctx)
{
}

static inline void ttlm_state_inprogress_exit(void *ctx)
{
}

static inline bool ttlm_state_inprogress_event(void *ctx, uint16_t event,
					       uint16_t data_len, void *data)
{
return true;
}

static inline void ttlm_state_negotiated_entry(void *ctx)
{
}

static inline void ttlm_state_negotiated_exit(void *ctx)
{
}

static inline bool ttlm_state_negotiated_event(void *ctx, uint16_t event,
					       uint16_t data_len, void *data)
{
return true;
}

static inline void ttlm_subst_sta_inprogress_entry(void *ctx)
{
}

static inline void ttlm_subst_sta_inprogress_exit(void *ctx)
{
}

static inline
bool ttlm_subst_sta_inprogress_event(void *ctx, uint16_t event,
				     uint16_t data_len, void *data)
{
return true;
}

static inline void ttlm_subst_ap_action_inprogress_entry(void *ctx)
{
}

static inline void ttlm_subst_ap_action_inprogress_exit(void *ctx)
{
}

static inline
bool ttlm_subst_ap_action_inprogress_event(void *ctx, uint16_t event,
					   uint16_t data_len, void *data)
{
return true;
}

static inline void ttlm_subst_ap_beacon_inprogress_entry(void *ctx)
{
}

static inline void ttlm_subst_ap_beacon_inprogress_exit(void *ctx)
{
}

static inline
bool ttlm_subst_ap_beacon_inprogress_event(void *ctx, uint16_t event,
					   uint16_t data_len, void *data)
{
	return true;
}

static inline void ttlm_subst_ap_btm_inprogress_entry(void *ctx)
{
}

static inline void ttlm_subst_ap_btm_inprogress_exit(void *ctx)
{
}

static inline
bool ttlm_subst_ap_btm_inprogress_event(void *ctx, uint16_t event,
					uint16_t data_len, void *data)
{
	return true;
}

static inline void ttlm_subst_teardown_inprogress_entry(void *ctx)
{
}

static inline void ttlm_subst_teardown_inprogress_exit(void *ctx)
{
}

static inline
bool ttlm_subst_teardown_inprogress_event(void *ctx, uint16_t event,
					  uint16_t data_len, void *data)
{
	return true;
}
#endif

struct wlan_sm_state_info ttlm_sm_info[] = {
	{
		(uint8_t)WLAN_TTLM_S_INIT,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"TTLM_INIT",
		ttlm_state_init_entry,
		ttlm_state_init_exit,
		ttlm_state_init_event
	},
	{
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		true,
		"TTLM_INPROGRESS",
		ttlm_state_inprogress_entry,
		ttlm_state_inprogress_exit,
		ttlm_state_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_S_NEGOTIATED,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"TTLM_NEGOTIATED",
		ttlm_state_negotiated_entry,
		ttlm_state_negotiated_exit,
		ttlm_state_negotiated_event
	},
	{
		(uint8_t)WLAN_TTLM_S_MAX,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"INVALID",
		NULL,
		NULL,
		NULL
	},
	{
		(uint8_t)WLAN_TTLM_SS_STA_INPROGRESS,
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"ST-STA_INPROGRESS",
		ttlm_subst_sta_inprogress_entry,
		ttlm_subst_sta_inprogress_exit,
		ttlm_subst_sta_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_SS_AP_ACTION_INPROGRESS,
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"ST-AP_ACTION_INPROGRESS",
		ttlm_subst_ap_action_inprogress_entry,
		ttlm_subst_ap_action_inprogress_exit,
		ttlm_subst_ap_action_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_SS_AP_BEACON_INPROGRESS,
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"ST-AP_BEACON_INPROGRESS",
		ttlm_subst_ap_beacon_inprogress_entry,
		ttlm_subst_ap_beacon_inprogress_exit,
		ttlm_subst_ap_beacon_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_SS_AP_BTM_INPROGRESS,
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"ST-AP_BTM_INPROGRESS",
		ttlm_subst_ap_btm_inprogress_entry,
		ttlm_subst_ap_btm_inprogress_exit,
		ttlm_subst_ap_btm_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_SS_TEARDOWN_INPROGRESS,
		(uint8_t)WLAN_TTLM_S_INPROGRESS,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"ST-TEARDOWN_INPROGRESS",
		ttlm_subst_teardown_inprogress_entry,
		ttlm_subst_teardown_inprogress_exit,
		ttlm_subst_teardown_inprogress_event
	},
	{
		(uint8_t)WLAN_TTLM_SS_MAX,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"INVALID",
		NULL,
		NULL,
		NULL
	},
};

#ifdef WLAN_FEATURE_11BE_MLO_TTLM
static const char *ttlm_sm_event_names[] = {
	"EV_TX_ACTION_REQ",
	"EV_TX_ACTION_REQ_START",
	"EV_TX_ACTION_REQ_ACTIVE",
	"EV_TX_ACTION_RSP",
	"EV_RX_ACTION_REQ",
	"EV_RX_ACTION_RSP",
	"EV_BEACON",
	"EV_BTM_LINK_DISABLE",
	"EV_TX_TEARDOWN",
	"EV_RX_TEARDOWN",
	"EV_TIMEOUT",
	"EV_TTLM_REQ_TIMEOUT",
	"EV_MAX",
};

enum wlan_ttlm_sm_state ttlm_get_state(struct wlan_mlo_peer_context *ml_peer)
{
	if (!ml_peer)
		return WLAN_TTLM_S_MAX;

	return ml_peer->ttlm_sm.ttlm_state;
}

enum wlan_ttlm_sm_state ttlm_get_sub_state(
			struct wlan_mlo_peer_context *ml_peer)
{
	if (!ml_peer)
		return WLAN_TTLM_SS_MAX;

	return ml_peer->ttlm_sm.ttlm_substate;
}

QDF_STATUS ttlm_sm_deliver_event(struct wlan_mlo_peer_context *ml_peer,
				 enum wlan_ttlm_sm_evt event,
				 uint16_t data_len, void *data)
{
	QDF_STATUS status;

	ttlm_lock_acquire(ml_peer);

	status = ttlm_sm_deliver_event_sync(ml_peer, event, data_len, data);

	ttlm_lock_release(ml_peer);

	return status;
}

QDF_STATUS ttlm_sm_create(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_sm *sm;
	uint8_t name[WLAN_SM_ENGINE_MAX_NAME];

	qdf_scnprintf(name, sizeof(name), "TTLM-Peer MLD:" QDF_MAC_ADDR_FMT,
		      QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
	sm = wlan_sm_create(name, ml_peer, WLAN_TTLM_S_INIT, ttlm_sm_info,
			    QDF_ARRAY_SIZE(ttlm_sm_info), ttlm_sm_event_names,
			    QDF_ARRAY_SIZE(ttlm_sm_event_names));
	if (!sm) {
		t2lm_err("TTLM state machine creation failed");
		return QDF_STATUS_E_NOMEM;
	}

	ml_peer->ttlm_sm.sm_hdl = sm;

	ttlm_lock_create(ml_peer);

	ttlm_timer_init(ml_peer);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ttlm_sm_destroy(struct wlan_mlo_peer_context *ml_peer)
{
	ttlm_timer_deinit(ml_peer);
	ttlm_lock_destroy(ml_peer);
	wlan_sm_delete(ml_peer->ttlm_sm.sm_hdl);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_mlo_parse_t2lm_info(uint8_t *ie,
				    struct wlan_t2lm_info *t2lm)
{
	struct wlan_ie_tid_to_link_mapping *t2lm_ie;
	enum wlan_t2lm_direction dir;
	uint8_t *t2lm_control_field;
	uint16_t t2lm_control;
	uint8_t link_mapping_presence_ind = 0;
	uint8_t *link_mapping_of_tids;
	uint8_t tid_num;
	uint8_t *ie_ptr = NULL;
	uint32_t ie_len_parsed = 0;
	uint32_t ie_len = 0;

	if (!ie || !t2lm) {
		t2lm_err("IE buffer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_ie = (struct wlan_ie_tid_to_link_mapping *)ie;
	ie_len = t2lm_ie->elem_len;

	/* Minimum IE length is 2 bytes:
	 *	elem_id_extn is 1 byte
	 *	t2lm_control_field can be of minimum 1 byte
	 */
	if (ie_len < WLAN_T2LM_CTRL_SIZE) {
		t2lm_debug("T2LM IE min length (%u) is invalid", ie_len);
		return QDF_STATUS_E_PROTO;
	}
	ie_len_parsed++;

	t2lm_control_field = t2lm_ie->data;
	if (!t2lm_control_field) {
		t2lm_err("t2lm_control_field is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_control = *t2lm_control_field;
	if (ie_len > WLAN_T2LM_CTRL_SIZE)
		t2lm_control = qdf_le16_to_cpu(*(uint16_t *)t2lm_control_field);

	dir = QDF_GET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DIRECTION_IDX,
			   WLAN_T2LM_CONTROL_DIRECTION_BITS);
	if (dir > WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Invalid direction");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm->direction = dir;
	t2lm->default_link_mapping =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_IDX,
			     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_BITS);

	t2lm->mapping_switch_time_present =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX,
			     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_BITS);

	t2lm->expected_duration_present =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_IDX,
			     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_BITS);

	t2lm->link_mapping_size =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_IDX,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_BITS);

	t2lm_debug("direction:%d default_link_mapping:%d mapping_switch_time_present:%d expected_duration_present:%d link_mapping_size:%d",
		   t2lm->direction, t2lm->default_link_mapping,
		    t2lm->mapping_switch_time_present,
		    t2lm->expected_duration_present,
		    t2lm->link_mapping_size);

	if (t2lm->default_link_mapping) {
		if (ie_len < (ie_len_parsed + sizeof(uint8_t))) {
			t2lm_rl_debug("Failed to parse Default link mapping=1");
			return QDF_STATUS_E_PROTO;
		}
		ie_len_parsed += sizeof(uint8_t);

		ie_ptr = t2lm_control_field + sizeof(uint8_t);
	} else {
		if (ie_len < (ie_len_parsed + sizeof(t2lm_control))) {
			t2lm_rl_debug("Failed to parse Default link mapping=0");
			return QDF_STATUS_E_PROTO;
		}
		ie_len_parsed += sizeof(t2lm_control);

		link_mapping_presence_ind =
			QDF_GET_BITS(t2lm_control,
				     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX,
				     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_BITS);
		ie_ptr = t2lm_control_field + sizeof(t2lm_control);
	}

	if (t2lm->mapping_switch_time_present) {
		if (ie_len < (ie_len_parsed + sizeof(uint16_t))) {
			t2lm_rl_debug("Failed to parse Mapping switch time");
			return QDF_STATUS_E_PROTO;
		}
		ie_len_parsed += sizeof(uint16_t);

		t2lm->mapping_switch_time =
			qdf_le16_to_cpu(*(uint16_t *)ie_ptr);
		ie_ptr += sizeof(uint16_t);
	}

	if (t2lm->expected_duration_present) {
		if (ie_len <
		    (ie_len_parsed + WLAN_T2LM_EXPECTED_DURATION_SIZE)) {
			t2lm_rl_debug("Failed to parse Expected duration");
			return QDF_STATUS_E_PROTO;
		}
		ie_len_parsed += WLAN_T2LM_EXPECTED_DURATION_SIZE;

		qdf_mem_copy(&t2lm->expected_duration, ie_ptr,
			     WLAN_T2LM_EXPECTED_DURATION_SIZE *
			     (sizeof(uint8_t)));
		ie_ptr += WLAN_T2LM_EXPECTED_DURATION_SIZE * (sizeof(uint8_t));
	}

	t2lm_debug("mapping_switch_time:%d expected_duration:%d",
		   t2lm->mapping_switch_time, t2lm->expected_duration);

	if (t2lm->default_link_mapping) {
		/* With default link mapping set to 1, there is no
		 * `Link Mapping of Tid n` field present.
		 */
		if (ie_len > ie_len_parsed) {
			t2lm_rl_debug("Link mapping of TID present when default link mapping is set");
			return QDF_STATUS_E_PROTO;
		}
		return QDF_STATUS_SUCCESS;
	}

	link_mapping_of_tids = ie_ptr;

	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		if (!(link_mapping_presence_ind & BIT(tid_num)))
			continue;

		if (!t2lm->link_mapping_size) {
			if (ie_len < (ie_len_parsed + sizeof(uint16_t))) {
				t2lm_rl_debug("Failed to parse Link mapping for tid=%u", tid_num);
				return QDF_STATUS_E_PROTO;
			}
			ie_len_parsed += sizeof(uint16_t);

			t2lm->ieee_link_map_tid[tid_num] =
				qdf_le16_to_cpu(*(uint16_t *)link_mapping_of_tids);
			link_mapping_of_tids += sizeof(uint16_t);
		} else {
			if (ie_len < (ie_len_parsed + sizeof(uint8_t))) {
				t2lm_rl_debug("Failed to parse Link mapping for tid=%u", tid_num);
				return QDF_STATUS_E_PROTO;
			}
			ie_len_parsed += sizeof(uint8_t);

			t2lm->ieee_link_map_tid[tid_num] =
				*(uint8_t *)link_mapping_of_tids;
			link_mapping_of_tids += sizeof(uint8_t);
		}

		t2lm_rl_debug("link mapping of TID%d is %x", tid_num,
			      t2lm->ieee_link_map_tid[tid_num]);
	}

	if (ie_len > ie_len_parsed) {
		t2lm_rl_debug("More data present at the end of T2LM element");
		return QDF_STATUS_E_PROTO;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_parse_bcn_prbresp_t2lm_ie(
		struct wlan_t2lm_context *t2lm_ctx, uint8_t *ie,
		uint32_t frame_len)
{
	struct wlan_t2lm_info t2lm = {0};
	struct extn_ie_header *ext_ie_hdr;
	QDF_STATUS retval;
	int i = 0;
	uint32_t ie_len_parsed = 0;

	qdf_mem_zero(&t2lm_ctx->established_t2lm,
		     sizeof(struct wlan_mlo_t2lm_ie));
	qdf_mem_zero(&t2lm_ctx->upcoming_t2lm, sizeof(struct wlan_mlo_t2lm_ie));

	t2lm_ctx->established_t2lm.t2lm.direction = WLAN_T2LM_INVALID_DIRECTION;
	t2lm_ctx->upcoming_t2lm.t2lm.direction = WLAN_T2LM_INVALID_DIRECTION;

	for (i = 0; i < WLAN_MAX_T2LM_IE; i++) {
		if (!ie || !frame_len) {
			t2lm_debug("ie is null or len is 0");
			return QDF_STATUS_E_NULL_VALUE;
		}

		if (frame_len == ie_len_parsed)
			return QDF_STATUS_SUCCESS;

		if (frame_len < (ie_len_parsed +
				 sizeof(struct extn_ie_header))) {
			t2lm_debug("Frame length is lesser than parsed T2LM IE header length");
			continue;
		}

		ext_ie_hdr = (struct extn_ie_header *)ie;

		if (!(ext_ie_hdr->ie_id == WLAN_ELEMID_EXTN_ELEM &&
		      ext_ie_hdr->ie_extn_id == WLAN_EXTN_ELEMID_T2LM))
			continue;

		ie_len_parsed += ext_ie_hdr->ie_len + sizeof(struct ie_header);
		if (frame_len < ie_len_parsed) {
			t2lm_debug("Frame length is lesser than parsed T2LM IE length");
			continue;
		}

		t2lm.direction = WLAN_T2LM_INVALID_DIRECTION;
		retval = wlan_mlo_parse_t2lm_info(ie, &t2lm);
		if (retval) {
			t2lm_err("Failed to parse the T2LM IE");
			return retval;
		}

		if (!t2lm.mapping_switch_time_present &&
		    t2lm.expected_duration_present) {
			qdf_mem_copy(&t2lm_ctx->established_t2lm.t2lm, &t2lm,
				     sizeof(struct wlan_t2lm_info));
		} else if (t2lm.mapping_switch_time_present) {
			qdf_mem_copy(&t2lm_ctx->upcoming_t2lm.t2lm, &t2lm,
				     sizeof(struct wlan_t2lm_info));
		}

		ie += ext_ie_hdr->ie_len + sizeof(struct ie_header);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_parse_t2lm_ie(
		struct wlan_t2lm_onging_negotiation_info *t2lm, uint8_t *ie,
		uint32_t frame_len)
{
	struct extn_ie_header *ext_ie_hdr = NULL;
	QDF_STATUS retval;
	enum wlan_t2lm_direction dir;
	struct wlan_t2lm_info t2lm_info;
	uint32_t ie_len_parsed = 0;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++)
		t2lm->t2lm_info[dir].direction = WLAN_T2LM_INVALID_DIRECTION;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (!ie || !frame_len) {
			t2lm_err("ie is null or len is 0");
			return QDF_STATUS_E_NULL_VALUE;
		}

		if (frame_len == ie_len_parsed) {
			t2lm_debug("Received T2LM IEs are parsed successfully");
			return QDF_STATUS_SUCCESS;
		}

		if (frame_len < (ie_len_parsed +
				 sizeof(struct extn_ie_header))) {
			t2lm_err("Frame length %d is lesser than parsed T2LM IE header length %zu",
				 frame_len,
				 ie_len_parsed + sizeof(struct extn_ie_header));
			return QDF_STATUS_E_PROTO;
		}

		ext_ie_hdr = (struct extn_ie_header *)ie;

		if (ext_ie_hdr->ie_id == WLAN_ELEMID_EXTN_ELEM &&
		    ext_ie_hdr->ie_extn_id == WLAN_EXTN_ELEMID_T2LM) {
			ie_len_parsed += ext_ie_hdr->ie_len + sizeof(struct ie_header);
			if (frame_len < ie_len_parsed) {
				t2lm_err("Frame length is lesser than parsed T2LM IE length");
				return QDF_STATUS_E_PROTO;
			}
			qdf_mem_zero(&t2lm_info, sizeof(t2lm_info));
			retval = wlan_mlo_parse_t2lm_info(ie, &t2lm_info);
			if (!retval &&
			    t2lm_info.direction < WLAN_T2LM_MAX_DIRECTION) {
				qdf_mem_copy(&t2lm->t2lm_info[t2lm_info.direction],
					     &t2lm_info,
					     sizeof(struct wlan_t2lm_info));
			} else {
				t2lm_debug("Failed to parse the T2LM IE");
				return retval;
			}
			ie += ext_ie_hdr->ie_len + sizeof(struct ie_header);
		}
	}

	if ((t2lm->t2lm_info[WLAN_T2LM_DL_DIRECTION].direction ==
			WLAN_T2LM_DL_DIRECTION ||
	     t2lm->t2lm_info[WLAN_T2LM_UL_DIRECTION].direction ==
			WLAN_T2LM_UL_DIRECTION) &&
	    t2lm->t2lm_info[WLAN_T2LM_BIDI_DIRECTION].direction ==
			WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Both DL/UL and BIDI T2LM IEs should not be present at the same time");

		qdf_mem_zero(t2lm, sizeof(*t2lm));
		for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
			t2lm->t2lm_info[dir].direction =
			    WLAN_T2LM_INVALID_DIRECTION;
		}

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t *wlan_mlo_add_t2lm_info_ie(uint8_t *frm, struct wlan_t2lm_info *t2lm,
				   struct wlan_objmgr_vdev *vdev)
{
	struct wlan_ie_tid_to_link_mapping *t2lm_ie;
	uint16_t t2lm_control = 0;
	uint8_t *t2lm_control_field;
	uint8_t *link_mapping_of_tids;
	uint8_t tid_num;
	uint8_t num_tids = 0;
	uint8_t link_mapping_presence_indicator = 0;
	struct vdev_mlme_obj *vdev_mlme;
	uint8_t *tmp_frm = frm;

	t2lm_ie = (struct wlan_ie_tid_to_link_mapping *)frm;
	t2lm_ie->elem_id = WLAN_ELEMID_EXTN_ELEM;
	t2lm_ie->elem_id_extn = WLAN_EXTN_ELEMID_T2LM;

	t2lm_ie->elem_len = sizeof(*t2lm_ie) - sizeof(struct ie_header);

	t2lm_control_field = t2lm_ie->data;

	QDF_SET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DIRECTION_IDX,
		     WLAN_T2LM_CONTROL_DIRECTION_BITS, t2lm->direction);

	QDF_SET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_IDX,
		     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_BITS,
		     t2lm->default_link_mapping);

	QDF_SET_BITS(t2lm_control,
		     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX,
		     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_BITS,
		     t2lm->mapping_switch_time_present);

	QDF_SET_BITS(t2lm_control,
		     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_IDX,
		     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_BITS,
		     t2lm->expected_duration_present);

	QDF_SET_BITS(t2lm_control,
		     WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_IDX,
		     WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_BITS,
		     t2lm->link_mapping_size);

	if (t2lm->default_link_mapping) {
		/* Link mapping of TIDs are not present when default mapping is
		 * set. Hence, the size of TID-To-Link mapping control is one
		 * octet.
		 */
		*t2lm_control_field = (uint8_t)t2lm_control;

		t2lm_ie->elem_len += sizeof(uint8_t);

		t2lm_rl_debug("T2LM IE added, default_link_mapping: %d dir:%d",
			      t2lm->default_link_mapping, t2lm->direction);

		frm += sizeof(*t2lm_ie) + sizeof(uint8_t);
	} else {
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++)
			if (t2lm->hw_link_map_tid[tid_num] ||
			    t2lm->ieee_link_map_tid[tid_num])
				link_mapping_presence_indicator |= BIT(tid_num);

		QDF_SET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_BITS,
			     link_mapping_presence_indicator);
		t2lm_rl_debug("T2LM IE added, direction:%d link_mapping_presence_indicator:%x",
			      t2lm->direction, link_mapping_presence_indicator);

		/* The size of TID-To-Link mapping control is two octets when
		 * default link mapping is not set.
		 */
		*(uint16_t *)t2lm_control_field = htole16(t2lm_control);
		frm += sizeof(*t2lm_ie) + sizeof(uint16_t);
		t2lm_ie->elem_len += sizeof(uint16_t);
	}

	if (t2lm->mapping_switch_time_present) {
		/* Mapping switch time is different for each vdevs. Hence,
		 * populate the mapping switch time from vdev_mlme_obj.
		 */
		vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
		if (!vdev_mlme) {
			t2lm_err("null vdev_mlme");
			return tmp_frm;
		}

		*(uint16_t *)frm =
			htole16(vdev_mlme->proto.ap.mapping_switch_time);
		frm += sizeof(uint16_t);
		t2lm_ie->elem_len += sizeof(uint16_t);
	}

	if (t2lm->expected_duration_present) {
		qdf_mem_copy(frm, &t2lm->expected_duration,
			     WLAN_T2LM_EXPECTED_DURATION_SIZE *
			     sizeof(uint8_t));
		frm += WLAN_T2LM_EXPECTED_DURATION_SIZE * sizeof(uint8_t);
		t2lm_ie->elem_len +=
			WLAN_T2LM_EXPECTED_DURATION_SIZE * sizeof(uint8_t);
	}

	t2lm_rl_debug("mapping_switch_time:%d expected_duration:%u",
		      t2lm->mapping_switch_time, t2lm->expected_duration);

	if (t2lm->default_link_mapping)
		return frm;

	link_mapping_of_tids = frm;

	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		if (!t2lm->ieee_link_map_tid[tid_num])
			continue;

		if (!t2lm->link_mapping_size) {
			*(uint16_t *)link_mapping_of_tids =
				htole16(t2lm->ieee_link_map_tid[tid_num]);
			t2lm_rl_debug("link mapping of TID%d is %x",
				      tid_num,
				      htole16(t2lm->ieee_link_map_tid[tid_num]));
			link_mapping_of_tids += sizeof(uint16_t);
		} else {
			*(uint8_t *)link_mapping_of_tids =
				t2lm->ieee_link_map_tid[tid_num];
			t2lm_rl_debug("link mapping of TID%d is %x",
				      tid_num,
				      t2lm->ieee_link_map_tid[tid_num]);
			link_mapping_of_tids += sizeof(uint8_t);
		}
		num_tids++;
	}

	if (!t2lm->link_mapping_size) {
		frm += num_tids * sizeof(uint16_t);
		t2lm_ie->elem_len += (num_tids * sizeof(uint16_t));
	} else {
		frm += num_tids * sizeof(uint8_t);
		t2lm_ie->elem_len += (num_tids * sizeof(uint8_t));
	}

	return frm;
}

uint8_t *wlan_mlo_add_t2lm_ie(uint8_t *frm,
			      struct wlan_t2lm_onging_negotiation_info *t2lm,
			      struct wlan_objmgr_vdev *vdev)
{
	uint8_t dir;

	if (!frm) {
		t2lm_err("frm is null");
		return NULL;
	}

	if (!t2lm) {
		t2lm_err("t2lm is null");
		return NULL;
	}

	/* As per spec, the frame should include one or two T2LM IEs. When it is
	 * two, then direction should DL and UL.
	 */
	if ((t2lm->t2lm_info[WLAN_T2LM_DL_DIRECTION].direction ==
			WLAN_T2LM_DL_DIRECTION ||
	     t2lm->t2lm_info[WLAN_T2LM_UL_DIRECTION].direction ==
			WLAN_T2LM_UL_DIRECTION) &&
	    t2lm->t2lm_info[WLAN_T2LM_BIDI_DIRECTION].direction ==
			WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Both DL/UL and BIDI T2LM IEs should not be present at the same time");
		return NULL;
	}

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (t2lm->t2lm_info[dir].direction !=
			WLAN_T2LM_INVALID_DIRECTION)
			frm = wlan_mlo_add_t2lm_info_ie(frm,
							&t2lm->t2lm_info[dir],
							vdev);
	}

	return frm;
}

/**
 * wlan_mlo_parse_t2lm_request_action_frame() - API to parse T2LM request action
 * frame.
 * @t2lm: Pointer to T2LM structure
 * @action_frm: Pointer to action frame
 * @frame_len: Received frame pointer
 * @category: T2LM action frame category
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_parse_t2lm_request_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		uint32_t frame_len,
		enum wlan_t2lm_category category)
{
	uint8_t *t2lm_action_frm;
	uint32_t ie_len_parsed;

	t2lm->category = category;

	/*
	 * T2LM request action frame
	 *
	 *   1-byte     1-byte     1-byte   variable
	 *-------------------------------------------
	 * |         |           |        |         |
	 * | Category| Protected | Dialog | T2LM IE |
	 * |         |    EHT    | token  |         |
	 * |         |  Action   |        |         |
	 *-------------------------------------------
	 */

	ie_len_parsed = sizeof(*action_frm) + sizeof(uint8_t);

	if (frame_len < ie_len_parsed) {
		t2lm_err("Action frame length %d too short", frame_len);
		return QDF_STATUS_E_FAILURE;
	}

	t2lm_action_frm = (uint8_t *)action_frm + sizeof(*action_frm);

	t2lm->dialog_token = *t2lm_action_frm;

	return wlan_mlo_parse_t2lm_ie(t2lm,
				      t2lm_action_frm + sizeof(uint8_t),
				      frame_len - ie_len_parsed);
}

/**
 * wlan_mlo_parse_t2lm_response_action_frame() - API to parse T2LM response
 * action frame.
 * @t2lm: Pointer to T2LM structure
 * @action_frm: Pointer to action frame
 * @frame_len: Action frame length
 * @category: T2LM action frame category
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_parse_t2lm_response_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		uint32_t frame_len,
		enum wlan_t2lm_category category)
{
	uint8_t *t2lm_action_frm;
	QDF_STATUS ret_val = QDF_STATUS_SUCCESS;
	uint32_t ie_len_parsed;

	t2lm->category = WLAN_T2LM_CATEGORY_RESPONSE;
	/*
	 * T2LM response action frame
	 *
	 *   1-byte     1-byte     1-byte   2-byte   variable
	 *----------------------------------------------------
	 * |         |           |        |        |         |
	 * | Category| Protected | Dialog | Status | T2LM IE |
	 * |         |    EHT    | token  |  code  |         |
	 * |         |  Action   |        |        |         |
	 *----------------------------------------------------
	 */

	ie_len_parsed = sizeof(*action_frm) + sizeof(uint8_t) +
			sizeof(uint16_t);

	if (frame_len < ie_len_parsed) {
		t2lm_err("Action frame length %d too short", frame_len);
		return QDF_STATUS_E_FAILURE;
	}

	t2lm_action_frm = (uint8_t *)action_frm + sizeof(*action_frm);

	t2lm->dialog_token = *t2lm_action_frm;
	t2lm->t2lm_resp_type =
	      qdf_le16_to_cpu(*(uint16_t *)(t2lm_action_frm + sizeof(uint8_t)));

	if (t2lm->t2lm_resp_type ==
			WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING) {
		t2lm_action_frm += sizeof(uint8_t) + sizeof(uint16_t);
		ret_val = wlan_mlo_parse_t2lm_ie(t2lm, t2lm_action_frm,
						 frame_len - ie_len_parsed);
	}

	return ret_val;
}

int wlan_mlo_parse_t2lm_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		uint32_t frame_len,
		enum wlan_t2lm_category category)
{
	QDF_STATUS ret_val = QDF_STATUS_SUCCESS;

	switch (category) {
	case WLAN_T2LM_CATEGORY_REQUEST:
		{
			ret_val = wlan_mlo_parse_t2lm_request_action_frame(
					t2lm, action_frm, frame_len, category);
			return qdf_status_to_os_return(ret_val);
		}
	case WLAN_T2LM_CATEGORY_RESPONSE:
		{
			ret_val = wlan_mlo_parse_t2lm_response_action_frame(
					t2lm, action_frm, frame_len, category);

			return qdf_status_to_os_return(ret_val);
		}
	case WLAN_T2LM_CATEGORY_TEARDOWN:
			/* Nothing to parse from T2LM teardown frame, just reset
			 * the mapping to default mapping.
			 *
			 * T2LM teardown action frame
			 *
			 *   1-byte     1-byte
			 *------------------------
			 * |         |           |
			 * | Category| Protected |
			 * |         |    EHT    |
			 * |         |  Action   |
			 *------------------------
			 */
			break;
	default:
			t2lm_err("Invalid category:%d", category);
	}

	return ret_val;
}

static uint8_t *wlan_mlo_add_t2lm_request_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;

	t2lm_info("T2LM request frame: category:%d action:%d dialog_token:%d",
		  args->category, args->action, args->arg1);
	return wlan_mlo_add_t2lm_ie(frm, (void *)buf, NULL);
}

static uint8_t *wlan_mlo_add_t2lm_response_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;
	/* Status code (2 bytes)*/
	*(uint16_t *)frm = htole16(args->arg2);
	frm += sizeof(uint16_t);

	t2lm_info("T2LM response frame: category:%d action:%d dialog_token:%d status_code:%d",
		  args->category, args->action, args->arg1, args->arg2);

	if (args->arg2 == WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING)
		frm = wlan_mlo_add_t2lm_ie(frm, (void *)buf, NULL);

	return frm;
}

uint8_t *wlan_mlo_add_t2lm_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{

	switch (category) {
	case WLAN_T2LM_CATEGORY_REQUEST:
		return wlan_mlo_add_t2lm_request_action_frame(frm, args,
							      buf, category);
	case WLAN_T2LM_CATEGORY_RESPONSE:
		return wlan_mlo_add_t2lm_response_action_frame(frm, args,
							      buf, category);
	case WLAN_T2LM_CATEGORY_TEARDOWN:
		*frm++ = args->category;
		*frm++ = args->action;
		return frm;
	default:
		t2lm_err("Invalid category:%d", category);
	}

	return frm;
}

/**
 * wlan_mlo_t2lm_handle_mapping_switch_time_expiry() - API to handle the mapping
 * switch timer expiry.
 * @t2lm_ctx: Pointer to T2LM context
 * @vdev: Pointer to vdev structure
 *
 * Return: None
 */
static void wlan_mlo_t2lm_handle_mapping_switch_time_expiry(
		struct wlan_t2lm_context *t2lm_ctx,
		struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_info *t2lm;

	t2lm_debug("Mapping switch time expired for vdev_id:%d ",
		   wlan_vdev_get_id(vdev));

	qdf_mem_copy(&t2lm_ctx->established_t2lm, &t2lm_ctx->upcoming_t2lm,
		     sizeof(struct wlan_mlo_t2lm_ie));

	t2lm_ctx->established_t2lm.t2lm.mapping_switch_time_present = false;
	t2lm_ctx->established_t2lm.t2lm.mapping_switch_time = 0;

	t2lm = &t2lm_ctx->established_t2lm.t2lm;
	t2lm_debug("Established mapping: disabled_link_bitmap:%x dir:%d default_map:%d MSTP:%d EDP:%d MST:%d ED:%d ieee_link_map:%x hw_link_map:%x",
		   t2lm_ctx->established_t2lm.disabled_link_bitmap,
		   t2lm->direction, t2lm->default_link_mapping,
		   t2lm->mapping_switch_time_present,
		   t2lm->expected_duration_present,
		   t2lm->mapping_switch_time, t2lm->expected_duration,
		   t2lm->ieee_link_map_tid[0], t2lm->hw_link_map_tid[0]);

	qdf_mem_zero(&t2lm_ctx->upcoming_t2lm, sizeof(struct wlan_mlo_t2lm_ie));
	t2lm_ctx->upcoming_t2lm.t2lm.direction = WLAN_T2LM_INVALID_DIRECTION;
}

/**
 * wlan_mlo_t2lm_handle_expected_duration_expiry() - API to handle the expected
 * duration timer expiry.
 * @t2lm_ctx: Pointer to T2LM context
 * @vdev: Pointer to vdev structure
 *
 * Return: none
 */
static void wlan_mlo_t2lm_handle_expected_duration_expiry(
		struct wlan_t2lm_context *t2lm_ctx,
		struct wlan_objmgr_vdev *vdev)
{
	t2lm_debug("Expected duration expired for vdev_id:%d ",
		   wlan_vdev_get_id(vdev));

	if (t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time_present) {
		/* Copy the new non-default ongoing mapping to established
		 * mapping if expected duration expires for the established
		 * mapping.
		 */
		wlan_mlo_t2lm_handle_mapping_switch_time_expiry(t2lm_ctx,
								vdev);
		return;
	}

	/* Use the default mapping when expected duration expires for the
	 * established mapping and no new non-default T2LM announcement is
	 * ongoing.
	 */
	qdf_mem_zero(&t2lm_ctx->established_t2lm,
		     sizeof(struct wlan_mlo_t2lm_ie));

	t2lm_ctx->established_t2lm.t2lm.direction = WLAN_T2LM_BIDI_DIRECTION;
	t2lm_ctx->established_t2lm.t2lm.default_link_mapping = 1;
	t2lm_ctx->established_t2lm.disabled_link_bitmap = 0;
	t2lm_ctx->established_t2lm.t2lm.link_mapping_size = 0;
	t2lm_debug("Set established mapping to default mapping");

	wlan_clear_peer_level_tid_to_link_mapping(vdev);
}

QDF_STATUS wlan_mlo_vdev_tid_to_link_map_event(
		struct wlan_objmgr_psoc *psoc,
		struct mlo_vdev_host_tid_to_link_map_resp *event)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_t2lm_context *t2lm_ctx;
	struct vdev_mlme_obj *vdev_mlme;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, event->vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		t2lm_err("null vdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!vdev->mlo_dev_ctx) {
		t2lm_err("null mlo_dev_ctx");
		mlo_release_vdev_ref(vdev);
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		t2lm_err("null vdev_mlme");
		mlo_release_vdev_ref(vdev);
		return QDF_STATUS_E_FAILURE;
	}

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	t2lm_debug("psoc_id:%d vdev_id:%d status:%d",
		   wlan_psoc_get_id(psoc), event->vdev_id, event->status);

	t2lm_dev_lock_acquire(t2lm_ctx);
	switch (event->status) {
	case WLAN_MAP_SWITCH_TIMER_TSF:

		/* Mapping switch time is different for each AP vdev of a given
		 * MLD as these vdevs can have separate beacon TDF value.
		 */
		if (t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time_present)
			vdev_mlme->proto.ap.mapping_switch_time =
				(event->mapping_switch_tsf &
				 WLAN_T2LM_MAPPING_SWITCH_TSF_BITS) >> 10;

		t2lm_debug("vdev_id:%d updated mapping switch time:%d",
			   event->vdev_id,
			   vdev_mlme->proto.ap.mapping_switch_time);
		break;
	case WLAN_MAP_SWITCH_TIMER_EXPIRED:
		vdev_mlme->proto.ap.mapping_switch_time = 0;
		wlan_mlo_t2lm_handle_mapping_switch_time_expiry(t2lm_ctx, vdev);

		/* Notify the registered caller about the link update*/
		wlan_mlo_dev_t2lm_notify_link_update(vdev,
					&t2lm_ctx->established_t2lm.t2lm);
		break;
	case WLAN_EXPECTED_DUR_EXPIRED:
		wlan_mlo_t2lm_handle_expected_duration_expiry(t2lm_ctx, vdev);

		/* Notify the registered caller about the link update*/
		wlan_mlo_dev_t2lm_notify_link_update(vdev,
					&t2lm_ctx->established_t2lm.t2lm);
		break;
	default:
		t2lm_err("Invalid status");
	}

	t2lm_dev_lock_release(t2lm_ctx);
	mlo_release_vdev_ref(vdev);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static
QDF_STATUS wlan_send_t2lm_info(struct wlan_objmgr_vdev *vdev,
			       struct wlan_t2lm_info *t2lm,
			       struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		t2lm_err("vdev is not MLO vdev");
		return status;
	}

	status = mlo_tx_ops->send_tid_to_link_mapping(vdev, t2lm);
	if (QDF_IS_STATUS_ERROR(status))
		t2lm_err("Failed to send T2LM command to FW");

	return status;
}
#else
static
QDF_STATUS wlan_send_t2lm_info(struct wlan_objmgr_vdev *vdev,
			       struct wlan_t2lm_info *t2lm,
			       struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops)
{
	struct wlan_objmgr_vdev *co_mld_vdev;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0;
	int i = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	mlo_get_ml_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	if (!vdev_count) {
		t2lm_err("Number of VDEVs under MLD is reported as 0");
		return QDF_STATUS_E_NULL_VALUE;
	}

	for (i = 0; i < vdev_count; i++) {
		co_mld_vdev = wlan_vdev_list[i];
		if (!co_mld_vdev) {
			t2lm_err("co_mld_vdev is null");
			mlo_release_vdev_ref(co_mld_vdev);
			continue;
		}

		if (mlo_is_sta_bridge_vdev(co_mld_vdev)) {
			t2lm_debug("skip co_mld_vdev for bridge sta");
			mlo_release_vdev_ref(co_mld_vdev);
			continue;
		}

		status = mlo_tx_ops->send_tid_to_link_mapping(co_mld_vdev,
							      t2lm);
		if (QDF_IS_STATUS_ERROR(status))
			t2lm_err("Failed to send T2LM command to FW");
		mlo_release_vdev_ref(co_mld_vdev);
	}

	return status;
}
#endif

QDF_STATUS wlan_send_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_t2lm_info *t2lm)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		t2lm_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;
	if (!mlo_tx_ops) {
		t2lm_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_tx_ops->send_tid_to_link_mapping) {
		t2lm_err("send_tid_to_link_mapping is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_send_t2lm_info(vdev, t2lm, mlo_tx_ops);

	return status;
}

/**
 * wlan_get_vdev_t2lm_mapping_status() - API to get vdev level T2LM info
 * @vdev: vdev object
 * @t2lm: T2LM info
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS wlan_get_vdev_t2lm_mapping_status(struct wlan_objmgr_vdev *vdev,
					     struct wlan_t2lm_info *t2lm)
{
	struct wlan_t2lm_context *t2lm_ctx;
	int i = 0;

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	t2lm_dev_lock_acquire(t2lm_ctx);
	qdf_mem_copy(&t2lm[i++], &t2lm_ctx->established_t2lm.t2lm,
		     sizeof(struct wlan_t2lm_info));
	t2lm_dev_lock_release(t2lm_ctx);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_get_peer_t2lm_mapping_status() - API to get peer level T2LM info
 * @peer: peer object
 * @t2lm: T2LM info
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS wlan_get_peer_t2lm_mapping_status(struct wlan_objmgr_peer *peer,
					     struct wlan_t2lm_info *t2lm)
{
	enum wlan_t2lm_direction dir = WLAN_T2LM_INVALID_DIRECTION;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_prev_t2lm_negotiated_info *t2lm_req;
	int i = 0;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	t2lm_req = &ml_peer->t2lm_policy.t2lm_negotiated_info;
	if ((t2lm_req->t2lm_info[WLAN_T2LM_DL_DIRECTION].direction ==
			WLAN_T2LM_DL_DIRECTION ||
	     t2lm_req->t2lm_info[WLAN_T2LM_UL_DIRECTION].direction ==
			WLAN_T2LM_UL_DIRECTION) &&
	     t2lm_req->t2lm_info[WLAN_T2LM_BIDI_DIRECTION].direction ==
			WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Both DL/UL and BIDI T2LM IEs should not be present at the same time");
		return QDF_STATUS_E_FAILURE;
	}

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (t2lm_req->t2lm_info[dir].direction !=
			WLAN_T2LM_INVALID_DIRECTION)
			qdf_mem_copy(&t2lm[i++], &t2lm_req->t2lm_info[dir],
				     sizeof(struct wlan_t2lm_info));
	}

	if (i == 0)
		return QDF_STATUS_E_EMPTY;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_get_t2lm_mapping_status(struct wlan_objmgr_vdev *vdev,
					struct wlan_t2lm_info *t2lm)
{
	struct wlan_objmgr_peer *peer;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("peer not found");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_get_peer_t2lm_mapping_status(peer, t2lm);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		t2lm_debug("peer level T2LM info");
		goto peer_release;
	}

	t2lm_debug("vdev level T2LM info");
	status = wlan_get_vdev_t2lm_mapping_status(vdev, t2lm);

peer_release:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);

	return status;
}

QDF_STATUS
wlan_send_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_objmgr_peer *peer)
{
	uint8_t dir, idx = 0;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_t2lm_info *t2lm_info;
	QDF_STATUS status = QDF_STATUS_E_NULL_VALUE;

	if (!peer) {
		t2lm_err("peer is null");
		return status;
	}

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		t2lm_err("ml peer is null");
		return status;
	}

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		t2lm_info = &ml_peer->t2lm_policy.t2lm_negotiated_info.t2lm_info[dir];
		if (t2lm_info && t2lm_info->direction !=
		    WLAN_T2LM_INVALID_DIRECTION) {
			t2lm_debug("send peer-level mapping to FW for dir: %d", dir);

			/* Notify the registered caller about the link update*/
			wlan_mlo_dev_t2lm_notify_link_update(vdev, t2lm_info);
			status = wlan_send_tid_to_link_mapping(vdev, t2lm_info);
			idx++;
		}
	}

	if (!idx)
		t2lm_debug("No peer-level mapping present");

	return status;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE)
void
wlan_clear_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer) {
		t2lm_err("Peer not found");
		return;
	}

	wlan_t2lm_clear_peer_negotiation(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}
#endif

enum qdf_hrtimer_restart_status
wlan_mlo_t2lm_timer_expiry_handler(qdf_hrtimer_data_t *arg)
{
	struct wlan_t2lm_timer *timer_ctx;
	struct wlan_t2lm_context *t2lm_ctx;
	struct wlan_objmgr_vdev *vdev;

	timer_ctx = container_of(arg, struct wlan_t2lm_timer, t2lm_timer);

	timer_ctx->timer_started = false;
	timer_ctx->timer_interval = 0;
	timer_ctx->timer_out_time = 0;

	t2lm_ctx = timer_ctx->t2lm_ctx;

	vdev = mlo_get_first_active_vdev_by_ml_dev_ctx(t2lm_ctx->mlo_dev_ctx);
	if (!vdev) {
		t2lm_err("null vdev");
		mlo_t2lm_reset_established_and_upcoming_mapping(t2lm_ctx->mlo_dev_ctx);
		return QDF_HRTIMER_NORESTART;
	}

	/* Since qdf_mutex_acquire cannot be called from interrupt context,
	 * change needed to create a workqueue and offload the timer expiry
	 * handling to workqueue.
	 */
	if (t2lm_ctx->established_t2lm.t2lm.expected_duration_present) {
		wlan_mlo_t2lm_handle_expected_duration_expiry(t2lm_ctx, vdev);

		/* Notify the registered caller about the link update*/
		wlan_mlo_dev_t2lm_notify_link_update(
				vdev, &t2lm_ctx->established_t2lm.t2lm);
		wlan_send_tid_to_link_mapping(
				vdev, &t2lm_ctx->established_t2lm.t2lm);

		wlan_handle_t2lm_timer(vdev);
	} else if (t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time_present) {
		wlan_mlo_t2lm_handle_mapping_switch_time_expiry(t2lm_ctx, vdev);

		/* Notify the registered caller about the link update*/
		wlan_mlo_dev_t2lm_notify_link_update(
				vdev, &t2lm_ctx->established_t2lm.t2lm);
		wlan_send_tid_to_link_mapping(
				vdev, &t2lm_ctx->established_t2lm.t2lm);
		wlan_handle_t2lm_timer(vdev);
	}

	mlo_release_vdev_ref(vdev);

	return QDF_HRTIMER_NORESTART;
}

/**
 * mlme_vdev_notify_link_update_event() - Notify link update Event
 * @vdev: Pointer to vdev object
 * @t2lm: Pointer to T2LM info structure
 *
 * Notify link update event
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS mlme_vdev_notify_link_update_event(
				struct wlan_objmgr_vdev *vdev,
				struct wlan_t2lm_info *t2lm)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		qdf_err("vdev_mlme is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_mlme->ops &&
	    vdev_mlme->ops->mlme_vdev_notify_link_update_event)
		ret = vdev_mlme->ops->mlme_vdev_notify_link_update_event(
					vdev, t2lm);

	return ret;
}

#ifndef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * wlan_mlo_t2lm_update_peer_to_peer_negotiation() - API to update peer-to-peer
 * level T2LM negotiation data structure on mapping switch time expiry and
 * expected duration expiry.
 * @ml_dev: Pointer to ML dev structure
 * @ml_peer: Pointer to ML peer
 * @arg: Pointer to advertised T2LM structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_t2lm_update_peer_to_peer_negotiation(
		struct wlan_mlo_dev_context *ml_dev,
		void *ml_peer, void *arg)
{
	struct wlan_mlo_peer_context *mlo_peer;
	struct wlan_t2lm_info *t2lm;
	struct wlan_prev_t2lm_negotiated_info *negotiated_t2lm = NULL;
	uint8_t dir = 0;

	mlo_peer = (struct wlan_mlo_peer_context *)ml_peer;
	if (!mlo_peer) {
		t2lm_err("null mlo_peer");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm = (struct wlan_t2lm_info *)arg;
	if (!t2lm) {
		t2lm_err("null T2LM");
		return QDF_STATUS_E_NULL_VALUE;
	}

	negotiated_t2lm = &mlo_peer->t2lm_policy.t2lm_negotiated_info;
	negotiated_t2lm->dialog_token = 0;

	/* Reset the peer-to-peer level mapping to default mapping */
	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		negotiated_t2lm->t2lm_info[dir].direction =
			WLAN_T2LM_INVALID_DIRECTION;
	}

	/* Copy the Advertised T2LM established mapping to peer-to-peer level
	 * DIBI direction data structure.
	 */
	qdf_mem_copy(&negotiated_t2lm->t2lm_info[WLAN_T2LM_BIDI_DIRECTION],
		     t2lm, sizeof(struct wlan_t2lm_info));

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlo_t2lm_link_update_notifier_callback() - This callback API is invoked
 * when mapping switch timer expires and expected duration expires.
 * @vdev: Pointer to vdev structure
 * @t2lm: Pointer to advertised T2LM structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_t2lm_link_update_notifier_callback(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_info *t2lm)
{
	/* Send event to user-space to notify link update */
	mlme_vdev_notify_link_update_event(vdev, t2lm);

	/* Go over all MLO peers on this MLD and clear the peer-to-peer level
	 * mapping.
	 */
	wlan_mlo_iterate_ml_peerlist(
			vdev->mlo_dev_ctx,
			wlan_mlo_t2lm_update_peer_to_peer_negotiation, t2lm);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_t2lm_register_link_update_notify_handler(
		struct wlan_mlo_dev_context *ml_dev)
{
	ml_dev->t2lm_ctx.link_update_callback_index =
		wlan_register_t2lm_link_update_notify_handler(
				wlan_mlo_t2lm_link_update_notifier_callback,
				ml_dev);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
wlan_mlo_t2lm_timer_init(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer = NULL;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_FAILURE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_dev_lock_create(&vdev->mlo_dev_ctx->t2lm_ctx);
	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	qdf_hrtimer_init(&t2lm_timer->t2lm_timer,
			 wlan_mlo_t2lm_timer_expiry_handler,
			 QDF_CLOCK_MONOTONIC,
			 QDF_HRTIMER_MODE_REL,
			 QDF_CONTEXT_TASKLET);

	t2lm_timer->timer_started = false;
	t2lm_timer->timer_interval = 0;
	t2lm_timer->timer_out_time = 0;
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_t2lm_timer_start(struct wlan_objmgr_vdev *vdev,
			  uint32_t interval)
{
	struct wlan_t2lm_timer *t2lm_timer;
	struct wlan_t2lm_context *t2lm_ctx;
	uint32_t target_out_time;
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	if (!interval) {
		t2lm_debug("Timer interval is 0");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_ctx = &mlo_dev_ctx->t2lm_ctx;
	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	interval = (interval * 1024) / 1000;
	target_out_time = qdf_system_ticks_to_msecs(qdf_system_ticks());
	target_out_time += interval;
	t2lm_debug("earlier timer @%u ms out, new @%u ms out",
		   t2lm_timer->timer_out_time, target_out_time);

	/* sometimes the host process the beacon maybe delay, it may help for
	 * update the new expected time.
	 */
	if (t2lm_timer->timer_out_time &&
	    (target_out_time > t2lm_timer->timer_out_time ||
	     (t2lm_timer->timer_out_time - target_out_time) <
	      WLAN_T2LM_MAPPING_SWITCH_TIME_DELAY))
		return QDF_STATUS_E_NULL_VALUE;

	if (t2lm_timer->timer_started)
		qdf_hrtimer_cancel(&t2lm_timer->t2lm_timer);

	t2lm_debug("t2lm timer started with interval %d ms", interval);
	t2lm_timer->timer_interval = interval;
	t2lm_timer->timer_started = true;
	t2lm_timer->timer_out_time = target_out_time;

	qdf_hrtimer_start(&t2lm_timer->t2lm_timer,
			  qdf_time_ms_to_ktime(t2lm_timer->timer_interval),
			  QDF_HRTIMER_MODE_REL);

	return QDF_STATUS_SUCCESS;
}

void wlan_t2lm_timer_stop(struct wlan_t2lm_timer *t2lm_timer)
{
	if (!t2lm_timer->timer_started)
		return;

	qdf_hrtimer_cancel(&t2lm_timer->t2lm_timer);
	t2lm_timer->timer_started = false;
	t2lm_timer->timer_interval = 0;
	t2lm_timer->timer_out_time = 0;
}

QDF_STATUS
wlan_mlo_t2lm_timer_stop(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_t2lm_timer_stop(t2lm_timer);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_handle_t2lm_timer(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_context *t2lm_ctx;
	struct vdev_mlme_obj *vdev_mlme;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	if (t2lm_ctx->established_t2lm.t2lm.expected_duration_present) {
		if (t2lm_ctx->established_t2lm.t2lm.expected_duration ==
		    T2LM_EXPECTED_DURATION_MAX_VALUE) {
			return status;
		}

		status = wlan_mlo_t2lm_timer_start(
				vdev,
				t2lm_ctx->established_t2lm.t2lm.expected_duration);
	} else if (t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time_present) {
		status = wlan_mlo_t2lm_timer_start(
				vdev,
				t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time);
	}

	return status;
}

/**
 * wlan_update_mapping_switch_time_expected_dur() - API to update the mapping
 * switch time and expected duration.
 * @vdev:Pointer to vdev
 * @rx_t2lm: Pointer to received T2LM IE
 * @tsf: TSF value of beacon/probe response
 *
 * Return: None
 */
static QDF_STATUS wlan_update_mapping_switch_time_expected_dur(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_context *rx_t2lm, uint64_t tsf)
{
	struct wlan_t2lm_context *t2lm_ctx;
	uint16_t tsf_bit25_10, ms_time;
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	tsf_bit25_10 = (tsf & WLAN_T2LM_MAPPING_SWITCH_TSF_BITS) >> 10;
	t2lm_ctx = &mlo_dev_ctx->t2lm_ctx;

	t2lm_dev_lock_acquire(t2lm_ctx);

	if ((t2lm_ctx->established_t2lm.t2lm.expected_duration_present &&
	    rx_t2lm->established_t2lm.t2lm.expected_duration_present) &&
	    (rx_t2lm->established_t2lm.t2lm.expected_duration <
	     t2lm_ctx->established_t2lm.t2lm.expected_duration)) {
		/* Established T2LM is already saved in the T2LM context.
		 * T2LM IE in the beacon/probe response frame has the updated
		 * expected duration.
		 */
		if (!qdf_mem_cmp(t2lm_ctx->established_t2lm.t2lm.ieee_link_map_tid,
				 rx_t2lm->established_t2lm.t2lm.ieee_link_map_tid,
				 sizeof(uint16_t) * T2LM_MAX_NUM_TIDS)) {
			t2lm_ctx->established_t2lm.t2lm.expected_duration =
				rx_t2lm->established_t2lm.t2lm.expected_duration;
		}
	} else if (rx_t2lm->established_t2lm.t2lm.expected_duration_present &&
		   !rx_t2lm->established_t2lm.t2lm.mapping_switch_time_present) {
		if (!qdf_mem_cmp(t2lm_ctx->established_t2lm.t2lm.ieee_link_map_tid,
				 rx_t2lm->established_t2lm.t2lm.ieee_link_map_tid,
				 sizeof(uint16_t) * T2LM_MAX_NUM_TIDS)) {
			t2lm_debug("T2LM mapping is already configured");
			t2lm_dev_lock_release(t2lm_ctx);
			return QDF_STATUS_E_ALREADY;
		}

		/* Mapping switch time is already expired when STA receives the
		 * T2LM IE from beacon/probe response frame.
		 */
		qdf_mem_copy(&t2lm_ctx->established_t2lm.t2lm,
			     &rx_t2lm->established_t2lm.t2lm,
			     sizeof(struct wlan_t2lm_info));

		/* Notify the registered caller about the link update*/
		wlan_mlo_dev_t2lm_notify_link_update(vdev,
					&t2lm_ctx->established_t2lm.t2lm);
		wlan_clear_peer_level_tid_to_link_mapping(vdev);
		wlan_send_tid_to_link_mapping(
				vdev, &t2lm_ctx->established_t2lm.t2lm);
	}

	if (rx_t2lm->upcoming_t2lm.t2lm.mapping_switch_time_present) {
		if (!qdf_mem_cmp(t2lm_ctx->established_t2lm.t2lm.ieee_link_map_tid,
				 rx_t2lm->upcoming_t2lm.t2lm.ieee_link_map_tid,
				 sizeof(uint16_t) * T2LM_MAX_NUM_TIDS)) {
			t2lm_debug("Ongoing mapping is already established");
			t2lm_dev_lock_release(t2lm_ctx);
			return QDF_STATUS_E_ALREADY;
		}

		qdf_mem_copy(&t2lm_ctx->upcoming_t2lm.t2lm,
			     &rx_t2lm->upcoming_t2lm.t2lm,
			     sizeof(struct wlan_t2lm_info));

		ms_time = t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time;
		/* Per test, -300ms is fine */
		if (ms_time > tsf_bit25_10) {
			t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time =
				(ms_time - tsf_bit25_10 - (3 * WLAN_T2LM_MAPPING_SWITCH_TIME_DELAY));
		} else {
			t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time =
				0xFFFF - (tsf_bit25_10 - ms_time) - (3 * WLAN_T2LM_MAPPING_SWITCH_TIME_DELAY);
		}
	}

	t2lm_dev_lock_release(t2lm_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_process_bcn_prbrsp_t2lm_ie(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_context *rx_t2lm_ie, uint64_t tsf)
{
	struct wlan_t2lm_context *t2lm_ctx;
	QDF_STATUS status;
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	/* Do not parse the T2LM IE if STA is not in connected state */
	if (!wlan_cm_is_vdev_connected(vdev))
		return QDF_STATUS_SUCCESS;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	mlo_dev_ctx = wlan_vdev_get_mlo_dev_ctx(vdev);
	if (!mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_ctx = &mlo_dev_ctx->t2lm_ctx;

	status = wlan_update_mapping_switch_time_expected_dur(
			vdev, rx_t2lm_ie, tsf);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	t2lm_dev_lock_acquire(t2lm_ctx);
	if (t2lm_ctx->established_t2lm.t2lm.expected_duration_present ||
	    t2lm_ctx->upcoming_t2lm.t2lm.mapping_switch_time_present) {
		wlan_handle_t2lm_timer(vdev);
	}
	t2lm_dev_lock_release(t2lm_ctx);

	return QDF_STATUS_SUCCESS;
}

int wlan_register_t2lm_link_update_notify_handler(
		wlan_mlo_t2lm_link_update_handler handler,
		struct wlan_mlo_dev_context *mldev)
{
	struct wlan_t2lm_context *t2lm_ctx = &mldev->t2lm_ctx;
	int i;

	for (i = 0; i < MAX_T2LM_HANDLERS; i++) {
		if (t2lm_ctx->is_valid_handler[i])
			continue;

		t2lm_ctx->link_update_handler[i] = handler;
		t2lm_ctx->is_valid_handler[i] = true;
		break;
	}

	if (i == MAX_T2LM_HANDLERS) {
		t2lm_err("Failed to register the link disablement callback");
		return -EINVAL;
	}

	return i;
}

void wlan_unregister_t2lm_link_update_notify_handler(
		struct wlan_mlo_dev_context *mldev,
		uint8_t index)
{
	if (index >= MAX_T2LM_HANDLERS)
		return;

	mldev->t2lm_ctx.link_update_handler[index] = NULL;
	mldev->t2lm_ctx.is_valid_handler[index] = false;
}

QDF_STATUS wlan_mlo_dev_t2lm_notify_link_update(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_info *t2lm)
{
	struct wlan_t2lm_context *t2lm_ctx;
	wlan_mlo_t2lm_link_update_handler handler;
	int i;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_FAILURE;

	if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE &&
	    !wlan_cm_is_vdev_connected(vdev)) {
		t2lm_err("Not associated!");
		return QDF_STATUS_E_AGAIN;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		t2lm_err("failed due to non-ML connection");
		return QDF_STATUS_E_INVAL;
	}

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;
	for (i = 0; i < MAX_T2LM_HANDLERS; i++) {
		if (!t2lm_ctx->is_valid_handler[i])
			continue;

		handler = t2lm_ctx->link_update_handler[i];
		if (!handler)
			continue;

		handler(vdev, t2lm);

		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_t2lm_timer_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer = NULL;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_FAILURE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	t2lm_timer->timer_started = false;
	t2lm_timer->timer_interval = 0;
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);
	qdf_hrtimer_kill(&t2lm_timer->t2lm_timer);
	t2lm_dev_lock_destroy(&vdev->mlo_dev_ctx->t2lm_ctx);
	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE) && defined(WLAN_FEATURE_11BE)
QDF_STATUS
wlan_mlo_link_disable_request_handler(struct wlan_objmgr_psoc *psoc,
				      void *evt_params)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id;
	bool is_connected = false;
	struct mlo_link_disable_request_evt_params *params;

	if (!psoc)
		return QDF_STATUS_E_NULL_VALUE;

	if (!evt_params) {
		t2lm_err("event params is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	params = (struct mlo_link_disable_request_evt_params *)evt_params;
	if (qdf_is_macaddr_zero(&params->mld_addr)) {
		t2lm_err("mld mac addr in event params is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!params->link_id_bitmap) {
		t2lm_debug("Link id bitmap is 0, no action frame to be sent");
		return QDF_STATUS_SUCCESS;
	}

	is_connected = wlan_get_connected_vdev_by_mld_addr(psoc,
							   params->mld_addr.bytes,
							   &vdev_id);
	if (!is_connected) {
		t2lm_err("Not connected to peer MLD " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(params->mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		t2lm_err("vdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = wlan_populate_link_disable_t2lm_frame(vdev, params);

	if (QDF_IS_STATUS_ERROR(status))
		t2lm_err("Failed to handle link disable");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return status;
}
#endif

