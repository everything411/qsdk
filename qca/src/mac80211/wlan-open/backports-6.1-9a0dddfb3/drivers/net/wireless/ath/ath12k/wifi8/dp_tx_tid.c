// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_tid.h"
#include "dp_pool.h"
#include "../dp_cmn.h"
#include "dp.h"
#include "../mac.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif
#include "../dp_tx.h"

#define ATH12K_FRAME_HEADER_SIZE 24
#define ATH12K_QOS_FRAME_HEADER_SIZE 26

static
u16 ath12k_wifi8_mpduq_sam_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8)
{
	struct ath12k_base *ab = NULL;
	u16 mpduq_sam_id = dp_hw_grp_wifi8->last_mpduq_sam_id;
	int i;

	if (!dp_hw_grp_wifi8->cumac_dp || !dp_hw_grp_wifi8->cumac_dp->ab)
		return HAL_SAM_INVALID_MPDUQ_ID;

	ab = dp_hw_grp_wifi8->cumac_dp->ab;

	spin_lock_bh(&dp_hw_grp_wifi8->sam_id_lock);

	for (i = 0;
	     i < MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED - MAX_NUM_SAM_RESERVED_MPDU_QUEUES;
	     i++) {
		mpduq_sam_id = mpduq_sam_id + 1;

		/* Wrap around to the first non-reserved MPDUQ SAM ID if
		 * the valid range is exceeded.
		 */
		if (mpduq_sam_id >= MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED)
			mpduq_sam_id = MAX_NUM_SAM_RESERVED_MPDU_QUEUES;

		/* SAM supports queue IDs from a single slice range (slize size: 256)
		 * due to v1 hardware limitations.
		 * TODO: Remove after v2 hardware.
		 */
		if (ab->hw_rev == ATH12K_HW_QCN9625_HW10 &&
		    (mpduq_sam_id < HAL_SAM_QUEUE_SLICE_START_IDX ||
		     mpduq_sam_id > HAL_SAM_QUEUE_SLICE_END_IDX))
			continue;

		if (test_bit(mpduq_sam_id, dp_hw_grp_wifi8->mpduq_sam_id_alloc_map))
			continue;

		set_bit(mpduq_sam_id, dp_hw_grp_wifi8->mpduq_sam_id_alloc_map);
		break;
	}

	dp_hw_grp_wifi8->last_mpduq_sam_id = mpduq_sam_id;
	if (i >= MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED - MAX_NUM_SAM_RESERVED_MPDU_QUEUES)
		mpduq_sam_id = HAL_SAM_INVALID_MPDUQ_ID;

	spin_unlock_bh(&dp_hw_grp_wifi8->sam_id_lock);

	return mpduq_sam_id;
}

void ath12k_wifi8_clear_mpduq_sam_id(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr)
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

	if (sw_mpduq_ptr->mpduq_sam_id != HAL_SAM_INVALID_MPDUQ_ID) {
		spin_lock_bh(&dp_hw_grp_wifi8->sam_id_lock);
		clear_bit(sw_mpduq_ptr->mpduq_sam_id,
			  dp_hw_grp_wifi8->mpduq_sam_id_alloc_map);
		spin_unlock_bh(&dp_hw_grp_wifi8->sam_id_lock);

		/* Send sam mpduq clear command for `mpduq_sam_id` to FW.*/
		if (ab && srng) {
			ret = ath12k_wifi8_hal_tx_sam_cmd_send
						(ab, srng, -1,
						 HAL_SAM_MPDU_QUEUE_CLEAR_PROGRAMMING_BO,
						 sw_mpduq_ptr->mpduq_sam_id, false,
						 false);

			if (ret < 0)
				ath12k_warn(ab,
					    "failed to send SAM mpdu clear command for %d: %d\n",
					    sw_mpduq_ptr->mpduq_sam_id, ret);
		}
	}
}

struct ath12k_dp_mpdu_q_info
*ath12k_alloc_peer_tid_mpduq(struct ath12k_dp_hw_group *dp_hw_grp,
			     struct ath12k_dp_peer *peer,
			     u8 tidno, u8 flow_type)
{
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	sw_mpduq_ptr = alloc_memory_pool(dp_hw_grp_wifi8->sw_mpduq_ctxt);
	if (!sw_mpduq_ptr)
		return NULL;

	memset(sw_mpduq_ptr, 0, sizeof(struct ath12k_dp_mpdu_q_info));
	sw_mpduq_ptr->mpduq_id = pool_node_to_id(dp_hw_grp_wifi8->sw_mpduq_ctxt,
						 sw_mpduq_ptr);
	sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_INVALID;
	sw_mpduq_ptr->flow_info.tid_num = tidno;
	sw_mpduq_ptr->flow_info.flow_type = HTT_TID_MPDUQ_TYPE;
	sw_mpduq_ptr->flow_info.peer_id = peer->peer_id;
	sw_mpduq_ptr->pn_addr = ath12k_dp_get_page_paddr(dp_hw_grp, peer->peer_id);
	/* Skip MPDU SAM ID allocation for mcast, mgmt and hol queues, mark as invalid.*/
	if (flow_type != HTT_TID_MSDUQ_UDP)
		sw_mpduq_ptr->mpduq_sam_id = HAL_SAM_INVALID_MPDUQ_ID;
	else
		sw_mpduq_ptr->mpduq_sam_id =
			ath12k_wifi8_mpduq_sam_id_alloc(dp_hw_grp_wifi8);

	pool_addr_from_id(dp_hw_grp_wifi8->mpduq_ctxt, sw_mpduq_ptr->mpduq_id,
			  &sw_mpduq_ptr->mpdu_q_vaddr, &sw_mpduq_ptr->mpdu_q_paddr);

	return sw_mpduq_ptr;
}

static inline
bool ath12k_tx_get_he_mac_cap(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_link_peer *link_peer)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_link_sta *arsta;
	struct ieee80211_link_sta *link_sta;
	const struct ieee80211_sta_he_cap *he_cap;
	bool ht_he_cap = false;

	arsta = ath12k_peer_get_link_sta(ab, link_peer);
	if (!arsta || !arsta->arvif) {
		ath12k_warn(ab, "arsta or arvif is NULL");
		return false;
	}
	link_sta = ath12k_mac_get_link_sta(arsta);
	if (!link_sta) {
		ath12k_warn(ab, "link sta is NULL");
		return false;
	}
	he_cap = &link_sta->he_cap;
	if (!he_cap) {
		ath12k_warn(ab, "he_cap is NULL");
		return false;
	}
	if ((he_cap->he_cap_elem.mac_cap_info[0] & 0x1u) != 0)
		ht_he_cap = true;

	return ht_he_cap;
}

static inline
u32 ath12k_wifi8_dp_tx_get_he_header_length(struct ath12k_dp_hw_group *dp_hw_grp,
					    struct ath12k_dp_peer *peer, u8 tid_num)
{
	bool ht_he_cap;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_link_sta *arsta;
	int i;
	u32 header_size = 0;
	u32 rep_ul_resp;
	struct ath12k *ar;

	if (!peer || !ath12k_dp_peer_get_sta(peer))
		return 0;

	if (tid_num >= NON_QOS_TID)
		return 0;

	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, i);
		if (link_peer)
			break;
	}

	arsta = ath12k_peer_get_link_sta(ab, link_peer);
	if (!arsta || !arsta->arvif || !arsta->arvif->ahvif ||
	    !arsta->arvif->ahvif->vif) {
		rcu_read_unlock();
		return 0;
	}

	ht_he_cap = ath12k_tx_get_he_mac_cap(dp_hw_grp, link_peer);
	ar = arsta->arvif->ar;
	if (!ar) {
		rcu_read_unlock();
		return 0;
	}
#ifdef CPTCFG_QCN_EXTN
	rep_ul_resp = ((ath12k_cfg_get(ab, ATH12K_CFG_REP_UL_RESP) >>
			ar->pdev->pdev_id) & 01);
#else
	rep_ul_resp = 0;
#endif /* CPTCFG_QCN_EXTN */
	//do we need STA check if we are checking sta_bss_peer
	if (arsta->arvif->ahvif->vif->type == NL80211_IFTYPE_STATION &&
	    peer->is_sta_bss_peer && ath12k_dp_peer_get_sta(peer)->wme &&
	    rep_ul_resp &&
	    ht_he_cap)
			header_size = 4;

	rcu_read_unlock();
	return header_size;
}

static inline
u32 ath12k_wifi8_dp_tx_get_header_length(struct ath12k_dp_hw_group *dp_hw_grp,
					 struct ath12k_dp_peer *peer,
					 struct hal_tx_mpdu_queue_head_info *ti,
					 u8 tid_num)
{
	u32 header_len = 0;

	if (ti->encap_type == ATH12K_HW_TXRX_RAW || ti->is_mgmtq) {
		header_len = 0;
		return header_len;
	}

	/* Set header length based on QoS vs non-QoS data TID */
	if (ti->tid > MAX_VALID_DATA_TID)
		header_len += ATH12K_FRAME_HEADER_SIZE;
	else
		header_len += ATH12K_QOS_FRAME_HEADER_SIZE;

	if (peer->is_sta_bss_peer_4addr ||
	    (peer->is_11s_mesh_peer && !peer->is_vdev_peer))
		header_len += ETH_ALEN;

	header_len += ath12k_wifi8_dp_tx_get_he_header_length(dp_hw_grp,
							      peer, tid_num);
	if (peer->is_epp_peer)
		header_len += ath12k_dp_tx_get_mpdu_encap_hdr_len(peer->sec_type);
	return header_len;
}

int ath12k_tx_send_mpduq_init(struct ath12k_dp_hw_group *dp_hw_grp,
			      struct ath12k_dp_peer *peer,
			      enum hal_tcl_encap_type tx_encap_type,
			      struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr)
{
	u8 tid_num = sw_mpduq_ptr->flow_info.tid_num;
	struct hal_tx_mpdu_queue_head_info ti = {0};
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id;
	int i;

	ti.paddr = sw_mpduq_ptr->mpdu_q_paddr;
	ti.queue_number = (sw_mpduq_ptr->queue_number & 0xFFFFFF);
	ti.peer_id = peer->peer_id;
	/*
	 * When VoW stats are disabled, program the peer-level stats_id.
	 * When VoW stats are enabled, program the per-TID stats_id so
	 * HW telemetry ring delivers isolated per-TID descriptors for
	 * this MPDUQ.
	 */
	if (tid_num < ATH12K_DATA_TID_MAX &&
	    peer->tid_stats_id[tid_num] < ATH12K_MAX_STATS_ID)
		ti.stats_id = peer->tid_stats_id[tid_num];
	else
		ti.stats_id = peer->stats_id;
	if (tid_num ==  MLO_MGMT_TID) {
		ti.is_mgmtq = true;
		ti.tid = TQM_NON_DATA_TID;
	} else {
		/*
		 *  NON_QOS_TID is enum 16 but MPDU HW struct has
		 *   only 4 bits tid; hence setting to 15 (TQM_NON_DATA_TID)
		 */
		ti.tid = (tid_num < NON_QOS_TID) ? tid_num : TQM_NON_DATA_TID;
	}
	ti.encap_type = tx_encap_type;
	if (peer->is_vdev_peer)
		ti.assoc_link_id = peer->hw_link_id;
	else
		ti.assoc_link_id = peer->assoc_hw_link_id;

	ti.link_id1 = ATH12K_INVALID_LINK_ID - 1;
	ti.link_id2 = ATH12K_INVALID_LINK_ID;

	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(peer, i);
		if (!link_peer)
			continue;

		hw_link_id = link_peer->hw_link_id;
		if (hw_link_id == ti.assoc_link_id)
			continue;
		if (ti.link_id1 == (ATH12K_INVALID_LINK_ID - 1)) {
			ti.link_id1 = hw_link_id;
			continue;
		}
		if (ti.link_id2 == ATH12K_INVALID_LINK_ID) {
			ti.link_id2 = hw_link_id;
			break;
		}
	}
	rcu_read_unlock();

	ti.mlo = peer->is_mlo;
	ti.pn_dma_addr = sw_mpduq_ptr->pn_addr;
	ti.header_len = ath12k_wifi8_dp_tx_get_header_length(dp_hw_grp, peer,
							     &ti, tid_num);
	ti.mpduq_sam_id = sw_mpduq_ptr->mpduq_sam_id;

	return ath12k_wifi8_hal_tx_mpdu_queue_setup(dp_hw_grp,
						    sw_mpduq_ptr->mpduq_id,
						    &ti);
}

struct ath12k_dp_tx_tid_info *ath12k_dp_get_tid(struct ath12k_dp_peer *peer, u8 tidno)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info =
			ath12k_dp_get_tx_flow_info_from_peer(peer);
	struct ath12k_dp_tx_tid_info *tid = NULL;

	rcu_read_lock();
	if (tidno < ATH12K_MAX_NUM_DATA_TIDS)
		tid = &tx_flow_info->tid_info[tidno];
	else if (tidno == NON_QOS_TID)
		tid = &tx_flow_info->tid_info[DEFAULT_TID];
	rcu_read_unlock();
	return tid;
}

struct ath12k_dp_mpdu_q_info *ath12k_peer_alloc_tid(struct ath12k_dp_hw_group *dp_hw_grp,
						    struct ath12k_dp_peer *peer,
						    enum hal_tcl_encap_type encap_type,
						    u8 tidno,
						    struct ath12k_dp_tx_tid_info **ptid,
						    u8 flow_type)
{
	struct ath12k_dp_tx_tid_info *tid = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	int ret;

	if ((tidno != NON_QOS_TID && tidno >= ATH12K_MAX_NUM_DATA_TIDS) ||
	    flow_type == HTT_TID_MSDUQ_MCAST) {
		*ptid = NULL;
		goto create_mpdu;
	}

	*ptid = tid = ath12k_dp_get_tid(peer, tidno);
	if (tid && tid->tid_num == tidno && tid->mpduq)
		return tid->mpduq;

	memset(tid, 0, sizeof(struct ath12k_dp_tx_tid_info));
	tid->tid_num = tidno;
	tid->peer_id = peer->peer_id;

create_mpdu:
	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	sw_mpduq_ptr = ath12k_alloc_peer_tid_mpduq(dp_hw_grp, peer, tidno, flow_type);
	if (!sw_mpduq_ptr) {
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		goto error;
	}
	ret = ath12k_tx_send_mpduq_init(dp_hw_grp, peer, encap_type, sw_mpduq_ptr);
	if (ret) {
		ath12k_err(ab, "HAL MPDUQ INIT FAILED\n");
		sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_DELETED;
		ath12k_wifi8_clear_mpduq_sam_id(dp_hw_grp_wifi8, sw_mpduq_ptr);
		free_memory_pool(dp_hw_grp_wifi8->sw_mpduq_ctxt, sw_mpduq_ptr);
		spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
		goto error;
	}
	sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_CREATED;
	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);

	return sw_mpduq_ptr;

error:
	*ptid = NULL;
	return NULL;
}

//In the caller API, make sure tid.num_of_active_msdu_queues is zero if tid exists
void ath12k_peer_free_tid(struct ath12k_dp_hw_group *dp_hw_grp,
			  struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
			  struct ath12k_dp_tx_tid_info *ptid)
{
	struct hal_tx_mpdu_queue_head *mpduq;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!sw_mpduq_ptr)
		goto tid_free;

	spin_lock_bh(&dp_hw_grp_wifi8->tx_pool_lock);
	mpduq = pool_node_from_id(dp_hw_grp_wifi8->mpduq_ctxt, sw_mpduq_ptr->mpduq_id);
	ath12k_wifi8_hal_mpduq_set_invalid(dp_hw_grp, mpduq, sw_mpduq_ptr->mpdu_q_paddr);
	sw_mpduq_ptr->mpduq_state = ATH12K_TX_Q_DELETED;
	ath12k_wifi8_clear_mpduq_sam_id(dp_hw_grp_wifi8, sw_mpduq_ptr);
	free_memory_pool(dp_hw_grp_wifi8->sw_mpduq_ctxt, sw_mpduq_ptr);
	spin_unlock_bh(&dp_hw_grp_wifi8->tx_pool_lock);

tid_free:
	if (!ptid)
		return;

	ptid->tid_num = ATH12K_INVALID_TID;
	ptid->peer_id = ATH12K_PEER_ID_INVALID;
}
