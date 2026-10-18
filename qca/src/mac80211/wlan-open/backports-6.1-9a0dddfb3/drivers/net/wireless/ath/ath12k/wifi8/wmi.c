// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "wmi.h"
#include "mgmt_rx.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif

void ath12k_wifi8_wmi_init_qcn9625(struct ath12k_base *ab,
				   struct ath12k_wmi_resource_config_arg *config)
{
	u8 total_vdevs;

	total_vdevs = ath12k_core_get_total_num_vdevs(ab);
	config->num_vdevs = ab->num_radios * total_vdevs;
	config->num_peers = ab->num_radios *
		ath12k_core_get_def_max_peers_per_radio(ab);
	config->num_tids = ath12k_core_get_max_num_tids(ab);
	config->num_offload_peers = TARGET_NUM_OFFLD_PEERS;
	config->num_offload_reorder_buffs = TARGET_NUM_OFFLD_REORDER_BUFFS;
	config->num_peer_keys = TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = TARGET_RX_TIMEOUT_HI_PRI;

	if (test_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ab->ag->flags))
		config->rx_decap_mode = TARGET_DECAP_MODE_RAW;
	else
		config->rx_decap_mode = TARGET_DECAP_MODE_NATIVE_WIFI;

	config->scan_max_pending_req = TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = TARGET_NUM_MCAST_GROUPS;
	config->num_mcast_table_elems = TARGET_NUM_MCAST_TABLE_ELEMS;
	config->mcast2ucast_mode = TARGET_MCAST2UCAST_MODE;
	config->tx_dbg_log_size = TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = TARGET_NUM_WDS_ENTRIES;
	config->dma_burst_size = TARGET_DMA_BURST_SIZE;
	config->rx_skip_defrag_timeout_dup_detection_check =
		TARGET_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK;
	config->vow_config = TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = TARGET_GTK_OFFLOAD_MAX_VDEV;
	config->num_msdu_desc = TARGET_NUM_MSDU_DESC;
	config->beacon_tx_offload_max_vdev = ab->num_radios * TARGET_MAX_BCN_OFFLD;
	config->rx_batchmode = TARGET_RX_BATCHMODE;
	/* Indicates host supports peer map v3 and unmap v2 support */
	config->peer_map_unmap_version = 0x32;
	config->twt_ap_pdev_count = ab->num_radios;
	config->twt_ap_sta_count = 1000;
	config->ema_max_vap_cnt = ab->num_radios * TARGET_MAX_MBSSID_GROUPS;
	config->ema_max_profile_period = TARGET_EMA_MAX_PROFILE_PERIOD;
	if (ath12k_carrier_vow_optimization)
		config->carrier_vow_optimization = ath12k_carrier_vow_optimization;
	config->beacon_tx_offload_max_vdev += config->ema_max_vap_cnt;

	if (test_bit(WMI_TLV_SERVICE_PEER_METADATA_V1A_V1B_SUPPORT, ab->wmi_ab.svc_map))
		config->peer_metadata_ver = ATH12K_PEER_METADATA_V1B;

	if (test_bit(WMI_TLV_SERVICE_SDWF_LEVEL0, ab->wmi_ab.svc_map))
		config->qos = true;

	if (test_bit(WMI_TLV_SERVICE_ATF, ab->wmi_ab.svc_map)) {
		config->atf_config |= WMI_RSRC_CFG_FLAG1_ATF_OFFLOAD_ENABLE;
#ifdef CPTCFG_QCN_EXTN
		config->carrier_config = ath12k_cfg_get(ab,
							ATH12K_CFG_CARRIER_PROFILE_CFG);
#endif /* CPTCFG_QCN_EXTN */
	}

#ifdef CPTCFG_QCN_EXTN
	config->max_beacon_size = ath12k_cfg_get(ab, ATH12K_CFG_AP_MAX_MGMT_FRM_SZ);
#else
	config->max_beacon_size = 1500;
#endif /* CPTCFG_QCN_EXTN */
	config->max_num_group_keys = ATH12K_GROUP_KEYS_NUM_MAX;
#ifdef CPTCFG_QCN_EXTN
	config->rep_ul_resp = ath12k_cfg_get(ab, ATH12K_CFG_REP_UL_RESP);
#else
	config->rep_ul_resp = 0;
#endif /* CPTCFG_QCN_EXTN */
#ifdef CPTCFG_QCN_EXTN
	config->host_reo_mgmt_support =
		!ath12k_cfg_get(ab, ATH12K_CFG_REO_MGMT_PATH_DISABLE);
#else
	config->host_reo_mgmt_support = 1;
#endif /* CPTCFG_QCN_EXTN */
}

void ath12k_wifi8_cu_notify(struct ath12k *ar,
			    struct ath12k_link_vif *update_arvif)
{
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;

	if (!test_bit(WMI_TLV_SERVICE_SHARED_CU_MEM_MODEL_COUNT_DOWN,
		      ar->ab->wmi_ab.svc_map))
		return;

	if (update_arvif) {
		ath12k_wifi8_cu_mem_update(ar->ab, update_arvif, true);
		return;
	}

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->cu_mem || !arvif->is_up ||
		    ath12k_mac_is_bridge_vdev(arvif))
			continue;

		vif = arvif->ahvif->vif;
		if (!vif->valid_links)
			continue;

		ath12k_wifi8_cu_mem_update(ar->ab, arvif, true);
	}
	spin_unlock_bh(&ar->data_lock);
}
