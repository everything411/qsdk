/*
 * hostapd / Station table
 * Copyright (c) 2002-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef STA_INFO_H
#define STA_INFO_H

#include "common/defs.h"
#include "list.h"
#include "vlan.h"
#include "common/wpa_common.h"
#include "common/ieee802_11_defs.h"
#include "common/sae.h"
#include "crypto/sha384.h"
#include "pasn/pasn_common.h"
#include "hostapd.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#include "ttlm.h"
#ifdef CONFIG_IEEE80211AX
#include "robust_av.h"
#endif

/* STA flags */
#define WLAN_STA_AUTH BIT(0)
#define WLAN_STA_ASSOC BIT(1)
#define WLAN_STA_SPP_AMSDU BIT(2)
#define WLAN_STA_CFP BIT(3)
#define WLAN_STA_UHR BIT(4)
#define WLAN_STA_AUTHORIZED BIT(5)
#define WLAN_STA_PENDING_POLL BIT(6) /* pending activity poll not ACKed */
#define WLAN_STA_SHORT_PREAMBLE BIT(7)
#define WLAN_STA_PREAUTH BIT(8)
#define WLAN_STA_WMM BIT(9)
#define WLAN_STA_MFP BIT(10)
#define WLAN_STA_HT BIT(11)
#define WLAN_STA_WPS BIT(12)
#define WLAN_STA_MAYBE_WPS BIT(13)
#define WLAN_STA_WDS BIT(14)
#define WLAN_STA_ASSOC_REQ_OK BIT(15)
#define WLAN_STA_WPS2 BIT(16)
#define WLAN_STA_GAS BIT(17)
#define WLAN_STA_VHT BIT(18)
#define WLAN_STA_WNM_SLEEP_MODE BIT(19)
#define WLAN_STA_VHT_OPMODE_ENABLED BIT(20)
#define WLAN_STA_VENDOR_VHT BIT(21)
#define WLAN_STA_PENDING_FILS_ERP BIT(22)
#define WLAN_STA_MULTI_AP BIT(23)
#define WLAN_STA_HE BIT(24)
#define WLAN_STA_6GHZ BIT(25)
#define WLAN_STA_PENDING_PASN_FILS_ERP BIT(26)
#define WLAN_STA_EHT BIT(27)
#define WLAN_STA_FT_AUTH BIT(28)
#define WLAN_STA_PENDING_DISASSOC_CB BIT(29)
#define WLAN_STA_PENDING_DEAUTH_CB BIT(30)
#define WLAN_STA_NONERP BIT(31)

/* STA flags ext */
#define WLAN_STA_SMD BIT(0)

/* wired mac authentication bypass sta, non-802.1x capable */
#define WIRED_STA_MAB BIT(2)

/* Maximum number of supported rates (from both Supported Rates and Extended
 * Supported Rates IEs). */
#define WLAN_SUPP_RATES_MAX 32
#define WLAN_SUPP_HT_RATES_MAX 77

#define WLAN_VHT_MCS_NSS 8
#define WLAN_VHT_EACH_NSS 2
#define WLAN_VHT_MCS 2

#define WLAN_ASSOC_REQ_MIN_INTERVAL_MS 150

struct hostapd_data;

struct mbo_non_pref_chan_info {
	struct mbo_non_pref_chan_info *next;
	u8 op_class;
	u8 pref;
	u8 reason_code;
	u8 num_channels;
	u8 channels[];
};

struct pending_eapol_rx {
	struct wpabuf *buf;
	struct os_reltime rx_time;
	enum frame_encryption encrypted;
};

struct eap_over_auth_data {
	int akm;
	int cipher;
	u16 group;
	u16 auth_transaction;
	u8 snonce[WPA_NONCE_LEN];
	u8 anonce[WPA_NONCE_LEN];
	u8 *rsnxe;
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	struct wpa_ptk ptk;
	size_t rsnxe_len;
	struct crypto_ecdh *ecdh;
	struct wpabuf *dhss;
	bool add_mic;
	u8 epp_pmkid_cur[PMKID_LEN];
	u8 epp_pmkid_next[PMKID_LEN];
};

#define EHT_ML_MAX_STA_PROF_LEN 1024
struct mld_info {
	bool mld_sta;

	struct ml_common_info {
		u8 mld_addr[ETH_ALEN];
		u16 medium_sync_delay;
		u16 eml_capa;
		u16 mld_capa;
	} common_info;

	struct mld_link_info links[MAX_NUM_MLD_LINKS];
	struct mld_peer_epcs_info epcs;
#ifdef CONFIG_IEEE80211BE
	struct tid_to_link_map_info tid_map_info;
#endif /* CONFIG_IEEE80211BE */
};

struct dscp_policy_state {
	size_t offset;
	u8 last_dialog_token;
	bool pending_more;
};

#ifdef CONFIG_IEEE80211BN
/**
 * enum smd_ap_state - SMD roaming AP state machine
 *
 * Complete state machine for ST Prep and ST Execute phases.
 * ST Prep states (0-3) are used during preparation phase.
 * ST Execute states (4-8) are used during execution phase.
 */
enum smd_ap_state {
	/* ST Prep states (from V24) */
	SMD_AP_STATE_IDLE = 0,
	SMD_AP_STATE_ST_PREP_STARTED,
	SMD_AP_STATE_ST_PREP_IAP_PENDING,
	SMD_AP_STATE_ST_PREP_COMPLETE,

       /* ST Execute states (NEW in V25) */
       SMD_AP_STATE_ST_EXEC_STARTED,      /* ST Execute initiated */
       SMD_AP_STATE_ST_EXEC_IAP_PENDING,  /* Waiting for IAP RESPONSE */
       SMD_AP_STATE_ST_EXEC_COMPLETE,
       SMD_AP_STATE_DL_DRAIN_ACTIVE,      /* DL Drain timeout active */
       SMD_AP_STATE_TRANSITION_COMPLETE,  /* Transition complete */

       /* ST Execute via Target AP-MLD states */
       SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_REQ,   /* Current AP received CTX_REQUEST */
       SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_WAIT,  /* Current AP awaiting driver response */
       SMD_AP_STATE_ST_EXEC_VIA_TGT_CURR_CTX_RESP,  /* Current AP sent CTX_RESP */

       SMD_AP_STATE_ST_EXEC_VIA_TGT_STARTED,   /* CTX_REQUEST sent to Current AP */
       SMD_AP_STATE_ST_EXEC_VIA_TGT_COMPLETE,  /* CTX_RESPONSE received, exec done */
};

/**
 * enum tgt_smd_roam_state - SMD roaming state for non-AP STA
 *
 * complete state machine for SMD at Target AP for non-AP STA MLD
 */
enum tgt_smd_roam_state {
	SMD_STA_ST_NONE,
	SMD_STA_ST_PREP_DONE,
	SMD_STA_ST_EXEC_DONE,
};

/* Forward declaration for sta_info pointer */
struct sta_info;

/**
 * struct smd_roam_ap_info - SMD roaming AP information
 *
 * Tracks potential target APs for UHR Link Reconfiguration.
 * Linked list of APs that the STA can roam to.
 */
struct smd_roam_ap_info {
	struct smd_roam_ap_info *next;

	u8 ap_mld_addr[ETH_ALEN];
	u8 sta_addr[ETH_ALEN];

	/* Channel information */
	u8 op_class;
	u8 channel;
	enum smd_ap_state state;

	/* Last seen timestamp */
	struct os_reltime last_seen;

	/* UHR ST preparation timeout tracking */
	bool uhr_st_prep_timeout_occurred;
	bool uhr_st_prep_timer_ongoing;    /* true while ST prep timer is armed */
	struct os_reltime uhr_st_prep_start;
	u8 st_prep_link_id;                /* MLD link that owns the ST prep timer */
	struct hostapd_data *st_prep_hapd; /* hapd of the prep link (for clone cleanup) */

	/* ST Execute fields  */
	u32 dl_drain_duration_tu;          /* DL Drain duration in TU */
	struct os_reltime dl_drain_start;  /* DL Drain start time */
	struct sta_info *sta;              /* Back pointer to station */
	bool smd_ctx_valid;
	struct sta_smd_ctx_info *smd_ctx;  /* SMD context for this AP MLD transition */
	bool uhr_st_iap_timer_ongoing;
};


/* SMD Capabilities structure */
struct smd_caps {
	bool dl_data_fwd; /* DL Data Forwarding capability */
	u8 max_prep_target_apmlds; /* Max Number Of Prepared Target AP MLDs */
	bool smd_type; /* SMD Type field */
	bool ptk_mode; /* PTK Mode field */
};

/* SMD (Seamless Multiband Device) station information */
struct smd_info {
	bool smd_sta; /* Station supports SMD */
	bool roam_sta; /* Station created via ST Prep (IAP) at Target AP */
	u8 smd_identifier[ETH_ALEN]; /* SMD Identifier from STA */
	u8 smd_timeout; /* Preparation Timeout, units of 64 TUs */
	struct smd_caps caps; /* SMD capabilities */
	struct smd_roam_ap_info *ap_list;  /* List of potential target APs */
	int uhr_target_prep_timer; /* Target AP prep timer */
	u8 *tgt_prep_timer_ctx; /* heap-allocated sta_addr copy passed to eloop */
	struct hostapd_data *tgt_prep_timer_hapd; /* assoc-link hapd at timer registration */
	enum tgt_smd_roam_state state; /* non-AP STA state in Tgt AP */
	bool flag;
	u8 current_ap_mld_addr[ETH_ALEN]; /* Current AP MLD addr at time of ST Prep */
	u8 st_exec_dialog_token; /* Dialog token from STA's ST Execute frame */

	/* Pending async GET_SMD_CTX request (Via-TAP path).
	 * Set when uhr_handle_st_exec_req() fires get_smd_ctx and cleared
	 * when uhr_handle_get_smd_ctx_done() delivers the result. */
	struct smd_get_ctx_pending {
		bool active;
		u8 target_ap_mld_addr[ETH_ALEN];
		u8 iap_transaction_id;
	} get_ctx_pending;
};
#endif /* CONFIG_IEEE80211BN */

struct sta_info {
	struct sta_info *next; /* next entry in sta list */
	struct sta_info *hnext; /* next entry in hash table list */

#ifdef CONFIG_QCN_EXTN
        struct sta_info_extn sta_extn;
#endif /* CONFIG_QCN_EXTN */

	u8 addr[6];
	be32 ipaddr;
	struct dl_list ip6addr; /* list head for struct ip6addr */
	u16 aid; /* STA's unique AID (1 .. 2007) or 0 if not yet assigned */
	u16 wds_mld_uid; /* STA's vlan ifname unique id (1..2007 non-repurposed, >= 3001 repurposed) or 0 if not */
	u16 disconnect_reason_code; /* RADIUS server override */
	u32 flags; /* Bitfield of WLAN_STA_* */
	u16 capability;
	u16 listen_interval; /* or beacon_int for APs */
	u8 supported_rates[WLAN_SUPP_RATES_MAX];
	int supported_rates_len;
	u8 qosinfo; /* Valid when WLAN_STA_WMM is set */
	int ft_over_ds_saquery_status;
	u8 control_mic_pad;

#ifdef CONFIG_MESH
	enum mesh_plink_state plink_state;
	u16 peer_lid;
	u16 my_lid;
	u16 peer_aid;
	u16 mpm_close_reason;
	int mpm_retries;
	u8 my_nonce[WPA_NONCE_LEN];
	u8 peer_nonce[WPA_NONCE_LEN];
	u8 aek[32];	/* SHA256 digest length */
	u8 mtk[WPA_TK_MAX_LEN];
	size_t mtk_len;
	u8 mgtk_rsc[6];
	u8 mgtk_key_id;
	u8 mgtk[WPA_TK_MAX_LEN];
	size_t mgtk_len;
	u8 igtk_rsc[6];
	u8 igtk[WPA_TK_MAX_LEN];
	size_t igtk_len;
	u16 igtk_key_id;
	u8 sae_auth_retry;
#endif /* CONFIG_MESH */

	unsigned int nonerp_set:1;
	unsigned int no_short_slot_time_set:1;
	unsigned int no_short_preamble_set:1;
	unsigned int no_ht_gf_set:1;
	unsigned int no_ht_set:1;
	unsigned int ht40_intolerant_set:1;
	unsigned int ht_20mhz_set:1;
	unsigned int no_p2p_set:1;
	unsigned int qos_map_enabled:1;
	unsigned int hs20_deauth_requested:1;
	unsigned int hs20_deauth_on_ack:1;
	unsigned int session_timeout_set:1;
	unsigned int radius_das_match:1;
	unsigned int ecsa_supported:1;
	unsigned int added_unassoc:1;
	unsigned int pending_wds_enable:1;
	unsigned int power_capab:1;
	unsigned int agreed_to_steer:1;
	unsigned int hs20_t_c_filtering:1;
	unsigned int ft_over_ds:1;
	unsigned int external_dh_updated:1;
	unsigned int post_csa_sa_query:1;
	unsigned int sp_ie_validated:1;

	u16 auth_alg;

	enum {
		STA_NULLFUNC = 0, STA_DISASSOC, STA_DEAUTH, STA_REMOVE,
		STA_DISASSOC_FROM_CLI
	} timeout_next;

	u16 deauth_reason;
	u16 disassoc_reason;

	/* IEEE 802.1X related data */
	struct eapol_state_machine *eapol_sm;

	struct pending_eapol_rx *pending_eapol_rx;

	u64 acct_session_id;
	struct os_reltime acct_session_start;
	int acct_session_started;
	int acct_terminate_cause; /* Acct-Terminate-Cause */
	int acct_interim_interval; /* Acct-Interim-Interval */
	unsigned int acct_interim_errors;

	/* For extending 32-bit driver counters to 64-bit counters */
	u32 last_rx_bytes_hi;
	u32 last_rx_bytes_lo;
	u32 last_tx_bytes_hi;
	u32 last_tx_bytes_lo;

	u8 *challenge; /* IEEE 802.11 Shared Key Authentication Challenge */

	struct wpa_state_machine *wpa_sm;
	struct rsn_preauth_interface *preauth_iface;

	int vlan_id; /* 0: none, >0: VID */
	struct vlan_description *vlan_desc;
	int vlan_id_bound; /* updated by ap_sta_bind_vlan() */
	 /* PSKs from RADIUS authentication server */
	struct hostapd_sta_wpa_psk_short *psk;

	char *identity; /* User-Name from RADIUS */
	char *radius_cui; /* Chargeable-User-Identity from RADIUS */

	u32 last_rx_mgmt_rate;
	struct ieee80211_ht_capabilities *ht_capabilities;
	struct ieee80211_vht_capabilities *vht_capabilities;
	struct ieee80211_vht_operation *vht_operation;
	u8 vht_opmode;
	struct ieee80211_he_capabilities *he_capab;
	size_t he_capab_len;
	struct ieee80211_he_6ghz_band_cap *he_6ghz_capab;
	struct ieee80211_eht_capabilities *eht_capab;
	size_t eht_capab_len;
	struct ieee80211_uhr_capabilities *uhr_capab;
	size_t uhr_capab_len;

	 u8  skip_sa_query; /* 0: trigger sa query procedure
			     * 1: skip sa query procedure */
	int sa_query_count; /* number of pending SA Query requests;
			     * 0 = no SA Query in progress */
	int sa_query_timed_out;
	u8 *sa_query_trans_id; /* buffer of WLAN_SA_QUERY_TR_ID_LEN *
				* sa_query_count octets of pending SA Query
				* transaction identifiers */
	struct os_reltime sa_query_start;

#if defined(CONFIG_INTERWORKING) || defined(CONFIG_DPP)
#define GAS_DIALOG_MAX 8 /* Max concurrent dialog number */
	struct gas_dialog_info *gas_dialog;
	u8 gas_dialog_next;
#endif /* CONFIG_INTERWORKING || CONFIG_DPP */

	struct wpabuf *wps_ie; /* WPS IE from (Re)Association Request */
	struct wpabuf *p2p_ie; /* P2P IE from (Re)Association Request */
	struct wpabuf *hs20_ie; /* HS 2.0 IE from (Re)Association Request */
	/* Hotspot 2.0 Roaming Consortium from (Re)Association Request */
	struct wpabuf *roaming_consortium;
	char *t_c_url; /* HS 2.0 Terms and Conditions Server URL */
	struct wpabuf *hs20_deauth_req;
	char *hs20_session_info_url;
	int hs20_disassoc_timer;
#ifdef CONFIG_FST
	struct wpabuf *mb_ies; /* MB IEs from (Re)Association Request */
#endif /* CONFIG_FST */

	struct os_reltime connected_time;

#ifdef CONFIG_SAE
	struct sae_data *sae;
	unsigned int mesh_sae_pmksa_caching:1;
#endif /* CONFIG_SAE */

	/* valid only if session_timeout_set == 1 */
	struct os_reltime session_timeout;

	/* Timestamp of last received Association Request from this STA */
	struct os_reltime last_assoc_req_rx_time;

	/* Last Authentication/(Re)Association Request/Action frame sequence
	 * control */
	u16 last_seq_ctrl;
	/* Last Authentication/(Re)Association Request/Action frame subtype */
	u8 last_subtype;

#ifdef CONFIG_MBO
	u8 cell_capa; /* 0 = unknown (not an MBO STA); otherwise,
		       * enum mbo_cellular_capa values */
	struct mbo_non_pref_chan_info *non_pref_chan;
	int auth_rssi; /* Last Authentication frame RSSI */
#endif /* CONFIG_MBO */

	u8 *supp_op_classes; /* Supported Operating Classes element, if
			      * received, starting from the Length field */

	u8 rrm_enabled_capa[5];

	s8 min_tx_power;
	s8 max_tx_power;

#ifdef CONFIG_TAXONOMY
	struct wpabuf *probe_ie_taxonomy;
	struct wpabuf *assoc_ie_taxonomy;
#endif /* CONFIG_TAXONOMY */

#ifdef CONFIG_FILS
	u8 fils_snonce[NONCE_LEN];
	u8 fils_session[FILS_SESSION_LEN];
	u8 fils_erp_pmkid[PMKID_LEN];
	u8 *fils_pending_assoc_req;
	size_t fils_pending_assoc_req_len;
	unsigned int fils_pending_assoc_is_reassoc:1;
	unsigned int fils_dhcp_rapid_commit_proxy:1;
	unsigned int fils_erp_pmkid_set:1;
	unsigned int fils_drv_assoc_finish:1;
	struct wpabuf *fils_hlp_resp;
	struct wpabuf *hlp_dhcp_discover;
	void (*fils_pending_cb)(struct hostapd_data *hapd, struct sta_info *sta,
				u16 resp, struct wpabuf *data, int pub);
#ifdef CONFIG_FILS_SK_PFS
	struct crypto_ecdh *fils_ecdh;
#endif /* CONFIG_FILS_SK_PFS */
	struct wpabuf *fils_dh_ss;
	struct wpabuf *fils_g_sta;
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	u8 *owe_pmk;
	size_t owe_pmk_len;
	u8 *owe_pmkid;
	struct crypto_ecdh *owe_ecdh;
	u16 owe_group;
#endif /* CONFIG_OWE */

	u8 *ext_capability;
	char *ifname_wds; /* WDS ifname, if in use */

#ifdef CONFIG_DPP2
	struct dpp_pfs *dpp_pfs;
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	enum wpa_alg last_tk_alg;
	int last_tk_key_idx;
	u8 last_tk[WPA_TK_MAX_LEN];
	size_t last_tk_len;
	u8 *sae_postponed_commit;
	size_t sae_postponed_commit_len;
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_AIRTIME_POLICY
	unsigned int airtime_weight;
	unsigned int dyn_airtime_weight;
	struct os_reltime backlogged_until;
#endif /* CONFIG_AIRTIME_POLICY */
#ifdef CONFIG_ATF_OFFLOAD
        struct atf_peer atf_peer;
	struct dl_list atf_candidate_list;
#endif

#ifdef CONFIG_PASN
	struct pasn_data *pasn;
#endif /* CONFIG_PASN */

	/* Vendor Specific OUI from associated STA */
	u8 vendor_oui[3];

#ifdef CONFIG_IEEE80211BE
	struct mld_info mld_info;
	u8 mld_assoc_link_id;
	struct link_reconf_req_list *reconf_req;
	struct hostapd_sta_add_params *recfg_sta_add_params[MAX_NUM_MLD_LINKS];

	/* if receive auth request from partner link, when partner sta exist,
	 * response send without add the station in kernel.
	 */
	u8 unadded_sta;
	/*auth received existing authorized sta*/
	int mld_auth;
	u8 reply_addr[6];
#endif /* CONFIG_IEEE80211BE */
	u8 skip_kernel_delete;

	/* External plugin-provided tails to append to outgoing management frames */
	u8 *ext_auth_tail;
	size_t ext_auth_tail_len;
	u8 *ext_assoc_tail;
	size_t ext_assoc_tail_len;

	bool dscp_policy_capable;
	struct hostapd_dscp_policy **policies;
	u8 num_dscp_policies;
	u8 unsolicited_dialog_token;
	struct dscp_policy_state dscp_state;
	struct sta_info *sa_query_triggered_sta;
#ifdef CONFIG_IEEE80211BE
	u16 link_addr_conflict_bitmap; /* bitmap of partner link indices (bit k
					* set when link k's per-link address
					* conflicted with an existing MFP STA
					* during association; re-walk only those
					* links on SA Query timeout comeback) */
#endif
	bool dscp_reset;
	bool ft_re_add;
	u16 max_idle_period; /* if nonzero, the granted BSS max idle period in
			      * units of 1000 TUs */
#ifdef RDK_ONEWIFI
	u8 *assoc_req;
	size_t assoc_req_len;
#endif

#ifdef CONFIG_IEEE80211AX
	struct dl_list scs_req_desc; /* list of hostapd_scs_req_desc_data */
	u16 scs_session_count;
	struct hostapd_mscs_ctxt *mscs_ctxt;
	bool mscs_session_exists;
#endif

	u64 last_known_sta_id_timestamp;
	bool pending_drv_add;

	/**
	 * rssi_reject_timeout - Time when RSSI rejection expires
	 *
	 * If non-zero, indicates when a client rejected due to low RSSI
	 * may retry association. Used to implement rssi_reject_assoc_timeout.
	 */
	struct os_time rssi_reject_timeout;
	struct wpabuf *sae_pw_id;
	unsigned int sae_pw_id_counter;
#ifdef CONFIG_IEEE80211BN
	bool is_mapc_peer;
	struct mapc_parameters mapc_params;
#endif /* CONFIG_IEEE80211BN */

	u32 flags_ext;
#ifdef CONFIG_IEEE80211BN
	/* SMD information */
	struct smd_info smd_info;
	bool dl_sn_not_transferred;
	bool ul_sn_not_transferred;
#endif /* CONFIG_IEEE80211BN */
#ifdef CONFIG_ENC_ASSOC
	bool epp_sta; /* Indicates if the station is an EPP peer */
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_PMKSA_PRIVACY
	u8 snonce[NONCE_LEN]; /* SNonce to compute next PMKID if
			       * PMKID caching privacy is on */
	u8 anonce[NONCE_LEN]; /* ANonce to compute next PMKID if
			       * PMKID caching privacy is on */
	u8 epp_pmkid_next[PMKID_LEN];
#endif /* CONFIG_PMKSA_PRIVACY */

#ifdef CONFIG_IEEE8021X_AUTH
	struct eap_over_auth_data eap_auth_data;
#endif /* CONFIG_IEEE8021X_AUTH */
};


/* Default value for maximum station inactivity. After AP_MAX_INACTIVITY has
 * passed since last received frame from the station, a nullfunc data frame is
 * sent to the station. If this frame is not acknowledged and no other frames
 * have been received, the station will be disassociated after
 * AP_DISASSOC_DELAY seconds. Similarly, the station will be deauthenticated
 * after AP_DEAUTH_DELAY seconds has passed after disassociation. */
#define AP_MAX_INACTIVITY (5 * 60)
#define AP_DISASSOC_DELAY (3)
#define AP_DEAUTH_DELAY (1)
/* Number of seconds to keep STA entry with Authenticated flag after it has
 * been disassociated. */
#define AP_MAX_INACTIVITY_AFTER_DISASSOC (1 * 30)
/* Number of seconds to keep STA entry after it has been deauthenticated. */
#define AP_MAX_INACTIVITY_AFTER_DEAUTH (1 * 5)

#define DEFINE_PARTNER_STA_FUNC_CB(obj_name) \
static inline int set_partner_sta_cb_##obj_name(struct hostapd_data *hapd, \
						struct sta_info *sta, \
						void *data) \
{ \
	sta->obj_name = data; \
	return 0; \
}

#define SET_EACH_PARTNER_STA_OBJ(hapd, sta, objname, data) \
set_for_each_partner_link_sta(hapd, sta, data, set_partner_sta_cb_##objname)

DEFINE_PARTNER_STA_FUNC_CB(wpa_sm)
int set_for_each_partner_link_sta(struct hostapd_data *hapd,
				  struct sta_info *psta,
				  void *data,
				  int (*cb)(struct hostapd_data *hapd,
					    struct sta_info *sta, void *data));
void set_link_id_for_each_partner_link_sta(struct hostapd_data *hapd,
					   struct sta_info *psta,
					   int link_id);

int ap_for_each_sta(struct hostapd_data *hapd,
		    int (*cb)(struct hostapd_data *hapd, struct sta_info *sta,
			      void *ctx),
		    void *ctx);
struct hostapd_ft_over_ds_ml_sta_entry *ap_get_ft_ds_ml_sta(struct hostapd_data *hapd,
							    const u8 *sta_mld);
struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta);
struct sta_info *ap_get_unadded_sta(struct hostapd_data *hapd, const u8 *sta);
struct sta_info * ap_get_link_sta(struct hostapd_data *hapd,
				  const u8 *link_addr);
struct sta_info * ap_get_sta_p2p(struct hostapd_data *hapd, const u8 *addr);
void ap_sta_hash_add(struct hostapd_data *hapd, struct sta_info *sta);
void ap_free_sta(struct hostapd_data *hapd, struct sta_info *sta);
#ifdef CONFIG_IEEE80211BE
/*
 * ap_free_sta_link - remove a single link from an MLD STA
 *
 * Use for per-link removal (ML link removal, ML reconfiguration) where the
 * STA remains connected on other links. Issues NL80211_CMD_REMOVE_LINK_STA
 * then cleans up hostapd state via ap_free_sta().
 *
 * Do NOT use for a full MLD disconnect — use ap_free_sta() on the assoc link
 * which issues NL80211_CMD_DEL_STATION (MLD-level) atomically.
 */
void ap_free_sta_link(struct hostapd_data *hapd, struct sta_info *sta);
#endif /* CONFIG_IEEE80211BE */
void ap_free_unadded_link_sta(struct hostapd_data *hapd, struct sta_info *sta);
void ap_sta_ip6addr_del(struct hostapd_data *hapd, struct sta_info *sta);
void hostapd_free_stas(struct hostapd_data *hapd);
void ap_handle_timer(void *eloop_ctx, void *timeout_ctx);
void ap_sta_replenish_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			      u32 session_timeout);
void ap_sta_session_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			    u32 session_timeout);
void ap_sta_no_session_timeout(struct hostapd_data *hapd,
			       struct sta_info *sta);
void ap_sta_session_warning_timeout(struct hostapd_data *hapd,
				    struct sta_info *sta, int warning_time);
struct sta_info * ap_sta_add(struct hostapd_data *hapd, const u8 *addr);
void ap_sta_disassociate(struct hostapd_data *hapd, struct sta_info *sta,
			 u16 reason);
void ap_sta_deauthenticate(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 reason);
#ifdef CONFIG_WPS
int ap_sta_wps_cancel(struct hostapd_data *hapd,
		      struct sta_info *sta, void *ctx);
#endif /* CONFIG_WPS */
int ap_sta_bind_vlan(struct hostapd_data *hapd, struct sta_info *sta);
int ap_sta_set_vlan(struct hostapd_data *hapd, struct sta_info *sta,
		    struct vlan_description *vlan_desc);
void ap_sta_start_sa_query(struct hostapd_data *hapd, struct sta_info *sta);
void ap_sta_stop_sa_query(struct hostapd_data *hapd, struct sta_info *sta);
void ap_sta_set_sa_query_timeout(struct hostapd_data *hapd,
				 struct sta_info *sta, int value);
int ap_check_sa_query_timeout(struct hostapd_data *hapd, struct sta_info *sta);
const char * ap_sta_wpa_get_keyid(struct hostapd_data *hapd,
				  struct sta_info *sta);
const u8 * ap_sta_wpa_get_dpp_pkhash(struct hostapd_data *hapd,
				     struct sta_info *sta);
void ap_sta_disconnect(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *addr, u16 reason);

bool ap_sta_set_authorized_flag(struct hostapd_data *hapd, struct sta_info *sta,
				int authorized);
void ap_sta_set_authorized_event(struct hostapd_data *hapd,
				 struct sta_info *sta, int authorized);
bool ap_sta_set_authorized(struct hostapd_data *hapd,
			   struct sta_info *sta, int authorized);
static inline int ap_sta_is_authorized(struct sta_info *sta)
{
	return sta->flags & WLAN_STA_AUTHORIZED;
}

static inline void ap_sta_reset_assoc_req_rx_times(struct sta_info *sta)
{
	os_memset(&sta->last_assoc_req_rx_time, 0,
		  sizeof(sta->last_assoc_req_rx_time));
}

void ap_sta_deauth_cb(struct hostapd_data *hapd, struct sta_info *sta);
void ap_sta_disassoc_cb(struct hostapd_data *hapd, struct sta_info *sta);
void ap_sta_clear_disconnect_timeouts(struct hostapd_data *hapd,
				      struct sta_info *sta);
void ap_sta_clear_assoc_timeout(struct hostapd_data *hapd,
				struct sta_info *sta);

int ap_sta_flags_txt(u32 flags, char *buf, size_t buflen);
void ap_sta_delayed_1x_auth_fail_disconnect(struct hostapd_data *hapd,
					    struct sta_info *sta,
					    unsigned timeout);
int ap_sta_pending_delayed_1x_auth_fail_disconnect(struct hostapd_data *hapd,
						   struct sta_info *sta);
int ap_sta_re_add(struct hostapd_data *hapd, struct sta_info *sta,
		  int check_authorized);

void ap_free_sta_pasn(struct hostapd_data *hapd, struct sta_info *sta);

static inline bool ap_sta_is_mld(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	return (hapd->conf->mld_ap && sta && sta->mld_info.mld_sta);
#ifdef CONFIG_QCN_EXTN
	} else
		return false;
#endif /* CONFIG_QCN_EXTN */
#else /* CONFIG_IEEE80211BE */
	return false;
#endif /* CONFIG_IEEE80211BE */
}

static inline void ap_sta_set_mld(struct sta_info *sta, bool mld)
{
#ifdef CONFIG_IEEE80211BE
	if (sta)
		sta->mld_info.mld_sta = mld;
#endif /* CONFIG_IEEE80211BE */
}

#ifdef CONFIG_IEEE80211BE
void ap_sta_remove_link_sta(struct hostapd_data *hapd,
                            struct sta_info *sta,
			    int check_authorized, bool skip_kernel_delete);
int ap_sta_check_link_sta(struct hostapd_data *hapd,
			  struct sta_info *sta, const u8 *link_addr);
#endif
void ap_sta_free_sta_profile(struct mld_info *info);

void hostapd_free_link_stas(struct hostapd_data *hapd);
void set_wpa_sm_for_each_partner_link(struct hostapd_data *hapd,
				      struct sta_info *psta, void *wpa_sm);
void clear_wpa_sm_for_each_partner_link(struct hostapd_data *hapd,
					struct sta_info *psta);
void set_valid_for_each_partner_link_sta(struct hostapd_data *hapd,
                                           struct sta_info *psta,
                                           int valid);

int hostapd_free_partner_link_stas(struct hostapd_data *hapd,
				   struct sta_info *sta,
				   void *ctx);

int skip_prune_for_partner_links(struct hostapd_data *hapd,
				 struct sta_info *sta);
bool station_supports_256qam(struct sta_info *sta);
struct sta_info *ap_sta_get_from_obss(struct hostapd_data *hapd,
				      const u8 *mld_addr,
				      const u8 *link_addr,
				      struct hostapd_data **ohapd);
struct sta_info *ap_sta_get_by_link_addr(struct hostapd_data *hapd, const u8 *link_addr,
					 struct sta_info *curr_sta);
void ap_sta_cleanup_all(struct hostapd_data *hapd, struct sta_info *sta,
			struct sta_info *curr_sta);


static inline bool ap_sta_is_epp(const struct sta_info *sta)
{
#ifdef CONFIG_ENC_ASSOC
	return sta && sta->epp_sta;
#else /* CONFIG_ENC_ASSOC */
	return false;
#endif /* CONFIG_ENC_ASSOC */
}

static inline bool ap_sta_support_enc_assoc(struct hostapd_data *hapd,
					    const u8 *rsnxe, size_t rsnxe_len)
{
#ifdef CONFIG_ENC_ASSOC
		return (hapd->conf->assoc_frame_encryption &&
			ieee802_11_rsnx_capab_len(rsnxe,
						  rsnxe_len,
						  WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION));
#else /* CONFIG_ENC_ASSOC */
	return false;
#endif /* CONFIG_ENC_ASSOC */
}
#endif /* STA_INFO_H */
