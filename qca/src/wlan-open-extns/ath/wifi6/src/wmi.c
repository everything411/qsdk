// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../core.h"
#include "wmi.h"
#include "../ini.h"

void ath12k_wifi6_wmi_init_qcn9074(struct ath12k_base *ab,
				   struct ath12k_wmi_resource_config_arg *config)
{

	u8 total_vdevs;

	total_vdevs = ath12k_core_get_total_num_vdevs(ab);
	config->num_vdevs = ab->num_radios * total_vdevs;
	config->num_peers = ab->num_radios *
		ath12k_core_get_def_max_peers_per_radio(ab);
	config->num_tids = ath12k_core_get_max_num_tids(ab);

	config->num_offload_peers = WIFI6_TARGET_NUM_OFFLD_PEERS;
	config->num_offload_reorder_buffs = WIFI6_TARGET_NUM_OFFLD_REORDER_BUFFS;
	config->num_peer_keys = WIFI6_TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = WIFI6_TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = WIFI6_TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = WIFI6_TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = WIFI6_TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = WIFI6_TARGET_RX_TIMEOUT_HI_PRI;

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		config->rx_decap_mode = WIFI6_TARGET_DECAP_MODE_RAW;
	else
		config->rx_decap_mode = WIFI6_TARGET_DECAP_MODE_NATIVE_WIFI;

	config->scan_max_pending_req = WIFI6_TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = WIFI6_TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = WIFI6_TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = WIFI6_TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = WIFI6_TARGET_NUM_MCAST_GROUPS;
	config->num_mcast_table_elems = WIFI6_TARGET_NUM_MCAST_TABLE_ELEMS;
	config->mcast2ucast_mode = WIFI6_TARGET_MCAST2UCAST_MODE;
	config->tx_dbg_log_size = WIFI6_TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = WIFI6_TARGET_NUM_WDS_ENTRIES;
	config->dma_burst_size = WIFI6_TARGET_DMA_BURST_SIZE;
	config->rx_skip_defrag_timeout_dup_detection_check =
		WIFI6_TARGET_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK;
	config->vow_config = WIFI6_TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = WIFI6_TARGET_GTK_OFFLOAD_MAX_VDEV;
	config->num_msdu_desc = WIFI6_TARGET_NUM_MSDU_DESC;
	config->beacon_tx_offload_max_vdev = ab->num_radios * WIFI6_TARGET_MAX_BCN_OFFLD;
	config->rx_batchmode = WIFI6_TARGET_RX_BATCHMODE;
	config->peer_map_unmap_version = 0x32;
	config->twt_ap_pdev_count = ab->num_radios;
	config->twt_ap_sta_count = 1000;
	config->ema_max_vap_cnt = ab->num_radios;
	config->ema_max_profile_period = WIFI6_TARGET_EMA_MAX_PROFILE_PERIOD;
	config->beacon_tx_offload_max_vdev += config->ema_max_vap_cnt;
	config->max_num_group_keys = ATH12K_GROUP_KEYS_NUM_MAX;
}
