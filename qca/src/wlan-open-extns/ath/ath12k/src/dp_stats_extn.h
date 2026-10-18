/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef DP_STATS_EXTN_H
#define DP_STATS_EXTN_H
#include "../core.h"
#include "../wmi.h"
#include "../mac.h"
#include "../debug.h"
#include "../vendor.h"
#include "../dp_stats.h"

#ifndef CPTCFG_QCN_EXTN

static void ath12k_dp_rx_scan_radio_stats_update(
		struct ath12k_telemetry_dp_vif *telemetry_vif_stats,
		struct ath12k_dp_rx_scan_radio_stats *scan_radio_stats)
{
}

#else

void ath12k_dp_rx_scan_radio_stats_update(
		struct ath12k_telemetry_dp_vif *telemetry_vif_stats,
		struct ath12k_dp_rx_scan_radio_stats *scan_radio_stats);

#endif //CPTCFG_QCN_EXTN

#endif //DP_STATS_EXTN_H
