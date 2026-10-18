/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CMN_H
#define CMN_H

#include "includes.h"
#include "common/defs.h"

#ifndef ICM_PRI_SEC20_PENALIZATION_FACTOR
#define ICM_PRI_SEC20_PENALIZATION_FACTOR (75)
#endif

#ifdef CONFIG_QCN_APP_EXTN
#include "qacs/qacs.h"
#endif

#include "includes.h"
#include "utils/list.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "repurpose.h"
#include "../src/common/qca-vendor.h"
#include "cbs.h"
#include "rropinfo.h"
#include "wpa_config_extn.h"
#include "reg_extn.h"
#include "ucode_extn.h"
#ifdef RDK_ONEWIFI
#include "ap_drv_ops_extn.h"
#include "eloop_rdk_extn.h"
#include "wpa_auth_rdk_extn.h"
#include "wps_rdk_extn.h"
#endif /* RDK_ONEWIFI */

struct hostapd_config;
struct sta_info;
struct hostapd_iface;
struct nl_msg;
struct hostapd_channel_data;
enum ieee80211_op_mode;
struct hostapd_sta_add_params;
enum bw_type;
struct hostapd_freq_params;
struct hostapd_data;
struct ieee802_11_elems;
enum oper_chan_width;
enum chan_width;
struct wpa_ctrl;
struct hostapd_bss_config;
struct hostapd_multi_mbssid;
struct hostapd_multi_mbssid_group;
struct i802_bss;
enum wpa_event_type;
struct nlattr;
struct wpa_ctrl;
union wpa_event_data;
struct ieee80211_neighbor_ap_info;
struct wpa_supplicant;
struct wpa_bss;
struct dfs_event;
struct wpa_connect_work;
struct csa_settings;
struct ubus_context;
struct blob_buf;
struct uc_value;
struct uc_vm;
struct ieee80211_mgmt;
struct wpa_driver_scan_params;
struct dl_list;
struct hostapd_hw_modes;
struct wpa_driver_nl80211_data;
struct nl80211_vendor_cmd_info;
struct wpa_scan_res;
struct wpabuf;
struct wpa_config;
struct freq_survey;
struct wpa_ssid;
struct channel_list_changed;

#define UDBG_ENH_SERVER_IP_SIZE INET6_ADDRSTRLEN
#define UDBG_ENH_APP_ID_SIZE 64u

struct ieee80211_240mhz_vendor_oper_extn {
	u8 ccfs1;
	u8 ccfs0;
	u16 punct_bitmap;
	u16 is5ghz240mhz          :1,
	    bfmess320mhz          :3,
	    numsound320mhz        :3,
	    nonofdmaulmumimo320mhz:1,
	    mubfmr320mhz          :1;
	u8 mcs_map_320mhz[3];
} STRUCT_PACKED;

/*
 * Introduced ieee80211_240mhz_vendor_oper_extn_v2 to correct field
 * ordering (ccfs0 before ccfs1) as per spec. The legacy struct
 * ieee80211_240mhz_vendor_oper_extn is retained for backward compatibility
 * until a broader review decides on its deprecation. New code should use
 * ieee80211_240mhz_vendor_oper_v2.
 */
struct ieee80211_240mhz_vendor_oper_extn_v2 {
	u8 ccfs0;
	u8 ccfs1;
	u16 punct_bitmap;
	u16 is5ghz240mhz      :1,
	bfmess320mhz          :3,
	numsound320mhz        :3,
	nonofdmaulmumimo320mhz:1,
	mubfmr320mhz          :1;
	u8 mcs_map_320mhz[3];
} STRUCT_PACKED;


struct ieee80211_240mhz_params_extn {
	struct ieee80211_240mhz_vendor_oper_extn *eht_240mhz_capab;
	size_t eht_240mhz_capab_len;
};

/**
 * enum cswopts_flags - Channel Switch Option flags
 *
 * Bitmap flags that control the behaviour of the channel-switch handler
 * (CSH).  Multiple flags may be OR-ed together into a single cswopts value.
 *
 * @CSH_OPT_NONDFS_RANDOM:        Select a random channel from non-DFS
 *                                channels only.
 * @CSH_OPT_IGNORE_CSA_DFS:       Ignore a CSA received from the uplink AP
 *                                when the target channel is DFS; instead pick
 *                                a non-DFS channel locally.
 * @CSH_OPT_CAC_APUP_BYSTA:       AP VAP performs CAC when it is brought up
 *                                by a STA VAP.
 * @CSH_OPT_CSA_APUP_BYSTA:       AP VAP sends a CSA when it is brought up
 *                                by a STA VAP (implicit channel change).
 * @CSH_OPT_RCSA_TO_UPLINK:       Send an RCSA to the uplink AP on radar
 *                                detection or upon receiving an RCSA.
 * @CSH_OPT_PROCESS_RCSA:         Process a received RCSA and act on it.
 * @CSH_OPT_APRIORI_NEXT_CHANNEL: On radar detection, switch to the
 *                                pre-configured apriori next channel.
 */
enum cswopts_flags {
	CSH_OPT_NONDFS_RANDOM        = 0x01,
	CSH_OPT_IGNORE_CSA_DFS       = 0x02,
	CSH_OPT_CAC_APUP_BYSTA       = 0x04,
	CSH_OPT_CSA_APUP_BYSTA       = 0x08,
	CSH_OPT_RCSA_TO_UPLINK       = 0x10,
	CSH_OPT_PROCESS_RCSA         = 0x20,
	CSH_OPT_APRIORI_NEXT_CHANNEL = 0x40,
};

/* Accessor macros operating on a cswopts unsigned int value */
#define IS_CSH_NONDFS_RANDOM_ENABLED(cswopts)        ((cswopts) & CSH_OPT_NONDFS_RANDOM)
#define IS_CSH_IGNORE_CSA_DFS_ENABLED(cswopts)       ((cswopts) & CSH_OPT_IGNORE_CSA_DFS)
#define IS_CSH_CAC_APUP_BYSTA_ENABLED(cswopts)       ((cswopts) & CSH_OPT_CAC_APUP_BYSTA)
#define IS_CSH_CSA_APUP_BYSTA_ENABLED(cswopts)       ((cswopts) & CSH_OPT_CSA_APUP_BYSTA)
#define IS_CSH_RCSA_TO_UPLINK_ENABLED(cswopts)       ((cswopts) & CSH_OPT_RCSA_TO_UPLINK)
#define IS_CSH_PROCESS_RCSA_ENABLED(cswopts)         ((cswopts) & CSH_OPT_PROCESS_RCSA)
#define IS_CSH_APRIORI_NEXT_CHANNEL_ENABLED(cswopts) ((cswopts) & CSH_OPT_APRIORI_NEXT_CHANNEL)

/* Aggregate mask of all supported CSwOpts bits */
#define CSH_OPT_VALID_MASK (CSH_OPT_NONDFS_RANDOM   | \
			    CSH_OPT_IGNORE_CSA_DFS  | \
			    CSH_OPT_CAC_APUP_BYSTA  | \
			    CSH_OPT_CSA_APUP_BYSTA  | \
			    CSH_OPT_RCSA_TO_UPLINK  | \
			    CSH_OPT_PROCESS_RCSA    | \
			    CSH_OPT_APRIORI_NEXT_CHANNEL)

/**
 * cswopts_validate - Check that a raw value fits within the supported mask
 * @val: Raw (long int) value read from config or ctrl_iface
 *
 * Return: true if valid (non-negative and no unknown bits set), false otherwise.
 */
static inline bool cswopts_validate(long int val)
{
	return val >= 0 && !(val & ~CSH_OPT_VALID_MASK);
}

/**
 * convert_cswopts_to_str - Return a human-readable name for a single CSwOpts bit.
 * @opt: A single CSH_OPT_* constant
 *
 * Return: Human-readable description of the option, or "unknown" for unrecognised values.
 */
static inline const char *convert_cswopts_to_str(unsigned int opt)
{
	switch (opt) {
	case CSH_OPT_NONDFS_RANDOM:
		return "(0x01: non-DFS random channel select)";
	case CSH_OPT_IGNORE_CSA_DFS:
		return "(0x02: ignore uplink CSA to DFS channel)";
	case CSH_OPT_CAC_APUP_BYSTA:
		return "(0x04: AP VAP runs CAC on STA-triggered bring-up)";
	case CSH_OPT_CSA_APUP_BYSTA:
		return "(0x08: AP VAP sends CSA on STA-triggered bring-up)";
	case CSH_OPT_RCSA_TO_UPLINK:
		return "(0x10: forward RCSA to uplink AP on radar)";
	case CSH_OPT_PROCESS_RCSA:
		return "(0x20: process received RCSA)";
	case CSH_OPT_APRIORI_NEXT_CHANNEL:
		return "(0x40: switch to apriori next channel on radar)";
	default:
		return "unknown";
	}
}

struct driver_dcs_config {
	u8 cmd_type;
	u16 dcs_enable;

	/* Bitmask indicating which fields below are valid */
	u32 valid_mask;

	/* DCS configuration parameters */
	u32 intr_detection_threshold;
	u32 phyerr_penalty;
	u32 phyerr_threshold;
	u32 radarerr_threshold;
	u32 txerr_threshold;
	u32 sample_size;
	u8 coch_intr_threshold;
	u8 user_max_cu;
};

enum dcs_cmd_type {
	GET_DCS_CONFIG,
	SET_DCS_CONFIG,
};

#define DCS_CSA_TBTT_DEFAULT        5
#define DCS_CSA_TBTT_MAX            30
#define DCS_CSA_TBTT_MIN            1

/* OBSS SNR threshold range */
#define OBSS_SNR_MIN 0
#define OBSS_SNR_MAX 127

/* valid_mask bits for driver_dcs_config */
#define DCS_VALID_INTR_DET_THR      BIT(0)
#define DCS_VALID_PHYERR_PENALTY    BIT(1)
#define DCS_VALID_PHYERR_THR        BIT(2)
#define DCS_VALID_RADARERR_THR      BIT(3)
#define DCS_VALID_TXERR_THR         BIT(4)
#define DCS_VALID_SAMPLE_SIZE       BIT(5)
#define DCS_VALID_COCH_THR          BIT(6)
#define DCS_VALID_USER_MAX_CU       BIT(7)

#define BASE_6G_FREQ 5950
#define FREQ_6GHZ_CHAN2             5935
#define FREQ_2GHZ_CHAN14            2484
#define FREQ_5GHZ_240MHZ_START      5500
#define FREQ_5GHZ_240MHZ_END        5730

#define IEEE_2GHZ_CHAN_MIN          1
#define IEEE_2GHZ_CHAN_MAX          13
#define IEEE_2GHZ_CHAN_SEC_SPLIT    7

struct bonded_channel_freq_extn {
	u16 start_freq;
	u16 end_freq;
};

#define OPCLS_TAB_IDX_NONE     0
#define OPCLS_TAB_IDX_US       1
#define OPCLS_TAB_IDX_EUROPE   2
#define OPCLS_TAB_IDX_JAPAN    3
#define OPCLS_TAB_IDX_GLOBAL   4
#define OPCLS_TAB_IDX_CHINA    6
#define OPCLS_TAB_IDX_MAX      6

#ifndef DEFAULT_NOISE_FLOOR_2GHZ
#define DEFAULT_NOISE_FLOOR_2GHZ (-89)
#endif

struct driver_dcs_sim {
	u16 type;
	u32 intf_bitmap;
};

struct  hostapd_sta_add_params_extn {
	struct ieee80211_240mhz_params_extn params_240mhz;
	/* Per-SS bitmap of negotiated VHT MCS 10/11 support for this peer.
	 * Populated from QCN IE parsing during association.
	 * Bit N set means NSS (N+1) supports MCS 10 and 11.
	 */
	u8 higher_vhtmcs_supp;
	/* QCA-internal HE capability bitmap negotiated via QCN IE attrs 0x03/0x04. */
	u32 he_cap_info_internal;
};

struct sta_info_extn {
	struct ieee80211_240mhz_params_extn params_240mhz;
#ifdef CONFIG_IEEE80211AC
	unsigned int mu_cap_war_mu_force_join:1;
	unsigned int mu_cap_war_mu_capable:1;
	unsigned int mu_cap_war_su_join:1;
#endif /* CONFIG_IEEE80211AC */
	/*
	 * SNR of the (Re)Association Request frame, computed as
	 * ssi_signal - noise_floor. Used to filter HT 40 MHz intolerant
	 * notifications from weak/distant stations.
	 */
	int assoc_snr;
	/*
	 * wds_ie_peer - WDS IE peer capability flag
	 *
	 * Set to true when the peer station advertised WDS_IE_CAP_STA in its
	 * association request and the AP BSS has wds_ie=1 configured.
	 * Used by handle_assoc_cb_wds_ie_extn() and ap_free_sta_wds_ie_extn()
	 * to drive hostapd_set_wds_sta() enable/disable calls.
	 */
	bool wds_ie_peer;
	/* Per-SS bitmap of negotiated VHT MCS 10/11 support for this peer.
	 * Populated from the QCN IE VHT MCS10/11 attribute (0x02) during
	 * association.  Bit N set means NSS (N+1) supports MCS 10 and 11.
	 * Zero if the peer did not advertise MCS10/11 via QCN IE.
	 */
	u8 higher_vhtmcs_supp;
	/* QCA-internal HE capability bitmap negotiated via QCN IE attrs 0x03/0x04.
	 * bit 0: RX 1xLTF + 0.4us GI
	 * bit 1: RX 2xLTF + 0.4us GI
	 * bit 2: 2xLTF in 160/80+80 MHz
	 */
	u32 he_cap_info_internal;
	/*
	 * wds_ie_active - WDS IE active state flag
	 *
	 * Set to true when hostapd_set_wds_sta() has been called with enable=1
	 * for this station. Cleared when hostapd_set_wds_sta() is called with
	 * enable=0. Guards against redundant enable/disable calls.
	 */
	bool wds_ie_active;
};

/**
 * struct esp_update_event - Data for EVENT_ESP_UPDATE
 * @link_id: Link for which ESP airtime update was received
 * @airtime: Airtime fraction computed in the firmware
 */
struct esp_update_event {
	u8 link_id;
	u8 airtime;
};

struct dcs_intf_event {
	u32 freq;
	enum chan_width chan_width;
	u32 cf1;
	u32 cf2;
	u32 chan_bw_interference_bitmap;
	u8 link_id;
	u16 type;
};

struct i802_bss_extn {
	struct i802_link *scan_link_extn;
};

struct cbs_event {
	u32 scan_complete_freq;
	enum scan_status status;
	u8 link_id;
};

struct scan_results_event {
	struct cbs_event cbs_event;
};

struct chan_params {
	u32 cf1;
	u32 cf2;
	enum chan_width chan_width;
	u16 punct_bitmap;
};

union wpa_event_data_extn {
	struct esp_update_event esp_update_event;
	struct dcs_intf_event dcs_intf_event;
	struct hostapd_hw_blocklist_info hw_blocklist_info;
	struct scan_results_event scan_results_event;
	struct {
		u8 adfs_capable;
		u8 radio_idx;
	} agile_capable;
};

#define EXTN_MAX_BLOCK_CHAN_LIST 255

struct hostapd_extn_block_chan_list {
	u8 n_chan;
	u8 chans[EXTN_MAX_BLOCK_CHAN_LIST];
};

struct ieee802_11_elems_extn {
	const u8 *eht_240mhz_capab;
	u8 eht_240mhz_capab_len;

	/* HE MCS 12/13 (4K-QAM) peer capability from QCN IE */
	u16 he_mcs_12_13_peer_cap;

	/* VHT MCS 10/11 (1024-QAM) peer support from QCN IE (attr 0x02).
	 * Non-zero when the peer advertised support; zero otherwise.
	 */
	u8 vht_mcs10_11_peer_cap;
	/* HE 400ns SGI peer support from QCN IE attr 0x03.
	 * bit 0: peer supports RX 1xLTF + 0.4us GI
	 * bit 1: peer supports RX 2xLTF + 0.4us GI
	 */
	u8 he_400ns_sgi_peer_cap;
	/* HE 2xLTF in 160/80+80 MHz peer support from QCN IE attr 0x04. */
	u8 he_2xltf_160_80p80_peer_cap;

	/*
	 * wds_ie / wds_ie_len - WDS vendor IE from received frames
	 *
	 * Set by ieee802_11_parse_vendor_specific_elems_extn() when the
	 * WDS vendor IE (OUI 00:13:84, type 0x01) is found.
	 * Points to the OUI byte (first byte of the vendor IE payload,
	 * i.e. after the EID and Length octets).  wds_ie_len is the
	 * Length field value (OUI + Type + Cap + Ver = 6 bytes minimum).
	 */
	const u8 *wds_ie;
	u8 wds_ie_len;
};

/* WDS IE OUI as a 24-bit big-endian integer (00:13:84) */
#define OUI_WDS_IE  0x001384

#ifndef CONFIG_QCN_APP_EXTN
struct qacs_conf_extn {
	bool rank_en;	/* Channel ranking enable (1) / disable (0) */
	u8 acs_pri_sec20_penalty;	/* PRI_SEC20 usability penalty percentage */
	bool wradar;	/* Wideband radar handling enable (1) / disable (0) */
	int rep_txpower_policy;/* Report/tx power policy: accepts only 1 or 2 */

	/* Dwell time bounds and current dwell time in milliseconds */
	u16 min_dwell;
	u16 max_dwell;
	u16 dwelltime;
	u8 dcs_channel_penalty;

	/* Debug/trace controls:
	 * - upper 0xFF00: module bitmap
	 * - lower 0x00FF: debug level
	 */
	u16 dbg_module_bitmap;
	u8 dbg_level;
	bool acs_2g_scan_all;
};
#endif

struct hostapd_data_extn {
#ifdef CONFIG_IEEE80211AC
	/* Per-BSS control for MU-MIMO capability override WAR */
	bool mu_cap_war;
	bool mu_cap_war_override;
	struct dl_list mu_cap_war_sta_list;
#endif /* CONFIG_IEEE80211AC */
	/* Dedup list: STAs for which a trigger event was already sent. */
	struct dl_list log_trigger_sent;
};

struct hapd_interfaces_extn {
	/* Common BH STA connection state for dependent repeater AP bring-up. */
	bool bh_sta_connected;
};

/* NOL IE vendor element encoding/decoding constants */
#define DFS_NOL_IE_FIXED_HDR_LEN 7 /* EID+Len+OUI(3)+Type+Count */
#define DFS_NOL_IE_ENTRY_LEN 10    /* freq(4)+bw(4)+bitmap(2) */
#define DFS_NOL_IE_MIN_LEN DFS_NOL_IE_FIXED_HDR_LEN

struct dfs_nol_ie_info_extn {
	u32 freq;              /* Center frequency in MHz */
	u32 bandwidth;         /* Bandwidth in MHz (20, 40, 80, 160, 320) */
	u16 subchan_bitmap;    /* Bitmap of affected 20MHz subchannels */
};

#ifdef CONFIG_IEEE80211AC
struct hostapd_mu_cap_war_sta_entry_extn {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	struct os_reltime last_probe_time;
};

#define is_mu_cap_war_active(hapd)				\
({								\
	struct hostapd_data_extn *h_ext = &(hapd)->hapd_extn;   \
	(h_ext->mu_cap_war && !h_ext->mu_cap_war_override);	\
})

#define is_sta_vht_only(sta) \
	((sta)->flags & WLAN_STA_VHT) && \
	!((sta)->flags & (WLAN_STA_HE | WLAN_STA_EHT))

#define is_sta_elems_vht_only(elems) \
	((elems)->vht_capabilities || (elems)->vendor_vht) && \
	!(elems)->he_capabilities && !(elems)->eht_capabilities

#define MU_CAP_WAR_DB_ENTRY_TIMEOUT_SEC 300

#endif /* CONFIG_IEEE80211AC */

struct dcs_conf_extn {
	/* Last configured DCS enable value */
	u16 enable_bitmap;

	u16 bw_reduction_ctrl;

	/* CSA TBTT value */
	u32 dcs_csa_tbtt;

	/* Stored DCS WLAN interference parameters */
	u32 intr_detection_threshold;
	u32 phyerr_penalty;
	u32 phyerr_threshold;
	u32 radarerr_threshold;
	u32 txerr_threshold;
	u32 sample_size;
	u8 coch_intr_threshold;
	u8 user_max_cu;

	/* Random channel selection enable bitmap
	 * 0 = Disabled.
	 * BIT(0) = CW, BIT(1) = WLAN, BIT(2) = AWGN, BIT(4) = OBSS
	 * BITs 5..7 reserved.
	 */
	u8 dcs_random_chan_bitmap;

	/*
	 * DCS event action bitmap.
	 * 0 = discard all DCS interference events.
	 * BIT(0) = process CW, BIT(1) = process WLAN,
	 * BIT(4) = process OBSS.
	 */
	u16 dcs_event_action;

	/*
	 * DCS event notification bitmap.
	 * 0 = do not emit INTERFERENCE_DETECTED event.
	 * BIT(0) = notify CW, BIT(1) = notify WLAN,
	 * BIT(4) = notify OBSS.
	 */
	u16 dcs_event_notify;
};

enum tpe_tx_pwr_interp_unit {
	TPE_REG_EIRP_PSD = 0,	/* Interpretation PSD */
	TPE_REG_EIRP = 1,	/* Interpretation EIRP */
};

#define QCN_HOP_COUNT_CONNECTED 1
#define QCN_HOP_COUNT_UNKNOWN 255

struct hostapd_config_extn {
	/* Add Per-radio configuration for extn here */

	/* Manages RNR advertisement of 6 GHz BSS information for both
	 * in-band and out-of-band for each frame type includes Beacon,
	 * Probe Response and FILS discovery frame.
	 */
	u8 rnr_6ghz_colocated_enable;
	bool rnr_ess_colocated_en;
	bool rnr_6ghz_override;
	bool skip_cac;    /* Skip DFS CAC for Repeater AP */
	bool ignorecac;   /* Skip DFS CAC for Root AP */
	int ind_rptr;    /* 1 - Independent Rep; 0 - Dependent */
	bool repeater;   /* 1 - Repeater config; 0 - Not a Repeater */
	/* Same SSID Repeater
	 * 0 = different SSIDs or not a repeater
	 * 1 = same SSID repeater configuration
	 */
	int same_ssid;
	bool qacs_enable;
	bool uplink_csa;
	bool rpt_max_phy;
	/* 1 - Allow channel switch for Repeater AP, when BH STA is connected
	 * 0 - Disallow channel switch for Repeater AP, when BH STA is not connected
	 */
	bool rptr_allow_chan_sw;
	u32 acs_periodic_interval;
	bool disable_iface_during_cac; /* Disable iface creation during DFS CAC (boot-up CAC feature off) */

	/* Indicates whether HE MCS 12/13 support is enabled
	 * (the support is enabled by default)
	 */
	bool he_mcs_12_13_enabled;
	bool rcsa_tx;
	bool process_rcsa;
	struct qacs_conf_extn qacs_conf;
	struct chan_params cur_chan_params;
	struct dcs_conf_extn dcs_conf;
	struct hostapd_extn_block_chan_list block_chan_list;

	/*
	 * Primary channel list – restricts ACS, DFS channel hopping, and
	 * scan operations to a user-configured subset of the regulatory
	 * channel list.
	 * Stored as frequencies (MHz, u16 is sufficient for all bands).
	 */
	u16 primary_freq_list[MAX_NUM_CHANNELS];
	u8 num_primary_freq;

	/* MLO Repurpose specific configurations */
	u16 repurpose_vht_width;
	u16 repurpose_he_width;
	u16 user_repurpose_vht_width;
	u16 user_repurpose_he_width;

	/* OBSS SNR thresholds */
	u8 obss_snr_threshold;    /* OBSS SNR threshold */
	u8 obss_rx_snr_threshold; /* OBSS RX SNR threshold */
	u8 opclass_tbl_idx;       /* Country IE operating class table index */

	/* Channel Switch Options bitmap
	 * Bit 0 (0x1): Random non DFS channel selection
	 * Bit 1 (0x2): Ignore CSA from Root AP on DFS
	 * Bit 2 (0x4): CAC before joining Root AP
	 * Bit 3 (0x8): Repeater AP propagates CSA received from RootAP
	 * Bit 4 (0x10): Send RCSA on radar detection
	 * Bit 5 (0x20): Process RCSA from downstream
	 * Bit 6 (0x40): Apriori next channel propagation
	 */
	unsigned int cswopts;

	/* Config to set EHT operation CCFS0 to 0*/
	bool eht_config_ccfs0;

	/* Auto-recovery after NOL VAP down */
	bool autorecovery_after_nol_vapdown;

	/* CBS (Continuous Background Scan) params */
	struct cbs_params_extn cbs_params;

	/* UDBG client config loaded from hostapd.conf */
	bool udbg_enh_enable;
	char udbg_enh_server_ip[UDBG_ENH_SERVER_IP_SIZE];
	int udbg_enh_server_port;
	char udbg_enh_app_id[UDBG_ENH_APP_ID_SIZE];
	size_t udbg_enh_records;
	size_t udbg_enh_ring_max_bytes;
	uint32_t udbg_enh_service_delay_ms;
};

/**
 * struct check_40mhz_2g4_extn_args - Extension arguments for check_40mhz_2g4
 * @threshold: Minimum SNR (dB) a BSS entry must have to be considered in the
 *             40 MHz coexistence check. BSS entries with bss->snr below this
 *             value are silently ignored. Default 0 passes all normal signals.
 */
struct check_40mhz_2g4_extn_args {
	u8 threshold; /* OBSS SNR threshold in dB */
};

/**
 * struct handle_action_extn_args - Extension arguments for handle_action
 * @rssi: Rssi (dB) of the received action frame. Used to filter weak frames
 * against obss_rx_snr_threshold.
 */
struct handle_action_extn_args {
	int rssi;
};

/**
 * struct ht40_intolerant_bss_extn - Per-BSS HT40 intolerance CLI override.
 * @is_overridden: Whether HT40 intolerance is overridden via CLI.
 * @value: HT40 intolerance value set by the user.
 */

struct ht40_intolerant_bss_extn {
	bool is_overridden;
	bool value;
};

/**
 * struct puren_bss_extn - Per-BSS pure-N CLI override.
 * @is_overridden: Whether pure-N mode is overridden via CLI.
 * @value: pure-N value set via cli.
 */
struct puren_bss_extn {
	bool is_overridden;
	bool value;
};

/**
 * struct pure11ac_bss_extn - Per-BSS pure-11AC CLI override.
 * @is_overridden: Whether pure-11AC mode is overridden via CLI.
 * @value: pure-11AC value set via cli.
 */
struct pure11ac_bss_extn {
	bool is_overridden;
	bool value;
};

/**
 * struct pure11ax_bss_extn - Per-BSS pure-11AX CLI override.
 * @is_overridden: Whether pure-11AX mode is overridden via CLI.
 * @value: pure-11AC value set via cli.
 */
struct pure11ax_bss_extn {
	bool is_overridden;
	bool value;
};

struct hostapd_bss_config_extn {
	/* Add Per-BSS configuration for extn here */
	u8 nontx_vendor_elem_size;
	u8 nontx_optional_elem_size;
	enum repurpose_mode repurpose_mode;
	enum qca_wlan_vendor_vap_submode_type vap_submode;
	/* Config to enable/disable HT40 intolerance */
	struct ht40_intolerant_bss_extn ht40_intol;
	/* Config to control adding single common PSD to TPE IE */
	bool tpe_common_psd;
	/* Config to set Tx power interpretation in the TPE IE */
	enum tpe_tx_pwr_interp_unit tpe_tx_pwr_interp;
	/* Config to set the minimum TX power for punctured channels in the TPE IE */
	bool tpe_punct_channel_tx_pwr;
	/* Config to set user defined opclass in ECSA IE */
	u8 ecsa_opclass;
	/*
	 * When set to true, the BSS operates in pure IEEE 802.11g mode.
	 *
	 * By default, an IEEE 802.11g BSS supports association from both
	 * IEEE 802.11b and IEEE 802.11g STAs. When pureg_bss is enabled,
	 * the BSS shall not allow association from any IEEE 802.11b STA.
	 */
	bool pureg_bss;
	/* Enable VHT MCS 10/11 (1024-QAM) support via QCN IE negotiation.
	 * true = enabled (default), false = disabled.
	 */
	bool vht_mcs_10_11_supp;
	/* Enable VHT MCS 10/11 for non-QCA peers that did not advertise
	 * support via QCN IE.  false = disabled (default), true = enabled.
	 */
	bool vht_mcs_10_11_nq2q_peer_supp;
	/* Enable HE 400ns SGI (0.4us GI) support via QCN IE negotiation.
	 * true = enabled (default when HW supports it), false = disabled.
	 */
	bool he_400ns_sgi_supp;
	/* Enable HE 2xLTF in 160/80+80 MHz support via QCN IE negotiation.
	 * true = enabled (default when HW supports it), false = disabled.
	 */
	bool he_2xltf_160_80p80_supp;
	/*
	 * When set to true, the BSS operates in pure IEEE 802.11n mode.
	 *
	 * By default, an IEEE 802.11n BSS supports association from both
	 * HT-capable and non-HT STAs. When puren_bss is enabled, the BSS shall not
	 * allow association from non-HT STA.
	 * This per-BSS configuration overrides the per-radio 'require_ht' config.
	 */
	struct puren_bss_extn puren_bss;
	/*
	 * When set to true, the BSS operates in pure IEEE 802.11ac mode.
	 *
	 * By default, an IEEE 802.11ac BSS supports association from both
	 * VHT-capable and non-VHT STAs. When pure11ac_bss is enabled, the BSS shall not
	 * allow association from non-VHT STA.
	 * This per-BSS configuration overrides the per-radio 'require_vht' config.
	 */
	struct pure11ac_bss_extn pure11ac_bss;
	/*
	 * When set to true, the BSS operates in pure IEEE 802.11ax mode.
	 *
	 * By default, an IEEE 802.11ax BSS supports association from both
	 * HE-capable and non-HE STAs. When pure11ax_bss is enabled, the BSS shall not
	 * allow association from non-HE STA.
	 * This per-BSS configuration overrides the per-radio 'require_he' config.
	 */
	struct pure11ax_bss_extn pure11ax_bss;
	/*
	 * wds_ie - WDS vendor IE advertisement control (AP mode)
	 *
	 * When set to 1, the AP advertises the WDS vendor IE (OUI 00:13:84,
	 * type 0x01) in beacon, probe response, and association response
	 * frames.  The AP also parses the WDS IE from association requests
	 * and calls hostapd_set_wds_sta() to enable WDS mode for stations
	 * that mutually advertise WDS_IE_CAP_STA.
	 *
	 * 0 = WDS IE disabled (default)
	 * 1 = WDS IE enabled
	 */
	int wds_ie;
	int allow_3addr_mc;
};

struct esp_extn {
	u8 airtime;
	u8 ppdu_dur;
	u8 ba_window;
	u8 enable;
	u32 computed_airtime;
};

enum dynamic_acs_action_extn {
	DYNAMIC_ACS_DISABLE = 0,
	CHANNEL_CHANGE_CSA = 1,  // Perform CSA
	NO_CHANNEL_CHANGE = 2,  // Report-only
};

#define HOSTAPD_DCS_MAX_TRIGGERS 3
#define HOSTAPD_DCS_AGING_TIME_SEC 300

#define ALLOWED_DCS_EVENT_ACTION_MASK (DCS_CW_INTF | DCS_WLAN_INTF | DCS_OBSS_INTF)

#define DCS_ENABLE_TIME         (30 * 60)
#define DCS_ENABLE_TIME_MIN     (5 * 60)
#define DCS_ENABLE_TIME_MAX     (60 * 60)

#define HOSTAPD_DCS_REENABLE_TIME_SEC DCS_ENABLE_TIME

#define RCSA_MAX_OPTIONAL_IE_LEN 32

struct hostapd_rcsa_ctx {
	bool rcsa_inprogress;
	s8 bh_discon_wait_cnt;
	u8 optional_ie[RCSA_MAX_OPTIONAL_IE_LEN];
	u8 optional_ie_len;
	s8 rcsa_tx_cnt;
};

struct hostapd_iface_extn {
	struct esp_extn esp;
	u16 csa_bitmap;
	bool acs_success;
	bool acs_failed;
	enum dynamic_acs_action_extn dynamic_acs_action;
	bool dfs_available_from_sta;
	struct dfs_nol_ie_info_extn nol_info; /* single NOL entry for uplink CSA */
	bool nol_info_valid;                  /* true when nol_info holds a valid entry */
	bool periodic_acs_timer_set;

	/* Penalty percentage to be applied for non-priority channels in QACS */
	u8 vlp_non_prior_penalty;

	bool dfs_no_wradar;
	bool ignorecac;
	bool allow_scan_on_dfs_chan;
	char sta_wpa_state[32]; /* Stores the STA WPA state, in case of repeater */
	bool acs_dfs_cac_pending;  /* ACS picked DFS channel, waiting for CAC */
	int vap_type;
	u16 vlp_threshold_freq; /* Stores 6 GHz VLP priority threshold frequency */

	struct os_reltime dcs_trigger_ts[HOSTAPD_DCS_MAX_TRIGGERS]; /* Recent DCS trigger timestamps */
	u8 dcs_trigger_count; /* Valid entries in dcs_trigger_ts[] */
	bool dcs_reenable_timer_set; /* DCS re-enable timer status: active or not */
	u32 dcs_re_enable_time;
	bool dcs_disabled_excessive_triggers; /* DCS disabled state due to execessive triggers*/
	u16 dcs_excess_trigger_enable_bitmap; /* Bitmap used while DCS is disabled due to excessive triggers*/
	u16 dcs_excess_trigger_restore_bitmap; /* Bitmap restored after DCS is enabled back */
	bool dcs_in_progress; /* DCS-triggered channel switch is in progress */
	unsigned int cac_abort:1;
	/* Radio capability for HE MCS 12/13 support */
	u16 he_mcs_12_13_radio_cap;

	/* Peer capability for HE MCS 12/13 support. */
	u16 he_mcs_12_13_peer_cap;

	struct hostapd_hw_blocklist_info *hw_blocklist_info;
	unsigned int num_hw_blocklist;
	bool check_hw_blocklist;
	struct hostapd_rcsa_ctx rcsa_ctx;

	bool agile_capable;

	/* Regulatory: interface waiting for regulatory domain update for AFC. */
	bool is_waiting_for_afc_regd;
	/* Regulatory: interface forced to NO_IR due to regulatory constraint.
	 * Recovery attempted on the next REGDOM_SET_BY_DRIVER event. */
	bool is_regdom_forced_down;
	/* CAC was running when country change was detected.  The disable has
	 * been done; enable is deferred until DFS_CAC_ABORTED is received. */
	bool country_change_post_cac_abort;
};

struct hostapd_channel_data_extn {
	bool is_non_primary;
	long double acs_total_interference_factor;
	s8 secondary_channel;
	u8 eht_bw320_offset;

	u8 acs_channel_penalty;
};

struct hostapd_hw_modes_extn {
#ifdef CONFIG_QCN_APP_EXTN
	struct qacs_data_extn qacs_extn;
#endif
};

#ifdef CONFIG_QCN_APP_EXTN 
#define NORMAL_SCAN_TRIGGER normal_scan /* normal_scan and cbs_vendor_scan are defined in qacs.h file */
#define CBS_SCAN_TRIGGER cbs_vendor_scan
#else
#define NORMAL_SCAN_TRIGGER 0
#define CBS_SCAN_TRIGGER 1
#endif


enum hostapd_dcs_intf_type {
	DCS_CW_INTF     = 0x0001,
	DCS_WLAN_INTF   = 0x0002,
	DCS_AWGN_INTF   = 0x0004,
	DCS_AFC_INTF    = 0x0008,
	DCS_OBSS_INTF   = 0x0010,
};

/**
 * struct wpa_supplicant_extn - QCN extension struct for struct wpa_supplicant.
 * @he_mcs_12_13_radio_cap: Self hardware capability for HE MCS 12/13 support.
 * @he_mcs_12_13_peer_cap: Peer capability for HE MCS 12/13 support.
 * @hw_blocklist_info: HW blocklist channel info array.
 * @num_hw_blocklist: Number of HW blocklist entries.
 * @check_hw_blocklist: Flag indicating HW blocklist check is needed.
 * @is_waiting_for_afc_regd: Regulatory: station waiting for regulatory domain update for AFC.
 * @active_alpha2: Last active regulatory alpha2 seen via a REGDOM_TYPE_COUNTRY event.
 *                 Used to detect actual country changes vs. repeated events.
 */
struct wpa_supplicant_extn {
	u16 he_mcs_12_13_radio_cap;
	u16 he_mcs_12_13_peer_cap;
	struct hostapd_hw_blocklist_info *hw_blocklist_info;
	unsigned int num_hw_blocklist;
	bool check_hw_blocklist;
	bool is_waiting_for_afc_regd;
	/* Last active regulatory alpha2 seen via a REGDOM_TYPE_COUNTRY event.
	 * Used to detect actual country changes vs. repeated events. */
	char active_alpha2[3];
};

/**
 * struct wpa_config_extn - QCN extension configuration parameters.
 * @he_mcs_12_13_enabled: Indicates whether HE MCS 12/13 support is enabled.
 *                        (enabled by default)
 * @vht_mcs_10_11_supp: Enable VHT MCS 10/11 (1024-QAM) support via QCN IE
 *                      true = enabled (default), false = disabled
 * @he_400ns_sgi_supp: Enable 400ns SGI in HE mode using QCN IE
 * @he_2xltf_160_80p80_supp: Enable 2x LTF in HE mode using QCN IE for
 *                           160/80+80 BW
 */
struct wpa_config_extn {
	bool he_mcs_12_13_enabled;
	int allow_3addr_mc;
	bool vht_mcs_10_11_supp;
	bool he_400ns_sgi_supp;
	bool he_2xltf_160_80p80_supp;
};

int get_centre_freq_6g(int chan_idx, int chan_width, int *centre_freq);
int get_next_max_width(int chan_width);

#define IEEE80211_MS_TO_TU(x) (((x) * 1000) / 1024)
#define IEEE80211_TU_TO_MS(x) (((x) * 1024) / 1000)
#define HOSTAPD_NON_CAC_SWITCH_TIME_TU_EXTN(beacon_int) \
	(IEEE80211_MS_TO_TU(250) + (2 * (beacon_int)))
#define HOSTAPD_NON_CAC_SWITCH_TIME_MSEC_EXTN(beacon_int) \
	IEEE80211_TU_TO_MS(HOSTAPD_NON_CAC_SWITCH_TIME_TU_EXTN(beacon_int))

static inline bool hostapd_mcst_allows_skip_cac_extn(u32 mcst,
						     u16 beacon_int,
						     bool is_dfs)
{
	return (is_dfs && mcst &&
		(mcst <= HOSTAPD_NON_CAC_SWITCH_TIME_TU_EXTN(beacon_int)));
}

#ifndef CONFIG_QCN_EXTN

static inline void
hostapd_get_oper_center_freq_seg_extn(struct hostapd_config *conf,
				      u8 *oper_centr_freq_seg0_idx,
				      u8 *oper_centr_freq_seg1_idx,
				      enum oper_chan_width *oper_chwidth)
{
	return;
}

static inline u8
hostapd_set_legacy_oper_centr_freq_seg0_extn(struct hostapd_config *conf,
					     u8 oper_centr_freq_seg0_idx)
{
	return oper_centr_freq_seg0_idx;
}

static inline int
hostapd_modify_n_chans_for_240mhz_extn(struct hostapd_iface *iface,
				       int n_chans)
{
	return n_chans;
}

static inline int
hostapd_modify_supported_op_class_for_240mhz_extn(int freq,
						  enum oper_chan_width ch_width,
						  u8 *op_class)
{
	return -1;
}

static inline void hostapd_dcs_restore_extn(struct hostapd_iface *iface,
					    const char *reason)
{
	return;
}

static inline void hostapd_dcs_iface_deinit_extn(struct hostapd_iface *iface)
{
	return;
}

static inline int
hostapd_get_n_chans_and_frequency_extn(enum oper_chan_width oper_chwidth,
				       int cf1,
				       int *n_chans,
				       int *frequency)
{
	return -1;
}

static inline int hostapd_get_dfs_half_chwidth_extn(enum chan_width width)
{
	return 0;
}

static inline int
hostapd_dfs_get_allowed_channels_extn(int n_chans,
				      int *is_allowed,
				      unsigned int *allowed_no)
{
	return -1;
}

static inline int
hostapd_dfs_adjust_center_freq_extn(int oper_chwidth,
				    short chan,
				    u8 *oper_centr_freq_seg0_idx,
				    u8 *oper_centr_freq_seg1_idx)
{
	return -1;
}

static inline int
hostapd_get_bw_and_startchan_for_240mhz_extn(enum oper_chan_width
					     eht_oper_chwidth,
					     u8 eht_oper_centr_freq_seg0_idx,
					     u16 *bw, u8 *start_chan)
{
	return -1;
}

static inline size_t
hostapd_modify_buflen_for_qcn_ie_extn(struct hostapd_data *hapd)
{
	return 0;
}

static inline size_t
hostapd_qcn_buflen_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd)
{
	return 0;
}

static inline u8 *
hostapd_qcn_eid_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd,
					       u8 *pos)
{
	return pos;
}

static inline size_t
wpas_modify_buflen_for_qcn_ie_extn(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline u8 *
hostapd_eid_qcn_vendor_ie_extn(struct hostapd_data *hapd, u8 *eid,
				int opmode)
{
	return eid;
}

static inline u8 *
wpas_eid_qcn_vendor_ie_extn(struct wpa_supplicant *wpa_s, u8 *eid)
{
	return eid;
}

static inline void
wpas_add_qcn_ie_probe_req_extn(struct wpa_supplicant *wpa_s,
			       struct wpabuf **extra_ie)
{
	return;
}

static inline void
wpas_add_qcn_ie_assoc_req_extn(struct wpa_supplicant *wpa_s)
{
	return;
}

static inline u16
hostapd_copy_sta_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     int opmode,
				     struct ieee802_11_elems_extn *elems_extn)
{
	return 0;
}

static inline void
hostapd_drv_set_peer_he_mcs_12_13_cap_extn(struct hostapd_data *hapd,
					   struct ieee802_11_elems_extn *elems_extn)
{
	return;
}

static inline void
hostapd_drv_set_vht_mcs_10_11_supp_extn(struct hostapd_data *hapd,
					struct sta_info *sta,
					struct ieee802_11_elems_extn *elems_extn)
{
}

static inline void
hostapd_drv_set_he_400ns_sig_2xltf_160_supp_extn(struct hostapd_data *hapd,
						 struct sta_info *sta,
						 struct ieee802_11_elems_extn
						 *elems_extn)
{
}

static inline void
hostapd_set_sta_vht_mcs10_11_and_he_cap_internal_extn(struct hostapd_data *hapd,
						      struct sta_info *sta)
{
}

static inline void
wpas_drv_set_peer_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq,
					const u8 *ies, size_t ies_len)
{
	return;
}

static inline void
wpa_bss_check_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
				       struct wpa_bss *bss)
{
	return;
}

static inline bool
wpas_sta_cac_5g_320mhz_update_freq_params_extn(enum chan_width width,
					     u8 cf2_idx,
					     struct hostapd_freq_params *params)
{
	return false;
}

static inline void
wpas_ch_switch_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					union wpa_event_data *data)
{
	return;
}

static inline void
hostapd_get_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				struct sta_info_extn *sta_extn,
				struct ieee80211_240mhz_vendor_oper_extn *dest)
{
	return;
}

static inline void hostapd_sta_os_free_extn(struct sta_info_extn *sta_extn)
{
	return;
}

static inline int
ieee802_11_parse_vendor_specific_eht_240mhz_cap_extn(struct ieee802_11_elems
						     *elems,
						     unsigned int oui_flag,
						     const u8 *pos,
						     size_t elen)
{
	return -1;
}

static inline int
ieee802_11_parse_vendor_specific_elems_extn(struct ieee802_11_elems *elems,
					    unsigned int oui_flag,
					    const u8 *pos, size_t elen)
{
	return -1;
}

static inline void
hostapd_copy_sta_add_params_extn(struct hostapd_sta_add_params_extn
				 *params_extn,
				 struct sta_info_extn *sta_extn)
{
	return;
}

static inline void
wpa_driver_nl80211_sta_add_extn(void *priv,
				struct hostapd_sta_add_params
				*params)
{
	return;
}

static inline int
hostapd_drv_fetch_and_set_vendor_bssid_extn(struct hostapd_data *hapd)
{
	return -1;
}

static inline void
hostapd_free_bss_index_extn(struct hostapd_data *hapd)
{
	return;
}

inline int wpa_driver_nl80211_dcs_config_extn(void *priv,
					      u8 link_id,
					      struct driver_dcs_config *params)
{
	return -1;
}

inline int wpa_driver_nl80211_dcs_sim_extn(void *priv,
					   u8 link_id,
					   struct driver_dcs_sim *params)
{
	return -1;
}

static inline void *wpa_driver_nl80211_get_survey_extn(struct i802_bss *bss,
						       void *ctx)
{
	return ctx;
}

inline int nl80211_disable_opclass_chans_extn(void *priv, u8 link_id,
					      u8 is_disable,
					      u8 opclass,
					      const u8 *chan_list,
					      size_t chan_count)
{
	return -1;
}

static inline int netlink_increase_rcvbuf_extn(int sock)
{
	return -1;
}

static inline int wpa_driver_nl80211_cbs_trigger_scan(void *priv,
						      const struct cbs_params_extn *params,
						      int *freq_list, int link_id)
{
	return -1;
}

static inline int
nl80211_notify_radar_detected_extn(void *priv,
				   struct hostapd_freq_params *freq)
{
	return -1;
}

static inline bool
hostapd_dfs_get_valid_punc_bitmap_extn(int chan_freq,
				       u16 punct_bitmap,
				       int center_freq,
				       int half_width)
{
	return false;
}

static inline int
hostapd_find_dfs_range_extn(struct hostapd_iface *iface,
			    enum chan_width bandwidth,
			    struct hostapd_freq_params *freq_params)
{
	return -1;
}

static inline bool
hostapd_dfs_skip_wradar_chan_extn(struct hostapd_iface *iface,
				  struct hostapd_hw_modes *mode,
				  struct hostapd_channel_data *chan,
				  int first_chan_idx, int n_chans)
{
	return false;
}

static inline int
hostapd_is_dfs_overlap_extn(struct hostapd_iface *iface,
			    enum chan_width width,
			    int center_freq, u16 punct_bitmap)
{
	return -1;
}

static inline void
hostapd_modify_supported_op_class_for_320mhz_extn(int freq,
						  u8 *op_class)
{
	return;
}

static inline bool
hostapd_skip_rnr_6ghz_colocated_extn(struct hostapd_data *hapd, u32 type)
{
	return false;
}

static inline bool
hostapd_rnr_colocated_ess_indication_extn(struct hostapd_data *hapd, u32 type)
{
	return false;
}

static inline int
hostapd_ctrl_iface_receive_process_extn(struct hostapd_data *hapd,
					char *buf, char *reply,
					int reply_size,
					struct sockaddr_storage *from,
					socklen_t fromlen, int *reply_len)
{
	return -EOPNOTSUPP;
}

static inline void
hostapd_config_defaults_extn(struct hostapd_config *conf)
{
	return;
}

static inline void
hostapd_config_defaults_bss_extn(struct hostapd_bss_config *bss)
{
	return;
}

static inline int
hostapd_config_fill_extn(struct hostapd_config *conf,
			 struct hostapd_bss_config *bss,
			 const char *buf, char *pos, int line)
{
	return -EOPNOTSUPP;
}

static inline int
hostapd_ctrl_iface_set_extn(struct hostapd_data *hapd, char *cmd, char *value)
{
	return -EOPNOTSUPP;
}

static inline int
hostapd_ctrl_iface_status_extn(struct hostapd_data *hapd, char *buf,
			       size_t buflen, size_t curr_len)
{
	return curr_len;
}

static inline int
wpas_ctrl_iface_set_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			 const char *value, bool *is_extn_cmd)
{
	*is_extn_cmd = false;
	return -EOPNOTSUPP;
}

static inline int
wpas_ctrl_iface_get_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			 char *buf, size_t buflen, bool *is_extn_cmd)
{
	*is_extn_cmd = false;
	return -EOPNOTSUPP;
}

static inline int
wpa_ctrl_get_freq_list_extn(struct wpa_supplicant *wpa_s,
			    char *reply, int reply_size)
{
	return -EOPNOTSUPP;
}

static inline int
wpa_ctrl_chan_sw_finished_notify_extn(struct wpa_supplicant *wpa_s,
				      const char *buf, char *reply,
				      int reply_size)
{
	return -EOPNOTSUPP;
}

static inline int
wpa_config_process_cswopts_extn(struct wpa_config *config, int line,
				const char *pos)
{
	return -EOPNOTSUPP;
}

static inline int
wpa_supplicant_ctrl_iface_set_cswopts_extn(struct wpa_supplicant *wpa_s,
					   const char *value)
{
	return -EOPNOTSUPP;
}

static inline int
compute_sec_channel_offset_extn(int primary_freq, int center_freq1,
				enum chan_width width)
{
	return -EOPNOTSUPP;
}

static inline void
wpa_get_bss_channel_oper_info_extn(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss)
{
	return;
}

static inline bool
compute_dfs_for_chanwidth_extn(int freq, int chanwidth)
{
	return false;
}

static inline void
wpa_supp_pre_connect_state_handle_extn(struct wpa_supplicant *wpa_s,
				       struct wpa_bss *bss)
{
	return;
}

static inline void
sme_pre_connect_timer_extn(void *eloop_ctx, void *timeout_ctx)
{
	return;
}

static inline void
wpa_bss_update_link_rnr_ap_info_extn(struct wpa_supplicant *wpa_s,
				     struct wpa_bss *bss,
				     const u8 *bssid_ptr,
				     const struct ieee80211_neighbor_ap_info *ap_info,
				     const u8 *mld_params, u8 link_id)
{
	return;
}

static inline void
hostapd_csa_bitmap_update_extn(struct hostapd_iface *iface, int freq)
{
	return;
}

static inline int
uc_hostapd_iface_switch_channel_extn(struct hostapd_iface *iface,
				     bool is_dfs, char *wpa_state,
				     struct csa_settings *csa)
{
	return -EOPNOTSUPP;
}

static inline void
hostapd_iface_set_supplicant_channel_extn(struct hostapd_iface *hapd_iface)
{
	return;
}

static inline int
nl80211_vendor_event_qca_extn(struct i802_bss *bss,
			      u32 subcmd, u8 *data, size_t len)
{
	return -1;
}

static inline int
hostapd_get_agile_capable_extn(struct hostapd_iface *iface)
{
	return -1;
}

static inline int
hostapd_wpa_event_extn(void *ctx, int event,
		       union wpa_event_data *data)
{
	return -1;
}

static inline int
wpa_supplicant_event_extn(struct wpa_supplicant *wpa_s,
			  int event,
			  union wpa_event_data *data)
{
	return -1;
}

static inline int
qca_nl80211_handle_wifi_config_evt_extn(struct i802_bss *bss,
					u8 *data, size_t len)
{
	return -1;
}

static inline void
nl80211_set_vendor_6ghz_hw_blocked_chans_support_extn(
	struct wpa_driver_nl80211_data *drv,
	const struct nl80211_vendor_cmd_info *vinfo)
{
}

static inline void
hostapd_query_hw_blocklist_extn(struct hostapd_iface *iface,
				struct hostapd_data *hapd)
{
}

static inline bool
hostapd_is_hw_blocklisted_combo_extn(struct hostapd_iface *iface,
				     u16 freq, u16 center_freq, u16 bw,
				     u16 puncture_pattern, u8 pwr_mode_id)
{
	return false;
}

static inline int
hostapd_validate_hw_blocklist_for_freq_params_extn(
	struct hostapd_iface *iface,
	const struct hostapd_freq_params *freq_params,
	u8 pwr_mode_id, const char *op_name)
{
	return 0;
}

static inline int
hostapd_validate_current_6ghz_hw_blocklist_extn(
	struct hostapd_iface *iface,
	u8 pwr_mode_id, const char *op_name)
{
	return 0;
}

static inline void
wpas_query_hw_blocklist_extn(struct wpa_supplicant *wpa_s)
{
}

static inline void
wpas_country_change_init_active_alpha2_extn(struct wpa_supplicant *wpa_s)
{
}

static inline bool
wpas_is_6ghz_hwbl_link_ok_extn(struct wpa_supplicant *wpa_s,
				const struct wpa_bss *bss)
{
	return true;
}

static inline void
wiphy_info_qca_vendor_command_extn(struct wpa_driver_nl80211_data *drv,
				   const struct nl80211_vendor_cmd_info *vinfo)
{
}

static inline
u8 * hostapd_eid_esp_extn(struct hostapd_data *hapd, u8 *eid, size_t len)
{
       return eid;
}

static inline
size_t hostapd_esp_ie_len_extn(struct hostapd_data *hapd)
{
       return 0;
}

static inline void
acs_process_hostapd_scan_data(struct hostapd_iface *iface) {}

static inline int hostapd_ctrl_iface_get_extn(struct hostapd_data *hapd, char *cmd,
					      char *buf, size_t buflen)
{
	return -1;
}

static inline void
acs_request_scan_add_freqs_extn(struct hostapd_channel_data *chan,
				int **freq) {}

static inline void
acs_modify_scan_params_extn(struct hostapd_iface *iface,
			    struct wpa_driver_scan_params *params) {}

static inline void
hostapd_ml_acs_check_and_notify(struct hostapd_iface *iface, bool status)
{
	return;
}

static inline void
wpa_supplicant_start_sta_scan(void *eloop_ctx, void *timeout_ctx)
{
	return;
}

static inline bool
check_40mhz_2g4_bss_snr_below_threshold_extn(
	const struct wpa_scan_res *bss,
	const struct check_40mhz_2g4_extn_args *extn_args)
{
	return false;
}

static inline bool
hostapd_2040_coex_action_snr_below_threshold_extn(
	struct hostapd_data *hapd, int rssi)
{
	return false;
}

static inline int
hostapd_rssi_to_snr_extn(struct hostapd_data *hapd, int ssi_signal)
{
	return ssi_signal;
}

static inline bool
hostapd_ht40_intolerant_snr_below_threshold_extn(
	struct hostapd_data *hapd, struct sta_info *sta)
{
	return false;
}

static inline int
acs_handle_channel_change_extn(struct hostapd_iface *iface,
			       struct hostapd_channel_data *chan,
			       int err)
{
	return -EOPNOTSUPP;
}

static inline void
acs_init_extn(struct hostapd_iface *iface, uint8_t trigger)
{
	return;
}

static inline int
hostapd_cbs_handle_single_channel_survey(struct hostapd_iface *iface,
					 struct hostapd_channel_data *chan,
					 struct freq_survey *survey)
{
	return -EOPNOTSUPP;
}

static inline int
acs_handle_channel_change_failed_extn(struct hostapd_iface *iface,
				      struct hostapd_channel_data *ideal_chan,
				      int err)
{
	return -EOPNOTSUPP;
}

static inline bool
acs_hwbl_candidate_ok(struct hostapd_iface *iface,
		      struct hostapd_channel_data *chan,
		      u32 bw, int bw320_offset, u16 punct_bitmap,
		      u8 nl80211_pwr_mode)
{
	return true;
}

static inline bool
acs_hwbl_chan_ok_extn(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
		      int n_chans, struct hostapd_channel_data *chan,
		      long double factor)
{
	return true;
}

static inline bool
hostapd_acs_is_chan_blocked(struct hostapd_iface *iface, u8 chan)
{
	return false;
}

static inline bool
hostapd_hwbl_validate_6ghz(struct hostapd_iface *iface,
			    struct hostapd_channel_data *chan,
			    u16 bw, u16 center_freq, u16 punct_bitmap,
			    u8 nl80211_pwr_mode)
{
	return true;
}

static inline bool
acs_scan_event_expected_extn(struct hostapd_iface *iface)
{
	return false;
}

static inline int
qca_nl80211_handle_dcs_config_evt_extn(struct i802_bss *bss,
				       u8 *data, size_t len)

{
	return -1;
}

static inline int
intf_awgn_find_channel_list(struct hostapd_iface *iface, int chan_width,
			    struct hostapd_channel_data ***chandef_list,
			    int *awgn_interference_freqs)
{
	return -1;
}

static inline void
update_chan_params(struct hostapd_data *hapd, int cf1, int cf2,
		   enum chan_width chwidth)
{
}

static inline int
hostapd_ctrl_iface_dcs_extn(struct hostapd_data *hapd, const char *cmd, char *reply,
			    int reply_size)
{
	return -1;
}

static bool
dcs_get_bw_reduction_ctrl_extn(struct hostapd_config *conf, u16 dcs_intf_type)
{
	return false;
}

static inline bool
hostapd_dcs_awgn_handle_rand_chan_disabled_extn(struct hostapd_iface *iface)
{
	return false;
}

static struct hostapd_channel_data *
get_chan_data_by_freq(struct hostapd_hw_modes *mode, int freq)
{
	return NULL;
}

static inline int
is_chan_range_available(struct hostapd_hw_modes *mode,
			int first_chan_idx, int num_chans)
{
	return -1;
}

static inline void
reduced_chan_width(struct hostapd_iface *iface, int *new_chan_width,
		   int chan_width, int freq,
		   struct hostapd_hw_modes *mode,
		   u32 chan_bw_interference_bitmap)
{
}

static inline int
hostapd_get_6g_chan_list_extn(struct hostapd_iface *iface,
			      char *buf, size_t buflen)
{
	return -1;
}

static inline void
hostapd_iface_init_extn(struct hostapd_iface *iface)
{
}

static inline void
hostapd_iface_deinit_extn(struct hostapd_iface *iface)
{
}

static inline void
hostapd_free_hw_features_extn(struct hostapd_hw_modes *hw_features,
			      size_t num_hw_features)
{
}

static inline void
wpas_iface_init_extn(struct wpa_supplicant *wpa_s)
{
}

static inline void
wpas_iface_deinit_extn(struct wpa_supplicant *wpa_s)
{
}

static inline int
intf_chan_range_available_5g(struct hostapd_hw_modes *mode,
			     int first_chan_idx, int num_chans)
{
	return -1;
}

static inline int
intf_chan_range_available_2g(struct hostapd_hw_modes *mode,
			     int first_chan_idx, int num_chans)
{
	return -1;
}

static inline int
get_centre_freq(struct hostapd_channel_data *first_chan,
		int chan_width, int *centre_freq)
{
	return -1;
}

static inline struct hostapd_iface *
hostapd_afc_find_iface(struct hostapd_data *hapd, int hw_idx)
{
	return NULL;
}

static inline int
intf_chan_range_available_6g(struct hostapd_hw_modes *mode,
			     int first_chan_idx, int num_chans)
{
	return -1;
}

static bool
is_chan_disabled(struct hostapd_hw_modes *mode, int chan_num)
{
	return false;
}

static inline int
chan_pri_allowed(const struct hostapd_channel_data *chan)
{
	return -1;
}

static inline int
hostapd_trigger_dynamic_acs(struct hostapd_data *hapd,
			    enum dynamic_acs_action_extn acs_action)
{
	return -1;
}

static inline void
hostapd_periodic_acs_start(struct hostapd_iface *iface)
{
}

static inline void
hostapd_periodic_acs_stop(struct hostapd_iface *iface)
{
}

static inline void
hostapd_periodic_acs_schedule(struct hostapd_iface *iface)
{
}

static inline int
hostapd_drv_dcs_config(struct hostapd_data *hapd, u8 link_id,
		       struct driver_dcs_config *params)
{
	return -1;
}

static inline int
nl80211_set_allow_3addr_mc_extn(void *priv, u8 val)
{
	return -1;
}

static inline int
wpa_ctrl_set_allow_3addr_mc_extn(struct wpa_supplicant *wpa_s, int val)
{
	return -1;
}

static inline bool
hostapd_is_bh_sta_connecting_or_connected_extn(struct hostapd_iface *iface)
{
	return false;
}

static inline bool
hostapd_is_bh_sta_link_connected_extn(struct hostapd_iface *iface)
{
	return false;
}

static inline void
hostapd_update_bh_sta_connected_extn(struct hostapd_iface *iface,
				     const char *wpa_state)
{
}

static inline void
dcs_enable_init(struct hostapd_data *hapd, u16 enable_bitmap)
{
}

static inline bool
hostapd_handle_csa_target_unavailable_extn(struct hostapd_data *hapd,
					   int freq, int finished)
{
	return false;
}

static inline bool
hostapd_5ghz_eht_320_channel_bw_extn(struct hostapd_hw_modes *mode,
				     int channel_idx)
{
	return false;
}

/* Primary channel list stubs (ChanSel-001..012) */
static inline int
hostapd_set_primary_chanlist(struct hostapd_data *hapd, const char *chan_str)
{
	return -1;
}

static inline int
hostapd_get_primary_chanlist(struct hostapd_iface *iface,
			     char *buf, size_t buflen)
{
	return -1;
}

static inline int
hostapd_is_chan_in_primary_list(struct hostapd_iface *iface, u16 freq)
{
	return 1;
}

static inline
bool chan_pri_allowed_extn(const struct hostapd_channel_data *chan)
{
	return 0;
}

static inline int
hostapd_update_assoc_resp_with_hop_count_extn(struct hostapd_data *hapd)
{
    return -1;
}

static inline int
nl80211_get_he_mcs_12_13_extn(void *priv, u8 radio_idx, u16 *radio_cap)
{
	return -1;
}

static inline int
nl80211_set_he_mcs_12_13_peer_cap_extn(void *priv, u8 radio_idx, u16 peer_cap)
{
	return -1;
}

static inline int
hostapd_set_he_mcs_12_13_peer_cap_extn(struct hostapd_data *hapd)
{
	return -1;
}

static inline int
wpas_set_he_mcs_12_13_peer_cap_extn(struct wpa_supplicant *wpa_s, int freq)
{
	return -1;
}

static inline int
hostapd_set_he_mcs_12_13_cap_extn(struct hostapd_data *hapd)
{
	return -1;
}

static inline int
wpas_set_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq)
{
	return -1;
}

static inline bool
wpas_bss_uses_nol_channel_extn(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	return false;
}

static inline bool
wpas_link_uses_nol_channel_extn(struct wpa_supplicant *wpa_s, int freq,
					enum chan_width width, int cf1, int cf2)
{
	return false;
}

static inline void
wpas_dfs_radar_detected_sta_mode(struct wpa_supplicant *wpa_s,
				      struct dfs_event *radar)
{
	return;
}

static inline void
wpas_dfs_nop_finished_sta_mode(struct wpa_supplicant *wpa_s,
				    struct dfs_event *radar)
{
	return;
}

static inline void
wpa_config_alloc_empty_extn(struct wpa_config *config)
{
	return;
}

static inline bool hostapd_regdom_bringup_failed_extn(struct hostapd_iface *iface)
{
	return false;
}


static inline void hostapd_set_afc_regd_wait(struct hostapd_iface *orig_iface)
{
	return;
}

static inline int hostapd_handle_regchannel_update_extn(struct hostapd_iface *iface,
							bool afc_regd)
{
	return 0;
}


static inline bool regdom_subchans_valid(struct hostapd_iface *iface)
{
	return false;
}

static inline
void hostapd_country_change_post_cac_abort_extn(struct hostapd_iface *iface)
{
	return;
}

static inline enum chan_width
hostapd_oper_chwidth_to_chanwidth_extn(int oper_chwidth,
				       int sec_channel_offset)
{
	return CHAN_WIDTH_20;
}

static inline void
hostapd_override_ht_capabilities_extn(const struct hostapd_data *hapd,
				      struct ieee80211_ht_capabilities *cap)
{
}

/* ------------------------------------------------------------------ */
/* WDS vendor IE stubs (CONFIG_QCN_EXTN not set)                       */
/* ------------------------------------------------------------------ */
static inline size_t
hostapd_wds_ie_len_extn(struct hostapd_data *hapd)
{
	return 0;
}

static inline u8 *
hostapd_eid_wds_ie_extn(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	return eid;
}

static inline u16
check_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta,
		  const u8 *wds_ie, size_t wds_ie_len)
{
	return 0; /* WLAN_STATUS_SUCCESS */
}

static inline void
handle_assoc_cb_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta)
{
	return;
}

static inline void
ap_free_sta_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta)
{
	return;
}

static inline size_t
wds_ie_assoc_req_len_extn(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	return 0;
}

static inline u8 *
wds_ie_populate_assoc_req_extn(struct wpa_supplicant *wpa_s,
			       struct wpa_ssid *ssid,
			       u8 *pos, size_t avail)
{
	return pos;
}

static inline void
wds_ie_process_assoc_resp_extn(struct wpa_supplicant *wpa_s,
			       const u8 *ies, size_t ies_len)
{
	return;
}

static inline void
wpa_bss_update_mld_link_mcst_extn(struct wpa_bss *bss, u8 link_id,
				  const u8 *ies, size_t ies_len)
{
	return;
}

static inline u32 wpa_bss_get_mld_link_mcst_extn(struct wpa_bss *bss,
						 u8 link_id)
{
	return 0;
}

static inline void
wpa_bss_parse_basic_mle_per_sta_mcst_extn(struct wpa_bss *bss,
					  struct wpabuf *mlbuf,
					  size_t common_info_len)
{
	return;
}

#else

int dfs_get_start_chan_idx(struct hostapd_iface *iface, int *seg1_start,
			   int chan_width, int channel_no,
			   bool is_offloaded_cac);
int dfs_get_used_n_chans(struct hostapd_iface *iface, int *seg1,
			 int chan_width);
void hostapd_get_oper_center_freq_seg_extn(struct hostapd_config *conf,
					   u8 *oper_centr_freq_seg0_idx,
					   u8 *oper_centr_freq_seg1_idx,
					   enum oper_chan_width *oper_chwidth);
u8 hostapd_set_legacy_oper_centr_freq_seg0_extn(struct hostapd_config *conf,
						u8 oper_centr_freq_seg0_idx);
int
hostapd_acs_update_puncturing_bitmap_extn(struct hostapd_config *conf,
					  u16 bw,
					  struct hostapd_channel_data *chan);
int hostapd_modify_n_chans_for_240mhz_extn(struct hostapd_iface *iface,
					   int n_chans);
int
hostapd_modify_supported_op_class_for_240mhz_extn(int freq,
						  enum oper_chan_width ch_width,
						  u8 *op_class);

int hostapd_is_usable_chan(struct hostapd_iface *iface, int frequency, int primary);

/**
 * hostapd_modify_buflen_for_qcn_ie_extn - Compute QCN Vendor IE byte count
 * @hapd: per-BSS hostapd context
 *
 * Computes the total number of bytes required for the QCN Vendor IE.
 * The caller should add the returned value to its buffer-length accumulator
 * before allocating the frame buffer.
 *
 * Returns: number of bytes needed for the QCN Vendor IE, or 0 if not needed
 */
size_t hostapd_modify_buflen_for_qcn_ie_extn(struct hostapd_data *hapd);

/**
 * wpas_modify_buflen_for_qcn_ie_extn - Compute QCN Vendor IE byte count
 * @wpas: wpa_supplicant context
 *
 * Computes the total number of bytes required for the QCN Vendor IE.
 * The caller should add the returned value to its buffer-length accumulator
 * before allocating the frame buffer.
 *
 * Returns: number of bytes needed for the QCN Vendor IE, or 0 if not needed
 */
size_t wpas_modify_buflen_for_qcn_ie_extn(struct wpa_supplicant *wpa_s);

/**
 * hostapd_eid_qcn_vendor_ie_extn - Encode the QCN Vendor IE
 * @hapd: per-BSS hostapd context
 * @eid: write cursor pointing to the next free byte in the IE buffer
 * @opmode: IEEE 802.11 operating mode
 *
 * Returns: updated write cursor (pos advanced past the completed IE) on
 *          success, or the original @eid value if no attribute is active or
 *          @eid is NULL.
 */
u8 * hostapd_eid_qcn_vendor_ie_extn(struct hostapd_data *hapd, u8 *eid,
				    enum ieee80211_op_mode opmode);

/**
 * wpas_eid_qcn_vendor_ie_extn - Encode the QCN Vendor IE
 * @wpas:   wpa_supplicant context
 * @eid:    write cursor pointing to the next free byte in the IE buffer
 * @opmode: IEEE 802.11 operating mode
 *
 * Returns: updated write cursor (pos advanced past the completed IE) on
 *          success, or the original @eid value if no attribute is active or
 *          @eid is NULL.
 */
u8 * wpas_eid_qcn_vendor_ie_extn(struct wpa_supplicant *wpa_s, u8 *eid);

#ifdef CONFIG_SME
/**
 * wpas_add_qcn_ie_probe_req_extn - Append QCN Vendor IE to probe-request extra IEs
 * @wpa_s:     wpa_supplicant context
 * @extra_ie:  pointer to the wpabuf holding extra probe-request IEs
 */
void wpas_add_qcn_ie_probe_req_extn(struct wpa_supplicant *wpa_s,
				    struct wpabuf **extra_ie);

/**
 * wpas_add_qcn_ie_assoc_req_extn - Append QCN Vendor IE to SME assoc-request IEs
 * @wpa_s:  wpa_supplicant context
 */
void wpas_add_qcn_ie_assoc_req_extn(struct wpa_supplicant *wpa_s);
#endif /* CONFIG_SME */

/**
 * hostapd_qcn_buflen_add_240mhz_attr - Compute 240 MHz QCN IE attribute byte count
 * @hapd: per-BSS hostapd context
 *
 * Returns: number of bytes needed for the 240 MHz attribute, or 0 if not needed
 */
size_t hostapd_qcn_buflen_add_240mhz_attr(struct hostapd_data *hapd);

/**
 * hostapd_qcn_eid_add_240mhz_attr - Encode the 240 MHz vendor attribute into a QCN IE
 * @hapd: per-BSS hostapd context
 * @pos: write cursor pointing to the next free byte in the IE buffer;
 * @opmode: IEEE 802.11 operating mode
 *
 * Returns: updated write cursor (pos advanced past the written TLV) on
 *          success, or the original @pos value when the attribute is skipped.
 */
u8 * hostapd_qcn_eid_add_240mhz_attr(struct hostapd_data *hapd, u8 *pos,
				     enum ieee80211_op_mode opmode);

/**
 * hostapd_qcn_buflen_add_5ghz_320mhz_csa_attr - Compute CSA 320 MHz
 * QCN attribute byte count
 * @hapd: per-BSS hostapd context
 *
 * Returns: number of bytes needed for the CSA 320 MHz QCN attribute, or 0 if
 * not needed.
 */
size_t hostapd_qcn_buflen_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd);

/**
 * hostapd_qcn_eid_add_5ghz_320mhz_csa_attr - Encode CSA 320 MHz QCN attribute
 * @hapd: per-BSS hostapd context
 * @pos: write cursor inside an already-started QCN vendor IE
 *
 * Appends QCN_ATTRIB_5GHZ_320MHZ_CSA carrying CSA target center segment
 * fields and puncture bitmap from cs_freq_params.
 *
 * Returns: updated write cursor, or the original @pos when skipped.
 */
u8 * hostapd_qcn_eid_add_5ghz_320mhz_csa_attr(struct hostapd_data *hapd,
					       u8 *pos);

int hostapd_dfs_get_start_chan_idx_extn(struct hostapd_iface *iface);
int hostapd_get_n_chans_and_frequency_extn(enum oper_chan_width oper_chwidth,
					   int cf1,
					   int *n_chans,
					   int *frequency);
int hostapd_get_dfs_half_chwidth_extn(enum chan_width width);
int hostapd_dfs_get_allowed_channels_extn(int n_chans,
					  int *is_allowed,
					  unsigned int *allowed_no);
int hostapd_dfs_adjust_center_freq_extn(int oper_chwidth,
					short chan,
					u8 *oper_centr_freq_seg0_idx,
					u8 *oper_centr_freq_seg1_idx);
int
hostapd_get_bw_and_startchan_for_240mhz_extn(enum oper_chan_width
					     eht_oper_chwidth,
					     u8 eht_oper_centr_freq_seg0_idx,
					     u16 *bw, u8 *start_chan);
u8 * hostapd_eid_vendor_240mhz_extn(struct hostapd_data *hapd, u8 *eid,
				    enum ieee80211_op_mode opmode);
u16
hostapd_copy_sta_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info *sta,
				     enum ieee80211_op_mode opmode,
				     struct ieee802_11_elems_extn *elems_extn);
/**
 * hostapd_drv_set_peer_he_mcs_12_13_cap_extn - Store peer HE MCS 12/13 capability
 * @hapd: hostapd BSS data
 * @sta: station info structure
 * @elems_extn: extended elements populated by the common parser
 *              (ieee802_11_parse_vendor_specific_elems_extn path)
 *
 * Reads the peer HE MCS 12/13 NSS bitmaps from @elems_extn and send it to the
 * driver.
 *
 * Returns: 0 on success, -1 on invalid arguments.
 */
void hostapd_drv_set_peer_he_mcs_12_13_cap_extn(struct hostapd_data *hapd,
						struct ieee802_11_elems_extn
						*elems_extn);
/**
 * hostapd_drv_set_vht_mcs_10_11_supp_extn - Stores VHT mcs 10/11 flags
 * @hapd: hostapd BSS data
 * @sta: station info structure
 * @elems_extn: extended elements populated by the common parser
 *
 * Reads VHT MCS 10/11 support flags from BSS and store in STA information.
 *
 * Returns: none
 */
void hostapd_drv_set_vht_mcs_10_11_supp_extn(struct hostapd_data *hapd,
					     struct sta_info *sta,
					     struct ieee802_11_elems_extn *elems_extn);

/**
 * hostapd_drv_set_he_400ns_sig_2xltf_160_supp_extn - Stores HE 400ns SIG and 2x LTF
 * for 160 MHz and 80p80 MHz flags
 * @hapd: hostapd BSS data
 * @sta: station info structure
 * @elems_extn: extended elements populated by the common parser
 *
 * Reads HE 400ns SIG and 2x LTF for 160 MHz and 80p80 MHz flags from BSS and stores
 * in STA information.
 *
 * Returns: none
 */
void hostapd_drv_set_he_400ns_sig_2xltf_160_supp_extn(struct hostapd_data *hapd,
						      struct sta_info *sta,
						      struct ieee802_11_elems_extn
						      *elems_extn);

/**
 * hostapd_set_sta_vht_mcs10_11_and_he_cap_internal_extn - Notify driver of peer
 * VHT MCS10/11 and HE 400ns SGI/2x LTF for 160 MHz
 * @hapd: hostapd BSS data
 * @sta: station info structure
 */
void hostapd_set_sta_vht_mcs10_11_and_he_cap_internal_extn(struct hostapd_data *hapd,
							   struct sta_info *sta);

/**
 * wpas_drv_set_peer_he_mcs_12_13_cap_extn - Send AP HE MCS 12/13 cap from
 * assoc-response IEs to the driver.
 * @wpa_s:  wpa_supplicant context
 * @freq: Associated frequency in MHz
 * @ies: Information elements from the frames
 * @ies_len: Length of the IE.
 *
 * Reads the peer HE MCS 12/13 NSS bitmaps from @elems_extn and send it to the
 * driver.
 */
void wpas_drv_set_peer_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq,
					     const u8 *ies, size_t ies_len);

/**
 * wpas_drv_set_peer_vht_mcs10_11_and_he_cap_internal_extn - STA mode:
 * parse AP QCN IE from assoc response and notify driver of negotiated
 * VHT MCS 10/11 and HE 400ns SGI / 2xLTF 160/80+80 peer capabilities.
 */
void wpas_drv_set_peer_vht_mcs10_11_and_he_cap_internal_extn(
		struct wpa_supplicant *wpa_s,
		const u8 *ies, size_t ies_len);

void wpa_bss_check_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    struct wpa_bss *bss);
bool wpas_sta_cac_5g_320mhz_update_freq_params_extn(enum chan_width width,
					     u8 cf2_idx,
					     struct hostapd_freq_params *params);
void wpas_ch_switch_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    union wpa_event_data *data);

void hostapd_get_eht_240mhz_cap_extn(struct hostapd_data *hapd,
				     struct sta_info_extn *sta_extn,
				     struct ieee80211_240mhz_vendor_oper_extn
				     *dest);
void hostapd_sta_os_free_extn(struct sta_info_extn *sta_extn);
int
ieee802_11_parse_vendor_specific_eht_240mhz_cap_extn(struct ieee802_11_elems
						     *elems,
						     unsigned int oui_flag,
						     const u8 *pos,
						     size_t elen);
int ieee802_11_parse_vendor_specific_elems_extn(struct ieee802_11_elems *elems,
						unsigned int oui_flag,
						const u8 *pos, size_t elen);
void hostapd_copy_sta_add_params_extn(struct hostapd_sta_add_params_extn
				      *params_extn,
				      struct sta_info_extn *sta_extn);
void wpa_driver_nl80211_sta_add_extn(void *priv,
				     struct hostapd_sta_add_params *params);
int wpa_driver_nl80211_dcs_config_extn(void *priv, u8 link_id,
				       struct driver_dcs_config *params);
int nl80211_notify_radar_detected_extn(void *priv,
					      struct hostapd_freq_params *freq);
int wpa_driver_nl80211_dcs_sim_extn(void *priv, u8 link_id,
				    struct driver_dcs_sim *params);
int nl80211_disable_opclass_chans_extn(void *priv, u8 link_id,
				       u8 is_disable,
				       u8 opclass,
				       const u8 *chan_list,
				       size_t chan_count);
int netlink_increase_rcvbuf_extn(int sock);
int wpa_driver_nl80211_cbs_trigger_scan(void *priv,
					const struct cbs_params_extn *params,
					int *freq_list, int link_id);
void *wpa_driver_nl80211_get_survey_extn(struct i802_bss *bss, void *ctx);
int hostapd_drv_fetch_and_set_vendor_bssid_extn(struct hostapd_data *hapd);
void hostapd_free_bss_index_extn(struct hostapd_data *hapd);
bool hostapd_dfs_get_valid_punc_bitmap_extn(int chan_freq,
					    u16 punct_bitmap,
					    int center_freq,
					    int half_width);
int hostapd_find_dfs_range_extn(struct hostapd_iface *iface,
				enum chan_width bandwidth,
				struct hostapd_freq_params *freq_params);
bool hostapd_dfs_skip_wradar_chan_extn(struct hostapd_iface *iface,
				       struct hostapd_hw_modes *mode,
				       struct hostapd_channel_data *chan,
				       int first_chan_idx, int n_chans);
int hostapd_is_dfs_overlap_extn(struct hostapd_iface *iface,
				enum chan_width width,
				int center_freq, u16 punct_bitmap);
void hostapd_modify_supported_op_class_for_320mhz_extn(int freq, u8 *op_class);
void hostapd_ignorecac_init_iface_extn(struct hostapd_iface *iface);
bool hostapd_ignorecac_should_skip_cac_extn(struct hostapd_iface *iface);
void
hostapd_ignorecac_update_freq_params_extn(struct hostapd_iface *iface,
					  struct hostapd_freq_params *freq_params);
void hostapd_beacon_set_skip_cac_extn(struct hostapd_iface *iface,
				      struct hostapd_freq_params *freq_params);
bool hostapd_ignorecac_handle_dfs_extn(struct hostapd_iface *iface,
				       int start_idx, int n_chans);
void hostapd_ignorecac_switch_channel_extn(struct hostapd_data *hapd,
					   struct csa_settings *settings);
bool hostapd_ignorecac_chan_switch_complete_extn(struct hostapd_data *hapd,
						 u8 power_mode_6ghz,
						 int width, int width_device,
						 int is_dfs);

/* static declaration of this function is present in hostapd */
u16 get_lower_bandwidth_puncture_pattern(u16 prifreq, u16 cur_pat,
					 u16 cur_cenfreq, u16 cur_bw,
					 u16 target_bw);

/**
 * hostapd_handle_5ghz_320mhz_bw_indication_extn - Update BW Indication IE
 * parameters for non-standard 5 GHz 320 MHz operation
 * @hapd: hostapd data structure
 * @chan1: Pointer to CCFS0 (input: 320 MHz CCFS0, output: effective CCFS0)
 * @chan2: Pointer to CCFS1 (input: 320 MHz CCFS1, output: effective CCFS1)
 * @punct_bitmap: Pointer to puncture bitmap (input: 320 MHz, output: effective)
 * @bandwidth: Pointer to bandwidth (output: effective bandwidth in MHz)
 *
 * Caller is expected to invoke this only for non-standard 5 GHz 320 MHz
 * operation with puncturing. The function derives the effective standard
 * bandwidth (160/80/40/20 MHz) and the corresponding puncture bitmap.
 *
 * Returns: 0 on success, -1 on error
 */
int hostapd_handle_5ghz_320mhz_bw_indication_extn(struct hostapd_data *hapd,
						  u8 *chan1, u8 *chan2,
						  u16 *punct_bitmap,
						  int *bandwidth);
bool hostapd_skip_rnr_6ghz_colocated_extn(struct hostapd_data *hapd, u32 type);
bool hostapd_rnr_6ghz_override_extn(struct hostapd_data *hapd);
bool hostapd_rnr_colocated_ess_indication_extn(struct hostapd_data *hapd);
int
hostapd_ctrl_iface_receive_process_extn(struct hostapd_data *hapd,
					char *buf, char *reply,
					int reply_size,
					struct sockaddr_storage *from,
					socklen_t fromlen, int *reply_len);
void
hostapd_config_defaults_extn(struct hostapd_config *conf);
void
hostapd_config_defaults_bss_extn(struct hostapd_bss_config *bss);
int
hostapd_config_fill_extn(struct hostapd_config *conf,
			 struct hostapd_bss_config *bss,
			 const char *buf, char *pos, int line);

int nl80211_vendor_event_qca_extn(struct i802_bss *bss,
				  u32 subcmd, u8 *data, size_t len);
int hostapd_ctrl_iface_set_extn(struct hostapd_data *hapd, char *cmd, char *value);
int hostapd_wpa_event_extn(void *ctx, enum wpa_event_type event,
			   union wpa_event_data *data);
int wpa_supplicant_event_extn(struct wpa_supplicant *wpa_s,
			      enum wpa_event_type event,
			      union wpa_event_data *data);
void hostapd_sync_country_from_driver(struct hostapd_data *hapd);
int hostapd_ctrl_iface_status_extn(struct hostapd_data *hapd, char *buf,
				   size_t buflen, size_t curr_len);
int hostapd_set_he_mcs_12_13_peer_cap_extn(struct hostapd_data *hapd);
int wpas_set_he_mcs_12_13_peer_cap_extn(struct wpa_supplicant *wpa_s, int freq);

/**
 * nl80211_get_he_mcs_12_13_extn - Fetch self-cap HE MCS 12/13 NSS bitmap
 * @priv:      driver private data (struct i802_bss *)
 * @radio_cap: output - radio NSS bitmap for HE MCS 12/13 support
 *
 * Sends QCA GET_WIFI_CONFIGURATION with GENERIC_COMMAND=
 * QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP to get the hardware
 * capability for HE MCS 12 13 support.
 *
 * Returns: 0 on success, negative on failure.
 */
int nl80211_get_he_mcs_12_13_extn(void *priv, u8 radio_idx, u16 *radio_cap);

/**
 * nl80211_set_he_mcs_12_13_peer_cap_extn - Set peer HE MCS 12/13 NSS bitmap
 * @priv: driver private data (struct i802_bss *)
 * @radio_idx: radio index to target the specific radio
 * @peer_cap: peer NSS bitmap for HE MCS 12/13 support
 *
 * Sends HE MCS 12/13 peer capability using SET_WIFI_CONFIGURATION with
 * GENERIC_COMMAND=QCA_NL80211_VENDOR_SUBCMD_HE_MCS_12_13_SUPP.
 *
 * Returns: 0 on success, negative on failure.
 */
int nl80211_set_he_mcs_12_13_peer_cap_extn(void *priv, u8 radio_idx, u16 peer_cap);
/**
 * nl80211_set_vht_mcs10_11_and_he_cap_internal_extn - Send negotiated
 * VHT MCS 10/11 peer support and HE cap internal bitmap to the driver
 * via QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION (attrs 35/154/155).
 *
 * @priv:            driver private handle
 * @peer_addr:       peer MAC address
 * @vht_mcs10_11:    1 if VHT MCS10/11 supported, 0 otherwise
 * @he_cap_internal: HE capability bitmap (0 = omit attribute)
 *
 * Returns: 0 on success, negative errno on failure.
 */
int nl80211_set_vht_mcs10_11_and_he_cap_internal_extn(void *priv,
						      const u8 *peer_addr,
						      u8 vht_mcs10_11,
						      u32 he_cap_internal);
int nl80211_set_allow_scan_on_dfs_chan_extn(void *priv, bool enable);
int wpas_ctrl_iface_set_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			     const char *value, bool *is_extn_cmd);
int wpas_ctrl_iface_get_extn(struct wpa_supplicant *wpa_s, const char *cmd,
			     char *buf, size_t buflen, bool *is_extn_cmd);
int wpa_ctrl_get_freq_list_extn(struct wpa_supplicant *wpa_s,
				char *reply, int reply_size);
int wpa_ctrl_chan_sw_finished_notify_extn(struct wpa_supplicant *wpa_s,
					  const char *buf, char *reply,
					  int reply_size);
int wpa_config_process_cswopts_extn(struct wpa_config *config, int line,
				    const char *pos);
int wpa_supplicant_ctrl_iface_set_cswopts_extn(struct wpa_supplicant *wpa_s,
					       const char *value);
int compute_sec_channel_offset_extn(int primary_freq, int center_freq1,
				    enum chan_width width);
void wpa_get_bss_channel_oper_info_extn(struct wpa_supplicant *wpa_s,
					struct wpa_bss *bss);
bool compute_dfs_for_chanwidth_extn(int freq, int chanwidth);
void wpa_supp_pre_connect_state_handle_extn(struct wpa_supplicant *wpa_s,
					    struct wpa_bss *bss);
void sme_pre_connect_timer_extn(void *eloop_ctx, void *timeout_ctx);
void wpa_bss_update_link_rnr_ap_info_extn(struct wpa_supplicant *wpa_s,
					  struct wpa_bss *bss,
					  const u8 *bssid_ptr,
					  const struct ieee80211_neighbor_ap_info *ap_info,
					  const u8 *mld_params, u8 link_id);
void wpa_bss_update_mld_link_mcst_extn(struct wpa_bss *bss, u8 link_id,
				       const u8 *ies, size_t ies_len);
u32 wpa_bss_get_mld_link_mcst_extn(struct wpa_bss *bss, u8 link_id);
void wpa_bss_parse_basic_mle_per_sta_mcst_extn(struct wpa_bss *bss,
					       struct wpabuf *mlbuf,
					       size_t common_info_len);
void hostapd_csa_bitmap_update_extn(struct hostapd_iface *iface, int freq);
int hostapd_send_uplink_csa_extn(struct hostapd_iface *iface,
				 int channel, int freq,
				 int secondary_channel,
				 u8 current_vht_oper_chwidth,
				 u8 oper_centr_freq_seg0_idx,
				 u8 oper_centr_freq_seg1_idx,
				 u16 punct_bitmap);
int hostapd_send_rcsa_extn(struct hostapd_iface *iface,
			   int channel, int freq,
			   int secondary_channel,
			   u8 current_vht_oper_chwidth,
			   u8 oper_centr_freq_seg0_idx,
			   u8 oper_centr_freq_seg1_idx,
			   u16 punct_bitmap);
bool hostapd_rcsa_rx_hdl(struct hostapd_data *hapd,
			 const u8 *buf, size_t len);
void hostapd_uplink_cancel_disconnect_timeout_extn(struct hostapd_iface *iface);
void hostapd_ucode_trigger_bhsta_disconnect_extn(struct hostapd_iface *iface);
struct ubus_context *ubus_ap_fetch_context_extn(void);
struct blob_buf *ubus_ap_fetch_bbuf_extn(void);
struct uc_value *ucode_ap_fetch_iface_reg_extn(void);
struct uc_vm *ucode_ap_fetch_vm_extn(void);
int hostapd_prepare_nol_ie_bmap_extn(struct hostapd_iface *iface,
				     int channel, int freq,
				     int secondary_channel,
				     int current_vht_oper_chwidth,
				     int oper_centr_freq_seg0_idx,
				     int oper_centr_freq_seg1_idx,
				     u16 punct_bitmap,
				     u16 radar_bitmap_oper);
int hostapd_dfs_request_channel_switch(struct hostapd_iface *iface,
				       int channel, int freq,
				       int secondary_channel,
				       u8 current_vht_oper_chwidth,
				       u8 oper_centr_freq_seg0_idx,
				       u8 oper_centr_freq_seg1_idx,
				       u16 punct_bitmap);
int hostapd_dfs_abort_cac_and_request_channel_switch(struct hostapd_iface *iface,
						     int channel, int freq,
						     int secondary_channel,
						     u8 current_vht_oper_chwidth,
						     u8 oper_centr_freq_seg0_idx,
						     u8 oper_centr_freq_seg1_idx,
						     u16 punct_bitmap);
int set_dfs_state(struct hostapd_iface *iface, int freq, int ht_enabled,
		  int chan_offset, int chan_width, int cf1,
		  int cf2, u32 state, u16 radar_bitmap);

struct uc_value *uc_wpas_notify_uplink_csa_extn(struct uc_vm *vm, size_t nargs);
struct uc_value *uc_wpas_iface_reconnect_extn(struct uc_vm *vm, size_t nargs);
struct uc_value *uc_wpas_notify_rcsa_extn(struct uc_vm *vm, size_t nargs);
u32 wpas_ucode_get_link_mcst_extn(struct wpa_supplicant *wpa_s, int link_id);
int handle_action_extn(struct hostapd_data *hapd,
		       const struct ieee80211_mgmt *mgmt, size_t len,
		       unsigned int freq);
int handle_action_vs_extn(struct hostapd_data *hapd,
			  struct sta_info *sta,
			  const struct ieee80211_mgmt *mgmt,
			  size_t len, unsigned int freq, bool protected);
u8 *add_ml_link_info_ie(u8 *buf, size_t buf_len,
			u16 link_id_bitmap);
bool hostapd_is_ml_info_ie(const u8 *ie, size_t rem_len);
int uc_hostapd_iface_switch_channel_extn(struct hostapd_iface *iface,
					 bool is_dfs, char *wpa_state,
					 struct csa_settings *csa);
void hostapd_iface_set_supplicant_channel_extn(struct hostapd_iface *hapd_iface);
int acs_get_bw_center_chan(int freq, enum bw_type bw);
struct hostapd_channel_data *
acs_find_ideal_chan(struct hostapd_iface *iface);
int acs_study_options(struct hostapd_iface *iface);
void hostapd_update_nf(struct hostapd_iface *iface,
		       struct hostapd_channel_data *chan,
		       struct freq_survey *survey);
void hostapd_ml_acs_check_and_notify(struct hostapd_iface *iface, bool status);
void wpa_supplicant_start_sta_scan(void *eloop_ctx, void *timeout_ctx);
bool hostapd_is_bh_sta_connecting_or_connected_extn(struct hostapd_iface *iface);
bool hostapd_is_bh_sta_link_connected_extn(struct hostapd_iface *iface);
void hostapd_update_bh_sta_connected_extn(struct hostapd_iface *iface,
					  const char *wpa_state);
bool check_40mhz_2g4_bss_snr_below_threshold_extn(
	const struct wpa_scan_res *bss,
	const struct check_40mhz_2g4_extn_args *extn_args);
bool hostapd_2040_coex_action_snr_below_threshold_extn(
	struct hostapd_data *hapd, int rssi);
int hostapd_rssi_to_snr_extn(struct hostapd_data *hapd, int ssi_signal);
bool hostapd_ht40_intolerant_snr_below_threshold_extn(
	struct hostapd_data *hapd, struct sta_info *sta);
int wpa_ctrl_set_wds_ie_extn(struct wpa_supplicant *wpa_s, int val);
int wpa_ctrl_get_wds_ie_extn(struct wpa_supplicant *wpa_s,
			     char *reply, int reply_size);
int nl80211_set_allow_3addr_mc_extn(void *priv, u8 val);
int wpa_ctrl_set_allow_3addr_mc_extn(struct wpa_supplicant *wpa_s, int val);
#ifdef HOSTAPD
struct hostapd_data *
switch_link_hapd(struct hostapd_data *hapd, int link_id);
struct hostapd_data *
get_link_hapd(struct hostapd_data *hapd, const u8 *ies, size_t len,
	      int *link_id);
#else
static inline struct hostapd_data *
switch_link_hapd(struct hostapd_data *hapd, int link_id)
{
    return hapd;
}

static inline struct hostapd_data *
get_link_hapd(struct hostapd_data *hapd, const u8 *ies, size_t len,
	      int *link_id)
{
	if (link_id)
		*link_id = -1;

	return NULL;
}
#endif
int qca_nl80211_handle_wifi_config_evt_extn(struct i802_bss *bss,
					    u8 *data, size_t len);
size_t hostapd_esp_ie_len_extn(struct hostapd_data *hapd);
u8 * hostapd_eid_esp_extn(struct hostapd_data *hapd, u8 *eid, size_t len);
size_t hostapd_esp_ie_len_extn(struct hostapd_data *hapd);
int hostapd_ctrl_iface_get_extn(struct hostapd_data *hapd, char *cmd,
				char *buf, size_t buflen);
#ifdef CONFIG_ACS
int hostapd_handle_cli_acs_extn(struct hostapd_data *hapd, char *pos,
				char *buf, size_t buflen);
#endif /* CONFIG_ACS */

#ifndef CONFIG_QCN_APP_EXTN
static inline struct hostapd_channel_data *
qacs_find_ideal_chan(struct hostapd_iface *iface)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
	return NULL;
}

static inline void
qacs_adjust_sec_chan_and_eht320_offset(struct hostapd_iface *iface,
				       struct hostapd_channel_data *ideal_chan)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
}

static inline int
acs_process_hostapd_scan_data(struct hostapd_iface *iface)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
	return -EOPNOTSUPP;
}

static inline void
qacs_reset_scan_stats(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
}

static inline int
acs_process_hostapd_scan_data_per_freq(struct hostapd_iface *iface,
					   int freq_filter)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
	return -EOPNOTSUPP;
}

static inline void
acs_update_total_interference_factor_extn(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
		      int n_chans, struct hostapd_channel_data *chan,
		      long double factor)
{
}

static inline void acs_reset_chan_ranks(struct hostapd_hw_modes *mode)
{
}

static inline void acs_rank_channels(struct hostapd_hw_modes *mode)
{
}

static inline
int hostapd_get_dcs_channel_penalty_extn(struct hostapd_iface *iface,
					 struct hostapd_channel_data *chan)
{
	wpa_printf(MSG_ERROR, "QACS is not supported");
	return -EOPNOTSUPP;
}
#ifdef CONFIG_IEEE80211BN
static inline int
qacs_select_best_npca_chan(struct hostapd_iface *iface,
		struct hostapd_hw_modes *mode,
		struct hostapd_channel_data *ideal_chan,
		enum oper_chan_width chwidth,
		u8 *npca_primary_channel,
		u16 *npca_punct_bitmap)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_IEEE80211BN */

#else
void acs_update_total_interference_factor_extn(struct hostapd_iface *iface,
                      struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
                      int n_chans, struct hostapd_channel_data *chan,
                      long double factor);
void acs_reset_chan_ranks(struct hostapd_hw_modes *mode);
void acs_rank_channels(struct hostapd_hw_modes *mode);
struct hostapd_channel_data *
qacs_find_ideal_chan(struct hostapd_iface *iface);
void qacs_adjust_sec_chan_and_eht320_offset(struct hostapd_iface *iface,
					    struct hostapd_channel_data *ideal_chan);
int acs_process_hostapd_scan_data(struct hostapd_iface *iface);
int acs_process_hostapd_scan_data_per_freq(struct hostapd_iface *iface,
					       int freq_filter);
void qacs_reset_scan_stats(struct hostapd_iface *iface,
			   struct hostapd_hw_modes *mode);
int hostapd_get_dcs_channel_penalty_extn(struct hostapd_iface *iface,
					 struct hostapd_channel_data *chan);

#ifdef CONFIG_IEEE80211BN
int qacs_select_best_npca_chan(struct hostapd_iface *iface,
		struct hostapd_hw_modes *mode,
		struct hostapd_channel_data *ideal_chan,
		enum oper_chan_width chwidth,
		u8 *npca_primary_channel,
		u16 *npca_punct_bitmap);
#endif /* CONFIG_IEEE80211BN */

#endif /*CONFIG_QCN_APP_EXTN */

int hostapd_set_nontx_optional_vendor_elem_size_extn(struct hostapd_data *hapd,
						     struct hostapd_bss_config *conf,
						     char *value);

int hostapd_set_cswopts_extn(struct hostapd_config_extn *conf_extn,
			     const char *value);

#ifdef CONFIG_ACS
void acs_request_scan_add_freqs_extn(struct hostapd_channel_data *chan,
				     int **freq);
void acs_modify_scan_params_extn(struct hostapd_iface *iface,
				 struct wpa_driver_scan_params *params);
#else
static inline void
acs_request_scan_add_freqs_extn(struct hostapd_channel_data *chan,
				int **freq) {}

static inline void
acs_modify_scan_params_extn(struct hostapd_iface *iface,
			    struct wpa_driver_scan_params *params) {}
#endif /* CONFIG_ACS */
#ifdef CONFIG_IEEE80211AC
void hostapd_mu_cap_war_state_init_extn(struct hostapd_data *hapd);
void hostapd_mu_cap_war_update_db_extn(struct hostapd_data *hapd,
				       const u8 *addr, u8 *vht_cap_offset);
void hostapd_mu_cap_war_mu_state_changed_extn(struct hostapd_data *hapd);
void hostapd_mu_cap_war_sta_list_flush_extn(struct hostapd_data *hapd);
void hostapd_mu_cap_war_kickout_timer_extn(void *eloop_ctx, void *timeout_ctx);
void hostapd_mu_cap_war_client_cap_extn(struct hostapd_data *hapd,
					struct sta_info *sta);
void hostapd_mu_cap_war_expire_queries(struct hostapd_data *hapd);
#endif /* CONFIG_IEEE80211AC */

int hostapd_ctrl_iface_dcs_extn(struct hostapd_data *hapd, const char *cmd, char *reply,
				int reply_size);
#ifdef CONFIG_ACS
int
acs_handle_channel_change_extn(struct hostapd_iface *iface,
			       struct hostapd_channel_data *chan,
			       int err);
void acs_init_extn(struct hostapd_iface *iface, uint8_t trigger);
int
acs_handle_channel_change_failed_extn(struct hostapd_iface *iface,
				      struct hostapd_channel_data *ideal_chan,
				      int err);
bool acs_hwbl_candidate_ok(struct hostapd_iface *iface,
			   struct hostapd_channel_data *chan,
			   u32 bw, int bw320_offset, u16 punct_bitmap,
			   u8 nl80211_pwr_mode);
bool acs_hwbl_chan_ok_extn(struct hostapd_iface *iface,
			   struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
			   int n_chans, struct hostapd_channel_data *chan,
			   long double factor);
bool hostapd_acs_is_chan_blocked(struct hostapd_iface *iface, u8 chan);
#ifdef CONFIG_IEEE80211BE
void acs_update_puncturing_bitmap(struct hostapd_iface *iface,
				  struct hostapd_hw_modes *mode, u32 bw,
				  int n_chans,
				  struct hostapd_channel_data *chan,
				  long double factor, int index_primary);
#endif /* CONFIG_IEEE80211BE */
bool acs_scan_event_expected_extn(struct hostapd_iface *iface);
bool hostapd_hwbl_validate_6ghz(struct hostapd_iface *iface,
				struct hostapd_channel_data *chan,
				u16 bw, u16 center_freq, u16 punct_bitmap,
				u8 nl80211_pwr_mode);

#else
static inline int
acs_handle_channel_change_extn(struct hostapd_iface *iface,
			       struct hostapd_channel_data *chan,
			       int err)
{
	return -EOPNOTSUPP;
}

static inline void
acs_init_extn(struct hostapd_iface *iface, uint8_t trigger)
{
	return;
}

static inline int
acs_handle_channel_change_failed_extn(struct hostapd_iface *iface,
				      struct hostapd_channel_data *ideal_chan,
				      int err)
{
	return -EOPNOTSUPP;
}

static inline bool
acs_hwbl_candidate_ok(struct hostapd_iface *iface,
		      struct hostapd_channel_data *chan,
		      u32 bw, int bw320_offset, u16 punct_bitmap,
		      u8 nl80211_pwr_mode)
{
	return true;
}

static inline bool
acs_hwbl_chan_ok_extn(struct hostapd_iface *iface,
		      struct hostapd_hw_modes *mode, u32 bw, int bw320_offset,
		      int n_chans, struct hostapd_channel_data *chan,
		      long double factor)
{
	return true;
}

static inline bool
hostapd_hwbl_validate_6ghz(struct hostapd_iface *iface,
			    struct hostapd_channel_data *chan,
			    u16 bw, u16 center_freq, u16 punct_bitmap,
			    u8 nl80211_pwr_mode)
{
	return true;
}

static inline bool
acs_scan_event_expected_extn(struct hostapd_iface *iface)
{
	return false;
}
#endif /* CONFIG_ACS */
bool
acs_usable_bw_chan(const struct hostapd_channel_data *chan, enum bw_type bw);
int qca_nl80211_handle_dcs_config_evt_extn(struct i802_bss *bss,
					   u8 *data, size_t len);
int intf_awgn_find_channel_list(struct hostapd_iface *iface, int chan_width,
				struct hostapd_channel_data ***chandef_list,
				int *awgn_interference_freqs);
void update_chan_params(struct hostapd_data *hapd, int cf1, int cf2,
			enum chan_width chwidth);
bool dcs_get_bw_reduction_ctrl_extn(struct hostapd_config *conf, u16 dcs_intf_type);
bool hostapd_dcs_awgn_handle_rand_chan_disabled_extn(struct hostapd_iface *iface);
void hostapd_dcs_restore_extn(struct hostapd_iface *iface, const char *reason);
void hostapd_dcs_iface_deinit_extn(struct hostapd_iface *iface);
struct hostapd_channel_data *
get_chan_data_by_freq(struct hostapd_hw_modes *mode, int freq);
int is_chan_range_available(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans);
void reduced_chan_width(struct hostapd_iface *iface, int *new_chan_width,
			int chan_width, int freq,
			struct hostapd_hw_modes *mode,
			u32 chan_bw_interference_bitmap);
int hostapd_get_6g_chan_list_extn(struct hostapd_iface *iface,
				  char *buf, size_t buflen);
void hostapd_iface_init_extn(struct hostapd_iface *iface);
void hostapd_iface_deinit_extn(struct hostapd_iface *iface);
void hostapd_free_hw_features_extn(struct hostapd_hw_modes *hw_features,
				   size_t num_hw_features);
void wpas_iface_init_extn(struct wpa_supplicant *wpa_s);
void wpas_iface_deinit_extn(struct wpa_supplicant *wpa_s);
/**
 * hostapd_get_6ghz_thresh_priority_freq_extn() - Helper function to fetch the
 * VLP priority threshold frequency from driver
 * @iface: Pointer to hostapd interface data
 *
 * Return: 0 if threshold frequency was fetched successfully, else error code.
 */
int hostapd_get_6ghz_thresh_priority_freq_extn(struct hostapd_iface *iface);
int hostapd_get_agile_capable_extn(struct hostapd_iface *iface);
bool nl80211_is_6ghz_hw_blocked_chans_supported_extn(void *priv);
int nl80211_fetch_hw_blocked_chans_extn(void *priv, int radio_idx);

void wiphy_info_qca_vendor_command_extn(struct wpa_driver_nl80211_data *drv,
					const struct nl80211_vendor_cmd_info *vinfo);
void hostapd_query_hw_blocklist_extn(struct hostapd_iface *iface,
				     struct hostapd_data *hapd);
void hostapd_free_hw_blocklist_info_extn(
	struct hostapd_hw_blocklist_info *hw_blocklist_info,
	unsigned int num_hw_blocklist);
bool hostapd_is_hw_blocklisted_combo_extn(struct hostapd_iface *iface,
					  u16 freq, u16 center_freq, u16 bw,
					  u16 puncture_pattern, u8 pwr_mode_id);
int hostapd_validate_hw_blocklist_for_freq_params_extn(
	struct hostapd_iface *iface,
	const struct hostapd_freq_params *freq_params,
	u8 pwr_mode_id, const char *op_name);
int hostapd_validate_current_6ghz_hw_blocklist_extn(
	struct hostapd_iface *iface,
	u8 pwr_mode_id, const char *op_name);
void wpas_query_hw_blocklist_extn(struct wpa_supplicant *wpa_s);
#ifdef HOSTAPD
struct hostapd_iface *hostapd_afc_find_iface(struct hostapd_data *hapd, int hw_idx);
#else
static inline struct hostapd_iface *
hostapd_afc_find_iface(struct hostapd_data *hapd, int hw_idx)
{
	return NULL;
}
#endif /* HOSTAPD */
void wpas_country_change_init_active_alpha2_extn(struct wpa_supplicant *wpa_s);
void wpas_country_change_check_current_bss(struct wpa_supplicant *wpa_s,
					   struct channel_list_changed *info);
bool wpas_is_6ghz_hwbl_link_ok_extn(struct wpa_supplicant *wpa_s,
				     const struct wpa_bss *bss);
int intf_chan_range_available_5g(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans);
int intf_chan_range_available_2g(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans);
int get_centre_freq(struct hostapd_channel_data *first_chan,
		    int chan_width, int *centre_freq);
int intf_chan_range_available_6g(struct hostapd_hw_modes *mode,
				 int first_chan_idx, int num_chans);
bool is_chan_disabled(struct hostapd_hw_modes *mode, int chan_num);
int chan_pri_allowed(const struct hostapd_channel_data *chan);
#ifdef CONFIG_ACS
int hostapd_trigger_dynamic_acs(struct hostapd_data *hapd,
				enum dynamic_acs_action_extn acs_action);
void hostapd_periodic_acs_start(struct hostapd_iface *iface);
void hostapd_periodic_acs_stop(struct hostapd_iface *iface);
void hostapd_periodic_acs_schedule(struct hostapd_iface *iface);
int hostapd_cbs_handle_single_channel_survey(struct hostapd_iface *iface,
					     struct hostapd_channel_data *chan,
					     struct freq_survey *survey);
#else
static inline int
hostapd_trigger_dynamic_acs(struct hostapd_data *hapd,
			    enum dynamic_acs_action_extn acs_action)
{
	return -1;
}

static inline void
hostapd_periodic_acs_start(struct hostapd_iface *iface)
{
}

static inline void
hostapd_periodic_acs_stop(struct hostapd_iface *iface)
{
}

static inline void
hostapd_periodic_acs_schedule(struct hostapd_iface *iface)
{
}

static inline int
hostapd_cbs_handle_single_channel_survey(struct hostapd_iface *iface,
					 struct hostapd_channel_data *chan,
					 struct freq_survey *survey)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_ACS */

int hostapd_drv_dcs_config(struct hostapd_data *hapd, u8 link_id,
			   struct driver_dcs_config *params);
void dcs_enable_init(struct hostapd_data *hapd, u16 enable_bitmap);

int hostapd_validate_mbssid_configuration_extn(struct hostapd_data *hapd);

bool hostapd_is_mesh_vap_extn(struct hostapd_bss_config *conf);
struct hostapd_multi_mbssid_group *
hostapd_get_mesh_group_extn(struct hostapd_multi_mbssid *multi_mbssid);
bool hostapd_has_mesh_vap_in_group_extn(struct hostapd_data *hapd,
					struct hostapd_multi_mbssid *multi_mbssid);
void hostapd_mesh_mbssid_reserve_group_extn(struct hostapd_multi_mbssid *multi_mbssid,
					    u8 max_bssid_indicator,
					    u8 *group_index, u64 *prefix_mask);
bool hostapd_mesh_mbssid_reject_duplicate_extn(struct hostapd_data *hapd,
					       struct hostapd_multi_mbssid *multi_mbssid);
int hostapd_mesh_mbssid_grow_or_reuse_group_extn(struct hostapd_data *hapd,
						 struct hostapd_multi_mbssid *multi_mbssid,
						 u8 max_bssid_indicator,
						 u8 *group_index, u64 *prefix_mask);
void hostapd_mesh_mbssid_mark_group_extn(struct hostapd_data *hapd,
					 struct hostapd_multi_mbssid_group *group);

int hostapd_get_channel_idx(struct hostapd_hw_modes *mode, int channel_num);

#ifdef HOSTAPD
/**
 * hostapd_5ghz_eht_320_channel_bw_extn() - Helper function to check whether to
 * report "320MHz" on 5 GHz.
 * @mode: HW mode with regulatory channel list
 * @channel_idx: Index into @mode->channels[] for the queried channel
 *
 * Return true only if:
 *  - @channel advertises HOSTAPD_CHAN_WIDTH_320 in allowed_bw,
 *  - channel number is in {100..144 step 4}, and
 *  - all channels in that set exist in @mode and are not HOSTAPD_CHAN_DISABLED.
 *
 * Return: true if "320MHz" can be shown for this channel; false otherwise.
 */
bool hostapd_5ghz_eht_320_channel_bw_extn(struct hostapd_hw_modes *mode,
					  int channel_idx);
#else
static inline bool
hostapd_5ghz_eht_320_channel_bw_extn(struct hostapd_hw_modes *mode,
				     int channel_idx)
{
	return false;
}
#endif /* HOSTAPD */

/**
 * hostapd_ttlm_restore_default_mapping_for_5g_cac() - Restore default
 * TID-to-link mapping when a DFS CSA triggers CAC on the 5 GHz link.
 * @hapd: Pointer to the hostapd BSS instance
 * @settings: CSA channel parameters
 *
 * When a DFS Channel Switch Announcement (CSA) is triggered and the new channel
 * requires a Channel Availability Check (CAC), the 5 GHz link becomes
 * temporarily unavailable. If any TIDs are exclusively mapped to that 5 GHz
 * link — either through an Advertised TTLM or a Peer-to-Peer (P2P) negotiated
 * TTLM — those TIDs would be rendered unusable for the duration of the CAC.
 * To prevent traffic disruption, this function restores the default TID-to-link
 * mapping by issuing an Advertised TTLM that maps all TIDs back to the full set
 * of available MLD links.
 *
 * For the BSS, if TTLM is enabled (ttlm_enable) and the BSS is part of a
 * multi-link device (MLD) with more than one affiliated link,
 * hostapd_ttlm_handle_5g_only_tid_map_for_cac() is called to evaluate the
 * current TID mapping state and apply the appropriate Advertised TTLM update.
 *
 * Return: None
 */
void
hostapd_ttlm_restore_default_mapping_for_5g_cac(struct hostapd_data *hapd,
						struct csa_settings *settings);

/**
 * hostapd_handle_csa_target_unavailable_extn() - Recover from stale CSA target
 * @hapd: BSS instance handling the channel switch event
 * @freq: Frequency reported by the channel switch completion event
 * @finished: Non-zero when the driver reports channel switch completion
 *
 * Handle the case where a CSA completes for the originally requested target,
 * but that target has already become DFS-unavailable/NOL due to a DFS
 * violation during restart/start. In this situation, hostapd can otherwise
 * treat the stale CH_SWITCH event as a successful move to the requested
 * target and continue normal post-switch processing.
 *
 * This helper verifies that the event corresponds to the in-progress CSA
 * target and that the target chandef now contains unavailable DFS subchannels.
 * When that condition is met, it clears the old CSA state and triggers fresh
 * channel selection through the DFS recovery path instead of allowing normal
 * channel switch completion handling to continue.
 *
 * Return: true if the stale CSA target was handled and normal caller
 * processing should stop; false otherwise.
 */
bool hostapd_handle_csa_target_unavailable_extn(struct hostapd_data *hapd,
						int freq, int finished);
struct hostapd_channel_data *
dfs_downgrade_bandwidth_helper(struct hostapd_iface *iface, int *secondary_channel,
						u8 *oper_centr_freq_seg0_idx,
						u8 *oper_centr_freq_seg1_idx,
						u8 *oper_chwidth,
						int *channel_type);
struct hostapd_channel_data *
dfs_get_valid_channel_helper(struct hostapd_iface *iface,
					 int *secondary_channel,
					 u8 *oper_centr_freq_seg0_idx,
					 u8 *oper_centr_freq_seg1_idx,
					 int type);
int hostapd_dfs_start_channel_switch_cac_helper(struct hostapd_iface *iface);
int hostapd_drv_mark_vap_submode(struct hostapd_data *hapd,
				 enum qca_wlan_vendor_vap_submode_type submode);
int hostapd_drv_mark_vap_submode_extn(void *priv, unsigned int vendor_id,
				      unsigned int subcmd,
				      const char *ifname,
				      u8 vap_submode);

u16 hostapd_get_width_from_oper_chwidth_extn(enum oper_chan_width oper_chwidth,
					     int secondary_channel);

u8 uc_hostapd_bandwidth_to_oper_chwidth_extn(int bandwidth);

enum oper_chan_width hostapd_get_oper_chwidth_from_width_extn(u16 width);

/**
 * hostapd_set_he_mcs_12_13_cap_extn - Fetch and store the HE MCS 12/13
 * hardware radio capability for the given BSS.
 * @hapd: per-BSS hostapd context
 *
 * Queries the driver for the self HE MCS 12/13 NSS bitmap and stores it
 * in iface_extn->he_mcs_12_13_radio_cap.
 *
 * Returns: 0 on success, -1 on failure.
 */
int hostapd_set_he_mcs_12_13_cap_extn(struct hostapd_data *hapd);

/**
 * wpas_set_he_mcs_12_13_cap_extn - Fetch and store the HE MCS 12/13
 * hardware radio capability for the wpa_supplicant instance.
 * @wpa_s: wpa_supplicant context
 *
 * Queries the driver for the self HE MCS 12/13 NSS bitmap and stores it
 * in wpas_extn->he_mcs_12_13_radio_cap.
 *
 * Returns: 0 on success, -1 on failure.
 */
int wpas_set_he_mcs_12_13_cap_extn(struct wpa_supplicant *wpa_s, int freq);

/**
 * wpa_config_alloc_empty_extn - Set the default value for config parameters.
 * @config: wpa_supplicant extensions configuration values
 */
void wpa_config_alloc_empty_extn(struct wpa_config *config);

#define MGMT_MIN_FRAME_SIZE_REQUIRED_MLO_MBSSID 2000
#define EMA_MLO_BSS_MAX_LIMIT 8

#define DEFAULT_HE_MCS_12_13_SUPPORT true
#define DEFAULT_VHT_MCS_10_11_SUPPORT true
#define DEFAULT_VHT_MCS_10_11_NQ2Q_PEER_SUPPORT false
#define DEFAULT_HE_400NS_SGI_SUPPORT true
#define DEFAULT_HE_2XLTF_160_80P80_SUPPORT true

/*
 * hostapd_regdom_bringup_failed_extn - Handle bring-up failure due to
 * regulatory/country constraints.
 *
 * Return: true  — parked to NO_IR, caller should return 0 (not DISABLED).
 *         false — not a regulatory failure; caller should set DISABLED.
 */
bool hostapd_regdom_bringup_failed_extn(struct hostapd_iface *iface);

/*
 * hostapd_set_afc_regd_wait - Mark all interfaces on the same radio as waiting
 * for AFC regulatory domain update.
 *
 * Return: None.
 */
void hostapd_set_afc_regd_wait(struct hostapd_iface *orig_iface);

/*
 * hostapd_handle_regchannel_update_extn - Regulatory dispatch after HW
 * infrastructure has been refreshed by the caller (hostapd_handle_regchannel_update).
 *
 * Handles all logic introduced by QSDK on top of the upstream baseline:
 *   1. Force-disable iface if freq could not be resolved (forced-down recovery).
 *   2. Sync country alpha2 from driver and detect a change.
 *   3a. Country changed  → hostapd_country_change_handle() — returns 1 (handled).
 *   3b. Was forced-down  → hostapd_regdom_restore_iface()  — returns 1 (handled).
 *   3c. Same country, no recovery needed → returns 0 so caller runs NO_IR + AFC.
 *
 * Return: 1 if the event was fully handled (country change or regdom restore),
 *         0 if same country and no recovery (caller proceeds with NO_IR + AFC),
 *        <0 on error.
 */
int hostapd_handle_regchannel_update_extn(struct hostapd_iface *iface, bool afc_regd);

/*
 * regdom_subchans_valid - Check if all 20 MHz sub-channels of the current
 * operating channel+BW are still valid in the new country.
 *
 * For non-6 GHz: hostapd_validate_non_6ghz_chan_bw() checks DISABLED, NO_IR,
 * and DFS_UNAVAILABLE per 20 MHz sub-channel.
 *
 * For 6 GHz: two checks:
 *
 *  1. HOSTAPD_CHAN_DISABLED: the new country does not include this frequency
 *     at all (e.g. India 5925-6425 MHz only, 6675 MHz = DISABLED).  No power
 *     mode can make a DISABLED channel legal.  Reject immediately.
 *
 *  2. Power mode validity at the current configured BW via regdom_6ghz_pwr_mode_ok().
 *     Even if the channel is not hard-DISABLED, it must be usable under at
 *     least one power mode at the current BW.  This is checked once for the
 *     primary sub-channel (k==0) since power mode applies to the whole
 *     bonded channel, not per-20MHz slice.
 *
 * Return: true  — all sub-channels pass; current channel can be kept.
 *         false — channel invalid in new country; waterfall must run.
 */
bool regdom_subchans_valid(struct hostapd_iface *iface);

/*
 * hostapd_country_change_post_cac_abort_extn - Continue the country change
 * flow after receiving DFS_CAC_ABORTED for a CAC that was aborted explicitly
 * by hostapd_country_change_handle() via hostapd_drv_abort_cac().
 *
 * At this point the CAC is stopped but the driver is still alive (interface
 * not yet disabled).  Perform the full disable + enable sequence here so
 * the new session starts cleanly.
 */
void hostapd_country_change_post_cac_abort_extn(struct hostapd_iface *iface);

/* Primary channel list APIs  */
int hostapd_set_primary_chanlist(struct hostapd_data *hapd, const char *chan_str);
int hostapd_get_primary_chanlist(struct hostapd_iface *iface,
				 char *buf, size_t buflen);
void hostapd_update_primary_chanlist_flags(struct hostapd_data *hapd);
int hostapd_is_chan_in_primary_list(struct hostapd_iface *iface, u16 freq);
int hostapd_update_assoc_resp_with_hop_count_extn(struct hostapd_data *hapd);

bool chan_pri_allowed_extn(const struct hostapd_channel_data *chan);

enum chan_width
hostapd_oper_chwidth_to_chanwidth_extn(int oper_chwidth,
				       int sec_channel_offset);
bool wpas_bss_uses_nol_channel_extn(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);
bool wpas_link_uses_nol_channel_extn(struct wpa_supplicant *wpa_s, int freq,
				     enum chan_width width, int cf1, int cf2);
void wpas_dfs_radar_detected_sta_mode(struct wpa_supplicant *wpa_s,
				      struct dfs_event *radar);
void wpas_dfs_nop_finished_sta_mode(struct wpa_supplicant *wpa_s,
				    struct dfs_event *radar);
/**
 * hostapd_override_ht_capabilities_extn - Override HT capabilities with extension config
 * @hapd: Pointer to hostapd BSS context
 * @cap: Pointer to HT capabilities element being constructed for beacon/probes.
 *
 * This function applies extension-specific overrides to the HT capabilities
 * element before it is advertised in beacon/probe frames.
 *
 */
void hostapd_override_ht_capabilities_extn(const struct hostapd_data *hapd,
					   struct ieee80211_ht_capabilities *cap);
/* ------------------------------------------------------------------ */
/* WDS vendor IE APIs (CONFIG_QCN_EXTN set)                            */
/* ------------------------------------------------------------------ */
size_t hostapd_wds_ie_len_extn(struct hostapd_data *hapd);
u8 *hostapd_eid_wds_ie_extn(struct hostapd_data *hapd, u8 *eid, size_t len);
u16 check_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *wds_ie, size_t wds_ie_len);
void handle_assoc_cb_wds_ie_extn(struct hostapd_data *hapd,
				 struct sta_info *sta);
void ap_free_sta_wds_ie_extn(struct hostapd_data *hapd, struct sta_info *sta);
size_t wds_ie_assoc_req_len_extn(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid);
u8 *wds_ie_populate_assoc_req_extn(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid,
				   u8 *pos, size_t avail);
void wds_ie_process_assoc_resp_extn(struct wpa_supplicant *wpa_s,
				    const u8 *ies, size_t ies_len);

/**
 * wpa_rcsa_handle_radar - Handle DFS radar event and send RCSA
 * @wpa_s: wpa_supplicant context for the associated interface
 * @radar: DFS radar event describing the detected radar
 */
void wpa_rcsa_handle_radar(struct wpa_supplicant *wpa_s,
			   const struct dfs_event *radar);

/**
 * hostapd_build_rcsa_optional_ies - Build optional IEs for RCSA notification
 * @nol_ie: pointer to NOL IE buffer, or NULL if not present
 * @nol_ie_len: length of the NOL IE buffer
 * @include_ml_ie: whether to include Multi-Link IE information
 * @link_id_bitmap: bitmap of link IDs to include in ML IE
 * @buf: output buffer for the constructed optional IEs
 * @buf_len: length of the output buffer
 *
 * Returns: length of the optional IEs written to @buf.
 */
size_t hostapd_build_rcsa_optional_ies(const u8 *nol_ie,
				       size_t nol_ie_len,
				       bool include_ml_ie,
				       u16 link_id_bitmap,
				       u8 *buf, size_t buf_len);

int wpas_set_dfs_state(struct wpa_supplicant *wpa_s, int freq,
		       int ht_enabled, int chan_offset, int chan_width,
		       int cf1, int cf2, u32 state, u16 radar_bitmap);

#endif /* CONFIG_QCN_EXTN */
#endif /* CMN_H */
