/*
 * hostapd / Multi-AP Coordination (MAPC) - IEEE 802.11bn
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HOSTAPD_MAPC_H
#define HOSTAPD_MAPC_H

#ifdef CONFIG_IEEE80211BN

#include "utils/common.h"
#include "utils/list.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/mapc_extn.h"
#endif /* CONFIG_QCN_EXTN */

struct hostapd_data;
struct hostapd_iface;
struct sta_info;

/* MAPC Control Field (IEEE80211BN_D1.4 §9.4.2.357.2, Figure 9-aa13)
 * B0: AP ID Present
 * B1: MAPC Capabilities Present
 * B2: MAPC Parameters Present
 * B3-B7: Reserved
 */
#define MAPC_CTRL_APID_PRESENT          0
#define MAPC_CTRL_CAPABILITIES_PRESENT  1
#define MAPC_CTRL_PARAMETERS_PRESENT    2

/* MAPC Capabilities field (IEEE80211BN_D1.4 §9.4.2.357.3, Figure 9-aa15)
 * B0:     Co-BF Supported
 * B1:     Co-SR Supported
 * B2:     Co-TDMA Supported
 * B3:     Co-RTWT Supported
 * B4:     Co-CR Supported
 * B5-B8:  Max Traffic Flows (4-bit; max flows = field value + 1)
 * B9-B13: Reserved
 * B14:    AP TB PPDU Response Supported
 * B15:    MAPC Security Supported
 */
#define MAPC_CAPABILITY_COBF_SUPPORT            0
#define MAPC_CAPABILITY_COSR_SUPPORT            1
#define MAPC_CAPABILITY_COTDMA_SUPPORT          2
#define MAPC_CAPABILITY_CORTWT_SUPPORT          3
#define MAPC_CAPABILITY_COCR_SUPPORT            4
#define MAPC_CAP_MAX_TRAFFIC_FLOWS_SHIFT        5
#define MAPC_CAP_MAX_TRAFFIC_FLOWS_MASK         0x01E0U  /* bits 5-8 */
#define MAPC_CAPABILITY_AP_TB_PPDU_RESPONSE     14
#define MAPC_CAPABILITY_SECURITY_SUPPORT        15

/* Mask of scheme-support bits in mapc_capability_bitmap (bits 0-4: Co-BF/SR/TDMA/RTWT/CR).
 * Bits 14 (AP_TB_PPDU_RESPONSE) and 15 (Security) are non-scheme HW flags
 * and must not gate frame-processing or TX-suppress logic. */
#define MAPC_SCHEME_CAP_MASK  0x001Fu

/*
 * MAPC HW capability bitmap — extended bits (bits 16+).
 *
 * These occupy positions beyond the 16-bit MAPC Capabilities spec field
 * (IEEE P802.11bn D1.4 §9.4.2.357.3, Figure 9-aa15).  They are NEVER
 * encoded into mapc_capability_bitmap or placed on air.
 * mapc_get_common_info_bitmap() translates each to its scheme-specific byte.
 *
 * The driver mirrors these constants as ATH12K_MAPC_CAP_* in mac.c.
 * Both sides must use the same numeric values.
 */
#define MAPC_HW_CAP_COTDMA_RX_TXOP_RETURN  16  /* → cotdma.mapc_cotdma_info B0
                                                   (MAPC_COTDMA_CAP_RX_TXOP_RETURN_SUPPORT) */

/* MAPC Parameters field (IEEE80211BN_D1.4 §9.4.2.357.3, Figure 9-aa16)
 * B0:     Co-BF Agreement Establishment Enabled
 * B1:     Co-SR Agreement Establishment Enabled
 * B2:     Co-TDMA Agreement Establishment Enabled
 * B3:     Co-RTWT Agreement Establishment Enabled
 * B4:     Co-CR Agreement Establishment Enabled
 * B5-B14: Reserved
 * B15:    Protected Negotiations Required
 */
#define MAPC_PARAMETER_COBF_ENABLED                 0
#define MAPC_PARAMETER_COSR_ENABLED                 1
#define MAPC_PARAMETER_COTDMA_ENABLED               2
#define MAPC_PARAMETER_CORTWT_ENABLED               3
#define MAPC_PARAMETER_COCR_ENABLED                 4
#define MAPC_PARAMETER_PROTECTED_NEGOTIATIONS_REQ   15

/* MAPC Optional Subelement IDs (IEEE80211BN_D1.4 §9.4.2.357.4, Table 9-bb7) */
#define MAPC_SUBELEM_PER_SCHEME_PROFILE     0   /* extensible */
#define MAPC_SUBELEM_SECURITY_PROFILE       1   /* extensible */
#define MAPC_SUBELEM_TRAFFIC_PROFILE        2   /* extensible; added in D1.4 */
/* 3-220: Reserved */
#define MAPC_SUBELEM_VENDOR_SPECIFIC        221 /* vendor defined */
/* 222-253: Reserved */
#define MAPC_SUBELEM_FRAGMENT               254
/* 255: Reserved */

/* Co-TDMA (IEEE80211BN_D1.4 §9.4.2.357.4.2.4) */
#define MAX_MAPC_COTDMA_AGR_PER_NEIGHBOR    1

/* Co-TDMA Capabilities byte (IEEE80211BN_D1.4 §9.4.2.357.4.2.4, Figure 9-aa24)
 * B0: Rx TXOP Return Support
 * B1-B7: Reserved
 */
#define MAPC_COTDMA_CAP_RX_TXOP_RETURN_SUPPORT  0

/* BW Info Header byte (IEEE80211BN_D1.4 §9.4.2.357.4.2.4, Figure 9-aa26)
 * B0-B2: Channel Width
 * B3:    Disabled Subchannel Bitmap Present
 * B4-B7: Reserved
 */
#define MAPC_BW_INFO_CHANNEL_WIDTH_MASK     0x07U   /* B0-B2 */
#define MAPC_BW_INFO_DSB_PRESENT            3       /* bit position */

/* Channel Width field values (IEEE80211BN_D1.4 §9.4.2.357.4.2.4, Table 9-bb10)
 * Values 5-7 are reserved.
 */
#define MAPC_CHANNEL_WIDTH_20MHZ            0
#define MAPC_CHANNEL_WIDTH_40MHZ            1
#define MAPC_CHANNEL_WIDTH_80MHZ            2
#define MAPC_CHANNEL_WIDTH_160MHZ           3
#define MAPC_CHANNEL_WIDTH_320MHZ           4

/* BSS Color Information byte (IEEE80211BN_D1.4 §9.4.2.357.4.2.4, Figure 9-aa27)
 * B0-B5: BSS Color
 * B6-B7: Reserved
 */
#define MAPC_BSS_COLOR_MASK                 0x3FU   /* B0-B5 */

/* Minimum Co-TDMA Per-Scheme Profile subelement length (sub_len field):
 * Scheme_Ctrl(1) + Co-TDMA_Cap(1) + BW_Hdr(1) + CCFS(1) + BSS_Color(1) = 5
 */
#define MAPC_COTDMA_SUB_ELEM_MIN_LEN        5
#define MAPC_COTDMA_DISABLE_SUBCHAN_ELEMENT_LEN 2

#define MAPC_APID_LEN  2
#define MAPC_CAP_LEN   2
#define MAPC_PARAM_LEN 2
/* Reserved bit masks for MAPC Common Info fields (IEEE P802.11bn §9.4.2.357.3) */
#define MAPC_CAP_RESERVED_MASK   0x3E00U  /* Capabilities: bits 9-13 reserved  (Fig 9-aa15) */
#define MAPC_PARAM_RESERVED_MASK 0x7FE0U  /* Parameters:   bits 5-14 reserved  (Fig 9-aa16) */
/* Max Scheme Requests per Per-Scheme Profile (Co-RTWT / Co-CR can have multiple) */
#define MAPC_MAX_SCHEME_REQUESTS 8
/*2(MAPC capabilities) + 2(MAPC Parameter)*/
#define MAPC_COMMON_INFO_MIN_LEN 4

#define MAPC_DISC_FRAME_MAX_LEN    128   /* Discovery Request/Response */
#define MAPC_NEGO_FRAME_MAX_LEN    256   /* Negotiation Request/Response */

/*MAPC Dialog token management*/
#define MAPC_DIALOG_TOKEN_MIN           1
#define MAPC_DIALOG_TOKEN_MAX           255
#define MAPC_DISC_TIMEOUT_SEC           10
#define MAPC_NEGO_TIMEOUT_SEC           10

#define MAPC_MAX_VALID_AP_LIST 12

#define MAPC_MAX_CO_AP_PEER              12
#define MAPC_MAX_CO_AP_DISCOVERED_PEER   12
#define MAPC_MAX_COTDMA_PEER             12
#define MAPC_MAX_CO_AP_ACTIVE_PEER       12

#define MAPC_CFG_KEY_MAX    16
#define MAPC_CFG_VAL_MAX    16
#define MAPC_CFG_PARAMS_MAX 12

struct mapc_cfg_req {
	char key[MAPC_CFG_KEY_MAX];
	char val[MAPC_CFG_VAL_MAX];
};

#define MAPC_DEFAULT_COTDMA_ENABLE                    0    /* Co-TDMA off; enable via conf or CLI */
#define MAPC_DEFAULT_DISCOVERY_REQUEST_INTERVAL_SEC   60   /* periodic discovery interval (s) */
#define MAPC_DEFAULT_DISCOVERY_MODE                   0    /* 0 = auto */
#define MAPC_DEFAULT_NEGOTIATION_MODE                 0    /* 0 = auto */
#define MAPC_DEFAULT_DISCOVERY_REQ_TIMEOUT            5   /* seconds */
#define MAPC_DEFAULT_NEGOTIATION_REQ_TIMEOUT          5   /* seconds */
#define MAPC_DEFAULT_MAX_AP_INACTIVITY                180 /* 3*Discovery Interval*/
#define MAPC_DEFAULT_COTDMA_DISCOVERY_SCHEME_PROFILE  1

enum mapc_dialog_type {
	MAPC_DIALOG_DISCOVERY = 0,
	MAPC_DIALOG_NEGOTIATION = 1,
};

/* MAPC Scheme Type field values (IEEE80211BN_D1.4 §9.4.2.357.4.2, Table 9-bb8)
 * Values 5-15 are reserved.
 */
typedef enum {
	MAPC_SCHEME_CO_BF   = 0,
	MAPC_SCHEME_CO_SR   = 1,
	MAPC_SCHEME_CO_TDMA = 2,
	MAPC_SCHEME_CO_RTWT = 3,
	MAPC_SCHEME_CO_CR   = 4,
	MAPC_SCHEME_MAX     = 5,
} ieee80211_mapc_scheme_type_t;

/* MAPC Operation Type field values (IEEE80211BN_D1.4 §9.4.2.357.4.2, Table 9-bb9)
 * Values 0-2 carried in MAPC Negotiation Request frame.
 * Values 3-5 carried in MAPC Negotiation Response frame.
 * Values 6-7 are reserved.
 */
typedef enum {
	MAPC_OP_AGREEMENT_ESTABLISHMENT = 0,
	MAPC_OP_AGREEMENT_UPDATE        = 1,
	MAPC_OP_AGREEMENT_TEARDOWN      = 2,
	MAPC_OP_REQUEST_ACCEPT          = 3,
	MAPC_OP_REQUEST_REJECT          = 4,
	MAPC_OP_REQUEST_ALTERNATE       = 5,
	MAPC_OP_MAX                     = 6,
} ieee80211_mapc_operation_type_t;

/** MAPC peer lifecycle state — stored in sta_info.mapc_params.peer_state */
enum mapc_peer_state {
	MAPC_PEER_STATE_NONE       = 0, /* sta_info is not a MAPC peer */
	MAPC_PEER_STATE_DISCOVERED = 1, /* seen via Discovery; no kernel entry yet */
	MAPC_PEER_STATE_ACTIVE     = 2, /* negotiated; kernel entry installed */
};

/** Per-peer negotiation transaction state */
enum mapc_negotiation_state {
	MAPC_NEG_IDLE = 0,
	MAPC_NEG_AGR_ESTABLISH_INPROGRESS,
	MAPC_NEG_AGR_UPDATE_INPROGRESS,
	MAPC_NEG_AGR_TEARDOWN_INPROGRESS,
};

/* Per-BSS MAPC state placeholder */
struct mapc_bss_config {
	/*
	 * mapc_hw_capability_bitmap has been moved to hostapd_iface
	 * (hapd->iface->mapc_hw_capability_bitmap) because it is a radio-level
	 * property shared across all BSSs.  mapc_capability_bitmap and
	 * mapc_parameter_bitmap remain per-BSS as they are derived from
	 * (iface->mapc_hw_capability_bitmap & mapc_usr_enabled_bitmap).
	 */
	u16 mapc_capability_bitmap;
	/* Tracks which MAPC schemes the user has enabled on this BSS.
	 * Bit positions follow MAPC_CAPABILITY_* defines (capability IE layout):
	 * B0: MAPC_CAPABILITY_COBF_SUPPORT
	 * B1: MAPC_CAPABILITY_COSR_SUPPORT
	 * B2: MAPC_CAPABILITY_COTDMA_SUPPORT
	 * B3: MAPC_CAPABILITY_CORTWT_SUPPORT
	 * B4: MAPC_CAPABILITY_COCR_SUPPORT
	 * B5-B15: reserved
	 */
	u16 mapc_usr_enabled_bitmap;
	/* B0: Co-BF Agreement Establishment Enabled
	 * B1: Co-SR Agreement Establishment Enabled
	 * B2: Co-TDMA Agreement Establishment Enabled
	 * B3: Co-RTWT Agreement Establishment Enabled
	 * B4: Co-CR Agreement Establishment Enabled
	 * B5-B14: Reserved
	 * B15: Protected Negotiations Required
	 */
	u16 mapc_parameter_bitmap;

	/* mapc_cotdma_enable: 1 = Co-TDMA feature enabled, 0 = disabled */
	int mapc_cotdma_enable;

	/* mapc_disc_req_interval_sec: periodic discovery interval */
	unsigned int mapc_disc_req_interval_sec;

	/* mapc_discovery_mode: 0 = automatic, 1 = manual */
	int discovery_mode;

	/* mapc_negotiation_mode: 0 = automatic, 1 = manual */
	int negotiation_mode;

	int max_mapc_co_ap_peer;

	u8 max_mapc_ctdma_peer;

	/* mapc_max_discovered_ap: max entries in DISCOVERED-CACHE */
	int max_mapc_discovered_ap_peer;

	/* mapc_discovery_req_timeout: discovery request timeout (seconds) */
	unsigned int discovery_req_timeout;

	/* mapc_negotiation_req_timeout: negotiation request timeout (seconds) */
	unsigned int negotiation_req_timeout;

	/* max_mapc_ap_inactivity: AP inactivity timeout (seconds) */
	unsigned int max_mapc_ap_inactivity;

	/* mapc_valid_coap_list: comma-separated list of allowed peer AP MACs */
	u8  valid_coap_list[MAPC_MAX_VALID_AP_LIST][ETH_ALEN];
	int valid_ap_count;

	/* mapc_enable_disc_cotdma_scheme_profile: 1 = include Co-TDMA
	 * scheme profile in discovery frames */
	int enable_disc_cotdma_scheme_profile;

	/* Co-TDMA Profile */
	struct {
		u8 mapc_cotdma_info;
	} cotdma;

	u8 active_primary_channel;
};

struct mapc_ctdma_profile {
	/* Channel / BSS descriptor */
	u8  channel_width;          /* MAPC_CHANNEL_WIDTH_* */
	u8  ccfs;                   /* center channel frequency segment */
	u16 disable_subchannel_bitmap;
	u8  bss_color;
	bool rx_txop_return_support;
};


/** Per-scheme cached profile from Discovery/Negotiation */
struct mapc_cached_scheme_profile {
	bool valid;
	u8 mapc_op_type;
	struct mapc_ctdma_profile cotdma;
};

/*store mapc context/data of remote Co-ordinating AP*/
struct  mapc_parameters {
	u16 apid;
	u16 remote_assigned_apid;
	/* B0: Co-BF support
	 * B1: Co-SR support
	 * B2: Co-TDMA support
	 * B3: Co-RTWT support
	 * B4: Co-CR support
	 * B5-B8: Max Traffic Flows
	 * B9-B13: reserved
	 * B14: AP TB PPDU Response support
	 * B15: Security support
	 */
	u16 mapc_capability_bitmap;
	/* B0: Co-BF Agreement Establishment Enabled
	 * B1: Co-SR Agreement Establishment Enabled
	 * B2: Co-TDMA Agreement Establishment Enabled
	 * B3: Co-RTWT Agreement Establishment Enabled
	 * B4: Co-CR Agreement Establishment Enabled
	 * B5-B14: Reserved
	 * B15: Protected Negotiations Required
	 */
	u16 mapc_parameter_bitmap;
	struct mapc_ctdma_profile ctdma_profile;
	u8 agreement_cnt[MAPC_SCHEME_MAX];
	enum mapc_negotiation_state neg_state;
	u8 negotiation_dialog_token;

	enum mapc_peer_state peer_state;

	struct mapc_cached_scheme_profile cached[MAPC_SCHEME_MAX];

	/*
	 * Bitmask of scheme IDs present in the most recently received
	 * Negotiation Request frame.  BIT(scheme_type) set per seen scheme.
	 * Reset before parsing each new Negotiation Request.
	 */
	u8 schemes_request_bitmask;

	/* Dialog token from last seen Discovery frame (for logging / dedup) */
	u8 discovery_dialog_token_last;

#ifdef CONFIG_QCN_EXTN
	struct mapc_vendor_peer_ctx vendor;
#endif /* CONFIG_QCN_EXTN */
	struct mapc_ctdma_profile last_sent_cotdma;
};

struct mapc_scheme_nego_req {
	bool include;
	ieee80211_mapc_operation_type_t op_type;
};

struct mapc_scheme_nego_resp {
	bool                            include;
	ieee80211_mapc_operation_type_t req_op;   /* 0/1/2 from received request */
	ieee80211_mapc_operation_type_t resp_op;  /* 3/4/5 for this scheme */
	const u8                       *alt_param_set;     /* ALTERNATE only; currently NULL */
	size_t                          alt_param_set_len;
};

/** Per-scheme vtable — add new schemes without touching framework code */
struct mapc_scheme_ops {
	ieee80211_mapc_scheme_type_t scheme_type;
	const char *name;
	/*
	 * Build Per-Scheme Profile subelement body (Scheme Param Set) into buf.
	 * Does NOT write the MAPC Scheme Request Set — that is handled by the
	 * caller (mapc_add_scheme_profile) from the mapc_scheme_request_set.
	 */
	int  (*build_scheme_ap_param_set)(struct wpabuf *buf,
			const struct hostapd_data *hapd);
	/*
	 * Parse received Per-Scheme Profile subelement body.
	 * Writes parsed profile into sta->mapc_params.cached[scheme_type].
	 */
	int  (*parse_scheme_profile)(const u8 *body, size_t len,
			struct sta_info *sta);
	/* Return true if AP should accept a Negotiation Request from this peer */
	bool (*accept_criteria)(const struct hostapd_data *hapd,
			const struct sta_info *sta);
	/*
	 * Populate the active ctdma_profile in params from the parsed
	 * cached[] entry.  Called at promotion time (DISCOVERED → ACTIVE)
	 * and on UPDATE ACCEPT.
	 */
	void (*fill_peer_params)(const struct sta_info *sta,
			struct mapc_parameters *params);

	/*
	 * has_params_changed - Detect whether local scheme parameters have
	 * changed relative to the active agreement with this peer.
	 *
	 * Called only for ACTIVE peers with agreement_cnt[scheme_type] > 0
	 * and neg_state == MAPC_NEG_IDLE.  Returns true if an Agreement Update
	 * should be initiated.
	 *
	 * The implementation compares current live parameters (from hapd->iconf
	 * or other live sources) against the active agreement parameters stored
	 * in sta->mapc_params (e.g., ctdma_profile for Co-TDMA).
	 *
	 * A NULL callback means the scheme does not support parameter-change
	 * detection; mapc_handle_negotiation_update() treats NULL as "no change".
	 */
	bool (*has_params_changed)(const struct hostapd_data *hapd,
			const struct sta_info *sta);
};

struct mapc_discovery_req {
	struct dl_list list;
	u8 dialog_token;
	u8 dst_addr[ETH_ALEN];
	struct hostapd_data *hapd;
};

struct mapc_scheme_request_entry {
	u8        op_type;
	bool      per_scheme_info_present;
	u8        per_scheme_info;
	const u8 *param_set;
	size_t    param_set_len;
};

struct mapc_scheme_request_set {
	bool include;
	u8   count;
	struct mapc_scheme_request_entry requests[MAPC_MAX_SCHEME_REQUESTS];
};

/* mapc_ie_params - Parameters for mapc_build_ie() */
struct mapc_ie_params {
	u8   mapc_ctrl_bitmap;
	u16  apid;
	struct mapc_scheme_request_set schemes[MAPC_SCHEME_MAX];
};

void mapc_iface_init(struct hostapd_iface *iface);
void mapc_iface_deinit(struct hostapd_iface *iface);
int mapc_init(struct hostapd_data *hapd);
void mapc_get_common_info_bitmap(struct hostapd_data *hapd);
void mapc_update_hw_capability_bitmap(struct hostapd_data *hapd,
				      u32 hw_cap_bitmap,
				      u8 max_ctdma_peers);

/* Deinitialize MAPC state; to be called during BSS teardown */
void mapc_deinit(struct hostapd_data *hapd);
/* Optional periodic MAPC Discovery Request scheduler */
int mapc_start_periodic_discovery(struct hostapd_data *hapd,
		unsigned int interval_secs);
void mapc_stop_periodic_discovery(struct hostapd_data *hapd);
bool mapc_is_periodic_disc_running(struct hostapd_data *hapd);
int mapc_set_config(struct hostapd_data *hapd,
		    const struct mapc_cfg_req *reqs, int count,
		    char *reply, size_t reply_size);
/* Tx helpers */
int mapc_send_discovery_request(struct hostapd_data *hapd, const u8 *dst);

int mapc_handle_negotiation_update(struct hostapd_data *hapd,
				   ieee80211_mapc_scheme_type_t scheme_type);

bool mapc_handle_primary_channel_change(struct hostapd_data *hapd);

int mapc_send_negotiation_request(struct hostapd_data *hapd, const u8 *dst,
				  const struct mapc_scheme_nego_req *reqs);

void hostapd_mapc_handle_action(struct hostapd_data *hapd, const u8 *src,
		const u8 *buf, size_t len);
void mapc_handle_neighbor_beacon(struct hostapd_data *hapd, const u8 *src);

int mapc_set_valid_coap_list(struct hostapd_data *hapd,
			     const u8 macs[][ETH_ALEN], int count);
bool mapc_is_valid_coap_peer(const struct mapc_bss_config *mc, const u8 *mac);
void mapc_cancel_sta_timers(struct hostapd_data *hapd, struct sta_info *sta);
struct hostapd_data *mapc_find_link_hapd(struct hostapd_data *hapd,
					 const u8 *peer_addr);

#else /* CONFIG_IEEE80211BN */

struct hostapd_data;
struct sta_info;
struct mapc_scheme_nego_req;

static inline int mapc_init(struct hostapd_data *hapd) { return 0; }
static inline void mapc_deinit(struct hostapd_data *hapd) {}
static inline void mapc_iface_init(struct hostapd_iface *iface) {}
static inline void mapc_iface_deinit(struct hostapd_iface *iface) {}
static inline void hostapd_mapc_handle_action(struct hostapd_data *hapd,
					      const u8 *src,
					      const u8 *buf, size_t len) {}
static inline int mapc_start_periodic_discovery(struct hostapd_data *hapd,
						 unsigned int interval_secs)
{ return 0; }
static inline void mapc_stop_periodic_discovery(struct hostapd_data *hapd) {}
static inline bool mapc_is_periodic_disc_running(struct hostapd_data *hapd)
{ (void)hapd; return false; }
static inline int mapc_send_discovery_request(struct hostapd_data *hapd,
					       const u8 *dst) { return 0; }
static inline int mapc_handle_negotiation_update(struct hostapd_data *hapd,
						  int scheme_id) { return 0; }
static inline bool mapc_handle_primary_channel_change(struct hostapd_data *hapd) { return false; }
static inline void mapc_update_hw_capability_bitmap(struct hostapd_data *hapd,
						     u32 hw_cap_bitmap,
						     u8 max_ctdma_peers) {}

#endif /* CONFIG_IEEE80211BN */

#endif /* HOSTAPD_MAPC_H */
