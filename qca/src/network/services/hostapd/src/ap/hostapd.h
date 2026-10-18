/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HOSTAPD_H
#define HOSTAPD_H

#ifdef CONFIG_SQLITE
#include <sqlite3.h>
#endif /* CONFIG_SQLITE */

#include "common/defs.h"
#include "common/dpp.h"
#include "utils/list.h"
#include "ap_config.h"
#include "drivers/driver.h"
#include "ubus.h"
#include "ucode.h"
#include "ttlm.h"
#include "atf/atf_offload.h"
#include "hostapd_log.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_MBO
#define OCE_STA_CFON_ENABLED(hapd) \
	((hapd->conf->oce & OCE_STA_CFON) && \
	 (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_OCE_STA_CFON))
#define OCE_AP_ENABLED(hapd) \
	(hapd->conf->oce & OCE_AP)
#else
#define OCE_STA_CFON_ENABLED(hapd) 0
#define OCE_AP_ENABLED(hapd)       0
#endif /* CONFIG_MBO */
#define TABLE_NAME "wifi_qos_table"
#define CHAIN_NAME "wifi_qos_chain"

struct wpa_ctrl_dst;
struct radius_server_data;
struct upnp_wps_device_sm;
struct hostapd_data;
struct sta_info;
struct ieee80211_ht_capabilities;
struct full_dynamic_vlan;
enum wps_event;
union wps_event_data;
struct rsn_pmksa_cache_entry;
#ifdef CONFIG_MESH
struct mesh_conf;
#endif /* CONFIG_MESH */

#ifdef CONFIG_CTRL_IFACE_UDP
#define CTRL_IFACE_COOKIE_LEN 8
#endif /* CONFIG_CTRL_IFACE_UDP */

struct hostapd_iface;
struct hostapd_mld;
struct smd_neighbor_update_ctx;

struct hapd_interfaces {
	int (*reload_config)(struct hostapd_iface *iface);
	struct hostapd_config * (*config_read_cb)(const char *config_fname);
	int (*ctrl_iface_init)(struct hostapd_data *hapd);
	void (*ctrl_iface_deinit)(struct hostapd_data *hapd);
	int (*ctrl_iface_recv)(struct hostapd_data *hapd,
			       char *buf, char *reply, int reply_size,
			       struct sockaddr_storage *from,
			       socklen_t fromlen);
	int (*for_each_interface)(struct hapd_interfaces *interfaces,
				  int (*cb)(struct hostapd_iface *iface,
					    void *ctx), void *ctx);
	int (*driver_init)(struct hostapd_iface *iface);

	size_t count;
	int global_ctrl_sock;
	struct dl_list global_ctrl_dst;
	char *global_iface_path;
	char *global_iface_name;
#ifndef CONFIG_NATIVE_WINDOWS
	gid_t ctrl_iface_group;
#endif /* CONFIG_NATIVE_WINDOWS */
	struct hostapd_iface **iface;

	size_t terminate_on_error;
#ifndef CONFIG_NO_VLAN
	struct dynamic_iface *vlan_priv;
#endif /* CONFIG_NO_VLAN */
#ifdef CONFIG_ETH_P_OUI
	struct dl_list eth_p_oui; /* OUI Extended EtherType handlers */
#endif /* CONFIG_ETH_P_OUI */
#ifdef CONFIG_ETH_P_1905
	struct dl_list eth_p_1905; /* IEEE 1905.1 CMDU transport handlers */
#endif /* CONFIG_ETH_P_1905 */
	int eloop_initialized;

#ifdef CONFIG_DPP
	struct dpp_global *dpp;
#ifdef CONFIG_DPP3
	struct os_reltime dpp_pb_time;
	struct os_reltime dpp_pb_announce_time;
	struct dpp_pb_info dpp_pb[DPP_PB_INFO_COUNT];
	struct dpp_bootstrap_info *dpp_pb_bi;
	u8 dpp_pb_c_nonce[DPP_MAX_NONCE_LEN];
	u8 dpp_pb_resp_hash[SHA256_MAC_LEN];
	struct os_reltime dpp_pb_last_resp;
	bool dpp_pb_result_indicated;
	char *dpp_pb_cmd;
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */

#ifdef CONFIG_CTRL_IFACE_UDP
       unsigned char ctrl_iface_cookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */

#ifdef CONFIG_IEEE80211BE
	struct hostapd_mld **mld;
	size_t mld_count;
	int (*mld_ctrl_iface_init)(struct hostapd_mld *mld);
	void (*mld_ctrl_iface_deinit)(struct hostapd_mld *mld);
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_ATF_OFFLOAD
	struct atf_offload atf;
#endif
#ifdef CONFIG_PROCESS_COORDINATION
	struct proc_coord *pc;
#endif /* CONFIG_PROCESS_COORDINATION */

#ifdef CONFIG_MQTT
	/*
	 * Single global MQTT connection for the entire hostapd instance.
	 * Shared by all interfaces and BSSes.  Initialised on the first
	 * interface setup if mqtt_enabled=1; deinitialized on teardown.
	 */
	struct mqtt_eloop_ctx *mqtt_ctx;
#endif /* CONFIG_MQTT */
#ifdef CONFIG_QCN_EXTN
	struct hapd_interfaces_extn interfaces_extn;
#endif /* CONFIG_QCN_EXTN */

};

enum hostapd_chan_status {
	HOSTAPD_CHAN_VALID = 0, /* channel is ready */
	HOSTAPD_CHAN_INVALID = 1, /* no usable channel found */
	HOSTAPD_CHAN_ACS = 2, /* ACS work being performed */
	HOSTAPD_CHAN_INVALID_NO_IR = 3, /* channel invalid due to AFC NO IR */
};

struct hostapd_probereq_cb {
	int (*cb)(void *ctx, const u8 *sa, const u8 *da, const u8 *bssid,
		  const u8 *ie, size_t ie_len, int ssi_signal);
	void *ctx;
};

#define HOSTAPD_RATE_BASIC 0x00000001

struct hostapd_rate_data {
	int rate; /* rate in 100 kbps */
	int flags; /* HOSTAPD_RATE_ flags */
};

struct hostapd_frame_info {
	unsigned int freq;
	u32 channel;
	u32 datarate;
	int ssi_signal; /* dBm */
	struct sta_smd_ctx_info *smd_ctx;
};

enum wps_status {
	WPS_STATUS_SUCCESS = 1,
	WPS_STATUS_FAILURE
};

enum pbc_status {
	WPS_PBC_STATUS_DISABLE,
	WPS_PBC_STATUS_ACTIVE,
	WPS_PBC_STATUS_TIMEOUT,
	WPS_PBC_STATUS_OVERLAP
};

struct wps_stat {
	enum wps_status status;
	enum wps_error_indication failure_reason;
	enum pbc_status pbc_status;
	u8 peer_addr[ETH_ALEN];
};

struct hostapd_neighbor_entry {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	struct wpa_ssid_value ssid;
	struct wpabuf *nr;
	struct wpabuf *lci;
	struct wpabuf *civic;
	/* LCI update time */
	struct os_time lci_date;
	int stationary;
	u32 short_ssid;
	u8 bss_parameters;
	u8 mld_addr[ETH_ALEN];      /* MLD MAC address */
	u8 smd_id[ETH_ALEN];        /* SMD ID (all zero means absent) */
	u8 self_entry;              /* Flag: 1 if own/self AP NR entry */
};

struct hostapd_bcn_report_entry {
	struct dl_list list;
	u8 op_class;
	u8 channel;
	u64 start_time;
	u16 duration;
	u8 report_info;
	u8 rcpi;
	u8 rsni;
	u8 bssid[ETH_ALEN];
	u8 antenna_id;
	u32 parent_tsf;
	u8 *subelem;
	size_t subelem_len;
};

struct hostapd_sae_commit_queue {
	struct dl_list list;
	int rssi;
	size_t len;
	u8 msg[];
};

struct mld_link_info {
	u8 valid:1;
	u8 rejected:1;
	u8 nstr_bitmap_len:2;
	u8 local_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];

	u8 nstr_bitmap[2];

	u16 capability;

	u16 status;
	u16 resp_sta_profile_len;
	u8 *resp_sta_profile;
};

/**
 * struct hostapd_openwrt_stats - OpenWrt custom STA/AP statistics
 */
struct hostapd_openwrt_stats {
	struct {
		u64 neighbor_report_tx;
	} rrm;

	struct {
		u64 bss_transition_query_rx;
		u64 bss_transition_request_tx;
		u64 bss_transition_response_rx;
	} wnm;
};

struct rx_critical_param {
	bool critical_flag;
	u8 bpcc;
	u8 switch_count;
};

struct rx_ecu_param {
	bool critical_update;
	u8 ebpcc;
	u8 countdown;
	bool all_updates_included;
};

/**
 * enum elemid_cu - used for detecting element id modification
 * category 2 critical update
 */
enum elemid_cu {
	ELEMID_CU_PARAM_EXT_EHTOP = 0,
	ELEMID_CU_PARAM_EXT_HEOP,
	ELEMID_CU_PARAM_VHTOP,
	ELEMID_CU_PARAM_HTOP,
	ELEMID_CU_PARAM_WMM,
	ELEMID_CU_PARAM_MU_EDCA,
	ELEMID_CU_PARAM_SPATIAL_REUSE,
	ELEMID_CU_PARAM_EXT_ML,

	ELEMID_CU_PARAM_MAX,
};

struct elemid_cu_param {
	u8 eid_len;
	u32 hash;
};

/**
 * enum link_reconfigure_type - Used to distinguish the link removal/disable
 * type
 */
enum link_reconfigure_type {
	HAPD_LINK_REMOVAL, /* Default */
	HAPD_LINK_DISABLE, /* Disable the link, instead of removal */
};

#define MAX_CHANNEL_USAGE_ELEMENTS 6
#define MAX_CHANNEL_ENTRIES_PER_ELEMENT 10

/**
 * struct channel_usage_entry - Represents a single channel entry in
 * Channel Usage element
 * @op_class: Operating Class of the channel
 * @channel : Channel Number
 */
struct channel_usage_entry {
	u8 op_class;
	u8 channel;
};

/**
 * struct channel_usage_elem - Represents a single Channel Usage element
 * @mode: Mode of operation for this Channel Usage element
 * @num_entries: Number of channel entries for this particular element
 * @entries: Array of channel entries for this element
 */
struct channel_usage_elem {
	u8 mode;
	u8 num_entries;
	struct channel_usage_entry entries[MAX_CHANNEL_ENTRIES_PER_ELEMENT];
};

/**
 * struct channel_usage_config - Represents the list of all Channel
 * Usage elements that have been configured
 * @num_elems: Number of Channel Usage elements configured
 * @elems: Array of Channel Usage elements
 */
struct channel_usage_config {
	u8 num_elems;
	struct channel_usage_elem elems[MAX_CHANNEL_USAGE_ELEMENTS];
};

enum hostapd_reenable_mode {
	/* Normal operation */
	REENABLE_NONE = 0,
	/* Reuse existing iface/link: skip add/remove */
	REENABLE_REUSE_LINK = 1,
	/* HT scan in progress; defer enable */
	REENABLE_HT_SCAN = 2,
	/* CAC in progress; defer enable */
	REENABLE_CAC = 3,
	/* Interface teardown in progress */
	REENABLE_DEINIT = 4,
};


#ifdef CONFIG_IEEE80211BN
/* UHR Enhanced Critical Update (ECU) state machine states
 * as per IEEE 802.11bn section 37.30.2.2.
 */
enum uhr_ecu_state {

	/* ECU(Enhanced Critical Update) is not started */
	UHR_ECU_IDLE,

	/* ECU started, advance notification interval going on */
	UHR_ECU_ADVANCE_NOTIFY,

	/* ECU crossed advance notification interval */
	UHR_ECU_POST_ADVANCE_NOTIFY,

	/* ECU update indication included in TIM element */
	UHR_ECU_UPDATE_IND_IN_TIM,

};

struct hostapd_uhr_ecu {
	enum uhr_ecu_state state;
	u8 countdown_timer;
};
struct eth_p_1905_ctx;
#endif /* CONFIG_IEEE80211BN */

/**
 * struct hostapd_data - hostapd per-BSS data structure
 */
struct hostapd_data {
	struct hostapd_iface *iface;
	struct hostapd_config *iconf;
	struct hostapd_bss_config *conf;
	struct hostapd_ubus_bss ubus;
	struct hostapd_ucode_bss ucode;
	int interface_added; /* virtual interface added for this BSS */
	unsigned int started:1;
	unsigned int disabled:1;
	unsigned int reenable_beacon:1;

	u8 own_addr[ETH_ALEN];

#ifdef CONFIG_QCN_EXTN
	/* Vendor BSSID derivation bookkeeping for non-MBSSID */
	u8 vendor_bss_index;
	bool vendor_bss_index_valid;

	struct hostapd_data_extn hapd_extn;
#endif /* CONFIG_QCN_EXTN */

	/* OpenWrt specific statistics */
	struct hostapd_openwrt_stats openwrt_stats;

	int num_sta; /* number of entries in sta_list */
	struct sta_info *sta_list; /* STA info list head */
#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])
	struct sta_info *sta_hash[STA_HASH_SIZE];

	/*
	 * Bitfield for indicating which AIDs are allocated. Only AID values
	 * 1-2007 are used and as such, the bit at index 1 corresponds to AID
	 * 1.
	 */
#define AID_WORDS ((2008 + 31) / 32)
	u32 sta_aid[AID_WORDS];
	u32 wds_sta_uid[AID_WORDS];

#ifdef CONFIG_QCN_EXTN
#define WDS_STA_UID_REPURPOSED_BASE 3001
#define WDS_STA_UID_REPURPOSED_PER_LINK 1000
#define WDS_STA_UID_REPURPOSED_WORDS \
	((WDS_STA_UID_REPURPOSED_PER_LINK + 31) / 32)
	u32 wds_sta_uid_repurpose[WDS_STA_UID_REPURPOSED_WORDS];
#endif /* CONFIG_QCN_EXTN */

	const struct wpa_driver_ops *driver;
	void *drv_priv;

	void (*new_assoc_sta_cb)(struct hostapd_data *hapd,
				 struct sta_info *sta, int reassoc);

	void *msg_ctx; /* ctx for wpa_msg() calls */
	void *msg_ctx_parent; /* parent interface ctx for wpa_msg() calls */

	int num_rates;
	struct hostapd_rate_data *current_rates;
	int *basic_rates;

	struct radius_client_data *radius;
	u64 acct_session_id;
	struct radius_das_data *radius_das;

	struct hostapd_cached_radius_acl *acl_cache;
	struct hostapd_acl_query_data *acl_queries;

	struct wpa_authenticator *wpa_auth;
	struct eapol_authenticator *eapol_auth;
	struct eap_config *eap_cfg;

	struct rsn_preauth_interface *preauth_iface;
	struct os_reltime michael_mic_failure;
	int michael_mic_failures;
	int tkip_countermeasures;

	int ctrl_sock;
	struct dl_list ctrl_dst;

	void *ssl_ctx;
	void *eap_sim_db_priv;
	struct crypto_rsa_key *imsi_privacy_key;
	struct radius_server_data *radius_srv;
	struct dl_list erp_keys; /* struct eap_server_erp_key */

	int parameter_set_count;
	bool tpc_eirp_valid;
	s32 tpc_eirp_dbm;

	/* Time Advertisement */
	u8 time_update_counter;
	struct wpabuf *time_adv;

#ifdef CONFIG_FULL_DYNAMIC_VLAN
	struct full_dynamic_vlan *full_dynamic_vlan;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

	struct l2_packet_data *l2;

#ifdef CONFIG_IEEE80211R_AP
	struct dl_list l2_queue;
	struct dl_list l2_oui_queue;
	struct eth_p_oui_ctx *oui_pull;
	struct eth_p_oui_ctx *oui_resp;
	struct eth_p_oui_ctx *oui_push;
	struct eth_p_oui_ctx *oui_sreq;
	struct eth_p_oui_ctx *oui_sresp;
	struct eth_p_oui_ctx *oui_rnotify;
#endif /* CONFIG_IEEE80211R_AP */

	struct wps_context *wps;

	int beacon_set_done;
	u8 reenable;
	struct wpabuf *wps_beacon_ie;
	struct wpabuf *wps_probe_resp_ie;
	struct wpabuf *plugin_vendor_elements; /* Dynamic vendor IEs set by plugin */
#ifdef CONFIG_WPS
	unsigned int ap_pin_failures;
	unsigned int ap_pin_failures_consecutive;
	struct upnp_wps_device_sm *wps_upnp;
	unsigned int ap_pin_lockout_time;

	struct wps_stat wps_stats;
#endif /* CONFIG_WPS */

#ifdef CONFIG_MACSEC
	struct ieee802_1x_kay *kay;
#endif /* CONFIG_MACSEC */

	struct hostapd_probereq_cb *probereq_cb;
	size_t num_probereq_cb;

	void (*public_action_cb)(void *ctx, const u8 *buf, size_t len,
				 int freq);
	void *public_action_cb_ctx;
	void (*public_action_cb2)(void *ctx, const u8 *buf, size_t len,
				  int freq);
	void *public_action_cb2_ctx;

	int (*vendor_action_cb)(void *ctx, const u8 *buf, size_t len,
				int freq);
	void *vendor_action_cb_ctx;

	void (*wps_reg_success_cb)(void *ctx, const u8 *mac_addr,
				   const u8 *uuid_e);
	void *wps_reg_success_cb_ctx;

	void (*wps_event_cb)(void *ctx, enum wps_event event,
			     union wps_event_data *data);
	void *wps_event_cb_ctx;

	void (*sta_authorized_cb)(void *ctx, const u8 *mac_addr,
				  int authorized, const u8 *p2p_dev_addr,
				  const u8 *ip);
	void *sta_authorized_cb_ctx;

	void (*setup_complete_cb)(void *ctx);
	void *setup_complete_cb_ctx;

	void (*new_psk_cb)(void *ctx, const u8 *mac_addr,
			   const u8 *p2p_dev_addr, const u8 *psk,
			   size_t psk_len);
	void *new_psk_cb_ctx;

#ifdef CONFIG_IEEE8021X_AUTH
	void (*send_eap_req)(struct hostapd_data *hapd, struct sta_info *sta,
			     u8 type, u16 auth_transaction, u16 status,
			     struct rsn_pmksa_cache_entry *cached_pmk,
			     const u8 *eap_req, size_t eap_req_len);
#endif /* CONFIG_IEEE8021X_AUTH */

	/* channel switch parameters */
	struct hostapd_freq_params cs_freq_params;
	u8 cs_count;
	int cs_block_tx;
	unsigned int cs_c_off_beacon;
	unsigned int cs_c_off_proberesp;
	int csa_in_progress;
	unsigned int cs_c_off_ecsa_beacon;
	unsigned int cs_c_off_ecsa_proberesp;

#ifdef CONFIG_IEEE80211AX
	bool cca_in_progress;
	int no_free_color;
	u8 cca_count;
	u8 cca_color;
	unsigned int cca_c_off_beacon;
	unsigned int cca_c_off_proberesp;
	struct os_reltime first_color_collision;
	struct os_reltime last_color_collision;
	u64 color_collision_bitmap;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BN
	struct hostapd_uhr_ecu uhr_ecu;
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_P2P
	struct p2p_data *p2p;
	struct p2p_group *p2p_group;
	struct wpabuf *p2p_beacon_ie;
	struct wpabuf *p2p_probe_resp_ie;

	/* Number of non-P2P association stations */
	int num_sta_no_p2p;

	/* Periodic NoA (used only when no non-P2P clients in the group) */
	int noa_enabled;
	int noa_start;
	int noa_duration;
#endif /* CONFIG_P2P */
#ifdef CONFIG_PROXYARP
	struct l2_packet_data *sock_dhcp;
	struct l2_packet_data *sock_ndisc;
	bool x_snoop_initialized;
#endif /* CONFIG_PROXYARP */
#ifdef CONFIG_MESH
	int num_plinks;
	int max_plinks;
	void (*mesh_sta_free_cb)(struct hostapd_data *hapd,
				 struct sta_info *sta);
	struct wpabuf *mesh_pending_auth;
	struct os_reltime mesh_pending_auth_time;
	u8 mesh_required_peer[ETH_ALEN];
#endif /* CONFIG_MESH */

#ifdef CONFIG_SQLITE
	struct hostapd_eap_user tmp_eap_user;
#endif /* CONFIG_SQLITE */

#ifdef CONFIG_SAE

#define COMEBACK_KEY_SIZE 8
#define COMEBACK_PENDING_IDX_SIZE 256

	/** Key used for generating SAE anti-clogging tokens */
	u8 comeback_key[COMEBACK_KEY_SIZE];
	struct os_reltime last_comeback_key_update;
	u16 comeback_idx;
	u16 comeback_pending_idx[COMEBACK_PENDING_IDX_SIZE];
	int dot11RSNASAERetransPeriod; /* msec */
	struct dl_list sae_commit_queue; /* struct hostapd_sae_commit_queue */
#endif /* CONFIG_SAE */

#ifdef CONFIG_TESTING_OPTIONS
	unsigned int ext_mgmt_frame_handling:1;
	unsigned int ext_eapol_frame_io:1;

	struct l2_packet_data *l2_test;

	enum wpa_alg last_gtk_alg;
	int last_gtk_key_idx;
	u8 last_gtk[WPA_GTK_MAX_LEN];
	size_t last_gtk_len;

	enum wpa_alg last_igtk_alg;
	int last_igtk_key_idx;
	u8 last_igtk[WPA_IGTK_MAX_LEN];
	size_t last_igtk_len;

	enum wpa_alg last_bigtk_alg;
	int last_bigtk_key_idx;
	u8 last_bigtk[WPA_BIGTK_MAX_LEN];
	size_t last_bigtk_len;

	bool force_backlog_bytes;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_MBO
	unsigned int mbo_assoc_disallow;
	u8 mbo_trans_reason;
	u16 mbo_assoc_retry;
#endif /* CONFIG_MBO */
	/* OCE: cached channel survey results for OCE Capability Indication */
	bool non_oce_ap_present;
	bool ap_11b_present;


	struct dl_list nr_db;
	struct dl_list bcn_report_db;

	u8 beacon_req_token;
	u8 lci_req_token;
	u8 range_req_token;
	u8 link_measurement_req_token;
	u8 chan_load_req_token;
	u8 noise_hist_req_token;
	unsigned int lci_req_active:1;
	unsigned int range_req_active:1;
	unsigned int link_mesr_req_active:1;
	unsigned int chan_load_req_active:1;
	unsigned int noise_hist_req_active:1;

	int dhcp_sock; /* UDP socket used with the DHCP server */

	struct ptksa_cache *ptksa;

#ifdef CONFIG_DPP
	int dpp_init_done;
	struct dpp_authentication *dpp_auth;
	u8 dpp_allowed_roles;
	int dpp_qr_mutual;
	int dpp_auth_ok_on_ack;
	bool dpp_wps;
	int dpp_in_response_listen;
	struct gas_query_ap *gas;
	struct dpp_pkex *dpp_pkex;
	struct dpp_bootstrap_info *dpp_pkex_bi;
	char *dpp_pkex_code;
	size_t dpp_pkex_code_len;
	char *dpp_pkex_identifier;
	enum dpp_pkex_ver dpp_pkex_ver;
	char *dpp_pkex_auth_cmd;
	struct os_reltime dpp_last_init;
	struct os_reltime dpp_init_iter_start;
	unsigned int dpp_init_max_tries;
	unsigned int dpp_init_retry_time;
	unsigned int dpp_resp_wait_time;
	unsigned int dpp_resp_max_tries;
	unsigned int dpp_resp_retry_time;
#ifdef CONFIG_DPP2
	struct wpabuf *dpp_presence_announcement;
	struct dpp_bootstrap_info *dpp_chirp_bi;
	int dpp_chirp_freq;
	int *dpp_chirp_freqs;
	int dpp_chirp_iter;
	int dpp_chirp_round;
	int dpp_chirp_scan_done;
	int dpp_chirp_listen;
	struct os_reltime dpp_relay_last_needs_ctrl;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_TESTING_OPTIONS
	char *dpp_config_obj_override;
	char *dpp_discovery_override;
	char *dpp_groups_override;
	unsigned int dpp_ignore_netaccesskey_mismatch:1;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_DPP */

#ifdef CONFIG_AIRTIME_POLICY
	unsigned int num_backlogged_sta;
	unsigned int airtime_weight;
#endif /* CONFIG_AIRTIME_POLICY */
#ifdef CONFIG_ATF_OFFLOAD
        bool atf_configured;
#endif

	u8 last_1x_eapol_key_replay_counter[8];

#ifdef CONFIG_SQLITE
	sqlite3 *rad_attr_db;
#endif /* CONFIG_SQLITE */

#ifdef CONFIG_CTRL_IFACE_UDP
       unsigned char ctrl_iface_cookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */

       struct rx_critical_param rx_cu_param;
       /*Element ID critical update data*/
       struct elemid_cu_param cu_eid[ELEMID_CU_PARAM_MAX];
       struct rx_ecu_param rx_ecu_param;
#ifdef CONFIG_IEEE80211BE
	u8 eht_mld_bss_param_change;
	struct hostapd_mld *mld;
	struct dl_list link;
	u8 mld_link_id;

	/* Cached partner info for ML probe response */
	struct mld_link_info partner_links[MAX_NUM_MLD_LINKS];

	/* 5 characters for "_link", up to 2 characters for <link ID>, so in
	 * total, additional 7 characters required. */
	char ctrl_sock_iface[IFNAMSIZ + 7 + 1];

	u8 eht_mld_link_removal_count;
	u8 eht_mld_link_removal_inprogress;
	enum link_reconfigure_type removal_type;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_NAN_USD
	struct nan_de *nan_de;
#endif /* CONFIG_NAN_USD */

	/* Per-module log level overrides. -1 means no override is set for
	 * that module and logging falls back to the global wpa_debug_level.
	 * Any other value is a HOSTAPD_LEVEL_* threshold: messages below it
	 * are suppressed regardless of the global level. Set via
	 * LOG_LEVEL <module> <level> ctrl_iface command. */
	int log_module_level[HOSTAPD_MOD_MAX];
	/* Per-peer log filter. Set by LOG_PEER ctrl command. */
	u8  log_peer_filter_set;
	u8  log_peer_addr[ETH_ALEN];

	u64 scan_cookie; /* Scan instance identifier for the ongoing HT40 scan
			  */
	bool disable_cu;
	struct hostapd_wmm_ac_params prev_wmm[4];
	size_t mbssid_idx;
	struct hostapd_multi_mbssid_group *mbssid_group;
	struct dl_list mbssid_bss;

	u16 mapping_switch_time;
	struct channel_usage_config chan_usage_config;
	void *hostapd_if_data; /* for per-interface/MLD frame_reg_table, opaque */

#ifdef CONFIG_IEEE80211BN
	struct smd_neighbor_update_ctx *smd_neighbor_update_ctx;
	struct dl_list mapc_discovery_reqs;
	u16 mapc_discovered_ap_count;
	u16 bss_active_peer_count;
	u8  bss_cotdma_active_count;
	u8  mapc_dialog_token_count;
	bool mapc_initialized;
	struct eth_p_1905_ctx *eth_p_1905_ctx;
#endif /* CONFIG_IEEE80211BN */

	bool is_update_beacon; /* To indentify whether its from UPDATE_BEACON comamnd */
	bool sp_ie_activated_sta;  /* Set when processing STA with validated SP IE */
	/* Transient guard: set during SET_RTT_RESPONDER_ROLE runtime update to
	 * suppress duplicate vendor cmd during the triggered beacon rebuild. */
	bool rtt_role_fw_sent;
};
/* True if hapd has a pending re-enable for any reason (user-disabled,
 * HT-scan deferred, or CAC in progress). */
static inline bool hapd_reenable_pending(const struct hostapd_data *hapd)
{
	return hapd->reenable == REENABLE_REUSE_LINK ||
		hapd->reenable == REENABLE_HT_SCAN ||
		hapd->reenable == REENABLE_CAC;
}


struct hostapd_sta_info {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	struct os_reltime last_seen;
	int ssi_signal;
	struct os_reltime probe_first_low_rssi_seen;
	int probe_delay_count;
#ifdef CONFIG_TAXONOMY
	struct wpabuf *probe_ie_taxonomy;
#endif /* CONFIG_TAXONOMY */
};

#ifdef CONFIG_IEEE80211BE

/**
 * struct hostapd_ft_over_ds_ml_sta_entry - ft over ds ml sta structure
 */
struct hostapd_ft_over_ds_ml_sta_entry {
	struct dl_list list;
	u8 mld_mac[ETH_ALEN];
	struct wpa_state_machine *wpa_sm;
	struct wpa_authenticator *wpa_auth;
	struct hostapd_data *hapd;
};

/**
 * struct hostapd_mld - hostapd per-mld data structure
 */
struct hostapd_mld {
	char name[IFNAMSIZ + 1];
	u8 mld_addr[ETH_ALEN];
	u16 free_links;
	u8 num_links;
	/* Number of hostapd_data (hapd) referencing this. num_links cannot be
	 * used since num_links can go to 0 even when a BSS is disabled and
	 * when it is re-enabled, the MLD should exist and hence it cannot be
	 * freed when num_links is 0.
	 */
	u8 refcount;

	struct hostapd_data *fbss;
	struct dl_list links; /* List head of all affiliated links */
	struct dl_list ft_ds_ml_stas;

	int ctrl_sock;
	struct dl_list ctrl_dst;
	char *ctrl_interface; /* Directory for UNIX domain sockets */

#ifdef CONFIG_IEEE80211BE
	struct mac_acl_entry *epcs_authorized_mac;
	int num_epcs_authorized_mac;
	/* true if any link in this MLD uses configured (mld_link_id) allocation */
	int link_id_mode;
#endif /* CONFIG_IEEE80211BE */
	struct ttlm_context ttlm_ctx;
};

#define HOSTAPD_MLD_MAX_REF_COUNT      0xFF
#endif /* CONFIG_IEEE80211BE */


/**
 * enum cac_completion_type - CAC completion context
 * @HAPD_CAC_COMPLETE_AFTER_BSS: CAC completed after a regular BSS
 *                               CAC procedure (non-CSA path).
 * @HAPD_CAC_COMPLETE_AFTER_CSA: CAC completed after CSA
 *                               on a DFS target channel.
 */
enum cac_completion_type {
	HAPD_CAC_COMPLETE_AFTER_BSS = 0,
	HAPD_CAC_COMPLETE_AFTER_CSA = 1,
};

/**
 * struct hostapd_iface - hostapd per-interface data structure
 */
struct hostapd_iface {
	struct hapd_interfaces *interfaces;
	struct hostapd_ucode_iface ucode;
	void *owner;
	char *config_fname;
	struct hostapd_config *conf;
	char phy[16]; /* Name of the PHY (radio) */

	enum hostapd_iface_state {
		HAPD_IFACE_UNINITIALIZED,
		HAPD_IFACE_DISABLED,
		HAPD_IFACE_COUNTRY_UPDATE,
		HAPD_IFACE_ACS,
		HAPD_IFACE_HT_SCAN,
		HAPD_IFACE_DFS,
		HAPD_IFACE_NO_IR,
		HAPD_IFACE_ENABLED
	} state;

#ifdef CONFIG_MESH
	struct mesh_conf *mconf;
#endif /* CONFIG_MESH */

	size_t num_bss;
	struct hostapd_data **bss;

#ifdef CONFIG_QCN_EXTN
	/* Bitmask of used vendor BSSID indices (non-MBSSID) */
	u32 vendor_bssid_used_mask;
	u16 radar_bit_pattern_extn;
	/* Set when boot-up CAC is active: all 5 GHz BSS were created before CAC
	 * completed and vdev_up is deferred until CAC finishes.
	 */
	unsigned int bootup_cac_in_progress:1;
#endif /* CONFIG_QCN_EXTN */

	unsigned int wait_channel_update:1;
	unsigned int cac_started:1;
	unsigned int skip_mesh_dfs:1;
#ifdef CONFIG_FST
	struct fst_iface *fst;
	const struct wpabuf *fst_ies;
#endif /* CONFIG_FST */

	/*
	 * When set, indicates that the driver will handle the AP
	 * teardown: delete global keys, station keys, and stations.
	 */
	unsigned int driver_ap_teardown:1;

	/*
	 * When set, indicates that this interface is part of list of
	 * interfaces that need to be started together (synchronously).
	 */
	unsigned int need_to_start_in_sync:1;

	/* Ready to start but waiting for other interfaces to become ready. */
	unsigned int ready_to_start_in_sync:1;

	int num_ap; /* number of entries in ap_list */
	struct ap_info *ap_list; /* AP info list head */
	struct ap_info *ap_hash[STA_HASH_SIZE];

	u64 drv_flags;
	u64 drv_flags2;
	unsigned int drv_rrm_flags;

	/* Maximum number of stations supported by driver (AP mode) */
	unsigned int drv_max_stations;

	/*
	 * A bitmap of supported protocols for probe response offload. See
	 * struct wpa_driver_capa in driver.h
	 */
	unsigned int probe_resp_offloads;

	/* extended capabilities supported by the driver */
	const u8 *extended_capa, *extended_capa_mask;
	unsigned int extended_capa_len;

	u16 mld_eml_capa, mld_mld_capa, mld_ext_mld_capa;

	unsigned int drv_max_acl_mac_addrs;

	struct hostapd_hw_modes *hw_features;
	int num_hw_features;
	struct hostapd_hw_modes *current_mode;
	int freq;
#ifdef CONFIG_QCN_EXTN
	u32 mcst;
#endif

	bool radar_detected;
	enum cac_completion_type cac_type;

	/* Background radar configuration */
	struct {
		int channel;
		int secondary_channel;
		int freq;
		int centr_freq_seg0_idx;
		int centr_freq_seg1_idx;
		enum oper_chan_width chwidth;
		/* Main chain is on temporary channel during
		 * CAC detection on radar offchain.
		 */
		unsigned int temp_ch:1;
		/* CAC started on radar offchain */
		unsigned int cac_started:1;
		struct os_reltime dfs_cac_start;
	} radar_background;

	/*
	 * User-configured RCAC channel
	 */
	int user_rcac_channel;

	u16 hw_flags;

	/* Number of associated Non-ERP stations (i.e., stations using 802.11b
	 * in 802.11g BSS) */
	int num_sta_non_erp;

	/* Number of associated stations that do not support Short Slot Time */
	int num_sta_no_short_slot_time;

	/* Number of associated stations that do not support Short Preamble */
	int num_sta_no_short_preamble;

	int olbc; /* Overlapping Legacy BSS Condition */

	/* Number of HT associated stations that do not support greenfield */
	int num_sta_ht_no_gf;

	/* Number of associated non-HT stations */
	int num_sta_no_ht;

	/* Number of HT associated stations 20 MHz */
	int num_sta_ht_20mhz;

	/* Number of HT40 intolerant stations */
	int num_sta_ht40_intolerant;

	/* Overlapping BSS information */
	int olbc_ht;

	u16 ht_op_mode;

	/* surveying helpers */

	/* number of channels surveyed */
	unsigned int chans_surveyed;

	/* lowest observed noise floor in dBm */
	s8 lowest_nf;

	/*
	 * User configured 6 GHz power mode
	 *
	 * No power mode change - -1
	 * Low power Indoor AP  -  0
	 * Standard power AP    -  1
	 * Very low power AP    -  2
	 *
	 */
	int power_mode_6ghz_before_change;

	/* channel utilization calculation */
	u64 last_channel_time;
	u64 last_channel_time_busy;
	u8 channel_utilization;
	struct os_reltime ch_util_update_time;

	unsigned int chan_util_samples_sum;
	unsigned int chan_util_num_sample_periods;
	unsigned int chan_util_average;

	/* eCSA IE will be added only if operating class is specified */
	u8 cs_oper_class;

	unsigned int dfs_cac_ms;
	struct os_reltime dfs_cac_start;

	/* Latched with the actual secondary channel information and will be
	 * used while juggling between HT20 and HT40 modes. */
	int secondary_ch;

#ifdef CONFIG_ACS
	unsigned int acs_num_completed_scans;
	unsigned int acs_num_retries;
	/* Parallel scan: defer setup until all MLD partners complete ACS */
	unsigned int acs_setup_deferred:1;
#endif /* CONFIG_ACS */

	void (*scan_cb)(struct hostapd_iface *iface);
	int num_ht40_scan_tries;

	struct dl_list sta_seen; /* struct hostapd_sta_info */
	unsigned int num_sta_seen;

	u8 dfs_domain;
#ifdef CONFIG_AIRTIME_POLICY
	unsigned int airtime_quantum;
#endif /* CONFIG_AIRTIME_POLICY */

#ifdef CONFIG_ATF_OFFLOAD
        struct atf_algo *atf_algo;
#endif /* CONFIG_ATF_OFFLOAD */

	/* Previous WMM element information */
	struct hostapd_wmm_ac_params prev_wmm[WMM_AC_NUM];

	/* Maximum number of interfaces supported for MBSSID advertisement */
	unsigned int mbssid_max_interfaces;
	/* Maximum profile periodicity for enhanced MBSSID advertisement */
	unsigned int ema_max_periodicity;

	/* Radar infected sub channel bit pattern */
	u16 radar_bit_pattern;

	int (*enable_iface_cb)(struct hostapd_iface *iface);
	int (*disable_iface_cb)(struct hostapd_iface *iface);

	/* Configured freq of interface is NO_IR */
	bool is_no_ir;

	bool is_ch_switch_dfs; /* Channel switch from ACS to DFS */
	/* 6 GHz AFC information */
	struct afc_sp_reg_info *afc_rsp_info;
	bool is_afc_power_event_received;
	/* After receiving AFC power event in retail mode, this flag is to set to
	 * true to indicate that the channel change is pending. Channel is changed
	 * once NL8011_WIPHY_REG_CHANGE event is received.
	 */
	bool is_afc_channel_change_pending;
	/* Connected repeater defers AFC power sync until the updated
	 * channel list/regulatory view is received through
	 * CHANNEL_LIST_CHANGED.
	 */
	bool is_afc_repeater_power_sync_pending;
	/* Cached PSD value for RNR */
	s8 rnr_psd;

#ifdef CONFIG_QCN_EXTN
	struct hostapd_iface_extn iface_extn;
#endif /* CONFIG_QCN_EXTN */

	struct hostapd_multi_hw_info *multi_hw_info;
	unsigned int num_multi_hws;
	struct hostapd_multi_hw_info *current_hw_info;
	/* Multi MBSSID group information */
	struct hostapd_multi_mbssid multi_mbssid;
	u32 mbssid_idx_bmap;
	size_t max_mgmt_frm_sz;
	u32 cs_time;
	int last_scan_aborted;
	bool csa_pending_on_cac_abort;
	struct csa_settings csa_settings;

	/* Monitor interface management */
	char monitor_iface[IFNAMSIZ + 1];
	int monitor_ifindex;
	bool monitor_iface_configured;

	/*
	 * When boot/CSA target is a DFS channel,
	 * preferred channel/width are saved here while AP temporarily operates
	 * on conf->intercac_chan.
	 * 0 means Inter CAC is not active.
	 */
	int preferred_chan;
	enum oper_chan_width preferred_chan_width;
#ifdef CONFIG_IEEE80211BN
	u32  mapc_hw_capability_bitmap;
	u8   mapc_max_ctdma_peers;
	u16  mapc_active_peer_count;
	u8   mapc_cotdma_active_count;
	bool mapc_iface_initialized;
#endif /* CONFIG_IEEE80211BN */
};


/*
 * enum peer_epcs_state - EPCS states of peer
 */
enum peer_epcs_state {
	EPCS_STATE_DISABLED = 0, /* The default state */
	EPCS_STATE_ENABLE_REQ_SENT, /* EPCS request sent to the peer, waiting for EPCS response */
	EPCS_STATE_ENABLED, /* EPCS session established */
};

/* TID value to be used when EPCS is enabled */
#define EPCS_TID_VALUE	7

/*
 * struct wlan_epcs_info - EPCS information of frame
 */
struct wlan_epcs_info {
	u8 action_code;
	u8 dialog_token;
	u16 status_code;
};

/*
 * struct mld_peer_epcs_info - per mld-peer epcs info
 */
struct mld_peer_epcs_info {
	bool is_epcs_capable;
	enum peer_epcs_state state;
	u8 self_gen_dialog_token;
	bool timer_started;
	u64 rule_handle;
};


/* hostapd.c */
int hostapd_for_each_interface(struct hapd_interfaces *interfaces,
			       int (*cb)(struct hostapd_iface *iface,
					 void *ctx), void *ctx);
int hostapd_reload_config(struct hostapd_iface *iface);
int hostapd_reload_config_bss(struct hostapd_iface *iface,
                              const char *iface_name,
                              char *buf);
void hostapd_reconfig_encryption(struct hostapd_data *hapd);
struct hostapd_data *
hostapd_alloc_bss_data(struct hostapd_iface *hapd_iface,
		       struct hostapd_config *conf,
		       struct hostapd_bss_config *bss);
int hostapd_set_ctrl_sock_iface(struct hostapd_data *hapd);
int hostapd_setup_interface(struct hostapd_iface *iface);
int hostapd_setup_interface_complete(struct hostapd_iface *iface, int err);
void hostapd_set_own_neighbor_report(struct hostapd_data *hapd);
void hostapd_interface_deinit(struct hostapd_iface *iface);
void hostapd_interface_free(struct hostapd_iface *iface);
struct hostapd_iface * hostapd_alloc_iface(void);
struct hostapd_iface * hostapd_init(struct hapd_interfaces *interfaces,
				    const char *config_file);
struct hostapd_iface *
hostapd_interface_init_bss(struct hapd_interfaces *interfaces, const char *phy,
			   const char *config_fname, int debug);
int hostapd_setup_bss(struct hostapd_data *hapd, bool first, bool start_beacon);
void hostapd_bss_deinit(struct hostapd_data *hapd);
void hostapd_bss_setup_multi_link(struct hostapd_data *hapd,
				  struct hapd_interfaces *interfaces);
void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc);
void hostapd_interface_deinit_free(struct hostapd_iface *iface);
void hostapd_deauthenticate_stations(struct hapd_interfaces *interfaces);
int hostapd_iface_num_sta(struct hostapd_iface *iface);
int hostapd_enable_iface(struct hostapd_iface *hapd_iface);
int hostapd_reload_iface(struct hostapd_iface *hapd_iface);
int hostapd_reload_bss_only(struct hostapd_data *bss);
int hostapd_disable_bss(struct hostapd_data *hapd, int tbtt, const char *event);
int hostapd_enable_bss(struct hostapd_data *hapd);
int hostapd_disable_iface(struct hostapd_iface *hapd_iface);
void hostapd_clear_local_tpe_bss(struct hostapd_data *hapd);
void hostapd_clear_local_tpe(struct hostapd_iface *iface);
void hostapd_bss_deinit_no_free(struct hostapd_data *hapd);
void hostapd_free_hapd_data(struct hostapd_data *hapd);
void hostapd_cleanup_iface_partial(struct hostapd_iface *iface);
int hostapd_add_iface(struct hapd_interfaces *ifaces, char *buf);
int hostapd_remove_hapd_iface(struct hostapd_iface *hapd_iface);
int hostapd_remove_iface(struct hapd_interfaces *ifaces, char *buf);
void hostapd_channel_list_updated(struct hostapd_iface *iface, int initiator);
void hostapd_set_state(struct hostapd_iface *iface, enum hostapd_iface_state s);
const char * hostapd_state_text(enum hostapd_iface_state s);
int hostapd_csa_in_progress(struct hostapd_iface *iface);
void hostapd_chan_switch_config(struct hostapd_data *hapd,
				struct hostapd_freq_params *freq_params);
int hostapd_switch_channel(struct hostapd_data *hapd,
			   struct csa_settings *settings);
int hostapd_abort_dfs_cac(struct hostapd_data *hapd);
int hostapd_abort_cac_for_channel_switch(struct hostapd_iface *iface,
					 struct csa_settings *settings);
void hostapd_get_channel_switch_time(struct hostapd_iface *iface,
				     struct hostapd_freq_params *freq_params);
int hostapd_force_channel_switch(struct hostapd_iface *iface,
				 struct csa_settings *settings);
void
hostapd_switch_channel_fallback(struct hostapd_iface *iface,
				const struct hostapd_freq_params *freq_params);
void hostapd_cleanup_cs_params(struct hostapd_data *hapd);
void hostapd_periodic_iface(struct hostapd_iface *iface);
int hostapd_owe_trans_get_info(struct hostapd_data *hapd);
void hostapd_ocv_check_csa_sa_query(void *eloop_ctx, void *timeout_ctx);
int hostapd_check_max_sta(struct hostapd_data *hapd);
void hostapd_mac_auth_timeout(void *eloop_ctx, void *timeout_ctx);

bool hostapd_is_cca_in_progress(struct hostapd_iface *iface);
void hostapd_switch_color(struct hostapd_data *hapd, u64 bitmap);
void hostapd_cleanup_cca_params(struct hostapd_data *hapd);

void hostapd_free_afc_data(struct hostapd_iface *iface);
void hostapd_check_get_afc_details(struct hostapd_data *hapd);
/* utils.c */
int hostapd_register_probereq_cb(struct hostapd_data *hapd,
				 int (*cb)(void *ctx, const u8 *sa,
					   const u8 *da, const u8 *bssid,
					   const u8 *ie, size_t ie_len,
					   int ssi_signal),
				 void *ctx);
void hostapd_prune_associations(struct hostapd_data *hapd, const u8 *addr,
				int mld_assoc_link_id);

/* drv_callbacks.c (TODO: move to somewhere else?) */
void hostapd_notify_assoc_fils_finish(struct hostapd_data *hapd,
				      struct sta_info *sta);
int hostapd_notif_assoc(struct hostapd_data *hapd, const u8 *addr,
			const u8 *req_ie, size_t req_ielen, const u8 *resp_ie,
			size_t resp_ielen, const u8 *link_addr, int reassoc);
void hostapd_notif_disassoc(struct hostapd_data *hapd, const u8 *addr);
void hostapd_notif_disassoc_mld(struct hostapd_data *assoc_hapd,
				struct sta_info *sta, const u8 *addr);
void hostapd_event_sta_low_ack(struct hostapd_data *hapd, const u8 *addr,
			       const u32 num_packets);
void hostapd_event_connect_failed_reason(struct hostapd_data *hapd,
					 const u8 *addr, int reason_code);
int hostapd_probe_req_rx(struct hostapd_data *hapd, const u8 *sa, const u8 *da,
			 const u8 *bssid, const u8 *ie, size_t ie_len,
			 int ssi_signal);
void hostapd_event_ch_switch(struct hostapd_data *hapd, int freq, int ht,
			     int offset, int width, int cf1, int cf2,
			     u16 punct_bitmap, u8 power_mode_6ghz,
			     int width_device, int cf_device, int finished);
struct survey_results;
void hostapd_event_get_survey(struct hostapd_iface *iface,
			      struct survey_results *survey_results);
void hostapd_acs_channel_selected(struct hostapd_data *hapd,
				  struct acs_selected_channels *acs_res);

const struct hostapd_eap_user *
hostapd_get_eap_user(struct hostapd_data *hapd, const u8 *identity,
		     size_t identity_len, int phase2);

struct hostapd_data * hostapd_get_iface(struct hapd_interfaces *interfaces,
					const char *ifname);
void hostapd_event_sta_opmode_changed(struct hostapd_data *hapd, const u8 *addr,
				      enum smps_mode smps_mode,
				      enum chan_width chan_width, u8 rx_nss);

int hostapd_change_config_freq(struct hostapd_data *hapd,
			       struct hostapd_config *conf,
			       struct hostapd_freq_params *params,
			       struct hostapd_freq_params *old_params);

#ifdef CONFIG_IEEE80211BN
bool hostapd_is_mapc_action(const struct ieee80211_mgmt *mgmt, size_t len);
#endif /* CONFIG_IEEE80211BN */
#ifdef CONFIG_FST
void fst_hostapd_fill_iface_obj(struct hostapd_data *hapd,
				struct fst_wpa_obj *iface_obj);
#endif /* CONFIG_FST */

int hostapd_set_acl(struct hostapd_data *hapd);
int hostapd_tx_bss_only(struct hostapd_data *hapd, const char *op_name);
struct hostapd_data * hostapd_mbssid_get_tx_bss(struct hostapd_data *hapd);
unsigned int hostapd_mbssid_get_bss_index(struct hostapd_data *hapd);
size_t hostapd_get_mbssid_max_num_bss(struct hostapd_data *hapd);
struct hostapd_data * hostapd_mld_get_link_bss(struct hostapd_data *hapd,
					       u8 link_id);
int hostapd_link_remove(struct hostapd_data *hapd, u32 count,
			enum link_reconfigure_type removal_type);
struct hostapd_data *
hostapd_interfaces_get_hapd(struct hapd_interfaces *interfaces,
			    const char *ifname);
bool hostapd_is_ml_partner(struct hostapd_data *hapd1,
			   struct hostapd_data *hapd2);
u8 hostapd_get_mld_id(struct hostapd_data *hapd);
int hostapd_mld_add_link(struct hostapd_data *hapd);
int hostapd_mld_remove_link(struct hostapd_data *hapd);
void hostapd_bss_link_deinit(struct hostapd_data *hapd);
u8 hostapd_get_active_links(struct hostapd_data *hapd);
struct hostapd_data * hostapd_mld_get_first_bss(struct hostapd_data *hapd);

int hostapd_build_beacon_data(struct hostapd_data *hapd,
			      struct beacon_data *beacon);
void free_beacon_data(struct beacon_data *beacon);
int hostapd_fill_cca_settings(struct hostapd_data *hapd,
			      struct cca_settings *settings);
bool hostapd_check_reenable_bss(struct hostapd_iface *iface,
				enum hostapd_reenable_mode reason);
bool
hostapd_switch_pending_bss(struct hostapd_iface *iface,
			   struct csa_settings *settings,
			   enum hostapd_reenable_mode reason);
bool hostapd_enable_pending_bss(struct hostapd_iface *iface,
				enum hostapd_reenable_mode reason,
				bool dfs_cleanup);
int hostapd_no_ir_channel_list_updated(struct hostapd_iface *iface);
#ifdef CONFIG_IEEE80211BE

void hostapd_set_ml_max_rec_links(struct hostapd_data *hapd, u8 ml_max_rec_links);
void hostapd_validate_update_ml_max_rec_links(struct hostapd_data *hapd);
bool hostapd_mld_is_first_bss(struct hostapd_data *hapd);
void hostapd_mld_interface_freed(struct hostapd_data *hapd);
int hostapd_wnm_add_multi_link_sub_elem(struct hostapd_data *hapd,
					u8 *links, u8 num_links,
					u8 *pos, size_t len);

#ifdef CONFIG_QCN_EXTN
/* for_each_mld_link iterator skips repurposed link. Use this when self is not
 * repurposed. To loop all links despite of repurpose state, use
 * for_each_mld_link_include_repurposed iterator
 */
#define for_each_mld_link(partner, self) \
	dl_list_for_each(partner, &self->mld->links, struct hostapd_data, link) \
		if (hostapd_is_repurpose_disabled_11be_extn(partner->conf)) { \
			continue; \
		} else

#define for_each_mld_link_include_repurposed(partner, self) \
	dl_list_for_each(partner, &self->mld->links, struct hostapd_data, link)

#else /* CONFIG_QCN_EXTN */
#define for_each_mld_link(partner, self) \
	dl_list_for_each(partner, &self->mld->links, struct hostapd_data, link)
#endif /* CONFIG_QCN_EXTN */

#else /* CONFIG_IEEE80211BE */

static inline bool hostapd_mld_is_first_bss(struct hostapd_data *hapd)
{
	return true;
}

#define for_each_mld_link(partner, self) \
	if (false)

#ifdef CONFIG_QCN_EXTN
#define for_each_mld_link_include_repurposed(partner, self) \
	if (false)
#endif /* CONFIG_QCN_EXTN */

#endif /* CONFIG_IEEE80211BE */

#define SP_AP_AND_CLIENT_POWER_DIFF            6
#define CONV_20MHZ_EIRP_TO_PSD_IN_DBM         13
#define SP_AP_AND_CLIENT_POWER_DIFF_IN_SCALE 600
#define TPE_NUM_POWER_SUPP_IN_11BE             5
#define TPE_NUM_EIRP_POWER_EXT_SUPPORTED       1
#define MAX_NUM_20_MHZ_IN_CURR_BW             16
#define EIRP_PWR_SCALE                       100
#define PSD_SCALE                            100
#define CHAN_MIN_TX_POWER                    -64
#define CHAN_MAX_TX_POWER                     63
#define CHAN_MIN_TWICE_TX_POWER              (CHAN_MIN_TX_POWER * 2)
#define CHAN_MAX_TWICE_TX_POWER              ((CHAN_MAX_TX_POWER * 2) + 1)

#define MAX_EIRP_THRESHOLD      33  /* A value above Max LPI and below Max SP */

#define NUM_20PP_PUNC_80MHZ      4
#define NUM_20PP_PUNC_160MHZ     8
#define NUM_40PP_PUNC_160MHZ     4
#define NUM_40PP_PUNC_320MHZ     8
#define NUM_80PP_PUNC_320MHZ     4
#define NUM_40P80PP_PUNC_320MHZ 12

/**
 * hostapd_iface_get_6ghz_chan_list() - Find the 6 GHz channel data for the given freq
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @pwr_type: Power type
 * @num_channels_6ghz: Output pointer to number of 6 GHz channels
 * @chan_idx: Output pointer to channel index for the given frequency
 *
 * Return: Pointer to hostapd_channel_data
 */
struct hostapd_channel_data *
hostapd_iface_get_6ghz_chan_list(struct hostapd_iface *iface,
				 u16 freq, u8 pwr_type,
				 u8 *num_channels_6ghz, u8 *chan_idx);

/**
 * hostapd_get_eirp_pwr() - Get eirp power based on channel input
 *
 * This API calculats the EIRP based on the AP power type or client power mode.
 * The callers are supposed to set both ap_power_type and client power type for
 * querying AFC based SP Tx powers for a particular client type. For other
 * cases, either of the two can be set.
 *
 * @iface: Pointer to iface
 * @freq: Primary Frequency in MHz
 * @cen_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @client_type: Client power type
 *
 * Return: EIRP power
 */
s16 hostapd_get_eirp_pwr(struct hostapd_iface *iface, u16 freq, u16 center_freq,
			 u16 bw, u16 in_punc_pattern, u8 ap_pwr_type,
			 bool is_client_lookup, u8 client_type,
			 bool is_twice_pwr);

/**
 * hostapd_get_chan_width_from_oper_chan_width() - Get the channel width
 * @iconf: Pointer to hostapd_config
 *
 * Return: Channel width enum value for the given config's operating ch width.
 */
enum chan_width
hostapd_get_chan_width_from_oper_chan_width(struct hostapd_config *iconf);

/**
 * hostapd_allow_6ghz_dynamic_puncture() - Check if dynamic puncturing is allowed
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @pwr_type: 6 GHz Power type.
 *
 * Return: true if dynamic puncturing is allowed, false otherwise.
 */
bool hostapd_allow_6ghz_dynamic_puncture(struct hostapd_iface *iface, u16 freq,
					 u8 pwr_type);

/**
 * hostapd_apply_6ghz_dynamic_puncturing() - Apply dynamic puncturing for 6 GHz
 * @iface: Pointer to hostapd_iface
 *
 * This function applies dynamic puncturing for 6 GHz channels in SP power mode.
 * It updates the puncturing pattern which gives the highest EIRP
 * for the given channel parameters.
 *
 * Return: None
 */
void hostapd_apply_6ghz_dynamic_puncturing(struct hostapd_iface *iface);

/**
 * hostapd_get_6ghz_best_pp() - Get the best 6 GHz Puncturing Pattern
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @pp: Output pointer to the best Puncturing Pattern
 * @is_bpm_enabled: Flag to indicate if the best power mode is enabled
 *
 * Return: 0 on success, -1 on failure.
 */
s8 hostapd_get_6ghz_best_pp(struct hostapd_iface *iface, u16 freq,
			    u16 center_freq, u16 bw, u16 *pp,
			    bool is_bpm_enabled);

/**
 * hostapd_get_best_ap_6ghz_power_mode() - Get the best 6 GHz AP power mode
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 *
 * Return: Best AP power mode for the given channel parameters.
 */
u8
hostapd_get_best_ap_6ghz_power_mode(struct hostapd_iface *iface,
				    u16 freq, u16 center_freq,
				    u16 bw,
				    u16 in_punc_pattern);

/**
 * hostapd_get_best_ap_6ghz_power_mode_for_iface() - Get the best 6 GHz power mode
 * @iface: Pointer to hostapd_iface
 *
 * Return: Best AP power mode for the iface's channel parameters
 */
u8
hostapd_get_best_ap_6ghz_power_mode_for_iface(struct hostapd_iface *iface);

/**
 * hostapd_get_bonded_chan_center_freq() - Get the bonded channel center freq
 *
 * This API returns the center frequency of the bonded channel based on the
 * frequency, bandwidth, 320 MHz BW center frequency and secondary channel offset.
 *
 * @freq: Frequency in MHz
 * @bw: Bandwidth in MHz
 * @cen320_freq: Center frequency for 320 MHz
 * @sec_chan_offset: Secondary channel offset
 *
 * Return: Center frequency in MHz for the bonded channel or 0 if not found.
 */
u16
hostapd_get_bonded_chan_center_freq(u16 freq, u16 bw, u16 cen320_freq,
				    s8 sec_chan_offset);

/**
 * hostapd_switch_power_mode() - Switch the power mode of the AP interface.
 *
 * This function switches the power mode of the AP interface and stores
 * the target power mode in the iface structure.
 *
 * @hapd: Pointer to hostapd_data
 *
 * Return: 0 on success, -1 on failure.
 */
int hostapd_switch_power_mode(struct hostapd_data *hapd);

/**
 * enum hostapd_afc_power_sync_result - AFC power sync result
 * @HOSTAPD_AFC_PWR_SYNC_NOOP: No action required. iface is not 6 GHz,
 *   current mode is not SP, or fallback target equals current mode.
 * @HOSTAPD_AFC_PWR_SYNC_UPDATED: Power mode CSA successfully initiated.
 * @HOSTAPD_AFC_PWR_SYNC_DEFERRED: Fallback blocked by a transient condition
 *   (CSA already in progress or a prior power-mode switch is still pending).
 *   For hostapd_force_afc_non_sp_power_mode(), is_afc_repeater_power_sync_pending
 *   is set by the function itself. For hostapd_sync_current_afc_power_mode(),
 *   the pending-repeater-sync caller re-arms it on receiving DEFERRED.
 *   Retry fires on the next REGDOM_SET_BY_DRIVER event. Valid only for
 *   connected-repeater contexts which have a guaranteed retry path.
 *   Root AP + BPM disabled callers must treat this as NO_IR (fail-safe).
 * @HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT: Current mode is SP but no valid
 *   non-SP mode exists for the current channel/BW/puncture tuple.
 * @HOSTAPD_AFC_PWR_SYNC_ERROR: Hard internal failure (iface/bss NULL,
 *   hw_features, select_hw_mode, or driver switch failed). No retry armed.
 */
enum hostapd_afc_power_sync_result {
	HOSTAPD_AFC_PWR_SYNC_NOOP,
	HOSTAPD_AFC_PWR_SYNC_UPDATED,
	HOSTAPD_AFC_PWR_SYNC_DEFERRED,
	HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT,
	HOSTAPD_AFC_PWR_SYNC_ERROR,
};

/**
 * hostapd_sync_current_afc_power_mode() - Sync AFC power mode on current tuple
 * @iface: Pointer to hostapd interface data
 * @ignore_best_mode_config: Ignore the best-power-mode knob and evaluate the
 * current channel tuple unconditionally
 *
 * Re-evaluate the best 6 GHz AP power mode for the currently operating
 * channel after fresh AFC data is available. This helper is intentionally
 * limited to power-mode-only updates on the existing operating tuple.
 *
 * Return:
 * * %HOSTAPD_AFC_PWR_SYNC_NOOP when no update is needed
 * * %HOSTAPD_AFC_PWR_SYNC_UPDATED when a power update is started
 * * %HOSTAPD_AFC_PWR_SYNC_DEFERRED when a CSA or pending power switch
 *   prevents evaluation; the pending-repeater-sync caller re-arms the
 *   retry flag
 * * %HOSTAPD_AFC_PWR_SYNC_INVALID_CURRENT when the current tuple cannot
 *   operate in the current power mode
 * * %HOSTAPD_AFC_PWR_SYNC_ERROR on internal failure
 */
#ifdef HOSTAPD
enum hostapd_afc_power_sync_result
hostapd_sync_current_afc_power_mode(struct hostapd_iface *iface,
				    bool ignore_best_mode_config);
enum hostapd_afc_power_sync_result
hostapd_force_afc_non_sp_power_mode(struct hostapd_iface *iface);
#else
static inline enum hostapd_afc_power_sync_result
hostapd_sync_current_afc_power_mode(struct hostapd_iface *iface,
				    bool ignore_best_mode_config)
{
	return HOSTAPD_AFC_PWR_SYNC_NOOP;
}

static inline enum hostapd_afc_power_sync_result
hostapd_force_afc_non_sp_power_mode(struct hostapd_iface *iface)
{
	return HOSTAPD_AFC_PWR_SYNC_NOOP;
}
#endif

/**
 * hostapd_iface_has_connected_backhaul_sta() - Check backhaul STA state
 * @iface: Pointer to hostapd interface data
 *
 * Check whether the interface is operating as a repeater with an associated
 * backhaul STA. Connected repeaters keep the current channel on AFC power
 * update and perform only current-channel power sync.
 *
 * Return: true when a backhaul STA is connected, false otherwise.
 */
#ifdef HOSTAPD
bool hostapd_iface_has_connected_backhaul_sta(struct hostapd_iface *iface);
#else
static inline bool
hostapd_iface_has_connected_backhaul_sta(struct hostapd_iface *iface)
{
	return false;
}
#endif

/**
 * hostapd_disconnect_backhaul_sta() - Disconnect associated backhaul STA
 * @iface: Pointer to hostapd interface data
 *
 * Resolve the backhaul STA interface name through ubus and issue DISCONNECT
 * through the wpa_supplicant control interface. This is used when the current
 * 6 GHz channel tuple is no longer valid after AFC data is updated.
 *
 * Return: 0 on success, non-zero on failure.
 */
#ifdef HOSTAPD
int hostapd_disconnect_backhaul_sta(struct hostapd_iface *iface);
#else
static inline int hostapd_disconnect_backhaul_sta(struct hostapd_iface *iface)
{
	return -1;
}
#endif

u16 hostapd_get_punct_bitmap(struct hostapd_data *hapd);

void hostapd_get_oper_chan_info_of_bss(struct hostapd_data *hapd,
				       enum oper_chan_width *width,
				       u8 *seg0, u8 *seg1);

u8 hostapd_get_oper_class_of_bss(struct hostapd_data *hapd);

enum oper_chan_width
hostapd_get_oper_chan_width_of_bss(struct hostapd_data *hapd);

bool hostapd_is_usable_punct_bitmap(struct hostapd_iface *iface);
void hostapd_gen_per_sta_profiles(struct hostapd_data *hapd);
size_t hostapd_eid_eht_ml_reconfig_len(struct hostapd_data *hapd);
u8 * hostapd_eid_eht_reconf_ml(struct hostapd_data *hapd, u8 *eid);
/**
 * hostapd_remove_bss() - Remove a BSS from the AP interface
 *
 * This function removes the BSS identified by the given index.
 * If the first BSS is removed while the interface is still in a
 * pre-beacon state (e.g., ACS, DFS, HT scan, or country update),
 * the driver context may not yet be fully initialized.
 * In such cases, the successor BSS (BSS #1) must be minimally
 * initialized. If this initialization fails, the entire interface
 * is removed.
 *
 * @iface: Pointer to hostapd_iface
 * @idx: Index of the BSS
 *
 * Return: 0 on successful BSS removal,
 * 	   1 if the BSS removal results in removing the entire
 * 	   interface (caller should not use the iface or BSS),
 * 	   -1 on failure.
 */
int hostapd_remove_bss(struct hostapd_iface *iface, unsigned int idx);
void hostapd_refresh_other_iface_beacons(struct hostapd_iface *hapd_iface);
void hostapd_refresh_all_iface_beacons(struct hostapd_iface *hapd_iface);

static inline bool ap_pmf_enabled(struct hostapd_bss_config *conf)
{
	return conf->ieee80211w != NO_MGMT_FRAME_PROTECTION ||
		conf->rsn_override_mfp != NO_MGMT_FRAME_PROTECTION ||
		conf->rsn_override_mfp_2 != NO_MGMT_FRAME_PROTECTION;
}

enum oper_chan_width
hostapd_chan_width_from_freq_params(struct hostapd_freq_params *freq_params);

struct hostapd_data *
hostapd_get_mbssid_bss_by_idx(struct hostapd_data *hapd, size_t idx);

struct hostapd_data *
hostapd_get_multi_group_bss(struct hostapd_multi_mbssid_group *group,
                           int bss_idx);
u8 hostapd_max_bssid_indicator(struct hostapd_data *hapd);
void hostapd_free_mbssid_idx(struct hostapd_data *hapd);
void hostapd_multi_mbssid_remove_bss(struct hostapd_data *hapd);
void hostapd_mld_ref_dec(struct hostapd_mld *mld);
void hostapd_interface_update_fils_ubpr(struct hostapd_iface *iface,
					bool iface_enabled);
/**
 * hostapd_set_no_ir_state - Set the NO_IR state for the interface
 * and bring down the corresponding 6 GHz link.
 * @iface: Pointer to hostapd interface data
 *
 * Return: None
 */
#ifdef HOSTAPD
void hostapd_set_no_ir_state(struct hostapd_iface *iface);
#else
static inline void
hostapd_set_no_ir_state(struct hostapd_iface *iface)
{
	/* No-op if hostapd is not defined */
}
#endif
void hostapd_no_ir_cleanup(struct hostapd_data *bss);

bool hostapd_is_6ghz_chan_txable(const struct hostapd_channel_data *c);
/**
 * hostapd_handle_afc_channel_change - Handle retail AFC channel change.
 *
 * Following are the steps:
 * 1. Fetch the updated channel list from the kernel using
 * NL8011_WIPHY_REG_CHANGE event.
 * 2. Randomly select a channel from the updated channel list.
 * 3. Apply best power mode on the selected channel and then set the
 * channel to the driver using drv_switch_channel.
 * 4. If channel change fails, move hostapd to NO_IR state.
 *
 * @iface: Pointer to hostapd interface data
 *
 * Returns: 0 on success, -1 on failure.
 */
#ifdef HOSTAPD
int hostapd_handle_afc_channel_change(struct hostapd_iface *iface);
#else
static inline int
hostapd_handle_afc_channel_change(struct hostapd_iface *iface)
{
	/* No-op if hostapd is not defined */
	return 0;
}
#endif

/**
 * afc_channel_change_timeout - Timeout handler for AFC channel change
 *
 * This function is called when the timeout for AFC channel change occurs.
 * After AFC response is received by the hostapd, it waits for
 * NL8011_WIPHY_REG_CHANGE event to get the updated channel list. If this
 * event is not received within the timeout period, this function
 * is invoked to set the hostapd state to no IR and clean up the AFC data.
 *
 * @eloop_ctx: Pointer to hostapd_iface structure
 * @timeout_ctx: Pointer to timeout context (not used)
 */
#ifdef HOSTAPD
void
afc_channel_change_timeout(void *eloop_ctx, void *timeout_ctx);
#else
static inline void
afc_channel_change_timeout(void *eloop_ctx, void *timeout_ctx)
{
	/* No-op if hostapd is not defined */
}
#endif
#ifdef CONFIG_IEEE80211BE
int hostapd_epcs_handle_and_send_action_frame(struct hostapd_data *hapd,
					      struct wlan_epcs_info *epcs_info,
					      struct sta_info *sta,
					      bool is_rx_frame);

void hostapd_handle_epcs_action(struct hostapd_data *hapd,
				const u8 *buf,
				size_t len);

void hostapd_get_epcs_capab(struct hostapd_data *hapd, struct sta_info *sta);

int hostapd_epcs_handle_cli(struct hostapd_data *hapd, char *pos,
			    char *buf, size_t buflen);
int hostapd_ctrl_iface_negotiated_ttlm_capabilities(struct hostapd_data *hapd,
						    char *buf, size_t buflen);
int hostapd_ctrl_iface_negotiated_ttlm_config(struct hostapd_data *hapd, const char *cmd,
					      char *buf, size_t buflen);
#endif /* CONFIG_IEEE80211BE */
bool
hostapd_verify_action_frame_has_min_length(const struct ieee80211_mgmt *mgmt,
					   size_t len);

/**
 * hostapd_find_eirp_in_afc_chan_obj() - Get AFC eirp power
 *
 * This API get the SP EIRP power from the AFC channel object based on the
 * channel center frequency and operating class.
 *
 * @chan_obj: Pointer to chan_obj
 * @freq: Frequency in MHz
 * @center_freq: Band center Frequency
 * @op_class: Operating class
 * @afc_eirp: Output pointer to AFC EIRP power
 *
 * Return: 0 on success, -1 on failure
 */
int
hostapd_find_eirp_in_afc_chan_obj(struct afc_chan_obj *chan_obj, u16 freq,
				  u16 center_freq, u8 op_class, s16 *afc_eirp);

/**
 * hostapd_reg_get_psd_from_chan_list() - Get the regulatory PSD for the freq
 * @iface: Pointer to hostapd_iface
 * @freq: Frequency in MHz
 * @center_freq: Band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Puncturing pattern
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @is_client_lookup: Whether the lookup is for client
 * @is_twice_pwr: Flag to indicate twice power
 * @psd_pwr: Output pointer to PSD power
 *
 * Return: 0 on success, -1 on failure
 */
int
hostapd_reg_get_psd_from_chan_list(struct hostapd_iface *iface, u16 freq,
				   u16 center_freq, u16 bw, u16 in_punc_pattern,
				   u8 ap_pwr_type, u8 client_type,
				   bool is_client_lookup, bool is_twice_pwr,
				   s16 *psd_pwr);

/**
 * hostapd_get_valid_puncture_pattern_arr() - Get the valid puncture pattern array
 * @bw: Bandwidth in MHz
 * @num_pp: Output pointer to store the number of valid puncture patterns
 * @pp_mask: Output pointer to store the puncture pattern mask for the given bandwidth
 *
 * Return: Pointer to the valid puncture pattern array or NULL if not found
 */
const u16 *
hostapd_get_valid_puncture_pattern_arr(u16 bw, u16 *num_pp, u16 *pp_mask);

/*
 * hostapd_afc_handle_cli - This CLI handler to set or get the afc channel
 * selection config
 *
 * @hapd: Pointer to hostapd data structure
 * @pos: Starting position of the buffer
 * @buf: Data buffer
 * @buflen: Data buffer length
 *
 * Return: Response length
 */
#ifdef CONFIG_QCN_EXTN
int hostapd_afc_handle_cli(struct hostapd_data *hapd, char *pos,
			   char *buf, size_t buflen);
#endif /* CONFIG_QCN_EXTN */

/**
 * hostapd_get_tpe_11ax_count() - Validate the input Tx power count
 *
 * This API checks if the input Tx power count and Interpretation are valid
 *
 * @tx_pwr_intrpn: Tx power interpretation
 * @tx_pwr_count: Tx power count
 *
 * Return: Total Tx power count for the given interpretation
 */
int
hostapd_get_tpe_11ax_count(u8 tx_pwr_intrpn, u8 tx_pwr_count);

/**
 * hostapd_validate_chan_bw_in_pwr_mode() - Validate the input channel parameters
 *
 * This API checks if the input channel parameters are valid in the given
 * power mode.
 *
 * @iface: Pointer to hostapd interface data
 * @freq: Frequency of the channel
 * @center_freq: Center frequency of the channel
 * @bw: Bandwidth of the channel
 * @pp: Puncturing bitmap
 * @pwr_type: Power type
 *
 * Return: true if valid, false otherwise
 */
bool
hostapd_validate_chan_bw_in_pwr_mode(struct hostapd_iface *iface, u16 freq,
				     u16 center_freq, u16 bw, u16 pp,
				     u8 pwr_type);

/*
 * hostapd_validate_non_6ghz_chan_bw - Validate a 2.4/5 GHz channel+BW.
 *
 * This API checks if the input channel parameters are valid.
 *
 * Punctured sub-channels (pp bitmap) are skipped.
 * Bonded center is validated via hostapd_get_bonded_chan_center_freq() for
 * BW > 20 MHz.
 *
 * Return: true if all non-punctured sub-channels are usable.
 */
bool
hostapd_validate_non_6ghz_chan_bw(struct hostapd_iface *iface, u16 freq,
				  u16 center_freq, u16 bw, u16 pp);

void hostapd_remove_non_tx_bsses(struct hostapd_data *tx_bss);

static inline bool
hostapd_is_ht_enabled(struct hostapd_data *hapd)
{
	return (hapd->iconf->ieee80211n && !hapd->conf->disable_11n);
}


static inline bool
hostapd_is_vht_enabled(struct hostapd_data *hapd)
{
	return (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac);
}


static inline bool
hostapd_is_he_enabled(struct hostapd_data *hapd)
{
#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11ax_extn(hapd->conf))
		return false;
#endif /* CONFIG_QCN_EXTN */
	return (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax);
}


static inline bool
hostapd_is_eht_enabled(struct hostapd_data *hapd)
{
#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return false;
#endif /* CONFIG_QCN_EXTN */
	return (hapd->iconf->ieee80211be && !hapd->conf->disable_11be);
}

static inline bool
hostapd_is_uhr_enabled(struct hostapd_data *hapd)
{
	return (hapd->iconf->ieee80211bn && !hapd->conf->disable_11bn);
}

static inline u8
hostapd_get_ecu_info(const struct rx_ecu_param *ecu_param)
{
	u8 ecu_info = ecu_param->ebpcc & UHR_ECU_INFO_ENHANCED_BPCC_MASK;

	if (ecu_param->critical_update)
		ecu_info |= (UHR_CRITICAL_UPDATE_TYPE_UHR_MODE <<
			     UHR_ECU_INFO_CRITICAL_UPDATE_TYPE_SHIFT) &
			    UHR_ECU_INFO_CRITICAL_UPDATE_TYPE_MASK;

	/*
	 * B7: Enhanced All Updates Included — set only while the UHR
	 * Parameters Update element is actually present in the frame
	 * (advance- and post-notification phases).  Using a dedicated
	 * flag rather than critical_update avoids incorrectly setting
	 * B7 during UHR_ECU_UPDATE_IND_IN_TIM when the element has
	 * already been dropped from the frame.
	 */
	if (ecu_param->all_updates_included)
		ecu_info |= UHR_ECU_INFO_ENHANCED_ALL_UPDATES_INCLUDED;

	return ecu_info;
}

/**
 * Vendor element format
 * ID (1 byte), Length (1 byte), OUI (3 bytes), Data (at least 1 byte)
 */
#define MIN_VENDOR_ELEM_LEN 6

/**
 * hostapd_update_vendor_elements - This is to handle user configured vendor
 *                                  elements addition and removal
 * @hapd: Pointer to hostapd data structure
 * @conf: Pointer to hostapd bss config structure
 * @data: Pointer to wpabuf structure and its only filled in soft ap case
 * @cmd: User command add or remove
 * @val: User configured data
 * @is_bcn_update_needed: Flag to determine whether beacon update should be sent
 *
 * Return: 0 for success -1 for failure
 */
int
hostapd_handle_vendor_elements_update(struct hostapd_data *hapd,
				      struct hostapd_bss_config *conf, struct wpabuf *data,
				      char *cmd, char *val, bool is_bcn_update_needed);

int convert_chwidth_to_20MHz_nchans(enum chan_width chan_width);

int find_6g_enabled_chans(struct hostapd_iface *iface,
			  int chan_width,
			  struct hostapd_channel_data **chandef_list,
			  struct hostapd_hw_modes *mode,
			  struct hostapd_channel_data **chan_6ghz,
			  int n_chans, int power_type);

#ifdef CONFIG_QCN_EXTN
/**
 * configured_fixed_chan_to_freq_helper - Helper to convert configured
 * channel to frequency
 * @iface: Pointer to hostapd_iface structure
 *
 * This is a wrapper function for configured_fixed_chan_to_freq() to support
 * vendor-specific extensions (QCN_EXTN). It determines the operating frequency
 * based on the configured channel.
 *
 * Returns: 0 on success, -1 on failure
 */
int configured_fixed_chan_to_freq_helper(struct hostapd_iface *iface);
#endif

enum hostapd_bss_category {
	CAT_ALL_BSS,      /* All started BSSes */
	CAT_TX_BSS,       /* TX BSS only */
	CAT_NON_TX_BSS,   /* Non‑TX BSSes only */

	CAT_MAX
};

/**
 * hostapd_is_bss_in_category - Decide whether a BSS should be disabled
 * @hapd: Pointer to hostapd BSS context
 * @cat: BSS disable category selector
 *
 * Return: %true if the BSS should be disabled for the given category,
 *         %false otherwise.
 */
bool hostapd_is_bss_in_category(struct hostapd_data *hapd,
				enum hostapd_bss_category cat);

/**
 * dfs_get_subchannel_count() - Get the total subchannel count for a given bandwidth
 * @bandwidth: Channel width enum value
 *
 * Return: Number of 20 MHz subchannels in the given bandwidth, or 0 if invalid.
 */
int dfs_get_subchannel_count(int bandwidth);

/**
 * hostapd_update_puncture_source() - Update puncture source per 20 MHz
 * subchannel
 * @iface: Pointer to hostapd interface
 * @center_freq: Center frequency in MHz
 * @bandwidth: Channel width enum value
 * @new_punct_bitmap: New puncture bitmap to apply
 * @source: Puncture source being updated
 *
 * Update the per-channel puncture source for the given puncture bitmap.
 *
 * Return: 0 on success, -1 on validation failure.
 */
int dfs_update_puncture_source(struct hostapd_iface *iface,
			       u16 center_freq, int bandwidth,
			       u16 new_punct_bitmap,
			       enum dfs_chan_puncture_source source);

/**
 * dfs_is_puncture_bitmap_bit_user() - Check whether a puncture bit is
 *                                     user-punctured
 * @iface: Pointer to hostapd interface
 * @bit: 20 MHz subchannel bit position
 *
 * Return: 1 if the bit maps to a user-punctured channel, 0 otherwise.
 */
int dfs_is_puncture_bitmap_bit_user(struct hostapd_iface *iface,
				    int bit);

/**
 * dfs_is_puncture_bitmap_bit_radar() - Check whether a puncture bit is
 *                                      radar-punctured
 * @iface: Pointer to hostapd interface
 * @bit: 20 MHz subchannel bit position
 *
 * Return: 1 if the bit maps to a radar-punctured channel, 0 otherwise.
 */
int dfs_is_puncture_bitmap_bit_radar(struct hostapd_iface *iface,
				     int bit);

/**
 * dfs_filter_punc_bitmap_by_src() - Filter bitmap by puncture source
 * @iface: Pointer to hostapd interface
 * @punct_bitmap: Puncture bitmap to filter
 * @source: Puncture source to match
 *
 * Return: Bitmap containing only bits that map to channels with @source.
 */
u16 dfs_filter_punc_bitmap_by_src(struct hostapd_iface *iface,
				  u16 punct_bitmap,
				  enum dfs_chan_puncture_source source);

/**
 * dfs_reset_punc_bitmap_src() - Reset puncture source for bitmap
 * @iface: Pointer to hostapd interface
 * @punct_bitmap: Puncture bitmap whose bits should be reset
 *
 * Reset the puncture source of channels mapped by set bits in @punct_bitmap
 * to %HOSTAPD_CHAN_PUNC_NONE.
 */
void dfs_reset_punc_bitmap_src(struct hostapd_iface *iface,
			       u16 punct_bitmap);
int hostapd_setup_monitor_iface(struct hostapd_iface *iface);
int hostapd_update_monitor_channel(struct hostapd_data *hapd,
				   const struct hostapd_freq_params *freq_params);
void hostapd_clear_old_bss(struct hostapd_data *bss);
struct hostapd_data * hostapd_mbssid_get_bss(struct hostapd_data *hapd, size_t i);

bool hostapd_is_lpi_chans_available(struct hostapd_iface *iface);
bool hostapd_is_indoor_sp_pwr_mode(struct hostapd_data *hapd);
#ifdef CONFIG_IEEE80211AX
/**
 * hostapd_set_current_6ghz_pwr_type - Set 6 GHz power mode when BPM is disabled
 * @iface: Pointer to hostapd interface
 * @power_mode: Output power mode to set
 *
 * Sets @power_mode to the configured 6 GHz regulatory power type.
 * Logs an error if SP mode is requested but no AFC power event has been
 * received - this should not happen on a running interface but the value
 * is passed to the driver for final validation.
 */
static inline void
hostapd_set_current_6ghz_pwr_type(struct hostapd_iface *iface,
				  int *power_mode)
{
	u8 cur_pwr_type = iface->conf->he_6ghz_reg_pwr_type;

	if (he_reg_is_sp(cur_pwr_type) && !iface->is_afc_power_event_received) {
		/* Should Not Happen: SP mode without AFC on a running iface */
		wpa_printf(MSG_ERROR,
			   "%s: SP power mode configured but AFC not received, passing to driver",
			   __func__);
	}
	*power_mode = cur_pwr_type;
}
#endif /* CONFIG_IEEE80211AX */

#endif /* HOSTAPD_H */
