/*
 * hostapd / Configuration definitions and helpers functions
 * Copyright (c) 2003-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HOSTAPD_CONFIG_H
#define HOSTAPD_CONFIG_H

#include "common/defs.h"
#include "utils/list.h"
#include "ip_addr.h"
#include "common/wpa_common.h"
#include "common/qca-vendor.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/sha256.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "vlan.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#if defined(HOSTAPD_EXTERNAL_PLUGIN_TESTAPP) && defined(CONFIG_QCN_EXTN)
#include "../../qcn_extns/hostapd_if_plugin.h"
#endif /* HOSTAPD_EXTERNAL_PLUGIN_TESTAPP && CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BN
#include "mapc.h"
#endif /* CONFIG_IEEE80211BN */

#define EHT_MCS_NSS_MAP_UNSET 0xffffU
#define EHT_MCS_NSS_SET 3

#define HE_MCS_NSS_MAP_UNSET 0xffffffffU
#define HE_MCS_NSS_SET 2

#ifdef CONFIG_IEEE80211AX
static inline bool hostapd_he_mcs_nss_set_is_set(const u32 mcs_nss_set[HE_MCS_NSS_SET])
{
	return mcs_nss_set[0] != HE_MCS_NSS_MAP_UNSET ||
		mcs_nss_set[1] != HE_MCS_NSS_MAP_UNSET;
}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
static inline bool hostapd_eht_mcs_nss_set_is_set(const u16 mcs_nss_set[EHT_MCS_NSS_SET])
{
	return mcs_nss_set[0] != EHT_MCS_NSS_MAP_UNSET ||
		mcs_nss_set[1] != EHT_MCS_NSS_MAP_UNSET ||
		mcs_nss_set[2] != EHT_MCS_NSS_MAP_UNSET;
}
#endif /* CONFIG_IEEE80211BE */

enum macaddr_acl {
	ACCEPT_UNLESS_DENIED = 0,
	DENY_UNLESS_ACCEPTED = 1,
	USE_EXTERNAL_RADIUS_AUTH = 2,
	ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST = 3,
	DENY_WITH_TIMED_ALLOW_WINDOW = 4
};

/* DENY_WITH_TIMED_ALLOW_WINDOW timing constraints (in seconds) */
#define SOFTBLOCK_WAIT_TIME_DEFAULT 10
#define SOFTBLOCK_WAIT_TIME_MIN 0
#define SOFTBLOCK_WAIT_TIME_MAX 100
#define SOFTBLOCK_ALLOW_TIME_DEFAULT 15
#define SOFTBLOCK_ALLOW_TIME_MIN 0
#define SOFTBLOCK_ALLOW_TIME_MAX 100

/**
 * struct acl_timed_deny_entry - Per-STA state for DENY_WITH_TIMED_ALLOW_WINDOW
 *
 * @next: Pointer to next entry in the acl_timed_deny linked list
 * @addr: MAC address of the station for which the timed state is saved
 * @phase_start: Timestamp when phase 1 began
 * @in_allow_phase: Current phase indicator (0=deny phase, 1=allow phase)
 */
struct acl_timed_deny_entry {
	struct acl_timed_deny_entry *next;
	u8 addr[ETH_ALEN];
	struct os_reltime phase_start;
	bool in_allow_phase;
};

/**
 * mesh_conf - local MBSS state and settings
 */
struct mesh_conf {
	u8 meshid[32];
	u8 meshid_len;
	/* Active Path Selection Protocol Identifier */
	u8 mesh_pp_id;
	/* Active Path Selection Metric Identifier */
	u8 mesh_pm_id;
	/* Congestion Control Mode Identifier */
	u8 mesh_cc_id;
	/* Synchronization Protocol Identifier */
	u8 mesh_sp_id;
	/* Authentication Protocol Identifier */
	u8 mesh_auth_id;
	u8 *rsn_ie;
	int rsn_ie_len;
#define MESH_CONF_SEC_NONE BIT(0)
#define MESH_CONF_SEC_AUTH BIT(1)
#define MESH_CONF_SEC_AMPE BIT(2)
	unsigned int security;
	enum mfp_options ieee80211w;
	int ocv;
	unsigned int pairwise_cipher;
	unsigned int group_cipher;
	unsigned int mgmt_group_cipher;
	int dot11MeshMaxRetries;
	int dot11MeshRetryTimeout; /* msec */
	int dot11MeshConfirmTimeout; /* msec */
	int dot11MeshHoldingTimeout; /* msec */
	int mesh_fwding;
};

#define MAX_STA_COUNT 2007
#define MAX_VLAN_ID 4094

typedef u8 macaddr[ETH_ALEN];

struct mac_acl_entry {
	macaddr addr;
	macaddr mask;
	struct vlan_description vlan_id;
};

struct hostapd_radius_servers;
struct ft_remote_r0kh;
struct ft_remote_r1kh;

#ifdef CONFIG_WEP
#define NUM_WEP_KEYS 4
struct hostapd_wep_keys {
	u8 idx;
	u8 *key[NUM_WEP_KEYS];
	size_t len[NUM_WEP_KEYS];
	int keys_set;
	size_t default_len; /* key length used for dynamic key generation */
};
#endif /* CONFIG_WEP */

typedef enum hostap_security_policy {
	SECURITY_PLAINTEXT = 0,
#ifdef CONFIG_WEP
	SECURITY_STATIC_WEP = 1,
#endif /* CONFIG_WEP */
	SECURITY_IEEE_802_1X = 2,
	SECURITY_WPA_PSK = 3,
	SECURITY_WPA = 4,
} secpolicy;

struct hostapd_ssid {
	u8 ssid[SSID_MAX_LEN];
	size_t ssid_len;
	u32 short_ssid;
	unsigned int ssid_set:1;
	unsigned int utf8_ssid:1;
	unsigned int wpa_passphrase_set:1;
	unsigned int wpa_psk_set:1;

	char vlan[IFNAMSIZ + 1];
	secpolicy security_policy;

	struct hostapd_wpa_psk *wpa_psk;
	char *wpa_passphrase;
	char *wpa_psk_file;
	struct sae_pt *pt;

#ifdef CONFIG_WEP
	struct hostapd_wep_keys wep;
#endif /* CONFIG_WEP */

#define DYNAMIC_VLAN_DISABLED 0
#define DYNAMIC_VLAN_OPTIONAL 1
#define DYNAMIC_VLAN_REQUIRED 2
	int dynamic_vlan;
	int vlan_no_bridge;
#define DYNAMIC_VLAN_NAMING_WITHOUT_DEVICE 0
#define DYNAMIC_VLAN_NAMING_WITH_DEVICE 1
#define DYNAMIC_VLAN_NAMING_END 2
	int vlan_naming;
	int per_sta_vif;
#ifdef CONFIG_FULL_DYNAMIC_VLAN
	char *vlan_tagged_interface;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
};


#define VLAN_ID_WILDCARD -1

struct hostapd_vlan {
	struct hostapd_vlan *next;
	int vlan_id; /* VLAN ID or -1 (VLAN_ID_WILDCARD) for wildcard entry */
	struct vlan_description vlan_desc;
	char ifname[IFNAMSIZ + 1];
	char bridge[IFNAMSIZ + 1];
	int configured;
	int dynamic_vlan;
#ifdef CONFIG_FULL_DYNAMIC_VLAN

#define DVLAN_CLEAN_WLAN_PORT	0x8
	int clean;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
};

#define PMK_LEN 32
#define KEYID_LEN 32
#define MIN_PASSPHRASE_LEN 8
#define MAX_PASSPHRASE_LEN 63
struct hostapd_sta_wpa_psk_short {
	struct hostapd_sta_wpa_psk_short *next;
	unsigned int is_passphrase:1;
	u8 psk[PMK_LEN];
	char passphrase[MAX_PASSPHRASE_LEN + 1];
	int ref; /* (number of references held) - 1 */
};

struct hostapd_wpa_psk {
	struct hostapd_wpa_psk *next;
	int group;
	char keyid[KEYID_LEN];
	int wps;
	u8 psk[PMK_LEN];
	u8 addr[ETH_ALEN];
	u8 p2p_dev_addr[ETH_ALEN];
	int vlan_id;
};

struct hostapd_eap_user {
	struct hostapd_eap_user *next;
	u8 *identity;
	size_t identity_len;
	struct {
		int vendor;
		u32 method;
	} methods[EAP_MAX_METHODS];
	u8 *password;
	size_t password_len;
	u8 *salt;
	size_t salt_len; /* non-zero when password is salted */
	int phase2;
	int force_version;
	unsigned int wildcard_prefix:1;
	unsigned int password_hash:1; /* whether password is hashed with
				       * nt_password_hash() */
	unsigned int macacl:1;
	int ttls_auth; /* EAP_TTLS_AUTH_* bitfield */
	struct hostapd_radius_attr *accept_attr;
	u32 t_c_timestamp;
};

struct hostapd_radius_attr {
	u8 type;
	struct wpabuf *val;
	struct hostapd_radius_attr *next;
};


#define NUM_TX_QUEUES 4
#define MAX_ROAMING_CONSORTIUM_LEN 15

struct hostapd_roaming_consortium {
	u8 len;
	u8 oi[MAX_ROAMING_CONSORTIUM_LEN];
};

struct hostapd_lang_string {
	u8 lang[3];
	u8 name_len;
	u8 name[252];
};

struct hostapd_venue_url {
	u8 venue_number;
	u8 url_len;
	u8 url[254];
};

#define MAX_NAI_REALMS 10
#define MAX_NAI_REALMLEN 255
#define MAX_NAI_EAP_METHODS 5
#define MAX_NAI_AUTH_TYPES 4
struct hostapd_nai_realm_data {
	u8 encoding;
	char realm_buf[MAX_NAI_REALMLEN + 1];
	char *realm[MAX_NAI_REALMS];
	u8 eap_method_count;
	struct hostapd_nai_realm_eap {
		u8 eap_method;
		u8 num_auths;
		u8 auth_id[MAX_NAI_AUTH_TYPES];
		u8 auth_val[MAX_NAI_AUTH_TYPES];
	} eap_method[MAX_NAI_EAP_METHODS];
};

struct anqp_element {
	struct dl_list list;
	u16 infoid;
	struct wpabuf *payload;
};

struct fils_realm {
	struct dl_list list;
	u8 hash[2];
	char realm[];
};

struct sae_password_entry {
	struct sae_password_entry *next;
	char *password;
	char *identifier;
	u8 peer_addr[ETH_ALEN];
	int vlan_id;
	struct sae_pt *pt;
	struct sae_pk *pk;
	u8 *success_mac;
	unsigned int num_success_mac, next_success_mac;
	u8 *fail_mac;
	unsigned int num_fail_mac, next_fail_mac;
};

struct dpp_controller_conf {
	struct dpp_controller_conf *next;
	u8 pkhash[SHA256_MAC_LEN];
	struct hostapd_ip_addr ipaddr;
};

struct airtime_sta_weight {
	struct airtime_sta_weight *next;
	unsigned int weight;
	u8 addr[ETH_ALEN];
};

#ifdef CONFIG_IEEE80211BN
struct smd_partner_entry {
	u8 mac_addr[ETH_ALEN];
	struct smd_partner_entry *next;
       u8 key[32];    /* AES-SIV-256 key; all-zero means no encryption */
       bool has_key;
};
#endif /* CONFIG_IEEE80211BN */

#define EXT_CAPA_MAX_LEN 15
#define MAX_VENDOR_ELEM_ALLOWED 10

#ifdef CONFIG_IEEE80211AX
struct he_phy_capabilities_info {
	bool he_su_beamformer;
	bool he_su_beamformee;
	bool he_mu_beamformer;
	bool he_dl_mu_ofdma;
	bool he_dl_mu_ofdma_bfer;
	bool he_ul_mu_ofdma;
	int he_ul_mumimo;
	u8 he_bfee_sts_lteq80;
	u8 he_bfee_sts_gt80;
	u8 he_multi_tid_aggr;
	u8 he_multi_tid_aggr_tx;
	u8 he_max_ampdu_len_exp;
	u8 he_su_ppdu_1x_ltf_800ns_gi;
	u8 he_su_mu_ppdu_4x_ltf_800ns_gi;
	u8 he_fragmentation;
	u8 he_max_frag_msdu;
	u8 he_min_frag_size;
	u8 he_amsdu_in_ampdu_suprt;
	u8 he_omi;
	u8 he_ndp_4x_ltf_3200ns_gi;
	u8 he_max_nc;
	u8 he_er_su_disable;
	u8 he_er_su_ppdu_1x_ltf_800ns_gi;
	u8 he_er_su_ppdu_4x_ltf_800ns_gi;
	u8 he_1024qam_lt242ru_rx_enable;
	u8 he_bsr_support;
};
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
struct eht_phy_capabilities_info {
	bool su_beamformer;
	bool su_beamformee;
	bool mu_beamformer;
	bool dl_mu_ofdma;
	bool ul_mu_ofdma;
	bool dl_ofdma_mumimo;
	bool ul_ofdma_mumimo;
	bool partial_bw_dl_mu_mimo;
	bool non_ofdma_ulmumimo_80mhz;
	bool non_ofdma_ulmumimo_160mhz;
	bool non_ofdma_ulmumimo_320mhz;
	u8 eht_mu_bfmr_mask;     /* bit0: &lt;=80, bit1: 160, bit2: 320 */
	u8 eht_mu_mimo_mask;     /* bit0: &lt;=80, bit1: 160, bit2: 320 */
	u8 eht_bfme_ss_80;
	u8 eht_bfme_ss_160;
	u8 eht_bfme_ss_320;
	u8 eht_ndp_4x_eht_ltf_and_320nsgi;
	u8 eht_num_sd_lt80;
	u8 eht_num_sd_160;
	u8 eht_num_sd_320;
	u8 eht_4x_eht_ltf_and_800ns_gi;
	u8 eht_rx_1024_and_4096_qam_ls_242_tone_ru;
	u8 eht_dl_ofdma_txbf;
	u8 eht_trig_su_bf_fb;
	u8 eht_trig_cqi_fb;
	u8 eht_non_trig_cqi_fb;
	u8 eht_sup_mcs15_in_mru;
	u8 eht_mcs14_dup_in_6ghz;
};
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
struct uhr_phy_capabilities_info {
	int uhr_2xldpc_tx;
	int uhr_2xldpc_rx;
};
#endif /* CONFIG_IEEE80211BN */

#define FEATURE_DISABLED	0
#define FEATURE_ENABLED		1


struct hostapd_uhr_npca_params {
	bool enable;
	u32 params;  /* npca_params (bits 0-31) */
	u16 disabled_subchan_bitmap;
};

/**
 * struct uhr_params_update_config - UHR params update configuration
 *
 * Configurable intervals for UHR parameter update notifications.
 */
struct uhr_params_update_config {
	u8 adv_notification_interval;
	u8 update_in_tim_interval;
	u16 mode_changed;
	struct hostapd_uhr_npca_params npca;
	/* TODO: Add DPS, DUO, P-EDCA, DBE, AP PUO, ELR mode param structs */
};

/* RTT responder role bitmask bits for hostapd_bss_config::rtt_responder_role */
#define HOSTAPD_RTT_RESPONDER_ROLE_11MC     BIT(0)
#define HOSTAPD_RTT_RESPONDER_ROLE_11AZ_NTB BIT(1)
#define HOSTAPD_RTT_RESPONDER_ROLE_11AZ_TB  BIT(2)

/**
 * struct hostapd_bss_config - Per-BSS configuration
 */
struct hostapd_bss_config {
	char iface[IFNAMSIZ + 1];
	char bridge[IFNAMSIZ + 1];
	char ft_iface[IFNAMSIZ + 1];
	char snoop_iface[IFNAMSIZ + 1];
	char vlan_bridge[IFNAMSIZ + 1];
	char wds_bridge[IFNAMSIZ + 1];
#ifdef CONFIG_QCN_EXTN
	struct hostapd_bss_config_extn bss_extn;
#endif /* CONFIG_QCN_EXTN */
	int bridge_hairpin; /* hairpin_mode on bridge members */

	enum hostapd_logger_level logger_syslog_level, logger_stdout_level;

	unsigned int logger_syslog; /* module bitfield */
	unsigned int logger_stdout; /* module bitfield */

	int max_num_sta; /* maximum number of STAs in station table */

	int dtim_period;
	unsigned int bss_load_update_period;
	unsigned int chan_util_avg_period;

	int ieee802_1x; /* use IEEE 802.1X */
	int eapol_version;
	int eap_server; /* Use internal EAP server instead of external
			 * RADIUS server */
	struct hostapd_eap_user *eap_user;
	char *eap_user_sqlite;
	char *eap_sim_db;
	unsigned int eap_sim_db_timeout;
	int eap_server_erp; /* Whether ERP is enabled on internal EAP server */
	struct hostapd_ip_addr own_ip_addr;
	int dynamic_own_ip_addr;
	char *nas_identifier;
	struct hostapd_radius_servers *radius;
	int radius_require_message_authenticator;
	int acct_interim_interval;
	int radius_request_cui;
	struct hostapd_radius_attr *radius_auth_req_attr;
	struct hostapd_radius_attr *radius_acct_req_attr;
	char *radius_req_attr_sqlite;
	int radius_das_port;
	unsigned int radius_das_time_window;
	int radius_das_require_event_timestamp;
	int radius_das_require_message_authenticator;
	struct hostapd_ip_addr radius_das_client_addr;
	u8 *radius_das_shared_secret;
	size_t radius_das_shared_secret_len;
#ifdef CONFIG_IEEE80211BN
	struct mapc_bss_config *mapc_conf;
#endif /* CONFIG_IEEE80211BN */
	struct hostapd_ssid ssid;

	int *supported_rates;
	int *basic_rates;

	char *eap_req_id_text; /* optional displayable message sent with
				* EAP Request-Identity */
	size_t eap_req_id_text_len;
	int eapol_key_index_workaround;

#ifdef CONFIG_WEP
	size_t default_wep_key_len;
	int individual_wep_key_len;
	int wep_rekeying_period;
	int broadcast_key_idx_min, broadcast_key_idx_max;
#endif /* CONFIG_WEP */
	int eap_reauth_period;
	int erp_send_reauth_start;
	char *erp_domain;
#ifdef CONFIG_TESTING_OPTIONS
	bool eap_skip_prot_success;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211AC
	u32 vht_capab;
#define VHT_CAP_BSS_OVR_SU_BEAMFORMER      BIT(0)
#define VHT_CAP_BSS_OVR_SU_BEAMFORMEE      BIT(1)
#define VHT_CAP_BSS_OVR_MU_BEAMFORMER      BIT(2)
#define VHT_CAP_BSS_OVR_SOUNDING_DIMENSION BIT(3)
#define VHT_CAP_BSS_OVR_STS_CAPABILITY     BIT(4)
	u32 vht_capab_mask;
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	struct he_phy_capabilities_info he_phy_capab;
#define HE_PHY_BSS_OVR_MU_BEAMFORMER    BIT(0)
#define HE_PHY_BSS_OVR_UL_MUMIMO        BIT(1)
#define HE_PHY_BSS_OVR_SU_BEAMFORMER    BIT(2)
#define HE_PHY_BSS_OVR_SU_BEAMFORMEE    BIT(3)
#define HE_PHY_BSS_OVR_DL_MU_OFDMA      BIT(4)
#define HE_PHY_BSS_OVR_DL_MU_OFDMA_BFER BIT(5)
#define HE_PHY_BSS_OVR_UL_MU_OFDMA      BIT(6)
#define HE_PHY_BSS_OVR_BFEE_STS_LTEQ80  BIT(7)
#define HE_PHY_BSS_OVR_BFEE_STS_GT80    BIT(8)
#define HE_PHY_BSS_OVR_MULTI_TID_AGGR   BIT(9)
#define HE_PHY_BSS_OVR_MULTI_TID_AGGR_TX BIT(10)
#define HE_PHY_BSS_OVR_MAX_AMPDU_LEN_EXP BIT(11)
#define HE_PHY_BSS_OVR_SU_PPDU_1X_LTF_800NS_GI BIT(12)
#define HE_PHY_BSS_OVR_SU_MU_PPDU_4X_LTF_800NS_GI BIT(13)
#define HE_PHY_BSS_OVR_MAX_FRAG_MSDU BIT(14)
#define HE_PHY_BSS_OVR_MIN_FRAG_SIZE BIT(15)
#define HE_PHY_BSS_OVR_OMI BIT(16)
#define HE_PHY_BSS_OVR_NDP_4X_LTF_3200NS_GI BIT(17)
#define HE_PHY_BSS_OVR_FRAGMENTATION BIT(18)
#define HE_PHY_BSS_OVR_AMSDU_IN_AMPDU_SUPRT BIT(19)
#define HE_PHY_BSS_OVR_MAX_NC_SUPRT BIT(20)
#define HE_PHY_BSS_OVR_ER_SU_DISABLE BIT(21)
#define HE_PHY_BSS_OVR_ER_SU_PPDU_1X_LTF_800NS_GI BIT(22)
#define HE_PHY_BSS_OVR_ER_SU_PPDU_4X_LTF_800NS_GI BIT(23)
#define HE_PHY_BSS_OVR_1024QAM_LT242RU_RX_ENABLE BIT(24)
#define HE_PHY_BSS_OVR_BSR_SUPPORT BIT(25)
	u32 he_phy_capab_mask;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	struct eht_phy_capabilities_info eht_phy_capab;
#define EHT_PHY_BSS_OVR_MU_BEAMFORMER   BIT(0)
#define EHT_PHY_BSS_OVR_SU_BEAMFORMER   BIT(4)
#define EHT_PHY_BSS_OVR_SU_BEAMFORMEE   BIT(5)
#define EHT_PHY_BSS_OVR_DL_MU_OFDMA     BIT(7)
#define EHT_PHY_BSS_OVR_UL_MU_OFDMA     BIT(8)
#define EHT_PHY_BSS_OVR_DL_OFDMA_MUMIMO BIT(9)
#define EHT_PHY_BSS_OVR_UL_OFDMA_MUMIMO BIT(10)
#define EHT_PHY_BSS_OVR_BFME_SS_80      BIT(11)
#define EHT_PHY_BSS_OVR_BFME_SS_160     BIT(12)
#define EHT_PHY_BSS_OVR_BFME_SS_320     BIT(13)
#define EHT_PHY_BSS_OVR_NON_OFDMA_UL_MUMIMO BIT(14)
#define EHT_PHY_BSS_OVR_MU_BFMR_MASK        BIT(15)
#define EHT_PHY_BSS_OVR_NDP_4X_EHT_LTF_AND_320NSGI BIT(16)
#define EHT_PHY_BSS_OVR_NUM_SD_LT80 BIT(17)
#define EHT_PHY_BSS_OVR_NUM_SD_160 BIT(18)
#define EHT_PHY_BSS_OVR_NUM_SD_320 BIT(19)
#define EHT_PHY_BSS_OVR_4X_EHT_LTF_AND_800NS_GI BIT(20)
#define EHT_PHY_BSS_OVR_RX_1024_AND_4096_QAM_LS_242_TONE_RU BIT(21)
#define EHT_PHY_BSS_OVR_DL_OFDMA_TXBF BIT(22)
#define EHT_PHY_BSS_OVR_SUP_MCS15_IN_MRU BIT(23)
#define EHT_PHY_BSS_OVR_MCS14_DUP_IN_6GHZ BIT(24)
#define EHT_PHY_BSS_OVR_TRIG_SU_BF_FB BIT(25)
#define EHT_PHY_BSS_OVR_TRIG_CQI_FB BIT(26)
#define EHT_PHY_BSS_OVR_NON_TRIG_CQI_FB BIT(27)
	u32 eht_phy_capab_mask;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	struct uhr_phy_capabilities_info uhr_phy_capab;
#define UHR_PHY_BSS_OVR_2XLDPC_TX   BIT(0)
#define UHR_PHY_BSS_OVR_2XLDPC_RX   BIT(1)
	u32 uhr_phy_capab_mask;
#endif /* CONFIG_IEEE80211BN */

	enum macaddr_acl macaddr_acl;
	struct mac_acl_entry *accept_mac;
	int num_accept_mac;
	struct mac_acl_entry *accept_mac_masked;
	int num_accept_mac_masked;
	struct mac_acl_entry *deny_mac;
	int num_deny_mac;
	struct mac_acl_entry *deny_mac_masked;
	int num_deny_mac_masked;
	/* deny phase duration (seconds) */
	unsigned int acl_deny_wait_time;
	/* allow phase duration (seconds) */
	unsigned int acl_deny_allow_time;
	/* per-STA state */
	struct acl_timed_deny_entry *acl_timed_deny_list;
	int wds_sta;
	int isolate;
	int start_disabled;
#ifdef HOSTAPD_EXTERNAL_PLUGIN
	int external_plugin_enable; /* Enable external plugin for this BSS */
#endif

	int auth_algs; /* bitfield of allowed IEEE 802.11 authentication
			* algorithms, WPA_AUTH_ALG_{OPEN,SHARED,LEAP} */

	int wpa; /* bitfield of WPA_PROTO_WPA, WPA_PROTO_RSN */
	int extended_key_id;
	int wpa_key_mgmt;
	int rsn_override_key_mgmt;
	int rsn_override_key_mgmt_2;
	enum mfp_options ieee80211w;
	enum mfp_options rsn_override_mfp;
	enum mfp_options rsn_override_mfp_2;
	int group_mgmt_cipher;
	int beacon_prot;
	int control_frame_prot;
	int group_control_frame_cipher;
	int max_cip_padding_delay; /* Configurable max_cip padding delay */
	/* dot11AssociationSAQueryMaximumTimeout (in TUs) */
	unsigned int assoc_sa_query_max_timeout;
	/* dot11AssociationSAQueryRetryTimeout (in TUs) */
	int assoc_sa_query_retry_timeout;
	/* Disable SA Query processing for WPA3 reassoc/MLD roam */
	int disable_sa_query;
#ifdef CONFIG_OCV
	int ocv; /* Operating Channel Validation */
#endif /* CONFIG_OCV */
	enum {
		PSK_RADIUS_IGNORED = 0,
		PSK_RADIUS_ACCEPTED = 1,
		PSK_RADIUS_REQUIRED = 2,
		PSK_RADIUS_DURING_4WAY_HS = 3,
	} wpa_psk_radius;
	int wpa_pairwise;
	int group_cipher; /* wpa_group value override from configuation */
	int wpa_group;
	int wpa_group_rekey;
	int wpa_group_rekey_set;
	int wpa_strict_rekey;
	int wpa_gmk_rekey;
	int wpa_ptk_rekey;
	enum ptk0_rekey_handling wpa_deny_ptk0_rekey;
	u32 wpa_group_update_count;
	u32 wpa_pairwise_update_count;
	int wpa_disable_eapol_key_retries;
	int rsn_pairwise;
	int rsn_override_pairwise;
	int rsn_override_pairwise_2;
	int rsn_preauth;
	char *rsn_preauth_interfaces;

	int rsn_override_omit_rsnxe;
	int rsn_override_mlo_compat;

#ifdef CONFIG_IEEE80211R_AP
	/* IEEE 802.11r - Fast BSS Transition */
	u8 mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	u8 r1_key_holder[FT_R1KH_ID_LEN];
	u32 r0_key_lifetime; /* PMK-R0 lifetime seconds */
	int rkh_pos_timeout;
	int rkh_neg_timeout;
	int rkh_pull_timeout; /* ms */
	int rkh_pull_retries;
	u32 reassociation_deadline;
	struct ft_remote_r0kh *r0kh_list;
	struct ft_remote_r1kh *r1kh_list;
	int pmk_r1_push;
	int ft_over_ds;
	int ft_psk_generate_local;
	int r1_max_key_lifetime;
	char *rxkh_file;
#endif /* CONFIG_IEEE80211R_AP */

	char *ctrl_interface; /* directory for UNIX domain sockets */
#ifndef CONFIG_NATIVE_WINDOWS
	gid_t ctrl_interface_gid;
#endif /* CONFIG_NATIVE_WINDOWS */
	int ctrl_interface_gid_set;

	char *ca_cert;
	char *server_cert;
	char *server_cert2;
	char *private_key;
	char *private_key2;
	char *private_key_passwd;
	char *private_key_passwd2;
	char *check_cert_subject;
	int check_crl;
	int check_crl_strict;
	unsigned int crl_reload_interval;
	unsigned int tls_session_lifetime;
	unsigned int tls_flags;
	unsigned int max_auth_rounds;
	unsigned int max_auth_rounds_short;
	char *ocsp_stapling_response;
	char *ocsp_stapling_response_multi;
	char *dh_file;
	char *openssl_ciphers;
	char *openssl_ecdh_curves;
	u8 *pac_opaque_encr_key;
	u8 *eap_fast_a_id;
	size_t eap_fast_a_id_len;
	char *eap_fast_a_id_info;
	int eap_fast_prov;
	int pac_key_lifetime;
	int pac_key_refresh_time;
	int eap_teap_auth;
	int eap_teap_separate_result;
	int eap_teap_id;
	int eap_teap_method_sequence;
	int eap_sim_aka_result_ind;
	int eap_sim_id;
	char *imsi_privacy_key;
	int eap_sim_aka_fast_reauth_limit;
	int tnc;
	int fragment_size;
	u16 pwd_group;

	char *radius_server_clients;
	int radius_server_auth_port;
	int radius_server_acct_port;
	int radius_server_acct_log;
	int radius_server_ipv6;

	int use_pae_group_addr; /* Whether to send EAPOL frames to PAE group
				 * address instead of individual address
				 * (for driver_wired.c).
				 */

	int ap_max_inactivity;
	int bss_max_idle;
	int max_acceptable_idle_period;
	bool no_disconnect_on_group_keyerror;
	int ignore_broadcast_ssid;
	int no_probe_resp_if_max_sta;

	int wmm_enabled;
	int wmm_uapsd;

	struct hostapd_vlan *vlan;

	macaddr bssid;

	/*
	 * Maximum listen interval that STAs can use when associating with this
	 * BSS. If a STA tries to use larger value, the association will be
	 * denied with status code 51.
	 */
	u16 max_listen_interval;

	int disable_pmksa_caching;
	int okc; /* Opportunistic Key Caching */

	int wps_state;
#ifdef CONFIG_WPS
	int wps_independent;
	int ap_setup_locked;
	u8 uuid[16];
	char *wps_pin_requests;
	char *device_name;
	char *manufacturer;
	char *model_name;
	char *model_number;
	char *serial_number;
	u8 device_type[WPS_DEV_TYPE_LEN];
	char *config_methods;
	u8 os_version[4];
	char *ap_pin;
	int skip_cred_build;
	u8 *extra_cred;
	size_t extra_cred_len;
	int wps_cred_processing;
	int wps_cred_add_sae;
	int force_per_enrollee_psk;
	u8 *ap_settings;
	size_t ap_settings_len;
	struct hostapd_ssid multi_ap_backhaul_ssid;
	char *upnp_iface;
	char *friendly_name;
	char *manufacturer_url;
	char *model_description;
	char *model_url;
	char *upc;
	struct wpabuf *wps_vendor_ext[MAX_WPS_VENDOR_EXTENSIONS];
	struct wpabuf *wps_application_ext;
	int wps_nfc_pw_from_config;
	int wps_nfc_dev_pw_id;
	struct wpabuf *wps_nfc_dh_pubkey;
	struct wpabuf *wps_nfc_dh_privkey;
	struct wpabuf *wps_nfc_dev_pw;
#endif /* CONFIG_WPS */
	int pbc_in_m1;
	char *server_id;

#define P2P_ENABLED BIT(0)
#define P2P_GROUP_OWNER BIT(1)
#define P2P_GROUP_FORMATION BIT(2)
#define P2P_MANAGE BIT(3)
#define P2P_ALLOW_CROSS_CONNECTION BIT(4)
	int p2p;
#ifdef CONFIG_P2P
	u8 ip_addr_go[4];
	u8 ip_addr_mask[4];
	u8 ip_addr_start[4];
	u8 ip_addr_end[4];
#endif /* CONFIG_P2P */

	int disassoc_low_ack;
	int skip_inactivity_poll;
	bool skip_disconnect;

#define TDLS_PROHIBIT BIT(0)
#define TDLS_PROHIBIT_CHAN_SWITCH BIT(1)
	int tdls;
	bool disable_11n;
	bool disable_11ac;
	bool disable_11ax;
	bool disable_11be;
	bool disable_11bn;
	bool bss_require_ht;
#ifdef CONFIG_IEEE80211AC
	bool bss_require_vht;
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	bool bss_require_he;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	bool bss_require_eht;
#endif /* CONFIG_IEEE80211BE */

	/* IEEE 802.11v */
	int time_advertisement;
	char *time_zone;
	int wnm_sleep_mode;
	int wnm_sleep_mode_no_keys;
	int bss_transition;

	/* IEEE 802.11u - Interworking */
	int interworking;
	int access_network_type;
	int internet;
	int asra;
	int esr;
	int uesa;
	int venue_info_set;
	u8 venue_group;
	u8 venue_type;
	u8 hessid[ETH_ALEN];

	/* IEEE 802.11u - Roaming Consortium list */
	unsigned int roaming_consortium_count;
	struct hostapd_roaming_consortium *roaming_consortium;

	/* IEEE 802.11u - Venue Name duples */
	unsigned int venue_name_count;
	struct hostapd_lang_string *venue_name;

	/* Venue URL duples */
	unsigned int venue_url_count;
	struct hostapd_venue_url *venue_url;

	/* IEEE 802.11u - Network Authentication Type */
	u8 *network_auth_type;
	size_t network_auth_type_len;

	/* IEEE 802.11u - IP Address Type Availability */
	u8 ipaddr_type_availability;
	u8 ipaddr_type_configured;

	/* IEEE 802.11u - 3GPP Cellular Network */
	u8 *anqp_3gpp_cell_net;
	size_t anqp_3gpp_cell_net_len;

	/* IEEE 802.11u - Domain Name */
	u8 *domain_name;
	size_t domain_name_len;

	unsigned int nai_realm_count;
	struct hostapd_nai_realm_data *nai_realm_data;

	struct dl_list anqp_elem; /* list of struct anqp_element */

	u16 gas_comeback_delay;
	size_t gas_frag_limit;
	int gas_address3;

	u8 qos_map_set[16 + 2 * 21];
	unsigned int qos_map_set_len;
	int bss_priority;
	int bss_priority_status;

	int proxy_arp;
	int na_mcast_to_ucast;

#ifdef CONFIG_HS20
	int hs20;
	int hs20_release;
	int disable_dgaf;
	u16 anqp_domain_id;
	unsigned int hs20_oper_friendly_name_count;
	struct hostapd_lang_string *hs20_oper_friendly_name;
	u8 *hs20_wan_metrics;
	u8 *hs20_connection_capability;
	size_t hs20_connection_capability_len;
	u8 *hs20_operating_class;
	u8 hs20_operating_class_len;
	unsigned int hs20_deauth_req_timeout;
	char *t_c_filename;
	u32 t_c_timestamp;
	char *t_c_server_url;
#endif /* CONFIG_HS20 */

	u8 wps_rf_bands; /* RF bands for WPS (WPS_RF_*) */

#ifdef CONFIG_RADIUS_TEST
	char *dump_msk_file;
#endif /* CONFIG_RADIUS_TEST */

	struct wpabuf *vendor_elements[MAX_VENDOR_ELEM_ALLOWED];
	size_t vendor_elements_count;
	size_t vendor_elements_len;
	size_t available_vendor_elem_size;
	struct wpabuf *assocresp_elements;

	unsigned int anti_clogging_threshold;
	unsigned int sae_sync;
	int sae_require_mfp;
	int sae_confirm_immediate;
	enum sae_pwe sae_pwe;
	int *sae_groups;
	struct sae_password_entry *sae_passwords;
	int sae_password_psk;
	int sae_track_password;
	struct wpabuf *sae_pw_id_key;
	unsigned int sae_pw_id_num;

	char *wowlan_triggers; /* Wake-on-WLAN triggers */

#ifdef CONFIG_TESTING_OPTIONS
	u8 bss_load_test[5];
	u8 bss_load_test_set;
	struct wpabuf *own_ie_override;
	struct wpabuf *rsne_override;
	struct wpabuf *rsnoe_override;
	struct wpabuf *rsno2e_override;
	struct wpabuf *rsnxe_override;
	struct wpabuf *rsnxoe_override;
	int sae_reflection_attack;
	int sae_commit_status;
	int sae_pk_omit;
	int sae_pk_password_check_skip;
	struct wpabuf *sae_commit_override;
	struct wpabuf *rsne_override_eapol;
	struct wpabuf *rsnxe_override_eapol;
	struct wpabuf *rsne_override_ft;
	struct wpabuf *rsnxe_override_ft;
	struct wpabuf *gtk_rsc_override;
	struct wpabuf *igtk_rsc_override;
	int no_beacon_rsnxe;
	u64 rsnxe_capab_mask;
	int skip_prune_assoc;
	int ft_rsnxe_used;
	unsigned int oci_freq_override_eapol_m3;
	unsigned int oci_freq_override_eapol_g1;
	unsigned int oci_freq_override_saquery_req;
	unsigned int oci_freq_override_saquery_resp;
	unsigned int oci_freq_override_ft_assoc;
	unsigned int oci_freq_override_fils_assoc;
	unsigned int oci_freq_override_wnm_sleep;
	struct wpabuf *eapol_m1_elements;
	struct wpabuf *eapol_m3_elements;
	bool eapol_m3_no_encrypt;
	bool eapol_key_reserved_random;
	int test_assoc_comeback_type;
	struct wpabuf *presp_elements;

#ifdef CONFIG_IEEE80211BE
	u16 eht_oper_puncturing_override;
#endif /* CONFIG_IEEE80211BE */
#endif /* CONFIG_TESTING_OPTIONS */

#define MESH_ENABLED BIT(0)
	int mesh;
	int mesh_fwding;

	u8 radio_measurements[RRM_CAPABILITIES_IE_LEN];

	int vendor_vht;
	int use_sta_nsts;

	char *no_probe_resp_if_seen_on;
	char *no_auth_if_seen_on;

	int pbss;

#ifdef CONFIG_MBO
	int mbo_enabled;
	/**
	 * oce - Enable OCE in AP and/or STA-CFON mode
	 *  - BIT(0) is Reserved
	 *  - Set BIT(1) to enable OCE in STA-CFON mode
	 *  - Set BIT(2) to enable OCE in AP mode
	 */
	unsigned int oce;
	 /**
	  *  Set BIT(6) to advertise cellular data aware capability
	  *  in AP mode
	  */
	 unsigned int mbo_ap_cap_ind;
	int mbo_cell_data_conn_pref;
	/* OCE IP Subnet Identifier attribute (Attr 108) */
	u8 oce_ip_subnet_id[6];
	bool oce_ip_subnet_id_set;
	/* OCE ESS Report element (EID 97) */
	bool oce_ess_report_enabled;
	int oce_ess_edge;
	int oce_ess_rssi_threshold;
	int oce_tx_power; /* -128 = use regulatory max */
	/* OCE Reduced WAN Metrics (Attr 103): 0-15 */
	int oce_dl_availcap;
	int oce_ul_availcap;
#endif /* CONFIG_MBO */

	int ftm_responder;
	int ftm_initiator;

	/* RTT responder role bitmask;
	 * bit 0 (HOSTAPD_RTT_RESPONDER_ROLE_11MC): 11mc FTM responder,
	 * bit 1 (HOSTAPD_RTT_RESPONDER_ROLE_11AZ_NTB): 11az NTB,
	 * bit 2 (HOSTAPD_RTT_RESPONDER_ROLE_11AZ_TB): 11az TB */
	int rtt_responder_role;

#ifdef CONFIG_FILS
	u8 fils_cache_id[FILS_CACHE_ID_LEN];
	int fils_cache_id_set;
	struct dl_list fils_realms; /* list of struct fils_realm */
	int fils_dh_group;
	struct hostapd_ip_addr dhcp_server;
	int dhcp_rapid_commit_proxy;
	unsigned int fils_hlp_wait_time;
	u16 dhcp_server_port;
	u16 dhcp_relay_port;
	u32 fils_discovery_min_int;
	u32 fils_discovery_max_int;
#endif /* CONFIG_FILS */

	int multicast_to_unicast;
	int bridge_multicast_to_unicast;

	int broadcast_deauth;

	int notify_mgmt_frames;
	int externally_triggered_m3;
	int plugin_eap_offload;
	int external_pmk_cache;
	int plugin_eapol_key_offload;

#ifdef CONFIG_DPP
	char *dpp_name;
	char *dpp_mud_url;
	char *dpp_extra_conf_req_name;
	char *dpp_extra_conf_req_value;
	char *dpp_connector;
	struct wpabuf *dpp_netaccesskey;
	unsigned int dpp_netaccesskey_expiry;
	struct wpabuf *dpp_csign;
#ifdef CONFIG_DPP2
	struct dpp_controller_conf *dpp_controller;
	int dpp_relay_port;
	int dpp_configurator_connectivity;
	int dpp_pfs;
#endif /* CONFIG_DPP2 */
#endif /* CONFIG_DPP */

#ifdef CONFIG_OWE
	macaddr owe_transition_bssid;
	u8 owe_transition_ssid[SSID_MAX_LEN];
	size_t owe_transition_ssid_len;
	char owe_transition_ifname[IFNAMSIZ + 1];
	int *owe_groups;
	int owe_ptk_workaround;
#endif /* CONFIG_OWE */

	int coloc_intf_reporting;

	u8 send_probe_response;

	u8 transition_disable;

#define BACKHAUL_BSS 1
#define FRONTHAUL_BSS 2
	int multi_ap; /* bitmap of BACKHAUL_BSS, FRONTHAUL_BSS */
	int multi_ap_profile;
	/* Multi-AP Profile-1 clients not allowed to connect */
#define PROFILE1_CLIENT_ASSOC_DISALLOW BIT(0)
	/* Multi-AP Profile-2 clients not allowed to connect */
#define PROFILE2_CLIENT_ASSOC_DISALLOW BIT(1)
	unsigned int multi_ap_client_disallow;
	/* Primary VLAN ID to use in Multi-AP */
	int multi_ap_vlanid;

#ifdef CONFIG_AIRTIME_POLICY
	unsigned int airtime_weight;
	int airtime_limit;
	struct airtime_sta_weight *airtime_weight_list;
#endif /* CONFIG_AIRTIME_POLICY */

#ifdef CONFIG_ATF_OFFLOAD
	u8 atf_ssid_sched;
#endif /* CONFIG_ATF_OFFLOAD */

#ifdef CONFIG_MACSEC
	/**
	 * macsec_policy - Determines the policy for MACsec secure session
	 *
	 * 0: MACsec not in use (default)
	 * 1: MACsec enabled - Should secure, accept key server's advice to
	 *    determine whether to use a secure session or not.
	 */
	int macsec_policy;

	/**
	 * macsec_integ_only - Determines how MACsec are transmitted
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: Encrypt traffic (default)
	 * 1: Integrity only
	 */
	int macsec_integ_only;

	/**
	 * macsec_replay_protect - Enable MACsec replay protection
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: Replay protection disabled (default)
	 * 1: Replay protection enabled
	 */
	int macsec_replay_protect;

	/**
	 * macsec_replay_window - MACsec replay protection window
	 *
	 * A window in which replay is tolerated, to allow receipt of frames
	 * that have been misordered by the network.
	 *
	 * This setting applies only when MACsec replay protection active, i.e.,
	 *  - macsec_replay_protect is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: No replay window, strict check (default)
	 * 1..2^32-1: number of packets that could be misordered
	 */
	u32 macsec_replay_window;

	/**
	 * macsec_offload - Enable MACsec offload
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0 = MACSEC_OFFLOAD_OFF (default)
	 * 1 = MACSEC_OFFLOAD_PHY
	 * 2 = MACSEC_OFFLOAD_MAC
	 */
	int macsec_offload;

	/**
	 * macsec_port - MACsec port (in SCI)
	 *
	 * Port component of the SCI.
	 *
	 * Range: 1-65534 (default: 1)
	 */
	int macsec_port;

	/**
	 * mka_priority - Priority of MKA Actor
	 *
	 * Range: 0-255 (default: 255)
	 */
	int mka_priority;

	/**
	 * macsec_csindex - Cipher suite index for MACsec
	 *
	 * Range: 0-1 (default: 0)
	 */
	int macsec_csindex;

	/**
	 * macsec_icv_indicator - Always include ICV Indicator
	 * (for compatibility with older MACsec switches)
	 *
	 * Range: 0-1 (default: 0)
	 */
	int macsec_icv_indicator;

	/**
	 * mka_ckn - MKA pre-shared CKN
	 */
#define MACSEC_CKN_MAX_LEN 32
	size_t mka_ckn_len;
	u8 mka_ckn[MACSEC_CKN_MAX_LEN];

	/**
	 * mka_cak - MKA pre-shared CAK
	 */
#define MACSEC_CAK_MAX_LEN 32
	size_t mka_cak_len;
	u8 mka_cak[MACSEC_CAK_MAX_LEN];

#define MKA_PSK_SET_CKN BIT(0)
#define MKA_PSK_SET_CAK BIT(1)
#define MKA_PSK_SET (MKA_PSK_SET_CKN | MKA_PSK_SET_CAK)
	/**
	 * mka_psk_set - Whether mka_ckn and mka_cak are set
	 */
	u8 mka_psk_set;
#endif /* CONFIG_MACSEC */

#ifdef CONFIG_PASN
	/* Whether to allow PASN-UNAUTH */
	int pasn_noauth;

#ifdef CONFIG_ENC_ASSOC
	/* Whether to allow unauthenticated EPPKE (EPPKE without base AKM) */
	bool eppke_unauth;
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_TESTING_OPTIONS
	/*
	 * Normally, KDK should be derived if and only if both sides support
	 * secure LTF. Allow forcing KDK derivation for testing purposes.
	 */
	int force_kdk_derivation;

	/* If set, corrupt the MIC in the 2nd Authentication frame of PASN */
	int pasn_corrupt_mic;
#endif /* CONFIG_TESTING_OPTIONS */

	int *pasn_groups;

	/*
	 * The time in TUs after which the non-AP STA is requested to retry the
	 * PASN authentication in case there are too many parallel operations.
	 */
	u16 pasn_comeback_after;
#endif /* CONFIG_PASN */

	int urnm_mfpr_x20;
	int urnm_mfpr;

	unsigned int unsol_bcast_probe_resp_interval;

	u8 ext_capa_mask[EXT_CAPA_MAX_LEN];
	u8 ext_capa[EXT_CAPA_MAX_LEN];

	u8 rnr;
	u8 rnr_ie_allowed;
	int beacon_tx_mode;
	char *config_id;

	bool ssid_protection;
	bool known_sta_identification;

#ifdef CONFIG_IEEE80211BE
	/* The AP is part of an AP MLD */
	u8 mld_ap;

	/* The MLD ID to which the AP MLD is affiliated with */
	u8 mld_id;

	/* The AP's MLD MAC address within the AP MLD */
	u8 mld_addr[ETH_ALEN];

	bool enable_aal;

	/* MLD link-count limits */
	u8 ml_max_rec_links;
	/* Maximum accepted partner links per non-AP MLD STA (0 = unlimited) */
	u8 mld_max_links_per_sta;
	/* Minimum required partner links per non-AP MLD STA (0 = no minimum) */
	u8 mld_min_links_per_sta;
	/*
	 * Link rejection mode when mld_max_links_per_sta cap is hit.
	 * 0 = default: reject 2G first, keep 5G/6G (band-priority)
	 * 1 = range:   never reject 2G, reject 5G first (qca-wifi MLO_LINK_REJ_RANGE)
	 */
	u8 mld_link_rej_mode;

	bool single_link_emlsr;
	bool disable_eml;

/*
 * Bitmask controlling inclusion of UHR extension fields (i.e. fields added
 * by UHR to extend elements that existed pre-UHR, e.g. Enhanced Critical
 * Update Information and Age of BSS Load fields in the Basic Multi-Link
 * element).
 */
#define SKIP_UHR_EXTN_BCN        BIT(0)
#define SKIP_UHR_EXTN_PROBE_RESP BIT(1)
#define SKIP_UHR_EXTN_RNR        BIT(2)
	u8 skip_uhr_extn;

	/* User-configured link ID; -1 means auto-allocate */
	int mld_link_id;

#ifdef CONFIG_TESTING_OPTIONS
	/*
	 * If set indicate the AP as disabled in the RNR element included in the
	 * other APs in the AP MLD.
	 */
	bool mld_indicate_disabled;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_QCN_EXTN
	/*
	 * Override index used to derive vendor BSSID for when
	 * 'use_driver_vendor_addr=1'.
	 * -1 means not set and hostapd will use the interface
	 * BSS index available.
	 */
	int bss_index;
#endif /* CONFIG_QCN_EXTN */

	bool spp_amsdu;
	enum {
		FILS_UBPR_USER_DISABLED,
		FILS_UBPR_FORCE_DISABLED,
		FILS_UBPR_ENABLED
	} fils_state, ubpr_state;

	/* Flag to decide whether or not to force disable FILS/UBPR for
	 * 6GHz AP based on colocation.
	 */
	bool force_disable_in_band_discovery;

	bool wmm_override;
	struct hostapd_wmm_ac_params wmm_ac_params[4];

	enum qca_wlan_intf_offload_type ppe_vp_type;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_wmm_ac_params epcs_wmm_ac_params[4];
	struct hostapd_wmm_ac_params prev_epcs_wmm_ac_params[4];
	struct ieee80211_he_mu_edca_parameter_set epcs_he_mu_edca;
	int epcs_parameter_set_count;
	bool is_epcs_enabled;
	bool ttlm_enable;
#endif /* CONFIG_IEEE80211BE */
	bool enable_dscp_policy_capa;
	/*
	 * twt_responder_caps: 0 - Disable TWT
	 *                     1 - Enable TWT
	 *                     2 - Enable TWT & B-TWT
	 *                     3 - Enable TWT & B-TWT & R-TWT
	 */
	enum {
		TWT_DISABLED,
		TWT_ITWT_ENABLED,
		TWT_ITWT_BTWT_ENABLED,
		TWT_ITWT_BTWT_RTWT_ENABLED,
	} twt_responder_caps;

#ifdef CONFIG_IEEE80211AX
	bool scs;
	bool mscs;
	bool deferred_scs;
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211AC
	u16 vht_mcs_nss_set;
#endif /* CONFIG_IEEE80211AC */
	u32 ht_mcs_nss_set;
#ifdef CONFIG_IEEE80211AX
	u32 he_tx_mcs_nss_set[HE_MCS_NSS_SET];
	u32 he_rx_mcs_nss_set[HE_MCS_NSS_SET];
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	u16 eht_tx_mcs_nss_set[EHT_MCS_NSS_SET];
	u16 eht_rx_mcs_nss_set[EHT_MCS_NSS_SET];
#endif /* CONFIG_IEEE80211BE */

	enum beacon_rate_type rate_type;
	unsigned int beacon_rate;
	int identity_request_retry_interval;
	int eht_ltf;

	/* tpe_ie_config - Per-BSS TPE IE user configuration */
	ieee80211_tpe_config_user_params tpe_ie_config;
	enum rate_type probe_resp_rate_type;
	u16 probe_resp_rate;

	/**
	 * rssi_reject_assoc_rssi - Minimum RSSI for association (per-BSS)
	 *
	 * This is an override for the per-radio rssi_reject_assoc_rssi in
	 * struct hostapd_config. If set to 0, falls back to the radio-wide
	 * threshold. If both are 0, the feature is disabled.
	 *
	 * Value in dBm (e.g., -75 means -75 dBm)
	 */
	int rssi_reject_assoc_rssi;

	/**
	 * rssi_reject_assoc_timeout - Timeout for rejected association (per-BSS)
	 *
	 * This is the time until which the AP will continue rejecting
	 * a station, which was initially rejected because of low RSSI. Until
	 * this time has passed, the AP will continue rejecting the STA
	 * regardless of the RSSI value in the subsequent Assoc-Request frames
	 *
	 * If set to 0, no timeout is enforced, i.e., AP will accept the
	 * Assoc frame purely based on the RSSI value in the current incoming
	 * frame (and not dependent on any history)
	 *
	 * Falls back to radio-wide timeout if not set.
	 */
	int rssi_reject_assoc_timeout;

	/**
	 * rssi_deauth_grace_samples - Grace samples for RSSI deauth (per-BSS)
	 *
	 * Number of consecutive low RSSI samples before triggering deauth.
	 * Range: 1-100. Falls back to radio-wide setting if not set.
	 */
	int rssi_deauth_grace_samples;

#ifdef CONFIG_IEEE80211BN
	/**
	 * dps_assist - DPS Assist Support
	 *
	 * Per-BSS support for DPS Assist for AP. If driver support is set,
	 * 0 and 1 are used to enable and disable support.
	 */
	int dps_assist;

	/**
	 * uhr_params_update - UHR params update configuration
	 *
	 * for the UHR Parameters Update element (Draft IEEE P802.11bn 1.4)
	 */
	struct uhr_params_update_config uhr_params_update;
#endif /* CONFIG_IEEE80211BN */

#ifdef HOSTAPD_EXTERNAL_PLUGIN_TESTAPP
	struct hostapd_config_plugin plugin;
#endif

	int *security_profiles;       /* NULL-terminated array from hostapd_parse_intlist */
	int security_profile_ext_key_id; /* Reduced RSN Capabilities bit 0 */
	int security_profile_ocvc;    /* Reduced RSN Capabilities bit 1 */
	/* AP side */
#ifdef CONFIG_IEEE80211BN
	struct {
		int enabled; /* SMD AP enabled */
		u8 smd_identifier[ETH_ALEN]; /* SMD Identifier (MAC address) */
		u8 smd_prep_timeout; /* SMD Element Preparation Timeout, units of 64 TUs */
		/* UHR ST Execute configuration */
		u32 uhr_dl_drain_duration_tu;  /* DL Drain duration in TU */
		struct {
			bool dl_data_fwd; /* DL Data Forwarding */
			u8 max_prep_target_apmlds; /* Max Number Of Prepared Target AP MLDs */
			bool smd_type; /* SMD Type */
			bool ptk_mode; /* PTK Mode */
		} caps;
	} smd;

       /* SMD partner APs for roaming */
       struct smd_partner_entry *smd_partners;

	/* SMD Neighbor Update */
	int smd_neighbor_update_enabled;
	int smd_neighbor_expiry_time;    /* Entry expiry time in seconds */
	int smd_neighbor_pull_interval;  /* Pull request interval in seconds */
#endif /* CONFIG_IEEE80211BN */

	/* is_cmn_param - flag to identify MBSSID common parameter */
	bool is_cmn_param;

	/* cmn_param_id - MBSSID common parameter ID */
	int cmn_param_id;

	/* cmn_param_id - MBSSID common parameter values */
	int cmn_param_val[2];
#ifdef CONFIG_ENC_ASSOC
	unsigned int assoc_frame_encryption:1;
	unsigned int pmksa_caching_privacy:1;
	unsigned int eap_using_authentication_frames:1;
#endif /* CONFIG_ENC_ASSOC  */
	unsigned int report_connection_failures:1;

	int vlan_idle_cleanup;
};

/**
 * struct he_operation - HE operation
 */
struct he_operation {
	u8 he_bss_color;
	u8 he_bss_color_disabled;
	u8 he_bss_color_partial;
	u8 he_bss_color_collision_detection;
	u8 he_default_pe_duration;
	u8 he_twt_required;
	u8 he_twt_responder;
	u16 he_rts_threshold;
	u8 he_er_su_disable;
	u16 he_basic_mcs_nss_set;
};

/**
 * struct spatial_reuse - Spatial reuse
 */
struct spatial_reuse {
	u8 sr_control;
	u8 non_srg_obss_pd_max_offset;
	u8 srg_obss_pd_min_offset;
	u8 srg_obss_pd_max_offset;
	u8 srg_bss_color_bitmap[8];
	u8 srg_partial_bssid_bitmap[8];
};

/**
 * struct hostapd_config - Per-radio interface configuration
 */
struct hostapd_config {
	struct hostapd_bss_config **bss, *last_bss;
	size_t num_bss;

#ifdef CONFIG_QCN_EXTN
	struct hostapd_config_extn conf_extn;
#endif /* CONFIG_QCN_EXTN */
	u16 beacon_int;
	int rts_threshold;
	int fragm_threshold;
	u8 op_class;
	u8 channel;
	int enable_edmg;
	u8 edmg_channel;
	u8 acs;
	struct wpa_freq_range_list acs_ch_list;
	struct wpa_freq_range_list acs_freq_list;
	u8 acs_freq_list_present;
	int acs_exclude_dfs;
	int radio_idx; /* hw_idx from multi_hw_info, -1 if not multi-radio */
	int acs_enable_bw_downgrade;
	int acs_dfs_available_only;
	u8 min_tx_power;
	enum hostapd_hw_mode hw_mode; /* HOSTAPD_MODE_IEEE80211A, .. */
	bool hw_mode_set;
	int acs_exclude_6ghz_non_psc;
	int enable_background_radar;
	int bgcac_en;
	bool dfs_bw_reduce_en;
	int rcac_freq;
	enum {
		LONG_PREAMBLE = 0,
		SHORT_PREAMBLE = 1
	} preamble;

	const struct wpa_driver_ops *driver;
	char *driver_params;

	int ap_table_max_size;
	int ap_table_expiration_time;

	unsigned int track_sta_max_num;
	unsigned int track_sta_max_age;

	int max_num_sta;

	char country[3]; /* first two octets: country code as described in
			  * ISO/IEC 3166-1. Third octet:
			  * ' ' (ascii 32): all environments
			  * 'O': Outdoor environemnt only
			  * 'I': Indoor environment only
			  * 'X': Used with noncountry entity ("XXX")
			  * 0x00..0x31: identifying IEEE 802.11 standard
			  *	Annex E table (0x04 = global table)
			  */

	int ieee80211d;

	int ieee80211h; /* DFS */
	int dfs_test_mode;

	/*
	 * Local power constraint is an octet encoded as an unsigned integer in
	 * units of decibels. Invalid value -1 indicates that Power Constraint
	 * element will not be added.
	 */
	int local_pwr_constraint;

	/* Control Spectrum Management bit */
	int spectrum_mgmt_required;

	struct hostapd_tx_queue_params tx_queue[NUM_TX_QUEUES];

	/*
	 * WMM AC parameters, in same order as 802.1D, i.e.
	 * 0 = BE (best effort)
	 * 1 = BK (background)
	 * 2 = VI (video)
	 * 3 = VO (voice)
	 */
	struct hostapd_wmm_ac_params wmm_ac_params[4];

	int ht_op_mode_fixed;
	u16 ht_capab;
	int noscan;
	int no_ht_coex;
	int ieee80211n;
	int secondary_channel;
	int no_pri_sec_switch;
	int require_ht;
	int obss_interval;
	u32 vht_capab;
	int ieee80211ac;
	int require_vht;
	enum oper_chan_width vht_oper_chwidth;
	u8 vht_oper_centr_freq_seg0_idx;
	u8 vht_oper_centr_freq_seg1_idx;
	u8 ht40_plus_minus_allowed;

	/* Use driver-generated interface addresses when adding multiple BSSs */
	u8 use_driver_iface_addr;

#ifdef CONFIG_QCN_EXTN
	/* Use vendor-generated BSSID */
	u8 use_driver_vendor_addr;
#endif /* CONFIG_QCN_EXTN */

	u8 skip_unii1_dfs_switch;
	u8 ext_cap_len;

#ifdef CONFIG_FST
	struct fst_iface_cfg fst_cfg;
#endif /* CONFIG_FST */

#ifdef CONFIG_P2P
	u8 p2p_go_ctwindow;
#endif /* CONFIG_P2P */

#ifdef CONFIG_TESTING_OPTIONS
	double ignore_probe_probability;
	double ignore_auth_probability;
	double ignore_assoc_probability;
	double ignore_reassoc_probability;
	double corrupt_gtk_rekey_mic_probability;
	int ecsa_ie_only;
	int csa_ie_only;
	bool delay_eapol_tx;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_ACS
	unsigned int acs_num_scans;
	struct acs_bias {
		int channel;
		double bias;
	} *acs_chan_bias;
	unsigned int num_acs_chan_bias;
	/* Interval in seconds between ACS scan retries when driver is busy */
	unsigned int acs_scan_retry_interval;
	/* Maximum number of ACS scan retry attempts when driver is busy */
	int acs_scan_retry_max_count;
#endif /* CONFIG_ACS */
	int disable_40mhz_scan;

	struct wpabuf *lci;
	struct wpabuf *civic;
	int stationary_ap;

	int ieee80211ax;
#ifdef CONFIG_IEEE80211AX
	struct he_phy_capabilities_info he_phy_capab;
	struct he_operation he_op;
	struct ieee80211_he_mu_edca_parameter_set he_mu_edca;
	struct spatial_reuse spr;
	enum oper_chan_width he_oper_chwidth;
	u8 he_oper_centr_freq_seg0_idx;
	u8 he_oper_centr_freq_seg1_idx;
	u8 he_6ghz_max_mpdu;
	u8 he_6ghz_max_ampdu_len_exp;
	u8 he_6ghz_rx_ant_pat;
	u8 he_6ghz_tx_ant_pat;
	u8 he_6ghz_reg_pwr_type;
	u8 he_6ghz_min_rate;
	/*
	 * This indicates hostapd support for calculation of best power mode
	 * for the 6 GHz band. This is used to determine whether we have to
	 * determine the best power mode for the 6 GHz band during channel
	 * switch, power mode failure, AFC payload based random channel
	 * selection, etc.
	 */
	bool enable_best_power_mode;
	/*
	 * This controls the hostapd support for dynamically puncturing
	 * the Low Power channels or disabled channels in 6 GHz band in
	 * Standard Power mode. Setting this to false will allow hostapd
	 * to dynamically puncture channels. Setting this to true will
	 * disable dynamic puncturing.
	 */
	bool puncture_strict_6ghz;
	/*
	 * This configures the EIRP threshold to be used for dynamic
	 * puncturing of 6 GHz channels in Standard Power mode.
	 * If puncture_strict_6ghz is set to false, we try to puncture
	 * channels till the EIRP of the channel meets this threshold.
	 */
	s16 punc_eirp_thres_6ghz;
	bool discard_6g_awgn_event;

	int reg_def_cli_eirp_psd;
	int reg_sub_cli_eirp_psd;

	/*
	 * This value should be used when regulatory client EIRP PSD values
	 * advertised by an AP that is an SP AP or an indoor SP AP are
	 * insufficient to ensure that regulatory client limits on total EIRP
	 * are always met for all transmission bandwidths within the bandwidth
	 * of the AP’s BSS.
	 */
	int reg_def_cli_eirp;

	bool require_he;
#endif /* CONFIG_IEEE80211AX */

	/* VHT enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_VHT_ENABLED BIT(0)
#define CH_SWITCH_VHT_DISABLED BIT(1)
	unsigned int ch_switch_vht_config;

	/* HE enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_HE_ENABLED BIT(0)
#define CH_SWITCH_HE_DISABLED BIT(1)
	unsigned int ch_switch_he_config;

	int rssi_reject_assoc_rssi;
	int rssi_reject_assoc_timeout;
	int rssi_deauth_grace_samples;
	int rssi_ignore_probe_request;
	int rssi_probe_delay_time_window;
	int rssi_probe_delay_req_count;

#ifdef CONFIG_AIRTIME_POLICY
	enum {
		AIRTIME_MODE_OFF = 0,
		AIRTIME_MODE_STATIC = 1,
		AIRTIME_MODE_DYNAMIC = 2,
		AIRTIME_MODE_LIMIT = 3,
		__AIRTIME_MODE_MAX,
	} airtime_mode;
	unsigned int airtime_update_interval;
#define AIRTIME_MODE_MAX (__AIRTIME_MODE_MAX - 1)
#endif /* CONFIG_AIRTIME_POLICY */

#ifdef CONFIG_ATF_OFFLOAD
	bool atf_offload;
	bool commitatf;
	char *atf_offload_config;
	bool atf_ssid_grp;
	bool atf_strict_sched;
#endif /* CONFIG_ATF_OFFLOAD */

	int ieee80211be;
#ifdef CONFIG_IEEE80211BE
	enum oper_chan_width eht_oper_chwidth;
	u8 eht_oper_centr_freq_seg0_idx;
	struct eht_phy_capabilities_info eht_phy_capab;
	u16 punct_bitmap; /* a bitmap of disabled 20 MHz channels */
	u8 punct_acs_threshold;
	u8 eht_default_pe_duration;
	u8 eht_bw320_offset;
	bool require_eht;
#endif /* CONFIG_IEEE80211BE */

	int ieee80211bn;

	/* EHT enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_EHT_ENABLED BIT(0)
#define CH_SWITCH_EHT_DISABLED BIT(1)
	unsigned int ch_switch_eht_config;
	int disable_csa_dfs;

	/**
	 * @post_nol_freq: Frequency (MHz) the AP switches to once the
	 * NOL entry for this channel expires.
	 *
	 * -1 by default.
	 */
	int post_nol_freq;

	/**
	 * @post_nol_width: Bandwidth (MHz) the AP switches to once the
	 * NOL entry for post_nol_freq expires.
	 *
	 * -1 by default.
	 */
	int post_nol_width;

	/**
	 * @post_nol_bgcac_en: Whether to use Agile CAC for the post_nol_freq
	 * channel switch on NOL expiry.
	 *
	 * Enabled (1) by default.
	 */
	bool post_nol_bgcac_en;

	/**
	 * @next_radar_freq: Frequency (MHz) the AP switches to immediately
	 * upon radar detection on the operating channel.
	 *
	 * -1 by default
	 */
	int next_radar_freq;

	/**
	 * @next_radar_width: Bandwidth (MHz) the AP switches to immediately
	 * upon radar detection on the operating channel.
	 *
	 * -1 by default
	 */
	int next_radar_width;

#define CH_SWITCH_UHR_ENABLED BIT(0)
#define CH_SWITCH_UHR_DISABLED BIT(1)
	unsigned int ch_switch_uhr_config;

	enum mbssid {
		MBSSID_DISABLED = 0,
		MBSSID_ENABLED = 1,
		ENHANCED_MBSSID_ENABLED = 2,
		MULTI_MBSSID_GROUP_ENABLED = 3,
	} mbssid;
	unsigned int mbssid_max;

	/* Whether to enable TWT responder in HT and VHT modes */
	bool ht_vht_twt_responder;

	bool channel_usage;
	bool peer_to_peer_twt;
	int use_ru_puncture_dfs;

	/* Add support to control auto-unpuncture of radar puncture channels */
	int dfs_disable_auto_unpunc;
	int ccfs;
	int bandwidth_device;
	int center_freq_device;

#define MULTI_MBSSID_GROUP_SIZE_MIN 2
#define MULTI_MBSSID_GROUP_SIZE_MAX 8
#define MULTI_MBSSID_GROUP_SIZE_DEFAULT 4
	u8 group_size;

	/* Config to enable composite AP mode.
	 * Will be enabled by default if not explicitly disabled in the
	 * hostpd config.
	 */
	bool enable_6ghz_composite_ap;
	bool enable_mcs15;
	int afc_chan_sel_config;
	int cur_chan_eirp;
	int original_chan_width; /* Original channel width for AFC */
#ifdef CONFIG_QCN_EXTN
	bool downgrade_320mhz_opclass;

	/* RNR Member of ESS of 2.4/5 GHz colocated BSS param indication */
	bool rnr_colocated_ess;
#endif
	/* HE BSS color collision AP period (seconds) */
	u16 he_bss_color_collision_ap_period;

	/* Set I2R LMR policy to allow LMR response from ISTA */
	bool i2r_lmr_policy;

	/* Monitor interface name */
	char monitor_iface_name[IFNAMSIZ + 1];
	int *security_profiles;       /* NULL-terminated array from hostapd_parse_intlist */
	int security_profile_ext_key_id; /* Reduced RSN Capabilities bit 0 */
	int security_profile_ocvc;    /* Reduced RSN Capabilities bit 1 */
#ifdef CONFIG_IEEE80211BN
	bool npca_enable;
	u8 npca_primary_channel;
	u16 npca_punct_bitmap;
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_MQTT
	/*
	 * Global MQTT settings — one connection per hostapd instance.
	 * Set in the interface config block (before any bss= line).
	 *
	 * mqtt_enabled=1          enable MQTT on startup
	 * mqtt_broker_host=<h>    broker hostname or IP (default: localhost)
	 * mqtt_broker_port=<p>    broker TCP port (default: 1883)
	 */
	int   mqtt_enabled;
	char *mqtt_broker_host;
	int   mqtt_broker_port;
#endif /* CONFIG_MQTT */
	/* intermediate non-DFS channel to be used to boot-up quickly */
	int intercac_chan;
	int intercac_chwidth;
};


#ifdef CONFIG_IEEE80211BN
/* NPCA is channel-specific; call on every channel change so a stale
 * primary channel/puncture bitmap from the old channel is never
 * advertised. The AP may re-enable NPCA later via a UHR Parameters
 * Update ECU on the new channel.
 */
static inline void hostapd_disable_npca(struct hostapd_config *conf)
{
	conf->npca_enable = false;
	conf->npca_primary_channel = 0;
	conf->npca_punct_bitmap = 0;
}

/* struct hostapd_npca_state - snapshot of conf's NPCA fields
 *
 * Used to temporarily disable NPCA (e.g. while building a beacon for
 * a channel that has not actually been switched to yet) and restore
 * the prior state afterwards.
 */
struct hostapd_npca_state {
	bool npca_enable;
	u8 npca_primary_channel;
	u16 npca_punct_bitmap;
};

static inline void hostapd_save_npca(struct hostapd_config *conf,
				     struct hostapd_npca_state *state)
{
	state->npca_enable = conf->npca_enable;
	state->npca_primary_channel = conf->npca_primary_channel;
	state->npca_punct_bitmap = conf->npca_punct_bitmap;
}

static inline void hostapd_restore_npca(struct hostapd_config *conf,
					const struct hostapd_npca_state *state)
{
	conf->npca_enable = state->npca_enable;
	conf->npca_primary_channel = state->npca_primary_channel;
	conf->npca_punct_bitmap = state->npca_punct_bitmap;
}

int acs_npca_select_primary_chan(struct hostapd_iface *iface,
				 u8 *npca_channel,
				 u16 *npca_punct_bitmap);
#endif /* CONFIG_IEEE80211BN */


static inline enum oper_chan_width
hostapd_get_oper_chwidth(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		return conf->eht_oper_chwidth;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_chwidth;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_chwidth;
}

static inline void
hostapd_set_oper_chwidth(struct hostapd_config *conf,
			 enum oper_chan_width oper_chwidth)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		conf->eht_oper_chwidth = oper_chwidth;
	if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
		oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_chwidth = oper_chwidth;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_chwidth = oper_chwidth;
#ifdef CONFIG_QCN_EXTN
	hostapd_set_repurpose_oper_chwidth_extn(conf, oper_chwidth);
#endif /* CONFIG_QCN_EXTN */
}

static inline u8
hostapd_get_oper_centr_freq_seg0_idx(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		return conf->eht_oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_centr_freq_seg0_idx;
}

static inline void
hostapd_set_oper_centr_freq_seg0_idx(struct hostapd_config *conf,
				     u8 oper_centr_freq_seg0_idx)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		conf->eht_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
	if (is_6ghz_op_class(conf->op_class) &&
	    center_idx_to_bw_6ghz(oper_centr_freq_seg0_idx) == 4)
		oper_centr_freq_seg0_idx +=
			conf->channel > oper_centr_freq_seg0_idx ? 16 : -16;

#ifdef CONFIG_QCN_EXTN
	oper_centr_freq_seg0_idx =
		hostapd_set_legacy_oper_centr_freq_seg0_extn(
			conf, oper_centr_freq_seg0_idx);
#endif /* CONFIG_QCN_EXTN */

#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
}

static inline u8
hostapd_get_oper_centr_freq_seg1_idx(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_centr_freq_seg1_idx;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_centr_freq_seg1_idx;
}

static inline void
hostapd_set_oper_centr_freq_seg1_idx(struct hostapd_config *conf,
				     u8 oper_centr_freq_seg1_idx)
{
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_centr_freq_seg1_idx = oper_centr_freq_seg1_idx;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_centr_freq_seg1_idx = oper_centr_freq_seg1_idx;
}

static inline u8
hostapd_get_bw320_offset(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	/* 240MHz (320 MHz with puncturing) is supported in 5GHz */
	if (!is_6ghz_op_class(conf->op_class) &&
	    hostapd_get_oper_chwidth(conf) == CONF_OPER_CHWIDTH_320MHZ)
		return conf->eht_bw320_offset;
	else if (conf->ieee80211be && is_6ghz_op_class(conf->op_class) &&
		 hostapd_get_oper_chwidth(conf) == CONF_OPER_CHWIDTH_320MHZ)
		return conf->eht_bw320_offset;
#endif /* CONFIG_IEEE80211BE */
	return 0;
}

static inline void
hostapd_set_and_check_bw320_offset(struct hostapd_config *conf,
				   u8 bw320_offset)
{
#ifdef CONFIG_IEEE80211BE
	/* 240MHz (320 MHz with puncturing) is supported in 5GHz */
	if (!is_6ghz_op_class(conf->op_class) &&
	    hostapd_get_oper_chwidth(conf) == CONF_OPER_CHWIDTH_320MHZ) {
		conf->eht_bw320_offset = bw320_offset;
	} else if (conf->ieee80211be && is_6ghz_op_class(conf->op_class) &&
		   op_class_to_ch_width(conf->op_class) == CONF_OPER_CHWIDTH_320MHZ) {
		if (conf->channel) {
			/* If the channel is set, then calculate bw320_offset
			 * by center frequency segment 0.
			 */
			u8 seg0 = hostapd_get_oper_centr_freq_seg0_idx(conf);

			conf->eht_bw320_offset = (seg0 - 31) % 64 ? 2 : 1;
		} else {
			/* If the channel is not set, bw320_offset indicates
			 * preferred offset of 320 MHz.
			 */
			conf->eht_bw320_offset = bw320_offset;
		}
	} else {
		conf->eht_bw320_offset = 0;
	}
#endif /* CONFIG_IEEE80211BE */
}


int hostapd_mac_comp(const void *a, const void *b);
struct hostapd_config * hostapd_config_defaults(void);
void hostapd_config_defaults_bss(struct hostapd_bss_config *bss);
#ifdef CONFIG_IEEE80211BN
int hostapd_mapc_config_defaults_bss(struct hostapd_bss_config *bss);
#endif /* CONFIG_IEEE80211BN */
void hostapd_config_free_radius_attr(struct hostapd_radius_attr *attr);
void hostapd_config_free_eap_user(struct hostapd_eap_user *user);
void hostapd_config_free_eap_users(struct hostapd_eap_user *user);
void hostapd_config_clear_wpa_psk(struct hostapd_wpa_psk **p);
void hostapd_config_clear_rxkhs(struct hostapd_bss_config *conf);
void hostapd_config_free_bss(struct hostapd_bss_config *conf);
void hostapd_config_free(struct hostapd_config *conf);
int hostapd_maclist_found(struct mac_acl_entry *list, int num_entries,
			  const u8 *addr, struct vlan_description *vlan_id);
int hostapd_acl_maclist_found(struct hostapd_bss_config *conf, bool accept,
			      const u8 *addr, struct vlan_description *vlan_id);
const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *p2p_dev_addr,
			   const u8 *prev_psk, int *vlan_id);
int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf);
int hostapd_vlan_valid(struct hostapd_vlan *vlan,
		       struct vlan_description *vlan_desc);
const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan,
					int vlan_id);
struct hostapd_radius_attr *
hostapd_config_get_radius_attr(struct hostapd_radius_attr *attr, u8 type);
struct hostapd_radius_attr * hostapd_parse_radius_attr(const char *value);
int hostapd_config_check(struct hostapd_config *conf, int full_config);
int hostapd_config_check_npca_config(struct hostapd_config *conf);
void hostapd_set_security_params(struct hostapd_bss_config *bss,
				 int full_config);
int hostapd_sae_pw_id_in_use(struct hostapd_bss_config *conf);
bool hostapd_sae_pk_in_use(struct hostapd_bss_config *conf);
bool hostapd_sae_pk_exclusively(struct hostapd_bss_config *conf);
int hostapd_setup_sae_pt(struct hostapd_bss_config *conf);
int hostapd_acl_comp(const void *a, const void *b);
int hostapd_add_acl_maclist(struct mac_acl_entry **acl, int *num,
			    int vlan_id, const u8 *addr);
int hostapd_add_acl_maclist_masked(struct mac_acl_entry **acl, int *num,
				   int vlan_id, const u8 *addr,
				   const u8 *mask);
void hostapd_remove_acl_mac(struct mac_acl_entry **acl, int *num,
			    const u8 *addr);
void hostapd_remove_all_masked_entries_for_addr(struct mac_acl_entry **acl,
						int *num, const u8 *addr);
void hostapd_remove_acl_mac_masked_pair(struct mac_acl_entry **acl, int *num,
					const u8 *addr, const u8 *mask);
int hostapd_acl_add_entry(struct mac_acl_entry **exact_acl, int *num_exact,
			  struct mac_acl_entry **masked_acl, int *num_masked,
			  int vlan_id, const u8 *addr, const u8 *mask);
void hostapd_acl_del_entry(struct mac_acl_entry **exact_acl, int *num_exact,
			   struct mac_acl_entry **masked_acl, int *num_masked,
			   const u8 *addr);
void hostapd_acl_clear(struct mac_acl_entry **exact_acl, int *num_exact,
		       struct mac_acl_entry **masked_acl, int *num_masked);
void hostapd_config_free_acl_timed_list(struct hostapd_bss_config *conf);
bool hostapd_config_check_bss_6g(struct hostapd_bss_config *bss);

#endif /* HOSTAPD_CONFIG_H */

bool hostapd_is_beacon_tx_rate_preamble_valid(const struct hostapd_config *iconf,
					     const struct hostapd_bss_config *bss);
