// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "debug.h"
#include "ath12k_cmn_extn.h"
#include "cbs_extn.h"

static int ath12k_cbs_submit_scan(struct ath12k *ar, struct ath12k_cbs *cbs);

static const char *ath12k_cbs_state_str(enum ath12k_cbs_state state)
{
	switch (state) {
	case ATH12K_CBS_STATE_INIT:
		return "INIT";
	case ATH12K_CBS_STATE_SCAN:
		return "SCAN";
	case ATH12K_CBS_STATE_REST:
		return "REST";
	case ATH12K_CBS_STATE_WAIT:
		return "WAIT";
	}

	return "UNKNOWN";
}

static const char *ath12k_cbs_event_str(enum ath12k_cbs_event event)
{
	switch (event) {
	case ATH12K_CBS_EVENT_SCAN_START:
		return "SCAN_START";
	case ATH12K_CBS_EVENT_SCAN_NEXT:
		return "SCAN_NEXT";
	case ATH12K_CBS_EVENT_SCAN_CONTINUE:
		return "SCAN_CONTINUE";
	case ATH12K_CBS_EVENT_DWELL_SPLIT:
		return "DWELL_SPLIT";
	case ATH12K_CBS_EVENT_SCAN_CANCEL:
		return "SCAN_CANCEL";
	case ATH12K_CBS_EVENT_SCAN_COMPLETE:
		return "SCAN_COMPLETE";
	case ATH12K_CBS_EVENT_SCAN_RESTART:
		return "SCAN_RESTART";
	}

	return "UNKNOWN";
}

int ath12k_cbs_init_dwell_params(struct ath12k_cbs *cbs)
{
	int ret = 0;
	u32 idx;

	if (!cbs)
		return -EINVAL;

	memset(cbs->scan_offset, 0, sizeof(cbs->scan_offset));
	memset(cbs->scan_dwell_rest, 0, sizeof(cbs->scan_dwell_rest));
	cbs->max_arr_size_used = 0;
	cbs->max_dwell_split_cnt = 0;
	cbs->dwell_split_cnt = 0;

	switch (cbs->dwell_split_time) {
	case ATH12K_CBS_DWELL_TIME_20MS:
		switch (cbs->cbs_total_dwell_time) {
		case ATH12K_CBS_TOTAL_DWELL_TIME_20MS:
			cbs->max_arr_size_used = 1;
			cbs->scan_offset[0] = 0;
			break;
		case ATH12K_CBS_TOTAL_DWELL_TIME_200MS:
			cbs->max_arr_size_used = 10;
			for (idx = 0; idx < cbs->max_arr_size_used; idx++)
				cbs->scan_offset[idx] = idx * 10;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case ATH12K_CBS_DWELL_TIME_25MS:
		switch (cbs->cbs_total_dwell_time) {
		case ATH12K_CBS_TOTAL_DWELL_TIME_25MS:
			cbs->max_arr_size_used = 1;
			cbs->scan_offset[0] = 0;
			break;
		case ATH12K_CBS_TOTAL_DWELL_TIME_100MS:
			cbs->max_arr_size_used = 4;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 25;
			cbs->scan_offset[2] = 50;
			cbs->scan_offset[3] = 75;
			break;
		case ATH12K_CBS_TOTAL_DWELL_TIME_200MS:
			cbs->max_arr_size_used = 8;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 25;
			cbs->scan_offset[2] = 50;
			cbs->scan_offset[3] = 75;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = 25;
			cbs->scan_offset[6] = 50;
			cbs->scan_offset[7] = 75;
			break;
		default:

			ret = -EINVAL;
		}
		break;
	case ATH12K_CBS_DWELL_TIME_50MS:
		switch (cbs->cbs_total_dwell_time) {
		case ATH12K_CBS_TOTAL_DWELL_TIME_50MS:
			cbs->max_arr_size_used = 1;
			cbs->scan_offset[0] = 0;
			break;
		case ATH12K_CBS_TOTAL_DWELL_TIME_100MS:
			cbs->max_arr_size_used = 2;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 50;
			break;
		case ATH12K_CBS_TOTAL_DWELL_TIME_200MS:
			cbs->max_arr_size_used = 4;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 50;
			cbs->scan_offset[2] = 0;
			cbs->scan_offset[3] = 50;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	if (ret) {
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: invalid total dwell %d for dwell %d",
			   cbs->cbs_total_dwell_time, cbs->dwell_split_time);
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: dwell 20ms can have total dwell of 20ms, 200ms");
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: dwell 25ms can have total dwell of 25ms, 100ms, 200ms");
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: dwell 50ms can have total dwell of 50ms, 100ms, 200ms");
		return ret;
	}

	cbs->dwell_split_cnt = cbs->max_arr_size_used ? cbs->max_arr_size_used - 1 : 0;
	cbs->max_dwell_split_cnt = cbs->dwell_split_cnt;
	/* Initialize scan_dwell_rest array.
	 * The first scan (idx=0) should NOT have a rest period before it.
	 * Rest periods apply AFTER each scan completes, not before.
	 */
	for (idx = 0; idx < ATH12K_CBS_SCAN_OFFSET_ARRAY_SIZE; idx++) {
		if (idx == 0)
			/* No rest before the first scan */
			cbs->scan_dwell_rest[idx] = 0;
		else if (idx < cbs->max_arr_size_used)
			/* Rest after each subsequent scan */
			cbs->scan_dwell_rest[idx] = cbs->rest_time;
		else
			cbs->scan_dwell_rest[idx] = 0;
	}

	if (cbs->ar)
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: dwell params init: dwell_split_time=%u cbs_total_dwell_time=%u max_arr_size_used=%u max_dwell_split_cnt=%u rest_time=%u\n",
			   cbs->dwell_split_time, cbs->cbs_total_dwell_time,
			   cbs->max_arr_size_used, cbs->max_dwell_split_cnt,
			   cbs->rest_time);

	return ret;
}

static void ath12k_cbs_set_state(struct ath12k_cbs *cbs,
				 enum ath12k_cbs_state state)
{
	struct ath12k *ar = cbs->ar;
	enum ath12k_cbs_state old_state = cbs->state;

	cbs->state = state;

	if (ar && old_state != state)
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN, "CBS: state %s -> %s\n",
			   ath12k_cbs_state_str(old_state),
			   ath12k_cbs_state_str(state));
}

static void ath12k_cbs_resume_after_wait(struct ath12k_cbs *cbs)
{
	enum ath12k_cbs_event event = 0;
	struct ath12k *ar = cbs->ar;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: resume_after_wait: state=%s dwell_split_cnt=%d max_dwell_split_cnt=%d\n",
		   ath12k_cbs_state_str(cbs->state), cbs->dwell_split_cnt,
		   cbs->max_dwell_split_cnt);

	switch (cbs->state) {
	case ATH12K_CBS_STATE_SCAN:
		ath12k_err(ar->ab, "CBS: Invalid state %s\n",
			   ath12k_cbs_state_str(cbs->state));
		event = ATH12K_CBS_EVENT_SCAN_CANCEL;
		break;
	case ATH12K_CBS_STATE_REST:
		if (cbs->dwell_split_cnt < cbs->max_dwell_split_cnt)
			event = ATH12K_CBS_EVENT_DWELL_SPLIT;
		else
			event = ATH12K_CBS_EVENT_SCAN_NEXT;
		break;
	case ATH12K_CBS_STATE_WAIT:
		event = ATH12K_CBS_EVENT_SCAN_START;
		break;
	default:
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN, "CBS: current state: %s, current event to post: %d, cbs_enable flag: %d\n",
		   ath12k_cbs_state_str(cbs->state), event, cbs->cbs_enable);

	ath12k_cbs_post_event(cbs, event);
}

static void ath12k_cbs_timer(struct timer_list *timer)
{
	struct ath12k_cbs *cbs = from_timer(cbs, timer, timer);
	struct ath12k *ar = cbs->ar;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: timer fired: state=%s scan.state=%d\n",
		   ath12k_cbs_state_str(cbs->state), ar->scan.state);

	if (ar->scan.state != ATH12K_SCAN_IDLE) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN, "CBS: mac80211 scan is in progress, CBS will resume on scan complete\n");
		return;
	}

	ath12k_cbs_resume_after_wait(cbs);
}

void ath12k_cbs_resume_post_mac80211_scan(struct ath12k *ar)
{
        struct ath12k_cbs *cbs = &ar->ar_extn.cbs;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: resume_post_mac80211_scan: state=%s\n",
		   ath12k_cbs_state_str(cbs->state));

	if (cbs->state == ATH12K_CBS_STATE_INIT)
		return;

	cbs->cbs_enable = 1;

	ath12k_cbs_resume_after_wait(cbs);
}

int ath12k_cbs_allow_mac80211_scan(struct ath12k *ar)
{
	struct ath12k_cbs *cbs = &ar->ar_extn.cbs;

	if (!cbs->cbs_enable)
		return 0;

	if (cbs->state == ATH12K_CBS_STATE_SCAN) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: mac80211 scan blocked: CBS scan in progress\n");
		return -EBUSY;
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: pausing CBS to allow mac80211 scan\n");
		cbs->cbs_enable = 0;
	}

	return 0;
}

static bool ath12k_cbs_is_urgent_event(enum ath12k_cbs_event event)
{
	return event == ATH12K_CBS_EVENT_SCAN_CANCEL;
}

static void ath12k_cbs_cancel(struct ath12k_cbs *cbs)
{
	struct ath12k *ar = cbs->ar;

	if (ar)
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: cancel: state=%s scan_count=%u cancel_count=%u complete_count=%u\n",
			   ath12k_cbs_state_str(cbs->state), cbs->scan_count,
			   cbs->cancel_count, cbs->complete_count);

	del_timer_sync(&cbs->timer);
	cbs->ar = NULL;
	cbs->arvif = NULL;
	cbs->scan_id = 0;
	cbs->chan_list_idx = 0;
	cbs->dwell_split_cnt = cbs->max_dwell_split_cnt;
	cbs->pending_event = 0;

	ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_INIT);
}

static void ath12k_cbs_complete(struct ath12k_cbs *cbs)
{
	cbs->complete_count++;

	if (cbs->ar)
		ath12k_dbg(cbs->ar->ab, ATH12K_DBG_SCAN,
			   "CBS: scan cycle complete: complete_count=%u cbs_enable=%d\n",
			   cbs->complete_count, cbs->cbs_enable);

	if (cbs->cbs_enable == ATH12K_CBS_RUN_ONCE) {
		cbs->cbs_enable = 0;
		ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_INIT);
	} else if (cbs->cbs_enable == ATH12K_CBS_RUN_CONTINUOUS) {
		ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_WAIT);
		mod_timer(&cbs->timer, jiffies + msecs_to_jiffies(cbs->wait_time));
	} else {
		ath12k_err(NULL, "CBS: invalid enable option: %d", cbs->cbs_enable);
		ath12k_cbs_cancel(cbs);
	}

}

static void ath12k_cbs_work(struct work_struct *work)
{
	struct ath12k_cbs *cbs = container_of(work, struct ath12k_cbs, work);
	enum ath12k_cbs_event event;
	struct ath12k *ar;

	spin_lock_bh(&cbs->lock);
	event = cbs->pending_event;
	cbs->pending_event = 0;
	spin_unlock_bh(&cbs->lock);

	if (!event)
		return;

	ar = cbs->ar;
	if (ar)
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: work: dispatching event %s in state %s\n",
			   ath12k_cbs_event_str(event),
			   ath12k_cbs_state_str(cbs->state));

	ath12k_cbs_event_handler(cbs, event);
}

static void ath12k_cbs_scan_work(struct work_struct *work)
{
	struct ath12k_cbs *cbs = container_of(work, struct ath12k_cbs,
					      cbs_scan_work);
	struct ath12k *ar = cbs->ar;

	if (!ar)
		return;
	if (!cbs->cbs_scan_event)
		return;

	ath12k_vendor_send_cbs_scan_event(ar, cbs->cbs_scan_event);
}

bool ath12k_is_cbs_vendor_event(struct ath12k *ar)
{
	if (!ar->ar_extn.cbs.cbs_enable)
		return false;

	return true;
}

bool ath12k_cbs_skip_scan_event_and_send_scan_complete(
		struct ath12k *ar,
		const struct wmi_scan_event *scan_ev)
{
	struct ath12k_cbs *cbs;
	u32 scan_req_id;
	u32 event_type;
	u32 reason;
	bool is_final_scan_offset = 0;

	if (!ar)
		return false;

	cbs = &ar->ar_extn.cbs;

	if (!scan_ev)
		return false;

	event_type = le32_to_cpu(scan_ev->event_type);
	reason = le32_to_cpu(scan_ev->reason);
	scan_req_id = le32_to_cpu(scan_ev->scan_req_id);

	spin_lock_bh(&ar->data_lock);

	if (!ath12k_is_cbs_vendor_event(ar)) {
		spin_unlock_bh(&ar->data_lock);
		return false;
	}

	ath12k_dbg(NULL, ATH12K_DBG_WMI,
		   "ath12k scan state machine for CBS vendor scan event type %d req_id %d vdev_id %d reason %d freq %d\n",
		   event_type, scan_req_id, le32_to_cpu(scan_ev->vdev_id),
		   reason, le32_to_cpu(scan_ev->channel_freq));

	if (!(le32_to_cpu(scan_ev->event_type) == WMI_SCAN_EVENT_COMPLETED &&
	    le32_to_cpu(scan_ev->reason) == WMI_SCAN_REASON_COMPLETED)) {
		/* No operation required if scan is not successful */
		spin_unlock_bh(&ar->data_lock);
		return true;
	}

	if (!cbs->cbs_scan_event) {
		cbs->cbs_scan_event = kzalloc(sizeof(*cbs->cbs_scan_event),
					      GFP_ATOMIC);
		if (!cbs->cbs_scan_event) {
			spin_unlock_bh(&ar->data_lock);
			return true;
		}
	}

	memcpy(cbs->cbs_scan_event, scan_ev, sizeof(*cbs->cbs_scan_event));

	is_final_scan_offset = (cbs->scan_offset_idx >=
				cbs->max_arr_size_used - 1);
	spin_unlock_bh(&ar->data_lock);

	if (is_final_scan_offset) {
		ath12k_dbg(NULL, ATH12K_DBG_WMI, "Scheduling workq for CBS to send scan completion to Application\n");
		schedule_work(&cbs->cbs_scan_work);
	}

	if (event_type == WMI_SCAN_EVENT_DEQUEUED ||
	    event_type == WMI_SCAN_EVENT_COMPLETED)
		ath12k_cbs_scan_event_handler(ar, scan_req_id, event_type);

	return true;
}



int ath12k_cbs_init(struct ath12k *ar)
{
	struct ath12k_cbs *cbs;

	if (!ar)
		return -EINVAL;

	cbs = &ar->ar_extn.cbs;
	memset(cbs, 0, sizeof(*cbs));

	cbs->ar = ar;
	cbs->state = ATH12K_CBS_STATE_INIT;
	spin_lock_init(&cbs->lock);
	timer_setup(&cbs->timer, ath12k_cbs_timer, 0);
	INIT_WORK(&cbs->work, ath12k_cbs_work);
	INIT_WORK(&cbs->cbs_scan_work, ath12k_cbs_scan_work);

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN, "CBS: initialized\n");

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_cbs_init);

void ath12k_cbs_deinit(struct ath12k *ar)
{
	struct ath12k_cbs *cbs;

	if (!ar)
		return;

	cbs = &ar->ar_extn.cbs;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: deinit: state=%s scan_count=%u cancel_count=%u complete_count=%u\n",
		   ath12k_cbs_state_str(cbs->state), cbs->scan_count,
		   cbs->cancel_count, cbs->complete_count);

	ath12k_cbs_stop(ar);
	cancel_work_sync(&cbs->work);
	cancel_work_sync(&cbs->cbs_scan_work);
	del_timer_sync(&cbs->timer);
	kfree(cbs->cbs_scan_event);
	cbs->cbs_scan_event = NULL;
}
EXPORT_SYMBOL_GPL(ath12k_cbs_deinit);

int ath12k_cbs_start(struct ath12k *ar)
{
	struct ath12k_cbs *cbs;

	if (!ar)
		return -EINVAL;

	cbs = &ar->ar_extn.cbs;
	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: start: state=%s cbs_enable=%d nchans=%u scan.state=%d\n",
		   ath12k_cbs_state_str(cbs->state), cbs->cbs_enable,
		   cbs->nchans, ar->scan.state);

	if (ar->scan.state != ATH12K_SCAN_IDLE) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: start deferred: mac80211 scan active (scan.state=%d)\n",
			   ar->scan.state);
		return -EBUSY;
	}

	spin_lock_bh(&cbs->lock);
	if (!cbs->ar)
		cbs->ar = ar;

	cbs->chan_list_idx = 0;
	cbs->scan_id = ATH12K_CBS_SCAN_ID;

	if (!cbs->cbs_enable) {
		spin_unlock_bh(&cbs->lock);
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: start: cbs_enable=0, stopping CBS\n");
		ath12k_cbs_stop(ar);
		return 0;
	}

	if (ath12k_cbs_init_dwell_params(cbs)) {
		spin_unlock_bh(&cbs->lock);
		return -EINVAL;
	}

	spin_unlock_bh(&cbs->lock);

	ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_START);

	return 0;
}
EXPORT_SYMBOL_GPL(ath12k_cbs_start);

void ath12k_cbs_stop(struct ath12k *ar)
{
	struct ath12k_wmi_scan_cancel_arg arg = {0};
	struct ath12k_cbs *cbs;
	int ret;

	if (!ar)
		return;

	cbs = &ar->ar_extn.cbs;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: stop: state=%s scan_id=0x%x scan_count=%u cancel_count=%u\n",
		   ath12k_cbs_state_str(cbs->state), cbs->scan_id,
		   cbs->scan_count, cbs->cancel_count);

	cbs->cbs_enable = 0;

	if (cbs->state != ATH12K_CBS_STATE_INIT) {
		arg.req_type = WLAN_SCAN_CANCEL_SINGLE;
		arg.scan_id = cbs->scan_id ? cbs->scan_id : ATH12K_SCAN_ID;
		arg.pdev_id = ar->pdev ? ar->pdev->pdev_id : 0;
		del_timer_sync(&cbs->timer);
		spin_lock_bh(&cbs->lock);
		cbs->pending_event = ATH12K_CBS_EVENT_SCAN_CANCEL;
		spin_unlock_bh(&cbs->lock);
		ret = ath12k_wmi_send_scan_stop_cmd(ar, &arg);
		if (ret)
			ath12k_warn(ar->ab, "CBS: failed to stop active scan: %d\n", ret);
		ath12k_cbs_cancel(cbs);
	}
}

void ath12k_cbs_stop_async(struct ath12k *ar)
{
	struct ath12k_cbs *cbs;

	if (!ar)
		return;

	cbs = &ar->ar_extn.cbs;
	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: stop_async: state=%s\n",
		   ath12k_cbs_state_str(cbs->state));

	if (cbs->state != ATH12K_CBS_STATE_INIT)
		ath12k_cbs_stop(ar);
}

void ath12k_cbs_post_event(struct ath12k_cbs *cbs, enum ath12k_cbs_event event)
{
	struct ath12k *ar;

	if (!cbs || !event)
		return;

	ar = cbs->ar;
	spin_lock_bh(&cbs->lock);
	if (cbs->pending_event) {
		if (ar) {
			ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
				   "CBS: drop event %s while %s pending\n",
				   ath12k_cbs_event_str(event),
				   ath12k_cbs_event_str(cbs->pending_event));
		}
		spin_unlock_bh(&cbs->lock);
		return;
	}

	if (!cbs->ar && !ath12k_cbs_is_urgent_event(event)) {
		if (ar)
			ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
				   "CBS: ignore event %s without vif\n",
				   ath12k_cbs_event_str(event));
		spin_unlock_bh(&cbs->lock);
		return;
	}

	cbs->pending_event = event;
	schedule_work(&cbs->work);
	spin_unlock_bh(&cbs->lock);
}

int ath12k_cbs_event_handler(struct ath12k_cbs *cbs,
			     enum ath12k_cbs_event event)
{
	struct ath12k *ar;
	enum ath12k_cbs_state cur_state;
	int ret = 0;

	if (!cbs)
		return -EINVAL;

	ar = cbs->ar;
	cur_state = cbs->state;

	if (ar)
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN, "CBS: state %s event %s\n",
			   ath12k_cbs_state_str(cur_state),
			   ath12k_cbs_event_str(event));

	switch (event) {
	case ATH12K_CBS_EVENT_SCAN_START:
		switch (cur_state) {
		case ATH12K_CBS_STATE_INIT:
		case ATH12K_CBS_STATE_WAIT:
			cbs->chan_list_idx = 0;
			if (!cbs->max_arr_size_used) {
				if (ar) {
					ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
						   "CBS: SCAN_START failed: max_arr_size_used=0 (invalid dwell params)\n");
					ath12k_cbs_stop(ar);
				}
				ret = -EINVAL;
				break;
			}
			cbs->scan_offset_idx = 0;
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_SCAN);
			cbs->scan_count++;
			cbs->dwell_split_cnt = 0;
			cbs->scan_offset_idx = 0;
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_START: scan_count=%u chan_list_idx=%u nchans=%u\n",
					   cbs->scan_count, cbs->chan_list_idx, cbs->nchans);
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		default:
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_START ignored: already in state %s\n",
					   ath12k_cbs_state_str(cur_state));
			ret = -EALREADY;
			break;
		}
		break;
	case ATH12K_CBS_EVENT_SCAN_NEXT:
		switch (cur_state) {
		case ATH12K_CBS_STATE_SCAN:
			cbs->chan_list_idx++;
			cbs->dwell_split_cnt = 0;
			cbs->scan_offset_idx = 0;
			if (cbs->chan_list_idx >= cbs->nchans) {
				if (ar)
					ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
						   "CBS: SCAN_NEXT: all %u channels scanned, completing cycle\n",
						   cbs->nchans);
				ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_COMPLETE);
				break;
			}
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_NEXT: chan_list_idx=%u/%u\n",
					   cbs->chan_list_idx, cbs->nchans);
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		case ATH12K_CBS_STATE_REST:
			cbs->chan_list_idx++;
			cbs->dwell_split_cnt = 0;
 			cbs->scan_offset_idx = 0;
 			if (cbs->chan_list_idx >= cbs->nchans) {
				if (ar)
					ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
						   "CBS: SCAN_NEXT (from REST): all %u channels scanned, completing cycle\n",
						   cbs->nchans);
				ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_COMPLETE);
				break;
			}
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_NEXT (from REST): chan_list_idx=%u/%u\n",
					   cbs->chan_list_idx, cbs->nchans);
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_SCAN);
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		default:
			break;
		}
		break;
	case ATH12K_CBS_EVENT_DWELL_SPLIT:
		switch (cur_state) {
		case ATH12K_CBS_STATE_SCAN:
			if (cbs->scan_offset_idx >= cbs->max_arr_size_used)
				cbs->scan_offset_idx = 0;
			if (cbs->scan_offset_idx >= ATH12K_CBS_SCAN_OFFSET_ARRAY_SIZE) {
				if (ar) {
					ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
						   "CBS: DWELL_SPLIT: scan_offset_idx=%u out of bounds\n",
						   cbs->scan_offset_idx);
					ath12k_cbs_stop(ar);
				}
				ret = -EINVAL;
				break;
			}
			if (cbs->scan_dwell_rest[cbs->scan_offset_idx]) {
				if (ar)
					ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
						   "CBS: DWELL_SPLIT: resting %ums before next split (offset_idx=%u)\n",
						   cbs->scan_dwell_rest[cbs->scan_offset_idx],
						   cbs->scan_offset_idx);
				ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_REST);
				mod_timer(&cbs->timer,
					  jiffies + msecs_to_jiffies(cbs->scan_dwell_rest[cbs->scan_offset_idx]));
				break;
			}
			if (cbs->dwell_split_cnt < cbs->max_dwell_split_cnt)
				cbs->dwell_split_cnt++;
			cbs->scan_offset_idx++;
			if (cbs->scan_offset_idx >= cbs->max_arr_size_used) {
				cbs->scan_offset_idx = 0;
				if (cbs->dwell_split_cnt >= cbs->max_dwell_split_cnt) {
					if (ar)
						ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
							   "CBS: DWELL_SPLIT: all splits done (dwell_split_cnt=%d), moving to SCAN_NEXT\n",
							   cbs->dwell_split_cnt);
					ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_NEXT);
					break;
				}
			}
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: DWELL_SPLIT: submitting split scan offset_idx=%u dwell_split_cnt=%d/%d\n",
					   cbs->scan_offset_idx, cbs->dwell_split_cnt,
					   cbs->max_dwell_split_cnt);
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		case ATH12K_CBS_STATE_REST:
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_SCAN);
			cbs->scan_offset_idx++;
			if (cbs->scan_offset_idx >= cbs->max_arr_size_used)
				cbs->scan_offset_idx = 0;
			if (cbs->dwell_split_cnt < cbs->max_dwell_split_cnt)
				cbs->dwell_split_cnt++;
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: DWELL_SPLIT (from REST): offset_idx=%u dwell_split_cnt=%d/%d\n",
					   cbs->scan_offset_idx, cbs->dwell_split_cnt,
					   cbs->max_dwell_split_cnt);
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		default:
			break;
		}
		break;
	case ATH12K_CBS_EVENT_SCAN_CONTINUE:
		switch (cur_state) {
		case ATH12K_CBS_STATE_REST:
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_SCAN);
			cbs->scan_offset_idx = 0;
			if (ar)
				ret = ath12k_cbs_submit_scan(ar, cbs);
			break;
		default:
			break;
		}
		break;
	case ATH12K_CBS_EVENT_SCAN_CANCEL:
		if (cur_state != ATH12K_CBS_STATE_INIT) {
			cbs->cancel_count++;
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_CANCEL: cancel_count=%u\n",
					   cbs->cancel_count);
			ath12k_cbs_cancel(cbs);
		}
		break;
	case ATH12K_CBS_EVENT_SCAN_COMPLETE:
		if (ar)
			ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
				   "CBS: SCAN_COMPLETE: scan_count=%u complete_count=%u\n",
				   cbs->scan_count, cbs->complete_count + 1);
		ath12k_cbs_complete(cbs);
		break;
	case ATH12K_CBS_EVENT_SCAN_RESTART:
		if (cur_state != ATH12K_CBS_STATE_INIT) {
			if (ar)
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: SCAN_RESTART: scheduling restart in %dms\n",
					   cbs->wait_time);
			ath12k_cbs_cancel(cbs);
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_WAIT);
			mod_timer(&cbs->timer,
				  jiffies + msecs_to_jiffies(cbs->wait_time));
		}
		break;
	default:
		if (ar) {
			ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
				   "CBS: unknown event %d in state %s\n",
				   event, ath12k_cbs_state_str(cur_state));
			ath12k_cbs_stop(ar);
		}
		ret = -EINVAL;
		break;
	}

	return ret;
}

void ath12k_cbs_scan_event_handler(struct ath12k *ar, u32 scan_id,
				   u32 event_type)
{
	struct ath12k_cbs *cbs;

	if (!ar)
		return;

	cbs = &ar->ar_extn.cbs;

	if (scan_id && cbs->scan_id && scan_id != cbs->scan_id) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: scan_event_handler: ignoring scan_id=0x%x (expected 0x%x)\n",
			   scan_id, cbs->scan_id);
		return;
	}

	switch (event_type) {
	case WMI_SCAN_EVENT_COMPLETED:
		if (cbs->state == ATH12K_CBS_STATE_SCAN && cbs->dwell_split_cnt < cbs->max_dwell_split_cnt) {
			if (cbs->min_dwell_rest_time) {
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: WMI_SCAN_COMPLETED: dwell split rest %ums (dwell_split_cnt=%d/%d)\n",
					   cbs->min_dwell_rest_time, cbs->dwell_split_cnt,
					   cbs->max_dwell_split_cnt);
				ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_REST);
				mod_timer(&cbs->timer,
					  jiffies + msecs_to_jiffies(cbs->min_dwell_rest_time));
			} else {
				ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
					   "CBS: WMI_SCAN_COMPLETED: posting DWELL_SPLIT (dwell_split_cnt=%d/%d)\n",
					   cbs->dwell_split_cnt, cbs->max_dwell_split_cnt);
				ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_DWELL_SPLIT);
			}
		} else if (cbs->state == ATH12K_CBS_STATE_SCAN) {
			ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
				   "CBS: WMI_SCAN_COMPLETED: resting %ums before next channel (chan_list_idx=%u/%u)\n",
				   cbs->rest_time, cbs->chan_list_idx, cbs->nchans);
			ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_REST);
			mod_timer(&cbs->timer,
				  jiffies + msecs_to_jiffies(cbs->rest_time));
		}
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: WMI_SCAN_DEQUEUED: posting SCAN_RESTART\n");
		ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_RESTART);
		break;
	default:
		break;
	}
}

static int ath12k_cbs_submit_scan(struct ath12k *ar, struct ath12k_cbs *cbs)
{
	struct ath12k_wmi_scan_req_arg *scan_arg;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	int ret;

	if (!ar || !cbs || !cbs->arvif)
		return -EINVAL;

	if (!cbs->max_arr_size_used)
		cbs->max_arr_size_used = 1;

	if (cbs->scan_offset_idx >= cbs->max_arr_size_used)
		return -EINVAL;

	if (!cbs->nchans || cbs->chan_list_idx >= cbs->nchans)
		return -EINVAL;

	arvif = cbs->arvif;
	if (!arvif->ahvif || !arvif->ahvif->vif)
		return -EINVAL;

	vif = arvif->ahvif->vif;
	if (!vif)
		return -EINVAL;

	scan_arg = kzalloc(sizeof(*scan_arg), GFP_KERNEL);
	if (!scan_arg)
		return -ENOMEM;

	ath12k_wmi_start_scan_init(ar, scan_arg, vif->type);
	scan_arg->scan_id = ATH12K_CBS_SCAN_ID;
	scan_arg->scan_req_id = ATH12K_CBS_SCAN_ID;
	scan_arg->vdev_id = arvif->vdev_id;
	scan_arg->pdev_id = ar->pdev ? ar->pdev->pdev_id : 0;
	scan_arg->notify_scan_events = WMI_SCAN_EVENT_STARTED | WMI_SCAN_EVENT_COMPLETED;
	scan_arg->min_rest_time = 0;
	scan_arg->max_rest_time = 0;
	scan_arg->idle_time = 0;

	scan_arg->dwell_time_active = cbs->dwell_split_time;
	scan_arg->dwell_time_active_2g = cbs->dwell_split_time;
	scan_arg->dwell_time_active_6g = cbs->dwell_split_time;
	scan_arg->dwell_time_passive = cbs->dwell_split_time;
	scan_arg->dwell_time_passive_6g = cbs->dwell_split_time;
	scan_arg->burst_duration = cbs->dwell_split_time;

	scan_arg->extn.scan_start_offset = cbs->scan_offset[cbs->scan_offset_idx];
	scan_arg->chan_list.num_chan = 1;
	scan_arg->chan_list.chan = kcalloc(1, sizeof(struct chan_info), GFP_KERNEL);
	if (!scan_arg->chan_list.chan) {
		ret = -ENOMEM;
		goto out_free;
	}

	scan_arg->chan_list.chan[0].freq = cbs->chan_list[cbs->chan_list_idx];
	if (!scan_arg->chan_list.chan[0].freq)
		scan_arg->chan_list.chan[0].freq = 2412;
	scan_arg->chan_list.chan[0].phymode = MODE_11BE_EHT20;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: submit_scan: vdev_id=%u freq=%u offset=%ums offset_idx=%u chan_list_idx=%u/%u dwell_split_cnt=%d/%d dwell_time=%d\n",
		   scan_arg->vdev_id, scan_arg->chan_list.chan[0].freq,
		   scan_arg->extn.scan_start_offset, cbs->scan_offset_idx,
		   cbs->chan_list_idx, cbs->nchans,
		   cbs->dwell_split_cnt, cbs->max_dwell_split_cnt, cbs->dwell_split_time);

	ret = ath12k_wmi_send_scan_start_cmd(ar, scan_arg);

	if (ret)
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "CBS: submit_scan failed: ret=%d\n", ret);

out_free:
	kfree(scan_arg->chan_list.chan);
	kfree(scan_arg);
	return ret;
}

void ath12k_cbs_recovery_handler(struct ath12k *ar)
{
	struct ath12k_cbs *cbs = &ar->ar_extn.cbs;

	if (!cbs->cbs_enable || !cbs->arvif || cbs->state == ATH12K_CBS_STATE_INIT)
		return;

	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "CBS: recovery_handler: state=%s chan_list_idx=%u/%u is_up=%d\n",
		   ath12k_cbs_state_str(cbs->state), cbs->chan_list_idx,
		   cbs->nchans, cbs->arvif->is_up);

	/* The scan/timer submitted to the previous FW instance is gone;
	 * the WMI scan event that would have driven the state machine
	 * forward will never arrive.
	 */
	del_timer_sync(&cbs->timer);
	spin_lock_bh(&cbs->lock);
	cbs->pending_event = 0;
	spin_unlock_bh(&cbs->lock);

	/* If the link CBS was running on didn't come back up after
	 * recovery, there is nothing to resume onto - reset CBS instead.
	 */
	if (!cbs->arvif->is_up) {
		ath12k_cbs_stop(ar);
		return;
	}

	cbs->ar = ar;
	cbs->scan_id = ATH12K_CBS_SCAN_ID;

	/* Restart the full scan cycle from channel 0.  Resuming from
	 * chan_list_idx would produce a partial ACS report (channels
	 * before the SSR point were never scanned by the new FW instance).
	 * SCAN_START resets chan_list_idx, dwell_split_cnt and
	 * scan_offset_idx via the event handler, so do not preset them here.
	 */
	ath12k_cbs_set_state(cbs, ATH12K_CBS_STATE_INIT);
	ath12k_cbs_post_event(cbs, ATH12K_CBS_EVENT_SCAN_START);
}
EXPORT_SYMBOL_GPL(ath12k_cbs_recovery_handler);
