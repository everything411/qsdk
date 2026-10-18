// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "peer.h"
#include "htc.h"
#include "dp_htt.h"
#include "debugfs_htt_stats.h"
#include "debugfs_sta.h"
#include "debugfs.h"
#include "debug.h"
#include "dp_mon.h"
#include "dp_mon_filter.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ini.h"
#endif
#include "telemetry_agent_if.h"
#include "ath12k_notif.h"

static void
ath12k_dp_fill_txrate_gi(struct rate_info *txrate, u8 preamble, u8 gi)
{
	switch (preamble) {
	case WMI_RATE_PREAMBLE_HT:
	case WMI_RATE_PREAMBLE_VHT:
		if (gi == HTT_PPDU_STATS_SGI_0_4_US)
			txrate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case WMI_RATE_PREAMBLE_HE:
		txrate->he_gi = ath12k_he_gi_to_nl80211_he_gi(gi);
		break;
	case WMI_RATE_PREAMBLE_EHT:
		txrate->eht_gi = ath12k_eht_gi_to_nl80211_eht_gi(gi);
		break;
	case WMI_RATE_PREAMBLE_UHR:
		txrate->eht_gi = ath12k_eht_gi_to_nl80211_eht_gi(gi);
		break;
	default:
		break;
	}
}

/**
 * ath12k_htt_send() - Send htt packet from host
 * @ab : ath12k base handle
 * @dp : HTT DP handle
 * @skb: skb to be sent
 * @msg_type : command to be recorded in dp htt logger
 * @msg_data : Pointer to buffer needs to be recorded for above cmd
 *
 * Return: status code
 */
static inline int ath12k_htt_send(struct ath12k_base *ab,
				  struct ath12k_dp *dp,
				  struct sk_buff *skb,
				  u8 msg_type,
				  u8 *msg_data)
{
	int ret;

	if (!ab->ag->wsi_remap_in_progress && ab->is_bypassed)
		return -EPERM;

	ath12k_dp_htt_message_record(dp->htt_logger_handle, msg_type,
				     msg_data, HTT_LOGGER_COMMAND);
	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);

	return ret;
}

void ath12k_dp_htt_htc_tx_complete(struct ath12k_base *ab, struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ath12k_dp_htt_htc_tx_complete);

int ath12k_dp_htt_connect(struct ath12k_dp *dp)
{
	struct ath12k_htc_svc_conn_req conn_req = {0};
	struct ath12k_htc_svc_conn_resp conn_resp = {0};
	int status;
	struct ath12k_base *ab = dp->ab;
	bool htt_logging_enable;

	conn_req.ep_ops.ep_tx_complete = ath12k_dp_htt_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_dp_htt_htc_t2h_msg_handler;

	/* connect to control service */
	conn_req.service_id = ATH12K_HTC_SVC_ID_HTT_DATA_MSG;

	ab->htt_flag = true;
	status = ath12k_htc_connect_service(&dp->ab->htc, &conn_req,
					    &conn_resp);

	if (status)
		return status;

	dp->eid = conn_resp.eid;

	if (dp->arch_ops->dp_msdu_htt_connect) {
		status = dp->arch_ops->dp_msdu_htt_connect(dp);
		if (status)
			return status;
	}

	htt_logging_enable = DP_HTT_LOGGING_ENABLE;

	if (htt_logging_enable) {
		ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "HTT logging enabled via INI\n");
		ath12k_dp_htt_logging_init(&dp->htt_logger_handle, ab);
		if (!dp->htt_logger_handle)
			ath12k_warn(ab, "HTT logging initialization failed\n");
	} else {
		ath12k_info(ab, "HTT logging disabled via INI configuration\n");
		dp->htt_logger_handle = NULL;
	}

	return 0;
}

static int ath12k_get_ppdu_user_index(struct htt_ppdu_stats *ppdu_stats,
				      u16 peer_id)
{
	int i;

	for (i = 0; i < HTT_PPDU_STATS_MAX_USERS; i++) {
		if (ppdu_stats->user_stats[i].is_valid_peer_id) {
			if (peer_id == ppdu_stats->user_stats[i].peer_id)
				return i;
		} else {
			return i;
		}
	}

	return -EINVAL;
}

void
ath12k_dp_ppdu_stats_flush_tlv_parse(struct ath12k_base *ab,
				     struct ath12k_pdev_dp *dp_pdev,
				     struct htt_ppdu_stats_cmpltn_flush *msg,
				     struct htt_ppdu_stats_info *ppdu_info)
{
	struct ieee80211_rate_status status_rate = { 0 };
	struct ath12k_dp_link_peer *peer = NULL;
	struct ieee80211_tx_status status;
	struct ath12k_dp *dp = ab->dp;
	struct rate_info rate;
	u8 pdev_id;
	u16 sw_peer_id;

	pdev_id = ppdu_info->pdev_id;

	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id - 1);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	sw_peer_id = le16_to_cpu(msg->sw_peer_id);
	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, sw_peer_id);
	if (unlikely(!peer || !ath12k_dp_link_peer_get_sta(peer))) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   sw_peer_id);
		rcu_read_unlock();
		return;
	}

	if (ath12k_dp_link_peer_get_vif_type(peer) != NL80211_IFTYPE_MESH_POINT) {
		rcu_read_unlock();
		return;
	}

	if (ether_addr_equal(peer->addr, ath12k_dp_link_peer_get_vif(peer)->addr)) {
		rcu_read_unlock();
		return;
	}

	memset(&status, 0, sizeof(status));
	status.sta = ath12k_dp_link_peer_get_sta(peer);
	rate = peer->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;
	status.mpdu_fail = HTT_PPDU_STATS_NUM_MPDU(msg->info);
	ieee80211s_update_metric_ppdu(ath12k_dp_pdev_to_hw(dp_pdev), &status);

	rcu_read_unlock();
}

static void ath12k_copy_to_delay_stats(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_link_peer *peer,
				       struct htt_ppdu_user_stats *usr_stats,
				       struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_htt_ppdu_stats *ppdu_list_stats;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);
	ppdu_list_stats = &dp_pdev->stats.ppdu_list_stats;
	if (peer->delayba_flag) {
		ath12k_dbg_level(dp_pdev->dp->ab, ATH12K_DBG_TELEMETRY, ATH12K_DBG_L3,
				 "BA not yet recv for prev delayed ppdu[%d] - cur ppdu[%d]",
				 peer->last_delayed_ba_ppduid, ppdu_info->ppdu_id);
		ppdu_list_stats->delayed_ba_not_recvd++;
	}

	peer->ppdu_stats_delayba.sw_peer_id = le16_to_cpu(usr_stats->rate.sw_peer_id);
	peer->ppdu_stats_delayba.info0 = le32_to_cpu(usr_stats->rate.info0);
	peer->ppdu_stats_delayba.ru_end = le16_to_cpu(usr_stats->rate.ru_end);
	peer->ppdu_stats_delayba.ru_start = le16_to_cpu(usr_stats->rate.ru_start);
	peer->ppdu_stats_delayba.info1 = le32_to_cpu(usr_stats->rate.info1);
	peer->ppdu_stats_delayba.rate_flags = le32_to_cpu(usr_stats->rate.rate_flags);
	peer->ppdu_stats_delayba.resp_rate_flags =
		le32_to_cpu(usr_stats->rate.resp_rate_flags);

	peer->delayba_flag = true;
}

static void ath12k_copy_to_bar(struct ath12k_dp_link_peer *peer,
			       struct htt_ppdu_user_stats *usr_stats)
{
	usr_stats->rate.sw_peer_id = cpu_to_le16(peer->ppdu_stats_delayba.sw_peer_id);
	usr_stats->rate.info0 = cpu_to_le32(peer->ppdu_stats_delayba.info0);
	usr_stats->rate.ru_end = cpu_to_le16(peer->ppdu_stats_delayba.ru_end);
	usr_stats->rate.ru_start = cpu_to_le16(peer->ppdu_stats_delayba.ru_start);
	usr_stats->rate.info1 = cpu_to_le32(peer->ppdu_stats_delayba.info1);
	usr_stats->rate.rate_flags = cpu_to_le32(peer->ppdu_stats_delayba.rate_flags);
	usr_stats->rate.resp_rate_flags =
		cpu_to_le32(peer->ppdu_stats_delayba.resp_rate_flags);

	peer->delayba_flag = false;
}

static int
ath12k_dp_htt_process_stats_sch_cmd_status_tlv(struct ath12k_pdev_dp *dp_pdev,
					       const u32 *tlv_desc,
					       struct htt_ppdu_stats_info *ppdu_info)
{
	struct htt_ppdu_user_stats *usr_stats;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_link_peer *peer;
	u32 tlv_bitmap;
	u8 num_users;
	u16 peer_id;
	int i;

	num_users = ppdu_info->ppdu_stats.common.num_users;

	if (unlikely(num_users > ppdu_info->max_users)) {
		ath12k_warn(dp->ab,
			    "HTT PPDU STATS event has unexpected num_users %u, should be smaller than %u\n",
			    ppdu_info->ppdu_stats.common.num_users,
			    ppdu_info->max_users);
		return -EINVAL;
	}

	ppdu_info->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_SCH_CMD_STATUS);

	/* back up data rate tlv for all peers */
	if (ppdu_info->frame_type == HTT_STATS_PPDU_FTYPE_DATA &&
	    ppdu_info->delay_ba) {
		for (i = 0; i < num_users; i++) {
			peer_id = ppdu_info->ppdu_stats.user_stats[i].peer_id;
			tlv_bitmap = ppdu_info->ppdu_stats.user_stats[i].tlv_bitmap;
			if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMMON)))
				continue;

			rcu_read_lock();
			spin_lock_bh(&dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
									peer_id);
			if (!peer) {
				spin_unlock_bh(&dp->dp_lock);
				rcu_read_unlock();
				continue;
			}

			usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
			if (usr_stats->delay_ba) {
				ath12k_copy_to_delay_stats(dp_pdev, peer,
							   usr_stats,
							   ppdu_info);
				peer->last_delayed_ba_ppduid = ppdu_info->ppdu_id;
			}
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
		}
	}

	/* restore all peers' data rate tlv to mu-bar tlv */
	if (ppdu_info->frame_type == HTT_STATS_PPDU_FTYPE_BAR) {
		for (i = 0; i < ppdu_info->bar_num_users; i++) {
			peer_id = ppdu_info->ppdu_stats.user_stats[i].peer_id;
			tlv_bitmap = ppdu_info->ppdu_stats.user_stats[i].tlv_bitmap;

			if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMMON)))
				continue;

			rcu_read_lock();
			spin_lock_bh(&dp->dp_lock);
			peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev,
									peer_id);
			if (!peer) {
				spin_unlock_bh(&dp->dp_lock);
				rcu_read_unlock();
				continue;
			}

			usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
			if (usr_stats->cmpltn_cmn.status !=
					HTT_PPDU_STATS_USER_STATUS_OK) {
				spin_unlock_bh(&dp->dp_lock);
				rcu_read_unlock();
				continue;
			}

			if (peer->delayba_flag)
				ath12k_copy_to_bar(peer, usr_stats);
			spin_unlock_bh(&dp->dp_lock);
			rcu_read_unlock();
		}
	}

	return 0;
}

int
ath12k_dp_htt_process_stats_common_tlv(const u32 *tlv_desc,
				       struct htt_ppdu_stats_info *ppdu_info)
{
	u32 frame_type;
	struct htt_ppdu_stats_common *common;

	common = (struct htt_ppdu_stats_common *)tlv_desc;

	if (unlikely(common->num_users > ppdu_info->max_users))
		return -EINVAL;

	memcpy(&ppdu_info->ppdu_stats.common, tlv_desc,
	       sizeof(struct htt_ppdu_stats_common));

	frame_type = HTT_PPDU_STATS_CMN_GET_FTYPE(
			ppdu_info->ppdu_stats.common.flags);

	switch (frame_type) {
	case HTT_STATS_FTYPE_TIDQ_DATA_SU:
	case HTT_STATS_FTYPE_TIDQ_DATA_MU:
		if (HTT_STATS_GET_FRAME_CTRL_TYPE(ppdu_info->frame_ctrl) <=
				HTT_STATS_FRAME_CTRL_TYPE_CTRL)
			ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_CTRL;
		else
			ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_DATA;
		break;
	case HTT_STATS_FTYPE_SGEN_MU_BAR:
	case HTT_STATS_FTYPE_SGEN_BAR:
	case HTT_STATS_FTYPE_SGEN_BE_MU_BAR:
	case HTT_STATS_FTYPE_SGEN_BN_MU_BAR:
		ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_BAR;
		break;
	default:
		ppdu_info->frame_type = HTT_STATS_PPDU_FTYPE_CTRL;
		break;
	}
	ppdu_info->htt_frame_type = frame_type;
	ppdu_info->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_COMMON);

	return 0;
}

int
ath12k_dp_htt_process_usr_rate_stats(const u32 *tlv_desc,
				     struct htt_ppdu_stats_info *ppdu_info)
{
	const struct htt_ppdu_stats_user_rate *user_rate;
	struct htt_ppdu_user_stats *user_stats;
	int cur_user;
	u16 peer_id;

	user_rate = (struct htt_ppdu_stats_user_rate *)tlv_desc;
	peer_id = le16_to_cpu(user_rate->sw_peer_id);
	cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats, peer_id);

	if (unlikely(cur_user < 0))
		return -EINVAL;

	user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
	user_stats->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_USR_RATE);
	user_stats->peer_id = peer_id;
	user_stats->is_valid_peer_id = true;
	memcpy(&user_stats->rate, tlv_desc,
	       sizeof(struct htt_ppdu_stats_user_rate));

	return 0;
}

int
dp_process_ppdu_stats_usr_cmpltn_common_tlv(const u32 *tlv_desc,
					    struct htt_ppdu_stats_info *ppdu_info)
{
	const struct htt_ppdu_stats_usr_cmpltn_cmn *cmplt_cmn;
	struct htt_ppdu_user_stats *user_stats;
	int cur_user;
	u16 peer_id;

	cmplt_cmn = (struct htt_ppdu_stats_usr_cmpltn_cmn *)tlv_desc;
	peer_id = le16_to_cpu(cmplt_cmn->sw_peer_id);
	cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
					      peer_id);
	if (unlikely(cur_user < 0))
		return -EINVAL;

	user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
	user_stats->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON);
	user_stats->peer_id = peer_id;
	user_stats->is_valid_peer_id = true;
	memcpy(&user_stats->cmpltn_cmn, tlv_desc,
	       sizeof(struct htt_ppdu_stats_usr_cmpltn_cmn));
	ppdu_info->bar_num_users++;

	return 0;
}

int
ath12k_dp_htt_process_usr_compltn_ack_ba_stats(const u32 *tlv_desc,
					       struct htt_ppdu_stats_info *ppdu_info)
{
	const struct htt_ppdu_stats_usr_cmpltn_ack_ba_status *ba_status;
	struct htt_ppdu_user_stats *user_stats;
	int cur_user;
	u16 peer_id;
	u32 ppdu_id;

	ba_status = (struct htt_ppdu_stats_usr_cmpltn_ack_ba_status *)tlv_desc;
	ppdu_id = le32_to_cpu(ba_status->ppdu_id);
	peer_id = le16_to_cpu(ba_status->sw_peer_id);
	cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats,
					      peer_id);
	if (unlikely(cur_user < 0))
		return -EINVAL;

	user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
	user_stats->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS);
	user_stats->peer_id = peer_id;
	user_stats->is_valid_peer_id = true;
	ppdu_info->ppdu_id = HTT_PPDU_STATS_GET_PPDU_ID(ppdu_id);
	ppdu_info->ack_ba_done++;
	memcpy(&user_stats->ack_ba, tlv_desc,
	       sizeof(struct htt_ppdu_stats_usr_cmpltn_ack_ba_status));

	return 0;
}

int
ath12k_dp_htt_process_usr_cmn_stats(const u32 *tlv_desc,
				    struct htt_ppdu_stats_info *ppdu_info)
{
	struct htt_ppdu_user_stats *user_stats;
	struct htt_ppdu_stats_user_common *usr_cmn;
	int cur_user;
	u16 peer_id;

	usr_cmn = (struct htt_ppdu_stats_user_common *)tlv_desc;

	peer_id = le16_to_cpu(usr_cmn->sw_peer_id);
	cur_user = ath12k_get_ppdu_user_index(&ppdu_info->ppdu_stats, peer_id);

	if (unlikely(cur_user < 0))
		return -EINVAL;

	user_stats = &ppdu_info->ppdu_stats.user_stats[cur_user];
	user_stats->tlv_bitmap |= BIT(HTT_PPDU_STATS_TAG_USR_COMMON);
	memcpy(&user_stats->common, tlv_desc,
	       sizeof(struct htt_ppdu_stats_user_common));
	ppdu_info->frame_ctrl = HTT_PPDU_STATS_GET_FRAME_CTRL(user_stats->common.ctrl);
	user_stats->delay_ba = HTT_PPDU_STATS_GET_DELAY_BA(user_stats->common.info);
	ppdu_info->delay_ba = user_stats->delay_ba;

	return 0;
}

void
ath12k_dp_ppdu_stats_flush_tlv_parse_update(struct ath12k_pdev_dp *dp_pdev,
					    struct htt_ppdu_stats_cmpltn_flush *msg,
					    struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_dp_link_peer *peer;
	u16 sw_peer_id, num_msdu, num_mpdu;
	u32 drop_reason;
	u8 tid;

	rcu_read_lock();
	sw_peer_id = le16_to_cpu(msg->sw_peer_id);
	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							sw_peer_id);
	if (unlikely(!peer)) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			   sw_peer_id);
		rcu_read_unlock();
		return;
	}

	num_msdu = HTT_PPDU_STATS_FLUSH_GET_NUM_MSDU(msg->info);
	tid = msg->tid_num;
	drop_reason = __le32_to_cpu(msg->drop_reason);

	if (drop_reason >= HTT_FLUSH_MAX) {
		ath12k_err(dp_pdev->dp->ab, "Invalid drop_reason: %d", drop_reason);
		rcu_read_unlock();
		return;
	}

	if (tid >= ATH12K_DSCP_PRIORITY) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DATA, "Invalid tid: %d", tid);
		rcu_read_unlock();
		return;
	}

	if (peer->peer_stats.tx_ppdu_stats &&
	    ath12k_dp_advance_stats_enabled(dp_pdev)) {
		if (drop_reason == HTT_FLUSH_EXCESS_RETRIES)
			DP_STATS_INCR(peer->peer_stats.tx_ppdu_stats,
				      excess_retries_per_ac[TID_TO_WME_AC(tid)],
				      num_msdu);

		DP_STATS_INCR(peer->peer_stats.tx_ppdu_stats,
			      tx_msdu_flush_rsn[drop_reason],
			      num_msdu);
	}

	num_mpdu = HTT_PPDU_STATS_FLUSH_GET_NUM_MPDU(msg->info);
	peer->tx_retry_failed += num_mpdu;

	if (ath12k_extd_tx_stats_enabled(dp_pdev))
		ath12k_debugfs_sta_update_failure(peer, num_msdu);

	rcu_read_unlock();
}

int
ath12k_dp_htt_process_usr_compltn_flush(struct ath12k_pdev_dp *dp_pdev,
					const u32 *tlv_desc,
					struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_base *ab = dp_pdev->dp->ab;
	struct htt_ppdu_stats_cmpltn_flush *msg =
		(struct htt_ppdu_stats_cmpltn_flush *)tlv_desc;

	ath12k_dp_ppdu_stats_flush_tlv_parse_update(dp_pdev, msg, ppdu_info);
	/* No need to use these stats when SW is already
	 * doing it on a per packet basis
	 */
	if (dp_pdev->dp_stats_mask & DP_ENABLE_STATS)
		return -EINTR;

	ath12k_dp_ppdu_stats_flush_tlv_parse(ab, dp_pdev, msg, ppdu_info);

	/*
	 * In case of FLUSH update only the failure stats and stop further
	 * ppdu processing. Hence return error in case of FLUSH
	 */
	return -EINTR;
}

int ath12k_dp_htt_process_usr_info(struct ath12k_pdev_dp *dp_pdev,
				   const u32 *tlv_desc,
				   struct htt_ppdu_stats_info *ppdu_info)
{
	u8 max_users;
	struct htt_ppdu_stats_users_info *usr_info;

	usr_info = (struct htt_ppdu_stats_users_info *)tlv_desc;
	ppdu_info->tlv_bitmap |= BIT(HTT_PPDU_STATS_USERS_INFO);
	max_users = HTT_PPDU_STATS_GET_MAX_USERS(usr_info->info0);
	if (max_users > 0 && max_users <= HTT_PPDU_STATS_MAX_USERS) {
		ppdu_info->max_users = max_users;
	} else {
		ath12k_err(dp_pdev->dp->ab, "Invalid max_users %u in PPDU stats\n",
			   max_users);
		return -EINVAL;
	}

	return 0;
}

int ath12k_htt_tlv_ppdu_stats_parse(struct ath12k_base *ab,
				    struct ath12k_pdev_dp *dp_pdev,
				    u16 tag, u16 len, const void *ptr,
				    void *data)
{
	struct htt_ppdu_stats_info *ppdu_info;
	int ret = 0;

	ppdu_info = data;

	switch (tag) {
	case HTT_PPDU_STATS_TAG_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_common)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_stats_common_tlv(ptr, ppdu_info);
		break;
	case HTT_PPDU_STATS_TAG_USR_RATE:
		if (len < sizeof(struct htt_ppdu_stats_user_rate)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_usr_rate_stats(ptr, ppdu_info);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_usr_cmpltn_cmn)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ret = dp_process_ppdu_stats_usr_cmpltn_common_tlv(ptr, ppdu_info);

		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS:
		if (len <
		    sizeof(struct htt_ppdu_stats_usr_cmpltn_ack_ba_status)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_usr_compltn_ack_ba_stats(ptr,
								     ppdu_info);
		break;
	case HTT_PPDU_STATS_TAG_SCH_CMD_STATUS:
		ret = ath12k_dp_htt_process_stats_sch_cmd_status_tlv(dp_pdev,
								     ptr,
								     ppdu_info);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMMON:
		if (len < sizeof(struct htt_ppdu_stats_user_common)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
				    len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_usr_cmn_stats(ptr, ppdu_info);
		break;
	case HTT_PPDU_STATS_TAG_USR_COMPLTN_FLUSH:
		if (len < sizeof(struct htt_ppdu_stats_cmpltn_flush)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
					len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_usr_compltn_flush(dp_pdev, ptr,
							      ppdu_info);
		break;
	case HTT_PPDU_STATS_USERS_INFO:
		if (len < sizeof(struct htt_ppdu_stats_users_info)) {
			ath12k_warn(ab, "Invalid len %d for the tag 0x%x\n",
					len, tag);
			return -EINVAL;
		}

		ret = ath12k_dp_htt_process_usr_info(dp_pdev, ptr, ppdu_info);
		break;
	}
	return ret;
}

int ath12k_dp_htt_tlv_iter(struct ath12k_base *ab, struct ath12k_pdev_dp *dp_pdev,
			   const void *ptr, size_t len,
			   int (*iter)(struct ath12k_base *ar,
				       struct ath12k_pdev_dp *dp_pdev, u16 tag,
				       u16 len, const void *ptr, void *data),
			   void *data)
{
	struct htt_ppdu_stats_info *ppdu_info = NULL;
	const struct htt_tlv *tlv;
	const void *begin = ptr;
	u16 tlv_tag, tlv_len;
	int ret = -EINVAL;

	if (!data)
		return ret;

	ppdu_info = (struct htt_ppdu_stats_info *)data;

	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "htt tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}
		tlv = (struct htt_tlv *)ptr;
		tlv_tag = le32_get_bits(tlv->header, HTT_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, HTT_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath12k_err(ab, "htt tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}
		ret = iter(ab, dp_pdev, tlv_tag, tlv_len, ptr, ppdu_info);
		if (ret != 0)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}
	return 0;
}
static u32 ath12k_dp_eht_ru_alloc_from_ru_size(u16 ru_size)
{
	u32 width = 0;

	switch (ru_size) {
	case HTT_PPDU_STATS_RU_26:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_26;
		break;
	case HTT_PPDU_STATS_RU_52:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_52;
		break;
	case HTT_PPDU_STATS_RU_52_26:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_52P26;
		break;
	case HTT_PPDU_STATS_RU_106:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_106;
		break;
	case HTT_PPDU_STATS_RU_106_26:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_106P26;
		break;
	case HTT_PPDU_STATS_RU_242:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_242;
		break;
	case HTT_PPDU_STATS_RU_484:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_484;
		break;
	case HTT_PPDU_STATS_RU_484_242:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_484P242;
		break;
	case HTT_PPDU_STATS_RU_996:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_996;
		break;
	case HTT_PPDU_STATS_RU_996_484:
		width =  NL80211_RATE_INFO_EHT_RU_ALLOC_996P484;
		break;
	case HTT_PPDU_STATS_RU_996_484_242:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_996P484P242;
		break;
	case HTT_PPDU_STATS_RU_996x2:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_2x996;
		break;
	case HTT_PPDU_STATS_RU_996x2_484:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_2x996P484;
		break;
	case HTT_PPDU_STATS_RU_996x3:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_3x996;
		break;
	case HTT_PPDU_STATS_RU_996x3_484:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_3x996P484;
		break;
	case HTT_PPDU_STATS_RU_996x4:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_4x996;
		break;
	default:
		width = NL80211_RATE_INFO_EHT_RU_ALLOC_26;
	}
	return width;
}

static u32 ath12k_dp_he_ru_alloc_from_ru_size(u16 ru_size)
{
	u32 width = 0;

       switch (ru_size) {
       case HTT_PPDU_STATS_RU_26:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_26;
               break;
       case HTT_PPDU_STATS_RU_52:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_52;
               break;
       case HTT_PPDU_STATS_RU_106:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_106;
               break;
       case HTT_PPDU_STATS_RU_242:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_242;
               break;
       case HTT_PPDU_STATS_RU_484:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_484;
               break;
       case HTT_PPDU_STATS_RU_996:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_996;
               break;
       default:
               width = NL80211_RATE_INFO_HE_RU_ALLOC_26;
               break;
       }

       return width;
}

/* Align bw value as per host data structures */
static u8 ath12k_htt_bw_to_mac_bw(u32 rate_flags)
{
       u8 bw = HTT_USR_RATE_BW(rate_flags);

       switch (bw) {
       case HTT_PPDU_STATS_BANDWIDTH_320MHZ:
               bw = ATH12K_BW_320;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_160MHZ:
               bw = ATH12K_BW_160;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_80MHZ:
               bw = ATH12K_BW_80;
               break;
       case HTT_PPDU_STATS_BANDWIDTH_40MHZ:
               bw = ATH12K_BW_40;
               break;
       default:
               bw = ATH12K_BW_20;
       break;
       }

       return bw;
}

static void
ath12k_update_tx_ppdu_ru_mpdu_tried(struct ath12k *ar,
				    struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats,
				    struct ath12k_per_peer_tx_stats *peer_stats,
				    struct rate_info *txrate,
				    u8 ppdu_type,
				    u32 ru_type)
{
	if ((ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	     ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) &&
	    (txrate->flags & RATE_INFO_FLAGS_HE_MCS ||
	     txrate->flags & RATE_INFO_FLAGS_EHT_MCS ||
	     txrate->flags & RATE_INFO_FLAGS_UHR_MCS)) {
		int ru_loc_idx;

		if (txrate->flags & RATE_INFO_FLAGS_HE_MCS)
			ru_loc_idx = ath12k_he_ru_alloc_to_ru_loc_idx(ru_type);
		else
			ru_loc_idx = ath12k_eht_ru_alloc_to_ru_loc_idx(ru_type);

		if (ru_loc_idx >= 0 && ru_loc_idx < MAX_RU_LOCATIONS)
			tx_ppdu_stats->ru_loc_mpdu_succ_tried[ru_loc_idx].mpdu_tried +=
						peer_stats->mpdu_tried;
	}

	if (ppdu_type < HTT_PPDU_STATS_PPDU_TYPE_MAX)
		tx_ppdu_stats->transmit_type_mpdu_succ_tried[ppdu_type].mpdu_tried +=
					peer_stats->mpdu_tried;
}

static void
ath12k_dp_update_tx_ppdu_ru_num_mpdu(struct ath12k *ar,
				     struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats,
				     struct ath12k_per_peer_tx_stats *peer_stats,
				     struct rate_info *txrate,
				     u8 ppdu_type,
				     u32 ru_type)
{
	if ((ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	     ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) &&
	    (txrate->flags & RATE_INFO_FLAGS_HE_MCS ||
	     txrate->flags & RATE_INFO_FLAGS_EHT_MCS ||
	     txrate->flags & RATE_INFO_FLAGS_UHR_MCS)) {
		int ru_loc_idx;

		if (txrate->flags & RATE_INFO_FLAGS_HE_MCS)
			ru_loc_idx = ath12k_he_ru_alloc_to_ru_loc_idx(ru_type);
		else
			ru_loc_idx = ath12k_eht_ru_alloc_to_ru_loc_idx(ru_type);

		if (ru_loc_idx >= 0 && ru_loc_idx < MAX_RU_LOCATIONS)
			tx_ppdu_stats->ru_loc_mpdu_succ_tried[ru_loc_idx].num_mpdu +=
						peer_stats->succ_mpdu_pkts;
	}

	if (ppdu_type < HTT_PPDU_STATS_PPDU_TYPE_MAX)
		tx_ppdu_stats->transmit_type_mpdu_succ_tried[ppdu_type].num_mpdu +=
					peer_stats->succ_mpdu_pkts;
}

static void
ath12k_update_tx_ppdu_basic_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_link_peer *peer,
				  struct ath12k_per_peer_tx_stats *peer_stats,
				  struct htt_ppdu_user_stats *usr_stats,
				  struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats)
{
	struct rate_info *txrate = &peer->txrate;
	u32 succ_pkts = peer_stats->succ_pkts;
	int gi  = ath12k_debugfs_sta_get_gi_idx(txrate);
	int mcs = txrate->mcs;
	int bw  = ath12k_mac_mac80211_bw_to_ath12k_bw(txrate->bw);
	int nss = txrate->nss - 1;
	bool is_mcast;

	is_mcast = HTT_PPDU_STATS_USR_CMN_IS_MCAST(usr_stats->common.info);

	DP_STATS_INCC(tx_ppdu_stats, tx_ucast_success.packets, peer_stats->succ_pkts,
		      !is_mcast);
	DP_STATS_INCC(tx_ppdu_stats, tx_ucast_success.bytes, peer_stats->succ_bytes,
		      !is_mcast);
	DP_STATS_INCR(tx_ppdu_stats, tx_ppdus, 1);
	DP_STATS_INCR(tx_ppdu_stats, tx_mpdus_success, peer_stats->succ_mpdu_pkts);

	if (peer_stats->mpdu_tried > peer_stats->succ_mpdu_pkts)
		DP_STATS_INCR(tx_ppdu_stats, retries_mpdu,
			      (peer_stats->mpdu_tried - peer_stats->succ_mpdu_pkts));
	if (!is_mcast) {
		DP_STATS_UPD(tx_ppdu_stats, last_ack_rssi,
			     peer->peer_stats.last_ack_rssi);
		DP_STATS_UPD(tx_ppdu_stats, avg_ack_rssi,
			     -(s8)ewma_avg_ack_rssi_read(&peer->peer_stats.avg_ack_rssi));
	}

	/* Basic rate counters (pkt_type, gi, nss, bw) */
	if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_BE].mcs_count[mcs],
			      succ_pkts);
	} else if (txrate->flags & RATE_INFO_FLAGS_UHR_MCS) {
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_BN].mcs_count[mcs],
			      succ_pkts);
	} else if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_AX].mcs_count[mcs],
			      succ_pkts);
	} else if (txrate->flags & RATE_INFO_FLAGS_VHT_MCS) {
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_AC].mcs_count[mcs],
			      succ_pkts);
	} else if (txrate->flags & RATE_INFO_FLAGS_MCS) {
		/* HT: txrate->mcs = mcs + 8*(nss-1), range 0-31; clamp to array bound */
		mcs = min_t(int, mcs, MAX_MCS - 1);
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_N].mcs_count[mcs],
			      succ_pkts);
	} else {
		mcs = peer_stats->rate_idx;
		DP_STATS_INCR(tx_ppdu_stats, pkt_type[DOT11_A].mcs_count[mcs],
			      succ_pkts);
	}

	DP_STATS_INCC(tx_ppdu_stats, gi_count[gi], succ_pkts,
		      gi < ATH12K_GI_NUM);
	DP_STATS_INCC(tx_ppdu_stats, nss[nss], succ_pkts,
		      nss >= 0 && nss < ATH12K_NSS_NUM);
	DP_STATS_INCC(tx_ppdu_stats, bw[bw], succ_pkts,
		      bw < ATH12K_BW_MAX);

	DP_STATS_UPD(tx_ppdu_stats, ru_start, peer_stats->ru_start);
	DP_STATS_UPD(tx_ppdu_stats, ru_tones, peer_stats->ru_tones);

	if (peer_stats->mu_grpid < MAX_MU_GROUP_ID &&
	    peer_stats->ppdu_type != HTT_PPDU_STATS_PPDU_TYPE_SU) {
		if (peer_stats->mu_grpid & (MAX_MU_GROUP_ID - 1))
			DP_STATS_UPD(tx_ppdu_stats,
				     mu_group[peer_stats->mu_grpid],
				     (peer_stats->mu_pos + 1));
	}
}

static void
ath12k_update_tx_ppdu_adv_stats(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_dp_link_peer *peer,
				struct ath12k_per_peer_tx_stats *peer_stats,
				struct htt_ppdu_user_stats *usr_stats,
				struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats)
{
	bool is_ppdu_cookie_valid, is_pream_punct;
	u8 ppdu_type, idx, tx_pwr;
	u32 punc_mode, res_mcs;

	ppdu_type = peer_stats->ppdu_type;

	is_ppdu_cookie_valid =
		HTT_PPDU_STATS_USR_CMN_COOKIE_VALID(usr_stats->common.host_opaque_cookie);
	is_pream_punct = HTT_USR_CMPLTN_PREAM_PUNCT(usr_stats->cmpltn_cmn.flags);
	punc_mode = ath12k_dp_mon_get_puncture_type(usr_stats->rate.punctured,
						    peer_stats->bw);

	DP_STATS_INCC(tx_ppdu_stats, stbc, peer_stats->succ_pkts, peer_stats->stbc);
	DP_STATS_INCC(tx_ppdu_stats, ldpc, peer_stats->succ_pkts, peer_stats->ldpc);
	if (peer_stats->tid <= ATH12K_DSCP_PRIORITY) {
		DP_STATS_INCR(tx_ppdu_stats, wme_ac_type[TID_TO_WME_AC(peer_stats->tid)],
			      peer_stats->succ_pkts);
		DP_STATS_INCR(tx_ppdu_stats,
			      wme_ac_type_bytes[TID_TO_WME_AC(peer_stats->tid)],
			      peer_stats->succ_bytes);
	}
	DP_STATS_INCC(tx_ppdu_stats, ampdu_cnt, peer_stats->succ_mpdu_pkts,
		      peer_stats->is_ampdu);
	DP_STATS_INCC(tx_ppdu_stats, non_ampdu_cnt, peer_stats->succ_mpdu_pkts,
		      (!peer_stats->is_ampdu));
	if (is_ppdu_cookie_valid)
		DP_STATS_INCR(tx_ppdu_stats, num_ppdu_cookie_valid, 1);
	DP_STATS_INCC(tx_ppdu_stats, pream_punct_cnt, 1, is_pream_punct);

	for (idx = 0; idx < HTT_STATS_MAX_CHAINS; idx++)
		DP_STATS_UPD(tx_ppdu_stats, rssi_chain[idx],
			     usr_stats->cmpltn_cmn.rssi_chain[idx]);

	/* Update num_mpdu for RU location and transmit type */
	ath12k_dp_update_tx_ppdu_ru_num_mpdu(dp_pdev->ar, tx_ppdu_stats,
					     peer_stats, &peer->txrate,
					     ppdu_type, peer_stats->ru_tones);

	if (peer_stats->flags == WMI_RATE_PREAMBLE_EHT) {
		res_mcs = (peer_stats->mcs < MAX_MCS_11BE) ?
				peer_stats->mcs : (MAX_MCS - 1);

		DP_STATS_INCC(tx_ppdu_stats, su_be_ppdu_cnt.mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_SU);
		DP_STATS_INCC(tx_ppdu_stats,
			      mu_be_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA);
		DP_STATS_INCC(tx_ppdu_stats,
			      mu_be_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO);
		DP_STATS_INCR(tx_ppdu_stats, punc_bw[punc_mode], peer_stats->succ_pkts);
	}

	if (peer_stats->flags == WMI_RATE_PREAMBLE_UHR) {
		res_mcs = (peer_stats->mcs < MAX_MCS_11BN) ?
			peer_stats->mcs : (MAX_MCS - 1);

		DP_STATS_INCC(tx_ppdu_stats, su_bn_ppdu_cnt.mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_SU);
		DP_STATS_INCC(tx_ppdu_stats,
			      mu_bn_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA);
		DP_STATS_INCC(tx_ppdu_stats,
			      mu_bn_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[res_mcs],
			      1, ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO);
		DP_STATS_INCR(tx_ppdu_stats, punc_bw[punc_mode], peer_stats->succ_pkts);
	}

	tx_pwr = HTT_PPDU_GET_PER_CHAIN_TX_PWR(usr_stats->common.tx_pwr, 0);
	if (usr_stats->common.tx_pwr_multiplier)
		DP_STATS_UPD(tx_ppdu_stats, tx_pwr,
			     tx_pwr / usr_stats->common.tx_pwr_multiplier);
}

void
ath12k_update_extd_tx_stats(struct ath12k_pdev_dp *dp_pdev,
			    struct htt_ppdu_stats_info *ppdu_info,
			    struct ath12k_dp_link_peer *peer,
			    struct ath12k_per_peer_tx_stats *peer_stats,
			    struct htt_ppdu_user_stats *usr_stats)
{
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct ath12k_htt_tx_stats *tx_stats;
	struct ath12k_vif *ahvif;
	u8 tid = HTT_PPDU_STATS_NON_QOS_TID;
	u32 tlv_bitmap, retry_mpdus;

	if (usr_stats->processed_tlv_bitmap &
			BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS))
		return;

	tx_stats = peer->peer_stats.tx_stats;
	tx_ppdu_stats = peer->peer_stats.tx_ppdu_stats;
	if (!tx_stats && !tx_ppdu_stats)
		return;

	tlv_bitmap = usr_stats->tlv_bitmap;

	if (usr_stats->cmpltn_cmn.status != HTT_PPDU_STATS_USER_STATUS_OK) {
		if (ath12k_extd_tx_stats_enabled(dp_pdev))
			ath12k_debugfs_sta_update_retry(peer, peer_stats);

		if (tx_ppdu_stats) {
			retry_mpdus = peer_stats->mpdu_tried - peer_stats->succ_mpdu_pkts;
			DP_STATS_INCR(tx_ppdu_stats, retries_mpdu,
				      retry_mpdus);
		}
		return;
	}

	if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS)))
		return;

	usr_stats->processed_tlv_bitmap |=
		BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS);

	if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON))
		peer_stats->is_ampdu =
			HTT_USR_CMPLTN_IS_AMPDU(usr_stats->cmpltn_cmn.flags);

	/* MSDU Success */
	peer_stats->succ_pkts = le32_get_bits(usr_stats->ack_ba.info,
					      HTT_PPDU_STATS_ACK_BA_INFO_NUM_MSDU_M);
	/* MPDU Success */
	peer_stats->succ_mpdu_pkts =
		le32_get_bits(usr_stats->ack_ba.info,
			      HTT_PPDU_STATS_ACK_BA_INFO_NUM_MPDU_M);

	/* Success Bytes */
	peer_stats->succ_bytes = le32_to_cpu(usr_stats->ack_ba.success_bytes);

	/* WMM Stats updation */
	tid = le32_get_bits(usr_stats->ack_ba.info,
			    HTT_PPDU_STATS_ACK_BA_INFO_TID_NUM);
	dp_pdev->wmm_stats.tx_type =
			ath12k_tid_to_ac(tid > ATH12K_DSCP_PRIORITY ? 0 : tid);
	dp_pdev->wmm_stats.total_wmm_tx_pkts[dp_pdev->wmm_stats.tx_type]++;

	ahvif = ath12k_vif_to_ahvif(ath12k_dp_link_peer_get_vif(peer));
	ahvif->wmm_stats.tx_type = dp_pdev->wmm_stats.tx_type;
	ahvif->wmm_stats.total_wmm_tx_pkts[ahvif->wmm_stats.tx_type]++;

	peer_stats->tid = tid;

	/* Update debugfs stats (uses tx_stats internally) */
	if (ath12k_extd_tx_stats_enabled(dp_pdev)) {
		ath12k_debugfs_sta_update_success(peer, peer_stats);
		ath12k_debugfs_sta_update_retry(peer, peer_stats);
	}

	/* tx_ppdu_stats: basic counters */
	if (!tx_ppdu_stats || !ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
		return;

	ath12k_update_tx_ppdu_basic_stats(dp_pdev, peer, peer_stats,
					  usr_stats, tx_ppdu_stats);

	/* tx_ppdu_stats: advanced counters */
	if (!ath12k_htt_tx_ppdu_stats_enabled(dp_pdev) &&
	    !ath12k_dp_advance_stats_enabled(dp_pdev))
		return;

	ath12k_update_tx_ppdu_adv_stats(dp_pdev, peer, peer_stats,
					usr_stats, tx_ppdu_stats);
}

static void
ath12k_htt_update_tx_rate_stats(struct ath12k_dp_link_peer *peer,
				struct ath12k_per_peer_tx_stats *peer_stats,
				struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	u32 ratekbps;
	u16 peer_id;
	u8 pdev_id;
	u8 soc_id;

	tx_ppdu_stats = peer->peer_stats.tx_ppdu_stats;
	if (!tx_ppdu_stats)
		return; /* tx_ppdu_stats not allocated: breach detection skipped */

	if (!ath12k_htt_tx_ppdu_stats_enabled(dp_pdev))
		return;

	ratekbps = cfg80211_calculate_bitrate(&peer->txrate);

	DP_STATS_UPD(tx_ppdu_stats, tx_rate, ratekbps);

	if (IS_VALID_RSSI(tx_ppdu_stats->last_ack_rssi) &&
	    IS_VALID_RATE(tx_ppdu_stats->tx_rate)) {
		soc_id = ath12k_get_ab_device_id(dp_pdev->ar->ab);
		pdev_id = ath12k_get_pdev_id(dp_pdev->ar->pdev);
		peer_id = ath12k_dp_link_peer_get_peer_id(dp_pdev->ar->ab, peer);

		ath12k_telemetry_update_rssi_rate_breach(soc_id,
							 pdev_id,
							 peer_id,
							 peer->addr,
							 PATH_TYPE_TX,
							 tx_ppdu_stats->last_ack_rssi,
							 tx_ppdu_stats->tx_rate);
	}

	if (!ath12k_dp_advance_stats_enabled(dp_pdev))
		return;

	if (peer_stats->flags == WMI_RATE_PREAMBLE_OFDM ||
	    peer_stats->flags == WMI_RATE_PREAMBLE_CCK)
		DP_STATS_UPD(tx_ppdu_stats, tx_ratecode,
			     ath12k_mac_get_rate_hw_value(ratekbps));
	else
		DP_STATS_UPD(tx_ppdu_stats, tx_ratecode,
			     ATH12K_HW_RATE_CODE(peer_stats->mcs, peer_stats->nss,
						 peer_stats->flags, 0));

	if (tx_ppdu_stats->avg_tx_rate == INVALID_RATE)
		tx_ppdu_stats->avg_tx_rate = WEIGHTED_AVG_IN(tx_ppdu_stats->tx_rate);
	else
		WEIGHTED_AVG_UPDATE(tx_ppdu_stats->avg_tx_rate, tx_ppdu_stats->tx_rate);

	if (ath12k_dp_link_peer_get_vif(peer)) {
		if (ath12k_dp_link_peer_get_vif_type(peer) == NL80211_IFTYPE_AP &&
		    peer->dp_peer && peer->dp_peer->is_vdev_peer) {
			DP_STATS_UPD(tx_ppdu_stats, mcast_last_tx_rate, ratekbps);
			DP_STATS_UPD(tx_ppdu_stats, mcast_last_tx_rate_mcs,
				     peer_stats->mcs);
		} else {
			DP_STATS_UPD(tx_ppdu_stats, last_tx_rate_mcs, peer_stats->mcs);
		}
	}
}

void
ath12k_update_htt_stats_txrate(struct ath12k_pdev_dp *dp_pdev,
			       struct ath12k_dp_link_peer *peer,
			       u8 user, struct htt_ppdu_stats_info *ppdu_info,
			       struct ath12k_per_peer_tx_stats *peer_stats)
{
	struct htt_ppdu_stats *ppdu_stats = &ppdu_info->ppdu_stats;
	u32 tx_duration = 0, ru_tones = 0, ru_format, tlv_bitmap, rate_flags;
	struct htt_ppdu_stats_common *common = &ppdu_stats->common;
	bool resp_type_valid, is_ofdma, fixed_rate_used, is_mcast;
	bool extd_tx_stats_enabled, tx_ppdu_stats_enabled;
	u8 flags, mcs, nss, bw, sgi, dcm, rate_idx = 0;
	struct htt_ppdu_stats_user_rate *user_rate;
	struct htt_ppdu_user_stats *usr_stats;
	u16 rate = 0, ru_start, ru_end;
	struct ath12k_htt_tx_stats *tx_stats;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 ppdu_type;
	struct ath12k_base *ab = dp->ab;
	int ack_rssi, snr;
	int ret;

	usr_stats = &ppdu_stats->user_stats[user];

	tlv_bitmap = usr_stats->tlv_bitmap;

	if (usr_stats->processed_tlv_bitmap &
			BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON))
		return;

	if (tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON)) {
		peer_stats->succ_mpdu_pkts =
			__le16_to_cpu(usr_stats->cmpltn_cmn.mpdu_success);
		peer_stats->mpdu_tried = __le16_to_cpu(usr_stats->cmpltn_cmn.mpdu_tried);
		peer_stats->tid = usr_stats->cmpltn_cmn.tid_num;

		peer->tx_retry_count += peer_stats->mpdu_tried -
						peer_stats->succ_mpdu_pkts;
	}

	if (common->fes_duration_us)
		tx_duration = le32_to_cpu(common->fes_duration_us);

	user_rate = &usr_stats->rate;
	ppdu_type = HTT_USR_RATE_PPDU_TYPE(user_rate->info1);

	resp_type_valid = u32_get_bits(user_rate->info1,
				       HTT_PPDU_STATS_USER_RATE_INFO1_RESP_TYPE_VALID);
	if (resp_type_valid) {
		rate_flags = user_rate->resp_rate_flags;
		ru_start = user_rate->resp_ru_start;
		ru_end = user_rate->resp_ru_end;
		ppdu_type = HTT_USR_RESP_RATE_PPDU_TYPE(user_rate->resp_rate_flags);
		if (ppdu_type == HTT_PPDU_STATS_RESP_PPDU_TYPE_MU_OFDMA_UL)
	                ppdu_type = HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA;
		else
	                ppdu_type = HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO;
	} else {
		rate_flags = user_rate->rate_flags;
		ru_start = user_rate->ru_start;
		ru_end = user_rate->ru_end;
	}

	flags = HTT_USR_RATE_PREAMBLE(rate_flags);
	bw = ath12k_htt_bw_to_mac_bw(rate_flags);
	nss = HTT_USR_RATE_NSS(rate_flags) + 1;
	mcs = HTT_USR_RATE_MCS(rate_flags);
	sgi = HTT_USR_RATE_GI(rate_flags);
	dcm = HTT_USR_RATE_DCM(rate_flags);
	peer_stats->stbc = HTT_USR_RATE_STBC(rate_flags);
	peer_stats->ldpc = HTT_USR_RATE_LDPC(rate_flags);
	ru_format = FIELD_GET(HTT_PPDU_STATS_USER_RATE_INFO0_RU_SIZE,
			      user_rate->info0);

	if (ru_format == 1) {
		if (flags == WMI_RATE_PREAMBLE_HE)
			ru_tones = ath12k_dp_he_ru_alloc_from_ru_size(ru_start);
		else if (flags == WMI_RATE_PREAMBLE_EHT)
			ru_tones = ath12k_dp_eht_ru_alloc_from_ru_size(ru_start);
		else if (flags == WMI_RATE_PREAMBLE_UHR)
			ru_tones = ath12k_dp_eht_ru_alloc_from_ru_size(ru_start);
	}
	else if (!ru_format)
		ru_tones = le16_to_cpu(ru_end) - le16_to_cpu(ru_start) + 1;
	else
		ath12k_warn(ab, "unknown ru_format %u for peer %u\n",
				    ru_format, usr_stats->peer_id);

	is_ofdma = (ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA) |
			(ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA);

	ppdu_info->usr_ru_tones_sum += ru_tones;
	/* Note: If host configured fixed rates and in some other special
	 * cases, the broadcast/management frames are sent in different rates.
	 * Firmware rate's control to be skipped for this?
	 */

	if (flags == WMI_RATE_PREAMBLE_HE && mcs > ATH12K_HE_MCS_MAX) {
		ath12k_warn(ab, "Invalid HE mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_EHT && mcs > ATH12K_EHT_MCS_MAX) {
		ath12k_warn(ab, "Invalid EHT mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_UHR && mcs > ATH12K_UHR_MCS_MAX) {
		ath12k_warn(ab, "Invalid UHR mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_VHT && mcs > ATH12K_VHT_MCS_MAX) {
		ath12k_warn(ab, "Invalid VHT mcs %d peer stats",  mcs);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_HT && (mcs > ATH12K_HT_MCS_MAX || nss < 1)) {
		ath12k_warn(ab, "Invalid HT mcs %d nss %d peer stats",
			    mcs, nss);
		return;
	}

	if (flags == WMI_RATE_PREAMBLE_CCK || flags == WMI_RATE_PREAMBLE_OFDM) {
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(mcs,
							    flags,
							    &rate_idx,
							    &rate);
		if (ret < 0)
			return;
	}

	memset(&peer->txrate, 0, sizeof(peer->txrate));

	switch (flags) {
	case WMI_RATE_PREAMBLE_OFDM:
		peer->txrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_CCK:
		peer->txrate.legacy = rate;
		break;
	case WMI_RATE_PREAMBLE_HT:
		peer->txrate.mcs = mcs + 8 * (nss - 1);
		peer->txrate.flags = RATE_INFO_FLAGS_MCS;
		break;
	case WMI_RATE_PREAMBLE_VHT:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_VHT_MCS;
		break;
	case WMI_RATE_PREAMBLE_HE:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		peer->txrate.he_dcm = dcm;
		peer->txrate.he_ru_alloc = ru_tones;
		peer_stats->ru_tones = peer->txrate.he_ru_alloc;
		break;
	case WMI_RATE_PREAMBLE_EHT:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_EHT_MCS;
		peer->txrate.he_dcm = dcm;
		peer->txrate.eht_ru_alloc = ru_tones;
		peer_stats->ru_tones = peer->txrate.eht_ru_alloc;
		break;
	case WMI_RATE_PREAMBLE_UHR:
		peer->txrate.mcs = mcs;
		peer->txrate.flags = RATE_INFO_FLAGS_UHR_MCS;
		peer->txrate.he_dcm = dcm;

		/*
		 * We fill EHT params for UHR mode as well since
		 * the APIs such as _cfg80211_calculate_bitrate_eht_uhr() etc.
		 * remain common and use EHT params to calculate Tx Bit rate etc.
		 */
		peer->txrate.eht_ru_alloc = ru_tones;
		peer_stats->ru_tones = peer->txrate.eht_ru_alloc;
		break;
	}

	ath12k_dp_fill_txrate_gi(&peer->txrate, flags, sgi);

	peer->txrate.nss = nss;
	usr_stats->nss = nss;
	ppdu_info->usr_nss_sum += nss;
	peer->txrate.bw = ath12k_mac_bw_to_mac80211_bw(bw);

	is_mcast = HTT_PPDU_STATS_USR_CMN_IS_MCAST(usr_stats->common.info);
	snr = le32_to_cpu(usr_stats->cmpltn_cmn.ack_rssi);
	ack_rssi = ath12k_dp_get_rssi_value(snr, &peer->signal_stats,
					    &dp_pdev->ar->rssi_offsets, true);

	if (!is_mcast && ack_rssi) {
		peer->peer_stats.last_ack_rssi = ack_rssi;
		ewma_avg_ack_rssi_add(&peer->peer_stats.avg_ack_rssi,
				      -ack_rssi);
	}

	memcpy(&peer->last_txrate, &peer->txrate, sizeof(struct rate_info));

	if (is_ofdma) {
		if (flags == WMI_RATE_PREAMBLE_HE)
			peer->txrate.bw = RATE_INFO_BW_HE_RU;
		else if (flags == WMI_RATE_PREAMBLE_EHT)
			peer->txrate.bw = RATE_INFO_BW_EHT_RU;
		else if (flags == WMI_RATE_PREAMBLE_UHR)
			peer->txrate.bw = RATE_INFO_BW_EHT_RU;
	}

	extd_tx_stats_enabled = ath12k_extd_tx_stats_enabled(dp_pdev);
	tx_ppdu_stats_enabled = ath12k_htt_tx_ppdu_stats_enabled(dp_pdev);

	if (extd_tx_stats_enabled || tx_ppdu_stats_enabled) {
		peer_stats->ppdu_type = ppdu_type;
		peer_stats->rate_idx = rate_idx;
		peer_stats->duration = tx_duration;
		peer_stats->ru_tones = ru_tones;
		peer_stats->ba_fails =
			HTT_USR_CMPLTN_LONG_RETRY(usr_stats->cmpltn_cmn.flags) +
			HTT_USR_CMPLTN_SHORT_RETRY(usr_stats->cmpltn_cmn.flags);
		peer_stats->ru_start = ru_start;
		peer_stats->rate = rate;
		peer_stats->bw = bw;
		peer_stats->mcs = mcs;
		peer_stats->nss = nss;
		peer_stats->flags = flags;
		peer_stats->mu_pos = HTT_USR_RATE_USR_POS(user_rate->info0);
		peer_stats->mu_grpid = HTT_USR_RATE_MU_GRPID(user_rate->info0);
	}

	if (extd_tx_stats_enabled) {
		tx_stats = peer->peer_stats.tx_stats;
		/* PPDU stats reported for mgmt packet doesn't have valid tx bytes.
		 * So skip peer stats update for mgmt packets.
		 */
		if (!tx_stats)
			return;

		tx_stats->ppdu_type = ppdu_type;
		tx_stats->rate_idx = rate_idx;
		tx_stats->ru_tones = ru_tones;
		ath12k_debugfs_sta_update_misc(peer, peer_stats);
	}

	/* Update tx_ppdu_stats rate fields */
	if (tx_ppdu_stats_enabled) {
		tx_ppdu_stats = peer->peer_stats.tx_ppdu_stats;

		if (!tx_ppdu_stats)
			return;

		DP_STATS_INCR(tx_ppdu_stats, tx_mpdus_tried, peer_stats->mpdu_tried);
		if (ath12k_dp_advance_stats_enabled(dp_pdev))
			ath12k_update_tx_ppdu_ru_mpdu_tried(dp_pdev->ar,
							    tx_ppdu_stats, peer_stats,
							    &peer->txrate, ppdu_type,
							    peer_stats->ru_tones);

		fixed_rate_used = HTT_USR_RATE_IS_FIXED_RATE(user_rate->info2);
		if (!fixed_rate_used)
			ath12k_htt_update_tx_rate_stats(peer, peer_stats, dp_pdev);
	}
}

void
ath12k_update_per_peer_tx_stats(struct ath12k_pdev_dp *dp_pdev,
				struct ath12k_dp_link_peer *peer,
				u8 user, struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_per_peer_tx_stats peer_stats = {0};
	/*
	 * Stats update to txrate which will be exported to mac80211 to
	 * display in iw station dump
	 */
	spin_lock_bh(&peer->ppdu_stats_lock);
	ath12k_update_htt_stats_txrate(dp_pdev, peer, user, ppdu_info,
				       &peer_stats);
	spin_unlock_bh(&peer->ppdu_stats_lock);

	if ((ath12k_extd_tx_stats_enabled(dp_pdev) ||
	     ath12k_htt_tx_ppdu_stats_enabled(dp_pdev)) &&
	    peer_stats.tid <= HTT_PPDU_STATS_NON_QOS_TID)
		ath12k_update_extd_tx_stats(dp_pdev, ppdu_info, peer, &peer_stats,
					    &ppdu_info->ppdu_stats.user_stats[user]);
}

static void
ath12k_ppdu_per_user_stats_phy_tx_time_update(struct ath12k_base *ab,
					      struct ath12k_dp_link_peer *peer,
					      const struct htt_ppdu_stats_info *ppdu_info,
					      const struct htt_ppdu_user_stats *user)
{
	const struct htt_ppdu_stats_common *common;
	struct ath12k_dp_mon_peer_stats *stats;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	u32 ru_nss_width_sum = 0;
	u16 phy_ppdu_tx_time_us;
	u16 phy_tx_time_us = 0;
	u8 tid;
	u8 ac;

	lockdep_assert_held(&ab->dp->dp_lock);

	if (!peer || !ppdu_info) {
		ath12k_warn(ab, "Invalid ppdu user info received\n");
		return;
	}

	common = &ppdu_info->ppdu_stats.common;
	tx_ppdu_stats = peer->peer_stats.tx_ppdu_stats;

	ru_nss_width_sum = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
	if (!ru_nss_width_sum)
		ru_nss_width_sum = 1;

	phy_ppdu_tx_time_us = le16_to_cpu(common->phy_ppdu_tx_time_us);
	if (ppdu_info->htt_frame_type == HTT_STATS_FTYPE_TIDQ_DATA_SU)
		phy_tx_time_us = phy_ppdu_tx_time_us;
	else
		phy_tx_time_us = (phy_ppdu_tx_time_us *
				  user->nss * user->ru_tones) / ru_nss_width_sum;

	peer->tx_duration += phy_tx_time_us;
	tid = user->rate.tid_num;
	ac = ath12k_tid_to_ac(tid);
	stats = &peer->peer_stats.dp_mon_stats;
	stats->mon_stats.tx_airtime_consumption[ac].consumption += phy_tx_time_us;

	if (tx_ppdu_stats) {
		DP_STATS_INCR(tx_ppdu_stats, tx_ppdu_duration, phy_tx_time_us);
		DP_STATS_INCR(tx_ppdu_stats, tx_dur_ac[ac], phy_tx_time_us);
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "ppdu info id: %d tid: %d htt frame type: %d  ppdu frame type: %d time: %d nss: %d tones: %d sum [nss: %d tone: %d consum: %d]\n",
		   ppdu_info->ppdu_id, user->rate.tid_num, ppdu_info->htt_frame_type,
		   ppdu_info->frame_type, phy_ppdu_tx_time_us, user->nss,
		   user->ru_tones, ppdu_info->usr_nss_sum, ppdu_info->usr_ru_tones_sum,
		   stats->mon_stats.tx_airtime_consumption[ac].consumption);
}

void ath12k_dp_tx_ctrl_stats_update(struct ath12k_pdev_dp *dp_pdev,
				    struct ath12k_dp_link_peer *peer,
				    u8 user,
				    struct htt_ppdu_stats_info *ppdu_info)
{
	struct htt_ppdu_user_stats *usr_stats;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	bool is_rts_success, is_rts_failure;
	u16 frame_ctrl;
	u8 mprot_type;

	if (!ppdu_info || user >= ppdu_info->max_users ||
	    user >= HTT_PPDU_STATS_MAX_USERS)
		return;

	usr_stats = &ppdu_info->ppdu_stats.user_stats[user];

	if (usr_stats->processed_tlv_bitmap &
			BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON))
		return;

	tx_ppdu_stats = peer->peer_stats.tx_ppdu_stats;
	if (!tx_ppdu_stats)
		return;

	is_rts_success = HTT_USR_CMPLTN_RTS_SUCCESS(usr_stats->cmpltn_cmn.flags);
	is_rts_failure = HTT_USR_CMPLTN_RTS_FAILURE(usr_stats->cmpltn_cmn.flags);

	mprot_type = HTT_USR_CMPLTN_MPROT_TYPE(usr_stats->cmpltn_cmn.flags);
	if (mprot_type) {
		DP_STATS_INCC(tx_ppdu_stats, rts_success, 1, is_rts_success);
		DP_STATS_INCC(tx_ppdu_stats, rts_failure, 1, is_rts_failure);
	}

	frame_ctrl = HTT_PPDU_STATS_USR_CMN_FRAME_CTRL(usr_stats->common.ctrl);
	if (HTT_GET_FRAME_CTRL_TYPE(frame_ctrl) ==
			HTT_STATS_FRAME_CTRL_TYPE_CTRL) {
		if ((__le16_to_cpu(frame_ctrl) & HTT_STATS_FC0_SUBTYPE_MASK) ==
				HTT_STATS_FC0_SUBTYPE_VHT_NDP_AN)
			DP_STATS_INCR(tx_ppdu_stats, ndpa_cnt, 1);

		if ((__le16_to_cpu(frame_ctrl) & HTT_STATS_FC0_SUBTYPE_MASK) ==
				HTT_STATS_FC0_SUBTYPE_BAR)
			DP_STATS_INCR(tx_ppdu_stats, bar_cnt, 1);
	}
}

void ath12k_htt_update_peer_telemetry_stats(struct ath12k_pdev_dp *dp_pdev,
						   struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_base *ab = dp_pdev->ar->ab;
	struct ath12k_dp_link_peer *peer;
	struct htt_ppdu_stats *ppdu_stats;
	struct htt_ppdu_user_stats *user_stats = NULL;
	u32 tlv_bitmap;
	u8 uid;

	if (!ppdu_info)
		return;

	ppdu_stats = &ppdu_info->ppdu_stats;

	if (ppdu_info->frame_type != HTT_STATS_PPDU_FTYPE_DATA)
		return;

	for (uid = 0; uid < ppdu_info->max_users; uid++) {
		user_stats = &ppdu_stats->user_stats[uid];

		tlv_bitmap = user_stats->tlv_bitmap;
		if (!(tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_USR_RATE)))
			continue;

		rcu_read_lock();

		peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
								user_stats->peer_id);
		if (!peer || (peer->dp_peer && peer->dp_peer->is_vdev_peer)) {
			rcu_read_unlock();
			continue;
		}

		spin_lock_bh(&dp_pdev->dp->dp_lock);
		ath12k_ppdu_per_user_stats_phy_tx_time_update(ab, peer,
							      ppdu_info,
							      user_stats);
		spin_unlock_bh(&dp_pdev->dp->dp_lock);

		rcu_read_unlock();
	}
}

void ath12k_htt_free_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
			       struct htt_ppdu_stats_info *ppdu_info)
{
	/*
	 * Free only when BAR/ACK BA accounting is complete and descriptor
	 * is no longer referenced by any pending PPDU processing.
	 */
	if ((ppdu_info->ack_ba_done > 0 && ppdu_info->bar_num_users > 0) &&
	    (ppdu_info->ack_ba_done == ppdu_info->bar_num_users)) {
		list_del(&ppdu_info->list);
		dp_pdev->stats.ppdu_list_stats.ppdu_stat_list_depth--;
		kfree(ppdu_info);
	}
}

static void
ath12k_dp_htt_fill_user_stats_peer_mac(struct htt_ppdu_user_stats *user_stats,
				       struct ath12k_dp_link_peer *peer)
{
	if (!peer) {
		memset(user_stats->peer_mac, 0, ETH_ALEN);
		return;
	}
	memcpy(user_stats->peer_mac, peer->addr, ETH_ALEN);
}

void ath12k_htt_update_ppdu_stats(struct ath12k_pdev_dp *dp_pdev,
				  struct htt_ppdu_stats_info *ppdu_info)
{
	u8 user;
	u32 tlv_bitmap;
	struct htt_ppdu_user_stats *usr_stats;
	struct ath12k_dp_link_peer *peer;

	for (user = 0; user < ppdu_info->max_users; user++) {
		usr_stats = &ppdu_info->ppdu_stats.user_stats[user];
		tlv_bitmap = usr_stats->tlv_bitmap;

		if (!(tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_RATE)) ||
		    !(tlv_bitmap & (1 << HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON)))
			continue;

		rcu_read_lock();
		spin_lock_bh(&dp_pdev->dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
								usr_stats->peer_id);

		if (!peer || !ath12k_dp_link_peer_get_sta(peer)) {
			spin_unlock_bh(&dp_pdev->dp->dp_lock);
			rcu_read_unlock();
			continue;
		}

		ath12k_dp_htt_fill_user_stats_peer_mac(usr_stats, peer);
		if (peer->peer_stats.tx_ppdu_stats &&
		    ath12k_dp_advance_stats_enabled(dp_pdev))
			ath12k_dp_tx_ctrl_stats_update(dp_pdev, peer, user, ppdu_info);
		if (ppdu_info->frame_type != HTT_STATS_PPDU_FTYPE_CTRL) {
			if ((ppdu_info->htt_frame_type == HTT_STATS_FTYPE_TIDQ_DATA_MU) &&
			    !(tlv_bitmap &
			    (1 << HTT_PPDU_STATS_TAG_USR_COMPLTN_ACK_BA_STATUS))) {
				spin_unlock_bh(&dp_pdev->dp->dp_lock);
				rcu_read_unlock();
				continue;
			}
			ath12k_update_per_peer_tx_stats(dp_pdev, peer, user, ppdu_info);
		}
		spin_unlock_bh(&dp_pdev->dp->dp_lock);
		rcu_read_unlock();

		usr_stats->processed_tlv_bitmap |=
			BIT(HTT_PPDU_STATS_TAG_USR_COMPLTN_COMMON);
	}

	ath12k_htt_update_peer_telemetry_stats(dp_pdev, ppdu_info);
}

static bool ath12k_dp_htt_check_wrap_tsf(struct ath12k_pdev_dp *dp_pdev,
					 struct htt_ppdu_stats_info *ppdu_info,
					 u32 tsf_l32, u32 ppdu_id)
{
	uint32_t time_delta;

	if (ppdu_info->tsf_l32 > tsf_l32)
		time_delta = (MAX_TSF_L32 - ppdu_info->tsf_l32) + tsf_l32;
	else
		time_delta = tsf_l32 - ppdu_info->tsf_l32;

	if (time_delta > WRAP_DROP_TSF_DELTA) {
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_HTT, "PPDU Wrap Drop for s_ppdu_id %d |s_tsf_l32 %d | s_tlv_bitmap 0x%x | s_completion status %d |r_ppdu_id %d | r_tsf_l32 %d",
			   ppdu_info->ppdu_id, ppdu_info->tsf_l32,
			   ppdu_info->tlv_bitmap,
			   ppdu_info->ppdu_stats.user_stats[0].cmpltn_cmn.status,
			   ppdu_id, tsf_l32);

		return true;
	}

	return false;
}

static
struct htt_ppdu_stats_info *ath12k_dp_htt_get_ppdu_desc(struct ath12k_pdev_dp *dp_pdev,
							u32 ppdu_id,
							u32 tsf_l32, u8 pdev_id)
{
	struct htt_ppdu_stats_info *ppdu_info, *ppdu_info_n;
	struct ath12k_htt_ppdu_stats *ppdu_list_stats;

	ppdu_list_stats = &dp_pdev->stats.ppdu_list_stats;

	lockdep_assert_held(&dp_pdev->ppdu_list_lock);
	if (!list_empty(&dp_pdev->ppdu_stats_info)) {
		list_for_each_entry_safe(ppdu_info, ppdu_info_n,
					 &dp_pdev->ppdu_stats_info, list) {
			if (ppdu_info->ppdu_id == ppdu_id) {
				if (ath12k_dp_htt_check_wrap_tsf(dp_pdev, ppdu_info,
								 tsf_l32,
								 ppdu_id)) {
					ppdu_list_stats->ppdu_stat_list_depth--;
					ppdu_list_stats->ppdu_wrap_drop++;
					list_del(&ppdu_info->list);
					kfree(ppdu_info);
					break;
				}
				return ppdu_info;

			}
		}

		if (ppdu_list_stats->ppdu_stat_list_depth > HTT_PPDU_DESC_MAX_DEPTH) {
			ppdu_info = list_first_entry(&dp_pdev->ppdu_stats_info,
						     typeof(*ppdu_info), list);
			list_del(&ppdu_info->list);
			ppdu_list_stats->ppdu_stat_list_depth--;
			/* Update the stats once per ppdu info as this function can be
			 * called multiple times per ppdu info with data frame,
			 * avoid updating the same user stats again for data frame
			 */
			if (ppdu_info->frame_type != HTT_STATS_PPDU_FTYPE_DATA)
			        ath12k_htt_update_ppdu_stats(dp_pdev, ppdu_info);
			kfree(ppdu_info);
		}
	}

	ppdu_info = kzalloc(sizeof(*ppdu_info), GFP_ATOMIC);
	if (!ppdu_info)
		return NULL;

	list_add_tail(&ppdu_info->list, &dp_pdev->ppdu_stats_info);
	ppdu_list_stats->ppdu_stat_list_depth++;

	ppdu_info->max_users = HTT_PPDU_STATS_MAX_USERS;
	ppdu_info->pdev_id = pdev_id;
	ppdu_info->tsf_l32 = tsf_l32;

	return ppdu_info;
}

void
ath12k_dp_htt_ppdu_stats_update_tx_comp_stats(struct ath12k_pdev_dp *dp_pdev,
					      struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer = NULL;
	struct htt_ppdu_user_stats* usr_stats = NULL;
	struct rate_info rate;
	struct ieee80211_tx_status status;
	struct ieee80211_rate_status status_rate = { 0 };
	u32 peer_id = 0;
	int i;

	lockdep_assert_held(&ar->data_lock);

	ath12k_htt_update_ppdu_stats(dp_pdev, ppdu_info);
	/* Update below stats when msdu update path is disabled, It is likely
	 * the case when KPI is enabled
	 */
	if (dp_pdev->dp_stats_mask & DP_ENABLE_STATS)
		return;

	for (i = 0; i < ppdu_info->ppdu_stats.common.num_users; i++) {
		usr_stats = &ppdu_info->ppdu_stats.user_stats[i];
		peer_id = usr_stats->peer_id;
		rcu_read_lock();
		peer = ath12k_dp_link_peer_find_by_peerid_index(ab->dp,
								dp_pdev,
								peer_id);
		if (unlikely(!peer || !ath12k_dp_link_peer_get_sta(peer))) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "dp_tx: failed to find the peer with peer_id %d\n",
				peer_id);
			rcu_read_unlock();
			continue;
		}

		if (ath12k_dp_link_peer_get_vif_type(peer) !=
							NL80211_IFTYPE_MESH_POINT) {
			rcu_read_unlock();
			return;
		}

		if (ether_addr_equal(peer->addr,
				     ath12k_dp_link_peer_get_vif(peer)->addr)) {
			rcu_read_unlock();
			continue;
		}

		memset(&status, 0, sizeof(status));

		status.sta = ath12k_dp_link_peer_get_sta(peer);
		rate = peer->last_txrate;

		status_rate.rate_idx = rate;
		status_rate.try_count = 1;

		status.rates = &status_rate;
		status.n_rates = 1;
		status.mpdu_succ = usr_stats->cmpltn_cmn.mpdu_success;

		ieee80211s_update_metric_ppdu(ar->ah->hw, &status);
		rcu_read_unlock();
	}
}

static uint32_t ath12k_dp_htt_mask_ppdu_id(uint32_t ppdu_id)
{
	return (ppdu_id & DP_HTT_PPDU_ID_MASK);
}

bool ath12k_dp_htt_is_ppdu_completed(struct htt_ppdu_stats_info *ppdu_info)
{
	/*
	 * HTT_PPDU_STATS_TAG_SCH_CMD_STATUS is the last TLV of PPDU.
	 * Hence if this TLV is present consider the PPDU is completed.
	 */
	if (ppdu_info->tlv_bitmap & BIT(HTT_PPDU_STATS_TAG_SCH_CMD_STATUS))
		return true;

	return false;
}

static void ath12k_dp_htt_ppdu_notify(struct ath12k_pdev_dp *dp_pdev,
				      struct htt_ppdu_stats_info *ppdu_info)
{
	struct ath12k_ppdu_event event;
	struct sk_buff *skb;
	struct ath12k_ppdu_tx_info *ppdu_evt_data;
	unsigned int len;

	/* Early exit if no one is listening for TX events - avoid unnecessary work */
	if (!ath12k_ppdu_notifier_has_listeners(ATH12K_EVENT_PPDU_TX_COMPLETE))
		return;

	len = sizeof(*ppdu_evt_data);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	ppdu_evt_data = skb_put_zero(skb, len);
	memcpy(&ppdu_evt_data->ppdu_info, ppdu_info, sizeof(*ppdu_info));
	if (dp_pdev->dp)
		ppdu_evt_data->device_id = ath12k_get_ab_device_id(dp_pdev->dp->ab);

	memset(&event, 0, sizeof(event));
	event.skb = skb;

	ath12k_ppdu_notifier_call_chain(ATH12K_EVENT_PPDU_TX_COMPLETE, &event);

	if (refcount_read(&skb->users) > 1)
		ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
			   "Current SKB ref cnt held by TX PPDU evt listeners = %d\n",
			   refcount_read(&skb->users));
	kfree_skb(skb);
}

void ath12k_dp_htt_deliver_ppdu(struct ath12k_pdev_dp *dp_pdev,
				struct htt_ppdu_stats_info *ppdu_info)
{
	/* Update tx completion stats */
	ath12k_dp_htt_ppdu_stats_update_tx_comp_stats(dp_pdev, ppdu_info);
	/* Send PPDU notification to registered listeners */
	ath12k_dp_htt_ppdu_notify(dp_pdev, ppdu_info);
}

struct htt_ppdu_stats_info *
ath12k_dp_htt_process_ppdu(struct ath12k_pdev_dp *dp_pdev,
			   struct ath12k_htt_ppdu_stats_msg *msg, u32 len,
			   u8 pdev_id)
{
	struct htt_ppdu_stats_info *ppdu_info;
	struct ath12k_base *ab = dp_pdev->dp->ab;
	struct ath12k_htt_ppdu_stats *list_stats;
	u32 ppdu_id, tsf_l32;
	int ret;

	ppdu_id = le32_to_cpu(msg->ppdu_id);
	ppdu_id = ath12k_dp_htt_mask_ppdu_id(ppdu_id);
	list_stats = &dp_pdev->stats.ppdu_list_stats;
	/*
	 * Skip processing the buffer if previous ppdu buffer of
	 * same ppdu_id was dropped.
	 */
	if (unlikely(list_stats->last_ppdu_buf_drop)) {
		if (list_stats->last_ppdu_id == ppdu_id)
			return NULL;

		list_stats->last_ppdu_buf_drop = 0;
	}

	tsf_l32 = __le32_to_cpu(msg->timestamp);

	ppdu_info = ath12k_dp_htt_get_ppdu_desc(dp_pdev, ppdu_id, tsf_l32,
						pdev_id);
	if (!ppdu_info)
		return NULL;

	ppdu_info->pdev_id = pdev_id;
	ppdu_info->ppdu_id = ppdu_id;
	ret = ath12k_dp_htt_tlv_iter(ab, dp_pdev, msg->data, len,
				     ath12k_htt_tlv_ppdu_stats_parse,
				     (void *)ppdu_info);

	if (ret == -EINTR)
		return NULL;

	if (ret) {
		/* Error processing ppdu tags */
		list_stats->last_ppdu_id = ppdu_id;
		list_stats->last_ppdu_buf_drop = 1;

		dp_pdev->stats.ppdu_list_stats.ppdu_stat_list_depth--;
		list_del(&ppdu_info->list);
		kfree(ppdu_info);

		ath12k_warn(ab, "Failed to parse tlv %d\n", ret);
		return NULL;
	}

	return ppdu_info;
}

static int ath12k_htt_pull_ppdu_stats(struct ath12k_base *ab,
				      struct sk_buff *skb)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_htt_ppdu_stats_msg *msg;
	struct htt_ppdu_stats_info *ppdu_info;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k *ar;
	int ret = 0;
	u8 pdev_id;
	u32 len;

	ret = -EINVAL;
	msg = (struct ath12k_htt_ppdu_stats_msg *)skb->data;
	len = le32_get_bits(msg->info, HTT_T2H_PPDU_STATS_INFO_PAYLOAD_SIZE);
	if (len > (skb->len - struct_size(msg, data, 0))) {
		ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
			   "HTT PPDU STATS event has unexpected payload size %u, should be smaller than %u\n",
			   len, skb->len);
		return -EINVAL;
	}

	pdev_id = le32_get_bits(msg->info, HTT_T2H_PPDU_STATS_INFO_PDEV_ID);

	if (pdev_id < 1) {
		ath12k_warn(ab, "HTT PPDU STATS invalid pdev id");
		return -EINVAL;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		/* It is possible that the ar is not yet active (started).
		 * The above function will only look for the active pdev
		 * and hence %NULL return is possible. Just silently
		 * discard this message
		 */
		rcu_read_unlock();
		goto exit;
	}

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id - 1);
	if (!dp_pdev) {
		rcu_read_unlock();
		ret = -EINVAL;
		goto exit;
	}
	dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (ar->debug.is_pkt_logging &&
	    (dp_mon_pdev->rx_pktlog_mode == ATH12K_PKTLOG_MODE_LITE)) {
		trace_ath12k_htt_ppdu_stats(ar, skb->data, len);
		ath12k_htt_ppdu_pktlog_process(ar, (u8 *)skb->data, skb->len);
	}
	rcu_read_unlock();

	spin_lock_bh(&dp_pdev->ppdu_list_lock);

	ppdu_info = ath12k_dp_htt_process_ppdu(dp_pdev, msg, len, pdev_id);

	if (ppdu_info && ath12k_dp_htt_is_ppdu_completed(ppdu_info)) {
		ath12k_dp_htt_deliver_ppdu(dp_pdev, ppdu_info);
		ath12k_htt_free_ppdu_info(dp_pdev, ppdu_info);
	}

	spin_unlock_bh(&dp_pdev->ppdu_list_lock);
exit:
	return ret;
}

static void ath12k_htt_backpressure_event_handler(struct ath12k_base *ab,
						  struct sk_buff *skb)
{
	u32 *data;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u8 pdev_id, ring_type, ring_id, pdev_idx;
	u16 hp, tp;
	u32 backpressure_time;
	struct ath12k_bp_stats *bp_stats;

	if (skb->len < sizeof(u32) * 3)
		return;

	data = (u32 *)skb->data;
	pdev_id = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_PDEV_ID_M);
	ring_type = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_RING_TYPE_M);
	ring_id = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_RING_ID_M);
	++data;

	hp = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_HP_M);
	tp = u32_get_bits(*data, HTT_BACKPRESSURE_EVENT_TP_M);
	++data;

	backpressure_time = *data;

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "htt backpressure event, pdev %d, ring type %d,ring id %d, hp %d tp %d, backpressure time %d\n",
		   pdev_id, ring_type, ring_id, hp, tp, backpressure_time);

	if (ring_type == HTT_BACKPRESSURE_UMAC_RING_TYPE) {
		if (ring_id >= HTT_SW_UMAC_RING_IDX_MAX)
			return;

		bp_stats = &dp->device_stats.bp_stats.umac_ring_bp_stats[ring_id];
	} else if (ring_type == HTT_BACKPRESSURE_LMAC_RING_TYPE) {
		pdev_idx = DP_HW2SW_MACID(pdev_id);

		if (ring_id >= HTT_SW_LMAC_RING_IDX_MAX || pdev_idx >= MAX_RADIOS)
			return;

		bp_stats = &dp->device_stats.bp_stats.lmac_ring_bp_stats[ring_id][pdev_idx];
	} else {
		ath12k_warn(ab, "unknown ring type received in htt bp event %d\n",
			    ring_type);
		return;
	}

	spin_lock_bh(&ab->base_lock);
	bp_stats->hp = hp;
	bp_stats->tp = tp;
	bp_stats->count++;
	bp_stats->jiffies = jiffies;
	spin_unlock_bh(&ab->base_lock);
}

static void ath12k_htt_mlo_offset_event_handler(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct ath12k_htt_mlo_offset_msg *msg;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, j;
	u8 pdev_id;

	msg = (struct ath12k_htt_mlo_offset_msg *)skb->data;
	pdev_id = u32_get_bits(__le32_to_cpu(msg->info),
			       HTT_T2H_MLO_OFFSET_INFO_PDEV_ID);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		/* It is possible that the ar is not yet active (started).
		 * The above function will only look for the active pdev
		 * and hence %NULL return is possible. Just silently
		 * discard this message
		 */
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	pdev = ar->pdev;

	pdev->timestamp.info = __le32_to_cpu(msg->info);
	pdev->timestamp.sync_timestamp_lo_us = __le32_to_cpu(msg->sync_timestamp_lo_us);
	pdev->timestamp.sync_timestamp_hi_us = __le32_to_cpu(msg->sync_timestamp_hi_us);
	pdev->timestamp.mlo_offset_lo = __le32_to_cpu(msg->mlo_offset_lo);
	pdev->timestamp.mlo_offset_hi = __le32_to_cpu(msg->mlo_offset_hi);
	pdev->timestamp.mlo_offset_clks = __le32_to_cpu(msg->mlo_offset_clks);
	pdev->timestamp.mlo_comp_clks = __le32_to_cpu(msg->mlo_comp_clks);
	pdev->timestamp.mlo_comp_timer = __le32_to_cpu(msg->mlo_comp_timer);

	ag->mlo_tstamp_offset = ((u64)pdev->timestamp.mlo_offset_hi << 32 |
				 pdev->timestamp.mlo_offset_lo);

	/* MLO TSAMP OFFSET is common for all chips and
	 * fetch delta_tsf2 for all the radios for this event
	 */
	for (i = 0; i < ag->num_devices; i++) {
		struct ath12k_base *tmp_ab = ag->ab[i];

		/* Skip MLO offset processing during recovery/bypass to avoid NOC errors.
		 * PMM registers are not accessible during chip recovery or when the
		 * device is bypassed, and accessing them causes NOC bus errors. MLO
		 * timestamp synchronization will resume after recovery/bypass completes.
		 */
		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &tmp_ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_RECOVERY, &tmp_ab->dev_flags) ||
		    tmp_ab->is_bypassed)
			continue;

		for (j = 0; j < tmp_ab->num_radios; j++) {
			struct ath12k *tmp_ar;

			pdev = &tmp_ab->pdevs[j];
			tmp_ar = pdev->ar;
			if (!tmp_ar || !tmp_ab->ce_pipe_init_done)
				continue;

			if (tmp_ab->hw_params->hal_ops->hal_get_tsf2_scratch_reg)
				tmp_ab->hw_params->hal_ops->hal_get_tsf2_scratch_reg(tmp_ab, tmp_ar->lmac_id,
										     &tmp_ar->delta_tsf2);
		}
	}

	spin_unlock_bh(&ar->data_lock);
exit:
	rcu_read_unlock();
}

static void
ath12k_htt_pktlog_tx_handler(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct ath12k_htt_pktlog_msg *msg;
	u32 payload_avail;
	u16 payload_size;
	u8 pdev_id;

	if (!skb->data)
		return;

	if (skb->len < sizeof(msg->header)) {
		ath12k_warn(ab, "HTT PKTLOG MSG too short: %u\n", skb->len);
		return;
	}

	msg = (struct ath12k_htt_pktlog_msg *)skb->data;
	payload_size = le32_get_bits(msg->header, HTT_T2H_PKTLOG_PAYLOAD_SIZE);
	payload_avail = skb->len - sizeof(msg->header);
	if (payload_size > payload_avail) {
		ath12k_warn(ab,
			    "HTT PKTLOG MSG invalid payload size: %u > %u\n",
			    payload_size, payload_avail);
		return;
	}

	pdev_id = le32_get_bits(msg->header, HTT_T2H_PKTLOG_PDEV_ID);
	if (pdev_id < 1) {
		ath12k_warn(ab, "HTT PKTLOG MSG has invalid pdev id : %d\n",
			    pdev_id);
		return;
	}

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "pktlog tx: failed to get ar\n");
		return;
	}

	trace_ath12k_htt_pktlog_tx_handler(ar, msg->payload, payload_size,
					   ar->ab->pktlog_defs_checksum);

	if (ar->debug.is_pkt_logging)
		ath12k_htt_pktlog_process(ar, (u8 *)msg->payload, payload_size);
}

static void ath12k_htt_t2h_ppdu_id_fmt_handler(struct ath12k_dp *dp,
					       struct sk_buff *skb)
{
	u8 valid, bits, offset;
	struct ath12k_htt_ppdu_id_fmt_info *msg;

	if (skb->len < sizeof(struct ath12k_htt_ppdu_id_fmt_info))
		return;

	msg = (struct ath12k_htt_ppdu_id_fmt_info *)skb->data;
	valid = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_VALID_BITS);
	bits = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_GET_BITS);
	offset = le32_get_bits(msg->link_id, HTT_PPDU_ID_FMT_GET_OFFSET);

	if (valid) {
		dp->link_id_bits = bits;
		dp->link_id_offset = offset;
	}
}

static void
ath12k_htt_pri_link_peer_migrate_indication(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	struct ath12k_htt_pri_link_migr_ind_msg *msg;
	struct ath12k_hw_group *ag = ab->ag;
	u16 vdev_id, peer_id, ml_peer_id;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_base *pri_ab;
	struct ath12k_dp *dp;
	u8 pdev_id, chip_id;
	int ret;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_pdev_dp *dp_pdev;

	if (skb->len < sizeof(struct ath12k_htt_pri_link_migr_ind_msg))
		return;

	msg = (struct ath12k_htt_pri_link_migr_ind_msg *)skb->data;

	chip_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID);
	pdev_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID);
	vdev_id = le32_get_bits(msg->info0, ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID);
	peer_id = le32_get_bits(msg->info1, ATH12K_HTT_PRI_LINK_MIGR_PEER_ID);
	ml_peer_id = le32_get_bits(msg->info1, ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt MLO pri link migr ind for peer_id 0x%x ml_peer_id 0x%x vdev_id 0x%x pdev_id 0x%x chip_id 0x%x\n",
		   peer_id, ml_peer_id, vdev_id, pdev_id, chip_id);

	ml_peer_id |= ATH12K_PEER_ML_ID_VALID;

	if (chip_id >= ATH12K_MAX_SOCS) {
		ath12k_warn(ab,
			    "htt incorrect chip id %d in MLO pri link migration event\n",
			    chip_id);
		return;
	}

	pri_ab = ag->ab[chip_id];
	if (!pri_ab) {
		ath12k_warn(ab,
			    "htt can not find ab for chip id %d in MLO pri link migration event\n",
			    chip_id);
		return;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(pri_ab, vdev_id);
	if (!arvif || !arvif->ar) {
		ath12k_err(pri_ab, "htt error in getting arvif from vdev id:%d\n",
			   vdev_id);
		rcu_read_unlock();
		return;
	}

	dp = ath12k_ab_to_dp(pri_ab);
	dp_pdev = &arvif->ar->dp;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!peer) {
		ath12k_warn(pri_ab, "htt can not find peer fo peer id %d\n",
			    peer_id);
		goto exit_pri_link_migr_ind;
	}

	if (peer->ml_id != ml_peer_id) {
		ath12k_warn(pri_ab, "htt ML peer id mis-match. Expected %d got %d\n",
			    peer->ml_id, ml_peer_id);
		goto exit_pri_link_migr_ind;
	}

	if (peer->primary_link) {
		ath12k_warn(pri_ab, "htt ML peer is already primary\n");
		goto exit_pri_link_migr_ind;
	}

	ahsta = ath12k_sta_to_ahsta(ath12k_dp_link_peer_get_sta(peer));

	ahsta->migration_data.ar = arvif->ar;
	ahsta->migration_data.vdev_id = vdev_id;
	ahsta->migration_data.peer_id = peer_id;
	ahsta->migration_data.ml_peer_id = ml_peer_id;
	ahsta->migration_data.pdev_id = pdev_id;
	ahsta->migration_data.chip_id = chip_id;
	memcpy(ahsta->migration_data.link_addr, peer->addr, ETH_ALEN);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ahsta->migration_data.ppe_vp_num = peer->dp_peer->ppe_vp_num;
#else
	ahsta->migration_data.ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
#endif
	reinit_completion(&ahsta->dp_migration_event);

	ret = ath12k_dp_peer_migrate(ahsta, peer_id, chip_id, pdev_id);
	if (ret)
		ath12k_warn(pri_ab, "htt ML peer failed to migrate (%d)\n", ret);


exit_pri_link_migr_ind:
	spin_unlock_bh(&dp->dp_lock);
	rcu_read_unlock();

	if (ahsta)
		ieee80211_queue_work(arvif->ar->ah->hw, &ahsta->migration_wk);

}

static int ath12k_svc_burst_stats_update(struct ath12k_base *ab,
					 struct ath12k_dp_peer *mld_peer,
					 u8 tid, u8 q_type, u16 svc_int_success,
					 u16 svc_int_fail, u16 burst_sz_success,
					 u16 burst_sz_fail)
{
	struct fw_mpdu_stats *svc_intval_stats;
	struct fw_mpdu_stats *burst_size_stats;
	u8 q_id;

	if (tid >= QOS_TID_MAX)
		return -EINVAL;

	if (q_type >= QOS_TID_DEF_MSDUQ_MAX + QOS_TID_MDSUQ_MAX)
		return -EINVAL;

	q_id = q_type - QOS_TID_MDSUQ_MAX;

	if (!mld_peer->mld_stats.mld_qos_stats)
		return -ENODATA;

	svc_intval_stats =
		&mld_peer->mld_stats.mld_qos_stats[tid * QOS_TID_MDSUQ_MAX
		+ q_id].svc_intval_stats;
	if (!svc_intval_stats)
		return -ENODATA;

	svc_intval_stats->success_cnt += svc_int_success;
	svc_intval_stats->failure_cnt += svc_int_fail;

	burst_size_stats =
		&mld_peer->mld_stats.mld_qos_stats[tid * QOS_TID_MDSUQ_MAX
		+ q_id].burst_size_stats;
	if (!burst_size_stats)
		return -ENODATA;

	burst_size_stats->success_cnt += burst_sz_success;
	burst_size_stats->failure_cnt += burst_sz_fail;

	return 0;
}

static int ath12k_fw_mpdu_stats_update(struct ath12k_base *ab,
				       u32 tlv_tag,
				       u8 *data)
{
	struct htt_stats_strm_gen_mpdus_tlv *mpdus_tlv;
	struct htt_stats_strm_gen_mpdus_details_tlv *mpdus_detail_tlv;
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k *ar;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_pdev_dp *dp_pdev;
	int ret = 0, vdev_id;
	u16 svc_int_success, svc_int_failure, burst_sz_success;
	u16 info, burst_sz_failure, peer_id;
	u8 tid, q_type;

	if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_TAG) {
		mpdus_tlv = (struct htt_stats_strm_gen_mpdus_tlv *)data;
		peer_id = __le16_to_cpu(mpdus_tlv->peer_id);
		info = __le16_to_cpu(mpdus_tlv->info);
		svc_int_success = __le16_to_cpu(mpdus_tlv->svc_interval_success);
		svc_int_failure = __le16_to_cpu(mpdus_tlv->svc_interval_failure);
		burst_sz_success = __le16_to_cpu(mpdus_tlv->burst_size_success);
		burst_sz_failure = __le16_to_cpu(mpdus_tlv->burst_size_failure);
	} else if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_DETAILS_TAG) {
		mpdus_detail_tlv = (struct htt_stats_strm_gen_mpdus_details_tlv *)data;
		info = __le16_to_cpu(mpdus_detail_tlv->info);
		peer_id = __le16_to_cpu(mpdus_detail_tlv->peer_id);
	} else {
		return -ENOENT;
	}

	tid = u16_get_bits(info, SAWF_TTH_TID_MASK);
	q_type = u16_get_bits(info, SAWF_TTH_QTYPE_MASK);

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_peerid_index(dp, NULL, peer_id);
	if (!link_peer) {
		rcu_read_unlock();
		return -ENOENT;
	}
	vdev_id = link_peer->vdev_id;

	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		rcu_read_unlock();
		return -ENOENT;
	}

	if (!ar->ah) {
		rcu_read_unlock();
		return -ENOENT;
	}

	dp_hw = &ar->ah->dp_hw;
	dp_pdev = &ar->dp;
	if (!(ath12k_debugfs_is_qos_stats_enabled(ar) &
	      ATH12K_QOS_STATS_ADVANCED)) {
		rcu_read_unlock();
		return -EOPNOTSUPP;
	}

	if (tlv_tag == HTT_STATS_STRM_GEN_MPDUS_TAG) {
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
		if (!dp_peer) {
			rcu_read_unlock();
			return -ENOENT;
		}
		spin_lock_bh(&dp_hw->peer_hash_lock);
		ath12k_svc_burst_stats_update(ab, dp_peer, tid,
					      q_type, svc_int_success,
					      svc_int_failure,
					      burst_sz_success,
					      burst_sz_failure);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
	} else {
		rcu_read_unlock();
		ath12k_dbg(ab, ATH12K_DBG_QOS, "SDWF: peer_id %u tid %u qtype %u "
			   "svc_intvl: ts_prior %ums ts_now %ums "
			   "intvl_spec %ums margin %ums|"
			   "burst_size: consumed_bytes_orig %u "
			   "consumed_bytes_final %u remaining_bytes %u "
			   "burst_size_spec %u margin_bytes %u\n",
			   __le16_to_cpu(mpdus_detail_tlv->peer_id),
			   tid, q_type,
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_timestamp_prior_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_timestamp_now_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_interval_spec_ms),
			   __le16_to_cpu(mpdus_detail_tlv->svc_interval_interval_margin_ms),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_consumed_bytes_orig),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_consumed_bytes_final),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_remaining_bytes),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_burst_size_spec),
			   __le16_to_cpu(mpdus_detail_tlv->burst_size_margin_bytes));
	}
	return ret;
}

void ath12k_htt_sawf_streaming_stats_ind_handler(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const struct htt_tlv *tlv;
	u8 *data = NULL;
	u8 *tlv_data;
	u32 len, tlv_tag, tlv_len;

	data = skb->data + HTT_T2H_STREAMING_STATS_IND_HDR_SIZE;
	len = skb->len;

	if (len > HTT_T2H_STREAMING_STATS_IND_HDR_SIZE)
		len -= HTT_T2H_STREAMING_STATS_IND_HDR_SIZE;
	else
		return;

	while (len > 0) {
		tlv_data = data;
		tlv = (struct htt_tlv *)data;
		tlv_tag = le32_get_bits(tlv->header, HTT_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, HTT_TLV_LEN);

		if (!tlv_len)
			break;

		if (len < tlv_len) {
			ath12k_err(ab, "SDWF: len %d tlv_len %d\n", len, tlv_len);
			break;
		}

		data += sizeof(*tlv);

		ath12k_fw_mpdu_stats_update(ab, tlv_tag, data);

		data = (tlv_data + tlv_len);
		len -= tlv_len;
	}
}

void ath12k_dp_htt_htc_t2h_msg_handler(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_resp_msg *resp = (struct htt_resp_msg *)skb->data;
	enum htt_t2h_msg_type type;
	u16 peer_id;
	u8 vdev_id;
	u8 mac_addr[ETH_ALEN];
	u16 peer_mac_h16;
	u16 ast_hash = 0;
	u16 hw_peer_id;
	bool is_wds;

	type = le32_get_bits(resp->version_msg.version, HTT_T2H_MSG_TYPE);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt rx msg type :0x%0x\n", type);
	/* Log the HTT event */
	ath12k_dp_htt_message_record(dp->htt_logger_handle, type,
				     (u8 *)skb->data, HTT_LOGGER_EVENT);

	switch (type) {
	case HTT_T2H_MSG_TYPE_VERSION_CONF:
		dp->htt_tgt_ver_major = le32_get_bits(resp->version_msg.version,
						      HTT_T2H_VERSION_CONF_MAJOR);
		dp->htt_tgt_ver_minor = le32_get_bits(resp->version_msg.version,
						      HTT_T2H_VERSION_CONF_MINOR);
		complete(&dp->htt_tgt_version_received);
		break;
	/* TODO: remove unused peer map versions after testing */
	case HTT_T2H_MSG_TYPE_PEER_MAP:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, 0, 0, false);
		break;
	case HTT_T2H_MSG_TYPE_PEER_MAP2:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ast_hash = le32_get_bits(resp->peer_map_ev.info2,
					 HTT_T2H_PEER_MAP_INFO2_AST_HASH_VAL);
		hw_peer_id = le32_get_bits(resp->peer_map_ev.info1,
					   HTT_T2H_PEER_MAP_INFO1_HW_PEER_ID);
		is_wds = le32_get_bits(resp->peer_map_ev.info2,
				       HTT_T2H_PEER_MAP_INFO2_NEXT_HOP_M);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, ast_hash,
				      hw_peer_id, is_wds);
		break;
	case HTT_T2H_MSG_TYPE_PEER_MAP3:
		vdev_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_VDEV_ID);
		peer_id = le32_get_bits(resp->peer_map_ev.info,
					HTT_T2H_PEER_MAP_INFO_PEER_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_map_ev.info1,
					     HTT_T2H_PEER_MAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_map_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		ast_hash = le32_get_bits(resp->peer_map_ev.info2,
					 HTT_T2H_PEER_MAP3_INFO2_AST_HASH_VAL);
		hw_peer_id = le32_get_bits(resp->peer_map_ev.info2,
					   HTT_T2H_PEER_MAP3_INFO2_HW_PEER_ID);
		is_wds = le32_get_bits(resp->peer_map_ev.info3,
				       HTT_T2H_PEER_MAP3_INFO3_NEXT_HOP_M);
		ath12k_peer_map_event(ab, vdev_id, peer_id, mac_addr, ast_hash,
				      hw_peer_id, is_wds);
		break;
	case HTT_T2H_MSG_TYPE_PEER_UNMAP:
		peer_id = le32_get_bits(resp->peer_unmap_ev.info,
					HTT_T2H_PEER_UNMAP_INFO_PEER_ID);
		memset(mac_addr, 0, ETH_ALEN);
		ath12k_peer_unmap_event(ab, 0xFF, peer_id, mac_addr, false);
		break;
	case HTT_T2H_MSG_TYPE_PEER_UNMAP2:
		peer_id = le32_get_bits(resp->peer_unmap_ev.info,
					HTT_T2H_PEER_UNMAP_INFO_PEER_ID);
		vdev_id = le32_get_bits(resp->peer_unmap_ev.info,
					HTT_T2H_PEER_UNMAP_INFO_VDEV_ID);
		peer_mac_h16 = le32_get_bits(resp->peer_unmap_ev.info1,
					     HTT_T2H_PEER_UNMAP_INFO1_MAC_ADDR_H16);
		ath12k_dp_get_mac_addr(le32_to_cpu(resp->peer_unmap_ev.mac_addr_l32),
				       peer_mac_h16, mac_addr);
		is_wds = le32_get_bits(resp->peer_unmap_ev.info1,
				       HTT_T2H_PEER_UNMAP_INFO1_NEXT_HOP_M);
		ath12k_peer_unmap_event(ab, vdev_id, peer_id, mac_addr, is_wds);
		break;
	case HTT_T2H_MSG_TYPE_PPDU_STATS_IND:
		ath12k_htt_pull_ppdu_stats(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_EXT_STATS_CONF:
		ath12k_debugfs_htt_ext_stats_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_BKPRESSURE_EVENT_IND:
		ath12k_htt_backpressure_event_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_MLO_TIMESTAMP_OFFSET_IND:
		ath12k_htt_mlo_offset_event_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PKTLOG:
		ath12k_htt_pktlog_tx_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_QOS_MSDUQ_INFO_IND:
		ath12k_peer_qos_queue_ind_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PPDU_ID_FMT_IND:
		ath12k_htt_t2h_ppdu_id_fmt_handler(dp, skb);
		break;
	case HTT_T2H_MSG_TYPE_STREAMING_STATS_IND:
		ath12k_htt_sawf_streaming_stats_ind_handler(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_IND:
		ath12k_htt_pri_link_peer_migrate_indication(ab, skb);
		break;
	case HTT_T2H_MSG_TYPE_GLOBAL_PEER_ID_UNMAP:
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
				 "htt rx: HTT_T2H_MSG_TYPE_GLOBAL_PEER_ID_UNMAP received\n");
		ath12k_dp_arch_peer_cleanup_indication(dp, skb);
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_DP_HTT, "dp_htt event %d not handled\n",
			   type);
		break;
	}

	dev_kfree_skb_any(skb);
}
EXPORT_SYMBOL(ath12k_dp_htt_htc_t2h_msg_handler);

#define HTT_TARGET_VERSION_TIMEOUT_HZ (3 * HZ)

int ath12k_dp_tx_htt_h2t_ver_req_msg(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ver_req_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;

	init_completion(&dp->htt_tgt_version_received);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_ver_req_cmd *)skb->data;
	cmd->ver_reg_info = le32_encode_bits(HTT_H2T_MSG_TYPE_VERSION_REQ,
					     HTT_OPTION_TAG);

	if (!ath12k_ftm_mode) {
		cmd->tcl_metadata_version = le32_encode_bits(HTT_TAG_TCL_METADATA_VERSION,
							     HTT_OPTION_TAG) |
					    le32_encode_bits(HTT_TCL_METADATA_VER_SZ,
							     HTT_OPTION_LEN) |
					    le32_encode_bits(dp->tcl_metadata_ver,
							     HTT_OPTION_VALUE);
	}

	ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_VERSION_REQ,
			      (u8 *)cmd);
	if (ret) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	ret = wait_for_completion_timeout(&dp->htt_tgt_version_received,
					  HTT_TARGET_VERSION_TIMEOUT_HZ);
	if (ret == 0) {
		ath12k_warn(ab, "htt target version request timed out\n");
		return -ETIMEDOUT;
	}

	if (dp->htt_tgt_ver_major != HTT_TARGET_VERSION_MAJOR) {
		ath12k_err(ab, "unsupported htt major version %d supported version is %d\n",
			   dp->htt_tgt_ver_major, HTT_TARGET_VERSION_MAJOR);
		return -EOPNOTSUPP;
	}

	return 0;
}

int ath12k_dp_tx_htt_h2t_ppdu_stats_req(struct ath12k *ar, u32 mask)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ppdu_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	u8 pdev_mask;
	int ret;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		skb = ath12k_htc_alloc_skb(ab, len);
		if (!skb)
			return -ENOMEM;

		skb_put(skb, len);
		cmd = (struct htt_ppdu_stats_cfg_cmd *)skb->data;
		cmd->msg = le32_encode_bits(HTT_H2T_MSG_TYPE_PPDU_STATS_CFG,
					    HTT_PPDU_STATS_CFG_MSG_TYPE);

		pdev_mask = DP_SW2HW_MACID(ar->pdev_idx) + i;
		cmd->msg |= le32_encode_bits(pdev_mask, HTT_PPDU_STATS_CFG_PDEV_ID);
		cmd->msg |= le32_encode_bits(mask, HTT_PPDU_STATS_CFG_TLV_TYPE_BITMASK);

		ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_PPDU_STATS_CFG,
				      (u8 *)cmd);
		if (ret) {
			dev_kfree_skb_any(skb);
			return ret;
		}
	}

	return 0;
}

static int
ath12k_dp_tx_get_ring_id_type(struct ath12k_base *ab,
			      int mac_id, u32 ring_id,
			      enum hal_ring_type ring_type,
			      enum htt_srng_ring_type *htt_ring_type,
			      enum htt_srng_ring_id *htt_ring_id)
{
	int ret = 0;

	switch (ring_type) {
	case HAL_RXDMA_BUF:
		/* for some targets, host fills rx buffer to fw and fw fills to
		 * rxbuf ring for each rxdma
		 */
		if (!ab->hw_params->rx_mac_buf_ring) {
			if (!(ring_id == HAL_SRNG_SW2RXDMA_BUF0 ||
			      ring_id == HAL_SRNG_SW2RXDMA_BUF1)) {
				ret = -EINVAL;
			}
			*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
			*htt_ring_type = HTT_SW_TO_HW_RING;
		} else {
			if (ring_id == HAL_SRNG_SW2RXDMA_BUF0) {
				*htt_ring_id = HTT_HOST1_TO_FW_RXBUF_RING;
				*htt_ring_type = HTT_SW_TO_SW_RING;
#ifdef CPTCFG_EXT_IPA_OFFLOAD
			} else if (ring_id == HAL_SRNG_SW2RXDMA_BUF1) {
				*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
				*htt_ring_type = HTT_SW_TO_HW_RING;
			} else if (ring_id == HAL_SRNG_SW2RXDMA_BUF2) {
				*htt_ring_id = HTT_HOST2_TO_FW_RXBUF_RING;
				*htt_ring_type = HTT_SW_TO_SW_RING;
#endif
			} else {
				*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
				*htt_ring_type = HTT_SW_TO_HW_RING;
			}
		}
		break;
	case HAL_RXDMA_DST:
		*htt_ring_id = HTT_RXDMA_NON_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		if (ath12k_dp_mon_rx_get_quad_ring_support(ab->dp))
			*htt_ring_id = HTT_RXDMA_MONITOR_BUF_RING;
		else
			*htt_ring_id = HTT_RX_MON_HOST2MON_BUF_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		*htt_ring_id = HTT_RXDMA_MONITOR_STATUS_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_DST:
		if (ath12k_dp_mon_rx_get_quad_ring_support(ab->dp))
			*htt_ring_id = HTT_RXDMA_MONITOR_DEST_RING;
		else
			*htt_ring_id = HTT_RX_MON_MON2HOST_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_DESC:
		*htt_ring_id = HTT_RXDMA_MONITOR_DESC_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_WBM_IDLE_BUF:
		*htt_ring_type = HTT_SW_TO_HW_RING;
		*htt_ring_id = HTT_RXDMA_WBM_BUF0_RING;
		break;
	case HAL_PPE2WBM_IDLE_BUF:
		*htt_ring_type = HTT_SW_TO_HW_RING;
		*htt_ring_id = HTT_RXDMA_WBM_BUF2_RING;
		break;
	case HAL_TX_MONITOR_BUF:
		*htt_ring_id = HTT_TX_MON_HOST2MON_BUF_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_TX_MONITOR_DST:
		*htt_ring_id = HTT_TX_MON_MON2HOST_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_WBM_IDLE_BUF_MGMT:
		*htt_ring_type = HTT_SW_TO_HW_RING;
		*htt_ring_id = HTT_RXDMA_WBM_BUF1_RING;
		break;
	case HAL_ASE_STATUS_RING:
		*htt_ring_id = HTT_RXOLE_ASE_STATUS_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	default:
		ath12k_warn(ab, "Unsupported ring type in DP :%d\n", ring_type);
		ret = -EINVAL;
	}
	return ret;
}

int ath12k_dp_tx_htt_srng_setup(struct ath12k_base *ab, u32 ring_id,
				int mac_id, enum hal_ring_type ring_type)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_srng_setup_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	u32 ring_entry_sz;
	int len = sizeof(*cmd);
	dma_addr_t hp_addr, tp_addr;
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	hp_addr = ath12k_hal_srng_get_hp_addr(ab, srng);
	tp_addr = ath12k_hal_srng_get_tp_addr(ab, srng);

	if ((srng->flags & HAL_SRNG_FLAGS_REG_WRITE_EN)) {
		if (srng->ring_dir == HAL_SRNG_DIR_SRC)
			hp_addr = 0;

		if (srng->ring_dir == HAL_SRNG_DIR_DST)
			tp_addr = 0;
	}

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_srng_setup_cmd *)skb->data;
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_SRING_SETUP,
				      HTT_SRNG_SETUP_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |= le32_encode_bits(DP_SW2HW_MACID(mac_id),
					       HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |= le32_encode_bits(mac_id,
					       HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID);
	cmd->info0 |= le32_encode_bits(htt_ring_type,
				       HTT_SRNG_SETUP_CMD_INFO0_RING_TYPE);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_SRNG_SETUP_CMD_INFO0_RING_ID);

	cmd->ring_base_addr_lo = cpu_to_le32(params.ring_base_paddr &
					     HAL_ADDR_LSB_REG_MASK);

	cmd->ring_base_addr_hi = cpu_to_le32((u64)params.ring_base_paddr >>
					     HAL_ADDR_MSB_REG_SHIFT);

	ret = ath12k_hal_srng_get_entrysize(ab, ring_type);
	if (ret < 0)
		goto err_free;

	ring_entry_sz = ret;

	ring_entry_sz >>= 2;
	cmd->info1 = le32_encode_bits(ring_entry_sz,
				      HTT_SRNG_SETUP_CMD_INFO1_RING_ENTRY_SIZE);
	cmd->info1 |= le32_encode_bits(params.num_entries * ring_entry_sz,
				       HTT_SRNG_SETUP_CMD_INFO1_RING_SIZE);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_MSI_SWAP);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_TLV_SWAP);
	cmd->info1 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_RING_PTR_SWAP),
				       HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_HOST_FW_SWAP);
	if (htt_ring_type == HTT_SW_TO_HW_RING)
		cmd->info1 |= cpu_to_le32(HTT_SRNG_SETUP_CMD_INFO1_RING_LOOP_CNT_DIS);

	cmd->ring_head_off32_remote_addr_lo = cpu_to_le32(lower_32_bits(hp_addr));
	cmd->ring_head_off32_remote_addr_hi = cpu_to_le32(upper_32_bits(hp_addr));

	cmd->ring_tail_off32_remote_addr_lo = cpu_to_le32(lower_32_bits(tp_addr));
	cmd->ring_tail_off32_remote_addr_hi = cpu_to_le32(upper_32_bits(tp_addr));

	cmd->ring_msi_addr_lo = cpu_to_le32(lower_32_bits(params.msi_addr));
	cmd->ring_msi_addr_hi = cpu_to_le32(upper_32_bits(params.msi_addr));
	cmd->msi_data = cpu_to_le32(params.msi_data);

	cmd->intr_info =
		le32_encode_bits(params.intr_batch_cntr_thres_entries * ring_entry_sz,
				 HTT_SRNG_SETUP_CMD_INTR_INFO_BATCH_COUNTER_THRESH);
	cmd->intr_info |=
		le32_encode_bits(params.intr_timer_thres_us >> 3,
				 HTT_SRNG_SETUP_CMD_INTR_INFO_INTR_TIMER_THRESH);

	cmd->info2 = 0;
	if (params.flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		cmd->info2 = le32_encode_bits(params.low_threshold,
					      HTT_SRNG_SETUP_CMD_INFO2_INTR_LOW_THRESH);
	}

	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "%s msi_addr_lo:0x%x, msi_addr_hi:0x%x, msi_data:0x%x\n",
		   __func__, cmd->ring_msi_addr_lo, cmd->ring_msi_addr_hi,
		   cmd->msi_data);

	ath12k_dbg(ab, ATH12K_DBG_HAL,
		   "ring_id:%d, ring_type:%d, intr_info:0x%x, flags:0x%x\n",
		   ring_id, ring_type, cmd->intr_info, cmd->info2);

	ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_SRING_SETUP,
			      (u8 *)cmd);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_htt_srng_setup);

void ath12k_dp_get_htt_mgmt_filter(struct ath12k_base *ab,
				   struct htt_rx_ring_tlv_filter *tlv_filter)
{
	u16 ctrl_filter = FILTER_CTRL_BA_REQ;
	u16 mgmt_filter = FILTER_MGMT_ALL;

	mgmt_filter &= ~(FILTER_MGMT_PROBE_REQ | FILTER_MGMT_PROBE_RESP |
			 FILTER_MGMT_BEACON);

	tlv_filter->fp_mgmt_filter = mgmt_filter;
	tlv_filter->fp_ctrl_filter = ctrl_filter;
}
EXPORT_SYMBOL(ath12k_dp_get_htt_mgmt_filter);

void ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_mgmt_flag0_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_mgmt_flag0_fp_filter_set(ptr,
							     tlv_filter->fp_mgmt_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_mgmt_flag0_mo_filter_set(ptr,
							     tlv_filter->mo_mgmt_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_mgmt_flag0_md_filter_set(ptr,
							     tlv_filter->md_mgmt_filter);
}

void ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, MGMT, FLAGS1, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_mgmt_flag1_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_mgmt_flag1_fp_filter_set(ptr,
							     tlv_filter->fp_mgmt_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_mgmt_flag1_mo_filter_set(ptr,
							     tlv_filter->mo_mgmt_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_mgmt_flag1_md_filter_set(ptr,
							     tlv_filter->md_mgmt_filter);
}

void ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS2, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_ctrl_flag2_filter_set(u32 *ptr,
					  struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp)
		ath12k_dp_tx_htt_rx_ctrl_flag2_fp_filter_set(ptr,
							     tlv_filter->fp_ctrl_filter);
	if (tlv_filter->enable_mo)
		ath12k_dp_tx_htt_rx_ctrl_flag2_mo_filter_set(ptr,
							     tlv_filter->mo_ctrl_filter);
	if (tlv_filter->enable_md)
		ath12k_dp_tx_htt_rx_ctrl_flag2_md_filter_set(ptr,
							     tlv_filter->md_ctrl_filter);
}

void ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FP, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MO, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, CTRL, FLAGS3, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, MD, DATA, FLASG3, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

static void
ath12k_dp_tx_htt_rx_ctrl_data_flag3_filter_set(u32 *ptr,
					       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	if (tlv_filter->enable_fp) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_fp_filter_set(ptr,
							     tlv_filter->fp_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_fp_filter_set(ptr,
							     tlv_filter->fp_data_filter);
	}
	if (tlv_filter->enable_mo) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_mo_filter_set(ptr,
							     tlv_filter->mo_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_mo_filter_set(ptr,
							     tlv_filter->mo_data_filter);
	}
	if (tlv_filter->enable_md) {
		ath12k_dp_tx_htt_rx_ctrl_flag3_md_filter_set(ptr,
							     tlv_filter->md_ctrl_filter);
		ath12k_dp_tx_htt_rx_data_flag3_md_filter_set(ptr,
							     tlv_filter->md_data_filter);
	}
}

void
ath12k_dp_tx_htt_rx_mgmt_fpmo_flag0_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, ASSOC_REQ,
				      (filter & FILTER_MGMT_ASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, ASSOC_RESP,
				      (filter & FILTER_MGMT_ASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, REASSOC_REQ,
				      (filter & FILTER_MGMT_REASSOC_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, REASSOC_RESP,
				      (filter & FILTER_MGMT_REASSOC_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, PROBE_REQ,
				      (filter & FILTER_MGMT_PROBE_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, PROBE_RESP,
				      (filter & FILTER_MGMT_PROBE_RESP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, PROBE_TIMING_ADV,
				      (filter & FILTER_MGMT_TIM_ADVT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, RESERVED_7,
				      (filter & FILTER_MGMT_RESERVED_7) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, BEACON,
				      (filter & FILTER_MGMT_BEACON) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, ATIM,
				      (filter & FILTER_MGMT_ATIM) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, DISASSOC,
				      (filter & FILTER_MGMT_DISASSOC) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, AUTH,
				      (filter & FILTER_MGMT_AUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, DEAUTH,
				      (filter & FILTER_MGMT_DEAUTH) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, ACTION,
				      (filter & FILTER_MGMT_ACTION) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, ACTION_NOACK,
				      (filter & FILTER_MGMT_ACT_NO_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, MGMT, FLAGS0, RESERVED_15,
				      (filter & FILTER_MGMT_RESERVED_15) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_ctrl_fpmo_flag0_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_RESERVED_1,
				      (filter & FILTER_CTRL_RESERVED_1) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_RESERVED_2,
				      (filter & FILTER_CTRL_RESERVED_2) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_TRIGGER,
				      (filter & FILTER_CTRL_TRIGGER) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_RESERVED_4,
				      (filter & FILTER_CTRL_RESERVED_4) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_BF_REP_POLL,
				      (filter & FILTER_CTRL_BF_REP_POLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_VHT_NDP,
				      (filter & FILTER_CTRL_VHT_NDP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_FRAME_EXT,
				      (filter & FILTER_CTRL_FRAME_EXT) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTRL_WRAPPER,
				      (filter & FILTER_CTRL_CTRLWRAP) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, BAR,
				      (filter & FILTER_CTRL_BA_REQ) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, BA,
				      (filter & FILTER_CTRL_BA) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, PSPOLL,
				      (filter & FILTER_CTRL_PSPOLL) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, RTS,
				      (filter & FILTER_CTRL_RTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CTS,
				      (filter & FILTER_CTRL_CTS) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, ACK,
				      (filter & FILTER_CTRL_ACK) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CFEND,
				      (filter & FILTER_CTRL_CFEND) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, CTRL, FLAGS0, CFEND_ACK,
				      (filter & FILTER_CTRL_CFEND_CFACK) ? 1 : 0);
}

void
ath12k_dp_tx_htt_rx_data_fpmo_flag1_filter_set(u32 *ptr, u16 filter)
{
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, DATA, FLAGS1, MCAST,
				      (filter & FILTER_DATA_MCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, DATA, FLAGS1, UCAST,
				      (filter & FILTER_DATA_UCAST) ? 1 : 0);
	HTT_RX_PKT_ENABLE_SUBTYPE_SET(*ptr, FPMO, DATA, FLAGS1, NULL_DATA,
				      (filter & FILTER_DATA_NULL) ? 1 : 0);
}

int ath12k_dp_tx_htt_rx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int rx_buf_size, u8 rx_pktlog_mode,
				     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	int len = sizeof(*cmd);
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;
	u32 word = 0;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_rx_ring_selection_cfg_cmd *)skb->data;
	memset(cmd, 0, len);
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |=
			le32_encode_bits(DP_SW2HW_MACID(mac_id),
					 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |=
			le32_encode_bits(mac_id,
					 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_RING_ID);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_SS);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PS);
	cmd->info0 |= le32_encode_bits(tlv_filter->offset_valid,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO0_OFFSET_VALID);
	cmd->info0 |=
		le32_encode_bits(tlv_filter->drop_threshold_valid,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO0_DROP_THRES_VAL);

	ath12k_dp_mon_rx_enable(dp, cmd, tlv_filter);

	cmd->info1 = le32_encode_bits(rx_buf_size,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO1_BUF_SIZE);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_mgmt,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_MGMT);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_ctrl,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_CTRL);
	cmd->info1 |= le32_encode_bits(tlv_filter->conf_len_data,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO1_CONF_LEN_DATA);
	if (!tlv_filter->rxmon_disable)
		cmd->info1 |=
			le32_encode_bits(tlv_filter->rx_hdr_len,
					 HTT_RX_RING_SEL_CFG_CMD_INFO1_CONF_HDR_LEN);

	word = 0;
	ath12k_dp_tx_htt_rx_mgmt_flag0_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags0 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_mgmt_flag1_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags1 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_ctrl_flag2_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags2 = cpu_to_le32(word);

	word = 0;
	ath12k_dp_tx_htt_rx_ctrl_data_flag3_filter_set(&word, tlv_filter);
	cmd->pkt_type_en_flags3 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fpmo) {
		ath12k_dp_tx_htt_rx_mgmt_fpmo_flag0_filter_set(&word,
							tlv_filter->fpmo_mgmt_filter);
		ath12k_dp_tx_htt_rx_ctrl_fpmo_flag0_filter_set(&word,
							tlv_filter->fpmo_ctrl_filter);
	}
	cmd->pkt_type_en_fpmo_flags0 = cpu_to_le32(word);

	word = 0;
	if (tlv_filter->enable_fpmo)
		ath12k_dp_tx_htt_rx_data_fpmo_flag1_filter_set(&word,
							tlv_filter->fpmo_data_filter);
	cmd->pkt_type_en_fpmo_flags1 = cpu_to_le32(word);

	cmd->rx_filter_tlv = cpu_to_le32(tlv_filter->rx_filter);

	cmd->info2 = le32_encode_bits(tlv_filter->rx_drop_threshold,
				      HTT_RX_RING_SELECTION_CFG_CMD_INFO2_DROP_THRESHOLD);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_mgmt_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_MGMT_TYPE);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_ctrl_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_CTRL_TYPE);
	cmd->info2 |=
		le32_encode_bits(tlv_filter->enable_log_data_type,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO2_EN_LOG_DATA_TYPE);

	ath12k_dp_mon_rx_phy_err_filter_cfg(dp, cmd, tlv_filter);

	cmd->info3 =
		le32_encode_bits(tlv_filter->enable_rx_tlv_offset,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO3_EN_TLV_PKT_OFFSET);
	cmd->info3 |=
		le32_encode_bits(tlv_filter->rx_tlv_offset,
				 HTT_RX_RING_SELECTION_CFG_CMD_INFO3_PKT_TLV_OFFSET);

	if (tlv_filter->offset_valid) {
		cmd->rx_packet_offset =
			le32_encode_bits(tlv_filter->rx_packet_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_PACKET_OFFSET);

		cmd->rx_packet_offset |=
			le32_encode_bits(tlv_filter->rx_header_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_HEADER_OFFSET);

		cmd->rx_mpdu_offset =
			le32_encode_bits(tlv_filter->rx_mpdu_end_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_OFFSET);

		cmd->rx_mpdu_offset |=
			le32_encode_bits(tlv_filter->rx_mpdu_start_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_OFFSET);

		cmd->rx_msdu_offset =
			le32_encode_bits(tlv_filter->rx_msdu_end_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_OFFSET);

		cmd->rx_msdu_offset |=
			le32_encode_bits(tlv_filter->rx_msdu_start_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_MSDU_START_OFFSET);

		cmd->rx_attn_offset =
			le32_encode_bits(tlv_filter->rx_attn_offset,
					 HTT_RX_RING_SELECTION_CFG_RX_ATTENTION_OFFSET);
	}

	if (rx_pktlog_mode == ATH12K_PKTLOG_DISABLED) {
		if (tlv_filter->rx_mpdu_start_wmask > 0 &&
		    tlv_filter->rx_msdu_end_wmask > 0) {
			cmd->info2 |=
				le32_encode_bits(true,
						 HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACT_SET);
			cmd->rx_mpdu_start_end_mask =
				le32_encode_bits(tlv_filter->rx_mpdu_start_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_MASK);
			/* mpdu_end is not used for any hardwares so far
			 * please assign it in future if any chip is
			 * using through hal ops
			 */
			cmd->rx_mpdu_start_end_mask |=
				le32_encode_bits(tlv_filter->rx_mpdu_end_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MPDU_END_MASK);
			cmd->rx_msdu_end_word_mask =
				le32_encode_bits(tlv_filter->rx_msdu_end_wmask,
						 HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_MASK);
		}

		ath12k_dp_mon_rx_config_wmask(dp, cmd, tlv_filter);
	}

	cmd->rdi_based_source_cfg = cpu_to_le32(tlv_filter->rdi_based_source_cfg);

	ath12k_dp_mon_rx_config_packet_type_subtype(dp, cmd, tlv_filter);

	cmd->info4 = le32_encode_bits(tlv_filter->rx_mon_fpmo_data_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FPMO_DATA_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_fpmo_ctrl_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FPMO_CTRL_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_fpmo_mgmt_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FPMO_MGMT_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_fp_data_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FP_DATA_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_fp_ctrl_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FP_CTRL_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_fp_mgmt_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_FP_MGMT_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_mo_data_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MO_DATA_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_mo_ctrl_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MO_CTRL_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_mo_mgmt_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MO_MGMT_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_md_data_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MD_DATA_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_md_ctrl_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MD_CTRL_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_md_mgmt_hdrlen,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_MD_MGMT_HDRLEN);
	cmd->info4 |= le32_encode_bits(tlv_filter->rx_mon_enable_hdr_per_ppdu,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_RXMON_ENABLE_HDR_PER_PPDU);
	cmd->info4 |= le32_encode_bits(tlv_filter->fp_fpmo_data_mpdu_filter_in_en,
			HTT_RX_RING_SEL_CFG_CMD_INFO4_FP_FPMO_DATA_MPDU_FILTER_IN_EN);

	cmd->fp_data_mpdu_tlv_filter0 =
		cpu_to_le32(tlv_filter->fp_data_mpdu_tlv_filter0);
	cmd->fpmo_data_mpdu_tlv_filter0 =
		cpu_to_le32(tlv_filter->fpmo_data_mpdu_tlv_filter0);

	cmd->info5 = le32_encode_bits(tlv_filter->fp_data_mpdu_tlv_filter1,
			HTT_RX_RING_SEL_CFG_CMD_INFO5_FP_DATA_MPDU_TLV_FILTER_1);
	cmd->info5 |= le32_encode_bits(tlv_filter->fpmo_data_mpdu_tlv_filter1,
			HTT_RX_RING_SEL_CFG_CMD_INFO5_FPMO_DATA_MPDU_TLV_FILTER_1);

	ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG,
			      (u8 *)cmd);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_htt_rx_filter_setup);

int
ath12k_dp_tx_htt_h2t_ext_stats_req(struct ath12k *ar, u8 type,
				   struct htt_ext_stats_cfg_params *cfg_params,
				   u64 cookie)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	struct htt_ext_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;
	u32 pdev_id;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_ext_stats_cfg_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.msg_type = HTT_H2T_MSG_TYPE_EXT_STATS_CFG;

	pdev_id = ath12k_mac_get_target_pdev_id(ar);
	cmd->hdr.pdev_mask = 1 << pdev_id;

	cmd->hdr.stats_type = type;
	cmd->cfg_param0 = cpu_to_le32(cfg_params->cfg0);
	cmd->cfg_param1 = cpu_to_le32(cfg_params->cfg1);
	cmd->cfg_param2 = cpu_to_le32(cfg_params->cfg2);
	cmd->cfg_param3 = cpu_to_le32(cfg_params->cfg3);
	cmd->cookie_lsb = cpu_to_le32(lower_32_bits(cookie));
	cmd->cookie_msb = cpu_to_le32(upper_32_bits(cookie));

	ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_EXT_STATS_CFG,
			      (u8 *)cmd);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type stats request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath12k_dp_htt_mon_tx_filter_setup(struct ath12k_base *ab, u32 ring_id,
				      int mac_id, enum hal_ring_type ring_type,
				      int tx_buf_size,
				      struct htt_tx_ring_tlv_filter *htt_tlv_filter)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	const struct ath12k_dp_arch_mon_ops *mon_ops = ath12k_dp_mon_ops_get(dp);
	struct htt_tx_mon_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	int len;
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	/* htt_tx_mon_cfg_msg_size is set to zero for chips that don't support
	 * TX monitor (e.g. WCN7850, QCN9625); ath12k_dp is kzalloc'd so chips
	 * whose dp_op_device_init doesn't populate the field are safe to check
	 * here without a separate mon_ops null-guard.
	 */
	if (!dp->htt_tx_mon_cfg_msg_size)
		return -EOPNOTSUPP;

	len = dp->htt_tx_mon_cfg_msg_size;
	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);

	ret = ath12k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);

	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_tx_mon_ring_selection_cfg_cmd *)skb->data;
	memset(cmd, 0, len);

	/*word 0*/
	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_TX_MONITOR_CFG,
				      HTT_TX_MON_RING_CFG_CMD_INFO0_MSG_TYPE);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |=
			le32_encode_bits(DP_SW2HW_MACID(mac_id),
					 HTT_TX_MON_RING_CFG_CMD_INFO0_PDEV_ID);
	else
		cmd->info0 |=
			le32_encode_bits(mac_id,
					 HTT_TX_MON_RING_CFG_CMD_INFO0_PDEV_ID);

	cmd->info0 |= le32_encode_bits(!htt_tlv_filter->txmon_disable,
				       HTT_TX_MON_RING_CFG_CMD_INFO0_EN_TXMON);
	cmd->info0 |= le32_encode_bits(htt_ring_id,
				       HTT_TX_MON_RING_CFG_CMD_INFO0_RING_ID);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP),
				       HTT_TX_MON_RING_CFG_CMD_INFO0_SS);
	cmd->info0 |= le32_encode_bits(!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP),
				       HTT_TX_MON_RING_CFG_CMD_INFO0_PS);

	/*Custom Classify Filter*/
	cmd->info0 |=
		le32_encode_bits(!!htt_tlv_filter->mac_addr_filter_en,
				 HTT_TX_MON_RING_CFG_CMD_INFO0_MAC_ADDR_FLTR_CMD);
	cmd->info0 |=
		le32_encode_bits(!!htt_tlv_filter->pkt_buf_cnt_en,
				 HTT_TX_MON_RING_CFG_CMD_INFO0_PKT_BUF_CNT_EN);
	/*word 1*/
	cmd->info1 |=
		le32_encode_bits(tx_buf_size,
				 HTT_TX_MON_RING_CFG_CMD_INFO1_RING_BUFF_SIZE);
	cmd->info1 |=
		le32_encode_bits(dp->htt_tx_mon_cfg_version,
				 HTT_TX_MON_RING_CFG_CMD_INFO1_VERSION);

	/*word 1 & 2*/
	if (htt_tlv_filter->tx_mon_mgmt_filter) {
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_mgmt_pkt_dma_len,
				 HTT_TX_MON_RING_CFG_CMD_INFO1_CONF_DMA_LEN_MGMT);
		cmd->info2 |=
		le32_encode_bits(!!htt_tlv_filter->tx_mon_mgmt_filter,
				 HTT_TX_MON_FRAME_CTRL_INFO2_TYPE_MGMT);
	}
	/*word 1 & 2*/
	if (htt_tlv_filter->tx_mon_data_filter) {
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_data_pkt_dma_len,
				 HTT_TX_MON_RING_CFG_CMD_INFO1_CONF_DMA_LEN_DATA);
		cmd->info2 |=
		le32_encode_bits(!!htt_tlv_filter->tx_mon_data_filter,
				 HTT_TX_MON_FRAME_CTRL_INFO2_TYPE_DATA);
	}
	/*word 1 & 2*/
	if (htt_tlv_filter->tx_mon_ctrl_filter) {
		cmd->info1 |=
		le32_encode_bits(htt_tlv_filter->tx_mon_ctrl_pkt_dma_len,
				 HTT_TX_MON_RING_CFG_CMD_INFO1_CONF_DMA_LEN_CTRL);
		cmd->info2 |=
		le32_encode_bits(!!htt_tlv_filter->tx_mon_ctrl_filter,
				 HTT_TX_MON_FRAME_CTRL_INFO2_TYPE_CTRL);
	}
	/*word 2*/
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->mgmt_mpdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPSM);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->ctrl_mpdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPSC);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->data_mpdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPSD);

	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->mgmt_msdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSSM);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->ctrl_msdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSSC);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->data_msdu_start,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSSD);

	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->mgmt_mpdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPEM);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->ctrl_mpdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPEC);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->data_mpdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MPED);

	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->mgmt_msdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSEM);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->ctrl_msdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSEC);
	cmd->info2 |= le32_encode_bits(!!htt_tlv_filter->data_msdu_end,
				       HTT_TX_MON_FRAME_CTRL_INFO2_MSED);
	cmd->info2 |=
		le32_encode_bits(!!htt_tlv_filter->wmask.compaction_enable,
				 HTT_TX_MON_FRAME_CTRL_INFO2_EN_COMPACTION);

	/*word 3*/
	cmd->tlv_filter_mask_in0 =
		cpu_to_le32(htt_tlv_filter->tx_mon_downstream_tlv_flags);

	/*word 4*/
	cmd->tlv_filter_mask_in1 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags0);

	/*word 5*/
	cmd->tlv_filter_mask_in2 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags1);

	/*word 6*/
	cmd->tlv_filter_mask_in3 =
		cpu_to_le32(htt_tlv_filter->tx_mon_upstream_tlv_flags2);

	/*word 7*/
	cmd->tlv_word_mask_in0 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_fes_setup,
				 HTT_TX_MON_WMASK_IN0_FES_SETUP_MASK);

	cmd->tlv_word_mask_in0 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_peer_entry,
				 HTT_TX_MON_WMASK_IN0_PEER_ENTRY_MASK_V1);

	cmd->tlv_word_mask_in0 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_queue_ext,
				 HTT_TX_MON_WMASK_IN0_TX_QUEUE_EXT_MASK_V1);

	cmd->tlv_word_mask_in0 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_msdu_start,
				 HTT_TX_MON_WMASK_IN0_MSDU_START_MASK);

	/*word 8*/
	cmd->tlv_word_mask_in1 |=
		le32_encode_bits(htt_tlv_filter->wmask.pcu_ppdu_setup_init,
				 HTT_TX_MON_WMASK_IN1_PCU_PPDU_SETUP_INIT_MASK);

	/*word 9*/
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_mpdu_start,
				 HTT_TX_MON_WMASK_IN2_MPDU_START_MASK);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(htt_tlv_filter->wmask.rxpcu_user_setup,
				 HTT_TX_MON_WMASK_IN2_RXPCU_USER_SETUP_MASK);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(htt_tlv_filter->mgmt_mpdu_msdu_log_en,
				 HTT_TX_MON_WMASK_IN2_MGMT_MPDU_MSDU_LOG_EN);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(!!htt_tlv_filter->ctrl_mpdu_msdu_log_en,
				 HTT_TX_MON_WMASK_IN2_CTRL_MPDU_MSDU_LOG_EN);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(!!htt_tlv_filter->data_mpdu_msdu_log_en,
				 HTT_TX_MON_WMASK_IN2_DATA_MPDU_MSDU_LOG_EN);

	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(!!htt_tlv_filter->mgmt_log_typ,
				 HTT_TX_MON_WMASK_IN2_MGMT_LOG_TYP);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(!!htt_tlv_filter->ctrl_log_typ,
				 HTT_TX_MON_WMASK_IN2_CTRL_LOG_TYP);
	cmd->tlv_word_mask_in2 |=
		le32_encode_bits(!!htt_tlv_filter->data_log_typ,
				 HTT_TX_MON_WMASK_IN2_DATA_LOG_TYP);

	/*word 10*/
	cmd->tlv_word_mask_in3 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_queue_ext,
				 HTT_TX_MON_WMASK_IN3_TX_QUEUE_EXT_MASK_V2);

	cmd->tlv_word_mask_in3 |=
	le32_encode_bits(htt_tlv_filter->wmask.tx_peer_entry,
			 HTT_TX_MON_WMASK_IN3_PEER_ENTRY_MASK_V2);

	/*word 11*/
	cmd->tlv_word_mask_in4 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_fes_status_end,
				 HTT_TX_MON_WMASK_IN4_FES_STATUS_END_MASK);
	cmd->tlv_word_mask_in4 |=
		le32_encode_bits(htt_tlv_filter->wmask.response_end_status,
				 HTT_TX_MON_WMASK_IN4_RESPONSE_END_STATUS_MASK);

	/*word 12*/
	cmd->tlv_word_mask_in5 |=
		le32_encode_bits(htt_tlv_filter->wmask.tx_fes_status_prot,
				 HTT_TX_MON_WMASK_IN5_FES_STATUS_PROT_MASK);

	if (mon_ops && mon_ops->htt_tx_mon_cfg_fill_extended_wmask)
		mon_ops->htt_tx_mon_cfg_fill_extended_wmask(cmd, htt_tlv_filter);

	ret = ath12k_htt_send(ab, dp, skb, HTT_H2T_MSG_TYPE_TX_MONITOR_CFG,
			      (u8 *)cmd);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int
ath12k_dp_htt_rx_flow_fst_setup(struct ath12k_base *ab,
				struct htt_rx_flow_fst_setup *setup_info)
{
	struct sk_buff *skb;
	struct htt_rx_flow_fst_setup_cmd *cmd;
	int ret;
	u32 *key;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_rx_flow_fst_setup_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_SETUP_CFG,
				      HTT_DP_RX_FLOW_FST_SETUP_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(0, HTT_DP_RX_FLOW_FST_SETUP_PDEV_ID);

	cmd->info1 = le32_encode_bits(setup_info->max_entries,
				      HTT_DP_RX_FLOW_FST_SETUP_NUM_RECORDS);
	cmd->info1 |= le32_encode_bits(setup_info->max_search,
				       HTT_DP_RX_FLOW_FST_SETUP_MAX_SEARCH);
	cmd->info1 |= le32_encode_bits(setup_info->ip_da_sa_prefix,
				       HTT_DP_RX_FLOW_FST_SETUP_IP_DA_SA);

	cmd->base_addr_lo = setup_info->base_addr_lo;
	cmd->base_addr_hi = setup_info->base_addr_hi;

	key = (u32 *)setup_info->hash_key;
	cmd->toeplitz31_0 = *key++;
	cmd->toeplitz63_32 = *key++;
	cmd->toeplitz95_64 = *key++;
	cmd->toeplitz127_96 = *key++;
	cmd->toeplitz159_128 = *key++;
	cmd->toeplitz191_160 = *key++;
	cmd->toeplitz223_192 = *key++;
	cmd->toeplitz255_224 = *key++;
	cmd->toeplitz287_256 = *key++;
	cmd->info2 = le32_encode_bits(*key, HTT_DP_RX_FLOW_FST_SETUP_TOEPLITZ);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FST setup HTT message:",
			(void *)cmd, len);

	ret = ath12k_htt_send(ab, ath12k_ab_to_dp(ab), skb,
			      HTT_H2T_MSG_TYPE_RX_FSE_SETUP_CFG, (u8 *)cmd);
	if (ret) {
		ath12k_err(ab, "DP FSE setup msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE setup msg sent from host\n");

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}

int ath12k_dp_htt_rx_flow_fse_operation(struct ath12k_base *ab,
					enum dp_flow_fst_operation op_code,
					struct hal_flow_tuple_info *tuple_info)
{
	struct sk_buff *skb;
	struct htt_rx_msg_fse_operation *cmd;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_rx_msg_fse_operation *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_OPERATION_CFG,
				      HTT_H2T_MSG_RX_FSE_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(0, HTT_H2T_MSG_RX_FSE_PDEV_ID);
	cmd->info1 = le32_encode_bits(false, HTT_H2T_MSG_RX_FSE_IPSEC_VALID);

	if (op_code == DP_FST_CACHE_INVALIDATE_ENTRY) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_CACHE_INVALIDATE_ENTRY,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
		cmd->ip_src_addr_31_0 = htonl(tuple_info->src_ip_31_0);
		cmd->ip_src_addr_63_32 = htonl(tuple_info->src_ip_63_32);
		cmd->ip_src_addr_95_64 = htonl(tuple_info->src_ip_95_64);
		cmd->ip_src_addr_127_96 = htonl(tuple_info->src_ip_127_96);
		cmd->ip_dest_addr_31_0 = htonl(tuple_info->dest_ip_31_0);
		cmd->ip_dest_addr_63_32 = htonl(tuple_info->dest_ip_63_32);
		cmd->ip_dest_addr_95_64 = htonl(tuple_info->dest_ip_95_64);
		cmd->ip_dest_addr_127_96 = htonl(tuple_info->dest_ip_127_96);
		cmd->info2 = le32_encode_bits(tuple_info->src_port,
					      HTT_H2T_MSG_RX_FSE_SRC_PORT);
		cmd->info2 |= le32_encode_bits(tuple_info->dest_port,
					       HTT_H2T_MSG_RX_FSE_DEST_PORT);
		cmd->info3 = le32_encode_bits(tuple_info->l4_protocol,
					      HTT_H2T_MSG_RX_FSE_L4_PROTO);
	} else if (op_code == DP_FST_CACHE_INVALIDATE_FULL) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_CACHE_INVALIDATE_FULL,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	} else if (op_code == DP_FST_DISABLE) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_DISABLE,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	} else if (op_code == DP_FST_ENABLE) {
		cmd->info1 |= le32_encode_bits(HTT_RX_FSE_ENABLE,
					       HTT_H2T_MSG_RX_FSE_OPERATION);
	}

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FSE HTT message:",
			(void *)cmd, len);

	ret = ath12k_htt_send(ab, ath12k_ab_to_dp(ab), skb,
			      HTT_H2T_MSG_TYPE_RX_FSE_OPERATION_CFG,
			      (u8 *)cmd);
	if (ret) {
		ath12k_warn(ab, "DP FSE operation msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE operation msg sent from host\n");
	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_htt_rx_flow_fse_operation);

int ath12k_dp_htt_rx_fse_3_tuple_config_send(struct ath12k_base *ab,
					     u32 tuple_mask, u8 pdev_id)
{
	struct sk_buff *skb;
	struct htt_h2t_msg_rx_3_tuple_hash_cfg *cmd;
	int ret;
	int len = sizeof(*cmd);

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_h2t_msg_rx_3_tuple_hash_cfg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_RX_FSE_3_TUPLE_HASH_CFG,
				      HTT_H2T_MSG_RX_FSE_3_TUPLE_MSG_TYPE);
	cmd->info0 |= le32_encode_bits(pdev_id, HTT_H2T_MSG_RX_FSE_PDEV_ID);
	cmd->info1 |= le32_encode_bits(tuple_mask,
				       HTT_H2T_FLOW_CLASSIFY_3_TUPLE_FIELD_CONFIG);

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_FST, NULL, "FSE 3 TUPLE ENABLE HTT message:",
			(void *)cmd, len);

	ret = ath12k_htt_send(ab, ath12k_ab_to_dp(ab), skb,
			      HTT_H2T_MSG_TYPE_RX_FSE_3_TUPLE_HASH_CFG,
			      (u8 *)cmd);
	if (ret) {
		ath12k_err(ab, "DP FSE 3 TUPLE enable msg send failed ret:%d\n", ret);
		goto err_free;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "DP FSE 3 TUPLE enable msg sent from host\n");

	return 0;

err_free:
	dev_kfree_skb_any(skb);
	return ret;
}
