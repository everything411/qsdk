// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <net/netlink.h>
#include <net/mac80211.h>
#include "core.h"
#include "debug.h"
#include <net/genetlink.h>
#include <net/cfg80211.h>
#include "../net/wireless/core.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ath12k_cmn_extn.h"
#include "qcn_extns/vendor_extn.h"
#include "qcn_extns/me_hmmc_extn.h"
#include "qcn_extns/me_extn.h"
#include "qcn_extns/me_snoop_extn.h"
#include "qcn_extns/ranging_extn.h"
#endif /* CPTCFG_QCN_EXTN */
#include "mac.h"
#include "ppe.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "debugfs_htt_stats.h"
#include "spectral.h"
#include "telemetry.h"
#include "telemetry_agent_if.h"
#include "erp.h"
#include "vendor_services.h"
#include "dp_peer.h"
#include "dp_mon.h"
#include "dp_rx.h"
#include "me.h"
#include "peer.h"
#include <linux/vmalloc.h>
#include "ranging.h"

/* Forward declarations for functions defined later in this file that are
 * called before their definition. In QCN builds these are declared via
 * qcn_extns/ath12k_cmn_extn.h; in noextns builds they are static.
 */
#ifndef CPTCFG_QCN_EXTN
static struct ath12k_vif *ath12k_get_ahvif_from_wdev(struct wireless_dev *wdev);
static struct ath12k *ath12k_get_ar_from_wdev(struct wireless_dev *wdev, u8 link_id);
#endif /* !CPTCFG_QCN_EXTN */

static const struct nla_policy
ath12k_wifi_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA] = {.type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_6GHZ_VLP_PRIORITY_THRESH_FREQ] = {.type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_AGILE_CAPABLE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_VLAN] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_ESP_PARAMS] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_ALLOW_3ADDR_MC] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_VHT_MCS_10_11_PEER_SUPP] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_HE_CAP_INFO_INTERNAL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_PEER_MAC] = { .type = NLA_BINARY, .len = ETH_ALEN },
};

static const struct nla_policy
ath12k_cfg80211_afc_response_policy[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA] = { .type = NLA_BINARY,
						 .len = QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE },
};

static const struct nla_policy
ath12k_atf_offload_config_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG] = {.type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG] = {.type = NLA_BINARY,
							    .len = ATF_OFFLOAD_MAX_PAYLOAD},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_atf_grouping_param_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]  = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_vendor_atf_offload_peer_config_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_atf_peer_param_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC] = {.type = NLA_BINARY,
						       .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED] = {.type = NLA_FLAG},
};

static const struct nla_policy
ath12k_vendor_atf_offload_sched_duration_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_AC] =  {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION] = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_vendor_atf_offload_ssid_sched_policy[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_pri_link_migrate_policy[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR] = {.type = NLA_BINARY,
							     .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID] = {.type =  NLA_U8},
};

/* NLA policy for repurpose link command */
static const struct nla_policy
ath12k_repurpose_link_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = { .type = NLA_U8 },
};

static const struct nla_policy
ath12k_vendor_get_sta_info_policy[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC] = {.type = NLA_BINARY, .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_PROBE_REQ_BMISS_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_PROBE_RESP_BMISS_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_ALL_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RTS_RETRY_FAIL_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_NON_AGGREGATED_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_DATA_AGGREGATED_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_GOOD_PLCP_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_INVALID_DELIMITER_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_ACKS_GOOD_FCS_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BLOCKACK_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BEACON_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_OTHER_BEACON_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_UCAST_DATA_GOOD_FCS_COUNT] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_DATA_BC_MC_DROP_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_MCS0] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_MCS0] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_HW_BUFFERS_OVERFLOW_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_TX_POWER] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_IES] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PROBE_RESP_IES] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_DRIVER_DISCONNECT_REASON] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_TX_PACKETS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PER_MCS_RX_PACKETS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PAD] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_UPLINK_DELAY_JITTER] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_NSS_PKT_COUNT] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MCS_PKT_COUNT] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BW_PKT_COUNT] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CCA_STAT] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MISS_STAT] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LINK_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_RSSI] = {.type = NLA_S8},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MIN_RSSI] = {.type = NLA_S8},
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PS_STATE] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_me_config_policy[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_PARAM] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_VALUE] = { .type = NLA_U32 },
};

static const struct nla_policy
ath12k_vendor_igmp_tid_override_policy[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_RADIO_ID] = { .type = NLA_U8  },
	[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_VALUE]    = { .type = NLA_U32 },
};

static const struct nla_policy
ath12k_vendor_ext_mon_pkt_config_filter_policy[
QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_vendor_ext_mon_pkt_config_len_policy[
QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_ext_mon_pkt_config_policy[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_DATA_MPDU_TLV] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_ext_mon_data_mpdu_tlv_policy[
	QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MCAST] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_UCAST] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_NULL]  = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_ext_mon_filter_config_policy[
	QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MON_FLAG] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_ext_mon_snr_info_policy[QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_SNR] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AVG_SNR] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_TSTAMP] = {.type = NLA_U64},
};

static const struct nla_policy
ath12k_vendor_ext_mon_peer_info_policy[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR] = {.type = NLA_BINARY,
							.len = ETH_ALEN},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_SNR_INFO] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_RA_ADDR_MASK] = {.type = NLA_BINARY,
							     .len = ETH_ALEN},
};

static const struct nla_policy
ath12k_vendor_ext_mon_peer_config_policy[QCA_VENDOR_ATTR_EXT_MON_PEER_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_PEER_ACTION] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_ext_mon_policy[QCA_VENDOR_ATTR_EXT_MON_MAX + 1] = {
	[QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_DIRECTION] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_STATUS_CODE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_oem_data_policy[QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA] = { .type = NLA_BINARY,
					    .len = QCA_VENDOR_WLAN_OEM_DATA_BUF_MAX_SIZE},
	[QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO]            = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_TYPE]          = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_LINK_ID]           = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_IFINDEX]           = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_RADIO_IDX]         = { .type = NLA_U8 },
};

/* Inner (level-2) policy: validates each {PCP, TID} entry nested inside TABLE */
static const struct nla_policy
ath12k_pcp_tid_entry_policy[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID] = { .type = NLA_U8 },
};

/* Outer (level-1) policy: TABLE is a nested array of pcp_tid_entry attrs */
static const struct nla_policy
ath12k_pcp_tid_map_policy[QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE] = { .type = NLA_NESTED },
};

static const struct nla_policy
ath12k_tid_map_prty_policy[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL]     = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_TID_DEF] = { .type = NLA_U8 },
};

static const struct nla_policy
ath12k_vendor_me_dump_policy[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_OPERATION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_TYPE]      = { .type = NLA_U8 },
};

static const struct nla_policy
ath12k_multi_bss_param_policy[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_IFINDEX] = {.type = NLA_U32,},
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_LINKID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_CONFIGS] = { .type = NLA_NESTED },
};

static const struct nla_policy
ath12k_multi_bss_config_policy[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_1] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_2] = { .type = NLA_U32 },
};

static const struct nla_policy
ath12k_vendor_green_ap_policy[QCA_WLAN_VENDOR_ATTR_GREEN_AP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_ENABLE_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_GET_GREEN_AP_ENABLE_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_PS_TIMEOUT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_GET_GREEN_AP_PS_TIMEOUT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_GREEN_AP_LINK_ID] = { .type = NLA_U8 },
};

/**
 * ath12k_vendor_cmd_skip_check() - Check if vendor cmd shall be allowed to process
 *
 * Converts a wiphy pointer and an optional radio index (from
 * QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX) into the corresponding
 * struct ath12k pointer.
 *
 * @wiphy:     wiphy associated with the vendor command
 * @radio_idx: radio index parsed from NL attrs
 *
 * Return: 1 for bypass 0 for not bypass.
 */
static int ath12k_vendor_cmd_skip_check(struct wiphy *wiphy, struct wireless_dev *wdev,
					struct ath12k_wifi_generic_params wifi_params,
					int cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	int link_id, radio_idx;

	if (cmd == QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION ||
	    cmd == QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION) {
		radio_idx = (wifi_params.radio_idx == INVALID_RADIO_INDEX) ?
				0 : wifi_params.radio_idx;
		ar = ath12k_ah_to_ar(ah, radio_idx);
	} else if (cmd == QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION ||
		   cmd == QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION) {
		link_id = (wifi_params.link_id == INVALID_LINK_ID) ?
				0 : wifi_params.link_id;
		ar = ath12k_get_ar_from_wdev(wdev, link_id);
	}

	if (ar && ar->ab->is_bypassed &&
	    wifi_params.value != QCA_WLAN_VENDOR_RADIO_PARAM_WSI_BYPASS) {
		ath12k_info(ar->ab, "vendor cmd (%d) rejected: device is in bypassed state\n",
			    wifi_params.value);
		return 1;
	}

	return 0;
}

static int ath12k_vendor_send_multi_bss_vdev_param_wmi_cmd(struct ath12k_link_vif *arvif,
							   u32 param_id, u32 param_value)

{
	u32 val = 0, wmi_param_id = 0;
	int ret = 0;

	switch (param_id) {
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_BEACON_INT:
		wmi_param_id =  WMI_VDEV_PARAM_BEACON_INTERVAL;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_MU_BFMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMEE:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SOUNDING_DIM:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_BFMEE_STS:
		ret = ath12k_mac_set_vht_txbf_conf(arvif, &val);
		if (ret) {
			ath12k_err(arvif->ar->ab,
				   "failed to get vht txbf conf value for param:%u",
				   param_id);
			return ret;
		}
		wmi_param_id = WMI_VDEV_PARAM_TXBF;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_80:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_160:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_320:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMEE:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_DL_MU_OFDMA:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_DL_MU_OFDMA_BFER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_UL_MU_OFDMA:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MU_BEAMFORMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_UL_MUMIMO:
		ret = ath12k_mac_set_he_txbf_conf(arvif, &val, true);
		if (ret) {
			ath12k_err(arvif->ar->ab,
				   "failed to get he txbf conf value for param:%u",
				   param_id);
			return ret;
		}
		wmi_param_id = WMI_VDEV_PARAM_SET_HEMU_MODE;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_RTS_THRESHOLD:
		wmi_param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMEE:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_MU_BFMER:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_DL_MU_OFDMA:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_UL_MU_OFDMA:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_DL_OFDMA_MUMIMO:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_UL_OFDMA_MUMIMO:
		ret = ath12k_mac_set_eht_txbf_conf(arvif, &val, true);
		if (ret) {
			ath12k_err(arvif->ar->ab,
				   "failed to get eht txbf conf value for param:%u",
				   param_id);
			return ret;
		}
		wmi_param_id = WMI_VDEV_PARAM_SET_EHT_MU_MODE;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_LTF:
		wmi_param_id = WMI_VDEV_PARAM_EHT_LTF;
		break;
	default:
		ath12k_err(arvif->ar->ab, "param_id:%u not found", param_id);
		return -EINVAL;
	}

	ret = ath12k_wmi_multi_vdev_set_param(arvif->ar, arvif->mbssid_info,
					      wmi_param_id, val ? val : param_value);
	if (ret) {
		ath12k_info(arvif->ar->ab,
			    "failed multi vdev wmi for param_id:%u vdev_id:%u ret:%d",
			    param_id, arvif->vdev_id, ret);
	}

	return ret;
}

static int ath12k_vendor_apply_cmn_param_to_vdevs(struct ath12k_link_vif *arvif,
						  u32 param_id, u32 val)
{
	struct ieee80211_bss_conf *bss_conf =
		ath12k_mac_get_link_bss_conf(arvif);

	if (!bss_conf) {
		ath12k_err(arvif->ar->ab, "unable to access bss link conf");
		return -EINVAL;
	}

	switch (param_id) {
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_BEACON_INT:
		bss_conf->beacon_int = val;
		arvif->beacon_interval = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_MU_BFMER:
		bss_conf->vht_mu_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMER:
		bss_conf->vht_su_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_VHT_SU_BFMEE:
		bss_conf->vht_su_beamformee = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMER:
		bss_conf->he_su_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_SU_BFMEE:
		bss_conf->he_su_beamformee = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_MU_BEAMFORMER:
		bss_conf->he_mu_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_UL_MUMIMO:
		bss_conf->he_full_ul_mumimo = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_HE_RTS_THRESHOLD:
		bss_conf->frame_time_rts_th = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMER:
		bss_conf->eht_su_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_SU_BFMEE:
		bss_conf->eht_su_beamformee = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_MU_BFMER:
		bss_conf->eht_mu_beamformer = val;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_80:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_160:
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_EHT_BFME_SS_320:
		/*
		 * Setting these parameters to non-zero in userspace enables
		 * EHT SU beamformee (SU_BFMEE)
		 */
		if (val > 0)
			bss_conf->eht_su_beamformee = 1;
		break;
	case QCA_WLAN_VENDOR_MULTI_BSS_PARAM_ID_ENABLE_MCS15:
		bss_conf->enable_mcs15 = val;
		break;
	}

	return 0;
}

static int ath12k_vendor_set_cmn_param(struct ath12k_link_vif *arvif,
				       struct ath12k_vif *ahvif,
				       u32 param_id, u32 value)
{
	struct ath12k_mbssid_info *mbssid_info = arvif->mbssid_info;
	struct ieee80211_bss_conf *bss_conf = NULL;
	struct ath12k_link_vif *tmp_arvif;
	struct ath12k *ar = arvif->ar;
	int ret;

	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return -EINVAL;
	}

	if (!mbssid_info) {
		ath12k_err(ar->ab, "mbssid_info NULL");
		return -EINVAL;
	}

	bss_conf = ath12k_mac_get_link_bss_conf(arvif);
	if (!bss_conf || !bss_conf->mbssid_tx_vif) {
		ath12k_err(ar->ab, "mbssid_tx_vif  NULL");
		return -EINVAL;
	}

	if (bss_conf->nontransmitted) {
		ath12k_err(ar->ab, "Non-Tx BSS not allowed");
		return -EINVAL;
	}

	/* Set common param for Non-tx BSS */
	list_for_each_entry(tmp_arvif, &ar->arvifs, list) {
		if (!test_bit(tmp_arvif->vdev_id,
			      mbssid_info->nontx_vdev_bmap))
			continue;

		ret = ath12k_vendor_apply_cmn_param_to_vdevs(tmp_arvif,
							     param_id,
							     value);
		if (ret) {
			ath12k_err(ar->ab,
				   "Failed to apply cmn_param for Non-Tx vdev:%u",
				   tmp_arvif->vdev_id);
			return ret;
		}
	}

	/* set common param for Tx BSS */
	ret = ath12k_vendor_apply_cmn_param_to_vdevs(arvif, param_id, value);
	if (ret) {
		ath12k_err(ar->ab,
			   "Failed to apply cmn_param for Tx vdev:%u",
			   arvif->vdev_id);
		return ret;
	}

	ret = ath12k_vendor_send_multi_bss_vdev_param_wmi_cmd(arvif,
							      param_id,
							      value);
	return ret;
}

static int ath12k_vendor_set_multi_bss_param(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_MAX + 1];
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct nlattr *param_attr;
	u32 ref_bss_ifindex;
	u8 ref_bss_link_id = 0;
	int rem, ret;

	if (!wdev || !wdev->netdev || !data || !data_len) {
		ath12k_err(NULL, "multi_bss: wdev/netdev/data is NULL");
		return -EINVAL;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "multi_bss: vif is NULL");
		return -EINVAL;
	}

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "multi_bss: This command is applicable only for AP mode");
		return -EOPNOTSUPP;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	arvif = &ahvif->deflink;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_MAX, data,
			data_len, ath12k_multi_bss_param_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse multi BSS params: %d", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_IFINDEX] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_CONFIGS]) {
		ath12k_err(NULL,
			   "mandatory multi BSS param attribute missing");
		return -EINVAL;
	}

	ref_bss_ifindex =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_IFINDEX]);

	if (ieee80211_vif_is_mld(vif) &&
	    tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_LINKID]) {
		ref_bss_link_id =
			nla_get_u8(tb
				[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_REF_BSS_LINKID]);
		if (ref_bss_link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
			ath12k_err(NULL, "invalid ref link id %u", ref_bss_link_id);
			return -EINVAL;
		}
		arvif = wiphy_dereference(wiphy, ahvif->link[ref_bss_link_id]);
	}

	if (!arvif || !arvif->is_started) {
		ath12k_err(NULL, "multi_bss: arvif is null or not started");
		return -EINVAL;
	}

	nla_for_each_nested(param_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_CONFIGS],
			    rem) {
		struct nlattr *param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_MAX + 1];
		u32 param_id, param_val_1, param_val_2;

		ret = nla_parse_nested(param,
				       QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAMS_INFO_MAX,
				       param_attr,
				       ath12k_multi_bss_config_policy,
				       NULL);
		if (ret) {
			ath12k_err(arvif->ar->ab,
				   "failed to parse multi BSS param config: %d",
				   ret);
			return ret;
		}

		if (!param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_ID] ||
		    !param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_1]) {
			ath12k_err(arvif->ar->ab,
				   "mandatory multi BSS param config missing");
			return -EINVAL;
		}

		param_id =
			nla_get_u32(param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_ID]);
		param_val_1 =
			nla_get_u32(param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_1]);

		if (param[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_2]) {
			param_val_2 =
				nla_get_u32(param
					[QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_VAL_2]);
			ath12k_info(arvif->ar->ab,
				    "multi BSS param ref_ifindex %u ref_link_id %u param_id %u param_val_1 %u param_val_2 %u",
				    ref_bss_ifindex, ref_bss_link_id, param_id,
				    param_val_1, param_val_2);
		} else {
			ath12k_info(arvif->ar->ab,
				    "multi BSS param ref_ifindex %u ref_link_id %u param_id %u param_val_1 %u",
				    ref_bss_ifindex, ref_bss_link_id, param_id,
				    param_val_1);
		}

		ret = ath12k_vendor_set_cmn_param(arvif, ahvif, param_id,
						  param_val_1);
		if (ret) {
			ath12k_err(arvif->ar->ab,
				   "Failed to set param_id:%u and param_val:%u for multi bss",
				   param_id, param_val_1);
			return ret;
		}
	}

	return ret;
}

static const struct nla_policy
ath12k_rx_pkt_protocol_tag_policy[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_OP_CODE]    = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_PROTO_TYPE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_TAG_VALUE]  = {.type = NLA_U16},
};

static const struct nla_policy
ath12k_rx_flow_tag_op_policy[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_OP_CODE]  = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_IP_VER]   = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV4] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV4] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV6] = NLA_POLICY_EXACT_LEN(16),
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV6] = NLA_POLICY_EXACT_LEN(16),
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_PORT] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_PORT] = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_PROTO]    = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_TAG]      = {.type = NLA_U16},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DROP]     = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_RING_ID]  = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_fse_cce_stats_policy[QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_PDEV_ID] = {.type = NLA_U32},
};

static int ath12k_dp_rx_update_pdev_protocol_tag(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data, int data_len);

static int ath12k_dp_rx_flow_tag_op(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len);

static int ath12k_dp_rx_fse_cce_stats_dump(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int data_len);

/**
 * ath12k_vendor_repurpose_link() - Mark an MLO link for repurposing
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: vendor command data
 * @data_len: vendor command data length
 *
 * This vendor command marks a specific MLO link as being repurposed.
 * The link ID is extracted from the vendor attributes and the corresponding
 * bit is set in the repurpose_links_bmap across ath12k_vif, ieee80211_vif,
 * and wireless_dev structures.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ath12k_vendor_repurpose_link(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	u8 link_id;
	int ret;

	if (!vif)
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL,
			   "Repurpose not supported on vif type: %d",
			   vif->type);
		return -EOPNOTSUPP;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "ath12k VIF is NULL");
		return -EINVAL;
	}

	/* Parse vendor attributes */
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_repurpose_link_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse vendor attributes: %d", ret);
		return ret;
	}

	/* Check if link ID attribute is present */
	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]) {
		ath12k_err(NULL, "Link ID attribute missing");
		return -EINVAL;
	}

	link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);

	/* Validate link ID range */
	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		ath12k_err(NULL, "Invalid link ID %u (max %u)",
			   link_id, IEEE80211_MLD_MAX_NUM_LINKS - 1);
		return -EINVAL;
	}

	/* Do not allow repurpose setting on active link */
	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif) {
		ath12k_err(NULL, "No arvif found for the repurposing link");
		return -EINVAL;
	}
	if (arvif->is_started) {
		ath12k_err(NULL, "Cannot repurpose active link ID %u",
			   link_id);
		return -EBUSY;
	}

	/* Check if link is already marked for repurposing */
	if (ahvif->repurposed_links & BIT(link_id)) {
		ath12k_err(NULL, "Link ID %u already marked for repurposing",
			   link_id);
		return 0;
	}

	/* Mark link for repurposing in ieee80211_vif via mac80211 helper */
	ret = ieee80211_set_repurpose_link(vif, link_id);
	if (ret) {
		ath12k_err(NULL,
			   "Failed to set repurpose link in mac80211: %d",
			   ret);
		return ret;
	}

	/* Mark link for repurposing in ath12k_vif */
	ahvif->repurposed_links |= BIT(link_id);

	ath12k_dbg(NULL, ATH12K_DBG_MAC,
		   "Link ID %u marked for repurposing (bmap: 0x%x)",
		   link_id, ahvif->repurposed_links);

	return ret;
}

#ifdef CPTCFG_ATH12K_CFR
static const struct nla_policy
ath12k_peer_cfr_policy[QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR] =
		NLA_POLICY_EXACT_LEN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK] = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA] = NLA_POLICY_EXACT_LEN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA] = NLA_POLICY_EXACT_LEN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK] = NLA_POLICY_EXACT_LEN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK] = NLA_POLICY_EXACT_LEN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_RESP_DATA] = { .type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREQ] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SUBTYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_REPORT_INTERVAL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_OUI] = { .type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_FORMAT_VERSION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_TIMESTAMP_US] = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_INDEX] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_RSSI] = { .type = NLA_S8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_RX_ANTENNA_AGC] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_IS_LAST_REPORT] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FRAME_SEQUENCE_NUMBER] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CHIP_ID] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TSF] = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CFO] = { .type = NLA_S16 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CSI_LTF_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_NUM_SPATIAL_STREAMS] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_STOP_REASON] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_TARA_FILTER_AS_FP] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_COUNT] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_INTERVALMODE_SEL] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_LINK_ID] =
		NLA_POLICY_RANGE(NLA_U8, 0, IEEE80211_MLD_MAX_NUM_LINKS - 1),
};

/* CFR helper function to fetch sta, link_sta, ahsta & arsta based on mac address &
 * link id. must be called with rcu_read_lock()
 */
static inline int
ath12k_peer_periodic_cfr_get_sta_details(struct wireless_dev *wdev,
					 struct ieee80211_sta **sta,
					 struct ieee80211_vif *vif,
					 struct ieee80211_link_sta **link_sta,
					 struct ath12k_hw *ah,
					 struct ath12k_sta **ahsta,
					 struct ath12k_link_sta **arsta,
					 struct ath12k *ar,
					 u8 *cfr_peer_mac,
					 unsigned int link_id)
{
	unsigned int local_link_id = link_id;

	if (!sta || !link_sta || !ahsta || !arsta)
		return -EINVAL;

	*sta = ieee80211_find_sta(vif, cfr_peer_mac);
	if (*sta)
		goto ath_sta;

	/* search as peer link mac when valid link id provided */
	if (link_id == INVALID_LINK_ID) {
		ath12k_err(ar->ab, "STA not found");
		return -EINVAL;
	}
	*sta = ieee80211_find_sta_by_link_addrs(ah->hw, cfr_peer_mac,
						NULL, &local_link_id);
	if (!(*sta) || local_link_id != link_id) {
		ath12k_err(ar->ab, "STA not found");
		return -EINVAL;
	}

ath_sta:
	*ahsta = ath12k_sta_to_ahsta(*sta);
	if (!(*ahsta)) {
		ath12k_err(ar->ab, "failed to get ahsta for peer %pM", cfr_peer_mac);
		return -EINVAL;
	}

	if (!(*sta)->mlo)
		*arsta = wiphy_dereference(ah->hw->wiphy,
					   (*ahsta)->link[(*ahsta)->deflink.link_id]);
	else if (BIT(link_id) & (*ahsta)->links_map)
		*arsta = wiphy_dereference(ah->hw->wiphy, (*ahsta)->link[link_id]);

	if (!(*arsta)) {
		ath12k_err(ar->ab, "failed to get arsta for peer %pM link_id %u",
			   cfr_peer_mac, link_id);
		return -ENOENT;
	}

	if (!(*sta)->mlo)
		*link_sta = &(*sta)->deflink;
	else
		*link_sta = ath12k_mac_get_link_sta(*arsta);

	if (!(*link_sta)) {
		ath12k_err(ar->ab, "failed to get link sta for peer %pM link_id %u",
			   cfr_peer_mac, link_id);
		return -ENOENT;
	}

	return 0;
}

static int
ath12k_peer_periodic_cfr_configure(struct wireless_dev *wdev,
				   struct ieee80211_vif *vif,
				   struct ath12k_hw *ah,
				   struct ath12k_link_vif *arvif,
				   struct ath12k *ar,
				   struct nlattr **tb,
				   unsigned int link_id)
{
	struct ieee80211_sta *sta = NULL;
	struct ieee80211_link_sta *link_sta = NULL;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_link_sta *arsta = NULL;
	struct wmi_peer_cfr_capture_conf_arg arg = { 0 };
	u8 cfr_peer_mac[ETH_ALEN] = { 0 };
	u8 link_sta_mac[ETH_ALEN] = { 0 };
	u32 cfr_capture_enable = 0, cfr_capture_bw = 0;
	u32 cfr_capture_method = 0, cfr_capture_period = 0;
	u32 vdev_id;
	int ret;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR])
		return -EINVAL;

	ether_addr_copy(cfr_peer_mac,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR]));

	cfr_capture_enable = !!tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE];

	if (cfr_capture_enable) {
		/* If request is to enable periodic CFR - BW, periodicity &
		 * capture method are mandatory parameters.
		 */
		if (!tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH] ||
		    !tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY] ||
		    !tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD])
			return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH])
		cfr_capture_bw =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY])
		cfr_capture_period =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY]);

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD])
		cfr_capture_method =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD]);

	if (!ar->cfr.cfr_enabled) {
		ath12k_err(ar->ab, "cfr_enabled is false, can't enable periodic CFR");
		return -EINVAL;
	}

	rcu_read_lock();
	/* fetch sta details */
	ret = ath12k_peer_periodic_cfr_get_sta_details(wdev, &sta, vif,
						       &link_sta, ah, &ahsta,
						       &arsta, ar,
						       cfr_peer_mac, link_id);
	if (ret) {
		rcu_read_unlock();
		return ret;
	}
	if (!arsta->arvif || !arsta->arvif->ar ||
	    arsta->arvif != arvif ||
	    arsta->arvif->ar != ar) {
		ath12k_err(ar->ab,
			   "failed to get arsta for peer %pM link_id %u",
			   cfr_peer_mac, link_id);
		rcu_read_unlock();
		return -ENOENT;
	}

	ret = ath12k_cfr_peer_capture_validate(ar, arsta, link_sta->bandwidth,
					       &cfr_capture_enable,
					       &cfr_capture_bw,
					       &cfr_capture_period,
					       &cfr_capture_method);

	if (ret == -EALREADY) {
		rcu_read_unlock();
		return 0;
	}

	if (ret) {
		rcu_read_unlock();
		return ret;
	}

	/* copy link sta mac, bandwidth and vdev_id to use later to send
	 * wmi command
	 */
	vdev_id = arsta->arvif->vdev_id;
	ether_addr_copy(link_sta_mac, link_sta->addr);
	rcu_read_unlock();

	ath12k_cfr_peer_capture_fill_wmi_arg(&arg, cfr_capture_enable,
					     cfr_capture_bw,
					     cfr_capture_period,
					     cfr_capture_method);

	ret = ath12k_wmi_peer_set_cfr_capture_conf(ar, vdev_id,
						   link_sta_mac, &arg);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed peer CFR capture cfg vdev_id %u peer %pM: %d",
			    vdev_id, link_sta_mac, ret);
		return ret;
	}

	rcu_read_lock();
	ret = ath12k_peer_periodic_cfr_get_sta_details(wdev, &sta, vif,
						       &link_sta, ah,
						       &ahsta, &arsta,
						       ar, cfr_peer_mac,
						       link_id);
	if (ret) {
		ath12k_err(ar->ab, "Failed to refetch peer details post CFR WMI");
		rcu_read_unlock();
		return ret;
	}
	if (!arsta->arvif || !arsta->arvif->ar ||
	    arsta->arvif != arvif ||
	    arsta->arvif->ar != ar) {
		ath12k_err(ar->ab,
			   "failed to get arsta for peer %pM link_id %u when refetch",
			   cfr_peer_mac, link_id);
		rcu_read_unlock();
		return -ENOENT;
	}

	ath12k_cfr_peer_capture_update(ar, arsta, cfr_capture_enable,
				       cfr_capture_bw, cfr_capture_period,
				       cfr_capture_method);

	rcu_read_unlock();

	return 0;
}

static inline int
ath12k_vendor_set_cfr_enabled(struct ath12k *ar, bool enable)
{
	int ret = ath12k_wmi_pdev_set_param(ar,
					    WMI_PDEV_PARAM_PER_PEER_CFR_ENABLE,
					    enable,
					    ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab,
			   "Failed to enable/disable per peer cfr (%d)", ret);
		return ret;
	}

	ar->cfr.cfr_enabled = enable;
	return ret;
}

static int ath12k_vendor_peer_cfr_handler(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data,
					  int data_len)
{
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif;
	struct ath12k_hw *ah = NULL;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1];
	int ret;
	unsigned int version = 0;
	unsigned int link_id = INVALID_LINK_ID;

	lockdep_assert_wiphy(wiphy);

	if (!wdev || !data || !data_len) {
		ath12k_err(NULL, "invalid input to peer CFR handler\n");
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX, data,
			data_len, ath12k_peer_cfr_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse peer CFR vendor attributes\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION])  {
		ath12k_err(NULL, "CFR version not present");
		return -EINVAL;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "failed to get vif from wdev\n");
		return -EINVAL;
	}

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "failed to get ahvif from vif\n");
		return -EINVAL;
	}

	ah = ahvif->ah;
	if (ah->state != ATH12K_HW_STATE_ON)
		return -ENETDOWN;

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_LINK_ID]);

	/* Do not consider the link id if passed on non-mld vif */
	if (link_id != INVALID_LINK_ID && !ieee80211_vif_is_mld(vif))
		link_id = INVALID_LINK_ID;

	/* link id is mandatory on mld interface */
	if (link_id == INVALID_LINK_ID && ieee80211_vif_is_mld(vif)) {
		ath12k_err(NULL, "Link id is mandatory on MLD interface");
		return -EINVAL;
	}

	/* reject if vif does not have the link id requested */
	if (link_id != INVALID_LINK_ID && !(vif->valid_links & BIT(link_id))) {
		ath12k_err(NULL, "Invalid link on the VIF");
		return -EINVAL;
	}

	if (link_id == INVALID_LINK_ID)
		arvif = &ahvif->deflink;
	else
		arvif = wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[link_id]);

	if (!arvif || !arvif->ar) {
		ath12k_err(NULL, "NULL arvif/ar");
		return -EINVAL;
	}

	ar = arvif->ar;

	version = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION]);

	if (version == 1) {
		/* NL command received to disable timer at target */
		if (!tb[QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE]) {
			if (!ar->cfr.cfr_enabled)
				return 0;
			return ath12k_vendor_set_cfr_enabled(ar, false);
		}

		/* Enable the timer at target if not enabled earlier */
		if (!ar->cfr.cfr_enabled) {
			ret = ath12k_vendor_set_cfr_enabled(ar, true);
			if (ret)
				return ret;
		}

		/* handle if any peer needs periodic cfr to be enabled/disabled */
		if (tb[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR])
			return ath12k_peer_periodic_cfr_configure(wdev, vif, ah,
								  arvif, ar,
								  tb, link_id);

		return 0;
	}

	return -EOPNOTSUPP;
}
#endif

static void
ath12k_afc_response_buffer_display(struct ath12k_base *ab,
				   struct ath12k_afc_host_resp *afc_rsp)
{
	struct ath12k_afc_bin_resp_data *afc_bin = NULL;
	struct ath12k_afc_resp_freq_psd_info *freq_obj = NULL;
	struct ath12k_afc_resp_opclass_info *opclass_obj = NULL;
	struct ath12k_afc_resp_eirp_info *eirp_obj = NULL;
	u8 *tmp_ptr = NULL;
	int iter, iter_j;

	/* Display the AFC Response Fixed parameters */
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         AFC Response Fixed params\n"
		   "---------------------------\n");
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\nTLV header: %u\nStatus: %u\nTTL: %u\n"
		   "Length: %u\nResponse format: %u\n"
		   "---------------------------\n",
		   afc_rsp->header,
		   afc_rsp->status,
		   afc_rsp->time_to_live,
		   afc_rsp->length,
		   afc_rsp->resp_format);

	afc_bin = (struct ath12k_afc_bin_resp_data *)&afc_rsp->afc_resp[0];

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         Binary fixed\n"
		   "---------------------------\n");
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\nLocal Error code: %u\nVersion: 0x%x\n"
		   "AFC Version: 0x%x\nRequest ID: %u\nDate: 0x%x\n"
		   "Time: 0x%x\nServer resp: %u\n"
		   "Freq objs: %u\nOpclass objs: %u\n"
		   "---------------------------\n",
		   afc_bin->local_err_code,
		   afc_bin->version,
		   afc_bin->afc_wfa_version,
		   afc_bin->request_id,
		   afc_bin->avail_exp_time_d,
		   afc_bin->avail_exp_time_t,
		   afc_bin->afc_serv_resp_code,
		   afc_bin->num_frequency_obj,
		   afc_bin->num_channel_obj);

	/* Display Frequency/PSD info from AFC Response */
	freq_obj = (struct ath12k_afc_resp_freq_psd_info *)
		((u8 *)afc_bin +
		 sizeof(struct ath12k_afc_bin_resp_data));
	tmp_ptr = (u8 *)freq_obj;

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "         Freq Info\n"
		   "---------------------------\n");
	for (iter = 0; iter < afc_bin->num_frequency_obj; iter++) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Freq Info[%d]: 0x%x\nMax PSD[%d]: %u\n",
			   iter, freq_obj->freq_info, iter, freq_obj->max_psd);
		freq_obj = (struct ath12k_afc_resp_freq_psd_info *)
			((u8 *)freq_obj +
			 sizeof(struct ath12k_afc_resp_freq_psd_info));
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n");

	/* Display Opclass and channel EIRP info from AFC Response */
	opclass_obj = (struct ath12k_afc_resp_opclass_info *)
		((u8 *)tmp_ptr +
		 (afc_bin->num_frequency_obj *
		  sizeof(struct ath12k_afc_resp_freq_psd_info)));
	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n"
		   "      Opclass Info\n"
		   "---------------------------\n");
	for (iter = 0; iter < afc_bin->num_channel_obj; iter++) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "\nOpclass[%d]: %u\nNum channels[%d]: %u\n",
			   iter, opclass_obj->opclass, iter, opclass_obj->num_channels);

		eirp_obj = (struct ath12k_afc_resp_eirp_info *)
			((u8 *)opclass_obj +
			 sizeof(struct ath12k_afc_resp_opclass_info));
		for (iter_j = 0; iter_j < opclass_obj->num_channels; iter_j++) {
			ath12k_dbg(ab, ATH12K_DBG_AFC,
				   "\nChannel Info[%d]:\nCFI: %u\nEIRP: %u\n\n",
				   iter_j, eirp_obj->channel_cfi, eirp_obj->max_eirp_pwr);
			eirp_obj = (struct ath12k_afc_resp_eirp_info *)
				((u8 *)eirp_obj +
				 sizeof(struct ath12k_afc_resp_eirp_info));
		}

		opclass_obj = (struct ath12k_afc_resp_opclass_info *)
				((u8 *)opclass_obj +
				 sizeof(struct ath12k_afc_resp_opclass_info) +
				 (opclass_obj->num_channels *
				  sizeof(struct ath12k_afc_resp_eirp_info)));
	}

	ath12k_dbg(ab, ATH12K_DBG_AFC,
		   "\n---------------------------\n");
}

static struct ath12k_afc_host_resp *ath12k_extract_afc_resp(struct ath12k_base *ab,
							    struct nlattr **attr,
							    int *afc_resp_len)
{
	struct ath12k_afc_resp_opclass_info *start_opcls = NULL, *opclass_list = NULL;
	struct nlattr *frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX + 1];
	struct nlattr *opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX + 1];
	struct nlattr *chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX + 1];
	struct ath12k_afc_bin_resp_data *afc_fixed_params = NULL;
	struct ath12k_afc_resp_freq_psd_info *frange_obj = NULL;
	struct ath12k_afc_resp_eirp_info *chan_obj = NULL;
	u16 start_freq = 0, end_freq = 0, nl_len = 0;
	struct ath12k_afc_host_resp *afc_rsp = NULL;
	u32 num_frange_obj = 0, num_channels = 0;
	struct nlattr *nl, *nl_attr;
	u32 total_channels = 0;
	u8 num_opclas_obj = 0;
	int rem, iter, i;
	u8 *temp;

	/* Calculate the total number of Frequency range objects received in the
	 * AFC response
	 */
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO],
				    rem) {
			nl_len = nla_len(nl);
			if (nl_len < QCA_WLAN_AFC_RESP_FREQ_PSD_INFO_INFO_MIN_LEN) {
				ath12k_dbg(ab, ATH12K_DBG_AFC,
					   "Insufficient length %d for Frequency PSD info",
					   nl_len);
				goto fail;
			}
			num_frange_obj++;
		}
	}

	/* Calculate the total number of opclass objects and corresponding number
	 * of channels in each opclass object received in the AFC response
	 */
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO],
				    rem) {
			num_channels = 0;
			nl_len = nla_len(nl);
			if (nl_len < QCA_WLAN_AFC_RESP_OPCLASS_CHAN_EIRP_INFO_MIN_LEN) {
				ath12k_dbg(ab, ATH12K_DBG_AFC,
					   "Insufficient length %d for Opclass/Channel EIRP info",
					   nl_len);
				goto fail;
			}

			if (nla_parse(opclass_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			nla_for_each_nested(nl_attr,
					    opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST],
					    iter) {
				num_channels++;
			}

			num_opclas_obj++;
			total_channels += num_channels;
		}
	}

	/* Calculate the total length required for AFC response
	 * buffer allocation.
	 */
	*afc_resp_len = (sizeof(struct ath12k_afc_host_resp) +
			 sizeof(struct ath12k_afc_bin_resp_data) +
			 (num_frange_obj * sizeof(struct ath12k_afc_resp_freq_psd_info)) +
			 (num_opclas_obj * sizeof(struct ath12k_afc_resp_opclass_info)) +
			 (total_channels * sizeof(struct ath12k_afc_resp_eirp_info)));

	afc_rsp = kzalloc(*afc_resp_len, GFP_KERNEL);

	if (!afc_rsp) {
		ath12k_dbg(ab, ATH12K_DBG_AFC,
			   "Error allocating buffer for AFC response");
		goto fail;
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]) {
		afc_rsp->time_to_live =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]);
	}

	/* Update the AFC fixed parameters from the AFC response */
	afc_fixed_params = (struct ath12k_afc_bin_resp_data *)afc_rsp->afc_resp;

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]) {
		afc_fixed_params->request_id =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]) {
		afc_fixed_params->avail_exp_time_d =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]) {
		afc_fixed_params->avail_exp_time_t =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]);
	}

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]) {
		afc_fixed_params->afc_serv_resp_code =
			nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]);
	}

	/* Update the number of frequency range objects and opclass objects
	 * to the AFC response structure.
	 */
	afc_fixed_params->num_frequency_obj = num_frange_obj;
	afc_fixed_params->num_channel_obj = num_opclas_obj;

	/* Start parsing and updating the frequency range list */
	temp = (u8 *)afc_fixed_params;
	frange_obj =
	(struct ath12k_afc_resp_freq_psd_info *)(temp +
						 sizeof(struct ath12k_afc_bin_resp_data));

	if (!frange_obj)
		goto fail;

	i = 0;
	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO]) {
		nla_for_each_nested(nl, attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO],
				    rem) {
			if (nla_parse(frange_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START]) {
				start_freq =
				nla_get_u16(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START]);
			}

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END]) {
				end_freq =
				nla_get_u16(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END]);
			}
			frange_obj[i].freq_info = ((start_freq) | (end_freq << 16));

			if (frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD]) {
				frange_obj[i].max_psd =
				nla_get_u32(frange_info[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD]);
			}

			i++;
		}
	}

	/* Start parsing and updating the opclass list and corresponding channel
	 * and EIRP power information.
	 */
	temp = (u8 *)frange_obj;
	start_opcls = (struct ath12k_afc_resp_opclass_info *)
			(temp + sizeof(struct ath12k_afc_resp_freq_psd_info) *
			 num_frange_obj);
	opclass_list = start_opcls;

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO]) {
		nla_for_each_nested(nl,
				    attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO],
				    rem) {
			if (nla_parse(opclass_info,
				      QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX,
				      nla_data(nl),
				      nla_len(nl),
				      NULL, NULL)) {
				goto fail;
			}

			if (opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS]) {
				opclass_list->opclass =
				nla_get_u8(opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS]);
			}

			temp = (u8 *)opclass_list;
			chan_obj = (struct ath12k_afc_resp_eirp_info *)
					(temp + sizeof(struct ath12k_afc_resp_opclass_info));
			i = 0;
			nla_for_each_nested(nl_attr,
					    opclass_info[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST],
					    iter) {
				if (nla_parse(chan_info,
					      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX,
					      nla_data(nl_attr),
					      nla_len(nl_attr),
					      NULL, NULL)) {
					goto fail;
				}

				if (chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM]) {
					chan_obj[i].channel_cfi =
					nla_get_u8(chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM]);
				}

				if (chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP]) {
					chan_obj[i].max_eirp_pwr =
					nla_get_s32(chan_info[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP]);
				}

				i++;
			}

			opclass_list->num_channels = i;
			temp = (u8 *)chan_obj;
			opclass_list = (struct ath12k_afc_resp_opclass_info *)
					(temp + (sizeof(struct ath12k_afc_resp_eirp_info) *
						 opclass_list->num_channels));
		}
	}

	return afc_rsp;

fail:
	ath12k_dbg(ab, ATH12K_DBG_AFC, "Error parsing the AFC response from application");

	if (!afc_rsp)
		kfree(afc_rsp);

	return NULL;
}

static int ath12k_vendor_receive_afc_response(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1];
	struct ath12k_afc_host_resp *afc_rsp = NULL;
	int afc_resp_len = 0;
	enum qca_nl_afc_resp_type afc_resp_format;
	int ret = 0, hw_idx = -1;
	u8 i;

	ath12k_dbg(NULL, ATH12K_DBG_AFC, "Received AFC response event\n");

	if (!(data && data_len)) {
		ath12k_dbg(NULL, ATH12K_DBG_AFC,
			   "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX, data, data_len,
		      ath12k_cfg80211_afc_response_policy, NULL)) {
		ath12k_dbg(NULL, ATH12K_DBG_AFC,
			    "invalid set afc config policy attribute\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX]) {
		hw_idx = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX]);
		if (hw_idx >= ah->num_radio) {
			ath12k_dbg(NULL, ATH12K_DBG_AFC, "Invalid hw_idx attribute\n");
			ret = -EINVAL;
			goto out;
		}

		ar = &ah->radio[hw_idx];
	} else {
		ar = ah->radio;
		for (i = 0; i < ah->num_radio; i++, ar++)
			if (ar->supports_6ghz)
				break;
	}

	if (!ar) {
		ath12k_err(NULL, "ar is NULL \n");
		ret = -ENODATA;
		goto out;
	}

	afc_resp_format = QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP;
	switch (afc_resp_format) {
	case QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP:
		if (tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA]) {
			/* Extract total AFC response buffer length */
			afc_resp_len =
				nla_len(tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA]);

			if (afc_resp_len) {
				/* Memory allocation done to store AFC response
				 * sent by AFC application
				 */
				afc_rsp = kzalloc(afc_resp_len, GFP_KERNEL);
			} else {
				ath12k_warn(ar->ab,
					    "AFC JSON data is not present!");
				ret = -EINVAL;
				goto out;
			}

			/* Extract the AFC response buffer */
			if (afc_rsp) {
				nla_memcpy((void *)afc_rsp,
					   tb[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA],
					   afc_resp_len);
			} else {
				ath12k_warn(ar->ab,
					    "Response buffer allocation failed");
				ret = -EINVAL;
				goto out;
			}

		} else {
			ath12k_warn(ar->ab,
				    "AFC JSON data not found");
			ret = -EINVAL;
			goto out;
		}
		break;

	case QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP:
		/* The AFC response received from the user space application
		 * is expected to be packed in network byte order(Big endian).
		 * Since q6 is little endian, Host needs to convert the afc
		 * response to little endian format.
		 *
		 * Note: This conversion of data to little endian format is only
		 *       required for Binary type data. For raw JSON data,
		 *       no conversion is required since it is text string.
		 *
		 * Since all the members of the AFC response structure are defined
		 * to be 32-bit words, convert the length appropriately for
		 * conversion to little endian format.
		 */
		afc_rsp = ath12k_extract_afc_resp(ar->ab, tb, &afc_resp_len);

		if (!afc_rsp) {
			ret = -EINVAL;
			goto out;
		}

		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC response extraction successful!\n");

		ath12k_afc_response_buffer_display(ar->ab, afc_rsp);

		break;

	default:
		ath12k_warn(ar->ab, "Invalid response format type %d\n",
			    afc_resp_format);
		ret  = -EINVAL;
		goto exit;
	}

	/* Copy the data buffer to AFC memory location */
	ret = ath12k_copy_afc_response(ar, (char *)afc_rsp, afc_resp_len);
	if (ret)
		goto exit;

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC response copied to AFC memory\n");

	ret = ath12k_wmi_send_afc_cmd_tlv(ar, afc_resp_format,
					  WMI_AFC_CMD_SERV_RESP_READY);
	if (ret) {
		ath12k_warn(ar->ab,
			    "AFC Rx indication to FW failed: %d\n", ret);
		goto exit;
	}
	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC Resp RX indication sent to target\n");

exit:
	kfree(afc_rsp);
out:
	return ret;
}

#define nla_nest_end_checked(skb, start) do {           \
	if ((skb) && (start))                           \
		nla_nest_end(skb, start);               \
} while (0)

/**
 * ath12k_afc_power_event_update_or_get_len() - Function to fill vendor event
 * buffer  with AFC power update event or get required vendor buffer length
 * @vendor_event: Pointer to vendor event SK buffer
 * @pwr_evt: Pointer to AFC power event
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event, 0 if success, otherwise negative error code
 */
static int
ath12k_afc_power_event_update_or_get_len(struct ath12k *ar,
					 struct sk_buff *vendor_event,
					 struct ath12k_afc_sp_reg_info *pwr_evt)
{
	struct ath12k_afc_chan_obj *pow_evt_chan_info = NULL;
	struct ath12k_chan_eirp_obj *pow_evt_eirp_info = NULL;
	struct nlattr *nla_attr = NULL;
	struct nlattr *freq_info;
	struct nlattr *opclass_info = NULL;
	struct nlattr *chan_list = NULL;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;
	u8 hw_idx;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC power update complete event type put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			    pwr_evt->resp_id)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
		nla_put_u8(vendor_event,
			   QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE,
			   pwr_evt->fw_status_code)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT STATUS CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
		nla_put_s32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE,
			    pwr_evt->serv_resp_code)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT SERVER RESP CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(s32));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE,
			    pwr_evt->avail_exp_time_d)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT EXPIRE DATE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
		nla_put_u32(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME,
			    pwr_evt->avail_exp_time_t)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "AFC EVENT EXPIRE TIME put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event) {
		/* Update the Frequency and corresponding PSD info */
		nla_attr =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < pwr_evt->num_freq_objs; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
			(nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				     pwr_evt->afc_freq_info[i].low_freq) ||
			 nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				     pwr_evt->afc_freq_info[i].high_freq) ||
			 nla_put_u32(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD,
				     pwr_evt->afc_freq_info[i].max_psd))) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
				   "AFC FREQUENCY PSD INFO put failed, num %d",
				   pwr_evt->num_freq_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 3;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class, channel list and EIRP info */
		nla_attr =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	pow_evt_chan_info = pwr_evt->afc_chan_info;

	for (i = 0; i < pwr_evt->num_chan_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
			nla_put_u8(vendor_event,
				   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
				   pow_evt_chan_info[i].global_opclass)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
				   "AFC OPCLASS INFO put fail, num %d",
				   pwr_evt->num_chan_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list =
			nla_nest_start(vendor_event,
				       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		pow_evt_eirp_info = pow_evt_chan_info[i].chan_eirp_info;

		for (j = 0; j < pow_evt_chan_info[i].num_chans; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info)
					goto fail;
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    (nla_put_u8(vendor_event,
					QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
					pow_evt_eirp_info[j].cfi) ||
			     nla_put_u32(vendor_event,
				         QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP,
					 pow_evt_eirp_info[j].eirp_power))) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
					   "AFC CHAN EIRP_INFO put fail, num %d",
					   pow_evt_chan_info[i].num_chans);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
				len += nla_total_size(sizeof(u32));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}

	nla_nest_end_checked(vendor_event, nla_attr);

	hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
					     ar->freq_range.start_freq);
	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX, hw_idx)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "hw_idx put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

/**
 * ath12k_afc_expiry_event_update_or_get_len() - Function to fill vendor evt buffer
 * with info extracted from AFC request, or get required vendor buffer length.
 * @ar - Pointer to ath12k structure
 * @vendor_event: Pointer to vendor event SK buffer structure
 * @afc_req: Pointer to AFC request from regulatory component
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event  0 if success, otherwise negative error code
 */
static int
ath12k_afc_expiry_event_update_or_get_len(struct ath12k *ar,
					  struct sk_buff *vendor_event,
					  struct ath12k_afc_host_request *afc_req)
{
	struct nlattr *nla_attr = NULL;
	struct nlattr *freq_info;
	struct nlattr *opclass_info = NULL;
	struct nlattr *chan_list = NULL;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;
	struct ath12k_afc_opclass_obj *afc_opclass_obj;
	u8 hw_idx;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			afc_req->req_id)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_ATTR_AFC_REQ_ID put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION,
			(afc_req->version_major << 16) |
			afc_req->version_minor)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EVENT WFA version put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER,
			afc_req->min_des_power)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "QCA_WLAN_VENDOR_ATTR_AFC_REQ_MIN_DES_PWR put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u16));
	}

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT,
		       afc_req->afc_location->deployment_type)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EVENT AP deployment put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event) {
		/* Update the frequency range list from the Expiry event */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC FREQ RANGE LIST start put fail\n");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->freq_lst->num_ranges; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put freq list nest %d\n",
					   i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    (nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				 afc_req->freq_lst->range_objs[i].lowfreq) ||
		     nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				 afc_req->freq_lst->range_objs[i].highfreq))) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC REQ FREQ RANGE LIST put fail, num %d\n",
				   afc_req->freq_lst->num_ranges);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 2;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class and channel list */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS CHAN LIST start put fail\n");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->opclass_obj_lst->num_opclass_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put opclass nest %d\n",
					   i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		afc_opclass_obj = &afc_req->opclass_obj_lst->opclass_objs[i];

		if (vendor_event &&
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
			       afc_opclass_obj->opclass)) {
			ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS INFO OPCLASS put fail, num %d\n",
				   afc_req->opclass_obj_lst->num_opclass_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list = nla_nest_start(vendor_event,
						   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC OPCLASS INFO CHAN LIST start put fail\n");
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		for (j = 0; j < afc_opclass_obj->opclass_num_cfis; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info) {
					ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "Fail to put opclass cfis nest %d\n",
						   j);
					goto fail;
				}
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    nla_put_u8(vendor_event,
				       QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
				       afc_opclass_obj->cfis[j])) {
				ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC EIRP INFO CHAN NUM put fail, num %d\n",
					   afc_opclass_obj->opclass_num_cfis);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
					     ar->freq_range.start_freq);
	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX, hw_idx)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "hw_idx put fail\n");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

/**
 * afc_payload_reset_evt_get_data_len: Get the AFC payload resent event data
 * length.
 *
 * Return: Data length.
 */
static int afc_payload_reset_evt_get_data_len(void)
{
	u32 len = NLMSG_HDRLEN;

	/* Size reserved for event type and HW index */
	len += nla_total_size(sizeof(u8)) + nla_total_size(sizeof(u32));
	len = nla_total_size(len);

	return len;
}

int ath12k_send_afc_payload_reset(struct ath12k *ar)
{
	struct sk_buff *vendor_event;
	int ret = -EINVAL;
	int vendor_buffer_len, hw_index;
	struct ath12k_base *ab = ar->ab;

	hw_index = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
					       ar->freq_range.start_freq);
	if (hw_index == -1) {
		ath12k_err(ab, "Failed to get hw index for freq %d\n",
			   ar->freq_range.start_freq);
		goto out;
	}

	vendor_buffer_len = afc_payload_reset_evt_get_data_len();
	vendor_event = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
						   NULL,
						   vendor_buffer_len,
						   QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
						   GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "failed to allocate skb for afc expiry event\n");
		goto out;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET)) {
		ath12k_warn(ar->ab, "AFC payload reset complete event type put fail");
		goto out;
	}
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX,
			hw_index)) {
		ath12k_warn(ar->ab, "AFC payload reset complete event hw index put fail");
		goto out;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending afc payload reset to higher layer of type %d, hw_index: %d\n",
		   QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT, hw_index);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	ret = 0;

out:
	return ret;
}

int ath12k_send_afc_request(struct ath12k *ar, struct ath12k_afc_host_request *afc_req)
{
	struct sk_buff *vendor_event;
	int vendor_buffer_len;
	int ret = 0;

	if (!afc_req) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC, "AFC Host request is NULL\n");
		return -EINVAL;
	}

	vendor_buffer_len = ath12k_afc_expiry_event_update_or_get_len(ar, NULL, afc_req);
	vendor_event = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy,
						   NULL,
						   vendor_buffer_len,
						   QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
						   GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "failed to allocate skb for afc expiry event\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = ath12k_afc_expiry_event_update_or_get_len(ar, vendor_event, afc_req);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to update AFC request vendor event\n");
		ret = -EINVAL;
		goto out;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending expiry event to higher layer of type %d\n",
		   QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
out:
	return ret;
}

static const struct nla_policy
ath12k_vendor_rm_generic_policy[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_TTLM_MAPPING] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_PMLO] = {.type = NLA_STRING},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_LINK_BW_NSS_CHANGE] = {
								 .type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_DETSCHED] = {
								 .type = NLA_STRING},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_TTLM_INFO] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_wlan_telemetry_req_policy[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE] = {.type = NLA_NESTED},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC] = {.type = NLA_BINARY,
							.len = ETH_ALEN},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID] = {.type = NLA_U64},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID] = {.type = NLA_U8},
	[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_wlan_telemetry_feat_policy[QCA_VENDOR_ATTR_WLAN_FEAT_MAX + 1] = {
	[QCA_VENDOR_ATTR_WLAN_FEAT_TX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_RX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_PROTO] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_TID] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_DELAY] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_JITTER] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_SOJOURN] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_MON_STATS] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_TX_MON_STATS] = {.type = NLA_FLAG},
	[QCA_VENDOR_ATTR_WLAN_FEAT_MAC80211] = {.type = NLA_FLAG},
};

int ath12k_extract_feat_inputs(struct nlattr *tb_attr,
			       struct ath12k_telemetry_command *cmd)
{
	struct nlattr *feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_MAX + 1] = {0};
	int ret = 0;

	memset(&cmd->feat, 0, sizeof(struct ath12k_stats_feat));

	ret = nla_parse_nested(feat_attr, QCA_VENDOR_ATTR_WLAN_FEAT_MAX, tb_attr,
			       ath12k_wlan_telemetry_feat_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "nla parse failure: Feature input\n");
		return ret;
	}

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_TX])
		cmd->feat.feat_tx = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_RX])
		cmd->feat.feat_rx = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_PROTO])
		cmd->feat.feat_proto = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_TID])
		cmd->feat.feat_tid = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_MON_STATS])
		cmd->feat.feat_mon_stats = true;

	if (cmd->svc_id != INVALID_SVC_ID &&
	    feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX])
		cmd->feat.feat_sdwftx = true;

	if (cmd->svc_id != INVALID_SVC_ID &&
	    feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY])
		cmd->feat.feat_sdwfdelay = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_DELAY])
		cmd->feat.feat_delay = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_JITTER])
		cmd->feat.feat_jitter = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_SOJOURN])
		cmd->feat.feat_sojourn = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_TX_MON_STATS])
		cmd->feat.feat_tx_mon_stats = true;

	if (feat_attr[QCA_VENDOR_ATTR_WLAN_FEAT_MAC80211])
		cmd->feat.feat_mac80211 = true;

	return ret;
}

static int ath12k_extract_user_inputs(struct nlattr **tb,
				      struct ath12k_telemetry_command *cmd)
{
	int ret = 0;

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE])
		cmd->obj = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID])
		cmd->svc_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE])
		ret = ath12k_extract_feat_inputs(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE],
						 cmd);

	/**
	 * To have a unique request ID for an application, the request ID of
	 * the command is compounded with the PID of the requesting application
	 * such that the upper 32 bits represent the PID and the lower
	 * 32 bits represent the request ID provided for the command by the
	 * application.
	 */
	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID])
		cmd->request_id = nla_get_u64(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID]);

	if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID])
		cmd->link_id = nla_get_u8(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID]);
	else
		cmd->link_id = INVALID_LINK_ID;

	if (cmd->obj == STATS_OBJ_PEER) {
		if (tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC] &&
		    (nla_len(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC]) == ETH_ALEN))
			memcpy(cmd->mac,
			       nla_data(tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC]),
			       ETH_ALEN);
	}

	return ret;
}

static int ath12k_prepare_telemetry_common_vendor_attr(struct sk_buff *vendor_event,
						       struct ath12k_telemetry_command *cmd)
{
	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_OBJECT_EVENT,
		       cmd->obj)) {
		ath12k_err(NULL, "nla put failure: Common attr obj field");
		return -EINVAL;
	}

	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID_EVENT,
		       cmd->link_id)) {
		ath12k_err(NULL, "nla put failure: Common attr link_id field");
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID_EVENT,
			      cmd->request_id, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Common attr req_id field");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_rxdma_err_attrs(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       uint32_t *rxdma_error)
{
	struct nlattr *attr;
	int rxdma;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device rxdma error");
		return -EINVAL;
	}

	for (rxdma = 0; rxdma < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX &&
	     rxdma < QCA_VENDOR_ATTR_RXDMA_ERR_MAX; rxdma++)
		if (nla_put_u32(vendor_event, rxdma + 1, rxdma_error[rxdma])) {
			ath12k_err(ab,
				   "nla put failure: Device rxdma err attr %d",
				   rxdma + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_reo_err_attrs(struct ath12k_base *ab,
				     struct sk_buff *vendor_event,
				     uint32_t *reo_error)
{
	struct nlattr *attr;
	int reo_attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device reo error");
		return -EINVAL;
	}

	for (reo_attr = 0; reo_attr < HAL_REO_DEST_RING_ERROR_CODE_MAX &&
	     reo_attr < QCA_VENDOR_ATTR_REO_ERR_MAX;
	     reo_attr++)
		if (nla_put_u32(vendor_event, reo_attr + 1,
				reo_error[reo_attr])) {
			ath12k_err(ab,
				   "nla put failure: Device reo err attr %d",
				   reo_attr + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_device_rx_sw_wbm_drop_attrs(struct ath12k_base *ab,
						   struct sk_buff *vendor_event,
						   struct ath12k_telemetry_dp_device *device_dp_stats)
{
	struct nlattr *attr;
	int drop;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_WBM_SW_DROP_REASON_EVENT);

	if (!attr) {
		ath12k_err(ab, "nla nest failure: Device rx sw wbm drop");
		return -EINVAL;
	}

	for (drop = 0; drop < WBM_ERR_DROP_MAX &&
	     drop < QCA_VENDOR_ATTR_WBM_SW_DROP_MAX; drop++) {
		if (nla_put_u32(vendor_event, drop + 1,
				device_dp_stats->rx_wbm_sw_drop_reason[drop])) {
			ath12k_err(ab,
				   "nla put failure: Device rx sw wbm drop attr %d",
				   drop + 1);
			nla_nest_end(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_device_rx_sw_reo_drop_attrs(struct ath12k_base *ab,
						   struct sk_buff *vendor_event,
						   struct ath12k_telemetry_dp_device *device_dp_stats)
{
	int reo_drop_attr, ring_attr;
	struct nlattr *attr1, *attr2;

	attr1 = nla_nest_start(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_SW_DROP_REASON_EVENT);
	if (!attr1) {
		ath12k_err(ab, "nla nest failure: Device reo sw drop");
		return -EINVAL;
	}

	for (ring_attr = 0; ring_attr < DP_REO_RING_MAX &&
	     ring_attr < QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX; ring_attr++) {
		attr2 = nla_nest_start(vendor_event, ring_attr + 1);
		if (!attr2) {
			ath12k_err(ab,
				   "nla nest failure: Device reo sw drop ring %d",
				   ring_attr + 1);
			return -EINVAL;
		}

		for (reo_drop_attr = 0; reo_drop_attr < DP_RX_ERR_MAX &&
		     reo_drop_attr < QCA_VENDOR_ATTR_REO_SW_DROP_MAX;
		     reo_drop_attr++) {
			if (nla_put_u32(vendor_event, reo_drop_attr + 1,
					device_dp_stats->reo_sw_drop_reason[reo_drop_attr][ring_attr])) {
				ath12k_err(ab,
					   "nla put failure: Device rx sw REO drop attr %d ring %d",
					   reo_drop_attr + 1, ring_attr + 1);
				nla_nest_end(vendor_event, attr2);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, attr2);
	}
	nla_nest_end(vendor_event, attr1);

	return 0;
}

static int ath12k_fill_device_rx_stats(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	if (ath12k_fill_rxdma_err_attrs(ab, vendor_event,
					device_dp_stats->rxdma_error)) {
		ath12k_err(ab, "Error filling device rxdma err Stats");
		return -EINVAL;
	}

	if (ath12k_fill_reo_err_attrs(ab, vendor_event,
				      device_dp_stats->reo_error)) {
		ath12k_err(ab, "Error filling device reo err stats");
		return -EINVAL;
	}

	if (ath12k_fill_device_rx_sw_wbm_drop_attrs(ab, vendor_event,
						    device_dp_stats)) {
		ath12k_err(ab, "Error filling device rx sw wbm drop stats");
		return -EINVAL;
	}

	if (ath12k_fill_device_rx_sw_reo_drop_attrs(ab, vendor_event,
						    device_dp_stats)) {
		ath12k_err(ab, "Error filling device rx sw reo drop stats");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_device_tx_comp_err_attr(struct ath12k_base *ab,
					       struct sk_buff *vendor_event,
					       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	int tx_comp_attr, ring_attr;
	struct nlattr *attr1;
	struct nlattr *attr2;

	attr1 = nla_nest_start(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_EVENT);
	if (!attr1) {
		ath12k_err(ab, "nla nest failure: Device tx comp error");
		return -EINVAL;
	}

	for (ring_attr = 0; ring_attr < DP_TCL_NUM_RING_MAX &&
	     ring_attr < QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX; ring_attr++) {
		attr2 = nla_nest_start(vendor_event, ring_attr + 1);
		if (!attr2) {
			ath12k_err(ab,
				   "nla nest failure: Device tx comp error - ring %d",
				   ring_attr + 1);
			return -EINVAL;
		}

		for (tx_comp_attr = 0; tx_comp_attr < DP_TX_COMP_ERR_MAX &&
		     tx_comp_attr <
		     QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_MAX;
		     tx_comp_attr++) {
			if (nla_put_u32(vendor_event, tx_comp_attr + 1,
					device_dp_stats->tx_comp_err[tx_comp_attr][ring_attr])) {
				ath12k_err(ab,
					   "nla put failure: Device tx comp err attr %d ring %d",
					   ring_attr + 1, tx_comp_attr + 1);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, attr2);
	}
	nla_nest_end(vendor_event, attr1);

	return 0;
}

static int ath12k_fill_device_tx_stats(struct ath12k_base *ab,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_device *device_dp_stats)
{
	if (ath12k_fill_device_tx_comp_err_attr(ab, vendor_event,
						device_dp_stats)) {
		ath12k_err(ab, "Error filling device tx comp err stats");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_device_ppeds_stats(struct ath12k_base *ab,
					  struct sk_buff *vendor_event,
					  struct ath12k_telemetry_dp_device *dev_stats)
{
	int reason = 0;
	struct nlattr *tqm_rel_rsn_attr;

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TCL_PROD_CNT,
			dev_stats->ppeds_stats.tcl_prod_cnt)) {
		ath12k_err(NULL, "nla put failure: tcl prod cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TCL_CONS_CNT,
			dev_stats->ppeds_stats.tcl_cons_cnt)) {
		ath12k_err(NULL, "nla put failure: tcl cons cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_REO_PROD_CNT,
			dev_stats->ppeds_stats.reo_prod_cnt)) {
		ath12k_err(NULL, "nla put failure: reo prod cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_REO_CONS_CNT,
			dev_stats->ppeds_stats.reo_cons_cnt)) {
		ath12k_err(NULL, "nla put failure: reo cons cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_GET_TX_DESC_CNT,
			dev_stats->ppeds_stats.get_tx_desc_cnt)) {
		ath12k_err(NULL, "nla put failure: get tx desc cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_ALLOCATED,
			dev_stats->ppeds_stats.tx_desc_allocated)) {
		ath12k_err(NULL, "nla put failure: tx desc allocated");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_ALLOC_FAILS,
			dev_stats->ppeds_stats.tx_desc_alloc_fails)) {
		ath12k_err(NULL, "nla put failure: tx desc alloc fails");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TX_DESC_FREED,
			dev_stats->ppeds_stats.tx_desc_freed)) {
		ath12k_err(NULL, "nla put failure: tx desc freed");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_FW2WBM_PKT_DROPS,
			dev_stats->ppeds_stats.fw2wbm_pkt_drops)) {
		ath12k_err(NULL, "nla put failure: fw2wbm pkt drops");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_ENABLE_INTR_CNT,
			dev_stats->ppeds_stats.enable_intr_cnt)) {
		ath12k_err(NULL, "nla put failure: enable intr cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_DISABLE_INTR_CNT,
			dev_stats->ppeds_stats.disable_intr_cnt)) {
		ath12k_err(NULL, "nla put failure: disable intr cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_RELEASE_TX_SINGLE_CNT,
			dev_stats->ppeds_stats.release_tx_single_cnt)) {
		ath12k_err(NULL, "nla put failure: release tx single cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_RELEASE_RX_DESC_CNT,
			dev_stats->ppeds_stats.release_rx_desc_cnt)) {
		ath12k_err(NULL, "nla put failure: release rx desc cnt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_NUM_RX_DESC_FREED,
			dev_stats->ppeds_stats.num_rx_desc_freed)) {
		ath12k_err(NULL, "nla put failure: num rx desc freed");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_PPEDS_NUM_RX_DESC_REALLOC,
				dev_stats->ppeds_stats.num_rx_desc_realloc)) {
		ath12k_err(NULL, "nla put failure: num rx desc realloc");
		return -EINVAL;
	}

	tqm_rel_rsn_attr = nla_nest_start(vendor_event,
					  QCA_VENDOR_WLAN_TELEMETRY_PPEDS_TQM_REL_REASON);
	if (!tqm_rel_rsn_attr) {
		ath12k_err(NULL,
			   "nla nest failure: device stats - tqm rel rsn");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX &&
	     reason < HAL_WBM_TQM_REL_REASON_MAX; reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				dev_stats->ppeds_stats.tqm_rel_reason[reason])) {
			ath12k_err(NULL, "nla put failure: device stats-tqm rel rsn %d - %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
				   reason + 1);
			nla_nest_end(vendor_event, tqm_rel_rsn_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, tqm_rel_rsn_attr);

	return 0;
}

static int ath12k_prepare_device_vendor_event(struct sk_buff *vendor_event,
					      struct ath12k_dp *dp,
					      struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_device *telemetry_device;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_device = vmalloc(sizeof(*telemetry_device));
	if (!telemetry_device) {
		ath12k_err(dp->ab, "Failed to allocate telemetry_device for device stats");
		return -ENOMEM;
	}

	memset(telemetry_device, 0, sizeof(*telemetry_device));

	ath12k_dp_get_device_stats(dp, telemetry_device);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_device_rx_stats(dp->ab, vendor_event,
							telemetry_device)) {
				ath12k_err(dp->ab,
					   "Error filling device rx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(dp->ab,
				   "nla nest failure: device rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_device_tx_stats(dp->ab, vendor_event,
							telemetry_device)) {
				ath12k_err(dp->ab,
					   "Error filling device tx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(dp->ab, "nla nest failure: device tx feat stats");
			goto out;
		}
	}
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPEDS_STATS_EVENT);
	if (attr) {
		if (ath12k_fill_device_ppeds_stats(dp->ab, vendor_event,
						   telemetry_device)) {
			ath12k_err(dp->ab,
				   "Error filling device ppeds stats");
			goto out;
		}
		nla_nest_end(vendor_event, attr);
	} else {
		ath12k_err(dp->ab, "nla nest failure: device ppeds stats");
		goto out;
	}
	ret = 0;
out:
	vfree(telemetry_device);
	return ret;
}

static int ath12k_get_common_nl_event_attr_size(void)
{
	int common_size;

	common_size = nla_total_size(sizeof(u32)) +
		      nla_total_size(sizeof(u8)) + /* Link Id */
		      nla_total_size(sizeof(u64)); /* Request Id */

	return common_size;
}

static int ath12k_get_device_feat_rx_attr_size(void)
{
	int payload_size;
	int total_size;
	int attr_size;
	int ring;

	/* RXDMA ERR */
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	attr_size = nla_total_size_nested(payload_size);

	/* REO ERR */
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_DEST_RING_ERROR_CODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* WBM DROP Reason */
	payload_size = nla_total_size(sizeof(u32)) * WBM_ERR_DROP_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* REO DROP */
	for (ring = 0; ring < DP_REO_RING_MAX; ring++) {
		payload_size = nla_total_size(sizeof(u32)) * DP_RX_ERR_MAX;
		/* Size of each rings */
		attr_size += nla_total_size_nested(payload_size);
	}
	attr_size += nla_total_size_nested(attr_size);

	/* Parent RX STATS */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_device_feat_tx_attr_size(void)
{
	int attr_size = 0;
	int payload_size;
	int total_size;
	int ring;

	/* TX COMP ERR */
	for (ring = 0; ring < DP_TCL_NUM_RING_MAX; ring++) {
		payload_size = nla_total_size(sizeof(u32)) *
				DP_TX_COMP_ERR_MAX;
		/* Size of each rings */
		attr_size += nla_total_size_nested(payload_size);
	}

	attr_size += nla_total_size_nested(attr_size);

	/* Parent TX STATS */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_device_ppeds_attr_size(void)
{
	struct ath12k_ppeds_stats stats;
	int attr_size = 0;
	int payload_size = 0;
	int total_size = 0;

	payload_size = nla_total_size(sizeof(stats.tcl_prod_cnt)) +
		       nla_total_size(sizeof(stats.tcl_cons_cnt)) +
		       nla_total_size(sizeof(stats.reo_prod_cnt)) +
		       nla_total_size(sizeof(stats.reo_cons_cnt)) +
		       nla_total_size(sizeof(stats.get_tx_desc_cnt)) +
		       nla_total_size(sizeof(stats.tx_desc_allocated)) +
		       nla_total_size(sizeof(stats.tx_desc_alloc_fails)) +
		       nla_total_size(sizeof(stats.tx_desc_freed)) +
		       nla_total_size(sizeof(stats.fw2wbm_pkt_drops)) +
		       nla_total_size(sizeof(stats.enable_intr_cnt)) +
		       nla_total_size(sizeof(stats.disable_intr_cnt)) +
		       nla_total_size(sizeof(stats.release_tx_single_cnt)) +
		       nla_total_size(sizeof(stats.release_rx_desc_cnt)) +
		       nla_total_size(sizeof(stats.num_rx_desc_freed)) +
		       nla_total_size(sizeof(stats.num_rx_desc_realloc));

	attr_size = payload_size;
	payload_size = nla_total_size(sizeof(u32)) *
		       (HAL_WBM_TQM_REL_REASON_MAX);
	attr_size += nla_total_size_nested(payload_size);

	/* Parent device stats */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_device_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_rx)
		total_size += ath12k_get_device_feat_rx_attr_size();

	if (cmd->feat.feat_tx)
		total_size += ath12k_get_device_feat_tx_attr_size();

	total_size += ath12k_get_device_ppeds_attr_size();

	return total_size;
}

static int ath12k_get_peer_rx_stats_size(void)
{
	struct ath12k_dp_peer_rx_stats rx_stats;
	struct ath12k_dp_peer_rx_dbg_stats rx_dbg;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int dbg_size = 0;
	int ring_num;

	for (ring_num = 0; ring_num < DP_REO_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(rx_stats.recv_from_reo.packets)) +
				nla_total_size(sizeof(rx_stats.recv_from_reo.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(rx_stats.sent_to_stack.packets)) +
				nla_total_size(sizeof(rx_stats.sent_to_stack.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(rx_stats.sent_to_stack_fast.packets)) +
				nla_total_size(sizeof(rx_stats.sent_to_stack_fast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		/* Ring Attr Size */
		total_size += nla_total_size_nested(attr_size);
	}

	/* Advance */
	payload_size = nla_total_size(sizeof(rx_dbg.mcast.packets)) +
			nla_total_size(sizeof(rx_dbg.mcast.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(rx_dbg.ucast.packets)) +
			nla_total_size(sizeof(rx_dbg.ucast.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	dbg_size += nla_total_size(sizeof(rx_dbg.non_amsdu)) +
			nla_total_size(sizeof(rx_dbg.msdu_part_of_amsdu)) +
			nla_total_size(sizeof(rx_dbg.mpdu_retry));

	payload_size = nla_total_size(sizeof(rx_dbg.sg.packets)) +
			nla_total_size(sizeof(rx_dbg.sg.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	total_size += nla_total_size_nested(dbg_size);

	return total_size;
}

/**
 * ath12k_get_hw_tx_stats_attr_size() - NL buffer size for HW offload TX stats
 *
 * Returns the size covering all HW TX attributes.
 */
static int ath12k_get_hw_tx_stats_attr_size(void)
{
	int size = 0;

	size += nla_total_size_64bit(sizeof(u64)) * 4;
	size += nla_total_size(sizeof(u32)) * 3;

	return size;
}

/**
 * ath12k_get_hw_rx_stats_attr_size() - NL buffer size for HW offload RX stats
 *
 * Returns the size covering all HW RX attributes.
 */
static int ath12k_get_hw_rx_stats_attr_size(void)
{
	int size = 0;

	size += nla_total_size_64bit(sizeof(u64)) * 6;
	size += nla_total_size(sizeof(u32)) * 6;

	return size;
}

static int ath12k_get_feat_rx_peer_attr_size(void)
{
	int payload_size;
	int total_size;
	int attr_size;

	/* Rx Stats */
	payload_size = ath12k_get_peer_rx_stats_size();
	/* Rx Per Pkt Stats Attr */
	attr_size = nla_total_size_nested(payload_size);

	/* Rx WBM Err*/
	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_ENTR_RING_RXDMA_ECODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(u32)) *
			HAL_REO_DEST_RING_ERROR_CODE_MAX;
	attr_size += nla_total_size_nested(payload_size);

	/* HW RX stats Attr Size */
	attr_size += ath12k_get_hw_rx_stats_attr_size();

	/* RX packets/bytes derived from PPDU */
	attr_size += nla_total_size(sizeof(u32));
	attr_size += nla_total_size_64bit(sizeof(u64));

	/* Parent RX Stats Attr Size */
	total_size = nla_total_size_nested(attr_size);

	return total_size;
}

static int ath12k_get_htt_tx_stats_basic_attr_size(void)
{
	int total_size = 0;
	int payload_size;
	int attr_size;
	int pkt_type_payload;

	/* TX Unicast Success (struct ath12k_dp_pkt_info: packets + bytes) */
	payload_size = nla_total_size(sizeof(u32)) +  /* packets */
		       nla_total_size(sizeof(u64));    /* bytes */
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* TX PPDUs */
	total_size += nla_total_size(sizeof(u32));

	/* MPDU BASIC - TX MPDUs Success */
	total_size += nla_total_size(sizeof(u32));

	/* TX MPDUs Tried */
	total_size += nla_total_size(sizeof(u32));

	/* Retries MPDU */
	total_size += nla_total_size(sizeof(u32));

	/* RSSI BASIC - Last ACK RSSI */
	total_size += nla_total_size(sizeof(u32));

	/* Average ACK RSSI */
	total_size += nla_total_size(sizeof(u32));

	/* RSSI Chain Array [RSSI_CHAIN_LEN = 8] */
	payload_size = nla_total_size(sizeof(u32) * RSSI_CHAIN_LEN);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* TX Rate */
	total_size += nla_total_size(sizeof(u32));

	/* Basic rate counters (success counts only) - pkt_type[DOT11_MAX][MAX_MCS] */
	pkt_type_payload = nla_total_size(sizeof(u32)) * MAX_MCS;
	payload_size = nla_total_size_nested(pkt_type_payload) * DOT11_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* gi_count[ATH12K_GI_NUM] */
	payload_size = nla_total_size(sizeof(u32)) * ATH12K_GI_NUM;
	total_size += nla_total_size_nested(payload_size);

	/* nss[ATH12K_NSS_NUM] */
	payload_size = nla_total_size(sizeof(u32)) * ATH12K_NSS_NUM;
	total_size += nla_total_size_nested(payload_size);

	/* bw[ATH12K_BW_MAX] */
	payload_size = nla_total_size(sizeof(u32)) * ATH12K_BW_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* ru_start and ru_tones */
	total_size += nla_total_size(sizeof(u32));
	total_size += nla_total_size(sizeof(u32));

	/* mu_group[MAX_MU_GROUP_ID] */
	payload_size = nla_total_size(sizeof(u32)) * MAX_MU_GROUP_ID;
	total_size += nla_total_size_nested(payload_size);

	return total_size;
}

static int ath12k_get_htt_tx_stats_adv_attr_size(void)
{
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ru_payload;
	int tx_type_payload;
	int mu_payload;

	/* DEBUG/ADV - STBC */
	total_size += nla_total_size(sizeof(u32));

	/* LDPC */
	total_size += nla_total_size(sizeof(u32));

	/* WME AC Type Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u32) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* WME AC Type Bytes Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u64) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Excess Retries Per AC Array [WME_AC_MAX = 4] */
	payload_size = nla_total_size(sizeof(u32) * WME_AC_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* AMPDU Count */
	total_size += nla_total_size(sizeof(u32));

	/* Non-AMPDU Count */
	total_size += nla_total_size(sizeof(u32));

	/* Num PPDU Cookie Valid */
	total_size += nla_total_size(sizeof(u32));

	/* Last TX Rate MCS */
	total_size += nla_total_size(sizeof(u32));

	/* Multicast Last TX Rate */
	total_size += nla_total_size(sizeof(u32));

	/* Multicast Last TX Rate MCS */
	total_size += nla_total_size(sizeof(u32));

	/* Average TX Rate */
	total_size += nla_total_size(sizeof(u64));

	/* TX Ratecode */
	total_size += nla_total_size(sizeof(u16));

	/* Preamble Puncture Count */
	total_size += nla_total_size(sizeof(u32));

	/* RU Location Array [MAX_RU_LOCATIONS = 16] */
	/* Each dp_tx_pkt_info has: num_mpdu (u32) + mpdu_tried (u32) */
	payload_size = 0;
	ru_payload = nla_total_size(sizeof(u32)) +  /* num_mpdu */
		     nla_total_size(sizeof(u32));   /* mpdu_tried */
	payload_size += nla_total_size_nested(ru_payload * MAX_RU_LOCATIONS);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Transmit Type Array [MAX_TRANSMIT_TYPES = 9] */
	/* Each dp_tx_pkt_info has: num_mpdu (u32) + mpdu_tried (u32) */
	payload_size = 0;
	tx_type_payload = nla_total_size(sizeof(u32)) +  /* num_mpdu */
			  nla_total_size(sizeof(u32));   /* mpdu_tried */
	payload_size += nla_total_size_nested(tx_type_payload * MAX_TRANSMIT_TYPES);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* SU BE PPDU Count (struct pkt_type) */
	/* pkt_type has: mcs_count[MAX_MCS = 17] */
	payload_size = nla_total_size(sizeof(u32) * MAX_MCS);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* MU BE PPDU Count Array [TXRX_TYPE_MU_MAX = 2] */
	/* Each pkt_type has: mcs_count[MAX_MCS = 17] */
	payload_size = 0;
	mu_payload = nla_total_size(sizeof(u32)) * MAX_MCS;
	payload_size += nla_total_size_nested(mu_payload * TXRX_TYPE_MU_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* SU BN PPDU Count (struct pkt_type) */
	/* pkt_type has: mcs_count[MAX_MCS = 25] */
	payload_size = nla_total_size(sizeof(u32) * MAX_MCS);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* MU BN PPDU Count Array [TXRX_TYPE_MU_MAX = 2] */
	/* Each pkt_type has: mcs_count[MAX_MCS = 25] */
	payload_size = 0;
	mu_payload = nla_total_size(sizeof(u32)) * MAX_MCS;
	payload_size += nla_total_size_nested(mu_payload * TXRX_TYPE_MU_MAX);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* Punctured BW Array [MAX_PUNCTURED_MODE = 5] */
	payload_size = nla_total_size(sizeof(u32) * MAX_PUNCTURED_MODE);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	/* RTS Success */
	total_size += nla_total_size(sizeof(u32));

	/* RTS Failure */
	total_size += nla_total_size(sizeof(u32));

	/* BAR Count */
	total_size += nla_total_size(sizeof(u32));

	/* NDPA Count */
	total_size += nla_total_size(sizeof(u32));

	/* TX PPDU Duration */
	total_size += nla_total_size(sizeof(u64));

	/* TX Power */
	total_size += nla_total_size(sizeof(u8));

	/* TX msdu flush rsn */
	total_size += nla_total_size(sizeof(u32));

	/* TX Duration Per AC Array [WME_NUM_AC = 4] */
	payload_size = nla_total_size(sizeof(u64) * WME_NUM_AC);
	attr_size = nla_total_size_nested(payload_size);
	total_size += attr_size;

	return total_size;
}

/**
 * ath12k_get_tx_ppdu_stats_attr_size - NL attribute size for tx_ppdu_stats
 *
 * Returns total NL attribute size to serialize struct ath12k_htt_tx_ppdu_stats.
 * Reuses ath12k_get_htt_tx_stats_basic_attr_size() and
 * ath12k_get_htt_tx_stats_adv_attr_size() which now operate on tx_ppdu_stats fields.
 */
static int ath12k_get_tx_ppdu_stats_attr_size(void)
{
	int payload_size = 0;
	int total_size = 0;
	/* Basic tx_ppdu_stats fields */
	payload_size += ath12k_get_htt_tx_stats_basic_attr_size();

	/* Advanced tx_ppdu_stats fields */
	payload_size += ath12k_get_htt_tx_stats_adv_attr_size();

	total_size += nla_total_size_nested(payload_size);
	return total_size;
}

static int ath12k_get_feat_proto_peer_attr_size(void)
{
	int total_size = 0;
	int l3_attr_size, l4_attr_size, l5_attr_size, tx_attr_size, rx_attr_size;

	l3_attr_size = sizeof(u32) * DP_PKT_TYPE_L3_MAX;
	total_size += nla_total_size_nested(l3_attr_size);

	l4_attr_size = sizeof(u32) * DP_PKT_TYPE_L4_MAX;
	total_size += nla_total_size_nested(l4_attr_size);

	l5_attr_size = sizeof(u32) * DP_PKT_TYPE_L5_MAX;
	total_size += nla_total_size_nested(l5_attr_size);

	/* Nested Tx and Rx Levels Proto Event */
	tx_attr_size = nla_total_size_nested(total_size * TX_COMP_MAX);
	rx_attr_size = nla_total_size_nested(total_size * RX_RECV_MAX);

	/* Nested Tx and Rx Proto Event */
	total_size += nla_total_size_nested(tx_attr_size);
	total_size += nla_total_size_nested(rx_attr_size);

	/* Nested Proto Event */
	total_size += nla_total_size_nested(total_size);
	return total_size;
}

static u32 ath12k_get_mon_stats_attr_size(void)
{
	u32 payload_size;

	payload_size = nla_total_size(sizeof(u32)) * QCA_VENDOR_ATTR_MON_STATS_MAX;
	return nla_total_size_nested(payload_size);
}

static u32 ath12k_get_tx_mon_stats_attr_size(void)
{
	u32 payload_size;

	payload_size = nla_total_size(sizeof(u32)) * QCA_VENDOR_ATTR_TX_MON_STATS_MAX;
	return nla_total_size_nested(payload_size);
}

static int ath12k_get_feat_tx_peer_attr_size(void)
{
	struct ath12k_dp_peer_tx_stats tx_stats;
	struct ath12k_dp_peer_tx_dbg_stats tx_dbg;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int dbg_size = 0;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		/* Basic */
		payload_size = nla_total_size(sizeof(tx_stats.comp_pkt.packets)) +
				nla_total_size(sizeof(tx_stats.comp_pkt.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(tx_stats.tx_success.packets)) +
				nla_total_size(sizeof(tx_stats.tx_success.bytes));
		attr_size += nla_total_size_nested(payload_size);

		attr_size += nla_total_size(sizeof(tx_stats.tx_failed));

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}

	/* Advance */
	payload_size = nla_total_size(sizeof(u32)) *
			(HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX);
	dbg_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(u32)) *
			(HAL_WBM_TQM_REL_REASON_MAX);
	dbg_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(tx_dbg.mcast.packets)) +
			nla_total_size(sizeof(tx_dbg.mcast.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(tx_dbg.ucast.packets)) +
			nla_total_size(sizeof(tx_dbg.ucast.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	payload_size = nla_total_size(sizeof(tx_dbg.bcast.packets)) +
			nla_total_size(sizeof(tx_dbg.bcast.bytes));
	dbg_size += nla_total_size_nested(payload_size);

	dbg_size += nla_total_size(sizeof(tx_dbg.release_src_not_tqm)) +
			nla_total_size(sizeof(tx_dbg.retry_count)) +
			nla_total_size(sizeof(tx_dbg.total_msdu_retries)) +
			nla_total_size(sizeof(tx_dbg.multiple_retry_count)) +
			nla_total_size(sizeof(tx_dbg.ofdma)) +
			nla_total_size(sizeof(tx_dbg.amsdu_cnt)) +
			nla_total_size(sizeof(tx_dbg.non_amsdu_cnt)) +
			nla_total_size(sizeof(tx_dbg.inval_link_id_pkt_cnt));

	total_size += nla_total_size_nested(dbg_size);

	/* HW TX stats Attr Size */
	total_size += ath12k_get_hw_tx_stats_attr_size();

	/* Parent Tx per_pkt Attr Size */
	total_size = nla_total_size_nested(total_size);

	/* Parent Tx Attr Size */
	total_size = nla_total_size_nested(total_size);
	return total_size;
}

static int get_feat_sdwftx_attr_size_per_msduq(void)
{
	int payload_size = 0, attr_size = 0;
	int nested2_size = 0, nested3_size = 0;
	struct ath12k_dp_link_peer_qos_stats link_qos_stats;
	struct ath12k_dp_qos_tx_stats tx;

	attr_size = nla_total_size(sizeof(tx.tx_success.packets));
	attr_size += nla_total_size(sizeof(tx.tx_success.bytes));
	nested2_size = nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.tx_failed.packets));
	attr_size += nla_total_size(sizeof(tx.tx_failed.bytes));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.tx_ingress.packets));
	attr_size += nla_total_size(sizeof(tx.tx_ingress.bytes));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.dropped.fw_rem.packets));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem.bytes));
	nested3_size = nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.dropped.fw_rem_notx));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_tx));
	attr_size += nla_total_size(sizeof(tx.dropped.age_out));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason1));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason2));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_reason3));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_queue_disable));
	attr_size += nla_total_size(sizeof(tx.dropped.fw_rem_no_match));
	attr_size += nla_total_size(sizeof(tx.dropped.drop_threshold));
	attr_size += nla_total_size(sizeof(tx.dropped.drop_link_desc_na));
	attr_size += nla_total_size(sizeof(tx.dropped.invalid_drop));
	attr_size += nla_total_size(sizeof(tx.dropped.mcast_vdev_drop));
	attr_size += nla_total_size(sizeof(tx.dropped.invalid_rr));

	nested2_size += nla_total_size_nested(nested3_size + attr_size);

	attr_size = nla_total_size(sizeof(tx.svc_intval_stats.success_cnt));
	attr_size = nla_total_size(sizeof(tx.svc_intval_stats.failure_cnt));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.burst_size_stats.success_cnt));
	attr_size = nla_total_size(sizeof(tx.burst_size_stats.failure_cnt));
	nested2_size += nla_total_size_nested(attr_size);

	attr_size = nla_total_size(sizeof(tx.queue_depth));
	attr_size += nla_total_size(sizeof(tx.throughput));
	attr_size += nla_total_size(sizeof(tx.ingress_rate));
	attr_size += nla_total_size(sizeof(tx.min_throughput));
	attr_size += nla_total_size(sizeof(tx.max_throughput));
	attr_size += nla_total_size(sizeof(tx.avg_throughput));
	attr_size += nla_total_size(sizeof(tx.per));
	attr_size += nla_total_size(sizeof(tx.retries_pct));
	attr_size += nla_total_size(sizeof(tx.total_retries_count));
	attr_size += nla_total_size(sizeof(tx.retry_count));
	attr_size += nla_total_size(sizeof(tx.multiple_retry_count));
	attr_size += nla_total_size(sizeof(tx.failed_retry_count));
	attr_size += nla_total_size(sizeof(tx.reinject_pkt));
	attr_size += nla_total_size(sizeof(link_qos_stats.tid));
	attr_size += nla_total_size(sizeof(link_qos_stats.msduq));

	payload_size = nested2_size + attr_size;

	attr_size = nla_total_size(sizeof(u32)) * MAX_MCS;
	nested3_size = nla_total_size_nested(attr_size) *DOT11_MAX;
	/* pkt type */
	nested2_size = nla_total_size_nested(nested3_size + attr_size);
	/* sdwftx stats per msduq */
	payload_size += nla_total_size_nested(nested2_size + payload_size);

	return payload_size;
}

static int ath12k_get_feat_sdwftx_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0, per_msduq_size = 0, msduqs_size = 0;
	int sdwftx_event_size = 0, svc_event_size;
	u8 msduq = 0, user_def_msduq_per_tid = 0;

	if (cmd->svc_id == 0) {
		user_def_msduq_per_tid = QOS_TID_MDSUQ_MAX;
		msduq = user_def_msduq_per_tid * QOS_TID_MAX;
		per_msduq_size = get_feat_sdwftx_attr_size_per_msduq();
		msduqs_size = msduq * per_msduq_size;
		sdwftx_event_size = nla_total_size_nested(msduqs_size);
	} else {
		per_msduq_size = get_feat_sdwftx_attr_size_per_msduq();
		sdwftx_event_size = nla_total_size_nested(per_msduq_size);
	}

	svc_event_size = nla_total_size(sizeof(u8));
	total_size = svc_event_size + sdwftx_event_size;

	return total_size;
}

static int get_feat_sdwfdelay_attr_size_per_msduq(void)
{
	int payload_size = 0, attr_size = 0, attr1_size = 0;
	int nested1_size = 0, nested2_size = 0, nested3_size = 0;
	struct ath12k_dp_link_peer_qos_stats link_qos_stats;
	struct ath12k_dp_qos_delay_stats delay;

	attr_size = nla_total_size(sizeof(delay.nwdelay_avg));
	attr_size += nla_total_size(sizeof(delay.swdelay_avg));
	attr_size += nla_total_size(sizeof(delay.hwdelay_avg));
	attr_size += nla_total_size(sizeof(link_qos_stats.tid));
	attr_size += nla_total_size(sizeof(link_qos_stats.msduq));

	payload_size = attr_size;

	attr_size = nla_total_size(sizeof(delay.invalid_delay_pkts));
	attr_size += nla_total_size(sizeof(delay.delay_success));
	attr_size += nla_total_size(sizeof(delay.delay_failure));
	attr_size += nla_total_size(sizeof(delay.delay_hist.min));
	attr_size += nla_total_size(sizeof(delay.delay_hist.max));
	attr_size += nla_total_size(sizeof(delay.delay_hist.avg));

	attr1_size = nla_total_size(sizeof(u32)) * HIST_BUCKET_MAX;
	/* HW_TX_COMP_DELAY nest */
	nested3_size = nla_total_size_nested(attr1_size);

	/* HWDELAY_HISTOGRAM nest */
	nested2_size = nla_total_size_nested(nested3_size);

	/* SDWFDELAY_HWDELAY */
	nested1_size = nla_total_size_nested(nested2_size + attr_size);

	payload_size += nested1_size;

	return payload_size;
}

static int ath12k_get_feat_sdwfdelay_attr_size(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0, per_msduq_size = 0, msduqs_size = 0;
	int sdwfdelay_event_size = 0, svc_event_size;
	u8 msduq = 0, user_def_msduq_per_tid = 0;

	if (cmd->svc_id == 0) {
		user_def_msduq_per_tid = QOS_TID_MDSUQ_MAX;
		msduq = user_def_msduq_per_tid * QOS_TID_MAX;

		per_msduq_size = get_feat_sdwfdelay_attr_size_per_msduq();
		msduqs_size = msduq * per_msduq_size;

		sdwfdelay_event_size = nla_total_size_nested(msduqs_size);
	} else {
		per_msduq_size = get_feat_sdwfdelay_attr_size_per_msduq();
		sdwfdelay_event_size = nla_total_size_nested(per_msduq_size);
	}

	svc_event_size = nla_total_size(sizeof(u8));
	total_size = svc_event_size + sdwfdelay_event_size;

	return total_size;
}

/**
 * ath12k_vendor_get_rx_mon_stats_size() - Calculate size needed for RX monitor stats
 *
 * Calculates the total netlink buffer size required to serialize all RX monitor
 * statistics including basic counters, arrays, and nested rate statistics.
 *
 * Return: Total size in bytes needed for netlink attributes
 */
static int ath12k_vendor_get_rx_mon_stats_size(void)
{
	struct ath12k_rx_ppdu_stats stats;
	int total_size = 0;
	int payload_size;
	int payload_size_pkt, attr_size_pkt, payload_size_user;
	int attr_size_user, attr_size_type, attr_size_dot11;
	int payload_size_ppdu_nss, payload_size_ppdu_mcs;
	int attr_size_ppdu_nss, attr_size_ppdu_mcs, attr_signal_size;

	/* Basic counters: rx_duration is u64 */
	total_size += nla_total_size_64bit(sizeof(stats.rx_duration));

	/* Basic and advanced counters (u32) */
	total_size += nla_total_size(sizeof(stats.num_mpdu_fcs_ok));
	total_size += nla_total_size(sizeof(stats.num_mpdu_fcs_err));
	total_size += nla_total_size(sizeof(stats.non_ampdu_msdu_count));
	total_size += nla_total_size(sizeof(stats.ampdu_msdu_count));
	total_size += nla_total_size(sizeof(stats.num_mpdus));
	total_size += nla_total_size(sizeof(stats.num_ppdus));
	total_size += nla_total_size(sizeof(stats.num_mpdu_retry_count));
	total_size += nla_total_size(sizeof(stats.num_msdu_retry_count));
	total_size += nla_total_size(sizeof(stats.num_bar));
	total_size += nla_total_size(sizeof(stats.num_ndpa));
	total_size += nla_total_size(sizeof(stats.last_rx_rate));
	total_size += nla_total_size(sizeof(stats.rnd_avg_rx_rate));
	total_size += nla_total_size(sizeof(stats.avg_rx_rate));
	total_size += nla_total_size(sizeof(stats.rx_ratecode));

	/* reception_type array (u32) */
	payload_size = nla_total_size(sizeof(stats.reception_type[0])) *
					    HAL_RX_RECEPTION_TYPE_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* ppdu_reception array (u32) */
	payload_size = nla_total_size(sizeof(stats.ppdu_reception[0])) *
					    HAL_RX_RECEPTION_TYPE_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* gi_count array (u32) */
	payload_size = nla_total_size(sizeof(stats.gi_count[0])) *
					    HAL_RX_GI_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* nss_count array (u32) */
	payload_size = nla_total_size(sizeof(stats.nss_count[0])) *
					    HAL_RX_MAX_NSS;
	total_size += nla_total_size_nested(payload_size);

	/* ppdu_nss array (u32) */
	payload_size = nla_total_size(sizeof(stats.ppdu_nss[0])) *
					    HAL_RX_MAX_NSS;
	total_size += nla_total_size_nested(payload_size);

	/* bw_count array (u32) */
	payload_size = nla_total_size(sizeof(stats.bw_count[0])) *
					    HAL_RX_BW_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* punc_bw array (u32) */
	payload_size = nla_total_size(sizeof(stats.punc_bw[0])) *
					    MAX_PUNCTURED_MODE;
	total_size += nla_total_size_nested(payload_size);

	/* proto_type stats (DOT11_MAX pkt_types - covers all preamble types) */
	payload_size_pkt = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX;
	attr_size_pkt   = nla_total_size_nested(payload_size_pkt);
	attr_size_dot11 = attr_size_pkt * DOT11_MAX;
	total_size     += nla_total_size_nested(attr_size_dot11);

	/* wme_ac_type array (u32 pkts per AC) - attribute 21 */
	payload_size = nla_total_size(sizeof(u32)) * WME_NUM_AC;
	total_size += nla_total_size_nested(payload_size);

	/* wme_ac_type_bytes array (u64 bytes per AC) - attribute 22 */
	payload_size = nla_total_size_64bit(sizeof(u64)) * WME_NUM_AC;
	total_size += nla_total_size_nested(payload_size);

	/* su_ppdu_count stats (ATH12K_RX_PPDU_PROTO_MAX pkt_types) */
	payload_size_pkt = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX;
	attr_size_pkt   = nla_total_size_nested(payload_size_pkt);
	attr_size_dot11 = attr_size_pkt *
			  QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PPDU_PKT_TYPE_MAX;
	total_size     += nla_total_size_nested(attr_size_dot11);

	/* rx_mu stats (ATH12K_RX_MU_PROTO_MAX entries) */
	payload_size_ppdu_nss = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX;
	attr_size_ppdu_nss = nla_total_size_nested(payload_size_ppdu_nss);

	payload_size_ppdu_mcs = nla_total_size(sizeof(u32)) *
				QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX;
	attr_size_ppdu_mcs = nla_total_size_nested(payload_size_ppdu_mcs);

	payload_size_user =
		nla_total_size(sizeof(u32)) + /* mpdu_cnt_fcs_ok */
		nla_total_size(sizeof(u32)) + /* mpdu_cnt_fcs_err */
		attr_size_ppdu_nss +          /* ppdu nss */
		attr_size_ppdu_mcs;           /* ppdu mcs */

	attr_size_user = nla_total_size_nested(payload_size_user);
	attr_size_type = attr_size_user * QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MAX;
	attr_size_dot11 = nla_total_size_nested(attr_size_type) *
			  QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PPDU_PKT_TYPE_MAX;
	total_size += nla_total_size_nested(attr_size_dot11);

	/* signal stats */
	attr_signal_size  = nla_total_size(sizeof(stats.signal_stats.snr));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.snr_avg));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.snr_dp));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.snr_dp_avg));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.rssi));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.rssi_avg));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.rssi_dp));
	attr_signal_size += nla_total_size(sizeof(stats.signal_stats.rssi_dp_avg));
	/* RSSI calculation inputs */
	attr_signal_size += nla_total_size(sizeof(s8));   /* region_offset */
	attr_signal_size += nla_total_size(sizeof(u8));   /* bw_offset */
	attr_signal_size += nla_total_size(sizeof(s8));   /* avg_nf_dbm */
	attr_signal_size += nla_total_size(sizeof(s32));  /* rssi_temp_offset */
	attr_signal_size += nla_total_size(sizeof(u32));  /* xlna_bypass_offset */
	attr_signal_size += nla_total_size(sizeof(u32));  /* xlna_bypass_threshold */
	total_size += nla_total_size_nested(attr_signal_size);

	/* Per-AC RX duration nested */
	payload_size = nla_total_size_64bit(sizeof(stats.rx_duration_ac[0])) *
					    QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX;
	total_size += nla_total_size_nested(payload_size);

	/* Parent RX attr size */
	total_size = nla_total_size_nested(total_size);

	return total_size;
}

static int ath12k_get_tid_tx_stats_attr_size(bool is_hw_stats)
{
	int size = 0;
	int delay_hist_size;
	int delay_stat_size;
	int tqm_stats_cnt_max = is_hw_stats ? QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX_EXT :
					      QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX;

	/* tqm_status_cnt */
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      tqm_stats_cnt_max);

	/* htt_status_cnt */
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX);

	/* swdrop_cnt */
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      QCA_VENDOR_ATTR_TID_TX_SW_DROP_MAX);

	delay_hist_size = nla_total_size(sizeof(u32)) *
				QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_MAX;
	delay_stat_size = nla_total_size(sizeof(u32)) * 3 +  /* max, min, avg */
			  nla_total_size_nested(delay_hist_size);

	/* swq_delay, hwtx_delay, intfrm_delay - 3 delay histogram nests */
	size += nla_total_size_nested(delay_stat_size) * 3;

	/* Outer per-TID nest */
	return nla_total_size_nested(size);
}

static int ath12k_get_tid_rx_stats_attr_size(bool is_hw_stats)
{
	int size;
	int delay_size;
	int fail_cnt_max = is_hw_stats ? DP_TID_RX_SW_DROP_MAX_EXT :
					 DP_TID_RX_SW_DROP_MAX;

	/* delivered_to_stack, msdu_cnt, mcast_msdu_cnt, bcast_msdu_cnt */
	size = nla_total_size(sizeof(u32)) * 4;

       /* fail_cnt */
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      fail_cnt_max);

	/* reo_err: reo_code_inv (u32) + reo_codes nested */
	size += nla_total_size(sizeof(u32));
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      HAL_REO_DEST_RING_ERROR_CODE_MAX);

	/* rxdma_err: rxdma_code_inv (u32) + rxdma_codes nested */
	size += nla_total_size(sizeof(u32));
	size += nla_total_size_nested(nla_total_size(sizeof(u32)) *
				      HAL_REO_ENTR_RING_RXDMA_ECODE_MAX);

	/* to_stack_delay, intfrm_delay */
	delay_size = nla_total_size_nested(nla_total_size(sizeof(u32)) *
					   HIST_BUCKET_MAX);
	delay_size += nla_total_size(sizeof(u32)) * 3;
	size += nla_total_size_nested(delay_size) * 2;

	/* Outer per-TID nest */
	return nla_total_size_nested(size);
}

static int ath12k_get_tid_stats_attr_size(bool is_hw_stats)
{
	int tx_size, rx_size;

	/* TID_TX_STATS nest */
	tx_size = ath12k_get_tid_tx_stats_attr_size(is_hw_stats) *
			QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS;
	tx_size = nla_total_size_nested(tx_size);

	/* TID_RX_STATS nest */
	rx_size = ath12k_get_tid_rx_stats_attr_size(is_hw_stats) *
			QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS;
	rx_size = nla_total_size_nested(rx_size);

	/* Outer TID_STATS_EVENT nest */
	return nla_total_size_nested(tx_size + rx_size);
}

/**
 * ath12k_get_delay_hist_attr_size() - Calculate NL buffer size for one histogram
 *
 * Returns the netlink attribute size needed to serialize a single
 * struct hist_stats (min, max, avg + HIST_BUCKET_MAX frequency buckets).
 */
static int ath12k_get_delay_hist_attr_size(void)
{
	int size = 0;
	int freq_payload;

	/* min, max, avg */
	size += nla_total_size(sizeof(u32)) * 3;

	/* freq array: HIST_BUCKET_MAX u32 values */
	freq_payload = nla_total_size(sizeof(u32)) *
		QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_MAX;
	size += nla_total_size_nested(freq_payload);

	return size;
}

/**
 * ath12k_get_delay_stats_attr_size() - Calculate NL buffer size for all TID delay stats
 *
 * Returns the total netlink attribute size needed to serialize
 * struct ath12k_dp_peer_tid_agg_delay_stats (DP_TID_MAX TIDs, each with
 * TX SW queue, TX HW, and RX to-stack delay histograms).
 */
static int ath12k_get_delay_stats_attr_size(void)
{
	int hist_size;
	int tid_size;
	int total_size;

	hist_size = ath12k_get_delay_hist_attr_size();

	/* Per TID: TID index + 3 nested histograms */
	tid_size = nla_total_size(sizeof(u8));
	tid_size += nla_total_size_nested(hist_size) * 3;

	/* DP_TID_MAX TIDs, each wrapped in a nested attr */
	total_size = nla_total_size_nested(tid_size) * DP_TID_MAX;

	return total_size;
}

/**
 * ath12k_get_jitter_stats_attr_size() - Calculate NL buffer size for all Jitter stats
 *
 * Returns the total netlink attribute size needed to serialize
 * struct ath12k_dp_peer_tid_jitter_stats.
 */
static int ath12k_get_jitter_stats_attr_size(void)
{
	int tid_size;
	int total_size = 0;

	/* Per TID: TID index (u8) + 3x u32 + 3x u64 */
	tid_size = nla_total_size(sizeof(u8)) +
		   nla_total_size(sizeof(u32)) * 3 +
		   nla_total_size_64bit(sizeof(u64)) * 3;

	/* DP_TID_MAX TIDs, each wrapped in a nested attr */
	total_size = nla_total_size_nested(tid_size) * DP_TID_MAX;

	return total_size;
}

/**
 * ath12k_get_sojourn_stats_attr_size() - Calculate NL buffer size for all Sojourn stats
 *
 * Returns the total netlink attribute size needed to serialize
 * struct ath12k_dp_peer_tid_sojourn_stats.
 */
static int ath12k_get_sojourn_stats_attr_size(void)
{
	int tid_size;
	int total_size = 0;

	/* Per TID: TID index (u8) + 2x u32 */
	tid_size = nla_total_size(sizeof(u8)) +
		   nla_total_size(sizeof(u32)) +
		   nla_total_size(sizeof(u64));

	/* DP_TID_MAX TIDs, each wrapped in a nested attr */
	total_size = nla_total_size_nested(tid_size) * DP_TID_MAX;

	return total_size;
}

static int ath12k_get_mac80211_flow_stats_attr_size(void)
{
	return nla_total_size(5 * nla_total_size(sizeof(u32)));
}

static int ath12k_get_dp_peer_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	/* mac80211 flow stats are always emitted */
	total_size += ath12k_get_mac80211_flow_stats_attr_size();

	if (cmd->feat.feat_rx) {
		total_size += ath12k_get_feat_rx_peer_attr_size();
		/* Add size for RX monitor stats */
		total_size += ath12k_vendor_get_rx_mon_stats_size();
	}

	if (cmd->feat.feat_tx) {
		total_size += ath12k_get_feat_tx_peer_attr_size();
		total_size += ath12k_get_tx_ppdu_stats_attr_size();
	}

	if (cmd->feat.feat_proto)
		total_size += ath12k_get_feat_proto_peer_attr_size();

	if (cmd->feat.feat_mon_stats)
		total_size += ath12k_get_mon_stats_attr_size();

	if (cmd->feat.feat_sdwftx)
		total_size += ath12k_get_feat_sdwftx_attr_size(cmd);

	if (cmd->feat.feat_sdwfdelay)
		total_size += ath12k_get_feat_sdwfdelay_attr_size(cmd);

	if (cmd->feat.feat_delay)
		total_size += ath12k_get_delay_stats_attr_size();

	if (cmd->feat.feat_jitter)
		total_size += ath12k_get_jitter_stats_attr_size();

	if (cmd->feat.feat_sojourn)
		total_size += ath12k_get_sojourn_stats_attr_size();

	if (cmd->feat.feat_tx_mon_stats)
		total_size += ath12k_get_tx_mon_stats_attr_size();

	return total_size;
}

static int ath12k_get_feat_proto_vap_attr_size(void)
{
	int total_size = 0;
	int l3_attr_size, l4_attr_size, l5_attr_size;

	l3_attr_size = sizeof(u32) * DP_PKT_TYPE_L3_MAX;
	total_size += nla_total_size_nested(l3_attr_size);

	l4_attr_size = sizeof(u32) * DP_PKT_TYPE_L4_MAX;
	total_size += nla_total_size_nested(l4_attr_size);

	l5_attr_size = sizeof(u32) * DP_PKT_TYPE_L5_MAX;
	total_size += nla_total_size_nested(l5_attr_size);

	/* Nested Tx and Rx Levels Proto Event */
	total_size += nla_total_size_nested(total_size * TX_ENQUEUE_MAX);

	/* Nested Tx and Rx Proto Event */
	total_size += nla_total_size_nested(total_size);

	/* Nested Proto Event */
	total_size += nla_total_size_nested(total_size);
	return total_size;
}

static int ath12k_get_dp_ingress_attr_len(void)
{
	struct ath12k_dp_tx_ingress_stats ingress_stats;
	int total_size = 0;
	int payload_size;
	int attr_size;
	int ring_num;

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		payload_size = nla_total_size(sizeof(ingress_stats.recv_from_stack.packets)) +
				nla_total_size(sizeof(ingress_stats.recv_from_stack.bytes));
		attr_size = nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(ingress_stats.enque_to_hw.packets)) +
				nla_total_size(sizeof(ingress_stats.enque_to_hw.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(ingress_stats.enque_to_hw_fast.packets)) +
				nla_total_size(sizeof(ingress_stats.enque_to_hw_fast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_TCL_ENCAP_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * HAL_ENCRYPT_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TCL_DESC_TYPE_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(uint32_t)) * DP_TX_ENQ_ERR_MAX;
		attr_size += nla_total_size_nested(payload_size);

		payload_size = nla_total_size(sizeof(ingress_stats.mcast.packets)) +
				nla_total_size(sizeof(ingress_stats.mcast.bytes));
		attr_size += nla_total_size_nested(payload_size);

		/* TCL Ring Attr size */
		total_size += nla_total_size_nested(attr_size);
	}
	/* Parent Ingress Stats Attr size */
	total_size += nla_total_size_nested(total_size);

	return total_size;
}

static int ath12k_get_dp_vif_attr_len(struct ath12k_telemetry_command *cmd)
{
	int total_size = 0;

	if (cmd->feat.feat_tx)
		total_size += ath12k_get_dp_ingress_attr_len();

	if (cmd->feat.feat_proto)
		total_size += ath12k_get_feat_proto_vap_attr_size();

#ifdef CPTCFG_QCN_EXTN
	if (cmd->feat.feat_rx)
		total_size += ath12k_get_dp_rx_scan_radio_stats_len();
#endif /* CPTCFG_QCN_EXTN */

	/*Aggregated Sta Stats Size */
	total_size += ath12k_get_dp_peer_attr_len(cmd);

	return total_size;
}

static int ath12k_get_radio_cp_attr_len(void)
{
	int payload_size;

	payload_size = nla_total_size(sizeof(u32)) *
		       QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_AFTER_LAST;

	return nla_total_size_nested(payload_size);
}

static int ath12k_get_vap_cp_attr_len(void)
{
	int payload_size;

	payload_size = nla_total_size(sizeof(u32)) +
		(nla_total_size(sizeof(u64)) * (QCA_VENDOR_ATTR_TELEMETRY_CP_MAX - 1));
#ifdef CPTCFG_QCN_EXTN
	payload_size += ath12k_get_cp_tx_scan_radio_stats_len();
#endif /* CPTCFG_QCN_EXTN */

	return nla_total_size_nested(payload_size);
}

static int ath12k_get_dp_radio_attr_len(struct ath12k_telemetry_command *cmd,
					bool is_hw_stats)
{
	/* TID Stats Size */
	if (cmd->feat.feat_tid)
		return ath12k_get_tid_stats_attr_size(is_hw_stats);

	/*Aggregated Sta Stats Size */
	return ath12k_get_dp_peer_attr_len(cmd);
}

int ath12k_get_dp_vendor_event_len(struct ath12k_telemetry_command *cmd,
				   bool is_hw_stats)
{
	int total_size;

	total_size = ath12k_get_common_nl_event_attr_size();

	switch (cmd->obj) {
	case STATS_OBJ_PEER:
		total_size += ath12k_get_dp_peer_attr_len(cmd);
		break;
	case STATS_OBJ_VIF:
		total_size += ath12k_get_dp_vif_attr_len(cmd);
		total_size += ath12k_get_vap_cp_attr_len();
		break;
	case STATS_OBJ_RADIO:
		total_size += ath12k_get_dp_radio_attr_len(cmd, is_hw_stats);
		total_size += ath12k_get_radio_cp_attr_len();
		break;
	case STATS_OBJ_DEVICE:
		total_size += ath12k_get_device_attr_size(cmd);
		break;
	default:
		ath12k_err(NULL, "Invalid obj Type");
	}

	return NLMSG_HDRLEN + total_size;
}

static int
ath12k_fill_ru_loc_mpdu_succ_tried(struct sk_buff *vendor_event,
				   struct ath12k_dp_link_peer_stats
				   *link_peer_stats)
{
	struct nlattr *attr, *ru_attr;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	int i;

	if (!link_peer_stats || !link_peer_stats->tx_ppdu_stats)
		return -EINVAL;

	tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_MPDU_SUC_TRD);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: ru loc");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RU_ALLOC_MAX &&
	     i < MAX_RU_LOCATIONS; i++) {
		ru_attr = nla_nest_start(vendor_event, (i + 1));
		if (!ru_attr) {
			ath12k_err(NULL, "nla nest failure: ru loc %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_NUM_MPDU,
			tx_ppdu_stats->ru_loc_mpdu_succ_tried[i].num_mpdu)) {
			ath12k_err(NULL,
				   "nla put failed: ru loc num mpdu %d", i);
			nla_nest_cancel(vendor_event, ru_attr);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_MPDU_TRD,
			tx_ppdu_stats->ru_loc_mpdu_succ_tried[i].mpdu_tried)) {
			ath12k_err(NULL,
				   "nla put failed: ru loc mpdu tried %d", i);
			nla_nest_cancel(vendor_event, ru_attr);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, ru_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_transmit_type_mpdu_succ_tried(struct sk_buff *vendor_event,
					  struct ath12k_dp_link_peer_stats
					  *link_peer_stats)
{
	struct nlattr *attr, *tx_type_attr;
	struct ath12k_tx_pkt_info *tx_type_info;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats =
		link_peer_stats->tx_ppdu_stats;
	int i;

	if (!tx_ppdu_stats) {
		ath12k_err(NULL, "link peer tx stats is NULL");
		return -EINVAL;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDU_SUC_TRD);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: transmit type");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PPDU_MAX &&
	     i < MAX_TRANSMIT_TYPES; i++) {
		tx_type_attr = nla_nest_start(vendor_event, (i + 1));
		if (!tx_type_attr) {
			ath12k_err(NULL,
				   "nla nest failure: transmit type %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		tx_type_info =
			&tx_ppdu_stats->transmit_type_mpdu_succ_tried[i];

		if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_NUM_MPDU,
			tx_type_info->num_mpdu)) {
			ath12k_err(NULL,
				   "nla put failed: transmit type num mpdu %d",
				   i);
			nla_nest_cancel(vendor_event, tx_type_attr);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TX_PKT_INFO_MPDU_TRD,
			tx_type_info->mpdu_tried)) {
			ath12k_err(NULL,
				   "nla put failed: transmit type mpdu tried %d",
				   i);
			nla_nest_cancel(vendor_event, tx_type_attr);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, tx_type_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_su_be_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct nlattr *attr;
	int i;

	tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
	if (!tx_ppdu_stats)
		return -EINVAL;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_SU_BE_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: su be ppdu cnt");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX && i < MAX_MCS; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				tx_ppdu_stats->su_be_ppdu_cnt.mcs_count[i])) {
			ath12k_err(NULL,
				   "nla put failure: su be ppdu mcs %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_mu_be_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct nlattr *attr, *mu_attr;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats = NULL;
	int i, j;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_BE_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: mu be ppdu cnt");
		return -EINVAL;
	}

	tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
	if (!tx_ppdu_stats) {
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_MAX &&
	     i < TXRX_TYPE_MU_MAX; i++) {
		mu_attr = nla_nest_start(vendor_event, (i + 1));
		if (!mu_attr) {
			ath12k_err(NULL,
				   "nla nest failure: mu be ppdu %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX && j < MAX_MCS; j++) {
			if (nla_put_u32(vendor_event, (j + 1),
					tx_ppdu_stats->mu_be_ppdu_cnt[i].mcs_count[j])) {
				ath12k_err(NULL,
					   "nla put failure: mu be ppdu mcs %d:%d",
					   i, j);
				nla_nest_cancel(vendor_event, mu_attr);
				nla_nest_cancel(vendor_event, attr);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, mu_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_su_bn_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct nlattr *attr;
	int i;

	tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
	if (!tx_ppdu_stats)
		return -EINVAL;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_SU_BN_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: su bn ppdu cnt");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX && i < MAX_MCS; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				tx_ppdu_stats->su_bn_ppdu_cnt.mcs_count[i])) {
			ath12k_err(NULL,
				   "nla put failure: su bn ppdu mcs %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_mu_bn_ppdu_cnt(struct sk_buff *vendor_event,
			   struct ath12k_dp_link_peer_stats
			   *link_peer_stats)
{
	struct nlattr *attr, *mu_attr;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats = NULL;
	int i, j;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_BN_PPDU_CNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: mu bn ppdu cnt");
		return -EINVAL;
	}

	tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
	if (!tx_ppdu_stats) {
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_TXRX_MU_MAX &&
	     i < TXRX_TYPE_MU_MAX; i++) {
		mu_attr = nla_nest_start(vendor_event, (i + 1));
		if (!mu_attr) {
			ath12k_err(NULL,
				   "nla nest failure: mu bn ppdu %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX && j < MAX_MCS; j++) {
			if (nla_put_u32(vendor_event, (j + 1),
					tx_ppdu_stats->mu_bn_ppdu_cnt[i].mcs_count[j])) {
				ath12k_err(NULL,
					   "nla put failure: mu bn ppdu mcs %d:%d",
					   i, j);
				nla_nest_cancel(vendor_event, mu_attr);
				nla_nest_cancel(vendor_event, attr);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, mu_attr);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

/**
 * ath12k_fill_peer_tx_ppdu_stats_attr() - Fill peer TX PPDU stats attributes
 * @ar: ath12k radio handle
 * @vendor_event: vendor event skb to populate
 * @link_peer_stats: link peer stats containing tx_ppdu_stats
 *
 * Serializes struct ath12k_htt_tx_ppdu_stats into telemetry vendor netlink
 * attributes. This helper is used only for stats reporting when
 * DP_ENABLE_TX_PPDU_STATS is enabled and must not affect peer lifecycle state.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int
ath12k_fill_peer_tx_ppdu_stats_attr(struct ath12k *ar,
				    struct sk_buff *vendor_event,
				    struct ath12k_dp_link_peer_stats *link_peer_stats,
				    int peer_type)
{
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats = link_peer_stats->tx_ppdu_stats;
	struct nlattr *attr;
	int i, j;

	if (!tx_ppdu_stats)
		return 0;

	/* TX Unicast Success */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_UCAST_SUCC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx ucast success");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			tx_ppdu_stats->tx_ucast_success.packets)) {
		ath12k_err(NULL, "nla put failure: tx ucast success pkts");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      tx_ppdu_stats->tx_ucast_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: tx ucast success bytes");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/* TX PPDUs */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PPDUS,
			tx_ppdu_stats->tx_ppdus)) {
		ath12k_err(NULL, "nla put failed: tx ppdus");
		return -EINVAL;
	}

	/* MPDU BASIC - TX MPDUs Success */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDUS_SUCCESS,
			tx_ppdu_stats->tx_mpdus_success)) {
		ath12k_err(NULL, "nla put failed: tx mpdus success");
		return -EINVAL;
	}

	/* TX MPDUs Tried */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_MPDUS_TRIED,
			tx_ppdu_stats->tx_mpdus_tried)) {
		ath12k_err(NULL, "nla put failed: tx mpdus tried");
		return -EINVAL;
	}

	/* Retries MPDU */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RETRIES_MPDU,
			tx_ppdu_stats->retries_mpdu)) {
		ath12k_err(NULL, "nla put failed: retries mpdu");
		return -EINVAL;
	}
	/* RSSI Chain */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RSSI_CHAIN);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: rssi chain");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX &&
	     i < RSSI_CHAIN_LEN; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->rssi_chain[i])) {
			ath12k_err(NULL, "nla put failure: rssi chain %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* pkt_type[DOT11_MAX] - per dot11-std, per-MCS success counts */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PKT_TYPE);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: pkt type");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_DOT11_MAX &&
	     i < DOT11_MAX; i++) {
		struct nlattr *mcs_nest;

		mcs_nest = nla_nest_start(vendor_event, (i + 1));
		if (!mcs_nest) {
			nla_nest_cancel(vendor_event, attr);
			ath12k_err(NULL, "nla nest failure: pkt type mcs %d", i);
			return -EINVAL;
		}
		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX &&
		     j < MAX_MCS; j++) {
			if (nla_put_u32(vendor_event, (j + 1),
					tx_ppdu_stats->pkt_type[i].mcs_count[j])) {
				nla_nest_cancel(vendor_event, mcs_nest);
				nla_nest_cancel(vendor_event, attr);
				ath12k_err(NULL,
					   "nla put failure: pkt type[%d] mcs[%d]",
					   i, j);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, mcs_nest);
	}
	nla_nest_end(vendor_event, attr);

	/* gi_count[ATH12K_GI_NUM] */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_GI_COUNT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: sgi count");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_GI_MAX &&
	     i < ATH12K_GI_NUM; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->gi_count[i])) {
			nla_nest_cancel(vendor_event, attr);
			ath12k_err(NULL, "nla put failure: sgi count[%d]", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* nss[ATH12K_NSS_NUM] */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NSS);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: nss");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_NSS_MAX &&
	     i < ATH12K_NSS_NUM; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->nss[i])) {
			nla_nest_cancel(vendor_event, attr);
			ath12k_err(NULL, "nla put failure: nss[%d]", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* bw[ATH12K_BW_MAX] */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_BW);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: bw");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_BW_MAX &&
	     i < ATH12K_BW_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->bw[i])) {
			nla_nest_cancel(vendor_event, attr);
			ath12k_err(NULL, "nla put failure: bw[%d]", i);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		/* Last ACK RSSI */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LAST_ACK_RSSI,
				tx_ppdu_stats->last_ack_rssi)) {
			ath12k_err(NULL, "nla put failed: last ack rssi");
			return -EINVAL;
		}

		/* Avg ACK RSSI */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AVG_ACK_RSSI,
				tx_ppdu_stats->avg_ack_rssi)) {
			ath12k_err(NULL, "nla put failed: avg ack rssi");
			return -EINVAL;
		}

		/* TX Rate */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_RATE,
				tx_ppdu_stats->tx_rate)) {
			ath12k_err(NULL, "nla put failed: tx rate");
			return -EINVAL;
		}

		/* ru_start */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_START,
				tx_ppdu_stats->ru_start)) {
			ath12k_err(NULL, "nla put failed: ru start");
			return -EINVAL;
		}

		/* ru_tones */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RU_TONES,
				tx_ppdu_stats->ru_tones)) {
			ath12k_err(NULL, "nla put failed: ru tones");
			return -EINVAL;
		}

		/* mu_group[MAX_MU_GROUP_ID] */
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MU_GROUP);
		if (!attr) {
			ath12k_err(NULL, "nla nest failure: mu group id");
			return -EINVAL;
		}
		for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_MU_GRP_MAX &&
		     i < MAX_MU_GROUP_ID; i++) {
			if (nla_put_u32(vendor_event, (i + 1),
					tx_ppdu_stats->mu_group[i])) {
				nla_nest_cancel(vendor_event, attr);
				ath12k_err(NULL, "nla put failure: mu group id[%d]", i);
				return -EINVAL;
			}
		}
		nla_nest_end(vendor_event, attr);
	}

	if (!ath12k_dp_advance_stats_enabled(&ar->dp))
		return 0;

	/* DEBUG/ADV - STBC */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_STBC,
			tx_ppdu_stats->stbc)) {
		ath12k_err(NULL, "nla put failed: stbc");
		return -EINVAL;
	}

	/* LDPC */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LDPC,
			tx_ppdu_stats->ldpc)) {
		ath12k_err(NULL, "nla put failed: ldpc");
		return -EINVAL;
	}

	/* WME AC Type */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_WME_AC_TYPE);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: wme ac type");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_MAX &&
	     i < WME_AC_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->wme_ac_type[i])) {
			ath12k_err(NULL, "nla put failure: wme ac type %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* WME AC Type Bytes */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_WME_AC_BYTES);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: wme ac type bytes");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_MAX &&
	     i < WME_AC_MAX; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      tx_ppdu_stats->wme_ac_type_bytes[i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: wme ac type bytes %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* Excess Retries Per AC */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_EXCESS_RETRY_AC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: excess retries per ac");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_WME_AC_MAX &&
	     i < WME_AC_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				tx_ppdu_stats->excess_retries_per_ac[i])) {
			ath12k_err(NULL, "nla put failure: excess retries per ac %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* AMPDU Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AMPDU_CNT,
			tx_ppdu_stats->ampdu_cnt)) {
		ath12k_err(NULL, "nla put failed: ampdu cnt");
		return -EINVAL;
	}

	/* Non-AMPDU Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NON_AMPDU_CNT,
			tx_ppdu_stats->non_ampdu_cnt)) {
		ath12k_err(NULL, "nla put failed: non ampdu cnt");
		return -EINVAL;
	}

	/* Num PPDU Cookie Valid */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NUM_PPDU_COOKIE_VALID,
			tx_ppdu_stats->num_ppdu_cookie_valid)) {
		ath12k_err(NULL, "nla put failed: num ppdu cookie valid");
		return -EINVAL;
	}

	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		/* Average TX Rate */
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_AVG_RATE,
				      WEIGHTED_AVG_OUT(tx_ppdu_stats->avg_tx_rate),
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: avg tx rate");
			return -EINVAL;
		}

		/* TX Ratecode */
		if (nla_put_u16(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_RATECODE,
				tx_ppdu_stats->tx_ratecode)) {
			ath12k_err(NULL, "nla put failed: tx ratecode");
			return -EINVAL;
		}

		/* Last TX Rate MCS */
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_LAST_RATE_MCS,
				tx_ppdu_stats->last_tx_rate_mcs)) {
			ath12k_err(NULL, "nla put failed: last tx rate mcs");
			return -EINVAL;
		}

		/* TX PPDU Duration */
		if (nla_put_u64_64bit(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PPDU_DURATION,
			tx_ppdu_stats->tx_ppdu_duration,
			NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failed: tx ppdu duration");
			return -EINVAL;
		}

		/* TX Power */
		if (nla_put_u8(vendor_event,
			       QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_TX_PWR,
			       tx_ppdu_stats->tx_pwr)) {
			ath12k_err(NULL, "nla put failed: tx pwr");
			return -EINVAL;
		}
	}

	/* Multicast Last TX Rate */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MCAST_LAST_TX_RATE,
			tx_ppdu_stats->mcast_last_tx_rate)) {
		ath12k_err(NULL, "nla put failed: mcast last tx rate");
		return -EINVAL;
	}

	/* Multicast Last TX Rate MCS */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MCAST_LAST_TX_RATE_MCS,
			tx_ppdu_stats->mcast_last_tx_rate_mcs)) {
		ath12k_err(NULL, "nla put failed: mcast last tx rate mcs");
		return -EINVAL;
	}

	/* Preamble Puncture Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PREAM_PUNCT_CNT,
			tx_ppdu_stats->pream_punct_cnt)) {
		ath12k_err(NULL, "nla put failed: pream punct cnt");
		return -EINVAL;
	}

	/* RU Location Array */
	if (ath12k_fill_ru_loc_mpdu_succ_tried(vendor_event, link_peer_stats))
		return -EINVAL;

	/* Transmit Type Array */
	if (ath12k_fill_transmit_type_mpdu_succ_tried(vendor_event, link_peer_stats))
		return -EINVAL;

	/* SU BE PPDU Count */
	if (ath12k_fill_su_be_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* MU BE PPDU Count */
	if (ath12k_fill_mu_be_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* SU BN PPDU Count */
	if (ath12k_fill_su_bn_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* MU BN PPDU Count */
	if (ath12k_fill_mu_bn_ppdu_cnt(vendor_event, link_peer_stats))
		return -EINVAL;

	/* Punctured BW */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_PUNC_BW);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: punc bw");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_PUNC_BW_MAX &&
	     i < MAX_PUNCTURED_MODE; i++) {
		if (nla_put_u32(vendor_event, (i + 1), tx_ppdu_stats->punc_bw[i])) {
			ath12k_err(NULL, "nla put failure: punc bw %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	/* RTS Success */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RTS_SUCCESS,
			tx_ppdu_stats->rts_success)) {
		ath12k_err(NULL, "nla put failed: rts success");
		return -EINVAL;
	}

	/* RTS Failure */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_RTS_FAILURE,
			tx_ppdu_stats->rts_failure)) {
		ath12k_err(NULL, "nla put failed: rts failure");
		return -EINVAL;
	}

	/* BAR Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_BAR_CNT,
			tx_ppdu_stats->bar_cnt)) {
		ath12k_err(NULL, "nla put failed: bar cnt");
		return -EINVAL;
	}

	/* NDPA Count */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_NDPA_CNT,
			tx_ppdu_stats->ndpa_cnt)) {
		ath12k_err(NULL, "nla put failed: ndpa cnt");
		return -EINVAL;
	}

	/* MSDU Flush Reason */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_MSDU_FLUSH_RSN);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx msdu flush rsn");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_HTT_FLUSH_MAX &&
	     i < HTT_FLUSH_MAX; i++) {
		if (nla_put_u32(vendor_event, (i + 1),
				tx_ppdu_stats->tx_msdu_flush_rsn[i])) {
			ath12k_err(NULL, "nla put failure: tx msdu flush rsn %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}

	nla_nest_end(vendor_event, attr);

	/* TX Duration Per AC */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_WLAN_TELEMETRY_TX_PPDU_STATS_DUR_AC);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx dur ac");
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX &&
	     i < WME_NUM_AC; i++) {
		if (nla_put_u64_64bit(vendor_event, (i + 1),
				      tx_ppdu_stats->tx_dur_ac[i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: tx dur ac %d", i);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
static int
ath12k_fill_peer_mmesh_tx_stats_wrapper(struct sk_buff *vendor_event,
					struct ath12k_dp_peer_stats *peer_stats)
{
	struct	nlattr *attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_MMESH_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx Ext Htt stats");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_NOQOS,
			peer_stats->mmesh_stat.no_qos)) {
		ath12k_err(NULL, "nla put failed: mesh no qos");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_NOENC,
			peer_stats->mmesh_stat.no_enc)) {
		ath12k_err(NULL, "nla put failed: mesh no enc");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_TXINFO,
			peer_stats->mmesh_stat.txinfo)) {
		ath12k_err(NULL, "nla put failed: txinfo");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_AUTORATE,
			peer_stats->mmesh_stat.auto_rate)) {
		ath12k_err(NULL, "nla put failed: auto_rate");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_TOFW,
			peer_stats->mmesh_stat.tofw)) {
		ath12k_err(NULL, "nla put failed: mesh tofw");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_TXSTATS_MMESH_DIRECT,
			peer_stats->mmesh_stat.direct)) {
		ath12k_err(NULL, "nla put failed: mesh direct");
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_mmesh_rx_stats_wrapper(struct sk_buff *vendor_event,
					struct ath12k_dp_peer_stats *peer_stats)
{
	struct	nlattr *attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MMESH_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx Ext Htt stats");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXHDR_UPDT,
			peer_stats->mmesh_stat.rxhdr_updt)) {
		ath12k_err(NULL, "nla put failed: rxhdr updt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXFILTDROP,
			peer_stats->mmesh_stat.filter_drop)) {
		ath12k_err(NULL, "nla put failed: filter drop");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXKEY_LOOKUP_FAIL,
			peer_stats->mmesh_stat.rxkey_lookp_up_fail)) {
		ath12k_err(NULL, "nla put failed: key lookup fail");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXKEY_LOOKUP_SUCC,
			peer_stats->mmesh_stat.rxkey_lookp_up_succ)) {
		ath12k_err(NULL, "nla put failed: key lookup succ");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_RXSTATS_MMESH_RXHDR_ALLOC_FAIL,
			peer_stats->mmesh_stat.rxhdr_alloc_fail)) {
		ath12k_err(NULL, "nla put failed: rxhdr alloc");
		return -EINVAL;
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}
#endif

static int
ath12k_fill_peer_tx_ppdu_stats_wrapper(struct ath12k *ar,
				       struct sk_buff *vendor_event,
				       struct ath12k_dp_link_peer_stats *link_peer_stats,
				       int peer_type)
{
	struct nlattr *attr;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PPDU_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx Ext Htt stats");
		return -EINVAL;
	}

	if (link_peer_stats->tx_ppdu_stats) {
		if (ath12k_fill_peer_tx_ppdu_stats_attr(ar, vendor_event,
							link_peer_stats,
							peer_type)) {
			ath12k_err(NULL, "Error filling peer extended HTT TX stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_tx_dbg_stats_attrs(struct sk_buff *vendor_event,
				    struct ath12k_dp_peer_stats *peer_stats,
				    bool is_extended, bool is_hw_stats)
{
	struct nlattr *attr;
	int reason;

	/* HW peer stats report these counters OOB without extended stats.
	 * Non-HW targets keep the existing extended-stats gated output below.
	 */
	if (is_hw_stats) {
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
				peer_stats->tx_dbg.retry_count)) {
			ath12k_err(NULL, "nla put failure: tx dbg retry_count");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
				peer_stats->tx_dbg.total_msdu_retries)) {
			ath12k_err(NULL, "nla put failure: tx dbg total_msdu_retries");
			return -EINVAL;
		}
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
		if (!attr) {
			ath12k_err(NULL, "nla nest failure: tx dbg ucast");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
				peer_stats->tx_dbg.ucast.packets)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d packets",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
				      peer_stats->tx_dbg.ucast.bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d bytes",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr);
	}

	/* Advance Stats */
	if (!is_extended)
		return 0;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx dbg wbm rel reason");
		return -EINVAL;
	}

	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX &&
	     reason < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				peer_stats->tx_dbg.wbm_rel_reason[reason])) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
				   reason + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: tx dbg tqm rel reason");
		return -EINVAL;
	}
	for (reason = 0; reason < QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX &&
	     reason < HAL_WBM_TQM_REL_REASON_MAX; reason++) {
		if (nla_put_u32(vendor_event, reason + 1,
				peer_stats->tx_dbg.tqm_rel_reason[reason])) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d subtype %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON,
				   reason + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
			peer_stats->tx_dbg.release_src_not_tqm)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM);
		return -EINVAL;
	}

	if (!is_hw_stats) {
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
				peer_stats->tx_dbg.retry_count)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT);
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
				peer_stats->tx_dbg.total_msdu_retries)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES);
			return -EINVAL;
		}
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
		if (!attr) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats - ucast");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
				peer_stats->tx_dbg.ucast.packets)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d packets",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
				      peer_stats->tx_dbg.ucast.bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d bytes",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr);
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
			peer_stats->tx_dbg.multiple_retry_count)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT);
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA,
			peer_stats->tx_dbg.ofdma)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA);
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
			peer_stats->tx_dbg.amsdu_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT);
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
			peer_stats->tx_dbg.non_amsdu_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT);
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
			peer_stats->tx_dbg.inval_link_id_pkt_cnt)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT);
		return -EINVAL;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - mcast");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx_dbg.mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d packets",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx_dbg.mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d bytes",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_MCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - bcast");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx_dbg.bcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d packets",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx_dbg.bcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx dbg stats attr %d bytes",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_BCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_proto_rx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_peer_stats *peer_stats,
						 u8 level)
{
	struct nlattr *attr;
	int i, j;
	u32 stats = 0;

	if (!peer_stats->proto)
		return -EINVAL;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Rx L3 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX &&
	     i < DP_PKT_TYPE_L3_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l3[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Rx L3 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Rx L4 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX &&
	     i < DP_PKT_TYPE_L4_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l4[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Rx L4 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Rx L5 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX &&
	     i < DP_PKT_TYPE_L5_MAX; i++) {
		for (j = 0; j < DP_REO_DST_RING_MAX; j++)
			stats = stats + peer_stats->proto->rx[j][level].l5[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Rx L5 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_peer_proto_tx_stats_attrs(struct sk_buff *vendor_event,
						 struct ath12k_dp_peer_stats *peer_stats,
						 u8 level)
{
	struct nlattr *attr;
	int i, j;
	u32 stats = 0;

	if (!peer_stats->proto)
		return -EINVAL;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx L3 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX &&
	     i < DP_PKT_TYPE_L3_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l3[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Tx L3 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx L4 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX &&
	     i < DP_PKT_TYPE_L4_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l4[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Tx L4 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Tx L5 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX &&
	     i < DP_PKT_TYPE_L5_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++)
			stats = stats + peer_stats->proto->tx[j][level].l5[i];

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: Tx L5 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_tx_per_pkt_stats_attrs(struct sk_buff *vendor_event,
					struct ath12k_dp_peer_stats *peer_stats,
					int ring_num)
{
	struct nlattr *attr;

	/* Basic Stats */
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - tx comp");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].comp_pkt.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].comp_pkt.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - tx success");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->tx[ring_num].tx_success.packets)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->tx[ring_num].tx_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED,
			peer_stats->tx[ring_num].tx_failed)) {
		ath12k_err(NULL, "nla put failure: Peer tx per pkt stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED);
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_peer_proto_rx_stats(struct ath12k *ar,
					   struct sk_buff *vendor_event,
					   struct ath12k_dp_peer_stats *peer_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8 level;

	/*Peer Proto Rx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_RX);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_RX_MAX &&
	     level < RX_RECV_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (!attr1) {
			ath12k_err(NULL, "nla nest failure: Rx proto stats level %u",
				   level + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (ath12k_fill_peer_proto_rx_stats_attrs(vendor_event, peer_stats,
							  level)) {
			ath12k_err(NULL, "Error filling Rx proto stats");
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_peer_proto_tx_stats(struct ath12k *ar,
					   struct sk_buff *vendor_event,
					   struct ath12k_dp_peer_stats *peer_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8  level;

	/*Peer Proto Tx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_TX);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_TX_COMP_MAX &&
	     level < TX_COMP_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (!attr1) {
			ath12k_err(NULL, "nla nest failure: Tx proto stats level %u",
				   level + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (ath12k_fill_peer_proto_tx_stats_attrs(vendor_event, peer_stats,
							  level)) {
			ath12k_err(NULL, "Error filling Tx proto stats");
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_peer_proto_stats(struct ath12k *ar,
					struct sk_buff *vendor_event,
					struct ath12k_dp_peer_stats *peer_stats)
{
	if (ath12k_fill_peer_proto_tx_stats(ar,
					    vendor_event,
					    peer_stats)) {
		ath12k_err(NULL, "NL failure: Proto tx stats");
		return -EINVAL;
	}

	if (ath12k_fill_peer_proto_rx_stats(ar,
					    vendor_event,
					    peer_stats)) {
		ath12k_err(NULL, "NL failure: Proto rx stats");
		return -EINVAL;
	}
	return 0;
}

/**
 * ath12k_fill_peer_hw_tx_stats() - Pack HW TX stats into Ring 0's per-pkt stats
 *				    nested attribute.
 *
 * Called only when ring_num is 0 and HW offload stats are enabled.
 * Return: Invalid when failed, 0 when successful.
 */
static int
ath12k_fill_peer_hw_tx_stats(struct sk_buff *vendor_event,
			     struct ath12k_dp_link_peer_stats *link_peer_stats,
			     struct ath12k_dp_mld_peer_stats *mld_stats,
			     int peer_type)
{
	struct ath12k_dp_link_peer_hw_tx_stats *hw_link_tx;
	struct ath12k_dp_peer_hw_tx_stats *hw_tx;

	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		if (!link_peer_stats || !link_peer_stats->hw_link_stats) {
			ath12k_err(NULL,
				   " Invalid link_peer_stats: ptr:%p hw_link_stats:%p\n",
				   link_peer_stats,
				   link_peer_stats ? link_peer_stats->hw_link_stats :
				   NULL);
			return -EINVAL;
		}

		hw_link_tx = &link_peer_stats->hw_link_stats->hw_link_tx;

		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_SUM_ACK_RSSI,
				      hw_link_tx->sum_ack_rssi, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: TX SUM_ACK_RSSI");
			return -EINVAL;
		}

		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_SUM_PHY_RATE,
				      hw_link_tx->sum_phy_rate, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: TX SUM_PHY_RATE");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_ACKED_PPDU_COUNT,
				hw_link_tx->acked_ppdu_count)) {
			ath12k_err(NULL, "nla put failure: TX ACKED_PPDU_COUNT");
			return -EINVAL;
		}
	}

	if (peer_type != ATH12K_LINK_PEER) {
		if (!mld_stats || !mld_stats->hw_stats) {
			ath12k_err(NULL,
				   "Invalid mld_stats: ptr:%p hw_stats:%p\n",
				   mld_stats,
				   mld_stats ? mld_stats->hw_stats : NULL);
			return -EINVAL;
		}

		hw_tx = &mld_stats->hw_stats->hw_tx;

		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED_BYTES,
				      hw_tx->failed_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: TX FAILED_BYTES");
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP_BYTES,
				      hw_tx->drop_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: TX DROP_BYTES");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP1_PKTS,
				hw_tx->drop1_pkts)) {
			ath12k_err(NULL, "nla put failure: TX DROP1_PKTS");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_TX_DROP2_PKTS,
				hw_tx->drop2_pkts)) {
			ath12k_err(NULL, "nla put failure: TX DROP2_PKTS");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ath12k_fill_tid_delay_stats() - Serialize a hist_stats delay structure
 * @vendor_event: netlink skb
 * @delay: pointer to the hist_stats to serialize
 * @attr_id: the TID TX attribute ID to use as the outer nest
 *           (QCA_VENDOR_ATTR_TID_TX_SWQ_DELAY, _HWTX_DELAY, or _INTFRM_DELAY)
 *
 * Emits:
 *   <attr_id>
 *     QCA_VENDOR_ATTR_TID_DELAY_MAX_VAL  (u32)
 *     QCA_VENDOR_ATTR_TID_DELAY_MIN_VAL  (u32)
 *     QCA_VENDOR_ATTR_TID_DELAY_AVG_VAL  (u32)
 *     QCA_VENDOR_ATTR_TID_DELAY_HIST     (nested)
 *       1 .. HIST_BUCKET_MAX  (u32 each)
 *   </attr_id>
 *
 * Return: 0 on success, -EINVAL on failure.
 */
static int ath12k_fill_tid_delay_stats(struct sk_buff *vendor_event,
				       const struct hist_stats *delay,
				       int attr_id)
{
	struct nlattr *delay_attr;
	struct nlattr *hist_attr;
	int i;

	delay_attr = nla_nest_start(vendor_event, attr_id);
	if (!delay_attr)
		return -EINVAL;

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TID_DELAY_MAX_VAL,
			delay->max) ||
	    nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TID_DELAY_MIN_VAL,
			delay->min) ||
	    nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TID_DELAY_AVG_VAL,
			delay->avg)) {
		nla_nest_cancel(vendor_event, delay_attr);
		return -EINVAL;
	}

	hist_attr = nla_nest_start(vendor_event,
				   QCA_VENDOR_ATTR_TID_DELAY_HIST);
	if (!hist_attr) {
		nla_nest_cancel(vendor_event, delay_attr);
		return -EINVAL;
	}

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_MAX &&
	     i < ARRAY_SIZE(delay->hist.freq); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				delay->hist.freq[i])) {
			nla_nest_cancel(vendor_event, hist_attr);
			nla_nest_cancel(vendor_event, delay_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, hist_attr);
	nla_nest_end(vendor_event, delay_attr);

	return 0;
}

/**
 * ath12k_fill_peer_delay_stats() - Fill per-TID delay stats into NL vendor event
 * @ar: ath12k radio pointer (unused, kept for consistency with other fill functions)
 * @vendor_event: sk_buff to write into
 * @peer_stats: peer stats containing the delay pointer
 *
 * Serializes peer_stats->delay (struct ath12k_dp_peer_tid_agg_delay_stats) into
 * the vendor event buffer. For each TID (0..DP_TID_MAX-1) it emits:
 *   - QCA_VENDOR_ATTR_DELAY_STATS_TX_SWQ  (nested hist_stats)
 *   - QCA_VENDOR_ATTR_DELAY_STATS_TX_HW   (nested hist_stats)
 *   - QCA_VENDOR_ATTR_DELAY_STATS_RX_TO_STACK (nested hist_stats)
 *
 * Returns 0 on success, negative error code on failure.
 * If peer_stats->delay is NULL the function returns 0 immediately.
 */
static int ath12k_fill_peer_delay_stats(struct ath12k *ar,
					struct sk_buff *vendor_event,
					struct ath12k_dp_mld_peer_stats *mld_stats)
{
	struct ath12k_dp_peer_tid_agg_delay_stats *delay;
	struct ath12k_dp_peer_delay_tid_stats *tid_stats;
	struct nlattr *tid_attr;
	int tid;
	int ret;

	if (!mld_stats || !mld_stats->agg_mld_stats)
		return 0;

	delay = &mld_stats->agg_mld_stats->delay;

	for (tid = 0; tid < DP_TID_MAX &&
	     tid < QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS; tid++) {
		tid_stats = &delay->tid_stats[tid];

		tid_attr = nla_nest_start(vendor_event, tid + 1);
		if (!tid_attr) {
			ath12k_err(NULL, "nla nest failure: delay stats TID %d", tid);
			return -EINVAL;
		}

		ret = ath12k_fill_tid_delay_stats(vendor_event,
						  &tid_stats->tx_delay.tx_swq_delay,
						  QCA_VENDOR_ATTR_DELAY_STATS_TX_SWQ);
		if (ret) {
			ath12k_err(NULL, "Error filling TX SWQ delay for TID %d", tid);
			nla_nest_cancel(vendor_event, tid_attr);
			return ret;
		}

		ret = ath12k_fill_tid_delay_stats(vendor_event,
						  &tid_stats->tx_delay.hwtx_delay,
						  QCA_VENDOR_ATTR_DELAY_STATS_TX_HW);
		if (ret) {
			ath12k_err(NULL, "Error filling TX HW delay for TID %d", tid);
			nla_nest_cancel(vendor_event, tid_attr);
			return ret;
		}

		ret =
		    ath12k_fill_tid_delay_stats(vendor_event,
						&tid_stats->rx_delay.to_stack_delay,
						QCA_VENDOR_ATTR_DELAY_STATS_RX_TO_STACK);
		if (ret) {
			ath12k_err(NULL,
				   "Error filling RX to-stack delay for TID %d", tid);
			nla_nest_cancel(vendor_event, tid_attr);
			return ret;
		}

		nla_nest_end(vendor_event, tid_attr);
	}

	return 0;
}

/**
 * ath12k_fill_peer_jitter_stats() - Fill Jitter stats into NL vendor event
 * @ar: ath12k radio pointer (unused, kept for consistency with other fill functions)
 * @vendor_event: sk_buff to write into
 * @peer_stats: peer stats containing the delay pointer
 *
 * Returns 0 on success, negative error code on failure.
 * If peer_stats->jitter is NULL the function returns 0 immediately.
 */
static int ath12k_fill_peer_jitter_stats(struct ath12k *ar,
					 struct sk_buff *vendor_event,
					 struct ath12k_dp_mld_peer_stats *mld_stats)
{
	struct nlattr *tid_attr;
	struct ath12k_dp_peer_tid_agg_jitter_stats *jitter;
	int tid;

	if (!mld_stats || !mld_stats->agg_mld_stats)
		return 0;

	jitter = &mld_stats->agg_mld_stats->jitter;

	for (tid = 0; tid < QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS &&
	     tid < DP_TID_MAX; tid++) {
		tid_attr = nla_nest_start(vendor_event, tid + 1);
		if (!tid_attr) {
			ath12k_err(NULL, "nla nest failure: jitter stats TID %d", tid);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_JITTER,
				jitter->tid_stats[tid].tx_avg_jitter) ||
		    nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_DELAY,
				jitter->tid_stats[tid].tx_avg_delay) ||
		    nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_JITTER_STATS_TX_AVG_ERR,
				      jitter->tid_stats[tid].tx_avg_err,
				      NL80211_ATTR_PAD) ||
		    nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_JITTER_STATS_TX_TOTAL_SUCCESS,
				      jitter->tid_stats[tid].tx_total_success,
				      NL80211_ATTR_PAD) ||
		    nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_JITTER_STATS_TX_DROP,
				      jitter->tid_stats[tid].tx_drop,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: jitter stats attr");
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, tid_attr);
	}

	return 0;
}

static int ath12k_fill_peer_sojourn_stats(struct ath12k *ar,
					  struct sk_buff *vendor_event,
					  struct ath12k_dp_mld_peer_stats *mld_stats)
{
	struct ath12k_dp_peer_tid_sojourn_stats *tid_stats;
	struct ath12k_dp_peer_tid_agg_sojourn_stats *sojourn;
	struct nlattr *tid_attr;
	int tid;
	u64 avg_sojourn_msdu;

	if (!mld_stats || !mld_stats->agg_mld_stats)
		return 0;

	sojourn = &mld_stats->agg_mld_stats->sojourn;

	for (tid = 0; tid < QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS &&
	     tid < DP_TID_MAX; tid++) {
		tid_stats = &sojourn->tid_stats[tid];
		tid_attr = nla_nest_start(vendor_event, tid + 1);
		if (!tid_attr) {
			ath12k_err(NULL, "nla nest failure: sojourn stats TID %d", tid);
			return -EINVAL;
		}

		avg_sojourn_msdu = ewma_avg_sojourn_read(&tid_stats->avg_sojourn_msdu);

		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_SOJOURN_STATS_SUM_SOJOURN_MSDU,
				tid_stats->sum_sojourn_msdu) ||
		    nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_SOJOURN_STATS_NUM_MSDUS,
				tid_stats->num_msdus) ||
		    nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_SOJOURN_STATS_AVG_SOJOURN_MSDU,
				      avg_sojourn_msdu, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: sojourn stats attr");
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}

		nla_nest_end(vendor_event, tid_attr);
	}

	return 0;
}

static int ath12k_fill_peer_tx_stats(struct ath12k *ar,
				     struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     bool is_extended,
				     struct ath12k_dp_link_peer_stats *link_peer_stats,
				     struct ath12k_dp_mld_peer_stats *mld_stats,
				     int peer_type)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;
	bool is_hw_stats = false;


	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp))
		is_hw_stats = true;

	/*Tx Per Pkt Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer per pkt stats");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX &&
	     ring_num < DP_TCL_NUM_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats - ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (ath12k_fill_peer_tx_per_pkt_stats_attrs(vendor_event, peer_stats,
							    ring_num)) {
			ath12k_err(NULL, "Error filling peer tx per pkt stats for ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		/* HW offload TX stats — Ring 0 only */
		if (((ring_num + 1) == QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_0) &&
		    ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
			if (ath12k_fill_peer_hw_tx_stats(vendor_event,
							 link_peer_stats,
							 mld_stats,
							 peer_type)) {
				ath12k_err(NULL, "Error filling peer HW TX stats");
				nla_nest_end(vendor_event, attr1);
				nla_nest_end(vendor_event, attr);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	/* Tx Debug Stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_DBG_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer tx dbg stats");
		return -EINVAL;
	}
	if (ath12k_fill_peer_tx_dbg_stats_attrs(vendor_event, peer_stats,
						is_extended, is_hw_stats)) {
		ath12k_err(NULL, "Error filling peer tx dbg stats");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/* Tx PPDU Stats */
	if (link_peer_stats &&
	    ath12k_htt_tx_ppdu_stats_enabled(&ar->dp)) {
		if (ath12k_fill_peer_tx_ppdu_stats_wrapper(ar, vendor_event,
							   link_peer_stats,
							   peer_type)) {
			return -EINVAL;
		}
	}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	if (ath12k_fill_peer_mmesh_tx_stats_wrapper(vendor_event, peer_stats)) {
		ath12k_err(NULL, "Error filling  mmesh stats\n");
		return -EINVAL;
	}
#endif

	return 0;
}

static int
ath12k_fill_vap_proto_tx_stats_attrs(struct sk_buff *vendor_event,
				     struct ath12k_dp_aggr_vif_stats *vif_stats,
				     u8 level)
{
	struct nlattr *attr;
	int i, j;
	u32 stats = 0;

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L3);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vap Tx L3 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L3_MAX &&
	     i < DP_PKT_TYPE_L3_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++) {
			if (!vif_stats->stats[j].proto)
				continue;
			stats = stats + vif_stats->stats[j].proto->tx[level].l3[i];
		}

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: vap Tx L3 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L4);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vap Tx L4 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L4_MAX &&
	     i < DP_PKT_TYPE_L4_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++) {
			if (!vif_stats->stats[j].proto)
				continue;
			stats = stats + vif_stats->stats[j].proto->tx[level].l4[i];
		}

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: vap Tx L4 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	stats = 0;
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_L5);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vap Tx L5 stats");
		return -EINVAL;
	}

	for (i = 0; i < QCA_VENDOR_ATTR_PROTO_STATS_L5_MAX &&
	     i < DP_PKT_TYPE_L5_MAX; i++) {
		for (j = 0; j < DP_TCL_NUM_RING_MAX; j++) {
			if (!vif_stats->stats[j].proto)
				continue;
			stats = stats + vif_stats->stats[j].proto->tx[level].l5[i];
		}

		if (nla_put_u32(vendor_event, i + 1, stats)) {
			ath12k_err(NULL, "nla put failed: vap Tx L5 stats");
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		stats = 0;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_vap_proto_tx_stats(struct ath12k *ar,
					  struct sk_buff *vendor_event,
					  struct ath12k_dp_aggr_vif_stats *vif_stats)
{
	struct nlattr *attr;
	struct nlattr *attr1;
	u8  level;

	/*Peer Proto Tx Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PROTO_STATS_EVENT_VAP);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer pproto stats");
		return -EINVAL;
	}

	for (level = 0; level < QCA_VENDOR_ATTR_PROTO_STATS_TX_ENQ_MAX &&
	     level < TX_ENQUEUE_MAX; level++) {
		attr1 = nla_nest_start(vendor_event, level + 1);
		if (!attr1) {
			ath12k_err(NULL, "nla nest failure: vap proto stats level %u",
				   level + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (ath12k_fill_vap_proto_tx_stats_attrs(vendor_event, vif_stats,
		    level)) {
			ath12k_err(NULL, "Error filling Tx proto stats");
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);
	return 0;
}

static int ath12k_fill_vap_proto_stats(struct ath12k *ar,
				       struct sk_buff *vendor_event,
				       struct ath12k_telemetry_dp_vif *vif_stats)
{
	int ret;

	/* Aggregated Peer Proto  Stats */
	ret = ath12k_fill_peer_proto_stats(ar, vendor_event,
					   &(vif_stats->aggr_vif_stats.peer_stats));
	if (ret) {
		ath12k_err(NULL, "Error filling vap tx stats");
		return ret;
	}

	/* Vap proto stats */
	ret = ath12k_fill_vap_proto_tx_stats(ar, vendor_event,
					     &vif_stats->aggr_vif_stats);
	return ret;
}

static int ath12k_fill_peer_rx_wbm_err_attrs(struct sk_buff *vendor_event,
					     struct ath12k_dp_peer_stats *peer_stats)
{
	if (ath12k_fill_rxdma_err_attrs(NULL, vendor_event,
					peer_stats->wbm_err.rxdma_error)) {
		ath12k_err(NULL, "Error filling peer rxdma error");
		return -EINVAL;
	}

	if (ath12k_fill_reo_err_attrs(NULL, vendor_event,
				      peer_stats->wbm_err.reo_error)) {
		ath12k_err(NULL, "Error filling peer reo error");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_fill_peer_rx_per_pkt_stats_attrs(struct sk_buff *vendor_event,
						   struct ath12k_dp_peer_stats *peer_stats,
						   int ring_num)
{
	struct nlattr *attr;

	/* Basic Stats */
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx from reo");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].recv_from_reo.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].recv_from_reo.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx to stack");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].sent_to_stack.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx to stack fast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx[ring_num].sent_to_stack_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx[ring_num].sent_to_stack_fast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes | ring %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
			   ring_num + 1);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int
ath12k_fill_peer_rx_dbg_stats_attrs(struct sk_buff *vendor_event,
				    struct ath12k_dp_peer_stats *peer_stats,
				    bool is_extended, bool is_hw_stats)
{
	struct nlattr *attr;

	/* HW peer stats report these counters OOB without extended stats.
	 * Non-HW targets keep the existing extended-stats gated output below.
	 */
	if (is_hw_stats) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
		if (!attr) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats - rx ucast hw");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
				peer_stats->rx_dbg.ucast.packets)) {
			ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d packets",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
				      peer_stats->rx_dbg.ucast.bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d bytes",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr);
	}

	/* Advance Stats */
	if (!is_extended)
		return 0;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx mcast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx_dbg.mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d packets",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx_dbg.mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d bytes",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_MCAST);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	if (!is_hw_stats) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
		if (!attr) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats - rx ucast");
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
				peer_stats->rx_dbg.ucast.packets)) {
			ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d packets",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
				      peer_stats->rx_dbg.ucast.bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d bytes",
				   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_UCAST);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr);
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
			peer_stats->rx_dbg.non_amsdu)) {
		ath12k_err(NULL, "nla put failure: Peer rx dbg stats attr %d",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU);
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
			peer_stats->rx_dbg.msdu_part_of_amsdu)) {
		ath12k_err(NULL, "nla put failure: Peer rx dbg stats - msdu_part_of_amsdu");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,
			peer_stats->rx_dbg.mpdu_retry)) {
		ath12k_err(NULL, "nla put failure: Peer rx dbg stats - mpdu_retry");
		return -EINVAL;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_SG);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats - rx sg");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			peer_stats->rx_dbg.sg.packets)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d packets",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_SG);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      peer_stats->rx_dbg.sg.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Peer rx per pkt stats attr %d bytes",
			   QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_SG);
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

/**
 * ath12k_vendor_fill_rx_wme_ac_stats_pkts() - Serialize WME AC packet counts
 * @skb: Socket buffer for netlink message
 * @wme_ac_type: Array of per-AC packet counts (u32[WME_NUM_AC])
 *
 * Fills a flat u32 array (one entry per AC, 1-based attribute IDs) into the
 * already-started nest for QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE (attr 22).
 * The caller is responsible for nla_nest_start/end.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int ath12k_vendor_fill_rx_wme_ac_stats_pkts(struct sk_buff *skb,
						   u32 *wme_ac_type_pkts)
{
	int i;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX &&
	     i < WME_NUM_AC; i++) {
		if (nla_put_u32(skb, i + 1, wme_ac_type_pkts[i])) {
			ath12k_err(NULL, "nla put failure: rx wme pkt attr error");
			return -EMSGSIZE;
		}
	}
	return 0;
}

/**
 * ath12k_vendor_fill_rx_wme_ac_bytes() - Serialize WME AC byte counts
 * @skb: Socket buffer for netlink message
 * @wme_ac_type_bytes: Array of per-AC byte counts (u64[WME_NUM_AC])
 *
 * Fills a flat u64 array (one entry per AC, 1-based attribute IDs) into the
 * already-started nest for QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE_BYTES
 * (attr 23). The caller is responsible for nla_nest_start/end.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int ath12k_vendor_fill_rx_wme_ac_bytes(struct sk_buff *skb,
					      u64 *wme_ac_type_bytes)
{
	int i;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX &&
	     i < WME_NUM_AC; i++) {
		if (nla_put_u64_64bit(skb, i + 1, wme_ac_type_bytes[i],
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: rx wme byte attr error");
			return -EMSGSIZE;
		}
	}
	return 0;
}

/**
 * ath12k_vendor_fill_rx_signal_stats() - Serialize RX signal statistics
 * @skb: Socket buffer for netlink message
 * @stats: Per-peer RX signal statistics
 * @ar: radio handle, used to read rssi_offsets (nf/temp/xlna fields)
 *
 * Serializes both the derived SNR/RSSI values and the five raw inputs
 * that feed ath12k_dp_get_rssi_value():
 *   rssi_region_offset, bw_offset, avg_nf_dbm, rssi_temp_offset,
 *   xlna_bypass_offset, xlna_bypass_threshold
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int
ath12k_vendor_fill_rx_signal_stats(struct sk_buff *skb,
				   struct ath12k_dp_link_peer_rx_signal_stats *stats,
				   struct ath12k *ar)
{
	u8 bw_offset = ath12k_dp_get_bw_offset(stats->channel_bw);

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR,
		       stats->snr) ||
	    nla_put_u16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_AVG,
			stats->snr_avg) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP,
		       stats->snr_dp) ||
	    nla_put_u16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_SNR_DP_AVG,
			stats->snr_dp_avg) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI,
		       stats->rssi) ||
	    nla_put_s16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_AVG,
			stats->rssi_avg) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP,
		       stats->rssi_dp) ||
	    nla_put_s16(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_DP_AVG,
			stats->rssi_dp_avg) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_REGION_OFFSET,
		       stats->rssi_region_offset) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_BW_OFFSET,
		       bw_offset) ||
	    nla_put_s8(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_AVG_NF_DBM,
		       ar->rssi_offsets.avg_nf_dbm) ||
	    nla_put_s32(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_RSSI_TEMP_OFFSET,
			ar->rssi_offsets.rssi_temp_offset) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_XLNA_BYPASS_OFF,
			ar->rssi_offsets.xlna_bypass_offset) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_RX_MON_SIGNAL_STATS_XLNA_BYPASS_THR,
			ar->rssi_offsets.xlna_bypass_threshold))
		return -EMSGSIZE;

	return 0;
}

static int ath12k_put_rx_mu_stats(struct sk_buff *skb,
				   struct ath12k_rx_peer_user_stats *rx_mu)
{
	struct nlattr *user, *ppdu_nss, *mcs;
	int i, j;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_ATTR_USER_TYPE_MAX &&
	     i < TXRX_TYPE_MU_MAX; i++) {
		user = nla_nest_start(skb, i + 1); /* use index as nested tag */
		if (!user)
			return -EMSGSIZE;

		nla_put_u32(skb, QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_OK,
			    rx_mu[i].mpdu_cnt_fcs_ok);
		nla_put_u32(skb, QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MPDU_ERR,
			    rx_mu[i].mpdu_cnt_fcs_err);

		ppdu_nss = nla_nest_start(skb,
					  QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_PPDU_NSS);
		if (!ppdu_nss)
			return -EMSGSIZE;

		for (j = 0; j < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX &&
		     j < HAL_RX_MAX_NSS; j++)
			nla_put_u32(skb, j + 1, rx_mu[i].ppdu_nss[j]);
		nla_nest_end(skb, ppdu_nss);

		mcs = nla_nest_start(skb,
				     QCA_VENDOR_WLAN_TELEMETRY_ATTR_RX_MU_MCS_COUNTS);
		if (!mcs)
			return -EMSGSIZE;

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX &&
		     j < MAX_MCS; j++)
			nla_put_u32(skb, j + 1,
				    rx_mu[i].ppdu.mcs_count[j]);

		nla_nest_end(skb, mcs);

		nla_nest_end(skb, user);
	}
	return 0;
}

/**
 * ath12k_vendor_fill_rx_mon_stats() - Serialize RX monitor statistics
 * @skb: Socket buffer for netlink message
 * @rx_stats: Pointer to RX peer stats structure
 * @is_extended: Boolean flag to control serialization of extended statistics
 *
 * Serializes RX monitor statistics to netlink attributes. Basic counters are
 * always serialized. Extended statistics (large arrays and detailed breakdowns)
 * are only serialized when is_extended is true.
 *
 * Return: 0 on success, -EMSGSIZE if buffer space insufficient
 */
static int ath12k_vendor_fill_rx_mon_stats(struct sk_buff *skb,
					   struct ath12k_rx_ppdu_stats *rx_stats,
					   bool is_extended,
					   int peer_type, struct ath12k *ar)
{
	struct nlattr *recep_attr, *gi_count_attr, *nss_count_attr;
	struct nlattr *bw_count_attr;
	struct nlattr *signal_stat_attr;
	struct nlattr *ppdu_nss_attr, *punc_bw_attr, *su_ppdu_cnt_attr;
	struct nlattr *nla_wme_pkts, *ppdu_cnt_attr, *nla_wme_bytes;
	struct nlattr *pkt_type_attr, *mu_stats;
	struct nlattr *pkt_type_nest, *pkt_type_mu_stats;
	int i, j;
	struct ath12k_dp_link_peer_rx_signal_stats *signal_stats =
							&rx_stats->signal_stats;
	u32 val;
	struct nlattr *ac_attr;
	int ac;

	/* Basic counters */
	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_OK,
			rx_stats->num_mpdu_fcs_ok) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_FCS_ERR,
			rx_stats->num_mpdu_fcs_err) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NON_AMPDU_MSDU_COUNT,
			rx_stats->non_ampdu_msdu_count) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_AMPDU_MSDU_COUNT,
			rx_stats->ampdu_msdu_count)) {
		ath12k_err(NULL, "nla failure: RX mon stats");
		return -EMSGSIZE;
	}

	if (peer_type == ATH12K_LEGACY_PEER ||
	    peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_DURATION,
				      rx_stats->rx_duration, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla failure: RX mon stats - rx duration");
			return -EMSGSIZE;
		}

		ac_attr = nla_nest_start(skb,
					 QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_DUR_AC);
		if (!ac_attr)
			return -EMSGSIZE;

		for (ac = 0; ac < QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_MAX &&
		     ac < WME_NUM_AC ; ac++) {
			if (nla_put_u64_64bit(skb,
					      QCA_VENDOR_WLAN_TELEMETRY_RATE_WME_AC_BE
					      + ac,
					      rx_stats->rx_duration_ac[ac],
					      NL80211_ATTR_PAD)) {
				ath12k_err(NULL, "nla failure: rx duration ac %d", ac);
				nla_nest_cancel(skb, ac_attr);
				return -EMSGSIZE;
			}
		}
		nla_nest_end(skb, ac_attr);
	}

	/* Reception type array */
	recep_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RECEPTION_TYPE);

	if (!recep_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon reception type");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MAX &&
	     i < HAL_RX_RECEPTION_TYPE_MAX; i++) {
		if (nla_put_u32(skb, (i + 1), rx_stats->reception_type[i])) {
			nla_nest_cancel(skb, recep_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, recep_attr);

	/* gi_count array */
	gi_count_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_GI_COUNT);
	if (!gi_count_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon GI count");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_GI_MAX &&
	     i < HAL_RX_GI_MAX; i++) {
		if (nla_put_u32(skb, (i + 1), rx_stats->gi_count[i])) {
			nla_nest_cancel(skb, gi_count_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, gi_count_attr);

	/* nss_count array */
	nss_count_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_COUNT);
	if (!nss_count_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon NSS count");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX &&
	     i < HAL_RX_MAX_NSS; i++) {
		if (nla_put_u32(skb, (i + 1), rx_stats->nss_count[i])) {
			nla_nest_cancel(skb, nss_count_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, nss_count_attr);

	/* bw_count array */
	bw_count_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_BW_COUNT);
	if (!bw_count_attr) {
		ath12k_err(NULL, "nla nest failure: RX mon BW count");
		return -EMSGSIZE;
	}
	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_BW_MAX &&
	     i < HAL_RX_BW_MAX; i++) {
		if (nla_put_u32(skb, (i + 1), rx_stats->bw_count[i])) {
			nla_nest_cancel(skb, bw_count_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, bw_count_attr);

	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU,
			rx_stats->num_mpdus) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_PPDU,
			rx_stats->num_ppdus)) {
		ath12k_err(NULL, "nla nest failure: RX mon basic bytes stats");
		return -EMSGSIZE;
	}
	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MPDU_RETRY_COUNT,
			rx_stats->num_mpdu_retry_count) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_MSDU_RETRY_COUNT,
			rx_stats->num_msdu_retry_count)) {
		ath12k_err(NULL, "nla nest failure: RX mon retry count stats");
		return -EMSGSIZE;
	}

	signal_stat_attr = nla_nest_start(skb,
					  QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_SIGNAL_STATS);
	if (!signal_stat_attr) {
		ath12k_err(NULL, "nla nest failure: RX signal stats");
		return -EMSGSIZE;
	}

	if (ath12k_vendor_fill_rx_signal_stats(skb, signal_stats, ar)) {
		ath12k_err(NULL, "nla put failure: RX signal stats");
		nla_nest_cancel(skb, signal_stat_attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, signal_stat_attr);

	/* Advanced stats - only reported when advance_stats is enabled */
	if (!ath12k_dp_advance_stats_enabled(&ar->dp))
		return 0;

	if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_BAR,
			rx_stats->num_bar) ||
		nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_NUM_NDPA,
			    rx_stats->num_ndpa)) {
		ath12k_err(NULL, "nla failure: Failed to put extended RX mon stats");
		return -EMSGSIZE;
	}

	ppdu_cnt_attr = nla_nest_start(skb,
				       QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_RECEPTION);
	if (!ppdu_cnt_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RECEP_MAX &&
	     i < HAL_RX_RECEPTION_TYPE_MAX; i++) {
		if (nla_put_u32(skb, i + 1, rx_stats->ppdu_reception[i])) {
			nla_nest_cancel(skb, ppdu_cnt_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, ppdu_cnt_attr);

	ppdu_nss_attr = nla_nest_start(skb,
				       QCA_VENDOR_ATTR_WLAN_TELEMETRY_PPDU_NSS);
	if (!ppdu_nss_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX &&
	     i < HAL_RX_MAX_NSS; i++) {
		if (nla_put_u32(skb, i + 1, rx_stats->ppdu_nss[i])) {
			nla_nest_cancel(skb, ppdu_nss_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, ppdu_nss_attr);

	/* per mcs pkt type stats */

	pkt_type_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_TYPE);
	if (!pkt_type_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PKT_TYPE_MAX &&
	     i < DOT11_MAX; i++) {
		pkt_type_nest = nla_nest_start(skb, i + 1);
		if (!pkt_type_nest) {
			nla_nest_cancel(skb, pkt_type_attr);
			return -EMSGSIZE;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX &&
		     j < MAX_MCS; j++) {
			val = rx_stats->proto_type[i].mcs_count[j];

			if (nla_put_u32(skb, j + 1, val)) {
				nla_nest_cancel(skb, pkt_type_nest);
				nla_nest_cancel(skb, pkt_type_attr);
				return -EMSGSIZE;
			}
		}

		nla_nest_end(skb, pkt_type_nest);
	}
	nla_nest_end(skb, pkt_type_attr);

	/* WME AC packet counts (flat u32 array, one per AC) */
	nla_wme_pkts = nla_nest_start(skb,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE_PKTS);
	if (!nla_wme_pkts)
		return -ENOBUFS;

	if (ath12k_vendor_fill_rx_wme_ac_stats_pkts(skb, rx_stats->wme_ac_type_pkts)) {
		nla_nest_cancel(skb, nla_wme_pkts);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, nla_wme_pkts);

	/* WME AC byte counts (flat u64 array, one per AC) */
	nla_wme_bytes = nla_nest_start(skb,
				       QCA_VENDOR_ATTR_WLAN_TELEMETRY_WME_AC_TYPE_BYTES);
	if (!nla_wme_bytes)
		return -ENOBUFS;

	if (ath12k_vendor_fill_rx_wme_ac_bytes(skb,
					       rx_stats->wme_ac_type_bytes)) {
		nla_nest_cancel(skb, nla_wme_bytes);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, nla_wme_bytes);

	/* su_ppdu_cnt stats */
	su_ppdu_cnt_attr = nla_nest_start(skb,
					  QCA_VENDOR_ATTR_WLAN_TELEMETRY_SU_PPDU_COUNT);
	if (!su_ppdu_cnt_attr)
		return -EMSGSIZE;

	for (i = 0; i < (QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PPDU_PKT_TYPE_MAX) &&
	     i < ATH12K_RX_PPDU_PROTO_MAX; i++) {
		pkt_type_nest = nla_nest_start(skb, i + 1);
		if (!pkt_type_nest) {
			nla_nest_cancel(skb, su_ppdu_cnt_attr);
			return -EMSGSIZE;
		}

		for (j = 0; j < QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX &&
		     j < MAX_MCS; j++) {
			val = rx_stats->su_ppdu_count[i].mcs_count[j];
			if (nla_put_u32(skb, j + 1, val)) {
				nla_nest_cancel(skb, pkt_type_nest);
				nla_nest_cancel(skb, su_ppdu_cnt_attr);
				return -EMSGSIZE;
			}
		}

		nla_nest_end(skb, pkt_type_nest);
	}
	nla_nest_end(skb, su_ppdu_cnt_attr);

	punc_bw_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PUNC_BW);
	if (!punc_bw_attr)
		return -EMSGSIZE;

	for (i = 0; i < QCA_VENDOR_WLAN_TELEMETRY_ATTR_PUNC_BW_MAX &&
	     i < MAX_PUNCTURED_MODE; i++) {
		if (nla_put_u32(skb, i + 1, rx_stats->punc_bw[i])) {
			nla_nest_cancel(skb, punc_bw_attr);
			return -EMSGSIZE;
		}
	}
	nla_nest_end(skb, punc_bw_attr);

	/* RX MU stats */
	mu_stats = nla_nest_start(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MU);
	if (!mu_stats)
		return -EMSGSIZE;

	for (i = 0; i < QCA_WLAN_VENDOR_ATTR_WLAN_TELEMETRY_RX_PPDU_PKT_TYPE_MAX &&
	     i < ATH12K_RX_PPDU_PROTO_MAX; i++) {
		pkt_type_mu_stats = nla_nest_start(skb, i + 1);
		if (!pkt_type_mu_stats) {
			nla_nest_cancel(skb, mu_stats);
			return -EMSGSIZE;
		}

		if (ath12k_put_rx_mu_stats(skb, rx_stats->rx_mu[i])) {
			nla_nest_cancel(skb, pkt_type_mu_stats);
			nla_nest_cancel(skb, mu_stats);
			return -EMSGSIZE;
		}
		nla_nest_end(skb, pkt_type_mu_stats);
	}
	nla_nest_end(skb, mu_stats);

	/* Rate stats */
	if (peer_type == ATH12K_LEGACY_PEER ||
	    peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_LAST_RX_RATE,
				rx_stats->last_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RND_AVG_RX_RATE,
				    rx_stats->rnd_avg_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_AVG_RX_RATE,
				    rx_stats->avg_rx_rate) ||
			nla_put_u32(skb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_RATECODE,
				    rx_stats->rx_ratecode)) {
			ath12k_err(NULL, "nla failure: extended RX mon rate stats");
			return -EMSGSIZE;
		}
	}

	return 0;
}

/**
 * ath12k_fill_peer_hw_rx_stats() - Pack HW RX stats into Ring 0's per-pkt
 *				    stats nested attribute.
 *
 * Called only when ring_num is 0 and HW stats are enabled.
 * Return: Invalid when failed, 0 when successful.
 */

static int
ath12k_fill_peer_hw_rx_stats(struct sk_buff *vendor_event,
			     struct ath12k_dp_link_peer_stats *link_peer_stats,
			     struct ath12k_dp_mld_peer_stats *mld_stats,
			     int peer_type)
{
	struct ath12k_dp_link_peer_hw_rx_stats *hw_link_rx;
	struct ath12k_dp_peer_hw_rx_stats *hw_rx;

	if (!link_peer_stats || !link_peer_stats->hw_link_stats) {
		ath12k_err(NULL, "Invalid link_peer_stats: ptr:%p hw_link_stats:%p\n",
			   link_peer_stats,
			   link_peer_stats ? link_peer_stats->hw_link_stats : NULL);
		return -EINVAL;
	}

	hw_link_rx = &link_peer_stats->hw_link_stats->hw_link_rx;

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_GCAST_BYTES,
			      hw_link_rx->success_gcast_bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: RX SUCCESS_GCAST_BYTES");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_GCAST_PKTS,
			hw_link_rx->success_gcast_pkts)) {
		ath12k_err(NULL, "nla put failure: RX SUCCESS_GCAST_PKTS");
		return -EINVAL;
	}
	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED_MPDU_BYTES,
			      hw_link_rx->failed_mpdu_bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: RX FAILED_MPDU_BYTES");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_RX_FAILED_MPDU,
			hw_link_rx->failed_mpdu)) {
		ath12k_err(NULL, "nla put failure: RX FAILED_MPDU");
		return -EINVAL;
	}
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP1_UCAST_PKTS,
			hw_link_rx->drop1_ucast_pkts)) {
		ath12k_err(NULL, "nla put failure: RX DROP1_UCAST_PKTS");
		return -EINVAL;
	}

	if (peer_type == ATH12K_LEGACY_PEER || peer_type == ATH12K_LINK_PEER) {
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUM_RSSI,
				      hw_link_rx->sum_rssi, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: RX SUM_RSSI");
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUM_PHY_RATE,
				      hw_link_rx->sum_phy_rate, NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: RX SUM_PHY_RATE");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_RX_SUCCESS_PPDU_COUNT,
				hw_link_rx->success_ppdu_count)) {
			ath12k_err(NULL, "nla put failure: RX SUCCESS_PPDU_COUNT");
			return -EINVAL;
		}
	}

	if (peer_type != ATH12K_LINK_PEER) {
		if (!mld_stats || !mld_stats->hw_stats) {
			ath12k_err(NULL,
				   "Invalid mld_stats: ptr:%p hw_stats:%p\n",
				   mld_stats,
				   mld_stats ? mld_stats->hw_stats : NULL);
			return -EINVAL;
		}

		hw_rx = &mld_stats->hw_stats->hw_rx;

		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_UCAST_BYTES,
				      hw_rx->drop_ucast_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: RX DROP_UCAST_BYTES");
			return -EINVAL;
		}
		if (nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_GCAST_BYTES,
				      hw_rx->drop_gcast_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL, "nla put failure: RX DROP_GCAST_BYTES");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP2_UCAST_PKTS,
				hw_rx->drop2_pkts)) {
			ath12k_err(NULL, "nla put failure: RX DROP2_UCAST_PKTS");
			return -EINVAL;
		}
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_PER_PKT_STATS_RX_DROP_GCAST_PKTS,
				hw_rx->drop_gcast_pkts)) {
			ath12k_err(NULL, "nla put failure: RX DROP_GCAST_PKTS");
			return -EINVAL;
		}
	}

	return 0;
}

static int ath12k_fill_mac80211_stats(struct sk_buff *vendor_event,
				      const struct ath12k_mac80211_flow_stats *flow)
{
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC80211_TX_NETIF_PKTS,
			flow->tx_netif_pkts) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC80211_TX_DRV_PKTS,
			flow->tx_drv_pkts) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC80211_RX_DRV_PKTS,
			flow->rx_drv_pkts) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC80211_RX_NETIF_PKTS,
			flow->rx_netif_pkts) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC80211_RX_FORWARDED_PKTS,
			flow->rx_forwarded_pkts)) {
		ath12k_err(NULL, "nla put failure: mac80211 flow stats");
		return -EINVAL;
	}
	return 0;
}

static int ath12k_fill_peer_rx_stats(struct ath12k *ar,
				     struct sk_buff *vendor_event,
				     struct ath12k_dp_peer_stats *peer_stats,
				     struct ath12k_dp_link_peer_stats *link_peer_stats,
				     struct ath12k_dp_mld_peer_stats *mld_stats,
				     struct ath12k_dp_rx_pkt_ppdu_stats *rx_pkt_stats,
				     bool is_extended,
				     int peer_type)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	struct ath12k_rx_ppdu_stats *rx_mon_stats;
	int ring_num;
	bool is_hw_stats = false;

	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp))
		is_hw_stats = true;

	/*Rx Per Pkt Stats*/
	attr = nla_nest_start(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: Peer per pkt stats Rx");
		return -EINVAL;
	}

	for (ring_num = 0;
	     ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX &&
	     ring_num < DP_REO_DST_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: Peer per pkt stats Rx ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (ath12k_fill_peer_rx_per_pkt_stats_attrs(vendor_event,
							    peer_stats,
							    ring_num)) {
			ath12k_err(NULL,
				   "nla put failure: Peer rx per_pkt stats for ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		/* HW offload RX stats — Ring 0 only */
		if (((ring_num + 1) == QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_0) &&
		    ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
			if (ath12k_fill_peer_hw_rx_stats(vendor_event,
							 link_peer_stats,
							 mld_stats,
							 peer_type)) {
				ath12k_err(NULL, "Error filling peer HW RX stats");
				nla_nest_cancel(vendor_event, attr1);
				nla_nest_cancel(vendor_event, attr);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	/* RX packets/bytes derived from PPDU, independent of extended RX
	 * stats knob (DP_ENABLE_EXT_RX_STATS)
	 */
	if (rx_pkt_stats) {
		if (nla_put_u32(vendor_event,
				QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MSDU_COUNT,
				rx_pkt_stats->rx_packets) ||
		    nla_put_u64_64bit(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MSDU_BYTES,
				      rx_pkt_stats->rx_bytes,
				      NL80211_ATTR_PAD)) {
			ath12k_err(NULL,
				   "nla put failure: RX packet/byte counters");
			return -EMSGSIZE;
		}
	}

	/* Rx Debug Stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_DBG_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: Peer rx dbg stats");
		return -EINVAL;
	}
	if (ath12k_fill_peer_rx_dbg_stats_attrs(vendor_event, peer_stats,
						is_extended, is_hw_stats)) {
		ath12k_err(NULL, "Error filling peer rx dbg stats");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	/*Rx WBM Err Stats*/
	if (ath12k_fill_peer_rx_wbm_err_attrs(vendor_event, peer_stats)) {
		ath12k_err(NULL, "Error filling peer rx wbm err stats");
		return -EINVAL;
	}

	if (link_peer_stats && ath12k_dp_rx_ppdu_stats_enabled(&ar->dp)) {
		rx_mon_stats = link_peer_stats->rx_ppdu_stats;

		if (!rx_mon_stats)
			return 0;

		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_MON_STATS);
		if (!attr) {
			ath12k_err(NULL, "nla nest failure: RX monitor stats");
			return -EINVAL;
		}

		if (ath12k_vendor_fill_rx_mon_stats(vendor_event, rx_mon_stats,
						    is_extended, peer_type, ar)) {
			ath12k_err(NULL, "nla put failure: RX monitor stats");
			nla_nest_cancel(vendor_event, attr);
			return -EMSGSIZE;
		}

		nla_nest_end(vendor_event, attr);
	}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	if (ath12k_fill_peer_mmesh_rx_stats_wrapper(vendor_event, peer_stats)) {
		ath12k_err(NULL, "Error filling  mmesh stats\n");
		return -EINVAL;
	}
#endif

	return 0;
}

static int
ath12k_tele_sdwftx_stats_update(struct sk_buff *skb, struct ath12k_dp_qos_tx_stats *tx,
				u8 tid, u8 q_id)
{
	struct nlattr *attr1 = NULL, *attr2 = NULL;
	int ret = -EINVAL, pkt_type, mcs;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_DEPTH, tx->queue_depth) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_THROUGHPUT, tx->throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS_RATE, tx->ingress_rate) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MIN_THROUGHPUT, tx->min_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX_THROUGHPUT, tx->max_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AVG_THROUGHPUT, tx->avg_throughput) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_ERROR_RATE, tx->per) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PERCENTAGE, tx->retries_pct) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PKTS_CNT, tx->retry_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TOTAL_RETRIES_CNT, tx->total_retries_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MULTIPLE_RETRIES_CNT, tx->multiple_retry_count) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED_RETRIES_CNT, tx->failed_retry_count) ||
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_REINJECT_PKTS, tx->reinject_pkt) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TID, tid) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_ID, q_id)) {
		ath12k_err(NULL, "nla_put_failure: SDWF TX attributes \n");
		goto end;
	}

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SUCCESS);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_SUCCESS \n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			tx->tx_success.packets) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      tx->tx_success.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_SUCCESS attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_FAILED \n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			tx->tx_failed.packets) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      tx->tx_failed.bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_FAILED attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_INGRESS \n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			tx->tx_ingress.packets) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      tx->tx_ingress.bytes, NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_INGRESS attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_DROP \n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_TX,
			tx->dropped.fw_rem_tx) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_NOTX,
			tx->dropped.fw_rem_notx) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_AGED_FRAMES,
			tx->dropped.age_out) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON1,
			tx->dropped.fw_reason1) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON2,
			tx->dropped.fw_reason2) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON3,
			tx->dropped.fw_reason3) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_DISABLE_QUEUE,
			tx->dropped.fw_rem_queue_disable) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_TILL_NONMATCHING,
			tx->dropped.fw_rem_no_match) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_THRESHOLD_DROP,
			tx->dropped.drop_threshold) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_LINK_DESC_UNAVAIL_DROP,
			tx->dropped.drop_link_desc_na) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_MSDU_OR_DROP,
			tx->dropped.invalid_drop) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MULTICAST_DROP,
			tx->dropped.mcast_vdev_drop) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_RR,
			tx->dropped.invalid_rr)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_DROP attributes\n");
		goto end;
	}

	attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_MPDU);
	if (!attr2) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX_DROP REMOVE MPDU\n");
		goto end;
	}

	if (nla_put_u32(skb,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			tx->dropped.fw_rem.packets) ||
	    nla_put_u64_64bit(skb,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      tx->dropped.fw_rem.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX_DROP REMOVE MPDU attributes\n");
		goto end;
	}

	nla_nest_end(skb, attr2);
	nla_nest_end(skb, attr1);

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE\n");
		goto end;
	}

	for (pkt_type = 0;
	     pkt_type < QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MAX &&
	     pkt_type < DOT11_MAX; pkt_type++) {
		attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A + pkt_type);
		if (!attr2) {
			ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE_80211\n");
			goto end;
		}
		for (mcs = 0; mcs < MAX_MCS &&
		     mcs < QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_MAX; mcs++) {
			if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 + mcs,
				tx->pkt_type[pkt_type].mcs_count[mcs])) {
				ath12k_err(NULL, "nla_nest_failure: SDWFTX PKT_TYPE_80211 %d mcs attribute %d\n",
					   QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A + pkt_type,
					   QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 + mcs);
				goto end;
			}
		}
		nla_nest_end(skb, attr2);
	}
	nla_nest_end(skb, attr1);

	attr1 =  nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SERVICE_INTERVAL);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX SERVICE_INTERVAL\n");
		goto end;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT,
			tx->svc_intval_stats.success_cnt) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,
			tx->svc_intval_stats.failure_cnt)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX SERVICE_INTERVAL attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);

	attr1 =  nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_BURST_SIZE);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWFTX BURST SIZE\n");
		goto end;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT,
			tx->burst_size_stats.success_cnt) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,
			tx->burst_size_stats.failure_cnt)) {
		ath12k_err(NULL, "nla_put_failure: SDWFTX BURST SIZE attributes\n");
		goto end;
	}
	nla_nest_end(skb, attr1);
	ret = 0;
end:
	return ret;
}

static int ath12k_fill_sdwftx_stats(struct sk_buff *skb,
			     struct ath12k_dp_link_peer_qos_stats *link_qos_stats,
			     u8 svc_id)
{
	struct nlattr *attr = NULL;
	struct ath12k_dp_qos_tx_stats *tx = NULL;
	int ret = -EINVAL;
	u8 msduq = 0, tid, q_idx;

	if (svc_id == 0) {
		for (tid = 0; tid < QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX &&
		     tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0;
			     q_idx < QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX &&
			     q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				tx = &link_qos_stats->tx[tid][q_idx];
				attr = nla_nest_start(skb, msduq);
				if (!attr) {
					ath12k_err(NULL, "nla_nest_failure: SDWF TX for msduq %u\n", msduq);
					goto end;
				}
				ret = ath12k_tele_sdwftx_stats_update(skb, tx,
							  tid, q_idx);
				if (ret) {
					ath12k_err(NULL, "sdwf tx stats update failure for msduq %u\n", msduq);
					goto end;
				}
				nla_nest_end(skb, attr);
				msduq++;
			}
		}
	} else {
		tx = &link_qos_stats->tx[0][0];
		if (!tx) {
			ath12k_err(NULL, "nla_nest_failure: SDWF TX stats NA \n");
			goto end;
		}
		attr = nla_nest_start(skb, msduq);
		if (!attr) {
			ath12k_err(NULL, "nla_nest_failure: SDWF TX for msduq %u\n", msduq);
			goto end;
		}

		ret = ath12k_tele_sdwftx_stats_update(skb, tx, link_qos_stats->tid,
						      link_qos_stats->msduq);
		if (ret) {
			ath12k_err(NULL, "sdwf tx stats update failure for msduq : 0\n");
			goto end;
		}
		nla_nest_end(skb, attr);
	}
end:
	return ret;
}

static int ath12k_tele_sdwfdelay_stats_update(struct sk_buff *skb,
					      struct ath12k_dp_qos_delay_stats *delay,
					      u8 tid, u8 q_id)
{
	struct nlattr *attr1 = NULL, *attr2 = NULL, *attr3 = NULL;
	size_t size;
	int ret = -EINVAL, buc_id;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_SOFTWARE_DELAY_AVG, delay->swdelay_avg) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_NETWORK_DELAY_AVG, delay->nwdelay_avg) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HARDWARE_DELAY_AVG, delay->hwdelay_avg) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_TID, tid) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_QUEUE_ID, q_id)) {
		ath12k_err(NULL, "nla_put_failure: SDWF DELAY stats attributes \n");
		goto end;
	}

	attr1 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY);
	if (!attr1) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HWDELAY\n");
		goto end;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_INVALID_PKTS,
			delay->invalid_delay_pkts) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_SUCCESS,
		    delay->delay_success) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_FAILURE,
		    delay->delay_failure) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAXIMUM,
		    sizeof(int), &delay->delay_hist.max) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MINIMUM,
		    sizeof(int), &delay->delay_hist.min) ||
	    nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AVERAGE,
		    sizeof(int), &delay->delay_hist.avg)) {
		ath12k_err(NULL, "nla_put_failure: SDWF DELAY HWDELAY attributes\n");
		goto end;
	}

	attr2 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_HISTOGRAM);
	if (!attr2) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HWDELAY HISTOGRAM\n");
		goto end;
	}

	attr3 = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_TX_COMP_DELAY);
	if (!attr3) {
		ath12k_err(NULL, "nla_nest_failure: SDWF DELAY HW_TX_COMP_DELAY TYPE\n");
		goto end;
	}

	size = ARRAY_SIZE(delay->delay_hist.hist.freq);
	for (buc_id = 0; buc_id < HIST_BUCKET_MAX && buc_id < size &&
	     buc_id < QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_MAX; buc_id++) {
		if (nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_TELE_DELAY_HIST_BUCKET_ID_0 + buc_id,
				delay->delay_hist.hist.freq[buc_id])) {
			ath12k_err(NULL, "nla_put_failure: SDWF DELAY HW_TX_COMP_DELAY TYPE attributes\n");
			goto end;
		}
	}
	nla_nest_end(skb, attr3);

	nla_nest_end(skb, attr2);

	nla_nest_end(skb, attr1);
	ret = 0;
end:
	return ret;
}

static int
ath12k_fill_sdwfdelay_stats(struct sk_buff *skb,
			    struct ath12k_dp_link_peer_qos_stats *link_qos_stats,
			    u8 svc_id)
{
	struct nlattr *attr = NULL;
	struct ath12k_dp_qos_delay_stats *delay = NULL;
	int ret = -EINVAL;
	u8 msduq = 0, tid, q_idx;

	if (svc_id == 0) {
		for (tid = 0; tid < QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX &&
		     tid < QOS_TID_MAX; tid++) {
			for (q_idx = 0;
			     q_idx < QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX &&
			     q_idx < QOS_TID_MDSUQ_MAX; q_idx++) {
				delay = &link_qos_stats->delay[tid][q_idx];
				attr = nla_nest_start(skb, msduq);
				if (!attr) {
					ath12k_err(NULL, "nla_nest_failure: SDWF DELAY for msduq %u\n", msduq);
					goto end;
				}
				ret = ath12k_tele_sdwfdelay_stats_update(skb, delay, tid, q_idx);
				if (ret) {
					ath12k_err(NULL, "sdwf delay stats update failure for msduq %u\n", msduq);
					goto end;
				}
				nla_nest_end(skb, attr);
				msduq++;
			}
		}
	} else {
		delay = &link_qos_stats->delay[0][0];
		if (!delay) {
			ath12k_err(NULL, "nla_nest_failure: SDWF DELAY stats NA \n");
			goto end;
		}
		attr = nla_nest_start(skb, msduq);
		if (!attr) {
			ath12k_err(NULL, "nla_nest_failure: SDWF DELAY for msduq %u\n", msduq);
			goto end;
		}

		ret = ath12k_tele_sdwfdelay_stats_update(skb, delay,
							 link_qos_stats->tid,
							 link_qos_stats->msduq);
		if (ret) {
			ath12k_err(NULL, "sdwf stats update failure for msduq : 0\n");
			goto end;
		}
		nla_nest_end(skb, attr);
	}
end:
	return ret;
}

static int ath12k_prepare_peer_vendor_event(struct sk_buff *vendor_event,
					    struct ath12k_vif *ahvif,
					    struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_peer *telemetry_peer;
	struct ath12k_dp_link_peer_hw_stats *hw_link_stats = NULL;
	struct ath12k_dp_peer_hw_stats *hw_stats = NULL;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats = NULL;
	struct ath12k *ar = &ahvif->ah->radio[0];
	struct ath12k_dp_proto_stats_peer *proto = NULL;
	struct ath12k_dp_mld_agg_latency_peer_stats *agg_mld_stats = NULL;
	struct ath12k_dp_link_peer_qos_stats *qos_stats = NULL;
	struct ath12k_rx_ppdu_stats *rx_mon_stats;
	struct nlattr *attr;
	int ret = -EINVAL;

	telemetry_peer = vmalloc(sizeof(*telemetry_peer));
	if (!telemetry_peer) {
		ath12k_err(NULL, "Allocation failure for peer_stats");
		return -ENOMEM;
	}

	memset(telemetry_peer, 0, sizeof(*telemetry_peer));

	/* Allocate RX stats if requested */
	rx_mon_stats = vmalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats)
		goto out;

	memset(rx_mon_stats, 0,
	       sizeof(struct ath12k_rx_ppdu_stats));
	telemetry_peer->link_peer_stats.rx_ppdu_stats = rx_mon_stats;
	telemetry_peer->peer_type = ATH12K_PEER_INVAL;

	tx_ppdu_stats = vzalloc(sizeof(*tx_ppdu_stats));

	if (!tx_ppdu_stats) {
		vfree(rx_mon_stats);
		vfree(telemetry_peer);
		return -ENOMEM;
	}
	telemetry_peer->link_peer_stats.tx_ppdu_stats = tx_ppdu_stats;

	if (ath12k_proto_stats_enabled(&ar->dp)) {
		proto = vzalloc(sizeof(*proto));
		if (!proto)
			goto out;

		telemetry_peer->peer_stats.proto = proto;
	}

	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
		hw_link_stats = vzalloc(sizeof(*hw_link_stats));
		if (!hw_link_stats) {
			goto out;
		}
		telemetry_peer->link_peer_stats.hw_link_stats = hw_link_stats;

		hw_stats = vzalloc(sizeof(*hw_stats));
		if (!hw_stats)
			goto out;

		telemetry_peer->mld_stats.hw_stats = hw_stats;
	}

	if (ath12k_dp_latency_stats_enabled(&ar->dp)) {
		agg_mld_stats = vzalloc(sizeof(*agg_mld_stats));

		if (!agg_mld_stats)
			goto out;

		telemetry_peer->mld_stats.agg_mld_stats = agg_mld_stats;
	}

	if (ath12k_debugfs_is_qos_stats_enabled(ar)) {
		qos_stats = vzalloc(sizeof(*qos_stats));
		if (!qos_stats)
			goto out;

		telemetry_peer->link_peer_stats.link_qos_stats = qos_stats;
	}

	if (ath12k_get_peer_telemetry_stats(ahvif, telemetry_peer, cmd->mac,
					    cmd->link_id)) {
		ath12k_err(NULL, "Error getting peer stats from dp");
		goto out;
	}

	/* mac80211 flow stats: MLD-level only, gated on feat_mac80211 like TX/RX */
	if (cmd->feat.feat_mac80211 &&
	    telemetry_peer->peer_type == ATH12K_MLD_PEER) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAC_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_mac80211_stats(vendor_event,
						       &telemetry_peer->mac80211_flow)) {
				ath12k_err(NULL, "nla put failure: mac80211 flow stats");
				nla_nest_cancel(vendor_event, attr);
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: mac80211 flow stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_tx_stats(ar,
						      vendor_event,
						      &telemetry_peer->peer_stats,
						      telemetry_peer->is_extended,
						      &telemetry_peer->link_peer_stats,
						      &telemetry_peer->mld_stats,
						      telemetry_peer->peer_type)) {
				ath12k_err(NULL, "nla put failure: Sta tx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta tx feat stats");
			goto out;
		}
	}
	if (cmd->feat.feat_proto) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_EVENT);
		if (attr) {
			if (ath12k_fill_peer_proto_stats(ar,
							 vendor_event,
							 &telemetry_peer->peer_stats)) {
				ath12k_err(NULL, "nla put failure: Proto stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta Proto feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_peer_rx_stats(ar,
						      vendor_event,
						      &telemetry_peer->peer_stats,
						      &telemetry_peer->link_peer_stats,
						      &telemetry_peer->mld_stats,
						      &telemetry_peer->rx_pkt_ppdu_stats,
						      telemetry_peer->is_extended,
						      telemetry_peer->peer_type)) {
				ath12k_err(NULL, "nla put failure: Sta rx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta rx feat stats");
			goto out;
		}
	}

	if (cmd->svc_id != INVALID_SVC_ID &&
	    (cmd->feat.feat_sdwftx || cmd->feat.feat_sdwfdelay)) {
		ret = ath12k_telemetry_get_qos_stats(ahvif,
						     telemetry_peer, cmd);
		if (ret) {
			ath12k_err(NULL, "SDWF stats get failure\n");
			goto out;
		}

		if (nla_put_u8(vendor_event,
			       QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID_EVENT,
			       cmd->svc_id)) {
			ath12k_err(NULL, "nla put failure: SDWF svc id event");
			ret = -EINVAL;
			goto out;
		}

		qos_stats = telemetry_peer->link_peer_stats.link_qos_stats;
		if (cmd->feat.feat_sdwftx) {
			attr = nla_nest_start(vendor_event,
					      QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFTX_STATS_EVENT);
			if (!attr) {
				ath12k_err(NULL, "nla nest failure: SDWF tx feat event");
				ret = -EINVAL;
				goto out;
			}

			if (ath12k_fill_sdwftx_stats(vendor_event,
						     qos_stats,
						     cmd->svc_id)) {
				ath12k_err(NULL, "nla put failure: SDWF tx stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		}

		if (cmd->feat.feat_sdwfdelay) {
			attr = nla_nest_start(vendor_event,
					      QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFDELAY_STATS_EVENT);
			if (!attr) {
				ath12k_err(NULL, "nla nest failure: SDWF delay feat event");
				ret = -EINVAL;
				goto out;
			}

			if (ath12k_fill_sdwfdelay_stats(vendor_event,
							qos_stats,
							cmd->svc_id)) {
				ath12k_err(NULL, "nla put failure: SDWF delay stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		}
	}

	if (cmd->feat.feat_delay && ath12k_dp_latency_stats_enabled(&ar->dp)) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_DELAY_EVENT);
		if (attr) {
			if (ath12k_fill_peer_delay_stats(ar,
							 vendor_event,
							 &telemetry_peer->mld_stats)) {
				ath12k_err(NULL, "nla put failure: Delay stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta Delay feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_jitter && ath12k_dp_latency_stats_enabled(&ar->dp)) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_JITTER_EVENT);
		if (attr) {
			if (ath12k_fill_peer_jitter_stats(ar,
							  vendor_event,
							  &telemetry_peer->mld_stats)) {
				ath12k_err(NULL, "nla put failure: Jitter stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta Jitter feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_sojourn && ath12k_dp_latency_stats_enabled(&ar->dp)) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_SOJOURN_EVENT);
		if (attr) {
			if (ath12k_fill_peer_sojourn_stats(ar,
							   vendor_event,
							   &telemetry_peer->mld_stats)) {
				ath12k_err(NULL, "nla put failure: Sojourn stats");
				ret = -EINVAL;
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Sta Sojourn feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	vfree(qos_stats);
	vfree(agg_mld_stats);
	vfree(hw_stats);
	vfree(hw_link_stats);
	vfree(proto);
	vfree(tx_ppdu_stats);
	vfree(rx_mon_stats);
	vfree(telemetry_peer);
	return ret;
}

static int ath12k_send_cp_event(struct ath12k_telemetry_command *cmd,
				struct sk_buff *vendor_event,
				struct ath12k_vif *ahvif)
{
	struct ath12k_mgmt_frame_stats *mgmt_stats = &ahvif->mgmt_stats;
	u64 tx_success_count = 0, tx_failure_count = 0;
	struct ath12k_fw_stats_req_params param = {0};
	struct ath12k_fw_stats_bcn *bcn_stats;
	u64 total_tx_mgmt_succ_count;
	u64 total_tx_mgmt_fail_count;
	struct ath12k_link_vif *arvif;
	int ret = 0, vdev_id = 0;
	u64 total_tx_mgmt_count;
	struct nlattr *attr;
	bool found = false;
	struct ath12k *ar;

	/* No specific link requested; fall back to the default link */
	arvif = &ahvif->deflink;
	if (cmd->link_id != INVALID_LINK_ID) {
		/* Validate that the requested link_id is within range and
		 * actually exists in the driver's active links bitmap.
		 */
		if (cmd->link_id >= ATH12K_NUM_MAX_LINKS ||
		    !(ahvif->links_map & BIT(cmd->link_id))) {
			ath12k_err(NULL, "Invalid link_id %d for control path stats\n",
				   cmd->link_id);
			return -EINVAL;
		}
		arvif = ath12k_get_arvif_from_link_id(ahvif, cmd->link_id);
	}

	if (!arvif || !arvif->ar) {
		ath12k_err(NULL, "Failed to get arvif for link_id %d\n", cmd->link_id);
		return -EINVAL;
	}

	ar = arvif->ar;

	/* Get beacon stats from FW */
	ath12k_fw_stats_reset(ar);
	param.pdev_id = ath12k_mac_get_target_pdev_id(ar);
	param.vdev_id = arvif->vdev_id;
	param.stats_id = WMI_REQUEST_BCN_STAT;

	ret = ath12k_mac_get_fw_stats_per_vif(ar, &param);
	if (ret) {
		ath12k_err(ar->ab, "failed to request fw stats: %d\n", ret);
		return ret;
	}

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(bcn_stats, &ar->fw_stats.bcn, list) {
		if (bcn_stats->vdev_id == arvif->vdev_id) {

			vdev_id = bcn_stats->vdev_id;
			tx_success_count = bcn_stats->tx_bcn_succ_cnt;
			tx_failure_count = bcn_stats->tx_bcn_outage_cnt;

			total_tx_mgmt_succ_count = mgmt_stats->aggr_tx_mgmt_success_cnt +
				tx_success_count;
			total_tx_mgmt_fail_count = mgmt_stats->aggr_tx_mgmt_fail_cnt +
				tx_failure_count;
			total_tx_mgmt_count = total_tx_mgmt_succ_count +
				total_tx_mgmt_fail_count;

			found = true;
			break;
		}
	}
	ath12k_fw_stats_bcn_free(&ar->fw_stats.bcn);
	spin_unlock_bh(&ar->data_lock);
	if (!found) {
		ath12k_err(ar->ab, "beacon stats not found for vdev_id %d\n",
			   arvif->vdev_id);
		return -ENOENT;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_VAP_CP_STATS_EVENT);
	if (!attr) {
		ath12k_err(ar->ab, "nla nest failure: CP stats\n");
		return -EINVAL;
	}

	if (ath12k_scan_radio_supported(ar->pdev)) {
#ifdef CPTCFG_QCN_EXTN
		ret = ath12k_fill_cp_tx_scan_radio_stats(vendor_event, mgmt_stats);
		if (ret) {
			nla_nest_cancel(vendor_event, attr);
			return ret;
		}
#endif /* CPTCFG_QCN_EXTN */
		nla_nest_end(vendor_event, attr);
		return 0;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TELEMETRY_CP_VDEV_ID, vdev_id) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_BEACON_COUNT,
		tx_success_count,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_BEACON_OUTAGE_COUNT,
		tx_failure_count,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_PROBE_REQUEST,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_PROBE_REQ],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_PROBE_RESPONSE,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_PROBE_RESP],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_REQUEST_SUCCESS,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_ASSOC_REQ],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_REQUEST_FAILURE,
		mgmt_stats->tx_fail_cnt[QCA_VENDOR_MGMT_STATS_ASSOC_REQ],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_RESPONSE_SUCCESS,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_ASSOC_RESP],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ASSOC_RESPONSE_FAILURE,
		mgmt_stats->tx_fail_cnt[QCA_VENDOR_MGMT_STATS_ASSOC_RESP],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_SENT_SUCCESS,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_ACTION],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_SENT_SUCCESS,
		mgmt_stats->tx_succ_cnt[QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_SENT_FAIL,
		mgmt_stats->tx_fail_cnt[QCA_VENDOR_MGMT_STATS_ACTION],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_SENT_FAIL,
		mgmt_stats->tx_fail_cnt[QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_COMPLETION_SUCCESS,
		mgmt_stats->tx_compl_succ[QCA_VENDOR_MGMT_STATS_ACTION],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_COMPLETION_SUCCESS,
		mgmt_stats->tx_compl_succ[QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_COMPLETION_FAIL,
		mgmt_stats->tx_compl_fail[QCA_VENDOR_MGMT_STATS_ACTION],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_ACTION_NO_ACK_COMPLETION_FAIL,
		mgmt_stats->tx_compl_fail[QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_FRAMES,
		total_tx_mgmt_count,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_SUCCESS_COUNT,
		total_tx_mgmt_succ_count,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_TX_MGMT_FAILURE_COUNT,
		total_tx_mgmt_fail_count,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||

	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_BEACON_COUNT,
		mgmt_stats->rx_cnt[QCA_VENDOR_MGMT_STATS_BEACON],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_PROBE_REQUEST_UCAST,
		mgmt_stats->rx_cnt[QCA_VENDOR_MGMT_STATS_PROBE_REQ],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_PROBE_REQUEST_BCAST,
		ar->dp.stats.telemetry_stats.rx_probe_req_bc,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_NO_RATE_MATCH,
		mgmt_stats->rx_assoc_no_rate_match,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_BAD_WPAIE,
		mgmt_stats->rx_assoc_bad_wpaie,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ASSOC_WITH_CAP_MISMATCH,
		mgmt_stats->rx_assoc_cap_mismatch,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ACTION,
		mgmt_stats->rx_cnt[QCA_VENDOR_MGMT_STATS_ACTION],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_ACTION_NO_ACK,
		mgmt_stats->rx_cnt[QCA_VENDOR_MGMT_STATS_ACTION_NO_ACK],
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID) ||
	    nla_put_u64_64bit
		(vendor_event,
		QCA_VENDOR_ATTR_TELEMETRY_CP_RX_MGMT_FRAMES,
		mgmt_stats->aggr_rx_mgmt,
		QCA_VENDOR_ATTR_TELEMETRY_CP_INVALID)) {
		ath12k_err(ar->ab, "nla put failure: cp stats\n");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_stats_device_setup(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct sk_buff *vendor_event;
	struct ath12k *ar;
	struct ath12k_dp *dp;
	int len, ret;

	if (cmd->link_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid HW Link ID %d", cmd->link_id);
		return -EINVAL;
	}

	ar = &ah->radio[cmd->link_id];
	if (!ar->ab) {
		ath12k_err(NULL, "ab not present\n");
		return -EINVAL;
	}

	dp = ar->ab->dp;

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ath12k_err(ar->ab, "Device stats return. Recovery in progress\n");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd,
					     ath12k_dp_hw_peer_stats_enabled(&ar->dp));
	ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d\n",
		   len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT,
						   GFP_KERNEL);
	if (!vendor_event) {
		ath12k_err(ar->ab, "Error allocating vendor event\n");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_device_vendor_event(vendor_event, dp, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(ar->ab, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

/**
 * ath12k_dp_get_radio_cp_stats() - Collect all Radio Control Path stats
 * @ar: pointer to ath12k radio instance
 * @telemetry_radio: pre-populated DP radio stats (RTS counters sourced here)
 * @dp_tx_failed: aggregated TX failure count from DP layer
 * @cp_stats: caller-allocated struct to be filled with all CP stat values
 *
 * Gathers every field needed by the Radio CP NL event into @cp_stats.
 *
 * Return: 0 on success; negative error code if FW stats request fails
 *         (partial stats are still populated in that case).
 */
int ath12k_dp_get_radio_cp_stats(struct ath12k_telemetry_dp_radio *telemetry_radio,
				 struct ath12k_radio_cp_stats *cp_stats,
				 struct ath12k *ar,
				 u32 dp_tx_failed)
{
	struct ath12k_dp_link_peer_stats *link_peer_stats;
	struct ath12k_link_vif *arvif;

	cp_stats->tx_failed = dp_tx_failed;

	/* RTS counters: use the same aggregated pdev stats source as RADIO DP
	 * so RADIO CP RTS matches RADIO DP RTS exactly.
	 */
	if (telemetry_radio &&
	    telemetry_radio->aggr_pdev_stats.link_peer_stats.tx_ppdu_stats) {
		link_peer_stats = &telemetry_radio->aggr_pdev_stats.link_peer_stats;
		cp_stats->tx_rts_success = link_peer_stats->tx_ppdu_stats->rts_success;
		cp_stats->tx_rts_fail    = link_peer_stats->tx_ppdu_stats->rts_failure;
	}

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif, &ar->arvifs, list)
		cp_stats->rx_mgmt += arvif->ahvif->mgmt_stats.aggr_rx_mgmt;
	cp_stats->rx_mgmt += ar->dp.stats.telemetry_stats.rx_probe_req_bc;

	/* Read all telemetry stats under the same lock so they are
	 * consistent with each other and with rx_mgmt above.
	 */
	cp_stats->rx_crc_err     = ar->dp.stats.telemetry_stats.rx_crc_err;
	cp_stats->rx_decrypt_err = ar->dp.stats.telemetry_stats.rx_decrypt_err;
	cp_stats->rx_mic_err     = ar->dp.stats.telemetry_stats.rx_mic_err;
	cp_stats->rx_over_run    = ar->dp.stats.telemetry_stats.rx_over_run;
	cp_stats->rx_ctrl        = ar->dp.stats.telemetry_stats.rx_bar_cnt;
	spin_unlock_bh(&ar->data_lock);

	return 0;
}

/**
 * ath12k_mac_get_chan_survey_noise() - Read the cached noise floor for a channel.
 * @ar:      radio instance whose survey cache is queried
 * @channel: channel whose noise floor is requested
 *
 * Uses the same freq-to-survey-index mapping as the WMI survey event handler
 * (freq_to_idx() in wmi.c) so the correct ar->survey[] slot is always
 * addressed.  Only bands that this radio actually supports are counted,
 * matching the indexing used when ar->survey[] is populated.
 *
 * Caller must hold the wiphy lock (lockdep_assert_wiphy).
 *
 * Return: noise floor in dBm, or ATH12K_DEFAULT_NOISE_FLOOR if the survey
 *         slot has not been populated yet or the channel is not found.
 */
static s8 ath12k_mac_get_chan_survey_noise(struct ath12k *ar,
					   struct ieee80211_channel *channel)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	int band, ch, idx = 0;
	s8 noise;

	lockdep_assert_wiphy(hw->wiphy);

	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
		if (!ar->mac.sbands[band].channels)
			continue;

		sband = hw->wiphy->bands[band];
		if (!sband)
			continue;

		for (ch = 0; ch < sband->n_channels; ch++, idx++) {
			if (sband->channels[ch].center_freq == channel->center_freq)
				goto found;
		}
	}

	/* Channel not found in any band supported by this radio */
	return ATH12K_DEFAULT_NOISE_FLOOR;

found:
	if (idx >= ATH12K_NUM_CHANS)
		return ATH12K_DEFAULT_NOISE_FLOOR;

	spin_lock_bh(&ar->data_lock);
	if (ar->survey[idx].filled & SURVEY_INFO_NOISE_DBM)
		noise = ar->survey[idx].noise;
	else
		noise = ATH12K_DEFAULT_NOISE_FLOOR;
	spin_unlock_bh(&ar->data_lock);

	return noise;
}

static int ath12k_fill_radio_cp_stats(struct ath12k_telemetry_dp_radio *telemetry_radio,
				      struct ath12k_telemetry_command *cmd,
				      struct sk_buff *vendor_event,
				      struct ath12k *ar,
				      u32 dp_tx_failed)
{
	struct ath12k_radio_cp_stats cp_stats = {};
#ifdef CPTCFG_ATH12K_DEBUGFS
	struct nlattr *nf_nest;
#endif
	struct nlattr *attr;
	s8 dynamic_nf = 0;
	int ret;
	int i;

	ret = ath12k_dp_get_radio_cp_stats(telemetry_radio, &cp_stats, ar,
					   dp_tx_failed);
	if (ret)
		return ret;

	/* Obtain dynamic (runtime) NF via the WMI BSS survey path - the same
	 * mechanism used by "iw dev survey dump".  After triggering the survey
	 * request, find the survey index for rx_channel and read its noise
	 * field under data_lock.
	 *
	 * Static NF (bdf_nf_chains[]) is fetched separately via HTT PHY stats.
	 */
	if (ar->rx_channel) {
		ath12k_mac_update_bss_chan_survey(ar, ar->rx_channel);
		dynamic_nf = ath12k_mac_get_chan_survey_noise(ar, ar->rx_channel);
	}

#ifdef CPTCFG_ATH12K_DEBUGFS
	/* Fetch static NF chains via HTT PHY stats (type 37) */
	ret = ath12k_telemetry_get_phy_nf(ar);
	if (ret)
		ath12k_warn(ar->ab,
			    "failed to fetch static NF via HTT PHY stats: %d\n", ret);
#endif /* CPTCFG_ATH12K_DEBUGFS */

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_EVENT);
	if (!attr) {
		ath12k_err(ar->ab, "nla nest failure: radio CP stats\n");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_FAILED,
			cp_stats.tx_failed) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_RTS_SUCCESS,
			cp_stats.tx_rts_success) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_TX_RTS_FAIL,
			cp_stats.tx_rts_fail) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_MGMT,
			cp_stats.rx_mgmt) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_CTRL,
			cp_stats.rx_ctrl) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_DECRYPT_ERR,
			cp_stats.rx_decrypt_err) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_MIC_ERR,
			cp_stats.rx_mic_err) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_OVER_RUN,
			cp_stats.rx_over_run) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_RX_CRC_ERR,
			cp_stats.rx_crc_err) ||
	    nla_put_s32(vendor_event,
			QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_CHAN_NF_DYNAMIC,
			dynamic_nf)) {
		ath12k_err(ar->ab, "nla put failure: radio CP stats\n");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}

#ifdef CPTCFG_ATH12K_DEBUGFS
	/* Send all valid static NF chains as a nested attribute.
	 * Each entry is an NLA_S32 indexed by chain number (1-based).
	 * Chains with value == 1 are invalid/unused and are skipped.
	 * bdf_nf_chains[] is only populated when CPTCFG_ATH12K_DEBUGFS is
	 * enabled (via ath12k_telemetry_get_phy_nf()); omit this attribute
	 * entirely when built without debugfs to avoid sending stale/zero
	 * values to userspace.
	 */
	nf_nest = nla_nest_start
		(vendor_event,
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RADIO_CP_STATS_CHAN_NF_STATIC);
	if (!nf_nest) {
		ath12k_err(ar->ab, "nla nest start failure: static NF\n");
		nla_nest_cancel(vendor_event, attr);
		return -EINVAL;
	}
	for (i = 0; i < ATH12K_HTT_STATS_MAX_CHAINS &&
	     i < QCA_VENDOR_ATTR_WLAN_TELEMETRY_NSS_MAX; i++) {
		/* Skip chains where the firmware reported the sentinel value 1.
		 * The firmware uses 1 (positive 1 dBm) to indicate an unused or
		 * inactive chain. This value is unambiguous: valid noise floor
		 * readings are always negative in dBm (e.g. ATH12K_DEFAULT_NOISE_FLOOR
		 * = -95), so +1 dBm can never occur as a real NF measurement.
		 */
		if (ar->bdf_nf_chains[i] != 1 &&
		    nla_put_s32(vendor_event, i + 1,
				ar->bdf_nf_chains[i])) {
			ath12k_err(ar->ab,
				   "nla put failure: static NF chain %d\n", i + 1);
			nla_nest_cancel(vendor_event, nf_nest);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, nf_nest);
#endif /* CPTCFG_ATH12K_DEBUGFS */
	nla_nest_end(vendor_event, attr);

	return 0;
}

static struct ath12k_vif *ath12k_get_ahvif_from_wdev(struct wireless_dev *wdev)
{
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	return ahvif;
}

static int ath12k_stats_peer_setup(struct ath12k_telemetry_command *cmd)
{
	struct ath12k_vif *ahvif = NULL;
	struct ath12k *ar;
	bool is_hw_stats;
	struct sk_buff *vendor_event;
	int len, ret;

	if (!cmd->wdev) {
		ath12k_err(NULL, "wdev not present\n");
		return -EINVAL;
	}

	ahvif = ath12k_get_ahvif_from_wdev(cmd->wdev);

	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present\n");
		return -EINVAL;
	}

	ar = ahvif->deflink.ar;

	if (ahvif->deflink.ar && ahvif->deflink.ar->ab &&
	    test_bit(ATH12K_FLAG_CRASH_FLUSH, &ahvif->deflink.ar->ab->dev_flags)) {
		ath12k_err(NULL, "Peer stats return. Recovery in progress\n");
		return -EINVAL;
	}

	if (cmd->link_id != INVALID_LINK_ID &&
	    (cmd->link_id >= ATH12K_NUM_MAX_LINKS ||
	     !(ahvif->links_map & BIT(cmd->link_id)))) {
		ath12k_err(NULL, "Invalid link_id %d in peer stats setup",
			   cmd->link_id);
		return -EINVAL;
	}

	is_hw_stats = ath12k_dp_hw_peer_stats_enabled(ar ? &ar->dp : NULL);
	len = ath12k_get_dp_vendor_event_len(cmd, is_hw_stats);
	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d", len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT,
						   GFP_KERNEL);

	if (!vendor_event) {
		ath12k_err(NULL, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_peer_vendor_event(vendor_event, ahvif, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(NULL, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

static int ath12k_fill_vap_rx_stats(struct ath12k *ar,
				    struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/*Rx scan stats */
#ifdef CPTCFG_QCN_EXTN
	if (ath12k_scan_radio_supported(ar->pdev)) {
		ret = ath12k_fill_rx_scan_radio_stats(
				vendor_event, &telemetry_vif->rx_scan_radio_stats);
		return ret;
	}
#endif /* CPTCFG_QCN_EXTN */

	/* Aggregated Peer Rx Stats */
	ret = ath12k_fill_peer_rx_stats(ar,
					vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					&telemetry_vif->aggr_vif_stats.link_peer_stats,
					&telemetry_vif->aggr_vif_stats.mld_stats,
					&telemetry_vif->aggr_vif_stats.rx_pkt_ppdu_stats,
					telemetry_vif->is_extended,
					ATH12K_PEER_INVAL);

	return ret;
}

static int ath12k_fill_tx_ingress_stats_attrs(struct sk_buff *vendor_event,
					      struct ath12k_dp_tx_ingress_stats *ingress_tx_stats,
					      bool is_extended)
{
	struct nlattr *attr;
	int attr_index;

	/* Basic stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats from stack");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->recv_from_stack.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->recv_from_stack.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->enque_to_hw.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats enq to hw fast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->enque_to_hw_fast.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->enque_to_hw_fast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_SG_PKT);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats sg pkt");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->sg_pkt.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_SG_PKT);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->sg_pkt.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_SG_PKT);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_INGRESS_STATS_SG_DMA_MAP_ERR,
			ingress_tx_stats->sg_dma_map_err)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_SG_DMA_MAP_ERR);
		return -EINVAL;
	}

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats drop type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < QCA_VENDOR_ATTR_TX_INGRESS_ENQ_ERR_MAX &&
	     attr_index < DP_TX_ENQ_ERR_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->drop[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
				   attr_index + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	if (!is_extended)
		return 0;

	/* Extended stats */
	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encap type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_MAX &&
	     attr_index < HAL_TCL_ENCAP_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->encap_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
				   attr_index + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats encrypt type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_MAX &&
	     attr_index < HAL_ENCRYPT_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->encrypt_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
				   attr_index + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats desc type");
		return -EINVAL;
	}

	for (attr_index = 0; attr_index < QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_MAX &&
	     attr_index < DP_TCL_DESC_TYPE_MAX; attr_index++) {
		if (nla_put_u32(vendor_event, attr_index + 1,
				ingress_tx_stats->desc_type[attr_index])) {
			ath12k_err(NULL, "nla put failure: Ingress stats attr %d type %d",
				   QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
				   attr_index + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, attr);

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
	if (!attr) {
		ath12k_err(NULL,
			   "nla nest failure: vif ingress stats mcast");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_PKTS,
			ingress_tx_stats->mcast.packets)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d packets",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}

	if (nla_put_u64_64bit(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PKTINFO_BYTES,
			      ingress_tx_stats->mcast.bytes,
			      NL80211_ATTR_PAD)) {
		ath12k_err(NULL, "nla put failure: Ingress stats attr %d bytes",
			   QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_MCAST);
		nla_nest_end(vendor_event, attr);
		return -EINVAL;
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_tx_ingress_stats(struct sk_buff *vendor_event,
					struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	struct nlattr *attr1;
	struct nlattr *attr;
	int ring_num;

	attr = nla_nest_start(vendor_event,
			      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT);
	if (!attr) {
		ath12k_err(NULL, "nla nest failure: vif ingress stats");
		return -EINVAL;
	}

	for (ring_num = 0; ring_num < DP_TCL_NUM_RING_MAX &&
	     ring_num < QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX; ring_num++) {
		attr1 = nla_nest_start(vendor_event, ring_num + 1);
		if (!attr1) {
			ath12k_err(NULL,
				   "nla nest failure: vif ingress stats - ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}

		if (ath12k_fill_tx_ingress_stats_attrs(vendor_event,
						       &telemetry_vif->aggr_vif_stats.stats[ring_num].tx_i,
						       telemetry_vif->is_extended)) {
			ath12k_err(NULL,
				   "Error filling peer tx ingress stats for ring %d",
				   ring_num + 1);
			nla_nest_cancel(vendor_event, attr1);
			nla_nest_cancel(vendor_event, attr);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr1);
	}
	nla_nest_end(vendor_event, attr);

	return 0;
}

static int ath12k_fill_vap_tx_stats(struct ath12k *ar,
				    struct sk_buff *vendor_event,
				    struct ath12k_telemetry_dp_vif *telemetry_vif)
{
	int ret;

	/* Aggregated sta tx Stats */
	ret = ath12k_fill_peer_tx_stats(ar,
					vendor_event,
					&telemetry_vif->aggr_vif_stats.peer_stats,
					telemetry_vif->is_extended,
					&telemetry_vif->aggr_vif_stats.link_peer_stats,
					&telemetry_vif->aggr_vif_stats.mld_stats,
					ATH12K_PEER_INVAL);
	if (ret) {
		ath12k_err(NULL, "Error filling vap tx stats");
		return ret;
	}

	/* Ingress tx stats */
	ret = ath12k_fill_tx_ingress_stats(vendor_event, telemetry_vif);

	return ret;
}

static int ath12k_prepare_vif_vendor_event(struct sk_buff *vendor_event,
					   struct ath12k_vif *ahvif,
					   struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_vif *telemetry_vif;
	struct ath12k_dp_proto_stats_peer *peer_proto;
	struct ath12k_dp_proto_stats_vif *vif_proto;
	struct ath12k_rx_ppdu_stats *rx_mon_stats;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct ath12k_dp_link_peer_hw_stats *hw_link_stats;
	struct ath12k_dp_peer_hw_stats *hw_stats;
	struct ath12k *ar = &ahvif->ah->radio[0];
	struct nlattr *attr;
	u8 index;
	int ret = -EINVAL;

	if (!cmd) {
		ath12k_err(NULL, "cmd parameter is NULL");
		return ret;
	}

	telemetry_vif = vmalloc(sizeof(*telemetry_vif));
	if (!telemetry_vif) {
		ath12k_err(NULL, "Allocation failed for vap stats");
		return -ENOMEM;
	}

	memset(telemetry_vif, 0, sizeof(*telemetry_vif));

	tx_ppdu_stats = vzalloc(sizeof(*tx_ppdu_stats));
	if (!tx_ppdu_stats) {
		vfree(telemetry_vif);
		return -ENOMEM;
	}

	telemetry_vif->aggr_vif_stats.link_peer_stats.tx_ppdu_stats = tx_ppdu_stats;
	/* Allocate RX monitor stats for vif aggregation */
	rx_mon_stats = vzalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats) {
		vfree(tx_ppdu_stats);
		vfree(telemetry_vif);
		return -ENOMEM;
	}
	telemetry_vif->aggr_vif_stats.link_peer_stats.rx_ppdu_stats = rx_mon_stats;

	peer_proto = vzalloc(sizeof(*peer_proto));
	if (!peer_proto) {
		vfree(rx_mon_stats);
		vfree(tx_ppdu_stats);
		vfree(telemetry_vif);
		return -ENOMEM;
	}
	telemetry_vif->aggr_vif_stats.peer_stats.proto = peer_proto;

	vif_proto = vzalloc(sizeof(*vif_proto) * DP_TCL_NUM_RING_MAX);
	if (!vif_proto) {
		vfree(peer_proto);
		vfree(rx_mon_stats);
		vfree(tx_ppdu_stats);
		vfree(telemetry_vif);
		return -ENOMEM;
	}

	for (index = 0; index < DP_TCL_NUM_RING_MAX; index++)
		telemetry_vif->aggr_vif_stats.stats[index].proto = (vif_proto + index);

	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
		hw_link_stats = vzalloc(sizeof(*hw_link_stats));
		if (!hw_link_stats) {
			vfree(vif_proto);
			vfree(peer_proto);
			vfree(rx_mon_stats);
			vfree(tx_ppdu_stats);
			vfree(telemetry_vif);
			return -ENOMEM;
		}
		telemetry_vif->aggr_vif_stats.link_peer_stats.hw_link_stats =
									hw_link_stats;

		hw_stats = vzalloc(sizeof(*hw_stats));
		if (!hw_stats) {
			vfree(hw_link_stats);
			vfree(vif_proto);
			vfree(peer_proto);
			vfree(rx_mon_stats);
			vfree(tx_ppdu_stats);
			vfree(telemetry_vif);
			return -ENOMEM;
		}
		telemetry_vif->aggr_vif_stats.mld_stats.hw_stats = hw_stats;
	}

	if (ath12k_send_cp_event(cmd, vendor_event, ahvif))
		ath12k_err(NULL, "Failed to send cp event");

	ath12k_dp_get_vif_stats(ahvif, telemetry_vif, cmd->link_id);

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_vap_rx_stats(ar,
						     vendor_event,
						     telemetry_vif)) {
				ath12k_err(NULL, "Error filling vap rx stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Vap rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_vap_tx_stats(ar,
						     vendor_event,
						     telemetry_vif)) {
				ath12k_err(NULL, "Error filling vap tx stats");
				goto out;
			}

			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: Vap tx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_proto) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_PROTO_EVENT);
		if (attr) {
			if (ath12k_fill_vap_proto_stats(ar,
							vendor_event,
							telemetry_vif)) {
				ath12k_err(NULL, "nla put failure: vap Proto stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(NULL, "nla nest failure: vap Proto feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
		vfree(hw_stats);
		vfree(hw_link_stats);
	}
	vfree(vif_proto);
	vfree(peer_proto);
	vfree(tx_ppdu_stats);
	vfree(rx_mon_stats);
	vfree(telemetry_vif);
	return ret;
}

static int ath12k_stats_vif_setup(struct ath12k_telemetry_command *cmd)
{
	struct ath12k_vif *ahvif = NULL;
	struct ath12k *ar;
	bool is_hw_stats;
	struct sk_buff *vendor_event;
	int len, ret;

	if (!cmd->wdev) {
		ath12k_err(NULL, "wdev not present\n");
		return -EINVAL;
	}

	ahvif = ath12k_get_ahvif_from_wdev(cmd->wdev);

	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present\n");
		return -EINVAL;
	}

	ar = ahvif->deflink.ar;

	if (ahvif->deflink.ar && ahvif->deflink.ar->ab &&
	    test_bit(ATH12K_FLAG_CRASH_FLUSH, &ahvif->deflink.ar->ab->dev_flags)) {
		ath12k_err(NULL, "VAP stats return. Recovery in progress\n");
		return -EINVAL;
	}

	if (cmd->link_id != INVALID_LINK_ID &&
	    (cmd->link_id >= ATH12K_NUM_MAX_LINKS ||
	     !(ahvif->links_map & BIT(cmd->link_id)))) {
		ath12k_err(NULL, "Invalid link_id");
		return -EINVAL;
	}

	is_hw_stats = ath12k_dp_hw_peer_stats_enabled(ar ? &ar->dp : NULL);
	len = ath12k_get_dp_vendor_event_len(cmd, is_hw_stats);
	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d", len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT,
						   GFP_KERNEL);

	if (!vendor_event) {
		ath12k_err(NULL, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_vif_vendor_event(vendor_event, ahvif, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;
out:
	ath12k_err(NULL, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

static int ath12k_fill_radio_rx_stats(struct ath12k *ar,
				      struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	struct ath12k_dp_aggr_pdev_stats *aggr_pdev_stats;
	int ret;

	aggr_pdev_stats = &telemetry_radio->aggr_pdev_stats;

	/* Aggregated peer rx stats */
	ret = ath12k_fill_peer_rx_stats(ar,
					vendor_event,
					&aggr_pdev_stats->peer_stats,
					&aggr_pdev_stats->link_peer_stats,
					&aggr_pdev_stats->mld_stats,
					&aggr_pdev_stats->rx_pkt_ppdu_stats,
					telemetry_radio->is_extended,
					ATH12K_PEER_INVAL);

	return ret;
}

static int ath12k_fill_radio_tx_stats(struct ath12k *ar,
				      struct sk_buff *vendor_event,
				      struct ath12k_telemetry_dp_radio *telemetry_radio)
{
	int ret;
	struct ath12k_dp_link_peer_stats *link_peer_stats;

	link_peer_stats = &telemetry_radio->aggr_pdev_stats.link_peer_stats;
	/* Aggregated peer tx stats */
	ret = ath12k_fill_peer_tx_stats(ar,
					vendor_event,
					&telemetry_radio->aggr_pdev_stats.peer_stats,
					telemetry_radio->is_extended,
					link_peer_stats,
					&telemetry_radio->aggr_pdev_stats.mld_stats,
					ATH12K_PEER_INVAL);

	return ret;
}

static int ath12k_fill_tid_rx_stats(struct sk_buff *vendor_event,
				    const struct ath12k_dp_aggr_pdev_tid_stats *tid_stats,
				    int tid, bool is_hw_stats)
{
	const struct ath12k_tid_rx_stats *rx = &tid_stats->tid_rx[tid];
	const struct ath12k_reo_error_stats *reo_err = &tid_stats->tid_reo_err[tid];
	const struct ath12k_rxdma_error_stats *rxdma_err = &tid_stats->tid_rxdma_err[tid];
	int fail_cnt_max = is_hw_stats ? DP_TID_RX_SW_DROP_MAX_EXT :
					 DP_TID_RX_SW_DROP_MAX;
	struct nlattr *tid_attr;
	struct nlattr *arr_attr;
	int i;

	tid_attr = nla_nest_start(vendor_event, tid + 1);
	if (!tid_attr)
		return -EINVAL;

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TID_RX_DELIVERED_TO_STACK,
			rx->delivered_to_stack) ||
	    nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TID_RX_MSDU_CNT,
			rx->msdu_cnt) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TID_RX_MCAST_MSDU_CNT,
			rx->mcast_msdu_cnt) ||
	    nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TID_RX_BCAST_MSDU_CNT,
			rx->bcast_msdu_cnt)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	/* RX fail counts */
	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_RX_FAIL_CNT);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < fail_cnt_max && i < ARRAY_SIZE(rx->fail_cnt); i++) {
		if (nla_put_u32(vendor_event, i + 1, rx->fail_cnt[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* REO error stats */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODE_INV,
			reo_err->reo_code_inv)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_RX_REO_ERR_CODES);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < HAL_REO_DEST_RING_ERROR_CODE_MAX &&
		    i < ARRAY_SIZE(reo_err->reo_code); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				reo_err->reo_code[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* RXDMA error stats */
	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODE_INV,
			rxdma_err->rxdma_code_inv)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_RX_RXDMA_ERR_CODES);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < HAL_REO_ENTR_RING_RXDMA_ECODE_MAX &&
		    i < ARRAY_SIZE(rxdma_err->rxdma_code); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				rxdma_err->rxdma_code[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* Delay stats */
	if (ath12k_fill_tid_delay_stats(vendor_event, &rx->to_stack_delay,
					QCA_VENDOR_ATTR_TID_RX_TO_STACK_DELAY) ||
	    ath12k_fill_tid_delay_stats(vendor_event, &rx->intfrm_delay,
					QCA_VENDOR_ATTR_TID_RX_INTFRM_DELAY)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	nla_nest_end(vendor_event, tid_attr);
	return 0;
}

static int ath12k_fill_tid_tx_stats(struct sk_buff *vendor_event,
				    const struct ath12k_dp_aggr_pdev_tid_stats *tid_stats,
				    int tid_idx, bool is_hw_stats)
{
	const struct ath12k_tid_tx_stats *tx = &tid_stats->tid_tx[tid_idx];
	struct nlattr *tid_attr;
	struct nlattr *arr_attr;
	int tqm_stats_cnt_max = is_hw_stats ? QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX_EXT :
					      QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX;
	int i;

	tid_attr = nla_nest_start(vendor_event, tid_idx + 1);
	if (!tid_attr)
		return -EINVAL;

       /* TQM status counts */
	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_TX_TQM_STATUS_CNT);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < tqm_stats_cnt_max && i < ARRAY_SIZE(tx->tqm_status_cnt); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				tx->tqm_status_cnt[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* HTT status counts */
	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_TX_HTT_STATUS_CNT);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX &&
		    i < ARRAY_SIZE(tx->htt_status_cnt); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				tx->htt_status_cnt[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* SW drop counts */
	arr_attr = nla_nest_start(vendor_event,
				  QCA_VENDOR_ATTR_TID_TX_SW_DROP_CNT);
	if (!arr_attr) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}
	for (i = 0; i < QCA_VENDOR_ATTR_TID_TX_SW_DROP_MAX &&
		    i < ARRAY_SIZE(tx->swdrop_cnt); i++) {
		if (nla_put_u32(vendor_event, i + 1,
				tx->swdrop_cnt[i])) {
			nla_nest_cancel(vendor_event, arr_attr);
			nla_nest_cancel(vendor_event, tid_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, arr_attr);

	/* Software queue delay histogram */
	if (ath12k_fill_tid_delay_stats(vendor_event, &tx->swq_delay,
					   QCA_VENDOR_ATTR_TID_TX_SWQ_DELAY)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	/* HW TX completion delay histogram */
	if (ath12k_fill_tid_delay_stats(vendor_event, &tx->hwtx_delay,
					   QCA_VENDOR_ATTR_TID_TX_HWTX_DELAY)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	/* Inter-frame delay histogram */
	if (ath12k_fill_tid_delay_stats(vendor_event, &tx->intfrm_delay,
					   QCA_VENDOR_ATTR_TID_TX_INTFRM_DELAY)) {
		nla_nest_cancel(vendor_event, tid_attr);
		return -EINVAL;
	}

	nla_nest_end(vendor_event, tid_attr);

	return 0;
}

static int ath12k_fill_radio_tid_stats(struct ath12k *ar,
				       struct sk_buff *vendor_event,
				       struct ath12k_dp_aggr_pdev_tid_stats *tid_stats)
{
	struct ath12k_base *ab = ar->ab;
	struct nlattr *tx_attr;
	struct nlattr *rx_attr;
	int tid;

	/* TX TID stats */
	tx_attr = nla_nest_start(vendor_event,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_TX_STATS);
	if (!tx_attr) {
		ath12k_err(ab, "nla nest failure: TID TX stats");
		return -EINVAL;
	}

	for (tid = 0; tid < QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS &&
	     tid < VOW_DATA_TID_MAX; tid++) {
		if (ath12k_fill_tid_tx_stats(vendor_event, tid_stats, tid,
					     ath12k_dp_hw_peer_stats_enabled(&ar->dp))) {
			ath12k_err(ab, "Error filling TID TX stats for tid %d",
				   tid);
			nla_nest_cancel(vendor_event, tx_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, tx_attr);

	/* RX TID stats */
	rx_attr = nla_nest_start(vendor_event,
				 QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_RX_STATS);
	if (!rx_attr) {
		ath12k_err(ab, "nla nest failure: TID RX stats");
		return -EINVAL;
	}

	for (tid = 0; tid < QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS &&
	     tid < VOW_DATA_TID_MAX; tid++) {
		if (ath12k_fill_tid_rx_stats(vendor_event, tid_stats, tid,
					     ath12k_dp_hw_peer_stats_enabled(&ar->dp))) {
			ath12k_err(ab, "Error filling TID RX stats for tid %d",
				   tid);
			nla_nest_cancel(vendor_event, rx_attr);
			return -EINVAL;
		}
	}
	nla_nest_end(vendor_event, rx_attr);

	return 0;
}

static int ath12k_fill_radio_mon_stats(struct sk_buff *vendor_event,
				       struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp_stats *stats;
	struct ath12k_dp_mon *dp_mon;

	stats = &dp_pdev->dp_mon_pdev->mon_stats;
	dp_mon = dp_pdev->dp_mon_pdev->dp_mon;

	if (!dp_mon) {
		ath12k_err(NULL, "dp_mon not present");
		return -ENODEV;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_REAPED,
			stats->status_buf_reaped)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "status_buf_reaped");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_PROCESSED,
			stats->status_buf_processed)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "status_buf_processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_FREE,
			stats->status_buf_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "status_buf_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_STATUS_BUF_ERROR_FREE,
			stats->status_buf_error_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "status_buf_error_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_RING_DESC_EMPTY,
			stats->ring_desc_empty)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ring_desc_empty");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_RING_DESC_FLUSH,
			stats->ring_desc_flush)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ring_desc_flush");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_RING_DESC_TRUNC,
			stats->ring_desc_trunc)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ring_desc_trunc");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_PROCESSED,
			stats->pkt_tlv_processed)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "pkt_tlv_processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_FREE,
			stats->pkt_tlv_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "pkt_tlv_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_ERROR_FREE,
			stats->pkt_tlv_error_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "pkt_tlv_error_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_TO_MAC80211,
			stats->pkt_tlv_to_mac80211)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "pkt_tlv_to_mac80211");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_TRUNCATED,
			stats->pkt_tlv_truncated)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "pkt_tlv_truncated");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PKT_TLV_REAPED,
			stats->pkt_tlv_reaped)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "pkt_tlv_reaped");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_ALLOC,
			stats->num_skb_alloc)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_skb_alloc");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_FREE,
			stats->num_skb_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_skb_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_TO_MAC80211,
			stats->num_skb_to_mac80211)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "num_skb_to_mac80211");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_PPDU_REAPED,
			stats->num_ppdu_reaped)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_ppdu_reaped");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_PPDU_PROCESSED,
			stats->num_ppdu_processed)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_ppdu_processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_RAW,
			stats->num_skb_raw)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_skb_raw");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_FRAG_RAW,
			stats->num_frag_raw)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_frag_raw");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_SKB_ETH,
			stats->num_skb_eth)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_skb_eth");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NUM_FRAG_ETH,
			stats->num_frag_eth)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_frag_eth");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_DROP_TLV,
			stats->drop_tlv)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "drop_tlv");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_USED,
			stats->ppdu_desc_used)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ppdu_desc_used");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_PROC,
			stats->ppdu_desc_proc)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ppdu_desc_proc");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_FREE,
			stats->ppdu_desc_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "ppdu_desc_free");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_PPDU_DESC_FREE_LIST_EMPTY_CNT,
			stats->ppdu_desc_free_list_empty_cnt)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "ppdu_desc_free_list_empty_cnt");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_RESTITCH_INSUFF_FRAGS_CNT,
			stats->restitch_insuff_frags_cnt)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "restitch_insuff_frags_cnt");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_INVALID_STATUS_MAGIC_NUM,
			stats->invalid_status_magic_num)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "invalid_status_magic_num");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_INVALID_PKT_MAGIC_NUM,
			stats->invalid_pkt_magic_num)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "invalid_pkt_magic_num");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_NULL_MPDU_Q,
			stats->null_mpdu_q)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "null_mpdu_q");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_SKB_ALLOC_FAIL,
			stats->skb_alloc_fail)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "skb_alloc_fail");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_RX_HDR_NOT_RCVD,
			stats->rx_hdr_not_rcvd)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "rx_hdr_not_rcvd");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_MIN_FRAGS_UNAVAILABLE,
			stats->min_frags_unavailable)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "min_frags_unavailable");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_INVALID_MPDU_HDR_LEN,
			stats->invalid_mpdu_hdr_len)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "invalid_mpdu_hdr_len");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_INVALID_IN_USE,
			stats->invalid_in_use)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "invalid_in_use");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_INVALID_END_OFFSET,
			stats->invalid_end_offset)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "invalid_end_offset");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_MON_STATS_STATUS_FRAG_ADD_TO_SKB,
			stats->status_frag_add_to_skb)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s",
			   "status_frag_add_to_skb");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_STATUS_NUM_FRAG_REPLENISH,
			dp_mon->num_frag_replenish)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_frag_replenish");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_MON_STATS_STATUS_NUM_FRAG_FREE,
			dp_mon->num_frag_free)) {
		ath12k_err(NULL, "nla put failure: mon_stats-> %s", "num_frag_free");
		return -EMSGSIZE;
	}

	return 0;
}


static int ath12k_fill_radio_tx_mon_stats(struct sk_buff *vendor_event,
				       struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp_tx_mon_stats *tx_mon_stats;
	struct ath12k_pdev_tx_mon_stats *pdev_tx_mon_stats;
	struct ath12k_tx_mon_ssr_stats *ssr_stats;
	struct ath12k_dp_mon *dp_mon;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct ath12k_pdev_tx_mon *tx_pdev_mon;
	u8 tx_mon_mode = 0;
	u8 tx_ext_mon_filter_mode = 0;

	if (unlikely(!dp_pdev->dp_mon_pdev)) {
		ath12k_err(NULL, "dp_mon_pdev not initialized");
		return -ENODEV;
	}

	dp_mon = dp_pdev->dp_mon_pdev->dp_mon;
	if (unlikely(!dp_mon || !dp_mon->dp_tx_mon)) {
		ath12k_err(NULL, "dp_mon_tx not present");
		return -ENODEV;
	}

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	tx_pdev_mon = dp_pdev->dp_mon_pdev->dp_pdev_tx_mon;
	tx_mon_stats = &dp_mon->dp_tx_mon->tx_mon_stats;
	ath12k_dp_mon_tx_update_buf_ownership_stats(dp_pdev->dp);

	if (tx_pdev_mon && tx_pdev_mon->tx_monitor_started)
		/**
		 * TX_monitor_mode stores the internal dp_mon_tx_filter_mode enum value
		 * which is zero-based (DP_MON_TX_FULL_MONITOR=0, HYBRID=1, EXT_MON=2,
		 * SPL_PKT_CAP=3).
		 * The vendor attribute encoding is one-based so that 0
		 * can be reserved for "TX monitor disabled" — callers that check
		 * tx_mon_mode==0 treat it as disabled. Add 1 to convert internal
		 * zero-based mode to the vendor one-based encoding.
		 */
		tx_mon_mode = tx_pdev_mon->tx_monitor_mode + 1;

	if (mon_ops && mon_ops->ext_mon_get_filter_mode)
		tx_ext_mon_filter_mode = mon_ops->ext_mon_get_filter_mode(dp_pdev);

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_REPLENISHED,
			tx_mon_stats->buf_replenished)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "replenished buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_IN_REAP,
			tx_mon_stats->in_reap)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "buffers in reap");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_IN_HARDWARE,
			tx_mon_stats->with_hw)) {
		ath12k_err(NULL, "nla put failure: tx mon stats %s",
			   "buffers with hardware");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_FREE,
			tx_mon_stats->free)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "free buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_REPLENISH_ERR,
			tx_mon_stats->replenish_err)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "buffers having replenish err");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_PROC_ERR,
			tx_mon_stats->proc_err)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "buffers having processing err");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_BUF_ALLOC_FAILED,
			tx_mon_stats->alloc_fail)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "buffers allocation failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_DMA_FAILED,
			tx_mon_stats->dma_fail)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "buffers dma failed");
		return -EMSGSIZE;
	}

	if (unlikely(!dp_pdev->dp_mon_pdev->dp_pdev_tx_mon)) {
		ath12k_err(NULL, "dp_pdev_tx_mon not present");
		return -ENODEV;
	}

	pdev_tx_mon_stats = &dp_pdev->dp_mon_pdev->dp_pdev_tx_mon->pdev_tx_mon_stats;
	ssr_stats = &dp_pdev->ar->tx_mon_ssr_stats;

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_NUM_BUFS_REAPED,
			pdev_tx_mon_stats->num_bufs_reaped)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "num buffers reaped");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_TRUNCATED_BUF,
			pdev_tx_mon_stats->truncated_buf)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "truncated buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_FLUSHED_BUF,
			pdev_tx_mon_stats->flushed_buf)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "flushed buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_NULL_BUF,
			pdev_tx_mon_stats->null_buf)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "null buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_MON_DESC_FREE,
			pdev_tx_mon_stats->mon_desc_free)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "free monitor descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PKT_BUF_NULL,
			pdev_tx_mon_stats->pkt_buf_null)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "null pkt buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_BUF_NULL,
			pdev_tx_mon_stats->status_buf_null)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "null status buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PREP_WQ_FAILED,
			pdev_tx_mon_stats->prep_wq_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "prep wq failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_EMPTY_DESCRIPTORS,
			pdev_tx_mon_stats->empty_descriptor)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "empty descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_PROCESSED,
			pdev_tx_mon_stats->ppdu_processed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "ppdu processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_DESC_PROCESSED,
			pdev_tx_mon_stats->status_desc_processed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "status descriptors processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_DESC_OVERFLOW,
			pdev_tx_mon_stats->ppdu_desc_overflow)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ppdu descriptors overflow");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_ZERO_STATUS_DESC,
			pdev_tx_mon_stats->zero_status_desc)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "zero status descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PPDU_PREP_FAILED,
			pdev_tx_mon_stats->ppdu_prep_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ppdu prep failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_TLV_PROCESS_FAILED,
			pdev_tx_mon_stats->tlv_process_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "tlv processing failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_DATA_GEN_FAILED,
			pdev_tx_mon_stats->data_gen_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "data generation failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_BUF_EXTRACT_FAILED,
			pdev_tx_mon_stats->buf_extract_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "buffer extraction failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_MAGIC_VALUE_ERROR,
			pdev_tx_mon_stats->magic_value_error)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "magic value error");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PKT_BUF_FREE,
			pdev_tx_mon_stats->pkt_buf_free)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "free pkt buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_STATUS_BUF_FREE,
			pdev_tx_mon_stats->status_buf_free)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "free status buffers");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_MU_USER_FRAME,
			pdev_tx_mon_stats->mu_user_frame)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s", "mu user frames");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_DATA_PPDU_DELIVERED,
			pdev_tx_mon_stats->data_ppdu_delivered)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "non-protection ppdu delivered");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PROT_PPDU_DELIVERED,
			pdev_tx_mon_stats->prot_ppdu_delivered)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "prot ppdu delivered");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_SELF_GEN_FAILED,
			pdev_tx_mon_stats->self_gen_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "self gen frames failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_SKB_ALLOC_FAILED,
			pdev_tx_mon_stats->skb_alloc_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "skb alloc failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_RING_EXTRACT_FAILED,
			pdev_tx_mon_stats->ring_extract_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ring extraction failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_GET_NUM_USERS_FAILED,
			pdev_tx_mon_stats->get_num_users_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "get num users failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_PACKET_BUF_PROCESSED,
			pdev_tx_mon_stats->pkt_buf_processed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "pkt buffers processed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_RESTART_ATTEMPTS,
			ssr_stats->restart_attempts)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr restart attempts");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_RESTART_SUCCESS,
			ssr_stats->restart_success)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr restart success");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_RESTART_NO_POOL,
			ssr_stats->restart_no_pool)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr restart no pool");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_RESTART_FAIL,
			ssr_stats->restart_fail)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr restart failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_WORK_CANCEL,
			ssr_stats->work_cancel)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr work cancel");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_MON_DESC_FLUSHED,
			ssr_stats->mon_desc_flushed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr flushed mon descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_PPDU_DESC_DRAINED,
			ssr_stats->ppdu_desc_drained)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr drained ppdu descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_SSR_STATUS_DESC_DRAINED,
			ssr_stats->status_desc_drained)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "ssr drained status descriptors");
		return -EMSGSIZE;
	}

	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_TX_MON_MODE,
		       tx_mon_mode)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: tx monitor mode");
		return -EMSGSIZE;
	}

	if (nla_put_u8(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_EXT_MON_FILTER_MODE,
		       tx_ext_mon_filter_mode)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "extended monitor filter mode");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_FRAMES_DROP_IN_SW,
			pdev_tx_mon_stats->frames_drop_in_sw)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: frames drop by host");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event, QCA_VENDOR_ATTR_TX_MON_STATS_WMI_PEER_SEND_FAILED,
			pdev_tx_mon_stats->wmi_peer_send_failed)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: wmi peer send failed");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_TOTAL_FRAMES_DELIVERED,
			pdev_tx_mon_stats->total_frames_delivered)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: total frames delivered");
		return -EMSGSIZE;
	}

	if (nla_put_u32(vendor_event,
			QCA_VENDOR_ATTR_TX_MON_STATS_CUSTOM_CALL_BACK_DELIVERED,
			pdev_tx_mon_stats->custom_call_back_delivered)) {
		ath12k_err(NULL, "nla put failure: tx mon stats: %s",
			   "frames delivered via custom call back");
		return -EMSGSIZE;
	}

	return 0;
}

static int ath12k_prepare_radio_vendor_event(struct sk_buff *vendor_event,
					     struct ath12k_pdev_dp *dp_pdev,
					     struct ath12k_telemetry_command *cmd)
{
	struct ath12k_telemetry_dp_radio *telemetry_radio;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_base *ab = dp_pdev->ar->ab;
	struct ath12k_htt_tx_ppdu_stats *tx_ppdu_stats;
	struct ath12k_rx_ppdu_stats *rx_mon_stats;
	struct ath12k_dp_aggr_pdev_tid_stats *flat_tid_stats;
	struct ath12k_dp_link_peer_hw_stats *hw_link_stats;
	struct ath12k_dp_peer_hw_stats *hw_stats;
	struct nlattr *attr;
	int ret = -EINVAL;
	u32 dp_tx_failed = 0;
	int ring;

	/* TID stats */
	if (cmd->feat.feat_tid) {
		flat_tid_stats = vzalloc(sizeof(*flat_tid_stats));
		if (!flat_tid_stats)
			return -ENOMEM;

		if (ath12k_dp_pdev_get_tid_stats(ar, flat_tid_stats)) {
			ath12k_err(ab, "Error getting pdev TID stats");
			vfree(flat_tid_stats);
			return -EINVAL;
		}

		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TID_STATS_EVENT);
		if (!attr) {
			ath12k_err(ab, "nla nest failure: Radio TID stats");
			vfree(flat_tid_stats);
			return -EINVAL;
		}

		if (ath12k_fill_radio_tid_stats(ar, vendor_event, flat_tid_stats)) {
			ath12k_err(ab, "Error filling radio TID stats");
			nla_nest_cancel(vendor_event, attr);
			vfree(flat_tid_stats);
			return -EINVAL;
		}
		nla_nest_end(vendor_event, attr);
		vfree(flat_tid_stats);
		return 0;
	}

	telemetry_radio = vmalloc(sizeof(*telemetry_radio));
	if (!telemetry_radio) {
		ath12k_err(ab, "Allocation failure for radio_stats");
		return -EINVAL;
	}

	memset(telemetry_radio, 0, sizeof(*telemetry_radio));

	tx_ppdu_stats = vzalloc(sizeof(*tx_ppdu_stats));
	if (!tx_ppdu_stats) {
		vfree(telemetry_radio);
		return -ENOMEM;
	}
	telemetry_radio->aggr_pdev_stats.link_peer_stats.tx_ppdu_stats = tx_ppdu_stats;

	rx_mon_stats = vzalloc(sizeof(*rx_mon_stats));
	if (!rx_mon_stats) {
		vfree(tx_ppdu_stats);
		vfree(telemetry_radio);
		return -ENOMEM;
	}
	telemetry_radio->aggr_pdev_stats.link_peer_stats.rx_ppdu_stats = rx_mon_stats;

	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
		hw_link_stats = vzalloc(sizeof(*hw_link_stats));
		if (!hw_link_stats) {
			vfree(rx_mon_stats);
			vfree(tx_ppdu_stats);
			vfree(telemetry_radio);
			return -ENOMEM;
		}
		telemetry_radio->aggr_pdev_stats.link_peer_stats.hw_link_stats =
									hw_link_stats;

		hw_stats = vzalloc(sizeof(*hw_stats));
		if (!hw_stats) {
			vfree(hw_link_stats);
			vfree(rx_mon_stats);
			vfree(tx_ppdu_stats);
			vfree(telemetry_radio);
			return -ENOMEM;
		}
		telemetry_radio->aggr_pdev_stats.mld_stats.hw_stats = hw_stats;
	}

	ath12k_dp_get_pdev_stats(dp_pdev, telemetry_radio);

	/* Compute total TX failures from aggregated pdev stats (includes
	 * both live and deleted peer stats).
	 */
	for (ring = 0; ring < DP_TCL_NUM_RING_MAX; ring++)
		dp_tx_failed +=
			telemetry_radio->aggr_pdev_stats.peer_stats.tx[ring].tx_failed;

	/* Fill radio CP stats (pdev-level control path stats) */
	if ((cmd->feat.feat_tx || cmd->feat.feat_rx) &&
	    ath12k_fill_radio_cp_stats(telemetry_radio, cmd, vendor_event, ar,
				       dp_tx_failed)) {
		ath12k_err(ab, "Error filling radio CP stats");
		goto out;
	}

	if (cmd->feat.feat_rx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_rx_stats(ar,
						       vendor_event,
						       telemetry_radio)) {
				ath12k_err(ab,
					   "Error filling radio rx feat stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "nla nest failure: Radio rx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_tx_stats(ar, vendor_event,
						       telemetry_radio)) {
				ath12k_err(ab,
					   "Error filling radio tx feat stats");
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "NLA nest failure: Radio tx feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_mon_stats) {
		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_MON_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_mon_stats(vendor_event, dp_pdev)) {
				ath12k_err(ab,
					   "Error filling radio mon feat stats");
				nla_nest_cancel(vendor_event, attr);
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "nla nest failure: Radio mon feat stats");
			goto out;
		}
	}

	if (cmd->feat.feat_tx_mon_stats) {
		if (!ath12k_dp_tx_mon_feature_eval(dp_pdev->dp)) {
			ath12k_err(NULL, "TX monitor feature not available");
			goto out;
		}

		attr = nla_nest_start(vendor_event,
				      QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_MON_STATS_EVENT);
		if (attr) {
			if (ath12k_fill_radio_tx_mon_stats(vendor_event, dp_pdev)) {
				ath12k_err(ab,
					   "Error filling radio tx mon feat stats");
				nla_nest_cancel(vendor_event, attr);
				goto out;
			}
			nla_nest_end(vendor_event, attr);
		} else {
			ath12k_err(ab, "nla nest failure: Radio tx mon feat stats");
			goto out;
		}
	}

	ret = 0;
out:
	if (ath12k_dp_hw_peer_stats_enabled(&ar->dp)) {
		vfree(hw_stats);
		vfree(hw_link_stats);
	}
	vfree(tx_ppdu_stats);
	vfree(rx_mon_stats);
	vfree(telemetry_radio);
	return ret;
}

static int ath12k_stats_radio_setup(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_pdev_dp *dp_pdev;
	struct sk_buff *vendor_event;
	struct ath12k *ar;
	int len, ret;

	if (cmd->link_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid HW Link ID %d", cmd->link_id);
		return -EINVAL;
	}

	ar = &ah->radio[cmd->link_id];
	if (!ar) {
		ath12k_err(NULL, "ar not present");
		return -EINVAL;
	}

	if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)) {
		ath12k_err(ar->ab, "Radio stats return. Recovery in progress\n");
		return -EINVAL;
	}

	dp_pdev = &ar->dp;
	if (!dp_pdev) {
		ath12k_err(ar->ab, "dp_pdev not present");
		return -EINVAL;
	}

	len = ath12k_get_dp_vendor_event_len(cmd,
					     ath12k_dp_hw_peer_stats_enabled(&ar->dp));
	ath12k_dbg(ar->ab, ATH12K_DBG_TELEMETRY, "Vendor Event Length = %d",
		   len);

	vendor_event = cfg80211_vendor_event_alloc(cmd->wiphy, cmd->wdev, len,
						   QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT,
						   GFP_KERNEL);
	if (!vendor_event) {
		ath12k_err(ar->ab, "Error allocating vendor event");
		return -EINVAL;
	}

	ret = ath12k_prepare_telemetry_common_vendor_attr(vendor_event, cmd);
	if (ret)
		goto out;

	ret = ath12k_prepare_radio_vendor_event(vendor_event, dp_pdev, cmd);
	if (ret)
		goto out;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return ret;

out:
	ath12k_err(ar->ab, "Error sending telemetry vendor event");
	kfree_skb(vendor_event);
	return ret;
}

int ath12k_wifi_stats_reply_setup(struct ath12k_telemetry_command *cmd)
{
	int ret;

	switch (cmd->obj) {
	case STATS_OBJ_PEER:
		ret = ath12k_stats_peer_setup(cmd);
		break;
	case STATS_OBJ_VIF:
		ret = ath12k_stats_vif_setup(cmd);
		break;
	case STATS_OBJ_RADIO:
		ret = ath12k_stats_radio_setup(cmd);
		break;
	case STATS_OBJ_DEVICE:
		ret = ath12k_stats_device_setup(cmd);
		break;
	default:
		ath12k_err(NULL, "Invalid obj type\n");
		ret = -EINVAL;
	}

	return ret;
}

static int ath12k_wifi_stats_reply_setup_schedule(struct ath12k_telemetry_command *cmd)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(cmd->wiphy);
	struct ath12k_stats_list_entry *stats_entry;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag;
	struct ath12k *ar;

	stats_entry = kzalloc(sizeof(*stats_entry), GFP_KERNEL);
	if (!stats_entry) {
		ath12k_err(NULL, "Allocation failure for stats_entry\n");
		return -EINVAL;
	}

	ar = ah->radio;
	if (!ar) {
		ath12k_err(NULL, "No radio present\n");
		kfree(stats_entry);
		return -EINVAL;
	}

	if (ar->ab && ar->ab->ag) {
		ag = ar->ab->ag;
	} else {
		ath12k_err(ar->ab, "ag not found\n");
		kfree(stats_entry);
		return -EINVAL;
	}

	memcpy(&stats_entry->usr_command, cmd,
	       sizeof(struct ath12k_telemetry_command));

	list_add_tail(&stats_entry->node, &ag->stats_work.work_list);

	wiphy_work_queue(ah->hw->wiphy, &ag->stats_work.stats_nb_work);

	return 0;
}

static int
ath12k_vendor_set_rtt_responder_role(struct wireless_dev *wdev,
				     struct ath12k_wifi_generic_params *params)
{
	struct ieee80211_vif *vif;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ath12k *ar;
	u32 supported_role;
	u32 role;
	u8 link_id;
	int ret;

	if (!wdev || !params)
		return -EINVAL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	if (vif->type != NL80211_IFTYPE_AP)
		return -EOPNOTSUPP;

	role = params->value;
	if (role && role & ~ATH12K_RTT_RESPONDER_ROLE_MASK)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	if (params->link_id != INVALID_LINK_ID) {
		link_id = params->link_id;
		if (link_id >= ATH12K_NUM_MAX_LINKS ||
		    !(ahvif->links_map & BIT(link_id)))
			return -EINVAL;
	} else {
		link_id = 0;
	}

	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif || !arvif->ar)
		return -EINVAL;

	ar = arvif->ar;
	supported_role = ath12k_supported_rtt_responder_roles(ar->ab);
	role &= supported_role;
	if (!role)
		return -EOPNOTSUPP;

	ret = ath12k_wmi_vdev_set_param_cmd(ar, arvif->vdev_id,
		WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_RESPONDER_ROLE,
		role);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to set RTT responder role 0x%x on vdev %i: %d\n",
			    role, arvif->vdev_id, ret);
		return ret;
	}

	arvif->rtt_ctx.rtt_responder_role = role;

	ath12k_dbg(ar->ab, ATH12K_DBG_CFG,
		   "set RTT responder role 0x%x supported 0x%x vdev %i link %u\n",
		   role, supported_role, arvif->vdev_id, link_id);

	return 0;
}

/**
 * ath12k_vendor_send_pasn_event() - Send a QCA PASN vendor event to userspace.
 * @wiphy: wiphy of the AP interface.
 * @wdev: wireless device of the AP interface.
 * @link_id: MLD link ID, or 0 for non-MLO.
 * @action: QCA_WLAN_VENDOR_PASN_ACTION_AUTH or
 *          QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT.
 * @src_addr: local MAC address for this PASN session, or NULL.
 * @peer_addr: remote MAC address of the PASN peer.
 * @ltf_keyseed_required: true if the initiator requires an LTF key seed.
 *
 * Builds and delivers a QCA_NL80211_VENDOR_SUBCMD_PASN vendor event carrying
 * one peer entry. Used to request PASN authentication from hostapd (AUTH
 * action) or to notify hostapd that the secure ranging context should be
 * removed (DELETE action).
 *
 * Returns 0 on success or a negative errno on failure.
 */
int ath12k_vendor_send_pasn_event(struct wiphy *wiphy, struct wireless_dev *wdev,
				  u8 link_id, u32 action, const u8 *src_addr,
				  const u8 *peer_addr, bool ltf_keyseed_required)
{
	struct nlattr *peers;
	struct nlattr *peer;
	struct sk_buff *skb;
	int ret;

	if (!wiphy || !wdev || !peer_addr)
		return -EINVAL;

	skb = cfg80211_vendor_event_alloc(wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_PASN_EVENT_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	ret = nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PASN_ACTION, action);
	if (ret)
		goto nla_fail;

	ret = nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID, link_id);
	if (ret)
		goto nla_fail;

	peers = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEERS);
	if (!peers) {
		ret = -EMSGSIZE;
		goto nla_fail;
	}

	peer = nla_nest_start(skb, 0);
	if (!peer) {
		ret = -EMSGSIZE;
		goto nla_fail;
	}

	if (src_addr) {
		ret = nla_put(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR,
			      ETH_ALEN, src_addr);
		if (ret)
			goto nla_fail;
	}

	ret = nla_put(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR,
		      ETH_ALEN, peer_addr);
	if (ret)
		goto nla_fail;

	if (ltf_keyseed_required) {
		ret = nla_put_flag(skb,
				   QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED);
		if (ret)
			goto nla_fail;
	}

	nla_nest_end(skb, peer);
	nla_nest_end(skb, peers);
	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;

nla_fail:
	ath12k_err(NULL,
		   "failed to build PASN vendor event action %u for peer %pM ret %d\n",
		   action, peer_addr, ret);
	kfree_skb(skb);
	return ret;
}

static int ath12k_vendor_wlan_telemetry_wiphy_getstats(struct wiphy *wiphy,
						       struct wireless_dev *wdev,
						       const void *data,
						       int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1];
	struct ath12k_telemetry_command cmd = {0};
	int ret;

	ret = nla_parse(tb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX, data, data_len,
			ath12k_wlan_telemetry_req_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "nla parse failure: Getstats wiphy telemetry\n");
		return ret;
	}

	cmd.wiphy = wiphy;
	cmd.svc_id = INVALID_SVC_ID;

	if (ath12k_extract_user_inputs(tb, &cmd)) {
		ath12k_err(NULL, "Error parsing user input\n");
		return -EINVAL;
	}

	ret = ath12k_wifi_stats_reply_setup_schedule(&cmd);

	return ret;
}

static int ath12k_vendor_wlan_telemetry_wdev_getstats(struct wiphy *wiphy,
						      struct wireless_dev *wdev,
						      const void *data,
						      int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX + 1];
	struct ath12k_telemetry_command cmd = {0};
	int ret;

	ret = nla_parse(tb, QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX, data, data_len,
			ath12k_wlan_telemetry_req_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "NLA Parse failure - Getstats wdev telemetry\n");
		return ret;
	}

	cmd.wiphy = wiphy;
	cmd.wdev = wdev;
	cmd.svc_id = INVALID_SVC_ID;

	if (ath12k_extract_user_inputs(tb, &cmd)) {
		ath12k_err(NULL, "Error parsing user input\n");
		return ret;
	}

	ret = ath12k_wifi_stats_reply_setup_schedule(&cmd);

	return ret;
}

/* Extract attributes from the NL message */
static void ath12k_vendor_wifi_extract_generic_command_params(struct nlattr **tb,
							      struct ath12k_wifi_generic_params *params)
{
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND])
		params->command = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE])
		params->value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]) {
		params->data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
		params->data_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH])
		params->length = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS])
		params->flags = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX])
		params->ifindex = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX]);
	else
		params->ifindex = 0xFFFFFFFF;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		params->link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
	else
		params->link_id = INVALID_LINK_ID;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX])
		params->radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);
	else
		params->radio_idx = INVALID_RADIO_INDEX;

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "wifi param: %d data: %p data len: %d ifindex: %d link id: %d radio idx: %d\n",
		   params->value, params->data, params->data_len,
		   params->ifindex, params->link_id, params->radio_idx);
}

static int
ath12k_vendor_iface_mode_config(struct wireless_dev *wdev,
				struct ath12k_wifi_generic_params *wifi_params)
{
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	enum wmi_peer_authorize_mode mode;
	struct ath12k *ar;
	u8 link_id;

	if (!wdev || !wifi_params)
		return -EINVAL;

	switch (wifi_params->value) {
	case QCA_WLAN_VENDOR_IFACE_MODE_CLEAR:
		mode = WMI_PEER_AUTHORIZE_CLEAR;
		break;
	case QCA_WLAN_VENDOR_IFACE_MODE_OPEN:
		mode = WMI_PEER_AUTHORIZE_OPEN_MODE;
		break;
	case QCA_WLAN_VENDOR_IFACE_MODE_SECURED:
		mode = WMI_PEER_AUTHORIZE_SECURED_MODE;
		break;
	default:
		ath12k_err(NULL, "%s: invalid iface mode %u\n", __func__,
			   wifi_params->value);
		return -EINVAL;
	}

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
		return 0;

	link_id = wifi_params->link_id == INVALID_LINK_ID ? 0 : wifi_params->link_id;
	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif || !arvif->ar)
		return -EINVAL;

	/* Use SW encryption authorization mode when SW encryption is enabled
	 * to ensure the correct encryption mode is communicated to firmware.
	 */
	ar = arvif->ar;
	if (mode == WMI_PEER_AUTHORIZE_SECURED_MODE)
		arvif->secured_bss = true;

	if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ar->ab->ag->flags) &&
	    mode == WMI_PEER_AUTHORIZE_SECURED_MODE)
		mode = WMI_PEER_AUTHORIZE_SW_ENCRYPTION_MODE;

	return ath12k_wmi_set_peer_param(ar, arvif->addr, arvif->vdev_id,
					 WMI_PEER_AUTHORIZE, mode);
}


static int ath12k_vendor_wifi_config_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_vif *ahvif = NULL;
	int ppe_vp_type = 0;
	u8 vap_submode;
	char *type = NULL;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);

	if (ret) {
		ath12k_err(NULL,
			   "Invalid attribute with vendor wifi config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wifi params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		if (ath12k_vendor_cmd_skip_check(wiphy, wdev, wifi_params,
						 QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
			return -EPERM;

		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid param command received\n");
				return -EINVAL;
			}
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_set_wifi_params_extn(wiphy, wdev,
							    &wifi_params);
#endif /* CPTCFG_QCN_EXTN */
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wifi params \n");
				return -EINVAL;
			}
			break;
		case QCA_WLAN_VENDOR_WIFI_PARAM_INTERFACE_EN_DIS_MODE:
			ret = ath12k_vendor_iface_mode_config(wdev, &wifi_params);
			if (ret)
				return ret;
			break;
		case QCA_WLAN_VENDOR_WIFI_PARAM_RTT_RESPONDER_ROLE:
			ret = ath12k_vendor_set_rtt_responder_role(wdev, &wifi_params);
			if (ret) {
				ath12k_dbg(NULL, ATH12K_DBG_CFG,
					   "failed to set RTT responder role: %d\n", ret);
				return ret;
			}
			break;
		default:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_wifi_config_handler_extn(wiphy, wdev,
								     &wifi_params);
			if (ret) {
				ath12k_dbg(NULL, ATH12K_DBG_CFG,
					   "Un-supported generic command\n");
				return ret;
			}
#endif /* CPTCFG_QCN_EXTN */
		}
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE]) {
		ppe_vp_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE]);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ppe_vp_type > PPE_VP_USER_TYPE_DS) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "ppe_vp_type value greater than 4 (%d)(%s)\n",
			   ppe_vp_type, wdev->netdev->name);
		return -EINVAL;
	}

	switch (ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
		type = "passive";
		break;
	case PPE_VP_USER_TYPE_ACTIVE:
		type = "active";
		break;
	case PPE_VP_USER_TYPE_DS:
		type = "ds";
		break;
	default:
		type = "passive";
		ppe_vp_type = 1;
		break;
	}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (!ath12k_ppe_ds_enabled) {
		type = "passive";
		ppe_vp_type = 1;
		pr_err("Overriding offload type to passive as DS isn't enabled\n");
	}
#else
	type = "passive";
	ppe_vp_type = 1;
	pr_err("Overriding offload type to passive as DS isn't enabled\n");
#endif
	if (wdev->ppe_vp_type != ppe_vp_type)
		wdev->ppe_vp_type = ppe_vp_type;
	else
		return ret;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif) {
		ath12k_err(NULL, "vif is NULL\n");
		return -EINVAL;
	}
	if (vif->type == NL80211_IFTYPE_AP_VLAN) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "vif is AP_VLAN\n");
		return ret;
	}
	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif) {
		ath12k_err(NULL, "ahvif is NULL\n");
		return -EINVAL;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ppe_vp_type != ATH12K_INVALID_PPE_VP_TYPE &&
	    ahvif->dp_vif.ppe_vp_num != ATH12K_INVALID_PPE_VP_NUM) {
		ret = ath12k_vif_update_vp_config(ahvif, ppe_vp_type);

		if (ret)
			pr_err("ppe_vp mode config update failed\n");
		else
			wdev->ppe_vp_type = ppe_vp_type;
	}
#endif

	pr_info("[%s] vendor cmd type [%s] %d (%s) state %d\n",
		current->comm,  wdev->netdev->name, wdev->ppe_vp_type,
		type, netif_running(wdev->netdev));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]) {
		vap_submode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]);
		if (vap_submode > QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MAX) {
			ath12k_err(NULL,
				   "%s-Invalid vap_submode: %d for(%s)\n", __func__,
				   wdev->vap_submode, wdev->netdev->name);
			return -EINVAL;
		}

		if (wdev->vap_submode == vap_submode) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "%s-vap_submode: %d for(%s) already configured\n",
				   __func__, wdev->vap_submode, wdev->netdev->name);
			return 0;
		}
		wdev->vap_submode = vap_submode;

		vif = wdev_to_ieee80211_vif(wdev);
		if (vif)
			ahvif = ath12k_vif_to_ahvif(vif);

		if (ahvif)
			ahvif->dp_vif.dp_features |= DP_FEATURE_MESH;

		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "%s-configured vap_submode: %d for(%s)\n", __func__,
			   wdev->vap_submode, wdev->netdev->name);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_VLAN]) {
		u8 dynamic_vlan = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_DYNAMIC_VLAN]);

		vif = wdev_to_ieee80211_vif(wdev);
		if (!vif || vif->type != NL80211_IFTYPE_AP) {
			ath12k_err(NULL, "%s-invalid vif for dynamic_vlan\n",
				   __func__);
			return -EINVAL;
		}

		ahvif = ath12k_vif_to_ahvif(vif);
		if (!ahvif) {
			ath12k_err(NULL, "%s-ahvif is NULL for dynamic_vlan\n",
				   __func__);
			return -EINVAL;
		}

		ahvif->u.ap.dynamic_vlan = !!dynamic_vlan;
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "%s-configured dynamic_vlan: %u for(%s)\n",
			   __func__, ahvif->u.ap.dynamic_vlan,
			   wdev->netdev->name);
	}

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_vendor_set_wifi_config_extn(wiphy, tb, wdev))
		return -EINVAL;
#endif /* CPTCFG_QCN_EXTN */

	return 0;
}

static int ath12k_vendor_wiphy_config_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);

	if (ret) {
		ath12k_err(NULL,
			   "Invalid attribute with vendor wiphy config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wiphy params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		if (ath12k_vendor_cmd_skip_check(wiphy, wdev, wifi_params,
						 QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION))
			return -EPERM;

		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid param command received\n");
				return -EINVAL;
			}
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_set_wiphy_params_extn(wiphy,
							    &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy params \n");
				return -EINVAL;
			}
#endif /* CPTCFG_QCN_EXTN */
			break;

#ifdef CPTCFG_QCN_EXTN
		case QCA_NL80211_VENDOR_RADIO_CONFIG_HWADDR:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid hwaddr received\n");
				return -EINVAL;
			}
			ret = ath12k_vendor_set_wiphy_hwaddr_extn(wiphy,
								  &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy hwaddr\n");
				return -EINVAL;
			}
			break;
		case QCA_NL80211_VENDOR_RADIO_SR_SELF_CONFIG:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid sr param command received\n");
				return -EINVAL;
			}
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_set_wiphy_sr_params_extn(wiphy,
								     &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wiphy hwaddr\n");
				return -EINVAL;
			}
#endif /* CPTCFG_QCN_EXTN */
			break;
		case QCA_NL80211_VENDOR_RADIO_SMART_ANT_CONFIG:
			if (!wifi_params.data) {
				ath12k_err(NULL,
					   "Invalid SA param command received\n");
				return -EINVAL;
			}
			ret = ath12k_vendor_set_wiphy_sa_configs(wiphy,
								 &wifi_params);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set SA config: %d\n", ret);
				return -EINVAL;
			}
			break;

#endif /* CPTCFG_QCN_EXTN */

		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

static int ath12k_vendor_get_wifi_config_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct sk_buff *skb;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	int ret;
	u64 value = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute with vendor wifi config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wifi params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		if (ath12k_vendor_cmd_skip_check(wiphy, wdev, wifi_params,
						 QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION))
			return -EPERM;

		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_get_wifi_params_extn(wiphy, wdev,
							    &wifi_params, &value);
#endif /* CPTCFG_QCN_EXTN */
			if (ret) {
				ath12k_err(NULL,
					   "Failed to set wifi params \n");
				return -EINVAL;
			}
			break;
		case QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_get_wifi_config_handler_extn(wiphy,
									 tb, &value);
#endif /* CPTCFG_QCN_EXTN */
			if (ret) {
				ath12k_err(NULL, "Failed to get HE MCS 12/13 capability\n");
				return ret;
			}
			break;
		case ACFG_PARAM_RADIO_SCAN_BLANKING_MODE:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_get_scan_blanking_mode_extn(wiphy, &value);
			if (ret) {
				ath12k_err(NULL, "Failed to get scan blanking mode\n");
				return ret;
			}
#endif /* CPTCFG_QCN_EXTN */
			break;
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG, "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP:
			__le16 cap = cpu_to_le16((u16)value);

			if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
				    sizeof(cap), &cap)) {
				ret = -EINVAL;
				goto err;
			}
			goto send_reply;
		default:
			break;
		}

		switch (wifi_params.value) {
		case QCA_WLAN_VENDOR_VDEV_PARAM_VDEV_TSF:
			if ((nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
			    value, NL80211_ATTR_PAD)) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
			    sizeof(u64))) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
		default:
			if ((nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_DATA, value)) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
			    sizeof(u32))) ||
			    (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
		}
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE]) {
		vif = wdev_to_ieee80211_vif_vlan(wdev, false);
		if (!vif) {
			ret = -EINVAL;
			goto err;
		}

		vif = wdev_to_ieee80211_vif_vlan(wdev, false);
		if (!vif) {
			ret = -EINVAL;
			goto err;
		}

		ahvif = ath12k_vif_to_ahvif(vif);
		if (!ahvif) {
			ret = -EINVAL;
			goto err;
		}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_OFFLOAD_TYPE,
			       ahvif->dp_vif.ppe_vp_type)) {
			ret = -EINVAL;
			goto err;
		}
#endif
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE]) {
		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_CONFIG_VAP_SUBMODE,
			       wdev->vap_submode)) {
			ath12k_err(NULL,
				   "nla_put failed for ATTR_CONFIG_VAP_SUBMODE\n");
			ret = -EINVAL;
			goto err;
		}
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "%s-vap_submode: %d for(%s)\n", __func__,
			   wdev->vap_submode, wdev->netdev->name);
	}

send_reply:
	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		ath12k_err(NULL,
			   "send failed with err=%d\n", ret);
		return ret;
	}

	return 0;

err:
	ath12k_err(NULL,
		   "get failed with err=%d\n", ret);
	kfree_skb(skb);
	return ret;
}

static int ath12k_vendor_get_wiphy_config_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params wifi_params;
	struct sk_buff *skb;
	int ret;
#define SELF_SR_CONFIG_SIZE 64
	char value_arr[SELF_SR_CONFIG_SIZE] = {0};
	u32 value = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data, data_len,
			ath12k_wifi_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute with vendor wiphy config %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "wiphy:%p wdev: %p Extract wiphy params\n",
			   wiphy, wdev);
		memset(&wifi_params, 0, sizeof(struct ath12k_wifi_generic_params));
		ath12k_vendor_wifi_extract_generic_command_params(tb, &wifi_params);
		if (ath12k_vendor_cmd_skip_check(wiphy, wdev, wifi_params,
						 QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION))
			return -EPERM;

		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_get_wiphy_params_extn(wiphy,
							    &wifi_params, &value);
			if (ret) {
				ath12k_err(NULL,
					   "Failed to get wifi params\n");
				return -EINVAL;
			}
#endif /* CPTCFG_QCN_EXTN */
			break;
		case QCA_NL80211_VENDOR_RADIO_SR_SELF_CONFIG:
#ifdef CPTCFG_QCN_EXTN
			ret = ath12k_vendor_get_wiphy_sr_params_extn(wiphy,
								     &wifi_params,
								     value_arr,
								     sizeof(value_arr));
#endif /* CPTCFG_QCN_EXTN */
			if (ret) {
				ath12k_err(NULL, "Failed to get SR wifi params\n");
				return -EINVAL;
			}
			break;
#ifdef CPTCFG_QCN_EXTN
		case QCA_NL80211_VENDOR_RADIO_SMART_ANT_CONFIG:
			ret = ath12k_vendor_get_wiphy_sa_configs(wiphy,
								 &wifi_params,
								 &value);
			if (ret) {
				ath12k_err(NULL, "Failed to get SA configs:%d\n",
					   ret);
				return -EINVAL;
			}
			break;
#endif
		default:
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Un-supported generic command\n");
			return -EOPNOTSUPP;
		}
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]) {
		int data_attr = QCA_WLAN_VENDOR_ATTR_PARAM_DATA;
		int length_attr = QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH;
		int flags_attr = QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS;

		switch (wifi_params.command) {
		case QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS:
			if ((nla_put_u32(skb, data_attr, value)) ||
			    (nla_put_u32(skb, length_attr, sizeof(u32))) ||
			    (nla_put_u32(skb, flags_attr, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
		case QCA_NL80211_VENDOR_RADIO_SR_SELF_CONFIG:
			if ((nla_put(skb, data_attr, strlen(value_arr), value_arr)) ||
			    (nla_put_u32(skb, length_attr, strlen(value_arr))) ||
			    (nla_put_u32(skb, flags_attr, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
#ifdef CPTCFG_QCN_EXTN
		case QCA_NL80211_VENDOR_RADIO_SMART_ANT_CONFIG:
			if ((nla_put_u32(skb, data_attr, value)) ||
			    (nla_put_u32(skb, length_attr, sizeof(u32))) ||
			    (nla_put_u32(skb, flags_attr, 0))) {
				ret = -EINVAL;
				goto err;
			}
			break;
#endif
		}
	}

#ifdef CPTCFG_QCN_EXTN
	ath12k_vendor_get_wiphy_config_handler_extn(wiphy, tb, skb);
#endif

	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		ath12k_err(NULL,
			   "send failed with err=%d\n", ret);
		return ret;
	}

	return 0;

err:
	ath12k_err(NULL,
		   "put failed with err=%d\n", ret);
	kfree_skb(skb);
	return ret;
}

static
int ath12k_vendor_trigg_pri_link_migrate(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_mac_link_migrate_usr_params arg;
	u8 mac_addr[ETH_ALEN] = {0};
	struct ath12k_vif *ahvif;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX + 1] = {0};
	u8 link_id;
	int ret;

	if (WARN_ON(!vif))
		return -EINVAL;

	/* not supported in case of non-ML vif */
	if (!vif->valid_links)
		return -EOPNOTSUPP;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX, data, data_len,
			ath12k_pri_link_migrate_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attribute in primary link migrate");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID])
		return -EINVAL;

	if (tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR] &&
	    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR]) == ETH_ALEN)) {
		memcpy(mac_addr,
		       nla_data(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR]),
		       ETH_ALEN);
	}

	link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID]);

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return -EINVAL;

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "primary link migration command received link_id %u, mac_addr %pM",
			 link_id, mac_addr);

	arg.link_id = link_id;
	memcpy(arg.addr, mac_addr, ETH_ALEN);

	ret = ath12k_mac_process_link_migrate_req(ahvif, &arg);

	if (ret)
		ath12k_info(NULL,
			    "Failed to trigger primary link migration command\n");

	return ret;
}

int ath12k_vendor_put_umac_migration_notif(struct ieee80211_vif *vif, u8 *mac_addr, u8 link_id)
{
	struct wireless_dev *wdev;
	struct sk_buff *skb;

	wdev = ieee80211_vif_to_wdev(vif);
	if (!wdev)
		return -EINVAL;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX,
					  GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR, ETH_ALEN, mac_addr) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID, link_id)) {
		kfree(skb);
		return -1;
	}

	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;
}

int
ath12k_vendor_send_power_update_complete(struct ath12k *ar,
					 struct ath12k_afc_info *afc)
{
	struct ath12k_afc_sp_reg_info *afc_reg_info = afc->afc_reg_info;
	struct sk_buff *vendor_event;
	int vendor_buffer_len;

	vendor_buffer_len =
		ath12k_afc_power_event_update_or_get_len(ar, NULL,
							 afc_reg_info);
	if (vendor_buffer_len < 0) {
		ath12k_warn(ar->ab,
			    "failed to get AFC power update event length: %d\n",
			    vendor_buffer_len);
		return vendor_buffer_len;
	}

	vendor_event =
	cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, NULL, vendor_buffer_len,
				    QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
				    GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_warn(ar->ab,
			    "failed to allocate skb for afc expiry event\n");
		return -ENOMEM;
	}

	if (ath12k_afc_power_event_update_or_get_len(ar, vendor_event,
						     afc_reg_info)) {
		ath12k_warn(ar->ab, "Failed to update AFC power vendor event\n");
		goto fail;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "Sending AFC update complete event to user application");
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	return 0;

fail:
	kfree_skb(vendor_event);
	return -EINVAL;
}

#ifndef CPTCFG_QCN_EXTN
static
#endif
struct ath12k *ath12k_get_ar_from_wdev(struct wireless_dev *wdev, u8 link_id)
{
	struct ieee80211_vif *vif =  NULL;
	struct ath12k_vif *ahvif = NULL;
	struct ieee80211_hw *hw = NULL;
	struct ath12k *ar = NULL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return NULL;

	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return NULL;

	hw = ahvif->ah->hw;
	if (!hw) {
		return NULL;
	}

	ar = ath12k_get_ar_by_vif(hw, vif, link_id);

	return ar;
}

static const struct nla_policy
ath12k_afc_clear_payload_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
};

static int ath12k_vendor_clear_afc_payload(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	u8 link_id = 0;
	int err;

	if (!wdev)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_clear_payload_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	err = ath12k_wmi_send_afc_cmd_tlv(ar, QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
					  WMI_AFC_CMD_CLEAR_AFC_PAYLOAD);

	return err;
}

static const struct nla_policy
ath12k_afc_reset_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID] = {.type = NLA_U8 },
};

static const struct nla_policy
ath12k_afc_fetch_power_info_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX] = {.type = NLA_U8 },
};

#ifdef CPTCFG_QCN_EXTN
static const struct nla_policy
ath12k_hw_blocked_chans_query_policy
	[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_RADIO_INDEX] = {.type = NLA_S32 },
};
#endif

/**
 * ath12k_validate_afc_fetch_input - Validate input parameters for AFC power
 * info fetch
 * @data: Pointer to input data
 * @data_len: Length of input data
 *
 * This function validates the input parameters for fetching AFC power
 * information. It checks if the wireless device and data pointers are
 * valid and if the data length is non-zero.
 * Returns 0 if the input is valid, otherwise returns a negative error code.
 */
static int ath12k_validate_afc_fetch_input(const void *data, int data_len)
{
	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length or NULL data pointer");
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_parse_afc_fetch_attrs - Parse attributes for AFC power info fetch
 * @data: Pointer to input data
 * @data_len: Length of input data
 * @tb: Array to store parsed attributes
 *
 * This function parses the attributes from the input data for fetching AFC
 * power information. It uses nla_parse to extract the attributes
 * based on the defined policy.
 * Returns 0 on success or a negative error code on failure.
 */
static int ath12k_parse_afc_fetch_attrs(const void *data, int data_len,
					struct nlattr *tb[])
{
	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_fetch_power_info_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	return 0;
}

/**
 * ath12k_get_radio_by_id - Get ath12k instance by radio index
 * @wiphy: Pointer to wiphy
 * @radio_id: HW radio index
 *
 * This function retrieves the ath12k instance corresponding to the
 * specified radio index.
 * Returns a pointer to the ath12k instance or NULL if not found or invalid.
 */
#ifndef CPTCFG_QCN_EXTN
static
#endif
struct ath12k *ath12k_get_radio_by_id(struct wiphy *wiphy, u8 radio_id)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio id %d", radio_id);
		return NULL;
	}

	return &ah->radio[radio_id];
}

static struct ath12k *ath12k_get_radio_by_index(struct wiphy *wiphy,
						struct nlattr *tb[])
{
	u8 radio_idx;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]) {
		struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
		struct ath12k_hw *ah;

		if (!hw)
			return NULL;

		ah = ath12k_hw_to_ah(hw);
		if (!ah || ah->num_radio != 1)
			return NULL;

		return ath12k_ah_to_ar(ah, 0);
	}

	radio_idx = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]);

	return ath12k_get_radio_by_id(wiphy, radio_idx);
}

/**
 * ath12k_prepare_and_send_afc_response - Prepare and send AFC response
 * @ar: Pointer to ath12k instance
 * @wiphy: Pointer to wiphy
 * @afc_reg_info: Pointer to AFC regulatory info
 *
 * This function prepares and sends the AFC response to the user space
 * application. It allocates a socket buffer, populates it with the AFC
 * regulatory information, and sends it using cfg80211_vendor_cmd_reply.
 * Returns 0 on success or a negative error code on failure.
 */
static int
ath12k_prepare_and_send_afc_response(struct ath12k *ar,
				     struct wiphy *wiphy,
				     struct ath12k_afc_sp_reg_info *afc_reg_info)
{
	struct sk_buff *skb;
	int skb_buf_len;

	skb_buf_len = ath12k_afc_power_event_update_or_get_len(ar, NULL, afc_reg_info);
	if (skb_buf_len < 0) {
		ath12k_warn(ar->ab,
			    "failed to get AFC response length: %d\n",
			    skb_buf_len);
		return skb_buf_len;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, skb_buf_len);
	if (!skb) {
		ath12k_err(ar->ab, "skb alloc failed");
		return -ENOMEM;
	}

	if (ath12k_afc_power_event_update_or_get_len(ar, skb, afc_reg_info)) {
		ath12k_warn(ar->ab, "Failed to update AFC power fetch event");
		kfree_skb(skb);
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
	     "Sending AFC power fetch complete event to user application");

	return cfg80211_vendor_cmd_reply(skb);
}

/**
 * ath12k_vendor_fetch_afc_power_info - Fetch AFC power info from driver
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wireless device
 * @data: Pointer to input data
 * @data_len: Length of input data
 *
 * This function handles the vendor command to fetch AFC power information
 * from the driver. It validates the input, retrieves the appropriate
 * ath12k instance, and prepares the response to be sent back to the user.
 * Returns 0 on success or a negative error code on failure.
 */
static int ath12k_vendor_fetch_afc_power_info(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	struct ath12k_afc_info *afc_info;
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	int err;

	err = ath12k_validate_afc_fetch_input(data, data_len);
	if (err)
		return err;

	err = ath12k_parse_afc_fetch_attrs(data, data_len, tb);
	if (err)
		return err;

	ar = ath12k_get_radio_by_index(wiphy, tb);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL in %s", __func__);
		return -ENODATA;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
		   "AFC fetch power info command received radio_id: %u",
		   nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX]));

	if (!ar->supports_6ghz || !ar->afc.is_6ghz_afc_power_event_received) {
		ath12k_dbg(ar->ab, ATH12K_DBG_AFC,
			   "6 GHz radio: %u, AFC power event received: %u",
			   ar->supports_6ghz, ar->afc.is_6ghz_afc_power_event_received);
		return -EOPNOTSUPP;
	}

	afc_info = &ar->afc;
	afc_reg_info = afc_info->afc_reg_info;
	if (!afc_reg_info) {
		ath12k_err(NULL, "AFC reg info not found");
		return -EINVAL;
	}

	return ath12k_prepare_and_send_afc_response(ar, wiphy, afc_reg_info);
}

static int ath12k_vendor_reset_afc(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	struct ath12k *ar;
	u8 link_id = 0;
	int err;

	if (!wdev)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX, data,
		      data_len, ath12k_afc_reset_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_CONFIG_MAX parsing failed");
		return -EINVAL;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID]);
	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	err = ath12k_wmi_send_afc_cmd_tlv(ar, QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
					  WMI_AFC_CMD_RESET_AFC);

	return err;
}
static const struct nla_policy
ath12k_cfg80211_power_mode_set_policy[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID] = {.type = NLA_U8 },
};

static int ath12k_vendor_6ghz_power_mode_change(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct ath12k *ar;
	u8 link_id = 0;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX + 1];
	u8 ap_6ghz_pwr_mode;
	int err;

	if (!wdev)
		return -EINVAL;

	if (wdev->iftype != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "Invalid iftype %d for 6 GHz power mode change",
			   wdev->iftype);
		return -EINVAL;
	}

	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid data length data ptr: %pK ", data);
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX, data,
		      data_len, ath12k_cfg80211_power_mode_set_policy, NULL)) {
		ath12k_err(NULL,
			   "QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE parsing failed");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID]) {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID]);
		if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
			ath12k_err(NULL, "Invalid link id %d", link_id);
			return -EINVAL;
		}
	}

	if (!wdev->links[link_id].ap.beacon_interval) {
		ath12k_err(NULL, "Beacon interval not set for link id %d",
			   link_id);
		return -EOPNOTSUPP;
	}

	if (!wdev->links[link_id].ap.chandef.chan ||
		wdev->links[link_id].ap.chandef.chan->band != NL80211_BAND_6GHZ) {
		ath12k_err(NULL, "Invalid channel / band for link id %d",
			   link_id);
		return -EINVAL;
	}

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	if (!tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE])
		return -EINVAL;

	ap_6ghz_pwr_mode =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE]);

	if (ap_6ghz_pwr_mode < QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI ||
	    ap_6ghz_pwr_mode > QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP) {
		ath12k_err(NULL, "Invalid 6 GHZ pwr mode configuration");
		return -EINVAL;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "6 GHz power mode change request for link id %d, pwr mode %d\n",
		   link_id, ap_6ghz_pwr_mode);
	err = ieee80211_6ghz_power_mode_change(wiphy, wdev, ap_6ghz_pwr_mode,
					       link_id, true);

	return err;
}

int ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						       struct wireless_dev *wdev,
						       u8 link_id)
{
	struct sk_buff *vendor_event;
	int ret = 0;
	int vendor_buffer_len = nla_total_size(sizeof(u8));
	u8 ap_power_mode = wdev->links[link_id].reg_6g_power_mode;

	/* NOTE: lockdep_assert_held is called in ath12k_mac_bss_info_changed */
	vendor_event =
	cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, wdev, vendor_buffer_len,
				    QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX,
				    GFP_KERNEL);
	if (!vendor_event) {
		ath12k_warn(ar->ab, "SKB alloc failed for 6 GHz power mode evt\n");
		goto out;
	}

	ret = nla_put_u8(vendor_event,
			 QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE,
			 ap_power_mode);

	if (ret) {
		ath12k_warn(ar->ab, "6 GHZ power mode vendor evt failed\n");
		goto out;
	}

	if (wdev->valid_links) {
		ret = nla_put_u8(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID, link_id);
		if (ret) {
			ath12k_warn(ar->ab, "Failed to put 6 GHz link id\n");
			goto out;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Send power mode update complete for Link id %d\n", link_id);
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
out:
	return ret;
}

static const struct nla_policy
ath12k_vendor_sdwf_phy_policy[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS] = {.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_sdwf_svc_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_samples_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_thershold_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD] = {.type = NLA_U8},
        [QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_telemetry_sdwf_sla_detect_config_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD] = {.type = NLA_U8},
};

static const struct nla_policy
ath12k_vendor_sdwf_dev_policy[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS] = {.type = NLA_NESTED},
};

static const struct nla_policy
ath12k_vendor_sdwf_streaming[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID] = {.type = NLA_U32},
};

static const struct nla_policy
ath12k_telemetric_sla_policy[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC] = {.type = NLA_BINARY,
								 .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC] = {.type = NLA_BINARY,
								     .len = ETH_ALEN},
	[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC] = {.type = NLA_U8},
};

static int ath12k_vendor_set_sdwf_config(struct ath12k_base *ab,
					 struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 struct nlattr *svc_params)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	struct ath12k_qos_params param_dl = {0};
	struct ath12k_qos_params param_ul = {0};
	bool ul_params = false;
	bool dl_params = false;
	int ret = 0;
	u16 qos_id_dl = QOS_ID_INVALID;
	u16 qos_id_ul = QOS_ID_INVALID;
	u16 svc_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
				svc_params,
				ath12k_vendor_sdwf_svc_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attribute with SDWF configure command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
	} else {
		ath12k_err(ab, "Mandatory attributes not available\n");
		return -EINVAL;
	}

	/* Check if DL prams exists */
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL] \
	    || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS]){
		dl_params = true;
	}

	/* Check if UL prams exists */
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE] \
	   || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY] \
	   || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE] || tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE]){
		ul_params = true;
	}

	if (ath12k_check_erp_power_down(ab->ag)) {
		ret = ath12k_core_power_up(ab->ag);
		if (ret) {
			ath12k_err(ab, "power up is failed\n");
			return ret;
		}
	}

	/* Get the required params */

	if (dl_params) {
		ath12k_qos_set_default(&param_dl);

		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP])
			param_dl.min_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP])
			param_dl.mean_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE])
			param_dl.burst_size = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL])
			param_dl.min_service_interval = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND])
			param_dl.delay_bound = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL])
			param_dl.msdu_life_time = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO])
			param_dl.priority = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID])
			 param_dl.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS])
			param_dl.msdu_delivery_info = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS]);

		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_dl = ath12k_qos_configure(ab, NULL,
							 &param_dl,
							 QOS_PROFILE_DL,
							 NULL);
			if (qos_id_dl == QOS_ID_INVALID) {
				ath12k_err(ab, "Unable to configure DL QoS profile(SVC__ID:%d)", svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_dl = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
			if (qos_id_dl != QOS_ID_INVALID) {
				ath12k_qos_update(ab, NULL, &param_dl,
						  QOS_PROFILE_DL,
						  qos_id_dl, NULL);
			} else {
				qos_id_dl = ath12k_qos_configure(ab, NULL,
								 &param_dl,
								 QOS_PROFILE_DL,
								 NULL);
			}
		}
		ath12k_telemetry_set_svclass_cfg(true, svc_id,
						 param_dl.min_data_rate,
						 param_dl.mean_data_rate,
						 param_dl.burst_size,
						 param_dl.min_service_interval,
						 param_dl.delay_bound,
						 param_dl.msdu_life_time,
						 param_dl.msdu_delivery_info);
	}

	if (ul_params) {
		ath12k_qos_set_default(&param_ul);

		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL])
			param_ul.min_service_interval = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE])
			param_ul.burst_size = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT])
			param_ul.min_data_rate = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY])
			param_ul.delay_bound = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE])
			param_ul.ul_ofdma_disable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE])
			param_ul.ul_mu_mimo_disable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE]);
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID])
			 param_ul.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);

		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_ul = ath12k_qos_configure(ab, NULL,
							 &param_ul,
							 QOS_PROFILE_UL,
							 NULL);
			if (qos_id_ul == QOS_ID_INVALID) {
				ath12k_err(ab, "Config Failed Svc ID:%d",
					   svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_ul = ath12k_sdwf_get_ul_qos_id(ab,
							      svc_id);
			if (qos_id_ul != QOS_ID_INVALID)
				ath12k_qos_update(ab, NULL, &param_ul,
						  QOS_PROFILE_UL,
						  qos_id_ul, NULL);
			else
				qos_id_ul = ath12k_qos_configure(ab, NULL,
								 &param_ul,
								 QOS_PROFILE_UL,
								 NULL);
		}
	}

	/* Create a DL only service class if only TID is specified */
	if (!ul_params && !dl_params && tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]) {
		ath12k_qos_set_default(&param_dl);
		param_dl.tid = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID]);
		if (!ath12k_sdwf_service_configured(ab, svc_id)) {
			qos_id_dl = ath12k_qos_configure(ab, NULL,
							 &param_dl,
							 QOS_PROFILE_DL,
							 NULL);
			if (qos_id_dl == QOS_ID_INVALID) {
				ath12k_err(ab, "Unable to configure DL QoS profile(SVC__ID:%d)",
					   svc_id);
				return -EINVAL;
			}
		} else {
			qos_id_dl = ath12k_sdwf_get_dl_qos_id(ab,
							      svc_id);
			if (qos_id_dl != QOS_ID_INVALID)
				ath12k_qos_update(ab, NULL, &param_dl,
						  QOS_PROFILE_DL, qos_id_dl,
						  NULL);
			else
				qos_id_dl = ath12k_qos_configure(ab, NULL,
								 &param_dl,
								 QOS_PROFILE_DL,
								 NULL);
		}
	}

	ath12k_sdwf_map_service_class(ab, svc_id, qos_id_dl, qos_id_ul);

	return 0;
}

static int ath12k_vendor_disable_sdwf_config(struct ath12k_base *ab,
					     struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     struct nlattr *svc_params)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	u8 svc_id = 0;
	int ret = 0;
	u16 dl_qos_id;
	u16 ul_qos_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
			       svc_params,
			       ath12k_vendor_sdwf_svc_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attributes with SDWF disable command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
	} else {
		ath12k_err(ab, "Mandatory attribute not available\n");
		return -EINVAL;
	}

	if (!ath12k_sdwf_service_configured(ab, svc_id)) {
		ath12k_err(ab, "Service Class %d is not configured\n",
			   svc_id);
		return -EINVAL;
	}

	dl_qos_id = ath12k_sdwf_get_dl_qos_id(ab, svc_id);
	if (dl_qos_id != QOS_ID_INVALID) {
		ret = ath12k_qos_disable(ab, NULL,
					 QOS_PROFILE_DL,
					 dl_qos_id,
					 NULL);
		ath12k_telemetry_set_svclass_cfg(false, svc_id, 0, 0,
						 0, 0, 0, 0, 0);
	}

	ul_qos_id = ath12k_sdwf_get_ul_qos_id(ab, svc_id);
	if (ul_qos_id != QOS_ID_INVALID)
		ret = ath12k_qos_disable(ab, NULL,
					 QOS_PROFILE_UL,
					 ul_qos_id,
					 NULL);

	ret = ath12k_sdwf_unmap_service_class(ab, svc_id);

	return ret;
}

static int ath12k_vendor_view_sdwf_config(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct sk_buff *msg,
					  const void *data,
					  int data_len,
					  unsigned long *storage)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah;
	struct ath12k_base *ab;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];
	struct nlattr *svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX + 1];
	struct ath12k_qos_ctx *qos_ctx;
	struct ath12k_qos *profile;
	struct nlattr *svc_classes, *svc_class;
	int ret = 0, i, j = 0;
	int tailroom = 0, nest_start_length = 0;
	int nest_end_length = 0, nested_range = 0;
	u8 svc_id = 0;

	ah = ath12k_hw_to_ah(hw);
	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return -EINVAL;
	}

	ab = ar->ab;

	if (!ab) {
		ath12k_err(NULL, "ab is NULL");
		return -EINVAL;
	}

	if (!storage)
		return -ENODATA;

	qos_ctx = ath12k_get_qos(ab);
	if (!qos_ctx) {
		ath12k_err(ab, "QoS context not valid");
		return -ENODATA;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX, data, data_len,
			ath12k_vendor_sdwf_phy_policy, NULL);

	if (ret) {
		ath12k_err(ab, "Invalid attr with SDWF cmd");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION] &&
	    nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]) == QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_GET) {
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = nla_parse_nested(svc, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX,
					       tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS],
					       ath12k_vendor_sdwf_svc_policy, NULL);
			if (ret) {
				ath12k_err(ab, "Invalid attr SDWF view cmd");
				return -EINVAL;
			}
			if (svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]) {

				svc_id = nla_get_u8(svc[QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID]);
				if (!ath12k_sdwf_service_configured(ab, svc_id)) {
					ath12k_err(ab, "Invalid Svc ID: %d",
						   svc_id);
					return -EINVAL;
				}
			}
		}
	} else {
		ath12k_err(ab, "Invalid attribute with SDWF view command\n");
		return -EINVAL;
	}
	/* return 0 to end the dump */
	if (*storage == QOS_PROFILES_MAX)
		return 0;

	svc_classes = nla_nest_start(msg,
				     QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS);

	if (!svc_classes)
		return -ENOBUFS;

	tailroom = skb_tailroom(msg);
	for (i = (svc_id) ? (svc_id) : (*storage);
	     i < QOS_PROFILES_MAX && tailroom > nested_range;
	     i += (svc_id) ? (QOS_PROFILES_MAX) : (1)) {
		u16 id_dl = qos_ctx->svc_class[i].dl_qos_id;
		u16 id_ul = qos_ctx->svc_class[i].ul_qos_id;

		if (!ath12k_sdwf_service_configured(ab, i))
			continue;

		if (id_dl != QOS_ID_INVALID)
			profile = &qos_ctx->profiles[id_dl];
		nest_start_length = msg->len;
		svc_class = nla_nest_start(msg, j);
		if (!svc_class)
			goto nla_put_failure;

		if ((id_dl != QOS_ID_INVALID) &&
		    (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID,
			       i) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP,
				profile->params.min_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP,
				profile->params.mean_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE,
				profile->params.burst_size) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL,
				profile->params.min_service_interval) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND,
				profile->params.delay_bound) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL,
				profile->params.msdu_life_time) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO,
				 profile->params.priority) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID,
				profile->params.tid) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS,
				profile->params.msdu_delivery_info)
		    ))
			goto nla_put_failure;

		if (id_ul != QOS_ID_INVALID)
			profile = &qos_ctx->profiles[id_ul];
		if ((id_ul !=  QOS_ID_INVALID) &&
		    (nla_put_u8(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID, i) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL,
				profile->params.min_service_interval) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT,
				profile->params.min_data_rate) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY,
				profile->params.delay_bound) ||
		    nla_put_u32(msg,
				QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE,
				profile->params.burst_size) ||
		    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID,
				profile->params.tid) ||
		    nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE,
			       profile->params.ul_ofdma_disable) ||
		    nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE,
			       profile->params.ul_mu_mimo_disable)
		))
			goto nla_put_failure;

		nest_end_length = nla_nest_end(msg, svc_class);
		nested_range = nest_end_length - nest_start_length;
		tailroom -= nested_range;
		j++;
	}
	nla_nest_end(msg, svc_classes);

	*storage = (svc_id) ? (QOS_PROFILES_MAX) : (i);

	if (!j)
		return 0;

	return msg->len;

nla_put_failure:
	return -ENOBUFS;
}

static int ath12k_vendor_telemetry_sdwf_sla_samples_config(struct nlattr *sla_samples)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX + 1];
	struct ath12k_sla_samples_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX,
			       sla_samples,
			       ath12k_vendor_telemetry_sdwf_sla_samples_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid set telemetry sla samples config policy attribute\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT])
		t_param.moving_avg_pkt = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN])
		t_param.moving_avg_win = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT])
		t_param.sla_num_pkt = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC])
		t_param.sla_time_sec = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC]);

	ret = ath12k_telemetry_sdwf_sla_samples_config(t_param);
	return ret;
}

static int ath12k_vendor_telemetry_sdwf_sla_thershold_config(struct nlattr *sla_threshold)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX + 1];
	struct ath12k_sla_thershold_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX,
			       sla_threshold,
			       ath12k_vendor_telemetry_sdwf_sla_thershold_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid telemetry sla thershold config policy attribute\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID])
		t_param.svc_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP])
		t_param.min_throughput_rate =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP])
		t_param.max_throughput_rate =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE])
		t_param.burst_size = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL])
		t_param.service_interval =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND])
		t_param.delay_bound = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL])
		t_param.msdu_ttl =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS])
		t_param.msdu_rate_loss = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE])
		t_param.per = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD])
		t_param.mcs_min_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD])
		t_param.mcs_max_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD])
		t_param.retries_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD]);

	ret = ath12k_telemetry_sdwf_sla_thershold_config(t_param);

	return ret;
}

static int ath12k_vendor_telemetry_sdwf_sla_detection_config(struct nlattr *sla_detect)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX + 1];
	struct ath12k_sla_detect_cfg t_param = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX,
			       sla_detect,
			       ath12k_vendor_telemetry_sdwf_sla_detect_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid telemetry sdwf sla detection config policy attribute\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM])
		t_param.sla_detect =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP])
		t_param.min_throughput_rate =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP])
		t_param.max_throughput_rate =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE])
		t_param.burst_size =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL])
		t_param.service_interval =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND])
		t_param.delay_bound =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL])
		t_param.msdu_ttl =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS])
		t_param.msdu_rate_loss = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE])
		t_param.per = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD])
		t_param.mcs_min_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD])
		t_param.mcs_max_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD])
		t_param.retries_thres = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD]);

	ret = ath12k_telemetry_sdwf_sla_detection_config(t_param);

	return ret;
}

static int ath12k_vendor_sdwf_phy_operations(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah;
	struct ath12k_base *ab;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];
	u8 sdwf_oper;
	int ret = 0;

	ah = ath12k_hw_to_ah(hw);
	ar = ath12k_ah_to_ar(ah, 0);
	if (!ar) {
		ath12k_err(NULL, "ar is NULL");
		return -EINVAL;
	}

	ab = ar->ab;
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX, data, data_len,
			ath12k_vendor_sdwf_phy_policy, NULL);
	if (ret) {
		ath12k_err(ab, "Invalid attr with SDWF radio commands");
		goto end;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]) {
		sdwf_oper = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION]);
	} else {
		ath12k_err(ab, "SDWF radio level operation missing");
		ret = -EINVAL;
		goto end;
	}

	switch (sdwf_oper) {
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = ath12k_vendor_set_sdwf_config(ab, wiphy, wdev,
							    tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]);
		} else {
			ath12k_err(ab, "SDWF svc parameters missing");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_DEL:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]) {
			ret = ath12k_vendor_disable_sdwf_config(ab, wiphy, wdev, tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS]);
		} else {
			ath12k_err(ab, "SDWF service id missing with delete");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_SAMPLES_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_samples_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla samples parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_BREACH_DETECTION_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_detection_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla breach detect parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_THRESHOLD_SET:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS]) {
			ret = ath12k_vendor_telemetry_sdwf_sla_thershold_config(tb[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS]);
		} else {
			ath12k_err(NULL, "SDWF sla threshnew parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	default:
		ath12k_err(ab, "Invalid operation with SDWF radio commands");
		ret = -EINVAL;
	}
end:
	return ret;
}

static int ath12k_vendor_atf_offload_ssid_group_config(struct ath12k *ar,
						       struct nlattr **tb)
{
	struct nlattr *group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX + 1];
	struct ath12k_atf_group_info *group_info;
	struct nlattr *group_attr;
	int ret, rem, i = 0;

	nla_for_each_nested(group_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG],
			    rem) {
		ret = nla_parse_nested(group, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX,
				       group_attr, ath12k_vendor_atf_grouping_param_policy, NULL);
		if (ret) {
			ath12k_warn(ar->ab, "ATF: Group payload has invalid attributes");
			return -EINVAL;
		}

		if (!group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS] ||
		    !group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]) {
			ath12k_warn(ar->ab, "ATF: All group parameters not present");
			return -EINVAL;
		}

		if (i >= ATH12K_ATF_MAX_GROUPS) {
			ath12k_warn(ar->ab, "ATF: Too many groups, maximum allowed is %d",
				    ATH12K_ATF_MAX_GROUPS);
			return -EINVAL;
		}
		group_info = &ar->atf_table.group_info[i];
		i++;

		group_info->group_id =
			(u32)nla_get_u8(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX]);
		group_info->group_airtime =
			(u32)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED]);
		group_info->group_policy =
			(u32)nla_get_u8(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY]);
		group_info->unconfigured_peers =
			(u16)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS]);
		group_info->configured_peers =
			(u16)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS]);
		group_info->unconfigured_peers_airtime =
			(u32)nla_get_u16(group[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME]);
	}
	ar->atf_table.total_groups = i;

	ret = ath12k_wmi_atf_send_group_config(ar);
	if (ret)
		ath12k_warn(ar->ab, "ATF: Failed to send group config");

	return ret;
}

static int ath12k_vendor_atf_offload_wmm_ac_config(struct ath12k *ar,
						   struct nlattr **tb)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct sk_buff *skb;
	void *buf, *ptr;
	u32 buf_len, len, pdev_id;
	int ret;
	struct wmi_atf_ssid_grp_request_fixed_param *cmd;

	len = sizeof(*cmd);

	pdev_id = ar->pdev->pdev_id;

	buf = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]);
	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]);
	if (!buf_len) {
		ath12k_warn(ar->ab, "No data present in ATF WMM AC config command\n");
		return -EINVAL;
	}
	len += buf_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;
	cmd = (struct wmi_atf_ssid_grp_request_fixed_param *)ptr;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ATF_GRP_WMM_AC_CFG_REQUEST_FIXED_PARAM,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	ptr += sizeof(*cmd);
	memcpy(ptr, buf, buf_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI ATF WMM ac config for pdev id %u\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_ATF_GROUP_WMM_AC_CONFIG_REQUEST_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_update_atf_peer_info(struct ath12k *ar,
					struct ath12k_atf_peer_params *peer_param)
{
	struct ath12k_link_sta *arsta;
	struct ath12k_atf_peer_info *param_peer_info;
	union ath12k_config_param val = {0};
	int i;
	void *dp_peer;

	param_peer_info = peer_param->peer_info;

	rcu_read_lock();
	spin_lock_bh(&ar->arsta_lock);

	for (i = 0; i < peer_param->num_peers; i++) {
		arsta = ath12k_link_sta_find_by_addr(ar, param_peer_info->peer_macaddr);
		if (!arsta || !arsta->ahsta) {
			param_peer_info++;
			continue;
		}

		dp_peer = ath12k_sta_get_dp_peer_rcu(arsta->ahsta);
		if (!dp_peer) {
			param_peer_info++;
			continue;
		}

		val.atf_params.atf_peer_conf_airtime = param_peer_info->percentage_peer;
		val.atf_params.atf_group_index = param_peer_info->group_index;

		ath12k_dp_link_peer_set_param_by_dp_peer_and_link_id(dp_peer,
								     arsta->link_id,
								     ATH12K_DP_LINK_PEER_ATF_PARAM,
								     &val);
		param_peer_info++;
	}

	spin_unlock_bh(&ar->arsta_lock);
	rcu_read_unlock();
}

static int ath12k_vendor_atf_offload_peer_config(struct ath12k *ar,
						 struct nlattr *peer_config)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX + 1];
	struct nlattr *peer_attr;
	struct nlattr *peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX + 1];
	struct ath12k_atf_peer_params atf_peer_param = {0};
	struct ath12k_atf_peer_info *peer_info;
	int ret, rem;
	u32 num_peers = 0;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX,
			       peer_config,
			       ath12k_vendor_atf_offload_peer_config_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "ATF: No data present in ATF peer condig command\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD]) {
		ath12k_err(ar->ab, "ATF: Peer payload is missing");
		return -EINVAL;
	}

	nla_for_each_nested(peer_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD],
			    rem) {
		num_peers++;
	}

	atf_peer_param.num_peers = num_peers;

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE])
		atf_peer_param.atf_flags = FIELD_PREP(WMI_ATF_FULL_UPDATE, 1);

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE])
		atf_peer_param.atf_flags = atf_peer_param.atf_flags |
					   FIELD_PREP(WMI_ATF_PEER_PENDING, 1);

	atf_peer_param.atf_flags = atf_peer_param.atf_flags |
				   FIELD_PREP(WMI_ATF_PEER_VALID_PDEV, 1);

	atf_peer_param.pdev_id = ar->pdev->pdev_id;

	peer_info = kzalloc(atf_peer_param.num_peers * sizeof(struct ath12k_atf_peer_info),
			    GFP_KERNEL);
	if (!peer_info) {
		ath12k_err(ar->ab, "ATF: Failed to allocate memory for peer_info");
		return -ENOMEM;
	}

	num_peers = 0;
	nla_for_each_nested(peer_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD],
			    rem) {
		ret = nla_parse_nested(peer, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAX,
				       peer_attr, ath12k_vendor_atf_peer_param_policy, NULL);
		if (ret) {
			ath12k_warn(ar->ab, "ATF: Peer payload is invalid");
			kfree(peer_info);
			return -EINVAL;
		}

		if (!peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC] ||
		    !(nla_len(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC]) == ETH_ALEN) ||
		    !peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME] ||
		    !peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX]) {
			ath12k_warn(ar->ab, "ATF: All peer parameters not present");
			kfree(peer_info);
			return -EINVAL;
		}


		memcpy(peer_info[num_peers].peer_macaddr,
				nla_data(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC]),
					 ETH_ALEN);
		peer_info[num_peers].percentage_peer =
			nla_get_u16(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME]);
		peer_info[num_peers].group_index =
			(u16)nla_get_u8(peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX]);
		if (peer[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED])
			peer_info[num_peers].explicit_peer_flag = 1;
		else
			peer_info[num_peers].explicit_peer_flag = 0;
		num_peers++;
	}

	atf_peer_param.peer_info = peer_info;

	ath12k_update_atf_peer_info(ar, &atf_peer_param);
	ret = ath12k_wmi_atf_send_peer_config(ar, &atf_peer_param);
	if (ret)
		ath12k_warn(ar->ab, "Failed to send peer config");

	kfree(peer_info);
	return ret;
}

static
void ath12k_atf_offload_reset_stats_cb(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_dp_link_peer *peer,
				       void *data)
{
	struct ath12k_atf_peer_airtime *atf_peer_airtime;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return;

	peer->atf_actual_airtime = 0;
	atf_peer_airtime = &peer->atf_peer_airtime;

	memset(atf_peer_airtime, 0, sizeof(*atf_peer_airtime));
}

static void ath12k_atf_offload_reset_stats(struct ath12k *ar)
{
	struct ath12k_pdev_dp *dp = &ar->dp;
	struct ath12k_pdev_dp_stats *pdev_stats = &dp->stats;
	struct ath12k_atf_pdev_airtime *atf_pdev_airtime =
		&pdev_stats->atf_airtime;
	struct ath12k_atf *atf_table = &ar->atf_table;
	int i;

	memset(atf_pdev_airtime, 0, sizeof(*atf_pdev_airtime));

	for (i = 0; i < atf_table->total_groups; i++) {
		atf_table->group_info[i].atf_actual_airtime = 0;
		atf_table->group_info[i].atf_actual_duration = 0;
		atf_table->group_info[i].atf_actual_ul_duration = 0;
	}

	ath12k_dp_link_peer_iterate_by_dp_pdev(&ar->dp,
					       ath12k_atf_offload_reset_stats_cb,
					       NULL);

	ar->atf_stats_accum_start_time = ath12k_get_timestamp_in_us();
}

struct ath12k_atf_update_airtime_params {
	u32 pdev_ul_airtime;
	u32 pdev_actual_airtime;
};

static
void ath12k_atf_offload_update_peer_airtime_cb(struct ath12k_pdev_dp *dp_pdev,
					       struct ath12k_dp_link_peer *peer,
					       void *data)
{
	int ac, i;
	u8 group_index = 0xFF;
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_atf_update_airtime_params *params =
					(struct ath12k_atf_update_airtime_params *)data;
	u32 pdev_ul_airtime = params->pdev_ul_airtime;
	u32 pdev_actual_airtime = params->pdev_actual_airtime;
	struct ath12k_atf_peer_airtime *atf_peer_airtime;
	u32 peer_airtime, peer_ul_airtime;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return;

	peer_airtime = 0;
	peer_ul_airtime = 0;

	for (i = 0; i < ar->atf_table.total_groups; i++) {
		if (peer->atf_group_index ==
				ar->atf_table.group_info[i].group_id) {
			group_index = i;
			break;
		}
	}

	atf_peer_airtime = &peer->atf_peer_airtime;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		peer_airtime += atf_peer_airtime->tx_airtime_consumption[ac].consumption;
		peer_ul_airtime += atf_peer_airtime->rx_airtime_consumption[ac].consumption;
	}

	if (peer_airtime > 0 && pdev_actual_airtime > 0) {
		peer->atf_actual_duration = peer_airtime;
		peer->atf_actual_airtime =
			(u32)div_u64((u64)peer_airtime * 100ULL,  pdev_actual_airtime);
	} else {
		peer->atf_actual_airtime = 0;
		peer->atf_actual_duration = 0;
	}

	if (peer_ul_airtime > 0 && pdev_ul_airtime > 0) {
		peer->atf_actual_ul_duration = peer_ul_airtime;
		peer->atf_ul_airtime =
			(u32)div_u64((u64)peer_ul_airtime * 100ULL, pdev_ul_airtime);
	} else {
		peer->atf_ul_airtime = 0;
		peer->atf_actual_ul_duration = 0;
	}

	if (group_index < ar->atf_table.total_groups) {
		ar->atf_table.group_info[group_index].atf_actual_duration += peer_airtime;
		ar->atf_table.group_info[group_index].atf_actual_ul_duration +=
									peer_ul_airtime;
		ar->atf_table.group_info[group_index].atf_actual_airtime +=
								peer->atf_actual_airtime;
	} else {
		ath12k_warn(ar->ab, "ATF: Invalid group index %u for peer %pM (max: %u)",
			    group_index, peer->addr, ar->atf_table.total_groups - 1);
	}
}

static void ath12k_atf_offload_update_peer_airtime(struct ath12k *ar)
{
	struct ath12k_pdev_dp *ar_dp = &ar->dp;
	struct ath12k_pdev_dp_stats *pdev_stats = &ar_dp->stats;
	struct ath12k_atf_pdev_airtime *atf_pdev_airtime =
		&pdev_stats->atf_airtime;
	u32 pdev_actual_airtime = 0, pdev_ul_airtime = 0;
	int ac;
	struct ath12k_atf_update_airtime_params params;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		pdev_actual_airtime += atf_pdev_airtime->tx_airtime_consumption[ac];
		pdev_ul_airtime += atf_pdev_airtime->rx_airtime_consumption[ac];
	}

	params.pdev_ul_airtime = pdev_ul_airtime;
	params.pdev_actual_airtime = pdev_actual_airtime;

	ath12k_dp_link_peer_iterate_by_dp_pdev(&ar->dp,
					       ath12k_atf_offload_update_peer_airtime_cb,
					       &params);

	ath12k_info(ar->ab, "Total Airtime(us)     %u", pdev_actual_airtime);
	ath12k_info(ar->ab, "Total UL Airtime(us)  %u", pdev_ul_airtime);
}

struct ath12k_atf_offload_stats {
	u16 peer_count;
	struct atf_peer_stat *peer_stats;
};

static void ath12k_atf_offload_print_stats_cb(struct ath12k_pdev_dp *dp_pdev,
					      struct ath12k_dp_link_peer *peer,
					      void *data)
{
	struct ath12k_atf_offload_stats *offload_stats =
					(struct ath12k_atf_offload_stats *)data;
	struct atf_peer_stat *peer_stats = offload_stats->peer_stats;
	u16 peer_count = offload_stats->peer_count;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return;

	if (peer_count >= ATH12K_ATF_MAX_PEERS)
		return;

	memcpy(peer_stats[peer_count].addr, peer->addr, ETH_ALEN);
	peer_stats[peer_count].atf_actual_airtime = peer->atf_actual_airtime;
	peer_stats[peer_count].atf_peer_conf_airtime = peer->atf_peer_conf_airtime;
	peer_stats[peer_count].atf_group_index = peer->atf_group_index;
	peer_stats[peer_count].atf_actual_duration = peer->atf_actual_duration;
	peer_stats[peer_count].atf_ul_airtime = peer->atf_ul_airtime;
	peer_stats[peer_count].atf_actual_ul_duration = peer->atf_actual_ul_duration;

	offload_stats->peer_count++;
}

static void ath12k_atf_offload_print_stats(struct timer_list *t)
{
	struct ath12k *ar = from_timer(ar, t, atf_stats_timer);
	u8 borrowed, unused;
	int i;
	struct ath12k_atf *atf_table = &ar->atf_table;
	struct atf_peer_stat *peer_stats;
	u64 current_time = ath12k_get_timestamp_in_us();
	u32 time_diff = (u32)(current_time - ar->atf_stats_accum_start_time);
	struct ath12k_atf_offload_stats offload_stats = {0};

	peer_stats = kcalloc(ATH12K_ATF_MAX_PEERS, sizeof(*peer_stats), GFP_ATOMIC);
	if (!peer_stats) {
		ath12k_warn(ar->ab, "ATF: Failed to allocate memory for peer stats");
		return;
	}

	ath12k_info(ar->ab, "Total radio duration(us): %u", time_diff);
	ath12k_atf_offload_update_peer_airtime(ar);
	ath12k_info(ar->ab, "************************************* ATF STATS For SSID Groups **************************************");
	ath12k_info(ar->ab, "GroupID   Configured   Actual(Relative)   Borrowed   Unused   Duration(us)   ActualUL   UL(us)   Actual");

	for (i = 0; i < atf_table->total_groups; i++) {
		borrowed = 0;
		unused = 0;

		if ((atf_table->group_info[i].group_airtime / 10) >
		    atf_table->group_info[i].atf_actual_airtime)
			unused = (atf_table->group_info[i].group_airtime / 10) -
				 atf_table->group_info[i].atf_actual_airtime;
		else
			borrowed = atf_table->group_info[i].atf_actual_airtime -
				   (atf_table->group_info[i].group_airtime / 10);

		ath12k_info(ar->ab, "  %-9d %-15d %-17d %-8d %-10d %-14d %-6d %-10d %-7d",
			    atf_table->group_info[i].group_id,
			    atf_table->group_info[i].group_airtime / 10,
			    atf_table->group_info[i].atf_actual_airtime,
			    borrowed,
			    unused,
			    ar->atf_table.group_info[i].atf_actual_duration,
			    ar->atf_table.group_info[i].atf_ul_airtime,
			    ar->atf_table.group_info[i].atf_actual_ul_duration,
			    time_diff ?
			    (u32)div_u64((u64)ar->atf_table.group_info[i].atf_actual_duration * 100ULL, time_diff) : 0);
	}

	offload_stats.peer_stats = peer_stats;
	ath12k_dp_link_peer_iterate_by_dp_pdev(&ar->dp,
					       ath12k_atf_offload_print_stats_cb,
					       &offload_stats);

	ath12k_info(ar->ab, "*****************************************************************************************************");
	ath12k_info(ar->ab, "**************************************** ATF STATS For PEERs ****************************************");
	ath12k_info(ar->ab, "PeerMAC             GroupId  Configured  Actual(Relative)    Borrowed    Unused    Duration(us)   ActualUL  UL(us)  Actual");

	for (i = 0; i < offload_stats.peer_count; i++) {
		borrowed = 0;
		unused = 0;

		if (peer_stats[i].atf_peer_conf_airtime / 10 > peer_stats[i].atf_actual_airtime)
			unused = (peer_stats[i].atf_peer_conf_airtime / 10) -
				 peer_stats[i].atf_actual_airtime;
		else
			borrowed = peer_stats[i].atf_actual_airtime -
				   (peer_stats[i].atf_peer_conf_airtime / 10);

		ath12k_info(ar->ab, "%-3pM     %-8d %-13d %-19d %-9d %-11d %-13d %-6d %-9d %-7d",
			    peer_stats[i].addr,
			    peer_stats[i].atf_group_index,
			    peer_stats[i].atf_peer_conf_airtime / 10,
			    peer_stats[i].atf_actual_airtime,
			    borrowed,
			    unused,
			    peer_stats[i].atf_actual_duration,
			    peer_stats[i].atf_ul_airtime,
			    peer_stats[i].atf_actual_ul_duration,
			    time_diff ?
			    (u32)div_u64((u64)peer_stats[i].atf_actual_duration * 100ULL, time_diff) : 0);
	}

	ath12k_atf_offload_reset_stats(ar);
	mod_timer(&ar->atf_stats_timer,
		  jiffies + (ar->atf_stats_timeout * HZ));

	kfree(peer_stats);
}

static int ath12k_vendor_offload_sched_duration_config(struct ath12k *ar,
						       struct nlattr *sched_duration_param)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX + 1];
	int ret;
	u32 ac, duration, pdev_id = ar->pdev->pdev_id, value;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_MAX,
			       sched_duration_param,
			       ath12k_vendor_atf_offload_sched_duration_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF schedule duration policy\n");
		return ret;
	}

	ac = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_AC]);
	duration = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION]);

	value = ((ac << 30) & GENMASK(31, 30)) | (duration & GENMASK(29, 0));
	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_SCHED_DURATION,
					value, pdev_id);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set ATF schedule param for pdev: %u\n", pdev_id);
		return ret;
	}

	return ret;
}

static int ath12k_vendor_offload_ssid_scheduling_config(struct ieee80211_hw *hw,
							struct ath12k *ar,
							struct wireless_dev *wdev,
							struct nlattr *ssid_sched_param)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX + 1];
	int ret;
	u8 ssid_cheduling, link_id = 0;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_MAX,
			       ssid_sched_param,
			       ath12k_vendor_atf_offload_ssid_sched_policy, NULL);
	if (ret) {
		ath12k_err(ar->ab, "Invalid ATF SSID schedule config policy\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID])
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID]);

	ssid_cheduling = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING]);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;
	ahvif = (struct ath12k_vif *)vif->drv_priv;
	if (!ahvif)
		return -EINVAL;

	arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
	if (!arvif)
		return -EINVAL;

	ret = ath12k_wmi_vdev_set_param_cmd(ar,
					    arvif->vdev_id, WMI_VDEV_PARAM_ATF_SSID_SCHED_POLICY,
					    ssid_cheduling);
	if (ret) {
		ath12k_warn(ar->ab, "Failed to set ATF SSID schedule param for vdev: %u\n", arvif->vdev_id);
		return ret;
	}

	return ret;
}

/**
 * struct ath12k_atf_dumpit_ctx - Context passed to the per-peer iterator
 *                                used by ath12k_vendor_atf_stats_dumpit.
 * @msg:          SKB being filled with netlink attributes.
 * @peers_data:   Outer nest attribute wrapping all per-peer entries.
 * @j:            Running index used as the nest key for each peer entry.
 * @tailroom:     Remaining tailroom in @msg; updated after each peer entry.
 * @nested_range: Size of the last peer entry; used to track tailroom usage.
 * @ret:          Accumulated return value; set to a negative errno on error.
 */
struct ath12k_atf_dumpit_ctx {
	struct sk_buff *msg;
	struct nlattr *peers_data;
	int j;
	int tailroom;
	int nested_range;
	int ret;
};

/**
 * ath12k_vendor_atf_stats_dumpit_cb() - Per-peer callback for ATF stats dumpit.
 * @peer: Current ath12k_dp_link_peer being visited by the iterator.
 * @data: Pointer to a struct ath12k_atf_dumpit_ctx.
 *
 * Called once per peer by ath12k_dp_link_peer_iterate_by_dp_pdev().
 * Skips non-STA peers, checks remaining SKB tailroom, then serialises
 * the peer's ATF airtime counters as nested netlink attributes into the
 * SKB carried in the context. On any nla_put failure the nest is
 * cancelled and a negative errno is stored in ctx->ret; returning that
 * non-zero value stops the iteration immediately.
 *
 * Return: 0 to continue iteration, negative errno to abort.
 */
static void
ath12k_vendor_atf_stats_dumpit_cb(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_dp_link_peer *peer,
				  void *data)
{
	struct ath12k_atf_dumpit_ctx *ctx = (struct ath12k_atf_dumpit_ctx *)data;
	struct sk_buff *msg = ctx->msg;
	struct nlattr *peer_data;
	int nest_start_length = 0;
	int nest_end_length = 0;

	if (!ath12k_dp_link_peer_get_sta(peer))
		return;

	if (ctx->tailroom <= ctx->nested_range)
		return;

	peer_data = nla_nest_start(msg, ctx->j++);
	if (!peer_data) {
		ctx->ret = -ENOBUFS;
		return;
	}

	if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC,
		    ETH_ALEN, peer->addr) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BE_AIRTIME,
			peer->atf_peer_airtime.tx_airtime_consumption[0].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BK_AIRTIME,
			peer->atf_peer_airtime.tx_airtime_consumption[1].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VI_AIRTIME,
			peer->atf_peer_airtime.tx_airtime_consumption[2].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VO_AIRTIME,
			peer->atf_peer_airtime.tx_airtime_consumption[3].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BE_AIRTIME,
			peer->atf_peer_airtime.rx_airtime_consumption[0].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BK_AIRTIME,
			peer->atf_peer_airtime.rx_airtime_consumption[1].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VI_AIRTIME,
			peer->atf_peer_airtime.rx_airtime_consumption[2].consumption) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VO_AIRTIME,
			peer->atf_peer_airtime.rx_airtime_consumption[3].consumption)) {
		nla_nest_cancel(msg, peer_data);
		ctx->ret = -ENOBUFS;
		return;
	}

	nest_end_length = nla_nest_end(msg, peer_data);
	ctx->nested_range = nest_end_length - nest_start_length;
	ctx->tailroom -= ctx->nested_range;
}

static int ath12k_vendor_atf_stats_dumpit(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct sk_buff *msg,
					  const void *data,
					  int data_len,
					  unsigned long *storage)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct ath12k_base *ab;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1];
	struct nlattr *peer_attr, *peers_data;
	struct ath12k_atf_dumpit_ctx ctx = {};
	int ret;
	u8 radio_id;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX, data, data_len,
			ath12k_atf_offload_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "AFT: Invalid attributes in ATF stats view\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]) {
		ath12k_err(NULL, "ATF: Missing radio index in ATF stats view\n");
		return -EINVAL;
	}

	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "ATF: Invalid radio id %d\n", radio_id);
		return -EINVAL;
	}

	ar = ath12k_ah_to_ar(ah, radio_id);
	if (!ar)
		return -ENODEV;

	ab = ar->ab;
	if (!ab)
		return -ENODEV;

	if (!storage)
		return -ENODATA;

	/* nl80211_vendor_cmd_dump() calls dumpit() in a while(1) loop as long
	 * as it returns a positive value. This function writes the complete
	 * snapshot in one shot, so mark the session done (*storage = 1) before
	 * returning msg->len. On the next call *storage is non-zero: return
	 * -ENOENT to signal the loop to stop cleanly.
	 */
	if (*storage)
		return -ENOENT;

	peer_attr = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS);
	if (!peer_attr)
		return -ENOBUFS;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BE_AIRTIME,
			ar->dp.stats.atf_airtime.tx_airtime_consumption[0]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BK_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[1]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VI_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[2]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VO_AIRTIME,
				    ar->dp.stats.atf_airtime.tx_airtime_consumption[3]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BE_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[0]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BK_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[1]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VO_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[2]) ||
			nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VI_AIRTIME,
				    ar->dp.stats.atf_airtime.rx_airtime_consumption[3])) {
		nla_nest_cancel(msg, peer_attr);
		return -ENOBUFS;
	}

	peers_data = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS);
	if (!peers_data) {
		nla_nest_cancel(msg, peer_attr);
		return -ENOBUFS;
	}

	ctx.msg        = msg;
	ctx.peers_data = peers_data;
	ctx.tailroom   = skb_tailroom(msg);

	ath12k_dp_link_peer_iterate_by_dp_pdev(&ar->dp,
					       ath12k_vendor_atf_stats_dumpit_cb,
					       &ctx);
	if (ctx.ret) {
		nla_nest_cancel(msg, peers_data);
		nla_nest_cancel(msg, peer_attr);
		return ctx.ret;
	}

	nla_nest_end(msg, peers_data);
	nla_nest_end(msg, peer_attr);
	*storage = 1;
	return msg->len;
}

static void
ath12k_atf_offload_set_atf_stats_timeout(struct ath12k *ar)
{
	if (timer_pending(&ar->atf_stats_timer))
		del_timer_sync(&ar->atf_stats_timer);

	mod_timer(&ar->atf_stats_timer, jiffies + (ar->atf_stats_timeout * HZ));
}

static int
ath12k_vendor_atf_offload_config_handler(struct wiphy *wiphy,
		struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX + 1];
	u8 radio_id, atf_enable, atf_strict_scheduling;
	u16 vo_dedicated_time, vi_dedicated_time;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX, data, data_len,
			ath12k_atf_offload_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attributes with ATF config commands\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]) {
		ath12k_err(NULL, "ATF: Missing radio index in ATF config\n");
		return -EINVAL;
	}
	radio_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX]);
	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "ATF: Invalid radio id %d\n", radio_id);
		return -EINVAL;
	}

	ar = ath12k_ah_to_ar(ah, radio_id);

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED]) {
		atf_enable = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED]);
		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_DYNAMIC_ENABLE,
						atf_enable, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF: %d\n",
				    atf_enable ? "enable" : "disable",
				    ret);
			return ret;
		}
		ar->commitatf = atf_enable;
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG]) {
		ret = ath12k_vendor_atf_offload_ssid_group_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set SSID config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG]) {
		ret = ath12k_vendor_atf_offload_wmm_ac_config(ar, tb);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set WMM AC config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]) {
		ret = ath12k_vendor_atf_offload_peer_config(ar,
							    tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set peer config\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED]) {
		ar->atf_stats_enable =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED]);

		if (timer_pending(&ar->atf_stats_timer)) {
			del_timer_sync(&ar->atf_stats_timer);
			return 0;
		}

		if (ar->atf_stats_enable) {
			ath12k_atf_offload_reset_stats(ar);
			ar->atf_stats_timeout = ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT;
			timer_setup(&ar->atf_stats_timer,
				    ath12k_atf_offload_print_stats, 0);
			mod_timer(&ar->atf_stats_timer, jiffies +
				  (ar->atf_stats_timeout * HZ));
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED]) {
		atf_strict_scheduling =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_STRICT_SCH,
						atf_strict_scheduling, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to %s ATF sctrict scheduling: %d\n",
				    atf_strict_scheduling ? "enable" : "disable",
				    ret);
			return ret;
		}
		ar->atf_strict_scheduling = atf_strict_scheduling;
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]) {
		vo_dedicated_time = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VO_DEDICATED_TIME,
						vo_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vo_dedicated_time  %d\n", ret);
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]) {
		vi_dedicated_time =
			nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG]);

		ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_ATF_VI_DEDICATED_TIME,
						vi_dedicated_time, ar->pdev->pdev_id);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set vi_dedicated_time  %d\n", ret);
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]) {
		ret = ath12k_vendor_offload_sched_duration_config(ar,
								  tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF schedule duration\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]) {
		ret = ath12k_vendor_offload_ssid_scheduling_config(hw, ar, wdev,
								   tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY]);
		if (ret) {
			ath12k_warn(ar->ab, "failed to set ATF ssid scheduling\n");
			return ret;
		}
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT]) {
		ar->atf_stats_timeout =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT]);
		if (!ar->atf_stats_enable)
			return -EINVAL;
		ath12k_atf_offload_set_atf_stats_timeout(ar);
		return 0;
	}

	ath12k_err(NULL, "Invalid or missing ATF offload attributes\n");
	return -EINVAL;

}

static int ath12k_vendor_parse_rm(struct wiphy *wiphy, struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX + 1];
	struct ath12k_vendor_service_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX,
			data, data_len, ath12k_vendor_rm_generic_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION])
		info.app_info.app_version =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION]);

	info.app_info.driver_version = 0x1;

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]) {
		ret = ath12k_vendor_parse_rm_erp(wiphy, wdev,
						 tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP]);
		return ret;
	} else if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID]) {
		info.id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID]);
		info.service_data = -EINVAL;
		if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA])
			info.service_data =
				nla_get_u64(
					tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA]);

		ath12k_err(NULL, "service id:%d data:%lld\n",
			  info.id, info.service_data);
	} else {
		ath12k_err(NULL, "invalid service id attributes provided for QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF])
		info.init_config_type =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF]);
	else
		ath12k_err(NULL, "invalid attributes provided for QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC\n");

	ret = ath12k_vendor_initialize_service(wiphy, wdev, &info);
	if (ret) {
		ath12k_err(NULL,
			   "RM Init failed for service id:%d data:%lld\n",
			   info.id, info.service_data);
	}

	return ret;
}

static const struct nla_policy
ath12k_reg_get_eirp_policy[QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE] = { .type = NLA_U8 },
};

/**
 * wlan_cfg80211_send_reg_eirp_update - Send EIRP update to userspace
 * @wiphy: Pointer to the wiphy structure
 * @eirp_list: Array of channel_power structures
 * @n_channels: Number of channels in the list
 *
 * This function packages the EIRP data (center frequency, channel number,
 * and transmit power) into nested netlink attributes and sends it to
 * userspace via a vendor command reply.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int wlan_cfg80211_send_reg_eirp_update(struct wiphy *wiphy,
					      struct channel_power *eirp_list,
					      u8 n_channels)
{
	struct sk_buff *skb;
	struct nlattr *nla_attr;
	int i;

	if (!eirp_list || n_channels == 0)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	/* Center Frequency */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].center_freq))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	/* Channel Number */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].chan_num))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	/* Transmit Power */
	nla_attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER);
	if (!nla_attr)
		goto fail;

	for (i = 0; i < n_channels; i++) {
		if (nla_put_u16(skb, i, eirp_list[i].tx_power))
			goto fail;
	}
	nla_nest_end(skb, nla_attr);

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

/**
 * ath12k_vendor_get_reg_eirp_handler - Handle vendor command to get 6 GHz EIRP data
 * @wiphy: Pointer to the wiphy structure
 * @wdev: Pointer to the wireless device
 * @data: Pointer to vendor command attributes
 * @data_len: Length of the vendor command data
 *
 * This function parses vendor attributes to determine the 6 GHz AP/client power mode,
 * retrieves the corresponding regulatory EIRP data, and sends it back to userspace.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int ath12k_vendor_get_reg_eirp_handler(struct wiphy *wiphy, struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	enum wmi_reg_6g_client_type client_type = WMI_REG_MAX_CLIENT_TYPE;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX + 1];
	enum wmi_reg_6g_ap_type ap_6ghz_pwr_mode;
	struct ieee80211_supported_band *band;
	struct channel_power *chan_eirp_list;
	bool is_client_needed;
	struct ath12k *ar;
	int ret_val = 0;
	u8 link_id;

	if (!wdev || !data || !data_len) {
		ath12k_err(NULL, "Invalid input to EIRP handler");
		return -EINVAL;
	}

	for_each_valid_link(wdev, link_id) {
		if (wdev->links[link_id].ap.chandef.chan &&
		    wdev->links[link_id].ap.chandef.chan->band == NL80211_BAND_6GHZ)
			break;
	}

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return -EINVAL;

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	band = &ar->mac.sbands[NL80211_BAND_6GHZ];
	if (!band || band->n_channels == 0)
		return -EINVAL;

	chan_eirp_list = kzalloc(band->n_channels * sizeof(*chan_eirp_list), GFP_KERNEL);
	if (!chan_eirp_list)
		return -ENOMEM;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE, data,
		      data_len, ath12k_reg_get_eirp_policy, NULL)) {
		ath12k_err(NULL, "Failed to parse EIRP vendor attributes");
		ret_val = -EINVAL;
		goto free_eirp;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE]) {
		ath12k_err(NULL, "Missing mandatory EIRP power type attribute");
		ret_val = -EINVAL;
		goto free_eirp;
	}

	ap_6ghz_pwr_mode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE]);
	if (ap_6ghz_pwr_mode < WMI_REG_INDOOR_AP || ap_6ghz_pwr_mode > WMI_REG_VLP_AP) {
		ath12k_err(NULL, "Invalid 6 GHz AP power mode: %u", ap_6ghz_pwr_mode);
		ret_val = -EINVAL;
		goto free_eirp;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE])
		client_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE]);
	if (client_type >= WMI_REG_MAX_CLIENT_TYPE) {
		ath12k_err(NULL, "Invalid client type: %u", client_type);
		ret_val = -EINVAL;
		goto free_eirp;
	}

	is_client_needed = (client_type < WMI_REG_MAX_CLIENT_TYPE);

	ret_val = ath12k_mac_reg_get_max_reg_eirp_from_chan_list(ar, ap_6ghz_pwr_mode,
								 client_type, is_client_needed,
								 chan_eirp_list);
	if (ret_val) {
		ath12k_err(NULL, "Failed to retrieve regulatory EIRP data");
		goto free_eirp;
	}

	ret_val = wlan_cfg80211_send_reg_eirp_update(wiphy, chan_eirp_list, band->n_channels);

free_eirp:
	kfree(chan_eirp_list);
	return ret_val;
}

static const struct nla_policy
ath12k_vendor_channel_switch_time_policy[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_PUNCT_BMAP] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL] = { .type = NLA_U32 },
};

int ath12k_get_num_beaconing_vifs(struct ath12k *ar)
{
	u32 count = 0;
	struct ath12k_link_vif *arvif;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_up &&
		    arvif->ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
		    arvif->beacon_interval > 0)
			count++;
	}
	return count;
}

#ifdef CPTCFG_QCN_EXTN
static enum nl80211_chan_width
ath12k_vendor_bandwidth_to_chan_width(u32 bandwidth)
{
	switch (bandwidth) {
	case 20:
		return NL80211_CHAN_WIDTH_20;
	case 40:
		return NL80211_CHAN_WIDTH_40;
	case 80:
		return NL80211_CHAN_WIDTH_80;
	case 160:
		return NL80211_CHAN_WIDTH_160;
	case 5:
		return NL80211_CHAN_WIDTH_5;
	case 10:
		return NL80211_CHAN_WIDTH_10;
	case 1:
		return NL80211_CHAN_WIDTH_1;
	case 2:
		return NL80211_CHAN_WIDTH_2;
	case 4:
		return NL80211_CHAN_WIDTH_4;
	case 8:
		return NL80211_CHAN_WIDTH_8;
	case 16:
		return NL80211_CHAN_WIDTH_16;
	case 320:
		return NL80211_CHAN_WIDTH_320;
	default:
		return bandwidth;
	}
}

static int
ath12k_vendor_validate_cs_time_chandef(struct wiphy *wiphy,
				       u32 freq, u32 bandwidth_attr,
				       u32 center_freq1, u32 center_freq2,
				       struct cfg80211_chan_def *chandef)
{
	struct ieee80211_channel *channel;

	if (!freq)
		return -EINVAL;

	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel)
		return -EINVAL;

	memset(chandef, 0, sizeof(*chandef));
	chandef->chan = channel;
	chandef->width = ath12k_vendor_bandwidth_to_chan_width(bandwidth_attr);

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_5:
	case NL80211_CHAN_WIDTH_10:
	case NL80211_CHAN_WIDTH_1:
	case NL80211_CHAN_WIDTH_2:
	case NL80211_CHAN_WIDTH_4:
	case NL80211_CHAN_WIDTH_8:
	case NL80211_CHAN_WIDTH_16:
		if (center_freq2)
			return -EINVAL;
		if (center_freq1 && center_freq1 != freq)
			return -EINVAL;
		center_freq1 = freq;
		center_freq2 = 0;
		break;
	case NL80211_CHAN_WIDTH_320:
		if (center_freq1 == freq + 150 ||
		    center_freq1 == freq + 130 ||
		    center_freq1 == freq + 110 ||
		    center_freq1 == freq + 90 ||
		    center_freq1 == freq - 90 ||
		    center_freq1 == freq - 110 ||
		    center_freq1 == freq - 130 ||
		    center_freq1 == freq - 150)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_160:
		if (center_freq1 == freq + 70 ||
		    center_freq1 == freq + 50 ||
		    center_freq1 == freq - 50 ||
		    center_freq1 == freq - 70)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_80P80:
	case NL80211_CHAN_WIDTH_80:
		if (center_freq1 == freq + 30 ||
		    center_freq1 == freq - 30)
			break;
		fallthrough;
	case NL80211_CHAN_WIDTH_40:
		if (center_freq1 == freq + 10 ||
		    center_freq1 == freq - 10)
			break;
		fallthrough;
	default:
		return -EINVAL;
	}

	chandef->center_freq1 = center_freq1;
	chandef->center_freq2 = center_freq2;
	return 0;
}

static int ath12k_vendor_get_channel_switch_time(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	u32 beacon_time, restart_time, dfs_time;
	struct ath12k_link_vif *arvif;
	u64 tot_chan_switch_time;
	struct ath12k *ar;
	int dfs_required;
	u8 link_id;
	struct ath12k_base *ab;
	struct sk_buff *reply;
	u32 tgt_restart_time;
	u32 drv_restart_time;
	u32 active_vifs;
	u32 freq = 0, center_freq1 = 0, center_freq2 = 0;
	u32 bandwidth_attr = NL80211_CHAN_WIDTH_20_NOHT;
	u32 channel_switch_time = 0;
	struct cfg80211_chan_def chandef;
	int ret;

	if (!wdev || !wdev->netdev)
		return -EINVAL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return -EINVAL;

	if (!data || !data_len) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "No data provided for channel switch time\n");
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX,
			data, data_len, ath12k_vendor_channel_switch_time_policy,
			NULL);
	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Failed to parse channel switch time attributes: %d\n", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ]) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Channel switch time: missing freq\n");
		return -EINVAL;
	}

	freq = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_FREQ]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH])
		bandwidth_attr =
		      nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_BANDWIDTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1])
		center_freq1 =
		   nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ1]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2])
		center_freq2 =
		   nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_CENTER_FREQ2]);

	ret = ath12k_vendor_validate_cs_time_chandef(hw->wiphy, freq,
						     bandwidth_attr,
						     center_freq1,
						     center_freq2,
						     &chandef);
	if (ret) {
		ath12k_dbg(NULL, ATH12K_DBG_MAC,
			   "Channel switch time: invalid chandef params (freq=%u width=%u cf1=%u cf2=%u)\n",
			   freq, bandwidth_attr, center_freq1, center_freq2);
		return ret;
	}

	/*
	 * Get beacon interval of link vif that corresponds to the channel.
	 * Use ath12k_mac_select_scan_device to get the ar corresponding
	 * to this channel freq and get corresponding link.
	 */
	ar = ath12k_mac_select_scan_device(hw, vif, center_freq1);
	if (!ar)
		return -EINVAL;

	link_id = ath12k_mac_find_link_id_by_ar(ahvif, ar);
	arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
	if (!arvif)
		return -EINVAL;
	/*
	 * Accounts for two beacon intervals:
	 * 1. Time to receive CSA completion event from firmware.
	 * 2. Time for firmware to transmit a beacon on the new channel
	 *    after processing the vdev up command from the host.
	 */
	beacon_time = arvif->beacon_interval * 2;

	ab = ar->ab;
	if (!ab)
		return -EINVAL;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "vendor get channel switch time: freq=%u bw=%u cf1=%u cf2=%u\n",
		   freq, bandwidth_attr, center_freq1, center_freq2);

	active_vifs = ath12k_get_num_beaconing_vifs(ar);
	tgt_restart_time = ATH12K_CSA_FW_RESTART_TIME_DELAY;
	drv_restart_time = (active_vifs * ATH12K_CHAN_SWITCH_RESTART_TIME_DELAY);

	restart_time = tgt_restart_time + drv_restart_time;

	if (!ab->qmi.cal_done && !ab->early_cal_support)
		restart_time += ATH12K_CSA_CALDB_UNDONE_TIME;

	dfs_required = cfg80211_chandef_dfs_required(hw->wiphy, &chandef,
						     vif->type);
	if (dfs_required > 0 &&
	    !cfg80211_chandef_dfs_available(wiphy, &chandef)) {
		dfs_time = cfg80211_chandef_dfs_cac_time(hw->wiphy, &chandef,
							 false, false);
		dfs_time += ATH12K_CSA_DFS_GRACE_TIME;
	} else {
		dfs_time = 0;
	}

	tot_chan_switch_time = beacon_time + restart_time + dfs_time;
	if (tot_chan_switch_time > U32_MAX)
		channel_switch_time = U32_MAX;
	else
		channel_switch_time = tot_chan_switch_time;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "get channel switch time: beacon=%u restart=%u dfs=%u total=%u\n",
		   beacon_time, restart_time, dfs_time, channel_switch_time);

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!reply)
		return -ENOMEM;

	if (nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_TOTAL,
			channel_switch_time)) {
		kfree_skb(reply);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(reply);
}
#endif /* CPTCFG_QCN_EXTN */

static int ath12k_vendor_sdwf_streaming_stats_configure(struct wireless_dev *wdev,
							struct nlattr *streaming_stats)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX + 1];
	struct ath12k *ar = NULL;
	int ret = 0;
	u8 basic_stats_configure, extnd_stats_configure, link_id;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX,
			       streaming_stats,
			       ath12k_vendor_sdwf_streaming, NULL);
	if (ret) {
		ath12k_err(NULL, "invalid sawf streaming stats configuration\n");
		return ret;
	}

	if (wdev->valid_links) { /* MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID])
			return -EINVAL;
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID]);
		if (!(wdev->valid_links & BIT(link_id)))
			return -ENOLINK;
	} else { /* NON-MLO case */
		if (!tb[QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID])
			link_id = 0;
		else
			return -EINVAL;
	}

	ar = ath12k_get_ar_from_wdev(wdev, link_id);
	if (!ar)
		return -ENODATA;

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS]) {
		basic_stats_configure = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS]);
		ret = ath12k_htt_sawf_streaming_stats_configure(ar, HTT_STRM_GEN_MPDUS_STATS,
								basic_stats_configure, 0, 0, 0, 0);
		if (ret)
			return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS]) {
		extnd_stats_configure = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS]);
		ret = ath12k_htt_sawf_streaming_stats_configure(ar, HTT_STRM_GEN_MPDUS_DETAILS_STATS,
								extnd_stats_configure, 0, 0, 0, 0);
	}

	return ret;
}

static int ath12k_vendor_telemetry_sla_reset_stats(struct nlattr *clr_stats)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX + 1];
	int ret = 0;
	u8 svc_id, mac_addr[ETH_ALEN] = { 0 }, mld_mac_addr[ETH_ALEN] = { 0 }, set_clear;

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX,
			       clr_stats,
			       ath12k_telemetric_sla_policy, NULL);

	if (ret) {
		ath12k_err(NULL, "Invalid attribute with telemetry sla reset stats command\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID])
		svc_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC] &&
	    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC]) == ETH_ALEN))
		memcpy(mac_addr, nla_data(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC]),
		       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC] &&
	    (nla_len(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC]) == ETH_ALEN))
		memcpy(mld_mac_addr, nla_data(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC]),
		       ETH_ALEN);

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR])
		set_clear = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR]);

	return ath12k_telemetry_reset_peer_stats(mac_addr);
}

static int ath12k_vendor_sdwf_dev_operations(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX + 1];
	u8 sdwf_oper;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX, data, data_len,
			ath12k_vendor_sdwf_dev_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Invalid attributes with SAWF device level commands\n");
		goto end;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION]) {
		sdwf_oper = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION]);
	} else {
		ath12k_err(NULL, "SAWF device level operation missing\n");
		ret = -EINVAL;
		goto end;
	}

	switch (sdwf_oper) {
	case QCA_WLAN_VENDOR_SDWF_DEV_OPER_STREAMING_STATS:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS]) {
			ret = ath12k_vendor_sdwf_streaming_stats_configure(wdev, tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS]);
		} else {
			ath12k_err(NULL, "SAWF default streaming statsparameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	case QCA_WLAN_VENDOR_SDWF_DEV_OPER_RESET_STATS:
		if (tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS]) {
			ret = ath12k_vendor_telemetry_sla_reset_stats(tb[QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS]);
		} else {
			ath12k_err(NULL, "SAWF clear telemetry stats parameters missing\n");
			ret = -EINVAL;
			goto end;
		}
		break;
	default:
		ath12k_err(NULL, "Invalid operation = %d with SAWF device level commands\n", sdwf_oper);
		ret = -EINVAL;
	}
end:
	return ret;
}

void ath12k_vendor_telemetry_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 svc_id, u8 param, bool set_clear,
					   u8 tid, u8 *mld_addr)
{
	struct nlattr *notify_params;
	struct wireless_dev *wdev;
	struct sk_buff *skb;
	u8 access_category;

	wdev = ieee80211_vif_to_wdev(vif);

	if (!wdev)
		return;

	if (!wdev->wiphy)
		return;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL, "No memory available to send notify breach event\n");
		return;
	}

	switch (tid) {
	case 0:
	case 3:
		access_category = 0; //AC_BE
		break;
	case 1:
	case 2:
		access_category = 1; //AC_BK
		break;
	case 4:
	case 5:
		access_category = 2; //AC_VI
		break;
	case 6:
	case 7:
		access_category = 3; //AC_VO
		break;
	default:
		ath12k_err(NULL, "Invalid TID = %u for notifying breach event\n", tid);
		goto err;
	}

	notify_params = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_SDWF_DEV_SLA_BREACHED_PARAMS);
	if (!notify_params)
		goto err;
	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC, ETH_ALEN, mac_addr) ||
	    (mld_addr && nla_put(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC,
	    ETH_ALEN, mld_addr)) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID, svc_id) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE, param) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR, set_clear) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC, access_category)) {
		ath12k_err(NULL, "No memory available at NL to send notify breach event\n");
		goto err;
	}

	nla_nest_end(skb, notify_params);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	return;
err:
	kfree(skb);
}

static const char *get_breach_type_str(u8 breach_type)
{
	switch (breach_type) {
	case 0:
		return "RSSI_MIN";
	case 1:
		return "RSSI_MAX";
	case 2:
		return "ACK_RSSI_MIN";
	case 3:
		return "ACK_RSSI_MAX";
	case 4:
		return "TX_RATE_MIN";
	case 5:
		return "TX_RATE_MAX";
	case 6:
		return "RX_RATE_MIN";
	case 7:
		return "RX_RATE_MAX";
	default:
		return "UNKNOWN";
	}
}

/**
 * ath12k_vendor_rssi_rate_notify_breach - Send RSSI/Rate breach NL event
 * This function creates and sends a netlink vendor event to userspace
 * notifying about RSSI/Rate threshold breach.
 */
void ath12k_vendor_rssi_rate_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 breach_type, u32 threshold_value,
					   u32 detected_value, bool set_clear,
					   u8 *mld_addr)
{
	struct nlattr *notify_params;
	struct wireless_dev *wdev;
	struct sk_buff *skb;

	wdev = ieee80211_vif_to_wdev(vif);

	if (!wdev)
		return;

	if (!wdev->wiphy)
		return;

	skb = cfg80211_vendor_event_alloc(wdev->wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX,
					  GFP_ATOMIC);
	if (!skb) {
		ath12k_err(NULL, "No memory for RSSI/Rate breach event\n");
		return;
	}

	notify_params =
		nla_nest_start(skb,
			       QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RSSI_RATE_BREACH_PARAMS);
	if (!notify_params)
		goto err;

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MAC,
		    ETH_ALEN, mac_addr) ||
	    (mld_addr && nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_PEER_MLD_MAC,
				 ETH_ALEN, mld_addr)) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_TYPE, breach_type))
		goto nla_put_failure;

	if (breach_type <= THRESHOLD_ACKRSSI_MAX) {
		if (nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD,
				(s32)threshold_value) ||
		    nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE,
				(s32)detected_value))
			goto nla_put_failure;
	} else {
		if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_THRESHOLD,
				threshold_value) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_VALUE,
				detected_value))
			goto nla_put_failure;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_RATE_BREACH_SET_CLEAR,
		       set_clear))
		goto nla_put_failure;

	nla_nest_end(skb, notify_params);
	cfg80211_vendor_event(skb, GFP_ATOMIC);

	if (set_clear) {
		if (breach_type <= THRESHOLD_ACKRSSI_MAX) {
			pr_err("RSSI/Rate Telemetry: %s Breach Detected, peer: %pM, Threshold: %d, Detected Value: %d\n",
			       get_breach_type_str(breach_type), mac_addr,
			       (s32)threshold_value, (s32)detected_value);
		} else {
			pr_err("RSSI/Rate Telemetry: %s Breach Detected, peer: %pM, Threshold: %u, Detected Value: %u\n",
			       get_breach_type_str(breach_type), mac_addr,
			       threshold_value, detected_value);
		}
	}
	return;

nla_put_failure:
	ath12k_err(NULL, "No memory for RSSI/Rate breach NL attributes\n");
err:
	kfree_skb(skb);
}

static struct ath12k_link_vif *
ath12k_vendor_get_non_scan_arvif(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (list_empty(&ar->arvifs))
		return NULL;

	list_for_each_entry(arvif, &ar->arvifs, list)
		if (!arvif->is_scan_vif && arvif->is_started)
			return arvif;

	return list_first_entry(&ar->arvifs, typeof(*arvif), list);
}

int ath12k_vendor_put_ar_hw_link_id(struct sk_buff *vendor_event,
				    struct ath12k *ar)
{
	if (nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID,
			ar->pdev->hw_link_id)) {
		ath12k_err(ar->ab, "failed to put hw link id %u for soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ar_link_mac_addr(struct sk_buff *vendor_event,
				       struct ath12k *ar)
{
	struct ath12k_link_vif *arvif = ath12k_vendor_get_non_scan_arvif(ar);

	if (!arvif)
		return -EINVAL;

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LINK_MAC,
		    6, (void *)arvif->bssid)) {
		ath12k_err(ar->ab, "failed to put mac addr for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

enum qca_wlan_vendor_channel_width
ath12k_nl_chan_bw_to_qca_vendor_chan_bw(enum nl80211_chan_width chan_bw)
{
	switch (chan_bw) {
	case NL80211_CHAN_WIDTH_20:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ;
	case NL80211_CHAN_WIDTH_40:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ;
	case NL80211_CHAN_WIDTH_80:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_160:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ;
	case NL80211_CHAN_WIDTH_80P80:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ;
	case NL80211_CHAN_WIDTH_320:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ;
	default:
		return QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID;
	}

	return QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID;
}

int ath12k_vendor_put_ar_chan_info(struct sk_buff *vendor_event,
				   struct ath12k *ar)
{
	struct ieee80211_chanctx_conf *ctx = NULL;

	ctx = ath12k_mac_get_first_active_arvif_chanctx(ar);

	if (!ctx)
		return -1;

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW,
		       ath12k_nl_chan_bw_to_qca_vendor_chan_bw(ctx->def.width))) {
		ath12k_err(ar->ab, "failed to put chan bw for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ,
			ctx->def.chan->center_freq)) {
		ath12k_err(ar->ab, "failed to put chan center freq for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ar_nss_chains(struct sk_buff *vendor_event,
				    struct ath12k *ar)
{
	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK,
		       ar->pdev->cap.tx_chain_mask)) {
		ath12k_err(ar->ab, "failed to put tx chain mask for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK,
		     ar->pdev->cap.rx_chain_mask)) {
		ath12k_err(ar->ab, "failed to put rx chain mask for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ar_wireless_mode(struct sk_buff *vendor_event,
				       struct ath12k *ar)
{
	struct ieee80211_chanctx_conf *ctx;
	struct ath12k_link_vif *arvif;
	enum wmi_phy_mode phymode;

	ctx = ath12k_mac_get_first_active_arvif_chanctx(ar);
	if (!ctx)
		return -1;

	arvif = container_of(ctx, struct ath12k_link_vif, chanctx);
	phymode = arvif->ahvif->dp_vif.dp_link_vif[arvif->link_id].phymode;

	if (nla_put_string(vendor_event, QCA_WLAN_VENDOR_ATTR_LINK_WIRELESS_MODE,
			   ath12k_mac_phymode_str(phymode))) {
		ath12k_err(ar->ab, "failed to put wireless mode for hw link id %u soc %d",
			   ar->pdev->hw_link_id, ath12k_get_ab_device_id(ar->ab));
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ab_soc_id(struct sk_buff *vendor_event,
				struct ath12k_base *ab)
{
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID,
		       ab->device_id)) {
		ath12k_err(ab, "failed to put soc device id for soc %d",
			   ab->device_id);
		return -1;
	}

	return 0;
}

int ath12k_vendor_put_ab_num_links(struct sk_buff *vendor_event,
				   struct ath12k_base *ab,
				   const int num_active_links)
{
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS,
		       num_active_links)) {
		ath12k_err(ab, "failed to put number of hw links for soc %d",
			   ab->device_id);
		return -1;
	}

	return 0;
}

static int ath12k_vendor_set_wifi_params_me(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    struct ath12k_wifi_generic_params *params)
{
	u16 grp_limit = ATH12K_ME_MAX_GRP_LIMIT;
	u32 val = *(u32 *)params->data;
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_me_db *me_db;
	struct ath12k_vif *ahvif;
	u32 me_flags = 0;
	bool disable_sg;

	ahvif = ath12k_get_ahvif_from_wdev(wdev);
	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present");
		return -EINVAL;
	}

	/* Validate interface type - ME only applicable to AP mode */
	if (ahvif->vif->type != NL80211_IFTYPE_AP) {
		ath12k_err(NULL, "ME configuration only supported on AP interfaces");
		return -EOPNOTSUPP;
	}

	dp_vif = &ahvif->dp_vif;
	if (!dp_vif) {
		ath12k_err(NULL, "dp_vif not present");
		return -EINVAL;
	}

	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db) {
		ath12k_err(NULL, "me_db not present");
		return -EINVAL;
	}

	switch (params->value) {
	case QCA_WLAN_VENDOR_VDEV_PARAM_ME:
		if (val == 5) {
			me_flags = ATH12K_ME_FLAGS_BIT_ME5;
			dp_vif->dp_features |= DP_FEATURE_ME;
			disable_sg = true;  /* SG capabiltiy to be disabled */
		} else if (val == 6) {
			me_flags = ATH12K_ME_FLAGS_BIT_ME6;
			dp_vif->dp_features |= DP_FEATURE_ME;
			disable_sg = true;  /* SG capability to be disabled */
		} else if (val == 0) {
			me_flags = 0;
			dp_vif->dp_features &= ~(DP_FEATURE_ME);
			if (ahvif->dp_vif.tx_encap_type == ATH12K_HW_TXRX_ETHERNET) {
				/* SG capability to be restored */
				disable_sg = false;
			}
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Unsupported value for param: %d value: %d\n",
				   params->value, val);
			goto fail;
		}
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_IGMP_ME:
		if (val != 0 && val != 1) {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Unsupported value for param: %d value: %d\n",
				   params->value, val);
			goto fail;
		}
		me_flags = val ? ATH12K_ME_FLAGS_BIT_IGMP_EN : 0;
		break;

	case QCA_WLAN_VENDOR_VDEV_PARAM_ME_GRP_LIMIT:
		if (val > 0 && val < ATH12K_ME_MAX_GRP_LIMIT) {
			grp_limit = val;
		} else {
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Unsupported value for param: %d value: %d\n",
				   params->value, val);
			goto fail;
		}
		break;

	default:
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
				"Unsupported param: %d\n", params->value);
		goto fail;
	}

	/* Update the ME Database */
	spin_lock_bh(&me_db->lock);

	/* Clear the older flags */
	if (params->value == QCA_WLAN_VENDOR_VDEV_PARAM_ME)
		me_db->me_flags &= ~ATH12K_ME_OFFLOAD_MASK;
	else if (params->value == QCA_WLAN_VENDOR_VDEV_PARAM_IGMP_ME)
		me_db->me_flags &= ~ATH12K_ME_FLAGS_BIT_IGMP_EN;

	me_db->me_flags |= me_flags;
	me_db->grp_limit = grp_limit;

	spin_unlock_bh(&me_db->lock);

	ath12k_print_me_configs(me_db);
	ath12k_me_db_put(me_db);

	/* Workqueue to enable/disable SG capability*/
	if (ahvif->dp_vif.tx_encap_type == ATH12K_HW_TXRX_ETHERNET &&
	    params->value == QCA_WLAN_VENDOR_VDEV_PARAM_ME) {
		ahvif->disable_sg = disable_sg;
		schedule_work(&ahvif->disable_sg_netdev_work);
	}

	return 0;

fail:
	ath12k_me_db_put(me_db);
	return -EINVAL;
}

static int ath12k_vendor_me_config_handler(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX + 1];
	struct ath12k_wifi_generic_params params = {0};
	u32 value = 0;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX,
			data, data_len,
			ath12k_vendor_me_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse ME config attributes: %d\n", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_PARAM]) {
		ath12k_err(NULL, "Missing ME config parameter attribute\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_VALUE]) {
		ath12k_err(NULL, "Missing ME config value attribute\n");
		return -EINVAL;
	}

	params.value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_PARAM]);
	value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ME_CONFIG_VALUE]);

	/* Validate parameter ID */
	if (params.value != QCA_WLAN_VENDOR_VDEV_PARAM_ME &&
	    params.value != QCA_WLAN_VENDOR_VDEV_PARAM_IGMP_ME &&
	    params.value != QCA_WLAN_VENDOR_VDEV_PARAM_ME_GRP_LIMIT) {
		ath12k_err(NULL, "Invalid ME parameter ID: %u\n", params.value);
		return -EINVAL;
	}

	params.data = &value;

	return ath12k_vendor_set_wifi_params_me(wiphy, wdev, &params);
}

static int ath12k_vendor_me_dump(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_MAX + 1];
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_me_db *me_db;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_base *ab;
	int ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_MAX,
		      data, data_len,
		      ath12k_vendor_me_dump_policy, NULL)) {
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_TYPE])
		return -EINVAL;

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);
	dp_vif = &ahvif->dp_vif;
	ab = ath12k_ah_to_ar(ahvif->ah, 0)->ab;

	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db)
		return -ENOENT;

#ifdef CPTCFG_QCN_EXTN
	switch (nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_TYPE])) {
	case IEEE80211_ME_STATS_PRINT_INFO:
		ret = ath12k_me_info_print_extn(me_db, dp_vif);
		break;
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
	case IEEE80211_ME_STATS_PRINT_SNOOP:
		ret = ath12k_me_snoop_print_extn(ab, me_db);
		break;
#endif /*CONFIG_BRIDGE_MCAST_OFFLOAD*/
	default:
		ret = -EINVAL;
	}
#else
	ret = -EOPNOTSUPP;
#endif /* CPTCFG_QCN_EXTN */

	ath12k_me_db_put(me_db);
	return ret;
}

static int ath12k_vendor_igmp_tid_override_handler(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_MAX + 1];
	struct ath12k *ar;
	u8 radio_id = 0;
	u32 value;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_MAX,
			data, data_len,
			ath12k_vendor_igmp_tid_override_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse IGMP TID override attrs: %d\n", ret);
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_RADIO_ID])
		radio_id =
		    nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_RADIO_ID]);

	if (radio_id >= ah->num_radio) {
		ath12k_err(NULL, "Invalid radio_id %u for IGMP TID (num_radio=%u)\n",
			   radio_id, ah->num_radio);
		return -EINVAL;
	}

	ar = ath12k_ah_to_ar(ah, radio_id);
	if (!ar) {
		ath12k_err(NULL, "Failed to get ar for radio_id %u\n", radio_id);
		return -EINVAL;
	}

	/* GET operation: VALUE attr absent — return cached value */
	if (!tb[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_VALUE]) {
		struct sk_buff *skb;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							  nla_total_size(sizeof(u32)));
		if (!skb)
			return -ENOMEM;

		if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_VALUE,
				ar->igmp_tid_override)) {
			kfree_skb(skb);
			return -ENOBUFS;
		}

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "IGMP/MLD TID override get: radio=%u value=%u\n",
			   radio_id, ar->igmp_tid_override);

		return cfg80211_vendor_cmd_reply(skb);
	}

	/* SET operation: VALUE attr present */
	value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_VALUE]);

	if (value > 7) {
		ath12k_err(NULL, "Invalid IGMP TID override value: %u (must be 0-7)\n",
			   value);
		return -EINVAL;
	}

	ret = ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_IGMPMLD_AC_OVERRIDE,
					value, ar->pdev->pdev_id);
	if (ret) {
		ath12k_err(ar->ab,
			   "Failed to set IGMP/MLD TID override (radio=%u value=%u): %d\n",
			   radio_id, value, ret);
	} else {
		ar->igmp_tid_override = value;
		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "IGMP/MLD TID override set: radio=%u value=%u (%s)\n",
			   radio_id, value, value ? "enabled" : "disabled");
	}

	return ret;
}

static const struct nla_policy
ath12k_vendor_me_list_policy[QCA_WLAN_VENDOR_ATTR_ME_LIST_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_OPERATION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_IP_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR] = { .type = NLA_BINARY,
						.len = ATH12K_IPV4_ADDR_LEN },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR] = { .type = NLA_BINARY,
						.len = ATH12K_IPV6_ADDR_LEN },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_MASK] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_ME_LIST_PREFIX] = { .type = NLA_U32 },
};

/**
 * ath12k_parse_me_list_entry - Parse ME list entry from netlink attributes
 * @tb: netlink attribute table
 * @me_entry: output ME entry structure
 * @ip_type: IP type (IPv4 or IPv6)
 */
static int ath12k_parse_me_list_entry(struct nlattr **tb,
				      struct ieee80211_wlanconfig_me_list *me_entry,
				      u8 ip_type)
{
	ath12k_dbg(NULL, ATH12K_DBG_CFG, "Parsing ME list entry, IP type: %u\n", ip_type);

	if (ip_type == QCA_WLAN_VENDOR_ME_IP_TYPE_IPV4) {
		/* Parse IPv4 address */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR] ||
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR])
				!= ATH12K_IPV4_ADDR_LEN) {
			ath12k_err(NULL, "Invalid IPv4 address\n");
			return -EINVAL;
		}
		memcpy(&me_entry->ip,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV4_ADDR]),
			ATH12K_IPV4_ADDR_LEN);

		/* Parse IPv4 mask */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_MASK]) {
			ath12k_err(NULL, "Missing IPv4 mask\n");
			return -EINVAL;
		}
		me_entry->mask = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_MASK]);

		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			"IPv4 ME entry: %pI4, mask: 0x%08x\n",
			&me_entry->ip, me_entry->mask);
	} else if (ip_type == QCA_WLAN_VENDOR_ME_IP_TYPE_IPV6) {
		/* Parse IPv6 address */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR] ||
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR])
				!= ATH12K_IPV6_ADDR_LEN) {
			ath12k_err(NULL, "Invalid IPv6 address\n");
			return -EINVAL;
		}
		memcpy(me_entry->ipv6,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IPV6_ADDR]),
			ATH12K_IPV6_ADDR_LEN);

		/* Parse IPv6 prefix */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_PREFIX]) {
			ath12k_err(NULL, "Missing IPv6 prefix\n");
			return -EINVAL;
		}
		me_entry->mask = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_PREFIX]);

		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "IPv6 ME entry: %pI6, prefix: %u\n",
			   me_entry->ipv6, me_entry->mask);
	} else {
		ath12k_err(NULL, "Invalid IP type: %u\n", ip_type);
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_validate_me_list_entry - Validate ME list entry
 * @me_entry: ME entry to validate
 */
static int ath12k_validate_me_list_entry(struct ieee80211_wlanconfig_me_list *me_entry)
{
	bool is_ipv6 = (me_entry->me_list_type == IEEE80211_HMMC_LIST_V6 ||
			me_entry->me_list_type == IEEE80211_DENY_LIST_V6);

	if (is_ipv6) {
		/* IPv6 multicast addresses start with 0xFF */
		u8 *ipv6_bytes = (u8 *)me_entry->ipv6;

		if (ipv6_bytes[0] != 0xFF) {
			ath12k_err(NULL, "Invalid IPv6 multicast address: %pI6\n",
				   me_entry->ipv6);
			return -EINVAL;
		}

		/* Validate prefix length */
		if (me_entry->mask > 128) {
			ath12k_err(NULL, "Invalid IPv6 prefix length: %u\n",
				   me_entry->mask);
			return -EINVAL;
		}
	} else {
		/* IPv4 multicast range: 224.0.0.0 to 239.255.255.255 */
		u32 addr = ntohl(me_entry->ip);

		if ((addr & 0xF0000000) != 0xE0000000) {
			ath12k_err(NULL, "Invalid IPv4 multicast address: %pI4\n",
				   &me_entry->ip);
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ath12k_add_me_list_entry - Add entry to ME list
 * @ahvif: ath12k_vif pointer
 * @me_entry: ME entry to add
 */
static int ath12k_add_me_list_entry(struct ath12k_vif *ahvif,
				    struct ieee80211_wlanconfig_me_list *me_entry)
{
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_me_db *me_db;
	int ret = 0;

	dp_vif = &ahvif->dp_vif;

	/* This will take the ref on me_db */
	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db) {
		ath12k_err(NULL, "me_db is NULL");
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "Adding ME list entry, type: %u\n", me_entry->me_list_type);

	/* Select target list based on list type */
	switch (me_entry->me_list_type) {
	case IEEE80211_HMMC_LIST:
		ret = ath12k_me_hmmc_add(me_db, &me_entry->ip, false,
					 me_entry->mask, ATH12K_ME_HMMC_ACTION);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed adding HMMC IPV4 entry to db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_HMMC_LIST_V6:
		ret = ath12k_me_hmmc_add(me_db, me_entry->ipv6, true,
					 me_entry->mask, ATH12K_ME_HMMC_ACTION);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed adding HMMC IPV6 entry to db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_DENY_LIST:
		ret = ath12k_me_hmmc_add(me_db, &me_entry->ip, false,
					 me_entry->mask, ATH12K_ME_DENYLIST_ACTION);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed adding DENY IPV4 entry to db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_DENY_LIST_V6:
		ret = ath12k_me_hmmc_add(me_db, me_entry->ipv6, true,
					 me_entry->mask, ATH12K_ME_DENYLIST_ACTION);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed adding DENY IPV6 entry to db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	default:
		ath12k_err(NULL, "Invalid ME list type: %u\n", me_entry->me_list_type);
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	/*TODO: Increment the stats counter */
	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "Added a ME list entry to db:%p, me_entry: %p\n",
		   &me_db->hmmc_db, me_entry);

	/* This will drop the ref for me_db */
	ath12k_me_db_put(me_db);
	return ret;
}

/**
 * ath12k_del_me_list_entry - Delete entry from ME list
 * @ahvif: ath12k_vif pointer
 * @me_entry: ME entry to delete
 */
static int ath12k_del_me_list_entry(struct ath12k_vif *ahvif,
				    struct ieee80211_wlanconfig_me_list *me_entry)
{
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_me_db *me_db;
	int ret = 0;

	dp_vif = &ahvif->dp_vif;

	/* This will take the ref on me_db */
	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db) {
		ath12k_err(NULL, "me_db is NULL");
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "Deleting ME list entry, type: %u\n", me_entry->me_list_type);

	switch (me_entry->me_list_type) {
	case IEEE80211_HMMC_LIST:
		ret = ath12k_me_hmmc_del(me_db, &me_entry->ip, false,
					 me_entry->mask);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed deleting HMMC IPV4 entry from db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_HMMC_LIST_V6:
		ret = ath12k_me_hmmc_del(me_db, me_entry->ipv6, true,
					 me_entry->mask);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed deleting HMMC IPV6 entry from db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_DENY_LIST:
		ret = ath12k_me_hmmc_del(me_db, &me_entry->ip, false,
					 me_entry->mask);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed deleting DENY IPV4 entry from db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	case IEEE80211_DENY_LIST_V6:
		ret = ath12k_me_hmmc_del(me_db, me_entry->ipv6, true,
					 me_entry->mask);
		if (ret < 0) {
			/* TODO: Increment the stats */
			ath12k_dbg(NULL, ATH12K_DBG_CFG,
				   "Failed deleting DENY IPV6 entry from db:%p, me_entry: %p\n",
				   &me_db->hmmc_db, me_entry);
			ath12k_me_db_put(me_db);
			return ret;
		}
		break;
	default:
		ath12k_err(NULL, "Invalid ME list type: %u\n", me_entry->me_list_type);
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	/* TODO: Increment the stats */
	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "Deleted ME list entry from db: %p\n", &me_db->hmmc_db);

	/* This will drop the ref for me_db */
	ath12k_me_db_put(me_db);
	return ret;
}

/**
 * ath12k_dump_me_list_entries - Dump ME list entries
 * @ahvif: ath12k_vif pointer
 * @list_type: ME list type to dump
 */
static int ath12k_dump_me_list_entries(struct ath12k_vif *ahvif, u8 list_type)
{
	struct ath12k_dp_vif *dp_vif;
	struct ath12k_me_db *me_db;
#ifdef CPTCFG_QCN_EXTN
	u32 filter_flags;
#endif /* CPTCFG_QCN_EXTN */
	u16 count = 0;
	int ret;

	dp_vif = &ahvif->dp_vif;

	/* This will take the ref on me_db */
	me_db = ath12k_me_db_get(dp_vif);
	if (!me_db) {
		ath12k_err(NULL, "me_db is NULL");
		return -EINVAL;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "Dumping ME list entries from db: %p, list_type: %u\n",
		   &me_db->hmmc_db, list_type);

	/* Map list_type to dump filter flags */
#ifdef CPTCFG_QCN_EXTN
	switch (list_type) {
	case IEEE80211_HMMC_LIST:
		filter_flags = ATH12K_ME_DUMP_HMMC_V4;
		break;
	case IEEE80211_HMMC_LIST_V6:
		filter_flags = ATH12K_ME_DUMP_HMMC_V6;
		break;
	case IEEE80211_DENY_LIST:
		filter_flags = ATH12K_ME_DUMP_DENY_V4;
		break;
	case IEEE80211_DENY_LIST_V6:
		filter_flags = ATH12K_ME_DUMP_DENY_V6;
		break;
	case IEEE80211_ME_LIST_ALL:
		filter_flags = ATH12K_ME_DUMP_ALL;
		break;
	default:
		ath12k_err(NULL, "Invalid ME list type: %u\n", list_type);
		ath12k_me_db_put(me_db);
		return -EINVAL;
	}

	ret = ath12k_me_hmmc_dump_extn(me_db, filter_flags, &count);
#else
	ret = -EOPNOTSUPP;
#endif /* CPTCFG_QCN_EXTN */
	if (ret < 0)
		ath12k_err(NULL, "Error dumping ME list entries (type %u)\n",
			   list_type);
	else
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "ME list dump complete: %u entries\n", count);

	/* This will drop the ref for me_db */
	ath12k_me_db_put(me_db);
	return ret;
}

/**
 * ath12k_vendor_hmmc_deny_list_handler - Handle ME list vendor command
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: vendor command data
 * @data_len: vendor command data length
 *
 * This function handles ME list configuration commands from userspace.
 * It supports both HMMC and Deny lists with IPv4/IPv6 addresses.
 */
static int ath12k_vendor_hmmc_deny_list_handler(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_MAX + 1];
	struct ieee80211_wlanconfig_me_list me_entry;
	u8 operation, list_type, ip_type, hw_idx = 0;
	int ifidx = 0;
	int ret = 0;
	struct ath12k_vif *ahvif;

	ath12k_dbg(NULL, ATH12K_DBG_CFG, "Received ME list command\n");

	/* Validate input parameters */
	if (!data || !data_len) {
		ath12k_err(NULL, "Invalid ME list data\n");
		return -EINVAL;
	}

	ahvif = ath12k_get_ahvif_from_wdev(wdev);
	if (!ahvif) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "Failed to retrieve ahvif\n");
		return -EINVAL;
	}

	/* Validate interface type - HMMC configs are for AP mode alone */
	if (ahvif->vif->type != NL80211_IFTYPE_AP) {
		ath12k_dbg(NULL, ATH12K_DBG_CFG,
			   "HMMC config is only supported on AP interfaces\n");
		return -EOPNOTSUPP;
	}

	/* Parse netlink attributes */
	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ME_LIST_MAX, data, data_len,
			ath12k_vendor_me_list_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse ME list attributes: %d\n", ret);
		return ret;
	}

	if (!tb[NL80211_ATTR_IFINDEX]) {
		ath12k_err(NULL, "Not a valid interface index for ME\n");
		return -EINVAL;
	}
	ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	if (ifidx < 0)
		return -EINVAL;

	/* Extract operation */
	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_OPERATION]) {
		ath12k_err(NULL, "Missing ME list operation\n");
		return -EINVAL;
	}
	operation = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_OPERATION]);

	/* Extract list type */
	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_TYPE]) {
		ath12k_err(NULL, "Missing ME list type\n");
		return -EINVAL;
	}
	list_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_TYPE]);

	/* Extract IP type */
	if (!tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IP_TYPE]) {
		ath12k_err(NULL, "Missing ME list IP type\n");
		return -EINVAL;
	}
	ip_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ME_LIST_IP_TYPE]);

	/* Get radio instance */
	ar = &ah->radio[hw_idx];
	if (!ar) {
		ath12k_err(NULL, "Invalid radio instance\n");
		return -ENODEV;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ME list operation: %u, list_type: %u, ip_type: %u\n",
		   operation, list_type, ip_type);

	/* Initialize ME entry */
	memset(&me_entry, 0, sizeof(me_entry));
	me_entry.me_list_type = list_type;

	/* Process based on operation */
	switch (operation) {
	case IEEE80211_WLANCONFIG_ME_LIST_ADD:
	case IEEE80211_WLANCONFIG_ME_LIST_DEL:

		/* Extract IP address and mask/prefix */
		ret = ath12k_parse_me_list_entry(tb, &me_entry, ip_type);
		if (ret) {
			ath12k_err(NULL, "Failed to parse ME list entry: %d\n", ret);
			return ret;
		}

		/* Validate multicast address */
		ret = ath12k_validate_me_list_entry(&me_entry);
		if (ret) {
			ath12k_err(NULL, "Invalid ME list entry: %d\n", ret);
			return ret;
		}

		/* Process ADD/DEL operation */
		if (operation == IEEE80211_WLANCONFIG_ME_LIST_ADD)
			ret = ath12k_add_me_list_entry(ahvif, &me_entry);
		else
			ret = ath12k_del_me_list_entry(ahvif, &me_entry);
		break;

	case IEEE80211_WLANCONFIG_ME_LIST_DUMP:
		ret = ath12k_dump_me_list_entries(ahvif, list_type);
		break;

	default:
		ath12k_err(NULL, "Invalid ME list operation: %u\n", operation);
		ret = -EINVAL;
		break;
	}

	ath12k_dbg(NULL, ATH12K_DBG_CFG,
		   "ME list operation %u completed with result: %d\n",
		   operation, ret);

	return ret;
}

static int
ath12k_ext_mon_extract_filter(struct nlattr *filter_attr,
			      struct ath12k_ext_mon_pkt_config *pkt)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MAX + 1];
	int ret;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MAX,
			       filter_attr,
			       ath12k_vendor_ext_mon_pkt_config_filter_policy,
			       NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ext mon filter: %d\n", ret);
		return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT]) {
		u32 filter = nla_get_u32(
				tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT]);
		if (filter & ~ATH12K_EXT_MON_FILTER_ALL) {
			ath12k_err(NULL, "invalid mgmt filter 0x%x\n", filter);
			return -EINVAL;
		}
		pkt->filter[ATH12K_EXT_MON_FRAME_MGMT] = filter;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL]) {
		u32 filter = nla_get_u32(
				tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL]);
		if (filter & ~ATH12K_EXT_MON_FILTER_ALL) {
			ath12k_err(NULL, "invalid ctrl filter 0x%x\n", filter);
			return -EINVAL;
		}
		pkt->filter[ATH12K_EXT_MON_FRAME_CTRL] = filter;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA]) {
		u32 filter = nla_get_u32(
				tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA]);
		if (filter & ~ATH12K_EXT_MON_FILTER_ALL) {
			ath12k_err(NULL, "invalid data filter 0x%x\n", filter);
			return -EINVAL;
		}
		pkt->filter[ATH12K_EXT_MON_FRAME_DATA] = filter;
	}

	return 0;
}

static int
ath12k_ext_mon_extract_filter_len(struct nlattr *len_attr,
				  struct ath12k_ext_mon_pkt_config *pkt)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MAX + 1];
	int ret;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MAX,
			       len_attr, ath12k_vendor_ext_mon_pkt_config_len_policy,
			       NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ext mon len: %d\n", ret);
		return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT]) {
		u8 len = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT]);

		if (len < QCA_VENDOR_EXT_MON_LEN_64B ||
		    len > QCA_VENDOR_EXT_MON_LEN_FULL_PKT) {
			ath12k_err(NULL, "invalid mgmt frame len %u\n", len);
			return -EINVAL;
		}
		pkt->len[ATH12K_EXT_MON_FRAME_MGMT] = len;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL]) {
		u8 len = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL]);

		if (len < QCA_VENDOR_EXT_MON_LEN_64B ||
		    len > QCA_VENDOR_EXT_MON_LEN_FULL_PKT) {
			ath12k_err(NULL, "invalid ctrl frame len %u\n", len);
			return -EINVAL;
		}
		pkt->len[ATH12K_EXT_MON_FRAME_CTRL] = len;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA]) {
		u8 len = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA]);

		if (len < QCA_VENDOR_EXT_MON_LEN_64B ||
		    len > QCA_VENDOR_EXT_MON_LEN_FULL_PKT) {
			ath12k_err(NULL, "invalid data frame len %u\n", len);
			return -EINVAL;
		}
		pkt->len[ATH12K_EXT_MON_FRAME_DATA] = len;
	}

	return 0;
}

static int
ath12k_ext_mon_extract_data_mpdu_tlv(struct nlattr *attr,
				     struct ath12k_ext_mon_data_mpdu_tlv_config *tlv)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MAX + 1] = {0};
	int ret;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MAX,
			       attr,
			       ath12k_vendor_ext_mon_data_mpdu_tlv_policy, NULL);
	if (ret)
		return ret;

	if (tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MCAST]) {
		tlv->mcast =
			nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MCAST]);
		if (tlv->mcast > ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX) {
			ath12k_err(NULL, "invalid data MPDU TLV mcast filter 0x%x\n",
				   tlv->mcast);
			return -EINVAL;
		}
	}
	if (tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_UCAST]) {
		tlv->ucast =
			nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_UCAST]);
		if (tlv->ucast > ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX) {
			ath12k_err(NULL, "invalid data MPDU TLV ucast filter 0x%x\n",
				   tlv->ucast);
			return -EINVAL;
		}
	}
	if (tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_NULL]) {
		tlv->null_frm =
			nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_NULL]);
		if (tlv->null_frm > ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX) {
			ath12k_err(NULL, "invalid data MPDU TLV null filter 0x%x\n",
				   tlv->null_frm);
			return -EINVAL;
		}
	}

	tlv->tlv_configured = true;
	return 0;
}

static int
ath12k_ext_mon_extract_pkt_config(struct nlattr *pkt_attr,
				  struct ath12k_ext_mon_pkt_config *pkt,
				  bool is_peer_filter)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_MAX + 1];
	u32 data_filter;
	int ret;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_MAX,
			       pkt_attr, ath12k_vendor_ext_mon_pkt_config_policy,
			       NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ext mon pkt config: %d\n", ret);
		return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER]) {
		ret = ath12k_ext_mon_extract_filter(
			tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER],
			pkt);
		if (ret)
			return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN]) {
		ret = ath12k_ext_mon_extract_filter_len(
			tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN],
			pkt);
		if (ret)
			return ret;
	}

	if (is_peer_filter) {
		data_filter = pkt->filter[ATH12K_EXT_MON_FRAME_DATA];

		/* When FP or FPMO Data MPDU TLV filtering is active, the
		 * driver must send TLV masks for all three data subtypes.
		 * Default each subtype to 0xF (all TLVs) if it is in the
		 * data filter — packets are delivered so rx_header is valid.
		 * Default to 0x7 (mpdu_start+msdu_end+mpdu_end, no rx_header)
		 * for subtypes not in the filter, where rx_header is not allowed.
		 * User-supplied masks in the NL command override these defaults.
		 * The GET response shows all three subtypes; unset ones reflect
		 * their default value.
		 */
		pkt->data_mpdu_tlv.mcast = (data_filter & FILTER_DATA_MCAST) ?
					ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX : 0x7;
		pkt->data_mpdu_tlv.ucast = (data_filter & FILTER_DATA_UCAST) ?
					ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX : 0x7;
		pkt->data_mpdu_tlv.null_frm = (data_filter & FILTER_DATA_NULL) ?
					ATH12K_EXT_MON_MPDU_TLV_FILTER_MAX : 0x7;

		if (tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_DATA_MPDU_TLV]) {
			ret = ath12k_ext_mon_extract_data_mpdu_tlv(
				tb[QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_DATA_MPDU_TLV],
				&pkt->data_mpdu_tlv);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int
ath12k_ext_mon_extract_filter_config(struct nlattr *filter_attr,
				     struct ath12k_ext_mon_filter_config *filter)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MAX + 1] = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MAX,
			       filter_attr, ath12k_vendor_ext_mon_filter_config_policy,
			       NULL);

	if (ret) {
		ath12k_err(NULL, "failed to parse filter config attributes: %d\n", ret);
		return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL]) {
		u8 level = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL]);

		if (level < QCA_VENDOR_EXT_MON_FILTER_LEVEL_MSDU ||
		    level > QCA_VENDOR_EXT_MON_FILTER_LEVEL_PPDU) {
			ath12k_err(NULL, "invalid filter level %u\n", level);
			return -EINVAL;
		}
		filter->level = level;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE])
		filter->disable =
			nla_get_flag(tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE]);

	if (filter->disable)
		return 0;

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER]) {
		ret = ath12k_ext_mon_extract_pkt_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER],
			&filter->all_peer, true);
		if (ret)
			return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR]) {
		ret = ath12k_ext_mon_extract_pkt_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR],
			&filter->all_neighbor, false);
		if (ret)
			return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER]) {
		ret = ath12k_ext_mon_extract_pkt_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER],
			&filter->target_peer, true);
		if (ret)
			return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR]) {
		ret = ath12k_ext_mon_extract_pkt_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR],
			&filter->target_neighbor, false);
		if (ret)
			return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA]) {
		u8 meta_data = nla_get_u8(
				tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA]);

		if (meta_data & ~ATH12K_EXT_MON_METADATA_VALID_MASK) {
			ath12k_err(NULL, "invalid metadata mask %u\n", meta_data);
			return -EINVAL;
		}
		filter->meta_data = meta_data;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MON_FLAG]) {
		u8 mon_flag =
			nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MON_FLAG]);

		if (mon_flag >= ATH12K_EXT_MON_MAX) {
			ath12k_err(NULL, "invalid monitor flag %u\n", mon_flag);
			return -EINVAL;
		}
		filter->monitor_flags = mon_flag;
	}

	return 0;
}

static int
ath12k_ext_mon_extract_peer_info(struct nlattr *peer_info_attr,
				 struct ath12k_ext_mon_peer_config *peer)
{
	int ret, rem, i = 0;
	struct nlattr *attr;

	nla_for_each_nested(attr, peer_info_attr, rem) {
		struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAX + 1] = { 0 };

		if (i >= peer->count)
			break;

		ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAX,
				       attr, ath12k_vendor_ext_mon_peer_info_policy,
				       NULL);
		if (ret)
			return ret;

		if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR] &&
		    (nla_len(tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR]) == ETH_ALEN))
			nla_memcpy(peer->peer_info[i].mac_addr,
				   tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR],
				   ETH_ALEN);
		else {
			ath12k_err(NULL, "invalid MAC address\n");
			return -EINVAL;
		}

		if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA])
			peer->peer_info[i].ra_addr = nla_get_flag(
					tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA]);

		if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP])
			peer->peer_info[i].ta_info.bitmap =
				nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP]);

		i++;
	}

	return 0;
}

static int
ath12k_ext_mon_extract_peer_config(struct nlattr *peer_attr,
				   struct ath12k_ext_mon_peer_config *peer)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_PEER_MAX + 1] = {0};
	int ret = 0;

	ret = nla_parse_nested(tb, QCA_VENDOR_ATTR_EXT_MON_PEER_MAX,
			       peer_attr, ath12k_vendor_ext_mon_peer_config_policy,
			       NULL);

	if (ret) {
		ath12k_err(NULL, "failed to parse peer config attributes: %d\n", ret);
		return ret;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_ACTION]) {
		u8 action = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PEER_ACTION]);

		if (action < QCA_VENDOR_EXT_MON_PEER_ACTION_ADD ||
		    action > QCA_VENDOR_EXT_MON_PEER_ACTION_REMOVE) {
			ath12k_err(NULL, "invalid peer action %u\n", action);
			return -EINVAL;
		}
		peer->action = action;
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT]) {
		u8 count = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT]);

		if (count > ATH12K_EXT_MON_MAX_PEERS) {
			ath12k_err(NULL, "invalid peer count %d\n", count);
			return -EINVAL;
		}
		peer->count = count;

		if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO]) {
			ret = ath12k_ext_mon_extract_peer_info(
					tb[QCA_VENDOR_ATTR_EXT_MON_PEER_INFO],
					peer);
			if (ret)
				return ret;
		} else
			return -EINVAL;
	}

	return 0;
}

static int
ath12k_ext_mon_parse_request(struct nlattr **tb,
			     struct ath12k_ext_mon_config *req)
{
	int ret = 0;
	u8 cmd_type, direction;

	if (!tb[QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE]) {
		ath12k_err(NULL, "missing mandatory cmd_type attribute\n");
		return -EINVAL;
	}

	cmd_type = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE]);
	if (cmd_type < QCA_VENDOR_EXT_MON_CMD_TYPE_SET_FILTER ||
	    cmd_type > QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER) {
		ath12k_err(NULL, "invalid cmd_type %u\n", cmd_type);
		return -EINVAL;
	}
	req->cmd_type = cmd_type;

	if (!tb[QCA_VENDOR_ATTR_EXT_MON_DIRECTION]) {
		ath12k_err(NULL, "missing mandatory direction attribute\n");
		return -EINVAL;
	}

	direction = nla_get_u8(tb[QCA_VENDOR_ATTR_EXT_MON_DIRECTION]);
	if (direction < QCA_VENDOR_EXT_MON_DIRECTION_RX ||
	    direction > QCA_VENDOR_EXT_MON_DIRECTION_TX) {
		ath12k_err(NULL, "invalid direction %u\n", direction);
		return -EINVAL;
	}
	req->direction = direction;

	if (req->cmd_type == QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER ||
	    req->cmd_type == QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER)
		return 0;

	if (tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG]) {
		ret = ath12k_ext_mon_extract_filter_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG],
			&req->filter);
		if (ret) {
			ath12k_err(NULL, "failed to parse filter config attributes: %d\n",
				   ret);
			return ret;
		}
	}

	if (tb[QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG]) {
		ret = ath12k_ext_mon_extract_peer_config(
			tb[QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG],
			&req->peer);
		if (ret) {
			ath12k_err(NULL, "failed to parse peer config attributes: %d\n",
				   ret);
			return ret;
		}
	}

	return 0;
}

static int
ath12k_ext_mon_get_filter_attr_len(void)
{
	int len = 0;

	len = nla_total_size(sizeof(u32));
	len += nla_total_size(sizeof(u32));
	len += nla_total_size(sizeof(u32));

	return len;
}

static int
ath12k_ext_mon_get_len_attr_len(void)
{
	int len = 0;

	len = nla_total_size(sizeof(u8));
	len += nla_total_size(sizeof(u8));
	len += nla_total_size(sizeof(u8));

	return len;
}

static int
ath12k_ext_mon_get_pkt_attr_len(void)
{
	int len = 0;
	int payload = 0;

	payload = ath12k_ext_mon_get_filter_attr_len();
	len += nla_total_size(payload);

	payload = ath12k_ext_mon_get_len_attr_len();
	len += nla_total_size(payload);

	len += nla_total_size(3 * nla_total_size(sizeof(u8)));
	return len;
}

static int
ath12k_ext_mon_get_filter_config_attr_len(void)
{
	int len = 0;
	int payload = 0;

	len = nla_total_size(sizeof(u8));
	len += nla_total_size(0);

	payload = ath12k_ext_mon_get_pkt_attr_len();
	len += nla_total_size(payload);

	payload = ath12k_ext_mon_get_pkt_attr_len();
	len += nla_total_size(payload);

	payload = ath12k_ext_mon_get_pkt_attr_len();
	len += nla_total_size(payload);

	payload = ath12k_ext_mon_get_pkt_attr_len();
	len += nla_total_size(payload);

	len += nla_total_size(sizeof(u8));

	len += nla_total_size(sizeof(u8));

	return len;
}

static int
ath12k_ext_mon_get_snr_info_attr_len(void)
{
	int len = 0;

	len = nla_total_size(sizeof(u8));
	len += nla_total_size(sizeof(u8));
	len += nla_total_size(sizeof(u64));

	return len;
}

static int
ath12k_ext_mon_get_peer_info_attr_len(void)
{
	int len = 0;
	int payload = 0;

	len += nla_total_size(ETH_ALEN);
	len += nla_total_size(0);
	len += nla_total_size(sizeof(u8));
	payload = ath12k_ext_mon_get_snr_info_attr_len();
	len += nla_total_size(payload);

	return len;
}

static int
ath12k_ext_mon_get_peer_config_attr_len(const struct ath12k_ext_mon_config *resp)
{
	int len = 0;
	int payload = 0;
	int i = 0;

	len += nla_total_size(sizeof(u8));
	len += nla_total_size(sizeof(u8));

	for (i = 0; i < resp->peer.count; i++) {
		payload = ath12k_ext_mon_get_peer_info_attr_len();
		len += nla_total_size(payload);
	}

	return len;
}

static int
ath12k_ext_mon_calculate_resp_len(const struct ath12k_ext_mon_config *req,
				  const struct ath12k_ext_mon_config *resp)
{
	int total_len = 0;
	int payload = 0;

	total_len = nla_total_size(sizeof(u8));
	total_len += nla_total_size(sizeof(u8));
	total_len += nla_total_size(sizeof(u8));

	switch (req->cmd_type) {
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER:
		payload = ath12k_ext_mon_get_filter_config_attr_len();
		total_len += nla_total_size(payload);
		break;
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER:
		payload = ath12k_ext_mon_get_peer_config_attr_len(resp);
		total_len += nla_total_size(payload);
		break;
	default:
		break;
	}

	return total_len;
}

static int
ath12k_ext_mon_put_filter(struct sk_buff *skb,
			  const struct ath12k_ext_mon_pkt_config *pkt)
{
	struct nlattr *attr;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put_u32(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_MGMT,
			pkt->filter[ATH12K_EXT_MON_FRAME_MGMT]) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_CTRL,
			pkt->filter[ATH12K_EXT_MON_FRAME_CTRL]) ||
	    nla_put_u32(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_FILTER_DATA,
			pkt->filter[ATH12K_EXT_MON_FRAME_DATA])) {
		nla_nest_cancel(skb, attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, attr);

	return 0;
}

static int
ath12k_ext_mon_put_filter_len(struct sk_buff *skb,
			      const struct ath12k_ext_mon_pkt_config *pkt)
{
	struct nlattr *attr;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_MGMT,
		       pkt->len[ATH12K_EXT_MON_FRAME_MGMT]) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_CTRL,
		       pkt->len[ATH12K_EXT_MON_FRAME_CTRL]) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_LEN_DATA,
		       pkt->len[ATH12K_EXT_MON_FRAME_DATA])) {
		nla_nest_cancel(skb, attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, attr);

	return 0;
}

static int
ath12k_ext_mon_put_data_mpdu_tlv(struct sk_buff *skb,
				 const struct ath12k_ext_mon_data_mpdu_tlv_config *tlv)
{
	struct nlattr *attr;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PKT_CONFIG_DATA_MPDU_TLV);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_MCAST,
		       tlv->mcast) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_UCAST,
		       tlv->ucast) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_DATA_MPDU_TLV_NULL,
		       tlv->null_frm)) {
		nla_nest_cancel(skb, attr);
		return -EMSGSIZE;
	}
	nla_nest_end(skb, attr);
	return 0;
}

static int
ath12k_ext_mon_put_pkt_config(struct sk_buff *skb, int attrtype,
			      const struct ath12k_ext_mon_pkt_config *pkt,
			      bool is_peer_filter)
{
	struct nlattr *attr;
	int ret;

	attr = nla_nest_start(skb, attrtype);
	if (!attr)
		return -EMSGSIZE;

	ret = ath12k_ext_mon_put_filter(skb, pkt);
	if (ret)
		goto err;

	ret = ath12k_ext_mon_put_filter_len(skb, pkt);
	if (ret)
		goto err;

	if (is_peer_filter && pkt->data_mpdu_tlv.tlv_configured) {
		ret = ath12k_ext_mon_put_data_mpdu_tlv(skb, &pkt->data_mpdu_tlv);
		if (ret)
			goto err;
	}

	nla_nest_end(skb, attr);
	return 0;

err:
	nla_nest_cancel(skb, attr);
	return ret;
}

static int
ath12k_ext_mon_put_filter_config(struct sk_buff *skb,
				 const struct ath12k_ext_mon_filter_config *filter)
{
	struct nlattr *attr;
	int ret;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG);
	if (!attr)
		return -EMSGSIZE;

	if (filter->disable) {
		if (nla_put_flag(skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_DISABLE))
			goto err;
		nla_nest_end(skb, attr);
		return 0;
	}

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_LEVEL,
		       filter->level))
		goto err;

	ret = ath12k_ext_mon_put_pkt_config(
		skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_PEER,
		&filter->all_peer, true);
	if (ret)
		goto err;

	ret = ath12k_ext_mon_put_pkt_config(
		skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_ALL_NEIGHBOR,
		&filter->all_neighbor, false);
	if (ret)
		goto err;

	ret = ath12k_ext_mon_put_pkt_config(
		skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_PEER,
		&filter->target_peer, true);
	if (ret)
		goto err;

	ret = ath12k_ext_mon_put_pkt_config(
		skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_TARGET_NEIGHBOR,
		&filter->target_neighbor, false);
	if (ret)
		goto err;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_META_DATA,
		       filter->meta_data))
		goto err;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_FILTER_CONFIG_MON_FLAG,
		       filter->monitor_flags))
		goto err;

	nla_nest_end(skb, attr);
	return 0;

err:
	nla_nest_cancel(skb, attr);
	return -EMSGSIZE;
}

static int
ath12k_ext_mon_put_snr_info(struct sk_buff *skb,
			    const struct ath12k_ext_mon_snr_info *snr_info)
{
	struct nlattr *attr;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_SNR_INFO);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_SNR,
		       snr_info->snr) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_AVG_SNR,
		       snr_info->avg_snr) ||
	    nla_put_u64_64bit(skb, QCA_VENDOR_ATTR_EXT_MON_SNR_INFO_TSTAMP,
			      snr_info->timestamp, NL80211_ATTR_PAD)) {
		nla_nest_cancel(skb, attr);
		return -EMSGSIZE;
	}

	nla_nest_end(skb, attr);
	return 0;
}

static int
ath12k_ext_mon_put_peer_info(struct sk_buff *skb, int idx,
			     const struct ath12k_ext_mon_peer_info *peer_info)
{
	struct nlattr *attr;
	int ret;

	attr = nla_nest_start(skb, idx);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_MAC_ADDR,
		    ETH_ALEN, peer_info->mac_addr))
		goto err;

	if (peer_info->ra_addr &&
	    nla_put_flag(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_ADDR_IS_RA))
		goto err;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO_BITMAP,
		       peer_info->ta_info.bitmap))
		goto err;

	ret = ath12k_ext_mon_put_snr_info(skb, &peer_info->ta_info.snr_info);
	if (ret)
		goto err;

	nla_nest_end(skb, attr);
	return 0;

err:
	nla_nest_cancel(skb, attr);
	return -EMSGSIZE;
}

static int
ath12k_ext_mon_put_peer_config(struct sk_buff *skb,
			       const struct ath12k_ext_mon_peer_config *peer)
{
	struct nlattr *attr;
	struct nlattr *peer_info_attr;
	int ret;
	int i;

	attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_CONFIG);
	if (!attr)
		return -EMSGSIZE;

	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_COUNT, peer->count))
		goto err;

	peer_info_attr = nla_nest_start(skb, QCA_VENDOR_ATTR_EXT_MON_PEER_INFO);
	if (!peer_info_attr)
		goto err;

	for (i = 0; i < peer->count; i++) {
		ret = ath12k_ext_mon_put_peer_info(skb, i, &peer->peer_info[i]);
		if (ret)
			goto err_peer_info;
	}

	nla_nest_end(skb, peer_info_attr);
	nla_nest_end(skb, attr);
	return 0;

err_peer_info:
	nla_nest_cancel(skb, peer_info_attr);
err:
	nla_nest_cancel(skb, attr);
	return -EMSGSIZE;
}

static int
ath12k_ext_mon_put_response(struct sk_buff *skb,
			    const struct ath12k_ext_mon_config *req,
			    const struct ath12k_ext_mon_config *resp)
{
	if (nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_CMD_TYPE, req->cmd_type) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_DIRECTION, req->direction) ||
	    nla_put_u8(skb, QCA_VENDOR_ATTR_EXT_MON_STATUS_CODE, resp->status_code))
		return -EMSGSIZE;

	if (resp->status_code != ATH12K_EXT_MON_SUCCESS)
		return 0;

	switch (req->cmd_type) {
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_FILTER:
		return ath12k_ext_mon_put_filter_config(skb, &resp->filter);
	case QCA_VENDOR_EXT_MON_CMD_TYPE_GET_PEER:
		return ath12k_ext_mon_put_peer_config(skb, &resp->peer);
	default:
		return 0;
	}
}

static int
ath12k_ext_mon_handle_request(struct wiphy *wiphy,
			      struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_ext_mon_config *req)
{
	struct sk_buff *skb;
	int resp_len = 0;
	struct ath12k_ext_mon_config *resp;
	int ret;

	resp = vzalloc(sizeof(*resp));
	if (!resp)
		return -ENOMEM;

	ath12k_dp_ext_mon_process_request(dp_pdev, req, resp);

	resp_len = ath12k_ext_mon_calculate_resp_len(req, resp);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, resp_len);
	if (!skb) {
		vfree(resp);
		return -ENOMEM;
	}

	ret = ath12k_ext_mon_put_response(skb, req, resp);
	if (ret) {
		vfree(resp);
		goto nla_put_failure;
	}

	vfree(resp);
	return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EMSGSIZE;
}

static int
ath12k_vendor_extended_monitor_handler(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	struct nlattr *tb[QCA_VENDOR_ATTR_EXT_MON_MAX + 1];
	struct ath12k_ext_mon_config *req;
	struct ath12k *ar = NULL;
	struct ath12k_vif *ahvif = NULL;
	int ret;

	if (wdev->iftype != NL80211_IFTYPE_MONITOR) {
		ath12k_err(NULL, "requested interface is not a monitor interface!\n");
		return -EINVAL;
	}

	ahvif = ath12k_get_ahvif_from_wdev(wdev);
	if (!ahvif) {
		ath12k_err(NULL, "ahvif not present");
		return -EINVAL;
	}

	ar = ahvif->deflink.ar;
	if (!ar) {
		ath12k_err(NULL, "ar not found for monitor interface\n");
		return -EINVAL;
	}

	ret = nla_parse(tb, QCA_VENDOR_ATTR_EXT_MON_MAX, data, data_len,
			ath12k_vendor_ext_mon_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ext mon attributes: %d\n", ret);
		return ret;
	}

	req = vzalloc(sizeof(*req));
	if (!req)
		return -ENOMEM;

	ret = ath12k_ext_mon_parse_request(tb, req);
	if (ret) {
		vfree(req);
		ath12k_err(NULL, "error parsing ext mon user input: %d\n", ret);
		return ret;
	}

	ret = ath12k_ext_mon_handle_request(wiphy, &ar->dp, req);
	if (ret) {
		vfree(req);
		ath12k_err(NULL, "error in handling ext mon request: %d\n", ret);
		return ret;
	}

	vfree(req);
	return 0;
}

static int ath12k_vendor_set_pcp_tid_map(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_hw	*hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw	*ah = hw->priv;
	struct ath12k_hw_group	*ag = ah->ag;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX + 1];
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	struct nlattr *entry;
	int ret, rem;
	u8 pcp, tid;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX,
			data, data_len, ath12k_pcp_tid_map_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "pcp_tid_map: nla_parse failed: %d\n", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE]) {
		ath12k_err(NULL, "pcp_tid_map: missing TABLE attribute\n");
		return -EINVAL;
	}

	/* Iterate nested {PCP, TID} entries — partial update: only the PCPs
	 * present in this message are modified; others retain current values.
	 */
	nla_for_each_nested(entry,
			    tb[QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE], rem) {
		struct nlattr *etb[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_MAX + 1];

		ret = nla_parse_nested(etb,
				       QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_MAX,
				       entry, ath12k_pcp_tid_entry_policy, NULL);
		if (ret) {
			ath12k_err(NULL,
				   "pcp_tid_map: nested entry parse failed: %d\n",
					ret);
			return -EINVAL;
		}

		if (!etb[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP] ||
		    !etb[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID]) {
			ath12k_err(NULL,
				   "pcp_tid_map: entry missing PCP or TID attr\n");
			return -EINVAL;
		}

		pcp = nla_get_u8(etb[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP]);
		tid = nla_get_u8(etb[QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID]);

		if (pcp > ATH12K_MAX_PCP || tid > ATH12K_MAX_TID) {
			ath12k_err(NULL,
				   "pcp_tid_map: PCP=%u TID=%u out of range (0-7)\n",
					pcp, tid);
			return -EINVAL;
		}

		/* Partial update: only this PCP slot is modified */
		dp_hw_grp->pcp_tid_map[pcp] = tid;
	}

	/* Program all SOC TCL registers via the wrapper API (sanity checks
	 * are performed inside ath12k_dp_pcp_tid_map before HAL call)
	 */
	return ath12k_dp_pcp_tid_map(dp_hw_grp);
}

static int ath12k_vendor_get_pcp_tid_map(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_hw       *hw        = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw          *ah        = hw->priv;
	struct ath12k_hw_group    *ag       = ah->ag;
	struct ath12k_dp_hw_group *dp_hw_grp = ag->dp_hw_grp;
	struct sk_buff *skb;
	int i;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	/* Build NLA_NESTED reply to match what the userspace
	 * pcp_tid_map_get_cb() uses nla_for_each_nested(entry, TABLE)
	 * then nla_parse_nested(etb, entry) to extract PCP and TID from
	 * each entry — so each entry must itself be a nested attribute.
	 */
	struct nlattr *table_nest, *entry_nest;

	table_nest = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_TABLE);
	if (!table_nest)
		goto fail;

	for (i = 0; i < ATH12K_DP_PCP_TID_MAP_SIZE; i++) {
		/* Each {PCP, TID} pair wrapped in its own nested attr */
		entry_nest = nla_nest_start(skb, i);
		if (!entry_nest)
			goto fail;

		if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_PCP, (u8)i) ||
		    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_PCP_TID_ENTRY_TID,
			       dp_hw_grp->pcp_tid_map[i]))
			goto fail;

		nla_nest_end(skb, entry_nest);
	}
	nla_nest_end(skb, table_nest);

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

static int ath12k_vendor_set_tid_map_precedence(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct ieee80211_hw	*hw	= wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw	*ah	= hw->priv;
	struct ath12k_hw_group	*ag	= ah->ag;
	struct ath12k_dp_hw_group *dp_hw_grp	= ag->dp_hw_grp;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX + 1];
	u8 prec_val;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX,
			data, data_len, ath12k_tid_map_prty_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "tid_map_prty: nla_parse failed: %d\n", ret);
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL]) {
		ath12k_err(NULL, "tid_map_prty: missing VAL attribute\n");
		return -EINVAL;
	}

	prec_val = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL]);
	if (prec_val > ATH12K_DP_MAX_TID_PRECEDENCE_VAL) {
		ath12k_err(NULL, "tid_map_prty: VAL=%u out of range (0=DSCP, 1=PCP)\n",
			   prec_val);
		return -EINVAL;
	}

	/* Store in dp_hw_grp (ath12k-private). Do NOT write to wiphy. */
	dp_hw_grp->tid_map_precedence = prec_val;

	/* Step 3: Program all SOC TCL registers (reads from dp_hw_grp) */
	return ath12k_dp_tid_map_precedence(dp_hw_grp);
}

static int ath12k_vendor_get_tid_map_precedence(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct ieee80211_hw       *hw        = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw          *ah        = hw->priv;
	struct ath12k_hw_group    *ahg       = ah->ag;
	struct ath12k_dp_hw_group *dp_hw_grp = ahg->dp_hw_grp;
	struct sk_buff *skb;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_VAL,
		       dp_hw_grp->tid_map_precedence))
		goto fail;

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

static struct ath12k_link_sta *
ath12k_vendor_get_primary_link_arsta(struct wiphy *wiphy,
				     struct ath12k_sta *ahsta)
{
	struct ath12k_link_sta *arsta;

	lockdep_assert_wiphy(wiphy);

	if (!ahsta->is_mlo)
		return &ahsta->deflink;

	if (ahsta->primary_link_id >= ATH12K_NUM_MAX_LINKS ||
	    !(ahsta->links_map & BIT(ahsta->primary_link_id)))
		return &ahsta->deflink;

	arsta = wiphy_dereference(wiphy, ahsta->link[ahsta->primary_link_id]);
	if (!arsta)
		return &ahsta->deflink;

	return arsta;
}

static int
ath12k_vendor_get_data_link_peer_rssi(struct ath12k_dp_peer *dp_peer, u8 link_id,
				      s8 *min_rssi, s8 *max_rssi)
{
	struct ath12k_dp_link_peer *link_peer;
	u8 link_peer_mac[ETH_ALEN];

	rcu_read_lock();

	link_peer = ath12k_dp_link_peer_find_by_logical_link_id(dp_peer, link_id);
	if (!link_peer) {
		rcu_read_unlock();
		return -ENOENT;
	}

	ether_addr_copy(link_peer_mac, link_peer->addr);

	rcu_read_unlock();

	return ath12k_dp_mon_get_link_peer_rssi(dp_peer, link_peer_mac,
						min_rssi, max_rssi);
}

static int ath12k_vendor_get_sta_info_dumpit(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     struct sk_buff *skb,
					     const void *data,
					     int data_len,
					     unsigned long *storage)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX + 1];
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_vif *ahvif;
	struct ath12k_link_sta *primary_arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_peer *dp_peer;
	unsigned long links_map = 0;
	const u8 *peer_mac;
	u8 link_id = 0;
	s8 min_rssi, max_rssi;
	s8 data_min_rssi, data_max_rssi;
	u8 ps_state;
	int ret, cur_link_id;

	lockdep_assert_wiphy(wiphy);

	if (!vif)
		return -EINVAL;

	ahvif = ath12k_vif_to_ahvif(vif);

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX, data, data_len,
			ath12k_vendor_get_sta_info_policy, NULL);
	if (ret)
		return ret;

	if (*storage == 1)
		return 0;

	if (!tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC] ||
	    nla_len(tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC]) != ETH_ALEN)
		return -EINVAL;

	peer_mac = nla_data(tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC]);

	if (wdev->valid_links) {
		if (!tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LINK_ID])
			return -EINVAL;

		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LINK_ID]);
		if (link_id != INVALID_LINK_ID &&
		    (link_id >= ATH12K_NUM_MAX_LINKS ||
		     !(wdev->valid_links & BIT(link_id))))
			return -ENOLINK;
	} else {
		link_id = 0;
	}

	sta = ieee80211_find_sta_by_ifaddr(ahvif->ah->hw, peer_mac, NULL);
	if (!sta)
		return -ENOENT;

	ahsta = ath12k_sta_to_ahsta(sta);
	if (!ahsta)
		return -ENOENT;
	if (ahsta->ahvif != ahvif)
		return -ENOENT;

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(wiphy, ahsta);
	if (!dp_peer)
		return -ENOENT;

	primary_arsta = ath12k_vendor_get_primary_link_arsta(wiphy, ahsta);
	if (!primary_arsta || primary_arsta->is_self_peer)
		return -ENOENT;

	max_rssi = primary_arsta->max_rssi;
	min_rssi = primary_arsta->min_rssi;
	ps_state = primary_arsta->peer_ps_state;

	if (link_id == INVALID_LINK_ID) {
		if (ahsta->is_mlo)
			links_map = ahsta->links_map;
		else if (ahsta->deflink.link_id < ATH12K_NUM_MAX_LINKS)
			links_map = BIT(ahsta->deflink.link_id);

		for_each_set_bit(cur_link_id, &links_map, ATH12K_NUM_MAX_LINKS) {
			ret = ath12k_vendor_get_data_link_peer_rssi(dp_peer, cur_link_id,
								    &data_min_rssi,
								    &data_max_rssi);
			if (ret)
				continue;

			min_rssi = min(min_rssi, data_min_rssi);
			max_rssi = max(max_rssi, data_max_rssi);
		}
	} else {
		ret = ath12k_vendor_get_data_link_peer_rssi(dp_peer, link_id,
							    &data_min_rssi,
							    &data_max_rssi);
		min_rssi = min(min_rssi, data_min_rssi);
		max_rssi = max(max_rssi, data_max_rssi);
	}

	if (nla_put_s8(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX_RSSI, max_rssi) ||
	    nla_put_s8(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MIN_RSSI, min_rssi) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_PS_STATE, ps_state))
		return -ENOBUFS;

	*storage += 1;
	return skb->len;
}

int
ath12k_vendor_send_es_oem_data(struct ieee80211_hw *hw, u8 radio_id, u32 content_type,
			       u32 num_bytes_valid, const u8 *data)
{
	struct sk_buff *vendor_event = NULL;
	struct oem_vendor_build *packaged = NULL;
	int vendor_buffer_len, nla_vendor_len;
	int ret = -EINVAL;

	/* Length used to allocate temp buffer to pack content type +
	 * num_bytes_valid + payload
	 */
	vendor_buffer_len = sizeof(struct oem_vendor_build) + num_bytes_valid;

	/* Length used to allocate nl event */
	nla_vendor_len = nla_total_size(vendor_buffer_len);

	vendor_event = cfg80211_vendor_event_alloc(hw->wiphy, NULL, nla_vendor_len,
						 QCA_NL80211_VENDOR_SUBCMD_OEM_DATA_INDEX,
						   GFP_ATOMIC);
	if (!vendor_event)
		return -ENOMEM;

	packaged = kmalloc(vendor_buffer_len, GFP_ATOMIC);
	if (!packaged) {
		ret = -ENOMEM;
		goto out;
	}

	packaged->l_radio_id = radio_id;
	packaged->l_content_type = cpu_to_le32(content_type);
	packaged->l_num_bytes_valid = cpu_to_le32(num_bytes_valid);
	memcpy(packaged->l_data, data, num_bytes_valid);

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA,
		    vendor_buffer_len, packaged)) {
		ret = -EMSGSIZE;
		goto out;
	}

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	ret = 0;

out:
	kfree(packaged);
	/* freeing skb is handled by callee (cfg80211_vendor_event)*/
	if (!ret)
		return ret;

	if (vendor_event)
		kfree_skb(vendor_event);
	return ret;
}

static int ath12k_vendor_oem_data(struct wiphy *wiphy, struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX + 1];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k *ar;
	const struct oem_vendor_build *packaged;
	u32 content_type, num_bytes_valid;
	u8 *data_ptr, radio_id;
#ifdef CPTCFG_QCN_EXTN
	u8 device_info;
#endif
	int ret, payload_len;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX,
			(struct nlattr *)data, data_len,
			ath12k_oem_data_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "Failed to parse OEM Data attributes: %d\n", ret);
		return ret;
	}

#ifdef CPTCFG_QCN_EXTN
	if (tb[QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO]) {
		device_info = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_OEM_DEVICE_INFO]);
		if (device_info == QCA_VENDOR_OEM_DEVICE_RANGING) {
			ret = ath12k_vendor_oem_data_lowi_extn(wiphy, wdev, hw,
							       ah, tb);
			if (ret != -EAGAIN)
				return ret;
		}
	}
#endif /* CPTCFG_QCN_EXTN */

	if (!tb[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA]) {
		ath12k_err(NULL, "Missing OEM Data buffer attribute\n");
		return -EINVAL;
	}

	packaged = nla_data(tb[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA]);
	payload_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA]);
	if (payload_len < sizeof(*packaged)) {
		ath12k_err(NULL, "Invalid OEM Data payload length %d\n", payload_len);
		return -EINVAL;
	}

	radio_id = packaged->l_radio_id;
	content_type = le32_to_cpu(packaged->l_content_type);
	num_bytes_valid = le32_to_cpu(packaged->l_num_bytes_valid);
	if (payload_len - sizeof(*packaged) < num_bytes_valid) {
		ath12k_err(NULL, "OEM Data payload underrun: %u valid bytes, %d payload\n",
			   num_bytes_valid, payload_len);
		return -EINVAL;
	}

	data_ptr = (u8 *)packaged->l_data;

	rcu_read_lock();
	ar = ath12k_ah_to_ar(ah, radio_id);
	rcu_read_unlock();

	if (!ar) {
		ath12k_warn(ar->ab, "invalid ar for Energy Service OEM data\n");
		return -EINVAL;
	}

	ath12k_wmi_send_energy_mgmt_oem_data(ar, content_type, num_bytes_valid,
					     data_ptr);

	return 0;
}

static int
ath12k_vendor_ch_switch_put_chan_base(struct sk_buff *skb,
				      const struct cfg80211_chan_def *chandef,
				      const struct ath12k_vendor_ch_switch_attrs *attrs)
{
	u32 freq;
	u8 width;
	int ret;

	if (!chandef)
		return -EINVAL;

	freq = chandef->chan ? chandef->chan->center_freq : 0;
	width = ath12k_nl_chan_bw_to_qca_vendor_chan_bw(chandef->width);

	if (freq) {
		ret = nla_put_u32(skb, attrs->freq, freq);
		if (ret)
			goto nla_fail;
	}

	if (width) {
		ret = nla_put_u8(skb, attrs->width, width);
		if (ret)
			goto nla_fail;
	}

	return 0;

nla_fail:
	return -EMSGSIZE;
}

static int
ath12k_vendor_ch_switch_put_cur_chan(struct sk_buff *skb,
				     const struct cfg80211_chan_def *chandef)
{
	static const struct ath12k_vendor_ch_switch_attrs attrs = {
		.freq = QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_FREQ,
		.width = QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_WIDTH,
	};
	int ret;

	if (!chandef)
		return -EINVAL;

	ret = ath12k_vendor_ch_switch_put_chan_base(skb, chandef, &attrs);
	if (ret)
		return ret;

	if (chandef->center_freq1) {
		ret = nla_put_u32(skb,
				  QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ1,
				  chandef->center_freq1);
		if (ret)
			goto nla_fail;
	}

	if (chandef->center_freq2) {
		ret = nla_put_u32(skb,
				  QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_CENTER_FREQ2,
				  chandef->center_freq2);
		if (ret)
			goto nla_fail;
	}

	if (chandef->punctured) {
		ret = nla_put_u16(skb,
				  QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_PUNCTURED,
				  chandef->punctured);
		if (ret)
			goto nla_fail;
	}

	if (chandef->radar_bitmap) {
		ret = nla_put_u16(skb,
				  QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CUR_RADAR_BITMAP,
				  chandef->radar_bitmap);
		if (ret)
			goto nla_fail;
	}

	return 0;

nla_fail:
	return -EMSGSIZE;
}

static int
ath12k_vendor_ch_switch_put_new_chan(struct sk_buff *skb,
				     const struct cfg80211_chan_def *chandef)
{
	static const struct ath12k_vendor_ch_switch_attrs attrs = {
		.freq = QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_FREQ,
		.width = QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_NEW_WIDTH,
	};

	return ath12k_vendor_ch_switch_put_chan_base(skb, chandef, &attrs);
}

/**
 * ath12k_vendor_ch_switch_reason_notify() - Send channel switch reason event
 * @ar: ath12k device pointer
 * @reason: Reason for the channel switch, enum qca_wlan_vendor_ch_switch_reason
 * @old: Current/old channel definition (may be NULL); all cfg80211_chan_def
 *       fields are sent to userspace when non-NULL
 * @new: New channel definition (may be NULL)
 *
 * Sends a vendor event to userspace with the reason for an upcoming channel
 * switch together with the complete cfg80211_chan_def of the current/old
 * channel (primary freq, width, center_freq1, center_freq2, punctured bitmap)
 * and the new channel's primary frequency and width.
 *
 * This is called before the actual channel change is applied so that userspace
 * can track the reason and fully reconstruct the old channel configuration.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_vendor_ch_switch_reason_notify(struct ath12k *ar,
					  enum qca_wlan_vendor_ch_switch_reason reason,
					  const struct cfg80211_chan_def *old_chandef,
					  const struct cfg80211_chan_def *new_chandef)
{
	struct wireless_dev *wdev;
	struct wiphy *wiphy;
	struct sk_buff *skb;
	struct ath12k_link_vif *arvif;
	u32 old_freq = 0, new_freq = 0;
	int event_idx;
	int ret;

	if (!ar || !ar->ab || !ar->ah || !ar->ah->hw)
		return -EINVAL;

	wiphy = ath12k_ar_to_hw(ar)->wiphy;
	arvif = ath12k_vendor_get_non_scan_arvif(ar);
	if (!arvif || !arvif->ahvif || !arvif->ahvif->vif)
		return -ENODEV;

	wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
	if (!wdev)
		return -ENODEV;

	event_idx = QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON_INDEX;
	skb = cfg80211_vendor_event_alloc(wiphy, wdev, NLMSG_DEFAULT_SIZE,
					  event_idx, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	ret = nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_CH_SWITCH_REASON_CODE, reason);
	if (ret)
		goto nla_fail;

	ret = ath12k_vendor_ch_switch_put_cur_chan(skb, old_chandef);
	if (ret)
		goto nla_fail;

	ret = ath12k_vendor_ch_switch_put_new_chan(skb, new_chandef);
	if (ret)
		goto nla_fail;

	if (old_chandef && old_chandef->chan)
		old_freq = old_chandef->chan->center_freq;

	if (new_chandef && new_chandef->chan)
		new_freq = new_chandef->chan->center_freq;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			 "vendor ch switch event reason %d old %u/%d center %u/%u punct 0x%x radar 0x%x new %u/%d\n",
			 reason, old_freq, old_chandef ? old_chandef->width : 0,
			 old_chandef ? old_chandef->center_freq1 : 0,
			 old_chandef ? old_chandef->center_freq2 : 0,
			 old_chandef ? old_chandef->punctured : 0,
			 old_chandef ? old_chandef->radar_bitmap : 0,
			 new_freq,
			 new_chandef ? new_chandef->width : 0);

	cfg80211_vendor_event(skb, GFP_ATOMIC);
	return 0;

nla_fail:
	ath12k_warn(ar->ab,
		    "failed to build channel switch reason vendor event reason %d ret %d\n",
		    reason, ret);
	kfree_skb(skb);
	return ret;
}

void ath12k_vendor_event_chain_mask_changed(struct ath12k *ar, int ifindex)
{
	struct sk_buff *event;
	int hw_idx;
	int len;

	len = nla_total_size(sizeof(u32));
	if (ifindex > 0)
		len += nla_total_size(sizeof(u32));

	event = cfg80211_vendor_event_alloc(ar->ah->hw->wiphy, NULL, len,
					    QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_INDEX,
					    GFP_ATOMIC);
	if (!event) {
		ath12k_warn(ar->ab,
			    "failed to alloc skb for dynamic chain mask vendor event\n");
		return;
	}

	hw_idx = cfg80211_get_hw_idx_by_freq(ar->ah->hw->wiphy,
					     ar->freq_range.start_freq);
	if (hw_idx < 0) {
		ath12k_warn(ar->ab,
			    "failed to get hw_idx for dynamic chain mask event freq %u\n",
			    ar->freq_range.start_freq);
		kfree_skb(event);
		return;
	}

	if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_CHAIN_MASK_EVENT_HW_IDX,
			hw_idx)) {
		ath12k_warn(ar->ab,
			    "failed to build dynamic chain mask vendor event for hw_idx %d\n",
			    hw_idx);
		kfree_skb(event);
		return;
	}

	if (ifindex > 0 &&
	    nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_CHAIN_MASK_EVENT_IFINDEX,
			ifindex)) {
		ath12k_warn(ar->ab,
			    "failed to build dynamic chain mask vendor event for ifindex %d\n",
			    ifindex);
		kfree_skb(event);
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "sending dynamic chain mask vendor event for hw_idx %d ifindex %d\n",
		   hw_idx, ifindex);
	cfg80211_vendor_event(event, GFP_ATOMIC);
}

/* ---- Spectral scan vendor command handlers ---- */

#ifdef CPTCFG_ATH12K_SPECTRAL

#define SPECTRAL_SCALING_LOW_LEVEL_OFFSET	7
#define SPECTRAL_SCALING_HIGH_LEVEL_OFFSET	5
#define SPECTRAL_SCALING_RSSI_THRESH		5
#define SPECTRAL_IPQ8074_DEFAULT_MAX_GAIN	62
#define ATH12K_SPECTRAL_RECAPTURE_SCAN_PERIOD_THRESHOLD	52

/* Helpers for parsing spectral scan NL attrs into param fields. */
#define ATTR_U32(id, fptr) do {				\
	const int _a = (id);				\
	if (tb[_a])					\
		*(fptr) = nla_get_u32(tb[_a]);		\
} while (0)
#define ATTR_U8(id, fptr) do {				\
	const int _a = (id);				\
	if (tb[_a])					\
		*(fptr) = nla_get_u8(tb[_a]);		\
} while (0)

#define ATTR_BOOL(id, fptr) do {			\
	const int _a = (id);				\
	if (tb[_a])					\
		*(fptr) = !!nla_get_u32(tb[_a]);	\
} while (0)

static const struct nla_policy
ath12k_spectral_scan_policy[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_RECAPTURE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETION_TIMEOUT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DEBUG_LEVEL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_RING_DEBUG] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DMA_BUFFER_DEBUG] = { .type = NLA_U8 },
};

/* Returns the ath12k radio for a spectral vendor command.
 *
 * Reads NL80211_ATTR_WIPHY_RADIO_INDEX from the outer nl80211 message via
 * rdev->cur_cmd_info->attrs[], maps it to the ar, and verifies at least one
 * vdev is active (required for WMI spectral commands).
 *
 * Must be called with the wiphy lock held.
 */
static struct ath12k *ath12k_spectral_get_ar(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	struct ath12k *ar;
	u8 radio_idx;

	/* cur_cmd_info is set by nl80211_vendor_cmd() before dispatching
	 * and cleared after; it is always non-NULL here, but guard defensively
	 * in case of future call-path changes.
	 */
	if (!rdev->cur_cmd_info ||
	    !rdev->cur_cmd_info->attrs[NL80211_ATTR_WIPHY_RADIO_INDEX]) {
		ath12k_err(NULL,
			   "spectral: NL80211_ATTR_WIPHY_RADIO_INDEX missing in command\n");
		return NULL;
	}

	radio_idx = nla_get_u8(rdev->cur_cmd_info->attrs[NL80211_ATTR_WIPHY_RADIO_INDEX]);

	ar = ath12k_get_radio_by_id(wiphy, radio_idx);
	if (!ar)
		return NULL;

	if (list_empty(&ar->arvifs)) {
		ath12k_warn(ar->ab,
			    "spectral: no active vdev on radio %u, start a vdev first\n",
			    radio_idx);
		return NULL;
	}

	return ar;
}

/* Build and send the SPECTRAL_SCAN_START reply skb with an error code,
 * matching the QCA vendor spec. Always returns -EINVAL so callers can
 * `return ath12k_spectral_scan_start_reply_error(...)`.
 */
static int
ath12k_spectral_scan_start_reply_error(struct wiphy *wiphy,
			enum qca_wlan_vendor_spectral_scan_error_code err_code)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (skb) {
		nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_ERROR_CODE,
			    err_code);
		cfg80211_vendor_cmd_reply(skb);
	}
	return -EINVAL;
}

/* Resolve the BW slot index for FFT-size validation. */
static int
ath12k_spectral_resolve_bw_idx(struct ath12k *ar, struct nlattr **tb)
{
	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]) {
		u8 bw = nla_get_u8(tb[
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]);
		return ath12k_spectral_nl80211_bw_to_idx(bw);
	}

	if (ar->spectral.params.bandwidth)
		return ath12k_spectral_nl80211_bw_to_idx(ar->spectral.params.bandwidth);

	return -1;
}

static int
ath12k_vendor_spectral_validate_agile_cap(struct wiphy *wiphy,
					  struct ath12k *ar)
{
	if (!ath12k_spectral_is_agile_capable(ar)) {
		ath12k_warn(ar->ab, "spectral scan: agile mode not supported\n");
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED);
	}
	return 0;
}

static int
ath12k_vendor_spectral_validate_agile_freq(struct wiphy *wiphy,
					   struct ath12k *ar,
					   struct nlattr **tb,
					   bool has_config)
{
	if (has_config && tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY]) {
		u32 frequency;

		frequency = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY]);
		if (!frequency) {
			ath12k_warn(ar->ab,
				    "spectral scan: agile frequency is zero\n");
			return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED);
		}
	} else if (!has_config && !ar->spectral.params.frequency) {
		/* SCAN-only request: frequency must have been set by a prior
		 * CONFIG command. Reject early rather than letting firmware
		 * run an agile scan at frequency 0.
		 */
		ath12k_warn(ar->ab,
			    "spectral scan: agile frequency not initialized\n");
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2]) {
		ath12k_warn(ar->ab,
			    "spectral scan: agile frequency2 not supported yet\n");
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED);
	}

	if (has_config) {
		u32 scan_count = ar->spectral.params.scan_count;

		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT])
			scan_count = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT]);

		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT] &&
		    !scan_count) {
			ath12k_warn(ar->ab,
				    "spectral scan: agile finite scan_count required\n");
			return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
		}
	}

	return 0;
}

static int
ath12k_vendor_spectral_validate_agile_width(struct wiphy *wiphy,
					    struct ath12k *ar,
					    struct nlattr **tb,
					    bool has_scan)
{
	u8 width;

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]) {
		width = nla_get_u8(tb[
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]);
	} else if (ar->spectral.params.bandwidth) {
		/* bandwidth is zero-initialized and 0 also maps to
		 * NL80211_CHAN_WIDTH_20_NOHT, so cached bandwidth 0 is treated as
		 * unset until a later stage adds explicit config-state tracking.
		 */
		width = ar->spectral.params.bandwidth;
	} else if (has_scan) {
		/* SCAN is being triggered but no bandwidth is set in the request
		 * or cached from a prior CONFIG. Reject with a clear error rather
		 * than letting firmware run an agile scan at an undefined width.
		 */
		ath12k_warn(ar->ab,
			    "spectral scan: agile bandwidth not initialized\n");
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_NOT_INITIALIZED);
	} else {
		/* CONFIG-only request with no bandwidth — accept it. The user
		 * is setting params incrementally; bandwidth will be validated
		 * when a scan is actually triggered.
		 */
		return 0;
	}

	if (width > NL80211_CHAN_WIDTH_320) {
		ath12k_warn(ar->ab,
			    "spectral scan: agile bandwidth %u out of range [0, %u]\n",
			    width, NL80211_CHAN_WIDTH_320);
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
	}

	if (ath12k_spectral_nl80211_bw_to_idx(width) < 0) {
		ath12k_warn(ar->ab,
			    "spectral scan: agile bandwidth %u invalid\n", width);
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
	}

	if (width == NL80211_CHAN_WIDTH_80P80) {
		ath12k_warn(ar->ab,
			    "spectral scan: agile 80+80 MHz not supported yet\n");
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED);
	}

	if (!ath12k_spectral_is_agile_bw_capable(ar, width)) {
		ath12k_warn(ar->ab,
			    "spectral scan: agile bandwidth %u not supported\n", width);
		return ath12k_spectral_scan_start_reply_error(wiphy,
			QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_MODE_UNSUPPORTED);
	}

	return 0;
}

static int
ath12k_vendor_spectral_validate_agile(struct wiphy *wiphy,
				      struct ath12k *ar,
				      struct nlattr **tb,
				      enum qca_wlan_vendor_attr_spectral_scan_request_type
				      req_type)
{
	bool has_config = req_type !=
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN;
	bool has_scan = req_type !=
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG;
	int ret;

	ret = ath12k_vendor_spectral_validate_agile_cap(wiphy, ar);
	if (ret)
		return ret;

	ret = ath12k_vendor_spectral_validate_agile_freq(wiphy, ar, tb, has_config);
	if (ret)
		return ret;

	return ath12k_vendor_spectral_validate_agile_width(wiphy, ar, tb, has_scan);
}

static int ath12k_vendor_spectral_scan_start(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1];
	/* Default: update params AND trigger a scan. */
	enum qca_wlan_vendor_attr_spectral_scan_request_type req_type =
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN_AND_CONFIG;
	enum qca_wlan_vendor_spectral_scan_mode nl_mode =
		QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL;
	struct ath12k *ar;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
			data, data_len, ath12k_spectral_scan_policy, NULL);
	if (ret)
		return ret;
	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE])
		req_type = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE])
		nl_mode = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE]);

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar)
		return -EINVAL;

	if (req_type > QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG)
		return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);

	if (nl_mode > QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE)
		return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);

	if (nl_mode == QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE) {
		ret = ath12k_vendor_spectral_validate_agile(wiphy, ar, tb,
							    req_type);
		if (ret)
			return ret;
		/* Validation passed, continue to shared param-update
		 * and configure/start blocks below.
		 */
	}

	/* Step 1: update scan params in software if request includes CONFIG. */
	if (req_type != QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN) {
		struct ath12k_spectral_params *p = &ar->spectral.params;
		bool active;

		spin_lock_bh(&ar->spectral.lock);
		active = ar->spectral.scan_active;
		spin_unlock_bh(&ar->spectral.lock);
		if (active && nl_mode == QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE) {
			ath12k_warn(ar->ab,
				    "spectral scan_start: rejecting agile param update while scan is active\n");
			return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
		}

		/* Validate user-supplied attrs BEFORE storing. The ATTR_U32 macro
		 * below unconditionally writes into *p, so a post-store check
		 * would leave dirty state behind on rejection.
		 */
		if (nl_mode == QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL &&
		    (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY] ||
		     tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2])) {
			ath12k_warn(ar->ab,
				    "spectral scan: frequency param not supported in normal mode\n");
			return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_UNSUPPORTED);
		}

		/* Reject out-of-range scan_rpt_mode (valid 0..3). */
		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE]) {
			u32 v = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE]);

			if (v > ATH12K_SPECTRAL_RPT_MODE_MAX) {
				ath12k_warn(ar->ab,
					    "spectral scan: rpt_mode %u out of range [0, %u]\n",
					    v, ATH12K_SPECTRAL_RPT_MODE_MAX);
				return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
			}
		}

		/* Reject out-of-range bandwidth (nl80211_chan_width). */
		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]) {
			u8 v = nla_get_u8(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH]);

			if (v > NL80211_CHAN_WIDTH_320) {
				ath12k_warn(ar->ab,
					    "spectral scan: bandwidth %u out of range [0, %u]\n",
					    v, NL80211_CHAN_WIDTH_320);
				return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
			}
		}

		/* Reject scan_count > MAX. */
		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT]) {
			u32 v = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT]);

			if (v > ar->spectral.scan_count_max) {
				ath12k_warn(ar->ab,
					    "spectral scan: count %u exceeds max %u\n",
					    v, ar->spectral.scan_count_max);
				return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
			}
		}

		/* Reject fft_size out of [fft_size_min, fft_size_max[bw]].
		 * If we don't know the BW (no attr in this call, no cached
		 * attr), fall back to the chip-wide cap so we don't falsely
		 * narrow to the 20 MHz slot before the vdev is on a channel.
		 */
		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE]) {
			u32 v = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE]);
			u32 fft_min  = ar->spectral.param_min_max.fft_size_min;
			int idx      = ath12k_spectral_resolve_bw_idx(ar, tb);
			const u16 *hw_fft_max =
				ar->ab->hw_params->spectral.fft_size_max;
			u32 fft_max;

			if (idx >= 0) {
				fft_max = ar->spectral.param_min_max.fft_size_max[idx];
			} else {
				int bw;

				fft_max = 0;
				for (bw = 0; bw < ATH12K_SPECTRAL_NUM_BW_SLOTS; bw++) {
					if (hw_fft_max[bw] > fft_max)
						fft_max = hw_fft_max[bw];
				}
			}

			if (v < fft_min || v > fft_max) {
				ath12k_warn(ar->ab,
					    "spectral scan: fft_size %u out of [%u, %u] (bw_idx %d)\n",
					    v, fft_min, fft_max, idx);
				return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
			}
		}

		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT,
			 &p->scan_count);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD,
			 &p->scan_period);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY,
			 &p->scan_priority);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE,
			 &p->scan_fft_size);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA,
			  &p->scan_gc_ena);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA,
			  &p->scan_restart_ena);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF,
			 &p->scan_noise_floor_ref);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY,
			 &p->scan_init_delay);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR,
			 &p->scan_nb_tone_thr);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR,
			 &p->scan_str_bin_thr);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE,
			  &p->scan_wb_rpt_mode);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE,
			  &p->scan_rssi_rpt_mode);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR,
			 &p->scan_rssi_thr);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT,
			  &p->scan_pwr_format);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE,
			 &p->scan_rpt_mode);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE,
			 &p->scan_bin_scale);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ,
			  &p->scan_dbm_adj);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK,
			 &p->scan_chn_mask);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD,
			 &p->fft_period);
		ATTR_BOOL(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT,
			  &p->short_report);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY,
			 &p->frequency);
		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2,
			 &p->frequency2);
		ATTR_U8(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH,
			&p->bandwidth);
		struct nlattr *recapture_attr =
			tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_RECAPTURE];
		if (recapture_attr) {
			u8 recapture = nla_get_u8(recapture_attr);

			if (recapture) {
				if (p->scan_period >=
				    ATH12K_SPECTRAL_RECAPTURE_SCAN_PERIOD_THRESHOLD) {
					p->fft_recapture = 1;
				} else {
					ath12k_warn(ar->ab,
						    "spectral scan: FFT recapture cannot be enabled due to scan period: %u us\n",
						    p->scan_period);
					p->fft_recapture = 0;
				}
			} else {
				p->fft_recapture = 0;
			}
		}

		if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD]) {
			if (p->fft_recapture && p->scan_period <
			    ATH12K_SPECTRAL_RECAPTURE_SCAN_PERIOD_THRESHOLD) {
				ath12k_warn(ar->ab,
					    "spectral scan: FFT recapture disabled due to scan period: %u us\n",
					    p->scan_period);
				p->fft_recapture = 0;
			}
		}

		ATTR_U32(QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETION_TIMEOUT,
			 &p->completion_timeout_us);
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral config: count=%u period=%u fft_size=%u\n",
			   p->scan_count, p->scan_period, p->scan_fft_size);
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral config: priority=%u gc_ena=%u short_report=%u\n",
			   p->scan_priority, p->scan_gc_ena, p->short_report);
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral config: rpt_mode=%u bin_scale=%u chn_mask=%u\n",
			   p->scan_rpt_mode, p->scan_bin_scale, p->scan_chn_mask);
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral config: freq=%u freq2=%u bw=%u timeout_us=%u\n",
			   p->frequency, p->frequency2, p->bandwidth,
			   p->completion_timeout_us);

		/* Discard any sample still queued in the relay buffer from
		 * before this config change, so a stale sample captured
		 * under the old params isn't served as the first result of
		 * the next get_samples call.
		 */
		ath12k_spectral_reset_buffer(ar);
	}

	/* Step 2: configure firmware and trigger scan if request includes SCAN. */
	if (req_type != QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG) {
		enum spectral_scan_mode smode =
			(nl_mode == QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE) ?
			SPECTRAL_SCAN_MODE_AGILE :
			SPECTRAL_SCAN_MODE_NORMAL;
		bool already_active;

		/* Don't restart a running scan; collected samples would be lost. */
		if (smode == SPECTRAL_SCAN_MODE_AGILE) {
			spin_lock_bh(&ar->spectral.lock);
			already_active = ar->spectral.scan_active;
			spin_unlock_bh(&ar->spectral.lock);
			if (already_active) {
				ath12k_warn(ar->ab,
					    "spectral scan_start: agile scan already active, rejecting\n");
				return ath12k_spectral_scan_start_reply_error(wiphy,
				QCA_WLAN_VENDOR_SPECTRAL_SCAN_ERR_PARAM_INVALID_VALUE);
			}
		} else if (ar->spectral.scan_active) {
			ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
				   "spectral scan_start: scan already active, ignoring\n");
			return 0;
		}

		ar->spectral.samples_done = 0;
		ret = ath12k_spectral_configure_scan_params(ar, smode);
		if (ret)
			return ret;

		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral scan_start: configure_scan_params(%s) OK\n",
			   smode == SPECTRAL_SCAN_MODE_AGILE ? "AGILE" : "NORMAL");

		ret = ath12k_spectral_start_scan(ar);
		if (ret) {
			if (smode == SPECTRAL_SCAN_MODE_AGILE) {
				int stop_ret;

				ath12k_warn(ar->ab,
					    "spectral scan_start: agile start_scan failed (%d), rolling back\n",
					    ret);
				stop_ret = ath12k_spectral_stop_scan(ar);
				if (stop_ret)
					ath12k_warn(ar->ab,
						    "spectral scan_start: agile rollback stop_scan failed (%d)\n",
						    stop_ret);
				ath12k_spectral_rollback_agile(ar);
			}
			return ret;
		}
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral scan_start: start_scan OK\n");
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral scan_start: SUCCESS iface=%s req_type=%d\n",
		   wdev && wdev->netdev ? wdev->netdev->name : "?", req_type);
	return 0;
}

static int ath12k_vendor_spectral_scan_stop(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct ath12k *ar;
	int ret;

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar)
		return -EINVAL;

	ret = ath12k_spectral_stop_scan(ar);
	if (ret)
		ath12k_warn(ar->ab,
			    "spectral scan_stop: stop_scan failed ret=%d\n",
			    ret);
	else
		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral scan_stop: SUCCESS iface=%s\n",
			   wdev && wdev->netdev ? wdev->netdev->name : "?");
	return ret;
}

static int ath12k_vendor_spectral_get_config(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct ath12k_spectral_params *p;
	struct sk_buff *skb;
	struct ath12k *ar;

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb) {
		ath12k_warn(ar->ab, "spectral get_config: skb alloc failed\n");
		return -ENOMEM;
	}

	p = &ar->spectral.params;
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: count=%u period=%u fft_size=%u priority=%u\n",
		   p->scan_count, p->scan_period, p->scan_fft_size, p->scan_priority);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: gc_ena=%u restart_ena=%u nf_ref=%u\n",
		   p->scan_gc_ena, p->scan_restart_ena, p->scan_noise_floor_ref);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: init_delay=%u nb_tone_thr=%u str_bin_thr=%u\n",
		   p->scan_init_delay, p->scan_nb_tone_thr, p->scan_str_bin_thr);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: wb_rpt=%u rssi_rpt=%u rssi_thr=%u pwr_fmt=%u\n",
		   p->scan_wb_rpt_mode, p->scan_rssi_rpt_mode,
		   p->scan_rssi_thr, p->scan_pwr_format);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: rpt_mode=%u bin_scale=%u dbm_adj=%u\n",
		   p->scan_rpt_mode, p->scan_bin_scale, p->scan_dbm_adj);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: chn_mask=%u fft_period=%u short_report=%u\n",
		   p->scan_chn_mask, p->fft_period, p->short_report);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_config: freq=%u freq2=%u bw=%u timeout_us=%u\n",
		   p->frequency, p->frequency2, p->bandwidth, p->completion_timeout_us);

	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT,
		    p->scan_count);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD,
		    p->scan_period);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY,
		    p->scan_priority);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE,
		    p->scan_fft_size);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA,
		    p->scan_gc_ena);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA,
		    p->scan_restart_ena);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF,
		    p->scan_noise_floor_ref);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY,
		    p->scan_init_delay);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR,
		    p->scan_nb_tone_thr);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR,
		    p->scan_str_bin_thr);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE,
		    p->scan_wb_rpt_mode);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE,
		    p->scan_rssi_rpt_mode);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR,
		    p->scan_rssi_thr);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT,
		    p->scan_pwr_format);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE,
		    p->scan_rpt_mode);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE,
		    p->scan_bin_scale);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ,
		    p->scan_dbm_adj);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK,
		    p->scan_chn_mask);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD,
		    p->fft_period);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT,
		    p->short_report);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY,
		    p->frequency);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FREQUENCY_2,
		    p->frequency2);
	nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BANDWIDTH,
		   p->bandwidth);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COMPLETION_TIMEOUT,
		    p->completion_timeout_us);

	/* Return current scan mode so userspace knows whether NORMAL or
	 * AGILE spectral scan is configured.
	 */
	{
		enum spectral_scan_mode cur_mode;

		/* ar->spectral.mode is protected by ar->spectral.lock
		 * (spinlock_bh), NOT by wiphy_lock. Using wiphy_lock here
		 * caused a deadlock when wiphy_lock was held by another task
		 * while this handler tried to acquire it.
		 */
		spin_lock_bh(&ar->spectral.lock);
		cur_mode = ar->spectral.mode;
		spin_unlock_bh(&ar->spectral.lock);

		nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_MODE,
			    (cur_mode == SPECTRAL_SCAN_MODE_AGILE) ?
			    QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE :
			    QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL);
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int ath12k_vendor_spectral_get_diag(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int data_len)
{
	struct ath12k_spectral_diag_stats *d;
	struct sk_buff *skb;
	struct ath12k *ar;

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 256);
	if (!skb) {
		ath12k_warn(ar->ab, "spectral get_diag: skb alloc failed\n");
		return -ENOMEM;
	}

	d = &ar->spectral.diag;
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_diag: sig_mismatch=%llu sec80_insufflen=%llu\n",
		   d->sig_mismatch, d->sec80_sfft_insufflen);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_diag: no_sec80=%llu vhtseg1_mismatch=%llu\n",
		   d->no_sec80_sfft, d->vhtseg1id_mismatch);
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_diag: vhtseg2_mismatch=%llu\n",
		   d->vhtseg2id_mismatch);

	nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SIG_MISMATCH,
			  d->sig_mismatch, NLA_U64);
	nla_put_u64_64bit(skb,
			  QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_SEC80_SFFT_INSUFFLEN,
			  d->sec80_sfft_insufflen, NLA_U64);
	nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_NOSEC80_SFFT,
			  d->no_sec80_sfft, NLA_U64);
	nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG1ID_MISMATCH,
			  d->vhtseg1id_mismatch, NLA_U64);
	nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_DIAG_VHTSEG2ID_MISMATCH,
			  d->vhtseg2id_mismatch, NLA_U64);

	return cfg80211_vendor_cmd_reply(skb);
}

static int ath12k_vendor_spectral_get_cap(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct sk_buff *skb;
	struct ath12k *ar;

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar || !ar->spectral.enabled)
		return -EPERM;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 768);
	if (!skb) {
		ath12k_warn(ar->ab, "spectral get_cap: skb alloc failed\n");
		return -ENOMEM;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_cap: hw_gen=3 num_bw_caps=%u fft_size=%u\n",
		   ar->spectral.spectral_cap.num_bw_caps_entry,
		   ar->spectral.params.scan_fft_size);

	nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_PHYDIAG);
	nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RADAR);
	nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_SPECTRAL);
	nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_ADVANCED_SPECTRAL);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN,
		    QCA_WLAN_VENDOR_SPECTRAL_SCAN_CAP_HW_GEN_3);
	nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_FORMULA_ID, 0);
	nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_LOW_LEVEL_OFFSET,
		    SPECTRAL_SCALING_LOW_LEVEL_OFFSET);
	nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HIGH_LEVEL_OFFSET,
		    SPECTRAL_SCALING_HIGH_LEVEL_OFFSET);
	nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RSSI_THR,
		    SPECTRAL_SCALING_RSSI_THRESH);
	nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_DEFAULT_AGC_MAX_GAIN,
		   SPECTRAL_IPQ8074_DEFAULT_MAX_GAIN);

	/* Spectral supports a single detector for 20/40/80/160 MHz on all
	 * chipsets. 80+80 MHz is never supported. 320 MHz support is
	 * chipset-specific and tracked in hw_params.
	 */
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_20_MHZ, 1);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_40_MHZ, 1);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80_MHZ, 1);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_160_MHZ, 1);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_80P80_MHZ,
		    0);
	nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_NUM_DETECTORS_320_MHZ,
		    ar->ab->hw_params->spectral.supports_320mhz ? 1 : 0);

	/* Agile spectral scan capability flags, derived from chainmask table
	 * (populated in wmi.c via WMI_SERVICE_READY_EXT2 chainmask caps).
	 * Only emit the flag attribute when the capability is actually supported
	 */
	{
		struct ath12k_pdev_cap *cap = &ar->pdev->cap;

		if (cap->agile_spectral_cap)
			nla_put_flag(skb,
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL);
		if (cap->agile_spectral_cap_160)
			nla_put_flag(skb,
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_160);
		if (cap->agile_spectral_cap_80p80)
			nla_put_flag(skb,
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_80_80);
		if (cap->agile_spectral_cap_320)
			nla_put_flag(skb,
			QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_AGILE_SPECTRAL_320);

		ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
			   "spectral get_cap: agile=%d agile_160=%d agile_80p80=%d agile_320=%d\n",
			   cap->agile_spectral_cap, cap->agile_spectral_cap_160,
			   cap->agile_spectral_cap_80p80, cap->agile_spectral_cap_320);
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_cap: SUCCESS iface=%s\n",
		   wdev && wdev->netdev ? wdev->netdev->name : "?");
	return cfg80211_vendor_cmd_reply(skb);
}

static int ath12k_vendor_spectral_get_status(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	enum spectral_scan_mode mode;
	struct sk_buff *skb;
	struct ath12k *ar;

	ar = ath12k_spectral_get_ar(wiphy);
	if (!ar)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 64);
	if (!skb) {
		ath12k_warn(ar->ab, "spectral get_status: skb alloc failed\n");
		return -ENOMEM;
	}

	mode = ar->spectral.mode;
	ath12k_dbg(ar->ab, ATH12K_DBG_SPECTRAL,
		   "spectral get_status: mode=%d(%s) enabled=%d active=%d\n",
		   mode,
		   mode >= SPECTRAL_SCAN_MODE_MAX ? "DISABLED" :
		   mode == SPECTRAL_SCAN_MODE_AGILE ? "AGILE" : "NORMAL",
		   mode < SPECTRAL_SCAN_MODE_MAX,
		   mode < SPECTRAL_SCAN_MODE_MAX);

	if (mode < SPECTRAL_SCAN_MODE_MAX)
		nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ENABLED);
	if (mode < SPECTRAL_SCAN_MODE_MAX)
		nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ACTIVE);
	if (mode == SPECTRAL_SCAN_MODE_AGILE)
		nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE,
			    QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_AGILE);
	else if (mode == SPECTRAL_SCAN_MODE_NORMAL)
		nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MODE,
			    QCA_WLAN_VENDOR_SPECTRAL_SCAN_MODE_NORMAL);

	return cfg80211_vendor_cmd_reply(skb);
}

#endif /* CPTCFG_ATH12K_SPECTRAL */

static const struct nla_policy
ath12k_vendor_rf_path_mode_policy[QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX] = { .type = NLA_U32 },
};

static void ath12k_vendor_rf_path_mode_event(struct wiphy *wiphy,
					     u32 rf_path_index,
					     u32 status)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
					  nla_total_size(sizeof(u32)) * 2,
					  QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL,
			   "rf_path_mode: failed to alloc vendor event skb\n");
		return;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX,
			rf_path_index) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_STATUS,
			status)) {
		nlmsg_free(skb);
		ath12k_err(NULL, "rf_path_mode: failed to fill event attr\n");
		return;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
}

static int ath12k_vendor_rf_path_mode_get(struct wiphy *wiphy)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct sk_buff *reply;
	struct ath12k *ar;
	int i;

	for_each_ar(ah, ar, i) {
		if (!(ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) ||
		    ar->supports_6ghz || !ar->rf_path_ctx.supported)
			continue;

		reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							    nla_total_size(sizeof(u32)));
		if (!reply)
			return -ENOMEM;

		if (nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX,
				ar->rf_path_ctx.current_index)) {
			nlmsg_free(reply);
			return -ENOBUFS;
		}

		return cfg80211_vendor_cmd_reply(reply);
	}

	return -EOPNOTSUPP;
}

static int ath12k_vendor_green_ap_handler(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GREEN_AP_MAX + 1];
	struct ath12k_link_vif *arvif;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	unsigned int link_id;
	struct ath12k *ar;
	int ret;

	lockdep_assert_wiphy(wiphy);

	if (data && data_len) {
		ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GREEN_AP_MAX, data, data_len,
				ath12k_vendor_green_ap_policy, NULL);
		if (ret) {
			ath12k_err(NULL, "GreenAP: failed to parse attrs: %d\n", ret);
			return ret;
		}
	} else {
		/* tb is a stack variable — without nla_parse it contains garbage.
		 * Zero it manually so all tb[] checks behave as if no attrs were
		 * sent; link_id defaults to 0 and the GET path is entered.
		 */
		memset(tb, 0, sizeof(tb));
	}
	/* Resolve link_id from request: mandatory for MLO, default 0 for non-MLO */
	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return -EINVAL;
	ahvif = ath12k_vif_to_ahvif(vif);

	if (vif->valid_links) {
		link_id = tb[QCA_WLAN_VENDOR_ATTR_GREEN_AP_LINK_ID] ?
			  nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_GREEN_AP_LINK_ID]) : 0;
		if (!(vif->valid_links & BIT(link_id)))
			return -ENOLINK;
		arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
	} else {
		link_id = 0;
		arvif = &ahvif->deflink;
	}

	if (!arvif || !arvif->ar)
		return -ENOLINK;
	ar = arvif->ar;

	/* GET path: no SET attrs present.
	 * cfg80211tool get_ap_ps_on / get_ps_timeout sends only the link_id
	 * attr (no SET attrs), so absence of SET attrs is the correct GET signal.
	 * GET attrs are response-only — userspace never sends them, so checking
	 * tb[GET_*] would never be true.
	 */
	if (!tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_ENABLE_MODE] &&
	    !tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_PS_TIMEOUT]) {
		struct sk_buff *reply;

		reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							    nla_total_size(sizeof(u32)) *
							    2);
		if (!reply)
			return -ENOMEM;

		if (nla_put_u8(reply, QCA_WLAN_VENDOR_ATTR_GET_GREEN_AP_ENABLE_MODE,
			       ahvif->ap_ps_on[link_id]) ||
		    nla_put_u32(reply, QCA_WLAN_VENDOR_ATTR_GET_GREEN_AP_PS_TIMEOUT,
				ahvif->ps_timeout[link_id])) {
			nlmsg_free(reply);
			return -ENOBUFS;
		}

		return cfg80211_vendor_cmd_reply(reply);
	}

	/* SET path */
	if (tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_ENABLE_MODE]) {
		u8 mode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_ENABLE_MODE]);

		switch (mode) {
		case QCA_WLAN_VENDOR_ATTR_GREEN_AP_MODE_DISABLED:
			ar->ap_ps_mode = ATH12K_GREEN_AP_MODE_DISABLED;
			break;
		case QCA_WLAN_VENDOR_ATTR_GREEN_AP_MODE_NO_STA:
			ar->ap_ps_mode = ATH12K_GREEN_AP_MODE_NO_STA;
			break;
		case QCA_WLAN_VENDOR_ATTR_GREEN_AP_MODE_NUM_STREAM:
			ar->ap_ps_mode = ATH12K_GREEN_AP_MODE_NUM_STREAM;
			break;
		default:
			ath12k_err(ar->ab,
				   "GreenAP: invalid mode %u\n", mode);
			return -EINVAL;
		}

		ar->ap_ps_enabled = (ar->ap_ps_mode != ATH12K_GREEN_AP_MODE_DISABLED);
		if (!ar->ap_ps_enabled) {
			ar->ap_ps_disabled_by_agile = false;
			ar->num_stations_multistream = 0;
		} else if (ar->ap_ps_mode == ATH12K_GREEN_AP_MODE_NUM_STREAM) {
			ath12k_mac_ap_ps_seed_multistream_count(ar, NULL);
		}
		arvif->vap_cfg.ap_ps_on = mode;
		ahvif->ap_ps_on[link_id] = mode;
		if (ar->hw_link_id < ATH12K_GROUP_MAX_RADIO)
			ahvif->ap_ps_on_by_hwlink[ar->hw_link_id] = mode;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "GreenAP: pdev_id %u mode set to %u link_id %u\n",
			   ar->pdev->pdev_id, mode, link_id);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_PS_TIMEOUT]) {
		u32 value = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SET_GREEN_AP_PS_TIMEOUT]);

		if (value < ATH12K_GREEN_AP_PS_TIMEOUT_MIN ||
		    value > ATH12K_GREEN_AP_PS_TIMEOUT_MAX) {
			ath12k_err(NULL,
				   "GreenAP: ps_timeout out of range (%u), using default %u\n",
				   value, ATH12K_GREEN_AP_PS_TIMEOUT_DEFAULT);
			value = ATH12K_GREEN_AP_PS_TIMEOUT_DEFAULT;
		}
		ar->ap_ps_timeout = value;
		arvif->vap_cfg.ps_timeout = value;
		ahvif->ps_timeout[link_id] = value;
		if (ar->hw_link_id < ATH12K_GROUP_MAX_RADIO)
			ahvif->ps_timeout_by_hwlink[ar->hw_link_id] = value;
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "GreenAP: pdev_id %u ps_timeout set to %u s link_id %u\n",
			   ar->pdev->pdev_id, value, link_id);
	}

	ath12k_mac_ap_ps_recalc(ar);

	return 0;
}

static const struct nla_policy
ath12k_vendor_pasn_peer_policy[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_AKM] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_CIPHER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_PASSWORD] = { .type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_PMKID] =
		NLA_POLICY_EXACT_LEN_WARN(WLAN_PMKID_LEN),
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_COMEBACK_AFTER] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_COOKIE] = { .type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_TYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEER_CONTROL_FLAG] = { .type = NLA_U16 },
};

static int ath12k_vendor_parse_pasn_peer(struct nlattr *peer_attr,
					 struct nlattr **peer)
{
	int ret;

	ret = nla_parse_nested(peer, QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX,
			       peer_attr, ath12k_vendor_pasn_peer_policy, NULL);
	if (ret)
		return ret;

	if (!peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR])
		return -EINVAL;

	return 0;
}

/**
 * ath12k_pasn_arvif_from_wdev() - Resolve ath12k_link_vif from wdev + link_id.
 * @wdev: wireless device from nl80211 callback.
 * @link_id: link_id from vendor attr, or INVALID_LINK_ID.
 *
 * For non-MLO (link_id == INVALID_LINK_ID) iterate links_map to find the
 * active arvif rather than using deflink (link_id=0 placeholder which does
 * not match the active scan-link vdev at ATH12K_DEFAULT_SCAN_LINK=15).
 *
 * Returns the arvif on success, NULL on lookup failure.
 */
static struct ath12k_link_vif *
ath12k_pasn_arvif_from_wdev(struct wireless_dev *wdev, u8 link_id)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_vif *ahvif;
	unsigned long links;
	u8 lid;

	if (!vif)
		return NULL;
	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		return NULL;

	if (link_id != INVALID_LINK_ID)
		return ath12k_get_arvif_from_link_id(ahvif, link_id);

	/* Non-MLO: find the started arvif in links_map.
	 * deflink.link_id == 0 is a placeholder and may not match the
	 * active scan-link vdev (link_id = ATH12K_DEFAULT_SCAN_LINK).
	 */
	links = ahvif->links_map;
	for_each_set_bit(lid, &links, ATH12K_NUM_MAX_LINKS) {
		struct ath12k_link_vif *a =
			wiphy_dereference(ahvif->ah->hw->wiphy, ahvif->link[lid]);
		if (a && a->ar && a->is_started)
			return a;
	}
	return NULL;
}

#ifdef CPTCFG_QCN_EXTN
static const struct nla_policy ath12k_vendor_secure_ranging_ctx_policy
		[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR] =
		NLA_POLICY_EXACT_LEN_WARN(ETH_ALEN),
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK] = {
		.type = NLA_BINARY, .len = WMI_MAX_KEY_LEN },
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED] = {
		.type = NLA_BINARY, .len = ATH12K_SECURE_RANGING_LTF_KEYSEED_MAX_LEN },
	[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LINK_ID] = { .type = NLA_U8 },
};

static int ath12k_vendor_secure_ranging_ctx_cmd(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX + 1];
	struct ath12k_wmi_ltf_keyseed_arg arg;
	struct ath12k_link_vif *arvif;
	const u8 *src_addr = NULL;
	const u8 *peer_addr;
	u8 link_id = INVALID_LINK_ID;
	u32 action;
	int ret;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX,
			data, data_len, ath12k_vendor_secure_ranging_ctx_policy,
			NULL);
	if (ret)
		return ret;

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR])
		return -EINVAL;

	peer_addr = nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_PEER_MAC_ADDR]);
	if (is_zero_ether_addr(peer_addr))
		return -EINVAL;
	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR])
		src_addr = nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SRC_ADDR]);

	action = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_ACTION]);
	if (action > QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_DELETE)
		return -EINVAL;

	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LINK_ID]) {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LINK_ID]);
		if (!is_valid_link_id(link_id) || link_id >= ATH12K_NUM_MAX_LINKS)
			return -EINVAL;
	}

	arvif = ath12k_pasn_arvif_from_wdev(wdev, link_id);
	if (!arvif || !arvif->ar)
		return -ENOLINK;

	if (action == QCA_WLAN_VENDOR_SECURE_RANGING_CTX_ACTION_DELETE) {
		ret = ath12k_wmi_send_rtt_pasn_deauth(arvif->ar, peer_addr);
		if (ret)
			return ret;
		ath12k_pasn_fw_peer_delete(arvif, peer_addr, false);
		ath12k_pasn_peer_delete(arvif, peer_addr);
		return 0;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_TK]) {
		ret = ath12k_vendor_install_secure_ranging_tk(arvif, tb);
		if (ret)
			return ret;
		/* SECURE_CTX only after TK is actually installed. */
		ath12k_pasn_peer_set_secure_ctx(arvif, src_addr, peer_addr, true);
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED])
		return 0;

	if (!tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_SHA_TYPE])
		return -EINVAL;

	memset(&arg, 0, sizeof(arg));
	arg.vdev_id = arvif->vdev_id;
	ether_addr_copy(arg.peer_mac, peer_addr);
	if (tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER])
		arg.rsn_authmode =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_CIPHER]);
	arg.keyseed = nla_data(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED]);
	arg.keyseed_len =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_LTF_KEYSEED]);
	if (!arg.keyseed_len)
		return -EINVAL;

	ret = ath12k_wmi_send_ltf_key_seed(arvif->ar, &arg);
	if (ret)
		return ret;
	ath12k_pasn_peer_set_ltf_keyseed(arvif, peer_addr, true);

	return 0;
}
#endif /* CPTCFG_QCN_EXTN */

static const struct nla_policy
ath12k_vendor_pasn_policy[QCA_WLAN_VENDOR_ATTR_PASN_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PASN_ACTION] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_PASN_PEERS] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID] = { .type = NLA_U8 },
};

static int ath12k_vendor_pasn_delete_peer(struct ath12k_link_vif *arvif,
					  const u8 *peer_addr)
{
	int ret;

	ret = ath12k_wmi_send_rtt_pasn_deauth(arvif->ar, peer_addr);
	if (ret)
		ath12k_warn(arvif->ar->ab,
			    "PASN: deauth failed for %pM: %d\n",
			    peer_addr, ret);
	ath12k_pasn_peer_delete(arvif, peer_addr);
	return ret;
}

static int ath12k_vendor_pasn_peer_action(struct ath12k_link_vif *arvif,
						 struct nlattr **tb, u32 action)
{
	const int peer_mac_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR;
	const int peer_type_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_TYPE;
	const int ltf_keyseed_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED;
	const int control_flag_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_CONTROL_FLAG;
	struct nlattr *peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1];
	struct nlattr *peer_attr;
	int rem;

	if (!tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS])
		return -EINVAL;

	ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN vendor peer action %u vdev %u\n",
		   action, arvif->vdev_id);

	nla_for_each_nested(peer_attr,
			    tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS], rem) {
		bool ltf;
		const u8 *peer_addr;
		int ret;

		ret = ath12k_vendor_parse_pasn_peer(peer_attr, peer);
		if (ret) {
			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN peer parse failed action %u ret %d\n",
				   action, ret);
			return ret;
		}

		peer_addr = nla_data(peer[peer_mac_attr]);
		if (is_zero_ether_addr(peer_addr)) {
			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN invalid zero peer addr action %u\n",
				   action);
			return -EINVAL;
		}

		if (action == QCA_WLAN_VENDOR_PASN_ACTION_PEER_CREATE) {
			u32 peer_type;
			u8 sec;

			if (!peer[peer_type_attr]) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN create missing peer type %pM\n",
					   peer_addr);
				return -EINVAL;
			}

			peer_type = nla_get_u32(peer[peer_type_attr]);
			if (peer_type != ATH12K_PASN_PEER_TYPE_UNSECURE &&
			    peer_type != ATH12K_PASN_PEER_TYPE_SECURE) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN create invalid type %u peer %pM\n",
					   peer_type, peer_addr);
				return -EINVAL;
			}

			ltf = peer[ltf_keyseed_attr];
			if (ltf && peer_type == ATH12K_PASN_PEER_TYPE_UNSECURE) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN LTF requested for open peer %pM\n",
					   peer_addr);
				return -EINVAL;
			}

			sec = peer_type == ATH12K_PASN_PEER_TYPE_SECURE ?
				ATH12K_WMI_RTT_PASN_SECURITY_MODE_MAC_SEC :
				ATH12K_WMI_RTT_PASN_SECURITY_MODE_NONE;
			if (ltf)
				sec = ATH12K_WMI_RTT_PASN_SECURITY_MODE_MAC_PHY_SEC;

			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN create peer %pM type %u ltf %u sec %u\n",
				   peer_addr, peer_type, ltf, sec);

			ret = ath12k_pasn_peer_create_or_update(arvif, NULL,
								peer_addr, ltf, sec);
			if (ret) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN create SW peer failed %pM ret %d\n",
					   peer_addr, ret);
				return ret;
			}

			ret = ath12k_pasn_fw_peer_create(arvif, peer_addr);
			if (ret) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN create FW peer failed %pM ret %d\n",
					   peer_addr, ret);
				ath12k_pasn_peer_delete(arvif, peer_addr);
				return ret;
			}

			continue;
		}

		if (action == QCA_WLAN_VENDOR_PASN_ACTION_PEER_DELETE) {
			bool skip_peer_del = false;
			u16 control_flag;

			if (!peer[control_flag_attr]) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN delete missing control flag %pM\n",
					   peer_addr);
				return -EINVAL;
			}

			control_flag = nla_get_u16(peer[control_flag_attr]);
			switch (control_flag) {
			case ATH12K_PASN_PEER_DELETE_NORMAL:
			case ATH12K_PASN_PEER_DELETE_FLUSH_KEYS:
				break;
			case ATH12K_PASN_PEER_DELETE_ALREADY_DELETED:
				skip_peer_del = true;
				break;
			default:
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN delete invalid flag %u peer %pM\n",
					   control_flag, peer_addr);
				return -EINVAL;
			}

			ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
				   "RTT PASN delete peer %pM control 0x%x skip %u\n",
				   peer_addr, control_flag, skip_peer_del);

			ret = ath12k_pasn_fw_peer_delete(arvif, peer_addr,
							 skip_peer_del);
			if (ret) {
				ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
					   "RTT PASN delete FW peer failed %pM ret %d\n",
					   peer_addr, ret);
				return ret;
			}

			ath12k_pasn_peer_delete(arvif, peer_addr);
		}
	}

	return 0;
}

static int ath12k_vendor_pasn_auth_peer(struct ath12k_link_vif *arvif,
					struct nlattr *peer[])
{
	struct ath12k_wmi_rtt_pasn_auth_status_arg arg = {};
	const u8 *peer_addr;

	peer_addr = nla_data(peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR]);

	if (peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR])
		ether_addr_copy(arg.source_mac,
				nla_data(peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR]));
	else
		ether_addr_copy(arg.source_mac, arvif->bssid);

	ether_addr_copy(arg.peer_mac, peer_addr);
	arg.status = peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS] ? 0 : 1;

	if (peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_AKM])
		arg.akm = nla_get_u32(peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_AKM]);

	if (peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_CIPHER])
		arg.cipher = nla_get_u32(peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_CIPHER]);

	if (peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_COMEBACK_AFTER])
		arg.comeback_after = nla_get_u16(
			peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_COMEBACK_AFTER]);

	ath12k_pasn_peer_set_auth_status(arvif, arg.source_mac,
					 arg.peer_mac, !arg.status);
	if (ath12k_pasn_peer_auth_status_sent(arvif, arg.peer_mac))
		return 0;

	ath12k_pasn_peer_set_auth_status_sent(arvif, arg.peer_mac);

	return ath12k_wmi_send_pasn_auth_status(arvif->ar, &arg);
}

/**
 * ath12k_vendor_pasn_cmd() - Handle QCA_NL80211_VENDOR_SUBCMD_PASN from hostapd.
 * @wiphy: wiphy of the AP interface.
 * @wdev: wireless device of the AP interface.
 * @data: netlink attribute data.
 * @data_len: length of @data.
 *
 * Processes PASN authentication results or DELETE actions from hostapd.
 * For each peer in the nested peer list, sends
 * WMI_RTT_PASN_AUTH_STATUS_CMD to firmware on auth success, or sends
 * WMI_RTT_PASN_DEAUTH_CMD and removes the SW tracking entry on DELETE.
 *
 * Returns 0 on success or a negative errno on failure.
 */
static int ath12k_vendor_pasn_cmd(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PASN_MAX + 1];
	struct nlattr *peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1];
	struct ath12k_link_vif *arvif;
	struct nlattr *peer_attr;
	u32 action = QCA_WLAN_VENDOR_PASN_ACTION_AUTH;
	u8 link_id = INVALID_LINK_ID;
	int rem, ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PASN_MAX, data, data_len,
			ath12k_vendor_pasn_policy, NULL);
	if (ret)
		return ret;

	if (tb[QCA_WLAN_VENDOR_ATTR_PASN_ACTION]) {
		action = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_PASN_ACTION]);
		if (action > QCA_WLAN_VENDOR_PASN_ACTION_PEER_DELETE)
			return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID]) {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID]);
		if (!is_valid_link_id(link_id) || link_id >= ATH12K_NUM_MAX_LINKS)
			return -EINVAL;
	}

	arvif = ath12k_pasn_arvif_from_wdev(wdev, link_id);
	if (!arvif || !arvif->ar)
		return -ENOLINK;

	if (action == QCA_WLAN_VENDOR_PASN_ACTION_PEER_CREATE ||
	    action == QCA_WLAN_VENDOR_PASN_ACTION_PEER_DELETE)
		return ath12k_vendor_pasn_peer_action(arvif,
			(struct nlattr **)tb, action);

	if (!tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS])
		return -EINVAL;

	/* Process all peers; continue on per-peer errors so FW receives
	 * status for all peers, not just those before the first failure.
	 */
	nla_for_each_nested(peer_attr, tb[QCA_WLAN_VENDOR_ATTR_PASN_PEERS], rem) {
		const u8 *peer_addr;
		int peer_ret;

		peer_ret = ath12k_vendor_parse_pasn_peer(peer_attr, peer);
		if (peer_ret) {
			ath12k_warn(arvif->ar->ab,
				    "PASN: failed to parse peer attr: %d\n",
				    peer_ret);
			ret = peer_ret;
			continue;
		}

		peer_addr = nla_data(peer[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR]);
		if (is_zero_ether_addr(peer_addr)) {
			ret = -EINVAL;
			continue;
		}

		if (action == QCA_WLAN_VENDOR_PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT)
			peer_ret = ath12k_vendor_pasn_delete_peer(arvif, peer_addr);
		else
			peer_ret = ath12k_vendor_pasn_auth_peer(arvif, peer);

		if (peer_ret)
			ret = peer_ret;
	}

	return ret;
}

static int ath12k_vendor_list_pasn_peer_cmd(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	const int ltf_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_LTF_KEYSEED_REQUIRED;
	const int status_attr =
		QCA_WLAN_VENDOR_ATTR_PASN_PEER_STATUS_SUCCESS;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PASN_MAX + 1];
	struct ath12k_rtt_pasn_peer *pasn_peer;
	struct ath12k_link_vif *arvif;
	struct nlattr *peers;
	struct nlattr *peer;
	struct sk_buff *skb;
	u8 link_id = INVALID_LINK_ID;
	int ret = 0;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PASN_MAX,
			data, data_len, ath12k_vendor_pasn_policy,
			NULL);
	if (ret)
		return ret;

	if (tb[QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID]) {
		link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_PASN_LINK_ID]);
		if (!is_valid_link_id(link_id) || link_id >= ATH12K_NUM_MAX_LINKS)
			return -EINVAL;
	}

	arvif = ath12k_pasn_arvif_from_wdev(wdev, link_id);
	if (!arvif || !arvif->ar)
		return -ENOLINK;

	ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
		   "RTT PASN list peers vdev %u link %u\n",
		   arvif->vdev_id, link_id);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	peers = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEERS);
	if (!peers)
		goto put_failure;

	spin_lock_bh(&arvif->rtt_ctx.pasn_peer_lock);
	list_for_each_entry(pasn_peer, &arvif->rtt_ctx.pasn_peer_list, list) {
		peer = nla_nest_start(skb, 0);
		if (!peer) {
			ret = -ENOBUFS;
			goto unlock;
		}

		if (!is_zero_ether_addr(pasn_peer->src_addr) &&
		    nla_put(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEER_SRC_ADDR,
			    ETH_ALEN, pasn_peer->src_addr)) {
			ret = -ENOBUFS;
			goto unlock;
		}

		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAC_ADDR,
			    ETH_ALEN, pasn_peer->peer_addr)) {
			ret = -ENOBUFS;
			goto unlock;
		}

		if (pasn_peer->ltf_keyseed_required &&
		    nla_put_flag(skb, ltf_attr)) {
			ret = -ENOBUFS;
			goto unlock;
		}

		if ((pasn_peer->flags & ATH12K_PASN_F_AUTH_SUCCESS) &&
		    nla_put_flag(skb, status_attr)) {
			ret = -ENOBUFS;
			goto unlock;
		}

		ath12k_dbg(arvif->ar->ab, ATH12K_DBG_RTT,
			   "RTT PASN list peer %pM flags 0x%x ltf %u\n",
			   pasn_peer->peer_addr, pasn_peer->flags,
			   pasn_peer->ltf_keyseed_required);

		nla_nest_end(skb, peer);
	}
unlock:
	spin_unlock_bh(&arvif->rtt_ctx.pasn_peer_lock);

	if (ret)
		goto put_failure;

	nla_nest_end(skb, peers);

	return cfg80211_vendor_cmd_reply(skb);

put_failure:
	kfree_skb(skb);
	return -ENOBUFS;
}

static int ath12k_vendor_rf_path_mode_handler(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_MAX + 1];
	struct ath12k *ar;
	u32 rf_path_index;
	int ret, i;
	bool any_failed = false;
	bool feature_supported = false;

	lockdep_assert_wiphy(wiphy);

	/* Allow the command when the radio is ready (OFF = firmware up, no
	 * interface yet) or running (ON).  Reject during recovery/wedged
	 * states where the WMI link may be unavailable.
	 */
	if (ah->state == ATH12K_HW_STATE_RESTARTING ||
	    ah->state == ATH12K_HW_STATE_WEDGED)
		return -ENODEV;

	if (data && data_len) {
		ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_MAX,
				data, data_len,
				ath12k_vendor_rf_path_mode_policy, NULL);
		if (ret) {
			ath12k_err(NULL,
				   "rf_path_mode: failed to parse attrs: %d\n",
				   ret);
			return ret;
		}
	} else {
		memset(tb, 0, sizeof(tb));
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX])
		return ath12k_vendor_rf_path_mode_get(wiphy);

	rf_path_index = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_INDEX]);
	if (rf_path_index > QCA_WLAN_VENDOR_RF_PATH_MODE_5G_HIGH_RANGE) {
		ath12k_err(NULL,
			   "rf_path_mode: invalid mode %u (0=5G full range, 1=5G high range)\n",
			   rf_path_index);
		return -EINVAL;
	}

	ath12k_dbg(ath12k_ah_to_ar(ah, 0)->ab, ATH12K_DBG_MAC,
		   "rf_path_mode SET: requested mode %u (%s)\n",
		   rf_path_index,
		   rf_path_index == QCA_WLAN_VENDOR_RF_PATH_MODE_5G_HIGH_RANGE ?
		   "5G high range" : "5G full range");

	for_each_ar(ah, ar, i) {
		if (!(ar->pdev->cap.supported_bands & WMI_HOST_WLAN_5GHZ_CAP) ||
		    ar->supports_6ghz)
			continue;

		if (!ar->rf_path_ctx.supported)
			continue;

		feature_supported = true;

		ret = ath12k_mac_handle_rf_path_switch(ar, rf_path_index);
		if (ret) {
			ath12k_warn(ar->ab,
				    "rf_path_mode: pdev %u switch to index %u failed: %d\n",
				    ar->pdev->pdev_id, rf_path_index, ret);
			any_failed = true;
		}
	}

	if (!feature_supported)
		return -EOPNOTSUPP;

	/* Always send the completion event — on success and on failure.
	 * STATUS=0 means the switch succeeded; STATUS=1 means it failed.
	 * The event is not sent for -EOPNOTSUPP (feature absent entirely)
	 * since there is nothing meaningful to report in that case.
	 */
	ath12k_vendor_rf_path_mode_event(wiphy, rf_path_index,
					 any_failed ? 1 : 0);
	return any_failed ? -EIO : 0;
}

static struct wiphy_vendor_command ath12k_vendor_commands[] = {
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		.doit = ath12k_vendor_wifi_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
		.doit = ath12k_vendor_get_wifi_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC,
		.doit = ath12k_vendor_parse_rm,
		.policy = ath12k_vendor_rm_generic_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE,
		.doit = ath12k_vendor_trigg_pri_link_migrate,
		.policy = ath12k_pri_link_migrate_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE,
		.doit = ath12k_vendor_receive_afc_response,
		.policy = ath12k_cfg80211_afc_response_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_MAX
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE,
		.doit = ath12k_vendor_6ghz_power_mode_change,
		.policy = ath12k_cfg80211_power_mode_set_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS,
		.doit = ath12k_vendor_sdwf_phy_operations,
		.dumpit = ath12k_vendor_view_sdwf_config,
		.policy = ath12k_vendor_sdwf_phy_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS,
		.doit = ath12k_vendor_atf_offload_config_handler,
		.dumpit = ath12k_vendor_atf_stats_dumpit,
		.policy = ath12k_atf_offload_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY,
		.doit = ath12k_vendor_wlan_telemetry_wiphy_getstats,
		.policy = ath12k_wlan_telemetry_req_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV,
		.doit = ath12k_vendor_wlan_telemetry_wdev_getstats,
		.policy = ath12k_wlan_telemetry_req_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_CLEAR_PAYLOAD,
		.doit = ath12k_vendor_clear_afc_payload,
		.policy = ath12k_afc_clear_payload_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_RESET,
		.doit = ath12k_vendor_reset_afc,
		.policy = ath12k_afc_reset_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_FETCH_POWER_EVENT,
		.doit = ath12k_vendor_fetch_afc_power_info,
		.policy = ath12k_afc_fetch_power_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION,
		.doit = ath12k_vendor_wiphy_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION,
		.doit = ath12k_vendor_get_wiphy_config_handler,
		.policy = ath12k_wifi_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_GET_REG_EIRP,
		.doit = ath12k_vendor_get_reg_eirp_handler,
		.policy = ath12k_reg_get_eirp_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS,
		.doit = ath12k_vendor_sdwf_dev_operations,
		.policy = ath12k_vendor_sdwf_dev_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ME_CONFIG,
		.doit = ath12k_vendor_me_config_handler,
		.policy = ath12k_vendor_me_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_ME_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_IGMP_TID_OVERRIDE,
		.doit           = ath12k_vendor_igmp_tid_override_handler,
		.policy         = ath12k_vendor_igmp_tid_override_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_IGMP_TID_OVERRIDE_MAX,
		.flags          = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ME_LIST,
		.doit = ath12k_vendor_hmmc_deny_list_handler,
		.policy = ath12k_vendor_me_list_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_ME_LIST_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_ME_STATS_PRINT,
		.doit           = ath12k_vendor_me_dump,
		.policy         = ath12k_vendor_me_dump_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_ME_STATS_PRINT_MAX,
		.flags          = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},

#ifdef CPTCFG_QCN_EXTN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO,
		.doit = ath12k_vendor_get_rropinfo,
		.policy = ath12k_rrop_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO,
		.doit = ath12k_vendor_get_sta_240mhz_info,
		.policy = ath12k_240mhz_sta_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_240MHZ_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
		.doit = ath12k_vendor_rule_config_notify,
		.policy = ath12k_rule_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DERIVE_LINK_BSS_ADDR,
		.doit = ath12k_vendor_derive_link_bss_addr_extn,
		.policy = ath12k_wifi_mac_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_MAC_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_CTL_TABLE,
		.doit = ath12k_vendor_ctl_table,
		.policy = ath12k_ctl_table_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_RTT_BLOB,
		.doit = ath12k_vendor_rtt_blob,
		.policy = ath12k_rtt_blob_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_RTT_BLOB_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_STA_MAX_PWR_TABLE,
		.doit = ath12k_vendor_sta_max_pwr_table,
		.policy = ath12k_sta_max_pwr_table_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_STA_MAX_PWR_TABLE_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_POWER_TABLE,
		.doit = ath12k_vendor_power_table,
		.policy = ath12k_power_table_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_POWER_TABLE_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX,
		.doit = ath12k_vendor_home_offchan_tx_rx_handler,
		.policy = ath12k_vendor_home_offchan_tx_rx_policy,
		.maxattr = QCA_VENDOR_ATTR_WLAN_HOME_OFFCHAN_TX_RX_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT,
		.doit = ath12k_vendor_secure_ranging_ctx_cmd,
		.policy = ath12k_vendor_secure_ranging_ctx_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG,
		.doit = ath12k_vendor_dcs_config_handler,
		.policy = ath12k_vendor_dcs_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_DCS_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN,
		.doit = ath12k_vendor_parse_cbs_scan_params,
		.policy = ath12k_vendor_scan_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_SCAN_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CHANNEL_SWITCH_TIME,
		.doit = ath12k_vendor_get_channel_switch_time,
		.policy = ath12k_vendor_channel_switch_time_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CHANNEL_SWITCH_TIME_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCS_SIM,
		.doit = ath12k_vendor_dcs_sim_handler,
		.policy = ath12k_vendor_dcs_sim_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_DCS_SIM_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_REG_PARAMS,
		.doit = ath12k_vendor_reg_params_handler,
		.policy = ath12k_reg_params_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_REG_PARAMS_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS,
		.doit = ath12k_vendor_get_hw_blocked_chans_extn,
		.policy = ath12k_hw_blocked_chans_query_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_HW_BLOCKED_CHANS_REQ_MAX,
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO,
		.dumpit = ath12k_vendor_get_sta_info_dumpit,
		.policy = ath12k_vendor_get_sta_info_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_REPURPOSE_LINK_INDICATION,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = ath12k_vendor_repurpose_link,
		.policy = ath12k_repurpose_link_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
	},
#ifdef CPTCFG_ATH12K_CFR
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG,
		.doit = ath12k_vendor_peer_cfr_handler,
		.policy = ath12k_peer_cfr_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = ath12k_vendor_extended_monitor_handler,
		.policy = ath12k_vendor_ext_mon_policy,
		.maxattr = QCA_VENDOR_ATTR_EXT_MON_MAX,
	},
#ifdef CPTCFG_QCN_EXTN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDMA_SCHEDULE_CONFIG,
		.doit = ath12k_vendor_tdma_schedule_config,
		.policy = ath12k_vendor_tdma_schedule_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_TDMA_SCHEDULE_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
#endif /* CPTCFG_QCN_EXTN */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OEM_DATA,
		.doit = ath12k_vendor_oem_data,
		.policy = ath12k_oem_data_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_PCP_TID_MAP,
		.doit = ath12k_vendor_set_pcp_tid_map,
		.policy = ath12k_pcp_tid_map_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_PCP_TID_MAP,
		.doit = ath12k_vendor_get_pcp_tid_map,
		.policy = ath12k_pcp_tid_map_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PCP_TID_MAP_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_TID_MAP_PRECEDENCE,
		.doit = ath12k_vendor_set_tid_map_precedence,
		.policy = ath12k_tid_map_prty_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_TID_MAP_PRECEDENCE,
		.doit = ath12k_vendor_get_tid_map_precedence,
		.policy = ath12k_tid_map_prty_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_TID_MAP_PRECEDENCE_MAX,
	},

#ifdef CPTCFG_ATH12K_SPECTRAL

	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START,
		.doit           = ath12k_vendor_spectral_scan_start,
		.policy         = ath12k_spectral_scan_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP,
		.doit           = ath12k_vendor_spectral_scan_stop,
		.policy         = ath12k_spectral_scan_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG,
		.doit           = ath12k_vendor_spectral_get_config,
		.policy         = ath12k_spectral_scan_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS,
		.doit           = ath12k_vendor_spectral_get_diag,
		.policy         = VENDOR_CMD_RAW_DATA,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO,
		.doit           = ath12k_vendor_spectral_get_cap,
		.policy         = VENDOR_CMD_RAW_DATA,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS,
		.doit           = ath12k_vendor_spectral_get_status,
		.policy         = ath12k_spectral_scan_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
	},
#endif /* CPTCFG_ATH12K_SPECTRAL */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd    = QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE,
		.doit           = ath12k_vendor_rf_path_mode_handler,
		.policy         = ath12k_vendor_rf_path_mode_policy,
		.maxattr        = QCA_WLAN_VENDOR_ATTR_RF_PATH_MODE_MAX,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_MULTI_BSS_PARAM,
		.doit = ath12k_vendor_set_multi_bss_param,
		.policy = ath12k_multi_bss_param_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_MULTI_BSS_PARAM_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GREEN_AP,
		.doit = ath12k_vendor_green_ap_handler,
		.policy = ath12k_vendor_green_ap_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_GREEN_AP_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV | WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PASN,
		.doit = ath12k_vendor_pasn_cmd,
		.policy = ath12k_vendor_pasn_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PASN_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LIST_PASN_PEER,
		.doit = ath12k_vendor_list_pasn_peer_cmd,
		.policy = ath12k_vendor_pasn_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_PASN_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_RX_PKT_PROTOCOL_TAG,
		.doit = ath12k_dp_rx_update_pdev_protocol_tag,
		.policy = ath12k_rx_pkt_protocol_tag_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_RX_FLOW_TAG_OP,
		.doit = ath12k_dp_rx_flow_tag_op,
		.policy = ath12k_rx_flow_tag_op_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_FSE_CCE_STATS_DUMP,
		.doit = ath12k_dp_rx_fse_cce_stats_dump,
		.policy = ath12k_fse_cce_stats_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV,
	},
#ifdef CPTCFG_QCN_EXTN
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_PARAMS,
		.doit = ath12k_vendor_mapc_peer_params_extn,
		.policy = ath12k_mapc_peer_params_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MAPC_COTDMA_TXOP_POLICY,
		.doit = ath12k_vendor_mapc_cotdma_txop_policy_extn,
		.policy = ath12k_mapc_cotdma_txop_policy_attr_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MAPC_PEER_GET_PARAMS,
		.doit = ath12k_vendor_mapc_peer_get_params_extn,
		.policy = ath12k_mapc_get_peer_params_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd   = QCA_NL80211_VENDOR_SUBCMD_MAPC_COTDMA_E2E_CONFIG,
		.doit = ath12k_vendor_cotdma_e2e_config_extn,
		.policy = ath12k_cotdma_e2e_config_policy,
		.maxattr = QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
	},
#endif /* CPTCFG_QCN_EXTN */
};

/**
 * ath12k_dp_rx_update_pdev_protocol_tag() - CCE per-protocol tag vendor cmd handler
 *
 * Programs firmware via WMI_PDEV_UPDATE_PKT_ROUTING_CMDID with USE_CCE2 (= 3)
 * so that every matched MSDU gets cce_metadata stamped in the rx_msdu_end TLV.
 * Also stores the mapping in pdev_dp->protocol_tag_map[] for stats lookup.
 */
static int
ath12k_dp_rx_update_pdev_protocol_tag(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_MAX + 1];
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_wmi_pkt_route_param param = {};
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	unsigned long links_map;
	unsigned int link_id;
	struct ath12k *ar;
	u32 proto_type;
	u16 tag_value = 0;
	u8 op_code;
	int ret = -ENODEV;

	if (!vif) {
		ath12k_err(NULL, "rx_protocol_tag: no vif for wdev\n");
		return -EINVAL;
	}

	ahvif = ath12k_vif_to_ahvif(vif);

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_MAX,
			data, data_len,
			ath12k_rx_pkt_protocol_tag_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "rx_protocol_tag: failed to parse NL attrs\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_PROTO_TYPE]) {
		ath12k_err(NULL, "rx_protocol_tag: missing required attrs\n");
		return -EINVAL;
	}

	/* OP_CODE is optional; old userspace omits it and always means ADD */
	op_code = tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_OP_CODE]
		  ? nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_OP_CODE])
		  : ATH12K_WMI_PKTROUTE_ADD;
	proto_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_PROTO_TYPE]);

	if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
		if (!tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_TAG_VALUE]) {
			ath12k_err(NULL, "rx_protocol_tag: TAG_VALUE required for ADD\n");
			return -EINVAL;
		}
		tag_value = nla_get_u16(
			    tb[QCA_WLAN_VENDOR_ATTR_RX_PKT_PROTOCOL_TAG_TAG_VALUE]);
	} else if (op_code != ATH12K_WMI_PKTROUTE_DEL) {
		ath12k_err(NULL, "rx_protocol_tag: invalid op_code %u (0=ADD 1=DEL)\n",
			   op_code);
		return -EINVAL;
	}

	if (proto_type >= ATH12K_PKT_TYPE_MAX) {
		ath12k_err(NULL, "rx_protocol_tag: proto_type %u out of range (max %u)\n",
			   proto_type, ATH12K_PKT_TYPE_MAX - 1);
		return -EINVAL;
	}

	param.opcode           = op_code;
	param.route_type_bmap  = BIT(proto_type);
	param.dst_ring_handler = ATH12K_WMI_PKTROUTE_USE_FSE;
	param.dst_ring         = 1;
	/* Encode proto_type in meta_data (PROP approach): firmware stamps
	 * proto_type + offset in cce_metadata; RX path decodes proto_type with
	 * O(1) subtraction and retrieves the full u16 user tag from the host
	 * software map — no hardware width constraint on the tag value.
	 * Exception: CCE_DROP (0xDEAD) is sent as-is so firmware drops matched pkts.
	 */
	if (tag_value == CCE_DROP)
		param.meta_data = CCE_DROP;
	else
		param.meta_data = proto_type + ATH12K_RX_PROTOCOL_TAG_START_OFFSET;

	/* Program CCE rule on every active link's pdev. Each band has its own
	 * ar/pdev and WMI channel to firmware — deflink alone covers 2G only
	 * and leaves 5G/6G pdefs unconfigured in an MLD setup.
	 */
	links_map = ahvif->links_map;
	for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
		if (!arvif || !arvif->ar)
			continue;

		ar = arvif->ar;
		ret = ath12k_wmi_send_pdev_pkt_route(ar, &param);
		if (ret) {
			ath12k_warn(ar->ab,
				    "rx_protocol_tag: WMI failed link%u op=%u proto=%u tag=0x%04x ret=%d\n",
				    link_id, op_code, proto_type, tag_value, ret);
			continue;
		}

		dp_pdev = &ar->dp;
		if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
			if (!dp_pdev->protocol_tag_map[proto_type].enabled)
				dp_pdev->protocol_tag_active_count++;
			dp_pdev->protocol_tag_map[proto_type].tag     = tag_value;
			dp_pdev->protocol_tag_map[proto_type].enabled = true;
		} else {
			if (dp_pdev->protocol_tag_map[proto_type].enabled) {
				dp_pdev->protocol_tag_map[proto_type].tag     = 0;
				dp_pdev->protocol_tag_map[proto_type].enabled = false;
				dp_pdev->protocol_tag_active_count--;
			}
		}
	}

	return ret;
}

/**
 * ath12k_dp_rx_flow_tag_op() - FSE flow-tag add/delete vendor cmd handler
 *
 * Parses the 5-tuple + tag/drop/ring_id from NL attrs, populates
 * struct rx_flow_info, and calls ath12k_dp_rx_flow_add_entry() or
 * ath12k_dp_rx_flow_delete_entry().
 */
static int
ath12k_dp_rx_flow_tag_op(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_MAX + 1];
	struct ath12k_base *ab_list[ATH12K_MAX_SOCS] = {};
	struct rx_flow_info flow_info = {};
	struct hal_flow_tuple_info *fti = &flow_info.flow_tuple_info;
	u8 op_code, ip_ver, proto;
	int ret = 0;

	/* FST is per hw_group; collect ab_list just to get a valid ab pointer. */
	if (!ath12k_core_get_ab_list_by_wiphy(wiphy, ab_list, ATH12K_MAX_SOCS) ||
	    !ab_list[0]) {
		ath12k_err(NULL, "rx_flow_tag_op: no SoC found for this wiphy\n");
		return -ENODEV;
	}

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_MAX,
			data, data_len,
			ath12k_rx_flow_tag_op_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "rx_flow_tag_op: failed to parse NL attrs\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_OP_CODE] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_IP_VER]  ||
	    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_PORT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_PORT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_PROTO]) {
		ath12k_err(NULL, "rx_flow_tag_op: missing required attrs\n");
		return -EINVAL;
	}

	op_code = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_OP_CODE]);
	if (op_code > 1) {
		ath12k_err(NULL, "rx_flow_tag_op: invalid op_code %u (0=ADD 1=DEL)\n",
			   op_code);
		return -EINVAL;
	}

	ip_ver = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_IP_VER]);
	if (ip_ver != 4 && ip_ver != 6) {
		ath12k_err(NULL, "rx_flow_tag_op: ip_ver must be 4 or 6\n");
		return -EINVAL;
	}

	flow_info.is_addr_ipv4 = (ip_ver == 4);

	if (ip_ver == 4) {
		if (!tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV4] ||
		    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV4]) {
			ath12k_err(NULL, "rx_flow_tag_op: missing IPv4 addrs\n");
			return -EINVAL;
		}
		fti->src_ip_31_0  =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV4]);
		fti->dest_ip_31_0 =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV4]);
		/* upper words stay zero for IPv4 */
	} else {
		const u8 *src_v6, *dst_v6;

		if (!tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV6] ||
		    !tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV6]) {
			ath12k_err(NULL, "rx_flow_tag_op: missing IPv6 addrs\n");
			return -EINVAL;
		}
		src_v6 = nla_data(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_IPV6]);
		dst_v6 = nla_data(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_IPV6]);

		fti->src_ip_127_96 = get_unaligned_be32(src_v6);
		fti->src_ip_95_64  = get_unaligned_be32(src_v6 + 4);
		fti->src_ip_63_32  = get_unaligned_be32(src_v6 + 8);
		fti->src_ip_31_0   = get_unaligned_be32(src_v6 + 12);

		fti->dest_ip_127_96 = get_unaligned_be32(dst_v6);
		fti->dest_ip_95_64  = get_unaligned_be32(dst_v6 + 4);
		fti->dest_ip_63_32  = get_unaligned_be32(dst_v6 + 8);
		fti->dest_ip_31_0   = get_unaligned_be32(dst_v6 + 12);
	}

	fti->src_port    = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_SRC_PORT]);
	fti->dest_port   = nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DST_PORT]);
	proto            = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_PROTO]);
	fti->l4_protocol = proto;

	if (op_code == 0 /* ADD */) {
		if (!tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_TAG]) {
			ath12k_err(NULL, "rx_flow_tag_op: TAG required for ADD\n");
			return -EINVAL;
		}
		flow_info.fse_metadata =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_TAG]);
		flow_info.drop         = !!tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_DROP];

		if (tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_RING_ID]) {
			flow_info.ring_id =
			nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_RX_FLOW_TAG_RING_ID]);
		}

		ret = ath12k_dp_rx_flow_add_entry(ab_list[0], &flow_info);
		if (ret)
			ath12k_warn(ab_list[0],
				    "rx_flow_tag_op: add failed ret=%d\n", ret);
	} else /* DEL */ {
		ret = ath12k_dp_rx_flow_delete_entry(ab_list[0], &flow_info);
		if (ret)
			ath12k_warn(ab_list[0],
				    "rx_flow_tag_op: del failed ret=%d\n", ret);
	}

	return ret;
}

/**
 * ath12k_dp_rx_fse_cce_stats_dump() - Dump FSE/CCE statistics via NL vendor cmd.
 *
 * Reads from dp_pdev->fse_cce_stats (per-pdev CCE/FSE counters) and
 * ab->dp_hw_grp->fst (FST table state) and returns them in the NL reply.
 */
static int
ath12k_dp_rx_fse_cce_stats_dump(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_MAX + 1];
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct ath12k_fse_cce_stats *st;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	unsigned long links_map;
	unsigned int link_id;
	struct dp_rx_fst *fst;
	struct ath12k *ar;
	struct sk_buff *skb;
	int ret, i;

	ret = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_MAX,
			data, data_len,
			ath12k_fse_cce_stats_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "fse_cce_stats: nla_parse failed: %d\n", ret);
		return ret;
	}

	/* Resolve ar from wdev.  If PDEV_ID attr is present, treat its value as
	 * an MLD link_id and return data for that specific link — this lets
	 * ath-tools iterate per-link by calling the same cmd with link_id=0,1,...
	 * without any NL schema change.  If the requested link is not active,
	 * return -ENODEV so the caller knows to stop iterating.
	 * Without PDEV_ID: return the first active link (backward-compatible).
	 */
	if (!vif) {
		ath12k_err(NULL, "fse_cce_stats: no vif for wdev\n");
		return -EINVAL;
	}
	ahvif     = ath12k_vif_to_ahvif(vif);
	links_map = ahvif->links_map;

	if (tb[QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_PDEV_ID]) {
		u32 req_link =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_PDEV_ID]);

		if (req_link >= IEEE80211_MLD_MAX_NUM_LINKS ||
		    !test_bit(req_link, &links_map))
			return -ENODEV;
		link_id = req_link;
	} else {
		link_id = find_first_bit(&links_map, IEEE80211_MLD_MAX_NUM_LINKS);
	}

	if (link_id < IEEE80211_MLD_MAX_NUM_LINKS)
		arvif = wiphy_dereference(wiphy, ahvif->link[link_id]);
	else
		arvif = &ahvif->deflink;
	if (!arvif || !arvif->ar) {
		ath12k_err(NULL, "fse_cce_stats: no ar for vif\n");
		return -EINVAL;
	}
	ar = arvif->ar;

	st  = &ar->dp.fse_cce_stats;
	fst = ath12k_ab_to_dp(ar->ab)->dp_hw_grp->fst;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, NLMSG_DEFAULT_SIZE);
	if (!skb)
		return -ENOMEM;

	/* Per-protocol CCE tagged-pkt counters: attrs 2..21 */
	for (i = 0; i < ATH12K_PKT_TYPE_MAX; i++) {
		if (nla_put_u64_64bit(skb,
				      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_CCE_TAGGED_PKTS_0
				      + i,
				      st->cce_tagged_pkts[i],
				      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID))
			goto fail;
	}

	if (nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_CCE_DROP_PKTS,
			      st->cce_drop_pkts,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FSE_TAGGED_PKTS,
			      st->fse_tagged_pkts,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FSE_DROP_PKTS,
			      st->fse_drop_pkts,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FSE_NEW_FLOW,
			      st->fse_new_flow_pkts,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_REO_CCE_DROP,
			      st->reo_err_cce_drop,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_WBM_CCE_DROP,
			      st->wbm_reo_cce_drop,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID) ||
	    nla_put_u64_64bit(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_WBM_FSE_DROP,
			      st->wbm_rxdma_fse_drop,
			      QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_INVALID))
		goto fail;

	if (fst) {
		if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FST_NUM_ENTRIES,
				fst->num_entries) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FST_IPV4_CNT,
				fst->ipv4_fse_rule_cnt) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FST_IPV6_CNT,
				fst->ipv6_fse_rule_cnt) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FST_ADD_FAIL,
				fst->flow_add_fail) ||
		    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_FST_DEL_FAIL,
				fst->flow_del_fail))
			goto fail;
	}

	/* Per-protocol CCE tag map: 20 entries, each as packed u32 (tag | enabled<<16) */
	for (i = 0; i < ATH12K_PKT_TYPE_MAX; i++) {
		u32 val = (u32)ar->dp.protocol_tag_map[i].tag |
			  (ar->dp.protocol_tag_map[i].enabled ? BIT(16) : 0);

		if (nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_FSE_CCE_STATS_CCE_PROTO_TAG_0 + i,
				val))
			goto fail;
	}

	return cfg80211_vendor_cmd_reply(skb);

fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

static const struct nl80211_vendor_cmd_info ath12k_vendor_events[] = {
	[QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_POWER_MODE_CHANGE_COMPLETED
	},
	[QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT] = {
	      .vendor_id = QCA_NL80211_VENDOR_ID,
	      .subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV,
	},
        [QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX] = {
                .vendor_id = QCA_NL80211_VENDOR_ID,
                .subcmd = QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD
        },
	[QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS,
	},
	[QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
	},
	[QCA_NL80211_VENDOR_SUBCMD_ESP_ESTIMATE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_FW_RECOVERY_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_HOME_OFFCHAN_TX_RX,
	},
	[QCA_NL80211_VENDOR_SUBCMD_DCS_INTERFERENCE_COMPUTE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCS_CONFIG,
	},
	[QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR_INDEX] = {
	      .vendor_id = QCA_NL80211_VENDOR_ID,
	      .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTENDED_MONITOR,
	},
	[QCA_NL80211_VENDOR_SUBCMD_TPC_EIRP_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_TPC_EIRP_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_OEM_DATA_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_OEM_DATA,
	},
	[QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_HW_BLOCKED_CHANS,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
	},
	[QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_CH_SWITCH_REASON,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_PEER_MSDUQ_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION,
	},
	[QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_COMPLETE,
	},
	[QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_WLAN_NFCAL_POWER_EVENT,
	},
	[QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd    = QCA_NL80211_VENDOR_SUBCMD_RF_PATH_MODE,
	},
	[QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCAN_RADIO_CHAN_STATS,
	},
	[QCA_NL80211_VENDOR_SUBCMD_PASN_EVENT_INDEX] = {
		.vendor_id = QCA_NL80211_VENDOR_ID,
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_PASN,
	},
};

int ath12k_vendor_register(struct ath12k_hw *ah)
{
	ah->hw->wiphy->vendor_commands = ath12k_vendor_commands;
	ah->hw->wiphy->n_vendor_commands = ARRAY_SIZE(ath12k_vendor_commands);
	ah->hw->wiphy->vendor_events = ath12k_vendor_events;
	ah->hw->wiphy->n_vendor_events = ARRAY_SIZE(ath12k_vendor_events);
	return 0;
}
