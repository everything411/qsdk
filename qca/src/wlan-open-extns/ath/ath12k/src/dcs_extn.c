// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/netlink.h>
#include <net/mac80211.h>
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include "../core.h"
#include "../wmi.h"
#include "../mac.h"
#include "../debug.h"
#include "../vendor.h"
#include "ath12k_cmn_extn.h"
#include "dcs_extn.h"

void ath12k_mac_set_intf_detect(struct ath12k *ar, u16 intf_detect_bitmap)
{
	u16 prev_config_bitmap;
	int ret;

	spin_lock_bh(&ar->data_lock);
	intf_detect_bitmap &= ATH12K_DCS_VALID_INTF_BITMAP;
	prev_config_bitmap = ar->ar_extn.dcs_config_bitmap;

	ar->ar_extn.dcs_config_bitmap = intf_detect_bitmap;

	spin_unlock_bh(&ar->data_lock);
	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_DCS,
					intf_detect_bitmap, ar->pdev->pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set DCS config: %d\n", ret);
		spin_lock_bh(&ar->data_lock);
		ar->ar_extn.dcs_config_bitmap = prev_config_bitmap;
		spin_unlock_bh(&ar->data_lock);
	}
}

int ath12k_mac_nlwidth_to_chwidth(enum nl80211_chan_width ch_width)
{
	switch (ch_width) {
		case NL80211_CHAN_WIDTH_320:
			return ATH12K_CHWIDTH_320;
		case NL80211_CHAN_WIDTH_160:
		case NL80211_CHAN_WIDTH_80P80:
			return ATH12K_CHWIDTH_160;
		case NL80211_CHAN_WIDTH_80:
			return ATH12K_CHWIDTH_80;
		case NL80211_CHAN_WIDTH_40:
			return ATH12K_CHWIDTH_40;
		case NL80211_CHAN_WIDTH_20:
		case NL80211_CHAN_WIDTH_20_NOHT:
			return ATH12K_CHWIDTH_20;
		case NL80211_CHAN_WIDTH_16:
			return ATH12K_CHWIDTH_16;
		case NL80211_CHAN_WIDTH_10:
			return ATH12K_CHWIDTH_10;
		case NL80211_CHAN_WIDTH_8:
			return ATH12K_CHWIDTH_8;
		case NL80211_CHAN_WIDTH_4:
			return ATH12K_CHWIDTH_4;
		case NL80211_CHAN_WIDTH_2:
			return ATH12K_CHWIDTH_2;
		case NL80211_CHAN_WIDTH_1:
			return ATH12K_CHWIDTH_1;
		default:
			return ATH12K_CHWIDTH_20;
	}
}

static u32 ath12k_transform_intf_bitmap(int input_bitmap,
					struct cfg80211_chan_def *chandef)
{
	u16 input_bits[ATH12K_MAX_20MHZ_SEGMENTS] = {0};
	u16 output_bits[ATH12K_MAX_20MHZ_SEGMENTS] = {0};
	u32 start_freq, segment_freq;
	u16 bandwidth, num_sub_chans;
	u32 output_bitmap = 0;
	u16 primary_index = -1;

	if (chandef->width == NL80211_CHAN_WIDTH_80P80)
		bandwidth = ATH12K_CHWIDTH_80;
	else
		bandwidth = ath12k_mac_nlwidth_to_chwidth(chandef->width);
	num_sub_chans = bandwidth / ATH12K_CHWIDTH_20;
	start_freq = (chandef->center_freq1 - bandwidth / 2) + 10;

	for (int i = 0; i < ATH12K_MAX_20MHZ_SEGMENTS; i++) {
		segment_freq = start_freq + (i * 20);
		if (segment_freq == chandef->chan->center_freq) {
			primary_index = i;
			break;
		}
	}
	if (primary_index == -1)
		return 0;

	for (int i = 0; i < ATH12K_MAX_20MHZ_SEGMENTS; ++i)
		input_bits[i] = BIT(i) & input_bitmap;

	if (bandwidth == ATH12K_CHWIDTH_80 && primary_index >= 4)
		return 0;
	if (bandwidth == ATH12K_CHWIDTH_160 && primary_index >= 8)
		return 0;
	if (bandwidth == ATH12K_CHWIDTH_320 && primary_index >= 16)
		return 0;

	for (int i = 0; i < num_sub_chans; ++i) {
		int src = i, dst = i;

		if (bandwidth == ATH12K_CHWIDTH_40) {
			if (primary_index == 1)
				dst = 1 - i;
		} else if (bandwidth == ATH12K_CHWIDTH_80) {
			dst = intf_map_80[primary_index][i];
		} else if (bandwidth == ATH12K_CHWIDTH_160) {
			dst = intf_map_160[primary_index][i];
		} else if (bandwidth == ATH12K_CHWIDTH_320) {
			dst = intf_map_320[primary_index][i];
		}
		if (dst < 0 || dst >= ATH12K_MAX_20MHZ_SEGMENTS)
			return 0;
		output_bits[dst] = input_bits[src];
	}

	for (int i = 0; i < ATH12K_MAX_20MHZ_SEGMENTS; ++i)
		output_bitmap |= output_bits[i] ? BIT(i) : 0;

	return output_bitmap;
}

void cfg80211_intf_notify_extn(struct wiphy *wiphy, struct ath12k *ar, u16 type,
			       u32 interference_bitmap)
{
	struct ath12k_link_vif *tmp_arvif = NULL, *arvif;
	struct sk_buff *vendor_event;
	struct wireless_dev *wdev;
	int ret;

	rcu_read_lock();
	spin_lock_bh(&ar->data_lock);
	if (list_empty(&ar->arvifs)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "arvifs list is empty\n");
		spin_unlock_bh(&ar->data_lock);
		rcu_read_unlock();
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "DCS intf notify event to userspace (type=0x%x bitmap=0x%x)\n",
		   type, interference_bitmap);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "Checking arvif: is_started=%d ahvif=%p\n",
			   arvif->is_started, arvif->ahvif);
		if (!tmp_arvif && (arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP)
		    && arvif->is_started) {
			tmp_arvif = arvif;
			break;
		}
	}

	spin_unlock_bh(&ar->data_lock);

	if (!tmp_arvif || !tmp_arvif->ahvif) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "No valid arvif found for DCS intf update\n");
		rcu_read_unlock();
		return;
	}

	if (!tmp_arvif->ahvif->vif) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "vif is NULL for DCS intf update\n");
		rcu_read_unlock();
		return;
	}

	wdev = ieee80211_vif_to_wdev(tmp_arvif->ahvif->vif);

	rcu_read_unlock();

	if (!wdev) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "wdev is NULL for DCS intf update\n");
		return;
	}

	vendor_event = cfg80211_vendor_event_alloc(wiphy, wdev,
			NLMSG_DEFAULT_SIZE,
			QCA_NL80211_VENDOR_SUBCMD_DCS_INTERFERENCE_COMPUTE_INDEX,
			GFP_ATOMIC);

	if (!vendor_event) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "SKB alloc failed for CW Intf evt\n");
		return;
	}

	if (wdev->valid_links) {
		ret = nla_put_u8(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_DCS_LINK_ID,
				 tmp_arvif->link_id);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to put link id\n");
			goto nla_put_failure;
		}
	}

	ret = nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_DCS_ENABLE,
			type);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to put DCS interference type\n");
		goto nla_put_failure;
	}

	ret = nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_DCS_INTERFERENCE_BITMAP,
				interference_bitmap);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to put DCS interference bitmap\n");
		goto nla_put_failure;
	}

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;

nla_put_failure:
	nlmsg_free(vendor_event);
}

void ath12k_vendor_send_intf_notify_extn(struct ath12k *ar, u16 type,
					 u32 intf_bitmap)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	cfg80211_intf_notify_extn(hw->wiphy, ar, type, intf_bitmap);
}

void ath12k_wmi_dcs_cw_interference_event_extn(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       u32 pdev_id)
{
	struct ath12k *ar;
	struct wmi_dcs_cw_info cw_info = {};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &cw_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse cw tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "CW detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (!(ar->ar_extn.dcs_config_bitmap & WMI_DCS_CW_INTF)) {
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}
	spin_unlock_bh(&ar->data_lock);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "CW Interference detected for pdev=%d\n",
		   pdev_id);

	ath12k_vendor_send_intf_notify_extn(ar, WMI_DCS_CW_INTF, ATH12K_DCS_SEG_PRI20);
exit:
	rcu_read_unlock();
}

void ath12k_wmi_dcs_obss_interference_event_extn(struct ath12k_base *ab,
						 struct sk_buff *skb,
						 u32 pdev_id)
{
	struct ath12k *ar;
	struct wmi_dcs_obss_info obss_info = {};
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	u32 intf_bitmap, modified_host_bitmap;
	struct ath12k_hw *ah;
	struct ath12k_link_vif *arvif;
	bool ap_started = false;
	bool sta_present = false;
	int ret;

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_err(ab, "OBSS detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (!(ar->ar_extn.dcs_config_bitmap & WMI_DCS_OBSS_INTF)) {
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}
	list_for_each_entry(arvif, &ar->arvifs, list) {
		struct ieee80211_vif *vif;

		if (!arvif->is_started || !arvif->ahvif)
			continue;

		vif = arvif->ahvif->vif;
		if (!vif)
			continue;

		if (vif->type == NL80211_IFTYPE_STATION) {
			sta_present = true;
			break;
		}

		if (vif->type == NL80211_IFTYPE_AP) {
			ap_started = true;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

	if (sta_present) {
		ath12k_err(ab,
			   "dropping OBSS interference event on pdev=%d (STA vif present)\n",
			   pdev_id);
		goto exit;
	}

	if (!ap_started) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "dropping OBSS interference event on pdev=%d (no AP vif)\n",
			   pdev_id);
		goto exit;
	}

	ah = ar->ah;

	arg.ar = ar;
	arg.chanctx_conf = NULL;
	ieee80211_iter_chan_contexts_atomic(ah->hw, ath12k_mac_get_any_chanctx_conf_iter,
					    &arg);
	chanctx_conf = arg.chanctx_conf;
	if (!chanctx_conf) {
		ath12k_err(ab, "chanctx_conf is not available\n");
		goto exit;
	}

	if (!chanctx_conf->def.chan ||
	    (chanctx_conf->def.chan->band != NL80211_BAND_5GHZ &&
	     chanctx_conf->def.chan->band != NL80211_BAND_6GHZ)) {
		int band = chanctx_conf->def.chan ?
			   chanctx_conf->def.chan->band : NL80211_BAND_2GHZ;
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "dropping OBSS interference event on pdev=%d (unsupported band %d)\n",
			   pdev_id, band);
		goto exit;
	}

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &obss_info);
	if (ret) {
		ath12k_err(ab, "failed to parse obss tlv %d\n", ret);
		goto exit;
	}

	if (obss_info.chan_freq &&
	    obss_info.chan_freq != chanctx_conf->def.chan->center_freq) {
		ath12k_err(ab,
			   "dropping OBSS interference event on pdev=%d (cur_freq=%u obss_chan_freq=%u)\n",
			   pdev_id, chanctx_conf->def.chan->center_freq,
			   obss_info.chan_freq);
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "OBSS Interference detected for pdev=%d\n",
		   pdev_id);

	intf_bitmap = obss_info.chan_bw_interference_bitmap;
	if (!intf_bitmap) {
		ath12k_err(ab, "intf_bitmap is not available\n");
		goto exit;
	}

	intf_bitmap &= (~ATH12K_DCS_SEG_PRI20);

	modified_host_bitmap = ath12k_transform_intf_bitmap(intf_bitmap, &chanctx_conf->def);

	if (modified_host_bitmap == 0 && intf_bitmap != 0) {
		ath12k_err(ab, "Bitmap transformation failed\n");
	}

	if (chanctx_conf->def.radar_bitmap) {
		u16 bandwidth = ath12k_mac_nlwidth_to_chwidth(chanctx_conf->def.width);
		u16 num_sub_chans = bandwidth / ATH12K_CHWIDTH_20;
		u32 bw_mask = num_sub_chans ? (BIT(num_sub_chans) - 1) : 0;

		modified_host_bitmap |= (chanctx_conf->def.radar_bitmap & bw_mask);
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "OBSS: radar_bitmap=0x%x bw_mask=0x%x merged_bitmap=0x%x\n",
			   chanctx_conf->def.radar_bitmap, bw_mask,
			   modified_host_bitmap);
	}

	ath12k_vendor_send_intf_notify_extn(ar, WMI_DCS_OBSS_INTF, modified_host_bitmap);

exit:
	rcu_read_unlock();
}

static inline u32 dcs_get_intr_detection_thr(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->intr_detection_threshold;
}

static inline u32 dcs_get_sample_size(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->sample_size;
}

static inline u32 dcs_get_phyerr_penalty(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->phyerr_penalty;
}

static inline u32 dcs_get_phyerr_threshold(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->phyerr_threshold;
}

static inline u32 dcs_get_radarerr_threshold(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->radarerr_threshold;
}

static inline u32 dcs_get_txerr_threshold(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->txerr_threshold;
}

static inline u8 dcs_get_coch_intr_threshold(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->coch_intr_threshold;
}

static inline u8 dcs_get_user_max_cu(struct ath12k *ar) {
	struct dcs_wlan_intr_user_cfg *cfg = &ar->ar_extn.wlan_intf.dcs_cfg;
	return cfg->user_max_cu;
}

/*
 * Cache previous/current WLAN interference stats.
 */

void
ath12k_wlan_intf_update_stats_extn(struct ath12k *ar,
			const struct wmi_dcs_wlan_interference_stats *stats)
{
	spin_lock_bh(&ar->data_lock);
	/* Move current to previous if valid */
	ar->ar_extn.wlan_intf.prev_valid = ar->ar_extn.wlan_intf.curr_valid;
	if (ar->ar_extn.wlan_intf.curr_valid)
		ar->ar_extn.wlan_intf.prev_stats =
					ar->ar_extn.wlan_intf.curr_stats;

	/* Store new current stats */
	ar->ar_extn.wlan_intf.curr_stats = *stats;
	ar->ar_extn.wlan_intf.curr_valid = true;
	spin_unlock_bh(&ar->data_lock);
}

void ath12k_wlan_intf_rollover_stats_extn(struct ath12k *ar)
{
	spin_lock_bh(&ar->data_lock);
	ar->ar_extn.wlan_intf.prev_valid = ar->ar_extn.wlan_intf.curr_valid;
	if (ar->ar_extn.wlan_intf.curr_valid)
		ar->ar_extn.wlan_intf.prev_stats =
					ar->ar_extn.wlan_intf.curr_stats;
	spin_unlock_bh(&ar->data_lock);
}

void ath12k_wmi_dcs_wlan_interference_event_extn(struct ath12k_base *ab,
						 struct sk_buff *skb,
						 u32 pdev_id)
{
	struct wmi_dcs_wlan_interference_stats wlan_info = {};
	struct wmi_dcs_wlan_interference_stats *prev, *curr;
	struct dcs_wlan_intr_metrics metrics = {};
	struct ath12k_mac_get_any_chanctx_conf_arg cca;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret;
	u32 cycle_count_den, tsf_den;
	u32 wlan_bitmap = ATH12K_DCS_SEG_PRI20, full_bw_bitmap;
	u16 bw, num_sub;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &wlan_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse wlan intr tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "WLAN INTR detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (!(ar->ar_extn.dcs_config_bitmap & WMI_DCS_WLAN_INTF)) {
		spin_unlock_bh(&ar->data_lock);
		goto exit;
	}
	spin_unlock_bh(&ar->data_lock);

	/* Update prev/curr cache with the new stats */
	ath12k_wlan_intf_update_stats_extn(ar, &wlan_info);

	/* Fetch snapshots */
	if (!ar->ar_extn.wlan_intf.prev_valid ||
	    !ar->ar_extn.wlan_intf.curr_valid)
		goto exit;

	prev = &ar->ar_extn.wlan_intf.prev_stats;
	curr = &ar->ar_extn.wlan_intf.curr_stats;

	/* Basic sanity: counters wrap or TSF non-increment */
	if (curr->listen_time <= 0 ||
	    curr->reg_tsf32 <= prev->reg_tsf32) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "DCS: ignoring due to non-incremental TSF or listen_time<=0");
		/* Roll current into previous for next run */
		ath12k_wlan_intf_rollover_stats_extn(ar);
		goto exit;
	}

	metrics.reg_tsf_delta = curr->reg_tsf32 - prev->reg_tsf32;

	/* Ignore if RXCLR decreased (reset scenario) */
	if (prev->reg_rxclr_cnt > curr->reg_rxclr_cnt) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "DCS: ignoring due to negative rxclr count");
		goto rollover_exit;
	}

	metrics.rxclr_delta = curr->reg_rxclr_cnt - prev->reg_rxclr_cnt;
	metrics.rxclr_ext_delta = curr->reg_rxclr_ext_cnt -
				  prev->reg_rxclr_ext_cnt;
	metrics.tx_frame_delta = curr->reg_tx_frame_cnt -
				 prev->reg_tx_frame_cnt;
	metrics.rx_frame_delta = curr->reg_rx_frame_cnt -
				 prev->reg_rx_frame_cnt;
	metrics.cycle_count_delta = curr->reg_cycle_cnt - prev->reg_cycle_cnt;

	/* Avoid divide by 0 after shift */
	if ((metrics.cycle_count_delta >> 8) == 0 || (metrics.reg_tsf_delta >> 8) == 0) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "DCS: cycle_count_delta too small");
		goto rollover_exit;
	}

	/* Compute channel utilization percentages */
	cycle_count_den = (u32)(metrics.cycle_count_delta >> 8);
	tsf_den = (u32)(metrics.reg_tsf_delta >> 8);

	metrics.reg_total_cu = div_u64(((u64)(metrics.rxclr_delta >> 8) * 100), cycle_count_den);
	metrics.reg_tx_cu = div_u64(((u64)(metrics.tx_frame_delta >> 8) * 100), cycle_count_den);
	metrics.reg_rx_cu = (u32)div_u64(((u64)(metrics.rx_frame_delta >> 8) * 100), cycle_count_den);

	metrics.rx_time_cu = div_u64(((u64)(curr->rx_time >> 8) * 100), tsf_den);

	if (metrics.rx_time_cu > metrics.reg_rx_cu)
		metrics.rx_time_cu = metrics.reg_rx_cu;

	metrics.wasted_tx_cu = ((curr->tx_waste_time >> 8) * 100) / tsf_den;

	if (metrics.reg_tx_cu < metrics.wasted_tx_cu)
		metrics.wasted_tx_cu = metrics.reg_tx_cu;

	metrics.tx_err = (metrics.reg_tx_cu && metrics.wasted_tx_cu) ?
		 (metrics.wasted_tx_cu * 100) / metrics.reg_tx_cu : 0;

	metrics.reg_unused_cu = (metrics.reg_total_cu >= (metrics.reg_tx_cu +
				 metrics.rx_time_cu)) ?	(metrics.reg_total_cu -
				 (metrics.reg_tx_cu + metrics.rx_time_cu)) : 0;

	metrics.total_wasted_cu = metrics.reg_unused_cu + metrics.wasted_tx_cu;

	/* PHY error deltas and rates */
	if (curr->reg_ofdm_phyerr_cnt < prev->reg_ofdm_phyerr_cnt)
		metrics.reg_ofdm_phyerr_delta = curr->reg_ofdm_phyerr_cnt;
	else
		metrics.reg_ofdm_phyerr_delta = curr->reg_ofdm_phyerr_cnt -
					prev->reg_ofdm_phyerr_cnt;

	if (curr->reg_cck_phyerr_cnt < prev->reg_cck_phyerr_cnt)
		metrics.reg_cck_phyerr_delta = curr->reg_cck_phyerr_cnt;
	else
		metrics.reg_cck_phyerr_delta = curr->reg_cck_phyerr_cnt -
					       prev->reg_cck_phyerr_cnt;

	metrics.reg_ofdm_phyerr_cu = metrics.reg_ofdm_phyerr_delta *
				     dcs_get_phyerr_penalty(ar);
	metrics.total_wasted_cu += (metrics.reg_ofdm_phyerr_cu > 0) ?
				    (((metrics.reg_ofdm_phyerr_cu >> 8) * 100) /
				     tsf_den) : 0;
	metrics.ofdm_phy_err_rate = (curr->reg_ofdm_phyerr_cnt * 1000) /
			     curr->listen_time;
	metrics.cck_phy_err_rate = (curr->reg_cck_phyerr_cnt * 1000) /
			    curr->listen_time;

	metrics.max_phy_err_rate = max(metrics.ofdm_phy_err_rate,
				       metrics.cck_phy_err_rate);
	metrics.max_phy_err_count = max(curr->reg_ofdm_phyerr_cnt,
					curr->reg_cck_phyerr_cnt);

	if (((metrics.max_phy_err_rate >= dcs_get_phyerr_threshold(ar)) &&
	    (metrics.max_phy_err_count > dcs_get_phyerr_threshold(ar))) ||
	    (curr->phyerr_cnt > dcs_get_radarerr_threshold(ar)))
		metrics.too_many_phy_errors = 1;

	if (metrics.reg_unused_cu >= dcs_get_coch_intr_threshold(ar))
		ar->ar_extn.intf_detect_cnt += 2;
	else if (metrics.too_many_phy_errors &&
		 (((metrics.total_wasted_cu > (dcs_get_coch_intr_threshold(ar) + 10)) &&
		 ((metrics.reg_tx_cu + metrics.reg_rx_cu) > dcs_get_user_max_cu(ar))) ||
		 ((metrics.reg_tx_cu > DCS_TX_MAX_CU) &&
		 (metrics.tx_err >= dcs_get_txerr_threshold(ar)))))
		ar->ar_extn.intf_detect_cnt++;

	if (ar->ar_extn.intf_detect_cnt >= dcs_get_intr_detection_thr(ar)) {
		ar->ar_extn.intf_detect_cnt = 0;
		ar->ar_extn.samp_cnt = 0;
		/*
		 * Fetch the current channel context so we can build a bitmap
		 * that covers the full operating bandwidth (e.g. 160 MHz),
		 * not just the primary 20 MHz sub-channel.  Hostapd uses this
		 * bitmap together with cur_chan_params to drive find_random_channel();
		 * sending only ATH12K_DCS_SEG_PRI20 causes it to treat the
		 * channel as 20 MHz wide and pick a wrong/narrow replacement.
		 */
		cca.ar = ar;
		cca.chanctx_conf = NULL;
		ah = ar->ah;

		ieee80211_iter_chan_contexts_atomic(ah->hw,
						   ath12k_mac_get_any_chanctx_conf_iter,
						   &cca);
		if (cca.chanctx_conf) {
			bw = ath12k_mac_nlwidth_to_chwidth(
					cca.chanctx_conf->def.width);
			num_sub = bw / ATH12K_CHWIDTH_20;
			/*
			 * Build a bitmap with all sub-channel bits set
			 * for the operating bandwidth, then transform it
			 * to the host-side segment ordering.  This tells
			 * hostapd the full channel is affected, matching
			 * what the OBSS handler does.
			 */
			full_bw_bitmap = num_sub ?
				(BIT(num_sub) - 1) : ATH12K_DCS_SEG_PRI20;
			wlan_bitmap = ath12k_transform_intf_bitmap(full_bw_bitmap,
								   &cca.chanctx_conf->def);
			if (!wlan_bitmap)
				wlan_bitmap = ATH12K_DCS_SEG_PRI20;
		}
		ath12k_vendor_send_intf_notify_extn(ar, WMI_DCS_WLAN_INTF,
						    wlan_bitmap);
	} else if (!ar->ar_extn.intf_detect_cnt ||
		ar->ar_extn.samp_cnt >= dcs_get_sample_size(ar)) {
		ar->ar_extn.intf_detect_cnt = 0;
		ar->ar_extn.samp_cnt = 0;
	}

	ar->ar_extn.samp_cnt++;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "DCS: deltas rxclr=%u rxclr_ext=%u tx_frame=%u rx_frame=%u cycle=%u",
		   metrics.rxclr_delta, metrics.rxclr_ext_delta,
		   metrics.tx_frame_delta, metrics.rx_frame_delta,
		   metrics.cycle_count_delta);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "DCS: cu total=%u tx=%u rx=%u rx_time=%u unused=%u wasted_tx=%u tx_err=%u\n",
		   metrics.reg_total_cu, metrics.reg_tx_cu, metrics.reg_rx_cu,
		   metrics.rx_time_cu, metrics.reg_unused_cu,
		   metrics.wasted_tx_cu, metrics.tx_err);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "DCS: phyerr_delta ofdm=%u cck=%u rates ofdm=%u cck=%u total_wasted_cu=%u phyerr_ofdm_cu=%u\n",
		   metrics.reg_ofdm_phyerr_delta, metrics.reg_cck_phyerr_delta,
		   metrics.ofdm_phy_err_rate, metrics.cck_phy_err_rate,
		   metrics.total_wasted_cu, metrics.reg_ofdm_phyerr_cu);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "DCS: max_phy_err_rate=%u max_phy_err_count=%u\n",
		   metrics.max_phy_err_rate, metrics.max_phy_err_count);
rollover_exit:
	ath12k_wlan_intf_rollover_stats_extn(ar);
exit:
	rcu_read_unlock();
}
