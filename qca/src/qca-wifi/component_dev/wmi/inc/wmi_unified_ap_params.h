/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* This file has WIN specific WMI structures and macros. */

#ifndef _WMI_UNIFIED_AP_PARAMS_H_
#define _WMI_UNIFIED_AP_PARAMS_H_

#ifdef WLAN_FEATURE_11BE_MLO
#include <wmi_unified_ap_11be_params.h>
#endif

/* Country code string length*/
#define COUNTRY_CODE_LEN 2
/* Civic information size in bytes */
#define CIVIC_INFO_LEN 256

/* Same as MAX_20MHZ_SEGMENTS */
#define MAX_20MHZ_SEGS 16

/* Same as MAX_ANTENNA_EIGHT */
#define MAX_NUM_ANTENNA 8

#define DELAY_BOUND_ULTRA_LOW 10
#define DELAY_BOUND_LOW 100
#define DELAY_BOUND_MID 200
#define DELAY_BOUND_HIGH 300
#define SVC_INTERVAL_ULTRA_LOW 20
#define SVC_INTERVAL_LOW 50
#define TIME_TO_LIVE_ULTRA_LOW 20
#define TIME_TO_LIVE_LOW 200
#define TIME_TO_LIVE_MID 250
#define SAWF_TID_INFER_LESS_THAN(param, threshold) \
	p_cmd->param < threshold

enum wmi_sched_disabled  {
	SCHED_MODE_DL_MU_MIMO,
	SCHED_MODE_UL_MU_MIMO,
	SCHED_MODE_DL_OFDMA,
	SCHED_MODE_UL_OFDMA,
	SCHED_MODE_INV_DMODE,
};

/**
 * wmi_wifi_pos_lcr_info - LCR info structure
 * @pdev_id: host pdev id
 * @req_id: LCR request id
 * @civic_len: Civic info length
 * @country_code: Country code string
 * @civic_info: Civic info
 */
struct wmi_wifi_pos_lcr_info {
	uint32_t pdev_id;
	uint16_t req_id;
	uint32_t civic_len;
	uint8_t  country_code[COUNTRY_CODE_LEN];
	uint8_t civic_info[CIVIC_INFO_LEN];
};

/**
 * lci_motion_pattern - LCI motion pattern
 * @LCI_MOTION_NOT_EXPECTED: Not expected to change location
 * @LCI_MOTION_EXPECTED: Expected to change location
 * @LCI_MOTION_UNKNOWN: Movement pattern unknown
 */
enum wifi_pos_motion_pattern {
	wifi_pos_MOTION_NOT_EXPECTED = 0,
	wifi_pos_MOTION_EXPECTED     = 1,
	wifi_pos_MOTION_UNKNOWN      = 2
};

/**
 * wifi_pos_lci_info - LCI info structure
 * @pdev_id: host pdev_id
 * @req_id: LCI request id
 * @latitude: Latitude value
 * @longitude: Longitude value
 * @altitude: Altitude value
 * @latitude_unc: Latitude uncertainty value
 * @longitude_unc: Longitude uncertainty value
 * @altitude_unc: Altitude uncertainty value
 * @motion_pattern: Motion pattern
 * @floor: Floor value
 * @height_above_floor: Height above floor
 * @height_unc: Height uncertainty value
 * @usage_rules: Usage rules
 */
struct wifi_pos_lci_info {
	uint32_t pdev_id;
	uint16_t req_id;
	int64_t latitude;
	int64_t longitude;
	int32_t altitude;
	uint8_t latitude_unc;
	uint8_t longitude_unc;
	uint8_t altitude_unc;
	enum wifi_pos_motion_pattern motion_pattern;
	int32_t floor;
	int32_t height_above_floor;
	int32_t height_unc;
	uint32_t usage_rules;
};

/**
 * struct peer_add_wds_entry_params - WDS peer entry add params
 * @dest_addr: Pointer to destination macaddr
 * @peer_addr: Pointer to peer mac addr
 * @flags: flags
 * @vdev_id: Vdev id
 */
struct peer_add_wds_entry_params {
	const uint8_t *dest_addr;
	uint8_t *peer_addr;
	uint32_t flags;
	uint32_t vdev_id;
};

/**
 * struct peer_del_wds_entry_params - WDS peer entry del params
 * @dest_addr: Pointer to destination macaddr
 * @vdev_id: Vdev id
 */
struct peer_del_wds_entry_params {
	uint8_t *dest_addr;
	uint32_t vdev_id;
};

/**
 * struct peer_del_multi_wds_entry_val - List element of the wds del param
 * @dest_addr: destination macaddr
 * @type: AST type
 */
struct peer_del_multi_wds_entry_val {
	uint8_t dest_addr[QDF_MAC_ADDR_SIZE];
	uint8_t type;
};

/**
 * struct peer_del_multi_wds_entry_params - multi WDS peer entry del params
 * to be sent to WMI
 * @vdev_id: vdev id for FW reference
 * @pdev_id: pdev id for which multi ast delete is sent
 * @num_entries: Number of entries in the dest list
 * @dest_list: List to hold the mac addresses to be deleted
 */
struct peer_del_multi_wds_entry_params {
	uint8_t vdev_id;
	uint8_t pdev_id;
	uint16_t num_entries;
	struct peer_del_multi_wds_entry_val *dest_list;
};

/**
 * struct peer_wds_entry_list - multi WDS peer entry del list
 * @dest_addr: destination macaddr
 * @type: AST type
 * @delete_in_fw: Set if this mac is supposed to be deleted at FW
 * @ase_list_elem: List element to hold list entries.
 */
struct peer_wds_entry_list {
	uint8_t *dest_addr;
	uint8_t type;
	uint8_t delete_in_fw;
	TAILQ_ENTRY(peer_wds_entry_list) ase_list_elem;
};

/**
 * struct peer_del_multi_wds_entries - multi WDS peer entry del params
 * @vdev_id: vdev id
 * @num_entries: Number of entries in the ase list
 * @ase_list: List to hold the AST entries to be deleted
 */
struct peer_del_multi_wds_entries {
	uint8_t vdev_id;
	uint16_t num_entries;
	TAILQ_HEAD(, peer_wds_entry_list) ase_list;
};

/**
 * struct peer_update_wds_entry_params - WDS peer entry update params
 * @wds_macaddr: Pointer to destination macaddr
 * @peer_add: Pointer to peer mac addr
 * @flags: flags
 * @vdev_id: Vdev id
 */
struct peer_update_wds_entry_params {
	uint8_t *wds_macaddr;
	uint8_t *peer_macaddr;
	uint32_t flags;
	uint32_t vdev_id;
};

/**
 * struct ctl_table_params - Ctl table params
 * @ctl_array: pointer to ctl array
 * @ctl_cmd_len: ctl command length
 * @is_2g: is 2G
 * @target_type: target type
 * @ctl_band: ctl band
 * @pdev_id: pdev id
 */
struct ctl_table_params {
	uint8_t *ctl_array;
	uint16_t ctl_cmd_len;
	bool is_2g;
	uint32_t target_type;
	uint32_t ctl_band;
	uint32_t pdev_id;
};

/**
 * struct wlan_tdma_sched_cmd_param - TDMA entry update params
 * @pdev_id: pdev id
 * @schedule_type: schedule type
 * @schedule_handle_id: schedule id
 * @owner_bssid: BSSID or mac address
 * @start_time_tsf_low: Start time for tsf low
 * @start_time_tsf_high: Start time for tsf high
 * @num_busy_slots: Number of busy slots
 * @busy_slot_dur_ms: Busy slots
 * @busy_slot_intvl_ms: Interval for busy slots
 * @edca_params_valid: edca Param valid check
 * @aifsn: aifsn
 * @cwmin: cwmin
 * @cwmax: cwmax
 */
struct wlan_tdma_sched_cmd_param {
	uint32_t pdev_id;
	uint8_t schedule_type;
	uint8_t schedule_handle_id;
	uint8_t  owner_bssid[QDF_MAC_ADDR_SIZE];
	uint32_t start_time_tsf_low;
	uint32_t start_time_tsf_high;
	uint16_t num_busy_slots;
	uint16_t busy_slot_dur_ms;
	uint16_t busy_slot_intvl_ms;
	bool edca_params_valid;
	uint16_t aifsn[WMI_MAX_NUM_AC];
	uint16_t cwmin[WMI_MAX_NUM_AC];
	uint16_t cwmax[WMI_MAX_NUM_AC];
};

/**
 * struct wlan_link_recmnd_param - Link Recommendation update params
 * @vdev_id: vdev id
 * @assoc_id: aid (association id) of the station
 * @linkid_bitmap: link id
 */
struct wlan_link_recmnd_param {
	uint32_t vdev_id;
	uint32_t assoc_id;
	uint16_t linkid_bitmap;
};

/**
 * struct rate2power_table_params - Rate2Power table params
 * @pwr_array: pointer to pwr array
 * @pwr_cmd_len: rate2power command length
 * @freq_band: Frequency band
 * @sub_band: Sub band for the frequency band
 * @end_of_update: Flag to denote the end of update
 * @is_ext: is extension flag
 * @target_type: target type
 * @pdev_id: pdev id
 */
struct rate2power_table_params {
	int8_t   *pwr_array;
	uint16_t pwr_cmd_len;
	uint32_t freq_band;
	uint32_t sub_band;
	uint32_t end_of_update;
	uint32_t is_ext;
	uint32_t target_type;
	uint32_t pdev_id;
};

/**
 * struct sta_peer_table_list - peer list params
 * @peer_macaddr: array of STA MAC addresses
 * @peer_pwr_limit: Peer power limit
 */
struct sta_peer_table_list {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	int32_t peer_pwr_limit;
};

/**
 * struct sta_pwr_table_params - sta pwr table params
 * @num_peers: No.of stations
 * @peer_list: pointer to struct sta_peer_table_list
 */
struct sta_max_pwr_table_params {
	uint32_t num_peers;
	struct sta_peer_table_list *peer_list;
};

/**
 * struct mimogain_table_params - MIMO gain table params
 * @array_gain: pointer to array gain table
 * @tbl_len: table length
 * @multichain_gain_bypass: bypass multichain gain
 */
struct mimogain_table_params {
	uint8_t *array_gain;
	uint16_t tbl_len;
	bool multichain_gain_bypass;
	uint32_t pdev_id;
};

/**
 * struct packet_power_info_params - packet power info params
 * @chainmask: chain mask
 * @chan_width: channel bandwidth
 * @rate_flags: rate flags
 * @su_mu_ofdma: su/mu/ofdma flags
 * @nss: number of spatial streams
 * @preamble: preamble
 * @hw_rate:
 */
struct packet_power_info_params {
	uint16_t chainmask;
	uint16_t chan_width;
	uint16_t rate_flags;
	uint16_t su_mu_ofdma;
	uint16_t nss;
	uint16_t preamble;
	uint16_t hw_rate;
	uint32_t pdev_id;
};

/**
 * struct halphy_cal_status_params - Halphy can params info
 * @pdev_id: Pdev id
 */
struct halphy_cal_status_params {
	uint32_t pdev_id;
};

/* struct ht_ie_params - HT IE params
 * @ie_len: IE length
 * @ie_data: pointer to IE data
 * @tx_streams: Tx streams supported for this HT IE
 * @rx_streams: Rx streams supported for this HT IE
 */
struct ht_ie_params {
	uint32_t ie_len;
	uint8_t *ie_data;
	uint32_t tx_streams;
	uint32_t rx_streams;
};

/* struct vht_ie_params - VHT IE params
 * @ie_len: IE length
 * @ie_data: pointer to IE data
 * @tx_streams: Tx streams supported for this VHT IE
 * @rx_streams: Rx streams supported for this VHT IE
 */
struct vht_ie_params {
	uint32_t ie_len;
	uint8_t *ie_data;
	uint32_t tx_streams;
	uint32_t rx_streams;
};

/**
 * struct set_quiet_mode_params - Set quiet mode params
 * @enabled: Enabled
 * @period: Quite period
 * @intval: Quite interval
 * @duration: Quite duration
 * @offset: offset
 */
struct set_quiet_mode_params {
	uint8_t enabled;
	uint8_t period;
	uint16_t intval;
	uint16_t duration;
	uint16_t offset;
};

/**
 * struct set_bcn_offload_quiet_mode_params - Set quiet mode params
 * @vdev_id: Vdev ID
 * @period: Quite period
 * @duration: Quite duration
 * @next_start: Next quiet start
 * @flag: 0 - disable, 1 - enable and continuous, 3 - enable and single shot
 * @mlo_partner: Partner link information
 */
struct set_bcn_offload_quiet_mode_params {
	uint32_t vdev_id;
	uint32_t period;
	uint32_t duration;
	uint32_t next_start;
	uint32_t flag;
#ifdef WLAN_FEATURE_11BE_MLO
	struct wmi_mlo_bcn_offload_partner_links mlo_partner;
#endif
};

/**
 * struct bcn_offload_control - Beacon offload control params
 * @vdev_id: vdev identifer of VAP to control beacon tx
 * @bcn_ctrl_op: values from enum bcn_offload_control_param
 */
struct bcn_offload_control {
	uint32_t vdev_id;
	enum bcn_offload_control_param bcn_ctrl_op;
};

/**
 * struct wmi_host_tim_info - TIM info in SWBA event
 * @tim_len: TIM length
 * @tim_mcast:
 * @tim_bitmap: TIM bitmap
 * @tim_changed: TIM changed
 * @tim_num_ps_pending: TIM num PS sta pending
 * @vdev_id: Vdev id
 */
typedef struct {
	uint32_t tim_len;
	uint32_t tim_mcast;
	uint32_t tim_bitmap[WMI_HOST_TIM_BITMAP_ARRAY_SIZE];
	uint32_t tim_changed;
	uint32_t tim_num_ps_pending;
	uint32_t vdev_id;
} wmi_host_tim_info;

/* Maximum number of NOA Descriptors supported */
#define WMI_HOST_P2P_MAX_NOA_DESCRIPTORS 4
/**
 * struct wmi_host_p2p_noa_info - p2p noa information
 * @modified: NoA modified
 * @index: Index
 * @oppPS: Oppurtunstic ps
 * @ctwindow: CT window
 * @num_descriptors: number of descriptors
 * @noa_descriptors: noa descriptors
 * @vdev_id: Vdev id
 */
typedef struct {
	uint8_t modified;
	uint8_t index;
	uint8_t oppPS;
	uint8_t ctwindow;
	uint8_t num_descriptors;
	wmi_host_p2p_noa_descriptor
		noa_descriptors[WMI_HOST_P2P_MAX_NOA_DESCRIPTORS];
	uint32_t vdev_id;
} wmi_host_p2p_noa_info;

/**
 * struct wmi_host_quiet_info - Quiet info in SWBA event
 * @vdev_id: vdev_id for quiet info structure
 * @tbttcount: quiet start tbtt count
 * @period: Beacon interval between quiets
 * @duration: TUs of each quiet
 * @offset: TUs from TBTT to quiet start
 */
typedef struct {
	uint32_t vdev_id;
	uint32_t tbttcount;
	uint32_t period;
	uint32_t duration;
	uint32_t offset;
} wmi_host_quiet_info;

/**
 * struct wmi_host_offchan_data_tx_compl_event - TX completion event
 * @desc_id: from tx_send_cmd
 * @status: VWMI_MGMT_TX_COMP_STATUS_TYPE
 * @pdev_id: pdev_id
 */
struct wmi_host_offchan_data_tx_compl_event {
	uint32_t desc_id;
	uint32_t status;
	uint32_t pdev_id;
};

/**
 * struct wmi_host_pdev_tpc_config_event - host pdev tpc config event
 * @pdev_id: pdev_id
 * @regDomain:
 * @chanFreq:
 * @phyMode:
 * @twiceAntennaReduction:
 * @twiceMaxRDPower:
 * @twiceAntennaGain:
 * @powerLimit:
 * @rateMax:
 * @numTxChain:
 * @ctl:
 * @flags:
 * @maxRegAllowedPower:
 * @maxRegAllowedPowerAGCDD:
 * @maxRegAllowedPowerAGSTBC:
 * @maxRegAllowedPowerAGTXBF:
 * @ratesArray:
 */
typedef struct {
	uint32_t pdev_id;
	uint32_t regDomain;
	uint32_t chanFreq;
	uint32_t phyMode;
	uint32_t twiceAntennaReduction;
	uint32_t twiceMaxRDPower;
	int32_t  twiceAntennaGain;
	uint32_t powerLimit;
	uint32_t rateMax;
	uint32_t numTxChain;
	uint32_t ctl;
	uint32_t flags;
	int8_t  maxRegAllowedPower[WMI_HOST_TPC_TX_NUM_CHAIN];
	int8_t  maxRegAllowedPowerAGCDD[WMI_HOST_TPC_TX_NUM_CHAIN][WMI_HOST_TPC_TX_NUM_CHAIN];
	int8_t  maxRegAllowedPowerAGSTBC[WMI_HOST_TPC_TX_NUM_CHAIN][WMI_HOST_TPC_TX_NUM_CHAIN];
	int8_t  maxRegAllowedPowerAGTXBF[WMI_HOST_TPC_TX_NUM_CHAIN][WMI_HOST_TPC_TX_NUM_CHAIN];
	uint8_t ratesArray[WMI_HOST_TPC_RATE_MAX];
} wmi_host_pdev_tpc_config_event;


#ifdef QCA_RSSI_DB2DBM
/**
 * struct rssi_temp_off_param
 * @rssi_temp_offset: RSSI Temperature offset.
 */
struct rssi_temp_off_param {
	int32_t rssi_temp_offset;
};

/**
 * struct rssi_dbm_conv_param
 * @curr_bw: Current bandwidth
 * @curr_rx_chainmask: Current Rx chain mask
 * @xbar_config: Xbar configuration
 * @xlna_bypass_offset: Xlna bypass offset
 * @xlna_bypass_threshold: Xlna bypass threshold
 * @nf_bw_dbm: Noise floor table.
 */
struct rssi_dbm_conv_param {
	uint32_t curr_bw;
	uint32_t curr_rx_chainmask;
	uint32_t xbar_config;
	int32_t xlna_bypass_offset;
	int32_t xlna_bypass_threshold;
	int8_t nf_hw_dbm[MAX_NUM_ANTENNA][MAX_20MHZ_SEGS];
};

/**
 * struct rssi_db2dbm_param
 * @pdev_id: PDEV ID sent by Target
 * @rssi_temp_off_present: Set to true if temperature offset info preset
 * @rssi_dbm_info_present: Set to true if XBAR and XLNA info present
 * @temp_off_param: Object of struct rssi_temp_off_param
 * @rssi_dbm_param: Object of struct rssi_dbm_conv_param
 */
struct rssi_db2dbm_param {
	uint32_t pdev_id;
	bool rssi_temp_off_present;
	bool rssi_dbm_info_present;
	struct rssi_temp_off_param temp_off_param;
	struct rssi_dbm_conv_param rssi_dbm_param;
};
#endif

/**
 * struct wmi_host_peer_sta_kickout_event
 * @peer_macaddr: peer mac address
 * @reason: kickout reason
 * @rssi: rssi
 * @pdev_id: pdev_id
 */
typedef struct {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint32_t reason;
	uint32_t rssi;
} wmi_host_peer_sta_kickout_event;

/**
 * struct wmi_host_peer_create_response_event - Peer Create response event param
 * @vdev_id: vdev id
 * @mac_address: Peer Mac Address
 * @status: Peer create status
 *
 */
struct wmi_host_peer_create_response_event {
	uint32_t vdev_id;
	struct qdf_mac_addr mac_address;
	uint32_t status;
};

/**
 * struct wmi_host_peer_delete_response_event - Peer Delete response event param
 * @vdev_id: vdev id
 * @mac_address: Peer Mac Address
 *
 */
struct wmi_host_peer_delete_response_event {
	uint32_t vdev_id;
	struct qdf_mac_addr mac_address;
};

/**
 * struct wmi_host_pdev_tpc_event - WMI host pdev TPC event
 * @pdev_id: pdev_id
 * @tpc:
 */
typedef struct {
	uint32_t pdev_id;
	int32_t tpc[WMI_HOST_TX_POWER_LEN];
} wmi_host_pdev_tpc_event;

/**
 * struct wmi_host_pdev_nfcal_power_all_channels_event - NF cal event data
 * @nfdbr:
 *   chan[0 ~ 7]: {NFCalPower_chain0, NFCalPower_chain1,
 *                 NFCalPower_chain2, NFCalPower_chain3,
 *                 NFCalPower_chain4, NFCalPower_chain5,
 *                 NFCalPower_chain6, NFCalPower_chain7},
 * @nfdbm:
 *   chan[0 ~ 7]: {NFCalPower_chain0, NFCalPower_chain1,
 *                 NFCalPower_chain2, NFCalPower_chain3,
 *                 NFCalPower_chain4, NFCalPower_chain5,
 *                 NFCalPower_chain6, NFCalPower_chain7},
 * @freqnum:
 *   chan[0 ~ 7]: frequency number
 * @pdev_id: pdev_id
 * @num_freq: number of valid frequency in freqnum
 * @num_nfdbr_dbm: number of valid entries in dbr/dbm array
 *
 */
typedef struct {
	int8_t nfdbr[WMI_HOST_RXG_CAL_CHAN_MAX * WMI_HOST_MAX_NUM_CHAINS];
	int8_t nfdbm[WMI_HOST_RXG_CAL_CHAN_MAX * WMI_HOST_MAX_NUM_CHAINS];
	uint32_t freqnum[WMI_HOST_RXG_CAL_CHAN_MAX];
	uint32_t pdev_id;
	uint16_t num_freq;
	uint16_t num_nfdbr_dbm;
} wmi_host_pdev_nfcal_power_all_channels_event;

/**
 * struct wds_addr_event - WDS addr event structure
 * @event_type: event type add/delete
 * @peer_mac: peer mac
 * @dest_mac: destination mac address
 * @vdev_id: vdev id
 */
typedef struct {
	uint32_t event_type[4];
	u_int8_t peer_mac[QDF_MAC_ADDR_SIZE];
	u_int8_t dest_mac[QDF_MAC_ADDR_SIZE];
	uint32_t vdev_id;
} wds_addr_event_t;

/**
 * struct wmi_host_peer_sta_ps_statechange_event - ST ps state change event
 * @peer_macaddr: peer mac address
 * @peer_ps_stats: peer PS state
 * @pdev_id: pdev_id
 */
typedef struct {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint32_t peer_ps_state;
} wmi_host_peer_sta_ps_statechange_event;

/**
 * struct wmi_host_inst_stats_resp
 * @iRSSI: Instantaneous RSSI
 * @peer_macaddr: peer mac address
 * @pdev_id: pdev_id
 */
typedef struct {
	uint32_t iRSSI;
	wmi_host_mac_addr peer_macaddr;
	uint32_t pdev_id;
} wmi_host_inst_stats_resp;

typedef struct {
	uint32_t software_cal_version;
	uint32_t board_cal_version;
	/* board_mcn_detail:
	 * Provide a calibration message string for the host to display.
	 * Note: on a big-endian host, the 4 bytes within each uint32_t portion
	 * of a WMI message will be automatically byteswapped by the copy engine
	 * as the messages are transferred between host and target, to convert
	 * between the target's little-endianness and the host's big-endianness.
	 * Consequently, a big-endian host should manually unswap the bytes
	 * within the board_mcn_detail string buffer to get the bytes back into
	 * the desired natural order.
	 */
	uint8_t board_mcn_detail[WMI_HOST_BOARD_MCN_STRING_BUF_SIZE];
	uint32_t cal_ok; /* filled with CALIBRATION_STATUS enum value */
} wmi_host_pdev_check_cal_version_event;

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
/**
 * enum wmi_pdev_pkt_routing_op_code_type - packet routing supported opcodes
 * @ADD_PKT_ROUTING: Add packet routing command
 * @DEL_PKT_ROUTING: Delete packet routing command
 *
 * Defines supported opcodes for packet routing/tagging
 */
enum wmi_pdev_pkt_routing_op_code_type {
	ADD_PKT_ROUTING,
	DEL_PKT_ROUTING,
};

/**
 * enum wmi_pdev_pkt_routing_pkt_type - supported packet types for
 * routing & tagging
 * @PDEV_PKT_TYPE_ARP_IPV4: Route/Tag for packet type ARP IPv4 (L3)
 * @PDEV_PKT_TYPE_NS_IPV6: Route/Tag for packet type NS IPv6 (L3)
 * @PDEV_PKT_TYPE_IGMP_IPV4: Route/Tag for packet type IGMP IPv4 (L3)
 * @PDEV_PKT_TYPE_MLD_IPV6: Route/Tag for packet type MLD IPv6 (L3)
 * @PDEV_PKT_TYPE_DHCP_IPV4: Route/Tag for packet type DHCP IPv4 (APP)
 * @PDEV_PKT_TYPE_DHCP_IPV6: Route/Tag for packet type DHCP IPv6 (APP)
 * @PDEV_PKT_TYPE_DNS_TCP_IPV4: Route/Tag for packet type TCP DNS IPv4 (APP)
 * @PDEV_PKT_TYPE_DNS_TCP_IPV6: Route/Tag for packet type TCP DNS IPv6 (APP)
 * @PDEV_PKT_TYPE_DNS_UDP_IPV4: Route/Tag for packet type UDP DNS IPv4 (APP)
 * @PDEV_PKT_TYPE_DNS_UDP_IPV6: Route/Tag for packet type UDP DNS IPv6 (APP)
 * @PDEV_PKT_TYPE_ICMP_IPV4: Route/Tag for packet type ICMP IPv4 (L3)
 * @PDEV_PKT_TYPE_ICMP_IPV6: Route/Tag for packet type ICMP IPv6 (L3)
 * @PDEV_PKT_TYPE_TCP_IPV4: Route/Tag for packet type TCP IPv4 (L4)
 * @PDEV_PKT_TYPE_TCP_IPV6: Route/Tag for packet type TCP IPv6 (L4)
 * @PDEV_PKT_TYPE_UDP_IPV4: Route/Tag for packet type UDP IPv4 (L4)
 * @PDEV_PKT_TYPE_UDP_IPV6: Route/Tag for packet type UDP IPv6 (L4)
 * @PDEV_PKT_TYPE_IPV4: Route/Tag for packet type IPv4 (L3)
 * @PDEV_PKT_TYPE_IPV6: Route/Tag for packet type IPv6 (L3)
 * @PDEV_PKT_TYPE_EAP: Route/Tag for packet type EAP (L2)
 *
 * Defines supported protocol types for routing/tagging
 */
enum wmi_pdev_pkt_routing_pkt_type {
	PDEV_PKT_TYPE_ARP_IPV4,
	PDEV_PKT_TYPE_NS_IPV6,
	PDEV_PKT_TYPE_IGMP_IPV4,
	PDEV_PKT_TYPE_MLD_IPV6,
	PDEV_PKT_TYPE_DHCP_IPV4,
	PDEV_PKT_TYPE_DHCP_IPV6,
	PDEV_PKT_TYPE_DNS_TCP_IPV4,
	PDEV_PKT_TYPE_DNS_TCP_IPV6,
	PDEV_PKT_TYPE_DNS_UDP_IPV4,
	PDEV_PKT_TYPE_DNS_UDP_IPV6,
	PDEV_PKT_TYPE_ICMP_IPV4,
	PDEV_PKT_TYPE_ICMP_IPV6,
	PDEV_PKT_TYPE_TCP_IPV4,
	PDEV_PKT_TYPE_TCP_IPV6,
	PDEV_PKT_TYPE_UDP_IPV4,
	PDEV_PKT_TYPE_UDP_IPV6,
	PDEV_PKT_TYPE_IPV4,
	PDEV_PKT_TYPE_IPV6,
	PDEV_PKT_TYPE_EAP,
	PDEV_PKT_TYPE_VLAN,
	PDEV_PKT_TYPE_MAX
};

/**
 * enum wmi_pdev_dest_ring_handler_type - packet routing options post CCE
 * tagging
 * @PDEV_WIFIRXCCE_USE_CCE_E: Use REO destination ring from CCE
 * @PDEV_WIFIRXCCE_USE_ASPT_E: Use REO destination ring from ASPT
 * @PDEV_WIFIRXCCE_USE_FT_E: Use REO destination ring from FSE
 * @PDEV_WIFIRXCCE_USE_CCE2_E: Use REO destination ring from CCE2
 *
 * Defines various options for routing policy
 */
enum wmi_pdev_dest_ring_handler_type {
	PDEV_WIFIRXCCE_USE_CCE_E  = 0,
	PDEV_WIFIRXCCE_USE_ASPT_E = 1,
	PDEV_WIFIRXCCE_USE_FT_E   = 2,
	PDEV_WIFIRXCCE_USE_CCE2_E = 3,
};

/**
 * struct wmi_rx_pkt_protocol_routing_info - RX packet routing/tagging params
 * @pdev_id: pdev id
 * @op_code: Opcode option from wmi_pdev_pkt_routing_op_code_type enum
 * @routing_type_bitmap: Bitmap of protocol that is being configured. Only
 * one protocol can be configured in one command. Supported protocol list
 * from enum wmi_pdev_pkt_routing_pkt_type
 * @dest_ring_handler: Destination ring selection from enum
 * wmi_pdev_dest_ring_handler_type
 * @dest_ring: Destination ring number to use if dest ring handler is CCE
 * @meta_data: Metadata to tag with for given protocol
 */
struct wmi_rx_pkt_protocol_routing_info {
	uint32_t      pdev_id;
	enum wmi_pdev_pkt_routing_op_code_type op_code;
	uint32_t      routing_type_bitmap;
	uint32_t      dest_ring_handler;
	uint32_t      dest_ring;
	uint32_t      meta_data;
};
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */

/**
 * struct fd_params - FD cmd parameter
 * @vdev_id: vdev id
 * @frame_ctrl: frame control field
 * @wbuf: FD buffer
 */
struct fd_params {
	uint8_t vdev_id;
	uint16_t frame_ctrl;
	qdf_nbuf_t wbuf;
};

/**
 * struct set_qbosst_params - Set QBOOST params
 * @vdev_id: vdev id
 * @value: value
 */
struct set_qboost_params {
	uint8_t vdev_id;
	uint32_t value;
};

/**
 * struct mcast_group_update_param - Mcast group table update to target
 * @action: Addition/deletion
 * @wildcard: iwldcard table entry?
 * @mcast_ip_addr: mcast ip address to be updated
 * @mcast_ip_addr_bytes: mcast ip addr bytes
 * @nsrcs: number of entries in source list
 * @filter_mode: filter mode
 * @is_action_delete: is delete
 * @is_filter_mode_snoop: is filter mode snoop
 * @ucast_mac_addr: ucast peer mac subscribed to mcast ip
 * @srcs: source mac accpted
 * @mask: mask
 * @vap_id: vdev id
 * @is_mcast_addr_len: is mcast address length
 */
struct mcast_group_update_params {
	int action;
	int wildcard;
	uint8_t *mcast_ip_addr;
	int mcast_ip_addr_bytes;
	uint8_t nsrcs;
	uint8_t filter_mode;
	bool is_action_delete;
	bool is_filter_mode_snoop;
	uint8_t *ucast_mac_addr;
	uint8_t *srcs;
	uint8_t *mask;
	uint8_t vap_id;
	bool is_mcast_addr_len;
};

struct pdev_qvit_params {
	uint8_t *utf_payload;
	uint32_t len;
};

/**
 * struct wmi_host_wmeParams - WME params
 * @wmep_acm: ACM paramete
 * @wmep_aifsn:	AIFSN parameters
 * @wmep_logcwmin: cwmin in exponential form
 * @wmep_logcwmax: cwmax in exponential form
 * @wmep_txopLimit: txopLimit
 * @wmep_noackPolicy: No-Ack Policy: 0=ack, 1=no-ack
 */
struct wmi_host_wmeParams {
	u_int8_t	wmep_acm;
	u_int8_t	wmep_aifsn;
	u_int8_t	wmep_logcwmin;
	u_int8_t	wmep_logcwmax;
	u_int16_t   wmep_txopLimit;
	u_int8_t	wmep_noackPolicy;
};

/**
 * struct wmm_update_params - WMM update params
 * @wmep_array: WME params for each AC
 */
struct wmm_update_params {
	struct wmi_host_wmeParams *wmep_array;
};

/**
 * struct wmi_host_mgmt_tx_compl_event - TX completion event
 * @desc_id: from tx_send_cmd
 * @status: WMI_MGMT_TX_COMP_STATUS_TYPE
 * @pdev_id: pdev_id
 * @ppdu_id: ppdu_id
 * @retries_count: retries count
 * @tx_tsf: 64 bits completion timestamp
 */
typedef struct {
	uint32_t	desc_id;
	uint32_t	status;
	uint32_t	pdev_id;
	uint32_t        ppdu_id;
	uint32_t	retries_count;
	uint64_t	tx_tsf;
} wmi_host_mgmt_tx_compl_event;

#ifdef QCA_MANUAL_TRIGGERED_ULOFDMA
#define MAX_ULOFDMA_SU_TRIGGER 8
#define MAX_ULOFDMA_MU_PEER 8
#define WMI_HOST_ULOFDMA_SU_TRIGGER 0
#define WMI_HOST_ULOFDMA_MU_TRIGGER 1
#define MAX_ULOFDMA_CHAINS 8

/**
 * enum wmi_host_ulofdma_trig_status - manual ulofdma trig response status
 * @wmi_host_ul_ofdma_manual_trig_txerr_none - none
 * @wmi_host_ul_ofdma_manual_trig_txerr_resp - response timeout, mismatch
 *                                             BW mismatch, mimo ctrl mismatch
 *                                             CRC error..
 * @wmi_host_ul_ofdma_manual_trig_txerr_filt - blocked by tx filtering
 * @wmi_host_ul_ofdma_manual_trig_txerr_fifo - fifo, misc errors in HW
 * @wmi_host_ul_ofdma_manual_trig_txerr_swabort - software initialted abort
 *                                                (TX_ABORT)
 * @wmi_host_ul_ofdma_manual_trig_txerr_max - max status
 * @wmi_host_ul_ofdma_manual_trig_txerr_invalid - invalid status
 */
typedef enum {
	wmi_host_ul_ofdma_manual_trig_txerr_none,
	wmi_host_ul_ofdma_manual_trig_txerr_resp,
	wmi_host_ul_ofdma_manual_trig_txerr_filt,
	wmi_host_ul_ofdma_manual_trig_txerr_fifo,
	wmi_host_ul_ofdma_manual_trig_txerr_swabort,
	wmi_host_ul_ofdma_manual_trig_txerr_max     = 0xff,
	wmi_host_ul_ofdma_manual_trig_txerr_invalid =
					wmi_host_ul_ofdma_manual_trig_txerr_max,
} wmi_host_ulofdma_trig_status;

/**
 * struct wmi_host_manual_ul_ofdma_trig_feedback_evt - Trigger feedback event
 * @vdev_id: Associated vdev_id
 * @trigger_type: SU-0, MU-1
 * @curr_su_manual_trig: current_su_manual_trig
 * @remaining_su_manual_trig: remaining_su_manual_trig
 * @remaining_mu_trig_peers: remaining_mu_trig_peers
 * @manual_trig_status: manual trigger status
 * @macaddr: peer macaddr
 * @num_peer: number of peers
 */
typedef struct {
	uint32_t vdev_id;
	uint32_t trigger_type;
	wmi_host_ulofdma_trig_status manual_trig_status;
	uint8_t  macaddr[MAX_ULOFDMA_MU_PEER][QDF_MAC_ADDR_SIZE];
	uint8_t  num_peer;
	union {
		struct {
			uint32_t curr_su_manual_trig;
			uint32_t remaining_su_manual_trig;
		} su;
		struct {
			uint32_t remaining_mu_trig_peers;
		} mu;
	} u;
} wmi_host_manual_ul_ofdma_trig_feedback_evt;

/**
 * struct wmi_host_rx_peer_userinfo - Trigger response peer info
 * @peer_mac: peer macaddr
 * @resp_type: QOS DATA  or QOS NULL
 * @mcs: Rx peer trig resp mcs
 * @nss: Rx peer trig resp nss
 * @gi_ltf_type: Rx peer trig resp gi ltf type
 * @per_chain_rssi: Per chain rssi
 */
struct wmi_host_rx_peer_userinfo {
	struct qdf_mac_addr peer_mac;
	uint8_t resp_type;
	uint8_t mcs;
	uint8_t nss;
	uint8_t gi_ltf_type;
	uint32_t per_chain_rssi[MAX_ULOFDMA_CHAINS];
};

/**
 * struct wmi_host_rx_peer_common_info - Trigger response common info
 * @vdev_id: Associated vdev_id
 * @rx_ppdu_resp_type: SU-0, MU-1
 * @num_peer: Number of peers
 * @rx_resp_bw: Rx trig response bandwidth
 * @combined_rssi: Rx trig response combined rssi
 */
struct wmi_host_rx_peer_common_info {
	uint8_t vdev_id;
	uint8_t rx_ppdu_resp_type;
	uint8_t num_peers;
	uint8_t rx_resp_bw;
	uint32_t combined_rssi;
};

/**
 * struct wmi_host_rx_peer_userinfo_evt_data - Trigger response event
 * @comm_info: Trigger response common info
 * @peer_info: Trigger response peer info
 */
struct wmi_host_rx_peer_userinfo_evt_data {
	struct wmi_host_rx_peer_common_info comm_info;
	struct wmi_host_rx_peer_userinfo peer_info[MAX_ULOFDMA_MU_PEER];
};
#endif /* QCA_MANUAL_TRIGGERED_ULOFDMA */

/**
 * struct wmi_host_chan_info_event - Channel info WMI event
 * @pdev_id: pdev_id
 * @err_code: Error code
 * @freq: Channel freq
 * @cmd_flags: Read flags
 * @noise_floor: Noise Floor value
 * @rx_clear_count: rx clear count
 * @cycle_count: cycle count
 * @chan_tx_pwr_range: channel tx power per range
 * @chan_tx_pwr_tp: channel tx power per throughput
 * @rx_frame_count: rx frame count
 * @rx_11b_mode_data_duration: 11b mode data duration
 * @my_bss_rx_cycle_count: self BSS rx cycle count
 * @tx_frame_cnt: tx frame count
 * @mac_clk_mhz: mac clock
 * @vdev_id: unique id identifying the VDEV
 * @tx_frame_count: tx frame count
 * @rx_clear_ext20_count: ext20 frame count
 * @rx_clear_ext40_count: ext40 frame count
 * @rx_clear_ext80_count: ext80 frame count
 * @per_chain_noise_floor: Per chain NF value in dBm
 */
typedef struct {
	uint32_t pdev_id;
	uint32_t err_code;
	uint32_t freq;
	uint32_t cmd_flags;
	uint32_t noise_floor;
	uint32_t rx_clear_count;
	uint32_t cycle_count;
	uint32_t chan_tx_pwr_range;
	uint32_t chan_tx_pwr_tp;
	uint32_t rx_frame_count;
	uint32_t rx_11b_mode_data_duration;
	uint32_t my_bss_rx_cycle_count;
	uint32_t tx_frame_cnt;
	uint32_t mac_clk_mhz;
	uint32_t vdev_id;
	uint32_t tx_frame_count;
	uint32_t rx_clear_ext20_count;
	uint32_t rx_clear_ext40_count;
	uint32_t rx_clear_ext80_count;
	uint32_t per_chain_noise_floor[WMI_HOST_MAX_CHAINS];
} wmi_host_chan_info_event;

/**
 * struct wmi_host_scan_blanking_params - Scan blanking parameters
 * @valid: Indicates whether the structure is valid
 * @blanking_count: blanking count
 * @blanking_duration: blanking duration
 */
typedef struct {
	bool valid;
	uint32_t blanking_count;
	uint32_t blanking_duration;
} wmi_host_scan_blanking_params;

/**
 * struct wmi_host_pdev_channel_hopping_event
 * @pdev_id: pdev_id
 * @noise_floor_report_iter: Noise threshold iterations with high values
 * @noise_floor_total_iter: Total noise threshold iterations
 */
typedef struct {
	uint32_t pdev_id;
	uint32_t noise_floor_report_iter;
	uint32_t noise_floor_total_iter;
} wmi_host_pdev_channel_hopping_event;

/**
 * struct peer_chan_width_switch_params - Peer channel width capability wrapper
 * @num_peers: Total number of peers connected to AP
 * @max_peers_per_cmd: Peer limit per WMI command
 * @vdev_id: vdev id
 * @chan_width_peer_list: List of capabilities for all connected peers
 */

struct peer_chan_width_switch_params {
	uint32_t num_peers;
	uint32_t max_peers_per_cmd;
	uint32_t vdev_id;
	struct peer_chan_width_switch_info *chan_width_peer_list;
};

/**
 * struct wmi_pdev_enable_tx_mode_selection - fw tx mode selection
 * @pdev_id: radio id
 * @enable_tx_mode_selection: flag to enable tx mode selection
 */
struct wmi_pdev_enable_tx_mode_selection {
	uint32_t pdev_id;
	uint32_t enable_tx_mode_selection;
};

/**
 * struct wmi_intra_bss_params - params intra_bss enable/disable cmd
 * @vdev_id: unique id identifying the VDEV, generated by the caller
 * @peer_macaddr: MAC address used for installing
 * @enable: Enable/Disable the command
 */
struct wmi_intra_bss_params {
	uint16_t vdev_id;
	uint8_t *peer_macaddr;
	bool enable;
};

#ifdef WLAN_SUPPORT_MESH_LATENCY
/**
 * struct wmi_vdev_latency_info_params - vdev latency info params
 * @service_interval: service interval in miliseconds
 * @burst_size: burst size in bytes
 * @latency_tid: tid associated with this latency information
 * @ac: Access Category associated with this td
 * @ul_enable: Bit to indicate ul latency enable
 * @dl_enable: Bit to indicate dl latency enable
 */
struct wmi_vdev_latency_info_params {
	uint32_t service_interval;
	uint32_t burst_size;
	uint32_t latency_tid :8,
			 ac          :2,
			 ul_enable   :1,
			 dl_enable   :1,
			 reserved    :20;
};

/**
 * struct wmi_vdev_tid_latency_config_params - VDEV TID latency config params
 * @num_vdev: Number of vdevs in this message
 * @vdev_id: Associated vdev id
 * @pdev_id: Associated pdev id
 * @latency_info: Vdev latency info
 */
struct wmi_vdev_tid_latency_config_params {
	uint8_t num_vdev;
	uint16_t vdev_id;
	uint16_t pdev_id;
	struct wmi_vdev_latency_info_params latency_info[1];
};

/**
 * struct wmi_peer_latency_info_params - peer latency info params
 * @peer_mac: peer mac address
 * @service_interval: service interval in miliseconds
 * @burst_size: burst size in bytes
 * @latency_tid: tid associated with this latency information
 * @ac: Access Category associated with this td
 * @ul_enable: Bit to indicate ul latency enable
 * @dl_enable: Bit to indicate dl latency enable
 * @flow_id: Flow id associated with tid
 * @add_or_sub: Bit to indicate add/delete of latency params
 * @sawf_ul_param: Bit to indicate if UL params are for SAWF/SCS
 * @max_latency: Maximum latency in milliseconds
 * @min_throughput: Minimum throughput in Kbps
 */
struct wmi_peer_latency_info_params {
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint32_t service_interval;
	uint32_t burst_size;
	uint32_t latency_tid :8,
			ac          :2,
			ul_enable   :1,
			dl_enable   :1,
			flow_id     :4,
			add_or_sub  :2,
			sawf_ul_param :1,
			reserved    :13;
	uint32_t max_latency;
	uint32_t min_throughput;
};

/**
 * struct wmi_peer_latency_config_params - peer latency config params
 * @num_peer: Number of peers in this message
 * @vdev_id: Associated vdev id
 * @pdev_id: Associated pdev id
 * @latency_info: Vdev latency info
 */
struct wmi_peer_latency_config_params {
	uint32_t num_peer;
	uint32_t pdev_id;
	struct wmi_peer_latency_info_params latency_info[2];
};
#endif

#ifdef WLAN_WSI_STATS_SUPPORT
/**
 * struct wmi_wsi_stats_info_params - WSI stats info
 * @pdev_id: PDEV ID
 * @wsi_ingress_load_info: Ingress count for the PSOC
 * @wsi_egress_load_info: Egress count for the PSOC
 */
struct wmi_wsi_stats_info_params {
	uint32_t pdev_id;
	uint32_t wsi_ingress_load_info;
	uint32_t wsi_egress_load_info;
};
#endif

#ifdef QCA_MANUAL_TRIGGERED_ULOFDMA
/**
 * struct wmi_trigger_ul_ofdma_su_params - SU manual trigger ul_ofdma info
 * @vdev_id: Associated vdev id
 * @length: trigger length
 * @nss : trigger nss value
 * @mcs : trigger mcs value
 * @gi_ltf: trigger gi_ltf value
 * @reserved: reserved bits
 * @rssi : trigger rssi value
 * @macaddr : Peer macaddr
 * @preferred_ac: Preferred AC
 * @num_trigger: Number of SU ULOFDMA triggers
 */
struct wmi_trigger_ul_ofdma_su_params {
	uint32_t vdev_id;
	uint32_t length:12,
		 nss:4,
		 mcs:4,
		 gi_ltf:2,
		 reserved:10;
	int8_t rssi;
	uint8_t macaddr[QDF_MAC_ADDR_SIZE];
	uint8_t preferred_ac;
	uint8_t num_trigger;
};

/**
 * struct wmi_trigger_ul_ofdma_mu_params - MU manual trigger ul_ofdma info
 * @vdev_id: Associated vdev id
 * @num_peer: Number of MU ULOFDMA peers
 * @preferred_ac: Preferred AC
 * @macaddr : Peer macaddr
 */
struct wmi_trigger_ul_ofdma_mu_params {
	uint32_t vdev_id;
	uint8_t num_peer;
	uint8_t preferred_ac;
	uint8_t macaddr[MAX_ULOFDMA_MU_PEER][QDF_MAC_ADDR_SIZE];
};
#endif

#ifdef CONFIG_SAWF_DEF_QUEUES
/**
 * struct wmi_rc_params- rate control parameters
 * @upper_cap_nss: Max NSS
 * @upper_cap_mcs: Max MCS
 * @en_nss_cap: Enable NSS upper cap
 * @en_mcc_cap: Enable MCS upper cap
 * @retry_thresh: Retry threshold
 * @mcs_drop: number of MCS to drop
 * @en_retry_thresh: Enable rate retry threshold
 * @en_mcs_drop: Enable mcs drop number
 * @min_mcs_probe_intvl: min mcs probe interval
 * @max_mcs_probe_intvl: max mcs probe interval
 * @min_nss_probe_intvl: min nss probe interval
 * @max_nss_probe_intvl: max nss probe interval
 */
struct wmi_rc_params {
	uint8_t upper_cap_nss;
	uint8_t upper_cap_mcs;
	uint8_t en_nss_cap;
	uint8_t en_mcs_cap;
	uint8_t retry_thresh;
	uint8_t mcs_drop;
	uint8_t en_retry_thresh;
	uint8_t en_mcs_drop;
	uint16_t min_mcs_probe_intvl;
	uint16_t max_mcs_probe_intvl;
	uint16_t min_nss_probe_intvl;
	uint16_t max_nss_probe_intvl;
};

/**
 * struct wmi_sawf_params - Service Class Parameters
 * @svc_id: Service ID
 * @min_thruput_rate: min throughput in kilobits per second
 * @max_thruput_rate: max throughput in kilobits per second
 * @burst_size:  burst size in bytes
 * @service_interval: service interval
 * @delay_bound: delay bound in in milli seconds
 * @msdu_ttl: MSDU Time-To-Live
 * @priority: Priority
 * @tid: TID
 * @msdu_rate_loss: MSDU loss rate in parts per million
 */
struct wmi_sawf_params {
	uint32_t svc_id;
	uint32_t min_thruput_rate;
	uint32_t max_thruput_rate;
	uint32_t burst_size;
	uint32_t service_interval;
	uint32_t delay_bound;
	uint32_t msdu_ttl;
	uint32_t priority;
	uint32_t tid;
	uint32_t msdu_rate_loss;
	uint32_t disabled_modes;
};
#endif

#ifdef QCA_STANDALONE_SOUNDING_TRIGGER

/* TXBF souding max peer macro */
#define MAX_MU_TXBF_SOUNDING_USER 3

/**
 * struct wmi_txbf_sounding_trig_param - TXBF souding parameters
 * @pdev_id: pdev id
 * @vde_id: vdev id
 * @[1-0]feedback_type: single user/multi user feedback type,Range[0-SU, 1-MU, 2-CQI]
 * @ng: [3:2] Ng -Range[0-1]
 * @codebook: [4] Codebook -Range[0-1]
 * @bw: [7:5] Bandwidth -Range[0-4]
 * @cqi_triggered_type -[8] Rnage[0-1]
 * @reserved: [31:9] reserved
 * @sounding_repeats: sounding repetations,Range[[0-3]-MU, [0-5]-SU/CQI]
 * @num_sounding_peers: number of peers, Range[1- SU, [1-3]-MUi/CQI]
 * @macaddr: mac address of peers
 */
struct wmi_txbf_sounding_trig_param {
	u_int32_t  pdev_id;
	u_int32_t  vdev_id;
	u_int32_t  feedback_type      :2,
		   ng                 :2,
		   codebook           :1,
		   bw                 :3,
		   cqi_triggered_type :1,
		   reserved           :23;
	u_int32_t  sounding_repeats;
	u_int8_t   num_sounding_peers;
	u_int8_t   macaddr[MAX_MU_TXBF_SOUNDING_USER][QDF_MAC_ADDR_SIZE];
};

enum wmi_host_standalone_sounding_status {
	WMI_HOST_STANDALONE_SOUND_STATUS_OK,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_NUM_PEERS_EXCEEDED,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_NG_INVALID,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_NUM_REPEAT_EXCEEDED,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_PEER_DOESNOT_SUPPORT_BW,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_INVALID_PEER,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_INVALID_VDEV,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_PEER_DOES_NOT_SUPPORT_MU_FB,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_DMA_NOT_CONFIGURED,
	WMI_HOST_STANDALONE_SOUND_STATUS_ERR_COMPLETE_FAILURE,
};

#define MAX_NUM_SOUNDING_REPEATS 5
/**
 * struct wmi_host_standalone_sounding_evt_params - Standalone sounding
 * command complete event params
 * @vdev_id: vdev id
 * @status: Standalone sounding command status
 * @buffer_uploaded: number of CV buffers uploaded
 * @num_sounding_repeats: Number of sounding repeats
 * @num_snd_failed: Number of sounding failures per repetition
 */
struct wmi_host_standalone_sounding_evt_params {
	uint32_t vdev_id;
	uint32_t status;
	uint32_t buffer_uploaded;
	uint32_t num_sounding_repeats;
	uint32_t snd_failed[MAX_NUM_SOUNDING_REPEATS];
};
#endif /* QCA_STANDALONE_SOUNDING_TRIGGER */

#endif
