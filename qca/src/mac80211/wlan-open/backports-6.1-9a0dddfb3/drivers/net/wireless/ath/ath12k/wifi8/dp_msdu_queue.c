// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_msdu_queue.h"
#include "dp_pool.h"
#include "dp.h"
#include "dp_tx_tid.h"

static
u16 ath12k_wifi8_msduq_sam_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8)
{
	struct ath12k_base *ab = NULL;
	u16 msduq_sam_id = dp_hw_grp_wifi8->last_msduq_sam_id;
	int i;

	if (!dp_hw_grp_wifi8->cumac_dp || !dp_hw_grp_wifi8->cumac_dp->ab)
		return HAL_SAM_INVALID_MSDUQ_ID;

	ab = dp_hw_grp_wifi8->cumac_dp->ab;

	spin_lock_bh(&dp_hw_grp_wifi8->sam_id_lock);

	for (i = 0;
	     i < MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED - MAX_NUM_SAM_RESERVED_MSDU_QUEUES;
	     i++) {
		msduq_sam_id = msduq_sam_id + 1;

		/* Wrap around to the first non-reserved MSDUQ SAM ID if
		 * the valid range is exceeded.
		 */
		if (msduq_sam_id >= MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED)
			msduq_sam_id = MAX_NUM_SAM_RESERVED_MSDU_QUEUES;

		/* SAM supports queue IDs from a single slice range (slize size: 256)
		 * due to v1 hardware limitations.
		 * TODO: Remove after v2 hardware.
		 */
		if (ab->hw_rev == ATH12K_HW_QCN9625_HW10 &&
		    (msduq_sam_id < HAL_SAM_QUEUE_SLICE_START_IDX ||
		     msduq_sam_id > HAL_SAM_QUEUE_SLICE_END_IDX))
			continue;

		if (test_bit(msduq_sam_id, dp_hw_grp_wifi8->msduq_sam_id_alloc_map))
			continue;

		set_bit(msduq_sam_id, dp_hw_grp_wifi8->msduq_sam_id_alloc_map);
		break;
	}

	dp_hw_grp_wifi8->last_msduq_sam_id = msduq_sam_id;
	if (i >= MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED - MAX_NUM_SAM_RESERVED_MSDU_QUEUES)
		msduq_sam_id = HAL_SAM_INVALID_MSDUQ_ID;

	spin_unlock_bh(&dp_hw_grp_wifi8->sam_id_lock);

	return msduq_sam_id;
}

void ath12k_wifi8_clear_msduq_sam_id(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     struct ath12k_dp_msdu_q_info *sw_msduq_ptr)
{
	struct ath12k_base *ab = NULL;
	struct ath12k_dp_wifi8 *dp_wifi8 = NULL;
	struct hal_srng *srng = NULL;
	int ret;

	if (!dp_hw_grp_wifi8)
		return;

	if (dp_hw_grp_wifi8->cumac_dp && dp_hw_grp_wifi8->cumac_dp->ab) {
		ab = dp_hw_grp_wifi8->cumac_dp->ab;
		dp_wifi8 = ath12k_get_dp_wifi8(dp_hw_grp_wifi8->cumac_dp);
		srng = &ab->hal.srng_list[dp_wifi8->sam_cmd_ring.ring_id];
	}

	if (sw_msduq_ptr->msduq_sam_id != HAL_SAM_INVALID_MSDUQ_ID) {
		spin_lock_bh(&dp_hw_grp_wifi8->sam_id_lock);
		clear_bit(sw_msduq_ptr->msduq_sam_id,
			  dp_hw_grp_wifi8->msduq_sam_id_alloc_map);
		spin_unlock_bh(&dp_hw_grp_wifi8->sam_id_lock);

		/* Send sam msduq clear command for `msduq_sam_id` to FW.*/
		if (ab && srng) {
			ret = ath12k_wifi8_hal_tx_sam_cmd_send
						(ab, srng, -1,
						 HAL_SAM_MSDU_QUEUE_CLEAR_PROGRAMMING_BO,
						 sw_msduq_ptr->msduq_sam_id, false,
						 false);

			if (ret < 0)
				ath12k_warn(ab,
					    "failed to send SAM msdu clear command for %d: %d\n",
					    sw_msduq_ptr->msduq_sam_id, ret);
		}
	}
}

struct ath12k_dp_msdu_q_info
*ath12k_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			    struct ath12k_dp_peer *peer)
{
	u32 sw_msduq_idx;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	sw_msduq_ptr = alloc_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt);
	if (!sw_msduq_ptr) {
		ath12k_warn(ab, "no memory available for SW MSDUQ\n");
		return NULL;
	}
	sw_msduq_idx = pool_node_to_id(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
	if (sw_msduq_idx >= NUM_TOTAL_MSDU_QUEUES) {
		ath12k_warn(ab, "sw_msduq_idx %d out of allocated msduq indexes\n",
			    sw_msduq_idx);
		free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
		return NULL;
	}

	memset(sw_msduq_ptr, 0, sizeof(struct ath12k_dp_msdu_q_info));

	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_INVALID;
	sw_msduq_ptr->msduq_idx = sw_msduq_idx;
	sw_msduq_ptr->mlo = peer->is_mlo;
	sw_msduq_ptr->flow_info.peer_id = peer->peer_id;
	sw_msduq_ptr->allocated = 1;
	sw_msduq_ptr->svc_id = ATH12K_INVALID_SVC_ID;
	sw_msduq_ptr->svc = HAL_TQM_SERVICE_CATEGORY_MAX;

	return sw_msduq_ptr;
}

static enum wmi_phy_mode
ath12k_wifi8_dp_get_phymode(struct ath12k_dp_hw_group *dp_hw_grp,
			    struct ath12k_dp_peer *peer)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	unsigned long peer_links_map, scan_links_map;
	struct ath12k_dp_link_vif *dp_link_vif;
	enum wmi_phy_mode phymode = MODE_11A;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_link_sta *arsta;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	u8 link_id;

	if (peer->is_vdev_peer) {
		link_id = peer->hw_links[peer->hw_link_id];

		vif = ath12k_dp_peer_get_vif(peer);
		if (vif) {
			ahvif = ath12k_vif_to_ahvif(vif);
			dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
			if (dp_link_vif)
				phymode = dp_link_vif->phymode;
		}
	} else {
		peer_links_map = peer->peer_links_map;
		scan_links_map = ATH12K_SCAN_LINKS_MASK;

		rcu_read_lock();
		for_each_andnot_bit(link_id, &peer_links_map,
				    &scan_links_map,
				    ATH12K_NUM_MAX_LINKS) {
			link_peer = rcu_dereference(peer->link_peers[link_id]);
			if (!link_peer)
				continue;

			arsta = ath12k_peer_get_link_sta(ab, link_peer);
			if (!arsta) {
				ath12k_warn(ab, "Not able to found arsta for the peer %pM with link id %d\n",
					    peer->addr, link_peer->link_id);
				continue;
			}

			phymode = max(phymode, arsta->phymode);
		}
		rcu_read_unlock();
	}

	return phymode;
}

int ath12k_init_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      struct ath12k_dp_msdu_q_info *sw_msduq_ptr)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct hal_tx_msdu_flow_info ti = {0};
	u8 tid_num = sw_msduq_ptr->flow_info.tid_num;
	enum wmi_phy_mode phymode;
	enum wme_ac ac;

	ti.paddr = sw_msduq_ptr->msdu_q_paddr;
	ti.queue_number = (sw_msduq_ptr->queue_number & 0xFFFFFF);
	ti.peer_id = peer->peer_id;
	ti.bitmap = sw_msduq_ptr->bitmap;
	ti.msduq_sam_id = sw_msduq_ptr->msduq_sam_id;

	/*
	 * When VoW stats are disabled, program the peer-level stats_id.
	 * When VoW stats are enabled, program the per-TID stats_id so
	 * HW telemetry ring delivers isolated per-TID descriptors for
	 * this MSDUQ.
	 */
	if (tid_num < ATH12K_DATA_TID_MAX &&
	    peer->tid_stats_id[tid_num] < ATH12K_MAX_STATS_ID)
		ti.stats_id = peer->tid_stats_id[tid_num];
	else
		ti.stats_id = peer->stats_id;
	ti.tid = ath12k_dp_tx_get_tid(tid_num);

	if (tid_num == MLO_MGMT_TID) {
		ti.is_mgmtq = true;
		ti.svc = HAL_TQM_SERVICE_CATEGORY_MAX;
	} else {
		phymode = ath12k_wifi8_dp_get_phymode(dp_hw_grp, peer);

		/* Map the sevice category */
		ac = ath12k_tid_to_ac(ti.tid > ATH12K_DSCP_PRIORITY ? 0 : ti.tid);
		switch (ac) {
		case WME_AC_BE:
			fallthrough;
		case WME_AC_BK:
			if (phymode >= MODE_11AC_VHT20 && phymode <= MODE_11BN_UHR40_2G)
				ti.svc = HAL_TQM_SERVICE_CATEGORY_SC2;
			else
				ti.svc = HAL_TQM_SERVICE_CATEGORY_SC3;
			break;
		case WME_AC_VI:
			fallthrough;
		case WME_AC_VO:
			if (phymode >= MODE_11AC_VHT20 && phymode <= MODE_11BN_UHR40_2G)
				ti.svc = HAL_TQM_SERVICE_CATEGORY_SC0;
			else
				ti.svc = HAL_TQM_SERVICE_CATEGORY_SC1;
			break;
		default:
			ath12k_warn(ab, "invalid AC %d for the tid %d peer %pM\n",
				    ac, ti.tid, peer->addr);
			ti.svc = HAL_TQM_SERVICE_CATEGORY_MAX;
			break;
		}
	}
	if (peer->is_mlo)
		ti.mlo = 1;

	sw_msduq_ptr->svc = ti.svc;

	ath12k_dbg(ab, ATH12K_DBG_DP_TX, "Peer %pM tid %d svc %d flow 0x%x MSDUQ setup\n",
		   peer->addr, ti.tid, ti.svc, ti.queue_number);

	return ath12k_wifi8_hal_tx_msdu_queue_setup(dp_hw_grp,
						    sw_msduq_ptr->msduq_idx,
						    &ti);
}

u8 ath12k_dp_get_chipid_bitmap(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_peer *dp_peer)
{
	int i;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id;
	struct ath12k_dp *partner_dp;
	u8 bitmap = 0;

	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
		if (!link_peer)
			continue;

		hw_link_id = link_peer->hw_link_id;
		partner_dp =
		ath12k_dp_hw_grp_to_dp(dp_hw_grp,
				       dp_hw_grp->hw_links[hw_link_id].device_id);
		if (!partner_dp)
			continue;

		/* HW10 implicitly routes add-msdu status to the cumac chip,
		 * so skip programming it explicitly. HW20 fixed this and
		 * requires the cumac chip id to be set explicitly too.
		 */
		if (partner_dp == ath12k_get_central_dp(partner_dp) &&
		    partner_dp->ab->hw_rev != ATH12K_HW_QCN9625_HW20)
			continue;

		bitmap |= BIT(partner_dp->device_id);
	}
	rcu_read_unlock();
	return bitmap;
}

struct ath12k_dp_msdu_q_info
*ath12k_init_alloc_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *peer,
				 u8 tid_num, u8 msduq_type,
				 u8 mgmt_msduq_type)
{
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u8 is_qos = 0;
	int ret;

	if (msduq_type >= HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_0 &&
	    msduq_type <= HTT_TID_MSDUQ_MGMT_LINK_AGNOSTIC &&
	    mgmt_msduq_type >= MGMT_MSDUQ_TYPE_MAX)
		return NULL;

	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_msduq_ptr = ath12k_alloc_tx_msdu_flowq(dp_hw_grp, peer);
	if (!sw_msduq_ptr) {
		ath12k_warn(ab, "SW MSDUQ allocation failed\n");
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		return NULL;
	}

	is_qos = (tid_num == NON_QOS_TID) ? 0 : 1;

	sw_msduq_ptr->qos = is_qos;
	sw_msduq_ptr->flow_info.tid_num = tid_num;
	sw_msduq_ptr->flow_info.flow_type = msduq_type;
	sw_msduq_ptr->bitmap = ath12k_dp_get_chipid_bitmap(dp_hw_grp, peer);
	/* Skip MSDU SAM ID allocation for mcast, mgmt and hol queues, mark as invalid.*/
	if (msduq_type != HTT_TID_MSDUQ_NONUDP && msduq_type != HTT_TID_MSDUQ_UDP)
		sw_msduq_ptr->msduq_sam_id = HAL_SAM_INVALID_MSDUQ_ID;
	else
		sw_msduq_ptr->msduq_sam_id =
			ath12k_wifi8_msduq_sam_id_alloc(dp_hw_grp_wifi8);

	pool_addr_from_id(dp_hw_grp_wifi8->msduq_ctxt, sw_msduq_ptr->msduq_idx,
			  &sw_msduq_ptr->msdu_q_vaddr,
			  &sw_msduq_ptr->msdu_q_paddr);

	ret = ath12k_init_tx_msdu_flowq(dp_hw_grp, peer, sw_msduq_ptr);
	if (ret) {
		ath12k_warn(ab, "HAL MSDUQ write failed\n");
		sw_msduq_ptr->msduq_state = ATH12K_TX_Q_DELETED;
		ath12k_wifi8_clear_msduq_sam_id(dp_hw_grp_wifi8, sw_msduq_ptr);
		free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		return NULL;
	}

	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_CREATED;

	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);

	return sw_msduq_ptr;
}

void ath12k_free_tx_msdu_flowq(struct ath12k_dp_hw_group *dp_hw_grp,
			       struct ath12k_dp_msdu_q_info *sw_msduq_ptr)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
				ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_wifi8_dp_congestion_control *congstn =
				&dp_hw_grp_wifi8->congstn;
	struct hal_tx_msdu_flow *msduq;

	if (!sw_msduq_ptr)
		return;

	/* Remove from per-service-category threshold_list if tracked */
	if (sw_msduq_ptr->in_threshold_list) {
		spin_lock_bh(&congstn->threshold_list_lock);
		list_del(&sw_msduq_ptr->threshold_node);
		sw_msduq_ptr->in_threshold_list = false;
		spin_unlock_bh(&congstn->threshold_list_lock);
	}

	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_msduq_ptr->allocated = 0;
	msduq = pool_node_from_id(dp_hw_grp_wifi8->msduq_ctxt, sw_msduq_ptr->msduq_idx);
	ath12k_wifi8_hal_msduq_set_invalid(dp_hw_grp, msduq, sw_msduq_ptr->msdu_q_paddr);
	sw_msduq_ptr->msduq_state = ATH12K_TX_Q_DELETED;
	ath12k_wifi8_clear_msduq_sam_id(dp_hw_grp_wifi8, sw_msduq_ptr);
	free_memory_pool(dp_hw_grp_wifi8->sw_msduq_ctxt, sw_msduq_ptr);
	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
}
