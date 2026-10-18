// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_stats_extn.h"

void ath12k_dp_rx_scan_radio_stats_update(
		struct ath12k_telemetry_dp_vif *telemetry_vif_stats,
		struct ath12k_dp_rx_scan_radio_stats  *rx_scan_radio_stats)
{

	if (!telemetry_vif_stats || !rx_scan_radio_stats)
		return;

	telemetry_vif_stats->rx_scan_radio_stats.rx_ok_bytes =
		rx_scan_radio_stats->rx_ok_bytes;
	telemetry_vif_stats->rx_scan_radio_stats.rx_err_bytes =
		rx_scan_radio_stats->rx_err_bytes;
	telemetry_vif_stats->rx_scan_radio_stats.rx_ok_pkts =
		rx_scan_radio_stats->rx_ok_pkts;
	telemetry_vif_stats->rx_scan_radio_stats.rx_err_pkts =
		rx_scan_radio_stats->rx_err_pkts;
	telemetry_vif_stats->rx_scan_radio_stats.rx_mgmt_pkts =
		rx_scan_radio_stats->rx_mgmt_pkts;
	telemetry_vif_stats->rx_scan_radio_stats.rx_ctrl_pkts =
		rx_scan_radio_stats->rx_ctrl_pkts;
	telemetry_vif_stats->rx_scan_radio_stats.rx_data_pkts =
		rx_scan_radio_stats->rx_data_pkts;
}
