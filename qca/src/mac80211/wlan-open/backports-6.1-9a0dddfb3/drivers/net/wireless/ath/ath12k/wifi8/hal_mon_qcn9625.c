// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "../hal_mon_cmn.h"
#include "../dp_mon.h"
#include "hal_mon.h"
#include "hal_qcn9625.h"

/*
 * Generic monitor ops shared by all QCN9625 revisions. Each revision's
 * ops-init helper (ath12k_wifi8_hal_qcn9625_hw{10,20}_mon_ops_init) starts from
 * this table and overrides only the entries that are version-specific — the
 * mpdu_start parser and its WMASK getter. Defining the shared entries once
 * keeps a single source of truth: a new monitor op added here is inherited by
 * every revision automatically.
 */
const struct hal_mon_ops hal_qcn9625_mon_ops_base = {
	.get_mon_mpdu_end_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_end_wmask_get,
	.get_mon_msdu_end_wmask =
		ath12k_wifi8_hal_mon_rx_msdu_end_wmask_get,
	.get_mon_ppdu_end_usr_stats_wmask =
		ath12k_wifi8_hal_mon_rx_ppdu_end_usr_stats_wmask_get,
	.rx_msdu_end_info_get =
		ath12k_wifi8_hal_mon_rx_msdu_end_info_parse,
	.rx_ppdu_eu_stats_info_get =
		ath12k_wifi8_hal_mon_rx_ppdu_eu_stats_info_parse,
	.rx_desc_get_msdu_payload =
		ath12k_wifi8_hal_mon_rx_desc_get_msdu_payload,
	.tx_parse_status_tlv = ath12k_wifi8_hal_mon_tx_parse_status_tlv,
	.tx_status_get_num_user = ath12k_wifi8_hal_mon_tx_status_get_num_user,
	.hal_mon_set_mon_buf_desc = ath12k_wifi8_hal_mon_set_mon_buf_desc,
	.is_mon_buf_addr_tlv = ath12k_wifi8_is_mon_buf_addr_tlv,
	.hal_mon_tx_ppdu_info = ath12k_wifi8_hal_mon_tx_ppdu_info,
	.get_tx_mon_wmask_config = ath12k_wifi8_hal_tx_mon_get_wmask_config,
	.tx_fes_setup_info_get =
		ath12k_wifi8_hal_mon_tx_fes_setup_info_parse,
	.tx_peer_entry_info_get =
		ath12k_wifi8_hal_mon_tx_peer_entry_info_parse,
	.tx_queue_ext_info_get =
		ath12k_wifi8_hal_mon_tx_queue_ext_info_parse,
	.tx_mpdu_start_info_get =
		ath12k_wifi8_hal_mon_tx_mpdu_start_info_parse,
	.tx_fes_status_info_get =
		ath12k_wifi8_hal_mon_tx_fes_status_end_info_parse,
	.tx_response_end_status_info_get =
		ath12k_wifi8_hal_mon_tx_response_end_status_info_parse,
	.tx_fes_status_prot_info_get =
		ath12k_wifi8_hal_mon_tx_fes_status_prot_info_parse,
	.tx_pcu_ppdu_setup_init_info_get =
		ath12k_wifi8_hal_mon_tx_pcu_ppdu_setup_init_info_parse,
	.extract_tx_mon_ring_desc = ath12k_wifi8_extract_tx_mon_ring_desc,
	.tx_mon_pkt_buf_cnt_in_desc = ath12k_wifi8_tx_mon_pkt_buf_cnt_in_desc,
};

/* HW2.0 (E3R86) writable instance, populated by the init helper below. */
static struct hal_mon_ops hal_qcn9625_hw20_mon_ops;

void ath12k_wifi8_hal_qcn9625_hw20_mon_ops_init(struct ath12k_hal *hal)
{
	hal_qcn9625_hw20_mon_ops = hal_qcn9625_mon_ops_base;
	hal_qcn9625_hw20_mon_ops.get_mon_mpdu_start_wmask =
		ath12k_wifi8_hal_mon_rx_mpdu_start_wmask_get;
	hal_qcn9625_hw20_mon_ops.rx_mpdu_start_info_get =
		ath12k_wifi8_hal_mon_rx_mpdu_start_info_parse;

	hal->hal_mon_ops = &hal_qcn9625_hw20_mon_ops;
}
