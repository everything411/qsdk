/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_CMN_EXTN_H
#define ATH12K_CMN_EXTN_H

#include <linux/ieee80211.h>
#include <linux/if_ether.h>
#include <linux/mhi.h>
#include <linux/uuid.h>
#include "../../net/mac80211/qcn_extns/cmn_extn.h"
#include "rropinfo.h"
#include "sa_api.h"
#include "cbs_extn.h"

struct ath12k_base;
struct ath12k_hw_blocklist_state;
struct ath12k_reg_info;
struct ath12k_afc_info;
struct ieee80211_240mhz_vendor_oper_extn;
struct ieee80211_ops_extn;
struct ieee80211_iface_combination;
struct ath12k_vif;
struct ath12k_vif_sysfs_entry;
struct ath12k_link_vif;
struct wmi_chan_info_event;
struct ath12k_wmi_scan_req_arg;
enum wmi_tlv_event_id;
struct wmi_start_scan_cmd;
struct ath12k_wmi_vdev_create_arg;
struct ath12k_wmi_peer_assoc_arg;
struct ath12k_wmi_mgmt_rx_arg;
enum hal_ring_type;
struct hal_srng_params;
struct ath12k_link_sta;
struct ath12k_hw;
struct ath12k_skb_cb;
struct ath12k_sta;
struct wmi_unit_test_cmd;
struct ath12k_wmi_resource_config_params;
struct ath12k_wmi_resource_config_arg;
struct wmi_scan_event;
struct sk_buff;
struct wmi_scan_blanking_params_info;
struct wmi_vdev_ratemask_arg;

#ifndef ATH12K_NUM_CHANS
#define ATH12K_NUM_CHANS 102
#endif

#ifndef ATH12K_HW_MAX_QUEUES
#define ATH12K_HW_MAX_QUEUES MIN(NR_CPUS, 5)
#endif

struct ath12k_mgmt;
struct ath12k_mgmt_irq_grp;
struct mgmt_srng;

struct ath12k_vif_extn {
	struct list_head peer_240mhz_list_extn;
	spinlock_t data_lock;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	u8 bssid[ETH_ALEN];
	bool bssid_set;
};

struct ath12k_dp_vif_extn {
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	uint32_t mhdr; /* mesh header */
	uint32_t mhdr_len; /* mesh header len */
	uint32_t mdbg; /* mesh debug */
#define MESH_DBG_TX		0x1
#define MESH_DBG_RX		0x2
#define MESH_DBG_TXHDR_DUMP	0x4
#define MESH_DBG_RXHDR_DUMP	0x8
	uint32_t rx_filter; /* mesh rx filter */
	bool mesh_tx; /*  drop disassocition frames if configured */
#endif
	bool allow_3addr_mc;
};

/**
 * struct esp_extn - ESP (Estimated Service Parameters) extension
 * @air_time_fraction: Configured airtime fraction
 * @ppdu_duration: Configured PPDU duration
 * @ba_window: Configured Block ACK window size
 * @periodicity: ESP Airtime fraction indication period in seconds
 * @fw_esp_air_time: Firmware-estimated airtime (from WMI event)
 * @airtime_update_work: Work struct for vendor event delivery to report
 * airtime received from firmware
 */
struct esp_extn {
	u32 air_time_fraction;
	u8 ppdu_duration;
	u8 ba_window;
	u16 periodicity;
	u32 fw_esp_air_time;
	struct wiphy_work airtime_update_work;
};
struct ath12k_link_vif_extn {
#ifdef CPTCFG_EXT_IPA_STATS
	struct ath12k_dp_ipa_stats_vif_hw_stats *stats;
#endif
	/* GPR frame buffer and TX params for GPR transmission.
	 * period reuses SCAN_DUR from the NL message as the carrier.
	 */
	struct sk_buff *gpr_frame;
	u32 gpr_period;
	u8 gpr_nss;
	u8 gpr_preamble;
	u8 gpr_mcs;
	/* Periodic timer that fires every gpr_period ms to retransmit gpr_frame */
	struct timer_list gpr_timer;
	/* Runtime enable/disable flag controlled via debugfs */
	bool gpr_timer_enabled;
	/* Work item that performs the actual GPR TX in process context */
	struct work_struct gpr_work;
	/* debugfs entry for the gpr_timer enable/disable knob */
	struct dentry *debugfs_gpr_timer;
	/* debugfs entry for the gpr_stats print/clear knob */
	struct dentry *debugfs_gpr_stats;
	/* GPR statistics counters */
	/* times timer re-armed periodically */
	atomic_t gpr_timer_start_count;
	/* times user changed the period */
	atomic_t gpr_timer_resize_count;
	/* frames successfully queued for TX */
	atomic_t gpr_send_count;
	/* VHT MCS 10/11 (1024-QAM) support flags.
	 * vht_mcs10_11_supp: overall enable for this VAP (default: true on HE targets).
	 * vht_mcs10_11_nq2q_peer_supp: enable for peers that did not advertise
	 *   MCS10/11 support via QCN IE (non-QCA peers). Default: false.
	 */
	bool vht_mcs10_11_supp;
	bool vht_mcs10_11_nq2q_peer_supp;
	bool rtt_initiator_role;
};

struct sta_240mhz_info {
	struct ieee80211_240mhz_vendor_oper_extn params_240MHz_extn;
	u8 mac_addr[ETH_ALEN];
	struct list_head list;
};

/* Maximum wait time for off-channel operations (milliseconds). */
#define ATH12K_MAX_OFFCHAN_WAIT        50
/* Off Rx timemout delay for scan radio (ms) */
#define ATH12K_SCAN_RADIO_OFFCHAN_RX_DELAY	150
/* Maximum number of custom pkts for tx per request */
#define ATH12K_MAX_CUSTOM_TX_PKT	20
/* Checks whether SKB has custom pkt */
#define ATH12K_IS_CUSTOM_PKT(cb)	(cb->flags & (ATH12K_SKB_CUSTOM_MGMT_TX |\
						      ATH12K_SKB_CUSTOM_OFFCHAN_MGMT_TX))
/* Checks if Tx Params configured by User */
#define ATH12K_CUSTOM_TX_PARAM_CONFIGURED_EXTN(ar) (ar->ar_extn.raw_pkt_ctx.tx_param.tx_param_configured)

/* Band types used in offchan tx */
enum band_type {
	BAND_DEFAULT,
	BAND_2GHZ,
	BAND_5GHZ,
	BAND_6GHZ,
	BAND_INVALID
};

/* Preamble types used in offchan tx */
enum preamble_type {
	PREAMBLE_OFDM,
	PREAMBLE_CCK,
	PREAMBLE_INVALID
};

/**
 * struct ath12k_blanking_params - Scan blanking parameters
 * @valid: Indicates if blanking parameters are valid
 * @blanking_count: Number of blanking events during scan
 * @blanking_duration: Total blanking duration in msec
 */
struct ath12k_blanking_params {
	__le32 valid;
	__le32 blanking_count;
	__le32 blanking_duration;
} __packed;

/**
 * struct ath12k_offchan_stat - statistics for off-channel activity
 * @noise_floor:        Noise floor in dBm (signed 16-bit).
 * @tx_frame_count:        Number of frames transmitted.
 * @rx_frame_count:        Number of frames received.
 * @rx_clear_count:        RX clear time (units: driver-specific ticks).
 * @cycle_count:           Total channel cycle time (units: ticks).
 * @dwell_time:            Dwell time on the off-channel (ms).
 * @chanswitch_time_htof:  Time to switch from home to off-channel (us).
 * @chanswitch_time_ftoh:  Time to switch from off-channel to home (us).
 * @blank_params:          Blanking Param stats
 */
struct ath12k_offchan_stat {
	s16 noise_floor;
	u32 tx_frame_count;
	u32 rx_frame_count;
	u32 rx_clear_count;
	u32 cycle_count;
	u32 dwell_time;
	u32 chanswitch_time_htof;
	u32 chanswitch_time_ftoh;
	struct ath12k_blanking_params blank_params;
};

/**
 * struct ath12k_skb_tx_param - TX parameters for offchannel tx
 * @nss: Number of spatial streams
 * @preamble: Preamble type
 * @mcs: MCS value
 * @retry: Retry count
 * @power: Power level
 * @tx_beamforming: Beamforming enable flag
 * @is_data: Is skb has data packet
 * @tx_param_configured: Is tx params configured
 */
struct ath12k_skb_tx_param {
	u32 nss;
	enum preamble_type preamble;
	u32 mcs;
	u8 retry;
	u8 power;
	u8 tx_beamforming;
	bool is_data;
	bool tx_param_configured;
};

/**
 * struct ath12k_raw_pkt_ctx - context for raw packet off-channel ops
 * @pkt_list:            Queue of pending raw packets.
 * @chan_lock:            Lock protecting @pkt_list and channel state.
 * @offchan_timer:        Timeout timer for off-channel operations.
 * @chan_stat:            Accumulated off-channel statistics.
 * @vif:                  Associated virtual interface.
 * @tx_params:            Tx params set by user.
 * @transaction_id:       Transaction identifier per request.
 * @num_cmpl_pending:     Number of pending completions.
 * @req_freq:             Requested freq(if set) for offchan operation.
 * @custom_tx_status:     Tx completion status of queued packets.
 * @tracked_skbs:         Array of SKB pointers for completion tracking.
 * @num_tracked_skbs:     Number of SKBs currently being tracked.
 * @link_id:              MLO link ID for the operation (stored from user request).
 * @home_chan:            True if currently on the home channel.
 * @operation_status:     Overall operation status (0=success, 1=failure).
 * @func_type:            Function type for the operation.
 * @num_frames:           Number of frames sent from userspace.
 * @ts_scan_start:        Scan start timestamp
 * @ts_foreign_entry:     Foreign chan entry timestamp
 * @ts_foreign_exit:      Foreign chan exit timestamp
 * @ts_scan_done:         Scan complete timestamp
 * @raw_ctx_mutex:        Mutex to serialize raw context operations (held until user event)
 * @frame_id:             Per-frame IDs supplied by userspace, echoed back in the TX status event.
 */
struct ath12k_raw_pkt_ctx {
	struct sk_buff_head  pkt_list;
	spinlock_t chan_lock;
	struct timer_list offchan_timer;
	struct ath12k_offchan_stat chan_stat;
	struct ieee80211_vif  *vif;
	struct ath12k_skb_tx_param tx_param;
	u32 transaction_id;
	atomic_t num_cmpl_pending;
	u32 req_freq;
	u32 custom_tx_status[ATH12K_MAX_CUSTOM_TX_PKT];
	struct sk_buff *tracked_skbs[ATH12K_MAX_CUSTOM_TX_PKT];
	u8 num_tracked_skbs;
	u8 link_id;
	bool home_chan;
	u8 operation_status;
	u8 func_type;
	u8 num_frames;
	ktime_t ts_scan_start;
	ktime_t ts_foreign_entry;
	ktime_t ts_foreign_exit;
	ktime_t ts_scan_done;
	struct mutex raw_ctx_mutex;
	u8 frame_id[ATH12K_MAX_CUSTOM_TX_PKT];
};

#define DCS_TX_MAX_CU          30


/**
 * struct ath12k_sta_extn - per-STA extension state
 * @primary_soc_id: PSOC id of the primary link chosen for this STA.
 * @avg_link_rssi: Averaged RSSI across the STA's links used for primary
 * link selection.
 */
struct ath12k_sta_extn {
	s8 primary_soc_id;
	s8 avg_link_rssi;
};

/**
 * struct ath12k_cotdma_e2e_cache_entry - per-QMID Co-TDMA E2E config cache entry
 */
#define ATH12K_MAX_COTDMA_E2E_QMIDS  8

struct ath12k_cotdma_e2e_cache_entry {
	u8   config_mode;
	u16  qmid;
	bool bsta_mac_valid;
	u8   bsta_mac[ETH_ALEN];
};

/**
 * struct ath12k_mapc_peer_cache - per-peer MAPC vendor parameter cache
 * Populated by Q2Q, TXOP, and E2E vendor handlers; read back by GET_PARAMS.
 */
struct ath12k_mapc_peer_cache {
	u32 mapc_capability_bitmap;
	u32 apid_to_neighbor_peer;
	u32 apid_from_neighbor_peer;
	u32 channel_width;
	u32 ccfs;
	u32 disable_subchannel_bitmap;
	u32 bss_color;
	u32 rx_txop_return_support;
	u32 q2q_apid_to_neighbor_peer;
	u32 q2q_apid_from_neighbor_peer;
	u32 primary_ac;
	u32 nbr_ap_prio;
	u32 service_start_time;
	u32 service_interval;
	u32 service_end_time;
	u32 critical_traffic_dur_thresh_us;
	u32 max_shared_txop_dur_us;
	u32 min_shared_txop_dur_us;
	u32 param_sets_recvd;
	u32 scheme_enable_bitmap;
	struct ath12k_cotdma_e2e_cache_entry e2e_entries[ATH12K_MAX_COTDMA_E2E_QMIDS];
	u8  e2e_entry_count;
};

/**
 * struct ath12k_link_sta_extn - per-link STA extension state
 * @rssi_assoc: RSSI captured during association for this link; used as the base
 *		RSSI when deriving per-link averages and primary link selection.
 * @sa_sta: Smart Antenna extention for STA
 * @higher_vhtmcs_supp: Per-SS bitmap of negotiated VHT MCS 10/11 support for this peer.
 *                      Populated from the QCN IE VHT MCS10/11 attribute (0x02) during association.
 *                      Bit N set means NSS (N+1) supports MCS 10 and 11. Zero if the
 *                      peer did not advertise MCS10/11 via QCN IE.
 * @he_cap_info_internal: QCA-internal HE capability bitmap negotiated via QCN IE attrs 0x03/0x04.
 *                        bit 0: RX 1xLTF + 0.4us GI (attr 0x03 byte 0)
 *                        bit 1: RX 2xLTF + 0.4us GI (attr 0x03 byte 1)
 *                        bit 2: 2xLTF in 160/80+80 MHz (attr 0x04)
 *                        Zero if the peer did not advertise support via QCN IE.
 */
struct ath12k_link_sta_extn {
	s8 rssi_assoc;
	struct sa_sta_extn sa_sta;
	u8 higher_vhtmcs_supp;
	u32 he_cap_info_internal;
	struct ath12k_mapc_peer_cache mapc_cache;
};

struct wmi_dcs_wlan_interference_stats {
	u32 reg_tsf32;
	u32 last_ack_rssi;
	u32 tx_waste_time;
	u32 rx_time;
	u32 phyerr_cnt;
	u32 listen_time;
	u32 reg_tx_frame_cnt;
	u32 reg_rx_frame_cnt;
	u32 reg_rxclr_cnt;
	u32 reg_cycle_cnt;
	u32 reg_rxclr_ext_cnt;
	u32 reg_ofdm_phyerr_cnt;
	u32 reg_cck_phyerr_cnt;
	s32 chan_nf;
	u32 my_bss_rx_cycle_count;
};
/* DCS user-configured thresholds (from hostapd_cli),
 * fallback to macros in dcs_extn.h when not set.
 */
struct dcs_wlan_intr_user_cfg {
	u32 intr_detection_threshold;
	u32 phyerr_penalty;
	u32 phyerr_threshold;
	u32 radarerr_threshold;
	u32 txerr_threshold;
	u32 sample_size;
	u8  coch_intr_threshold;
	u8  user_max_cu;
};

struct wlan_intf_extn {
	struct wmi_dcs_wlan_interference_stats prev_stats;
	struct wmi_dcs_wlan_interference_stats curr_stats;
	bool prev_valid;
	bool curr_valid;
	struct dcs_wlan_intr_user_cfg dcs_cfg;
};

#define ATH12K_MSEC_PER_SEC		1000
#define ATH12K_MAX_CAC_TIMEOUT_MS	(24 * 60 * 60 * ATH12K_MSEC_PER_SEC)
#define ATH12K_MAX_CAC_TIMEOUT_SEC	(ATH12K_MAX_CAC_TIMEOUT_MS / ATH12K_MSEC_PER_SEC)
#define NO_CAC_OVERRIDE		-1

/**
 * struct ath12k_extn - ath12k driver private extensions
 * @esp:  esp extensions structure
 * @raw_pkt_ctx:   raw packet home/off-channel context.
 * @radio_kobj:    pointer to radio kernel object.
 * @vifs_kobj:     Pointer to vifs kernel object.
 * @block_dfs_channels: 1:DFS channels disabled by blockdfslist command,
 *   0: DFS channel are not disabled by blockdfslist command
 * @blockdfs_disabled_chans: bitmap of sband->channels[] indices that were
 *   disabled by the blockdfslist logic; used to avoid clearing DISABLED on
 *   channels that were disabled by regulatory rather than by this feature
 * @cac_timeout_override_ms: Custom CAC timeout override in ms.
 *	NO_CAC_OVERRIDE = no override set (use regulatory domain CAC timeout).
 *	Any positive value = override in ms.
 * @vif_entries:   Pointer to 17 sysfs vif entries.
 * @dcs_config_bitmap: Bitmap of DCS enablement mode.
 * @rtplinst:      Representative tx power list instances for ATH12K_NUM_CHANS
 * @wlan_intf: WLAN interference structure.
 * @intf_detect_cnt: Interefernce detection counter.
 * @samp_cnt: Sample counter.
 * @he_mcs_12_13_peer_cap: Intersected peer HE MCS 12/13 capability, for
 *   the AP. Populated by hostapd via
 *   QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP from the STA's Association
 *   Request.
 *   bits[7:0]  = L80 NSS bitmap (BW <= 80 MHz) → mcs_set bits[23:16]
 *   bits[15:8] = G80 NSS bitmap (BW >  80 MHz) → mcs_set bits[31:24]
 * @he_mcs_12_13_radio_cap: Radio capability for HE MCS 12/13 support.
 * @sa_ar: Smart Antenna extention for radio
 */
struct ath12k_extn {
	struct esp_extn esp;
	struct ath12k_raw_pkt_ctx raw_pkt_ctx;

	/* sysfs entries under ieee80211: radioX */
	struct kobject *radio_kobj;
	struct kobject *vifs_kobj;
	bool block_dfs_channels;
	DECLARE_BITMAP(blockdfs_disabled_chans, 64);
	s32 cac_timeout_override_ms;
	struct ath12k_vif_sysfs_entry *vif_entries[17];
	u16 dcs_config_bitmap;
	struct rtplinst_extn rtplinst[ATH12K_NUM_CHANS];
	struct wlan_intf_extn wlan_intf;
	u8 intf_detect_cnt;
	u8 samp_cnt;
	u16 he_mcs_12_13_peer_cap;
	u16 he_mcs_12_13_radio_cap;

	/* Smart Antenna Specific */
	struct sa_ar_extn sa_ar;

	/* Continuous Background Scan structure */
	struct ath12k_cbs cbs;

	u8 scan_blanking_mode;

	/* serialises WMI OEM cmd -> response */
	struct completion lowi_oem_done;
};

/**
 * struct ath12k_reg_info_extn - ath12k regulatory info extensions
 * @reg_6ghz_thresh_priority_freq: VLP priority threshold frequency
 * @hw_blocklist: Parsed HW blocklist payload for the current reg event
 *
 */
struct ath12k_reg_info_extn {
	u16 reg_6ghz_thresh_priority_freq;
	struct ath12k_hw_blocklist_update *hw_blocklist;
};

/**
 * struct offchan_event_work_with_txn - Work structure for off-channel event
 * @work: Work structure for scheduling the event
 * @wdev: Pointer to the wireless device
 * @transaction_id: Transaction ID for correlation with user-space request
 * @raw_pkt_ctx: Pointer to raw packet context for per-packet status
 *
 * This structure is used to schedule off-channel statistics events to user space
 * with transaction ID support for request-response correlation.
 */
struct offchan_event_work_with_txn {
	struct work_struct work;
	struct wireless_dev *wdev;
	u32 transaction_id;
	struct ath12k_raw_pkt_ctx *raw_pkt_ctx;
};

/**
 * struct ath12k_home_offchan_params - Parameters for home/off-channel operation
 * @wdev: Pointer to the wireless device
 * @offchan_frame: Netlink attribute containing the frame data and parameters
 * @band: Operating band (e.g., 2.4GHz, 5GHz, 6GHz)
 * @freq: Channel frequency in MHz
 * @transaction_id: Transaction ID for correlation with user-space request
 * @scan_dur: Scan duration in milliseconds for off-channel operation
 * @num_frames: Number of frames to transmit
 * @func: Function type (TX_MGMT, TX_DATA, or RX)
 * @link_id: MLO link ID (0 for non-MLO or default link)
 * @is_mld: Whether this is an MLO device
 */
struct ath12k_home_offchan_params {
	struct wireless_dev *wdev;
	struct nlattr *offchan_frame;
	int band;
	int freq;
	u32 transaction_id;
	u16 scan_dur;
	u8 num_frames;
	u8 func;
	u8 link_id;
	bool is_mld;
};

/**
 * struct ath12k_wmi_scan_req_arg_extn - Extension parameters for scan request
 * @scan_start_offset: scan start offset
 */
struct ath12k_wmi_scan_req_arg_extn {
	u32 scan_start_offset;
};

/**
 * struct hal_rx_frm_type_info - Per-PPDU 802.11 frame type counters
 * @rx_mgmt_cnt: Number of 802.11 management frames decoded for this PPDU
 * @rx_ctrl_cnt: Number of 802.11 control frames decoded for this PPDU
 * @rx_data_cnt: Number of 802.11 data frames decoded for this PPDU
 */
struct hal_rx_frm_type_info {
	u8 rx_mgmt_cnt;
	u8 rx_ctrl_cnt;
	u8 rx_data_cnt;
};

struct ath12k_wsi_remap_work {
    struct work_struct work;
    struct ath12k_base *ab;
};

/**
 * struct ath12k_wifi6_phyrx_abort_info - PHY abort and SIG field metadata
 * @phyrx_abort: Set to 1 if PHY aborted the reception of this PPDU
 * @phyrx_abort_reason: PHY abort reason code (valid when phyrx_abort is set)
 * L-SIG/L-SIG-A fields:
 * @l_sig_length: PSDU length in octets
 * @l_sig_a_parity: Even parity over rate and length fields
 * @l_sig_a_pkt_type: Packet type (rate field encoding)
 * @l_sig_a_implicit_sounding: Captured implicit sounding flag
 * HT-SIG fields:
 * @ht_length: PSDU length in octets
 * @smoothing: Smoothing recommended flag
 * @not_sounding: Not sounding flag
 * @aggregation: PSDU contains A-MPDU flag
 * @ht_stbc: Space-time block coding streams
 * @ht_crc: HT-SIG CRC
 * VHT-SIG-A fields:
 * @vht_no_txop_ps: TXOP_PS_NOT_ALLOWED flag
 * @vht_crc: VHT-SIG-A CRC
 * HE-SIG-A SU fields:
 * @he_sig_a_su_crc: HE-SIG-A SU CRC
 * HE-SIG-A MU-DL fields:
 * @he_sig_a_mu_dl_crc: HE-SIG-A MU-DL CRC
 */
struct ath12k_wifi6_phyrx_abort_info {
	bool phyrx_abort;
	u8   phyrx_abort_reason;
	u32  l_sig_length:12;
	u32  l_sig_a_parity:1;
	u32  l_sig_a_pkt_type:4;
	u32  l_sig_a_implicit_sounding:1;
	u32  ht_length:16;
	u32  smoothing:1;
	u32  not_sounding:1;
	u32  aggregation:1;
	u32  ht_stbc:2;
	u32  ht_crc:8;
	u32  vht_no_txop_ps:1;
	u32  vht_crc:8;
	u32  he_sig_a_su_crc:4;
	u32  he_sig_a_mu_dl_crc:4;
};

/**
 * struct hal_mon_ppdu_info_extn - Monitor-mode PPDU extension metadata
 * @rx_state: RX pipeline state at the time this PPDU was reported
 * @rx_antenna: RX antenna bitmap (one bit per RF chain) indicating chains that contributed
 * @rssi: Per-chain RSSI(signed), indexed by RF chain (up to 8 entries)
 * @sw_frame_group_id: Software-defined frame group identifier for this PPDU
 * @frm_type_info: Aggregated 802.11 frame-type counters for the PPDU
 * @phyrx_abort_info: PHY abort and SIG field metadata; NULL when feature is disabled
 */
struct hal_mon_ppdu_info_extn {
	u8 rx_state;
	u32 rx_antenna;
	s8 rssi[8];
	u8 sw_frame_group_id;
	struct hal_rx_frm_type_info frm_type_info;
	struct ath12k_wifi6_phyrx_abort_info *phyrx_abort_info;
};

struct ath12k_dp_rx_scan_radio_stats {
	u64 rx_ok_bytes;
	u64 rx_err_bytes;
	u32 rx_ok_pkts;
	u32 rx_err_pkts;
	u32 rx_mgmt_pkts;
	u32 rx_ctrl_pkts;
	u32 rx_data_pkts;
};

/* PHY error reason codes 0-53 are valid; codes 54-63 are reserved */
#define ATH12K_WIFI6_PHYRX_ERR_MAX	64
/* Bit shift to position the upper 32-bit half of the 64-bit PHY error mask */
#define ATH12K_WIFI6_PHYRX_ERR_MASK_CONT_SHIFT	32

/**
 * struct ath12k_wifi6_undecoded_metadata - Undecoded metadata capture state
 * @undecoded_metadata_capture: Flag indicating capture is currently armed
 * @phyrx_error_mask: Lower 32 bits of the PHY RX error mask (errors 0-31)
 * @phyrx_error_mask_cont: Upper 32 bits of the PHY RX error mask (errors 32-63)
 * @rx_undecoded_count: Total count of undecoded PPDUs processed
 * @rx_undecoded_error: Per-error-code count of undecoded PPDUs
 */
struct ath12k_wifi6_undecoded_metadata {
	bool undecoded_metadata_capture;
	u32  phyrx_error_mask;
	u32  phyrx_error_mask_cont;
	u32  rx_undecoded_count;
	u32  rx_undecoded_error[ATH12K_WIFI6_PHYRX_ERR_MAX];
};

/**
 * struct ath12k_pdev_mon_dp_extn - Per-pdev monitor datapath extension
 * @rx_scan_radio_stats: Scan radio RX statistics
 * @rx_tlv_log: RX TLV logger instance; NULL if logging is disabled
 * @tx_tlv_log: TX TLV logger instance; NULL if logging is disabled
 * @undecoded_meta: Undecoded metadata capture state; NULL when feature is disabled
 * @phyrx_abort_info: PHY abort and SIG field metadata for the current PPDU; NULL when feature is disabled
 */
struct ath12k_pdev_mon_dp_extn {
	struct ath12k_dp_rx_scan_radio_stats rx_scan_radio_stats;
	struct ath12k_dp_mon_tlv_logger *rx_tlv_log;
	struct ath12k_dp_mon_tlv_logger *tx_tlv_log;
	struct ath12k_wifi6_undecoded_metadata *undecoded_meta;
	struct ath12k_wifi6_phyrx_abort_info *phyrx_abort_info;
};

struct ath12k *ath12k_get_ar_from_wdev(struct wireless_dev *wdev, u8 link_id);

struct ath12k *ath12k_get_radio_by_id(struct wiphy *wiphy, u8 radio_id);

/*
 * struct ath12k_mgmt_irq_grp_extn - Driver private extension for mgmt irq_grp
 * @priority: Priority interrupt
 */
struct ath12k_mgmt_irq_grp_extn {
	bool priority;
};

/* keep it same as `struct mgmt_srng` */
struct mgmt_srng_extn {
	u32 *vaddr_unaligned;
	u32 *vaddr;
	dma_addr_t paddr_unaligned;
	dma_addr_t paddr;
	int size;
	u32 ring_id;
	u8 cached;
};

#define ATH12K_SRNG_STATS_EXTN_MGMT_FRM_STYPE_MAX 16
/**
 * struct ath12k_device_mgmt_srng_stats_extn - Driver private extension for
 *                                             mgmt srng stats
 * @rx_highpri_pkts: Per-Frame subtype stats for high priority packets
 */
struct ath12k_device_mgmt_srng_stats_extn {
	u32 rx_highpri_pkts[ATH12K_SRNG_STATS_EXTN_MGMT_FRM_STYPE_MAX];
};

/**
 * struct ath12k_mgmt_wifi8_extn - WiFi8 device-specific mgmt info
 * @reo_dst_highpri_rx_ring: Ring interface for private high priority ring
 */
struct ath12k_mgmt_wifi8_extn {
	struct mgmt_srng_extn reo_dst_highpri_rx_ring;
};


int ath12k_vs_action_has_ml_link_info_ie_extn(struct ath12k *ar,
					      u8 catagory,
					      const u8 *pos,
					      const u8 *end);

bool ath12k_is_beacon_report_elem(u8 action_code, const u8 *buf, const u8 *end);

struct ath12k_radio_cfg_extn {
	u8 scan_f_strict_passive_pch;
};

#ifndef CPTCFG_QCN_EXTN
static inline void
int ath12k_vs_action_has_ml_link_info_ie_extn(struct ath12k *ar,
					      u8 catagory,
					      const u8 *pos,
					      const u8 *end)
{
	return 0;
}

bool ath12k_is_beacon_report_elem(u8 action_code, const u8 *buf,
				  const u8 *end)
{
	return false;
}

static inline void
ath12k_mac_hw_allocate_extn(struct ieee80211_hw *hw,
			    const struct ieee80211_ops_extn *ops_extn)
{
	return;
}

static inline void ath12k_mac_hw_register_extn(struct ath12k_hw *ah)
{
}

static inline void ath12k_wifi7_hw_init_extn(struct ath12k_base *ab)
{
	return;
}

static inline void ath12k_wifi8_hw_init_extn(struct ath12k_base *ab)
{
	return;
}

static inline void ath12k_mac_init_arvif_extn(struct ath12k_vif *ahvif,
					      struct ath12k_link_vif *arvif)
{
	return;
}

static inline void ath12k_mac_deinit_arvif_extn(struct ath12k_link_vif *arvif)
{
	return;
}

static inline void ath12k_gpr_debugfs_register_extn(struct ath12k_link_vif *arvif,
						     struct dentry *link_debugfs)
{
	return;
}
static inline void ath12k_mac_remove_link_interface_extn(struct ath12k_link_vif *arvif)
{
	return;
}

static inline void ath12k_mgmt_stats_scan_radio_tx_reset(struct ath12k_vif *ahvif)
{
	return;
}

static inline void ath12k_wmi_peer_migration_event_extn(struct ath12k_vif *ahvif)
{
	return;
}

static inline
int ath12k_wmi_op_rx_extn(enum wmi_tlv_event_id id, struct ath12k_base *ab,
			  struct sk_buff *skb)
{
	return -1;
}

static inline int
ath12k_add_sta_240mhz_info_extn(struct ath12k_vif *ahvif,
				u8 *mac,
				struct ieee80211_240mhz_vendor_oper_extn
				*params)
{
	return -1;
}

static inline void ath12k_mac_setup_radio_iface_comb_extn(
		struct ieee80211_iface_combination *comb)
{
	return;
}

static inline void ath12k_mac_setup_extn(struct ath12k *ar)
{
}

static inline void
ath12k_mac_dfs_violation_recovery_extn(struct ath12k_link_vif *arvif,
				       struct ieee80211_chanctx_conf *ctx)
{
}

static inline void
ath12k_mac_setnol_recovery_extn(struct ath12k_link_vif *arvif,
				struct ieee80211_chanctx_conf *ctx)
{
}

static inline void ath12k_mac_cleanup_unregister_extn(struct ath12k *ar)
{
}

static inline void
ath12k_core_nol_extn_auto_dump_all_radios(struct ath12k_base *ab)
{
}

static inline void
ath12k_core_nol_extn_auto_dump_radio_locked(struct ath12k *ar,
					     u8 radio_idx,
					     const char *reason)
{
}

static inline void
ath12k_core_nol_extn_auto_restore_radio_locked(struct ath12k *ar, u8 radio_idx)
{
}

static inline void
ath12k_core_nol_extn_auto_restore_all_radios(struct ath12k_base *ab)
{
}

static inline void
ath12k_core_nol_extn_regdom_change_radio_locked(struct ath12k *ar)
{
}

static inline int ath12k_custom_tx_extn(struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	return -1;
}

static inline void ath12k_custom_tx_free_extn(struct sk_buff *skb, u32 status)
{
}

static inline void ath12k_sysfs_init_extn(struct ath12k *ar)
{
}

static inline void ath12k_sysfs_cleanup_extn(struct ath12k *ar)
{
}

static inline void ath12k_wmi_prepare_tx_params_extn(struct ath12k_skb_cb *skb_cb,
						     void *ptr)
{
}

static inline void ath12k_update_offchan_stats_extn(struct ath12k *ar,
						    struct sk_buff *skb,
						    u32 freq,
						    struct wmi_chan_info_event *ch_info,
						    int channel_idx)
{
}

static inline void ath12k_send_scan_radio_chan_stats_event_extn(struct ath12k *ar,
								u32 freq,
								const struct wmi_scan_blanking_params_info *blanking)
{
}

static inline int ath12k_vendor_get_scan_blanking_mode_extn(struct wiphy *wiphy,
								  u64 *value)
{
	return -EOPNOTSUPP;
}

static inline void ath12k_update_offchan_ts_extn(struct ath12k *ar,
						 u32 event_type)
{
}

static inline int schedule_home_offchan_stats_event_ext(struct wireless_dev *wdev,
                                                         u32 transaction_id,
							 struct ath12k_raw_pkt_ctx *raw_pkt_ctx)
{
	return -1;
}

static int ath12k_vendor_home_offchan_tx_rx_handler(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    const void *data,
						    int data_len)
{
	return -1;
}

static void ath12k_home_offchan_tx_drain(struct ath12k_raw_pkt_ctx *raw_pkt_ctx) {
	return;
}

static void ath12k_home_offchan_cleanup(struct ath12k *ar) {
	return;
}

static void ath12k_custom_tx_send(struct ieee80211_vif *vif,
				  struct ath12k_raw_pkt_ctx *raw_pkt_ctx) {
	return;
}

static inline int
ath12k_mac_setup_vdev_create_arg_scan_radio_extn(struct ath12k_link_vif *arvif,
						 struct ath12k_wmi_vdev_create_arg *arg)
{
	return 0;
}

static inline int
ath12k_wmi_tlv_scan_radio_caps_ext2(struct ath12k_base *ab, u16 tag,
				    u16 len, const void *ptr,
				    void *data)
{
	return 0;
}

static inline void
ath12k_wmi_offchan_txrx_update_scan_params_extn(struct ath12k *ar,
					struct ath12k_wmi_scan_req_arg *arg)
{
}

static inline void
ath12k_wmi_prepare_scan_req_extn(struct ath12k *ar,
				 struct ath12k_wmi_scan_req_arg *arg,
				 const u8 *ssid,
				 u8 ssid_len)
{
}

static inline void
ath12k_wmi_start_scan_init_extn(struct ath12k *ar,
				struct ath12k_wmi_scan_req_arg *arg)
{
}

static inline void
ath12k_wmi_send_scan_start_cmd_extn(struct ath12k *ar,
				    struct wmi_start_scan_cmd *cmd,
				    struct ath12k_wmi_scan_req_arg *arg)
{
}

static inline void
ath12k_wmi_dcs_cw_interference_event_extn(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  u32 pdev_id)
{
}

static inline void
ath12k_wmi_dcs_wlan_interference_event_extn(struct ath12k_base *ab,
					    struct sk_buff *skb,
					    u32 pdev_id)
{
}

static inline void
ath12k_wmi_dcs_obss_interference_event_extn(struct ath12k_base *ab,
					    struct sk_buff *skb,
					    u32 pdev_id)
{
}

static inline int
ath12k_wmi_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
		    int (*iter)(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data),
		    void *data)
{
	return -1;
}

int ath12k_pcic_mgmt_interrupt_handler_extn(struct ath12k_mgmt_irq_grp *irq_grp)
{
	return -1;
}

static inline int
ath12k_wmi_send_unit_test_cmd(struct ath12k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args)
{
	return -1;
}

static inline int
ath12k_wmi_dcs_event_parser(struct ath12k_base *ab, u16 tag, u16 len,
			    const void *ptr, void *data)
{
	return -1;
}

static inline void ath12k_extn_reconfig_extn_params(struct ath12k *ar)
{
}

int ath12k_wifi8_mgmt_init_extn(struct ath12k_mgmt *mgmt)
{
	return 0;
}

void ath12k_wifi8_mgmt_deinit_extn(struct ath12k_mgmt *mgmt)
{
}

int ath12k_mgmt_srng_type_valid_extn(struct ath12k_base *ab, enum hal_ring_type type)
{
	return -1;
}

int ath12k_mgmt_srng_setup_extn(struct ath12k_base *ab, enum hal_ring_type type,
				bool *cached, struct hal_srng_params *params)
{
	return -1;
}

int ath12k_wifi8_mgmt_rx_ring_setup_extn(struct ath12k_base *ab)
{
	return -1;
}

void ath12k_mgmt_srng_hw_disable_extn(struct ath12k_base *ab)
{

}

void ath12k_wifi8_mgmt_rx_ring_free_extn(struct ath12k_base *ab)
{
}

static inline bool
ath12k_cbs_skip_scan_event_and_send_scan_complete(
	struct ath12k *ar,
	const struct wmi_scan_event *scan_ev)
{
	return false;
}

static inline bool
ath12k_is_cbs_vendor_event(struct ath12k *ar)
{
	return false;
}

int ath12k_wifi8_mgmt_rx_deliver_mmpdu_extn(struct ath12k_mgmt *mgmt,
					    struct ath12k *partner_ar,
					    struct sk_buff *mmpdu, u32 pkt_type)
{
	return -1;
}

ssize_t ath12k_debugfs_dump_device_mgmt_srng_stats_extn(struct ath12k_base *ab, char *buf,
							int len, int size)
{
	return len;
}

int ath12k_wifi8_mgmt_dump_ring_stats_extn(struct ath12k_mgmt *mgmt, char *buf, int size,
					   int len)
{
	return len;
}

static int
ath12k_wmi_pull_reg_hw_blocklist_extn(struct ath12k_base *ab, struct sk_buff *skb,
				      struct ath12k_reg_info_extn *reg_info_extn)
{
	return 0;
}

static inline int
ath12k_wmi_hw_blocklist_chan_event_extn(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	return -EOPNOTSUPP;
}

static inline bool
ath12k_wmi_is_reg_hw_blocklist_fragment_only_extn(struct ath12k_base *ab,
						  struct ath12k_reg_info *reg_info)
{
	return false;
}

static inline void
ath12k_wmi_free_reg_hw_blocklist_extn(struct ath12k_reg_info_extn *reg_info_extn)
{
}

static inline void
ath12k_wmi_set_hw_blocklist_host_service_flag_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_params *wmi_cfg,
		struct ath12k_wmi_resource_config_arg *tg_cfg)
{
}

static inline void
ath12k_wmi_set_hw_blocklist_service_support_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_arg *res_cfg)
{
}

static inline void
ath12k_reg_hw_blocklist_free_for_mode_extn(struct ath12k_base *ab, int pdev_idx,
					   u32 pwr_type)
{
}

static inline void
ath12k_reg_hw_blocklist_free_extn(struct ath12k_base *ab)
{
}

static inline void ath12k_debugfs_register_hw_blocklist_extn(struct ath12k *ar)
{
}

static inline bool
ath12k_wmi_handle_afc_power_info_hwbl_extn(
		struct ath12k_base *ab, struct sk_buff *skb,
		struct ath12k_afc_info *afc_info)
{
	return false;
}

static inline bool
ath12k_wmi_handle_afc_continuation_hwbl_extn(
		struct ath12k_base *ab, struct sk_buff *skb,
		struct ath12k_afc_info *afc_info)
{
	return false;
}

static inline bool
ath12k_wmi_handle_reg_chan_list_hwbl_extn(struct ath12k_base *ab,
					  struct ath12k_reg_info **reg_info)
{
	return false;
}

int ath12k_validate_6ghz_chandef_extn(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      unsigned int link_id,
				      const struct cfg80211_chan_def *chandef,
				      enum ieee80211_ap_reg_power power_type)
{
	return 0;
}

static inline bool
ath12k_control_beaconing_bootup_cac_check_extn(struct ath12k *ar,
					       struct ath12k_link_vif *arvif,
					       struct ieee80211_bss_conf *link_conf)
{
	return false;
}

static inline int
ath12k_vendor_oem_data_lowi_extn(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 struct ieee80211_hw *hw,
				 struct ath12k_hw *ah,
				 struct nlattr **tb)
{
	return -1;
}
#else

struct ath12k_dp_hw_group_extn {
	/* Per-ring hot TX descriptor list for lock-free fast-TX allocation.
	 * Populated in batches of ATH12K_DP_TX_HOT_DESC_BATCH_SIZE from the
	 * regular tx_desc_free_list (under tx_desc_lock) and consumed without
	 * a spinlock on the DP_SKB_FAST_TX path.
	 */
	struct list_head tx_hot_desc_list[ATH12K_HW_MAX_QUEUES];
	bool tx_desc_extn_initialized;
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	struct ath12k_dp_hw_group_ipa *ipa;
#endif
};

struct ath12k_pci_extn {
	/* SMMU DS */
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	struct dma_iommu_mapping *smmu_mapping;
	struct iommu_domain *iommu_domain;
	u8 smmu_s1_enable;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	dma_addr_t smmu_iova_ipa_start;
	dma_addr_t smmu_iova_ipa_current;
	size_t smmu_iova_ipa_len;
#endif
};

struct ath12k_base_extn {
	u16 reg_6ghz_thresh_priority_freq[MAX_RADIOS];
	/* hw_blocklist: Per-pdev HW blocklist state; protected by ab->base_lock */
	struct ath12k_hw_blocklist_state *hw_blocklist[MAX_RADIOS];
	/* hw_bl_reg_info: Cached reg_info for multi-fragment HWBL reg events */
	struct ath12k_reg_info *hw_bl_reg_info[MAX_RADIOS];
	/* hw_bl_afc_info: Cached afc_info for multi-fragment HWBL AFC events */
	struct ath12k_afc_info *hw_bl_afc_info[MAX_RADIOS];
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	struct ath12k_ipa *ipa_ctx;
	/* This is to hold the rx_buffers allocated initially in RXDMA for IPA
	 * as in UD we make use of common API for allocating RX buffer initially
	 * and for refilling the buffers.
	 * IPA ops are not initialized by the time RX buffers allocated initially
	 * 1. hence store the RX buffers in ath12k_base and once IPA is up map them
	 * 2. for any further refilling, check if IPA ops are initialized and
	 *    map the buffer from ath12k_rx_bufs_replinsh
	 */
	bool rx_init_buf_mapped;
	void **rx_buf_pool;
	u32 rx_buf_cnt;
	dma_addr_t mem_pa;
#endif
	/* Smart Antenna Specific */
	struct sa_base_extn sa_base;
};

void ath12k_mac_hw_allocate_extn(struct ieee80211_hw *hw,
			  const struct ieee80211_ops_extn *ops_extn);

/**
 * ath12k_mac_hw_register_extn - Extension hook for ath12k_mac_hw_register
 * @ah: ath12k_hw instance
 *
 * Conditionally sets WIPHY_FLAG_SUPPORTS_PARALLEL_HW_SCAN on the wiphy based
 * on the "parallel_hw_scan" INI key.  Must be called from
 * ath12k_mac_hw_register() after the wiphy has been allocated and the INI
 * context is available.
 */
void ath12k_mac_hw_register_extn(struct ath12k_hw *ah);

void ath12k_wifi7_hw_init_extn(struct ath12k_base *ab);

void ath12k_wifi8_hw_init_extn(struct ath12k_base *ab);


void ath12k_mac_init_arvif_extn(struct ath12k_vif *ahvif,
				struct ath12k_link_vif *arvif);
void ath12k_mac_deinit_arvif_extn(struct ath12k_link_vif *arvif);
void ath12k_gpr_arvif_init(struct ath12k_link_vif *arvif);
void ath12k_gpr_arvif_deinit(struct ath12k_link_vif *arvif);
void ath12k_gpr_debugfs_register_extn(struct ath12k_link_vif *arvif,
				      struct dentry *link_debugfs);
void ath12k_mac_remove_link_interface_extn(struct ath12k_link_vif *arvif);

void ath12k_mgmt_stats_scan_radio_tx_reset(struct ath12k_vif *ahvif);

const void **ath12k_wmi_tlv_parse_alloc(struct ath12k_base *ab,
					struct sk_buff *skb, gfp_t gfp);

void ath12k_wmi_peer_migration_event_extn(struct ath12k_vif *ahvif);

int ath12k_wmi_op_rx_extn(enum wmi_tlv_event_id id, struct ath12k_base *ab,
			  struct sk_buff *skb);

int ath12k_wmi_tlv_scan_radio_caps_ext2(struct ath12k_base *ab, u16 tag,
					u16 len, const void *ptr,
					void *data);

int ath12k_add_sta_240mhz_info_extn(struct ath12k_vif *ahvif,
				     u8 *mac,
				     struct ieee80211_240mhz_vendor_oper_extn
				     *params);
/**
 * ath12k_custom_tx_extn() - Extended custom transmission handler for
 *                           management frames
 * @raw_pkt_ctx: Pointer to raw packet context containing SKB queue
 *
 * This function handles the transmission of custom management frames,
 * including both regular and off-channel transmissions.
 * It walks through all SKBs in the raw packet context queue,
 * performs various validation checks, and queues them to WMI.
 *
 * Return: 0 on success (frames queued for transmission)
 *         -EINVAL if raw_pkt_ctx is NULL
 *         -ENODATA if no packets were processed
 */
int ath12k_custom_tx_extn(struct ath12k_raw_pkt_ctx *raw_pkt_ctx);

/*
 * ath12k_wmi_prepare_tx_params_extn() - Prepare tx params for WMI command
 * @skb_cb: Pointer to the skb_cb
 * @ptr: Pointer to the WMI buffer
 *
 * This function prepares the tx params for the WMI command by:
 * Setting the appropriate fields in the tx_param_dword0 and tx_param_dword1 values
 */
void ath12k_wmi_prepare_tx_params_extn(struct ath12k_skb_cb *skb_cb, void *ptr);

/**
 * ath12k_custom_tx_free_extn() - Free custom management TX skb and
 *                                update status
 * @skb: Socket buffer containing the transmitted frame
 * @status: Transmission status code from firmware
 *
 * This function handles the cleanup of custom management frames (both regular
 * and off-channel) after transmission. It updates the transmission status in
 * the virtual interface structure and frees the socket buffer.
 *
 * Return: Void
 */
void ath12k_custom_tx_free_extn(struct sk_buff *skb, u32 status);

/**
 * ath12k_update_offchan_stats_extn - Update off-channel statistics
 * @ar: ath12k radio instance
 * @skb: Point to skb containing chaninfo event buffer
 * @freq: Frequency in MHz from chan_info event
 * @ch_info: Channel info event data
 *
 * Updates the off-channel statistics if an off-channel operation is in progress
 * and the frequency matches the current off-channel request.
 *
 * Return: Void
 */
void ath12k_update_offchan_stats_extn(struct ath12k *ar,
				      struct sk_buff *skb,
				      u32 freq,
				      struct wmi_chan_info_event *ch_info,
				      int channel_idx);
void ath12k_send_scan_radio_chan_stats_event_extn(struct ath12k *ar,
						  u32 freq,
						  const struct wmi_scan_blanking_params_info *blanking);
int ath12k_vendor_get_scan_blanking_mode_extn(struct wiphy *wiphy, u64 *value);

/**
 * ath12k_update_offchan_ts_extn - Update offchan timestamps based on scan events
 * @ar: ath12k radio instance
 * @event_type: Scan event type from WMI
 *
 * Captures timestamps at key scan event transitions
 * - WMI_SCAN_EVENT_STARTED: Start of offchan operation
 * - WMI_SCAN_EVENT_FOREIGN_CHAN: Entry to foreign channel
 * - WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT: Exit from foreign channel
 * - WMI_SCAN_EVENT_COMPLETED: Offchan operation finished
 *
 * Return: Void
 */
void ath12k_update_offchan_ts_extn(struct ath12k *ar, u32 event_type);

/**
 * ath12k_custom_txrx_init - Initialize custom txrx context
 * @ar: ath12k radio instance
 *
 * This function initializes raw pkt context, lock, list and timer
 *
 * Return: Void
 */
void ath12k_custom_txrx_init(struct ath12k *ar);

/**
 * ath12k_custom_txrx_deinit - Deinitialize custom txrx context
 * @ar: ath12k radio instance
 *
 * This function destroys the timer and raw pkt list.
 *
 * Return: Void
 */
void ath12k_custom_txrx_deinit(struct ath12k *ar);

void ath12k_mac_setup_radio_iface_comb_extn(
		struct ieee80211_iface_combination *comb);

void ath12k_mac_setup_extn(struct ath12k *ar);

void ath12k_mac_dfs_violation_recovery_extn(struct ath12k_link_vif *arvif,
					    struct ieee80211_chanctx_conf *ctx);
void ath12k_mac_setnol_recovery_extn(struct ath12k_link_vif *arvif,
				     struct ieee80211_chanctx_conf *ctx);

void ath12k_mac_cleanup_unregister_extn(struct ath12k *ar);

void ath12k_core_nol_extn_auto_dump_all_radios(struct ath12k_base *ab);
void ath12k_core_nol_extn_auto_dump_radio_locked(struct ath12k *ar,
						  u8 radio_idx,
						  const char *reason);
void ath12k_core_nol_extn_auto_restore_radio_locked(struct ath12k *ar,
						     u8 radio_idx);
void ath12k_core_nol_extn_auto_restore_all_radios(struct ath12k_base *ab);
void ath12k_core_nol_extn_regdom_change_radio_locked(struct ath12k *ar);


void ath12k_sysfs_init_extn(struct ath12k *ar);
void ath12k_sysfs_cleanup_extn(struct ath12k *ar);

/**
 * schedule_home_offchan_stats_event_ext - Schedule home/off-channel statistics event
 * @wdev: Pointer to the wireless device
 * @transaction_id: Transaction ID for correlation with user-space request
 * @raw_pkt_ctx: Pointer to raw packet context for per-packet TX status
 *
 * This function schedules a work item to send home/off-channel statistics as a vendor
 * event to user space. The transaction ID is used to correlate the event with the
 * original request from user space. The raw_pkt_ctx provides per-packet TX status.
 *
 * Return: 0 on success
 */
int schedule_home_offchan_stats_event_ext(struct wireless_dev *wdev,
					   u32 transaction_id,
					   struct ath12k_raw_pkt_ctx *raw_pkt_ctx);

/**
 * ath12k_vendor_home_offchan_tx_rx_handler - Vendor command handler for home/off-channel TX/RX
 * @wiphy: Pointer to the wireless hardware
 * @wdev: Pointer to the wireless device
 * @data: Pointer to the vendor command data
 * @data_len: Length of the vendor command data
 *
 * This function handles the vendor command for transmitting and receiving frames
 * on home channel or off-channel. It parses the netlink attributes, prepares the
 * frame for transmission, and schedules the transmission based on whether it's
 * home channel or off-channel operation.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_vendor_home_offchan_tx_rx_handler(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len);

/**
 * ath12k_home_offchan_tx_drain - Drain all packets from home/off-channel transmission queue
 * @raw_pkt_ctx: Pointer to the raw packet context structure
 *
 * This function removes and frees all packets from the home/off-channel transmission
 * queue. It should be called during cleanup or when a home/off-channel operation
 * times out or is cancelled.
 */
void ath12k_home_offchan_tx_drain(struct ath12k_raw_pkt_ctx *raw_pkt_ctx);

/**
 * ath12k_home_offchan_cleanup - Cleanup home/off-channel resources
 * @ar: Pointer to the ath12k radio structure
 *
 * This function cleans up all home/off-channel related resources including stopping
 * the timer, draining the packet queue, and clearing the vif pointer. Should be
 * called during driver cleanup or detach.
 */
void ath12k_home_offchan_cleanup(struct ath12k *ar);

/**
 * ath12k_custom_tx_send - Send packet from queue on home offchan
 * @vif: Pointer to the virtual interface
 * @raw_pkt_ctx: Pointer to the raw packet context structure
 *
 * This function dequeues and transmits a packet from the home offchan queue.
 * It handles the transmission of custom management frames on the home offchan.
 */
void ath12k_custom_tx_send(struct ieee80211_vif *vif,
			   struct ath12k_raw_pkt_ctx *raw_pkt_ctx);

/**
 * ath12k_wmi_offchan_txrx_update_scan_params_extn - Update scan params required
 *                                                    for custom txrx
 * @ar: Pointer to the ath12k radio structure
 * @arg: Pointer to the scan parameter
 *
 * This function sets priority and chan stats flags in scan parameter for
 * custom txrx.
 */
void ath12k_wmi_offchan_txrx_update_scan_params_extn(struct ath12k *ar,
					     struct ath12k_wmi_scan_req_arg *arg);

/**
 * ath12k_wmi_prepare_scan_req_extn - Prepare scan request extension data
 * @ar: Pointer to the ath12k radio structure
 * @arg: Pointer to the scan request parameters
 * @ssid: Pointer to the scan request SSID
 * @ssid_len: Length of the scan request SSID
 *
 * Store scan request SSID information for the scan vif and update strict
 * passive scan parameter from userspace radio configuration when applicable.
 */
void ath12k_wmi_prepare_scan_req_extn(struct ath12k *ar,
				      struct ath12k_wmi_scan_req_arg *arg,
				      const u8 *ssid,
				      u8 ssid_len);

/**
 * ath12k_wmi_start_scan_init_extn - Initialize CBS scan offset in scan args
 * @ar: Pointer to the ath12k radio structure
 * @arg: Pointer to the scan parameter
 *
 * This function initializes the scan_start_offset field in the scan request
 * argument for CBS support.
 */
void ath12k_wmi_start_scan_init_extn(struct ath12k *ar,
				     struct ath12k_wmi_scan_req_arg *arg);

/**
 * ath12k_wmi_send_scan_start_cmd_extn - Copy CBS scan offset into WMI command
 * @ar: Pointer to the ath12k radio structure
 * @cmd: Pointer to the WMI start scan command
 * @arg: Pointer to the scan parameter
 *
 * This function copies the scan_start_offset from the scan request argument
 * into the WMI start scan command for CBS support.
 */
void ath12k_wmi_send_scan_start_cmd_extn(struct ath12k *ar,
					 struct wmi_start_scan_cmd *cmd,
					 struct ath12k_wmi_scan_req_arg *arg);

                   int ath12k_mac_setup_vdev_create_arg_scan_radio_extn(struct ath12k_link_vif *arvif,
						     struct ath12k_wmi_vdev_create_arg *arg);
void ath12k_wmi_dcs_cw_interference_event_extn(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       u32 pdev_id);
int ath12k_wmi_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
			int (*iter)(struct ath12k_base *ab, u16 tag, u16 len,
				    const void *ptr, void *data),
			void *data);

/**
 * ath12k_wmi_send_unit_test_cmd() - Build and send a WMI_UNIT_TEST_CMDID
 * command to FW. Shared by all unit test based debug commands (AFC grace
 * timer, DFS unit test, M3 unit test, AWGN unit test, and extn callers).
 * @ar: pointer to ath12k radio
 * @ut_cmd: unit test command header (vdev_id, module_id, num_args, diag_token)
 * @test_args: array of num_args u32 test arguments
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_wmi_send_unit_test_cmd(struct ath12k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args);

int ath12k_wmi_dcs_event_parser(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data);
void ath12k_wmi_dcs_obss_interference_event_extn(struct ath12k_base *ab,
						 struct sk_buff *skb,
						 u32 pdev_id);

void ath12k_wmi_dcs_wlan_interference_event_extn(struct ath12k_base *ab,
						 struct sk_buff *skb,
						 u32 pdev_id);

int ath12k_get_best_primary_umac_w_rssi(struct ath12k_hw *ah,
                                         struct ath12k_vif *ahvif,
                                         struct ath12k_sta *ahsta,
                                         unsigned long valid_links,
                                         u8 *primary_link_id);
u32 ath12k_get_monitor_version(struct ath12k_base *ab);

void ath12k_mgmt_rx_event_extn(struct ath12k *ar, struct sk_buff *skb,
		struct ath12k_wmi_mgmt_rx_arg *rx_ev);

bool ath12k_cbs_skip_scan_event_and_send_scan_complete(
	struct ath12k *ar,
	const struct wmi_scan_event *scan_ev);
bool ath12k_is_cbs_vendor_event(struct ath12k *ar);

/**
 * ath12k_reg_chan_list_cc_ext_parse_extn - Helper function to parse extn TLVs
 * from WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT event
 * @ab: Pointer to the ath12k base structure
 * @next_tlv: Pointer to the next TLV to be parsed
 * @reg_info_extn: Pointer to the output struct
 */
int ath12k_reg_chan_list_cc_ext_parse_extn(struct ath12k_base *ab,
					   const void *next_tlv,
					   struct ath12k_reg_info_extn *reg_info_extn);

/**
 * ath12k_reg_handle_chan_list_extn - Helper function to save the extn params
 * from reg_info_extn to ar_extn
 * @ab: Pointer to the ath12k base structure
 * @pdev_idx: pdev identifier
 * @reg_info_extn: Pointer to the input ath12k_reg_info_extn struct
 */
void ath12k_reg_handle_chan_list_extn(struct ath12k_base *ab,
				      int pdev_idx,
				      const struct ath12k_reg_info_extn *reg_info_extn);

/**
 * ath12k_reg_hw_blocklist_free_for_mode_extn - Free HW blocklist for a given mode
 * @ab: Pointer to the ath12k base structure
 * @pdev_idx: pdev identifier
 * @pwr_type: AP power type
 */
void ath12k_reg_hw_blocklist_free_for_mode_extn(struct ath12k_base *ab,
						int pdev_idx,
						u32 pwr_type);

/**
 * ath12k_reg_hw_blocklist_free_extn - Free all HW blocklist entries
 * @ab: Pointer to the ath12k base structure
 */
void ath12k_reg_hw_blocklist_free_extn(struct ath12k_base *ab);
int ath12k_reg_hw_blocklist_snapshot_extn(struct ath12k_base *ab, u8 phy_id,
					  struct ath12k_hw_blocklist_state *snapshot);
void ath12k_reg_hw_blocklist_free_snapshot_extn(struct ath12k_hw_blocklist_state *snapshot);

/**
 * ath12k_change_6g_txpow_sta_mode_validate_extn - Validate SP power mode
 * against HW blocklist for standalone STA; disconnect if blocked.
 * @ar: Radio instance
 * @wdev: Wireless device of the STA vif
 * @link_id: Link identifier
 *
 * Return: true if HWBL-blocked (caller should abort power mode change),
 *         false if valid.
 */
bool ath12k_change_6g_txpow_sta_mode_validate_extn(struct ath12k *ar,
						    struct wireless_dev *wdev,
						    u8 link_id);

/**
 * ath12k_wmi_handle_afc_power_info_hwbl_extn - Handle AFC power info for HW blocklist
 * @ab: Pointer to the ath12k base structure
 * @skb: Socket buffer containing the WMI event data
 * @afc_info: Pointer to the parsed AFC info
 *
 * Returns false if no HW blocklist data is present and normal source flow should
 * continue. Returns true if the event was handled by the extension (deferred or
 * fully processed).
 */
bool ath12k_wmi_handle_afc_power_info_hwbl_extn(struct ath12k_base *ab,
						struct sk_buff *skb,
						struct ath12k_afc_info *afc_info);

/**
 * ath12k_wmi_handle_afc_continuation_hwbl_extn - Handle AFC continuation HWBL
 * @ab: ath12k base
 * @skb: WMI event SKB
 * @afc_info: parsed AFC info
 *
 * FW does not set event_type on continuation frames of a multi-event HWBL
 * update; they fall to the default arm of the AFC event_type switch. This
 * handler processes them if a first-fragment afc_info is cached for the pdev.
 *
 * Return: true if consumed as HWBL continuation, false otherwise.
 */
bool ath12k_wmi_handle_afc_continuation_hwbl_extn(struct ath12k_base *ab,
						  struct sk_buff *skb,
						  struct ath12k_afc_info *afc_info);

/**
 * ath12k_debugfs_register_hw_blocklist_extn - Register HW blocklist debugfs entries
 * @ar: Pointer to the ath12k radio structure
 */
void ath12k_debugfs_register_hw_blocklist_extn(struct ath12k *ar);

/**
 * ath12k_wmi_handle_reg_chan_list_hwbl_extn - Handle HW blocklist in reg chan list event
 * @ab: Pointer to the ath12k base structure
 * @reg_info: Pointer to pointer of the reg_info struct for the current event.
 *   On first-fragment-with-more: ownership is stolen (*reg_info set to NULL).
 *   On final fragment: *reg_info is swapped to the cached first-fragment reg_info
 *   (the fragment-only event's reg_info is freed internally).
 *
 * Returns false if no HW blocklist data is present or on single-fragment / final
 * fragment where normal source flow should call ath12k_reg_handle_chan_list.
 * Returns true if the event was handled (deferred or error).
 */
bool ath12k_wmi_handle_reg_chan_list_hwbl_extn(struct ath12k_base *ab,
					       struct ath12k_reg_info **reg_info);

/**
 * ath12k_wmi_pull_reg_hw_blocklist_extn - Pull HW blocklist information from WMI event
 * @ab: Pointer to the ath12k base structure
 * @skb: Socket buffer containing the WMI event data
 * @reg_info_extn: Pointer to the output struct to store the parsed HW blocklist info
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_wmi_pull_reg_hw_blocklist_extn(
		struct ath12k_base *ab, struct sk_buff *skb,
		struct ath12k_reg_info_extn *reg_info_extn);

/**
 * ath12k_wmi_hw_blocklist_chan_event_extn - Handle HW blocklist channel event
 * @ab: Pointer to the ath12k base structure
 * @skb: Socket buffer containing the WMI event data
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_wmi_hw_blocklist_chan_event_extn(struct ath12k_base *ab,
					    struct sk_buff *skb);

/**
 * ath12k_wmi_is_reg_hw_blocklist_fragment_only_extn - Check if HW blocklist event is fragment-only
 * @ab: Pointer to the ath12k base structure
 * @reg_info: Pointer to the reg_info struct to be checked
 *
 * Return: true if the event is fragment-only, false otherwise
 */
bool ath12k_wmi_is_reg_hw_blocklist_fragment_only_extn(
		struct ath12k_base *ab,
		struct ath12k_reg_info *reg_info);

/**
 * ath12k_wmi_free_reg_hw_blocklist_extn - Free resources allocated for HW blocklist info
 * @reg_info_extn: Pointer to the reg_info_extn struct containing the HW blocklist info
 */
void ath12k_wmi_free_reg_hw_blocklist_extn(
		struct ath12k_reg_info_extn *reg_info_extn);

/**
 * ath12k_wmi_set_hw_blocklist_host_service_flag_extn - Set host service flag in
 * WMI resource config
 * @ab: Pointer to the ath12k base structure
 * @wmi_cfg: Pointer to the WMI resource config parameters
 * @tg_cfg: Pointer to the WMI resource config argument containing the target service info
 */
void ath12k_wmi_set_hw_blocklist_host_service_flag_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_params *wmi_cfg,
		struct ath12k_wmi_resource_config_arg *tg_cfg);

/**
 * ath12k_wmi_set_hw_blocklist_service_support_extn - Set HW blocklist support flag
 * in WMI resource config argument
 * @ab: Pointer to the ath12k base structure
 * @res_cfg: Pointer to the WMI resource config argument to be updated
 */
void ath12k_wmi_set_hw_blocklist_service_support_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_arg *res_cfg);

/**
 * ath12k_validate_6ghz_chandef_extn - Validate 6GHz channel definition for AP mode
 * @hw: Pointer to the ieee80211 hardware structure
 * @vif: Pointer to the ieee80211 virtual interface structure
 * @link_id: Link identifier for which the validation is being performed
 * @chandef: Pointer to the channel definition to be validated
 * @power_type: Regulatory power type for AP mode
 */
int ath12k_validate_6ghz_chandef_extn(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      unsigned int link_id,
				      const struct cfg80211_chan_def *chandef,
				      enum ieee80211_ap_reg_power power_type);

/**
 * ath12k_extn_reconfig_extn_params - Reconfig extn params after FW recovery
 * @ar: Pointer to the ath12k radio structure
 */
void ath12k_extn_reconfig_extn_params(struct ath12k *ar);

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
void ath12k_peer_assoc_h_mesh_extn(struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   struct ieee80211_link_sta *link_sta,
				   struct ath12k_wmi_peer_assoc_arg *arg);
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

/**
 * ath12k_wifi8_mgmt_rx_reap_packets - Reap management packets from the rings
 * @ab: ath12k_base instance
 * @ring: HAL ring structure for the appropriate ring packets are reaped from
 * @mmpdu_list: List to store reaped packets
 *
 * Return: Number of packets reaped, 0 if ring is empty
 */
int ath12k_wifi8_mgmt_rx_reap_packets(struct ath12k_base *ab, struct mgmt_srng *ring,
				      struct sk_buff_head *mmpdu_list);

/**
 * ath12k_wifi8_mgmt_rx_process_packets - Process an MMPDU reaped from the ring
 * @mgmt: ath12k_mgmt instance
 * @mmpdu_list: List of buffers reaped from the ring
 * @pkt_type: Packet type (based on rings)
 *
 * Return: None
 */
void ath12k_wifi8_mgmt_rx_process_packets(struct ath12k_mgmt *mgmt,
					  struct sk_buff_head *mmpdu_list, u32 pkt_type);

/**
 * ath12k_wifi8_mgmt_rx_deliver_mmpdu - Deliver MMPDUs to the stack
 * @mgmt: ath12k_mgmt instance
 * @partner_ar: ath12k radio (resolved based on hw_link_id)
 * @mmpdu: Management frame
 * @status: skb cb to store rx info
 * @pkt_type: Packet type (based on rings)
 *
 * Return: None
 */
void ath12k_wifi8_mgmt_rx_deliver_mmpdu(struct ath12k_mgmt *mgmt,
					struct ath12k *partner_ar,
					struct sk_buff *mmpdu,
					struct ieee80211_rx_status *status,
					u32 pkt_type);

/**
 * ath12k_pcic_mgmt_interrupt_handler_extn - Handle driver private high priority IRQ for
 *                                           management frames
 * @irq_grp: Interrupt group corresponding to this private IRQ
 *
 * Return: 0 on handling the IRQ, errors otherwise
 */
int ath12k_pcic_mgmt_interrupt_handler_extn(struct ath12k_mgmt_irq_grp *irq_grp);

/**
 * ath12k_wifi8_mgmt_init_extn - Initialize WiFi8 device-specific private objects
 * @mgmt: ath12k mgmt instance
 *
 * Return: 0 on success, errors otherwise
 */
int ath12k_wifi8_mgmt_init_extn(struct ath12k_mgmt *mgmt);

/**
 * ath12k_wifi8_mgmt_deinit_extn - Deinitialize WiFi8 device-specific private objects
 * @mgmt: ath12k_mgmt instance
 *
 * Return: Void
 */
void ath12k_wifi8_mgmt_deinit_extn(struct ath12k_mgmt *mgmt);

/**
 * ath12k_mgmt_srng_type_valid_extn - Validate if the ring type is supported for driver
 *                                    private IRQ
 * @ab: ath12k_base instance
 * @type: HAL ring type
 *
 * Return: 0 if supported, error otherwise
 */
int ath12k_mgmt_srng_type_valid_extn(struct ath12k_base *ab, enum hal_ring_type type);

/**
 * ath12k_mgmt_srng_setup_extn - Set up driver private mgmt SRNG interface
 * @ab: ath12k_base instance
 * @type: HAL ring type
 * @cached: Pointer to indicate whether cached alloc can be used
 * @params: Pointer to configure IRQ batch threshold parameters
 *
 * Return: 0 on success, error otherwise
 */
int ath12k_mgmt_srng_setup_extn(struct ath12k_base *ab, enum hal_ring_type type,
				bool *cached, struct hal_srng_params *params);

/**
 * ath12k_wifi8_mgmt_rx_ring_setup_extn - Set up driver private mgmt rings
 * @ab: ath12k_base instance
 *
 * Return: 0 on success, error otherwise
 */
int ath12k_wifi8_mgmt_rx_ring_setup_extn(struct ath12k_base *ab);

void ath12k_mgmt_srng_hw_disable_extn(struct ath12k_base *ab);

/**
 * ath12k_wifi8_mgmt_rx_ring_free_extn - Free driver private mgmt rings
 * @ab: ath12k_base instance
 *
 * Return: Void
 */
void ath12k_wifi8_mgmt_rx_ring_free_extn(struct ath12k_base *ab);

/**
 * ath12k_wifi8_mgmt_rx_deliver_mmpdu_extn - Do driver private handling for mgmt frames
 * @mgmt: ath12k_mgmt instance
 * @ar: ath12k instance corresponding to hw_link_id of the frame
 * @mmpdu: Management frame
 * @pkt_type: Management ring type
 */
int ath12k_wifi8_mgmt_rx_deliver_mmpdu_extn(struct ath12k_mgmt *mgmt, struct ath12k *ar,
					    struct sk_buff *mmpdu, u32 pkt_type);

/**
 * ssize_t ath12k_debugfs_dump_device_mgmt_srng_stats_extn - Dump driver private stats
 * @ab: ath12k_base instance
 * @buf: Buffer to write stats
 * @len: Length used
 * @size: Max size of the buffer
 */
ssize_t ath12k_debugfs_dump_device_mgmt_srng_stats_extn(struct ath12k_base *ab, char *buf,
							int len, int size);

/**
 * ath12k_wifi8_mgmt_dump_ring_stats_extn - Dump driver private mgmt rings
 * @mgmt: ath12k_mgmt instance
 * @buf: Buffer to write stats
 * @len: Length used
 * @size: Max size of the buffer
 */
int ath12k_wifi8_mgmt_dump_ring_stats_extn(struct ath12k_mgmt *mgmt, char *buf, int size,
					   int len);

/**
 * ath12k_control_beaconing_bootup_cac_check_extn - Check boot-up CAC state
 * and defer vdev_up if DFS CAC is still in progress.
 *
 * @ar:        ath12k radio instance
 * @arvif:     link vif for which vdev_up is being considered
 * @link_conf: ieee80211_bss_conf for the link
 *
 * Returns true if vdev_up should be deferred (CAC still running), false
 * otherwise.
 */
bool ath12k_control_beaconing_bootup_cac_check_extn(struct ath12k *ar,
						     struct ath12k_link_vif *arvif,
						     struct ieee80211_bss_conf *link_conf);

/**
 * ath12k_peer_assoc_h_vht_mcs10_11_extn - Encode VHT MCS 10/11 into tx_mcs_set
 * @ar:     ath12k radio instance
 * @arvif:  link vif
 * @arsta:  link sta (carries higher_vhtmcs_supp from QCN IE negotiation)
 * @arg:    peer_assoc WMI argument being built
 *
 * Returns: None
 */
void ath12k_peer_assoc_h_vht_mcs10_11_extn(struct ath12k *ar,
					   struct ath12k_link_vif *arvif,
					   struct ath12k_link_sta *arsta,
					   struct ath12k_wmi_peer_assoc_arg *arg);

/**
 * ath12k_peer_assoc_h_he_cap_internal_extn - Encode HE 400ns SGI / 2x LTF
 * 160/80+80 MHz internal bitmap into peer_assoc arg
 * @ar:   ath12k radio instance
 * @arsta: link sta (carries he_cap_info_internal from QCN IE negotiation)
 * @arg:  peer_assoc WMI argument being built
 * @band: nl80211_band used to select the local HW band capability
 *
 * Returns: None
 */
void ath12k_peer_assoc_h_he_cap_internal_extn(struct ath12k *ar,
					      struct ath12k_link_sta *arsta,
					      struct ath12k_wmi_peer_assoc_arg *arg,
					      enum nl80211_band band);

/**
 * ath12k_mac_vht_mcs10_11_ratemask_extn - Extend per-NSS VHT ratemask with
 * MCS10/11 (1024-QAM) bits for VAPs with QCN IE negotiated MCS10/11 support.
 * @arvif: link vif whose ratemask is being built
 * @arg:   ratemask WMI arg already populated with standard MCS 0-9 bits
 */
void ath12k_mac_vht_mcs10_11_ratemask_extn(struct ath12k_link_vif *arvif,
					   struct wmi_vdev_ratemask_arg *arg);

/**
 * ath12k_vendor_oem_data_lowi_extn() - Handle LOWI OEM data vendor commands
 * @wiphy: wiphy on which the OEM data vendor command was received
 * @wdev: wireless_dev the command targets, may be NULL if the caller expects
 *        it to be resolved from @tb's IFINDEX attribute
 * @hw: ieee80211_hw instance backing @wdev
 * @ah: ath12k_hw instance backing @hw
 * @tb: parsed OEM data vendor attributes (QCA_WLAN_VENDOR_ATTR_OEM_DATA_*)
 *
 * Resolves @wdev from the IFINDEX attribute when not already known, decodes
 * the LOWI command/event type, MLO link ID and radio index carried in @tb,
 * and dispatches RTT_REGISTER/START/CANCEL, CHANNEL_INFO, CAPABILITIES,
 * AVAIL_CH and CONFIG_LCI requests from the userspace lowi-server to the
 * corresponding ranging handler.
 *
 * Return: 0 or a negative error code on success/failure. Returns -EAGAIN if
 * tb[QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_TYPE] is absent, signalling the
 * caller to fall through to the legacy Energy Service OEM-data path.
 */
int ath12k_vendor_oem_data_lowi_extn(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     struct ieee80211_hw *hw,
				     struct ath12k_hw *ah, struct nlattr **tb);
#endif /* CPTCFG_QCN_EXTN */
#endif /* ATH12K_CMN_EXTN_H */
