/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TELEMETRY_H
#define ATH12K_DP_TELEMETRY_H

#include "../core.h"

#define DP_TELEMETRY_MAX_UCAST_PEERS	512
#define DP_TELEMETRY_EXT_MAX_UCAST_PEERS	1024
#define DP_TELEMETRY_UCAST_PEER_THRESHOLD	511
#define DP_TELEMETRY_EXT_PEER_START	(DP_TELEMETRY_UCAST_PEER_THRESHOLD + 1)
#define DP_TELEMETRY_DEFAULT_PEER_START	(DP_TELEMETRY_UCAST_PEER_THRESHOLD - 1)
#define DP_TELEMETRY_MAX_GCAST_DL_PEERS	80
#define DP_TELEMETRY_MAX_GCAST_UL_PEERS	8

#define DP_TELEMETRY_EXT_PEER_WINDOW	0x458
#define DP_TELEMETRY_PEER_WINDOW	0x258
#define DP_TELEMETRY_PEER_2PEER_WINDOW	\
	(DP_TELEMETRY_PEER_WINDOW / \
	 (DP_TELEMETRY_MAX_UCAST_PEERS + \
	  DP_TELEMETRY_MAX_GCAST_DL_PEERS + \
	  DP_TELEMETRY_MAX_GCAST_UL_PEERS))
#define DP_TELEMETRY_EXT_PEER_2PEER_WINDOW	\
	(DP_TELEMETRY_EXT_PEER_WINDOW / \
	 (DP_TELEMETRY_EXT_MAX_UCAST_PEERS + \
	  DP_TELEMETRY_MAX_GCAST_DL_PEERS + \
	  DP_TELEMETRY_MAX_GCAST_UL_PEERS))

#define DP_TELEMETRY_INVALID_LINK_BAND_ID	2046

int ath12k_wifi8_dp_telemetry_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi8_dp_telemetry_ring_init(struct ath12k_base *ab);
int ath12k_wifi8_dp_telemetry_ring_cleanup(struct ath12k_base *ab);
int ath12k_wifi8_dp_process_tx_peer_telemetry(struct ath12k_dp *dp, int budget);
int ath12k_wifi8_dp_process_rx_peer_telemetry(struct ath12k_dp *dp, int budget);
int ath12k_wifi8_dp_telemetry_init(struct ath12k_dp *dp);
int ath12k_wifi8_dp_telemetry_deinit(struct ath12k_dp *dp);
void ath12k_wifi8_dp_telemetry_peer_count_update(struct ath12k_base *ab,
						 unsigned int num_peers);
int ath12k_wifi8_dp_telemetry_peer_config(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX]);
int ath12k_wifi8_dp_telemetry_peer_delete(struct ath12k_dp *dp, u16 stats_id,
					  u16 link_band_id[HAL_TASC_BAND_MAX]);
int ath12k_wifi8_dp_telemetry_umac_init(struct ath12k_base *ab);
#endif
