/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef CTRL_IFACE_AP_H
#define CTRL_IFACE_AP_H

enum maxnss_htmode_t {
	MAXNSS_HTMODE_UNSET = 0x00,
	MAXNSS_HTMODE_HT_N = 0x01,
	MAXNSS_HTMODE_VHT_AC = 0x02,
	MAXNSS_HTMODE_HE_AX = 0x04,
	MAXNSS_HTMODE_EHT_BE = 0x08,
	MAXNSS_HTMODE_MAX = 0x0F
};

#define HOSTAPD_PHY_RATE_TSYM_HE_EHT_TENTHS_US 136
#define HOSTAPD_PHY_RATE_TSYM_HT_VHT_TENTHS_US 36

#define HOSTAPD_PHY_RATE_NSD_20MHZ_HE_EHT 234
#define HOSTAPD_PHY_RATE_NSD_40MHZ_HE_EHT 468
#define HOSTAPD_PHY_RATE_NSD_80MHZ_HE_EHT 980
#define HOSTAPD_PHY_RATE_NSD_160MHZ_HE_EHT 1960
#define HOSTAPD_PHY_RATE_NSD_320MHZ_EHT 3920

#define HOSTAPD_PHY_RATE_NSD_20MHZ_HT_VHT 52
#define HOSTAPD_PHY_RATE_NSD_40MHZ_HT_VHT 108
#define HOSTAPD_PHY_RATE_NSD_80MHZ_VHT 234
#define HOSTAPD_PHY_RATE_NSD_160MHZ_VHT 468

#define HOSTAPD_PHY_RATE_NBPSC_BPSK 1
#define HOSTAPD_PHY_RATE_NBPSC_QPSK 2
#define HOSTAPD_PHY_RATE_NBPSC_16QAM 4
#define HOSTAPD_PHY_RATE_NBPSC_64QAM 6
#define HOSTAPD_PHY_RATE_NBPSC_256QAM 8
#define HOSTAPD_PHY_RATE_NBPSC_1024QAM 10
#define HOSTAPD_PHY_RATE_NBPSC_4096QAM 12

#define HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2 2
#define HOSTAPD_PHY_RATE_CODE_DENOMINATOR_3 3
#define HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4 4
#define HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6 6

#define HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR 1
#define HOSTAPD_PHY_RATE_CODE_2_3_NUMERATOR 2
#define HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR 3
#define HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR 5

#define MCS_MAP_BITS_PER_NSS 2
#define MCS_MAP_NSS_MASK 0x3

#define VHT_MCS_MAP_0_7_MAX_MCS   7
#define VHT_MCS_MAP_0_8_MAX_MCS   8
#define VHT_MCS_MAP_0_9_MAX_MCS   9
#define MCS_MAP_NOT_SUPP      0

#define HE_MCS_MAP_0_7_MAX_MCS     7
#define HE_MCS_MAP_0_9_MAX_MCS     9
#define HE_MCS_MAP_0_11_MAX_MCS    11

#define HOSTAPD_EHT_MAX_MCS_9               9
#define HOSTAPD_EHT_MAX_MCS_11              11
#define HOSTAPD_EHT_MAX_MCS_13              13

#define HOSTAPD_EHT_MCS_NSS_SETS_20MHZ_PLUS     1
#define HOSTAPD_EHT_MCS_NSS_SETS_160_OR_80P80   2
#define HOSTAPD_EHT_MCS_NSS_SETS_320MHZ         3

#define HOSTAPD_HE_MCS_MAP_COUNT_20_40_80        1
#define HOSTAPD_HE_MCS_MAP_COUNT_160             2
#define HOSTAPD_HE_MCS_MAP_COUNT_80P80           3

#define IEEE80211_RATE_UNIT_KBPS        500U
#define IEEE80211_RATE_VAL_MASK         0x7f
#define HOSTAPD_MODE_RATE_UNIT_KBPS     100U
#define HOSTAPD_PHY_RATE_TENTHS_US_TO_KBPS_SCALE  10000ULL

int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
				 char *buf, size_t buflen);
int hostapd_ctrl_iface_sta(struct hostapd_data *hapd, const char *txtaddr,
			   char *buf, size_t buflen);
int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd, const char *txtaddr,
				char *buf, size_t buflen);
int hostapd_ctrl_iface_deauthenticate(struct hostapd_data *hapd,
				      const char *txtaddr);
int hostapd_ctrl_iface_disassociate(struct hostapd_data *hapd,
				    const char *txtaddr);
int hostapd_ctrl_iface_signature(struct hostapd_data *hapd,
				 const char *txtaddr,
				 char *buf, size_t buflen);
int hostapd_ctrl_iface_dump_tk(struct hostapd_data *hapd, const char *cmd,
			       char *buf, size_t buflen);
int hostapd_ctrl_iface_poll_sta(struct hostapd_data *hapd,
				const char *txtaddr);
int hostapd_ctrl_iface_status(struct hostapd_data *hapd, char *buf,
			      size_t buflen);
int hostapd_parse_freq_params(const char *pos,
			      struct hostapd_freq_params *params,
			      unsigned int freq);
int hostapd_parse_csa_settings(struct hostapd_iface *iface,
			       const char *pos,
			       struct csa_settings *settings);
int hostapd_parse_channel_usage_settings(const char *pos,
					 struct channel_usage_config *cfg);
int hostapd_ctrl_iface_stop_ap(struct hostapd_data *hapd);
int hostapd_ctrl_iface_pmksa_list(struct hostapd_data *hapd, char *buf,
				  size_t len);
void hostapd_ctrl_iface_pmksa_flush(struct hostapd_data *hapd);
int hostapd_ctrl_iface_pmksa_add(struct hostapd_data *hapd, char *cmd);
int hostapd_ctrl_iface_pmksa_list_mesh(struct hostapd_data *hapd,
				       const u8 *addr, char *buf, size_t len);
void * hostapd_ctrl_iface_pmksa_create_entry(const u8 *aa, char *cmd);

int hostapd_ctrl_iface_disassoc_imminent(struct hostapd_data *hapd,
					 const char *cmd);
int hostapd_ctrl_iface_ess_disassoc(struct hostapd_data *hapd,
				    const char *cmd);
int hostapd_ctrl_iface_bss_tm_req(struct hostapd_data *hapd,
				  const char *cmd);
int hostapd_ctrl_iface_acl_add_mac(struct hostapd_bss_config *conf,
				   bool accept, const char *cmd);
int hostapd_ctrl_iface_acl_del_mac(struct hostapd_bss_config *conf,
				   bool accept, const char *txtaddr);
void hostapd_ctrl_iface_acl_clear_list(struct hostapd_bss_config *conf,
					bool accept);
int hostapd_ctrl_iface_acl_show_mac(struct hostapd_bss_config *conf,
				    bool accept,
				    char *buf, size_t buflen);
int hostapd_ctrl_iface_set_mbssid_tx(struct hostapd_data *hapd, const char *cmd);
int hostapd_disassoc_accept_mac(struct hostapd_data *hapd);
int hostapd_disassoc_deny_mac(struct hostapd_data *hapd);
u8 hostapd_maxnss(struct hostapd_data *hapd, struct sta_info *sta);
#endif /* CTRL_IFACE_AP_H */
