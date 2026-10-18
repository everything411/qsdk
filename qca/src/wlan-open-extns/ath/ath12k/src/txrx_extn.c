// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/skbuff.h>
#include <net/mac80211.h>
#include <linux/ieee80211.h>
#include <linux/bitfield.h>
#include "../core.h"
#include "../mac.h"
#include "../wmi.h"
#include "../debug.h"
#include "../debugfs.h"
#include "ath12k_cmn_extn.h"
#include "vendor_extn.h"

/**
 * ath12k_reset_raw_pkt_ctx - Reset raw packet context to clean state
 * @raw_pkt_ctx: Pointer to raw packet context structure
 *
 * This function resets all fields of the raw packet context to their
 * initial/clean state. It should be called after operation completion,
 * timeout, or cancellation to ensure clean state for next operation.
 *
 * Note: This function does NOT drain the packet queue or cancel timers.
 * Those operations should be done separately before calling this function.
 * IMPORTANT: The caller must hold the chan_lock before calling this function.
 */
static void ath12k_reset_raw_pkt_ctx(struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	if (!raw_pkt_ctx)
		return;

	raw_pkt_ctx->vif = NULL;
	raw_pkt_ctx->transaction_id = 0;
	raw_pkt_ctx->func_type = QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_INVALID;
	raw_pkt_ctx->num_frames = 0;
	raw_pkt_ctx->home_chan = false;
	raw_pkt_ctx->req_freq = 0;
	raw_pkt_ctx->operation_status = 0;
	raw_pkt_ctx->link_id = INVALID_LINK_ID;

	/* Reset timing timestamps */
	raw_pkt_ctx->ts_scan_start = 0;
	raw_pkt_ctx->ts_foreign_entry = 0;
	raw_pkt_ctx->ts_foreign_exit = 0;
	raw_pkt_ctx->ts_scan_done = 0;

	atomic_set(&raw_pkt_ctx->num_cmpl_pending, 0);
	memset(raw_pkt_ctx->custom_tx_status, 0, sizeof(raw_pkt_ctx->custom_tx_status));
	memset(&raw_pkt_ctx->chan_stat, 0, sizeof(raw_pkt_ctx->chan_stat));
	/* Clear tracking array */
	memset(raw_pkt_ctx->tracked_skbs, 0, sizeof(raw_pkt_ctx->tracked_skbs));
	raw_pkt_ctx->num_tracked_skbs = 0;
	memset(raw_pkt_ctx->frame_id, 0, sizeof(raw_pkt_ctx->frame_id));
}

/**
 * ath12k_handle_tx_completion - Handle TX completion for all packets
 * @ar: Pointer to ath12k structure
 * @raw_pkt_ctx: Pointer to raw packet context
 * @wdev: Wireless device for event sending
 * @transaction_id: Transaction ID for event correlation
 *
 * This function handles the completion logic when all TX packets are completed.
 * It cancels the timer, sends completion event, resets context, and drains queue.
 *
 * Note: This function handles all the completion logic that was previously inline.
 */
static void ath12k_handle_tx_completion(struct ath12k *ar,
					struct ath12k_raw_pkt_ctx *raw_pkt_ctx,
					struct wireless_dev *wdev,
					u32 transaction_id)
{
	bool timer_cancelled;
	int ret;
	if (!ar || !raw_pkt_ctx || !wdev)
		return;

	/* Acquire lock for cleanup operations */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	/* Only send completion event if operation is still active (vif is set)
	 * If vif is NULL, timeout handler already sent the event
	 */
	if (raw_pkt_ctx->vif) {
		/* Set operation status to success */
		raw_pkt_ctx->operation_status = 0;
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		/* Cancel timer before sending event to prevent race conditions */
		timer_cancelled = del_timer_sync(&raw_pkt_ctx->offchan_timer);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "All packets completed, timer cancelled: %s, sending event\n",
			   timer_cancelled ? "SUCCESS" : "ALREADY FIRED");
		/* Send event to userspace */
		ret = schedule_home_offchan_stats_event_ext(wdev, transaction_id, raw_pkt_ctx);
		if (ret) {
			ath12k_err(ar->ab, "Failed to schedule completion event: %d\n", ret);
		}
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Operation already completed by timeout handler, skipping completion event\n");
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
	}
	/* Drain the packet queue - this will unlink and free all remaining SKBs */
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "SUCCESS: All packets completed, calling drain to cleanup tracking list\n");
	ath12k_home_offchan_tx_drain(raw_pkt_ctx);
}

void ath12k_custom_tx_free_extn(struct sk_buff *skb, u32 status)
{
	struct ath12k_skb_cb *skb_cb;
	struct ath12k *ar;
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx;
	struct wireless_dev *wdev = NULL;
	struct ieee80211_vif *vif;
	u8 idx = 0;
	u32 transaction_id;
	u32 normalized_status;
	bool found_skb = false;

	if (!skb)
		return;

	skb_cb = ATH12K_SKB_CB(skb);
	if (!skb_cb) {
		dev_kfree_skb_any(skb);
		return;
	}

	ar = skb_cb->u.ar;
	if (!ar) {
		dev_kfree_skb_any(skb);
		return;
	}

	raw_pkt_ctx = &ar->ar_extn.raw_pkt_ctx;
	if (!raw_pkt_ctx) {
		dev_kfree_skb_any(skb);
		return;
	}

	/* Check if this is a custom packet before processing */
	if (!ATH12K_IS_CUSTOM_PKT(skb_cb)) {
		dev_kfree_skb_any(skb);
		return;
	}

	spin_lock_bh(&raw_pkt_ctx->chan_lock);

	/* Normalize status values: treat both 0 and 3 as success (0) */
	if (status == 0 || status == 3)
		normalized_status = 0;
	else
		normalized_status = status;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "custom tx completion: status=%u, searching %u tracked skbs\n",
		   status, raw_pkt_ctx->num_tracked_skbs);

	for (idx = 0; idx < raw_pkt_ctx->num_tracked_skbs && idx < ATH12K_MAX_CUSTOM_TX_PKT; idx++) {
		if (raw_pkt_ctx->tracked_skbs[idx] == skb) {
			/* Update status for this packet */
			raw_pkt_ctx->custom_tx_status[idx] = normalized_status;
			/* Clear the tracking entry */
			raw_pkt_ctx->tracked_skbs[idx] = NULL;
			found_skb = true;
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "found skb at index %u: raw_status=%u normalized_status=%u\n",
				   idx, status, normalized_status);
			break;
		}
	}

	if (!found_skb) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "skb not found in tracking array, freeing normally\n");
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		dev_kfree_skb_any(skb);
		return;
	}

	/* Extract transaction ID from raw_pkt_ctx */
	transaction_id = raw_pkt_ctx->transaction_id;
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	vif = skb_cb->vif;
	if (!vif) {
		ath12k_warn(ar->ab, "failed to find vif to update txcompl mgmt stats\n");
		dev_kfree_skb_any(skb);
		return;
	}

	/* Get the wireless_dev for sending the event */
	if (ar->ah && ar->ah->hw && ar->ah->hw->wiphy)
		wdev = ieee80211_vif_to_wdev(vif);

	/* Always decrement completion counter regardless of wdev availability */
	if (atomic_dec_and_test(&raw_pkt_ctx->num_cmpl_pending)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "all packets completed (txn_id=0x%x), calling completion handler\n",
			   transaction_id);
		if (wdev)
			ath12k_handle_tx_completion(ar, raw_pkt_ctx, wdev, transaction_id);
		else
			ath12k_home_offchan_tx_drain(raw_pkt_ctx);
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "packets still pending (txn_id=0x%x)\n", transaction_id);
	}

	dev_kfree_skb_any(skb);
}

/**
 * ath12k_fill_tx_param() - Save Tx param config in ar
 * @skb: Pointer to the skb
 * @nss: NSS
 * @preamble: Preamble
 * @mcs: MCS
 * @retry: Number of Retry
 * @power: Tx power
 * tx_beamforming: Tx Beamforming
 * band: Band
 *
 * This function saves user configured Tx Parameters
 * raw_pkt_ctx which is later used to send these
 * configs to target
 *
 * Return: 0 On success -E* on error
 */
#define OFFCHAN_MAX_MCS 7
#define OFFCHAN_MAX_CCK_MCS 3
#define ATH12K_GPR_MIN_PERIOD_MS 10

/**
 * ath12k_validate_mgmt_tx_params - Validate TX parameters for management frames
 * @ar: ath12k radio context (for error logging)
 * @vif: virtual interface for basic_rates check; pass NULL to skip the check
 * @preamble: preamble type (PREAMBLE_OFDM=0 or PREAMBLE_CCK=1)
 * @mcs: MCS index (0-7 for OFDM, 0-3 for CCK)
 * @band: band type; BAND_5GHZ rejects CCK
 *
 * Validates preamble range, MCS range, band/CCK compatibility, and basic rate
 * membership.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_validate_mgmt_tx_params(struct ath12k *ar,
					  struct ieee80211_vif *vif,
					  u8 preamble, u8 mcs, u8 band)
{
	/* CCK bit positions: 3, 2, 1, 0 for rates 11, 5.5, 2, 1 Mbps */
	const u8 cck_bit_pos[] = {3, 2, 1, 0};
	/* OFDM bit positions: 10, 8, 6, 4, 11, 9, 7, 5 for
	 * rates 48, 24, 12, 6, 54, 36, 18, 9 Mbps
	 */
	const u8 ofdm_bit_pos[] = {10, 8, 6, 4, 11, 9, 7, 5};
	u8 rate_idx;

	if (preamble >= PREAMBLE_INVALID) {
		ath12k_err(ar->ab, "preamble %u not supported\n", preamble);
		return -EINVAL;
	}

	if (mcs > OFFCHAN_MAX_MCS) {
		ath12k_err(ar->ab, "mcs %u not supported\n", mcs);
		return -EINVAL;
	}

	/* CCK is not valid on 5 GHz or 6 GHz */
	if ((band == BAND_5GHZ || band == BAND_6GHZ) && preamble == PREAMBLE_CCK) {
		ath12k_err(ar->ab, "CCK rate not valid for 5G/6G\n");
		return -EINVAL;
	}

	/* Skip basic rate check if vif is NULL (data frames) or not configured */
	if (!vif || !vif->bss_conf.basic_rates)
		return 0;

	if (preamble == PREAMBLE_CCK) {
		if (mcs > OFFCHAN_MAX_CCK_MCS) {
			ath12k_err(ar->ab, "Invalid CCK mcs %u for mgmt\n", mcs);
			return -EINVAL;
		}
		rate_idx = cck_bit_pos[mcs];
	} else {
		rate_idx = ofdm_bit_pos[mcs];
	}

	if (!(vif->bss_conf.basic_rates & BIT(rate_idx))) {
		ath12k_err(ar->ab, "Basic rate expected for mgmt packets\n");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_tx_param(struct sk_buff *skb,
				u8 nss, u8 preamble, u8 mcs,
				u8 retry, u8 power, u8 tx_beamforming,
				u8 band)
{
	struct ath12k_skb_cb *skb_cb;
	struct ath12k *ar;
	struct ath12k_skb_tx_param *tx_param;
	struct ieee80211_hdr *hdr;
	__le16 fc;

	skb_cb = ATH12K_SKB_CB(skb);
	ar = skb_cb->u.ar;
	tx_param = &ar->ar_extn.raw_pkt_ctx.tx_param;

	hdr = (struct ieee80211_hdr *)skb->data;
	fc = hdr->frame_control;

	memset(tx_param, 0, sizeof(*tx_param));
	if (ieee80211_is_data(fc)) {
		tx_param->is_data = true;
		/* Data frames: validate basic params only, skip basic_rates check */
		if (ath12k_validate_mgmt_tx_params(ar, NULL, preamble, mcs, band))
			return -EINVAL;
		goto fill_tx_param;
	}

	if (ath12k_validate_mgmt_tx_params(ar, skb_cb->vif, preamble, mcs, band))
		return -EINVAL;

fill_tx_param:
	tx_param->nss = nss;
	tx_param->preamble = preamble;
	tx_param->mcs = mcs;
	tx_param->retry = retry;
	tx_param->power = power;
	tx_param->tx_beamforming = tx_beamforming;
	tx_param->tx_param_configured = true;

	return 0;
}

/**
 * ath12k_process_rate_params() - Process rate parameters for WMI command
 * @mcs: Pointer to the MCS value (will be updated)
 * @nss: Pointer to the NSS value (will be updated)
 * @preamble: The preamble type
 *
 * This function processes the rate parameters for CCK and OFDM rates,
 * updating the MCS and NSS values as needed.
 */
static void ath12k_process_rate_params(u32 *mcs, u32 *nss,
				       enum preamble_type preamble)
{
	/* CCK bit positions: 3, 2, 1, 0 for rates 1, 2, 5.5, 11 Mbps */
	const u8 cck_bit_pos[] = {3, 2, 1, 0};
	/* OFDM bit positions: 10, 8, 6, 4, 11, 9, 7, 5 for
	 * rates 6, 9, 12, 18, 24, 36, 48, 54 Mbps
	 */
	const u8 ofdm_bit_pos[] = {10, 8, 6, 4, 11, 9, 7, 5};

	if (preamble == PREAMBLE_CCK) {
		if (*mcs < ARRAY_SIZE(cck_bit_pos))
			*mcs = (0x1 << cck_bit_pos[*mcs]);
		else
			*mcs = 0;
		/* For CCK, NSS is always 1 */
		*nss = 1;
	} else if (preamble == PREAMBLE_OFDM) {
		if (*mcs < ARRAY_SIZE(ofdm_bit_pos))
			*mcs = (0x1 << ofdm_bit_pos[*mcs]);
		else
			*mcs = 0;
		/* For OFDM, NSS is always 1 */
		*nss = 1;
	}
}

/**
 * ath12k_validate_offchan_operation - Validate device state for off-channel operations
 * @ar: Pointer to ath12k structure
 * @arvif: Pointer to ath12k_link_vif structure
 *
 * This function performs common sanity checks to ensure the device is in a valid
 * state for off-channel operations (both TX and RX functions).
 *
 * Return: 0 if validation passes, negative error code otherwise
 */
static int ath12k_validate_offchan_operation(struct ath12k *ar,
					     struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif;
	if (!arvif->ahvif) {
		ath12k_err(NULL, "Invalid ahvif\n");
		return -EINVAL;
	}
	ahvif = arvif->ahvif;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ath12k_err(ar->ab, "Not supported on monitor interface\n");
		return -EOPNOTSUPP;
	}
	if (unlikely(test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))) {
		ath12k_err(ar->ab, "Not allowed during recovery\n");
		return -EPERM;
	}
	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ath12k_err(ar->ab, "Not allowed during crash flush\n");
		return -EPERM;
	}
	if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
		ath12k_err(ar->ab, "Not allowed during CAC\n");
		return -EAGAIN;
	}
	if (ar->csa_active_cnt) {
		ath12k_err(ar->ab, "Not allowed during CSA\n");
		return -EAGAIN;
	}
	if (ath12k_mac_is_bridge_vdev(arvif)) {
		ath12k_err(ar->ab, "Not allowed on bridge vdev\n");
		return -EPERM;
	}
	return 0;
}

void ath12k_wmi_prepare_tx_params_extn(struct ath12k_skb_cb *skb_cb, void *ptr)
{
	struct ath12k *ar = skb_cb->u.ar;
	struct ath12k_skb_tx_param *tx_param = &ar->ar_extn.raw_pkt_ctx.tx_param;
	struct wmi_mgmt_send_params *params = (struct wmi_mgmt_send_params *)ptr;
	u32 *tx_param_dword0 = &params->tx_param_dword0;
	u32 *tx_param_dword1 = &params->tx_param_dword1;
	u32 mcs = tx_param->mcs;
	u32 nss = tx_param->nss;
	u32 preamble = tx_param->preamble;

	if (!ATH12K_CUSTOM_TX_PARAM_CONFIGURED_EXTN(ar) ||
	    !ATH12K_IS_CUSTOM_PKT(skb_cb))
		return;

	ath12k_process_rate_params(&mcs, &nss, preamble);

	/* Set power in dword0 [7:0] */
	*tx_param_dword0 |= le32_encode_bits(tx_param->power,
					     WMI_TX_PARAMS_DWORD0_POWER) |
			    /* Set MCS in dword0 [19:8] */
			    le32_encode_bits(mcs,
					     WMI_TX_PARAMS_DWORD0_MCS_MASK) |
			    /* Set NSS in dword0 [27:20] */
			    le32_encode_bits((nss ? (1 << (nss - 1)) : 0),
					     WMI_TX_PARAMS_DWORD0_NSS_MASK) |
			    /* Set retry limit in dword0 [31:28] */
			    le32_encode_bits((tx_param->retry ? (tx_param->retry & 0xF) : 1),
					     WMI_TX_PARAMS_DWORD0_RETRY_LIMIT);

	/* Set preamble type in dword1 [19:15] */
	*tx_param_dword1 |= le32_encode_bits((1 << preamble),
					     WMI_TX_PARAMS_DWORD1_PREAMBLE_TYPE) |
			    /* Set frame type in dword1 [20] */
			    le32_encode_bits(tx_param->is_data,
					     WMI_TX_PARAMS_DWORD1_FRAME_TYPE) |
			    /* Set beamforming in dword1 [22] */
			    le32_encode_bits(tx_param->tx_beamforming,
					     WMI_TX_PARAMS_DWORD1_BEAMFORM) |
			    /* Set retry limit ext in dword1 [25:23] */
			    le32_encode_bits((tx_param->retry ? ((tx_param->retry & 0x70) >> 4) : 0),
					     WMI_TX_PARAMS_DWORD1_RETRY_LIMIT_EXT);
}

int ath12k_custom_tx_extn(struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	struct sk_buff *skb, *tmp_skb;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_tx_info *info;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k *ar = NULL;
	struct ieee80211_hdr *hdr;
	struct sk_buff_head *q;
	struct ieee80211_hw *hw;
	u8 stored_link_id;
	bool is_prb_rsp;
	int processed_count = 0;

	if (!raw_pkt_ctx) {
		ath12k_err(NULL, "custom tx: NULL raw_pkt_ctx\n");
		return -EINVAL;
	}

	/* Get vif from raw_pkt_ctx - it should be the same for all packets */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	vif = raw_pkt_ctx->vif;
	if (!vif) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		ath12k_err(NULL, "custom tx: NULL vif in raw_pkt_ctx\n");
		return -EINVAL;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		ath12k_err(NULL, "custom tx: NULL ahvif\n");
		return -EINVAL;
	}

	/* Use stored link_id from raw_pkt_ctx for MLO selection */
	stored_link_id = raw_pkt_ctx->link_id;
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	/* Use proper MLO link selection pattern with RCU lock */
	rcu_read_lock();
	if (stored_link_id == INVALID_LINK_ID) {
		arvif = &ahvif->deflink;
	} else {
		if (stored_link_id < ATH12K_NUM_MAX_LINKS)
			arvif = rcu_dereference(ahvif->link[stored_link_id]);
		else
			arvif = NULL;
	}
	if (!arvif || !arvif->ar) {
		rcu_read_unlock();
		ath12k_err(NULL, "ath12k_custom_tx_extn: Invalid arvif or ar for link_id %u\n", stored_link_id);
		return -EINVAL;
	}
	ar = arvif->ar;
	hw = ar->ah->hw;
	rcu_read_unlock();

	lockdep_assert_wiphy(hw->wiphy);

	/* Re-acquire lock for queue operations */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);

	/* Initialize tracking array */
	raw_pkt_ctx->num_tracked_skbs = 0;

	skb_queue_walk_safe(&raw_pkt_ctx->pkt_list, skb, tmp_skb) {
		/* Critical errors - abort entire operation */
		if (!skb) {
			ath12k_err(NULL, "ath12k_custom_tx_extn: NULL skb in queue - aborting operation\n");
			processed_count = -EINVAL;
			goto cleanup_and_fail;
		}

		skb_cb = ATH12K_SKB_CB(skb);
		if (!skb_cb) {
			ath12k_err(NULL, "ath12k_custom_tx_extn: NULL skb_cb - aborting operation\n");
			processed_count = -EINVAL;
			goto cleanup_and_fail;
		}

		info = IEEE80211_SKB_CB(skb);
		if (!info) {
			ath12k_err(NULL, "ath12k_custom_tx_extn: NULL info - aborting operation\n");
			processed_count = -EINVAL;
			goto cleanup_and_fail;
		}

		info->control.vif = vif;
		hdr = (struct ieee80211_hdr *)skb->data;
		if (!hdr) {
			ath12k_err(NULL, "ath12k_custom_tx_extn: NULL hdr - aborting operation\n");
			processed_count = -EINVAL;
			goto cleanup_and_fail;
		}

		if (skb_cb->flags & ATH12K_SKB_CUSTOM_OFFCHAN_MGMT_TX)
			info->flags |= IEEE80211_TX_CTL_TX_OFFCHAN;

		/* Non-critical errors - skip this packet but continue with others */
		is_prb_rsp = ieee80211_is_probe_resp(hdr->frame_control);
		if (is_prb_rsp &&
		    atomic_read(&ar->num_pending_mgmt_tx) > ATH12K_PRB_RSP_DROP_THRESHOLD) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "Probe response dropped due to threshold\n");
			__skb_unlink(skb, &raw_pkt_ctx->pkt_list);
			dev_kfree_skb_any(skb);
			continue;
		}

		q = &ar->wmi_mgmt_tx_queue;
		if (skb_queue_len_lockless(q) >= ATH12K_TX_MGMT_NUM_PENDING_MAX) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI queue full, skipping packet\n");
			__skb_unlink(skb, &raw_pkt_ctx->pkt_list);
			dev_kfree_skb_any(skb);
			continue;
		}

		/* Store SKB pointer in tracking array BEFORE removing from queue */
		if (raw_pkt_ctx->num_tracked_skbs < ATH12K_MAX_CUSTOM_TX_PKT) {
			raw_pkt_ctx->tracked_skbs[raw_pkt_ctx->num_tracked_skbs] = skb;
			raw_pkt_ctx->num_tracked_skbs++;
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "added skb to tracking array at index %u\n",
				   raw_pkt_ctx->num_tracked_skbs - 1);
		} else {
			ath12k_err(ar->ab, "tracking array full, skipping packet\n");
			__skb_unlink(skb, &raw_pkt_ctx->pkt_list);
			dev_kfree_skb_any(skb);
			continue;
		}

		__skb_unlink(skb, &raw_pkt_ctx->pkt_list);

		/* Queue the SKB to WMI management TX queue */
		skb_queue_tail(q, skb);

		if (!(info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
			atomic_inc(&ar->num_pending_mgmt_tx);

		processed_count++;
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "queued packet %d to wmi queue\n", processed_count);
	}
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	/* Trigger WMI work to process all queued packets */
	if (processed_count > 0 && ar && hw) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "triggering wmi work for %d packets\n", processed_count);
		ath12k_mgmt_over_wmi_tx_work(hw->wiphy, &ar->wmi_mgmt_tx_work);
	} else {
		ath12k_err(ar->ab, "cannot trigger wmi work: processed=%d ar=%p hw=%p\n",
			   processed_count, ar, hw);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "processed %d packets\n", processed_count);

	if (processed_count > 0)
		return 0;

	return -ENODATA;

cleanup_and_fail:
	ath12k_err(ar->ab, "Critical error in packet processing - resetting context\n");
	/* Reset context to clean state - this will clear vif, transaction_id, etc. */
	ath12k_reset_raw_pkt_ctx(raw_pkt_ctx);
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	/* Drain any remaining packets to prevent memory leaks */
	ath12k_home_offchan_tx_drain(raw_pkt_ctx);
	return processed_count;
}

static void
ath12k_chan_info_event_extn(struct ath12k_extn *ar_extn,
			    const struct wmi_chan_info_event *ch_info_ev,
			    int channel_idx)
{
	struct rtplinst_extn *inst;

	if (channel_idx < 0 || channel_idx >= ATH12K_NUM_CHANS)
		return;

	inst = &ar_extn->rtplinst[channel_idx];

	/* Populate RTPL fields from channel info event */
        inst->primary_freq = le32_to_cpu(ch_info_ev->freq);
	inst->txpower_throughput = (int)le32_to_cpu(ch_info_ev->chan_tx_pwr_tp);
	inst->txpower_range = (int)le32_to_cpu(ch_info_ev->chan_tx_pwr_range);
}

struct ath12k_chan_info_blanking_parse {
	const struct wmi_scan_blanking_params_info *blanking;
};

static int ath12k_wmi_tlv_chan_info_blanking_parse(struct ath12k_base *ab,
						   u16 tag, u16 len,
						   const void *ptr, void *data)
{
	struct ath12k_chan_info_blanking_parse *parse = data;

	if (tag == WMI_TAG_SCAN_BLANKING_PARAMS_INFO)
		parse->blanking = ptr;

	return 0;
}

static int ath12k_wmi_tlv_chan_info_parse(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	int ret;

	if (tag == WMI_TAG_ARRAY_STRUCT && len > 0) {
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tlv_chan_info_blanking_parse,
					  data);
		if (ret)
			return ret;
	}
	return 0;
}

void ath12k_update_offchan_stats_extn(struct ath12k *ar,
				      struct sk_buff *skb,
				      u32 freq,
				      struct wmi_chan_info_event *ch_info,
				      int channel_idx)
{
	struct ath12k_raw_pkt_ctx *raw_ctx;
	struct ath12k_offchan_stat *stats;
	struct ath12k_blanking_params *blank_stats;
	struct ath12k_chan_info_blanking_parse parse = {};
	struct ath12k_base *ab;

	if (!ar) {
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "failed to update offchan stats, ar is NULL");
		return;
	}

	if (!ch_info) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "failed to update offchan stats, ch_info is NULL");
		return;
	}

	if (ch_info->cmd_flags == WMI_CHAN_INFO_START_RESP)
		ath12k_chan_info_event_extn(&ar->ar_extn, ch_info,
					    channel_idx);

	ab = ar->ab;
	raw_ctx = &ar->ar_extn.raw_pkt_ctx;

	spin_lock_bh(&raw_ctx->chan_lock);

	/* Check if offchan operation is active:
	 * 1. Timer is pending (indicates offchan operation in progress)
	 * 2. Frequency matches the current offchan frequency
	 */
	if (timer_pending(&raw_ctx->offchan_timer) &&
	    raw_ctx->req_freq == freq) {
		stats = &raw_ctx->chan_stat;

		stats->noise_floor = (s16)le32_to_cpu(ch_info->noise_floor);
		stats->tx_frame_count = le32_to_cpu(ch_info->tx_frame_cnt);
		stats->rx_clear_count = le32_to_cpu(ch_info->rx_clear_count);
		stats->rx_frame_count = le32_to_cpu(ch_info->rx_frame_count);
		stats->cycle_count = le32_to_cpu(ch_info->cycle_count);

		ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				    ath12k_wmi_tlv_chan_info_parse, &parse);

		blank_stats = &raw_ctx->chan_stat.blank_params;
		if (parse.blanking) {
			blank_stats->valid = 1;
			blank_stats->blanking_duration =
				le32_to_cpu(parse.blanking->blanking_duration);
			blank_stats->blanking_count =
				le32_to_cpu(parse.blanking->blanking_count);
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "scan blanking params: valid=%u count=%u duration=%u\n",
				   blank_stats->valid, blank_stats->blanking_count,
				   blank_stats->blanking_duration);
		} else {
			blank_stats->valid = 0;
			blank_stats->blanking_count = 0;
			blank_stats->blanking_duration = 0;
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "scan blanking params not present in event\n");
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "offchan stats updated: freq=%u nf=%d tx=%u rx=%u "
			   "rx_clear=%u cycle=%u\n",
			   freq, stats->noise_floor, stats->tx_frame_count,
			   stats->rx_frame_count, stats->rx_clear_count,
			   stats->cycle_count);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Disgarding offchan stats update [%u, %u, %u, %u]",
			    timer_pending(&raw_ctx->offchan_timer),
			    raw_ctx->home_chan, raw_ctx->req_freq,
			    freq);
	}
	spin_unlock_bh(&raw_ctx->chan_lock);

	if (le32_to_cpu(ch_info->err_code) == 0)
		ath12k_send_scan_radio_chan_stats_event_extn(ar, freq, parse.blanking);
}

void ath12k_send_scan_radio_chan_stats_event_extn(struct ath12k *ar,
						  u32 freq,
						  const struct wmi_scan_blanking_params_info *blanking)
{
	struct ath12k_blanking_params blank = {};
	struct ath12k_link_vif *arvif;
	struct cfg80211_chan_def *chandef = NULL;
	enum wmi_phy_mode phymode = MODE_UNKNOWN;
	const char *phymode_str;
	struct sk_buff *vendor_skb;

	if (!ath12k_scan_radio_blanking_supported(ar->pdev))
		return;

	if (blanking) {
		blank.valid             = 1;
		blank.blanking_count    = le32_to_cpu(blanking->blanking_count);
		blank.blanking_duration = le32_to_cpu(blanking->blanking_duration);
	}

	/* scan radio has exactly one VAP; get its home channel phy mode */
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->chanctx.def.chan) {
			chandef = &arvif->chanctx.def;
			break;
		}
	}
	if (chandef)
		phymode = ath12k_mac_get_phymode(ar, chandef->chan->band,
						 chandef->width);
	phymode_str = ath12k_phymode_str(phymode);

	vendor_skb = cfg80211_vendor_event_alloc(ath12k_ar_to_hw(ar)->wiphy, NULL,
						 128,
						 QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS_INDEX,
						 GFP_ATOMIC);
	if (!vendor_skb) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "scan_radio_chan_stats: alloc failed\n");
		return;
	}

	if (nla_put_u32(vendor_skb, QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_FREQ,
			freq))
		goto err_free_skb;
	if (nla_put_u8(vendor_skb, QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_VALID,
		       (u8)blank.valid))
		goto err_free_skb;
	if (nla_put_u32(vendor_skb, QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_COUNT,
			blank.blanking_count))
		goto err_free_skb;
	if (nla_put_u32(vendor_skb, QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_BLANKING_DURATION,
			blank.blanking_duration))
		goto err_free_skb;
	if (nla_put_string(vendor_skb, QCA_WLAN_VENDOR_ATTR_SCAN_RADIO_CHAN_STATS_PHYMODE,
			   phymode_str))
		goto err_free_skb;

	cfg80211_vendor_event(vendor_skb, GFP_ATOMIC);
	return;

err_free_skb:
	ath12k_err(ar->ab, "scan_radio_chan_stats: nla_put failed\n");
	kfree_skb(vendor_skb);
}

/**
 * ath12k_handle_rx_scan_event - Handle RX function scan completion or failure
 * @ar: Pointer to ath12k structure
 * @raw_ctx: Pointer to raw packet context
 * @is_success: true for completion, false for failure
 * @event_type: The scan event type (used for failure logging)
 *
 * This function is called when scan completes or fails for RX function type.
 * It sends the appropriate event to userspace and cleans up the context.
 *
 * Note: Called with raw_ctx->chan_lock held, releases and re-acquires it.
 */
static void ath12k_handle_rx_scan_event(struct ath12k *ar,
					struct ath12k_raw_pkt_ctx *raw_ctx,
					bool is_success,
					u32 event_type)
{
	struct wireless_dev *wdev;
	u32 transaction_id;
	struct ieee80211_vif *vif_snapshot;

	/* Check if this is RX function and operation is active */
	if (raw_ctx->func_type != QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX || !raw_ctx->vif)
		return;

	/* Capture vif and transaction_id under lock protection */
	vif_snapshot = raw_ctx->vif;
	transaction_id = raw_ctx->transaction_id;

	/* Release lock before timer cancellation to avoid deadlock */
	spin_unlock_bh(&raw_ctx->chan_lock);

	/* Cancel timer first to prevent timeout handler interference */
	del_timer_sync(&raw_ctx->offchan_timer);

	/* Get wdev from captured vif (safe since we have a snapshot) */
	wdev = ieee80211_vif_to_wdev(vif_snapshot);
	if (!wdev) {
		ath12k_err(ar->ab, "Failed to get wdev for RX %s\n",
			   is_success ? "completion" : "failure");
		spin_lock_bh(&raw_ctx->chan_lock);
		return;
	}

	if (is_success) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "RX function scan completed, sending event for transaction 0x%x\n",
			   transaction_id);
	} else {
		ath12k_warn(ar->ab, "RX function scan failed (event=%u), sending failure event for transaction 0x%x\n",
			    event_type, transaction_id);
	}

	/* Re-acquire lock for final operations */
	spin_lock_bh(&raw_ctx->chan_lock);

	/* Double-check vif is still valid after timer cancellation */
	if (!raw_ctx->vif) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Operation already completed by timeout handler\n");
		spin_unlock_bh(&raw_ctx->chan_lock);
		return;
	}

	if (is_success) {
		/* Set operation status to success */
		raw_ctx->operation_status = 0;
	} else {
		/* Set operation status to failure */
		raw_ctx->operation_status = 1;
	}

	/* Send event to userspace */
	schedule_home_offchan_stats_event_ext(wdev, transaction_id, raw_ctx);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "RX function %s cleanup completed\n",
		   is_success ? "completion" : "failure");
}

/**
 * ath12k_compute_offchan_timing_stats - Compute offchan timing stats
 *                                       from timestamps
 *
 * @raw_ctx: Pointer to raw packet context
 *
 * Computes timing statistics:
 * - dwell_time: Time spent on foreign channel (foreign_exit - foreign_entry)
 * - chanswitch_time_htof: Time to switch from home to foreign
 *                         (foreign_entry - scan_start)
 * - chanswitch_time_ftoh: Time to switch from foreign to home
 *                         (scan_done - foreign_exit)
 */
static void ath12k_compute_offchan_timing_stats(struct ath12k_raw_pkt_ctx *raw_ctx)
{
	struct ath12k_offchan_stat *stats = &raw_ctx->chan_stat;
	s64 dwell_us, htof_us, ftoh_us;

	if (!raw_ctx->ts_scan_start)
		return;

	/* Compute dwell time if we have both entry and exit timestamps */
	if (raw_ctx->ts_foreign_entry && raw_ctx->ts_foreign_exit) {
		dwell_us = ktime_us_delta(raw_ctx->ts_foreign_exit,
					  raw_ctx->ts_foreign_entry);
		if (dwell_us > 0)
			stats->dwell_time = (u32)dwell_us;
	}

	/* Compute home-to-foreign channel switch time */
	htof_us = ktime_us_delta(raw_ctx->ts_foreign_entry,
				 raw_ctx->ts_scan_start);
	if (htof_us > 0)
		stats->chanswitch_time_htof = (u32)htof_us;

	/* Compute foreign-to-home channel switch time */
	if (raw_ctx->ts_foreign_exit && raw_ctx->ts_scan_done) {
		ftoh_us = ktime_us_delta(raw_ctx->ts_scan_done,
					 raw_ctx->ts_foreign_exit);
		if (ftoh_us > 0)
			stats->chanswitch_time_ftoh = (u32)ftoh_us;
	}

	ath12k_dbg(NULL, ATH12K_DBG_WMI,
		   "offchan timing: dwell=%u us, htof=%u us, ftoh=%u us",
		   stats->dwell_time, stats->chanswitch_time_htof,
		   stats->chanswitch_time_ftoh);
}

void ath12k_update_offchan_ts_extn(struct ath12k *ar, u32 event_type)
{
	struct ath12k_raw_pkt_ctx *raw_ctx;
	__le32 scan_event =  le32_to_cpu(event_type);

	if (!ar)
		return;

	raw_ctx = &ar->ar_extn.raw_pkt_ctx;

	/* Only update timestamps if offchan operation is active */
	spin_lock_bh(&raw_ctx->chan_lock);
	if (!timer_pending(&raw_ctx->offchan_timer)) {
		spin_unlock_bh(&raw_ctx->chan_lock);
		return;
	}

	switch (scan_event) {
	case WMI_SCAN_EVENT_STARTED:
		raw_ctx->ts_scan_start = ktime_get();
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: captured scan_start timestamp\n");
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		raw_ctx->ts_foreign_entry = ktime_get();
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: captured foreign_entry timestamp\n");
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
		raw_ctx->ts_foreign_exit = ktime_get();
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: captured foreign_exit timestamp\n");
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		raw_ctx->ts_scan_done = ktime_get();
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: captured scan done timestamp\n");
		break;
	case WMI_SCAN_EVENT_START_FAILED:
	case WMI_SCAN_EVENT_DEQUEUED:
	case WMI_SCAN_EVENT_PREEMPTED:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: scan failure event %u\n",
			   scan_event);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "offchan: event %u not handler\n", scan_event);
		spin_unlock_bh(&raw_ctx->chan_lock);
		return;
	}
	ath12k_compute_offchan_timing_stats(raw_ctx);
	/* Handle RX function completion/failure after timing stats computation */
	if (scan_event == WMI_SCAN_EVENT_COMPLETED) {
		/* Handle RX function completion */
		ath12k_handle_rx_scan_event(ar, raw_ctx, true, scan_event);
	} else if (scan_event == WMI_SCAN_EVENT_START_FAILED ||
		   scan_event == WMI_SCAN_EVENT_DEQUEUED ||
		   scan_event == WMI_SCAN_EVENT_PREEMPTED) {
		/* Handle RX function failure */
		ath12k_handle_rx_scan_event(ar, raw_ctx, false, scan_event);
	}
	spin_unlock_bh(&raw_ctx->chan_lock);
}

void ath12k_wmi_offchan_txrx_update_scan_params_extn(struct ath12k *ar,
						     struct ath12k_wmi_scan_req_arg *arg)
{
	struct ath12k_raw_pkt_ctx *raw_ctx = &ar->ar_extn.raw_pkt_ctx;

	if (!timer_pending(&raw_ctx->offchan_timer))
		return;

	arg->scan_ev_foreign_chn_exit = 1;
	arg->scan_ev_preempted = 1;
	arg->scan_priority = WMI_SCAN_PRIORITY_HIGH;
	if (raw_ctx->func_type == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX)
		arg->scan_f_chan_stat_evnt = 1;
	else
		arg->scan_f_chan_stat_evnt = 0;
}

/**
 * ath12k_custom_txrx_timeout_handler - Timeout handler for home off-channel operations
 * @t: Timer structure
 *
 * Called when home/off-channel operation times out. Cleans up and sends error response.
 */
static void ath12k_custom_txrx_timeout_handler(struct timer_list *t)
{
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx = from_timer(raw_pkt_ctx, t, offchan_timer);
	struct ath12k_vif *ahvif;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif_snapshot;
	u32 transaction_id;
	u8 func_type;

	if (!raw_pkt_ctx)
		return;

	/* Capture vif pointer and other data under lock protection */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	vif_snapshot = raw_pkt_ctx->vif;
	if (!vif_snapshot) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		return;
	}

	/* Get ahvif while still holding the lock */
	ahvif = (struct ath12k_vif *)vif_snapshot->drv_priv;
	if (!ahvif) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		return;
	}

	ah = ahvif->ah;
	if (!ah) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		return;
	}

	ar = ah->radio;
	if (!ar) {
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
		return;
	}

	transaction_id = raw_pkt_ctx->transaction_id;
	func_type = raw_pkt_ctx->func_type;
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	wdev = ieee80211_vif_to_wdev(vif_snapshot);
	if (!wdev) {
		ath12k_err(ar->ab, "Failed to get wdev from vif\n");
		return;
	}

	ath12k_warn(ar->ab, "Home/off-channel operation timed out for transaction 0x%x (func_type=%u)\n",
		    transaction_id, func_type);

	/* Drain the queue first (this acquires and releases the lock internally) */
	ath12k_home_offchan_tx_drain(raw_pkt_ctx);

	/* Now acquire lock for all remaining operations to ensure atomicity */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);

	/* Set operation status to failure (timeout) */
	raw_pkt_ctx->operation_status = 1;

	/* Send timeout response to user space while still holding lock */
	schedule_home_offchan_stats_event_ext(wdev, transaction_id, raw_pkt_ctx);

	spin_unlock_bh(&raw_pkt_ctx->chan_lock);
}

void ath12k_custom_txrx_init(struct ath12k *ar)
{
	struct ath12k_raw_pkt_ctx *raw_ctx = &ar->ar_extn.raw_pkt_ctx;

	spin_lock_init(&raw_ctx->chan_lock);
	timer_setup(&raw_ctx->offchan_timer,
		    ath12k_custom_txrx_timeout_handler, 0);
	skb_queue_head_init(&raw_ctx->pkt_list);
	mutex_init(&raw_ctx->raw_ctx_mutex);
}

void ath12k_custom_txrx_deinit(struct ath12k *ar)
{
	struct ath12k_raw_pkt_ctx *raw_ctx = &ar->ar_extn.raw_pkt_ctx;

	del_timer_sync(&raw_ctx->offchan_timer);

	spin_lock_bh(&raw_ctx->chan_lock);
	skb_queue_purge(&raw_ctx->pkt_list);
	spin_unlock_bh(&raw_ctx->chan_lock);
	mutex_destroy(&raw_ctx->raw_ctx_mutex);
}

/**
 * ath12k_vendor_home_offchan_tx_rx_frame - Process and transmit home/off-channel frame
 * @params: Pointer to home/off-channel operation parameters
 *
 * This function parses the nested netlink attributes containing frame data and
 * transmission parameters (NSS, preamble, MCS, retry, power), allocates an SKB,
 * sets up the control buffers, and queues the frame for transmission. It determines
 * whether the transmission is on home channel or off-channel based on the current
 * operating frequency and sets appropriate flags. For off-channel transmissions,
 * it sets up a timer for timeout handling.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_vendor_home_offchan_tx_rx_frame(struct ath12k_home_offchan_params *params)
{
	struct wireless_dev *wdev = params->wdev;
	struct nlattr *offchan_frame = params->offchan_frame;
	int band = params->band;
	int freq = params->freq;
	u32 transaction_id = params->transaction_id;
	u16 scan_dur = params->scan_dur;
	u8 num_frames = params->num_frames;
	u8 func = params->func;
	u8 link_id = params->link_id;
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX + 1];
	int ret, nss = 0, preamble = 0, mcs = 0, retry = 0, power = 0, rem;
	uint8_t tx_bf = 0;
	char *buf;
	struct nlattr *frame;
	unsigned int attrsize = (QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX + 1) * sizeof(struct nlattr *);
	struct sk_buff *skb;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ath12k *ar;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_vif *vif = NULL;
	struct ieee80211_bss_conf *bss_conf;
	int buf_len;
	struct ath12k_hw *ah;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wdev->wiphy);
	struct ieee80211_chanctx_conf *ctx = NULL;
	int cur_freq = 0;
	struct ieee80211_channel *chan;
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx;
	bool is_home_channel = false;
	u32 beacon_interval = 100;
	u32 timeout_ms;
	u8 effective_link_id;
	u8 frame_idx;
	u8 cur_frame_id;

	/* Common setup for both TX and RX functions */
	ah = ath12k_hw_to_ah(hw);
	if (!ah) {
		ath12k_err(NULL, "Failed to get ah from wiphy\n");
		return -ENODEV;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "Failed to get vif from wdev\n");
		return -ENODEV;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "Failed to get ahvif from vif\n");
		return -ENODEV;
	}

	/* Validate link_id parameter early to catch invalid values */
	if (link_id != INVALID_LINK_ID && link_id >= ATH12K_NUM_MAX_LINKS) {
		ath12k_err(NULL, "Invalid link_id %u (max allowed: %u)\n",
			   link_id, ATH12K_NUM_MAX_LINKS - 1);
		return -EINVAL;
	}

	rcu_read_lock();

	if (link_id == INVALID_LINK_ID || !ieee80211_vif_is_mld(vif)) {
		effective_link_id = 0;
		beacon_interval = vif->bss_conf.beacon_int;
	} else {
		effective_link_id = link_id;
		bss_conf = rcu_dereference(vif->link_conf[effective_link_id]);
		if (bss_conf) {
			beacon_interval = bss_conf->beacon_int;
		}
	}

	if (beacon_interval == 0 || beacon_interval > 1000) {
		ath12k_err(NULL, "Invalid bcn int %u, using default 100\n",
			    beacon_interval);
		beacon_interval = 100;
	}

	if (effective_link_id == 0) {
		arvif = &ahvif->deflink;
	} else {
		if (effective_link_id < ATH12K_NUM_MAX_LINKS)
			arvif = rcu_dereference(ahvif->link[effective_link_id]);
		else
			arvif = NULL;
	}
	if (!arvif || !arvif->ar) {
		rcu_read_unlock();
		ath12k_err(NULL, "Failed to get arvif for effective_link_id %u\n", effective_link_id);
		return -EINVAL;
	}
	ar = arvif->ar;

	if (!arvif->chanctx.def.chan) {
		rcu_read_unlock();
		ath12k_err(NULL, "Failed to send link info due to no chan ctx\n");
		return -EINVAL;
	}

	ctx = &arvif->chanctx;
	chan = ctx->def.chan;
	cur_freq = chan->center_freq;
	is_home_channel = (freq == cur_freq);

	/* Validate ar and ar_extn before accessing raw_pkt_ctx */
	if (!ar || !ar->ab) {
		rcu_read_unlock();
		ath12k_err(NULL, "Invalid ar or ar->ab pointer\n");
		return -EINVAL;
	}

	/* Get raw_pkt_ctx from ar_extn structure */
	raw_pkt_ctx = &ar->ar_extn.raw_pkt_ctx;

	/* Scan radio does not support beacon.
	* Need this delay for host FW message processing,
	* as beacon interval accounts this in delay in serving radio.
	*/
	if (ath12k_scan_radio_supported(ar->pdev)) {
		timeout_ms = ATH12K_MAX_OFFCHAN_WAIT + scan_dur +
				ATH12K_SCAN_RADIO_OFFCHAN_RX_DELAY;
	} else {
		timeout_ms = ATH12K_MAX_OFFCHAN_WAIT + scan_dur +
				beacon_interval;
	}

	ret = ath12k_validate_offchan_operation(ar, arvif);
	if (ret) {
		rcu_read_unlock();
		ath12k_err(ar->ab, "Validation failed for off-channel operation: %d\n", ret);
		return ret;
	}

	rcu_read_unlock();

	/* Acquire mutex to serialize operations - hold until user event arrives.
	 * Cancel command will invoke the event hanlder in the callee which will
	 * will unlock the mutex post sending the event to user space
	 */
	ret = mutex_lock_interruptible(&raw_pkt_ctx->raw_ctx_mutex);
	if (ret) {
		ath12k_err(ar->ab, "[TXN:0x%x] Failed to acquire raw_ctx_mutex: %d\n", transaction_id, ret);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "home/offchan frame handler: func=%u freq=%d home_chan=%d transaction_id=0x%x\n",
		   func, freq, is_home_channel, transaction_id);

	/* Common context initialization for both TX and RX functions */
	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	raw_pkt_ctx->transaction_id = transaction_id;
	raw_pkt_ctx->func_type = func;
	raw_pkt_ctx->num_frames = (func == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX) ? 0 : num_frames;
	raw_pkt_ctx->home_chan = is_home_channel;
	raw_pkt_ctx->vif = vif;
	raw_pkt_ctx->req_freq = freq;
	raw_pkt_ctx->link_id = effective_link_id;  /* Save link_id for later use */
	memset(&raw_pkt_ctx->chan_stat, 0, sizeof(raw_pkt_ctx->chan_stat));
	/* Initialize all packet statuses to 1 (failed) by default.
	 * They will be set to 0 (success) only when we receive completion.
	 * This way, any packet that times out or doesn't complete will
	 * already be marked as failed without needing explicit handling.
	 */
	memset(raw_pkt_ctx->custom_tx_status, 1, sizeof(raw_pkt_ctx->custom_tx_status));
	/* Set atomic counter to expected number of completions (num_frames) */
	atomic_set(&raw_pkt_ctx->num_cmpl_pending, num_frames);
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	/* For RX function, set timer and return early */
	if (func == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "RX function - setting timer for %u ms (base=%u + scan=%u + beacon=%u) (home_chan=%d)\n",
			   timeout_ms, ATH12K_MAX_OFFCHAN_WAIT, scan_dur, beacon_interval, is_home_channel);
		mod_timer(&raw_pkt_ctx->offchan_timer,
			  jiffies + msecs_to_jiffies(timeout_ms));

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "RX function setup complete, waiting for ROC from application\n");
		/* mutex will be unlocked in the timeout handler or scan event completion*/
		return 0;
	}

	/* TX function - offchan_frame must be present */
	if (!offchan_frame) {
		ath12k_err(NULL, "offchan_frame is NULL for TX function\n");
		mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
		return -EINVAL;
	}

	frame_idx = 0;
	nla_for_each_nested(frame, offchan_frame, rem) {
		memset(tb, 0, attrsize);
		ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX,
							   frame,
							   ath12k_vendor_home_offchan_tx_rx_frame_policy, NULL);
		if (ret) {
			ath12k_err(NULL, "Invalid Offchan Frame policy\n");
			mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
			return ret;
		}
		/* Parse frame_id for this frame; commit only after successful SKB allocation */
		cur_frame_id = (u8)-1;
		if (frame_idx < ATH12K_MAX_CUSTOM_TX_PKT &&
		    tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_ID])
			cur_frame_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_ID]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS])
			nss = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE])
			preamble = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS])
			mcs = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_RETRY])
			retry = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_RETRY]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_POWER])
			power = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_POWER]);
		if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_TX_BEAMFORMING])
			tx_bf = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_TX_BEAMFORMING]);

		/* TX function - process frame data */
		if (!tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]) {
			ath12k_err(NULL, "Frame data missing for TX function\n");
			mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
			return -EINVAL;
		}

		buf = nla_data(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]);
		buf_len = nla_len(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Frame data received: len=%d, band=%d\n", buf_len, band);

		/* Allocate a new skb with enough headroom for the 802.11 header */
		skb = dev_alloc_skb(buf_len);
		if (!skb) {
			ath12k_err(ar->ab, "[TXN:0x%x] Failed to allocate SKB\n", transaction_id);
			mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
			return -ENOMEM;
		}

		/* Commit frame_id and advance index only after successful SKB allocation */
		if (frame_idx < ATH12K_MAX_CUSTOM_TX_PKT)
			raw_pkt_ctx->frame_id[frame_idx] = cur_frame_id;
		frame_idx++;

		skb_put_data(skb, buf, buf_len);

		info = IEEE80211_SKB_CB(skb);
		skb_cb = ATH12K_SKB_CB(skb);

		/* Clear the skb_cb structure */
		memset(skb_cb, 0, sizeof(*skb_cb));

		skb_cb->vif = vif;

		/* Set the ar in the skb_cb */
		skb_cb->u.ar = ar;

		skb_cb->link_id = effective_link_id;

		/* Transaction ID is already stored in raw_pkt_ctx->transaction_id above */
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Stored transaction ID 0x%x in raw_pkt_ctx\n", transaction_id);

		ret = ath12k_fill_tx_param(skb, nss, preamble, mcs, retry, power, tx_bf, band);
		if (ret) {
			ath12k_err(ar->ab, "Failed to fill tx param: %d\n", ret);
			dev_kfree_skb_any(skb);
			/* Cancel timer if it was set for off-channel operation */
			if (!is_home_channel)
				del_timer_sync(&raw_pkt_ctx->offchan_timer);
			mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
			return ret;
		}

		/* Set the control.vif field */
		info->control.vif = vif;

		/*
		* Set flags to indicate to driver whether this is home channel or off-channel TX
		* This is CRITICAL for the driver to handle the packet correctly
		*/
		if (is_home_channel) {
			skb_cb->flags |= ATH12K_SKB_CUSTOM_MGMT_TX;
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "Home channel TX - flag set to ATH12K_SKB_CUSTOM_MGMT_TX\n");
		} else {
			skb_cb->flags |= ATH12K_SKB_CUSTOM_OFFCHAN_MGMT_TX;
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "Off-channel TX - flag set to ATH12K_SKB_CUSTOM_OFFCHAN_MGMT_TX\n");
		}

		/* Initialize context under lock protection BEFORE queuing packet */
		spin_lock_bh(&raw_pkt_ctx->chan_lock);
		/* Now add packet to queue */
		skb_queue_tail(&raw_pkt_ctx->pkt_list, skb);
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);
	}

	if (is_home_channel) {
		/* Home channel - send immediately */
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Home channel TX - sending immediately\n");
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Off-channel TX - setting timer for %u ms (base=%u + scan=%u + beacon=%u) and sending %u packets to WMI queue\n",
			   timeout_ms, ATH12K_MAX_OFFCHAN_WAIT, scan_dur, beacon_interval, num_frames);
		mod_timer(&raw_pkt_ctx->offchan_timer,
			   jiffies + msecs_to_jiffies(timeout_ms));
	}

	/* Call ath12k_custom_tx_extn with raw_pkt_ctx to process all packets */
	ret = ath12k_custom_tx_extn(raw_pkt_ctx);
	if (ret) {
		ath12k_err(ar->ab, "[TXN:0x%x] Failed to process custom tx packets: %d\n", transaction_id, ret);
		/* Cancel timer if it was set for off-channel operation */
		if (!is_home_channel)
			del_timer_sync(&raw_pkt_ctx->offchan_timer);
		mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
		return ret;
	}

	return ret;
}

void ath12k_home_offchan_tx_drain(struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	struct sk_buff *skb;
	int drained_count = 0;

	if (!raw_pkt_ctx)
		return;

	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	while ((skb = skb_dequeue(&raw_pkt_ctx->pkt_list)) != NULL) {
		dev_kfree_skb_any(skb);
		drained_count++;
	}
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	if (drained_count > 0) {
		ath12k_dbg(NULL, ATH12K_DBG_WMI,
			   "ath12k_home_offchan_tx_drain: Drained %d SKBs from queue (not transmitted)\n",
			   drained_count);
	}
}


void ath12k_home_offchan_cleanup(struct ath12k *ar)
{
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx = &ar->ar_extn.raw_pkt_ctx;

	if (!raw_pkt_ctx->vif)
		return;

	del_timer_sync(&raw_pkt_ctx->offchan_timer);
	ath12k_home_offchan_tx_drain(raw_pkt_ctx);

	/* Clear the vif pointer to indicate cleanup */
	raw_pkt_ctx->vif = NULL;
}

/**
 * ath12k_convert_app_band_to_nl80211 - Convert application band to NL80211 band
 * @app_band: Band value from application (0=auto, 1=2GHz, 2=5GHz, 3=6GHz)
 *
 * Converts application band enumeration to NL80211 band enumeration
 * for use with ieee80211_channel_to_frequency()
 *
 * Return: NL80211_BAND_* value, or -EINVAL for invalid band
 */
static int ath12k_convert_app_band_to_nl80211(int app_band)
{
	switch (app_band) {
	case BAND_DEFAULT:  /* 0 - auto, use 2GHz as default */
		return NL80211_BAND_2GHZ;
	case BAND_2GHZ:     /* 1 - 2GHz */
		return NL80211_BAND_2GHZ;
	case BAND_5GHZ:     /* 2 - 5GHz */
		return NL80211_BAND_5GHZ;
	case BAND_6GHZ:     /* 3 - 6GHz */
		return NL80211_BAND_6GHZ;
	default:
		return -EINVAL;
	}
}

static void ath12k_gpr_timer_handler(struct timer_list *t);
static void ath12k_gpr_work_handler(struct work_struct *work);

/**
 * ath12k_gpr_arvif_init - Initialise GPR timer for a link VIF
 * @arvif: link VIF to initialise
 *
 * Called once per arvif at creation time.
 */
void ath12k_gpr_arvif_init(struct ath12k_link_vif *arvif)
{
	timer_setup(&arvif->arvif_extn.gpr_timer,
		    ath12k_gpr_timer_handler, 0);
	INIT_WORK(&arvif->arvif_extn.gpr_work, ath12k_gpr_work_handler);
	arvif->arvif_extn.gpr_timer_enabled = true;
}

/**
 * ath12k_gpr_arvif_deinit - Clean up GPR state for a link VIF
 * @arvif: link VIF to clean up
 *
 * Cancels the periodic timer and frees any stored GPR frame.
 * Called at arvif teardown.
 */
void ath12k_gpr_arvif_deinit(struct ath12k_link_vif *arvif)
{
	del_timer_sync(&arvif->arvif_extn.gpr_timer);
	cancel_work_sync(&arvif->arvif_extn.gpr_work);
	if (arvif->arvif_extn.gpr_frame) {
		dev_kfree_skb_any(arvif->arvif_extn.gpr_frame);
		arvif->arvif_extn.gpr_frame = NULL;
	}
}

/**
 * ath12k_gpr_tx - Transmit a stored GPR frame on the home channel
 * @arvif: link VIF on which to transmit
 *
 * Clones the frame stored in arvif->arvif_extn.gpr_frame, sets up the
 * skb control block for home-channel management TX, populates raw_pkt_ctx
 * minimally, calls ath12k_custom_tx_extn(), and releases the mutex
 * immediately - no completion event is awaited (fire-and-forget).
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_gpr_tx(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx = &ar->ar_extn.raw_pkt_ctx;
	struct ieee80211_vif *vif = arvif->ahvif ? arvif->ahvif->vif : NULL;
	struct sk_buff *skb;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	int ret;

	if (!vif) {
		ath12k_err(ar->ab, "GPR TX: no vif on arvif\n");
		return -EINVAL;
	}

	if (!arvif->arvif_extn.gpr_frame) {
		ath12k_err(ar->ab, "GPR TX: no frame stored\n");
		return -ENODATA;
	}

	/* Clone the stored frame so the original is preserved for future calls */
	skb = skb_copy(arvif->arvif_extn.gpr_frame, GFP_ATOMIC);
	if (!skb) {
		ath12k_err(ar->ab, "GPR TX: skb_copy failed\n");
		return -ENOMEM;
	}

	info = IEEE80211_SKB_CB(skb);
	skb_cb = ATH12K_SKB_CB(skb);
	memset(skb_cb, 0, sizeof(*skb_cb));

	skb_cb->vif = vif;
	skb_cb->u.ar = ar;
	skb_cb->link_id = arvif->link_id;
	/* GPR always transmits on the home channel */
	skb_cb->flags |= ATH12K_SKB_CUSTOM_MGMT_TX;
	info->control.vif = vif;

	ret = ath12k_fill_tx_param(skb,
				   arvif->arvif_extn.gpr_nss,
				   arvif->arvif_extn.gpr_preamble,
				   arvif->arvif_extn.gpr_mcs,
				   0 /* retry */, 0 /* power */, 0 /* tx_bf */,
				   BAND_DEFAULT);
	if (ret) {
		ath12k_err(ar->ab, "GPR TX: ath12k_fill_tx_param failed: %d\n", ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	/* Populate raw_pkt_ctx minimally for home-channel single-frame TX.
	 * Called from process context (workqueue) so mutex_trylock is safe;
	 * skip this TX cycle if another operation holds the mutex.
	 */
	if (!mutex_trylock(&raw_pkt_ctx->raw_ctx_mutex)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "GPR TX: mutex busy, skipping this cycle\n");
		dev_kfree_skb_any(skb);
		return -EBUSY;
	}

	spin_lock_bh(&raw_pkt_ctx->chan_lock);
	raw_pkt_ctx->vif = vif;
	raw_pkt_ctx->func_type = QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_GPR;
	raw_pkt_ctx->home_chan = true;
	raw_pkt_ctx->num_frames = 1;
	raw_pkt_ctx->link_id = arvif->link_id;
	atomic_set(&raw_pkt_ctx->num_cmpl_pending, 1);
	skb_queue_tail(&raw_pkt_ctx->pkt_list, skb);
	spin_unlock_bh(&raw_pkt_ctx->chan_lock);

	ret = ath12k_custom_tx_extn(raw_pkt_ctx);
	if (ret)
		ath12k_err(ar->ab, "GPR TX: ath12k_custom_tx_extn failed: %d\n", ret);
	else
		atomic_inc(&arvif->arvif_extn.gpr_send_count);

	/* Fire-and-forget: release mutex immediately, no completion event needed */
	mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);

	return ret;
}


/**
 * ath12k_gpr_timer_handler - Periodic timer callback for GPR transmission
 * @t: timer_list embedded in ath12k_link_vif_extn
 *
 * Called every gpr_period ms in softirq context.  Re-arms the timer for
 * the next period BEFORE scheduling the TX work, so the period is measured
 * from timer-fire to timer-fire rather than from TX-completion to
 * TX-completion.
 *
 */
static void ath12k_gpr_timer_handler(struct timer_list *t)
{
	struct ath12k_link_vif_extn *extn =
		from_timer(extn, t, gpr_timer);
	struct ath12k_link_vif *arvif =
		container_of(extn, struct ath12k_link_vif, arvif_extn);


	if (!extn->gpr_timer_enabled)
		return;

	if (extn->gpr_period)
		mod_timer(&arvif->arvif_extn.gpr_timer,
			  jiffies + msecs_to_jiffies(extn->gpr_period));

	atomic_inc(&extn->gpr_timer_start_count);

	/* Defer actual TX to process context */
	schedule_work(&arvif->arvif_extn.gpr_work);
}

static void ath12k_gpr_work_handler(struct work_struct *work)
{
	struct ath12k_link_vif_extn *extn =
		container_of(work, struct ath12k_link_vif_extn, gpr_work);
	struct ath12k_link_vif *arvif =
		container_of(extn, struct ath12k_link_vif, arvif_extn);

	ath12k_gpr_tx(arvif);
}

int ath12k_vendor_home_offchan_tx_rx_handler(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX + 1];
	struct ath12k_home_offchan_params params;
	int ret = 0;
	int func = 0, chan = 0, band = 0, scan_dur = 0, freq = 0;
	int nl80211_band = 0;
	u32 transaction_id = 0;
	u8 num_frames = 0;
	u8 link_id = 0;
	u8 gpr_band;
	bool is_mld = false;

	ret = nla_parse(tb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX,
			data, data_len,
			ath12k_vendor_home_offchan_tx_rx_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute with vendor offchan %d\n", ret);
		return ret;
	}

	/* Extract transaction ID for event correlation */
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID])
		transaction_id = nla_get_u32(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID]);

	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC])
		func = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC]);
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN])
		chan = nla_get_u16(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN]);
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN_BAND])
		band = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_CHAN_BAND]);
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SCAN_DUR])
		scan_dur = nla_get_u16(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_SCAN_DUR]);
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES])
		num_frames = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES]);

	/* Check if MLD is enabled */
	if (tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_IS_MLD])
		is_mld = nla_get_flag(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_IS_MLD]);

	/* Extract link_id only if MLD is enabled */
	if (is_mld && tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_LINK_ID])
		link_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_LINK_ID]);

	/* Convert application band to NL80211 band for frequency calculation */
	nl80211_band = ath12k_convert_app_band_to_nl80211(band);
	if (nl80211_band < 0) {
		ath12k_err(NULL, "Invalid band value: %d\n", band);
		return -EINVAL;
	}

	freq = ieee80211_channel_to_frequency(chan, nl80211_band);
	/* Handle GPR function - store frame in the arvif for later use */
	if (func == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_GPR) {
		struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
		struct ath12k_hw *ah;
		struct ieee80211_vif *vif;
		struct ath12k_vif *ahvif;
		struct ath12k_link_vif *arvif;
		struct nlattr *offchan_frame;
		struct nlattr *frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX + 1];
		const u8 *buf;
		int buf_len;
		struct sk_buff *skb;
		u8 effective_link_id;
		u8 nss = 0, preamble = 0, mcs = 0;

		ah = ath12k_hw_to_ah(hw);
		if (!ah) {
			ath12k_err(NULL, "GPR: Failed to get ah from wiphy\n");
			return -ENODEV;
		}

		vif = wdev_to_ieee80211_vif(wdev);
		if (!vif) {
			ath12k_err(NULL, "GPR: Failed to get vif from wdev\n");
			return -ENODEV;
		}

		ahvif = ath12k_vif_to_ahvif(vif);
		if (!ahvif) {
			ath12k_err(NULL, "GPR: Failed to get ahvif from vif\n");
			return -ENODEV;
		}

		if (link_id != INVALID_LINK_ID && link_id >= ATH12K_NUM_MAX_LINKS) {
			ath12k_err(NULL, "GPR: Invalid link_id %u\n", link_id);
			return -EINVAL;
		}

		if (!tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME]) {
			ath12k_err(NULL, "GPR: Frame attribute missing\n");
			return -EINVAL;
		}

		offchan_frame = tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME];

		/* Parse the single nested frame entry (num_frames == 1) */
		{
			struct nlattr *frame;
			int rem;

			buf = NULL;
			buf_len = 0;
			nla_for_each_nested(frame, offchan_frame, rem) {
				ret = nla_parse_nested(frame_tb,
						       QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MAX,
						       frame,
						       ath12k_vendor_home_offchan_tx_rx_frame_policy,
						       NULL);
				if (ret) {
					ath12k_err(NULL, "GPR: Failed to parse frame: %d\n", ret);
					return ret;
				}
				if (!frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]) {
					ath12k_err(NULL, "GPR: Frame data missing\n");
					return -EINVAL;
				}
				buf = nla_data(frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]);
				buf_len = nla_len(frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_DATA]);
				if (frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS])
					nss = nla_get_u8(frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_NSS]);
				if (frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE])
					preamble = nla_get_u8(frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_PREAMBLE]);
				if (frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS])
					mcs = nla_get_u8(frame_tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME_MCS]);
				break; /* Only one frame expected */
			}
		}

		if (!buf || !buf_len) {
			ath12k_err(NULL, "GPR: No valid frame data found\n");
			return -EINVAL;
		}

		rcu_read_lock();

		effective_link_id = (link_id == INVALID_LINK_ID || !ieee80211_vif_is_mld(vif))
					? 0 : link_id;

		if (effective_link_id == 0) {
			arvif = &ahvif->deflink;
		} else {
			arvif = rcu_dereference(ahvif->link[effective_link_id]);
		}

		if (!arvif || !arvif->ar) {
			rcu_read_unlock();
			ath12k_err(NULL, "GPR: Failed to get arvif for link_id %u\n",
				   effective_link_id);
			return -EINVAL;
		}

		rcu_read_unlock();

		skb = dev_alloc_skb(buf_len);
		if (!skb) {
			ath12k_err(arvif->ar->ab, "GPR: Failed to allocate skb\n");
			return -ENOMEM;
		}
		skb_put_data(skb, buf, buf_len);

		/* GPR is a broadcast Probe Response — NSS must be 1 */
		if (nss != 1) {
			ath12k_err(NULL,"GPR: nss %u not supported, bcast require nss=1\n",
				   nss);
			dev_kfree_skb_any(skb);
			return -EINVAL;
		}

		/* Validate TX params at configuration time */
		{
			if (arvif->ar->pdev->cap.supported_bands & BIT(NL80211_BAND_5GHZ))
				gpr_band = BAND_5GHZ;
			else if (arvif->ar->pdev->cap.supported_bands & BIT(NL80211_BAND_6GHZ))
				gpr_band = BAND_6GHZ;
			else
				gpr_band = BAND_2GHZ;

			ret = ath12k_validate_mgmt_tx_params(arvif->ar,
							     arvif->ahvif->vif,
							     preamble, mcs,
							     gpr_band);
		}
		if (ret) {
			dev_kfree_skb_any(skb);
			return ret;
		}

		/* Free any previously stored GPR frame before replacing */
		if (arvif->arvif_extn.gpr_frame) {
			dev_kfree_skb_any(arvif->arvif_extn.gpr_frame);
			arvif->arvif_extn.gpr_frame = NULL;
		}

		arvif->arvif_extn.gpr_frame = skb;
		/* period is carried via SCAN_DUR in the NL message for GPR */
		if (scan_dur != arvif->arvif_extn.gpr_period)
			atomic_inc(&arvif->arvif_extn.gpr_timer_resize_count);
		arvif->arvif_extn.gpr_period = scan_dur;
		arvif->arvif_extn.gpr_nss = nss;
		arvif->arvif_extn.gpr_preamble = preamble;
		arvif->arvif_extn.gpr_mcs = mcs;

		ath12k_info(arvif->ar->ab,
			    "GPR params: period=%u nss=%u preamble=%u mcs=%u link_id=%u\n",
			    scan_dur, nss, preamble, mcs, effective_link_id);
		ath12k_info(arvif->ar->ab,
			    "GPR frame: len=%d\n", buf_len);
		print_hex_dump(KERN_INFO, "GPR pkt: ", DUMP_PREFIX_OFFSET,
			       16, 1, skb->data, buf_len, true);

		/* Start the periodic GPR timer if enabled; first TX fires immediately */
		if (arvif->arvif_extn.gpr_timer_enabled)
			mod_timer(&arvif->arvif_extn.gpr_timer, jiffies);
		return 0;
	}

	/* Handle CANCEL function - doesn't need frame data */
	if (func == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_CANCEL) {
		struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
		struct ath12k_hw *ah;
		struct ath12k *ar;
		struct ath12k_raw_pkt_ctx *raw_pkt_ctx;
		struct wireless_dev *cancel_wdev = wdev;

		ah = ath12k_hw_to_ah(hw);
		if (!ah) {
			ath12k_err(NULL, "Failed to get ah from wiphy for cancel\n");
			return -ENODEV;
		}
		/* Get ar from band */
		ar = ath12k_ah_to_ar(ah, band);
		if (!ar) {
			ath12k_err(NULL, "Failed to get ar from ah for cancel\n");
			return -ENODEV;
		}
		raw_pkt_ctx = &ar->ar_extn.raw_pkt_ctx;
		/* Check if there's an ongoing operation */
		if (!raw_pkt_ctx->vif) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "No ongoing operation to cancel\n");
			return 0;
		}
		/* Verify transaction ID matches if provided */
		if (transaction_id != 0 && raw_pkt_ctx->transaction_id != transaction_id) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "Transaction ID mismatch: expected 0x%x, got 0x%x\n",
				   raw_pkt_ctx->transaction_id, transaction_id);
			return -EINVAL;
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Cancelling operation for transaction 0x%x\n",
			   raw_pkt_ctx->transaction_id);
		/* 1. Cancel timer if running */
		if (timer_pending(&raw_pkt_ctx->offchan_timer)) {
			del_timer_sync(&raw_pkt_ctx->offchan_timer);
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "Timer cancelled\n");
		}
		/* 2. Drain packet queue (stop ongoing transmission) */
		ath12k_home_offchan_tx_drain(raw_pkt_ctx);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Packet queue drained\n");
		/* 3. Send cancel confirmation event to userspace */
		if (cancel_wdev) {
			schedule_home_offchan_stats_event_ext(cancel_wdev, raw_pkt_ctx->transaction_id, raw_pkt_ctx);
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "Cancel confirmation event scheduled\n");
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "CANCEL operation completed successfully\n");
		return 0;
	}

	/* Frame attribute is not required for RX function */
	if (func != QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX) {
		if (!tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME]) {
			ath12k_err(NULL, "Offchan Frame missing for TX function\n");
			return -EINVAL;
		}
	}

	/* Assign values to params structure */
	params.wdev = wdev;
	params.offchan_frame = tb[QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FRAME];
	params.band = band;
	params.freq = freq;
	params.transaction_id = transaction_id;
	params.scan_dur = scan_dur;
	params.num_frames = num_frames;
	params.func = func;
	params.link_id = link_id;
	params.is_mld = is_mld;

	/* Call the unified frame handler for both TX and RX functions */
	ret = ath12k_vendor_home_offchan_tx_rx_frame(&params);
	if (ret) {
		ath12k_err(NULL, "failed to process home/offchan operation\n");
		return ret;
	}

	return 0;
}

int send_home_offchan_stats_event_with_transaction_id(struct wireless_dev *wdev,
						      u32 transaction_id,
						      struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	struct sk_buff *skb;
	struct ath12k_offchan_stat *stats = NULL;
	int i;

	/* Get the statistics from raw_pkt_ctx */
	if (raw_pkt_ctx)
		stats = &raw_pkt_ctx->chan_stat;

	/* Get the wireless device from vdev */
	if (!wdev) {
		ath12k_err(NULL, "Invalid wireless device\n");
		return -EINVAL;
	}

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
		QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX_INDEX, GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL, "Failed to allocate skb for vendor event\n");
		return -ENOMEM;
	}

	/* Add interface index so userspace can identify the wdev */
	if (wdev->netdev &&
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_IFINDEX, wdev->netdev->ifindex)) {
		ath12k_err(NULL, "Failed to put ifindex\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* Add transaction ID for correlation */
	if (transaction_id && nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_TRANSACTION_ID, transaction_id)) {
		ath12k_err(NULL, "Failed to put transaction ID\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* Add function type */
	if (raw_pkt_ctx && nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_FUNC, raw_pkt_ctx->func_type)) {
		ath12k_err(NULL, "Failed to put function type\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* Add number of frames */
	if (raw_pkt_ctx && nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_NUM_FRAMES, raw_pkt_ctx->num_frames)) {
		ath12k_err(NULL, "Failed to put num_frames\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* Add operation status (0=success, 1=failure) */
	if (raw_pkt_ctx && nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_STATUS, raw_pkt_ctx->operation_status)) {
		ath12k_err(NULL, "Failed to put operation_status\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* Add channel switch timing for all functions (TX and RX, home and off-channel) */
	if (raw_pkt_ctx && stats) {
		if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_HTOF, stats->chanswitch_time_htof) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CHANSWITCH_FTOH, stats->chanswitch_time_ftoh)) {
			ath12k_err(NULL, "Failed to put channel switch timing\n");
			kfree_skb(skb);
			return -EINVAL;
		}
	}

	/* Add channel statistics only for RX function type */
	if (raw_pkt_ctx && stats && raw_pkt_ctx->func_type == QCA_VENDOR_WLAN_HOME_OFFCHAN_TX_RX_FUNC_RX) {
		if (nla_put_s16(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_NOISE_FLOOR, stats->noise_floor) ||
			nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_VALID, stats->blank_params.valid) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_FRAME_COUNT, stats->tx_frame_count) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_FRAME_COUNT, stats->rx_frame_count) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_RX_CLEAR_COUNT, stats->rx_clear_count) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_CYCLE_COUNT, stats->cycle_count) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_DWELL_TIME, stats->dwell_time) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_COUNT, stats->blank_params.blanking_count) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_BLANKING_DURATION, stats->blank_params.blanking_duration)) {
			ath12k_err(NULL, "Failed to put offchan stats fields\n");
			kfree_skb(skb);
			return -EINVAL;
		}
	}

	/* Add per-packet TX status if raw_pkt_ctx is provided */
	if (raw_pkt_ctx && raw_pkt_ctx->num_frames > 0) {
		struct nlattr *pkt_status_nest;
		/* Create nested attribute for packet status array */
		pkt_status_nest = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_EVENT_TX_PKT_STATUS);
		if (!pkt_status_nest) {
			ath12k_err(NULL, "Failed to create nested attribute for packet status\n");
			kfree_skb(skb);
			return -EINVAL;
		}
		/* Add status for all frames sent from application */
		for (i = 0; i < raw_pkt_ctx->num_frames && i < ATH12K_MAX_CUSTOM_TX_PKT; i++) {
			struct nlattr *status_entry;
			status_entry = nla_nest_start(skb, i + 1);
			if (!status_entry) {
				ath12k_err(NULL, "Failed to create nested attribute for status entry\n");
				kfree_skb(skb);
				return -EINVAL;
			}
			/* Add packet status id */
			if (nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_ID, raw_pkt_ctx->frame_id[i])) {
				ath12k_err(NULL, "Failed to put packet %d status id\n", i);
				kfree_skb(skb);
				return -EINVAL;
			}

			ath12k_dbg(NULL, ATH12K_DBG_WMI,
				   "Packet %d status: %u\n", i, raw_pkt_ctx->custom_tx_status[i]);
			/* Add packet status result */
			if (nla_put_u8(skb, QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_PKT_STATUS_RESULT, (u8)raw_pkt_ctx->custom_tx_status[i])) {
				ath12k_err(NULL, "Failed to put packet %d status result\n", i);
				kfree_skb(skb);
				return -EINVAL;
			}
			nla_nest_end(skb, status_entry);
		}
		nla_nest_end(skb, pkt_status_nest);
	}

	ath12k_dbg(NULL, ATH12K_DBG_WMI,
		   "OFFCHAN EVENT generated with transaction ID 0x%x, sending to userspace\n",
		   transaction_id);
	/* Send the vendor event */
	cfg80211_vendor_event(skb, GFP_KERNEL);

	if (raw_pkt_ctx) {
		/* Reset context to clean state after event is sent */
		spin_lock_bh(&raw_pkt_ctx->chan_lock);
		ath12k_reset_raw_pkt_ctx(raw_pkt_ctx);
		spin_unlock_bh(&raw_pkt_ctx->chan_lock);

		/* Release the raw_ctx_mutex AFTER context reset to ensure clean state */
		mutex_unlock(&raw_pkt_ctx->raw_ctx_mutex);
	}

	return 0;
}

/**
 * send_home_offchan_stats_work_fn_with_txn - Workqueue handler for
 * home/off-channel event with transaction ID
 * @work: Pointer to the work_struct member of offchan_event_work_with_txn structure
 *
 * This function is scheduled by schedule_home_offchan_stats_event_ext() and runs in
 * process context. It retrieves the parent offchan_event_work_with_txn structure
 * using container_of(), calls send_offchan_stats_event_with_transaction_id() to
 * send the vendor event to user space, and then frees the work structure.
 *
 * The function is used to defer the event sending from atomic context (where it
 * was scheduled) to process context (where cfg80211 vendor events can be sent).
 *
 * Context: Process context (workqueue)
 * Memory: Frees the offchan_event_work_with_txn structure
 */
static void send_home_offchan_stats_work_fn_with_txn(struct work_struct *work)
{
	struct offchan_event_work_with_txn *event_work =
			container_of(work, struct offchan_event_work_with_txn, work);

	send_home_offchan_stats_event_with_transaction_id(event_work->wdev,
							  event_work->transaction_id,
							  event_work->raw_pkt_ctx);

	kfree(event_work);
}

int schedule_home_offchan_stats_event_ext(struct wireless_dev *wdev, u32 transaction_id,
					   struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	struct offchan_event_work_with_txn *work;

	work = kzalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		ath12k_err(NULL, "Failed to allocate memory for event work, transaction 0x%x\n",
			   transaction_id);
		return -ENOMEM;
	}

	work->wdev = wdev;
	work->transaction_id = transaction_id;
	work->raw_pkt_ctx = raw_pkt_ctx;
	INIT_WORK(&work->work, send_home_offchan_stats_work_fn_with_txn);
	schedule_work(&work->work);

	return 0;
}

static ssize_t ath12k_write_gpr_timer(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	bool enable;

	if (kstrtobool_from_user(user_buf, count, &enable))
		return -EINVAL;

	if (arvif->arvif_extn.gpr_timer_enabled == enable)
		return count;

	arvif->arvif_extn.gpr_timer_enabled = enable;

	if (enable) {
		/* Re-start timer only if a GPR frame has been configured */
		if (arvif->arvif_extn.gpr_frame && arvif->arvif_extn.gpr_period)
			mod_timer(&arvif->arvif_extn.gpr_timer, jiffies);
	} else {
		del_timer_sync(&arvif->arvif_extn.gpr_timer);
		cancel_work_sync(&arvif->arvif_extn.gpr_work);
	}

	return count;
}

static ssize_t ath12k_read_gpr_timer(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	char buf[ATH12K_BUF_SIZE_32];
	size_t len;

	len = scnprintf(buf, sizeof(buf), "%u\n",
			arvif->arvif_extn.gpr_timer_enabled);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations ath12k_fops_gpr_timer = {
	.open  = simple_open,
	.write = ath12k_write_gpr_timer,
	.read  = ath12k_read_gpr_timer,
};

/**
 * ath12k_read_gpr_stats - Print GPR statistics to debugfs
 *
 * Read: cat link0/gpr_stats
 * Write 0: echo 0 > link0/gpr_stats  (clears all counters)
 */
static ssize_t ath12k_read_gpr_stats(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct ath12k_link_vif_extn *extn = &arvif->arvif_extn;
	char buf[512];
	int len = 0;
	u64 expires_ns = 0;

	if (timer_pending(&extn->gpr_timer)) {
		long remaining = (long)(extn->gpr_timer.expires - jiffies);

		if (remaining > 0)
			expires_ns = jiffies_to_nsecs((unsigned long)remaining);
	}

	len += scnprintf(buf + len, sizeof(buf) - len, "------------------------------------\n");
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR on this VAP enabled    - %d   |\n", extn->gpr_timer_enabled ? 1 : 0);
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR state                  - %s   |\n", timer_pending(&extn->gpr_timer) ? "Active" : "Dormant");
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR Minimum Period (ms)    - %d   |\n", ATH12K_GPR_MIN_PERIOD_MS);
	len += scnprintf(buf + len, sizeof(buf) - len, "| Current Timestamp (ns)     - %llu |\n", ktime_get_ns());
	len += scnprintf(buf + len, sizeof(buf) - len, "| Timer expires in (ns)      - %llu |\n", expires_ns);
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR timer start count      - %u   |\n", atomic_read(&extn->gpr_timer_start_count));
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR timer resize count     - %u   |\n", atomic_read(&extn->gpr_timer_resize_count));
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR timer send count       - %u   |\n", atomic_read(&extn->gpr_send_count));
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR timer user period (ms) - %u   |\n", extn->gpr_period);
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR NSS                    - %u   |\n", extn->gpr_nss);
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR Preamble               - %u   |\n", extn->gpr_preamble);
	len += scnprintf(buf + len, sizeof(buf) - len, "| GPR MCS                    - %u   |\n", extn->gpr_mcs);
	len += scnprintf(buf + len, sizeof(buf) - len, "------------------------------------\n");

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_write_gpr_stats(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath12k_link_vif *arvif = file->private_data;
	struct ath12k_link_vif_extn *extn = &arvif->arvif_extn;
	u8 val;

	if (kstrtou8_from_user(user_buf, count, 0, &val))
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	/* Clear all GPR statistics counters */
	atomic_set(&extn->gpr_timer_start_count,  0);
	atomic_set(&extn->gpr_timer_resize_count, 0);
	atomic_set(&extn->gpr_send_count,         0);

	return count;
}

static const struct file_operations ath12k_fops_gpr_stats = {
	.open  = simple_open,
	.write = ath12k_write_gpr_stats,
	.read  = ath12k_read_gpr_stats,
};

/**
 * ath12k_gpr_debugfs_register_extn - Create GPR timer debugfs entry for a link VIF
 * @arvif: link VIF for which to create the entry
 * @link_debugfs: parent debugfs directory (vif->link_debugfs[link_id])
 */
void ath12k_gpr_debugfs_register_extn(struct ath12k_link_vif *arvif,
				      struct dentry *link_debugfs)
{
	arvif->arvif_extn.debugfs_gpr_timer =
		debugfs_create_file("gpr_timer", 0644, link_debugfs,
				    arvif, &ath12k_fops_gpr_timer);
	arvif->arvif_extn.debugfs_gpr_stats =
		debugfs_create_file("gpr_stats", 0644, link_debugfs,
				    arvif, &ath12k_fops_gpr_stats);
}
