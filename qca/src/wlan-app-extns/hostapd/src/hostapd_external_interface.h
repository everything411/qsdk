/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _HOSTAPD_EXTERNAL_INTERFACE_H
#define _HOSTAPD_EXTERNAL_INTERFACE_H

/*
 * External interface definitions between hostapd and plugin
 */

#include <stdint.h>
#include <stdbool.h>
#include "utils/common.h"
#include "hostapd_if/hostapd_if_common.h"

/*
 * Optional additional match length macro (disabled)
 *
 * #define MAX_ADDITIONAL_MATCH_LEN 20
 */

#ifndef MAX_MLO_LINKS
#define MAX_MLO_LINKS 8
#endif

#ifndef MAX_INTERFACE_NAME
#define MAX_INTERFACE_NAME 16
#endif

/*
 * Maximum lengths for PTK components passed via get/set APIs
 */
#ifndef MAX_KCK_LEN
#define MAX_KCK_LEN 24
#endif

#ifndef MAX_KEK_LEN
#define MAX_KEK_LEN 64
#endif

#ifndef MAX_TK_LEN
#define MAX_TK_LEN 32
#endif

/*
 * Macros referenced by plugin APIs; redefine locally to avoid extra
 * includes
 */

/*
 * from src/common/wpa_common.h
 */
#ifndef PMK_LEN_MAX
#define PMK_LEN_MAX 64
#endif

/*
 * from src/common/wpa_common.h
 */
#ifndef PMKID_LEN
#define PMKID_LEN 16
#endif

/*
 * match WPA_GTK_MAX_LEN from src/common/wpa_common.h
 */
#ifndef MAX_GTK_LEN
#define MAX_GTK_LEN 32
#endif

/*
 * match WPA_PMK_NAME_LEN from src/common/wpa_common.h
 */
#ifndef PMK_R1_NAME_LEN
#define PMK_R1_NAME_LEN 16
#endif

/*
 * Maximum EAP/RADIUS identity length; matches the check in
 * hostapd/radius.c (identity_len > 512).
 */
#ifndef MAX_IDENTITY_LEN
#define MAX_IDENTITY_LEN 512
#endif

/*
 * Maximum RADIUS Chargeable-User-Identity (CUI) attribute length;
 * bounded by the maximum RADIUS attribute value size (RFC 2865).
 */
#ifndef MAX_RADIUS_CUI_LEN
#define MAX_RADIUS_CUI_LEN 253
#endif

#ifndef WPA_PMK_NAME_LEN
#define WPA_PMK_NAME_LEN 16
#endif

enum hostapd_if_frame_policy {
	HOSTAPD_IF_FRAME_DO_NOTHING,
	HOSTAPD_IF_FRAME_NOTIFY,
	HOSTAPD_IF_FRAME_INVOKE,
	HOSTAPD_IF_FRAME_OFFLOAD
};

enum hostapd_if_frame_reg_type {
	HOSTAPD_IF_FRAME_TYPE_AUTH,
	HOSTAPD_IF_FRAME_TYPE_ASSOC,
	HOSTAPD_IF_FRAME_TYPE_ACTION,
	HOSTAPD_IF_FRAME_TYPE_DEAUTH,
	HOSTAPD_IF_FRAME_TYPE_DISASSOC,
	HOSTAPD_IF_FRAME_TYPE_PROBE,
	HOSTAPD_IF_FRAME_TYPE_REMOTE_AUTH,
	HOSTAPD_IF_FRAME_TYPE_MAX
};


enum hostapd_if_action_frame_type {
	HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO,
	HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO_NEIGHBOUR_REQ,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WNM,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_QUERY,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_RESP,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_REQ,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_RESP,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WMM,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_ADDTS_REQ,
	HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_DELTS,
	HOSTAPD_IF_FRAME_TYPE_ACTION_FT,
	HOSTAPD_IF_FRAME_TYPE_ACTION_FT_REQ,
	HOSTAPD_IF_FRAME_TYPE_ACTION_FT_RESP,
	HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR,
	HOSTAPD_IF_FRAME_TYPE_ACTION_MAX
};

static inline const char *hostapd_if_action_frame_type_string(
	enum hostapd_if_action_frame_type type)
{
	switch (type) {
	case HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO_NEIGHBOUR_REQ:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_RADIO_NEIGHBOUR_REQ";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WNM:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WNM";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_QUERY:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_QUERY";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_RESP:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_BTM_RESP";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_REQ:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_REQ";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_RESP:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WNM_DMS_RESP";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WMM:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WMM";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_ADDTS_REQ:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_ADDTS_REQ";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_DELTS:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_WMM_DELTS";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_FT:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_FT";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_FT_REQ:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_FT_REQ";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_FT_RESP:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_FT_RESP";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_VENDOR";
	case HOSTAPD_IF_FRAME_TYPE_ACTION_MAX:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_MAX";
	default:
		return "HOSTAPD_IF_FRAME_TYPE_ACTION_UNKNOWN";
	}
}

struct hostapd_if_frame_category {
	enum hostapd_if_frame_reg_type type;
	union {
		enum hostapd_if_action_frame_type action_type;
	} u;
};

/*
 * PMK-R1 container
 *
 * All variable-length fields are stored as fixed-size arrays so that
 * the structure can be copied by value and no individual members need
 * to be freed by the receiver.
 */
struct hostapd_if_pmk_r1 {
	uint8_t pmk_r1[PMK_LEN_MAX];
	uint8_t pmk_r1_len;
	uint8_t pmk_r1_name[PMK_R1_NAME_LEN];
	int pairwise;

	int expires_in;
	int session_timeout;

	uint8_t identity[MAX_IDENTITY_LEN];
	uint8_t identity_len;

	uint8_t radius_cui[MAX_RADIUS_CUI_LEN];
	uint16_t radius_cui_len;
};

struct hostapd_if_frame_ctx {
	int rx_link_id;
	int status_code;

	union {
		struct {
			int allow_reuse;
			uint16_t auth_alg;

			/*
			 * transaction only relevant for SAE algorithm
			 */
			uint16_t auth_transaction;
			int rssi;

			/*
			 * status code is outside
			 */
			uint8_t sta_assoc_link_mac[6];
		} auth_req; /* not valid for remote auth (FT-over-DS) */

		struct {
			int allow_reuse;
			uint16_t auth_transaction;
			uint16_t auth_alg;
			uint8_t sta_assoc_link_mac[6];
			struct hostapd_if_pmk_r1 *pmk_r1;

			/*
			 * status code is outside
			 */
			uint8_t *additional_ies;
			uint16_t additional_ies_len;
		} auth_resp;

		struct {
			int is_reassoc;
			int rssi;
			uint32_t valid_link_bitmap;
			uint8_t sta_link_mac[MAX_MLO_LINKS][6];
			uint8_t sta_assoc_link_mac[6];
		} assoc_req;

		struct {
			int is_reassoc;
			int rssi;
			uint8_t sta_assoc_link_mac[6];
			uint8_t *additional_ies;
			uint16_t additional_ies_len;
			struct {
				uint8_t *pmk;
				size_t pmk_len;
				uint8_t *pmkid;
			} pmk;
		} assoc_resp;

		struct {
			bool is_ml_sta;
		} remote_auth_req;

		struct {
			struct hostapd_if_pmk_r1 *pmk_r1;
			bool is_ml_sta;
		} remote_auth_resp;

		struct {
			uint8_t category;
			uint8_t action_code;
			int rssi;
		} action;
	} data;
};

struct dot1x_ctx {
	uint8_t *identity;
	size_t identity_len;
	int8_t *cui;
	size_t cui_len;
	uint64_t multi_session_id;
};
enum hostapd_if_event_type {
	HOSTAPD_IF_EVENT_AUTH_TX_COMPLETE,
	HOSTAPD_IF_EVENT_ASSOC_TX_COMPLETE,
	HOSTAPD_IF_EVENT_AUTHORIZE_COMPLETION,
	/*
	 * deauth / disassoc should be sent in tx-status also
	 */
	HOSTAPD_IF_EVENT_DEAUTH,
	HOSTAPD_IF_EVENT_DISASSOC,
	HOSTAPD_IF_EVENT_SA_QUERY_COMPLETION,
	HOSTAPD_IF_EVENT_GTK_COMPLETION,
	HOSTAPD_IF_EVENT_EAPOL_M2_RECEIVED,
	HOSTAPD_IF_EVENT_ACTION_COMPLETION,
	HOSTAPD_IF_EVENT_INBOUND_CALL_ERROR,
	HOSTAPD_IF_EVENT_DOT1X_COMPLETE,
	HOSTAPD_IF_EVENT_RADIUS_SEND,
	HOSTAPD_IF_EVENT_RADIUS_RECEIVE,
	HOSTAPD_IF_EVENT_RADIUS_COA,
	HOSTAPD_IF_EVENT_MAX
};

struct hostapd_if_event {
	enum hostapd_if_event_type type;

	char ifname[MAX_INTERFACE_NAME];

	/*
	 * sta_mac not relevant for HOSTAPD_IF_EVENT_GTK_COMPLETION
	 */
	uint8_t sta_mac[6];

	union {
		struct {
			int ok;
			uint16_t status;
			uint16_t aid;
		} assoc_resp_completion;

		struct {
			int link_id;
			uint8_t link_mac[6];
			bool is_tx_status;
			int tx_status_ok;
			enum hostapd_if_disconnect_type type;
			uint16_t reason_code;
		} deauth_disassoc;

		struct {
			enum hostapd_if_sa_query_status status;
		} sa_query;

		struct {
			int authorized;
		} authorize_completion;

		struct {
			uint8_t identity[MAX_RADIUS_CUI_LEN];
			size_t identity_len;
			int success;
		} dot1x_completion;
		struct {
			const uint8_t *attrs;
			size_t attrs_len;
			uint8_t hdr_code;
			uint32_t msg_type; //unused for CoA
		} radius_msg;
		struct {
			enum HOSTAPD_IF_INBOUND_ERROR {
				HOSTAPD_IF_AUTH_RESPONSE_ERROR,
				HOSTAPD_IF_ASSOC_RESPONSE_ERROR,
				HOSTAPD_IF_SEND_DISCONNECT_ERROR,
				HOSTAPD_IF_TRIGGER_EAPOL_M3_ERROR,
				HOSTAPD_IF_SET_BEACON_PROBE_VENDOR_IES_ERROR,
				HOSTAPD_IF_SET_PMK_ERROR,
				HOSTAPD_IF_SET_PTK_ERROR,
				HOSTAPD_IF_SET_GTK_ERROR,
				HOSTAPD_IF_START_SA_QUERY_ERROR,
				HOSTAPD_IF_EAPOL_TX_ERROR,
				HOSTAPD_IF_SEND_FRAME_ERROR,
				HOSTAPD_IF_EAPOL_KEY_TX_ERROR,
				HOSTAPD_IF_SET_AUTHORIZED_ERROR,
			} type;
			const char *func;
			int line_num;
		} inbound_call_error;
	} data;
};


/*
 * Maximum sizes for raw capability IE buffers passed via get_sta_info.
 * Sized to accommodate the largest possible on-air encoding of each IE body.
 */
#ifndef HOSTAPD_IF_HT_CAP_MAX_LEN
#define HOSTAPD_IF_HT_CAP_MAX_LEN  26  /* sizeof(ieee80211_ht_capabilities) */
#endif

#ifndef HOSTAPD_IF_VHT_CAP_MAX_LEN
#define HOSTAPD_IF_VHT_CAP_MAX_LEN 12  /* sizeof(ieee80211_vht_capabilities) */
#endif

#ifndef HOSTAPD_IF_HE_CAP_MAX_LEN
#define HOSTAPD_IF_HE_CAP_MAX_LEN  54  /* max HE capabilities IE body */
#endif

#ifndef HOSTAPD_IF_EHT_CAP_MAX_LEN
#define HOSTAPD_IF_EHT_CAP_MAX_LEN 80  /* max EHT capabilities IE body */
#endif

/**
 * enum hostapd_if_band - Operating band identifiers used in
 *                        hostapd_if_sta_info and hostapd_if_mld_link_info
 */
enum hostapd_if_band {
	HOSTAPD_IF_BAND_2GHZ    = 0,
	HOSTAPD_IF_BAND_5GHZ    = 1,
	HOSTAPD_IF_BAND_6GHZ    = 2,
	HOSTAPD_IF_BAND_60GHZ   = 3,
	HOSTAPD_IF_BAND_UNKNOWN = 4,
};

/*
 * Per-station capability flags reported in hostapd_if_sta_info::cap_flags.
 * Derived from sta_info::flags (WLAN_STA_*) and the operating mode.
 */

#define HOSTAPD_IF_STA_CAP_OFDM  BIT(0) /* station supports OFDM rates */
#define HOSTAPD_IF_STA_CAP_11G   BIT(1) /* 802.11g (2.4 GHz OFDM) capable */
#define HOSTAPD_IF_STA_CAP_11N   BIT(2) /* 802.11n (HT) capable */
#define HOSTAPD_IF_STA_CAP_HT    BIT(3) /* HT association (WLAN_STA_HT) */
#define HOSTAPD_IF_STA_CAP_HT40  BIT(4) /* HT40 capable */
#define HOSTAPD_IF_STA_CAP_VHT   BIT(5) /* VHT (802.11ac) capable */
#define HOSTAPD_IF_STA_CAP_HE    BIT(6) /* HE (802.11ax) capable */
#define HOSTAPD_IF_STA_CAP_EHT   BIT(7) /* EHT (802.11be) capable */
#define HOSTAPD_IF_STA_CAP_MLD   BIT(8) /* Multi-Link Device */
#define HOSTAPD_IF_STA_CAP_6GHZ  BIT(9) /* 6 GHz capable */

/**
 * struct hostapd_if_radio_info - Radio/channel/signal information
 *
 * Common structure for radio information used by both MLD and non-MLD stations.
 */
struct hostapd_if_radio_info {
	int freq;                    /* Operating frequency in MHz */
	uint8_t channel;             /* Primary channel number */
	enum hostapd_if_band band;   /* Operating band */
	int8_t rssi;                 /* Current RSSI in dBm */
	int8_t avg_rssi;             /* Average RSSI in dBm */
	uint32_t cap_flags;          /* Capability flags */
};

/**
 * struct hostapd_if_mld_link_info - Per-link information for an MLD STA
 *
 * One entry per affiliated link; valid entries have @valid == true.
 * The array index in hostapd_if_mld_info::links[] represents the link_id.
 * For MLD stations, per-link radio and signal information is stored here.
 */
struct hostapd_if_mld_link_info {
	bool valid;                  /* true if this link entry is populated */
	uint8_t local_addr[ETH_ALEN];       /* AP link MAC address */
	uint8_t peer_addr[ETH_ALEN];        /* STA link MAC address */
	struct hostapd_if_radio_info radio; /* Radio/channel/signal info */
};

/**
 * struct hostapd_if_mld_info - MLD capabilities and per-link information
 *
 * Populated from sta_info::mld_info when the station is an MLD.
 */
struct hostapd_if_mld_info {
	uint8_t mld_addr[ETH_ALEN];         /* MLD MAC address */
	uint16_t eml_capa;           /* EML Capabilities field */
	uint16_t mld_capa;           /* MLD Capabilities and Operations field */
	uint8_t num_links;           /* Number of valid entries in @links[] */
	struct hostapd_if_mld_link_info links[MAX_MLO_LINKS];
};

/**
 * struct hostapd_if_sta_info - Per-client capabilities and radio information
 *
 * Returned by the get_sta_info() API. hostapd populates this
 * from sta_info (capability IEs, flags, MLD info) and from the driver via
 * hostapd_drv_read_sta_data() (RSSI, current rates).
 *
 * The structure is intentionally self-contained (fixed-size arrays, no
 * internal hostapd pointers) so that it can be passed safely across the
 * plugin boundary.
 *
 * For MLD stations, radio/channel/signal information is stored per-link
 * in mld_info.links[]. For non-MLD stations, this information is stored
 * in the non_mld structure.
 */
struct hostapd_if_sta_info {
	/* ---- HT capabilities IE body (IEEE 802.11n) ---- */
	uint8_t ht_caps[HOSTAPD_IF_HT_CAP_MAX_LEN];
	uint8_t ht_caps_len;         /* 0 if HT not supported */

	/* ---- VHT capabilities IE body (IEEE 802.11ac) ---- */
	uint8_t vht_caps[HOSTAPD_IF_VHT_CAP_MAX_LEN];
	uint8_t vht_caps_len;        /* 0 if VHT not supported */

	/* ---- HE capabilities IE body (IEEE 802.11ax) ---- */
	uint8_t he_caps[HOSTAPD_IF_HE_CAP_MAX_LEN];
	uint8_t he_caps_len;         /* 0 if HE not supported */

	/* ---- EHT capabilities IE body (IEEE 802.11be) ---- */
	uint8_t eht_caps[HOSTAPD_IF_EHT_CAP_MAX_LEN];
	uint8_t eht_caps_len;        /* 0 if EHT not supported */

	/* ---- Radio/channel/signal information (union based on MLD status) ---- */
	bool is_mld_sta;             /* true if the station is an MLD */
	union {
		/* For MLD stations: per-link information including radio/signal */
		struct hostapd_if_mld_info mld_info;

		/* For non-MLD stations: single-link radio/channel/signal info */
		struct hostapd_if_radio_info non_mld;
	} u;
};

struct hostapd_external_app_object {
	/*
	 * Northbound
	 */
	void (*init)();

	int (*interface_create)(char *ifname, void *ctx);
	void (*interface_remove)(char *ifname, void *ctx);
	void (*invoke_assoc)(char *ifname, uint8_t *sta_mac,
			     const uint8_t *frame, uint16_t frame_len,
			     struct hostapd_if_frame_ctx *ctx);

	void (*invoke_auth)(char *ifname, uint8_t *sta_mac,
			    const uint8_t *frame, uint16_t frame_len,
			    struct hostapd_if_frame_ctx *ctx);

	void (*notify_assoc)(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
			     const uint8_t *frame, uint16_t frame_len,
			     struct hostapd_if_frame_ctx *ctx);

	void (*notify_auth)(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
			    const uint8_t *frame, uint16_t frame_len,
			    struct hostapd_if_frame_ctx *ctx);

	void (*notify_deauth)(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
			      const void *frame, size_t frame_len,
			      struct hostapd_if_frame_ctx *ctx);

	void (*notify_disassoc)(void *hapd_ctx, char *ifname, uint8_t *sta_mac,
				const void *frame, size_t frame_len,
				struct hostapd_if_frame_ctx *ctx);

	void (*notify_event)(void *hapd_ctx, struct hostapd_if_event *event);

	void (*eapol_rx)(char *ifname, uint8_t link_id, const uint8_t *sa,
			 uint8_t *frame, uint16_t frame_len);

	void (*eapol_key_rx)(char *ifname, uint8_t link_id, const uint8_t *sa,
			     uint8_t *frame, uint16_t frame_len);

	void (*offload_action)(char *ifname, const uint8_t *sta_mac,
			       const uint8_t *frame, uint16_t frame_len,
			       uint8_t link_id,
			       struct hostapd_if_frame_ctx *ctx);

	void (*invoke_remote_auth)(char *ifname, uint8_t *sta_mac,
				   const uint8_t *ies,
				   uint16_t ies_len,
				   struct hostapd_if_frame_ctx *ctx);

	void (*notify_action)(char *ifname, const uint8_t *sta_mac,
			      const uint8_t *frame, uint16_t frame_len,
			      int link_id,
			      struct hostapd_if_frame_ctx *ctx);

	void (*notify_remote_auth)(char *ifname, uint8_t *sta_mac,
				   const uint8_t *ies,
				   uint16_t ies_len,
				   struct hostapd_if_frame_ctx *ctx);

	int (*pull_pmk_r1)(char *ifname, uint8_t *sta_mac,
			   uint8_t pmk_r1_name[WPA_PMK_NAME_LEN],
			   uint8_t pmk_r1[PMK_LEN_MAX], size_t *pmk_r1_len,
			   int *pairwise, int *session_timeout,
			   uint8_t identity[MAX_RADIUS_CUI_LEN],
			   size_t *identity_len,
			   uint8_t radius_cui[MAX_RADIUS_CUI_LEN],
			   size_t *radius_cui_len);

	int (*pull_pmk)(char *ifname, uint8_t *sta_mac,
			uint8_t pmk[PMK_LEN_MAX],
			size_t *pmk_len, uint8_t pmkid[PMKID_LEN],
			int *session_timeout);

	/*
	 * Southbound
	 */

	/*
	 * All southbound calls should be originated from this callback
	 * handler, since this will be called from the eloop
	 */
	void (*register_frame)(void *ifname_ctx,
			       struct hostapd_if_frame_category *cat,
			       enum hostapd_if_frame_policy policy);

	void (*register_event)(void *ifname_ctx,
			       enum hostapd_if_event_type type,
			       bool set);

	/*
	 * ASYNC: Association response
	 */
	int (*assoc_response)(char *ifname, uint8_t *sta_mac,
			      struct hostapd_if_frame_ctx *ctx);

	/*
	 * ASYNC: Authentication response
	 */
	int (*auth_response)(char *ifname, uint8_t *sta_mac,
			     struct hostapd_if_frame_ctx *ctx);

	/*
	 * ASYNC: Send Deauthentication
	 */
	int (*send_deauth)(char *ifname, uint8_t *sta_mac,
			   uint16_t reason_code, int link_id,
			   uint8_t *added_data,
			   uint8_t added_data_len);

	/*
	 * ASYNC: Send Disassociation
	 */
	int (*send_disassoc)(char *ifname, uint8_t *sta_mac,
			     uint16_t reason_code, int link_id,
			     uint8_t *added_data,
			     uint8_t added_data_len);

	/*
	 * ASYNC: Trigger EAPOL M3
	 */
	int (*trigger_eapol_m3)(char *ifname, uint8_t *sta_mac);

	/*
	 * ASYNC: Set beacon/probe vendor IEs
	 */
	int (*set_beacon_probe_vendor_ies)(char *ifname,
					   uint8_t *buf, size_t buf_len,
					   int link_id);

	/*
	 * ASYNC: Set PMK
	 */
	int (*set_pmk)(char *ifname, uint8_t *sta_mac,
		       uint8_t *pmk, size_t pmk_len, uint8_t *pmkid,
		       int session_timeout, struct dot1x_ctx *ctx, bool dot1x_done);

	int (*get_pmk)(char *ifname, uint8_t *sta_mac,
		       uint8_t pmk[PMK_LEN_MAX], size_t *pmk_len,
		       uint8_t pmkid[PMKID_LEN]);

	/*
	 * ASYNC: Set PTK
	 */
	int (*set_ptk)(char *ifname, uint8_t *sta_mac,
		       uint8_t *kck, size_t kck_len,
		       uint8_t *kek, size_t kek_len,
		       uint8_t *tk, size_t tk_len,
		       bool authorized);

	int (*get_ptk)(char *ifname, uint8_t *sta_mac,
		       uint8_t kck[MAX_KCK_LEN], size_t *kck_len,
		       uint8_t kek[MAX_KEK_LEN], size_t *kek_len,
		       uint8_t tk[MAX_TK_LEN], size_t *tk_len);

	/*
	 * ASYNC: Set GTK
	 */
	int (*set_gtk)(char *ifname, int link_id, int gtk_idx,
		       uint8_t *gtk, size_t gtk_len);

	int (*get_gtk)(char *ifname, int link_id, int *gtk_idx,
		       const uint8_t gtk[MAX_GTK_LEN], size_t *gtk_len);

	/*
	 * ASYNC: Start SA Query
	 */
	int (*start_sa_query)(char *ifname, uint8_t *sta_mac, int link_id);

	/*
	 * ASYNC: Set station authorized state
	 */
	void (*set_authorized)(char *ifname, uint8_t *sta_mac, int authorized);

	/*
	 * ASYNC: Send action frame
	 */
	void (*send_action)(char *ifname, uint8_t *sta_mac,
			    uint8_t *frame, int frame_len,
			    struct hostapd_if_frame_ctx *ctx);

	/*
	 * ASYNC: Remote auth response
	 */
	int (*remote_auth_response)(char *ifname, uint8_t *sta_mac,
				    struct hostapd_if_frame_ctx *ctx);

	/*
	 * ASYNC: EAPOL Tx
	 */
	void (*eapol_tx)(char *ifname, uint8_t *sta_mac, int link_id,
                              uint8_t type, uint8_t *data,
                              uint16_t data_len, bool with_header);

	/*
	 * ASYNC: EAPOL-Key Tx
	 */
	void (*eapol_key_tx)(char *ifname,  uint8_t *sta_mac, uint8_t link_id,
			     uint8_t *frame, uint16_t frame_len);

	/*
	 * ASYNC: Send generic frame
	 */
	void (*send_frame)(char *ifname, int link_id, uint8_t *frame,
			   uint16_t frame_len);

	/*
	 * PMK-R1 setter (disabled)
	 *
	 * void (*set_pmk_r1)(char *ifname, uint8_t *sta_mac,
	 *                    struct hostapd_if_pmk_r1 *);
	 */

	/*
	 * Query per-client capabilities and other information.
	 *
	 * hostapd fills *info from sta_info (capability IEs, flags, MLD info)
	 * and from the driver via hostapd_drv_read_sta_data (RSSI, rates).
	 *
	 * @ifname:  interface name of the BSS on which the station is associated
	 * @sta_mac: station MAC address (MLD address for MLD stations)
	 * @info:    caller-allocated structure to be filled by hostapd
	 *
	 * Returns 0 on success, negative errno on failure (e.g. -ENOENT if
	 * the station is not found).
	 */
	int (*get_sta_info)(char *ifname, uint8_t *sta_mac,
			    struct hostapd_if_sta_info *info);
};

/*
 * Call from library constructor. This has to be called before
 * hostapd's main.
 */
void hostapd_plugin_register(struct hostapd_external_app_object *plugin);

#endif /* _HOSTAPD_EXTERNAL_INTERFACE_H */
