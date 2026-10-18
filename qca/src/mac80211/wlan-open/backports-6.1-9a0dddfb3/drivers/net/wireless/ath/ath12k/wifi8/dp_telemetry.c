// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"
#include "dp.h"
#include "hal.h"
#include "dp_telemetry.h"

static struct ath12k_dp_peer *
ath12k_wifi8_dp_telemetry_peer_find(struct ath12k_dp_hw_group *dp_hw_grp,
				    u16 dp_peer_id, u8 hw_link_id)
{
	struct ath12k_pdev_dp *dp_pdev;

	if (hw_link_id >= ATH12K_GROUP_MAX_RADIO)
		return NULL;

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
	if (!dp_pdev)
		return NULL;

	return ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
						   dp_peer_id);
}

int ath12k_wifi8_dp_telemetry_ring_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	int ret;

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->tx_peer_telemetry_ring,
				   HAL_PEER_TX_TELEMETRY, 0, 0,
				   DP_TELEMETRY_TX_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "Peer Tx Telemetry ring alloc failed: %d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->rx_peer_telemetry_ring,
				   HAL_PEER_RX_TELEMETRY, 0, 0,
				   DP_TELEMETRY_RX_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "Peer Rx Telemetry ring alloc failed: %d\n", ret);
		goto err;
	}

	return 0;
err:
	ath12k_wifi8_dp_telemetry_ring_cleanup(ab);
	return ret;
}

int ath12k_wifi8_dp_telemetry_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	int ret;

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->tx_peer_telemetry_ring,
				  HAL_PEER_TX_TELEMETRY, 0, 0);
	if (ret) {
		ath12k_warn(ab, "Peer Tx Telemetry ring init failed: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->rx_peer_telemetry_ring,
				  HAL_PEER_RX_TELEMETRY, 0, 0);
	if (ret) {
		ath12k_warn(ab, "Peer Rx Telemetry ring init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int ath12k_wifi8_dp_telemetry_ring_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8->tx_peer_telemetry_ring);
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_peer_telemetry_ring);

	return 0;
}

/**
 * ath12k_wifi8_dp_update_tx_link_telemetry() - Update per-band TX link stats
 * @dp_peer: pointer to the MLD DP peer
 * @link_peer: pointer to the per-link DP peer
 * @link_id: MLO link identifier used to index into dp_peer->stats[]
 * @band: pointer to the per-band telemetry descriptor
 *
 * Accumulates TX success, retransmission, RSSI, PPDU count and PHY rate
 * stats from one band entry of a TX peer telemetry descriptor into the
 * corresponding per-link hw_tx stats and dp_peer->stats[link_id].tx[0].
 */
static void
ath12k_wifi8_dp_update_tx_link_telemetry(struct ath12k_dp_peer *dp_peer,
					 struct ath12k_dp_link_peer *link_peer,
					 int link_id,
					 const struct tx_peer_band_telemetry *band,
					 u64 drop_bytes, u32 drop1_pkts)
{
	struct ath12k_dp_link_peer_hw_tx_stats *hw_link_tx;
	struct ath12k_dp_peer_tx_stats *tx;
	struct ath12k_dp_peer_tx_dbg_stats *tx_dbg;
	u32 success_packets;
	u64 success_bytes;

	if (!link_peer->peer_stats.hw_link_stats)
		return;

	/* Use Ring idx 0 to store the stats update from HW desc */
	tx = &dp_peer->stats[link_id].tx[0];
	tx_dbg = &dp_peer->stats[link_id].tx_dbg;
	hw_link_tx = &link_peer->peer_stats.hw_link_stats->hw_link_tx;

	tx_dbg->total_msdu_retries +=
		le32_get_bits(band->info2,
			      TX_PEER_BAND_TELEMETRY_STATS_INFO2_NUM_RETRANSMISSIONS);

	if (!dp_peer->is_vdev_peer) {
		success_bytes =
			(u64)le32_to_cpu(band->info0) |
			((u64)le32_get_bits(band->info1,
			 TX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES) << 32);

		success_packets =
			le32_get_bits(band->info1,
			      TX_PEER_BAND_TELEMETRY_STATS_INFO1_NUM_SUCCESS_PACKETS);

		tx->tx_success.bytes += success_bytes;
		tx->tx_success.packets += success_packets;

		/* Update comp_pkt on actual transmission link */
		tx->comp_pkt.packets += success_packets;
		tx->comp_pkt.bytes   += success_bytes;

		if (success_packets)
			WRITE_ONCE(link_peer->peer_stats.last_ack, jiffies);

		tx_dbg->ucast.bytes +=
			(u64)le32_to_cpu(band->info0) |
			((u64)le32_get_bits(band->info1,
			 TX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES) << 32);

		tx_dbg->ucast.packets +=
		le32_get_bits(band->info1,
			      TX_PEER_BAND_TELEMETRY_STATS_INFO1_NUM_SUCCESS_PACKETS);
	} else {
		tx->tx_success.bytes += drop_bytes;
		tx->tx_success.packets += drop1_pkts;
		tx->comp_pkt.bytes   += drop_bytes;
		tx->comp_pkt.packets += drop1_pkts;
	}

	hw_link_tx->sum_ack_rssi +=
		(u64)le32_to_cpu(band->info4) |
		((u64)le32_get_bits(band->info5,
		TX_PEER_BAND_TELEMETRY_STATS_INFO5_UPPER_SUM_ACK_RSSI) << 32);

	hw_link_tx->acked_ppdu_count +=
		le32_get_bits(band->info5,
			      TX_PEER_BAND_TELEMETRY_STATS_INFO5_PPDU_COUNT);

	hw_link_tx->sum_phy_rate +=
		(u64)le32_to_cpu(band->info6) |
		((u64)le32_get_bits(band->info5,
		TX_PEER_BAND_TELEMETRY_STATS_INFO5_UPPER_SUM_PHY_RATES) << 32);
}

/**
 * ath12k_wifi8_dp_update_tx_peer_telemetry() - Update TX peer telemetry stats
 * @dp_peer: pointer to the DP peer
 * @tx_desc: pointer to the TX peer telemetry descriptor
 *
 * Updates peer TX completion/fail/drop/retry counters from the descriptor
 * into hw_tx, then iterates over all bands and calls
 * ath12k_wifi8_dp_update_tx_link_telemetry() for each band's link stats.
 */
static void
ath12k_wifi8_dp_update_tx_peer_telemetry(struct ath12k_dp_peer *dp_peer,
					 const struct tx_peer_telemetry_desc *tx_desc)
{
	struct ath12k_dp_peer_hw_tx_stats *hw_tx;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer_tx_stats *tx;
	struct ath12k_dp_peer_tx_dbg_stats *tx_dbg;
	int link_id, primary_link_id = -1;
	u32 failed_pkts, retried_pkts, drop1_pkts;
	u64 drop_bytes;
	u8 band_id;

	if (!dp_peer->mld_stats.hw_stats)
		return;

	hw_tx = &dp_peer->mld_stats.hw_stats->hw_tx;

	/* HW Peer Tx stats */
	hw_tx->failed_bytes +=
		(u64)le32_to_cpu(tx_desc->lower_fail_bytes) |
		((u64)le32_get_bits(tx_desc->info1,
		TX_PEER_TELEMETRY_DESC_INFO1_UPPER_FAIL_BYTES) << 32);

	drop_bytes =
		(u64)le32_to_cpu(tx_desc->lower_drop_bytes) |
		((u64)le32_get_bits(tx_desc->info1,
		TX_PEER_TELEMETRY_DESC_INFO1_UPPER_DROP_BYTES) << 32);


	drop1_pkts =
		le32_get_bits(tx_desc->info3,
			      TX_PEER_TELEMETRY_DESC_INFO3_NUM_DROPPED1_PACKETS);

	if (!dp_peer->is_vdev_peer) {
		hw_tx->drop_bytes += drop_bytes;
		hw_tx->drop1_pkts += drop1_pkts;
	}

	hw_tx->drop2_pkts +=
		le32_get_bits(tx_desc->info4,
			      TX_PEER_TELEMETRY_DESC_INFO4_NUM_DROPPED2_PACKETS);

	failed_pkts =
		le32_get_bits(tx_desc->info2,
			      TX_PEER_TELEMETRY_DESC_INFO2_NUM_FAIL_PACKETS);

	retried_pkts =
		le32_get_bits(tx_desc->info5,
			      TX_PEER_TELEMETRY_DESC_INFO5_NUM_RETRIED_PACKETS);

	/* Per-band/link Tx stats */
	for (link_id = 0; link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; link_id++) {

		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_id);
		if (!link_peer)
			continue;

		/* For MLO peers: primary_link flag overrides any earlier assignment.
		 * For non-MLO/legacy peers: first valid link is used.
		 */
		if (primary_link_id < 0 || link_peer->primary_link)
			primary_link_id = link_id;

		band_id = link_peer->hw_link_id;
		if (band_id >= MAX_TX_PEER_BAND)
			continue;

		ath12k_wifi8_dp_update_tx_link_telemetry(dp_peer, link_peer,
							 link_id,
							 &tx_desc->peer_band[band_id],
							 drop_bytes, drop1_pkts);
	}

	if (primary_link_id < 0)
		return;

	/* Store MLD-level stats into primary link's stats[primary_link_id].tx[0] */
	tx = &dp_peer->stats[primary_link_id].tx[0];
	tx_dbg = &dp_peer->stats[primary_link_id].tx_dbg;
	tx->comp_pkt.packets += failed_pkts;
	tx->comp_pkt.bytes   += (u64)le32_to_cpu(tx_desc->lower_fail_bytes) |
				((u64)le32_get_bits(tx_desc->info1,
					TX_PEER_TELEMETRY_DESC_INFO1_UPPER_FAIL_BYTES) << 32);
	tx->tx_failed += failed_pkts;
	tx_dbg->retry_count += retried_pkts;
}

/**
 * ath12k_wifi8_dp_update_rx_link_telemetry() - Update per-band RX link stats
 * @dp_peer: pointer to the MLD DP peer
 * @link_peer: pointer to the per-link DP peer
 * @link_id: MLO link identifier used to index into dp_peer->stats[]
 * @band: pointer to the per-band telemetry descriptor
 *
 * Accumulates RX success, fail, drop, RSSI, PPDU count and PHY rate stats
 * from one band entry of an RX peer telemetry descriptor into the
 * corresponding per-link hw_rx stats and dp_peer->stats[link_id].rx[0].
 */
static void
ath12k_wifi8_dp_update_rx_link_telemetry(struct ath12k_dp_peer *dp_peer,
					 struct ath12k_dp_link_peer *link_peer,
					 int link_id,
					 const struct rx_peer_band_telemetry *band)
{
	struct ath12k_dp_peer_rx_stats *rx;
	struct ath12k_dp_link_peer_hw_rx_stats *hw_link_rx;
	struct ath12k_dp_peer_rx_dbg_stats *rx_dbg;

	if (!link_peer->peer_stats.hw_link_stats)
		return;

	rx = &dp_peer->stats[link_id].rx[0];
	rx_dbg = &dp_peer->stats[link_id].rx_dbg;
	hw_link_rx = &link_peer->peer_stats.hw_link_stats->hw_link_rx;

	if (!dp_peer->is_vdev_peer) {
		rx_dbg->ucast.bytes +=
		(u64)le32_to_cpu(band->info0) |
		((u64)le32_get_bits(band->info1,
		RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES) << 32);

		rx_dbg->ucast.packets +=
		le32_get_bits(band->info4,
		RX_PEER_BAND_TELEMETRY_STATS_INFO4_NUM_SUCES_FST_TRY_UCAST_PKT) +
		le32_get_bits(band->info5,
		RX_PEER_BAND_TELEMETRY_STATS_INFO5_NUM_SUCES_RETRIED_UCAST_PKT);
	}

	rx->recv_from_reo.bytes +=
		((u64)le32_to_cpu(band->info0) |
		((u64)le32_get_bits(band->info1,
		RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES) << 32)) +
		((u64)le32_to_cpu(band->info2) |
		((u64)le32_get_bits(band->info1,
		RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_GCAST_BYTES) << 32));

	rx->recv_from_reo.packets +=
		le32_get_bits(band->info4,
		RX_PEER_BAND_TELEMETRY_STATS_INFO4_NUM_SUCES_FST_TRY_UCAST_PKT) +
		le32_get_bits(band->info5,
		RX_PEER_BAND_TELEMETRY_STATS_INFO5_NUM_SUCES_RETRIED_UCAST_PKT) +
		le32_get_bits(band->info6,
		RX_PEER_BAND_TELEMETRY_STATS_INFO6_NUM_SUCES_GCAST_PKT);

	hw_link_rx->success_ucast_bytes +=
		(u64)le32_to_cpu(band->info0) |
		((u64)le32_get_bits(band->info1,
		RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_BYTES) << 32);

	hw_link_rx->success_gcast_bytes +=
		(u64)le32_to_cpu(band->info2) |
		((u64)le32_get_bits(band->info1,
		RX_PEER_BAND_TELEMETRY_STATS_INFO1_UPPER_SUCCESS_GCAST_BYTES) << 32);

	hw_link_rx->failed_mpdu_bytes +=
		(u64)le32_to_cpu(band->info3) |
		((u64)le32_get_bits(band->info4,
		RX_PEER_BAND_TELEMETRY_STATS_INFO4_UPPER_FAIL_MPDU_BYTES) << 32);

	hw_link_rx->drop1_ucast_pkts +=
		le32_get_bits(band->info8,
		RX_PEER_BAND_TELEMETRY_STATS_INFO8_NUM_UCAST_DROPPED1_PKTS);

	hw_link_rx->success_gcast_pkts +=
		le32_get_bits(band->info6,
			      RX_PEER_BAND_TELEMETRY_STATS_INFO6_NUM_SUCES_GCAST_PKT);

	hw_link_rx->failed_mpdu +=
		le32_get_bits(band->info7,
			      RX_PEER_BAND_TELEMETRY_STATS_INFO7_NUM_FAILED_MPDUS);

	hw_link_rx->sum_rssi +=
		(u64)le32_to_cpu(band->info9) |
		((u64)le32_get_bits(band->info10,
		RX_PEER_BAND_TELEMETRY_STATS_INFO10_UPPER_SUM_RSSIS) << 32);

	hw_link_rx->success_ppdu_count +=
		le32_get_bits(band->info10,
			      RX_PEER_BAND_TELEMETRY_STATS_INFO10_PPDU_COUNT);

	hw_link_rx->sum_phy_rate +=
		(u64)le32_to_cpu(band->info11) |
		((u64)le32_get_bits(band->info10,
		RX_PEER_BAND_TELEMETRY_STATS_INFO10_UPPER_SUM_PHY_RATES) << 32);
}

/**
 * ath12k_wifi8_dp_update_rx_peer_telemetry() - Update RX peer telemetry stats
 * @dp_peer: pointer to the DP peer
 * @rx_desc: pointer to the RX peer telemetry descriptor
 *
 * Copies peer RX drop counters from the descriptor into
 * hw_rx, then iterates over all bands and calls
 * ath12k_wifi8_dp_update_rx_link_telemetry() for each band's link stats.
 */
static void
ath12k_wifi8_dp_update_rx_peer_telemetry(struct ath12k_dp_peer *dp_peer,
					 const struct rx_peer_telemetry_desc *rx_desc)
{
	struct ath12k_dp_peer_hw_rx_stats *hw_rx;
	struct ath12k_dp_link_peer *link_peer;
	int link_id;
	u8 band_id;

	if (!dp_peer->mld_stats.hw_stats)
		return;

	hw_rx = &dp_peer->mld_stats.hw_stats->hw_rx;

	/* HW Rx stats */
	hw_rx->drop_ucast_bytes +=
		(u64)le32_to_cpu(rx_desc->lower_drop_ucast_bytes) |
		((u64)le32_get_bits(rx_desc->info1,
			RX_PEER_TELEMETRY_DESC_INFO1_UPPER_DROP_UCAST_BYTES) << 32);

	hw_rx->drop_gcast_bytes +=
		(u64)le32_to_cpu(rx_desc->lower_drop_gcast_bytes) |
		((u64)le32_get_bits(rx_desc->info1,
			RX_PEER_TELEMETRY_DESC_INFO1_UPPER_DRP_GCAST_BYTES) << 32);

	hw_rx->drop2_pkts +=
		le32_get_bits(rx_desc->info2,
			      RX_PEER_TELEMETRY_DESC_INFO2_NUM_UCAST_DROPPED2_PACKETS);

	hw_rx->drop_gcast_pkts +=
		le32_get_bits(rx_desc->info3,
			      RX_PEER_TELEMETRY_DESC_INFO3_NUM_GCAST_DROPPED_PACKETS);

	/* Per-band/link Rx stats */
	for (link_id = 0; link_id < ATH12K_DP_PEER_MAX_MLO_LINKS; link_id++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_id);
		if (!link_peer)
			continue;

		band_id = link_peer->hw_link_id;
		if (band_id >= MAX_RX_PEER_BAND)
			continue;

		ath12k_wifi8_dp_update_rx_link_telemetry(dp_peer, link_peer,
							 link_id,
							 &rx_desc->peer_band[band_id]);
	}
}

/**
 * ath12k_wifi8_dp_update_tx_tid_telemetry() - Update per-TID TX pdev stats
 * @dp_hw_grp: HW group context used to resolve per-band pdev
 * @dp_pdev: pdev DP context holding tid_stats
 * @tx_desc: TX peer telemetry descriptor from the ring
 * @tid_idx: TID index to use for stats (0 for vdev peers, actual TID otherwise)
 * @is_vdev_peer: true if the peer is a vdev/self peer
 *
 * Accumulates HW TX completion, drop and fail counters from the
 * descriptor into dp_pdev->tid_stats.tid_tx[0][tid].tqm_status_cnt[]
 * using the TASC HW reason indices.
 */
static void
ath12k_wifi8_dp_update_tx_tid_telemetry(struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_pdev_dp *dp_pdev,
					const struct tx_peer_telemetry_desc *tx_desc,
					u8 tid_idx,
					bool is_vdev_peer)
{
	struct ath12k_tid_tx_stats *tx = &dp_pdev->tid_stats.tid_tx[0][tid_idx];
	const struct tx_peer_band_telemetry *band;
	u32 success_mask;
	u8 band_id;

	if (is_vdev_peer) {
		tx->tqm_status_cnt[HAL_TASC_REASON_HW_COMPLETION] +=
			le32_get_bits(tx_desc->info3,
				      TX_PEER_TELEMETRY_DESC_INFO3_NUM_DROPPED1_PACKETS);
	} else {
		tx->tqm_status_cnt[HAL_TASC_REASON_HW_DROP1] +=
			le32_get_bits(tx_desc->info3,
				      TX_PEER_TELEMETRY_DESC_INFO3_NUM_DROPPED1_PACKETS);
	}

	tx->tqm_status_cnt[HAL_TASC_REASON_HW_DROP2] +=
		le32_get_bits(tx_desc->info4,
			      TX_PEER_TELEMETRY_DESC_INFO4_NUM_DROPPED2_PACKETS);

	tx->tqm_status_cnt[HAL_TASC_REASON_HW_FAILED] +=
		le32_get_bits(tx_desc->info2,
			      TX_PEER_TELEMETRY_DESC_INFO2_NUM_FAIL_PACKETS);

	if (!is_vdev_peer) {
		success_mask = TX_PEER_BAND_TELEMETRY_STATS_INFO1_NUM_SUCCESS_PACKETS;
		for (band_id = 0; band_id < MAX_TX_PEER_BAND; band_id++) {
			struct ath12k_pdev_dp *band_pdev;

			band = &tx_desc->peer_band[band_id];
			band_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, band_id);
			if (!band_pdev)
				continue;

			tx = &band_pdev->tid_stats.tid_tx[0][tid_idx];
			tx->tqm_status_cnt[HAL_TASC_REASON_HW_COMPLETION] +=
					le32_get_bits(band->info1, success_mask);
		}
	}
}

/**
 * ath12k_wifi8_dp_update_rx_tid_telemetry() - Update per-TID RX pdev stats
 * @dp_hw_grp: HW group context used to resolve per-band pdev
 * @dp_pdev: pdev DP context holding tid_stats
 * @rx_desc: RX peer telemetry descriptor from the ring
 * @tid_idx: TID index to use for stats (0 for vdev peers, actual TID otherwise)
 *
 * Accumulates HW RX success, unicast drop and gcast drop counters from
 * the descriptor into dp_pdev->tid_stats.tid_rx[0][tid].
 */
static void
ath12k_wifi8_dp_update_rx_tid_telemetry(struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_pdev_dp *dp_pdev,
					const struct rx_peer_telemetry_desc *rx_desc,
					u8 tid_idx)
{
	struct ath12k_tid_rx_stats *rx = &dp_pdev->tid_stats.tid_rx[0][tid_idx];
	const struct rx_peer_band_telemetry *band;
	u8 band_id;

	rx->fail_cnt[DP_TID_RX_HW_DROP2_UCAST] +=
		le32_get_bits(rx_desc->info2,
			      RX_PEER_TELEMETRY_DESC_INFO2_NUM_UCAST_DROPPED2_PACKETS);

	rx->fail_cnt[DP_TID_RX_HW_DROP_GCAST] +=
		le32_get_bits(rx_desc->info3,
			      RX_PEER_TELEMETRY_DESC_INFO3_NUM_GCAST_DROPPED_PACKETS);

	for (band_id = 0; band_id < MAX_RX_PEER_BAND; band_id++) {
		struct ath12k_pdev_dp *band_pdev;

		band = &rx_desc->peer_band[band_id];
		band_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, band_id);
		if (!band_pdev)
			continue;

		rx = &band_pdev->tid_stats.tid_rx[0][tid_idx];
		rx->msdu_cnt +=
			le32_get_bits(band->info4,
			RX_PEER_BAND_TELEMETRY_STATS_INFO4_NUM_SUCES_FST_TRY_UCAST_PKT) +
			le32_get_bits(band->info5,
			RX_PEER_BAND_TELEMETRY_STATS_INFO5_NUM_SUCES_RETRIED_UCAST_PKT);

		rx->fail_cnt[DP_TID_RX_HW_DROP1_UCAST] +=
			le32_get_bits(band->info8,
			RX_PEER_BAND_TELEMETRY_STATS_INFO8_NUM_UCAST_DROPPED1_PKTS);
	}
}

/**
 * ath12k_wifi8_dp_process_tx_peer_telemetry() - Process TX peer telemetry ring
 * @dp: DP context
 * @budget: Max number of descriptors to process in this NAPI poll cycle
 *
 * Drains the TQM2SW_TELEMETRY ring. For each descriptor:
 *   - Extracts the stats_id
 *   - Looks up the dp_peer from stats ID from desc
 *   - Calls ath12k_wifi8_dp_update_tx_peer_telemetry() to account stats
 *
 * Returns: number of descriptors processed (work_done for NAPI budget)
 */
int ath12k_wifi8_dp_process_tx_peer_telemetry(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct tx_peer_telemetry_desc *tx_desc;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct hal_srng *srng;
	u16 stats_id, dp_peer_id;
	u8 hw_link_id, tid, tid_idx;
	int num_descs = 0;

	srng = &dp->hal->srng_list[dp_wifi8->tx_peer_telemetry_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget > 0) {
		tx_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!tx_desc)
			break;

		/* Extract stats_id from descriptor */
		stats_id = le32_get_bits(tx_desc->info0,
					 TX_PEER_TELEMETRY_DESC_INFO0_STATS_ID);

		if (stats_id >= ATH12K_MAX_STATS_ID) {
			ath12k_err(ab, "Tx: stats_id %u out of bounds\n",
				   stats_id);
			goto next_tx_desc;
		}

		dp_peer_id = dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id;
		hw_link_id = dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id;
		tid = dp_hw_grp_wifi8->stats_id_map[stats_id].tid;

		if (dp_peer_id >= MAX_DP_PEER_LIST_SIZE) {
			ath12k_dbg(ab, ATH12K_DBG_TELEMETRY,
				   "Tx: dp_peer_id %u out of bounds for stats_id %u\n",
				   dp_peer_id, stats_id);
			goto next_tx_desc;
		}

		rcu_read_lock();
		dp_peer = ath12k_wifi8_dp_telemetry_peer_find(dp_hw_grp,
							      dp_peer_id,
							      hw_link_id);
		if (!dp_peer) {
			ath12k_dbg(ab, ATH12K_DBG_TELEMETRY,
				   "Tx: dp_peer NULL for stats_id:%u, dp_peer_id:%u\n",
				   stats_id, dp_peer_id);
			rcu_read_unlock();
			goto next_tx_desc;
		}

		ath12k_wifi8_dp_update_tx_peer_telemetry(dp_peer, tx_desc);

		/*
		 * When VoW stats are enabled, the stats_id_map carries a
		 * valid TID. Update per-TID pdev stats from the descriptor,
		 * distributing counters to the correct pdev for each band.
		 * Vdev peers use TID 0 and account drop1 as completion.
		 */
		tid_idx = dp_peer->is_vdev_peer ? 0 : tid;
		dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
		if (dp_pdev && ath12k_dp_vow_stats_enabled(dp_pdev) &&
		    (dp_peer->is_vdev_peer || tid < ATH12K_DATA_TID_MAX))
			ath12k_wifi8_dp_update_tx_tid_telemetry(dp_hw_grp,
								dp_pdev,
								tx_desc,
								tid_idx,
								dp_peer->is_vdev_peer);
		rcu_read_unlock();

next_tx_desc:
		num_descs++;
		budget--;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return num_descs;
}

/**
 * ath12k_wifi8_dp_process_rx_peer_telemetry() - Process RX peer telemetry ring
 * @dp: DP context
 * @budget: Max number of descriptors to process in this NAPI poll cycle
 *
 * Drains the REO2SW_TELEMETRY ring. For each descriptor:
 *   - Extracts the stats_id
 *   - Looks up ithe dp_peer from Stats ID from desc
 *   - Calls ath12k_wifi8_dp_update_rx_peer_telemetry() to account stats
 *
 * Returns: number of descriptors processed (work_done for NAPI budget)
 */
int ath12k_wifi8_dp_process_rx_peer_telemetry(struct ath12k_dp *dp, int budget)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct rx_peer_telemetry_desc *rx_desc;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_pdev_dp *dp_pdev;
	struct hal_srng *srng;
	u16 stats_id, dp_peer_id;
	u8 hw_link_id, tid, tid_idx;
	int num_descs = 0;

	srng = &dp->hal->srng_list[dp_wifi8->rx_peer_telemetry_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (budget > 0) {
		rx_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!rx_desc)
			break;

		/* Extract TASC stats_id from descriptor */
		stats_id = le32_get_bits(rx_desc->info0,
					 RX_PEER_TELEMETRY_DESC_INFO0_STATS_ID);

		if (stats_id >= ATH12K_MAX_STATS_ID) {
			ath12k_err(ab, "Rx: stats_id %u out of bounds\n",
				   stats_id);
			goto next_rx_desc;
		}

		dp_peer_id = dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id;
		hw_link_id = dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id;
		tid = dp_hw_grp_wifi8->stats_id_map[stats_id].tid;

		if (dp_peer_id >= MAX_DP_PEER_LIST_SIZE) {
			ath12k_dbg(ab, ATH12K_DBG_TELEMETRY,
				   "Rx: dp_peer_id %u out of bounds for stats_id %u\n",
				   dp_peer_id, stats_id);
			goto next_rx_desc;
		}

		rcu_read_lock();
		dp_peer = ath12k_wifi8_dp_telemetry_peer_find(dp_hw_grp,
							      dp_peer_id,
							      hw_link_id);
		if (!dp_peer) {
			ath12k_dbg(ab, ATH12K_DBG_TELEMETRY,
				   "Rx: dp_peer NULL for stats_id:%u, dp_peer_id:%u\n",
				   stats_id, dp_peer_id);
			rcu_read_unlock();
			goto next_rx_desc;
		}

		ath12k_wifi8_dp_update_rx_peer_telemetry(dp_peer, rx_desc);

		/*
		 * When VoW stats are enabled, the stats_id_map carries a
		 * valid TID. Update per-TID pdev stats from the descriptor,
		 * distributing counters to the correct pdev for each band.
		 * Vdev peers use TID 0.
		 */
		tid_idx = dp_peer->is_vdev_peer ? 0 : tid;
		dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp_hw_grp, hw_link_id);
		if (dp_pdev && ath12k_dp_vow_stats_enabled(dp_pdev) &&
		    (dp_peer->is_vdev_peer || tid < ATH12K_DATA_TID_MAX))
			ath12k_wifi8_dp_update_rx_tid_telemetry(dp_hw_grp,
								dp_pdev,
								rx_desc,
								tid_idx);
		rcu_read_unlock();

next_rx_desc:
		num_descs++;
		budget--;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return num_descs;
}

int ath12k_wifi8_dp_telemetry_init(struct ath12k_dp *dp)
{
	ath12k_wifi8_hal_tasc_peer_clk_cycle_config(dp->ab);
	ath12k_wifi8_hal_tasc_peer_tx_cfg(dp->ab, true);

	ath12k_wifi8_hal_tasc_peer_tx_window(dp->ab,
					     DP_TELEMETRY_PEER_2PEER_WINDOW);
	ath12k_wifi8_hal_tasc_peer_tx_max_peer(
					dp->ab,
					DP_TELEMETRY_MAX_UCAST_PEERS,
					DP_TELEMETRY_MAX_GCAST_DL_PEERS,
					DP_TELEMETRY_MAX_GCAST_UL_PEERS);

	ath12k_wifi8_hal_tasc_peer_tx_fail_drop_default(dp->ab);
	ath12k_wifi8_hal_num_transmission_map_default(dp->ab);

	ath12k_wifi8_hal_tasc_peer_rx_cfg(dp->ab, true);

	ath12k_wifi8_hal_tasc_peer_rx_window(dp->ab,
					     DP_TELEMETRY_PEER_2PEER_WINDOW);
	ath12k_wifi8_hal_tasc_peer_rx_max_peer(
					dp->ab,
					DP_TELEMETRY_MAX_UCAST_PEERS,
					DP_TELEMETRY_MAX_GCAST_DL_PEERS);

	ath12k_wifi8_hal_tasc_peer_rx_fail_drop_default(dp->ab);

	return 0;
}

int ath12k_wifi8_dp_telemetry_deinit(struct ath12k_dp *dp)
{
	ath12k_wifi8_hal_tasc_peer_tx_cfg(dp->ab, false);
	ath12k_wifi8_hal_tasc_peer_rx_cfg(dp->ab, false);
	return 0;
}

static void ath12k_wifi8_dp_telemetry_set_peer_config(struct ath12k_base *ab, u16 window,
						      u16 ucast_peer, u8 gcast_dl_peer,
						      u8 gcast_ul_peer)
{
	ath12k_wifi8_hal_tasc_peer_tx_window(ab, window);
	ath12k_wifi8_hal_tasc_peer_rx_window(ab, window);
	ath12k_wifi8_hal_tasc_peer_tx_max_peer(ab, ucast_peer, gcast_dl_peer,
					       gcast_ul_peer);
	ath12k_wifi8_hal_tasc_peer_rx_max_peer(ab, ucast_peer, gcast_dl_peer);
}

void ath12k_wifi8_dp_telemetry_peer_count_update(struct ath12k_base *ab,
						 unsigned int num_peers)
{
	u16 window, ucast_peer;
	u8 gcast_dl_peer, gcast_ul_peer;

	if (num_peers == DP_TELEMETRY_EXT_PEER_START) {
		window = DP_TELEMETRY_EXT_PEER_2PEER_WINDOW;
		ucast_peer = DP_TELEMETRY_EXT_MAX_UCAST_PEERS;
		gcast_dl_peer = DP_TELEMETRY_MAX_GCAST_DL_PEERS;
		gcast_ul_peer = DP_TELEMETRY_MAX_GCAST_UL_PEERS;
	} else if (num_peers == DP_TELEMETRY_DEFAULT_PEER_START) {
		window = DP_TELEMETRY_PEER_2PEER_WINDOW;
		ucast_peer = DP_TELEMETRY_MAX_UCAST_PEERS;
		gcast_dl_peer = DP_TELEMETRY_MAX_GCAST_DL_PEERS;
		gcast_ul_peer = DP_TELEMETRY_MAX_GCAST_UL_PEERS;
	} else {
		return;
	}

	ath12k_wifi8_dp_telemetry_set_peer_config(ab, window, ucast_peer,
						  gcast_dl_peer, gcast_ul_peer);
}

int ath12k_wifi8_dp_telemetry_peer_config(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX])
{
	ath12k_wifi8_hal_tasc_peer_tx_set_id(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_tx_band(dp->ab, link_band_id, 1);

	ath12k_wifi8_hal_tasc_peer_rx_set_id(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_rx_band(dp->ab, link_band_id, 1);
	return 0;
}

int ath12k_wifi8_dp_telemetry_peer_delete(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX])
{
	ath12k_wifi8_hal_tasc_reset_peer_tx(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_tx_band(dp->ab, link_band_id, 0);
	ath12k_wifi8_hal_tasc_reset_peer_rx(dp->ab, stats_id);
	ath12k_wifi8_hal_tasc_peer_rx_band(dp->ab, link_band_id, 0);
	return 0;
}

int ath12k_wifi8_dp_telemetry_umac_peer_setup(struct ath12k_base *ab)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct stats_to_peer_id_map *stats_map;
	struct ath12k_dp_link_peer *link_peer;
	u16 link_band_id[HAL_TASC_BAND_MAX];
	u16 tid_band_id[HAL_TASC_BAND_MAX];
	struct ath12k_dp_peer *dp_peer;
	u16 stats_id;
	int i;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(ab->dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return -EINVAL;

	for (stats_id = 0; stats_id < ATH12K_MAX_STATS_ID; stats_id++) {
		stats_map = &dp_hw_grp_wifi8->stats_id_map[stats_id];
		if (stats_map->dp_peer_id == ATH12K_MLO_PEER_ID_INVALID ||
		    stats_map->hw_link_id == ATH12K_DP_HW_LINK_ID_INVALID)
			continue;

		for (i = 0; i < HAL_TASC_BAND_MAX; i++) {
			link_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;
			tid_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;
		}

		rcu_read_lock();
		dp_peer = ath12k_wifi8_dp_telemetry_peer_find(ab->dp->dp_hw_grp,
							      stats_map->dp_peer_id,
							      stats_map->hw_link_id);
		if (!dp_peer) {
			rcu_read_unlock();
			continue;
		}

		if (stats_map->tid == ATH12K_INVALID_TID) {
			for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
				link_peer =
				ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
				if (!link_peer)
					continue;
				link_band_id[link_peer->hw_link_id] =
							link_peer->link_band_id;
			}
			rcu_read_unlock();
			ath12k_wifi8_dp_telemetry_peer_config(ab->dp, stats_id,
							      link_band_id);
		} else {
			for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
				link_peer =
				ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
				if (!link_peer)
					continue;
				if (link_peer->tid_band_id[stats_map->tid] <
								ATH12K_MAX_STATS_ID)
					tid_band_id[link_peer->hw_link_id] =
						link_peer->tid_band_id[stats_map->tid];
			}
			rcu_read_unlock();
			ath12k_wifi8_dp_telemetry_peer_config(ab->dp, stats_id,
							      tid_band_id);
		}
	}

	return 0;
}

int ath12k_wifi8_dp_telemetry_umac_init(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wifi8_dp_telemetry_ring_init(ab);
	if (ret)
		return ret;

	ret = ath12k_wifi8_dp_telemetry_init(ab->dp);
	if (ret)
		return ret;

	ret = ath12k_wifi8_dp_telemetry_umac_peer_setup(ab);
	if (ret)
		return ret;

	return 0;
}
