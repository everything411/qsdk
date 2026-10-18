/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_VENDOR_H
#define ATH12K_VENDOR_H

#include "qca-vendor.h"

#define QCA_NL80211_VENDOR_ID 0x001374

#define INVALID_LINK_ID 0xFF
#define is_valid_link_id(X) !((X) >= INVALID_LINK_ID)

#define QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE          5000
#define QCA_WLAN_AFC_RESP_DESC_FIELD_START_OCTET       14
#define QCA_WLAN_AFC_RESP_DESC_FIELD_END_OCTET         30
#define ATF_OFFLOAD_MAX_PAYLOAD                                2048
#define ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT              30
#define ATH12K_ATF_MAX_PEERS				512

#define QCA_VENDOR_WLAN_TELEMETRY_LEGACY_MCS_MAX     12
#define QCA_VENDOR_WLAN_TELEMETRY_VHT_MCS_MAX        10
#define QCA_VENDOR_WLAN_TELEMETRY_HT_MCS_MAX         32
#define QCA_VENDOR_WLAN_TELEMETRY_HE_MCS_MAX         14
#define QCA_VENDOR_WLAN_TELEMETRY_EHT_MCS_MAX        16
#define QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX         24	/* UHR (11BN) */
#define QCA_VENDOR_WLAN_TELEMETRY_MCS_MAX \
	(QCA_VENDOR_WLAN_TELEMETRY_UHR_MCS_MAX + 1)

#define QCA_VENDOR_WLAN_OEM_DATA_BUF_MAX_SIZE		5120

#define QCA_VENDOR_WLAN_TELEMETRY_DATA_TIDS 9

#define INVALID_RADIO_INDEX 0xFF

#define ATH12K_MGMT_TX_RETRY_LIMIT_MIN 1
#define ATH12K_MGMT_TX_RETRY_LIMIT_MAX 14

#define ATH12K_IPV4_ADDR_LEN         4
#define ATH12K_IPV6_ADDR_LEN         16

#define ATH12K_MAX_PCP	7
#define ATH12K_MAX_TID	7

#define ATH12K_SECURE_RANGING_LTF_KEYSEED_MAX_LEN 48

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
extern unsigned int ath12k_ppe_ds_enabled;
#endif
struct ath12k;
struct ath12k_hw;
struct ath12k_afc_info;
struct ath12k_afc_host_request;
struct ath12k_base;
struct ath12k_hw_group;

struct ath12k_wifi_generic_params {
	u32 command;
	u32 value;
	void *data;
	u32 data_len;
	u32 length;
	u32 flags;
	u32 ifindex;
	u8 link_id;
	u8 radio_idx;
};

struct atf_peer_stat {
	u8 addr[ETH_ALEN];
	u32 atf_actual_airtime;
	u32 atf_peer_conf_airtime;
	u8 atf_group_index;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;
};

/* ME List Entry Structure */
struct ieee80211_wlanconfig_me_list {
	union {
		u_int32_t ip;			/* IPv4 address */
		u_int32_t ipv6[4];		/* IPv6 address (128-bit) */
	};
	u_int32_t mask;				/* Mask or Prefix for IPv4 or IPv6 */
	enum ieee80211_me_list me_list_type;	/* ME List type */
};

struct oem_vendor_build {
	u8 l_radio_id;
	__le32 l_content_type;
	__le32 l_num_bytes_valid;
	u8 l_data[];
} __packed;

struct ath12k_vendor_ch_switch_attrs {
	u32 freq;
	u32 width;
};

/**
 * ath12k_vendor_send_power_update_complete - Send 6 GHz power update complete
 * vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @afc - Pointer to AFC info
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_vendor_send_power_update_complete(struct ath12k *ar,
					 struct ath12k_afc_info *afc);

/**
 * ath12k_send_afc_request - Send AFC request vendor NL event to the application
 * @ar: Pointer to ath12k structure
 * @afc_req: Pointer to AFC host request
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_send_afc_request(struct ath12k *ar, struct ath12k_afc_host_request *afc_req);

/**
 * ath12k_send_afc_payload_reset - Send AFC payload reset vendor NL event
 * to userspace.
 *
 * @ar: Pointer to ar
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_send_afc_payload_reset(struct ath12k *ar);

/**
 * Opclass, channel and EIRP information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO
 * to understand the minimum length calculation.
 */
#define QCA_WLAN_AFC_RESP_OPCLASS_CHAN_EIRP_INFO_MIN_LEN       \
	NLA_ALIGN((NLA_HDRLEN + sizeof(uint8_t)) +              \
		  ((3 * NLA_HDRLEN) + sizeof(uint8_t) +         \
		   sizeof(uint32_t)))

/**
 * Frequency/PSD information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO
 * to understand the minimum length calculation.
 */
#define QCA_WLAN_AFC_RESP_FREQ_PSD_INFO_INFO_MIN_LEN           \
	NLA_ALIGN((3 * NLA_HDRLEN) +                            \
		  (2 * sizeof(uint16_t)) +                      \
		  sizeof(uint32_t))

/* SDWF MSDUQ dimension constants.
 * QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX: number of QoS TIDs (= QOS_TID_MAX).
 * QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX: number of MSDUQs per TID
 *     (= QOS_TID_MDSUQ_MAX).
 */
#define QCA_WLAN_VENDOR_ATTR_TELE_SDWF_TID_MAX		8
#define QCA_WLAN_VENDOR_ATTR_TELE_SDWF_MSDUQ_PER_TID_MAX	2

/**
 * ath12k_vendor_send_6ghz_power_mode_update_complete - Send 6 GHz power mode
 * update vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @wdev - Pointer to wdev
 * @link_id - Link ID for which the power mode update is complete
 */
int
ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						   struct wireless_dev *wdev,
						   u8 link_id);

/**
 * Alias for BE (802.11be final standard name) to EHT (draft name).
 * Both refer to the same technology.
 */
#define QCA_VENDOR_ATTR_RATE_STATS_BE_CNT \
	QCA_VENDOR_ATTR_RATE_STATS_EHT_CNT

#define ATH12K_VENDOR_PUT(vendor_event, type, attr, param)             \
	do {                                                            \
		if (nla_put_##type(vendor_event, attr, param)) {        \
			ath12k_err(NULL, "Fails to put " #attr "\n");   \
			return -1;                                      \
		}                                                       \
	} while (0)

enum qca_wlan_vendor_channel_width
ath12k_nl_chan_bw_to_qca_vendor_chan_bw(enum nl80211_chan_width chan_bw);

int ath12k_vendor_put_ar_hw_link_id(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ar_link_mac_addr(struct sk_buff *vendor_event,
				       struct ath12k *ar);
int ath12k_vendor_put_ar_chan_info(struct sk_buff *vendor_event,
				   struct ath12k *ar);
int ath12k_vendor_put_ar_nss_chains(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ar_wireless_mode(struct sk_buff *vendor_event,
				       struct ath12k *ar);
int ath12k_vendor_put_ab_soc_id(struct sk_buff *vendor_event,
				struct ath12k_base *ab);
int ath12k_vendor_put_ab_num_links(struct sk_buff *vendor_event,
				   struct ath12k_base *ab,
				   const int num_active_links);
int ath12k_vendor_put_ag_num_socs(struct sk_buff *vendor_event,
				  struct ath12k_hw_group *ag);
void ath12k_vendor_telemetry_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 svc_id, u8 param, bool set_clear,
					   u8 tid, u8 *mld_addr);
void ath12k_vendor_rssi_rate_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 breach_type, u32 threshold_value,
					   u32 detected_value, bool set_clear,
					   u8 *mld_addr);
int ath12k_vendor_send_es_oem_data(struct ieee80211_hw *hw, u8 radio_id, u32 content_type,
				   u32 num_bytes_valid, const u8 *data);
int ath12k_vendor_send_pasn_event(struct wiphy *wiphy, struct wireless_dev *wdev,
				  u8 link_id, u32 action, const u8 *src_addr,
				  const u8 *peer_addr, bool ltf_keyseed_required);
int ath12k_vendor_register(struct ath12k_hw *ah);
int ath12k_vendor_put_umac_migration_notif(struct ieee80211_vif *vif,
					   u8 *mld_addr, u8 link_id);
int ath12k_vendor_ch_switch_reason_notify(struct ath12k *ar,
					  enum qca_wlan_vendor_ch_switch_reason reason,
					  const struct cfg80211_chan_def *old_chandef,
					  const struct cfg80211_chan_def *new_chandef);

/**
 * struct ath12k_sdwf_msduq_evt_data - Per-peer MSDUQ vendor event payload.
 *
 * @hw_link_id:       Hardware link ID of the radio the peer is associated on.
 * @link_mac:         Link MAC address of the peer.
 * @mld_mac:          MLD MAC address (valid only when @mlo is true).
 * @mlo:              True when the peer is an MLO peer.
 * @msduq_id:         MSDUQ identifier within the peer's QoS context.
 * @svc_id:           Service class ID assigned to this MSDUQ.
 * @svc_type:         Service class type (DL=0, UL=1).
 * @priority:         Service class priority.
 * @tid:              Traffic identifier mapped to this MSDUQ.
 * @ac:               Access category mapped to this MSDUQ.
 * @mark_metadata:    SKB mark/metadata value for this flow.
 * @service_interval: Service interval in milliseconds.
 * @burst_size:       Burst size in bytes.
 * @delay_bound:      Delay bound in milliseconds.
 * @min_throughput:   Minimum throughput in kbps.
 * @peer_id:          Peer ID used to look up the peer.
 */
struct ath12k_sdwf_msduq_evt_data {
	u16 hw_link_id;
	u8 link_mac[ETH_ALEN];
	u8 mld_mac[ETH_ALEN];
	bool mlo;
	u8 msduq_id;
	u8 svc_id;
	u8 svc_type;
	u8 priority;
	u8 tid;
	u8 ac;
	u32 mark_metadata;
	u32 service_interval;
	u32 burst_size;
	u32 delay_bound;
	u32 min_throughput;
	u16 peer_id;
};

void
ath12k_vendor_sdwf_msduq_send_event(struct ath12k *ar,
				    const struct ath12k_sdwf_msduq_evt_data *e);
struct ath12k_wmi_nfcal_power_event;
int ath12k_vendor_nfcal_power_event(struct ath12k *ar,
				    const struct ath12k_wmi_nfcal_power_event *param);

/**
 * ath12k_vendor_event_chain_mask_changed() - Notify userspace of a chain mask change.
 * @ar: per-radio ath12k context for which the chain mask was modified.
 * @ifindex: netdev ifindex that initiated the chain mask update, or -1.
 *
 * Allocates and sends a %QCA_NL80211_VENDOR_SUBCMD_CHAIN_MASK_CHANGED vendor
 * event over nl80211 so that userspace applications are notified whenever
 * the TX or RX chain mask is updated
 *
 * Context: Any context. GFP_ATOMIC allocation; safe to call from non-sleepable
 *          paths.
 */
void ath12k_vendor_event_chain_mask_changed(struct ath12k *ar, int ifindex);

struct ath12k_cotdma_e2e_entry {
	u8   config_mode;
	u16  qmid;
	u8   mapc_peer_mac[ETH_ALEN];
	bool bsta_mac_valid;
	u8   bsta_mac[ETH_ALEN];
};

#define QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(submode) \
	((submode) == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_MESH || \
	 (submode) == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_ETH_OFFLOAD_MESH ||\
	 (submode) == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_RAW_MODE_MESH)

#endif
