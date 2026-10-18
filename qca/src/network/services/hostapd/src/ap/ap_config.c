/*
 * hostapd / Configuration helper functions
 * Copyright (c) 2003-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "crypto/sha1.h"
#include "crypto/tls.h"
#include "radius/radius_client.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_1x_defs.h"
#include "common/eapol_common.h"
#include "common/dhcp.h"
#include "common/sae.h"
#include "eap_common/eap_wsc_common.h"
#include "eap_server/eap.h"
#include "wpa_auth.h"
#include "sta_info.h"
#include "airtime_policy.h"
#include "ap_config.h"
#include "interference.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#include "ieee802_11.h"
#include "uhr_neighbor_update.h"
#include "vlan_init.h"

#define RADIUS_CLIENT_MAX_RETRIES 10
#define RADIUS_CLIENT_MAX_WAIT	120

static void hostapd_config_free_vlan(struct hostapd_bss_config *bss)
{
	struct hostapd_vlan *vlan, *prev;

	vlan = bss->vlan;
	prev = NULL;
	while (vlan) {
		prev = vlan;
		vlan = vlan->next;
		vlan_cancel_cleanup_for_vlan(prev);
		os_free(prev);
	}

	bss->vlan = NULL;
}


#ifndef DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES
#define DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES 0
#endif /* DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES */

#ifdef CONFIG_IEEE80211BN
int hostapd_mapc_config_defaults_bss(struct hostapd_bss_config *bss)
{
	bss->mapc_conf = os_zalloc(sizeof(*bss->mapc_conf));
	if (!bss->mapc_conf) {
		wpa_printf(MSG_ERROR, "failed to alloc mapc_conf");
		return -ENOMEM;
	}

	bss->mapc_conf->mapc_cotdma_enable  =
				MAPC_DEFAULT_COTDMA_ENABLE;
	bss->mapc_conf->max_mapc_ctdma_peer =
				MAPC_MAX_COTDMA_PEER;
	bss->mapc_conf->mapc_disc_req_interval_sec =
				MAPC_DEFAULT_DISCOVERY_REQUEST_INTERVAL_SEC;
	bss->mapc_conf->discovery_mode =
				MAPC_DEFAULT_DISCOVERY_MODE;
	bss->mapc_conf->negotiation_mode =
				MAPC_DEFAULT_NEGOTIATION_MODE;
	bss->mapc_conf->max_mapc_co_ap_peer =
				MAPC_MAX_CO_AP_PEER;
	bss->mapc_conf->max_mapc_discovered_ap_peer =
				MAPC_MAX_CO_AP_DISCOVERED_PEER;
	bss->mapc_conf->discovery_req_timeout =
				MAPC_DEFAULT_DISCOVERY_REQ_TIMEOUT;
	bss->mapc_conf->negotiation_req_timeout =
				MAPC_DEFAULT_NEGOTIATION_REQ_TIMEOUT;
	bss->mapc_conf->max_mapc_ap_inactivity =
				MAPC_DEFAULT_MAX_AP_INACTIVITY;
	bss->mapc_conf->enable_disc_cotdma_scheme_profile =
				MAPC_DEFAULT_COTDMA_DISCOVERY_SCHEME_PROFILE;
	return 0;
}
#endif /* CONFIG_IEEE80211BN */


void hostapd_config_defaults_bss(struct hostapd_bss_config *bss)
{
	dl_list_init(&bss->anqp_elem);
#if CONFIG_MBO
	bss->oce_tx_power = -128; /* -128 = not set, use regulatory max */
#endif
	bss->rtt_responder_role = 0;

	bss->logger_syslog_level = HOSTAPD_LEVEL_INFO;
	bss->logger_stdout_level = HOSTAPD_LEVEL_INFO;
	bss->logger_syslog = (unsigned int) -1;
	bss->logger_stdout = (unsigned int) -1;

#ifdef CONFIG_WEP
	bss->auth_algs = WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED;

	bss->wep_rekeying_period = 300;
	/* use key0 in individual key and key1 in broadcast key */
	bss->broadcast_key_idx_min = 1;
	bss->broadcast_key_idx_max = 2;
#else /* CONFIG_WEP */
	bss->auth_algs = WPA_AUTH_ALG_OPEN;
#endif /* CONFIG_WEP */
	bss->eap_reauth_period = 3600;

	bss->wpa_group_rekey = 600;
	bss->wpa_gmk_rekey = 86400;
	bss->wpa_deny_ptk0_rekey = PTK0_REKEY_ALLOW_ALWAYS;
	bss->wpa_group_update_count = 4;
	bss->wpa_pairwise_update_count = 4;
	bss->wpa_disable_eapol_key_retries =
		DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES;
	bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
#ifdef CONFIG_NO_TKIP
	bss->wpa_pairwise = WPA_CIPHER_CCMP;
	bss->wpa_group = WPA_CIPHER_CCMP;
#else /* CONFIG_NO_TKIP */
	bss->wpa_pairwise = WPA_CIPHER_TKIP;
	bss->wpa_group = WPA_CIPHER_TKIP;
#endif /* CONFIG_NO_TKIP */
	bss->rsn_pairwise = 0;

	bss->max_num_sta = MAX_STA_COUNT;
	bss->acl_deny_wait_time = SOFTBLOCK_WAIT_TIME_DEFAULT;
	bss->acl_deny_allow_time = SOFTBLOCK_ALLOW_TIME_DEFAULT;

	bss->dtim_period = 2;

	bss->radius->radius_server_retries = RADIUS_CLIENT_MAX_RETRIES;
	bss->radius->radius_max_retry_wait = RADIUS_CLIENT_MAX_WAIT;
	bss->radius_server_auth_port = 1812;
	bss->eap_sim_db_timeout = 1;
	bss->eap_sim_id = 3;
	bss->eap_sim_aka_fast_reauth_limit = 1000;
	bss->ap_max_inactivity = AP_MAX_INACTIVITY;
	bss->bss_max_idle = 1;
	bss->eapol_version = EAPOL_VERSION;

	bss->max_listen_interval = 65535;

	bss->pwd_group = 19; /* ECC: GF(p=256) */

	bss->assoc_sa_query_max_timeout = 1000;
	bss->assoc_sa_query_retry_timeout = 201;
	bss->disable_sa_query = 0;
	bss->group_mgmt_cipher = WPA_CIPHER_AES_128_CMAC;

#ifdef CONFIG_IEEE80211BN
	/* SMD Neighbor Update defaults */
	bss->smd_neighbor_update_enabled = 0;
	bss->smd_neighbor_expiry_time = SMD_NEIGHBOR_ENTRY_EXPIRE_SEC;
	bss->smd_neighbor_pull_interval = SMD_NEIGHBOR_PULL_PERIOD_SEC;
#endif

#ifdef EAP_SERVER_FAST
	 /* both anonymous and authenticated provisioning */
	bss->eap_fast_prov = 3;
	bss->pac_key_lifetime = 7 * 24 * 60 * 60;
	bss->pac_key_refresh_time = 1 * 24 * 60 * 60;
#endif /* EAP_SERVER_FAST */

	/* Set to -1 as defaults depends on HT in setup */
	bss->wmm_enabled = -1;

#ifdef CONFIG_QCN_EXTN
	/* Not set by default; use runtime available BSS index */
	bss->bss_index = -1;
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211R_AP
	bss->ft_over_ds = 1;
	bss->rkh_pos_timeout = 86400;
	bss->rkh_neg_timeout = 60;
	bss->rkh_pull_timeout = 1000;
	bss->rkh_pull_retries = 4;
	bss->r0_key_lifetime = 1209600;
#endif /* CONFIG_IEEE80211R_AP */

	bss->radius_das_time_window = 300;
	bss->radius_require_message_authenticator = 1;
	bss->identity_request_retry_interval = 0;

	bss->anti_clogging_threshold = 5;
	bss->sae_sync = 3;

	bss->gas_frag_limit = 1400;

#ifdef CONFIG_FILS
	dl_list_init(&bss->fils_realms);
	bss->fils_hlp_wait_time = 30;
	bss->dhcp_server_port = DHCP_SERVER_PORT;
	bss->dhcp_relay_port = DHCP_SERVER_PORT;
	bss->fils_discovery_min_int = 20;
#endif /* CONFIG_FILS */

	bss->broadcast_deauth = 1;

#ifdef CONFIG_MBO
	bss->mbo_cell_data_conn_pref = -1;
#endif /* CONFIG_MBO */

	/* Disable TLS v1.3 by default for now to avoid interoperability issue.
	 * This can be enabled by default once the implementation has been fully
	 * completed and tested with other implementations. */
	bss->tls_flags = TLS_CONN_DISABLE_TLSv1_3;

	bss->max_auth_rounds = 100;
	bss->max_auth_rounds_short = 50;

	bss->send_probe_response = 1;

#ifdef CONFIG_HS20
	bss->hs20_release = (HS20_VERSION >> 4) + 1;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MACSEC
	bss->mka_priority = DEFAULT_PRIO_NOT_KEY_SERVER;
	bss->macsec_port = 1;
#endif /* CONFIG_MACSEC */

	/* Default to strict CRL checking. */
	bss->check_crl_strict = 1;

	bss->multi_ap_profile = MULTI_AP_PROFILE_2;

#ifdef CONFIG_TESTING_OPTIONS
	bss->sae_commit_status = -1;
	bss->test_assoc_comeback_type = -1;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_PASN
	/* comeback after 10 TUs */
	bss->pasn_comeback_after = 10;
	bss->pasn_noauth = 1;
#endif /* CONFIG_PASN */
	bss->urnm_mfpr_x20 = -1;
	bss->urnm_mfpr = -1;
	bss->force_disable_in_band_discovery = 1;

	bss->wmm_override = false;
#define ecw2cw(ecw) ((1 << (ecw)) - 1)
	const int aCWmin = 4, aCWmax = 10;
	const struct hostapd_wmm_ac_params ac_bk =
		{ aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
	const struct hostapd_wmm_ac_params ac_be =
		{ aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
	const struct hostapd_wmm_ac_params ac_vi = /* video traffic */
		{ aCWmin - 1, aCWmin, 2, 3008 / 32, 0 };
	const struct hostapd_wmm_ac_params ac_vo = /* voice traffic */
		{ aCWmin - 2, aCWmin - 1, 2, 1504 / 32, 0 };
#undef ecw2cw
	bss->wmm_ac_params[0] = ac_be;
	bss->wmm_ac_params[1] = ac_bk;
	bss->wmm_ac_params[2] = ac_vi;
	bss->wmm_ac_params[3] = ac_vo;
	bss->enable_dscp_policy_capa = false;
	bss->twt_responder_caps = TWT_ITWT_ENABLED;
	bss->bss_priority = 0;
	bss->bss_priority_status = 0;

#ifdef CONFIG_IEEE80211AC
	/* 0 means not set by user; will use hardware supported map by default */
	bss->vht_mcs_nss_set = 0;
	bss->vht_capab = 0;
	bss->vht_capab_mask = 0;
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	os_memset(&bss->he_phy_capab, 0, sizeof(bss->he_phy_capab));
	bss->he_phy_capab_mask = 0;
	os_memset(bss->he_tx_mcs_nss_set, 0xff,
		  sizeof(bss->he_tx_mcs_nss_set));
	os_memset(bss->he_rx_mcs_nss_set, 0xff,
		  sizeof(bss->he_rx_mcs_nss_set));
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	os_memset(bss->eht_tx_mcs_nss_set, 0xff, sizeof(bss->eht_tx_mcs_nss_set));
	os_memset(bss->eht_rx_mcs_nss_set, 0xff, sizeof(bss->eht_rx_mcs_nss_set));
	os_memset(&bss->eht_phy_capab, 0, sizeof(bss->eht_phy_capab));
	bss->eht_phy_capab_mask = 0;
	bss->eht_phy_capab.eht_mu_bfmr_mask = 0x7;
	bss->eht_phy_capab.eht_mu_mimo_mask = 0x7;
	bss->eht_phy_capab.mu_beamformer = 1;

	bss->single_link_emlsr = false;
	bss->disable_eml = false;
	bss->skip_uhr_extn = SKIP_UHR_EXTN_RNR;
	bss->mld_link_id = -1; /* -1 = auto-allocate */
#endif /* CONFIG_IEEE80211BE */
	bss->ht_mcs_nss_set = 0;
	bss->group_control_frame_cipher = WPA_CIPHER_BIP_GMAC_256;
	/* Default: do not gate EAPOL M3 (can be enabled per-BSS config) */
	bss->externally_triggered_m3 = 0;
	bss->plugin_eap_offload = 0;
	bss->plugin_eapol_key_offload = 0;

#ifdef HOSTAPD_EXTERNAL_PLUGIN
	/* Default: external plugin disabled (can be enabled per-BSS config) */
	bss->external_plugin_enable = 0;
#endif

#ifdef CONFIG_IEEE80211BN
	bss->uhr_phy_capab.uhr_2xldpc_tx = 1;
	bss->uhr_phy_capab.uhr_2xldpc_rx = 1;
	bss->dps_assist = FEATURE_ENABLED;

	/* UHR intervals are represented as TUs */
	bss->uhr_params_update.adv_notification_interval = 10;
	bss->uhr_params_update.update_in_tim_interval = 10;
#endif /* CONFIG_IEEE80211BN */

	/* This max size includes wmm and user configured vendor elements */
	bss->available_vendor_elem_size = MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE;

#ifdef CONFIG_TESTING_OPTIONS
	bss->rsnxe_capab_mask = ~0ULL;
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_ENC_ASSOC
	bss->assoc_frame_encryption = 0;
	bss->pmksa_caching_privacy = 0;
	bss->eap_using_authentication_frames = 0;
#endif /* CONFIG_ENC_ASSOC */

	bss->report_connection_failures = 0;
	bss->vlan_idle_cleanup = 30;
}

#ifdef CONFIG_IEEE80211BE
static void hostapd_set_default_epcs_params(struct hostapd_bss_config *bss)
{
	bss->epcs_he_mu_edca.he_mu_ac_be_param[HE_MU_AC_PARAM_ACI_IDX] = 0x03;
	bss->epcs_he_mu_edca.he_mu_ac_be_param[HE_MU_AC_PARAM_ECW_IDX] = 0xA4;
	bss->epcs_he_mu_edca.he_mu_ac_be_param[HE_MU_AC_PARAM_TIMER_IDX] = 255;

	bss->epcs_he_mu_edca.he_mu_ac_bk_param[HE_MU_AC_PARAM_ACI_IDX] = 0x27;
	bss->epcs_he_mu_edca.he_mu_ac_bk_param[HE_MU_AC_PARAM_ECW_IDX] = 0xA4;
	bss->epcs_he_mu_edca.he_mu_ac_bk_param[HE_MU_AC_PARAM_TIMER_IDX] = 255;

	bss->epcs_he_mu_edca.he_mu_ac_vi_param[HE_MU_AC_PARAM_ACI_IDX] = 0x42;
	bss->epcs_he_mu_edca.he_mu_ac_vi_param[HE_MU_AC_PARAM_ECW_IDX] = 0x43;
	bss->epcs_he_mu_edca.he_mu_ac_vi_param[HE_MU_AC_PARAM_TIMER_IDX] = 255;

	bss->epcs_he_mu_edca.he_mu_ac_vo_param[HE_MU_AC_PARAM_ACI_IDX] = 0x62;
	bss->epcs_he_mu_edca.he_mu_ac_vo_param[HE_MU_AC_PARAM_ECW_IDX] = 0x32;
	bss->epcs_he_mu_edca.he_mu_ac_vo_param[HE_MU_AC_PARAM_TIMER_IDX] = 255;

	bss->epcs_wmm_ac_params[WMM_AC_BE].cwmin = 4;
	bss->epcs_wmm_ac_params[WMM_AC_BE].cwmax = 9;
	bss->epcs_wmm_ac_params[WMM_AC_BE].aifs = 3;
	bss->epcs_wmm_ac_params[WMM_AC_BE].txop_limit = 0;

	bss->epcs_wmm_ac_params[WMM_AC_BK].cwmin = 4;
	bss->epcs_wmm_ac_params[WMM_AC_BK].cwmax = 9;
	bss->epcs_wmm_ac_params[WMM_AC_BK].aifs = 7;
	bss->epcs_wmm_ac_params[WMM_AC_BK].txop_limit = 0;

	bss->epcs_wmm_ac_params[WMM_AC_VI].cwmin = 3;
	bss->epcs_wmm_ac_params[WMM_AC_VI].cwmax = 4;
	bss->epcs_wmm_ac_params[WMM_AC_VI].aifs = 2;
	bss->epcs_wmm_ac_params[WMM_AC_VI].txop_limit = 188;

	bss->epcs_wmm_ac_params[WMM_AC_VO].cwmin = 2;
	bss->epcs_wmm_ac_params[WMM_AC_VO].cwmax = 3;
	bss->epcs_wmm_ac_params[WMM_AC_VO].aifs = 2;
	bss->epcs_wmm_ac_params[WMM_AC_VO].txop_limit = 102;
}
#endif /* CONFIG_IEEE80211BE */

struct hostapd_config * hostapd_config_defaults(void)
{
#define ecw2cw(ecw) ((1 << (ecw)) - 1)

	struct hostapd_config *conf;
	struct hostapd_bss_config *bss;
	const int aCWmin = 4, aCWmax = 10;
	const struct hostapd_wmm_ac_params ac_bk =
		{ aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
	const struct hostapd_wmm_ac_params ac_be =
		{ aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
	const struct hostapd_wmm_ac_params ac_vi = /* video traffic */
		{ aCWmin - 1, aCWmin, 2, 3008 / 32, 0 };
	const struct hostapd_wmm_ac_params ac_vo = /* voice traffic */
		{ aCWmin - 2, aCWmin - 1, 2, 1504 / 32, 0 };
	const struct hostapd_tx_queue_params txq_bk =
		{ 7, ecw2cw(aCWmin), ecw2cw(aCWmax), 0, 0, 0 };
	const struct hostapd_tx_queue_params txq_be =
		{ 3, ecw2cw(aCWmin), 4 * (ecw2cw(aCWmin) + 1) - 1, 0, 0, 0};
	const struct hostapd_tx_queue_params txq_vi =
		{ 1, (ecw2cw(aCWmin) + 1) / 2 - 1, ecw2cw(aCWmin), 30, 0, 0};
	const struct hostapd_tx_queue_params txq_vo =
		{ 1, (ecw2cw(aCWmin) + 1) / 4 - 1,
		  (ecw2cw(aCWmin) + 1) / 2 - 1, 15, 0, 0};

#undef ecw2cw

	conf = os_zalloc(sizeof(*conf));
	bss = os_zalloc(sizeof(*bss));
	if (conf == NULL || bss == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for "
			   "configuration data.");
		os_free(conf);
		os_free(bss);
		return NULL;
	}
	conf->bss = os_calloc(1, sizeof(struct hostapd_bss_config *));
	if (conf->bss == NULL) {
		os_free(conf);
		os_free(bss);
		return NULL;
	}
	conf->bss[0] = bss;

	bss->radius = os_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		os_free(conf->bss);
		os_free(conf);
		os_free(bss);
		return NULL;
	}

#ifdef CONFIG_IEEE80211BN
	if (hostapd_mapc_config_defaults_bss(bss) < 0) {
		os_free(bss->radius);
		os_free(conf->bss);
		os_free(conf);
		os_free(bss);
		return NULL;
	}
#endif /* CONFIG_IEEE80211BN */

	hostapd_config_defaults_bss(bss);
#ifdef CONFIG_QCN_EXTN
	hostapd_config_defaults_bss_extn(bss);
#endif

	/* Security IE defaults */
	bss->security_profiles = NULL;
	bss->security_profile_ext_key_id = 0;
	bss->security_profile_ocvc = 0;

	conf->num_bss = 1;

	conf->beacon_int = 100;
	conf->rts_threshold = -2; /* use driver default: 2347 */
	conf->fragm_threshold = -2; /* user driver default: 2346 */
	/* Set to invalid value means do not add Power Constraint IE */
	conf->local_pwr_constraint = -1;

	conf->wmm_ac_params[0] = ac_be;
	conf->wmm_ac_params[1] = ac_bk;
	conf->wmm_ac_params[2] = ac_vi;
	conf->wmm_ac_params[3] = ac_vo;

	conf->tx_queue[0] = txq_vo;
	conf->tx_queue[1] = txq_vi;
	conf->tx_queue[2] = txq_be;
	conf->tx_queue[3] = txq_bk;

	conf->ht_capab = HT_CAP_INFO_SMPS_DISABLED;

	conf->ap_table_max_size = 255;
	conf->ap_table_expiration_time = 60;
	conf->track_sta_max_age = 180;

#ifdef CONFIG_TESTING_OPTIONS
	conf->ignore_probe_probability = 0.0;
	conf->ignore_auth_probability = 0.0;
	conf->ignore_assoc_probability = 0.0;
	conf->ignore_reassoc_probability = 0.0;
	conf->corrupt_gtk_rekey_mic_probability = 0.0;
	conf->ecsa_ie_only = 0;
#endif /* CONFIG_TESTING_OPTIONS */

	conf->acs = 0;
	conf->acs_ch_list.num = 0;
#ifdef CONFIG_ACS
	conf->acs_num_scans = 5;
	conf->acs_scan_retry_interval = 5;
	conf->acs_scan_retry_max_count = 25;
	conf->acs_enable_bw_downgrade = 0;
	conf->radio_idx = -1;
#endif /* CONFIG_ACS */

#ifdef CONFIG_IEEE80211AX
	conf->he_op.he_rts_threshold = HE_OPERATION_RTS_THRESHOLD_MASK >>
		HE_OPERATION_RTS_THRESHOLD_OFFSET;
	/* Set default basic MCS/NSS set to single stream MCS 0-7 */
	conf->he_op.he_basic_mcs_nss_set = 0xfffc;
	/* Set default to be decided by Driver/underlying HW */
	conf->he_phy_capab.he_ul_mumimo = -1;
	conf->he_op.he_bss_color_disabled = 1;
	conf->he_op.he_bss_color_partial = 0;
	conf->he_op.he_bss_color = os_random() % 63 + 1;
	conf->he_op.he_bss_color_collision_detection = 1;
	conf->he_bss_color_collision_ap_period = DOT11BSS_COLOR_COLLISION_AP_PERIOD;
	conf->he_op.he_twt_responder = 1;
	conf->he_6ghz_max_mpdu = 2;
	conf->he_6ghz_max_ampdu_len_exp = 7;
	conf->he_6ghz_rx_ant_pat = 1;
	conf->he_6ghz_tx_ant_pat = 1;
	conf->discard_6g_awgn_event = 0;
	conf->he_6ghz_reg_pwr_type = HE_REG_INFO_6GHZ_AP_TYPE_VLP;
	conf->he_6ghz_min_rate = 6;
	conf->enable_best_power_mode = 1;
	conf->puncture_strict_6ghz = 0;
	conf->punc_eirp_thres_6ghz = CHAN_MIN_TX_POWER;
	conf->reg_def_cli_eirp_psd = -1;
	conf->reg_sub_cli_eirp_psd = -1;
	conf->reg_def_cli_eirp = -1;
#endif /* CONFIG_IEEE80211AX */

	/* The third octet of the country string uses an ASCII space character
	 * by default to indicate that the regulations encompass all
	 * environments for the current frequency band in the country. */
	conf->country[2] = ' ';

	conf->rssi_reject_assoc_rssi = 0;
	conf->rssi_reject_assoc_timeout = 30;
	conf->rssi_deauth_grace_samples = 10;
	conf->rssi_probe_delay_time_window = 0;
	conf->rssi_probe_delay_req_count = 0;
	bss->rssi_reject_assoc_rssi = 0;
	bss->rssi_reject_assoc_timeout = 30;
	bss->rssi_deauth_grace_samples = 10;

#ifdef CONFIG_AIRTIME_POLICY
	conf->airtime_update_interval = AIRTIME_DEFAULT_UPDATE_INTERVAL;
#endif /* CONFIG_AIRTIME_POLICY */
	conf->group_size = MULTI_MBSSID_GROUP_SIZE_DEFAULT;
	conf->enable_6ghz_composite_ap = 1;
	conf->cur_chan_eirp = CHAN_MIN_EIRP_POWER;
	conf->afc_chan_sel_config = HOSTAPD_AFC_CHAN_SEL_ALL;
	conf->original_chan_width = 0;
	conf->post_nol_freq = -1;
	conf->post_nol_width = -1;
	conf->post_nol_bgcac_en = 1;
	conf->next_radar_freq = -1;
	conf->next_radar_width = -1;

	hostapd_set_and_check_bw320_offset(conf, 0);
#ifdef CONFIG_QCN_EXTN
	hostapd_config_defaults_extn(conf);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BE
	/* set ML max rec links as Invalid */
	bss->ml_max_rec_links = ML_IE_MAX_REC_LINKS_INVAL;
	conf->eht_phy_capab.mu_beamformer = true;
	hostapd_set_default_epcs_params(bss);
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	conf->npca_enable = 0;
	conf->npca_primary_channel = 0;
	conf->npca_punct_bitmap = 0;
#endif /* CONFIG_IEEE80211BN */

	bss->rate_type = BEACON_RATE_LEGACY;
	bss->beacon_rate = 0;
#ifdef CONFIG_QCN_EXTN
       conf->downgrade_320mhz_opclass = true;
#endif

	return conf;
}


int hostapd_mac_comp(const void *a, const void *b)
{
	return os_memcmp(a, b, sizeof(macaddr));
}


static int hostapd_config_read_wpa_psk(const char *fname,
				       struct hostapd_ssid *ssid)
{
	FILE *f;
	char buf[128], *pos;
	const char *keyid;
	char *context;
	char *context2;
	char *token;
	char *name;
	char *value;
	int line = 0, ret = 0, len, ok;
	u8 addr[ETH_ALEN];
	struct hostapd_wpa_psk *psk;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "WPA PSK file '%s' not found.", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		int vlan_id = 0;
		int wps = 0;

		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		context = NULL;
		keyid = NULL;
		while ((token = str_token(buf, " ", &context))) {
			if (!os_strchr(token, '='))
				break;
			context2 = NULL;
			name = str_token(token, "=", &context2);
			if (!name)
				break;
			value = str_token(token, "", &context2);
			if (!value)
				value = "";
			if (!os_strcmp(name, "keyid")) {
				keyid = value;
			} else if (!os_strcmp(name, "wps")) {
				wps = atoi(value);
			} else if (!os_strcmp(name, "vlanid")) {
				vlan_id = atoi(value);
			} else {
				wpa_printf(MSG_ERROR,
					   "Unrecognized '%s=%s' on line %d in '%s'",
					   name, value, line, fname);
				ret = -1;
				break;
			}
		}

		if (ret == -1)
			break;

		if (!token)
			token = "";
		if (hwaddr_aton(token, addr)) {
			wpa_printf(MSG_ERROR,
				   "Invalid MAC address '%s' on line %d in '%s'",
				   token, line, fname);
			ret = -1;
			break;
		}

		psk = os_zalloc(sizeof(*psk));
		if (psk == NULL) {
			wpa_printf(MSG_ERROR, "WPA PSK allocation failed");
			ret = -1;
			break;
		}
		psk->vlan_id = vlan_id;
		if (is_zero_ether_addr(addr))
			psk->group = 1;
		else
			os_memcpy(psk->addr, addr, ETH_ALEN);

		pos = str_token(buf, "", &context);
		if (!pos) {
			wpa_printf(MSG_ERROR, "No PSK on line %d in '%s'",
				   line, fname);
			os_free(psk);
			ret = -1;
			break;
		}

		ok = 0;
		len = os_strlen(pos);
		if (len == 2 * PMK_LEN &&
		    hexstr2bin(pos, psk->psk, PMK_LEN) == 0)
			ok = 1;
		else if (len >= 8 && len < 64 &&
			 pbkdf2_sha1(pos, ssid->ssid, ssid->ssid_len,
				     4096, psk->psk, PMK_LEN) == 0)
			ok = 1;
		if (!ok) {
			wpa_printf(MSG_ERROR,
				   "Invalid PSK '%s' on line %d in '%s'",
				   pos, line, fname);
			os_free(psk);
			ret = -1;
			break;
		}

		if (keyid) {
			len = os_strlcpy(psk->keyid, keyid, sizeof(psk->keyid));
			if ((size_t) len >= sizeof(psk->keyid)) {
				wpa_printf(MSG_ERROR,
					   "PSK keyid too long on line %d in '%s'",
					   line, fname);
				os_free(psk);
				ret = -1;
				break;
			}
		}

		psk->wps = wps;

		psk->next = ssid->wpa_psk;
		ssid->wpa_psk = psk;
	}

	fclose(f);

	return ret;
}


static int hostapd_derive_psk(struct hostapd_ssid *ssid)
{
	ssid->wpa_psk = os_zalloc(sizeof(struct hostapd_wpa_psk));
	if (ssid->wpa_psk == NULL) {
		wpa_printf(MSG_ERROR, "Unable to alloc space for PSK");
		return -1;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "SSID",
			  (u8 *) ssid->ssid, ssid->ssid_len);
	wpa_hexdump_ascii_key(MSG_DEBUG, "PSK (ASCII passphrase)",
			      (u8 *) ssid->wpa_passphrase,
			      os_strlen(ssid->wpa_passphrase));
	if (pbkdf2_sha1(ssid->wpa_passphrase,
			ssid->ssid, ssid->ssid_len,
			4096, ssid->wpa_psk->psk, PMK_LEN) != 0) {
		wpa_printf(MSG_ERROR, "Error in pbkdf2_sha1()");
		return -1;
	}
	wpa_hexdump_key(MSG_DEBUG, "PSK (from passphrase)",
			ssid->wpa_psk->psk, PMK_LEN);
	return 0;
}


/*
 * ap_sp_implied_key_mgmt - Derive implied key_mgmt from security profiles
 *
 * When security_profiles[] contains a SAE-EXT-KEY profile (1, 2, 9, or 10),
 * the AP must prepare SAE PT even if wpa_key_mgmt does not explicitly include
 * SAE-EXT-KEY.  Returns WPA_KEY_MGMT_SAE_EXT_KEY | WPA_KEY_MGMT_FT_SAE_EXT_KEY
 * when such a profile is configured, 0 otherwise.
 */
static int ap_sp_implied_key_mgmt(const struct hostapd_bss_config *conf)
{
	int i;

	if (!conf->security_profiles)
		return 0;
	for (i = 0; conf->security_profiles[i] >= 0; i++) {
		int p = conf->security_profiles[i];

		if (p == SECURITY_PROFILE_NUM_EPPKE_SAE ||
		    p == SECURITY_PROFILE_NUM_EPPKE_FT_SAE ||
		    p == SECURITY_PROFILE_NUM_SAE ||
		    p == SECURITY_PROFILE_NUM_FT_SAE)
			return WPA_KEY_MGMT_SAE_EXT_KEY |
			       WPA_KEY_MGMT_FT_SAE_EXT_KEY;
	}
	return 0;
}

int hostapd_setup_sae_pt(struct hostapd_bss_config *conf)
{
#ifdef CONFIG_SAE
	struct hostapd_ssid *ssid = &conf->ssid;
	struct sae_password_entry *pw;
	int *groups = conf->sae_groups;
	int default_groups[] = { 19, 0, 0 };

	if ((conf->sae_pwe == SAE_PWE_HUNT_AND_PECK &&
	     !hostapd_sae_pw_id_in_use(conf) &&
	     !wpa_key_mgmt_sae_ext_key(conf->wpa_key_mgmt |
				       conf->rsn_override_key_mgmt |
				       conf->rsn_override_key_mgmt_2 |
				       ap_sp_implied_key_mgmt(conf)) &&
	     !hostapd_sae_pk_in_use(conf)) ||
	    conf->sae_pwe == SAE_PWE_FORCE_HUNT_AND_PECK ||
	    !wpa_key_mgmt_sae(conf->wpa_key_mgmt |
			      conf->rsn_override_key_mgmt |
			      conf->rsn_override_key_mgmt_2 |
			      ap_sp_implied_key_mgmt(conf)))
		return 0; /* PT not needed */

	if (!groups) {
		groups = default_groups;
		if (wpa_key_mgmt_sae_ext_key(conf->wpa_key_mgmt |
					     conf->rsn_override_key_mgmt |
					     conf->rsn_override_key_mgmt_2 |
					     ap_sp_implied_key_mgmt(conf)))
			default_groups[1] = 20;
	}

	sae_deinit_pt(ssid->pt);
	ssid->pt = NULL;
	if (ssid->wpa_passphrase) {
		ssid->pt = sae_derive_pt(groups, ssid->ssid, ssid->ssid_len,
					 (const u8 *) ssid->wpa_passphrase,
					 os_strlen(ssid->wpa_passphrase),
					 NULL, 0);
		if (!ssid->pt)
			return -1;
	}

	for (pw = conf->sae_passwords; pw; pw = pw->next) {
		sae_deinit_pt(pw->pt);
		pw->pt = sae_derive_pt(groups, ssid->ssid, ssid->ssid_len,
				       (const u8 *) pw->password,
				       os_strlen(pw->password),
				       (const u8 *) pw->identifier,
				       pw->identifier ?
				       os_strlen(pw->identifier) : 0);
		if (!pw->pt)
			return -1;
	}
#endif /* CONFIG_SAE */

	return 0;
}


int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf)
{
	struct hostapd_ssid *ssid = &conf->ssid;

	if (hostapd_setup_sae_pt(conf) < 0)
		return -1;

	if (ssid->wpa_passphrase != NULL) {
		if (ssid->wpa_psk != NULL) {
			wpa_printf(MSG_DEBUG, "Using pre-configured WPA PSK "
				   "instead of passphrase");
		} else {
			wpa_printf(MSG_DEBUG, "Deriving WPA PSK based on "
				   "passphrase");
			if (hostapd_derive_psk(ssid) < 0)
				return -1;
		}
		ssid->wpa_psk->group = 1;
	}

	return hostapd_config_read_wpa_psk(ssid->wpa_psk_file, &conf->ssid);
}


static void hostapd_config_free_radius(struct hostapd_radius_server *servers,
				       int num_servers)
{
	int i;

	for (i = 0; i < num_servers; i++) {
		os_free(servers[i].shared_secret);
		os_free(servers[i].ca_cert);
		os_free(servers[i].client_cert);
		os_free(servers[i].private_key);
		os_free(servers[i].private_key_passwd);
	}
	os_free(servers);
}


struct hostapd_radius_attr *
hostapd_config_get_radius_attr(struct hostapd_radius_attr *attr, u8 type)
{
	for (; attr; attr = attr->next) {
		if (attr->type == type)
			return attr;
	}
	return NULL;
}


struct hostapd_radius_attr * hostapd_parse_radius_attr(const char *value)
{
	const char *pos;
	char syntax;
	struct hostapd_radius_attr *attr;
	size_t len;

	attr = os_zalloc(sizeof(*attr));
	if (!attr)
		return NULL;

	attr->type = atoi(value);

	pos = os_strchr(value, ':');
	if (!pos) {
		attr->val = wpabuf_alloc(1);
		if (!attr->val) {
			os_free(attr);
			return NULL;
		}
		wpabuf_put_u8(attr->val, 0);
		return attr;
	}

	pos++;
	if (pos[0] == '\0' || pos[1] != ':') {
		os_free(attr);
		return NULL;
	}
	syntax = *pos++;
	pos++;

	switch (syntax) {
	case 's':
		attr->val = wpabuf_alloc_copy(pos, os_strlen(pos));
		break;
	case 'x':
		len = os_strlen(pos);
		if (len & 1)
			break;
		len /= 2;
		attr->val = wpabuf_alloc(len);
		if (!attr->val)
			break;
		if (hexstr2bin(pos, wpabuf_put(attr->val, len), len) < 0) {
			wpabuf_free(attr->val);
			os_free(attr);
			return NULL;
		}
		break;
	case 'd':
		attr->val = wpabuf_alloc(4);
		if (attr->val)
			wpabuf_put_be32(attr->val, atoi(pos));
		break;
	default:
		os_free(attr);
		return NULL;
	}

	if (!attr->val) {
		os_free(attr);
		return NULL;
	}

	return attr;
}


void hostapd_config_free_radius_attr(struct hostapd_radius_attr *attr)
{
	struct hostapd_radius_attr *prev;

	while (attr) {
		prev = attr;
		attr = attr->next;
		wpabuf_free(prev->val);
		os_free(prev);
	}
}


void hostapd_config_free_eap_user(struct hostapd_eap_user *user)
{
	hostapd_config_free_radius_attr(user->accept_attr);
	os_free(user->identity);
	bin_clear_free(user->password, user->password_len);
	bin_clear_free(user->salt, user->salt_len);
	os_free(user);
}


void hostapd_config_free_eap_users(struct hostapd_eap_user *user)
{
	struct hostapd_eap_user *prev_user;

	while (user) {
		prev_user = user;
		user = user->next;
		hostapd_config_free_eap_user(prev_user);
	}
}


#ifdef CONFIG_WEP
static void hostapd_config_free_wep(struct hostapd_wep_keys *keys)
{
	int i;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		bin_clear_free(keys->key[i], keys->len[i]);
		keys->key[i] = NULL;
	}
}
#endif /* CONFIG_WEP */


void hostapd_config_clear_wpa_psk(struct hostapd_wpa_psk **l)
{
	struct hostapd_wpa_psk *psk, *tmp;

	for (psk = *l; psk;) {
		tmp = psk;
		psk = psk->next;
		bin_clear_free(tmp, sizeof(*tmp));
	}
	*l = NULL;
}


#ifdef CONFIG_IEEE80211R_AP

void hostapd_config_clear_rxkhs(struct hostapd_bss_config *conf)
{
	struct ft_remote_r0kh *r0kh, *r0kh_prev;
	struct ft_remote_r1kh *r1kh, *r1kh_prev;

	r0kh = conf->r0kh_list;
	conf->r0kh_list = NULL;
	while (r0kh) {
		r0kh_prev = r0kh;
		r0kh = r0kh->next;
		os_free(r0kh_prev);
	}

	r1kh = conf->r1kh_list;
	conf->r1kh_list = NULL;
	while (r1kh) {
		r1kh_prev = r1kh;
		r1kh = r1kh->next;
		os_free(r1kh_prev);
	}
}

#endif /* CONFIG_IEEE80211R_AP */


static void hostapd_config_free_anqp_elem(struct hostapd_bss_config *conf)
{
	struct anqp_element *elem;

	while ((elem = dl_list_first(&conf->anqp_elem, struct anqp_element,
				     list))) {
		dl_list_del(&elem->list);
		wpabuf_free(elem->payload);
		os_free(elem);
	}
}


static void hostapd_config_free_fils_realms(struct hostapd_bss_config *conf)
{
#ifdef CONFIG_FILS
	struct fils_realm *realm;

	while ((realm = dl_list_first(&conf->fils_realms, struct fils_realm,
				      list))) {
		dl_list_del(&realm->list);
		os_free(realm);
	}
#endif /* CONFIG_FILS */
}


static void hostapd_config_free_sae_passwords(struct hostapd_bss_config *conf)
{
	struct sae_password_entry *pw, *tmp;

	pw = conf->sae_passwords;
	conf->sae_passwords = NULL;
	while (pw) {
		tmp = pw;
		pw = pw->next;
		str_clear_free(tmp->password);
		os_free(tmp->identifier);
#ifdef CONFIG_SAE
		sae_deinit_pt(tmp->pt);
#endif /* CONFIG_SAE */
#ifdef CONFIG_SAE_PK
		sae_deinit_pk(tmp->pk);
#endif /* CONFIG_SAE_PK */
		os_free(tmp->success_mac);
		os_free(tmp->fail_mac);
		os_free(tmp);
	}
}


#ifdef CONFIG_DPP2
static void hostapd_dpp_controller_conf_free(struct dpp_controller_conf *conf)
{
	struct dpp_controller_conf *prev;

	while (conf) {
		prev = conf;
		conf = conf->next;
		os_free(prev);
	}
}
#endif /* CONFIG_DPP2 */


void hostapd_config_free_bss(struct hostapd_bss_config *conf)
{
	size_t i;

	if (conf == NULL)
		return;

	hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);

	str_clear_free(conf->ssid.wpa_passphrase);
	os_free(conf->ssid.wpa_psk_file);
#ifdef CONFIG_WEP
	hostapd_config_free_wep(&conf->ssid.wep);
#endif /* CONFIG_WEP */
#ifdef CONFIG_FULL_DYNAMIC_VLAN
	os_free(conf->ssid.vlan_tagged_interface);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
#ifdef CONFIG_SAE
	sae_deinit_pt(conf->ssid.pt);
#endif /* CONFIG_SAE */

	hostapd_config_free_eap_users(conf->eap_user);
	os_free(conf->eap_user_sqlite);

	os_free(conf->eap_req_id_text);
	os_free(conf->erp_domain);
	os_free(conf->accept_mac);
	os_free(conf->accept_mac_masked);
	os_free(conf->deny_mac);
	os_free(conf->deny_mac_masked);
	hostapd_config_free_acl_timed_list(conf);
	os_free(conf->nas_identifier);
	if (conf->radius) {
		hostapd_config_free_radius(conf->radius->auth_servers,
					   conf->radius->num_auth_servers);
		hostapd_config_free_radius(conf->radius->acct_servers,
					   conf->radius->num_acct_servers);
		os_free(conf->radius->force_client_dev);
	}
	hostapd_config_free_radius_attr(conf->radius_auth_req_attr);
	hostapd_config_free_radius_attr(conf->radius_acct_req_attr);
	os_free(conf->radius_req_attr_sqlite);
	os_free(conf->rsn_preauth_interfaces);
	os_free(conf->ctrl_interface);
	os_free(conf->config_id);
	os_free(conf->ca_cert);
	os_free(conf->server_cert);
	os_free(conf->server_cert2);
	os_free(conf->private_key);
	os_free(conf->private_key2);
	os_free(conf->private_key_passwd);
	os_free(conf->private_key_passwd2);
	os_free(conf->check_cert_subject);
	os_free(conf->ocsp_stapling_response);
	os_free(conf->ocsp_stapling_response_multi);
	os_free(conf->dh_file);
	os_free(conf->openssl_ciphers);
	os_free(conf->openssl_ecdh_curves);
	os_free(conf->pac_opaque_encr_key);
	os_free(conf->eap_fast_a_id);
	os_free(conf->eap_fast_a_id_info);
	os_free(conf->eap_sim_db);
	os_free(conf->imsi_privacy_key);
	os_free(conf->radius_server_clients);
	os_free(conf->radius);
	os_free(conf->radius_das_shared_secret);
	hostapd_config_free_vlan(conf);
	os_free(conf->time_zone);
	os_free(conf->supported_rates);
	os_free(conf->basic_rates);

#ifdef CONFIG_IEEE80211R_AP
	hostapd_config_clear_rxkhs(conf);
	os_free(conf->rxkh_file);
	conf->rxkh_file = NULL;
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_WPS
	os_free(conf->wps_pin_requests);
	os_free(conf->device_name);
	os_free(conf->manufacturer);
	os_free(conf->model_name);
	os_free(conf->model_number);
	os_free(conf->serial_number);
	os_free(conf->config_methods);
	os_free(conf->ap_pin);
	os_free(conf->extra_cred);
	os_free(conf->ap_settings);
	hostapd_config_clear_wpa_psk(&conf->multi_ap_backhaul_ssid.wpa_psk);
	str_clear_free(conf->multi_ap_backhaul_ssid.wpa_passphrase);
	os_free(conf->upnp_iface);
	os_free(conf->friendly_name);
	os_free(conf->manufacturer_url);
	os_free(conf->model_description);
	os_free(conf->model_url);
	os_free(conf->upc);
	for (i = 0; i < MAX_WPS_VENDOR_EXTENSIONS; i++)
		wpabuf_free(conf->wps_vendor_ext[i]);
	wpabuf_free(conf->wps_application_ext);
	wpabuf_free(conf->wps_nfc_dh_pubkey);
	wpabuf_free(conf->wps_nfc_dh_privkey);
	wpabuf_free(conf->wps_nfc_dev_pw);
#endif /* CONFIG_WPS */

	os_free(conf->roaming_consortium);
	os_free(conf->venue_name);
	os_free(conf->venue_url);
	os_free(conf->nai_realm_data);
	os_free(conf->network_auth_type);
	os_free(conf->anqp_3gpp_cell_net);
	os_free(conf->domain_name);
	hostapd_config_free_anqp_elem(conf);

#ifdef CONFIG_RADIUS_TEST
	os_free(conf->dump_msk_file);
#endif /* CONFIG_RADIUS_TEST */

#ifdef CONFIG_HS20
	os_free(conf->hs20_oper_friendly_name);
	os_free(conf->hs20_wan_metrics);
	os_free(conf->hs20_connection_capability);
	os_free(conf->hs20_operating_class);
	os_free(conf->t_c_filename);
	os_free(conf->t_c_server_url);
#endif /* CONFIG_HS20 */

	for (i = 0; i < conf->vendor_elements_count; i++)
		wpabuf_free(conf->vendor_elements[i]);
	wpabuf_free(conf->assocresp_elements);

	os_free(conf->sae_groups);
#ifdef CONFIG_OWE
	os_free(conf->owe_groups);
#endif /* CONFIG_OWE */

	os_free(conf->wowlan_triggers);

	os_free(conf->server_id);

#ifdef CONFIG_TESTING_OPTIONS
	wpabuf_free(conf->own_ie_override);
	wpabuf_free(conf->rsne_override);
	wpabuf_free(conf->rsnoe_override);
	wpabuf_free(conf->rsno2e_override);
	wpabuf_free(conf->rsnxe_override);
	wpabuf_free(conf->rsnxoe_override);
	wpabuf_free(conf->sae_commit_override);
	wpabuf_free(conf->rsne_override_eapol);
	wpabuf_free(conf->rsnxe_override_eapol);
	wpabuf_free(conf->rsne_override_ft);
	wpabuf_free(conf->rsnxe_override_ft);
	wpabuf_free(conf->gtk_rsc_override);
	wpabuf_free(conf->igtk_rsc_override);
	wpabuf_free(conf->eapol_m1_elements);
	wpabuf_free(conf->eapol_m3_elements);
	wpabuf_free(conf->presp_elements);
#endif /* CONFIG_TESTING_OPTIONS */

	os_free(conf->no_probe_resp_if_seen_on);
	os_free(conf->no_auth_if_seen_on);

	hostapd_config_free_fils_realms(conf);

#ifdef CONFIG_DPP
	os_free(conf->dpp_name);
	os_free(conf->dpp_mud_url);
	os_free(conf->dpp_extra_conf_req_name);
	os_free(conf->dpp_extra_conf_req_value);
	os_free(conf->dpp_connector);
	wpabuf_free(conf->dpp_netaccesskey);
	wpabuf_free(conf->dpp_csign);
#ifdef CONFIG_DPP2
	hostapd_dpp_controller_conf_free(conf->dpp_controller);
#endif /* CONFIG_DPP2 */
#endif /* CONFIG_DPP */

	hostapd_config_free_sae_passwords(conf);

#ifdef CONFIG_AIRTIME_POLICY
	{
		struct airtime_sta_weight *wt, *wt_prev;

		wt = conf->airtime_weight_list;
		conf->airtime_weight_list = NULL;
		while (wt) {
			wt_prev = wt;
			wt = wt->next;
			os_free(wt_prev);
		}
	}
#endif /* CONFIG_AIRTIME_POLICY */

#ifdef CONFIG_PASN
	os_free(conf->pasn_groups);
#endif /* CONFIG_PASN */

	os_free(conf->security_profiles);

	wpabuf_clear_free(conf->sae_pw_id_key);

#ifdef CONFIG_IEEE80211BN
	{
		struct smd_partner_entry *partner = conf->smd_partners;

		conf->smd_partners = NULL;
		while (partner) {
			struct smd_partner_entry *next = partner->next;

			os_free(partner);
			partner = next;
		}
	}

	os_free(conf->mapc_conf);
#endif /* CONFIG_IEEE80211BN */
	os_free(conf);
}


/**
 * hostapd_config_free - Free hostapd configuration
 * @conf: Configuration data from hostapd_config_read().
 */
void hostapd_config_free(struct hostapd_config *conf)
{
	size_t i;

	if (conf == NULL)
		return;

	for (i = 0; i < conf->num_bss; i++)
		hostapd_config_free_bss(conf->bss[i]);
	os_free(conf->bss);
	os_free(conf->acs_ch_list.range);
	os_free(conf->acs_freq_list.range);
	os_free(conf->driver_params);
#ifdef CONFIG_ACS
	os_free(conf->acs_chan_bias);
#endif /* CONFIG_ACS */
	wpabuf_free(conf->lci);
	wpabuf_free(conf->civic);

#ifdef CONFIG_ATF_OFFLOAD
	os_free(conf->atf_offload_config);
#endif /* CONFIG_ATF_OFFLOAD */

	os_free(conf);
}

/**
 * hostapd_maclist_found_with_mask - Find a MAC address using mask-aware search
 * @list: Masked MAC address list (all entries have non-trivial masks)
 * @num_entries: Number of addresses in the list
 * @addr: Address to search for
 * @vlan_id: Buffer for returning VLAN ID or %NULL if not needed
 * Returns: 1 if address is in the list or 0 if not.
 *
 * Performs O(n) linear search applying each entry's mask before comparing.
 * This function is only called for the masked list.
 */
static int hostapd_maclist_found_with_mask(struct mac_acl_entry *list,
					   int num_entries, const u8 *addr,
					   struct vlan_description *vlan_id)
{
	int i, j;
	bool match;
	u8 addr_masked;
	u8 list_masked;

	for (i = 0; i < num_entries; i++) {
		/* Apply mask and compare byte-by-byte */
		match = true;
		for (j = 0; j < ETH_ALEN; j++) {
			addr_masked = addr[j] & list[i].mask[j];
			list_masked = list[i].addr[j] & list[i].mask[j];
			if (addr_masked != list_masked) {
				match = false;
				break;
			}
		}
		if (match) {
			wpa_printf(MSG_DEBUG,
				   "ACL: MATCH FOUND (masked) - " MACSTR
				   " matches " MACSTR " with mask " MACSTR,
				   MAC2STR(addr), MAC2STR(list[i].addr),
				   MAC2STR(list[i].mask));
			if (vlan_id)
				*vlan_id = list[i].vlan_id;
			return 1;
		}
	}

	wpa_printf(MSG_DEBUG, "ACL: No match found for " MACSTR, MAC2STR(addr));
	return 0;
}

/**
 * hostapd_maclist_found - Find a MAC address from a list
 * @list: MAC address list
 * @num_entries: Number of addresses in the list
 * @addr: Address to search for
 * @vlan_id: Buffer for returning VLAN ID or %NULL if not needed
 * Returns: 1 if address is in the list or 0 if not.
 *
 * Perform a binary search for given MAC address from a pre-sorted list.
 */
int hostapd_maclist_found(struct mac_acl_entry *list, int num_entries,
			  const u8 *addr, struct vlan_description *vlan_id)
{
	int start, end, middle, res;

	start = 0;
	end = num_entries - 1;

	while (start <= end) {
		middle = (start + end) / 2;
		res = os_memcmp(list[middle].addr, addr, ETH_ALEN);
		if (res == 0) {
			if (vlan_id)
				*vlan_id = list[middle].vlan_id;
			return 1;
		}
		if (res < 0)
			start = middle + 1;
		else
			end = middle - 1;
	}

	return 0;
}


/**
 * hostapd_acl_maclist_found - Search both exact and masked accept/deny lists
 * @conf: BSS configuration containing the accept/deny MAC ACL lists
 * @accept: true  = search accept_mac (exact) + accept_mac_masked (masked)
 *          false = search deny_mac   (exact) + deny_mac_masked   (masked)
 * @addr: MAC address to search for
 * @vlan_id: Buffer for returning VLAN ID, or %NULL if not needed
 * Returns: 1 if the address is found in either list, 0 if not found.
 *
 */
int hostapd_acl_maclist_found(struct hostapd_bss_config *conf, bool accept,
			      const u8 *addr, struct vlan_description *vlan_id)
{
	struct mac_acl_entry *exact, *masked;
	int num_exact, num_masked;

	if (accept) {
		exact      = conf->accept_mac;
		num_exact  = conf->num_accept_mac;
		masked     = conf->accept_mac_masked;
		num_masked = conf->num_accept_mac_masked;
	} else {
		exact      = conf->deny_mac;
		num_exact  = conf->num_deny_mac;
		masked     = conf->deny_mac_masked;
		num_masked = conf->num_deny_mac_masked;
	}

	if (hostapd_maclist_found(exact, num_exact, addr, vlan_id))
		return 1;

	if (num_masked > 0 &&
	    hostapd_maclist_found_with_mask(masked, num_masked, addr, vlan_id))
		return 1;

	return 0;
}

int hostapd_vlan_valid(struct hostapd_vlan *vlan,
		       struct vlan_description *vlan_desc)
{
	struct hostapd_vlan *v = vlan;
	int i;

	if (!vlan_desc->notempty || vlan_desc->untagged < 0 ||
	    vlan_desc->untagged > MAX_VLAN_ID)
		return 0;
	for (i = 0; i < MAX_NUM_TAGGED_VLAN; i++) {
		if (vlan_desc->tagged[i] < 0 ||
		    vlan_desc->tagged[i] > MAX_VLAN_ID)
			return 0;
	}
	if (!vlan_desc->untagged && !vlan_desc->tagged[0])
		return 0;

	while (v) {
		if (!vlan_compare(&v->vlan_desc, vlan_desc) ||
		    v->vlan_id == VLAN_ID_WILDCARD)
			return 1;
		v = v->next;
	}
	return 0;
}


const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
	struct hostapd_vlan *v = vlan;
	while (v) {
		if (v->vlan_id == vlan_id)
			return v->ifname;
		v = v->next;
	}
	return NULL;
}


const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *p2p_dev_addr,
			   const u8 *prev_psk, int *vlan_id)
{
	struct hostapd_wpa_psk *psk;
	int next_ok = prev_psk == NULL;

	if (vlan_id)
		*vlan_id = 0;

	if (p2p_dev_addr && !is_zero_ether_addr(p2p_dev_addr)) {
		wpa_printf(MSG_DEBUG, "Searching a PSK for " MACSTR
			   " p2p_dev_addr=" MACSTR " prev_psk=%p",
			   MAC2STR(addr), MAC2STR(p2p_dev_addr), prev_psk);
		addr = NULL; /* Use P2P Device Address for matching */
	} else {
		wpa_printf(MSG_DEBUG, "Searching a PSK for " MACSTR
			   " prev_psk=%p",
			   MAC2STR(addr), prev_psk);
	}

	for (psk = conf->ssid.wpa_psk; psk != NULL; psk = psk->next) {
		if (next_ok &&
		    (psk->group ||
		     (addr && ether_addr_equal(psk->addr, addr)) ||
		     (!addr && p2p_dev_addr &&
		      ether_addr_equal(psk->p2p_dev_addr, p2p_dev_addr)))) {
			if (vlan_id)
				*vlan_id = psk->vlan_id;
			return psk->psk;
		}

		if (psk->psk == prev_psk)
			next_ok = 1;
	}

	return NULL;
}


#ifdef CONFIG_SAE_PK
static bool hostapd_sae_pk_password_without_pk(struct hostapd_bss_config *bss)
{
	struct sae_password_entry *pw;
	bool res = false;

	if (bss->ssid.wpa_passphrase &&
#ifdef CONFIG_TESTING_OPTIONS
	    !bss->sae_pk_password_check_skip &&
#endif /* CONFIG_TESTING_OPTIONS */
	    sae_pk_valid_password(bss->ssid.wpa_passphrase))
		res = true;

	for (pw = bss->sae_passwords; pw; pw = pw->next) {
		if (!pw->pk &&
#ifdef CONFIG_TESTING_OPTIONS
		    !bss->sae_pk_password_check_skip &&
#endif /* CONFIG_TESTING_OPTIONS */
		    sae_pk_valid_password(pw->password))
			return true;

		if (bss->ssid.wpa_passphrase && res && pw->pk &&
		    os_strcmp(bss->ssid.wpa_passphrase, pw->password) == 0)
			res = false;
	}

	return res;
}
#endif /* CONFIG_SAE_PK */


bool hostapd_config_check_bss_6g(struct hostapd_bss_config *bss)
{
	if (bss->wpa != WPA_PROTO_RSN) {
		wpa_printf(MSG_ERROR,
			   "Pre-RSNA security methods are not allowed in 6 GHz");
		return false;
	}

	if (bss->ieee80211w != MGMT_FRAME_PROTECTION_REQUIRED) {
		wpa_printf(MSG_ERROR,
			   "Management frame protection is required in 6 GHz");
		return false;
	}

	if (bss->wpa_key_mgmt & (WPA_KEY_MGMT_PSK |
				 WPA_KEY_MGMT_FT_PSK |
				 WPA_KEY_MGMT_PSK_SHA256)) {
		wpa_printf(MSG_ERROR, "Invalid AKM suite for 6 GHz");
		return false;
	}

	if (bss->rsn_pairwise & (WPA_CIPHER_WEP40 |
				 WPA_CIPHER_WEP104 |
				 WPA_CIPHER_TKIP)) {
		wpa_printf(MSG_ERROR,
			   "Invalid pairwise cipher suite for 6 GHz");
		return false;
	}

	if (bss->wpa_group & (WPA_CIPHER_WEP40 |
			      WPA_CIPHER_WEP104 |
			      WPA_CIPHER_TKIP)) {
		wpa_printf(MSG_ERROR, "Invalid group cipher suite for 6 GHz");
		return false;
	}

#ifdef CONFIG_SAE
	if (wpa_key_mgmt_sae(bss->wpa_key_mgmt) &&
	    bss->sae_pwe == SAE_PWE_HUNT_AND_PECK) {
		wpa_printf(MSG_INFO, "SAE: Enabling SAE H2E on 6 GHz");
		bss->sae_pwe = SAE_PWE_BOTH;
	}
#endif /* CONFIG_SAE */

	return true;
}


static int hostapd_config_check_bss(struct hostapd_bss_config *bss,
				    struct hostapd_config *conf,
				    int full_config)
{
	size_t i;

	if (full_config && is_6ghz_op_class(conf->op_class) &&
	    !hostapd_config_check_bss_6g(bss))
		return -1;

	/*
	 * plugin_eap_offload handles EAP authentication outside hostapd, so
	 * a local EAP server or configured RADIUS auth server is not required.
	 */
	if (full_config && bss->ieee802_1x && !bss->eap_server &&
	    !bss->radius->auth_servers && !bss->plugin_eap_offload) {
		wpa_printf(MSG_ERROR, "Invalid IEEE 802.1X configuration (no "
			   "EAP authenticator configured).");
		return -1;
	}

#ifdef CONFIG_WEP
	if (bss->wpa) {
		int wep, i;

		wep = bss->default_wep_key_len > 0 ||
		       bss->individual_wep_key_len > 0;
		for (i = 0; i < NUM_WEP_KEYS; i++) {
			if (bss->ssid.wep.keys_set) {
				wep = 1;
				break;
			}
		}

		if (wep) {
			wpa_printf(MSG_ERROR, "WEP configuration in a WPA network is not supported");
			return -1;
		}
	}
#endif /* CONFIG_WEP */

	if (full_config && bss->wpa &&
	    bss->wpa_psk_radius != PSK_RADIUS_IGNORED &&
	    bss->wpa_psk_radius != PSK_RADIUS_DURING_4WAY_HS &&
	    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
		wpa_printf(MSG_ERROR, "WPA-PSK using RADIUS enabled, but no "
			   "RADIUS checking (macaddr_acl=2) enabled.");
		return -1;
	}

	if (full_config && bss->wpa &&
	    wpa_key_mgmt_wpa_psk_no_sae(bss->wpa_key_mgmt) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL &&
	    bss->wpa_psk_radius != PSK_RADIUS_DURING_4WAY_HS &&
	    (bss->wpa_psk_radius != PSK_RADIUS_REQUIRED ||
	     bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH)) {
		wpa_printf(MSG_ERROR, "WPA-PSK enabled, but PSK or passphrase "
			   "is not configured.");
		return -1;
	}

	if (full_config && !is_zero_ether_addr(bss->bssid)) {
		for (i = 0; i < conf->num_bss; i++) {
			if (conf->bss[i] != bss &&
			    (hostapd_mac_comp(conf->bss[i]->bssid,
					      bss->bssid) == 0)) {
				wpa_printf(MSG_ERROR, "Duplicate BSSID " MACSTR
					   " on interface '%s' and '%s'.",
					   MAC2STR(bss->bssid),
					   conf->bss[i]->iface, bss->iface);
				return -1;
			}
		}
	}

#ifdef CONFIG_IEEE80211R_AP
	if (full_config && wpa_key_mgmt_ft(bss->wpa_key_mgmt) &&
	    (bss->nas_identifier == NULL ||
	     os_strlen(bss->nas_identifier) < 1 ||
	     os_strlen(bss->nas_identifier) > FT_R0KH_ID_MAX_LEN)) {
		wpa_printf(MSG_ERROR, "FT (IEEE 802.11r) requires "
			   "nas_identifier to be configured as a 1..48 octet "
			   "string");
		return -1;
	}
#endif /* CONFIG_IEEE80211R_AP */

	if (full_config && conf->ieee80211n &&
	    conf->hw_mode == HOSTAPD_MODE_IEEE80211B) {
		bss->disable_11n = true;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) in 11b mode is not "
			   "allowed, disabling HT capabilities");
	}

#ifdef CONFIG_WEP
	if (full_config && conf->ieee80211n &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11n = true;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WEP is not "
			   "allowed, disabling HT capabilities");
	}
#endif /* CONFIG_WEP */

	if (full_config && conf->ieee80211n && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				   WPA_CIPHER_CCMP_256 | WPA_CIPHER_GCMP_256)))
	{
		bss->disable_11n = true;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WPA/WPA2 "
			   "requires CCMP/GCMP to be enabled, disabling HT "
			   "capabilities");
	}

	if (full_config && (!conf->ieee80211n || bss->disable_11n) &&
	    bss->bss_require_ht) {
		bss->bss_require_ht = false;
		wpa_printf(MSG_INFO,
			   "Disabling bss_require_ht as IEEE 802.11n is disabled for this BSS");
	}

#ifdef CONFIG_IEEE80211AC
#ifdef CONFIG_WEP
	if (full_config && conf->ieee80211ac &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11ac = true;
		wpa_printf(MSG_ERROR,
			   "VHT (IEEE 802.11ac) with WEP is not allowed, disabling VHT capabilities");
	}
#endif /* CONFIG_WEP */

	if (full_config && conf->ieee80211ac && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				   WPA_CIPHER_CCMP_256 | WPA_CIPHER_GCMP_256)))
	{
		bss->disable_11ac = true;
		wpa_printf(MSG_ERROR,
			   "VHT (IEEE 802.11ac) with WPA/WPA2 requires CCMP/GCMP to be enabled, disabling VHT capabilities");
	}

	if (full_config && (!conf->ieee80211ac || bss->disable_11ac) &&
	    bss->bss_require_vht) {
		bss->bss_require_vht = false;
		wpa_printf(MSG_INFO,
			   "Disabling bss_require_vht as IEEE 802.11ac is disabled for this BSS");
	}

	if (bss->vht_mcs_nss_set) {
		if (!conf->ieee80211ac || bss->disable_11ac) {
			bss->vht_mcs_nss_set = 0;
			wpa_printf(MSG_ERROR,
				   "Selective VHT-MCS rejected: VHT not allowed in current mode");
			return -1;
		}

	}

	if (bss->vht_capab_mask) {
		if (!conf->ieee80211ac || bss->disable_11ac) {
			bss->vht_capab = 0;
			bss->vht_capab_mask = 0;
			wpa_printf(MSG_ERROR,
				   "BSS VHT capabilities rejected: VHT not allowed in current mode");
			return -1;
		}
	}
#endif /* CONFIG_IEEE80211AC */
	if (bss->ht_mcs_nss_set) {
		if (!conf->ieee80211n || bss->disable_11n || !bss->wmm_enabled ||
		    conf->hw_mode == HOSTAPD_MODE_IEEE80211B) {
			bss->ht_mcs_nss_set = 0;
			wpa_printf(MSG_ERROR,
				   "Selective HT-MCS rejected: HT not allowed in current mode");
			return -1;
		}
	}
#ifdef CONFIG_IEEE80211AX
#ifdef CONFIG_WEP
	if (full_config && conf->ieee80211ax &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11ax = true;
		wpa_printf(MSG_ERROR,
			   "HE (IEEE 802.11ax) with WEP is not allowed, disabling HE capabilities");
	}
#endif /* CONFIG_WEP */

	if (full_config && conf->ieee80211ax && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				   WPA_CIPHER_CCMP_256 | WPA_CIPHER_GCMP_256)))
	{
		bss->disable_11ax = true;
		wpa_printf(MSG_ERROR,
			   "HE (IEEE 802.11ax) with WPA/WPA2 requires CCMP/GCMP to be enabled, disabling HE capabilities");
	}

	if (full_config && (!conf->ieee80211ax || bss->disable_11ax) &&
	    bss->bss_require_he) {
		bss->bss_require_he = false;
		wpa_printf(MSG_INFO,
			   "Disabling bss_require_he as IEEE 802.11ax is disabled for this BSS");
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (full_config && !bss->disable_11be && bss->disable_11ax) {
		bss->disable_11be = true;
		bss->mld_ap = 0;
		wpa_printf(MSG_INFO,
			   "Disabling IEEE 802.11be as IEEE 802.11ax is disabled for this BSS");
	}

	if (full_config && (!conf->ieee80211be || bss->disable_11be) &&
	    bss->bss_require_eht) {
		bss->bss_require_eht = false;
		wpa_printf(MSG_INFO,
			   "Disabling bss_require_eht as IEEE 802.11be is disabled for this BSS");
	}

	if ((!conf->ieee80211be || bss->disable_11be) && bss->mld_ap) {
		wpa_printf(MSG_INFO,
			   "Cannot enable mld_ap when IEEE 802.11be is disabled");
		return -1;
	}
#ifdef CONFIG_QCN_EXTN
	/* Validate mld_addr is configured when use_driver_vendor_addr is enabled */
	if (bss->mld_ap && conf->use_driver_vendor_addr &&
			is_zero_ether_addr(bss->mld_addr)) {
		wpa_printf(MSG_ERROR, "MLD: mld_addr must be configured when use_driver_vendor_addr is enabled");
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

#if defined(CONFIG_IEEE80211BE) && defined(CONFIG_QCN_EXTN)
	if (full_config && hostapd_config_check_bss_repurpose_mode_extn(conf, bss)) {
		wpa_printf(MSG_ERROR, "Repurpose mode validation failed");
		return -1;
	}
#endif /* CONFIG_IEEE80211BE && CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211AX
	if (bss->he_phy_capab_mask) {
#ifdef CONFIG_QCN_EXTN
		if (!conf->ieee80211ax || bss->disable_11ax ||
		    hostapd_is_repurpose_disabled_11ax_extn(bss)) {
#else
		if (!conf->ieee80211ax || bss->disable_11ax) {
#endif /* CONFIG_QCN_EXTN */
			u32 mask = bss->he_phy_capab_mask;

			os_memset(&bss->he_phy_capab, 0,
				  sizeof(bss->he_phy_capab));
			bss->he_phy_capab_mask = 0;
			wpa_printf(MSG_ERROR,
				   "BSS HE capability overrides (mask=0x%x) rejected: "
				   "IEEE 802.11ax not allowed",
				   mask);
			return -1;
		}
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_WPS
	if (full_config && bss->wps_state && bss->ignore_broadcast_ssid) {
		wpa_printf(MSG_INFO, "WPS: ignore_broadcast_ssid "
			   "configuration forced WPS to be disabled");
		bss->wps_state = 0;
	}

#ifdef CONFIG_WEP
	if (full_config && bss->wps_state &&
	    bss->ssid.wep.keys_set && bss->wpa == 0) {
		wpa_printf(MSG_INFO, "WPS: WEP configuration forced WPS to be "
			   "disabled");
		bss->wps_state = 0;
	}
#endif /* CONFIG_WEP */

	if (full_config && bss->wps_state && bss->wpa &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				    WPA_CIPHER_CCMP_256 |
				    WPA_CIPHER_GCMP_256)))) {
		wpa_printf(MSG_INFO, "WPS: WPA/TKIP configuration without "
			   "WPA2/CCMP/GCMP forced WPS to be disabled");
		bss->wps_state = 0;
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
	if (full_config && bss->hs20 &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				    WPA_CIPHER_CCMP_256 |
				    WPA_CIPHER_GCMP_256)))) {
		wpa_printf(MSG_ERROR, "HS 2.0: WPA2-Enterprise/CCMP "
			   "configuration is required for Hotspot 2.0 "
			   "functionality");
		return -1;
	}
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	if (full_config && bss->mbo_enabled && (bss->wpa & 2) &&
	    bss->ieee80211w == NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_ERROR,
			   "MBO: PMF needs to be enabled whenever using WPA2 with MBO");
		return -1;
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_OCV
	if (full_config && bss->ieee80211w == NO_MGMT_FRAME_PROTECTION &&
	    bss->ocv) {
		wpa_printf(MSG_ERROR,
			   "OCV: PMF needs to be enabled whenever using OCV");
		return -1;
	}
#endif /* CONFIG_OCV */

#ifdef CONFIG_SAE_PK
	if (full_config && hostapd_sae_pk_in_use(bss) &&
	    hostapd_sae_pk_password_without_pk(bss)) {
		wpa_printf(MSG_ERROR,
			   "SAE-PK: SAE password uses SAE-PK style, but does not have PK configured");
		return -1;
	}
#endif /* CONFIG_SAE_PK */

#ifdef CONFIG_FILS
#ifdef CONFIG_QCN_EXTN
	if (full_config && bss->fils_discovery_max_int &&
	    (!conf->ieee80211ax || bss->disable_11ax ||
	     hostapd_is_repurpose_disabled_11ax_extn(bss))) {
#else
	if (full_config && bss->fils_discovery_max_int &&
	    (!conf->ieee80211ax || bss->disable_11ax)) {
#endif /* CONFIG_QCN_EXTN */
		wpa_printf(MSG_ERROR,
			   "Currently IEEE 802.11ax support is mandatory to enable FILS discovery transmission.");
		return -1;
	}

	if (full_config && bss->fils_discovery_max_int &&
	    bss->unsol_bcast_probe_resp_interval) {
		wpa_printf(MSG_ERROR,
			   "Cannot enable both FILS discovery and unsolicited broadcast Probe Response at the same time");
		return -1;
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (full_config && conf->ieee80211be && !bss->disable_11be &&
	    !hostapd_is_repurpose_disabled_11be_extn(bss) &&
	    !bss->beacon_prot && ap_pmf_enabled(bss)) {
#else
	if (full_config && conf->ieee80211be && !bss->disable_11be &&
	    !bss->beacon_prot && ap_pmf_enabled(bss)) {
#endif /* CONFIG_QCN_EXTN */
		bss->beacon_prot = 1;
		wpa_printf(MSG_INFO,
			   "Enabling beacon protection as IEEE 802.11be is enabled for this BSS");
	}

	/* Avoid duplicate radio link in same MLD by comparing the interface name
	 * of all the other 11BE capable bss configured and ensure that it does
	 * not have same interface name.
	 */
	for (i = 0; i < conf->num_bss; i++) {
		if (conf->bss[i] != bss && (conf->ieee80211be && !bss->disable_11be)) {
			if (os_strncmp(conf->bss[i]->iface, bss->iface,
				       IFNAMSIZ) == 0) {
				wpa_printf(MSG_ERROR, "Duplicate MLD %s on link"
					   " BSSID " MACSTR,
					   bss->iface, MAC2STR(bss->bssid));
				return -1;
			}
		}
	}


#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(bss)) {
#endif /* CONFIG_QCN_EXTN */
	if (bss->mld_ap) {
		/* set ML Max rec links to default, if it is not configured */
		if (bss->enable_aal &&
		    (bss->ml_max_rec_links == ML_IE_MAX_REC_LINKS_INVAL))
			bss->ml_max_rec_links = ML_IE_DEF_MAX_REC_LINKS;
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (full_config && conf->ieee80211bn && !bss->disable_11bn) {
		if (!conf->ieee80211be) {
			wpa_printf(MSG_ERROR,
				   "Cannot set ieee80211bn without ieee80211be");
			return -1;
		}

		if (!bss->mld_ap) {
			wpa_printf(MSG_ERROR,
				   "Cannot set ieee80211bn without mld_ap");
			return -1;
		}
	}
#endif

	/* Do not advertise SPP A-MSDU support if not using CCMP/GCMP */
	if (full_config && bss->spp_amsdu &&
	    !(bss->wpa &&
	      bss->rsn_pairwise & (WPA_CIPHER_CCMP_256 | WPA_CIPHER_CCMP |
				   WPA_CIPHER_GCMP_256 | WPA_CIPHER_GCMP)))
		bss->spp_amsdu = false;

	if (!hostapd_is_beacon_tx_rate_preamble_valid(conf, bss))
		return -1;

#ifdef CONFIG_IEEE80211BE

	if (bss->eht_phy_capab_mask) {
#ifdef CONFIG_QCN_EXTN
		if (!conf->ieee80211be || bss->disable_11be ||
		    hostapd_is_repurpose_disabled_11be_extn(bss)) {
#else
		if (!conf->ieee80211be || bss->disable_11be) {
#endif /* CONFIG_QCN_EXTN */
			u32 mask = bss->eht_phy_capab_mask;

			os_memset(&bss->eht_phy_capab, 0,
				  sizeof(bss->eht_phy_capab));
			bss->eht_phy_capab_mask = 0;
			wpa_printf(MSG_ERROR,
				   "BSS EHT capability overrides (mask=0x%x) rejected: "
				   "IEEE 802.11be not allowed",
				   mask);
			return -1;
		}
	}

	if (bss->eht_phy_capab.eht_bfme_ss_80 > 7) {
		wpa_printf(MSG_ERROR,
			   "Invalid bss_eht_bfme_ss_80=%u (valid range 0..7)",
			   bss->eht_phy_capab.eht_bfme_ss_80);
		return -1;
	}

	if (bss->eht_phy_capab.eht_bfme_ss_160 > 7) {
		wpa_printf(MSG_ERROR,
			   "Invalid bss_eht_bfme_ss_160=%u (valid range 0..7)",
			   bss->eht_phy_capab.eht_bfme_ss_160);
		return -1;
	}

	if (bss->eht_phy_capab.eht_bfme_ss_320 > 7) {
		wpa_printf(MSG_ERROR,
			   "Invalid bss_eht_bfme_ss_320=%u (valid range 0..7)",
			   bss->eht_phy_capab.eht_bfme_ss_320);
		return -1;
	}

	if (!bss->eht_phy_capab.mu_beamformer && bss->eht_phy_capab.eht_mu_bfmr_mask) {
		wpa_printf(MSG_ERROR,
			   "bss_eht_mu_bfmr set but MU beamformer capability is not enabled");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (bss->eht_ltf &&
	    !(conf->ieee80211be && !bss->disable_11be &&
	      !hostapd_is_repurpose_disabled_11be_extn(bss))) {
#else
	if (bss->eht_ltf &&
	    !(conf->ieee80211be && !bss->disable_11be)) {
#endif /* CONFIG_QCN_EXTN */
		wpa_printf(MSG_ERROR,
			   "Selective EHT LTF rejected: EHT not allowed in current mode");
		bss->eht_ltf = 0;
		return -1;
	}
#endif /* CONFIG_IEEE80211BE */

	return 0;
}

#ifdef CONFIG_IEEE80211BN
int hostapd_config_check_npca_config(struct hostapd_config *conf)
{
	enum oper_chan_width chwidth;
	int seg0, npca_chan;
	bool in_secondary;
	int bw_mhz;

	if (!conf->npca_enable)
		return 0;

        if (!conf->npca_primary_channel)
		return 0;

	/* For 6 GHz op_class the bandwidth is defined by op_class, not by the
	 * explicit eht_oper_chwidth config knob, so derive it the same way the
	 * rest of the stack does (ieee802_11_eht.c, repurpose.c). */
	if (is_6ghz_op_class(conf->op_class))
		chwidth = op_class_to_ch_width(conf->op_class);
	else
		chwidth = hostapd_get_oper_chwidth(conf);
	if (chwidth != CONF_OPER_CHWIDTH_80MHZ &&
	    chwidth != CONF_OPER_CHWIDTH_160MHZ &&
	    chwidth != CONF_OPER_CHWIDTH_320MHZ) {
		wpa_printf(MSG_ERROR,
			   "NPCA is only supported for 80/160/320 MHz bandwidths");
		return -1;
	}

	seg0 = (int)hostapd_get_oper_centr_freq_seg0_idx(conf);
	npca_chan = (int)conf->npca_primary_channel;
	in_secondary = false;

	if (chwidth == CONF_OPER_CHWIDTH_80MHZ) {
		/*
		 * 80 MHz: seg0 is the center; subchannels are at seg0±2 and
		 * seg0±6. Secondary 40 MHz half = the pair of subchannels on
		 * the opposite side of seg0 from the primary channel.
		 */
		bw_mhz = 80;
		if (conf->channel < seg0)
			in_secondary = (npca_chan >= seg0 + 2 &&
					npca_chan <= seg0 + 6);
		else
			in_secondary = (npca_chan >= seg0 - 6 &&
					npca_chan <= seg0 - 2);
	} else if (chwidth == CONF_OPER_CHWIDTH_160MHZ) {
		/*
		 * 160 MHz: seg0 is the center of the full 160 MHz band.
		 * The primary 80 MHz half contains conf->channel.
		 * Secondary half = the 4 subchannels on the side of seg0
		 * that does not contain the primary channel.
		 */
		bw_mhz = 160;
		if (conf->channel < seg0)
			in_secondary = (npca_chan >= seg0 + 2 &&
					npca_chan <= seg0 + 14);
		else
			in_secondary = (npca_chan >= seg0 - 14 &&
					npca_chan <= seg0 - 2);
	} else {
		/*
		 * 320 MHz: seg0 is the center of the full 320 MHz band.
		 * Secondary half = the 8 subchannels on the side of seg0
		 * that does not contain the primary channel.
		 */
		bw_mhz = 320;
		if (conf->channel < seg0)
			in_secondary = (npca_chan >= seg0 + 2 &&
					npca_chan <= seg0 + 30);
		else
			in_secondary = (npca_chan >= seg0 - 30 &&
					npca_chan <= seg0 - 2);
	}

	if (!in_secondary) {
		wpa_printf(MSG_ERROR,
			   "NPCA primary channel %d not in secondary segment "
			   "(seg0=%d, bw=%d MHz)",
			   npca_chan, seg0, bw_mhz);
		return -1;
	}

	return 0;
}
#endif /* CONFIG_IEEE80211BN */


static int hostapd_config_check_cw(struct hostapd_config *conf, int queue)
{
	int tx_cwmin = conf->tx_queue[queue].cwmin;
	int tx_cwmax = conf->tx_queue[queue].cwmax;
	int ac_cwmin = conf->wmm_ac_params[queue].cwmin;
	int ac_cwmax = conf->wmm_ac_params[queue].cwmax;

	if (tx_cwmin > tx_cwmax) {
		wpa_printf(MSG_ERROR,
			   "Invalid TX queue cwMin/cwMax values. cwMin(%d) greater than cwMax(%d)",
			   tx_cwmin, tx_cwmax);
		return -1;
	}
	if (ac_cwmin > ac_cwmax) {
		wpa_printf(MSG_ERROR,
			   "Invalid WMM AC cwMin/cwMax values. cwMin(%d) greater than cwMax(%d)",
			   ac_cwmin, ac_cwmax);
		return -1;
	}
	return 0;
}


int hostapd_config_check(struct hostapd_config *conf, int full_config)
{
	size_t i;

	if (full_config && is_6ghz_op_class(conf->op_class) &&
	    !conf->hw_mode_set) {
		/* Use the appropriate hw_mode value automatically when the
		 * op_class parameter has been set, but hw_mode was not. */
		conf->hw_mode = HOSTAPD_MODE_IEEE80211A;
	}

	if (full_config && conf->ieee80211d &&
	    (!conf->country[0] || !conf->country[1])) {
		wpa_printf(MSG_ERROR, "Cannot enable IEEE 802.11d without "
			   "setting the country_code");
		return -1;
	}

	if (full_config && conf->ieee80211h && !conf->ieee80211d) {
		wpa_printf(MSG_ERROR, "Cannot enable IEEE 802.11h without "
			   "IEEE 802.11d enabled");
		return -1;
	}

	if (full_config && conf->local_pwr_constraint != -1 &&
	    !conf->ieee80211d) {
		wpa_printf(MSG_ERROR, "Cannot add Power Constraint element without Country element");
		return -1;
	}

	if (full_config && conf->spectrum_mgmt_required &&
	    conf->local_pwr_constraint == -1) {
		wpa_printf(MSG_ERROR, "Cannot set Spectrum Management bit without Country and Power Constraint elements");
		return -1;
	}

#ifdef CONFIG_AIRTIME_POLICY
	if (full_config && conf->airtime_mode > AIRTIME_MODE_STATIC &&
	    !conf->airtime_update_interval) {
		wpa_printf(MSG_ERROR, "Airtime update interval cannot be zero");
		return -1;
	}
#endif /* CONFIG_AIRTIME_POLICY */
	for (i = 0; i < NUM_TX_QUEUES; i++) {
		if (hostapd_config_check_cw(conf, i))
			return -1;
	}

#ifdef CONFIG_IEEE80211BE
	if (full_config && conf->ieee80211be && !conf->ieee80211ax) {
		wpa_printf(MSG_ERROR,
			   "Cannot set ieee80211be without ieee80211ax");
		return -1;
	}

	if (full_config)
		hostapd_set_and_check_bw320_offset(conf,
						   conf->eht_bw320_offset);
#endif /* CONFIG_IEEE80211BE */

	if (full_config && conf->mbssid && !conf->ieee80211ax) {
		wpa_printf(MSG_ERROR,
			   "Cannot enable multiple BSSID support without ieee80211ax");
		return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (full_config && !hostapd_config_check_repurpose_width_extn(conf)) {
		wpa_printf(MSG_ERROR, "Wrong repurpose width configurations");
		return -1;
	}
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	if (full_config && hostapd_config_check_npca_config(conf))
		return -1;
#endif /* CONFIG_IEEE80211BN */

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_config_check_bss(conf->bss[i], conf, full_config))
			return -1;
	}

#ifdef CONFIG_QCN_EXTN
	if (full_config && conf->use_driver_vendor_addr &&
	    conf->mbssid != MBSSID_DISABLED &&
	    !is_6ghz_op_class(conf->op_class)) {
		wpa_printf(MSG_ERROR,
			   "Driver vendor address allocation is supported for MBSSID mode only in 6 GHz");
		return -1;
	}
#endif

	return 0;
}


void hostapd_set_security_params(struct hostapd_bss_config *bss,
				 int full_config)
{
#ifdef CONFIG_WEP
	if (bss->individual_wep_key_len == 0) {
		/* individual keys are not use; can use key idx0 for
		 * broadcast keys */
		bss->broadcast_key_idx_min = 0;
	}
#endif /* CONFIG_WEP */

	if ((bss->wpa & 2) && bss->rsn_pairwise == 0)
		bss->rsn_pairwise = bss->wpa_pairwise;
	if (bss->group_cipher)
		bss->wpa_group = bss->group_cipher;
	else
		bss->wpa_group = wpa_select_ap_group_cipher(bss->wpa,
							    bss->wpa_pairwise,
							    bss->rsn_pairwise);
	if (!bss->wpa_group_rekey_set)
		bss->wpa_group_rekey = bss->wpa_group == WPA_CIPHER_TKIP ?
			600 : 86400;

	if (full_config) {
		bss->radius->auth_server = bss->radius->auth_servers;
		bss->radius->acct_server = bss->radius->acct_servers;
	}

	if (bss->wpa && bss->ieee802_1x) {
		bss->ssid.security_policy = SECURITY_WPA;
	} else if (bss->wpa) {
		bss->ssid.security_policy = SECURITY_WPA_PSK;
	} else if (bss->ieee802_1x) {
		int cipher = WPA_CIPHER_NONE;
		bss->ssid.security_policy = SECURITY_IEEE_802_1X;
#ifdef CONFIG_WEP
		bss->ssid.wep.default_len = bss->default_wep_key_len;
		if (full_config && bss->default_wep_key_len) {
			cipher = bss->default_wep_key_len >= 13 ?
				WPA_CIPHER_WEP104 : WPA_CIPHER_WEP40;
		} else if (full_config && bss->ssid.wep.keys_set) {
			if (bss->ssid.wep.len[0] >= 13)
				cipher = WPA_CIPHER_WEP104;
			else
				cipher = WPA_CIPHER_WEP40;
		}
#endif /* CONFIG_WEP */
		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
		if (full_config)
			bss->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_NO_WPA;
#ifdef CONFIG_WEP
	} else if (bss->ssid.wep.keys_set) {
		int cipher = WPA_CIPHER_WEP40;
		if (bss->ssid.wep.len[0] >= 13)
			cipher = WPA_CIPHER_WEP104;
		bss->ssid.security_policy = SECURITY_STATIC_WEP;
		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
		if (full_config)
			bss->wpa_key_mgmt = WPA_KEY_MGMT_NONE;
#endif /* CONFIG_WEP */
	} else {
		bss->ssid.security_policy = SECURITY_PLAINTEXT;
		if (full_config) {
			bss->wpa_group = WPA_CIPHER_NONE;
			bss->wpa_pairwise = WPA_CIPHER_NONE;
			bss->rsn_pairwise = WPA_CIPHER_NONE;
			bss->wpa_key_mgmt = WPA_KEY_MGMT_NONE;
		}
	}
}


int hostapd_sae_pw_id_in_use(struct hostapd_bss_config *conf)
{
	int with_id = 0, without_id = 0;
	struct sae_password_entry *pw;

	if (conf->ssid.wpa_passphrase)
		without_id = 1;

	for (pw = conf->sae_passwords; pw; pw = pw->next) {
		if (pw->identifier)
			with_id = 1;
		else
			without_id = 1;
		if (with_id && without_id)
			break;
	}

	if (with_id && !without_id)
		return 2;
	return with_id;
}


bool hostapd_sae_pk_in_use(struct hostapd_bss_config *conf)
{
#ifdef CONFIG_SAE_PK
	struct sae_password_entry *pw;

	for (pw = conf->sae_passwords; pw; pw = pw->next) {
		if (pw->pk)
			return true;
	}
#endif /* CONFIG_SAE_PK */

	return false;
}


#ifdef CONFIG_SAE_PK
bool hostapd_sae_pk_exclusively(struct hostapd_bss_config *conf)
{
	bool with_pk = false;
	struct sae_password_entry *pw;

	if (conf->ssid.wpa_passphrase)
		return false;

	for (pw = conf->sae_passwords; pw; pw = pw->next) {
		if (!pw->pk)
			return false;
		with_pk = true;
	}

	return with_pk;
}
#endif /* CONFIG_SAE_PK */


int hostapd_acl_comp(const void *a, const void *b)
{
	const struct mac_acl_entry *aa = a;
	const struct mac_acl_entry *bb = b;
	return os_memcmp(aa->addr, bb->addr, sizeof(macaddr));
}

int hostapd_add_acl_maclist(struct mac_acl_entry **acl, int *num,
			    int vlan_id, const u8 *addr)
{
	struct mac_acl_entry *newacl;

	newacl = os_realloc_array(*acl, *num + 1, sizeof(**acl));
	if (!newacl) {
		wpa_printf(MSG_ERROR, "MAC list reallocation failed");
		return -1;
	}

	*acl = newacl;
	os_memcpy((*acl)[*num].addr, addr, ETH_ALEN);
	/* Initialize mask to all-ones (exact match by default) */
	os_memset((*acl)[*num].mask, 0xff, ETH_ALEN);
	os_memset(&(*acl)[*num].vlan_id, 0, sizeof((*acl)[*num].vlan_id));
	(*acl)[*num].vlan_id.untagged = vlan_id;
	(*acl)[*num].vlan_id.notempty = !!vlan_id;
	(*num)++;

	return 0;
}

void hostapd_remove_acl_mac(struct mac_acl_entry **acl, int *num,
			    const u8 *addr)
{
	int i = 0;

	while (i < *num) {
		if (ether_addr_equal((*acl)[i].addr, addr)) {
			os_remove_in_array(*acl, *num, sizeof(**acl), i);
			(*num)--;
		} else {
			i++;
		}
	}
}

int hostapd_add_acl_maclist_masked(struct mac_acl_entry **acl, int *num,
				   int vlan_id, const u8 *addr, const u8 *mask)
{
	struct mac_acl_entry *newacl;

	newacl = os_realloc_array(*acl, *num + 1, sizeof(**acl));
	if (!newacl) {
		wpa_printf(MSG_ERROR, "MAC masked list reallocation failed");
		return -1;
	}

	*acl = newacl;
	os_memcpy((*acl)[*num].addr, addr, ETH_ALEN);
	os_memcpy((*acl)[*num].mask, mask, ETH_ALEN);
	os_memset(&(*acl)[*num].vlan_id, 0, sizeof((*acl)[*num].vlan_id));
	(*acl)[*num].vlan_id.untagged = vlan_id;
	(*acl)[*num].vlan_id.notempty = !!vlan_id;
	(*num)++;

	return 0;
}

/**
 * hostapd_remove_all_masked_entries_for_addr - Remove all masked ACL entries
 *     matching an address
 * @acl: pointer to the masked MAC list
 * @num: pointer to the entry count
 * @addr: MAC address to match
 *
 * Removes every masked entry whose address field equals @addr, regardless of
 * the mask value.  Used when no specific mask is given (i.e. "remove all rules
 * for this address").
 */
void hostapd_remove_all_masked_entries_for_addr(struct mac_acl_entry **acl,
						int *num, const u8 *addr)
{
	int i = 0;

	while (i < *num) {
		if (ether_addr_equal((*acl)[i].addr, addr)) {
			os_remove_in_array(*acl, *num, sizeof(**acl), i);
			(*num)--;
		} else {
			i++;
		}
	}
}

/**
 * hostapd_remove_acl_mac_masked_pair - Remove a specific masked ACL entry
 * @acl: pointer to the masked MAC list
 * @num: pointer to the entry count
 * @addr: MAC address to match
 * @mask: mask to match (both addr AND mask must match)
 *
 * Removes the masked entry whose addr+mask pair exactly matches the given
 * values.  Since the add path prevents duplicate addr+mask pairs, at most one
 * entry will be removed.
 */
void hostapd_remove_acl_mac_masked_pair(struct mac_acl_entry **acl, int *num,
					const u8 *addr, const u8 *mask)
{
	int i = 0;

	while (i < *num) {
		if (ether_addr_equal((*acl)[i].addr, addr) &&
		    os_memcmp((*acl)[i].mask, mask, ETH_ALEN) == 0) {
			os_remove_in_array(*acl, *num, sizeof(**acl), i);
			(*num)--;
			break; /* addr+mask pairs are unique; stop after first match */
		} else {
			i++;
		}
	}
}

int hostapd_acl_add_entry(struct mac_acl_entry **exact_acl, int *num_exact,
			  struct mac_acl_entry **masked_acl, int *num_masked,
			  int vlan_id, const u8 *addr, const u8 *mask)
{
	static const u8 all_ones[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (os_memcmp(mask, all_ones, ETH_ALEN) == 0) {
		/* Exact-match entry: add to sorted list */
		if (hostapd_add_acl_maclist(exact_acl, num_exact,
					    vlan_id, addr) < 0)
			return -1;
		/* Re-sort to maintain binary-search invariant */
		qsort(*exact_acl, *num_exact, sizeof(**exact_acl),
		      hostapd_acl_comp);
	} else {
		/* Masked entry: append to masked list */
		if (hostapd_add_acl_maclist_masked(masked_acl, num_masked,
						   vlan_id, addr, mask) < 0)
			return -1;
	}

	return 0;
}

void hostapd_acl_del_entry(struct mac_acl_entry **exact_acl, int *num_exact,
			   struct mac_acl_entry **masked_acl, int *num_masked,
			   const u8 *addr)
{
	hostapd_remove_acl_mac(exact_acl, num_exact, addr);
	hostapd_remove_all_masked_entries_for_addr(masked_acl, num_masked, addr);
}

void hostapd_acl_clear(struct mac_acl_entry **exact_acl, int *num_exact,
		       struct mac_acl_entry **masked_acl, int *num_masked)
{
	os_free(*exact_acl);
	*exact_acl = NULL;
	*num_exact = 0;

	os_free(*masked_acl);
	*masked_acl = NULL;
	*num_masked = 0;
}

void hostapd_config_free_acl_timed_list(struct hostapd_bss_config *conf)
{
	struct acl_timed_deny_entry *entry, *tmp;

	if (!conf)
		return;

	entry = conf->acl_timed_deny_list;
	while (entry) {
		tmp = entry->next;
		os_free(entry);
		entry = tmp;
	}
	conf->acl_timed_deny_list = NULL;
}

bool hostapd_is_beacon_tx_rate_preamble_valid(const struct hostapd_config *iconf,
		const struct hostapd_bss_config *bss)
{
	if (bss->rate_type == BEACON_RATE_HT &&
	    !(iconf->ieee80211n && !bss->disable_11n)) {
		wpa_printf(MSG_ERROR,
			   "HT rate is configured for beacon_rate, but 11n is disabled");
		return false;
	} else if (bss->rate_type == BEACON_RATE_VHT &&
		   !(iconf->ieee80211ac && !bss->disable_11ac)) {
			wpa_printf(MSG_ERROR,
				   "VHT rate is configured for beacon_rate, but 11ac is disabled");
			return false;
	} else if (bss->rate_type == BEACON_RATE_HE) {
#ifdef CONFIG_QCN_EXTN
		if (!(iconf->ieee80211ax && !bss->disable_11ax &&
		      !hostapd_is_repurpose_disabled_11ax_extn(bss))) {
#else
		if (!(iconf->ieee80211ax && !bss->disable_11ax)) {
#endif /* CONFIG_QCN_EXTN */
			wpa_printf(MSG_ERROR,
				   "HE rate is configured for beacon_rate, but 11ax is disabled");
			return false;
		}
	} else if (bss->rate_type == BEACON_RATE_EHT) {
		/* EHT preamible requires 11be enabled */
#ifdef CONFIG_QCN_EXTN
		if (!(iconf->ieee80211be && !bss->disable_11be &&
		      !hostapd_is_repurpose_disabled_11be_extn(bss))) {
#else
		if (!(iconf->ieee80211be && !bss->disable_11be)) {
#endif /* CONFIG_QCN_EXTN */
			wpa_printf(MSG_ERROR,
				   "EHT rate is configured for beacon_rate, but 11be is disabled");
			return false;
		}
		/* Disallow EHT MCS 15 when enable_mcs15 is disabled */
		if (!iconf->enable_mcs15 && bss->beacon_rate == 15) {
			wpa_printf(MSG_ERROR,
				   "EHT MCS 15 is configured for beacon_rate, but enable_mcs15 is disabled");
			return false;
		}
	}

	return true;
}
