// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor dual ring
 * model
 */
#include <linux/if_vlan.h>
#include "../dp_mon.h"
#include "../dp_tx_mon.h"
#include "../debug.h"
#include "hal_qcn9274.h"
#include "hal_mon.h"
#include "../peer.h"
#include "../dp_mon_filter.h"
#include "dp_mon2.h"
#include "../trace.h"
#include "../wmi.h"
#include "../ath12k_notif.h"
#include "dp_tx_mon.h"
#ifdef CPTCFG_QCN_EXTN
#include "../../net/mac80211/qcn_extns/cmn_extn.h"
#include "qcn_extns/dp_mon_extn.h"
#endif /* CPTCFG_QCN_EXTN */

static inline u8
ath12k_wifi7_get_ext_mon_peer_filter_mode(struct ath12k_dp_tx_ext_mon *tx_ext_mon)
{
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon_config;
	u8 filter_mode;

	lockdep_assert_held(&tx_ext_mon->tx_ext_mon_lock);
	tx_ext_mon_config = tx_ext_mon->tx_ext_mon_config;
	filter_mode = (tx_ext_mon_config->fp_enabled << 1) |
		       tx_ext_mon_config->fpmo_enabled;

	if (tx_ext_mon_config->monitor_flags == ATH12K_EXT_MON_PKT_CAP)
		filter_mode = ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER;
	return filter_mode;
}

/**
 * ath12k_wifi7_get_ext_mon_peer_filter_mode_locked() - classify the active TX ext-mon
 * peer-filter mode from the current configuration
 * @dp_pdev: DP pdev handle
 *
 * Maps the (fp_enabled, fpmo_enabled) bit-pair in tx ext_mon structure to a
 * &enum ath12k_dp_tx_ext_mon_peer_filter value that controls both the
 * per-frame SW filter path and whether the staged peer list is pushed
 * into the HW peer-filter table:
 *
 * fp_enabled  fpmo_enabled  result                               HW peers
 * ----------  ------------  -----------------------------------  --------
 *        0            0     (ext_mon disabled — not applicable)     -
 *        0            1     %ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER    yes
 *        1            0     %ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER   no
 *        1            1     %ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER    no
 *
 * When monitor_flags is %ATH12K_EXT_MON_PKT_CAP the mode is forced to
 * %ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER regardless of the bit-pair above.
 *
 * Return: &enum ath12k_dp_tx_ext_mon_peer_filter value, or 0 if the
 *         ext-monitor config pointer is NULL or disabled.
 */
u8 ath12k_wifi7_get_ext_mon_peer_filter_mode_locked(struct ath12k_pdev_dp *dp_pdev)
{
	u8 filter_mode = 0;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon;
	struct ath12k_pdev_tx_mon *tx_mon;

	if (!dp_pdev || !dp_pdev->dp_mon_pdev)
		return 0;

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	if (!tx_mon)
		return 0;

	lockdep_assert_not_held(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	spin_lock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
	tx_ext_mon = tx_mon->tx_ext_mon.tx_ext_mon_config;
	if (unlikely(!tx_ext_mon)) {
		ath12k_warn(dp_pdev->dp, "ext_mon in tx direction is null\n");
		spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);
		return 0;
	}
	filter_mode = ath12k_wifi7_get_ext_mon_peer_filter_mode(&tx_mon->tx_ext_mon);
	spin_unlock(&tx_mon->tx_ext_mon.tx_ext_mon_lock);

	return filter_mode;
}

/**
 * ath12k_wifi7_dp_ext_mon_filter() - decide whether an MPDU passes TX
 *	ext-mon capture filtering for the pdev's current mode.
 * @dp_pdev: DP pdev handle
 * @mpdu: the TX MPDU skb being considered for ext-mon capture
 *
 * Ext-mon capture filtering runs in one of four modes. Three modes are
 * selected by the (fp_enabled, fpmo_enabled) pair; the fourth is forced
 * when monitor_flags == ATH12K_EXT_MON_PKT_CAP regardless of those bits:
 *
 *   - ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER (fp_enabled=1, fpmo_enabled=0):
 *     blanket "all_peer" capture. Every peer is accepted, so only the
 *     frame-type subtype filter (fp) needs to pass.
 *
 *   - ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER (fp_enabled=0, fpmo_enabled=1):
 *     targeted "target_peer" capture whose peer scoping is enforced by
 *     firmware via the HW peer-filter table. Both the subtype filter
 *     (fpmo) and the local peer_list check (selfgen / leaked frames) must
 *     pass.
 *
 *   - ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER (fp_enabled=1, fpmo_enabled=1):
 *     peer scoping is enforced in software via peer_list. If the peer
 *     matches, the frame passes unconditionally (return 0); if not, the
 *     frame-type filter (fp) is checked and its result returned.
 *
 *   - ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER (monitor_flags==PKT_CAP):
 *     special packet capture mode. The (fp_enabled, fpmo_enabled) bits are
 *     ignored; filter_mode is forced to SPC_PEER_FILTER. Peer scoping is
 *     host-side only via peer_list: a matching peer passes (return 0); a
 *     non-matching peer is passed to the frame-type filter (fp). HW
 *     peer-filter programming is skipped for this mode.
 *
 * Return: 0 if the MPDU passes filtering for the active mode, -EINVAL
 *	if it is rejected (or if fraglist/skb fragment counts disagree).
 */
int ath12k_wifi7_dp_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu)
{
	struct ath12k_pdev_tx_mon *tx_mon;
	u8 filter_mode;
	int ret = 0, frag_count;
	struct ieee80211_hdr *wh;
	struct ath12k_dp_tx_ext_mon_config *tx_ext_mon_config;

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	if (!tx_mon)
		return 0;

	tx_ext_mon_config = tx_mon->tx_ext_mon.tx_ext_mon_config;
	filter_mode = ath12k_wifi7_get_ext_mon_peer_filter_mode(&tx_mon->tx_ext_mon);
	frag_count = ath12k_dp_mon_get_num_frags_in_fraglist(mpdu);
	if (frag_count) {
		if (!skb_shinfo(mpdu)->nr_frags) {
			ath12k_dbg(NULL, ATH12K_DBG_DP_MON_TX,
				   "ext mon: mpdu fraglist reports %d frag(s) but skb nr_frags is 0\n",
				   frag_count);
			return -EINVAL;
		}
		wh = (struct ieee80211_hdr *)ath12k_dp_mon_skb_get_frag_addr(mpdu, 0);
	} else {
		wh = (struct ieee80211_hdr *)mpdu->data;
	}

	switch (filter_mode) {
	case ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER:
		/* Perform subtype filtering */
		ret = ath12k_dp_ext_mon_filter_subtype(wh, &tx_ext_mon_config->fpmo);
		if (ret)
			return ret;
		/* Perform s/w peer check to filter selfgen & leaked frames */
		return ath12k_dp_ext_mon_filter_peer(wh, &tx_ext_mon_config->peer_list);
	case ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER:
		/* Perform subtype filtering */
		return ath12k_dp_ext_mon_filter_subtype(wh, &tx_ext_mon_config->fp);
	case ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER:
	case ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER:
		/* Perform s/w peer check to filter targeted peer frames */
		ret = ath12k_dp_ext_mon_filter_peer(wh, &tx_ext_mon_config->peer_list);
		if (!ret)
			return 0;
		/* Perform Type filtering */
		return ath12k_dp_ext_mon_filter_type(wh, &tx_ext_mon_config->fp);
	default:
		break;
	}

	return 0;
}

void
ath12k_wifi7_dp_mon_tx_setup_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev,
					    struct htt_tx_ring_tlv_filter *src_tlv_filter)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon =
		&dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ath12k_ext_mon_pkt_config ext_mon_filter;
	u8 mgmt_len, ctrl_len, data_len;
	bool sw_peer_filtering;

	src_tlv_filter->tx_mon_downstream_tlv_flags =
					HTT_TX_MON_FILTER_DW_STRM_TLV_DEFAULT_MODE;
	src_tlv_filter->tx_mon_upstream_tlv_flags0 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG0;
	src_tlv_filter->tx_mon_upstream_tlv_flags1 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG1;
	src_tlv_filter->tx_mon_upstream_tlv_flags2 =
					HTT_TX_MON_FILTER_UP_STRM_TLV_FLAG2;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_config = tx_ext_mon->tx_ext_mon_config;
	if (!tx_config) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		return;
	}
	if (!tx_config->fp_enabled && !tx_config->fpmo_enabled) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		return;
	}

	sw_peer_filtering = (tx_config->fp_enabled && tx_config->fpmo_enabled);
	ext_mon_filter = tx_config->fp_enabled ? tx_config->fp : tx_config->fpmo;

	if (ext_mon_filter.filter[ATH12K_EXT_MON_FRAME_MGMT] || sw_peer_filtering)
		src_tlv_filter->tx_mon_mgmt_filter = 0x1;
	if (ext_mon_filter.filter[ATH12K_EXT_MON_FRAME_CTRL] || sw_peer_filtering)
		src_tlv_filter->tx_mon_ctrl_filter = 0x1;
	if (ext_mon_filter.filter[ATH12K_EXT_MON_FRAME_DATA] || sw_peer_filtering)
		src_tlv_filter->tx_mon_data_filter = 0x1;

	mgmt_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_MGMT];
	src_tlv_filter->tx_mon_mgmt_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(mgmt_len);
	ctrl_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_CTRL];
	src_tlv_filter->tx_mon_ctrl_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(ctrl_len);
	data_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_DATA];
	src_tlv_filter->tx_mon_data_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(data_len);

	switch (tx_config->level) {
	case ATH12K_EXT_MON_FILTER_LEVEL_MPDU:
	case ATH12K_EXT_MON_FILTER_LEVEL_PPDU:
		src_tlv_filter->mgmt_mpdu_msdu_log_en = 1;
		src_tlv_filter->mgmt_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		src_tlv_filter->ctrl_mpdu_msdu_log_en = 1;
		src_tlv_filter->ctrl_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		src_tlv_filter->data_mpdu_msdu_log_en = 1;
		src_tlv_filter->data_log_typ = HTT_TX_MON_WMASK_IN2_MPDU_LOG;
		break;
	default:
		break;
	}
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
}

void
ath12k_wifi7_dp_mon_tx_setup_spl_pkt_cap_filter(struct ath12k_pdev_dp *dp_pdev,
						struct htt_tx_ring_tlv_filter
						 *src_tlv_filter)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_dp_tx_ext_mon *tx_ext_mon =
		&dp_mon_pdev->dp_pdev_tx_mon->tx_ext_mon;
	struct ath12k_dp_tx_ext_mon_config *tx_config;
	struct ath12k_ext_mon_pkt_config ext_mon_filter;
	struct ath12k_dp *dp = dp_pdev->dp;
	u8 mgmt_len, ctrl_len, data_len;

	/* When level is MSDU, it will be forced to MPDU to match with prop */
	ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
	src_tlv_filter->mac_addr_filter_en = 1;

	spin_lock(&tx_ext_mon->tx_ext_mon_lock);
	tx_config = tx_ext_mon->tx_ext_mon_config;
	if (!tx_config || (!tx_config->fp_enabled && !tx_config->fpmo_enabled)) {
		spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
		return;
	}

	/* Lengths for fp and fpmo are same so only considering fp */
	ext_mon_filter = tx_config->fp;

	mgmt_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_MGMT];
	src_tlv_filter->tx_mon_mgmt_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(mgmt_len);
	ctrl_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_CTRL];
	src_tlv_filter->tx_mon_ctrl_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(ctrl_len);
	data_len = ext_mon_filter.len[ATH12K_EXT_MON_FRAME_DATA];
	src_tlv_filter->tx_mon_data_pkt_dma_len =
		ath12k_dp_mon_tx_get_ext_mon_filter_len(data_len);
	spin_unlock(&tx_ext_mon->tx_ext_mon_lock);
}

int ath12k_wifi7_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					 bool enable)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct dp_mon_tx_filter filter = {0};
	struct htt_tx_ring_tlv_filter *src_tlv_filter;
	enum dp_mon_tx_filter_srng_type srng_type =
		DP_MON_TX_FILTER_SRNG_TYPE_TXMON_DEST;
	u8 mode;

	if (!dp_mon_pdev || !dp_mon_pdev->dp_pdev_tx_mon) {
		ath12k_err(NULL, "TX Monitor: mon pdev / tx mon is NULL\n");
		return -EINVAL;
	}

	tx_mon = dp_mon_pdev->dp_pdev_tx_mon;
	mode = tx_mon->tx_monitor_mode;

	if (!dp || !dp->hal) {
		ath12k_err(NULL, "dp / dp hal  invalid - skipping tx mon mode config\n");
		return -EINVAL;
	}

	if (enable) {
		filter.valid = true;
		src_tlv_filter = &filter.filter;
		switch (mode) {
		case DP_MON_TX_FULL_MONITOR:
			ath12k_dp_mon_tx_setup_mon_mode_filter(dp, src_tlv_filter);
			break;
		case DP_MON_TX_FILTER_EXT_MON_MODE:
			ath12k_wifi7_dp_mon_tx_setup_ext_mon_filter(dp_pdev,
								    src_tlv_filter);
			break;
		case DP_MON_TX_FILTER_SPL_PKT_CAP:
			ath12k_wifi7_dp_mon_tx_setup_spl_pkt_cap_filter(dp_pdev,
									src_tlv_filter);
			break;
		default:
			ath12k_err(NULL, "Tx monitor mode invalid - skipping tx mon mode config\n");
			return -EINVAL;
		}
		ath12k_hal_mon_tx_get_wmask_config(dp->hal, &src_tlv_filter->wmask);
		tx_mon->tx_mon_filter[mode][srng_type] = filter;
	} else {
		tx_mon->tx_mon_filter[mode][srng_type] = filter;
	}
	ath12k_dp_mon_tx_display_filters(dp, mode, &filter);
	return 0;
}
EXPORT_SYMBOL(ath12k_wifi7_dp_mon_tx_config_filter);

int
ath12k_wifi7_dp_ext_mon_add_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_ext_mon_tx_peer_params
					 *peer_param)
{
	bool enable_filter_action = false;
	struct ath12k_pdev_tx_mon *tx_mon;
	struct ath12k_set_tx_peer_filter_params wmi_param = {0};
	struct ath12k_dp_ext_mon_peer *peer, *tmp;
	int hw_peer_filter_enabled = 0, ret = 0;
	struct list_head *peer_list;
	u8 filter_mode = 0;

	tx_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	filter_mode = ath12k_wifi7_get_ext_mon_peer_filter_mode_locked(dp_pdev);
	hw_peer_filter_enabled = (filter_mode == ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER);

	if (!hw_peer_filter_enabled)
		return ret;

	peer_list = peer_param->peers_to_wmi;

	wmi_param.vdev_id = (u32)peer_param->vdev_id;
	list_for_each_entry_safe(peer, tmp, peer_list, list) {
		if (!enable_filter_action && peer_param->toggle_hw_state) {
			enable_filter_action = true;
			wmi_param.action =
				WMI_PEER_TX_FILTER_ACTION_ADD_AND_ENABLE_FILTERING;
		} else {
			wmi_param.action = WMI_PEER_TX_FILTER_ACTION_ADD;
		}

		ether_addr_copy(wmi_param.mac_addr, peer->peer_info.mac_addr);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "Peer add attempt for %pM\n", wmi_param.mac_addr);
		if (ath12k_wmi_vdev_set_tx_peer_filter_cmd(dp_pdev->ar,
							   &wmi_param)) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, wmi_peer_send_failed);
			ath12k_warn(dp_pdev->dp,
				    "wmi add fail vdev %d peer addr %pM: mismatch H/W state\n",
				    wmi_param.vdev_id, wmi_param.mac_addr);
			if (wmi_param.action ==
				WMI_PEER_TX_FILTER_ACTION_ADD_AND_ENABLE_FILTERING)
				enable_filter_action = false;
			list_del(&peer->list);
			kfree(peer);
			peer_param->staged_count--;
			ret = -EINVAL;
		}
	}
	return ret;
}

int
ath12k_wifi7_dp_ext_mon_remove_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					    struct ath12k_dp_ext_mon_tx_peer_params
					    *peer_param)
{
	struct ath12k_set_tx_peer_filter_params wmi_param = {0};
	struct ath12k_dp_ext_mon_peer *peer;
	int is_last, ret = 0;
	struct list_head *peer_list;

	/**
	 * Unlike add, remove is sent unconditionally without checking
	 * hw_peer_filter_enabled: a peer may have been staged host-side
	 * only (never pushed to FW) if hw peer filtering wasn't active
	 * when it was added. Removing it is a harmless no-op on the FW
	 * side in that case.
	 */
	peer_list = peer_param->peers_to_wmi;
	wmi_param.vdev_id = (u32)peer_param->vdev_id;
	list_for_each_entry(peer, peer_list, list) {
		is_last = list_is_last(&peer->list, peer_list);
		if (is_last && peer_param->toggle_hw_state)
			wmi_param.action =
				WMI_PEER_TX_FILTER_ACTION_REMOVE_AND_CLEAR_FILTERING;
		else
			wmi_param.action = WMI_PEER_TX_FILTER_ACTION_REMOVE;

		ether_addr_copy(wmi_param.mac_addr, peer->peer_info.mac_addr);
		ath12k_dbg(dp_pdev->dp->ab, ATH12K_DBG_DP_MON_TX,
			   "Peer remove attempt for %pM\n", wmi_param.mac_addr);
		if (ath12k_wmi_vdev_set_tx_peer_filter_cmd(dp_pdev->ar, &wmi_param)) {
			ATH12K_TX_MON_STAT_INC_ERR(dp_pdev, wmi_peer_send_failed);
			ath12k_warn(dp_pdev->dp,
				    "Peer remove fail for %pM: mismatch H/W state.\n",
				    wmi_param.mac_addr);
			ath12k_warn(dp_pdev->dp, "Please retry add and remove.\n");
			ret = -EINVAL;
		}
	}
	return ret;
}

u32
ath12k_wifi7_dp_mon_tx_get_spc_bitmap(struct ath12k_base *ab)
{
	u32 bitmap = DP_TX_MON_SPC_BITMAP(ab);

	if (!bitmap || bitmap > ATH12K_EXT_MON_PKT_CAP_ALL_PROTOS)
		bitmap = ATH12K_EXT_MON_PKT_CAP_ALL_PROTOS;
	return bitmap;
}
