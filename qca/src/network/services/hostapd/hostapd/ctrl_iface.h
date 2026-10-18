/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef CTRL_IFACE_H
#define CTRL_IFACE_H

#ifndef CONFIG_NO_CTRL_IFACE
int hostapd_ctrl_iface_init(struct hostapd_data *hapd);
void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd);
int hostapd_global_ctrl_iface_init(struct hapd_interfaces *interface);
void hostapd_global_ctrl_iface_deinit(struct hapd_interfaces *interface);
int hostapd_mld_ctrl_iface_init(struct hostapd_mld *mld);
void hostapd_mld_ctrl_iface_deinit(struct hostapd_mld *mld);
#else /* CONFIG_NO_CTRL_IFACE */
static inline int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
	return 0;
}

static inline void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
}

static inline int
hostapd_global_ctrl_iface_init(struct hapd_interfaces *interface)
{
	return 0;
}

static inline void
hostapd_global_ctrl_iface_deinit(struct hapd_interfaces *interface)
{
}
#endif /* CONFIG_NO_CTRL_IFACE */
#ifdef CONFIG_QCN_EXTN
extern int hostapd_drv_set_muedca_mode(struct hostapd_data *hapd, int mode, int radio_idx);
extern int hostapd_config_he_mu_edca(struct ieee80211_he_mu_edca_parameter_set *params,
                                     const char *name, const char *val);

#endif /* CONFIG_QCN_EXTN */

enum bss_mbssid_cmn_param {
	CMD_INVALID = 0,
	CMD_BEACON_INT,
	CMD_VHT_MU_BFMER,
	CMD_VHT_SU_BFMER,
	CMD_VHT_SU_BFMEE,
	CMD_VHT_SOUNDING_DIM,
	CMD_VHT_BFMEE_STS,
	CMD_VHT_MCS_NSS_SET,
	CMD_HE_SU_BFMER,
	CMD_HE_SU_BFMEE,
	CMD_HE_MU_BEAMFORMER,
	CMD_HE_UL_MUMIMO,
	CMD_HE_BASIC_MCS_NSS_SET,
	CMD_HE_RTS_THRESHOLD,
	CMD_SPP_AMSDU,
	CMD_HE_TWT_RESPONDER,
	CMD_HE_6GHZ_MAX_AMPDU_LEN_EXP,
	CMD_HE_ER_SU_DISABLE,
	CMD_HE_BFEE_STS,
	CMD_HE_MULTI_TID_AGGR,
	CMD_HE_MULTI_TID_AGGR_RX,
	CMD_HE_MULTI_TID_AGGR_TX,
	CMD_HE_MAX_AMPDU_LEN_EXP,
	CMD_HE_SU_PPDU_1X_LTF_800NS_GI,
	CMD_HE_SU_MU_PPDU_4X_LTF_800NS_GI,
	CMD_HE_MAX_FRAG_MSDU,
	CMD_HE_MIN_FRAG_SIZE,
	CMD_HE_OMI,
	CMD_HE_NDP_4X_LTF_3200NS_GI,
	CMD_HE_FRAGMENTATION,
	CMD_HE_AMSDU_IN_AMPDU_SUPRT,
	CMD_HE_SUBFEE_STS_SUPRT,
	CMD_HE_MAX_NC_SUPRT,
	CMD_HE_ER_SU_PPDU_1X_LTF_800NS_GI,
	CMD_HE_ER_SU_PPDU_4X_LTF_800NS_GI,
	CMD_HE_BSR_SUPPORT,
	CMD_HE_6GHZ_MIN_RATE,
	CMD_EHT_SU_BFMER,
	CMD_EHT_SU_BFMEE,
	CMD_EHT_MU_BFMER,
	CMD_EHT_BFME_SS_80,
	CMD_EHT_BFME_SS_160,
	CMD_EHT_BFME_SS_320,
	CMD_EHT_LTF,
	CMD_ENABLE_MCS15,
	CMD_EHT_NDP_4X_EHT_LTF_AND_320NSGI,
	CMD_EHT_RX_1024_AND_4096_QAM_LS_242_TONE_RU,
	CMD_EHT_DL_OFDMA_TXBF,
	CMD_EHT_SUP_MCS15_IN_MRU,
	CMD_EHT_MCS14_DUP_IN_6GHZ,
	CMD_ECSA_IE_ONLY,
};
#endif /* CTRL_IFACE_H */
