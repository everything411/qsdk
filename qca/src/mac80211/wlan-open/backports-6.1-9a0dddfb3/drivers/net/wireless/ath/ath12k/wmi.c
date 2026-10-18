// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "wmi.h"
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/sort.h>
#include <linux/time.h>
#include <linux/of.h>
#include "core.h"
#include "debugfs.h"
#include "debug.h"
#include "mac.h"
#include "hw.h"
#include "peer.h"
#include "p2p.h"
#include "testmode.h"
#include "dp_mon.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "cfr.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ini.h"
#endif
#ifdef CPTCFG_ATHDEBUG
#include "athdbg_if.h"
#endif
#include "erp.h"
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_QCN_EXTN */
#endif
#include "smd.h"

struct ath12k_wmi_svc_ready_parse {
	bool wmi_svc_bitmap_done;
};

struct wmi_tlv_fw_stats_parse {
	const struct wmi_stats_event *ev;
	const struct wmi_per_chain_rssi_stat_params *rssi;
	int rssi_num;
	bool chain_rssi_done;
	bool rssi_stats_done;
	bool congestion_stats_done;
	bool peer_extd2_stats_done;
	bool pmf_bcn_protect_stats_done;
	bool vdev_extd_stats_done;
	struct ath12k_fw_stats *stats;
};

struct ath12k_wmi_dma_ring_caps_parse {
	struct ath12k_wmi_dma_ring_caps_params *dma_ring_caps;
	u32 n_dma_ring_caps;
};

struct ath12k_wmi_service_ext_arg {
	u32 default_conc_scan_config_bits;
	u32 default_fw_config_bits;
	struct ath12k_wmi_ppe_threshold_arg ppet;
	u32 he_cap_info;
	u32 mpdu_density;
	u32 max_bssid_rx_filters;
	u32 num_hw_modes;
	u32 num_phy;
	u32 num_chainmask_tables;
	struct ath12k_chainmask_table
		chainmask_table[ATH12K_MAX_CHAINMASK_TABLES];
};

struct ath12k_wmi_svc_rdy_ext_parse {
	struct ath12k_wmi_service_ext_arg arg;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	u32 n_hw_mode_caps;
	u32 tot_phy_id;
	struct ath12k_wmi_hw_mode_cap_params pref_hw_mode_caps;
	struct ath12k_wmi_mac_phy_caps_params *mac_phy_caps;
	u32 n_mac_phy_caps;
	const struct ath12k_wmi_soc_hal_reg_caps_params *soc_hal_reg_caps;
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_hal_reg_caps;
	u32 n_ext_hal_reg_caps;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	bool hw_mode_done;
	bool mac_phy_done;
	bool ext_hal_reg_done;
	u32 n_mac_phy_chainmask_combo;
	bool mac_phy_chainmask_combo_done;
	bool mac_phy_chainmask_cap_done;
	bool oem_dma_ring_cap_done;
	bool dma_ring_cap_done;
};

/**
 * ath12k_wmi_svc_rdy_ext2_arg - WMI service ready extended 2
 * @afc_deployment_type: AFC deployment type (indoor, outdoor)
 * @mapc_max_co_ap_peers: max general co-AP peers supported
 * @mapc_max_ctdma_ap_peers: max Co-TDMA peers supported
 */
struct ath12k_wmi_svc_rdy_ext2_arg {
	u32 reg_db_version;
	u32 hw_min_max_tx_power_2ghz;
	u32 hw_min_max_tx_power_5ghz;
	u32 chwidth_num_peer_caps;
	u32 preamble_puncture_bw;
	u32 max_user_per_ppdu_ofdma;
	u32 max_user_per_ppdu_mumimo;
	u32 target_cap_flags;
	u32 eht_cap_mac_info[WMI_MAX_EHTCAP_MAC_SIZE];
	u32 max_num_linkview_peers;
	u32 max_tid_msduq;
	u32 def_tid_msduq;
	u32 afc_deployment_type;
	u32 mapc_max_co_ap_peers;
	u32 mapc_max_ctdma_ap_peers;
};

struct ath12k_wmi_svc_rdy_ext2_parse {
	struct ath12k_wmi_svc_rdy_ext2_arg arg;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	struct ath12k_wmi_enh_aoa_caps_arg enh_aoa_caps;
	bool dma_ring_cap_done;
	bool spectral_bin_scaling_done;
	bool mac_phy_caps_ext_done;
	bool scan_radio_caps_done;
	bool hal_reg_caps_ext2_done;
	bool twt_caps_param_done;
	bool htt_msdu_idx_to_qtype_map;
	bool wmi_dbs_or_sbs_cap_ext;
	bool cus_bdf_ver_cap;
	bool soft_calib_support;
	bool wmi_coex_fix_channel_cap;
	bool wmi_aux_dev_cap;
	bool aoa_caps_exchange;
	bool aoa_per_band_caps;
	bool wmi_sar_flag_tlv_param;
	bool wmi_power_boost_cap;
	bool wmi_rssi_accuracy_improvement_cap;
	bool ltf_cap;
	bool chain_cap;
	bool mac_phy_caps_ext2_done;
	bool cu_mem_cfg_done;
};

struct ath12k_wmi_rdy_parse {
	u32 num_extra_mac_addr;
};

struct ath12k_wmi_dma_buf_release_arg {
	struct ath12k_wmi_dma_buf_release_fixed_params fixed;
	const struct ath12k_wmi_dma_buf_release_entry_params *buf_entry;
	const struct ath12k_wmi_dma_buf_release_meta_data_params *meta_data;
	u32 num_buf_entry;
	u32 num_meta;
	bool buf_entry_done;
	bool meta_data_done;
};

struct wmi_pdev_sscan_fw_param_parse {
	struct ath12k_wmi_pdev_sscan_fw_cmd_fixed_param fixed;
	struct ath12k_wmi_pdev_sscan_fft_bin_index *bin;
	struct ath12k_wmi_pdev_sscan_chan_info ch_info;
	struct ath12k_wmi_pdev_sscan_per_detector_info *det_info;
	bool bin_entry_done;
	bool det_info_entry_done;
	bool ch_info_valid;
	u32 num_fft_bin_index;
	u32 num_det_info;

};

struct wmi_spectral_capabilities_parse {
	struct ath12k_wmi_spectral_scan_bw_capabilities *sscan_bw_caps;
	struct ath12k_wmi_spectral_fft_size_capabilities *fft_size_caps;
	bool sscan_bw_caps_entry_done;
	bool fft_size_caps_entry_done;
	u32 num_bw_caps_entry;
	u32 num_fft_size_caps_entry;
};

struct ath12k_wmi_tlv_policy {
	size_t min_len;
};

struct wmi_tlv_mgmt_rx_parse {
	const struct ath12k_wmi_mgmt_rx_params *fixed;
	const u8 *frame_buf;
	bool frame_buf_done;
	struct ath12k_mgmt_rx_cu_arg cu_params;
	struct ath12k_wmi_mgmt_rx_mlo_link_removal_info
		**link_removal_info;
	u32 num_link_removal_info_count;
	bool mgmt_ml_info_done;
	bool bpcc_buf_done;
	bool parse_link_removal_info_done;
	struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info
		**bcast_ttlm_info;
	u32 num_bcast_ttlm_info_count;
	bool parse_bcast_ttlm_info_done;
};

struct ath12k_wmi_anomaly_parse {
	const struct wmi_anomaly_report_hdr *hdr;
	const void *ctx_ptr;
	u16 ctx_len;
	bool ctx_done;
};

static void
ath12k_wmi_delete_all_peer_resp_event(struct ath12k_base *ab, struct sk_buff *skb);

static void
ath12k_wmi_gpio_input_event(struct ath12k_base *ab, struct sk_buff *skb);

static void
ath12k_wmi_event_peer_sta_ps_state_chg(struct ath12k_base *ab, struct sk_buff *skb);

static void
ath12k_wmi_pdev_uhr_cu_event(struct ath12k_base *ab, struct sk_buff *skb);

static const struct ath12k_wmi_tlv_policy ath12k_wmi_tlv_policies[] = {
	[WMI_TAG_ARRAY_BYTE] = { .min_len = 0 },
	[WMI_TAG_ARRAY_UINT32] = { .min_len = 0 },
	[WMI_TAG_SERVICE_READY_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_event) },
	[WMI_TAG_SERVICE_READY_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_ext_event) },
	[WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS] = {
		.min_len = sizeof(struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params) },
	[WMI_TAG_SOC_HAL_REG_CAPABILITIES] = {
		.min_len = sizeof(struct ath12k_wmi_soc_hal_reg_caps_params) },
	[WMI_TAG_HAL_REG_CAPABILITIES_EXT2] = {
		.min_len = sizeof(struct ath12k_wmi_hal_reg_caps_ext2_params) -
			   sizeof(__le32) },
	[WMI_TAG_VDEV_START_RESPONSE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_start_resp_event) },
	[WMI_TAG_PEER_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_peer_delete_resp_event) },
	[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_bcn_tx_status_event) },
	[WMI_TAG_VDEV_STOPPED_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_stopped_event) },
	[WMI_TAG_VDEV_TPC_IE_POWER_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_tpc_ie_power_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_reg_chan_list_cc_ext_event) },
#ifdef CPTCFG_QCN_EXTN
	[WMI_TAG_REG_CHAN_PRIORITY] = {
		.min_len = sizeof(struct ath12k_wmi_reg_chan_priority) },
	[WMI_TAG_HW_BLACKLIST_CHAN_FIXED_PARAM] = {
		.min_len = sizeof(struct wmi_hw_blacklist_chan_fixed_param) },
	[WMI_TAG_HW_BLACKLIST_CHAN_DATA] = {
		.min_len = sizeof(struct wmi_hw_blacklist_chan_data) },
#endif
	[WMI_TAG_MGMT_RX_HDR] = {
		.min_len = sizeof(struct ath12k_wmi_mgmt_rx_params) },
	[WMI_TAG_MGMT_TX_COMPL_EVENT] = {
		.min_len = sizeof(struct wmi_mgmt_tx_compl_event) },
	[WMI_TAG_SCAN_EVENT] = {
		.min_len = sizeof(struct wmi_scan_event) },
	[WMI_TAG_PEER_STA_KICKOUT_EVENT] = {
		.min_len = sizeof(struct wmi_peer_sta_kickout_event) },
	[WMI_TAG_PDEV_UHR_CU_EVENT_FIXED_PARAM] = {
		.min_len = sizeof(struct wmi_pdev_uhr_cu_event_fixed_param) },
	[WMI_TAG_VDEV_UHR_CU_STATUS] = {
		.min_len = sizeof(struct wmi_vdev_uhr_cu_status) },
	[WMI_TAG_ROAM_EVENT] = {
		.min_len = sizeof(struct wmi_roam_event) },
	[WMI_TAG_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_chan_info_event) },
	[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_bss_chan_info_event) },
	[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_install_key_compl_event) },
	[WMI_TAG_READY_EVENT] = {
		.min_len = sizeof(struct ath12k_wmi_ready_event_min_params) },
	[WMI_TAG_SERVICE_AVAILABLE_EVENT] = {
		.min_len = sizeof(struct wmi_service_available_event) },
	[WMI_TAG_PEER_ASSOC_CONF_EVENT] = {
		.min_len = sizeof(struct wmi_peer_assoc_conf_event) },
	[WMI_TAG_RFKILL_EVENT] = {
		.min_len = sizeof(struct wmi_rfkill_state_change_event) },
	[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_ctl_failsafe_chk_event) },
	[WMI_TAG_PDEV_CHECK_CAL_VERSION_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_check_cal_version_event) },
	[WMI_TAG_PDEV_SET_TGTR2P_TABLE_EVENT_FIXED_PARAM] = {
		.min_len = sizeof(struct wmi_pdev_set_tgtr2p_table_event) },
	[WMI_TAG_HOST_SWFDA_EVENT] = {
		.min_len = sizeof(struct wmi_fils_discovery_event) },
	[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_probe_resp_tx_status_event) },
	[WMI_TAG_VDEV_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_delete_resp_event) },
	[WMI_TAG_TWT_ENABLE_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_enable_event) },
	[WMI_TAG_TWT_DISABLE_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_disable_event) },
	[WMI_TAG_P2P_NOA_INFO] = {
		.min_len = sizeof(struct ath12k_wmi_p2p_noa_info) },
	[WMI_TAG_P2P_NOA_EVENT] = {
		.min_len = sizeof(struct wmi_p2p_noa_event) },
	[WMI_TAG_PER_CHAIN_RSSI_STATS] = {
		.min_len = sizeof(struct wmi_per_chain_rssi_stat_params) },
	[WMI_TAG_11D_NEW_COUNTRY_EVENT] = {
		.min_len = sizeof(struct wmi_11d_new_cc_event) },
	[WMI_TAG_PEER_CREATE_RESP_EVENT] = {
		.min_len = sizeof(struct ath12k_wmi_peer_create_conf_ev) },
	[WMI_TAG_MUEDCA_PARAMS_CONFIG_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_update_muedca_event) },
	[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_add_dialog_event) },
	[WMI_TAG_OBSS_COLOR_COLLISION_EVT]
		= { .min_len = sizeof(struct wmi_obss_color_collision_event) },
	[WMI_TAG_TWT_BTWT_INVITE_STA_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_btwt_invite_sta_event) },
	[WMI_TAG_TWT_BTWT_REMOVE_STA_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_twt_btwt_invite_sta_event) },
	[WMI_TAG_OFFCHAN_DATA_TX_COMPL_EVENT] = {
		.min_len = sizeof(struct wmi_offchan_data_tx_compl_event) },
	[WMI_TAG_PEER_STA_PS_STATECHANGE_EVENT] = {
		.min_len = sizeof(struct wmi_peer_sta_ps_state_chg_event) },
#ifdef CPTCFG_QCN_EXTN
	[WMI_TAG_PEER_RATECODE_LIST_EVENT] = {
		.min_len = sizeof(struct wmi_peer_ratecode_list_fixed_param) },
#endif
};

__le32 ath12k_wmi_tlv_hdr(u32 cmd, u32 len)
{
	return le32_encode_bits(cmd, WMI_TLV_TAG) |
		le32_encode_bits(len, WMI_TLV_LEN);
}

__le32 ath12k_wmi_tlv_cmd_hdr(u32 cmd, u32 len)
{
	return ath12k_wmi_tlv_hdr(cmd, len - TLV_HDR_SIZE);
}

#define PRIMAP(_hw_mode_) \
	[_hw_mode_] = _hw_mode_##_PRI

static const int ath12k_hw_mode_pri_map[] = {
	PRIMAP(WMI_HOST_HW_MODE_SINGLE),
	PRIMAP(WMI_HOST_HW_MODE_DBS),
	PRIMAP(WMI_HOST_HW_MODE_SBS_PASSIVE),
	PRIMAP(WMI_HOST_HW_MODE_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_OR_SBS),
	/* keep last */
	PRIMAP(WMI_HOST_HW_MODE_MAX),
};

/**
 * ath12k_fcc_radar_types - Radar type table for the FCC (United States) domain.
 *
 * Maps each FCC radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_FCC].
 */
static const struct radar_type_id_to_name ath12k_fcc_radar_types[] = {
	{ 0x001, "TYPE_0" },
	{ 0x002, "TYPE_1" },
	{ 0x004, "TYPE_2" },
	{ 0x008, "TYPE_3" },
	{ 0x010, "TYPE_4" },
	{ 0x020, "TYPE_5" },
	{ 0x040, "TYPE_6" },
};

/**
 * ath12k_etsi301_radar_types - Radar type table for the ETSI EN 301 893 domain.
 *
 * Maps each ETSI EN 301 893 radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_ETSI_301].
 */
static const struct radar_type_id_to_name ath12k_etsi301_radar_types[] = {
	{ 0x001, "TYPE_1" },
	{ 0x002, "TYPE_2" },
	{ 0x004, "TYPE_3" },
	{ 0x008, "TYPE_4" },
	{ 0x010, "TYPE_52" },
	{ 0x020, "TYPE_62" },
	{ 0x003, "TYPE_1_2" },
	{ 0x023, "TYPE_1_2_6" },
};

/**
 * ath12k_etsi302_radar_types - Radar type table for the ETSI EN 302 502 domain.
 *
 * Maps each ETSI EN 302 502 radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_ETSI_302].
 */
static const struct radar_type_id_to_name ath12k_etsi302_radar_types[] = {
	{ 0x001, "TYPE_1" },
	{ 0x002, "TYPE_2" },
	{ 0x004, "TYPE_3" },
	{ 0x008, "TYPE_4" },
	{ 0x010, "TYPE_5" },
	{ 0x020, "TYPE_6" },
	{ 0x003, "TYPE_1_2" },
};

/**
 * ath12k_china_radar_types - Radar type table for the China DFS domain.
 *
 * Maps each China radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_CHINA].
 */
static const struct radar_type_id_to_name ath12k_china_radar_types[] = {
	{ 0x001, "TYPE_1" },
	{ 0x002, "TYPE_2" },
	{ 0x004, "TYPE_3" },
	{ 0x008, "TYPE_4" },
	{ 0x010, "TYPE_5" },
	{ 0x020, "TYPE_6" },
	{ 0x003, "TYPE_1_2" },
	{ 0x023, "TYPE_1_2_6" },
	{ 0x00C, "TYPE_3_4" },
};

/**
 * ath12k_korea_types - Radar type table for the Korea DFS domain.
 *
 * Maps each Korea radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_KOREA].
 */
static const struct radar_type_id_to_name ath12k_korea_types[] = {
	{ 0x001, "TYPE_1" },
	{ 0x002, "TYPE_2" },
	{ 0x004, "TYPE_3" },
	{ 0x008, "TYPE_4" },
};

/**
 * ath12k_japanW53_radar_types - Radar type table for the Japan W53 DFS domain.
 *
 * Maps each Japan W53 radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_JAPAN_W53].
 */
static const struct radar_type_id_to_name ath12k_japanW53_radar_types[] = {
	{ 0x001, "FIXED_PULSE_1" },
	{ 0x002, "FIXED_PULSE_2" },
	{ 0x004, "VAR_PULSE_3" },
	{ 0x008, "VAR_PULSE_4" },
};

/**
 * ath12k_japan_W56_radar_types - Radar type table for the Japan W56 DFS domain.
 *
 * Maps each Japan W56 radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_JAPAN_W56].
 */
static const struct radar_type_id_to_name ath12k_japan_W56_radar_types[] = {
	{ 0x001, "FIXED_PULSE_1" },
	{ 0x002, "FIXED_PULSE_2" },
	{ 0x004, "FIXED_PULSE_3" },
	{ 0x008, "VAR_PULSE_4" },
	{ 0x010, "VAR_PULSE_5" },
	{ 0x020, "VAR_PULSE_6" },
	{ 0x040, "CHIRP" },
	{ 0x003, "FIXED_PULSE_1_2" },
};

/**
 * ath12k_japan_W564_radar_types - Radar type table for the Japan W56.4 DFS domain.
 *
 * Maps each Japan W56.4 radar waveform bitmask to its type name.
 * Indexed via radar_domain_to_type_arr_tab[WMI_RADAR_DOMAIN_JAPAN_W564].
 */
static const struct radar_type_id_to_name ath12k_japan_W564_radar_types[] = {
	{ 0x001, "HOPPING" },
};

/**
 * radar_domain_to_type_arr_tab - Maps each radar domain to its type table.
 *
 * Indexed by enum wmi_radar_domain.  Each entry points to the per-domain
 * radar_type_id_to_name array and its length.
 * WMI_RADAR_DOMAIN_UNDEFINED has a zero-initialised entry.
 */
static const struct radar_domain_to_type_arr radar_domain_to_type_arr_tab[] = {
	[WMI_RADAR_DOMAIN_FCC]        = { "FCC", ath12k_fcc_radar_types,
					   ARRAY_SIZE(ath12k_fcc_radar_types) },
	[WMI_RADAR_DOMAIN_ETSI_301]   = { "ETSI_301", ath12k_etsi301_radar_types,
					   ARRAY_SIZE(ath12k_etsi301_radar_types) },
	[WMI_RADAR_DOMAIN_ETSI_302]   = { "ETSI_302", ath12k_etsi302_radar_types,
					   ARRAY_SIZE(ath12k_etsi302_radar_types) },
	[WMI_RADAR_DOMAIN_CHINA]      = { "CHINA", ath12k_china_radar_types,
					   ARRAY_SIZE(ath12k_china_radar_types) },
	[WMI_RADAR_DOMAIN_KOREA]      = { "KOREA", ath12k_korea_types,
					   ARRAY_SIZE(ath12k_korea_types) },
	[WMI_RADAR_DOMAIN_JAPAN_W53]  = { "JAPAN_W53", ath12k_japanW53_radar_types,
					   ARRAY_SIZE(ath12k_japanW53_radar_types) },
	[WMI_RADAR_DOMAIN_JAPAN_W56]  = { "JAPAN_W56", ath12k_japan_W56_radar_types,
					   ARRAY_SIZE(ath12k_japan_W56_radar_types) },
	[WMI_RADAR_DOMAIN_JAPAN_W564] = { "JAPAN_W564", ath12k_japan_W564_radar_types,
					   ARRAY_SIZE(ath12k_japan_W564_radar_types) },
};

/* Default string used when radar domain or type is unrecognized. */
static const char *const unknown_str = "UNKNOWN";

enum ath12k_type_req_ctrl_path_stats_id {
       TYPE_REQ_CTRL_PATH_PDEV_TX_STAT = 0,
       TYPE_REQ_CTRL_PATH_VDEV_EXTD_STAT,
       TYPE_REQ_CTRL_PATH_MEM_STAT,
       TYPE_REQ_CTRL_PATH_TWT_STAT,
       TYPE_REQ_CTRL_PATH_BMISS_STAT,
       TYPE_REQ_CTRL_PATH_PMLO_STAT,
       TYPE_REQ_CTRL_PATH_RRM_STA_STAT,
};

const char *mgmt_frame_name[] = { "AssocReq",
			    "AssocResp",
			    "ReAssocReq",
			    "ReAssocResp",
			    "ProbeReq",
			    "ProbeResp",
			     0, 0,
			    "Beacon",
			     0,
			    "DisAssoc",
			    "Auth",
			    "Deauth",
			    "Action"};

int ath12k_wmi_pdev_enable_telemetry_stats(struct ath12k_base *ab,
	struct ath12k *ar,
	struct wmi_request_ctrl_path_stats_cmd_fixed_param *param)
{
       struct ath12k_wmi_pdev *wmi = ar->wmi;
       enum ath12k_type_req_ctrl_path_stats_id req_id = TYPE_REQ_CTRL_PATH_PMLO_STAT;
	struct wmi_request_ctrl_path_stats_cmd_fixed_param *cmd;
       u32 pdev_id_array;
       u32 num_pdev_ids = 1;
       int len, ret;
       struct wmi_tlv *tlv;
       struct sk_buff *skb;
       void *ptr;

	len = sizeof(struct wmi_request_ctrl_path_stats_cmd_fixed_param) +
		TLV_HDR_SIZE + (sizeof(u32) * (num_pdev_ids));

       skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
       if (!skb)
               return -ENOMEM;

       cmd = (void *)skb->data;
       cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_CTRL_PATH_STATS_CMD_FIXED_PARAM,
                                                sizeof(*cmd));

       cmd->stats_id_mask = (1 << WMI_REQ_CTRL_PATH_PMLO_STAT);
	if (!param) {
		cmd->request_id = req_id;
		cmd->action = WMI_REQUEST_CTRL_PATH_STAT_PERIODIC_PUBLISH;
		cmd->subid = ATH12K_WMI_SUBID_PERIODICITY_1_SEC;
	} else {
		cmd->request_id = param->request_id;
		cmd->action = param->action;
		cmd->subid = param->subid;
	}

       pdev_id_array = ar->pdev->pdev_id;

       ptr = skb->data + sizeof(*cmd);

       tlv = ptr;
       tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
               FIELD_PREP(WMI_TLV_LEN, sizeof(u32) * num_pdev_ids);
       ptr += TLV_HDR_SIZE;
       memcpy(ptr, &pdev_id_array, sizeof(pdev_id_array));

       ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_CTRL_PATH_STATS_CMDID);
       if (ret) {
               dev_kfree_skb(skb);
               ath12k_warn(ab, "Failed to send WMI_REQUEST_CTRL_PATH_STATS_CMDID: %d", ret);
       }

       return ret;
}


enum wmi_host_channel_width
ath12k_wmi_get_host_chan_width(u32 width)
{
	enum wmi_host_channel_width host_width;

	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		host_width = WMI_HOST_CHAN_WIDTH_20;
		break;
	case NL80211_CHAN_WIDTH_40:
		host_width = WMI_HOST_CHAN_WIDTH_40;
		break;
	case NL80211_CHAN_WIDTH_80:
		host_width = WMI_HOST_CHAN_WIDTH_80;
		break;
	case NL80211_CHAN_WIDTH_160:
		host_width = WMI_HOST_CHAN_WIDTH_160;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		host_width = WMI_HOST_CHAN_WIDTH_80P80;
		break;
	case NL80211_CHAN_WIDTH_320:
		host_width = WMI_HOST_CHAN_WIDTH_320;
		break;
	default:
		host_width = WMI_HOST_CHAN_WIDTH_MAX;
		break;
	}

	return host_width;
}

#ifndef CPTCFG_QCN_EXTN
static
#endif
int
ath12k_wmi_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
		    int (*iter)(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data),
		    void *data)
{
	const void *begin = ptr;
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	int ret;


	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "wmi tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}

		tlv = ptr;
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}

		if (tlv_tag < ARRAY_SIZE(ath12k_wmi_tlv_policies) &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%u bytes is less than min length %zu)\n",
				   tlv_tag, ptr - begin, tlv_len,
				   ath12k_wmi_tlv_policies[tlv_tag].min_len);
			return -EINVAL;
		}

		ret = iter(ab, tlv_tag, tlv_len, ptr, data);
		if (ret)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}

	return 0;
}

static int ath12k_wmi_tlv_iter_parse(struct ath12k_base *ab, u16 tag, u16 len,
				     const void *ptr, void *data)
{
	const void **tb = data;

	if (tag < WMI_TAG_MAX)
		tb[tag] = ptr;

	return 0;
}

static int ath12k_wmi_tlv_parse(struct ath12k_base *ar, const void **tb,
				const void *ptr, size_t len)
{
	return ath12k_wmi_tlv_iter(ar, ptr, len, ath12k_wmi_tlv_iter_parse,
				   (void *)tb);
}

#ifndef CPTCFG_QCN_EXTN
static
#endif
const void **
ath12k_wmi_tlv_parse_alloc(struct ath12k_base *ab,
			   struct sk_buff *skb, gfp_t gfp)
{
	const void **tb;
	int ret;

	tb = kcalloc(WMI_TAG_MAX, sizeof(*tb), gfp);
	if (!tb)
		return ERR_PTR(-ENOMEM);

	ret = ath12k_wmi_tlv_parse(ab, tb, skb->data, skb->len);
	if (ret) {
		kfree(tb);
		return ERR_PTR(ret);
	}

	return tb;
}

static int ath12k_wmi_cmd_send_nowait(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
				      u32 cmd_id)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_cmd_hdr *cmd_hdr;
	int ret;

	if (!skb_push(skb, sizeof(struct wmi_cmd_hdr)))
		return -ENOMEM;

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	cmd_hdr->cmd_id = le32_encode_bits(cmd_id, WMI_CMD_HDR_CMD_ID);
	WMI_COMMAND_RECORD(wmi, skb, cmd_id);

	memset(skb_cb, 0, sizeof(*skb_cb));
	ret = ath12k_htc_send(&ab->htc, wmi->eid, skb);

	if (ret)
		goto err_pull;

	return 0;

err_pull:
	skb_pull(skb, sizeof(struct wmi_cmd_hdr));
	return ret;
}

int ath12k_wmi_cmd_send(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
			u32 cmd_id)
{
	struct ath12k_wmi_base *wmi_ab = wmi->wmi_ab;
	struct ath12k_base *ab = wmi_ab->ab;
	int ret = -EOPNOTSUPP;

	if (!(test_bit(ATH12K_FLAG_WMI_INIT_DONE, &wmi_ab->ab->dev_flags)) &&
		cmd_id != WMI_INIT_CMDID)
		return -ESHUTDOWN;

	if (!ab->ag->wsi_remap_in_progress && ab->is_bypassed)
		return -EPERM;

	might_sleep();

	if (ab->hw_params->credit_flow) {
		wait_event_timeout(wmi_ab->tx_credits_wq, ({
			ret = ath12k_wmi_cmd_send_nowait(wmi, skb, cmd_id);

			if (ret && test_bit(ATH12K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;
			(ret != -EAGAIN);
			}), WMI_SEND_TIMEOUT_HZ);
	} else {
		wait_event_timeout(wmi->tx_ce_desc_wq, ({
			ret = ath12k_wmi_cmd_send_nowait(wmi, skb, cmd_id);
			if (ret && test_bit(ATH12K_FLAG_CRASH_FLUSH,
					    &wmi_ab->ab->dev_flags))
				ret = -ESHUTDOWN;

			(ret != -ENOBUFS);
			}), WMI_SEND_TIMEOUT_HZ);
	}

	if (ret == -EAGAIN)
		ath12k_warn(wmi_ab->ab, "wmi command %d timeout\n", cmd_id);

	if (ret == -ENOBUFS)
		ath12k_warn(wmi_ab->ab, "ce desc not available for wmi command %d\n",
			    cmd_id);

	return ret;
}

static int ath12k_pull_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				     const void *ptr,
				     struct ath12k_wmi_service_ext_arg *arg)
{
	struct ath12k_base *ab = wmi_handle->wmi_ab->ab;
	const struct wmi_service_ready_ext_event *ev = ptr;
	int i;

	if (!ev)
		return -EINVAL;

	/* Move this to host based bitmap */
	arg->default_conc_scan_config_bits =
		le32_to_cpu(ev->default_conc_scan_config_bits);
	arg->default_fw_config_bits = le32_to_cpu(ev->default_fw_config_bits);
	arg->he_cap_info = le32_to_cpu(ev->he_cap_info);
	arg->mpdu_density = le32_to_cpu(ev->mpdu_density);
	arg->max_bssid_rx_filters = le32_to_cpu(ev->max_bssid_rx_filters);
	arg->ppet.numss_m1 = le32_to_cpu(ev->ppet.numss_m1);
	arg->ppet.ru_bit_mask = le32_to_cpu(ev->ppet.ru_info);

	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		arg->ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ev->ppet.ppet16_ppet8_ru3_ru0[i]);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi svc rdy ext hdl_version %u\n",
		   le32_get_bits(ev->fw_build_vers_ext, WMI_HDL_VERSION));

	return 0;
}

static int
ath12k_pull_mac_phy_cap_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				      struct ath12k_wmi_svc_rdy_ext_parse *svc,
				      u8 hw_mode_id, u8 phy_id,
				      struct ath12k_pdev *pdev)
{
	const struct ath12k_wmi_mac_phy_caps_params *mac_caps;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps = svc->hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *wmi_hw_mode_caps = svc->hw_mode_caps;
	const struct ath12k_wmi_mac_phy_caps_params *wmi_mac_phy_caps = svc->mac_phy_caps;
	struct ath12k_base *ab = wmi_handle->wmi_ab->ab;
	struct ath12k_band_cap *cap_band;
	struct ath12k_pdev_cap *pdev_cap = &pdev->cap;
	struct ath12k_fw_pdev *fw_pdev;
	u32 supported_bands;
	u32 phy_map;
	u32 hw_idx, phy_idx = 0;
	int i;

	if (!hw_caps || !wmi_hw_mode_caps || !svc->soc_hal_reg_caps)
		return -EINVAL;

	for (hw_idx = 0; hw_idx < le32_to_cpu(hw_caps->num_hw_modes); hw_idx++) {
		if (hw_mode_id == le32_to_cpu(wmi_hw_mode_caps[hw_idx].hw_mode_id))
			break;

		phy_map = le32_to_cpu(wmi_hw_mode_caps[hw_idx].phy_id_map);
		phy_idx = fls(phy_map);
	}

	if (hw_idx == le32_to_cpu(hw_caps->num_hw_modes))
		return -EINVAL;

	phy_idx += phy_id;
	if (phy_id >= le32_to_cpu(svc->soc_hal_reg_caps->num_phy))
		return -EINVAL;

	if (phy_idx >= svc->n_mac_phy_caps)
		return -EINVAL;

	mac_caps = wmi_mac_phy_caps + phy_idx;
	supported_bands = le32_to_cpu(mac_caps->supported_bands);

	pdev->pdev_id = ath12k_wmi_mac_phy_get_pdev_id(mac_caps);
	pdev->hw_link_id = ath12k_wmi_mac_phy_get_hw_link_id(mac_caps);
	pdev_cap->supported_bands |= supported_bands;
	pdev_cap->ampdu_density = le32_to_cpu(mac_caps->ampdu_density);
	pdev_cap->chainmask_table_id = mac_caps->chainmask_table_id;

	if (ab->fw_pdev_count >= ARRAY_SIZE(ab->fw_pdev)) {
		ath12k_warn(ab,
			    "fw_pdev array full (%zu entries), dropping pdev_id %u phy_id %u\n",
			    ARRAY_SIZE(ab->fw_pdev),
			    ath12k_wmi_mac_phy_get_pdev_id(mac_caps),
			    le32_to_cpu(mac_caps->phy_id));
		return -ENOSPC;
	}

	fw_pdev = &ab->fw_pdev[ab->fw_pdev_count];
	fw_pdev->supported_bands = supported_bands;
	fw_pdev->pdev_id = ath12k_wmi_mac_phy_get_pdev_id(mac_caps);
	fw_pdev->phy_id = le32_to_cpu(mac_caps->phy_id);
	ab->fw_pdev_count++;

	if (!(supported_bands & WMI_HOST_WLAN_2GHZ_CAP) &&
	    !(supported_bands & WMI_HOST_WLAN_5GHZ_CAP))
		return -EINVAL;

	/* Take non-zero tx/rx chainmask. If tx/rx chainmask differs from
	 * band to band for a single radio, need to see how this should be
	 * handled.
	 */

	if (supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		pdev_cap->vht_cap = le32_to_cpu(mac_caps->vht_cap_info_2g);
		pdev_cap->vht_mcs = le32_to_cpu(mac_caps->vht_supp_mcs_2g);
#ifdef CPTCFG_QCN_EXTN
		/* Extract per-SS MCS10/11 bitmap from bits [23:16] of vht_supp_mcs.
		 * Bit 24 is the validity flag; only store when it is set.
		 */
		if (pdev_cap->vht_mcs & BIT(24))
			pdev_cap->vht_higher_mcs_supp =
				(pdev_cap->vht_mcs >> 16) & 0xff;
#endif
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_2g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_2g);
	}

	if (supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		pdev_cap->vht_cap = le32_to_cpu(mac_caps->vht_cap_info_5g);
		pdev_cap->vht_mcs = le32_to_cpu(mac_caps->vht_supp_mcs_5g);
#ifdef CPTCFG_QCN_EXTN
		if (pdev_cap->vht_mcs & BIT(24))
			pdev_cap->vht_higher_mcs_supp =
				(pdev_cap->vht_mcs >> 16) & 0xff;
#endif
		pdev_cap->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_5g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_5g);
		pdev_cap->nss_ratio_enabled =
			WMI_NSS_RATIO_EN_DIS_GET(mac_caps->nss_ratio);
		pdev_cap->nss_ratio_info =
			WMI_NSS_RATIO_INFO_GET(mac_caps->nss_ratio);
	}

	/* tx/rx chainmask reported from fw depends on the actual hw chains used,
	 * For example, for 4x4 capable macphys, first 4 chains can be used for first
	 * mac and the remaining 4 chains can be used for the second mac or vice-versa.
	 * In this case, tx/rx chainmask 0xf will be advertised for first mac and 0xf0
	 * will be advertised for second mac or vice-versa. Compute the shift value
	 * for tx/rx chainmask which will be used to advertise supported ht/vht rates to
	 * mac80211.
	 */
	pdev_cap->tx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->tx_chain_mask, 32);
	pdev_cap->rx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->rx_chain_mask, 32);

	if (supported_bands & WMI_HOST_WLAN_2GHZ_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_2GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported = le32_to_cpu(mac_caps->max_bw_supported_2g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_2g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_2g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_2g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_2g);
#ifdef CPTCFG_QCN_EXTN
		cap_band->he_cap_info_internal =
			le32_to_cpu(mac_caps->he_cap_info_internal);
#endif
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_2g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet2g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet2g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet2g.ppet16_ppet8_ru3_ru0[i]);
	}

	if (supported_bands & WMI_HOST_WLAN_5GHZ_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_5GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
#ifdef CPTCFG_QCN_EXTN
		cap_band->he_cap_info_internal =
			le32_to_cpu(mac_caps->he_cap_info_internal);
#endif
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);

		cap_band = &pdev_cap->band[NL80211_BAND_6GHZ];
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);
	}

	return 0;
}

/*
 * ath12k_mvr_ch_switch_notify_work - Deferred work to send CH_SWITCH_NOTIFY
 * @work: work struct embedded in struct ath12k
 *
 * cfg80211_ch_switch_notify() acquires the wiphy mutex (a sleeping lock) and
 * must not be called from BH/softirq context.  The WMI MVR event handler runs
 * in softirq context (CE receive callback -> HTC -> WMI op_rx), so we defer
 * the notification to this work item which runs in process context.
 */
void ath12k_mvr_ch_switch_notify_work(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 mvr_ch_switch_notify_work);
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *arvif;
	struct wireless_dev *wdev;
	u32 vdev_bitmap, bit_pos;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	vdev_bitmap = ar->mvr_ch_switch_notify_vdev_bm;
	for (bit_pos = 0; bit_pos < 32; bit_pos++) {
		if (!(vdev_bitmap & BIT(bit_pos)))
			continue;

		rcu_read_lock();
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, bit_pos);
		if (!arvif || !arvif->ahvif || !arvif->ahvif->vif) {
			rcu_read_unlock();
			continue;
		}
		wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
		if (!wdev || !wdev->netdev) {
			rcu_read_unlock();
			continue;
		}
		/*
		 * arvif and wdev remain valid after rcu_read_unlock() because
		 * wiphy_lock is held — interface deletion requires wiphy_lock,
		 * so no vdev can be removed while we hold it.
		 */
		rcu_read_unlock();

		cfg80211_ch_switch_notify(wdev->netdev,
					  &arvif->chanctx.def,
					  arvif->link_id);
		spin_lock_bh(&ar->data_lock);
		ar->chanctx_switch_stats.nl_notify_count++;
		spin_unlock_bh(&ar->data_lock);

		/*
		 * For scan radio, SET_TPC WMI command is deferred until after
		 * the channel change completion response (MVR event) is received
		 * from FW. Send it here now that the channel change is complete.
		 */
		ath12k_mac_set_tpc_power(ar, arvif);
	}
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
}

static void ath12k_wmi_process_mvr_event(struct ath12k_base *ab, u32 *vdev_id_bm,
					 u32 num_vdev_bm)
{
	struct ath12k *ar = NULL;
	struct ath12k_link_vif *arvif = NULL;
	u32 vdev_bitmap, bit_pos, w;
	u64 switch_time_us, avg_switch_time_us = 0, mvr_resp_time_us;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi mvr resp num_vdev_bm %d vdev_id_bm[0]=0x%x vdev_id_bm[1]=0x%x\n",
		   num_vdev_bm, vdev_id_bm[0],
		   (num_vdev_bm == WMI_MVR_RESP_VDEV_BM_MAX_LEN ?
				   vdev_id_bm[1] : 0x00));

	if (num_vdev_bm > WMI_MVR_RESP_VDEV_BM_MAX_LEN) {
		ath12k_warn(ab, "wmi mvr resp: num_vdev_bm %d exceeds max %d, clamping\n",
			    num_vdev_bm, WMI_MVR_RESP_VDEV_BM_MAX_LEN);
		num_vdev_bm = WMI_MVR_RESP_VDEV_BM_MAX_LEN;
	}

	for (w = 0; w < num_vdev_bm; w++) {
		vdev_bitmap = vdev_id_bm[w];

		for (bit_pos = 0; bit_pos < 32; bit_pos++) {
			u32 vdev_id = w * 32 + bit_pos;

			if (!(vdev_bitmap & BIT(bit_pos)))
				continue;

			arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
			if (!arvif) {
				ath12k_warn(ab, "wmi mvr resp for unknown vdev %d",
					    vdev_id);
				continue;
			}

			arvif->mvr_processing = false;
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "wmi mvr vdev %d restarted\n", vdev_id);
		}
	}

	if (arvif)
		ar = arvif->ar;

	mvr_resp_time_us = ath12k_get_timestamp_in_us();

	if (ar)
		complete(&ar->mvr_complete);
	else
		return; /* Skip channel change stats */

	/* Last channel switch time */
	switch_time_us = mvr_resp_time_us -
			 ar->chanctx_switch_stats.entry_time_us;

	/* More robust running average calculation to prevent overflow:
	 * new_avg = old_avg + (new_value - old_avg) / n
	 */
	if (ar->chanctx_switch_stats.total_switches > 1) {
		u64 old_avg = ar->chanctx_switch_stats.avg_switch_time_us;

		if (switch_time_us > old_avg)
			avg_switch_time_us = old_avg +
				div_u64(switch_time_us - old_avg,
					ar->chanctx_switch_stats.total_switches);
		else
			avg_switch_time_us = old_avg -
				div_u64(old_avg - switch_time_us,
					ar->chanctx_switch_stats.total_switches);
	} else {
		avg_switch_time_us = switch_time_us;
	}

	spin_lock_bh(&ar->data_lock);
	ar->chanctx_switch_stats.mvr_resp_time_us = mvr_resp_time_us;
	ar->chanctx_switch_stats.last_switch_time_us = switch_time_us;
	ar->chanctx_switch_stats.avg_switch_time_us = avg_switch_time_us;

	/* Update min/max statistics */
	if (ar->chanctx_switch_stats.min_switch_time_us == 0 ||
	    switch_time_us < ar->chanctx_switch_stats.min_switch_time_us)
		ar->chanctx_switch_stats.min_switch_time_us = switch_time_us;

	if (switch_time_us > ar->chanctx_switch_stats.max_switch_time_us)
		ar->chanctx_switch_stats.max_switch_time_us = switch_time_us;

	spin_unlock_bh(&ar->data_lock);

	/*
	 * For scan radio, channel change is async (non-blocking MVR).
	 * Send NL80211_CMD_CH_SWITCH_NOTIFY from the driver upon MVR
	 * response, since the regular mac80211 CSA path is skipped.
	 * cfg80211_ch_switch_notify() acquires wiphy mutex (sleeping lock)
	 * so defer to a work item - we are in BH/softirq context here.
	 */
	if (ath12k_scan_radio_supported(ar->pdev)) {
		ar->mvr_ch_switch_notify_vdev_bm = vdev_id_bm[0];
		schedule_work(&ar->mvr_ch_switch_notify_work);
	}
}

static int ath12k_wmi_tlv_mvr_event_parse(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct wmi_pdev_mvr_resp_event_parse *parse = data;
	struct wmi_pdev_mvr_resp_event_fixed_param *fixed_param;

	switch(tag) {
	case WMI_TAG_MULTIPLE_VDEV_RESTART_RESPONSE_EVENT:
		fixed_param = (struct wmi_pdev_mvr_resp_event_fixed_param *)ptr;

		if (fixed_param->status) {
			ath12k_warn(ab, "wmi mvr resp event status %u\n",
				    fixed_param->status);
			return -EINVAL;
		}

		memcpy(&parse->fixed_param, fixed_param,
		       sizeof(struct wmi_pdev_mvr_resp_event_fixed_param));
		break;
	case WMI_TAG_ARRAY_UINT32:
		if ((len > WMI_MVR_RESP_VDEV_BM_MAX_LEN_BYTES) || (len == 0)) {
			ath12k_warn(ab, "wmi invalid vdev id len in mvr resp %u\n",
				    len);
			return -EINVAL;
		}

		parse->num_vdevs_bm = len / sizeof(u32);
		memcpy(parse->vdev_id_bm, ptr, len);
		break;
	default:
		break;
	}

	return 0;
}

static void ath12k_wmi_event_mvr_response(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct wmi_pdev_mvr_resp_event_parse parse = {};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mvr_event_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "wmi failed to parse mvr response tlv %d\n",
			    ret);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi mvr resp for pdev %d\n",
		   parse.fixed_param.pdev_id);

	ath12k_wmi_process_mvr_event(ab, parse.vdev_id_bm, parse.num_vdevs_bm);
}

static int
ath12k_pull_reg_cap_svc_rdy_ext(struct ath12k_wmi_pdev *wmi_handle,
				const struct ath12k_wmi_soc_hal_reg_caps_params *reg_caps,
				const struct ath12k_wmi_hal_reg_caps_ext_params *ext_caps,
				u8 phy_idx,
				struct ath12k_wmi_hal_reg_capabilities_ext_arg *param)
{
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_reg_cap;

	if (!reg_caps || !ext_caps)
		return -EINVAL;

	if (phy_idx >= le32_to_cpu(reg_caps->num_phy))
		return -EINVAL;

	ext_reg_cap = &ext_caps[phy_idx];

	param->phy_id = le32_to_cpu(ext_reg_cap->phy_id);
	param->eeprom_reg_domain = le32_to_cpu(ext_reg_cap->eeprom_reg_domain);
	param->eeprom_reg_domain_ext =
		le32_to_cpu(ext_reg_cap->eeprom_reg_domain_ext);
	param->regcap1 = le32_to_cpu(ext_reg_cap->regcap1);
	param->regcap2 = le32_to_cpu(ext_reg_cap->regcap2);
	/* check if param->wireless_mode is needed */
	param->low_2ghz_chan = le32_to_cpu(ext_reg_cap->low_2ghz_chan);
	param->high_2ghz_chan = le32_to_cpu(ext_reg_cap->high_2ghz_chan);
	param->low_5ghz_chan = le32_to_cpu(ext_reg_cap->low_5ghz_chan);
	param->high_5ghz_chan = le32_to_cpu(ext_reg_cap->high_5ghz_chan);
	ath12k_info(wmi_handle->wmi_ab->ab, "%s: low_2ghz_chan : %u high_2ghz_chan : %u low_5ghz_chan : %u high_5ghz_chan : %u\n",
		    __func__, param->low_2ghz_chan, param->high_2ghz_chan,
		    param->low_5ghz_chan, param->high_5ghz_chan);

	return 0;
}

static int ath12k_pull_service_ready_tlv(struct ath12k_base *ab,
					 const void *evt_buf,
					 struct ath12k_wmi_target_cap_arg *cap)
{
	const struct wmi_service_ready_event *ev = evt_buf;

	if (!ev) {
		ath12k_err(ab, "%s: failed by NULL param\n",
			   __func__);
		return -EINVAL;
	}

	cap->phy_capability = le32_to_cpu(ev->phy_capability);
	cap->max_frag_entry = le32_to_cpu(ev->max_frag_entry);
	cap->num_rf_chains = le32_to_cpu(ev->num_rf_chains);
	cap->ht_cap_info = le32_to_cpu(ev->ht_cap_info);
	cap->vht_cap_info = le32_to_cpu(ev->vht_cap_info);
	cap->vht_supp_mcs = le32_to_cpu(ev->vht_supp_mcs);
	cap->hw_min_tx_power = le32_to_cpu(ev->hw_min_tx_power);
	cap->hw_max_tx_power = le32_to_cpu(ev->hw_max_tx_power);
	cap->sys_cap_info = le32_to_cpu(ev->sys_cap_info);
	cap->min_pkt_size_enable = le32_to_cpu(ev->min_pkt_size_enable);
	cap->max_bcn_ie_size = le32_to_cpu(ev->max_bcn_ie_size);
	cap->max_num_scan_channels = le32_to_cpu(ev->max_num_scan_channels);
	cap->max_supported_macs = le32_to_cpu(ev->max_supported_macs);
	cap->wmi_fw_sub_feat_caps = le32_to_cpu(ev->wmi_fw_sub_feat_caps);
	cap->txrx_chainmask = le32_to_cpu(ev->txrx_chainmask);
	cap->default_dbs_hw_mode_index = le32_to_cpu(ev->default_dbs_hw_mode_index);
	cap->num_msdu_desc = le32_to_cpu(ev->num_msdu_desc);

	return 0;
}

/* Save the wmi_service_bitmap into a linear bitmap. The wmi_services in
 * wmi_service ready event are advertised in b0-b3 (LSB 4-bits) of each
 * 4-byte word.
 */
static void ath12k_wmi_service_bitmap_copy(struct ath12k_wmi_pdev *wmi,
					   const u32 *wmi_svc_bm)
{
	int i, j;

	for (i = 0, j = 0; i < WMI_SERVICE_BM_SIZE && j < WMI_MAX_SERVICE; i++) {
		do {
			if (wmi_svc_bm[i] & BIT(j % WMI_SERVICE_BITS_IN_SIZE32))
				set_bit(j, wmi->wmi_ab->svc_map);
		} while (++j % WMI_SERVICE_BITS_IN_SIZE32);
	}
}

static int ath12k_wmi_svc_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_svc_ready_parse *svc_ready = data;
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	u16 expect_len;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EVENT:
		if (ath12k_pull_service_ready_tlv(ab, ptr, &ab->target_caps))
			return -EINVAL;
		break;

	case WMI_TAG_ARRAY_UINT32:
		if (!svc_ready->wmi_svc_bitmap_done) {
			expect_len = WMI_SERVICE_BM_SIZE * sizeof(u32);
			if (len < expect_len) {
				ath12k_warn(ab, "invalid len %d for the tag 0x%x\n",
					    len, tag);
				return -EINVAL;
			}

			ath12k_wmi_service_bitmap_copy(wmi_handle, ptr);

			svc_ready->wmi_svc_bitmap_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_svc_ready_parse svc_ready = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_parse,
				  &svc_ready);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	return 0;
}

static u32 ath12k_wmi_mgmt_get_freq(struct ath12k *ar,
				    struct ieee80211_tx_info *info)
{
	struct ath12k_base *ab = ar->ab;
	u32 freq = 0;

	if (ab->hw_params->single_pdev_only &&
	    ar->scan.is_roc &&
	    (info->flags & IEEE80211_TX_CTL_TX_OFFCHAN))
		freq = ar->scan.roc_freq;

	return freq;
}

struct sk_buff *ath12k_wmi_alloc_skb(struct ath12k_wmi_base *wmi_ab, u32 len)
{
	struct sk_buff *skb;
	struct ath12k_base *ab = wmi_ab->ab;
	u32 round_len = roundup(len, 4);

	skb = ath12k_htc_alloc_skb(ab, WMI_SKB_HEADROOM + round_len);
	if (!skb)
		return NULL;

	skb_reserve(skb, WMI_SKB_HEADROOM);
	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		ath12k_warn(ab, "unaligned WMI skb data\n");

	skb_put(skb, round_len);
	memset(skb->data, 0, round_len);

	return skb;
}

static bool ath12k_get_skb_rate(struct ath12k_link_vif *arvif,
				struct sk_buff *frame,
				u16 *mcs, u8 *preamble)
{
	struct ath12k_skb_tx_info *skb_tx_info;
	bool rate_valid = false;
	int err;

	rcu_read_lock();
	skb_tx_info = ath12k_get_skb_tx_info(arvif->ar, frame);
	if (!skb_tx_info) {
		rcu_read_unlock();
		return false;
	}

	err = ath12k_tx_rate_info(arvif, &skb_tx_info->rate,
				  mcs, preamble);
	rcu_read_unlock();
	if (!err)
		rate_valid = true;

	ath12k_skb_rhash_remove(arvif->ar, frame);

	return rate_valid;
}

static bool ath12k_wmi_hw_link_id_in_mgmt_send(struct ath12k *ar)
{
	const struct ath12k_hw_ops *hw_ops = ar->ab->hw_params->hw_ops;

	if (hw_ops->hw_link_id_required_in_mgmt_send)
		return hw_ops->hw_link_id_required_in_mgmt_send(ar->ab);

	return false;
}

int ath12k_wmi_mgmt_send(struct ath12k *ar, u32 vdev_id, u32 buf_id,
			 struct sk_buff *frame, bool mlo_params_valid,
			 bool link_agnostic, bool is_cfr)
{
	struct ath12k_link_vif *arvif = ath12k_mac_get_arvif(ar, vdev_id);
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_mgmt_send_cmd *cmd;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(frame);
	struct ath12k_skb_cb *drv_info = ATH12K_SKB_CB(frame);
	struct wmi_mlo_mgmt_send_params *ml_params;
	struct wmi_mgmt_send_params *params;
	bool tx_params_valid = false;
	struct wmi_tlv *frame_tlv;
	bool hw_link_id_needed;
	struct sk_buff *skb;
	bool rate_present;
	u32 buf_len;
	int ret, len;
	u8 preamble;
	void *ptr;
	struct wmi_tlv *tlv;
	u16 mcs;
#ifdef CPTCFG_QCN_EXTN
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(frame);
#endif /* CPTCFG_QCN_EXTN */

	if (!arvif)
		return -EINVAL;

	buf_len = min_t(int, frame->len, WMI_MGMT_SEND_DOWNLD_LEN);

	len = sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, sizeof(u32));

	rate_present = ath12k_get_skb_rate(arvif, frame, &mcs, &preamble);
#ifdef CPTCFG_QCN_EXTN
	if (is_cfr || rate_present ||
	    ATH12K_CUSTOM_TX_PARAM_CONFIGURED_EXTN(skb_cb->u.ar))
		tx_params_valid = true;
#else
	if (is_cfr || rate_present)
		tx_params_valid = true;
#endif /* CPTCFG_QCN_EXTN */

	hw_link_id_needed = ath12k_wmi_hw_link_id_in_mgmt_send(ar);

	if (tx_params_valid || mlo_params_valid) {
		len += sizeof(*params);
		if (mlo_params_valid)
			len += TLV_HDR_SIZE + sizeof(*ml_params);
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mgmt_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MGMT_TX_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->desc_id = cpu_to_le32(buf_id);
	cmd->chanfreq = cpu_to_le32(ath12k_wmi_mgmt_get_freq(ar, info));
	cmd->paddr_lo = cpu_to_le32(lower_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->paddr_hi = cpu_to_le32(upper_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->frame_len = cpu_to_le32(frame->len);
	cmd->buf_len = cpu_to_le32(buf_len);
	cmd->tx_params_valid = tx_params_valid;

	if (drv_info->flags & ATH12K_SKB_MGMT_SMD_HI_PRI)
		cmd->tx_flags |= WMI_TX_MGMT_HI_PRIO_FLAG;

	frame_tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	frame_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, roundup(buf_len, sizeof(u32)));

	memcpy(frame_tlv->value, frame->data, buf_len);

	if (!tx_params_valid && !mlo_params_valid)
		goto send;

	ptr = skb->data + sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, sizeof(u32));

	tlv = ptr;

	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TX_SEND_PARAMS) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(struct wmi_mgmt_send_params) - TLV_HDR_SIZE);

	if (tx_params_valid) {
		params = ptr;

		if (rate_present) {
			params->tx_param_dword0 =
				le32_encode_bits(BIT(mcs),
						 WMI_TX_PARAMS_DWORD0_MCS_MASK) |
				/* always 1 nss */
				le32_encode_bits(BIT(0),
						 WMI_TX_PARAMS_DWORD0_NSS_MASK);
			params->tx_param_dword1 =
				le32_encode_bits(BIT(preamble),
						 WMI_TX_PARAMS_DWORD1_PREAMBLE_TYPE);
		}

		/* WMI_TX_PARAMS_DWORD1_CFR_CAPTURE should be set
		 * only when CFR is enabled.
		 */
		if (is_cfr)
			params->tx_param_dword1 |= WMI_TX_PARAMS_DWORD1_CFR_CAPTURE;

#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_prepare_tx_params_extn(skb_cb, params);
#endif /* CPTCFG_QCN_EXTN */
	}

	ptr += sizeof(struct wmi_mgmt_send_params);

	tlv = ptr;

	if (mlo_params_valid) {
		tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
			      FIELD_PREP(WMI_TLV_LEN, sizeof(*ml_params));
		ptr += TLV_HDR_SIZE;

		ml_params = ptr;
		ml_params->tlv_header = FIELD_PREP(WMI_TLV_TAG,
						   WMI_TAG_MLO_TX_SEND_PARAMS) |
					FIELD_PREP(WMI_TLV_LEN,
						   sizeof(*ml_params) - TLV_HDR_SIZE);

		/* Link-specific frames should be sent with hw_link_id while link-agnostic
		 * frames should be sent with invalid value.
		 */
		if (ath12k_hw_group_recovery_in_progress(ar->ab->ag) &&
		    ar->ab->ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
			ml_params->hw_link_id = ar->pdev->hw_link_id;
		} else if (hw_link_id_needed) {
			ml_params->hw_link_id = link_agnostic ? WMI_MLO_MGMT_TID :
						ar->pdev->hw_link_id;
		} else {
			ml_params->hw_link_id = WMI_MLO_MGMT_TID;
		}
	}
send:
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MGMT_TX_SEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MGMT_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_stats_request_cmd(struct ath12k *ar, u32 stats_id,
				      u32 vdev_id, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_request_stats_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_request_stats_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REQUEST_STATS_CMD,
						 sizeof(*cmd));

	cmd->stats_id = cpu_to_le32(stats_id);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_STATS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_REQUEST_STATS cmd\n");
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI request stats 0x%x vdev id %d pdev id %d\n",
		   stats_id, vdev_id, pdev_id);

	return ret;
}

/* For Big Endian Host, Copy Engine byte_swap is enabled
 * When Copy Engine does byte_swap, need to byte swap again for the
 * Host to get/put buffer content in the correct byte order
 */
void ath12k_ce_byte_swap(void *mem, u32 len)
{
	int i;

	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) {
		if (!mem)
			return;
		for (i = 0; i < (len / 4); i++) {
			*(u32 *)mem = swab32(*(u32 *)mem);
			mem += 4;
		}
	}
}

/* Send off-channel managemnt frame to firmware. when driver receive a
 * packet with off channel tx flag enabled. This API will send the
 * packet to firmware with WMI command WMI_TAG_OFFCHAN_DATA_TX_SEND_CMD
 * for off-chan tx.
 */
int ath12k_wmi_offchan_mgmt_send(struct ath12k *ar, u32 vdev_id, u32 buf_id,
				 struct sk_buff *frame)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(frame);
	struct wmi_mgmt_send_cmd *cmd;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len, buf_len_padded;
	int ret, len;
	void *ptr;
	struct wmi_tlv *tlv;

	buf_len = min(frame->len, WMI_MGMT_SEND_DOWNLD_LEN);
	buf_len_padded = roundup(buf_len, sizeof(u32));

	len = sizeof(*cmd) + sizeof(*frame_tlv) + buf_len_padded +
	      sizeof(*tlv) + sizeof(struct wmi_mgmt_send_params);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OFFCHAN_DATA_TX_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->desc_id = cpu_to_le32(buf_id);
	cmd->chanfreq = cpu_to_le32(ath12k_wmi_mgmt_get_freq(ar, info));
	cmd->paddr_lo = cpu_to_le32(lower_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->paddr_hi = cpu_to_le32(upper_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->frame_len = cpu_to_le32(frame->len);
	cmd->buf_len = cpu_to_le32(buf_len);
	cmd->tx_params_valid = 1;
	ptr += sizeof(*cmd);

	frame_tlv = ptr;
	frame_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, buf_len_padded);
	ptr += sizeof(*frame_tlv);

	memcpy(ptr, frame->data, buf_len);
	ath12k_ce_byte_swap(ptr, buf_len);
	ptr += buf_len_padded;

	tlv = ptr;
	/* Tx params not used currently */
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TX_SEND_PARAMS,
					     sizeof(struct wmi_mgmt_send_params));

#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_prepare_tx_params_extn(ATH12K_SKB_CB(frame), ptr);
#endif /* CPTCFG_QCN_EXTN */

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_OFFCHAN_DATA_TX_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_OFFCHAN_DATA_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_create(struct ath12k *ar, u8 *macaddr,
			   struct ath12k_wmi_vdev_create_arg *args)
{
	struct ath12k_wmi_vdev_txrx_streams_params *txrx_streams;
	struct wmi_vdev_create_cu_mem_offset_info *offset_info;
	bool is_ml_vdev = is_valid_ether_addr(args->mld_addr);
	struct wmi_vdev_create_mlo_params *ml_params;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_create_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	int ret, len;
	void *ptr;

	/* It can be optimized my sending tx/rx chain configuration
	 * only for supported bands instead of always sending it for
	 * both the bands.
	 */
	len = sizeof(*cmd) + TLV_HDR_SIZE +
	      (WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams)) +
	      TLV_HDR_SIZE + (is_ml_vdev ? sizeof(*ml_params) : 0) +
	      TLV_HDR_SIZE +
	      TLV_HDR_SIZE +
	      (args->cu_mem_info.size ? sizeof(*offset_info) : 0);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CREATE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(args->if_id);
	cmd->vdev_type = cpu_to_le32(args->type);
	cmd->vdev_subtype = cpu_to_le32(args->subtype);
	cmd->num_cfg_txrx_streams = cpu_to_le32(WMI_NUM_SUPPORTED_BAND_MAX);
	cmd->pdev_id = cpu_to_le32(args->pdev_id);
	cmd->mbssid_flags = cpu_to_le32(args->mbssid_flags);
	cmd->mbssid_tx_vdev_id = cpu_to_le32(args->mbssid_tx_vdev_id);
	cmd->vdev_stats_id = cpu_to_le32(args->if_stats_id);

	cmd->is_cfp_enabled = cpu_to_le32(args->is_cfp_enabled);

	cmd->global_vdev_id = cpu_to_le32(args->global_vdev_id);

	ether_addr_copy(cmd->vdev_macaddr.addr, macaddr);

	if (args->if_stats_id != ATH12K_INVAL_VDEV_STATS_ID)
		cmd->vdev_stats_id_valid = cpu_to_le32(BIT(0));

	ptr = skb->data + sizeof(*cmd);
	len = WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

	ptr += TLV_HDR_SIZE;
	txrx_streams = ptr;
	len = sizeof(*txrx_streams);
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = cpu_to_le32(WMI_TPC_CHAINMASK_CONFIG_BAND_2G);
	txrx_streams->supported_tx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_2GHZ].tx);
	txrx_streams->supported_rx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_2GHZ].rx);

	txrx_streams++;
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = cpu_to_le32(WMI_TPC_CHAINMASK_CONFIG_BAND_5G);
	txrx_streams->supported_tx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_5GHZ].tx);
	txrx_streams->supported_rx_streams =
				cpu_to_le32(args->chains[NL80211_BAND_5GHZ].rx);

	ptr += WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

	if (is_ml_vdev) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*ml_params));
		ptr += TLV_HDR_SIZE;
		ml_params = ptr;

		ml_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_VDEV_CREATE_PARAMS,
					       sizeof(*ml_params));
		ether_addr_copy(ml_params->mld_macaddr.addr, args->mld_addr);
		ptr += sizeof(*ml_params);
	} else {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	 /* wfdr2_mode placeholder TLV (empty, not used by host) */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	/* Add TBTT countdown offset info TLV if caller requested and AP type */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 args->cu_mem_info.size ?
					 sizeof(*offset_info) : 0);
	ptr += TLV_HDR_SIZE;

	if (args->cu_mem_info.size) {
		offset_info = ptr;
		offset_info->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SHARED_MEM_TBTT_OFFSET_INFO,
					       sizeof(*offset_info));
		offset_info->cu_mem_addr_lsb =
			cpu_to_le32(args->cu_mem_info.cu_mem_addr_lsb);
		offset_info->cu_mem_addr_msb =
			cpu_to_le32(args->cu_mem_info.cu_mem_addr_msb);
		offset_info->size = cpu_to_le32(args->cu_mem_info.size);

		offset_info->offset_info_1 = cpu_to_le32(args->cu_mem_info.offset1);
		offset_info->offset_info_2 = cpu_to_le32(args->cu_mem_info.offset2);
		offset_info->offset_info_3 = cpu_to_le32(args->cu_mem_info.offset3);
		offset_info->offset_info_4 = cpu_to_le32(args->cu_mem_info.offset4);
		offset_info->offset_info_5 = cpu_to_le32(args->cu_mem_info.offset5);

		ptr += sizeof(*offset_info);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "[radio_idx : %u] WMI vdev create cu_mem: vdev %d host_addr 0x%08x_%08x size %u off1 0x%08x off2 0x%08x off3 0x%08x off4 0x%08x off5 0x%08x\n",
			   ar->radio_idx, args->if_id,
			   args->cu_mem_info.cu_mem_addr_msb,
			   args->cu_mem_info.cu_mem_addr_lsb,
			   args->cu_mem_info.size,
			   args->cu_mem_info.offset1,
			   args->cu_mem_info.offset2,
			   args->cu_mem_info.offset3,
			   args->cu_mem_info.offset4,
			   args->cu_mem_info.offset5);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "[radio_idx : %u] WMI vdev create: id %d type %d subtype %d macaddr %pM pdevid %d vdev bitmap (allocate:0x%llx free:0x%llx) global_vdev_id:%d\n",
		   ar->radio_idx,
		   args->if_id, args->type, args->subtype, macaddr, args->pdev_id,
		   ar->allocated_vdev_map, ar->ab->free_vdev_map, cmd->global_vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit WMI_VDEV_CREATE_CMDID\n",
			    args->if_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_delete(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DELETE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "[radio_idx : %u] WMI vdev delete id %d num_peers : %d vdev bitmap (allocate:0x%llx free:0x%llx)\n",
		   ar->radio_idx,
		   vdev_id, ar->num_peers, ar->allocated_vdev_map,
		   ar->ab->free_vdev_map);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit WMI_VDEV_DELETE_CMDID\n",
			    vdev_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_stop(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_stop_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_STOP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev stop id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_STOP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit WMI_VDEV_STOP cmd\n",
			    vdev_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_down(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_down_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_down_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DOWN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev down id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DOWN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[radio_idx : %u] failed to submit WMI_VDEV_DOWN cmd vdev id : %d\n",
			    ar->radio_idx, vdev_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

static inline bool
ath12k_wmi_check_device_present(u32 width_device,
				u32 center_freq_device,
				u32 center_freq_oper)
{
	return (center_freq_device && width_device &&
		center_freq_device != center_freq_oper);
}

/* Fill mhz, band_center_freq1/2, and the PHY-mode bits of chan->info.
 *
 * Pass non-zero cf_device/width_device for the SW-prog-DFS device-width path;
 * pass 0/0 for the standard BSS-width path (mode and band_center_freq1 are
 * used directly in that case).
 */
void
ath12k_wmi_put_channel_info(struct ath12k_wmi_channel_params *chan,
			    u32 freq, u32 band_center_freq1,
			    u32 band_center_freq2,
			    enum wmi_phy_mode mode,
			    u32 cf_device, u32 width_device)
{
	u32 cf1;

	chan->mhz = cpu_to_le32(freq);

	if (cf_device && width_device) {
		/* Device-width path: resolve mode and base centre freq from
		 * the device channel width.
		 */
		switch (width_device) {
		case NL80211_CHAN_WIDTH_320:
			mode = MODE_11BE_EHT320;
			break;
		case NL80211_CHAN_WIDTH_160:
			mode = MODE_11BE_EHT160;
			break;
		case NL80211_CHAN_WIDTH_80:
			mode = MODE_11BE_EHT80;
			break;
		case NL80211_CHAN_WIDTH_40:
			mode = MODE_11BE_EHT40;
			break;
		default:
			mode = MODE_UNKNOWN;
			break;
		}
		cf1 = cf_device;
	} else {
		/* BSS-width path: mode and centre freq already resolved by
		 * the caller.
		 */
		cf1 = band_center_freq1;
	}

	/* Adjust band_center_freq1 and set band_center_freq2 for bandwidths
	 * that split the spectrum into two 80 MHz segments.
	 */
	if (mode == MODE_11BN_UHR320 || mode == MODE_11BE_EHT320) {
		chan->band_center_freq1 =
			cpu_to_le32(freq > cf1 ? cf1 + 80 : cf1 - 80);
		chan->band_center_freq2 = cpu_to_le32(cf1);
	} else if (mode == MODE_11BN_UHR160 || mode == MODE_11BE_EHT160) {
		chan->band_center_freq1 =
			cpu_to_le32(freq > cf1 ? cf1 + 40 : cf1 - 40);
		chan->band_center_freq2 = cpu_to_le32(cf1);
	} else {
		chan->band_center_freq1 = cpu_to_le32(cf1);
		chan->band_center_freq2 = 0;
	}

	chan->info |= le32_encode_bits(mode, WMI_CHAN_INFO_MODE);
}

static void ath12k_wmi_set_wmi_channel_device(struct ath12k_wmi_channel_params *chan_device,
					      struct wmi_vdev_start_req_arg *channel,
					      u32 cf_device, u32 width_device)
{
	memset(chan_device, 0, sizeof(*chan_device));

	chan_device->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
							 sizeof(*chan_device));

	ath12k_wmi_put_channel_info(chan_device, channel->freq,
				    channel->band_center_freq1,
				    channel->band_center_freq2,
				    MODE_UNKNOWN, cf_device, width_device);

	if (channel->passive)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
	if (channel->allow_ibss)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ADHOC_ALLOWED);
	if (channel->allow_ht)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
	if (channel->allow_vht)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
	if (channel->allow_he)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
	if (channel->ht40plus)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_HT40_PLUS);
	if (channel->chan_radar)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);
	if (channel->freq2_radar)
		chan_device->info |= cpu_to_le32(WMI_CHAN_INFO_DFS_FREQ2);

	chan_device->reg_info_1 = le32_encode_bits(channel->max_power,
						   WMI_CHAN_REG_INFO1_MAX_PWR) |
				  le32_encode_bits(channel->max_reg_power,
						   WMI_CHAN_REG_INFO1_MAX_REG_PWR);

	chan_device->reg_info_2 = le32_encode_bits(channel->max_antenna_gain,
						   WMI_CHAN_REG_INFO2_ANT_MAX) |
				  le32_encode_bits(channel->max_power,
						   WMI_CHAN_REG_INFO2_MAX_TX_PWR);
}

static void ath12k_wmi_put_wmi_channel(struct ath12k_wmi_channel_params *chan,
				       struct wmi_vdev_start_req_arg *arg)
{
	memset(chan, 0, sizeof(*chan));

	ath12k_wmi_put_channel_info(chan, arg->freq,
				    arg->band_center_freq1,
				    arg->band_center_freq2,
				    arg->mode, 0, 0);

	if (arg->passive)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
	if (arg->allow_ibss)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ADHOC_ALLOWED);
	if (arg->allow_ht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
	if (arg->allow_vht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
	if (arg->allow_he)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
	if (arg->ht40plus)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_HT40_PLUS);
	if (arg->chan_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);
	if (arg->freq2_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS_FREQ2);
	if (arg->is_stadfs_en)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_STA_DFS);

	chan->reg_info_1 = le32_encode_bits(arg->max_power,
					    WMI_CHAN_REG_INFO1_MAX_PWR) |
		le32_encode_bits(arg->max_reg_power,
				 WMI_CHAN_REG_INFO1_MAX_REG_PWR);

	chan->reg_info_2 = le32_encode_bits(arg->max_antenna_gain,
					    WMI_CHAN_REG_INFO2_ANT_MAX) |
		le32_encode_bits(arg->max_power, WMI_CHAN_REG_INFO2_MAX_TX_PWR);
}

int ath12k_wmi_vdev_start(struct ath12k *ar, struct wmi_vdev_start_req_arg *arg,
			  bool restart)
{
	struct ath12k_wmi_channel_params *chan_device;
	struct wmi_vdev_start_mlo_params *ml_params;
	struct wmi_vdev_start_smd_params *smd_params;
	struct wmi_partner_link_info *partner_info;
	struct wmi_uhr_ap_npca_params *npca_params;
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_start_request_cmd *cmd;
	bool device_params_present = false;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan;
	struct wmi_tlv *tlv;
	void *ptr;
	struct wmi_vdev_start_uhr_config *uhr_config;
	bool uhr_config_present = false;
	int ret, len, i, ml_arg_size = 0, smd_arg_size = 0;

	if (WARN_ON(arg->ssid_len > sizeof(cmd->ssid.ssid)))
		return -EINVAL;

	len = sizeof(*cmd) + sizeof(*chan) + TLV_HDR_SIZE;

	if (!restart && arg->ml.enabled) {
		ml_arg_size = TLV_HDR_SIZE + sizeof(*ml_params) +
			      TLV_HDR_SIZE + (arg->ml.num_partner_links *
					      sizeof(*partner_info));
		len += ml_arg_size;
	} else {
		len += 2 * TLV_HDR_SIZE;
	}

	device_params_present = ath12k_wmi_check_device_present(arg->width_device,
								arg->center_freq_device,
								arg->band_center_freq1);
	if (device_params_present)
		len += TLV_HDR_SIZE + sizeof(*chan_device);
	else
		len += TLV_HDR_SIZE;

	/* Reserve spaces for dbw_chan_info, vdev_start_smd_params
	 * and vdev_start_uhr_config.
	 */
	len += 3 * TLV_HDR_SIZE;

	if (!restart &&
	    arg->uhr_config.adv_notification_interval &&
	    arg->uhr_config.post_notification_interval &&
	    arg->uhr_config.update_in_tim_interval) {
		uhr_config_present = true;
		len += sizeof(*uhr_config);
	}

	if (!restart && arg->ml.enabled && arg->smd.enabled) {
		smd_arg_size = sizeof(*smd_params);
		len += smd_arg_size;
	}

	if (arg->npca.enabled)
		len += TLV_HDR_SIZE + sizeof(*npca_params);
	else
		len += TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_start_request_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_START_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->beacon_interval = cpu_to_le32(arg->bcn_intval);
	cmd->bcn_tx_rate = cpu_to_le32(arg->bcn_tx_rate);
	cmd->dtim_period = cpu_to_le32(arg->dtim_period);
	cmd->num_noa_descriptors = cpu_to_le32(arg->num_noa_descriptors);
	cmd->preferred_rx_streams = cpu_to_le32(arg->pref_rx_streams);
	cmd->preferred_tx_streams = cpu_to_le32(arg->pref_tx_streams);
	cmd->cac_duration_ms = cpu_to_le32(arg->cac_duration_ms);
	cmd->regdomain = cpu_to_le32(arg->regdomain);
	cmd->he_ops = cpu_to_le32(arg->he_ops);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);
	cmd->mbssid_flags = cpu_to_le32(arg->mbssid_flags);
	cmd->mbssid_tx_vdev_id = cpu_to_le32(arg->mbssid_tx_vdev_id);

	if (!restart) {
		if (arg->ssid) {
			cmd->ssid.ssid_len = cpu_to_le32(arg->ssid_len);
			memcpy(cmd->ssid.ssid, arg->ssid, arg->ssid_len);
		}
		if (arg->hidden_ssid)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_HIDDEN_SSID);
		if (arg->pmf_enabled)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_PMF_ENABLED);
	}

	cmd->flags |= cpu_to_le32(WMI_VDEV_START_LDPC_RX_ENABLED);
	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags))
		cmd->flags |= cpu_to_le32(WMI_VDEV_START_HW_ENCRYPTION_DISABLED);

	ptr = skb->data + sizeof(*cmd);
	chan = ptr;

	ath12k_wmi_put_wmi_channel(chan, arg);

	chan->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
						  sizeof(*chan));
	ptr += sizeof(*chan);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	/* Note: This is a nested TLV containing:
	 * [wmi_tlv][ath12k_wmi_p2p_noa_descriptor][wmi_tlv]..
	 */

	ptr += sizeof(*tlv);

	if (ml_arg_size) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*ml_params));
		ptr += TLV_HDR_SIZE;

		ml_params = ptr;

		ml_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_VDEV_START_PARAMS,
					       sizeof(*ml_params));

		ml_params->flags = le32_encode_bits(arg->ml.enabled,
						    ATH12K_WMI_FLAG_MLO_ENABLED) |
				   le32_encode_bits(arg->ml.assoc_link,
						    ATH12K_WMI_FLAG_MLO_ASSOC_LINK) |
				   le32_encode_bits(arg->ml.mcast_link,
						    ATH12K_WMI_FLAG_MLO_MCAST_VDEV) |
				   le32_encode_bits(arg->ml.link_add,
						    ATH12K_WMI_FLAG_MLO_LINK_ADD) |
				   le32_encode_bits(1,
						    ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID) |
				   le32_encode_bits(arg->ml.mlo_bridge_link,
						    ATH12K_WMI_FLAG_MLO_BRIDGE_LINK);
		ml_params->ieee_link_id = arg->ml.ieee_link_id;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "[radio_idx : %u] vdev %d start ml flags 0x%x ieee_link_id=%d\n",
			   ar->radio_idx,
			   arg->vdev_id, ml_params->flags, ml_params->ieee_link_id);

		ptr += sizeof(*ml_params);

		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 arg->ml.num_partner_links *
						 sizeof(*partner_info));
		ptr += TLV_HDR_SIZE;

		partner_info = ptr;

		for (i = 0; i < arg->ml.num_partner_links; i++) {
			partner_info->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PARTNER_LINK_PARAMS,
						       sizeof(*partner_info));
			partner_info->vdev_id =
				cpu_to_le32(arg->ml.partner_info[i].vdev_id);
			partner_info->hw_link_id =
				cpu_to_le32(arg->ml.partner_info[i].hw_link_id);
			ether_addr_copy(partner_info->vdev_addr.addr,
					arg->ml.partner_info[i].addr);
			partner_info->ieee_link_id = arg->ml.partner_info[i].logical_link_idx;
			partner_info->flags = le32_encode_bits(1,
							       ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID_PARTNER) |
					      le32_encode_bits(arg->ml.partner_info[i].mlo_bridge_link,
							       ATH12K_WMI_FLAG_MLO_BRIDGE_LINK);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "[radio_idx : %u] partner vdev %d hw_link_id %d macaddr%pM flags:0x%x\n",
				   ar->radio_idx,
				   partner_info->vdev_id, partner_info->hw_link_id,
				   partner_info->vdev_addr.addr, partner_info->flags);

			partner_info++;
		}

		ptr = partner_info;
	} else {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;

		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    device_params_present) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*chan_device));
		ptr += TLV_HDR_SIZE;

		chan_device = ptr;
		ath12k_wmi_set_wmi_channel_device(chan_device, arg,
						  arg->center_freq_device,
						  arg->width_device);
		ptr += sizeof(*chan_device);
	} else {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += sizeof(*tlv);
	}

	/* dbw_chan_info TLV */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += sizeof(*tlv);

	/* vdev_start_smd_params TLV */
	if (smd_arg_size) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*smd_params));
		ptr += TLV_HDR_SIZE;
		smd_params = ptr;
		smd_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SMD_VDEV_START_PARAMS,
					       sizeof(*smd_params));
		smd_params->flags =
			le32_encode_bits(arg->smd.enabled,
					 ATH12K_WMI_FLAG_SMD_ENABLED) |
			le32_encode_bits(arg->smd.dl_data_fwd,
					 ATH12K_WMI_FLAG_SMD_DL_DATA_FWD) |
			le32_encode_bits(arg->smd.ptk_mode,
					 ATH12K_WMI_FLAG_SMD_PTK_MODE);
		ether_addr_copy(smd_params->mac_addr.addr, arg->smd.smd_mac_addr);
		ptr += sizeof(*smd_params);

	} else {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}


	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 uhr_config_present ? sizeof(*uhr_config) : 0);
	ptr += TLV_HDR_SIZE;

	if (uhr_config_present) {
		uhr_config = ptr;
		uhr_config->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_START_UHR_CONFIG,
					       sizeof(*uhr_config));
		uhr_config->uhr_cu_intervals =
			le32_encode_bits(arg->uhr_config.adv_notification_interval,
					 WMI_UHR_CU_INTERVALS_ADV_NOTIF_MASK) |
			le32_encode_bits(arg->uhr_config.post_notification_interval,
					 WMI_UHR_CU_INTERVALS_POST_NOTIF_MASK) |
			le32_encode_bits(arg->uhr_config.update_in_tim_interval,
					 WMI_UHR_CU_INTERVALS_UPD_TIM_MASK);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_CU,
			   "vdev %u uhr_config adv_notif=%u post_notif=%u upd_tim=%u\n",
			   arg->vdev_id,
			   arg->uhr_config.adv_notification_interval,
			   arg->uhr_config.post_notification_interval,
			   arg->uhr_config.update_in_tim_interval);
		ptr += sizeof(*uhr_config);
	}

	/* uhr_ap_npca_params TLV */
	if (arg->npca.enabled) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "NPCA enabled in WMI\n");
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*npca_params));
		ptr += TLV_HDR_SIZE;

		npca_params = ptr;
		npca_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_NPCA_PARAMS,
					       sizeof(*npca_params));
		npca_params->vdev_id = cpu_to_le32(arg->vdev_id);
		npca_params->mode_tuple_field = cpu_to_le32(WMI_NPCA_MODE_ENABLE);

		/* Fill npca_chan same as chan_device, then override mhz
		 * with npca_freq
		 */
		if (device_params_present) {
			ath12k_wmi_set_wmi_channel_device(&npca_params->npca_chan,
							  arg,
							  arg->center_freq_device,
							  arg->width_device);
		} else {
			ath12k_wmi_put_wmi_channel(&npca_params->npca_chan, arg);
			npca_params->npca_chan.tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
						       sizeof(npca_params->npca_chan));
		}
		npca_params->npca_chan.mhz = cpu_to_le32(arg->npca.npca_freq);

		npca_params->puncture_20mhz_bitmap =
			cpu_to_le32(arg->npca.npca_punct_bitmap);
		npca_params->npca_cap1 =
			le32_encode_bits(arg->npca.npca_min_dur_threshold,
					 WMI_NPCA_CAP1_MIN_THRESHOLD) |
			le32_encode_bits(arg->npca.npca_switch_delay,
					 WMI_NPCA_CAP1_SWITCH_DELAY) |
			le32_encode_bits(arg->npca.npca_switch_back_delay,
					 WMI_NPCA_CAP1_SWITCH_BACK_DELAY) |
			le32_encode_bits(arg->npca.npca_initial_qsrc,
					 WMI_NPCA_CAP1_INITIAL_QSRC) |
			le32_encode_bits(arg->npca.npca_moplen,
					 WMI_NPCA_CAP1_MOPLEN);
		npca_params->npca_cap2 = 0;
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "npca params assign: vdev_id=%u npca_freq=%u punct_bitmap=0x%x min_dur=%u switch_delay=%u switch_back=%u init_qsrc=%u moplen=%u cap1=0x%x cap2=0x%x\n",
			   arg->vdev_id,
			   arg->npca.npca_freq,
			   arg->npca.npca_punct_bitmap,
			   arg->npca.npca_min_dur_threshold,
			   arg->npca.npca_switch_delay,
			   arg->npca.npca_switch_back_delay,
			   arg->npca.npca_initial_qsrc,
			   arg->npca.npca_moplen,
			   le32_to_cpu(npca_params->npca_cap1),
			   le32_to_cpu(npca_params->npca_cap2));
		ptr += sizeof(*npca_params);
	} else {
		/* keep TLV order when NPCA disabled */
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "[radio_idx : %u] vdev %s id 0x%x freq 0x%x mode 0x%x\n",
		   ar->radio_idx,
		   restart ? "restart" : "start", arg->vdev_id,
		   arg->freq, arg->mode);

	if (restart)
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_RESTART_REQUEST_CMDID);
	else
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_START_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit vdev_%s cmd\n",
			    arg->vdev_id, ar->radio_idx,
			    restart ? "restart" : "start");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_up(struct ath12k *ar, struct ath12k_wmi_vdev_up_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_up_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_up_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_UP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(params->vdev_id);
	cmd->vdev_assoc_id = cpu_to_le32(params->aid);

	ether_addr_copy(cmd->vdev_bssid.addr, params->bssid);

	if (params->tx_bssid) {
		ether_addr_copy(cmd->tx_vdev_bssid.addr, params->tx_bssid);
		cmd->nontx_profile_idx = cpu_to_le32(params->nontx_profile_idx);
		cmd->nontx_profile_cnt = cpu_to_le32(params->nontx_profile_cnt);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "[radio_idx : %u] WMI mgmt vdev up id 0x%x assoc id %d bssid %pM\n",
		   ar->radio_idx,
		   params->vdev_id, params->aid, params->bssid);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_UP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit WMI_VDEV_UP cmd\n",
			    params->vdev_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_tx_pn_request_cmd(struct ath12k *ar,
					   struct ath12k_wmi_peer_pn_arg *arg)
{
	struct wmi_peer_tx_pn_request_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_tx_pn_request_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_TX_PN_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->key_idx = cpu_to_le32(arg->key_idx);
	cmd->key_cipher = cpu_to_le32(arg->key_cipher);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_TX_PN_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_PEER_TX_PN_REQUEST cmd: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_create_cmd(struct ath12k *ar,
				    struct ath12k_wmi_peer_create_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_create_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;
	struct wmi_peer_create_mlo_params *ml_param;
	void *ptr;
	struct wmi_tlv *tlv;

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*ml_param);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CREATE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_addr);
	cmd->peer_type = cpu_to_le32(arg->peer_type);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	if (arg->peer_id != ATH12K_MLO_PEER_ID_INVALID)
		cmd->peer_id = cpu_to_le32(arg->peer_id);

	if (arg->sta_id != ATH12K_STA_ID_INVALID) {
		cmd->flags = cpu_to_le32(ATH12K_WMI_FLAG_STA_ID_VALID);
		cmd->sta_id = cpu_to_le32(arg->sta_id);
	}

	if (arg->epp_peer &&
	    test_bit(WMI_SERVICE_11BI_EPPKE_SUPPORT,
		     ar->ab->wmi_ab.svc_map))
		cmd->flags |=
			le32_encode_bits(1, WMI_PEER_CREATE_EPP_PEER_INDICATION);

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 sizeof(*ml_param));
	ptr += TLV_HDR_SIZE;
	ml_param = ptr;
	ml_param->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PEER_CREATE_PARAMS,
					       sizeof(*ml_param));
	ml_param->flags = le32_encode_bits(arg->ml_enabled,
					   ATH12K_WMI_FLAG_MLO_ENABLED) |
			  le32_encode_bits(arg->mlo_bridge_peer,
					   ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

	ptr += sizeof(*ml_param);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME, ATH12K_DBG_L1,
			 "[radio_idx : %u] WMI peer create vdev_id %d peer_addr %pM ml_flags 0x%x num_peer:%d bridge peer %d\n",
			 ar->radio_idx,
			 arg->vdev_id, arg->peer_addr, ml_param->flags,
			 ar->num_peers, arg->mlo_bridge_peer);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME, ATH12K_DBG_L1,
			 "[vdev_id : %u radio_idx : %u] WMI peer create peer_addr:%pM flags:%d peer_id:%d sta_id:%d\n",
			 arg->vdev_id, ar->radio_idx,
			 arg->peer_addr, cmd->flags, arg->peer_id, arg->sta_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to submit WMI_PEER_CREATE cmd\n",
			    arg->vdev_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_delete_cmd(struct ath12k *ar,
				    const u8 *peer_addr, u8 vdev_id,
				    u32 mlo_hw_link_id_bitmap,
				    bool peer_delete_send_mlo_hw_bitmap)
{
	struct ath12k_hw_group *ag = ar->ab->ag;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_delete_cmd *cmd;
	struct ath12k_wmi_peer_delete_mlo_params *mlo_params;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len;

	len = sizeof(*cmd) + sizeof(*mlo_params) + TLV_HDR_SIZE;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_peer_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_DELETE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ptr += sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*mlo_params));

	ptr += TLV_HDR_SIZE;
	mlo_params = ptr;
	mlo_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PARAMS_PEER_DELETE,
							sizeof(*mlo_params));

	if (ag && (ath12k_hw_group_recovery_in_progress(ag) ||
	    peer_delete_send_mlo_hw_bitmap) &&
	    mlo_hw_link_id_bitmap)
		mlo_hw_link_id_bitmap &= ~BIT(ar->pdev->hw_link_id);
	else
		mlo_hw_link_id_bitmap = 0;

	mlo_params->mlo_hw_link_id_bitmap = cpu_to_le32(mlo_hw_link_id_bitmap);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME, ATH12K_DBG_L0,
			 "[radio_idx : %u] WMI peer delete vdev_id %d peer_addr %pM num_peer : %d hw_link_id_bitmap 0x%x\n",
			 ar->radio_idx, vdev_id,  peer_addr,
			 ar->num_peers, mlo_hw_link_id_bitmap);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L0,
			 "[vdev_id : %u radio_idx : %u] WMI peer delete peer_delete_send_mlo_hw_bitmap: 0x%x\n",
			 vdev_id, ar->radio_idx, peer_delete_send_mlo_hw_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "[vdev_id : %u radio_idx : %u] failed to send WMI_PEER_DELETE cmd"
			   " peer_addr %pM num_peer : %d\n",
			    vdev_id, ar->radio_idx, peer_addr, ar->num_peers);
		dev_kfree_skb(skb);
	}

	return ret;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_wmi_config_peer_ppeds_routing(struct ath12k *ar,
					 const u8 *peer_addr, u8 vdev_id,
					 u32 service_code, u32 priority_valid,
					 u32 src_info, bool ppe_routing_enable,
					 bool use_ppe)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_peer_config_ppeds_cmd *cmd;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_config_ppeds_cmd *)skb->data;
	cmd->tlv_header  = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CONFIG_PPEDS_ROUTING,
						  sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	if (ppe_routing_enable) {
		if (use_ppe)
			cmd->ppe_routing_enable = WMI_AST_USE_PPE_ENABLED;
		else
			cmd->ppe_routing_enable = WMI_AST_USE_PPE_DISABLED;
	} else {
		cmd->ppe_routing_enable = WMI_PPE_ROUTING_DISABLED;
	}
	cmd->service_code = cpu_to_le32(service_code);
	cmd->priority_valid = cpu_to_le32(priority_valid);
	cmd->src_info = cpu_to_le32(src_info);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CONFIG_PPE_DS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_CONFIG_PPE_DS cmd" \
			   "peer_addr %pM num_peer : %d\n",
			    peer_addr, ar->num_peers);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ppe ds routing cmd peer_addr %pM vdev_id %d service_code %d " \
		   "priority_valid %d src_info %d ppe_routing_enable %d\n",
		   peer_addr, vdev_id, service_code, priority_valid,
		   src_info, ppe_routing_enable);

	return ret;
}
#endif

int ath12k_wmi_send_pdev_pkt_route(struct ath12k *ar,
				   struct ath12k_wmi_pkt_route_param *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_pkt_route_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_pkt_route_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_UPDATE_PKT_ROUTING_CMD) |
				     FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	cmd->pdev_id = ar->pdev->pdev_id;
	cmd->opcode = param->opcode;
	cmd->route_type_bmap = param->route_type_bmap;
	cmd->dst_ring = param->dst_ring;
	cmd->meta_data = param->meta_data;
	cmd->dst_ring_handler = param->dst_ring_handler;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "CCE PPE WMI pdev pkt route opcode %d route_bmap %d dst_ring %d meta_data %d" \
		   "dst_ring_handler %d\n", param->opcode, param->route_type_bmap,
		   param->dst_ring, param->meta_data, param->dst_ring_handler);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_UPDATE_PKT_ROUTING_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_UPDATE_PKT_ROUTING cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_set_regdomain(struct ath12k *ar,
				       struct ath12k_wmi_pdev_set_regdomain_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_regdomain_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_regdomain_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_REGDOMAIN_CMD,
						 sizeof(*cmd));

	cmd->reg_domain = cpu_to_le32(arg->current_rd_in_use);
	cmd->reg_domain_2g = cpu_to_le32(arg->current_rd_2g);
	cmd->reg_domain_5g = cpu_to_le32(arg->current_rd_5g);
	cmd->conformance_test_limit_2g = cpu_to_le32(arg->ctl_2g);
	cmd->conformance_test_limit_5g = cpu_to_le32(arg->ctl_5g);
	cmd->dfs_domain = cpu_to_le32(arg->dfs_domain);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev regd rd %d rd2g %d rd5g %d domain %d pdev id %d\n",
		   arg->current_rd_in_use, arg->current_rd_2g,
		   arg->current_rd_5g, arg->dfs_domain, arg->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_REGDOMAIN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_REGDOMAIN cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_peer_param(struct ath12k *ar, const u8 *peer_addr,
			      u32 vdev_id, u32 param_id, u32 param_val)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_SET_PARAM_CMD,
						 sizeof(*cmd));
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_val);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %d peer 0x%pM set param %d value %d\n",
		   vdev_id, peer_addr, param_id, param_val);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_set_mapc_params_cmd(struct ath12k *ar,
					const struct ath12k_wmi_peer_mapc_params_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_set_mapc_params_cmd_fixed_param *fixed;
	struct wmi_mapc_cmn_params *cmn;
	struct wmi_mapc_cmn_q2q_params *q2q;
	struct wmi_mapc_ctdma_profile *profile;
	struct wmi_mapc_ctdma_txop_sharing_policy *txop;
	struct wmi_mapc_cotdma_e2e_config *e2e;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret;
	size_t skb_len;
	bool has_cmn     = !!(arg->param_set_mask & ATH12K_WMI_MAPC_SET_CMN);
	bool has_q2q     = !!(arg->param_set_mask & ATH12K_WMI_MAPC_SET_Q2Q);
	bool has_profile = !!(arg->param_set_mask & ATH12K_WMI_MAPC_SET_PROFILE);
	bool has_txop    = !!(arg->param_set_mask & ATH12K_WMI_MAPC_SET_TXOP);
	bool has_e2e     = !!(arg->param_set_mask & ATH12K_WMI_MAPC_SET_E2E);

	/* fixed param is always present; each group is either 1-element or 0-element */
	skb_len = sizeof(*fixed);
	skb_len += TLV_HDR_SIZE + (has_cmn     ? sizeof(*cmn)     : 0);
	skb_len += TLV_HDR_SIZE + (has_q2q     ? sizeof(*q2q)     : 0);
	skb_len += TLV_HDR_SIZE + (has_profile ? sizeof(*profile) : 0);
	skb_len += TLV_HDR_SIZE + (has_txop    ? sizeof(*txop)    : 0);
	skb_len += TLV_HDR_SIZE + (has_e2e     ? sizeof(*e2e)     : 0);
	skb_len += 3 * TLV_HDR_SIZE; /* Co-SR, Co-BF, Co-rTWT: always 0-element stubs */

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, skb_len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	/* Fixed param — always present */
	fixed = ptr;
	fixed->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_SET_MAPC_PARAMS_CMD_FIXED_PARAM,
				       sizeof(*fixed));
	fixed->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(fixed->peer_macaddr.addr, arg->peer_addr);
	fixed->mapc_scheme_enable_bitmap = cpu_to_le32(WMI_MAPC_SCHEME_COTDMA);
	ptr += sizeof(*fixed);

	/* CMN group: capability bitmap + standard APID pair */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 has_cmn ? sizeof(*cmn) : 0);
	ptr += TLV_HDR_SIZE;
	if (has_cmn) {
		cmn = ptr;
		cmn->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MAPC_CMN_PARAMS,
							 sizeof(*cmn));
		cmn->mapc_capability_bitmap  = cpu_to_le32(arg->mapc_capability_bitmap);
		cmn->apid_to_neighbor_peer   = cpu_to_le32(arg->apid_to_neighbor_peer);
		cmn->apid_from_neighbor_peer = cpu_to_le32(arg->apid_from_neighbor_peer);
		ptr += sizeof(*cmn);
	}

	/* Slot 2: Q2Q group — QCA-to-QCA vendor APID pair */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 has_q2q ? sizeof(*q2q) : 0);
	ptr += TLV_HDR_SIZE;
	if (has_q2q) {
		q2q = ptr;
		q2q->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MAPC_CMN_Q2Q_PARAMS,
					       sizeof(*q2q));
		q2q->q2q_apid_to_neighbor_peer   =
			cpu_to_le32(arg->q2q_apid_to_neighbor_peer);
		q2q->q2q_apid_from_neighbor_peer =
			cpu_to_le32(arg->q2q_apid_from_neighbor_peer);
		ptr += sizeof(*q2q);
	}

	/* CTDMA profile group: channel info */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 has_profile ? sizeof(*profile) : 0);
	ptr += TLV_HDR_SIZE;
	if (has_profile) {
		profile = ptr;
		profile->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MAPC_CTDMA_PROFILE,
							     sizeof(*profile));
		profile->channel_width             = cpu_to_le32(arg->channel_width);
		profile->ccfs                      = cpu_to_le32(arg->ccfs);
		profile->bss_color                 = cpu_to_le32(arg->bss_color);
		profile->rx_txop_return_support =
			cpu_to_le32(arg->rx_txop_return_support ? 1 : 0);
		profile->disable_subchannel_bitmap =
			cpu_to_le32(arg->disable_subchannel_bitmap);
		ptr += sizeof(*profile);
	}

	/* Slot 4: TXOP policy group — Co-TDMA TXOP sharing policy */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 has_txop ? sizeof(*txop) : 0);
	ptr += TLV_HDR_SIZE;
	if (has_txop) {
		txop = ptr;
		txop->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MAPC_CTDMA_TXOP_SHARING_POLICY,
					       sizeof(*txop));
		txop->primary_ac                     = cpu_to_le32(arg->primary_ac);
		txop->nbr_ap_prio                    = cpu_to_le32(arg->nbr_ap_prio);
		txop->latency_sensitive_threshold_us = 0;
		txop->service_start_time = cpu_to_le32(arg->service_start_time);
		txop->service_interval               = cpu_to_le32(arg->service_interval);
		txop->service_end_time               = cpu_to_le32(arg->service_end_time);
		txop->critical_traffic_dur_thresh_us =
			cpu_to_le32(arg->critical_traffic_dur_thresh_us);
		txop->max_shared_txop_dur_us = cpu_to_le32(arg->max_shared_txop_dur_us);
		txop->min_shared_txop_dur_us = cpu_to_le32(arg->min_shared_txop_dur_us);
		ptr += sizeof(*txop);
	}

	/* COTDMA E2E config group */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 has_e2e ? sizeof(*e2e) : 0);
	ptr += TLV_HDR_SIZE;
	if (has_e2e) {
		e2e = ptr;
		memset(e2e, 0, sizeof(*e2e));
		e2e->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MAPC_COTDMA_E2E_CONFIG,
					       sizeof(*e2e));
		e2e->config_mode = cpu_to_le32(arg->e2e_config_mode);
		e2e->qmid        = cpu_to_le32(arg->e2e_qmid);
		if (arg->e2e_bsta_mac_valid)
			ether_addr_copy(e2e->bsta_mac.addr, arg->e2e_bsta_mac);
		ptr += sizeof(*e2e);
	}

	/* Co-SR, Co-BF, Co-rTWT — always 0-element stubs */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI_PEER_SET_MAPC_PARAMS: peer %pM vdev %d mask=0x%x cmn: apid_to=%u apid_from=%u cap=0x%04x q2q: apid_to=%u apid_from=%u\n",
		   arg->peer_addr, arg->vdev_id, arg->param_set_mask,
		   arg->apid_to_neighbor_peer, arg->apid_from_neighbor_peer,
		   arg->mapc_capability_bitmap,
		   arg->q2q_apid_to_neighbor_peer, arg->q2q_apid_from_neighbor_peer);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI_PEER_SET_MAPC_PARAMS: profile: ch_width=%u ccfs=%u dsb=0x%04x bss_color=%u rx_txop_return=%u txop: primary_ac=%u nbr_prio=%u svc_start=%u svc_interval=%u svc_end=%u crit_dur=%u max_txop=%u min_txop=%u\n",
		   arg->channel_width, arg->ccfs,
		   arg->disable_subchannel_bitmap, arg->bss_color,
		   arg->rx_txop_return_support,
		   arg->primary_ac, arg->nbr_ap_prio,
		   arg->service_start_time, arg->service_interval, arg->service_end_time,
		   arg->critical_traffic_dur_thresh_us,
		   arg->max_shared_txop_dur_us, arg->min_shared_txop_dur_us);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI_PEER_SET_MAPC_PARAMS: e2e: config_mode=%u qmid=%u bsta_mac_valid=%d\n",
		   arg->e2e_config_mode, arg->e2e_qmid, arg->e2e_bsta_mac_valid);


	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_SET_MAPC_PARAMS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_SET_MAPC_PARAMS_CMDID for %pM: %d\n",
			    arg->peer_addr, ret);
		dev_kfree_skb(skb);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_wmi_send_peer_set_mapc_params_cmd);


int ath12k_wmi_send_peer_flush_tids_cmd(struct ath12k *ar,
					u8 peer_addr[ETH_ALEN],
					u32 peer_tid_bitmap,
					u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_flush_tids_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_flush_tids_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_FLUSH_TIDS_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->peer_tid_bitmap = cpu_to_le32(peer_tid_bitmap);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI peer flush vdev_id %d peer_addr %pM tids %08x\n",
		   vdev_id, peer_addr, peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_FLUSH_TIDS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_FLUSH_TIDS cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_peer_rx_reorder_queue_setup(struct ath12k *ar,
					   int vdev_id, const u8 *addr,
					   dma_addr_t paddr, u8 tid,
					   u8 ba_window_size_valid,
					   u32 ba_window_size)
{
	struct wmi_peer_reorder_queue_setup_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_setup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_SETUP_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tid = cpu_to_le32(tid);
	cmd->queue_ptr_lo = cpu_to_le32(lower_32_bits(paddr));
	cmd->queue_ptr_hi = cpu_to_le32(upper_32_bits(paddr));
	cmd->queue_no = cpu_to_le32(tid);
	cmd->ba_window_size_valid = cpu_to_le32(ba_window_size_valid);
	cmd->ba_window_size = cpu_to_le32(ba_window_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi rx reorder queue setup addr %pM vdev_id %d tid %d\n",
		   addr, vdev_id, tid);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PEER_REORDER_QUEUE_SETUP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_SETUP\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_rx_reord_queue_remove(struct ath12k *ar,
				 struct ath12k_wmi_rx_reorder_queue_remove_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_reorder_queue_remove_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_remove_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_REMOVE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_macaddr);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->tid_mask = cpu_to_le32(arg->peer_tid_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "%s: peer_macaddr %pM vdev_id %d, tid_map %d", __func__,
		   arg->peer_macaddr, arg->vdev_id, arg->peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_REORDER_QUEUE_REMOVE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_REMOVE_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_param(struct ath12k *ar, u32 param_id,
			      u32 param_value, u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	if (ar->ab->is_bypassed) {
		ath12k_warn(ar->ab, "Chip is bypassed, skip Pdev set cmd");
		return 0;
	}

	cmd = (struct wmi_pdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_PARAM_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L2,
			 "WMI pdev set param %d pdev id %d value %d\n",
			 param_id, pdev_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_set_rf_path_cmd(struct ath12k *ar, u32 rf_path)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_rf_path_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_rf_path_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_RF_PATH_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->rf_path = cpu_to_le32(rf_path);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev set_rf_path %u pdev_id %u\n",
		   rf_path, ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_RF_PATH_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_RF_PATH_CMDID: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_frame_inject_cmd(struct ath12k *ar,
					  const struct wmi_frame_inject_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_frame_inject_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_frame_inject_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FRAME_INJECT_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->enable = cpu_to_le32(arg->enable);
	cmd->frame_type = cpu_to_le32(arg->frame_type);
	if (cmd->enable) {
		cmd->frame_inject_period = cpu_to_le32(arg->frame_inject_period);
		ether_addr_copy(cmd->frame_addr1.addr, arg->dstmac);
		cmd->fc_duration = cpu_to_le32(arg->fc_duration);
		cmd->bw = cpu_to_le32(arg->bw);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI frame inject vdev %u type %u enable %u period %u bw %u\n",
		   arg->vdev_id, arg->frame_type, arg->enable,
		   arg->frame_inject_period, arg->bw);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_FRAME_INJECT_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_FRAME_INJECT_CMDID: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_ps_mode(struct ath12k *ar, int vdev_id, u32 enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_ps_mode_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_ps_mode_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_MODE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->sta_ps_mode = cpu_to_le32(enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev set psmode %d vdev id %d\n",
		   enable, vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_MODE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_timer_for_mec(struct ath12k *ar, int vdev_id, u32 mec_timer)
{
	struct wmi_pdev_set_mec_timer_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_mec_timer_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_MEC_AGEING_TIMER_PARAMS,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->mec_aging_timer_threshold = cpu_to_le32(mec_timer);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_MEC_AGING_TIMER_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to set mec timer\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_vdev_uhr_cu_cmd(struct ath12k *ar,
			       struct ath12k_wmi_vdev_uhr_cu_arg *arg)
{
	struct wmi_vdev_uhr_cu_cmd_fixed_param *cmd;
	struct wmi_uhr_ap_dps_params *dps;
	struct wmi_uhr_ap_npca_params *npca;
	struct wmi_uhr_ap_duo_params *duo;
	struct wmi_uhr_ap_pedca_params *pedca;
	struct wmi_uhr_ap_dbe_params *dbe;
	struct wmi_uhr_ap_puo_params *puo;
	struct wmi_uhr_ap_elr_reception_params *elr;
	struct wmi_uhr_ap_mode_tuple_params *mode_tup;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	len = sizeof(*cmd) +
	      sizeof(*dps) +
	      sizeof(*npca) +
	      sizeof(*duo) +
	      sizeof(*pedca) +
	      sizeof(*dbe) +
	      sizeof(*puo) +
	      sizeof(*elr) +
	      TLV_HDR_SIZE + sizeof(*mode_tup); /* mode_tuple[] array: 1 entry */

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_UHR_CU_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ptr += sizeof(*cmd);

	dps = ptr;
	dps->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_DPS_PARAMS,
						 sizeof(*dps));
	dps->vdev_id = cpu_to_le32(arg->dps.vdev_id);
	dps->mode_tuple_field = cpu_to_le32(arg->dps.mode_tuple_field);
	ptr += sizeof(*dps);

	npca = ptr;
	npca->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_NPCA_PARAMS,
						  sizeof(*npca));
	npca->vdev_id = cpu_to_le32(arg->npca.vdev_id);
	npca->mode_tuple_field = cpu_to_le32(arg->npca.mode_tuple_field);
	npca->npca_chan.tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
				       sizeof(npca->npca_chan));
	npca->npca_chan.mhz            = cpu_to_le32(arg->npca.mhz);
	npca->npca_chan.band_center_freq1 = cpu_to_le32(arg->npca.band_center_freq1);
	npca->npca_chan.band_center_freq2 = cpu_to_le32(arg->npca.band_center_freq2);
	npca->npca_chan.info           = cpu_to_le32(arg->npca.info);
	npca->npca_chan.reg_info_1     = cpu_to_le32(arg->npca.reg_info_1);
	npca->npca_chan.reg_info_2     = cpu_to_le32(arg->npca.reg_info_2);
	npca->puncture_20mhz_bitmap = cpu_to_le32(arg->npca.puncture_20mhz_bitmap);
	npca->npca_cap1 = cpu_to_le32(arg->npca.npca_cap1);
	npca->npca_cap2 = cpu_to_le32(arg->npca.npca_cap2);
	ptr += sizeof(*npca);

	duo = ptr;
	duo->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_DUO_PARAMS,
						 sizeof(*duo));
	duo->vdev_id = cpu_to_le32(arg->duo.vdev_id);
	duo->mode_tuple_field = cpu_to_le32(arg->duo.mode_tuple_field);
	ptr += sizeof(*duo);

	pedca = ptr;
	pedca->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_PEDCA_PARAMS,
						   sizeof(*pedca));
	pedca->vdev_id = cpu_to_le32(arg->pedca.vdev_id);
	pedca->mode_tuple_field = cpu_to_le32(arg->pedca.mode_tuple_field);
	ptr += sizeof(*pedca);

	dbe = ptr;
	dbe->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_DBE_PARAMS,
						 sizeof(*dbe));
	dbe->vdev_id = cpu_to_le32(arg->dbe.vdev_id);
	dbe->mode_tuple_field = cpu_to_le32(arg->dbe.mode_tuple_field);
	ptr += sizeof(*dbe);

	puo = ptr;
	puo->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_PUO_PARAMS,
						 sizeof(*puo));
	puo->vdev_id = cpu_to_le32(arg->puo.vdev_id);
	puo->mode_tuple_field = cpu_to_le32(arg->puo.mode_tuple_field);
	ptr += sizeof(*puo);

	elr = ptr;
	elr->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_ELR_RECEPTION_PARAMS,
						 sizeof(*elr));
	elr->vdev_id = cpu_to_le32(arg->elr.vdev_id);
	elr->mode_tuple_field = cpu_to_le32(arg->elr.mode_tuple_field);
	ptr += sizeof(*elr);

	/* mode_tuple[] array: one entry encoding per-mode present/absent state */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*mode_tup));
	ptr += TLV_HDR_SIZE;

	mode_tup = ptr;
	mode_tup->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_MODE_TUPLE_UPDATE_PARAM,
				       sizeof(*mode_tup));
	mode_tup->vdev_id_mode_bitmap =
		le32_encode_bits(arg->vdev_id, WMI_UHR_MODE_TUP_VDEV_ID) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_DPS)),
				 WMI_UHR_MODE_TUP_DPS_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_NPCA)),
				 WMI_UHR_MODE_TUP_NPCA_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_DUO)),
				 WMI_UHR_MODE_TUP_DUO_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_PEDCA)),
				 WMI_UHR_MODE_TUP_PEDCA_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_DBE)),
				 WMI_UHR_MODE_TUP_DBE_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_AP_PUO)),
				 WMI_UHR_MODE_TUP_PUO_STATE) |
		le32_encode_bits(!!(arg->mode_present_bitmap &
				    BIT(IEEE80211_UHR_MODE_ID_ELR_RX)),
				 WMI_UHR_MODE_TUP_ELR_STATE);
	ptr += sizeof(*mode_tup);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd fixed_param: vdev_id %u\n",
		   arg->vdev_id);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd dps: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->dps.vdev_id, arg->dps.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd npca: vdev_id %u mode_tuple_field 0x%x mhz %u bcf1 %u bcf2 %u info 0x%x puncture_bitmap 0x%x npca_cap1 0x%x npca_cap2 0x%x\n",
		   arg->npca.vdev_id, arg->npca.mode_tuple_field,
		   arg->npca.mhz, arg->npca.band_center_freq1,
		   arg->npca.band_center_freq2, arg->npca.info,
		   arg->npca.puncture_20mhz_bitmap,
		   arg->npca.npca_cap1, arg->npca.npca_cap2);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd duo: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->duo.vdev_id, arg->duo.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd pedca: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->pedca.vdev_id, arg->pedca.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd dbe: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->dbe.vdev_id, arg->dbe.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd puo: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->puo.vdev_id, arg->puo.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd elr: vdev_id %u mode_tuple_field 0x%x\n",
		   arg->elr.vdev_id, arg->elr.mode_tuple_field);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev_uhr_cu_cmd mode_tuple[0]: vdev_id_mode_bitmap 0x%x\n",
		   le32_to_cpu(mode_tup->vdev_id_mode_bitmap));

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_UHR_CU_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_UHR_CU_CMDID for vdev %u: %d\n",
			    arg->vdev_id, ret);
		dev_kfree_skb(skb);
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_wmi_pdev_set_timer_for_mec);

int ath12k_wmi_pdev_suspend(struct ath12k *ar, u32 suspend_opt,
			    u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_suspend_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_suspend_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SUSPEND_CMD,
						 sizeof(*cmd));

	cmd->suspend_opt = cpu_to_le32(suspend_opt);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev suspend pdev_id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SUSPEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SUSPEND cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_resume(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_resume_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_resume_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_RESUME_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev resume pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_RESUME_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_RESUME cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* TODO FW Support for the cmd is not available yet.
 * Can be tested once the command and corresponding
 * event is implemented in FW
 */
int ath12k_wmi_pdev_bss_chan_info_request(struct ath12k *ar,
					  enum wmi_bss_chan_info_req_type type)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_bss_chan_info_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_bss_chan_info_req_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BSS_CHAN_INFO_REQUEST,
						 sizeof(*cmd));
	cmd->req_type = cpu_to_le32(type);
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bss chan info req type %d\n", type);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_BSS_CHAN_INFO_REQUEST cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_set_ap_ps_param_cmd(struct ath12k *ar, u8 *peer_addr,
					struct ath12k_wmi_ap_ps_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_ap_ps_peer_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_ap_ps_peer_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_AP_PS_PEER_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->param = cpu_to_le32(arg->param);
	cmd->value = cpu_to_le32(arg->value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set ap ps vdev id %d peer %pM param %d value %d\n",
		   arg->vdev_id, peer_addr, arg->param, arg->value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_AP_PS_PEER_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_AP_PS_PEER_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_sta_ps_param(struct ath12k *ar, u32 vdev_id,
				u32 param, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_sta_powersave_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_powersave_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param = cpu_to_le32(param);
	cmd->value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set sta ps vdev_id %d param %d value %d\n",
		   vdev_id, param, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STA_POWERSAVE_PARAM_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_force_fw_hang_cmd(struct ath12k *ar, u32 type, u32 delay_time_ms, bool nowait)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_force_fw_hang_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_force_fw_hang_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FORCE_FW_HANG_CMD,
						 len);

	cmd->type = cpu_to_le32(type);
	cmd->delay_time_ms = cpu_to_le32(delay_time_ms);
	if (nowait)
		ath12k_wmi_cmd_send_nowait(wmi, skb, WMI_FORCE_FW_HANG_CMDID);
	else
		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_FORCE_FW_HANG_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_FORCE_FW_HANG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

static bool ath12k_wmi_vdev_is_created(struct ath12k *ar, u32 vdev_id)
{
	bool created;

	spin_lock_bh(&ar->data_lock);
	created = ar->created_vdev_map & (1LL << vdev_id);
	spin_unlock_bh(&ar->data_lock);

	return created;
}

int ath12k_wmi_vdev_set_param_cmd(struct ath12k *ar, u32 vdev_id,
				  u32 param_id, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	if (!ath12k_wmi_vdev_is_created(ar, vdev_id)) {
		ath12k_warn(ar->ab,
			    "vdev %d set param %d value %d sent before vdev create, allocated_vdev_map 0x%llx, created_vdev_map 0x%llx\n",
			    vdev_id, param_id, param_value, ar->allocated_vdev_map,
			    ar->created_vdev_map);
		WARN_ON(1);
		return -ENOENT;
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev id 0x%x set param %d value %d\n",
		   vdev_id, param_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_copy_coex_config(struct ath12k *ar, struct wmi_coex_config_cmd *cmd,
                                       struct coex_config_arg *coex_config)
{
        switch (coex_config->config_type) {
        case WMI_COEX_CONFIG_BTC_ENABLE:
                cmd->coex_enable = coex_config->coex_enable;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " coex_enable %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->coex_enable);
                break;
        case WMI_COEX_CONFIG_WLAN_PKT_PRIORITY:
                cmd->wlan_pkt_type = coex_config->wlan_pkt_type;
                cmd->wlan_pkt_weight = coex_config->wlan_pkt_weight;
                cmd->bt_pkt_weight = coex_config->bt_pkt_weight;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " wlan pkt type 0x%x wlan pkt weight %u"
                           " bt pkt weight %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->wlan_pkt_type,
                           coex_config->wlan_pkt_weight,
                           coex_config->bt_pkt_weight);
                break;
        case WMI_COEX_CONFIG_PTA_INTERFACE:
                cmd->pta_num = coex_config->pta_num;
                cmd->coex_mode = coex_config->coex_mode;
                cmd->bt_txrx_time = coex_config->bt_txrx_time;
                cmd->bt_priority_time = coex_config->bt_priority_time;
                cmd->pta_algorithm = coex_config->pta_algorithm;
                cmd->pta_priority = coex_config->pta_priority;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " pta num %u coex mode 0x%x"
                           " bt_txrx_time 0x%x"
                           " bt_priority_time 0x%x pta alogrithm 0x%x"
                           " pta priority 0x%x\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->pta_num,
                           coex_config->coex_mode,
                           coex_config->bt_txrx_time,
                           coex_config->bt_priority_time,
                           coex_config->pta_algorithm,
                           coex_config->pta_priority);
                break;
        case WMI_COEX_CONFIG_AP_TDM:
                cmd->duty_cycle = coex_config->duty_cycle;
                cmd->wlan_duration = coex_config->wlan_duration;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " duty_cycle %u wlan_duration %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->duty_cycle,
                           coex_config->wlan_duration);
                break;
        case WMI_COEX_CONFIG_FORCED_ALGO:
                cmd->coex_algo = coex_config->coex_algo;
                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "WMI coex config type %u vdev id %d"
                           " coex_algorithm %u\n",
                           coex_config->config_type,
                           coex_config->vdev_id,
                           coex_config->coex_algo);
		break;
        default:
                break;
        }
}

int ath12k_send_coex_config_cmd(struct ath12k *ar,
                                struct coex_config_arg *coex_config)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_coex_config_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_coex_config_cmd *)skb->data;
        cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_COEX_CONFIG_CMD) |
                          FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

        cmd->vdev_id = coex_config->vdev_id;
        cmd->config_type = coex_config->config_type;
        ath12k_wmi_copy_coex_config(ar, cmd, coex_config);

        ret = ath12k_wmi_cmd_send(wmi, skb, WMI_COEX_CONFIG_CMDID);
        if (ret) {
                ath12k_warn(ar->ab, "failed to send WMI_COEX_CONFIG_CMD cmd\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_send_pdev_temperature_cmd(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_get_pdev_temperature_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_get_pdev_temperature_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_GET_TEMPERATURE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev get temperature for pdev_id %d\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_TEMPERATURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_GET_TEMPERATURE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_get_nfcal_power_cmd(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_get_pdev_nfcal_power_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_get_pdev_nfcal_power_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_GET_NFCAL_POWER,
						  sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev get nfcal power for pdev_id %d\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_NFCAL_POWER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_GET_NFCAL_POWER cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_check_cal_version_cmd(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_check_cal_version_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	if (ar->ab->is_bypassed) {
		ath12k_warn(ar->ab, "chip is bypassed, skip cal version check cmd\n");
		return 0;
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_check_cal_version_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_CHECK_CAL_VERSION_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(0);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev check cal version pdev_id %d\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_CHECK_CAL_VERSION_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_CHECK_CAL_VERSION cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_bcn_offload_control_cmd(struct ath12k *ar,
					    u32 vdev_id, u32 bcn_ctrl_op)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_bcn_offload_ctrl_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_offload_ctrl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_OFFLOAD_CTRL_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->bcn_ctrl_op = cpu_to_le32(bcn_ctrl_op);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bcn ctrl offload vdev id %d ctrl_op %d\n",
		   vdev_id, bcn_ctrl_op);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_OFFLOAD_CTRL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_BCN_OFFLOAD_CTRL_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_p2p_go_bcn_ie(struct ath12k *ar, u32 vdev_id,
			     const u8 *p2p_ie)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_p2p_go_set_beacon_ie_cmd *cmd;
	size_t p2p_ie_len, aligned_len;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	p2p_ie_len = p2p_ie[1] + 2;
	aligned_len = roundup(p2p_ie_len, sizeof(u32));

	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_P2P_GO_SET_BEACON_IE,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->ie_buf_len = cpu_to_le32(p2p_ie_len);

	ptr += sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_BYTE,
					     aligned_len);
	memcpy(tlv->value, p2p_ie, p2p_ie_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_P2P_GO_SET_BEACON_IE);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_P2P_GO_SET_BEACON_IE\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_bcn_fill_ml_info(struct ath12k_link_vif *arvif,
					struct wmi_bcn_tmpl_ml_info *ml_info)
{
	struct ieee80211_bss_conf *link_conf, *tx_link_conf;
	struct ath12k_base *ab = arvif->ar->ab;
	struct ath12k_link_vif *arvif_iter;
	u32 vdev_id = arvif->vdev_id;
	DECLARE_BITMAP(vdev_map_cat1, 64);
	DECLARE_BITMAP(vdev_map_cat2, 64);

	bitmap_zero(vdev_map_cat1, 64);
	bitmap_zero(vdev_map_cat2, 64);
	rcu_read_lock();

	tx_link_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!tx_link_conf) {
		rcu_read_unlock();
		goto err_fill_ml_info;
	}

	/* Fill CU flags for non-tx vdevs while setting tx vdev beacon.
	 */
	list_for_each_entry(arvif_iter, &arvif->ar->arvifs, list) {
		if (arvif_iter != arvif && arvif_iter->tx_vdev_id == arvif->vdev_id &&
		    ath12k_mac_is_ml_arvif(arvif_iter)) {
			link_conf = ath12k_mac_get_link_bss_conf(arvif_iter);
			if (!link_conf) {
				rcu_read_unlock();
				goto err_fill_ml_info;
			}
			/* If this is cu cat 1 for tx vdev, then it applies
			 * to non-tx vdev as well.
			 */
			if (link_conf->elemid_added || tx_link_conf->elemid_added)
				set_bit(arvif_iter->vdev_id, vdev_map_cat1);
			/* If arvif is not up, current set beacon will be bringing it up
			 * So for link addition, set critical update even if arvif is
			 * not up.
			 */
			if (link_conf->elemid_modified || !arvif_iter->is_up)
				set_bit(arvif_iter->vdev_id, vdev_map_cat2);
		}
	}

	rcu_read_unlock();

	ml_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_TMPL_ML_INFO_CMD,
						     sizeof(*ml_info));
	ml_info->hw_link_id = cpu_to_le32(arvif->ar->pdev->hw_link_id);

	if (tx_link_conf->elemid_added)
		set_bit(vdev_id, vdev_map_cat1);

	if (tx_link_conf->elemid_modified)
		set_bit(vdev_id, vdev_map_cat2);

err_fill_ml_info:
#if BITS_PER_LONG == 32
	ml_info->cu_vdev_map_cat1_lo = cpu_to_le32(vdev_map_cat1[0]);
	ml_info->cu_vdev_map_cat1_hi = cpu_to_le32(vdev_map_cat1[1]);
	ml_info->cu_vdev_map_cat2_lo = cpu_to_le32(vdev_map_cat2[0]);
	ml_info->cu_vdev_map_cat2_hi = cpu_to_le32(vdev_map_cat2[1]);
#else
	ml_info->cu_vdev_map_cat1_lo =
			   cpu_to_le32(ATH12K_GET_LOWER_32_BITS(vdev_map_cat1[0]));
	ml_info->cu_vdev_map_cat1_hi =
			   cpu_to_le32(ATH12K_GET_UPPER_32_BITS(vdev_map_cat1[0]));
	ml_info->cu_vdev_map_cat2_lo =
			   cpu_to_le32(ATH12K_GET_LOWER_32_BITS(vdev_map_cat2[0]));
	ml_info->cu_vdev_map_cat2_hi =
			   cpu_to_le32(ATH12K_GET_UPPER_32_BITS(vdev_map_cat2[0]));
#endif

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi CU filled ml info cat1_lo=0x%x cat1_hi=0x%x cat2_lo=0x%x cat2_hi=0x%x\n",
		   ml_info->cu_vdev_map_cat1_lo, ml_info->cu_vdev_map_cat1_hi,
		   ml_info->cu_vdev_map_cat2_lo, ml_info->cu_vdev_map_cat2_hi);
}

static void ath12k_wmi_fill_cu_arg(struct ath12k_link_vif *arvif,
				   struct wmi_critical_update_arg *cu_arg)
{
	struct ath12k_prb_resp_tmpl_ml_info *ar_ml_info;
	struct ath12k_base *ab = arvif->ar->ab;
	struct wmi_bcn_tmpl_ml_info *ml_info;
	struct ath12k *ar = arvif->ar;
	int i;

	if (!ath12k_mac_is_ml_arvif(arvif))
		return;

	/* Fill ML params
	 * ML params should be filled for all partner links
	 */
	cu_arg->num_ml_params = 0;
	/* TODO: Fill ML params. Will work without this info too */

	/* Fill ML info
	 * ML info should be filled for impacted link only
	 */
	cu_arg->num_ml_info = 1;
	cu_arg->ml_info = (struct wmi_bcn_tmpl_ml_info *)
			  kzalloc((cu_arg->num_ml_info * sizeof(*ml_info)),
				  GFP_KERNEL);

	if (!cu_arg->ml_info) {
		ath12k_warn(ab, "wmi failed to get memory for ml info");
		cu_arg->num_ml_info = 0;
	} else {
		for (i = 0; i < cu_arg->num_ml_info; i++) {
			ml_info = &cu_arg->ml_info[i];
			ath12k_wmi_bcn_fill_ml_info(arvif, ml_info);
			/* Retain copy of CU vdev bitmap. Which are used to
			 * update cu_vdev_map in 20TU probe response template.
			 */
			if (ar->supports_6ghz) {
				ar_ml_info = &arvif->ml_info;
				ar_ml_info->hw_link_id = ml_info->hw_link_id;
				ar_ml_info->cu_vdev_map_cat1_lo = ml_info->cu_vdev_map_cat1_lo;
				ar_ml_info->cu_vdev_map_cat1_hi = ml_info->cu_vdev_map_cat1_hi;
				ar_ml_info->cu_vdev_map_cat2_lo = ml_info->cu_vdev_map_cat2_lo;
				ar_ml_info->cu_vdev_map_cat2_hi = ml_info->cu_vdev_map_cat2_hi;
			}
		}
	}
}

static void *
ath12k_wmi_append_critical_update_params(struct ath12k *ar, u32 vdev_id,
					 void *ptr,
					 struct wmi_critical_update_arg *cu_arg)
{
	struct wmi_bcn_tmpl_ml_params *ml_params;
	struct wmi_bcn_tmpl_ml_info *ml_info;
	void *start = ptr;
	struct wmi_tlv *tlv;
	size_t ml_params_len = cu_arg->num_ml_params * sizeof(*ml_params);
	size_t ml_info_len = cu_arg->num_ml_info * sizeof(*ml_info);
	int i;

	/* Add ML params */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_params_len);
	ml_params = (struct wmi_bcn_tmpl_ml_params *)tlv->value;

	for (i = 0; i < cu_arg->num_ml_params; i++)
		memcpy(&ml_params[i], &cu_arg->ml_params[i],
		       sizeof(*ml_params));

	if (cu_arg->num_ml_params)
		kfree(cu_arg->ml_params);

	ptr += TLV_HDR_SIZE + ml_params_len;

	/* Add ML info */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_info_len);
	ml_info = (struct wmi_bcn_tmpl_ml_info *)tlv->value;

	for (i = 0; i < cu_arg->num_ml_info; i++)
		memcpy(&ml_info[i], &cu_arg->ml_info[i],
		       sizeof(*ml_info));

	if (cu_arg->num_ml_info)
		kfree(cu_arg->ml_info);

	ptr += TLV_HDR_SIZE + ml_info_len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi %ld bytes of additional data filled for CU\n",
		    (unsigned long)(ptr - start));
	return ptr;
}

int ath12k_wmi_bcn_tmpl(struct ath12k_link_vif *arvif,
			struct ieee80211_mutable_offsets *offs,
			struct sk_buff *bcn,
			struct ath12k_wmi_bcn_tmpl_ema_arg *ema_args)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct wmi_bcn_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *bcn_prb_info;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_bss_conf *conf;
	u32 vdev_id = arvif->vdev_id;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u32 ema_params = 0;
	void *ptr;
	int ret, len;
	size_t aligned_len = roundup(bcn->len, 4);
	struct wmi_critical_update_arg cu_arg = {
						 .num_ml_params = 0,
						 .ml_params = NULL,
						 .num_ml_info = 0,
						 .ml_info = NULL,
						};

	conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!conf) {
		ath12k_warn(ab,
			    "unable to access bss link conf in beacon template command for vif %pM link %u\n",
			    ahvif->vif->addr, arvif->link_id);
		return -EINVAL;
	}

	ath12k_wmi_fill_cu_arg(arvif, &cu_arg);

	len = sizeof(*cmd) + sizeof(*bcn_prb_info) + TLV_HDR_SIZE + aligned_len +
	      TLV_HDR_SIZE + (sizeof(struct wmi_bcn_tmpl_ml_params) * cu_arg.num_ml_params) +
	      TLV_HDR_SIZE + (sizeof(struct wmi_bcn_tmpl_ml_info) * cu_arg.num_ml_info);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		if (cu_arg.num_ml_params)
			kfree(cu_arg.ml_params);
		if (cu_arg.num_ml_info)
			kfree(cu_arg.ml_info);
		return -ENOMEM;
	}

	cmd = (struct wmi_bcn_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tim_ie_offset = cpu_to_le32(offs->tim_offset);

	if (conf->csa_active) {
		cmd->csa_switch_count_offset =
				cpu_to_le32(offs->cntdwn_counter_offs[0]);
		cmd->ext_csa_switch_count_offset =
				cpu_to_le32(offs->cntdwn_counter_offs[1]);
		cmd->csa_event_bitmap = cpu_to_le32(0xFFFFFFFF);
		arvif->current_cntdown_counter = bcn->data[offs->cntdwn_counter_offs[0]];
	}

	cmd->buf_len = cpu_to_le32(bcn->len);
	cmd->mbssid_ie_offset = cpu_to_le32(offs->mbssid_off);
	if (ema_args) {
		u32p_replace_bits(&ema_params, ema_args->bcn_cnt, WMI_EMA_BEACON_CNT);
		u32p_replace_bits(&ema_params, ema_args->bcn_index, WMI_EMA_BEACON_IDX);
		if (ema_args->bcn_index == 0)
			u32p_replace_bits(&ema_params, 1, WMI_EMA_BEACON_FIRST);
		if (ema_args->bcn_index + 1 == ema_args->bcn_cnt)
			u32p_replace_bits(&ema_params, 1, WMI_EMA_BEACON_LAST);
		cmd->ema_params = cpu_to_le32(ema_params);
	}
	cmd->feature_enable_bitmap = cpu_to_le32(u32_encode_bits(arvif->beacon_prot,
						 WMI_BEACON_PROTECTION_EN_BIT));
	cmd->feature_enable_bitmap |=
		cpu_to_le32(u32_encode_bits(arvif->control_frame_prot,
					    WMI_CONTROL_PROTECTION_EN_BIT));

	ptr = skb->data + sizeof(*cmd);

	bcn_prb_info = ptr;
	len = sizeof(*bcn_prb_info);
	bcn_prb_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							  len);
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;

	ptr += sizeof(*bcn_prb_info);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, bcn->data, bcn->len);

	ptr += (TLV_HDR_SIZE + aligned_len);

	ptr = ath12k_wmi_append_critical_update_params(ar, vdev_id, ptr,
						       &cu_arg);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ab, "[vdev_id : %u radio_idx : %u] failed to send WMI_BCN_TMPL_CMDID\n",
			    vdev_id, ar->radio_idx);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_install_key(struct ath12k *ar,
				struct wmi_vdev_install_key_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_install_key_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret, len, key_len_aligned;

	/* WMI_TAG_ARRAY_BYTE needs to be aligned with 4, the actual key
	 * length is specified in cmd->key_len.
	 */
	key_len_aligned = roundup(arg->key_len, 4);

	len = sizeof(*cmd) + TLV_HDR_SIZE + key_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_install_key_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_INSTALL_KEY_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->macaddr);
	cmd->key_idx = cpu_to_le32(arg->key_idx);
	cmd->key_flags = cpu_to_le32(arg->key_flags);
	cmd->key_cipher = cpu_to_le32(arg->key_cipher);
	cmd->key_len = cpu_to_le32(arg->key_len);
	cmd->key_txmic_len = cpu_to_le32(arg->key_txmic_len);
	cmd->key_rxmic_len = cpu_to_le32(arg->key_rxmic_len);
	if (arg->is_group_key_id_valid) {
		cmd->is_group_key_id_valid = cpu_to_le32(arg->is_group_key_id_valid);
		cmd->group_key_id = cpu_to_le32(arg->group_key_id);
	}

	if (arg->key_rsc_counter)
		cmd->key_rsc_counter = cpu_to_le64(arg->key_rsc_counter);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, key_len_aligned);
	memcpy(tlv->value, arg->key_data, arg->key_len);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_EAPOL, ATH12K_DBG_L1,
			 "WMI vdev install key idx %d flags: 0x%x cipher %d len %d\n",
			 arg->key_idx, arg->key_flags,
			 arg->key_cipher, arg->key_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_INSTALL_KEY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_INSTALL_KEY cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_copy_peer_flags(struct wmi_peer_assoc_complete_cmd *cmd,
				       struct ath12k_wmi_peer_assoc_arg *arg)
{
	cmd->peer_flags = 0;
	cmd->peer_flags_ext = 0;

	if (arg->is_wme_set) {
		if (arg->qos_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_QOS);
		if (arg->apsd_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_APSD);
		if (arg->ht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HT);
		if (arg->bw_40)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_40MHZ);
		if (arg->bw_80)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_80MHZ);
		if (arg->bw_160)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_160MHZ);
		if (arg->bw_320)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_320MHZ);

		/* Typically if STBC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->stbc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STBC);

		/* Typically if LDPC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->ldpc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_LDPC);

		if (arg->static_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STATIC_MIMOPS);
		if (arg->dynamic_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_DYN_MIMOPS);
		if (arg->spatial_mux_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_SPATIAL_MUX);
		if (arg->vht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_VHT);
		if (arg->he_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HE);
		if (arg->twt_requester)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_REQ);
		if (arg->twt_responder)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_RESP);
		if (arg->eht_flag)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_EHT);
		if (arg->uhr_flag)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_UHR);
	}

	/* Suppress authorization for all AUTH modes that need 4-way handshake
	 * (during re-association).
	 * Authorization will be done by host after Key installation.
	 */
	if (arg->auth_flag  && !arg->ml.ml_reconfig)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_AUTH);
	if (arg->need_ptk_4_way) {
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_PTK_4_WAY);
		if (arg->is_assoc)
			cmd->peer_flags &= cpu_to_le32(~WMI_PEER_AUTH);
	}
	if (arg->need_gtk_2_way)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_GTK_2_WAY);
	/* safe mode bypass the 4-way handshake */
	if (arg->safe_mode_enabled)
		cmd->peer_flags &= cpu_to_le32(~(WMI_PEER_NEED_PTK_4_WAY |
						 WMI_PEER_NEED_GTK_2_WAY));

	if (arg->is_pmf_enabled)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_PMF);

	if (arg->is_cfp_enabled)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_CFP);

	/* Disable AMSDU for station transmit, if user configures it */
	/* Disable AMSDU for AP transmit to 11n Stations, if user configures
	 * it
	 * if (arg->amsdu_disable) Add after FW support
	 **/

	/* Target asserts if node is marked HT and all MCS is set to 0.
	 * Mark the node as non-HT if all the mcs rates are disabled through
	 * iwpriv
	 **/
	if (arg->peer_ht_rates.num_rates == 0)
		cmd->peer_flags &= cpu_to_le32(~WMI_PEER_HT);
}

#define EMLSR_PAD_DELAY_MAX    5
#define EMLSR_TRANS_DELAY_MAX  6
#define EML_TRANS_TIMEOUT_MAX  11
#define TU_TO_USEC(t)          ((t) << 10)  /* (t) x 1024 */

static u32 ath12k_wmi_get_emlsr_pad_delay_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417i—Encoding of the EMLSR
	 * Padding Delay subfield.
	*/
	u32 pad_delay = u16_get_bits(eml_cap, IEEE80211_EML_CAP_EMLSR_PADDING_DELAY);
	static const u32 pad_delay_us[EMLSR_PAD_DELAY_MAX] = {0, 32, 64, 128, 256};

	if (pad_delay >= EMLSR_PAD_DELAY_MAX)
		return 0;

	return pad_delay_us[pad_delay];
}

static u32 ath12k_wmi_get_emlsr_trans_delay_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417j—Encoding of the EMLSR
	 * Transition Delay subfield.
	 */
	u32 trans_delay = u16_get_bits(eml_cap,
				       IEEE80211_EML_CAP_EMLSR_TRANSITION_DELAY);
	static const u32 trans_delay_us[EMLSR_TRANS_DELAY_MAX] = {
		0, 16, 32, 64, 128, 256
	};

	if (trans_delay >= EMLSR_TRANS_DELAY_MAX)
		return 0;

	return trans_delay_us[trans_delay];
}

static u32 ath12k_wmi_get_emlsr_trans_timeout_us(u16 eml_cap)
{
	/* IEEE Std 802.11be-2024 Table 9-417m—Encoding of the
	 * Transition Timeout subfield.
	 */
	u8 timeout = u16_get_bits(eml_cap, IEEE80211_EML_CAP_TRANSITION_TIMEOUT);
	static const u32 trans_timeout_us[EML_TRANS_TIMEOUT_MAX] = {
		0, 128, 256, 512,
		TU_TO_USEC(1),
		TU_TO_USEC((1U << 1)),
		TU_TO_USEC((1U << 2)),
		TU_TO_USEC((1U << 3)),
		TU_TO_USEC((1U << 4)),
		TU_TO_USEC((1U << 5)),
		TU_TO_USEC((1U << 6)),
	};

	if (timeout >= EML_TRANS_TIMEOUT_MAX)
		return 0;

	return trans_timeout_us[timeout];
}

static void *ath12k_wmi_peer_assoc_v2_cmd(struct ath12k *ar,
					  void *ptr,
					  struct ath12k_wmi_peer_assoc_arg *arg,
					  enum wmi_tlv_cmd_id *cmd_id)
{
	struct wmi_peer_uhr_npca_op_params *npca_params;
	struct wmi_peer_assoc_cip_info *cip_info;
	struct wmi_tlv *tlv;
	struct wmi_peer_assoc_smd_params *smd_params;
	int len;

	if (!test_bit(WMI_SERVICE_EXT_TLV_SUPPORT,
		     ar->ab->wmi_ab.svc_map))
		return ptr;

	*cmd_id = WMI_PEER_ASSOC_V2_CMDID;

	/*
	 * Fill the tlv here for WMI_PEER_ASSOC_V2_CMDID
	 */

	/*
	 * Fill empty TLV's for npca_params
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	/*
	 * Fill empty TLV's for create_mlo_params
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	/* Always emit the CIP TLV to preserve ordering */
	cip_info = ptr;
	cip_info->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_CIP_INFO,
				       sizeof(*cip_info));
	cip_info->cfp_enable = cpu_to_le32(arg->is_cfp_enabled);
	cip_info->cfp_padding_bits = cpu_to_le32(arg->control_mic_pad);

	ptr += sizeof(*cip_info);

	/*
	 * Advertise the SMD params TLV
	 */
	len = sizeof(*smd_params);
	smd_params = ptr;
	smd_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SMD_PEER_ASSOC_PARAMS,
							len);
	ether_addr_copy(smd_params->smd_identifier.addr, arg->smd.smd_mac_addr);
	smd_params->smd_capabilities = arg->smd.smd_enabled;

	if (arg->smd.smd_enabled)
		smd_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_PEER_SMD_ENABLED);

	if (arg->smd.dl_data_fwd)
		smd_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_PEER_SMD_DL_DATA_FWD);

	smd_params->flags |= le32_encode_bits(arg->smd.role,
					      ATH12K_WMI_FLAG_PEER_SMD_ROLE);

	ptr += sizeof(*smd_params);

	/*
	 * Fill empty TLV's for uhr rateset
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	/*
	 * Emit npca op params TLV; include element inside wrapper when enabled.
	 */
	tlv = ptr;
	if (arg->npca.enabled) {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*npca_params));
		ptr += TLV_HDR_SIZE;

		npca_params = ptr;
		npca_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_UHR_NPCA_OP_PARAMS,
					       sizeof(*npca_params));

		npca_params->npca_cap1 =
			le32_encode_bits(arg->npca.npca_primary_channel,
					 WMI_NPCA_PEER_CAP1_CHAN_OFFSET) |
			le32_encode_bits(arg->npca.npca_min_dur_threshold,
					 WMI_NPCA_PEER_CAP1_MIN_THRESHOLD) |
			le32_encode_bits(arg->npca.npca_switch_delay,
					 WMI_NPCA_PEER_CAP1_SWITCH_DELAY) |
			le32_encode_bits(arg->npca.npca_switch_back_delay,
					 WMI_NPCA_PEER_CAP1_SWITCH_BACK_DELAY) |
			le32_encode_bits(arg->npca.npca_initial_qsrc,
					 WMI_NPCA_PEER_CAP1_INITIAL_QSRC) |
			le32_encode_bits(arg->npca.npca_moplen,
					 WMI_NPCA_PEER_CAP1_MOPLEN) |
			(arg->npca.npca_punct_bitmap ?
			 WMI_NPCA_PEER_CAP1_DIS_SUBCHAN_BMAP_PRESENT : 0);
		npca_params->npca_cap2 =
			le32_encode_bits(arg->npca.npca_punct_bitmap,
					 WMI_NPCA_PEER_CAP2_PUNCTURE_BITMAP);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "peer npca params assign: vdev_id=%u npca_primary_channel=%u punct_bitmap=0x%x min_dur=%u switch_delay=%u switch_back=%u init_qsrc=%u moplen=%u cap1=0x%x cap2=0x%x\n",
			   arg->vdev_id,
			   arg->npca.npca_primary_channel,
			   arg->npca.npca_punct_bitmap,
			   arg->npca.npca_min_dur_threshold,
			   arg->npca.npca_switch_delay,
			   arg->npca.npca_switch_back_delay,
			   arg->npca.npca_initial_qsrc,
			   arg->npca.npca_moplen,
			   le32_to_cpu(npca_params->npca_cap1),
			   le32_to_cpu(npca_params->npca_cap2));
		ptr += sizeof(*npca_params);
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	return ptr;
}

int ath12k_wmi_send_peer_assoc_cmd(struct ath12k *ar,
				   struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_assoc_complete_cmd *cmd;
	struct ath12k_wmi_vht_rate_set_params *mcs;
	struct ath12k_wmi_he_rate_set_params *he_mcs;
	struct ath12k_wmi_eht_rate_set_params *eht_mcs;
	struct ath12k_wmi_ttlm_peer_params *ttlm_params;
	struct wmi_peer_assoc_mlo_params *ml_params;
	struct wmi_peer_assoc_mlo_partner_info_params *partner_info;
	struct wmi_peer_assoc_tid_to_link_map *ttlm;
	struct wmi_peer_assoc_msduq_params *msduq_params;
	struct wmi_peer_assoc_mpduq_params *mpduq_params;
	struct wmi_peer_assoc_hol_q_params *holq_params;
	enum wmi_tlv_cmd_id cmd_id = WMI_PEER_ASSOC_CMDID;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 peer_legacy_rates_align, eml_delay, eml_trans_timeout;
	u32 peer_ht_rates_align;
	int i, ret, len, dir, tid_num;
	u16 eml_cap;
	__le32 v;

	peer_legacy_rates_align = roundup(arg->peer_legacy_rates.num_rates,
					  sizeof(u32));
	peer_ht_rates_align = roundup(arg->peer_ht_rates.num_rates,
				      sizeof(u32));

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + (peer_legacy_rates_align * sizeof(u8)) +
	      TLV_HDR_SIZE + (peer_ht_rates_align * sizeof(u8)) +
	      sizeof(*mcs) + TLV_HDR_SIZE +
	      (sizeof(*he_mcs) * arg->peer_he_mcs_count) +
	      TLV_HDR_SIZE + (sizeof(*eht_mcs) * arg->peer_eht_mcs_count);

	if (arg->ml.enabled)
		len += TLV_HDR_SIZE + sizeof(*ml_params) +
		       TLV_HDR_SIZE + (arg->ml.num_partner_links * sizeof(*partner_info));
	else
		len += (2 * TLV_HDR_SIZE);

	len += TLV_HDR_SIZE + (arg->ttlm_params.num_dir * TTLM_MAX_NUM_TIDS *
			       sizeof(struct wmi_peer_assoc_tid_to_link_map));
	len += TLV_HDR_SIZE; // Operating mode

	if (arg->flowq_params.enabled)
		len += TLV_HDR_SIZE + sizeof(*mpduq_params) + TLV_HDR_SIZE +
		       (arg->flowq_params.num_links * sizeof(*msduq_params));
	else
		len += (2 * TLV_HDR_SIZE);

	if (arg->holq_params.enabled)
		len += TLV_HDR_SIZE + sizeof(*holq_params);
	else
		len += TLV_HDR_SIZE;

	if (test_bit(WMI_SERVICE_EXT_TLV_SUPPORT, ar->ab->wmi_ab.svc_map)) {
		/*
		 * Add length for the TLVs which needs to be sent for
		 * peer assoc v2 command
		 */

		/* Dummy TLV inclusion for create mlo params and npca */
		len += (2 * TLV_HDR_SIZE);

		len += sizeof(struct wmi_peer_assoc_cip_info);

		/*
		 * Dummy TLV inclusion for uhr rateset, npca op
		 */
		len += (2 * TLV_HDR_SIZE);

		if (arg->npca.enabled)
			len += sizeof(struct wmi_peer_uhr_npca_op_params);

		len += sizeof(struct wmi_peer_assoc_smd_params);
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_COMPLETE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	cmd->peer_new_assoc = cpu_to_le32(arg->peer_new_assoc);
	cmd->peer_associd = cpu_to_le32(arg->peer_associd);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);

	ath12k_wmi_copy_peer_flags(cmd, arg);

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_mac);

	cmd->peer_rate_caps = cpu_to_le32(arg->peer_rate_caps);
	cmd->peer_caps = cpu_to_le32(arg->peer_caps);
	cmd->peer_listen_intval = cpu_to_le32(arg->peer_listen_intval);
	cmd->peer_ht_caps = cpu_to_le32(arg->peer_ht_caps);
	cmd->peer_max_mpdu = cpu_to_le32(arg->peer_max_mpdu);
	cmd->peer_mpdu_density = cpu_to_le32(arg->peer_mpdu_density);
	cmd->peer_vht_caps = cpu_to_le32(arg->peer_vht_caps);
	cmd->peer_phymode = cpu_to_le32(arg->peer_phymode);

	/* Update 11ax capabilities */
	cmd->peer_he_cap_info = cpu_to_le32(arg->peer_he_cap_macinfo[0]);
	cmd->peer_he_cap_info_ext = cpu_to_le32(arg->peer_he_cap_macinfo[1]);
	cmd->peer_he_cap_info_internal = cpu_to_le32(arg->peer_he_cap_macinfo_internal);
	cmd->peer_he_caps_6ghz = cpu_to_le32(arg->peer_he_caps_6ghz);
	cmd->peer_he_ops = cpu_to_le32(arg->peer_he_ops);
	for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
		cmd->peer_he_cap_phy[i] =
			cpu_to_le32(arg->peer_he_cap_phyinfo[i]);
	cmd->peer_ppet.numss_m1 = cpu_to_le32(arg->peer_ppet.numss_m1);
	cmd->peer_ppet.ru_info = cpu_to_le32(arg->peer_ppet.ru_bit_mask);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cmd->peer_ppet.ppet16_ppet8_ru3_ru0[i] =
			cpu_to_le32(arg->peer_ppet.ppet16_ppet8_ru3_ru0[i]);

	/* Update 11be capabilities */
	memcpy_and_pad(cmd->peer_eht_cap_mac, sizeof(cmd->peer_eht_cap_mac),
		       arg->peer_eht_cap_mac, sizeof(arg->peer_eht_cap_mac),
		       0);
	memcpy_and_pad(cmd->peer_eht_cap_phy, sizeof(cmd->peer_eht_cap_phy),
		       arg->peer_eht_cap_phy, sizeof(arg->peer_eht_cap_phy),
		       0);
	memcpy_and_pad(&cmd->peer_eht_ppet, sizeof(cmd->peer_eht_ppet),
		       &arg->peer_eht_ppet, sizeof(arg->peer_eht_ppet), 0);

	/* Update SAM information */
	if (arg->sta_id != ATH12K_STA_ID_INVALID) {
		cmd->sam_peer_id_valid = 1;
		cmd->sam_peer_id = cpu_to_le32(arg->sta_id);
	}

	/* Update 11bn capabilities */
	memcpy_and_pad(cmd->peer_uhr_cap_mac, sizeof(cmd->peer_uhr_cap_mac),
		       arg->peer_uhr_cap_mac, sizeof(arg->peer_uhr_cap_mac),
		       0);
	memcpy_and_pad(cmd->peer_uhr_cap_phy, sizeof(cmd->peer_uhr_cap_phy),
		       arg->peer_uhr_cap_phy, sizeof(arg->peer_uhr_cap_phy),
		       0);

	/* Update peer legacy rate information */
	ptr += sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_legacy_rates_align);

	ptr += TLV_HDR_SIZE;

	cmd->num_peer_legacy_rates = cpu_to_le32(arg->peer_legacy_rates.num_rates);
	memcpy(ptr, arg->peer_legacy_rates.rates,
	       arg->peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	ptr += peer_legacy_rates_align;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_ht_rates_align);
	ptr += TLV_HDR_SIZE;
	cmd->num_peer_ht_rates = cpu_to_le32(arg->peer_ht_rates.num_rates);
	memcpy(ptr, arg->peer_ht_rates.rates,
	       arg->peer_ht_rates.num_rates);

	/* VHT Rates */
	ptr += peer_ht_rates_align;

	mcs = ptr;

	mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VHT_RATE_SET,
						 sizeof(*mcs));

	cmd->peer_nss = cpu_to_le32(arg->peer_nss);

	/* Update bandwidth-NSS mapping */
	cmd->peer_bw_rxnss_override = 0;
	cmd->peer_bw_rxnss_override |= cpu_to_le32(arg->peer_bw_rxnss_override);

	if (arg->vht_capable) {
		mcs->rx_max_rate = cpu_to_le32(arg->rx_max_rate);
		mcs->rx_mcs_set = cpu_to_le32(arg->rx_mcs_set);
		mcs->tx_max_rate = cpu_to_le32(arg->tx_max_rate);
		mcs->tx_mcs_set = cpu_to_le32(arg->tx_mcs_set);
	}

	/* HE Rates */
	cmd->peer_he_mcs = cpu_to_le32(arg->peer_he_mcs_count);
	cmd->min_data_rate = cpu_to_le32(arg->min_data_rate);

	ptr += sizeof(*mcs);

	len = arg->peer_he_mcs_count * sizeof(*he_mcs);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	/* Loop through the HE rate set */
	for (i = 0; i < arg->peer_he_mcs_count; i++) {
		he_mcs = ptr;
		he_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HE_RATE_SET,
							    sizeof(*he_mcs));

		he_mcs->rx_mcs_set = cpu_to_le32(arg->peer_he_rx_mcs_set[i]);
		he_mcs->tx_mcs_set = cpu_to_le32(arg->peer_he_tx_mcs_set[i]);
		ptr += sizeof(*he_mcs);
	}

	tlv = ptr;
	len = arg->ml.enabled ? sizeof(*ml_params) : 0;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;
	if (!len)
		goto skip_ml_params;

	ml_params = ptr;
	ml_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PEER_ASSOC_PARAMS,
						       len);
	ml_params->flags = cpu_to_le32(ATH12K_WMI_FLAG_MLO_ENABLED);
	ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID);

	if (arg->ml.assoc_link)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_ASSOC_LINK);

	if (arg->ml.primary_umac)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_PRIMARY_UMAC);

	if (arg->ml.logical_link_idx_valid)
		ml_params->flags |=
			cpu_to_le32(ATH12K_WMI_FLAG_MLO_LOGICAL_LINK_IDX_VALID);

	if (arg->ml.peer_id_valid)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_PEER_ID_VALID);

	if (arg->ml.bridge_peer)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

	if (arg->ml.mlo_link_add)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ADD);

	if (arg->ml.mlo_link_del)
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_DEL);

	ether_addr_copy(ml_params->mld_addr.addr, arg->ml.mld_addr);
	ml_params->logical_link_idx = cpu_to_le32(arg->ml.logical_link_idx);
	ml_params->ml_peer_id = cpu_to_le32(arg->ml.ml_peer_id);
	ml_params->ieee_link_id = cpu_to_le32(arg->ml.ieee_link_id);

	if (arg->ml.ml_reconfig)
		ml_params->ml_reconfig = 1;

	if (arg->ml.ml_recovery_reconfig)
		ml_params->ml_recovery_reconfig = 1;

	ml_params->new_master_ll_id = cpu_to_le32(arg->ml.new_master_ll_id);

	eml_cap = arg->ml.eml_cap;
	if (u16_get_bits(eml_cap, IEEE80211_EML_CAP_EMLSR_SUPP)) {
		/* Padding delay */
		eml_delay = ath12k_wmi_get_emlsr_pad_delay_us(eml_cap);
		ml_params->emlsr_padding_delay_us = cpu_to_le32(eml_delay);
		/* Transition delay */
		eml_delay = ath12k_wmi_get_emlsr_trans_delay_us(eml_cap);
		ml_params->emlsr_trans_delay_us = cpu_to_le32(eml_delay);
		/* Transition timeout */
		eml_trans_timeout = ath12k_wmi_get_emlsr_trans_timeout_us(eml_cap);
		ml_params->emlsr_trans_timeout_us = cpu_to_le32(eml_trans_timeout);
		ml_params->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_EMLSR_SUPPORT);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi peer (%pM) emlsr padding delay %u, trans delay %u trans timeout %u",
			   arg->peer_mac, ml_params->emlsr_padding_delay_us,
			   ml_params->emlsr_trans_delay_us,
			   ml_params->emlsr_trans_timeout_us);
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L0,
			 "peer (%pM) ml flags %x mld_addr %pM logical_link_idx %u ml peer id %d ieee_link_id %u num_partner_links %d is_bridge_peer %d\n",
			 arg->peer_mac, ml_params->flags, ml_params->mld_addr.addr,
			 ml_params->logical_link_idx, ml_params->ml_peer_id,
			 ml_params->ieee_link_id,
			 arg->ml.num_partner_links, arg->ml.bridge_peer);

	ptr += sizeof(*ml_params);

skip_ml_params:
	/* Loop through the EHT rate set */
	len = arg->peer_eht_mcs_count * sizeof(*eht_mcs);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	for (i = 0; i < arg->peer_eht_mcs_count; i++) {
		eht_mcs = ptr;
		eht_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_EHT_RATE_SET,
							     sizeof(*eht_mcs));

		eht_mcs->rx_mcs_set = cpu_to_le32(arg->peer_eht_rx_mcs_set[i]);
		eht_mcs->tx_mcs_set = cpu_to_le32(arg->peer_eht_tx_mcs_set[i]);
		ptr += sizeof(*eht_mcs);
	}

	/* Set EHT OPS bit-6 for disabling MCS15 */
	if (!arg->enable_mcs15)
		cmd->peer_eht_ops |= cpu_to_le32(IEEE80211_EHT_OPER_MCS15_DISABLE);

	tlv = ptr;
	len = arg->ml.enabled ? arg->ml.num_partner_links * sizeof(*partner_info) : 0;
	/* fill ML Partner links */
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	if (len == 0)
		goto ttlm;

	for (i = 0; i < arg->ml.num_partner_links; i++) {
		u32 cmd = WMI_TAG_MLO_PARTNER_LINK_PARAMS_PEER_ASSOC;

		partner_info = ptr;
		partner_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(cmd,
								  sizeof(*partner_info));
		partner_info->vdev_id = cpu_to_le32(arg->ml.partner_info[i].vdev_id);
		partner_info->hw_link_id =
			cpu_to_le32(arg->ml.partner_info[i].hw_link_id);
		partner_info->flags = cpu_to_le32(ATH12K_WMI_FLAG_MLO_ENABLED);

		if (arg->ml.partner_info[i].assoc_link)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_ASSOC_LINK);

		if (arg->ml.partner_info[i].primary_umac)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_PRIMARY_UMAC);

		if (arg->ml.partner_info[i].logical_link_idx_valid) {
			v = cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ID_VALID);
			partner_info->flags |= v;
		}

		if (arg->ml.partner_info[i].bridge_peer)
			partner_info->flags |=
				cpu_to_le32(ATH12K_WMI_FLAG_MLO_BRIDGE_PEER);

		if (arg->ml.partner_info[i].mlo_link_add)
			partner_info->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_ADD);

		if (arg->ml.partner_info[i].mlo_link_del)
			partner_info->flags |= cpu_to_le32(ATH12K_WMI_FLAG_MLO_LINK_DEL);

		partner_info->logical_link_idx =
			cpu_to_le32(arg->ml.partner_info[i].logical_link_idx);

		partner_info->ieee_link_id =
			cpu_to_le32(arg->ml.partner_info[i].ieee_link_id);
		partner_info->flags |=
			     cpu_to_le32(ATH12K_WMI_FLAG_MLO_IEEE_LINK_IDX_VALID_PARTNER);

		ptr += sizeof(*partner_info);
	}

ttlm:
	/* add ttlm params */
	ttlm_params = &arg->ttlm_params;
	tlv = ptr;
	len = ttlm_params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	if (!len)
		goto send_mpduq;

	for (dir = 0; dir < ttlm_params->num_dir; dir++) {
		struct ath12k_wmi_host_ttlm_of_tids *ttlm_of_tids = &ttlm_params->ttlm_info[dir];

		for (tid_num = 0; tid_num < TTLM_MAX_NUM_TIDS; tid_num++) {
			ttlm = (struct wmi_peer_assoc_tid_to_link_map *)ptr;
			ttlm->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_TID_TO_LINK_MAP,
						       sizeof(*ttlm) - TLV_HDR_SIZE);
			ttlm->tid_to_link_map_info = 0;
			/* populate tid number */
			ttlm->tid_to_link_map_info |= le32_encode_bits(tid_num, WMI_TTLM_TID_MASK);
			/* populate the direction */
			ttlm->tid_to_link_map_info |= le32_encode_bits(ttlm_of_tids->direction,
								       WMI_TTLM_DIR_MASK);
			/* populate default link mapping value */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->default_link_mapping,
						 WMI_TTLM_DEFAULT_MAPPING_MASK);
			/* populate ttlm provisioned links for the corressponding tid number */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->ttlm_provisioned_links[tid_num],
						 WMI_TTLM_LINK_MAPPING_MASK);
			ptr += sizeof(*ttlm);
		}
	}

send_mpduq:
	/* Operating mode dummy TLV */
	len = 0;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	len = arg->flowq_params.enabled ? sizeof(*mpduq_params) : 0;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;
	if (!len)
		goto send_msduq;
	mpduq_params = ptr;

	mpduq_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MGMT_MPDU_FLOWQ_PARAMS,
							  len);
	mpduq_params->mgmt_mpduq_address =
		cpu_to_le32(arg->flowq_params.mpduq_params.mgmt_mpduq_address);
	mpduq_params->pn_addr_39_32 =
		cpu_to_le32(arg->flowq_params.mpduq_params.pn_addr_39_32);
	mpduq_params->pn_addr_31_0 =
		cpu_to_le32(arg->flowq_params.mpduq_params.pn_addr_31_0);
	ptr += sizeof(*mpduq_params);

send_msduq:
	len = arg->flowq_params.enabled ?
			(arg->flowq_params.num_links * sizeof(*msduq_params)) : 0;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;
	if (!len)
		goto send_holq;
	for (i = 0; i < arg->flowq_params.num_links; i++) {
		struct peer_assoc_msduq_params *msduq;
		u32 cmd = WMI_TAG_MGMT_MSDU_FLOWQ_PARAMS;

		msduq = &arg->flowq_params.msduq_params[i];

		msduq_params = ptr;
		msduq_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(cmd,
								  sizeof(*msduq_params));
		msduq_params->mgmt_msduq_address =
			cpu_to_le32(msduq->mgmt_msduq_address);
		msduq_params->msdu_type =
			le32_encode_bits(msduq->link_id, WMI_MGMTQ_LINK_ID);
		msduq_params->msdu_type |=
			le32_encode_bits(msduq->flow_type, WMI_MGMTQ_MSDU_TYPE);
		ptr += sizeof(*msduq_params);
	}

send_holq:
	len = arg->holq_params.enabled ? sizeof(*holq_params) : 0;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;
	if (!len)
		goto send;
	holq_params = ptr;

	holq_params->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HOL_MSDU_FLOWQ_PARAMS,
							 len);
	holq_params->mpduq_msduq_number =
		le32_encode_bits(arg->holq_params.peer_id, WMI_HOLQ_PEER_ID);
	holq_params->mpduq_msduq_number |=
		le32_encode_bits(arg->holq_params.tid, WMI_HOLQ_TID);
	holq_params->mpduq_msduq_number |=
		le32_encode_bits(arg->holq_params.mpdu_type, WMI_HOLQ_MPDU_TYPE);
	holq_params->mpduq_msduq_number |=
		le32_encode_bits(arg->holq_params.msdu_type, WMI_HOLQ_MSDU_TYPE);
	holq_params->mpduq_address =
		cpu_to_le32(arg->holq_params.mpduq_address);
	holq_params->msduq_address =
		cpu_to_le32(arg->holq_params.msduq_address);
	holq_params->pn_addr_39_32 =
		cpu_to_le32(arg->holq_params.pn_addr_39_32);
	holq_params->pn_addr_31_0 =
		cpu_to_le32(arg->holq_params.pn_addr_31_0);
	ptr += sizeof(*holq_params);

send:
	ptr = ath12k_wmi_peer_assoc_v2_cmd(ar, ptr, arg, &cmd_id);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME, ATH12K_DBG_L0,
			 "wmi peer assoc vdev id %d assoc id %d peer mac %pM peer_flags %x rate_caps %x peer_caps %x listen_intval %d ht_caps %x max_mpdu %d nss %d phymode %d peer_mpdu_density %d vht_caps %x he cap_info %x he ops %x he cap_info_ext %x he phy %x %x %x peer_bw_rxnss_override %x peer_flags_ext %x eht mac_cap %x %x eht phy_cap %x %x %x peer_eht_ops %x uhr mac_cap %x %x uhr phy_cap %x sam_peer_id_valid %d sam_peer_id %x\n",
			 cmd->vdev_id, cmd->peer_associd, arg->peer_mac,
			 cmd->peer_flags, cmd->peer_rate_caps, cmd->peer_caps,
			 cmd->peer_listen_intval, cmd->peer_ht_caps,
			 cmd->peer_max_mpdu, cmd->peer_nss, cmd->peer_phymode,
			 cmd->peer_mpdu_density,
			 cmd->peer_vht_caps, cmd->peer_he_cap_info,
			 cmd->peer_he_ops, cmd->peer_he_cap_info_ext,
			 cmd->peer_he_cap_phy[0], cmd->peer_he_cap_phy[1],
			 cmd->peer_he_cap_phy[2],
			 cmd->peer_bw_rxnss_override, cmd->peer_flags_ext,
			 cmd->peer_eht_cap_mac[0], cmd->peer_eht_cap_mac[1],
			 cmd->peer_eht_cap_phy[0], cmd->peer_eht_cap_phy[1],
			 cmd->peer_eht_cap_phy[2], cmd->peer_eht_ops,
			 cmd->peer_uhr_cap_mac[0], cmd->peer_uhr_cap_mac[1],
			 cmd->peer_uhr_cap_phy[0], cmd->sam_peer_id_valid,
			 cmd->sam_peer_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, cmd_id);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_ASSOC_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
EXPORT_SYMBOL(ath12k_wmi_send_peer_assoc_cmd);
#endif

int ath12k_wmi_send_smd_roam_config(struct ath12k *ar,
				    struct ath12k_wmi_smd_roam_config_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_smd_roam_config_cmd *cmd;
	struct wmi_smd_roam_config_peer_tid_info *tid_info;
	struct sk_buff *skb;
	void *ptr;
	struct wmi_tlv *tlv;
	u32 len;
	int ret;

	/* Calculate the length of the command */
	len = sizeof(*cmd);

	if ((arg->role == SMD_ROAM_CONFIG_ROLE_TARGET_AP &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT) ||
	    (arg->role == SMD_ROAM_CONFIG_ROLE_STA &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT) ||
	    (arg->role == SMD_ROAM_CONFIG_ROLE_SERVING_AP &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_EXEC_REQ))
		len +=  TLV_HDR_SIZE + (IEEE80211_MAX_NUM_TIDS * sizeof(*tid_info));

	/* Allocate an SKB for the command */
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	/* Assign the initial pointer to the starting of the data segment */
	ptr = skb->data;
	cmd = ptr;

	/* Fill in the SKB */
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SMD_ROAM_CONFIG_PARAMS,
				       sizeof(*cmd));
	cmd->vdev_id    = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_mac);
	cmd->cmd_flags  = le32_encode_bits(arg->role,
					   WMI_SMD_ROAM_CONFIG_CMD_FLAGS_ROLE);
	cmd->cmd_flags  |= le32_encode_bits(arg->cmd_type,
					    WMI_SMD_ROAM_CONFIG_CMD_FLAGS_CMD_TYPE);
	cmd->cmd_flags  |= le32_encode_bits(arg->status,
					    WMI_SMD_ROAM_CONFIG_CMD_FLAGS_STATUS);
	cmd->cmd_flags  |= le32_encode_bits(arg->flags,
					    WMI_SMD_ROAM_CONFIG_CMD_FLAGS_FLAGS);
	cmd->dl_drain_time = arg->dl_drain_time;

	/* Add one TLV for every TID with the MLSN number */
	if ((arg->role == SMD_ROAM_CONFIG_ROLE_TARGET_AP &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT) ||
	    (arg->role == SMD_ROAM_CONFIG_ROLE_STA &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_DYNAMIC_CONTEXT) ||
	    (arg->role == SMD_ROAM_CONFIG_ROLE_SERVING_AP &&
	     arg->cmd_type == SMD_ROAM_CONFIG_CMD_EXEC_REQ)) {
		u32 i;

		ptr += sizeof(*cmd);
		tlv = ptr;
		len = IEEE80211_MAX_NUM_TIDS * sizeof(*tid_info);
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < IEEE80211_MAX_NUM_TIDS; i++) {
			tid_info = ptr;
			tid_info->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SMD_ROAM_PEER_TID_INFO,
						       sizeof(*tid_info));
			tid_info->tid_num = i;
			tid_info->mlsn_offset_word |=
				le32_encode_bits(arg->peer_tid_info[i].mlsn,
						 WMI_SMD_ROAM_CONFIG_PEER_TID_INFO_MLSN);
			tid_info->mlsn_offset_word |=
				le32_encode_bits(arg->peer_tid_info[i].offset,
						 WMI_SMD_ROAM_CONFIG_PEER_TID_INFO_OFFS);
			tid_info->tx_ba_window_size =
				arg->peer_tid_info[i].tx_buf_size;
			tid_info->rx_ba_window_size =
				arg->peer_tid_info[i].rx_buf_size;
			ath12k_dbg(ar->ab, ATH12K_DBG_SMD,
				   "tid: %u mlsn_offset_word: 0x%04x tx_ba: %u rx_ba=%u",
				   tid_info->tid_num, tid_info->mlsn_offset_word,
				   tid_info->tx_ba_window_size,
				   tid_info->rx_ba_window_size);
			ptr += sizeof(*tid_info);
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SMD,
		   "wmi smd roam config: vdev_id: %d peer_mac_addr: %pM cmd_flags: 0x%x dl_drain_time: %d\n",
		   cmd->vdev_id, cmd->peer_macaddr.addr,
		   cmd->cmd_flags, cmd->dl_drain_time);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ROAM_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send wmi roam config cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_update_scan_chan_list(struct ath12k *ar,
				     struct ath12k_wmi_scan_req_arg *req_arg)
{
	struct ieee80211_supported_band **bands;
	struct ath12k_wmi_scan_chan_list_arg *arg;
	struct cfg80211_chan_def *chandef;
	struct ieee80211_channel *channel, *req_channel;
	struct ieee80211_hw *hw = ar->ah->hw;
	struct ath12k_wmi_channel_arg *ch;
	enum nl80211_band band;
	int num_channels = 0;
	int i, ret;
	bool found = false;
	u32 low_freq, high_freq;

	/* Snapshot chan_info under data_lock to get a consistent view of the
	 * freq range for both the count pass and the fill pass. Without this,
	 * a concurrent ath12k_regd_update_freq_range() call from another
	 * regulatory update could widen the range between the two passes,
	 * causing the fill pass to write more entries than were allocated.
	 */
	spin_lock_bh(&ar->data_lock);
	low_freq = ar->chan_info.low_freq;
	high_freq = ar->chan_info.high_freq;
	spin_unlock_bh(&ar->data_lock);

	bands = hw->wiphy->bands;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(ar->mac.sbands[band].channels && bands[band]))
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			if (bands[band]->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;

			if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ)
				if (bands[band]->channels[i].center_freq <
				    low_freq ||
				    bands[band]->channels[i].center_freq >
				    high_freq)
					continue;

			num_channels++;
		}
	}

	if (!num_channels) {
		ath12k_warn(ar->ab, "pdev is not supported for this country\n");
		return -EOPNOTSUPP;
	}

	arg = kzalloc(struct_size(arg, channel, num_channels), GFP_KERNEL);

	if (!arg)
		return -ENOMEM;

	arg->pdev_id = ar->pdev->pdev_id;
	arg->nallchans = num_channels;

	ch = arg->channel;
	chandef = req_arg ? req_arg->chandef : NULL;
	req_channel = chandef ? chandef->chan : NULL;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!(ar->mac.sbands[band].channels && bands[band]))
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			channel = &bands[band]->channels[i];

			if (channel->flags & IEEE80211_CHAN_DISABLED)
				continue;

			if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
				if (bands[band]->channels[i].center_freq <
				    low_freq ||
				    bands[band]->channels[i].center_freq >
				    high_freq) {
					ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
						   "skip freq %d outside range %d-%d MHz\n",
						   bands[band]->channels[i].center_freq,
						   low_freq,
						   high_freq);
					continue;
				}
			}

                       if (req_channel && !found &&
			    req_channel->center_freq == channel->center_freq) {
                               ch->mhz = req_arg->chan_list.chan[0].freq;
                               ch->cfreq1 = chandef->center_freq1;
                               ch->cfreq2 = chandef->center_freq2;

                               ch->phy_mode = req_arg->chan_list.chan[0].phymode;
                               channel = req_channel;
                               found = true;
                       } else {
                               ch->mhz = channel->center_freq;
                               ch->cfreq1 = channel->center_freq;
                               ch->phy_mode = (channel->band == NL80211_BAND_2GHZ) ?
                                               MODE_11G : MODE_11A;
                       }

			/* TODO: Set to true/false based on some condition? */
			ch->allow_ht = true;
			ch->allow_vht = true;
			ch->allow_he = true;

			ch->dfs_set =
				!!(channel->flags & IEEE80211_CHAN_RADAR);
			ch->is_chan_passive = !!(channel->flags &
						IEEE80211_CHAN_NO_IR);
			ch->is_chan_passive |= ch->dfs_set;
			ch->minpower = 0;
			ch->maxpower = channel->max_power * 2;
			ch->maxregpower = channel->max_reg_power * 2;
			ch->antennamax = channel->max_antenna_gain * 2;

			if (channel->band == NL80211_BAND_6GHZ &&
			    cfg80211_channel_is_psc(channel))
				ch->psc_channel = true;

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "mac channel [%d/%d] freq %d maxpower %d "
				   "regpower %d antenna %d cfreq1 %d mode %d\n",
				   i, arg->nallchans,
				   ch->mhz, ch->maxpower, ch->maxregpower,
				   ch->antennamax, ch->cfreq1, ch->phy_mode);

			ch++;
			/* TODO: use quarrter/half rate, cfreq12, dfs_cfreq2
			 * set_agile, reg_class_idx
			 */
		}
	}

	ret = ath12k_wmi_send_scan_chan_list_cmd(ar, arg);
	kfree(arg);

	return ret;
}

void ath12k_wmi_start_scan_init(struct ath12k *ar,
				struct ath12k_wmi_scan_req_arg *arg,
				enum nl80211_iftype vif_type)
{
	/* setup commonly used values */
	arg->scan_req_id = 1;
	arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	arg->dwell_time_active = 50;
	arg->dwell_time_active_2g = 0;
	arg->dwell_time_passive = 150;
	arg->dwell_time_active_6g = 70;
	arg->dwell_time_passive_6g = 70;
	arg->min_rest_time = 50;
	arg->max_rest_time = 500;

	if (vif_type == NL80211_IFTYPE_STATION) {
		arg->scan_priority = WMI_SCAN_PRIORITY_HIGH;
		arg->dwell_time_active = 70;
		arg->dwell_time_active_2g = 70;
		arg->dwell_time_active_6g = 80;
		arg->dwell_time_passive_6g = 80;
	}

	if (ar->scan_min_rest_time)
		arg->min_rest_time = ar->scan_min_rest_time;
	if (ar->scan_max_rest_time)
		arg->max_rest_time = ar->scan_max_rest_time;

	arg->repeat_probe_time = 0;
	arg->probe_spacing_time = 0;
	arg->idle_time = 0;
	arg->max_scan_time = 20000;
	arg->probe_delay = 5;
	arg->notify_scan_events = WMI_SCAN_EVENT_STARTED |
				  WMI_SCAN_EVENT_COMPLETED |
				  WMI_SCAN_EVENT_BSS_CHANNEL |
				  WMI_SCAN_EVENT_FOREIGN_CHAN |
				  WMI_SCAN_EVENT_DEQUEUED;
	arg->scan_f_chan_stat_evnt = 1;
	arg->num_bssid = 1;

	/* fill bssid_list[0] with 0xff, otherwise bssid and RA will be
	 * ZEROs in probe request
	 */
	eth_broadcast_addr(arg->bssid_list[0].addr);
#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_start_scan_init_extn(ar, arg);
#endif
}

static void ath12k_wmi_copy_scan_event_cntrl_flags(struct wmi_start_scan_cmd *cmd,
						   struct ath12k_wmi_scan_req_arg *arg)
{
	/* Scan events subscription */
	if (arg->scan_ev_started)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_STARTED);
	if (arg->scan_ev_completed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_COMPLETED);
	if (arg->scan_ev_bss_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_BSS_CHANNEL);
	if (arg->scan_ev_foreign_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN);
	if (arg->scan_ev_dequeued)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_DEQUEUED);
	if (arg->scan_ev_preempted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_PREEMPTED);
	if (arg->scan_ev_start_failed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_START_FAILED);
	if (arg->scan_ev_restarted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESTARTED);
	if (arg->scan_ev_foreign_chn_exit)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT);
	if (arg->scan_ev_suspended)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_SUSPENDED);
	if (arg->scan_ev_resumed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESUMED);

	/** Set scan control flags */
	cmd->scan_ctrl_flags = 0;
	if (arg->scan_f_passive)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_PASSIVE);
	if (arg->scan_f_strict_passive_pch)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_STRICT_PASSIVE_ON_PCHN);
	if (arg->scan_f_promisc_mode)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROMISCUOS);
	if (arg->scan_f_capture_phy_err)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CAPTURE_PHY_ERROR);
	if (arg->scan_f_half_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_HALF_RATE_SUPPORT);
	if (arg->scan_f_quarter_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_QUARTER_RATE_SUPPORT);
	if (arg->scan_f_cck_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_CCK_RATES);
	if (arg->scan_f_ofdm_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_OFDM_RATES);
	if (arg->scan_f_chan_stat_evnt)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CHAN_STAT_EVENT);
	if (arg->scan_f_filter_prb_req)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROBE_REQ);
	if (arg->scan_f_bcast_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_BCAST_PROBE_REQ);
	if (arg->scan_f_offchan_mgmt_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_MGMT_TX);
	if (arg->scan_f_offchan_data_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_DATA_TX);
	if (arg->scan_f_force_active_dfs_chn)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS);
	if (arg->scan_f_add_tpc_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_TPC_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_ds_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_spoofed_mac_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_SPOOF_MAC_IN_PROBE_REQ);
	if (arg->scan_f_add_rand_seq_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_RANDOM_SEQ_NO_IN_PROBE_REQ);
	if (arg->scan_f_en_ie_whitelist_in_probe)
		cmd->scan_ctrl_flags |=
			cpu_to_le32(WMI_SCAN_ENABLE_IE_WHTELIST_IN_PROBE_REQ);
	if (arg->scan_f_higher_mcs_nac_scan)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_HIGHER_MCS_NAC_SCAN);

	cmd->scan_ctrl_flags |= le32_encode_bits(arg->adaptive_dwell_time_mode,
						 WMI_SCAN_DWELL_MODE_MASK);
}

int ath12k_wmi_send_scan_start_cmd(struct ath12k *ar,
				   struct ath12k_wmi_scan_req_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct wmi_start_scan_cmd *cmd;
	struct ath12k_wmi_ssid_params *ssid = NULL;
	struct ath12k_wmi_mac_addr_params *bssid;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u32 *tmp_ptr, extraie_len_with_pad = 0;
	u8 *phy_ptr;
	struct ath12k_wmi_hint_short_ssid_arg *s_ssid = NULL;
	struct ath12k_wmi_hint_bssid_arg *hint_bssid = NULL;
	u8 phymode_roundup = 0;

	len = sizeof(*cmd);

	len += TLV_HDR_SIZE;
	if (arg->chan_list.num_chan)
		len += arg->chan_list.num_chan * sizeof(u32);

	len += TLV_HDR_SIZE;
	if (arg->num_ssids)
		len += arg->num_ssids * sizeof(*ssid);

	if (arg->num_bssid > WLAN_SCAN_MAX_NUM_BSSID)
		return -EINVAL;

	len += TLV_HDR_SIZE;
	if (arg->num_bssid)
		len += sizeof(*bssid) * arg->num_bssid;

	if (arg->num_hint_bssid > WLAN_SCAN_MAX_HINT_BSSID)
		return -EINVAL;

	if (arg->num_hint_bssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_bssid * sizeof(*hint_bssid);

	if (arg->num_hint_s_ssid > WLAN_SCAN_MAX_HINT_S_SSID)
		return -EINVAL;

	if (arg->num_hint_s_ssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_s_ssid * sizeof(*s_ssid);

	len += TLV_HDR_SIZE;
	if (arg->extraie.len && arg->extraie.len <= 0xFFFF)
		extraie_len_with_pad =
			roundup(arg->extraie.len, sizeof(u32));
	if (extraie_len_with_pad <= (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len)) {
		len += extraie_len_with_pad;
	} else {
		ath12k_warn(ar->ab, "discard large size %d bytes extraie for scan start\n",
			    arg->extraie.len);
		extraie_len_with_pad = 0;
	}

	len += TLV_HDR_SIZE;

	if (arg->ie_whitelist.num_vendor_oui > PROBE_REQ_MAX_OUIS)
		return -EINVAL;

	if (arg->scan_f_en_ie_whitelist_in_probe)
		len += arg->ie_whitelist.num_vendor_oui *
				sizeof(struct wmi_vendor_oui);

	len += TLV_HDR_SIZE;
	if (arg->scan_f_wide_band)
		phymode_roundup =
			roundup(arg->chan_list.num_chan * sizeof(u8),
				sizeof(u32));
	len += phymode_roundup;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_START_SCAN_CMD,
						 sizeof(*cmd));

	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->scan_req_id = cpu_to_le32(arg->scan_req_id);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	if (ar->state_11d == ATH12K_11D_PREPARING)
		arg->scan_priority = WMI_SCAN_PRIORITY_MEDIUM;
	else
		arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
	cmd->notify_scan_events = cpu_to_le32(arg->notify_scan_events);

	/* Set NAC scan flag if any NRP is set already */
	spin_lock_bh(&dp->dp_lock);
	if (!list_empty(&dp->neighbor_peers))
		arg->scan_f_higher_mcs_nac_scan = true;
	spin_unlock_bh(&dp->dp_lock);

#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_offchan_txrx_update_scan_params_extn(ar, arg);
#endif /* CPTCFG_QCN_EXTN */

	ath12k_wmi_copy_scan_event_cntrl_flags(cmd, arg);

	cmd->scan_priority = cpu_to_le32(arg->scan_priority);
	cmd->dwell_time_active = cpu_to_le32(arg->dwell_time_active);
	cmd->dwell_time_active_2g = cpu_to_le32(arg->dwell_time_active_2g);
	cmd->dwell_time_passive = cpu_to_le32(arg->dwell_time_passive);
	cmd->dwell_time_active_6g = cpu_to_le32(arg->dwell_time_active_6g);
	cmd->dwell_time_passive_6g = cpu_to_le32(arg->dwell_time_passive_6g);
	cmd->min_rest_time = cpu_to_le32(arg->min_rest_time);
	cmd->max_rest_time = cpu_to_le32(arg->max_rest_time);
	cmd->repeat_probe_time = cpu_to_le32(arg->repeat_probe_time);
	cmd->probe_spacing_time = cpu_to_le32(arg->probe_spacing_time);
	cmd->idle_time = cpu_to_le32(arg->idle_time);
	cmd->max_scan_time = cpu_to_le32(arg->max_scan_time);
	cmd->probe_delay = cpu_to_le32(arg->probe_delay);
	cmd->burst_duration = cpu_to_le32(arg->burst_duration);
	cmd->num_chan = cpu_to_le32(arg->chan_list.num_chan);
	cmd->num_bssid = cpu_to_le32(arg->num_bssid);
	cmd->num_ssids = cpu_to_le32(arg->num_ssids);
	cmd->ie_len = cpu_to_le32(arg->extraie.len);
	cmd->n_probes = cpu_to_le32(arg->n_probes);
#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_send_scan_start_cmd_extn(ar, cmd, arg);
#endif

	ptr += sizeof(*cmd);

	len = arg->chan_list.num_chan * sizeof(u32);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, len);
	ptr += TLV_HDR_SIZE;
	tmp_ptr = (u32 *)ptr;

	for (i = 0; i < arg->chan_list.num_chan; ++i)
		tmp_ptr[i] = arg->chan_list.chan[i].freq;

	ptr += len;

	len = arg->num_ssids * sizeof(*ssid);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;

	if (arg->num_ssids) {
		ssid = ptr;
		for (i = 0; i < arg->num_ssids; ++i) {
			ssid->ssid_len = cpu_to_le32(arg->ssid[i].ssid_len);
			memcpy(ssid->ssid, arg->ssid[i].ssid,
			       arg->ssid[i].ssid_len);
			ssid++;
		}
	}

	ptr += (arg->num_ssids * sizeof(*ssid));
	len = arg->num_bssid * sizeof(*bssid);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;
	bssid = ptr;

	if (arg->num_bssid) {
		for (i = 0; i < arg->num_bssid; ++i) {
			ether_addr_copy(bssid->addr,
					arg->bssid_list[i].addr);
			bssid++;
		}
	}

	ptr += arg->num_bssid * sizeof(*bssid);

	len = extraie_len_with_pad;
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, len);
	ptr += TLV_HDR_SIZE;

	if (extraie_len_with_pad)
		memcpy(ptr, arg->extraie.ptr,
		       arg->extraie.len);

	ptr += extraie_len_with_pad;

       len = arg->ie_whitelist.num_vendor_oui * sizeof(struct wmi_vendor_oui);
       tlv = ptr;
       tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
                     FIELD_PREP(WMI_TLV_LEN, len);
       ptr += TLV_HDR_SIZE;

       if (arg->scan_f_en_ie_whitelist_in_probe) {
               /* TODO: fill vendor OUIs for probe req ie whitelisting */
               /* currently added for FW TLV validation */
       }

       ptr += cmd->num_vendor_oui * sizeof(struct wmi_vendor_oui);

       len = phymode_roundup;
       tlv = ptr;
       tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_BYTE) |
                     FIELD_PREP(WMI_TLV_LEN, len);
       ptr += TLV_HDR_SIZE;

       /* Wide Band Scan */
       if (arg->scan_f_wide_band) {
               phy_ptr = ptr;
               /* Add PHY mode TLV for wide band scan with phymode + 1 value
                * so that phymode '0' is ignored by FW as default value.
                */
               for (i = 0; i < arg->chan_list.num_chan; ++i)
                       phy_ptr[i] = arg->chan_list.chan[i].phymode + 1;
       }
       ptr += phymode_roundup;

	if (arg->num_hint_s_ssid) {
		len = arg->num_hint_s_ssid * sizeof(*s_ssid);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
		s_ssid = ptr;
		for (i = 0; i < arg->num_hint_s_ssid; ++i) {
			s_ssid->freq_flags = arg->hint_s_ssid[i].freq_flags;
			s_ssid->short_ssid = arg->hint_s_ssid[i].short_ssid;
			s_ssid++;
		}
		ptr += len;
	}

	if (arg->num_hint_bssid) {
		len = arg->num_hint_bssid * sizeof(struct ath12k_wmi_hint_bssid_arg);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
		hint_bssid = ptr;
		for (i = 0; i < arg->num_hint_bssid; ++i) {
			hint_bssid->freq_flags =
				arg->hint_bssid[i].freq_flags;
			ether_addr_copy(&hint_bssid->bssid.addr[0],
					&arg->hint_bssid[i].bssid.addr[0]);
			hint_bssid++;
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_START_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_START_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* HOST_TO_FW_DBM_MULTIPLIER - Scaling factor for dBm (in 0.25dBm units) values
 * sent to firmware
 */
#define HOST_TO_FW_DBM_MULTIPLIER 4
/**
 * ath12k_wmi_fill_tpc_power_cmd_header - Populate the fixed header fields
 *                                        for the TPC power WMI command
 * @cmd: Pointer to the WMI command structure to be filled
 * @vdev_id: Virtual device identifier
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function initializes the fixed portion of the
 * WMI_VDEV_SET_TPC_POWER_CMD structure. It sets the TLV header,
 * virtual device ID, PSD power flag, EIRP power (converted to firmware
 * units), and 6 GHz power type.
 */
static void
ath12k_wmi_fill_tpc_power_cmd_header(struct wmi_vdev_set_tpc_power_cmd *cmd,
				     u32 vdev_id,
				     struct ath12k_reg_tpc_power_info *param)
{
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_TPC_POWER_CMD, sizeof(*cmd));
	cmd->vdev_id = vdev_id;
	cmd->psd_power = param->is_psd_power;
	cmd->eirp_power = param->eirp_power * HOST_TO_FW_DBM_MULTIPLIER;
	cmd->power_type_6ghz = param->power_type_6g;
}

/**
 * ath12k_wmi_fill_empty_common_power_tlv - Fill an empty TLV for common power
 * info.
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 *
 * This helper function inserts a placeholder TLV for the common power info
 * array in the WMI command buffer. The TLV is tagged as an array of structures
 * but has a length of zero, indicating that no common power entries are present.
 *
 * This is required to maintain the expected TLV structure layout in the
 * firmware interface, even when no common power data is provided.
 */
static void
ath12k_wmi_fill_empty_common_power_tlv(u8 **ptr)
{
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 0 * sizeof(struct wmi_vdev_ch_power_info));
	*ptr += TLV_HDR_SIZE;
}

/**
 * ath12k_wmi_fill_psd_power_array - Populate the PSD power TLV array in the WMI
 * buffer.
 * @ar: Pointer to the ath12k device context
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function fills the WMI buffer with an array of TLVs representing
 * per-channel PSD (Power Spectral Density) power levels. Each entry includes
 * the channel center frequency and the corresponding PSD power value, scaled
 * for firmware consumption.
 *
 * The function also logs each entry for debugging purposes and advances the
 * buffer pointer accordingly.
 */
static void
ath12k_wmi_fill_psd_power_array(struct ath12k *ar, u8 **ptr,
				struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_ch_power_psd_info *ch_power_psd_info;
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);
	u32 psd_info_tlv_header;
	int i;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 param->num_psd_pwr_levels * sizeof(*ch_power_psd_info));
	*ptr += TLV_HDR_SIZE;

	psd_info_tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CH_PSD_POWER_INFO,
						     sizeof(*ch_power_psd_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "PSD Array:\n");

	for (i = 0; i < param->num_psd_pwr_levels; ++i) {
		ch_power_psd_info = (struct wmi_vdev_ch_power_psd_info *)(*ptr);
		ch_power_psd_info->tlv_header = psd_info_tlv_header;
		ch_power_psd_info->chan_cfreq =
			param->chan_psd_power_info[i].chan_cfreq;
		ch_power_psd_info->psd_power =
			param->chan_psd_power_info[i].tx_power *
			HOST_TO_FW_DBM_MULTIPLIER;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "chan_cfreq = %u, psd_power = %d\n",
			   ch_power_psd_info->chan_cfreq,
			   ch_power_psd_info->psd_power);

		*ptr += sizeof(*ch_power_psd_info);
	}
}

/**
 * ath12k_wmi_fill_eirp_power_array - Populate the EIRP power TLV array in the
 * WMI buffer
 * @ar: Pointer to the ath12k device context
 * @ptr: Pointer to the current position in the WMI buffer; will be updated
 * @param: Pointer to the TPC power configuration parameters
 *
 * This helper function fills the WMI buffer with an array of TLVs representing
 * per-channel EIRP (Equivalent Isotropically Radiated Power) power levels.
 * Each entry includes the channel center frequency and the corresponding
 * EIRP power value, scaled for firmware consumption.
 *
 * The function logs each entry for debugging and advances the buffer pointer
 * accordingly to maintain correct TLV structure alignment.
 */
static void
ath12k_wmi_fill_eirp_power_array(struct ath12k *ar, u8 **ptr,
				 struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_ch_power_eirp_info *ch_power_eirp_info;
	struct wmi_tlv *tlv = (struct wmi_tlv *)(*ptr);
	u32 eirp_info_tlv_header;
	int i;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 param->num_eirp_pwr_levels * sizeof(*ch_power_eirp_info));
	*ptr += TLV_HDR_SIZE;

	eirp_info_tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CH_EIRP_POWER_INFO,
						      sizeof(*ch_power_eirp_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "EIRP Array:\n");

	for (i = 0; i < param->num_eirp_pwr_levels; ++i) {
		ch_power_eirp_info =
			(struct wmi_vdev_ch_power_eirp_info *)(*ptr);
		ch_power_eirp_info->tlv_header = eirp_info_tlv_header;
		ch_power_eirp_info->chan_cfreq =
				param->chan_eirp_power_info[i].chan_cfreq;
		ch_power_eirp_info->eirp_power =
		    param->chan_eirp_power_info[i].tx_power *
		    HOST_TO_FW_DBM_MULTIPLIER;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "chan_cfreq = %u, eirp_power = %d\n",
			   ch_power_eirp_info->chan_cfreq,
			   ch_power_eirp_info->eirp_power);

		*ptr += sizeof(*ch_power_eirp_info);
	}
}

/**
 * ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp - Send WMI command
 * with both PSD and EIRP power levels
 * @ar: Pointer to ath12k device context
 * @vdev_id: Virtual device ID
 * @param: Pointer to TPC power info structure containing PSD and EIRP data
 *
 * Constructs and sends a WMI_VDEV_SET_TPC_POWER_CMD to firmware with both PSD
 * and EIRP values for 6 GHz Standard Power (SP) AP mode. The function allocates
 * a WMI buffer, populates the command and TLV structures for PSD and EIRP power
 * levels, and dispatches the command to firmware. It also includes debug
 * logging and error handling for traceability and robustness.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int
ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp(struct ath12k *ar,
							u32 vdev_id,
							struct ath12k_reg_tpc_power_info *param)
{
	struct wmi_vdev_set_tpc_power_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int len, ret;
	u8 *ptr;

	len = sizeof(*cmd) + TLV_HDR_SIZE;

	if (param->num_psd_pwr_levels > ATH12K_NUM_PWR_LEVELS ||
	    param->num_eirp_pwr_levels > ATH12K_MAX_EIRP_VALS)
		return -EINVAL;

	len += TLV_HDR_SIZE + (sizeof(struct wmi_vdev_ch_power_psd_info) *
			param->num_psd_pwr_levels);
	len += TLV_HDR_SIZE + (sizeof(struct wmi_vdev_ch_power_eirp_info) *
			param->num_eirp_pwr_levels);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_vdev_set_tpc_power_cmd *)ptr;

	ath12k_wmi_fill_tpc_power_cmd_header(cmd, vdev_id, param);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi TPC vdev_id: %d is_psd_power: %d eirp_power: %d power_type_6g: %d\n",
		   vdev_id, param->is_psd_power, param->eirp_power,
		   param->power_type_6g);

	ptr += sizeof(*cmd);

	ath12k_wmi_fill_empty_common_power_tlv(&ptr);
	ath12k_wmi_fill_psd_power_array(ar, &ptr, param);
	ath12k_wmi_fill_eirp_power_array(ar, &ptr, param);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_TPC_POWER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_VDEV_SET_TPC_POWER_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_vdev_set_tpc_power(struct ath12k *ar,
                                       u32 vdev_id,
                                       struct ath12k_reg_tpc_power_info *param)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_vdev_set_tpc_power_cmd *cmd;
        struct wmi_vdev_ch_power_info *ch;
        struct sk_buff *skb;
        struct wmi_tlv *tlv;
        u8 *ptr;
        int i, ret, len;

	if (param->num_pwr_levels > ATH12K_NUM_PWR_LEVELS)
		return -EINVAL;

	if (test_bit(WMI_TLV_SERVICE_BOTH_PSD_EIRP_FOR_AP_SP_CLIENT_SP_SUPPORT,
		     ar->ab->wmi_ab.svc_map) &&
	    (param->power_type_6g == WMI_REG_STD_POWER_AP ||
		 param->power_type_6g == REG_SP_CLIENT_TYPE))
		return ath12_wmi_send_vdev_set_both_psd_and_eirp_in_tpc_for_sp(ar,
							vdev_id, param);

        len = sizeof(*cmd) + TLV_HDR_SIZE;
        len += (sizeof(struct wmi_vdev_ch_power_info) * param->num_pwr_levels);

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
        if (!skb)
                return -ENOMEM;

        ptr = skb->data;

        cmd = (struct wmi_vdev_set_tpc_power_cmd *)ptr;
        cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_VDEV_SET_TPC_POWER_CMD) |
                          FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
        cmd->vdev_id = vdev_id;
        cmd->psd_power = param->is_psd_power;
        cmd->eirp_power = param->eirp_power;
        cmd->power_type_6ghz = param->power_type_6g;
        ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                   "wmi TPC vdev_id: %d is_psd_power: %d eirp_power: %d power_type_6g: %d\n",
                   vdev_id, param->is_psd_power, param->eirp_power, param->power_type_6g);

        ptr += sizeof(*cmd);
        tlv = (struct wmi_tlv *)ptr;
        tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_STRUCT) |
                      FIELD_PREP(WMI_TLV_LEN, param->num_pwr_levels * sizeof(*ch));

        ptr += TLV_HDR_SIZE;
        ch = (struct wmi_vdev_ch_power_info *)ptr;

        for (i = 0; i < param->num_pwr_levels; i++, ch++) {
                ch->tlv_header = FIELD_PREP(WMI_TLV_TAG,
                                            WMI_TAG_VDEV_CH_POWER_INFO) |
                                FIELD_PREP(WMI_TLV_LEN,
                                           sizeof(*ch) - TLV_HDR_SIZE);

                ch->chan_cfreq = param->chan_power_info[i].chan_cfreq;
                ch->tx_power = param->chan_power_info[i].tx_power;

                ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                           "wmi TPC chan_cfreq: %d , tx_power: %d\n",
                           ch->chan_cfreq, ch->tx_power);
        }

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_VDEV_SET_TPC_POWER_CMDID);
        if (ret) {
                ath12k_warn(ar->ab, "failed to send WMI_VDEV_SET_TPC_POWER_CMDID\n");
                dev_kfree_skb(skb);
        }
        return ret;
}

int ath12k_wmi_send_vdev_get_tpc_ie_power(struct ath12k *ar, u32 vdev_id,
					  u32 mgmt_rate)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_get_tpc_ie_power_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_get_tpc_ie_power_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_GET_TPC_IE_POWER_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));
	cmd->mgmt_rate = cpu_to_le32(mgmt_rate);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_GET_TPC_IE_POWER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_GET_TPC_IE_POWER_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_scan_stop_cmd(struct ath12k *ar,
				  struct ath12k_wmi_scan_cancel_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_stop_scan_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_stop_scan_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STOP_SCAN_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->requestor = cpu_to_le32(arg->requester);
	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);
	/* stop the scan with the corresponding scan_id */
	if (arg->req_type == WLAN_SCAN_CANCEL_PDEV_ALL) {
		/* Cancelling all scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_VDEV_ALL) {
		/* Cancelling VAP scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_VAP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_SINGLE) {
		/* Cancelling specific scan */
		cmd->req_type = WMI_SCAN_STOP_ONE;
	} else {
		ath12k_warn(ar->ab, "invalid scan cancel req_type %d",
			    arg->req_type);
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_STOP_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STOP_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_scan_chan_list_cmd(struct ath12k *ar,
				       struct ath12k_wmi_scan_chan_list_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_scan_chan_list_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan_info;
	struct ath12k_wmi_channel_arg *channel_arg;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, len;
	u16 num_send_chans, num_sends = 0, max_chan_limit = 0;
	__le32 *reg1, *reg2;

	channel_arg = &arg->channel[0];
	while (arg->nallchans) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_chan_limit = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len) /
			sizeof(*chan_info);

		num_send_chans = min3(arg->nallchans, max_chan_limit,
				      ATH12K_WMI_MAX_NUM_CHAN_PER_CMD);

		arg->nallchans -= num_send_chans;
		len += sizeof(*chan_info) * num_send_chans;

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_scan_chan_list_cmd *)skb->data;
		cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SCAN_CHAN_LIST_CMD,
							 sizeof(*cmd));
		cmd->pdev_id = cpu_to_le32(arg->pdev_id);
		cmd->num_scan_chans = cpu_to_le32(num_send_chans);
		if (num_sends)
			cmd->flags |= cpu_to_le32(WMI_APPEND_TO_EXISTING_CHAN_LIST_FLAG);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "WMI no.of chan = %d len = %d pdev_id = %d num_sends = %d\n",
			   num_send_chans, len, cmd->pdev_id, num_sends);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*chan_info) * num_send_chans;
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT,
						     len);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_send_chans; ++i) {
			chan_info = ptr;
			memset(chan_info, 0, sizeof(*chan_info));
			len = sizeof(*chan_info);
			chan_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
								       len);

			reg1 = &chan_info->reg_info_1;
			reg2 = &chan_info->reg_info_2;
			chan_info->mhz = cpu_to_le32(channel_arg->mhz);
			chan_info->band_center_freq1 = cpu_to_le32(channel_arg->cfreq1);
			chan_info->band_center_freq2 = cpu_to_le32(channel_arg->cfreq2);

			if (channel_arg->is_chan_passive)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
			if (channel_arg->allow_he)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
			else if (channel_arg->allow_vht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
			else if (channel_arg->allow_ht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
			if (channel_arg->half_rate)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_HALF_RATE);
			if (channel_arg->quarter_rate)
				chan_info->info |=
					cpu_to_le32(WMI_CHAN_INFO_QUARTER_RATE);

			if (channel_arg->psc_channel)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PSC);

			if (channel_arg->dfs_set)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);

			chan_info->info |= le32_encode_bits(channel_arg->phy_mode,
							    WMI_CHAN_INFO_MODE);
			*reg1 |= le32_encode_bits(channel_arg->minpower,
						  WMI_CHAN_REG_INFO1_MIN_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxpower,
						  WMI_CHAN_REG_INFO1_MAX_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxregpower,
						  WMI_CHAN_REG_INFO1_MAX_REG_PWR);
			*reg1 |= le32_encode_bits(channel_arg->reg_class_id,
						  WMI_CHAN_REG_INFO1_REG_CLS);
			*reg2 |= le32_encode_bits(channel_arg->antennamax,
						  WMI_CHAN_REG_INFO2_ANT_MAX);
			*reg2 |= le32_encode_bits(channel_arg->maxregpower,
						  WMI_CHAN_REG_INFO2_MAX_TX_PWR);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "WMI chan scan list chan[%d] = %u, chan_info->info %8x\n",
				   i, chan_info->mhz, chan_info->info);

			ptr += sizeof(*chan_info);

			channel_arg++;
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SCAN_CHAN_LIST_CMDID);
		if (ret) {
			ath12k_warn(ar->ab, "failed to send WMI_SCAN_CHAN_LIST cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		num_sends++;
	}

	return 0;
}

int ath12k_wmi_send_wmm_update_cmd(struct ath12k *ar, u32 vdev_id,
				   struct wmi_wmm_params_all_arg *param,
				   u32 wmm_param_type)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_wmm_params_cmd *cmd;
	struct wmi_wmm_params *wmm_param;
	struct wmi_wmm_params_arg *wmi_wmm_arg;
	struct sk_buff *skb;
	int ret, ac;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_wmm_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->wmm_param_type = cpu_to_le32(wmm_param_type);

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		switch (ac) {
		case WME_AC_BE:
			wmi_wmm_arg = &param->ac_be;
			break;
		case WME_AC_BK:
			wmi_wmm_arg = &param->ac_bk;
			break;
		case WME_AC_VI:
			wmi_wmm_arg = &param->ac_vi;
			break;
		case WME_AC_VO:
			wmi_wmm_arg = &param->ac_vo;
			break;
		}

		wmm_param = (struct wmi_wmm_params *)&cmd->wmm_params[ac];
		wmm_param->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
					       sizeof(*wmm_param));

		wmm_param->aifs = cpu_to_le32(wmi_wmm_arg->aifs);
		wmm_param->cwmin = cpu_to_le32(wmi_wmm_arg->cwmin);
		wmm_param->cwmax = cpu_to_le32(wmi_wmm_arg->cwmax);
		wmm_param->txoplimit = cpu_to_le32(wmi_wmm_arg->txop);
		wmm_param->acm = cpu_to_le32(wmi_wmm_arg->acm);
		wmm_param->no_ack = cpu_to_le32(wmi_wmm_arg->no_ack);

		ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L3,
				 "wmi wmm set ac %d aifs %d cwmin %d cwmax %d txop %d acm %d no_ack %d\n",
				 ac, wmm_param->aifs, wmm_param->cwmin,
				 wmm_param->cwmax, wmm_param->txoplimit,
				 wmm_param->acm, wmm_param->no_ack);
	}
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_WMM_PARAMS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_WMM_PARAMS_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_dfs_phyerr_offload_enable_cmd(struct ath12k *ar,
						  u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_dfs_phyerr_offload_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_dfs_phyerr_offload_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI dfs phy err offload enable pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_bios_cmd(struct ath12k_base *ab, u32 param_id,
			    const u8 *buf, size_t buf_len)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_interface_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	u8 *ptr;
	u32 len, len_aligned;
	int ret;

	len_aligned = roundup(buf_len, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_interface_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_INTERFACE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->param_type_id = cpu_to_le32(param_id);
	cmd->length = cpu_to_le32(buf_len);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, len_aligned);
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, buf, buf_len);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_INTERFACE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_INTERFACE_CMDID parameter id %d: %d\n",
			    param_id, ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_bios_sar_cmd(struct ath12k_base *ab, const u8 *psar_table)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_sar_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret;
	u8 *buf_ptr;
	u32 len, sar_table_len_aligned, sar_dbs_backoff_len_aligned;
	const u8 *psar_value = psar_table + ATH12K_ACPI_POWER_LIMIT_DATA_OFFSET;
	const u8 *pdbs_value = psar_table + ATH12K_ACPI_DBS_BACKOFF_DATA_OFFSET;

	sar_table_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_TABLE_LEN, sizeof(u32));
	sar_dbs_backoff_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN,
					      sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + sar_table_len_aligned +
		TLV_HDR_SIZE + sar_dbs_backoff_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_sar_table_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_SAR_TABLE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->sar_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_TABLE_LEN);
	cmd->dbs_backoff_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN);

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
					 sar_table_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, psar_value, ATH12K_ACPI_BIOS_SAR_TABLE_LEN);

	buf_ptr += sar_table_len_aligned;
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
					 sar_dbs_backoff_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, pdbs_value, ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_SAR_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_INTERFACE_CMDID %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_bios_geo_cmd(struct ath12k_base *ab, const u8 *pgeo_table)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct wmi_pdev_set_bios_geo_table_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret;
	u8 *buf_ptr;
	u32 len, sar_geo_len_aligned;
	const u8 *pgeo_value = pgeo_table + ATH12K_ACPI_GEO_OFFSET_DATA_OFFSET;

	sar_geo_len_aligned = roundup(ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + sar_geo_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_bios_geo_table_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_BIOS_GEO_TABLE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(WMI_PDEV_ID_SOC);
	cmd->geo_len = cpu_to_le32(ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN);

	buf_ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, sar_geo_len_aligned);
	buf_ptr += TLV_HDR_SIZE;
	memcpy(buf_ptr, pgeo_value, ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0],
				  skb,
				  WMI_PDEV_SET_BIOS_GEO_TABLE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send WMI_PDEV_SET_BIOS_GEO_TABLE_CMDID %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_delba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 initiator, u32 reason)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_delba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_delba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DELBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->initiator = cpu_to_le32(initiator);
	cmd->reasoncode = cpu_to_le32(reason);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi delba send vdev_id 0x%X mac_addr %pM tid %u initiator %u reason %u\n",
		   vdev_id, mac, tid, initiator, reason);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_DELBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_DELBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_set_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			      u32 tid, u32 status)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_setresponse_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_setresponse_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SETRESPONSE_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->statuscode = cpu_to_le32(status);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba set resp vdev_id 0x%X mac_addr %pM tid %u status %u\n",
		   vdev_id, mac, tid, status);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SET_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SET_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 buf_size)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->buffersize = cpu_to_le32(buf_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba send vdev_id 0x%X mac_addr %pM tid %u bufsize %u\n",
		   vdev_id, mac, tid, buf_size);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_clear_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_clear_resp_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_clear_resp_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_CLEAR_RESP_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba clear resp vdev_id 0x%X mac_addr %pM\n",
		   vdev_id, mac);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_CLEAR_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_CLEAR_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_init_country_cmd(struct ath12k *ar,
				     struct ath12k_wmi_init_country_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_init_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_country_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_INIT_COUNTRY_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	switch (arg->flags) {
	case ALPHA_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_ALPHA;
		memcpy(&cmd->cc_info.alpha2, arg->cc_info.alpha2, 3);
		break;
	case CC_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_COUNTRY_CODE);
		cmd->cc_info.country_code =
			cpu_to_le32(arg->cc_info.country_code);
		break;
	case REGDMN_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_REGDOMAIN);
		cmd->cc_info.regdom_id = cpu_to_le32(arg->cc_info.regdom_id);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_SET_INIT_COUNTRY_CMDID);

out:
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SET_INIT_COUNTRY CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_set_current_country_cmd(struct ath12k *ar,
					    struct wmi_set_current_country_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_set_current_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_set_current_country_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_CURRENT_COUNTRY_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	memcpy(&cmd->new_alpha2, &arg->alpha2, sizeof(arg->alpha2));
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SET_CURRENT_COUNTRY_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "set current country pdev id %d alpha2 %c%c\n",
		   ar->pdev->pdev_id,
		   arg->alpha2[0],
		   arg->alpha2[1]);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SET_CURRENT_COUNTRY_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_send_thermal_mitigation_cmd(struct ath12k *ar,
				       struct ath12k_wmi_thermal_mitigation_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_therm_throt_config_request_cmd *cmd;
	struct wmi_therm_throt_level_config_info *lvl_conf;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int i, ret, len;

	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		len = sizeof(*cmd) + TLV_HDR_SIZE + (ENHANCED_THERMAL_LEVELS * sizeof(*lvl_conf));
	else
		len = sizeof(*cmd) + TLV_HDR_SIZE + (THERMAL_LEVELS * sizeof(*lvl_conf));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_therm_throt_config_request_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_CONFIG_REQUEST,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->enable = cpu_to_le32(arg->enable);
	cmd->dc = cpu_to_le32(arg->dc);
	cmd->dc_per_event = cpu_to_le32(arg->dc_per_event);
	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		cmd->therm_throt_levels = cpu_to_le32(ENHANCED_THERMAL_LEVELS);
	else
		cmd->therm_throt_levels = cpu_to_le32(THERMAL_LEVELS);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map))
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 ENHANCED_THERMAL_LEVELS * sizeof(*lvl_conf));
	else
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 THERMAL_LEVELS * sizeof(*lvl_conf));

	lvl_conf = (struct wmi_therm_throt_level_config_info *)(skb->data +
								sizeof(*cmd) +
								TLV_HDR_SIZE);

	if (test_bit(WMI_SERVICE_THERM_THROT_5_LEVELS, ar->ab->wmi_ab.svc_map)) {
		for (i = 0; i < ENHANCED_THERMAL_LEVELS; i++) {
			lvl_conf->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_LEVEL_CONFIG_INFO,
						       sizeof(*lvl_conf));

			lvl_conf->temp_lwm = arg->levelconf[i].tmplwm;
			lvl_conf->temp_hwm = arg->levelconf[i].tmphwm;
			lvl_conf->dc_off_percent = arg->levelconf[i].dcoffpercent;
			lvl_conf->prio = arg->levelconf[i].priority;

			if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->pout_reduction_25db =
					arg->levelconf[i].pout_reduction_db;

			if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->tx_chain_mask = arg->levelconf[i].tx_chain_mask;
			lvl_conf->duty_cycle = arg->levelconf[i].duty_cycle;
			lvl_conf++;
		}
	} else {
		for (i = 0; i < THERMAL_LEVELS; i++) {
			lvl_conf->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_THERM_THROT_LEVEL_CONFIG_INFO,
						       sizeof(*lvl_conf));

			lvl_conf->temp_lwm = arg->levelconf[i].tmplwm;
			lvl_conf->temp_hwm = arg->levelconf[i].tmphwm;
			lvl_conf->dc_off_percent = arg->levelconf[i].dcoffpercent;
			lvl_conf->prio = arg->levelconf[i].priority;

			if (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->pout_reduction_25db =
					arg->levelconf[i].pout_reduction_db;

			if (test_bit(WMI_SERVICE_THERM_THROT_TX_CHAIN_MASK,
				     ar->ab->wmi_ab.svc_map))
				lvl_conf->tx_chain_mask = arg->levelconf[i].tx_chain_mask;
			lvl_conf->duty_cycle = arg->levelconf[i].duty_cycle;
			lvl_conf++;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev set thermal throt pdev_id %d enable %d dc %d dc_per_event %x levels %d\n",
		   ar->pdev->pdev_id, arg->enable, arg->dc,
		   arg->dc_per_event,
		   (test_bit(WMI_TLV_SERVICE_THERM_THROT_POUT_REDUCTION, ar->ab->wmi_ab.svc_map) ?
		   ENHANCED_THERMAL_LEVELS : THERMAL_LEVELS));

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_THERM_THROT_SET_CONF_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send THERM_THROT_SET_CONF cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_11d_scan_start_cmd(struct ath12k *ar,
				       struct wmi_11d_scan_start_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_11d_scan_start_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_start_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_11D_SCAN_START_CMD,
				       sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_period_msec = cpu_to_le32(arg->scan_period_msec);
	cmd->start_interval_msec = cpu_to_le32(arg->start_interval_msec);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_START_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send 11d scan start vdev id %d period %d ms internal %d ms\n",
		   arg->vdev_id, arg->scan_period_msec,
		   arg->start_interval_msec);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_START_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_11d_scan_stop_cmd(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_11d_scan_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_11d_scan_stop_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_11D_SCAN_STOP_CMD,
				       sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_11D_SCAN_STOP_CMDID);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send 11d scan stop vdev id %d\n",
		   cmd->vdev_id);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_11D_SCAN_STOP_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_send_twt_enable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_ENABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->sta_cong_timer_ms = cpu_to_le32(ATH12K_TWT_DEF_STA_CONG_TIMER_MS);
	cmd->default_slot_size = cpu_to_le32(ATH12K_TWT_DEF_DEFAULT_SLOT_SIZE);
	cmd->congestion_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_SETUP);
	cmd->congestion_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_TEARDOWN);
	cmd->congestion_thresh_critical =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_CRITICAL);
	cmd->interference_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_TEARDOWN);
	cmd->interference_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_SETUP);
	cmd->min_no_sta_setup = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_SETUP);
	cmd->min_no_sta_teardown = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_TEARDOWN);
	cmd->no_of_bcast_mcast_slots =
		cpu_to_le32(ATH12K_TWT_DEF_NO_OF_BCAST_MCAST_SLOTS);
	cmd->min_no_twt_slots = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_TWT_SLOTS);
	cmd->max_no_sta_twt = cpu_to_le32(ATH12K_TWT_DEF_MAX_NO_STA_TWT);
	cmd->mode_check_interval = cpu_to_le32(ATH12K_TWT_DEF_MODE_CHECK_INTERVAL);
	cmd->add_sta_slot_interval = cpu_to_le32(ATH12K_TWT_DEF_ADD_STA_SLOT_INTERVAL);
	cmd->remove_sta_slot_interval =
		cpu_to_le32(ATH12K_TWT_DEF_REMOVE_STA_SLOT_INTERVAL);
	/* TODO add MBSSID support */
	cmd->mbss_support = 0;

	cmd->flags = cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_LEGACY_BSSID |
				 ATH12K_WMI_TWT_ENABLE_FLAG_SPLIT_CONFIG |
				 ATH12K_WMI_TWT_ENABLE_FLAG_RESPONDER |
				 ATH12K_WMI_TWT_ENABLE_FLAG_ITWT_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_BTWT_RTWT);

	if (cmd->mbss_support)
		cmd->flags |= cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_11AX_BSSID);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_ENABLE_CMDID");
		dev_kfree_skb(skb);
	} else {
		ar->twt_enabled = 1;
	}
	return ret;
}

int
ath12k_wmi_send_twt_disable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_disable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_disable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_DISABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->flags = cpu_to_le32(ATH12K_WMI_TWT_ENABLE_FLAG_SPLIT_CONFIG |
				 ATH12K_WMI_TWT_ENABLE_FLAG_RESPONDER |
				 ATH12K_WMI_TWT_ENABLE_FLAG_ITWT_BTWT |
				 ATH12K_WMI_TWT_ENABLE_FLAG_BTWT_RTWT);
	cmd->reason_code = 0;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_DISABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_DISABLE_CMDID");
		dev_kfree_skb(skb);
	} else {
		ar->twt_enabled = 0;
	}
	return ret;
}

int ath12k_wmi_send_twt_add_dialog_cmd(struct ath12k *ar,
				       struct wmi_twt_add_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_add_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_add_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_ADD_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->wake_intvl_us = params->wake_intvl_us;
	cmd->wake_intvl_mantis = params->wake_intvl_mantis;
	cmd->wake_dura_us = params->wake_dura_us;
	cmd->sp_offset_us = params->sp_offset_us;
	cmd->b_twt_persistence = params->b_twt_persistence;
	cmd->b_twt_recommendation = params->b_twt_recommendation;
	cmd->min_wake_intvl_us = params->min_wake_intvl_us;
	cmd->max_wake_intvl_us = params->max_wake_intvl_us;
	cmd->min_wake_dura_us = params->min_wake_dura_us;
	cmd->max_wake_dura_us = params->max_wake_dura_us;
	cmd->sp_start_tsf_lo = (uint32_t)(params->wake_time_tsf & 0xFFFFFFFF);
	cmd->sp_start_tsf_hi = (uint32_t)(params->wake_time_tsf >> 32);
	cmd->announce_timeout_us = params->announce_timeout_us;
	cmd->link_id_bitmap = params->link_id_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_ul_tid_bitmap = params->r_twt_ul_tid_bitmap;

	cmd->flags = params->twt_cmd;
	if (params->flag_bcast)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_BCAST;
	if (params->flag_trigger)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_TRIGGER;
	if (params->flag_flow_type)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_FLOW_TYPE;
	if (params->flag_protection)
		cmd->flags |= WMI_TWT_ADD_DIALOG_FLAG_PROTECTION;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi add twt dialog vdev %u dialog id %u wake interval %u mantissa %u wake duration %u service period offset %u flags 0x%x\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->wake_intvl_us,
		   cmd->wake_intvl_mantis, cmd->wake_dura_us, cmd->sp_offset_us,
		   cmd->flags);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "B-TWT persistence %u B-TWT recommendation %u\n",
		   cmd->b_twt_persistence, cmd->b_twt_recommendation);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "Wake intveral: Max %u Min %u Wake duration: Max %u Min %u\n",
		   cmd->min_wake_intvl_us, cmd->max_wake_intvl_us,
		   cmd->min_wake_dura_us, cmd->min_wake_dura_us);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "SP start tsf lo %u hi %u announcement us %u\n",
		   cmd->sp_start_tsf_lo, cmd->sp_start_tsf_hi,
		   cmd->announce_timeout_us);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "RTWT link id bitmap %u dl %u ul %u\n",
		   cmd->link_id_bitmap, cmd->r_twt_dl_tid_bitmap,
		   cmd->r_twt_ul_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_ADD_DIALOG_CMDID);

	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to add twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_del_dialog_cmd(struct ath12k *ar,
				       struct wmi_twt_del_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_del_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_del_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_TWT_DEL_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->b_twt_persistence = params->b_twt_persistence;
	cmd->is_bcast_twt = params->is_bcast_twt;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi delete twt dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_DEL_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to delete twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_pause_dialog_cmd(struct ath12k *ar,
					 struct wmi_twt_pause_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_pause_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_pause_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_TWT_PAUSE_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi pause twt dialog vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_PAUSE_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to pause twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_resume_dialog_cmd(struct ath12k *ar,
					  struct wmi_twt_resume_dialog_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_resume_dialog_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_resume_dialog_params_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_TWT_RESUME_DIALOG_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->sp_offset_us = params->sp_offset_us;
	cmd->next_twt_size = params->next_twt_size;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi resume twt dialog vdev %u dialog id %u service period offset %u next twt subfield size %u\n",
		   cmd->vdev_id, cmd->dialog_id, cmd->sp_offset_us,
		   cmd->next_twt_size);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_RESUME_DIALOG_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to resume twt dialog: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_twt_vdev_cfg_cmd(struct ath12k *ar, u32 vdev_id,
				 u32 val)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_vdev_config_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_vdev_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_VDEV_CONFIG_CMD,
						 len);

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->twt_support = cpu_to_le32(val);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "send twt vdev config %d %d %x\n",
		   cmd->pdev_id, cmd->vdev_id, cmd->twt_support);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_VDEV_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_VDEV_CONFIG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_obss_spr_cmd(struct ath12k *ar, u32 vdev_id,
			     struct ieee80211_he_obss_pd *he_obss_pd)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_spatial_reuse_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_spatial_reuse_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_SPATIAL_REUSE_SET_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = cpu_to_le32(he_obss_pd->enable);
	cmd->obss_min = a_cpu_to_sle32(he_obss_pd->min_offset);
	cmd->obss_max = a_cpu_to_sle32(he_obss_pd->max_offset);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_obss_color_cfg_cmd(struct ath12k *ar, u32 vdev_id,
				  u8 bss_color, u32 period,
				  bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_color_collision_cfg_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_color_collision_cfg_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_COLOR_COLLISION_DET_CONFIG,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->evt_type = enable ? cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION) :
		cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION_DISABLE);
	cmd->current_bss_color = cpu_to_le32(bss_color);
	cmd->detection_period_ms = cpu_to_le32(period);
	cmd->scan_period_ms = cpu_to_le32(ATH12K_BSS_COLOR_COLLISION_SCAN_PERIOD_MS);
	cmd->free_slot_expiry_time_ms = 0;
	cmd->flags = 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_obss_color_collision_cfg id %d type %d bss_color %d detect_period %d scan_period %d\n",
		   cmd->vdev_id, cmd->evt_type, cmd->current_bss_color,
		   cmd->detection_period_ms, cmd->scan_period_ms);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_pdev_set_srg_bss_color_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	if (!bitmap)
		return -EINVAL;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_SRG_BSS_COLOR_BITMAP_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;

	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd pdev_id %d bss color bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_BSS_COLOR_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_set_srg_patial_bssid_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	if (!bitmap)
		return -EINVAL;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_PARTIAL_BSSID_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd pdev_id %d partial bssid bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_PARTIAL_BSSID_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_srg_obss_color_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd srg pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_srg_obss_bssid_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd srg pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_non_srg_obss_color_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd non_srg pdev_id %d bss color enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_NON_SRG_OBSS_COLOR_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_pdev_non_srg_obss_bssid_enable_bitmap(struct ath12k *ar, u32 *bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_pdev_obss_pd_bitmap_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_obss_pd_bitmap_cmd *)skb->data;
	cmd->tlv_header =
		FIELD_PREP(WMI_TLV_TAG,
			   WMI_TAG_PDEV_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);
	cmd->pdev_id = ar->pdev->pdev_id;
	memcpy(cmd->bitmap, bitmap, sizeof(cmd->bitmap));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "obss pd non_srg pdev_id %d bssid enable bitmap %08x %08x\n",
		   cmd->pdev_id, cmd->bitmap[0], cmd->bitmap[1]);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_NON_SRG_OBSS_BSSID_ENABLE_BITMAP_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_bss_color_change_enable_cmd(struct ath12k *ar, u32 vdev_id,
						bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_bss_color_change_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bss_color_change_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BSS_COLOR_CHANGE_ENABLE,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = enable ? cpu_to_le32(1) : 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_bss_color_change_enable id %d enable %d\n",
		   cmd->vdev_id, cmd->enable);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_BSS_COLOR_CHANGE_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_BSS_COLOR_CHANGE_ENABLE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery_tmpl(struct ath12k *ar, u32 vdev_id,
				   struct sk_buff *tmpl)
{
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;
	size_t aligned_len;
	struct wmi_fils_discovery_tmpl_cmd *cmd;

	if (!tmpl)
		return -EINVAL;

	aligned_len = roundup(tmpl->len, 4);
	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set FILS discovery template\n", vdev_id);

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FILS_DISCOVERY_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_FILS_DISCOVERY_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

static void *
ath12k_wmi_append_prb_resp_cu_params(struct ath12k *ar, u32 vdev_id, void *ptr)
{
	struct wmi_prb_resp_tmpl_ml_info_params *ml_info;
	struct ath12k_prb_resp_tmpl_ml_info *ar_ml_info;
	void *start = ptr;
	struct wmi_tlv *tlv;
	struct ath12k_link_vif *arvif = ath12k_mac_get_arvif(ar, vdev_id);
	size_t ml_info_len = sizeof(*ml_info);

	if (!arvif)
		return ptr;

	/* Add ML info */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, ml_info_len);
	ml_info = (struct wmi_prb_resp_tmpl_ml_info_params *)tlv->value;

	ar_ml_info = &arvif->ml_info;
	ml_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PRB_RESP_TMPL_ML_INFO_CMD,
						     sizeof(*ml_info));
	ml_info->hw_link_id = cpu_to_le32(ar_ml_info->hw_link_id);
	ml_info->cu_vdev_map_cat1_lo = cpu_to_le32(ar_ml_info->cu_vdev_map_cat1_lo);
	ml_info->cu_vdev_map_cat1_hi = cpu_to_le32(ar_ml_info->cu_vdev_map_cat1_hi);
	ml_info->cu_vdev_map_cat2_lo = cpu_to_le32(ar_ml_info->cu_vdev_map_cat2_lo);
	ml_info->cu_vdev_map_cat2_hi = cpu_to_le32(ar_ml_info->cu_vdev_map_cat2_hi);

	ptr += TLV_HDR_SIZE + sizeof(*ml_info);
	/* Reset CU bitmap and bpcc values*/
	memset(&arvif->ml_info, 0, sizeof(arvif->ml_info));
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi %ld bytes of additional data filled for prb resp CU\n",
		   (unsigned long)(ptr - start));
	return ptr;
}

int ath12k_wmi_peer_set_cfr_capture_conf(struct ath12k *ar,
					 u32 vdev_id, const u8 *mac_addr,
					 struct wmi_peer_cfr_capture_conf_arg *arg)
{
	struct wmi_peer_cfr_capture_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int ret;

	if (!arg)
		return -EINVAL;

	if (!mac_addr)
		return -EINVAL;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_cfr_capture_cmd_fixed_param *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PEER_CFR_CAPTURE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);

	ether_addr_copy(cmd->mac_addr.addr, mac_addr);
	cmd->request = arg->request;
	cmd->vdev_id = vdev_id;
	cmd->periodicity = arg->periodicity;
	cmd->bandwidth = arg->bandwidth;
	cmd->capture_method = arg->capture_method;

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_PEER_CFR_CAPTURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %d failed to send peer cfr capture cmd\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set cfr capture conf cmd->request=%d cmd->vdev_id=%d cmd->periodicity=%d cmd->bandwidth=%d cmd->capture_method=%d",
		   vdev_id, cmd->request, cmd->vdev_id, cmd->periodicity,
	           cmd->bandwidth, cmd->capture_method);
	return ret;
}

static void
ath12k_wmi_populate_cfr_rcc_param(struct ath12k *ar,
				  struct cfr_rcc_param *rcc,
				  struct wmi_cfr_filter_group_config *param,
				  u8 grp_id)
{
	struct ta_ra_cfr_cfg *tgt_cfg = &rcc->curr[grp_id];

	param->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CFR_FILTER_GROUP_CONFIG,
						   sizeof(*param));

	param->filter_group_id = cpu_to_le32(grp_id);
	if (tgt_cfg->valid_ta)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_TA);
	if (tgt_cfg->valid_ta_mask)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_TA_MASK);
	if (tgt_cfg->valid_ra)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_RA);
	if (tgt_cfg->valid_ra_mask)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_RA_MASK);
	if (tgt_cfg->valid_bw_mask)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_BW);
	if (tgt_cfg->valid_nss_mask)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_NSS);
	if (tgt_cfg->valid_mgmt_subtype)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_MGMT_SUBTYPE);
	if (tgt_cfg->valid_ctrl_subtype)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_CTRL_SUBTYPE);
	if (tgt_cfg->valid_data_subtype)
		param->filter_set_valid_mask |=
			cpu_to_le32(WMI_CFR_FILTER_GRP_CFG_VALID_DATA_SUBTYPE);

	ether_addr_copy(param->ta_addr.addr, tgt_cfg->ta_addr);
	ether_addr_copy(param->ta_addr_mask.addr, tgt_cfg->ta_addr_mask);
	ether_addr_copy(param->ra_addr.addr, tgt_cfg->ra_addr);
	ether_addr_copy(param->ra_addr_mask.addr, tgt_cfg->ra_addr_mask);

	param->bw_nss_filter =
		cpu_to_le32(u32_encode_bits(tgt_cfg->bw, WMI_CFR_FILTER_GRP_CFG_BW) |
			    u32_encode_bits(tgt_cfg->nss, WMI_CFR_FILTER_GRP_CFG_NSS));

	param->mgmt_subtype_filter = cpu_to_le32(tgt_cfg->mgmt_subtype_filter);
	param->ctrl_subtype_filter = cpu_to_le32(tgt_cfg->ctrl_subtype_filter);
	param->data_subtype_filter = cpu_to_le32(tgt_cfg->data_subtype_filter);
	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_CFR, ATH12K_DBG_L1,
			 "RCC grp %d v_msk %x ta %pM ta_msk %pM ra %pM ra_msk %pM bwnss %x mgmt %x ctrl %x data %x",
			 le32_to_cpu(param->filter_group_id),
			 le32_to_cpu(param->filter_set_valid_mask),
			 param->ta_addr.addr,
			 param->ta_addr_mask.addr,
			 param->ra_addr.addr,
			 param->ra_addr_mask.addr,
			 le32_to_cpu(param->bw_nss_filter),
			 le32_to_cpu(param->mgmt_subtype_filter),
			 le32_to_cpu(param->ctrl_subtype_filter),
			 le32_to_cpu(param->data_subtype_filter));
}

int ath12k_wmi_send_cfr_rcc_cmd(struct ath12k *ar, struct cfr_rcc_param *rcc)
{
	struct wmi_cfr_capture_filter_cmd_fixed_param *cmd;
	struct wmi_cfr_filter_group_config *param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	u32 len, i;
	u8 num_grp_tlvs;
	int ret;

	if (!rcc)
		return -EINVAL;

	num_grp_tlvs = hweight_long(rcc->modified_in_curr_session);

	len = sizeof(*cmd) + TLV_HDR_SIZE;
	len += num_grp_tlvs * sizeof(*param);

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_cfr_capture_filter_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CFR_CAPTURE_FILTER_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(rcc->pdev_id);

	/* set filter types that are enabled */
	if (rcc->m_directed_ftm)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_DIRECTED_FTM_ACK_EN);
	if (rcc->m_all_ftm_ack)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_ALL_FTM_ACK_EN);
	if (rcc->m_ndpa_ndp_directed)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_NDPA_NDP_DIRECTED_EN);
	if (rcc->m_ndpa_ndp_all)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_NDPA_NDP_ALL_EN);
	if (rcc->m_ta_ra_filter)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_TA_RA_TYPE_FILTER_EN);
	if (rcc->m_all_packet)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_ALL_PACKET_EN);
	if (rcc->en_ta_ra_filter_in_as_fp)
		cmd->filter_type |= cpu_to_le32(WMI_CFR_FILTER_IN_AS_FP_TA_RA_TYPE);

	cmd->capture_interval = le32_encode_bits(rcc->capture_interval,
						 WMI_CFR_CAPTURE_INTERVAL);
	cmd->capture_duration = le32_encode_bits(rcc->capture_duration,
						 WMI_CFR_CAPTURE_DURATION);
	cmd->filter_group_bitmap = le32_encode_bits(rcc->filter_group_bitmap,
						    WMI_CFR_FILTER_GROUP_BITMAP);
	cmd->ul_mu_user_mask_lower = cpu_to_le32(rcc->ul_mu_user_mask_lower);
	cmd->ul_mu_user_mask_upper = le32_encode_bits(rcc->ul_mu_user_mask_upper,
						      WMI_CFR_UL_MU_USER_UPPER);
	cmd->freeze_tlv_delay_cnt =
		cpu_to_le32(u32_encode_bits(!!rcc->freeze_tlv_delay_cnt_en,
					    WMI_CFR_FREEZE_DELAY_CNT_EN) |
			    u32_encode_bits(rcc->freeze_tlv_delay_cnt_thr,
					    WMI_CFR_FREEZE_DELAY_CNT_THR));
	cmd->capture_count =
		cpu_to_le32(u32_encode_bits(rcc->capture_count, WMI_CFR_CAPTURE_COUNT) |
			    u32_encode_bits(!!rcc->capture_intval_mode_sel,
					    WMI_CFR_CAPTURE_INTERVAL_MODE_SEL));

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 num_grp_tlvs * sizeof(*param));

	if (num_grp_tlvs) {
		ptr += TLV_HDR_SIZE;
		param = (struct wmi_cfr_filter_group_config *)ptr;

		for (i = 0; i < MAX_TA_RA_ENTRIES; i++) {
			if (!test_bit(i, &rcc->modified_in_curr_session))
				continue;
			ath12k_wmi_populate_cfr_rcc_param(ar, rcc, param, i);
			param++;
		}
	}

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_CFR_CAPTURE_FILTER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI RCC: failed to send capture filter cmd (%d)\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI | ATH12K_DBG_CFR, ATH12K_DBG_L1,
			 "RCC: pdev %d ftype %x intv %u dur %u gbit %x ul_l %x ul_u %x frz %x cnt %x",
			 le32_to_cpu(cmd->pdev_id),
			 le32_to_cpu(cmd->filter_type),
			 le32_to_cpu(cmd->capture_interval),
			 le32_to_cpu(cmd->capture_duration),
			 le32_to_cpu(cmd->filter_group_bitmap),
			 le32_to_cpu(cmd->ul_mu_user_mask_lower),
			 le32_to_cpu(cmd->ul_mu_user_mask_upper),
			 le32_to_cpu(cmd->freeze_tlv_delay_cnt),
			 le32_to_cpu(cmd->capture_count));
	return ret;
}

int ath12k_wmi_probe_resp_tmpl(struct ath12k *ar,
			       struct ath12k_link_vif *arvif,
			       struct sk_buff *tmpl)
{
	struct wmi_probe_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *probe_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len, mlinfo_tlv_len = 0;
	size_t aligned_len = roundup(tmpl->len, 4);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set probe response template\n", arvif->vdev_id);

	if (ath12k_mac_is_ml_arvif(arvif))
		mlinfo_tlv_len = TLV_HDR_SIZE + sizeof(struct wmi_prb_resp_tmpl_ml_info_params);

	len = sizeof(*cmd) + sizeof(*probe_info) + TLV_HDR_SIZE + aligned_len + mlinfo_tlv_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb) {
		memset(&arvif->ml_info, 0, sizeof(arvif->ml_info));
		return -ENOMEM;
	}
	cmd = (struct wmi_probe_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PRB_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);

	ptr = skb->data + sizeof(*cmd);

	probe_info = ptr;
	len = sizeof(*probe_info);
	probe_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							len);
	probe_info->caps = 0;
	probe_info->erp = 0;

	ptr += sizeof(*probe_info);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);
	ptr += (TLV_HDR_SIZE + aligned_len);

	if (ath12k_mac_is_ml_arvif(arvif))
		ptr = ath12k_wmi_append_prb_resp_cu_params(ar, arvif->vdev_id, ptr);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_PRB_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send probe response template command\n",
			    arvif->vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery(struct ath12k *ar, u32 vdev_id, u32 interval,
			      bool unsol_bcast_probe_resp_enabled)
{
	struct sk_buff *skb;
	int ret, len;
	struct wmi_fils_discovery_cmd *cmd;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set %s interval to %u TU\n",
		   vdev_id, unsol_bcast_probe_resp_enabled ?
		   "unsolicited broadcast probe response" : "FILS discovery",
		   interval);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ENABLE_FILS_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->interval = cpu_to_le32(interval);
	cmd->config = cpu_to_le32(unsol_bcast_probe_resp_enabled);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENABLE_FILS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery enable/disable command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

static void
ath12k_fill_band_to_mac_param(struct ath12k_base  *soc,
			      struct ath12k_wmi_pdev_band_arg *arg)
{
	u8 i;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *hal_reg_cap;
	struct ath12k_pdev *pdev;

	for (i = 0; i < soc->num_radios; i++) {
		pdev = &soc->pdevs[i];
		hal_reg_cap = &soc->hal_reg_cap[i];
		arg[i].pdev_id = pdev->pdev_id;

		switch (pdev->cap.supported_bands) {
		case WMI_HOST_WLAN_2GHZ_5GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			pdev->phy_name = ATH12K_PHY_2GHZ_5GHZ;
			break;
		case WMI_HOST_WLAN_2GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_2ghz_chan;
			pdev->phy_name = ATH12K_PHY_2GHZ;
			break;
		case WMI_HOST_WLAN_5GHZ_CAP:
			arg[i].start_freq = hal_reg_cap->low_5ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			if (hal_reg_cap->high_5ghz_chan <= ATH12K_MIN_5G_HIGH_BAND_FREQ)
				pdev->phy_name = ATH12K_PHY_5GHZ_LOW;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MAX_5G_LOW_BAND_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MIN_6GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_5GHZ_HIGH;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MIN_5GHZ_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MAX_5GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_5GHZ;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
				 hal_reg_cap->high_5ghz_chan <=
				 ATH12K_MAX_6G_LOW_BAND_FREQ)
				pdev->phy_name = ATH12K_PHY_6GHZ_LOW;
			else if (hal_reg_cap->low_5ghz_chan >=
				 ATH12K_MAX_6G_LOW_BAND_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_6GHZ_HIGH;
			else if (hal_reg_cap->low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
				 hal_reg_cap->high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ)
				pdev->phy_name = ATH12K_PHY_6GHZ;
			else
				pdev->phy_name = ATH12K_PHY_5GHZ;
			break;
		default:
			break;
		}

#ifdef CPTCFG_QCN_EXTN
		if (ath12k_scan_radio_supported(pdev))
			pdev->phy_name = ATH12K_PHY_SCAN_RADIO;
#endif /* CPTCFG_QCN_EXTN */
	}
}

static void
ath12k_wmi_copy_resource_config(struct ath12k_base *ab,
				struct ath12k_wmi_resource_config_params *wmi_cfg,
				struct ath12k_wmi_resource_config_arg *tg_cfg)
{
	wmi_cfg->num_vdevs = cpu_to_le32(tg_cfg->num_vdevs);
	wmi_cfg->num_peers = cpu_to_le32(tg_cfg->num_peers);
	wmi_cfg->num_offload_peers = cpu_to_le32(tg_cfg->num_offload_peers);
	wmi_cfg->num_offload_reorder_buffs =
		cpu_to_le32(tg_cfg->num_offload_reorder_buffs);
	wmi_cfg->num_peer_keys = cpu_to_le32(tg_cfg->num_peer_keys);
	wmi_cfg->num_tids = cpu_to_le32(tg_cfg->num_tids);
	wmi_cfg->ast_skid_limit = cpu_to_le32(tg_cfg->ast_skid_limit);
	wmi_cfg->tx_chain_mask = cpu_to_le32(tg_cfg->tx_chain_mask);
	wmi_cfg->rx_chain_mask = cpu_to_le32(tg_cfg->rx_chain_mask);
	wmi_cfg->rx_timeout_pri[0] = cpu_to_le32(tg_cfg->rx_timeout_pri[0]);
	wmi_cfg->rx_timeout_pri[1] = cpu_to_le32(tg_cfg->rx_timeout_pri[1]);
	wmi_cfg->rx_timeout_pri[2] = cpu_to_le32(tg_cfg->rx_timeout_pri[2]);
	wmi_cfg->rx_timeout_pri[3] = cpu_to_le32(tg_cfg->rx_timeout_pri[3]);
	wmi_cfg->rx_decap_mode = cpu_to_le32(tg_cfg->rx_decap_mode);
	wmi_cfg->scan_max_pending_req = cpu_to_le32(tg_cfg->scan_max_pending_req);
	wmi_cfg->bmiss_offload_max_vdev = cpu_to_le32(tg_cfg->bmiss_offload_max_vdev);
	wmi_cfg->roam_offload_max_vdev = cpu_to_le32(tg_cfg->roam_offload_max_vdev);
	wmi_cfg->roam_offload_max_ap_profiles =
		cpu_to_le32(tg_cfg->roam_offload_max_ap_profiles);
	wmi_cfg->num_mcast_groups = cpu_to_le32(tg_cfg->num_mcast_groups);
	wmi_cfg->num_mcast_table_elems = cpu_to_le32(tg_cfg->num_mcast_table_elems);
	wmi_cfg->mcast2ucast_mode = cpu_to_le32(tg_cfg->mcast2ucast_mode);
	wmi_cfg->tx_dbg_log_size = cpu_to_le32(tg_cfg->tx_dbg_log_size);
	wmi_cfg->num_wds_entries = cpu_to_le32(tg_cfg->num_wds_entries);
	wmi_cfg->dma_burst_size = cpu_to_le32(tg_cfg->dma_burst_size);
	wmi_cfg->mac_aggr_delim = cpu_to_le32(tg_cfg->mac_aggr_delim);
	wmi_cfg->rx_skip_defrag_timeout_dup_detection_check =
		cpu_to_le32(tg_cfg->rx_skip_defrag_timeout_dup_detection_check);
	wmi_cfg->vow_config = cpu_to_le32(tg_cfg->vow_config);
	wmi_cfg->gtk_offload_max_vdev = cpu_to_le32(tg_cfg->gtk_offload_max_vdev);
	wmi_cfg->num_msdu_desc = cpu_to_le32(tg_cfg->num_msdu_desc);
	wmi_cfg->max_frag_entries = cpu_to_le32(tg_cfg->max_frag_entries);
	wmi_cfg->num_tdls_vdevs = cpu_to_le32(tg_cfg->num_tdls_vdevs);
	wmi_cfg->num_tdls_conn_table_entries =
		cpu_to_le32(tg_cfg->num_tdls_conn_table_entries);
	wmi_cfg->beacon_tx_offload_max_vdev =
		cpu_to_le32(tg_cfg->beacon_tx_offload_max_vdev);
	wmi_cfg->num_multicast_filter_entries =
		cpu_to_le32(tg_cfg->num_multicast_filter_entries);
	wmi_cfg->num_wow_filters = cpu_to_le32(tg_cfg->num_wow_filters);
	wmi_cfg->num_keep_alive_pattern = cpu_to_le32(tg_cfg->num_keep_alive_pattern);
	wmi_cfg->keep_alive_pattern_size = cpu_to_le32(tg_cfg->keep_alive_pattern_size);
	wmi_cfg->max_tdls_concurrent_sleep_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_sleep_sta);
	wmi_cfg->max_tdls_concurrent_buffer_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_buffer_sta);
	wmi_cfg->wmi_send_separate = cpu_to_le32(tg_cfg->wmi_send_separate);
	wmi_cfg->num_ocb_vdevs = cpu_to_le32(tg_cfg->num_ocb_vdevs);
	wmi_cfg->num_ocb_channels = cpu_to_le32(tg_cfg->num_ocb_channels);
	wmi_cfg->num_ocb_schedules = cpu_to_le32(tg_cfg->num_ocb_schedules);
	wmi_cfg->bpf_instruction_size = cpu_to_le32(tg_cfg->bpf_instruction_size);
	wmi_cfg->max_bssid_rx_filters = cpu_to_le32(tg_cfg->max_bssid_rx_filters);
	wmi_cfg->use_pdev_id = cpu_to_le32(tg_cfg->use_pdev_id);
	wmi_cfg->flag1 = cpu_to_le32(tg_cfg->atf_config |
				     WMI_RSRC_CFG_FLAG1_BSS_CHANNEL_INFO_64);
	if (tg_cfg->carrier_config)
		wmi_cfg->carrier_config = cpu_to_le32(tg_cfg->carrier_config);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "ATF: carrier_config %d",
		   tg_cfg->carrier_config);

	if (tg_cfg->rf_path)
		wmi_cfg->rf_path = cpu_to_le32(tg_cfg->rf_path);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "rf_path_config %d",
		   tg_cfg->rf_path);

	if (tg_cfg->carrier_vow_optimization)
		wmi_cfg->flag1 |= WMI_RSRC_CFG_FLAG1_VIDEO_OVER_WIFI_ENABLE;
	wmi_cfg->peer_map_unmap_version = cpu_to_le32(tg_cfg->peer_map_unmap_version);
	wmi_cfg->sched_params = cpu_to_le32(tg_cfg->sched_params);
	wmi_cfg->twt_ap_pdev_count = cpu_to_le32(tg_cfg->twt_ap_pdev_count);
	wmi_cfg->twt_ap_sta_count = cpu_to_le32(tg_cfg->twt_ap_sta_count);
	wmi_cfg->flags2 = le32_encode_bits(tg_cfg->peer_metadata_ver,
					   WMI_RSRC_CFG_FLAGS2_RX_PEER_METADATA_VERSION);

	wmi_cfg->host_service_flags = cpu_to_le32(tg_cfg->afc_support <<
						   WMI_RSRC_CFG_HOST_SUPPORT_LP_SP_MODE_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_disable_timer_check <<
						   WMI_RSRC_CFG_HOST_AFC_DIS_TIMER_CHECK_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_disable_req_id_check <<
						   WMI_RSRC_CFG_HOST_AFC_DIS_REQ_ID_CHECK_BIT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_indoor_support <<
						   WMI_RSRC_CFG_HOST_AFC_INDOOR_SUPPORT);
	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->afc_outdoor_support <<
						   WMI_RSRC_CFG_HOST_AFC_OUTDOOR_SUPPORT);

	ath12k_dbg(NULL, ATH12K_DBG_WMI, "afc_support: %u "
		   "dis_timer_check: %u, dis_reqid_check: %u, indoor: %u, outdoor: %u\n",
		   tg_cfg->afc_support, tg_cfg->afc_disable_timer_check,
		   tg_cfg->afc_disable_req_id_check, tg_cfg->afc_indoor_support,
		   tg_cfg->afc_outdoor_support);

	wmi_cfg->host_service_flags |= cpu_to_le32(tg_cfg->is_reg_cc_ext_event_supported <<
				WMI_RSRC_CFG_HOST_SVC_FLAG_REG_CC_EXT_SUPPORT_BIT);
	if (ab->hw_params->reoq_lut_support)
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_REO_QREF_SUPPORT_BIT);
	wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_FULL_BW_NOL_SUPPORT_BIT);
	if (!ath12k_afc_reg_no_action) {
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_AFC_TRIGGER_ON_DEFAULT_CC_EVENT_BIT);
	}
	if (tg_cfg->def_flow_override)
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_DEF_FLOW_OVERRIDE_SET_BIT);
	if (tg_cfg->is_simulate_radar_320_supported)
		wmi_cfg->host_service_flags |=
			cpu_to_le32(1 <<
				    WMI_RSRC_CFG_HOST_SIMULATE_RADAR_320_SUPPORTED);

	/* Support CUMAC selection and initialization, only on when HW group is
	 * MLO capable and in case of Non-FTM mode bring up.
	 */
	if (ab->ag->cumac_enabled && ab->ag->mlo_capable && !ath12k_ftm_mode) {
		if (test_bit(WMI_SERVICE_PDEV_SET_CUMAC_CHIP_CMD_SUPPORT,
			     ab->wmi_ab.svc_map))
			wmi_cfg->host_service_flags |=
				cpu_to_le32(1 << WMI_RSRC_CFG_HOST_SVC_FLAG_CUMAC_CMD_SUPPORT_BIT);
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_set_hw_blocklist_host_service_flag_extn(ab, wmi_cfg, tg_cfg);
#endif

	wmi_cfg->host_service_flags |=
		cpu_to_le32(tg_cfg->host_reo_mgmt_support <<
			    WMI_RSRC_CFG_HOST_SVC_REO_MGMT_SUPPORT);

	wmi_cfg->ema_max_vap_cnt = cpu_to_le32(tg_cfg->ema_max_vap_cnt);
	wmi_cfg->ema_max_profile_period = cpu_to_le32(tg_cfg->ema_max_profile_period);
	wmi_cfg->flags2 |= cpu_to_le32(WMI_RSRC_CFG_FLAGS2_CALC_NEXT_DTIM_COUNT_SET);
	if (ath12k_frame_mode != ATH12K_HW_TXRX_ETHERNET)
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_INTRABSS_MEC_WDS_LEARNING_DISABLE;
	else
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_FW_AST_INDICATION_DISABLE;

	if (tg_cfg->is_wds_null_frame_supported)
		wmi_cfg->flags2 |= WMI_RSRC_CFG_FLAGS2_WDS_NULL_FRAME_SUPPORT;

	if (tg_cfg->rep_ul_resp)
		wmi_cfg->flags2 |= cpu_to_le32(tg_cfg->rep_ul_resp);

	wmi_cfg->ema_init_config =
		cpu_to_le32(u32_encode_bits(tg_cfg->max_beacon_size,
					    WMI_RSRC_CFG_EMA_INIT_CONFIG_BEACON_SIZE));
	wmi_cfg->flags2 |= (tg_cfg->qos) ?
			   (WMI_RSRC_CFG_FLAGS2_SAWF_CONFIG_ENABLE_SET) : (0);
	wmi_cfg->max_num_group_keys = cpu_to_le32(tg_cfg->max_num_group_keys);
#ifdef CPTCFG_QCN_EXTN
	wmi_cfg->smart_ant_cap = cpu_to_le32(tg_cfg->smart_ant_cap);
#endif

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ath12k_wmi_copy_resource_config_ipa(ab, wmi_cfg, tg_cfg);
#endif

}

/**
 * ath12k_set_afc_config() - Set AFC config parameters
 * @config: WMI resource config
 * @ab: ath12k base
 *
 * Return: None
 */
static void ath12k_set_afc_config(struct ath12k_wmi_resource_config_arg *config,
				  struct ath12k_base *ab)
{
	config->afc_indoor_support = false;
	config->afc_outdoor_support = false;

	if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_INDOOR)
		config->afc_indoor_support = true;
	else if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_OUTDOOR)
		config->afc_outdoor_support = true;
	else if (ab->afc_dev_deployment == ATH12K_AFC_DEPLOYMENT_UNKNOWN)
		config->afc_indoor_support = true;

	config->afc_support = ath12k_6ghz_sp_pwrmode_supp_enabled;
	config->afc_disable_timer_check = ath12k_afc_disable_timer_check;
	config->afc_disable_req_id_check = ath12k_afc_disable_req_id_check;
}

static int ath12k_init_cmd_send(struct ath12k_wmi_pdev *wmi,
				struct ath12k_wmi_init_cmd_arg *arg)
{
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct sk_buff *skb;
	struct wmi_init_cmd *cmd;
	struct ath12k_wmi_resource_config_params *cfg;
	struct ath12k_wmi_pdev_set_hw_mode_cmd *hw_mode;
	struct ath12k_wmi_pdev_band_to_mac_params *band_to_mac;
	struct ath12k_wmi_host_mem_chunk_params *host_mem_chunks;
	struct wmi_tlv *tlv;
	struct device *dev = ab->dev;
	bool three_way_coex_enabled = false;
	size_t ret, len;
	void *ptr;
	u32 hw_mode_len = 0;
	u16 idx;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX)
		hw_mode_len = sizeof(*hw_mode) + TLV_HDR_SIZE +
			      (arg->num_band_to_mac * sizeof(*band_to_mac));

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*cfg) + hw_mode_len +
	      (arg->num_mem_chunks ? (sizeof(*host_mem_chunks) * WMI_MAX_MEM_REQS) : 0);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_INIT_CMD,
						 sizeof(*cmd));

	ptr = skb->data + sizeof(*cmd);
	cfg = ptr;

	three_way_coex_enabled = of_property_read_bool(dev->of_node, "qcom,btcoex");
	if (three_way_coex_enabled)
		cfg->flag1 |= WMI_RSRC_CFG_FLAG1_THREE_WAY_COEX_CONFIG_OVERRIDE_SUPPORT;

	ath12k_wmi_copy_resource_config(ab, cfg, &arg->res_cfg);

	cfg->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RESOURCE_CONFIG,
						 sizeof(*cfg));

	ptr += sizeof(*cfg);
	host_mem_chunks = ptr + TLV_HDR_SIZE;
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params);

	for (idx = 0; idx < arg->num_mem_chunks; ++idx) {
		host_mem_chunks[idx].tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_WLAN_HOST_MEMORY_CHUNK,
					   len);

		host_mem_chunks[idx].ptr = cpu_to_le32(arg->mem_chunks[idx].paddr);
		host_mem_chunks[idx].size = cpu_to_le32(arg->mem_chunks[idx].len);
		host_mem_chunks[idx].req_id = cpu_to_le32(arg->mem_chunks[idx].req_id);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "WMI host mem chunk req_id %d paddr 0x%llx len %d\n",
			   arg->mem_chunks[idx].req_id,
			   (u64)arg->mem_chunks[idx].paddr,
			   arg->mem_chunks[idx].len);
	}
	cmd->num_host_mem_chunks = cpu_to_le32(arg->num_mem_chunks);
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params) * arg->num_mem_chunks;

	/* num_mem_chunks is zero */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE + len;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX) {
		hw_mode = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)ptr;
		hw_mode->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
							     sizeof(*hw_mode));

		hw_mode->hw_mode_index = cpu_to_le32(arg->hw_mode_id);
		hw_mode->num_band_to_mac = cpu_to_le32(arg->num_band_to_mac);

		ptr += sizeof(*hw_mode);

		len = arg->num_band_to_mac * sizeof(*band_to_mac);
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

		ptr += TLV_HDR_SIZE;
		len = sizeof(*band_to_mac);

		for (idx = 0; idx < arg->num_band_to_mac; idx++) {
			band_to_mac = (void *)ptr;

			band_to_mac->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BAND_TO_MAC,
						       len);
			band_to_mac->pdev_id = cpu_to_le32(arg->band_to_mac[idx].pdev_id);
			band_to_mac->start_freq =
				cpu_to_le32(arg->band_to_mac[idx].start_freq);
			band_to_mac->end_freq =
				cpu_to_le32(arg->band_to_mac[idx].end_freq);
			ptr += sizeof(*band_to_mac);
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_INIT_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_INIT_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_lro_cfg(struct ath12k *ar,
			    int pdev_id)
{
	struct ath12k_wmi_pdev_lro_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_lro_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_LRO_INFO_CMD,
						 sizeof(*cmd));

	get_random_bytes(cmd->th_4, sizeof(cmd->th_4));
	get_random_bytes(cmd->th_6, sizeof(cmd->th_6));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI lro cfg cmd pdev_id 0x%x\n", pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_LRO_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send lro cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_wait_for_service_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.service_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_wait_for_unified_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.unified_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_set_hw_mode(struct ath12k_base *ab,
			   enum wmi_host_hw_mode_config_type mode)
{
	struct ath12k_wmi_pdev_set_hw_mode_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	int len;
	int ret;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
						 sizeof(*cmd));

	cmd->pdev_id = WMI_PDEV_ID_SOC;
	cmd->hw_mode_index = cpu_to_le32(mode);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0], skb, WMI_PDEV_SET_HW_MODE_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_PDEV_SET_HW_MODE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_cmd_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	struct ath12k_wmi_init_cmd_arg arg = {};

	if (test_bit(WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT,
		     ab->wmi_ab.svc_map))
		arg.res_cfg.is_reg_cc_ext_event_supported = true;
	if (test_bit(WMI_TLV_SERVICE_RADAR_FLAGS_SUPPORT, ab->wmi_ab.svc_map))
		arg.res_cfg.is_full_bw_nol_feature_supported = true;

	if (test_bit(WMI_SERVICE_WDS_NULL_FRAME_SUPPORT, ab->wmi_ab.svc_map))
		arg.res_cfg.is_wds_null_frame_supported = true;

	if (test_bit(WMI_TLV_SERVICE_AFC_SUPPORT, ab->wmi_ab.svc_map))
		ath12k_set_afc_config(&arg.res_cfg, ab);

#ifdef CPTCFG_QCN_EXTN
	ath12k_wmi_set_hw_blocklist_service_support_extn(ab, &arg.res_cfg);
#endif
	if (test_bit(WMI_TLV_SERVICE_BANG_RADAR_320_SUPPORT,
		     ab->wmi_ab.svc_map))
		arg.res_cfg.is_simulate_radar_320_supported = true;

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_smart_ant_enable(ab))
		arg.res_cfg.smart_ant_cap = 1;
#endif

	ab->hw_params->wmi_init(ab, &arg.res_cfg);
	ab->wow.wmi_conf_rx_decap_mode = arg.res_cfg.rx_decap_mode;

	ath12k_info(ab, "WMI_INIT: num_vdevs=%u (num_radios=%u)\n",
		    arg.res_cfg.num_vdevs, ab->num_radios);

	arg.num_mem_chunks = wmi_ab->num_mem_chunks;
	arg.hw_mode_id = wmi_ab->preferred_hw_mode;
	arg.mem_chunks = wmi_ab->mem_chunks;

	if (ab->hw_params->single_pdev_only)
		arg.hw_mode_id = WMI_HOST_HW_MODE_MAX;

	arg.num_band_to_mac = ab->num_radios;
	ath12k_fill_band_to_mac_param(ab, arg.band_to_mac);
#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_parse_pdev_section(ab);
#endif

	dp->peer_metadata_ver = arg.res_cfg.peer_metadata_ver;

	/* RF path at boot: 0=full 5G, 1=high 5G (from INI rf_switch_config) */
	arg.res_cfg.rf_path = ab->rf_switch_config;
	return ath12k_init_cmd_send(&wmi_ab->wmi[0], &arg);
}

int ath12k_wmi_vdev_spectral_conf(struct ath12k *ar,
				  struct ath12k_wmi_vdev_spectral_conf_arg *arg)
{
	struct ath12k_wmi_vdev_spectral_conf_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_conf_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_CONFIGURE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_count = cpu_to_le32(arg->scan_count);
	cmd->scan_period = cpu_to_le32(arg->scan_period);
	cmd->scan_priority = cpu_to_le32(arg->scan_priority);
	cmd->scan_fft_size = cpu_to_le32(arg->scan_fft_size);
	cmd->scan_gc_ena = cpu_to_le32(arg->scan_gc_ena);
	cmd->scan_restart_ena = cpu_to_le32(arg->scan_restart_ena);
	cmd->scan_noise_floor_ref = cpu_to_le32(arg->scan_noise_floor_ref);
	cmd->scan_init_delay = cpu_to_le32(arg->scan_init_delay);
	cmd->scan_nb_tone_thr = cpu_to_le32(arg->scan_nb_tone_thr);
	cmd->scan_str_bin_thr = cpu_to_le32(arg->scan_str_bin_thr);
	cmd->scan_wb_rpt_mode = cpu_to_le32(arg->scan_wb_rpt_mode);
	cmd->scan_rssi_rpt_mode = cpu_to_le32(arg->scan_rssi_rpt_mode);
	cmd->scan_rssi_thr = cpu_to_le32(arg->scan_rssi_thr);
	cmd->scan_pwr_format = cpu_to_le32(arg->scan_pwr_format);
	cmd->scan_rpt_mode = cpu_to_le32(arg->scan_rpt_mode);
	cmd->scan_bin_scale = cpu_to_le32(arg->scan_bin_scale);
	cmd->scan_dbm_adj = cpu_to_le32(arg->scan_dbm_adj);
	cmd->scan_chn_mask = cpu_to_le32(arg->scan_chn_mask);
	cmd->scan_mode = cpu_to_le32(arg->scan_mode);
	cmd->scan_center_freq1 = cpu_to_le32(arg->scan_center_freq1);
	cmd->scan_chan_width = cpu_to_le32(arg->scan_chan_width);
	cmd->scan_chan_freq = cpu_to_le32(arg->scan_chan_freq);
	cmd->scan_center_freq2 = cpu_to_le32(arg->scan_center_freq2);
	cmd->recapture = cpu_to_le32(arg->recapture);

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "WMI spectral conf: vdev=%u count=%u period=%u fft_size=%u priority=%u gc_ena=%u restart=%u nf_ref=%u init_delay=%u nb_tone=%u str_bin=%u wb_rpt=%u rssi_rpt=%u rssi_thr=%u pwr_fmt=%u rpt_mode=%u bin_scale=%u dbm_adj=%u chn_mask=%u mode=%u cf1=%u cf2=%u chan_width=%u chan_freq=%u recapture=%u\n",
		   arg->vdev_id, arg->scan_count, arg->scan_period, arg->scan_fft_size,
		   arg->scan_priority, arg->scan_gc_ena, arg->scan_restart_ena,
		   arg->scan_noise_floor_ref, arg->scan_init_delay, arg->scan_nb_tone_thr,
		   arg->scan_str_bin_thr, arg->scan_wb_rpt_mode, arg->scan_rssi_rpt_mode,
		   arg->scan_rssi_thr, arg->scan_pwr_format, arg->scan_rpt_mode,
		   arg->scan_bin_scale, arg->scan_dbm_adj, arg->scan_chn_mask,
		   arg->scan_mode, arg->scan_center_freq1, arg->scan_center_freq2,
		   arg->scan_chan_width, arg->scan_chan_freq, arg->recapture);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral scan config wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_vdev_spectral_enable(struct ath12k *ar, u32 vdev_id,
				    u32 trigger, u32 enable)
{
	struct ath12k_wmi_vdev_spectral_enable_cmd *cmd;
	struct sk_buff *skb;
	u32 scan_mode = ATH12K_WMI_SPECTRAL_SCAN_MODE_NORMAL;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_enable_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_ENABLE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->trigger_cmd = cpu_to_le32(trigger);
	cmd->enable_cmd = cpu_to_le32(enable);
#ifdef CPTCFG_ATH12K_SPECTRAL
	if (ar->spectral.mode == SPECTRAL_SCAN_MODE_AGILE)
		scan_mode = ATH12K_WMI_SPECTRAL_SCAN_MODE_AGILE;
#endif
	cmd->scan_mode = cpu_to_le32(scan_mode);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI spectral enable cmd vdev id 0x%x trigger_cmd %u enable_cmd %u scan_mode %u\n",
		   vdev_id, trigger, enable, le32_to_cpu(cmd->scan_mode));

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral enable wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_pdev_dma_ring_cfg(struct ath12k *ar,
				 struct ath12k_wmi_pdev_dma_ring_cfg_arg *arg)
{
	struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DMA_RING_CFG_REQ,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(arg->pdev_id);
	cmd->module_id = cpu_to_le32(arg->module_id);
	cmd->base_paddr_lo = cpu_to_le32(arg->base_paddr_lo);
	cmd->base_paddr_hi = cpu_to_le32(arg->base_paddr_hi);
	cmd->head_idx_paddr_lo = cpu_to_le32(arg->head_idx_paddr_lo);
	cmd->head_idx_paddr_hi = cpu_to_le32(arg->head_idx_paddr_hi);
	cmd->tail_idx_paddr_lo = cpu_to_le32(arg->tail_idx_paddr_lo);
	cmd->tail_idx_paddr_hi = cpu_to_le32(arg->tail_idx_paddr_hi);
	cmd->num_elems = cpu_to_le32(arg->num_elems);
	cmd->buf_size = cpu_to_le32(arg->buf_size);
	cmd->num_resp_per_event = cpu_to_le32(arg->num_resp_per_event);
	cmd->event_timeout_ms = cpu_to_le32(arg->event_timeout_ms);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI DMA ring cfg req cmd pdev_id 0x%x\n",
		   arg->pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PDEV_DMA_RING_CFG_REQ_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send dma ring cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

static int ath12k_wmi_dma_buf_entry_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_ENTRY)
		return -EPROTO;

	if (arg->num_buf_entry >= le32_to_cpu(arg->fixed.num_buf_release_entry))
		return -ENOBUFS;

	arg->num_buf_entry++;
	return 0;
}

static int ath12k_wmi_dma_buf_meta_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_SPECTRAL_META_DATA)
		return -EPROTO;

	if (arg->num_meta >= le32_to_cpu(arg->fixed.num_meta_data_entry))
		return -ENOBUFS;

	arg->num_meta++;

	return 0;
}

static int ath12k_wmi_dma_buf_parse(struct ath12k_base *ab,
				    u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;
	const struct ath12k_wmi_dma_buf_release_fixed_params *fixed;
	u32 pdev_id;
	int ret;

	switch (tag) {
	case WMI_TAG_DMA_BUF_RELEASE:
		fixed = ptr;
		arg->fixed = *fixed;
		pdev_id = DP_HW2SW_MACID(le32_to_cpu(fixed->pdev_id));
		arg->fixed.pdev_id = cpu_to_le32(pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (!arg->buf_entry_done) {
			arg->num_buf_entry = 0;
			arg->buf_entry = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_entry_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf entry tlv %d\n",
					    ret);
				return ret;
			}

			arg->buf_entry_done = true;
		} else if (!arg->meta_data_done) {
			arg->num_meta = 0;
			arg->meta_data = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_meta_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf meta tlv %d\n",
					    ret);
				return ret;
			}

			arg->meta_data_done = true;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath12k_wmi_pdev_dma_ring_buf_release_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	struct ath12k_wmi_dma_buf_release_arg arg = {};
	struct ath12k_dbring_buf_release_event param;
	int ret;


	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dma_buf_parse,
				  &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma buf release tlv %d\n", ret);
		return;
	}

	param.fixed = arg.fixed;
	param.buf_entry = arg.buf_entry;
	param.num_buf_entry = arg.num_buf_entry;
	param.meta_data = arg.meta_data;
	param.num_meta = arg.num_meta;

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
		   "dma ring buf release: pdev_id=%u module_id=%u num_buf=%u num_meta=%u\n",
		   le32_to_cpu(param.fixed.pdev_id),
		   le32_to_cpu(param.fixed.module_id),
		   param.num_buf_entry,
		   param.num_meta);

	ret = ath12k_dbring_buffer_release_event(ab, &param);
	if (ret) {
		ath12k_warn(ab, "failed to handle dma buf release event %d\n", ret);
		return;
	}
}

#define make_min_max(max,min) (u32_encode_bits(max, 0xf0) | u32_encode_bits(min, 0xf))

static void
ath12k_wmi_pdev_update_muedca_params_status_event(struct ath12k_base *ab,
						  struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_update_muedca_event *ev;
	struct ieee80211_mu_edca_param_set *params;
	struct ath12k *ar;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MUEDCA_PARAMS_CONFIG_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev update muedca params ev");
		goto mem_free;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "MU-EDCA parameter change in invalid pdev %d\n",
			    ev->pdev_id);
		goto unlock;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "update MU-EDCA parameters for radio index %u\n",
		   ar->radio_idx);

	params = kzalloc(sizeof(*params), GFP_ATOMIC);
	if (!params) {
		ath12k_warn(ab,
			    "Failed to allocate memory for updated MU-EDCA Parameters");
		goto unlock;
	}

	params->ac_be.aifsn = ev->aifsn[WMI_AC_BE];
	params->ac_be.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_BE],
						 ev->ecwmin[WMI_AC_BE]);
	params->ac_be.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_BE];

	params->ac_bk.aifsn = ev->aifsn[WMI_AC_BK];
	params->ac_bk.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_BK],
						 ev->ecwmin[WMI_AC_BK]);
	params->ac_bk.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_BK];

	params->ac_vi.aifsn = ev->aifsn[WMI_AC_VI];
	params->ac_vi.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_VI],
						 ev->ecwmin[WMI_AC_VI]);
	params->ac_vi.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_VI];

	params->ac_vo.aifsn = ev->aifsn[WMI_AC_VO];
	params->ac_vo.ecw_min_max = make_min_max(ev->ecwmax[WMI_AC_VO],
						 ev->ecwmin[WMI_AC_VO]);
	params->ac_vo.mu_edca_timer = ev->muedca_expiration_time[WMI_AC_VO];

	cfg80211_update_muedca_params_event(ar->ah->hw->wiphy, ar->radio_idx,
					    params, GFP_ATOMIC);

	kfree(params);
unlock:
	rcu_read_unlock();
mem_free:
	kfree(tb);
}


static int ath12k_wmi_tlv_mac_phy_chainmask_caps(struct ath12k_base *soc,
						 u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_caps *cmask_caps = (struct wmi_mac_phy_chainmask_caps *)ptr;
	struct ath12k_chainmask_table *cmask_table;
	struct ath12k_pdev_cap *pdev_cap;
	u32 tag;
	int i, j;
	int ret = 0;

	if (!svc_rdy_ext->hw_mode_caps)
		return -EINVAL;

	if ((!svc_rdy_ext->arg.num_chainmask_tables) ||
	    (svc_rdy_ext->arg.num_chainmask_tables > ATH12K_MAX_CHAINMASK_TABLES))
		return -EINVAL;

	for (i = 0; i < svc_rdy_ext->arg.num_chainmask_tables; i++) {
		cmask_table = &svc_rdy_ext->arg.chainmask_table[i];

		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			tag = FIELD_GET(WMI_TLV_TAG, cmask_caps->tlv_header);

			if (tag != WMI_TAG_MAC_PHY_CHAINMASK_CAPABILITY)
				return -EPROTO;

			cmask_table->cap_list[j].chainmask = cmask_caps->chainmask;
			cmask_table->cap_list[j].supported_caps = cmask_caps->supported_flags;
			cmask_caps++;
			ath12k_dbg(soc, ATH12K_DBG_WMI,"[id %d] chainmask %x supported_caps %x",
				   cmask_table->table_id, cmask_table->cap_list[j].chainmask,
				   cmask_table->cap_list[j].supported_caps);
		}
	}

	for (i = 0; i < soc->num_radios; i++) {
		bool chainmask_table_found = false;

		pdev_cap = &soc->pdevs[i].cap;
		pdev_cap->agile_spectral_cap = false;
		pdev_cap->agile_spectral_cap_160 = false;
		pdev_cap->agile_spectral_cap_80p80 = false;
		pdev_cap->agile_spectral_cap_320 = false;

		for (j = 0; j < svc_rdy_ext->n_mac_phy_chainmask_combo; j++) {
			cmask_table = &svc_rdy_ext->arg.chainmask_table[j];
			if (cmask_table->table_id ==
					pdev_cap->chainmask_table_id) {
				chainmask_table_found = true;
				break;
			}
		}

		if (!chainmask_table_found) {
			ath12k_warn(soc,
				    "chainmask table id %u not found for pdev %d\n",
				    pdev_cap->chainmask_table_id, i);
			continue;
		}

		for (j = 0; j < cmask_table->num_valid_chainmasks; j++) {
			if (cmask_table->cap_list[j].supported_caps &
			    WMI_SUPPORT_CHAIN_MASK_ADFS) {
				if (cmask_table->cap_list[j].chainmask < BITS_PER_LONG)
					pdev_cap->adfs_chain_mask |=
						(1UL << cmask_table->cap_list[j].chainmask);
				else
					ret = -EINVAL;
			}

			pdev_cap->agile_spectral_cap |=
				!!(cmask_table->cap_list[j].supported_caps &
				   WMI_SUPPORT_AGILE_SPECTRAL);
			pdev_cap->agile_spectral_cap_160 |=
				!!(cmask_table->cap_list[j].supported_caps &
				   WMI_SUPPORT_AGILE_SPECTRAL_160);
			pdev_cap->agile_spectral_cap_320 |=
				!!(cmask_table->cap_list[j].supported_caps &
				   WMI_SUPPORT_AGILE_SPECTRAL_320);
		}

		pdev_cap->agile_spectral_cap_80p80 =
			pdev_cap->agile_spectral_cap_160;

		ath12k_dbg(soc, ATH12K_DBG_WMI, "updated adfs chain mask %lx for pdev %d",
			   pdev_cap->adfs_chain_mask, i);
		ath12k_dbg(soc, ATH12K_DBG_WMI,
			   "updated agile spectral caps for pdev %d: 20_40_80=%d 160=%d 80p80=%d 320=%d",
			   i, pdev_cap->agile_spectral_cap,
			   pdev_cap->agile_spectral_cap_160,
			   pdev_cap->agile_spectral_cap_80p80,
			   pdev_cap->agile_spectral_cap_320);
	}
	return ret;
}

static void ath12k_wmi_free_chainmask_caps(struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext)
{
	int i;

	if (!svc_rdy_ext->arg.num_chainmask_tables)
		return;

	for (i = 0; i < svc_rdy_ext->arg.num_chainmask_tables; i++) {
		if (!svc_rdy_ext->arg.chainmask_table[i].cap_list)
			continue;
		kfree(svc_rdy_ext->arg.chainmask_table[i].cap_list);
		svc_rdy_ext->arg.chainmask_table[i].cap_list = NULL;
	}
}

static int ath12k_wmi_tlv_mac_phy_chainmask_combo_parse(struct ath12k_base *soc,
							u16 tag, u16 len,
							const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct wmi_mac_phy_chainmask_combo *cmask_combo = (struct wmi_mac_phy_chainmask_combo *) ptr;
	u32 i = svc_rdy_ext->n_mac_phy_chainmask_combo;
	struct ath12k_chainmask_table *cmask_table;

	if (tag != WMI_TAG_MAC_PHY_CHAINMASK_COMBO)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_chainmask_combo >= svc_rdy_ext->arg.num_chainmask_tables)
		return -ENOBUFS;

	cmask_table = &svc_rdy_ext->arg.chainmask_table[i];
	cmask_table->table_id = cmask_combo->chainmask_table_id;
	cmask_table->num_valid_chainmasks = cmask_combo->num_valid_chainmask;
	cmask_table->cap_list = kcalloc(cmask_combo->num_valid_chainmask,
					sizeof(struct ath12k_chainmask_caps),
					GFP_ATOMIC);
	if (!svc_rdy_ext->arg.chainmask_table[i].cap_list)
		return -ENOMEM;

	svc_rdy_ext->n_mac_phy_chainmask_combo++;
	return 0;
}

static int ath12k_wmi_hw_mode_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hw_mode_cap_params *hw_mode_cap;
	u32 phy_map = 0;

	if (tag != WMI_TAG_HW_MODE_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_hw_mode_caps >= svc_rdy_ext->arg.num_hw_modes)
		return -ENOBUFS;

	hw_mode_cap = container_of(ptr, struct ath12k_wmi_hw_mode_cap_params,
				   hw_mode_id);
	svc_rdy_ext->n_hw_mode_caps++;

	phy_map = le32_to_cpu(hw_mode_cap->phy_id_map);
	svc_rdy_ext->tot_phy_id += fls(phy_map);

	return 0;
}

static int ath12k_wmi_hw_mode_caps(struct ath12k_base *soc,
				   u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	enum wmi_host_hw_mode_config_type mode, pref;
	u32 i;
	int ret;

	svc_rdy_ext->n_hw_mode_caps = 0;
	svc_rdy_ext->hw_mode_caps = ptr;

	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_hw_mode_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0 ; i < svc_rdy_ext->n_hw_mode_caps; i++) {
		hw_mode_caps = &svc_rdy_ext->hw_mode_caps[i];
		mode = le32_to_cpu(hw_mode_caps->hw_mode_id);

		if (mode >= WMI_HOST_HW_MODE_MAX)
			continue;

		pref = soc->wmi_ab.preferred_hw_mode;

		if (ath12k_hw_mode_pri_map[mode] <= ath12k_hw_mode_pri_map[pref]) {
			svc_rdy_ext->pref_hw_mode_caps = *hw_mode_caps;
			soc->wmi_ab.preferred_hw_mode = mode;
		}
	}

	ath12k_dbg(soc, ATH12K_DBG_WMI, "preferred_hw_mode:%d\n",
		   soc->wmi_ab.preferred_hw_mode);
	if (soc->wmi_ab.preferred_hw_mode == WMI_HOST_HW_MODE_MAX)
		return -EINVAL;

	return 0;
}

static int ath12k_wmi_mac_phy_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_caps >= svc_rdy_ext->tot_phy_id)
		return -ENOBUFS;

	len = min_t(u16, len, sizeof(struct ath12k_wmi_mac_phy_caps_params));
	if (!svc_rdy_ext->n_mac_phy_caps) {
		svc_rdy_ext->mac_phy_caps = kzalloc((svc_rdy_ext->tot_phy_id) * len,
						    GFP_ATOMIC);
		if (!svc_rdy_ext->mac_phy_caps)
			return -ENOMEM;
	}

	memcpy(svc_rdy_ext->mac_phy_caps + svc_rdy_ext->n_mac_phy_caps, ptr, len);
	svc_rdy_ext->n_mac_phy_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps_parse(struct ath12k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT)
		return -EPROTO;

	if (svc_rdy_ext->n_ext_hal_reg_caps >= svc_rdy_ext->arg.num_phy)
		return -ENOBUFS;

	svc_rdy_ext->n_ext_hal_reg_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps(struct ath12k_base *soc,
				       u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg reg_cap = {0};
	int ret;
	u32 i;

	svc_rdy_ext->n_ext_hal_reg_caps = 0;
	svc_rdy_ext->ext_hal_reg_caps = ptr;
	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_ext_hal_reg_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0; i < svc_rdy_ext->arg.num_phy; i++) {
		ret = ath12k_pull_reg_cap_svc_rdy_ext(wmi_handle,
						      svc_rdy_ext->soc_hal_reg_caps,
						      svc_rdy_ext->ext_hal_reg_caps, i,
						      &reg_cap);
		if (ret) {
			ath12k_warn(soc, "failed to extract reg cap %d\n", i);
			return ret;
		}

		if (reg_cap.phy_id >= MAX_RADIOS) {
			ath12k_warn(soc, "unexpected phy id %u\n", reg_cap.phy_id);
			return -EINVAL;
		}

		soc->hal_reg_cap[reg_cap.phy_id] = reg_cap;
	}
	return 0;
}

static int ath12k_wmi_ext_soc_hal_reg_caps_parse(struct ath12k_base *soc,
						 u16 len, const void *ptr,
						 void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	u8 hw_mode_id = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.hw_mode_id);
	u32 phy_id_map;
	int pdev_index = 0;
	int ret;

	svc_rdy_ext->soc_hal_reg_caps = ptr;
	svc_rdy_ext->arg.num_phy = le32_to_cpu(svc_rdy_ext->soc_hal_reg_caps->num_phy);

	soc->num_radios = 0;
	phy_id_map = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.phy_id_map);
	soc->fw_pdev_count = 0;

	while (phy_id_map && soc->num_radios < MAX_RADIOS) {
		ret = ath12k_pull_mac_phy_cap_svc_ready_ext(wmi_handle,
							    svc_rdy_ext,
							    hw_mode_id, soc->num_radios,
							    &soc->pdevs[pdev_index]);
		if (ret) {
			ath12k_warn(soc, "failed to extract mac caps, idx :%d\n",
				    soc->num_radios);
			return ret;
		}

		soc->num_radios++;

		/* For single_pdev_only targets,
		 * save mac_phy capability in the same pdev
		 */
		if (soc->hw_params->single_pdev_only)
			pdev_index = 0;
		else
			pdev_index = soc->num_radios;

		/* TODO: mac_phy_cap prints */
		phy_id_map >>= 1;
	}

	if (soc->hw_params->single_pdev_only) {
		soc->num_radios = 1;
		soc->pdevs[0].pdev_id = 0;
	}

	return 0;
}

static int ath12k_wmi_dma_ring_caps_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *parse = data;

	if (tag != WMI_TAG_DMA_RING_CAPABILITIES)
		return -EPROTO;

	parse->n_dma_ring_caps++;
	return 0;
}

static int ath12k_wmi_alloc_dbring_caps(struct ath12k_base *ab,
					u32 num_cap)
{
	size_t sz;
	void *ptr;

	sz = num_cap * sizeof(struct ath12k_dbring_cap);
	ptr = kzalloc(sz, GFP_ATOMIC);
	if (!ptr)
		return -ENOMEM;

	ab->db_caps = ptr;
	ab->num_db_cap = num_cap;

	return 0;
}

static void ath12k_wmi_free_dbring_caps(struct ath12k_base *ab)
{
	kfree(ab->db_caps);
	ab->db_caps = NULL;
	ab->num_db_cap = 0;
}

static int ath12k_wmi_dma_ring_caps(struct ath12k_base *ab,
				    u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *dma_caps_parse = data;
	struct ath12k_wmi_dma_ring_caps_params *dma_caps;
	struct ath12k_dbring_cap *dir_buff_caps;
	int ret;
	u32 i;

	dma_caps_parse->n_dma_ring_caps = 0;
	dma_caps = (struct ath12k_wmi_dma_ring_caps_params *)ptr;
	ret = ath12k_wmi_tlv_iter(ab, ptr, len,
				  ath12k_wmi_dma_ring_caps_parse,
				  dma_caps_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma ring caps tlv %d\n", ret);
		return ret;
	}

	if (!dma_caps_parse->n_dma_ring_caps)
		return 0;

	if (ab->num_db_cap) {
		ath12k_warn(ab, "Already processed, so ignoring dma ring caps\n");
		return 0;
	}

	ret = ath12k_wmi_alloc_dbring_caps(ab, dma_caps_parse->n_dma_ring_caps);
	if (ret)
		return ret;

	dir_buff_caps = ab->db_caps;
	for (i = 0; i < dma_caps_parse->n_dma_ring_caps; i++) {
		if (le32_to_cpu(dma_caps[i].module_id) >= WMI_DIRECT_BUF_MAX) {
			ath12k_warn(ab, "Invalid module id %d\n",
				    le32_to_cpu(dma_caps[i].module_id));
			ret = -EINVAL;
			goto free_dir_buff;
		}

		dir_buff_caps[i].id = le32_to_cpu(dma_caps[i].module_id);
		dir_buff_caps[i].pdev_id =
			DP_HW2SW_MACID(le32_to_cpu(dma_caps[i].pdev_id));
		dir_buff_caps[i].min_elem = le32_to_cpu(dma_caps[i].min_elem);
		dir_buff_caps[i].min_buf_sz = le32_to_cpu(dma_caps[i].min_buf_sz);
		dir_buff_caps[i].min_buf_align = le32_to_cpu(dma_caps[i].min_buf_align);
	}

	return 0;

free_dir_buff:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_wmi_svc_rdy_ext_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	int ret;
	u32 num_chainmask_tables;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT_EVENT:
		ret = ath12k_pull_svc_ready_ext(wmi_handle, ptr,
						&svc_rdy_ext->arg);
		if (ret) {
			ath12k_warn(ab, "unable to extract ext params\n");
			return ret;
		}
		break;

	case WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS:
		svc_rdy_ext->hw_caps = ptr;
		svc_rdy_ext->arg.num_hw_modes =
			le32_to_cpu(svc_rdy_ext->hw_caps->num_hw_modes);
		num_chainmask_tables =
			le32_to_cpu(svc_rdy_ext->hw_caps->num_chainmask_tables);
		if (num_chainmask_tables > ATH12K_MAX_CHAINMASK_TABLES) {
			ath12k_warn(ab,
				    "invalid num_chainmask_tables %u max %u\n",
				    num_chainmask_tables,
				    ATH12K_MAX_CHAINMASK_TABLES);
			return -EINVAL;
		}
		svc_rdy_ext->arg.num_chainmask_tables = num_chainmask_tables;
		break;

	case WMI_TAG_SOC_HAL_REG_CAPABILITIES:
		ret = ath12k_wmi_ext_soc_hal_reg_caps_parse(ab, len, ptr,
							    svc_rdy_ext);
		if (ret)
			return ret;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!svc_rdy_ext->hw_mode_done) {
			ret = ath12k_wmi_hw_mode_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->hw_mode_done = true;
		} else if (!svc_rdy_ext->mac_phy_done) {
			svc_rdy_ext->n_mac_phy_caps = 0;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_mac_phy_caps_parse,
						  svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse tlv %d\n", ret);
				return ret;
			}

			svc_rdy_ext->mac_phy_done = true;
		} else if (!svc_rdy_ext->ext_hal_reg_done) {
			ret = ath12k_wmi_ext_hal_reg_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->ext_hal_reg_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_combo_done) {
			svc_rdy_ext->n_mac_phy_chainmask_combo = 0;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_chainmask_combo_parse,
						  svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse chainmask combo tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_combo_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_cap_done) {
			ret = ath12k_wmi_tlv_mac_phy_chainmask_caps(ab, len, ptr, svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse chainmask caps tlv %d\n", ret);
				return ret;
			}
			svc_rdy_ext->mac_phy_chainmask_cap_done = true;
		} else if (!svc_rdy_ext->oem_dma_ring_cap_done) {
			svc_rdy_ext->oem_dma_ring_cap_done = true;
		} else if (!svc_rdy_ext->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
						       &svc_rdy_ext->dma_caps_parse);
			if (ret)
				return ret;

			svc_rdy_ext->dma_ring_cap_done = true;
		}
		break;

	default:
		break;
	}
	return 0;
}

static int ath12k_service_ready_ext_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext_parse svc_rdy_ext = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext_parse,
				  &svc_rdy_ext);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		goto err;
	}

	if (!test_bit(WMI_TLV_SERVICE_EXT2_MSG, ab->wmi_ab.svc_map))
		complete(&ab->wmi_ab.service_ready);

	kfree(svc_rdy_ext.mac_phy_caps);
	ath12k_wmi_free_chainmask_caps(&svc_rdy_ext);
	return 0;

err:
	kfree(svc_rdy_ext.mac_phy_caps);
	ath12k_wmi_free_chainmask_caps(&svc_rdy_ext);
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_svc_ready_ext2(struct ath12k_wmi_pdev *wmi_handle,
				      const void *ptr,
				      struct ath12k_wmi_svc_rdy_ext2_arg *arg)
{
	const struct wmi_service_ready_ext2_event *ev = ptr;

	if (!ev)
		return -EINVAL;

	arg->reg_db_version = le32_to_cpu(ev->reg_db_version);
	arg->hw_min_max_tx_power_2ghz = le32_to_cpu(ev->hw_min_max_tx_power_2ghz);
	arg->hw_min_max_tx_power_5ghz = le32_to_cpu(ev->hw_min_max_tx_power_5ghz);
	arg->chwidth_num_peer_caps = le32_to_cpu(ev->chwidth_num_peer_caps);
	arg->preamble_puncture_bw = le32_to_cpu(ev->preamble_puncture_bw);
	arg->max_user_per_ppdu_ofdma = le32_to_cpu(ev->max_user_per_ppdu_ofdma);
	arg->max_user_per_ppdu_mumimo = le32_to_cpu(ev->max_user_per_ppdu_mumimo);
	arg->target_cap_flags = le32_to_cpu(ev->target_cap_flags);
	arg->max_tid_msduq = le32_to_cpu(ev->max_num_msduq_supported_per_tid);
	arg->def_tid_msduq = le32_to_cpu(ev->default_num_msduq_supported_per_tid);
	arg->afc_deployment_type = le32_to_cpu(ev->afc_deployment_type);
	arg->mapc_max_co_ap_peers    = le32_to_cpu(ev->max_uhr_co_ap_peers);
	arg->mapc_max_ctdma_ap_peers = le32_to_cpu(ev->max_uhr_ctdma_ap_peers);
	return 0;
}

static void ath12k_wmi_eht_caps_parse(struct ath12k_pdev *pdev, u32 band,
				      const __le32 cap_mac_info[],
				      const __le32 cap_phy_info[],
				      const __le32 supp_mcs[],
				      const struct ath12k_wmi_ppe_threshold_params *ppet,
				       __le32 cap_info_internal)
{
	struct ath12k_band_cap *cap_band = &pdev->cap.band[band];
	u32 support_320mhz;
	u8 i;

	if (band == NL80211_BAND_6GHZ)
		support_320mhz = cap_band->eht_cap_phy_info[0] &
					IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;

	for (i = 0; i < WMI_MAX_EHTCAP_MAC_SIZE; i++)
		cap_band->eht_cap_mac_info[i] = le32_to_cpu(cap_mac_info[i]);

	for (i = 0; i < WMI_MAX_EHTCAP_PHY_SIZE; i++)
		cap_band->eht_cap_phy_info[i] = le32_to_cpu(cap_phy_info[i]);

	if (band == NL80211_BAND_6GHZ)
		cap_band->eht_cap_phy_info[0] |= support_320mhz;

	cap_band->eht_mcs_20_only = le32_to_cpu(supp_mcs[0]);
	cap_band->eht_mcs_80 = le32_to_cpu(supp_mcs[1]);
	if (band != NL80211_BAND_2GHZ) {
		cap_band->eht_mcs_160 = le32_to_cpu(supp_mcs[2]);
		cap_band->eht_mcs_320 = le32_to_cpu(supp_mcs[3]);
	}

	cap_band->eht_ppet.numss_m1 = le32_to_cpu(ppet->numss_m1);
	cap_band->eht_ppet.ru_bit_mask = le32_to_cpu(ppet->ru_info);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cap_band->eht_ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ppet->ppet16_ppet8_ru3_ru0[i]);

	cap_band->eht_cap_info_internal = le32_to_cpu(cap_info_internal);
}

static int ath12k_pull_peer_tx_pn_ev(struct ath12k_base *ab, struct sk_buff *skb,
				     struct wmi_peer_tx_pn_arg *peer_tx_pn)
{
	const struct wmi_peer_tx_pn_event *ev;
	const void **tb;
	int ret;
	u64 tsc = 0;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_TX_PN_RESPONSE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer tx pn ev\n");
		kfree(tb);
		return -EPROTO;
	}

	peer_tx_pn->vdev_id = le32_to_cpu(ev->vdev_id);
	ether_addr_copy(peer_tx_pn->mac_addr,
			ev->peer_macaddr.addr);
	peer_tx_pn->key_idx = le32_to_cpu(ev->key_ix);
	peer_tx_pn->key_cipher = le32_to_cpu(ev->key_cipher);
	memcpy(peer_tx_pn->pn, ev->pn, sizeof(ev->pn));

	/* The PN is provided in little endian order, with bits 7:0 of the PN
	 * residing in pn[0]. The key_type indirectly specifies the packet
	 * number length, and thus how many bytes within pn[] are filled with
	 * valid data. CCMP/GCMP PN is a 6byte value with firmware always
	 * returning the PN of next frame.
	 * Decrement by 1 so IPN/BIPN added in M3 IGTK/BIGTK
	 * KDE is that of the previous frame. This is to prevent
	 * flagging of BIP replay counter errors on the station
	 * side for the immediate next frame.
	 */
	tsc = ((u64)peer_tx_pn->pn[5] << 40) |
		((u64)peer_tx_pn->pn[4] << 32) |
		((u64)peer_tx_pn->pn[3] << 24) |
		((u64)peer_tx_pn->pn[2] << 16) |
		((u64)peer_tx_pn->pn[1] << 8)  |
		(u64)peer_tx_pn->pn[0];

	if (tsc > 0)
		tsc--;

	peer_tx_pn->pn[0] = tsc & 0xff;
	peer_tx_pn->pn[1] = (tsc >> 8)  & 0xff;
	peer_tx_pn->pn[2] = (tsc >> 16) & 0xff;
	peer_tx_pn->pn[3] = (tsc >> 24) & 0xff;
	peer_tx_pn->pn[4] = (tsc >> 32) & 0xff;
	peer_tx_pn->pn[5] = (tsc >> 40) & 0xff;

	kfree(tb);
	return 0;
}

static int
ath12k_wmi_tlv_mac_phy_caps_ext_parse(struct ath12k_base *ab,
				      const struct ath12k_wmi_caps_ext_params *caps,
				      struct ath12k_pdev *pdev)
{
	u32 bands;
	int i;

	if (ab->hw_params->single_pdev_only) {
		for (i = 0; i < ab->fw_pdev_count; i++) {
			struct ath12k_fw_pdev *fw_pdev = &ab->fw_pdev[i];

			if (fw_pdev->pdev_id == ath12k_wmi_caps_ext_get_pdev_id(caps) &&
			    fw_pdev->phy_id == le32_to_cpu(caps->phy_id)) {
				bands = fw_pdev->supported_bands;
				break;
			}
		}

		if (i == ab->fw_pdev_count)
			return -EINVAL;
	} else {
		bands = pdev->cap.supported_bands;
	}

	if (bands & WMI_HOST_WLAN_2GHZ_CAP)
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_2GHZ,
					  caps->eht_cap_mac_info_2ghz,
					  caps->eht_cap_phy_info_2ghz,
					  caps->eht_supp_mcs_ext_2ghz,
					  &caps->eht_ppet_2ghz,
					  caps->eht_cap_info_internal);

	if (bands & WMI_HOST_WLAN_5GHZ_CAP) {
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_5GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);

		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_6GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);
	}

	pdev->cap.eml_cap = le32_to_cpu(caps->eml_capability);
	pdev->cap.mld_cap = le32_to_cpu(caps->mld_capability);
	pdev->cap.ext_mld_cap = le32_to_cpu(caps->ext_mld_capability);

	/* EHT Duplicate in 6 GHz (MCS14) support: bit 23 of eht_cap_phy_info[1]
	 * corresponds to IEEE 802.11be EHT PHY cap byte 6 bit 7
	 * (IEEE80211_EHT_PHY_CAP6_EHT_DUP_6GHZ_SUPP = 0x80).
	 */
	if (bands & WMI_HOST_WLAN_5GHZ_CAP)
		pdev->cap.eht_dup_6ghz_supp =
			!!(le32_to_cpu(caps->eht_cap_phy_info_5ghz[1]) & BIT(23));
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "EHT MCS14 dup 6GHz pdev %d: %s (eht_cap_phy_5ghz[1]=0x%08x)\n",
		   pdev->pdev_id,
		   pdev->cap.eht_dup_6ghz_supp ? "supported" : "not supported",
		   le32_to_cpu(caps->eht_cap_phy_info_5ghz[1]));

	return 0;
}

static int ath12k_wmi_tlv_mac_phy_caps_ext(struct ath12k_base *ab, u16 tag,
					   u16 len, const void *ptr,
					   void *data)
{
	const struct ath12k_wmi_caps_ext_params *caps = ptr;
	struct ath12k_band_cap *cap_band;
	u32 support_320mhz;
	int i = 0, ret;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES_EXT)
		return -EPROTO;

	if (ab->hw_params->single_pdev_only) {
		if (caps->hw_mode_id == WMI_HOST_HW_MODE_SINGLE) {
			support_320mhz = le32_to_cpu(caps->eht_cap_phy_info_5ghz[0]) &
					 IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;
			cap_band = &ab->pdevs[0].cap.band[NL80211_BAND_6GHZ];
			cap_band->eht_cap_phy_info[0] |= support_320mhz;
		}

		if (ab->wmi_ab.preferred_hw_mode != le32_to_cpu(caps->hw_mode_id))
			return 0;
	} else {
		for (i = 0; i < ab->num_radios; i++) {
			if (ab->pdevs[i].pdev_id ==
			    ath12k_wmi_caps_ext_get_pdev_id(caps))
				break;
		}

		if (i == ab->num_radios)
			return -EINVAL;
	}

	ret = ath12k_wmi_tlv_mac_phy_caps_ext_parse(ab, caps, &ab->pdevs[i]);
	if (ret) {
		ath12k_warn(ab,
			    "failed to parse extended MAC PHY capabilities for pdev %d: %d\n",
			    ab->pdevs[i].pdev_id, ret);
		return ret;
	}

	return 0;
}

static void ath12k_wmi_uhr_caps_parse(struct ath12k_pdev *pdev, u32 band,
				      const __le32 cap_mac_info[],
				      const __le32 cap_phy_info[],
				      const __le32 npca_info)
{
	struct ath12k_band_cap *cap_band = &pdev->cap.band[band];
	u8 i;

	for (i = 0; i < WMI_MAX_UHRCAP_MAC_SIZE; i++)
		cap_band->uhr_cap_mac_info[i] = le32_to_cpu(cap_mac_info[i]);

	for (i = 0; i < WMI_MAX_UHRCAP_PHY_SIZE; i++)
		cap_band->uhr_cap_phy_info[i] = le32_to_cpu(cap_phy_info[i]);

	cap_band->uhr_param_npca_info = le32_to_cpu(npca_info);
}

static int
ath12k_wmi_tlv_mac_phy_caps_ext2_parse(struct ath12k_base *ab,
				       const struct ath12k_wmi_caps_ext2_params *caps,
				       struct ath12k_pdev *pdev)
{
	u32 bands = 0;
	u32 nss_info;
	int i;

	if (ab->hw_params->single_pdev_only) {
		if (caps->hw_mode_id == WMI_HOST_HW_MODE_SINGLE)
			return 0;

		for (i = 0; i < ab->fw_pdev_count; i++) {
			struct ath12k_fw_pdev *fw_pdev = &ab->fw_pdev[i];

			if (fw_pdev->pdev_id == ath12k_wmi_caps_ext2_get_pdev_id(caps) &&
			    fw_pdev->phy_id == le32_to_cpu(caps->phy_id)) {
				bands = fw_pdev->supported_bands;
				break;
			}
		}

		if (i == ab->fw_pdev_count)
			return -EINVAL;
	} else {
		bands = pdev->cap.supported_bands;
	}

	if (bands & WMI_HOST_WLAN_2GHZ_CAP)
		ath12k_wmi_uhr_caps_parse(pdev, NL80211_BAND_2GHZ,
					  caps->uhr_cap_mac_info_2ghz,
					  caps->uhr_cap_phy_info_2ghz,
					  cpu_to_le32(0));

	if (bands & WMI_HOST_WLAN_5GHZ_CAP) {
		ath12k_wmi_uhr_caps_parse(pdev, NL80211_BAND_5GHZ,
					  caps->uhr_cap_mac_info_5ghz,
					  caps->uhr_cap_phy_info_5ghz,
					  caps->npca_capability);

		ath12k_wmi_uhr_caps_parse(pdev, NL80211_BAND_6GHZ,
					  caps->uhr_cap_mac_info_5ghz,
					  caps->uhr_cap_phy_info_5ghz,
					  caps->npca_capability);
	}

	/* FW provides max TX/RX NSS info via nss_info in EXT2 caps */
	nss_info = le32_to_cpu(caps->nss_info);
	pdev->cap.max_tx_nss =
		le32_get_bits(nss_info, WMI_MAC_PHY_CAPABILITIES_EXT2_MAX_TX_NSS_MASK);
	pdev->cap.max_rx_nss =
		le32_get_bits(nss_info, WMI_MAC_PHY_CAPABILITIES_EXT2_MAX_RX_NSS_MASK);

	/* Sanity check FW-provided NSS values; cap to max supported (8) */
	if (pdev->cap.max_tx_nss > WMI_MAX_NUM_SS) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "pdev %d: Invalid max_tx_nss %d from FW, capping to %d\n",
			   pdev->pdev_id, pdev->cap.max_tx_nss, WMI_MAX_NUM_SS);
		pdev->cap.max_tx_nss = 0; /* Fallback to legacy behavior */
	}
	if (pdev->cap.max_rx_nss > WMI_MAX_NUM_SS) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "pdev %d: Invalid max_rx_nss %d from FW, capping to %d\n",
			   pdev->pdev_id, pdev->cap.max_rx_nss, WMI_MAX_NUM_SS);
		pdev->cap.max_rx_nss = 0; /* Fallback to legacy behavior */
	}

	return 0;
}

static int ath12k_wmi_tlv_mac_phy_caps_ext2(struct ath12k_base *ab, u16 tag,
					    u16 len, const void *ptr,
					    void *data)
{
	const struct ath12k_wmi_caps_ext2_params *caps = ptr;
	int i = 0, ret;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES_EXT2)
		return -EPROTO;

	if (ab->hw_params->single_pdev_only) {
		if (ab->wmi_ab.preferred_hw_mode != le32_to_cpu(caps->hw_mode_id) &&
		    caps->hw_mode_id != WMI_HOST_HW_MODE_SINGLE)
			return 0;
	} else {
		for (i = 0; i < ab->num_radios; i++) {
			if (ab->pdevs[i].pdev_id ==
			    ath12k_wmi_caps_ext2_get_pdev_id(caps))
				break;
		}

		if (i == ab->num_radios)
			return -EINVAL;
	}

	ret = ath12k_wmi_tlv_mac_phy_caps_ext2_parse(ab, caps, &ab->pdevs[i]);
	if (ret) {
		ath12k_warn(ab,
			    "failed to parse extended 2 MAC PHY capabilities for pdev %d: %d\n",
			    ab->pdevs[i].pdev_id, ret);
		return ret;
	}

	return 0;
}

static int ath12k_wmi_tlv_twt_caps_params(struct ath12k_base *ab, u16 tag,
					  u16 len, const void *ptr,
					  void *data)
{
	const struct ath12k_wmi_twt_caps_params *caps = ptr;

	if (tag != WMI_TAG_TWT_CAPS_PARAMS)
		return -EPROTO;

	ab->twt_cap_bitmap = le32_to_cpu(caps->twt_capability_bitmap);

	return 0;
}

static int
ath12k_wmi_tlv_enhanced_aoa_caps_parse(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct ath12k_wmi_enh_aoa_caps_arg *aoa_cap = data;
	const struct ath12k_wmi_enhanced_aoa_caps_param *caps = ptr;
	u32 tbl_idx, word_idx, byte_idx;

	if (tag != WMI_TAG_ENHANCED_AOA_CAPS_PARAM)
		return -EPROTO;

	aoa_cap->max_agc_gain_tbls = le32_to_cpu(caps->max_agc_gain_tbls);
	if (aoa_cap->max_agc_gain_tbls > ATH12K_PSOC_MAX_NUM_AGC_GAIN_TBLS)
		return -EINVAL;

	for (tbl_idx = 0; tbl_idx < aoa_cap->max_agc_gain_tbls; tbl_idx++) {
		word_idx = tbl_idx / WMI_AOA_BDF_NUM_GAIN_TBL_ELEMS_PER_WORD;
		byte_idx = tbl_idx % WMI_AOA_BDF_NUM_GAIN_TBL_ELEMS_PER_WORD;
		aoa_cap->max_bdf_entries_per_tbl[tbl_idx] =
			(le32_to_cpu(caps->max_bdf_gain_entries[word_idx]) >>
			 (byte_idx * 8)) & 0xFF;
	}

	return 0;
}

static int
ath12k_wmi_tlv_enh_aoa_caps(struct ath12k_base *ab, u16 len,
			    const void *ptr, void *data)
{
	return ath12k_wmi_tlv_iter(ab, ptr, len,
				   ath12k_wmi_tlv_enhanced_aoa_caps_parse,
				   data);
}

static int
ath12k_wmi_tlv_enh_aoa_per_band_caps_parse(struct ath12k_base *ab,
					   u16 tag, u16 len,
					   const void *ptr,
					   void *data)
{
	struct ath12k_wmi_enh_aoa_caps_arg *aoa_cap = data;
	const struct ath12k_wmi_enhanced_aoa_per_band_caps_param *cap = ptr;
	u16 *gain_array;
	u32 tbl_idx, word_idx, elem_idx, band_info;

	if (tag != WMI_TAG_ENHANCED_AOA_PER_BAND_CAPS_PARAM)
		return -EPROTO;

	band_info = le32_to_cpu(cap->band_info);
	switch (band_info) {
	case WMI_AOA_2G:
		gain_array = aoa_cap->max_agc_gain_per_tbl_2g;
		break;
	case WMI_AOA_5G:
		gain_array = aoa_cap->max_agc_gain_per_tbl_5g;
		break;
	case WMI_AOA_6G:
		gain_array = aoa_cap->max_agc_gain_per_tbl_6g;
		break;
	default:
		return 0;
	}

	for (tbl_idx = 0; tbl_idx < aoa_cap->max_agc_gain_tbls; tbl_idx++) {
		word_idx = tbl_idx / WMI_AOA_NUM_GAIN_TBL_ELEMS_PER_WORD;
		elem_idx = tbl_idx % WMI_AOA_NUM_GAIN_TBL_ELEMS_PER_WORD;
		gain_array[tbl_idx] =
			(le32_to_cpu(cap->max_agc_gain[word_idx]) >>
			 (elem_idx * 16)) & 0xFFFF;
	}

	return 0;
}

static int
ath12k_wmi_tlv_enh_aoa_per_band_caps(struct ath12k_base *ab,
				     u16 len, const void *ptr,
				     void *data)
{
	struct ath12k_wmi_enh_aoa_caps_arg *aoa_cap = data;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, ptr, len,
				  ath12k_wmi_tlv_enh_aoa_per_band_caps_parse,
				  data);
	if (ret)
		return ret;

	aoa_cap->valid = true;

	return 0;
}

static int ath12k_wmi_tlv_shared_cu_mem_config(struct ath12k_base *ab, u16 tag,
					       u16 len, const void *ptr,
					       void *data)
{
	const struct ath12k_wmi_shared_cu_mem_config *cfg;

	if (!test_bit(WMI_TLV_SERVICE_SHARED_CU_MEM_MODEL_COUNT_DOWN,
		      ab->wmi_ab.svc_map))
		return 0;

	if (tag != WMI_TAG_SHARED_CU_MEM_CONFIG)
		return -EPROTO;

	cfg = ptr;

	ab->cu_mem_cfg_mask = le32_to_cpu(cfg->config);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "shared critical update memory config mask: 0x%x\n",
		   ab->cu_mem_cfg_mask);

	return 0;
}

/* TLV iterator callback: counts and validates each EXT2 per-phy entry */
static int ath12k_wmi_hal_reg_caps_ext2_parse(struct ath12k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	u32 *n_hal_reg_caps_ext2 = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT2)
		return -EPROTO;

	if (*n_hal_reg_caps_ext2 >= ab->num_radios)
		return -ENOBUFS;

	(*n_hal_reg_caps_ext2)++;
	return 0;
}

static int ath12k_wmi_hal_reg_caps_ext2(struct ath12k_base *ab,
					u16 len, const void *ptr, void *data)
{
	const struct ath12k_wmi_hal_reg_caps_ext2_params *caps;
	u32 i, n_hal_reg_caps_ext2 = 0;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, ptr, len,
				  ath12k_wmi_hal_reg_caps_ext2_parse,
				  &n_hal_reg_caps_ext2);
	if (ret) {
		ath12k_warn(ab, "failed to iterate hal_reg_caps_ext2 TLV: %d\n",
			    ret);
		return ret;
	}

	if (!n_hal_reg_caps_ext2)
		return 0;

	/*
	 * ptr is the ARRAY_STRUCT payload: a contiguous sequence of
	 * ath12k_wmi_hal_reg_caps_ext2_params structs, each prefixed by its
	 * own TLV header (matching ath12k_wmi_hal_reg_caps_ext_params pattern).
	 */
	caps = (const struct ath12k_wmi_hal_reg_caps_ext2_params *)ptr;

	for (i = 0; i < n_hal_reg_caps_ext2; i++) {
		u32 phy_id = le32_to_cpu(caps[i].phy_id);

		if (phy_id >= MAX_RADIOS) {
			ath12k_warn(ab,
				    "hal_reg_caps_ext2: ignoring invalid phy_id %u\n",
				    phy_id);
			continue;
		}

		ab->hal_reg_cap_ext2[phy_id].phy_id = phy_id;
		ab->hal_reg_cap_ext2[phy_id].wireless_modes_ext =
			le32_to_cpu(caps[i].wireless_modes_ext);
		ab->hal_reg_cap_ext2[phy_id].low_2ghz_chan_ext =
			le32_to_cpu(caps[i].low_2ghz_chan_ext);
		ab->hal_reg_cap_ext2[phy_id].high_2ghz_chan_ext =
			le32_to_cpu(caps[i].high_2ghz_chan_ext);
		ab->hal_reg_cap_ext2[phy_id].low_5ghz_chan_ext =
			le32_to_cpu(caps[i].low_5ghz_chan_ext);
		ab->hal_reg_cap_ext2[phy_id].high_5ghz_chan_ext =
			le32_to_cpu(caps[i].high_5ghz_chan_ext);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "hal_reg_caps_ext2 phy_id %u: 2G_ext [%u, %u] MHz 5G_ext [%u, %u] MHz\n",
			   phy_id,
			   ab->hal_reg_cap_ext2[phy_id].low_2ghz_chan_ext,
			   ab->hal_reg_cap_ext2[phy_id].high_2ghz_chan_ext,
			   ab->hal_reg_cap_ext2[phy_id].low_5ghz_chan_ext,
			   ab->hal_reg_cap_ext2[phy_id].high_5ghz_chan_ext);
	}

	return 0;
}

static int ath12k_wmi_svc_rdy_ext2_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext2_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT2_EVENT:
		ret = ath12k_pull_svc_ready_ext2(wmi_handle, ptr,
						 &parse->arg);
		if (ret) {
			ath12k_warn(ab,
				    "failed to extract wmi service ready ext2 parameters: %d\n",
				    ret);
			return ret;
		}
		ab->chwidth_num_peer_caps = parse->arg.chwidth_num_peer_caps;
		ab->max_tid_msduq = parse->arg.max_tid_msduq;
		ab->def_tid_msduq = parse->arg.def_tid_msduq;
		ab->afc_dev_deployment = parse->arg.afc_deployment_type;
		ab->wmi_ab.dp_peer_meta_data_ver =
			u32_get_bits(parse->arg.target_cap_flags,
				     WMI_TARGET_CAP_FLAGS_RX_PEER_METADATA_VERSION);
		if (test_bit(WMI_TLV_SERVICE_UHR_MAX_CO_AP_PEERS,
			     ab->wmi_ab.svc_map))
			ab->mapc_max_co_ap_peers = parse->arg.mapc_max_co_ap_peers;
		if (test_bit(WMI_TLV_SERVICE_UHR_MAX_CTDMA_AP_PEERS_SUPPORT,
			     ab->wmi_ab.svc_map))
			ab->mapc_max_ctdma_peers = parse->arg.mapc_max_ctdma_ap_peers;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
						       &parse->dma_caps_parse);
			if (ret)
				return ret;

			parse->dma_ring_cap_done = true;
		} else if (!parse->spectral_bin_scaling_done) {
			/* TODO: This is a place-holder as WMI tag for
			 * spectral scaling is before
			 * WMI_TAG_MAC_PHY_CAPABILITIES_EXT
			 */
			parse->spectral_bin_scaling_done = true;
		} else if (!parse->mac_phy_caps_ext_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_caps_ext,
						  parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse extended MAC PHY capabilities WMI TLV: %d\n",
					    ret);
				return ret;
			}

			parse->mac_phy_caps_ext_done = true;
		} else if (!parse->hal_reg_caps_ext2_done) {
			ret = ath12k_wmi_hal_reg_caps_ext2(ab, len, ptr, parse);
			if (ret) {
				ath12k_warn(ab,
					    "failed to parse hal_reg_caps_ext2 WMI TLV: %d\n",
					    ret);
				return ret;
			}
			parse->hal_reg_caps_ext2_done = true;
		} else if (!parse->scan_radio_caps_done) {
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_scan_radio_caps_ext2,
						  parse);
			if (ret) {
				ath12k_warn(ab,
					    "failed to parse SCAN RADIO capabilities WMI TLV: %d\n",
					    ret);
				return ret;
			}
#endif /* CPTCFG_QCN_EXTN */

			parse->scan_radio_caps_done = true;
		} else if (!parse->twt_caps_param_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_twt_caps_params,
						  parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse twt capabilities WMI TLV: %d\n",
					    ret);
				return ret;
			}

			parse->twt_caps_param_done = true;
		} else if (!parse->htt_msdu_idx_to_qtype_map) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->htt_msdu_idx_to_qtype_map = true;
		} else if (!parse->wmi_dbs_or_sbs_cap_ext) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_dbs_or_sbs_cap_ext = true;
		} else if (!parse->cus_bdf_ver_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->cus_bdf_ver_cap = true;
		} else if (!parse->soft_calib_support) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->soft_calib_support = true;
		} else if (!parse->wmi_coex_fix_channel_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_coex_fix_channel_cap = true;
		} else if (!parse->wmi_aux_dev_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_aux_dev_cap = true;
		} else if (!parse->aoa_caps_exchange) {
			ret = ath12k_wmi_tlv_enh_aoa_caps(ab, len, ptr,
							  &parse->enh_aoa_caps);
			if (ret)
				return ret;
			parse->aoa_caps_exchange = true;
		} else if (!parse->aoa_per_band_caps) {
			ret = ath12k_wmi_tlv_enh_aoa_per_band_caps(ab, len, ptr,
								   &parse->enh_aoa_caps);
			if (ret)
				return ret;
			parse->aoa_per_band_caps = true;
		} else if (!parse->wmi_sar_flag_tlv_param) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_sar_flag_tlv_param = true;
		} else if (!parse->wmi_power_boost_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_power_boost_cap = true;
		} else if (!parse->wmi_rssi_accuracy_improvement_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->wmi_rssi_accuracy_improvement_cap = true;
		} else if (!parse->ltf_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->ltf_cap = true;
		} else if (!parse->chain_cap) {
			/* TODO: This is a place-holder as WMI tag
			 * before WMI_TAG_MAC_PHY_CAPABILITIES_EXT2
			 */
			parse->chain_cap = true;
		} else if (!parse->mac_phy_caps_ext2_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_caps_ext2,
						  parse);
			if (ret) {
				ath12k_warn(ab,
					    "failed to parse EXT2 MAC PHY cap WMI TLV: %d\n",
					    ret);
				return ret;
			}

			parse->mac_phy_caps_ext2_done = true;
		} else if (!parse->cu_mem_cfg_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_shared_cu_mem_config,
						  parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse tlv shared cu mem config: %d\n",
					    ret);
				return ret;
			}

			parse->cu_mem_cfg_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_ext2_event(struct ath12k_base *ab,
					   struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext2_parse svc_rdy_ext2 = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext2_parse,
				  &svc_rdy_ext2);
	if (ret) {
		ath12k_warn(ab, "failed to parse ext2 event tlv %d\n", ret);
		goto err;
	}

	ab->enh_aoa_caps = svc_rdy_ext2.enh_aoa_caps;
	complete(&ab->wmi_ab.service_ready);

	return 0;

err:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_vdev_start_resp_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_vdev_start_resp_event *vdev_rsp)
{
	const void **tb;
	const struct wmi_vdev_start_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_START_RESPONSE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev start resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_rsp = *ev;

	kfree(tb);
	return 0;
}

static int ath12k_copy_afc_power_event_fixed_info(struct ath12k_base *ab,
						  struct ath12k_afc_info *afc,
						  const void *ptr,
						  u16 len)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	struct wmi_afc_power_event_param afc_pwr_param = {};
	size_t copy_len;

	if (!ptr)
		return -EINVAL;

	copy_len = min_t(size_t, len, sizeof(afc_pwr_param));
	memcpy(&afc_pwr_param, ptr, copy_len);

	afc_reg_info = kzalloc(sizeof(*afc_reg_info), GFP_ATOMIC);

	if (!afc_reg_info)
		return -ENOMEM;

	afc_reg_info->fw_status_code =
				le32_to_cpu(afc_pwr_param.fw_status_code);
	afc_reg_info->resp_id = le32_to_cpu(afc_pwr_param.resp_id);
	afc_reg_info->serv_resp_code =
				le32_to_cpu(afc_pwr_param.afc_serv_resp_code);
	afc_reg_info->afc_wfa_version =
				le32_to_cpu(afc_pwr_param.afc_wfa_version);
	afc_reg_info->avail_exp_time_d =
				le32_to_cpu(afc_pwr_param.avail_exp_time_d);
	afc_reg_info->avail_exp_time_t =
				le32_to_cpu(afc_pwr_param.avail_exp_time_t);
	afc->afc_reg_info = afc_reg_info;

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "pwr event-fw status %d req id %d server resp code %d wfa version %d expiry date %d time %d\n",
		   afc_reg_info->fw_status_code, afc_reg_info->resp_id,
		   afc_reg_info->serv_resp_code, afc_reg_info->afc_wfa_version,
		   afc_reg_info->avail_exp_time_d, afc_reg_info->avail_exp_time_t);
	return 0;
}

static int ath12k_afc_freq_obj_cmp(const void *a, const void *b)
{
	const struct ath12k_afc_freq_obj *ra = a;
	const struct ath12k_afc_freq_obj *rb = b;

	if (ra->low_freq < rb->low_freq)
		return -1;
	if (ra->low_freq > rb->low_freq)
		return 1;
	return 0;
}

static int ath12k_wmi_afc_fill_freq_obj(struct ath12k_base *ab,
					const void *ptr, u16 len,
					struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct wmi_6ghz_afc_frequency_info *freq_buf = NULL;
	struct ath12k_afc_freq_obj *freq_obj = NULL;
	int i;
	/* AFC payload with 0 freq objects is still considered a valid payload. Hence,
	 * do not return an error.
	 */
	if (!afc_reg_info->num_freq_objs) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "No freq objects in afc power event\n");
		return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Num afc freq obj received %d\n",
		   afc_reg_info->num_freq_objs);
	freq_obj = kzalloc(afc_reg_info->num_freq_objs * sizeof(*freq_obj),
			   GFP_ATOMIC);
	if (!freq_obj)
		return -ENOMEM;

	freq_buf = (struct wmi_6ghz_afc_frequency_info *)ptr;
	for (i = 0; i < afc_reg_info->num_freq_objs; i++) {
		freq_obj[i].low_freq =
				le32_to_cpu(u32_get_bits(freq_buf[i].freq_info,
							 WMI_AFC_LOW_FREQUENCY));
		freq_obj[i].high_freq =
				le32_to_cpu(u32_get_bits(freq_buf[i].freq_info,
							 WMI_AFC_HIGH_FREQUENCY));
		freq_obj[i].max_psd = le32_to_cpu(freq_buf[i].psd_power_info);
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Freq obj: low: %d high: %d psd: %d\n",
			   freq_obj[i].low_freq, freq_obj[i].high_freq,
			   freq_obj[i].max_psd);
	}

	sort(freq_obj, afc_reg_info->num_freq_objs, sizeof(*freq_obj),
	     ath12k_afc_freq_obj_cmp, NULL);
	afc_reg_info->afc_freq_info = freq_obj;

	return 0;
}

static int ath12k_wmi_afc_fill_chan_obj(struct ath12k_base *ab,
					const void *ptr, u16 len,
					struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct ath12k_afc_chan_obj *chan_obj = NULL;
	struct ath12k_chan_eirp_obj *eirp_info = NULL;
	struct wmi_6ghz_afc_channel_info *chan_buf = NULL;
	int i;

	/* AFC payload with 0 channel objects is still considered a valid payload. Hence,
	 * do not return an error.
	 */
	if (!afc_reg_info->num_chan_objs) {
		ath12k_dbg(ab, ATH12K_DBG_AFC, "No channel objects in afc power event\n");
		return 0;
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC, "Num chan objects received %d\n",
		   afc_reg_info->num_chan_objs);
	chan_obj = kzalloc(afc_reg_info->num_chan_objs * sizeof(*chan_obj),
			   GFP_ATOMIC);
	if (!chan_obj)
		return -ENOMEM;

	chan_buf = (struct wmi_6ghz_afc_channel_info *)ptr;
	for (i = 0; i < afc_reg_info->num_chan_objs; i++) {
		chan_obj[i].global_opclass =
			le32_to_cpu(chan_buf[i].global_operating_class);
		chan_obj[i].num_chans =
				le32_to_cpu(chan_buf[i].num_channels);
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Chan obj %d  global_opclass : %d num_chans %d\n", i,
			   chan_obj[i].global_opclass, chan_obj[i].num_chans);
		eirp_info = kzalloc(chan_obj[i].num_chans * sizeof(*eirp_info),
				    GFP_ATOMIC);
		if (!eirp_info) {
			while (--i >= 0)
				kfree(chan_obj[i].chan_eirp_info);
			kfree(chan_obj);
			return -ENOMEM;
		}

		chan_obj[i].chan_eirp_info = eirp_info;
	}

	afc_reg_info->afc_chan_info = chan_obj;

	return 0;
}

static int ath12k_wmi_afc_fill_chan_eirp_obj(struct ath12k_base *ab,
					     const void *ptr, u16 len,
					     struct ath12k_afc_info *afc,
					     u32 total_eirp_info)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct wmi_afc_chan_eirp_power_info *eirp_buf = NULL;
	struct ath12k_afc_chan_obj *chan_info = NULL;
	struct ath12k_chan_eirp_obj *eirp_obj;
	int eirp_count, idx1, idx2, count = 0;

	eirp_buf = (struct wmi_afc_chan_eirp_power_info *)ptr;
	chan_info = afc_reg_info->afc_chan_info;
	for (idx1 = 0; idx1 < afc_reg_info->num_chan_objs; ++idx1) {
		eirp_obj = chan_info[idx1].chan_eirp_info;
		eirp_count = le32_to_cpu(chan_info[idx1].num_chans);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Chan obj %d Chan eirp count %d\n",
			   idx1, eirp_count);
		for (idx2 = 0; idx2 < eirp_count; ++idx2) {
			if (count >= total_eirp_info) {
				ath12k_warn(ab,
					    "AFC chan EIRP info truncated at chan obj %d idx %d, available %u\n",
					    idx1, idx2, total_eirp_info);
				return 0;
			}

			eirp_obj[idx2].cfi =
				le32_to_cpu(eirp_buf[count].channel_cfi);
			eirp_obj[idx2].eirp_power =
					le32_to_cpu(eirp_buf[count].eirp_pwr);
		ath12k_dbg(ab, ATH12K_DBG_AFC, "Chan eirp obj %d CFI %d EIRP %d\n",
			   idx2, eirp_obj[idx2].cfi, eirp_obj[idx2].eirp_power);
			++count;
		}
	}

	return 0;
}

/**
 * ath12k_copy_afc_expiry_event - Copy AFC expiry event from WMI params
 * @ab: Pointer to ath12k_base structure
 * @afc: Pointer to AFC information structure
 * @ptr: Pointer to event data
 * @len: Length of event data
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_copy_afc_expiry_event(struct ath12k_base *ab,
					struct ath12k_afc_info *afc, const void *ptr,
					u16 len)
{
	struct wmi_afc_expiry_event_param param = {};
	size_t copy_len;

	if (!ptr)
		return -EINVAL;

	copy_len = min_t(size_t, len, sizeof(param));
	memcpy(&param, ptr, copy_len);

	afc->request_id = le32_to_cpu(param.request_id);
	afc->event_subtype = le32_to_cpu(param.event_subtype);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "Received AFC expiry request id %u subtye %d\n",
		   afc->request_id, afc->event_subtype);
	return 0;
}

static int ath12k_wmi_afc_event_parser(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct ath12k_afc_info *afc = (struct ath12k_afc_info *)data;
	int total_eirp_obj, sub_tlv_size, ret = 0;
	struct wmi_tlv *tlv;
	u16 tlv_tag;
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;

	ath12k_dbg(ab, ATH12K_DBG_AFC, "AFC event tag 0x%x of len %d type %d\n",
		   tag, len, afc->event_type);

	switch (tag) {
	case WMI_TAG_AFC_EVENT_FIXED_PARAM:
		struct wmi_afc_event_fixed_param *fixed_param;

		if (!ptr || !len || len < sizeof(*fixed_param)) {
			ath12k_warn(ab,
				    "Wrong data or length(%d) for AFC fixed param\n",
				    len);
			return -EINVAL;
		}

		fixed_param = (struct wmi_afc_event_fixed_param *)ptr;
		afc->phy_id = le32_to_cpu(fixed_param->pdev_id);
		afc->event_type = le32_to_cpu(fixed_param->event_type);
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Received AFC event of type %d for pdev: %d\n",
			   afc->event_type, afc->phy_id);
		break;
	case WMI_TAG_AFC_EXPIRY_EVENT_PARAM:
		if (len == 0) {
			ath12k_warn(ab, "len should not be zero\n");
			return 0;
		}

		if (afc->event_type != ATH12K_AFC_EVENT_TIMER_EXPIRY)
			return 0;

		ret = ath12k_copy_afc_expiry_event(ab, afc, ptr, len);
		if (ret) {
			ath12k_warn(ab, "Failed to copy expiry event\n");
			return ret;
		}
		break;
	case WMI_TAG_AFC_POWER_EVENT_PARAM:
		if (len == 0)
			return 0;

		if (afc->event_type != ATH12K_AFC_EVENT_POWER_INFO)
			return 0;

		ret = ath12k_copy_afc_power_event_fixed_info(ab, afc, ptr, len);
		if (ret) {
			ath12k_warn(ab, "Failed to copy power event fixed info\n");
			return ret;
		}
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (len == 0)
			return 0;

		tlv = (struct wmi_tlv *)ptr;
		tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);

		if (tlv_tag == WMI_TAG_AFC_6GHZ_FREQUENCY_INFO) {
			sub_tlv_size = sizeof(struct wmi_6ghz_afc_frequency_info);
			afc_reg_info->num_freq_objs = len / sub_tlv_size;
			ret = ath12k_wmi_afc_fill_freq_obj(ab, ptr, len, afc);
		} else if (tlv_tag == WMI_TAG_AFC_6GHZ_CHANNEL_INFO) {
			sub_tlv_size = sizeof(struct wmi_6ghz_afc_channel_info);
			afc_reg_info->num_chan_objs = len / sub_tlv_size;
			ret = ath12k_wmi_afc_fill_chan_obj(ab, ptr, len, afc);
		} else if (tlv_tag == WMI_TAG_AFC_CHAN_EIRP_POWER_INFO) {
			sub_tlv_size = sizeof(struct wmi_afc_chan_eirp_power_info);
			total_eirp_obj = len / sub_tlv_size;
			ret =  ath12k_wmi_afc_fill_chan_eirp_obj(ab, ptr, len, afc,
								 total_eirp_obj);
		}
		break;
	default:
		ath12k_warn(ab, "Unknown afc event tag %d\n", tag);
		return -EINVAL;
	}

	return ret;
}

static void ath12k_wmi_afc_event(struct ath12k_base *ab,
				 struct sk_buff *skb)
{
	struct ath12k_afc_info afc = {};
	struct ath12k_afc_info *afc_info = &afc;
	struct ath12k *ar;
	int ret;
	u8 pdev_id;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_afc_event_parser, afc_info);
	if (ret) {
		ath12k_free_afc_power_event_info(afc_info);
		ath12k_warn(ab, "Failed to parse afc event type %d ret = %d\n",
			    afc_info->event_type, ret);
		return;
	}

	if (afc_info->event_type == ATH12K_AFC_EVENT_POWER_INFO)
		ath12k_info(ab, "Received AFC Event  event_type (%d)",
			    afc_info->event_type);

#ifdef CPTCFG_QCN_EXTN
	if (afc_info->event_type == ATH12K_AFC_EVENT_POWER_INFO &&
	    ath12k_wmi_handle_afc_power_info_hwbl_extn(ab, skb, afc_info))
		return;
#endif

	pdev_id = afc_info->phy_id;
	if (pdev_id >= ab->num_radios) {
		ath12k_warn(ab, "Received AFC Event for Invalid phy id %d\n", pdev_id);
		ath12k_free_afc_power_event_info(afc_info);
		return;
	}

	ar = ab->pdevs[pdev_id].ar;
	if (!ar && afc_info->event_type != ATH12K_AFC_EVENT_TIMER_EXPIRY) {
		ath12k_warn(ab, "Failed to get ar for afc processing\n");
		ath12k_free_afc_power_event_info(afc_info);
		return;
	}

	switch (afc_info->event_type) {
	case ATH12K_AFC_EVENT_POWER_INFO:
		ret = ath12k_reg_process_afc_power_event(ar, afc_info);
		if (ret)
			ath12k_warn(ab, "AFC reg rule update failed ret : %d\n",
				    ret);

		/* Update AFC application with power event update complete */
		ath12k_vendor_send_power_update_complete(ar, afc_info);
		if (afc_info->afc_reg_info->fw_status_code !=
		    REG_FW_AFC_POWER_EVENT_SUCCESS) {
			ath12k_free_afc_power_event_info(afc_info);
		}

		break;
	case ATH12K_AFC_EVENT_TIMER_EXPIRY:
		if (ar) {
			ar->afc.event_type = afc_info->event_type;
			ar->afc.event_subtype = afc_info->event_subtype;
			ar->afc.request_id = afc_info->request_id;
			ret = ath12k_process_expiry_event(ar);
			if (ret)
				ath12k_warn(ab, "Failed to process expiry event\n");
		} else {
			ab->afc_exp_info[pdev_id].is_afc_exp_valid = true;
			ab->afc_exp_info[pdev_id].event_subtype = afc_info->event_subtype;
			ab->afc_exp_info[pdev_id].req_id = afc_info->request_id;
			ath12k_dbg(ab, ATH12K_DBG_AFC,
				   "AFC expiry event received without ar\n");
		}
		break;
	default:
#ifdef CPTCFG_QCN_EXTN
		/* event_type=0 for AFC HWBL continuation events */
		if (ath12k_wmi_handle_afc_continuation_hwbl_extn(ab, skb, afc_info))
			return;
#endif
		ath12k_warn(ab, "AFC reg rule update failed ret : %d\n", ret);
		break;
	};
}

int ath12k_wmi_send_afc_cmd_tlv(struct ath12k *ar, int data_type,
				enum wmi_afc_cmd_type afc_cmd)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	int ret = 0;
	struct sk_buff *skb;
	struct wmi_afc_cmd_fixed_param *cmd = NULL;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_afc_cmd_fixed_param *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_AFC_CMD_FIXED_PARAM) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->cmd_type = cpu_to_le32(afc_cmd);
	cmd->serv_resp_format = cpu_to_le32(data_type);

	switch (afc_cmd) {
	case WMI_AFC_CMD_SERV_RESP_READY:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc indication for pdev id %d resp format %d\n",
			   cmd->pdev_id, cmd->serv_resp_format);
		break;
	case WMI_AFC_CMD_CLEAR_AFC_PAYLOAD:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc payload clear cmd for pdev id %d\n",
			   cmd->pdev_id);
		break;
	case WMI_AFC_CMD_RESET_AFC:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Sending afc reset cmd for pdev id %d\n",
			   cmd->pdev_id);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "Unknown AFC command\n");
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_AFC_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_AFC_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static struct ath12k_reg_rule
*create_ext_reg_rules_from_wmi(u32 num_reg_rules,
			       struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
{
	struct ath12k_reg_rule *reg_rule_ptr;
	u32 count;

	reg_rule_ptr = kzalloc((num_reg_rules * sizeof(*reg_rule_ptr)),
			       GFP_ATOMIC);

	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_START_FREQ);
		reg_rule_ptr[count].end_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_END_FREQ);
		reg_rule_ptr[count].max_bw =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_MAX_BW);
		reg_rule_ptr[count].reg_power =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_REG_PWR);
		reg_rule_ptr[count].ant_gain =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_ANT_GAIN);
		reg_rule_ptr[count].flags =
			le32_get_bits(wmi_reg_rule[count].flag_info,
				      REG_RULE_FLAGS);
		reg_rule_ptr[count].psd_flag =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_INFO);
		reg_rule_ptr[count].psd_eirp =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_EIRP);
	}

	return reg_rule_ptr;
}

static u8 ath12k_wmi_ignore_num_extra_rules(struct ath12k_wmi_reg_rule_ext_params *rule,
					    u32 num_reg_rules)
{
	u8 num_invalid_5ghz_rules = 0;
	u32 count, start_freq;

	for (count = 0; count < num_reg_rules; count++) {
		start_freq = le32_get_bits(rule[count].freq_info, REG_RULE_START_FREQ);

		if (start_freq >= ATH12K_MIN_6GHZ_FREQ)
			num_invalid_5ghz_rules++;
	}

	return num_invalid_5ghz_rules;
}

static const char *ath12k_cc_status_to_str(enum ath12k_reg_cc_code code)
{
       switch (code) {
       case REG_SET_CC_STATUS_PASS:
               return "REG_SET_CC_STATUS_PASS";
       case REG_CURRENT_ALPHA2_NOT_FOUND:
               return "REG_CURRENT_ALPHA2_NOT_FOUND";
       case REG_INIT_ALPHA2_NOT_FOUND:
               return "REG_INIT_ALPHA2_NOT_FOUND";
       case REG_SET_CC_CHANGE_NOT_ALLOWED:
               return "REG_SET_CC_CHANGE_NOT_ALLOWED";
       case REG_SET_CC_STATUS_NO_MEMORY:
               return "REG_SET_CC_STATUS_NO_MEMORY";
       case REG_SET_CC_STATUS_FAIL:
               return "REG_SET_CC_STATUS_FAIL";
       default:
               return "unknown cc status";
       }
}

static const char *ath12k_super_reg_6g_to_str(enum reg_super_domain_6g domain_id)
{
        switch (domain_id) {
        case FCC1_6G:
                return "FCC1_6G";
        case ETSI1_6G:
                return "ETSI1_6G";
        case ETSI2_6G:
                return "ETSI2_6G";
        case APL1_6G:
                return "APL1_6G";
        case FCC1_6G_CL:
                return "FCC1_6G_CL";
        default:
                return "unknown domain id";
        }
}

static const char *ath12k_6g_client_type_to_str(enum wmi_reg_6g_client_type type)
{
        switch (type) {
        case WMI_REG_DEFAULT_CLIENT:
                return "DEFAULT CLIENT";
        case WMI_REG_SUBORDINATE_CLIENT:
                return "SUBORDINATE CLIENT";
        default:
                return "unknown client type";
        }
}

static const char *ath12k_6g_ap_type_to_str(enum wmi_reg_6g_ap_type type)
{
        switch (type) {
        case WMI_REG_INDOOR_AP:
                return "INDOOR AP";
        case WMI_REG_STD_POWER_AP:
                return "STANDARD POWER AP";
        case WMI_REG_VLP_AP:
                return "VERY LOW POWER AP";
        default:
                return "unknown AP type";
       }
}

static const char *ath12k_sub_reg_6g_to_str(enum reg_subdomains_6g sub_id)
{
        switch (sub_id) {
        case FCC1_CLIENT_LPI_REGULAR_6G:
                return "FCC1_CLIENT_LPI_REGULAR_6G";
        case FCC1_CLIENT_SP_6G:
                return "FCC1_CLIENT_SP_6G";
        case FCC1_AP_LPI_6G:
                return "FCC1_AP_LPI_6G/FCC1_CLIENT_LPI_SUBORDINATE";
        case FCC1_AP_SP_6G:
                return "FCC1_AP_SP_6G";
        case ETSI1_LPI_6G:
                return "ETSI1_LPI_6G";
        case ETSI1_VLP_6G:
                return "ETSI1_VLP_6G";
        case ETSI2_LPI_6G:
                return "ETSI2_LPI_6G";
        case ETSI2_VLP_6G:
                return "ETSI2_VLP_6G";
        case APL1_LPI_6G:
                return "APL1_LPI_6G";
        case APL1_VLP_6G:
                return "APL1_VLP_6G";
        case EMPTY_6G:
                return "N/A";
        default:
                return "unknown sub reg id";
        }
}

static void ath12k_print_reg_rule(struct ath12k_base *ab, const char *prev,
				  u32 num_reg_rules,
				  const struct ath12k_reg_rule *reg_rule_ptr)
{
       const struct ath12k_reg_rule *reg_rule = reg_rule_ptr;
       u32 count;

       ath12k_dbg(ab, ATH12K_DBG_WMI, "%s reg rules number %d\n", prev, num_reg_rules);

       for (count = 0; count < num_reg_rules; count++) {
	       ath12k_dbg(ab, ATH12K_DBG_WMI,
			  "reg rule %d: (%d - %d @ %d) (%d, %d) (FLAGS %d) (psd flag %d EIRP %d dB/MHz)\n",
			  count + 1, reg_rule->start_freq, reg_rule->end_freq,
			  reg_rule->max_bw, reg_rule->ant_gain, reg_rule->reg_power,
			  reg_rule->flags, reg_rule->psd_flag, reg_rule->psd_eirp);
	       reg_rule++;
       }
}

static u8
ath12k_invalid_5g_reg_ext_rules_from_wmi(u32 num_reg_rules,
                                         struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
{
        u8 num_invalid_5g_rules = 0;
        u32 count, start_freq, end_freq;

        for (count = 0; count < num_reg_rules; count++) {
                start_freq = FIELD_GET(REG_RULE_START_FREQ,
                                       wmi_reg_rule[count].freq_info);
                end_freq = FIELD_GET(REG_RULE_END_FREQ,
                                     wmi_reg_rule[count].freq_info);

                if (start_freq >= ATH12K_MIN_6GHZ_FREQ &&
                    end_freq <= ATH12K_MAX_6GHZ_FREQ)
                        num_invalid_5g_rules++;
        }

        return num_invalid_5g_rules;
}

static int ath12k_pull_reg_chan_list_ext_update_ev(struct ath12k_base *ab,
						   struct sk_buff *skb,
						   struct ath12k_reg_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_ext_event *ev;
	struct ath12k_wmi_reg_rule_ext_params *ext_wmi_reg_rule;
	u32 num_2g_reg_rules, num_5g_reg_rules;
	u32 num_6g_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6g_reg_rules_cl[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u8 num_invalid_5ghz_ext_rules;
	u32 total_reg_rules = 0;
	int ret, i, j, skip_6g_rules_in_5g_rules = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processing regulatory ext channel list\n");

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch reg chan list ext update ev\n");
		kfree(tb);
		return -EPROTO;
	}

#ifdef CPTCFG_QCN_EXTN
	ret = ath12k_wmi_pull_reg_hw_blocklist_extn(ab, skb,
						    &reg_info->reg_info_extn);
	if (ret) {
		kfree(tb);
		return ret;
	}
#endif

	reg_info->num_2g_reg_rules = le32_to_cpu(ev->num_2g_reg_rules);
	reg_info->num_5g_reg_rules = le32_to_cpu(ev->num_5g_reg_rules);
	reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_lpi);
	reg_info->num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_sp);
	reg_info->num_6g_reg_rules_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_vlp);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
                  "6g reg info client type %s rnr_tpe_usable %d unspecified_ap_usable %d AP sub domain: lpi %s , sp %s , vlp %s\n",
                  ath12k_6g_client_type_to_str(reg_info->client_type),
                  reg_info->rnr_tpe_usable,
                  reg_info->unspecified_ap_usable,
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_lpi),
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_sp),
                  ath12k_sub_reg_6g_to_str
                  (ev->domain_code_6g_ap_vlp));

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_lpi[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_sp[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_vlp[i]);
		ath12k_dbg(ab, ATH12K_DBG_WMI,
                  "6g AP BW: lpi %d - %d sp %d - %d vlp %d - %d\n",
                  ev->min_bw_6g_ap_lpi,
                  ev->max_bw_6g_ap_lpi,
                  ev->min_bw_6g_ap_sp,
                  ev->max_bw_6g_ap_sp,
                  ev->min_bw_6g_ap_vlp,
                  ev->max_bw_6g_ap_vlp);
	}

	num_2g_reg_rules = reg_info->num_2g_reg_rules;
	total_reg_rules += num_2g_reg_rules;
	num_5g_reg_rules = reg_info->num_5g_reg_rules;
	total_reg_rules += num_5g_reg_rules;

	if (num_2g_reg_rules > MAX_REG_RULES || num_5g_reg_rules > MAX_REG_RULES) {
		ath12k_warn(ab, "Num reg rules for 2G/5G exceeds max limit (num_2g_reg_rules: %d num_5g_reg_rules: %d max_rules: %d)\n",
			    num_2g_reg_rules, num_5g_reg_rules, MAX_REG_RULES);
		kfree(tb);
		return -EINVAL;
	}

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		num_6g_reg_rules_ap[i] = reg_info->num_6g_reg_rules_ap[i];

		if (num_6g_reg_rules_ap[i] > MAX_6GHZ_REG_RULES) {
			ath12k_warn(ab, "Num 6G reg rules for AP mode(%d) exceeds max limit (num_6g_reg_rules_ap: %d, max_rules: %d)\n",
				    i, num_6g_reg_rules_ap[i], MAX_6GHZ_REG_RULES);
			kfree(tb);
			return -EINVAL;
		}

		total_reg_rules += num_6g_reg_rules_ap[i];
	}

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];

		num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];

		num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];

		if (num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] > MAX_6GHZ_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] > MAX_6GHZ_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] >  MAX_6GHZ_REG_RULES) {
			ath12k_warn(ab, "Num 6g client reg rules exceeds max limit, for client(type: %d)\n",
				    i);
			kfree(tb);
			return -EINVAL;
		}
	}

	switch (le32_to_cpu(ev->status_code)) {
	case WMI_REG_SET_CC_STATUS_PASS:
		reg_info->status_code = REG_SET_CC_STATUS_PASS;
		break;
	case WMI_REG_CURRENT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_CURRENT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_INIT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_INIT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_SET_CC_CHANGE_NOT_ALLOWED:
		reg_info->status_code = REG_SET_CC_CHANGE_NOT_ALLOWED;
		break;
	case WMI_REG_SET_CC_STATUS_NO_MEMORY:
		reg_info->status_code = REG_SET_CC_STATUS_NO_MEMORY;
		break;
	case WMI_REG_SET_CC_STATUS_FAIL:
		reg_info->status_code = REG_SET_CC_STATUS_FAIL;
		break;
	}

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS)
		ath12k_warn(ab, "reg chan list ext event failed status %d: %s\n",
			    reg_info->status_code,
			    ath12k_cc_status_to_str(reg_info->status_code));

	if (!total_reg_rules) {
		u32 phy_id = le32_to_cpu(ev->phy_id);
		struct ath12k *ar;

		ath12k_warn(ab, "No reg rules available, dfs %d, ctry %d domain %d\n",
			    ev->dfs_region, ev->country_id, ev->domain_code);
#ifdef CPTCFG_QCN_EXTN
		if (ath12k_wmi_is_reg_hw_blocklist_fragment_only_extn(ab, reg_info)) {
			kfree(tb);
			return 0;
		}
#endif

		if (phy_id >= ab->num_radios) {
			ath12k_warn(ab, "Invalid phy_id %d\n", phy_id);
			kfree(tb);
			return -EINVAL;
		}

		ar = ab->pdevs[phy_id].ar;
		if (!ar) {
			ath12k_warn(ab, "ar is NULL for phy_id %d\n", phy_id);
			kfree(tb);
			return -EINVAL;
		}
		/* Reset to the previous country */
		if (ab->workqueue &&
		    test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags))
			queue_work(ab->workqueue, &ar->reg_set_previous_country);
		else
			ath12k_warn(ab, "Cannot queue work, workqueue unavailable or device not registered\n");

		kfree(tb);
		return -EINVAL;
	}

	memcpy(reg_info->alpha2, &ev->alpha2, REG_ALPHA2_LEN);

	reg_info->dfs_region = le32_to_cpu(ev->dfs_region);
	reg_info->phybitmap = le32_to_cpu(ev->phybitmap);
	reg_info->num_phy = le32_to_cpu(ev->num_phy);
	reg_info->phy_id = le32_to_cpu(ev->phy_id);
	reg_info->ctry_code = le32_to_cpu(ev->country_id);
	reg_info->reg_dmn_pair = le32_to_cpu(ev->domain_code);
	ath12k_reg_update_cached_country_regdomain(
		ab, reg_info, le32_to_cpu(ev->domain_code_6g_super_id));

	reg_info->is_ext_reg_event = true;

	reg_info->min_bw_2g = le32_to_cpu(ev->min_bw_2g);
	reg_info->max_bw_2g = le32_to_cpu(ev->max_bw_2g);
	reg_info->min_bw_5g = le32_to_cpu(ev->min_bw_5g);
	reg_info->max_bw_5g = le32_to_cpu(ev->max_bw_5g);
	reg_info->min_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->min_bw_6g_ap_lpi);
	reg_info->max_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->max_bw_6g_ap_lpi);
	reg_info->min_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->min_bw_6g_ap_sp);
	reg_info->max_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->max_bw_6g_ap_sp);
	reg_info->min_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->min_bw_6g_ap_vlp);
	reg_info->max_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->max_bw_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->min_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_lpi[i]);
		reg_info->max_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_lpi[i]);
		reg_info->min_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_sp[i]);
		reg_info->max_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_sp[i]);
		reg_info->min_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_vlp[i]);
		reg_info->max_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_vlp[i]);
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "%s:cc_ext %s dfs %d BW: min_2g %d max_2g %d min_5g %d max_5g %d phy_bitmap 0x%x",
		   __func__, reg_info->alpha2, reg_info->dfs_region,
		   reg_info->min_bw_2g, reg_info->max_bw_2g,
		   reg_info->min_bw_5g, reg_info->max_bw_5g,
		   reg_info->phybitmap);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_2g_reg_rules %d num_5g_reg_rules %d",
		   num_2g_reg_rules, num_5g_reg_rules);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_6g_reg_rules_ap_lpi: %d num_6g_reg_rules_ap_sp: %d num_6g_reg_rules_ap_vlp: %d",
		   num_6g_reg_rules_ap[WMI_REG_INDOOR_AP],
		   num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP],
		   num_6g_reg_rules_ap[WMI_REG_VLP_AP]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Regular client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_DEFAULT_CLIENT]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Subordinate client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_SUBORDINATE_CLIENT]);

	ext_wmi_reg_rule =
		(struct ath12k_wmi_reg_rule_ext_params *)((u8 *)ev
			+ sizeof(*ev)
			+ sizeof(struct wmi_tlv));

	if (num_2g_reg_rules) {
		reg_info->reg_rules_2g_ptr =
			create_ext_reg_rules_from_wmi(num_2g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_2g_ptr) {
			ath12k_warn(ab, "Unable to Allocate memory for 2g rules\n");
			goto err_free;
		}
	}

	ext_wmi_reg_rule += num_2g_reg_rules;

	/* Firmware might include 6 GHz reg rule in 5 GHz rule list
	 * for few countries along with separate 6 GHz rule.
	 * Having same 6 GHz reg rule in 5 GHz and 6 GHz rules list
	 * causes intersect check to be true, and same rules will be
	 * shown multiple times in iw cmd.
	 * Hence, avoid parsing 6 GHz rule from 5 GHz reg rule list
	 */
	num_invalid_5ghz_ext_rules = ath12k_wmi_ignore_num_extra_rules(ext_wmi_reg_rule,
								       num_5g_reg_rules);

	if (num_invalid_5ghz_ext_rules) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "CC: %s 5 GHz reg rules number %d from fw, %d number of invalid 5 GHz rules",
			   reg_info->alpha2, reg_info->num_5g_reg_rules,
			   num_invalid_5ghz_ext_rules);

		num_5g_reg_rules = num_5g_reg_rules - num_invalid_5ghz_ext_rules;
		reg_info->num_5g_reg_rules = num_5g_reg_rules;
	}

        skip_6g_rules_in_5g_rules = ath12k_invalid_5g_reg_ext_rules_from_wmi(num_5g_reg_rules,
                                                                             ext_wmi_reg_rule);

        if(skip_6g_rules_in_5g_rules) {
                ath12k_dbg(ab, ATH12K_DBG_WMI,
                           "CC: %s 5g reg rules number %d from fw, %d number of invalid 5g rules",
                           reg_info->alpha2, reg_info->num_5g_reg_rules,
                           skip_6g_rules_in_5g_rules);

                num_5g_reg_rules = num_5g_reg_rules - skip_6g_rules_in_5g_rules;
                reg_info->num_5g_reg_rules = num_5g_reg_rules;
        }

	if (num_5g_reg_rules) {
		reg_info->reg_rules_5g_ptr =
			create_ext_reg_rules_from_wmi(num_5g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_5g_ptr) {
			ath12k_warn(ab, "Unable to Allocate memory for 5g rules\n");
			goto err_free;
		}
	}


	/* We have adjusted the number of 5 GHz reg rules above. But still those
	 * many rules needs to be adjusted in ext_wmi_reg_rule.
	 *
	 * NOTE: num_invalid_5ghz_ext_rules will be 0 for rest other cases.
	 */

	ext_wmi_reg_rule += (num_5g_reg_rules + num_invalid_5ghz_ext_rules);

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_info->reg_rules_6g_ap_ptr[i] =
			create_ext_reg_rules_from_wmi(num_6g_reg_rules_ap[i],
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_6g_ap_ptr[i]) {
			ath12k_warn(ab, "Unable to Allocate memory for 6g ap rules\n");
			goto err_free;
		}

		ath12k_print_reg_rule(ab, ath12k_6g_ap_type_to_str(i),
                                     num_6g_reg_rules_ap[i],
                                     reg_info->reg_rules_6g_ap_ptr[i]);

		ext_wmi_reg_rule += num_6g_reg_rules_ap[i];
	}

	for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
                          "AP type %s", ath12k_6g_ap_type_to_str(j));

		for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
			reg_info->reg_rules_6g_client_ptr[j][i] =
				create_ext_reg_rules_from_wmi(num_6g_reg_rules_cl[j][i],
							      ext_wmi_reg_rule);

			if (!reg_info->reg_rules_6g_client_ptr[j][i]) {
				ath12k_warn(ab, "Unable to Allocate memory for 6g client rules\n");
				goto err_free;
			}

			ath12k_print_reg_rule(ab, ath12k_6g_client_type_to_str(i),
                                             num_6g_reg_rules_cl[j][i],
                                             reg_info->reg_rules_6g_client_ptr[j][i]);

			ext_wmi_reg_rule += num_6g_reg_rules_cl[j][i];
		}
	}

	reg_info->client_type = le32_to_cpu(ev->client_type);
	reg_info->rnr_tpe_usable = ev->rnr_tpe_usable;
	reg_info->unspecified_ap_usable = ev->unspecified_ap_usable;
	reg_info->domain_code_6g_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_lpi);
	reg_info->domain_code_6g_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_sp);
	reg_info->domain_code_6g_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->domain_code_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_lpi[i]);
		reg_info->domain_code_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_sp[i]);
		reg_info->domain_code_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_vlp[i]);
	}

	reg_info->domain_code_6g_super_id = le32_to_cpu(ev->domain_code_6g_super_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "6g client type %s 6g super domain %s",
                  ath12k_6g_client_type_to_str(reg_info->client_type),
                  ath12k_super_reg_6g_to_str(reg_info->domain_code_6g_super_id));

#ifdef CPTCFG_QCN_EXTN
	ath12k_reg_chan_list_cc_ext_parse_extn(ab, ext_wmi_reg_rule,
					       &reg_info->reg_info_extn);
#endif

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processed regulatory ext channel list\n");

	kfree(tb);
	return 0;

err_free:
	kfree(reg_info->reg_rules_2g_ptr);
	reg_info->reg_rules_2g_ptr = NULL;
	kfree(reg_info->reg_rules_5g_ptr);
	reg_info->reg_rules_5g_ptr = NULL;
	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		kfree(reg_info->reg_rules_6g_ap_ptr[i]);
		reg_info->reg_rules_6g_ap_ptr[i] = NULL;
		for (j = 0; j < WMI_REG_MAX_CLIENT_TYPE; j++) {
			kfree(reg_info->reg_rules_6g_client_ptr[i][j]);
			reg_info->reg_rules_6g_client_ptr[i][j] = NULL;
		}
	}
	kfree(tb);
	return -ENOMEM;
}

static int ath12k_pull_peer_del_resp_ev(struct ath12k_base *ab, struct sk_buff *skb,
					struct wmi_peer_delete_resp_event *peer_del_resp)
{
	const void **tb;
	const struct wmi_peer_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	memset(peer_del_resp, 0, sizeof(*peer_del_resp));

	peer_del_resp->vdev_id = ev->vdev_id;
	ether_addr_copy(peer_del_resp->peer_macaddr.addr,
			ev->peer_macaddr.addr);

	kfree(tb);
	return 0;
}

static int ath12k_pull_vdev_del_resp_ev(struct ath12k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_pull_bcn_tx_status_ev(struct ath12k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id, u32 *tx_status)
{
	const void **tb;
	const struct wmi_bcn_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch bcn tx status ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);
	*tx_status = le32_to_cpu(ev->tx_status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_vdev_stopped_param_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					      u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_stopped_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_STOPPED_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev stop ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_wmi_mgmt_rx_sub_tlv_parse(struct ath12k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;
	struct ath12k_mgmt_rx_cu_arg *rx_cu_params;
	struct ath12k_wmi_mgmt_rx_cu_params *rx_cu_params_tlv;

	switch (tag) {
	case WMI_TAG_MLO_MGMT_RX_CU_PARAMS:
		rx_cu_params = &parse->cu_params;
		rx_cu_params_tlv = (struct ath12k_wmi_mgmt_rx_cu_params *)ptr;
		rx_cu_params->cu_vdev_map[0] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_1, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[1] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_1, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[2] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_2, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[3] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_2, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[4] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_3, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[5] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_3, CU_VDEV_MAP_HB);
		rx_cu_params->cu_vdev_map[6] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_4, CU_VDEV_MAP_LB);
		rx_cu_params->cu_vdev_map[7] =
			le32_get_bits(rx_cu_params_tlv->cu_vdev_map_4, CU_VDEV_MAP_HB);
		parse->mgmt_ml_info_done = true;
		break;
	case WMI_TAG_MLO_LINK_REMOVAL_TBTT_COUNT:
		if (parse->num_link_removal_info_count >=
		    TARGET_NUM_VDEVS * ATH12K_WMI_MLO_MAX_LINKS)
			return -EINVAL;
		parse->link_removal_info[parse->num_link_removal_info_count] =
			(struct ath12k_wmi_mgmt_rx_mlo_link_removal_info *)ptr;
		parse->num_link_removal_info_count++;
		parse->parse_link_removal_info_done = true;
		break;
	case WMI_TAG_MLO_TID_TO_LINK_MAPPING_BCAST_T2LM_INFO:
		if (parse->num_bcast_ttlm_info_count < TARGET_NUM_VDEVS - 1) {
			parse->bcast_ttlm_info[parse->num_bcast_ttlm_info_count] =
				(struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info *)ptr;
			parse->num_bcast_ttlm_info_count++;
			parse->parse_bcast_ttlm_info_done = true;
		}
		break;
	}
	return 0;
}

static int ath12k_wmi_tlv_mgmt_rx_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_MGMT_RX_HDR:
		parse->fixed = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (!parse->frame_buf_done) {
			parse->frame_buf = ptr;
			parse->frame_buf_done = true;
		} else if (!parse->bpcc_buf_done) {
			if (len == 0)
				break;
			parse->cu_params.bpcc_bufp = (u8*)ptr;
			parse->bpcc_buf_done = true;
		}
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_mgmt_rx_sub_tlv_parse, parse);
		if (ret) {
			ath12k_warn(ab, "failed to parse mgmt rx sub tlv %d\n", ret);
			return ret;
		}
		break;
	}
	return 0;
}

static bool ath12k_get_ar_next_vdev_pos(struct ath12k *ar, u32 *pos)
{
	bool bit;
	u32 i = 0;

	for (i = *pos; i < ar->ab->num_max_vdev_supported; i++) {
		bit = ar->allocated_vdev_map & (1LL << i);
		if (bit) {
			*pos = i;
			return true;
		}
	}
	return false;
}

static void ath12k_update_cu_params(struct ath12k_base *org_ab,
				    struct ath12k_mgmt_rx_cu_arg *cu_params)
{
	struct ath12k_hw_group *ag = org_ab->ag;
	const u8 *bpcc_ptr, *bpcc_bufp;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	struct ath12k_pdev *pdev;
	u8 device_id, hw_link_id;
	struct ath12k_base *ab;
	u32 vdev_id, pos = 0;
	bool critical_flag;
	struct ath12k *ar;
	u16 vdev_map;
	int i, j;

	if (!cu_params->bpcc_bufp)
		return;

	/* Iterate over all the radios */
	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		ab = ath12k_ag_to_ab(ag, device_id);

		for (i = 0; i < ab->num_radios; i++) {
			pdev = rcu_dereference(ab->pdevs_active[i]);
			if (!pdev || !pdev->ar)
				continue;

			ar = pdev->ar;

			if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags))
				continue;

			pos = 0;
			hw_link_id = ar->hw_link_id;
			for (j = 0; j < ar->num_created_vdevs; j++) {
				if (!ath12k_get_ar_next_vdev_pos(ar, &pos)) {
					pos++;
					continue;
				}
				vdev_id = pos;
				pos++;

				spin_lock_bh(&ar->data_lock);
				list_for_each_entry(arvif, &ar->arvifs, list) {
					if (arvif->vdev_id != vdev_id ||
					    !arvif->is_up ||
					    ath12k_mac_is_bridge_vdev(arvif))
						continue;

					vif = arvif->ahvif->vif;
					if (!vif->valid_links)
						continue;

					vdev_map = cu_params->cu_vdev_map[hw_link_id];
					critical_flag = vdev_map & (1 << vdev_id);
					bpcc_bufp = cu_params->bpcc_bufp;
					bpcc_ptr = bpcc_bufp + vdev_id +
					    (hw_link_id * MAX_AP_MLDS_PER_LINK);

					ieee80211_critical_update(vif,
								  arvif->link_id,
								  critical_flag,
								  *bpcc_ptr);
					break;
				}
				spin_unlock_bh(&ar->data_lock);
			}
		}
	}
}

static void
ath12k_update_link_removal_params(struct ath12k_base *ab,
				  const struct ath12k_mgmt_rx_mlo_link_removal_info *params,
				  u32 num_link_removal_params)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	struct ath12k_hw *ah;
	struct ath12k *ar, *target_ar = NULL;
	struct ath12k_link_vif *arvif;
	const struct ath12k_mgmt_rx_mlo_link_removal_info *info;
	u32 i, j, num_hw;

	/**
	 * If a broadcast probe request is received on a given pdev, FW sends MLO link
	 * removal information for all AP MLDs which satisfy both conditions below
	 *      1) The AP MLD has one APs being removed.
	 *      2) The AP MLD has a BSS on the pdev on which the broadcast probe request is received
	 */
	for (i = 0; i < num_link_removal_params; i++) {
		info = &params[i];

		if (info->hw_link_id >= ATH12K_GROUP_MAX_RADIO) {
			ath12k_warn(ab, "Wrong hw_link_id received:%d\n",
				    info->hw_link_id);
			continue;
		}

		for (num_hw = 0; num_hw < ag->num_hw; num_hw++) {
			ah = ath12k_ag_to_ah(ag, num_hw);
			if (!ah)
				continue;
			for_each_ar(ah, ar, j) {
				if (ar->hw_link_id == info->hw_link_id)
					target_ar = ar;
			}
		}
		if (!target_ar) {
			ath12k_warn(ab, "Couldn't fetch hw links for hw_link_id:%d\n",
				    info->hw_link_id);
			continue;
		}

		arvif = ath12k_mac_get_arvif(target_ar, info->vdev_id);
		if (!arvif) {
			ath12k_err(ab, "Error in getting arvif from vdev id:%d info link:%d\n",
				   info->vdev_id, info->hw_link_id);
			continue;
		}

		/* update mac80211 only if tbtt_count is greater than 0 */
		if (arvif->is_up && arvif->ahvif->vif->valid_links && info->tbtt_count)
			ieee80211_link_removal_count_update(arvif->ahvif->vif,
							    arvif->link_id,
							    info->tbtt_count);
	}
}

static void ath12k_wmi_update_ml_link_removal_info_count(struct ath12k_base *ab,
							 struct ath12k_wmi_mgmt_rx_arg *hdr,
							 struct wmi_tlv_mgmt_rx_parse *parse)
{
	u32 tbtt_val;
	int idx;

	hdr->num_link_removal_info = parse->num_link_removal_info_count;

	for (idx = 0; idx < hdr->num_link_removal_info; idx++) {
		tbtt_val = le32_to_cpu(parse->link_removal_info[idx]->tbtt_info);

		hdr->link_removal_info[idx].vdev_id =
			le16_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_VDEV_ID_GET);
		hdr->link_removal_info[idx].hw_link_id =
			le16_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_HW_LINK_ID_GET);
		hdr->link_removal_info[idx].tbtt_count =
			le32_get_bits(tbtt_val,
				      WMI_MGMT_RX_MLO_LINK_REMOVAL_INFO_TBTT_COUNT_GET);
	}
}

static void ath12k_wmi_update_ml_bcast_ttlm_info_params(struct ath12k_base *ab,
							struct ath12k_wmi_mgmt_rx_arg *hdr,
							struct wmi_tlv_mgmt_rx_parse *parse)
{
	struct ath12k_wmi_mgmt_rx_mlo_bcast_ttlm_info *ttlm;
	int idx;

	hdr->num_bcast_ttlm_info = parse->num_bcast_ttlm_info_count;

	for (idx = 0; idx < hdr->num_bcast_ttlm_info; idx++) {
		ttlm = parse->bcast_ttlm_info[idx];

		hdr->bcast_ttlm_info[idx].vdev_id =
			le32_get_bits(ttlm->ttlm_info,
				      WMI_MGMT_RX_MLO_BCAST_TTLM_INFO_VDEV_ID_GET);
		hdr->bcast_ttlm_info[idx].expec_dur =
			le32_get_bits(ttlm->ttlm_info,
				      WMI_MGMT_RX_MLO_BCAST_TTLM_INFO_EXPEC_DUR_GET);
	}
}

static int ath12k_pull_mgmt_rx_params_tlv(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  struct ath12k_wmi_mgmt_rx_arg *hdr)
{
	struct wmi_tlv_mgmt_rx_parse parse = { };
	const struct ath12k_wmi_mgmt_rx_params *ev;
	const u8 *frame;
	int i, ret;

	memset(&parse, 0, sizeof(parse));

	parse.link_removal_info = kcalloc(TARGET_NUM_VDEVS * ATH12K_WMI_MLO_MAX_LINKS,
					  sizeof(*parse.link_removal_info), GFP_ATOMIC);
	if (!parse.link_removal_info)
		return -ENOMEM;

	parse.bcast_ttlm_info = kcalloc(TARGET_NUM_VDEVS - 1,
					sizeof(*parse.bcast_ttlm_info), GFP_ATOMIC);
	if (!parse.bcast_ttlm_info) {
		kfree(parse.link_removal_info);
		return -ENOMEM;
	}

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mgmt_rx_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse mgmt rx tlv %d\n", ret);
		kfree(parse.link_removal_info);
		kfree(parse.bcast_ttlm_info);
		return ret;
	}

	ev = parse.fixed;
	frame = parse.frame_buf;

	if (!ev || !frame) {
		ath12k_warn(ab, "failed to fetch mgmt rx hdr");
		kfree(parse.link_removal_info);
		kfree(parse.bcast_ttlm_info);
		return -EPROTO;
	}

	hdr->pdev_id = le32_to_cpu(ev->pdev_id);
	hdr->chan_freq = le32_to_cpu(ev->chan_freq);
	hdr->channel = le32_to_cpu(ev->channel);
	hdr->snr = le32_to_cpu(ev->snr);
	hdr->rate = le32_to_cpu(ev->rate);
	hdr->phy_mode = le32_to_cpu(ev->phy_mode);
	hdr->buf_len = le32_to_cpu(ev->buf_len);
	hdr->status = le32_to_cpu(ev->status);
	hdr->flags = le32_to_cpu(ev->flags);
	hdr->rssi = a_sle32_to_cpu(ev->rssi);
	hdr->tsf_delta = le32_to_cpu(ev->tsf_delta);

	for (i = 0; i < ATH_MAX_ANTENNA; i++)
		hdr->rssi_ctl[i] = le32_to_cpu(ev->rssi_ctl[i]);

	if (parse.mgmt_ml_info_done) {
		rcu_read_lock();
		ath12k_update_cu_params(ab, &parse.cu_params);
		rcu_read_unlock();
	}

	if (skb->len < (frame - skb->data) + hdr->buf_len) {
		ath12k_warn(ab, "invalid length in mgmt rx hdr ev");
		kfree(parse.link_removal_info);
		kfree(parse.bcast_ttlm_info);
		return -EPROTO;
	}

	/* ML link removal info TLV */
	if (parse.num_link_removal_info_count)
		ath12k_wmi_update_ml_link_removal_info_count(ab, hdr, &parse);

	/* Bcast TTLM info TLV */
	if (parse.num_bcast_ttlm_info_count)
		ath12k_wmi_update_ml_bcast_ttlm_info_params(ab, hdr, &parse);

	/* shift the sk_buff to point to `frame` */
	skb_trim(skb, 0);
	skb_put(skb, frame - skb->data);
	skb_pull(skb, frame - skb->data);
	skb_put(skb, hdr->buf_len);

	kfree(parse.link_removal_info);
	kfree(parse.bcast_ttlm_info);

	return 0;
}

static int wmi_process_mgmt_tx_comp(struct ath12k *ar, u32 desc_id,
				    u32 status,
				    u32 ack_rssi)
{
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_hdr *hdr;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	u16 frm_stype;
	int num_mgmt;

	spin_lock_bh(&ar->data_lock);
	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, desc_id);

	if (!msdu) {
		ath12k_warn(ar->ab, "received mgmt tx compl for invalid msdu_id: %d\n",
			    desc_id);
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		spin_unlock_bh(&ar->data_lock);
		return -ENOENT;
	}

	idr_remove(&ar->txmgmt_idr, desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH12K_SKB_CB(msdu);
	ath12k_core_dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	hdr = (struct ieee80211_hdr *)msdu->data;
	if (skb_cb->flags & ATH12K_SKB_DEAUTH_DISASSOC_TRACKED) {
		ath12k_peer_deauth_disassoc_tx_dec(ar, hdr->addr1);
		skb_cb->flags &= ~ATH12K_SKB_DEAUTH_DISASSOC_TRACKED;
	}

	if (ieee80211_is_mgmt(hdr->frame_control)) {
		frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, hdr->frame_control);
		vif = skb_cb->vif;
		if (frm_stype >= ARRAY_SIZE(mgmt_frame_name)) {
			ath12k_warn(ar->ab,
				    "wmi mgmt tx compl: unknown frame subtype %u\n",
				    frm_stype);
			goto skip_mgmt_stats;
		}
		if (ATH12K_MGMT_MLME_FRAME(hdr->frame_control))
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MLME, ATH12K_DBG_L0,
					 "Tx completion for %s frame to STA %pM status %u\n",
					 mgmt_frame_name[frm_stype], hdr->addr1, status);

		if (!vif) {
			ath12k_warn(ar->ab, "failed to find vif to update txcompl mgmt stats\n");
			goto skip_mgmt_stats;
		}

	        ahvif = ath12k_vif_to_ahvif(vif);
		mgmt_stats = &ahvif->mgmt_stats;

		if (!status)
			mgmt_stats->tx_compl_succ[frm_stype]++;
		else
			mgmt_stats->tx_compl_fail[frm_stype]++;

		if (!status) {
			mgmt_stats->tx_cmpl_succ_pkts++;
			mgmt_stats->tx_cmpl_succ_bytes += msdu->len;
		} else if (status == 3) {
			mgmt_stats->tx_cmpl_retry_pkts++;
			mgmt_stats->tx_cmpl_retry_bytes += msdu->len;
		} else {
			mgmt_stats->tx_cmpl_err_pkts++;
			mgmt_stats->tx_cmpl_err_bytes += msdu->len;
		}
	}

skip_mgmt_stats:
	spin_unlock_bh(&ar->data_lock);

	/* Handle custom tx packet before memsetting
	 * skb_cb via info.
	 */
#ifdef CPTCFG_QCN_EXTN
	if (ATH12K_IS_CUSTOM_PKT(skb_cb)) {
		ath12k_custom_tx_free_extn(msdu, status);
		goto skip_tx_status;
	}
#endif /* CPTCFG_QCN_EXTN */

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	info->status.rates[0].idx = -1;

	if ((!(info->flags & IEEE80211_TX_CTL_NO_ACK)) && !status) {
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ack_rssi;
		info->status.flags |= IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if ((info->flags & IEEE80211_TX_CTL_NO_ACK) && !status)
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	ieee80211_tx_status_irqsafe(ath12k_ar_to_hw(ar), msdu);

#ifdef CPTCFG_QCN_EXTN
skip_tx_status:
#endif /* CPTCFG_QCN_EXTN */
	num_mgmt = atomic_dec_if_positive(&ar->num_pending_mgmt_tx);

	/* WARN when we received this event without doing any mgmt tx */
	if (num_mgmt < 0)
		WARN_ON_ONCE(1);

	if (!num_mgmt)
		wake_up(&ar->txmgmt_empty_waitq);

	return 0;
}

static int ath12k_pull_mgmt_tx_compl_param_tlv(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       struct wmi_mgmt_tx_compl_event *param)
{
	const void **tb;
	const struct wmi_mgmt_tx_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_MGMT_TX_COMPL_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch mgmt tx compl ev");
		kfree(tb);
		return -EPROTO;
	}

	param->pdev_id = ev->pdev_id;
	param->desc_id = ev->desc_id;
	param->status = ev->status;
	param->ppdu_id = ev->ppdu_id;
	param->ack_rssi = ev->ack_rssi;

	kfree(tb);
	return 0;
}

static void wmi_process_offchan_tx_comp(struct ath12k *ar, u32 desc_id,
					u32 status)
{
	struct sk_buff *msdu;
	struct ath12k_skb_cb *skb_cb;
	struct ieee80211_tx_info *info;
	struct ieee80211_hdr *hdr;

	spin_lock_bh(&ar->data_lock);
	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, desc_id);

	if (!msdu) {
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		spin_unlock_bh(&ar->data_lock);
		ath12k_warn(ar->ab, "received offchan tx compl for invalid msdu_id: %d\n",
			    desc_id);
		return;
	}

	idr_remove(&ar->txmgmt_idr, desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH12K_SKB_CB(msdu);
	hdr = (struct ieee80211_hdr *)msdu->data;
	dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);
	if (skb_cb->flags & ATH12K_SKB_DEAUTH_DISASSOC_TRACKED) {
		ath12k_peer_deauth_disassoc_tx_dec(ar, hdr->addr1);
		skb_cb->flags &= ~ATH12K_SKB_DEAUTH_DISASSOC_TRACKED;
	}

	spin_unlock_bh(&ar->data_lock);

	info = IEEE80211_SKB_CB(msdu);
	if (!(info->flags & IEEE80211_TX_CTL_NO_ACK) && !status)
		info->flags |= IEEE80211_TX_STAT_ACK;

#ifdef CPTCFG_QCN_EXTN
	if (!ATH12K_IS_CUSTOM_PKT(skb_cb)) {
		ieee80211_tx_status_irqsafe(ar->ah->hw, msdu);
	} else {
		ath12k_custom_tx_free_extn(msdu, status);
	}
#else
	ieee80211_tx_status_irqsafe(ar->ah->hw, msdu);
#endif /* CPTCFG_QCN_EXTN */
}

static int ath12k_pull_offchan_tx_compl_param_tlv(struct ath12k_base *ab,
						  struct sk_buff *skb,
						  struct wmi_offchan_data_tx_compl_event
						  *params)
{
	const void **tb;
	const struct wmi_offchan_data_tx_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFCHAN_DATA_TX_COMPL_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch offchan tx compl ev\n");
		kfree(tb);
		return -EPROTO;
	}

	*params = *ev;

	kfree(tb);
	return 0;
}

static void ath12k_wmi_event_scan_started(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan started event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		ar->scan.state = ATH12K_SCAN_RUNNING;

		if (ar->scan.is_roc)
			ieee80211_ready_on_channel(ath12k_ar_to_hw(ar));

		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "scan: [radio:%d] state -> RUNNING (WMI STARTED event) [%llu ms]\n",
			   ar->radio_idx, ktime_to_ms(ktime_get()));

		complete(&ar->scan.started);
		break;
	}
}

static void ath12k_wmi_event_scan_start_failed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan start failed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		complete(&ar->scan.started);
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_completed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		/* One suspected reason scan can be completed while starting is
		 * if firmware fails to deliver all scan events to the host,
		 * e.g. when transport pipe is full. This has been observed
		 * with spectral scan phyerr events starving wmi transport
		 * pipe. In such case the "scan completed" event should be (and
		 * is) ignored by the host as it may be just firmware's scan
		 * state machine recovering.
		 */
		ath12k_warn(ar->ab, "received scan completed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "scan: [radio:%d] COMPLETED (WMI COMPLETED event) [%llu ms]\n",
			   ar->radio_idx, ktime_to_ms(ktime_get()));
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_bss_chan(struct ath12k *ar, u32 scan_id, u32 freq)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan bss chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		/*In order to support off channel tx/rx along with off channel scan,
		 * scan vdev is created and started to do the tx. once we start the scan
		 * driver will send the ROC scan request to FW. Due to this, the first
		 * scan request is not treated as off channel scan in FW. So it
		 * sends bss scan event instead of foreign channel scan
		 */
		if (scan_id ==  ATH12K_ROC_SCAN_ID) {
			ar->scan_channel = ieee80211_get_channel(ar->ah->hw->wiphy,
								 freq);
			if (ar->scan.is_roc && ar->scan.roc_freq == freq)
				complete(&ar->scan.on_channel);
		} else {
			ar->scan_channel = NULL;
		}
		break;
	}
}

static void ath12k_wmi_event_scan_foreign_chan(struct ath12k *ar, u32 freq)
{
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);

	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan foreign chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ar->scan_channel = ieee80211_get_channel(hw->wiphy, freq);

		if (ar->scan.is_roc && ar->scan.roc_freq == freq)
			complete(&ar->scan.on_channel);

		break;
	}
}

static const char *
ath12k_wmi_event_scan_type_str(enum wmi_scan_event_type type,
			       enum wmi_scan_completion_reason reason)
{
	switch (type) {
	case WMI_SCAN_EVENT_STARTED:
		return "started";
	case WMI_SCAN_EVENT_COMPLETED:
		switch (reason) {
		case WMI_SCAN_REASON_COMPLETED:
			return "completed";
		case WMI_SCAN_REASON_CANCELLED:
			return "completed [cancelled]";
		case WMI_SCAN_REASON_PREEMPTED:
			return "completed [preempted]";
		case WMI_SCAN_REASON_TIMEDOUT:
			return "completed [timedout]";
		case WMI_SCAN_REASON_INTERNAL_FAILURE:
			return "completed [internal err]";
		case WMI_SCAN_REASON_MAX:
			break;
		}
		return "completed [unknown]";
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		return "bss channel";
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		return "foreign channel";
	case WMI_SCAN_EVENT_DEQUEUED:
		return "dequeued";
	case WMI_SCAN_EVENT_PREEMPTED:
		return "preempted";
	case WMI_SCAN_EVENT_START_FAILED:
		return "start failed";
	case WMI_SCAN_EVENT_RESTARTED:
		return "restarted";
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
		return "foreign channel exit";
	default:
		return "unknown";
	}
}

static int ath12k_pull_scan_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_scan_event *scan_evt_param)
{
	const void **tb;
	const struct wmi_scan_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_SCAN_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch scan ev");
		kfree(tb);
		return -EPROTO;
	}

	scan_evt_param->event_type = ev->event_type;
	scan_evt_param->reason = ev->reason;
	scan_evt_param->channel_freq = ev->channel_freq;
	scan_evt_param->scan_req_id = ev->scan_req_id;
	scan_evt_param->scan_id = ev->scan_id;
	scan_evt_param->vdev_id = ev->vdev_id;
	scan_evt_param->tsf_timestamp = ev->tsf_timestamp;

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_sta_kickout_ev(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_peer_sta_kickout_arg *arg)
{
	const void **tb;
	const struct wmi_peer_sta_kickout_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_STA_KICKOUT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer sta kickout ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->mac_addr = ev->peer_macaddr.addr;
	arg->reason = ev->reason;
	arg->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int ath12k_pull_roam_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_roam_event *roam_ev)
{
	const void **tb;
	const struct wmi_roam_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_ROAM_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch roam ev");
		kfree(tb);
		return -EPROTO;
	}

	roam_ev->vdev_id = ev->vdev_id;
	roam_ev->reason = ev->reason;
	roam_ev->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int freq_to_idx(struct ath12k *ar, int freq)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	int band, ch, idx = 0;

	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
		if (!ar->mac.sbands[band].channels)
			continue;

		sband = hw->wiphy->bands[band];
		if (!sband)
			continue;

		for (ch = 0; ch < sband->n_channels; ch++, idx++)
			if (sband->channels[ch].center_freq == freq)
				goto exit;
	}

exit:
	return idx;
}

static int ath12k_pull_chan_info_ev(struct ath12k_base *ab, struct sk_buff *skb,
				    struct wmi_chan_info_event *ch_info_ev)
{
	const void **tb;
	const struct wmi_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	ch_info_ev->err_code = ev->err_code;
	ch_info_ev->freq = ev->freq;
	ch_info_ev->cmd_flags = ev->cmd_flags;
	ch_info_ev->noise_floor = ev->noise_floor;
	ch_info_ev->rx_clear_count = ev->rx_clear_count;
	ch_info_ev->cycle_count = ev->cycle_count;
	ch_info_ev->chan_tx_pwr_range = ev->chan_tx_pwr_range;
	ch_info_ev->chan_tx_pwr_tp = ev->chan_tx_pwr_tp;
	ch_info_ev->rx_frame_count = ev->rx_frame_count;
	ch_info_ev->tx_frame_cnt = ev->tx_frame_cnt;
	ch_info_ev->mac_clk_mhz = ev->mac_clk_mhz;
	ch_info_ev->vdev_id = ev->vdev_id;

	kfree(tb);
	return 0;
}

static int
ath12k_pull_pdev_bss_chan_info_ev(struct ath12k_base *ab, struct sk_buff *skb,
				  struct wmi_pdev_bss_chan_info_event *bss_ch_info_ev)
{
	const void **tb;
	const struct wmi_pdev_bss_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev bss chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	bss_ch_info_ev->pdev_id = ev->pdev_id;
	bss_ch_info_ev->freq = ev->freq;
	bss_ch_info_ev->noise_floor = ev->noise_floor;
	bss_ch_info_ev->rx_clear_count_low = ev->rx_clear_count_low;
	bss_ch_info_ev->rx_clear_count_high = ev->rx_clear_count_high;
	bss_ch_info_ev->cycle_count_low = ev->cycle_count_low;
	bss_ch_info_ev->cycle_count_high = ev->cycle_count_high;
	bss_ch_info_ev->tx_cycle_count_low = ev->tx_cycle_count_low;
	bss_ch_info_ev->tx_cycle_count_high = ev->tx_cycle_count_high;
	bss_ch_info_ev->rx_cycle_count_low = ev->rx_cycle_count_low;
	bss_ch_info_ev->rx_cycle_count_high = ev->rx_cycle_count_high;
	bss_ch_info_ev->rx_bss_cycle_count_low = ev->rx_bss_cycle_count_low;
	bss_ch_info_ev->rx_bss_cycle_count_high = ev->rx_bss_cycle_count_high;

	kfree(tb);
	return 0;
}

static int
ath12k_pull_vdev_install_key_compl_ev(struct ath12k_base *ab, struct sk_buff *skb,
				      struct wmi_vdev_install_key_complete_arg *arg)
{
	const void **tb;
	const struct wmi_vdev_install_key_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev install key compl ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = le32_to_cpu(ev->vdev_id);
	ether_addr_copy(arg->macaddr, ev->peer_macaddr.addr);
	arg->key_idx = le32_to_cpu(ev->key_idx);
	arg->key_flags = le32_to_cpu(ev->key_flags);
	arg->status = le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_assoc_conf_ev(struct ath12k_base *ab, struct sk_buff *skb,
					  struct wmi_peer_assoc_conf_arg *peer_assoc_conf)
{
	const void **tb;
	const struct wmi_peer_assoc_conf_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_ASSOC_CONF_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer assoc conf ev");
		kfree(tb);
		return -EPROTO;
	}

	peer_assoc_conf->vdev_id = le32_to_cpu(ev->vdev_id);
	ether_addr_copy(peer_assoc_conf->macaddr, ev->peer_macaddr.addr);
	peer_assoc_conf->status = le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static void ath12k_wmi_op_ep_tx_credits(struct ath12k_base *ab)
{
	/* try to send pending beacons first. they take priority */
	wake_up(&ab->wmi_ab.tx_credits_wq);
}

static int ath12k_reg_11d_new_cc_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_11d_new_cc_event *ev;
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	const void **tb;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_11D_NEW_COUNTRY_EVENT];
	if (!ev) {
		kfree(tb);
		ath12k_warn(ab, "failed to fetch 11d new cc ev");
		return -EPROTO;
	}

	spin_lock_bh(&ab->base_lock);
	memcpy(&ab->new_alpha2, &ev->new_alpha2, REG_ALPHA2_LEN);
	spin_unlock_bh(&ab->base_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi 11d new cc %c%c\n",
		   ab->new_alpha2[0],
		   ab->new_alpha2[1]);

	kfree(tb);

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;
		ar->state_11d = ATH12K_11D_IDLE;
		complete(&ar->completed_11d_scan);
	}

	queue_work(ab->workqueue, &ab->update_11d_work);

	return 0;
}

static void ath12k_wmi_htc_tx_complete(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct ath12k_wmi_pdev *wmi = NULL;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_cmd_id id;
	u32 i;
	u8 wmi_ep_count;
	u8 eid;

	eid = skb_cb->u.eid;
	skb_pull(skb, sizeof(struct ath12k_htc_hdr));
	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = le32_get_bits(cmd_hdr->cmd_id, WMI_CMD_HDR_CMD_ID);
	dev_kfree_skb(skb);

	if (eid >= ATH12K_HTC_EP_COUNT)
		return;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params->max_radios)
		return;

	for (i = 0; i < ab->htc.wmi_ep_count; i++) {
		if (ab->wmi_ab.wmi[i].eid == eid) {
			wmi = &ab->wmi_ab.wmi[i];
			break;
		}
	}

	if (wmi) {
		WMI_COMMAND_TX_CMP_RECORD(wmi, id);
		wake_up(&wmi->tx_ce_desc_wq);
	}
}

static inline bool
ath12k_is_regd_alpha2_global_mode(const struct ieee80211_regdomain *regd)
{
	if (!regd)
		return false;

	return !strncmp(regd->alpha2, "00", 2);
}

static int ath12k_reg_handle_chan_list(struct ath12k_base *ab,
				       struct ath12k_reg_info *reg_info,
				       enum ieee80211_ap_reg_power power_type)
{
	const struct ieee80211_regdomain *wiphy_regd = NULL;
	struct ieee80211_regdomain *regd;
	int pdev_idx;
	struct ath12k *ar;

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS) {
		/* In case of failure to set the requested ctry,
		 * fw retains the current regd. We print a failure info
		 * and return from here.
		 */
		ath12k_warn(ab, "Failed to set the requested Country regulatory setting\n");
		return -EINVAL;
	}

	pdev_idx = reg_info->phy_id;

	if (pdev_idx >= ab->num_radios) {
		/* Process the event for phy0 only if single_pdev_only
		 * is true. If pdev_idx is valid but not 0, discard the
		 * event. Otherwise, it goes to fallback.
		 */
		if (ab->hw_params->single_pdev_only &&
		    pdev_idx < ab->hw_params->num_rxdma_per_pdev)
			goto retfail;
		else
			goto fallback;
	}

	ar = ab->pdevs[pdev_idx].ar;
	rcu_read_lock();
	if (ar)
		wiphy_regd = rcu_dereference(ar->ah->hw->wiphy->regd);

	/* Avoid multiple overwrites to default regd, during core
	 * stop-start after mac registration. Also, add an exception when
	 * the current regd in wiphy is 00.
	 */
	if (!ath12k_is_regd_alpha2_global_mode(wiphy_regd) &&
	    ab->default_regd[pdev_idx] && !ab->new_regd[pdev_idx] &&
	    !memcmp(ab->default_regd[pdev_idx]->alpha2,
		    reg_info->alpha2, 2)) {
		rcu_read_unlock();
		goto retfail;
	}

	rcu_read_unlock();

	regd = ath12k_reg_build_regd(ab, reg_info);
	if (!regd) {
		ath12k_warn(ab, "failed to build regd from reg_info\n");
		goto fallback;
	}

	if (ar && ar->supports_6ghz) {
		ath12k_dbg(ab, ATH12K_DBG_REG,
			   "Freeing AFC info for pdev %d\n", pdev_idx);
		spin_lock_bh(&ar->data_lock);
		ath12k_free_afc_power_event_info(&ar->afc);
		spin_unlock_bh(&ar->data_lock);
	}

	spin_lock_bh(&ab->base_lock);
	if (test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags)) {
		/* Once mac is registered, ar is valid and all CC events from
		 * fw is considered to be received due to user requests
		 * currently.
		 * Free previously built regd before assigning the newly
		 * generated regd to ar. NULL pointer handling will be
		 * taken care by kfree itself.
		 */
		kfree(ab->new_regd[pdev_idx]);
		ab->new_regd[pdev_idx] = regd;
		queue_work(ab->workqueue, &ar->regd_update_work);
	} else {
		/* Multiple events for the same *ar is not expected. But we
		 * can still clear any previously stored default_regd if we
		 * are receiving this event for the same radio by mistake.
		 * NULL pointer handling will be taken care by kfree itself.
		 */
		kfree(ab->default_regd[pdev_idx]);
		/* This regd would be applied during mac registration */
		ab->default_regd[pdev_idx] = regd;
	}

	ab->regd_freed = false;
	ab->dfs_region = reg_info->dfs_region;
	spin_unlock_bh(&ab->base_lock);

#ifdef CPTCFG_QCN_EXTN
	ath12k_reg_handle_chan_list_extn(ab, pdev_idx,
					 &reg_info->reg_info_extn);
#endif

	return 0;

fallback:
	/* Fallback to older reg (by sending previous country setting
	 * again if fw has succeeded and we failed to process here.
	 * The Regdomain should be uniform across driver and fw. Since the
	 * FW has processed the command and sent a success status, we expect
	 * this function to succeed as well. If it doesn't, CTRY needs to be
	 * reverted at the fw and the old SCAN_CHAN_LIST cmd needs to be sent.
	 */
	/* TODO: This is rare, but still should also be handled */
	WARN_ON(1);
retfail:
        return -EINVAL;
}

static int ath12k_reg_chan_list_event(struct ath12k_base *ab, struct sk_buff *skb)
{
       struct ath12k_reg_info *reg_info;
       int ret, i, j;

       reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
       if (!reg_info)
               return -ENOMEM;

       ret = ath12k_pull_reg_chan_list_ext_update_ev(ab, skb, reg_info);
       if (ret) {
               ath12k_warn(ab, "failed to extract regulatory info from received event\n");
               goto mem_free;
       }

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_wmi_handle_reg_chan_list_hwbl_extn(ab, &reg_info)) {
		ret = 0;
		goto mem_free;
	}
#endif

       ret = ath12k_reg_handle_chan_list(ab, reg_info, IEEE80211_REG_UNSET_AP);
       if (ret) {
               ath12k_warn(ab, "failed to process regulatory info from received event\n");
               goto mem_free;
       }
mem_free:
	if (reg_info) {
		kfree(reg_info->reg_rules_2g_ptr);
		kfree(reg_info->reg_rules_5g_ptr);
		if (reg_info->is_ext_reg_event) {
			for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
				kfree(reg_info->reg_rules_6g_ap_ptr[i]);
				for (j = 0; j < WMI_REG_MAX_CLIENT_TYPE; j++)
                                	kfree(reg_info->reg_rules_6g_client_ptr[i][j]);
                       }
		}
#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_free_reg_hw_blocklist_extn(&reg_info->reg_info_extn);
#endif
		kfree(reg_info);
	}
	return ret;
}

static int ath12k_wmi_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data)
{
	struct ath12k_wmi_rdy_parse *rdy_parse = data;
	struct wmi_ready_event fixed_param;
	struct ath12k_wmi_mac_addr_params *addr_list;
	struct ath12k_pdev *pdev;
	u32 num_mac_addr;
	int i;

	switch (tag) {
	case WMI_TAG_READY_EVENT:
		memset(&fixed_param, 0, sizeof(fixed_param));
		memcpy(&fixed_param, (struct wmi_ready_event *)ptr,
		       min_t(u16, sizeof(fixed_param), len));
		ab->wlan_init_status = le32_to_cpu(fixed_param.ready_event_min.status);
		rdy_parse->num_extra_mac_addr =
			le32_to_cpu(fixed_param.ready_event_min.num_extra_mac_addr);

		ether_addr_copy(ab->mac_addr,
				fixed_param.ready_event_min.mac_addr.addr);
		ab->pktlog_defs_checksum = le32_to_cpu(fixed_param.pktlog_defs_checksum);
		ab->wmi_ready = true;
		ab->max_ml_peer_supported = fixed_param.max_num_ml_peers;
		ab->fw_max_peers =
			le32_to_cpu(fixed_param.ready_event_min.num_total_peers);
		break;
	case WMI_TAG_ARRAY_FIXED_STRUCT:
		addr_list = (struct ath12k_wmi_mac_addr_params *)ptr;
		num_mac_addr = rdy_parse->num_extra_mac_addr;

		if (!(ab->num_radios > 1 && num_mac_addr >= ab->num_radios))
			break;

		if (len < ab->num_radios * sizeof(*addr_list))
			return -EINVAL;

		for (i = 0; i < ab->num_radios; i++) {
			pdev = &ab->pdevs[i];
			ether_addr_copy(pdev->mac_addr, addr_list[i].addr);
		}
		ab->pdevs_macaddr_valid = true;
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_rdy_parse rdy_parse = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rdy_parse, &rdy_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	complete(&ab->wmi_ab.unified_ready);
	return 0;
}

static void ath12k_peer_delete_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_delete_resp_event peer_del_resp;
	struct ath12k *ar;
	bool was_mlo = false;

	if (ath12k_pull_peer_del_resp_ev(ab, skb, &peer_del_resp) != 0) {
		ath12k_warn(ab, "failed to extract peer delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(peer_del_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer delete resp ev %d",
			    peer_del_resp.vdev_id);
		rcu_read_unlock();
		ath12k_critical_failure_trigger(ab, ATH12K_CRIT_PEER_FAILURE);
		return;
	}

	/* Remove peer from deletion tracker */
	if (ar->pdev->peer_del_tracker) {
		/* Check if this is an MLO peer before removing the entry;
		 * the entry is freed during removal and cannot be queried
		 * afterwards.
		 */
		was_mlo = ath12k_peer_del_tracker_is_mlo(ar->pdev,
							 le32_to_cpu(peer_del_resp.vdev_id),
							 peer_del_resp.peer_macaddr.addr);
		ath12k_peer_del_tracker_remove(ar->pdev,
					       le32_to_cpu(peer_del_resp.vdev_id),
					       peer_del_resp.peer_macaddr.addr);
	}

	rcu_read_unlock();

	ath12k_dbg_level(ab, ATH12K_DBG_PEER | ATH12K_DBG_MLME, ATH12K_DBG_L1,
			 "peer delete resp for vdev id %d addr %pM\n",
			 peer_del_resp.vdev_id, peer_del_resp.peer_macaddr.addr);

	if (was_mlo) {
		struct ath12k_hw_group *ag = ab->ag;

		ag->mlo_peer_count--;
		ath12k_dbg(ab, ATH12K_DBG_PEER | ATH12K_DBG_WSI_BYPASS,
			   "MLO peer del resp %pM mlo_peer_count=%u\n",
			   peer_del_resp.peer_macaddr.addr, ag->mlo_peer_count);
		if (!ag->mlo_peer_count)
			complete(&ag->peer_cleanup_complete);
	}
}

static void ath12k_vdev_delete_resp_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_del_resp_ev(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev delete resp ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_delete_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev delete resp for vdev id %d\n",
		   vdev_id);
}

static const char *ath12k_wmi_vdev_resp_print(u32 vdev_resp_status)
{
	switch (vdev_resp_status) {
	case WMI_VDEV_START_RESPONSE_INVALID_VDEVID:
		return "invalid vdev id";
	case WMI_VDEV_START_RESPONSE_NOT_SUPPORTED:
		return "not supported";
	case WMI_VDEV_START_RESPONSE_DFS_VIOLATION:
		return "dfs violation";
	case WMI_VDEV_START_RESPONSE_INVALID_REGDOMAIN:
		return "invalid regdomain";
	case WMI_VDEV_START_RESPONSE_INVALID_BAND:
		return "invalid band";
	case WMI_VDEV_START_RESPONSE_INVALID_PREFERRED_TX_RX_STREAMS:
		return "invalid preferred tx/rx streams";
	case WMI_VDEV_START_RESPONSE_INVALID_TX_VAP_CONFIG:
		return "invalid tx vap config";
	case WMI_VDEV_START_RESPONSE_BSS_PEER_NOT_FOUND:
		return "bss peer not found";
	case WMI_VDEV_START_RESPONSE_INCORRECT_CHANNEL_PARAMS:
		return "incorrect channel params";
	case WMI_VDEV_START_RESPONSE_GENERIC_VDEV_START_FAILURE:
		return "generic vdev start failure";
	default:
		return "unknown";
	}
}

static void ath12k_vdev_start_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_vdev_start_resp_event vdev_start_resp;
	struct ath12k *ar;
	u32 status;

	if (ath12k_pull_vdev_start_resp_tlv(ab, skb, &vdev_start_resp) != 0) {
		ath12k_warn(ab, "failed to extract vdev start resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(vdev_start_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev start resp ev %d",
			    vdev_start_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->last_wmi_vdev_start_status = 0;
	ar->max_allowed_tx_power = vdev_start_resp.max_allowed_tx_power;

	status = le32_to_cpu(vdev_start_resp.status);

	if (status) {
		ath12k_warn(ab, "vdev start resp error status %d (%s) for vdev id %d\n",
			    status, ath12k_wmi_vdev_resp_print(status),
			    vdev_start_resp.vdev_id);
		ar->last_wmi_vdev_start_status = status;
		ath12k_critical_failure_trigger(ab, ATH12K_CRIT_VAP_FAILURE);
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg_level(ab, (ATH12K_DBG_WMI | ATH12K_DBG_MAC), ATH12K_DBG_L1,
			      "vdev start resp for vdev id %d", vdev_start_resp.vdev_id);
}

static void ath12k_bcn_tx_status_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	u32 vdev_id, tx_status, found = 0;

	if (ath12k_pull_bcn_tx_status_ev(ab, skb, &vdev_id, &tx_status) != 0) {
		ath12k_warn(ab, "failed to extract bcn tx status");
		return;
	}

	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in bcn tx status event %d",
			    vdev_id);
		return;
	}

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (ath12k_mac_is_bridge_vdev(arvif))
			continue;
		if (vdev_id == arvif->vdev_id) {
			found = 1;
			break;
		}
	}
	spin_unlock_bh(&ar->data_lock);

	if (!found) {
		ath12k_warn(ab, "vdev not found in arlist");
		return;
	}

	if (arvif->is_up && arvif->ar)
		wiphy_work_queue(ath12k_ar_to_hw(arvif->ar)->wiphy,
				 &arvif->update_bcn_tx_status_work);
}

static void ath12k_vdev_stopped_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_stopped_param_tlv(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev stopped event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev stopped ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev stopped for vdev id %d", vdev_id);
}

static void
ath12k_update_bcast_ttlm_params(struct ath12k_base *ab,
				const struct ath12k_mgmt_rx_mlo_bcast_ttlm_info *params,
				u32 num_bcast_ttlm_params)
{
	struct ath12k_link_vif *arvif;
	const struct ath12k_mgmt_rx_mlo_bcast_ttlm_info *info;
	u32 i;

	for (i = 0; i < num_bcast_ttlm_params; i++) {
		u32 vdev_id, expec_dur;

		info = &params[i];
		vdev_id = le32_to_cpu(info->vdev_id);
		expec_dur = le32_to_cpu(info->expec_dur);
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
		if (!arvif) {
			ath12k_err(ab, "Error in getting arvif from vdev id:%d\n",
				   info->vdev_id);
			continue;
		}

		/* update mac80211 only if expec_dur is greater than 0 */
		if (arvif->is_up && arvif->ahvif->vif->valid_links && expec_dur)
			ieee80211_ttlm_info_expec_dur_update(arvif->ahvif->vif,
							     arvif->link_id,
							     expec_dur);
	}
}

static void ath12k_update_assoc_fail_stats(struct ath12k *ar,
					   struct ieee80211_hdr *hdr,
					   struct sk_buff *skb)
{
	struct ieee80211_mgmt *mgmt;
	const u8 *ie_start, *rsnie, *wpaie, *rates_ie;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	struct ath12k_link_vif *arvif;
	bool found = false;
	u16 capab_info;
	size_t ie_len;

	/* Validate skb before any processing */
	if (!skb || !skb->data ||
	    skb->len < offsetof(struct ieee80211_mgmt, u.assoc_req.variable))
		return;

	mgmt = (struct ieee80211_mgmt *)skb->data;

	/* Validate and compute the IE region from the skb */
	ie_start = mgmt->u.assoc_req.variable;
	if ((u8 *)ie_start >= skb->data + skb->len)
		return;

	ie_len = (skb->data + skb->len) - ie_start;
	if (ie_len > skb->len || ie_len > IEEE80211_MAX_DATA_LEN)
		return;

	rsnie    = cfg80211_find_ie(WLAN_EID_RSN, ie_start, ie_len);
	wpaie    = cfg80211_find_vendor_ie(WLAN_OUI_MICROSOFT,
					   WLAN_OUI_TYPE_MICROSOFT_WPA,
					   ie_start, ie_len);
	rates_ie = cfg80211_find_ie(WLAN_EID_SUPP_RATES, ie_start, ie_len);
	capab_info = le16_to_cpu(mgmt->u.assoc_req.capab_info);

	spin_lock_bh(&ar->data_lock);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (ether_addr_equal(arvif->bssid, hdr->addr3)) {
			found = true;
			break;
		}
	}

	if (!found) {
		spin_unlock_bh(&ar->data_lock);
		return;
	}

	mgmt_stats = &arvif->ahvif->mgmt_stats;

	/* Check for bad WPAIE: AP requires security but STA has no RSN/WPA IE */
	if ((arvif->rsnie_present || arvif->wpaie_present) && !rsnie && !wpaie)
		mgmt_stats->rx_assoc_bad_wpaie++;

	/* Check for no rate match: STA has no Supported Rates IE */
	if (!rates_ie)
		mgmt_stats->rx_assoc_no_rate_match++;

	/* Check for capability mismatch:
	 * AP requires privacy but STA doesn't advertise it.
	 */
	if ((arvif->rsnie_present || arvif->wpaie_present) &&
	    !(capab_info & WLAN_CAPABILITY_PRIVACY))
		mgmt_stats->rx_assoc_cap_mismatch++;

	spin_unlock_bh(&ar->data_lock);
}

/**
 * ath12k_bcast_probe_rl_check() - Rate-limit broadcast probe requests per STA.
 *
 * Called with ar->data_lock held (BH disabled).
 *
 * Looks up the source MAC address (@sa) in the per-radio rate-limit table.
 * If an entry exists and was last seen within ATH12K_BCAST_PROBE_RL_WINDOW_MS,
 * returns true (caller should drop the frame).  Otherwise inserts or refreshes
 * the entry and returns false (caller should forward the frame).
 *
 * On allocation failure the frame is always forwarded (fail-open).
 *
 * @ar: radio instance
 * @sa: source MAC address from the probe request (hdr->addr2)
 *
 * Returns: true if the frame should be dropped, false if it should be forwarded.
 */
static bool ath12k_bcast_probe_rl_check(struct ath12k *ar, const u8 *sa)
{
	struct ath12k_bcast_probe_rl_entry *entry;
	struct ath12k_bcast_probe_rl_entry *oldest = NULL;
	unsigned long window = msecs_to_jiffies(ar->bcast_probe_rl_window_ms);
	u64 key = ether_addr_to_u64(sa);
	int bkt;

	lockdep_assert_held(&ar->data_lock);

	hash_for_each_possible(ar->bcast_probe_rl, entry, hnode, key) {
		if (!ether_addr_equal(entry->addr, sa))
			continue;
		/* Entry found: check if still within the suppress window. */
		if (time_before(jiffies, entry->last_seen + window))
			return true;
		/* Window expired: refresh timestamp and forward. */
		entry->last_seen = jiffies;
		return false;
	}

	/* No entry yet: check table cap before allocating. */
	if (ar->bcast_probe_rl_entries >= ar->bcast_probe_rl_max_entries) {
		/* Table full: evict the oldest entry (smallest last_seen)
		 * and reuse it for this new STA. This keeps rate-limiting
		 * effective when many transient STAs fill the table.
		 * O(n) scan is acceptable -- only runs when table is full.
		 */
		hash_for_each(ar->bcast_probe_rl, bkt, entry, hnode) {
			if (!oldest ||
			    time_before(entry->last_seen, oldest->last_seen))
				oldest = entry;
		}

		if (oldest) {
			hash_del(&oldest->hnode);
			ether_addr_copy(oldest->addr, sa);
			oldest->last_seen = jiffies;
			hash_add(ar->bcast_probe_rl, &oldest->hnode, key);
			/* count unchanged: one evicted, one inserted */
		}
		return false;
	}

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return false;	/* fail-open: forward on alloc failure */

	ether_addr_copy(entry->addr, sa);
	entry->last_seen = jiffies;
	hash_add(ar->bcast_probe_rl, &entry->hnode, key);
	ar->bcast_probe_rl_entries++;
	return false;
}

/* Note: called under rcu_read_lock() */
static void ath12k_update_peer_tx_ba_params(struct ath12k *ar,
					    struct ieee80211_vif *vif,
					    struct sk_buff *skb)
{
	const u8 *buf, *addba_ext_ie, *end;
	struct ath12k_link_sta *arsta;
	u16 capab, buf_size, timeout;
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	u8 category, action, tid;
	bool has_protected;
	size_t min_len;

	mgmt = (struct ieee80211_mgmt *)skb->data;
	has_protected = ieee80211_has_protected(mgmt->frame_control);
	end = skb->data + skb->len;

	min_len = offsetof(struct ieee80211_mgmt, u.action.u.addba_resp.variable);
	if (has_protected)
		min_len += IEEE80211_CCMP_HDR_LEN;

	if (skb->len < min_len)
		return;

	buf = &mgmt->u.action.category;

	if (has_protected)
		buf += IEEE80211_CCMP_HDR_LEN;

	category = *buf++;
	action = *buf++;

	if (category != WLAN_CATEGORY_BACK || action != WLAN_ACTION_ADDBA_RESP)
		return;

	buf += 3; /* skip dialog token and status */

	capab = get_unaligned_le16(buf);
	buf += 2;

	timeout = get_unaligned_le16(buf);
	buf += 2;

	tid = u16_get_bits(capab, IEEE80211_ADDBA_PARAM_TID_MASK);
	if (tid >= IEEE80211_MAX_NUM_TIDS)
		return;

	buf_size = u16_get_bits(capab, IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK);

	spin_lock_bh(&ar->arsta_lock);
	arsta = ath12k_link_sta_find_by_addr(ar, mgmt->sa);
	if (!arsta || !arsta->ahsta) {
		spin_unlock_bh(&ar->arsta_lock);
		return;
	}

	ahsta = arsta->ahsta;
	sta = ath12k_ahsta_to_sta(ahsta);

	addba_ext_ie = cfg80211_find_ie(WLAN_EID_ADDBA_EXT, (u8 *)mgmt + min_len,
					skb->len - min_len);
	if (!addba_ext_ie)
		goto skip_addba_ext;

	if (addba_ext_ie[1] &&
	    addba_ext_ie + addba_ext_ie[1] + 1 <= end &&
	    sta->deflink.eht_cap.has_eht) {
		u8 buf_size_1k;

		buf_size_1k = u8_get_bits(addba_ext_ie[2],
					  IEEE80211_ADDBA_EXT_BUF_SIZE_MASK);
		buf_size |= (u16)buf_size_1k << IEEE80211_ADDBA_EXT_BUF_SIZE_SHIFT;

		buf_size = min(buf_size, IEEE80211_MAX_AMPDU_BUF_EHT);
	}

skip_addba_ext:
	spin_lock_bh(&ahsta->ba_lock);

	ahsta->tx_ba_params[tid].valid = true;
	ahsta->tx_ba_params[tid].tid = tid;
	ahsta->tx_ba_params[tid].amsdu = capab & IEEE80211_ADDBA_PARAM_AMSDU_MASK;
	ahsta->tx_ba_params[tid].timeout = timeout;
	ahsta->tx_ba_params[tid].buf_size = buf_size;
	ahsta->tx_ba_params[tid].policy =
		(capab & IEEE80211_ADDBA_PARAM_POLICY_MASK) >> 1;

	ath12k_dbg(ar->ab, ATH12K_DBG_SMD,
		   "AddBA Response from %pM, tid: %u amsdu: %d timeout: %u buf_size: %u policy: %u",
		   sta->addr, tid,
		   ahsta->tx_ba_params[tid].amsdu, timeout, buf_size,
		   ahsta->tx_ba_params[tid].policy);

	spin_unlock_bh(&ahsta->ba_lock);
	spin_unlock_bh(&ar->arsta_lock);
}

static void ath12k_mgmt_rx_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_mgmt_rx_arg rx_ev = {0};
	struct ath12k *ar;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_link_sta *arsta;
	bool is_4addr_null_pkt = false;
	struct ieee80211_hdr *hdr;
	u16 fc;
	struct ieee80211_supported_band *sband;
	struct ath12k_vif *ahvif;
	struct ath12k_mgmt_frame_stats *mgmt_stats;
	u16 frm_stype;
	struct ath12k_link_vif *arvif = NULL;
	struct ieee80211_vif *vif = NULL;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	s8 rssi;
	u32 vdev_id;
	u8 link_id;
	struct ath12k_link_vif *arvif_iter;

	rx_ev.num_link_removal_info = 0;
	if (ath12k_pull_mgmt_rx_params_tlv(ab, skb, &rx_ev) != 0) {
		ath12k_warn(ab, "failed to extract mgmt rx event");
		dev_kfree_skb(skb);
		return;
	}

	memset(status, 0, sizeof(*status));

	ath12k_dbg(ab, ATH12K_DBG_MGMT, "mgmt rx event status %08x\n",
		   rx_ev.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, rx_ev.pdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id %d in mgmt_rx_event\n",
			    rx_ev.pdev_id);
		dev_kfree_skb(skb);
		goto exit;
	}

	if ((test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) ||
	    (rx_ev.status & (WMI_RX_STATUS_ERR_DECRYPT |
			     WMI_RX_STATUS_ERR_KEY_CACHE_MISS |
			     WMI_RX_STATUS_ERR_CRC))) {
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.status & WMI_RX_STATUS_ERR_MIC)
		status->flag |= RX_FLAG_MMIC_ERROR;

	if (rx_ev.chan_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    rx_ev.chan_freq <= ATH12K_MAX_6GHZ_FREQ) {
		status->band = NL80211_BAND_6GHZ;
		status->freq = rx_ev.chan_freq;
	} else if (rx_ev.channel >= 1 && rx_ev.channel <= 14) {
		status->band = NL80211_BAND_2GHZ;
	} else if (rx_ev.channel >= 36 && rx_ev.channel <= ATH12K_MAX_5GHZ_CHAN) {
		status->band = NL80211_BAND_5GHZ;
	} else {
		/* Shouldn't happen unless list of advertised channels to
		 * mac80211 has been changed.
		 */
		WARN_ON_ONCE(1);
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.phy_mode == MODE_11B &&
	    (status->band == NL80211_BAND_5GHZ || status->band == NL80211_BAND_6GHZ))
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi mgmt rx 11b (CCK) on 5/6GHz, band = %d\n", status->band);

	sband = &ar->mac.sbands[status->band];

	if (status->band != NL80211_BAND_6GHZ)
		status->freq = ieee80211_channel_to_frequency(rx_ev.channel,
							      status->band);

	status->signal = rx_ev.snr + ar->rssi_offsets.rssi_offset;
	status->rate_idx = ath12k_mac_bitrate_to_idx(sband, rx_ev.rate / 100);

	hdr = (struct ieee80211_hdr *)skb->data;
	fc = le16_to_cpu(hdr->frame_control);
	frm_stype = FIELD_GET(IEEE80211_FCTL_STYPE, fc);

	if (ATH12K_MGMT_MLME_FRAME(hdr->frame_control))
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MLME, ATH12K_DBG_L0,
				 "Received %s frame from STA %pM\n",
				 mgmt_frame_name[frm_stype], hdr->addr2);
	/* Track assoc request failure stats (bad WPAIE, no rate match, cap mismatch) */
	if (ieee80211_is_assoc_req(hdr->frame_control) &&
	    skb->len >= offsetof(struct ieee80211_mgmt, u.assoc_req.variable))
		ath12k_update_assoc_fail_stats(ar, hdr, skb);

	spin_lock_bh(&ar->arsta_lock);
	arsta = ath12k_link_sta_find_by_addr(ar, hdr->addr1);
	if (!arsta)
		arsta = ath12k_link_sta_find_by_addr(ar, hdr->addr3);
	if (!arsta || !arsta->arvif->ahvif) {
		spin_unlock_bh(&ar->arsta_lock);
		/* 802.11 Probe Request address fields (ToDS=0, FromDS=0):
		 * addr1 = DA (Destination Address) - ff:ff:ff:ff:ff:ff for broadcast
		 * addr2 = SA/TA (Source/Transmitter - STA MAC address)
		 * addr3 = BSSID - ff:ff:ff:ff:ff:ff for wildcard (broadcast probe),
		 *                  specific AP MAC for directed (unicast) probe.
		 * Use addr3 (BSSID) to classify BC vs UC, not addr1 (DA).
		 */
		if (ieee80211_is_probe_req(hdr->frame_control)) {
			if (is_broadcast_ether_addr(hdr->addr3)) {
				/* Count broadcast probe requests at radio level.
				 * Avoids O(n) VAP iteration on every broadcast
				 * probe in high-density (16+ VAP) environments.
				 */
				spin_lock_bh(&ar->data_lock);
				ar->dp.stats.telemetry_stats.rx_probe_req_bc++;
				if (ar->bcast_probe_rl_enabled &&
				    ath12k_bcast_probe_rl_check(ar, hdr->addr2)) {
					ar->dp.stats.telemetry_stats.rx_bc_prb_req_drop++;
					spin_unlock_bh(&ar->data_lock);
					dev_kfree_skb(skb);
					goto exit;
				}
				spin_unlock_bh(&ar->data_lock);
			} else {
				/* Count directed (unicast) probe requests
				 * from unassociated stations for the matching AP vif.
				 * addr3 = BSSID in Probe Request.
				 * ar->data_lock serialises all ar->arvifs list
				 * modifications, so plain list_for_each_entry is
				 * correct here; rcu_read_lock() is not needed.
				 */
				spin_lock_bh(&ar->data_lock);
				list_for_each_entry(arvif_iter, &ar->arvifs, list) {
					if (arvif_iter->ahvif->vdev_type !=
					    WMI_VDEV_TYPE_AP)
						continue;
					if (!ether_addr_equal
					    (arvif_iter->bssid, hdr->addr3))
						continue;
					mgmt_stats = &arvif_iter->ahvif->mgmt_stats;
					mgmt_stats->rx_cnt
						[QCA_VENDOR_MGMT_STATS_PROBE_REQ]++;
					mgmt_stats->aggr_rx_mgmt++;
					break;
				}
				spin_unlock_bh(&ar->data_lock);
			}
		}
		goto skip_mgmt_stats;
	}

	link_id = arsta->link_id;
	vdev_id = arsta->arvif->vdev_id;
	ahvif = arsta->arvif->ahvif;
	spin_unlock_bh(&ar->arsta_lock);

	spin_lock_bh(&ar->data_lock);

	mgmt_stats = &ahvif->mgmt_stats;
	mgmt_stats->rx_cnt[frm_stype]++;
	mgmt_stats->aggr_rx_mgmt++;
	if (ieee80211_is_probe_req(fc) && is_broadcast_ether_addr(hdr->addr3))
		ar->dp.stats.telemetry_stats.rx_probe_req_bc++;

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif(ar, vdev_id);

	if (rx_ev.status & WMI_RX_STATUS_ERR_PN) {
		mgmt_stats->rx_pn_err_cnt++;
		if (arvif)
			arvif->rx_pn_err_cnt++;
		rcu_read_unlock();
		spin_unlock_bh(&ar->data_lock);
		dev_kfree_skb(skb);
		goto exit;
	}

	if (arvif && arvif->ahvif)
		vif = ath12k_ahvif_to_vif(arvif->ahvif);

	if (vif &&
	    (vif->type == NL80211_IFTYPE_AP || vif->type == NL80211_IFTYPE_STATION) &&
	    ieee80211_is_action(hdr->frame_control))
		ath12k_update_peer_tx_ba_params(ar, vif, skb);

	if (arvif) {
		rssi = status->signal;

		sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
						   hdr->addr2, NULL);
		if (!sta)
			goto skip_rssi_update;
		ahsta = ath12k_sta_to_ahsta(sta);
		if (!ahsta)
			goto skip_rssi_update;

		if (link_id < IEEE80211_MLD_MAX_NUM_LINKS)
			arsta = rcu_dereference(ahsta->link[link_id]);
		if (!arsta)
			arsta = &ahsta->deflink;

		if (arsta) {
			arsta->max_rssi = max(arsta->max_rssi, rssi);
			arsta->min_rssi = min(arsta->min_rssi, rssi);
		}
	}

skip_rssi_update:
	rcu_read_unlock();

	spin_unlock_bh(&ar->data_lock);

skip_mgmt_stats:

	is_4addr_null_pkt = ieee80211_is_nullfunc(hdr->frame_control) &&
			    ieee80211_has_a4(hdr->frame_control);

	if (ieee80211_is_data(hdr->frame_control) && !is_4addr_null_pkt) {
		ab->dp->device_stats.rx_pkt_null_frame_dropped++;
		dev_kfree_skb(skb);
		goto exit;
	}

	/* Firmware is guaranteed to report all essential management frames via
	 * WMI while it can deliver some extra via HTT. Since there can be
	 * duplicates split the reporting wrt monitor/sniffing.
	 */
	status->flag |= RX_FLAG_SKIP_MONITOR;

	if (is_4addr_null_pkt) {
		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, hdr->addr2);
		if (!arsta || arsta->is_bridge_peer || arsta->is_self_peer) {
			spin_unlock_bh(&ar->arsta_lock);
			ath12k_warn(ab, "arsta not found %pM\n",
				    hdr->addr2);
			dev_kfree_skb(skb);
			goto exit;
		}
		status->link_id = arsta->link_id;
		spin_unlock_bh(&ar->arsta_lock);

		pubsta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
						      hdr->addr2, NULL);
		if (pubsta && pubsta->valid_links)
			status->link_valid = 1;
		else
			status->link_id = 0;

		ab->dp->device_stats.rx_pkt_null_frame_handled++;
		ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L0,
			   "QoS NULL frame received for %pM state:%d\n",
			   hdr->addr2,
			   arsta ? arsta->ahsta->state : -1);
		ieee80211_rx_napi(ar->ah->hw, pubsta, skb, NULL);
		goto exit;
	}

	/* In case of PMF or (Re)Association Request/Response encryption,
	 * FW delivers decrypted frames with Protected Bit set including
	 * group privacy action frames.
	 */
	if (ieee80211_has_protected(hdr->frame_control)) {
		struct ath12k_link_sta *arsta;
		struct ieee80211_sta *sta;
		bool enc_assoc;

		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, hdr->addr2);
		if (!arsta) {
			spin_unlock_bh(&ar->arsta_lock);
			ath12k_warn(ab, "arsta not found %pM\n",
				    hdr->addr2);
			dev_kfree_skb(skb);
			goto exit;
		}

		sta = ath12k_ahsta_to_sta(arsta->ahsta);

		enc_assoc = sta && sta->epp_peer &&
			    (ieee80211_is_assoc_req(hdr->frame_control) ||
			     ieee80211_is_assoc_resp(hdr->frame_control) ||
			     ieee80211_is_reassoc_req(hdr->frame_control) ||
			     ieee80211_is_reassoc_resp(hdr->frame_control));

		spin_unlock_bh(&ar->arsta_lock);
		status->flag |= RX_FLAG_DECRYPTED;

		if (!ieee80211_is_robust_mgmt_frame(skb) &&
		    !enc_assoc) {
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_MMIC_STRIPPED;
			hdr->frame_control = __cpu_to_le16(fc &
					     ~IEEE80211_FCTL_PROTECTED);
		}
	}

	if (ieee80211_is_beacon(hdr->frame_control))
		ath12k_mac_handle_beacon(ar, skb);

	/**
	 * RX MLO Link removal info TLV. Parse this TLV only when
	 * num_link_removal_info is present, otherwise ignore it.
	 */
	if (rx_ev.num_link_removal_info)
		ath12k_update_link_removal_params(ab, rx_ev.link_removal_info,
						  rx_ev.num_link_removal_info);

	if (ieee80211_is_probe_req(hdr->frame_control)) {
		if (rx_ev.num_bcast_ttlm_info)
			ath12k_update_bcast_ttlm_params(ab,
							rx_ev.bcast_ttlm_info,
							rx_ev.num_bcast_ttlm_info);
		ath12k_core_cu_notify(ar, arvif);
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_mgmt_rx_event_extn(ar, skb, &rx_ev);
#endif

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx skb %p len %d ftype %02x stype %02x\n",
		   skb, skb->len,
		   fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx freq %d band %d snr %d, rate_idx %d\n",
		   status->freq, status->band, status->signal,
		   status->rate_idx);

	if (ath12k_mac_mgmt_need_smd_sta_session_ctx(skb) &&
	    !ath12k_smd_collect_sta_session_ctx(ar, skb)) {
		/* frame will be delivered from SMD context collector */
		rcu_read_unlock();
		return;
	}

	ieee80211_rx_ni(ath12k_ar_to_hw(ar), skb);

exit:
	rcu_read_unlock();
}

static void ath12k_mgmt_tx_compl_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_mgmt_tx_compl_event tx_compl_param = {0};
	struct ath12k *ar;

	if (ath12k_pull_mgmt_tx_compl_param_tlv(ab, skb, &tx_compl_param) != 0) {
		ath12k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(tx_compl_param.pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in mgmt_tx_compl_event\n",
			    tx_compl_param.pdev_id);
		goto exit;
	}

	wmi_process_mgmt_tx_comp(ar, le32_to_cpu(tx_compl_param.desc_id),
				 le32_to_cpu(tx_compl_param.status),
				 le32_to_cpu(tx_compl_param.ack_rssi));

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "mgmt tx compl ev pdev_id %d, desc_id %d, status %d",
		   tx_compl_param.pdev_id, tx_compl_param.desc_id,
		   tx_compl_param.status);

exit:
	rcu_read_unlock();
}

static void ath12k_offchan_tx_completion_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct wmi_offchan_data_tx_compl_event offchan_tx_cmpl_params = {0};
	u32 desc_id;
	u32 pdev_id;
	u32 status;
	struct ath12k *ar;

	if (ath12k_pull_offchan_tx_compl_param_tlv(ab, skb, &offchan_tx_cmpl_params)
	    != 0) {
		ath12k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}
	status  = __le32_to_cpu(offchan_tx_cmpl_params.status);
	pdev_id = __le32_to_cpu(offchan_tx_cmpl_params.pdev_id);
	desc_id = __le32_to_cpu(offchan_tx_cmpl_params.desc_id);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in offchan_tx_compl_event\n",
			    pdev_id);
		goto exit;
	}

	wmi_process_offchan_tx_comp(ar, desc_id, status);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "off chan tx compl ev pdev_id %d, desc_id %d, status %d",
		   pdev_id, desc_id, status);
exit:
	rcu_read_unlock();
}

static struct ath12k *ath12k_get_ar_on_scan_state(struct ath12k_base *ab,
						  u32 vdev_id,
						  enum ath12k_scan_state state)
{
	int i;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			ar = pdev->ar;

			spin_lock_bh(&ar->data_lock);
			if (ar->scan.state == state &&
			    ar->scan.arvif &&
			    ar->scan.arvif->vdev_id == vdev_id) {
				spin_unlock_bh(&ar->data_lock);
				return ar;
			}
			spin_unlock_bh(&ar->data_lock);
		}
	}
	return NULL;
}

static void ath12k_scan_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_scan_event scan_ev = {0};

	if (ath12k_pull_scan_ev(ab, skb, &scan_ev) != 0) {
		ath12k_warn(ab, "failed to extract scan event");
		return;
	}

	rcu_read_lock();

	/* In case the scan was cancelled, ex. during interface teardown,
	 * the interface will not be found in active interfaces.
	 * Rather, in such scenarios, iterate over the active pdev's to
	 * search 'ar' if the corresponding 'ar' scan is ABORTING and the
	 * aborting scan's vdev id matches this event info.
	 */
	if (le32_to_cpu(scan_ev.event_type) == WMI_SCAN_EVENT_COMPLETED &&
	    le32_to_cpu(scan_ev.reason) == WMI_SCAN_REASON_CANCELLED) {
		ar = ath12k_get_ar_on_scan_state(ab, le32_to_cpu(scan_ev.vdev_id),
						 ATH12K_SCAN_ABORTING);
		if (!ar)
			ar = ath12k_get_ar_on_scan_state(ab, le32_to_cpu(scan_ev.vdev_id),
							 ATH12K_SCAN_RUNNING);
	} else {
		ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(scan_ev.vdev_id));
	}

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_cbs_skip_scan_event_and_send_scan_complete(ar, &scan_ev)) {
		rcu_read_unlock();
		return;
	}
#endif

	if (!ar) {
		ath12k_warn(ab, "Received scan event for unknown vdev");
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "scan event %s type %d reason %d freq %d req_id %d scan_id %d vdev_id %d state %s (%d)\n",
		   ath12k_wmi_event_scan_type_str(le32_to_cpu(scan_ev.event_type),
						  le32_to_cpu(scan_ev.reason)),
		   le32_to_cpu(scan_ev.event_type),
		   le32_to_cpu(scan_ev.reason),
		   le32_to_cpu(scan_ev.channel_freq),
		   le32_to_cpu(scan_ev.scan_req_id),
		   le32_to_cpu(scan_ev.scan_id),
		   le32_to_cpu(scan_ev.vdev_id),
		   ath12k_scan_state_str(ar->scan.state), ar->scan.state);

	switch (le32_to_cpu(scan_ev.event_type)) {
	case WMI_SCAN_EVENT_STARTED:
		ath12k_wmi_event_scan_started(ar);
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		ath12k_wmi_event_scan_completed(ar);
#ifdef CPTCFG_QCN_EXTN
		ath12k_cbs_resume_post_mac80211_scan(ar);
#endif
		break;
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		ath12k_wmi_event_scan_bss_chan(ar, le32_to_cpu(scan_ev.scan_id),
					       le32_to_cpu(scan_ev.channel_freq));
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		ath12k_wmi_event_scan_foreign_chan(ar, le32_to_cpu(scan_ev.channel_freq));
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		ath12k_warn(ab, "received scan start failure event\n");
		ath12k_wmi_event_scan_start_failed(ar);
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		__ath12k_mac_scan_finish(ar);
		break;
	case WMI_SCAN_EVENT_PREEMPTED:
	case WMI_SCAN_EVENT_RESTARTED:
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
	default:
		break;
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_update_offchan_ts_extn(ar, scan_ev.event_type);
#endif /* CPTCFG_QCN_EXTN */

	spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();
}

struct ath12k_kickout_ctx {
	struct ath12k_base *ab;
	const struct wmi_peer_sta_kickout_arg *arg;
};

static void ath12k_kickout_iter_cb(struct ath12k *ar,
				   struct ath12k_link_sta *arsta,
				   void *data)
{
	struct ath12k_kickout_ctx *ctx = data;
	const struct wmi_peer_sta_kickout_arg *arg = ctx->arg;
	struct ath12k_vif *ahvif;
	struct ieee80211_sta *sta;
	unsigned int link_id;
	bool is_mlo;

	if (!arsta->arvif || !arsta->arvif->ahvif) {
		ath12k_warn(ctx->ab, "invalid arvif in peer sta kickout ev");
		return;
	}

	ahvif = arsta->arvif->ahvif;
	is_mlo = arsta->ahsta ? arsta->ahsta->is_mlo : false;

	if (is_mlo)
		sta = ieee80211_find_sta_by_link_addrs(ar->ah->hw, arg->mac_addr,
						       NULL, &link_id);
	else
		sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
					   arg->mac_addr, NULL);
	if (!sta) {
		ath12k_warn(ctx->ab, "Spurious quick kickout for %sSTA %pM\n",
			    is_mlo ? "MLO " : "", arg->mac_addr);
		return;
	}

	if (is_mlo && arsta->link_id != link_id) {
		ath12k_warn(ctx->ab,
			    "Spurious quick kickout for MLO STA %pM with invalid link_id, peer: %d, sta: %d\n",
			    arg->mac_addr, arsta->link_id, link_id);
		return;
	}

	if (ar->ab->hw_params->handle_beacon_miss &&
	    ahvif->vif->type == NL80211_IFTYPE_STATION &&
	    arg->reason == __cpu_to_le32(WMI_PEER_STA_KICKOUT_REASON_INACTIVITY))
		ath12k_mac_handle_beacon_miss(ar, arsta->arvif->vdev_id);
	else
		ieee80211_report_low_ack(sta, 10);

	ath12k_info(ctx->ab, "peer sta kickout event %pM reason %d\n",
		   arg->mac_addr, arg->reason);
}

static void ath12k_peer_sta_kickout_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_sta_kickout_arg arg = {};
	struct ath12k_kickout_ctx ctx = { .ab = ab, .arg = &arg };

	if (ath12k_pull_peer_sta_kickout_ev(ab, skb, &arg) != 0) {
		ath12k_warn(ab, "failed to extract peer sta kickout event");
		return;
	}

	if (!ath12k_arsta_itr_on_ab_by_addr(ab, arg.mac_addr,
					    ath12k_kickout_iter_cb, &ctx))
		ath12k_warn(ab, "peer not found %pM\n", arg.mac_addr);
}

static void ath12k_roam_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_roam_event roam_ev = {};
	struct ath12k *ar;
	u32 vdev_id;
	u8 roam_reason;

	if (ath12k_pull_roam_ev(ab, skb, &roam_ev) != 0) {
		ath12k_warn(ab, "failed to extract roam event");
		return;
	}

	vdev_id = le32_to_cpu(roam_ev.vdev_id);
	roam_reason = u32_get_bits(le32_to_cpu(roam_ev.reason),
				   WMI_ROAM_REASON_MASK);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi roam event vdev %u reason %d rssi %d\n",
		   vdev_id, roam_reason, roam_ev.rssi);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in roam ev %d", vdev_id);
		rcu_read_unlock();
		return;
	}

	if (roam_reason >= WMI_ROAM_REASON_MAX)
		ath12k_warn(ab, "ignoring unknown roam event reason %d on vdev %i\n",
			    roam_reason, vdev_id);

	switch (roam_reason) {
	case WMI_ROAM_REASON_BEACON_MISS:
		ath12k_mac_handle_beacon_miss(ar, vdev_id);
		break;
	case WMI_ROAM_REASON_BETTER_AP:
	case WMI_ROAM_REASON_LOW_RSSI:
	case WMI_ROAM_REASON_SUITABLE_AP_FOUND:
	case WMI_ROAM_REASON_HO_FAILED:
		ath12k_warn(ab, "ignoring not implemented roam event reason %d on vdev %i\n",
			    roam_reason, vdev_id);
		break;
	}

	rcu_read_unlock();
}

static void ath12k_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_chan_info_event ch_info_ev = {0};
	struct ath12k *ar;
	struct survey_info *survey;
	int idx;
	/* HW channel counters frequency value in hertz */
	u32 cc_freq_hz = ab->cc_freq_hz;

	if (ath12k_pull_chan_info_ev(ab, skb, &ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract chan info event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "chan info vdev_id %d err_code %d freq %d cmd_flags %d noise_floor %d rx_clear_count %d cycle_count %d mac_clk_mhz %d\n",
		   ch_info_ev.vdev_id, ch_info_ev.err_code, ch_info_ev.freq,
		   ch_info_ev.cmd_flags, ch_info_ev.noise_floor,
		   ch_info_ev.rx_clear_count, ch_info_ev.cycle_count,
		   ch_info_ev.mac_clk_mhz);

	if (le32_to_cpu(ch_info_ev.cmd_flags) == WMI_CHAN_INFO_END_RESP) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "chan info report completed\n");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(ch_info_ev.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in chan info ev %d",
			    ch_info_ev.vdev_id);
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:

#ifdef CPTCFG_QCN_EXTN
		if (ath12k_is_cbs_vendor_event(ar) &&
		    le32_to_cpu(ch_info_ev.cmd_flags) ==
		    WMI_CHAN_INFO_START_RESP)
			break;
#endif

		ath12k_warn(ab, "received chan info event without a scan request, ignoring\n");
		goto exit;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		break;
	}

	idx = freq_to_idx(ar, le32_to_cpu(ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "chan info: invalid frequency %d (idx %d out of bounds)\n",
			    ch_info_ev.freq, idx);
		goto exit;
	}

	/* If FW provides MAC clock frequency in Mhz, overriding the initialized
	 * HW channel counters frequency value
	 */
	if (ch_info_ev.mac_clk_mhz)
		cc_freq_hz = (le32_to_cpu(ch_info_ev.mac_clk_mhz) * 1000);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_START_RESP) {
		survey = &ar->survey[idx];
		memset(survey, 0, sizeof(*survey));
		survey->noise = le32_to_cpu(ch_info_ev.noise_floor);
		survey->filled = SURVEY_INFO_NOISE_DBM | SURVEY_INFO_TIME |
				 SURVEY_INFO_TIME_BUSY;
		survey->time = div_u64(le32_to_cpu(ch_info_ev.cycle_count), cc_freq_hz);
		survey->time_busy = div_u64(le32_to_cpu(ch_info_ev.rx_clear_count),
					    cc_freq_hz);
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_update_offchan_stats_extn(ar, skb,
					 le32_to_cpu(ch_info_ev.freq),
					 &ch_info_ev, idx);
#endif

exit:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}

static void
ath12k_pdev_bss_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_pdev_bss_chan_info_event bss_ch_info_ev = {};
	struct survey_info *survey;
	struct ath12k *ar;
	u32 cc_freq_hz = ab->cc_freq_hz;
	u64 busy, total, tx, rx, rx_bss;
	int idx;

	if (ath12k_pull_pdev_bss_chan_info_ev(ab, skb, &bss_ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract pdev bss chan info event");
		return;
	}

	busy = (u64)(le32_to_cpu(bss_ch_info_ev.rx_clear_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_clear_count_low);

	total = (u64)(le32_to_cpu(bss_ch_info_ev.cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.cycle_count_low);

	tx = (u64)(le32_to_cpu(bss_ch_info_ev.tx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.tx_cycle_count_low);

	rx = (u64)(le32_to_cpu(bss_ch_info_ev.rx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_cycle_count_low);

	rx_bss = (u64)(le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_low);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev bss chan info:\n pdev_id: %d freq: %d noise: %d cycle: busy %llu total %llu tx %llu rx %llu rx_bss %llu\n",
		   bss_ch_info_ev.pdev_id, bss_ch_info_ev.freq,
		   bss_ch_info_ev.noise_floor, busy, total,
		   tx, rx, rx_bss);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(bss_ch_info_ev.pdev_id));

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in bss_chan_info event\n",
			    bss_ch_info_ev.pdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);
	idx = freq_to_idx(ar, le32_to_cpu(bss_ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "bss chan info: invalid frequency %d (idx %d out of bounds)\n",
			    bss_ch_info_ev.freq, idx);
		goto exit;
	}

	survey = &ar->survey[idx];

	survey->noise     = le32_to_cpu(bss_ch_info_ev.noise_floor);
	survey->time      = div_u64(total, cc_freq_hz);
	survey->time_busy = div_u64(busy, cc_freq_hz);
	survey->time_rx   = div_u64(rx, cc_freq_hz);
	survey->time_bss_rx = div_u64(rx_bss, cc_freq_hz);
	survey->time_tx   = div_u64(tx, cc_freq_hz);
	survey->filled   |= (SURVEY_INFO_NOISE_DBM |
			     SURVEY_INFO_TIME |
			     SURVEY_INFO_TIME_BUSY |
			     SURVEY_INFO_TIME_RX |
			     SURVEY_INFO_TIME_BSS_RX |
			     SURVEY_INFO_TIME_TX);
exit:
	spin_unlock_bh(&ar->data_lock);
	complete(&ar->bss_survey_done);

	rcu_read_unlock();
}

static void ath12k_vdev_install_key_compl_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct wmi_vdev_install_key_complete_arg install_key_compl = {0};
	struct ath12k *ar;

	if (ath12k_pull_vdev_install_key_compl_ev(ab, skb, &install_key_compl) != 0) {
		ath12k_warn(ab, "failed to extract install key compl event");
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_WMI | ATH12K_DBG_EAPOL, ATH12K_DBG_L1,
			 "vdev install key ev idx %d flags %08x macaddr %pM status %d\n",
			 install_key_compl.key_idx, install_key_compl.key_flags,
			 install_key_compl.macaddr, install_key_compl.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, install_key_compl.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in install key compl ev %d",
			    install_key_compl.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->install_key_status = 0;

	if (install_key_compl.status != WMI_VDEV_INSTALL_KEY_COMPL_STATUS_SUCCESS) {
		ath12k_warn(ab, "install key failed for %pM status %d\n",
			    install_key_compl.macaddr, install_key_compl.status);
		ar->install_key_status = install_key_compl.status;
	}

	complete(&ar->install_key_done);
	rcu_read_unlock();
}

static int ath12k_wmi_tlv_services_parser(struct ath12k_base *ab,
					  u16 tag, u16 tag_len,
					  const void *ptr,
					  void *data)
{
	const struct wmi_service_available_event *ev;
	u32 *wmi_ext2_service_bitmap;
	int i, j;
	u16 expected_len;
	u16 wmi_max_ext2_service_words;

	expected_len = WMI_SERVICE_SEGMENT_BM_SIZE32 * sizeof(u32);
	if (tag_len < expected_len) {
		ath12k_warn(ab, "invalid length %d for the WMI services available tag 0x%x\n",
			    tag_len, tag);
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_SERVICE_AVAILABLE_EVENT:
		ev = (struct wmi_service_available_event *)ptr;
		for (i = 0, j = WMI_MAX_SERVICE;
		     i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT_SERVICE;
		     i++) {
			do {
				if (le32_to_cpu(ev->wmi_service_segment_bitmap[i]) &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi_ext_service_bitmap 0x%x 0x%x 0x%x 0x%x",
			   ev->wmi_service_segment_bitmap[0],
			   ev->wmi_service_segment_bitmap[1],
			   ev->wmi_service_segment_bitmap[2],
			   ev->wmi_service_segment_bitmap[3]);
		break;
	case WMI_TAG_ARRAY_UINT32:
		wmi_ext2_service_bitmap = (u32 *)ptr;
		wmi_max_ext2_service_words = tag_len / sizeof(u32);
		for (i = 0, j = WMI_MAX_EXT_SERVICE;
		     i < wmi_max_ext2_service_words && j < WMI_MAX_EXT2_SERVICE;
		     i++) {
			do {
				if (wmi_ext2_service_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		for (i = 0; i < wmi_max_ext2_service_words; i++)
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "wmi_ext2_service_bitmap[%u] 0x%04x",
				   i, wmi_ext2_service_bitmap[i]);
		break;
	}
	return 0;
}

static int ath12k_service_available_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_services_parser,
				  NULL);
	return ret;
}

static void ath12k_peer_assoc_conf_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_assoc_conf_arg peer_assoc_conf = {0};
	struct ath12k *ar;
	union ath12k_config_param val = {0};
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	void *dp_peer;

	if (ath12k_pull_peer_assoc_conf_ev(ab, skb, &peer_assoc_conf) != 0) {
		ath12k_warn(ab, "failed to extract peer assoc conf event");
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME, ATH12K_DBG_L1,
			 "peer assoc conf ev vdev id %d macaddr %pM status:%d\n",
			 peer_assoc_conf.vdev_id, peer_assoc_conf.macaddr,
			 peer_assoc_conf.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer_assoc_conf.vdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer assoc conf ev %d",
			    peer_assoc_conf.vdev_id);
		rcu_read_unlock();
		return;
	}

	if (!peer_assoc_conf.status) {
		/* rcu_read_lock already taken */
		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, peer_assoc_conf.macaddr);
		if (!arsta) {
			spin_unlock_bh(&ar->arsta_lock);
			goto done;
		}

		ahsta = arsta->ahsta;
		dp_peer = ath12k_sta_get_dp_peer_rcu(ahsta);
		if (!dp_peer) {
			spin_unlock_bh(&ar->arsta_lock);
			goto done;
		}

		val.assoc_success = true;
		ath12k_dp_link_peer_set_param_by_dp_peer_and_link_id(dp_peer,
								     arsta->link_id,
								     ATH12K_DP_LINK_PEER_ASSOC_PARAM,
								     &val);
		spin_unlock_bh(&ar->arsta_lock);
	}

done:
	complete(&ar->peer_assoc_done);
	rcu_read_unlock();
}

static void ath12k_peer_tx_pn_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_tx_pn_arg peer_tx_pn_arg = {};
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;

	if (ath12k_pull_peer_tx_pn_ev(ab, skb, &peer_tx_pn_arg) != 0) {
		ath12k_warn(ab, "failed to extract peer tx pn event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "peer tx pn ev vdev id %d macaddr %pM\n",
		   peer_tx_pn_arg.vdev_id, peer_tx_pn_arg.mac_addr);

	guard(rcu)();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer_tx_pn_arg.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer tx pn ev %d",
			    peer_tx_pn_arg.vdev_id);
		return;
	}

	arvif = ath12k_mac_get_arvif(ar, peer_tx_pn_arg.vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "vif not found for vdev id %d\n",
			    peer_tx_pn_arg.vdev_id);
		return;
	}

	if (peer_tx_pn_arg.key_idx == 1 || peer_tx_pn_arg.key_idx == 2)
		memcpy(arvif->gtk_pn, peer_tx_pn_arg.pn,
		       sizeof(peer_tx_pn_arg.pn));
	if (peer_tx_pn_arg.key_idx == 6 || peer_tx_pn_arg.key_idx == 7)
		memcpy(arvif->bigtk_pn, peer_tx_pn_arg.pn,
		       sizeof(peer_tx_pn_arg.pn));
}

static void
ath12k_wmi_fw_vdev_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      char *buf, u32 *length)
{
	const struct ath12k_fw_stats_vdev *vdev;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;
	struct ath12k_link_vif *arvif;
	u32 len = *length;
	u8 *vif_macaddr;
	int i;

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			 "ath12k VDEV stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "=================");

	list_for_each_entry(vdev, &fw_stats->vdevs, list) {
		arvif = ath12k_mac_get_arvif(ar, vdev->vdev_id);
		if (!arvif)
			continue;
		vif_macaddr = arvif->ahvif->vif->addr;

		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "VDEV ID", vdev->vdev_id);
		len += scnprintf(buf + len, buf_len - len, "%30s %pM\n",
				 "VDEV MAC address", vif_macaddr);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "beacon snr", vdev->beacon_snr);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "data snr", vdev->data_snr);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num rx frames", vdev->num_rx_frames);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num rts fail", vdev->num_rts_fail);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num rts success", vdev->num_rts_success);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num rx err", vdev->num_rx_err);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num rx discard", vdev->num_rx_discard);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num tx not acked", vdev->num_tx_not_acked);

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames", i,
					vdev->num_tx_frames[i]);

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames retries", i,
					vdev->num_tx_frames_retries[i]);

		for (i = 0 ; i < WLAN_MAX_AC; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"num tx frames failures", i,
					vdev->num_tx_frames_failures[i]);

		for (i = 0 ; i < MAX_TX_RATE_VALUES; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] 0x%08x\n",
					"tx rate history", i,
					vdev->tx_rate_history[i]);
		for (i = 0 ; i < MAX_TX_RATE_VALUES; i++)
			len += scnprintf(buf + len, buf_len - len,
					"%25s [%02d] %u\n",
					"beacon rssi history", i,
					vdev->beacon_rssi_history[i]);

		len += scnprintf(buf + len, buf_len - len, "\n");
		*length = len;
	}
}

static void
ath12k_wmi_fw_vdev_extd_stats_dump(struct ath12k *ar,
				   struct ath12k_fw_stats *fw_stats,
				   char *buf, u32 *length)
{
	const struct ath12k_fw_stats_vdev_extd *vdev_extd;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;
	struct ath12k_link_vif *arvif;
	u32 len = *length;

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			 "ath12k VDEV EXTENDED stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "==========================");

	list_for_each_entry(vdev_extd, &fw_stats->vdev_extds, list) {
		arvif = ath12k_mac_get_arvif(ar, vdev_extd->vdev_id);
		if (!arvif)
			continue;

		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "VDEV ID", vdev_extd->vdev_id);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num fils frames sent",
				 vdev_extd->fd_succ_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num fils frames sent fail",
				 vdev_extd->fd_fail_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num unsolicited prb rsp succ",
				 vdev_extd->unsolicited_prb_succ_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "num unsolicited prb rsp failed",
				 vdev_extd->unsolicited_prb_fail_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s 0x%x\n",
				 "VDEV info flags", vdev_extd->flags);
		len += scnprintf(buf + len, buf_len - len, "%30s %d dbm\n",
				 "VDEV tx power", vdev_extd->vdev_tx_power);

		len += scnprintf(buf + len, buf_len - len, "\n");
		*length = len;
	}
}

static void
ath12k_wmi_fw_bcn_stats_dump(struct ath12k *ar,
			     struct ath12k_fw_stats *fw_stats,
			     char *buf, u32 *length)
{
	const struct ath12k_fw_stats_bcn *bcn;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;
	struct ath12k_link_vif *arvif;
	u32 len = *length;
	size_t num_bcn;

	num_bcn = list_count_nodes(&fw_stats->bcn);

	len += scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s (%zu)\n",
			 "ath12k Beacon stats", num_bcn);
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "===================");

	list_for_each_entry(bcn, &fw_stats->bcn, list) {
		arvif = ath12k_mac_get_arvif(ar, bcn->vdev_id);
		if (!arvif)
			continue;
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "VDEV ID", bcn->vdev_id);
		len += scnprintf(buf + len, buf_len - len, "%30s %pM\n",
				 "VDEV MAC address", arvif->ahvif->vif->addr);
		len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
				 "================");
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "Num of beacon tx success", bcn->tx_bcn_succ_cnt);
		len += scnprintf(buf + len, buf_len - len, "%30s %u\n",
				 "Num of beacon tx failures", bcn->tx_bcn_outage_cnt);

		len += scnprintf(buf + len, buf_len - len, "\n");
		*length = len;
	}
}

static void
ath12k_wmi_fw_pdev_base_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				   char *buf, u32 *length, u64 fw_soc_drop_cnt)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len = scnprintf(buf + len, buf_len - len, "\n");
	len += scnprintf(buf + len, buf_len - len, "%30s\n",
			"ath12k PDEV stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			"=================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			"Channel noise floor", pdev->ch_noise_floor);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"Channel TX power", pdev->chan_tx_power);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"TX frame count", pdev->tx_frame_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"RX frame count", pdev->rx_frame_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"RX clear count", pdev->rx_clear_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"Cycle count", pdev->cycle_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			"PHY error count", pdev->phy_err_count);
	len += scnprintf(buf + len, buf_len - len, "%30s %10llu\n",
			"soc drop count", fw_soc_drop_cnt);

	*length = len;
}

static void
ath12k_wmi_fw_pdev_tx_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath12k PDEV TX stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "====================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HTT cookies queued", pdev->comp_queued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HTT cookies disp.", pdev->comp_delivered);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDU queued", pdev->msdu_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDU queued", pdev->mpdu_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs dropped", pdev->wmm_drop);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Local enqued", pdev->local_enqued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Local freed", pdev->local_freed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "HW queued", pdev->hw_queued);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PPDUs reaped", pdev->hw_reaped);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Num underruns", pdev->underrun);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PPDUs cleaned", pdev->tx_abort);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs requeued", pdev->mpdus_requed);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Excessive retries", pdev->tx_ko);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "HW rate", pdev->data_rc);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Sched self triggers", pdev->self_triggers);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Dropped due to SW retries",
			 pdev->sw_retry_failure);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Illegal rate phy errors",
			 pdev->illgl_rate_phy_err);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PDEV continuous xretry", pdev->pdev_cont_xretry);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "TX timeout", pdev->pdev_tx_timeout);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PDEV resets", pdev->pdev_resets);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "Stateless TIDs alloc failures",
			 pdev->stateless_tid_alloc_failure);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "PHY underrun", pdev->phy_underrun);
	len += scnprintf(buf + len, buf_len - len, "%30s %10u\n",
			 "MPDU is more than txop limit", pdev->txop_ovf);
	*length = len;
}

static void
ath12k_wmi_fw_pdev_rx_stats_dump(const struct ath12k_fw_stats_pdev *pdev,
				 char *buf, u32 *length)
{
	u32 len = *length;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	len += scnprintf(buf + len, buf_len - len, "\n%30s\n",
			 "ath12k PDEV RX stats");
	len += scnprintf(buf + len, buf_len - len, "%30s\n\n",
			 "====================");

	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Mid PPDU route change",
			 pdev->mid_ppdu_route_change);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Tot. number of statuses", pdev->status_rcvd);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 0", pdev->r0_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 1", pdev->r1_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 2", pdev->r2_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Extra frags on rings 3", pdev->r3_frags);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs delivered to HTT", pdev->htt_msdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs delivered to HTT", pdev->htt_mpdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MSDUs delivered to stack", pdev->loc_msdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDUs delivered to stack", pdev->loc_mpdus);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "Oversized AMSUs", pdev->oversize_amsdu);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PHY errors", pdev->phy_errs);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "PHY errors drops", pdev->phy_err_drop);
	len += scnprintf(buf + len, buf_len - len, "%30s %10d\n",
			 "MPDU errors (FCS, MIC, ENC)", pdev->mpdu_errs);
	*length = len;
}

static void
ath12k_wmi_fw_pdev_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      char *buf, u32 *length)
{
	const struct ath12k_fw_stats_pdev *pdev;
	u32 len = *length;

	pdev = list_first_entry_or_null(&fw_stats->pdevs,
					struct ath12k_fw_stats_pdev, list);
	if (!pdev) {
		ath12k_warn(ar->ab, "failed to get pdev stats\n");
		return;
	}

	ath12k_wmi_fw_pdev_base_stats_dump(pdev, buf, &len,
					   ar->ab->fw_soc_drop_count);
	ath12k_wmi_fw_pdev_tx_stats_dump(pdev, buf, &len);
	ath12k_wmi_fw_pdev_rx_stats_dump(pdev, buf, &len);

	*length = len;
}

void ath12k_wmi_fw_stats_dump(struct ath12k *ar,
			      struct ath12k_fw_stats *fw_stats,
			      u32 stats_id, char *buf)
{
	u32 len = 0;
	u32 buf_len = ATH12K_FW_STATS_BUF_SIZE;

	spin_lock_bh(&ar->data_lock);

	switch (stats_id) {
	case WMI_REQUEST_VDEV_STAT:
		ath12k_wmi_fw_vdev_stats_dump(ar, fw_stats, buf, &len);
		break;
	case WMI_REQUEST_BCN_STAT:
		ath12k_wmi_fw_bcn_stats_dump(ar, fw_stats, buf, &len);
		break;
	case WMI_REQUEST_PDEV_STAT:
		ath12k_wmi_fw_pdev_stats_dump(ar, fw_stats, buf, &len);
		break;
	case WMI_REQUEST_VDEV_EXTD_STAT:
		ath12k_wmi_fw_vdev_extd_stats_dump(ar, fw_stats, buf, &len);
		break;
	default:
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	if (len >= buf_len)
		buf[len - 1] = 0;
	else
		buf[len] = 0;

	ath12k_fw_stats_reset(ar);
}

static void
ath12k_wmi_pull_vdev_stats(const struct wmi_vdev_stats_params *src,
			   struct ath12k_fw_stats_vdev *dst)
{
	int i;

	dst->vdev_id = le32_to_cpu(src->vdev_id);
	dst->beacon_snr = le32_to_cpu(src->beacon_snr);
	dst->data_snr = le32_to_cpu(src->data_snr);
	dst->num_rx_frames = le32_to_cpu(src->num_rx_frames);
	dst->num_rts_fail = le32_to_cpu(src->num_rts_fail);
	dst->num_rts_success = le32_to_cpu(src->num_rts_success);
	dst->num_rx_err = le32_to_cpu(src->num_rx_err);
	dst->num_rx_discard = le32_to_cpu(src->num_rx_discard);
	dst->num_tx_not_acked = le32_to_cpu(src->num_tx_not_acked);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames[i] =
			le32_to_cpu(src->num_tx_frames[i]);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames_retries[i] =
			le32_to_cpu(src->num_tx_frames_retries[i]);

	for (i = 0; i < WLAN_MAX_AC; i++)
		dst->num_tx_frames_failures[i] =
			le32_to_cpu(src->num_tx_frames_failures[i]);

	for (i = 0; i < MAX_TX_RATE_VALUES; i++)
		dst->tx_rate_history[i] =
			le32_to_cpu(src->tx_rate_history[i]);

	for (i = 0; i < MAX_TX_RATE_VALUES; i++)
		dst->beacon_rssi_history[i] =
			le32_to_cpu(src->beacon_rssi_history[i]);
}

static void
ath12k_wmi_pull_vdev_extd_stats(const struct wmi_vdev_extd_stats_params *src,
				struct ath12k_fw_stats_vdev_extd *dst)
{
	dst->vdev_id = le32_to_cpu(src->vdev_id);
	dst->fd_succ_cnt = le32_to_cpu(src->fd_succ_cnt);
	dst->fd_fail_cnt = le32_to_cpu(src->fd_fail_cnt);
	dst->unsolicited_prb_succ_cnt =
		le32_to_cpu(src->unsolicited_prb_succ_cnt);
	dst->unsolicited_prb_fail_cnt =
		le32_to_cpu(src->unsolicited_prb_fail_cnt);
	dst->flags = le32_to_cpu(src->flags);
	dst->vdev_tx_power = le32_to_cpu(src->vdev_tx_power);
}

static void
ath12k_wmi_pull_bcn_stats(const struct ath12k_wmi_bcn_stats_params *src,
			  struct ath12k_fw_stats_bcn *dst)
{
	dst->vdev_id = le32_to_cpu(src->vdev_id);
	dst->tx_bcn_succ_cnt = le32_to_cpu(src->tx_bcn_succ_cnt);
	dst->tx_bcn_outage_cnt = le32_to_cpu(src->tx_bcn_outage_cnt);
}

static void
ath12k_wmi_pull_pdev_stats_base(const struct ath12k_wmi_pdev_base_stats_params *src,
				struct ath12k_fw_stats_pdev *dst)
{
	dst->ch_noise_floor = a_sle32_to_cpu(src->chan_nf);
	dst->tx_frame_count = __le32_to_cpu(src->tx_frame_count);
	dst->rx_frame_count = __le32_to_cpu(src->rx_frame_count);
	dst->rx_clear_count = __le32_to_cpu(src->rx_clear_count);
	dst->cycle_count = __le32_to_cpu(src->cycle_count);
	dst->phy_err_count = __le32_to_cpu(src->phy_err_count);
	dst->chan_tx_power = __le32_to_cpu(src->chan_tx_pwr);
}

static void
ath12k_wmi_pull_pdev_stats_tx(const struct ath12k_wmi_pdev_tx_stats_params *src,
			      struct ath12k_fw_stats_pdev *dst)
{
	dst->comp_queued = a_sle32_to_cpu(src->comp_queued);
	dst->comp_delivered = a_sle32_to_cpu(src->comp_delivered);
	dst->msdu_enqued = a_sle32_to_cpu(src->msdu_enqued);
	dst->mpdu_enqued = a_sle32_to_cpu(src->mpdu_enqued);
	dst->wmm_drop = a_sle32_to_cpu(src->wmm_drop);
	dst->local_enqued = a_sle32_to_cpu(src->local_enqued);
	dst->local_freed = a_sle32_to_cpu(src->local_freed);
	dst->hw_queued = a_sle32_to_cpu(src->hw_queued);
	dst->hw_reaped = a_sle32_to_cpu(src->hw_reaped);
	dst->underrun = a_sle32_to_cpu(src->underrun);
	dst->tx_abort = a_sle32_to_cpu(src->tx_abort);
	dst->mpdus_requed = a_sle32_to_cpu(src->mpdus_requed);
	dst->tx_ko = __le32_to_cpu(src->tx_ko);
	dst->data_rc = __le32_to_cpu(src->data_rc);
	dst->self_triggers = __le32_to_cpu(src->self_triggers);
	dst->sw_retry_failure = __le32_to_cpu(src->sw_retry_failure);
	dst->illgl_rate_phy_err = __le32_to_cpu(src->illgl_rate_phy_err);
	dst->pdev_cont_xretry = __le32_to_cpu(src->pdev_cont_xretry);
	dst->pdev_tx_timeout = __le32_to_cpu(src->pdev_tx_timeout);
	dst->pdev_resets = __le32_to_cpu(src->pdev_resets);
	dst->stateless_tid_alloc_failure =
		__le32_to_cpu(src->stateless_tid_alloc_failure);
	dst->phy_underrun = __le32_to_cpu(src->phy_underrun);
	dst->txop_ovf = __le32_to_cpu(src->txop_ovf);
}

static void
ath12k_wmi_pull_pdev_stats_rx(const struct ath12k_wmi_pdev_rx_stats_params *src,
			      struct ath12k_fw_stats_pdev *dst)
{
	dst->mid_ppdu_route_change =
		a_sle32_to_cpu(src->mid_ppdu_route_change);
	dst->status_rcvd = a_sle32_to_cpu(src->status_rcvd);
	dst->r0_frags = a_sle32_to_cpu(src->r0_frags);
	dst->r1_frags = a_sle32_to_cpu(src->r1_frags);
	dst->r2_frags = a_sle32_to_cpu(src->r2_frags);
	dst->r3_frags = a_sle32_to_cpu(src->r3_frags);
	dst->htt_msdus = a_sle32_to_cpu(src->htt_msdus);
	dst->htt_mpdus = a_sle32_to_cpu(src->htt_mpdus);
	dst->loc_msdus = a_sle32_to_cpu(src->loc_msdus);
	dst->loc_mpdus = a_sle32_to_cpu(src->loc_mpdus);
	dst->oversize_amsdu = a_sle32_to_cpu(src->oversize_amsdu);
	dst->phy_errs = a_sle32_to_cpu(src->phy_errs);
	dst->phy_err_drop = a_sle32_to_cpu(src->phy_err_drop);
	dst->mpdu_errs = a_sle32_to_cpu(src->mpdu_errs);
}

static int ath12k_wmi_tlv_fw_stats_data_parse(struct ath12k_base *ab,
					      struct wmi_tlv_fw_stats_parse *parse,
					      const void *ptr,
					      u16 len)
{
	const struct wmi_stats_event *ev = parse->ev;
	struct ath12k_fw_stats *stats = parse->stats;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta;
	int i, ret = 0;
	const void *data = ptr;

	if (!ev) {
		ath12k_warn(ab, "failed to fetch update stats ev");
		return -EPROTO;
	}

	if (!stats)
		return -EINVAL;

	rcu_read_lock();

	stats->pdev_id = le32_to_cpu(ev->pdev_id);
	ar = ath12k_mac_get_ar_by_pdev_id(ab, stats->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in update stats event\n",
			    le32_to_cpu(ev->pdev_id));
		ret = -EPROTO;
		goto exit;
	}

	for (i = 0; i < le32_to_cpu(ev->num_vdev_stats); i++) {
		const struct wmi_vdev_stats_params *src;
		struct ath12k_fw_stats_vdev *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		arvif = ath12k_mac_get_arvif(ar, le32_to_cpu(src->vdev_id));
		if (arvif) {
			sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
							   arvif->bssid,
							   NULL);
			if (sta) {
				ahsta = ath12k_sta_to_ahsta(sta);
				arsta = &ahsta->deflink;
				arsta->rssi_beacon = le32_to_cpu(src->beacon_snr);
				ath12k_dbg(ab, ATH12K_DBG_WMI,
					   "wmi stats vdev id %d snr %d\n",
					   src->vdev_id, src->beacon_snr);
			} else {
				ath12k_dbg(ab, ATH12K_DBG_WMI,
					   "not found station bssid %pM for vdev stat\n",
					   arvif->bssid);
			}
		}

		data += sizeof(*src);
		len -= sizeof(*src);
		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;
		ath12k_wmi_pull_vdev_stats(src, dst);
		stats->stats_id = WMI_REQUEST_VDEV_STAT;
		list_add_tail(&dst->list, &stats->vdevs);
	}
	for (i = 0; i < le32_to_cpu(ev->num_bcn_stats); i++) {
		const struct ath12k_wmi_bcn_stats_params *src;
		struct ath12k_fw_stats_bcn *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		data += sizeof(*src);
		len -= sizeof(*src);
		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;
		ath12k_wmi_pull_bcn_stats(src, dst);
		stats->stats_id = WMI_REQUEST_BCN_STAT;
		list_add_tail(&dst->list, &stats->bcn);
	}
	for (i = 0; i < le32_to_cpu(ev->num_pdev_stats); i++) {
		const struct ath12k_wmi_pdev_stats_params *src;
		struct ath12k_fw_stats_pdev *dst;

		src = data;
		if (len < sizeof(*src)) {
			ret = -EPROTO;
			goto exit;
		}

		stats->stats_id = WMI_REQUEST_PDEV_STAT;

		data += sizeof(*src);
		len -= sizeof(*src);

		dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
		if (!dst)
			continue;

		ath12k_wmi_pull_pdev_stats_base(&src->base, dst);
		ath12k_wmi_pull_pdev_stats_tx(&src->tx, dst);
		ath12k_wmi_pull_pdev_stats_rx(&src->rx, dst);
		list_add_tail(&dst->list, &stats->pdevs);
	}

exit:
	if (ret)
		ath12k_fw_stats_free(stats);
	rcu_read_unlock();
	return ret;
}

static int ath12k_wmi_tlv_rssi_chain_parse(struct ath12k_base *ab,
					   u16 tag, u16 len,
					   const void *ptr, void *data)
{
	const struct wmi_rssi_stat_params *stats_rssi = ptr;
	struct wmi_tlv_fw_stats_parse *parse = data;
	const struct wmi_stats_event *ev = parse->ev;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k *ar;
	int pdev_id;
	int vdev_id;
	int j;

	if (tag != WMI_TAG_RSSI_STATS)
		return -EPROTO;

	pdev_id = le32_to_cpu(ev->pdev_id);
	vdev_id = le32_to_cpu(stats_rssi->vdev_id);
	guard(rcu)();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in rssi chain parse\n",
			    pdev_id);
		return -EPROTO;
	}

	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "not found vif for vdev id %d\n", vdev_id);
		return -EPROTO;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "stats bssid %pM vif %p\n",
		   arvif->bssid, arvif->ahvif->vif);

	sta = ieee80211_find_sta_by_ifaddr(ath12k_ar_to_hw(ar),
					   arvif->bssid,
					   NULL);
	if (!sta) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "not found station of bssid %pM for rssi chain\n",
			   arvif->bssid);
		return -EPROTO;
	}

	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = &ahsta->deflink;

	BUILD_BUG_ON(ARRAY_SIZE(arsta->chain_signal) >
		     ARRAY_SIZE(stats_rssi->rssi_avg_beacon));

	for (j = 0; j < ARRAY_SIZE(arsta->chain_signal); j++)
		arsta->chain_signal[j] = le32_to_cpu(stats_rssi->rssi_avg_beacon[j]);

	return 0;
}

static int ath12k_wmi_tlv_vdev_extd_stats(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct wmi_tlv_fw_stats_parse *parse = data;
	const struct wmi_stats_event *ev = parse->ev;
	struct ath12k_fw_stats *stats = parse->stats;
	const struct wmi_vdev_extd_stats_params *src;
	struct ath12k_fw_stats_vdev_extd *dst;
	struct ath12k *ar;

	guard(rcu)();
	stats->pdev_id = le32_to_cpu(ev->pdev_id);
	ar = ath12k_mac_get_ar_by_pdev_id(ab, stats->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in vdev extd stats event\n",
			    le32_to_cpu(stats->pdev_id));
		return -EPROTO;
	}

	src = (struct wmi_vdev_extd_stats_params *)ptr;
	if (len < sizeof(*src)) {
		ath12k_err(ab,
			   "vdev extd parse failure: %u bytes left, %zu expected\n",
			   len, sizeof(*src));
		return -EPROTO;
	}

	dst = kzalloc(sizeof(*dst), GFP_ATOMIC);
	if (!dst)
		return -ENOMEM;

	ath12k_wmi_pull_vdev_extd_stats(src, dst);
	list_add_tail(&dst->list, &stats->vdev_extds);

	return 0;
}

static int ath12k_wmi_tlv_fw_stats_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_tlv_fw_stats_parse *parse = data;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_STATS_EVENT:
		parse->ev = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		ret = ath12k_wmi_tlv_fw_stats_data_parse(ab, parse, ptr, len);
		break;
	case WMI_TAG_PER_CHAIN_RSSI_STATS:
		parse->rssi = ptr;
		if (le32_to_cpu(parse->ev->stats_id) & WMI_REQUEST_RSSI_PER_CHAIN_STAT)
			parse->rssi_num = le32_to_cpu(parse->rssi->num_per_chain_rssi);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (parse->rssi_num && !parse->chain_rssi_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_rssi_chain_parse,
						  parse);
			if (ret)
				return ret;

			parse->chain_rssi_done = true;
		} else if (!parse->rssi_stats_done) {
			parse->rssi_stats_done = true;
		} else if (!parse->congestion_stats_done) {
			parse->congestion_stats_done = true;
		} else if (!parse->peer_extd2_stats_done) {
			parse->peer_extd2_stats_done = true;
		} else if (!parse->pmf_bcn_protect_stats_done) {
			parse->pmf_bcn_protect_stats_done = true;
		} else if (!parse->vdev_extd_stats_done) {
			if (len)
				parse->stats->stats_id = WMI_REQUEST_VDEV_EXTD_STAT;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_vdev_extd_stats,
						  parse);
			if (ret)
				return ret;

			parse->vdev_extd_stats_done = true;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int ath12k_wmi_pull_fw_stats(struct ath12k_base *ab, struct sk_buff *skb,
				    struct ath12k_fw_stats *stats)
{
	struct wmi_tlv_fw_stats_parse parse = {};

	stats->stats_id = 0;
	parse.stats = stats;

	return ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				   ath12k_wmi_tlv_fw_stats_parse,
				   &parse);
}

static void ath12k_wmi_fw_stats_process(struct ath12k *ar,
					struct ath12k_fw_stats *stats)
{
	struct ath12k_base *ab = ar->ab;
	bool is_end = true;
	size_t total_vdevs_started = 0;

	if (stats->stats_id == WMI_REQUEST_VDEV_STAT) {
		if (list_empty(&stats->vdevs)) {
			ath12k_warn(ab, "empty vdev stats");
			return;
		}
		/* FW sends VDEV stats only for the requested PDEV,
		 * hence limit until the count of VDEVs started within this PDEV.
		 */
		total_vdevs_started = ar->num_started_vdevs;

		if (total_vdevs_started)
			is_end = ((++ar->fw_stats.num_vdev_recvd) ==
				  total_vdevs_started);

		list_splice_tail_init(&stats->vdevs,
				      &ar->fw_stats.vdevs);

		if (is_end)
			complete(&ar->fw_stats_done);

		return;
	}

	if (stats->stats_id == WMI_REQUEST_BCN_STAT) {
		if (list_empty(&stats->bcn)) {
			ath12k_warn(ab, "empty beacon stats");
			return;
		}
		/* Mark end until we reached the count of all started VDEVs
		 * within the PDEV
		 */
		if (ar->num_started_vdevs)
			is_end = ((++ar->fw_stats.num_bcn_recvd) ==
				  ar->num_started_vdevs);

		list_splice_tail_init(&stats->bcn,
				      &ar->fw_stats.bcn);

		if (is_end)
			complete(&ar->fw_stats_done);
	}

	if (stats->stats_id == WMI_REQUEST_VDEV_EXTD_STAT) {
		if (list_empty(&stats->vdev_extds)) {
			ath12k_warn(ab, "empty vdev extd stats");
			return;
		}
		/* FW sends VDEV extd stats only for the requested PDEV,
		 * hence limit until the count of VDEVs started within this PDEV.
		 */
		total_vdevs_started = ar->num_started_vdevs;

		if (total_vdevs_started)
			is_end = ((++ar->fw_stats.num_vdev_extd_recvd) ==
				  total_vdevs_started);

		list_splice_tail_init(&stats->vdev_extds,
				      &ar->fw_stats.vdev_extds);

		if (is_end)
			complete(&ar->fw_stats_done);

		return;
	}
}

static void ath12k_update_stats_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_fw_stats stats = {};
	struct ath12k *ar;
	bool is_timer_req = false;
	int ret;

	INIT_LIST_HEAD(&stats.pdevs);
	INIT_LIST_HEAD(&stats.vdevs);
	INIT_LIST_HEAD(&stats.bcn);
	INIT_LIST_HEAD(&stats.vdev_extds);

	ret = ath12k_wmi_pull_fw_stats(ab, skb, &stats);
	if (ret) {
		ath12k_warn(ab, "failed to pull fw stats: %d\n", ret);
		goto free;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "event update stats");

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, stats.pdev_id);
	if (!ar) {
		rcu_read_unlock();
		ath12k_warn(ab, "failed to get ar for pdev_id %d: %d\n",
			    stats.pdev_id, ret);
		goto free;
	}

	spin_lock_bh(&ar->data_lock);

	/* Handle WMI_REQUEST_PDEV_STAT status update */
	if (stats.stats_id == WMI_REQUEST_PDEV_STAT) {
		struct ath12k_fw_stats_pdev *pdev_entry;

		/* Capture and clear the periodic-request flag. */
		is_timer_req = ar->pdev_stats_timer_req_pending;
		ar->pdev_stats_timer_req_pending = false;

		if (is_timer_req) {
			/* Periodic timer path: only replace the pdev list.
			 * Save vdev/bcn/vdev_extd lists, free all stats
			 * (clears only pdevs since others are detached),
			 * then restore saved lists.
			 */
			LIST_HEAD(saved_vdevs);
			LIST_HEAD(saved_bcn);
			LIST_HEAD(saved_vdev_extds);

			list_splice_init(&ar->fw_stats.vdevs, &saved_vdevs);
			list_splice_init(&ar->fw_stats.bcn, &saved_bcn);
			list_splice_init(&ar->fw_stats.vdev_extds, &saved_vdev_extds);

			ath12k_fw_stats_free(&ar->fw_stats);

			list_splice(&saved_vdevs, &ar->fw_stats.vdevs);
			list_splice(&saved_bcn, &ar->fw_stats.bcn);
			list_splice(&saved_vdev_extds, &ar->fw_stats.vdev_extds);
		} else {
			/* Synchronous path: free all stats as before. */
			ath12k_fw_stats_free(&ar->fw_stats);
		}

		list_splice_tail_init(&stats.pdevs, &ar->fw_stats.pdevs);

		/* Cache the latest pdev counters so periodic-timer consumers
		 * (chan_util, survey, noise floor) can read fresh values
		 * without issuing a new blocking WMI request.
		 */
		pdev_entry = list_first_entry_or_null(&ar->fw_stats.pdevs,
						      struct ath12k_fw_stats_pdev,
						      list);
		if (pdev_entry) {
			ar->pdev_prev_rx_clear_count = ar->pdev_rx_clear_count;
			ar->pdev_prev_cycle_count    = ar->pdev_cycle_count;
			ar->pdev_rx_clear_count = pdev_entry->rx_clear_count;
			ar->pdev_cycle_count    = pdev_entry->cycle_count;
			ar->pdev_chan_nf        = pdev_entry->ch_noise_floor;
		} else {
			ath12k_warn(ar->ab, "pdev stats timer: event received but pdev_entry is NULL\n");
		}

		if (!is_timer_req)
			complete(&ar->fw_stats_done);
		goto complete;
	}

	/* Handle WMI_REQUEST_VDEV_STAT and WMI_REQUEST_BCN_STAT updates. */
	ath12k_wmi_fw_stats_process(ar, &stats);

complete:
	/* Only signal fw_stats_complete for synchronous requests. */
	if (!is_timer_req)
		complete(&ar->fw_stats_complete);
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();

	/* Since the stats's pdev, vdev and beacon list are spliced and reinitialised
	 * at this point, no need to free the individual list.
	 */
	return;

free:
	ath12k_fw_stats_free(&stats);
}

/* PDEV_CTL_FAILSAFE_CHECK_EVENT is received from FW when the frequency scanned
 * is not part of BDF CTL(Conformance test limits) table entries.
 */
static void ath12k_pdev_ctl_failsafe_check_event(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_ctl_failsafe_chk_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev ctl failsafe check ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev ctl failsafe check ev status %d\n",
		   ev->ctl_failsafe_status);

	/* If ctl_failsafe_status is set to 1 FW will max out the Transmit power
	 * to 10 dBm else the CTL power entry in the BDF would be picked up.
	 */
	if (ev->ctl_failsafe_status != 0)
		ath12k_warn(ab, "pdev ctl failsafe failure status %d",
			    ev->ctl_failsafe_status);

	kfree(tb);
}

static void ath12k_pdev_check_cal_version_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_pdev_check_cal_version_event *ev;
	char board_mcn[WMI_BOARD_MCN_STRING_BUF_SIZE];
	const void **tb;
	u32 cal_status;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse cal version event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CHECK_CAL_VERSION_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev cal version check ev\n");
		kfree(tb);
		return;
	}

	cal_status = le32_to_cpu(ev->cal_status);
	memcpy(board_mcn, ev->board_mcn_detail, sizeof(board_mcn));
	board_mcn[WMI_BOARD_MCN_STRING_MAX_SIZE] = '\0';

	ath12k_info(ab, "****************** CAl DATA Version Details ********************\n");
	if (cal_status != WMI_CALIBRATION_NO_FEATURE) {
		ath12k_info(ab, "Current Meta Cal Version: 0x%x\n",
			    le32_to_cpu(ev->software_cal_version));
		ath12k_info(ab, "Board (MCN : %s) Cal Version: 0x%x\n",
			    board_mcn, le32_to_cpu(ev->board_cal_version));

		if (cal_status == WMI_CALIBRATION_OK)
			ath12k_info(ab, "Calibration Status: OK\n");
		else
			ath12k_info(ab, "Calibration Status: NOT OK\n");

		ath12k_info(ab, "Note: Please cross check board's MCN\n");
	} else {
		ath12k_info(ab, "Board was not calibrated with this feature\n");
	}

	kfree(tb);
}

static void ath12k_wmi_pdev_tgtr2p_table_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const struct wmi_pdev_set_tgtr2p_table_event *ev;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tgtr2p event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_SET_TGTR2P_TABLE_EVENT_FIXED_PARAM];
	if (!ev) {
		ath12k_warn(ab, "tgtr2p event: missing fixed param TLV\n");
		kfree(tb);
		return;
	}

	switch (le32_to_cpu(ev->status)) {
	case WMI_PDEV_TGTR2P_SUCCESS:
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "tgtr2p_table_event: pdev_id=%u status=SUCCESS\n",
			   le32_to_cpu(ev->pdev_id));
		break;
	case WMI_PDEV_TGTR2P_SUCCESS_WAITING_FOR_END_OF_UPDATE:
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "tgtr2p_table_event: pdev_id=%u status=WAITING_FOR_END_OF_UPDATE\n",
			   le32_to_cpu(ev->pdev_id));
		break;
	default:
		ath12k_warn(ab,
			    "tgtr2p_table_event: pdev_id=%u status=%u (error)\n",
			    le32_to_cpu(ev->pdev_id), le32_to_cpu(ev->status));
		break;
	}

	kfree(tb);
}

static int ath12k_wmi_dcs_intf_subtlv_parser(struct ath12k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	int ret = 0;
	struct wmi_dcs_awgn_info *awgn_info;
	struct wmi_dcs_cw_info *cw_info;
	struct wmi_dcs_obss_info *obss_info;
#ifdef CPTCFG_QCN_EXTN
	struct wmi_dcs_wlan_interference_stats_ev *wlan_info;
	struct wmi_dcs_wlan_interference_stats *tmp;
#endif /* CPTCFG_QCN_EXTN */

	switch (tag) {
	case WMI_TAG_DCS_AWGN_INT_TYPE:
		if (len < sizeof(*awgn_info))
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "AWGN Info TLV truncated: len=%u expected=%zu\n",
				   len, sizeof(*awgn_info));

		memcpy(data, ptr, min_t(size_t, len, sizeof(*awgn_info)));
		awgn_info = (struct wmi_dcs_awgn_info *)data;
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "AWGN Info: channel width: %d, chan freq: %d, center_freq0: %d, center_freq1: %d, bw_intf_bitmap: %d\n",
			   awgn_info->channel_width, awgn_info->chan_freq, awgn_info->center_freq0, awgn_info->center_freq1,
			   awgn_info->chan_bw_interference_bitmap);
		break;
	case WMI_TAG_WLAN_DCS_CW_INT:
		if (len < sizeof(*cw_info))
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "CW Info TLV truncated: len=%u expected=%zu\n",
				   len, sizeof(*cw_info));

		memcpy(data, ptr, min_t(size_t, len, sizeof(*cw_info)));
		cw_info = (struct wmi_dcs_cw_info *)data;
		ath12k_dbg(ab, ATH12K_DBG_WMI, "CW Info: channel=%d", cw_info->channel);
		break;
	case WMI_TAG_DCS_OBSS_INT_TYPE:
		if (len < sizeof(*obss_info))
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "OBSS Info TLV truncated: len=%u expected=%zu\n",
				   len, sizeof(*obss_info));

		memcpy(data, ptr, min_t(size_t, len, sizeof(*obss_info)));
		obss_info = (struct wmi_dcs_obss_info *)data;
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "OBSS Info: width: %d, cf: %d, cf0: %d, cf1: %d, bmap: %d\n",
			   obss_info->channel_width, obss_info->chan_freq,
			   obss_info->center_freq0, obss_info->center_freq1,
			   obss_info->chan_bw_interference_bitmap);
		break;
	case WMI_TAG_ATH_DCS_WLAN_INT_STAT:
#ifdef CPTCFG_QCN_EXTN
		wlan_info = (struct wmi_dcs_wlan_interference_stats_ev *)ptr;
		tmp = (struct wmi_dcs_wlan_interference_stats *)data;
		tmp->reg_tsf32 = le32_to_cpu(wlan_info->reg_tsf32);
		tmp->last_ack_rssi = le32_to_cpu(wlan_info->last_ack_rssi);
		tmp->tx_waste_time = le32_to_cpu(wlan_info->tx_waste_time);
		tmp->rx_time = le32_to_cpu(wlan_info->rx_time);
		tmp->phyerr_cnt = le32_to_cpu(wlan_info->phyerr_cnt);
		tmp->listen_time = le32_to_cpu(wlan_info->listen_time);
		tmp->reg_tx_frame_cnt = le32_to_cpu(wlan_info->reg_tx_frame_cnt);
		tmp->reg_rx_frame_cnt = le32_to_cpu(wlan_info->reg_rx_frame_cnt);
		tmp->reg_rxclr_cnt = le32_to_cpu(wlan_info->reg_rxclr_cnt);
		tmp->reg_cycle_cnt = le32_to_cpu(wlan_info->reg_cycle_cnt);
		tmp->reg_rxclr_ext_cnt = le32_to_cpu(wlan_info->reg_rxclr_ext_cnt);
		tmp->reg_ofdm_phyerr_cnt = le32_to_cpu(wlan_info->reg_ofdm_phyerr_cnt);
		tmp->reg_cck_phyerr_cnt = le32_to_cpu(wlan_info->reg_cck_phyerr_cnt);
		tmp->chan_nf = le32_to_cpu(wlan_info->chan_nf);
		tmp->my_bss_rx_cycle_count =
			le32_to_cpu(wlan_info->my_bss_rx_cycle_count);
#endif /* CPTCFG_QCN_EXTN */
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid tag for wmi dcs interference in subtlvs\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifndef CPTCFG_QCN_EXTN
static
#endif
int ath12k_wmi_dcs_event_parser(struct ath12k_base *ab,
				u16 tag, u16 len,
				const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi dcs awgn event tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_TAG_DCS_INTERFERENCE_EVENT:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_dcs_intf_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi dcs interference event\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

bool ath12k_wmi_validate_dcs_awgn_info(struct ath12k *ar, struct wmi_dcs_awgn_info *awgn_info)
{
	spin_lock_bh(&ar->data_lock);

	if (!ar->rx_channel) {
		spin_unlock_bh(&ar->data_lock);
		return false;
	}

	if (awgn_info->chan_freq != ar->rx_channel->center_freq) {
		spin_unlock_bh(&ar->data_lock);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "dcs interference event received with wrong channel %d",awgn_info->chan_freq);
		return false;
	}
	spin_unlock_bh(&ar->data_lock);

	switch (awgn_info->channel_width) {
	case WMI_HOST_CHAN_WIDTH_20:
		if (awgn_info->chan_bw_interference_bitmap > WMI_DCS_SEG_PRI20) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 20MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_40:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 40MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_80:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_160:
	case WMI_HOST_CHAN_WIDTH_80P80:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40 |
							      WMI_DCS_SEG_SEC80)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 80P80/160MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
		break;
	case WMI_HOST_CHAN_WIDTH_320:
		if (awgn_info->chan_bw_interference_bitmap > (WMI_DCS_SEG_PRI20 |
							      WMI_DCS_SEG_SEC20 |
							      WMI_DCS_SEG_SEC40 |
							      WMI_DCS_SEG_SEC80 |
							      WMI_DCS_SEG_SEC160)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "dcs interference event received with wrong chan width bmap %d for 320MHz",
				   awgn_info->chan_bw_interference_bitmap);
			return false;
		}
        break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "dcs interference event received with unknown channel width %d",
			   awgn_info->channel_width);
		return false;
	}
	return true;
}

static void
ath12k_wmi_dcs_awgn_interference_event(struct ath12k_base *ab,
				       struct sk_buff *skb,
				       u32 pdev_id)
{
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	struct wmi_dcs_awgn_info awgn_info = {};
	struct cfg80211_chan_def *chandef;
	struct ath12k *ar;
	int ret;

	if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_warn(ab, "firmware doesn't support awgn interference, so dropping dcs interference ev\n");
		return;
	}

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dcs_event_parser,
				  &awgn_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse awgn tlv %d\n", ret);
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "awgn detected in invalid pdev id(%d)\n",
			    pdev_id);
		goto exit;
	}
	if (!ar->supports_6ghz) {
		ath12k_warn(ab, "pdev does not supports 6G, so dropping dcs interference event\n");
		goto exit;
	}

	spin_lock_bh(&ar->data_lock);
	if (ar->awgn_intf_handling_in_prog) {
		spin_unlock_bh(&ar->data_lock);
		rcu_read_unlock();
		return;
	}
	spin_unlock_bh(&ar->data_lock);

	if (!ath12k_wmi_validate_dcs_awgn_info(ar, &awgn_info)) {
		ath12k_warn(ab, "Invalid DCS AWGN TLV - Skipping event");
		goto exit;
	}

	ath12k_info(ab, "Interface(pdev %d) : AWGN interference detected\n",
		    pdev_id);

	arg.ar = ar;
	arg.chanctx_conf = NULL;
	ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
					    ath12k_mac_get_any_chanctx_conf_iter, &arg);
	if (!arg.chanctx_conf) {
		ath12k_warn(ab, "failed to find valid chanctx_conf in AWGN event\n");
		goto exit;
	}
	chandef = &arg.chanctx_conf->def;
	if (!chandef) {
		ath12k_warn(ab, "chandef is not available\n");
		goto exit;
	}
	ar->awgn_chandef = *chandef;

	ieee80211_awgn_detected(ath12k_ar_to_hw(ar), awgn_info.chan_bw_interference_bitmap,
				chandef->chan);

	spin_lock_bh(&ar->data_lock);
	ar->awgn_intf_handling_in_prog = true;
	ar->chan_bw_interference_bitmap = awgn_info.chan_bw_interference_bitmap;
	spin_unlock_bh(&ar->data_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "AWGN : Interference handling started\n");
exit:
	rcu_read_unlock();
}

static void
ath12k_wmi_process_csa_switch_count_event(struct ath12k_base *ab,
					  const struct ath12k_wmi_pdev_csa_event *ev,
					  const u32 *vdev_ids)
{
	u32 current_switch_count = le32_to_cpu(ev->current_switch_count);
	u32 num_vdevs = le32_to_cpu(ev->num_vdevs);
	struct ieee80211_bss_conf *conf;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	int i;

	rcu_read_lock();
	for (i = 0; i < num_vdevs; i++) {
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_ids[i]);

		if (!arvif) {
			ath12k_warn(ab, "Recvd csa status for unknown vdev %d",
				    vdev_ids[i]);
			continue;
		}
		ahvif = arvif->ahvif;

		if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)
			continue;

		if (arvif->link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
			ath12k_warn(ab, "Invalid CSA switch count even link id: %d\n",
				    arvif->link_id);
			continue;
		}

		conf = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);
		if (!conf) {
			ath12k_warn(ab, "unable to access bss link conf in process csa for vif %pM link %u\n",
				    ahvif->vif->addr, arvif->link_id);
			continue;
		}

#ifdef CPTCFG_QCN_EXTN
		if (ahvif->vif->type == NL80211_IFTYPE_AP && !arvif->is_started)
			continue;

		if (!conf->csa_active)
#else
		if (!arvif->is_up || !conf->csa_active)
#endif
			continue;

		/* Finish CSA when counter reaches zero */
		if (!current_switch_count) {
			ath12k_dbg_level(ab, ATH12K_DBG_WMI, ATH12K_DBG_L3,
					  "CSA finish: vdev_id %u link_id %u vif %pM radio_idx %d band %d vdev_type %d host_count %u num_vdevs %u csa_active %d\n",
					  vdev_ids[i], arvif->link_id,
					  ahvif->vif->addr,
					  arvif->ar ? arvif->ar->radio_idx : -1,
					  conf->chanreq.oper.chan ?
					  conf->chanreq.oper.chan->band : -1,
					  ahvif->vdev_type,
					  arvif->current_cntdown_counter,
					  num_vdevs, conf->csa_active);
			ieee80211_csa_finish(ahvif->vif, arvif->link_id);
			arvif->current_cntdown_counter = 0;
		} else if (current_switch_count > 1) {
			/* If the count in event is not what we expect, don't update the
			 * mac80211 count. Since during beacon Tx failure, count in the
			 * firmware will not decrement and this event will come with the
			 * previous count value again
			 */
			if (current_switch_count != arvif->current_cntdown_counter)
				continue;

			arvif->current_cntdown_counter =
				ieee80211_beacon_update_cntdwn(ahvif->vif,
							       arvif->link_id);
		}
	}
	rcu_read_unlock();
}

static int ath12k_wmi_csa_tlv_iter(struct ath12k_base *ab, u16 tag, u16 len,
				   const void *ptr, void *data)
{
	struct ath12k_wmi_csa_vdev_ids_parse_state *s = data;

	if (tag == WMI_TAG_ARRAY_UINT32)
		s->vdev_ids_len = len;

	return 0;
}

static void
ath12k_wmi_pdev_csa_switch_count_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	struct ath12k_wmi_csa_vdev_ids_parse_state parse_state = {};
	const void **tb;
	const struct ath12k_wmi_pdev_csa_event *ev;
	const u32 *vdev_ids;
	u32 num_vdev_ids;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CSA_SWITCH_COUNT_STATUS_EVENT];
	vdev_ids = tb[WMI_TAG_ARRAY_UINT32];

	if (!ev || !vdev_ids) {
		ath12k_warn(ab, "failed to fetch pdev csa switch count ev");
		kfree(tb);
		return;
	}

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_csa_tlv_iter, &parse_state);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv for vdev ids len: %d\n", ret);
		kfree(tb);
		return;
	}

	if (parse_state.vdev_ids_len % sizeof(*vdev_ids)) {
		ath12k_warn(ab, "invalid vdev_ids tlv len %u\n",
			    parse_state.vdev_ids_len);
		kfree(tb);
		return;
	}

	num_vdev_ids = parse_state.vdev_ids_len / sizeof(*vdev_ids);
	if (num_vdev_ids != ev->num_vdevs) {
		ath12k_warn(ab,
			    "invalid number of vdev_ids (%u) for num_vdevs %u\n",
			    num_vdev_ids, ev->num_vdevs);
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev csa switch count %d for pdev %d, num_vdevs %d",
		   ev->current_switch_count, ev->pdev_id,
		   ev->num_vdevs);

	ath12k_wmi_process_csa_switch_count_event(ab, ev, vdev_ids);

	kfree(tb);
}

static void
ath12k_dfs_calculate_subchannels(struct ath12k_base *ab,
				 const struct ath12k_wmi_pdev_radar_event *radar,
				 bool do_full_bw_nol)
{
	u32 radar_found_freq, sub_channel_cfreq, radar_found_freq_low, radar_found_freq_high;
	struct ath12k_mac_get_any_chanctx_conf_arg arg;
	u16 radar_bitmap = 0, subchannel_count;
	bool chandef_device_present = false;
	struct cfg80211_chan_def *chandef;
	enum nl80211_chan_width width;
	struct ath12k *ar;
	u32 center_freq;
	int i;

	ar = ath12k_mac_get_ar_by_pdev_id(ab, radar->pdev_id);
	if (!ar) {
		ath12k_warn(ab, "Failed to fetch ar for pdev %d\n",
			    radar->pdev_id);
		return;
	}
	arg.ar = ar;
	arg.chanctx_conf = NULL;
	if (!radar->detector_id) {
		ieee80211_iter_chan_contexts_atomic(ath12k_ar_to_hw(ar),
						    ath12k_mac_get_any_chanctx_conf_iter, &arg);
		if (!arg.chanctx_conf) {
			ath12k_warn(ab, "failed to find valid chanctx_conf in radar detected event\n");
			return;
		}
		chandef = &arg.chanctx_conf->def;
		chandef_device_present = cfg80211_chandef_device_present(chandef);
	} else {
		chandef = &ar->agile_chandef;
	}

	if (!chandef) {
		ath12k_warn(ab, "chandef information is not available\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, " Operating freq:%u center_freq1:%u, center_freq2:%u",
		   chandef->chan->center_freq, chandef->center_freq1,chandef->center_freq2);

	if (chandef_device_present)
		width = chandef->width_device;
	else
		width = chandef->width;

	subchannel_count = ath12k_calculate_subchannel_count(width);
	if (!subchannel_count) {
		ath12k_warn(ab, "invalid subchannel count for bandwidth=%d\n", width);
		goto mark_radar;
	}

	if (do_full_bw_nol) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "put all channels in NOL\n");
		goto mark_radar;
	}
	ath12k_dbg(ab, ATH12K_DBG_WMI, "perform channel submarking\n");

	if (chandef_device_present)
		center_freq = chandef->center_freq_device;
	else
		center_freq = chandef->center_freq1;

	radar_found_freq = center_freq + radar->freq_offset;

	radar_found_freq_high = radar_found_freq_low = radar_found_freq;

	if (radar->is_chirp) {
		radar_found_freq_high = radar_found_freq_high + 10;
		radar_found_freq_low  = radar_found_freq_low  - 10;
	}

	sub_channel_cfreq = center_freq - ((subchannel_count-1) * 10);

	for(i=0; i < subchannel_count; i++) {
		if (sub_channel_cfreq >= 5260 &&
		    ((radar_found_freq_low >= sub_channel_cfreq-10 &&
		    radar_found_freq_low <= sub_channel_cfreq+10) ||
		    (radar_found_freq_high >= sub_channel_cfreq-10 &&
		    radar_found_freq_high <= sub_channel_cfreq+10)))
			radar_bitmap |= 1 << i;

		sub_channel_cfreq += 20;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "radar_bitmap:%0x and subchannel_count:%d",
		   radar_bitmap,subchannel_count);

mark_radar:
	if (!radar->detector_id) {
		ieee80211_radar_detected_bitmap(ar->ah->hw, radar_bitmap, chandef->chan);
	} else {
		ar->agile_chandef.radar_bitmap = radar_bitmap;
		ath12k_mac_background_dfs_event(ar, ATH12K_BGDFS_RADAR);
	}
}

/**
 * ath12k_wmi_radar_type_to_str() - Find the radar domain and type strings.
 * @radar_type: Radar type.
 * @radar_domain: Radar domain.
 * @d_name: Output pointer set to the domain name string.
 * @rt_name: Output pointer set to the radar type name string.
 *
 * Selects the per-domain radar_type_id_to_name array for the given radar
 * domain and finds the radar type name for the given radar type.
 *
 * Context: Any context.
 */
static void
ath12k_wmi_radar_type_to_str(u16 radar_type, u8 radar_domain,
			     const char **d_name, const char **rt_name)
{
	const struct radar_type_id_to_name *radar_id_to_name;
	u8 n_rtypes, i;

	*d_name = unknown_str;
	*rt_name = unknown_str;
	if (radar_domain >= ARRAY_SIZE(radar_domain_to_type_arr_tab))
		return;

	radar_id_to_name = radar_domain_to_type_arr_tab[radar_domain].radar_id_to_name;
	if (!radar_id_to_name)
		return;

	n_rtypes = radar_domain_to_type_arr_tab[radar_domain].n_rtypes;
	*d_name = radar_domain_to_type_arr_tab[radar_domain].d_name;
	for (i = 0; i < n_rtypes; i++) {
		if (radar_id_to_name[i].rtype == radar_type) {
			*rt_name = radar_id_to_name[i].rname;
			return;
		}
	}
}

/**
 * ath12k_wmi_parse_dfs_radar_flags() - Parse radar flag TLV payload.
 * @ab: ath12k base pointer.
 * @rf_ev: Pointer to radar flag TLV payload.
 * @do_full_bw_nol: Output flag indicating full-bandwidth NOL request.
 *
 * Parses radar flag fields, updates @do_full_bw_nol and prints the detailed
 * radar-domain, radar-type and radar-rssi in dBm information in the logs.
 *
 * Context: Any context.
 */
static void
ath12k_wmi_parse_dfs_radar_flags(struct ath12k_base *ab,
				 const struct wmi_pdev_radar_flags_param *rf_ev,
				 bool *do_full_bw_nol)
{
	const char *d_name, *rt_name;
	bool is_rssi_flag;
	u8 radar_domain;
	u16 radar_type;
	u32 val;

	val = le32_to_cpu(rf_ev->radar_flags);
	*do_full_bw_nol = u32_get_bits(val, WMI_PDEV_RADAR_FLAGS_DO_FULL_BW_NOL);
	radar_type = u32_get_bits(val, WMI_PDEV_RADAR_FLAGS_RADAR_TYPE);
	radar_domain = u32_get_bits(val, WMI_PDEV_RADAR_FLAGS_RADAR_DOMAIN);
	ath12k_wmi_radar_type_to_str(radar_type, radar_domain, &d_name, &rt_name);
	ath12k_info(ab,
		    "pdev dfs radar event flag: is the full bw nol set: %d domain: %s radar type: %u (%s)\n",
		    *do_full_bw_nol, d_name, radar_type, rt_name);
	is_rssi_flag = test_bit(WMI_SERVICE_RADAR_FLAGS_RSSI_DBM_SUPPORT,
				ab->wmi_ab.svc_map);
	ath12k_info(ab,
		    "pdev dfs radar rssi dbm flags host support %x\n", is_rssi_flag);
	if (is_rssi_flag) {
		s8 rssi = (s8)u32_get_bits(val, WMI_PDEV_RADAR_FLAGS_RADAR_RSSI);

		ath12k_info(ab,
			    "pdev dfs radar event flag: radar RSSI in dBm: %+d\n", rssi);
	}
}

static void
ath12k_wmi_pdev_dfs_radar_detected_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_pdev_radar_flags_param *rf_ev;
	const struct ath12k_wmi_pdev_radar_event *ev;
	struct ath12k *ar;
	bool do_full_bw_nol = false;
	bool is_radar_flag;
	const struct wmi_tlv *tlv;
	u16 tlv_tag;
	u32 len = 0;
	void *ptr;

	ptr = skb->data;

	len += sizeof(*ev) + TLV_HDR_SIZE;
	if (skb->len < len) {
		ath12k_warn(ab, "Radar event is of incorrect length\n");
		return;
	}

	tlv = ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);

	ptr += sizeof(*tlv);
	if (tlv_tag != WMI_TAG_PDEV_DFS_RADAR_DETECTION_EVENT) {
		ath12k_warn(ab, "pdev dfs event received with wrong tag %x\n", tlv_tag);
		return;
	}
	ev = ptr;
	ptr += sizeof(*ev);

	is_radar_flag = test_bit(WMI_TLV_SERVICE_RADAR_FLAGS_SUPPORT, ab->wmi_ab.svc_map);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "pdev dfs radar event found\n");
	ath12k_dbg(ab, ATH12K_DBG_WMI, "pdev dfs radar flags host support %x\n",
		   is_radar_flag);
	if (is_radar_flag) {
		/* Expect an array TLV containing a single radar flags param TLV */
		len += sizeof(*tlv) + sizeof(*tlv) + sizeof(*rf_ev);
		if (skb->len < len) {
			ath12k_warn(ab, "pdev dfs radar flag tlv size invalid\n");
			return;
		}

		/* Skip Array TLV Tag */
		ptr += sizeof(*tlv);
		tlv = ptr;
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		ptr += sizeof(*tlv);
		if (tlv_tag != WMI_TAG_PDEV_DFS_RADAR_FLAGS) {
			ath12k_warn(ab, "pdev dfs radar flag tlv received with wrong tag\n");
			return;
		}

		rf_ev = ptr;
		ath12k_wmi_parse_dfs_radar_flags(ab, rf_ev, &do_full_bw_nol);
	}

	ath12k_info(ab,
		    "pdev dfs radar detected on pdev %d, detection mode %d, chan freq %d, chan_width %d, detector id %d, seg id %d, timestamp %d, chirp %d, freq offset %d, sidx %d",
		    ev->pdev_id, ev->detection_mode, ev->chan_freq,
		    ev->chan_width, ev->detector_id, ev->segment_id,
		    ev->timestamp, ev->is_chirp, ev->freq_offset, ev->sidx);

	rcu_read_lock();

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(ev->pdev_id));
	if (!ar) {
		ath12k_warn(ab, "radar detected in invalid pdev %d\n",
			    ev->pdev_id);
		rcu_read_unlock();
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "DFS Radar Detected in pdev %d\n",
		   ev->pdev_id);

	ar->radar_detect_count++;
	do_full_bw_nol |= !READ_ONCE(ar->dfs_sub_channel_marking);

	if (ar->dfs_block_radar_events)
		ath12k_info(ab, "DFS Radar detected, but ignored as requested\n");
	else
		ath12k_dfs_calculate_subchannels(ab, ev, do_full_bw_nol);

	rcu_read_unlock();
}

static void
ath12k_wmi_dcs_interference_event(struct ath12k_base *ab,
				  struct sk_buff *skb)
{
	const struct wmi_dcs_interference_ev *dcs_intf_ev;
	const struct wmi_tlv *tlv;
	u32 pdev_id, interference_type;
	u16 tlv_tag;
	u8 *ptr;

	ptr = skb->data;

	if (skb->len < (sizeof(*dcs_intf_ev) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "dcs interference event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_DCS_INTERFERENCE_EVENT) {
		dcs_intf_ev = (struct wmi_dcs_interference_ev *)ptr;
		pdev_id = le32_to_cpu(dcs_intf_ev->pdev_id);
		interference_type = le32_to_cpu(dcs_intf_ev->interference_type);
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Interference detected on pdev %d, interference type %d\n",
			   pdev_id, interference_type);
	} else {
		ath12k_warn(ab, "dcs interference event received with wrong tag\n");
		return;
	}

	switch (interference_type) {
	case WMI_DCS_CW_INTF:
#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_dcs_cw_interference_event_extn(ab, skb, pdev_id);
#endif /* CPTCFG_QCN_EXTN */
		break;
	case WMI_DCS_WLAN_INTF:
#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_dcs_wlan_interference_event_extn(ab, skb, pdev_id);
#else
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "WLAN interference event not handled\n");
#endif
		break;
	case WMI_DCS_AWGN_INTF:
		ath12k_wmi_dcs_awgn_interference_event(ab, skb, pdev_id);
		break;
	case WMI_DCS_OBSS_INTF:
#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_dcs_obss_interference_event_extn(ab, skb, pdev_id);
#endif /* CPTCFG_QCN_EXTN */
		break;
	default:
		ath12k_warn(ab,
			    "For pdev=%d, Invalid Interference type=%d received from FW",
			    pdev_id, interference_type);
		break;
	}
}

static void ath12k_tm_wmi_event_segmented(struct ath12k_base *ab, u32 cmd_id,
					  struct sk_buff *skb)
{
	const void **tb;
	int ret;
	u16 length;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);

	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse ftm event tlv: %d\n", ret);
		return;
	}

	length = skb->len - TLV_HDR_SIZE;
	ath12k_tm_process_event(ab, cmd_id, tb, length);
	kfree(tb);
	tb = NULL;
}

static int
ath12k_wmi_tlv_pdev_temperature_parse(struct ath12k_base *ab,
				      u16 tag, u16 len,
				      const void *ptr, void *data)
{
	struct ath12k_wmi_pdev_temperature_event_parse *parse = data;

	switch (tag) {
	case WMI_TAG_PDEV_TEMPERATURE_EVENT:
		parse->ev = ptr;
		parse->fixed_param_parsed = true;
		break;
	case WMI_TAG_ARRAY_UINT32:
		if (!parse->fixed_param_parsed)
			break;
		if (len >= sizeof(u32)) {
			parse->rfa_temp = *(const a_sle32 *)ptr;
			parse->rfa_temp_valid = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void
ath12k_wmi_pdev_temperature_event(struct ath12k_base *ab,
				  struct sk_buff *skb)
{
	struct ath12k_wmi_pdev_temperature_event_parse parse = {};
	struct ath12k *ar;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_pdev_temperature_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse pdev temperature event tlv: %d\n", ret);
		return;
	}

	if (!parse.ev) {
		ath12k_warn(ab, "failed to fetch pdev temp ev");
		return;
	}

	if (parse.rfa_temp_valid)
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "pdev temperature ev temp %d pdev_id %d rfa_temp %d\n",
			   parse.ev->temp, parse.ev->pdev_id, parse.rfa_temp);
	else
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "pdev temperature ev temp %d pdev_id %d\n",
			   parse.ev->temp, parse.ev->pdev_id);

	rcu_read_lock();

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(parse.ev->pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in pdev temperature ev %d",
			    parse.ev->pdev_id);
		goto exit;
	}

	ath12k_thermal_event_temperature(ar, le32_to_cpu(parse.ev->temp),
					 parse.rfa_temp_valid,
					 le32_to_cpu(parse.rfa_temp));
exit:
	rcu_read_unlock();
}

static int ath12k_wmi_stats_parser(struct ath12k_base *ab,
				   u16 tag, u16 tag_len,
				   const void *ptr,
				   void *data)
{
	int ret = 0;
	u16 tlv_tag, tlv_len, len = 0;
	const struct wmi_tlv *tlv;
	struct wmi_therm_throt_level_stats_info *tt_stats = data;

	switch (tag) {
	case WMI_TAG_THERM_THROT_STATS_EVENT:
		break;
	case WMI_TAG_ARRAY_STRUCT:
		len = tag_len;
		tlv = (struct wmi_tlv *)ptr;
		tlv_tag = u32_get_bits(tlv->header, WMI_TLV_TAG);

		while (len > 0) {
			len -= sizeof(*tlv);
			tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
			ptr += sizeof(*tlv);
			struct wmi_therm_throt_level_stats_info *stats;

			stats = (struct wmi_therm_throt_level_stats_info *)ptr;

			memcpy(tt_stats, stats,
			       sizeof(struct wmi_therm_throt_level_stats_info));
			ptr += tlv_len;
			tt_stats++;
			len -= tlv_len;
		}
		break;
	default:
		ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
			    tag, len);
		return -EINVAL;
	}
	return ret;
}

static void ath12k_wmi_thermal_throt_stats_event(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	struct ath12k *ar;
	const void **tb;
	int ret;
	const struct wmi_therm_throt_stats_event *ev;
	struct wmi_therm_throt_level_stats_info *stats;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_err(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_THERM_THROT_STATS_EVENT];
	if (!ev) {
		ath12k_err(ab, "failed to fetch thermal throt stats ev");
		goto err;
	}

	/* Print debug only if DUT temperature is not in optimal range as this
	 * event is received once on every 2 DC
	 */
	if (ev->level > 0)
		ath12k_dbg(ab, ATH12K_DBG_WMI, "thermal stats ev level %d pdev_id %d\n",
			   ev->level, ev->pdev_id);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, ev->pdev_id);
	if (!ar)
		goto err;

	stats = ar->tt_level_stats;
	memcpy(&ar->tt_current_state, ev, sizeof(struct wmi_therm_throt_stats_event));
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_stats_parser,
				  stats);

	ath12k_thermal_event_throt_level(ar, ev->level);

err:
	kfree(tb);
}

static void ath12k_fils_discovery_event(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_fils_discovery_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse FILS discovery event tlv %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_HOST_SWFDA_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch FILS discovery event\n");
		kfree(tb);
		return;
	}

	ath12k_warn(ab,
		    "FILS discovery frame expected from host for vdev_id: %u, transmission scheduled at %u, next TBTT: %u\n",
		    ev->vdev_id, ev->fils_tt, ev->tbtt);

	kfree(tb);
}

static void ath12k_probe_resp_tx_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_probe_resp_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse probe response transmission status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab,
			    "failed to fetch probe response transmission status event");
		kfree(tb);
		return;
	}

	if (ev->tx_status)
		ath12k_warn(ab,
			    "Probe response transmission failed for vdev_id %u, status %u\n",
			    ev->vdev_id, ev->tx_status);

	kfree(tb);
}

static int ath12k_wmi_p2p_noa_event(struct ath12k_base *ab,
				    struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_p2p_noa_event *ev;
	const struct ath12k_wmi_p2p_noa_info *noa;
	struct ath12k *ar;
	int ret, vdev_id;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse P2P NoA TLV: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_P2P_NOA_EVENT];
	noa = tb[WMI_TAG_P2P_NOA_INFO];

	if (!ev || !noa) {
		ret = -EPROTO;
		goto out;
	}

	vdev_id = __le32_to_cpu(ev->vdev_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi tlv p2p noa vdev_id %i descriptors %u\n",
		   vdev_id, le32_get_bits(noa->noa_attr, WMI_P2P_NOA_INFO_DESC_NUM));

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id %d in P2P NoA event\n",
			    vdev_id);
		ret = -EINVAL;
		goto unlock;
	}

	ath12k_p2p_noa_update_by_vdev_id(ar, vdev_id, noa);

	ret = 0;

unlock:
	rcu_read_unlock();
out:
	kfree(tb);
	return ret;
}

static void ath12k_rfkill_state_change_event(struct ath12k_base *ab,
					     struct sk_buff *skb)
{
	const struct wmi_rfkill_state_change_event *ev;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_RFKILL_EVENT];
	if (!ev) {
		kfree(tb);
		return;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "wmi tlv rfkill state change gpio %d type %d radio_state %d\n",
			 le32_to_cpu(ev->gpio_pin_num),
			 le32_to_cpu(ev->int_type),
			 le32_to_cpu(ev->radio_state));

	spin_lock_bh(&ab->base_lock);
	ab->rfkill_radio_on = (ev->radio_state == cpu_to_le32(WMI_RFKILL_RADIO_STATE_ON));
	spin_unlock_bh(&ab->base_lock);

	queue_work(ab->workqueue, &ab->rfkill_work);
	kfree(tb);
}

/* Map ath12k hw_rev to cnss_diag hw_rev for diagnostic logging */
static u32 ath12k_get_diag_hw_id(struct ath12k_base *ab)
{
	switch (ab->hw_params->hw_rev) {
#ifdef CPTCFG_QCN_EXTN
	case ATH12K_HW_QCN9074_HW10:
		return ATH12K_CNSS_DIAG_HW_QCN9074;
	case ATH12K_HW_QCN9160_HW10:
		return ATH12K_CNSS_DIAG_HW_QCN9160;
#endif
	default:
		/* For all other chipsets, use hw_rev + offset */
		return ab->hw_params->hw_rev + ATH12K_DIAG_HW_ID_OFFSET;
	}
}

static void
ath12k_wmi_diag_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	u32 *dev_id;
	u8 *data;

	tlv = (struct wmi_tlv *)skb->data;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	tlv_len = FIELD_GET(WMI_TLV_LEN, tlv->header);

	if (tlv_tag == WMI_TAG_ARRAY_BYTE) {
		data = skb->data + sizeof(struct wmi_tlv);
		dev_id = (uint32_t *)data;
		*dev_id = ath12k_get_diag_hw_id(ab);
	} else {
		ath12k_warn(ab, "WMI Diag Event missing required tlv\n");
		return;
	}

	trace_ath12k_wmi_diag(ab, skb->data, skb->len);

	ath12k_fwlog_write(ab, data, tlv_len);
}

static void ath12k_wmi_twt_enable_event(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_enable_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi twt enable status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_ENABLE_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt enable wmi event\n");
		goto exit;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "wmi twt enable event pdev id %u status %u\n",
			 le32_to_cpu(ev->pdev_id),
			 le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_disable_event(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_disable_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi twt disable status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_DISABLE_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt disable wmi event\n");
		goto exit;
	}

	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "wmi twt disable event pdev id %d status %u\n",
			 le32_to_cpu(ev->pdev_id),
			 le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_btwt_invite_sta_compl_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_btwt_invite_sta_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi btwt invite sta event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_BTWT_INVITE_STA_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch btwt invite sta event\n");
		goto exit;
	}

	ath12k_info(ab, "wmi btwt invite sta event vdev id %u peer %pM dialog %d status %u\n",
		   le32_to_cpu(ev->vdev_id), ev->peer_macaddr.addr,
		   le32_to_cpu(ev->dialog_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_twt_btwt_remove_sta_compl_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_btwt_remove_sta_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse wmi btwt remove sta event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_BTWT_REMOVE_STA_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch btwt remove sta event\n");
		goto exit;
	}

	ath12k_info(ab, "wmi btwt remove sta event vdev id %u peer %pM dialog %d status %u\n",
		   le32_to_cpu(ev->vdev_id), ev->peer_macaddr.addr,
		   le32_to_cpu(ev->dialog_id),
		   le32_to_cpu(ev->status));

exit:
	kfree(tb);
}

static int ath12k_wmi_wow_wakeup_host_parse(struct ath12k_base *ab,
					    u16 tag, u16 len,
					    const void *ptr, void *data)
{
	const struct wmi_wow_ev_pg_fault_param *pf_param;
	const struct wmi_wow_ev_param *param;
	struct wmi_wow_ev_arg *arg = data;
	int pf_len;

	switch (tag) {
	case WMI_TAG_WOW_EVENT_INFO:
		param = ptr;
		arg->wake_reason = le32_to_cpu(param->wake_reason);
		ath12k_dbg(ab, ATH12K_DBG_WMI, "wow wakeup host reason %d %s\n",
			   arg->wake_reason, wow_reason(arg->wake_reason));
		break;

	case WMI_TAG_ARRAY_BYTE:
		if (arg && arg->wake_reason == WOW_REASON_PAGE_FAULT) {
			pf_param = ptr;
			pf_len = le32_to_cpu(pf_param->len);
			if (pf_len > len - sizeof(pf_len) ||
			    pf_len < 0) {
				ath12k_warn(ab, "invalid wo reason page fault buffer len %d\n",
					    pf_len);
				return -EINVAL;
			}
			ath12k_dbg(ab, ATH12K_DBG_WMI, "wow_reason_page_fault len %d\n",
				   pf_len);
			ath12k_dbg_dump(ab, ATH12K_DBG_WMI,
					"wow_reason_page_fault packet present",
					"wow_pg_fault ",
					pf_param->data,
					pf_len);
		}
		break;
	default:
		break;
	}

	return 0;
}

/**
 * ath12k_wmi_event_mib_stats() - Handle WMI_UPDATE_WHAL_MIB_STATS_EVENTID
 * @ab: ath12k base structure
 * @skb: received skb containing the MIB stats event
 *
 * Parses the WMI MIB stats event sent periodically by firmware and stores
 * the fcs_bad counter into ar->dp.stats.telemetry_stats.rx_crc_err for
 * each pdev.  The fcs_bad field counts FCS-errored frames at the radio
 * (pdev) level and is used to populate the "CRC Error" telemetry stat.
 */
static void ath12k_wmi_event_mib_stats(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct ath12k_wmi_mib_stats_event *ev;
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	const void **tb;
	struct ath12k *ar = NULL;
	u32 fcs_bad;
	int i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse MIB stats event TLV: %ld\n",
			    PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_UPDATE_WHAL_MIB_STATS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "MIB stats event TLV tag not found\n");
		kfree(tb);
		return;
	}

	/* Validate TLV payload length against the parsed structure size,
	 * not the raw skb length which includes headers and other TLVs.
	 */
	if (le32_get_bits(ev->tlv_header, WMI_TLV_LEN) <
	    (sizeof(*ev) - TLV_HDR_SIZE)) {
		ath12k_warn(ab, "MIB stats event TLV too small: %u bytes\n",
			    le32_get_bits(ev->tlv_header, WMI_TLV_LEN));
		kfree(tb);
		return;
	}

	/* The MIB stats event is per-pdev: identify the originating pdev
	 * from the WMI endpoint ID carried in the skb control block.
	 */
	for (i = 0; i < ab->htc.wmi_ep_count; i++) {
		if (ab->wmi_ab.wmi[i].eid == skb_cb->u.eid) {
			ar = ab->pdevs[i].ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "MIB stats event: no ar found for eid %u\n",
			    skb_cb->u.eid);
		kfree(tb);
		return;
	}

	fcs_bad = le32_to_cpu(ev->fcs_bad);

	/* Check for potential overflow before adding */
	if (ar->dp.stats.telemetry_stats.rx_crc_err > U32_MAX - fcs_bad) {
		ath12k_warn(ab,
			    "rx_crc_err counter overflow prevented for pdev %d\n", i);
		/* Saturate at maximum value instead of wrapping */
		ar->dp.stats.telemetry_stats.rx_crc_err = U32_MAX;
	} else {
		ar->dp.stats.telemetry_stats.rx_crc_err += fcs_bad;
	}

	kfree(tb);
}

static void ath12k_wmi_event_wow_wakeup_host(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_wow_ev_arg arg = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_wow_wakeup_host_parse,
				  &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse wmi wow wakeup host event tlv: %d\n",
			    ret);
		return;
	}

	complete(&ab->wow.wakeup_completed);
}

static void ath12k_wmi_gtk_offload_status_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_gtk_offload_status_event *ev;
	struct ath12k_link_vif *arvif;
	__be64 replay_ctr_be;
	u64 replay_ctr;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_GTK_OFFLOAD_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch gtk offload status ev");
		kfree(tb);
		return;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, le32_to_cpu(ev->vdev_id));
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "failed to get arvif for vdev_id:%d\n",
			    le32_to_cpu(ev->vdev_id));
		kfree(tb);
		return;
	}

	replay_ctr = le64_to_cpu(ev->replay_ctr);
	arvif->rekey_data.replay_ctr = replay_ctr;
	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi gtk offload event refresh_cnt %d replay_ctr %llu\n",
		   le32_to_cpu(ev->refresh_cnt), replay_ctr);

	/* supplicant expects big-endian replay counter */
	replay_ctr_be = cpu_to_be64(replay_ctr);

	ieee80211_gtk_rekey_notify(arvif->ahvif->vif, arvif->bssid,
				   (void *)&replay_ctr_be, GFP_ATOMIC);

	rcu_read_unlock();

	kfree(tb);
}

static void ath12k_wmi_event_mlo_setup_complete(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	const struct wmi_mlo_setup_complete_event *ev;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	u32 max_ml_peer_ids;
	const void **tb;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse mlo setup complete event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_SETUP_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch mlo setup complete event\n");
		kfree(tb);
		return;
	}

	if (le32_to_cpu(ev->pdev_id) > ab->num_radios)
		goto skip_lookup;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->pdev_id == le32_to_cpu(ev->pdev_id)) {
			ar = pdev->ar;
			break;
		}
	}

skip_lookup:
	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id %d status %u in setup complete event\n",
			    ev->pdev_id, ev->status);
		goto out;
	}

	ar->mlo_setup_status = le32_to_cpu(ev->status);
	max_ml_peer_ids = le32_to_cpu(ev->max_ml_peer_ids);
	if (!max_ml_peer_ids ||
	    max_ml_peer_ids > ATH12K_MAX_MLO_PEERS) {
		ath12k_warn(ab, "Invalid ML peer ids from firmware %d", max_ml_peer_ids);
		max_ml_peer_ids = ATH12K_MAX_MLO_PEERS;
	}
	ar->ah->max_ml_peer_ids = max_ml_peer_ids;
	if (ab->hw_params->hal_ops->hal_get_tqm_scratch_reg)
		ab->hw_params->hal_ops->hal_get_tqm_scratch_reg(ab, &ar->delta_tqm);
	complete(&ar->mlo_setup_done);

out:
	kfree(tb);
}

static void ath12k_wmi_event_teardown_complete(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const struct wmi_mlo_teardown_complete_event *ev;
	struct ath12k_hw_group *ag = ab->ag;
	bool complete_flag = true;
	struct ath12k_pdev *pdev;
	struct ath12k_hw *ah;
	struct ath12k *ar = NULL;
	const void **tb;
	int i, j, ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse teardown complete event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_TEARDOWN_COMPLETE];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch teardown complete event\n");
		kfree(tb);
		return;
	}

	if (ev->pdev_id > ab->num_radios)
		goto end;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == ev->pdev_id)
			ar = pdev->ar;
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in teardown complete ev %d",
			    ev->pdev_id);
		goto end;
	}

	ar->teardown_complete_event = true;
	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];

			if (!ar->teardown_complete_event)
				complete_flag = false;
		}
	}
	if (complete_flag && (ag->trigger_umac_reset || ag->mlo_teardown)) {
                complete(&ag->umac_reset_complete);
		ag->trigger_umac_reset = false;
		ag->mlo_teardown = false;
	}
end:
	kfree(tb);
}

static int ath12k_wmi_uhr_cu_status_parse(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_uhr_cu_event_parse *arg = data;

	if (tag != WMI_TAG_VDEV_UHR_CU_STATUS)
		return -EPROTO;

	if (!arg->status)
		arg->status = ptr;

	arg->count++;
	return 0;
}

static int ath12k_wmi_uhr_cu_event_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_uhr_cu_event_parse *arg = data;
	int ret;

	switch (tag) {
	case WMI_TAG_PDEV_UHR_CU_EVENT_FIXED_PARAM:
		arg->fixed = *(const struct wmi_pdev_uhr_cu_event_fixed_param *)ptr;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (len == 0)
			break;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_uhr_cu_status_parse, arg);
		if (ret) {
			ath12k_warn(ab, "failed to parse uhr cu status tlv: %d\n",
				    ret);
			return ret;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath12k_wmi_pdev_uhr_cu_event(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct ath12k_wmi_uhr_cu_event_parse arg = {};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_uhr_cu_event_parse, &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse pdev uhr cu event tlv: %d\n", ret);
		return;
	}

	if (!arg.count || !arg.status)
		return;

	ath12k_dbg(ab, ATH12K_DBG_WMI | ATH12K_DBG_CU,
		   "wmi pdev uhr cu event pdev_id %u vdev_status_count %u\n",
		   le32_to_cpu(arg.fixed.pdev_id), arg.count);

	ath12k_mac_handle_pdev_uhr_cu_event(ab,
					    le32_to_cpu(arg.fixed.pdev_id),
					    arg.status,
					    arg.count);
}

static void ath12k_wmi_event_send_cumac_complete(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const struct wmi_mlo_send_cumac_complete_event *ev;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev = NULL;
	const void **tb;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse cumac complete event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_SET_CUMAC_COMPLETE];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch cumac complete event\n");
		kfree(tb);
		return;
	}

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->pdev_id == le32_to_cpu(ev->pdev_id)) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in cumac completion event %d",
			    ev->pdev_id);
		goto out;
	}

	if (le32_to_cpu(ev->status) == WMI_PDEV_SET_CUMAC_CHIP_ID_SUCCESS) {
		complete(&ar->cumac_setup_done);
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "Received cumac completion status: SUCCESS\n");
	} else {
		ath12k_err(ab, "pdev id %d received cumac completion with failure status\n",
			   ev->pdev_id);
	}

out:
	kfree(tb);
}

static void ath12k_wmi_smd_roam_config_event(struct ath12k_base *ab,
					     struct sk_buff *skb)
{
	const struct wmi_smd_roam_config_event *ev;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	const void **tb;
	u32 vdev_id, event_flags;
	u32 role, cmd_type;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse smd roam config event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_SMD_ROAM_CONFIG_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch smd roam config event\n");
		kfree(tb);
		return;
	}

	vdev_id = le32_to_cpu(ev->vdev_id);
	event_flags = le32_to_cpu(ev->event_flags);
	role = WMI_PEER_SMD_EVT_FLAGS_GET_ROLE(event_flags);
	cmd_type = WMI_PEER_SMD_EVT_FLAGS_GET_SMD_TYPE(event_flags);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi smd roam config event: vdev_id=%u peer=%pM role=%u cmd_type=%u\n",
		   vdev_id, ev->peer_mac_addr.addr, role, cmd_type);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "smd roam event: invalid vdev_id %u\n", vdev_id);
		rcu_read_unlock();
		kfree(tb);
		return;
	}

	ahvif = arvif->ahvif;
	if (!ahvif->smd.exec_in_progress) {
		ath12k_warn(ab, "smd roam event: no SMD exec in progress\n");
		rcu_read_unlock();
		kfree(tb);
		return;
	}

	vif = ahvif->vif;

	/* exec_in_progress is intentionally NOT cleared here: this WMI event
	 * fires at the START of the DL drain window (FW has begun draining).
	 * mac80211 still needs to call __ieee80211_smd_dl_drain_complete() and
	 * then drv_uhr_link_reconfig(DYNAMIC_CONTEXT) before the state is done.
	 * The clear happens in ath12k_mac_op_uhr_link_reconfig() DYNAMIC_CONTEXT.
	 */
	rcu_read_unlock();
	kfree(tb);

	/* DL drain complete - notify mac80211 to proceed with Phase C */
	ieee80211_smd_dl_drain_complete_irqsafe(vif, ahvif->smd.target_mld_addr);
}

#ifdef CPTCFG_ATH12K_DEBUGFS

void ath12k_wmi_crl_path_stats_list_free(struct ath12k *ar, struct list_head *head)
{
	struct wmi_ctrl_path_stats_list *stats, *tmp;

	lockdep_assert_held(&ar->debug.wmi_ctrl_path_stats_lock);
	list_for_each_entry_safe(stats, tmp, head, list) {
		kfree(stats->stats_ptr);
		list_del(&stats->list);
		kfree(stats);
	}
}

int wmi_print_ctrl_path_pdev_tx_stats_tlv(struct ath12k_base *ab, u16 len, const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_pdev_stats_params *pdev_stats_skb = (struct wmi_ctrl_path_pdev_stats_params *)ptr;
	struct wmi_ctrl_path_pdev_stats_params *pdev_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
	struct ath12k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	pdev_stats = kzalloc(sizeof(*pdev_stats), GFP_ATOMIC);
	if (!pdev_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(pdev_stats, pdev_stats_skb, sizeof(struct wmi_ctrl_path_pdev_stats_params));
	stats->stats_ptr = pdev_stats;
	stats->tagid = WMI_TAG_CTRL_PATH_PDEV_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl stats\n");
		kfree(pdev_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_TAG_CTRL_PATH_PDEV_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_cal_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_cal_stats *cal_stats_skb = (struct wmi_ctrl_path_cal_stats *)ptr;
	struct wmi_ctrl_path_cal_stats *cal_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
	struct ath12k *ar = NULL;

	if (!stats)
		return -ENOMEM;

	cal_stats = kzalloc(sizeof(*cal_stats), GFP_ATOMIC);
	if (!cal_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(cal_stats, cal_stats_skb, sizeof(struct wmi_ctrl_path_cal_stats));
	stats->stats_ptr = cal_stats;
	stats->tagid = WMI_CTRL_PATH_CAL_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, cal_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		kfree(cal_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_CAL_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_btcoex_stats_tlv(struct ath12k_base *ab, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
				(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats_skb =
				(struct wmi_ctrl_path_btcoex_stats *)ptr;
	struct wmi_ctrl_path_btcoex_stats *btcoex_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	btcoex_stats = kzalloc(sizeof(*btcoex_stats), GFP_ATOMIC);
	if (!btcoex_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(btcoex_stats, btcoex_stats_skb, sizeof(*btcoex_stats));
	stats->stats_ptr = btcoex_stats;
	stats->tagid = WMI_CTRL_PATH_BTCOEX_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, btcoex_stats_skb->pdev_id + 1);
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl cal stats\n");
		kfree(btcoex_stats);
		list_del(&stats->list);
		kfree(stats);
		return -EINVAL;
	}

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_BTCOEX_STATS;
	stats_buff->ar = ar;
	return 0;
}

/*static void
ath12k_wmi_ctrl_path_pdev_stats_list_free(struct list_head *head)
{
	struct wmi_ctrl_path_pdev_stats *stats_list, *tmp;

	list_for_each_entry_safe(stats_list, tmp, head, list) {
		list_del(&stats_list->list);
		kfree(stats_list);
	}
}

static void
ath12k_wmi_ctrl_path_stats_list_free(struct ath12k_wmi_ctrl_path_stats_list *param)
{
	ath12k_wmi_ctrl_path_pdev_stats_list_free(&param->pdev_stats);
}
*/


int wmi_print_ctrl_path_awgn_stats_tlv(struct ath12k_base *ab, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
			    (struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_awgn_stats *awgn_stats_skb, *awgn_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;

	awgn_stats_skb = (struct wmi_ctrl_path_awgn_stats *)ptr;

	for (i = 0; i < ATH12K_GROUP_MAX_RADIO; i++) {
		ar = ath12k_mac_get_ar_by_pdev_id(ab,
						  ab->ag->dp_hw_grp->hw_links[i].pdev_idx);
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for wmi ctrl awgn stats\n");
			return -EINVAL;
		}

		if (ar->supports_6ghz)
			break;
	}

	if (!ar->supports_6ghz) {
		ath12k_warn(ab, "AWGN stats is not supported on a non 6 GHz radio\n");
		return -EINVAL;
	}

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	awgn_stats = kzalloc(sizeof(*awgn_stats), GFP_ATOMIC);

	if (!awgn_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(awgn_stats, awgn_stats_skb, sizeof(*awgn_stats));
	stats->stats_ptr = awgn_stats;
	list_add_tail(&stats->list, &stats_buff->list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_AWGN_STATS;
	stats_buff->ar = ar;

	return 0;
}

int wmi_print_ctrl_path_blanking_stats_tlv(struct ath12k_base *ab, u16 len,
					   const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
			(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_blanking_stats *blanking_stats_skb =
			(struct wmi_ctrl_path_blanking_stats *)ptr;
	struct wmi_ctrl_path_blanking_stats *blanking_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		struct ath12k_pdev *pdev = rcu_dereference(ab->pdevs_active[i]);

		if (pdev && pdev->ar) {
			ar = pdev->ar;
			break;
		}
	}
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for wmi ctrl blanking stats\n");
		return -EINVAL;
	}

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	blanking_stats = kzalloc(sizeof(*blanking_stats), GFP_ATOMIC);
	if (!blanking_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(blanking_stats, blanking_stats_skb, sizeof(*blanking_stats));
	stats->stats_ptr = blanking_stats;
	stats->tagid = WMI_CTRL_PATH_BLANKING_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar,
				&ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_BLANKING_STATS;
	stats_buff->ar = ar;
	return 0;
}

int wmi_print_ctrl_path_mem_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff =
		(struct wmi_ctrl_path_stats_ev_parse_param *)data;
	struct wmi_ctrl_path_mem_stats_params *mem_stats_skb =
		(struct wmi_ctrl_path_mem_stats_params *)ptr;
	struct wmi_ctrl_path_mem_stats_params *mem_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar || !ar->ctrl_mem_stats)
			continue;
		stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
		if (!stats)
			return -ENOMEM;

		mem_stats = kzalloc(sizeof(*mem_stats), GFP_ATOMIC);
		if (!mem_stats) {
			kfree(stats);
			return -ENOMEM;
		}

		memcpy(mem_stats, mem_stats_skb, sizeof(*mem_stats));
		stats->stats_ptr = mem_stats;
		stats->tagid = WMI_CTRL_PATH_MEM_STATS;
		list_add_tail(&stats->list, &stats_buff->list);

		spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
		ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
		spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
		ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_MEM_STATS;
		stats_buff->ar = ar;
	}

	return 0;
}

int wmi_print_ctrl_path_afc_stats_tlv(struct ath12k_base *ab, u16 len,
				      const void *ptr, void *data)
{
	struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = data;
	struct wmi_ctrl_path_afc_stats *afc_stats_skb, *afc_stats = NULL;
	struct wmi_ctrl_path_stats_list *stats;
	struct ath12k *ar = NULL;
	int i;
	struct ath12k_base *partner_ab;;
	u8 device_id, pdev_idx;

	afc_stats_skb = (struct wmi_ctrl_path_afc_stats *)ptr;

	for (i = 0; i < ATH12K_GROUP_MAX_RADIO; i++) {
		device_id = ab->ag->dp_hw_grp->hw_links[i].device_id;
		pdev_idx = ab->ag->dp_hw_grp->hw_links[i].pdev_idx;
		partner_ab = ath12k_ag_to_ab(ab->ag, device_id);
		ar = partner_ab->pdevs[pdev_idx].ar;
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for wmi ctrl afc stats\n");
			return -EINVAL;
		}

		if (ar->supports_6ghz)
			break;
	}

	if (!ar->supports_6ghz) {
		ath12k_warn(ab, "AFC stats is not supported on a non 6 GHz radio\n");
		return -EINVAL;
	}

	stats = kzalloc(sizeof(*stats), GFP_ATOMIC);
	if (!stats)
		return -ENOMEM;

	afc_stats = kzalloc(sizeof(*afc_stats), GFP_ATOMIC);

	if (!afc_stats) {
		kfree(stats);
		return -ENOMEM;
	}

	memcpy(afc_stats, afc_stats_skb, sizeof(*afc_stats));
	stats->stats_ptr = afc_stats;
	stats->tagid = WMI_CTRL_PATH_AFC_STATS;
	list_add_tail(&stats->list, &stats_buff->list);

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.wmi_ctrl_path_stats.pdev_stats);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_AFC_STATS;
	stats_buff->ar = ar;

	return 0;
}

int wmi_print_ctrl_path_pmlo_stats_tlv(struct ath12k_base *ab, u16 len, const void *ptr, void *data)
{
       struct wmi_ctrl_path_stats_ev_parse_param *stats_buff = (struct wmi_ctrl_path_stats_ev_parse_param *)data;
       struct wmi_ctrl_path_pmlo_telemetry_stats *pmlo_stats_skb = (struct wmi_ctrl_path_pmlo_telemetry_stats *)ptr;
       struct wmi_ctrl_path_pmlo_telemetry_stats *pmlo_stats = NULL;
       struct wmi_ctrl_path_stats_list *stats = kzalloc(sizeof(struct wmi_ctrl_path_stats_list), GFP_ATOMIC);
       struct ath12k *ar = NULL;
       u32 value;

       if (!stats)
               return -ENOMEM;

	pmlo_stats = kzalloc(sizeof(*pmlo_stats), GFP_ATOMIC);
       if (!pmlo_stats) {
               kfree(stats);
               return -ENOMEM;
       }

       memcpy(pmlo_stats, pmlo_stats_skb, sizeof(struct wmi_ctrl_path_pmlo_telemetry_stats));
       stats->stats_ptr = pmlo_stats;
	stats->tagid = WMI_CTRL_PATH_PMLO_STATS;
       list_add_tail(&stats->list, &stats_buff->list);

       ar = ath12k_mac_get_ar_by_pdev_id(ab, pmlo_stats_skb->pdev_id);
       if (!ar) {
               ath12k_warn(ab, "Failed to get ar for wmi ctrl stats\n");
		kfree(pmlo_stats);
               list_del(&stats->list);
               kfree(stats);
               return -EINVAL;
       }

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
       value = le32_to_cpu(pmlo_stats->estimated_air_time_per_ac);
       ar->stats.telemetry_stats.estimated_air_time_ac_be =
               u32_get_bits(value, GENMASK(7, 0));
       ar->stats.telemetry_stats.estimated_air_time_ac_bk =
               u32_get_bits(value, GENMASK(15, 8));
       ar->stats.telemetry_stats.estimated_air_time_ac_vi =
               u32_get_bits(value, GENMASK(23, 16));
       ar->stats.telemetry_stats.estimated_air_time_ac_vo =
               u32_get_bits(value, GENMASK(31, 24));

	ath12k_wmi_crl_path_stats_list_free(ar, &ar->debug.period_wmi_list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ar->debug.wmi_ctrl_path_stats_tagid = WMI_CTRL_PATH_PMLO_STATS;
       stats_buff->ar = ar;
       return 0;
}

static int ath12k_wmi_ctrl_stats_subtlv_parser(struct ath12k_base *ab,
					       u16 tag, u16 len,
					       const void *ptr, void *data)
{
	int ret = -EINVAL;

	switch (tag) {
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
		break;
	case WMI_TAG_CTRL_PATH_PDEV_STATS:
		ret = wmi_print_ctrl_path_pdev_tx_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_CAL_STATS:
		ret = wmi_print_ctrl_path_cal_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_BTCOEX_STATS:
		ret = wmi_print_ctrl_path_btcoex_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_AWGN_STATS:
		ret = wmi_print_ctrl_path_awgn_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_MEM_STATS:
		ret = wmi_print_ctrl_path_mem_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_AFC_STATS:
		ret = wmi_print_ctrl_path_afc_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_PMLO_STATS:
	        ret = wmi_print_ctrl_path_pmlo_stats_tlv(ab, len, ptr, data);
		break;
	case WMI_CTRL_PATH_BLANKING_STATS:
		ret = wmi_print_ctrl_path_blanking_stats_tlv(ab, len, ptr, data);
		break;
		/* Add case for newly wmi ctrl path added stats here */
	default:
		ath12k_warn(ab,
			    "Received invalid tag for wmi ctrl path stats in subtlvs, tag : 0x%x\n",
			    tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ath12k_wmi_ctrl_stats_event_parser(struct ath12k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	int ret;

	ath12k_dbg_level(ab, ATH12K_DBG_WMI, ATH12K_DBG_L3,
			 "wmi ctrl path stats tag 0x%x of len %d rcvd\n",
			 tag, len);

	switch (tag) {
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
		/* Fixed param is already processed*/
		ret = 0;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
		return 0;

		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_ctrl_stats_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi ctrl path stats\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void ath12k_wmi_ctrl_path_stats_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_ctrl_path_stats_event *fixed_param;
	struct wmi_ctrl_path_stats_ev_parse_param param = {0};
	struct ath12k_wmi_ctrl_path_stats_list *stats;
	const struct wmi_tlv *tlv;
	struct list_head *src, *dst = NULL;
	struct ath12k *ar = NULL;
	void *ptr = skb->data;
	u16 tlv_tag, tag_id;
	u32 more;
	int ret;

	if (!skb->data) {
		ath12k_warn(ab, "No data present in wmi ctrl stats event\n");
		return;
	}

	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "wmi ctrl stats event size invalid\n");
		return;
	}

	param.ar = NULL;

	tlv = ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag != WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM) {
		ath12k_warn(ab, "wmi ctrl stats without fixed param tlv at start\n");
		return;
	}

	INIT_LIST_HEAD(&param.list);

	fixed_param = ptr;
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_ctrl_stats_event_parser,
				  &param);
	if (ret) {
		ath12k_warn(ab, "failed to parse wmi_ctrl_path_stats tlv: %d\n", ret);
		if (!param.ar)
			return;
		ar = param.ar;
		goto free;
	}

	ar = param.ar;
	if (!ar)
		return;

	tag_id = ar->debug.wmi_ctrl_path_stats_tagid;
	stats = &ar->debug.wmi_ctrl_path_stats;
	more = __le32_to_cpu(fixed_param->more);

	src = &param.list;
	dst = &stats->pdev_stats;

	switch (tag_id) {
	case WMI_TAG_CTRL_PATH_PDEV_STATS:
		break;
	case WMI_CTRL_PATH_PMLO_STATS:
		ath12k_dp_mon_pdev_update_telemetry_stats(ar->ab, ar->pdev_idx);
		break;
	case WMI_TAG_CTRL_PATH_STATS_EV_FIXED_PARAM:
	case WMI_CTRL_PATH_CAL_STATS:
	case WMI_CTRL_PATH_BTCOEX_STATS:
	case WMI_CTRL_PATH_AWGN_STATS:
	case WMI_CTRL_PATH_MEM_STATS:
	case WMI_CTRL_PATH_AFC_STATS:
	case WMI_CTRL_PATH_BLANKING_STATS:
		break;
	/* Add case for newly wmi ctrl path added stats here */
	default:
		ath12k_warn(ab, "Received tag for wmi ctrl path stats %d\n", tag_id);
		goto free;
	}

	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	if (!more) {
		if (tag_id == WMI_CTRL_PATH_PMLO_STATS) {
			list_splice_tail_init(&param.list, &ar->debug.period_wmi_list);
		} else {
			if (!ar->debug.wmi_ctrl_path_stats_more_enabled)
				ath12k_wmi_crl_path_stats_list_free(ar,
								    &stats->pdev_stats);
			else
				ar->debug.wmi_ctrl_path_stats_more_enabled = false;
			list_splice_tail_init(src, dst);
		}
		complete(&ar->debug.wmi_ctrl_path_stats_rcvd);
	} else {
		if (tag_id == WMI_CTRL_PATH_PMLO_STATS) {
			list_splice_tail_init(&param.list, &ar->debug.period_wmi_list);
		} else {
			if (!ar->debug.wmi_ctrl_path_stats_more_enabled) {
				ath12k_wmi_crl_path_stats_list_free(ar,
								    &stats->pdev_stats);
				ar->debug.wmi_ctrl_path_stats_more_enabled = true;
			}
			list_splice_tail_init(src, dst);
		}
	}
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	return;
free:
	spin_lock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
	ath12k_wmi_crl_path_stats_list_free(ar, &param.list);
	spin_unlock_bh(&ar->debug.wmi_ctrl_path_stats_lock);
}
#else
static void ath12k_wmi_ctrl_path_stats_event(struct ath12k_base *ab,
struct sk_buff *skb)
{
	ath12k_update_stats_event(ab, skb);
}
#endif /* CPTCFG_ATH12K_DEBUGFS */

#ifdef CPTCFG_ATH12K_DEBUGFS
static int ath12k_wmi_tpc_stats_copy_buffer(struct ath12k_base *ab,
					    const void *ptr, u16 tag, u16 len,
					    struct wmi_tpc_stats_arg *tpc_stats)
{
	u32 len1, len2, len3, len4;
	s16 *dst_ptr;
	s8 *dst_ptr_ctl;

	len1 = le32_to_cpu(tpc_stats->max_reg_allowed_power.tpc_reg_pwr.reg_array_len);
	len2 = le32_to_cpu(tpc_stats->rates_array1.tpc_rates_array.rate_array_len);
	len3 = le32_to_cpu(tpc_stats->rates_array2.tpc_rates_array.rate_array_len);
	len4 = le32_to_cpu(tpc_stats->ctl_array.tpc_ctl_pwr.ctl_array_len);

	switch (tpc_stats->event_count) {
	case ATH12K_TPC_STATS_CONFIG_REG_PWR_EVENT:
		if (len1 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_REG_PWR_ALLOWED) {
			dst_ptr = tpc_stats->max_reg_allowed_power.reg_pwr_array;
			memcpy(dst_ptr, ptr, len1);
		}
		break;
	case ATH12K_TPC_STATS_RATES_EVENT1:
		if (len2 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY1) {
			dst_ptr = tpc_stats->rates_array1.rate_array;
			memcpy(dst_ptr, ptr, len2);
		}
		break;
	case ATH12K_TPC_STATS_RATES_EVENT2:
		/* EHT target powers; UHR target powers are identical so the
		 * display path also uses this buffer for UHR preambles.
		 */
		if (len3 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_RATES_ARRAY2) {
			dst_ptr = tpc_stats->rates_array2.rate_array;
			memcpy(dst_ptr, ptr, len3);
		}
		break;
	case ATH12K_TPC_STATS_CTL_TABLE_EVENT:
		if (len4 > len)
			return -ENOBUFS;

		if (tpc_stats->tlvs_rcvd & WMI_TPC_CTL_PWR_ARRAY) {
			dst_ptr_ctl = tpc_stats->ctl_array.ctl_pwr_table;
			memcpy(dst_ptr_ctl, ptr, len4);
		}
		break;
	}
	return 0;
}

static int ath12k_tpc_calc_tbl_size(__le32 d1, __le32 d2, __le32 d3, __le32 d4,
				    u32 *total_size)
{
	u32 v1, v2, v3, v4;

	*total_size = 0;

	v1 = le32_to_cpu(d1);
	v2 = le32_to_cpu(d2);
	v3 = le32_to_cpu(d3);
	v4 = le32_to_cpu(d4);

	if (check_mul_overflow(v1, v2, total_size) ||
	    check_mul_overflow(*total_size, v3, total_size) ||
	    check_mul_overflow(*total_size, v4, total_size))
		return -EINVAL;

	return 0;
}

static int ath12k_tpc_get_reg_pwr(struct ath12k_base *ab,
				  struct wmi_tpc_stats_arg *tpc_stats,
				  struct wmi_max_reg_power_fixed_params *ev)
{
	struct wmi_max_reg_power_allowed_arg *reg_pwr;
	u32 total_size;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received reg power array type %d length %d for tpc stats\n",
		   ev->reg_power_type, ev->reg_array_len);

	switch (le32_to_cpu(ev->reg_power_type)) {
	case TPC_STATS_REG_PWR_ALLOWED_TYPE:
		reg_pwr = &tpc_stats->max_reg_allowed_power;
		break;
	default:
		return -EINVAL;
	}

	/* Each entry is 2 byte hence multiplying the indices with 2 */
	if (ath12k_tpc_calc_tbl_size(ev->d1, ev->d2, ev->d3, ev->d4,
				     &total_size)) {
		return -EINVAL;
	}

	if (check_mul_overflow(total_size, 2U, &total_size))
		return -EINVAL;

	if (le32_to_cpu(ev->reg_array_len) != total_size) {
		ath12k_warn(ab,
			    "Total size and reg_array_len doesn't match for tpc stats\n");
		return -EINVAL;
	}

	memcpy(&reg_pwr->tpc_reg_pwr, ev, sizeof(struct wmi_max_reg_power_fixed_params));

	reg_pwr->reg_pwr_array = kzalloc(le32_to_cpu(reg_pwr->tpc_reg_pwr.reg_array_len),
					 GFP_ATOMIC);
	if (!reg_pwr->reg_pwr_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= WMI_TPC_REG_PWR_ALLOWED;

	return 0;
}

static int ath12k_tpc_get_rate_array(struct ath12k_base *ab,
				     struct wmi_tpc_stats_arg *tpc_stats,
				     struct wmi_tpc_rates_array_fixed_params *ev)
{
	struct wmi_tpc_rates_array_arg *rates_array;
	u32 flag = 0, rate_array_len;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received rates array type %d length %d for tpc stats\n",
		   ev->rate_array_type, ev->rate_array_len);

	switch (le32_to_cpu(ev->rate_array_type)) {
	case ATH12K_TPC_STATS_RATES_ARRAY1:
		rates_array = &tpc_stats->rates_array1;
		flag = WMI_TPC_RATES_ARRAY1;
		break;
	case ATH12K_TPC_STATS_RATES_ARRAY2:
		rates_array = &tpc_stats->rates_array2;
		flag = WMI_TPC_RATES_ARRAY2;
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid type of rates array for tpc stats\n");
		return -EINVAL;
	}
	memcpy(&rates_array->tpc_rates_array, ev,
	       sizeof(struct wmi_tpc_rates_array_fixed_params));
	rate_array_len = le32_to_cpu(rates_array->tpc_rates_array.rate_array_len);
	rates_array->rate_array = kzalloc(rate_array_len, GFP_ATOMIC);
	if (!rates_array->rate_array)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return 0;
}

static int ath12k_tpc_get_ctl_pwr_tbl(struct ath12k_base *ab,
				      struct wmi_tpc_stats_arg *tpc_stats,
				      struct wmi_tpc_ctl_pwr_fixed_params *ev)
{
	struct wmi_tpc_ctl_pwr_table_arg *ctl_array;
	u32 total_size, ctl_array_len, flag = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Received ctl array type %d length %d for tpc stats\n",
		   ev->ctl_array_type, ev->ctl_array_len);

	switch (le32_to_cpu(ev->ctl_array_type)) {
	case ATH12K_TPC_STATS_CTL_ARRAY:
		ctl_array = &tpc_stats->ctl_array;
		flag = WMI_TPC_CTL_PWR_ARRAY;
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid type of ctl pwr table for tpc stats\n");
		return -EINVAL;
	}

	if (ath12k_tpc_calc_tbl_size(ev->d1, ev->d2, ev->d3, ev->d4,
				     &total_size)) {
		return -EINVAL;
	}

	if (le32_to_cpu(ev->ctl_array_len) != total_size) {
		ath12k_warn(ab,
			    "Total size and ctl_array_len doesn't match for tpc stats\n");
		return -EINVAL;
	}

	memcpy(&ctl_array->tpc_ctl_pwr, ev, sizeof(struct wmi_tpc_ctl_pwr_fixed_params));
	ctl_array_len = le32_to_cpu(ctl_array->tpc_ctl_pwr.ctl_array_len);
	ctl_array->ctl_pwr_table = kzalloc(ctl_array_len, GFP_ATOMIC);
	if (!ctl_array->ctl_pwr_table)
		return -ENOMEM;

	tpc_stats->tlvs_rcvd |= flag;
	return 0;
}

static int ath12k_wmi_tpc_stats_subtlv_parser(struct ath12k_base *ab,
					      u16 tag, u16 len,
					      const void *ptr, void *data)
{
	struct wmi_tpc_rates_array_fixed_params *tpc_rates_array;
	struct wmi_max_reg_power_fixed_params *tpc_reg_pwr;
	struct wmi_tpc_ctl_pwr_fixed_params *tpc_ctl_pwr;
	struct wmi_tpc_stats_arg *tpc_stats = data;
	struct wmi_tpc_config_params *tpc_config;
	int ret = 0;

	if (!tpc_stats) {
		ath12k_warn(ab, "tpc stats memory unavailable\n");
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_TPC_STATS_CONFIG_EVENT:
		tpc_config = (struct wmi_tpc_config_params *)ptr;
		memcpy(&tpc_stats->tpc_config, tpc_config,
		       sizeof(struct wmi_tpc_config_params));
		break;
	case WMI_TAG_TPC_STATS_REG_PWR_ALLOWED:
		tpc_reg_pwr = (struct wmi_max_reg_power_fixed_params *)ptr;
		ret = ath12k_tpc_get_reg_pwr(ab, tpc_stats, tpc_reg_pwr);
		break;
	case WMI_TAG_TPC_STATS_RATES_ARRAY:
		tpc_rates_array = (struct wmi_tpc_rates_array_fixed_params *)ptr;
		ret = ath12k_tpc_get_rate_array(ab, tpc_stats, tpc_rates_array);
		break;
	case WMI_TAG_TPC_STATS_CTL_PWR_TABLE_EVENT:
		tpc_ctl_pwr = (struct wmi_tpc_ctl_pwr_fixed_params *)ptr;
		ret = ath12k_tpc_get_ctl_pwr_tbl(ab, tpc_stats, tpc_ctl_pwr);
		break;
	default:
		ath12k_warn(ab,
			    "Received invalid tag for tpc stats in subtlvs\n");
		return -EINVAL;
	}
	return ret;
}

static int ath12k_wmi_tpc_stats_event_parser(struct ath12k_base *ab,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct wmi_tpc_stats_arg *tpc_stats = (struct wmi_tpc_stats_arg *)data;
	int ret;

	switch (tag) {
	case WMI_TAG_HALPHY_CTRL_PATH_EVENT_FIXED_PARAM:
		ret = 0;
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type to pack inside that tlv
		 */
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tpc_stats_subtlv_parser,
					  tpc_stats);
		break;
	case WMI_TAG_ARRAY_INT16:
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_INT16,
						       len, tpc_stats);
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (len == 0)
			return 0;
		ret = ath12k_wmi_tpc_stats_copy_buffer(ab, ptr,
						       WMI_TAG_ARRAY_BYTE,
						       len, tpc_stats);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for tpc stats\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

void ath12k_wmi_free_tpc_stats_mem(struct ath12k *ar)
{
	struct wmi_tpc_stats_arg *tpc_stats = ar->debug.tpc_stats;

	lockdep_assert_held(&ar->data_lock);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "tpc stats mem free\n");
	if (tpc_stats) {
		kfree(tpc_stats->max_reg_allowed_power.reg_pwr_array);
		kfree(tpc_stats->rates_array1.rate_array);
		kfree(tpc_stats->rates_array2.rate_array);
		kfree(tpc_stats->ctl_array.ctl_pwr_table);
		kfree(tpc_stats);
		ar->debug.tpc_stats = NULL;
	}
}

static void ath12k_wmi_process_tpc_stats(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct ath12k_wmi_pdev_tpc_stats_event_fixed_params *fixed_param;
	struct wmi_tpc_stats_arg *tpc_stats;
	const struct wmi_tlv *tlv;
	void *ptr = skb->data;
	struct ath12k *ar;
	u16 tlv_tag;
	u32 event_count;
	int ret;

	if (!skb->data) {
		ath12k_warn(ab, "No data present in tpc stats event\n");
		return;
	}

	if (skb->len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "TPC stats event size invalid\n");
		return;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
	ptr += sizeof(*tlv);

	if (tlv_tag != WMI_TAG_HALPHY_CTRL_PATH_EVENT_FIXED_PARAM) {
		ath12k_warn(ab, "TPC stats without fixed param tlv at start\n");
		return;
	}

	fixed_param = (struct ath12k_wmi_pdev_tpc_stats_event_fixed_params *)ptr;
	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(fixed_param->pdev_id));
	if (!ar) {
		ath12k_warn(ab, "Failed to get ar for tpc stats\n");
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);
	if (!ar->debug.tpc_request) {
		/* Event is received either without request or the
		 * timeout, if memory is already allocated free it
		 */
		if (ar->debug.tpc_stats) {
			ath12k_warn(ab, "Freeing memory for tpc_stats\n");
			ath12k_wmi_free_tpc_stats_mem(ar);
		}
		goto unlock;
	}

	event_count = le32_to_cpu(fixed_param->event_count);
	if (event_count == 0) {
		if (ar->debug.tpc_stats) {
			ath12k_warn(ab,
				    "Invalid tpc memory present\n");
			goto unlock;
		}
		ar->debug.tpc_stats =
			kzalloc(sizeof(struct wmi_tpc_stats_arg),
				GFP_ATOMIC);
		if (!ar->debug.tpc_stats) {
			ath12k_warn(ab,
				    "Failed to allocate memory for tpc stats\n");
			goto unlock;
		}
	}

	tpc_stats = ar->debug.tpc_stats;
	if (!tpc_stats) {
		ath12k_warn(ab, "tpc stats memory unavailable\n");
		goto unlock;
	}

	if (!(event_count == 0)) {
		if (event_count != tpc_stats->event_count + 1) {
			ath12k_warn(ab,
				    "Invalid tpc event received\n");
			goto unlock;
		}
	}
	tpc_stats->pdev_id = le32_to_cpu(fixed_param->pdev_id);
	tpc_stats->end_of_event = le32_to_cpu(fixed_param->end_of_event);
	tpc_stats->event_count = le32_to_cpu(fixed_param->event_count);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "tpc stats event_count %d\n",
		   tpc_stats->event_count);
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tpc_stats_event_parser,
				  tpc_stats);
	if (ret) {
		ath12k_wmi_free_tpc_stats_mem(ar);
		ath12k_warn(ab, "failed to parse tpc_stats tlv: %d\n", ret);
		goto unlock;
	}

	if (tpc_stats->end_of_event)
		complete(&ar->debug.tpc_complete);

unlock:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}
#else
static void ath12k_wmi_process_tpc_stats(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
}
#endif

static long int
ath12k_pull_peer_create_conf_ev(struct ath12k_base *ab,
				struct sk_buff *skb,
				struct ath12k_wmi_peer_create_conf_arg *arg)
{
	const void **tb;
	const struct ath12k_wmi_peer_create_conf_ev *ev;
	long int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %ld\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_CREATE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer create response ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = __le32_to_cpu(ev->vdev_id);
	ether_addr_copy(arg->mac_addr, ev->peer_macaddr.addr);
	arg->status = __le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static void ath12k_wmi_peer_create_conf_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	struct ath12k_wmi_peer_create_conf_arg arg = {};
	struct ath12k *ar;

	if (ath12k_pull_peer_create_conf_ev(ab, skb, &arg)) {
		ath12k_err(ab, "failed to extract peer create conf event");
		return;
	}

	if (arg.status != ATH12K_WMI_PEER_CREATE_SUCCESS)
		ath12k_warn(ab, "Peer %pM creation failed due to %d",
			    arg.mac_addr, arg.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, arg.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer delete resp ev %d",
			    arg.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->peer_create_status = arg.status;
	complete(&ar->peer_create_conf);
	rcu_read_unlock();

	ath12k_dbg_level(ab, ATH12K_DBG_WMI | ATH12K_DBG_MLME, ATH12K_DBG_L1,
			 "Peer create conf event for %pM status %d",
			 arg.mac_addr, arg.status);
}

static int ath12k_wmi_pdev_sscan_fft_bin_index_parse(struct ath12k_base *soc,
						     u16 tag, u16 len,
						     const void *ptr, void *data)
{
	struct wmi_pdev_sscan_fw_param_parse *parse = data;

	if (tag != WMI_TAG_PDEV_SSCAN_FFT_BIN_INDEX)
		return -EPROTO;
	parse->bin = (struct ath12k_wmi_pdev_sscan_fft_bin_index *)ptr;
	parse->num_fft_bin_index++;
	return 0;
}

static int ath12k_wmi_pdev_sscan_per_detector_info_parse(struct ath12k_base *soc,
							 u16 tag, u16 len,
							 const void *ptr, void *data)
{
	struct wmi_pdev_sscan_fw_param_parse *parse = data;

	if (tag != WMI_TAG_PDEV_SSCAN_PER_DETECTOR_INFO)
		return -EPROTO;
	parse->det_info = container_of(ptr,
				       struct ath12k_wmi_pdev_sscan_per_detector_info,
				       detector_id);
	parse->num_det_info++;
	return 0;
}

static int ath12k_wmi_tlv_sscan_fw_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct wmi_pdev_sscan_fw_param_parse *parse = data;
	int ret;

	switch (tag) {

	case WMI_TAG_PDEV_SSCAN_FW_CMD_FIXED_PARAM:
		memcpy(&parse->fixed, ptr,
		       sizeof(struct ath12k_wmi_pdev_sscan_fw_cmd_fixed_param));
		parse->fixed.pdev_id = DP_HW2SW_MACID(parse->fixed.pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
	       if (!parse->bin_entry_done) {
		       ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						 ath12k_wmi_pdev_sscan_fft_bin_index_parse,
						 parse);

		       if (ret) {
			       ath12k_warn(ab, "failed to parse fft bin index %d\n",
					   ret);
			       return ret;
		       }

		       parse->bin_entry_done = true;
	       } else if (!parse->det_info_entry_done) {
		       ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						 ath12k_wmi_pdev_sscan_per_detector_info_parse,
						 parse);

		       if (ret) {
			       ath12k_warn(ab, "failed to parse detector info %d\n",
					   ret);
			       return ret;
		       }
		       parse->det_info_entry_done = true;
	       }
	       break;
	case WMI_TAG_PDEV_SSCAN_CHAN_INFO:
	       memcpy(&parse->ch_info, ptr,
		      sizeof(struct ath12k_wmi_pdev_sscan_chan_info));
	       parse->ch_info_valid = true;
	       parse->bin_entry_done = true;
	       break;
	default:
	       break;
	}
	return 0;

}

static void
ath12k_wmi_pdev_sscan_fw_param_event(struct ath12k_base *ab,
				     struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_pdev_sscan_fw_param_parse parse = { };
	struct wmi_pdev_sscan_fw_param_event param;
	int ret;
	u8 pdev_idx;

	ath12k_dbg(ab, ATH12K_DBG_SPECTRAL, "sscan fw param skb: len=%u\n", skb->len);
	if (ath12k_debug_mask & ATH12K_DBG_SPECTRAL)
		print_hex_dump(KERN_DEBUG, "skb->data: ", DUMP_PREFIX_OFFSET, 16, 1,
			       skb->data, skb->len, false);

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_sscan_fw_parse,
				  &parse);

	if (ret) {
		ath12k_warn(ab, "failed to parse padev sscan fw tlv %d\n", ret);
		return;
	}

	param.fixed             = parse.fixed;
	param.bin		= parse.bin;
	param.ch_info		= parse.ch_info;
	param.ch_info_valid	= parse.ch_info_valid;
	param.det_info		= parse.det_info;

	pdev_idx = param.fixed.pdev_id;
	ar = ab->pdevs[pdev_idx].ar;

#ifdef CPTCFG_ATH12K_SPECTRAL
	/* chan_info is only meaningful when FW
	 * advertises WMI_TLV_SERVICE_SPECTRAL_SESSION_INFO_SUPPORT.
	 */
	if (param.ch_info_valid &&
	    test_bit(WMI_TLV_SERVICE_SPECTRAL_SESSION_INFO_SUPPORT,
		     ab->wmi_ab.svc_map)) {
		spin_lock_bh(&ar->spectral.lock);
		/*
		 * Do not update ch_width here. It is set from the vdev
		 * channel at scan start and must reflect the home channel BW.
		 * This event arrives after FFT samples are already processed,
		 * so updating ch_width here would have no effect on them.
		 * Update the agile scan parameters and primary frequency only.
		 */
		ar->spectral.pri20_freq   = param.ch_info.operating_pri20_freq;
		ar->spectral.oper_cfreq1  = param.ch_info.operating_cfreq1;
		ar->spectral.sscan_cfreq1 = param.ch_info.sscan_cfreq1;
		ar->spectral.sscan_cfreq2 = param.ch_info.sscan_cfreq2;
		ar->spectral.sscan_bw     = param.ch_info.sscan_bw;
		spin_unlock_bh(&ar->spectral.lock);
	}
	if (param.det_info) {
		spin_lock_bh(&ar->spectral.lock);
		ar->spectral.start_freq = param.det_info->start_freq;
		ar->spectral.end_freq   = param.det_info->end_freq;
		spin_unlock_bh(&ar->spectral.lock);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "sscan fw param: pdev_id=%u smode=%u bin=%s ch_width=%u pri20_freq=%u cfreq1=%u cfreq2=%u sscan_bw=%u start_freq=%u end_freq=%u num_fft_bin_index=%u num_det_info=%u\n",
		   param.fixed.pdev_id,
		   param.fixed.spectral_scan_mode,
		   param.bin ? "present" : "absent",
		   ar->spectral.ch_width,
		   ar->spectral.pri20_freq,
		   ar->spectral.sscan_cfreq1,
		   ar->spectral.sscan_cfreq2,
		   ar->spectral.sscan_bw,
		   ar->spectral.start_freq,
		   ar->spectral.end_freq,
		   parse.num_fft_bin_index,
		   parse.num_det_info);
	if (param.bin) {
		u32 start_pri80 = FIELD_GET(GENMASK(15, 0), param.bin->pri80_bins);
		u32 end_pri80   = FIELD_GET(GENMASK(31, 16), param.bin->pri80_bins);
		u32 start_sec80 = FIELD_GET(GENMASK(15, 0), param.bin->sec80_bins);
		u32 end_sec80   = FIELD_GET(GENMASK(31, 16), param.bin->sec80_bins);
		u32 start_5mhz  = FIELD_GET(GENMASK(15, 0), param.bin->mid_5mhz_bins);
		u32 end_5mhz    = FIELD_GET(GENMASK(31, 16), param.bin->mid_5mhz_bins);

		ath12k_dbg(ab, ATH12K_DBG_SPECTRAL,
			   "sscan fft_bin_index: start_pri80=%u num_pri80=%u start_sec80=%u num_sec80=%u start_5mhz=%u num_5mhz=%u\n",
			   start_pri80, end_pri80 - start_pri80 + 1,
			   start_sec80, end_sec80 - start_sec80 + 1,
			   start_5mhz,  end_5mhz  - start_5mhz  + 1);
	}
#endif

}

static int
ath12k_wmi_spectral_scan_bw_cap_parse(struct ath12k_base *soc,
				     u16 tag, u16 len,
				     const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	if (tag != WMI_TAG_SPECTRAL_SCAN_BW_CAPABILITIES)
		return -EPROTO;
	parse->num_bw_caps_entry++;
	return 0;
}

static int
ath12k_wmi_spectral_fft_size_cap_parse(struct ath12k_base *soc,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	if (tag != WMI_TAG_SPECTRAL_FFT_SIZE_CAPABILITIES)
		return -EPROTO;

	parse->num_fft_size_caps_entry++;
	return 0;
}

static int
ath12k_wmi_tlv_spectral_cap_parse(struct ath12k_base *ab,
				  u16 tag, u16 len,
				  const void *ptr, void *data)
{
	struct wmi_spectral_capabilities_parse *parse = data;
	int ret;

	if (tag == WMI_TAG_ARRAY_STRUCT) {
		if (!parse->sscan_bw_caps_entry_done) {
			parse->num_bw_caps_entry = 0;
			parse->sscan_bw_caps = (struct ath12k_wmi_spectral_scan_bw_capabilities *)ptr;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					ath12k_wmi_spectral_scan_bw_cap_parse,
					parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse scan bw cap %d\n",
					    ret);
				return ret;
			}
			parse->sscan_bw_caps_entry_done = true;
		} else if (!parse->fft_size_caps_entry_done) {
			parse->num_fft_size_caps_entry = 0;
			parse->fft_size_caps = (struct ath12k_wmi_spectral_fft_size_capabilities *)ptr;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					ath12k_wmi_spectral_fft_size_cap_parse,
					parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse fft size cap %d\n",
					    ret);
				return ret;
			}
			parse->fft_size_caps_entry_done = true;
		}
	}
	return 0;
}

static void
ath12k_wmi_spectral_capabilities_event(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct wmi_spectral_capabilities_parse parse = { };
	struct wmi_spectral_capabilities_event param;
	struct ath12k *ar = NULL;
	int ret, size;
	u8 pdev_id, i;
	struct ath12k_pdev *pdev;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_spectral_cap_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse spectral capabilities tlv %d\n", ret);
		return;
	}

	param.sscan_bw_caps = parse.sscan_bw_caps;
	param.fft_size_caps = parse.fft_size_caps;
	param.num_bw_caps_entry = parse.num_bw_caps_entry;
	param.num_fft_size_caps_entry = parse.num_fft_size_caps_entry;

	pdev_id = param.sscan_bw_caps->pdev_id;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev && pdev->pdev_id == pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ar is NULL for pdev_id %d use default spectral fft size 7",
			   pdev_id);
		return;
	}
	size = sizeof(struct ath12k_wmi_spectral_fft_size_capabilities)*
					param.num_fft_size_caps_entry;

#ifdef CPTCFG_ATH12K_SPECTRAL
	ar->spectral.spectral_cap.fft_size_caps = kzalloc(size, GFP_ATOMIC);
	if (!ar->spectral.spectral_cap.fft_size_caps) {
		ath12k_warn(ab, "Failed to allocate memory");
		return;
	}
	memcpy(ar->spectral.spectral_cap.fft_size_caps,
		param.fft_size_caps, size);
	ar->spectral.spectral_cap.num_bw_caps_entry = param.num_bw_caps_entry;
	ar->spectral.spectral_cap.num_fft_size_caps_entry = param.num_fft_size_caps_entry;
#endif

}

static const char *ath12k_wmi_twt_add_dialog_event_status(u32 status)
{
	switch (status) {
	case WMI_ADD_TWT_STATUS_OK:
		return "ok";
	case WMI_ADD_TWT_STATUS_TWT_NOT_ENABLED:
		return "twt disabled";
	case WMI_ADD_TWT_STATUS_USED_DIALOG_ID:
		return "dialog id in use";
	case WMI_ADD_TWT_STATUS_INVALID_PARAM:
		return "invalid parameters";
	case WMI_ADD_TWT_STATUS_NOT_READY:
		return "not ready";
	case WMI_ADD_TWT_STATUS_NO_RESOURCE:
		return "resource unavailable";
	case WMI_ADD_TWT_STATUS_NO_ACK:
		return "no ack";
	case WMI_ADD_TWT_STATUS_NO_RESPONSE:
		return "no response";
	case WMI_ADD_TWT_STATUS_DENIED:
		return "denied";
	case WMI_ADD_TWT_STATUS_AP_PARAMS_NOT_IN_RANGE:
		return "peer AP wake interval, duration not in range";
	case WMI_ADD_TWT_STATUS_AP_IE_VALIDATION_FAILED:
		return "peer AP IE Validation Failed";
	case WMI_ADD_TWT_STATUS_ROAM_IN_PROGRESS:
		return "Roaming in progress";
	case WMI_ADD_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return "Channel switch in progress";
	case WMI_ADD_TWT_STATUS_SCAN_IN_PROGRESS:
		return "Scan in progress";
	case WMI_ADD_TWT_STATUS_DIALOG_ID_BUSY:
		return "FW is in the process of handling this dialog";
	case WMI_ADD_TWT_STATUS_BTWT_NOT_ENBABLED:
		return "Broadcast TWT is not enabled";
	case WMI_ADD_TWT_STATUS_RTWT_NOT_ENBABLED:
		return "Restricted TWT is not enabled";
	case WMI_ADD_TWT_STATUS_LINK_SWITCH_IN_PROGRESS:
		return "Link switch is ongoing";
	case WMI_ADD_TWT_STATUS_UNSUPPORTED_MODE_MLMR:
		return "Unsupported in MLMR mode";
	case WMI_ADD_TWT_STATUS_UNKNOWN_ERROR:
		fallthrough;
	default:
		return "unknown error";
	}
}

static void ath12k_wmi_twt_add_dialog_event(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_twt_add_dialog_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse wmi twt add dialog status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_TWT_ADD_DIALOG_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch twt add dialog wmi event\n");
		goto exit;
	}

	if (ev->status)
		ath12k_warn(ab,
			    "wmi add twt dialog event vdev %d dialog id %d status %s\n",
			    ev->vdev_id, ev->dialog_id,
			    ath12k_wmi_twt_add_dialog_event_status(ev->status));

exit:
	kfree(tb);
}

static void ath12k_wmi_suspend_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar = NULL;
	const struct wmi_suspend_resp_event *ev;
	struct ath12k_pdev *pdev;
	const void **tb;
	u32 pdev_id, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_PDEV_SUSPEND_EVENT_FIXED_PARAM];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev suspend resp ev");
		kfree(tb);
		return;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);
	kfree(tb);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "WMI suspend event received for pdev_id %d\n", pdev_id);

	if (pdev_id > ab->num_radios)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id received for WMI pdev suspend event\n");
		return;
	}

	ar->pdev_suspend = true;
	complete(&ar->suspend);
}

static void ath12k_wmi_pdev_resume_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_pdev_resume_resp_event *ev;
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	const void **tb;
	u32 pdev_id, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_PDEV_RESUME_EVENT];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev resume resp ev");
		kfree(tb);
		return;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);
	kfree(tb);
	ath12k_dbg(ab, ATH12K_DBG_WMI, "WMI resume event received for pdev_id %d\n", pdev_id);

	if (pdev_id > ab->num_radios)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];

		if (pdev && pdev->pdev_id == pdev_id) {
			ar = pdev->ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id received for WMI pdev resume event\n");
		return;
	}

	ar->pdev_suspend = false;
	complete(&ar->pdev_resume);
}

static void
ath12k_wmi_obss_color_collision_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_obss_color_collision_event *ev;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_OBSS_COLOR_COLLISION_EVT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch obss color collision ev");
		goto exit;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, ev->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "failed to find arvif with vedv id %d in obss_color_collision_event\n",
				ev->vdev_id);
		goto unlock;
	}

	if (ath12k_mac_is_bridge_vdev(arvif))
		goto unlock;

	switch (ev->evt_type) {
	case WMI_BSS_COLOR_COLLISION_DETECTION:
		ar = arvif->ar;
		arvif->obss_color_bitmap = ev->obss_color_bitmap;
		rcu_read_unlock();
		wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arvif->update_obss_color_notify_work);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
				"OBSS color collision detected vdev:%d, event:%d, bitmap:%08llx\n",
				ev->vdev_id, ev->evt_type, ev->obss_color_bitmap);
		goto exit;
	case WMI_BSS_COLOR_COLLISION_DISABLE:
	case WMI_BSS_COLOR_FREE_SLOT_TIMER_EXPIRY:
	case WMI_BSS_COLOR_FREE_SLOT_AVAILABLE:
		goto unlock;
	default:
		ath12k_warn(ab, "received unknown obss color collision detetction event\n");
	}

unlock:
	rcu_read_unlock();
exit:
	kfree(tb);
}

static int
ath12k_wmi_rssi_dbm_conv_subtlv_parser(struct ath12k_base *ab,
				       u16 tag, u16 len,
				       const void *ptr, void *data)
{
	struct wmi_rssi_dbm_conv_offsets *rssi_offsets =
		(struct wmi_rssi_dbm_conv_offsets *) data;
	struct wmi_rssi_dbm_conv_param_info *param_info;
	struct wmi_rssi_dbm_conv_temp_offset *temp_offset_info;
	int i, ret = 0, num_rx_ant, avg_nf = 0;

	switch (tag) {
	case WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO:
		if (len != sizeof(*param_info)) {
			ath12k_warn(ab, "wmi rssi dbm conv subtlv 0x%x invalid len rcvd",
				    tag);
			return -EINVAL;
		}
		param_info = (struct wmi_rssi_dbm_conv_param_info *)ptr;

		num_rx_ant = hweight32(param_info->curr_rx_chainmask);
		if (num_rx_ant < 1 || num_rx_ant > MAX_NUM_ANTENNA) {
			ath12k_warn(ab,
				    "wmi rssi dbm conv subtlv 0x%x recv invalid rx_chain_mask: %d",
				    tag, param_info->curr_rx_chainmask);
			return -EINVAL;
		}
		/* Using minimum pri20 Noise Floor across active chains instead
		 * of all sub-bands*/
		for (i = 0; i < num_rx_ant; i++) {
			if (param_info->curr_rx_chainmask & (0x01 << i))
				avg_nf += param_info->nf_hw_dbm[i][0];
		}
		if (avg_nf)
			avg_nf = avg_nf / num_rx_ant;
		rssi_offsets->avg_nf_dbm = (s8)avg_nf;
		rssi_offsets->xlna_bypass_offset = param_info->xlna_bypass_offset;
		rssi_offsets->xlna_bypass_threshold = param_info->xlna_bypass_threshold;
		break;
	case WMI_TAG_RSSI_DBM_CONVERSION_TEMP_OFFSET_INFO:
		if (len != sizeof(*temp_offset_info)) {
			ath12k_warn(ab, "wmi rssi dbm conv subtlv 0x%x invalid len rcvd",
				    tag);
			return -EINVAL;
		}
		temp_offset_info = (struct wmi_rssi_dbm_conv_temp_offset *)ptr;
		rssi_offsets->rssi_temp_offset = temp_offset_info->rssi_temp_offset;
		break;
	default:
		ath12k_warn(ab, "Received invalid sub-tag for wmi rssi dbm conversion\n");
		ret = -EINVAL;
	}
	return ret;
}

static int
ath12k_wmi_rssi_dbm_conv_event_parser(struct ath12k_base *ab,
				      u16 tag, u16 len,
				      const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi rssi dbm conv tag 0x%x of len %d rcvd",
		   tag, len);
	switch (tag) {
	case WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO_FIXED_PARAM:
		/* Fixed param is already processed*/
		break;
	case WMI_TAG_ARRAY_STRUCT:
		/* len 0 is expected for array of struct when there
		 * is no content of that type inside that tlv
		 */
		if (len == 0)
			return ret;
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_rssi_dbm_conv_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag for wmi rssi dbm conv interference event\n");
		ret = -EINVAL;
		break;

	}

	return ret;
}

static struct
ath12k *ath12k_wmi_rssi_dbm_process_fixed_param(struct ath12k_base *ab,
						u8 *ptr, size_t len)
{
	struct ath12k *ar;
	const struct wmi_tlv *tlv;
	struct wmi_rssi_dbm_conv_event_fixed_param *fixed_param;
	u16 tlv_tag;

	if(!ptr) {
		ath12k_warn(ab, "No data present in rssi dbm conv event\n");
		return NULL;
	}

	if (len < (sizeof(*fixed_param) + TLV_HDR_SIZE)) {
		ath12k_warn(ab, "rssi dbm conv event size invalid\n");
		return NULL;
	}

	tlv = (struct wmi_tlv *)ptr;
	tlv_tag = FIELD_GET(WMI_TLV_TAG, tlv->header);
	ptr += sizeof(*tlv);

	if (tlv_tag == WMI_TAG_RSSI_DBM_CONVERSION_PARAMS_INFO_FIXED_PARAM) {
		fixed_param = (struct wmi_rssi_dbm_conv_event_fixed_param *)ptr;

		ar = ath12k_mac_get_ar_by_pdev_id(ab, fixed_param->pdev_id);
		if (!ar) {
			ath12k_warn(ab, "Failed to get ar for rssi dbm conv event\n");
			return NULL;
		}
	} else {
		ath12k_warn(ab, "rssi dbm conv event received without fixed param tlv at start\n");
		return NULL;
	}

	return ar;
}

static void ath12k_wmi_rssi_dbm_conversion_param_info(struct ath12k_base *ab,
						      struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_rssi_dbm_conv_offsets *rssi_offsets;
	int ret, i;

	/* if pdevs are not active ignore the event */
	for (i = 0; i < ab->num_radios; i++) {
		if (!ab->pdevs_active[i])
			return;
	}

	ar = ath12k_wmi_rssi_dbm_process_fixed_param(ab, skb->data,
						     skb->len);
	if(!ar) {
		ath12k_warn(ab, "failed to get ar from rssi dbm conversion event\n");
		return;
	}

	rssi_offsets = &ar->rssi_offsets;
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rssi_dbm_conv_event_parser,
				  rssi_offsets);
	if (ret) {
		ath12k_warn(ab, "Unable to parse rssi dbm conversion event\n");
		return;
	}

	rssi_offsets->rssi_offset = rssi_offsets->avg_nf_dbm +
				    rssi_offsets->rssi_temp_offset;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "RSSI offset updated, current offset is %d\n",
		   rssi_offsets->rssi_offset);
}

static int ath12k_wmi_tbtt_offset_subtlv_parser(struct ath12k_base *ab, u16 tag,
						u16 len, const void *ptr,
						void *data)
{
	int ret = 0;
	struct ath12k *ar;
	u64 tx_delay = 0;
	struct wmi_tbtt_offset_info *tbtt_offset_info;
	struct ieee80211_chanctx_conf *conf;
	struct ath12k_link_vif *arvif;
	struct ieee80211_bss_conf *link_conf;
	struct ieee80211_vif *vif;

	tbtt_offset_info = (struct wmi_tbtt_offset_info *)ptr;

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, tbtt_offset_info->vdev_id);
	if (!ar) {
		ath12k_warn(ab, "ar not found, vdev_id %d\n", tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	arvif = ath12k_mac_get_arvif(ar, tbtt_offset_info->vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "arvif not found, vdev_id %d\n",
			    tbtt_offset_info->vdev_id);
		ret = -EINVAL;
		goto exit;
	}

	vif = arvif->ahvif->vif;
	if (!arvif->is_up || arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP ||
	    ath12k_mac_is_bridge_vdev(arvif)) {
		ret = 0;
		goto exit;
	}

	arvif->tbtt_offset = tbtt_offset_info->tbtt_offset;

	if (!vif->link_conf[arvif->link_id]) {
		ret = -ENOENT;
		goto exit;
	}

	link_conf = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
				      vif->link_conf[arvif->link_id]);

	if (!link_conf) {
		ret = -ENOENT;
		goto exit;
	}

	if (link_conf->csa_active) {
		ath12k_warn(ab, "Skip TBTT event since CSA is active\n");
		goto exit;
	}

	conf = rcu_dereference(link_conf->chanctx_conf);
	if (!conf) {
		ret = -ENOENT;
		goto exit;
	}

	if (conf->def.chan->band == NL80211_BAND_2GHZ) {
		/* 1Mbps Beacon: */
		/* 144 us ( LPREAMBLE) + 48 (PLCP Header)
		 * + 192 (1Mbps, 24 ytes)
		 * = 384 us + 2us(MAC/BB DELAY
		 */
		tx_delay = 386;
	} else if (conf->def.chan->band == NL80211_BAND_5GHZ ||
		   conf->def.chan->band == NL80211_BAND_6GHZ) {
		/* 6Mbps Beacon: */
		/* 20(lsig)+2(service)+32(6mbps, 24 bytes)
		 * = 54us + 2us(MAC/BB DELAY)
		 */
		tx_delay = 56;
	}
	arvif->tbtt_offset -= tx_delay;
	wiphy_work_queue(ath12k_ar_to_hw(ar)->wiphy, &arvif->update_bcn_template_work);
exit:
	rcu_read_unlock();
	return ret;
}

static int ath12k_wmi_tbtt_offset_event_parser(struct ath12k_base *ab,
					       u16 tag, u16 len,
					       const void *ptr, void *data)
{
	int ret = 0;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "wmi tbtt offset event tag 0x%x of len %d rcvd\n",
		   tag, len);

	switch (tag) {
	case WMI_TAG_TBTT_OFFSET_EXT_EVENT:
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tbtt_offset_subtlv_parser,
					  data);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag 0x%x for wmi tbtt offset event\n", tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ath12k_wmi_pull_tbtt_offset(struct ath12k_base *ab, struct sk_buff *skb,
				       struct wmi_tbtt_offset_ev_arg *arg)
{
	struct wmi_tbtt_offset_info tbtt_offset_info = {0};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tbtt_offset_event_parser,
				  &tbtt_offset_info);
	if (ret) {
		ath12k_warn(ab, "failed to parse tbtt tlv %d\n", ret);
		return -EINVAL;
	}
	return 0;
}

void ath12k_wmi_event_tbttoffset_update(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_tbtt_offset_ev_arg arg = {};
	int ret;

	ret = ath12k_wmi_pull_tbtt_offset(ab, skb, &arg);
	if (ret)
		ath12k_warn(ab, "failed to parse tbtt offset event: %d\n", ret);
}

static int ath12k_wmi_tlv_mlo_reconfig_link_removal_parse(struct ath12k_base *ab,
							  u16 tag, u16 len,
							  const void *ptr, void *data)
{
	struct ath12k_wmi_mlo_link_removal_event_params *param = data, *tmp;
	struct ath12k_wmi_mlo_link_removal_tbtt_update *tbtt;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_MLO_LINK_REMOVAL_EVENT_FIXED_PARAM:
		tmp = (struct ath12k_wmi_mlo_link_removal_event_params *)ptr;
		param->vdev_id = tmp->vdev_id;
		break;
	case WMI_TAG_MLO_LINK_REMOVAL_TBTT_UPDATE:
		tbtt = (struct ath12k_wmi_mlo_link_removal_tbtt_update *)ptr;
		param->tbtt_info.tbtt_count = le32_to_cpu(tbtt->tbtt_count);
		param->tbtt_info.tsf = (u64)(le32_to_cpu(tbtt->tsf_high)) << 32 |
					     le32_to_cpu(tbtt->tsf_low);
		param->tbtt_info.qtimer_reading = (u64)(le32_to_cpu(tbtt->qtimer_reading_high)) << 32 |
							le32_to_cpu(tbtt->qtimer_reading_low);
		break;
	default:
		ath12k_warn(ab, "Received invalid tag:%u\n", tag);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
ath12k_wmi_update_link_reconfig_remove_update(struct ath12k_link_vif *arvif,
					      struct ath12k_wmi_mlo_link_removal_event_params *ev)
{
	return ieee80211_update_link_reconfig_remove_update(arvif->ahvif->vif, arvif->link_id,
							    ev->tbtt_info.tbtt_count,
							    ev->tbtt_info.tsf,
							    ev->tbtt_info.tbtt_count ?
							    NL80211_CMD_LINK_REMOVAL_STARTED :
							    NL80211_CMD_LINK_REMOVAL_COMPLETED);
}

static void ath12k_wmi_event_mlo_reconfig_link_removal(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_wmi_mlo_link_removal_event_params ev = { };
	struct ath12k *ar;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mlo_reconfig_link_removal_parse,
				  &ev);

	if (ret) {
		ath12k_warn(ab, "failed to parse TLV for event:%x ret:%d\n",
			    WMI_MLO_LINK_REMOVAL_EVENTID, ret);
		return;
	}

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, le32_to_cpu(ev.vdev_id));
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "Link removal event received in invalid BSS %d\n",
			    le32_to_cpu(ev.vdev_id));
		return;
	}

	ar = arvif->ar;
	if (!ar) {
		rcu_read_unlock();
		ath12k_warn(ab, "Link removal event received on invalid vdev %d\n",
			    le32_to_cpu(ev.vdev_id));
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "Link removal event received in vdev :%d\n",
		   ev.vdev_id);

	if (ev.tbtt_info.tbtt_count && !arvif->is_link_removal_in_progress) {
		arvif->is_link_removal_update_pending = false;
		arvif->is_link_removal_in_progress = true;
		memset(&arvif->link_removal_data, 0,
		       sizeof(struct ath12k_wmi_mlo_link_removal_event_params));
	}

	if (!ev.tbtt_info.tbtt_count) {
		/* If UMAC migration is in progress then return now. The migration event
		 * will notify the upper layer about link removal event
		 */
		if (arvif->is_umac_migration_in_progress) {
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "UMAC Migration is in progress. Defer sending link removal event to upper layer\n");
			arvif->is_link_removal_update_pending = true;
			memcpy(&arvif->link_removal_data, &ev,
			       sizeof(struct ath12k_wmi_mlo_link_removal_event_params));

			rcu_read_unlock();
			return;
		}

		arvif->is_link_removal_in_progress = false;
		/* the arvif will eventually gets cleared hence no need to reset this*/
		arvif->update_skip_link = true;
	}

	ret = ath12k_wmi_update_link_reconfig_remove_update(arvif, &ev);
	if (ret)
		ath12k_warn(arvif->ar->ab, "sending link removal event FAILED:%d link_id:%d\n",
			    ret, arvif->link_id);

	rcu_read_unlock();
}

static int ath12k_wmi_tlv_peer_migrate_sub_tlv_parse(struct ath12k_base *ab,
						     u16 tag, u16 len,
						     const void *ptr, void *data)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event *parse = data;

	switch (tag) {
	case WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS:
		parse->peer_info[parse->num_pri_link_peer_mig_status++] =
			(struct wmi_mlo_primary_link_peer_migration_status *)ptr;
		break;
	}

	return 0;
}

static int ath12k_wmi_tlv_peer_ptqm_migrate_parse(struct ath12k_base *ab,
						  u16 tag, u16 len,
						  const void *ptr, void *data)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event *parse = data;
	struct wmi_mlo_pri_link_peer_mig_compl_fixed_param *fixed_param;
	int ret = 0;

	switch (tag) {
	case WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_COMPL_FIXED_PARAM:
		fixed_param = (struct wmi_mlo_pri_link_peer_mig_compl_fixed_param *)ptr;
		parse->fixed_param.vdev_id = fixed_param->vdev_id;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		ret = ath12k_wmi_tlv_iter(ab, ptr, len,
					  ath12k_wmi_tlv_peer_migrate_sub_tlv_parse,
					  parse);
		if (ret) {
			ath12k_warn(ab, "failed to parse PTQM event rx sub tlv %d\n", ret);
			return ret;
		}
		break;
	}

	return 0;
}

static void ath12k_wmi_peer_migration_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	struct wmi_mlo_pri_link_peer_migr_compl_event parse = {0};
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	const struct ath12k_hw_ops *hw_ops;
	struct ath12k *ar;
	struct ath12k_hw_group *ag;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	int vdev_id, num_peers, ret, i;
	u16 ml_peer_id;
	u8 status;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_peer_ptqm_migrate_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse mgmt rx tlv %d\n", ret);
		return;
	}

	vdev_id = le32_to_cpu(parse.fixed_param.vdev_id);
	num_peers = le32_to_cpu(parse.num_pri_link_peer_mig_status);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "MLO Peer Migration event for unknow vdev %d\n",
			    vdev_id);
		return;
	}

	ar = arvif->ar;
	ag = ar->ah->ag;
	hw_ops = ar->ab->hw_params->hw_ops;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "MLO Peer Migration event received for vdev %d, num_peers %d\n",
		   vdev_id, num_peers);

	for (i = 0; i < num_peers; i++) {
		union ath12k_config_param val = {0};
		ml_peer_id = le32_get_bits(parse.peer_info[i]->status_info,
					   WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS_ML_PEER_ID);
		status = le32_get_bits(parse.peer_info[i]->status_info,
				       WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_STATUS_STATUS);

		list_for_each_entry_safe(peer_node, tmp_peer, &arvif->peer_migrate_list,
					 list) {
			if (peer_node->ml_peer_id == ml_peer_id) {
				/* WiFi-8 Trigger master migration from host */
				if (hw_ops && hw_ops->dp_peer_migration)
					hw_ops->dp_peer_migration(arvif, peer_node);
				list_del(&peer_node->list);
				kfree(peer_node);
			}
		}

#ifdef CPTCFG_QCN_EXTN
		ath12k_wmi_peer_migration_event_extn(arvif->ahvif);
#endif /* CPTCFG_QCN_EXTN */

		/* Global peer id in case of WiFi-8 */
		if (!(hw_ops && hw_ops->dp_peer_migration))
			ml_peer_id |= ATH12K_PEER_ML_ID_VALID;

		ret = ath12k_dp_peer_get_param_by_peer_id(
				&ar->dp, ml_peer_id,
				ATH12K_DP_PEER_MAC_ADDR_PARAM,
				&val);
		if (ret) {
			ath12k_err(ab, "failed to find ML peer with id %d\n", ml_peer_id);
			goto exit_pri_link_mig_event;
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "peer migration status ML peer id %d status %u MAC %pM\n",
			   ml_peer_id, status, val.addr);

		spin_lock_bh(&ag->ahsta_lock);
		ahsta = ath12k_sta_find_by_addr_and_ahvif(ag, val.addr, arvif->ahvif);
		if (!ahsta) {
			spin_unlock_bh(&ag->ahsta_lock);
			continue;
		}
		ahsta->is_migration_in_progress = false;
		sta = ath12k_ahsta_to_sta(ahsta);

		/* Only disassoc when ML link removal in not in progress since
		 * if migration failed, ML link removal handling will take
		 * care to disconnect this peer
		 */
		if (!arvif->is_link_removal_in_progress &&
		    status != WMI_PRIMARY_LINK_PEER_MIGRATION_SUCCESS) {
			ath12k_mac_peer_disassoc(ab, sta, ahsta,
						 ATH12K_DBG_WMI);
			spin_unlock_bh(&ag->ahsta_lock);
			continue;
		}

		/* if peer is actually migrated, this condition should fail. If not, then
		 * possibly in HTT part some error occured
		 */
		if (!arvif->is_link_removal_in_progress &&
		    ahsta->primary_link_id == arvif->link_id) {
			ath12k_err(ab,
				   "unknown error occured during peer migration, disconnecting\n");
			ath12k_mac_peer_disassoc(ab, sta, ahsta,
						 ATH12K_DBG_WMI);
		}
		spin_unlock_bh(&ag->ahsta_lock);
	}

exit_pri_link_mig_event:
	rcu_read_unlock();

	/* Event is received for all queued ML peers in this arvif */
	if (list_empty(&arvif->peer_migrate_list)) {
		complete_all(&arvif->wmi_migration_event_resp);
		arvif->is_umac_migration_in_progress = false;

		/* Migration happened because of ML removal */
		if (arvif->is_link_removal_in_progress &&
		    arvif->is_link_removal_update_pending) {
			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "ML Link removal update was pending. Send it now\n");

			arvif->is_link_removal_in_progress = false;
			arvif->is_link_removal_update_pending = false;

			ret = ath12k_wmi_update_link_reconfig_remove_update(arvif,
									    &arvif->link_removal_data);
			if (ret)
				ath12k_warn(arvif->ar->ab, "sending link removal event FAILED:%d link_id:%d\n",
					    ret, arvif->link_id);
		}
	}

	return;
}


static void ath12k_wmi_tlv_cfr_cpature_event_fixed_param(const void *ptr,
							 void *data)
{
	struct ath12k_cfr_peer_tx_param *tx_params =
			(struct ath12k_cfr_peer_tx_param *)data;
	struct ath12k_wmi_cfr_peer_tx_event_param *params =
			(struct ath12k_wmi_cfr_peer_tx_event_param *)ptr;

	tx_params->capture_method = params->capture_method;
	tx_params->vdev_id = params->vdev_id;
	ether_addr_copy(tx_params->peer_mac_addr, params->mac_addr.addr);
	tx_params->primary_20mhz_chan = params->chan_mhz;
	tx_params->bandwidth = params->bandwidth;
	tx_params->phy_mode = params->phy_mode;
	tx_params->band_center_freq1 = params->band_center_freq1;
	tx_params->band_center_freq2 = params->band_center_freq2;
	tx_params->spatial_streams = params->sts_count;
	tx_params->correlation_info_1 = params->correlation_info_1;
	tx_params->correlation_info_2 = params->correlation_info_2;
	tx_params->status = params->status;
	tx_params->timestamp_us = params->timestamp_us;
	tx_params->counter = params->counter;
	memcpy(tx_params->chain_rssi, params->chain_rssi,
	       sizeof(tx_params->chain_rssi));

	if (WMI_CFR_CFO_MEASUREMENT_VALID & params->cfo_measurement)
		tx_params->cfo_measurement = FIELD_GET(WMI_CFR_CFO_MEASUREMENT_RAW_DATA,
						       params->cfo_measurement);
	else
		tx_params->cfo_measurement = 0;

	tx_params->rx_start_ts = params->rx_start_ts;
	tx_params->rx_ts_reset = params->rx_ts_reset;
}

static void ath12k_wmi_tlv_cfr_cpature_phase_fixed_param(const void *ptr,
							 void *data)
{
	struct ath12k_cfr_peer_tx_param *tx_params =
			(struct ath12k_cfr_peer_tx_param *)data;
	struct ath12k_wmi_cfr_peer_tx_event_phase_param *params =
			(struct ath12k_wmi_cfr_peer_tx_event_phase_param *)ptr;
	int i;

	for (i = 0; i < WMI_MAX_CHAINS; i++) {
		tx_params->chain_phase[i] = params->chain_phase[i];
		tx_params->agc_gain[i] = params->agc_gain[i];
		tx_params->agc_gain_tbl_index[i] = params->agc_gain_tbl_index[i];
	}
}

static int ath12k_wmi_tlv_cfr_capture_evt_parse(struct ath12k_base *ab,
						u16 tag, u16 len,
						const void *ptr, void *data)
{
	switch (tag) {
	case WMI_TAG_PEER_CFR_CAPTURE_EVENT:
		ath12k_wmi_tlv_cfr_cpature_event_fixed_param(ptr, data);
		break;
	case WMI_TAG_CFR_CAPTURE_PHASE_PARAM:
		ath12k_wmi_tlv_cfr_cpature_phase_fixed_param(ptr, data);
		break;
	default:
		ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
		tag, len);
		return -EINVAL;
	}

	return 0;
}

static void ath12k_wmi_parse_cfr_capture_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct ath12k_cfr_peer_tx_param params = {};
	int ret;

	ath12k_dbg_dump(ab, ATH12K_DBG_CFR_DUMP, "cfr_dump:", "",
			skb->data, skb->len);

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_cfr_capture_evt_parse,
				  &params);
	if (ret) {
		ath12k_warn(ab, "failed to parse cfr capture event tlv %d\n",
			    ret);
		return;
	}

	ret = ath12k_process_cfr_capture_event(ab, &params);
	if (ret)
		ath12k_dbg(ab, ATH12K_DBG_CFR,
			   "failed to process cfr cpature ret = %d\n", ret);
}

static int
ath12k_wmi_tlv_enh_aoa_phasedelta_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr,
					void *data)
{
	struct ath12k_wmi_enhanced_aoa_phasedelta_parse *parse = data;

	switch (tag) {
	case WMI_TAG_PDEV_ENHANCED_AOA_PHASEDELTA_EVENT_FIXED_PARAM:
		memcpy(&parse->fixed_param, ptr, sizeof(parse->fixed_param));
		break;
	case WMI_TAG_ARRAY_STRUCT:
		parse->data_hdr = ptr;
		parse->num_data_hdr = len / sizeof(*parse->data_hdr);
		break;
	case WMI_TAG_ARRAY_UINT32:
		parse->data_buf = ptr;
		parse->data_buf_len = len;
		break;
	default:
		break;
	}

	return 0;
}


static void
ath12k_wmi_pdev_enh_aoa_phasedelta_event(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct ath12k_wmi_enhanced_aoa_phasedelta_parse parse = {};
	struct ath12k *ar;

	if (ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				ath12k_wmi_tlv_enh_aoa_phasedelta_parse,
				&parse)) {
		ath12k_warn(ab, "Failed to parse enhanced aoa phase delta event");
		return;
	}

	if (!parse.data_hdr || !parse.data_buf) {
		ath12k_warn(ab, "failed to get the enhanced aoa data");
		return;
	}

	ar = ath12k_mac_get_ar_by_pdev_id(ab,
					  __le32_to_cpu(parse.fixed_param.pdev_id));
	if (!ar) {
		ath12k_warn(ab, "failed to get the pdev to parse enhanced aoa");
		return;
	}

	ath12k_wmi_cfr_handle_aoa_data(ar, &parse);
}

static void ath12k_process_ocac_complete_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_vdev_adfs_ocac_complete_event_fixed_param *ev;
	struct ath12k *ar;
	u32 vdev_id, status;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_VDEV_ADFS_OCAC_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch ocac completed ev\n");
		kfree(tb);
		return;
	}

	vdev_id = __le32_to_cpu(ev->vdev_id);
	status = __le32_to_cpu(ev->status);
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);

	if (!ar) {
		ath12k_warn(ab, "OCAC complete event in invalid vdev %u\n",
			    vdev_id);
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "aDFS OCAC %s on vdev %u: status %u, chan freq %u, chan_width %u, center %u, center1 %u, center2 %u\n",
		   status ? "aborted" : "completed", vdev_id,
		   status, __le32_to_cpu(ev->chan_freq),
		   __le32_to_cpu(ev->chan_width),
		   __le32_to_cpu(ev->center_freq),
		   __le32_to_cpu(ev->center_freq1),
		   __le32_to_cpu(ev->center_freq2));

	/* Validate that the event is for the channel we are currently
	 * tracking. A stale ACK for a previous channel (e.g. firmware
	 * completing an abort we issued before starting a new CAC) carries
	 * a different freq — ignore it to avoid disturbing the new CAC.
	 */
	if (ar->agile_chandef.chan &&
	    ar->agile_chandef.center_freq1 != __le32_to_cpu(ev->center_freq1)) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "aDFS OCAC event for stale center_freq1 %u MHz (current %u MHz), ignoring",
			   __le32_to_cpu(ev->center_freq1),
			   ar->agile_chandef.center_freq1);
		goto exit;
	}

	if (status) {
		if (ar->agile_abort_pending) {
			/* WMI ACK for an abort we initiated via
			 * ath12k_mac_op_abort_radar_background(). cfg80211 was
			 * already notified by the stop path; do not forward.
			 */
			ar->agile_abort_pending = false;
		} else {
			/* Firmware-initiated abort — cfg80211 must be notified. */
			ath12k_mac_background_dfs_event(ar, ATH12K_BGDFS_ABORT);
		}
		memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
		goto exit;
	}

	memset(&ar->agile_chandef, 0, sizeof(struct cfg80211_chan_def));
	ar->ap_ps_disabled_by_agile = false;
	/* Agile CAC succeeded: re-evaluate GAP eligibility.
	 * For pre-CAC, hostapd will immediately call set_radar_background
	 * for the next channel, disabling GAP again. For single-channel
	 * CAC (iw dev), this re-enables GAP as expected.
	 * Queue work to run in wiphy/sleepable context since
	 * ap_ps_recalc sends a WMI command.
	 */
	wiphy_work_queue(ar->ah->hw->wiphy, &ar->ap_ps_recalc_wq);
exit:
	kfree(tb);
}

static void ath12k_wmi_tid_to_link_map_event(struct ath12k_base *ab,
					     struct sk_buff *skb)
{
	const struct wmi_tid_to_link_mapping_event *ev;
	struct ath12k_link_vif *arvif;
	u16 mapping_switch_tsf;
	u32 status_type;
	const void **tb;
	u32 vdev_id;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_MLO_TID_TO_LINK_MAPPING_EVENT_FIXED_PARAM];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch TID to link mapping ev");
		kfree(tb);
		return;
	}

	vdev_id = le32_to_cpu(ev->vdev_id);
	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "failed to find arvif with vedv id %d in tid_to_link_map_event_event\n",
			    vdev_id);
		goto unlock;
	}

	mapping_switch_tsf = le32_get_bits(ev->mapping_switch_tsf,
					   WMI_TTLM_MAPPING_SWITCH_TSF_BITS);
	status_type = le32_to_cpu(ev->status_type);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "TID to link mapping event received.  vdev:%d, status:%d, mapping_switch_tsf: %u\n",
		   vdev_id, status_type, mapping_switch_tsf);
	ath12k_tid_to_link_mapping_evt_notify(arvif, mapping_switch_tsf,
					      status_type);

unlock:
	rcu_read_unlock();
	kfree(tb);
}

static int ath12k_wmi_tlv_mlo_3_link_tlt_evt_parse(struct ath12k_base *ab,
                                                   u16 tag, u16  len,
                                                   const void *ptr, void *data)
{
        struct mlo_tlt_selection_evt_params *tlt_sel_params;
        struct wmi_mlo_tlt_selection_for_tid_spray_event *ev;
        int i;

        switch (tag) {
        case WMI_TAG_MLO_TLT_SELECTION_FOR_TID_SPRAY_EVENT_FIXED_PARAM:
                tlt_sel_params = (struct mlo_tlt_selection_evt_params *)data;
                ev = (struct wmi_mlo_tlt_selection_for_tid_spray_event *)ptr;

                /* copy mld mac address */
                ether_addr_copy(tlt_sel_params->mld_addr, ev->mld_mac.addr);

                for (i = 0; i < WMI_TLT_MAX_LINKS; i++) {
                        /* fill link bit map values */
                        if (i < WMI_TLT_NUM_TID_PER_AC)
                                tlt_sel_params->link_bmap[i] =
                                        __le32_to_cpu(ev->link_bmap[i]);

                        /* fill link priority */
                        tlt_sel_params->link_priority[i] =
                                __le32_to_cpu(ev->hwlink_priority[i]);
                }
                break;

        default:
                ath12k_warn(ab, "Invalid tag received tag %d len %d\n",
                            tag, len);
                break;
        }
        return 0;
}

static u32 ath12k_mlo_get_link_maxphyrate(struct ath12k_base *ab,
					   struct rate_info *txrate,
					   u8 peer_hw_link_id,
					   u16 hw_link_id)
{
	if (hw_link_id == INVALID_HW_LINK_ID) {
		ath12k_err(ab, "invalid hw link id is passed: %d", hw_link_id);
		return 0;
	}

	if (hw_link_id != peer_hw_link_id)
		return 0;

	return cfg80211_calculate_bitrate(txrate);
}

static void
ath12k_update_peer_tlt_selection_weights(struct ath12k *ar,
					 struct ath12k_link_sta *arsta,
					 void *data)
{
	struct mlo_tlt_selection_evt_params *evt_params = data;
	struct ath12k_base *ab = ar->ab;
	u8 tid_weight[ATH12K_DATA_TID_MAX] = {0};
	u8 primary_tid_weight = 0;
	u8 secondary_tid_weight = 0;
	u32 primary_tid_capacity = 0;
	u32 secondary_tid_capacity = 0;
	u64 total_capacity = 0;
	union ath12k_config_param val = {0};
	union ath12k_config_param link_val = {0};
	struct ath12k_dp_peer *dp_peer;
	u8 i = 0;
	u8 tid_bitmap = 0;
	u8 common_link_tid_bitmap = 0;
	u32 common_link_capacity = 0;
	u8 link_id = arsta->link_id, hw_link_id = ar->hw_link_id;
	int ret;

	/* check whether link bitmap is valid or not */
	if ((evt_params->link_bmap[0] == 0) ||
	    (evt_params->link_bmap[1] == 0)) {
		ath12k_err(ab, "invalid link bitmap received "
			   "primary tid bitmap = %d secondary tid bitmap = %d",
			   evt_params->link_bmap[0], evt_params->link_bmap[1]);
		return;
	}

	if (!arsta->ahsta || !ath12k_ahsta_to_sta(arsta->ahsta)->valid_links)
		return;

	/* non 3 link association */
	if (hweight16(ath12k_ahsta_to_sta(arsta->ahsta)->valid_links) !=
	    ATH12K_3LINK_MLO_MAX_STA_LINKS)
		return;

	dp_peer = ath12k_sta_get_dp_peer_rcu(arsta->ahsta);
	if (!dp_peer) {
		ath12k_warn(ab, "tlt selection: dp_peer not found for %pM\n",
			    arsta->addr);
		return;
	}

	ret = ath12k_dp_link_peer_get_param_by_dp_peer_and_link_id(
			dp_peer, link_id,
			ATH12K_DP_LINK_PEER_TXRATE_PARAM, &link_val);
	if (ret) {
		ath12k_warn(ab, "tlt selection: txrate get failed for %pM link %u\n",
			    arsta->addr, link_id);
		return;
	}

	/* find the common link */
	common_link_tid_bitmap =
		evt_params->link_bmap[0] & evt_params->link_bmap[1];
	if (common_link_tid_bitmap) {
		common_link_capacity =
			ath12k_mlo_get_link_maxphyrate(ab,
						       &link_val.rate_info.txrate,
						       hw_link_id,
						       GET_3_LINK_TX_HW_LINK_ID(common_link_tid_bitmap));
		/* distribute the common link capacity */
		if (common_link_capacity) {
			primary_tid_capacity = common_link_capacity / 2;
			secondary_tid_capacity = common_link_capacity / 2;
		}
	}

	/* calculate primary tid capacity */
	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		/* skip the common link capacity addition */
		tid_bitmap = ((evt_params->link_bmap[0]) & (1 << i));
		if (tid_bitmap && (tid_bitmap != common_link_tid_bitmap)) {
			primary_tid_capacity +=
				ath12k_mlo_get_link_maxphyrate(ab,
							       &link_val.rate_info.txrate,
							       hw_link_id,
							       GET_3_LINK_TX_HW_LINK_ID(tid_bitmap));
		}
	}

	/* calculate secondary tid capacity */
	for (i = 0; i < ATH12K_DATA_TID_MAX; i++) {
		/* skip the common link capacity addition */
		tid_bitmap = ((evt_params->link_bmap[1]) & (1 << i));
		if (tid_bitmap && (tid_bitmap != common_link_tid_bitmap)) {
			secondary_tid_capacity +=
				ath12k_mlo_get_link_maxphyrate(ab,
							       &link_val.rate_info.txrate,
							       hw_link_id,
							       GET_3_LINK_TX_HW_LINK_ID(tid_bitmap));
		}
	}

	if (!primary_tid_capacity || !secondary_tid_capacity) {
		ath12k_err(ab, "Invalid link phy rate peer mld mac address: %pM"
			   " link_priority = %d:%d:%d primary tid bitmap = %d"
			   " secondary tid bitmap= %d common link bit map = %d",
			   evt_params->mld_addr, evt_params->link_priority[0],
			   evt_params->link_priority[1],
			   evt_params->link_priority[2],
			   evt_params->link_bmap[0], evt_params->link_bmap[1],
			   common_link_tid_bitmap);
		return;
	}

	total_capacity = primary_tid_capacity + secondary_tid_capacity;

	/* percentage calculation */
	primary_tid_weight = div64_u64((u64)(primary_tid_capacity * 100),
				       total_capacity);
	secondary_tid_weight = div64_u64((u64)(secondary_tid_capacity * 100),
					 total_capacity);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "peer mld mac address: %pM, link_priority = %d:%d:%d"
		   " primary_link_capacity = %d secondary_link_capacity = %d"
		   " primary_tid_weight = %d secondary_tid_weight = %d"
		   " primary tid bitmap = %d secondary tid bitmap = %d"
		   " common link capacity = %d common link bit map = %d",
		   evt_params->mld_addr, evt_params->link_priority[0],
		   evt_params->link_priority[1], evt_params->link_priority[2],
		   primary_tid_capacity, secondary_tid_capacity,
		   primary_tid_weight, secondary_tid_weight,
		   evt_params->link_bmap[0], evt_params->link_bmap[1],
		   common_link_capacity, common_link_tid_bitmap);

	/* best effort */
	tid_weight[0] = primary_tid_weight;
	tid_weight[3] = secondary_tid_weight;

	/* background */
	tid_weight[1] = primary_tid_weight;
	tid_weight[2] = secondary_tid_weight;

	/* video */
	tid_weight[4] = primary_tid_weight;
	tid_weight[5] = secondary_tid_weight;

	/* voice */
	tid_weight[6] = primary_tid_weight;
	tid_weight[7] = secondary_tid_weight;

	memcpy(val.tid_weight, tid_weight, sizeof(tid_weight));

	/* Write the computed tid_weight[] back via the param API */
	ret = ath12k_dp_link_peer_set_param_by_dp_peer_and_link_id(
			dp_peer, link_id,
			ATH12K_DP_LINK_PEER_TID_WEIGHT_PARAM, &val);
	if (ret)
		ath12k_warn(ab,
			    "tlt selection: tid_weight set failed for %pM link %u\n",
			    arsta->addr, link_id);
}

static void ath12k_wmi_mlo_3_link_tlt_selection(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct mlo_tlt_selection_evt_params tlt_sel_params = {0};
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mlo_3_link_tlt_evt_parse,
				  &tlt_sel_params);
	if (ret) {
		ath12k_warn(ab, "failed to fetch tlt selection tlv %d", ret);
		return;
	}

	if (!ath12k_arsta_itr_on_ab_by_addr(ab, tlt_sel_params.mld_addr,
					    ath12k_update_peer_tlt_selection_weights,
					    &tlt_sel_params))
		ath12k_warn(ab, "tlt selection: peer not found %pM\n",
			    tlt_sel_params.mld_addr);
}

static void
ath12k_wmi_pktlog_decode_info(struct ath12k_base *ab,
                                  struct sk_buff *skb)
{
	struct ath12k *ar;
	const void **tb;
	int ret;
	u32 pdev_id;
	struct ath12k_pktlog *pktlog;
	const struct ath12k_pl_fw_info *pktlog_info;

	if (!test_bit(WMI_TLV_SERVICE_PKTLOG_DECODE_INFO_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_warn(ab, "firmware doesn't support pktlog decode info support\n");
		return;
	}
	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}
	pktlog_info = tb[WMI_TAG_PDEV_PKTLOG_DECODE_INFO];
	if (!pktlog_info) {
		ath12k_warn(ab, "failed to fetch pktlog debug info");
		kfree(tb);
		return;
	}

	pdev_id = DP_SW2HW_MACID(pktlog_info->pdev_id);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog pktlog_defs_json_version: %d", pktlog_info->pktlog_defs_json_version);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog software_image: %s", pktlog_info->software_image);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog chip_info: %s", pktlog_info->chip_info);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog pdev_id: %d", pdev_id);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in pktlog decode info %d", pdev_id);
		kfree(tb);
		return;
	}
	pktlog = &ar->debug.pktlog;
	pktlog->fw_version_record = 1;
	if (pktlog->buf == NULL) {
		ath12k_warn(ab, "failed to initialize, start pktlog\n");
		kfree(tb);
		return;
	}
	pktlog->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM_FW_VERSION_SUPPORT;
	memcpy(pktlog->buf->bufhdr.software_image, pktlog_info->software_image, sizeof(pktlog_info->software_image));
	memcpy(pktlog->buf->bufhdr.chip_info, pktlog_info->chip_info, sizeof(pktlog_info->chip_info));
	pktlog->buf->bufhdr.pktlog_defs_json_version = pktlog_info->pktlog_defs_json_version;
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pktlog new magic_num: 0x%x\n", pktlog->buf->bufhdr.magic_num);
	kfree(tb);
}

static int ath12k_pull_vdev_tsf_report_ev(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  struct wmi_vdev_host_tsf_arg *arg)
{
	const struct wmi_vdev_tsf_report_event *ev;
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tsf report tlv: %d\n", ret);
		return ret;
	}
	ev = tb[WMI_TAG_VDEV_TSF_REPORT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev tsf report event\n");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = le32_to_cpu(ev->vdev_id);
	arg->tsf = ((u64)le32_to_cpu(ev->tsf_high) << 32) |
		le32_to_cpu(ev->tsf_low);
	arg->tsf_low = le32_to_cpu(ev->tsf_low);
	arg->tsf_high = le32_to_cpu(ev->tsf_high);
	arg->qtimer_low = le32_to_cpu(ev->qtimer_low);
	arg->qtimer_high = le32_to_cpu(ev->qtimer_high);
	arg->tsf_id = le32_to_cpu(ev->tsf_id);
	arg->tsf_id_valid = le32_to_cpu(ev->tsf_id_valid);
	arg->mac_id = le32_to_cpu(ev->mac_id);
	arg->mac_id_valid = le32_to_cpu(ev->mac_id_valid);
	arg->wlan_global_tsf_low = le32_to_cpu(ev->wlan_global_tsf_low);
	arg->wlan_global_tsf_high = le32_to_cpu(ev->wlan_global_tsf_high);
	arg->tqm_timer_low = le32_to_cpu(ev->tqm_timer_low);
	arg->tqm_timer_high = le32_to_cpu(ev->tqm_timer_high);
	arg->use_tqm_timer = le32_to_cpu(ev->use_tqm_timer);

	kfree(tb);
	return 0;
}

static void ath12k_vdev_tsf_report_event(struct ath12k_base *ab,
					 struct sk_buff *skb)
{
	struct wmi_vdev_host_tsf_arg tsf_event = {};
	struct ath12k *ar;

	if (ath12k_pull_vdev_tsf_report_ev(ab, skb, &tsf_event) != 0) {
		ath12k_warn(ab, "failed to extract vdev tsf report event\n");
		return;
	}
	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, tsf_event.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id %d in tsf report event\n",
			    tsf_event.vdev_id);
		rcu_read_unlock();
		return;
	}
	ar->tsf_report = tsf_event;
	complete(&ar->tsf_report_done);
	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "vdev tsf report: vdev_id %d tsf %llu qtimer %u:%u\n",
		   tsf_event.vdev_id, tsf_event.tsf,
		   tsf_event.qtimer_high, tsf_event.qtimer_low);
}

int ath12k_wmi_send_pcie_gen_lane(struct ath12k *ar, u32 enable, u32 config_type,
				  u32 pcie_gen, u32 pcie_lane)
{
	struct wmi_energy_mgmt_pcie_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI Set PCIe GenXLane config type to %u\n", config_type);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_energy_mgmt_pcie_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ENERGY_MGMT_PCIE_CMD_FIXED_PARAM,
						 len);

	cmd->enable = cpu_to_le32(enable);
	cmd->config = cpu_to_le32(config_type);
	cmd->pcie_gen = cpu_to_le32(pcie_gen);
	cmd->pcie_lane = cpu_to_le32(pcie_lane);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENERGY_MGMT_PCIE_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "WMI failed to send PCIe config command\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_pcie_low_power(struct ath12k *ar, u32 enable, u32 config_type)
{
	struct wmi_energy_mgmt_pcie_lpm_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI Set PCIe Low Power state %u\n", config_type);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_energy_mgmt_pcie_lpm_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(
					    WMI_TAG_ENERGY_MGMT_PCIE_LPM__CMD_FIXED_PARAM,
						 len);

	cmd->enable = cpu_to_le32(enable);
	cmd->config = cpu_to_le32(config_type);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENERGY_MGMT_PCIE_LPM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI pdev %i failed to send PCIe Low Power enable/disable command\n",
			    ar->pdev->pdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_dcvs_cmd(struct ath12k *ar, u32 config)
{
	struct wmi_energy_mgmt_dcvs_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI Set DCVS mode to %u\n", config);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_energy_mgmt_dcvs_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ENERGY_MGMT_DCVS_CMD_FIXED_PARAM,
						 len);
	cmd->config = cpu_to_le32(config);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENERGY_MGMT_DCVS_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "WMI failed to send DCVS config command\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_dps_assist_cmd(struct ath12k *ar, u32 vdev_id, u32 config)
{
	struct wmi_energy_mgmt_dps_assist_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI Set DPS assisting mode to %s\n",
		   config ? "enable" : "disable");

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_energy_mgmt_dps_assist_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(
				   WMI_TAG_ENERGY_MGMT_DPS_ASSISTING_ROLE_CMD_FIXED_PARAM,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->config = cpu_to_le32(config);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_ENERGY_MGMT_DPS_ASSISTING_ROLE_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI failed to send DPS assisting role config command\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_low_power_20mhz(struct ath12k *ar, bool config)
{
	struct wmi_energy_mgmt_eco_mode_cmd *cmd;
	struct sk_buff *skb;
	u32 wmi_config;
	int ret, len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI Set 20 MHz Low Power mode to %s\n",
		   config ? "enable" : "disable");

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_energy_mgmt_eco_mode_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(
					     WMI_TAG_ENERGY_MGMT_ECO_MODE_CMD_FIXED_PARAM,
					     len);
	cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));

	if (config)
		wmi_config = WMI_LOW_POWER_20MHZ_ENABLE;
	else
		wmi_config = WMI_LOW_POWER_20MHZ_DISABLE;

	cmd->enable = cpu_to_le32(wmi_config);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENERGY_MGMT_ECO_MODE_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI failed to send 20 MHz Low Power config command\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

static void ath12k_vdev_tpc_ie_power_event(struct ath12k_base *ab,
					   struct sk_buff *skb)
{
	const struct wmi_vdev_tpc_ie_power_event *ev;
	struct ath12k_link_vif *arvif;
	const void **tb;
	u32 vdev_id;
	s32 tx_pwr_qdbm;
	s32 tpc_eirp_dbm;
	s32 old_tpc_eirp_dbm;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tpc ie power event tlv: %ld\n",
			    PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_VDEV_TPC_IE_POWER_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch tpc ie power event\n");
		kfree(tb);
		return;
	}

	vdev_id = le32_to_cpu(ev->vdev_id);
	tx_pwr_qdbm = a_sle32_to_cpu(ev->tx_pwr);
	tpc_eirp_dbm = tx_pwr_qdbm / ATH12K_TPC_QDBM_PER_DBM;
	if (tpc_eirp_dbm > ATH12K_TPC_EIRP_DBM_MAX)
		tpc_eirp_dbm = ATH12K_TPC_EIRP_DBM_MAX;
	else if (tpc_eirp_dbm < ATH12K_TPC_EIRP_DBM_MIN)
		tpc_eirp_dbm = ATH12K_TPC_EIRP_DBM_MIN;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi tpc ie power event vdev %u pdev %u qdbm %d dbm %d\n",
		   vdev_id, le32_to_cpu(ev->pdev_id), tx_pwr_qdbm,
		   tpc_eirp_dbm);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		ath12k_warn(ab, "invalid vdev id in tpc ie power event %u\n",
			    vdev_id);
		rcu_read_unlock();
		kfree(tb);
		return;
	}

	old_tpc_eirp_dbm = arvif->tpc_ie_eirp;
	if (old_tpc_eirp_dbm == tpc_eirp_dbm) {
		rcu_read_unlock();
		kfree(tb);
		return;
	}

	arvif->tpc_ie_eirp = tpc_eirp_dbm;

	if (arvif->is_up && arvif->ar)
		wiphy_work_queue(ath12k_ar_to_hw(arvif->ar)->wiphy,
				 &arvif->tpc_ie_eirp_work);

	rcu_read_unlock();
	kfree(tb);
}

int ath12k_wmi_send_tdma_schedule_request(struct ath12k *ar,
					  const struct ath12k_tdma_sched_info *sched)
{
	struct wmi_tdma_schedule_request_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int ret, len, i;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_tdma_schedule_request_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TDMA_SCHEDULE_REQUEST_CMD,
						  len);
	cmd->pdev_id = cpu_to_le32(sched->pdev_id);
	cmd->schedule_type = cpu_to_le32(sched->sched_type);
	cmd->schedule_handle_id = cpu_to_le32(sched->sched_id);
	ether_addr_copy(cmd->owner_bssid.addr, sched->bssid);
	cmd->start_time_tsf_low = cpu_to_le32(sched->start_time_tsf_low);
	cmd->start_time_tsf_high = cpu_to_le32(0);
	cmd->num_busy_slots = cpu_to_le32(sched->num_busy_slots);
	cmd->busy_slot_dur_ms = cpu_to_le32(sched->busy_slot_dur_ms);
	cmd->busy_slot_intvl_ms = cpu_to_le32(sched->busy_slot_intvl_ms);
	cmd->edca_params_valid = cpu_to_le32(sched->edca_params_valid ? 1 : 0);

	for (i = 0; i < WMI_AC_MAX; i++) {
		cmd->aifsn[i] = cpu_to_le32(sched->aifsn[i]);
		cmd->ecwmin[i] = cpu_to_le32(sched->cwmin[i]);
		cmd->ecwmax[i] = cpu_to_le32(sched->cwmax[i]);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI TDMA schedule request pdev %u type %u id %u\n",
		   sched->pdev_id, sched->sched_type, sched->sched_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_TDMA_SCHEDULE_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI failed to send TDMA schedule request command: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_energy_mgmt_oem_data(struct ath12k *ar, u32 content_type,
					 u32 num_bytes_valid, u8 *data)
{
	struct wmi_energy_mgmt_oem_data_cmd *cmd;
	size_t aligned_len;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int len, ret;
	void *ptr;

	aligned_len = roundup(num_bytes_valid, sizeof(u32));
	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_energy_mgmt_oem_data_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_ENERGY_MGMT_OEM_DATA_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->content_type = content_type;
	cmd->num_bytes_valid = num_bytes_valid;

	ptr += sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, data, num_bytes_valid);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI energy service OEM Data cmd content type %u\n",
		   content_type);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENERGY_MGMT_OEM_DATA_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send energy service OEM Data wmi cmd\n");
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

static void ath12k_wmi_energy_mgmt_oem_data_event(struct ath12k_base *ab,
						  struct sk_buff *skb)
{
	const struct wmi_energy_mgmt_oem_data_event *oem_data_event;
	const struct wmi_tlv *tlv;
	struct ieee80211_hw *hw;
	struct ath12k *ar;
	u32 content_type, num_bytes_valid;
	const void **tb;
	u8 radio_id;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse OEM data tlv\n");
		return;
	}
	oem_data_event = tb[WMI_ENERGY_MGMT_OEM_DATA_EVENT_FIXED_PARAM];
	if (!oem_data_event) {
		ath12k_warn(ab, "failed to fetch OEM data event\n");
		goto end;
	}

	content_type = le32_to_cpu(oem_data_event->content_type);
	num_bytes_valid = le32_to_cpu(oem_data_event->num_bytes_valid);

	tlv = tb[WMI_TAG_ARRAY_BYTE];

	rcu_read_lock();
	ar = ath12k_mac_get_any_ar(ab);
	if (!ar) {
		ath12k_warn(ab, "invalid ar for Energy Service OEM data event\n");
		rcu_read_unlock();
		goto end;
	}
	hw = ar->ah->hw;
	radio_id = ar->radio_idx;
	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "Energy Service OEM data event");

	ath12k_vendor_send_es_oem_data(hw, radio_id, content_type,
				       num_bytes_valid, tlv->value);
end:
	kfree(tb);
}

static void ath12k_wmi_pdev_rf_path_resp_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	const struct wmi_pdev_set_rf_path_resp_event *ev;
	struct ath12k *ar = NULL;
	const void **tb;
	u32 pdev_id;
	int ret, i;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse rf_path resp TLV: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_SET_RF_PATH_RESP_EVENT_FIXED_PARAM];
	if (!ev) {
		ath12k_warn(ab, "rf_path resp event TLV not found\n");
		goto out;
	}

	pdev_id = le32_to_cpu(ev->pdev_id);

	/* pdevs_active may be NULL before any VAP is created; use pdevs[] instead. */
	for (i = 0; i < ab->num_radios; i++) {
		if (ab->pdevs[i].pdev_id == pdev_id) {
			ar = ab->pdevs[i].ar;
			break;
		}
	}

	if (!ar) {
		ath12k_warn(ab, "rf_path resp: invalid pdev_id %u\n", pdev_id);
		goto out;
	}

	ar->rf_path_ctx.is_fw_resp_success = !le32_to_cpu(ev->status);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "rf_path resp: pdev_id %u rf_path %u status %u\n",
		   pdev_id,
		   le32_to_cpu(ev->rf_path),
		   le32_to_cpu(ev->status));

	complete(&ar->rf_path_ctx.rf_switch_done);
out:
	kfree(tb);
}

struct ath12k_nfcal_parse_state {
	const struct wmi_pdev_nfcal_power_all_channels_event *ev;
	struct ath12k_wmi_nfcal_power_event *param;
	u32 array_idx;
};

static void ath12k_wmi_nfcal_parse_array(const void *ptr, u16 len,
					 void *dst, size_t elem_size,
					 u32 max_elems, u32 *count)
{
	const struct wmi_tlv *tlv;
	u16 tlv_len;
	u32 i = 0;

	while (len >= sizeof(*tlv) && i < max_elems) {
		tlv = ptr;
		tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);
		if (tlv_len > len || tlv_len < elem_size)
			break;
		memcpy((u8 *)dst + i * elem_size, ptr, elem_size);
		ptr += tlv_len;
		len -= tlv_len;
		i++;
	}
	*count = i;
}

static int ath12k_wmi_nfcal_power_tlv_iter(struct ath12k_base *ab, u16 tag,
					   u16 len, const void *ptr,
					   void *data)
{
	struct ath12k_nfcal_parse_state *s = data;
	struct ath12k_wmi_nfcal_power_event *param = s->param;
	u32 count;

	switch (tag) {
	case WMI_TAG_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENT:
		if (len >= sizeof(*s->ev))
			s->ev = ptr;
		break;
	case WMI_TAG_ARRAY_STRUCT:
		switch (s->array_idx++) {
		case 0: /* nfdbr array */
			ath12k_wmi_nfcal_parse_array(ptr, len,
						     param->nfdbr,
						     sizeof(*param->nfdbr),
						     ATH12K_WMI_RXG_CAL_CHAN_MAX *
						     ATH12K_WMI_MAX_NUM_CHAINS,
						     &count);
			param->num_nfdbr_dbm = (u16)count;
			break;
		case 1: /* nfdbm array */
			ath12k_wmi_nfcal_parse_array(ptr, len,
						     param->nfdbm,
						     sizeof(*param->nfdbm),
						     ATH12K_WMI_RXG_CAL_CHAN_MAX *
						     ATH12K_WMI_MAX_NUM_CHAINS,
						     &count);
			break;
		case 2: /* freqnum array */
			ath12k_wmi_nfcal_parse_array(ptr, len,
						     param->freqnum,
						     sizeof(*param->freqnum),
						     ATH12K_WMI_RXG_CAL_CHAN_MAX,
						     &count);
			param->num_freq = (u16)count;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath12k_wmi_pdev_nfcal_power_all_channels_event(struct ath12k_base *ab,
							   struct sk_buff *skb)
{
	struct ath12k_wmi_nfcal_power_event param = {};
	struct ath12k_nfcal_parse_state s = { .param = &param };
	struct ath12k *ar;
	int ret;

	ath12k_info(ab, "WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID received from FW\n");

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_nfcal_power_tlv_iter, &s);
	if (ret) {
		ath12k_warn(ab, "failed to parse nfcal power event tlv: %d\n", ret);
		return;
	}

	if (!s.ev) {
		ath12k_warn(ab, "failed to fetch nfcal power all channels event\n");
		return;
	}

	param.pdev_id = le32_to_cpu(s.ev->pdev_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "nfcal power event pdev_id %u num_nfdbr_dbm %u num_freq %u\n",
		   param.pdev_id, param.num_nfdbr_dbm, param.num_freq);

	rcu_read_lock();

	ar = ath12k_mac_get_ar_by_pdev_id(ab, param.pdev_id);
	if (!ar) {
		ath12k_warn(ab, "nfcal power event: invalid pdev_id %u\n", param.pdev_id);
		rcu_read_unlock();
		return;
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_vendor_nfcal_power_event(ar, &param);
#endif /* CPTCFG_QCN_EXTN */

	rcu_read_unlock();
}

static int ath12k_wmi_anomaly_tlv_parse(struct ath12k_base *ab, u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct ath12k_wmi_anomaly_parse *parse = data;

	switch (tag) {
	case WMI_TAG_ANOMALY_REPORT_HDR:
		/* TLV#0, fixed 8 bytes */
		if (!ptr || !len || len < sizeof(*parse->hdr) - sizeof(__le32))
			return -EPROTO;
		parse->hdr = ptr;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		/* TLV#1, context array: 0 (heartbeat) or 1 entry */
		if (parse->ctx_done)
			return -EPROTO;
		parse->ctx_ptr = ptr;
		parse->ctx_len = len;
		parse->ctx_done = true;
		break;

	default:
		return -EPROTO;
	}

	return 0;
}

static void ath12k_fw_anomaly_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_anomaly_parse parse = {};
	const struct wmi_anomaly_entry_t *entries;
	u32 num_entries, anomaly_id;
	u16 entry_tag;
	u32 i;
	int ret;
	u32 pdev_id;
	struct ath12k *ar = NULL;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "Anomaly event received\n");
	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_anomaly_tlv_parse, &parse);
	if (ret || !parse.hdr || !parse.ctx_done) {
		ath12k_warn(ab, "invalid anomaly report tlv format ret=%d\n",
			    ret);
		return;
	}

	pdev_id = le32_to_cpu(*(const __le32 *)parse.hdr);
	ar = ath12k_mac_get_ar_by_pdev_id(ab, pdev_id);

	if (!ar) {
		ath12k_warn(ab, "Invalid pdev_id=%u\n", pdev_id);
		return;
	}

	/* TLV#0: print anomaly report header */
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "Anomaly TLV#0 hdr: pdev_id=%u, radio_idx=%u\n",
		   le32_to_cpu(*(const __le32 *)parse.hdr), ar->radio_idx);
	ath12k_dbg_dump(ab, ATH12K_DBG_WMI, "Anomaly TLV#0 hdr raw", "",
			parse.hdr, sizeof(__le32));

	/* Heartbeat: 0 entries */
	if (parse.ctx_len == 0) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "Anomaly event Heartbeat\n");
		return;
	}

	if (parse.ctx_len % sizeof(*entries) != 0) {
		ath12k_warn(ab, "Invalid anomaly ctx_len %u\n", parse.ctx_len);
		return;
	}

	entries = (const struct wmi_anomaly_entry_t *)parse.ctx_ptr;
	num_entries = parse.ctx_len / sizeof(*entries);

	for (i = 0; i < num_entries; i++) {
		entry_tag = le32_get_bits(entries[i].tlv_header, WMI_TLV_TAG);
		if (entry_tag != WMI_TAG_ANOMALY_ENTRY) {
			ath12k_warn(ab, "Invalid anomaly entry tag %u at index %u\n",
				    entry_tag, i);
			return;
		}

		anomaly_id = le32_to_cpu(entries[i].anomaly_id);

		/* bit0 == 1 => Event type, ignore */
		if (anomaly_id & BIT(0)) {
			ath12k_dbg(ab, ATH12K_DBG_WMI, "Anomaly_id:Error type\n");
			continue;
		}

		/* TLV#1: print anomaly entry fields */
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Anomaly TLV#1 entry[%u]: anomaly_id=0x%x vdev_id=%u priority=%u related_cmd_id=%u\n",
			   i,
			   le32_to_cpu(entries[i].anomaly_id),
			   le32_to_cpu(entries[i].vdev_id),
			   le32_to_cpu(entries[i].priority),
			   le32_to_cpu(entries[i].related_cmd_id));
		ath12k_dbg_dump(ab, ATH12K_DBG_WMI, "Anomaly TLV#1 entry raw", "",
				&entries[i], sizeof(entries[i]) - sizeof(__le32));
#ifdef CPTCFG_ATHDEBUG
		struct athdbg_wmi_event_info info = {
			.radio_idx  = ar->radio_idx,
			.hw_link_id = ar->hw_link_id,
			.tlv_data   = skb->data,
			.tlv_len    = skb->len,
		};
		/* bit0 == 0 => forward full original TLV payload */
		athdbg_if_send_tlv(ab, WMI_ANOMALY_REPORT_EVENTID, &info);
#endif
	}
}

static void ath12k_wmi_mapc_peer_setup_status_event(struct ath12k_base *ab,
						    struct sk_buff *skb)
{
	const struct wmi_mapc_peer_setup_status_event_fixed_param *ev;
	u8 mac[ETH_ALEN];
	u32 recvd, missing;

	if (skb->len < sizeof(*ev)) {
		ath12k_warn(ab,
			    "MAPC SETUP STATUS event too short: %d < %zu bytes\n",
			    skb->len, sizeof(*ev));
		return;
	}

	ev = (const struct wmi_mapc_peer_setup_status_event_fixed_param *)skb->data;

	ether_addr_copy(mac, ev->peer_macaddr.addr);
	recvd   = le32_to_cpu(ev->param_sets_recvd);
	missing = le32_to_cpu(ev->param_sets_missing);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "MAPC SETUP STATUS: vdev_id=%u peer=%pM scheme_bitmap=0x%x sets_recvd=0x%x (CMN:%c Q2Q:%c PROF:%c TXOP:%c) sets_missing=0x%x\n",
		   le32_to_cpu(ev->vdev_id), mac,
		   le32_to_cpu(ev->scheme_enable_bitmap),
		   recvd,
		   (recvd & ATH12K_WMI_MAPC_SET_CMN)     ? 'Y' : 'N',
		   (recvd & ATH12K_WMI_MAPC_SET_Q2Q)     ? 'Y' : 'N',
		   (recvd & ATH12K_WMI_MAPC_SET_PROFILE) ? 'Y' : 'N',
		   (recvd & ATH12K_WMI_MAPC_SET_TXOP)    ? 'Y' : 'N',
		   missing);

	if (missing)
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "MAPC SETUP STATUS: peer %pM still waiting for sets: %s%s%s%s\n",
			   mac,
			   (missing & ATH12K_WMI_MAPC_SET_CMN)     ? "CMN " : "",
			   (missing & ATH12K_WMI_MAPC_SET_Q2Q)     ? "Q2Q " : "",
			   (missing & ATH12K_WMI_MAPC_SET_PROFILE) ? "PROFILE " : "",
			   (missing & ATH12K_WMI_MAPC_SET_TXOP)    ? "TXOP " : "");
	else
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "MAPC SETUP STATUS: peer %pM fully configured — Co-TDMA active\n",
			   mac);
}


static void ath12k_wmi_op_rx(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_event_id id;
	struct ath12k_wmi_pdev *wmi = NULL;
	u32 i;
	u8 eid, wmi_ep_count;

	eid = skb_cb->u.eid;
	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = le32_get_bits(cmd_hdr->cmd_id, WMI_CMD_HDR_CMD_ID);

	wmi_ep_count = ab->htc.wmi_ep_count;

	if (!skb_pull(skb, sizeof(struct wmi_cmd_hdr)))
		goto out;

	if (wmi_ep_count <= ab->hw_params->max_radios) {
		for (i = 0; i < ab->htc.wmi_ep_count; i++) {
			if (ab->wmi_ab.wmi[i].eid == eid) {
				wmi = &ab->wmi_ab.wmi[i];
				break;
			}
		}
	}

	if (wmi) {
		if (id != WMI_DIAG_EVENTID && id != WMI_MGMT_RX_EVENTID)
			WMI_EVENT_RX_RECORD(wmi, skb, id);
	}

	switch (id) {
		/* Process all the WMI events here */
	case WMI_SERVICE_READY_EVENTID:
		ath12k_service_ready_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT_EVENTID:
		ath12k_service_ready_ext_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT2_EVENTID:
		ath12k_service_ready_ext2_event(ab, skb);
		break;
	case WMI_REG_CHAN_LIST_CC_EXT_EVENTID:
		ath12k_reg_chan_list_event(ab, skb);
		break;
	case WMI_READY_EVENTID:
		ath12k_ready_event(ab, skb);
		break;
	case WMI_PEER_DELETE_RESP_EVENTID:
		ath12k_peer_delete_resp_event(ab, skb);
		break;
	case WMI_VDEV_START_RESP_EVENTID:
		ath12k_vdev_start_resp_event(ab, skb);
		break;
	case WMI_OFFLOAD_BCN_TX_STATUS_EVENTID:
		ath12k_bcn_tx_status_event(ab, skb);
		break;
	case WMI_VDEV_STOPPED_EVENTID:
		ath12k_vdev_stopped_event(ab, skb);
		break;
	case WMI_VDEV_TPC_IE_POWER_EVENTID:
		ath12k_vdev_tpc_ie_power_event(ab, skb);
		break;
	case WMI_MGMT_RX_EVENTID:
		ath12k_mgmt_rx_event(ab, skb);
		/* mgmt_rx_event() owns the skb now! */
		return;
	case WMI_MGMT_TX_COMPLETION_EVENTID:
		ath12k_mgmt_tx_compl_event(ab, skb);
		break;
	case WMI_SCAN_EVENTID:
		ath12k_scan_event(ab, skb);
		break;
	case WMI_PEER_STA_KICKOUT_EVENTID:
		ath12k_peer_sta_kickout_event(ab, skb);
		break;
	case WMI_ROAM_EVENTID:
		ath12k_roam_event(ab, skb);
		break;
	case WMI_CHAN_INFO_EVENTID:
		ath12k_chan_info_event(ab, skb);
		break;
	case WMI_PDEV_BSS_CHAN_INFO_EVENTID:
		ath12k_pdev_bss_chan_info_event(ab, skb);
		break;
	case WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID:
		ath12k_vdev_install_key_compl_event(ab, skb);
		break;
	case WMI_SERVICE_AVAILABLE_EVENTID:
		ath12k_service_available_event(ab, skb);
		break;
	case WMI_PEER_ASSOC_CONF_EVENTID:
		ath12k_peer_assoc_conf_event(ab, skb);
		break;
	case WMI_PEER_STA_PS_STATECHG_EVENTID:
		ath12k_wmi_event_peer_sta_ps_state_chg(ab, skb);
		break;
	case WMI_PEER_TX_PN_RESPONSE_EVENTID:
		ath12k_peer_tx_pn_event(ab, skb);
		break;
	case WMI_UPDATE_STATS_EVENTID:
		ath12k_update_stats_event(ab, skb);
		break;
	case WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID:
		ath12k_pdev_ctl_failsafe_check_event(ab, skb);
		break;
	case WMI_PDEV_CHECK_CAL_VERSION_EVENTID:
		ath12k_pdev_check_cal_version_event(ab, skb);
		break;
	case WMI_PDEV_SET_TGTR2P_TABLE_EVENTID:
		ath12k_wmi_pdev_tgtr2p_table_event(ab, skb);
		break;
	case WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID:
		ath12k_wmi_pdev_csa_switch_count_status_event(ab, skb);
		break;
	case WMI_PDEV_TEMPERATURE_EVENTID:
		ath12k_wmi_pdev_temperature_event(ab, skb);
		break;
	case WMI_THERM_THROT_STATS_EVENTID:
		ath12k_wmi_thermal_throt_stats_event(ab, skb);
		break;
	case WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID:
		ath12k_wmi_pdev_dma_ring_buf_release_event(ab, skb);
		break;
	case WMI_HOST_FILS_DISCOVERY_EVENTID:
		ath12k_fils_discovery_event(ab, skb);
		break;
	case WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID:
		ath12k_probe_resp_tx_status_event(ab, skb);
		break;
	case WMI_RFKILL_STATE_CHANGE_EVENTID:
		ath12k_rfkill_state_change_event(ab, skb);
		break;
	case WMI_TWT_ENABLE_EVENTID:
		ath12k_wmi_twt_enable_event(ab, skb);
		break;
	case WMI_TWT_DISABLE_EVENTID:
		ath12k_wmi_twt_disable_event(ab, skb);
		break;
	case WMI_P2P_NOA_EVENTID:
		ath12k_wmi_p2p_noa_event(ab, skb);
		break;
	case WMI_PDEV_DFS_RADAR_DETECTION_EVENTID:
		ath12k_wmi_pdev_dfs_radar_detected_event(ab, skb);
		break;
	case WMI_VDEV_DELETE_RESP_EVENTID:
		ath12k_vdev_delete_resp_event(ab, skb);
		break;
	case WMI_DIAG_EVENTID:
		ath12k_wmi_diag_event(ab, skb);
		break;
	case WMI_UPDATE_WHAL_MIB_STATS_EVENTID:
		ath12k_wmi_event_mib_stats(ab, skb);
		break;
	case WMI_WOW_WAKEUP_HOST_EVENTID:
		ath12k_wmi_event_wow_wakeup_host(ab, skb);
		break;
	case WMI_GTK_OFFLOAD_STATUS_EVENTID:
		ath12k_wmi_gtk_offload_status_event(ab, skb);
		break;
	case WMI_MLO_SETUP_COMPLETE_EVENTID:
		ath12k_wmi_event_mlo_setup_complete(ab, skb);
		break;
	case WMI_MLO_TEARDOWN_COMPLETE_EVENTID:
		ath12k_wmi_event_teardown_complete(ab, skb);
		break;
	case WMI_PDEV_UHR_CU_EVENTID:
		ath12k_wmi_pdev_uhr_cu_event(ab, skb);
		break;
	case WMI_PDEV_SET_CUMAC_CHIP_ID_CONFIRMATION_EVENTID:
		ath12k_wmi_event_send_cumac_complete(ab, skb);
		break;
	case WMI_SMD_ROAM_CONFIG_EVENTID:
		ath12k_wmi_smd_roam_config_event(ab, skb);
		break;
	case WMI_HALPHY_STATS_CTRL_PATH_EVENTID:
		ath12k_wmi_process_tpc_stats(ab, skb);
		break;
	case WMI_OFFCHAN_DATA_TX_COMPLETION_EVENTID:
		ath12k_offchan_tx_completion_event(ab, skb);
		break;
	case WMI_CTRL_PATH_STATS_EVENTID:
		ath12k_wmi_ctrl_path_stats_event(ab, skb);
		break;
	case WMI_11D_NEW_COUNTRY_EVENTID:
		ath12k_reg_11d_new_cc_event(ab, skb);
		break;
	case WMI_PEER_CREATE_CONF_EVENTID:
		ath12k_wmi_peer_create_conf_event(ab, skb);
		break;
	case WMI_TBTTOFFSET_EXT_UPDATE_EVENTID:
		ath12k_wmi_event_tbttoffset_update(ab, skb);
		break;
	case WMI_PDEV_SET_RF_PATH_RESP_EVENTID:
		ath12k_wmi_pdev_rf_path_resp_event(ab, skb);
		break;
	/* add Unsupported events (rare) here */
	case WMI_PEER_OPER_MODE_CHANGE_EVENTID:
	case WMI_PDEV_DMA_RING_CFG_RSP_EVENTID:
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ignoring unsupported event 0x%x\n", id);
		break;
	/* add Unsupported events (frequent) here */
	case WMI_PDEV_GET_HALPHY_CAL_STATUS_EVENTID:
	case WMI_MGMT_RX_FW_CONSUMED_EVENTID:
	case WMI_TWT_DEL_DIALOG_EVENTID:
	case WMI_TWT_PAUSE_DIALOG_EVENTID:
	case WMI_TWT_RESUME_DIALOG_EVENTID:
		/* debug might flood hence silently ignore (no-op) */
		break;
	case WMI_PDEV_SSCAN_FW_PARAM_EVENTID:
		ath12k_wmi_pdev_sscan_fw_param_event(ab, skb);
		break;
	case WMI_SPECTRAL_CAPABILITIES_EVENTID:
		ath12k_wmi_spectral_capabilities_event(ab, skb);
		break;
	case WMI_PDEV_RESUME_EVENTID:
		ath12k_wmi_pdev_resume_event(ab, skb);
		break;
	case WMI_PDEV_SUSPEND_EVENTID:
		ath12k_wmi_suspend_event(ab, skb);
		break;
	case WMI_PDEV_UTF_EVENTID:
		if (test_bit(ATH12K_FLAG_FTM_SEGMENTED, &ab->dev_flags))
			ath12k_tm_wmi_event_segmented(ab, id, skb);
		else
			ath12k_tm_wmi_event_unsegmented(ab, id, skb);
		break;
	case WMI_DCS_INTERFERENCE_EVENTID:
		ath12k_wmi_dcs_interference_event(ab, skb);
                break;
	case WMI_MUEDCA_PARAMS_CONFIG_EVENTID:
		ath12k_wmi_pdev_update_muedca_params_status_event(ab, skb);
		break;
	case WMI_PDEV_MULTIPLE_VDEV_RESTART_RESP_EVENTID:
		ath12k_wmi_event_mvr_response(ab, skb);
		break;
	case WMI_TWT_ADD_DIALOG_EVENTID:
		ath12k_wmi_twt_add_dialog_event(ab, skb);
		break;
	case WMI_OBSS_COLOR_COLLISION_DETECTION_EVENTID:
		ath12k_wmi_obss_color_collision_event(ab, skb);
		break;
	case WMI_PEER_CFR_CAPTURE_EVENTID:
		ath12k_wmi_parse_cfr_capture_event(ab, skb);
		break;
	case WMI_PDEV_ENHANCED_AOA_PHASEDELTA_EVENTID:
		ath12k_wmi_pdev_enh_aoa_phasedelta_event(ab, skb);
		break;
	case WMI_PDEV_RSSI_DBM_CONVERSION_PARAMS_INFO_EVENTID:
		ath12k_wmi_rssi_dbm_conversion_param_info(ab, skb);
		break;
	case WMI_MLO_LINK_REMOVAL_EVENTID:
		ath12k_wmi_event_mlo_reconfig_link_removal(ab, skb);
		break;
	case WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_EVENT_ID:
		ath12k_wmi_peer_migration_event(ab, skb);
		break;
	case WMI_AFC_EVENTID:
		ath12k_wmi_afc_event(ab, skb);
		break;
	case WMI_VDEV_ADFS_OCAC_COMPLETE_EVENTID:
		ath12k_process_ocac_complete_event(ab, skb);
		break;
	case WMI_MLO_TID_TO_LINK_MAP_EVENT_ID:
		ath12k_wmi_tid_to_link_map_event(ab, skb);
		break;
	case WMI_PDEV_PKTLOG_DECODE_INFO_EVENTID:
		ath12k_wmi_pktlog_decode_info(ab, skb);
		break;
	case WMI_TWT_BTWT_INVITE_STA_COMPLETE_EVENTID:
		ath12k_wmi_twt_btwt_invite_sta_compl_event(ab, skb);
		break;
	case WMI_TWT_BTWT_REMOVE_STA_COMPLETE_EVENTID:
		ath12k_wmi_twt_btwt_remove_sta_compl_event(ab, skb);
		break;
	case WMI_MLO_TLT_SELECTION_FOR_TID_SPRAY_EVENTID:
		ath12k_wmi_mlo_3_link_tlt_selection(ab, skb);
		break;
	case WMI_VDEV_DELETE_ALL_PEER_RESP_EVENTID:
		ath12k_wmi_delete_all_peer_resp_event(ab, skb);
		break;
	case WMI_VDEV_TSF_REPORT_EVENTID:
		ath12k_vdev_tsf_report_event(ab, skb);
		break;
	case WMI_RTT_PASN_PEER_CREATE_REQ_EVENTID:
		ath12k_wmi_rtt_pasn_peer_create_req_event(ab, skb);
		break;
	case WMI_RTT_PASN_PEER_DELETE_EVENTID:
		ath12k_wmi_rtt_pasn_peer_delete_event(ab, skb);
		break;
	case WMI_GPIO_INPUT_EVENTID:
		ath12k_wmi_gpio_input_event(ab, skb);
		break;
	case WMI_ENERGY_MGMT_OEM_DATA_EVENTID:
		ath12k_wmi_energy_mgmt_oem_data_event(ab, skb);
		break;
	case WMI_PDEV_NFCAL_POWER_ALL_CHANNELS_EVENTID:
		ath12k_wmi_pdev_nfcal_power_all_channels_event(ab, skb);
		break;
	case WMI_ANOMALY_REPORT_EVENTID:
		ath12k_fw_anomaly_event(ab, skb);
		break;
	case WMI_PEER_MAPC_SETUP_STATUS_EVENTID:
		ath12k_wmi_mapc_peer_setup_status_event(ab, skb);
		break;
	default:
#ifdef CPTCFG_QCN_EXTN
		if (!ath12k_wmi_op_rx_extn(id, ab, skb))
			break;
#endif /* CPTCFG_QCN_EXTN */

		ath12k_dbg_level(ab, ATH12K_DBG_WMI, ATH12K_DBG_L1,
				 "Unknown eventid: 0x%x\n", id);
		break;
	}

out:
	dev_kfree_skb(skb);
}

static void ath12k_wmi_ps_state_chg_iter(struct ath12k *ar,
					 struct ath12k_link_sta *arsta,
					 void *data)
{
	const struct wmi_peer_sta_ps_state_chg_event *ev = data;
	enum ath12k_wmi_peer_ps_state prev_state;

	prev_state = arsta->peer_ps_state;
	arsta->peer_ps_state = le32_to_cpu(ev->peer_ps_state);
	arsta->peer_current_ps_valid = !!le32_to_cpu(ev->peer_ps_valid);

	if ((ev->ps_supported_bitmap & WMI_PEER_PS_VALID) &&
	    (ev->ps_supported_bitmap & WMI_PEER_PS_STATE_TIMESTAMP) &&
	    ev->peer_ps_valid) {
		if (arsta->peer_ps_state == WMI_PEER_PS_STATE_ON) {
			arsta->ps_start_time = le32_to_cpu(ev->peer_ps_timestamp);
			arsta->ps_start_jiffies = jiffies;
		} else if (arsta->peer_ps_state == WMI_PEER_PS_STATE_OFF &&
			   prev_state == WMI_PEER_PS_STATE_ON) {
			arsta->ps_total_duration +=
				(le32_to_cpu(ev->peer_ps_timestamp) -
				 arsta->ps_start_time);
		}
	}
}

static void ath12k_wmi_event_peer_sta_ps_state_chg(struct ath12k_base *ab,
						   struct sk_buff *skb)
{
	const struct wmi_peer_sta_ps_state_chg_event *ev;
	const void **tb;
	u8 mac[ETH_ALEN];

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ath12k_warn(ab, "failed to parse tlv: %ld\n", PTR_ERR(tb));
		return;
	}

	ev = tb[WMI_TAG_PEER_STA_PS_STATECHANGE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch sta ps change ev");
		kfree(tb);
		return;
	}

	ether_addr_copy(mac, ev->peer_macaddr.addr);
	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "event peer sta ps change ev addr %pM state %u sup_bitmap %x ps_valid %u ts %u\n",
		   mac, le32_to_cpu(ev->peer_ps_state),
		   le32_to_cpu(ev->ps_supported_bitmap),
		   le32_to_cpu(ev->peer_ps_valid),
		   le32_to_cpu(ev->peer_ps_timestamp));

	if (!ath12k_arsta_itr_on_ab_by_addr(ab, mac,
					    ath12k_wmi_ps_state_chg_iter, (void *)ev))
		ath12k_warn(ab, "failed to get valid link sta/arvif/ar for %pM\n",
			    mac);
	kfree(tb);
}

static int ath12k_connect_pdev_htc_service(struct ath12k_base *ab,
					   u32 pdev_idx)
{
	int status;
	static const u32 svc_id[] = {
		ATH12K_HTC_SVC_ID_WMI_CONTROL,
		ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC1,
		ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC2
	};
	struct ath12k_htc_svc_conn_req conn_req = {};
	struct ath12k_htc_svc_conn_resp conn_resp = {};

	/* these fields are the same for all service endpoints */
	conn_req.ep_ops.ep_tx_complete = ath12k_wmi_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_wmi_op_rx;
	conn_req.ep_ops.ep_tx_credits = ath12k_wmi_op_ep_tx_credits;

	/* connect to control service */
	conn_req.service_id = svc_id[pdev_idx];

	status = ath12k_htc_connect_service(&ab->htc, &conn_req, &conn_resp);
	if (status) {
		ath12k_warn(ab, "failed to connect to WMI CONTROL service status: %d\n",
			    status);
		return status;
	}

	ab->wmi_ab.wmi_endpoint_id[pdev_idx] = conn_resp.eid;
	ab->wmi_ab.wmi[pdev_idx].eid = conn_resp.eid;
	ab->wmi_ab.max_msg_len[pdev_idx] = conn_resp.max_msg_len;
	init_waitqueue_head(&ab->wmi_ab.wmi[pdev_idx].tx_ce_desc_wq);

	return 0;
}

#ifndef CPTCFG_QCN_EXTN
static
#endif
int
ath12k_wmi_send_unit_test_cmd(struct ath12k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_unit_test_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	u32 *ut_cmd_args;
	int buf_len, arg_len;
	int ret;
	int i;

	arg_len = sizeof(u32) * le32_to_cpu(ut_cmd.num_args);
	buf_len = sizeof(ut_cmd) + arg_len + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_unit_test_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UNIT_TEST_CMD,
						 sizeof(ut_cmd));

	cmd->vdev_id = ut_cmd.vdev_id;
	cmd->module_id = ut_cmd.module_id;
	cmd->num_args = ut_cmd.num_args;
	cmd->diag_token = ut_cmd.diag_token;

	ptr = skb->data + sizeof(ut_cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, arg_len);

	ptr += TLV_HDR_SIZE;

	ut_cmd_args = ptr;
	for (i = 0; i < le32_to_cpu(ut_cmd.num_args); i++)
		ut_cmd_args[i] = test_args[i];

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI unit test : module %d vdev %d n_args %d token %d\n",
		   cmd->module_id, cmd->vdev_id, cmd->num_args,
		   cmd->diag_token);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_UNIT_TEST_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_UNIT_TEST CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_afc_grace_timer(struct ath12k *ar, u32 afc_grace_timer_value)
{
	struct ath12k_link_vif *arvif;
	u32 afc_args[AFC_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found) {
		ath12k_warn(ar->ab, "No valid vdev found to set AFC grace timer on ar:%d\n",
			    ar->pdev_idx);
		return -EINVAL;
	}

	afc_args[AFC_GRACE_TIMER_SUBCMDID] = AFC_UNIT_TEST_GRACE_TIMER_SUBCMDID;
	afc_args[AFC_GRACE_TIMER_PDEV_ID] = ar->pdev->pdev_id;
	afc_args[AFC_GRACE_TIMER_VALUE] = afc_grace_timer_value;

	wmi_ut.vdev_id = cpu_to_le32(arvif->vdev_id);
	wmi_ut.module_id = cpu_to_le32(AFC_UNIT_TEST_MODULE_ID);
	wmi_ut.num_args = cpu_to_le32(AFC_MAX_TEST_ARGS);
	wmi_ut.diag_token = cpu_to_le32(AFC_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Sending AFC grace timer value to FW\n");

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, afc_args);
}

int ath12k_wmi_simulate_radar(struct ath12k *ar, u32 radar_params)
{
	struct ath12k_link_vif *arvif;
	u32 dfs_args[DFS_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;
	bool sta_dfs_en = ath12k_ar_to_hw(ar)->wiphy->sta_dfs_en;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (!arvif->is_started)
			continue;

		if ((arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP) ||
		    ((arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA) &&
		     sta_dfs_en)) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	dfs_args[DFS_TEST_CMDID] = 0;
	dfs_args[DFS_TEST_PDEV_ID] = ar->pdev->pdev_id;
	/* Currently we pass segment_id(b0 - b1), chirp(b2)
	 * freq offset (b3 - b10), detector_id(b11 - b12) to unit test.
	 */
	dfs_args[DFS_TEST_RADAR_PARAM] = radar_params;

	wmi_ut.vdev_id = cpu_to_le32(arvif->vdev_id);
	wmi_ut.module_id = cpu_to_le32(DFS_UNIT_TEST_MODULE);
	wmi_ut.num_args = cpu_to_le32(DFS_MAX_TEST_ARGS);
	wmi_ut.diag_token = cpu_to_le32(DFS_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Triggering Radar Simulation\n");

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, dfs_args);
}

int ath12k_wmi_dbglog_cfg(struct ath12k *ar, u32 param, u64 value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_dbglog_config_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	u32 module_id_bitmap;
	int ret, len;

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(module_id_bitmap);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;
	cmd = (struct wmi_dbglog_config_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DEBUG_LOG_CONFIG_CMD,
						 sizeof(*cmd));
	cmd->dbg_log_param = param;

	tlv = (struct wmi_tlv *)((u8 *)cmd + sizeof(*cmd));
	tlv->header = FIELD_PREP(WMI_TLV_TAG, WMI_TAG_ARRAY_UINT32) |
		      FIELD_PREP(WMI_TLV_LEN, sizeof(u32));

	switch (param) {
	case WMI_DEBUG_LOG_PARAM_LOG_LEVEL:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_DISABLE:
	case WMI_DEBUG_LOG_PARAM_VDEV_ENABLE_BITMAP:
		cmd->value = value;
		break;
	case WMI_DEBUG_LOG_PARAM_MOD_ENABLE_BITMAP:
	case WMI_DEBUG_LOG_PARAM_WOW_MOD_ENABLE_BITMAP:
		cmd->value = value;
		module_id_bitmap = value >> 32;
		memcpy(tlv->value, &module_id_bitmap, sizeof(module_id_bitmap));
		break;
	default:
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_DBGLOG_CFG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_DBGLOG_CFG_CMDID\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

#ifdef CPTCFG_ATH12K_DEBUGFS

int ath12k_wmi_send_twt_btwt_invite_sta_cmd(struct ath12k *ar,
					    struct wmi_twt_btwt_invite_sta_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_btwt_invite_sta_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_btwt_invite_sta_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_BTWT_INVITE_STA_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi BTWT invite sta vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_BTWT_INVITE_STA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to B-TWT invite sta: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_twt_btwt_remove_sta_cmd(struct ath12k *ar,
					    struct wmi_twt_btwt_remove_sta_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_btwt_remove_sta_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_btwt_remove_sta_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_BTWT_REMOVE_STA_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = params->vdev_id;
	ether_addr_copy(cmd->peer_macaddr.addr, params->peer_macaddr);
	cmd->dialog_id = params->dialog_id;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;
	cmd->r_twt_dl_tid_bitmap = params->r_twt_dl_tid_bitmap;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi BTWT remove sta vdev %u dialog id %u\n",
		   cmd->vdev_id, cmd->dialog_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_TWT_BTWT_REMOVE_STA_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "failed to send wmi command to B-TWT remove sta: %d",
			    ret);
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_wmi_ctrl_stats_cmd(struct ath12k *ar,
				   struct wmi_ctrl_path_stats_arg *arg)
{
	struct wmi_request_ctrl_path_stats_cmd_fixed_param fixed_cmd;
	struct wmi_ctrl_path_stats_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct ath12k_debug *debug = &ar->debug;
	__le32 pdev_id;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int len, ret;
	void *ptr;
	u32 stats_id;

	switch (arg->stats_id) {
	case WMI_REQ_CTRL_PATH_PDEV_TX_STAT:
	case WMI_REQ_CTRL_PATH_CAL_STAT:
	case WMI_REQ_CTRL_PATH_BTCOEX_STAT:
		stats_id = (1 << arg->stats_id);
		break;
	case WMI_REQ_CTRL_PATH_AWGN_STAT:
	case WMI_REQ_CTRL_PATH_AFC_STAT:
		if (ar->supports_6ghz) {
			stats_id = (1 << arg->stats_id);
		} else {
			ath12k_warn(ab,
			  "Stats id %d %s stats are only supported for 6GHz",
			  arg->stats_id,
			  (arg->stats_id ==
			   WMI_REQ_CTRL_PATH_AWGN_STAT) ? "AWGN" : "AFC");
			return -EIO;
		}
		break;
	case WMI_REQ_CTRL_PATH_MEM_STAT:
		ar->ctrl_mem_stats = true;
		stats_id = (1 << arg->stats_id);
		break;
	case WMI_REQ_CTRL_PATH_BLANKING_STAT:
		stats_id = (1 << arg->stats_id);
		break;
		/* Add case for newly wmi ctrl path stats here */
	case WMI_REQ_CTRL_PATH_PMLO_STAT:
		fixed_cmd.request_id = arg->req_id;
		fixed_cmd.action = arg->action;
		fixed_cmd.subid = 0;
		return ath12k_wmi_pdev_enable_telemetry_stats(ab, ar,
							      &fixed_cmd);
	default:
		ath12k_warn(ab, "Unsupported stats id %d", arg->stats_id);
		return -EIO;
		break;
	}

	pdev_id = cpu_to_le32(ath12k_mac_get_target_pdev_id(ar));

	len = sizeof(*cmd) +
		TLV_HDR_SIZE + sizeof(u32) +
		TLV_HDR_SIZE + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CTRL_PATH_STATS_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->stats_id = cpu_to_le32(stats_id);
	cmd->req_id = cpu_to_le32(arg->req_id);
	cmd->action = cpu_to_le32(arg->action);

	ptr = skb->data + sizeof(*cmd);

	/* The below TLV arrays optionally follow this fixed param TLV structure
	 * 1. ARRAY_UINT32 pdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the pdevs identified in the array.
	 * 2. ARRAY_UNIT32 vdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the vdevs identified in the array.
	 * 3. ath12k_wmi_mac_addr_params peer_macaddr[];
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the peers with the MAC addresses specified
	 *      in the array
	 */

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));
	ptr += TLV_HDR_SIZE;
	memcpy(ptr, &pdev_id, sizeof(u32));
	ptr += sizeof(u32);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += TLV_HDR_SIZE;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	if (arg->action == WMI_REQUEST_CTRL_PATH_STAT_GET)
		reinit_completion(&ar->debug.wmi_ctrl_path_stats_rcvd);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_REQUEST_CTRL_PATH_STATS_CMDID);
	if (ret) {
		dev_kfree_skb(skb);
		ath12k_warn(ab, "Failed to send WMI_REQUEST_CTRL_PATH_STATS_CMDID: %d",
			    ret);
	} else {
		if (arg->action == WMI_REQUEST_CTRL_PATH_STAT_GET) {
			if (!wait_for_completion_timeout(&debug->wmi_ctrl_path_stats_rcvd,
							 WMI_CTRL_STATS_READY_TIMEOUT)) {
				ath12k_warn(ab, "wmi ctrl path stats timed out\n");
				ret = -ETIMEDOUT;
			}
		}
	}
	return ret;
}
#endif

int ath12k_wmi_send_tpc_stats_request(struct ath12k *ar,
				      enum wmi_halphy_ctrl_path_stats_id tpc_stats_type)
{
	struct wmi_request_halphy_ctrl_path_stats_cmd_fixed_params *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	__le32 *pdev_id;
	u32 buf_len;
	void *ptr;
	int ret;

	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(u32) + TLV_HDR_SIZE + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;
	cmd = (struct wmi_request_halphy_ctrl_path_stats_cmd_fixed_params *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HALPHY_CTRL_PATH_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->stats_id_mask = cpu_to_le32(WMI_REQ_CTRL_PATH_PDEV_TX_STAT);
	cmd->action = cpu_to_le32(WMI_REQUEST_CTRL_PATH_STAT_GET);
	cmd->subid = cpu_to_le32(tpc_stats_type);

	ptr = skb->data + sizeof(*cmd);

	/* The below TLV arrays optionally follow this fixed param TLV structure
	 * 1. ARRAY_UINT32 pdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the pdevs identified in the array.
	 * 2. ARRAY_UNIT32 vdev_ids[]
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the vdevs identified in the array.
	 * 3. ath12k_wmi_mac_addr_params peer_macaddr[];
	 *      If this array is present and non-zero length, stats should only
	 *      be provided from the peers with the MAC addresses specified
	 *      in the array
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));
	ptr += TLV_HDR_SIZE;

	pdev_id = ptr;
	*pdev_id = cpu_to_le32(ath12k_mac_get_target_pdev_id(ar));
	ptr += sizeof(*pdev_id);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += TLV_HDR_SIZE;

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_REQUEST_HALPHY_CTRL_PATH_STATS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_REQUEST_STATS_CTRL_PATH_CMDID\n");
		dev_kfree_skb(skb);
		return ret;
	}
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI get TPC STATS sent on pdev %d\n",
		   ar->pdev->pdev_id);

	return ret;
}

int ath12k_wmi_pdev_m3_dump_enable(struct ath12k *ar, u32 enable)
{
        struct ath12k_link_vif *arvif;
        u32 m3_args[WMI_M3_MAX_TEST_ARGS];
        struct wmi_unit_test_cmd wmi_ut;
        bool arvif_found = false;

        list_for_each_entry(arvif, &ar->arvifs, list) {
                if (arvif->is_started) {
                        arvif_found = true;
                        break;
                }
        }

        if (!arvif_found)
                return -EINVAL;

        m3_args[WMI_M3_TEST_CMDID] = WMI_DBG_ENABLE_M3_SSR;
        m3_args[WMI_M3_TEST_ENABLE] = enable;

        wmi_ut.vdev_id = arvif->vdev_id;
        wmi_ut.module_id = WMI_M3_UNIT_TEST_MODULE;
        wmi_ut.num_args = WMI_M3_MAX_TEST_ARGS;
        wmi_ut.diag_token = WMI_M3_UNIT_TEST_TOKEN;

        ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "%s M3 SSR dump\n",
                   enable ? "Enabling" : "Disabling");

        return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, m3_args);
}

int ath12k_wmi_simulate_awgn(struct ath12k *ar, u32 chan_bw_interference_bitmap)
{
	u32 awgn_args[WMI_AWGN_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
        struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
        bool arvif_found = false;

        if (!test_bit(WMI_TLV_SERVICE_DCS_AWGN_INT_SUPPORT, ar->ab->wmi_ab.svc_map)) {
                ath12k_warn(ar->ab, "firmware doesn't support awgn interference, so can't simulate it\n");
                return -EOPNOTSUPP;
        }

        list_for_each_entry(arvif, &ar->arvifs, list) {
		ahvif = arvif->ahvif;
                if (arvif->is_started && ahvif->vdev_type == WMI_VDEV_TYPE_AP) {
                        arvif_found = true;
                        break;
                }
        }

        if (!arvif_found)
                return -EINVAL;

        awgn_args[WMI_AWGN_TEST_AWGN_INT] = WMI_UNIT_TEST_AWGN_INTF_TYPE;
        awgn_args[WMI_AWGN_TEST_BITMAP] = chan_bw_interference_bitmap;

        wmi_ut.vdev_id = arvif->vdev_id;
        wmi_ut.module_id = WMI_AWGN_UNIT_TEST_MODULE;
        wmi_ut.num_args = WMI_AWGN_MAX_TEST_ARGS;
        wmi_ut.diag_token = WMI_AWGN_UNIT_TEST_TOKEN;

        ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
                   "Triggering AWGN Simulation, interference bitmap : 0x%x\n",
                   chan_bw_interference_bitmap);

        return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, awgn_args);
}

int ath12k_wmi_connect(struct ath12k_base *ab)
{
	u32 i;
	u8 wmi_ep_count;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params->max_radios)
		return -1;

	for (i = 0; i < wmi_ep_count; i++)
		ath12k_connect_pdev_htc_service(ab, i);

	return 0;
}

static void ath12k_wmi_recording_cleanup(struct ath12k_wmi_pdev *wmi_handle)
{
	struct ath12k_base *ab = wmi_handle->wmi_ab->ab;
	struct wmi_cmd_debug *cmd_log;
	struct wmi_cmd_comp_debug *tx_cmp_log;
	struct wmi_event_debug *evt_log;

	cmd_log = rcu_access_pointer(wmi_handle->wmi_cmd_log);
	tx_cmp_log = rcu_access_pointer(wmi_handle->wmi_cmd_tx_cmp_log);
	evt_log = rcu_access_pointer(wmi_handle->wmi_evt_log);
	rcu_assign_pointer(wmi_handle->wmi_cmd_log, NULL);
	rcu_assign_pointer(wmi_handle->wmi_cmd_tx_cmp_log, NULL);
	rcu_assign_pointer(wmi_handle->wmi_evt_log, NULL);
	synchronize_rcu();

	if (!test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ab->ag->flags)) {
		memset(cmd_log, 0,
		       wmi_handle->wmi_cmd_log_size * sizeof(*cmd_log));
		memset(tx_cmp_log, 0,
		       wmi_handle->wmi_cmd_tx_cmp_log_size * sizeof(*tx_cmp_log));
		memset(evt_log, 0,
		       wmi_handle->wmi_evt_log_size * sizeof(*evt_log));
		rcu_assign_pointer(wmi_handle->wmi_cmd_log, cmd_log);
		rcu_assign_pointer(wmi_handle->wmi_cmd_tx_cmp_log, tx_cmp_log);
		rcu_assign_pointer(wmi_handle->wmi_evt_log, evt_log);
		wmi_handle->dbg_cmd_tail_idx = 0;
		wmi_handle->dbg_cmd_tx_cmp_tail_idx = 0;
		wmi_handle->dbg_evt_tail_idx = 0;
	} else {
		kfree(cmd_log);
		kfree(tx_cmp_log);
		kfree(evt_log);
		wmi_handle->wmi_recording_enabled = false;
	}
}

void ath12k_wmi_recording_init(struct ath12k_wmi_pdev *wmi_handle,
				      struct ath12k_base *ab)
{
	struct wmi_cmd_debug *wmi_cmd_log = NULL;
	struct wmi_cmd_comp_debug *wmi_cmd_tx_cmp_log = NULL;
	struct wmi_event_debug *wmi_evt_log = NULL;
	u32 wmi_common_log_entries = 1024;
	size_t cmd_bytes, txcmp_bytes, evt_bytes;

	cmd_bytes = wmi_common_log_entries * sizeof(*wmi_handle->wmi_cmd_log);
	txcmp_bytes = wmi_common_log_entries * sizeof(*wmi_handle->wmi_cmd_tx_cmp_log);
	evt_bytes = wmi_common_log_entries * sizeof(*wmi_handle->wmi_evt_log);

	wmi_cmd_log = kzalloc(cmd_bytes, GFP_KERNEL);
	if (!wmi_cmd_log)
		goto err;
	wmi_cmd_tx_cmp_log = kzalloc(txcmp_bytes, GFP_KERNEL);
	if (!wmi_cmd_tx_cmp_log)
		goto err;
	wmi_evt_log = kzalloc(evt_bytes, GFP_KERNEL);
	if (!wmi_evt_log)
		goto err;

	rcu_assign_pointer(wmi_handle->wmi_cmd_log, wmi_cmd_log);
	rcu_assign_pointer(wmi_handle->wmi_cmd_tx_cmp_log, wmi_cmd_tx_cmp_log);
	rcu_assign_pointer(wmi_handle->wmi_evt_log, wmi_evt_log);
	wmi_handle->wmi_cmd_log_size = wmi_common_log_entries;
	wmi_handle->wmi_cmd_tx_cmp_log_size = wmi_common_log_entries;
	wmi_handle->wmi_evt_log_size = wmi_common_log_entries;
	wmi_handle->dbg_cmd_tail_idx = 0;
	wmi_handle->dbg_cmd_tx_cmp_tail_idx = 0;
	wmi_handle->dbg_evt_tail_idx = 0;
	wmi_handle->verbosity = 1;
	wmi_handle->wmi_recording_enabled = true;
	return;
err:
	kfree(wmi_evt_log);
	kfree(wmi_cmd_tx_cmp_log);
	kfree(wmi_cmd_log);
	ath12k_err(ab, "WMI Recording Failure : Memory Allocation\n");
}

static void ath12k_wmi_pdev_detach(struct ath12k_base *ab, u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi_handle;

	if (WARN_ON(pdev_id >= MAX_RADIOS))
		return;

	wmi_handle = &ab->wmi_ab.wmi[pdev_id];

	if (wmi_handle->wmi_recording_enabled)
		ath12k_wmi_recording_cleanup(wmi_handle);

	/* TODO: Deinit any pdev specific wmi resource */
}

int ath12k_wmi_pdev_attach(struct ath12k_base *ab,
			   u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi_handle;

	if (pdev_id >= ab->hw_params->max_radios)
		return -EINVAL;

	wmi_handle = &ab->wmi_ab.wmi[pdev_id];
	wmi_handle->wmi_ab = &ab->wmi_ab;
	ab->wmi_ab.ab = ab;
	/* TODO: Init remaining resource specific to pdev */

	return 0;
}

int ath12k_wmi_attach(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wmi_pdev_attach(ab, 0);
	if (ret)
		return ret;

	ab->wmi_ab.ab = ab;
	ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_MAX;

	/* It's overwritten when service_ext_ready is handled */
	if (ab->hw_params->single_pdev_only)
		ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_SINGLE;

	/* TODO: Init remaining wmi soc resources required */
	init_completion(&ab->wmi_ab.service_ready);
	init_completion(&ab->wmi_ab.unified_ready);

	return 0;
}

void ath12k_wmi_detach(struct ath12k_base *ab)
{
	int i;

	if (!test_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags) &&
	    !test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
		return;

	/* TODO: Deinit wmi resource specific to SOC as required */

	for (i = 0; i < ab->htc.wmi_ep_count; i++)
		ath12k_wmi_pdev_detach(ab, i);

	clear_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags);
	ath12k_wmi_free_dbring_caps(ab);
}

int ath12k_wmi_hw_data_filter_cmd(struct ath12k *ar, struct wmi_hw_data_filter_arg *arg)
{
	struct wmi_hw_data_filter_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);

	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_hw_data_filter_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HW_DATA_FILTER_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->enable = cpu_to_le32(arg->enable ? 1 : 0);

	/* Set all modes in case of disable */
	if (arg->enable)
		cmd->hw_filter_bitmap = cpu_to_le32(arg->hw_filter_bitmap);
	else
		cmd->hw_filter_bitmap = cpu_to_le32((u32)~0U);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi hw data filter enable %d filter_bitmap 0x%x\n",
		   arg->enable, arg->hw_filter_bitmap);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_HW_DATA_FILTER_CMDID);
}

int ath12k_wmi_wow_host_wakeup_ind(struct ath12k *ar)
{
	struct wmi_wow_host_wakeup_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_host_wakeup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_HOSTWAKEUP_FROM_SLEEP_CMD,
						 sizeof(*cmd));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow host wakeup ind\n");

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);
}

int ath12k_wmi_wow_enable(struct ath12k *ar)
{
	struct wmi_wow_enable_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_enable_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ENABLE_CMD,
						 sizeof(*cmd));

	cmd->enable = cpu_to_le32(1);
	cmd->pause_iface_config = cpu_to_le32(WOW_IFACE_PAUSE_ENABLED);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow enable\n");

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_CMDID);
}

int ath12k_wmi_wow_add_wakeup_event(struct ath12k *ar, u32 vdev_id,
				    enum wmi_wow_wakeup_event event,
				    u32 enable)
{
	struct wmi_wow_add_del_event_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_add_del_event_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ADD_DEL_EVT_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->is_add = cpu_to_le32(enable);
	cmd->event_bitmap = cpu_to_le32((1 << event));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow add wakeup event %s enable %d vdev_id %d\n",
		   wow_wakeup_event(event), enable, vdev_id);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);
}

int ath12k_wmi_wow_add_pattern(struct ath12k *ar, u32 vdev_id, u32 pattern_id,
			       const u8 *pattern, const u8 *mask,
			       int pattern_len, int pattern_offset)
{
	struct wmi_wow_add_pattern_cmd *cmd;
	struct wmi_wow_bitmap_pattern_params *bitmap;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	size_t len;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +			/* array struct */
	      sizeof(*bitmap) +			/* bitmap */
	      sizeof(*tlv) +			/* empty ipv4 sync */
	      sizeof(*tlv) +			/* empty ipv6 sync */
	      sizeof(*tlv) +			/* empty magic */
	      sizeof(*tlv) +			/* empty info timeout */
	      sizeof(*tlv) + sizeof(u32);	/* ratelimit interval */

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	/* cmd */
	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_ADD_PATTERN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pattern_id = cpu_to_le32(pattern_id);
	cmd->pattern_type = cpu_to_le32(WOW_BITMAP_PATTERN);

	ptr += sizeof(*cmd);

	/* bitmap */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*bitmap));

	ptr += sizeof(*tlv);

	bitmap = ptr;
	bitmap->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_BITMAP_PATTERN_T,
						    sizeof(*bitmap));
	memcpy(bitmap->patternbuf, pattern, pattern_len);
	memcpy(bitmap->bitmaskbuf, mask, pattern_len);
	bitmap->pattern_offset = cpu_to_le32(pattern_offset);
	bitmap->pattern_len = cpu_to_le32(pattern_len);
	bitmap->bitmask_len = cpu_to_le32(pattern_len);
	bitmap->pattern_id = cpu_to_le32(pattern_id);

	ptr += sizeof(*bitmap);

	/* ipv4 sync */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* ipv6 sync */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* magic */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	ptr += sizeof(*tlv);

	/* pattern info timeout */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);

	ptr += sizeof(*tlv);

	/* ratelimit interval */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, sizeof(u32));

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow add pattern vdev_id %d pattern_id %d pattern_offset %d pattern_len %d\n",
		   vdev_id, pattern_id, pattern_offset, pattern_len);

	ath12k_dbg_dump(ar->ab, ATH12K_DBG_WMI, NULL, "wow pattern: ",
			bitmap->patternbuf, pattern_len);
	ath12k_dbg_dump(ar->ab, ATH12K_DBG_WMI, NULL, "wow bitmask: ",
			bitmap->bitmaskbuf, pattern_len);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_ADD_WAKE_PATTERN_CMDID);
}

int ath12k_wmi_wow_del_pattern(struct ath12k *ar, u32 vdev_id, u32 pattern_id)
{
	struct wmi_wow_del_pattern_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_wow_del_pattern_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_WOW_DEL_PATTERN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pattern_id = cpu_to_le32(pattern_id);
	cmd->pattern_type = cpu_to_le32(WOW_BITMAP_PATTERN);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv wow del pattern vdev_id %d pattern_id %d\n",
		   vdev_id, pattern_id);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_WOW_DEL_WAKE_PATTERN_CMDID);
}

static struct sk_buff *
ath12k_wmi_op_gen_config_pno_start(struct ath12k *ar, u32 vdev_id,
				   struct wmi_pno_scan_req_arg *pno)
{
	struct nlo_configured_params *nlo_list;
	size_t len, nlo_list_len, channel_list_len;
	struct wmi_wow_nlo_config_cmd *cmd;
	__le32 *channel_list;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	u32 i;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      /* TLV place holder for array of structures
	       * nlo_configured_params(nlo_list)
	       */
	      sizeof(*tlv);
	      /* TLV place holder for array of uint32 channel_list */

	channel_list_len = sizeof(u32) * pno->a_networks[0].channel_count;
	len += channel_list_len;

	nlo_list_len = sizeof(*nlo_list) * pno->uc_networks_count;
	len += nlo_list_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	ptr = skb->data;
	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NLO_CONFIG_CMD, sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(pno->vdev_id);
	cmd->flags = cpu_to_le32(WMI_NLO_CONFIG_START | WMI_NLO_CONFIG_SSID_HIDE_EN);

	/* current FW does not support min-max range for dwell time */
	cmd->active_dwell_time = cpu_to_le32(pno->active_max_time);
	cmd->passive_dwell_time = cpu_to_le32(pno->passive_max_time);

	if (pno->do_passive_scan)
		cmd->flags |= cpu_to_le32(WMI_NLO_CONFIG_SCAN_PASSIVE);

	cmd->fast_scan_period = cpu_to_le32(pno->fast_scan_period);
	cmd->slow_scan_period = cpu_to_le32(pno->slow_scan_period);
	cmd->fast_scan_max_cycles = cpu_to_le32(pno->fast_scan_max_cycles);
	cmd->delay_start_time = cpu_to_le32(pno->delay_start_time);

	if (pno->enable_pno_scan_randomization) {
		cmd->flags |= cpu_to_le32(WMI_NLO_CONFIG_SPOOFED_MAC_IN_PROBE_REQ |
					  WMI_NLO_CONFIG_RANDOM_SEQ_NO_IN_PROBE_REQ);
		ether_addr_copy(cmd->mac_addr.addr, pno->mac_addr);
		ether_addr_copy(cmd->mac_mask.addr, pno->mac_addr_mask);
	}

	ptr += sizeof(*cmd);

	/* nlo_configured_params(nlo_list) */
	cmd->no_of_ssids = cpu_to_le32(pno->uc_networks_count);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, nlo_list_len);

	ptr += sizeof(*tlv);
	nlo_list = ptr;
	for (i = 0; i < pno->uc_networks_count; i++) {
		tlv = (struct wmi_tlv *)(&nlo_list[i].tlv_header);
		tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_BYTE,
						     sizeof(*nlo_list));

		nlo_list[i].ssid.valid = cpu_to_le32(1);

		nlo_list[i].ssid.ssid.ssid_len =
			cpu_to_le32(pno->a_networks[i].ssid.ssid_len);
		memcpy(nlo_list[i].ssid.ssid.ssid,
		       pno->a_networks[i].ssid.ssid,
		       le32_to_cpu(nlo_list[i].ssid.ssid.ssid_len));

		if (pno->a_networks[i].rssi_threshold &&
		    pno->a_networks[i].rssi_threshold > -300) {
			nlo_list[i].rssi_cond.valid = cpu_to_le32(1);
			nlo_list[i].rssi_cond.rssi =
					cpu_to_le32(pno->a_networks[i].rssi_threshold);
		}

		nlo_list[i].bcast_nw_type.valid = cpu_to_le32(1);
		nlo_list[i].bcast_nw_type.bcast_nw_type =
					cpu_to_le32(pno->a_networks[i].bcast_nw_type);
	}

	ptr += nlo_list_len;
	cmd->num_of_channels = cpu_to_le32(pno->a_networks[0].channel_count);
	tlv = ptr;
	tlv->header =  ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, channel_list_len);
	ptr += sizeof(*tlv);
	channel_list = ptr;

	for (i = 0; i < pno->a_networks[0].channel_count; i++)
		channel_list[i] = cpu_to_le32(pno->a_networks[0].channels[i]);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi tlv start pno config vdev_id %d\n",
		   vdev_id);

	return skb;
}

static struct sk_buff *ath12k_wmi_op_gen_config_pno_stop(struct ath12k *ar,
							 u32 vdev_id)
{
	struct wmi_wow_nlo_config_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	cmd = (struct wmi_wow_nlo_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NLO_CONFIG_CMD, len);

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->flags = cpu_to_le32(WMI_NLO_CONFIG_STOP);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi tlv stop pno config vdev_id %d\n", vdev_id);
	return skb;
}

int ath12k_wmi_wow_config_pno(struct ath12k *ar, u32 vdev_id,
			      struct wmi_pno_scan_req_arg  *pno_scan)
{
	struct sk_buff *skb;

	if (pno_scan->enable)
		skb = ath12k_wmi_op_gen_config_pno_start(ar, vdev_id, pno_scan);
	else
		skb = ath12k_wmi_op_gen_config_pno_stop(ar, vdev_id);

	if (IS_ERR_OR_NULL(skb))
		return -ENOMEM;

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
}

static void ath12k_wmi_fill_ns_offload(struct ath12k *ar,
				       struct wmi_arp_ns_offload_arg *offload,
				       void **ptr,
				       bool enable,
				       bool ext)
{
	struct wmi_ns_offload_params *ns;
	struct wmi_tlv *tlv;
	void *buf_ptr = *ptr;
	u32 ns_cnt, ns_ext_tuples;
	int i, max_offloads;

	ns_cnt = offload->ipv6_count;

	tlv  = buf_ptr;

	if (ext) {
		ns_ext_tuples = offload->ipv6_count - WMI_MAX_NS_OFFLOADS;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 ns_ext_tuples * sizeof(*ns));
		i = WMI_MAX_NS_OFFLOADS;
		max_offloads = offload->ipv6_count;
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 WMI_MAX_NS_OFFLOADS * sizeof(*ns));
		i = 0;
		max_offloads = WMI_MAX_NS_OFFLOADS;
	}

	buf_ptr += sizeof(*tlv);

	for (; i < max_offloads; i++) {
		ns = buf_ptr;
		ns->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_NS_OFFLOAD_TUPLE,
							sizeof(*ns));

		if (enable) {
			if (i < ns_cnt)
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_VALID);

			memcpy(ns->target_ipaddr[0], offload->ipv6_addr[i], 16);
			memcpy(ns->solicitation_ipaddr, offload->self_ipv6_addr[i], 16);

			if (offload->ipv6_type[i])
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_IS_IPV6_ANYCAST);

			memcpy(ns->target_mac.addr, offload->mac_addr, ETH_ALEN);

			if (!is_zero_ether_addr(ns->target_mac.addr))
				ns->flags |= cpu_to_le32(WMI_NSOL_FLAGS_MAC_VALID);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "wmi index %d ns_solicited %pI6 target %pI6",
				   i, ns->solicitation_ipaddr,
				   ns->target_ipaddr[0]);
		}

		buf_ptr += sizeof(*ns);
	}

	*ptr = buf_ptr;
}

static void ath12k_wmi_fill_arp_offload(struct ath12k *ar,
					struct wmi_arp_ns_offload_arg *offload,
					void **ptr,
					bool enable)
{
	struct wmi_arp_offload_params *arp;
	struct wmi_tlv *tlv;
	void *buf_ptr = *ptr;
	int i;

	/* fill arp tuple */
	tlv = buf_ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 WMI_MAX_ARP_OFFLOADS * sizeof(*arp));
	buf_ptr += sizeof(*tlv);

	for (i = 0; i < WMI_MAX_ARP_OFFLOADS; i++) {
		arp = buf_ptr;
		arp->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARP_OFFLOAD_TUPLE,
							 sizeof(*arp));

		if (enable && i < offload->ipv4_count) {
			/* Copy the target ip addr and flags */
			arp->flags = cpu_to_le32(WMI_ARPOL_FLAGS_VALID);
			memcpy(arp->target_ipaddr, offload->ipv4_addr[i], 4);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "wmi arp offload address %pI4",
				   arp->target_ipaddr);
		}

		buf_ptr += sizeof(*arp);
	}

	*ptr = buf_ptr;
}

int ath12k_wmi_arp_ns_offload(struct ath12k *ar,
			      struct ath12k_link_vif *arvif,
			      struct wmi_arp_ns_offload_arg *offload,
			      bool enable)
{
	struct wmi_set_arp_ns_offload_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *buf_ptr;
	size_t len;
	u8 ns_cnt, ns_ext_tuples = 0;

	ns_cnt = offload->ipv6_count;

	len = sizeof(*cmd) +
	      sizeof(*tlv) +
	      WMI_MAX_NS_OFFLOADS * sizeof(struct wmi_ns_offload_params) +
	      sizeof(*tlv) +
	      WMI_MAX_ARP_OFFLOADS * sizeof(struct wmi_arp_offload_params);

	if (ns_cnt > WMI_MAX_NS_OFFLOADS) {
		ns_ext_tuples = ns_cnt - WMI_MAX_NS_OFFLOADS;
		len += sizeof(*tlv) +
		       ns_ext_tuples * sizeof(struct wmi_ns_offload_params);
	}

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	buf_ptr = skb->data;
	cmd = buf_ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_ARP_NS_OFFLOAD_CMD,
						 sizeof(*cmd));
	cmd->flags = cpu_to_le32(0);
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->num_ns_ext_tuples = cpu_to_le32(ns_ext_tuples);

	buf_ptr += sizeof(*cmd);

	ath12k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 0);
	ath12k_wmi_fill_arp_offload(ar, offload, &buf_ptr, enable);

	if (ns_ext_tuples)
		ath12k_wmi_fill_ns_offload(ar, offload, &buf_ptr, enable, 1);

	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_SET_ARP_NS_OFFLOAD_CMDID);
}

int ath12k_wmi_gtk_rekey_offload(struct ath12k *ar,
				 struct ath12k_link_vif *arvif, bool enable)
{
	struct ath12k_rekey_data *rekey_data = &arvif->rekey_data;
	struct wmi_gtk_rekey_offload_cmd *cmd;
	struct sk_buff *skb;
	__le64 replay_ctr;
	int len;

	len = sizeof(*cmd);
	skb =  ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_GTK_OFFLOAD_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);

	if (enable) {
		cmd->flags = cpu_to_le32(GTK_OFFLOAD_ENABLE_OPCODE);

		/* the length in rekey_data and cmd is equal */
		memcpy(cmd->kck, rekey_data->kck, sizeof(cmd->kck));
		memcpy(cmd->kek, rekey_data->kek, sizeof(cmd->kek));

		replay_ctr = cpu_to_le64(rekey_data->replay_ctr);
		memcpy(cmd->replay_ctr, &replay_ctr,
		       sizeof(replay_ctr));
	} else {
		cmd->flags = cpu_to_le32(GTK_OFFLOAD_DISABLE_OPCODE);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "offload gtk rekey vdev: %d %d\n",
		   arvif->vdev_id, enable);
	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath12k_wmi_gtk_rekey_getinfo(struct ath12k *ar,
				 struct ath12k_link_vif *arvif)
{
	struct wmi_gtk_rekey_offload_cmd *cmd;
	struct sk_buff *skb;
	int len;

	len = sizeof(*cmd);
	skb =  ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_gtk_rekey_offload_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_GTK_OFFLOAD_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->flags = cpu_to_le32(GTK_OFFLOAD_REQUEST_STATUS_OPCODE);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "get gtk rekey vdev_id: %d\n",
		   arvif->vdev_id);
	return ath12k_wmi_cmd_send(ar->wmi, skb, WMI_GTK_OFFLOAD_CMDID);
}

int ath12k_wmi_sta_keepalive(struct ath12k *ar,
			     const struct wmi_sta_keepalive_arg *arg)
{
	struct wmi_sta_keepalive_arp_resp_params *arp;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_sta_keepalive_cmd *cmd;
	struct sk_buff *skb;
	size_t len;

	len = sizeof(*cmd) + sizeof(*arp);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_keepalive_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_KEEPALIVE_CMD, sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->enabled = cpu_to_le32(arg->enabled);
	cmd->interval = cpu_to_le32(arg->interval);
	cmd->method = cpu_to_le32(arg->method);

	arp = (struct wmi_sta_keepalive_arp_resp_params *)(cmd + 1);
	arp->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_KEEPALVE_ARP_RESPONSE,
						 sizeof(*arp));
	if (arg->method == WMI_STA_KEEPALIVE_METHOD_UNSOLICITED_ARP_RESPONSE ||
	    arg->method == WMI_STA_KEEPALIVE_METHOD_GRATUITOUS_ARP_REQUEST) {
		arp->src_ip4_addr = cpu_to_le32(arg->src_ip4_addr);
		arp->dest_ip4_addr = cpu_to_le32(arg->dest_ip4_addr);
		ether_addr_copy(arp->dest_mac_addr.addr, arg->dest_mac_addr);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi sta keepalive vdev %d enabled %d method %d interval %d\n",
		   arg->vdev_id, arg->enabled, arg->method, arg->interval);

	return ath12k_wmi_cmd_send(wmi, skb, WMI_STA_KEEPALIVE_CMDID);
}

int ath12k_wmi_mlo_setup(struct ath12k *ar, struct wmi_mlo_setup_arg *mlo_params)
{
	struct wmi_mlo_setup_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	u32 *partner_links, num_links;
	int i, ret, arg_len, buf_len;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;

	num_links = mlo_params->num_partner_links;
	if (num_links > ATH12K_WMI_MLO_MAX_PARTNER_LINKS)
		return -EINVAL;

	arg_len = num_links * sizeof(u32);
	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + arg_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_setup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_SETUP_CMD,
						 sizeof(*cmd));
	cmd->mld_group_id = mlo_params->group_id;
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->max_num_ml_peers = cpu_to_le32(mlo_params->max_ml_peer_supported);
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, arg_len);
	ptr += TLV_HDR_SIZE;

	partner_links = ptr;
	for (i = 0; i < num_links; i++)
		partner_links[i] = mlo_params->partner_link_id[i];

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_SETUP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_MLO_SETUP_CMDID command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_ready(struct ath12k *ar)
{
	struct wmi_mlo_ready_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_ready_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_READY_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_READY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_MLO_READY_CMDID command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_teardown(struct ath12k *ar, bool umac_reset,
			    u32 reason_code, bool erp_standby_mode)
{
	struct wmi_mlo_teardown_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_teardown_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_TEARDOWN_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->umac_reset = cpu_to_le32(umac_reset);
	cmd->reason_code = cpu_to_le32(reason_code);
	cmd->erp_standby_mode = cpu_to_le32(erp_standby_mode);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_TEARDOWN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI MLO teardown command: %d\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_mlo_reconfig_link_removal(struct ath12k *ar, u32 vdev_id,
					 const u8 *reconfig_ml_ie,
					 size_t reconfig_ml_ie_len)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_mlo_link_removal_cmd_fixed_param *cmd;
	struct wmi_tlv *reconfig_ie_tlv;
	struct sk_buff *skb;
	int ret, len;
	u32 reconfig_ie_len_aligned = roundup(reconfig_ml_ie_len,
					      sizeof(u32));
	void *ptr;

	len = TLV_HDR_SIZE + sizeof(*cmd) + reconfig_ie_len_aligned;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_link_removal_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_LINK_REMOVAL_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->reconfig_ml_ie_num_bytes_valid = cpu_to_le32(reconfig_ml_ie_len);

	ptr = skb->data + sizeof(*cmd);

	reconfig_ie_tlv = ptr;
	reconfig_ie_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE,
						     reconfig_ie_len_aligned);
	memcpy(reconfig_ie_tlv->value, reconfig_ml_ie, reconfig_ml_ie_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_LINK_REMOVAL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_MLO_LINK_REMOVAL_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

bool ath12k_wmi_is_umac_migration_supported(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;

	return test_bit(WMI_SERVICE_UMAC_MIGRATION_SUPPORT, wmi_ab->svc_map);
}

bool ath12k_wmi_is_master_migration_supported(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;

	return test_bit(WMI_SERVICE_ML_PEER_MASTER_MIGRATION_SUPPORT, wmi_ab->svc_map);
}

int ath12k_wmi_mlo_send_ptqm_migrate_cmd(struct ath12k_link_vif *arvif,
				         struct list_head *peer_migr_list,
				         u16 num_peers)
{
	struct ath12k_mac_pri_link_migr_peer_node *peer_node, *tmp_peer;
	struct wmi_mlo_pri_link_peer_mig_fixed_param *cmd;
	struct wmi_mlo_new_pri_link_peer_info *peer_info;
	u16 max_entry_per_cmd = 0, max_entry_cnt = 0;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret = -EINVAL, len, i;

	max_entry_per_cmd = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] -
			     sizeof(*cmd) - TLV_HDR_SIZE) /
			    sizeof(*peer_info);

	if (!max_entry_per_cmd)
		return -EINVAL;

	reinit_completion(&arvif->wmi_migration_event_resp);

	while (num_peers > 0) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_entry_cnt = min(max_entry_per_cmd, num_peers);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Setting max entry limit for migration as %u\n",
			   max_entry_cnt);

		num_peers -= max_entry_cnt;
		len += sizeof(*peer_info) * max_entry_cnt;

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_mlo_pri_link_peer_mig_fixed_param *)skb->data;
		cmd->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_PRIMARY_LINK_PEER_MIGRATION_FIXED_PARAM,
					       (sizeof(*cmd)));
		cmd->vdev_id = cpu_to_le32(arvif->vdev_id);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*peer_info) * max_entry_cnt;
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
		ptr += TLV_HDR_SIZE;

		i = 0;
		list_for_each_entry(peer_node, peer_migr_list, list) {
			peer_info = ptr;
			len = sizeof(*peer_info);
			peer_info->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_NEW_PRIMARY_LINK_PEER_INFO,
						       len);
			peer_info->new_link_info =
				le32_encode_bits(peer_node->ml_peer_id,
						 WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_ML_PEER_ID) |
				le32_encode_bits(peer_node->hw_link_id,
						 WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_HW_LINK_ID);

			ath12k_dbg(ab, ATH12K_DBG_WMI,
				   "ml_peer_id %u new hw link id %d\n",
				   peer_node->ml_peer_id, peer_node->hw_link_id);

			ptr += len;

			if (++i == max_entry_cnt)
				break;
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_PRIMARY_LINK_PEER_MIGRATION_CMDID);
		if (ret) {
			ath12k_warn(ar->ab, "failed to send WMI_MLO_PRIMARY_LINK_PEER_MIGRATION cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "WMI PTQM command sent. vdev_id %d num_peers %d\n",
			   arvif->vdev_id, max_entry_cnt);

		/* now that command is sent, remove this from the list so
		 * that next iteration does not consider it again. Removing
		 * from list also indicates that command was sent successfully
		 * to firmware.
		 */
		i = 0;
		list_for_each_entry_safe(peer_node, tmp_peer, peer_migr_list, list) {
			list_move_tail(&peer_node->list, &arvif->peer_migrate_list);
			if (++i == max_entry_cnt)
				break;
		}

		/* cancel the worker if already there is a worker scheduled and
		 * schedule a new one. This basically resets the timer
		 */
		cancel_work_sync(&arvif->wmi_migration_cmd_work);
		queue_work(ar->ab->workqueue, &arvif->wmi_migration_cmd_work);
		arvif->is_umac_migration_in_progress = true;
	}

	return ret;
}

int ath12k_wmi_pdev_ap_ps_cmd_send(struct ath12k *ar, u8 pdev_id,
				   u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_ap_ps_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_ap_ps_cmd *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_PDEV_GREEN_AP_PS_ENABLE_CMD) |
			  FIELD_PREP(WMI_TLV_LEN, sizeof(*cmd) - TLV_HDR_SIZE);
	cmd->pdev_id = pdev_id;
	cmd->param_value = param_value;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send ap ps enable/disable cmd %d\n",ret);
		dev_kfree_skb(skb);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi pdev ap ps set pdev id %d value %d\n",
		   pdev_id, param_value);

	return ret;
}

bool ath12k_wmi_is_mvr_supported(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;

	return test_bit(WMI_TLV_SERVICE_MULTIPLE_VDEV_RESTART,
			 wmi_ab->svc_map) &&
		test_bit(WMI_TLV_SERVICE_MULTIPLE_VDEV_RESTART_RESPONSE_SUPPORT,
			 wmi_ab->svc_map);
}

int ath12k_wmi_pdev_multiple_vdev_restart(struct ath12k *ar,
					  struct wmi_pdev_multiple_vdev_restart_req_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = ar->ab;
	struct wmi_pdev_multiple_vdev_restart_request_cmd *cmd;
	struct ath12k_wmi_channel_params *chan;
	struct wmi_uhr_ap_npca_params *npca_params;
	struct wmi_tlv *tlv;
	struct ath12k_wmi_channel_params *chan_device;
	u32 num_vdev_ids;
	__le32 *vdev_ids;
	size_t vdev_ids_len;
	size_t npca_tlv_len;
	struct sk_buff *skb;
	void *ptr;
	int ret, len, i;
	bool device_params_present = false;

	if (ab->ag && ab->ag->num_devices >= ATH12K_MIN_NUM_DEVICES_NLINK) {
		if (WARN_ON(arg->vdev_ids.id_len > TARGET_NUM_VDEVS + TARGET_NUM_BRIDGE_VDEVS))
			return -EINVAL;
	} else {
		if (WARN_ON(arg->vdev_ids.id_len > TARGET_NUM_VDEVS))
			return -EINVAL;
	}

	num_vdev_ids = arg->vdev_ids.id_len;
	vdev_ids_len = num_vdev_ids * sizeof(__le32);

	/* dbw_chan (optional) + dbw_chan_info (optional, zero-len placeholder) */
	len = sizeof(*cmd) + TLV_HDR_SIZE + vdev_ids_len +
	      sizeof(*chan) + TLV_HDR_SIZE + TLV_HDR_SIZE +
	      TLV_HDR_SIZE + TLV_HDR_SIZE + TLV_HDR_SIZE;

	device_params_present = ath12k_wmi_check_device_present(arg->width_device,
								arg->center_freq_device,
								arg->vdev_start_arg.band_center_freq1);

	if (device_params_present)
		len += TLV_HDR_SIZE + sizeof(*chan_device);

	npca_tlv_len = arg->npca_arg.enabled ? sizeof(*npca_params) : 0;
	len += TLV_HDR_SIZE + npca_tlv_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_multiple_vdev_restart_request_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->num_vdevs = cpu_to_le32(arg->vdev_ids.id_len);

	cmd->puncture_20mhz_bitmap = cpu_to_le32(arg->ru_punct_bitmap);

	cmd->flags = cpu_to_le32(WMI_MVR_RESPONSE_SUPPORT_EXPECTED);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, vdev_ids_len);
	vdev_ids = (__le32 *)tlv->value;

	for (i = 0; i < num_vdev_ids; i++)
		vdev_ids[i] = cpu_to_le32(arg->vdev_ids.id[i]);

	ptr += TLV_HDR_SIZE + vdev_ids_len;
	chan = (struct ath12k_wmi_channel_params *)ptr;

	ath12k_wmi_put_wmi_channel(chan, &arg->vdev_start_arg);

	chan->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL, sizeof(*chan));
	ptr += sizeof(*chan);

	/* Zero length TLVs for phymode_list, preferred_tx_stream_list
	 * and preferred_rx_stream_list which are mandatory if any of
	 * the following TLVs are to be sent to target.
	 */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, 0);
	ptr += sizeof(*tlv);

	if (test_bit(WMI_TLV_SERVICE_SW_PROG_DFS_SUPPORT, ar->ab->wmi_ab.svc_map) &&
	    device_params_present) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 sizeof(*chan_device));
		ptr += TLV_HDR_SIZE;
		chan_device = ptr;
		ath12k_wmi_set_wmi_channel_device(chan_device, &arg->vdev_start_arg,
						  arg->center_freq_device,
						  arg->width_device);
		ptr += sizeof(*chan_device);
	} else {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += sizeof(*tlv);
	}

	/* Zero-length placeholder for dbw_chan_info (mandatory before npca_params) */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += sizeof(*tlv);

	/* npca_params TLV — single common entry for all vdevs */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, npca_tlv_len);
	ptr += TLV_HDR_SIZE;

	if (arg->npca_arg.enabled) {
		struct wmi_npca_arg *na = &arg->npca_arg;

		npca_params = ptr;
		npca_params->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UHR_AP_NPCA_PARAMS,
					       sizeof(*npca_params));
		npca_params->vdev_id = 0;
		npca_params->mode_tuple_field = cpu_to_le32(WMI_NPCA_MODE_ENABLE);

		ath12k_wmi_put_wmi_channel(&npca_params->npca_chan,
					   &arg->vdev_start_arg);
		npca_params->npca_chan.tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
					       sizeof(npca_params->npca_chan));
		npca_params->npca_chan.mhz = cpu_to_le32(na->npca_freq);

		npca_params->puncture_20mhz_bitmap =
			cpu_to_le32(na->npca_punct_bitmap);
		npca_params->npca_cap1 =
			le32_encode_bits(na->npca_min_dur_threshold,
					 WMI_NPCA_CAP1_MIN_THRESHOLD) |
			le32_encode_bits(na->npca_switch_delay,
					 WMI_NPCA_CAP1_SWITCH_DELAY) |
			le32_encode_bits(na->npca_switch_back_delay,
					 WMI_NPCA_CAP1_SWITCH_BACK_DELAY) |
			le32_encode_bits(na->npca_initial_qsrc,
					 WMI_NPCA_CAP1_INITIAL_QSRC) |
			le32_encode_bits(na->npca_moplen,
					 WMI_NPCA_CAP1_MOPLEN);
		npca_params->npca_cap2 = 0;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "mvr npca npca_freq=%u punct=0x%x min_dur=%u sw_dly=%u swb_dly=%u qsrc=%u moplen=%u cap1=0x%x\n",
			   na->npca_freq, na->npca_punct_bitmap,
			   na->npca_min_dur_threshold, na->npca_switch_delay,
			   na->npca_switch_back_delay, na->npca_initial_qsrc,
			   na->npca_moplen,
			   le32_to_cpu(npca_params->npca_cap1));
		ptr += sizeof(*npca_params);
	}

	spin_lock_bh(&ar->data_lock);
	ar->chanctx_switch_stats.mvr_posting_time_us = ath12k_get_timestamp_in_us();
	spin_unlock_bh(&ar->data_lock);
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_MULTIPLE_VDEV_RESTART_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "wmi failed to send mvr command (%d)\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi mvr cmd sent num_vdevs %d freq %d\n",
		   num_vdev_ids, arg->vdev_start_arg.freq);

	return ret;
}

static void ath12k_wmi_put_peer_list(struct ath12k_base *ab,
				     struct wmi_chan_width_peer_list *peer_list,
				     struct wmi_chan_width_peer_arg *peer_arg,
				     u32 num_peers, int start_idx)
{
	struct wmi_chan_width_peer_list *itr;
	struct wmi_chan_width_peer_arg *arg_itr;
	int i;
	u32 host_chan_width;

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi peer channel width switch command peer list\n");

	for (i = 0; i < num_peers; i++) {
		itr = &peer_list[i];
		arg_itr = &peer_arg[start_idx + i];

		itr->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHAN_WIDTH_PEER_LIST,
							 sizeof(*itr));
		ether_addr_copy(itr->mac_addr.addr, arg_itr->mac_addr.addr);

		/* Convert ieee80211_sta_rx_bw to wmi_host_chan_width */
		switch (arg_itr->chan_width) {
		case IEEE80211_STA_RX_BW_20:
			host_chan_width = WMI_HOST_CHAN_WIDTH_20;
		break;
		case IEEE80211_STA_RX_BW_40:
			host_chan_width = WMI_HOST_CHAN_WIDTH_40;
			break;
		case IEEE80211_STA_RX_BW_80:
			host_chan_width = WMI_HOST_CHAN_WIDTH_80;
		break;
		case IEEE80211_STA_RX_BW_160:
			host_chan_width = WMI_HOST_CHAN_WIDTH_160;
		break;
		case IEEE80211_STA_RX_BW_320:
			host_chan_width = WMI_HOST_CHAN_WIDTH_320;
		break;
		default:
			ath12k_warn(ab, "invalid bw %d switching back to 20 MHz\n",
				    arg_itr->chan_width);
			host_chan_width = WMI_HOST_CHAN_WIDTH_20;
			break;
		}

		itr->chan_width = cpu_to_le32(host_chan_width);
		itr->puncture_20mhz_bitmap = cpu_to_le32(arg_itr->puncture_20mhz_bitmap);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "   (%u) width %u addr %pM punct_bitmap 0x%x host chan_width: %d\n",
			   i + 1, arg_itr->chan_width, arg_itr->mac_addr.addr,
			   arg_itr->puncture_20mhz_bitmap, host_chan_width);
	}
}

static int ath12k_wmi_peer_chan_width_switch(struct ath12k *ar,
					     struct wmi_peer_chan_width_switch_arg *arg)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_chan_width_switch_req_cmd *cmd;
	struct wmi_chan_width_peer_list *peer_list;
	struct wmi_tlv *tlv;
	u32 num_peers;
	size_t peer_list_len;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;

	num_peers = arg->num_peers;

	if (WARN_ON(num_peers > ab->chwidth_num_peer_caps))
		return -EINVAL;

	peer_list_len = num_peers * sizeof(*peer_list);

	len = sizeof(*cmd) + TLV_HDR_SIZE + peer_list_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_chan_width_switch_req_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CHAN_WIDTH_SWITCH_CMD,
						 sizeof(*cmd));
	cmd->num_peers = cpu_to_le32(num_peers);
	cmd->vdev_var = cpu_to_le32(arg->vdev_var);

	ptr = skb->data + sizeof(*cmd);
	tlv = (struct wmi_tlv *)ptr;

	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, peer_list_len);
	peer_list = (struct wmi_chan_width_peer_list *)tlv->value;

	ath12k_wmi_put_peer_list(ab, peer_list, arg->peer_arg, num_peers,
				 arg->start_idx);

	ptr += peer_list_len;

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CHAN_WIDTH_SWITCH_CMDID);
	if (ret) {
		ath12k_warn(ab, "wmi failed to send peer chan width switch command (%d)\n",
			    ret);
		dev_kfree_skb(skb);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi peer chan width switch cmd sent num_peers %d \n",
		   num_peers);

	return ret;
}

void ath12k_wmi_set_peers_chan_width(struct ath12k_link_vif *arvif,
				     struct wmi_chan_width_peer_arg *peer_arg,
				     int num, u8 start_idx)
{
	struct ath12k *ar = arvif->ar;
	struct wmi_chan_width_peer_arg *arg;
	int i, err;

	for (i = 0; i < num; i++) {
		arg = &peer_arg[start_idx + i];

		err = ath12k_wmi_set_peer_param(ar, arg->mac_addr.addr,
						arvif->vdev_id, WMI_PEER_CHWIDTH,
						arg->chan_width);
		if (err) {
			ath12k_warn(ar->ab, "failed to update STA %pM peer bw %d: %d\n",
				    arg->mac_addr.addr, arg->chan_width, err);
			continue;
		}
	}
}

void ath12k_wmi_peer_chan_width_switch_work(struct wiphy *wiphy, struct wiphy_work *work)
{
	struct ath12k_link_vif *arvif = container_of(work, struct ath12k_link_vif,
						     peer_ch_width_switch_work);
	struct ath12k *ar = arvif->ar;
	struct ath12k_peer_ch_width_switch_data *data;
	struct wmi_peer_chan_width_switch_arg arg;
	struct ath12k_link_sta *arsta;
	union ath12k_config_param val;
	void *dp_peer;
	const u8 *peer_addr;
	unsigned long time_left = 0;
	int count_left, curr_count, max_count_per_cmd = ar->ab->chwidth_num_peer_caps;
	int cmd_num = 0, valid, i, ret;

	/* possible that the worker got scheduled after complete was triggered. In
	 * this case we don't wait for timeout
	 */
	if (arvif->peer_ch_width_switch_data->count == arvif->num_stations)
		goto send_cmd;

	time_left = wait_for_completion_timeout(&arvif->peer_ch_width_switch_send,
						ATH12K_PEER_CH_WIDTH_SWITCH_TIMEOUT_HZ);
	if (time_left == 0) {
		/* Even though timeout occured, we would send the command for the peers
		 * for which we received sta rc update event, hence not returning
		 */
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "timed out waiting for all peers in peer channel width switch\n");
	}

send_cmd:

	data = arvif->peer_ch_width_switch_data;

	/* Filter out peers that have not completed firmware association.
	 * Peer data may have been queued while associated but the peer could
	 * have been deleted and reconnected without completing assoc by the
	 * time this work runs. Compact the array in-place before batching.
	 */
	valid = 0;
	for (i = 0; i < data->count; i++) {
		peer_addr = data->peer_arg[i].mac_addr.addr;

		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, peer_addr);
		spin_unlock_bh(&ar->arsta_lock);

		if (!arsta) {
			ath12k_info(ar->ab,
				    "wmi skip chan width switch, arsta not found for peer %pM\n",
				    peer_addr);
			continue;
		}

		dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(wiphy, arsta->ahsta);
		if (!dp_peer)
			continue;

		memset(&val, 0, sizeof(val));
		if (ath12k_dp_link_peer_get_param_by_dp_peer_and_link_id(dp_peer,
				arsta->link_id, ATH12K_DP_LINK_PEER_ASSOC_PARAM,
				&val) || !val.assoc_success) {
			ath12k_info(ar->ab,
				    "wmi skip chan width switch for non-assoc peer %pM\n",
				    peer_addr);
			continue;
		}

		data->peer_arg[valid++] = data->peer_arg[i];
	}
	data->count = valid;

	spin_lock_bh(&ar->data_lock);
	arg.vdev_var = arvif->vdev_id;
	spin_unlock_bh(&ar->data_lock);

	arg.vdev_var |= ATH12K_PEER_VALID_VDEV_ID | ATH12K_PEER_PUNCT_BITMAP_VALID;
	arg.peer_arg = data->peer_arg;

	count_left = data->count;

	while (count_left > 0) {
		if (count_left <= max_count_per_cmd)
			curr_count = count_left;
		else
			curr_count = max_count_per_cmd;

		count_left -= curr_count;

		cmd_num++;

		arg.num_peers = curr_count;
		arg.start_idx = (cmd_num - 1) * max_count_per_cmd;

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "wmi peer channel width switch command num %u\n",
			   cmd_num);

		ret = ath12k_wmi_peer_chan_width_switch(ar, &arg);
		if (ret) {
			/* fallback */
			ath12k_wmi_set_peers_chan_width(arvif, arg.peer_arg,
							arg.num_peers,
							arg.start_idx);
		}
	}

	kfree(arvif->peer_ch_width_switch_data);
	arvif->peer_ch_width_switch_data = NULL;
}

int ath12k_wmi_send_wsi_stats_info(struct ath12k *ar,
				   struct ath12k_wmi_wsi_stats_info_param *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_wsi_stats_info_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_wsi_stats_info_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_WSI_STATS_INFO_CMD,
						 (sizeof(*cmd)));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->wsi_ingress_load_info = cpu_to_le32(param->wsi_ingress_load_info);
	cmd->wsi_egress_load_info = cpu_to_le32(param->wsi_egress_load_info);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev wsi stats info pdev_id %d ingress_load_info %d egress_load_info %d\n",
		   cmd->pdev_id, cmd->wsi_ingress_load_info, cmd->wsi_egress_load_info);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_WSI_STATS_INFO_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_WSI_STATS_INFO_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_set_neighbor_rx_cmd(struct ath12k *ar,
					struct ath12k_set_neighbor_rx_params *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_neighbor_rx_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_neighbor_rx_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_FILTER_NRP_CONFIG_CMD,
						(sizeof(*cmd)));
	cmd->vdev_id = cpu_to_le32(param->vdev_id);
	cmd->action = cpu_to_le32(param->action);
	cmd->type = WMI_FILTER_NRP_TYPE_STA_MACADDR;
	cmd->bssid_idx = cpu_to_le32(1);
	ether_addr_copy(cmd->macaddr.addr, param->nrp_addr);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set nrp config vdev_id %d action %d nrp mac %pM\n",
		   cmd->vdev_id, cmd->action, param->nrp_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send VDEV_FILTER_NEIGHBOR_RX_PACKETS_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_set_tx_peer_filter_cmd(struct ath12k *ar,
				 struct ath12k_set_tx_peer_filter_params *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_tx_peer_filter_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_tx_peer_filter_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_TX_FILTER_CMD,
						sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(param->vdev_id);
	cmd->action = cpu_to_le32(param->action);
	ether_addr_copy(cmd->macaddr.addr, param->mac_addr);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set peer tx filter vdev_id %d action %d mac %pM\n",
		   param->vdev_id, param->action, param->mac_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_TX_FILTER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_TX_FILTER_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_wmi_vdev_set_tx_peer_filter_cmd);

int ath12k_wmi_tx_mon_pkt_cap_set_config(struct ath12k *ar, u32 bitmap)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tx_mon_pkt_cap_set_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_tx_mon_pkt_cap_set_config_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SOC_TX_PACKET_CUSTOM_CLASSIFY_CMD,
				       sizeof(*cmd));
	cmd->pkt_type_bitmap = cpu_to_le32(bitmap << 1);

	ath12k_dbg(ar->ab, ATH12K_DBG_DP_MON_TX | ATH12K_DBG_WMI,
		   "WMI tx mon pkt cap config bitmap 0x%x\n", bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SOC_TX_PACKET_CUSTOM_CLASSIFY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SOC_TX_PACKET_CUSTOM_CLASSIFY_CMDID: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

static void *
ath12k_populate_link_control_tlv(void *buf_ptr,
				 struct ath12k_wmi_ttlm_peer_params *params)
{
	struct wmi_mlo_peer_link_control_param *link_control;
	u8 pref_link = 0;
	u8 latency = 0;
	u8 links = 0;
	struct wmi_tlv *tlv = buf_ptr;

	/* The Link Preference TLV is planned to be deprecated,
	 * so the TLV is going to be exlcuded by default.
	 */
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	buf_ptr += TLV_HDR_SIZE;

	tlv = buf_ptr;
	if (params->preferred_links.num_pref_links) {
		tlv->header =
			ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					   sizeof(struct wmi_mlo_peer_link_control_param));

		buf_ptr += TLV_HDR_SIZE;

		link_control = (struct wmi_mlo_peer_link_control_param *)buf_ptr;
		link_control->tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_MLO_PEER_LINK_CONTROL_PARAM,
					   sizeof(*link_control) - TLV_HDR_SIZE);
		link_control->num_links =
			cpu_to_le32(params->preferred_links.num_pref_links);
		links = cpu_to_le32(params->preferred_links.num_pref_links);

		for (pref_link = 0; pref_link < links; pref_link++) {
			link_control->link_priority_order[pref_link] =
			cpu_to_le32(params->preferred_links.preferred_link_order[pref_link]);
		}
		link_control->flags =
			cpu_to_le32(params->preferred_links.link_control_flags);
		link_control->tx_link_tuple_bitmap =
			cpu_to_le32(params->preferred_links.tlt_characterization_params);

		for (latency = 0; latency < WLAN_MAX_AC; latency++) {
			link_control->max_timeout_ms[latency] =
				cpu_to_le32(params->preferred_links.timeout[latency]);
		}
		buf_ptr += sizeof(struct wmi_mlo_peer_link_control_param);
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		buf_ptr += TLV_HDR_SIZE;
	}
	return buf_ptr;
}

static void*
ath12k_wmi_fill_tid_to_link_map_info(struct ath12k_wmi_ttlm_peer_params *params,
				     struct wmi_tid_to_link_map *ttlm,
				     void *ptr)
{
	struct ath12k_wmi_host_ttlm_of_tids *ttlm_of_tids;
	int dir, tid;

	for (dir = 0; dir < params->num_dir; dir++) {
		ttlm_of_tids = &params->ttlm_info[dir];
		for (tid = 0; tid < TTLM_MAX_NUM_TIDS; tid++) {
			ttlm = (struct wmi_tid_to_link_map *)ptr;
			ttlm->tlv_header =
				ath12k_wmi_tlv_hdr(WMI_TAG_TID_TO_LINK_MAP,
						   sizeof(*ttlm) - TLV_HDR_SIZE);
			/* populate tid number */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(tid,
						 WMI_TTLM_TID_MASK);

			/* populate direction */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->direction,
						 WMI_TTLM_DIR_MASK);

			/* populate default link mapping value */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->default_link_mapping,
						 WMI_TTLM_DEFAULT_MAPPING_MASK);

			/* populate ttlm provisioned links for the
			 * corressponding tid number
			 */
			ttlm->tid_to_link_map_info |=
				le32_encode_bits(ttlm_of_tids->ttlm_provisioned_links[tid],
						 WMI_TTLM_LINK_MAPPING_MASK);

			ptr += sizeof(*ttlm);
		}
	}
	return ptr;
}

int
ath12k_wmi_send_mlo_peer_tid_to_link_map_cmd(struct ath12k *ar,
					     struct ath12k_wmi_ttlm_peer_params *params,
					     bool ttlm_info)
{
	struct wmi_peer_tid_to_link_map_fixed_param *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tid_to_link_map *ttlm = NULL;
	int ret, buf_len;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;

	/* find the buf len pref link */
	buf_len = sizeof(*cmd);

	buf_len += TLV_HDR_SIZE;

	if (ttlm_info) {
		if (params->num_dir > ATH12K_WMI_TTLM_MAX_DIRECTION)
			return -EINVAL;
		buf_len += params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);
	}

	/* Update the length for preferred link tlv.
	 * The link preference tlv is planned to be deprecated, so the tlv
	 * is going to be excluded by default
	 */
	buf_len += TLV_HDR_SIZE;

	/* update the length for link control tlv */
	buf_len += TLV_HDR_SIZE;
	if (params->preferred_links.num_pref_links)
		buf_len += sizeof(struct wmi_mlo_peer_link_control_param);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_tid_to_link_map_fixed_param *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_TID_TO_LINK_MAP_FIXED_PARAM,
				       sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(params->pdev_id);
	ether_addr_copy(cmd->link_macaddr.addr, params->peer_macaddr);

	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;

	if (ttlm_info) {
		buf_len = params->num_dir * TTLM_MAX_NUM_TIDS * sizeof(*ttlm);
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 buf_len);
		ptr += TLV_HDR_SIZE;

		ptr = ath12k_wmi_fill_tid_to_link_map_info(params, ttlm, ptr);
	} else {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	ptr = ath12k_populate_link_control_tlv(ptr, params);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MLO_PEER_TID_TO_LINK_MAP_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void
ath12k_wmi_ttlm_update_ie_info(struct ath12k *ar,
			       struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *ie_info,
			       struct ath12k_wmi_mlo_ttlm_ie *params)
{
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.direction,
				 WMI_ADV_TTLM_CTRL_DIRECTION_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.default_link_mapping,
				 WMI_ADV_TTLM_CTRL_DEFAULT_LINK_MAPPING_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.mapping_switch_time_present,
				 WMI_ADV_TTLM_CTRL_MST_PRESENT_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.expected_duration_present,
				 WMI_ADV_TTLM_CTRL_ED_PRESENT_MASK);
	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(params->ttlm.link_mapping_size,
				 WMI_ADV_TTLM_CTRL_LINK_MAP_SIZE_MASK);

	ie_info->map_switch_time = cpu_to_le32(params->ttlm.mapping_switch_time);
	ie_info->expected_duration = cpu_to_le32(params->ttlm.expected_duration);
	ie_info->disabled_link_bitmap = cpu_to_le32(params->disabled_link_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "dir:%d def_map:%d mst_persent:%d ed_present:%d "
		   "link_map_size:%d mst:%d ed:%d disabled_link_map 0x%x",
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_DIRECTION_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_DEFAULT_LINK_MAPPING_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_MST_PRESENT_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_ED_PRESENT_MASK),
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_LINK_MAP_SIZE_MASK),
		   ie_info->map_switch_time,
		   ie_info->expected_duration,
		   ie_info->disabled_link_bitmap);

	/* Do not fill link mapping values when default mapping is set to 1 */
	if (params->ttlm.default_link_mapping)
		return;

	ie_info->tid_to_link_map_ctrl |=
		le32_encode_bits(0xff,
				 WMI_ADV_TTLM_CTRL_MAP_PRESENCE_INDICATOR_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "link_map_present_indicator: 0x%x",
		   le32_get_bits(ie_info->tid_to_link_map_ctrl,
				 WMI_ADV_TTLM_CTRL_MAP_PRESENCE_INDICATOR_MASK));

	ie_info->ieee_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[0],
				 WMI_ADV_TTLM_TID0_LINK_MAP_MASK);
	ie_info->ieee_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[1],
				 WMI_ADV_TTLM_TID1_LINK_MAP_MASK);
	ie_info->ieee_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[2],
				 WMI_ADV_TTLM_TID2_LINK_MAP_MASK);
	ie_info->ieee_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[3],
				 WMI_ADV_TTLM_TID3_LINK_MAP_MASK);
	ie_info->ieee_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[4],
				 WMI_ADV_TTLM_TID4_LINK_MAP_MASK);
	ie_info->ieee_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[5],
				 WMI_ADV_TTLM_TID5_LINK_MAP_MASK);
	ie_info->ieee_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[6],
				 WMI_ADV_TTLM_TID6_LINK_MAP_MASK);
	ie_info->ieee_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.ieee_link_map_tid[7],
				 WMI_ADV_TTLM_TID7_LINK_MAP_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ieee link map of tid 0:0x%x 1:0x%x 2:0x%x 3:0x%x 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
		   le32_get_bits(ie_info->ieee_tid_0_1_link_map, WMI_ADV_TTLM_TID0_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_0_1_link_map, WMI_ADV_TTLM_TID1_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_2_3_link_map, WMI_ADV_TTLM_TID2_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_2_3_link_map, WMI_ADV_TTLM_TID3_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_4_5_link_map, WMI_ADV_TTLM_TID4_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_4_5_link_map, WMI_ADV_TTLM_TID5_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_6_7_link_map, WMI_ADV_TTLM_TID6_LINK_MAP_MASK),
		   le32_get_bits(ie_info->ieee_tid_6_7_link_map, WMI_ADV_TTLM_TID7_LINK_MAP_MASK));

	ie_info->hw_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[0],
				 WMI_ADV_TTLM_TID0_LINK_MAP_MASK);
	ie_info->hw_tid_0_1_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[1],
				 WMI_ADV_TTLM_TID1_LINK_MAP_MASK);
	ie_info->hw_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[2],
				 WMI_ADV_TTLM_TID2_LINK_MAP_MASK);
	ie_info->hw_tid_2_3_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[3],
				 WMI_ADV_TTLM_TID3_LINK_MAP_MASK);
	ie_info->hw_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[4],
				 WMI_ADV_TTLM_TID4_LINK_MAP_MASK);
	ie_info->hw_tid_4_5_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[5],
				 WMI_ADV_TTLM_TID5_LINK_MAP_MASK);
	ie_info->hw_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[6],
				 WMI_ADV_TTLM_TID6_LINK_MAP_MASK);
	ie_info->hw_tid_6_7_link_map |=
		le32_encode_bits(params->ttlm.hw_link_map_tid[7],
				 WMI_ADV_TTLM_TID7_LINK_MAP_MASK);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "hw link map of tid 0:0x%x 1:0x%x 2:0x%x 3:0x%x 4:0x%x 5:0x%x 6:0x%x 7:0x%x\n",
		   le32_get_bits(ie_info->hw_tid_0_1_link_map, WMI_ADV_TTLM_TID0_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_0_1_link_map, WMI_ADV_TTLM_TID1_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_2_3_link_map, WMI_ADV_TTLM_TID2_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_2_3_link_map, WMI_ADV_TTLM_TID3_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_4_5_link_map, WMI_ADV_TTLM_TID4_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_4_5_link_map, WMI_ADV_TTLM_TID5_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_6_7_link_map, WMI_ADV_TTLM_TID6_LINK_MAP_MASK),
		   le32_get_bits(ie_info->hw_tid_6_7_link_map, WMI_ADV_TTLM_TID7_LINK_MAP_MASK));
}

int
ath12k_wmi_ap_tid_to_link_map_config(struct ath12k *ar,
				     struct ath12k_wmi_tid_to_link_map_ap_params *params)
{
	struct wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *cmd;
	struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *ie_info;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int i, ret, buf_len;

	if (params->num_ttlm_info > WLAN_MAX_TTLM_IE) {
		ath12k_warn(ar->ab,
			    "Failed to send TTLM command to FW for vdev id %d as ttlm info %d is greater than max %d",
			    params->vdev_id,
			    params->num_ttlm_info,
			    WLAN_MAX_TTLM_IE);
		return -EINVAL;
	}
	buf_len = sizeof(*cmd) + TLV_HDR_SIZE + (params->num_ttlm_info *
						 sizeof(*ie_info));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mlo_ap_vdev_tid_to_link_map_cmd_fixed_param *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MLO_TID_TO_LINK_MAPPING_CMD_FIXED_PARAM,
				       sizeof(*cmd));
	cmd->pdev_id = params->pdev_id;
	cmd->vdev_id = params->vdev_id;
	ptr = skb->data + sizeof(*cmd);
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 (params->num_ttlm_info *
					  sizeof(*ie_info)));
	ptr += TLV_HDR_SIZE;

	for (i = 0; i < params->num_ttlm_info; i++) {
		ie_info = (struct wmi_mlo_ap_vdev_tid_to_link_map_ie_info *)ptr;
		ie_info->tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_MLO_TID_TO_LINK_MAPPING_IE_INFO,
					   sizeof(*ie_info) - TLV_HDR_SIZE);
		/* update the ie info params */
		ath12k_wmi_ttlm_update_ie_info(ar, ie_info, &params->ie[i]);
		ptr += sizeof(*ie_info);
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MLO_AP_VDEV_TID_TO_LINK_MAP_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_dl_qos_profile_create(struct ath12k_base *ab,
				     struct ath12k_qos_params *param,
				     u8 qos_profile_id)
{
	struct ath12k *ar;
	struct ath12k_wmi_pdev *wmi;
	struct wmi_sawf_svc_cfg_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int len, ret;

	if (ab->is_bypassed)
		return 0;

	ar = ab->pdevs[0].ar;
	if (!ar)
		return -EINVAL;

	wmi = ar->wmi;
	if (!wmi)
		return -EINVAL;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sawf_svc_cfg_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SAWF_SERVICE_CLASS_CFG_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->svc_class_id = cpu_to_le32(qos_profile_id);
	cmd->min_thruput_kbps = cpu_to_le32(param->min_data_rate);
	cmd->max_thruput_kbps = cpu_to_le32(param->mean_data_rate);
	cmd->burst_size_bytes = cpu_to_le32(param->burst_size);
	cmd->svc_interval_ms = cpu_to_le32(param->min_service_interval);
	cmd->delay_bound_ms = cpu_to_le32(param->delay_bound);
	cmd->time_to_live_ms = cpu_to_le32(param->msdu_life_time);
	cmd->priority = cpu_to_le32(param->priority);
	cmd->tid = cpu_to_le32(param->tid);
	cmd->msdu_loss_rate_ppm = cpu_to_le32(param->msdu_delivery_info);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "QoS Profile Configure: Profile ID: %u, min_throughput: %u,"
		   "max_throughput: %u, burst_size: %u, svc_interval: %u,"
		   "delay_bound: %u, TTL: %u, priority: %u,"
		   "tid: %u, msdu_loss_rate: %u",
		   cmd->svc_class_id, cmd->min_thruput_kbps,
		   cmd->max_thruput_kbps,
		   cmd->burst_size_bytes, cmd->svc_interval_ms,
		   cmd->delay_bound_ms,
		   cmd->time_to_live_ms, cmd->priority, cmd->tid,
		   cmd->msdu_loss_rate_ppm);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SAWF_SERVICE_CLASS_CFG_CMDID);
	if (ret) {
		ath12k_err(ab,
			   "failed to config/reconfig QoS params");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_dl_qos_profile_delete(struct ath12k_base *ab, u8 qos_profile_id)
{
	struct ath12k *ar;
	struct ath12k_wmi_pdev *wmi;
	struct wmi_sawf_svc_disable_cmd_fixed_param *cmd;
	struct sk_buff *skb;
	int len, ret;

	ar = ab->pdevs[0].ar;
	if (!ar)
		return -ENOMEM;

	wmi = ar->wmi;
	if (!wmi)
		return -ENOMEM;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sawf_svc_disable_cmd_fixed_param *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SAWF_SERVICE_CLASS_DISABLE_CMD_FIXED_PARAM,
						 sizeof(*cmd));

	cmd->svc_class_id = cpu_to_le32(qos_profile_id);

	ath12k_dbg_level(ar->ab, ATH12K_DBG_WMI, ATH12K_DBG_L2,
			 "QoS profile WMI disable: qos id: %u",
			 cmd->svc_class_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_SAWF_SERVICE_CLASS_DISABLE_CMDID);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to disable qos id: %u\n",
			   qos_profile_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_ul_qos_profile_config(struct ath12k *ar,
				     struct ath12k_wmi_ul_qos_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tid_latency_info *latency_info;
	struct wmi_peer_tid_latency_config_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	u32 len;
	u32 num_peer = 1;
	int ret;

	if (!params)
		return -EINVAL;

	len = sizeof(*cmd) + TLV_HDR_SIZE + (num_peer * sizeof(*latency_info));
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		ath12k_warn(ar->ab, "wmi cfg peer latency fail-Outof Memory\n");
		return -ENOMEM;
	}
	cmd = (struct wmi_peer_tid_latency_config_fixed_param *)skb->data;
	cmd->tlv_header =
		le32_encode_bits(WMI_TAG_PEER_TID_LATENCY_CONFIG_FIXED_PARAM,
				 WMI_TLV_TAG) |
		le32_encode_bits((sizeof(*cmd) - TLV_HDR_SIZE),
				 WMI_TLV_LEN);

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	len = sizeof(*latency_info) * num_peer;

	tlv->header = le32_encode_bits(WMI_TAG_ARRAY_STRUCT, WMI_TLV_TAG) |
			le32_encode_bits(len, WMI_TLV_LEN);

	latency_info =
		(struct wmi_tid_latency_info *)(skb->data + sizeof(*cmd) + TLV_HDR_SIZE);

	latency_info->tlv_header =
		le32_encode_bits(WMI_TAG_TID_LATENCY_INFO,
				 WMI_TLV_TAG) |
		le32_encode_bits((sizeof(*latency_info) - TLV_HDR_SIZE),
				 WMI_TLV_LEN);

	latency_info->service_interval = cpu_to_le32(params->service_interval);
	latency_info->burst_size_diff = cpu_to_le32(params->burst_size);
	latency_info->max_latency = cpu_to_le32(params->max_latency);
	latency_info->min_tput = cpu_to_le32(params->min_throughput);

	ether_addr_copy(latency_info->destmac.addr, params->peer_mac);

	latency_info->latency_tid_info =
		le32_encode_bits(params->latency_tid, SDWF_UL_TID_NUM) |
		le32_encode_bits(params->ac, SDWF_UL_AC) |
		le32_encode_bits(params->dl_enable, SDWF_UL_DL_EN) |
		le32_encode_bits(params->ul_enable, SDWF_UL_UL_EN) |
		le32_encode_bits(params->add_or_sub, SDWF_UL_BURST_SZ_SUM) |
		le32_encode_bits(params->sawf_ul_param, SDWF_UL_PARAM) |
		le32_encode_bits(params->ofdma_disable,
				 SDWF_UL_UL_OFDMA_DISABLE) |
		le32_encode_bits(params->mu_mimo_disable,
				 SDWF_UL_UL_MU_MIMO_DISABLE);
	ret = ath12k_wmi_cmd_send_nowait(wmi, skb,
					 WMI_PEER_TID_LATENCY_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_PEER_TID_LATENCY_CONFIG_CMDID cmd %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_pktlog_enable(struct ath12k *ar, u32 pktlog_filter)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_pktlog_enable_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_pktlog_enable_cmd *)skb->data;

        cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PKTLOG_ENABLE_CMD,
                                                 sizeof(*cmd));

        cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));
        cmd->evlist = cpu_to_le32(pktlog_filter);
        cmd->enable = cpu_to_le32(ATH12K_WMI_PKTLOG_ENABLE_FORCE);

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_PDEV_PKTLOG_ENABLE_CMDID);
        if (ret) {
                ath12k_err(ar->ab,
			   "failed to send WMI_PDEV_PKTLOG_ENABLE_CMDID\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_pdev_pktlog_disable(struct ath12k *ar)
{
        struct ath12k_wmi_pdev *wmi = ar->wmi;
        struct wmi_pktlog_disable_cmd *cmd;
        struct sk_buff *skb;
        int ret;

        skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
        if (!skb)
                return -ENOMEM;

        cmd = (struct wmi_pktlog_disable_cmd *)skb->data;

        cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_PKTLOG_DISABLE_CMD,
                                                 sizeof(*cmd));

        cmd->pdev_id = cpu_to_le32(DP_HW2SW_MACID(ar->pdev->pdev_id));

        ret = ath12k_wmi_cmd_send(wmi, skb,
                                  WMI_PDEV_PKTLOG_DISABLE_CMDID);
        if (ret) {
                ath12k_err(ar->ab,
			   "failed to send WMI_PDEV_PKTLOG_DISABLE_CMDID\n");
                dev_kfree_skb(skb);
        }

        return ret;
}

int ath12k_wmi_vdev_adfs_ch_cfg_cmd_send(struct ath12k *ar,
					 u32 vdev_id,
					 struct cfg80211_chan_def *def)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_adfs_ch_cfg_cmd *cmd;
	u32 min_duration_ms;
	struct sk_buff *skb;
	int ret = 0;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ch_cfg_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_ADFS_CH_CFG_CMD,
						 sizeof(struct wmi_vdev_adfs_ch_cfg_cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	if (ar->ab->dfs_region == ATH12K_DFS_REG_ETSI) {
		cmd->ocac_mode = WMI_ADFS_MODE_QUICK_OCAC;
		min_duration_ms = cfg80211_chandef_dfs_cac_time(ar->ah->hw->wiphy,
								def, true, false);
		cmd->min_duration_ms = cpu_to_le32(min_duration_ms);

		if (min_duration_ms == MIN_WEATHER_RADAR_CHAN_PRECAC_TIMEOUT)
			cmd->max_duration_ms = cpu_to_le32(MAX_WEATHER_RADAR_CHAN_PRECAC_TIMEOUT);
		else
			cmd->max_duration_ms = cpu_to_le32(MAX_PRECAC_TIMEOUT);
	} else if (ar->ab->dfs_region == ATH12K_DFS_REG_FCC) {
		cmd->ocac_mode = cpu_to_le32(WMI_ADFS_MODE_QUICK_RCAC);
		cmd->min_duration_ms = cpu_to_le32(MIN_RCAC_TIMEOUT);
		cmd->max_duration_ms = cpu_to_le32(MAX_RCAC_TIMEOUT);
	}

	cmd->chan_freq = cpu_to_le32(def->chan->center_freq);
	cmd->chan_width = cpu_to_le32(ath12k_wmi_get_host_chan_width(def->width));
	cmd->center_freq1 = cpu_to_le32(def->center_freq1);
	cmd->center_freq2 = cpu_to_le32(def->center_freq2);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "Send adfs channel cfg command for vdev id %d "
		   "mode as %d min duration %d chan_freq %d chan_width %d\n"
		   "center_freq1 %d center_freq2 %d", cmd->vdev_id,
		   cmd->ocac_mode, cmd->min_duration_ms, cmd->chan_freq,
		   cmd->chan_width, cmd->center_freq1, cmd->center_freq2);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_CH_CFG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_ADFS_CH_CFG_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_adfs_ocac_abort_cmd_send(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_adfs_ocac_abort_cmd *cmd;
	struct sk_buff *skb;
	int ret = 0;

	if (!ar->agile_chandef.chan) {
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "Currently, agile CAC is not active on any channel."
			   "Ignore abort");
		return ret;
	}

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_adfs_ocac_abort_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_ADFS_OCAC_ABORT_CMD,
						 sizeof(struct wmi_vdev_adfs_ocac_abort_cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_ADFS_OCAC_ABORT_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_ADFS_ABORT_CMD\n");
		dev_kfree_skb(skb);
		return ret;
	}
	return ret;
}

int ath12k_wmi_atf_send_group_config(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_atf_group_info *group;
	struct wmi_atf_group_info *group_info;
	struct wmi_atf_ssid_grp_request_fixed_param *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len, i;
	u32 pdev_id = ar->pdev->pdev_id;

	len = sizeof(*cmd) + TLV_HDR_SIZE + TLV_HDR_SIZE;
	len += ar->atf_table.total_groups * sizeof(*group_info);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = (struct wmi_atf_ssid_grp_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_SSID_GRP_REQUEST_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);

	/* Adding two tag array structs because the WMI command structure expects two array
	 * arguments. Since the first one is deprecated, it will be sent with a size of 0.
	 */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT, TLV_HDR_SIZE);
	ptr += TLV_HDR_SIZE;

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 ar->atf_table.total_groups *
					 sizeof(*group_info));
	ptr += TLV_HDR_SIZE;

	group_info = (struct wmi_atf_group_info *)ptr;
	for (i = 0; i < ar->atf_table.total_groups; i++) {
		group = &ar->atf_table.group_info[i];
		group_info->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_SSID_GROUPING_REQUEST_EVENT_V2,
					       sizeof(*group_info));
		group_info->atf_group_id = cpu_to_le32(group->group_id);
		group_info->atf_group_units = cpu_to_le32(group->group_airtime);
		group_info->atf_group_flags =
			le32_encode_bits(group->group_policy, WMI_ATF_GROUP_SCHED_POLICY);
		group_info->atf_total_num_peers =
			le32_encode_bits(group->unconfigured_peers,
					 WMI_ATF_GROUP_NUM_IMPLICIT_PEERS) |
			le32_encode_bits(group->configured_peers,
					 WMI_ATF_GROUP_NUM_EXPLICIT_PEERS);
		group_info->atf_total_implicit_peer_units =
			cpu_to_le32(group->unconfigured_peers_airtime);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "id: %d  airtime %d flags %d num: %d",
			   group_info->atf_group_id, group_info->atf_group_units,
			   group_info->atf_group_flags, group_info->atf_total_num_peers);

		group_info++;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF SSID group config for pdev id %u total groups %u\n",
		   ar->pdev->pdev_id, ar->atf_table.total_groups);
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_ATF_SSID_GROUPING_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_ATF_SSID_GROUPING_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_atf_send_peer_config(struct ath12k *ar,
				    struct ath12k_atf_peer_params *peer_param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_atf_request_fixed_param *cmd;
	struct ath12k_atf_peer_info *param_peer_info;
	struct wmi_atf_peer_info *peer_info;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
	void *ptr;
	int ret, len, i;

	len = sizeof(*cmd) + TLV_HDR_SIZE + TLV_HDR_SIZE;
	len += peer_param->num_peers * sizeof(*peer_info);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb) {
		ath12k_err(ar->ab, "failed to allocate skb");
		return -ENOMEM;
	}

	ptr = skb->data;

	cmd = (struct wmi_peer_atf_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ATF_REQUEST,
						 sizeof(*cmd));
	cmd->num_peers = cpu_to_le32(peer_param->num_peers);
	cmd->pdev_id = cpu_to_le32(peer_param->pdev_id);
	cmd->atf_flags = cpu_to_le32(peer_param->atf_flags);
	ptr += sizeof(*cmd);

	/* Adding two tag array structs because the WMI command structure expects two array
	 * arguments. Since the first one is deprecated, it will be sent with a size of 0.
	 */
	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT, TLV_HDR_SIZE);
	ptr += TLV_HDR_SIZE;

	tlv = (struct wmi_tlv *)ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
					 peer_param->num_peers *
					 sizeof(*peer_info));
	ptr += TLV_HDR_SIZE;

	param_peer_info = peer_param->peer_info;

	peer_info = (struct wmi_atf_peer_info *)ptr;
	for (i = 0; i < peer_param->num_peers; i++) {
		peer_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_PEER_REQUEST_EVENT_V2,
							       sizeof(*peer_info));
		memcpy(peer_info->peer_macaddr, param_peer_info->peer_macaddr, ETH_ALEN);
		peer_info->atf_peer_info =
			le32_encode_bits(param_peer_info->percentage_peer, WMI_ATF_PEER_AIRTIME) |
			le32_encode_bits(param_peer_info->group_index, WMI_ATF_PEER_GROUP_ID) |
			le32_encode_bits(param_peer_info->explicit_peer_flag,
					 WMI_ATF_PEER_CONFIGURED);
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "ATF: peer mac %pM percentage %u group_id %u explicit_peer_flag %u\n",
			   param_peer_info->peer_macaddr, param_peer_info->percentage_peer,
			   param_peer_info->group_index, param_peer_info->explicit_peer_flag);
		peer_info++;
		param_peer_info++;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "ATF: WMI ATF peer config for num_peers %u pdev id %u atf_flags %u peers %u\n",
		   peer_param->num_peers, peer_param->pdev_id,
		   peer_param->atf_flags, peer_param->num_peers);
	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_ATF_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_PEER_ATF_REQUEST_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_peer_delete_all(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_delete_all_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_delete_all_cmd *)skb->data;

	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DELETE_ALL_PEER_FIXED_PARAMS,
				       sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->peer_type_bitmap = 0;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_PEER, ATH12K_DBG_L0,
			 "WMI VDEV Peer delete all for vdev_id:%d", arvif->vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_ALL_PEER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit PEER DELETE ALL cmd vdev_id: %d\n",
			    arvif->vdev_id);
		dev_kfree_skb(skb);
	}

	return ret;
}

static long
ath12k_wmi_delete_all_peer_resp_pull(struct ath12k_base *ab,
				     struct sk_buff *skb,
				     struct wmi_delete_all_peer_resp_arg *arg)
{
	const void **tb;
	const struct wmi_delete_all_peer_resp_ev *ev;
	long ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %ld\n", ret);
		return ret;
	}

	ev = tb[WMI_VDEV_DELETE_ALL_PEER_RESP_EVENT_FIXED_PARAM];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer delete all resp ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = __le32_to_cpu(ev->vdev_id);
	arg->status = __le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

/**
 * ath12k_wmi_send_ltf_key_seed() - Program the LTF key seed into firmware.
 * @ar: ath12k radio.
 * @arg: keyseed parameters including vdev_id, peer MAC, RSN authmode,
 *	and the key seed bytes.
 *
 * Sends WMI_VDEV_SET_LTF_KEY_SEED_CMDID with the keyseed TLV appended.
 * Called after successful PASN authentication to enable secure LTF.
 *
 * Returns 0 on success or a negative errno on failure.
 */
int ath12k_wmi_send_ltf_key_seed(struct ath12k *ar,
				 struct ath12k_wmi_ltf_keyseed_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_wmi_vdev_set_ltf_key_seed_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int keyseed_len_aligned;
	int len, ret;

	keyseed_len_aligned = roundup(arg->keyseed_len, 4);
	len = sizeof(*cmd) + TLV_HDR_SIZE + keyseed_len_aligned;
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_LTF_KEY_SEED_CMD_FIXED_PARAM,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_mac);
	cmd->rsn_authmode = cpu_to_le32(arg->rsn_authmode);
	cmd->key_seed_len = cpu_to_le32(arg->keyseed_len);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, keyseed_len_aligned);
	memcpy(tlv->value, arg->keyseed, arg->keyseed_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_LTF_KEY_SEED_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send VDEV set LTF keyseed: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

/**
 * ath12k_wmi_send_rtt_pasn_deauth() - Send WMI_RTT_PASN_DEAUTH_CMD to firmware.
 * @ar: ath12k radio.
 * @peer_mac: MAC address of the PASN peer to deauthenticate.
 *
 * Returns 0 on success or a negative errno on failure.
 */
int ath12k_wmi_send_rtt_pasn_deauth(struct ath12k *ar, const u8 *peer_mac)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_wmi_rtt_pasn_deauth_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RTT_PASN_DEAUTH_CMD_FIXED_PARAM,
						 sizeof(*cmd));
	ether_addr_copy(cmd->peer_mac_addr.addr, peer_mac);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_RTT_PASN_DEAUTH_CMD);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send RTT PASN deauth: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

static int ath12k_wmi_rtt_pasn_tlv_iter(struct ath12k_base *ab, u16 tag,
					u16 len, const void *ptr,
					void *data)
{
	struct ath12k_wmi_rtt_pasn_tlv_parse *p = data;

	if (tag == p->fixed_tag) {
		if (len < p->fixed_min_len)
			return -EINVAL;
		p->fixed = ptr;
	} else if (tag == WMI_TAG_ARRAY_STRUCT) {
		p->peers = ptr;
		p->peers_len = len;
	}

	return 0;
}

/**
 * ath12k_wmi_rtt_pasn_peer_create_req_event() - Handle WMI PASN peer create request.
 * @ab: ath12k base device.
 * @skb: WMI event skb containing WMI_RTT_PASN_PEER_CREATE_REQ_EVENTID TLVs.
 *
 * Parses the fixed header and per-peer array, records each peer in the SW
 * tracking list, and queues pasn_fw_peer_create_work to create the FW peer
 * outside softirq context.
 */
void ath12k_wmi_rtt_pasn_peer_create_req_event(struct ath12k_base *ab,
					       struct sk_buff *skb)
{
	struct ath12k_wmi_rtt_pasn_tlv_parse parse = {
		.fixed_tag = WMI_TAG_RTT_PASN_PEER_CREATE_REQ_EVENT_FIXED_PARAM,
		.fixed_min_len = sizeof(struct ath12k_wmi_rtt_pasn_peer_create_req_event),
	};
	const struct ath12k_wmi_rtt_pasn_peer_create_req_param *peer;
	struct ath12k_link_vif *arvif;
	u32 vdev_id;
	u16 offset;
	int ret;

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN create event: skb->len=%u\n", skb->len);

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rtt_pasn_tlv_iter, &parse);
	if (ret || !parse.fixed || !parse.peers) {
		ath12k_warn(ab,
			    "failed to parse RTT PASN peer create event: ret=%d fixed=%p peers=%p\n",
			    ret, parse.fixed, parse.peers);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN create: peers_len=%u sizeof(peer)=%zu\n",
		   parse.peers_len, sizeof(*peer));

	if (parse.peers_len % sizeof(*peer)) {
		ath12k_warn(ab,
			    "invalid RTT PASN peer create array length %u (sizeof(peer)=%zu)\n",
			    parse.peers_len, sizeof(*peer));
		return;
	}

	const struct ath12k_wmi_rtt_pasn_peer_create_req_event *fixed_create =
		(const struct ath12k_wmi_rtt_pasn_peer_create_req_event *)parse.fixed;
	vdev_id = le32_to_cpu(fixed_create->vdev_id);

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN create: vdev_id=%u\n", vdev_id);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "invalid vdev id %u in RTT PASN peer create event\n",
			    vdev_id);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN create: arvif=%p ar=%p link_id=%u num_peers=%u\n",
		   arvif, arvif->ar, arvif->link_id, arvif->num_peers);

	for (offset = 0; offset < parse.peers_len; offset += sizeof(*peer)) {
		u32 control_flag;
		u8 security_mode;
		bool ltf_required;

		peer = (const void *)((const u8 *)parse.peers + offset);
		control_flag = le32_to_cpu(peer->control_flag);
		security_mode = control_flag & ATH12K_WMI_RTT_PASN_SECURITY_MODE_MASK;
		ltf_required = !!(control_flag &
				  ATH12K_WMI_RTT_PASN_LTF_KEYSEED_REQUIRED);

		ath12k_dbg(ab, ATH12K_DBG_RTT,
			   "RTT PASN create peer[%u]: self=%pM dest=%pM ctrl=0x%x sec=%u ltf=%d akm=%u cipher=%u\n",
			   offset / (u32)sizeof(*peer),
			   peer->self_mac_addr.addr, peer->dest_mac_addr.addr,
			   control_flag, security_mode, ltf_required,
			   le32_to_cpu(peer->akm), le32_to_cpu(peer->cipher_suite));

		ret = ath12k_pasn_peer_create_or_update(arvif,
							peer->self_mac_addr.addr,
							peer->dest_mac_addr.addr,
							ltf_required, security_mode);
		if (ret) {
			ath12k_warn(ab, "failed to track PASN peer %pM: %d\n",
				    peer->dest_mac_addr.addr, ret);
			continue;
		}

		/* FW peer creation requires wiphy_lock so queue a work item.
		 * The vendor auth event to hostapd is sent from within the work
		 * handler, AFTER FW confirms the peer — guaranteeing the peer
		 * exists in FW before PASN auth frames can arrive.
		 */
		wiphy_work_queue(ath12k_ar_to_hw(arvif->ar)->wiphy,
				 &arvif->rtt_ctx.pasn_fw_peer_create_work);

		ath12k_dbg(ab, ATH12K_DBG_RTT,
			   "RTT PASN create peer[%u]: FW peer create work queued for %pM vdev=%u\n",
			   offset / (u32)sizeof(*peer),
			   peer->dest_mac_addr.addr, arvif->vdev_id);
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN create event done: vdev_id=%u\n", vdev_id);
	rcu_read_unlock();
}

static void
ath12k_wmi_pasn_notify_delete(struct ath12k_link_vif *arvif,
			      const struct ath12k_wmi_rtt_pasn_peer_delete_param *peer,
			      u32 peer_idx)
{
	int ret;

	ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN delete peer[%u]: peer=%pM ctrl=0x%x\n",
		   peer_idx, peer->peer_mac_addr.addr,
		   le32_to_cpu(peer->control_flag));

	ret = ath12k_vendor_send_pasn_event(
			ath12k_ar_to_hw(arvif->ar)->wiphy,
			ieee80211_vif_to_wdev(arvif->ahvif->vif),
			arvif->link_id,
			QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT,
			NULL, peer->peer_mac_addr.addr,
			false);
	if (ret)
		ath12k_warn(arvif->ar->ab,
			    "failed to send PASN delete vendor event %pM: %d\n",
			    peer->peer_mac_addr.addr, ret);
	else
		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN delete peer[%u]: vendor event sent %pM\n",
			   peer_idx, peer->peer_mac_addr.addr);
}

/**
 * ath12k_wmi_rtt_pasn_peer_delete_event() - Handle WMI PASN peer delete event.
 * @ab: ath12k base device.
 * @skb: WMI event skb containing WMI_RTT_PASN_PEER_DELETE_EVENTID TLVs.
 *
 * Parses the event and sends a QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT
 * vendor event to hostapd for each peer so it can clean up the secure
 * ranging context.
 */
void ath12k_wmi_rtt_pasn_peer_delete_event(struct ath12k_base *ab,
					   struct sk_buff *skb)
{
	struct ath12k_wmi_rtt_pasn_tlv_parse parse = {
		.fixed_tag = WMI_TAG_RTT_PASN_PEER_DELETE_EVENT_FIXED_PARAM,
		.fixed_min_len = sizeof(struct ath12k_wmi_rtt_pasn_peer_delete_event),
	};
	const struct ath12k_wmi_rtt_pasn_peer_delete_param *peer;
	struct ath12k_link_vif *arvif;
	u32 vdev_id;
	u16 offset;
	int ret;

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN delete event: skb->len=%u\n", skb->len);

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rtt_pasn_tlv_iter, &parse);
	if (ret || !parse.fixed || !parse.peers) {
		ath12k_warn(ab,
			    "failed to parse RTT PASN peer delete event: ret=%d fixed=%p peers=%p\n",
			    ret, parse.fixed, parse.peers);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN delete: peers_len=%u sizeof(peer)=%zu\n",
		   parse.peers_len, sizeof(*peer));

	if (parse.peers_len % sizeof(*peer)) {
		ath12k_warn(ab,
			    "invalid RTT PASN peer delete array length %u (sizeof(peer)=%zu)\n",
			    parse.peers_len, sizeof(*peer));
		return;
	}

	const struct ath12k_wmi_rtt_pasn_peer_delete_event *fixed_del =
		(const struct ath12k_wmi_rtt_pasn_peer_delete_event *)parse.fixed;
	vdev_id = le32_to_cpu(fixed_del->vdev_id);

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN delete: vdev_id=%u\n", vdev_id);

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_id);
	if (!arvif) {
		rcu_read_unlock();
		ath12k_warn(ab, "invalid vdev id %u in RTT PASN peer delete event\n",
			    vdev_id);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN delete: arvif=%p ar=%p link_id=%u\n",
		   arvif, arvif->ar, arvif->link_id);

	for (offset = 0; offset < parse.peers_len; offset += sizeof(*peer)) {
		peer = (const void *)((const u8 *)parse.peers + offset);
		ath12k_wmi_pasn_notify_delete(arvif, peer,
					      offset / (u32)sizeof(*peer));
	}

	ath12k_dbg(ab, ATH12K_DBG_RTT,
		   "RTT PASN delete event done: vdev_id=%u\n", vdev_id);
	rcu_read_unlock();
}

int ath12k_wmi_pasn_peer_delete_all(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_delete_all_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_delete_all_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DELETE_ALL_PEER_FIXED_PARAMS,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arvif->vdev_id);
	cmd->peer_type_bitmap = cpu_to_le32(BIT(WMI_PEER_TYPE_PASN));

	ath12k_dbg(ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN delete_all: WMI vdev_id=%u", arvif->vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_ALL_PEER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI PASN peer delete all vdev_id=%u: %d",
			    arvif->vdev_id, ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

/**
 * ath12k_wmi_send_pasn_auth_status() - Send PASN authentication status to firmware.
 * @ar: ath12k radio.
 * @arg: authentication result including peer/source MAC, AKM, cipher, and
 *	optional comeback timeout.
 *
 * Builds and sends WMI_RTT_PASN_AUTH_STATUS_CMD with a single
 * WMI_TAG_RTT_PASN_AUTH_STATUS_PARAM TLV. Called after hostapd reports
 * the PASN authentication result via QCA_NL80211_VENDOR_SUBCMD_PASN.
 *
 * Returns 0 on success or a negative errno on failure.
 */
int
ath12k_wmi_send_pasn_auth_status(struct ath12k *ar,
				 const struct ath12k_wmi_rtt_pasn_auth_status_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_wmi_rtt_pasn_auth_status_cmd *cmd;
	struct ath12k_wmi_rtt_pasn_auth_status_param *param;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int len, ret;

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*param);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RTT_PASN_AUTH_STATUS_CMD_FIXED_PARAM,
				       sizeof(*cmd));

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, sizeof(*param));
	param = (void *)tlv->value;
	param->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RTT_PASN_AUTH_STATUS_PARAM,
				       sizeof(*param));
	ether_addr_copy(param->peer_mac_addr.addr, arg->peer_mac);
	param->status = cpu_to_le32(arg->status);
	ether_addr_copy(param->source_mac_addr.addr, arg->source_mac);
	param->akm = cpu_to_le32(arg->akm);
	param->cipher_suite = cpu_to_le32(arg->cipher);
	param->timeout_value = cpu_to_le32(arg->comeback_after);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_RTT_PASN_AUTH_STATUS_CMD);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send RTT PASN auth status: %d\n", ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

static void
ath12k_wmi_gpio_input_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const struct wmi_gpio_input_event *ev;
	const void **tb;
	u32 gpio_num, gpio_value;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse GPIO input event tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_GPIO_INPUT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch GPIO input event\n");
		kfree(tb);
		return;
	}

	gpio_num   = le32_to_cpu(ev->gpio_num);
	gpio_value = le32_to_cpu(ev->value);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "WMI GPIO input event pin %u value %u\n",
		   gpio_num, gpio_value);

	if (gpio_num < 32) {
		int i;
		struct ath12k *ar;

		for (i = 0; i < ab->num_radios; i++) {
			ar = &ab->pdevs[i].ar[0];
			ar = ab->pdevs[i].ar;
			if (!ar)
				continue;
			if (ar->radio_cfg.gpio_cfg[gpio_num].configured) {
				ar->radio_cfg.gpio_cfg[gpio_num].gpio_value = gpio_value;
				break;
			}
		}
	}

	kfree(tb);
}

static void
ath12k_wmi_delete_all_peer_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_delete_all_peer_resp_arg arg = {};
	struct ath12k_link_vif *arvif;

	if (ath12k_wmi_delete_all_peer_resp_pull(ab, skb, &arg)) {
		ath12k_warn(ab, "failed to parse vdev delete all peer response\n");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, arg.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev delete resp ev %d",
			    arg.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->delete_all_peer_done);

	arvif = ath12k_mac_get_arvif(ar, arg.vdev_id);
	if (arvif && arvif->num_ml_peers_del_all) {
		struct ath12k_hw_group *ag = ab->ag;

		ag->mlo_peer_count -= arvif->num_ml_peers_del_all;
		ath12k_dbg(ab, ATH12K_DBG_PEER | ATH12K_DBG_WSI_BYPASS,
			   "MLO peer del all resp vdev %u ar[%u] num_ml_peers_del_all=%u mlo_peer_count=%u\n",
			   arg.vdev_id, ar->radio_idx,
			   arvif->num_ml_peers_del_all, ag->mlo_peer_count);
		if (!ag->mlo_peer_count)
			complete(&ag->peer_cleanup_complete);

		arvif->num_ml_peers_del_all = 0;
	}

	rcu_read_unlock();

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "Delete all peer response status:%d for vdev:%d\n",
			 arg.status, arg.vdev_id);
}

int ath12k_wmi_vdev_rate_mask(struct ath12k *ar, struct wmi_vdev_ratemask_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_ratemask_cmd *cmd;
	struct sk_buff *skb;
	size_t len;
	int ret;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_ratemask_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CONFIG_RATEMASK, len);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->type = cpu_to_le32(arg->type);
	/* lower64 */
	cmd->mask_lower32 = cpu_to_le32(arg->mask_lower32);
	cmd->mask_higher32 = cpu_to_le32(arg->mask_higher32);
	/* higher64 */
	cmd->mask_lower32_2 = cpu_to_le32(arg->mask_lower32_2);
	cmd->mask_higher32_2 = cpu_to_le32(arg->mask_higher32_2);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi vdev rate mask vdev_id %d type %d lower32 0x%x lower32_2 0x%x higher32 0x%x higher32_2 0x%x\n",
		   arg->vdev_id, arg->type, arg->mask_lower32,
		   arg->mask_lower32_2, arg->mask_higher32,
		   arg->mask_higher32_2);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_RATEMASK_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send vdev %d rate mask cmd: %d\n",
			    arg->vdev_id, ret);
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

static int ath12k_vendor_secure_ranging_cipher_to_wmi(u32 cipher,
						      u32 *wmi_cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
		*wmi_cipher = WMI_CIPHER_AES_CCM;
		return 0;
	case WLAN_CIPHER_SUITE_TKIP:
		*wmi_cipher = WMI_CIPHER_TKIP;
		return 0;
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		*wmi_cipher = WMI_CIPHER_AES_GCM;
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

int ath12k_vendor_install_secure_ranging_tk(struct ath12k_link_vif *arvif,
					    struct nlattr **tb)
{
	struct wmi_vdev_install_key_arg arg = {};
	struct ath12k *ar = arvif->ar;
	u32 cipher;
	int ret;

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR])
		return -EINVAL;

	/* When hardware crypto is disabled the driver operates in pure SW
	 * encryption mode and does not program keys into firmware.  Skip TK
	 * installation and return success so the PASN flow completes normally;
	 * ranging will proceed without FW-level frame protection.
	 */
	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags))
		return 0;

	arg.key_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK]);
	if (!arg.key_len || arg.key_len > WMI_MAX_KEY_LEN)
		return -EINVAL;

	ret = ath12k_vendor_secure_ranging_cipher_to_wmi(
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER]),
			&cipher);
	if (ret)
		return ret;

	arg.vdev_id = arvif->vdev_id;
	arg.macaddr = nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]);
	arg.key_idx = 0;
	arg.key_flags = WMI_KEY_PAIRWISE;
	arg.key_cipher = cipher;
	arg.key_data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK]);

	reinit_completion(&ar->install_key_done);

	ret = ath12k_wmi_vdev_install_key(ar, &arg);
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&ar->install_key_done,
					 WMI_INSTALL_KEY_TIMEOUT_HZ))
		return -ETIMEDOUT;

	return ar->install_key_status ? -EINVAL : 0;
}

int ath12k_wmi_send_cumac_config(struct ath12k *ar,
				 u32 cumac_chip_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_send_cumac_cmd *cmd = NULL;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_send_cumac_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_CUMAC_CHIP,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->cumac_chip_id = cpu_to_le32(cumac_chip_id);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "wmi send cumac config cumac chip id:%d on pdev id %d\n",
		   cumac_chip_id, cmd->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_SET_CUMAC_CHIP_CMDID);

	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_SET_CUMAC_CHIP_CMDID");
		dev_kfree_skb(skb);
		return ret;
	}

	return 0;
}

int ath12k_wmi_vdev_tsf_tstamp_action_cmd(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_tsf_tstamp_action_cmd *cmd;
	struct sk_buff *skb;
	int len, ret;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_tsf_tstamp_action_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TSF_TSTAMP_ACTION_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tsf_action = cpu_to_le32(TSF_TSTAMP_QTIMER_CAPTURE_REQ);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI VDEV_TSF_TSTAMP_ACTION_CMD vdev_id %u action %u\n",
		   vdev_id, TSF_TSTAMP_QTIMER_CAPTURE_REQ);
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_TSF_TSTAMP_ACTION_CMDID: %d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_aggr_size_cmd(struct ath12k *ar,
				  struct set_custom_aggr_size_params *params)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_set_custom_aggr_size_params_cmd *cmd = NULL;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (void *)skb->data;
	cmd->tlv_header = FIELD_PREP(WMI_TLV_TAG,
				     WMI_TAG_VDEV_SET_CUSTOM_AGGR_SIZE_CMD) |
		FIELD_PREP(WMI_TLV_LEN, len - TLV_HDR_SIZE);

	cmd->vdev_id = params->vdev_id;
	cmd->tx_aggr_size = params->tx_aggr_size;
	cmd->rx_aggr_size = params->rx_aggr_size;
	cmd->enable_bitmap = FIELD_PREP(WMI_VDEV_AGGR_AC, params->ac) |
		FIELD_PREP(WMI_VDEV_AGGR_TYPE, params->aggr_type) |
		FIELD_PREP(WMI_VDEV_TX_AGGR_SZ_DISABLE,
			   params->tx_aggr_size_disable) |
		FIELD_PREP(WMI_VDEV_RX_AGGR_SZ_DISABLE,
			   params->rx_aggr_size_disable) |
		FIELD_PREP(WMI_VDEV_AGGR_AC_ENABLE,
			   params->tx_ac_enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set aggr size vdev_id %d tx_aggr %d rx_aggr %d ac %d aggr_type %d tx_aggr_size_disable %d rx_aggr_size_disable %d tx_ac_enable %d",
		   params->vdev_id, params->tx_aggr_size, params->rx_aggr_size,
		   params->ac, params->aggr_type, params->tx_aggr_size_disable,
		   params->rx_aggr_size_disable, params->tx_ac_enable);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID);

	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* pn_rx_filter is a bitmask of IEEE80211_FILTER_TYPE_* values.
 * Host passes the requested management-frame filter mask to FW,
 * and FW stores it per vdev as vdev_pn_rx_filter.
 */
int ath12k_wmi_vdev_set_pn_mgmt_rx_filter_cmd(struct ath12k *ar, u32 vdev_id,
					      u32 pn_rx_filter)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_pn_mgmt_rx_filter_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_pn_mgmt_rx_filter_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_PN_MGMT_RX_FILTER_CMD, len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->pn_rx_filter = cpu_to_le32(pn_rx_filter);
	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev id 0x%x set pn_rx_filter 0x%x\n",
		   vdev_id, pn_rx_filter);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_PN_MGMT_RX_FILTER_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_PN_MGMT_RX_FILTER_CMDID\n");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_multi_vdev_set_param(struct ath12k *ar,
				    const struct ath12k_mbssid_info *mbssid_info,
				    u32 param_id, u32 param_value)
{
	struct wmi_pdev_multiple_vdev_set_param_cmd *cmd;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	unsigned long vdev_id;
	size_t vdev_ids_len;
	__le32 *vdev_ids;
	u32 i = 1;
	int len, ret;

	if (ar->ab->is_bypassed) {
		ath12k_warn(ar->ab, "Chip is bypassed, skip multi vdev wmi set cmd");
		return 0;
	}

	vdev_ids_len = (mbssid_info->nontx_cnt + 1) * sizeof(*vdev_ids);
	len = sizeof(*cmd) + TLV_HDR_SIZE + vdev_ids_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_multiple_vdev_set_param_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_MULTIPLE_VDEV_SET_PARAM_CMD,
				       sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, vdev_ids_len);
	vdev_ids = (__le32 *)tlv->value;

	/* Copy Tx BSS's vdev id */
	vdev_ids[0] = cpu_to_le32(mbssid_info->tx_vdev_id);

	/* Copy Non-Tx BSS's vdev id */
	for_each_set_bit(vdev_id, mbssid_info->nontx_vdev_bmap, ATH12K_MAX_NUM_VDEVS)
		vdev_ids[i++] = cpu_to_le32(vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_MULTIPLE_VDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_err(ar->ab,
			   "failed to send WMI_PDEV_MULTIPLE_VDEV_SET_PARAM_CMDID");
		dev_kfree_skb(skb);
	}

	ath12k_info(ar->ab,
		    "WMI multi vdev set param pdev id %u tx vdev id %u param %u value %u",
		    ar->pdev->pdev_id, mbssid_info->tx_vdev_id,
		    param_id, param_value);
	return ret;
}

int ath12k_wmi_send_peer_uhr_omp_cmd(struct ath12k *ar, u32 sw_peer_id,
				     u32 pdev_id,
				     const struct ath12k_wmi_uhr_omp_link_params *links,
				     u8 num_links)
{
	struct wmi_peer_uhr_omp_npca_params *npca;
	struct wmi_peer_uhr_omp_dso_params *dso;
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_uhr_omp_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	bool has_dso_update;
	bool has_npca_update;
	void *ptr;
	int ret, len, i;

	if (!num_links)
		return -EINVAL;

	for (i = 0, has_npca_update = false; i < num_links; i++) {
		if (links[i].npca_update) {
			has_npca_update = true;
			break;
		}
	}

	for (i = 0, has_dso_update = false; i < num_links; i++) {
		if (links[i].dso_mode_update) {
			has_dso_update = true;
			break;
		}
	}

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE; /* NPCA: dummy or real */
	if (has_npca_update)
		len += num_links * sizeof(*npca);
	len += TLV_HDR_SIZE; /* dummy DPS */
	if (has_dso_update)
		len += TLV_HDR_SIZE + num_links * sizeof(*dso);
	else
		len += TLV_HDR_SIZE; /* dummy DSO */

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

	cmd = ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_UHR_OMP_CMD,
						 sizeof(*cmd));
	cmd->sw_peer_id = cpu_to_le32(sw_peer_id);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);

	tlv = ptr;
	if (has_npca_update) {
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 num_links * sizeof(*npca));
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_links; i++) {
			npca = ptr;
			npca->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_UHR_OMP_NPCA_PARAMS,
						       sizeof(*npca));
			npca->omp_npca_caps =
				le32_encode_bits(links[i].hw_link_id,
						 WMI_PEER_UHR_OMP_NPCA_CAPS_HW_LINK_ID) |
				(links[i].npca_enable ?
				 le32_encode_bits(1,
						  WMI_PEER_UHR_OMP_NPCA_CAPS_ENABLE) :
				 0) |
				(links[i].npca_mode_update ?
				 le32_encode_bits(1,
						  WMI_PEER_UHR_OMP_NPCA_CAPS_MODE_UPDATE
						 ) : 0);
			npca->omp_npca_param =
				le32_encode_bits(links[i].npca_switch_delay,
					WMI_PEER_UHR_OMP_NPCA_PARAM_SWITCH_DELAY) |
				le32_encode_bits(links[i].npca_switch_back_delay,
					WMI_PEER_UHR_OMP_NPCA_PARAM_SWITCH_BACK_DELAY);
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "WMI UHR OMP cmd sw_peer_id %u pdev_id %u hw_link_id %u npca_enable %d switch_delay %u switch_back_delay %u mode_update %d\n",
				   sw_peer_id, pdev_id, links[i].hw_link_id,
				   links[i].npca_enable, links[i].npca_switch_delay,
				   links[i].npca_switch_back_delay,
				   links[i].npca_mode_update);
			ptr += sizeof(*npca);
		}
	} else {
		/* dummy NPCA TLV - required by TLV ordering even when not used */
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	/* dummy DPS TLV - required by TLV ordering even when not used */
	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
	ptr += TLV_HDR_SIZE;

	if (has_dso_update) {
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT,
						 num_links * sizeof(*dso));
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_links; i++) {
			dso = ptr;
			dso->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_UHR_OMP_DSO_PARAMS,
						       sizeof(*dso));
			dso->omp_dso_caps =
				le32_encode_bits(links[i].hw_link_id,
						 WMI_PEER_UHR_OMP_DSO_CAPS_HW_LINK_ID) |
				(links[i].dso_enable ?
				 le32_encode_bits(1,
						  WMI_PEER_UHR_OMP_DSO_CAPS_ENABLE) : 0) |
				(links[i].dso_mode_update ?
				 le32_encode_bits(1,
						  WMI_PEER_UHR_OMP_DSO_CAPS_MODE_UPDATE
						 ) : 0);
			dso->omp_dso_param =
				le32_encode_bits(links[i].dso_padding_delay,
					WMI_PEER_UHR_OMP_DSO_PARAM_PADDING_DELAY) |
				le32_encode_bits(links[i].dso_switch_back_delay,
					WMI_PEER_UHR_OMP_DSO_PARAM_SWITCH_BACK_DELAY) |
				le32_encode_bits(links[i].dso_subband,
						 WMI_PEER_UHR_OMP_DSO_PARAM_SUBBAND);
			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "WMI UHR OMP cmd sw_peer_id %u pdev_id %u hw_link_id %u dso_enable %d mode_update %d subband %u padding_delay %u switch_back_delay %u\n",
				   sw_peer_id, pdev_id, links[i].hw_link_id,
				   links[i].dso_enable, links[i].dso_mode_update,
				   links[i].dso_subband, links[i].dso_padding_delay,
				   links[i].dso_switch_back_delay);
			ptr += sizeof(*dso);
		}
	} else {
		/* dummy DSO TLV - required by TLV ordering even when not used */
		tlv = ptr;
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);
		ptr += TLV_HDR_SIZE;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_UHR_OMP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_UHR_OMP_CMDID: %d\n", ret);
		dev_kfree_skb(skb);
	}
	return ret;
}
